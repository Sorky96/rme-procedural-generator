# Procedural World Generator — postęp

## 2026-10-27 — ręczna historia i rozszerzony round-trip

Stan: **G0 IN PROGRESS**. Bazą kodu jest `a71fe4b`; wyniki poniżej z tego buildu. Powstała gałąź `docs/procedural-g0-validation` z `master`, następnie fast-forward do naprawy callbacka.

- **G0-06 PASS w badanym zakresie:** ręczny Draw gumką 1×1, transakcja Lua i ręczny Draw brushem `dried grass` utworzyły oddzielne wpisy. Trzy Undo i trzy Redo sprawdziły migawki ground, stosów ID, flag, house ID i obecności encji dla obszaru 24×24 oraz osobny tile Lua; po kolejnych Undo fixture przywrócono. `group_actions=true`, `use_automagic=true`, limit historii 400, 40 MiB. Pierwsza próba drugiej edycji gumką nie zmieniła zawartości, więc asercja testu słusznie ją odrzuciła; cofnięto tę próbę i użyto innego brusha.
- **Częściowy G0-08 PASS:** syntetyczne tile na piętrach 6 i 7 z ground UID/AID oraz flagą, spawnem radius 3 i `Rat` (90 s) utworzono, zapisano i odczytano w RME z `1098-BlackTek`. Kontrola objęła także 400 bazowych ground i halo; osobno sparsowano XML spawnu. Bez domu, stref, kontenerów i odbiorcy zewnętrznego.
- **G0-07 FAIL dla istniejącego Item:** zmiana `ground.actionId` wewnątrz nieudanej transakcji nie została wycofana mimo propagacji błędu. Udana transakcja z tą samą zmianą również nie utworzyła wpisu historii. Diagnostyka przywróciła wartość bez zapisu. Potrzebna osobna naprawa śledzenia mutacji Item oraz testy regresyjne.
- **Potwierdzone ryzyko utraty danych w Save As:** zapis `g0-metadata.otbm` obok `g0-smoke.otbm` zachował w nagłówku nazwy `g0-smoke-house.xml` i `g0-smoke-spawn.xml` i nadpisał istniejący XML spawnu mapy źródłowej. Dotyczyło to wyłącznie map syntetycznych. Skopiowano kompletny zestaw do osobnego katalogu; pierwotny pusty XML spawnu odtworzono na podstawie wcześniej zweryfikowanej zawartości, a osieroconą kopię OTBM usunięto po sprawdzeniu identycznego hasha. Poprawka tego workflow i regresja ochrony wszystkich plików pomocniczych pozostają zadaniem przed G1.
- Podczas zamykania sesji testowej pojawiła się asercja wxWidgets `GetWindow() != nullptr` w `GetDescription()`; wybrano Stop po zapisaniu i zamknięciu mapy. Nie ustalono przyczyny ani związku z testem.
- Osobny target `test_universal_rotator` zbudowano z `--parallel 2`, następnie `ctest --test-dir build-ninja -R "^test_universal_rotator$" --output-on-failure`: **PASS 1/1**, 0.20 s, kod 0. Nie jest to pełna suita projektu.
- Szczegółowe kroki i wyniki: [TEST_PLAN.md](procedural/TEST_PLAN.md). Zachowane skrypty można uruchomić tylko na izolowanej kopii fixture.

## 2026-10-27 — G0-B: naprawa błędu callbacka Lua

Aktualny stan: **G0 IN PROGRESS**, konkretny bloker ignorowanego callbacka usunięty. Gałąź `fix/lua-transaction-callback` utworzona z `master` i uzupełniona fast-forwardem audytu `b2bc871`.

