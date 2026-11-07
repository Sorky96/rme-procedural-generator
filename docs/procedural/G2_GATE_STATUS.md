# G2 — stan bramki „Sensowna powierzchnia i border”, 2026-11-07

**PARTIAL.** Kryteria rdzenia, adaptera i borderów mają dowody z testów i próbników na rzeczywistym kliencie 1098-BlackTek. Kryterium „teren czytelny w RME” ma dotąd ocenę użytkownika z dwóch pojedynczych widoków (250×250 i 500×500, seed domyślny), a nie przegląd kilku seedów. Dług D5 (bordery poza obszarem) rozstrzygnięty dla Surface 2026-11-07 (sekcja niżej). Bramka nie jest zamknięta: brakuje oceny wizualnej kilku seedów.

## Kryteria z planu G2 i dowody

| Kryterium | Dowód | Status |
| --- | --- | --- |
| Naturalne rzeki | `test_procedural_surface`: każda rzeka kończy się w jeziorze albo na krawędzi obszaru, ma minimum długości, a każdy jej kafel leży w oknie 2×2 złożonym z rzeki lub jeziora (test dodany 2026-11-07, patrz niżej). Ocena wyglądu: użytkownik, po poszerzeniu rzek („Wygląda lepiej”). | PASS rdzeń; wygląd ocena jednego widoku |
| Jeziora, skały | Te same testy: minimum rozmiaru jeziora i płatu skały, górne limity udziału, 24 seedy 100×100, 6 seedów 250×250, 3 seedy 500×500, każda cecha pojawia się w każdym zbiorze seedów. | PASS rdzeń |
| Brzegi | Brzeg ma co najwyżej `shoreWidth` kroków od jeziora (test dodany), ląd styka się z jeziorem tylko przez brzeg. Domyślnie 2 kafle po teście GUI. | PASS rdzeń; wygląd: ocena użytkownika, 500×500 |
| Przejścia biomów | Gładkie przejście trawa/ziemia ze skalą szumu 10 i usuwaniem płatów < 24 komórek (test `transitionBand`: 88 krawędzi twardych vs 246 miękkich). Udział ziemi w otwartym lądzie: 0,24–0,39 (100×100), 0,28–0,32 (250×250), 0,29–0,31 (500×500). | PASS rdzeń; wygląd: ocena użytkownika |
| Mapowanie semantyczne tylko istniejącym autoborderem | Resolver `--surface`: `pairs_without_border=0` we wszystkich 6 przebiegach (seedy 1–4 przy 100×100, 1729 przy 250×250 i 500×500). Nie dodano własnej logiki borderów. | PASS (klient 1098) |
| Roślinność i detale zależne od terenu | [G2_VEGETATION.md](G2_VEGETATION.md); test rdzenia: brak dekoracji na wodzie, brzegu i skale, brak drzew przy wodzie; E2E z Undo/Redo. Ocena użytkownika w GUI: „Drzewa wyglądają dobrze”. | PASS testy; wygląd ocena |
| Testy rozkładów | Powyżej plus dekoracje: udział drzew w lądzie ≈ 3,1% (24 seedy 100×100) i 3,2% (6 seedów 250×250). | PASS |
| Brak nieobsługiwanych itemów | Resolver i podgląd sprawdzają każdy item (definicja, clientId, nie meta; dekoracje także nie grunt ani border); E2E: dla każdej komórki położone itemy zgodne z planem. | PASS (klient 1098) |
| Testy wizualne na kilku seedach | **Nie wykonano.** Są tylko pojedyncze widoki użytkownika; brak zrzutów z kilku seedów w repozytorium. | OTWARTE |
| Brak znanych krytycznych błędów borderów | `pairs_without_border=0`; D5 (bordery poza obszarem przy cudzym sąsiedzie) rozwiązany dla Surface przez pozostawienie pustych komórek przy istniejącym gruncie, test `SURFACE_NEXT_TO_TERRAIN_PASS`. | PASS testy (klient 1098); wygląd szwu nieoceniony |

## Co zmieniono w tym kroku

