# Generator proceduralny — plan testów i dowody G0

Stan na 2026-10-27. Baza: `5a01a8fe1c34465559701d50681cc2210ef32c67`. Kryteria pochodzą z [planu projektu](../PROJECT_RME_PROCEDURAL_GENERATOR.md); punkty integracji opisano w [ARCHITECTURE.md](ARCHITECTURE.md).

## Wyniki wykonane w audycie

| Sprawdzenie | Wynik | Co rzeczywiście potwierdza |
|---|---|---|
| `rtk git status --short --branch` przed zmianami | Czysty `master`; utworzono `docs/procedural-phase0-audit` | Punkt odniesienia lokalnego kodu; nie ustala SHA upstream |
| `rtk proxy codegraph status .` | Aktualny indeks, 3328 plików | Dostępność indeksu; nie pokrycie testami |
| MCP CodeGraph i `ast-grep` dla `g_gui.GetCurrentBrush()` | Potwierdzone zależności trzech przeciążeń `DrawOperations::draw` | Rzeczywista zależność rysowania od GUI |
| Pierwsze `rtk proxy cmd /c build_ninja.bat` | FAIL: Ninja nieobecny w `PATH` | Problem środowiska, bez kompilacji źródeł |
| Ponowienie kanonicznego buildu z lokalnym Ninja i `VCPKG_ROOT` | Zależności i konfiguracja OK; kompilacja RME FAIL, `C3859/C1076`, Windows 1455 | Zabrakło pamięci wirtualnej przy domyślnej równoległości; nie jest to zaliczony build |
| Wznowienie tego samego targetu/build tree z `--parallel 2` | **PASS**, kod 0, `[382/382] Linking CXX executable rme.exe` | 2026-10-26: `build-ninja/rme.exe`, 8026112 bajtów, uruchomiony w GUI jako OTAcademy Map Editor 4.1.2; bez zmiany źródeł lub pagefile |
| `tools/test_tibia_data.py -v` przez Python runtime Codex | 40 testów: 33 PASS, 7 SKIP | Parsery i CLI na danych syntetycznych. Integracja `Tests1098` pominięta z powodu braku klienta |
| `tools/tibia_data.py --otb data/1098/items.otb info` | PASS: 26282 itemy, OTB 3.57.62-10.98 | Odczyt istniejącego OTB, bez potwierdzenia DAT/SPR i zgodności renderowania |
| Inwentaryzacja plików w checkout | Nie znaleziono DAT, SPR, OTBM ani gotowego EXE; obecne OTB w `data/` | Brak kompletnego zestawu do testu GUI w tym katalogu; nie przeszukiwano prywatnych map poza projektem |
| Assets wskazane później przez użytkownika | `C:\repo\ProjectSorky\otclient\data\things\1098\Tibia.dat` i `Tibia.spr` | Użytkownik wskazał też `otclient_gl_x64.exe`; nie jest to build RME i nie zastępuje smoke edytora |
| Inspekcja wskazanych DAT/SPR + lokalnego OTB 10.98 | PASS samego parsowania, kod 0, brak ostrzeżeń: DAT `0x42A3`, SPR `0x57BBD603`, 26282 itemy DAT i OTB, 338944 sprite'y | Parser rozpoznał format 10.98, extended, improved animations, frame groups, RGB (24 próbki walidacji SPR). Podpisy nie dowodzą zgodności semantycznej mapowania OTB — dalszy test wykrył rozbieżności |
| Te same testy narzędzia z kopiami wskazanych assets w odizolowanym harnessie | **34 PASS / 6 FAIL**, kod 1, bez SKIP | Wskazany DAT różni się od fixture oczekiwanego przez oryginalne testy. Błędy dotyczą liczby itemów, właściwości ID 930/1162, sprite'ów i hasha OTB; nie zmieniano oczekiwań testów |
| Probe callbacka Sol2 z konfiguracją projektu | REPRODUCED: `func();` nie rzuca mimo `error()` po efekcie ubocznym; kod diagnostyki 0 | Potwierdza zachowanie Lua/Sol2 używanych przez build, nie testuje mapy, transakcji RME ani Undo |
| Smoke Lua 20x20 w GUI, `1098-BlackTek` | **PASS** umieszczenie 400 ground, pojedyncze Undo i pojedyncze Redo, 2026-10-26 | Region X/Y 100–119, Z=7; halo 99–120 pozostało nieobecne. Test pustej mapy bez chronionych metadanych; nie sprawdza borderowania ani rozdzielenia historii od ręcznych edycji |
| Zapis standardowym workflow | **PASS zapisu**, `g0-smoke.otbm` 3351 bajtów oraz XML domów i spawnów po 33 bajty | Zapis nie oznacza jeszcze poprawnego ponownego odczytu |
| Zwykłe File → Open zapisanej mapy | Wybrano standardowy profil `10.98`, wymagający innych assets | Nagłówek nie rozróżnia profili o tym samym numerze klienta/OTB. Anulowano wybór assets dla niewłaściwego profilu; wymagany jawny wybór `1098-BlackTek` |
| Wznowienie GUI 2026-10-27 | Proces zniknął po OK w Preferences podczas ustawiania Client Path | Potwierdzono brak okna/procesu; brak komunikatu i brak znalezionego wpisu Application Error 1000. Przyczyna nieustalona; późniejsza próba z izolowanym config poniżej |
| Ponowny odczyt z izolowanym config i jawnym profilem | **PASS dla fixture**, 2026-10-27 18:36:36 | 400 dokładnych ground ID z brusha, puste stosy/creature/spawn/house ID/mapFlags, halo nieobecne. Trawa widoczna w rendererze. Puste pliki domów/spawnów nie dowodzą zachowania złożonych metadanych |
| Błąd callbacka na mapie, G0-07 | **FAIL rollbacku**, 2026-10-27 18:37:31 | `propagated=false; tile_present=true; tile_count=401`. `error()` został zignorowany przez wrapper, a zmiana trafiła do historii. Błąd widoczny w logu pochodzi dopiero z końcowego `assert` diagnostyki |
| Sprzątanie po próbie błędu | **PASS ręcznego Undo i kontroli**, 18:38:26 | Jedno Undo, następnie ponowiony verifier potwierdził oryginalne 400 pól. Nie zapisano diagnostycznej zmiany na dysku |

