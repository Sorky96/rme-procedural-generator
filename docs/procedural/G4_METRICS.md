# G4-F — metryki i kryteria ustalone PRZED oceną na zbiorze testowym, 2026-11-08

**Status: zapisane, jeszcze nie uruchomione na zbiorze testowym.** Do chwili zacommitowania tego pliku nie policzono i nie oglądano żadnej wartości na mapach testowych (rodzina testowa każdego foldu). Wartości treningowe i walidacyjne z G4-C/D były oglądane. **Zastrzeżenie o przecieku:** rodzina testowa foldu k jest rodziną walidacyjną foldu k−1, więc widziałem dystanse strojeń z foldu k−1 do celu rodziny testowej foldu k (G4_FIT.md, G4_COVERAGE.md, G4_DECOR.md). Strojenie foldu k, które będzie oceniane, nie było mierzone na tym celu, ale cel testowy nie jest w pełni „niewidziany”; to osłabia niezależność testu i wchodzi do zastrzeżeń wyniku. Zmiana którejkolwiek reguły poniżej po uruchomieniu na teście wymaga osobnego wpisu i oznaczenia wyniku jako „po zmianie kryterium”.

## Porównanie
- **Baseline (ręczne presety):** domyślne `SurfaceTuning` i domyślne `DecorTuning`.
- **Kandydat:** `SurfaceTuning` i `DecorTuning` z `fold-N.profile` (uczone na trenowaniu, krok wybrany po walidacji).
- Folds z `reference_split` (parametry domyślne), klient 10.98, mapowanie `accepted-10.98-v2`, tylko powierzchnia (z ≤ 7). Fold jest **oceniany**, jeśli ma cel testowy (co najmniej jedna mapa testowa z ≥ 1000 kafli naturalnych) i cel treningowy. Pozostałe są raportowane jako N/A, nie jako poprawa ani pogorszenie.
- Generowanie: bok 256, seedy **1001–1032** (32 seedy; rozłączne z seedami dopasowania 1–6), bez maski, wegetacja włączona, ten sam seed dla baseline i kandydata (para).

## Metryki stylu (miara: mniejsza = lepiej)
1. **S1 udziały:** L1 (ppm) między udziałami 5 rodzajów naturalnych wyniku (suma po seedach) a udziałami celu testowego.
2. **S2 sąsiedztwa:** L1 (ppm) macierzy sąsiedztwa 5×5 wyniku względem celu testowego.
3. **S3 wegetacja:** `decorDistance` (średnia różnic 24 stawek w ‰ po kontekstach, w których cel testowy ma ≥ 200 kafli); fold bez porównywalnego kontekstu = N/A dla S3.
Cel testowy liczy się jak cele treningowe: `buildTarget` / `buildDecorTarget` (rodziny po równo, mapy w rodzinie po równo, minimum 1000 kafli naturalnych).

## Metryka krytyczna grywalności
4. **P1 spójność chodzenia:** udział największej 4-spójnej składowej kafli chodzących (Grass, Dirt, Shore) wśród wszystkich kafli chodzących wyniku, osobno dla każdego seeda (w ppm). Brak kafli chodzących = N/A dla tego seeda.
Pozostałe warunki grywalności z planu (100 % poprawnych ID, zero zmian poza maską, border_error_rate) nie zależą od wartości strojenia (profil zmienia tylko liczby w zakresie walidowanym przez generator, a kontrola ścieżki zatwierdzania nie jest zmieniana), więc **nie są mierzone przez ten ewaluator**; to jest założenie do potwierdzenia testem pełnego potoku w GUI (krok G4-GUI).

## Agregacja i niepewność
- Dla każdego oceniane­go foldu i metryki: różnica parami (kandydat − baseline) dla każdego z 32 seedów; raportowane: mediana, 10. i 90. percentyl różnicy, liczba seedów, w których kandydat jest lepszy.
- Fold jest „lepszy” w metryce, jeśli mediana różnicy < 0 **i** kandydat jest lepszy w ≥ 24 z 32 seedów (75 %). Fold jest „gorszy”, jeśli mediana > 0 i kandydat jest gorszy w ≥ 24 z 32. Inaczej „bez rozstrzygnięcia”.
- Porównania między foldami: liczba foldów lepszych / bez rozstrzygnięcia / gorszych oraz rozstęp median. Brak jednej liczby zbiorczej; foldy mają różną liczbę map (1–2), więc nie uśredniam ich w jedną wartość.

## Kryterium G4 (część stylu i grywalności)
G4 jest **spełniony w mierzonym zakresie**, jeśli jednocześnie:
- dla S1 i S2: lepszych jest co najmniej 2/3 ocenianych foldów i żaden fold nie jest „gorszy” w obu naraz;
- S3: lepszych jest więcej foldów niż gorszych (spośród tych z porównywalnym kontekstem);
- P1: w każdym ocenianym foldzie mediana P1 kandydata ≥ 95 % mediany P1 baseline.
W przeciwnym razie wynik to **nie spełniony** (z wyliczeniem, które warunki zawiodły), a nie „częściowo spełniony”. Ograniczenia danych (jedna-dwie mapy na fold, 3 mapy z wegetacją) wpisuję jako zastrzeżenie do wyniku niezależnie od jego kierunku.

## Czego ta ewaluacja nie obejmuje
Podobieństwa wizualnego, naturalności rzek i gór, klastrowania na skalach 16/32/64, ocen człowieka, podziemi, klienta BlackTek, reguł kontekstowych poza wegetacją, oraz testu memoryzacji wyniku generatora względem referencji.

## Iteracja 2 (zapisana 2026-11-08 po obejrzeniu wyniku iteracji 1; przed jej uruchomieniem)

**Co się zmienia: model, nie metryki ani kryterium bramki.** Metryki S1, S2, S3, P1, seedy 1001–1032, agregacja, reguła „lepszy/gorszy”, kryterium G4 i zestaw foldów są dokładnie te z iteracji 1. Zmienia się dopasowanie `SurfaceTuning`:
- **Ograniczenie spójności lądu w dopasowaniu:** kandydat (wartość jednego parametru w przeszukiwaniu) jest odrzucany, jeśli mediana spójności lądu P1 na seedach dopasowania (1–6, bok 160) jest niższa niż **97 %** mediany P1 ręcznych (domyślnych) parametrów na tych samych seedach. Margines 97 % (zamiast 95 % z kryterium) wybrany z góry, żeby zostawić zapas na zmianę boku i seedów. Odrzucony kandydat nie uczestniczy w wyborze; start (ręczne parametry) zawsze spełnia ograniczenie.
- Cel treningowy, wybór kroku po walidacji i dopasowanie wegetacji bez zmian.

**Zastrzeżenia, które ta iteracja dziedziczy i pogłębia:**
- Iteracja 2 została zaprojektowana po zobaczeniu wyników iteracji 1 na tych samych foldach testowych. Foldy testowe są więc używane po raz drugi i wynik nie jest niezależnym testem w sensie G4; to ocena po adaptacji modelu do znanych słabości. Wynik zostanie oznaczony „po zmianie modelu” i nie zastępuje iteracji 1, tylko leży obok niej.
- Seedy testowe są te same, mapy testowe te same; różnica dotyczy wyłącznie strojenia.
- Jeśli kandydat nie przejdzie ograniczenia w żadnym kroku, profil foldu jest identyczny z ręcznymi parametrami (wynik „bez rozstrzygnięcia”), co jest dozwolonym i uczciwym rezultatem.

## Iteracja 3 — profile per styl (zapisana 2026-11-08, przed jej uruchomieniem)

**Co się zmienia:** zamiast jednego profilu uczonego na zlepku rodzin, jeden profil na styl. **Dane nie pozwalają na ocenę na mapach niewidzianych poza jednym przypadkiem** (patrz niżej), więc ta iteracja ma trzy części o różnej mocy dowodowej i nie zastępuje kryterium G4 z planu.

**Definicje.**
- *Styl* = rodzina z podziału G4-A (deklarowana ∪ po treści), mająca co najmniej jedną mapę stylu z celem (≥ 1000 kafli naturalnych na z ≤ 7; `buildTarget`). Stylów z celem jest 8 (m01, m05, m06, winter, caves, m15, m19, swamp); m17 nie ma celu.
- *Profil stylu* = `SurfaceTuning` dopasowany do celu stylu z ograniczeniem spójności lądu 97 % (jak w iteracji 2; train = walidacja = styl, wynik ostatniego kroku) oraz `DecorTuning` z dopasowania zamkniętego na kontekstach stylu (konteksty poniżej 200 kafli zostają wbudowane).
- Dopasowanie: seedy 1–6, bok 160. Ocena: seedy 1001–1032, bok 256, pary seedów jak wcześniej.

**Część A — odwzorowanie własnego stylu (in-sample).** Dla każdego stylu: profil stylu kontra baseline (ręczne parametry) w metrykach S1, S2, S3 i P1 względem celu **tego samego stylu**. Reguła „lepszy/gorszy” i definicje S1–S3, P1 jak w iteracji 1. *To jest ocena na danych, z których profil powstał.*

**Część B — mapa wyłączona z uczenia w obrębie stylu (leave-one-map-out).** Dla stylów z co najmniej dwiema mapami z celem: profil dopasowany na pozostałych mapach stylu, ocena na wyłączonej mapie, względem baseline. Z obecnych danych kwalifikuje się tylko **swamp (M12, M13): dwa przypadki**. Wynik raportowany osobno, bez progu liczbowego (n = 2 nie wystarcza do wniosku o uogólnieniu).

**Część C — rozróżnialność stylów.** Macierz: dla każdej pary (profil stylu i, cel stylu j) mediana po 32 seedach z S1 + S2. Styl j jest *rozpoznany*, jeśli jego własny profil ma najmniejszą medianę wśród wszystkich 8 profili. Raportowany też baseline jako dodatkowy wiersz (nie jest kandydatem do rozpoznania).

**Kryterium iteracji 3 (wszystkie jednocześnie):**
- A: profil jest lepszy od baseline zarówno w S1, jak i w S2 w co najmniej 2/3 stylów, i żaden styl nie jest gorszy w obu naraz;
- C: rozpoznanych co najmniej 2/3 stylów;
- P1: w każdym profilu stylu (A i B) mediana P1 ≥ 95 % mediany baseline;
- B: bez progu; jeśli którykolwiek z dwóch przypadków jest „gorszy” w S1 i S2 naraz, zapisuję to jako zawód.

**Interpretacja z góry:** spełnienie A, C i P1 oznacza wyłącznie, że profile per styl odwzorowują style, z których powstały, bez utraty spójności lądu. **Nie** dowodzi poprawy na mapach niewidzianych i **nie** spełnia bramki G4 z planu; ta pozostaje oceniana przez część B, która z n = 2 niczego nie rozstrzyga. Wynik jest oznaczany „po zmianie modelu”, foldy testowe z iteracji 1–2 pozostają użyte.

## Iteracja 4 — kształt rzek (D9 część wodna, D6), zapisana 2026-11-08 przed implementacją i przed jakimkolwiek pomiarem rzek

**Zakres.** D9 tylko dla ciał wodnych: rzeki (długość, szerokość, krętość, odcinki proste) i rozmiary jezior. Nadal poza zakresem (dług D9 zostaje): rozwidlenia, jaskinie (komory, korytarze), góry (ściany do plateau), szlaki, osady, mikrostruktury, wąskie gardła, piętra. D6: rzeki generatora prowadzone po osiach.

**Definicje (stałe, z góry).**
- Woda = rodzaj Water (jezioro, rzeka, morze; bez Shore) na z ≤ 7. W referencjach przez zaakceptowane mapowanie, w generatorze Lake i River.
- Składowa wody = 4-spójna składowa, per piętro. *Szkielet* = ścienianie Zhang-Suen (standardowe, dwa podprzebiegi, sąsiedztwo 8) składowej.
- *Składowa rzeczna*: pole ≥ 40 kafli, długość szkieletu L ≥ 20 kafli, średnia szerokość pole / L ≤ 7. Inne składowe: jeziora (pole < 20 000) i morze (pole ≥ 20 000; pomijane).
- Cechy składowej rzecznej: krętość T = 1000 · L / max(bok pudełka) w ‰; udział odcinków prostych S = ‰ kafli szkieletu leżących w ciągu ≥ 12 kolejnych kafli w jednym wierszu lub kolumnie; szerokość W = 1000 · pole / L (tysięczne kafla).
- Wartość referencyjna cechy = mediana po wszystkich składowych rzecznych map (zbiorczo). Wartość wygenerowana dla seeda = mediana po składowych rzecznych tego seeda; seed bez składowej rzecznej jest raportowany jako „bez rzeki” i nie wchodzi do porównań.
- Mapa referencyjna *kwalifikuje się*, jeśli ma ≥ 5 składowych rzecznych. Jeśli nie ma co najmniej dwóch takich map, ocena D6 jest raportowana jako niemożliwa, nie jako zaliczona.

**Zmiana modelu.** Nowy parametr `SurfaceTuning::meander` (0..8 kafli, domyślnie 0 = dotychczasowy wynik bez zmian bajt w bajt): poprzeczne przesunięcie linii środkowej trasy rzeki o gładki szum wzdłuż trasy, zerowe przy końcach i w pobliżu ujść dopływów. Dopasowanie `meander` z siatki {0,1,2,3,4,5,6,8}: minimalizuje |T_gen − T_ref| + |S_gen − S_ref| na seedach 1–6, bok 160.

**Ocena (dwa foldy krzyżowe: dopasowanie na jednej kwalifikującej się mapie, ocena na drugiej).** Seedy 1001–1032, bok 256, pary baseline (meander 0) / kandydat; różnica odległości |gen − ref| dla T oraz dla S; reguła „lepszy/gorszy” jak w iteracji 1 (mediana różnicy i ≥ 75 % seedów).

