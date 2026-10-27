# Generator proceduralny — audyt architektury G0

Stan: audyt źródeł 2026-10-26, uzupełniony wynikami runtime 2026-10-27. Ten dokument opisuje istniejące punkty integracji i decyzje do sprawdzenia przed G1. Zakres wykonanych testów podaje `TEST_PLAN.md`; G0 niezaliczone.

## Baza i środowisko

- Checkout: `C:\repo\remeres-map-editor-redux-master`.
- Bazowy commit lokalny: `5a01a8fe1c34465559701d50681cc2210ef32c67` (`Initial snapshot of RME Redux`). To początek lokalnej historii, nie potwierdzony SHA upstream.
- Gałąź audytu: `docs/procedural-phase0-audit`, utworzona z czystego `master`.
- `LICENSE` zawiera GPL v3; nagłówki źródeł wskazują GPL v3 lub późniejszą. Licencje danych wejściowych i zależności wymagają osobnego rozpatrzenia.
- CodeGraph: indeks aktualny, 3328 plików, 21857 węzłów, 56553 krawędzi. Użyto MCP `codegraph_explore` i CLI `status`; zależności grafu są wskazówką, wnioski poniżej opierają się na zwróconym kodzie.
- CMake wymaga 3.28+, C++23, wxWidgets 3.3. Lokalny CMake: 4.3.0-rc3. Build wykrył Visual Studio `18\Community`, MSVC `14.51.36231`, triplet `x64-windows`.
- Ninja znaleziono w `C:\repo\vcpkg\downloads\tools\ninja-1.13.2-windows`; nie był w odziedziczonym `PATH`. Ustawienia `PATH` i `VCPKG_ROOT=C:\repo\vcpkg` dodano tylko w procesie buildu.
- Baseline vcpkg: `1940ee77e81573713c0d364c42f5990172198be1`. Zainstalowane m.in. wxWidgets 3.3.1#1, Lua 5.5.0#1, Sol2 3.5.0#1, Boost 1.90.0#1, protobuf 6.33.4#1. Build zakończony kodem 0 po wznowieniu z `--parallel 2`; dokładny przebieg i ograniczenia: [TEST_PLAN.md](TEST_PLAN.md).

## Mapa istniejących mechanizmów

Lokalizacje dotyczą bazowego SHA; numery linii mogą zmienić się po implementacji.

