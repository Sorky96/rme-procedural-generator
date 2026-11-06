# G1: dokładna maska rzeczywistego zaznaczenia

Nowszy wycinek: [G1_VALIDATION.md](G1_VALIDATION.md) składa kontrolę parametrów/maski, klienta, snapshotu i integralności przygotowanego preview. Żywe membership sprawdza się nadal osobno; integracja z atomowym commit i pełna kontrola niezależnych mutacji assets pozostają otwarte.

Stan 2026-11-02: **PASS_G1_EXACT_SELECTION_MASK_ONLY; pełne G1 IN PROGRESS**. Wycinek uzupełnia [budżet przygotowania](G1_BUDGET.md) i [izolację selection](G1_SELECTION.md). Dotyczy pobierania oraz ponownego sprawdzania członkostwa w zaznaczeniu.

## Kontrakt

`captureGenerationSelection` w `source/procedural/adapter/generation_selection.{h,cpp}` odczytuje `Selection::getTiles()` wskazanego `Editor&`. Wynik `GenerationSelection` posiada minimalny prostokąt na jednym piętrze, dokładną maskę row-major, generację mapy, liczbę członków i estymatę pamięci. Otwory mają zero; pusty kafel w kolekcji selection ma jeden, również bez bitu selected. Same flagi przedmiotów lub kafla poza kolekcją nie rozszerzają maski. Nie używa cache `minPosition`/`maxPosition`, nie prostokątuje zaznaczenia i nie zmienia jego stanu.

Brak zaznaczenia oznacza `Empty`, nigdy cały prostokąt lub mapę. Wiele pięter oznacza `MultipleFloors`; nie wykonuje się cichej projekcji na Z. Każdy wpis musi wskazywać rzeczywisty kafel tej mapy pod jego współrzędnymi, w granicach nagłówka i `Position`. Odłączone kopie i obiekty innej mapy są odrzucane. Ścisły porządek `Position` (z, y, x), zgodny z istniejącą kolekcją `Selection`, wyklucza duplikaty i nieprawidłową kolejność bez dodatkowych kontenerów.

`SelectionMaskBudget` domyślnie ma zera i odmawia pracy. `maxCells` ogranicza cały prostokąt maski, także otwory, a `maxBytes` rozlicza `sizeof(GenerationSelection) + 32 + cells`. Pierwszy przebieg bez alokacji sprawdza tożsamość, granice i limity. Dopiero potem powstaje jeden własny wektor; `bad_alloc` daje `AllocationFailed` bez częściowego wyniku. To estymata własnego wejścia i allowance, nie RSS ani cały budżet przygotowania. Gdy wywołujący zachowuje tę maskę obok `PreparedTerrain`, musi uwzględnić ją oddzielnie: dotychczasowy kontrakt przygotowania wyłącza pożyczone wejścia.

`validateGenerationSelection` działa bez alokacji. Sprawdza generację mapy, geometrię, binarną maskę, liczbę jedynek, ścisłe unikalne bieżące członkostwo i dokładne minimalne granice. Zamiana wybranego pola na inne przy tych samych bounds i liczbie pól oznacza `StaleSelection`. Nieprawidłowy bieżący wpis oznacza `InvalidSelection`. Region z dopisanym zerowym marginesem również jest odrzucany.