**Kryterium D6 (wszystkie jednocześnie, w obu foldach):**
1. kandydat lepszy od baseline w T i w S;
2. spójność chodzenia P1 mediana kandydata ≥ 95 % mediany baseline;
3. niezmiennik generatora: każda komórka rzeki jest 4-połączona przez komórki rzeki z jeziorem lub krawędzią obszaru, w 100 % seedów (rzeka nie zostaje wyspą).
Brak spełnienia któregokolwiek = D6 niezamknięty, z wyliczeniem. Ocena w obiektywnych cechach nie zastępuje oceny wizualnej (ślepe A/B jest osobnym krokiem).

**Zastrzeżenia z góry:** dowód z co najwyżej kilku referencji (prawdopodobnie M06 i M19), pomiar kształtu to szkielet kratowy (przybliżenie), próg „prosty odcinek ≥ 12” i definicja składowej rzecznej wybrane przed pomiarem, ale bez wiedzy o ich czułości.

## Iteracja 5 — kanały wodne wraz z rzekami połączonymi z morzem (zapisana 2026-11-08 przed implementacją i przed pomiarem)

**Powód.** Metryka iteracji 4 pomijała składowe wody ≥ 20 000 kafli (morze), więc duże rzeki połączone z morzem nie weszły do referencji ([G4_COMPARE.md](G4_COMPARE.md)). Iteracja 5 mierzy *kanały*: wąskie części wody niezależnie od tego, czy łączą się z morzem. Najpierw pomiar bazowy referencji i generatora (5a), potem ewentualna zmiana generatora (5b) oceniana tymi samymi kryteriami.

**Definicje (stałe, z góry).**
- Woda = rodzaj Water (jezioro, rzeka, morze; bez Shore) na z ≤ 7, per piętro. Przy referencjach przez zaakceptowane mapowanie v3, w generatorze Lake i River.
- *Woda otwarta* = otwarcie morfologiczne maski wody kwadratem 13×13 (erozja, potem dylatacja; kafle poza obszarem liczą się jako brak wody). *Kanał* = woda minus woda otwarta (wąskie części, szerokość ≤ 12). Kwadrat sprzyja osiom; to przybliżenie.
- *Składowa kanału*: 4-spójna składowa kanału; zaliczana, jeśli pole ≥ 40 kafli, a długość szkieletu (Zhang-Suen, jak w iteracji 4) L ≥ 20.
- Cechy (na składowych zaliczonych, zbiorczo na mapę lub seed): **L** = suma długości szkieletów na 65 536 kafli obszaru (kafle obecne w referencji; komórki obszaru w generatorze); **W** = 1000 · pole / L (tysięczne kafla); **B** = liczba rozgałęzień szkieletu (kafel szkieletu z ≥ 3 sąsiadami szkieletu w sąsiedztwie 8) na 100 kafli szkieletu; **C** = udział składowych kanału, które stykają się z wodą otwartą lub krawędzią obszaru, w ‰.
- Mapa referencyjna *kwalifikuje się*, jeśli ma ≥ 5 zaliczonych składowych. Bez co najmniej dwóch takich map ocena jest niemożliwa, nie zaliczona.

**Pomiar bazowy (5a).** Referencje: wartości L, W, B, C każdej kwalifikującej się mapy (bez uśredniania poza zakresem). Generator: profile m06 i m19, seedy 1001–1032, bok 256, mediana po seedach (seed bez składowej kanału: „bez kanału”, osobno policzony).

**Kryterium (dla 5a i 5b, jednakowe).** Dla profilu stylu X z referencją M_X (m06→M06, m19→M19; to jest dopasowanie in-sample, nie test na mapie niewidzianej): cecha jest *w paśmie*, jeśli mediana generatora leży w [0,67·ref; 1,5·ref]. Profil „zaliczony”, jeśli ≥ 3 z 4 cech jest w paśmie i seedów z kanałem jest ≥ 75 %. Wynik 5a to opis stanu, nie bramka. Zmiana generatora 5b jest zaliczona, jeśli oba profile są zaliczone oraz spełnione pozostają: P1 (spójność chodzenia) mediana ≥ 95 % bazowej i niezmiennik „każda komórka rzeki dochodzi do jeziora lub krawędzi” w 100 % seedów (jeśli 5b zmienia ten niezmiennik, nowy zapisuję przed zmianą).

**Zastrzeżenia z góry.** Dwie referencje z rzekami (M06, M19), próg 13×13 i progi pasma wybrane bez znajomości wartości, kanał to przybliżenie (też małe jeziora ≤ 12 szerokości i zatoki wejdą do pomiaru), brak oceny wizualnej. Jeśli 5a pokaże, że definicja kanału nie oddziela rzek od zatok na wycinku, który oglądałem, zgłaszam to i zmieniam definicję **przed** 5b z zapisem w tym pliku.

## Iteracja 6 — struktury w dużej skali (zapisana 2026-11-08 przed implementacją i przed pomiarem)

**Powód.** Porównanie wizualne ([G4_COMPARE.md](G4_COMPARE.md)): generator daje jednolitą mozaikę w jednej skali, referencje mają duże masy lądu, morza i skały. Obecna miara G4 (udziały, sąsiedztwo o odległości 1) tego nie opisuje.

**Definicje (stałe, z góry).**
- Rodzaje naturalne: Grass, Dirt, Rock, Shore, Water (w generatorze Lake i River razem jako Water). Komórki innego rodzaju (Other, Unmapped, NoGround, brak kafla, Preserve) nie biorą udziału w żadnej parze.
- *Zgodność na odległości d*: ‰ par komórek (x, y) i (x+d, y) oraz (x, y) i (x, y+d), obu naturalnych, o tym samym rodzaju, d ∈ {4, 16, 64}; liczona na piętrach 0..7 zbiorczo dla referencji i na jednym obszarze dla seeda generatora.
- *Zgodność przypadkowa* A0 = Σ_k s_k² ‰, gdzie s_k to udział rodzaju k wśród komórek naturalnych. *Nadmiar* E_d = zgodność(d) − A0, w ‰ (różnica zgodności ponad losowy układ o tych samych udziałach), więc miara nie zależy od samych udziałów gruntu.
- Referencja = wartość E_d mapy; generator = mediana E_d po seedach.

**Pomiar bazowy (6a).** Profile m06 i m19, seedy 1001–1032, bok 256, profil v7; referencje M06 i M19 (m06→M06, m19→M19, in-sample jak w iteracji 5).

**Kryterium (dla 6a i 6b).** Cecha E_16 i E_64 *w paśmie*, jeśli mediana generatora leży w [0,67·ref; 1,5·ref]. Profil „zaliczony”, jeśli oba E_16 i E_64 są w paśmie. E_4 raportuję bez oceny (kontrola, że drobna skala się nie psuje: mediana E_4 generatora w paśmie jak wyżej to warunek dodatkowy dla 6b).

**Zmiana modelu (6b, tylko jeśli 6a pokaże lukę).** Dopasowanie `SurfaceTuning::featureSize` z poszerzonej siatki {24, 32, 48, 64, 96, 128, 192, 256} na seedach 1–6, minimalizując |E_16 − ref| + |E_64 − ref| (reszta strojenia bez zmian). Ocena na seedach 1001–1032. Jeśli sama zmiana wielkości cechy nie wystarczy, kolejne kroki (np. dodatkowa skala szumu) zapisuję tutaj osobno przed ich wykonaniem. Warunki dodatkowe 6b: P1 (spójność chodzenia) mediana ≥ 95 % bazowej, niezmiennik rzek w 100 % seedów.

**Zastrzeżenia z góry.** Dwie referencje, dopasowanie in-sample, brak oceny wizualnej (zgodność przy lagu nie rozróżnia „jeden duży ląd” od „wiele dużych płatów”), referencje to duże mapy z elementami ludzkimi (poza naturalnymi komórkami), a okno generatora (256) ucina lag 64 przy brzegu. Jeśli 6a pokaże, że wartości referencji zależą silnie od wybranego fragmentu mapy, zgłaszam to i zmieniam definicję przed 6b.

## Iteracja 7 — plamy ziemi i szerokość plaży (zapisana 2026-11-08 przed implementacją i przed pomiarem)

**Powód.** Oględziny użytkownika w GUI (profile v8, obszar 500×500): ziemia w trawie jest zazębioną „kropkowaną” plamą zamiast zwartych plam; plaże są cienkie (w referencji M19 szerokie). Użytkownik zlecił poprawę obu punktów.

**Definicje (stałe, z góry).** Rodzaje jak w iteracji 6 (komórki naturalne: Grass, Dirt, Rock, Shore, Water; reszta pomijana; Lake i River jako Water).
- **D — gęstość granicy ziemi**: 1000 · (liczba par 4-sąsiednich komórek Dirt–Grass) / (liczba komórek Dirt), zbiorczo. Duże D = poszarpana, zazębiona ziemia.
- **F — zwartość ziemi**: ‰ komórek Dirt leżących w 4-spójnych składowych Dirt o polu ≥ 100.
- **P — grubość plaży**: 1000 · (liczba komórek Shore) / (liczba komórek Shore mających 4-sąsiada Water). 1000 = plaża o grubości jednej komórki.
- Cecha N/A, jeśli mianownik jest mały: Dirt < 1000 komórek (D, F), Shore < 1000 komórek (P).
- Referencja = wartość zbiorcza mapy (piętra 0..7, komórki obecne); generator = mediana po seedach.

**Pomiar bazowy (7a).** Profile m06 i m19 w wersji v8 (featureSize 192), seedy 1001–1032, **bok 512** (bliżej użycia w GUI i zgodnie z obserwacją z iteracji 6, że bok 256 ucina skalę); referencje M06 i M19 (m06→M06, m19→M19; in-sample).

**Kryterium (dla 7a i 7b).** Cecha D, F, P (jeśli nie N/A) *w paśmie*, gdy mediana generatora leży w [0,67·ref; 1,5·ref]. Profil „zaliczony”, jeśli wszystkie nie-N/A cechy są w paśmie. Warunki dodatkowe dla 7b: (1) udział każdego z 5 rodzajów w medianie generatora różni się od bazowego (7a) najwyżej o 5 punktów procentowych; (2) niezmiennik rzek w 100 % seedów; (3) P1 (spójność chodzenia) mediana ≥ 95 % bazowej, jeśli narzędzie pomiaru P1 jest dostępne dla boku 512, a jeśli nie, zgłaszam to jako niesprawdzone.

**Zmiana modelu (7b, tylko jeśli 7a pokaże lukę).** Dopasowanie `transitionWidth` z siatki {0, 500, 1000, 2000, 4000, 8000, 16000} oraz `shoreWidth` z siatki {0..8} (podniesienie górnego limitu walidacji z 4 do 8, zewnętrzne pierścienie jak dotąd losowane szumem), osobno dla każdego profilu, na seedach 1–4 bok 512, minimalizując sumę względnych błędów |gen − ref| / ref po nie-N/A cechach. Ocena na seedach 1001–1032. Jeśli samo strojenie nie wystarczy, kolejną zmianę (np. szum wygładzający brzeg ziemi) zapisuję tutaj osobno przed wykonaniem.

**Zastrzeżenia z góry.** Dwie referencje, in-sample; D i F to miary kształtu, nie ocena wizualna; zawężenie przejścia trawa/ziemia zmienia sąsiedztwo rodzajów dopasowane wcześniej (warunek 1 to tylko udziały, nie sąsiedztwo); „plaża” to rodzaj Shore z mapowania klucz→rodzaj (piasek), w referencjach także piasek poza wodą wchodzi do Shore (np. pustynie), więc P mierzy tylko komórki Shore przy wodzie względem wszystkich.

**Zmiana definicji P (zapisana 2026-11-08 po pomiarze referencji 7a, przed pomiarem generatora i przed jakąkolwiek zmianą modelu).** Pomiar referencji pokazał, że P z pierwotnej definicji jest bezużyteczny: wartości 33 795 (M06) i 22 366 (M19), bo większość komórek Shore w referencjach to piasek daleko od wody (pustynie, wyspy piasku), a nie plaża. Zastępuję: **P′ = 1000 · (komórki Shore osiągalne z komórek Shore sąsiadujących z Water przez 4-sąsiednie komórki Shore w nie więcej niż 8 krokach, wliczając komórki początkowe) / (komórki Shore mające 4-sąsiada Water)**. P′ = 1000 oznacza plażę o grubości jednej komórki. Reszta kryterium bez zmian (pasmo [0,67·ref; 1,5·ref], N/A przy < 1000 komórek osiągalnych). Wartości referencji P′ zobaczę dopiero po tym zapisie; wcześniejsze P zostaje w opisie jako pomiar nieprzydatny, nie jako zaliczone.

## Iteracja 7 — wyniki 7a i zapis zmiany modelu 7b (2026-11-08, przed jej wykonaniem)

**7a (wartości zmierzone przed zmianą generatora, seedy 1001–1032, bok 512, profile v8).** Referencje (M06 / M19): D 421 / 451, F 749 / 625, P′ 5 638 / 5 656. Generator m06: D 112, F 975, P′ 1 987; m19: D 203, F 951, P′ 1 979. W paśmie: m06 1 z 3 (F), m19 0 z 3.

**Obserwacja sprzeczna z wcześniejszą diagnozą.** Zgodnie z liczbami granica ziemi w generatorze jest *gładsza* (D niższe), a jej zwartość *większa* (F wyższe) niż w referencjach, które mają więcej małych plam ziemi. Wcześniejszy opis „kropkowana ziemia w generatorze” opierał się na oględzinach i nie potwierdza się metryką D i F; metryka nie mierzy jednak rozmiaru pojedynczych „kropek” (zob. zastrzeżenia). Plaża jest ok. 2,8 razy cieńsza niż w referencjach (P′).

**Zmiana modelu 7b.** (1) Jak zapisano wyżej: dopasowanie `transitionWidth` z siatki {0, 500, 1000, 2000, 4000, 8000, 16000} i `shoreWidth` z siatki {0..8}, osobno dla każdego profilu, na seedach 1–4, bok 512, minimalizując sumę względnych błędów po nie-N/A cechach D, F, P′ (spadek współrzędnych: najpierw `transitionWidth`, potem `shoreWidth`). (2) Ponieważ pierścienie plaży powyżej trzeciego nigdy nie powstają (próg szumu `5 + 30 · pierścień` procent przekracza 100 % od pierścienia 4, a wymagany rozmiar jeziora rośnie 3×), samo podniesienie limitu `shoreWidth` do 8 nic nie zmienia. Zmieniam więc regułę: próg szumu pierścienia = min(5 + 30 · pierścień, 80) %, wymagany rozmiar jeziora = min(30 · 3^(pierścień−1), 270) komórek; wyniki dla `shoreWidth` ≤ 2 pozostają bajtowo identyczne, a dla 3 zmienia się próg pierścienia 3 z 95 % na 80 %. Limit walidacji `shoreWidth` z 4 na 8.

