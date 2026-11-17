# I51 — rdzeń rankingu i narzędzie offline

Stan implementacji: **częściowy; I51 i G6 nie są zaliczone**. Kryteria z
[`G6_METRICS.md`](../../../docs/procedural/G6_METRICS.md), commit `9149923`, pozostają bez zmian.
Nie zmieniono generatora, GUI, profili, zapisu map ani domyślnego pędzla `mountain`.

## Interfejs

`candidate_score.{h,cpp}` mierzy powierzchnię istniejącymi funkcjami G4 i ocenia ją
wobec jawnie dostarczonych `CandidateBand`. Porównanie jest leksykograficzne:
`hardFail` rosnąco, `inBand` malejąco, `distance` rosnąco, indeks rosnąco.
Odległość to `round(1000 * suma(abs(log(value/ref))))`; dla wartości 0 logarytm
ilorazu przyjmuje `log(1/64)`. Zaokrąglenie następuje raz, po sumowaniu w stałej
kolejności metryk; porównywana jest liczba całkowita. Powtarzalność sprawdzana jest
na tym samym buildzie MSVC x64; nie dowiedziono zgodności bibliotecznego `log`
między różnymi platformami i kompilatorami.

Warunek twardy obejmuje nieudaną walidację, brak pełnej osiągalności POI,
niechodliwe krawędzie Z, `unrepaired` oraz usunięte POI, czapy, piętra budynków
i lochy. Usunięta roślinność nie oznacza usuniętych miejsc. Przycinanie nieobsadzonych
składowych jaskini (`removedCaveComponents`) jest osobnym licznikiem budowy/naprawy,
nie licznikiem usuniętych POI. `scoreCandidate` przyjmuje wynik walidacji producenta;
sam nie zastępuje walidacji adaptera na danych klienta.

`candidateSeed(seed, 0) == seed`. Dalsze indeksy używają SplitMix64:
`x = seed + index * 0x9e3779b97f4a7c15`, następnie finalizatora z mnożnikami
`0xbf58476d1ce4e5b9`, `0x94d049bb133111eb` i przesunięciami 30, 27, 31.
Wszystkie działania są na `uint64_t`, z określonym przepełnieniem.

## Uruchomienie

Cele CMake: `candidate_rank`, `test_procedural_candidate_score`.

```text
candidate_rank PROFILE SIDE FIRST_USER_SEED LAST_USER_SEED
  [--count 1..16] [--bands FILE]
  [--path-length 0..400] [--building-density 0..100]
  [--rock-blocks 0|1] [--g5-levels]
```

Domyślne N=4, maksimum 16. Długość szlaków i gęstość budynków nie należą do
profilu `g4-profile/6`, dlatego są jawnymi argumentami, jak w pomiarach G5 (D20).
`--g5-levels` ustawia geometrię I47: naprawa, brody 48, ruiny 20%, obozy 4,
wejścia 16, trzy czapy (minimum 64, maksimum 10%), przełęcze 4, dalekie wyjścia,
korytarze, piętra budynków i lochy. Brak ponawiania seedów. Kształty ramp oraz
blokowanie klas dekoracji są modelem offline używanym przez `style_render`;
nie jest to pomiar na końcowej masce adaptera.

Bez `--bands` narzędzie zapisuje wyłącznie pomiary, jawnie z `mode=measure-only`,
bez zwycięzcy i bez oceny bramki. Z `--bands` wybiera najlepszego z N kandydatów
dla każdego seeda użytkownika. Jeśli wszyscy mają `hardFail=1`, zwycięzca nadal
ma tę flagę; nie oznacza to zaakceptowania wyniku do zapisu.

Plik pasm ma nagłówek `g6-ranking-bands/1`, następnie wiersze:

```text
metric reference lowerHundredths upperHundredths
```

Granice są inkluzywne i podane w setnych jednostki metryki: pasmo względne
zapisuje się jako `67*ref` i `150*ref`. Próg bezwzględny trzeba przenieść zgodnie
z odpowiednim zapisem G4/G5. Nazwy i jednostki wypisuje `candidate_rank --help`.
Ten format przenosi jawne dane wejściowe oceny; nie ustanawia nowych pasm ani
nie wybiera listy metryk dla m06/m19. Duplikaty, puste pasma, nieznane metryki,
niepoprawne granice i niedodatnie referencje są odrzucane.

