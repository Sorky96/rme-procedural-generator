# G1 — natywny dialog generatora terenu

Aktualizacja 2026-11-03: [G1_DIALOG_MATRIX.md](G1_DIALOG_MATRIX.md) dodaje GUI 500×500, maskę z otworami i wybrane odmowy na tym samym kodzie produktu. Poniższy raport zachowuje zakres pierwszego testu 100×100.

2026-11-03: **PASS_G1_NATIVE_DIALOG_100_ONLY**. Pełne **G1 IN PROGRESS**. G2 nierozpoczęte.

Menu **Map → Generate terrain…** otwiera modalny dialog: obszar/prostokąt lub bieżące zaznaczenie, współrzędne, seed, trzy wagi i nazwy brushy. Preview przygotowuje jeden własny wynik, Validate ponownie sprawdza jego źródło, a Place on map przekazuje konkretny diff do istniejącego DrawOperations i jednej akcji Undo. Cancel porzuca wynik. Każda zmiana wejścia natychmiast usuwa stary wynik i wyłącza Place/Validate. Nie zmieniono rdzenia generacji, commitu ani historii.

## Rzeczywisty zrzut

![Zwalidowany podgląd 100×100, seed 1729](screenshots/g1-dialog-20261103.jpg)

To **minimapa konkretnych kafli przygotowanego wyniku**, z kolorami istniejącego TileColorCalculator. Nie jest to renderer sprite’ów. Dopuszcza ją minimalny preview z §5.4 planu projektu. Nie wykonuje odczytu aktywnej mapy ani klienta podczas paint; bitmapa powstaje z własnego preview i snapshotu. Zrzut pochodzi z rzeczywistego natywnego okna prywatnej aplikacji testowej, po Validate. Zapisano oryginalny JPEG zwrócony przez computer-use bez przeróbek.

[Zrzut po zmniejszeniu do minimalnego rozmiaru](screenshots/g1-dialog-minimum-20261103.jpg). W sprawdzonym środowisku objaśnienia, klient, status i wszystkie przyciski są widoczne; minimapa przeskalowuje się po resize. Naprawiono kolejność tworzenia etykiet, ich nazwy w drzewie UIA oraz kolizje mnemoników. Sprawdzono Alt+S i Alt+P. Nie testowano innego DPI ani pełnego czytnika ekranu.

## Niezależna weryfikacja Codex

Kanoniczne `cmd /c build_ninja.bat`: **exit 0**, log `local-data/procedural-smoke/g1-dialog-final-canonical-20261103.log`.

Prywatny normal-entry probe `g1-dialog-final-rme-20261103`: **exit 0**, normalne zamknięcie, pusty stderr, niezmienione curated assets i źródła. Test otwiera wyłącznie syntetyczne mapy w pamięci. Operator computer-use wykonał prawdziwe polecenia menu i przycisków; obserwator C++ nie zastępuje tych poleceń bezpośrednimi wywołaniami handlerów.

| Sprawdzenie | Wynik |
|---|---|
| Błędny seed `not-a-seed` → Preview | Czytelny błąd, brak zmiany źródła/historii |
| Seed 1729, obszar 100×100, wagi 3/2/1 | Preview gotowy, 10 000 zmian; mapa, revision i historia bez zmian |
| Validate | Ponowna kontrola wyniku i aktualnego źródła; mapa bez zmian |
| Zmiana seedu na 1728 | Wynik usunięty; Validate/Place wyłączone |
| Powrót do 1729, Preview, kliknięcie Cancel | Źródło i historia bez zmian |
| Ponowne otwarcie, Preview, Place | Jeden wpis historii; każdy wynikowy kafel zgodny z niezależnie przygotowanym preview |
| Prawdziwe Ctrl+Z | Jeden Undo usuwa generację i odtwarza źródłowy kontekst |
| Prawdziwe Edit → Redo | Wynik ponownie zgodny kafel po kaflu; chroniony sentinel zachowany |

Log zawiera `DIALOG_INVALID_SEED_PASS`, `DIALOG_PREVIEW_PASS`, `DIALOG_VALIDATE_PASS`, `DIALOG_INPUT_INVALIDATION_PASS`, `DIALOG_CANCEL_PASS`, `DIALOG_GUI_COMMIT_PASS`, `DIALOG_GUI_UNDO_PASS`, `DIALOG_GUI_REDO_PASS`, `DIALOG_PASS`. Fingerprint niezależnego wyniku domyślnego: `6839872153377310206`.

`g1_verify_dialog_probe.py` ponownie sprawdził hashe **421 obiektów RME**, 421 wejść kompilacji (w tym PCH), modułu obserwatora, obu EXE, wejść dialogu i curated assets. Od poprzedniego zaliczonego core probe zmieniły się tylko dwa wcześniejsze CPP menu; nowy CPP dialogu dochodzi osobno. Z wcześniejszych rozszerzonych wejść dowodu core zmienił się wyłącznie CMake dodający dialog. Starszy probe 100/500 i fault injection pozostaje dowodem historycznym na wcześniejszym EXE; nie oznaczono go jako ponownie uruchomionego testu bieżącego GUI.

