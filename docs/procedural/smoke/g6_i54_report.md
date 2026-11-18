# I54 — lokalny feedback i historia wersji

Prerejestracja: `e9ca540`, sekcja I54 w `G6_METRICS.md`. Raport wykonawcy;
nie zamyka G6 ani nie zastępuje niezależnego przeglądu i regresji integratora.

## Implementacja

Surface udostępnia Approve / Reject / Adjust dla wybranego, przygotowanego
kandydata. Wybór w galerii nie zapisuje ocen. Approve i Adjust wymagają
dopuszczalnego wyniku; Reject działa również dla hardFail. Handler sprawdza
aktualność mapy, zasobów, ustawień i odcisków przygotowanego kandydata także
przy wymuszonym wywołaniu. Notatka ma limit 1024 **bajtów UTF-8**, z widocznym
błędem przy zapisie i zachowaniem tekstu do poprawy.

`procedural/styles/history.json` w lokalnym katalogu profili użytkownika
zawiera format `g6-style-history/1`, oceny, wersje i aktywną wersję. Wpis ma
losowy identyfikator SHA-256, klienta i hashe jego zasobów, odcisk bazowego
profilu, lokalną wersję lub ustawienia wbudowane, politykę rankingu, indeks
kandydata, seed użytkownika/początkowy/rzeczywisty oraz pięć odcisków pięter.
Zapisuje pełne ustawienia Surface i dekoracji użyte przy generacji. Nie
zbiera ścieżek/nazw map, współrzędnych ani zawartości kafli. Notatka jest
jawnym tekstem wpisanym przez użytkownika, nie automatyczną telemetrią.

Adjust zapisuje nową migawkę z rodzicem i **wyłącznie** następującą poprawką
stylu: pathLength, buildingDensity, ruinPercent, campCount, fordLength,
rockPasses, shoreSmoothing (0 lub 2) i repairReach. Zakres jest wypisany
w GUI. Nie zapisuje edytowanych pędzli, geometrii gór/jaskiń ani pozostałych
kontrolek. Bazowe strojenie, pochodzenie pasm, ograniczenia klienta, paleta,
statystyki i dekoracje profilu pozostają w zachowanej migawce. Pliki
wejściowe `*.profile` pozostają tylko do odczytu; ich format i parser nie
zmieniły się. Brak profilu wyłącza Adjust, ale pozwala oceniać wynik.

Pierwszy Adjust zachowuje także bazową wersję. Jawne Activate / rollback
wybiera zgodną wersję i unieważnia serię. Kolejny Adjust wskazuje tę wersję
jako rodzica, zachowując młodsze gałęzie. Ponowne otwarcie okna/aplikacji
wczytuje oceny, historię i ostatnią aktywowaną lokalną wersję. Bazowa wersja
bez poprawki zachowuje zachowanie dawnego profilu i domyślne kontrolki GUI.

Odczyt ma limity: 8 MiB, 4096 ocen, 256 wersji, głębokość JSON 16; sprawdza
typy, zakresy, UTF-8, rodziców, zgodność migawek i aktywną wersję. Nieznany
format lub uszkodzenie blokuje zapis, bez resetowania pliku. Zapis sprawdza
bajtową migawkę wczytanego pliku pod blokadą katalogową, opróżnia bufor
pliku tymczasowego i atomowo podmienia plik przez Windows MoveFileExW.
Konflikt i błąd zapisu zachowują wcześniejsze dane. Blokada po awarii nie
jest automatycznie usuwana; użytkownik otrzymuje błąd zamiast nadpisania.
Historia osiągająca limit jest odrzucana bez usuwania wcześniejszych ocen.

Kontrolki korzystają z wxWidgets, fontu i kolorów Theme oraz Theme::Grid.
Mają nazwy dostępności, widoczne etykiety i natywną obsługę klawiatury.
Feedback i historia nie wywołują operacji na mapie ani kolejce Undo.
Generator, mountain, ranking A, blokady I52/I53, format map i pojedynczy
Action dla Place pozostają bez zmian. Nie dodano item IDs, synchronizacji,
uczenia ani automatycznej zmiany rankingu.

## Walidacja wykonawcy

- Windows x64 Release, `build_ninja.bat`: **PASS**. Użyto istniejącego
  `vcpkg_installed` głównego checkoutu, z `VCPKG_MANIFEST_INSTALL=OFF`, oraz
  Ninja 1.13.2 wskazanego w zleceniu. Log: `scratchpad/i54-validation.log`.
- `g5_run_suite.py` bez `--probes`: **22/22, SUITE_PASS**. To wynik rdzenia,
  nie pełna bramka z sondami Commit/Surface. Test Surface potwierdza
  `1729`, `100×100 = 17159159863505677800`.
- CTest dla store'u, profilu, oferty, Surface, rankingu kandydatów, serii
  i copy guard: **7/7 PASS**.
