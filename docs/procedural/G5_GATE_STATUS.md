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

## Iteracja 31 — wejście nad każdą rampą w GUI, jaskinia bez budynków (2026-11-10): PARTIAL, błąd (a) nieodtworzony, (b) poprawione, ocena w GUI czeka

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 31”, commit `00ddf55`. Commity: `fb36897` (sonda `guiCaves`), `1008b32` (okno: jaskinia bez budynków).

**Kryterium 1: spełnione.** Odcisk Seed 1729 100×100 bez zmian, stare wiersze sondy bez zmian.
**Kryterium 2: NIE spełnione (nieodtworzone).** 64 kombinacje (prostokąt 512 w (800,1000,7), 8 seedów, oba style, budynki 0 i 40, skała „rocky ground” i „mountain”, roślinność, ścieżki): 4660 par wejście–rampa, każda rampa ma wejście nad sobą i odwrotnie. Przyczyna objawu z GUI nieznana; brak poprawki produkcyjnej.
**Kryterium 3: spełnione.** Przy budynkach 0 pola jaskini aktywne, żądanie niesie `caveEntrances`, przypadki z budynkami 0 w sondzie przechodzą.
**Kryterium 4: spełnione.** 18 z 18 testów rdzenia (cele przebudowane), obie sondy PASS, `SUITE_PASS` w worktree i w głównym checkout. **Odchylenie:** build wykonawcy przerwany przez pełny dysk C:; za zgodą użytkownika usunięto `vcpkguildtreesngle`.

**Nie zweryfikowane:** przyczyna braku wejścia w sesji GUI użytkownika (prostokąt, seed i stan mapy nieznane), maski i ochrona na dużym obszarze, wygląd w GUI.

## Iteracja 32 — płaskowyż z − 1 z roślinnością i rampami (2026-11-10): PASS w pomiarach i sondzie, ocena w GUI czeka

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 32”, commit `246b922`. Commity: `693ca6c` (historia N kroków), `52196f7` (rdzeń), `30b5ce3` (podgląd, commit, zakładka „Levels”).

**Kryterium 1: spełnione.** Odcisk Seed 1729 bez zmian; stare liczby i odciski sondy bez zmian. **Odchylenie:** szacunki pamięci w starych wierszach sondy wzrosły (+296 B, +72768 B w PREPARATION_SIZE_PASS).
**Kryterium 2: spełnione.** PLATEAU_CROSS_CHECK 60/60.
**Kryterium 3: spełnione.** m06: płaskowyże 4 [2..10], Top 18837,5, rampy 11 [5..22]; m19: 5,5 [2..10], Top 58767, rampy 17 [5..28]. Rampa w każdym płaskowyżu, każde Top osiągalne, dwa przebiegi identyczne. **Odchylenie:** przy ciasnym miejscu mniej ramp niż wzór (zawsze ≥ 1).
**Kryterium 4–5: spełnione.** Kolejność historii: płaskowyż, jaskinia, powierzchnia. 18/18 testów, obie sondy, `SUITE_PASS` w worktree i w głównym checkout.

**Nie zweryfikowane:** wygląd w GUI, ruch w grze, wczytanie w oryginalnym RME.

## Iteracja 33 — góry bez ścian, piętra z − 2 i z − 3, limit procentowy (2026-11-10): PASS w pomiarach i sondzie, ocena w GUI czeka

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 33” (`7a54575`, poprawki `e0d2a76`, `250fc86`, `c3c0cdd`, `ad59acc`, `507c43e`). Commity: `71d02da` (rdzeń), `2d29a41` (podgląd, commit, okno), test i pomiar w ostatnim commicie.

**Kryterium 1: spełnione.** Odcisk Seed 1729 i przypadki bez płaskowyżu bez zmian; przypadek płaskowyżu zmieniony świadomie (pas z gruntem trawy, nie „mountain”). Stary profil z `plateauEdge` wczytuje się, pole jest ignorowane.
**Kryterium 2: spełnione.** Ręczne siatki i porównanie z niezależnym przeszukiwaniem na 60 siatkach (3 piętra, korytarze, limity).
**Kryterium 3: spełnione** (`plateauMaxPercent` 10, mediana [min..max] komórek piętra):

| Profil | z − 1 | z − 2 | z − 3 |
|---|---:|---:|---:|
| m06 | 20476,5 [19713..20905] (≈ 7,8 %) | 1894 [1597..2027] | 141,5 [0..195] |
| m19 | 25621,5 [23938..26201] (≈ 9,8 %) | 2406 [2152..2580] | 184,5 [0..238] |

Rampa w każdym płaskowyżu, wszystkie piętra osiągalne z powierzchni, limity (f) i (g) spełnione, dwa przebiegi identyczne. Minimalne odsunięcie wyższych pięter 4 pola (poprawka zatwierdzona przez orkiestratora).
**Kryteria 4–5: spełnione.** Kolejność historii: najwyższe piętro, niższe, jaskinia, powierzchnia. Odmowa przy zajętym piętrze i przy zakresie z ponad 0. 18/18 testów, obie sondy, `SUITE_PASS` w worktree i w głównym checkout.

**Nie zweryfikowane:** wygląd w GUI, ruch w grze, wczytanie w oryginalnym RME.

## Iteracja 34 — czapy gór na własnym zboczu (2026-11-11): PASS w pomiarach i sondzie, ocena w GUI czeka

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 34”, `8b7a14f`. Commity: `2aedc0b`, `74f7614`, `2eed650`, `7d65381`.

**Kryterium 1: spełnione.** Odcisk Seed 1729 i wszystkie przypadki bez gór bez zmian (porównanie z iteracją 33 bez różnic poza czasem i pamięcią). Przypadki gór zmienione świadomie (cała czapa, otwarcia rampy, brak korytarzy).
**Kryterium 2: spełnione.** PLATEAU_CROSS_CHECK 60/60, PLATEAU_LEVELS_LIMIT_CROSS_CHECK 60/60.
**Kryterium 3: spełnione** (mediana [min..max]; roślinność: udział na trawie czapy / na trawie powierzchni):

| Profil | Piętro | Czapy | Komórki | Rampy | Roślinność czapa / powierzchnia |
|---|---|---:|---:|---:|---:|
| m06 | z − 1 | 4 [2..12] | 20884 | 12 | 11,5 % / 11,7 % |
| m06 | z − 2 | 1,5 [1..4] | 1907 | 4,5 | 10,8 % / 11,7 % |
| m06 | z − 3 | 1 [1..2] | 155 | 1 | 10,4 % / 11,7 % |
| m19 | z − 1 | 5 [1..10] | 18080 | 12,5 | 38,3 % / 37,6 % |
| m19 | z − 2 | 1,5 [0..5] | 1687 | 4 | 38,1 % / 37,6 % |
| m19 | z − 3 | 1 [0..2] | 113,5 | 1 | 35,9 % / 37,6 % |

Zbocze pod każdą czapą, wszystko osiągalne, limity, roślinność ≥ 88 % powierzchni, dwa przebiegi identyczne.
**Kryteria 4–5: spełnione.** Sonda: grunt „mountain” pod każdą czapą po autoborderze, otwarcia rampy tylko przy górach. 18/18 testów, obie sondy, `SUITE_PASS` w worktree i w głównym checkout.

**Nie zweryfikowane:** wygląd w GUI (narożniki zbocza, przesunięcie rysowania), ruch w grze.

## Iteracja 36 — walidator osiągalności przez wszystkie piętra, pomiar bramki G5 (2026-11-12): walidator PASS, **bramka G5 FAIL**

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 36” (`4b56caf`, uzupełnienie maski `2154da9`). Commity: `1ba60ef` (rdzeń `levelReachabilityOf`), `fe0cc50` (pomiar, `docs/procedural/smoke/g5_measure_levels.py`), `84a5b66` (adapter: maska z rozwiązanych przedmiotów, okno: linia stanu, ostrzeżenie przy Place, ponawianie seedów).

**Model.** Skała powierzchni poza czapami przechodnia (10 z 10 wariantów „rocky ground” bez flagi blokowania w 10.98); zbocze „mountain” pod czapą niechodliwe. „small rocks” ma warianty mieszane, więc adapter liczy maskę na komórkę. **Odchylenie:** maska obejmuje też blokujące przedmioty obrysów, murów i gruntu, nie tylko dekor (inaczej model byłby niezgodny z mapą). `style_render` używa reguły ostrożnej (klasa blokuje, gdy blokuje którykolwiek wariant).

**Kryteria 1, 2, 3, 5: spełnione.** Odcisk Seed 1729 i stare wiersze sond bez zmian (poza szacunkami pamięci). 100 porównań z niezależnym przeszukiwaniem pięciu pięter. Sonda: model zgodny z mapą na pięciu piętrach, krawędzie Z z przedmiotami floorchange, ponawianie seedów w oknie. 18/18 testów, obie sondy, `SUITE_PASS` w worktree i w głównym checkout.

**Kryterium 4 (bramka G5): (a) NIE, (b) NIE, (c) NIE, (d) tak.** Pełna osiągalność: m06 0 z 32, m19 0 z 32 (bez ponawiania i z 8 próbami). Krawędzie Z chodliwe z obu stron: m06 692 / 774, m19 708 / 748.

| Profil | Drzwi | Obozy i ruiny | Wejścia jaskini | Wyjścia jaskini | Komórki czap | Komórki jaskini |
|---|---:|---:|---:|---:|---:|---:|
| m06 | 2835 / 3086 | 1197 / 1266 | 1 / 83 | 75 / 83 | 541265 / 593789 | 54944 / 57163 |
| m19 | 3248 / 4102 | 1254 / 1523 | 57 / 101 | 89 / 101 | 109139 / 322370 | 92943 / 106611 |

**Przyczyny (diagnostyka na seedzie 1001, bez zmian generatora):** (1) naprawa osiągalności z iteracji 23 używa starego modelu (skała przechodnia, dekor ignorowany), więc roślinność blokująca dzieli ląd — w m19 główna przyczyna; (2) zbocza pod czapami (od iteracji 34) przecinają drogi, które wcześniej szły po skale — w m06 główna przyczyna; (3) wejścia jaskini leżą na brzegu skały, który od iteracji 34 jest zboczem. Wyłączenie blokowania dekoru i zboczy w modelu przywraca pełną osiągalność w obu profilach.

**Nie zweryfikowane:** ruch w grze, kroki ukośne, wygląd linii stanu i ostrzeżenia w GUI, obszar 2000×2000 z walidatorem.

## Iteracja 37 — naprawa wielopiętrowa i mosty (2026-11-12): implementacja i sondy PASS, **bramka G5 FAIL; D22 pozostaje otwarty**

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 37”. Commity implementacji: `567269f` (rdzeń), `fc1f91b` (adapter, okno, sonda i pomiar). Użytkownik zatwierdził zastępowanie wody gruntem mostu z pędzla wall „bridge”; kryteriów nie zmieniano w tej gałęzi.

**Implementacja.** `repairLevelReach` ocenia kompletny plan po każdej naprawie. Dijkstra: chodliwe 1, blokujący dekor 3, woda 6; stan zawiera długość kolejnego odcinka wody, ograniczoną przez `fordLength`. Remisy według indeksu komórki (y, x). Zbocza, mury, Preserve i Solid jaskini są zabronione. Bez ścieżki: usunięcie czapy i wspartych na niej wyższych czap albo powierzchniowego POI; limit 64, potem `unrepaired`. Czapy powstają przed filtrem wejść, jaskinia po nim. Adapter używa rozwiązanych wariantów dekoru i gruntu, pomiar reguły ostrożnej.

