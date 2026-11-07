# G3-C — regiony, topologia i piętra, 2026-11-07

**PARTIAL (G3-C).** Zrobione: spójne obszary przechodnie, regiony tego samego gruntu (z oznaczeniem nieprzechodnich i zamkniętych), cechy pięter. Niezrobione: wąskie gardła, rzeki/jaskinie/szlaki jako osobne cechy, połączenia między piętrami, osady i POI. Bramka G3 nie jest zaliczona.

## Co jest
- Rdzeń `source/procedural/core/topology.*` (bez edytora, bez item ID i nazw): union-find po `SemanticMap`, łączność 4-sąsiednia (przekątne nie łączą).
  - **Obszar przechodni** (walkable): kafel ma grunt, nie blokuje i nie ma nieznanej zawartości. Na piętro: liczba składowych, największa, 5 największych, kubełki rozmiarów (potęgi 2), „kieszenie” poniżej progu (domyślnie 16 pól).
  - **Region gruntu**: spójne kafle o tym samym kluczu gruntu. Klucz jest nieprzechodni, gdy ≥ 90 % jego kafli blokuje (próg w opcjach). Region jest „zamknięty”, gdy żaden jego kafel nie dotyka krawędzi zajętego prostokąta piętra.
  - Na piętro także: kafle, nieznane, zmiany pięter, teleporty.
- Anulowanie: `std::stop_token` sprawdzany co wiersz. Pamięć robocza ok. 9 B na komórkę największego piętra, alokowana raz; błąd alokacji zwracany jako błąd.
- Test `test_procedural_topology` PASS na siatkach policzonych ręcznie: 3 składowe (4, 2 i 10 pól), pole nieznane wykluczone, brak połączenia po przekątnej, jezioro zamknięte kontra staw przy krawędzi, zmiany pięter i teleporty, anulowanie przed pracą, odmowa złych opcji i niezgodnego podsumowania, powtarzalność.

## Wynik na `forgotten.otbm` (klient 1098-BlackTek)
Raport lokalny `topology-report.json` (SHA-256 `041ECE44…B1A2`) w `local-data/`; bez współrzędnych. Dwa przebiegi bajtowo identyczne, `semantic-report.json` bez zmian względem G3-B (`5C9C1024…2B4C`); przebiegi kończą się same, stderr i `wx-log.txt` puste, 4,6–4,8 s wall całego przebiegu (topologii nie wydzielono).

- Pola przechodnie razem: 95 103 na 16 piętrach. Piętro 7: 46 534 pola w 489 składowych, największa 22 634 (49 %), druga 8 332; 363 kieszenie < 16 pól (1 415 pól). Pozostałe piętra: od 34 do 383 składowych, największa od 6 do 1 371 pól.
- Zmiany pięter wg cechy `floor_change`: 613 kafli; teleporty: 27.
- Klucze nieprzechodnie (≥ 90 % blokujących, bez użycia nazw): m.in. `sea` 45 034 kafli w 48 regionach (największy 18 469, 44 zamknięte), `clear water` 33 regiony (wszystkie zamknięte), `lava` 21 regionów, `mountain` 260, `earth (stone border)` 118. Sumy kafli po kluczach = 340 595 (liczba kafli).
- Najbardziej rozdrobnione: grunt bez pędzla/pusty 1 770 regionów, `cave` 430, `mountain top ground` 357, `grass` 306 (największy 12 851 kafli).

Przebieg na fixture z `unknown`: 395 pól przechodnych, 1 składowa (395), 5 nieznanych wykluczonych, region „bez klucza” 3 kafle w 2 regionach, zgodnie z przewidywaniem z fixture.

## Czego NIE zweryfikowano
- Wynik na dużej mapie nie ma niezależnej kontroli (np. innym programem): poprawność liczenia składowych potwierdzają testy ręcznie policzone i zgodność sum, nie porównanie z drugą implementacją.
- „Przechodni” to przechodniość po flagach itemów (blokujący item, np. drzewo, ściana, zamyka kafel), nie pełna symulacja ruchu: bez przekątnych, bez rozmiaru stworów, bez przejść między piętrami, bez drzwi otwieranych/zamkniętych.
- Brak wąskich gardeł, długości i krętości rzek, komór i korytarzy jaskiń, sieci szlaków, osad. Region gruntu to nie nazwany biom: nazwy biomów byłyby zgadywaniem, więc raport podaje klucz gruntu z klienta i dane o nieprzechodniości.
- „Zamknięty” liczony względem zajętego prostokąta piętra, nie granic mapy, więc morze przy brzegu zajętego obszaru jest „otwarte”.
- Próg 90 % nieprzechodniości i próg kieszeni 16 są założeniami; nie sprawdzano wrażliwości na nie.
- Pamięć: 9 B/komórkę to szacunek z kodu, RSS procesu nie mierzono; czas to jeden komplet pomiarów, nie benchmark.
