# G4 — stan bramki „Uczenie stylu L1/L2”, 2026-11-08

**G4 NIE SPEŁNIONA w mierzonym zakresie.** Ocena G4-F uruchomiona raz, na kryteriach z [G4_METRICS.md](G4_METRICS.md) zapisanych i zacommitowanych przed uruchomieniem (ewaluator `f251ba7`). Dwa przebiegi dały bajtowo identyczny raport (8 s każdy, jedna maszyna). Żadnego kryterium ani progu nie zmieniano po zobaczeniu wyniku.

**Bramka z planu:** „mierzalna i powtarzalna poprawa wybranych metryk stylu nad ręcznymi presetami, bez pogorszenia krytycznych metryk grywalności.”

## Wynik (klient 10.98, powierzchnia z ≤ 7, bok 256, seedy 1001–1032, pary baseline/kandydat)
Różnica = kandydat − baseline, mniejsza = lepiej. S1/S2 w ppm (max 2 000 000 każda), S3 w tysięcznych ‰, P1 w ppm. „Lepszy” = mediana < 0 i kandydat wygrywa w ≥ 24 z 32 seedów.

| Fold (test) | Mapy | S1 udziały | S2 sąsiedztwa | S3 wegetacja | P1 mediana: baseline → kandydat |
| --- | ---: | --- | --- | --- | --- |
| caves (M14, M16) | 2 | gorszy (+440 140) | gorszy (+425 428) | lepszy (−4 833, 3 wyrazy) | 999 730 → 889 678 (89 %) |
| M01 | 1 | bez rozstrzygnięcia (0) | bez rozstrzygnięcia (0) | N/A | 999 730 → 999 730 |
| M05 | 1 | gorszy (+1 022 156) | gorszy (+1 011 538) | gorszy (+85 916) | 999 730 → 946 054 |
| M06 | 1 | lepszy (−274 079) | lepszy (−276 892) | lepszy (−2 071, 24/32) | 999 730 → 933 535 |
| M15 | 1 | N/A: brak profilu (walidacja M17 bez danych) | | | |
| M17 | 1 | N/A: brak celu testowego (0 kafli naturalnych) | | | |
| M19 | 1 | lepszy (−817 977) | lepszy (−788 629) | lepszy (−8 396) | 999 730 → 947 825 |
| swamp (M12, M13) | 2 | lepszy (−320 283) | lepszy (−325 744) | N/A | 999 730 → **689 523** |
| zima (M11, M18) | 2 | bez rozstrzygnięcia (0) | bez rozstrzygnięcia (0) | N/A | 999 730 → 999 730 |

M01 i zima: wybrany krok walidacji to baseline, więc kandydat jest tym samym generatorem (różnica dokładnie 0).

## Kryteria bramki (zapisane z góry) i wynik
| Kryterium | Wymaganie | Wynik |
| --- | --- | --- |
| S1 udziały | lepszych ≥ 2/3 ocenianych foldów (7) | 3 z 7: **nie** |
| S2 sąsiedztwa | lepszych ≥ 2/3 | 3 z 7: **nie** |
| Brak foldu gorszego w obu naraz | 0 | 2 (caves, M05): **nie** |
| S3 wegetacja | lepszych więcej niż gorszych (foldy z kontekstem) | 3 lepsze, 1 gorszy (4 foldy): tak |
| P1 spójność chodzenia | w każdym foldzie kandydat ≥ 95 % mediany baseline | 5 z 7 foldów poniżej 95 %: **nie** |

**Wynik: nie spełniona** (zawiodły S1, S2, brak foldu gorszego w obu, P1).

## Co z tego wynika
- Dopasowanie poprawia styl na 3 z 7 ocenianych foldów (M06, M19, swamp) i pogarsza na 2 (caves, M05), a na 2 nie zmienia nic. Poprawy są duże tam, gdzie cel testowy przypomina mieszankę treningową (M19: −818 tys. ppm z 2 mln możliwych).
- **Krytyczna metryka grywalności jest gorsza.** Dopasowane parametry (jezioro do 300 ‰, skała do 320 ‰, do 8 rzek) tną ląd: mediana największej spójnej składowej chodzenia spada z 99,97 % do 68,9 % (swamp), 88,9 % (caves), 93–95 % w pozostałych. Poprawa dystansu udziałów jest więc kupowana kosztem spójności lądu. To jest wada samej funkcji celu (patrz niżej), nie błąd narzędzia.
- Funkcja celu (dystans L1 udziałów i sąsiedztw) nie zawiera spójności lądu ani żadnego ograniczenia grywalności, więc nic nie powstrzymuje dopasowania przed skrajnymi siatkami.

