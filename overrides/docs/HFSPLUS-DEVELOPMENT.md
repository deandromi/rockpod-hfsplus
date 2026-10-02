# Rockpod HFS+ alleen-lezen: geïntegreerde experimentele broncode

Status: **geen installeerbare firmware, geen geslaagde ARM-build en geen iPod-opstarttest**.
Doelapparaat: iPod Classic / Rockbox-target `ipod6g`, 64 MB, overeenkomstig de
meegeleverde installatie. Dit pakket bevat de volledige aangeleverde Rockpod-bronboom
met de hieronder beschreven wijzigingen. Het vervangt het eerdere losse leesprototype.

## Wat nu aangesloten is

- HFS+-partitiedetectie en mounten in `firmware/common/disk.c`.
- HFS+-bestanden en mappen achter de bestaande FAT-bestandsinterface, inclusief
  lezen, seek, EOF, volume-identiteit en unmounten.
- HFS+-schrijfacties worden geweigerd; de hogere bestandsinterface geeft waar
  mogelijk `EROFS` terug bij aanmaken, schrijfopen, verwijderen en hernoemen.
- Dezelfde lezer in `bootloader/ipod-s5l87xx.c`, het bootloaderdoel voor ipod6g.
  Na een geslaagde HFS+-mount toont deze `HFS+ read-only test` en probeert de
  bestaande laadcode `/.rockbox/rockbox.ipod` te laden.
- Een mislukte HFS+-mount toont een HFS+-fout in plaats van uitsluitend de generieke
  partitiemelding. De foutmelding wordt niet onderdrukt om door te kunnen starten.
- `HAVE_HFSPLUS_RO` staat in deze bronboom aan voor ipod6g-hardwarebuilds.
  De directorycache staat hierbij uit. FAT blijft een alternatief mountpad,
  maar volledige FAT-regressietests zijn nog niet uitgevoerd.

De bestaande launcher krijgt deze mogelijkheden alleen met een nieuw gebouwde,
afzonderlijk geïnstalleerde bootloader. Alleen `.rockbox` vervangen wijzigt de
bestaande bootloader niet. Er is hier niets op een iPod geïnstalleerd of geflasht.

## Bouwen op een ontwikkelcomputer

Vereist: de Rockbox `arm-elf-eabi`-toolchain met GCC 9.5.0, de gebruikelijke
Rockbox-buildafhankelijkheden, een host-C-compiler, GNU make, Perl, Python 3 en zip.
De toolchain moet in `PATH` staan. De meegeleverde `tools/rockboxdev.sh` is het
bestaande Rockbox-toolchainbouwscript; downloads en installatie daarvan zijn hier
niet voltooid. Het hieronder genoemde script downloadt geen compiler.

Vanuit de uitgepakte `rockpod-master`:

```sh
sh tools/hfsplus/build-ipod6g.sh
```

Dit maakt een nieuwe bouwmap, bouwt de normale Rockpod-firmware en daarna de
bootloader, en verzamelt pas na beide geslaagde builds:

- `output/rockbox.zip`: het normale `.rockbox`-pakket.
- `output/bootloader-ipod6g.ipod`: de afzonderlijke bootloader.

Het script flasht niets. Ook een geslaagde build bewijst nog niet dat deze
experimentele code op de iPod werkt. Linkruimte, ARM-ABI, opstarten en werkelijk
gebruik van het hele bestandssysteem moeten dan nog worden getest. In deze
uitvoeromgeving stopt de preflight op `Ontbreekt: arm-elf-eabi-gcc`. Pogingen de
compiler te downloaden kregen HTTP 403. Er zijn daarom bewust geen bestanden
meegeleverd die als bruikbare firmwarebinary worden gepresenteerd.

## Ondersteunde HFS+-subset

- HFS+ versie 4, uitsluitend schoon afgemelde volumes.
- APM (`Apple_HFS`), MBR en primaire GPT met header- en tabel-CRC; ook ruwe volumes.
- Logische schijfsectoren van 512 of 4096 bytes en partitietabel-eenheden van
  512 of 4096 bytes in de geteste combinaties; grenzen en uitlijning worden gecontroleerd.
- Catalogusbladeren, UTF-16 naar UTF-8 en dataforks met maximaal acht inline extents.
- Interne, geïnitialiseerde, lege journals met geldige headerchecksum en `HFSJ`
  als laatste mountversie, in beide bytevolgordes. Geen journal replay.
- Gefragmenteerde data binnen inline extents, gedeeltelijke reads en EOF.

Niet ondersteund: HFSX, HFS-wrappers, extents-overflow, resourceforks, symlinks,
hardlinks, macOS-gecomprimeerde bestanden, encryptie, gecomprimeerde DMG-containers,
GPT-herstel uit de back-uptabel en uitgebreide MBR-partities.
Niet-ondersteunde objecten, te lange namen en bestanden boven de FAT-interfacegrens
worden door de adapter overgeslagen. NUL en een dubbele punt in de opgeslagen naam
worden niet als gewone Rockpod-bestandsnamen aangeboden.

