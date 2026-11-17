# I51 — raport wykonawcy, 2026-11-17

Wiążąca polityka: `G6_METRICS.md`, commit `396e057`. **G6 pozostaje otwarte.**

Zaimplementowano stałe 26 metryk i jawne pasma w setnych, N/A bez zmiany
mianownika, ograniczone odległości log/log1p/asinh oraz dokładną agregację
holdoutu i baseline. Generator, GUI, zapis map i kryteria nie zostały zmienione.
Domyślny mountain i brak zależności od ID przedmiotów pozostają bez zmian.

## Macierz i próg

Dwa pełne przebiegi na tej samej końcowej wersji MSVC x64 Release, C++23:
m06/m19, 32 seedy użytkownika 1001–1032, N=16, bok 512, `--g5-levels`,
pathLength/buildingDensity 40/30 i 80/40, rockBlocks 0/1. Łącznie 2048
wygenerowanych i ocenionych kandydatów. Brak ponawiania nieudanych seedów.
Każdy przebieg wykonano w czterech niezależnych blokach ośmiu seedów per
profil, następnie połączono w kanonicznej kolejności. Test CLI potwierdza
bajtową zgodność raportu dzielonego z raportem pojedynczego procesu.

Polecenia końcowego pomiaru (od katalogu worktree):

```text
rtk proxy py -I docs/procedural/smoke/g6_measure_candidates.py scratchpad/build/procedural/candidate_rank.exe C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10 scratchpad/i51-matrix-complete --g5-levels
rtk proxy py -I docs/procedural/smoke/g6_evaluate_holdout.py C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/g4-profiles/10.98-v10 scratchpad/i51-matrix-complete scratchpad/i51-holdout.json
```

| Profil | Sukcesy | Udział | Dokładny baseline losowy | Baseline % | Próg 23/32 |
| --- | --- | --- | --- | --- | --- |
| m06 | 18/32 | 56.250% | 127/256 | 49.609375% | FAIL |
| m19 | 18/32 | 56.250% | 1/2 | 50.000000% | FAIL |

**Wynik: I51_HOLDOUT_FAIL.**

Wynik nie zmienia pasm, modelu ani progu. Przekazany rodzicowi do decyzji;
ten raport nie jest zamknięciem I51 ani G6.
Macierz: hardFail u 6/512 kandydatów m06 i 1/512 m19; wszyscy zwycięzcy
mają hardFail=0. Porażka progu wynika z porównania odległości holdout.
Kod wyjścia oceny: 1 (ważny pomiar poniżej progu), nie blokada danych.

Sukces: zwycięzca hardFail=0 i odległość ściśle poniżej średniej pozycji
8 i 9 wszystkich 16 kandydatów. Wszystkie seedy i hardFail pozostają
w mianowniku. Baseline to dokładna średnia udziałów kandydatów spełniających
ten sam warunek. Oba profile oceniono na wszystkich mapach.

## Wyniki per mapa i rodzina

| Mapa / rodzina | m06 sukcesy | m06 baseline | m19 sukcesy | m19 baseline |
| --- | --- | --- | --- | --- |
| M01 | 13/32 | 27/64 | 18/32 | 1/2 |
| M05 | 20/32 | 253/512 | 20/32 | 1/2 |
| M11 | 19/32 | 127/256 | 15/32 | 255/512 |
| M12 | 16/32 | 253/512 | 19/32 | 1/2 |
| M13 | 12/32 | 127/256 | 12/32 | 255/512 |
| M14 | 21/32 | 253/512 | 18/32 | 255/512 |
| M15 | 20/32 | 63/128 | 21/32 | 1/2 |
| M16 | 0/32 | 1/256 | 12/32 | 61/128 |
| M17 | 0/32 | 1/256 | 12/32 | 61/128 |
| M18 | 16/32 | 251/512 | 14/32 | 255/512 |
| family.M01 | 13/32 | 27/64 | 18/32 | 1/2 |
| family.M05 | 20/32 | 253/512 | 20/32 | 1/2 |
| family.M19-related | 22/32 | 63/128 | 21/32 | 1/2 |
| family.main | 18/32 | 127/256 | 18/32 | 1/2 |
| family.winter | 18/32 | 253/512 | 14/32 | 255/512 |

Główny wynik = (M01 + M05 + (M11+M18)/2)/3. M12–M17 są wyłącznie
osobnym raportem rodziny pokrewnej M19 i nie wchodzą do progu.

Pełne wyniki 64 par, odległości zwycięzcy/mediany, baseline, porażki,
referencje i przyczyny N/A: [wersjonowany raport JSON](g6_i51_holdout.json).
Surowe raporty kandydatów: lokalnie `scratchpad/i51-matrix-complete/`.

## Powtarzalność i pochodzenie

Raporty obu przebiegów dla każdego profilu są bajtowo identyczne;
wszystkie procesy macierzy zakończyły się kodem 0 i pustym stderr.

| Profil | SHA-256 raportu każdego przebiegu |
| --- | --- |
| m06 | `e9584f1e0a1b4369567f594045f9267466ed3562fbcf78901d1da294f18b9e99` |
| m19 | `59de9ee474f5bcbbd5785f976475e0bf85b3fbaef6e8634d5db121e611c886e3` |