**Zastrzeżenia dodatkowe z góry.** Dopasowanie względem D i F może *zwiększyć* poszarpanie ziemi (D rośnie), co może być sprzeczne z oceną wizualną użytkownika; wynik ma trafić do oceny wzrokowej w GUI przed uznaniem poprawki za skuteczną. D i F nie mierzą liczby drobnych „kropek” (składowych ≤ 4 komórek), jeśli takie obserwacje okażą się potrzebne, dodaję miarę osobno z zapisem.

## Iteracja 7c — plaża (zapisana 2026-11-08 po wyniku 7b i po wyborze użytkownika „najpierw plaża”, przed implementacją i pomiarem)

7b nasyciło siatki i P′ doszło tylko do ok. 2 315 (cel ok. 5 640, pasmo 3 777–8 457). Zamiast nowego parametru (zmiana schematu profilu) zmieniam regułę pierścieni dla wszystkich plaż: próg szumu pierścienia = min(5 + 10 · pierścień, 40) % (było min(5 + 30 · pierścień, 80) %), wymagany rozmiar jeziora = min(30 · 3^(pierścień−1), 90) komórek (było do 270). Pierścień 0 bez zmian. Konsekwencja: wyniki dla `shoreWidth` ≥ 1 zmieniają się (pierścień 1: próg 15 % zamiast 35 %), a profile dopasowane wcześniej mają szerszą plażę bez zmiany pliku.

**Wybór `shoreWidth` per profil** (bez zmiany reszty strojenia): wartość z {0..8}, dla której mediana P′ po seedach 1–4 (bok 512, profil v8) jest najbliższa referencji (M06 5 638, M19 5 656); ocena na seedach 1001–1032. Kryterium: P′ w paśmie [0,67·ref; 1,5·ref] w obu profilach. Warunki dodatkowe: udziały rodzajów (mediana) nie różnią się od bazowych 7a o więcej niż 5 punktów procentowych (Shore oczywiście rośnie, więc dla niego próg to 10 punktów, bo to cel zmiany), niezmiennik rzek 100 % seedów. `transitionWidth` pozostaje, jak w v8 (nie zmieniam ziemi w tym kroku).

## Iteracja 8 — masywy skalne (zapisana 2026-11-08 przed implementacją i przed pomiarem)

**Powód.** Użytkownik wybrał punkt „Góry i skała” z listy dalszych cech D9 ([PROJECT_RME_PROCEDURAL_GENERATOR.md](../PROJECT_RME_PROCEDURAL_GENERATOR.md), FAZA 4). Zakres: kształt masywów skalnych na powierzchni (z ≤ 7). Wielopiętrowe ściany do plateau pozostają w FAZIE 5.

**Definicje (stałe, z góry).** Rodzaje jak w iteracji 6 (komórki naturalne; Rock = rodzaj Rock). Komórki Rock w 4-spójnych składowych.
- **RB — gęstość granicy skały**: 1000 · (liczba par 4-sąsiednich Rock–(inny rodzaj naturalny)) / (liczba komórek Rock). Mniejsze = zwarta, gładka skała.
- **RF — zwartość masywów**: ‰ komórek Rock leżących w składowych o polu ≥ 500.
- **RS — pył skalny**: ‰ komórek Rock leżących w składowych o polu < 20.
- Cecha N/A, gdy komórek Rock < 1000. Referencja = wartość zbiorcza mapy (piętra 0..7); generator = mediana po seedach.

**Pomiar bazowy (8a).** Profile m06 i m19 w wersji v10, seedy 1001–1032, bok 512; referencje M06 i M19 (in-sample, m06→M06, m19→M19).

**Kryterium (8a i 8b).** Każda nie-N/A cecha *w paśmie* [0,67·ref; 1,5·ref]; profil „zaliczony”, jeśli wszystkie nie-N/A cechy są w paśmie. Pasmo dla cechy o referencji bliskiej 0 (np. RS poniżej 20 ‰) zastępuję progiem bezwzględnym: wartość generatora ≤ ref + 20 ‰. Warunki dodatkowe dla 8b: udziały rodzajów (mediana) wobec 8a ±5 punktów procentowych; niezmiennik rzek w 100 % seedów; testy rdzenia PASS.

**Zmiana modelu (8b, tylko jeśli 8a pokaże lukę).** Dopasowanie z siatek: `rockPermille` zostaje (udział z profilu), `minRockCells` {8, 24, 64, 160}; ewentualnie wygładzanie granicy skały (nowa reguła zapisana osobno przed wykonaniem, jeśli sama zmiana `minRockCells` nie wystarczy). Dopasowanie na seedach 1–4 bok 512, ocena na 1001–1032.

**Zastrzeżenia z góry.** Dwie referencje; M06 ma mało skały (ok. 5 %), więc RB/RF/RS dla niej mogą być niepewne; skała w referencjach obejmuje „mountain”, „rocky ground” i pokrewne brushe wg mapowania, bez ścian i wielopiętrowych struktur; metryki kształtu, nie ocena wizualna (należy do użytkownika).

### Iteracja 8 — wynik 8a i zapis zmiany modelu 8b (2026-11-08, przed jej wykonaniem)

**8a (v10, bok 512, seedy 1001–1032).** Referencje M06 / M19: RB 333 / 121, RF 167 / 862, RS 36 / 15. Generator m06: RB 49, RF 998, RS 0; m19: RB 33, RF 1000, RS 0. W paśmie: m06 0 z 3, m19 2 z 3 (RF, RS). Skała generatora jest **bardzo gładka i zlana w duże bryły** (granica 3–7 razy mniejsza niż w referencjach); referencyjna skała M06 jest rozdrobniona (RF 167), M19 zwarta, ale z 3,7 razy dłuższą granicą.

**Przyczyna (z kodu, zapisana przed zmianą).** Szum wysokości ma stałe 3 oktawy: krok, krok/2, krok/4 (wagi 4:2:1). Po podniesieniu `featureSize` do 192 w iteracji 6 najdrobniejsza oktawa ma krok 48 i wagę 1/7, więc brzegi skały, jezior i plaży straciły drobny szczegół; to skutek uboczny iteracji 6, nie zmierzony wcześniej.

**Zmiana modelu 8b.** Szum wysokości dostaje dodatkowe oktawy, gdy `featureSize` > 24: liczba oktaw = min(8, max(3, ⌊log2(featureSize/6)⌋ + 1)), wagi 2^(n−1−oktawa), dzielnik 2^n − 1; dla `featureSize` ≤ 24 wynik bajt w bajt jak dotąd (3 oktawy 4:2:1). Szum wilgotności (ziemia/trawa) bez zmian, żeby nie zwiększać poszarpania ziemi, które użytkownik ocenił negatywnie. Ocena na seedach 1001–1032, bok 512; kryterium jak w 8a; warunki dodatkowe: udziały ±5 punktów procentowych wobec v10, niezmiennik rzek, testy rdzenia PASS; dodatkowo raportuję ponownie E4/E16/E64 z iteracji 6 i P′ z iteracji 7c (nie jako kryterium, jako kontrolę skutków ubocznych).

## Iteracja 9 — szlaki (zapisana 2026-11-08 przed implementacją i przed pomiarem)

**Powód.** Użytkownik wybrał „szlaki” z listy dalszych cech (D9, D15; [PROJECT_RME_PROCEDURAL_GENERATOR.md](../PROJECT_RME_PROCEDURAL_GENERATOR.md), FAZA 4: ścieżki). Generator nie tworzy szlaków w ogóle, więc pomiar bazowy generatora jest zerowy z definicji. Ten krok (9a) ustala, **czy szlaki da się zmierzyć w referencjach** i jak wyglądają, zanim cokolwiek zostanie zaprojektowane.

**Definicje (stałe, z góry).**
- *Kafel szlaku*: kafel, którego klucz gruntu (nazwa brusha lub `item <nazwa>` dla gruntu bez brusha) jest jednym z: `cobblestone`, `dark cobblestone`, `grassy cobblestone`, `item cobblestone`, `white gravel`, `pavement`, `item pavement`. Lista wybrana z nazw kluczy występujących w referencjach (nie z obejrzanych kształtów); celowo bez podłóg i płytek (`stone floor`, `gray stone tiles`, `wooden floor`, `tiles`, dachy), bo to podłogi budynków, a nie szlaki.
- *Składowa szlaku*: 4-spójna składowa kafli szlaku, per piętro (z ≤ 7); zaliczona, jeśli pole ≥ 40 kafli, długość szkieletu (Zhang-Suen, jak w iteracjach 4–5) L ≥ 20 i szerokość pole/L ≤ 7 (kształt wąski i długi; szersze płaty to place, nie szlaki).
- Cechy na mapę (zbiorczo): liczba składowych; **Ls** = suma długości szkieletów na 65 536 obecnych kafli; **Ws** = 1000 · pole / L; **Bs** = rozgałęzienia na 100 kafli szkieletu (×1000); **Us** = udział kafli szlaku w obecnych kaflach (‰).
- Mapa referencyjna *kwalifikuje się*, jeśli ma ≥ 5 zaliczonych składowych. Jeśli mniej niż dwie mapy się kwalifikują, ocena szlaków jest niemożliwa i **nie zaczynam implementacji generatora** (zgłaszam to).

**Pomiar 9a.** Wszystkie 19 map referencyjnych (mapowanie v3), bez oceny, tylko opis; wynik decyduje, czy 9b (generowanie szlaków i ocena z pasmami jak w iteracji 5) w ogóle ma podstawy.

**Zastrzeżenia z góry.** Rozpoznanie szlaku po nazwie brusha jest heurystyką (płytka brukowa może być też placem lub wnętrzem); szlak w referencji może być z innego gruntu (np. ubita ziemia) i wtedy nie jest widoczny; tylko M06 i M19 mają takie grunty w większej ilości.

### Iteracja 9 — wynik 9a i zapis 9b: opcjonalne szlaki (2026-11-08, przed implementacją)

**9a (referencje, metryka składowych szlaku).** M06: 22 składowe, L 70, W 3 642, B 30 200, 0,46 % powierzchni; M19: 5 składowych, L 78, W 4 606, B 26 259, 0,72 %; reszta map: 0. Szlaki mierzalne tylko w M06 i M19.

**Decyzja użytkownika (2026-11-08):** prosty szlak opcjonalny (domyślnie wyłączony).

**Model (9b).** Nowy rodzaj komórki `SurfaceKind::Path`; `SurfaceTuning::pathLength` = cel długości szkieletu szlaków na 65 536 komórek obszaru (0 = wyłączone, 0..400); nie jest częścią profilu stylu (schemat profilu bez zmian), ustawia go użytkownik w oknie generatora. Szlaki powstają po rzekach i plażach: trasy między losowymi komórkami lądu (Grass lub Dirt) w odległości 48–160, wyszukiwane A* po komórkach Grass, Dirt i Shore (koszt Shore wyższy), z szumem w koszcie dla krętości; rzeki, jeziora i skała są nieprzekraczalne (brak mostów w tym kroku); szerokość 3–4 komórek (stempel 3×3 z jednostronnym poszerzeniem szumem); szlak nadpisuje tylko Grass, Dirt i Shore. Dopóki długość szkieletu poniżej celu, dodawane są kolejne trasy (limit prób); `pathLength` 0 daje wynik bajt w bajt jak dotąd. Resolver: profil z 7 pędzlami (szósty indeks: pędzel szlaku) obsługuje komórki Path; profil z 6 pędzlami odrzuca siatkę zawierającą Path; wygenerowane szlaki nie dostają roślinności.

**Ocena (seedy 1001–1032, bok 512, profile v10 z m06 i m19).** `pathLength` dobrany tak, by L było możliwie bliskie referencji danego profilu (siatka {40, 80, 160, 320} na seedach 1–4), pomiar tą samą definicją składowej szlaku co w 9a (pole ≥ 40, szkielet ≥ 20, szerokość ≤ 7) na komórkach Path. Kryterium: L, W i B w paśmie [0,67·ref; 1,5·ref] (ref: m06 70 / 3 642 / 30 200; m19 78 / 4 606 / 26 259), co najmniej 75 % seedów z ≥ 1 składową; warunki dodatkowe: udziały pozostałych rodzajów (mediana) wobec stanu bez szlaków ±1 punkt procentowy, szlak nigdy na Rock, Lake, River (test rdzenia), niezmienniki rzek i przypięty odcisk Seed 1729 (przy `pathLength` 0) bez zmian, testy rdzenia PASS.

**Zastrzeżenia z góry.** Cele z dwóch map (M19 z dokładnie 5 składowymi); generator nie ma celów szlaków (osad, bram) i nie łączy szlaków z mostami; szlaki są losowymi trasami po lądzie, bez wniosku o realizmie; brush szlaku wybiera użytkownik (domyślnie `cobblestone`, jeśli istnieje w kliencie).

### Iteracja 9c — wygładzenie szlaków (zapis sporządzony PO pomiarze: zaniedbanie procesu)

**Zastrzeżenie dotyczące procesu.** Zapis kryteriów 9c miał powstać przed zmianą kodu, ale polecenie, które go zapisywało, zakończyło się błędem składni i niczego nie zapisało; zauważyłem to dopiero po pomiarze. Treść poniżej była przedstawiona użytkownikowi w czacie przed zmianą (zgoda: „Tak.”), a kryteria są te same co w 9b, więc nie zostały dobrane pod wynik, ale formalnie **nie jest to wcześniejsza rejestracja**.

