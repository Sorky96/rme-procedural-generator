# G1 — diagnostyka ostrzeżenia SetFocus

2026-11-04. W przebiegu `g1-focus-chain-rme-20261104` zapisano dwa stosy wywołań dla błędu Win32 `0x00000057` na głównym wątku.

| Zdarzenie | Ścieżka do błędnego wywołania |
|---|---|
| Aktywacja okna przy otwartym generatorze | `MainMenuBar::OnProceduralTerrain` → `wxDialog::ShowModal` → pętla zdarzeń → `wxTopLevelWindowMSW::OnActivate` → `DoRestoreLastFocus` → `wxSetFocusToChild` → `wxWindow::SetFocus` |
| Zamykanie aplikacji z dwiema kartami | `MainFrame::OnExit` → `DoQuerySave` → `EditorManager::CloseCurrentEditor` → `MapTabbook::DeleteTab` → `wxAuiNotebook::DoRemovePage` → `DoModifySelection` → `wxSetFocusToChild` → `wxWindow::SetFocus` |

Pierwszy stos pochodzi z przywracania fokusu po aktywacji, drugi z wyboru pozostałej karty przy usuwaniu aktywnej. W obu przypadkach samo wywołanie `SetFocus` pochodzi z wxWidgets. Stosy nie zawierają obsługi kroków przygotowania Preview. Nie identyfikują wadliwego HWND ani przyczyny wcześniejszego ostrzeżenia przy round-trip.

Obserwator zapisał wszystkie dziewięć markerów unieważnienia, w tym poprawne ponowienie i nowy dialog. Runner przekroczył limit oczekiwania przed dokończeniem sekwencji w GUI. Po wznowieniu proces zamknięto ręcznie, a log aplikacji pokazuje zamknięcie kart i zwolnienie zasobów. Runner nie zapisał kodu wyjścia ani końcowego manifestu wykonania. Próbę zaliczono jako diagnostykę stosów. Dotychczasowe wyniki funkcjonalne pozostają częściowe. Interposer zapisuje każdy stos raz, natomiast łańcuch logowania wypisał każdy komunikat dwukrotnie do stderr.

Surowy dowód jest w `local-data/procedural-smoke/g1-focus-chain-rme-20261104/run/focus-trace.log`. Kod produkcyjny i vendored wxWidgets pozostały bez zmian. Pierwsza próba builda zatrzymała się na niedostępnej deklaracji SDK, a kolejną prywatną instancję wyłączono po zastąpieniu jej właściwym wariantem loggera.

Próba korzysta z prywatnej instancji RME i syntetycznej mapy używanej w [teście unieważniania Preview](G1_INVALIDATION.md). Opcjonalny obserwator nasłuchuje logu wxWidgets, zapisuje stos wywołań ostrzeżenia `SetFocus` w `run/focus-trace.log` i przekazuje komunikat dalej. Linker tworzy osobny PDB dla binarium testowego. DbgHelp używa symboli lokalnych z tego katalogu i z zainstalowanych bibliotek wxWidgets.

```text
rtk proxy <PY> docs/procedural/smoke/g1_run_dialog_probe.py local-data/procedural-smoke/NEW_RUN local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --invalidation --focus-trace
```

`<PY>` to `C:/Users/Sorky/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe -X utf8`.

## Stan okien przy błędzie w przebiegu odmów (2026-11-06)

Win32 `SetFocus` kończy się błędem 87 (`0x57`), gdy okno docelowe albo jego przodek jest wyłączony. Sprawdzono to osobnym testem: wyłączone okno najwyższego poziomu, wyłączony panel nadrzędny i wyłączone okno docelowe dają 87, ukryty panel lub ukryte okno nie dają błędu, a zniszczone okno daje 1400.

`ProceduralFocusTrace` zapisuje teraz przy błędzie także stan okien (foreground, active, focus i jego przodkowie, wszystkie okna najwyższego poziomu). Przebieg `g1-focus-state-b-20261106` (`--refusals --focus-trace`, operator: menu i dialog) dał sześć markerów `REFUSAL_CASE_PASS`, `REFUSAL_PASS` i jedno zapisane ostrzeżenie `SetFocus` (stderr wypisuje je dwukrotnie przez łańcuch logowania). W chwili błędu:

| Okno | Stan |
|---|---|
| główna ramka (`wxFrame`) | `wx_enabled=0`, widoczna |
| `Generate Terrain` | `modal=1`, włączone, foreground i active |
| fokus | przycisk w dialogu generatora |

Stos wskazuje źródło: `LoadVersion` → `LayoutManager::LoadPerspective` → `PaletteManager::CreatePalette` → `GUI::SelectBrushInternal` → `wxWindow::SetFocus`, wywołane z zegara próbnika podczas `ShowModal` dialogu generatora. To krok 4 próbnika odmów (`loadClient()` po `UnloadVersion`), który ładuje klienta przy wyłączonej ramce. Ostrzeżenie w tym przebiegu pochodzi więc z kroku próbnika, a nie z obsługi Preview, Validate czy Place. W normalnej pracy klient nie ładuje się, gdy otwarty jest modalny dialog generatora.

Czego to **nie** pokazuje: dwa wcześniejsze stosy (aktywacja ramki przy otwartym generatorze oraz usuwanie karty przy zamykaniu) nie mają zapisanego stanu okien, więc dla nich wyłączone okno jest hipotezą zgodną z testem Win32, nie ustaleniem. Nie sprawdzono, czy bez kroku 4 przebieg odmów jest wolny od ostrzeżenia, ani czy fokus wraca poprawnie po zamknięciu dialogu. Runner zakończył się kodem 1 z powodu niepustego stderr, tak jak w poprzednich przebiegach odmów.

```text
rtk proxy <PY> docs/procedural/smoke/g1_run_dialog_probe.py local-data/procedural-smoke/NEW_RUN local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --refusals --focus-trace
```

## Zamykanie aplikacji i kod wyjścia próbników (2026-11-06)

Przebieg `g1-focus-rt-20261106` (`--roundtrip --focus-trace`, 100×100) zakończył się `ROUNDTRIP_PASS` i jednym ostrzeżeniem `SetFocus` (stderr wypisuje je dwukrotnie przez łańcuch logowania). Stos: File → Exit → `MainFrame::DoQuerySave` → `EditorManager::CloseCurrentEditor` → `MapTabbook::DeleteTab` → `wxAuiNotebook::DoRemovePage` → `DoModifySelection` → `wxWindow::SetFocus`. Stan okien w tej chwili: główna ramka wyłączona (`wx_enabled=0`), widoczne okno „Closing” (włączone, foreground, niemodalne w sensie `IsModal`). Zgodnie z testem Win32 z sekcji powyżej (cel lub przodek wyłączony daje błąd 87) jest to ten sam mechanizm: wx wybiera kolejną kartę po usunięciu aktywnej i ustawia fokus w ramce, którą okno „Closing” wyłączyło na czas zamykania. Aplikacja się kończy, więc fokus nie ma tu skutku; nie zbadano, czy poza zamykaniem zachowuje się inaczej.

Przebieg powtórzono z dodatkowym zapisem stanu okien w `g1-focus-rt-c-20261106`: pełny `ROUNDTRIP_PASS`, to samo jedno ostrzeżenie z tym samym stosem.

**Kod wyjścia próbników ze śladem.** Dwa przebiegi ze śladem (`g1-focus-state-b-20261106` i `g1-focus-rt-20261106`) kończyły się kodem `0xC0000374` (uszkodzenie sterty) po końcowym markerze, a przebiegi bez śladu kodem 0. Przyczyna po lekturze kodu wx: `ProceduralFocusTrace` jest aktywnym celem logowania, a wx przy zamykaniu wykonuje `delete wxLog::SetActiveTarget(nullptr)` (`init.cpp`), podczas gdy próbnik usuwał ten sam obiekt przez `unique_ptr` (podwójne usunięcie). Próbniki zwalniają teraz własność (`trace.release()`). Przebieg `g1-focus-rt-c-20261106` po tej zmianie ma pełny `ROUNDTRIP_PASS` i kod wyjścia 0. Wniosek opiera się na dwóch przebiegach z uszkodzeniem i jednym bez po zmianie; to był błąd próbnika diagnostycznego, produkcyjny EXE nie zawiera tego kodu. Przebiegi z kodem `0xC0000374` zachowano bez zaliczenia.

Niewyjaśniony pozostaje stos z 2026-11-04 (aktywacja ramki przy otwartym generatorze w przebiegu unieważniania): nie ma dla niego zapisanego stanu okien, a dzisiejsze przebiegi go nie odtworzyły.