| Mechanizm | Kod | Konsekwencja |
|---|---|---|
| Rysowanie | `source/editor/operations/draw_operations.cpp`: `DrawOperations::draw` (301, 432, 497), `drawGroundOrEraserImpl` (134) | Wybiera brush z GUI, kopiuje tile, tworzy `Change` i wykonuje akcje. Nie wywoływać w workerze ani nie używać wielokrotnie jako substytutu jednej transakcji generatora. |
| Undo/Redo | `source/editor/action_queue.cpp`: `addBatch` (86), `addAction` (142), `undo` (152), `redo` (162) | Commit następuje przed dopisaniem historii; grupowanie zależy od typu, ustawień i czasu. Trzeba przetestować granicę między generacją a ręcznymi edycjami w obu kierunkach. |
| Części batcha | `source/editor/action.cpp`: `BatchAction::addAndCommitAction` (697), `commit` (713), `undo` (721) | `addAndCommitAction` od razu mutuje mapę, a dopiero potem dodaje akcję do wektora. Brak dowodu silnej gwarancji wyjątków. Nie utożsamiać batcha z transakcją odporną na awarię. |
| Istniejący sandbox preview | `source/brushes/managers/autoborder_preview_manager.cpp`: `CopyMapArea` (68), `SimulateBrush` (95), `ApplyBorders` (137), `PruneUnchanged` (181) | Posiada własny `BaseMap`, kopiuje tile do docelowych `TileLocation` i borderuje bufor. Jest związany z kursorem, globalnym brushem i footprintem GUI; ocenić wydzielenie potrzebnej części zamiast budować drugi silnik borderów. |
| Wspólne bordery | `source/map/tile_operations.cpp`: `TileOperations::borderize` (97) | Deleguje do `GroundBrush::doBorders`. Reguły pozostają w istniejących brushach, adapter dostarcza mapę roboczą i kontroluje zasięg zmian. |
| Tile i ochrona | `source/map/tile.h`: `Tile` (63), `empty` (122), `getZoneIds` (166), `getMapFlags` (253) | Tile ma ground, stos itemów, creature, spawn, house ID, flags, extra i lokalizację z dodatkowymi relacjami. `empty()` sprawdza `size()`, więc nie jest pełnym dowodem braku chronionych metadanych. |
| UID/AID | `source/game/item.h`: `getUniqueID` (519), `getActionID` (527) | Ochrona musi analizować również kontenery i powiązania; flaga tile ani ground nie zastępują inspekcji całej zawartości. |
| Tożsamość mapy | `source/map/map.cpp`: `Map` (39), `initializeEmpty` (55), `open` (84); `Map::getGeneration` w `map.h` | Generation zmienia się przy tworzeniu/ponownym inicjalizowaniu/otwieraniu, nie stanowi numeru każdej edycji. Dla stale preview potrzebna kontrola snapshotu i metadanych. |
| Granice | `source/map/position.h`: `Position::isValid` (161) | Korzysta z limitów projektu, odrzuca wartości ujemne przez konwersję unsigned. Sprawdzać przed alokacją i przed dodaniem halo/footprintów; nie kopiować szerszych zakresów z niektórych bindingów Lua. |
| Klient i assets | `source/app/managers/version_manager.h`: `IsVersionLoaded` (27), `getLoadedVersion` (31), `lastLoadHasOtb` (41) | Zestaw danych musi być jawnie zgodny. Same katalogi `data/<version>` i plik OTB nie dowodzą kompletnego klienta. |
| Losowość | `source/util/common.cpp`: `getRandomGenerator`, `uniform_random`, `random`; `GroundBrush::draw` w `source/brushes/ground/ground_brush.cpp` | Generator losowy startuje z `random_device`; noise seed nie kontroluje wariantów ground/doodad. Przed G1 potrzebny kontrolowany kontekst RNG adaptera bez zmiany losowości zwykłego rysowania. |
| Jawny wybór wariantów doodad | `source/brushes/doodad/doodad_brush.h`: `getRandomSingleItem(variation, roll)` (88), `getComposite(variation, roll)` (91); `source/brushes/doodad/doodad_brush_items.cpp`: odpowiednie implementacje (193, 120) | Już istnieją przeciążenia przyjmujące wynik losowania. Korzystać z nich przy planowaniu deterministycznego adaptera; nie dodawać drugiego mechanizmu wyboru ani nie zakładać konieczności zmiany całego RNG brushy. Sprawdzić również wszystkie wywołania borderowania. |
| Lua | `source/lua/lua_api.cpp`: `registerAll`; `source/lua/lua_api_app.cpp`: `LuaTransaction` (95), `transaction` (381) | Istnieją transakcje i API proceduralne. Lua nie daje z samego faktu istnienia bindingów kontraktu snapshotu, deterministyczności ani pracy w tle. |
| Mutacje Lua | `source/lua/lua_api_tile.cpp`: `registerTile` (349), eksporty `applyBrush`, `borderize`, `ground` | Modyfikacje korzystają ze śledzenia Undo, ale bezpośrednio dotyczą mapy. To kandydaci do smoke testu istniejącej integracji, nie docelowa ścieżka commitowania z handlera generatora. |
| Zapis użytkownika | `source/editor/persistence/editor_persistence.cpp`: `EditorPersistence::saveMap` (54) | Koordynuje backupy, pliki pomocnicze, marker odzyskiwania i rollback zapisu. Generator powinien korzystać z normalnego workflow zapisu edytora. |
| Format OTBM | `source/io/iomap_otbm.cpp`: `IOMapOTBM::saveMapToDisk` (669), `saveMap` (813) | Niskopoziomowy zapis obejmuje mapę i dane pomocnicze. Nie zastępuje koordynatora backupów. Sprawdzić wybrany format i odbiorcę, bez zmian serializacji. |
| Live | `source/live/live_action.cpp`: `NetworkedBatchAction::addAndCommitAction` (47), `commit` (69) | Częściowe akcje mogą być rozgłaszane. MVP nie wspiera Commit w live. |

