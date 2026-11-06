# G1 — komunikat Undo i prezentacja zaznaczenia

Aktualizacja 2026-11-03: [kolejny krok — pomiar pamięci i blokowania GUI](G1_RESOURCES.md). Kod produkcyjny tego raportu zachował się bez zmian; responsywne anulowanie nadal otwarte.

2026-11-03: poprawka UX zakończona i sprawdzona w natywnym Windows GUI. Pełne **G1 IN PROGRESS**; G2 nierozpoczęte. Codex wykonał ten krok **bez nowego uruchomienia Claude**.

## Efekt

Odmowa historii pokazuje aktualne limity pamięci/liczby akcji, proponuje mniejszy obszar albo sprawdzenie **File → Preferences → General** i dopiero potem nowe Preview. Reguły przyjmowania akcji i limity produktu nie zmieniły się.

![Odmowa przy 40 MiB i wskazówki postępowania](screenshots/g1-dialog-ux-history-20261103.jpg)

W trybie Current selection pola prostokąta i ich etykiety są ukryte. Powrót do Empty rectangle zachowuje wcześniejsze wartości. Nad minimapą pojawia się rzeczywisty zakres przechwycony przez Preview, pozycja i liczba wybranych pól. Po zwolnieniu preview opis ponownie prosi o przechwycenie obszaru.

![Wycentrowane pięć pól w obszarze 4×4](screenshots/g1-dialog-ux-selection-20261103.jpg)

Minimapa korzysta z `wxMemoryDC::SelectObjectAsSource` i `StretchBlit` z jawnym prostokątem docelowym. Margines nie jest zaokrąglany do całych pikseli źródła. Kontekst GDI ma lokalny czas życia; bitmapa pozostaje buforowana, bez tworzenia przeskalowanego obrazu w paint. Nie ma zmian w algorytmie, historii ani formacie OTBM. API sprawdzono przez Context7 dla wxWidgets 3.3.

![Ten sam wynik w węższym oknie](screenshots/g1-dialog-ux-narrow-20261103.jpg)

Oryginalne JPEG z computer-use, bez przetwarzania. Oceniono centrowanie, opis, kontrolki i przyciski przy 866×660 oraz 716×660. Zielone pola i czarny pusty kafel to minimapa danych, nie podgląd sprite’ów.

## Wyniki i granice dowodu

| Przebieg w `local-data/procedural-smoke/` | Wynik i zakres |
|---|---|
| `g1-dialog-polish-rme-20261103` | **FAIL / exit 8**: test wymagał frazy bez podziału wiersza. Zachowany; nie zaliczono. |
| `g1-dialog-polish-retry-rme-20261103` | **PASS_G1_NATIVE_DIALOG_UX_MATRIX_ONLY / exit 0**: 500×500, pięć pól z otworami, odmowy wejść/starego membership, limit historii 40 MiB, ponowienie przy 512 MiB oraz oba cykle Place/Undo/Redo. Centrowanie nadal nie przeszło oceny wizualnej. |
| `g1-dialog-centering-rme-20261103` | **PASS_G1_NATIVE_DIALOG_SELECTION_UX_ONLY / exit 0**: końcowe rysowanie, opis i ukryte pola, resize, odmowa starego membership, Preview/Validate/Place/Undo/Redo pięciu pól. **To test selection-only, nie pełna macierz.** |

Pierwsza porażka dotyczyła porównania zawiniętego tekstu; obserwator normalizuje teraz podziały wierszy i zapisuje surowy komunikat. Drugi przebieg potwierdził `MATRIX_HISTORY_GUIDANCE_PASS` i `MATRIX_HISTORY_RETRY_PASS`. Limit 40→512 zmieniono rzeczywistym GUI tylko w prywatnej instancji, z zachowaniem 400 akcji. Źródła curated profilu zachowały hashe.

Pierwsza poprawka centrowania przez skalowanie i device origin nie wystarczyła na Windows. Po tej obserwacji zmieniono wyłącznie sposób rysowania; końcowy test selection-only sprawdził tę wersję. **Nie powtarzano 500×500 ani odmowy 40 MiB na końcowym EXE**. Dowód ich wykonania dotyczy wcześniejszego EXE poniżej. Oba przebiegi PASS miały normalny shutdown, pusty runtime stderr i niezmienione wejściowe assets.

Obserwator niezależnie porównuje cały kontekst kafli z przygotowanym wynikiem, historię, rewizję i chroniony sentinel poza obszarem. Dla zaznaczenia kontroluje otwory, dokładne membership bez dopisywania flag selected oraz tożsamość oryginalnych kafli po Undo. Fixture i zmiana membership są programowe; polecenia GUI są rzeczywistymi wejściami operatora. Test nie zalicza gestów myszy tworzących zaznaczenie.

Kanoniczny `build_ninja.bat`: **exit 0** po końcowej zmianie. Log: `g1-dialog-polish-canonical-20261103.log`. Niezależny verifier końcowego przebiegu sprawdził **421 obiektów RME, 421 wejść kompilacji (w tym PCH), moduł obserwatora, źródła UI, oba EXE i curated assets**. Od wcześniejszej macierzy GUI zmienił się tylko produkcyjny CPP dialogu oraz jego nagłówek. Wejścia core pozostają zgodne z historycznym dowodem; G0 i fault injection core nie były uruchamiane ponownie. Przy linkowaniu probe pozostały dwa wcześniejsze C4723 w nagłówku wxWidgets `gdiimage.h:117–118`; vendora nie zmieniano.

| Binarium | SHA256 |
|---|---|
| Końcowy normalny EXE | `163537e3314739b40d5b305320149ba75a4170a317dc940b6bdc26a2a1231e32` |
| Końcowy probe selection-only | `b39b9994f9fa5caed7241fbe45d7b9c8bbe7a203bb3d2db33719616c0a92ce2e` |
| Normalny EXE macierzy przed końcową poprawką paint | `a40cd2e339c4f63e919d403bafc3efc1a28b08c3b67963e9e94df64569a65c6d` |
| Probe tej macierzy | `b415eae4093db0e2c5da5558863e73a84488c848e6ebcc336ef850ab2d31d660` |

## Odtworzenie i dalszy krok

`g1_run_dialog_probe.py OUTPUT PROFILE_DATA CONFIG --selection-only` uruchamia wyłącznie fixture pięciu pól. W GUI: Current selection, seed 1729, wagi 1/0/0, Preview → Validate → po `MATRIX_STALE_MEMBERSHIP_INJECTED` Place (odmowa) → Preview → Validate → resize → Place → Undo → Redo. Weryfikacja: `g1_verify_dialog_probe.py OUTPUT --selection-only`. Polecenia uruchamiać przez `rtk proxy` i Python wskazany w [poprzednim scenariuszu](G1_DIALOG_MATRIX.md). `--matrix` nadal wymaga całej macierzy, teraz również obu markerów historii i prezentacji zaznaczenia.

Kolejne trzy zadania: (1) responsywne anulowanie/harmonogram i RSS, (2) pozostałe odmowy/awarie GUI: live, busy, brak klienta, wielopiętrowe zaznaczenie i alokacje, (3) końcowy generatorowy GUI save/reopen OTBM ze wszystkimi companionami i macierz G1. Nie zweryfikowano innych DPI/platform, sprite preview, współistnienia wyników ani pełnego round-trip. Nie uznano częściowego pokrycia za pełne G1.

Wspólne niezacommitowane zmiany zachowano; brak commit/push. Nie otwierano map użytkownika ani forbidden map/companionów; bez ingerencji w Docker, bazę i świat użytkownika.
