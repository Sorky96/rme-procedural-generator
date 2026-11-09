# G5: wyniki i stan bramki

Wyniki iteracji G5 od numeru 24 (kryteria w [G5_METRICS.md](G5_METRICS.md)). Iteracje 12–23 (osady, walidator osiągalności, brody, szlaki, brzegi, okno, POI, naprawa) są w historycznym [G4_GATE_STATUS.md](G4_GATE_STATUS.md). **G5 (z planu):** z ustalonego startu osiągalne są wymagane POI, prawidłowe wejścia i wyjścia Z oraz brak krytycznych odcięć; dziś sprawdzone tylko na powierzchni jednego piętra.

## Iteracja 24 — graf regionów nad powierzchnią (2026-11-09): PARTIAL, kryteria 1–3 spełnione

Kryteria zapisane przed implementacją ([G5_METRICS.md](G5_METRICS.md), „Iteracja 24”, commit `8a6bfe1`, przeniesione do pliku G5 w `a115e93`; grep potwierdził). Zaimplementowano `buildRegionGraph` (`region_graph.{h,cpp}`): węzły = 4-spójne składowe klas Land / Rock / Water o ≥ 16 komórkach (mniejsze to fragmenty), krawędzie sąsiedztwa z wagą (pary komórek), węzły POI (start, wejścia, wyjścia) z regionem i odległością trasy od startu; czysta funkcja, generator bez zmian; wiersz `graph` w `style_render`.

**Kryterium 1: spełnione.** Nowy `test_procedural_region_graph` z ręcznie policzonym układem 14×6: 4 węzły (Land 20, Water 18, Land 30, Rock 16), krawędzie (0,1) 6, (0,3) 12, (1,2) 6, ramka i środek ciężkości skały, start (5,2), wyjścia północne 2, południowe 3, zachodnie (w skale) 5, wschodnie nieosiągalne; fragment 3×3 skały nie jest węzłem; suma komórek węzłów + fragmentów + Preserve = liczba komórek; wejście w regionie Land; wszystkie komórki jednej klasy Floor/Land; błędy, `stop_token` i deterministyczność. **Kryterium 3: spełnione** (17 testów rdzenia i obie sondy G1 PASS; rdzeń generatora bez zmian; dwa przebiegi bajtowo identyczne).

**Kryterium 2: spełnione.** Seedy 1001–1032, bok 512, profile v10, `buildingDensity` 30 / 40, `pathLength` 40 / 80, `fordLength` 48, `repairReach` 1 (mediana [min..max], m06 / m19):

| miara | m06 | m19 |
| --- | ---: | ---: |
| węzły | 24 [15..31] | 27 [15..42] |
| Land / Rock / Water | 7 / 6 / 10 | 8 / 8 / 11 |
| krawędzie | 32 [19..42] | 36 [20..52] |
| fragmenty < 16 komórek | 4 [0..11] | 5 [1..12] |
| seedy bez nieosiągniętych POI | **32 z 32** | **32 z 32** |
| graf sąsiedztwa spójny | 32 z 32 | 32 z 32 |
| maks. odległość do wejścia (kroki) | 585 [425..910] | 570 [374..1028] |
| maks. odległość do wyjścia (kroki) | 591 [309..996] | 520 [294..980] |

Wymagane (kryterium przechodzenia): wszystkie węzły POI z odległością ≥ 0 w 32 z 32 seedów (zgodnie z iteracją 23): jest 32 z 32. Czas jednego grafu mieści się w czasie generowania (jeden seed z grafem i całą resztą 0,25 s w `style_render`; to nie jest osobny pomiar grafu).

**Zastrzeżenia (z zapisu):** klasy są grube (Land obejmuje trawę, dirt, plażę, szlaki i budynki); sąsiedztwo to dotyk komórek, nie „przejście”; odległość to kroki 4-sąsiedzkie, nie czas w grze; graf nie zna ruin, obozowisk ani punktów z listy; brak wielu pięter i wejść Z; próg 16 jest moim wyborem; graf nie jest zapisywany ani pokazywany w GUI. **Nie zweryfikowano:** wydajności samego grafu (nie mierzono osobno), zachowania przy maskach i ochronie (Preserve jest wykluczony z grafu), użyteczności grafu dla oceny jakości (nie ma jeszcze odbiorcy).

## Iteracja 25 — ruiny i obozowiska (2026-11-09): PARTIAL, kryteria 1–4 spełnione, wygląd czeka na użytkownika

