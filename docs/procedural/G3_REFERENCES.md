# G3-E — powtarzalność na zestawie referencji, 2026-11-07

**PARTIAL (G3-E).** Cały pipeline G3-A…D uruchomiono na 19 mapach zwykłego klienta 10.98 oraz wcześniej na jednej mapie BlackTek. Raporty są powtarzalne (dwie pełne serie bajtowo identyczne). Pamięć zmierzono, a dla największej mapy (blisko budżetu 2 GB) wprowadzono limit i anulowanie, także w trakcie ładowania (D8, z długiem resztkowym). Bramka G3 jest spełniona dla tych map w opisanym zakresie, ale G3 nie zamykam: patrz „Czego NIE zweryfikowano”.

## Dwa różne klienty (ważne)
„10.98” (zwykły) i „1098-BlackTek” to **różne** zestawy danych: `Tibia.dat` zwykłego 10.98 ma 23 725 itemów, BlackTek 26 381 (sygnatury nagłówków są takie same, `42A3`/`57BBD603`, więc nie wolno ich rozróżniać po sygnaturze). Mapy zwykłego 10.98 analizowano wyłącznie z `Tibia.dat/spr` z folderu tych map i wpisem „10.98” (`dat_otb`); `forgotten.otbm` wyłącznie z BlackTek. Assety nigdy nie były mieszane.

Błąd w moim pierwszym podejściu (zgłoszony przez użytkownika): profil G0 miał `clients.toml` z jednym wpisem (BlackTek), więc wpis „10.98” z `config.toml` nie miał odpowiednika, a start RME wracał do pierwszego klienta (BlackTek) i próbował go ładować. Naprawa: profil dla zwykłego 10.98 to katalog `data/` z repo (ma wpis „10.98”); runner odmawia, gdy żądany klient nie występuje w `clients.toml`, a próbnik odmawia, gdy załadowany klient różni się od żądanego (`CLIENT_MISMATCH`).

## Zestaw (klient 10.98, 19 map)
Mapy prywatne; poniżej aliasy (mapowanie na nazwy tylko lokalnie w `local-data/`). Wszystkie: OTBM 3, items 3.57, przebiegi kończą się same, `stderr` i `wx-log.txt` puste (38 przebiegów).

| id | plik MiB | wymiary nagłówka | kafle | itemy | unknown ground/items | różne id unknown | pokrycie | pięter | klucze gruntu | szczyt pamięci MiB |
|---|---|---|---|---|---|---|---|---|---|---|
| M01 | 0,44 | 256×256 | 38 588 | 60 435 | 43 / 47 | 14 | 99,8 % | 11 | 6 | 244 |
| M02 | 0,01 | 2048² | 393 | 554 | 0 / 0 | 0 | 100 % | 4 | 1 | 236 |
| M03 | 0,01 | 2048² | 503 | 1 225 | 0 / 0 | 0 | 100 % | 1 | 8 | 236 |
| M04 | 0,03 | 2048² | 2 218 | 4 278 | 0 / 0 | 0 | 100 % | 3 | 12 | 236 |
| M05 | 0,22 | 2048² | 24 832 | 29 815 | 0 / 0 | 0 | 100 % | 5 | 27 | 242 |
| M06 | 71,08 | 2000² | 7 835 705 | 8 520 654 | 0 / 48 | 15 | 100 % | 16 | 74 | **1 899** |
| M07 | 5,97 | 5000² | 760 126 | 792 422 | 0 / 0 | 0 | 100 % | 1 | 2 | 451 |
| M08 | 10,27 | 5000² | 1 322 539 | 1 354 835 | 0 / 0 | 0 | 100 % | 1 | 2 | 555 |
| M09 | 5,97 | 5000² | 760 126 | 792 422 | 0 / 0 | 0 | 100 % | 1 | 2 | 452 |
| M10 | 10,12 | 5000² | 1 322 539 | 1 322 539 | 0 / 0 | 0 | 100 % | 1 | 2 | 553 |
| M11 | 0,05 | 2048² | 4 388 | 6 930 | 0 / 0 | 0 | 100 % | 4 | 10 | 238 |
| M12 | 0,21 | 2048² | 23 729 | 30 041 | 0 / 0 | 0 | 100 % | 2 | 3 | 241 |
| M13 | 0,13 | 2048² | 14 343 | 17 323 | 0 / 0 | 0 | 100 % | 2 | 1 | 240 |
| M14 | 0,06 | 2048² | 5 245 | 8 077 | 0 / 0 | 0 | 100 % | 4 | 8 | 238 |
| M15 | 0,07 | 2048² | 6 718 | 9 329 | 0 / 0 | 0 | 100 % | 2 | 4 | 238 |
| M16 | 0,07 | 2048² | 7 422 | 9 268 | 0 / 0 | 0 | 100 % | 4 | 6 | 237 |
| M17 | 0,21 | 2048² | 19 005 | 29 667 | 0 / 0 | 0 | 100 % | 5 | 2 | 241 |
| M18 | 0,02 | 2048² | 1 907 | 3 393 | 0 / 0 | 0 | 100 % | 3 | 8 | 237 |
| M19 | 19,35 | 2048² | 1 888 858 | 2 630 326 | 0 / 0 | 0 | 100 % | 16 | 104 | 717 |

