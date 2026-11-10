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

## Iteracja 28 — adapter wejść Z, jaskini i wyjścia rampą (2026-11-09): PARTIAL, kryteria 1–4 i 6 spełnione, 5 z odchyleniem, ocena w GUI (7) czeka na użytkownika

Kryteria zapisane przed implementacją ([G5_METRICS.md](G5_METRICS.md), „Iteracja 28”, commit `97a2094`; grep potwierdził). Decyzje użytkownika: dwa kroki historii (jaskinia, potem powierzchnia), odmowa całości przy zajętym piętrze z + 1, wyjście rampą przy lądowisku, wejście „cave enter”. Commity: `597be12` (rdzeń `placeCaveExits`), `cb2f3cd`…`16e76f2` (adapter, Commit, Undo, podgląd piętra, sonda), `254a851` (poprawka z przeglądu: pełna historia i pusta jaskinia), `5d0a149` (pola okna).

Zaimplementowano: rdzeń `placeCaveExits` (`ExitShape`, `CaveExit`, `CaveExitPlan`; pierwsze dopasowanie w kolejności (dx² + dy², y, x, wariant), wejście bez wyjścia pominięte), opcja `caveExitShapes=ramp` w `style_render` (pola `exits`, `skipped` dopisane do wiersza `under` tylko przy tej opcji); pola profilu `caveEntrance`, `caveExit`, `caveGround` (na końcu struktury), warstwy planu `entrances`, `exitTops`, `exits`, `caveFloor` (haszowane tylko, gdy niepuste); druga jednostka przygotowania `PreparedTerrain::cave` (piętro z + 1, własny snapshot, plan i podgląd, wspólny budżet); błędy `LowerFloorOccupied` i `NoLowerFloor`; Commit w dwóch krokach przez `ActionQueue::beginTerrainPair` / `finishTerrainPair` (cofnięcie pierwszego kroku przy błędzie drugiego, publikacja zmian po obu krokach); wybór piętra w podglądzie okna; zakładka „Underground” w oknie.

**Kryterium 1: spełnione.** Odcisk Seed 1729 100×100 = 17159159863505677800 bez zmian; przy `caveEntrances` 0 wynik `style_render` bajt w bajt jak dotąd (oba profile, 32 seedy); stare przypadki sondy `--surface` bez zmian poza polami czasu i szacunkami pamięci (te zmieniły się, bo urosły struktury `GroundProfile`, `GroundPlan` i `PreparedTerrain`; **odchylenie**, odciski i liczby zmian bez zmian).

**Kryterium 2: spełnione.** Testy ręczne `placeCaveExits` w `test_procedural_underground` (kolejność, zablokowane lądowisko, bliskie wejścia, `occupied`, kolejność wariantów, złe wejścia, przerwanie) i porównanie z niezależnym przeszukiwaniem: 80 z 80.

**Kryterium 3: spełnione.** Seedy 1001–1032, bok 512, profile v10, `caveEntrances` 4, `fordLength` 48, `repairReach` 1, `pathLength` 40 / 80, `buildingDensity` 30 / 40, `caveExitShapes=ramp`:

| profil | wejścia | wejścia z wyjściem | razem |
|---|---|---|---|
| m06 | 13 [8..16] | 13 [8..16] | 419 / 419 = 100 % |
| m19 | 16 [15..16] | 16 [15..16] | 510 / 510 = 100 % |

Dwa przebiegi bajtowo identyczne. **Odchylenie:** `style_render` nie generuje roślinności, więc pomiar nie wyklucza pól z roślinnością (`occupied` puste); przy włączonej „Vegetation” w oknie odsetek może być niższy, nie zmierzony.

