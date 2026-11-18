# G6: kryteria zapisane przed implementacją i pomiarem

Dokument G6 (FAZA 6: wielu kandydatów, ranking, feedback, wersjonowanie). Format jak w [G5_METRICS.md](G5_METRICS.md): każda iteracja ma kryteria zapisane i zacommitowane przed implementacją; zmiana po pomiarze tylko jawnie, z datą.

## Decyzje użytkownika (2026-11-17)

1. Ranking **leksykograficzny**: najpierw warunki twarde, potem zgodność stylu, na końcu rozstrzygnięcie remisu.
2. Ocena na **istniejących mapach**: rodziny spoza M06 i M19 według foldów [G4_SPLIT.md](G4_SPLIT.md), czyli M01, M05, zima (M11 + M18) i rodzina M12–M17. Użytkownik może dostarczyć wycinki dużych światów jako dodatkowe mapy holdout; wycinek M06/M19 nie jest holdoutem (D17).
3. Feedback w GUI: Approve/Reject/Adjust oraz wybór z galerii kandydatów, zapis **lokalny** (JSON obok profilu, poza repo).
4. ML: decyzję podejmuje wykonawca. **Decyzja: odstąpienie z uzasadnieniem** (sekcja „ML” niżej).
5. Ślepy test na istniejących mapach: 20 par, ranking wygrywa w ≥ 65 % par.
6. N kandydatów: domyślnie 4, maksymalnie 16.

## ML — odstąpienie (2026-11-17)

Styl opisują ręczne profile m06/m19 (D20). Rodzin danych holdout jest 5, a map 19, z jednego klienta (10.98). Feedback użytkownika jeszcze nie istnieje. Model uczony na takich danych nie da się uczciwie odróżnić od dopasowania do M06/M19, a każdy fold miałby 1–7 map. Według PROJECT udokumentowane odstąpienie nie blokuje wydania. Eksperyment wraca pod rozwagę, gdy zbierze się co najmniej 200 ocen feedbacku z co najmniej 3 rodzin map. Wtedy model musi pobić ranking leksykograficzny z I51 według kryteriów ślepego testu z I55, zapisanych przed eksperymentem.

## Iteracja 51 — wynik jakości kandydata (zapisana 2026-11-17 przed implementacją i przed pomiarem)

**Zakres.** Czysta funkcja rdzenia oceniająca jeden wynik generatora (siatka powierzchni, plan poziomów, raport osiągalności) względem profilu stylu oraz narzędzie offline rankujące N seedów. Iteracja **nie zmienia generatora, okna ani zapisu na mapę**. Odcisk Seed 1729 100×100 = 17159159863505677800 zostaje bez zmian.

**Definicja wyniku (`CandidateScore`, porównanie leksykograficzne krotki):**
1. **Twarde (`hardFail`, 0 lub 1):** 1, gdy walidacja nie przechodzi, osiągalność wielopiętrowa nie jest pełna (`levelReachabilityOf`, D22) albo naprawa usunęła miejsca. Kandydat z 1 przegrywa z każdym kandydatem z 0. Kopie fragmentów dochodzą w I53.
2. **Pasma (`inBand`, więcej = lepiej):** liczba metryk kształtu z `g5_shape_table` (szlaki, budynki, obrys, przejścia, rzeki, skala; tylko metryki, które profil ma w pasmie [0,67·ref; 1,5·ref] lub w progu bezwzględnym z G4/G5) mieszczących się w pasmie profilu.
3. **Odległość (`distance`, mniej = lepiej):** suma po tych samych metrykach |log(wartość / ref)|, przy wartości 0 obcięta do log(1/64). Porównanie w stałym przecinku (×1000, zaokrąglenie), żeby wynik był deterministyczny.
4. **Remis:** niższy indeks kandydata.

Kandydat i ma seed `mix(seedUżytkownika, i)` z ustaloną funkcją mieszającą w rdzeniu. **Kandydat 0 to dokładnie seed użytkownika**, więc N = 1 daje bajt w bajt dzisiejszy wynik.