**Most.** Kierunek jest zapisany przez rdzeń z osi ścieżki, w zakręcie pierwszeństwo ma poziomy. Adapter wybiera pierwszy wariant o dodatniej szansie z odpowiedniego węzła horizontal/vertical załadowanego pędzla. Bez pędzla zostaje piasek. **Odchylenie geometryczne:** przejście poszerza wodę o jedną komórkę po obu stronach osi (tylko woda, z przycięciem do prostokąta). Jednokomórkowy most miał chodliwy grunt, ale ukośne obramowania morza blokowały oba przyczółki. Poszerzenie pozwala zachować zwykły autoborder, bez usuwania przedmiotów obramowania. Liczba mostów poniżej oznacza komórki gruntu mostu wraz z tym poszerzeniem.

**Testy i sondy.** 18/18 celów rdzenia i obie sondy PASS, `SUITE_PASS`, po kanonicznym buildzie Windows x64 i osobnej przebudowie testów/style_render (dwa zadania). Odcisk Seed 1729 100×100 = 17159159863505677800. Niezależny Dijkstra: 120 losowych siatek; niezależne przejście pięciu pięter: 100 planów. Siatki ręczne: dekor, most, bród, zakaz murów/zboczy, usunięcie czapy z wyższą i powrót skały, usunięcie POI, filtr wejścia pod czapą, limit 64. Sonda sprawdza oba kierunki mostu i brodu, usunięty dekor, Commit/Undo/Redo oraz zgodność maski z mapą w planie z jaskinią i czapą.

**Bramka G5.** Seedy 1001–1032, bok 512, ustawienia iteracji 36 plus `repairReach=1`. Każdy wariant uruchomiony dwa razy; stdout i stderr bajtowo identyczne. Mediana [min..max]:

| Profil | Próby | Pełne seedy | Chodliwe krawędzie Z | Naprawy | Komórki mostu | Usunięte czapy | Usunięte POI | Udział usuniętych POI |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| m06 | 1 | 32/32 | 610/610 | 19 [9..26] | 4 [0..107] | 0 [0..0] | 0 [0..1] | 0,04595 % |
| m06 | ≤8 | 32/32 | 610/610 | 19 [9..26] | 4 [0..107] | 0 [0..0] | 0 [0..1] | 0,04595 % |
| m19 | 1 | 1/32 | 668/668 | 64 [13..64] | 0 [0..18] | 0 [0..0] | 0 [0..2] | 0,07035 % |
| m19 | ≤8 | 7/32 | 589/589 | 64 [13..64] | 0 [0..18] | 0 [0..0] | 0 [0..1] | 0,03511 % |

Kryterium 4(a) i 4(b) **nie spełnione dla m19**; 4(c), 4(d), 4(e) spełnione dla obu profili. Naprawa nie zalicza bramki kasowaniem: usunięte POI łącznie odpowiednio 2, 2, 4 i 2.

**Przyczyny i przykłady.** m19, seed 1001: po 64 naprawach (76 usuniętych dekorów) osiągalne czapy 4351/9677; na z−1, z−2 i z−3 pozostaje 395, 52 i 15 kieszeni. Pierwszy zgłoszony cel czapy: (196,523,4). Reguła ostrożna roślinności daje wiele małych składowych i wyczerpuje limit. m19, seed 1003: 19 napraw, jaskinia 0/334, brak umieszczonych wejść/wyjść; nie ma drogi po Floor do składowej startu. Naprawa zgłasza `unrepaired`, nie kopie przez zabroniony Solid ani nie usuwa całej jaskini. Bez ponawiania `unrepaired` w 31/32 planów m19, z ponawianiem w 25/32.

**Pozostałe odchylenia i granice.** Dla zgodności starych wyników filtr wejść jest włączony razem z nową naprawą; `repairReach=0` zachowuje historyczne wejścia pod czapą. W starej macierzy sondy `SURFACE_CAVE_GUI_PASS` opcja repairReach była 1, więc jej wyniki zmieniają się świadomie; przy blokującym gruncie „mountain” naprawa może usunąć wszystkie wejścia i zgłosić `unrepaired`. Stare przypadki bez naprawy zachowują odciski i liczby poza szacunkami pamięci (nowe pola struktur). Adapter naprawia przed autoborderem, potem sprawdza pełną maskę rozwiązanych kafli i raportuje pozostałe blokady; nie ponawia wyszukiwania na końcowej masce obramowań. W złożonym przypadku sondy końcowy stan jest `unrepaired=true`, przy zgodnym z mapą modelu.

**Nie zweryfikowane:** wygląd mostów, przesiek i gór w GUI, ruch w grze, oryginalny RME, inne klienty, naprawa 2000×2000. Pełny raport, surowe wiersze sond i pomiary są w niecommitowanym `local-data/I37_IMPLEMENTATION_REPORT.md`.

## Iteracja 38 (2026-11-12)

Commity `504236b`, `bd6b921`. Roślinność czap jest dopuszczana tylko wtedy, gdy nie rozcina składowej (dokładny test punktu artykulacji, poza limitem 64 napraw). Wejścia do jaskiń wybierane przed czapami, z wcięciem 3×3 w C_1. Składowe jaskiń bez umieszczonego wejścia są zamieniane na Solid. Build, 18/18 testów, obie sondy PASS, `SUITE_PASS` w głównym checkoucie. Odcisk Seed 1729 100×100 = 17159159863505677800 bez zmian.

| Profil | Pełne seedy (1 próba) | Krawędzie Z | Naprawy | Usunięte POI | Mediana wejść | Flora czap z−1/z−2/z−3 | Usunięte jaskinie |
|---|---:|---:|---:|---:|---:|---:|---:|
| m06 | 32/32 | 774/774 | 3 [0..7] | 0,04510 % | 2 | 97,4 / 93,0 / 89,0 % | 0 |
| m19 | 32/32 | 750/750 | 15 [6..23] | 0,06986 % | 3 | 94,7 / 92,0 / 80,1 % | 2 |

Kryteria (a), (b), (c), (d), (e), (g) spełnione; dwa przebiegi bajtowo identyczne. **(f) nie spełnione:** mediana wejść 2 i 3 (wymagane ≥8). Przyczyna leży przed czapami: selektor RockFace z iteracji 29 (Rock z trzech stron, otwarte południe, budżet 6800 prób, odstęp 24) znajduje mało kandydatów, a dopasowanie wyjścia odrzuca wejścia z roślinnością w miejscu docelowym. Seed m06 1013 nie ma wejść już przy `repairReach=0`. Bramka G5: **FAIL tylko na (f)**.

**Nie zweryfikowane:** wygląd w GUI, ruch w grze. Raport: niecommitowany `local-data/I38_IMPLEMENTATION_REPORT.md` w worktree.

## Iteracja 39 (2026-11-12)

Commity `bee437a`, `4b4c75a`. Po autoborderze adapter buduje maskę blokad z faktycznych przedmiotów i naprawia ponownie (łączny limit 64, przez DrawOperations, w istniejących krokach Undo). Selektor wejść do jaskiń znajduje więcej kandydatów. Build, 18/18 testów, obie sondy PASS, `SUITE_PASS` w głównym checkoucie; `FINAL_MASK_REPAIR_E2E_PASS` (seed 1212, Undo/Redo).

| Profil | Pełne seedy (1 próba) | Krawędzie Z | Naprawy | Usunięte POI | Mediana wejść | Flora czap z−1/z−2/z−3 |
|---|---:|---:|---:|---:|---:|---:|
| m06 | 32/32 | 1808/1808 | 2 [0..5] | 0,04040 % | 18,5 | 97,5 / 93,2 / 86,2 % |
| m19 | 32/32 | 2737/2737 | 14,5 [6..22] | 0,04465 % | 33 | 94,7 / 91,5 / 79,2 % |

Kryteria (a)–(g) spełnione, dwa przebiegi bajtowo identyczne: **G5_GATE_PASS** dla osiągalności i poziomów. Zdiagnozowany bloker zastępczego przypadku: krawędź „shallow water” (border morza, przedmiot blokujący) na trawie, niewidoczna dla naprawy sprzed autoborderu; jest teraz usuwana.

**Nie zweryfikowane:** dokładny przypadek ze zrzutu (91 miejsc, (1149,1026,7)) nie został odtworzony, bo nieznane były pozostałe ustawienia; wygląd w GUI po usunięciu krawędzi wody; ruch w grze.

## Iteracja 40 (2026-11-12)

Przyczyna: semantyczny Dirt pozostający we wnętrzu wyspy poza zasięgiem pierścieni brzegu; nie dekor ani ścieżka. Przed poprawką w korpusie m06 było 15 wysp, 21 komórek Dirt i 122 Grass; m19: 8 wysp, 107 Grass. Rdzeń po POI i brodach, przed maską zapisu, dekorem, czapami i naprawą zamienia niedostępne wyspy bez POI na Shore. Główna składowa (największa, remisy wierszami), połączenia lądowe, POI oraz składowe dotykające granicy prostokąta lub Preserve są zachowane. Odcisk Seed 1729 bez zmian: 17159159863505677800 (brak takich wysp).

Pomiar jak w iteracji 39, seedy 1001–1032, bok 512, reguła ostrożna dekoru. Wyniki jednakowe bez ponawiania i z limitem 8 prób; oba przebiegi stdout/stderr bajtowo identyczne.

| Profil | Pełne seedy | Krawędzie Z | Naprawy, mediana [min..max] | Usunięte POI | Mediana wejść | Flora czap z−1/z−2/z−3 | Wyspy | Wyspy nie tylko Shore / komórki inne niż Shore |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| m06 | 32/32 | 1808/1808 | 2 [0..5] | 0,04040 % | 18,5 | 97,5 / 93,2 / 86,2 % | 15 | 0 / 0 |
| m19 | 32/32 | 2737/2737 | 14,5 [6..22] | 0,04465 % | 33 | 94,7 / 91,5 / 79,2 % | 8 | 0 / 0 |

**G5_GATE_PASS**, kryteria (a)–(g) bez regresji. Kanoniczny build Windows x64, przebudowane cele rdzenia, 18/18 testów, obie sondy PASS, **SUITE_PASS**. Niezależny DFS: 120/120 planów; ręczne przypadki: dirt, bród/most/ścieżka, POI, Preserve, granica, największa składowa, inne piętro i metadane mostu. Stare asercje szerokości plaży i różnic po dodaniu brodu uwzględniają normalizację wysp; sonda brodu nadal sprawdza chodliwość, połączenia i Undo/Redo (443 komórki brodu, 7 połączonych składowych, zero blokad). Szacunek pamięci roboczej uwzględnia klasyfikację wysp. Pełne wyniki testów i sond oraz ograniczenia: niecommitowany `local-data/I40_IMPLEMENTATION_REPORT.md`.

**Nie zweryfikowane:** dokładny przypadek użytkownika (brak ustawień), wygląd w GUI, ruch w grze, inne klienty niż 10.98, wczytanie w oryginalnym RME, profilowanie dużych obszarów.

