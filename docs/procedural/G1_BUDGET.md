# G1: wspólny budżet przygotowania terenu

Nowszy wycinek: [G1_MASK.md](G1_MASK.md) weryfikuje pobranie i rewalidację dokładnej maski rzeczywistego Selection. Integracja z dialogiem oraz kontrolą przy commit nadal pozostaje otwarta; poniższe wyniki budżetu zachowano historycznie i powtórzono w nowym pełnym probe.

Stan 2026-11-02: **PASS_G1_JOINT_PREPARATION_BUDGET_ONLY; pełne G1 IN PROGRESS**. Ten wycinek uzupełnia [izolację zaznaczenia](G1_SELECTION.md) i [sandbox](G1_PREVIEW.md) o jeden limit przygotowania. Nie wprowadza zatwierdzania zmian ani interfejsu generatora.

## Kontrakt

`prepareTerrain` w `source/procedural/adapter/terrain_preparation.{h,cpp}` łączy capture snapshotu, semantyczny rdzeń, resolver i `DrawOperations::prepareTerrainPreview`. Zwraca własny, przenoszalny `PreparedTerrain`: snapshot, konkretny plan ID, sandbox/diff i raport `PreparationMemory`. Semantyczna siatka jest niszczona przed budową sandboxa. Wynik nie pożycza maski ani profilu wywołującego.

`PreparationBudget` określa `maxCells`, `maxContextCells`, `maxChanges` i `maxBytes`. Domyślne zera odmawiają wykonania. Wspólne przygotowanie działa wyłącznie na wątku GUI, bez yield i bez mutacji źródłowej mapy, globalnego RNG, brushes lub zaznaczenia. Obsługuje `stop_token`, w tym odmowę przed rozpoczęciem oraz anulowanie podczas pracy; nie jest to jeszcze obsługa przycisku Cancel ani harmonogram responsywnego GUI.

Jeden licznik pozostałych bajtów maleje po sprawdzonych rezerwacjach. Najpierw rezerwowane są infrastruktura (`sizeof(PreparedTerrain)` i 64 KiB allowance), rdzeń i plan, następnie snapshot, a sandbox otrzymuje tylko pozostały limit. Raport sumuje `infrastructureBytes`, `snapshotBytes`, `semanticBytes`, `planBytes` i `previewBytes`. Zwolnionych etapów nie kredytuje ponownie: jest to konserwatywna suma kosztów etapów, a nie pomiar chwilowej pamięci żywych obiektów.

Preflight sprawdza geometrię i przepełnienia, halo dwóch kafli zgodne z capture, granice współrzędnych oraz limity komórek. Po snapshot sprawdza pełne rozmiary masek i liczbę faktycznie zapisywalnych pól przed generacją/resolverem. Rzadka maska nie zużywa limitu zmian całym prostokątem. Nazwy trzech brushes i identyfikator klienta mają długość 1–256 bajtów. Plan uwzględnia wektor ID, trzy ciągi profilu, zachowany identyfikator klienta i jego lokalną kopię resolvera wraz z narzutami. Nowe `VersionManager::GetCurrentVersionIDView()` pozwala sprawdzić długość bez wcześniejszej alokacji; widok traci ważność przy zmianie/unload klienta, dotychczasowy getter pozostaje bez zmian.

Błędy etapów są zachowane w wariancie `PreparationError`. Błędy wejścia i zewnętrzne `bad_alloc` mają osobne `PreparationInputError`. Odmowa nie publikuje częściowego wyniku. Finalny test stop poprzedza zwrot właścicieli.

## Granice budżetu

64 KiB i narzuty alokacji są jawnymi konserwatywnymi założeniami estymatora, a nie zmierzoną gwarancją stosu lub systemowego RAM. Budżet nie obejmuje źródłowej mapy, pożyczonych wejść, już załadowanych assets, całej aplikacji/UI/renderera, alokacji runtime, narzutu alokatora poza przyjętymi założeniami, innych równocześnie przechowywanych wyników ani przyszłego Undo. Nie ma tu twardego ograniczenia heap/RSS całego procesu.

Testowy licznik przechwytuje żądane bajty zwykłego `new/new[]` na wątku wywołania. Używa stałej tablicy bez własnych alokacji podczas pomiaru; nie zmienia układu wskaźników malloc przekazywanych między RME a DLL wxWidgets. Nie obejmuje kompletnie aligned-new, alokacji C/DLL/GPU ani RSS. Rozgrzewka używa istniejącego profilu i niezależnej ścieżki etapów. Tablica pomiarowa fixture, źródło i assets pozostają poza raportem zadania. Zerowy wynik po destrukcji dotyczy wyłącznie śledzonych żądań, nie dowodzi braku wszystkich wycieków procesu.

## Implementacja i niezależny przegląd

Claude Code wykonał dwa pliki adaptera w osobnym checkout przez lokalny bridge. Uruchomienia `231dc92b-7e46-4d8c-844b-725c2d5fd266` oraz poprawka `817abf8b-36b6-46f5-8d75-a9c290090c11` zakończyły się kodem 0, bez odmów narzędzi i z pustym stderr. Przejrzano report, stdout, stderr, pełne nowe pliki i diff; istniejący tracked diff workera nie zmienił się między tymi uruchomieniami. Claude nie wykonywał builda ani testów.

