# I51b — raport wykonawcy, 2026-11-18

Prerejestracja: `9ab1ca0`, G6_METRICS I51b. Polityka metryk: `396e057`.
**Wynik: I51B_FAIL. G6 pozostaje otwarte.**

Zaimplementowano tylko jawny eksperyment kolejności: A = hardFail, inBand malejąco,
distance, indeks; B = hardFail, distance, inBand malejąco, indeks.
Domyślny ranking A, generator, GUI, serializacja map, pasma g6-bands-v1,
26 metryk, jednostki, N/A i kryteria nie zostały zmienione.
Domyślny mountain i brak zależności od ID przedmiotów pozostają bez zmian.

## Macierz i decyzja

Nowe seedy 2001–2032, N=16, bok 512, --g5-levels; m06 długość/gęstość
40/30, rockBlocks=0; m19 80/40, rockBlocks=1. Wspólne wiersze kandydatów
dla A/B, dwa pełne przebiegi, łącznie 2048 generacji. Każdy przebieg profilu
podzielono na cztery niezależne bloki po osiem seedów, scalone w kolejności
kanonicznej. Brak retry, usuwania seedów i dodatkowych wariantów.
Nie oceniano historycznej prywatnej macierzy 1001–1032.

| Profil | A | B | Baseline dokładny A=B | B >=23 i B>A | Zmiany winnera |
| --- | --- | --- | --- | --- | --- |
| m06 | 29/32 | 28/32 | 125/256 | FAIL | 26/32 |
| m19 | 27/32 | 21/32 | 1/2 | FAIL | 23/32 |

B nie przewyższa A w żadnym profilu. Dodatkowo m19 B=21/32 nie osiąga
progu 23/32. Jest to ważny pomiar jakości poniżej warunków przyjęcia
(kod wyjścia ewaluatora 1), a nie blokada danych.

Warunki przyjęcia są łączne i osobne dla obu profili. Wynik nie powoduje
dostrajania pasm, wag, parametrów ani progu. B pozostaje wyłącznie jawnym
trybem eksperymentalnym; nie zastępuje produkcyjnego A.
I51 zachowuje historyczne FAIL 18/32 obu profili. I51b nie zamyka G6:
holdout ujawniono wcześniej, nowe seedy nie przywracają jego ślepoty.
Nie deklarujemy istotności statystycznej ani jakości na nowych mapach.

Sukces wymaga hardFail=0 i odległości ściśle poniżej mediany wszystkich
16 kandydatów (średnia pozycji 8 i 9); remisy przegrywają. Główny wynik
to (M01 + M05 + (M11+M18)/2)/3. M12–M17 są tylko dodatkiem.
Baseline to dokładna średnia udziałów kandydatów spełniających ten sam
warunek w każdym seedzie; hardFail i wszystkie seedy zostają w mianowniku.

## Wyniki per mapa i rodzina

Poprawa/pogorszenie/remis oznaczają mniejszą/większą/równą odległość
holdout B względem A, bez zaokrąglania. JSON zawiera osobno przejścia
sukces/porażka, również gdy zwycięzca się zmienił, a odległość nie.

### m06

| Mapa / rodzina | A sukcesy | B sukcesy | Baseline dokładny | Poprawa / pogorszenie / remis |
| --- | --- | --- | --- | --- |
| M01 | 11/32 | 10/32 | 213/512 | 10 / 14 / 8 |
| M05 | 19/32 | 22/32 | 63/128 | 17 / 9 / 6 |
| M11 | 26/32 | 25/32 | 249/512 | 16 / 10 / 6 |
| M12 | 26/32 | 25/32 | 249/512 | 9 / 17 / 6 |
| M13 | 7/32 | 4/32 | 63/128 | 10 / 16 / 6 |
| M14 | 22/32 | 27/32 | 249/512 | 13 / 13 / 6 |
| M15 | 23/32 | 27/32 | 125/256 | 18 / 8 / 6 |
| M16 | 0/32 | 0/32 | 3/512 | 0 / 0 / 32 |
| M17 | 0/32 | 0/32 | 3/512 | 0 / 0 / 32 |
| M18 | 20/32 | 23/32 | 31/64 | 13 / 12 / 7 |
| family.M01 | 11/32 | 10/32 | 213/512 | 10 / 14 / 8 |
| family.M05 | 19/32 | 22/32 | 63/128 | 17 / 9 / 6 |
| family.M19-related | 26/32 | 28/32 | 249/512 | 8 / 18 / 6 |
| family.main | 29/32 | 28/32 | 125/256 | 14 / 12 / 6 |
| family.winter | 25/32 | 26/32 | 249/512 | 17 / 9 / 6 |