## Zastrzeżenia do wyniku
- **Dane:** 7 ocenianych foldów to po jednej–dwóch mapach; trzy foldy z wegetacją 3 mapy. Przedziały to rozrzut po 32 seedach generatora, nie niepewność doboru map.
- **Przeciek:** rodzina testowa foldu k była walidacyjną foldu k−1 (G4_METRICS.md); wynik jest nieco mniej niezależny, niż sugeruje nazwa „test”.
- **Interpretacja zapisu metryk:** G4_METRICS.md mówi o „sumie po seedach” w opisie S1/S2 i o parach per seed w agregacji; ewaluator liczy dystanse per seed (konieczne do par). Nie licząc sumarycznego wariantu.
- **S3** liczono na tych foldach, gdzie cel testowy ma kontekst ≥ 200 kafli (24 z 3–24 wyrazów).
- **Nie zmierzono:** podobieństwa wizualnego, naturalności rzek i gór, klastrowania 16/32/64, ocen człowieka, podziemi, BlackTek, `border_error_rate`, memoryzacji wyniku generatora względem referencji (D10), 100 % poprawnych ID i zera zmian poza maską (te zależą od ścieżki zatwierdzania, nie od strojenia, ale nie sprawdzano ich w GUI).
- **Profil nie jest podpięty do GUI** (strojenie powierzchni i wegetacji nie ma pól w `GenerationConfig`/dialogu), więc nie ma jeszcze testu ręcznego w RME; dopóki nie ma w nim niczego do wyboru, byłby bez sensu.

## Co zostaje (propozycje, nie wykonano)
1. Dodać spójność lądu (P1) jako twarde ograniczenie lub składnik funkcji celu dopasowania i powtórzyć G4-F. Każda taka zmiana to nowa iteracja, nie poprawka tego wyniku: zapisać nowe kryteria przed uruchomieniem i oznaczyć wynik „po zmianie kryterium”.
2. Profile per styl (wybrzeże, trawa, góry) zamiast jednego zlepka, i więcej map z rozpoznanym gruntem.
3. Zawęzić siatki parametrów lub dodać regularyzację ku wartościom ręcznym.

## Iteracja 2 — dopasowanie z ograniczeniem spójności lądu (2026-11-08, „po zmianie modelu”)

**G4 nadal NIE spełniona.** Kryteria zapisane przed uruchomieniem ([G4_METRICS.md](G4_METRICS.md), sekcja „Iteracja 2”, commit `83607af`); metryki, seedy, foldy i reguła bramki bez zmian. Zmienił się tylko model: kandydat w dopasowaniu `SurfaceTuning` jest odrzucany, gdy mediana spójności lądu na seedach 1–6 jest niższa niż 97 % mediany ręcznych parametrów (`fc41d6b`). Dwa dopasowania (36 s) i dwie oceny bajtowo identyczne.

| Fold (test) | S1 udziały | S2 sąsiedztwa | S3 wegetacja | P1 mediana: baseline → kandydat |
| --- | --- | --- | --- | --- |
| caves | gorszy (+354 676) | gorszy (+347 019) | lepszy (−5 000) | 999 730 → 958 154 |
| M01, zima | bez zmian (kandydat = ręczne) | | N/A | bez zmian |
| M05 | gorszy (+836 959) | gorszy (+832 825) | gorszy (+85 167) | 999 730 → 958 154 |
| M06 | lepszy (−410 079) | lepszy (−342 307) | **gorszy (+2 523)** | 999 730 → 958 154 |
| M19 | lepszy (−815 337) | lepszy (−781 036) | lepszy (−8 312) | 999 730 → 966 000 |
| swamp | lepszy (−320 068) | lepszy (−312 398) | N/A | 999 730 → 958 154 |
| M15, M17 | N/A (jak w iteracji 1) | | | |

| Kryterium | Iteracja 1 | Iteracja 2 |
| --- | --- | --- |
| S1 lepszych ≥ 2/3 foldów (7) | 3 z 7: nie | 3 z 7: nie |
| S2 lepszych ≥ 2/3 | 3 z 7: nie | 3 z 7: nie |
| Fold gorszy w obu naraz = 0 | 2: nie | 2: nie |
| S3 więcej lepszych niż gorszych | 3 : 1: tak | 2 : 2: **nie** |
| P1 każdy fold ≥ 95 % baseline | 5 foldów poniżej: nie | **0 poniżej: tak** (958 154–999 730 przy progu 949 744) |
| **Bramka** | nie spełniona | **nie spełniona** (zawodzą S1, S2, brak foldu gorszego w obu, S3) |