Wyjście TSV zawiera seed użytkownika, indeks, seed mieszany, odcisk pierwotnej
powierzchni (przed budową/naprawą poziomów), warunek twardy, wynik i surowe metryki
powierzchni **po** budowie/naprawie. Brak mierzalnego ilorazu oznaczony jest `NA`,
nie zerem. Zerowa długość kanałów/szlaków i zerowa gęstość budynków pozostają zerem.
Brak wybranej metryki albo jej ujemna wartość daje `score-error` i kod wyjścia 4;
narzędzie nie ogłasza wtedy zwycięzcy dla tego seeda. Kod 2 oznacza błędne wejście
lub błąd potoku generacji/pomiaru. Raport częściowy przy niezerowym kodzie wyjścia
nie jest wynikiem zaliczenia. Odcisk początkowej powierzchni dla N=1 z domyślnym
strojeniem, seedem 1729 i bokiem 100 wynosi `17159159863505677800`.

Pełną macierz powtarzalności uruchamia:

```text
rtk proxy py -I docs/procedural/smoke/g6_measure_candidates.py EXE PROFILE_DIR scratchpad/i51
  [--bands-dir DIR] [--g5-levels]
```

Skrypt używa obu profili, seedów użytkownika 1001–1032, N=16 i boku 512,
z ustawieniami szlaków/budynków 40/30 i 80/40 oraz `rockBlocks` 0/1.
Uruchamia każdy wariant dwa razy i porównuje bajty raportów. Bez zatwierdzonych
plików `m06.bands` i `m19.bands` wynik jest wyłącznie
`MEASUREMENT_REPEATABILITY_PASS`, nigdy zaliczeniem rankingu I51.

Test CLI ze sztucznymi danymi, bez prywatnych map:

```text
rtk proxy py -I tests/test_candidate_rank.py EXE scratchpad/cli-tests
```

## Konkretne luki do rozstrzygnięcia przed zaliczeniem I51

1. `g4-profile/6` zawiera strojenie, udziały i sąsiedztwa, ale nie referencje ani
   pasma kształtu. Historyczne referencje są w tabelach G4. Brakuje jednoznacznej
   listy metryk wchodzących do wyniku I51 dla każdego profilu: przykładowo E4 jest
   kontrolą w I6, a `paths.B` zostało wyłączone z kryterium I9d jako artefakt
   szkieletowania. Nie wybierano listy na podstawie obecnych wyników generatora.
2. I51 określa wartość zerową, lecz nie referencję 0, ujemny `scale.E*`, N/A oraz
   brak wymaganych składowych (rzek, szlaków, budynków). `log(value/ref)` jest
   wtedy nieokreślony lub niemożliwy do policzenia. Rdzeń jawnie odmawia wyniku
   zamiast dopisywać epsilon, usuwać metryki zależnie od kandydata lub zmieniać wzór.
3. `G4_COMPARE.md` opisuje M06 i M19, ale nie zawiera wskazanego przez I51 ręcznego
   przypisania M01/M05/M11/M18/M12–M17 do profilu bliższego stylem.
   Nie ustalono też sposobu sprowadzenia wyników per mapa do decyzji dla pary
   `(seed użytkownika, profil)` przy różnych odpowiedziach poszczególnych map.
   Nie wybrano agregacji ani losowego baseline po obejrzeniu wyników.
4. `G4_SPLIT.md` łączy M12–M17 **razem z M19** w jedną rodzinę po treści.
   Jest to sprzeczne z opisem tych map w G6 jako rodzin spoza M19.
   Wyniku na nich nie można bez dodatkowego rozstrzygnięcia nazwać niezależnym
   testem poza rodziną M19. Listy holdout i kryterium ≥70% nie zmieniono.

Z tych powodów nie powstał domyślny, oficjalny zestaw pasm m06/m19 ani ocena
holdout, a implementacja pozostaje częściowa. Nie policzono ani nie ogłoszono
spełnienia progu 70% lub wyniku losowego wyboru. Potrzebne są decyzje uzupełniające
przed tym pomiarem; ten dokument nie zmienia `G6_METRICS.md`.