- Nowe kontrole w teście rdzenia: szerokość rzeki i górna granica szerokości brzegu, oraz przegląd 500×500 (3 seedy).
- Kontrola szerokości rzeki znalazła jednokaflowy ogonek na źródle rzeki (seed 1, 100×100, komórka 51,50): rozszerzenie dodawało tylko sąsiada wschodniego i południowego, więc ostatnia komórka trasy nie miała pełnego bloku 2×2. Teraz każda komórka trasy dodaje też południowo-wschodnią (lakes bez zmian). Skutek uboczny: rzeki są lekko grubsze na przekątnych. Przypięty odcisk Seed 1729 100×100 zmieniono z 4385766052247269449 na 13556304753518616142. Wersja algorytmu pozostaje 2 (niewydana).

## Zmiany po teście GUI na kilku seedach (2026-11-07)

Użytkownik: wygląd ok; rzeki powinny się pogrubiać po zlaniu i zwężać ku źródłu, nie wypływać „z pustych miejsc”, a brushe mają być wybierane z list, nie wpisywane.

- **Źródła rzek:** tylko komórki skały. Trasa startuje od pierwszej komórki nieskalnej w dół od skały, więc rzeka wychodzi z krawędzi skały; bez skały nie ma rzek. Test: każda rzeka styka się ze skałą.
- **Szerokość rzeki (druga wersja, po zrzucie użytkownika: „rzeki nadal nie poprawione”):** pierwsza wersja pogrubiała rzekę tylko po zlewie dopływów, więc pojedyncza rzeka miała równe 2 kafle. Teraz rozmiar kafla to liczba komórek przebytych od najdalszego źródła plus 15 za każdą wpadającą trasę: poniżej 20 → 2 kafle, poniżej 45 → 3, od 45 → 4. Źródło przy skale jest najcieńsze, rzeka rośnie stopniowo ku ujściu, a zlew dodaje skokowo. Dopływy tylko wpadają, rzeka nie rozwidla się w dół. Kafle rzeki w pełnym bloku 3×3: 1232 z 4971 (24 seedy 100×100), 453 z 1869 (6 seedów 250×250), 183 z 735 (3 seedy 500×500). Skała i jeziora nie są nadpisywane, więc tuż przy skale blok może być przycięty (test to dopuszcza). Szacunek pamięci roboczej (40 B/komórkę) nie był przeliczany po dodaniu dwóch wektorów uint16 na komórkę.
- **Odcisk** Seed 1729 100×100: 1951698793224813922 (poprzednio 13556304753518616142). Rzeki są teraz w 24 z 24 seedów 100×100 (poprzednio 23).
- **Dialog:** pola brushy to listy rozwijane tylko do wyboru (brushe gruntu dla trawy/ziemi/wody/skały/brzegu, doodady dla drzew/krzaków/kamieni), z domyślnym wyborem `grass`, `dirt`, `clear water`, `rocky ground`, `sand`, `trees`, `bushes`, `small rocks`, jeśli istnieją w załadowanym kliencie. Lista nie sprawdza, czy brush jest sensowny dla roli (np. woda jako trawa); to nadal waliduje resolver.
- Próbniki po zmianie: `g2d3-surface-160136`, `g2d3-commit-160136` (`--surface` zapisuje `passed: false` tylko przez D1; kod wyjścia 0, stderr pusty), wiersze commitu G1 bez zmian, `pairs_without_border=0`, `halo_tiles_modified` 101 (100×100 seed 1) i 55 (250×250), pozostałe 0 (D5).
- **Wolne otwieranie dialogu (potwierdzone przez użytkownika jako dialog):** dane zawierają 15741 brushy doodad i 4756 gruntu (liczone z XML w `brushes/` i `data/`), a osiem list było wypełnianych przy każdym otwarciu, łącznie ok. 70 tysięcy pozycji. Listy to teraz `wxComboBox` tylko do wyboru: do rozwinięcia zawierają jedną pozycję (domyślną albo pierwszą), pełna lista ładuje się przy pierwszym rozwinięciu. Lista doodadów jest ograniczona do brushy z pojedynczymi itemami wariantu 0, bo tylko takie resolver akceptuje. Czasu otwarcia nie mierzono przed ani po zmianie; zmiana jest oparta na liczbie pozycji, nie na pomiarze, a lazy ładowanie w zdarzeniu rozwinięcia nie było sprawdzone w natywnym RME (możliwe migotanie lub opóźnienie przy pierwszym rozwinięciu listy 15 tysięcy pozycji).
- **Test GUI użytkownika po lazy ładowaniu:** „Jest ok” (dialog otwiera się bez problemu, listy działają); bez pomiaru czasu i bez osobnej oceny rzek na seedach 1, 7, 4242.
- **Niezweryfikowane:** wygląd nowych rzek i działanie list w natywnym RME; dialog z klientem, który nie ma domyślnych nazw (pierwsza pozycja listy); wydajność dialogu przy dużej liczbie brushy.