Szczyt pamięci to maksimum `PeakWorkingSetSize` całego procesu (zawiera klienta, assety i GUI, ok. 235 MiB samego startu), odczytywane co 50 ms; seria B dała wartości w granicach ±1 MiB.

## Co to dowodzi
- **Powtarzalność:** dwie pełne serie, 190 plików (3 raporty JSON i 7 plików analizy na mapę) bajtowo identycznych, 0 różnic. Źródłowe mapy i `Tibia.dat/spr` mają identyczne hashe przed i po każdym przebiegu (runner przerywa przy różnicy).
- **Ścieżka `unknown` na prawdziwej mapie:** M01 ma 43 nieznane grunty i 47 nieznanych itemów (14 różnych id), M06 ma 48 nieznanych itemów (15 id); raporty podają id i liczności, pokrycie analizy wynosi 99,8 % dla M01. To zamyka lukę z G3_NORMALIZER.md („unknown tylko na fixture”). Przyczyny wyjaśniono w sekcji „Dlaczego te id są nieznane”.
- **Skala:** 7,8 mln kafli i 16 pięter (M06) w 7,1 s wall całego przebiegu.

## Dlaczego te id są nieznane
Sprawdzono każde z 29 nieznanych id w `data/1098/items.otb` (`tools/tibia_data.py`): wszystkie 14 id z M01 (30 845–41 132) **nie istnieje w items.otb** (to id nowsze niż ten klient, otb ma 26 282 pozycji), a wszystkie 15 id z M06 (4 422–4 437) **mają w items.otb grupę „deprecated”** (pozycje bez id klienta). Czyli `unknown` pochodzi z danych map, nie z błędu importera ani niezgodnego klienta; raport podaje te id i liczności, a normalizator nie zgaduje ich klasy.

## Limit pamięci i anulowanie (D8)
Dług D8 (brak limitu pamięci i anulowania importu) zamknięto 2026-11-07 w dwóch krokach; poniżej stan końcowy z porównaniem „przed → po” (klient 10.98, M06 = mapa 71 MiB, 7,8 mln kafli, jeden przebieg na pomiar, ten sam EXE i maszyna).

**Limit pamięci procesu** (`ReferenceImportLimits.maxProcessBytes`, domyślnie 2 048 MiB w runnerze) działa w trzech miejscach:
1. **Przed ładowaniem:** bieżący working set + dolne ograniczenie `estimateLoadBytes` (rozmiar nieskompresowanego pliku × 16; dla gzip rozmiar z trailera ISIZE). Odmawia tylko mapy, które nie zmieszczą się nawet przy najniższej zaobserwowanej gęstości (zmierzone 23,4–36,1 B na bajt dla plików ≥ 5 MiB, nigdy poniżej 18). Pierwsza wersja używała górnego ograniczenia 40 B/B i odrzucała M06 przy 2 GiB, choć mieści się.
2. **W trakcie ładowania (nowe):** loader `IOMapOTBM` ma opcjonalny predykat przerwania (`setAbortCheck`), który wątek wywołujący sprawdza co 5 ms; wątki robocze kończą na następnym kaflu. Predykat zwraca prawdę przy zgłoszeniu `stop_token` albo gdy working set przekroczy limit. Dzięki temu limit obowiązuje także podczas ładowania.
3. **Przed zbudowaniem modelu semantycznego:** zmierzony working set + `analysisWorkingBytes` (6 B/komórkę + 16 B/komórkę największego piętra).