Powód: oględziny użytkownika w GUI (m19, `pathLength` 80) pokazały szlaki skręcające schodkowo pod kątem prostym i jeden biegnący krawędzią skały. Zmiana: (1) ruchy po przekątnej w A* (koszt 14 zamiast 10, heurystyka oktylowa), (2) dodatkowy koszt komórek sąsiadujących ze skałą, (3) wygładzenie trasy średnią ruchomą (okno ±6) i stemplowanie wzdłuż odcinków między wygładzonymi punktami. Szlak nadal nadpisuje tylko Grass, Dirt, Shore. Ocena: kryteria jak w 9b (L, W, B w paśmie referencji, `pathLength` dobierany na seedach 1–4 z {40, 80, 160, 320}, ocena na seedach 1001–1032, bok 512), plus kontrola udziału komórek szlaku przy skale.

### Iteracja 9d — silniejsze odpychanie szlaków od skały (zapisana 2026-11-08 przed zmianą, po pomiarze stanu bazowego)

Powód: użytkownik (po GUI 9c): szlak wokół masywu biegnie tuż przy skale; „ma bardziej odpychać”. Użytkownik potwierdził też, że Validate i Undo/Redo ze szlakami przeszły. Stan bazowy (v10, seedy 1001–1032, bok 512, po 9c): udział komórek szlaku z komórką Rock w promieniu 3 (Czebyszew), mediana po seedach: m06 0 ‰, m19 87,5 ‰ (kontrola „rocknear”).

Zmiana: koszt komórki w A* rośnie z bliskością skały zamiast stałych +25 dla sąsiedztwa 4: dodatek 60 / 40 / 25 / 12 dla odległości 1 / 2 / 3 / 4 od Rock (odległość Czebyszewa, wyznaczona raz na obszar). Reszta bez zmian.

Kryterium: (1) rocknear (średnia i mediana po seedach) w m19 co najmniej o połowę niższy niż w stanie bazowym; (2) L i W nadal w paśmie referencji (jak w 9b); (3) testy rdzenia PASS, szlak nigdy na Rock/Lake/River, wyłączone = bajt w bajt. B nie wchodzi do kryterium (artefakt szkieletowania opisany w 9c). Zastrzeżenie: mediana jest niska już w stanie bazowym (szlak przy skale to rzadki przypadek), więc „połowa” dotyczy małej liczby; ocena wzrokowa należy do użytkownika.

## Iteracja 10 — przejścia między rodzajami (zapisana 2026-11-08 przed implementacją i przed pomiarem)

**Powód.** Użytkownik wybrał „przejścia” (FAZA 4: przejścia biomów, [PROJECT_RME_PROCEDURAL_GENERATOR.md](../PROJECT_RME_PROCEDURAL_GENERATOR.md)). Obecna miara G4 porównuje tylko sąsiedztwo ogólne (rozkład par rodzajów w całej mapie). Przejście oznacza tu: **z czym graniczy dany rodzaj** (np. czy skała styka się z ziemią, trawą, piaskiem czy wodą).

**Definicje (stałe, z góry).** Rodzaje jak w iteracji 6 (komórki naturalne; Lake i River jako Water). Para = dwie 4-sąsiednie komórki obu naturalnych, o różnych rodzajach.
- *Skład granicy skały*: wśród par, w których jedna komórka jest Rock, udział (‰) par, w których druga to Dirt (**RD**), Grass (**RG**), Shore (**RS**), Water (**RW**).
- *Skład granicy ziemi*: wśród par z komórką Dirt (druga komórka inna niż Dirt) udział (‰) par z Grass (**DG**), Rock (**DR**), Shore (**DS**), Water (**DW**).
- Skład N/A, gdy liczba par danej granicy < 1000. Referencja = wartość zbiorcza mapy (piętra 0..7); generator = mediana po seedach.

**Pomiar bazowy (10a).** Profile m06 i m19 v10, seedy 1001–1032, bok 512; referencje M06 i M19 (m06→M06, m19→M19, in-sample). Najpierw tylko opis i ocena luk, bez zmiany generatora.

**Kryterium (dla 10a i ewentualnego 10b).** Cecha *w paśmie*, gdy mediana generatora leży w [0,67·ref; 1,5·ref], a dla cechy o ref < 50 ‰ gdy |gen − ref| ≤ 50 ‰. Profil „zaliczony”, jeśli ≥ 6 z (nie-N/A) 8 cech jest w paśmie. Ewentualna zmiana generatora (10b) zostanie zapisana osobno przed wykonaniem; warunki dodatkowe dla niej: udziały rodzajów ±5 punktów procentowych, testy rdzenia, przypięty odcisk Seed 1729 bez zmian dla `pathLength` 0 (chyba że zmiana jest jawnie zapisana jako zmieniająca wyniki).

**Zastrzeżenia z góry.** Dwie referencje, in-sample; skład granicy nie rozróżnia, czy styk jest gładki, czy poszarpany (to opisuje D/RB z iteracji 7–8); w referencjach przejście przez element „Other” (budynki, drogi) jest poza miarą; wartości, w których większość granicy to woda (Shore/Water), zależą od udziału wody i plaży, już dopasowanych w iteracjach 6–7c.

### Iteracja 10 — wynik 10a i zapis zmiany 10b (2026-11-08, przed jej wykonaniem)

**10a (v10, bok 512, seedy 1001–1032).** Referencje M06 / M19: skład granicy skały RD/RG/RS/RW = 33/938/24/4 i 99/502/251/147; skład granicy ziemi DG/DR/DS/DW = 951/32/12/4 i 728/155/27/88. Generator m06: RD 254, RG 717, RS 0, RW 29; DG 741, DR 55, DS 145, DW 47 — w paśmie 6 z 8 (poza: RD i DS). Generator m19: RD 322, RG 661, RS 0, RW 15; DG 614, DR 148, DS 194, DW 44 — w paśmie 3 z 8 (poza: RD, RS, RW, DS, DW).

**Odczyt luk.** (1) Ziemia w generatorze styka się ze skałą i plażą mniej więcej proporcjonalnie do swojego udziału (RD ≈ udział ziemi w lądzie, ok. 30 %), a w referencjach ziemia **unika** skały (RD 33–99) i plaży (DS 12–27): wokół skały i plaży jest trawa. (2) W M19 skała styka się z plażą i wodą (RS 251, RW 147), w generatorze prawie nigdy (RS 0, RW 15), bo skała to najwyższe, a jezioro najniższe komórki wysokości; tego nie da się naprawić regułą lokalną bez zmiany modelu wysokości, więc 10b tego nie dotyczy.

**Zmiana modelu 10b (tylko punkt 1).** Po wyznaczeniu skały i plaży (przed rzekami) każda komórka Dirt w odległości Czebyszewa ≤ 2 od komórki Rock lub Shore zmienia się w Grass („obwódka trawy”). Parametrów nie ma (stała reguła, dotyczy wszystkich profili); zmienia wyniki Surface (także dla `pathLength` 0), więc przypięty odcisk Seed 1729 100×100 zostanie świadomie zmieniony.

**Kryterium 10b.** RD i DS w paśmie w obu profilach (m06 i m19); cechy będące w paśmie w 10a pozostają w paśmie; udziały rodzajów (mediana) wobec 10a ±5 punktów procentowych; testy rdzenia PASS; niezmienniki rzek i szlaków bez zmian (testy). RS i RW dla M19 nie są kryterium (zob. luka 2).

**Zastrzeżenia z góry.** Obwódka trawy zmniejsza udział ziemi i jej poszarpanie przy skale; ocena wzrokowa należy do użytkownika; in-sample na dwóch referencjach.

### Iteracja 10 — wynik 10b i zapis 10c (2026-11-08, przed wykonaniem 10c)

**10b (pełna obwódka trawy 2 komórki wokół Rock i Shore; v10, bok 512, seedy 1001–1032).** m06: 8 z 8 cech w paśmie (RD 0, DS 0 i DR 0 mieszczą się w paśmie bezwzględnym ±50 ‰ wokół referencji 33/12/32). m19: 2 z 8 (RD 0 wobec ref. 99 poza pasmem [66, 148]; DR 0 wobec 155 poza pasmem, a w 10a było w paśmie; DS 0 w paśmie bezwzględnym). **Kryterium 10b nie spełnione dla m19**: obwódka całkowicie usuwa styk ziemi ze skałą, a w M19 jest on niezerowy. Udziały rodzajów w granicach (m06: Grass 403, Dirt 160, Shore 45; m19: 295 / 114 / 45 ‰; zmiana < 5 punktów procentowych wobec 10a).

**Zmiana 10c.** Obwódka wokół Rock jest częściowa: komórka Dirt w odległości Czebyszewa ≤ 2 od Rock zmienia się w Grass, gdy szum wartościowy (krok 4) komórki jest poniżej 75 % zakresu (prawdopodobieństwo zmiany ok. 75 %); obwódka wokół Shore pozostaje pełna (ref. DS 12–27 jest bliskie zeru). Wartość 75 % wybrana tak, by zostawić ok. jedną czwartą styku (referencje RD 33–99 przy 254–322 w 10a); wybrana po zobaczeniu 10a i 10b (adaptacyjnie, jedna wartość, bez przeszukiwania siatki).

**Kryterium 10c.** Jak w 10b: RD i DS w paśmie w obu profilach, cechy będące w paśmie w 10a pozostają w paśmie (m.in. DR w m19), udziały ±5 punktów procentowych, testy rdzenia PASS. Jeśli DR w m19 wypadnie z pasma (możliwe: RD i DR zależą od tego samego styku, ale mają różne mianowniki), zgłaszam to jako niespełnione i **nie** szukam dalszych parametrów dla metryk; wybór między pełną a częściową obwódką przechodzi wtedy do oceny wzrokowej użytkownika.

## Iteracja 11 — osady i struktury, część 1: pomiar budynków w referencjach (zapisana 2026-11-08 przed implementacją i przed pomiarem)

**Powód.** Użytkownik wybrał „osady i struktury” (ostatni punkt z listy; FAZA 5 planu: struktury, osady, ruiny, obozowiska). Generator nie tworzy żadnych budowli, więc stan bazowy jest zerowy z definicji. Ten krok (11a) ustala, **jak wyglądają budynki w referencjach i czy da się je zmierzyć**; decyzja o zakresie generowania (ściany, drzwi, podłogi przez pędzle RME) zapada dopiero po jego wyniku.

**Definicje (stałe, z góry).** Piętra z ≤ 7. *Komórka muru*: kafel z cechą Wall lub Door. *Budynek*: 8-spójna składowa komórek muru; zaliczona, jeśli ma ≥ 12 komórek i obwiednię ≥ 4 × 4. Cechy na mapę: liczba budynków; **Bd** = budynki na 65 536 obecnych kafli; mediana dłuższego i krótszego boku obwiedni (**Bl**, **Bs**); mediana liczby komórek muru (**Bw**); średnia liczba komórek Door na budynek (**Bdoor**, ×1000); udział budynków z ≥ 1 drzwiami (‰, **Bdp**). Mapa referencyjna *kwalifikuje się*, gdy ma ≥ 5 budynków; bez ≥ 2 takich map pomiar jest niemożliwy i generowanie struktur nie ma podstaw z danych.

**Pomiar 11a.** Wszystkie 19 map referencyjnych, tylko opis; wynik określa zakres ewentualnego 11b (zapis osobno przed wykonaniem). Bez oceny generatora.

**Zastrzeżenia z góry.** Cecha Wall pochodzi z definicji itemów klienta (brusha ściany); mur wolnostojący, płot i ściana jaskini też ją mają, więc „budynek” to heurystyka kształtu; wnętrza, dachy (piętra wyżej) i meble są poza miarą; referencje to głównie M06 i M19.

### Iteracja 11 — wynik 11a i zapis 11b: podstawy budynków (2026-11-08, przed implementacją)

**11a (referencje).** M06: 1 313 budynków, 31 na 65 536 kafli, mediana 9 × 7, ok. 30 komórek muru, 1,66 drzwi na budynek; M19: 582, 37, 9 × 7, 24 komórki, 3,44 drzwi; M05, M15, M01 mniej i inne (M01 to raczej płoty). Pomiar możliwy w 5 mapach.

**Decyzja użytkownika (2026-11-08):** zakres „same podstawy budynków”, bez ścian i drzwi.

**Model 11b.** Nowy rodzaj komórki `SurfaceKind::Floor` i parametr `SurfaceTuning::buildingDensity` = cel liczby budynków na 65 536 komórek obszaru (0 = wyłączone, 0..100, domyślnie 0; nie jest częścią profilu stylu). Po szlakach: losowe prostokąty podłogi, dłuższy bok 7–11 (jednostajnie), krótszy 5–9 (jednostajnie, nie większy niż dłuższy), losowa orientacja; prostokąt razem z marginesem 2 komórek musi leżeć w całości na Grass lub Dirt (bez Path, Shore, Rock, Lake, River, Floor); do 40 prób na budynek, przy braku miejsca budynek jest pomijany. Podłoga ma pędzel podłogi (okno generatora, domyślnie `stone floor`, jeśli istnieje w kliencie); resolver: profil z 8 pędzlami (indeks 7: podłoga; indeks 6: szlak, wypełniany zawsze, gdy podłoga włączona) dopuszcza komórki Floor; profile z 6 i 7 pędzlami odrzucają siatkę z Floor. Brak ścian, drzwi i dachów; podłoga nie dostaje roślinności ani szlaków.

**Ocena (seedy 1001–1032, bok 512, profile v10; `buildingDensity` 30 dla m06 i 40 dla m19, stałe z góry, bez dopasowania).** Składowa podłogi: 4-spójna składowa Floor. Cechy: liczba składowych na 65 536 komórek (ref. m06 31, m19 37), mediana dłuższego boku obwiedni (ref. 9, 9), mediana krótszego (ref. 7, 7). Kryterium: wszystkie trzy cechy w paśmie [0,67·ref; 1,5·ref] w obu profilach; warunki dodatkowe: Floor tylko na dawnym Grass lub Dirt (test rdzenia), brak nakładania się budynków, udziały rodzajów (mediana) wobec stanu bez budynków ±1 punkt procentowy, `buildingDensity` 0 daje wynik bajt w bajt jak dotąd (przypięty odcisk Seed 1729), testy PASS.