Codex sprawdził implementację i zlecił poprawki: użycie bezalokacyjnego widoku zamiast kopiującego gettera klienta, liczenie trzech ciągów profilu zamiast dziewięciu oraz jawne sprawdzenie zgodności rozmiarów masek zamiast cichego minimum. Codex dodał getter, integrację CMake, testy i weryfikator oraz skopiował zaakceptowane pliki byte-for-byte. Hashe integracji są w `local-data/procedural-smoke/g1-preparation-integration-20261102.json` i są sprawdzane przez verifier. Nie wykonano commit ani push; wspólne niezacommitowane zmiany zachowano.

## Wyniki

Kanoniczny Windows x64 `build_ninja.bat`: exit 0, 181 kroków, log `local-data/procedural-smoke/g1-preparation-canonical-20261102.log`. Bieżący normalny EXE SHA-256: `a8ce2a3d23f8dfd41abc9a5d0963729b801de8d7069a349fc0c1fedfa56dd439`.

Pełna próba normalnego Application przez testowy wxModule: `local-data/procedural-smoke/g1-preparation-visible-rme-20261102`. Exit 0, normalny shutdown, pusty stderr, źródłowe assets bez zmian. Użyto prywatnych assets referencyjnych i dwóch syntetycznych kart w pamięci; nie otwierano plików map użytkownika. Verifier `g1_verify_preview_probe.py --preparation` potwierdził 416 aktualnych obiektów RME, 877 źródeł bazy G0 i dziesięć oczekiwanych różnic G1. Dowody: `verification.json` i `current-verification.json` w tym katalogu. Probe EXE SHA-256: `72c9b31c5e213836df106dbcb4c1385d6a6ab44097b0e1f5027d8918b281db6b`.

| Rozmiar | Wspólna estymata (B) | Szczyt śledzonych żądań new (B) | Śledzone bajty po destrukcji | Fingerprint |
|---|---:|---:|---:|---|
| 100×100 | 7 234 198 | 2 923 649 | 0 | 18297947229083653543 |
| 500×500 | 168 808 358 | 69 507 073 | 0 | 5645408562528222048 |

Obie wielkości porównano z osobno złożoną ścieżką core → resolver → sandbox: konkretne ID i fingerprint są identyczne. Dokładny wspólny limit przechodzi; limit o jeden bajt mniejszy odmawia jako `PreviewError::Limit`. Limit wystarczający dla każdego pojedynczego etapu, lecz niewystarczający dla ich wspólnego przygotowania, również odmawia. Sprawdzono sumę składników, własność po przeniesieniu, odrzucenie moved-from preview oraz niezależność od późniejszej zmiany maski/profilu wywołującego.

Przeszło **106 przypadków jednorazowych błędów alokacji** w przygotowaniu: kontrolowane `AllocationFailed` albo pełny deterministyczny wynik, bez częściowego wyniku i zmian źródła/cache; ponowna próba pozostaje deterministyczna. Przeszły też zera i krótkie limity cells/context/changes, limit snapshotu zachowujący jego typ błędu, rzadka maska, przepełniona/ujemna geometria, brak zapisywalnych pól, oversized profil odrzucony przed alokacją, brakujący brush, niewłaściwy wątek, brak klienta, pre-cancel i stop podczas 500×500. Chronione UID przy granicy water/grass propaguje `OutOfMask` bez zmiany mapy.

Powtórzono cały istniejący probe: cztery warianty sandboxa 100×100/500×500, 79 jego faultów alokacji, pełny diff i ochrony, base-ground, maski/temple, epoch assets, cache i zdarzenia dwóch edytorów, selected-context i selected-border refusal oraz zwykły setter Lua i zaznaczenie z Undo/Redo. Ta regresja zwykłych akcji nie zalicza przyszłego commitu generatora. Starsze programy testowe snapshotu/kolejności borderów pozostają historycznymi dowodami przy niezmienionych właściwych źródłach.

Pierwszy przebieg `g1-preparation-budget-rme-20261102` zachowano: mimo poprawnych markerów i kodu 0 zawiera błąd wxWidgets `SetFocus` (0x00000057) w stderr i **nie jest czystym zaliczeniem**. Pierwotny runner wypisywał `stderr_empty=false`, lecz nie uwzględniał go w `passed`; teraz pusty stderr jest warunkiem sukcesu. Dodano testową opcję `--visible` i powtórzono pełną próbę z widocznym prywatnym oknem; nowy przebieg jest czysty. Nie udowodniono przyczyny błędu ani tego, że widoczność naprawia go ogólnie. Nie zmieniono kodu produkcyjnego UI ani wxWidgets i nie filtrowano stderr.

## Pozostałe wymagania G1

Otwarte pozostają: rzeczywisty RSS/cała aplikacja i współistnienie wyników, pamięć przyszłego Undo, GUI Cancel i harmonogram, dokładna maska GUI oraz walidacja staleness assets/mapy/maski/selection przed commit. Przyszły commit musi uwzględnić membership pustych kafli w kolekcji selection, również bez bitu selected. Brakuje atomowego zatwierdzenia dokładnego diffu z awariami/Undo/Redo i niezależną historią ręcznego rysowania, minimalnego dialogu/renderowanego preview, GUI/OTBM round-trip 100×100 i 500×500 oraz potwierdzenia kolejności równych z na innych platformach.

Nie rozpoczynano G2. Nie używano forgotten.otbm ani companionów i nie ingerowano w Docker, bazę lub świat użytkownika.