Uwaga do CLI: argumenty globalne narzędzia poprzedzają podkomendę. Próba `info --otb ...` została odrzucona przez parser; poprawne polecenie podano w tabeli. Nie traktować nieudanej próby jako błędu odczytu OTB.

SHA-256 wejścia smoke testu (tylko odczyt, bez kopiowania assets do Git):

| Plik | SHA-256 |
|---|---|
| DAT 10.98 ze wskazanego katalogu | `14bc3d04b8167744e3207a09da1ae1fecb89d4222e50ed93850defee032458cb` |
| SPR 10.98 ze wskazanego katalogu | `041f55f13662517e57cf93fb0c3f743ad8d2bd8aec14d408842f5bf57eeb6653` |
| `data/1098/items.otb` | `9f73f8bf0d5799cd7caef1b37ec979a14fcaa52405127c7838a13b32ab1a67ee` |

### Rozbieżność referencyjnego OTB i dostarczonego klienta

Harness: `local-data/procedural-smoke/g0-1098-20261026/`. Skopiowano bez modyfikacji `tools/tibia_data.py` i `tools/test_tibia_data.py`, a kopie DAT/SPR i lokalnego OTB umieszczono w wymaganym przez testy `Tests1098/`. Log: `tibia-data-tests.log` w katalogu harnessu. Dane są lokalne i ignorowane przez Git.

Referencyjny test oczekuje maksymalnego ID DAT 23725 i 23626 itemów; dostarczony DAT ma odpowiednio 26381 i 26282. Dla client ID 930 test oczekuje sprite'ów 5255/5256, a otrzymuje 9833/9834. Oracle hasha sprite'ów nie zgadza się z OTB dla ID 930. To wynik na innym zestawie danych, nie dowód regresji parsera; jednocześnie blokuje uznanie standardowego OTB za zgodne z tym klientem.

