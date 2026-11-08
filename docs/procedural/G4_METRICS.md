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
