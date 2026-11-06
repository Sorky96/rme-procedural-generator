# G1: bieżący edytor i cel offline

## Nowszy etap: commit i historia

Rdzeń atomowego commitu/Undo/Redo i publikacja actual membership zostały następnie zaimplementowane i zweryfikowane w [G1_COMMIT.md](G1_COMMIT.md). Poniższy raport zachowuje historyczny zakres samego target guard; pełna bramka G1 pozostaje IN PROGRESS.

Stan 2026-11-03: **PASS_G1_CURRENT_OFFLINE_TARGET_ONLY; pełne G1 IN PROGRESS**. Wycinek uzupełnia [katalog assets](G1_ASSETS.md), [walidację wyniku](G1_VALIDATION.md) i [dokładną maskę](G1_MASK.md). Ten etap składa kontrole potrzebne przy przyszłym wejściu do commitu.

## Kontrakt

`validateTerrainTarget` i `validateSelectedTerrainTarget` w `source/procedural/adapter/terrain_target.{h,cpp}` pobierają bieżący edytor przez `g_gui.GetCurrentEditor()` przy każdym wywołaniu. Nie przyjmują ani nie przechowują dawnych Editor/Map/MapTab pointers. Tożsamością jest istniejąca `snapshot.input.mapGeneration`, przechwycona podczas przygotowania.

Wspólny preflight odrzuca kolejno niewłaściwy wątek (przed dostępem do GUI), stop, brak edytora, inną generację mapy, dowolną sesję live oraz busy/deferred selection. Używa istniejących getterów i nie zmienia fokusowania kart. Wariant prostokątny następnie wywołuje `validatePreparedTerrain` z aktualnymi config/profile/mask. Membership GUI nie jest z niego domyślnie wyprowadzane; trwająca sesja zaznaczenia jest jednak blokowana dla obu wariantów.

Wariant zaznaczenia sprawdza dodatkowo `validateGenerationSelection` na tym bieżącym edytorze, wymaga tego samego regionu/Z w konfiguracji i przekazuje dokładną maskę do walidacji wyniku. `TargetValidationError` zachowuje oddzielne typy błędów celu, membership i przygotowania; nie zamienia ich w ogólne false. Preflight jest współdzielony, dzięki czemu nie wykonuje się powtórnie pełnego skanu assets/snapshotu.

Wywołania są synchroniczne na głównym wątku, bez yield, alokowania wyników, zmian mapy, RNG, cache, katalogów lub historii. Normalna ścieżka poprawnie zarejestrowanej karty nie alokuje również przy wyszukaniu bieżącego edytora. Wewnętrzny lookup tabbook używa istniejącego wpisu `conv`; dowolna korupcja rejestracji notebooka pozostaje poza kontraktem.

## Życie i współdzielone widoki

Sukces zwraca pożyczony `Editor*` do natychmiastowej kontynuacji w tym samym wywołaniu GUI. Nie nadaje zachowanego prawa do późniejszej mutacji. Po yield, nowym zdarzeniu lub zapamiętaniu wyniku trzeba ponownie pobrać i sprawdzić cel. Przyszły commit musi wejść przez tę kontrolę bezpośrednio przed własną atomową operacją.

Jednostką tożsamości jest mapa/edytor. Drugi widok tego samego Editor może przejść, także po zamknięciu pierwszego widoku. Przełączenie na inną mapę odmawia; powrót do niezmienionego źródła może przejść. Po zamknięciu ostatniego widoku i utworzeniu nowej mapy generacja jest inna, niezależnie od ewentualnego ponownego użycia adresu przez alokator. Stary wskaźnik źródła nie jest odczytywany.

Kontrola `IsLive` obejmuje zarówno klienta, jak i serwer, także przed zestawieniem połączenia. Adapter wyłącznie odczytuje ten stan. Nie tworzy sesji ani gniazd i nie wykonuje connect, bind, accept, start lub stop sieci.

## Pozostałe granice

Nadal trzeba zaimplementować atomowy apply/rollback/Undo/Redo, właściwe publikowanie selection membership dla pustych pól, niezależną historię ręcznego rysowania, budżet pamięci Undo i kontrolę reentrancy/aktywnej transakcji Lua lub historii. Guard sam nie mutuje mapy, więc nie rozwiązuje tych obowiązków. Pozostają minimalny dialog/renderowane preview, responsywny GUI Cancel/harmonogram i GUI/OTBM round-trip 100×100/500×500. Dotychczasowe wyłączenia RSS, surowych pikseli/GPU/cache i spoza zakresu katalogu obowiązują nadal.

## Claude i review

Claude wykonał wyłącznie dwa nowe pliki, run `1ab3e529-7bf2-4610-81b3-f3031eec2bb4`; poprawka dokumentacji `ead98698-19d3-4c0e-929b-b2fe4ce034dc`. Oba completed/exit 0, pusty stderr, brak odmów, zgodne report/stdout i niezmieniony zastany tracked diff. Codex przejrzał kod, rzeczywistą ścieżkę GUI lookup oraz konstrukcję/niszczenie sesji live. Poprawiono zbyt szerokie zdanie o pomijaniu selection: pomija się membership prostokąta, ale busy/deferred jest zawsze sprawdzane. Worker nie budował ani nie testował.

