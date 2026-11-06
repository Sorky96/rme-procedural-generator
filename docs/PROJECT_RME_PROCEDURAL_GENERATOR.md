# RME Procedural World Generator - Master Plan

> Dokument startowy projektu dla Codex / Claude Code. Stan: FAZA 0 w toku; G0 niezaliczone. Wersja planu: 1.1 (2026-10-26). Aktualne dowody: `docs/PROGRESS.md` oraz `docs/procedural/TEST_PLAN.md`.
>
> **Główny cel:** pełny, zintegrowany z Remere's Map Editor generator map Tibii, który potrafi analizować dostarczone mapy referencyjne i coraz wierniej odtwarzać **ich styl, logikę przestrzenną i grywalność**, bez kopiowania całych fragmentów 1:1.

## 0. Zasady dla agenta AI (PRZECZYTAJ NAJPIERW)

1. Pracuj w repozytorium użytkownika. **Najpierw wykonaj audyt aktualnego kodu i wersji**, potem przedstaw konkretne zmiany. Nie zakładaj, że nazwy API i katalogów podane w planie faktycznie istnieją.
2. Nie deklaruj funkcjonalności jako gotowej, dopóki nie jest zaimplementowana, przetestowana i zademonstrowana.
3. Rozwijaj funkcjonalności etapowo, w małych commitach. Każdy etap musi przechodzić testy regresyjne oraz mieć kryterium akceptacji.
4. Wykorzystuj istniejący kod RME: mapy, tiles, brushes, autoborder, undo/redo, OTBM, obsługę assets, Lua, dialogi. Unikaj równoległej implementacji tych samych mechanizmów.
5. Nie modyfikuj oryginalnych map treningowych. Operuj na kopiach i zapisuj backupy.
6. **Separacja:** analiza referencji -> model/gramatyka świata -> generator -> walidator -> konwersja na elementy RME -> zatwierdzanie na mapie.
7. Priorytety: poprawność danych > bezpieczeństwo mapy > grywalność > wiarygodność stylu > wydajność > liczba dodatków.
8. Projekt ma wspierać wiele wersji klienta/assetów, ale konkretną zgodność należy uzgadniać z danymi rzeczywiście dostępnymi w repozytorium.
9. Nie pobieraj, nie publikuj i nie dołączaj do repozytorium zastrzeżonych map ani assetów bez odpowiednich praw. Traktuj wejściowe mapy jako dane lokalne użytkownika. Nie używaj ich do trenowania zewnętrznych usług bez wyraźnej zgody.
10. Po każdej sesji uzupełnij `docs/PROGRESS.md`: wykonane, dowody testów, problemy, następne 3 zadania. Jeżeli testu nie uruchomiono, napisz to wprost.
11. Stosuj `AGENTS.md` i `C:\Users\Sorky\.codex\RTK.md`. Przed zmianami utwórz dedykowaną gałąź z `master`, jeżeli dostępne jest repozytorium Git. Brak metadanych zgłoś; nie inicjalizuj Git ani nie zmieniaj nazw katalogów w celu obejścia problemu. Redakcja tego planu nie uruchamia automatycznie implementacji ani jego przykładowego promptu z sekcji 11.
12. Narzędzia: CodeGraph najpierw do orientacji w kodzie, **tylko gdy istnieje `.codegraph/`**; bez indeksu nie uruchamiaj indeksowania samodzielnie. Context7 do aktualnej dokumentacji zależności, najlepiej właściwej wersji. `ast-grep` do zapytań strukturalnych (podgląd przed zmianą), `rg` do tekstu i konfiguracji. Polecenia shell uruchamiaj przez RTK, pozostałe CLI przez `rtk proxy`. Narzędzia i skille ProjectSorky dobieraj do rzeczywistego zakresu: kontrakty API/protokołu i OTClient HTML UI nie są częścią tego generatora RME.

## 1. Kontekst techniczny i granice systemu

Docelowy edytor: **Remere's Map Editor Redux**. Źródłem prawdy jest lokalny kod użytkownika i jego `AGENTS.md`; adres upstream jest jedynie odniesieniem:
- https://github.com/Open-Tibia-Tools/remeres-map-editor-redux
- Dokumentacja skryptów: `scripts/README.md` w wybranym commicie (zweryfikuj, nie zakładaj kompletności).

Lokalny `CMakeLists.txt` wymaga CMake 3.28+, C++23 i wxWidgets 3.3; `vcpkg.json` deklaruje m.in. Lua i Sol2 oraz baseline `1940ee77e81573713c0d364c42f5990172198be1`. Faktyczne wersje zainstalowanych bibliotek i toolchaina należy zapisać podczas G0; nie wnioskuj o wersji Lua wyłącznie z README. Kanoniczny build Windows x64: `rtk proxy cmd /c build_ninja.bat`, wynik `build-ninja/rme.exe`. Nie zmieniaj vendored FastNoiseLite ani wygenerowanego protobuf.

### 1.0 Wynik przeglądu przygotowawczego (2026-10-26)

Pierwszy przegląd dotyczył źródeł, nie działania programu. W jego trakcie brakowało `.git` i `.codegraph`; istniało `.gito`, którego nie zmieniano. Poniższa tabela zachowuje ustalenia tego przeglądu, a aktualny stan audytu znajduje się w `docs/PROGRESS.md`.

Po przygotowaniu środowiska przez użytkownika rozpoczęto FAZĘ 0: Git i CodeGraph działają, baza lokalna to `5a01a8fe1c34465559701d50681cc2210ef32c67`, gałąź `docs/procedural-phase0-audit`. Powstały `docs/procedural/ARCHITECTURE.md` i `docs/procedural/TEST_PLAN.md`; wyniki buildu i testów aktualizuje raport postępu. Nie oznacza to zaliczenia G0.

| Potwierdzony punkt | Lokalizacja i konsekwencja dla planu |
|---|---|
| Istnieją proceduralne API Lua i demonstrator | `source/lua/lua_api.cpp`: `registerNoise`, `registerAlgo`, `registerGeo`; `scripts/terrain_generator_demo.lua`. Najpierw oceń ponowne użycie; demo ma sztywne item ID i bezpośrednie zmiany tile, więc nie jest wzorcem bezpiecznej integracji. |
| Rysowanie zależy od GUI i tworzy akcje | `source/editor/operations/draw_operations.cpp`: przeciążenia `DrawOperations::draw` pobierają `g_gui.GetCurrentBrush()` i stosują akcje/batche. To nie jest gotowe API do izolowanego preview. |
| Historia może grupować sąsiednie akcje | `source/editor/action_queue.cpp`: `ActionQueue::addBatch`, `addAction`; `source/editor/action_queue.h`: fabryki akcji/batchy. Generator musi mieć sprawdzoną granicę transakcji i nie scalać się z ręcznym rysowaniem. |
| Istnieje transakcja skryptowa | `source/lua/lua_api_app.cpp`: `LuaTransaction`, `transaction`, eksport `app.transaction`. Zweryfikuj rzeczywisty rollback i śledzenie wszystkich mutacji; sama nazwa nie dowodzi atomowości dowolnego generatora. |
| Warianty brushy nie wynikają z seeda noise | `source/brushes/ground/ground_brush.cpp`: `GroundBrush::draw` wywołuje `random`; `source/util/common.cpp`: `getRandomGenerator` używa `random_device` i lokalnego dla wątku `mt19937`. Wymagany kontrolowany RNG również przy rozwiązywaniu brushy. |
| Identyfikacja mapy nie zastępuje kontroli zmian | `source/map/map.h`: `Map::getGeneration`; `source/map/map.cpp`: konstrukcja, `initializeEmpty`, `open`. Nie traktuj tej wartości jako licznika każdej edycji. |
| Zapis obejmuje pliki pomocnicze | `source/io/iomap_otbm.cpp`: `IOMapOTBM::saveMapToDisk` zapisuje również spawny, domy oraz zależnie od formatu strefy i waypointy. Testuj komplet danych i zgodność wybranego formatu. |