### m19

| Mapa / rodzina | A sukcesy | B sukcesy | Baseline dokładny | Poprawa / pogorszenie / remis |
| --- | --- | --- | --- | --- |
| M01 | 19/32 | 17/32 | 253/512 | 9 / 14 / 9 |
| M05 | 25/32 | 24/32 | 1/2 | 9 / 14 / 9 |
| M11 | 16/32 | 12/32 | 255/512 | 8 / 15 / 9 |
| M12 | 16/32 | 27/32 | 1/2 | 16 / 7 / 9 |
| M13 | 6/32 | 7/32 | 255/512 | 12 / 11 / 9 |
| M14 | 15/32 | 21/32 | 255/512 | 16 / 7 / 9 |
| M15 | 27/32 | 23/32 | 1/2 | 12 / 11 / 9 |
| M16 | 11/32 | 13/32 | 249/512 | 11 / 11 / 10 |
| M17 | 11/32 | 13/32 | 249/512 | 11 / 11 / 10 |
| M18 | 13/32 | 14/32 | 255/512 | 10 / 13 / 9 |
| family.M01 | 19/32 | 17/32 | 253/512 | 9 / 14 / 9 |
| family.M05 | 25/32 | 24/32 | 1/2 | 9 / 14 / 9 |
| family.M19-related | 14/32 | 21/32 | 1/2 | 15 / 8 / 9 |
| family.main | 27/32 | 21/32 | 1/2 | 10 / 13 / 9 |
| family.winter | 13/32 | 16/32 | 255/512 | 11 / 12 / 9 |

Pełne wyniki A/B per seed, mapa i rodzina, scores zwycięzców (indeks,
hardFail, inBand, distance), mediana, baseline oraz przejścia par:
[g6_i51b_holdout.json](g6_i51b_holdout.json). Surowe raporty i puste stderr
pozostają w `scratchpad/i51b-matrix/` tego worktree.

## HardFail i pokrycie

| Profil | Wspólni kandydaci hardFail | A zwycięzcy hardFail | B zwycięzcy hardFail |
| --- | --- | --- | --- |
| m06 | 10/512 | 0/32 | 0/32 |
| m19 | 1/512 | 0/32 | 0/32 |

| Mapa | Znana referencja /26 | Różne wkłady m06 | Różne wkłady m19 |
| --- | --- | --- | --- |
| M01 | 8 | 6 | 5 |
| M05 | 8 | 8 | 7 |
| M11 | 7 | 6 | 5 |
| M12 | 10 | 9 | 8 |
| M13 | 7 | 7 | 6 |
| M14 | 10 | 10 | 8 |
| M15 | 11 | 11 | 10 |
| M16 | 5 | 3 | 2 |
| M17 | 5 | 3 | 2 |
| M18 | 5 | 4 | 3 |

Małe pokrycie i cap ograniczają rozdzielczość holdoutu. Referencyjne N/A
daje stałe ln(64) wszystkim kandydatom, bez usuwania pozycji. JSON podaje
przyczyny każdej pozycji N/A (liczebność <5 lub progi G4 dla plam/granic)
i metryki rzeczywiście różnicujące. Błąd wejścia blokuje ocenę, nie jest N/A.