Zaakceptowane pliki skopiowano byte-for-byte, manifest `local-data/procedural-smoke/g1-target-integration-20261103.json`. Codex wykonał integrację CMake, testy i niezależną walidację. Wspólne zmiany zachowano; bez commit/push. Nie używano forgotten.otbm/companionów, map użytkownika, Dockera, bazy lub świata; G2 nierozpoczęte.

## Niezależne wyniki

Kanoniczny Windows x64 `build_ninja.bat` exit 0: `local-data/procedural-smoke/g1-target-canonical-20261103.log`. Normalny EXE SHA-256 `8282a1c75a88bd1d2553bccb2cb553bb5d763a24c599dc1e23f9d0d170ceeb62`. Pełny nowy normalny Application z testowym wxModule w `g1-target-rme-20261103`: **PASS/exit 0/pusty stderr/normalny shutdown/assets wejściowe bez zmian**. Probe EXE `e1a427c1cc42b3e209606263c6683fb465c7ebda8e15eff468bd31982acce85b`.

Verifier `g1_verify_preview_probe.py --target` potwierdził 420 aktualnych obiektów RME, 877 źródeł bazy G0 i dziesięć oczekiwanych różnic G1. Pełne dowody w `verification.json`, `current-verification.json`, build-manifest i logu probe. Wcześniejsze focused runs border-order/snapshot zachowują aktualne właściwe źródła/obiekty, ale są historycznymi pełnymi EXE sprzed dodania adaptera target; nie nazwano ich nowymi przebiegami.

Aktualny edytor, prostokąty od (400,100,7):

| Rozmiar | Alokacje guardu | Fingerprint konkretnego preview |
|---|---:|---|
| 100×100 | 0 | 10815617532789726571 |
| 500×500 | 0 | 14888103173838574161 |

Oba wywołania zwróciły dokładnie bieżący Editor*, zachowały snapshot, revision, tile count, rozmiar/pozycję historii, RNG i cache obserwowanych edytorów. Wywołanie wybranego zakresu 500×500 oraz osobny przypadek 4×4 z pięcioma faktycznymi pustymi członkami również wykonały zero alokacji. Wybrany zakres 100×100 był testowany wcześniej jako capture/przygotowanie nieaktywnej mapy; tego wyniku nie przedstawiono jako sukcesu guardu aktywnego celu.

**22 odmowy** obejmują inną bieżącą mapę, oba warianty w busy/deferred selection, pre-cancel i worker, propagację zmienionej konfiguracji oraz revision, maskę niezgodną z wynikiem, region/mapGeneration zaznaczenia, zamianę membership przy stałych bounds/count oraz późniejszy stale snapshot mimo przywrócenia membership. Po zamknięciu źródła oba warianty zwracają NoEditor; po utworzeniu nowej mapy oba WrongMap. Nie przechowuje się starego Editor pointer. Nie wymuszano konkretnego ponownego użycia adresu przez alokator.

Rzeczywisty dodatkowy widok tej samej mapy został zaakceptowany, także po zamknięciu pierwotnego widoku. Przejście do innej mapy odmówiło, a powrót do niezmienionego źródła przeszedł. Osobne niepołączony LiveClient i niepowiązany z portem LiveServer dały LiveSession w obu wariantach. Konstrukcja tych fixture nie wywołuje connect/resolve/bind/accept ani NetworkConnection::start; cleanup zamyka wyłącznie nieaktywne obiekty własnego procesu. Powrót obiektu serwerowego do lokalnego stanu ponownie dopuszcza zgodny wynik. Nie jest to test połączonej sesji sieciowej.

Powtórzono cały probe: cztery sandboxy 100×100/500×500, 79 faultów sandboxa, 106 przygotowania i dokładne limity, 29 odmów walidacji, 39 mutacji katalogu, maski z otworami, epokę/unload/reload, selected-context/selected-border, cache/zdarzenia dwóch edytorów i zwykłe selection/Lua/UndoRedo. Dotychczasowe fingerprinty, estymaty i piki new pozostają zgodne. Przebieg ma ręcznie obsłużony informacyjny startup Warnings prywatnych assets przez computer-use, w tej samej instancji bez restartu; nie jest unattended ani wizualną akceptacją renderer-preview. Po wyjściu brak prywatnych okien. `git diff --check` PASS.

Nie zaliczono commitu generatora, reentrancy/wykluczenia aktywnej transakcji Lua/historii, jego Undo/Redo/membership, GUI przycisku Commit, responsywnego Cancel, trwałego OOM/RSS ani GUI/OTBM round-trip. Nie zmieniano istniejących klas GUI/Editor/LiveManager ani zachowania Lua.