- Prywatne `g1_run_resolver_probe.py ... --series`: **PASS**, exit 0,
  `input_assets_unchanged=true`, `CANDIDATE_DIALOG_PASS`. Wynik i manifest
  źródeł: `scratchpad/i54-client/verification.json` i `build-manifest.json`.
  Wyniki I54 mają markery `I54_BUILTIN_FEEDBACK_NO_BASE_ADJUST_DISABLED_PASS`
  oraz `I54_SELECTED_FEEDBACK_UTF8_ADJUST_ROLLBACK_BRANCH_REOPEN_STALE_NO_MUTATION_PASS`.
  Sonda sprawdziła również odmowę Approve/Adjust i zapis Reject dla hardFail.
  Wynik M06 po historii miał wybranego kandydata #2 i zachował pojedyncze
  Place/Undo/Redo. Przeszły dotychczasowe przypadki N=1/4/16, oba profile,
  anulowanie, aktualność i sześć przypadków I53.
- `git diff --check`: **PASS**. Nie wykonywano pomiaru ani twierdzeń
  o poprawie wydajności lub jakości rankingu.

Log linkera sondy zawiera znane ostrzeżenia wxWidgets C4723 w zewnętrznym
`gdiimage.h` oraz komunikat środowiska vcvars o `vswhere.exe`; kompilacja
i linkowanie zakończyły się kodem 0. Na stderr uruchomionej sondy były
wyłącznie tolerowane przez istniejący runner komunikaty wx o SetFocus.

Test `test_procedural_style_history` obejmuje wszystkie akcje, dokładny
powrót uint64 i UTF-8, limit notatki, niedozwolony UTF-8, brak bazy,
hardFail, zgodność klienta/zasobów, restart, rollback i gałąź, konflikt
dwóch store'ów, zewnętrzną zmianę pliku, blokadę, błąd utworzenia pliku
tymczasowego, uszkodzony/obcy format, błędne liczby i drzewo oraz limit
rozmiaru. Testy profili kontrolują dotychczasowy odczyt i ofertę profili.

Rozszerzenie prywatnej sondy `--series` korzysta z rzeczywistego klienta,
produkcyjnych kontrolek/handlerów i ActionQueue, na syntetycznych mapach.
Sprawdza ocenę innego kandydata, notatkę, Adjust, rollback, gałąź,
ponowne otwarcie okna, odmowę starego wyniku i hardFail oraz Place → jedno
Undo → Redo z porównaniem do niezależnie przygotowanego wyniku.

Prywatne historie, profile testowe, konfiguracja klienta i logi są wyłącznie
w `scratchpad/`; nie należą do commitu. Nie instalowano zależności,
nie zmieniano `C:/repo/vcpkg/buildtrees` i nie wykonywano push.

## Kroki ręcznego odbioru GUI

1. W prywatnym profilu aplikacji załaduj zgodnego klienta i bazowy profil
   `*.profile`. Otwórz nową mapę i generator Natural surface; wybierz profil.
2. Wygeneruj czterech kandydatów. Strzałkami wybierz innego niż rekomendowany.
   Sprawdź etykietę Selected, seed i stan walidacji.
3. Wpisz notatkę z polskimi znakami, kliknij Approve, potem Reject. Sprawdź
   komunikaty i brak zmian mapy/Undo. Przekrocz 1024 bajty; zapis ma odmówić
   i pozostawić tekst do poprawy.
4. Zmień obsługiwany parametr stylu. Stary podgląd ma utracić możliwość
   oceny. Wygeneruj nowy dopuszczalny wynik, sprawdź wypisany zakres Adjust
   i kliknij Adjust. Powinna powstać lokalna wersja, a seria zniknąć.
5. W historii wybierz bazę i kliknij Activate / rollback. Wygeneruj nowy
   wynik i zrób Adjust. Sprawdź rodzica nowej gałęzi oraz obecność poprzedniej.
6. Zamknij i ponownie otwórz aplikację z tym samym klientem. Sprawdź
   zachowanie ocen, gałęzi i aktywnej lokalnej wersji. Inny klient lub inne
   zasoby nie mogą oferować tych lokalnych wersji.
7. Otwórz dwa okna ze wspólnym snapshotem historii. Zapisz w pierwszym,
   następnie w drugim: drugie ma zgłosić konflikt bez utraty pierwszego zapisu.
8. Wygeneruj dopuszczalny wynik i wykonaj Place, Undo, Redo. Place ma mieć
   jeden wpis Undo. Sprawdź stan hardFail: Reject dostępny, Approve/Adjust
   i Place niedostępne. Sprawdź fokus i tabulację oraz jasny/ciemny Theme.

Ręczny odbiór wizualny/klawiatury i ponowne uruchomienie całego procesu
przez człowieka pozostają niewykonane; automatyczna sonda ponownie otwiera
store i okno. Ślepy test jakości I55 pozostaje osobnym zadaniem. Integrator
wykonuje własny build, pełny SUITE_PASS z sondami, G5 `--i47` (m06=0,
m19=1), aktualizuje `G6_GATE_STATUS` i usuwa build-ninja tego worktree.