## Granica C++ i Lua

Zestaw klienta wskazany w tej sesji: `C:\repo\ProjectSorky\otclient\data\things\1098`. Parsowanie DAT/SPR jest poprawne, ale testy referencyjne wykazały rozbieżne mapowanie względem standardowego `data/1098/items.otb`. Do smoke testu należy zweryfikować istniejący profil `1098-BlackTek` (`dat_only`), a nie zakładać zgodności standardowego profilu `10.98` na podstawie podpisów. `ItemDefinitionResolver::resolveDatOnly` (`source/item_definitions/core/item_definition_resolver.cpp:249`) zaczyna od identycznych server/client ID i stosuje mapowania XML; `VersionManager::LoadCanonicalAssets` wybiera ten tryb na podstawie profilu. Szczegóły i hashe: [TEST_PLAN.md](TEST_PLAN.md).

Rekomendacja do zweryfikowania testem: rdzeń generatora, snapshot, resolver i commit w C++; Lua opcjonalnie do presetów, konfiguracji i demonstracji. Nie uruchamiać istniejącego stanu Lua ani managerów GUI w workerze. Istniejące `noise`, `algo`, `geo` ocenić pod kątem wydzielenia potrzebnych algorytmów dopiero w ich pierwszym rzeczywistym użyciu.

`scripts/terrain_generator_demo.lua` jest materiałem orientacyjnym: ma sztywne ID, korzysta z bezpośrednich mutacji tile i osobnej losowości dekoracji. Nie rozszerzać go do produkcyjnego generatora przed testami bezpieczeństwa.

