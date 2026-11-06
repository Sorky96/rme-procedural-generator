# G1 — atomowy commit konkretnego preview i historia

Aktualizacja 2026-11-03: osobny [G1_DIALOG.md](G1_DIALOG.md) potwierdza bieżące GUI 100×100 z minimapą i Place/Cancel/Undo/Redo. Poniższy raport zachowuje historyczny zakres core; pełne G1 pozostaje IN PROGRESS.

Stan 2026-11-03: **PASS_G1_ATOMIC_TERRAIN_HISTORY_CORE_ONLY**. Pełne **G1 IN PROGRESS**; zaliczenie dotyczy rdzenia commitu/historii, bez GUI workflow generatora.

## Kontrakt

`DrawOperations::commitTerrainPreview` i `commitSelectedTerrainPreview` ponownie sprawdzają aktualny edytor/offline, mapę, payload snapshotu, assets, konfigurację i odpowiednio dokładną maskę membership. Kopiują konkretne kafle własnego preview: ID, subtype, kolejność itemów, flagi i minimapColor pozostają identyczne. Commit/Undo/Redo nie wywołują brushy, RNG, ponownej generacji ani autobordera.

Wynik jest potwierdzeniem przyjęcia akcji do historii. Callback Lua uruchomiony po publikacji może wykonać następną edycję lub Undo; receipt nie jest trwałym potwierdzeniem bieżącej zawartości mapy. Wszystkie dane receipt powstają przed callbackami.

`ACTION_GENERATE_TERRAIN` tworzy osobny wpis także przy `GROUP_ACTIONS=1`, pomiędzy ręcznymi akcjami oraz kolejnymi generacjami. Zwykła ścieżka `addAction/addBatch` nie przyjmuje tego typu. Generator korzysta z istniejących `Action`, `Change`, `BatchAction` i `ActionQueue`, z osobną ścieżką przygotowanego swapu; nie dodaje drugiej historii.

## Granica atomowości

Przed pierwszą zmianą kafla powstają kopie wyników, Change/batch, rezerwacja infrastruktury mapy, wpis cache minimapy, ewentualna pojemność Selection i miejsce w deque historii. Dopiero potem następują bezalokacyjne wymiany właścicieli kafli i wskaźników membership oraz publikacja historii. Stare kafle przechodzą bezpośrednio do Change, więc Undo zachowuje oryginalną tożsamość i pełne dane. Nie usuwa się Redo ani starej historii przed ostatnią możliwą odmową/alokacją.

`BaseMap::TileReservation` usuwa przy odmowie wyłącznie utworzone przez siebie cells/nodes/floors i odtwarza chunk masks. Zachowuje istniejące TileLocation i metadane. Niewykorzystana pojemność wektora gridu może pozostać; zawartość gridu, liczba kafli, revision, dirty flag, zaznaczenie i historia mają pozostać bez zmian. `swapReservedTile` nie aktualizuje indeksów encji: ścieżka odmawia kafli z domem, spawnem, creature, mapflags/extra oraz location counts/exits. Nie jest ogólną transakcją dowolnych encji.

Undo/Redo rezerwują potrzebne zasoby przed zmianą indeksu historii. Błąd alokacji minimapy lub kolekcji zaznaczenia zwraca odmowę bez częściowej zmiany. Live, aktywna transakcja Lua i busy/deferred selection również odmawiają. Podczas przejścia historii zwykłe sesje Selection i transakcje Lua nie mogą się rozpocząć. Dirty tracker/minimapa/obserwatory GUI są powiadamiane po publikacji; wyjątek obserwatora nie wycofuje przyjętej akcji. Lua `actionChange` i zmiany membership emitują się po zwolnieniu blokady historii.

## Zaznaczenie i budżet

Membership jest niezależne od flag kafla i itemów. Change wymienia faktyczny stan członkostwa w obu kierunkach, również dla pustych kafli bez bitu selected i dla przywrócenia po null-source Undo. Pozwala to odtworzyć zaznaczenie po późniejszej zmianie bez wpisu historii. Ogólna ścieżka Action usuwa przemieszczane wskaźniki bez polegania na selected bit i zapamiętuje brakujące flagowe odtworzenie przy zwykłym Undo; obejmuje to późniejszą akcję Selection po generacji.

