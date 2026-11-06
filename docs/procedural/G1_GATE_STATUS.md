# G1 — stan kryteriów na 2026-11-05

**G1 IN PROGRESS.** Zakres bramki wynika z [planu generatora](../PROJECT_RME_PROCEDURAL_GENERATOR.md) i [planu testów](TEST_PLAN.md#testy-do-dodania-wraz-z-g1). Poniższa macierz uwzględnia dowody późniejsze niż listy otwartych prac w raportach etapowych.

| Kryterium | Aktualny dowód i jego zakres |
| --- | --- |
| 100×100 i 500×500 na jednym piętrze | Oba rozmiary przeszły funkcjonalnie w natywnym RME na EXE `15c25557…`. Status obu przebiegów OTBM pozostaje częściowy z powodu ostrzeżenia `SetFocus`. [Round-trip](G1_ROUNDTRIP.md) |
| Preview zgodny z Place, jedno Undo i Redo | Obserwator porównał wszystkie wygenerowane kafle z niezależnie przygotowanym wynikiem. Place utworzył jeden wpis historii. Rdzeń ma także próby atomowości i odrębnej historii. [Round-trip](G1_ROUNDTRIP.md), [commit](G1_COMMIT.md), [regresja etapowa](G1_STAGED_CORE.md) |
| Komplet plików po zapisie i ponownym otwarciu | OTBM 5 i pięć companionów XML dla 1098-BlackTek sprawdzono w pełnych próbach obu rozmiarów. Trzy próby czasowe 500×500 zapisały komplet identyczny bajtowo z zachowaną pełną próbą tego rozmiaru. [Round-trip](G1_ROUNDTRIP.md), [powtarzalność](G1_PLACE_TIMING_REPEATABILITY.md) |
| Brak zmian poza maską i w danych chronionych | Pełny wynik prostokąta i fixture sprawdzono w round-tripie. Test natywnej dokładnej maski potwierdził zachowanie jednego chronionego kafla wewnątrz maski, wraz z UID 32101, tożsamością obiektów i zaznaczeniem. [Round-trip](G1_ROUNDTRIP.md), [macierz dialogu](G1_DIALOG_MATRIX.md) |
| Zmierzony budżet zasobów 500×500 | Aktualny build ma pomiary etapowego Preview oraz trzy próby Place, Undo, Redo i Save. Ocena pozostaje otwarta do uzgodnienia progów. [Cancel](G1_CANCEL.md), [powtarzalność](G1_PLACE_TIMING_REPEATABILITY.md) |
| Zmierzony budżet zasobów 100×100 | Aktualny build ma jeden zweryfikowany pomiar Place, Undo, Redo i Save. Pomiar Preview, Validate i Cancel pochodzi ze starszego synchronicznego buildu. Ocena pozostaje otwarta do uzgodnienia progów. [Pomiar 100×100](G1_ROUNDTRIP.md#pomiar-place-undo-redo-i-save-dla-100100), [pomiar bazowy](G1_RESOURCES.md) |

Rdzeń C++, resolver i sandbox mają dowody dla seedów, błędnych wejść i deterministycznego diffu. Natywny dialog pokrywa Preview, Validate, Place, Cancel i odmowę starego zaznaczenia. Logi operacji dialogu mają dowód runtime tylko dla ścieżek z przebiegu odmów (patrz niżej); Validate, Cancel i Place nie były w nim objęte. Podgląd MVP sprawdzono jako minimapę. [Rdzeń](G1_CORE.md), [resolver](G1_RESOLVER.md), [sandbox](G1_PREVIEW.md), [macierz dialogu](G1_DIALOG_MATRIX.md)

Anulowanie aktywnego Preview 500×500 ma aktualny dowód. Natywne odmowy Preview, w tym jednorazowa awaria alokacji halo, oraz edycja seeda i zamknięcie dialogu w trakcie pracy mają dowód funkcjonalny na EXE `1660d46b…`. Porównanie manifestów z `15c25557…` wykazało zmiany w `terrain_preparation.cpp` i `terrain_generator_dialog.cpp`. Ponowienie `--refusals` na drzewie z 2026-11-06 (`NEW_RUN_REFUSALS_2`, EXE obserwatora `6de317d2…`, produkcyjny `b87d1abb…`) przeszło funkcjonalnie: sześć przypadków i `REFUSAL_PASS`, verifier `PARTIAL…FOCUS_WARNING` jak poprzednio. To odświeża dowód dla zmiany preflightu. Współistnienie wyników, wycieki oraz zakres testu ograniczenia wariantów i prób naprawy pozostają otwarte. [Cancel](G1_CANCEL.md), [odmowy](G1_REFUSALS.md), [unieważnienie](G1_INVALIDATION.md)

Pomiar Preview 100×100 na etapowym buildzie wykonano 2026-11-06 (patrz niżej). Diagnoza `SetFocus` w przebiegu odmów: [G1_FOCUS_TRACE.md](G1_FOCUS_TRACE.md#stan-okien-przy-błędzie-w-przebiegu-odmów-2026-11-06); w pozostałych przebiegach pozostaje hipotezą.

## Wstępne progi zasobów (2026-11-06)

Progi ustalono po obejrzeniu pomiarów z 2026-11-03…05, więc są **wstępne**: zaliczenie wymaga świeżej serii prób na jednym buildzie, ocenianej względem poniższych wartości bez ich zmiany.

| Metryka | 500×500 | 100×100 |
| --- | --- | --- |
| Preview do gotowości | ≤ 3 s | ≤ 250 ms (zob. zatwierdzenie niżej) |
| Maks. przerwa timera GUI, Preview | ≤ 250 ms | ≤ 250 ms |
| Place do zmiany historii | ≤ 1 s | ≤ 250 ms |
| Maks. przerwa timera GUI, Place | ≤ 750 ms | ≤ 250 ms |
| Przyrost working set, Preview | ≤ 150 MiB | ≤ 30 MiB |
| Przyrost working set / private commit, Place+Undo+Redo+Save | ≤ 200 MiB | ≤ 30 MiB |
| Pamięć po Cancel | bez trendu wzrostowego w 10 cyklach | j.w. |

## Zatwierdzenie progów 100×100 (2026-11-06)

Użytkownik zatwierdził progi 100×100 z tabeli powyżej. W tabeli pozycja „Preview do gotowości” miała wartość „do pomiaru”; wpisano w jej miejsce ≤ 250 ms (zmierzone ok. 110 ms). Tę konkretną wartość zaproponowano w rozmowie i użytkownik nie potwierdził jej osobno, więc jest ona do potwierdzenia. Zatwierdzone progi nie zmieniają wcześniejszych wyników: jedna seria 100×100 na buildzie etapowym mieści się w nich, ale seria jest pojedyncza i nie powtórzono jej.

## Logi operacji dialogu (2026-11-06)

`stdout.log` przebiegu `NEW_RUN_REFUSALS_2` zawiera wpisy `Terrain generator: …` dla: gotowości do wyboru obszaru (info), pięciu odmów (warning), dwukrotnie „Preparing preview” (info) i końcowego „Preview ready” z liczbą kafli, seedem i szacunkiem pamięci (info). Wpisów Validate, Cancel i „placed N tiles” w tym przebiegu nie wykonano, więc kryterium logów było potwierdzone częściowo.

Przebieg `g1-logs-roundtrip-100-20261106` (`--roundtrip --roundtrip-timing`, side 100) dodał „Preview validated. 10000 tiles will change when you place it.” (info) i „placed 10000 tiles as one undo step” (info). Cancel potwierdza przebieg `g1-cancel-logs-4-20261106` (`--cancel-preview`): w logu „Preparing preview” (16:16:00.419) i 0,3 s później „Preview cancelled. Press Preview to try again.” (info), a obserwator zapisał `RESOURCE_IN_FLIGHT_CANCEL_PASS` (pomiar `interrupted-preview` 277 ms, `interrupt` 36 ms). Ten przebieg **nie przeszedł jako całość**: po ponownym Preview operator anulował zamiast przejść Validate i obserwator zakończył `FAILED Cancel requires observed Validate`. Zaliczono więc tylko anulowanie w locie i jego log, nie ponowienie z seedem 1728 ani trzy cykle zasobów. Trzy wcześniejsze próby tego dnia padły: dwie, bo Preview 500×500 kończyło się po ok. 1,35 s przed Cancel, jedna, bo Esc zamknęło dialog przed startem Preview. Kryterium logów jest tym samym potwierdzone dla Preview, Validate, Place, Cancel i odmów.

## Pomiar 100×100 na obecnym buildzie (2026-11-06)

EXE produkcyjny `b87d1abb…`, obserwator `87ffbd44…`, verifier `PARTIAL_G1_GUI_ROUNDTRIP_TIMING_FOCUS_WARNING` (jedyne ostrzeżenie `SetFocus` 0x57), CSV `4bc89b1b…`. Pomiar kwantyzowany do 20 ms.

| Operacja | Czas | Maks. przerwa GUI | Wzrost working set | Wzrost private |
|---|---|---|---|---|
| Place | 63,1 ms | 89,0 ms | 4,3 MiB | 4,9 MiB |
| Undo | 25,8 ms | 37,3 ms | 2,6 MiB | 3,0 MiB |
| Redo | 20,5 ms | 54,4 ms | 0,6 MiB | 0,7 MiB |
| Save | 85,9 ms | 89,7 ms | 0,0 MiB | 0,0 MiB |

Wzrost łączny od początku Place do końca Save: working set 4,4 MiB, private commit 5,4 MiB. Względem wstępnych progów 100×100: Place ≤ 250 ms, przerwa GUI ≤ 250 ms i przyrost ≤ 30 MiB są spełnione w tej jednej próbie. To pojedyncza próba, więc nie spełnia warunku „świeżej serii prób”. Pomiar Preview 100×100 i seria powtórzeń pozostają otwarte.


## Anulowanie i seria zasobów Preview 500×500 (2026-11-06)

Przebieg `g1-cancel-logs-6-20261106` (`--cancel-preview`) przeszedł w całości: anulowanie w locie, retry z seedem 1728 i trzy pełne cykle Preview, Validate, Cancel. Runner `passed: true`, stderr pusty, verifier `PASS_G1_GUI_IN_FLIGHT_CANCEL_ONLY`, EXE produkcyjny `b87d1abb…`, obserwator `506b714c…`. Wcześniejsze próby tego dnia (nr 1–5) padły z przyczyn operatorskich: Cancel w niewłaściwym momencie lub zbyt krótkie okno; pozostają w `local-data` bez zaliczenia.

| Cykl | Preview | Maks. przerwa GUI, Preview | Wzrost WS, Preview | Pamięć po Cancel (WS / private) |
|---|---|---|---|---|
| 1 (seed 1728) | 761 ms | 139 ms | 84,7 MiB | 750,8 MB / 589,6 MB |
| 2 | 1739 ms | 152 ms | 85,1 MiB | 751,6 MB / 592,4 MB |
| 3 | 1417 ms | 154 ms | 84,1 MiB | 752,6 MB / 592,9 MB |

Względem wstępnych progów 500×500: Preview ≤ 3 s, przerwa GUI ≤ 250 ms i przyrost WS ≤ 150 MiB są spełnione we wszystkich trzech cyklach. Czas Preview waha się 0,76–1,74 s, więc próg trzeba ocenić z większej serii. Pamięć po Cancel rośnie o około 1 MB working set na cykl, ale trzy cykle nie rozstrzygają, czy to trend. Próg „bez trendu wzrostowego w 10 cyklach” **nie jest oceniony**: wymaga 10 cykli.

Log dialogu zawiera pełną sekwencję: „Preparing preview”, „Preview cancelled”, „Preview ready … (seed 1728)”, „Preview validated” i dwa kolejne cykle.

## Trend pamięci po Cancel (2026-11-06)

Nowy próbnik `tests/test_procedural_dialog_leak.cpp` i runner `docs/procedural/smoke/g1_run_leak_trend.py` są **samosterowane**: wysyłają kolejkowane zdarzenia menu i przycisków do dialogu, więc sprawdzają produkcyjne obsługi, ale nie natywne wejście operatora. Cykl: otwarcie z menu, Preview, Cancel w locie, Preview, Validate, Cancel, 1 s odczekania, pomiar. Przy starcie aplikacji pojawia się modalny dialog „Warnings” (ostrzeżenia ładowania klienta, `EditorFactory::EnsureVersion`); próbnik zamyka go sam i zapisuje to w `warnings-dialog-dismissed.txt`.

Kryterium ustalone przed pierwszym przebiegiem i nieulegające zmianie: 2 cykle rozgrzewki, potem nachylenie metodą najmniejszych kwadratów working set i private ≤ 1 MiB na cykl oraz brak wzrostu obiektów GDI, USER i uchwytów względem cyklu 2. 500×500, EXE produkcyjny `b87d1abb…`.

| Przebieg | Cykle | Nachylenie WS | Nachylenie private | GDI / USER / uchwyty | Wynik |
|---|---|---|---|---|---|
| `g1-leak-10d-20261106` | 10 | +1,55 MiB/cykl | +1,99 MiB/cykl | −12 / −1 / 0 | **nie spełnia** |
| `g1-leak-30-20261106` | 30 | +0,79 MiB/cykl | +0,95 MiB/cykl | 0 / 0 / 0 | spełnia, na granicy |

Przebieg 30-cyklowy mieści się w kryterium z niewielkim zapasem (0,95 przy limicie 1,0). Wzrost nie jest liniowy: private rośnie skokowo (np. około +12 MiB przy cyklu 21), a w cyklach 21–30 nachylenie jest ujemne (−0,25 MiB/cykl). Cykle 3–10 mają nachylenie 1,84 MiB/cykl, cykle 11–20 0,80. Taki kształt pasuje do fragmentacji sterty lub rosnącej puli alokatora, który się stabilizuje, ale nie rozstrzyga o braku wycieku. Uchwyty i obiekty GUI są stałe. Kryterium „bez trendu wzrostowego” z planu jest więc **spełnione formalnie przy 30 cyklach**, a nie przy 10, i wynik zależy od liczby cykli. Wymaga powtórzenia, żeby ocenić zmienność między przebiegami.

### Powtórzenia 30 cykli (2026-11-06)

| Przebieg | Nachylenie WS | Nachylenie private | Wzrost private cykle 3→30 | GDI / USER / uchwyty względem cyklu 2 | Wynik wg kryterium |
|---|---|---|---|---|---|
| `g1-leak-30-20261106` | +0,79 | +0,95 | 22,0 MiB | 0 / 0 / 0 | spełnia |
| `g1-leak-30-r2-20261106` | +0,77 | +0,90 | 22,5 MiB | +12 / +1 / 0 | **nie spełnia** (GDI, USER) |
| `g1-leak-30-r3-20261106` | +0,78 | +0,90 | 21,9 MiB | +1 / 0 / 0 | **nie spełnia** (GDI) |

Nachylenia pamięci są powtarzalne (WS 0,77–0,79, private 0,90–0,95 MiB/cykl, wszystkie poniżej limitu 1,0), a nachylenie spada w kolejnych dekadach cykli (private w przebiegu r2: 1,89 → 0,73 → 0,02; r3: 1,73 → 0,64 → −0,04). Wzrost łączny ok. 22 MiB za 27 cykli jest niemal identyczny w trzech próbach i skupia się w pierwszych 20 cyklach.

Dwie porażki wynikają ze składnika GDI/USER, który porównuje **pojedynczą próbkę** końcową z próbką cyklu 2. Serie GDI oscylują między dwoma poziomami (np. 481 i 493, różnica 12 obiektów, w r1 477 i 489) i nie rosną; USER oscyluje o 1. Cykl 2 trafił w niski poziom, więc ostatni cykl na wysokim daje formalny „wzrost”. Kryterium zostawiono bez zmian zgodnie z ustaleniem sprzed pierwszego przebiegu, więc oficjalny wynik to jedno spełnienie i dwie porażki. Uzupełniająca obserwacja post hoc: maksimum GDI w cyklach 21–30 nie przekracza maksimum w cyklach 3–12 w żadnym z trzech przebiegów (493 / 493 / 493 oraz 489 w r1). To nie zastępuje kryterium; ewentualna zmiana na porównanie maksimów wymaga decyzji i ponownego przebiegu na tej samej definicji.

Przyczyna oscylacji GDI i skoków private nie została zbadana.

## Preview 100×100 na buildzie etapowym (2026-11-06)

Przebieg `g1-res-100-20261106` (`--resources`: trzy cykle 100×100 i trzy 500×500, każdy Preview, Validate, Cancel, natywne wejście operatora). Runner `passed: true`, stderr pusty, verifier `PASS_G1_GUI_RESOURCE_BASELINE_ONLY`, EXE produkcyjny `b87d1abb…`, obserwator `424eb788…`. Weryfikator wymagał wcześniej starej rewizji produkcyjnej dla tego trybu; zmieniono go tak, by `--resources` był traktowany jak build etapowy (usunięto gałąź porównania ze starym buildem).

| Cykl | Rozmiar | Preview | Maks. przerwa GUI, Preview | Wzrost WS, Preview | Wzrost private, Preview |
|---|---|---|---|---|---|
| 1 | 100×100 | 109 ms | 43 ms | 3,6 MiB | 3,6 MiB |
| 2 | 100×100 | 111 ms | 53 ms | 0,2 MiB | 0,2 MiB |
| 3 | 100×100 | 109 ms | 38 ms | 0,03 MiB | 0,03 MiB |
| 4 | 500×500 | 1351 ms | 146 ms | 80,2 MiB | 83,7 MiB |
| 5 | 500×500 | 726 ms | 136 ms | 79,2 MiB | 79,2 MiB |
| 6 | 500×500 | 861 ms | 148 ms | 80,2 MiB | 83,2 MiB |

Względem wstępnych progów 100×100: przerwa GUI Preview ≤ 250 ms i przyrost WS Preview ≤ 30 MiB są spełnione we wszystkich trzech cyklach. Próg „Preview do gotowości” dla 100×100 był „do pomiaru”: zmierzone ok. 110 ms, wymaga zatwierdzenia wartości. Maksymalna przerwa timera w fazie odczekania po Cancel (nie w Preview) wyniosła 201 ms w cyklu 2 (100×100) i 172 ms w cyklu 5 (500×500), poniżej 250 ms. Pamięć po Cancel dla 100×100 ustabilizowana na 587,4–588,0 MB private. Cykle 500×500 potwierdzają wcześniejszy przebieg: Preview 0,73–1,35 s, przerwa GUI ≤ 148 ms.

To pojedyncza seria na jednym buildzie, nie oceniana statystycznie. Pamięć po 10 cyklach 100×100 nie była mierzona osobno.
