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
