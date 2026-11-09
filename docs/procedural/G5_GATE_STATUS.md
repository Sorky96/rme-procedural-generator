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

## Iteracja 26 — ognisko w obozowisku (2026-11-09): PARTIAL, kryteria spełnione z jedną rozbieżnością, ognisko z prawdziwym pędzlem nie sprawdzone

Kryteria zapisane przed implementacją ([G5_METRICS.md](G5_METRICS.md), „Iteracja 26”, commit `366f757`; grep potwierdził). Zaimplementowano: `SurfaceGrid::fires` (po jednej komórce Path w środku polany; w odcisku tylko, gdy niepusta; `valid()` wymaga Path), nowa geometria obozowiska (polana: dysk Path dx² + dy² ≤ 18 = 61 komórek, ognisko w środku, namiot Floor 3×3 przesunięty o 3 komórki na wschód, wolny teren Grass lub Dirt w promieniu 7 i brak Floor w promieniu 12), `GroundProfile::campfire` (pędzel doodad, bez identyfikatorów w kodzie), warstwa planu `GroundPlan::fires` (w odcisku planu tylko, gdy niepusta; niezależna od „Vegetation”), przedmiot wybierany z wariantów pędzla po komórce i ziarnie, walidacja w resolverze, przygotowaniu i podglądzie, pole „Campfire brush” w zakładce Buildings (puste, gdy klient nie ma pędzla „campfire”).

**Kryterium 1: spełnione** (bez obozowisk lub bez pędzla ogniska: odcisk Seed 1729 i 17 testów rdzenia bez zmian). **Kryterium 2: spełnione:** test `surface`: jedno ognisko na obozowisko; geometria policzona ręcznie i sprawdzona na seedzie z dokładnie jednym obozowiskiem (siatka 37×37, `campCount` 50): ognisko na Path, miejsce = środek namiotu 3 na wschód od ogniska, 8 luk, 9 komórek Floor i 52 komórki Path w dysku (61 − 9); `valid()` odrzuca ognisko poza Path (w teście rdzenia przez ręcznie zbudowaną siatkę). **Kryterium 3: spełnione, ale z zamiennikiem pędzla:** klient 10.98 w repo **nie ma pędzla „campfire”** (są przedmioty 1421–1426 bez pędzla; pędzel istnieje w danych 12.90+), więc nowy przypadek sondy `SURFACE_FIRES_E2E_PASS` (bok 250, 7 ognisk) używa pędzla „small rocks” jako pędzla ogniska i sprawdza mechanizm: warstwa ognisk w planie = komórki ognisk, odmowy (nieznany pędzel, pędzel gruntu, profil bez pędzla podłogi), po Commit dokładnie jeden element na każdej komórce ogniska (id zgodne z planem) i żaden gdzie indziej, jeden krok historii, Undo i Redo przywracają stan; odcisk planu z ogniskiem różni się od planu bez. **Kryterium 5: spełnione** (17 testów rdzenia i obie sondy G1 PASS po przebudowaniu `rme`; dwa przebiegi bajtowo identyczne).

**Kryterium 4: spełnione z rozbieżnością.** Seedy 1001–1032, bok 512, ruiny 30 %, 6 obozowisk, `fordLength` 48, `repairReach` 1: wszystkie miejsca osiągnięte w 32 z 32 seedów (m06 i m19), wszystkie POI w 32 z 32; obozowisk **w m06 24 w każdym seedzie, w m19 mediana 24, ale min 12** (zapis wymagał „nadal 24 na mapę”: w niektórych seedach m19 nowe wymagania wolnego terenu (promień 7, brak Floor w 12) dają o połowę mniej obozowisk).

**Nie zweryfikowano (i zastrzeżenia z zapisu):** wyglądu; działania z prawdziwym pędzlem „campfire” (brak go w kliencie z repo; użytkownik ma sprawdzić na swoim kliencie, czy pole „Campfire brush” się wypełnia i czy ognisko się pojawia); wariantów pędzla „campfire” (pierścień, spalone drewno, ogień wybierane po komórce, więc część ognisk może być zgaszona); blokowania komórki ogniska w grze; ogniska jako POI; wpływu na wydajność.

## Iteracja 27 — wejścia Z i prosta jaskinia, część rdzeniowa (2026-11-09): PARTIAL, kryteria 1–4 spełnione, nic nie jest jeszcze rysowane