SHA-256 EXE: `0393e799096c060c3da1e6991cb93a6254b6663dea56963b88bea4a3ac34b97c`.

Pasma są wersjonowane w `docs/procedural/g6-bands-v1/`. Ich referencje
sprawdzono względem M06/M19, bez wyznaczania ich z wyników generatora.
Niezależne obliczenie w Pythonie potwierdziło wszystkie 1024 wyniki
rdzenia (inBand i distance) i wybór zwycięzców.

## Audyt prywatnych referencji

PASS: komplet M01–M19 z czterech jawnych serii pod
`C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke`:
`g4-channels-20261108a`, `g4-scale-20261108a`, `g4-trans-20261108a`,
`g4-build-20261108a`. Profile: `g4-profiles/10.98-v10/{m06,m19}.profile`.
Potwierdzono klienta 10.98, zgodne wersje map pomiędzy seriami, potwierdzenia
PASS, zgodność źródła import/semantic/verification, DAT/SPR, keymap oraz
powtarzanych podsumowań kanałów/skali/szlaków. Hashe podsumowań i profili
są w JSON. Nie kopiowano map, profili ani obrazów do wersjonowanych plików.

| Mapa | Znana referencja / 26 | Faktycznie różne wkłady m06 | Faktycznie różne wkłady m19 |
| --- | --- | --- | --- |
| M01 | 8 | 5 | 5 |
| M05 | 8 | 7 | 7 |
| M11 | 7 | 5 | 5 |
| M18 | 5 | 3 | 3 |
| M12 | 10 | 8 | 8 |
| M13 | 7 | 6 | 6 |
| M14 | 10 | 9 | 8 |
| M15 | 11 | 10 | 10 |
| M16 | 5 | 2 | 2 |
| M17 | 5 | 2 | 2 |

Znana referencja oznacza pozycję potencjalnie różnicującą; obcięcie kary
może dać jednakowe wkłady wszystkich zmierzonych kandydatów. Ostatnie dwie
kolumny liczą pozycje rzeczywiście różniące się w macierzy danego profilu.
Żadna oceniana mapa nie jest pozbawiona takich pozycji.

Referencyjne N/A wnosi stałe ln(64) dla każdego kandydata. N/A pochodzi
z braku obiektów, liczebności poniżej pięciu składowych/budynków oraz
progów 1000 dla plam i granic. Szczegółowe przyczyny każdej pozycji są
w JSON. Brak obiektów daje długość/gęstość 0, szerokości/boki/obrys N/A.
Zachowano istniejącą definicję G4: excess bez par ma wartość 0.

## Testy niezależne od pomiaru

- Izolowany build rzeczywistego `source/procedural/CMakeLists.txt`,
  MSVC 19.51.36257 x64 Release, C++23, Ninja 1.13.2: PASS.
- CTest: 21/21 PASS (19 testów rdzenia, CLI, holdout); po zmianach
  skryptów ponowiono właściwe testy CLI/holdout: PASS.
- Ręcznie policzone kolejności: hardFail przegrywa z lepszymi pasmami,
  odległości 875/788 oraz remis indeksem. ln(2)=0.69314718056,
  asinh(1)=0.88137358702, asinh(1)-asinh(-1)=1.76274717404.
  26*ln(64) daje 108131; 24*ln(2)+2*asinh(1) daje 18398.
- Testy granic B/A50/U20 i setnych, ref=0, signed excessów, N/A,
  obcięcia kary i jednego zaokrąglenia: PASS.
- Ręczna agregacja (0+6+(4+8)/2)/3=4, parzysta mediana 0..15=7.5,
  baseline 7/16 z jednym hardFail i średnia 7/32 z seedem remisowym: PASS.
  Pełny przykład 32 par daje 16/32 oraz baseline dokładnie 5/32.
- Brak kolumny, błędna liczba, zły wynik rdzenia, niekompletne seedy,
  usunięta referencja i zmieniona wersja: odmowa pomiaru PASS.
- N=1, SplitMix64, odcisk 1729 100×100
  `17159159863505677800`, brak mutacji: PASS.

## Ograniczenia i przekazanie

Nie wykonano kanonicznego build_ninja.bat, g5_run_suite z sondami,
G5 --i47, GUI, końcowej maski adaptera, gry ani oryginalnego RME.
Nie dowiedziono determinizmu libm między kompilatorami/platformami.
Prywatne podsumowania wykorzystano z historycznych potwierdzeń; nie
uruchamiano ponownego importu map. Potwierdzenia nie zawierają hasha OTB,
więc nie udowodniono niezależnie tożsamości binarnego OTB.

Rodzic niezależnie przegląda, scala ff, wykonuje kanoniczny build,
obie walidacje G5 i raport bramki. build-ninja w tym worktree nie powstał.
Build i pliki robocze pozostały wyłącznie w scratchpad. Nie instalowano
zależności, nie dotykano C:/repo/vcpkg/buildtrees, nie wykonano push.