W `data/clients.toml` istnieje `1098-BlackTek`, `configType='dat_only'`, `dataDirectory='1098'`. `ItemDefinitionResolver::resolveDatOnly` rozpoczyna od server ID = client ID, następnie stosuje XML. Profil załadowano w RME z walidacją podpisów; potwierdzono zakres smoke `grass`, bez podpinania standardowego OTB. Dodatkowy odczyt: DAT ID 4526 jest przechodnim ground z minimap color 24 i sprite'ami 6592/6593/6705/6709; standardowy OTB wiąże server ID 4526 z client ID 4515. Brush `grass` w `data/1098/brushes/brushes.xml` używa server look ID 4526. Loader zgłosił odwrócone zakresy `Walls` 1205..1202, `Borders` 4514..4225 i 3394..3360 oraz brakujące ID 30001 i dalsze w `Others`. Całość materiałów nie jest zatwierdzona.

### Reprodukcja obsługi błędu callbacka Sol2

Pliki lokalne w tym samym ignorowanym harnessie: `sol_callback_probe.cpp` oraz `run_sol_callback_probe.cmd`. Polecenie z katalogu repo: `rtk proxy cmd /c local-data\procedural-smoke\g0-1098-20261026\run_sol_callback_probe.cmd`. Użyto MSVC x64, `/std:c++latest /EHsc /MD`, rzeczywistego `source/lua/lua_sol_config.h`, nagłówków i `lua.lib` z `build-ninja/vcpkg_installed/x64-windows` oraz odpowiadającego im `lua.dll`.

Probe zawiera `static_assert` równoważności `sol::function` i `sol::protected_function`; uruchamia callback inkrementujący licznik przed `error()`, ignoruje jego wynik w `try/catch`, a następnie wykonuje drugie wywołanie z kontrolą `valid()`. Wynik: `returned_without_throw=true`, `counter_after_ignored_result=1`, `checked_call_failed=true`. To kod 0 **diagnostyki odtwarzającej ryzyko**, nie PASS rollbacku RME. Powiązanie ze źródłem: `LuaAPI::transaction` wywołuje callback bez inspekcji wyniku i potem przechodzi do commit. Pełny G0-07 musi sprawdzić rzeczywiste tile i historię.

Polecenie uruchomionych testów:

```powershell
rtk proxy C:\Users\Sorky\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe tools/test_tibia_data.py -v
```

Powtórzenie kanonicznego buildu z ustawieniami tylko dla bieżącej sesji PowerShell:

```powershell
$env:PATH = 'C:\repo\vcpkg\downloads\tools\ninja-1.13.2-windows;' + $env:PATH
$env:VCPKG_ROOT = 'C:\repo\vcpkg'
rtk proxy cmd /c build_ninja.bat
```

Ścieżkę Ninja zweryfikować na danym komputerze. Nie używać opcji `clean`/`clean-all` bez potrzeby. Zmiana `PATH` służy uruchomieniu istniejącego skryptu, nie zastępuje go alternatywnym buildem.

Po błędzie pamięci wznowiono wygenerowany przez ten skrypt target `rme` w tym samym `build-ninja`, po `vcvarsall.bat x64`, poleceniem `cmake --build build-ninja --target rme --parallel 2`. Lokalny wrapper: `local-data/procedural-smoke/g0-1098-20261026/build_rme_limited.cmd`. Nie zmieniono `build_ninja.bat`, flag kompilacji ani zależności. Pierwsza próba inline przez `cmd /c` nie przeszła cytowania ścieżki `vcvarsall`; zastąpiono ją czytelnym plikiem CMD. Finalny wynik: kod 0, linkowanie `[382/382]`, EXE z 2026-10-26 18:45:53.

### Powtórzenie ręcznego smoke

