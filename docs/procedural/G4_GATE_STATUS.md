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

### Iteracja 9c — wygładzenie szlaków (2026-11-08): L i W w paśmie, B poza pasmem (świadomie)

Zapis kryteriów powstał po pomiarze (zaniedbanie procesu opisane w [G4_METRICS.md](G4_METRICS.md), „Iteracja 9c”; kryteria jak w 9b i przedstawione użytkownikowi przed zmianą): ruchy po przekątnej, koszt przy skale, wygładzenie średnią ruchomą. Pomiar (seedy 1001–1032, bok 512, v10): **m06** (`pathLength` 40): L 56,5 (ref. 70) w paśmie, W 4 546 (ref. 3 642) w paśmie, **B 3 697 (ref. 30 200) poza pasmem**; **m19** (80): L 89,5 (78) w paśmie, W 4 526 (4 606) w paśmie, **B 5 700 (26 259) poza pasmem**. Przed zmianą B wynosiło 28 494 / 29 639 (w paśmie). Udział komórek szlaku przy skale: mediana 0 ‰ / 13 ‰ (wcześniej 0 / 9 ‰), czyli nie zmieniło się (ten przypadek jest rzadki).

**Wniosek.** Spadek B nie oznacza mniej rozgałęzień szlaków, tylko mniej „rozgałęzień” szkieletu: metryka B liczy kafle szkieletu z ≥ 3 sąsiadami, a poszarpany, schodkowy obrys szlaku daje ich wiele (artefakt szkieletowania), podobnie jak w ręcznie malowanych szlakach referencji. Wygładzone szlaki są bliższe temu, czego chciał użytkownik (płynne łuki, oględziny wycinka 200×200 potwierdzają gładki łuk o szerokości 3–4), ale **kryterium B z 9b nie jest spełnione**; decyzję wzrokową użytkownika przyjęto nad metryką. Testy rdzenia PASS (szlak nigdy na rock/lake/river, deterministyczne, wyłączone = bajt w bajt jak dotąd). Zastrzeżenia jak w 9b; ocena wzrokowa w GUI po wygładzeniu należy do użytkownika.

### Iteracja 9d — odpychanie szlaków od skały (2026-11-08): kryterium spełnione

Kryterium zapisane przed zmianą ([G4_METRICS.md](G4_METRICS.md), „Iteracja 9d”, commit `f8ae580`): udział komórek szlaku z Rock w promieniu 3 (średnia i mediana po seedach) o co najmniej połowę niższy; L i W w paśmie; testy rdzenia PASS. Zmiana: koszt A* rośnie z bliskością skały (60/40/25/12 dla odległości 1/2/3/4). Wynik (seedy 1001–1032, bok 512, v10, `pathLength` 40 / 80):

| Profil | rocknear mediana przed → po | rocknear średnia przed → po | L (ref.) | W (ref.) |
| --- | --- | --- | --- | --- |
| m06 | 0 → 0 ‰ | 36,9 → 8,2 ‰ | 55,5 (70), w paśmie | 4 434 (3 642), w paśmie |
| m19 | 87,5 → 10 ‰ | 99,4 → 16,7 ‰ | 91,0 (78), w paśmie | 4 655 (4 606), w paśmie |

Spadek średniej: m19 ok. 83 %, m06 ok. 78 %. Testy rdzenia PASS. **Potwierdzone przez użytkownika w GUI (2026-11-08):** Validate i Undo/Redo ze szlakami przeszły; użytkownik wybrał własny brush szlaku (ciemny), co wyjaśnia wygląd na zrzutach 9c. Zastrzeżenia: mediana w stanie bazowym była już niska (przypadek rzadki, stąd kontrola średnią); B nadal poza pasmem (artefakt opisany w 9c); ocena wzrokowa po 9d należy do użytkownika; udziały rodzajów po dodaniu szlaków nie były mierzone osobno.

### Regresja G1/G2 po zmianach iteracji 3–9 (2026-11-08)

Sondy na rzeczywistym kliencie 10.98 z prywatnego profilu (`g1_run_resolver_probe.py`, bieżący kod: profile v6–v10, meander, szlaki, nowa reguła plaży, więcej oktaw szumu):
- `--commit` (`g4-reg-commit-20261108a`): `"passed": true`, `input_assets_unchanged: true`, `stderr` pusty; PREVIEW_PASS, COMMIT_PASS, COMMIT_SIZE_PASS ×2, PREVIEW_BASE_GROUND/EXCLUDED/MASK_EPOCH_PASS, testy błędów (preparation, validation, selection) PASS.
- `--surface` (`g4-reg-surface-20261108a`): wszystkie asercje PASS (SURFACE_RESOLVER_PASS, SURFACE_RESOLVER_SIZE_PASS ×6, SURFACE_E2E_PASS ×2, SURFACE_VEGETATION_E2E_PASS ×2, SURFACE_NEXT_TO_TERRAIN_PASS ×3, SURFACE_BORDER_STATS ×6 bez par bez borderu); `"passed": false` wyłącznie dlatego, że `stderr` nie jest pusty: 7 ostrzeżeń wxWidgets `SetFocus` (błąd Win32 87), **identycznych co do liczby i rozmiaru (1 113 bajtów) jak w historycznym przebiegu G2 `g2g-surface-182831`**, czyli znany dług D1/D2, a nie skutek tych zmian.

