# G3-D — ekstrakcja cech i raport zbiorczy, 2026-11-07

**PARTIAL (G3-D).** Jest raport zbiorczy (JSON, MD, CSV) i cechy z sekcji 4.2 planu w zakresie opisanym niżej. Bramka G3 („powtarzalny raport, statystyki i mapa semantyczna dla każdej mapy referencyjnej”) jest spełniona tylko dla jednej referencji (`forgotten.otbm`) i fixture; nie jest zaliczona.

## Co jest
- Rdzeń `source/procedural/core/features.*` (bez edytora, bez item ID i nazw):
  - **Macierz sąsiedztwa gruntów**: pary kafli na wschód i na południe (zachód i północ to transpozycje), gęsta, odmowa przy > 512 kluczach.
  - **Bloki 16/32/64** wyrównane do współrzędnych bezwzględnych (także ujemnych): histogramy decylowe zajętości, udziału pól przechodnich i udziału gruntu nieprzechodniego.
  - **Cechy per piętro**: liczba kafli z każdą z 16 cech.
  - **Częstość cech w funkcji odległości** (4-sąsiednia, BFS wielostartowy) od najbliższego kafla gruntu nieprzechodniego: doodad, ściana, border, przedziały 1, 2, 3–4, 5–8, 9–16, 17+/brak źródła.
  - Anulowanie `stop_token` co wiersz i co 4096 pobrań z kolejki; pamięć robocza ok. 5 B/komórkę największego piętra.
- Rdzeń `source/procedural/core/feature_report.*`: `renderJson`, `renderMarkdown`, `renderCsv` (tabele: `ground_keys`, `floors`, `adjacency`, `scales`, `distance`). Same liczby całkowite (udziały w promilach), `\n`, bez czasu, adresów i locale. Raport jest agregatem: bez współrzędnych kafli i bez surowej zawartości. Zawiera pokrycie analizy (kafle bez nieznanej zawartości) i listę `unknown`.
- Adapter `makeReportInput`; próbnik zapisuje 7 plików do `run/analysis/`.
- Test `test_procedural_features` PASS na mapie 3×2 policzonej ręcznie (macierz sąsiedztwa, histogramy skal, liczniki cech, przedziały odległości), bloki przy ujemnym x, brak źródła, odmowy (za dużo kluczy, niezgodna topologia, anulowanie), powtarzalność renderów, cytowanie CSV i escapowanie JSON.

## Wynik na `forgotten.otbm` (klient 1098-BlackTek)
Pliki lokalne w `local-data/` (nie w repo); dwa przebiegi dają bajtowo identyczne wszystkie 7 plików (m.in. `reference-analysis.json` 42 712 B, SHA-256 `252029EE…`; `reference-analysis.md` 3 900 B, `874CEB62…`). Przebiegi kończą się same, stderr i `wx-log.txt` puste, 4,7–4,8 s wall całego przebiegu (generowania raportu nie wydzielono).

- Pokrycie analizy: 340 595 z 340 595 kafli bez nieznanej zawartości (100,0 %). Dla fixture z `unknown`: 395 z 400 (98,7 %), 4 różne nieznane id w raporcie.
- Zależność od odległości od gruntu nieprzechodniego (przykład użytecznej cechy): udział kafli z borderem maleje z 68,2 % (odległość 1) przez 46,1 % (2) do 17,1 % (9–16); doodad ma udział 5,8 % tuż przy gruncie nieprzechodnim, ok. 12–14 % w odległości 2–16.
- Bloki 16×16: 2 540 niepustych, z czego 33,6 % w ≥ 90 % to grunt nieprzechodni, a 2,5 % to w ≥ 90 % pola przechodnie; dla 32 i 64 odpowiednio 24,7 %/1,0 % i 15,9 %/0,5 %.
- Najczęstsze przejścia gruntów: `cave`–`cave (alternate border)` 6 541, `cave (alternate border)`–`earth (stone border)` 6 307, `(no brush)`–`cave` 4 117, `grass`–`sea` 2 426.

## Czego NIE zweryfikowano
- Tylko jedna mapa referencyjna i fixture syntetyczne. Brak porównania map ani dowodu, że cechy odróżniają style (to zadanie G4). Brak testu „kilku referencji” z planu.
- Z sekcji 4.2 planu brakuje: długości/krętości/szerokości/rozwidleń rzek, rozmiarów jezior i wysp jako osobnych cech (są tylko regiony gruntu z G3-C), gęstości i klastrów lasu, wysokości/stosunku ścian do plateau gór, komór/korytarzy/cykli/przewężeń jaskiń, sieci szlaków, mostów, rozmieszczenia budowli i osad, mikrostruktur (powtarzalnych fragmentów) i testu podobieństwa/memoryzacji.
- Raport nie zawiera wariantu HTML. CSV i MD są pisane w UTF-8 bez BOM; zgodność z konkretnym arkuszem kalkulacyjnym nie była sprawdzana.
- Częstość cech w funkcji odległości zakłada, że „grunt nieprzechodni” (≥ 90 % blokujących kafli, próg z G3-C) odpowiada wodzie, lawie i skale; nie rozdziela ich. Odległość liczona 4-sąsiednio po prostokącie piętra, bez uwzględnienia przeszkód ani innych pięter.
- Macierz sąsiedztwa liczy tylko sąsiadów na tym samym piętrze; klucz „(no brush)” miesza kafle bez gruntu i grunt bez pędzla.
- Powtarzalność sprawdzona w jednym środowisku (ten sam EXE i klient, 2 przebiegi); nie sprawdzano innego locale systemu ani innej kolejności ładowania. Renderery używają wyłącznie liczb całkowitych, więc locale nie powinien wpływać, ale nie testowano tego.
- Pamięć: 5 B/komórkę to szacunek z kodu, RSS procesu nie mierzono; czas to jeden komplet pomiarów, nie benchmark. Anulowanie przetestowano w teście rdzenia tylko jako zgłoszenie przed pracą.
