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