Oba API przyjmują żywy `Editor&` i działają synchronicznie na głównym wątku, bez yield. Przed odczytem edytora odrzucają niewłaściwy wątek, a następnie stop oraz busy/deferred selection. Pętle sprawdzają stop co 4096 wpisów/bajtów i przed sukcesem. Nie wymagają aktywnego klienta i nie przełączają karty. Model jednego wątku GUI odpowiada [dokumentacji wxWidgets 3.3](https://docs.wxwidgets.org/3.3/overview_thread.html), sprawdzonej przez Context7.

## Granica aktualności

Walidator porównuje aktualny zestaw współrzędnych, nie historię zdarzeń ani dawne wskaźniki. Wymiana kafla przez historię może pozostawić prawidłową maskę; zmiana zaznaczenia i powrót do dokładnie tego samego zestawu również ją przywraca. Zmiana flag wyboru itemów przy tym samym członkostwie nie zmienia maski. Nie zapisuje mapRevision i nie przechowuje wskaźników.

Zawartość mapy, snapshot z halo, revision i assets wymagają odrębnej walidacji. Przykładowo utworzenie pustego kafla w otworze nie zmienia członkostwa maski, lecz unieważnia wcześniejszy snapshot, który przechwycił tam brak kafla. Sukces walidatora maski nie zezwala na commit i nie zastępuje kontroli źródła. Caller gwarantuje życie edytora i wskaźników w jego prawidłowej kolekcji selection; helper nie naprawia dangling pointers.

## Claude i review

Claude przygotował wyłącznie dwa nowe pliki w osobnym checkout. Run `b57245cc-6c6f-4cd5-9ce7-67bff4ec9fb1` i poprawka `fdf3a1aa-14bf-407c-a0b0-967a19c46e31`: completed/exit 0, pusty stderr, brak odmów. Codex odczytał report/stdout/stderr, przejrzał nowe pliki i pełny tracked diff; zastany tracked diff workera nie zmienił się. Worker nie uruchamiał builda/testów ani nie commitował.

Codex wykrył, że równość liczby i pozycji jedynek nie wystarcza do wykrycia dopisanego pustego marginesu. Claude dodał porównanie minimalnego prostokąta w istniejącym przebiegu walidacji; test regresyjny sprawdza ten przypadek. Usunięto nieużywany include. Zaakceptowane pliki skopiowano byte-for-byte; hashe są w `local-data/procedural-smoke/g1-exact-selection-integration-20261102.json`. Codex odpowiada za CMake, niezależne testy, integrację i raport.

## Niezależna weryfikacja

Kanoniczny Windows x64 `build_ninja.bat` exit 0: `local-data/procedural-smoke/g1-exact-selection-canonical-20261102.log`. Nowy normalny EXE SHA-256: `9b4c2334c00ea74ee2dc669e884fccc4f422306143a0b1b0c499a6fed7b3e44a`. Nie zmieniono produkcyjnego build scriptu ani dotychczasowego zachowania Selection/Editor/Map.

Pełny probe normalnego Application z testowym wxModule: `local-data/procedural-smoke/g1-exact-selection-rme-20261102`. **PASS, exit 0, normalny shutdown, pusty stderr, źródłowe assets niezmienione**. Dwie rzeczywiste prywatne karty edytora, syntetyczne kafle w pamięci, bez otwierania plików map. Walidator `g1_verify_preview_probe.py --exact-selection` potwierdził 417 aktualnych obiektów RME oraz 877 źródeł bazy G0 z dziesięcioma oczekiwanymi różnicami G1. Dowody `verification.json`, `current-verification.json`, manifest builda i pełny log w tym katalogu. Probe EXE SHA-256: `e22990baf715990208883b9b65103c7631650ac9048c61a90b453a20552f8c55`.

| Prostokąt maski | Wybrane pola | Estymata własnego wejścia (B) | Fingerprint przygotowanego preview |
|---|---:|---:|---|
| 100×100 | 9 999 | 10 104 | 12307128149775414366 |
| 500×500 | 249 999 | 250 104 | 6528612615874077346 |

Każdy prostokąt ma jeden otwór. Wszystkie wybrane kafle są puste i nie mają flagi selected. 100×100 uruchomiono dla nieaktywnego edytora, 500×500 dla aktywnego. W obu przypadkach capture wykonał dokładnie jedną obserwowaną alokację, a rewalidacja zero. Dokładny limit bajtów przechodzi. Testy sprawdziły każdą maskę i przekazały ją do rzeczywistego `prepareTerrain`: dokładnie tyle zmian, brak planu w otworze, każdy diff wewnątrz maski, źródłowe kafle nadal puste i snapshot nadal zgodny.

Zamiana członkostwa przy tych samych bounds i liczbie pól jest odrzucana; odtworzenie dokładnego zestawu ponownie przechodzi. Utworzenie pustego kafla w otworze pozostawia maskę aktualną, ale odrzuca wcześniejszy snapshot. Po zmianie/wyczyszczeniu żywego zaznaczenia przechwycony wynik pozostaje własny i niezmieniony. Aktywna karta i globalny RNG nie zmieniają się.

Przeszły: domyślny limit komórek, limit bajtów mniejszy od wymaganego, jedyny punkt błędu alokacji maski, pre-cancel obu API, odmowa obu API z workera, odmowa w deferred/internal session, niebinarny/krótki mask buffer, błędny count, ujemna i skrajna geometria, padding, ujemny i poza-nagłówkowy żywy wpis, odłączona kopia oraz wpis innej mapy pod tymi samymi współrzędnymi, wiele pięter i pusta kolekcja. Zmiana flag itemu przy stałym membership oraz kontrolowana wymiana obiektu kafla z prawidłową aktualizacją kolekcji nie unieważniają samej maski. Nie jest to test commitu generatora ani jego Undo.

Powtórzono pełne wcześniejsze kontrole sandboxa 100×100/500×500, 79 jego faultów, 106 faultów wspólnego przygotowania i dokładne granice budżetu, snapshot/protection, selected context i selected-border refusal, cache/zdarzenia dwóch edytorów oraz zwykłe zaznaczenie/setter Lua z Undo/Redo. Estymaty i obserwowane piki zwykłego new przygotowania pozostały zgodne z poprzednim wycinkiem; brak tezy o przyspieszeniu lub gwarancji RSS. Starsze próby z błędami zachowano jako historyczne. `git diff --check` przeszedł.

Nie wykonano wymuszonej korupcji wewnętrznej kolekcji null/duplicate/order, trwałego OOM, anulowania w środku przebiegu samej maski ani pomiaru RSS. Kod odmawia malformed wpisów przez jawne kontrole; wskaźniki mają pozostać żywe według kontraktu Selection.

## Otwarte wymagania

Helper nie wprowadza dialogu wyboru zakresu, renderowanego preview ani połączenia walidacji z przyciskiem Commit. Nie rozwiązuje publikowania membership pustych kafli po przyszłym commit/Undo/Redo. Pozostają atomowy commit dokładnego diffu, iniekcja jego awarii, osobna historia ręcznego rysowania, pełna kontrola staleness przy commit, GUI Cancel/harmonogram, pamięć Undo/RSS/innych wyników oraz GUI/OTBM round-trip 100×100 i 500×500. Nie rozpoczynano G2, nie używano forgotten.otbm/companionów, map użytkownika, Dockera, bazy ani świata. Wspólne niezacommitowane zmiany zachowano; bez commit/push.