**Wniosek:** ograniczenie naprawiło krytyczną grywalność (spójność lądu ≥ 95,8 % ręcznej we wszystkich foldach, wcześniej do 69 %), nie niszcząc poprawy w tych trzech foldach, gdzie cel testowy przypomina mieszankę treningową (M06, M19, swamp: nadal −0,3 do −0,8 mln ppm). Nie poprawiło dwóch foldów, w których styl się nie poprawia: caves (cel to tylko ziemia) i M05 (cel to prawie sama trawa), gdzie dopasowany do mieszanki generator jest dalej od celu niż ręczne parametry; w M06 wegetacja zmieniła się z lepszej na gorszą (+2 523 przy rozrzucie −3 286…+6 381, czyli blisko zera). Zwykły zlepek stylów nie opisuje pojedynczej rodziny, co potwierdza kierunek „profile per styl”.

**Zastrzeżenia iteracji 2:** foldy testowe użyte po raz drugi, po adaptacji modelu do wyniku iteracji 1; wartość 97 % wybrana z góry, ale cały pomysł ograniczenia wynika z oglądania wyniku iteracji 1. Pozostałe zastrzeżenia jak w iteracji 1 (jedna–dwie mapy na fold, przeciek walidacja/test, brak oceny wizualnej, podziemi, BlackTek, memoryzacji generatora, brak podpięcia do GUI).

## Iteracja 3 — profile per styl (2026-11-08, „po zmianie modelu”)

Kryteria zapisane przed uruchomieniem ([G4_METRICS.md](G4_METRICS.md), „Iteracja 3”, commit `975fba4`; narzędzie `269da1b`). Dwa przebiegi (44 s) bajtowo identyczne. **Kryterium iteracji 3 spełnione w mierzonym zakresie, ale to wynik in-sample i nie spełnia bramki G4 z planu** (interpretacja zapisana z góry).

**A — własny styl (in-sample), 8 stylów:** profil lepszy od ręcznych parametrów w S1 i S2 we **wszystkich 8** (32/32 seedów w każdym), żaden gorszy; wegetacja (S3) lepsza we wszystkich 4 stylach z porównywalnym kontekstem (caves, m05, m06, m19). Spójność lądu P1 ≥ 95 % we wszystkich 8 (95,2–100 %).

**B — mapa wyłączona z uczenia w obrębie stylu (jedyny przypadek, swamp, n = 2):** profil z M13 na M12 i z M12 na M13 jest lepszy od ręcznych w S1 i S2 (32/32 seedów; −320 252 / −307 163 oraz −493 180 / −487 248 ppm), P1 95,9 % i 95,2 % (tuż nad progiem 95 %). To jedyny wynik na mapie niewidzianej, ale M12 i M13 to ta sama deklarowana rodzina (nazwy plików), dwa przypadki nie wystarczają do wniosku o uogólnieniu.

**C — rozróżnialność:** rozpoznanych 6 z 8 stylów (własny profil najbliższy własnemu celowi). Nie rozpoznane: m01 i swamp (profile m06 i m19 są o włos bliżej: m01: 2 821 743 własny vs 2 811 131 profil m06). Profile m15, m19, swamp i winter są do siebie podobne, bo dopasowanie dochodzi do krańców siatek (jezioro 300 ‰, skała do 320 ‰, rzeki 0–8, brzeg 3–4).

| Kryterium iteracji 3 | Wymaganie | Wynik |
| --- | --- | --- |
| A | ≥ 2/3 stylów lepszych w S1 i S2, żaden gorszy w obu | 8 z 8, 0 gorszych: tak |
| C | ≥ 2/3 rozpoznanych | 6 z 8: tak |
| P1 | każdy profil ≥ 95 % baseline | 0 poniżej: tak |
| B | bez progu, zawód jeśli gorszy w S1 i S2 | 0 zawodów |

**Jak to czytać (zastrzeżenia, z których żadne nie jest drobne):**
- A i C to dopasowanie do danych, z których profil powstał. Ręczne parametry są dalekie od większości celów (np. cel „caves” to prawie sama ziemia: 3,0 mln ppm baseline vs 12 712 profilu), więc „lepszy” jest łatwe do osiągnięcia; to dowodzi, że **generator potrafi odtworzyć rozkład rodzajów danego stylu**, nie że styl jest wiernie naśladowany.
- Wynik mierzy rozkład udziałów, sąsiedztwa rodzajów, wegetację i spójność lądu. **Nie ocenia wyglądu** (kształty rzek i gór, klastry, naturalność), a rzeki wciąż prowadzą głównie po osiach (D6). Dopasowane wartości sięgają krańców siatek, więc profil jest granicą możliwości siatki, nie optimum.
- Pięć stylów ma jedną mapę z celem; m01 to sama woda, caves to sam grunt ziemi, więc profil takiego stylu opisuje jeden rodzaj gruntu.
- Nie ma jeszcze zapisanych plików profili stylów (narzędzie raportuje strojenia w JSON), ani podpięcia do GUI/`GenerationConfig`.
- Nadal nie mierzono: podziemi, BlackTek, memoryzacji generatora względem referencji (D10), `border_error_rate`, 100 % poprawnych ID i zera zmian poza maską w pełnym potoku.