Audyt prywatnych wejść: PASS dla M01–M19, klient 10.98, zgodne wersje map,
potwierdzenia PASS, źródła import/semantic/verification, DAT/SPR, keymap
i powtarzane podsumowania. Cztery jawne serie: g4-channels-20261108a,
g4-scale-20261108a, g4-trans-20261108a, g4-build-20261108a; profile
g4-profiles/10.98-v10. Map, profili i obrazów nie kopiowano do Git.
Cały referenceAudit (w tym hashe profili) jest identyczny z raportem I51.

## Powtarzalność i zamrożenie

Dwa raporty każdego profilu są bajtowo identyczne. Wszystkie procesy
macierzy zakończyły się kodem 0 i pustym stderr. Ewaluator niezależnie
odtworzył 1024 scores rdzenia i oba wybory przed odczytem holdoutu.
Dodatkowy audyt odtworzył wszystkie 1024 seedy kandydatów (SplitMix64),
sprawdził sumy par i zgodność zmian sukcesów z przejściami A/B: PASS.

| Profil | SHA-256 raportu każdego przebiegu |
| --- | --- |
| m06 | `224733843d77dbf81109029df6337dc783ce3690dfa36b22dd652509d01e6389` |
| m19 | `9c5ddb61a3bccb195ea5119dd6e6807a01765a5566d67e1a42d67a25dff05f6f` |

SHA-256 EXE: `272d634ea438979c67fc09c03359d5e510dc970f339e4e9b39fecb3483dd4a21`.
Kod pomiaru/oceny i EXE nie zmieniły się od uruchomienia macierzy;
lokalny manifest hashy: `scratchpad/i51b-code-freeze.json`.

Polecenia (od katalogu worktree):

```text
rtk proxy py -I docs/procedural/smoke/g6_measure_candidates.py scratchpad/i51b-build/procedural/candidate_rank.exe C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10 scratchpad/i51b-matrix --g5-levels --i51b
rtk proxy py -I docs/procedural/smoke/g6_evaluate_i51b.py C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10 scratchpad/i51b-matrix scratchpad/i51b-holdout.json
```

## Testy i ograniczenia

- Izolowany build rzeczywistego source/procedural/CMakeLists.txt, MSVC
  19.51.36257 x64 Release, C++23, Ninja 1.13.2: PASS.
- CTest 22/22 PASS; po uzupełnieniu testu agregacji ponowiono test I51b:
  3/3 PASS. Pozostałe testy I51 holdoutu: 8/8, CLI: PASS.
- Ręczne 26 pasm: dwa razy ln(1.5) daje 811; ln(577/384) daje 407
  przy 25 pasmach. A wybiera pierwszego, B drugiego; kandydat z hardFail
  przegrywa mimo 26 pasm i distance=0. Remisy pasm i indeksu: PASS.
- Ręczna agregacja 32 par: 16 popraw, 8 pogorszeń, 8 remisów;
  A=16/32, B=24/32, baseline 5/64. Test granic B=22 (FAIL),
  A=B=23 (FAIL), A=22/B=23 (PASS), odmowy niekompletnej macierzy,
  błędnego score/winnera i znacznika eksperymentu: PASS.
- N=1, kandydat 0, fingerprint 1729 100×100 =
  17159159863505677800, powtarzalność i brak mutacji: PASS.

Nie wykonano kanonicznego build_ninja.bat, g5_run_suite z sondami,
G5 --i47, ręcznego GUI/Place/Undo, gry, oryginalnego RME ani końcowej
maski adaptera. Te regresje wykonuje rodzic niezależnie po przeglądzie
i ff-merge. Nie potwierdzono determinizmu libm między kompilatorami.
Nie importowano ponownie map: historyczne potwierdzenia są dowodem wejść,
lecz nie zawierają hasha OTB, więc tożsamości binarnego OTB nie dowiedziono.

G6_GATE_STATUS pozostawiono bez zmian. Nie utworzono build-ninja w tym
worktree; build i wszystkie pliki robocze są tylko w scratchpad.
Nie instalowano zależności, nie dotykano C:/repo/vcpkg/buildtrees,
nie wykonano push. Scratchpad i local-data nie wchodzą do commita.