**Co regresja NIE obejmuje.** Sondy używają profilu z 6 pędzlami (bez szlaków); ścieżkę z 7 pędzlami (szlaki) potwierdził użytkownik w GUI (Preview, Validate, Place, Undo/Redo), bez osobnej sondy. Nie powtarzano pomiarów wydajności.

## Iteracja 10 — przejścia między rodzajami (2026-11-08): m06 8 z 8, m19 2 z 8; kryterium NIE spełnione dla m19

Definicje i kolejne zmiany zapisane przed wykonaniem ([G4_METRICS.md](G4_METRICS.md), „Iteracja 10”, commity `4f5ff75`, `5ec7c07`, `354d9a7`); rdzeń `PatchStats` rozszerzony o skład granic skały i ziemi (test PASS, wartości policzone ręcznie).

**Referencje (skład granicy):** M06 skała RD/RG/RS/RW 33/938/24/4, ziemia DG/DR/DS/DW 951/32/12/4; M19 skała 99/502/251/147, ziemia 728/155/27/88.

| Wariant (v10, bok 512, seedy 1001–1032) | m06 w paśmie | m19 w paśmie | uwagi |
| --- | --- | --- | --- |
| 10a (bez zmian) | 6 z 8 | 3 z 8 | ziemia styka się ze skałą i plażą proporcjonalnie do udziału (RD 254–322 wobec 33–99; DS 145–194 wobec 12–27) |
| 10b (pełna obwódka trawy 2 komórki wokół Rock i Shore) | 8 z 8 | 2 z 8 | RD 0 (m19 ref. 99) poza pasmem, DR 0 (155) wypadło z pasma |
| 10c (obwódka częściowa wokół Rock, ok. 75 %; pełna wokół Shore) | **8 z 8** (RD 36 wobec 33) | **2 z 8** (RD 43 wobec 99 poza pasmem; DR 21 wobec 155 poza; DS 0 w paśmie) | wybrany wariant, stały, bez parametrów |

**Wniosek.** Dla m06 reguła „ziemia nie styka się ze skałą i plażą” odtwarza skład granic (8 z 8, RD 36 wobec 33). Dla m19 jej nie wystarcza: referencja M19 ma znacznie więcej styku skały z ziemią i, przede wszystkim, **skała styka się z plażą i wodą (RS 251, RW 147)**, czego generator nie robi (RS 0), bo skała to najwyższe, a jezioro najniższe komórki wysokości. To wymagałoby zmiany modelu wysokości, nie reguły lokalnej, więc **nie** została wykonana. Kryterium z zapisu (RD i DS w paśmie w obu profilach) nie jest spełnione dla m19; zgodnie z zapisem nie szukano dalszych parametrów dla metryk. Udziały rodzajów w granicach (m06 Grass 403, Dirt 161; m19 294 / 115; zmiana < 5 punktów procentowych).

**Zmiana zachowania.** Reguła obwódki zmienia wyniki Surface dla wszystkich profili i dla `pathLength` 0; przypięty odcisk Seed 1729 100×100 zmieniono z 9975948746684599016 na 17159159863505677800 (test PASS, dodany test reguły obwódki: brak Dirt w odległości 2 od Shore, część styku Dirt–Rock zostaje). **Nie sprawdzono:** wyglądu w GUI, regresji G1 po tej zmianie (zmiana jest w rdzeniu, nie w resolverze), in-sample na dwóch referencjach.

**Ocena użytkownika (2026-11-08, GUI, m06/m19 500×500 z obwódką trawy):** „wygląda ok”; ocena subiektywna, nie zmienia wyniku metryk (kryterium składu granic dla m19 nadal niespełnione).

## Iteracja 11a — osady i struktury: budynki w referencjach (2026-11-08)

Definicje zapisane przed pomiarem ([G4_METRICS.md](G4_METRICS.md), „Iteracja 11”, commit `1a16d26`); funkcja `extractBuildingShapes` z testem (PASS, wartości policzone ręcznie), wyjście sondy `buildings-surface.txt`. Budynek = 8-spójna składowa kafli z cechą Wall lub Door (≥ 12 komórek, obwiednia ≥ 4 × 4), piętra z ≤ 7; wszystkie 19 map; tylko opis.

| Mapa | budynków | na 65 536 kafli | mediana boków (dłuższy × krótszy) | komórek muru | drzwi na budynek | z drzwiami |
| --- | --- | --- | --- | --- | --- | --- |
| M06 | 1 313 | 31 | 9 × 7 | 30 | 1,66 | 51 % |
| M19 | 582 | 37 | 9 × 7 | 24 | 3,44 | 61 % |
| M05 | 13 | 37 | 11 × 8 | 39 | 2,15 | 85 % |
| M15 | 5 | 48 | 11 × 6 | 19 | 0,20 | 20 % |
| M01 | 47 | 94 | 5 × 4 | 16 | 0 | 0 % (to raczej płoty/murki) |
| M11, M12 | 1–2 | — | — | — | — | — |
| pozostałe 12 | 0 | — | — | — | — | — |