**Stan bramki G4 z planu (mapy niewidziane):** nie wykazana. Jedyny dowód na niewidzianej mapie (B, swamp, n = 2) jest korzystny, ale zbyt wąski. Aby ją wykazać, potrzeba co najmniej kilku stylów z ≥ 2 niezależnymi mapami z celem.

## Iteracja 4 — kształt rzek (D9 część wodna, D6) (2026-11-08): kryterium D6 NIE spełnione

Kryteria zapisane przed pomiarem ([G4_METRICS.md](G4_METRICS.md), „Iteracja 4”, commit `59b860c`); implementacja `99f617c`. Referencje z rzekami (≥ 5 składowych): tylko M06 (29 rzek, T 1626 ‰, S 0 ‰) i M19 (8 rzek, T 1669 ‰, S 0 ‰); pozostałe 10 map z ekstrakcją wody nie kwalifikuje się (kwalifikują się 2, więc ocena jest możliwa, ale na dwóch mapach, w dwóch foldach krzyżowych). Generator bez meandrowania: T 1361 ‰, S 181 ‰ (31 z 32 seedów z rzeką).

| Wariant | Meander (dopasowany) | T mediana | S mediana | T: lepszych/gorszych seedów (werdykt) | S: lepszych/gorszych (werdykt) | P1 (kandydat/baseline) | rzeka-wyspa |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 4a (pierwszy model) | 8 (maks. siatki) | 1398 | 70 | 18/9 (undecided) | 15/6 (undecided) | 99,997 % | 0 |
| 4b (po zmianie modelu) | 8 (maks. siatki) | 1514 | 24 | 22/6 (undecided) | 16/5 (undecided) | 99,972 % | 0 |

Mediany różnic odległości od referencji (4b, fold M06→M19): T −58 ‰ (p10 −219, p90 +54), S −77 ‰ (p10 −281, p90 +79); fold M19→M06 analogicznie (−56 / −77). Wniosek mierzalny: rzeki są bardziej kręte (T +153 ‰ do referencji 1626–1669) i prawie bez długich odcinków prostych (S 181 → 24 ‰ przy referencji 0), ale kryterium (lepszy w T i S wg reguły: mediana < 0 i ≥ 75 % seedów) nie jest spełnione w żadnym z dwóch foldów: najlepszy udział seedów to 22/31 = 71 %, a nierozstrzygnięte przypadki wynikają głównie z remisów. Spełnione: P1 ≥ 95 % (kandydat ≈ baseline) i niezmiennik (0 porażek w 1200/1200 seedach: rzeka zawsze dochodzi do jeziora/krawędzi).

**Zastrzeżenia (istotne).**
- 4b to zmiana modelu po zobaczeniu wyniku 4a (zmiana fali, ciągłość, stemplowanie interpolowane, czyszczenie rzek-wysp); kryterium i seedy te same, ale to adaptacyjne ponowne użycie tych samych dwóch map. 4a jest wynikiem pierwotnym; 4b nie zastępuje go jako dowodu.
- Fold jest dwuwarstwowy: dopasowanie na jednej mapie z rzekami, test na drugiej; obie dały meander 8, czyli dopasowanie nasyca się na krańcu siatki, więc optimum nie zostało znalezione.
- Pomiar to szkielet kratowy (przybliżenie); progi (odcinek ≥ 12, składowa rzeczna) wybrane przed pomiarem bez wiedzy o czułości.
- Nie oceniano wyglądu (ślepe A/B), rozwidleń, rozmiarów jezior, podziemi ani BlackTek. Referencja S = 0 oznacza, że w referencjach nie ma 12-kafelkowych prostych szkieletu; część tej różnicy może wynikać z samego szkieletowania.
- Rzeki w profilach v6 mają meander 8 (profile v5 nie są już wspierane).
- **Korekta (2026-11-08, [G4_COMPARE.md](G4_COMPARE.md)):** metryka rzek iteracji 4 pomija składowe wody ≥ 20 000 kafli (morze), więc duże rzeki połączone z morzem nie weszły ani do referencji, ani do dopasowania meandru. Referencje (M06 W ≈ 1,6) opisują wąskie strumienie, a rzeki generatora są szersze (W ≈ 4,4–6,6). Wynik iteracji 4 dotyczy tylko takich strumieni.

## Iteracja 5a — kanały wodne, pomiar bazowy (2026-11-08)

Definicje i kryterium zapisane przed pomiarem ([G4_METRICS.md](G4_METRICS.md), „Iteracja 5”, commit `f0e04bc`). Referencje (profil v7, mapowanie v3): kwalifikują się M06 (218 składowych), M08 (72), M10 (72), M19 (82), M07 i M09 mają po 1 (nie). Zgodnie z zapisem porównuję m06→M06 i m19→M19; M08 i M10 mają identyczne wartości (ta sama rodzina), nie weszły do oceny. Generator: seedy 1001–1032, bok 256, kanał w 32/32 seedach w obu profilach; dwa przebiegi bajtowo identyczne.

