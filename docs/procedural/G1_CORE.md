# G1-A — rdzeń semantyczny

Status: implementacja pierwszego pakietu G1, bez integracji commitu i GUI. Cała bramka G1 pozostaje IN PROGRESS.

## Kontrakt

[`generation.h`](../../source/procedural/core/generation.h) i [`generation.cpp`](../../source/procedural/core/generation.cpp) tworzą osobną bibliotekę `rme_procedural_core`. Nie zależą od wxWidgets, Lua, item ID, brushy ani żywej mapy. Target `rme` zależy od tej biblioteki; aktualnie konsumentem API jest test C++, a edytor nie udostępnia jeszcze polecenia generacji.

- `GenerationConfig`: jawne X/Y/Z, szerokość/wysokość, seed i całkowite wagi grass/dirt/water. To pierwszy baseline niezależnych pól, bez noise, rzek i modelu biomów. Naturalność terenu nie jest kryterium tego pakietu.
- Przegląd przed adapterem: `MapExtent`, `CoreBudget` i `GenerationLayout` domyślnie zerują pola. Puste granice/budżety odmawiają pracy. `SemanticGrid::valid()` sprawdza nieujemny, niepusty region, dokładną liczbę komórek i znane wartości enum; adapter musi dodatkowo porównać region ze zleceniem oraz aktualne granice.
- `MapExtent` zawiera wyłączne granice przekazane przez przyszły adapter. Przy pobieraniu ich z RME trzeba uwzględnić włączne maksima `Position::isValid()` oraz rzeczywisty rozmiar mapy. Rdzeń nie przyjmuje mapy o domyślnym/hardkodowanym rozmiarze.
- Płaska siatka obejmuje jedno piętro, indeks `y * width + x`. Maska i ochrona to read-only `span<const uint8_t>` o dokładnie takim rozmiarze; wartości 0/1. Pusta maska oznacza cały prostokąt, pusta ochrona — brak chronionych pól. Całkowicie wykluczony obszar jest błędem. Niezmieniane pola otrzymują `Terrain::Preserve`; nie jest to polecenie usunięcia tile.
- `StableRandom` ma własny silnik `mt19937_64` z `seed_seq` wyprowadzonym z obu połówek seeda i identyfikatora strumienia. Nie korzysta z globalnego RNG brushy, `random_device`, `std::hash` ani `uniform_int_distribution`. Losowanie z zakresu używa rejection sampling. Strumień terenu v1 ma stałą domenę `0x7465727261696e`.
- Każde pole zużywa jeden roll terenu, także wykluczone przez maskę/ochronę. Dzięki temu zmiana samej maski nie przesuwa losowania pozostałych pól w tym samym prostokącie. Nie obiecuje się zgodności fragmentów po zmianie rozmiaru/pochodzenia prostokąta ani kontroli RNG istniejących brushy.
- `fingerprint()` jest niekryptograficznym FNV-1a po jawnych bajtach little-endian: wersja algorytmu, region i komórki. Nie hashuje paddingu C++, ustawień ani całego snapshotu mapy. Przyszłe provenance musi osobno obejmować seed/config, wejścia i assets. Preview/Commit/Redo będą wymagać także hasha konkretnego diffu.
- `validateGeneration` odrzuca ujemne współrzędne, Z poza zakresem, zerowe/wychodzące poza granice rozmiary, zerową sumę wag, niedopasowane/niebinarne maski, brak pól do zapisu, przekroczenie limitów i nieprzedstawialny rozmiar wektora. Sumy/mnożenia są poszerzone przed działaniem. Walidacja nie alokuje siatki.
- `CoreBudget` dotyczy liczby pól oraz rozmiaru samego wyjścia (`sizeof(SemanticGrid)` + komórki). Nie jest limitem pamięci całego zadania, allocator overhead ani pomiarem RSS. Snapshot, halo, maski, warianty, sandbox, konkretne tile i Undo muszą mieć oddzielne sprawdzone rozliczenie przed dopuszczeniem commitu G1.
- `stop_token`: odmowa przed alokacją, kontrola podczas generacji na granicy każdego wiersza i przed zwrotem. Anulowanie/`bad_alloc` zwraca błąd zamiast częściowej siatki. Podczas synchronicznego wywołania caller musi utrzymywać wejściowe spany przy życiu i nie mutować ich równolegle. Przed workerem adapter będzie musiał stworzyć własny spójny snapshot.
- `validateGeneration` przyjmuje ten sam token i sprawdza anulowanie także podczas walidacji maski/ochrony co 4096 pól. Fingerprinty i algorytm v1 pozostały niezmienione. Commit ma zastosować przechowywany, zweryfikowany konkretny diff; ponowne losowanie przy Commit/Redo nie jest kontraktem.

## Weryfikacja

[`test_procedural_core.cpp`](../../tests/test_procedural_core.cpp) wywołuje rzeczywisty kod tej biblioteki, bez modelu w Pythonie. Sprawdza powtarzalność i rozdzielenie RNG, przedziały (także duże), seed/config, czyste wagi, szeroką sumę wag, nieregularną maskę, ochronę, brak modyfikacji wejść, granice i odmowy limitów, anulowanie zgłoszone przed startem oraz fingerprint z niezależnie wyliczoną stałą.

Próby 100×100 i 500×500 porównują całe wektory, odmienny seed i obecność trzech terenów. Wypisany czas obejmuje tylko pojedynczą generację rdzenia. Nie jest to pomiar resolvera/commitu, pełnego szczytu RAM ani zaliczenie rozmiarów E2E G1.

Budowanie: kanoniczny `rtk proxy cmd /c build_ninja.bat`, następnie w środowisku MSVC x64:

```text
rtk proxy cmake --build build-ninja --target test_procedural_core --parallel 2
rtk proxy ctest --test-dir build-ninja -R "^test_procedural_core$" -V
```

Wyniki wykonanych prób znajdują się w bieżącym wpisie `../PROGRESS.md`. Test nie używa `assert`, więc warunki działają także w Release. Rozszerzenie `g1-core-faults-tests-20261102.log` sprawdziło pięć jednorazowych błędów alokacji małej generacji i deterministyczne ponowienia; nie obejmuje trwałego OOM ani późniejszych etapów adaptera. Nie sprawdzono anulowania w trakcie biegu, równoległej mutacji wejść ani zgodności fingerprintów między toolchainami/platformami. Nie uruchamiano nowego GUI RME, ponieważ pakiet nie dodaje przepływu dostępnego użytkownikowi; manualny test całego workflow nadal należy do G1-C.

## Dalsza praca G1

1. G1-B: snapshot maski/halo/ochrony i assets, kontrolowany resolver wykorzystujący istniejące DrawOperations/autoborder; pełne budżety i sprawdzenie zasięgu konkretnego diffu.
2. G1-C: odrzucanie stale preview, atomowy commit przygotowanej akcji, osobne Undo/Redo, Cancel i blokada live.
3. Minimalny dialog i rzeczywiste E2E 100×100/500×500 z pełnym round-trip, zachowaniem danych chronionych i pomiarami zasobów. Dopiero to może zamknąć G1.