**Zastrzeżenia z góry.** Podłoga bez ścian nie jest budynkiem; porównanie obwiedni podłogi z obwiednią muru referencji to przybliżenie (mur obejmuje obrys, podłoga wnętrze, więc podłoga bywa o 2 komórki mniejsza); rozmieszczenie jest losowe (bez dróg, bez skupisk osad); liczba budynków z dwóch map.

### Iteracja 11c — ściany budynków (zapisana 2026-11-08 przed implementacją; decyzja użytkownika: „wolę razem ze ścianami, drzwi niewymagane”)

**Zakres.** Do podstaw budynków (11b) dochodzi obrys ścian. Ściany to przedmioty na kaflu (nie grunt): plan zyskuje warstwę przedmiotów ścian, kładzionych jak roślinność po obliczeniu borderów gruntu, a dopasowanie elementów ściany (poziome, pionowe, narożniki) robi istniejący mechanizm RME (`WallBrush::doWalls`) na buforze podglądu, bez drugiego silnika. Drzwi: poza zakresem (budynek ma ciągły obrys).

**Model.** `GroundProfile::wall` = nazwa pędzla ściany (puste = bez ścian; dozwolone tylko przy włączonych podstawach); `GroundPlan::walls` = identyfikator przedmiotu ściany (pierwszy element pędzla, `WallBrush::getPlacementItemId`) dla każdej *komórki obrysu*: komórki Floor mającej 4-sąsiada innego rodzaju (dla prostokąta 9×7: 28 komórek). Resolver sprawdza, że pędzel istnieje, daje niezerowy identyfikator i że przedmiot nie jest gruntem ani meta-itemem; podgląd sprawdza, że każdy przedmiot ściany należy do pędzla profilu (`hasWall`), leży na komórce z gruntem planu i że plan nie ma ściany poza maską. Okno generatora: pole wyboru „Walls” i pędzel ściany; ściany tylko przy „Buildings” > 0. Odcisk podglądu i plan zawierają warstwę ścian.

**Ocena.** (1) Metryka rdzenia: mediana liczby komórek obrysu na budynek (komórki Floor z 4-sąsiadem nie-Floor) w paśmie [0,67·ref; 1,5·ref] względem mediany komórek muru w referencjach (m06 30, m19 24), seedy 1001–1032, bok 512, gęstość jak w 11b (30 / 40). (2) Test rdzenia: obrys ma 2·(dłuższy+krótszy)−4 komórek dla każdego prostokąta. (3) Weryfikacja na rzeczywistym kliencie w GUI przez użytkownika (Preview, Validate, Place, Undo/Redo ze ścianami; poprawność narożników). (4) Regresja G1 `--commit` i `--surface` bez zmian wyników. Warunek dodatkowy: brak ścian poza maską i na komórkach chronionych (kontrola podglądu).

**Zastrzeżenia z góry.** Metryka (1) wynika z rozmiarów zadanych w 11b (dla prostokąta obrys to 28 komórek) i jest sprawdzeniem implementacji, nie niezależnym dowodem; dopasowanie ścian (narożniki, T) zależy od mechanizmu RME i klienta, który zweryfikuje użytkownik; budynek bez drzwi nie jest wejściem do rozgrywki; dachów i wnętrz brak; sonda G1 nie obejmuje jeszcze ścieżki ze ścianami.


## Iteracja 12 — osady i struktury, część 2: skupiska, drzwi i szlaki do drzwi (zapisana 2026-11-09 przed implementacją i przed pomiarem)

**Zakres (decyzja użytkownika 2026-11-09):** drzwi (jedno wejście na budynek, pędzel drzwi przez istniejący mechanizm RME), szlaki prowadzące do drzwi, skupiska budynków zamiast losowego rozmieszczenia. Ten zapis obejmuje **12a: definicje i pomiar w referencjach** oraz **kryteria oceny generatora (12c)**. Projekt (12b) zostanie zapisany osobno, po wyniku 12a, przed implementacją.

**Definicje (stałe, z góry).** Piętra z 0..7, każde piętro osobno. *Budynek* jak w 11a (8-spójna składowa kafli Wall lub Door, ≥ 12 komórek, obwiednia ≥ 4 × 4); *obwiednia* = prostokąt [x0..x1] × [y0..y1] składowej. Po stronie generatora budynek to 4-spójna składowa Floor (jej obwiednia jest obwiednią obrysu ścian). *Odstęp* dwóch budynków na tym samym piętrze = max(dx, dy), gdzie dx = max(0, a.x0 − b.x1, b.x0 − a.x1), analogicznie dy (budynki sąsiadujące bezpośrednio mają odstęp 1). *Kafel szlaku* = klucz gruntu z listy 9a (`cobblestone`, `dark cobblestone`, `grassy cobblestone`, `item cobblestone`, `white gravel`, `pavement`, `item pavement`); po stronie generatora komórka Path. *Kafel wody* = rodzaj Water/Lake/River wg tego samego mapowania (keymap) co w iteracjach 4–10. *Strona* budynku = jedna z czterech krawędzi obwiedni (N, E, S, W); *drzwi brzegowe* = kafel Door leżący na krawędzi obwiedni, a jego strona to ta krawędź (narożnik liczy się do obu). *Wyjście na zewnątrz* od drzwi = kolejne kafle w kierunku prostopadłym do strony, poza obwiednią.

Cechy na mapę (zbiorczo; N/A, gdy mapa ma < 30 budynków):
- **G** — mediana (po budynkach z co najmniej jednym sąsiadem na piętrze) najmniejszego odstępu do innego budynku.
- **Skupisko** = składowa łączenia pojedynczego (single linkage) budynków z odstępem ≤ 12 (stała). **Kn** = skupiska na 65 536 obecnych kafli (×1000); **Ks** = mediana liczby budynków w skupisku; **Ki** = udział budynków samotnych (skupisko 1) w ‰; **Kb** = udział budynków w skupiskach ≥ 3 w ‰.
- **Bp** = udział budynków, mających kafel szlaku w odległości Czebyszewa ≤ 3 od obwiedni (‰); kontrola **Cp** = udział obecnych kafli niebędących wodą ani szlakiem, mających kafel szlaku w odległości ≤ 3 (‰). **Bv** = udział budynków z kaflem wody w odległości ≤ 8 od obwiedni (‰); kontrola **Cv** = to samo dla obecnych kafli (bez wody). Wniosek o relacji budynek–szlak lub budynek–woda wyłącznie jako Bp względem Cp i Bv względem Cv.
- **Dh** = udział budynków z ≥ 1 kaflem Door (‰); **Dn** = mediana liczby kafli Door na budynek wśród budynków z drzwiami; **Db** = udział kafli Door leżących na krawędzi obwiedni (‰) — pozostałe to drzwi wewnętrzne lub okna-drzwi zaliczone przez heurystykę.
- **Dp** (orientacja względem szlaku): zbiór S = budynki z ≥ 1 drzwiami brzegowymi i z kaflem szlaku w odległości ≤ 3 od obwiedni. *Strona ze szlakiem* = strona, dla której z jakiegokolwiek kafla jej krawędzi wyjście na zewnątrz w ≤ 3 krokach trafia w kafel szlaku. **Dp** = udział budynków z S, w których przynajmniej jedna strona z drzwiami jest stroną ze szlakiem (‰). Kontrola **Dp0** = średnia po S z |stron ze szlakiem| / 4 (‰) — oczekiwany wynik przy losowej stronie wejścia. Wniosek o orientacji tylko jako Dp względem Dp0. N/A, gdy |S| < 30.

**Pomiar 12a.** Wszystkie 19 map referencyjnych, tylko opis, bez oceny generatora; wyniki z liczebnością (budynków, |S|). Mapa *kwalifikuje się* do cech skupisk i odstępów, gdy ma ≥ 30 budynków (spodziewane: M06, M19; M05 z 13 budynkami nie kwalifikuje się). Jeśli kwalifikują się < 2 mapy, ocena skupisk jest niemożliwa i zgłaszam to przed projektem.

**Kryteria oceny generatora (12c; seedy 1001–1032, bok 512, profile v10 m06 i m19 z ustawieniami jak w `g5_shape_table.py`: m06 pathLength 40 / buildingDensity 30, m19 80 / 40; wartości progu gęstości budynków nie są dopasowywane po zobaczeniu wyniku).** Cecha *w paśmie*, gdy mediana generatora leży w [0,67·ref; 1,5·ref]; dla cech w ‰ (Ki, Kb, Bp, Bv, Dh, Db, Dp) oraz dla ref < 50: |gen − ref| ≤ 50 ‰ (bezwzględnie). Cechy oceniane: G, Kn, Ks, Ki, Kb, Bp, Bv (każda także względem kontroli Cp, Cv: kierunek różnicy Bp − Cp i Bv − Cv zgodny z referencją), Dp (kierunek Dp − Dp0 zgodny z referencją). **Odstępstwa celowe, nie liczone do pasma:** Dn (zamysł: dokładnie jedno wejście na budynek) i Db (wszystkie drzwi na krawędzi). Profil „zaliczony”, jeśli ≥ 75 % ocenianych (nie-N/A) cech jest w paśmie lub ma zgodny kierunek. Warunki dodatkowe: każdy budynek ma dokładnie jedne drzwi (test rdzenia), drzwi leżą na komórce obrysu ścian z sąsiadem zewnętrznym będącym komórką przechodnią (nie wodą, nie skałą), od drzwi do szlaku istnieje droga (komórki Grass/Dirt/Path/Shore) w ≤ 25 krokach lub budynek jest zgłoszony jako bez dojścia (odsetek raportowany), przy buildingDensity 0 i pathLength 0 wynik bajt w bajt jak przed iteracją (przypięty odcisk Seed 1729), 14 testów rdzenia i obie sondy G1 (`g5_run_suite.py --probes`) PASS, dwa przebiegi bajtowo identyczne.

**Zastrzeżenia z góry.** Cechy referencji pochodzą z dwóch kwalifikujących się map (D17), więc pasma są in-sample i nie dowodzą ogólności; Wall/Door w referencji obejmuje płoty, murki i bramy (część „budynków” to nie domy), a drzwi wewnętrzne i podwójne drzwi zaniżają Db i zawyżają Dn; odległości 3, 8 i 12 są stałymi z góry, a nie dopasowanymi; kontrola Cp/Cv jest najprostszą kontrolą obszarową, nie uwzględnia, że budynki stoją na lądzie z natury daleko od skały; orientacja drzwi względem szlaku zależy od tego, które kafle szlaku referencja ma w kluczach 9a (szlaki z ubitej ziemi są niewidoczne); piętra > 7 i wnętrza poza miarą; ocena wzrokowa należy do użytkownika i ma pierwszeństwo przed metryką (rozbieżność odnotuję).

### Iteracja 12 — wynik 12a, stan bazowy generatora i zapis 12b: osady (2026-11-09, przed implementacją)

**12a (referencje, M06 / M19 / pomocniczo M01).** Odstęp najbliższego sąsiada G = 4; skupiska ok. 11–12 na 65 536 kafli (Kn 11 167 / 11 955), mediana skupiska 1, samotnych (Ki) 265 / 189 ‰, w skupiskach ≥ 3 (Kb) 661 / 714 ‰; budynki przy szlaku: Bp 93 / 209 ‰ wobec kontroli Cp 14 / 25 ‰; woda bez związku (Bv ≤ Cv); drzwi brzegowe zwrócone ku szlakowi: Dp 855 / 750 ‰ wobec Dp0 594 / 546 ‰ (|S| = 69 / 76). Szczegóły i zastrzeżenia: [G4_GATE_STATUS.md](G4_GATE_STATUS.md), „Iteracja 12a”.

**Stan bazowy generatora (seedy 1001–1032, bok 512, v10, mediana [min..max]; `g5_shape_table.py`).** m06: G 10 [8..11], Kn 16 875, Ks 1, Ki 358, Kb 442, Bp 0 [0..33], Cp 10, Bv 83, Cv 189, Dh 0. m19: G 6 [5..7], Kn 11 750, Ks 2, Ki 131, Kb 762, Bp 15, Cp 17, Bv 125, Cv 173, Dh 0. Drzwi i orientacja: N/A (brak drzwi). Dwa przebiegi bajtowo identyczne.

**Model 12b (stałe z góry, nie dopasowywane po zobaczeniu wyniku; działa tylko przy `buildingDensity` > 0, przy 0 wynik bajt w bajt jak dotąd).**
1. *Skupiska.* Budynki stawiane kolejno: **30 %** prób to kotwice (losowe położenie jak dotąd), **70 %** to dołączenia: wybierany jest losowo jeden z już postawionych budynków, jedna z jego czterech stron i odstęp **3–6 kafli** (jednostajnie; margines 2 komórek wolnych dla Grass/Dirt zostaje, więc najmniejszy odstęp to 3), przesunięcie wzdłuż strony losowe tak, by obwiednie miały wspólny zakres. Rozmiary budynków i limit prób (40 na budynek) bez zmian; nieudane dołączenie liczy się jako próba.
2. *Drzwi.* Jeden kafel drzwi na budynek, na środkowej komórce obrysu strony zwróconej ku celowi (cel: najbliższy kafel Path w odległości Czebyszewa ≤ 60 od środka budynku, w chwili stawiania budynku w kolejności umieszczenia; bez celu strona losowa). `SurfaceGrid::doors` (indeksy komórek) jest nowym polem; nie zmienia rodzajów komórek. Komórka drzwi odpada, jeśli po masce i ochronie przestała być Floor.
3. *Szlaki do drzwi.* Komórka przed drzwiami (schodek) staje się Path; od schodka prowadzi trasa tą samą procedurą co szlaki 9d (A*, wygładzenie, stemplowanie 3–4 komórek; wspólna funkcja wyodrębniona z `drawPaths` bez zmiany jej wyniku) do celu z pkt 2. Budynek bez celu dostaje tylko schodek i zasila sieć: kolejne budynki łączą się z najbliższym kaflem Path. Trasa odrzucona przez limit długości (3 × odległość Czebyszewa) lub brak drogi = budynek „bez dojścia”; ich odsetek jest raportowany.
4. *Pędzel drzwi* (adapter, część 2 etapu 12; osobno od rdzenia): `GroundProfile::door` = nazwa pędzla drzwi RME; drzwi kładzie istniejący `DoorBrush` na komórce ściany po `WallBrush::doWalls`; bez drugiego silnika. Nie jest częścią tej decyzji liczbowej.