**Wniosek.** Budynki da się zmierzyć w pięciu mapach (≥ 5 budynków: M01, M05, M06, M15, M19), a w M06 i M19 są liczne: ok. 1 budynek na 2 000 kafli (czyli ok. 125 w obszarze 512×512), typowo 9 × 7 kafli, ok. 30 komórek muru, w połowie z drzwiami. Generator nie tworzy żadnych budowli, więc stan bazowy jest zerowy z definicji.

**Zastrzeżenia.** Cecha Wall obejmuje też płoty i wolnostojące mury (M01, część M15), więc „budynek” to heurystyka kształtu; wnętrza, dachy (piętra wyżej) i meble są poza miarą; dowody głównie z M06 i M19. **Czego generowanie wymaga (nie zrobione):** ściany i drzwi to nie grunt, lecz przedmioty na kaflu wymagające pędzla ściany z automatycznym dopasowaniem elementów oraz pędzla drzwi; obecny plan (`GroundPlan`) przenosi tylko grunt i doodady, więc podgląd, zatwierdzanie i Undo wymagają rozszerzenia o warstwę przedmiotów strukturalnych.

## Iteracja 11b — podstawy budynków (2026-11-08): cechy w paśmie, warunek udziałów niespełniony dla m19

Model, kryteria i stałe gęstości zapisane przed implementacją ([G4_METRICS.md](G4_METRICS.md), „Iteracja 11”, commit `ae1c33a`). Zaimplementowano: `SurfaceKind::Floor`, `SurfaceTuning::buildingDensity` (0 = wyłączone, nie jest częścią profilu), prostokąty podłogi 7–11 × 5–9 (losowa orientacja) z marginesem 2 komórek na Grass/Dirt, resolver z ośmioma pędzlami (indeks 6 szlak, 7 podłoga), pola „Buildings” i „Floor brush” w oknie generatora (domyślnie 0, `stone floor`), odcisk podglądu z `buildingDensity`.

**Pomiar (seedy 1001–1032, bok 512, v10; gęstość 30 dla m06, 40 dla m19, stałe z góry):**

| Profil | budynków na 65 536 (ref.) | dłuższy bok (ref.) | krótszy bok (ref.) | w paśmie |
| --- | --- | --- | --- | --- |
| m06 | 30 (31) | 9 (9) | 7 (7) | **3 z 3** |
| m19 | 40 (37) | 9 (9) | 7 (7) | **3 z 3** |

Warunek dodatkowy „udziały rodzajów ±1 punkt procentowy wobec stanu bez budynków”: m06 maksymalna zmiana 0,8 punktu procentowego (spełniony), **m19 1,8 punktu procentowego (Grass 294,5 → 276,5 ‰) — niespełniony**: podłogi zabierają ok. 3–4 % powierzchni lądu, więc udział trawy spada, co wynika z konstrukcji, nie z błędu. Testy rdzenia PASS (wyłączone = bajt w bajt jak dotąd; podłoga tylko na dawnym Grass/Dirt; litery prostokąta; wartość 101 odrzucona; deterministyczne). Regresja G1: sonda `--commit` `passed: true` (stderr pusty), sonda `--surface` wszystkie asercje PASS (stderr jak w historycznym przebiegu: 7 ostrzeżeń `SetFocus`, 1 113 bajtów).

**Zastrzeżenia.** Podłoga bez ścian, drzwi i dachów nie jest budynkiem; cechy w paśmie wynikają z zadanych rozmiarów (7–11 × 5–9 dobrane do referencji 9 × 7), więc to sprawdzenie implementacji, nie niezależny dowód; rozmieszczenie losowe (bez dróg i skupisk osad, szlaki nie prowadzą do budynków); porównanie obwiedni podłogi z obwiednią muru to przybliżenie. **Niezweryfikowane:** okno generatora z nowymi polami, Preview/Validate/Place/Undo z podłogami (ścieżka z 8 pędzlami; sondy używają 6), wygląd w GUI.

## Iteracja 11c — ściany budynków (2026-11-08): metryka w paśmie, wygląd i poprawność narożników czekają na użytkownika

Zakres, model i kryteria zapisane przed implementacją ([G4_METRICS.md](G4_METRICS.md), „Iteracja 11c”, commit `6a3048a`; decyzja użytkownika: ściany razem z podłogą, drzwi niewymagane). Zaimplementowano: `GroundProfile::wall` (pędzel ściany) i `GroundPlan::walls` (identyfikator przedmiotu ściany dla każdej komórki obrysu fundamentu; obrys = komórka Floor z 4-sąsiadem innego rodzaju), walidację w resolverze (pędzel ściany istnieje, nie jest dekoracją ściany, przedmiot nie jest gruntem ani meta-itemem) i w podglądzie (przedmiot należy do pędzla, leży na komórce planu, nie ma ściany poza maską), nałożenie po roślinności i wyrównanie elementów przez istniejący `WallBrush::doWalls` na buforze podglądu (bez drugiego silnika), odcisk planu i podglądu ze ścianami, pola „Walls” i „Wall brush” w oknie generatora (aktywne przy „Buildings” > 0).