| przypadek | przed (górny szacunek, bez przerwania) | po (dolny szacunek + przerwanie w ładowaniu) |
|---|---|---|
| M06, limit 2 048 MiB | odmowa przed ładowaniem | **PASS**, szczyt 1 900 MiB (ładowanie 614 ms, normalizacja 1 303 ms) |
| M06, limit 1 500 MiB | (nie dotyczy: szacunek odrzucał wcześniej) | przerwane w ładowaniu: `memory_budget_exceeded`, szczyt **1 503 MiB**, ładowanie 503 ms |
| M06, limit 1 000 MiB | odmowa przed ładowaniem | odmowa przed ładowaniem (dolny szacunek 1 137 MiB + 234 MiB) |
| M06, estymator zaniżony do 1 B/B, limit 1 200 MiB (test samej kontroli w ładowaniu) | (z limitem 1 500 MiB) szczyt **1 793 MiB**, czyli nadwyżka ok. 293 MiB | przerwane w ładowaniu, szczyt **1 221 MiB**, czyli nadwyżka 21 MiB |

**Anulowanie** (`stop_token`): sprawdzane przed i po haszowaniu, w trakcie ładowania (co 5 ms, predykatem), po ładowaniu, co 4 096 kafli w normalizacji, co wiersz w topologii, cechach i obrazach. Opóźnienie od żądania do powrotu z `normalizeReference` na M06 (jeden przebieg na punkt):
| żądanie po | etap | przed | po |
|---|---|---|---|
| 0–150 ms | przed ładowaniem (haszowanie) | 154–304 ms | 150–300 ms |
| 300–600 ms | **w trakcie ładowania** | 1 433–1 713 ms | **78–310 ms** |
| 700 ms | ładowanie (koniec) | 1 433 ms | 650 ms |
| 1 000–2 200 ms | normalizacja | 1 196–1 217 ms | 1 164–1 207 ms |
| 3 000 ms | topologia | 428 ms | 557 ms |
| 4 500 ms | topologia | 5 ms | 5 ms |
W importie opóźnienie zawiera zniszczenie wczytanej mapy przy powrocie (ok. 1,2 s dla w pełni wczytanej mapy 7,8 mln kafli); przerwanie w ładowaniu niszczy tylko część mapy, stąd mniejsze wartości. Przy normalizacji zostaje ok. 1,2 s.

**Regresja:** po zmianach pełny zestaw 19 map przy domyślnym limicie 2 048 MiB (M06 też przechodzi) dał 295 plików (190 analiz i 105 plików semantycznych) bajtowo identycznych z serią sprzed zmian w loaderze, a weryfikator PNG przeszedł 100 % kontroli; `forgotten.otbm` (BlackTek) bez zmian; stderr i `wx-log.txt` puste. **Ścieżka edytora (bez haka):** to samo wczytanie bez predykatu daje identyczną liczbę kafli jak import z hakiem na 5 mapach (M19 1 888 858, 5000×5000 z wodą 1 322 539, mapa 19 005, mapa 38 588, `forgotten.otbm` 340 595).