| Cecha | m06: generator | M06 | pasmo | m19: generator | M19 | pasmo |
| --- | --- | --- | --- | --- | --- | --- |
| L (szkielet na 65 536 kafli) | 455 | 384 | 257–576: w paśmie | 832 | 302 | 202–453: **poza** |
| W (szerokość, tys. kafla) | 4 679 | 2 898 | 1 942–4 347: **poza** (za szeroko) | 5 705 | 4 337 | 2 906–6 506: w paśmie |
| B (rozgałęzienia / 100 kafli szkieletu, ×1000) | 26 987 | 41 134 | 27 560–61 701: **poza** (nieznacznie) | 32 744 | 38 913 | 26 072–58 370: w paśmie |
| C (łączące się z wodą otwartą lub brzegiem, ‰) | 916 | 614 | 411–921: w paśmie (przy granicy) | 750 | 865 | 580–1 298: w paśmie |
| W paśmie | **2 z 4: nie zaliczony** | | | **3 z 4: zaliczony** | | |

**Wniosek (opis stanu, nie bramka).** Ilość i rozgałęzienie kanałów w generatorze są w tym samym rzędzie wielkości co w referencjach; wyjątki to zbyt szerokie kanały w m06 (4,7 vs 2,9 kafla) i 2,7 razy za dużo długości kanałów w m19. Pomiar nie wskazuje braku dopływów jako głównej różnicy między generatorem a referencjami; wcześniejsze wrażenie z oglądu (rzeki bez sieci) brało się z wycinka wybranego pod kątem największej ilości lądu oraz ze wspólnej skali szumu, której ta metryka nie opisuje.

**Zastrzeżenia.** Dopasowanie in-sample (profil z tej samej mapy). Dwie referencje z rzekami, dwie ocenione. Kanał to przybliżenie (otwarcie 13×13, kwadrat sprzyja osiom; wchodzą też wąskie zatoki i ramiona jezior). Nie oceniano wyglądu, struktur w dużej skali ani rzek w M08/M10. 5b (zmiana generatora) nie została wykonana, bo 5a nie wykazała wyraźnej luki w dopływach.

## Iteracja 6 — struktury w dużej skali (2026-11-08): kryterium NIE spełnione w zapisanym układzie

Definicje i kryterium zapisane przed pomiarem ([G4_METRICS.md](G4_METRICS.md), „Iteracja 6”, commit `5aa85a0`); rdzeń `scale_stats` z testem (PASS, wartości policzone ręcznie). Referencje (nadmiar zgodności E_d nad losowym układem, ‰): M06 E4 471, E16 394, E64 233; M19 E4 578, E16 425, E64 197. Seedy 1001–1032, bok 256, dwa przebiegi bajtowo identyczne.

| Wariant | Profil | E4 | E16 | E64 | E16 i E64 w paśmie |
| --- | --- | --- | --- | --- | --- |
| 6a (profil v7: featureSize 24 / 12) | m06 | 454 | 147 | −12 | **0 z 2** |
| | m19 | 310 | 16 | −1 | **0 z 2** |
| 6b (featureSize dopasowany z siatki do 256 → 192 na seedach 1–6) | m06 | 571 | 399 (w paśmie) | 121 (pasmo 156–350: **poza**) | **1 z 2** |
| | m19 | 606 | 399 (w paśmie) | 102 (pasmo 132–296: **poza**) | **1 z 2** |

**Wniosek.** 6a pokazało lukę ilościowo: generator ma praktycznie zerową zgodność na odległości 16 i 64 ponad przypadek (referencje ok. 400 i 200–230). Zmiana samego `featureSize` na 192 zamyka lukę dla E4 i E16, ale E64 zostaje poniżej pasma w obu profilach, więc **kryterium 6b nie jest spełnione**.

**Dodatkowa obserwacja (nie była zapisana z góry, jawnie eksploracyjna: bok 512, 8 seedów, bez oceny).** Przy boku 512 featureSize 192 daje E4/E16/E64 = 614/496/261 (m06) i 638/477/247 (m19), czyli wszystkie mieszczą się w pasmach. Zatem niedobór E64 przy boku 256 wynika w dużej części z ucinania odległości 64 przez okno 256 (zastrzeżenie zapisane z góry), a E_d zależy od wielkości obszaru. Wartość 192 nie jest więc ostatecznym wynikiem: dopasowanie zależy od wybranego boku, a w GUI użytkownik wybiera dowolny obszar.