**Kryterium 4: spełnione.** Sonda `--surface`: `SURFACE_CAVE_E2E_PASS	128	entrances=5 exits=5 skipped=0 caveCells=882` (Seed 1729, bok 128, `caveEntrances` 20, `rockPermille` 200, `buildingDensity` 10, pędzle „cave enter”, „ramp”, „cave”). Sprawdzone: podgląd na z i z + 1, przedmiot wejścia tylko na wejściach z wyjściem, rampa w P i ogon w P + t, górny kafel w P, grunt jaskini dokładnie na Floor, brak gruntu na Solid, nic poza prostokątem; Commit = dwa kroki i mapa równa podglądom; Undo raz (powierzchnia wraca, jaskinia zostaje), Undo dwa razy (oba piętra jak przed), Redo dwa razy; wymuszony błąd drugiego kroku (limit historii 1) zostawia oba piętra i historię jak przed; odmowy (zajęty kafel na z + 1 także przy masce, z = 15, nieznany lub nie-doodad pędzel wejścia, „small rocks” jako wyjście, nie-grunt jako grunt jaskini, częściowy profil); budżet wspólny (o 1 bajt i o 1 zmianę za mało = odmowa); zmieniony plan jaskini odrzucony przez walidację; przedmioty wejścia i rampy nie blokują według danych klienta; plan z jaskinią ma inny odcisk. Dopisane po przeglądzie: Commit przy historii pełnej do limitu kroków przycina najstarsze kroki zamiast odmawiać; pusta jaskinia (bez skały) daje jeden krok historii. **Odchylenia:** (a) górny kafel rampy 10.98 („stairs” 459) to grunt ze zmianą piętra w dół, więc w P zastępuje grunt powierzchni (zadanie dla wykonawcy wymagało nie-gruntu, kryterium tego nie określało); inne grunty jako górny kafel są odrzucane; (b) obramowania: w P żadne, wokół P kilka elementów obramowań RME (liczone w sondzie, wygląd nie oceniony); (c) pusta jaskinia daje jeden krok zamiast dwóch (pusty krok Undo nic by nie robił).

**Kryterium 5: spełnione z odchyleniem.** Pola „Cave entrances” (0..20), „Z entrance brush”, „Cave exit brush”, „Cave ground brush” (domyślnie „cave enter”, „ramp”, „cave”, gdy klient je ma) są w **osobnej zakładce „Underground”**, nie w zakładce Terrain (każda sekcja okna to osobna zakładka); aktywne przy Surface z budynkami lub obozowiskami, pędzle dopiero przy wejściach > 0; komunikaty dla nowych odmów. Wygląd i działanie w GUI nie sprawdzone (kryterium 7); zapamiętywanie wartości między otwarciami okna nie sprawdzone.

**Kryterium 6: spełnione.** 18 z 18 testów rdzenia, obie sondy G1 PASS po przebudowie `rme`, `SUITE_PASS` w dwóch przebiegach; jedyne stderr to znane ostrzeżenia SetFocus.

**Nie zweryfikowane:** działanie w grze (gdzie serwer stawia gracza po `floorchange=down` i po rampie), wygląd wejścia (wariant „cave enter” wybierany po komórce, może być zwrócony w złą stronę), wygląd jaskini i obramowań w RME, pola okna w GUI, odsetek wyjść przy włączonej roślinności, inne klienty niż 10.98.

**Poprawka po teście GUI użytkownika (2026-11-10):** lista „Cave exit brush” była pusta z rampami (okno brało tylko doodady z pojedynczymi przedmiotami, rampy to kompozyty), więc pole wyjścia zostawało puste. Okno buduje teraz osobną listę z tej samej kontroli co adapter (`resolveCaveExitBrush`); sonda sprawdza, że w 10.98 to dokładnie „bamboo ramp”, „dirt ramp”, „pyramid ramp”, „ramp”, „snowy ramp” (`exitBrushes=5`). Sonda omijała okno, dlatego błąd nie był wykryty.

## Iteracja 29 — ściany jaskini, style wejścia, wyjścia poza skałą (2026-11-10): PARTIAL, kryterium 3a NIE spełnione dla RockFace, pozostałe spełnione, ocena w GUI (7) czeka na użytkownika