**Pomiar (seedy 1001–1032, bok 512, v10, gęstość 30 / 40 jak w 11b).** Mediana komórek obrysu na budynek: m06 28 (ref. mur 30, pasmo 20,1–45), m19 28 (ref. 24, pasmo 16,1–36): **w paśmie w obu**. Test rdzenia PASS (obrys = 2·(dłuższy+krótszy)−4 komórek dla każdego fundamentu). Regresja G1: sonda `--commit` `passed: true`, stderr pusty; sonda `--surface` wszystkie asercje PASS (stderr jak historycznie: 7 ostrzeżeń `SetFocus`, 1 113 bajtów). `rme.exe` zlinkowany.

**Czego NIE zweryfikowano.** Pełnej ścieżki GUI ze ścianami (okno, Preview, Validate, Place, Undo/Redo), poprawnego dopasowania elementów ściany (poziome, pionowe, narożniki) przez `WallBrush::doWalls` na kliencie użytkownika, zachowania przy chronionych komórkach i masce; sonda G1 używa profilu bez ścian, więc nowa warstwa nie ma jeszcze testu e2e na rzeczywistym kliencie poza GUI użytkownika; metryka obrysu wynika z rozmiarów zadanych w 11b (sprawdzenie implementacji, nie niezależny dowód). Budynek bez drzwi i dachu nie jest wejściem do rozgrywki.

**Ocena użytkownika (2026-11-08, GUI, ściany budynków, `Buildings` > 0, `Walls` włączone):** „wygląda jak realne budynki”; Preview, Validate, Place, Undo i Redo przeszły bez problemu (potwierdzone przez użytkownika, nie sondą). Tym zamknięta niezweryfikowana dotąd ścieżka GUI z ośmioma pędzlami i warstwą ścian; poprawność dopasowania narożników oceniona wzrokowo przez użytkownika.

## Korekta błędu: kolejność pól `SurfaceTuning` a parser profilu (wykryta 2026-11-08 przy pełnym przebiegu testów przed PR)

**Błąd.** W iteracji 9 dodałem do `SurfaceTuning` pole `pathLength`, a w 11b `buildingDensity`, **przed** polem `meander`. Parser profilu inicjalizował strojenie pozycyjnie (`SurfaceTuning {v[0] … v[10]}`), więc zapisana w profilu wartość `meander` (8) trafiała od iteracji 9 do `pathLength`, a od 11b do `buildingDensity`, a `meander` zostawał 0. Test `test_procedural_style_profile` (round trip strojenia) od 13fac4d był czerwony; **nie uruchamiałem wtedy całego zestawu testów**, tylko pojedyncze (to moje zaniedbanie).

**Skutki.** (1) GUI: okno po wczytaniu profilu zawsze nadpisuje `pathLength` i `buildingDensity` wartościami z pól, więc nie powstawały niechciane szlaki ani budynki, ale **od iteracji 9 do dziś profile nie stosowały meandru rzek** (rzeki w zrzutach po 9 są rzekami bez meandru). (2) Pomiary: `style_render` z profilami v10 miał od 9 do 11 `meander` 0, a w iteracji 10 dodatkowo małe szlaki (`pathLength` 8 z błędnego mapowania), w 11 `buildingDensity` podawane jawnie. Pomiary iteracji 9 (szlaki) podawały `pathLength` jawnie.

**Naprawa.** Kolejność pól przywrócona (pierwsze jedenaście jak w profilu, potem `pathLength`, `buildingDensity`), parser przypisuje pola po nazwach. Wszystkie 14 testów rdzenia PASS.

**Powtórzenie pomiarów z poprawnym wczytywaniem profilu (seedy 1001–1032, bok 512, v10):** szlaki 9d: m06 L 53 / W 4 381 (poprzednio 55,5 / 4 434), m19 L 89 / W 4 618 (91 / 4 655), „szlak przy skale” średnia 8,2 / 16,9 ‰ (8,2 / 16,7): bez zmian w pasmach; przejścia 10c: **m06 7 z 8** (poprzednio 8 z 8: DW 55 wobec 4 wypadło o 5 ‰ poza pasmo ±50 ‰, bo rzeki z meandrem częściej stykają się z ziemią), m19 2 z 8 (bez zmian); budynki 11b: bez zmian (30 / 9 / 7, 40 / 9 / 7), udziały m06 0,8, m19 1,7 punktu procentowego. Kryteria iteracji 10 dla m06 (8 z 8) podane wcześniej były liczone bez meandru; poprawna wartość to 7 z 8.

## Iteracja 12a — osady: skupiska, szlaki i drzwi w referencjach (2026-11-09): PARTIAL, pomiar opisowy