**Warunki dodatkowe 12b (rdzeń).** `buildingDensity` 0: odcisk Seed 1729 bez zmian (`pathLength` 0 i > 0); `pathLength` > 0 przy `buildingDensity` 0 bez zmian bajt w bajt; dla `buildingDensity` > 0 wyniki **świadomie się zmieniają** (nowe rozmieszczenie, schodki i szlaki), więc przypięte odciski takich przypadków zostaną zaktualizowane jawnie w commicie; dokładnie jedne drzwi na każdy budynek i drzwi na komórce obrysu Floor; schodek jest Path; 14 testów rdzenia + nowy test PASS; `g5_run_suite.py --probes` PASS po przebudowaniu `rme`; dwa przebiegi bajtowo identyczne.

**Ocena 12c** wg kryteriów zapisanych wyżej (pasma, seedy 1001–1032, profile m06 i m19, `buildingDensity` 30 i 40, `pathLength` jak w `g5_shape_table.py`). Pozostaje: GUI (pędzel drzwi, widok, Preview/Validate/Place/Undo/Redo) do oceny użytkownika.

**Zastrzeżenia z góry.** Procent 30/70, odstępy 3–6, zasięg 60 i szerokość trasy wybrane z referencji M06/M19 (G 4, ok. 2,8 budynku na skupisko), nie dopasowane na seedach generatora; zmiana tych stałych po zobaczeniu wyniku wymaga nowego zapisu i oznaczenia „po zmianie modelu”. Pomiar G i Kn jest wrażliwy na stałe 3–6 i 12 (czułości nie badano). Drzwi w generatorze to jedno wejście (idealizacja), referencje mają ich więcej (Dn 2 / 4).

### Iteracja 12 — wynik 12c (model 12b) i zapis 12d: udział kotwic (2026-11-09; zapis przed zmianą stałej i przed ponownym pomiarem)

**Wynik 12c dla modelu 12b (commit rdzenia z 30 % kotwic; seedy 1001–1032, bok 512, v10, mediana).** Kryterium „≥ 75 % ocenianych cech w paśmie lub ze zgodnym kierunkiem” **niespełnione w obu profilach (4 z 8)**.

| Cecha | m06 ref → gen | m06 | m19 ref → gen | m19 |
| --- | --- | --- | --- | --- |
| G | 4 → 4 | w paśmie | 4 → 4 | w paśmie |
| Kn | 11 167 → 5 875 | **poza** (pasmo 7 482–16 750) | 11 955 → 4 750 | **poza** (8 010–17 932) |
| Ks | 1 → 2 | **poza** (0,67–1,5) | 1 → 3 | **poza** |
| Ki | 265 → 62 | **poza** (\|Δ\| 203 > 50) | 189 → 37 | **poza** (152) |
| Kb | 661 → 850 | **poza** (189) | 714 → 928 | **poza** (214) |
| Bp − Cp | +79 → +924 | kierunek zgodny | +184 → +908 | zgodny |
| Bv − Cv | −29 → −130 | kierunek zgodny | −23 → −98 | zgodny |
| Dp − Dp0 | +261 → +503 | kierunek zgodny | +204 → +495 | zgodny |

Odstępstwa celowe (poza pasmem z założenia): Dn 1 (ref 2 / 4), Db 1000 (ref 231 / 563), Dh 1000. Warunki dodatkowe rdzenia: jedne drzwi na budynek, drzwi na obrysie, schodek Path, odstęp ≥ 3, `buildingDensity` 0 bez zmian (odcisk Seed 1729 i PNG `pathLength` 80 / `buildingDensity` 0 bajt w bajt jak przed zmianą), 15 testów PASS.

**Obserwacje poza kryteriami.** (1) Bp = 1000 z konstrukcji (każdy budynek ma schodek), więc nie odróżnia od referencji tak jak Bp = 93 / 209; kierunek jest spełniony trywialnie. (2) Długość szlaków (L na 65 536) rośnie z 53 / 89 (9d, bez budynków) do 243 / 280 przy budynkach, bo każdy budynek dostaje trasę do najbliższej sieci; referencje mają 70 / 78 (cała mapa, wg listy 9a). Szlaki zajmują więc wyraźnie więcej niż w referencji. (3) Budynki są zbyt zbite: Ki 62 / 37 wobec 265 / 189 ‰.

**Zmiana 12d (jedna stała, zapisana przed ponownym pomiarem; wynik oznaczony „po zmianie modelu”).** `clusterAnchorPercent` 30 → **60**. Uzasadnienie wyłącznie z tabeli: referencje mają 19–27 % budynków samotnych i skupiska o medianie 1; generator przy 30 % kotwic ma 4–6 % samotnych. Wartość 60 wybrana jako okrągła połowa drogi do przewidywanej wartości, nie dopasowana na seedach; jeśli i ona nie spełni kryterium, nie dobieram dalej bez nowej decyzji użytkownika. Pozostałe stałe (odstęp 3–6, zasięg 60, 12 w definicji skupiska) bez zmian. Kryteria oceny te same co w 12c (powyżej). Długość szlaków (obserwacja 2) w tej iteracji nie jest zmieniana ani oceniana; zostaje zgłoszona jako odchylenie.

**Zastrzeżenia z góry.** Zmiana po zobaczeniu wyniku in-sample na tych samych seedach i mapach (D17); poprawa Ki/Kn przy jednej stałej nie dowodzi ogólności. Ks i Kb mogą nie zmienić się zgodnie z oczekiwaniem, bo zależą od rozkładu rozmiarów skupisk, a nie od jednego parametru.

### Iteracja 12e — adapter drzwi (zapisana 2026-11-09 przed implementacją; decyzja użytkownika: „Adapter i GUI drzwi”)

**Zakres.** Drzwi z rdzenia (`SurfaceGrid::doors`) trafiają do planu i podglądu istniejącym mechanizmem RME, bez drugiego silnika; wyniki rdzenia i kryteria 12c się nie zmieniają.
- `GroundProfile::door` = rodzaj drzwi z zamkniętej listy nazw (Normal door, Locked door, Magic door, Quest door, Normal door (alt), Archway), mapowanych na `DoorType`; puste = bez drzwi (ściana ciągła jak dotąd). Dozwolone tylko przy pędzlu ściany (profil z 8 pędzlami). Żadnych identyfikatorów przedmiotów w kodzie: element drzwi wybiera pędzel ściany profilu przez `DoorBrush`.
- `GroundPlan::doors` = flaga na komórkę (jedna na budynek, na komórce obrysu ze ścianą); dopisywana do odcisku planu tylko przy drzwiach, więc plany bez drzwi zachowują dotychczasowy odcisk.
- Podgląd: po `WallBrush::doWalls` na każdej komórce drzwi `DoorBrush(typ).draw` na buforze podglądu; jeśli `canDraw` jest fałszem (pędzel ściany nie ma drzwi tego rodzaju przy tym ułożeniu), podgląd odmawia z błędem zamiast po cichu zostawić ścianę. Elementy drzwi pędzla ściany należą już do dozwolonych (`getRelatedItems` obejmuje drzwi).
- Okno generatora: pole wyboru „Doors” (aktywne przy włączonych ścianach) i lista rodzaju drzwi.

**Ocena.** (1) Testy: plan z drzwiami ma odcisk inny niż bez, plan bez drzwi bajt w bajt jak dotąd; odmowy (drzwi bez ściany, nieznany rodzaj, profil bez 8 pędzli). (2) Sondy G1 `--commit` i `--surface` PASS po przebudowie `rme`; do sondy dojdzie przypadek ze ścianami i drzwiami (podgląd, zatwierdzenie, Undo, Redo, liczba drzwi = liczba budynków w obrębie maski). (3) GUI: ocena użytkownika (wygląd, kierunek drzwi względem szlaku, Undo/Redo); jego wybór wizualny ma pierwszeństwo.

**Zastrzeżenia z góry.** Czy wybrany pędzel ściany ma drzwi danego rodzaju dla wszystkich ułożeń, zależy od klienta i danych brushy; nieudany `canDraw` kończy podgląd błędem. Nie ma automatycznego przypisywania ID drzwi domu (poza zakresem). Rdzeń nie wie, jakiego rodzaju są drzwi.

### Iteracja 12f — strona drzwi budynku bez szlaku (zapisana 2026-11-09 przed zmianą; powód: ocena wzrokowa użytkownika w GUI)

**Obserwacja użytkownika (GUI, zrzut):** drzwi lewego górnego budynku są po lewej, a szlak biegnie z prawej i od dołu. Przyczyna w modelu 12b: budynek, przy którym w chwili przetwarzania nie ma jeszcze kafla Path w zasięgu 60, dostaje **losową** stronę drzwi, a późniejsze budynki łączą się potem z jego schodkiem, więc szlak okrąża budynek.

**Zmiana (jedna reguła; nie zmienia rozmieszczenia budynków, więc G, Kn, Ks, Ki, Kb bez zmian).** Gdy nie ma celu w postaci kafla Path w zasięgu 60, strona drzwi jest zwrócona ku **środkowi najbliższego innego budynku** (każdego, także późniejszego w kolejności umieszczania) w odległości Czebyszewa ≤ 60; losowa strona zostaje tylko wtedy, gdy nie ma żadnego budynku w zasięgu. Trasy: bez zmian (budynek bez Path w zasięgu dostaje tylko schodek, następne łączą się z nim).

**Ocena.** Kryteria 12c bez zmian; oczekiwanie: Dp i pozostałe cechy skupisk nie pogarszają się, a drzwi dwóch sąsiednich budynków są zwrócone ku sobie (test: dla pary budynków bez szlaku strona drzwi każdego jest stroną ku drugiemu). Decyduje oglądowa ocena użytkownika; rozbieżność z metryką odnotuję.

**Zastrzeżenia z góry.** Zwrot ku najbliższemu budynkowi nie gwarantuje zwrotu ku docelowemu szlakowi; zmiana po obserwacji z jednego zrzutu.

### Iteracja 12g — krótsze szlaki do drzwi (zapisana 2026-11-09 przed implementacją i przed pomiarem; decyzja użytkownika: „Skrócić szlaki”)

**Powód.** Przy włączonych budynkach długość szlaków (L na 65 536 kafli) wynosi ok. 300–340 wobec 70 / 78 w referencjach (M06 / M19), bo każdy budynek dostaje trasę do najbliższej sieci w zasięgu 60 od swojego środka.

**Zmiana.** Nowa stała `doorTrailReach` (odległość Czebyszewa od środka budynku do najbliższego kafla Path): trasa od schodka jest wyznaczana tylko wtedy, gdy cel leży w tym zasięgu; inaczej budynek dostaje sam schodek (jak dotąd przy braku celu). Strona drzwi nadal wybierana w zasięgu 60 (`doorTargetReach`, reguła 12f). Rozmieszczenie budynków bez zmian.

**Wybór wartości (reguła zapisana z góry).** Pomiar wariantów `doorTrailReach` ∈ {16, 24, 36} (seedy 1001–1032, bok 512, profile v10 m06 i m19, `buildingDensity` 30 / 40, **`pathLength` 0**, bo losowe długie szlaki 9d same dają L 53 / 89 i zależą od ustawienia użytkownika). Wybieram **największą** wartość, dla której mediana L ≤ 1,5 · ref w obu profilach (≤ 105 dla m06, ≤ 117 dla m19; ref 70 / 78). Jeśli żadna nie spełnia, wybieram 16 i zgłaszam kryterium jako niespełnione. Raportuję też **odsetek budynków bez dojścia** (schodek w składowej Path 8-spójnej, w której jest dokładnie jedne drzwi; przy `pathLength` 0 to znaczy: schodek bez trasy do sąsiada) oraz Bp, Dp, G, Kn, Ks, Ki, Kb (powinny być bez zmian: rozmieszczenie to samo).

**Kryteria dodatkowe.** `buildingDensity` 0 bez zmian (odcisk Seed 1729); 15 testów rdzenia i obie sondy G1 PASS po przebudowaniu `rme`; dwa przebiegi bajtowo identyczne; decyduje ocena wzrokowa użytkownika (jego wybór ma pierwszeństwo przed metryką).

**Zastrzeżenia z góry.** Krótszy zasięg zostawi więcej budynków bez trasy (to cena skrócenia, mierzona jako odsetek bez dojścia); L liczone tylko dla `pathLength` 0, z długimi szlakami 9d suma będzie większa o ich wkład; pasmo dla L pochodzi z dwóch map (D17); zakres {16, 24, 36} wybrany z góry, nie dopasowany.

### Iteracja 12h — szlaki do drzwi mają dokądś prowadzić (zapisana 2026-11-09 przed implementacją i przed pomiarem; powód: ocena wzrokowa użytkownika po 12g, trzy zrzuty)

**Obserwacje użytkownika (GUI, `pathLength` 0):** (1) pojedyncze schodki bez trasy wyglądają jak kikuty; (2) trasy między sąsiednimi budynkami kończą się tuż przed celem (np. zrzut 3: zaokrąglona plama pod budynkiem urywa się 2 kafle od kikuta drugich drzwi), więc nie wyglądają, jakby gdzieś prowadziły. Przyczyny: (1) schodek powstaje zawsze, także gdy nie ma z czym go połączyć (12g); (2) wygładzenie trasy średnią ruchomą skraca jej końce, a stemplowanie nie dochodzi do punktu startu i celu.

