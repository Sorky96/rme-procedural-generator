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