**Zastrzeżenia.** Dopasowanie in-sample na dwóch referencjach; metryka nie odróżnia jednego dużego lądu od wielu dużych płatów; nie sprawdzono P1 (spójność chodzenia) ani niezmiennika rzek dla featureSize 192 (warunki dodatkowe 6b), więc 6b jest niepełne także z tego powodu. Wygląd oceni użytkownik w GUI na profilach `10.98-v8` (tylko m06 i m19 mają featureSize 192; to ręczna zmiana pliku profilu, nie wynik `style_styles`).

## Iteracja 7 — plamy ziemi i szerokość plaży (2026-11-08): kryterium NIE spełnione

Definicje, zmiana P na P′ i zmiana modelu zapisane przed pomiarami ([G4_METRICS.md](G4_METRICS.md), „Iteracja 7”, commity `6aa735e`, `1073167`, `79e54ad`); rdzeń `PatchStats` w `scale_stats` z testem (PASS, wartości policzone ręcznie).

**7a (bok 512, seedy 1001–1032, profile v8).** Referencje M06 / M19: D 421 / 451, F 749 / 625, P′ 5 638 / 5 656. Generator m06: D 112, F 975, P′ 1 987; m19: D 203, F 951, P′ 1 979. W paśmie: m06 1 z 3 (F), m19 0 z 3. Pierwotna definicja P (grubość plaży względem wszystkich komórek Shore) dała dla referencji 33 795 i 22 366 (piasek daleko od wody), więc ją zastąpiono P′ przed oceną generatora.

**7b (dopasowanie na seedach 1–4, bok 512, zmiana reguły pierścieni plaży: próg szumu ≤ 80 %, wymagane jezioro ≤ 270 komórek, limit `shoreWidth` 8).** Spadek współrzędnych wskazał oba profile na granice siatek: `transitionWidth` 16 000 i `shoreWidth` 8. Wartości po dopasowaniu: m06 D 210, F 948, P′ 2 316; m19 D 206, F 939, P′ 2 315. **Pasma nadal niespełnione**: D (pasmo 282–632 i 302–676) jest poniżej, P′ (3 777–8 457 i 3 790–8 484) jest poniżej. Zmiana zapisanej reguły pierścieni podniosła P′ tylko z ok. 1 980 do ok. 2 315, bo pierścienie powyżej trzeciego powstają z małym prawdopodobieństwem (próg szumu 80 %). Do referencji (≈ 5 600, czyli plaża ok. 5–6 komórek) brakuje jeszcze więcej niż dwukrotnie.

**Wnioski i zastrzeżenia.**
- Metryki D i F pokazują, że granica ziemi w generatorze jest **gładsza** (D niższe), a nie bardziej poszarpana niż w referencjach (które mają wiele małych plam ziemi). To nie zgadza się z oceną wizualną „kropkowana ziemia”; D i F nie mierzą drobnych „kropek” (składowych ≤ 4 komórek). Dopasowanie względem D zwiększyłoby poszarpanie, więc nie jest oczywiste, że poprawia wygląd dla użytkownika.
- Beach: pełne dopasowanie wymaga zmiany modelu (nowy parametr lub inna reguła pierścieni, zmiana schematu profilu), niezapisana jeszcze.
- Dopasowanie in-sample na dwóch referencjach; testy rdzenia PASS (w tym zmieniony limit `shoreWidth` ≤ 8); profile v8 w `local-data` nie zostały zmienione.

### Iteracja 7c — plaża (2026-11-08): P′ w paśmie w obu profilach

Kryteria zapisane przed pomiarem ([G4_METRICS.md](G4_METRICS.md), „Iteracja 7c”). Reguła pierścieni plaży: próg szumu min(5 + 10·pierścień, 40) %, wymagane jezioro min(30·3^(pierścień−1), 90) komórek. `shoreWidth` wybrany z {0..8} na seedach 1–4 (bok 512): 8 w m06 i m19 (P′ rośnie z 1 000 przy 1 do 5 835 / 5 807 przy 8, referencje 5 638 / 5 656). Ocena na seedach 1001–1032, bok 512, profile `g4-profiles/10.98-v9`:

| Profil | P′ generatora | referencja | pasmo | wynik |
| --- | --- | --- | --- | --- |
| m06 | 5 912 | 5 638 | 3 777–8 457 | **w paśmie** |
| m19 | 5 871 | 5 656 | 3 790–8 484 | **w paśmie** |

Udział Shore rośnie z ok. 1 % do 3,4 % (cel zmiany); pozostałe udziały spadają o mniej niż 3 punkty procentowe (m06 Grass 41,2 → 39,9 %, m19 Grass 28,1 → 26,4 %), więc warunek dodatkowy spełniony. **Niezmiennik rzek nie był ponownie mierzony** (plaża jest nakładana przed rzekami, które nadpisują komórki, więc reguła plaży go nie dotyka; nie sprawdzono pomiarem), nie uruchamiano regresji G1 `--commit` po zmianie reguły. Test rdzenia PASS po zmianie przypiętego odcisku Seed 1729 100×100 z 6783381837833077004 na 9975948746684599016 (zmiana reguły zmienia wynik dla `shoreWidth` ≥ 1, zgodnie z zapisem; wcześniejsze odciski w dokumentach G2 są historyczne).

