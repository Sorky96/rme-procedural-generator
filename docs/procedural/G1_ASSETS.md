# G1: fingerprint bieżącego katalogu assets generacji

Nowszy wycinek: [G1_TARGET.md](G1_TARGET.md) składa kontrolę bieżącego żywego celu offline z walidacją wyniku i opcjonalnie rzeczywistego membership. Atomowy commit, reentrancy/historia i GUI nadal pozostają otwarte. Poniższe wyniki katalogu powtórzono w nowym pełnym probe.

Wycinek 2026-11-03 uzupełnia [walidację przygotowanego wyniku](G1_VALIDATION.md). Dotyczy zmian załadowanych definicji, brushy ground i metadanych używanych przez generację, także bez zmiany epoki VersionManager. Pełne G1 pozostaje **IN PROGRESS**.

## Decyzja i zakres

Same liczniki setterów nie obejmują zapisów przez zachowane `mutableEditorData` ani publiczne obiekty reguł. `fingerprintGenerationAssets` w `source/procedural/adapter/generation_assets.{h,cpp}` za każdym wywołaniem odczytuje bieżące wartości. Pracuje tylko na głównym wątku, bez alokacji, yield, zmiany katalogów/RNG/cache, renderowania, dekodowania lub uploadu GL. Dane robocze mają stały rozmiar. Stop jest sprawdzany na wejściu, co najwyżej co 4096 jednostek w nieograniczonych pętlach i przed sukcesem.

Fingerprint obejmuje:

- epokę i ID klienta oraz wersje, kolejność/ID i maksymalny ID store;
- widoczne przez `ItemDefinitionView` identity/group/type, pełną maskę flag, wszystkie obecne 16 `ItemAttributeKey`, teksty/JSON oraz powiązanie editor brush;
- istniejący `GameSprite` każdego client ID: obecność/tożsamość, publiczne wymiary/patterny/frames, minimapę, light, draw height/offset i `sprite_ids`;
- cały rejestr brushy: klucze, tożsamości, ID i typ ground; dla każdego GroundBrush także nazwę, friends/hate, look/Z, flagi borderów, optional, randomize, szanse/warianty i uporządkowane reguły/specific cases;
- globalne i własne AutoBorder z ID/group/ground/13 tile IDs oraz globalne `GroundBrush::border_types`.

Waga jest obserwowana przez istniejące API atrybutów z jego przeliczeniem do int64; nie hashuje się ukrytej reprezentacji float. Wewnętrzne tablice store mają spełniać kontrakt równych rozmiarów utrzymywany przez append. Nie jest to sanitizer dowolnie uszkodzonych tabel. Gdy `allIds()` zawiera duplikaty, hash obejmuje ich kolejność i wartości rozwiązywane przez aktualne `get(id)`, tak jak ścieżka generacji; nie ma dostępu do przesłoniętych wierszy przez indeks.

## Własność i tożsamość

Zapamiętanych surowych linków `ItemEditorData::brush` nie dereferencjonuje się. Są tokenami tożsamości. Niepusty link AutoBorder musi wskazywać własny obiekt `unique_ptr` lub obiekt w aktualnym globalnym rejestrze; potwierdzenie odbywa się przez porównanie wskaźników. Nieznany link, pusty block/case lub nieprawidłowy typ istniejącego sprite oznacza `InvalidAssets`. Własne obiekty i obiekty rejestrów są odczytywane wyłącznie przez ich prawidłowych właścicieli.

Jedyna zmiana w klasie GroundBrush to wąskie zaprzyjaźnienie czytnika; nie dodano mutatorów ani publicznych widoków prywatnych danych. Nie zmieniono układu klasy, losowania, draw lub borderowania.

Fingerprint jest konserwatywny i **lokalny dla procesu**: zawiera tokeny wskaźników i kolejność unordered registry. Nie zapisuje się go do mapy, nie służy jako trwały klucz i nie włącza do deterministycznego fingerprintu wyniku. Niepowiązana z bieżącym regionem zmiana objętej części katalogu także unieważnia preview; przywrócenie identycznych wartości i tożsamości może przywrócić zgodność. To kontrola spójności 64-bit, nie kryptograficzna ochrona przed fałszowaniem pamięci.

## Integracja

DrawOperations sprawdza i zapamiętuje fingerprint przed budową sandboxa, odmawiając malformed katalogu przed użyciem raw border links. `TerrainPreview::matchesAssets` przelicza go na żądanie. `validatePreparedTerrain` po sprawdzeniu klienta/epoki, a przed kontrolami source/preview mogącymi korzystać z cached links, wymaga zgodności bieżącego katalogu. Niezgodność lub `InvalidAssets` daje `StaleAssets`, stop daje `Cancelled`.