## Iteracja 41 — dalekie wyjścia jaskiń i korytarze z drzwiami (2026-11-12): PASS w pomiarach i sondach

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 41”, bez zmian. Rdzeń `561cf88`, dopasowanie ciasnych korytarzy `071b4ad`, zachowanie odległości po połączeniu składowych `c7cb069`, adapter/GUI `0540d28`. `CaveExit::distant` oznacza rampę bez wejścia/dziury na powierzchni. Wieloźródłowy BFS od wszystkich wejść, odległość ≥24, remisy (y,x); krótka jaskinia dostaje przedłużenie tunelu szerokości 3 o 24..48 kroków Manhattan. Przedłużenie łączące składowe sprawdza wszystkie ich wejścia i zachowanie odległości także wcześniej umieszczonych dalekich ramp. Korytarze długości 3, szerokości 1, drzwi pośrodku i Solid prostopadle; zwężenie nie rozcina pozostałego Floor. Niemożliwy układ z ≥2 salami jest odrzucany, zamiast zwrócenia planu bez wymaganego korytarza. `GroundProfile::caveDoor` = `stone wall`, zwykłe zamknięte drzwi bez klucza z danych 10.98, oba kierunki, bez ID w kodzie. Opcja drzwi domyślnie wyłączona; brak odpowiedniego pędzla wyłącza ją z komunikatem. Dalekie wyjścia domyślnie włączone przy jaskini. Maska adaptera traktuje wyznaczone drzwi jaskini jako otwieralne; pozostałe blokery i dotychczasowe drzwi budynków zachowują poprzednie zachowanie.

Pomiar `g5_measure_levels.py --i41`: seedy 1001–1032, bok 512, ustawienia iteracji 39, oba nowe mechanizmy włączone. Osobno bez ponawiania i z limitem 8 prób; wyniki jednakowe. Każdy wariant dwa razy: stdout/stderr bajtowo identyczne, stdout zawiera odcisk całego LevelPlan (piętra, chodliwość, dekor, krawędzie Z i POI).

| Profil | Pełne seedy | Składowe z dalekim wyjściem | Mediana udziału | BFS średnia / minimum | Seedy z drzwiami | Drzwi z błędnym sąsiedztwem | Chodliwe krawędzie Z |
|---|---:|---:|---:|---:|---:|---:|---:|
| m06 | 32/32 | 32/32 | 100 % | 174,9375 / 35 | 32/32 | 0/32 | 1840/1840 |
| m19 | 32/32 | 32/32 | 100 % | 179,125 / 91 | 32/32 | 0/32 | 2769/2769 |

Bez nieosiągalnych POI i `unrepaired`; kieszenie jaskini/czap = 0. Naprawy: m06 2 [0..5], m19 14,5 [6..22]; usunięte czapy 0, usunięte POI odpowiednio 0,04014 % i 0,04444 %. Mediana wejść 18,5 / 33. Roślinność czap względem powierzchni z−1/z−2/z−3: m06 97,5/93,2/86,2 %, m19 94,7/91,5/79,2 %. Wyspy: 15 / 8; wyspy nie tylko Shore i komórki inne niż Shore = 0. **G5_GATE_PASS**.

Status kryteriów:

1. **PASS:** testy ręczne rozdzielnych składowych i krótkiej jaskini, odmowa pod czapami, rezerwacje, wyjście bez nowej dziury; 120 niezależnych BFS i determinizm.
2. **PASS:** mediana udziału 100 % w obu profilach, każda odległość ≥24.
3. **PASS:** opcja domyślnie wyłączona, pędzel zapisany w `GroundProfile::caveDoor`; sonda sprawdza oba stany dostępności kontrolki i zamknięte zwykłe drzwi bez klucza. Test rdzenia sprawdza odmowę niemożliwego układu.
4. **PASS:** 32/32 seedów w każdym profilu, każde drzwi z dwoma przeciwnymi sąsiadami. `CAVE_FAR_EXIT_DOOR_GUI_PASS farExits=1 doors=1 maskDoors=1 undoRedo=4 brush=stone wall`: przedmiot na mapie, sąsiedztwo i chodliwość w rzeczywistej masce adaptera.
5. **PASS:** 32/32 w obu profilach bez ponawiania i z ≤8 próbami, wszystkie cele osiągalne, 100 % krawędzi Z. Stara sonda pięciu pięter: `LEVEL_REACHABILITY_E2E_PASS`, 0 rozbieżności modelu z mapą, floorchange 112/112.
6. **PASS:** rzeczywisty dialog Preview/Validate/Place, cztery Undo do pustej mapy, cztery Redo z identycznymi stosami w zakresie z=4..8; nowy znacznik sondy powyżej.
7. **PASS:** odcisk Seed 1729 100×100 = 17159159863505677800 bez zmian, kanoniczny build Windows x64, osobno przebudowane cele testów i style_render (D25), 18/18 testów, obie sondy PASS, `SUITE_PASS`. Dwa przebiegi pomiaru identyczne. Końcowe sondy: `suite-commit-20261112-220835` i `suite-surface-20261112-220835`; dane klienta niezmienione, tylko 99 znanych ostrzeżeń SetFocus w sondzie powierzchni, bez innych wpisów stderr.

**Decyzje i odchylenia.** Bez zmiany kryteriów ani progów. Korytarz ma stałą dozwoloną długość 3; długość celu przedłużenia tunelu liczona w metryce Manhattan, potem obowiązkowo ponowny BFS od wejść. Obie komórki powierzchni (głowa i przybycie) są dodatkowo wymagane jako otwarty grunt. Historyczne sondy relacji rampa=wejście jawnie wyłączają nowe opcje; szacunki pamięci uwzględniają nowe pola i warstwy. Regresję zbyt szerokiego wyjątku chodliwości drzwi budynków wykryła stara sonda i poprawiono ją przed końcową serią.

**Nie zweryfikowane:** wizualna ocena użytkownika, ruch i otwieranie drzwi na serwerze gry, inne klienty niż 10.98, oryginalny RME, obszar 2000×2000 z nowymi opcjami, wszystkie awarie alokacji (D3). Pełny raport i surowe wyniki w niecommitowanym `local-data/I41_IMPLEMENTATION_REPORT.md` i `local-data/i41-measurement/`.

## Iteracja 42 — przełęcze w górach (2026-11-12): PASS w pomiarach i sondach

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 42”, bez zmian. Rdzeń: `4c8fc93`, adapter/GUI: `c18c7e7`. Nowe `passes` 0..8 (domyślnie 0), kontrolka „Mountain passes” w Levels. Masyw: 4-spójny Rock, ≥400 komórek. Przeciwległe końce przekrojów co 3 komórki, następnie dwa BFS: obejście i przejście przez cały dopuszczalny masyw; obejście ≥2× przejście. Dijkstra: krok `1+4*liczba_czap`, zakręt +6, co najmniej 3 kroki proste między zakrętami, remisy (y,x). Pas szerokości 3, bez dekoru; wycięcie wszystkich czap z dodatkową komórką odstępu. Po każdym cięciu ponowny wybór wejść oraz przebudowa czap/ramp istniejącymi selektorami z dotychczasowym minimum pola. Bez pędzla Path adapter wybiera Dirt. Po autoborderze usuwa blokujące bordery/dekor przez DrawOperations i sprawdza cały pas.

Pomiar `py -I docs/procedural/smoke/g5_measure_levels.py --i42`: seedy 1001–1032, bok 512, ustawienia iteracji 41, oba profile i passes=0/4. Każdy wariant dwa razy bez ponawiania oraz z limitem 8 prób; stdout/stderr bajtowo identyczne, odcisk całego LevelPlan w stdout. Wyniki bez ponawiania i z ≤8 próbami jednakowe:

| Profil | passes | Pełne seedy | Przełęcze mediana / suma | Maks. przez / obejście | Blokady pasa / pod czapą / małe czapy | Chodliwe krawędzie Z |
|---|---:|---:|---:|---:|---:|---:|
| m06 | 0 | 32/32 | 0 / 0 | — | — | 1840/1840 |
| m19 | 0 | 32/32 | 0 / 0 | — | — | 2769/2769 |
| m06 | 4 | 32/32 | 4 / 116 | 0,5 | 0 / 0 / 0 | 1878/1878 |
| m19 | 4 | 32/32 | 4 / 92 | 0,5 | 0 / 0 / 0 | 2809/2809 |

**G5_GATE_PASS** dla obu ustawień passes, bez nieosiągalnych celów ani `unrepaired`. Zachowane kryteria wcześniejszych iteracji: mediana wejść ≥8, roślinność czap ≥70% powierzchni, usunięte POI ≤2%, niedostępne wyspy wyłącznie Shore, dalekie wyjścia i drzwi jaskini. Szczegóły kategorii, kieszeni i napraw w `local-data/i42-measurement/summary.json`.

Status kryteriów:

1. **PASS:** ręczny masyw, rozcięcie czapy, minimum pola, rampa na pasie usunięta i wybrana ponownie; sonda potwierdza grunt chodliwy, Dirt bez pędzla Path i pustą przestrzeń nad pasem.
2. **PASS:** BFS rzeczywistych tras, Dijkstra przez masyw, ograniczenie liczby, domyślne 0; count=9 odrzucany, anulowanie i determinizm w testach.
3. **PASS:** mediana 4 w obu profilach; każda końcowa trasa ≤0,5 obejścia sprzed danego cięcia (wymagane ≤0,6).
4. **PASS:** zero blokad, zero pokrycia czapą i zero składowych czap poniżej minimum; sonda sprawdza 261 komórek w końcowej masce adaptera i na zapisanej mapie.
5. **PASS:** wszystkie 32 seedy, wszystkie kategorie POI i krawędzie Z osiągalne, passes=0 i passes=4, bez ponawiania i z ≤8 próbami.
6. **PASS:** `MOUNTAIN_PASS_GUI_PASS cells=261 farExits=1 doors=1 maskDoors=1 undoRedo=4`; rzeczywisty dialog Preview/Validate/Place, cztery Undo do pustej mapy i cztery Redo z identycznymi stosami z=4..8. Brak przedmiotów nad pasem na z−1, z−2 i z−3. Dodatkowo Dirt przy braku pędzla Path z wyłączoną naprawą.
7. **PASS:** kanoniczny build Windows x64, osobna przebudowa style_render i 18 celów (D25), 18/18 testów, obie sondy PASS, **SUITE_PASS**. Seed 1729 100×100 przy passes=0: **17159159863505677800**, bez zmian. Końcowe sondy: `suite-commit-20261112-225322` i `suite-surface-20261112-225322`; dane klienta niezmienione, 100 znanych ostrzeżeń SetFocus w sondzie Surface, zero innych wpisów stderr.

Dodatkowa kontrola zgodności passes=0: niezależnie skompilowane źródła narzędzia i rdzenia z `af333e6` w niecommitowanym local-data; cały stdout i stderr dla obu profili, 32 seedów i retry=0/8 identyczny z nowym narzędziem (`PASSES_ZERO_BASELINE_BYTE_IDENTICAL`, `local-data/i42-baseline/comparison.json`).

**Decyzje i odchylenia.** Kryteria, progi i parametry pomiaru niezmienione. Obejście mierzone przed konkretnym cięciem na piętrze powierzchni, bez skrótów Z; końcowa trasa na modelu levelReachabilityOf po dekoracji i naprawie. Selektory ponownie wyznaczają całą listę wejść/ramp, nie tylko element na pasie. Przy passes=0 dochodzą bajty nowych pól w szacunkach pamięci; stare odciski i wyniki semantyczne pozostają bez zmian.

