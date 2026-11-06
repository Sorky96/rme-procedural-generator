# G1 — pomiar pamięci procesu i blokowania GUI

2026-11-03: **PASS_G1_GUI_RESOURCE_BASELINE_ONLY**. Zakończono pomiar bazowy; pełne **G1 IN PROGRESS**. **Nie wdrożono ani nie zaliczono responsywnego anulowania w trakcie generowania.** G2 nierozpoczęte. Codex wykonał krok bez nowego uruchomienia Claude.

## Cel i wynik

Zmierzono rzeczywiste Preview/Validate/Cancel w natywnym RME: trzy cykle 100×100 i trzy 500×500, bez drugiego dużego wyniku porównawczego w obserwatorze. Syntetyczna mapa zawierała jeden chroniony sentinel poza obszarem, pozycja generowania (100,100,7), seed 1729, wagi 3/2/1 i brushes grass/dirt/clear water, curated klient 1098-BlackTek.

Preview 500×500 dochodziło do obserwacji gotowego wyniku w **523,5–528,4 ms**, z przerwami timera GUI **536,5–546,1 ms**. Zmierzony working set procesu w tych oknach osiągnął **805,6 MiB**, przyrost względem stanu bezpośrednio przed Preview wyniósł **79,3–80,5 MiB**. UI nadal blokuje na czas przygotowania. To punkt odniesienia dla zmiany harmonogramu, bez twierdzenia o poprawie wydajności.

![Rzeczywiste Preview 500×500 podczas przebiegu pomiarowego](screenshots/g1-resources-500-20261103.jpg)

## Dane

Wszystkie wartości pamięci w tabeli to **MiB = 1 048 576 bajtów**. „Po Cancel” pochodzi z końca dodatkowego okna obserwacji trwającego co najmniej sekundę po zamknięciu dialogu. Pierwszy cykl każdego rozmiaru uwzględnia rozgrzanie jego ścieżki alokacji; nie usuwano go ze statystyki.

| Cykl | Rozmiar | Preview do obserwacji READY [ms] | Maks. przerwa GUI [ms] | WS przed Preview | Szczyt WS | Przyrost WS | WS po Cancel |
|---|---|---:|---:|---:|---:|---:|---:|
| 1 | 100×100 | 64.6 | 103.4 | 719.0 | 722.4 | 3.5 | 721.9 |
| 2 | 100×100 | 66.0 | 101.8 | 722.5 | 723.1 | 0.5 | 722.5 |
| 3 | 100×100 | 67.1 | 93.9 | 723.4 | 723.4 | 0.0 | 722.9 |
| 4 | 500×500 | 528.4 | 541.9 | 724.2 | 804.6 | 80.5 | 724.5 |
| 5 | 500×500 | 523.5 | 536.5 | 726.0 | 805.4 | 79.4 | 724.9 |
| 6 | 500×500 | 524.4 | 546.1 | 726.2 | 805.6 | 79.3 | 728.8 |

Validate trwało 24,0–27,0 ms dla 100×100 i 92,0–96,7 ms dla 500×500. Obserwacja zamkniętego dialogu po Cancel: odpowiednio 41,0–49,3 ms oraz 107,7–119,4 ms. Te czasy obejmują dojście do kolejnego odczytu timera, nie są dokładnym czasem CPU handlera.

Szczyty prywatnego commit procesu dla Preview 500×500 wyniosły **647,6 / 649,2 / 649,4 MiB**, przyrosty **83,2 / 81,2 / 81,4 MiB**. Po Cancel i sekundzie obserwacji: **567,4 / 567,7 / 572,3 MiB**. Working set po dużych cyklach: **724,5 / 724,9 / 728,8 MiB**. Ostatni cykl pozostawił kilka MiB więcej; trzy powtórzenia i liczniki procesu nie rozstrzygają, czy odpowiada za to cache/allocator czy retencja wymagająca dalszej analizy. **Nie zaliczono testu braku wycieków.**

Bazowy working set zawiera assets, renderer i pozostałą część RME. Widoczne w UI **7,7 / 180,3 MiB** są estymatami przygotowania i obrazu, a **512 MiB** jest budżetem tej estymaty, nie limitem RSS całego procesu. Mniejszy zmierzony przyrost WS nie dowodzi poprawności estymatora dla wszystkich map ani braku krótkich szczytów.

[Pełny surowy CSV: 24 rekordy, w tym pamięć prywatna i odstępy próbkowania](benchmarks/g1-ui-resources-20261103.csv). SHA256: `56564d3bb5a2d85b0c46471809c2833a0e47d4646089aa70a4472cbed1286cf4`.

## Metoda i zabezpieczenia

`tests/test_procedural_dialog_resources.cpp` jest prywatnym modułem normalnego wejścia aplikacji. Operator wykonuje komendy wyłącznie przez natywne GUI. `wxEventFilter` obserwuje początek komendy i zawsze zwraca `Event_Skip`; timer GUI 20 ms wykrywa wynik, walidację i zamknięcie. Maksymalna przerwa timera może obejmować fragment odstępu sprzed komendy, renderowanie i obciążenie systemu. Nie jest dokładnym pomiarem opóźnienia konkretnego kliknięcia Cancel.