## D5: generowanie obok istniejącego terenu (2026-11-07)

- **Przyczyna:** po położeniu gruntu `GroundBorderCalculator` liczy bordery także dla kafli sąsiednich (halo, a przy dziurach w selekcji i chronionych kaflach także w obszarze). Gdy brzeg obszaru ma brush o wyższym priorytecie niż istniejący sąsiad, border ląduje na kaflu sąsiada, a potok G1 odrzuca każdą zmianę poza zapisywalnym obszarem (celowo, bez przycinania).
- **Odtworzenie (przed poprawką):** nowy test `nextToTerrain` w `--surface` (obszar 100×100 seed 1 otoczony trawą) zakończył się odmową przygotowania, a po pierwszej połowie poprawki odmową `InvalidPlan` (preview wymaga ścisłej równości „item istnieje ⇔ komórka zapisywalna”).
- **Rozwiązanie (tylko Surface):** `surfaceKeepClear(snapshot)` wskazuje komórki zapisywalne, które mają w 8-sąsiedztwie komórkę niezapisywalną (poza obszarem, poza maską lub chronioną) z gruntem. Te komórki zostają puste (`Preserve` w generatorze i resolverze, bez itemu), więc kafle z borderami są ≥2 kroki od cudzego terenu. Preview dopuszcza pustą zapisywalną komórkę tylko wtedy, gdy ta sama funkcja ją wskazuje (ścisła równość zachowana). Classic i test G1 `OutOfMask` bez zmian. Przy pustej mapie i pustym otoczeniu wynik jest identyczny jak wcześniej (próbniki, odciski).
- **Wynik testu** (`d5d-surface-162711`): `SURFACE_NEXT_TO_TERRAIN_PASS` dla 100×100 seed 1 (szew 396 komórek, 9604 położone), 100×100 seed 1 z istniejącym kaflem w środku (404 / 9595) i 250×250 seed 1729 (996 / 61504): przygotowanie i walidacja przechodzą, komórki obok istniejącego gruntu są puste, wszystkie pozostałe położone, istniejący teren bez zmian po commicie, Undo i Redo. Regresje: `--preview`, `--commit` PASS (`d5e-*-162844`), wiersze commitu G1 bez zmian, test rdzenia PASS.
- **Koszt:** 1-kaflowy pusty szew przy istniejącym terenie (trzeba go wypełnić ręcznie), brak płynnego przejścia na stronie sąsiada. Szacunek pamięci przygotowania Surface wzrósł o 2 bajty na komórkę plus narzut (`bytes` w `SURFACE_E2E_PASS` 100×100: 13385154 → 13405218).
- **Test GUI użytkownika (2026-11-07):** generowanie obok istniejącego terenu, „Wygląda dobrze” (ocena wizualna; Undo/Redo na GUI nie raportowane osobno).
- **Niezweryfikowane:** połączenie z roślinnością obok terenu (nie testowane); inne typy sąsiadów niż trawa (ring w teście to `grass`); kafle z samymi itemami bez gruntu (nie liczą się jako sąsiad); klienty inne niż 1098-BlackTek.

## Nieregularny brzeg (2026-11-07, uwaga zewnętrznego recenzenta)

Uwaga: koncepcja dobra, piasek nieregularny, miejscami trawa powinna się wcinać; wygląda „jak losowe”. Interpretacja: pas piasku wokół jeziora miał stałą szerokość (obrys jeziora), a naturalna plaża powinna się zwężać i poszerzać.