Skrypty z faktycznie wykonanych prób: [umieszczenie/Undo/Redo](smoke/g0_smoke.lua), [odczyt zawartości](smoke/g0_verify_saved.lua), [błąd callbacka](smoke/g0_error_probe.lua). Skopiować je do `scripts/` obok testowego EXE, przeładować skrypty i uruchamiać ręcznie przyciskiem Run w Script Manager. Nie ustawiać automatycznego uruchamiania. Test wykonuje istniejące bindingi Lua, nie pełny pipeline brush/autoborder.

1. Nowa pusta mapa z klientem `1098-BlackTek`: uruchomić `g0_smoke.lua`, wykonać jedno Undo i uruchomić go ponownie, potem jedno Redo i uruchomić trzeci raz. Wyniki 2026-10-26: PLACE 18:53:46, UNDO 18:54:18, REDO 18:54:33, wszystkie PASS.
2. Zapisać jako `g0-smoke.otbm` w izolowanym katalogu. Zamknąć i otworzyć z ekranu Welcome z jawnym profilem `1098-BlackTek`, następnie uruchomić `g0_verify_saved.lua`. Nie zakładać, że zwykłe File → Open rozpozna wariant klienta.
3. Dopiero po PASS odczytu uruchomić `g0_error_probe.lua`. Dodaje ground w pustym (140,140,7), po czym celowo zgłasza błąd. Obecny build kończy diagnostykę FAIL. Cofnąć raz i ponowić verifier; nie zapisywać wyniku nieudanej transakcji.

Wznowienie 2026-10-27 korzystało z katalogu roboczego harnessu i lokalnego `config.toml` (RME odczytuje go względem cwd):

```toml
[[clients]]
name = '1098-BlackTek'
clientPath = 'C:\repo\ProjectSorky\otclient\data\things\1098\'
default = true
```

OTBM w Welcome: wersja 3, rozmiar 2048x2048, items major/minor 3/57, względne pliki `g0-smoke-house.xml` i `g0-smoke-spawn.xml`. `otbmVersions=[3]` w TOML oznacza wersję 3; loader odejmuje 1, uzyskując `MAP_OTBM_3=2`. Nie mylić numeru formatu z wartością enum.

## Warunki testu integracyjnego G0

### G0-B — naprawa callbacka i regresja, 2026-10-27

Gałąź `fix/lua-transaction-callback`, baza audytu `b2bc871`. Zmiana w
`source/lua/lua_api_app.cpp`: jawny `sol::protected_function`, sprawdzenie wyniku
callbacka i przekazanie błędu do istniejącej ścieżki rollbacku. Dokumentacja Sol2
sprawdzona przez Context7 `/thephd/sol2`; składnia `result.valid()` i ekstrakcja
`sol::error` potwierdzone kompilacją z zainstalowaną wersją.

- Build: `rtk proxy cmd /c local-data\procedural-smoke\g0-1098-20261026\build_rme_limited.cmd`, kod **0**, `[2/2] Linking CXX executable rme.exe`. Po korekcie formatowania ponowiono build końcowego źródła, również kod 0. EXE 8026112 bajtów, 2026-10-27 18:52:13; ten EXE uruchomiono w GUI.
- Regresja: [`lua_tests/test_transaction_rollback.lua`](../../lua_tests/test_transaction_rollback.lua), uruchomiona ręcznie na świeżo otwartym fixture z jawnym `1098-BlackTek` i pustą historią. **PASS**, 18:53:46. Trzy callbacki tworzą po dwa tile i zgłaszają różne błędy. Sprawdzono komunikaty, usunięcie sześciu mutacji, niezmieniony rozmiar/indeks historii, dwie udane transakcje, odrzucenie wartości zwracanych callbacka, dwa Undo, dwa Redo oraz przywrócenie 400 pól. W historii Redo pozostają dwie poprawne akcje testowe.
- Oryginalny `g0_error_probe.lua`: **PASS**, 18:54:08 — `propagated=true`, `tile_present=false`, `tile_count=400`, komunikat `G0 ROLLBACK CONTENT PASS`. To ta sama reprodukcja, która na bazie audytu pozostawiała 401 pól.
- `g0_verify_saved.lua` po obu testach: **PASS**, 18:54:30 — 400 dokładnych ground ID, puste stosy/encje/house ID/flagi i halo.
- Plik OTBM nie został zapisany podczas regresji. SHA-256 przed i po: `2e0728b71bc5506db21a22be8e29920c17d418f599a7fcbf627536eb45f728ce`.