**Nie zweryfikowane:** ocena wizualna użytkownika, ruch na serwerze gry, inne klienty niż 10.98, oryginalny RME, obszar 2000×2000 z przełęczami, pełne profilowanie i wszystkie awarie alokacji (D3). Pełny raport: niecommitowany `local-data/I42_IMPLEMENTATION_REPORT.md`.

## Iteracja 43 — piętro i schody w budynkach, dług D27 część 1 (2026-11-13): PASS w pomiarach i sondach

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 43”, bez zmian. Rdzeń `c4edf9a`, adapter/GUI `0030c4d`, sonda i pomiar `8a12eef`. Nowe pola `SurfaceTuning::buildingStoreys` (domyślnie wyłączone) i `upperFloorPercent` (0..100, domyślnie 30); w zakładce Buildings: „Upper floors”, „Upper floor share (%)”, „Stairs brush”. Kwalifikuje się budynek z drzwiami, prostokątnym obrysem Floor i wnętrzem ≥ 16 komórek (ruiny i obozy nie). Jeden rzut na budynek (osobny strumień), wiersze wg pierwszej komórki. Piętro z − 1: ten sam obrys, podłoga z pędzla podłogi budynku, mury z pędzla murów, bez drzwi. Kolizja (czapa, lądowisko, otwarcie rampy, przełęcz albo inne piętro w odległości ≤ 1) zostawia budynek parterowym. Schody: kształt rampy (komórka S z przedmiotem floorchange, ogon, komórka wejścia za ogonem), S przy murze równoległym do kierunku, przybycie S + kierunek wewnątrz; wnętrze parteru bez S i ogona spójne z drzwiami; wybór losowy spośród dopuszczalnych pozycji. **Pędzel schodów: doodad „ramp”** z danych 10.98 (`GroundProfile::buildingStairs`, kształty jak wyjścia jaskini: rampa 1388–1395 z ogonem, na z − 1 otwarcie „stairs” z floorchange down), bez identyfikatorów w kodzie. Dane 10.98 nie mają doodadu pojedynczych schodów domowych z częścią na wyższym piętrze („wooden stairs”/„stairs” są tylko przedmiotami RAW). Bez pędzla opcja jest wyłączona z komunikatem. W `levelPlanOf` wnętrza pięter należą do piętra pierwszej czapy albo do osobnego piętra z − 1, nowa kategoria POI `Storey`, krawędź Z S ↔ przybycie. Naprawa usuwa nieosiągalne piętro albo piętro usuniętego budynku (`removedStoreys`).

Pomiar `py -I docs/procedural/smoke/g5_measure_levels.py --i43`: seedy 1001–1032, bok 512, ustawienia iteracji 42 (passes = 4), opcja wyłączona i włączona z domyślnym 30 %. Każdy wariant dwa razy bez ponawiania i z limitem 8 prób; stdout/stderr bajtowo identyczne, wyniki z ponawianiem i bez jednakowe.

| Profil | Piętra | Pełne seedy | Kwalifikujące / z piętrem | Udział (mediana) | Ze schodami | Kolizje przy wyborze / usunięte naprawą | Komórki pod czapą/przełęczą | Storey osiągnięte | Chodliwe krawędzie Z |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| m06 | wył. | 32/32 | — | — | — | — | — | 0/0 | 1878/1878 |
| m19 | wył. | 32/32 | — | — | — | — | — | 0/0 | 2809/2809 |
| m06 | wł. | 32/32 | 2942 / 895 | 30,4 % | 895/895 | 1 / 1 | 0 | 30357/30357 | 2773/2773 |
| m19 | wł. | 32/32 | 3902 / 1206 | 31,3 % | 1206/1206 | 0 / 1 | 0 | 41155/41155 | 4015/4015 |

Udział na seed: m06 18,3–44,6 %, m19 21,2–41,7 %. Pozostałe kategorie, naprawy i wskaźniki wcześniejszych iteracji identyczne przy opcji wyłączonej i włączonej (drzwi 3116 / 4132, usunięte POI 0,0401 % / 0,0592 %, mediana wejść 18 / 34,5, roślinność czap ≥ 81,5 % powierzchni, przełęcze mediana 4, zero blokad i pokryć, dalekie wyjścia 100 %, wyspy tylko Shore). **G5_GATE_PASS.** Szczegóły w `local-data/i43-measurement/summary.json`.

Status kryteriów:

1. **PASS:** opcja domyślnie wyłączona. Bajtowe porównanie `style_render` z rdzeniem i narzędziem z `4476731` (osobna kompilacja w niecommitowanym `local-data/i43-baseline/`): m06 i m19, passes 0 i 4, retry 0 i 8 — stdout i stderr identyczne poza nową linią kategorii `storeys 0/0`, także przy jawnym `buildingStoreys=0` (`STOREYS_OFF_BASELINE_BYTE_IDENTICAL`). Podłoga, mury bez drzwi, kolizje: testy rdzenia i sonda.
2. **PASS:** pędzel „ramp” w profilu (`buildingStairs`), parter: rampa i ogon na podłodze, wolna komórka wejścia; z − 1: otwarcie i przybycie na podłodze. Brak pędzla wyłącza opcję z komunikatem (sonda). Schody nie rozcinają parteru (sprawdzane przy wyborze i niezależnie w testach).
3. **PASS (model rdzenia):** każda komórka piętra osiągalna ze startu przez drzwi i schody (30357 i 41155 komórek); 32/32 seedów z opcją i bez, bez ponawiania i z ≤ 8 próbami.
4. **PASS:** mediana udziału 30,4 % i 31,3 % (pasmo 20..40 %), 100 % pięter ze schodami, 0 komórek pięter pod czapami i przełęczami.
5. **PASS:** `BUILDING_STAIRS_GUI_PASS adapterStairs=4 maskCells=116 doorCells=10 committedPairs=4 storeyWalls=106 undoRedo=2 dialogFloors=2 brush=ramp`: rzeczywista maska adaptera (komórki pięter i schodów otwarte), przedmiot schodów na z i otwarcie na z − 1 w podglądzie i na mapie, mury pięter, Commit, Undo do pustej mapy, Redo z identycznymi stosami z = 4..8; dialog: przełącznik, komunikat i Preview/Validate.
6. **PASS:** determinizm (dwa przebiegi identyczne), odcisk Seed 1729 100×100 = 17159159863505677800 przy wyłączonej opcji (test rdzenia), nowe testy rdzenia (`BUILDING_STOREY_CORE_PASS seeds=60`: ręczne siatki, kolizja z czapą, wspólne piętro z czapą, naprawa, odmowy, anulowanie, 60 wygenerowanych osad z niezależnym sprawdzeniem 3D), kanoniczny build Windows x64, osobna przebudowa style_render i 18 celów (D25), 18/18 testów, obie sondy PASS, **SUITE_PASS** (`suite-commit-20261113-073733`, `suite-surface-20261113-073733`; dane klienta niezmienione, 101 znanych ostrzeżeń SetFocus, zero innych wpisów stderr).

**Decyzje i odchylenia.** Kryteria i progi bez zmian. (a) Maska adaptera, jak od iteracji 41, traktuje zamknięte drzwi budynków jako blokujące; dlatego sonda sprawdza przejście start → drzwi → schody → piętro na rzeczywistej masce z otwartymi komórkami drzwi, a walidator w oknie dla budynków z murami zgłasza drzwi i piętra jako nieosiągalne i Place pyta o kontynuację. Z tego powodu sonda wykonuje w oknie Preview/Validate, a Commit/Undo/Redo na tym samym typie wyniku z `prepareTerrain`. Zmiana semantyki drzwi budynków poza zakresem (zapisane przy D27). (b) Odstęp 1 komórki od czap, lądowisk, otwarć i przełęczy na z − 1 (kryterium mówi o kolizji). (c) Piętro bez gór dostaje własną jednostkę z − 1 i sprawdzenie pustego z − 1 jak dla czap. (d) Opcja wymaga murów (pędzla muru); z włączoną opcją budowany jest świat wielopiętrowy także bez naprawy, jak przy przełęczach.

**Nie zweryfikowane:** ocena wizualna użytkownika (wygląd rampy „ramp” w budynku, mury piętra), ruch po schodach na serwerze gry, Place w oknie bez pytania o nieosiągalne drzwi, inne klienty niż 10.98, oryginalny RME, obszar 2000×2000 z piętrami, wszystkie awarie alokacji (D3). Pełny raport: niecommitowany `local-data/I43_IMPLEMENTATION_REPORT.md`.
### Poprawka po iteracji 43 (2026-11-13): drzwi w masce poziomów

Maska blokad adaptera traktowała zamknięte drzwi budynków jako blokujące (wyjątek był tylko dla drzwi korytarzy jaskini), więc walidator w oknie zgłaszał drzwi i piętra budynków jako nieosiągalne. Teraz każde drzwi z planu (`doors`) zrobione pędzlem drzwi i niezablokowane są chodliwe na każdym piętrze; sonda porównuje mapę z maską według tej samej reguły. Build, 18/18 testów, obie sondy PASS, `SUITE_PASS`. Pomiarów bramki nie powtarzano (rdzeń bez zmian; reguła ostrożna nie dotyczy drzwi). Nie sprawdzone: Place w oknie bez pytania o nieosiągalne miejsca.

### Poprawka po iteracji 43 (2026-11-13): naprawa zatrzymana na rampie w zamkniętej kieszeni

Zgłoszenie z GUI (seed 1729, Buildings 100, mury, drzwi, piętra 30 %, „ramp”, wyspa z rzekami): „Repairs: 0 … Unrepaired. Unreachable: 5913 places (first: 1508, 1053, 7)”. Dokładnego prostokąta ani ustawień zakładek Area/Terrain/Levels nie znamy, więc dokładnego przypadku nie odtworzono. W przeszukanych prostokątach wokół (1508, 1053) (512×512, 768×768, z roślinnością i bez) ten sam objaw wystąpił w (1300, 850) 512×512: 0 napraw, „Unrepaired”, 195 nieosiągalnych komórek czapy. Objaw jest taki sam przy wyłączonych piętrach, więc przyczyna nie leży w piętrach. Czapa ma jedyną rampę, której dolny koniec (komórka z przedmiotem rampy na z) leży w pasie powierzchni zamkniętym przez samą czapę i nieruszany brzeg obszaru (`surfaceKeepClear`). Żaden przedmiot tam nie blokuje, więc nie ma czego czyścić. `repairLevelReach` (świat) bierze cele w kolejności (piętro, komórka), więc dolny koniec rampy na z był celem przed komórkami czapy. Dla takiego celu (nie Floor, nie wejście jaskini) przerywała całą naprawę jako „unrepaired”, a wszystkie dalsze cele zostawały bez obsługi: drzwi i budynki odcięte rzekami (normalnie usuwane jako „removed places”), czapy, piętra. Tak wyjaśniamy 0 napraw i 0 usuniętych miejsc przy tysiącach nieosiągalnych miejsc w zgłoszeniu (to wniosek, nie odtworzenie).