**Zmiany (obie dotyczą tylko budynków; `buildingDensity` 0 i długie szlaki 9d bez zmian bajt w bajt).**
1. `PathRouter::connect` dostaje opcję `reachEnds` (włączaną tylko dla tras do drzwi): oprócz wygładzonej linii stemplowane są odcinki od schodka do początku linii i od jej końca do celu.
2. Schodek powstaje tylko wtedy, gdy budynek ma cel: kafel Path w ≤ `doorTrailReach` (24) od środka (jak dotąd) **albo** jeszcze nieprzetworzony budynek w tym zasięgu (środek–środek, Czebyszew), któremu drzwi się przyłączą: wtedy schodek jest znacznikiem czekającym na partnera, a partner znajdzie go jako cel. Budynek bez żadnego celu nie dostaje schodka (drzwi wychodzą prosto na trawę/grunt).

**Ocena (seedy 1001–1032, bok 512, v10, `pathLength` 0, `buildingDensity` 30 / 40).** (a) L (mediana) ≤ 1,5·ref (≤ 105 m06, ≤ 117 m19) przy `doorTrailReach` 24; jeśli nie, wartość 16 (jedyna alternatywa, zapisana z góry); (b) **kikuty**: odsetek drzwi, których schodek jest jedynym drzwiami w swojej składowej Path (8-spójnej), ≤ 100 ‰ (cel: 12g miało ok. 650–670 ‰ „izolowanych”, z czego większość to właśnie kikuty); (c) raportuję odsetek drzwi bez schodka (budynki bez celu) oraz G, Kn, Ks, Ki, Kb, Dp (powinny zostać bez zmian); (d) testy: 15 testów rdzenia + sondy G1 PASS po przebudowaniu `rme`, dwa przebiegi bajtowo identyczne, wszystkie drzwi nadal na obrysie, schodek (jeśli jest) to Path przed drzwiami; (e) decyduje ocena wzrokowa użytkownika.

**Zastrzeżenia z góry.** Pary budynków mogą się łączyć trasą wąską lub łamaną (A* ze stałymi kosztami); budynek, którego partner nie uzyska trasy (limit długości, brak drogi), zostaje z kikutem (to właśnie mierzy (b)); zmiana po obserwacji trzech zrzutów jednego użytkownika; zasięg 24 w dalszym ciągu nie badany pod kątem czułości.

### Iteracja 13 — walidator osiągalności powierzchni (zapisana 2026-11-09 przed implementacją i przed pomiarem; decyzja użytkownika: „Walidator osiągalności najpierw”)

**Zakres.** Rdzeń dostaje czystą funkcję `reachabilityOf(SurfaceGrid, walls)` (`source/procedural/core/reachability.{h,cpp}`), która tylko **mierzy** (bez naprawy i bez odrzucania seedów; to osobna iteracja po pomiarze). Nic nie zmienia wyniku generatora ani odcisków.

**Model przechodności (założenie, nie zmierzone na kliencie).** Przechodnie: Grass, Dirt, Rock, Shore, Path oraz wnętrze Floor. Nieprzechodnie: Lake, River, Preserve (komórka nie jest generowana, więc nieznana) oraz, gdy `walls` włączone, komórka obrysu Floor (Floor z 4-sąsiadem innego rodzaju lub poza obszarem — ta sama definicja co w resolverze) **bez drzwi**; komórka drzwi jest przechodnia. Bez ścian obrys jest przechodni. Łączność 4-sąsiedzka (jak `topology`).

**Wyjście.** Liczba komórek przechodnich, składowych, największa składowa, udział największej w ‰, komórki poza największą, „kieszenie” (składowe ≥ 16 komórek poza największą), budynki (składowe 4-spójne Floor), budynki zamknięte (drzwi poza największą składową albo brak drzwi przy `walls`), budynki osiągalne.

**Dlaczego bez pasma z referencji.** Referencje (piętro 7) mają największą składową ok. 10 % pól przechodnich (np. 74 945 z 718 346), bo mapy to wyspy i obszary rozdzielone nieznanym; wycinek 512×512 generatora nie jest z tym porównywalny. Wartości referencji zostają **N/A** jako pasmo; kryteria są bezwzględne.

**Kryteria.**
1. Testy ręcznie policzone: jezioro dzielące ląd na dwie składowe (liczby z rysunku), budynek z drzwiami (osiągalny), bez drzwi przy `walls` (zamknięty), te same dane bez `walls` (osiągalny), komórki Preserve nieprzechodnie, pusta siatka, `stop_token`.
2. Test krzyżowy: na ≥ 50 losowych siatkach 16×16 wynik równa się niezależnej implementacji w teście (inny algorytm: union-find) w liczbie składowych, największej i budynkach zamkniętych.
3. Deterministyczność: dwa przebiegi identyczne; pamięć robocza ≤ 8 B/komórkę (oszacowanie, nie pomiar).
4. Pomiar (seedy 1001–1032, bok 512, profile v10 m06 i m19, `buildingDensity` 30 / 40, `pathLength` domyślna z profilu i 0, ściany i drzwi włączone; mediana [min..max]): udział największej składowej (‰), komórki poza największą na 65 536, kieszenie, budynki zamknięte / budynki. **Kryterium przechodzenia tylko dla budynków:** zamknięte = 0 w ≥ 90 % seedów (medianowo 0). Pozostałe liczby to stan bazowy dla mostów i przełęczy, bez pasma (rzeki dzielą ląd legalnie).
5. Wszystkie testy rdzenia i obie sondy G1 PASS; `buildingDensity` 0 bez zmian bajt w bajt (odcisk Seed 1729 `17159159863505677800`).

**Zastrzeżenia z góry.** Nie ma jeszcze POI ani „ustalonego startu” z G5 (start/POI w generatorze nie istnieją): ocena „POI osiągalne” = N/A, jedyne wymagane punkty to wejścia budynków. Przechodność Rock i Shore przyjęta, nie sprawdzona na kliencie (D22 do zapisu). Piętra i przejścia Z poza zakresem (generator działa na jednym piętrze).

### Iteracja 14 — brody przez wodę (zapisana 2026-11-09 przed implementacją i przed pomiarem; decyzja użytkownika: „Mosty/brody”)

**Zakres.** Wybrałem **brody**, nie mosty: bród to przejście z kafli Path ułożone na rzece lub jeziorze, więc używa siódmego pędzla (szlaku) i nie wymaga nowego pędzla ani identyfikatorów przedmiotów. Mosty z osobnym przedmiotem (drewno, balustrady) wymagają pędzla mostu w profilu i zostają poza zakresem (do TECH_DEBT).

**Zmiana (tylko przy nowym polu `SurfaceTuning::fordLength`, dopisanym na końcu struktury; 0 = wyłączone = wynik bajt w bajt jak dotąd).** Po rzekach, szlakach i budynkach: dopóki istnieje składowa przechodnia ≥ 16 komórek (`reachabilityPocketCells`) poza największą, która ma w zasięgu ≤ `fordLength` komórek wody (River, Lake; ruch 4-sąsiedzki, tylko przez wodę) inną składową przechodnią, najmniejsza taka składowa (kolejność: najmniejszy numer etykiety w porządku wierszowym) dostaje bród: najkrótsza trasa przez wodę (BFS, remisy wg stałej kolejności sąsiadów) jest stemplowana jako Path razem z sąsiadami wodnymi trasy (ok. 3 szerokości). Po każdym brodzie składowe liczone od nowa. Model przechodności jak w iteracji 13 (ściany włączone). Brody powstają przed zastosowaniem maski i ochrony (komórki Preserve zostają Preserve).

**Kryteria.**
1. Testy ręcznie policzone: dwie połowy lądu rozdzielone rzeką szerokości 3 — `fordLength` 3 łączy je (1 składowa, trasa 3 komórek wody), `fordLength` 2 nie łączy; brak wody w zasięgu = bez zmian; dwa przebiegi identyczne; `fordLength` 0 = odcisk jak przed zmianą (Seed 1729: 17159159863505677800).
2. Pomiar (seedy 1001–1032, bok 512, profile v10 m06 i m19, ustawienia jak w iteracji 13, `fordLength` ∈ {8, 16, 32}): seedy z budynkami zamkniętymi (`sealed` > 0), największa składowa (‰), komórki Path dodane brodami na 65 536 kafli. **Kryterium przechodzenia: ≥ 29 z 32 seedów bez zamkniętych budynków (≥ 90 %) w obu profilach.** Wybór wartości: najmniejsza z {8, 16, 32} spełniająca kryterium w obu profilach; jeśli żadna, wartość 32 i werdykt NIE spełnione.
3. Wszystkie testy rdzenia i obie sondy G1 PASS po przebudowie `rme`; przy `fordLength` 0 testy z odciskami bez zmian.
4. Adapter/GUI: pole „Ford length” w oknie generatora (0 = wyłączone), profil musi mieć siódmy pędzel szlaku (jak przy `pathLength`); podgląd, Undo i Redo z brodami (przypadek sondy `--surface`); ocena wyglądu przez użytkownika ma pierwszeństwo.

**Zastrzeżenia z góry.** Bród z Path wygląda jak szara ścieżka przez wodę, nie jak most; nie sprawdzano, czy kafel Path na wodzie jest akceptowalny dla gracza (przechodni w grze zależy od pędzla szlaku). Ford może przeciąć długą rzekę w wielu miejscach (jedna rzeka dzieli ląd w wielu składowych); liczba brodów rośnie z liczbą składowych. Maska i ochrona mogą zerwać bród (Preserve). Walidator mierzy tylko model przechodności (D22).

### Iteracja 14b — brody z piasku zamiast ze szlaku, limit zasięgu 256 (zapisana 2026-11-09 przed zmianą kodu; powód: uwaga użytkownika po 14, zrzut z GUI)

**Obserwacja użytkownika:** „Tym mostem nie da się przejść”; pytanie, dlaczego zasięg ma limit 64. **Diagnoza (pomiar w sondzie, bok 250, pierwszy seed z brodem):** 5 z 6 kafli brodu jest blokujących. Kafel ma grunt `cobbled pavement`, brzeg `sand` i blokujący element `shallow water` (id 4648, krawędź wody nałożona przez automatyczne obramowanie, bo pędzel szlaku ma niższy priorytet niż woda). Błąd mojej iteracji 14: kryteria sprawdzały łączność modelu, nie kafle po zatwierdzeniu.

**Zmiana.** Kafle brodu mają rodzaj **Shore** (piasek), nie Path: ten sam pędzel i te same reguły obramowania co plaże, które w RME leżą przy wodzie bez blokujących krawędzi na własnym kaflu. Przechodność w modelu walidatora bez zmian (Shore i Path są przechodnie), więc liczby z iteracji 14 (seedy zamknięte, największa składowa) powinny pozostać takie same; ford nie wymaga już siódmego pędzla (adapter i okno wracają do wymagania pędzla szlaku tylko przy `pathLength`). Limit `fordLength` rośnie z 64 do **256** (walidacja i pole okna); 64 było moim arbitralnym wyborem, bez uzasadnienia pomiarem.

**Kryteria.** (1) Sonda `--surface`: po Commit **żaden kafel brodu nie jest blokujący** i każdy ma grunt (nowa asercja; to kryterium zawiodło w 14). (2) Liczba seedów z zamkniętymi budynkami dla `fordLength` 8/16/32/48/64 identyczna jak w iteracji 14 (m06 9/9/5/2/1, m19 9/9/5/2/2 z 32 seedów); jeśli się różni, zapisać różnicę. (3) Zasięg 256: pomiar czasu `style_render` na 32 seedach m06 (obecnie 6–7 s przy 32 wobec 4 s bez brodów) i liczba seedów z zamkniętymi; bez pasma, tylko raport. (4) 16 testów rdzenia, obie sondy G1 PASS, odcisk Seed 1729 bez zmian; testy rdzenia brodów zmieniają się z Path na Shore (hand-count bez zmian poza nazwą rodzaju).

**Zastrzeżenia z góry.** Brod z piasku wygląda jak mielizna/piaszczysta ławica, nie most; most z osobnym przedmiotem nadal poza zakresem. Czy sam `sand` na wodzie jest przechodni na kliencie sprawdza tylko asercja na kaflach (właściwość blokowania z danych klienta), nie chodzenie w grze. Sąsiednie kafle wody mogą dostać krawędzie piasku (to normalne obramowanie).

### Iteracja 14c — rozszerzone końce brodu (zapisana 2026-11-09 przed zmianą kodu; powód: uwaga użytkownika po 14b, zrzut z GUI: „zbyt wąskie przy początku/końcu”)

**Obserwacja użytkownika:** brod z piasku (14b) jest w środku dość szeroki, ale na początku i na końcu zbyt wąski, żeby nim przejść. **Co wiem z pomiaru (nowa asercja w sondzie, bok 250, lakePermille 250, `fordLength` 256, 71 kafli brodu, 3 kieszenie):** żaden kafel brodu ani lądowy kafel obok niego nie ma flagi blokowania, a każda kieszeń połączona brodem w modelu jest połączona także na zatwierdzonej mapie (ruch 4-sąsiedzki). Czyli według flag klienta bród jest przechodni; skargę użytkownika odczytuję jako wąskie przewężenie przy styku z lądem (bród kończy się w rogu brzegu, więc styk to 1–2 kafle), a nie jako błąd flag. Jeśli użytkownik sprawdzał chodzenie w grze i wciąż się nie da, przyczyny nie znam.

**Zmiana (tylko przy `fordLength` > 0).** Dla pierwszej i ostatniej komórki trasy brodu wszystkie komórki wody (River, Lake) w odległości Czebyszewa ≤ **3** stają się Shore (stała `fordMouthRadius = 3`, w granicach obszaru). Środek brodu bez zmian (3 szerokości). Łączność składowych bez zmian (tylko więcej piasku przy końcach).

**Kryteria.**
1. Testy ręcznie policzone (rzeka 3 szerokości, 20×8, zasięg 3): krawędź stykowa brodu z lądem (komórki brodu 4-sąsiadujące z lądem) wynosi na każdym końcu **4** (rzędy 0..3 przy górnym brzegu obszaru) zamiast 2; liczba komórek Shore w całości brodu zmienia się z 6 na liczbę policzoną z rysunku (zapisana w teście); jezioro 4 szerokości analogicznie.
2. W środku obszaru (nie przy brzegu) styk wynosi 7 na każdym końcu (test na siatce 20×16 z rzeką 3 szerokości, trasa w środkowym rzędzie).
3. Sonda `--surface`: nadal 0 blokujących kafli brodu i obok niego, kieszenie połączone na zatwierdzonej mapie.
4. Liczby seedów z zamkniętymi budynkami dla `fordLength` 8/16/32/48/64/256 bez zmian względem 14b (m06 9/9/5/2/1/0, m19 9/9/5/2/2/0); komórki brodów rosną (raport, bez pasma); 16 testów rdzenia, obie sondy G1, odcisk Seed 1729 bez zmian (przy `fordLength` 0 nic się nie zmienia).
5. Ocena wyglądu przez użytkownika ma pierwszeństwo; nie potrafię z metryki ocenić, czy „da się przejść” w jego rozumieniu.