De catalogus wordt lineair gescand; op grote bibliotheken kan dit erg traag zijn.
Er is geen volledige HFS+-casefolding of Unicode-normalisatie. De zelfstandige
hosttool gebruikt exacte opgeslagen spelling; de Rockpod-padlaag behoudt haar
bestaande naamvergelijking. Dit is dus geen volledige HFS+-implementatie.

## Gevolgen van alleen-lezen

Instellingen opslaan, databaseopbouw, playlistadministratie en onderdelen van
PictureFlow verwachten schrijfmogelijkheden. Die kunnen fouten geven of functies
blokkeren; muziekafspelen is niet bewezen. Alleen-lezen is hier een stap om het
mount- en laadpad te onderzoeken, nog geen bruikbare dagelijkse muziekfirmware.

De bestaande Python-generator voor `.pfraw` en covercache is niet gewijzigd.
De generator vervangt niet alle andere schrijfacties van PictureFlow.
USB-mass-storage geeft de computer nog steeds rechtstreekse opslagtoegang; deze
wijziging maakt USB-toegang niet alleen-lezen. Een volume met een gevuld journal
of inconsistente mountstatus wordt geweigerd en niet door deze driver hersteld.
HFS+ gebruiken bewijst niet dat de oorzaak van de gemelde corruptie is verholpen.

## Reproduceerbare hostcontroles

```sh
cd tools/hfsplus
make test
ASAN_OPTIONS=detect_leaks=0 make sanitize
cd ../..
python3 tools/hfsplus/tests/check_integration.py --sanitize
```

`--sanitize` en `make sanitize` gebruiken Linux/GCC-vlaggen. Op andere systemen kan
men eerst `make test` en `check_integration.py` zonder `--sanitize` proberen.
Het integratiescript maakt een expliciete hostconfiguratie in een tijdelijke map;
het bouwt nooit firmware en die configuratie mag niet voor ARM worden gebruikt.

Uitgevoerd op Linux/GCC:

- 19 synthetische imagetestmethoden en afzonderlijke C-API-tests geslaagd.
- Partitietabeltests voor APM/MBR/GPT, 512/4096-eenheden, ongeldige checksums en grenzen geslaagd.
- 24 adaptergevallen geslaagd: 12 voor firmwareheaders en 12 voor bootloaderheaders.
  Deze lezen een testbestand via de werkelijke adaptercode met gesimuleerde opslag,
  testen EOF/seek/uitlijning en bevestigen nul opslagwrites en nul reads buiten de image.
- C-syntaxiscontrole van de betrokken bestandslaag en bootloader geslaagd met de
  ipod6g-targetheaders op de host. De 64-bit host geeft een bekende `SIZE_MAX`-
  herdefinitiewaarschuwing in Rockbox-headers; dit is geen ARM-compilatietest.
- De bovenstaande imagetests en adaptergevallen ook geslaagd met AddressSanitizer
  en UndefinedBehaviorSanitizer. Lekdetectie stond uit wegens de uitvoeromgeving.

De adaptertest gebruikt opslag- en mutexstubs en test niet de volledige uitvoering
van `open()` tot hardware. Alle images zijn zelf geconstrueerd, niet onafhankelijk
met macOS gemaakt. Geen emulator, echte HFS+-iPod, ARM-link of opstarttest is uitgevoerd.
De fixture `rockbox.ipod` bevat uitsluitend `TEST ONLY!!!`, geen firmware.

Voor een gewone image kan de hosttool partitiedetectie demonstreren:

```sh
cd tools/hfsplus
make clean
make
./hfsplus-inspect --scan image.raw info
./hfsplus-inspect --scan image.raw ls /.rockbox
```

De hosttool accepteert gewone bestanden, geen apparaatpaden. Voor GPT Basic Data
met HFS+-inhoud is een expliciet volumebegin nodig in de hosttool; de firmwareadapter
probeert iedere gedetecteerde kandidaat. Gebruik voor expliciete bytegrenzen
`--offset BEGIN --length LENGTE` in plaats van `--scan`.

## Overgebleven verificatie

1. Een echte ARM-build van zowel firmware als bootloader, inclusief controle van
   geheugen- en linkerlimieten en eventuele compiler-/linkerfouten oplossen.
2. De volledige bestandslaag en het firmwarelaadpad uitvoeren tegen onafhankelijke
   macOS-HFS+-images, plus FAT-regressietests.
3. Een hardware-opstarttest met herstelmogelijkheid; vervolgens apart beoordelen
   wat er nodig is om Rockpod zonder permanente schrijfacties bruikbaar te maken.

Formaatreferentie: Apple TN1150, HFS Plus Volume Format:
https://developer.apple.com/library/archive/technotes/tn/tn1150.html

Nieuwe code: GPL-2.0-or-later, passend bij Rockbox; zie `COPYING.txt`.
