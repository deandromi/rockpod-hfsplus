/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HFSPLUS_RB_H
#define HFSPLUS_RB_H
#include "fat.h"
#include "disk.h"

#define HFSPLUS_NOT_HFS (-100)
void hfsplus_init(void);
bool hfsplus_mounted(int volume);
int hfsplus_mount(int volume, int drive, sector_t start, sector_t count);
void hfsplus_unmount(int volume);
int hfsplus_partitions(int drive, struct partinfo *parts, int capacity,
                       int *multiplier);
int hfsplus_last_error(void);
int hfsplus_sector_size(int volume);
unsigned int hfsplus_cluster_size(int volume);
bool hfsplus_size(int volume, sector_t *size, sector_t *free);
bool fat_is_readonly(const struct fat_file *file);
int hfsplus_open_root(int volume, struct fat_file *file);
int hfsplus_open(const struct fat_file *parent, long id, struct fat_file *file);
int hfsplus_readdir(struct fat_filestr *stream, struct fat_dirscan_info *scan,
                    struct fat_direntry *entry);
long hfsplus_readwrite(struct fat_filestr *stream, unsigned long sectors,
                       void *buffer, bool write);
int hfsplus_seek(struct fat_filestr *stream, unsigned long sector);
#endif