Osobny `std::jthread` odczytuje jedynie liczniki własnego procesu, bez dostępu do GUI, Tile, mapy ani managerów. Żądany odstęp próbkowania to 5 ms; największy zmierzony odstęp w całym przebiegu wyniósł **6,8 ms**. Krótsze szczyty mogły zostać pominięte. Join następuje przed zniszczeniem stanu samplera.

Working set oznacza bieżące strony rezydentne procesu; private commit jest osobnym licznikiem. Znaczenie i wywołanie sprawdzono w [Microsoft PROCESS_MEMORY_COUNTERS_EX](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex) i [GetProcessMemoryInfo](https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo). Context7 potwierdził kontrakt filtrów wxWidgets 3.3; nie znalazł odpowiedniej dokumentacji PSAPI, użyto oficjalnego Microsoft Learn.

Stan mapy był sprawdzany podczas każdego odczytu GUI: ta sama tożsamość i rewizja, dokładnie jeden niezmieniony sentinel, pusta historia i brak dirty. Wszystkie sześć Cancel zachowało ten stan. **Cancel wykonano dopiero po gotowym Preview i Validate.** Nie wysyłano go z obserwatora i nie symulowano szybkiego anulowania w trakcie pracy.

Sprzęt: Intel Core i7-11850H, 16 procesorów logicznych, 34 069 032 960 bajtów łącznej pamięci fizycznej zgłoszonej przez system (około 31,7 GiB); Windows AMD64, build `10.0.26300`. Build MSVC/Ninja **Release**. Nie zmieniano ustawień systemu ani obciążenia innych aplikacji. Instrumentacja i computer-use również mają koszt; to bazowy pomiar tego środowiska, nie wynik laboratoryjny ani benchmark porównawczy.

## Weryfikacja

Kanoniczny `build_ninja.bat`: **exit 0**, log `local-data/procedural-smoke/g1-resources-canonical-20261103.log`. Produkcyjne CPP i normalny EXE są niezmienione względem zaakceptowanego kroku UX. Nowy kod to obserwator oraz opcja `--resources` runnera/verifiera. Poprzedni nieudany build modułu (`g1-resources-rme-20261103`) zachowano: błąd nazwy PSAPI przy konfiguracji nagłówków; nie uruchomił testu i nie został zaliczony. Poprawka używa `GetProcessMemoryInfo` i systemowej biblioteki Psapi.

`g1-resources-final-rme-20261103`: **PASS / exit 0 / normalny shutdown / pusty stderr / assets i wejścia źródłowe bez zmian**. Niezależny verifier sprawdził 421 obiektów RME, 421 wejść kompilacji (w tym PCH), obserwator, oba EXE, curated assets, hash CSV i komplet 24 rekordów w oczekiwanej kolejności. Przy linkowaniu probe pozostały dwa wcześniejsze C4723 w nagłówku wxWidgets `gdiimage.h:117–118`; vendora nie zmieniano. Starszych audytów G0 i core nie powtarzano.

Normalny EXE SHA256: `163537e3314739b40d5b305320149ba75a4170a317dc940b6bdc26a2a1231e32`.

Probe SHA256: `930e0e9c463309d3c42ed01b1af058d48aeada2ed14347cb84703912d8968a6b`.

Drugi [oryginalny screen 100×100](screenshots/g1-resources-100-20261103.jpg). Oba JPEG pochodzą z natywnego computer-use, bez przeróbek. Przedstawiają minimapę, nie renderer sprite’ów. Prywatny proces zakończył się normalnie.

## Odtworzenie i następny punkt

```text
rtk proxy <PY> docs/procedural/smoke/g1_run_dialog_probe.py local-data/procedural-smoke/NEW_RUN local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --resources
rtk proxy <PY> docs/procedural/smoke/g1_verify_dialog_probe.py local-data/procedural-smoke/NEW_RUN --resources
```

`<PY>` = `C:/Users/Sorky/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe -X utf8`. Po `RESOURCE_CASE_READY` otworzyć Map → Generate terrain. Cykle 1–3: 100×100; 4–6: 500×500. Każdorazowo Preview → po markerze READY Validate → po markerze VALIDATE_PASS Cancel → odczekać kolejny CASE_READY. Nie zamykać procesu pomiędzy cyklami. Runner sam nie klika. Timeout pozostawia ten sam prywatny proces do inspekcji.

Następny krok implementacyjny: etapowe przygotowanie z anulowaniem i identyfikatorem zadania. Snapshot/klient/resolver/DrawOperations muszą zachować potwierdzone reguły wątków; worker może dostać wyłącznie własne dane. Obecnego `prepareTerrain` nie należy przenosić w całości na worker: kontroluje GUI thread i używa managerów oraz żywej mapy. Trzeba mierzyć także czas finalnego Validate i zwalniania wyniku, a opóźniony wynik po Cancel lub zmianie celu odrzucać przed publikacją.

Pozostałe kolejne zadania: odmowy/awarie GUI oraz końcowy generatorowy save/reopen OTBM ze wszystkimi companionami. Ten przebieg nie sprawdzał Commit/Undo, RSS commitu, gęstych map, pracy równoległej, wielopiętrowości, innych DPI/platform, leak detectora ani pełnej macierzy G1. Wspólne niezacommitowane zmiany zachowano, bez commit/push i bez nowego Claude. Nie otwierano map użytkownika ani forbidden map/companionów; bez ingerencji w Docker, bazę i świat użytkownika.