## Walidacja wykonawcy (2026-11-17)

Build izolowanego rdzenia: MSVC 19.51.36257 x64, Release, C++23, konfiguracja
CMake w `scratchpad/build`. Wykorzystano rzeczywisty `source/procedural/CMakeLists.txt`
z pominięciem budowy aplikacji i jej zależności. Nie uruchamiano instalacji
zależności ani nie dotykano `C:/repo/vcpkg/buildtrees`.

Pełna lista CTest: **19/19 PASS**:

| Test | Wynik |
| --- | --- |
| test_procedural_core | PASS |
| test_procedural_surface | PASS; odcisk 1729 bez zmian |
| test_procedural_reference_header | PASS |
| test_procedural_semantic_map | PASS |
| test_procedural_topology | PASS |
| test_procedural_features | PASS |
| test_procedural_semantic_image | PASS |
| test_procedural_similarity | PASS |
| test_procedural_style_profile | PASS |
| test_procedural_style_fit | PASS |
| test_procedural_decor | PASS |
| test_procedural_style_profile_set | PASS |
| test_procedural_water | PASS |
| test_procedural_scale | PASS |
| test_procedural_settlement | PASS |
| test_procedural_reachability | PASS |
| test_procedural_region_graph | PASS |
| test_procedural_underground | PASS |
| test_procedural_candidate_score | PASS |

Nowy test sprawdza ręczne odległości 0/875/788 (jedna metryka: 182/95) i kolejność czterech kandydatów,
przegraną warunku twardego mimo lepszych pasm, priorytet liczby pasm, oba remisy,
granice inkluzywne, `log(1/64)`, zaokrąglenie sumy, niezależność od kolejności pasm,
odmowy N/A/ujemnych wartości/zerowych referencji/duplikatów/niepoprawnej siatki,
warunki twarde (w tym krawędzie Z i kategorie POI), wektor kontrolny SplitMix64,
seedy skrajne, N=1, odcisk, brak mutacji, wspólne jednostki G4, niezgodny plan
i anulowanie.

`tests/test_candidate_rank.py`: **CANDIDATE_RANK_CLI_PASS** — sztuczny profil,
ranking z ręcznie podanym pasmem testowym, dwa identyczne raporty, niezależne
odtworzenie porządku z TSV, domyślne N=4, N=1/16, odcisk, seed `UINT64_MAX`,
odmowy limitów/składni/nieznanej metryki/duplikatów/zerowej referencji/N/A,
smoke poziomów z `rockBlocks=1`. Pasmo testowe nie jest pasmem m06 ani m19.

`g6_measure_candidates.py`: macierz pomiarowa m06/m19, 32 × 16 kandydatów na profil,
dwa przebiegi, bok 512 — **MEASUREMENT_REPEATABILITY_PASS**, 2048 pomiarów łącznie.
Tryb bez `--g5-levels` i bez `--bands`; nie zalicza pełnego kryterium 2 I51,
bo brakuje zatwierdzonej polityki rankingu. Raporty robocze w
`scratchpad/i51-repeatability-final/`.

SHA-256 obu identycznych raportów dla każdego profilu:

- m06: `be8d4dc3c21e8039c9092374f02cfce888a9b6e1e7a8695b7c6a62429ecea721`
- m19: `05879a8b0f839539ac7c919bb401c058ac4f80c51357ce3f99d7d9bfcfed47c6`

`g5_run_suite.py`: **SUITE_FAIL**, kod 1: brak Ninja na PATH i skonfigurowanego
`build-ninja` w tym worktree. To nie jest SUITE_PASS; niezależny CTest nie
zastępuje sond GUI. Nie uruchamiano `g5_measure_levels.py --i47`, kanonicznego
`build_ninja.bat`, sond GUI, testu holdout, gry ani oryginalnego RME. Rodzic
niezależnie przegląda i scala, wykonuje kanoniczny build i obie walidacje G5,
zapisuje `G6_GATE_STATUS.md`. G6 pozostaje otwarte.