Poprawka (`18113e6`): gdy taki cel jest dolnym końcem rampy pierwszej czapy, naprawa usuwa tę rampę. Czapę bez dostępu usuwa w kolejnym przejściu dotychczasowe usuwanie czap (`removedCaps`). Kryteria iteracji 36–40 i 43 bez zmian, walidacji nie wyłączono. Inne cele bez legalnej naprawy nadal kończą się „unrepaired”. Test rdzenia „ramp into sealed pocket” w `test_procedural_reachability` (bez poprawki FAIL). Sonda `STOREY_SEALED_RAMP_REPAIR_PASS` (`0bb027f`, wymagana w `g5_run_suite.py`): seed 1729, (1300, 850) 512×512, piętra włączone, wszystko osiągalne, 2 naprawy, 1 usunięta czapa, 3442/3442 komórek pięter. Build, 18/18 testów, obie sondy PASS, SUITE_PASS, odcisk Seed 1729 100×100 = 17159159863505677800 bez zmian. Ten sam prostokąt po poprawce: z roślinnością 21867/21867, bez pięter 20050/20050. 768×768 w (1000, 600) z roślinnością: 49873/49873.

**Nie zweryfikowane:** dokładny przypadek z GUI (potrzebne: X, Y, szerokość, wysokość, profil stylu, długość ścieżek i brodów, przełęcze, poziomy gór, roślinność, liczba prób); Place w oknie; pomiary bramki (nie powtarzane); analogiczny przypadek wyjścia jaskini lądującego w zamkniętej kieszeni (nadal „unrepaired”).

## Iteracja 44 — nieprzechodnia skała w profilu m19, przypadek z GUI (2026-11-13): PASS

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 44”, bez zmian. Rdzeń `80e4de6`, adapter `8839e19` i `51263b7`, narzędzia pomiaru `9cfd432`, sonda `69b3468`.

**Adnotacja do wcześniejszych wyników m19 (iteracje 39–43).** Wszystkie pomiary bramki m19 z iteracji 39–43 (`style_render`, model offline) liczono przy przechodniej skale: pędzel skały m19 „mountain” ma w kliencie 10.98 nieprzechodnie podłoże (919), czego model offline nie widział; reguła ostrożna obejmowała tylko dekor. Tamte liczby m19 (np. 32/32, 4132 drzwi w iteracji 43) opisują więc teren, w którym skała jest do przejścia, a nie rzeczywistą mapę w GUI. m06 („mountain top ground”) jest przechodnia i tego nie dotyczy.

Zmiany: `SurfaceTuning::rockBlocks` (domyślnie wyłączone; poza profilem). Adapter ustawia go w przygotowaniu i walidacji z flag klienta wariantów podłoża pędzla skały (`withRockPassability`; żaden wywołujący nie podaje go ręcznie). Rdzeń przy `rockBlocks`: budynki, obozy, ruiny i wejścia do jaskiń tylko na lądzie połączonym ze startem (`startLandOf`, BFS z drzwiami/przerwami, skała zamknięta; budynek musi dotykać tego lądu, wejście — mieć tam pole przed sobą), rampy czap tylko na tym lądzie (także przy przełęczach i bez świata poziomów), wejście w ścianie chodliwe w `levelPlanOf`. Naprawa: cel na powierzchni bez ścieżki usuwa jedną naprawą wszystkie POI swojej składowej (budynki, obozy, wejścia, rampy czap), `removedPois` liczone jak dotąd.

Status kryteriów:

1. **PASS:** wejście w ścianie przy nieprzechodniej skale stoi na gruncie pędzla ziemi z profilu (`profile.brushes[1]`, wariant bazowy, bez identyfikatorów w kodzie); przedmiot wejścia zostaje. Krawędzie skały na wejściu i na polu przed nim adapter czyści jak na przełęczach. W przypadku z GUI wszystkie krawędzie Z chodliwe w masce adaptera (sonda, `allReached` i `walkableEdges == edges`); sonda jaskiń z `rock=mountain` (`SURFACE_CAVE_GUI_PASS`) przechodzi.
2. **PASS:** test rdzenia `IMPASSABLE_ROCK_PASS seeds=12 doors=389 entrances=53 removedPois=15` (każde drzwi i pole przed wejściem na lądzie startu, determinizm), ręczne siatki: POI za pasem skały usunięte, wejście z polem na lądzie startu zostaje.
3. **PASS:** test rdzenia „one repair removes every POI of a cut-off area” (3 POI, 1 naprawa); istniejące testy naprawy bez zmian.
4. **PASS:** `M19_ROCK_GUI_CASE_PASS floors=5 rockBlocks m06=0 m19=1 status=Reachable: 24032/24032 places on 5 floors. Seed 1729, 1 attempts. Repairs: 20, …, removed places: 20.` — dokładny przypadek przez prawdziwe okno (fixture m19, x 1000, y 1000, 512×512, 10 wejść, 3 poziomy gór, budynki 100 z murami, Normal door, piętra 30 % „ramp”). Przed zmianą: 28523/33736, 64 naprawy, „Unrepaired”, 5213 nieosiągalnych. Znacznik wymagany w `g5_run_suite.py`.
5. **PASS:** `py -I docs/procedural/smoke/g5_measure_levels.py --i44 --rock-blocks m06=0 m19=1` (przechodność z sondy; ustawienia iteracji 43, passes = 4, piętra 30 %), `G5_GATE_PASS`. Każdy wariant dwa razy, bez ponawiania i z limitem 8 prób; wyniki identyczne.

| Profil | Skała | Pełne seedy | Chodliwe krawędzie Z | Drzwi | Mediana wejść | Storey osiągnięte | Usunięte POI naprawą (udział) | Mediana napraw |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| m06 | przechodnia | 32/32 | 2773/2773 | 3116 | 18 | 30357/30357 | 2 (0,04 %) | 2 |
| m19 | nieprzechodnia | 32/32 | 2508/2508 | 2929 | 17 | 28880/28880 | 0 (0 %) | 11 |

POI odcięte skałą m19 nie powstają przy generowaniu (drzwi 2929 wobec 4132 w pomiarze iteracji 43 bez przechodności skały) i nie są liczone jako usunięte naprawą. Udział pięter m06 30,4 %, m19 30,5 %; przełęcze mediana 4, zero blokad; wyspy tylko Shore; roślinność czap ≥ 0,75 powierzchni. Szczegóły: niecommitowany `local-data/i44-measurement/summary.json`.
6. **PASS:** determinizm (pomiar i test rdzenia), odcisk Seed 1729 100×100 = 17159159863505677800 bez zmian (`rockBlocks` domyślnie wyłączone), build Windows x64, 18/18 testów rdzenia, **SUITE_PASS** (`suite-commit-20261113-164940`, `suite-surface-20261113-164940`; 103 znane ostrzeżenia SetFocus, zero innych wpisów stderr).

**Decyzje i odchylenia.** (a) Grunt pod wejściem: pędzel ziemi profilu zamiast gruntu jaskini — podgląd przyjmuje na powierzchni tylko grunty pędzli profilu (grunt jaskini dawał `InvalidItem` w sondzie jaskiń). (b) Fixture iteracji 39 (`FINAL_MASK_RAW_REGRESSION`) zachowuje strojenie m19, ale z przechodnim pędzlem skały („rocky ground”) i pierwszym seedem od 1212, którego autoborder nadal wymaga naprawy (1215): przy nieprzechodniej „mountain” żaden z 32 seedów nie potrzebował już tej naprawy, bo POI nie powstają za skałą. Przypadek nieprzechodniej skały pokrywa `M19_ROCK_GUI_CASE_PASS`. (c) Naprawa obszarem działa także przy przechodniej skale (kryterium tego nie ogranicza); m06 bez zmian w wynikach bramki. (d) Ląd startu liczony przed przełęczami: ląd połączony dopiero przełęczą traci POI (ostrożnie). (e) Wyjścia jaskiń mogą łączyć odcięty ląd, ale filtr ich nie uwzględnia (ostrożnie).

**Nie zweryfikowane:** ocena wizualna wejścia na gruncie ziemi w ścianie „mountain” i wyczyszczonych krawędzi, ruch na serwerze gry, Place/Undo/Redo w oknie dla przypadku z GUI (sonda robi Preview/Validate), inne klienty niż 10.98, oryginalny RME, wejście w ścianie bez świata poziomów (naprawa i przełęcze wyłączone, piętra wyłączone) — wtedy krawędzie skały na wejściu nie są czyszczone.

## Iteracja 45 — lochy pod budynkami, dług D27 część 2 (2026-11-13): PASS w pomiarach i sondach

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 45”, bez zmian. Rdzeń `690a75a`, adapter/GUI `94f4c4d`, sonda i pomiar `2b21fb3`. Nowe pola `SurfaceTuning::buildingDungeons` (domyślnie wyłączone) i `dungeonPercent` (0..100, domyślnie 20); w zakładce Levels (sekcja jaskiń, „Underground” z kryteriów): „Dungeons”, „Dungeon share (%)”, „Dungeon floor/wall/stairs brush”. Kwalifikacja jak przy piętrach (budynek z drzwiami, prostokątny obrys, wnętrze ≥ 16; funkcja `qualifyingBuildings` wspólna dla obu), jeden rzut na budynek z osobnego strumienia. Loch leży na piętrze jaskini z + 1: 2..6 pomieszczeń (wnętrza 4×4..9×9), każde następne doczepione do wcześniejszego prostym korytarzem długości 2..6 i szerokości 1..2 z drzwiami w murze obu pomieszczeń (jeden korytarz na bok pomieszczenia). Mury: pierścień (8-sąsiedztwo) komórek chodliwych. Odstępy: między pierścieniem muru lochu a pierścieniem ściany jaskini (Floor + pas ścian) oraz innego lochu co najmniej 2 puste komórki; niepołączone części jednego lochu ≥ 3 komórki; komórki chodliwe ≥ 2 od brzegu obszaru i nie pod komórką Preserve powierzchni. Brak miejsca: budynek bez lochu (`noSpace`). Zejście: komórka D we wnętrzu z D − kierunek na murze budynku i przybyciem D + kierunek we wnętrzu; wnętrze bez D (i bez schodów oraz ogona piętra z iteracji 43) spójne z drzwiami; D ani przybycie nie leżą na schodach, ogonie ani polu wejścia piętra. Na z + 1 schody w górę stoją na D, ogon, lądowisko i pole za ogonem w pomieszczeniu 0. **Pędzle (dane 10.98, bez identyfikatorów w kodzie):** podłoga „dark tiled stone floor” (`GroundProfile::dungeonFloor`), mury „dark stone wall” z jego zamkniętymi drzwiami normal bez klucza (`dungeonWall`), zejście/wyjście doodad „ramp” (`dungeonStairs`: na z + 1 rampa 1388–1395 z ogonem, na powierzchni otwarcie „stairs” z floorchange down jako grunt). W `levelPlanOf` pomieszczenia i korytarze to nowa kategoria POI `Dungeon` na piętrze jaskini, ich drzwi to POI `Door`, krawędzie Z: D (z) ↔ D (z + 1) i D (z + 1) ↔ przybycie (z). Naprawa usuwa nieosiągalny loch i loch usuniętego budynku (`removedDungeons`).

Pomiar `py -I docs/procedural/smoke/g5_measure_levels.py --i45 --rock-blocks m06=0 m19=1`: seedy 1001–1032, bok 512, ustawienia iteracji 44 (passes = 4, piętra 30 %, rzeczywista przechodność skały), opcja wyłączona i włączona z domyślnym 20 %. Każdy wariant dwa razy bez ponawiania i z limitem 8 prób; stdout/stderr bajtowo identyczne, wyniki z ponawianiem i bez jednakowe.

