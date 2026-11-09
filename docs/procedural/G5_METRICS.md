# G5: kryteria zapisane przed implementacją i pomiarem

Dokument G5 (FAZA 5: topologia i wielopiętrowe światy). Iteracje 12–23 tej fazy (osady, walidator osiągalności, brody, szlaki przez skałę i rzekę, brzegi, okno, POI, naprawa) są zapisane w historycznym [G4_METRICS.md](G4_METRICS.md) i [G4_GATE_STATUS.md](G4_GATE_STATUS.md), bo powstawały na gałęzi G4; od iteracji 24 kryteria i wyniki G5 są w tym dokumencie i w [G5_GATE_STATUS.md](G5_GATE_STATUS.md). Zasada ta sama: kryteria, pasma i seedy zapisuję przed implementacją i pomiarem, sprawdzam grepem, że zapis jest w pliku.

### Iteracja 24 — graf regionów nad powierzchnią (FAZA 5, punkt 3 planu; zapisana 2026-11-09 przed implementacją i przed pomiarem; decyzja użytkownika: „Graf regionów”)

**Zakres.** Czysta funkcja rdzenia `buildRegionGraph(SurfaceGrid, walls)` (`region_graph.{h,cpp}`) opisująca powierzchnię jako graf: **bez zmiany generatora, bez GUI, bez zapisu na mapę**; jedynym odbiorcą na razie jest `style_render` i testy (graf jest podstawą późniejszych ocen jakości i wielu pięter).

**Definicje.**
- **Klasa komórki:** Land = Grass, Dirt, Shore, Path, Floor; Rock = Rock; Water = Lake, River; Preserve nie ma klasy (nie wchodzi do grafu).
- **Region (węzeł):** 4-spójna składowa jednej klasy o co najmniej **16** komórkach (`regionMinCells`); mniejsze fragmenty nie są węzłami (liczone osobno w `smallFragments` i `smallFragmentCells`). Numeracja węzłów: kolejność pierwszej komórki (wiersze, potem kolumny). Węzeł zawiera: klasę, liczbę komórek, ramkę (minX, minY, maxX, maxY) i środek ciężkości (średnia współrzędnych, dzielenie całkowite).
- **Krawędź sąsiedztwa:** dwa różne węzły ze stykającymi się (4-sąsiedztwo) komórkami; waga = liczba takich par komórek (każda para raz, poziomo i pionowo); krawędzie sortowane po (a, b), a < b.
- **Węzły POI:** start, wejścia (drzwi) i wyjścia z iteracji 22 (tym samym modelem przechodności, ściany włączone); każdy ma indeks węzła-regionu, w którym leży (−1 dla fragmentu mniejszego niż 16) oraz **odległość trasy** od startu w krokach 4-sąsiedzkich po komórkach przechodnich (−1: nieosiągalny; start 0).

**Kryteria.**
1. Test ręcznie policzony (14×6: Water x 6..8 = 18, Rock x 0..3, y 1..4 = 16, Land po lewej = 20, Land po prawej x 9..13 = 30): 4 węzły w kolejności (Land 20, Water 18, Land 30, Rock 16), krawędzie (0,1) waga 6, (0,3) waga 12, (1,2) waga 6; start (5,2) w węźle 0, wyjścia północne (5,0) odległość 2, południowe (5,5) odległość 3, zachodnie (0,2) (skała, węzeł 3) odległość 5, wschodnie (13,2) w węźle 2 nieosiągalne (−1); fragment mniejszy niż 16 (Rock 3×3 w osobnej siatce) nie jest węzłem i liczy się w `smallFragments`; suma komórek węzłów + fragmentów + Preserve = liczba komórek; deterministyczność; pusta siatka nieprawidłowa; `stop_token`.
2. Pomiar (seedy 1001–1032, bok 512, profile v10 m06 i m19, `buildingDensity` 30 / 40, `pathLength` 40 / 80, `fordLength` 48, `repairReach` 1): mediana i zakres liczby węzłów (Land, Rock, Water osobno), krawędzi, fragmentów; udział seedów, w których wszystkie węzły POI mają odległość ≥ 0 (**oczekiwanie: 32 z 32, zgodnie z iteracją 23; to kryterium przechodzenia**); mediana i maksimum odległości do wejść i wyjść; graf sąsiedztwa spójny (każdy węzeł osiągalny z każdego po krawędziach) w ilu seedach (bez pasma).
3. 16 testów rdzenia + nowy test, obie sondy G1 PASS (rdzeń generatora bez zmian: odcisk Seed 1729 bez zmian); dwa przebiegi bajtowo identyczne.

**Zastrzeżenia z góry.** Klasy są grube (Land obejmuje trawę, dirt, plażę, szlaki i budynki; wnętrza regionów nie są rozróżniane); sąsiedztwo to dotyk komórek, nie „przejście”; odległość trasy to kroki 4-sąsiedzkie, nie czas w grze; graf nie zna ruin, obozowisk ani punktów z listy; nie ma jeszcze wielu pięter ani wejść Z; próg 16 komórek jest moim wyborem; graf nie jest zapisywany ani pokazywany w GUI.