- **Zmiana:** `shoreWidth` domyślnie 3 (poprzednio 2). Pierwszy pierścień przy jeziorze zawsze jest piaskiem; każdy dalszy pierścień powstaje tylko tam, gdzie gładki szum (siatka co 6 komórek, osobny strumień `shore_s2`) przekracza próg rosnący z pierścieniem (35%, 65%, 95% zakresu). Efekt: plaża szeroka w jednych miejscach, a gdzie indziej trawa wcina się do pierwszego pierścienia.
- **Rozkład** (liczba kafli piasku 1/2/3 kroki od wody): 4253/3290/1273 (24 seedy 100×100), 6126/4763/1863 (6 seedów 250×250), 12984/10185/4085 (3 seedy 500×500). Test wymaga, żeby pierścienie 2 i 3 istniały i były rzadsze niż poprzednie.
- Odcisk Seed 1729 100×100: 7666204984290374682. Test rdzenia, `--surface` (z `SURFACE_NEXT_TO_TERRAIN_PASS`) i `--commit` PASS (`g2f-surface-171132`, `g2f-commit-171132`), `pairs_without_border=0`, wiersze commitu G1 bez zmian.
- **Niezweryfikowane:** wygląd w RME i czy to odpowiada intencji uwagi („kępy trawy w piasku” mogły oznaczać też izolowane kępy trawy wewnątrz piasku, czego ta zmiana nie robi: trawa wcina się tylko od zewnątrz); wpływ na roślinność (drzewa trzymają 2 kroki od brzegu, więc teren z szerszym piaskiem ma mniej drzew przy wodzie).

## Wyniki prób (profil `g0-reference-profile-20261030`, prywatne przebiegi, ukryte okno)

| Próbnik | Przebieg | Wynik |
| --- | --- | --- |
| rdzeń | `test_procedural_surface` | PASS; 151 punktów alokacji z wstrzykniętą awarią (wszystkie zgłoszone), 7 dla dekoracji |
| `--surface` | `g2d-surface-153056` | wszystkie znaczniki PASS (`SURFACE_RESOLVER_SIZE_PASS`, `SURFACE_E2E_PASS`, `SURFACE_VEGETATION_E2E_PASS`, `SURFACE_RESOLVER_PASS`); runner zapisał `passed: false` z powodu stderr (D1), kod wyjścia 0 |
| `--preview` | `g2d-preview-153056` | `passed: true` |
| `--commit` | `g2d-commit-153056` | `passed: true`; wiersze `TERRAIN_COMMIT_SIZE_PASS` 100×100 (8881480 / 3965894 / 80195 / 6608263949644166501) i 500×500 (218763816 / 98785798 / 1987447 / 173161415496114986) identyczne jak w [G1_COMMIT.md](G1_COMMIT.md) |

Statystyki borderów: `halo_tiles_modified` = 95 (100×100, seed 1) i 50 (250×250, seed 1729), pozostałe 0.

## Czego nie zweryfikowano

- Przeglądu wizualnego kilku seedów w RME (krok dla użytkownika poniżej).
- Dialogu i wyglądu po ostatniej zmianie rzek (rzeki jeszcze nieoglądane po poszerzeniu o komórkę południowo-wschodnią).
- Klientów innych niż 1098-BlackTek.
- D5, wpływu drzew i kamieni na przechodniość, pomiarów wydajności poza jednorazowymi liczbami z testu rdzenia (`generation_ms`, to nie jest porównanie przed/po).
- Dokładnej dolnej granicy szerokości brzegu (test sprawdza tylko górną; brzeg może być węższy tam, gdzie lądem jest skała lub rzeka).

## Test GUI do wykonania (kilka seedów)

Dla seedów 1, 7 i 4242, rozmiar 250×250, „Natural surface”, roślinność włączona: zrób podgląd i wygeneruj, a potem oceń rzeki (czy nigdzie nie ma pojedynczego kafla), brzegi jezior, przejścia trawa/ziemia, skały i bordery. Zrzut każdego seeda do `docs/procedural/screenshots/`, po czym ten dokument można zaktualizować do PASS dla kryterium wizualnego.
