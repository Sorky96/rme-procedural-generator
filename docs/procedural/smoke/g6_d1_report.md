# D1 — ostrzeżenie SetFocus przy zamykaniu kart i programu

2026-11-19. Kryteria: [G6_METRICS.md](../G6_METRICS.md), sekcja „D1 i D4”
(commit `04b03de`). Wynik: **przypadek odtworzony, poprawiony, sprawdzony 2/2 przebiegami**.

## Przyczyna

`EditorManager::CloseCurrentEditor` usuwał kartę wewnątrz `ScopedInfoBar("Closing map...")`.
Okno „Closing” tworzy `wxWindowDisabler`, więc ramka jest wyłączona. W wxWidgets 3.3.1
`wxAuiNotebook::DoRemovePage` po usunięciu strony wywołuje `SetSelectionToWindow` →
`DoModifySelection`, które ustawia fokus na pozostałej stronie (`auibook.cpp:4220`).
Win32 `SetFocus` w oknie z wyłączonym przodkiem kończy się błędem 87 (0x57).
Dotyczy to każdego zamknięcia bieżącej karty, gdy zostaje inna karta: File → Exit,
File → Close i przycisk zamknięcia karty. Nie tylko wyjścia z programu.

## Poprawka

`MapTabbook::DetachTab` usuwa stronę z notebooka bez jej niszczenia, a
`MapTabbook::DestroyDetachedTab` ją niszczy. `CloseCurrentEditor` odłącza stronę przed
pokazaniem okna „Closing”, czyli przy włączonej ramce. Zwolnienie mapy i zasobów GL
(kosztowna część) nadal odbywa się pod tym oknem. `DeleteTab` składa obie operacje,
więc `CloseAllEditors` i `CloseLiveEditors` działają jak dotąd (bez okna „Closing”).
Kolejność palet i zdarzeń zmiany strony jest taka sama jak w `DeletePage`. Różnica:
`DestroyDetachedTab` usuwa też wpis z mapy `conv`, który wcześniej zostawał.

## Sonda

- `g6_d1_close_probe.cpp`: prywatny moduł wx z normalnym wejściem aplikacji, budowany przez
  `g1_build_input_probe.py --application-entry --symbols`, z `ProceduralFocusTrace`.
- `g6_d1_run_close_probe.py OUTPUT PROFILE_DATA CONFIG [--case …]`: każdy przypadek w nowym
  procesie i kopii profilu. PASS wymaga kodu 0, markerów w kolejności, niezmienionych
  assets i pustego stderr.

Przypadki (karta startowa jest pierwszą kartą):

| Przypadek | Przebieg |
|---|---|
| `exit` | 2 czyste karty, File → Exit |
| `cancel` | pierwsza karta brudna (1 pole), bieżąca czysta; File → Exit zamyka czystą, na „Save changes” Cancel; sprawdzenie, że została brudna karta z 1 polem; odrzucenie zmian i ponowne Exit |
| `tab-close` | 3 karty; File → Close; zdarzenie `PAGE_CLOSE` bieżącej karty (ta sama obsługa co przycisk); File → Exit |

Klient 10.98 (`1098-BlackTek`) z profilu `g0-reference-profile-20261030`. Start programu
ładuje klienta i pokazuje modalną listę 22 ostrzeżeń tilesetów (`missing_id`,
`inverted_range`). W starych przebiegach zamykał ją operator. Sonda zapisuje jej treść
i zamyka ją przez `EndModal(wxID_OK)`.

## Wyniki

| Przebieg | EXE `rme.exe` | exit | cancel | tab-close |
|---|---|---|---|---|
| przed poprawką (`04b03de`) | `80cba5b9…` | FOCUS_WARNING, 1 błąd | FOCUS_WARNING, 1 błąd | FOCUS_WARNING, 2 błędy |
| po poprawce, przebieg 1 | `fb12d483…` | PASS | PASS | PASS |
| po poprawce, przebieg 2 | `fb12d483…` | PASS | PASS | PASS |

Przed poprawką każdy błąd ma stos `MainFrame::OnExit` lub obsługę menu/karty →
`DoQuerySave` → `CloseCurrentEditor` → `MapTabbook::DeleteTab` → `wxAuiNotebook::DoRemovePage`
→ `DoModifySelection` → `SetFocus`. Stan okien w chwili błędu: ramka `wx_enabled=0`,
widoczne okno „Closing”. To ten sam stos co w [G1_FOCUS_TRACE.md](../G1_FOCUS_TRACE.md).
Każdy błąd wx wypisuje dwukrotnie do stderr. We wszystkich przebiegach kod wyjścia
wynosił 0, a markery były kompletne. Po poprawce stderr i `focus-trace.log` są puste.
Anulowanie zachowuje proces, brudną kartę, jej pole i stan `ShouldSave`.

Odrzucone próby przygotowawcze zachowano prywatnie. W pierwszej sonda czekała na
startowy dialog ostrzeżeń, a przypadki przekroczyły limit czasu. W drugiej sonda
startowała w pętli tego dialogu, przed utworzeniem karty startowej. Dało to 3 lub 4
karty zamiast 2 lub 3, ale ostrzeżenie odtworzyło się tak samo.

## Granice

- Polecenia są zdarzeniami `wxEVT_MENU` i `PAGE_CLOSE` w kolejce produkcyjnych handlerów,
  a nie kliknięciami myszą. Cancel to `WM_CLOSE` na natywnym oknie „Save changes”, które
  zwraca IDCANCEL, a nie kliknięcie przycisku.
- Mapy są puste i syntetyczne. Nie mierzono czasu zamykania dużej mapy ani nie oglądano
  okna „Closing”. Nie sprawdzono wizualnie, która kontrolka ma fokus po zamknięciu.
- Nie badano innych źródeł SetFocus: D2 (stos aktywacji przy generatorze) i ostrzeżeń
  `LoadVersion` przy wyłączonej ramce w próbniku odmów.
- Nie uruchomiono testów CTest, zestawu G5 ani testów Lua. Regresję wykonuje koordynator.