**Zastrzeżenia.** In-sample na dwóch referencjach; P′ to miara grubości (nie wyglądu); w referencjach plaża obejmuje także piasek wokół kałuż i rzek, a generator jej nie robi przy rzekach; ocena wzrokowa w GUI należy do użytkownika. Ziemia (D, F) nie została zmieniona w tym kroku (D pozostaje poniżej pasma), czeka na wybór kierunku.

### Korekta profilu m19 (2026-11-08, na prośbę użytkownika po ocenie wzrokowej)

Po oglądzie v9 użytkownik uznał, że trawa i ziemia w m19 tworzą „wąskie paski” (kropkowaną mozaikę), w m06 zwarte plamy. `transitionWidth` m19 obniżony z 16 000 (górna granica siatki dopasowania) do 6 000 jak w m06: profile `g4-profiles/10.98-v10` (ręczna zmiana pliku, nie wynik `style_styles`). Decyzja wzrokowa użytkownika ma pierwszeństwo przed metryką: D (granica trawa/ziemia) m19 spada względem 7a/7c, czyli dalej od referencji M19 (451), co jest świadomą zgodą. Inne profile z `transitionWidth` 16 000 (m05, winter) nie zostały zmienione.

## Iteracja 8 — masywy skalne (2026-11-08): kryterium NIE spełnione

Definicje, wynik 8a i zmiana modelu zapisane przed pomiarem ([G4_METRICS.md](G4_METRICS.md), „Iteracja 8”, commity `ee52877`, `76a859e`); rdzeń `PatchStats` rozszerzony o skałę (test PASS, wartości policzone ręcznie). Referencje M06 / M19: RB (granica skały) 333 / 121, RF (zwartość ≥ 500) 167 / 862, RS (pył < 20) 36 / 15.

| Wariant (v10, bok 512, seedy 1001–1032) | m06 RB / RF / RS | m19 RB / RF / RS | w paśmie |
| --- | --- | --- | --- |
| 8a (3 oktawy szumu wysokości) | 49 / 998 / 0 | 33 / 1000 / 0 | m06 0 z 3, m19 2 z 3 |
| 8b (więcej oktaw przy `featureSize` > 24, do 8; 6 dla 192) | 77 / 988 / 0 | 47 / 993 / 0 | m06 0 z 3, m19 2 z 3 |

**Wniosek.** Skała generatora jest gładka i zlana w duże bryły (granica 3–10 razy krótsza niż w referencjach; referencyjna skała M06 jest rozdrobniona, M19 zwarta, ale z nierówną krawędzią). Dodatkowe oktawy podniosły RB o ok. 40–55 %, ale nie wystarczyły do pasma; **kryterium nie spełnione**. Przyczyną luki było to, że podniesienie `featureSize` do 192 w iteracji 6 usunęło drobny szczegół z brzegów skały, jezior i plaż (szum miał 3 oktawy).

**Skutki uboczne 8b (kontrola, nie kryterium).** Udziały rodzajów w granicach ±5 punktów procentowych wobec v10 (m06 Grass 390 ‰, Dirt 173 ‰, Shore 45 ‰; m19 Grass 277 ‰, Dirt 127 ‰, Shore 45 ‰), P′ 5 549 / 5 530 (nadal w paśmie), E4/E16/E64 przy boku 512: m06 604 / 448 / 224 (ref 471 / 394 / 233), m19 647 / 496 / 236 (ref 578 / 425 / 197). Zmiana szumu dotyczy profili z `featureSize` > 24 (m06, m19, winter); dla ≤ 24 wynik bajt w bajt jak dotąd (przypięty odcisk Seed 1729 bez zmian, test PASS).

**Zastrzeżenia.** Dwie referencje; skała M06 to tylko ok. 5 % powierzchni; nie sprawdzono niezmiennika rzek ani regresji G1 po zmianie szumu (rzeki zależą od wysokości, więc wpływ jest możliwy); wygląd oceni użytkownik w GUI. Pozostała luka (granica skały) wymagałaby drobnoziarnistego zaburzenia progu skały, niezapisanego jeszcze.

**Ocena użytkownika (2026-11-08, GUI, m19 500×500 po iteracji 8b):** „wygląda ok” (brzegi, skała i plaża po dodaniu oktaw szumu); ocena subiektywna, nie zmienia wyniku metryk (kryterium iteracji 8 nadal niespełnione).

## Iteracja 9a — szlaki: czy da się je zmierzyć w referencjach (2026-11-08)