Definicje, stałe i pasma zapisane przed implementacją i pomiarem ([G4_METRICS.md](G4_METRICS.md), „Iteracja 12”, commit `46b7024`; potwierdzone grepem przed implementacją). Kod: `settlement_stats` (rdzeń; test z ręcznie policzonymi wartościami PASS, commit `ff142e6`), wyjście sondy `settlements-surface.txt`, jedno polecenie `docs/procedural/smoke/g5_reference_settlements.py`. Wszystkie 19 map (kopie w `local-data/`, runner hashuje źródło przed i po), piętra z 0..7, klient 10.98, mapowanie `accepted-10.98-v3`. Dwa przebiegi M06 dały bajtowo identyczny plik (`cmp`); pozostałe mapy raz.

| Mapa | budynków | G | Kn (×1000) | Ks | Ki ‰ | Kb ‰ | Bp / Cp ‰ | Bv / Cv ‰ | Dh ‰ | Dn | Db ‰ | Dp / Dp0 ‰ (\|S\|) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- | ---: | ---: | ---: | --- |
| M06 | 1 313 | 4 | 11 167 | 1 | 265 | 661 | 93 / 14 | 287 / 316 | 512 | 2 | 231 | 855 / 594 (69) |
| M19 | 582 | 4 | 11 955 | 1 | 189 | 714 | 209 / 25 | 245 / 268 | 613 | 4 | 563 | 750 / 546 (76) |
| M01 | 47 | 4 | 34 204 | 2 | 148 | 765 | 0 / 0 | 319 / 169 | 0 | N/A | N/A | N/A (0) |
| M05, M11, M12, M15 | 13, 1, 2, 5 | N/A (< 30 budynków) | | | | | | | | | | |
| pozostałe 12 | 0 | N/A | | | | | | | | | | |

Kwalifikują się trzy mapy (≥ 30 budynków): M06, M19, M01. M01 to według 11a raczej płoty (0 drzwi, 0 kafli szlaku wg listy 9a), więc dla skupisk i odstępów jest dowodem pomocniczym, a nie wzorcem osady. Cechy drzwi i orientacji dają tylko M06 i M19.

**Co z tego wynika (opisowo, bez oceny generatora).**
- **Skupiska.** Najbliższy sąsiad leży typowo w odstępie 4 kafli (M06, M19, M01), a 66–71 % budynków (M06, M19) należy do skupisk ≥ 3; samotnych jest 19–27 %. Skupisk jest ok. 11–12 na 65 536 kafli wobec 31–37 budynków na 65 536, więc średnio ok. 3 budynki na skupisko, ale mediana skupiska to 1 (wiele pojedynczych, kilka dużych). Losowe rozmieszczenie z 11b, o gęstości 30–40 na 65 536, daje pewnie znacznie mniej bliskich sąsiadów; to jest do zmierzenia w 12c, nie zakładam.
- **Szlak.** Budynki leżą przy szlaku 6,6× (M06: 93 vs 14 ‰) i 8,4× (M19: 209 vs 25 ‰) częściej niż przeciętny kafel lądu. Relacja jest wyraźna, choć bezwzględnie tylko co 11. (M06) i co 5. (M19) budynek ma kafel szlaku w odległości ≤ 3: szlaki w sensie listy 9a to jedynie część dróg.
- **Woda.** Brak związku: Bv ≤ Cv w M06 (287 vs 316) i M19 (245 vs 268); M01 (319 vs 169) jest jedyną mapą z większym Bv, ale to płoty. Wniosek: generator nie potrzebuje modelu „budynek blisko wody”, wystarczy, że budynek nie stoi na wodzie (już zapewnione marginesem na Grass/Dirt).
- **Drzwi.** Drzwi ma 51 % (M06) i 61 % (M19) budynków, mediana kafli Door to 2 i 4, a tylko 23 % i 56 % kafli Door leży na krawędzi obwiedni. Czyli „drzwi” w referencji to często wiele kafli (okna-drzwi, bramy, drzwi wewnętrzne); założenie „jedno wejście na budynek” to **celowa idealizacja** (odstępstwo zapisane z góry), nie odwzorowanie referencji.
- **Orientacja drzwi względem szlaku.** Wśród budynków z drzwiami brzegowymi i szlakiem w pobliżu (|S| = 69 w M06, 76 w M19) strona z drzwiami jest stroną ze szlakiem w 855 ‰ i 750 ‰ przypadków, wobec 594 ‰ i 546 ‰ przy losowej stronie: drzwi zwracają się ku szlakowi o 26 i 20 punktów procentowych częściej niż przypadek. Niepewność próby (jedna mapa, n ≈ 70): ok. ±40 ‰ (1 odchylenie standardowe z rozkładu dwumianowego), więc kierunek jest wiarygodny, wielkość nie.

**Czego NIE zweryfikowano.**
- Niezależności: wszystko pochodzi z M06 i M19 (D17), M01 jest tylko pomocnicza; brak trzeciej mapy z domami, więc nie ma odrębnej walidacji.
- Rozrzutu między mapami (jedna wartość na mapę; podano tylko niepewność dwumianową dla Dp).
- Szlaków spoza listy 9a (ubita ziemia, trawa wydeptana), a więc rzeczywistej liczby budynków przy drogach: Bp jest dolnym oszacowaniem.
- Czy „budynek” to dom: Wall/Door w referencji obejmuje płoty, bramy i murki (D19); odległości 3, 8 i 12 są stałymi z góry, nie badano czułości na nie.
- Piętra > 7, wnętrz, dachów, mostów.
- Pomiaru generatora: baseline generatora w tych miarach (12c) nie był jeszcze liczony; projekt 12b nie został napisany.