**Kryteria.**
1. **Test ręcznie policzony:** 3–4 sztuczne kandydaty z zadanymi metrykami dają kolejność zgodną z ręcznym wyliczeniem, w tym przypadki „hardFail przegrywa mimo lepszych pasm”, remis po `inBand` rozstrzygany przez `distance` oraz remis całkowity rozstrzygany indeksem.
2. **Determinizm:** dwa przebiegi narzędzia (profile m06 i m19, seedy 1001–1032 jako seedy użytkownika, N = 16, bok 512) dają bajtowo identyczny raport. Kandydat 0 = wynik dotychczasowy (sprawdzony odciskiem).
3. **Sprawdzian poza próbą (nieokrężny).** Wynik liczony jest wobec pasm profilu, czyli in-sample M06/M19. Oceną jest odległość tych samych metryk od map holdout, liczona na nich narzędziami G4 (M01, M05, M11, M18, M12–M17, osobno per mapa, wobec profilu bliższego stylem według [G4_COMPARE.md](G4_COMPARE.md)). Dla każdej pary (seed użytkownika, profil) zwycięzca rankingu spośród 16 ma odległość holdout **mniejszą niż mediana kandydatów** w **≥ 70 %** par, dla każdego z obu profili osobno. Raport podaje też wynik losowego wyboru jako punkt odniesienia (oczekiwane ~50 %).
4. **Bez regresji:** `g5_run_suite.py` → SUITE_PASS z sondami, bramka `g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1` → 32/32 m06 i m19, odcisk Seed 1729 bez zmian.

**Zastrzeżenia z góry.** Metryki kształtu mierzą tylko powierzchnię (jaskinia i piętra wchodzą wyłącznie przez warunek twardy). Przypisanie mapy holdout do profilu jest ręczne. Pasma pochodzą z dwóch map (D17), więc kryterium 3 sprawdza tylko, czy wynik nie jest gorszy od losowego wyboru na mapach niewidzianych, a nie czy jest dobrym ocenem stylu. To rozstrzyga ślepy test w I55.

## Plan dalszych iteracji (kryteria zostaną zapisane przed każdą z nich)

### I53 — wykrywanie zgodnych fragmentów (2026-11-18, przed implementacją)

Zakres: odcisk semantyczny powierzchni kandydata i porównanie z lokalnym
indeksem referencji użytych przez profile. Wykorzystujemy algorytm `similarity`
z G4: okna 16×16, co najmniej 50% obecnych komórek, pomijanie jednolitych
okien i próbkowanie po treści modulo 4. Porównanie ignoruje pozycję i piętro.
Referencje i generator używają identycznych nazw klas powierzchni i tej
samej konwencji obecności. Referencje przeliczamy z danych semantycznych i
zatwierdzonego mapowania G4, nigdy z istniejących hashy nazw gruntu. Nieznane
i Preserve są nieobecne. Raport wymienia mapowania i ograniczenia klas.

Indeks lokalny ma wersję formatu, klienta, parametry odcisku i manifest
źródeł. Do decyzji wchodzą wyłącznie M06/M19. M01/M05/M11/M18 ani mapy
M12–M17 nie mogą zasilać indeksu lub filtra. Okna obecne w obu mapach
usuwamy z obu referencji i kandydata przed porównaniem. Raport zawiera
liczby okien przed i po filtrze oraz wspólne unikatowe hashe na referencję.
Warunek twardy: dla dowolnej pojedynczej referencji ≥16 wspólnych unikatowych
hashy oraz ≥100‰ zawierania względem odcisku kandydata po filtrze. Progi
pochodzą z G4 i pozostają zamrożone. Mniej niż 16 okien po filtrze oznacza
„za mało danych”, bez deklaracji braku kopii. Indeks bez wystarczającej
referencji, z błędem lub niezgodnością nie daje wyniku oceny.

