# HDR-bijdragen aan darktable

## Takenlijst

1. [x] PQ/HLG-helderheid herstellen bij import en export, issue #20562. Gepusht en aangeboden als [PR #22516](https://github.com/darktable-org/darktable/pull/22516).
2. [ ] Gain-map-import toevoegen, issue #17399. UltraHDR-export loopt al via PR #21458.
3. [ ] macOS EDR-preview valideren op kleur, helderheid en display-headroom, issue #17710 en PR #21358.
4. [ ] HDR-detectie en AgX-autoapply voor HEIC en JXL onderzoeken, issues #19979 en #20238.
5. [ ] HDR-PNG-export controleren met onafhankelijke viewers en metadata-inspectie, issue #19918.
6. [ ] Native HDR-rendering voorbereiden voor macOS en Wayland, issues #20477 en #20688.

## Taak 1: meting en scope

De diagnoseprobe compileert de oorspronkelijke PQ/HLG-profielfuncties uit
`src/common/colorspaces.c` en evalueert hun LittleCMS-tonecurves.
Dit is een meting aan de profielgrens, geen volledige bestandsimport/export.

| Controle | Oorspronkelijk resultaat | Referentie |
|---|---|---|
| PQ-code voor 203 nits naar lineair | 0,020300006 | 1 bij 203 nits referentiewit |
| Lineair wit naar PQ-code | 1 | 0,580688881 bij 203 nits |
| HLG-code 0,75 naar lineair | 0,278836608 | Ongeveer 1 bij 1000 nits piek en 203 nits referentiewit |
| HLG-code 0 naar lineair | 0,000533333 | 0 bij een nulzwart-doeldisplay |

De PQ-meting reproduceert de factor 49,26 uit de bugmelding. De HLG-functie
bevat een inverse OETF met een vaste zwartcorrectie, maar geen
luminantie-afhankelijke OOTF. RGB-kanalen afzonderlijk schalen volstaat niet
voor de correcte HLG-omzetting van gekleurde pixels.

## Implementatie

- Gebruik benoemde referentiewaarden voor SDR-wit en HLG-doeldisplay.
- Herstel absolute/relatieve PQ-luminantie op de gedeelde kleurconversiegrens.
- Voeg de HLG-OOTF en inverse toe op een grens waar de drie RGB-kanalen
  gezamenlijk beschikbaar zijn. Houd rekening met de primairafhankelijke luma.
- Controleer import en export voor Rec.2020 en P3, CPU en OpenCL, en de
  LittleCMS-route. Behoud highlights boven relatief wit.
- Controleer embedded ICC-profielen afzonderlijk; CICP en ICC mogen niet
  dezelfde correctie dubbel krijgen.
- Verifieer zwart, wit, gekleurde pixels en HDR-highlights tegen onafhankelijke
  referentiewaarden. Een export/import-rondgang alleen is onvoldoende.
- Controleer de bestaande UltraHDR-export en CLLI-metadata op regressies.

De gebruiker heeft deze aanpak goedgekeurd. CPU en OpenCL gebruiken nu dezelfde
transferfuncties in `src/common/hdr-transfer.h`. HLG gebruikt RGB-luminantie
voor de OOTF; PQ gebruikt 203 nits referentiewit. De kleurconversies behandelen
de transfer apart van de primairmatrix, ook via LittleCMS.

De bestandsproeven vonden daarnaast twee PNG-leesfouten: de unknown-chunkconfiguratie
schakelde native cICP-parsing uit en 16-bit blauw gebruikte de lage byte van groen.
Beide zijn hersteld. JXL leest de oorspronkelijke HDR-transfermetadata voordat
een benaderend, gegenereerd ICC-profiel wordt gekozen.

## Testomgeving

Probe: `/tmp/darktable-hdr-diagnosis/profile-probe`.
De probe reproduceert de fout in de oorspronkelijke PQ/HLG-profielen.
Referentiebestanden en de onafhankelijke transferformules staan in
dezelfde tijdelijke directory.