| Profil | Lochy | Pełne seedy | Chodliwe krawędzie Z | Kwalifikujące / z lochem | Udział (mediana) | Pomieszczenia (mediana, zakres) | Ze schodami / spójne | Kolizje z jaskinią / między lochami / złe drzwi | Dungeon osiągnięte | Drzwi osiągnięte |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| m06 | wył. | 32/32 | 2773/2773 | — | — | — | — | — | 0/0 | 3116/3116 |
| m19 | wył. | 32/32 | 2508/2508 | — | — | — | — | — | 0/0 | 2929/2929 |
| m06 | wł. | 32/32 | 3795/3795 | 2942 / 511 | 17,4 % | 4 (2..6) | 511/511 | 0 / 0 / 0 | 92216/92216 | 6104/6104 |
| m19 | wł. | 32/32 | 3420/3420 | 2759 / 456 | 16,2 % | 4 (2..6) | 456/456 | 0 / 0 / 0 | 81272/81272 | 5545/5545 |

Udział na seed: m06 12,0–25,3 %, m19 10,2–25,0 % (m19: 3 seedy bez kwalifikujących się budynków — 1016, 1023, 1030 — pominięte w medianie, jak przy piętrach). Wybrane bez miejsca: m06 57 z 568, m19 81 z 537; bez pozycji schodów 0; usunięte naprawą 0. Przy wyłączonej opcji wszystkie wskaźniki identyczne z iteracją 44 (drzwi 3116 / 2929, piętra 30357 / 28880, mediana wejść 18 / 17, usunięte POI 2 / 0). Przy włączonej: pozostałe kategorie bez zmian, usunięte POI 2 / 0 (udział liczony z drzwiami lochów w mianowniku), mediana wejść 18 / 17, przełęcze mediana 4, zero blokad, wyspy tylko Shore, roślinność czap ≥ 0,75 powierzchni. **G5_GATE_PASS.** Szczegóły: niecommitowany `local-data/i45-measurement/summary.json`.

Status kryteriów:

1. **PASS:** opcja domyślnie wyłączona. Bajtowe porównanie `style_render` z rdzeniem i narzędziem z `a229aa1` (osobna kompilacja w niecommitowanym `local-data/i45-baseline/`): ustawienia iteracji 44, m06 i m19, retry 0 i 8 — stdout i stderr identyczne poza nową linią kategorii `dungeons 0/0`, także przy jawnym `buildingDungeons=0` (`DUNGEONS_OFF_BASELINE_BYTE_IDENTICAL`). Pomieszczenia 2..6, wnętrza 4..9, korytarze 2..6 × 1..2, drzwi z dwoma przeciwnymi chodliwymi sąsiadami, odstęp od jaskini i innych lochów: testy rdzenia, pomiar i sonda.
2. **PASS:** zejście w budynku przy murze, wyjście w lochu dokładnie pod nim z lądowiskiem i polem za ogonem w pomieszczeniu 0; parter bez otwarcia spójny z drzwiami, bez kolizji ze schodami piętra (test rdzenia z piętrami i lochami w tych samych budynkach, 48 wygenerowanych światów, połowa z piętrami). Brak pędzla wyłącza opcję z komunikatem (sonda: mur bez drzwi „bridge”).
3. **PASS (model rdzenia i maska adaptera):** każda komórka lochu osiągalna ze startu przez drzwi budynku, zejście i korytarze (92216 i 81272 komórek); bramka 32/32 z opcją i bez, bez ponawiania i z ≤ 8 próbami. W sondzie wszystkie komórki lochu i drzwi otwarte w rzeczywistej masce adaptera, `allReached`, krawędzie Z chodliwe.
4. **PASS:** mediana udziału 17,4 % i 16,2 % (pasmo 10..30 %), mediana 4 pomieszczeń na loch, 100 % lochów z zejściem i wyjściem, 0 kolizji z jaskinią.
5. **PASS:** `DUNGEON_GUI_PASS adapterStairs=12 maskCells=2217 walls=1634 doors=70 committedStairs=9 committedWalls=1254 committedDoors=54 undoRedo=3 dialogFloors=3 brushes=dark tiled stone floor/dark stone wall/ramp`: mury z pędzla, zamknięte drzwi bez klucza (oba kierunki), rampa na z + 1 i otwarcie z floorchange down na z w podglądzie i na mapie, maska adaptera; przez prawdziwe okno Preview/Validate/Place, Undo do pustej mapy i Redo z identycznymi stosami z = 4..8.
6. **PASS:** determinizm (dwa przebiegi pomiaru identyczne, testy rdzenia), odcisk Seed 1729 100×100 = 17159159863505677800 (opcja wyłączona), nowe testy rdzenia (`BUILDING_DUNGEON_CORE_PASS seeds=48 qualifying=529 placed=180 rooms=683`: ręczne siatki, jaskinia pod budynkiem, piętra w tych samych budynkach, naprawa usuwa nieosiągalny loch i loch odciętego budynku, odmowy, anulowanie, 48 światów z niezależnym sprawdzeniem reguł i osiągalności 3D), kanoniczny build Windows x64, osobna przebudowa style_render i 18 celów (D25), 18/18 testów, obie sondy PASS, **SUITE_PASS** (`suite-commit-20261113-175919`, `suite-surface-20261113-175919`; dane klienta niezmienione, 104 znane ostrzeżenia SetFocus, zero innych wpisów stderr). D27 zaktualizowany w TECH_DEBT.md (część 2 zamknięta, opisana reszta).

**Decyzje i odchylenia.** Kryteria i progi bez zmian. (a) Loch wymaga włączonej jaskini (cave entrances > 0): piętro z + 1 jest rysowane jednostką jaskini i jej sprawdzeniem pustego z + 1; bez jaskini opcja jest nieaktywna z komunikatem, a rdzeń niczego nie umieszcza. (b) „Odstęp ≥ 2 komórek” liczony między pierścieniami murów (lochu oraz Floor jaskini z pasem ścian) — dwie puste komórki, czyli odległość Czebyszewa komórek chodliwych lochu od Floor jaskini ≥ 5; tak samo między lochami. (c) Drzwi na obu końcach korytarza (każde wejście do pomieszczenia). (d) Lochy wymagają budynków z murami i drzwiami (kwalifikacja jak w iteracji 43). (e) Udział usuniętych POI liczony przez dotychczasowy skrypt z drzwiami lochów w mianowniku; przy wyłączonej opcji identyczny z iteracją 44. (f) Pierwsza wersja sondy sprawdzała mury pędzlem gruntu (`brushOf`) i dawała fałszywy FAIL; poprawiono przed commitem na zestaw elementów pędzla muru.

**Nie zweryfikowane:** ocena wizualna użytkownika (wygląd „dark stone wall” bez elementów tjunction/diagonal, rampa „ramp” jako schody w budynku), ruch po schodach i otwieranie drzwi na serwerze gry, inne klienty niż 10.98, oryginalny RME, obszar 2000×2000 z lochami, lochy przy masce zaznaczenia (komórki poza maską są wykluczane przez Preserve, ale sonda tego nie sprawdza), wszystkie awarie alokacji (D3). Pełny raport: niecommitowany `local-data/I45_IMPLEMENTATION_REPORT.md`.

### Iteracja 45b — poprawki lochów po teście GUI (2026-11-13): PASS

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 45b”, bez zmian. Poprawka `98bfa08`, sonda `7078bdb`.

1. **Rogi lochów — diagnoza:** pierścień murów to 8-sąsiedztwo komórek chodliwych, a adapter kładł podłogę lochu pod każdą komórką pierścienia, także pod narożnikami stykającymi się z lochem tylko po przekątnej. RME stawia tam element słupka (25202) albo narożnika (25204) pędzla „dark stone wall”, który zostawia większość kafla odkrytą, więc podłoga była widoczna poza murem. Pędzel podłogi nie ma borderów (`friend all`), więc to nie border. **Poprawka:** komórka pierścienia bez sąsiada chodliwego po boku dostaje tylko element muru, bez gruntu (`caveFloor` 5); walidacja podglądu to sprawdza. W sondzie 0 kafli z podłogą lochu poza obrysem w podglądzie i na mapie (elementy na narożnikach: 25204×50, 25202×46, reszta to proste odcinki przy korytarzach).
2. **Zejście — diagnoza:** na powierzchni stała górna część doodadu „ramp”, grunt „stairs” (459), rysowany jako otwór, czyli czarna dziura. **Poprawka:** nowe pole profilu `GroundProfile::dungeonDescent` (pędzel gruntu albo doodadu, bez ID w kodzie); jego grunt z floorchange down zastępuje podłogę budynku. Domyślnie w oknie „gray stone stairs” (doodad z danych 10.98, pełny grunt schodów w dół; w danych 10.98 schody w dół są wyłącznie gruntami). Rampa w górę w lochu bez zmian; krawędź Z chodliwa w masce adaptera.
3. `DUNGEON_GUI_PASS … outsideFloor=0/0 surfaceOpenings=0/0` (podgląd/mapa, zero gruntu „stairs” na z 7), Place/Undo/Redo jak w iteracji 45. Pomiar `--i45` (rdzeń bez zmian): `summary.json` identyczny z pomiarem iteracji 45, `G5_GATE_PASS`, oba przebiegi identyczne; odcisk Seed 1729 bez zmian; 18/18 testów (cele przebudowane osobno), **SUITE_PASS** (`suite-commit-20261113-202336`, `suite-surface-20261113-202336`; 104 ostrzeżenia SetFocus, zero innych wpisów stderr).

**Nie zweryfikowane:** wygląd w GUI po poprawce (ocena użytkownika), kierunek grafiki „gray stone stairs” względem rampy w lochu, przejście po schodach na serwerze gry, który element trafia na który narożnik (sonda liczy elementy, nie ich położenie).
### Iteracja 45c — lochy: kierunek schodów, przejścia, mury budynku (2026-11-13): PASS

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 45c”, bez zmian. Poprawki `9bbc421` (rdzeń), `2b5224c` (adapter/okno), sonda `2ecad33`.

1. **Kierunek.** Rdzeń używał dowolnego z czterech kształtów doodadu „ramp”, a „gray stone stairs” ma jeden wariant. Teraz loch używa tylko kształtów zwróconych na północ (rampa z floorchange north, ogon i lądowisko na południe od niej), a zejście domyślnie to pędzel gruntu „stairs down” (grunt z floorchange down; w danych 10.98 to para dla schodów w górę na północ). Pole profilu `dungeonDescent` opisuje teraz schody zwrócone na północ.
2. **Przejścia — diagnoza:** drzwi stały, ale w slotach drzwi pędzla „dark stone wall” są przedmioty 25283–25296, które dane klienta opisują jako zwykły „stone wall” (bez typu door, nieprzechodnie). Logika drzwi RME postawiła je jako „drzwi”, więc na każdym końcu korytarza widać było pełny mur (a maska adaptera traktowała je jako otwieralne). **Poprawka:** `resolveCaveDoorBrush` przyjmuje tylko przedmioty będące w kliencie drzwiami; „dark stone wall” jest odrzucany (opcja wyłączona z komunikatem), domyślny mur lochu to „brick wall” (prawdziwe drzwi normal).
3. **Mury budynku — diagnoza:** przy nieprzechodniej skale (m19) adapter czyści blokery na każdej komórce wejścia i na polu na południe od niej (wejścia w ścianie skały, iteracja 44). Schody w dół lochu są zapisane jako komórka wejścia, a pole na południe od nich to mur budynku — był usuwany jako „czyszczalny” element muru. **Poprawka:** czyszczone są tylko wejścia w ścianie (przedmiot wejścia na gruncie), nie zejścia lochów.
4. `DUNGEON_GUI_PASS … committedDoors=64 outsideFloor=0/0 surfaceOpenings=0/0 missingWalls=0 brushes=dark tiled stone floor/brick wall/ramp/stairs down` (adapter ze skałą „mountain”): rampy na północ w podglądzie i na mapie, mur budynku na południe od każdego zejścia, wszystkie zaplanowane mury budynków w podglądzie, drzwi lochu to przedmioty drzwi klienta, „dark stone wall” odrzucony w oknie. Pomiar `--i45 --rock-blocks m06=0 m19=1`: `G5_GATE_PASS`, wyłączona opcja identyczna z iteracją 44; włączona: udział 16,3 % / 17,3 %, mediana 4 pomieszczeń, schody i spójność 486/486 i 458/458, zero kolizji i złych drzwi, Dungeon 90977/90977 i 83501/83501. Testy rdzenia (`BUILDING_DUNGEON_CORE_PASS` sprawdza kierunek północny), odcisk Seed 1729 bez zmian, 18/18, **SUITE_PASS** (`suite-commit-20261113-211146`, `suite-surface-20261113-211146`; 104 ostrzeżenia SetFocus, zero innych wpisów stderr).

