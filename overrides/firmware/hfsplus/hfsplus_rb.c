/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "config.h"
#include "hfsplus_rb.h"
#include "hfsplus_partition.h"
#include "storage.h"
#include "mutex.h"
#include "fs_attr.h"
#include <limits.h>
#include <string.h>

struct hfs_io {
    int drive;
    uint32_t sector_size;
    uint64_t size;
    uint8_t buffer[4096] __attribute__((aligned(32)));
};

static struct hfs_state {
    struct hfs_ro_volume volume;
    struct hfs_io io;
    struct mutex mutex;
    uint8_t scratch[HFS_RO_MAX_NODE_SIZE];
} states[NUM_VOLUMES];

static struct hfs_io scan_io;
static int last_error;

static int setup_io(struct hfs_io *io, int drive)
{
    struct storage_info info;
    storage_get_info(drive, &info);
    if (info.sector_size < 512 || info.sector_size > sizeof(io->buffer) ||
        (info.sector_size & (info.sector_size - 1)) ||
        info.num_sectors > UINT64_MAX / info.sector_size)
        return HFS_RO_UNSUPPORTED;
    io->drive = drive;
    io->sector_size = info.sector_size;
    io->size = (uint64_t)info.num_sectors * info.sector_size;
    return 0;
}

static int device_read(void *ctx, uint64_t offset, void *buffer, size_t length)
{
    struct hfs_io *io = ctx;
    uint8_t *out = buffer;
    if (offset > io->size || length > io->size - offset)
        return -1;
    while (length) {
        uint32_t within = offset % io->sector_size;
        size_t chunk = io->sector_size - within;
        if (chunk > length)
            chunk = length;
        if (storage_read_sectors(IF_MD(io->drive,) offset / io->sector_size,
                                  1, io->buffer))
            return -1;
        memcpy(out, io->buffer + within, chunk);
        out += chunk;
        offset += chunk;
        length -= chunk;
    }
    return 0;
}

void hfsplus_init(void)
{
    last_error = 0;
    for (unsigned int i = 0; i < NUM_VOLUMES; ++i) {
        hfs_ro_unmount(&states[i].volume);
        mutex_init(&states[i].mutex);
    }
}

bool hfsplus_mounted(int volume)
{
    return (unsigned int)volume < NUM_VOLUMES && states[volume].volume.mounted;
}

bool fat_is_readonly(const struct fat_file *file)
{
    return file && hfsplus_mounted(IF_MV_VOL(file->volume));
}

int hfsplus_last_error(void)
{
    return last_error;
}

int hfsplus_partitions(int drive, struct partinfo *parts, int capacity,
                       int *multiplier)
{
    struct hfs_partition found[MAX_PARTITIONS_PER_DRIVE];
    memset(parts, 0, sizeof(*parts) * capacity);
    int rc = setup_io(&scan_io, drive);
    if (rc)
        return rc;
    rc = hfs_partition_scan(device_read, &scan_io, scan_io.size,
                             scan_io.sector_size, found,
                             MAX_PARTITIONS_PER_DRIVE);
    if (rc < 0) {
        last_error = rc;
        return rc;
    }
    if (rc > capacity)
        return HFS_RO_BUFFER;
    *multiplier = 1;
    for (int i = 0; i < rc; ++i) {
        parts[i].start = found[i].offset / scan_io.sector_size;
        parts[i].size = found[i].length / scan_io.sector_size;
        parts[i].type = found[i].type;
        int mult = found[i].unit / scan_io.sector_size;
        if (mult > *multiplier)
            *multiplier = mult;
    }
    return rc;
}

int hfsplus_mount(int volume, int drive, sector_t start, sector_t count)
{
    if ((unsigned int)volume >= NUM_VOLUMES || hfsplus_mounted(volume))
        return HFS_RO_ARGUMENT;
    struct hfs_state *s = &states[volume];
    int rc = setup_io(&s->io, drive);
    if (rc)
        return rc;
    if (!count || start > s->io.size / s->io.sector_size ||
        count > s->io.size / s->io.sector_size - start)
        return HFS_RO_FORMAT;
    uint64_t base = (uint64_t)start * s->io.sector_size;
    uint64_t length = (uint64_t)count * s->io.sector_size;
    uint8_t sig[2];
    if (length < 1536 || device_read(&s->io, base + 1024, sig, sizeof(sig)))
        return HFSPLUS_NOT_HFS;
    if (sig[0] != 'H' || (sig[1] != '+' && sig[1] != 'X'))
        return HFSPLUS_NOT_HFS;
    mutex_lock(&s->mutex);
    rc = hfs_ro_mount(&s->volume, device_read, &s->io, base, length,
                      s->scratch, sizeof(s->scratch));
    mutex_unlock(&s->mutex);
    if (rc)
        last_error = rc;
    return rc;
}

void hfsplus_unmount(int volume)
{
    if ((unsigned int)volume < NUM_VOLUMES)
        hfs_ro_unmount(&states[volume].volume);
}

int hfsplus_sector_size(int volume)
{
    return states[volume].io.sector_size;
}

unsigned int hfsplus_cluster_size(int volume)
{
    return states[volume].volume.block_size;
}