Kryteria zapisane przed implementacją ([G5_METRICS.md](G5_METRICS.md), „Iteracja 29”, commit `41924a7`; grep potwierdził). Commity: `277ebf4`, `eb9bdb1` (rdzeń: `CaveEntranceStyle`, `caveEntranceAllowed`, Rock usunięty z pól wyjścia), `46b83ec` (adapter: `caveWall`, dziura jako grunt), `17cec10` (okno: „Entrance style”, „Cave wall brush”), `ab7bb9c` (sonda dla obu stylów).

**Kryterium 1: spełnione.** Odcisk Seed 1729 100×100 = 17159159863505677800 bez zmian; przy `caveEntrances` 0 wyniki bez zmian; stare przypadki sondy bez zmian poza polami czasu i szacunkami pamięci (+112 bajtów, nowe pola struktur; **odchylenie** jak w iteracji 28). Wiersz `SURFACE_CAVE_E2E_PASS` zmienił się zgodnie z zapowiedzią.

**Kryterium 2: spełnione.** Testy ręczne obu stylów (z odrzuceniami), odrzucenie P lub P + d na Rock, porównanie z niezależnym przeszukiwaniem dla obu stylów, deterministyczność; 18 z 18 testów rdzenia.

**Kryterium 3: (a) NIE spełnione dla RockFace, (b), (c), (d) spełnione.** Seedy 1001–1032, bok 512, opcje z iteracji 28:

| profil, styl | wejścia | z wyjściem | odsetek | reguła stylu / wyjścia poza Rock |
|---|---|---|---|---|
| m06 RockFace | 3 [0..7] | 3 [0..7] | 100 % | 100 / 100 |
| m06 FloorHole | 16 [16..16] | 16 [16..16] | 100 % | 512 / 512 |
| m19 RockFace | 6 [1..11] | 6 [1..11] | 100 % | 174 / 174 |
| m19 FloorHole | 16 [16..16] | 16 [16..16] | 100 % | 512 / 512 |

RockFace daje medianę 3 (m06) i 6 (m19) wejść przy wymaganych ≥ 8: reguła „północ, zachód i wschód to skała, południe otwarte” rzadko jest spełniona na pasach skały tych profili. Kryterium nie zmieniam; decyzja o regule należy do użytkownika. Dwa przebiegi bajtowo identyczne.

**Kryterium 4: spełnione.** `SURFACE_CAVE_E2E_PASS	256	style=rockface entrances=18 exits=18 skipped=0 caveCells=7146 wallCells=3821 exitBrushes=5` oraz `style=floorhole entrances=20 exits=20 skipped=0 caveCells=5935 wallCells=3108 exitBrushes=5` (pędzle „cave enter” / „cave hole”, „ramp”, „cave”, „mountain”). Sprawdzone: reguła stylu, przedmiot wejścia albo grunt dziury, P i P + d poza Rock, grunt ściany dokładnie na Solid z sąsiadem Floor, brak gruntu na pozostałych Solid, nic poza prostokątem, Commit, Undo, Redo, wymuszony błąd kroku 2, nowe odmowy, różne odciski stylów.

**Kryterium 5: spełnione (bez oceny w GUI).** Zakładka „Underground”: „Entrance style” (Rock face / Floor hole), lista „Z entrance brush” przełącza się między doodadami a gruntami, „Cave wall brush” (domyślnie „mountain”).

**Kryterium 6: spełnione.** 18 z 18 testów rdzenia, obie sondy PASS, `SUITE_PASS` w dwóch przebiegach (worktree wykonawcy i główny checkout po kanonicznym `build_ninja.bat`), te same wiersze sondy. **Odchylenie:** wykonawca budował w worktree obejściem (brak pamięci i dysku przy kanonicznym buildzie); kanoniczny build przeszedł w głównym checkout.

**Nie zweryfikowane:** wygląd ścian „mountain” w jaskini (lica tylko od wschodu i południa), kierunek wariantów „cave enter”, wygląd „cave hole” na trawie, działanie w grze, inne klienty niż 10.98.