**Zastrzeżenia z góry.** Promień 3 to mój wybór (bez pomiaru referencji; w referencjach nie ma brodów z piasku); większe piaszczyste „łaty” na końcach mogą wyglądać nienaturalnie; sąsiednie kafle wody dostają krawędzie piasku.

### Iteracja 14d — naturalniejszy kształt brodu (zapisana 2026-11-09 przed zmianą kodu; decyzja użytkownika po 14c: „Wolę naturalny wygląd”)

**Powód.** Po 14c bród to prosta linia o równej szerokości i krawędziach jak od linijki; użytkownik nie wie, czy to naturalne, i woli naturalny wygląd. **Ocena „naturalny” należy do użytkownika; poniższe kryteria mierzą tylko, że kształt nie jest prostą równą linią i że niczego nie psuje.**

**Zmiana (tylko dla brodów z trasą ≥ 8 komórek wody; krótsze zostają jak w 14c, więc testy ręczne 14c bez zmian).** (1) Oś brodu to nie najkrótsza trasa BFS, lecz linia od początku do końca trasy z poprzecznym wychyleniem `offset = (szum − 0,5) · 2 · A · sin(π·p/L)`, `A = min(4, L/6)` komórek, szum z `valueNoise` (ziarno generatora, bezwzględne współrzędne punktu bazowego, krok 8), więc wychylenie jest 0 na obu końcach. (2) Szerokość zmienna: wokół każdej komórki osi woda w odległości Czebyszewa `d` zamienia się w piasek, `d = 2` gdy szum (osobny strumień) > 40 000 z 65 536, inaczej `d = 1` (szerokość 3 lub 5, zmienna gładko co ok. 8 komórek). (3) Końce jak w 14c (wlot promienia 3). (4) Zabezpieczenie: po zbudowaniu osi sprawdzane jest, czy początek łączy się z celem po komórkach przechodnich; jeśli nie, dodatkowo stemplowana jest prosta trasa BFS z 14c (gwarancja postępu i zakończenia pętli). `placeFords` dostaje opcjonalny argument z ziarnem i początkiem obszaru (domyślnie 0).

**Kryteria.**
1. Testy ręczne 14c (rzeka 3, jezioro 4, siatka 20×16, kieszeń 12) przechodzą bez zmian.
2. Test na szerokiej wodzie (siatka 64×24, ląd x 0..7 i x 56..63, woda 48 kolumn, `fordLength` 64): jedna składowa po brodzie; deterministyczność (dwa przebiegi i dwa różne ziarna dają różne kształty); w środkowej jednej trzeciej długości każda kolumna ma od 3 do 9 komórek piasku; środek ciężkości piasku w kolumnie zmienia się o ≥ 2 rzędy między kolumnami (kształt nie jest prostą linią); piasek tylko tam, gdzie była woda.
3. Sonda `--surface`: 0 blokujących kafli brodu i obok niego, kieszenie połączone na zatwierdzonej mapie.
4. Seedy z zamkniętymi budynkami identyczne jak w 14c (zasięg 8/16/32/48/64/256: m06 9/9/5/2/1/0, m19 9/9/5/2/2/0 z 32); komórki brodów i czas generowania podane (bez pasma); dwa przebiegi bajtowo identyczne; 16 testów rdzenia, obie sondy G1, odcisk Seed 1729 bez zmian (`fordLength` 0 nic nie zmienia).

**Zastrzeżenia z góry.** Stałe (A, 8, 40 000, szerokość 3/5) to moje wybory bez pomiaru referencji (w referencjach nie ma brodów); wychylona oś może przechodzić przez komórki nie będące wodą (nie są zmieniane); czy wygląda naturalnie, ocenia użytkownik; zabezpieczenie (4) może dodać prostą trasę, wtedy kształt będzie mniej naturalny.

### Iteracja 15 — szlaki przez skałę, „przełęcze” (część 1) (zapisana 2026-11-09 przed implementacją i przed pomiarem; decyzja użytkownika: obie opcje, najpierw szlak przez skałę)

**Zakres i odchylenie od zasady „najpierw pomiar w referencjach”.** Pomiar na zatwierdzonej mapie (G4_GATE_STATUS, „Pomiar przed przełęczami”) pokazał, że skała jest przechodnia, więc przełęcz nie jest tu potrzebą osiągalności, tylko kształtem szlaków. **Nie zmierzyłem w referencjach, ile szlaków przechodzi przez skałę** (wymaga nowego kodu sondy); to odchylenie od zasady, kryteria poniżej są więc bezwzględne, bez pasma z referencji (N/A). Opcja 2 użytkownika (skała blokująca i przełęcze jako jedyne przejścia) jest **osobną późniejszą iteracją** (wymaga pędzla blokującego w profilu i zmiany modelu osiągalności); tu jej nie ma.

**Zmiana (nowe pole `SurfaceTuning::rockPasses`, 0/1, dopisane na końcu; 0 = jak dotąd, bajt w bajt).** Przy 1 router szlaków (`PathRouter`, także trasy do drzwi) może wchodzić na komórki Rock za dodatkową cenę 40 na komórkę (krok bazowy 10–20), więc szlak przechodzi przez pasmo skały tylko tam, gdzie objazd jest dłuższy (najwęższe miejsce pasma jako zastępstwo siodła; wysokości nie są dostępne w routerze). Komórki Rock pod trasą i jej obramowaniem stają się Path.

**Miara.** `cut` = liczba komórek Path, które mają Rock w odległości ≤ 3 po obu stronach w jednej osi (lewo i prawo albo góra i dół), czyli szlak „przecinający” pasmo; liczone na 65 536 komórek, mediana [min..max], seedy 1001–1032, bok 512, profile m06/m19, `buildingDensity` 30 / 40.

**Kryteria.**
1. `rockPasses` 0: wszystkie testy rdzenia i odcisk Seed 1729 bez zmian; wynik `style_render` identyczny jak dotychczas.
2. `rockPasses` 1, `pathLength` z profilu (40 / 80): mediana `cut` co najmniej 2× większa niż przy 0 i `cut` > 0 w ≥ 75 % seedów (24 z 32); długość szlaków L (mediana) nie więcej niż 1,5× wartość przy 0 (bez regresji kształtu szlaków).
3. Dwa przebiegi bajtowo identyczne; 16 testów rdzenia i obie sondy G1 PASS; sonda `--surface` nadal 0 blokujących kafli brodu, kieszenie połączone.
4. Ocena wyglądu przez użytkownika ma pierwszeństwo.

**Zastrzeżenia z góry.** Cena 40 i zasięg 3 są moimi wyborami bez referencji; szlak przez skałę jest szerszy niż prawdziwa przełęcz (stempel 3–4 kafle) i może wyglądać jak wycięty korytarz; kafle Path na skale używają pędzla szlaku (jego krawędzie względem skały nie były sprawdzane pod kątem blokowania, tak jak przy brodzie; będzie to w sondzie, jeśli kryterium 3 to wykaże); osiągalność się nie zmienia (skała jest przechodnia).

### Iteracja 16 — cele szlaków po drugiej stronie skały (zapisana 2026-11-09 przed implementacją i przed pomiarem; decyzja użytkownika: „Cele szlaków przez skałę”)

**Powód.** W iteracji 15 szlaki mogły przechodzić przez skałę, ale prawie tego nie robiły (3 i 8 z 32 seedów; nawet przy darmowej skale 13 i 22), bo końce tras są losowymi punktami lądu. Wąskie gardło to wybór celów, nie koszt.

**Zmiana (tylko `rockPasses` = 1 i `pathLength` > 0; przy 0 wynik i strumień losowy bajt w bajt jak dotąd).** W `drawPaths` co drugie podejście (parzyste numery prób) szuka celu „przez skałę”: z losowej komórki lądu (Grass lub Dirt) w losowym z 8 kierunków, do 8 prób, maks. 48 kroków: po drodze nie może być wody, musi pojawić się ciąg 3–30 komórek Rock, a po nim komórka lądu; cel to ta komórka przesunięta o 4 kroki dalej, jeśli nadal jest lądem. Gdy się nie uda, podejście używa dotychczasowego losowego celu. Router (koszt skały 40, z 15) bez zmian.

**Kryteria (bez pasma z referencji, N/A jak w 15).** Seedy 1001–1032, bok 512, profile v10, `pathLength` 40 / 80, `buildingDensity` 30 / 40, `rockPasses` 1, miara `cut` jak w iteracji 15:
1. Mediana `cut` ≥ 2× większa niż przy `rockPasses` 0 **i** `cut` > 0 w ≥ 24 z 32 seedów, w obu profilach.
2. Długość szlaków L (mediana) ≤ 1,5× wartość przy `rockPasses` 0 (166 / 295 → ≤ 249 / ≤ 443).
3. `rockPasses` 0: 16 testów rdzenia i odcisk Seed 1729 bez zmian, wynik `style_render` identyczny; dwa przebiegi bajtowo identyczne; obie sondy G1 PASS; nowy test `surface`: `rockPasses` 1 jest deterministyczny i daje inny wynik niż 0.
4. Ocena wyglądu przez użytkownika ma pierwszeństwo.

**Zastrzeżenia z góry.** Stałe (3–30 komórek skały, 48 kroków, 4 kroki za skałą, co drugie podejście) to moje wybory; część seedów może nie mieć pasma skały w zasięgu, wtedy 75 % jest nieosiągalne i napiszę to; szlak przez skałę jest szeroki jak zwykły szlak (3–4 kafle); pędzel szlaku na skale nie był sprawdzany pod kątem blokowania.

### Iteracja 17 — szlaki dochodzą do celu w skale i przechodzą przez wodę (zapisana 2026-11-09 przed implementacją i przed pomiarem; powód: uwagi użytkownika do iteracji 16, pięć zrzutów z GUI)

**Uwagi użytkownika.** (1) Szlak przez skałę „nie dochodzi do żadnego miejsca, kończy się w skale”. (2) Wolałby szlak z `dirt` albo do wyboru. (3) Szlak, który dochodzi do rzeki, powinien przechodzić na drugi brzeg. (4) Poszarpane brzegi piasku i trawy przy jeziorze (osobna sprawa, poniżej).

**Diagnoza (1).** `PathRouter::connect` wygładza trasę średnią kroczącą (okno 6), co skraca oba końce o kilka komórek; w iteracji 12h to naprawiłem opcją `reachEnds` tylko dla tras do drzwi. Szlaki z `drawPaths` jej nie mają, a cel „po drugiej stronie skały” leży 4 komórki za skałą, więc trasa urywa się w skale.
**Odpowiedź (2).** Pole „Path brush” jest listą wszystkich pędzli gruntu; `dirt` można wybrać dziś bez zmian w kodzie. Nie zmieniam domyślnego (cobblestone), chyba że użytkownik zdecyduje inaczej.

**Zmiany.**
- 17a: `drawPaths` stempluje do rzeczywistego startu i celu (`reachEnds`) **tylko przy `rockPasses` = 1**; przy 0 bajt w bajt jak dotąd.
- 17b: przy `fordLength` > 0 router (szlaki i trasy do drzwi) może przechodzić przez komórki **River** (nie Lake) za dodatkowe 25 na komórkę; komórki wody pod trasą i obramowaniem stają się **Shore (piasek), nie Path** (Path na wodzie dostaje blokującą krawędź wody, patrz 14b) i są oznaczone maską, więc późniejszy szlak nie zamienia ich w Path. Przy `fordLength` 0 bajt w bajt jak dotąd. Szerokość przejścia nie jest ograniczona poza kosztem (rzeki są wąskie), `fordLength` nie jest jej limitem w tej iteracji.

**Miara (nowa, w `style_render`).** „Czubek” szlaku = komórka Path mająca ≤ 3 komórek Path wśród 8 sąsiadów (czapa końca trasy). `tipRock` = czubki z ≥ 3 komórkami Rock wśród 8 sąsiadów; `tipWater` = czubki z ≥ 3 komórkami River lub Lake wśród 8 sąsiadów; liczone na mapę. Seedy 1001–1032, bok 512, profile v10, `pathLength` 40 / 80, `buildingDensity` 30 / 40.

**Kryteria.**
1. `rockPasses` 1: liczba seedów z `tipRock` > 0 po zmianie ≤ 3 z 32 w obu profilach (stan sprzed zmiany zapisuję jako bazowy).
2. `fordLength` 48 (obok `pathLength`): liczba seedów z `tipWater` > 0 co najmniej o połowę mniejsza niż przy `fordLength` 0 (stan bazowy zapisuję); seedy z zamkniętymi budynkami dla `fordLength` 8/16/32/48/64/256 podaję (mogą się zmienić; bez pasma).
3. `rockPasses` 0 i `fordLength` 0: odcisk Seed 1729 i wynik `style_render` bez zmian, 16 testów rdzenia; sonda `--surface`: 0 blokujących kafli brodu i obok niego, kieszenie połączone; obie sondy G1 PASS; dwa przebiegi bajtowo identyczne.
4. Ocena wyglądu przez użytkownika ma pierwszeństwo.

**Zastrzeżenia z góry.** Czubek to heurystyka (może liczyć też narożniki stempla); przejście szlaku przez rzekę to piaszczysty bród 3–4 kafle (nie most); trasy do drzwi też mogą przechodzić przez rzeki; nie sprawdzano, czy szlak przez rzekę wygląda dobrze; punkt (4) (poszarpane brzegi piasek/trawa) **nie jest w tej iteracji** — wymaga osobnej diagnozy.