bool hfsplus_size(int volume, sector_t *size, sector_t *free)
{
    if (!hfsplus_mounted(volume))
        return false;
    const struct hfs_ro_volume *v = &states[volume].volume;
    if (size)
        *size = (uint64_t)v->blocks * v->block_size / 1024;
    if (free)
        *free = (uint64_t)v->free_blocks * v->block_size / 1024;
    return true;
}

int hfsplus_open_root(int volume, struct fat_file *file)
{
    memset(file, 0, sizeof(*file));
#ifdef HAVE_MULTIVOLUME
    file->volume = volume;
#else
    (void)volume;
#endif
    file->firstcluster = HFS_RO_ROOT_ID;
    file->hfs_kind = 1;
    file->e.entry = HFS_RO_ROOT_ID;
    return 0;
}

int hfsplus_open(const struct fat_file *parent, long id, struct fat_file *file)
{
    struct hfs_state *s = &states[IF_MV_VOL(parent->volume)];
    struct hfs_ro_iterator it;
    struct hfs_ro_entry e;
    mutex_lock(&s->mutex);
    int rc = hfs_ro_iter_init(&s->volume, (uint32_t)parent->firstcluster, &it);
    if (rc)
        goto out;
    while ((rc = hfs_ro_iter_next(&s->volume, &it, &e)) > 0) {
        if (e.id != (uint32_t)id)
            continue;
        if (e.unsupported || e.data.size > FAT_MAX_FILE_SIZE) {
            rc = HFS_RO_UNSUPPORTED;
            goto out;
        }
#ifdef HAVE_MULTIVOLUME
        file->volume = parent->volume;
#endif
        file->firstcluster = id;
        file->dircluster = parent->firstcluster;
        file->e.entry = e.id;
        file->hfs_data = e.data;
        file->hfs_kind = e.kind;
        rc = 0;
        goto out;
    }
    if (!rc)
        rc = HFS_RO_NOT_FOUND;
out:
    mutex_unlock(&s->mutex);
    return rc;
}

int hfsplus_readdir(struct fat_filestr *stream, struct fat_dirscan_info *scan,
                    struct fat_direntry *entry)
{
    struct hfs_state *s = &states[IF_MV_VOL(stream->fatfilep->volume)];
    struct hfs_ro_entry e;
    int rc = 0;
    mutex_lock(&s->mutex);
    if (scan->entry == FAT_DIRSCAN_RW_VAL) {
        rc = hfs_ro_iter_init(&s->volume, (uint32_t)stream->fatfilep->firstcluster,
                              &scan->hfs_iterator);
        if (rc)
            goto out;
    }
    while ((rc = hfs_ro_iter_next(&s->volume, &scan->hfs_iterator, &e)) > 0) {
        if (e.unsupported || strlen(e.name) > FAT_DIRENTRY_NAME_MAX ||
            e.data.size > FAT_MAX_FILE_SIZE)
            continue;
        fat_empty_fat_direntry(entry);
        strcpy((char *)entry->name, e.name);
        entry->attr = ATTR_READ_ONLY | (e.kind == 1 ? ATTR_DIRECTORY : ATTR_ARCHIVE);
        entry->filesize = (uint32_t)e.data.size;
        entry->firstcluster = (int32_t)e.id;
        scan->entry = e.id;
        scan->entries = 1;
        goto out;
    }
out:
    if (rc <= 0) {
        fat_empty_fat_direntry(entry);
        scan->entries = 0;
    }
    mutex_unlock(&s->mutex);
    return rc;
}

int hfsplus_seek(struct fat_filestr *stream, unsigned long sector)
{
    struct hfs_state *s = &states[IF_MV_VOL(stream->fatfilep->volume)];
    uint64_t bytes = stream->fatfilep->hfs_data.size;
    uint64_t sectors = (bytes + s->io.sector_size - 1) / s->io.sector_size;
    if (sector > sectors)
        return FAT_SEEK_EOF;
    stream->lastsector = sector;
    stream->eof = false;
    return 0;
}

long hfsplus_readwrite(struct fat_filestr *stream, unsigned long sectors,
                       void *buffer, bool write)
{
    if (write)
        return -1;
    if (!sectors || stream->eof)
        return 0;
    struct hfs_state *s = &states[IF_MV_VOL(stream->fatfilep->volume)];
    if (sectors > LONG_MAX / s->io.sector_size)
        return HFS_RO_ARGUMENT;
    struct hfs_ro_entry e;
    memset(&e, 0, sizeof(e));
    e.kind = stream->fatfilep->hfs_kind;
    e.data = stream->fatfilep->hfs_data;
    size_t requested = sectors * s->io.sector_size, got;
    uint64_t offset = (uint64_t)stream->lastsector * s->io.sector_size;
    mutex_lock(&s->mutex);
    int rc = hfs_ro_pread(&s->volume, &e, offset, buffer, requested, &got);
    mutex_unlock(&s->mutex);
    if (rc)
        return rc;
    size_t transferred = (got + s->io.sector_size - 1) / s->io.sector_size;
    size_t padded = transferred * s->io.sector_size;
    if (padded > got)
        memset((uint8_t *)buffer + got, 0, padded - got);
    stream->lastsector += transferred;
    stream->eof = offset + got >= e.data.size;
    return (long)transferred;
}
