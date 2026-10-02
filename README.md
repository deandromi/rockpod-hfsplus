# Rockpod HFS+ read-only experiment

Experimentele HFS+-integratie voor iPod Classic (`ipod6g`, 64 MB).
Nog niet op een iPod getest. Een groene build bewijst uitsluitend dat de
controles en compilatie zijn geslaagd, niet dat opstarten of afspelen werkt.

De workflow haalt Rockpod van `nuxcodes/rockpod` op commit
`951d17c0575fbb61f3c585b5f0c3a0a941dcb10f` op: de versie in de aangeleverde
`rockpod-master.zip`. `overrides/` bevat alle toegevoegde en gewijzigde bestanden.
`ci/assemble.py` controleert de originele en gewijzigde SHA-256-hashes voordat
het de wijzigingen toepast. Dit is Rockpod, geen vervanging door stock Rockbox.

De Actions-workflow draait hosttests, bouwt de Rockbox ARM-toolchain met GCC
9.5.0 en compileert daarna firmware en bootloader. Bij succes verschijnt een
artifact met `rockbox.zip`, `bootloader-ipod6g.ipod`, de complete gebruikte
broncode, checksums en documentatie. Bij fouten zijn de logs beschikbaar.
Er wordt niets automatisch geïnstalleerd of geflasht.

De HFS+-driver ondersteunt uitsluitend lezen van een beperkte HFS+-subset.
Instellingen, playlistbeheer, databaseopbouw en PictureFlow kunnen door
ontbrekende schrijfmogelijkheden falen. De bestaande PFRAW-generator blijft
ongewijzigd. Zie `overrides/docs/HFSPLUS-DEVELOPMENT.md` voor tests en beperkingen.

Nieuwe HFS+-code is GPL-2.0-or-later; bestaande bronbestanden behouden hun
oorspronkelijke copyright- en licentievermeldingen.