Kryteria zapisane przed implementacją ([G5_METRICS.md](G5_METRICS.md), „Iteracja 25”, commit `6815b84`; grep potwierdził), z jawnym odchyleniem od zasady „najpierw pomiar w referencjach” (definicje to moja idealizacja, kryteria bezwzględne). Zaimplementowano: `SurfaceTuning::ruinPercent` (0..100) i `campCount` (0..50 na 65 536 komórek), dopisane na końcu, 0 = bajt w bajt jak dotąd; `SurfaceGrid::gaps` i `sites` (w odcisku tylko, gdy niepuste; `valid()` sprawdza je na Floor); **ruina** = budynek bez drzwi i bez trasy do drzwi, z lukami w murze (≥ 2, około ćwierć obrysu bez narożników); **obozowisko** = polana (dysk Path r = 3) z otwartym namiotem (Floor 3×3, cały obrys to luka), w miejscu bez wody, skały i podłogi (≤ 12) i szlaku (≤ 5), odstęp środków ≥ 12; resolver pomija ścianę na lukach; walidator liczy luki jak drzwi (przechodnie), `PoiReport` ma `sites` i `sitesReached`; tryb naprawy brodów dołącza miejsca; pola „Ruins (%)” i „Camps” w zakładce Buildings (profil z podłogą, 8 pędzli, także przy samych obozowiskach).

**Kryterium 1: spełnione** (opcje 0: odcisk Seed 1729 i 17 testów rdzenia bez zmian). **Kryterium 2: spełnione** (testy ręczne: budynek bez drzwi i bez luk zamknięty, z jedną luką osiągalny (69 komórek, 1 składowa), miejsce osiągnięte przez lukę, luka i miejsce poza Floor odrzucone przez `valid()`, odcisk się zmienia; test właściwości `ruinsAndCamps`: opcje wyłączone bez luk, włączone: deterministyczne, poprawna siatka, drzwi + miejsca pokrywają fundamenty, każde miejsce osiągnięte po naprawie, obozowisko ma jedno miejsce i 8 luk, `campCount` 51 odrzucone). **Kryterium 4: spełnione:** 17 testów rdzenia i obie sondy G1 PASS po przebudowaniu `rme`; nowy przypadek `--surface` `SURFACE_RUINS_E2E_PASS` (bok 250: 146 luk, 23 miejsca, 948 ścian w planie; liczba ścian + liczba luk = liczba komórek obrysu; Commit jako jeden krok, **żaden element ściany na kaflu luki**, Undo, Redo); dwa przebiegi bajtowo identyczne.

**Kryterium 3: spełnione.** Seedy 1001–1032, bok 512, profile v10, `ruinPercent` 30, `campCount` 6, `fordLength` 48, `repairReach` 1 (m06 / m19, `buildingDensity` 30 / 40):

| miara | m06 | m19 |
| --- | ---: | ---: |
| budynki (mediana) | 120 | 160 |
| ruiny (mediana [min..max]) | 35 [26..43] | 47 [40..60] |
| udział ruin wśród budynków (mediana) | 29,2 % | 29,4 % |
| obozowiska | 24 [24..24] | 24 [24..24] |
| luki na ruinę (mediana) | 5 | 5 |
| seedy ze wszystkimi miejscami osiągniętymi | **32 z 32** | **32 z 32** |
| seedy ze wszystkimi POI osiągniętymi | 32 z 32 | 32 z 32 |

Wymagane: ≥ 29 z 32 seedów ze wszystkimi ruinami i obozowiskami osiągniętymi z startu: jest 32 z 32; udział ruin ≈ 30 % zgodnie z oczekiwaniem.

**Nie zweryfikowano (i zastrzeżenia z zapisu):** wyglądu (ruina to budynek z dziurami w murze, bez gruzu; obozowisko to polana ze ścieżką i podłogą bez ognisk i doodadów, bo profil nie ma na nie pędzla); czy pędzel ściany daje sensowne końce muru przy lukach (liczy `WallBrush::doWalls`); pomiaru w referencjach (nie zrobiono); ruin i obozowisk jako osobnych POI w grafie regionów (tylko `sites` w walidatorze); zachowania przy maskach i ochronie; pól okna „Ruins (%)” i „Camps” w GUI (zbudowane, nieoglądane); wydajności (nie mierzono osobno).