De CLI in `/Users/mayk/darktable-hdr-install` start niet: hij verwijst naar
de ontbrekende `libOpenEXR-3_4.33.dylib`.
De nieuwe lokale build `build-hdr-fix` gebruikt de beschikbare bibliotheken.
De bestaande installatie is niet vervangen.

## Verificatie

- 28 CLI-gevallen geslaagd: AVIF, PNG, HEIF en JXL import; Rec.2020 en P3;
  PQ en HLG; matrix- en LittleCMS-export; PNG-herimport met ICC en cICP.
  Grootste afwijking van de onafhankelijke referentie: 0,000687, onder de grens 0,003.
- Vier transfer-unitchecks en acht bestaande CLLI-unitchecks geslaagd.
- OpenCL-kernels compileren; bestaande waarschuwingen over floatliteralen blijven.
- Werkelijke Apple M5 Pro OpenCL-import gecontroleerd. PQ/HLG-export loopt op
  CL0 zonder verloren events. HLG-export is gelijk aan CPU; PQ verschilt maximaal
  één 16-bit codewaarde.
- AVIF PQ-export meldt MaxCLL 203 nits en MaxFALL 99 nits voor de SDR-referentie.
- UltraHDR-bestandsexport is nog niet geverifieerd: de CLI weigert de lange
  modulenaam als extensie. Een tijdelijke modulealias bereikte de exporter,
  maar de export mislukte. De alias is verwijderd; dit is een open controle,
  geen bewezen regressie of geslaagde test.
- Python-lint en `git diff --check` geslaagd.
- De volledige build wordt bij de bestaande filmic-unitstest geblokkeerd door
  GNU-linkeroptie `--wrap`, die Apple ld niet ondersteunt. Productietargets en
  de gerichte HDR-tests bouwen wel. Geen volledige testsuiteclaim.

Herhaal de CPU-bestandscontrole met:

```sh
~/.pixi/envs/main/bin/python src/tests/test-hdr-transfer-cli.py \
  --cli build-hdr-fix/bin/darktable-cli \
  --fixtures /tmp/darktable-hdr-diagnosis \
  --output /tmp/darktable-hdr-diagnosis/fixed-regression
```

Resultaten en afzonderlijke CLI-logs staan in de uitvoermap. De fixturebestanden
komen uit de hieronder genoemde onafhankelijke referentierepository.

## Gepubliceerde bijdrage

- [PR #22516](https://github.com/darktable-org/darktable/pull/22516), gebaseerd op upstream `61dea294be`.
- Branch: `codex/fix-pq-hlg-transfer`, gepusht naar de GitHub-fork.
- Implementatie: `fa075b7fd8`; afzonderlijke release note: `a0628d29cd`.
- [Onderzoeksreactie op #20562](https://github.com/darktable-org/darktable/issues/20562#issuecomment-5998290485).
- Definitieve PR-branch: volledige build geslaagd, 13 CTest-tests geslaagd,
  alle 28 HDR-bestandsproeven opnieuw geslaagd. De eerdere filmic-linkerbeperking
  geldt niet meer op deze actuele upstream-basis.
- Bestaande exposure-integratietest geslaagd met de lichte vergelijking.
  Delta E niet uitgevoerd wegens ontbrekende colour-science; volledige
  integratiesuite en varianten met uitgeschakelde optionele features niet getest.
- GPU- en CLLI-experimenten hierboven betreffen de oorspronkelijke lokale
  implementatie. De blauwe PNG-byte is al upstream hersteld en ontbreekt in de PR.

## Bronnen

- [Bugmelding #20562](https://github.com/darktable-org/darktable/issues/20562)
- [Onafhankelijke HDR-referentiebeelden en generator](https://github.com/kennylevinsen/hdr-test-images)
- [ITU-R BT.2100](https://www.itu.int/rec/R-REC-BT.2100)
