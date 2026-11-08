# G4-C — cel stylu z map i dopasowanie parametrów generatora, 2026-11-08

**PARTIAL, wynik w dużej mierze negatywny.** Mechanizm (cel z map treningowych, deterministyczne dopasowanie `SurfaceTuning`, profile) działa i jest powtarzalny. Na dostępnych danych **dopasowanie nie uogólnia na inne rodziny map**: poprawia dopasowanie do map treningowych, ale na rodzinie walidacyjnej zwykle jest gorsze niż ręczne parametry. Nie ma jeszcze oceny na zbiorze testowym (G4-F); żadnych wartości testowych nie liczono ani nie oglądano.

## Co jest
- `style_profile`: schemat `g4-profile/2` (dodano `target` i `tuning`), `targetFromStats`, `buildTarget` (każda rodzina waży tyle samo, mapy w rodzinie po równo; mapa liczy się od progu kafli naturalnych), `serializeStats`/`parseStats` (`g4-kindstats/1`).
- `style_fit`: projekcja `SurfaceKind` → `StyleKind` (Lake i River = Water), `statsOfSurface`, `evaluateTuning` (suma po seedach), `distance` (L1 w ppm: udziały 5 rodzajów + 25 komórek sąsiedztwa), `fitTuning` (spadek współrzędnych po stałych siatkach dla 7 parametrów: lake, rock, dirt, shoreWidth, riverCount, transitionWidth, featureSize; pozostałe 3 stałe), `selectStep` (krok o najmniejszym dystansie na walidacji).
- Sonda G3 zapisuje `kind-stats-surface.txt` i `kind-stats-underground.txt` (zaakceptowane mapowanie z G4-B); runner ma `--keymap`. Narzędzie `style_fit`: per fold cel z map treningowych, walidacja wybiera krok, test nie jest czytany; zapisuje `fold-N.profile`, `profile-all.profile`, `fit-report.json`.
- Testy: `test_procedural_style_profile` i `test_procedural_style_fit` PASS (m.in. cel osiągalny: dopasowanie zbliża się o ≥ 2× do celu wygenerowanego znanym tuningiem; dystans treningowy nie rośnie między przebiegami; optymalny start nie zmienia się; powtarzalność; anulowanie). Dwie serie `style_fit` (17 s każda) dały bajtowo identyczne pliki. Podział z G4-A bez zmian po refaktorze progu ogólności.

## Dane: ile map faktycznie niesie rozkład gruntu na powierzchni (z≤7)
Statystyki przez mapowanie z G4-B, kafle w naturalnych rodzajach (Grass, Dirt, Rock, Shore, Water) na z≤7:

| Mapa | Naturalne | NoGround | Uwaga |
| --- | ---: | ---: | --- |
| M05 | 17 805 | 2 271 | użyteczna |
| M06 | 2 280 292 | 276 838 | użyteczna, dominuje objętością (wagi per mapa i rodzina to adresują) |
| M11 | 2 987 | 69 | użyteczna (głównie Rock, Water) |
| M14 | 3 427 | 302 | użyteczna (tylko Dirt) |
| M18 | 693 | 624 | poniżej progu 1000 |
| M19 | 794 042 | 157 772 | użyteczna |
| M01, M12, M13, M15, M16, M17 | 0 | 33–31 987 | **zero kafli naturalnych na powierzchni**: prawie wszystkie kafle to NoGround (ground bez brusha w danych 10.98) |

Wniosek: z 12 map stylu tylko **5 niesie użyteczny rozkład** (M05, M06, M11, M14, M19), a zero kafli naturalnych na pozostałych wynika z tego, że ich grunty nie należą do brushy w `data/` 10.98 (klucz „bez brusha”), nie z tego, że są puste.

## Wynik dopasowania (ręczne parametry = domyślne `SurfaceTuning`, seedy dopasowania 1–6, bok 160)
Dystans L1 do celu, ppm (max 4 000 000); „walidacja” = dystans do celu innej rodziny, wybór kroku po nim. Brak foldu 0 (test = rodzina „caves”, cele train/walidacja nie istnieją).

| Fold (test) | Train: baseline → wybrany | Walidacja: baseline → wybrany | Wybrany krok |
| --- | ---: | ---: | --- |
| M01 (brak danych testowych) | 2 080 966 → 2 080 966 | 1 648 968 → 1 648 968 | 0 (baseline) |
| M05 | 2 161 088 → 788 761 | 2 186 911 → 2 121 998 | 2 |
| M06 | 227 302 → 139 377 | 3 407 994 → 3 186 637 | 1 |
| zima (M11, M18) | 1 166 603 → 1 166 603 | 1 929 118 → 1 929 118 | 0 (baseline) |
| wszystkie mapy stylu (bez walidacji) | 1 433 073 → 306 460 | – | – |

- Dopasowanie obniża dystans do celu treningowego 2–10 razy, ale walidacja poprawia się tylko o 3–7 % (foldy M05, M06) i **pogarsza się dwukrotnie** w foldach M01 i zima (przy wyborze po walidacji wygrywa baseline).
- Dopasowane parametry sięgają krańców siatek (jezioro 300 ‰, skała 320 ‰, 8 rzek, `transitionWidth` 16 000), czyli dopasowanie rozkładu udziałów bez kontroli wyglądu; wizualnie niesprawdzone.
- Rozrzut: dwa pełne przebiegi dały ten sam wynik (determinizm), ale nie ma przedziałów niepewności po seedach ani po mapach; na foldach z jedną mapą nie da się ich sensownie podać.

## Dlaczego to nie uogólnia (hipotezy, niesprawdzone)
Rodziny to różne style (wybrzeże świata M06, trawiasta M05, górska zima M11, sama ziemia M14), a średnia ważona rodzin nie opisuje żadnej z nich; cel treningowy jest zlepkiem. Brak jednej wspólnej dystrybucji, którą dałoby się „pobić” w sensie przybliżenia obcej rodziny.

## Czego NIE zrobiono
- Klastrowanie i przejścia biomów jako osobne metryki (tu tylko udziały i sąsiedztwo rodzajów); brak metryk 16/32/64 i kształtu rzek.
- Jakakolwiek ocena na teście, G4-D (reguły kontekstowe), G4-E (podziemia/wersje), G4-F.
- Walidacja wizualna parametrów i kontrola metryk grywalności (brak POI w wyniku Surface; `reachable_*` nie mierzono w G4-C).
- Mapowanie BlackTek; pojedyncza maszyna.