W trybie offline z podanym indeksem i w Surface GUI z aktywnym zgodnym
lokalnym indeksem kopiowanie spełniające oba progi daje hardFail przed
rankingiem A i blokuje Place także po wymuszonym wywołaniu. Bez indeksu
lub przy zbyt małej próbce użytkownik widzi, że podobieństwa nie oceniono.
Jeśli jawnie skonfigurowany indeks jest uszkodzony/niezgodny, seria zgłasza
błąd zamiast pomijać kontrolę. Kontrola nie modyfikuje mapy. W GUI korzysta
z istniejącego anulowania, aktualności wyniku i wspólnego budżetu I52.

Testy: ręczna arytmetyka obu progów; kopia przesunięta z inną kolejnością
kluczy; niezależny teren; okna ogólne, rzadkie, jednolite i za mała próbka;
niezgodne/uszkodzone dane i anulowanie. Sonda: wykryta kopia blokuje Place,
czysty dopuszczalny kandydat nadal ma jedno Place/Undo/Redo. Jeśli wejścia
lokalne są dostępne, raport dla m06/m19, seedy 1001–1032, N=1, bok 512,
ustawienia I51 z --g5-levels, dwa identyczne przebiegi. Zapisujemy wszystkie
trafienia bez korekty progów. Zgodność hasha klas nie dowodzi kopiowania
przez generator. Kontrola nie obejmuje przedmiotów ani geometrii podziemi.

Regresja: kanoniczny build, SUITE_PASS z sondami, G5 --i47 32/32 obu
profili, fingerprint 1729 100×100 bez zmian. Raport I53 i kroki GUI w
G6_GATE_STATUS. Prywatne indeksy i mapy pozostają poza Git.

### I52 — kandydaci w GUI (2026-11-18, przed implementacją)

Zakres: istniejące okno generatora Surface, N domyślnie 4, zakres 1–16,
galeria/lista z miniaturą, indeksem, rzeczywistym seedem i stanem walidacji.
Ranking A pozostaje niezaliczonym baseline: UI nie obiecuje jakości i nie
używa B. Dla rozpoznanego profilu m06/m19 stosuje jawne pasma g6-bands-v1;
bez przypisanej polityki pokazuje brak rankingu, nie zgaduje profilu.

1. Kandydat i używa candidateSeed(userSeed,i); kandydat 0 zachowuje seed.
   N=1 zachowuje dotychczasowy wynik i fingerprint 1729 100×100.
   Istniejące retry walidacji są jawnie raportowane wraz z użytym seedem;
   nie wolno przedstawiać seeda po retry jako pierwotnego. Dwa uruchomienia
   z tymi samymi wejściami dają te same wyniki i wybór rekomendowany.
2. Dla dostępnego rankingu domyślnie zaznaczony najlepszy dopuszczalny A;
   użytkownik może wybrać innego dopuszczalnego kandydata. Wybrany i
   rekomendowany są rozróżnione tekstem, nie wyłącznie kolorem. Bez rankingu
   domyślnie pierwszy dopuszczalny. Nie wolno zapisać kandydata hardFail,
   niezwalidowanego, niekompletnego lub ze starego zestawu parametrów.
3. Ocena warunku twardego uwzględnia rzeczywistą walidację adaptera i
   osiągalność, nie tylko przybliżenie offline. Wybór zmienia podgląd
   istniejącym mechanizmem; generacja i przełączanie nie mutują mapy.
   Place zapisuje dokładnie wybrany wynik, jeden Action/Undo; Redo odtwarza
   go bez ponownego losowania. Zmiana mapy/parametrów unieważnia kandydatów.
4. Generacja poza wątkiem GUI, postęp i anulowanie. Zamykanie okna,
   anulowanie, zmiana mapy i spóźniony wynik nie mogą dotknąć nieaktualnego
   okna/mapy. Pamięć ma jawny łączny budżet całej serii, nie N niezależnych
   pełnych budżetów. Brak miejsca powoduje czytelny błąd bez częściowego Place;
   nie wolno po cichu obniżać N. Nie przechowujemy N pełnych kopii mapy.