Context7: sprawdzono dokumentację wxWidgets 3.3: [wątki i komunikacja](https://docs.wxwidgets.org/3.3/overview_thread.html), [wxThread](https://docs.wxwidgets.org/3.3/classwx_thread.html). GUI obsługuj w głównym wątku, wyniki workera przekazuj zdarzeniami, np. `wxQueueEvent`; odbiorca musi pozostawać żywy. Dokumentacja biblioteki nie dowodzi bezpieczeństwa wątkowego klas RME.

### 1.1 Zakres końcowego rozwiązania

- Generowanie całych nowych obszarów, wypełnianie zaznaczeń, rozszerzanie istniejących map i łączenie obszaru ze starym terenem.
- Powierzchnia oraz poziomy Z: góry, wejścia/wyjścia, podziemia, jaskinie, wyspy, archipelagi.
- Biomy i przejścia: trawy, lasy, bagna, pustynie, śnieg/lód, skały, tereny nadmorskie, rzeki, jeziora; konfiguracja rozszerzalna.
- Struktury: ścieżki, drogi, przełęcze, mosty, wioski/ruiny/obozowiska, pomieszczenia i dungeony, miejsca zainteresowania (POI).
- Sensowne dekoracje, krawędzie, przejścia pięter, poprawne item ID/brush mapping i spawn placement tylko na życzenie.
- Możliwość ustawienia parametrów, seedów, masek, stref ochronnych i ograniczeń (np. zachowaj domy, UID/AID, spawny).
- UI w RME z podglądem, historią seedów, generacją wariantów, oceną podobieństwa do stylu, możliwością odrzucenia lub zatwierdzenia.
- Import referencyjnych map OTBM oraz w miarę możliwości innych formatów, jeśli istnieje bezpieczny importer.
- **Uczenie stylu z map użytkownika:** statystyki, wzorce, relacje przestrzenne, przejścia, budowle i wielopiętrowa topologia; opcjonalnie wytrenowane modele wspierające wybór/regenerację wariantów.
- Ewaluacja z mapami referencyjnymi, testowanie ścieżek i możliwość kontroli postępu jakości.

### 1.2 Niefunkcjonalne wymagania

- Deterministyczność względem konfiguracji + seeda + wersji algorytmu/profilu.
- Zero destrukcyjnych zmian przed wyraźnym zatwierdzeniem. Zatwierdzenie jako jedna logiczna transakcja Undo/Redo.
- Kontrola pamięci, anulowanie długich operacji, raport błędów, logi, brak freeze GUI.
- Poprawny round-trip: wygeneruj -> zapisz OTBM -> otwórz -> waliduj.
- Testy jednostkowe bez uruchomienia pełnego edytora tam, gdzie to możliwe.
- Brak zależności od serwisów zewnętrznych do samego generowania i uczenia na lokalnych mapach.

### 1.3 Pierwszy użyteczny zakres (MVP, G1)

MVP: jeden poziom Z, maska dokładnego zaznaczenia lub pusty prostokąt, grass/dirt/water, ręczny profil semantyczny dla jednego **zweryfikowanego** zestawu klient/assets, seed, preview, walidacja, jawny commit i jedno Undo/Redo. Góry wielopiętrowe, hydrologia zaawansowana, struktury, uczenie i ML należą do dalszych faz. Typy mogą uwzględniać Z od początku, ale G1 nie obiecuje generacji wielu pięter.

Minimalny dialog uruchamiający ten workflow powstaje w FAZIE 1; FAZA 7 rozbudowuje GUI. Nie twórz wszystkich modułów i pustych katalogów z wyprzedzeniem. Każdy powstaje wraz z pierwszą potrzebną funkcjonalnością i testami.

## 2. Architektura logiczna

```text
Mapy referencyjne (*.otbm + właściwe assets)
   | -> Importer/Validator -> Canonical Tile/World Representation
   | -> Feature Extractor -> Dataset + Anonymized Style Metrics
   | -> Style Profiles / Statistical Models / Optional ML Models
   |                                        |
   +----------------------------------------+ 
                                            v
GUI RME -> GenerationConfig -> World Planner -> Terrain Generator
                              |                |
                              |                -> Biomes / Hydrology / Elevation
                              -> POI/Routes -> Structures / Caves / Floors
                                            |
                                   Style-Aware Placement
                                            |
                                Semantic Safety Validator
                                            |
                               Brush/Item Resolver + Autoborder
                                            |
                          Concrete Diff + Connectivity/Safety Validator
                                            |
                         Preview/Diff -> Transaction/Undo -> RME Map
                                            |
                              OTBM Round-Trip + Quality Report
```

**Ważne:** abstrakcyjny model mapy nie może wymagać konkretnych sprite ID. To adapter określa, jak abstrakcyjna `water_edge`, `forest_floor`, `mountain_wall` itd. staje się brushami/elementami właściwej wersji Tibii.

Walidacja po resolverze jest obowiązkowa: rzeczywiste przedmioty, warianty doodadów, ich footprinty i bordery mogą zmienić przechodniość oraz zasięg zmian. Naprawa wymaga ponownego rozwiązania i walidacji; ustal limit prób, po którym kandydat jest odrzucany.

### 2.1 Moduły

| Moduł | Odpowiedzialność | Testy |
|---|---|---|
| `core` | typy, RNG, konfiguracja, siatki, praca na poziomach Z | determinism/property |
| `reference` | import, sanity-check, anonimizowane cechy map | fixture/golden |
| `learning` | statystyki, wzorce, profile biomów/stylów, opcjonalny ML | holdout/metrics |
| `planning` | plan dużych struktur świata, regiony, punkty POI, trasy | connectivity |
| `terrain` | heightmap, maski, biomy, hydrologia | distribution |
| `structures` | góry, jaskinie, miasta, ruiny, ścieżki, mosty | reachability |
| `decoration` | flora, fauna jako dekoracja, detale, item clusters | density/overlap |
| `validation` | grywalność, zgodność assetów, reguły mapy | property/integration |
| `rme_adapter` | map tiles, brushes, borders, undo, OTBM | integration |
| `ui` | dialog, podgląd, presety, status, dry-run, commit | manual/UI |
| `benchmark` | wydajność, jakość, porównanie seeda i stylu | benchmark/golden |

Przykładowy układ katalogów (dostosować po audycie):

```text
source/procedural/{core,reference,learning,planning,terrain,structures,decoration,validation,rme_adapter,ui}/
scripts/procedural/presets/
tests/procedural/{unit,integration,regression,fixtures}/
tools/reference_dataset/
docs/procedural/{ARCHITECTURE,DATASET,STYLE_MODEL,TEST_PLAN}.md
docs/PROGRESS.md
profiles/                  # tylko neutralne sample profile, bez cudzych map
local-data/references/     # gitignored
local-data/models/         # gitignored, o ile nie ma praw do publikacji
```

## 3. Model danych

Projektuj struktury, nie przywiązuj się do poniższych nazw w kodzie:

- `WorldRegion`: bounding box, poziomy Z, granice/otoczenie, maska obszaru, protected areas.
- `GenerationConfig`: region, seed, wersja algorytmu, `styleProfileId`, biomeWeights, water, roughness, roads, density, caveRate, targetFloor, mode, constraints.
- `WorldCell`: współrzędne (x,y,z), wysokość konceptualna, biome, ground semantic, water classification, features, occupancy, passability.
- `RegionGraph`: POI, wejścia do podziemi, przełęcze, drogi, połączenia i ich wymagania.
- `TileStyleFeatures`: histogramy ground/brush, adjacency, border sequences, obiekty/cluster, topologia, gęstości per biome, floor relationships.
- `StyleProfile`: wersja schematu, klient/assets, identyfikatory referencji, statystyki + rozkłady + tolerancje + model opcjonalny.
- `GenerationResult`: wygenerowane komórki, lista zmian dla RME, metryki jakości, warnings, seed, provenance.

**RNG:** dziel seed na stabilne strumienie dla regionów, dróg, rzek, biomów, dekoracji, jaskiń. Ten sam seed odtwarza identyczny rezultat w obrębie przypiętej wersji algorytmu.

Kontrakt powtarzalności obejmuje również snapshot wejściowej mapy i halo, maskę, chronione dane, ustawienia autoborder, manifest brushy/materials/assets, wersję resolvera oraz build/toolchain. Zapisuj ich identyfikatory lub hashe w provenance. Ustal stabilną kolejność iteracji, rozstrzygania remisów i wyprowadzania strumieni RNG; nie opieraj tego na `std::hash` ani kolejności kontenerów haszujących. Nie obiecuj identycznych wyników zmiennoprzecinkowych pomiędzy platformami bez osobnych testów.

Mierz dwa hashe: kanonicznego wyniku semantycznego i finalnego diffu RME (współrzędne, kolejność stosu itemów, atrybuty). Preview i Commit wykorzystują ten sam przygotowany wynik, bez ponownego losowania brushy. Redo odtwarza zapisane zmiany, nie uruchamia generatora ponownie. Rozwiązanie kontrolowanego RNG dla adaptera musi zostać wybrane i przetestowane w G0/G1 bez globalnego reseedowania RNG ręcznej edycji.

## 4. Uczenie na mapach referencyjnych - kluczowy wyróżnik

### 4.1 Co oznacza "uczenie"

Generator nie będzie magicznie uczony po jednokrotnym podaniu pliku. Wprowadź **reprodukowalny lokalny pipeline**:

1. Zaimportuj i sprawdź mapy OTBM oraz zgodność wersji itemów/assets.
2. Zamień tile/item ID na reprezentację semantyczną; zmapuj nierozpoznane elementy do `unknown` z raportem, **nie zgaduj**.
3. Wydziel regiony i biomy, poziomy Z, struktury i relacje pomiędzy nimi.
4. Wyciągnij mierzalne cechy, policz rozkłady i zachowaj przykładowe wzorce.
5. Zapisz **wersjonowany profil stylu** oraz dane diagnostyczne.
6. Generuj kandydatów według profilu i oceń, które najbardziej pasują.
7. Dopuszczaj korekty użytkownika (`good/bad`, wybór wariantu, maski, ręczne poprawki) jako sygnał do aktualizacji profilu; tylko po jego świadomym zatwierdzeniu.
8. Jeżeli uzyskasz wystarczająco duży i zróżnicowany zbiór: trenuj oddzielny model ML i porównaj go z baseline statystycznym na danych niewidzianych.

**Priorytet:** najpierw skuteczny model statystyczny / regułowy, później ML, a nie odwrotnie. Mała liczba dostarczonych map najpewniej nie wystarczy do wytrenowania wiarygodnego modelu generatywnego od zera.

### 4.2 Cechy do wydobywania

- Proporcje terenu i regionów w różnych skalach (16x16, 32x32, 64x64, większe).
- Matryce sąsiedztwa typów podłoża i borderów (N/E/S/W oraz diagonale).
- Długości, krętość, szerokość, rozwidlenia, ujścia rzek; wielkość jezior i wysp.
- Las: rozkład drzew, przerwy, klastry, gęstość w funkcji biomu i odległości od wody.
- Góry: powierzchnie, krawędzie, stosunek ścian do plateau, wysokość i liczba pięter, dostępność przełęczy.
- Rozmieszczenie skał, roślin, gruzu, detali, przedmiotów i zestawów brushy.
- Jaskinie: rozmiar komór, długość korytarzy, cykle, przewężenia, liczba wejść, relacje pięter.
- Sieć szlaków: odstępy, szerokości, częstości rozwidleń, mosty, łączenie regionów/POI.
- Rozmieszczenie budowli, osad, ruin, ważnych obiektów w relacji do otoczenia.
- Mikrostruktury: powtarzalne aranżacje małych fragmentów i reguły ich modyfikacji.
- Mapa przechodniości: składowe spójne, wąskie gardła, obszary niedostępne, możliwe odległości przejścia.

### 4.3 Jak naśladować styl bez kopiowania map

- Ucz **reguł, rozkładów, relacji i mikrostruktur**. Nie wklejaj bez zmian obszarów referencyjnych.
- Przy ponownym użyciu motywu transformuj semantykę i ponownie rozwiąż elementy kierunkowe. Nie obracaj mechanicznie item ID: ściany, drzwi i przejścia Z wymagają jawnych odpowiedników; brak mapowania oznacza odrzucenie transformacji. Sam obrót/odbicie nie zapewnia nowości ani praw do publikacji.
- Dodaj test podobieństwa lokalnych fragmentów, aby wykrywać niezamierzone kopie całych obszarów.
- Oddziel profile np. `classic-tibia-forest`, `classic-tibia-mountain`, `user-custom-v1` zamiast jednego rozkładu dla całego świata.
- Obsłuż **hierarchię skali**: układ świata -> region -> kompleks (np. las/jaskinia) -> segment -> tile/dekoracja.
- Styl zależy od **wersji Tibii i użytych assetów**; zapisuj tę informację w profilu.

### 4.4 Dane i podział na zbiory

- Referencje lokalne: `local-data/references/` z manifestem (nazwa, sha256, format, wersja klienta, licencja/pochodzenie, biomy, region, piętra, uwagi).
- Zbiory train/validation/test rozdzielaj **po niezależnych mapach lub dużych regionach z buforem przestrzennym**, nie przez losowanie sąsiednich kafelków. Inaczej wyniki będą sztucznie zawyżone.
- Unikaj dominowania dużej mapy nad małymi: ważenie per map/region/biome.
- Rozdziel osobno testy podobieństwa wizualnego od testów grywalności i technicznej poprawności.
- Nie zapisuj surowych map w repozytorium Git, jeśli nie istnieje zgoda/licencja.

### 4.5 Etapy zaawansowania modelu

- **L0 (baseline):** ręczne presety + noise + zasady generowania.
- **L1:** automatyczne statystyki referencji i dopasowywanie parametrów generatora.
- **L2:** reguły warunkowe, n-gramy sąsiedztwa / wzorce 2D, rozkłady skupień, modele grafów POI i przejść.
- **L3:** ranking wielu wygenerowanych kandydatów na podstawie similarity/quality score oraz feedback użytkownika.
- **L4 (opcjonalnie, jeśli dane uzasadniają):** trenowany lokalny model do rozkładania biomów, rozmieszczania dekoracji lub proponowania fragmentów, z porównaniem do L3 na holdout. Nie wymuszaj sieci neuronowej, jeśli nie daje poprawy.
- **L5:** iteracyjne uczenie po korektach użytkownika i wersjonowanie profili, z historią zmian i możliwością rollbacku.

### 4.6 Metryki jakości i ewaluacja

Mierz co najmniej:

1. `valid_item_ratio` oraz liczba nierozpoznanych ID.
2. `border_error_rate`, konflikty warstw i niepożądane puste tile.
3. `reachable_poi_ratio`, `reachable_walkable_ratio`, liczba odciętych regionów oraz błędnych połączeń Z.
4. Odchylenie rozkładów biomów/obiektów względem map referencyjnych.
5. Różnicę rozkładów sąsiedztw oraz wieloskalowych fragmentów, długości rzek, rozmiarów jezior, gęstości lasów.
6. Similarity score po konwersji do map semantycznych i, jeśli dostępne, rendered minimap.
7. Oceny człowieka: naturalność, podobieństwo stylistyczne, różnorodność, grywalność (ślepe A/B na mapach niewidzianych).
8. `novelty / memorization`: procent podejrzanie identycznych dużych fragmentów danych referencyjnych.
9. Czas, pamięć peak i rozmiar transakcji dla 100x100 / 500x500 / 1000x1000 na 1 oraz wielu poziomach.

**Nie optymalizuj wyłącznie jednego similarity score**, bo model zacznie imitować teksturę zamiast tworzyć sensowne przejścia. Brak "idealnego" obiektywnego wyniku podobieństwa.

Dla każdej metryki określ dziedzinę, mianownik, sposób agregacji, fixture, zestaw seedów i próg przed oceną holdout. Brak wymaganych POI oznacza `N/A`, nie 100% osiągalności. Wygenerowany diff musi mieć 100% poprawnych ID, zero naruszeń ochrony i zero zmian poza dozwoloną maską; `unknown` w referencji raportuj z pokryciem analizy. `border_error_rate` wymaga jawnych reguł/orakla, a ocena wizualna jest osobnym dowodem. Dla G4 zapisz porównanie z baseline na stałym zestawie seedów, rozrzut wyników i niepewność; progi jakości ustal przed testem, nie po obejrzeniu wyników.

## 5. Silnik proceduralny

### 5.1 Generacja hierarchiczna

1. **World Planner**: granice obszaru, sąsiedztwo istniejących terenów, docelowe regiony i główne POI.
2. **Macro Terrain**: wysokość, wilgotność, temperatura, kierunki odpływu, wybrzeża, woda.
3. **Biome Assignment**: biom na podstawie klimatu, wysokości, odległości od wody i stylu.
4. **Topology**: sieć dróg, głównych przejść, mostów, przełęczy, wejść do jaskiń i poziomów Z.
5. **Structures**: wioski, ruiny, jaskinie, dungeony, budowle w odpowiednich lokalizacjach.
6. **Micro Terrain**: patch'e ground, mniejsze skarpy, detale terenu.
7. **Decoration**: klastry roślin, skał, elementy charakterystyczne dla biomu.
8. **Resolver + Autoborder**: mapa semantyki na brushes/items i spójne brzegi.
9. **Validation + Repair**: sprawdzenie dostępności, zamkniętych przejść i konfliktów; naprawa lub odrzucenie seeda.
10. **Preview/Commit**: pokaż wynik/diff; zastosuj dopiero po zatwierdzeniu.

### 5.2 Algorytmy (dobierz po benchmarkach)

- Heightmap: layered Simplex/Perlin, domain warp, ridged noise, maski regionów.
- Rzeki: kierunkowy odpływ/flow accumulation albo prostsze constrained routing, obsługa jezior i ujść, kontrola szerokości.
- Biomy: reguły ekologiczne + profile z referencji, wygładzanie granic z zachowaniem organiczności.
- Drogi: A* / Dijkstra na grafie kosztów (nachylenie, woda, biome, teren chroniony).
- Lasy/zdobienia: grupowanie warunkowe + Poisson sampling tam, gdzie ma sens; style-dependent density.
- Jaskinie: cellular automata, room-and-corridor, graph repair, łączenie pięter.
- Osady: graf ulic, parcelacja i biblioteka reguł/prefabów; zabudowa ma respektować przejścia.
- Poprawa stylu: stochastic local search i reranking wariantów przez oceny jakości.

### 5.3 Reguły bezpieczeństwa

- Tryby: `new_area`, `replace_empty_only`, `replace_allowed_types`, `selection_only`, `blend_edges`.
- Domy, teleporty, spawny, UID/AID, strefy PZ, questowe przedmioty i elementy oznaczone jako chronione muszą być zachowane domyślnie.
- Konflikt -> ostrzeżenie + odrzucenie/dry-run, nigdy ciche kasowanie.
- Generowanie w workerze na kopii danych, stosowanie zmian do mapy wyłącznie zgodnie z zasadami wątkowości RME.
- Rozszerz generowany obszar o halo sąsiadów do obliczeń borderów; **nie modyfikuj** zewnętrznych tile bez jawnie określonej polityki.

### 5.4 Kontrakt Preview -> Commit i ograniczenia zasobów

1. Przypnij wynik do konkretnego edytora i tożsamości mapy. Na głównym wątku pobierz spójny snapshot maski, halo i chronionych danych oraz wersji assets. Worker otrzymuje wyłącznie własne dane; nie używa wskaźników do żywych `Tile`, GUI, Lua ani globalnych managerów bez potwierdzonego kontraktu wątkowego.
2. Przed Commit sprawdź, czy edytor istnieje, mapa i assets są te same oraz czy wejściowe dane maski/halo i istotne metadane nie zmieniły się od snapshotu. Sama aktywna karta, `hasChanged`, indeks Undo lub `getGeneration()` nie wystarczają do wykrywania edycji. Niezgodność unieważnia preview i wymaga regeneracji; nie przenoś wyniku automatycznie do innej mapy.
3. Polityka ochrony dotyczy całych tile i footprintów, zawartości zagnieżdżonych kontenerów oraz powiązanych danych domów, spawnów, stref, waypointów i pozycji specjalnych. Maska zaznaczenia nie jest tylko bounding boxem. Halo jest domyślnie tylko do odczytu; jeśli poprawnego bordera nie można uzyskać w masce, odrzuć wynik albo pokaż rozszerzenie zasięgu do jawnego zatwierdzenia.
4. Planowane tryby rozdziel na politykę zapisu (`empty_only` / `allowed_types`), maskę obszaru i politykę krawędzi (`blend_edges`). Zaznaczenie samo w sobie nie upoważnia do nadpisania istniejących danych. W MVP użyj `empty_only`; definicja pustego tile musi obejmować metadane, nie tylko brak ground.
5. Przygotuj cały konkretny diff i stan potrzebny do cofania przed zmianą mapy. Integrację rysowania prowadź przez `DrawOperations`, rozszerzając je w niezbędnym zakresie o jawny kontekst brush/RNG/batch, jeśli audyt to potwierdzi. Nie symuluj kliknięć ani nie przełączaj globalnego brusha GUI. `Action`/`Change`/`ActionQueue` pozostają jedyną historią zmian; nie twórz konkurencyjnego Undo.
6. Pojedynczy `BatchAction` oznacza grupowanie historii, a nie automatycznie atomowy commit i rollback po błędzie. Udowodnij testem: brak częściowego wyniku po błędzie, brak pustego wpisu historii, brak scalenia z poprzednią/następną edycją, zachowane metadane i indeksy. Jeśli istniejące operacje wymagają zmian, zaplanuj mały osobny PR integracyjny przed generatorem.
7. Anulowanie przygotowania zawsze pozostawia mapę i historię bez zmian. W MVP finalne zastosowanie przygotowanego batcha jest krótką sekcją bez anulowania i bez ponownego wejścia do obsługi edycji. Jeśli pomiary wykażą długi commit, wdrożenie większych obszarów wymaga bezpiecznego etapowania z rollbackiem i blokadą konfliktujących edycji, a nie częściowych commitów widocznych dla użytkownika.
8. Oszacuj szczyt pamięci przed alokacją: snapshot + halo + komórki + warianty + konkretne tile + undo/redo. Sprawdzaj przepełnienia rozmiarów i granice z `Position::isValid()` oraz aktualnych limitów mapy; oddziel wysokość koncepcyjną od Z (wyższe piętro Tibii ma mniejsze Z). Limit komórek, pamięci, kandydatów i prób naprawy musi prowadzić do czytelnego odrzucenia przed mutacją. Chunking nie usuwa kosztu całego Undo.
9. Zamknięcie okna/mapy, zmiana wersji klienta i nowa generacja anulują lub unieważniają zadanie. Odrzucaj opóźnione wyniki po identyfikatorze zadania; nie wysyłaj zdarzeń do zniszczonego odbiorcy. Minimalny preview może być semantyczny; rzeczywisty render i finalny diff pozostają rozróżnione w UI.
10. MVP działa offline. W sesji live/collaboration blokuj Commit z czytelnym komunikatem do czasu osobnego projektu kontroli konfliktów i testów integracyjnych. Nie zakładaj atomowości rozproszonej istniejących akcji.

### 5.5 Przechodniość i zgodność zapisu

Walidator używa konkretnych stosów itemów, flag klienta i jawnej polityki ruchu (kierunki, diagonale, narożniki). Przejścia Z są skierowanymi krawędziami o znanych wymaganiach; schody, drabiny, rope spots, teleporty, drzwi i akcje skryptowe mogą wymagać danych serwera. Nieznane zachowanie oznacz jako `unknown/conditional` i nie zaliczaj go automatycznie jako grywalne. Statyczny test edytora nie zastępuje testu na docelowym OTS.

Round-trip obejmuje `.otbm` i wszystkie używane pliki pomocnicze w osobnym katalogu testowym. Porównuj kanoniczną zawartość (w tym atrybuty i kolejność stosu), nie bajtowy hash pliku z kompresją. Wybierz wersję OTBM i odbiorcę testu; istniejący kod obsługuje m.in. OTBM 5, więc nie obiecuj zgodności każdego formatu z oryginalnym RME. Zachowaj zgodność już wspieranych formatów; nie dodawaj atrybutów ani nie zmieniaj serializacji w ramach generatora.

## 6. UI wewnątrz RME

### Panel "Generate Area"

- Tabs: `Area`, `Terrain`, `Biomes`, `Structures`, `Decoration`, `Reference Style`, `Advanced`, `Validation`.
- Rozmiar, pozycja X/Y/Z, wybór zaznaczenia, seed + randomize, biom główny i mieszane biomy.
- Suwaki: mountains, water, forests, paths, rough edges, vegetation, details, floor count, POI density.
- Panel stylu: wybór profilu wyuczonego z map, lista referencji, analiza i aktualizacja profilu.
- Podgląd: minimapa semantyczna oraz (później) rzeczywisty render z assetów; warianty A/B/C i ich score.
- `Preview`, `Regenerate`, `Compare`, `Validate`, `Place on map`, `Cancel`.
- Statystyki: udziały biomów/wody, przechodniość, nieznane item ID, ostrzeżenia, runtime/memory.
- Zapis/odczyt presetów do plików o wersjonowanym schemacie.

### UX wymagany

- Płynny podgląd z throttlingiem, aktualizacją w tle i możliwością anulowania.
- Rozdzielona operacja `Preview` i `Commit`.
- Jedno cofnięcie logicznie odwraca całą zatwierdzoną generację.
- Przy regenerowaniu czyść tylko preview, nie mapę użytkownika.
- Używaj `Theme` dla kolorów, fontów i odstępów oraz wspólnych `VirtualItemGrid` / `VirtualBrushGrid` dla katalogów itemów/brushy. Podgląd mapy integruj z właściwą warstwą overlay lub osobną powierzchnią; zasoby GL/NanoVG muszą mieć RAII, a pętle renderujące nie mogą alokować per tile/frame.

## 7. Harmonogram wdrożenia z bramkami akceptacji

Statusy do uzupełniania: `[ ] TODO`, `[~] IN PROGRESS`, `[x] VERIFIED`, `[!] BLOCKED`. Nie zaznaczaj punktu bez testu.

### FAZA 0 - Repozytorium i audyt

**Końcowa ocena 2026-11-02: VERIFIED.** Wszystkie sześć pozycji audytu i G0-01…09 oceniono w [raporcie końcowym](procedural/G0_FINAL_20261102.md), z konkretnymi dowodami oraz jawną różnicą semantyki aktualnego odbiorcy spawnów. Bramka build + punkty integracji + round-trip RME została spełniona; nie oznacza to pełnej zgodności świata OTS ani gwarancji przyszłego generatora. G1 nierozpoczęte. Datowane opisy 27–28.10 poniżej pozostają historią audytu.

- [x] Użyj istniejącego checkoutu, sprawdź Git i gałąź z `master`, zapisz SHA oraz lokalne zmiany. Potwierdź licencję, Windows x64, `build_ninja.bat`, wersje toolchaina i zależności. Dowody w audycie: lokalny snapshot `5a01a8f`, build zakończony z `--parallel 2`; pochodzenie upstream niepotwierdzone.
- [x] Zidentyfikuj API Map/Tile/Item/Brush/Autoborder/Undo i Lua; linkuj konkretne pliki i funkcje. Dowód: przegląd CodeGraph oraz tabela w `docs/procedural/ARCHITECTURE.md`; nie jest to walidacja runtime.
- [x] Sprawdź odczyt/zapis OTBM i kwestie multi-floor, house/spawn/UID/AID. Końcowe rzeczywiste RME round-trip i pełne niezależne porównania fixture OTBM 3, osobna strefa OTBM 5; aktualny odbiorca zbadany oddzielnie. Radius/direction legacy nie są zachowane przez jego konwerter, czego nie zaliczono jako pełnej zgodności spawnów. Dowody i granice: `docs/procedural/G0_FINAL_20261102.md`.
- [x] Zbuduj RME, otwórz testową mapę, zapisz kopię, uruchom minimalny skrypt modyfikujący 20x20 z Undo. 2026-10-26/27: 400 ground, Undo/Redo i odczyt zapisu PASS dla syntetycznego fixture; szczegóły i ograniczenia w `TEST_PLAN.md`.
- [x] Oceń istniejące `noise`/`algo`/`geo`, demonstrator terenu i transakcje Lua; wybierz granicę C++/Lua na podstawie testu, nie nazwy API. Decyzja 2026-11-01: rdzeń/snapshot/resolver/commit C++, Lua opcjonalne presety/konfiguracja/demo. Dowody: próbki noise/algo/geo, Draw/Lua/Draw, reprodukcje i regresje callbacków/setterów/metod, analiza RNG i snapshotu/halo w `ARCHITECTURE.md` oraz `TEST_PLAN.md`. Wybór nie oznacza gotowości adaptera ani pełnej atomowości; pozostałe testy G0 i projekt G1 nadal otwarte.
- [x] Utwórz `docs/procedural/ARCHITECTURE.md`, `docs/PROGRESS.md`, `docs/procedural/TEST_PLAN.md` i plan małych PR. Dokumenty utworzono; zawierają rzeczywiste symbole i jawne wyniki wykonanych kontroli. Nie nadpisano istniejącej dokumentacji ogólnej.

**Bramka G0:** reproducible build + opis rzeczywistych punktów integracji + round-trip testowej mapy.

Stan 2026-10-27: smoke pustej mapy przeszedł, lecz G0 pozostaje niezaliczone. Odtworzony błąd callbacka `app.transaction` (401 zamiast 400 pól) naprawiono w G0-B przez kontrolę wyniku chronionego wywołania Sol2. Regresja na mapie potwierdziła propagację błędów, rollback sześciu mutacji i brak wpisów historii po trzech nieudanych transakcjach; udane transakcje i Undo/Redo działają. Nie jest to dowód pełnej atomowości generatora: testy metadanych i rozdzielenia historii od ręcznej edycji nadal wymagane. Dowody w `TEST_PLAN.md`.

Późniejsza próba G0-06 przeszła dla `Draw` → Lua → `Draw`, także przy włączonym grupowaniu historii. Test zapisu/odczytu objął piętra 6/7, ground UID/AID, flagi i spawn z potworem. Odkryto dwa blokery: bezpośrednia zmiana `Item.actionId` w transakcji nie jest śledzona ani cofana, a „Save As” nowej mapy w tym samym katalogu zachowało stare nazwy plików pomocniczych i nadpisało XML spawnu używany przez źródłową mapę. Dane testowe były syntetyczne, przywrócone. Naprawa obu błędów i testy regresyjne są wymagane przed zakończeniem G0. Domy, strefy i zewnętrzny odbiorca pozostają niezweryfikowane.

Aktualizacja 2026-10-28: przygotowana poprawka Save As i backupu przeszła ograniczone testy kolizji oraz błędu dostępu; pełna atomowość i crash recovery nie są wykazane. Osobny fixture domu zachował w RME po ponownym odczycie nazwę, miasto, ID, czynsz i rozmiar. Wykryto również utratę niezapisanej zmiany czynszu przy zamknięciu bez ostrzeżenia. Próby noise oraz wybranych operacji algo/geo przeszły dla opisanych danych, nie dowodzą deterministyczności całego generatora. Na ich podstawie pozostaje robocza granica: rdzeń, snapshot, resolver i commit w C++; Lua jako opcjonalna konfiguracja i narzędzie demonstracyjne. Ostateczna decyzja oraz G0 nadal wymagają rozwiązania rollbacku itemów i pozostałych testów z `TEST_PLAN.md`.

### FAZA 1 - Rdzeń abstrakcyjny i podstawowa integracja

**IN PROGRESS, 2026-11-02:** rozpoczęto G1-A na gałęzi `feature/procedural-g1-core`. [Rdzeń i ograniczenia](procedural/G1_CORE.md); test C++ 100×100/500×500 PASS nie zastępuje bramki integracyjnej G1 poniżej.

- [x] `GenerationConfig`, stabilne strumienie RNG, siatka jednego piętra z jawnym Z, maski i chronione obszary. Rdzeń C++ i testy gotowe; ochrona i dokładna maska z rzeczywistego snapshotu i zaznaczenia: [snapshot](procedural/G1_SNAPSHOT.md), [maska](procedural/G1_MASK.md), a w pełnych round-tripach 100×100 i 500×500 pięć kafli fixture pozostało bez zmian.
- [x] Generowanie grass/dirt/water bez zależności od item ID, zgodnie z MVP z sekcji 1.3. Pierwszy semantyczny baseline v1, bez noise i oceny naturalności; rzeczywiste testy C++ 100×100/500×500 PASS.
- [x] Prosty resolver do właściwych brushy/items zgodnie z klientem. [Resolver](procedural/G1_RESOLVER.md); według raportu resolver odrzuca brak brusha i itemu, a brak klienta sprawdzono w natywnych odmowach ([odmowy](procedural/G1_REFUSALS.md)); testów resolvera nie uruchamiałem ponownie przy tej aktualizacji.
- [x] Dry-run (Preview i Validate), commit i Undo/Redo, logi. [Commit](procedural/G1_COMMIT.md), [round-trip](procedural/G1_ROUNDTRIP.md); logi dialogu mają dowód runtime dla Preview, Validate, Place, Cancel i odmów ([stan bramki](procedural/G1_GATE_STATUS.md)).
- [x] Test deterministyczności i błędnych parametrów. Rdzeń C++ PASS; fingerprinty podglądu, maski i commitu 100×100 i 500×500 są identyczne w kolejnych buildach ([regresja etapowa](procedural/G1_STAGED_CORE.md)), a trzy próby 500×500 zapisały komplet plików identyczny bajtowo ([powtarzalność](procedural/G1_PLACE_TIMING_REPEATABILITY.md)). Zakres: seed 1729 i dwa rozmiary.
- [x] Minimalny dialog: seed, obszar, Preview, Validate, Commit, Cancel; obsługa braku klienta/assets i nieaktualnego wyniku. [Dialog](procedural/G1_DIALOG.md), [macierz](procedural/G1_DIALOG_MATRIX.md), [odmowy](procedural/G1_REFUSALS.md), [unieważnienie](procedural/G1_INVALIDATION.md).
- [~] Test konkretnego diffu, anulowania, błędu zastosowania, zmiany mapy/assets po preview, przekroczenia budżetu oraz oddzielnego wpisu Undo. Commit offline. Wykonane: [commit](procedural/G1_COMMIT.md), [anulowanie](procedural/G1_CANCEL.md), [unieważnienie](procedural/G1_INVALIDATION.md), [budżet](procedural/G1_BUDGET.md), [zasoby](procedural/G1_ASSETS.md). Poza zakresem: rzeczywista sesja sieciowa live (tryb offline sprawdzono na nieuruchomionym hoście live). Otwarte: błędy alokacji sprawdzono w opisanych miejscach, nie we wszystkich.

**G1:** 100x100 i 500x500 na jednym piętrze w zmierzonym budżecie zasobów, preview zgodny z zastosowanym diffem, zapis i otwarcie kompletu plików, jedno Undo i Redo, zero zmian poza maską i w danych chronionych. Brak budżetu dla 500x500 jest ograniczeniem, nie zaliczeniem G1.

**Stan 2026-11-06:** wszystkie pozycje listy mają dowód poza jednym wyjątkiem opisanym przy ostatniej pozycji. Pełny round-trip obu rozmiarów powtórzono na bieżącym binarium, progi zasobów 100×100 zatwierdzono, a wzrost pamięci w 200 cyklach uznano za niebędący blokerem (decyzja użytkownika). Bramka pozostaje **IN PROGRESS**: rzeczywiste sesje sieciowe live uznano za poza zakresem G1 (decyzja użytkownika 2026-11-06). Nie rozstrzygnięto pozostałych miejsc awarii alokacji ani stosu `SetFocus` z aktywacji ramki przy otwartym generatorze (2026-11-04); `SetFocus` przy zamykaniu i w przebiegu odmów wyjaśniono. Część dowodów to pojedyncze serie. Szczegóły: [stan bramki](procedural/G1_GATE_STATUS.md).

### FAZA 2 - Sensowna powierzchnia i border

- [ ] Naturalne rzeki, jeziora, teren skalny i brzegi; strefy przejściowe biomów. Wielopiętrowe góry pozostają w FAZIE 5.
- [ ] Rozszerzenie mapowania semantycznego i testy istniejącego autoborder: wielokierunkowe krawędzie, zachowanie okolicy; bez drugiego silnika borderowania.
- [ ] Roślinność i detale warunkowe zależne od terenu.
- [ ] Testy rozkładów, brak nieobsługiwanych itemów, testy wizualne na kilku seedach.

**G2:** wygenerowany teren czytelny w RME, brak znanych krytycznych błędów borderów.

### FAZA 3 - Import i analiza map referencyjnych

- [ ] Lokalny import OTBM, wykrywanie zgodności item/asset.
- [ ] Normalizator map do reprezentacji semantycznej + `unknown` reports.
- [ ] Rozpoznawanie regionów, biomów, cech topologicznych, pięter.
- [ ] Feature extraction + raport CSV/JSON/HTML lub MD.
- [ ] Testy na minimum kilku własnych fixture i na referencjach użytkownika, bez publikacji referencji.

**G3:** dla każdej mapy referencyjnej generowany jest powtarzalny raport, statystyki i mapa semantyczna.

### FAZA 4 - Uczenie stylu L1/L2

- [ ] Profile stylu i manifest referencji z wersjonowanym schematem.
- [ ] Dopasowanie rozkładów ground, adjacency, clustering, biome transitions.
- [ ] Reguły zależne od kontekstu: las przy wodzie, kamienie przy górach, ścieżki, przejścia.
- [ ] Profile niezależne dla powierzchni / podziemi / wersji assetów.
- [ ] Holdout evaluation: porównanie z baseline na mapach niewidzianych.

**G4:** mierzalna i powtarzalna poprawa wybranych metryk stylu nad ręcznymi presetami, bez pogorszenia krytycznych metryk grywalności.

### FAZA 5 - Topologia i wielopiętrowe światy

- [ ] Połączenia świata: graph regionów, POI, trasy, mosty, przełęcze.
- [ ] Multi-level mountain logic i przejścia Z.
- [ ] Jaskinie, dungeony, korytarze, komory, wejścia, wyjścia.
- [ ] Struktury: osady/ruiny/obozowiska, placement z kontrolą kolizji.
- [ ] Walidator osiągalności i automatyczny repair / seed rejection.

**G5:** z ustalonego startu osiągalne są wymagane POI, prawidłowe wejścia i wyjścia Z oraz brak krytycznych odcięć.

### FAZA 6 - Zaawansowane uczenie i rankowanie L3/L4/L5

- [ ] Generowanie wielu kandydatów + quality scoring + wybór najlepszego.
- [ ] Feedback `approve/reject/adjust` i historia profili.
- [ ] Opcjonalny eksperyment ML dla wybranego podzadania tylko gdy dane są wystarczające i zgodne z prawami. Udokumentowane odstąpienie od ML nie blokuje wydania generatora statystycznego.
- [ ] Porównanie na holdout z L2/L3, analiza overfittingu oraz wykrywanie kopiowanych fragmentów.
- [ ] Mechanizm wersjonowania modeli i rollbacku.

**G6:** ranking i wersjonowany feedback ocenione na nieznanych mapach oraz w ślepym teście użytkownika; opcjonalny model ML zostaje wdrożony tylko jeśli przewyższa prostszy baseline według wcześniej ustalonych kryteriów.

### FAZA 7 - Pełne GUI i workflow

- [ ] Dialog `Generate Area` z opisanymi zakładkami i parametrami.
- [ ] Podgląd danych i render mapy, warianty, nawigacja, porównanie jakości.
- [ ] Import/analiza referencji i panel profili stylu.
- [ ] Generowanie w tle, anulowanie, progress, cache preview, commit/undo.
- [ ] Obsługa ochrony obszarów i ostrzeżeń.

**G7:** cały workflow możliwy z GUI, bez terminala i bez ręcznego uruchamiania skryptów.

### FAZA 8 - Optymalizacja i niezawodność

- [ ] Benchmarki 100x100, 500x500, 1000x1000 na 1 i wielu piętrach; zapisz wyniki i sprzęt.
- [ ] Chunking, caching, optymalizacja pamięci i inkrementalny preview.
- [ ] Testy crash recovery, anulowania, dużych map, dziwnych item ID i brakujących brushy.
- [ ] CI: build, unit, integration smoke, format/lint, release artifacts.
- [ ] Kompatybilność zapisu/odczytu ze wskazanym serwerem i klientem OTS, jeśli dostarczone.

**G8:** brak błędów krytycznych, benchmarki udokumentowane, regresje przechodzą automatycznie.

Testy regresyjne i podstawowy CI dodawaj wraz z funkcjonalnością od FAZY 1; FAZA 8 rozszerza pokrycie i pomiary. Nie odkładaj limitów pamięci, anulowania ani sprawdzania granic do optymalizacji końcowej.

### FAZA 9 - Beta, dokumentacja, wydanie v1.0

- [ ] Przykładowe profile, presety i demo bez rozpowszechniania map chronionych.
- [ ] Instrukcja instalacji, uruchomienia, importu referencji, uczenia profilu, generowania i eksportu.
- [ ] Dokumentacja formatów/profili, konfiguracji, troubleshooting, rozszerzania generatora.
- [ ] Test końcowy: generuj dużą mapę powierzchni i podziemi, oceń, zatwierdź, zapisz, ponownie otwórz, przejdź ścieżki.
- [ ] Release notes, tag `v1.0.0`, przykładowe nagranie/wizualizacje, lista znanych ograniczeń.

**G9 / Definition of Done:** użytkownik może w GUI RME dostarczyć mapy, wytrenować/dopasować profil stylu, wygenerować kompletny spójny wielopoziomowy obszar, podejrzeć wynik, zatwierdzić go bez niszczenia istniejącej mapy, zapisać i otworzyć OTBM. Rezultat przechodzi testy kompatybilności, topologii i jakości na mapach niewidzianych.

## 8. Minimalne scenariusze testów end-to-end

1. **Determinism:** ten sam seed/config/style/model version -> ten sam hash semantycznego wyniku na tym samym buildzie.
2. **Undo:** wygenerowanie 500x500, Commit, Undo -> oryginalny snapshot mapy; Redo -> identyczny wynik.
3. **Protected objects:** obszar zawiera dom/spawn/UID/AID/PZ -> generator nie zmienia chronionych danych.
4. **Neighbor blending:** generacja w środku starej mapy -> poprawne brzegi, brak zmian poza dopuszczonym zasięgiem.
5. **OTBM:** round-trip bez uszkodzeń i nieoczekiwanej utraty atrybutów.
6. **Reachability:** POI naziemne i podziemne są połączone zgodnie z konfiguracją.
7. **Reference learning:** trening na A/B, test na C -> raport metryk porównany z baseline.
8. **Novelty:** wykrywanie dużych identycznych fragmentów również po obrotach/odbiciach; wyklucz trywialne jednolite obszary według jawnej reguły. Raportuj podejrzane kopie i stosuj ustalony próg odrzucenia. Zgoda na użycie danych nie zastępuje wymagania generowania nowych obszarów.
9. **Cancel:** anulowanie podglądu lub operacji -> brak zmian mapy i wycieków.
10. **Scale:** kilka pięter 1000x1000 -> stabilne zasoby, czytelny progres, brak niekontrolowanego OOM.
11. **Stale preview:** zmień tile/halo, wykonaj Undo/Redo, zamknij mapę, przełącz kartę albo assets po Preview -> brak zastosowania wyniku do niewłaściwego stanu/mapy.
12. **Transaction failure:** kontrolowany błąd przygotowania lub zastosowania -> brak częściowej mutacji i uszkodzenia indeksów/historii; następna ręczna edycja ma osobny Undo.
13. **Footprint/mask:** doodad przecina nieregularne zaznaczenie lub chroniony tile, border wymaga wyjścia poza maskę -> odrzucenie albo jawnie zatwierdzony szerszy diff.
14. **Resource limits:** skrajne wymiary/Z, przepełnienie rozmiaru, brak budżetu na Undo i wiele wariantów -> odrzucenie przed zmianą mapy; opóźniony wynik anulowanego zadania nie zastępuje nowego preview.

## 9. Ryzyka i decyzje projektowe

| Ryzyko | Reakcja |
|---|---|
| Mało map treningowych | reguły/statystyki + ostrożna walidacja, bez obietnic zaawansowanego ML |
| Różne item IDs | adapter wersji + mapowanie semantyczne + raport unknown |
| Mapy nie są jednorodne | segmentacja biomów i profile per region |
| Cechy nierozpoznawalne automatycznie | ręczne etykiety / narzędzia adnotacji / heurystyki z confidence |
| Niepoprawne autobordery | integracja brush + testy sąsiadów i golden fixtures |
| Nieprzechodnie mapy | graf topologii + walidacja + repair |
| Zależność od rozwijającego się forka | pin SHA, branch projektu, testy CI i adapter zamiast masowych zmian core |
| Przeuczenie / kopiowanie referencji | spatial holdout, dedup, novelty tests, ograniczenie patch reuse |
| Koszt obliczeniowy | profiling, chunking i generowanie wieloetapowe |
| Prawa do map/assetów | prywatny dataset i jawne prawa do użycia |

## 10. Strategia pracy Codex/Claude

Każdą fazę rozbij na PR o pojedynczej odpowiedzialności. Dla każdego PR agent ma przedstawić:

```text
Cel:
Zweryfikowane pliki/API:
Zakres zmian:
Ryzyka i zabezpieczenia:
Testy dodane:
Testy uruchomione + wynik:
Demo/screenshot/log (jeśli dotyczy):
Co pozostało:
```

- Agent nie może „zaliczyć” testów opisem ani pozornym mockiem, gdy istnieje wymóg integracji z prawdziwym RME.
- W każdej fazie uaktualnij `docs/PROGRESS.md` z dokładnym stanem i blokadami.
- Nie wprowadzaj modelu ML, zanim baseline statystyczny i ewaluacja nie działają.
- Ostatnim krokiem każdej fazy jest krótki manualny test w RME i zrzut/raport do weryfikacji człowieka.

## 11. Pierwszy prompt dla agenta (skopiuj dosłownie)

> Jesteś głównym inżynierem projektu proceduralnego generatora map Tibii wewnątrz Remere's Map Editor. Przeczytaj `docs/PROJECT_RME_PROCEDURAL_GENERATOR.md` w całości i traktuj go jako plan projektu oraz kontrakt kryteriów akceptacji, z zachowaniem `AGENTS.md` i zakresu aktualnego polecenia użytkownika.
>
> Zacznij **WYŁĄCZNIE od FAZY 0**. Zidentyfikuj repozytorium, SHA, istniejące API Lua/C++ i build. Korzystaj z CodeGraph, jeśli istnieje indeks, Context7 dla dokumentacji zależności oraz RTK/ast-grep/rg zgodnie z zasadami projektu. Nie zakładaj nazw funkcji bez przejrzenia kodu. Zbuduj przez `rtk proxy cmd /c build_ninja.bat` i uruchom minimalny bezpieczny smoke test importu, edycji 20x20, Undo/Redo i zapisu/otwarcia OTBM wraz z plikami pomocniczymi, jeśli środowisko i dane na to pozwalają. Utwórz `docs/procedural/ARCHITECTURE.md`, `docs/procedural/TEST_PLAN.md` i uzupełnij `docs/PROGRESS.md`. Wskazuj rzeczywiste symbole i lokalizacje kodu. Sprawdź ryzyka z sekcji 5.4; nie wdrażaj jeszcze finalnego generatora ani ML. Testów niewykonanych nie zaliczaj. Na końcu podaj raport zgodny z sekcją 10, wykaz blokerów i maksymalnie trzy kolejne zadania.

## 12. Lista informacji/danych do dostarczenia przez użytkownika

Do G0 potrzebne są przynajmniej jeden zgodny zestaw assets i bezpieczna mapa testowa (może być syntetyczna). Nie pobieraj ich automatycznie. Pozostałe referencje i oceny można dostarczać etapowo przed G3/G4:

- Wersja klienta Tibii i odpowiadające jej `items.otb`, `items.xml`, `dat/spr` lub assets obsługiwane przez konkretny build.
- Kilka map referencyjnych OTBM; najlepiej obejmujących powierzchnię, góry, jaskinie, miasta, różne style regionów i poziomy Z.
- Informacja, które mapy to pożądany styl i które elementy są szczególnie wartościowe.
- Przykładowa mapa, do której generator powinien dopasowywać nowy obszar.
- Zestaw 5-10 ręcznych przykładów "dobry wynik / zły wynik", gdy pierwsze wersje będą dostępne.
- Docelowy OTS i wymagana zgodność OTBM; ograniczenia dotyczące spawnów, stref PZ, domów itd.

## 13. Śledzenie projektu

W `docs/PROGRESS.md` trzymaj aktualny stan. Poniżej szablon startowy, nie raport wykonania:

| Phase | Status | Commit | Dowody/testy | Blokery | Następne zadanie |
|---|---|---|---|---|---|
| 0 | TODO | - | - | - | Audyt repozytorium |
| 1 | TODO | - | - | - | Po G0 |
| 2 | TODO | - | - | - | Po G1 |
| 3 | TODO | - | - | - | Po G2 |
| 4 | TODO | - | - | - | Po G3 |
| 5 | TODO | - | - | - | Po G4 |
| 6 | TODO | - | - | - | Po G5 |
| 7 | TODO | - | - | - | Po G6 |
| 8 | TODO | - | - | - | Po G7 |
| 9 | TODO | - | - | - | Po G8 |

---

## 14. Zmiany redakcyjne w wersji 1.1

- Dopasowano plan do lokalnego CMake/build i udokumentowano brak Git/CodeGraph bez udawania wykonania G0.
- Wskazano istniejące API proceduralne i ryzyka demonstratora; ograniczono MVP oraz przesunięto minimalne GUI do G1.
- Dodano konkretną walidację po resolverze, kontrolowany RNG brushy i identyczność Preview/Commit/Redo.
- Doprecyzowano snapshot, konflikt ze zmianami mapy, ochronę halo/footprintów, atomowość i pamięć Undo; live poza MVP.
- Rozdzielono walidację statyczną od testu OTS, dodano pliki pomocnicze do round-trip i mierzalne kryteria ewaluacji.
- Dokumentacja szczegółowa trafia do `docs/procedural/`; ML pozostaje opcjonalne. Implementacja generatora nie została rozpoczęta.

---

**Cel końcowy nie jest „losową ładną mapą”.** Jest nim lokalny, powtarzalny i bezpieczny generator map o spójnym stylu zmapowanym z referencji, który umożliwia świadome projektowanie świata i weryfikację grywalności, a jego jakość można mierzyć i poprawiać wraz z kolejnymi mapami i ocenami użytkownika.