Odczytany log Script Manager:

```text
[18:53:46] ROLLBACK PASS: 3 original errors propagated, 6 tile mutations restored, history unchanged.
[18:53:46] TRANSACTION REGRESSION PASS: successful commits, separate Undo/Redo, no failed entries; fixture restored to 400 tiles.
[18:54:08] G0 ROLLBACK CONTENT PASS; history still requires a separate check.
[18:54:30] G0 SAVED CONTENT PASS: 400 exact ground IDs; empty stacks, entities, house IDs, flags and halo.
```

Test jest celowo poza ogólnymi runnerami Lua, wymaga dedykowanej mapy i uruchomienia
ręcznego. Nie wykonano całej suity Lua/CTest ani testów awarii alokacji, zmiany lub
zamknięcia edytora w callbacku, zagnieżdżonych transakcji czy rollbacku złożonych
metadanych. G0-06 z rzeczywistą ręczną edycją przez `DrawOperations` nadal wymaga
osobnego dowodu; dwie transakcje Lua nie zastępują tego przypadku.

Wymagane: działający `build-ninja/rme.exe`, jawnie wybrany klient z kompletnymi assets, zgodny brush ground oraz mapa syntetyczna. Brak któregoś warunku oznacza niewykonanie testu, nie PASS. Map użytkownika i danych referencyjnych nie modyfikować.

Testy wykonywać w `local-data/procedural-smoke/<run-id>/`, z osobnymi plikami wyjściowymi dla każdej próby. OTBM i wszystkie wskazywane pliki pomocnicze muszą znajdować się w kopii testowej; szczególnie sprawdzić bezwzględne ścieżki do XML, aby zapis nie dotknął oryginałów.

Zapisać: SHA i diff, wersje kompilatora/zależności, klient i podpisy/hashes assets, wersję OTBM, nazwy brushy, ustawienia automagic/grupowania/Undo, współrzędne, log i wynik każdego kroku. Dla testów wydajności także sprzęt, czas i pamięć; G0 nie stawia jeszcze tezy o przyspieszeniu.

Wykonana próba: syntetyczna mapa, profil `1098-BlackTek`, OTBM 3, Z=7, maska X/Y 100–119 i halo 99–120. Brush `grass` wybrany z aktualnych materiałów; ID pochodziło z `lookId` załadowanego brusha. Fixture zawiera wyłącznie ground, bez pięter dodatkowych i złożonych metadanych.

## Protokół G0 — minimalny smoke test

| ID | Czynność | Kryterium PASS |
|---|---|---|
| G0-01 | Uruchom build Windows x64 i następnie program | Kod wyjścia 0, istnieje nowy EXE z tego buildu; otwarcie GUI bez błędu |
| G0-02 | Załaduj wybrany klient i utwórz syntetyczną mapę | Brak nierozwiązanych ID/brakujących assets w testowanym zakresie, zapisany format i wersja klienta |
| G0-03 | Zapisz stan S0 i przygotuj pusty obszar 20x20 na Z=7 z halo | Dokładna maska 400 pól; znany stan otoczenia i metadanych |
| G0-04 | Uruchom minimalną edycję Lua w `app.transaction` na kopii, używając ID zweryfikowanego dla klienta | 400 zamierzonych zmian, brak zmian poza zadeklarowanym zakresem; zanotuj rzeczywistą drogę wywołań |
| G0-05 | Cofnij raz, następnie ponów raz | Undo przywraca S0, Redo przywraca S1 wraz ze stosami itemów i metadanymi, nie uruchamia losowania od nowa |
| G0-06 | Przed i po transakcji wykonaj osobną ręczną edycję przez `DrawOperations` | Historia rozdziela ręczne edycje i transakcję; pojedyncze Undo nie usuwa sąsiedniej edycji |
| G0-07 | W osobnej próbie wywołaj `error()` po kilku mutacjach callbacka transakcji | Pełny rollback, brak częściowego wyniku i wpisu historii, błąd zgłoszony; test szczególnie istotny dla Sol2 z włączonymi safety |
| G0-08 | Zapisz S1 standardowym workflow, zamknij i ponownie otwórz kopię | Zgodność kanonicznej zawartości mapy i kompletu plików pomocniczych, brak utraty UID/AID, domów/spawnów/stref objętych fixture |
| G0-09 | Otwórz wynik w zadeklarowanym odbiorcy formatu, jeśli dostępny | Wynik zapisany osobno dla oryginalnego RME/OTS; brak odbiorcy oznacza niezweryfikowaną zgodność z nim |