## Iteracja 12b–12d — skupiska, drzwi i szlaki do drzwi w rdzeniu (2026-11-09): PARTIAL, kryterium NIE spełnione

Zakres od użytkownika: drzwi + szlaki do drzwi + skupiska. Model, stałe i kryteria zapisane przed implementacją i przed każdym pomiarem ([G4_METRICS.md](G4_METRICS.md), „Iteracja 12”: definicje `46b7024`, model 12b `7bccd95`, wynik 12c i zapis 12d `99c60d7`; zapisy sprawdzone grepem). Zaimplementowano w rdzeniu (commit `49e6737`): budynki w skupiskach (kotwice losowe, reszta dołączana w odstępie 3–6 kafli do już postawionego), jedne drzwi na budynek (`SurfaceGrid::doors`, środek strony zwróconej ku najbliższemu szlakowi w ≤ 60, inaczej strona losowa), schodek Path przed drzwiami i trasa do najbliższego szlaku (wspólny `PathRouter` wyodrębniony z `drawPaths`). Adapter (pędzel drzwi RME, plan, podgląd, okno generatora) **nie jest zrobiony**.

**Pomiar generatora (seedy 1001–1032, bok 512, profile v10; ustawienia m06 pathLength 40 / buildingDensity 30, m19 80 / 40; mediana; dwa przebiegi bajtowo identyczne).**

| Cecha | ref m06 | 12b (30 % kotwic) | 12d (60 %) | ref m19 | 12b | 12d |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| G (odstęp) | 4 | 4 ✓ | 4 ✓ | 4 | 4 ✓ | 4 ✓ |
| Kn (×1000) | 11 167 | 5 875 ✗ | 9 375 ✓ | 11 955 | 4 750 ✗ | 7 500 ✗ (pasmo od 8 010) |
| Ks | 1 | 2 ✗ | 2 ✗ | 1 | 3 ✗ | 2 ✗ |
| Ki ‰ | 265 | 62 ✗ | 150 ✗ | 189 | 37 ✗ | 72 ✗ |
| Kb ‰ | 661 | 850 ✗ | 741 ✗ (Δ 80) | 714 | 928 ✗ | 875 ✗ |
| Bp − Cp (kierunek) | +79 | +924 ✓ | +910 ✓ | +184 | +908 ✓ | +896 ✓ |
| Bv − Cv (kierunek) | −29 | −130 ✓ | −127 ✓ | −23 | −98 ✓ | −86 ✓ |
| Dp − Dp0 (kierunek) | +261 | +503 ✓ | +527 ✓ | +204 | +495 ✓ | +509 ✓ |
| **cechy zaliczone** | | 4 z 8 | **5 z 8** | | 4 z 8 | **4 z 8** |

**Wynik: kryterium (≥ 75 % ocenianych cech) NIE spełnione ani w 12b, ani po zmianie 12d, w obu profilach.** Stałych dalej nie dobierałem (zapis z góry). Zmiana 12d (kotwice 60 %) poprawiła Kn i Ki, nie przywróciła Ki ani Ks; w stanie obecnym w kodzie jest 60 % (bliższe referencji niż 30 %), co jest odnotowane jako decyzja po zmianie modelu.

**Co jest spełnione z założeń.** Dokładnie jedne drzwi na budynek, drzwi na obrysie, schodek Path, odstęp ≥ 3, drzwi znikają, gdy komórka przestała być Floor po masce (test `settlements`); `buildingDensity` 0 bez zmian: pinned odcisk Seed 1729 i PNG `pathLength` 80 / `buildingDensity` 0 dla 4 seedów **bajt w bajt** identyczne z rdzeniem sprzed zmian (porównanie dwóch binariów); 15 testów rdzenia PASS (`g5_run_suite.py --probes`: obie sondy G1 PASS po przebudowaniu `rme`).

**Zmiany wyników (świadome).** Dla `buildingDensity` > 0: inne rozmieszczenie budynków, Path przed drzwiami i trasy; w sondzie `--surface` liczba komórek ściany dla 100×100 zmieniła się z 150 na 164 (250×250: 1 038 bez zmiany); odcisk `SURFACE_RESOLVER_SIZE` (bez budynków) bez zmiany. Test `buildings` rozluźniony: oprócz Floor dozwolone Path (Grass/Dirt/Shore → Path).

**Odchylenia, których NIE oceniano kryterium.** (1) **Szlaki:** długość (L na 65 536) rośnie z 53 / 89 (9d) do 299 / 344 przy włączonych budynkach (ref 70 / 78), bo każdy budynek dostaje trasę do najbliższej sieci; pasma 9b nie dotyczą już tej konfiguracji. Do rozstrzygnięcia: mniej tras (np. tylko do kafla Path w ≤ 25, albo wspólne trasy klastra) albo `pathLength` 0. (2) **Bp = 1000 z konstrukcji** (schodek), więc ta cecha ma tylko kierunek. (3) Dn = 1 i Db = 1000 są idealizacją (ref 2–4 i 23–56 %).