Definicja (lista kluczy gruntu i składowa szlaku) zapisana przed pomiarem ([G4_METRICS.md](G4_METRICS.md), „Iteracja 9”, commit `468b8ee`); funkcja `extractKeyShapes` z testem (PASS, wartości policzone ręcznie), wyjście sondy `paths-surface.txt`. Wszystkie 19 map referencyjnych (mapowanie v3), z ≤ 7; bez oceny, tylko opis.

| Mapa | składowych szlaku | L (szkielet na 65 536 kafli) | W (tys. kafla) | B (rozgałęzienia, ×1000 na 100 kafli) | kafli szlaku | udział w obecnych kaflach |
| --- | --- | --- | --- | --- | --- | --- |
| M06 | 22 | 70 | 3 642 | 30 200 | 12 905 | 0,46 % |
| M19 | 5 | 78 | 4 606 | 26 259 | 7 346 | 0,72 % |
| pozostałe 17 | 0 | — | — | — | ≤ 2 | ≈ 0 |

**Wniosek.** Szlaki (wg wąskiej listy kluczy: bruk, żwir, chodnik) da się zmierzyć w dwóch mapach (M06: 22 składowe, M19: dokładnie 5, czyli na granicy kwalifikacji); zajmują 0,5–0,7 % powierzchni i mają rząd ok. 70–80 kafli szkieletu na 65 536 kafli (np. w obszarze 512×512 to ok. 300 kafli, kilka odcinków po kilkadziesiąt kafli). Warunek zapisany z góry (≥ 2 kwalifikujące się mapy) jest spełniony, więc generowanie szlaków nie jest zablokowane danymi, ale podstawa jest bardzo wąska.

**Zastrzeżenia.** Rozpoznanie po nazwie brusha (heurystyka); szlak z innego gruntu (np. ubita ziemia, piasek) jest niewidoczny; wartość M19 oparta na 5 składowych; generator szlaków nie istnieje, więc stan bazowy generatora jest zerowy z definicji. Generowanie wymagałoby nowego rodzaju powierzchni przechodzącego przez resolver, okno generatora (pole brusha szlaku), profil (schemat) i odcisk podglądu.

## Iteracja 9b — opcjonalne szlaki (2026-11-08): metryka w paśmie, integracja z GUI niezweryfikowana

Model, kryteria i cele zapisane przed implementacją ([G4_METRICS.md](G4_METRICS.md), „Iteracja 9”, commity `468b8ee`, `4ea15a3`). Zaimplementowano: `SurfaceKind::Path`, `SurfaceTuning::pathLength` (0 = wyłączone, nie jest częścią profilu stylu), trasy A* po Grass/Dirt/Shore między losowymi punktami lądu (odległość 48–160, koszt z szumem, szerokość 3–4), resolver z siódmym pędzlem (profil z 7 pędzlami; z 6 odrzuca siatkę ze szlakami), pola „Path length” i „Path brush” w oknie generatora (domyślnie 0, pędzel `cobblestone`), odcisk podglądu z `pathLength`.

**Pomiar (seedy 1001–1032, bok 512, profile v10; `pathLength` dobrany na seedach 1–4 z {40, 80, 160, 320}: 40 dla m06, 80 dla m19; kanał szlaku jak w 9a: pole ≥ 40, szkielet ≥ 20, szerokość ≤ 7):**

| Profil | seedy ze szlakiem | L (ref.) | W (ref.) | B (ref.) | w paśmie |
| --- | --- | --- | --- | --- | --- |
| m06, `pathLength` 40 | 32/32 | 53,5 (70) | 3 679 (3 642) | 28 494 (30 200) | **3 z 3** |
| m19, `pathLength` 80 | 32/32 | 92,5 (78) | 3 746 (4 606) | 29 639 (26 259) | **3 z 3** |

Testy rdzenia PASS (`paths`: wyłączone dają wynik bajt w bajt jak dotąd — przypięty odcisk Seed 1729 bez zmian; szlak nigdy nie nadpisuje skały, jeziora ani rzeki; deterministyczne; `pathLength` 401 odrzucone).

**Czego NIE zweryfikowano.** Pełnej ścieżki GUI: `rme.exe` nie został zlinkowany, bo RME działa (blokada pliku), więc okno generatora z polami szlaku, resolver z siedmioma pędzlami, Preview/Validate/Place/Undo ze szlakami nie były uruchamiane; nie uruchamiano regresji G1 `--commit` ani sondy palety po zmianie resolvera; warunku dodatkowego „udziały rodzajów ±1 punkt procentowy” nie mierzono osobno (szlaki zajmują 0,5–1 % powierzchni); szlaki nie mają celów (osad, bram), nie łączą się z mostami, nie dostają roślinności; cele pochodzą z dwóch map (M19: 5 składowych), więc „w paśmie” oznacza zgodność z dwiema referencjami, nie realizm.