## Iteracja 30 — rampa wyjścia pod wejściem, bez dziury na powierzchni (2026-11-10): kryteria 1–5 spełnione, ocena w GUI (6) czeka na użytkownika

Kryteria zapisane przed implementacją ([G5_METRICS.md](G5_METRICS.md), „Iteracja 30”, commit `2d4064e`; grep potwierdził). Commity: `ce11d56` (rdzeń: wyjście tylko P = E), `63e2c7d` (adapter: bez przedmiotu „top” rampy, warstwa `exitTops` usunięta), `4ec35cb` (korekta: ogon rampy może leżeć na L).

**Odchylenie od definicji (zapisane w G5_METRICS.md jako korekta przed pomiarem).** Warunek „ogon różny od L” był błędem specyfikacji: we wszystkich wariantach ramp 10.98 dolna połowa rampy leży na L. Wykonawca wdrożył regułę bez zmian i dostał 0 wyjść (`SUITE_FAIL`); po korekcie ogon może leżeć na L (nadal Floor i niezarezerwowany).

**Kryterium 1: spełnione.** Odcisk Seed 1729 100×100 = 17159159863505677800 bez zmian; przypadki bez jaskini bez zmian poza szacunkami pamięci (usunięty wektor `exitTops`). Wiersze `SURFACE_CAVE_E2E_PASS` bez zmian liczb (zmienił się układ, nie liczby).

**Kryterium 2: spełnione.** Testy ręczne obu stylów z odrzuceniami (cel na skale, wodzie, dekorze, ognisku; L nie Floor; ogon nie Floor), przypadek ogona na L przyjmowany, prawdziwa geometria ramp 10.98 przyjmowana; 160 porównań z niezależnym przeszukiwaniem (80 siatek na styl); deterministyczność; 18 z 18 testów rdzenia.

**Kryterium 3: spełnione.** Seedy 1001–1032, bok 512, opcje z iteracji 29:

| profil, styl | wejścia | z wyjściem | odsetek | P = E / D otwarte / L Floor |
|---|---|---|---|---|
| m06 RockFace | 3 [0..7] | 3 [0..7] | 100 % | 100 / 100 / 100 |
| m06 FloorHole | 16 [16..16] | 16 [16..16] | 100 % | 512 / 512 / 512 |
| m19 RockFace | 6 [1..11] | 6 [1..11] | 100 % | 174 / 174 / 174 |
| m19 FloorHole | 16 [16..16] | 16 [16..16] | 100 % | 512 / 512 / 512 |

Dwa przebiegi bajtowo identyczne (wyjście i obrazy). Mediana wejść RockFace dalej poniżej 8 (kryterium 3a iteracji 29, D24), nieoceniana tu.

**Kryterium 4: spełnione.** `SURFACE_CAVE_E2E_PASS	256	style=rockface entrances=18 exits=18 skipped=0 caveCells=7146 wallCells=3821 exitBrushes=5` oraz `style=floorhole entrances=20 exits=20 skipped=0 caveCells=5935 wallCells=3108 exitBrushes=5`. Sprawdzone: rampa dokładnie pod wejściem, brak przedmiotu „top” rampy na powierzchni, przedmiot wejścia albo grunt dziury na E, grunt D bez zmian, grunt jaskini na L, ściany, prostokąt, Commit, Undo, Redo, wymuszony błąd kroku 2, odmowy.

**Kryterium 5: spełnione.** 18 z 18 testów rdzenia, obie sondy PASS, `SUITE_PASS` w worktree i w głównym checkout po kanonicznym `build_ninja.bat`. **Odchylenie procesu:** pierwszy przebieg w worktree użył nieprzebudowanych testów rdzenia (`build_ninja.bat` nie buduje celów `test_procedural_*` ani `style_render`); testy i pomiar powtórzone po przebudowie tych celów.

**Nie zweryfikowane:** przesunięcie gracza na L po zejściu i na D po wejściu rampą (zachowanie serwera opisane z pamięci), wygląd rampy pod wejściem, działanie w grze, inne klienty niż 10.98.