Kryteria zapisane przed implementacją ([G5_METRICS.md](G5_METRICS.md), „Iteracja 27”, commit `d0bd493`; grep potwierdził), zakres wybrany przez użytkownika: „wejścia + prosta jaskinia”, podzielony na rdzeń (ta iteracja) i adapter (iteracja 28). Zaimplementowano: `SurfaceTuning::caveEntrances` (0..20 na 65 536 komórek, dopisane na końcu, 0 = bajt w bajt jak dotąd), `SurfaceGrid::entrances` (Rock na brzegu skały, ≥ 3 od brzegu obszaru, bez wody i podłogi w oknie 5×5, odstęp ≥ 24; w odcisku tylko, gdy niepusta; `valid()` wymaga Rock), nowy `underground.{h,cpp}`: `CaveGrid` (Solid / Floor, z + 1), `generateCave` (lądowisko 21 komórek, tunel 3 szerokości o osi wychylonej szumem między kolejnymi wejściami lub w stronę środka przy jednym, sala co ok. 24 komórki osi, brzeg 1 komórki Solid), `undergroundReachabilityOf` (wejście osiągnięte, gdy jego komórka na powierzchni leży w składowej startu; komórki jaskini osiągnięte przez składową z lądowiskiem osiągniętego wejścia; kieszenie), wiersz `under` w `style_render`.

**Kryterium 1: spełnione** (`caveEntrances` 0: odcisk Seed 1729 i 17 testów rdzenia bez zmian). **Kryterium 2: spełnione** (nowy `test_procedural_underground`: wejścia na Rock, przy otwartym gruncie, 3 od brzegu, w odstępach ≥ 24, deterministyczne, `valid()`; jaskinia dwóch wejść w 40×20: oba lądowiska po 21 komórek Floor, brzeg Solid, jedna składowa, deterministyczna, inne ziarno daje inną jaskinię, brak wejść daje samo Solid, jedno wejście daje lądowisko i tunel w jednej składowej; walidator na ręcznie zbudowanej siatce 40×20 z jeziorem: osiągnięte tylko wejście po stronie startu, dwa dyski 42 komórki / 2 składowe / 21 osiągniętych / 1 kieszeń, po dodaniu 15-komórkowego tunelu 57 komórek / 1 składowa / wszystkie osiągnięte; niezgodne rozmiary odrzucone; porównanie z niezależnym przeszukiwaniem na 80 losowych parach siatek: 80 z 80). **Kryterium 4: spełnione** (18 testów rdzenia, obie sondy G1 PASS po przebudowaniu `rme`; dwa przebiegi bajtowo identyczne).

**Kryterium 3: spełnione.** Seedy 1001–1032, bok 512, profile v10, `caveEntrances` 4, `buildingDensity` 30 / 40, `pathLength` 40 / 80, `fordLength` 48, `repairReach` 1 (m06 / m19): wejścia mediana 13 [8..16] / 16 [15..16] (zapis: do 16, bo 4 × 4); wszystkie seedy mają wejścia (32 z 32); komórki jaskini na 65 536: mediana 1518 / 2364; w seedach z ≥ 2 wejściami (32 z 32) **jaskinia ma jedną składową i 0 kieszeni w 32 z 32, a wszystkie wejścia są osiągnięte z startu w 32 z 32** (wymagane ≥ 90 %; skała jest przechodnia, więc jej brzegi są osiągalne razem z resztą lądu).

**Zastrzeżenia (z zapisu):** jaskinia to tunele i sale bez wody, bez bocznych dróg i ślepych zakoułków (poza jednym wejściem); wejścia tylko na brzegu skały (nie w budynkach, nie schody); wyjście z jaskini to ten sam punkt (x, y); naprawa osiągalności z iteracji 23 nie zna wejść Z (wynik 32 z 32 wynika z przechodniości skały, nie z naprawy); nic nie jest jeszcze rysowane ani przekładane na przedmioty (iteracja 28: przedmiot wejścia z pędzla doodad, pędzel gruntu jaskini, Preview, Validate, Commit i Undo dla dwóch pięter, okno); wielopiętrowe góry (kilka pięter pod spodem) poza zakresem. **Nie zweryfikowano:** wyglądu jaskini (nie ma jeszcze na mapie), wydajności (nie mierzono osobno), zachowania przy maskach i ochronie, jaskini pod wejściami przy bardzo małych obszarach.