5. Istniejący układ i Theme; wybór dostępny klawiaturą, widoczny fokus,
   czytelny stan błędu i pustego wyniku. Bez feedbacku approve/reject/adjust
   i historii profili (I54), bez zmiany generatora, mountain i formatu map.

Testy: N=1/4/16 i granice, oba profile, fallback bez pasm, kolejność A,
wybór nierekomendowanego, hardFail blokuje Place, anulowanie/stare wyniki,
budżet serii i brak mutacji przed Place. Sonda rzeczywistego klienta:
wybór kandydata -> Place -> jedno Undo -> Redo i zgodność z podglądem.
Build; g5_run_suite z sondami SUITE_PASS; G5 --i47 rockBlocks m06=0,m19=1
32/32; odcisk bez zmian. Osobno raport ręcznego GUI (jeśli niewykonany,
nie ogłaszamy pełnego zaliczenia I52). Bez twierdzeń o poprawie jakości.

### I51b — eksperyment kolejności rankingu (2026-11-18, przed implementacją i pomiarem)

**Decyzja użytkownika:** przygotować osobno zarejestrowany eksperyment poprawy
rankingu. I51 pozostaje FAIL (m06 18/32, m19 18/32). Poniższy eksperyment
nie zastępuje historycznego wyniku ani kryteriów I51.

**Hipoteza, nie ustalenie:** dyskretna liczba trafionych pasm może przesłaniać
mniejsze, ciągłe błędy pozostałych metryk. Sprawdzamy jedną zmianę porządku,
bez uczenia wag, selekcji metryk ani nowych wariantów po zobaczeniu wyników.
Nie obliczono dotąd wyniku proponowanego wariantu na raportach I51.

**A (kontrola):** (hardFail rosnąco, inBand malejąco, distance rosnąco, indeks).
**B (jedyny eksperyment):** (hardFail rosnąco, distance rosnąco, inBand malejąco, indeks).
Oba są rankingiem leksykograficznym z warunkiem twardym na pierwszym miejscu.
distance pozostaje całkowitą sumą z polityki 396e057. Pasma g6-bands-v1,
26 metryk, N/A, jednostki, cap i wszystkie parametry generatora bez zmian.
Nie zmieniamy produkcyjnego domyślnego porządku A podczas eksperymentu.

**Dane i wykonanie:** nowa macierz seedów użytkownika 2001–2032, N=16,
bok 512; profile m06/m19, pathLength/buildingDensity 40/30 i 80/40,
rockBlocks 0/1, --g5-levels. Ten sam zestaw kandydatów służy A i B,
bez ponawiania ani usuwania nieudanych seedów. Dwa pełne przebiegi muszą
dać bajtowo identyczne raporty. Kandydat 0 zachowuje seed użytkownika.
Historyczne seedy 1001–1032 można ocenić dopiero po zamrożeniu kodu,
wyłącznie jako wynik dodatkowy, bez udziału w decyzji o przyjęciu B.

**Ocena główna:** te same mapy M01,M05,M11,M18 i średnia po trzech rodzinach,
jak w I51. M12–M17 wyłącznie dodatkowo. Sukces = hardFail zwycięzcy 0
i odległość holdout ściśle poniżej mediany wszystkich 16 kandydatów.
Próg dla B: co najmniej 23/32 osobno dla m06 i m19. Ponadto B musi mieć
ściśle więcej sukcesów niż A na nowej macierzy w każdym profilu.
Warunki są łączne; niespełnienie któregokolwiek oznacza I51B_FAIL,
bez przyjęcia B i bez automatycznej zmiany kryteriów. Nie deklarujemy
istotności statystycznej na podstawie samego przekroczenia progu.

Raport obejmuje A i B per seed, mapa i rodzina; pary poprawa/pogorszenie/
bez zmiany, liczbę zmian zwycięzcy, hardFail, inBand i distance zwycięzców,
baseline losowy liczony dokładnie jak w I51, pokrycie i przyczyny N/A.
Nie używamy holdoutu do wyboru zwycięzcy; służy wyłącznie ewaluacji.
Błąd danych unieważnia przebieg, nie jest porażką jakości ani N/A.