**Nie zweryfikowane:** wygląd „stairs down” i „brick wall” w GUI (ocena użytkownika; grafiki nie dało się obejrzeć narzędziem — mapowanie sprite'ów z dostępnego Tibia.dat było niespójne), przejście w grze, sonda nie była uruchomiona na kodzie bez poprawki muru (błąd 3 zdiagnozowany z kodu; sonda sprawdza mur przy każdym zejściu przy nieprzechodniej skale).

## Iteracja 46 — porządki w długu D22–D27 (2026-11-14): PASS, D25 i D26 zamknięte, D24 i D23 zawężone

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 46”, bez zmian. Commity: `46bd778` (adapter: wariant „cave enter”), `9568922` (walidacja podglądu jaskini po nazwie pędzla), `61e5ad4` (sonda: wariant wejścia, lica ścian jaskini, przypadek D26), `860611f` (`g5_run_suite.py`: przebudowa celów, `local-data` z głównego checkoutu). Rdzeń bez zmian.

1. **D25 — PASS.** `g5_run_suite.py` listuje w `build-ninja` (`ninja -t targets all`) cele `style_render` i `test_procedural_*`, przebudowuje je przez `vcvarsall.bat` (znaleziony przez `vswhere`) i `cmake --build`, drukuje listę celów i liczbę kroków budowania (`rebuilt targets (19): style_render test_procedural_core …`, `rebuild exit=0 build steps=38`), a przy błędzie kończy się `SUITE_FAIL: core target rebuild failed` (sprawdzone: pierwsza wersja z błędnym cytowaniem ścieżki `vcvarsall.bat` zakończyła się tym komunikatem, bez uruchamiania testów). Względne `PROFILE_DATA` i `CONFIG`, których nie ma względem bieżącego katalogu, są brane z `--local-data`, domyślnie z `local-data` głównego checkoutu (pierwszy wpis `git worktree list --porcelain`); seria w worktree uruchomiona jako `--probes procedural-smoke/g0-reference-profile-20261030/data procedural-smoke/g0-reference-profile-20261030/config.toml`. Wyjście sond nadal w `local-data/procedural-smoke` danego checkoutu (wymóg `g1_run_resolver_probe.py`).
2. **D24 — (a) i (b) naprawione, (c) zmierzone, bez naprawy.**
   (a) Reguła RockFace (`caveEntranceAllowed`) wymaga otwartego pola na południe od wejścia, więc każde wejście w ścianie patrzy na południe. Adapter wybierał alternatywę doodadu po komórce i seedzie (`(e + seed) % 2`). Kierunek alternatyw ustalony z map referencyjnych (skaner OTBM w notatniku wykonawcy, 19 map M01–M19 i forgotten): przy schodach w ścianie z gruntem 459 przedmiot 8203 (pierwsza alternatywa „cave enter”) stoi 4 razy ze skałą 919 od północy, zachodu i wschodu i otwartym południem, 8212 (druga) 5 razy ze skałą od północy, południa i zachodu i otwartym wschodem; 8202 („cave enter3”, pierwsza) raz ze skałą od północy, zachodu i wschodu (bez kafla na południu). `CaveBrushes::entrance` niesie teraz tylko pierwszą alternatywę (wszystkie są nadal sprawdzane jako zwykłe przedmioty jaskini), walidacja podglądu przyjmuje tylko ją. Sonda: każde wejście RockFace ma otwarte południe, niesie pierwszą alternatywę i nigdzie nie ma drugiej (`caves`, `guiCaves`). Narzędzie `tibia_data.py` nie pozwoliło obejrzeć grafik: dla tego `Tibia.dat` daje niespójne sprite'y (np. 3031 jako pojemnik), więc kierunek opiera się na mapach referencyjnych, nie na grafice.
   (b) Walidacja podglądu jaskini rozwiązuje `caveGround` i `caveWall` z pól profilu po nazwie i sprawdza grunt Floor, ściany i ich elementy borderu względem tych pędzli; indeks 1 profilu nie jest już używany dla jaskini (indeksy zostają tylko dla płaskowyżu, poza zakresem).
   (c) `CAVE_WALL_FACES` w sondzie (256 × 256, „mountain”): RockFace — 3821 komórek ściany, elementy 873 × 611, 874 × 2385, 877 × 385; FloorHole — 3108 komórek, 873 × 522, 874 × 1801, 877 × 374. To rodzina borderu 3 pędzla „mountain” (lico wschodnie, południowe i narożnik SE). 1870 z 3821 (1522 z 3108) komórek ściany widzi podłogę tylko od północy lub zachodu i nie ma lica. W danych 10.98 pędzel „mountain” ma lica tylko w rodzinach 3 i 51 (`e`, `s`, narożniki `dne`, `dsw`, `dse`); rodzina 10 (`n`, `w`, …) to krawędź do innych gruntów, nie lico, a rodzina 29 jest opcjonalna. Brak wariantów lica północnego i zachodniego, więc zgodnie z kryterium bez naprawy.
3. **D23 — stan na HEAD.** Górny kafel rampy jako druga dziura: nieprawdziwe od iteracji 30 (brak przedmiotu „top”, sonda `checkItem(…, topIds)`). Wariant „cave enter”: naprawiony (punkt 2a). Pomiar wyjść bez `occupied`: nieprawdziwe dla pomiaru bramki (`style_render` w trybie poziomów buduje świat z dekoracją i podaje `occupied` do `placeCaveExits`); stary tryb `--cave-exits` (tabele iteracji 29–30) nadal liczy bez roślinności, opisane w kodzie. Naprawa nie zna wejść Z: nieprawdziwe od iteracji 36–37 (`levelReachabilityOf`, `repairLevelReach`). Pusty cały prostokąt z + 1 przy masce: prawdziwe i celowe — jaskinia jest generowana w całym prostokącie bez znajomości maski, a komórki poza maską nie są zapisywane, więc dopuszczenie zajętego z + 1 dawałoby nieciągłe jaskinie; wymaga zmiany generatora jaskiń, zostaje w D23. Jedno Undo zostawia jaskinię bez wejścia: prawdziwe (historia N kroków, kolejność płaskowyż, jaskinia, powierzchnia); to decyzja użytkownika „dwa kroki”, bez zmiany. Ruch w grze: niesprawdzalny bez serwera.
4. **D22, D26, D27.** D22 zamknięty (opis zaktualizowany). D26: `D26_RAMP_ENTRANCE_PASS runs=40 pairedRamps=404 farExitRamps=10` — prawdziwe okno z ustawieniami domyślnymi plus 10 wejść, fixture m06 i m19 (strojenie i palety profili g4), prostokąt 512 × 512 wokół (1018, 1237), powierzchnia na z 6 (rampy na z 7) i z 7, seedy 1729 i 1001–1004, dalekie wyjścia włączone (domyślnie) i wyłączone; każda rampa poza dalekimi wyjściami ma wejście bezpośrednio nad sobą, każde wejście ma rampę pod sobą, pole nad dalekim wyjściem ma grunt i nie blokuje, okno raportuje wszystkie miejsca osiągalne. Objaw nie występuje; D26 zamknięty jako nieodtwarzalny po zmianach iteracji 37–45. **Odkrycie:** przy m19 z ustawieniami domyślnymi (bez przełęczy, skała nieprzechodnia) we wszystkich 20 przebiegach w tym prostokącie naprawa usuwa wszystkie wejścia (11–28 usuniętych miejsc, osiągalnych 75–24276 miejsc), więc m19 nie daje tu żadnej rampy; pokrycie D26 dla ramp pochodzi z m06. D27 zaktualizowany zgodnie z 45c.
5. **PASS, bez regresji.** Pomiar `py -I docs/procedural/smoke/g5_measure_levels.py --i45 --rock-blocks m06=0 m19=1` na przebudowanym `style_render`: **G5_GATE_PASS**, `summary.json` bajtowo identyczny z pomiarem iteracji 45c (worktree `agent-a49bab52faf7e9ad8`); rdzeń bez zmian, więc wariant wejścia nie zmienia planu ani odcisku. Determinizm: oba przebiegi każdego wariantu identyczne (`identical: true`). Odcisk Seed 1729 100 × 100 = 17159159863505677800 (test `test_procedural_surface`). Wiersze `SURFACE_CAVE_E2E_PASS` i `SURFACE_CAVE_LAYOUT` z tymi samymi liczbami co w iteracji 30. Kanoniczny `build_ninja.bat` w worktree, 18/18 testów rdzenia po przebudowie z serii, obie sondy PASS, **SUITE_PASS** (`suite-commit-20261114-100028`, `suite-surface-20261114-100028`; seria przebudowała 19 celów — 0 kroków, bo poprzedni przebieg serii przebudował je w 38 krokach; dane klienta niezmienione, 144 znane ostrzeżenia SetFocus, zero innych wpisów stderr).

**Nie zweryfikowane:** grafika obu alternatyw „cave enter” (kierunek ustalony z map referencyjnych, nie z obrazu), wygląd ścian jaskini bez lic północnych i zachodnich w GUI, przyczyna oryginalnego objawu D26 (dane z sesji użytkownika nieznane; sprawdzono tylko opisane kombinacje), rampy D26 dla m19 przy ustawieniach domyślnych (nie powstają), ruch w grze, inne klienty niż 10.98, oryginalny RME.

### Poprawka po iteracji 46 — m19 bez jaskiń w GUI (2026-11-14): PASS

Zgłoszenie użytkownika (HEAD `59ee144`): m19, x 1000, y 1000, 512 × 512, seed 1729, 10 wejść w ścianie skały, reszta domyślna — brak jaskiń. Kryteria bez zmian. Poprawka `dfa7a11` (adapter), sonda `36b1b7e`.

1. **Odtworzenie (prawdziwe okno):** `M19_CAVES_GUI_CASE candidates=771 coreEntrances=40 startLandEntrances=20 mapEntrances=0 withRamp=0 caveCells=0 status=Reachable: 21616/21616 places on 3 floors. Seed 1729, 1 attempts. Repairs: 20, bridges: 0, removed caps: 0, removed places: 20.` — 771 komórek spełnia złagodzoną regułę RockFace, rdzeń stawia 40 wejść, filtr lądu startu (iteracja 44) zostawia 20, naprawa w adapterze usuwa wszystkie 20 razem z jaskiniami.
2. **Przyczyna:** maska blokad adaptera (`terrain_preparation.cpp`) brała grunt rozwiązany dla rodzaju Rock, czyli nieprzechodnie podłoże „mountain” (919), także na komórce wejścia w ścianie, choć adapter kładzie tam później chodliwy grunt ziemi (iteracja 44). Każde takie wejście było więc nieosiągalne, a naprawa nie może ścieżki przez grunt blokujący (koszt 0) i usuwa wejście. Offline (`style_render`, bez maski z klienta) ten sam seed ma 34 wejścia, stąd mediana 17 w pomiarze bramki. **To nie regresja iteracji 46:** wiersz sondy iteracji 44 (`380b522`) miał już `Repairs: 20, … removed places: 20` (opisane wtedy jako usunięte miejsca); `46bd778` zmienia tylko identyfikator przedmiotu wejścia, a maska go nie czyta. Błąd jest od iteracji 44, gdy skała m19 stała się nieprzechodnia.
3. **Poprawka:** przy nieprzechodniej skale, stylu RockFace i dostępnym chodliwym gruncie ziemi maska nie blokuje komórek wejść w ścianie (te same warunki co położenie gruntu ziemi). Po poprawce: `mapEntrances=20 withRamp=20 caveCells=12284`, `Reachable: 33941/33941 … Repairs: 0, … removed places: 0`, znacznik `M19_CAVES_GUI_PASS` wymagany w `g5_run_suite.py`. Przypadek z iteracji 44: `Reachable: 36357/36357 places on 5 floors … Repairs: 0, removed places: 0` (wcześniej 24032 i 20 usuniętych). `D26_RAMP_ENTRANCE_PASS runs=40 pairedRamps=740 m19PairedRamps=336 farExitRamps=20` (wcześniej m19 bez ramp; teraz wymagane > 0).
4. Bez regresji: `g5_measure_levels.py --i45 --rock-blocks m06=0 m19=1` — **G5_GATE_PASS**, `summary.json` bajtowo identyczny z iteracją 45c/46 (rdzeń bez zmian, pomiar offline nie używa maski adaptera); odcisk Seed 1729 bez zmian (test rdzenia), 18/18, **SUITE_PASS** (`suite-commit-20261114-105147`, `suite-surface-20261114-105147`; 145 ostrzeżeń SetFocus, zero innych wpisów stderr).

**Nie zweryfikowane:** wygląd i ruch w grze, pozostałe miejsca, w których maska z klienta może się różnić od modelu offline (pomiar bramki nadal jej nie używa), stylu FloorHole przy nieprzechodniej skale (nie dotyczy: dziura stoi na trawie lub ziemi).

## Iteracja 47 — wygląd jaskiń (2026-11-15): PASS w pomiarach i sondach

Kryteria: [G5_METRICS.md](G5_METRICS.md), „Iteracja 47”, bez zmian. Gałąź `fix/procedural-g5-cave-shape` z `cc8c06d`. Commity: `cd5ceb0` (rdzeń: nowe `generateCave`), `007a0fc` (testy kształtu), `58674c1` (`style_render`: wiersz `level cave i47` i obraz `<seed>-cave.png`; `g5_measure_levels.py --i47`), `d1b55bb` (pętle z dala od innych tuneli), `2488bf4` (test: pętla w każdej jaskini z ≥ 4 wejściami), `5be96e1` (pomiar par równoległych wzdłuż cięciw).

1. **Graf połączeń — PASS.** Wejścia (posortowane i bez powtórzeń, tylko po to, by wynik nie zależał od kolejności) łączy drzewo rozpinające o najmniejszej długości (Prim po kwadracie odległości, remis do niższego indeksu komórki). Przy ≥ 4 wejściach 1 pętla, przy ≥ 8 — 2: pary spoza drzewa nie dłuższe niż 1,5 × najdłuższa krawędź drzewa, wybierane według stosunku drogi po drzewie do odległości prostej. Pętla musi odchodzić od obu końców ≥ 35° od tuneli w tym wejściu, a jej punkty 1/4, 1/2 i 3/4 leżą ≥ 1/5 jej długości od pozostałych zaplanowanych tuneli. Bez tego warunku pomiar znalazł pętlę biegnącą ok. 30 pól obok krawędzi drzewa (m06, 1 para) — stąd `d1b55bb`. Układ bez takiej pary dostaje mniej pętli (w pomiarze nie wystąpiło: 64 pętle na 32 seedy m06, 57 w m19; test: pętla w 24/24 jaskiniach z ≥ 4 wejściami).
   Test rdzenia: krata 4 × 4 wejść (łączenie wierszami dałoby cztery równoległe tunele przez obszar) — identyczna jaskinia, `rooms` i połączenia po odwróceniu i przestawieniu wejść; długość krawędzi drzewa równa MST z niezależnego algorytmu Kruskala; brak połączenia końca wiersza z początkiem następnego; 2 pętle.
   Pomiar par równoległych (`style_render`): osie, których cięciwa ma ≥ 40 % boku; para jest „obok siebie”, gdy cięciwy różnią się o < 20°, co najmniej połowa osi krótszej leży wzdłuż cięciwy dłuższej i te komórki są średnio < 32 pola od niej. **0 par** w m06 i m19. Najdłuższy prosty odcinek osi (najdłuższa cięciwa kolejnych komórek osi, z których każda leży ≤ 1 pole od cięciwy): **m06 ≤ 20, m19 ≤ 16** (kryterium ≤ 20); test rdzenia 256 × 256, 24 seedy: 15.
2. **Tunele — PASS.** Oś z punktów co 7–10 pól wokół łuku o wychyleniu do min(L/5, 28) pól (połowa do całości, strona losowa, odwrócona przy krawędzi obszaru), plus boczne wahnięcie do min(max(L/10, 2), 6) pól na punkt; każde załamanie co najmniej 4 pola od cięciwy sąsiadów (przesuwanie punktu na boki, potem dwa przejścia poprawiające), odcinki dłuższe niż 10 pól dzielone na kawałki ≤ 8 z załamaniem 2,5 pola na przemian. Punkty trzymają 3 pola od krawędzi (wychylenie skracane, nie obcinane — wcześniej obcięte punkty tworzyły proste wzdłuż krawędzi do 110 pól). Szerokość 2–5 z szumu wartości (krok 7, rozciągnięty, by 2 i 5 też występowały), kwadrat bez narożników przy 4 i 5. 0–2 ślepe odnogi na tunel od 24 pól osi: 6–14 pól, jedno załamanie, szerokość 2–3, małe poszerzenie na końcu. Pomiar: 631 tuneli i 654 odnogi (m06), 611 i 609 (m19).
3. **Groty — PASS.** Co ok. 36 pól osi (co najmniej jedna od 16 pól) grota: elipsa o promieniach 4–12, środek przesunięty o pół promienia od komórki osi, brzeg z szumu (krok 3), 4 rundy automatu komórkowego (Floor przy ≥ 5 sąsiadach Floor z 8 albo pozostaje Floor przy ≥ 4), z wyniku tylko część połączona z komórką osi (3 × 3 wokół niej wymuszone) — więc każda grota należy do sieci. Środek trafia do `CaveGrid::rooms` (korytarze z drzwiami, dalekie wyjścia i ich testy działają jak dotąd). Udział komórek grot (Floor z ≥ 5 sąsiadami Floor z 8 i szerokością > 5: najkrótszy przebieg Floor przez komórkę wzdłuż obu osi i obu przekątnych, przekątna ×1,4): **mediana 0,779 w m06 (0,735–0,811) i 0,779 w m19 (0,517–0,822)**, kryterium ≥ 0,25; 1641 grot w m06, 1515 w m19. Test rdzenia: 0,798.
4. **Spójność i GUI — PASS.** Wszystkie wejścia w jednej składowej: `apart 0` we wszystkich 64 jaskiniach pomiaru; test rdzenia — wszystkie wejścia, wszystkie groty i cała podłoga w jednej składowej (krata i 24 seedy). Lądowiska (21 pól) pod wejściami bez zmian. Przypadek użytkownika przez prawdziwe okno (sonda `--surface`, m19, x 1000, y 1000, 512 × 512, seed 1729, 10 wejść w ścianie skały): `M19_CAVES_GUI_CASE … mapEntrances=20 withRamp=20 caveCells=13240 status=Reachable: 34897/34897 places on 3 floors. … Repairs: 0, … removed places: 0`; przypadek iteracji 44: `M19_ROCK_GUI_CASE_PASS … Reachable: 37313/37313 places on 5 floors … removed places: 0`; `D26_RAMP_ENTRANCE_PASS runs=40 pairedRamps=740 m19PairedRamps=336 farExitRamps=20`; `DUNGEON_GUI_PASS`.
   Bramka: `py -I docs/procedural/smoke/g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1` (ustawienia i warunki `--i45` plus kryteria kształtu) — **G5_GATE_PASS**, m06 i m19 **32/32** w każdym wariancie (lochy 0/1, ponawianie 0/8), krawędzie Z wszystkie chodliwe (m06 2773/2773 i 3777/3777, m19 2509/2509 i 3387/3387), 0 `Unrepaired`, udział usuniętych POI ≤ 0,0004, mediana wejść 18 (m06) i 17 (m19); i41: dalekie wyjście w każdej składowej (mediana 1,0), min. odległość 41 i 34, drzwi w 32/32 seedach, 0 błędnych; lochy: mediana udziału 0,164 i 0,160, mediana pomieszczeń 4, 0 kolizji z jaskinią. Pomiar w worktree, `local-data/i47-measurement/summary.json`.
5. **Bez regresji — PASS.** Determinizm: oba przebiegi każdego wariantu bajtowo identyczne (`identical: true`), testy rdzenia sprawdzają powtórzenie i niezależność od kolejności wejść; brak `std::random_device` (strumienie `StableRandom` z ziarnem i bezwzględnymi komórkami końców tunelu oraz szum `surfaceValueNoise`). `stop_token` sprawdzany przed każdym tunelem (test anulowania). **Odcisk Seed 1729 100 × 100 = 17159159863505677800 bez zmian** — to odcisk powierzchni (`test_procedural_surface`), jaskinia go nie zmienia. Kanoniczny `build_ninja.bat` w worktree, 18/18 testów rdzenia, obie sondy PASS, **SUITE_PASS** (`suite-commit-20261115-162102`, `suite-surface-20261115-162102`; 145 znanych ostrzeżeń SetFocus, zero innych wpisów stderr). Odciski jaskiń w sondzie (`caveFingerprint`) i liczby komórek jaskini zmieniły się zgodnie z założeniem.

**Obraz do oceny:** tryb renderowania `style_render` zapisuje teraz `<seed>-cave.png` (skała, podłoga, wejście); przykłady m19 512 × 512 z 16 wejściami: seedy 1729 i 1001 w katalogu roboczym wykonawcy (poza repozytorium).

**Nie zweryfikowane:** wygląd w GUI na mapie (ściany, bordery i lica „mountain” wokół nieregularnych grot oglądane tylko jako maska PNG), ruch w grze, wpływ na D23/D24 (lica północne i zachodnie przy bardziej poszarpanych ścianach), inne klienty niż 10.98, oryginalny RME. Udział grot (~0,78) jest znacznie powyżej progu 0,25 — jaskinia to sieć grot połączonych krótkimi tunelami; jeśli użytkownik woli więcej tuneli, parametry to odstęp grot (36) i ich promienie (4–12).