Normalny EXE SHA256: `f4f7332a79b3b21058fe5ae9eabb408480902f48a3a6b689ba2352804423940e`.

Probe SHA256: `10f221a93452f8e9127248d5e6e764de0ce9132ab577599c09257642f1d67945`.

Końcowy `git diff --check` przy normalnej konfiguracji repo: exit 0. Nowe źródła/testy/raport sprawdzono osobno pod kątem trailing whitespace i znaków zastępczych. Integracja pozostaje na `feature/procedural-g1-core` w `C:/repo/remeres-map-editor-redux-master`; współdzielony worktree Claude zachowano.

Zrzut główny SHA256: `d7631a71bbc129e4638647d8ad689ce9c11f4bdfcf1e1c9070ff64cd53ed8bd0`.

W linkowaniu prywatnego probe zapisano dwa ostrzeżenia MSVC C4723 w nagłówku wxWidgets `wx/msw/gdiimage.h:117–118` (potential divide by zero). Nie edytowano biblioteki; runtime i zmiana rozmiaru w tej próbie przeszły. Nie jest to dowód braku problemów we wszystkich ścieżkach wx/GDI.

## Claude i integracja

Claude implementował tylko dwa nowe pliki `source/ui/dialogs/terrain_generator_dialog.h/.cpp` w dedykowanym worktree, run `a064a4f3-aa6f-4e2c-9293-49ce9186bc52`. Pełny tracked diff workera pozostał identyczny z poprzednim zaakceptowanym stanem; jedynym dodatkiem były te dwa przeczytane pliki. Codex wykonał integrację menu, testy oraz poprawki po review: std::format/wxstr, wyłączenie Place dla pustego diffu, pełny układ, etykiety/mnemoniki, repaint i ponowne wrap statusu oraz obsługę bad_alloc konwersji wejść także w Validate/Place.

Read-only review Claude: `acedd784-5875-4970-ae8d-cf5afdb1a4a1`; bez zmian checkoutu. Oba report/stdout: completed/exit 0, bez permission denials, pusty stderr. Claude nie kompilował i nie wykonywał testów. Poprawki po jego review sprawdził Codex; nie są deklarowane jako ponownie ocenione przez Claude.

Pierwszy build `g1-dialog-canonical-20261103.log` przeszedł, ale inspekcja `g1-dialog-rme-20261103` ujawniła przycięte objaśnienie/klienta. Przerwano scenariusz przyciskiem Cancel przed wykonaniem wymaganych etapów; obserwator świadomie zakończył się exit 8 / `First dialog UI sequence incomplete`. Zachowano cały przebieg jako **FAIL/niepełny**, bez zaliczenia. Po poprawkach wykonano nowy pełny build i osobny pełny przebieg opisany wyżej. Nie restartowano zakończonych audytów G0.

## Odtworzenie

Po kanonicznym buildzie, z nowym katalogiem NEW_RUN, uruchomić przez `rtk proxy` Python z runtime Codex:

```text
<PY> docs/procedural/smoke/g1_run_dialog_probe.py local-data/procedural-smoke/NEW_RUN local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml
<PY> docs/procedural/smoke/g1_verify_dialog_probe.py local-data/procedural-smoke/NEW_RUN
```

`<PY>` = `C:/Users/Sorky/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe -X utf8`. Między poleceniami trzeba rzeczywiście wykonać scenariusz UI z tabeli. Informacyjny startup warning prywatnych assets zamknięto w tej samej instancji. Timeout pozostawia tę samą instancję do zbadania.

## Pozostały zakres G1

Nie zaliczono GUI zaznaczenia/odmów/awarii, GUI 500×500, widoku sprite’ów ani generatorowego GUI save/reopen OTBM ze wszystkimi companionami. Generowanie jest synchroniczne bez yield: Cancel działa między operacjami i po przygotowaniu, nie przerywa trwającego generowania. Wciąż potrzebne są responsywne planowanie/anulowanie oraz pomiary całej aplikacji/RSS i współistniejących wyników.

Produkt zachowuje najwyżej jeden własny wynik, zwalnia poprzedni przed kolejnym Preview i używa maksymalnie 500×500. Limity 512 MiB przygotowania i 768 MiB przejścia/Undo są estymatami polityki rdzenia. Bitmapa ma osobny ograniczony allowance `7*width*height+64 KiB`, maska do 1 MiB; GUI, C/DLL/GPU, źródło, assets i pozostała historia nie są twardo objęte tymi limitami. Wyświetlana estymata preview obejmuje bitmapę i maskę, nie jest pomiarem RSS.

Następne kroki: (1) GUI zaznaczenie/odmowy i 500×500, (2) responsywność/anulowanie i RAM, (3) pełny generatorowy GUI/OTBM round-trip oraz końcowa macierz. Wspólne niezacommitowane zmiany zachowano; bez commit/push. Nie otwierano forgotten.otbm/companionów, map ani świata użytkownika; bez ingerencji w Docker i bazę.