**Ograniczenie dowodu:** mapy oceniano już w I51; nowe seedy nie czynią
ich ponownie niewidzianymi. I51b jest eksperymentem rozwojowym po ujawnieniu
holdoutu. Nawet PASS nie zamyka G6 ani nie zastępuje oceny na nowych mapach
lub ślepego testu I55. Nie dobieramy nowych map na podstawie wyników B.

**Testy i regresja:** ręcznie policzony przypadek, w którym A i B wybierają
różnych kandydatów; hardFail i pełne remisy; niezależne odtworzenie obu
porządków w Pythonie; agregacja par i odmowa niekompletnej macierzy.
Build, g5_run_suite z obiema sondami → SUITE_PASS; G5 --i47 z rockBlocks
m06=0,m19=1 → 32/32 obu profili; fingerprint 1729 100×100 bez zmian.
Bez zmian GUI, Place/Undo, generatora i formatu map. Implementacja przez
agenta Codex medium w worktree, po tym commicie; przegląd, ff-merge,
niezależna regresja, raport G6_GATE_STATUS i cleanup build-ninja worktree.

### I51 — uzupełnienie wiążące przed implementacją i pomiarem (2026-11-17)

Poniższe reguły zastępują wcześniejsze niedookreślone wybory metryk,
odległości i przypisania holdoutu. Zapis po częściowej implementacji b828824,
ale przed pomiarem rankingu i holdoutu; wcześniejsza macierz mierzyła tylko
powtarzalność, bez pasm. Nie zmieniamy generatora ani progu zaliczenia.

Stały zestaw 26 metryk dla obu profili. B(r)=[0,67r;1,50r],
A50(r)=[max(0,r−50);r+50], U20(r)=[0;r+20]; granice inkluzywne,
przechowywane dokładnie w setnych jednostki.

| Metryki w kolejności | Referencje m06 | Referencje m19 | Pasmo |
| --- | --- | --- | --- |
| channels.L,W,B,C | 384;2898;41134;614 | 302;4337;38913;865 | B |
| scale.E16,E64 | 394;233 | 425;197 | B |
| patch.D,F,P | 421;749;5638 | 451;625;5656 | B |
| patch.RB,RF,RS | 333;167;36 | 121;862;15 | B, poza m19 RS: U20 |
| trans.R0,R1,R2,R3 | 33;938;24;4 | 99;502;251;147 | A50 gdy r<50, inaczej B |
| trans.D0,D1,D2,D3 | 951;32;12;4 | 728;155;27;88 | A50 gdy r<50, inaczej B |
| paths.L,W | 70;3642 | 78;4606 | B |
| build.per64k,long,short | 31;9;7 | 37;9;7 | B |
| outline.median | 30 | 24 | B |

Źródła: G4_GATE_STATUS, tabele kanałów (129–134), skali (143), ziemi/plaży
(162), szlaków (209–212), przejść (263), budynków (281–284);
G4_METRICS: skała (186), obrys (295), reguły pasm (107,123,142,148,178,214,243,285).
P oznacza poprawione P′. Przejścia R0..R3=RD/RG/RS/RW, D0..D3=DG/DR/DS/DW.
E4 jest kontrolą; paths.B wyłączono historycznie jako artefakt (I9d).
rockadj/rocknear nie mają pasm referencyjnych obu profili. rivers.T/S/W
nie otrzymały takich pasm, późniejszy pomiar kanałów obejmuje sieci szerzej.
Wszystkie osiem przejść pozostaje aktywne. Pozostałe metryki raportujemy
diagnostycznie, bez dodatkowych głosów w rankingu.