**Czego NIE zweryfikowano.** Adaptera i GUI (pędzel drzwi przez `DoorBrush`, Preview/Validate/Place/Undo/Redo z drzwiami i schodkami, wygląd osad); „bez dojścia” (odsetek budynków, których schodek nie łączy się z siecią) nie liczony; czułości na stałe 3–6, 12, 60; niezależnych map (D17); wpływu na udziały rodzajów (Path rośnie); wydajności; zachowania przy masce i chronionych komórkach poza testem `settlements`.

## Iteracja 12e — adapter drzwi (2026-11-09): PARTIAL, sondy PASS, GUI czeka na użytkownika

Zakres i kontrole zapisane przed implementacją ([G4_METRICS.md](G4_METRICS.md), „Iteracja 12e”, commit `c0f7883`; zapis sprawdzony grepem). Zaimplementowano (commit `c96a238`): `GroundProfile::door` (rodzaj z zamkniętej listy: Normal door, Locked door, Magic door, Quest door, Normal door (alt), Archway; mapowanie na `DoorType` w `door_types.h`, bez identyfikatorów przedmiotów), `GroundPlan::doors` (flaga na komórce obrysu; w odcisku planu tylko przy drzwiach), walidacja w resolverze, przygotowaniu i podglądzie, nałożenie `DoorBrush::draw` po `WallBrush::doWalls` (odmowa `InvalidItem`, gdy pędzel ściany nie ma takich drzwi dla ułożenia), pola „Doors” i „Door kind” w oknie generatora (aktywne przy włączonych ścianach).

**Znaleziona i naprawiona przeszkoda.** RME zamienia element drzwi w obiekt klasy `Door`; kontrola odcisku podglądu (`hashItem`) przyjmowała wyłącznie klasę `Item` i odrzucała podgląd (`PreviewError::InvalidItem`). Dopuszczono `Door` bez przypisanego ID drzwi domu; nic innego nie zostało poluzowane.

**Wyniki.** Nowy przypadek sondy `--surface` (`SURFACE_DOORS_E2E_PASS`): 100×100 6 drzwi (6 budynków), 250×250 38 drzwi (38 budynków); po Commit dokładnie jeden element drzwi na zaplanowaną komórkę i żaden poza nią, jedna pozycja historii, po Undo zero drzwi i liczba kafli jak przed, po Redo ponownie komplet. Odmowy: drzwi bez pędzla ściany, nieznany rodzaj. Odcisk planu z drzwiami różni się od planu bez drzwi, grunt i ściany planu identyczne. `g5_run_suite.py --probes`: 15 z 15 testów rdzenia, obie sondy PASS na przebudowanym `rme` (ostrzeżeń `SetFocus` w `--surface` jest teraz 11 zamiast 9: przybyły dwa przypadki z `NewMap`; tylko ten znany rodzaj stderr).

**Czego NIE zweryfikowano.** Okna generatora w RME (nowe pola, wygląd, kierunek drzwi względem szlaku, Preview/Validate/Place/Undo/Redo z drzwiami na kliencie użytkownika) czeka na ocenę użytkownika; sonda używa pędzla `stone wall` i „Normal door”; dla innych pędzli ścian i rodzajów drzwi podgląd może odmówić (brak drzwi danego rodzaju w pędzlu); ID drzwi domu nie są przypisywane; trasy do drzwi pozostają ok. 4× dłuższe niż w referencji (patrz 12d).

## Iteracja 12f — strona drzwi ku najbliższemu budynkowi (2026-11-09): wdrożona po uwadze użytkownika, czeka na jego ocenę

Reguła zapisana przed zmianą ([G4_METRICS.md](G4_METRICS.md), „Iteracja 12f”). Pomiar po zmianie (seedy 1001–1032, bok 512, v10; dwa przebiegi bajtowo identyczne): G, Kn, Ks, Ki, Kb **bez zmian** względem 12d (rozmieszczenie budynków nie zależy od drzwi), Dp 1000 (jak dotąd), długość szlaków L 303 / 337 (było 299 / 344). Kryterium 12c nadal nie spełnione (5 z 8 w m06, 4 z 8 w m19). 15 testów rdzenia i obie sondy G1 PASS po przebudowaniu `rme` (sonda drzwi: 6 i 38). **Nie zweryfikowano:** testem jednostkowym, że drzwi sąsiednich budynków bez szlaku są zwrócone ku sobie (zapisane w ocenie, ale nie napisane); poprawy na rzeczywistym zrzucie (ocena użytkownika).