- `LuaAPI::transaction` sprawdza wynik `sol::protected_function`; błąd callbacka trafia do istniejącego rollbacku i wraca do Lua. Bez zmian implementacji snapshotów, `ActionQueue` i serializacji.
- Build końcowego źródła Windows x64 **PASS**, kod 0, przyrostowo z `--parallel 2`; EXE z 18:52:13 uruchomiono w GUI.
- Nowy test `lua_tests/test_transaction_rollback.lua` **PASS**, 18:53:46: trzy błędy z zachowanymi komunikatami, sześć mutacji wycofanych, historia bez nowych wpisów po błędzie. Udane transakcje i ich osobne Undo/Redo poprawne; fixture przywrócone do 400 pól.
- Oryginalna reprodukcja `g0_error_probe.lua` **PASS**, 18:54:08. Końcowy verifier **PASS**, 18:54:30. SHA-256 OTBM przed/po identyczny; test nie zapisywał mapy.
- Dokumentację API i instrukcję uruchamiania regresji zaktualizowano. CodeGraph i Context7 posłużyły do kontroli mechanizmu; selektor `sorky-validate-change` nie ma trasy dla RME, więc zastosowano build i testy rzeczywistego edytora.
- Kontrola diffu oraz UTF-8, bloków kodu i względnych linków dokumentacji **PASS**. Zachowane skrypty zgodne z wykonanymi po normalizacji CRLF/LF; początkowe porównanie bajtowe odrzuciło wyłącznie zmianę końców linii po przełączeniu gałęzi. Nowy skrypt regresji zgodny z kopią uruchomioną w RME.
- Nadal niezweryfikowane: ręczna edycja obok transakcji, wiele pięter i złożone metadane, zagnieżdżone transakcje, zamknięcie/przełączenie mapy w callbacku, awarie alokacji oraz zgodność z zewnętrznym RME/OTS. Pełnej suity Lua/CTest nie uruchamiano. Szczegółowe logi: [TEST_PLAN.md](procedural/TEST_PLAN.md#g0-b--naprawa-callbacka-i-regresja-2026-10-27).

Następne zadania: G0-06 z ręczną edycją; fixture z metadanymi/wieloma piętrami i round-trip; następnie zamknięcie decyzji adaptera przed G1-A.

## 2026-10-27 — wynik audytu i testów G0

Wpis historyczny audytu przed naprawą G0-B powyżej: **G0 IN PROGRESS**, G1 nierozpoczęte. Ówczesne zmiany obejmowały dokumentację i ręczne skrypty diagnostyczne, bez zmian kodu produkcyjnego. Szczegółowe polecenia, wersje, hashe oraz zakres dowodów: [TEST_PLAN.md](procedural/TEST_PLAN.md).

- Build Windows x64 **PASS**, kod 0: po problemie Ninja/PATH i błędzie pamięci PCH wznowiono ten sam target przez `cmake --build build-ninja --target rme --parallel 2`. EXE 8026112 bajtów, linkowanie `[382/382]`; uruchomiono GUI 4.1.2.
- Profil `1098-BlackTek` (`dat_only`) załadowany z assets wskazanych przez użytkownika i włączoną walidacją podpisów. Zakres `grass` działa; loader zgłasza błędne zakresy i brakujące ID innych materiałów. Standardowego OTB nie zatwierdzono dla tego DAT.
- Edycja syntetycznej mapy Lua: **400 ground + pojedyncze Undo + pojedyncze Redo PASS**. Halo pozostało nieobecne. Zapis OTBM i dwóch pustych XML, a następnie jawny odczyt z profilem BlackTek: **PASS dla tego fixture**. Verifier potwierdził wszystkie ground ID, puste stosy/encje/house ID/flagi i halo; trawa widoczna w rendererze.
- **Bloker: G0-07 FAIL.** Celowe `error()` po zmianie tile w `app.transaction` nie propaguje się i nie wycofuje zmiany: `propagated=false; tile_present=true; tile_count=401`. Końcowy assert diagnostyki zgłosił błąd. Jedno ręczne Undo oraz ponowny verifier przywróciły 400 pól; nie zapisano zmiany diagnostycznej.
- Zwykły File → Open dobiera standardowy profil `10.98` na podstawie nagłówka. Test odczytu wykonano przez Welcome z jawnym `1098-BlackTek`. Po zatwierdzeniu ścieżki w Preferences proces raz zniknął bez komunikatu; przyczyna nieustalona. Do dalszej próby użyto izolowanego `config.toml` i jawnego cwd.
- CodeGraph MCP był dostępny 26.10; 27.10 transport zamknięty, kontynuowano przez działający CLI `codegraph explore`. Context7 i `ast-grep` wykorzystano w audycie źródeł. Skill `sorky-validate-change` wskazał dla dokumentacji przegląd diffu.
- Nadal niewykonane: rozdzielenie historii z ręcznymi edycjami, pełne metadane UID/AID/domów/spawnów/stref i wiele pięter, zgodność z zewnętrznym RME/OTS. Dotychczasowe 33 PASS / 7 SKIP parsera i 34 PASS / 6 FAIL z niestandardowym klientem pozostają osobnymi wynikami.
- Końcowa kontrola dokumentacji: UTF-8, domknięte bloki kodu i względne linki w 5 plikach Markdown **PASS**; trzy zachowane skrypty są bajtowo zgodne z wykonanymi źródłami; oba puste XML pomocnicze poprawnie parsują się jako `houses`/`spawns`.

### Następne trzy zadania

1. G0-B: naprawić kontrolę wyniku callbacka i rollback `app.transaction`; zachowany [probe](procedural/smoke/g0_error_probe.lua) jest reprodukcją błędu, a nie zaliczonym testem.
2. Dokończyć G0-06 i round-trip fixture z metadanymi/wieloma piętrami; osobno zapisać wynik zgodności z docelowym odbiorcą.
3. Dopiero po G0 zamknąć decyzje adaptera C++ (RNG, snapshot, atomowy commit) i rozpocząć G1-A.

## 2026-10-26 — rozpoczęcie FAZY 0

Wpis historyczny: stan podczas trwającego buildu. Wynik końcowy i bieżące zadania opisuje wpis z 27.10 powyżej. Zastąpił wcześniejsze ograniczenia Git/CodeGraph z przeglądu przygotowawczego.

- Baza: `5a01a8fe1c34465559701d50681cc2210ef32c67`, lokalny initial snapshot. Nie ustalono równoważnego SHA upstream. Początkowy working tree czysty.
- Gałąź: `docs/procedural-phase0-audit`, utworzona z `master` przed zmianami.
- CodeGraph MCP działa; CLI zgłosił aktualny indeks 3328 plików / 21857 węzłów / 56553 krawędzi. Zbadano ścieżki rysowania, Undo, Lua, preview i zapisu; `ast-grep` potwierdził pobieranie brusha z GUI.
- Utworzono [architekturę i plan PR](procedural/ARCHITECTURE.md) oraz [plan testów z dowodami](procedural/TEST_PLAN.md). Szczególne ryzyka: natychmiastowy commit części batcha, GUI/RNG w rysowaniu, niepełna definicja pustego tile, zależność preview od kursora i obsługa błędu callbacka Lua. Wnioski runtime pozostają niepotwierdzone do smoke testu.
- Context7: dokumentacja Sol2 `/thephd/sol2`, obsługa wyniku `protected_function_result::valid()`. Jest to dokumentacja upstream; dokładne zachowanie wersji z vcpkg wymaga reprodukcji.
- Reprodukcja na zainstalowanych Lua 5.5.0#1 / Sol2 3.5.0#1 z konfiguracją projektu: ignorowane `sol::function` wraca bez wyjątku mimo `error()` i zachowuje wcześniejszy efekt uboczny; `valid()` wykrywa błąd. Lokalny probe C++ i polecenie opisuje `TEST_PLAN.md`. Potwierdza ryzyko wrappera transakcji, ale nie zastępuje testu prawdziwej mapy/rollbacku/Undo. Nie zmieniono kodu produkcyjnego.
- Build: pierwsze uruchomienie `build_ninja.bat` zatrzymane na Ninja poza `PATH`. Znaleziono lokalny Ninja 1.13.2 w vcpkg; ponowiono ten sam skrypt z tymczasowym `PATH` i `VCPKG_ROOT`. Wykryto CMake 4.3.0-rc3, Visual Studio 18 Community i MSVC 14.51.36231. Zbudowano zależności i skonfigurowano RME, ale kompilacja przy domyślnej równoległości zakończyła się `C3859/C1076` i Windows 1455 (brak pamięci wirtualnej dla PCH). Wznowiono ten sam target i build tree z `--parallel 2`; próba trwa. Nie zmieniano pagefile, źródeł ani kanonicznego skryptu.
- Testy `tools/test_tibia_data.py -v`: 40 przypadków, **33 PASS / 7 SKIP**, kod wyjścia 0. Pominięto testy integracyjne wymagające `Tests1098`.
- Odczyt `data/1098/items.otb`: **PASS**, 26282 itemy, wersja OTB 3.57.62-10.98. Poprawne CLI: `tools/tibia_data.py --otb data/1098/items.otb info`; początkowe umieszczenie `--otb` po `info` zostało odrzucone przez parser, a następnie poprawione.
- Początkowo nie znaleziono kompletu DAT/SPR, mapy OTBM ani gotowego EXE w checkout. Użytkownik wskazał następnie `C:\repo\ProjectSorky\otclient\data\things\1098` oraz `otclient_gl_x64.exe`. Inspekcja wskazanych DAT/SPR z lokalnym OTB 10.98 przeszła: podpisy `0x42A3` / `0x57BBD603`, 26282 itemy i 338944 sprite'y, brak ostrzeżeń. Dane tylko odczytano; EXE OTClient nie zastępuje buildu RME.
- Dalszy test ujawnił, że poprawne parsowanie i te same podpisy **nie dowodzą zgodności mapowania**: oryginalne testy uruchomione na kopiach wskazanych assets i lokalnego OTB w ignorowanym harnessie dały **34 PASS / 6 FAIL** (kod 1, bez SKIP). Błędy dotyczą oczekiwanego fixture, flag/ID i hasha sprite'ów. Nie zmieniano testów, oryginalnych assets ani mapowania, by uzyskać PASS. Szczegóły i log lokalny opisuje `TEST_PLAN.md`.
- CodeGraph potwierdził istniejący tryb `ItemDefinitionResolver::resolveDatOnly`; kandydatem do smoke jest profil `1098-BlackTek` z `data/clients.toml`. DAT ID 4526 odpowiada przechodniemu ground, a standardowy OTB mapuje server ID 4526 do innego client ID (4515). Wybór `dat_only` wymaga potwierdzenia ładowania i materiałów w GUI; nie zatwierdzono standardowego OTB dla tego klienta.
- GUI, Lua 20x20, rollback, Undo/Redo, round-trip RME i kompatybilność z zewnętrznym odbiorcą: **niewykonane**. Testy narzędzia danych nie zastępują tych dowodów.

### Historyczne zadania podczas buildu

1. Doprowadzić próbę kanonicznego buildu do wyniku końcowego i zapisać log/ograniczenia.
2. Potwierdzić kompletny zestaw assets i uruchomić smoke test zgodnie z G0-01–G0-09 w `docs/procedural/TEST_PLAN.md`.
3. Na podstawie reprodukcji zamknąć decyzje o granicy C++/Lua, RNG, snapshotach i atomowym commitowaniu; dopiero wtedy rozpocząć G1-A.

## 2026-10-26 — analiza i redakcja planu

Zakres sesji: przygotowanie `PROJECT_RME_PROCEDURAL_GENERATOR.md` do realizacji. Wersja planu: 1.1. Nie rozpoczęto implementacji generatora; przegląd źródeł jest częściowym przygotowaniem do G0, a nie zaliczeniem tej bramki.

### Wykonano

- Przeczytano plan, `AGENTS.md`, `RTK.md`, konfigurację CMake/vcpkg, fragmenty instrukcji skryptów i skryptu buildu.
- Sprawdzono punkty integracji: `DrawOperations`, `ActionQueue`, `LuaTransaction`, rejestrację `noise`/`algo`/`geo`, demo terenu, RNG ground/doodad, tożsamość mapy i zapis plików pomocniczych OTBM.
- Doprecyzowano MVP, minimalne GUI, snapshot i konflikty, deterministyczność adaptera, ochronę obszaru, rollback, budżet pamięci i kryteria testów. Szczegóły i lokalizacje źródeł: sekcja 1.0 planu.
- Context7: `/websites/wxwidgets/3.3`, dokumentacja komunikacji worker–GUI i życia odbiorcy zdarzeń. To weryfikacja dokumentacji, nie test RME.

### Dowody i ograniczenia

- `rtk git status --short --branch`: brak repozytorium Git; w katalogu nie ma `.git`, jest `.gito`. Nie zmieniano metadanych, nie utworzono gałęzi ani commita, SHA nieustalone.
- Brak `.codegraph/`: zgodnie z instrukcjami nie wywoływano zapytań do nieistniejącego indeksu ani nie uruchamiano indeksowania.
- `rtk proxy ast-grep --version`: 0.45.1. Zapytanie strukturalne `g_gui.GetCurrentBrush()` potwierdziło zależność trzech przeciążeń rysowania od aktualnego brusha GUI.
- Użyto `rg` i odczytów źródeł przez `rtk proxy`. Nie zakładano, że deklaracje API dowodzą poprawności runtime.
- Kontrola dokumentów w PowerShell: poprawne odczytanie UTF-8 bez znaków zastępczych, domknięte bloki kodu, sekcje 0–14 i 12 odwołań do istniejących plików źródłowych/skryptów — OK. Pierwsza próba kontroli przez `python` nie uruchomiła się (brak interpretera pod tą komendą); kontrolę wykonano przez PowerShell.
- Build, uruchomienie GUI, smoke test Lua 20x20, Undo/Redo, round-trip OTBM, testy regresyjne i benchmarki: **nie uruchomiono**. Zmiany obejmują wyłącznie dokumentację.

### Stan faz

| Faza | Status | Commit | Dowody/testy | Blokery lub zależności | Następne zadanie |
|---|---|---|---|---|---|
| 0 | IN PROGRESS | Audyt `b2bc871`, naprawa `a71fe4b` | Build, rollback tworzenia tile i mieszana historia PASS; częściowy round-trip pięter, UID/AID, flag i spawnu PASS | Bezpośrednia zmiana AID nie jest śledzona; Save As nadpisuje współdzielone XML; domy/strefy i odbiorca zewnętrzny niezweryfikowane | Naprawa obu błędów i pozostałe fixture |
| 1 | TODO | — | — | G0 | MVP jednego piętra |
| 2 | TODO | — | — | G1 | Teren i bordery |
| 3 | TODO | — | — | G2 oraz referencje | Analiza referencji |
| 4 | TODO | — | — | G3 oraz holdout | Profile L1/L2 |
| 5 | TODO | — | — | G4 | Topologia wielu pięter |
| 6 | TODO | — | — | G5 | Ranking i feedback, opcjonalny ML |
| 7 | TODO | — | — | G6 | Rozbudowa GUI |
| 8 | TODO | — | — | G7 | Szersze pomiary i niezawodność |
| 9 | TODO | — | — | G8 | Beta i wydanie |

### Historyczne zadania z przeglądu przygotowawczego (aktualny plan powyżej)

1. Ustalić pochodzenie kopii, dostęp do właściwego checkoutu Git i SHA; utworzyć gałąź z `master` i zapisać lokalne zmiany. CodeGraph wykorzystywać po udostępnieniu indeksu.
2. Potwierdzić toolchain, dostępne assets i mapę testową; wykonać kanoniczny build Windows x64 i zapisać wynik, bez deklarowania powodzenia z wyprzedzeniem.
3. Przeprowadzić smoke test na kopii kompletu plików (20x20, Undo/Redo, zapis/odczyt), a wyniki audytu i plan integracji umieścić w `docs/procedural/ARCHITECTURE.md` oraz `docs/procedural/TEST_PLAN.md`.