**Dług resztkowy (D8 w `TECH_DEBT.md`):** (1) samo ładowanie to nadal ok. 94 % szczytu (1 793 z 1 900 MiB dla M06), więc mapa wiele większa niż 71 MiB nie zmieści się w 2 GiB; zmiana tego wymaga innego sposobu wczytywania niż `Map` (np. strumieniowy importer bezpośrednio do modelu semantycznego), czego nie zrobiono; (2) opóźnienie anulowania przy normalizacji to ok. 1,2 s (zniszczenie wczytanej mapy); (3) dolne ograniczenie szacunku sprawia, że mapa rzadka i duża jest ładowana aż do limitu, zanim zostanie przerwana (strata czasu, nie pamięci: nadwyżka 3–21 MiB); (4) kalibracja na 19 mapach jednego klienta, pomiar pamięci tylko na Windows, pojedyncze przebiegi; (5) zmiana w `source/io` dotyczy też edytora (ścieżka bez haka): zweryfikowana tylko równością liczby kafli, nie ręcznym testem w GUI.

## Kontrola niezależna: statystyki RME (GUI, ścieżka edytora)
Użytkownik otworzył `forgotten.otbm` w zwykłym RME (po zmianie w loaderze, bez haka przerwania) i podał okno **Map → Statistics** (2026-11-07). Zestawienie z raportami tego pipeline'u (klient BlackTek):
| wielkość | RME (edytor) | pipeline G3 |
|---|---|---|
| kafle | 340 595 | 340 595 |
| itemy | 429 037 | 429 037 |
| kafle nieprzechodnie (RME) / kafle z cechą `blocking` | 244 283 | 244 283 |
| kafle przechodnie (RME) / pola przechodnie wg G3 | 96 312 | 95 103 |
| przedmioty przesuwalne (RME liczy itemy) / kafle z `loose_item` | 1 361 | 1 238 |
| kontenery niepuste (RME) / kafle z `container` (też puste) | 5 | 431 |
Trzy pierwsze wartości są identyczne, co jest niezależnym potwierdzeniem importu, liczenia itemów i cechy `blocking` oraz tego, że wczytanie przez edytor po zmianie loadera działa. Różnice w pozostałych wierszach wynikają z innych definicji, nie z błędu: RME uznaje za przechodni każdy kafel bez blokady, a G3 wymaga jeszcze gruntu i braku nieznanej zawartości (96 312 − 95 103 = 1 209 kafli bez gruntu i bez blokady; tych kafli nie sprawdzałem pojedynczo); RME liczy itemy przesuwalne, G3 kafle z takim itemem; RME liczy tylko kontenery z zawartością, G3 każdy kontener. Nie porównano pozostałych pozycji (kafle „detailed” 39 085, depoty 42, itemy z Action/Unique ID, domy i miasta), bo pipeline ich nie liczy.

## Czego NIE zweryfikowano
- Nie oceniano, czy klasyfikacje są sensowne dla tych map (np. czy cechy odróżniają style), ani nie oglądano ich w RME. Cechy i raporty są mierzone, nie ocenione.
- Nie sprawdzono, czy `data/1098/items.otb` jest dokładnie tym `items.otb`, z którym te mapy powstały; wskazuje na to zgodność (OTBM 3, items 3.57, 0 unknown na 17 z 19 map), nie dowód. Id unknown w M01 i M06 wyjaśniono osobno (sekcja „Dlaczego te id są nieznane”), co nie wyklucza niedopasowania `items.otb` na innych mapach.
- Wyniki z dwóch serii pochodzą z jednej maszyny i jednego EXE; nie testowano innego locale systemu.
- Czasy to jeden komplet pomiarów (dwie serie, ten sam wynik w granicach 0,1 s dla wall), nie benchmark przed/po; nie wydzielono czasu poszczególnych etapów.
- Anulowanie importu: przerywalne w ładowaniu (hak w loaderze), opóźnienie zmierzone; ok. 1,2 s przy normalizacji (zniszczenie mapy) zostaje (patrz „Limit pamięci i anulowanie”).
- Brak części cech z sekcji 4.2 planu (rzeki, jaskinie, szlaki, osady, mikrostruktury, test podobieństwa); patrz G3_FEATURES.md. Nie wykonano podziału train/validation/test ani oceny podobieństwa między mapami (zakres G4).
- Pliki towarzyszące (house, spawn, zones) nie są wczytywane.