`TerrainCommitBudget` jest fail-closed. Jeden licznik uint64 obejmuje nadal posiadane przygotowanie oraz konserwatywny koszt commitu/Undo: nowe i przemieszczane kafle, infrastrukturę, Change/Action/batch, historię, scratch i zapas Selection. Publiczne liczby `PreparedTerrain::memory` nie obniżają opłaty: wykonywane jest ponowne oszacowanie kontenerów/payload i użyty prywatny peak preview; pozostaje większa opłata, bez refundowania wcześniejszych etapów. Nowy batch odmawia ponad UINT32_MAX, ponieważ stary cache BatchAction jest uint32. Generator respektuje także ustawienia liczby i pamięci Undo, bez zmiany polityki zwykłych akcji.

Zapas Selection wynosi `3 * sizeof(Tile*) * (S0 + liczba zmian) + 32`. Przed przyszłym Undo/Redo rezerwacja może odmówić, jeśli bieżąca kolekcja wymagałaby większej nowej alokacji; przy istniejącej wystarczającej pojemności swap nie alokuje. Odmowa zachowuje mapę i historię, po zmniejszeniu zaznaczenia można ponowić. To estymaty polityki, nie twardy limit alokatora ani RSS. Inna historia, source map/assets, inne zadania, GUI/render/obserwatory i alokacje C/DLL/GPU pozostają poza limitem.

## Claude i niezależna weryfikacja

Codex zaprojektował i zintegrował ścieżkę commitu oraz testy. Claude implementował wyłącznie `source/map/basemap.h/.cpp` w dedykowanym worktree; przyjęto sprawdzony diff i byte-identical integration manifest. Run implementacyjny: `f1a70830-50b7-4919-b699-fc2e39353beb`. Review: `f9c68d5b-ef2d-4fed-b323-3bf49f91ca77`, `46601207-d478-4391-aaa3-1a175876b5ee`, `1c06e930-0bca-4366-a11c-c2ebfd40000b`, `f9d6ea26-ebe7-4a57-8243-ce26b04b041e`. Raporty completed/exit 0, bez permission denials, pusty stderr; read-only runs nie zmieniły checkoutu. Uwagi dotyczące callbacków, preflight minimapy, Redo membership i równej liczby zmienionych członków uwzględniono. Ostatnią drobną zmianę powiadomienia przy równolicznej zamianie oraz ogólnego komunikatu odmowy historii wykonał i niezależnie sprawdził Codex po ostatnim review; nie są deklarowane jako byte-identical review Claude. Potwierdzono w kodzie, że 32 B allowance zgadza się z kSnapshotAllocationOverhead, a nazwy brushes są inline std::array, nie osobnym heap vector. SelectionThread jest uruchamiany po main-selection.start i dołączany przed finish, bez yield; nie wymuszano dowolnego równoległego użycia kolejki. Review nie zastępuje wykonania testów.

Końcowy Windows x64 build exit 0: `local-data/procedural-smoke/g1-commit-complete-canonical-20261103.log`.

Pełny run `g1-commit-complete-rme-20261103`: PASS/exit 0/normalny shutdown/pusty stderr/assets wejściowe bez zmian. `current-verification.json` potwierdza 421 aktualnych obiektów RME oraz 877 bazowych źródeł G0 z dokładnie 22 oczekiwanymi różnicami G1. Porównano także bieżące źródła i hashe EXE; istniejące, niezwiązane whitespace w współdzielonym G0 nie było czyszczone.

Normalny EXE SHA256: `8f6cb69762f2df0641a3f0e150329cb85197648dd8b1fe3f4677a4dad7b6abc1`. Probe SHA256: `0cdbe48cfc71479ac8416434790e919fc5ef30c3f247efd41998ddc6695fccd2`.

| Rozmiar | Estymata transition/Undo B | Peak śledzonego new B | Alokacje przed publikacją | Fingerprint konkretnego wyniku |
|---|---:|---:|---:|---:|
| 100×100 | 8881480 | 3965894 | 80195 | 6608263949644166501 |
| 500×500 | 218763816 | 98785798 | 1987447 | 173161415496114986 |

