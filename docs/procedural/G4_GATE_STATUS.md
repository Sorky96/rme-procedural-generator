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