Skan nie jest częścią render hot path ani pętli każdego kafla. Nowy skalar jest objęty istniejącym `sizeof` estymatora: wspólne przygotowanie rośnie o kolejne 16 B bez nowej alokacji per tile. Powiązanie źródła i dotychczasowy konkretny fingerprint wyniku pozostają oddzielne.

## Co pozostaje poza kontrolą

Nie obejmuje raw pikseli, obrazów, tekstur, atlasu/GPU, runtime animacji, geometry caches ani wnętrza brushy innych niż ground. Nie hashuje doodad/collection/RAW links definicji, których ścieżka G1 terrain nie używa. Odczytuje załadowane dane; nie monitoruje zmian plików na dysku przed ich załadowaniem. Prawidłowa własność samych rejestrów/kontenerów pozostaje wymaganiem C++.

Pełne G1 nadal wymaga końcowego guard życia/właściwego edytora i trybu offline, atomowego commitu/awarii/Undo/Redo i membership pustych kafli, niezależnej historii ręcznego rysowania, GUI dialogu/renderowanego preview/Cancel/harmonogramu, budżetu Undo/RSS/innych wyników i GUI/OTBM round-trip 100×100/500×500. Ten wycinek nie zalicza tych wymagań i nie rozpoczyna G2.

## Claude i review

Claude wykonał dwa nowe pliki przez bridge, run `14ea4969-9641-4259-add4-6a0b0e291666`: completed/exit 0, brak odmów, pusty stderr, report/stdout zgodne. Tracked diff workera pozostał identyczny z poprzednim run. Codex przejrzał pełne nowe pliki, raport i diff, w tym polling, pointer ownership i zakres każdego pola. Worker nie budował ani nie testował. Pliki skopiowano byte-for-byte; manifest `local-data/procedural-smoke/g1-assets-integration-20261103.json`. Codex wykonał integrację, testy i niezależną weryfikację.

Nie używano forgotten.otbm/companionów, map użytkownika, Dockera, bazy ani świata. Wspólne niezacommitowane zmiany zachowano; bez commit/push.

## Niezależne wyniki

Stan 2026-11-03: **PASS_G1_GENERATION_ASSET_CATALOG_ONLY; pełne G1 IN PROGRESS.**

Windows x64 build exit 0, EXE `e17b2f82…`. Pełny probe `g1-assets-rme-20261103`: PASS/exit 0/pusty stderr/normalny shutdown/assets wejściowe bez zmian, 419 aktualnych obiektów i 877 źródeł bazy G0 z dziesięcioma oczekiwanymi różnicami G1. 39 mutacji katalogu bez podbicia epoki odmówiło walidacji; po odtworzeniu wartości wynik znów przechodził. Skan miał zero alokacji. Osobne świeżo linkowane testy border-order i snapshotu również PASS, w tym 24 faultów snapshotu. Powtórzono cały probe 100×100/500×500, 79 faultów sandboxa, 106 przygotowania, maski, walidację oraz selection/Lua/UndoRedo.

Pełne hashe EXE: normalny `e17b2f82ce8671737a5b38398d1783f0387574bb9a7aedd2fd40185eef98bd08`, probe `4a61cc1af577f18d93010751039ed1fb6d151923d2dcfd80881977dc7c7ae4f0`. Log kanoniczny: `local-data/procedural-smoke/g1-assets-canonical-20261103.log` (33 kroki). Dowody w `g1-assets-rme-20261103/verification.json`, `current-verification.json`, logu probe i build-manifest. Aktualne focused runs: `g1-assets-border-order-rme-20261103` i `g1-assets-snapshot-rme-20261103`, oba exit 0/pusty stderr; verifier sprawdza ich źródła, obiekty i EXE względem aktualnego builda. Starsze dowody zachowano historycznie.

Przypadki obejmują wszystkie kategorie wymienione w TEST_PLAN, w tym rzeczywiste wiszące nieowning links w kontrolowanych prywatnych fixture. Skan ma zero obserwowanych new/new[] i jedną próbkę czasu 13.4346 ms na tym załadowanym katalogu. To nie benchmark porównawczy, gwarancja czasu, RSS ani koszt każdego katalogu. Szukanie globalnego ownera linków jest liniowe, koszt może rosnąć z liczbą reguł i borderów. Publiczne rejestry/unique_ptr owners muszą pozostać prawidłowe; nie sprawdzono dowolnej korupcji pamięci, równoległych mutacji, trwałego OOM ani mid-call stop.

Dotychczasowe fingerprinty 100×100/500×500 pozostały identyczne; estymaty i peak new potwierdzono ponownie w raporcie. Podczas startupu prywatnej instancji informacyjny dialog Warnings dotyczący referencyjnych assets obsłużono przez computer-use, bez restartu lub edycji assets. To nie unattended test ani wizualna akceptacja renderowanego preview generatora. Po pełnym przebiegu normalny exit 0 i brak pozostawionych prywatnych okien. git diff --check PASS.