**N/A i odległość.** Każdy kandydat ma 26 pozycji; nie zmieniamy mianownika
ani zestawu zależnie od kandydata. Brak kolumny, błędna liczba lub awaria
pomiaru unieważnia przebieg. Definicyjne N/A nie jest błędem technicznym.
Brak kanałów/szlaków daje długość 0, brak budynków gęstość 0; szerokości,
boki i obrys bez obiektów pozostają N/A. Zachowujemy progi liczebności G4.
N/A przy znanej referencji: 0 punktów inBand i wkład C=ln(64).
Ujemne excessy są poprawnymi pomiarami. Pasma stosujemy do wartości surowych.
Dla metryk nieujemnych i ref>0: d=min(C,abs(ln(value/ref))), zero daje C.
Dla ref=0: d=min(C,ln(1+value/u)), u=1 natywna jednostka.
Dla signed excessów: d=min(C,abs(asinh(value/u)−asinh(ref/u))), u=1‰.
distance=round(1000·suma d), jedno zaokrąglenie po sumie w kolejności tabeli.
To jawne rozszerzenie wcześniejszej definicji, obejmujące ograniczenie kary
i transformację excessów; nie deklaracja zachowania obecnego kodu.
Porządek: hardFail rosnąco, inBand malejąco, distance rosnąco, indeks rosnąco.

**Holdout.** Każdy profil porównujemy ze wszystkimi mapami M01,M05,M11,M18,
bez wyboru bliższego stylu. Odległość per mapa = średnia 26 wkładów.
Referencyjne N/A wnosi stałe C dla wszystkich kandydatów; raport pokazuje
przyczyny i liczbę pozycji różnicujących. Mapa bez żadnej takiej pozycji
blokuje ocenę. Zima = średnia M11/M18; wynik główny = średnia trzech rodzin
M01, M05, zima (po 1/3). M12–M17: wyłącznie osobny raport rodziny M19.
Mediana 16 kandydatów to średnia pozycji 8 i 9. Sukces pary wymaga hardFail=0
i odległości zwycięzcy ściśle poniżej mediany. Remis przegrywa; nie usuwamy
hardFail, N/A ani seedów z mianownika. Próg: co najmniej 23/32 dla każdego
profilu. Baseline losowy liczymy dokładnie jako udział kandydatów spełniających
ten sam warunek w każdym seedzie, następnie średnią po seedach.
Raport zawiera wyniki per mapa, rodzina i wynik główny, również porażki.

Macierz: seedy 1001–1032, N=16, bok 512, dwa identyczne przebiegi;
m06 pathLength/buildingDensity=40/30, m19=80/40, rockBlocks=0/1,
geometria --g5-levels. Kandydat 0 zachowuje seed użytkownika.
Testy obejmują nowe reguły odległości, N/A vs błąd, granice pasm,
agregację rodzin, medianę parzystą, remisy i dokładny baseline losowy.
Mapy holdout analizowano wcześniej w G4: jest to ocena poza wybranymi
rodzinami, nie całkowicie nietknięty zbiór badawczy. Zamrażamy listę
zaakceptowaną przez użytkownika, nie dobieramy jej ponownie po wynikach.

### Korekta holdoutu — decyzja użytkownika 2026-11-17

Przed pomiarem rankingu i holdoutu użytkownik zaakceptował korektę podziału:
niezależny holdout G6 obejmuje M01, M05 oraz rodzinę zimową M11+M18.
M12–M17 raportujemy osobno jako dane pokrewne M19 według G4_SPLIT.md;
nie wchodzą do zaliczenia progu holdout z kryterium 3 I51. Ta korekta
zastępuje wcześniejsze określenie M12–M17 jako rodziny spoza M19.
Nie zmienia progu ≥70% ani wymogu osobnego wyniku dla obu profili.

- **I52:** N kandydatów w oknie generatora (domyślnie 4, maks. 16), wybór najlepszego, galeria; jedno Undo na Place.
- **I53:** test memoryzacji: odcisk wyniku wobec referencji (`similarity.*`) jako warunek twardy.
- **I54:** feedback Approve/Reject/Adjust, lokalna historia, wersjonowane profile z rollbackiem.
- **I55:** ocena G6: holdout wobec L2/L3, overfitting, ślepy test 20 par (≥ 65 %).