Przeszło 112 wymuszonych błędów ordinary new/new[] commitu, 15 prób rezerwacji, dokładny limit i jeden bajt za mało, maska z otworem, dwie osobne generacje i ręczne rysowanie przed/po, aktywna transakcja Lua, busy/live/thread/stop, zmienione membership, sfałszowane publiczne accounting i overflow. Sprawdzono faktyczne opróżnienie cache minimapy przed faultem Undo i Redo, fault realnego powiększenia Selection, odmowę jej budżetu i retry, flagged/unflagged null-source Redo, późniejsze zwykłe Selection Undo, równoliczną zamianę członków z Lua selectionChange i callback actionChange tworzący osobną akcję. Dotychczasowy pełny probe 100/500, 79 faultów sandboxa, 106 przygotowania, 29 odmów prepared i 39 mutacji katalogu assets również PASS.

Pomiary new obejmują testowy scalar/array C++ allocator i nie są RSS, C/DLL/aligned/GPU ani obietnicą wydajności. Zero alokacji wykazano w samym przygotowanym swap/publikacji do pierwszego post-publish obserwatora przy dostępnym wpisie cache; potrzebna alokacja preflight jest osobno testowana i może odmówić. Obserwatory po akceptacji nie należą do obszaru wymuszonego rollbacku.

Liczba 15 oznacza próby rezerwacji na kolejnych ordinal alokacji, nie liczbę potwierdzonych odrębnych `bad_alloc`: zachowana pojemność gridu może zmniejszyć liczbę alokacji przy kolejnej próbie. Liczba 112 commitu dotyczy rzeczywiście zaobserwowanych wymuszonych odmów.

Regresja zwykłej kolejki: `g1-commit-complete-queue-20261103`, świeży link z tym samym normalnym EXE, wynik `PASS`, przypadki `35`. Porównania prywatnych snapshotów OTBM/XML i istniejącego Redo/Undo wykonuje niezmieniony probe G0. Ostrzeżenia GUI: `35`; zapisano też osiem celowo wstrzykniętych awarii powiadomienia GUI przy pełnym, cofawalnym commit. Ten historyczny harness nie jest czystą wizualną bramką ani pełnym ponownym G0. Jego ograniczeń nie przeniesiono na czysty stderr głównej próby G1.

Informacyjne ostrzeżenie startupu curated assets zamknięto ręcznie przez computer-use w tej samej instancji. Nie edytowano assets. Polecenie odtwarzania: `rtk proxy <PY> docs/procedural/smoke/g1_run_resolver_probe.py local-data/procedural-smoke/NEW_RUN local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --commit --visible`, następnie `rtk proxy <PY> docs/procedural/smoke/g1_verify_preview_probe.py local-data/procedural-smoke/NEW_RUN --commit`. `<PY>` = `C:/Users/Sorky/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe -X utf8`.

Wcześniejsze przebiegi `g1-commit-rme-20261103`, `g1-commit-final-rme-20261103` i `g1-commit-membership-rme-20261103` zachowano historycznie; ostatni runner przekroczył 180 s przy oczekiwaniu na startup warning, a jego późniejszy PREVIEW_PASS nie otrzymuje aktualnego/pełnego zaliczenia. Nie restartowano tego procesu; pozwolono tej samej instancji zakończyć się normalnie. Te przebiegi nie potwierdzają bieżącego EXE po ostatnich poprawkach. Zachowano też błąd pierwszej kompilacji nowego modułu (`g1-commit-canonical-20261103.log`); usunięty problem dedukcji listy const/mutable Tile* nie jest pominiętym testem.

## Nadal poza zaliczeniem

Brakuje GUI Commit/Cancel, renderowanego preview, responsywnego harmonogramu oraz końcowego GUI/OTBM round-trip generatora. Nie wymuszano trwałego/systemowego OOM, dowolnych równoległych mutacji lub korupcji ownerów assets, ani zgodności wszystkich platform. Zwykła kolejka nie otrzymuje ogólnej gwarancji atomowości dowolnego istniejącego Action. Obsłużenie informacyjnego startup warning prywatnych assets nie jest wizualną akceptacją generatora. Nie otwierano map użytkownika, forbidden map/companionów, bazy ani Dockera; bez commit/push. G2 nierozpoczęte.