Minimalna edycja Lua jest testem istniejącego API, nie zgodą na pomijanie `DrawOperations` w produkcyjnym handlerze generatora. Nie stosować bez sprawdzenia demonstratora `terrain_generator_demo.lua`, bo ma sztywne ID i nie kontroluje wszystkich źródeł RNG.

Porównanie kanoniczne obejmuje współrzędne i obecność tile (brak kontra pusty), ground, uporządkowany stos itemów, atrybuty, zawartość kontenerów, creature/spawn, flagi, house ID, strefy i powiązane metadane. Porównanie samej minimapy, `Tile::isContentEqual` albo rozmiaru pliku nie jest pełnym oraklem. Nie deklarować PASS round-trip, dopóki taki zakres nie został porównany i opisany.

## Testy do dodania wraz z G1

| Warstwa | Wymagane przypadki |
|---|---|
| Rdzeń bez GUI | Stałe seedy, stabilna kolejność, niepoprawne rozmiary/Z, przepełnienia, budżet pamięci, nieregularna maska, ochrona |
| Resolver/sandbox | Poprawne ID, deterministyczność konkretnego diffu, border przy brzegu maski, read-only halo, footprint przecinający ochronę, brak mutacji oryginału |
| Commit/historia | Zmiana wejścia po Preview, zamknięcie mapy, przełączenie karty/assets, błąd zastosowania, brak scalenia historii, pełne Undo/Redo, blokada live |
| Zadania w tle | Cancel bez zmian mapy/historii, odrzucenie starego wyniku, zamknięcie odbiorcy zdarzeń, ograniczenie liczby wariantów i prób naprawy |
| E2E | 100x100 i 500x500, Preview zgodny z Commit, komplet plików round-trip, metadane i otoczenie zachowane |

Nie zaliczać symulacji w Pythonie jako testu rzeczywistego C++ borderowania lub renderera. `tests/test_ground_border_ordering.py` zawiera model/symulację; do G1 wymagane są dowody na kodzie RME.

Istniejący CMake rejestruje `test_universal_rotator` w CTest; skrypt `build_ninja.bat` buduje tylko `rme`. Po zakończeniu konfiguracji trzeba osobno zbudować i uruchomić wymagany target. Obecność pozostałych `tests/*.cpp` na dysku nie dowodzi ich rejestracji w CTest. Skrypty `lua_tests/` wymagają kontekstu RME, ich README nie zastępuje logu wykonania.

## Stan bramki

G0 pozostaje **IN PROGRESS**. Konkretny bloker ignorowanego błędu callbacka G0-07 naprawiono i zweryfikowano dla tworzenia tile oraz historii. Build, edycja 20x20, Undo/Redo i odczyt ground-only fixture przeszły. G0-06 (rozdzielenie ręcznych edycji), multi-floor i round-trip UID/AID/domów/spawnów/stref oraz odbiorca zewnętrzny G0-09 nadal niezweryfikowane. Przed G1 dokończyć wymagany protokół i decyzje adaptera, bez rozszerzania wyniku tej regresji na pełną atomowość generatora.