**Ocena użytkownika po 12f (2026-11-09, GUI, dwa zrzuty, Undo/Redo i Validate działają):** drzwi i szlaki do drzwi uznane za w porządku, użytkownik chce iść dalej. Moje uwagi z tych zrzutów (nie zmierzone): budynek bez szlaku w zasięgu ma tylko krótki schodek bez trasy (zrzut 1 u dołu, zrzut 2 przy prawym budynku); drzwi lewego budynku na zrzucie 2 zwracają się ku odległemu szlaku (zasięg 60), a nie ku sąsiadowi 25 kafli obok; długie szlaki 9d kończą się zaokrąglonym „blobem” w środku lądu; szlaki zajmują dużo powierzchni (L ok. 4× referencji, 12d). Ocena ogólna użytkownika ma pierwszeństwo przed metryką kryterium 12c (nadal niespełnionego); rozbieżność odnotowana.

## Iteracja 12g — krótsze szlaki do drzwi (2026-11-09): L w paśmie przy `pathLength` 0; cena: ok. 2/3 budynków bez trasy

Kryteria i reguła wyboru zapisane przed implementacją ([G4_METRICS.md](G4_METRICS.md), „Iteracja 12g”; grep potwierdził). Nowa stała `doorTrailReach`: trasa od schodka tylko do kafla Path w ≤ R od środka budynku. Wynik pomiaru wariantów (seedy 1001–1032, bok 512, v10, `pathLength` 0; mediana; dwa przebiegi bajtowo identyczne):

| R | L m06 (ref 70, pasmo 47–105) | L m19 (ref 78, 52–117) | bez dojścia m06 / m19 (‰) |
| --- | ---: | ---: | ---: |
| 16 | 20,5 | 25,5 | 696 / 672 |
| **24 (wybrane wg reguły)** | **48** | **72,5** | 671 / 637 |
| 36 | 116 ✗ | 151 ✗ | 629 / 597 |
| poprzednio (60) | ok. 300 | ok. 340 | n/d |

Wybór R = 24 (największy spełniający L ≤ 1,5·ref w obu profilach). G, Kn, Ks, Ki, Kb, Bp i Dp bez zmian względem 12d/12f (rozmieszczenie to samo). **Przy domyślnych ustawieniach profilu** (`pathLength` 40 / 80 jak w GUI-owych sugestiach) L wynosi 108 / 177, bo dochodzą losowe długie szlaki 9d; wynik m06 jest tuż nad pasmem, m19 powyżej.
**Koszt:** „bez dojścia” (schodek bez trasy do innych drzwi) to ok. 64–67 % budynków; w referencjach tylko 9–21 % budynków ma szlak w pobliżu (Bp 93 / 209 ‰), więc odsetek jest tej samej klasy, ale wynika z krótkiego zasięgu, a nie z modelu referencji. 15 testów rdzenia i obie sondy G1 PASS po przebudowaniu `rme`. **Nie zweryfikowano:** wyglądu w GUI (ocena użytkownika, ma pierwszeństwo); innych `doorTrailReach` poza trzema z zapisu; `style_render` ma teraz wiersz `access` (odsetek bez dojścia) liczony tylko, gdy są drzwi.

## Iteracja 12h — szlaki do drzwi mają dokądś prowadzić (2026-11-09): kikuty usunięte, L poza pasmem w m19; czeka na ocenę użytkownika

Kryteria i reguła wyboru zapisane przed implementacją ([G4_METRICS.md](G4_METRICS.md), „Iteracja 12h”; grep potwierdził). Zmiany: (1) trasy do drzwi stemplowane aż do schodka i celu (`reachEnds`; długie szlaki 9d bez zmian), (2) schodek tylko przy celu w zasięgu albo przy późniejszym partnerze w zasięgu; drzwi samotnego budynku wychodzą prosto na grunt; strona drzwi ku partnerowi.

Pomiar (seedy 1001–1032, bok 512, v10, `pathLength` 0; mediana; dwa przebiegi bajtowo identyczne):

| `doorTrailReach` | L m06 (≤ 105) | L m19 (≤ 117) | kikuty m06 / m19 (‰) | drzwi bez schodka m06 / m19 (‰) |
| --- | ---: | ---: | ---: | ---: |
| 24 | 174 ✗ | 277 ✗ | 25 / 18 | 104 / 43 |
| **16 (wybrane wg reguły, bo 24 nie spełnia)** | **96 ✓** | **174 ✗** | **33 / 31** | 208 / 103 |

Kryterium (a) L: **niespełnione w m19** (174 wobec ≤ 117), spełnione w m06; kryterium (b) kikuty ≤ 100 ‰: **spełnione** (było ok. 650 ‰ „izolowanych” w 12g). G, Kn, Ki, Kb, Dp bez zmian; Bp 791 / 897 ‰ (nie 1000, bo samotne budynki nie mają schodka). 15 testów rdzenia i obie sondy G1 PASS po przebudowaniu `rme`; test `settlements` zmieniony świadomie: schodek przed drzwiami nie jest już wymagany (dla boku ≥ 250 wymagany co najmniej jeden). **Nie zweryfikowano:** wyglądu w GUI (ocena użytkownika ma pierwszeństwo); zasięgów innych niż 16 i 24; „dokądś prowadzi” poza metryką kikutów (trasa dwóch sąsiadów może być łamana lub szeroka).