Potwierdzone ryzyko na poziomie biblioteki: `transaction` wywołuje `sol::function` i nie sprawdza wyniku callbacka. `source/lua/lua_sol_config.h` włącza `SOL_ALL_SAFETIES_ON`. Context7 wskazuje, że błędy wywołań chronionych należy rozpoznawać przez wynik `valid()`: [dokumentacja Sol2](https://github.com/ThePhD/sol2/blob/develop/documentation/source/api/protected_function.md). Lokalny probe skompilowany MSVC z nagłówkiem konfiguracyjnym projektu i zainstalowanymi Sol2 3.5.0#1 / Lua 5.5.0#1 potwierdził alias `sol::function = sol::protected_function`: callback wykonał inkrementację i `error()`, a `func();` wróciło bez wyjątku; jawne sprawdzenie drugiego wywołania wykazało nieważny wynik. Wynik: `returned_without_throw=true`, `counter_after_ignored_result=1`, `checked_call_failed=true`, kod 0. Źródło i polecenie probe: [TEST_PLAN.md](TEST_PLAN.md).

Potwierdzenie na prawdziwej mapie RME 2026-10-27: `g0_error_probe.lua` wykonał zmianę jednego tile i `error()` w callbacku. Zewnętrzne `pcall` otrzymało sukces, tile pozostał, liczba pól wzrosła z 400 do 401. Końcowy assert diagnostyki zgłosił FAIL rollbacku. Jedno ręczne Undo cofnęło zmianę i ponowny verifier potwierdził oryginalne 400 pól. Zatem transakcja z błędem trafia do historii i nie daje wymaganego rollbacku. Naprawę obsługi wyniku zaplanować jako osobny mały PR integracyjny G0-B z testem, zamiast traktować obecny wrapper jako gotową granicę bezpieczeństwa generatora. Skrypty i dokładny zakres: [TEST_PLAN.md](TEST_PLAN.md).

## Projektowany przepływ G1

Aktualizacja G0-B (2026-10-27): wrapper przyjmuje jawnie `sol::protected_function`, sprawdza `protected_function_result::valid()` i dla błędu rzuca `std::runtime_error` z treścią Sol2. Istniejąca obsługa wyjątku wywołuje rollback i przekazuje błąd do Lua. Nie zmieniono implementacji snapshotów, commitowania ani rollbacku. Regresja w `lua_tests/test_transaction_rollback.lua` przeszła na nowym EXE: trzy błędy i sześć mutacji wycofane, komunikaty zachowane, historia bez nowych wpisów po błędach; dwie udane transakcje rozdzielone w Undo/Redo. To zamyka konkretny błąd ignorowanego callbacka, nie pozostałe ryzyka atomowości i metadanych z audytu.

1. Główny wątek: przypisanie konkretnego edytora, assets, maski i halo; kontrola budżetu i spójne pobranie danych.
2. Worker: własne płaskie dane semantyczne i kontrolowane strumienie RNG; zero odwołań do GUI i żywych tile.
3. Przygotowanie konkretnego diffu na izolowanej mapie: ponowne użycie reguł brushy i borderów. Na początku na głównym wątku, dopóki nie ma dowodu bezpieczeństwa assets/brushy w workerze. Czas i pamięć przygotowania muszą być mierzone.
4. Walidacja konkretnego wyniku: ID, footprinty, ochrona, granice, przechodniość. Preview zachowuje przygotowane warianty, nie losuje ich ponownie przy Commit.
5. Główny wątek: ponowne sprawdzenie snapshotu, tożsamości i assets, a następnie zastosowanie przez rozszerzony w minimalnym zakresie kontrakt `DrawOperations` + istniejące `Action`/`Change`/`ActionQueue`.
6. Undo/Redo odtwarza zapisane stany. Zapis przez istniejący workflow edytora. Brak ingerencji w format OTBM, układ `Tile` ani vendored kod.

Ten przepływ jest projektem, nie listą istniejących API. Przed wyborem sygnatur wymagane są testy poniższych decyzji.

## Decyzje do zamknięcia przed G1

- Jak `DrawOperations` przygotuje diff bez globalnego brusha i bez częściowych mutacji, wykorzystując istniejący sandbox preview?
- Jak zapewnić kontrolowany RNG dla ground, doodad i borderów bez globalnego reseedowania?
- Jak rozpoznać zmianę maski/halo i metadanych po Preview, w tym Undo/Redo oraz zamknięcie mapy?
- Jak zachować silną gwarancję błędu przy zastosowaniu batcha i nie scalać go z ręcznymi akcjami?
- Jaki limit komórek i pamięci obejmuje pełny koszt snapshotu, diffu i Undo dla 100x100 i 500x500?

## Plan małych PR

| PR | Zakres | Dowód wymagany przed zakończeniem |
|---|---|---|
| G0-A | Ten audyt, protokół testów i identyfikacja środowiska | Kontrola dokumentów i jawny wynik próby buildu; bez deklaracji pełnego G0 |
| G0-B | Reprodukowany smoke test na zgodnych assets i mapie syntetycznej | Build, 20x20, osobne Undo/Redo, rollback błędu Lua, zapis i ponowny odczyt kompletu plików |
| G1-A | Minimalny rdzeń semantyczny i walidacja konfiguracji | Determinizm, maska, granice i budżety; bez map mutation |
| G1-B | Przygotowanie diffu przez wspólne reguły rysowania i kontrolowany RNG | Sandbox, ID, footprinty, halo, brak mutacji oryginału i równość konkretnego diffu |
| G1-C | Commit istniejącą historią i minimalne GUI | Nieaktualne preview, błąd zastosowania, Undo/Redo i round-trip 100x100 / 500x500 |

Nie przechodzić do G1 przed zamknięciem G0-B. Nie tworzyć pustych modułów na zapas. Przy współpracy z Claude przypisywać osobne PR i worktree; nie prowadzić równoległych edycji tych samych plików oraz nie używać wspólnego katalogu buildu.
