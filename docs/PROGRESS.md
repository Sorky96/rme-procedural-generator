# Procedural World Generator — postęp

## 2026-11-08 — D13: pokrycie gruntu (PARTIAL, 96 nowych kluczy zaakceptowanych 2026-11-08)

Przyczyna: ground bez brusha w `data/` 10.98 dostawał pusty klucz (650 item id, 789 tys. kafli). Naprawa: opcja normalizatora nadaje klucz `item <nazwa z definicji klienta>`; G3 bez regresji (190 plików bajtowo identycznych z opcją wyłączoną). Bez gruntu: 116 ‰ → 37 ‰ kafli; użytecznych map na powierzchni 5 → 9; podział po treści bez scaleń (poprzednie scalenia były artefaktem pustych kluczy), 9 rodzin; dopasowanie poprawia walidację w 6 z 8 foldów (wcześniej 2 z 4), baseline wygrywa w 2. [G4_COVERAGE.md](procedural/G4_COVERAGE.md).

## 2026-11-08 — G4-C: cel stylu i dopasowanie parametrów (PARTIAL, wynik w dużej mierze negatywny)

Mapowanie z G4-B zaakceptowane. Cel z map treningowych (waga równa na rodzinę), deterministyczne dopasowanie 7 parametrów `SurfaceTuning` po stałych siatkach, profile `g4-profile/2`; testy PASS, dwie serie bajtowo identyczne. Z 12 map stylu tylko 5 (M05, M06, M11, M14, M19) ma kafle w gruntach z brushy na powierzchni; reszta to prawie wyłącznie „ground bez brusha”. Dopasowanie obniża dystans do celu treningowego 2–10×, ale na rodzinie walidacyjnej poprawia 3–7 % lub pogarsza (2 z 4 foldów wybiera baseline). Nic nie liczono na zbiorze testowym. [G4_FIT.md](procedural/G4_FIT.md).

## 2026-11-07 — G4-B: schemat profilu i mapowanie klucz→rodzaj (PARTIAL)

`g4-profile/1` (klient, hashe `items.otb`/`Tibia.dat`, poziom i piętra, źródła z wagami, mapowanie, statystyki całkowitoliczbowe) z walidacją, odmową niezgodnego klienta/assetów/poziomu i projekcją statystyk przez mapowanie; mapowanie po całych słowach nazwy brusha, bez ID. Test PASS. Propozycja mapowania dla 10.98 (108 kluczy, 5 Unmapped = 2 ‰ kafli) **czeka na akceptację użytkownika**; profile nie są jeszcze wypełniane (G4-C). [G4_PROFILE.md](procedural/G4_PROFILE.md).

## 2026-11-07 — G4-A: manifest, podział po rodzinach, test podobieństwa (PARTIAL)

Rdzeń: odciski okien 16×16 (shift-niezmiennicze, bez pozycji i piętra), zawieranie w ‰, filtr okien ogólnych, manifest `g4-manifest/1`, rodziny po treści i 5 foldów leave-one-family-out; narzędzie `reference_split`. Test rdzenia PASS. Na 19 mapach 10.98: pary shape 1000 ‰ (kontrola pozytywna), 5 foldów z przeciekiem ≤ 45 ‰; dwie serie bajtowo identyczne, raporty G3 bez zmian. Pierwszy przebieg zdegenerował się (9 map w jednej rodzinie), więc dodano filtr okien ogólnych (jawnie opisane). Nie ma testu memoryzacji wyniku generatora ani podziału BlackTek. [G4_SPLIT.md](procedural/G4_SPLIT.md). Następne: G4-B (schemat profilu, mapowanie klucz→rodzaj).

## 2026-11-07 — G4-0: zgodność items.otb z klientem 10.98 (PARTIAL)

Gałąź `feature/procedural-g4-style-learning` z `origin/master` (PR #4 scalony). Niezależny skrypt porównał 25 548 itemów `data/1098/items.otb` z `Tibia.dat` zwykłego 10.98 w sześciu cechach: 0 niezgodności; nagłówek 3.57 na wszystkich 19 mapach; dwie serie bajtowo identyczne. Nie dowiedziono, że mapy powstały z tym konkretnym OTB (brak drugiego pliku). [G4_ITEMS_OTB.md](procedural/G4_ITEMS_OTB.md), D11 zaktualizowany. Następne: G4-A (manifest, podział po rodzinach, test memoryzacji).

## 2026-11-07 — G3 zamknięte z długiem (decyzja użytkownika)

Bramka G3 „Import i analiza map referencyjnych” zamknięta jako PASS z długiem, bez twierdzenia, że wszystko zweryfikowano: D8 (resztki: ładowanie `Map` to ok. 94 % szczytu pamięci, anulowanie przy normalizacji ok. 1,2 s), D9 (cechy z 4.2: rzeki, jaskinie, szlaki, osady, mikrostruktury, wąskie gardła), D10 (brak testu podobieństwa/memoryzacji i podziału zbiorów), D11 (companion XML, kontenery, niepotwierdzony `items.otb`, brak HTML, jedna maszyna). Kryterium bramki spełnione dla 20 map (19 zwykłego 10.98, 1 BlackTek): powtarzalne raporty (dwie serie, bajtowo identyczne), statystyki i mapa semantyczna (PNG na piętro, niezależnie zweryfikowany). Kontrola niezależna: Map Statistics w RME zgodne co do kafli, itemów i kafli nieprzechodnich. [Stan bramki](procedural/G3_GATE_STATUS.md), [dług](procedural/TECH_DEBT.md). Następne: G4 (profile stylu, dopasowanie rozkładów, holdout), najpierw D10.

## 2026-11-07 — D7 zamknięty decyzją użytkownika

Użytkownik zamknął dług D7 (G2): zrzuty seedów 4242, 7 i 1 ocenił jako OK. To jego ocena wizualna; nie wykonano przeliczenia szacunku pamięci Surface (40 B/komórkę) ani testu płynności szerokości rzeki, co zapisano w [TECH_DEBT.md](procedural/TECH_DEBT.md) i [G2_GATE_STATUS.md](procedural/G2_GATE_STATUS.md). D5 i D6 zostają.

## 2026-11-07 — D8: ręczny test w RME zaliczony

Użytkownik otworzył `forgotten.otbm` w zwykłym RME po zmianie loadera i podał Map → Statistics: 340 595 kafli, 429 037 itemów i 244 283 kafli nieprzechodnich, identycznie jak raporty G3 (kafle, itemy, cecha `blocking`). Potwierdza to import i cechę `blocking` niezależnym programem oraz poprawne działanie ścieżki edytora bez haka. Definicje pozostałych pozycji różnią się (przechodniość, przesuwalne, kontenery), nie błąd; szczegóły w [G3_REFERENCES.md](procedural/G3_REFERENCES.md#kontrola-niezależna-statystyki-rme-gui-ścieżka-edytora). D8: usunięto punkt „brak testu w RME”.

## 2026-11-07 — D8: dług G3 domknięty dalej (hak przerwania w loaderze)

Dodano opcjonalny predykat przerwania do `IOMapOTBM` (osobna zmiana w `source/io`, 45 linii; bez haka zachowanie bez zmian) i użyto go w imporcie: limit pamięci obowiązuje w trakcie ładowania (nadwyżka 3–21 MiB zamiast 293 MiB), a anulowanie w ładowaniu trwa 78–310 ms zamiast 1,4–1,7 s. Dzięki twardej kontroli w ładowaniu szacunek przed ładowaniem jest dolną granicą (16 B/B), więc M06 (71 MiB) przechodzi przy domyślnym limicie 2 048 MiB (szczyt 1 900 MiB). Regresja: 295 plików zestawu i komplet `forgotten.otbm` bajtowo identyczne; ścieżka edytora (bez haka) daje identyczną liczbę kafli na 5 mapach. Resztki: ładowanie `Map` to 94 % szczytu (wymaga innego sposobu wczytywania), anulowanie przy normalizacji ok. 1,2 s, brak ręcznego testu w RME. Szczegóły: [G3_REFERENCES.md](procedural/G3_REFERENCES.md#limit-pamięci-i-anulowanie-d8). **Do wykonania przez użytkownika:** krótki test otwarcia mapy w RME (kroki w odpowiedzi).

## 2026-11-07 — G3: eksport mapy semantycznej i status bramki

Mapa semantyczna jest teraz zapisywana jako artefakt: deterministyczny PNG (8-bitowy indeksowany, bez zależności) na piętro plus `legend.json` (sloty, kolory, ramki pięter), w `local-data/` (prywatne). Niezależny weryfikator (Python `zlib`) przeszedł wszystkie PNG 19 map zwykłego 10.98 i `forgotten.otbm` (wymiary, CRC, filtry, liczba pikseli = liczba kafli, piksele „unknown” = komórki unknown), dwie serie dały 105 bajtowo identycznych plików, pozostałe raporty bez zmian. Wyjaśniono `unknown` w M01/M06 (id spoza items.otb i „deprecated”). Dodano [G3_GATE_STATUS.md](procedural/G3_GATE_STATUS.md): kryteria bramki i luki, decyzja o zamknięciu należy do użytkownika. Następne: decyzja o G3, brakujące cechy z 4.2 (rzeki, jaskinie, szlaki, osady, mikrostruktury), test podobieństwa/memoryzacji.

## 2026-11-07 — D8: limit pamięci i anulowanie importu (zamknięty z długiem resztkowym)

Import ma limit pamięci procesu (szacunek 40 B/B przed ładowaniem, pomiar po nim; gzip przez trailer ISIZE) i `stop_token` (przed/po haszowaniu i ładowaniu, co 4 096 kafli, co wiersz w analizach). Pomiary na M06 (71 MiB): odmowa przy 2 048 MiB, PASS przy 3 328 MiB (szczyt 1 898 MiB, z czego ładowanie 1 792), opóźnienie anulowania 154–1 713 ms w imporcie i 5–428 ms w analizie. Regresja: 190 plików zestawu i komplet `forgotten.otbm` bajtowo identyczne. Resztki: loader nieprzerywalny (ładowanie i zniszczenie mapy do 1,7 s), szacunek konserwatywny (M06 nie mieści się w 2 GiB wg szacunku, choć zużywa 1 898 MiB), przekroczenie możliwe w trakcie ładowania. Szczegóły: [G3_REFERENCES.md](procedural/G3_REFERENCES.md#limit-pamięci-i-anulowanie-d8). Następne: kalibracja szacunku na większej liczbie map, punkt anulowania w loaderze (osobny commit), brakujące cechy z 4.2.

## 2026-11-07 — G3-E: zestaw 19 map zwykłego 10.98 (PARTIAL)

Analiza G3-A…D uruchomiona na 19 mapach klienta 10.98 (osobny klient od BlackTek: `Tibia.dat` ma 23 725 vs 26 381 itemów). Dwie pełne serie dały 190 bajtowo identycznych plików raportów, źródła i `Tibia.dat/spr` bez zmian, 38 przebiegów bez stderr/dialogów. Prawdziwe `unknown` na dwóch mapach (M01: 90 sztuk w 14 id, pokrycie 99,8 %; M06: 48 w 15 id). Szczyt pamięci mapy 71 MiB (7,8 mln kafli): 1 899 MiB, 93 % budżetu 2 GB: dług D8. Błąd w moim runnerze (start RME ładował BlackTek, bo profil nie miał wpisu „10.98”) naprawiony i opisany. Nie oceniano sensowności cech, brak anulowania i limitu pamięci. Szczegóły: [G3_REFERENCES.md](procedural/G3_REFERENCES.md). Następne: limit pamięci i anulowanie importu (D8), brakujące cechy z 4.2, wyjaśnienie unknown w M01/M06.

## 2026-11-07 — G3-D: ekstrakcja cech i raport zbiorczy (PARTIAL)

Dodano `extractFeatures` (sąsiedztwo gruntów, bloki 16/32/64, cechy per piętro, częstość cech vs odległość od gruntu nieprzechodniego) i renderery JSON/MD/CSV (liczby całkowite, bez czasu i locale), próbnik zapisuje 7 plików. `forgotten.otbm`: 100,0 % pokrycia, dwa przebiegi bajtowo identyczne we wszystkich plikach; fixture z `unknown`: 98,7 %. Test na mapie 3×2 policzonej ręcznie PASS. Nie zrobiono: rzek, jaskiń, szlaków, osad, mikrostruktur, testu podobieństwa, HTML; tylko jedna referencja. Szczegóły: [G3_FEATURES.md](procedural/G3_FEATURES.md). Następne: G3-E (powtarzalność na kilku referencjach, pamięć, anulowanie), brakujące cechy z sekcji 4.2.

## 2026-11-07 — G3-C: regiony i topologia (PARTIAL)

Dodano `analyzeTopology` (rdzeń): składowe przechodnie, regiony tego samego gruntu (nieprzechodnie, zamknięte) i cechy pięter, bez ID i nazw, z anulowaniem. `forgotten.otbm`: 95 103 pola przechodnie, na piętrze 7 największa składowa 22 634 z 46 534 pól, sumy kafli zgodne, dwa przebiegi bajtowo identyczne. Nie zrobiono wąskich gardeł, rzek/jaskiń/szlaków, połączeń pięter; liczenia nie porównano z niezależną implementacją. Szczegóły: [G3_TOPOLOGY.md](procedural/G3_TOPOLOGY.md). Następne: G3-D (feature extraction i raport zbiorczy), wąskie gardła, kontenery.

## 2026-11-07 — G3-B: normalizator semantyczny (PARTIAL)

Dodano `SemanticMap` (rdzeń) i `normalizeReference` (adapter): klucz gruntu z pędzla klienta i 16 cech strukturalnych z definicji itemów, bez wpisanych ID. `forgotten.otbm`: 3 028 669 komórek, 77 kluczy gruntu, suma kafli po kluczach = 340 595, dwa przebiegi bajtowo identyczne. Kontrola sumy znalazła błąd (93 kafle bez cech traktowane jako puste), naprawiony bitem `Present`. Ścieżka `unknown` zweryfikowana na syntetycznym fixture (8/8 kontroli, kontrola negatywna); nie sprawdzono jej na mapie z realnymi brakami, kontenerów ani pamięci procesu. Szczegóły: [G3_NORMALIZER.md](procedural/G3_NORMALIZER.md). Następne: G3-C (regiony, biomy, topologia).

## 2026-11-07 — G3-A: import tylko do odczytu (PARTIAL)

Gałąź `feature/procedural-g3-reference-analysis`. Dodano SHA-256 i werdykt zgodności (rdzeń), adapter importu bez `Editor` i próbnik na prywatnej kopii. `forgotten.otbm`: zgodna z 1098-BlackTek, 340 595 kafli, 0 unknown, dwa przebiegi bajtowo identyczne, źródło bez zmian. Nie zrobiono: kontenerów, companion XML, anulowania/limitu pamięci, G3-B…E. Szczegóły i luki: [G3_IMPORT.md](procedural/G3_IMPORT.md). Następne: G3-B (normalizator semantyczny i `unknown`), kontenery, fixture z niezgodną wersją.

## 2026-11-07 — G2 zamknięte (decyzja użytkownika)

Bramka G2 „Sensowna powierzchnia i border” zamknięta jako PASS z długiem, bez podawania, że wszystko zweryfikowano: D5 (szew obok terenu), D6 (rzeki głównie po osiach), D7 (brak ostatniego przeglądu seeda 4242, szacunek pamięci nieprzeliczony), tylko klient 1098-BlackTek. Gałąź `feature/procedural-g2-surface`: 19 lokalnych commitów, nic nie wypchnięte, brak PR. [Stan bramki](procedural/G2_GATE_STATUS.md), [dług](procedural/TECH_DEBT.md).

## 2026-11-07 — G2: wyspy w jeziorach, małe stawy, gładsze rzeki

Po zrzutach: wypełnianie wysp <24 komórek w jeziorach, pierścienie brzegu zależne od wielkości jeziora, rzeki rysowane po wygładzonej osi z płynnie rosnącą szerokością. Test rdzenia, `--surface`, `--commit` PASS, odcisk Seed 1729 → 6783381837833077004. Wygląd w RME i `rme.exe` po tej zmianie niezweryfikowane (RME działał). [Szczegóły](procedural/G2_GATE_STATUS.md).

## 2026-11-07 — G2: zrzuty kilku seedów (4242, 7, 1)

Zrzuty zapisane w `docs/procedural/screenshots/`; użytkownik nie wystawił oceny. Obserwacje z zrzutów: piasek nieregularny i dobry, ale na seedzie 7 wyspa piasku w jeziorze, na seedzie 1 rzeka z kątami prostymi i skokową szerokością. Kryterium wizualne G2 nadal otwarte. [Szczegóły](procedural/G2_GATE_STATUS.md).

## 2026-11-07 — G2: nieregularny brzeg jezior

Po uwadze recenzenta (piasek równomierny, trawa powinna się wcinać): `shoreWidth` 3, pierwszy pierścień zawsze, dalsze według gładkiego szumu. Rozkład kafli piasku 1/2/3 kroki od wody 4253/3290/1273 (24 seedy 100×100). Test rdzenia, `--surface`, `--commit` PASS, odcisk Seed 1729 → 7666204984290374682. Wygląd w RME niezweryfikowany. [Szczegóły](procedural/G2_GATE_STATUS.md).

## 2026-11-07 — D5: Surface obok istniejącego terenu

Bordery Surface przy istniejącym terenie powodowały odmowę podglądu (zmiana poza obszarem). Odtworzone testem `nextToTerrain` w `--surface`; poprawka: komórki zapisywalne obok niezapisywalnych z gruntem zostają puste (1-kaflowy szew), `surfaceKeepClear` wspólne dla przygotowania i preview (ścisła równość zachowana). `SURFACE_NEXT_TO_TERRAIN_PASS` (100×100, z kaflem w środku, 250×250), regresje `--preview`/`--commit`/rdzeń PASS, wiersze commitu G1 bez zmian. Szew i wygląd w RME niezweryfikowane. [Szczegóły](procedural/G2_GATE_STATUS.md).

## 2026-11-07 — G2: rzeki ze skał o zmiennej szerokości, brushe z list

Po teście GUI: źródła rzek tylko ze skał, szerokość rzeki 2–4 kafli rosnąca z odległością od źródła i ze zlewami (po zrzucie: pierwsza wersja zmieniała szerokość tylko przy zlewie), pola brushy w dialogu zamienione na listy rozwijane (grunt / doodad). Test rdzenia, `--surface`, `--preview`, `--commit` na kliencie 1098 PASS (D1 bez zmian), odcisk Seed 1729 → 1951698793224813922; wolne otwieranie dialogu (user: dialog; ~70 tys. pozycji w 8 listach) → `wxComboBox` readonly ładowany przy pierwszym rozwinięciu, doodady tylko z pojedynczymi itemami; niezmierzone i nieprzetestowane natywnie. Wygląd i listy w RME niezweryfikowane. [Szczegóły](procedural/G2_GATE_STATUS.md).

## 2026-11-07 — G2-D: kontrole rozkładów, szerokość rzeki, stan bramki

Test rdzenia sprawdza teraz szerokość rzeki (okno 2×2) i górną granicę szerokości brzegu oraz 3 seedy 500×500. Kontrola znalazła jednokaflowy ogonek na źródle rzeki; poprawione (komórka południowo-wschodnia przy rozszerzaniu), przypięty odcisk Seed 1729 → 13556304753518616142. `--surface`, `--preview`, `--commit` na kliencie 1098 przeszły, wiersze commitu G1 bez zmian, `pairs_without_border=0`. Status bramki G2 **PARTIAL**: brak przeglądu wizualnego kilku seedów w RME, D5 otwarty. [Stan bramki](procedural/G2_GATE_STATUS.md).

## 2026-11-07 — G2: szerszy piasek (shoreWidth 2) po teście GUI G2-C

Użytkownik: roślinność w GUI wygląda dobrze, Undo/Redo działa; piasek wokół jezior za cienki. Domyślne `shoreWidth` 1 → 2 (zakres 0–4 bez zmian), przypięty odcisk Seed 1729 100x100 zmieniony na 4385766052247269449. `test_procedural_surface` PASS, próbnik `--surface` (100 i 250, bez i z roślinnością) PASS, `pairs_without_border=0`. Test GUI użytkownika (500x500, roślinność włączona): piasek, drzewa, kamienie i skały oceniono jako „ok”; to ocena wizualna jednego widoku, bez Undo/Redo i bez wielu seedów na tym rozmiarze.

## 2026-11-07 — G2-C: roślinność i detale zależne od terenu

Nowa warstwa dekoracji dla Surface (opcja wyłączona domyślnie): drzewa, krzaki i kamienie z brushy doodad (tylko pojedyncze itemy), reguły zależne od terenu (drzewa tylko na trawie i nie przy wodzie, kamienie na ziemi i przy skale). `GroundPlan::decor`, podgląd, commit z jednym Undo, dialog. `test_procedural_surface`, próbnik `--surface` (100 i 250, Undo i Redo), regresje G1 (`--preview`, `--commit`, rdzeń) PASS. Status **PARTIAL**: dialog i wygląd w RME niezweryfikowane, wielokaflowe doodady poza zakresem. [Raport](procedural/G2_VEGETATION.md).

## 2026-11-07 — G2: poprawki wyglądu po pierwszym teście GUI

Test GUI potwierdził Natural surface, Undo i Redo (po restarcie RME zniknęły nakładki „entry/spawn”, które nie pochodziły z generatora). Uwagi: ziemia „poszatkowana”, rzeki za wąskie. Zmieniono gładkie przejście trawa/ziemia z usuwaniem płatów < 24 komórek oraz poszerzenie rzek do ≥ 2 kafli; `test_procedural_surface` i `--surface` PASS, fingerprinty przepięte. Status **PARTIAL**: efekt wizualny czeka na ponowny ogląd w RME. [Raport](procedural/G2_CORE.md).

## 2026-11-07 — G2-B2: powierzchnia v2 w potoku Preview/Commit

`GenerationConfig` (algorithm, surface), `GroundProfile` (count, 3 lub 6 brushy), przygotowanie, hash podglądu, resolver i dialog („Terrain style”) obsługują Surface obok niezmienionego Classic. Regresje G1 przeszły: resolver, `--preview` i `--commit` z fingerprintami zgodnymi z dokumentami G1. `--surface` end-to-end (przygotowanie, walidacja, commit jako jedno Undo, Undo, Redo) działa dla 100×100 i 250×250 po podniesieniu limitu Undo do 512 MiB; runner dał `passed: false` tylko przez dwa ostrzeżenia `SetFocus` w stderr (D1). Domyślny limit Undo 40 MiB odrzuca 250×250 (`HistoryRefused`). Dodano test-only zamykanie okna „Warnings” w próbnikach. Status **PARTIAL**: nie sprawdzono dialogu w GUI ani wyglądu w RME. [Raport](procedural/G2_PIPELINE.md).

## 2026-11-06 — G2-B: resolver powierzchni i bordery na kliencie 1098

`resolveSurfacePlan` (profil grass, dirt, rocky ground, sand, sea, sea) i istniejący `GroundBorderCalculator` przeszły próbnik na rzeczywistym kliencie (`g2-surface-resolver-2-20261106`, `SURFACE_RESOLVER_PASS`; pierwszy przebieg padł na pustej konfiguracji klienta, zanim wykonał się kod resolvera). Sześć przebiegów 100×100–500×500, zero par różnych brushy bez bordera, ale borders sięgają poza obszar przy cudzym sąsiedzie (95 i 50 kafli), co potok G1 odrzuca: dług D5. Status **PARTIAL**: brak wpięcia w Preview/Commit/dialog i wyniku w RME, czasy to pojedyncze serie. [Raport](procedural/G2_BORDERS.md).

## 2026-11-06 — G2-A: rdzeń powierzchni v2

Nowy generator `surface.h/.cpp` (jeziora, rzeki, skała, brzegi, pas przejściowy trawa/ziemia) obok niezmienionego v1. `test_procedural_surface` przeszedł (24 seedy 100×100, 6 seedów 250×250, 92 wstrzyknięte awarie alokacji, maska bez wpływu na inne komórki); `test_procedural_core` nadal PASS. Status **PARTIAL**: nie ma adaptera ani wyniku w RME, czytelność i borders nieocenione. Czasy to pojedyncze serie. [Raport](procedural/G2_CORE.md). G2 w toku.

## 2026-11-06 — G1: zamknięte z długiem

Decyzja użytkownika: G1 zamknięte z udokumentowanym długiem. Pięć kryteriów bramki ma dowody, progi zasobów 500×500 i 100×100 są zatwierdzone i spełnione; otwarte pozycje D1–D4 są w [TECH_DEBT.md](procedural/TECH_DEBT.md). Zastrzeżenia: czasy Place 500×500 pochodzą z EXE `15c25557…`, nie z obecnego `b87d1abb…`; wiele dowodów to pojedyncze serie; PR #1 nie ma CI, a commity pośrednie nie były budowane osobno. Sesje live poza zakresem. Zmieniono tylko status w `G1_GATE_STATUS.md`, liście Fazy 1 i tym pliku; wpisy poniżej z napisem „G1 IN PROGRESS” to historia z daty pomiaru. G2 nierozpoczęte.

## 2026-11-06 — G1: zatwierdzenie progów i dług techniczny

Zatwierdzono progi 500×500 i 100×100 (w tym Preview 100×100 ≤ 250 ms) i uznano weryfikator za wystarczający. Zmierzone wartości mieszczą się w progach (ocena w `G1_GATE_STATUS.md`), z zastrzeżeniem, że czasy Place 500×500 pochodzą z EXE `15c25557…`, a nie z obecnego. Nowy `docs/procedural/TECH_DEBT.md` zapisuje jako dług: `SetFocus` przy zamykaniu (D1), niewyjaśniony stos aktywacji (D2), awarie alokacji poza przetestowanymi miejscami (D3) i niezbadane skoki pamięci (D4). Sesje live poza zakresem.

## 2026-11-06 — G1: SetFocus przy zamykaniu i błąd próbnika śladu

Ślad z `--roundtrip --focus-trace` pokazał, że `SetFocus` przy File → Exit pochodzi z `MapTabbook::DeleteTab` przy ramce wyłączonej przez okno „Closing” (błąd Win32 87). Przebiegi ze śladem kończyły się `0xC0000374`, bo `ProceduralFocusTrace` był usuwany dwa razy (`unique_ptr` i wx przy zamykaniu); po zwolnieniu własności kod wyjścia to 0. Sesje live uznano za poza zakresem G1. Stos aktywacji z 2026-11-04 nadal bez wyjaśnienia. Zmiana w kodzie produkcyjnym nie była wprowadzana.

## 2026-11-06 — G1: lista Fazy 1 uzupełniona o dowody

Lista Fazy 1 w `PROJECT_RME_PROCEDURAL_GENERATOR.md` pokazywała nieodhaczone pozycje, dla których istniały już dowody (resolver, commit z Undo/Redo, dialog, testy). Odhaczono je z odnośnikami do raportów; jedna pozycja (testy diffu, anulowania, budżetu, unieważnienia) zostaje częściowa: brak rzeczywistej sesji sieciowej live i nie wszystkich miejsc awarii alokacji. Użytkownik uznał wzrost pamięci w serii 200 cykli za niebędący blokerem. Bramka G1 pozostaje IN PROGRESS. Nie weryfikowano na nowo samych dowodów, tylko ich odwzorowanie na listę.

## 2026-11-06 — G1: seria 200 cykli trendu pamięci

`g1-leak-200-20261106` (samosterowany, 500×500, EXE `b87d1abb…`): kryterium ustalone z góry spełnione (nachylenie WS i private 0,31 MiB/cykl, GDI/USER/uchwyty bez wzrostu po rozgrzewce, stderr pusty). Pomocniczy test ograniczoności ustalony przed uruchomieniem (różnica średnich cykli 101–150 i 151–200 ≤ 5 MiB) nie jest spełniony: +8,6 MiB. Pamięć rośnie skokami i wolniej niż w pierwszych 30 cyklach, ale plateau nie wykazano; przyczyna nie zbadana.

## 2026-11-06 — G1: pełny round-trip OTBM na obecnym binarium

Powtórzono Place → Undo → Redo → Save → Close → Open dla 100×100 (`g1-roundtrip-100-cur-c-20261106`) i 500×500 (`g1-roundtrip-500-cur-20261106`) na EXE `b87d1abb…`. Oba: `ROUNDTRIP_PASS`, wczytane 10 005 i 250 005 kafli, verifier `PARTIAL…FOCUS_WARNING` (ostrzeżenie `SetFocus` w stderr). Weryfikator ignoruje cztery linie rozszerzenia Google Drive i zapisuje je w `ignored_external_stderr`. Dwie wcześniejsze próby 100×100 zakończyły się błędem kolejności kroków operatora (wniosek z logów). Inne wersje OTBM i zewnętrzni czytelnicy nie były sprawdzane.

## 2026-11-06 — G1: diagnoza SetFocus w przebiegu odmów i zatwierdzenie progów 100×100

Zatwierdzono progi 100×100 (wartość Preview ≤ 250 ms zaproponowana, bez osobnego potwierdzenia). Win32 `SetFocus` daje błąd 87, gdy cel lub przodek jest wyłączony (test osobny). `ProceduralFocusTrace` zapisuje stan okien; przebieg `g1-focus-state-b-20261106` pokazał wyłączoną ramkę pod modalnym dialogiem generatora, a stos wskazał `LoadVersion` → `SelectBrushInternal` wywołane z kroku próbnika odmów (`loadClient`). Dla stosów aktywacji i usuwania karty wyłączone okno pozostaje hipotezą. Nie sprawdzono powrotu fokusu po zamknięciu dialogu ani przebiegu odmów bez kroku `loadClient`.

## 2026-11-06 — G1: pomiar Preview 100×100 na buildzie etapowym

`--resources` (3 × 100×100, 3 × 500×500, natywne wejście) przeszedł i został zweryfikowany (`PASS_G1_GUI_RESOURCE_BASELINE_ONLY`). Preview 100×100: ok. 110 ms, przerwa GUI ≤ 53 ms, przyrost WS ≤ 3,6 MiB, w progach wstępnych. Zmieniono `g1_verify_dialog_probe.py`: tryb `--resources` jest traktowany jak build etapowy, a stara gałąź porównania ze starym buildem usunięta. **G1 IN PROGRESS.**

## 2026-11-06 — G1: powtórzenia trendu pamięci (3 × 30 cykli)

Nachylenia powtarzalne: WS 0,77–0,79, private 0,90–0,95 MiB/cykl (limit 1,0), łączny wzrost ≈ 22 MiB, malejący z czasem. Wg ustalonego kryterium jedno spełnienie i dwie porażki z powodu składnika GDI/USER (pojedyncza próbka vs cykl 2; GDI oscyluje o 12 obiektów, nie rośnie). Kryterium nie zmieniono. Przyczyna oscylacji i skoków pamięci niezbadana. **G1 IN PROGRESS.**

## 2026-11-06 — G1: samosterowany próbnik trendu pamięci

Dodano `test_procedural_dialog_leak.cpp` i `g1_run_leak_trend.py` (kolejkowane zdarzenia menu i przycisków, nie natywne wejście). 10 cykli 500×500 nie spełnia kryterium (WS +1,55, private +1,99 MiB/cykl), 30 cykli spełnia je na granicy (WS +0,79, private +0,95; limit 1,0). Wzrost jest skokowy i w cyklach 21–30 ujemny; uchwyty, GDI i USER stałe. Wynik zależy od liczby cykli i wymaga powtórzenia. **G1 IN PROGRESS.**

## 2026-11-06 — G1: anulowanie Preview 500×500 i trzy cykle zasobów

`--cancel-preview` przeszedł w całości (`PASS_G1_GUI_IN_FLIGHT_CANCEL_ONLY`) po pięciu nieudanych próbach z przyczyn operatorskich. Preview 0,76–1,74 s, przerwa GUI ≤ 154 ms, przyrost WS ≈ 85 MiB: w progach wstępnych. Pamięć po Cancel rośnie ≈ 1 MB WS na cykl, a próg „bez trendu w 10 cyklach” wymaga 10 cykli, więc jest nieoceniony. **G1 IN PROGRESS.**

## 2026-11-06 — G1: log Cancel potwierdzony, pełny przebieg anulowania nie

W `--cancel-preview` anulowanie Preview 500×500 w locie przeszło (`RESOURCE_IN_FLIGHT_CANCEL_PASS`, log „Preview cancelled”), ale przebieg jako całość padł na kroku retry z powodu pomyłki operatora (`Cancel requires observed Validate`); trzy cykle zasobów nie zostały wykonane. Trzy wcześniejsze próby padły przez zbyt krótkie okno czasowe lub zamknięcie dialogu Esc. Kryterium logów operacji jest potwierdzone dla wszystkich ścieżek dialogu. **G1 IN PROGRESS.**

## 2026-11-06 — G1: pomiar 100×100 na obecnym buildzie i logi Place/Validate

Jedna próba `--roundtrip --roundtrip-timing` (100×100): Place 63 ms, Undo 26 ms, Redo 20 ms, Save 86 ms; maks. przerwa GUI 90 ms; przyrost łącznie 4,4 MiB working set i 5,4 MiB private. Verifier `PARTIAL…FOCUS_WARNING`. Log potwierdza „Preview validated” i „placed 10000 tiles as one undo step”. To pojedyncza próba względem wstępnych progów, nie seria; Preview 100×100 i logi Cancel nadal bez dowodu. **G1 IN PROGRESS.**

## 2026-11-06 — G1: ponowienie odmów Preview i weryfikacja logów

`--refusals` na obecnym drzewie przeszło funkcjonalnie (sześć przypadków, `REFUSAL_PASS`; verifier `PARTIAL_G1_GUI_PREVIEW_REFUSALS_FOCUS_WARNING`, jedyne ostrzeżenie to znany `SetFocus` 0x57). Dwie wcześniejsze porażki wynikały z kliknięcia w mapę, które kończy próbnik przez `_Exit(8)`; przyczynę wywnioskowano z logu, nie z kodu. Log dialogu potwierdzono w runtime dla odmów i Preview, nie dla Validate, Cancel i Place. **G1 IN PROGRESS.**

## 2026-11-06 — G1: logi operacji dialogu generatora

`TerrainGeneratorDialog::SetStatus` zapisuje każdy komunikat statusu przez spdlog (info/warn dla błędów), a udane Place loguje liczbę kafli. Pokrywa to Preview gotowe, Validate, Cancel, odmowy i Place. Kanoniczny build przeszedł (exit 0; Ninja z `C:\repo\vcpkg\downloads\tools`, bo nie jest na PATH tej powłoki). **Nie uruchomiono RME ani nie sprawdzono treści logu w runtime**, więc kryterium logów pozostaje niezweryfikowane. CTest na aktualnym drzewie: 7/7 PASS (w tym `test_procedural_core`). **G1 IN PROGRESS**.

## 2026-11-05 — G1: pierwszy pomiar Place, Undo, Redo i Save dla 100×100

Claude Code rozszerzył harness w osobnym worktree. Po integracji natywny przebieg na EXE `15c25557…` zakończył się kodem 0, a niezależny weryfikator zwrócił **PARTIAL_G1_GUI_ROUNDTRIP_TIMING_FOCUS_WARNING**. Place zaobserwowano po 84,34 ms. Maksymalny odstęp timera GUI wyniósł 113,71 ms, a przyrost working set i private commit odpowiednio 6,09 i 7,34 MiB. Trzy zachowane próby 500×500 zweryfikowano ponownie z archiwalnymi źródłami próbnika i runnera. [Wyniki, CSV i zrzut](procedural/G1_ROUNDTRIP.md#pomiar-place-undo-redo-i-save-dla-100100). **G1 IN PROGRESS**.

## 2026-11-05 — G1: przegląd kryteriów po próbach 500×500

Przegląd Claude Code przez lokalny bridge i niezależne porównanie manifestów potwierdziły brak aktualnego pomiaru zasobów dla 100×100. Tryb pomiaru Place, Undo, Redo i Save jest ograniczony do 500×500. Próby odmów Preview i unieważnienia mają dowód na buildzie sprzed zmiany preflightu. Dodatkowa kontrola SHA‑256 potwierdziła zgodność OTBM i pięciu companionów XML ze wszystkich prób czasowych z zachowanym pełnym round-tripem 500×500. [Macierz kryteriów i kolejność prac](procedural/G1_GATE_STATUS.md). **G1 IN PROGRESS**.

## 2026-11-05 — G1: trzy pomiary Place, Undo, Redo i Save dla 500×500

Dwa nowe natywne przebiegi na syntetycznych OTBM 5 i prywatnych profilach potwierdziły powtarzalność na tym samym buildzie. Niezależny weryfikator dla wszystkich trzech zwrócił **PARTIAL_G1_GUI_ROUNDTRIP_TIMING_FOCUS_WARNING**. Place zaobserwowano po 403,4–419,4 ms. Maksymalny odstęp timera GUI w oknie Place wyniósł 428,1–544,4 ms, a szczytowe przyrosty working set i private commit wyniosły odpowiednio 110,23–110,56 i 114,82–115,25 MiB. [Raport, CSV i zrzut z RME](procedural/G1_PLACE_TIMING_REPEATABILITY.md). **G1 IN PROGRESS**. Ocena bramki zasobów czeka na uzgodnione progi.

## 2026-11-05 — G1: niezależna weryfikacja pomiaru 500×500

Tryb `--roundtrip-timing` sprawdził cztery wiersze CSV względem logu GUI oraz potwierdził hashe aktualnego buildu. Wynik **PARTIAL_G1_GUI_ROUNDTRIP_TIMING_FOCUS_WARNING** zachowuje znane ostrzeżenie. Próby na kopiach potwierdziły odrzucanie uszkodzonych pomiarów. [Raport](procedural/G1_ROUNDTRIP.md). Pełne **G1 IN PROGRESS**.

## 2026-11-05 — G1: powtarzalna weryfikacja ochrony wewnątrz maski

Tryb `--protected-selection` potwierdził zachowany przebieg GUI, 421 źródeł i 421 obiektów oraz markery UID 32101. Odrzucił kopie z brakującym, błędnym lub powtórzonym markerem oraz zmienionym EXE próbnika. Wynik **PASS_G1_GUI_PROTECTED_SELECTION_ONLY**. [Odtworzenie](procedural/G1_DIALOG_MATRIX.md). Pełne **G1 IN PROGRESS**.

## 2026-11-05 — G1: chroniony kafel wewnątrz dokładnej maski

Natywne RME pokazało pięć zaznaczonych pól i cztery zmiany. Kafel (203,203,7) zachował UID 32101, zawartość, tożsamość kafla i gruntu oraz członkostwo w zaznaczeniu po Place, Undo i Redo. Odmowa nieaktualnego zaznaczenia również przeszła. **PASS_G1_GUI_PROTECTED_SELECTION_ONLY**, kod 0, pusty stderr. [Raport próby](procedural/G1_DIALOG_MATRIX.md). Pełne **G1 IN PROGRESS**.

## 2026-11-05 — G1: zakres dowodu ochrony w teście zaznaczenia

Przegląd Claude w trybie tylko do odczytu oraz niezależna kontrola kodu i logu potwierdziły Place, Undo i Redo dla pięciu pustych pól zaznaczenia w natywnym GUI. Otwór (202,202,7) jest poza dokładną maską, a chroniony sentinel (900,900,7) leży poza całym obszarem. Dowód GUI ochrony pola należącego do maski pozostaje otwarty. W proponowanej próbie pole (203,203,7) należy nadal do pięciu zaznaczonych pól, ale ma grunt i unikatowy ID. Preview powinno zgłosić cztery zmiany, a kontrole po Place, Undo i Redo porównać zawartość, tożsamość kafla i członkostwo w zaznaczeniu. Pole (201,201,7) nadal służy do próby odrzucenia nieaktualnego zaznaczenia. Zob. [macierz GUI](procedural/G1_DIALOG_MATRIX.md). Pełne **G1 IN PROGRESS**.

## 2026-11-05 — G1: pierwszy pomiar Place, Undo, Redo i Save dla 500×500

Prywatny próbnik natywnego RME zarejestrował cztery operacje na syntetycznej mapie. Place trwało 413,8 ms do obserwacji zmiany historii. Największy odstęp timera wyniósł 544,4 ms, szczytowy przyrost working set 110,23 MiB, a private commit 114,82 MiB. Build kanoniczny i runner zakończyły się kodem 0. Jedno odłożone ostrzeżenie `SetFocus`, status **PARTIAL_G1_GUI_TIMING_FOCUS_WARNING**. Jeden przebieg przy limicie historii 512 MiB w prywatnym profilu. [Metoda, wyniki i odtworzenie](procedural/G1_ROUNDTRIP.md#pomiar-place-undo-redo-i-save-dla-500500). Pełne **G1 IN PROGRESS**.


## 2026-11-04 — G1: round-trip 100×100 na tym samym buildzie co 500×500

Natywne Preview/Validate/Place/Undo/Redo/Save/Close/Open przeszły. RME odczytał 10 005 kafli, a porównania potwierdziły 10 000 wygenerowanych kafli, pięć kafli fixture i pięć XML. Hashe plików pokrywają się z poprzednią próbą 100×100. Kod wyjścia 0, jedno odłożone ostrzeżenie `SetFocus`, status **PARTIAL_G1_GUI_OTBM5_ROUNDTRIP_FOCUS_WARNING**. [Raport obu rozmiarów](procedural/G1_ROUNDTRIP.md). Pełne **G1 IN PROGRESS**.

## 2026-11-04 — G1: powtórzony round-trip 500×500 na aktualnym buildzie

Natywne Place/Undo/Redo/Save/Close/Open przeszły po poprawce kontroli wstępnej. RME odczytał i wyświetlił teren. Porównania potwierdziły 250 000 wygenerowanych kafli, pięć kafli fixture oraz pięć XML. OTBM i XML mają identyczne hashe jak poprzednia próba 500×500. Kod wyjścia 0, jedno odłożone ostrzeżenie `SetFocus`, status **PARTIAL_G1_GUI_OTBM5_ROUNDTRIP_FOCUS_WARNING**. [Raport i zrzut](procedural/G1_ROUNDTRIP.md). Pełne **G1 IN PROGRESS**.

## 2026-11-04 — G1: ponowny pomiar Cancel po poprawce kontroli budżetu

**PASS_G1_GUI_IN_FLIGHT_CANCEL_ONLY** na aktualnym EXE. Cancel przerwał Preview 500×500, ponowienie z seedem 1728 przeszło Validate, a mapa i historia zachowały stan. Trzy pełne cykle dały maksymalną przerwę GUI 168,1 ms, czas do podglądu 0,90–1,28 s i przyrost working set 79,6–80,9 MiB. Proces zwrócił 0 z pustym stderr. [Raport, CSV i zrzut](procedural/G1_CANCEL.md). Pełne **G1 IN PROGRESS**.

## 2026-11-04 — G1: kontrola budżetu przed alokacją zadania Preview

Powtórzenie rdzenia wykryło alokację ramki korutyny przed odmową za małego budżetu. Kontrole wstępne przeniesiono przed utworzenie zadania. Po poprawce build przeszedł, fingerprinty 100×100 i 500×500 są zgodne z historycznymi wynikami, a 80 prób awarii sandboxa, 108 przygotowania, 112 commitu i 15 rezerwacji przeszło. Zwykłe RME odmówiło obszaru poza mapą i wykonało Preview po poprawieniu współrzędnej. Próba rdzenia zwróciła 0 i zachowuje status **PARTIAL_G1_STAGED_CORE_FOCUS_WARNING** z powodu odłożonego ostrzeżenia `SetFocus`. [Raport i zestawienie dowodów](procedural/G1_STAGED_CORE.md). Pełne **G1 IN PROGRESS**.

## 2026-11-04 — G1: pełna ścieżka zapisu i odczytu dla 500×500

Natywne Preview/Validate/Place/Undo/Redo/Save/Close/Open przeszły funkcjonalnie. Place zajęło jeden krok historii przy prywatnym limicie Undo 512 MiB. RME odczytał 250 005 kafli, a porównanie potwierdziło 250 000 wygenerowanych kafli, pięć kafli fixture i pięć XML. Proces zwrócił 0 z dwoma odnotowanymi ostrzeżeniami `SetFocus`, więc przebieg zachowuje status **PARTIAL_G1_GUI_OTBM5_ROUNDTRIP_FOCUS_WARNING**. [Raport i zrzut](procedural/G1_ROUNDTRIP.md). Pełne **G1 IN PROGRESS**.

## 2026-11-04 — G1: stosy wywołań ostrzeżenia SetFocus

Prywatny przebieg diagnostyczny z symbolami zarejestrował dwa wywołania. wxWidgets przywracał fokus przy aktywacji okna w trakcie modalnego dialogu i wybierał pozostałą kartę przy zamykaniu aplikacji. Żaden stos nie przechodził przez kroki przygotowania Preview. Markery unieważnienia przeszły, ale runner przekroczył czas oczekiwania przed ręcznym zamknięciem, więc nie ma końcowej weryfikacji tego przebiegu. [Raport i ścieżki stosów](procedural/G1_FOCUS_TRACE.md). Pełne **G1 IN PROGRESS**.

## 2026-11-04 — G1: zmiana seeda i zamknięcie podczas Preview

**PARTIAL_G1_GUI_PREVIEW_INVALIDATION_FOCUS_WARNING**. Natywna edycja seeda i Alt+F4 unieważniły aktywne Preview 500×500. W obu przypadkach obserwator przez dwie sekundy nie zobaczył starego wyniku. Ponowienie z seedem 1728 i podgląd w nowym oknie działały, a mapa i historia zachowały stan. Build i weryfikacja plików przeszły. Proces zakończył się kodem 0 z jednym ostrzeżeniem `SetFocus`, także bez otwierania OTBM. [Raport i zrzut](procedural/G1_INVALIDATION.md). Pełne **G1 IN PROGRESS**.

## 2026-11-04 — G1: kontrola ponownego odczytu po zamknięciu komunikatu

Powtórzono round-trip OTBM 5. Obserwator zaczekał na powrót do głównego okna i pozostawił mapę do ręcznej inspekcji. RME wyświetlił ponownie odczytany teren w rendererze sprite’ów. Porównania 10 000 wygenerowanych kafli, pięciu kafli fixture i pięciu XML przeszły. Ostrzeżenie `SetFocus` pojawiło się jeszcze podczas pytania o import kreatury, przed zakończeniem obserwacji i ręcznym zamknięciem procesu. Status **PARTIAL_G1_GUI_OTBM5_ROUNDTRIP_FOCUS_WARNING**. [Raport i zrzut z renderera](procedural/G1_ROUNDTRIP.md).

## 2026-11-04 — G1: zapis i ponowne otwarcie wygenerowanego OTBM 5

**PARTIAL_G1_GUI_OTBM5_ROUNDTRIP_FOCUS_WARNING**. Natywne Place/Undo/Redo potwierdziły jeden krok historii i zgodność 10 000 wygenerowanych kafli. Po Save → Close → Open RME odczytał ten sam teren, a niezależny verifier potwierdził pięć nienaruszonych kafli fixture i zgodność pięciu plików XML. Build przeszedł, proces zakończył się kodem 0 z jednym ostrzeżeniem wxWidgets `SetFocus`. [Raport i zrzut](procedural/G1_ROUNDTRIP.md).

Pełne **G1 IN PROGRESS**. G2 pozostaje nierozpoczęte.

## 2026-11-04 — G1: odmowy Preview w natywnym dialogu

**PARTIAL_G1_GUI_PREVIEW_REFUSALS_FOCUS_WARNING**. Natywne Preview odmówiło przy wielopiętrowym zaznaczeniu, zajętej sesji zaznaczenia, nieuruchomionym hoście live, wstrzykniętym błędzie alokacji halo oraz braku klienta. Po przywróceniu klienta podgląd 100×100 działał w tym samym dialogu. Mapa i historia zachowały stan. Build i kontrola 421 obiektów przeszły. Przebieg zakończył się kodem 0 z jednym ostrzeżeniem wxWidgets `SetFocus`. [Raport, zrzut i odtworzenie](procedural/G1_REFUSALS.md).

Pełne **G1 IN PROGRESS**. Kolejny krok obejmuje generatorowy zapis i ponowne otwarcie OTBM z companionami. G2 pozostaje nierozpoczęte.

## 2026-11-04 — G1: anulowanie podczas przygotowania Preview

**PASS_G1_GUI_IN_FLIGHT_CANCEL_ONLY**. Dialog wykonuje przygotowanie porcjami na głównym wątku. Natywny Cancel podczas Preview 500×500 zachował mapę i historię. Ponowienie z seedem 1728 przeszło Preview i Validate. W trzech pełnych cyklach największa przerwa GUI wyniosła 180,9 ms wobec 546,1 ms w bazie. Przyrost working set wyniósł 79,9–81,3 MiB, a czas do gotowego podglądu wzrósł do 0,99–1,57 s. Kanoniczny build Windows x64 i niezależny verifier przeszły. [Raport, CSV i zrzuty](procedural/G1_CANCEL.md).

Pełne **G1 IN PROGRESS**. W tym przebiegu nie powtórzono Place/Undo/Redo i nie wykonano pełnej macierzy odmów ani generatorowego round-trip OTBM. G2 pozostaje nierozpoczęte.

## 2026-11-03 — G1: bazowy pomiar RSS i blokowania GUI

**PASS_G1_GUI_RESOURCE_BASELINE_ONLY**, pełne **G1 IN PROGRESS**. [Raport, CSV i screeny](procedural/G1_RESOURCES.md). Trzy rzeczywiste cykle Preview/Validate/Cancel dla 100×100 i trzy dla 500×500, bez drugiego dużego wyniku w obserwatorze. Preview 500×500 do obserwacji READY: 523,5–528,4 ms; przerwy timera GUI: 536,5–546,1 ms; szczyt WS całego procesu 805,6 MiB, przyrost 79,3–80,5 MiB. Po dużych Cancel WS 724,5/724,9/728,8 MiB; trzy cykle nie rozstrzygają braku wycieków. Wszystkie sześć cykli zachowało mapę, sentinel, rewizję i pustą historię. Normalny shutdown/exit 0/pusty stderr/assets bez zmian; 421 obiektów i hash CSV zweryfikowane. Produkcja i normalny EXE pozostały niezmienione; Codex bez nowego Claude.

**Nie wdrożono ani nie zaliczono Cancel w trakcie generowania.** Pomiar obejmuje Cancel po gotowym Preview i Validate. Budżet estymatora 512 MiB nie jest limitem RSS procesu. Następny punkt: etapowanie/anulowanie i odrzucanie opóźnionych wyników; dalej odmowy GUI i końcowy OTBM/companion round-trip. G2 nierozpoczęte, starsze audyty zachowane, bez dostępu do map/świata/DB/Dockera użytkownika.


## 2026-11-03 — G1: wskazówki Undo i czytelne małe zaznaczenie

Zakończono poprawkę UX: komunikat odmowy podaje bieżące limity i drogę do Preferences, tryb selection ukrywa pola prostokąta i pokazuje przechwycony zakres/liczbę pól, minimapa jest wycentrowana przez jawny prostokąt `StretchBlit`. [Raport, screeny i hashe](procedural/G1_DIALOG_UX.md). Bez nowego uruchomienia Claude. Końcowy build Windows exit 0; `g1-dialog-centering-rme-20261103` **PASS_G1_NATIVE_DIALOG_SELECTION_UX_ONLY**, normalny shutdown/pusty stderr/assets bez zmian i 421 obiektów zweryfikowanych. Sprawdzono opis, ukryte pola, centrowanie/resize oraz stale refusal/Place/Undo/Redo pięciu pól z otworami.

Pełna macierz 500×500 + selection oraz odmowa 40 MiB/ponowienie 512 MiB przeszła wcześniej w tym kroku (`g1-dialog-polish-retry-rme-20261103`, **PASS_G1_NATIVE_DIALOG_UX_MATRIX_ONLY**), przed końcową zmianą samego paint. **Nie powtarzano tej macierzy na końcowym EXE**; pierwszy nieudany test porównania zawiniętego komunikatu zachowano jako FAIL. Pełne **G1 IN PROGRESS**. Następny element: responsywne anulowanie/harmonogram i RSS; dalej pozostałe odmowy GUI oraz końcowy generatorowy OTBM/companion round-trip. G2 nierozpoczęte; wcześniejszych audytów nie restartowano, wspólne zmiany i dane użytkownika zachowane.


## 2026-11-03 — G1: GUI 500×500 i dokładne zaznaczenie

**PASS_G1_NATIVE_DIALOG_MATRIX_ONLY**, pełne **G1 IN PROGRESS**. [Raport i screeny](procedural/G1_DIALOG_MATRIX.md). Rzeczywiste Preview/Validate/Place/Undo/Redo dla 250 000 pól oraz pięciu unflagged members w masce 4×4 z otworami; porównano cały kontekst i sentinel, Undo odtworzył tożsamość kafli i membership. Odmowy pustego selection, brusha, zerowych wag, bounds i starego membership zachowały źródło/historię. Przy Undo 40 MiB Place 500×500 poprawnie odmówił; w tej samej prywatnej instancji ustawiono 512 MiB przez Preferences i ponowiono z sukcesem. Zwykłe źródła/EXE bez zmian; aktualny build exit 0, probe exit 0/normalny shutdown/pusty stderr/assets bez zmian, 421 obiektów RME zweryfikowano. Codex wykonał krok bez nowego uruchomienia Claude.

Otwarte: pozostałe odmowy/awarie GUI, doprecyzowanie komunikatu Undo i prezentacji małej maski, responsywne anulowanie/RSS oraz końcowy generatorowy GUI/OTBM round-trip. G2 nierozpoczęte; wspólne zmiany i dane użytkownika zachowane. Następne 3 zadania podano w raporcie.


## 2026-11-03 — G1: natywny dialog i rzeczywiste GUI 100×100

**PASS_G1_NATIVE_DIALOG_100_ONLY**, pełne **G1 IN PROGRESS**. [Raport i screen](procedural/G1_DIALOG.md). Map → Generate terrain…: Preview/Validate/Cancel i Place jako jedna akcja Undo; podgląd jest minimapą konkretnych kafli. Kanoniczny build exit 0; `g1-dialog-final-rme-20261103` PASS/exit 0/normalny shutdown/pusty stderr/assets bez zmian. Niezależny obserwator sprawdził brak mutacji przed Place, błędny seed i invalidację, Cancel oraz rzeczywiste Place/Undo/Redo z porównaniem 10 000 kafli. Verifier potwierdził 421 aktualnych obiektów RME. Claude implementował dialog i wykonał read-only review; Codex poprawił układ/etykiety oraz wykonał testy. Pierwszą niepełną inspekcję zachowano jako FAIL, bez zaliczenia.

Otwarte: GUI selection/refusals/failures i 500×500, sprite preview, responsywne anulowanie/harmonogram, RSS/inne wyniki oraz generatorowy GUI/OTBM round-trip. G2 nierozpoczęte; wspólne zmiany i dane użytkownika zachowane, bez commit/push.


## G1 — atomowy commit i Undo/Redo, 2026-11-03

**PASS_G1_ATOMIC_TERRAIN_HISTORY_CORE_ONLY**, pełne **G1 IN PROGRESS**. [G1_COMMIT.md](procedural/G1_COMMIT.md). Nowy kanoniczny Windows x64 build exit 0; `g1-commit-complete-rme-20261103`: exit 0, normalny shutdown, pusty stderr i niezmienione curated assets. Weryfikator potwierdził 421 aktualnych obiektów, 877 bazowych źródeł i 22 zamierzonych różnic. Claude wykonał rezerwację mapy i review, Codex integrację oraz niezależne testy. Współdzielone zmiany G0/Claude zachowano; bez commit/push i bez dostępu do map/world/DB/Dockera użytkownika.

Commit kopiuje konkretny diff bez RNG/autobordera do osobnej Action/Change/ActionQueue; wszystkie alokacje przed swapem i publikacją. Undo/Redo wymieniają pełny stan i actual selection membership niezależnie od flag, także null-source Redo i późniejsze zwykłe Selection Undo. Przeszło 112 faultów commitu, 15 rezerwacji, rozmiary 100/500, exact/short budget, stale/busy/live/Lua/thread/cancel, minimap i Selection preflight faults/retry, callback Lua i równoliczne zmiany membership. Bieżący pełny wcześniejszy probe również PASS. Osobna świeża regresja kolejki G0: PASS funkcjonalny; stare ograniczenia/ostrzeżenia GUI opisano w raporcie.

Otwarte: GUI dialog/renderowane preview i Commit/Cancel, responsywne planowanie/RSS/inne jobs oraz generator GUI/OTBM round-trip. Nie utożsamiać rdzenia historii z całą bramką. Następne 3 zadania: (1) minimalny dialog G1 z aktualnym request/mask/preview, (2) GUI Commit/Cancel i ograniczenie równoległych wyników, (3) pełny GUI/OTBM round-trip i domknięcie macierzy G1. G2 nierozpoczęte.

## 2026-11-03 — G1: bieżący cel offline i życie karty

**PASS_G1_CURRENT_OFFLINE_TARGET_ONLY; pełne G1 IN PROGRESS.** [G1_TARGET.md](procedural/G1_TARGET.md). Nowy guard pobiera bieżący Editor na nowo, porównuje istniejącą mapGeneration, odmawia live i busy/deferred selection, po czym składa dotychczasowe walidacje. Wariant zaznaczenia sprawdza też rzeczywiste membership. Zwrócony pointer jest pożyczony do natychmiastowej kontynuacji, bez zachowanego prawa do późniejszego commitu.

Claude wykonał dwa pliki i poprawkę dokumentacji; Codex przejrzał, zintegrował i niezależnie przetestował. Windows x64 build exit 0, EXE `8282a1c7…`. `g1-target-rme-20261103`: PASS/exit 0/pusty stderr/normalny shutdown/assets bez zmian, 420 aktualnych obiektów, 877 źródeł bazy G0 i dziesięć oczekiwanych różnic G1. Guard 100×100/500×500 i selected 500×500/4×4 bez alokacji; 22 odmowy, przełączanie map, współdzielone widoki, zamknięcie ostatniej karty i nowa mapa, oba niepołączone stany live. Bez connect/bind/accept/usługi sieciowej. Pełna poprzednia regresja również PASS. Startup warnings obsłużono w tej samej prywatnej instancji przez computer-use.

Otwarte: atomowy commit/awarie/UndoRedo/membership pustych pól, reentrancy i aktywna transakcja Lua/historii, osobna historia ręcznej edycji, GUI dialog/preview/Cancel/harmonogram, RAM/RSS/Undo/inne wyniki i GUI/OTBM round-trip. G2 nierozpoczęte; wspólne zmiany i dane użytkownika zachowane, bez commit/push.

## 2026-11-03 — G1: niezależne zmiany katalogu assets

**PASS_G1_GENERATION_ASSET_CATALOG_ONLY; pełne G1 IN PROGRESS.** [G1_ASSETS.md](procedural/G1_ASSETS.md). Nowy odczyt bieżących wartości wykrywa zmiany definicji/ground brushy/borderów i metadanych sprite także przez zachowane mutowalne referencje. Preview przechowuje osobny process-local fingerprint; walidacja sprawdza go przed source/preview korzystającymi z cached links. Nie zmienia deterministycznych fingerprintów wyników i nie skanuje assets w renderer hot path. Claude wykonał adapter; Codex przejrzał i niezależnie zintegrował/przetestował.

Windows x64 build exit 0, EXE `e17b2f82…`. Pełny probe `g1-assets-rme-20261103`: PASS/exit 0/pusty stderr/normalny shutdown/assets wejściowe bez zmian, 419 aktualnych obiektów i 877 źródeł bazy G0 z dziesięcioma oczekiwanymi różnicami G1. 39 mutacji katalogu bez podbicia epoki odmówiło walidacji; po odtworzeniu wartości wynik znów przechodził. Skan miał zero alokacji. Osobne świeżo linkowane testy border-order i snapshotu również PASS, w tym 24 faultów snapshotu. Powtórzono cały probe 100×100/500×500, 79 faultów sandboxa, 106 przygotowania, maski, walidację oraz selection/Lua/UndoRedo.

Otwarte: końcowy guard edytora/offline, atomowy commit/awarie/UndoRedo/membership pustych pól, osobna historia ręcznej edycji, GUI dialog/renderowany preview/Cancel/harmonogram, RAM/RSS/Undo/inne wyniki i GUI/OTBM round-trip. Raw pixels/tekstury/GPU/cache/runtime animacji i wnętrza innych brushy są poza fingerprintem tego wycinka. G2 nierozpoczęte; dane użytkownika i wspólne zmiany zachowano, bez commit/push.

## 2026-11-02 — G1: wspólna walidacja przygotowanego wyniku

**PASS_G1_PREPARED_VALIDATION_ONLY; pełne G1 nadal IN PROGRESS.** [G1_VALIDATION.md](procedural/G1_VALIDATION.md). `validatePreparedTerrain` sprawdza config/profile/dokładną maskę, epokę i ID klienta, pełny snapshot mapy oraz integralność preview bez alokacji lub ponownej generacji. Prywatny fingerprint źródła wiąże preview z przechwyconym wejściem i konkretnym planem. Sukces jest chwilową kontrolą, nie zachowanym prawem do commit; żywe membership sprawdza się osobno. Niezależne edycje assets bez podbicia epoki nadal nie są w pełni pokryte.

Claude wykonał dwa pliki walidatora; Codex przejrzał bridge/kod, dodał powiązanie preview i niezależnie przetestował integrację. Kanoniczny Windows x64 build exit 0, EXE `47ab14b0…`. `g1-validation-rme-20261102`: PASS/exit 0/pusty stderr/normalny shutdown/assets bez zmian, 418 aktualnych obiektów, 877 źródeł bazy G0 i dziesięć oczekiwanych różnic G1. Walidacja 100×100/500×500 miała zero alokacji i niezmienione fingerprinty. Przeszło 29 przypadków odmowy, unload/reload klienta oraz pełna regresja maski/budżetu/sandboxa/selection/Lua. Startup wymagał zamknięcia informacyjnego dialogu ostrzeżeń prywatnych assets przez computer-use; kontynuowano ten sam proces, bez restartu.

Otwarte: pełne stemplowanie niezależnych mutacji assets, końcowa kontrola życia/właściwego edytora i trybu offline przy commit, atomowa transakcja/awarie/UndoRedo/membership pustych pól/osobna historia, dialog/renderowany preview, GUI Cancel/harmonogram, RSS/Undo/inne wyniki i GUI/OTBM round-trip. G2 nierozpoczęte, wspólne zmiany i dane użytkownika zachowane; bez commit/push.

## 2026-11-02 — G1: dokładna maska rzeczywistego zaznaczenia

**PASS_G1_EXACT_SELECTION_MASK_ONLY; pełne G1 nadal IN PROGRESS.** [G1_MASK.md](procedural/G1_MASK.md). Nowy adapter odczytuje faktyczne `Selection::getTiles()`, w tym puste kafle bez flagi selected, zachowuje otwory i odmawia pustej/multifloor/busy/obcej kolekcji. Wynik posiada maskę; rewalidacja bez alokacji sprawdza dokładny aktualny zestaw i minimalne granice na tej samej mapie. Nie zastępuje kontroli snapshotu/revision/assets i nie nadaje prawa do commit.

Claude wykonał dwa pliki w osobnym checkout; Codex wykrył i zlecił poprawkę kontroli zerowego paddingu, zintegrował oraz niezależnie zbudował i przetestował. Kanoniczny Windows x64 build exit 0, EXE `9b4c2334…`. `g1-exact-selection-rme-20261102`: PASS/exit 0/pusty stderr/normalny shutdown/assets bez zmian. Dwie rzeczywiste karty, maski 100×100 i 500×500 z jednym otworem: 9 999 / 249 999 członków, estymaty 10 104 / 250 104 B. Jedna alokacja capture, zero rewalidacji; dokładny konkretny diff w masce, źródło bez zmian. Przeszły stale same-bounds swap, własność, podmiana wskaźnika przy tym samym membership, limity/fault/cancel/thread/floor/identity i pełna wcześniejsza regresja budżetu/sandboxa/selection/Lua. Verifier: 417 aktualnych obiektów, 877 źródeł bazy G0, dziesięć oczekiwanych różnic G1.

Otwarte: podłączenie maski do dialogu i walidacji przy commit, membership pustych pól przy publikacji/UndoRedo, atomowy commit/awarie/osobna historia, GUI Cancel/harmonogram, RAM/RSS/Undo/inne wyniki, renderer-preview i GUI/OTBM round-trip. G2 nierozpoczęte; wspólne zmiany i dane użytkownika zachowane, bez commit/push.

## 2026-11-02 — G1: wspólny budżet przygotowania

**PASS_G1_JOINT_PREPARATION_BUDGET_ONLY; pełne G1 nadal IN PROGRESS.** [G1_BUDGET.md](procedural/G1_BUDGET.md). `prepareTerrain` łączy snapshot, semantyczny rdzeń, resolver i sandbox pod jednym konserwatywnym limitem; wynik posiada snapshot/plan/preview, a siatka semantyczna znika przed sandboxem. Sprawdzone limity komórek/halo/faktycznych zmian, stop i propagacja błędów nie publikują częściowego wyniku. Claude wykonał adapter w osobnym checkout; Codex zlecił poprawki, przejrzał i zintegrował pliki oraz niezależnie zbudował i przetestował całość.

Kanoniczny Windows x64 build exit 0 (181 kroków), EXE `a8ce2a3d…`. Czysty pełny probe `g1-preparation-visible-rme-20261102`: exit 0, pusty stderr, normalny shutdown i niezmienione assets. 100×100: estymata 7 234 198 B / szczyt śledzonych żądań new 2 923 649 B; 500×500: 168 808 358 B / 69 507 073 B. Po destrukcji zero śledzonych bajtów. Dokładny limit i limit mniejszy o bajt, zgodność niezależnej ścieżki, 106 faultów przygotowania oraz pełna poprzednia regresja sandboxa/selection/Lua przeszły. Verifier potwierdził 416 aktualnych obiektów, 877 źródeł bazy G0 i dziesięć oczekiwanych różnic G1. Pierwszy przebieg z błędem wx SetFocus w stderr zachowano jako nieczysty; runner wymaga teraz pustego stderr.

To zamyka wspólne rozliczenie przygotowania, ale nie gwarantuje RSS. Otwarte: cała aplikacja/źródło/assets/inne wyniki/przyszłe Undo, GUI Cancel/harmonogram, dokładna maska i staleness, membership pustej selection przy commit, atomowy commit/awarie/Undo/Redo, dialog/renderowany preview i GUI/OTBM round-trip. G2 nierozpoczęte; wspólne zmiany i dane użytkownika zachowano, bez commit/push.

## 2026-11-02 — G1: izolacja zaznaczenia bez odmowy całego kontekstu

**PASS_G1_PREVIEW_SELECTION_ISOLATION_ONLY; pełne G1 nadal IN PROGRESS.** [G1_SELECTION.md](procedural/G1_SELECTION.md). Wspólne `markSelectionChanged` powiadamia bieżący edytor tylko dla rzeczywistego kafla jego mapy. Kopie i inne mapy nie unieważniają cache; usunięto tymczasowe `SelectedContext` z sandboxa. Chronione zmiany zaznaczonego bordera nadal odrzucają cały kandydat jako `OutOfMask`, także przy identycznych ID.

Build Windows x64 `g1-selection-canonical-20261102.log` exit 0, EXE `7048a947…`. Pełny nowy probe `g1-selection-lua-rme-20261102` PASS/exit 0/normalny shutdown/pusty stderr/niezmienione źródłowe assets. Dwa rzeczywiste edytory, publiczny oracle cache i zdarzenia Lua, kontekst aktywnej/nieaktywnej mapy, selected-border refusal z kontrolą dodatnią oraz zwykłe zaznaczenie i setter Lua z Undo/Redo przeszły. Powtórzono rzeczywisty sandbox 100×100/500×500 i 79 faultów alokacji z obserwacją cache obu edytorów. Verifier sprawdził 415 aktualnych obiektów i 877 źródeł bazy G0: dziesięć oczekiwanych różnic G1, w tym nowy guard w TileOperations. Claude wykonał niezależne read-only review; Codex testy/integrację. Zachowano oba wcześniejsze FAIL harnessu.

Otwarte: GUI dokładnej maski i staleness, membership pustych kafli przy przyszłym commit, pełny budżet/RSS, GUI Cancel/harmonogram, atomowy commit dokładnego diffu/awarie/Undo/Redo i niezależna historia rysowania, dialog/renderowany preview oraz GUI/OTBM round-trip. Regresja zwykłych akcji nie jest testem commitu generatora. G2 nierozpoczęte, wspólne zmiany G0 i dane użytkownika zachowane; bez commitów/push.

## 2026-11-02 — G1: własny sandbox i borderowany diff

**Etap sandboxa zweryfikowany; pełne G1 nadal IN PROGRESS.** [Kontrakt, dowody i ograniczenia](procedural/G1_PREVIEW.md). DrawOperations przygotowuje własny BaseMap przez istniejący TerrainPlacement i GroundBorderCalculator; rzeczywiste zmiany poza maską/ochroną odrzucają cały kandydat, bez zmiany mapy. Claude implementował deterministyczny porządek reguł i wykonał osobny read-only review. Wykryte powiadomienia zaznaczenia z kopii zabezpieczono odmową `SelectedContext` przed aktualizacjami; kompletne wsparcie zaznaczonego kontekstu pozostaje otwarte.

Kanoniczny build po review PASS 0, EXE `fdfc4632…`. Końcowy realny probe `g1-preview-reviewed-rme-20261102` PASS/exit 0/normalny shutdown/pusty stderr: core → resolver → borderowany sandbox 100×100/500×500, pełne powtórzenia, 79 faultów alokacji, limity, Cancel w trakcie wywołania, UID/AID/PZ i out-of-mask, nieregularna maska/temple, wybrany kontekst, base-ground variant i stale assets po reloadzie. Niezależny verifier potwierdził 415 bieżących obiektów i tylko dziewięć oczekiwanych różnic wobec 877 źródeł bazy G0. Regresja snapshotu i osobny realny test nakładających się reguł borderów PASS; CTest core 1/1. Żaden wynik nie jest pełnym zaliczeniem G1.

Następne otwarte wymagania: zaznaczenie bez skutków ubocznych, pełny budżet pracy/RSS, GUI Cancel i harmonogram, stale selection/mask/relevant assets przed commit, atomowy commit dokładnego preview przez historię, Undo/Redo i błędy, osobne ręczne rysowanie, dialog/renderowany podgląd oraz GUI/OTBM round-trip 100×100/500×500. G2 nierozpoczęte. Wspólne zmiany G0 zachowane; bez commitów/push i bez ingerencji w mapy użytkownika, companions, Docker, bazę lub świat.

## 2026-11-02 — G1-B: własny snapshot regionu i halo

**G1 nadal IN PROGRESS.** Dodano pełne własne kopie tile i osobne metadane TileLocation w regionie z halo dwóch pól, estymator kopii 64-bit i porównanie wartości uwzględniające atrybuty/rekurencyjne kontenery/Creature/Spawn/opaque payloady. [Kontrakt, dowody i ograniczenia](procedural/G1_SNAPSHOT.md). Capture nie mutuje mapy i odmawia stratnej kopii przeciw bieżącym definicjom. Rewalidacja wykrywa także bezpośrednie, nieśledzone zmiany danych w kontekście. Nie jest to jeszcze sandbox/diff/commit ani kompletna ochrona przed zmianą wszystkich assets i maski dialogu.

Claude wdrożył estymator i testy w osobnym worktree (run `668dec3c…`, poprawki review `0e66a728…`). Codex odczytał raporty/diffy/nowe pliki i odrzucił kopiowanie atrybutów przed rozliczeniem oraz stand-ins aplikacji; poprawiono przed integracją. Nowe const view/getter nie zmieniają layoutu ani istniejącego Lua API. Codex wdrożył capture/equality, integrację i rozszerzone rzeczywiste testy. Worker niczego nie kompilował ani nie uruchamiał; bez odmów i commitów.

Focused build trzech CPP PASS. Pierwszy kanoniczny wrapper miał błąd kolejności VCPKG_ROOT/vcvarsall: script przełączył się na vcpkg z VS, wyczyścił własne CMakeFiles i rozpoczął niepotrzebny rebuild zależności. Zakończono wyłącznie zweryfikowane własne drzewo CMD 23128; log `g1-snapshot-canonical-20261102.log` pozostaje przerwany, bez PASS. Przywrócono C:/repo/vcpkg właściwym wrapperem; kanoniczny **PASS 0**, 414 kroków, `g1-snapshot-recover-canonical-20261102.log`. Nie edytowano produkcyjnego build_ninja ani środowiska użytkownika.

Rzeczywisty probe `g1-snapshot-rme-20261102` **PASS, normalny exit 0, stderr pusty**, bez klienta, GUI/Application::OnInit i plików mapy. Wykonano test braku alokacji estymatora, wzrostu payloadu, depth 64/65, null item, dokładnego/przekroczonego budżetu, pre-Cancel, własności halo, wykrywania UID/AID/opaque/creature saved/alternatives/spawn NPC radius/location counters i innej mapy/revision, niezgodnych definicji kopii oraz **24/24** jednorazowych błędów alokacji capture z brakiem mutacji źródła.

Snapshot rzadkiej mapy 100×100: 10816 pól kontekstu, 453760 B estymaty, 0,2661 ms; 500×500: 254016 pól, 10661760 B, 8,6326 ms. Oba zmieściły się w estymowanym budżecie snapshotu 64 MiB. To nie pełny generator, gęsta mapa, ścisły heap/RSS ani przyspieszenie. Nie wykonano trwałego OOM, praktycznie niemożliwego overflow kontenerów ani Cancel z GUI podczas pracy.

Verifier **PASS_G1_OWNED_SNAPSHOT_ONLY**, `g1-snapshot-verification-20261102.json`: aktualne 414 obiektów probe, źródła/test/EXE, właściwy vcpkg root, brak odmów workera i 877 bazowych źródeł; tylko sześć zamierzonych różnic GroundBrush/VersionManager/Creature/ItemAttributes. Normalny EXE `f6185abf…`; `git diff --check` i kontrola whitespace nowych plików PASS. Współdzielone zmiany zachowane. Wszystkie własne procesy zakończone.

Następne wymagane prace G1: placement i wspólny autoborder w odłączonym buforze DrawOperations, konkretny diff/ochrona jego zasięgu oraz pełny budżet pracy; atomowy commit/stale checks z osobnym Undo/Redo i awariami; minimalny dialog i rzeczywiste E2E/round-trip 100×100/500×500. G2 nierozpoczęte. Docker, baza, świat i zabroniona mapa/companions nietknięte; nowych commitów brak.

## 2026-11-02 — G1-B: resolver klienta i implementacja Claude

**G1 IN PROGRESS.** Dodano rzeczywisty plan Grass/Dirt/Water → nazwane brushe aktywnego klienta → deterministyczne placement IDs. [Kontrakt i testy](procedural/G1_RESOLVER.md). Resolver sprawdza klienta, input/siatkę/maski, catalog wariantów, itemy i budżet swojego payloadu; nie zmienia mapy ani globalnego brusha/RNG. Nie wykonuje jeszcze DrawOperations placement/border sandboxu ani commitu.

Claude wdrożył licznik rewizji assets w osobnym czystym worktree/gałęzi, run `e14767b5-9984-4e39-8bf7-ddd8b20b88ce`. Codex sprawdził report/stdout/stderr/status/pełny diff i niezależnie ścieżki load/unload, po czym zintegrował dwa pliki VersionManager. Worker nie wykonywał testów ani poleceń; bez odmów, commitów i innych zmian. Capture przechowuje rewizję, a resolver odmawia wejścia sprzed przeładowania tego samego klienta. Niezależne edycje assets poza VersionManager nadal wymagają kontroli przed commit.

Kanoniczny build po integracji **PASS 0**, 178 kroków, `g1-resolver-revision-canonical-20261102.log`; normalny EXE `969d4a85…`. Rzeczywisty probe **PASS, normalny exit 0, stderr pusty**, `g1-resolver-revision-rme-20261102/verification.json`: 100×100 i 500×500, powtarzalne placement IDs, maska/ochrona, UID/AID/PZ i temple bez mutacji, RNG, błędny klient/brush/item, budżet, pre-Cancel, odmowa z workera, no-op i wymuszony reload, nieudany reload, explicit/empty unload oraz odrzucenie starego wejścia. Oryginalne assets identyczne; awaria dotyczyła wyłącznie prywatnej kopii katalogu. Zamknięto wyłącznie ostrzeżenie startowe prywatnego RME przez computer-use.

Pierwszy runtime **FAIL exit 9** zachowany w `g1-resolver-rme-20261102`: wykrył zbyt ścisły warunek skumulowanych szans. Poprawiono obsługę prawidłowych wariantów o wadze zero. Pojedynczy resolver 10000/250000 pozycji: 0,2923/6,7105 ms; fingerprint 15173774556512220455 / 2039901658534265928. To nie pomiar całego generatora ani pełny G1 E2E.

Verifier `g1-resolver-verification-20261102.json` **PASS_G1_PLACEMENT_AND_ASSET_REVISION_ONLY**: aktualność 411 obiektów probe, źródeł, testu i EXE; porównanie 877 bazowych źródeł wykazało tylko cztery zamierzone różnice GroundBrush/VersionManager. `git diff --check` PASS. Współdzielone zmiany zachowano. Worktree Claude pozostaje zachowany: aplikacja odmówiła archiwizacji primary checkout i attach niemanaged źródłowego checkoutu; bez ręcznego usuwania.

Do pełnego G1 nadal wymagane snapshot tile/halo/relevant assets, DrawOperations sandbox i cały konkretny diff, pełny budżet/RAM, Cancel z GUI podczas pracy, stale map/selection/assets przed commit, atomowy commit i jego błędy, osobne Undo/Redo, minimalny dialog i E2E round-trip 100×100/500×500. G2 nierozpoczęte. Docker, baza, świat i zabroniona mapa/companions nietknięte; nowych commitów brak. Stan narzędzia celu przy końcowej kontroli: `usageLimited`, nie `complete`; nie zmniejszono zakresu ani nie zaliczono całej bramki.

## 2026-11-02 — G1-B: własne wejście i kontrolowany wybór ground

**G1 nadal IN PROGRESS.** Dodano `captureGenerationInput` na głównym wątku: własne config/extent/maski, identyfikator i rewizja wskazanej mapy, konserwatywna ochrona `empty_only`, limity payloadu i Cancel. Chronione są całe zajęte tile oraz metadata TileLocation bez Tile (spawn coverage, waypoint, temple, house exit). [Kontrakt i granice](procedural/G1_INPUT.md). To nie pełny snapshot z halo/assets ani walidacja stale preview.

`GroundBrush::getPlacementItemIdForRoll` wydziela istniejący wybór wariantu, używany również przez zwykłe draw. Jawny roll nie dotyka globalnego RNG; zachowano historyczne granice `<` i fallback. Rdzeń otrzymał bezpieczne domyślne wartości, `SemanticGrid::valid()` i Cancel także w walidacji maski. Fingerprinty algorytmu v1 nie zmieniły się.

Kanoniczny Windows x64 `build_ninja.bat` **PASS 0**, 31 kroków; `g1-input-canonical-20261102.log`, normalny EXE `76d29a1c…`. CTest rdzenia **PASS 1/1 Release**, pięć rzeczywiście przechwyconych jednorazowych błędów alokacji i dokładne retry; `g1-core-faults-tests-20261102.log`. Pełne wektory semantyczne 100×100/500×500 nadal powtarzalne; nie są E2E adaptera ani pomiarem całego RAM.

Probe na rzeczywistych obiektach RME **PASS, exit 0, stderr pusty**, `g1-input-rme-entry-20261102/verification.json`: 12 syntetycznych pozycji, dziewięć wariantów ochrony, brak mutacji/aliasowania, budżet, pre-Cancel, odmowa z workera, 10000 powtarzalnych wyborów brusha bez przesunięcia RNG. Bez klienta, otwierania map, Application::OnInit, GUI i głównej pętli. Pierwszy link miał LNK2005 przez dwa `main`; zachowany jako FAIL w `g1-input-rme-20261102`. Builder G1 użył prywatnego obiektu wejścia, bez edycji produkcyjnego Application.

Verifier **PASS_G1_INPUT_AND_VARIANT_SLICE_ONLY**, `g1-input-verification-20261102.json`: 410 obiektów probe, hashe źródeł i EXE; spośród 877 źródeł manifestu G0 różnią się wyłącznie dwa celowo zmienione pliki GroundBrush. Pozostałe współdzielone C++ zachowane. Przegląd Claude był read-only, bez narzędzi, odmów i zmian; uwagi o inicjalizacji i siatce uwzględniono. Nie zastępuje niezależnych testów.

Otwarte: pełny snapshot/halo/assets i resolver DrawOperations/autoborder, cały budżet pracy/Undo, Cancel w trakcie biegu, stale preview, atomowy commit z błędem zastosowania, oddzielne Undo/Redo, minimalny dialog i rzeczywiste E2E z round-trip 100×100/500×500. G2 nierozpoczęte. Docker, baza, świat i zabroniona mapa/companions nietknięte; bez commitów wspólnych zmian.

## 2026-11-02 — rozpoczęte G1, pierwszy pakiet G1-A

**G1 IN PROGRESS**, gałąź `feature/procedural-g1-core` kontynuuje istniejącą historię G0 z zachowaniem wszystkich wspólnych niezacommitowanych zmian. Dodano bibliotekę C++ `rme_procedural_core` i rzeczywiste testy bez GUI: [kontrakt i zakres](procedural/G1_CORE.md). Rdzeń generuje semantyczne grass/dirt/water, ma jawne Z, stabilne lokalne strumienie RNG, nieregularną maskę i ochronę, walidację granic/wag/rozmiarów/limitów, fingerprint i stop_token. Nie używa globalnego brusha, Lua, item ID ani żywej mapy.

Nowy target CTest `test_procedural_core` **PASS 1/1 w Release**, bez `assert` wyłączanych przez NDEBUG. Log: `local-data/procedural-smoke/g1-core-tests-20261102.log`; build targetu 0. Pełne wektory 100×100 i 500×500 powtórzyły się dla tego samego seeda, inny seed zmienił wynik. Maska/ochrona zachowały wykluczone pola i nie przesunęły RNG pozostałych. Sprawdzono błędne granice/Z/wagi/maski, nieprzedstawialny rozmiar, dokładny/przekroczony budżet oraz anulowanie przed startem. Fingerprint ma stałą kontrolną wyliczoną niezależnie po jawnych bajtach.

Pojedyncza generacja rdzenia: 100×100 0,1132 ms / wyjście 10048 B, 500×500 2,6514 ms / 250048 B. To czas jednej próby na tym MSVC x64, bez oceny przyspieszenia i bez pomiaru pełnego peak RAM. `CoreBudget` obejmuje wyłącznie siatkę wyjścia; budżety snapshotu/halo/resolvera/Undo nie są jeszcze wdrożone. **Te próby nie zaliczają G1 E2E.**

Pierwszy kanoniczny `build_ninja.bat` **PASS 0**, 411 kroków i link, log `g1-core-initial-build-20261102.log`. Pierwsze wywołanie omyłkowo użyło starego wrappera i nadpisało lokalny `g0-final-gate-canonical-20261102.log`; zachowana kopia tego nowego logu ma nazwę G1. Poprzedni rzeczywisty log G0 `g0-final-canonical-20261102.log` nadal zawiera `ninja: no work to do` i BUILD SUCCESSFUL; pozostałe raporty G0 i archiwalne EXE niezmienione. Dalsze buildy G1 mają własny wrapper/log. Nie odtworzono utraconego logu z domysłów.

Końcowy ponowny kanoniczny build po ostatniej zmianie rdzenia/testu również **PASS 0**, `g1-core-canonical-20261102.log`; biblioteka i EXE RME zlinkowane. `git diff --check` PASS. Ast-grep nie znalazł `std::shared_ptr` w nowym rdzeniu/teście. Wcześniejsze źródła C++ z audytu G0 pozostały niezmienione; dodano wyłącznie nowy moduł i jego targety.

Nie wykonano jeszcze: resolvera/sandboxu, pobrania ochrony ze snapshotu RME, pełnego budżetu pracy, commit/Undo/Redo, stale preview, live guard ani dialogu. Nie sprawdzono iniekcji bad_alloc, przerwania w trakcie generacji ani innych platform. Nowego GUI nie uruchamiano — pakiet nie ma jeszcze przepływu dostępnego użytkownikowi. Docker, baza, świat i zabroniona mapa/companions nietknięte; bez commitów wspólnych zmian.

Następne trzy zadania: G1-B snapshot i kontrolowany resolver przez wspólne DrawOperations/autoborder; G1-C spójność preview/atomowy commit i osobne Undo/Redo; minimalny dialog i rzeczywiste E2E 100×100/500×500 z zasobami i kompletem plików. G0 pozostaje zakończonym audytem; nie powtarzać jego zamkniętych testów.

## 2026-11-02 — FAZA 0 VERIFIED, G1 nierozpoczęte

Zakończono bramkę audytu: powtarzalny build Windows x64, opis rzeczywistych punktów integracji i round-trip syntetycznej mapy. [Raport końcowy](procedural/G0_FINAL_20261102.md) zawiera ocenę wszystkich sześciu pozycji fazy 0 oraz G0-01…09, źródła dowodów, ograniczenia i uzasadnienie wyniku odbiorcy. Nie uznano częściowych testów za pełną zgodność świata OTS.

- Kanoniczny build `build_ninja.bat` **PASS 0**, `g0-final-gate-canonical-20261102.log`, `ninja: no work to do`; EXE `e8c3041e…`. Początkowo brakowało Ninja w PATH; ustawiono istniejący katalog Ninja i VCPKG_ROOT wyłącznie w procesie. Pozostają ostrzeżenia CMake CMP0167 i nieużytej zmiennej toolchain.
- Nowy normalny GUI `g0-draw-lua-clean-20261102` **PASS**: dwa rzeczywiste pociągnięcia dried grass przez GUI, pomiędzy nimi Lua, trzy oddzielne wpisy, wszystkie trzy Undo/Redo i cleanup. Group actions true, automagic false. Zwykły exit 0, stderr pusty, bez zapisu fixture. Istniejący test z automagic true zachowuje swój osobny historyczny zakres.
- Pierwszy `g0-draw-lua-final-20261102` przeszedł historię, ale miał asercje dostępności wx podczas zamykania; zachowano go z diagnostyką, nie jako czysty shutdown. Kontrola zamknęła panel Script Manager i nie odpytywała accessibility podczas destrukcji. Nie ustalono pełnej przyczyny i nie zmieniono renderera/biblioteki/systemu.
- `g0-final-gate-complete-20261102/verification.json` **PASS_G0_AUDIT_EVIDENCE**, verifier 0: aktualność kluczowych implementacji wobec manifestów, 35+67+1 pełnych porównań snapshotów, pełny verifier 4 snapshotów/2 Save As i 4 kontroli negatywnych, nested paths i strefa. Ponownie sprawdzono 180 aktualnych źródeł/76 obiektów oraz 6 wyników BlackTek i TOML. Zakończonych runtime audytów nie restartowano, ich raportów nie nadpisano.
- G0-09 ma zakończony audyt z ograniczeniem: RME zachowuje XML spawnu, obecny odbiorca zmienia radius i pomija direction. **Nie** zaliczono pełnej semantycznej zgodności spawnów, aktywacji potworów, normalnego serwera, DB/Dockera ani kompletnego skonwertowanego świata. Ograniczenie oraz przyszły kontrakt ochrony opisano w raporcie i ARCHITECTURE.

W tej kontynuacji zmieniono dokumentację i dodano wyłącznie lokalne artefakty weryfikacji; bez edycji produkcyjnego C++, nowych commitów, operacji na Dockerze/bazie/świecie lub `forgotten.otbm` i companionach. Wspólne niezacommitowane zmiany zachowano. Wszystkie własne procesy GUI i buildu zakończone. Nie wykonano pełnej suity GUI/Lua/CTest, wszystkich klientów i wszystkich faz awarii/Cancel; nie deklaruje się pełnej atomowości, odporności na zanik zasilania ani deterministyczności generatora. Brak dalszych wymaganych prac w bramce audytu G0. G1 pozostaje TODO i nie zostało rozpoczęte.

Poniżej chronologia wcześniejszych ocen; ich FAIL/PARTIAL/IN PROGRESS są historyczne i pozostają zachowane.

## 2026-11-02 — bieżący punkt zakończony, przekazanie do nowego czatu

Na prośbę użytkownika dokończono aktualny punkt BlackTek i zakończono zakres tej sesji. Pełny stan, polecenia i granice dowodu: [HANDOFF_G0_20261102.md](procedural/HANDOFF_G0_20261102.md). **Nie oznaczono całego G0 jako ukończonego.**

- Aktualny izolowany build bez unity **PASS, exit 0**, `g0-blacktek-current-sources-20261102/build-hostx64-result.json`, źródła niezmienione; MSVC 14.44.35207 HostX64. Utworzony EXE serwera nie był uruchamiany. Zachowano nieudane próby z nieaktualnym vcxproj oraz C1060.
- Osobny aktualny IOMap/Zones probe **6/6**, runner 0; `runtime-current/verification-current.json`: **PASS_BOUNDED_CURRENT_RECEIVER**. Prawdziwe loadMap/loadHouses, wybrane dane domu/pól/nested items, PZ w runtime cache, XML spawnu i produkcyjny odczyt wygenerowanego TOML. Pięć kontroli negatywnych ma oczekiwane wyniki; dane i wszystkie artefakty zachowane.
- Końcowy verifier sprawdził 180 źródeł i 76 obiektów wobec aktualnego serwera HEAD `4f82d1b…`, manifest/EXE, źródło probe, hashe wejść/DAT/konwersji i niezależnie zawartość TOML. Dawne 6/6 pozostaje historyczne; nowe wyniki są powiązane z obecnymi źródłami.
- Różnica semantyki: nowy konwerter zachowuje Rat przy 160,160,7 i 90 s, lecz używa fixed zone 158…162/padding 2, bez legacy radius=3/direction. Nie deklaruje się ich zachowania ani działania potworów, normalnego startupu, DB, wdrożonego Dockera lub ponownego otwarcia konwertowanego OTBM jako kompletnego świata.

Końcowe normalne GUI RME ma już pełny PASS z pustym stderr (`g0-gui-single-tab-20261102`), a użytkownik potwierdził fizyczną widoczność w poprzednim przebiegu. Wszystkie prywatne procesy/okna zakończone. Docker i zabroniona mapa nietknięte, współdzielone zmiany zachowane, nowych commitów brak. Następne trzy zadania dla nowego czatu: przejrzeć końcową macierz G0 bez restartu audytu → rozstrzygnąć znaczenie nowej semantyki odbiorcy dla bramki i ochrony spawnów → zamknąć pozostałe wymagania i dopiero wtedy oznaczyć G0 jako ukończone, bez rozpoczynania G1.

## 2026-11-02 — GUI zaliczone, końcowy audyt wykrył nowsze źródła BlackTek

**G0 nadal IN PROGRESS.** Użytkownik potwierdził fizycznie okno i wiersz ID 1. `g0-gui-local-observation-20261102` dokończył rzeczywiste 0/1/0/1/0 i zwykły exit 0, jeden wx SetFocus przy dwóch pustych kartach; timeout launchera nie zakończył dziecka, a istniejący terminalny proces zweryfikowano bez restartu przez obserwator uchwytu i `--verify-existing-house-gui`. Osobny `g0-gui-single-tab-20261102` przeszedł wszystkie pięć widocznych stanów, menu Redo, zwykły exit 0, pusty stderr i cały runner 0. Kanoniczny `build_ninja.bat` ponowiony: exit 0, `ninja: no work to do`, EXE nadal `e8c3041e…`; zachowane ostrzeżenie CMake CMP0167, bez błędu kompilacji. Nie zmieniono renderera; wcześniejsze puste obrazy pozostają PARTIAL, ich przyczyna nieustalona.

Finalna kontrola tożsamości źródeł wykazała **186/187 rozbieżności** z dawnym manifestem BlackTek, przy 85/85 nadal identycznych starych obiektach. Obecny checkout serwera `4f82d1b1375f994106cc012f3058b3a248bd9a43` ma czyste `src`, 77 CPP/103 H i nowy mechanizm Zones zamiast dawnych Spawns. Dawne 6/6 jest prawidłowym wynikiem historycznego snapshotu, nie dowodem aktualnego serwera. G0-09 wymaga aktualizacji przed zamknięciem.

`g0-blacktek-current-20261102`: pierwszy nowy build ujawnił nieaktualną listę usuniętych CPP w zachowanym vcxproj. Zakończono wyłącznie zweryfikowane własne drzewo MSBuild (PID 35744), zachowano log i wynik build 1, brak binarium; źródła nie zmieniły się podczas próby. **Nie zaliczono buildu.** Nowy `g0-blacktek-current-sources-20261102` używa prywatnego override listy odpowiadającej aktualnemu `premake5.lua` (`src/**.cpp`), bez unity, własne obj/bin i manifest źródeł przed/po. Trwa kompilacja; nie uruchamiać jej ponownie po samym timeout obserwacji. Plików projektu/źródeł serwera, działającego Dockera ani świata użytkownika nie zmieniono.

Następne trzy zadania: zakończyć izolowany aktualny build → dostosować osobny probe do rzeczywistego IOMap/Houses/Zones i sprawdzić prywatne XML wraz z kontrolami negatywnymi → dokończyć końcową macierz G0. Nie rozpoczynać od nowa audytu RME i nie zmieniać wcześniejszych FAIL/PARTIAL na PASS.

## 2026-11-02 — wznowiona obserwacja, aktualny obraz House jest czytelny

Użytkownik wyjaśnił, że po poprzednim pytaniu nie widział otwartego RME; okno zostało wcześniej zamknięte, więc ta odpowiedź nie rozstrzyga dawnej prezentacji. Po poleceniu kontynuacji uruchomiono normalny kanoniczny EXE `e8c3041e…` na nowym prywatnym run `g0-gui-local-observation-20261102`, PID 31092, okno 7279474. Nowy rzeczywisty obraz CUA pokazuje nagłówek House, pustą tabelę, a po Add niebieski wiersz ID 1; widoczne są też terrain assets. Wcześniejszy objaw obecnie nie występuje, przyczyna zmiany pozostaje nieustalona. Nie zmieniono kodu renderera ani ustawień systemu.

**Próba trwa, nie PASS.** Zarejestrowano tylko 0/1; po Ctrl+N istnieją dwie puste karty (aktywna Untitled-2), nic nie zapisano. Stan z jednym domem pozostawiono do odpowiedzi na nowe pytanie o fizyczny ekran. Win32 potwierdził pełną ścieżkę i żywy proces; osobny obserwator zachowuje uchwyt aż do rzeczywistego wyjścia, także gdy launcher przekroczy limit obserwacji. Po takim timeout nie uruchamiać drugiej kopii. Następnie dokończyć Undo/menu Redo/Undo, zwykły shutdown i końcowy przegląd macierzy; przygotowano zakres ewentualnego aktualnego powtórzenia Draw/Lua/Draw. G0 nadal nieoznaczone jako ukończone. Docker, zabroniona mapa i wspólne zmiany nietknięte.

## 2026-11-02 — objaw odtworzony poza RME, końcowe GUI wymaga obserwacji lokalnej

**G0 niezaliczone.** Kolejna tura dostarczyła nowej kontroli przyczyny zamiast powtarzać audyt danych. Windows zgłasza aktywną sesję console 1, Intel UHD Graphics 32.0.101.7088, status OK, 1920×1080; hipotezy rozłączonego RDP nie potwierdzono.

Trzy niezależne kontrole mają manifesty, archiwalne źródła, logi i normalny exit 0 z pustym stderr; `wgl-control-comparison-20261102.json` oraz `verify_wgl_controls_20261102.py` wiążą ich wyniki:
- `g0-wgl-control-20261102`: zero obiektów RME, tylko wxWidgets/OpenGL 4.5. Natywny tekst widoczny, obszar GL szary; back ma trzy dokładne RGB, front czarny, swap sukces bez błędów.
- `g0-native-wgl-20261102`: sam Win32/WGL, bez wxWidgets/RME/NanoVG/assets. Intel OpenGL 4.6, taki sam wynik buforów i udany swap, przechwycony obszar biały.
- `g0-native-wgl-generic-20261102`: osobny proces/okno z programowym formatem GDI Generic 1.1; front i back mają dokładne RGB, rzeczywisty obraz CUA pokazuje czerwony/zielony/niebieski pas. Nie jest to obsługiwany renderer RME ani zastępcze zaliczenie GUI.

Objaw sprzętowej prezentacji/przechwytywania występuje zatem także poza kodem edytora i wxWidgets. **Nie ustalono, czy fizyczny obraz jest pusty, czy ginie w przechwytywaniu; nie dowiedziono usterki sterownika.** Brak odpowiedzi na pytanie o fizyczny ekran. Dalsza bezpieczna weryfikacja końcowego GUI wymaga tej obserwacji albo zmiany zewnętrznego stanu sesji graficznej. Bez niej powtarzanie tych samych uruchomień nie zamknie bramki i nie uzasadnia spekulacyjnej poprawki produkcyjnej.

Nie zmieniono sterowników, rejestru, ustawień Windows ani produkcyjnego renderera. Wszystkie prywatne okna kontrolne zamknięto zwyczajnie i potwierdzono ich brak w inventory. Działający Docker, zabroniona mapa i wspólne zmiany pozostały nietknięte. Nadal obowiązują dotychczasowe PASS danych/I/O/BlackTek i ograniczenia macierzy G0. Następne trzy zadania: lokalna obserwacja fizycznej prezentacji sprzętowej → końcowy normalny GUI na prywatnym run → przegląd i zamknięcie macierzy G0. Nie wznawiać całego audytu ani powtarzać kontroli sprzętowych bez nowej informacji.

## 2026-11-02 — diagnostyka prezentacji GUI, bez pełnego zaliczenia

**G0 nadal IN PROGRESS.** Kontynuowano istniejące wyniki bez ponownego audytu BlackTek/Lua/I/O. Zachowano wspólne zmiany i poprzednie FAIL/PARTIAL; bez commitów, operacji na Dockerze lub zabronionej mapie.

`g0-render-buffers-20261102/partial-verification.json`: osobny testowy moduł obserwował rzeczywiste Add → Undo → Redo z menu → Undo. Mapa i dokładne dane tabeli przeszły 0/1/0/1/0, pięć kolejnych paintów, rendering włączony, wcześniejszy błąd GL 0. Normalne zamknięcie kodem 0; jedno ostrzeżenie wx SetFocus. Obejrzane przednie PNG 0/1 są czarne, tylne zawierają nagłówek i wiersz. Bufor tylny po swap nie jest dowodem prezentacji. Runner prawidłowo nie zaliczył brakującej dodatniej obserwacji wizualnej. Dodatkowe Ctrl+N utworzyło drugą pustą kartę; nic nie zapisano. Moduł/EXE mają manifest i zachowaną kopię źródła. Normalny kanoniczny EXE pozostał `e8c3041e…`.

Próby `g0-gui-screenclip-20261102`, `g0-render-observer-20261102` i `g0-gui-visible-startup-20261102` zachowano jako częściowe. Ostatnia potwierdziła, że samo SW_SHOWNORMAL nie usuwa objawu; po Undo prywatny proces zakończył się normalnie kodem 0. Screenclip nie dostarczył ukończonego zrzutu regionu. Nie zastępują wcześniejszego rzeczywistego GUI PASS innym wynikiem.

`g0-render-swap-20261102/partial-verification.json`: oddzielny eksperyment 0/1/0, dodatkowy natywny SwapBuffers zwrócił 1, GetLastError 0, GL error 0. W pięciu własnych oknach canvas/parent brak WS_EX_COMPOSITED i WS_EX_LAYERED. Obejrzany dodatkowy front czarny, back z nagłówkiem/wierszem, obraz CUA nadal pusty. To nie dowodzi usterki sterownika ani przechwytywania i nie jest normalnym GUI PASS. Cofnięto dom i zamknięto zwyczajnie: child 0, stderr pusty, runner prawidłowo 1 dla niepełnego protokołu. Zapytano użytkownika o obraz fizycznego ekranu; odpowiedź nie nadeszła przed zamknięciem tej prywatnej próby. Testy GUI nie pozostały uruchomione.

Następne trzy zadania:
1. Rozstrzygnąć różnicę pomiędzy rzeczywistym ekranem a przechwytywaniem; zachowane bufory, stan GL i udany dodatkowy swap zawężają pytanie, nie uzasadniają spekulacyjnej zmiany renderera.
2. Potwierdzić końcowy protokół normalnego GUI na nowym prywatnym run; nie powtarzać całego audytu danych.
3. Dopiero po tym przejrzeć końcową macierz i oznaczyć spełnione wymagania fazy 0; nie uznawać diagnostyki za pełny GUI PASS.

## 2026-11-01 — końcowe I/O/Cancel PASS, bramka nadal otwarta przez ocenę GUI

**G0 IN PROGRESS.** W [TEST_PLAN.md](procedural/TEST_PLAN.md) jest teraz macierz sześciu pozycji fazy 0 i G0-01…09, z konkretnymi dowodami i granicami. Nie oznaczono częściowego GUI jako pełnego zaliczenia.

- Poprawiono pełne ścieżki companion w resolverze, backupie, markerze/rollbacku i odnośniku OTBM. Końcowy kanoniczny EXE `e8c3041e3e7d6b06599a2c391fcc0599cfd945ebab2583e98cd958c0b4a45cfd`, build 0. `g0-nested-companions-final-20261101/verification-final.json` PASS: relative/absolute nested XML, rzeczywisty błąd write/restore i ograniczenie zewnętrznego celu, dokładny niezależny odczyt; trzy kontrole negatywne readera PASS. Zachowano wcześniejszy rzeczywisty FAIL/PARTIAL i ograniczenie dawnego helpera, nie restartowano udanego child tylko do zmiany raportu.
- `g0-persistence-after-paths-20261101` PASS na tym samym końcowym kodzie: cztery snapshoty, dwa Save As rel/abs, waypoint BOTH, błąd writer, 402 pozycje/pełne pozostałe węzły/nested items/XML, cztery negatywne kontrole. Stderr pusty.
- `g0-save-cancel-final-20261101` PASS: testowe zdarzenie rzeczywistego przycisku Cancel po utworzeniu/skróceniu celu, nowy i istniejący zestaw, pełne przywrócenie stanu/bajtów/cleanup, reset Cancel, oba retry i odczyty RME; pełne porównanie niezależne, child/runner 0, stderr pusty. Nie ręczne kliknięcie ani wszystkie fazy.
- `g0-gui-after-paths-20261101`: normalny końcowy EXE, nowa pusta mapa, Add/Undo i zwykły exit 0, lecz pełny protokół wizualny **PARTIAL**, runner 1. Brak widocznego wiersza GL po Add, stare piksele canvas, jedno ostrzeżenie SetFocus. `startup-shutdown-verification.json` zalicza tylko start/obsługę/zamknięcie z diagnostyką; nie zastępuje pełnego GUI PASS. Kontrola `g0-gui-baseline-control-20261101` na nieprzebudowanym, wcześniej zweryfikowanym `07d87869…` pokazała ten sam objaw i exit 0. To nie ustala przyczyny.

Następny zakres jest konkretny: wyjaśnić rozbieżność renderowania/przechwytywania na prywatnym profilu, potwierdzić wymagany normalny GUI oraz dopiero wtedy zaakceptować końcową macierz. Nie rozpoczynać ponownie audytu BlackTek/Lua/I/O i nie oznaczać ich samodzielnie jako pełnego G0. Działający Docker oraz zabroniona mapa/companiony pozostały nietknięte; współdzielonych zmian nie cofano, nowych commitów brak.

## 2026-11-01 — nowe pliki po niepełnym rollbacku i ścieżki XML

**G0 nadal IN PROGRESS.** `g0-removal-denied-final-20261101` przeszedł z kodem 0 i pustym stderr: rzeczywisty writer utworzył nowe OTBM/spawn, zablokowany house XML spowodował błąd zapisu/odtworzenia, a prywatne deny DELETE/DELETE_CHILD uniemożliwiły usunięcie nowych celów. Kompletny marker V1 (9 rekordów), backup domu, oba nowe pliki i metadane edytora zachowane; następny zapis odrzucony. Guard zwolnił blokadę i usunął tylko własną odmowę ACL. Pierwszy błędnie przygotowany scenariusz zachowano jako FAIL, bez nadpisywania dowodów.

`g0-partial-rollback-crash-20261101` przeszedł ograniczony protokół przerwania/restartu: obserwacja rzeczywistego `Save failed`, zweryfikowany uchwyt prywatnego PID 39916 i oczekiwany kod 18177. Po usunięciu wyłącznie testowej odmowy ACL zwykły EXE pokazał `Incomplete save` z dwoma istniejącymi celami. Próba Ctrl+S nowej pustej mapy odmówiona z widoczną ścieżką markera. Niezależny verifier potwierdził wszystkie cztery zachowane pliki i marker bajt w bajt, brak odrzuconego celu oraz sześć niezmienionych wejść. Launcher zakończył tylko własny proces po PASS; nie był to normalny shutdown ani test zaniku zasilania. Szczegóły, hashe i komendy w [TEST_PLAN.md](procedural/TEST_PLAN.md).

Próba podkatalogu ujawniła rzeczywisty błąd `wxFileName` i niespójność ścieżek backup/writer. Resolver parsuje teraz pełną ścieżkę; backup/marker/rollback korzystają z tego samego resolvera co XML. Pierwsza regresja `g0-nested-companions-20261101` jest **PARTIAL**: względny zapis/odczyt i realny zablokowany rollback przeszły, odczyt absolutnego companion zawiódł, ponieważ OTBM zapisywał basename zamiast podkatalogu. Poprawiono również ten odnośnik, zachowując rodzaj/kolejność atrybutów. Kanoniczne buildy, końcowy nested runtime i pełny fixture po zmianie serializatora przeszły. Dokładne wyniki i ograniczenia są w aktualnej macierzy TEST_PLAN; pełna bramka pozostaje otwarta z powodu ostatniej nieukończonej oceny GUI.

## 2026-11-01 — publikacja niepustej historii i aktualne normalne GUI

**G0 nadal IN PROGRESS.** `g0_rme_queue_allocation_probe.cpp` używa testowych podklas istniejących wirtualnych fabryk (bez hooka w produkcji) do uzbrojenia scalar C++ alokatora po commit akcji, przy wejściu do publikacji batcha. Przygotowuje prawdziwą historię Lua i sprawdza tożsamości/etykiety wpisów, indeks, licznik pamięci oraz rzeczywiste Undo/Redo. Merge wymuszono oknem timestamp tylko w podklasie testowej, bez zmiany zegara systemowego.

- `g0-queue-allocation-before-20261101`: **REPRODUCED FAILURE, proces 9**. Po 14 wpisach pierwsza odmowa wzrostu deque zachowała liczbę wpisów 14/14, ale zmieniła stan kolejki; kod zwiększał licznik pamięci przed odrzuconym `push_back` (odczyt po błędzie 5850). Baseline pełnego snapshotu PASS; próba odmowy nie doszła do całej dalszej weryfikacji. Zachowany `reproduction.json`.
- `g0-queue-allocation-after-20261101`: **PARTIAL**. Wszystkie 35 przypadków C++ zakończyło asercje, ale niezależny reader odrzucił 14 snapshotów po zamierzonym przycinaniu w testach limitów. Przywracanie testowego AID przez setter 0 nie odtwarzało braku atrybutu. Zachowano `partial-verification.json`; nie poluzowano orakla. Test końcowy przywraca pełny snapshot pola przez zwykłą akcję, co odróżnia odbudowę fixture po celowej ewikcji od testowanego Undo.
- `g0-queue-allocation-final-20261101`: **PASS, kod 0 procesu/runnera**, 35 przypadków i 35 niezależnych pełnych porównań OTBM/XML, 35 kontroli `clear()`. Wzrost deque 9 przypadków/2 rzeczywiste odmowy publikacji; limit liczby wpisów 9/2; merge z Redo 8/1; limit pamięci 9/2. Razem 7 odrzuconych alokacji zachowało całe stare wpisy/etykiety/indeks/licznik i mapę; wykonano istniejące Redo i Undo. Osiem dalszych odmów GUI było raportowanych i pozostawiło kompletną cofawalną akcję. Pozostałe próby bez trafienia także przeszły.

Poprawka `ActionQueue::addBatch` najpierw planuje odrzucenie Redo i dotychczasowe przycięcie, następnie rezerwuje miejsce dla merge albo nowy slot deque. Dopiero po powodzeniu zmienia stare wpisy i licznik. `clear()` zeruje także `memory_size`. Nie zmienia on-disk ani układu Tile; **nie stanowi pełnej gwarancji wyjątków w `Action::commit` lub dla dowolnego nie-Lua wywołującego**. Zachowano dotychczasowe jednorazowe przycinanie liczby wpisów; limit ustawiony poniżej istniejącej liczby nie jest od razu ścisłym limitem. Nie deklaruje się ścisłego budżetu RAM ani wydajności; przygotowanie slotu/kontenera może przejściowo wymagać dodatkowej pamięci.

Kanoniczny build **PASS, 2/2, kod 0**, log `g0-queue-allocation-canonical-20261101.log`; normalny EXE `07d8786909395c813adcba3afc09ed8da0f468f7d12eb623f46da1350ae630af`, `action_queue.cpp` `7efc3a44fe3d5e96c3c0e18ff8e66143efa01d3a9589b3ce945166a08e97ff63`. Końcowy moduł `b011cebe54a6c0e38132c67935a2d9029a140075226d3d885688d1c6d4998c1f`, testowy EXE `fc61f91a1963b47cb26d9c9549f01786767e0d35ab80105cc84eb445b980238a`, 408 świeżych obiektów. Manifest obejmuje kolejkę i jej obiekt. Context7 Microsoft C++ Docs i lokalny nagłówek MSVC potwierdziły gwarancję wstawiania do deque; rezerwację przed merge skontrolowano przez ast-grep.

Regresje na tej samej aktualnej bazie **PASS**:

1. `g0-house-history-after-queue-20261101`: osiem znaczników konflikt/retry/tożsamość/Add/Edit/Delete/odtworzenie/cleanup/strażnik live bez nasłuchu; pełny snapshot porównany niezależnie. EXE `af625038bd61c92d61f731e1f3172b7b4369e8974b0475733f8d540e66713d37`.
2. `g0-lua-allocation-after-queue-20261101`: ponowiona pełna seria 67 przypadków, 67 porównań, 400 pól jako jedna akcja, Undo/Redo, prywatny zapis i odczyt RME oraz niezależny reader. Ten sam test źródłowy `4056f8f8…`, nowy EXE `eb11440879aa235f9864a44293449d3cc5ebc2f9f11ef6ae9d99b9477eeea13e`.
3. `g0-house-gui-after-queue-20261101`: kopia **normalnego** EXE bez modułu. Nowa pusta mapa, House 0 → Add ID 1/rent 0/size 0 → Ctrl+Z 0 → Edit/Redo 1 → Ctrl+Z 0. Obserwacje zapisane przed Alt+F4; zwykły exit **0**, cały runner PASS, stderr **pusty**, bez alertu i promptu zapisu. Tytuł nadal miał asterisk po Undo; nie wnioskowano z niego o stanie danych. Nie zapisano mapy. To ograniczony dowód normalnego GUI/shutdown na obecnym buildzie, nie test wszystkich widoków/live.

Wszystkie hashe oryginalnego fixture i kopii niezmienione. Moduł kolejki ma 36 ostrzeżeń wx `SetFocus`, jawnie zapisanych w raporcie; nie oznaczono go jako wizualny GUI PASS. Podobne ograniczenia modułu Lua nadal obowiązują. Nie zmieniano Dockera, bazy, świata użytkownika ani zabronionej mapy; wspólne zmiany zachowane. Selector walidacji RME nadal review-only, więc użyto powyższych rzeczywistych testów.

Następne 3 zadania: (1) wymusić odmowę usunięcia nowego prywatnego celu przy rollbacku zapisu i sprawdzić zachowanie V1/plików; (2) dokończyć jawne scenariusze przerwania częściowego zapisu/odzyskiwania bez dotykania świata użytkownika; (3) końcowa macierz wymagań G0-01…09 i ocena bramki. Pozostałe nieprzetestowane VM/stałe OOM/aktywne peerowanie są ograniczeniami opisanych dowodów, nie zaliczeniem ogólnej atomowości.

## 2026-11-01 — błędy alokacji commit Lua, dom i ponowiony smoke 400 pól

**G0 nadal IN PROGRESS.** Nowy osobny `g0_rme_lua_allocation_probe.cpp` wykonał produkcyjne `app.transaction` na aktywnej karcie prywatnego fixture. Jednorazowa odmowa scalar C++ `operator new` jest uzbrajana dopiero po pięciu setterach ground AID (również pole domu, zagnieżdżony kontener i piętra 6/7), przed powrotem z callbacka. Nie wyczerpuje pamięci systemu ani nie podmienia alokatora w normalnym RME.

- `g0-lua-allocation-before-20261101`: **FAIL, kod 9**. Bez iniekcji commit/Undo usuwały house ID pola 104/102/7 (wykryte dopiero przez niezależny snapshot, nie sam test AID). Przy pierwszej odmowie alokacji rollback zostawiał **401 zamiast 402 pól**, historia 0/0. Zachowane logi, oba snapshoty i `reproduction.json`; żaden z nich nie jest pełnym PASS.
- `g0-lua-allocation-after-20261101`: **PARTIAL, kod 9** po zachowaniu kopii rollbacku, house ID i dirty state. Wszystkie 58 zapisanych snapshotów przeszły niezależne pełne porównanie OTBM/XML, lecz przypadek 57 zgłosił błąd i przywrócił mapę, zachowując już opublikowaną historię 1/1. Zachowany `partial-verification.json` opisuje niespójność, nie zaliczenie serii.
- `g0-lua-allocation-final-20261101`: **PASS danych/historii opisanego zakresu**, cały proces/runner 0. **67 przypadków i 67 niezależnych pełnych porównań**; baseline ma 58 alokacji. Z 58 rzeczywistych odmów 54 zakończyły się pełnym rollbackiem, historią 0/0, przywróconym domem i niezmienionym dirty state. Cztery odmowy dotyczyły powiadomień GUI: odnotowana diagnostyka, kompletny commit pięciu zmian, jedna akcja i poprawne Undo przywracające cały fixture. Pozostałe próby bez trafienia iniekcji także wykonały pełny commit i Undo. Po każdym przypadku nowa pusta transakcja potwierdziła zakończenie poprzedniej.

Poprawki: `LuaTransaction` zachowuje własne snapshoty do przyjęcia całego batcha; do tymczasowego odtworzenia bazowego stanu używa osobnych kopii. Rollback przywraca także wcześniejszy dirty state. Odłączenie indeksu domu nie zeruje house ID w historycznym obiekcie. `Editor::notifyStateChange` odnotowuje `bad_alloc` obserwatora GUI zamiast propagować go jako błąd operacji na mapie, szczególnie po publikacji historii. Zmiany mieszczą się w istniejących metodach, bez zmiany formatu/układu Tile. **Koszt:** dodatkowa kopia istniejącego zmienionego pola w commit Lua do zakończenia publikacji; nie mierzono czasu/pamięci i nie deklaruje się przyspieszenia.

Na tych samych aktualnych obiektach ponowiono istniejące skrypty `g0_smoke.lua`/`g0_verify_saved.lua`: rzeczywisty nowy pusty edytor → 400 ground z załadowanego `grass` (4526), jedna akcja → Undo usuwa wszystkie pola i zachowuje puste halo → Redo przywraca wynik → nowy prywatny zapis przez `EditorPersistence::saveMap` i ponowny odczyt RME. Niezależny reader porównał dokładnie 400 pozycji/ground/puste stosy i metadane oraz oba puste XML. Oryginalny fixture i trzy kopie niezmienione, brak markera; hashe wykonywanych skryptów sprawdzone.

Kanoniczny build **PASS** (`g0-lua-notification-canonical-20261101.log`), normalny EXE `f274e2ca51ca78c4a871b22a86acc9d5ac65f707305079e41018d65ce34caa3b`. Osobny EXE `4554b08e33dc1e1a52e845a3701b11275cb255988c1cd86505be2b2ca1dda9dc`, 408 świeżych obiektów, moduł `4056f8f861a5736347429c4b9f3dc1581f66b84efb5236778423b5db1961e1be`. Źródła i normalny EXE zgodne z końcowym manifestem. Selector skillu dla RME nadal review-only; wykonano właściwy build i testy rzeczywistego zachowania.

**Granice:** seria zaczyna z pustą historią i sprawdza pięć istniejących ground setterów; nie obejmuje VM Lua, uporczywego/współbieżnego OOM, niepustej historii/Redo/limitów, pozostałych mutatorów i aktywnych peerów. Moduł niszczy testowe edytory, po czym używa `_Exit`; brak dowodu globalnego shutdown. Stderr zawiera cztery oczekiwane raporty awarii powiadomień oraz **69 ostrzeżeń wx `SetFocus`** przy seryjnym tworzeniu/zamykaniu kart w jednym callbacku. Przyczyny ostrzeżeń nie ustalono; nie deklaruje się czystego wizualnego GUI. Zachowano je w logu i `post-verification-diagnostics.json`, bez zmiany pierwotnego raportu funkcjonalnego. Zamknięto wyłącznie odczytany zwykły dialog ostrzeżeń materiałów prywatnego profilu. Docker i zabroniona mapa nietknięte, wspólne zmiany zachowane.

Następne 3 zadania: (1) test zachowania niepustej historii/Redo i limitów przy błędzie przyjęcia batcha; (2) pozostałe fazy przerwania/odzyskiwania zapisu oraz błąd usuwania nowego celu; (3) końcowa macierz G0-01…09 i ocena bramki na podstawie aktualnych, wystarczających dowodów. Pozostałe jawne ograniczenia powyżej nie są pełnym zaliczeniem atomowości.

## 2026-11-01 — odtworzona utrata domu przy błędzie alokacji i poprawka

Test-only `g0_rme_house_allocation_probe.cpp` zastępuje scalar `operator new` tylko w osobnym EXE i uzbraja pojedynczą odmowę w wątku testu bezpośrednio przed rzeczywistym `Houses::addHouse`. Na wcześniejszych obiektach (normalna baza `ed8c2b92…`, testowy EXE `102a637f…`) faktyczne `std::bad_alloc` wstawiania węzła zniszczyło obiekt przejęty parametrem przez wartość. `g0-house-allocation-before-20261101`: kod **9**, launcher **FAIL**, zachowane `reproduction.json` i log. Dalszych przypadków ten przebieg nie osiągnął.

Poprawka ograniczona do `Houses::addHouse` i deklaracji: parametr `unique_ptr&&`, najpierw `try_emplace` pustego węzła, potem niezgłaszające wyjątków przekazanie własności i aktualizacja metadanych/licznika. Odmowa kolizji również pozostawia obiekt u wywołującego. Nie zmienia formatu mapy ani implementacji akcji; wszystkie istniejące miejsca wywołania kompilują się. Wzorzec warunkowego przenoszenia `try_emplace` sprawdzono w Context7 Microsoft C++ Docs.

Kanoniczny `build_ninja.bat`: **PASS, 155/155, kod 0**, normalny EXE `0677c3a73e5f52a23e498f78cd37020e59cf09341bf37cbd6e297b9f6a31be3d`. `g0-house-allocation-after-20261101`: ten sam hash źródła modułu co w reprodukcji, 408 aktualnych obiektów, osobny EXE `1cb5303dfc9822b18744f93be926d69c54ad13dfb5db8fa0f72031d7fb16e799`; **PASS procesu i launchera, kod 0**. Potwierdzono faktyczny wyjątek, zachowanego właściciela/tożsamość/generację/nazwę/czynsz, pustą mapę po błędzie, nienaruszony licznik ID, udane ponowienie z tym samym obiektem i bezpieczną odmowę duplikatu. Nie zapisuje mapy. Wszystkie wejścia i kopie zachowały hashe. Manifest obejmuje też `house.cpp`, nagłówek i obiekt. Brak pełnego dowodu wyjątków w `Action::commit`, metadanych po wstawieniu i `ActionQueue`; ten wynik dotyczy wstawienia do `Houses`.

Regresja historii po poprawce **PASS**, `g0-house-history-allocation-fix-20261101`, 408 aktualnych obiektów, testowy EXE `720c55bfe6a91978a88f8abfea00199e49504b8ddea92b8c2d4db96c459ac3fc`, źródło `f1408cf54b8fc0c0118a31f96a04157e00e01e36bd40cf0f4c7f19619144db38`. Osiem znaczników: konflikt/retry Undo i Redo, odrzucenie zastępcy, pełny łańcuch Add/Edit/Delete, odtworzenie pól/wyjścia/tożsamości, czyszczenie historii oraz blokada stanu live bez otwierania sieci. Snapshot przywróconej mapy przeszedł ponowny odczyt RME i niezależne pełne porównanie OTBM/XML; wejścia zachowały hashe. Proces/launcher kod 0. Moduł czeka teraz także na koniec `OnInit` i aktywne główne okno; lokalne edytory niszczy przed `_Exit`, nie testuje globalnego shutdown. Testowe akcje nie są wykonywane w normalnej sesji użytkownika.

Decyzję C++/Lua zapisano już jako wybraną granicę odpowiedzialności, na podstawie testów noise/algo/geo, Draw/Lua/Draw oraz błędów i regresji transakcji. Oznaczono wyłącznie ten punkt planu jako wykonany. **G0 nadal IN PROGRESS**. Kolejne trzy zadania: (1) błędy alokacji pełnego commit/rollback Lua i kolejki; (2) pozostałe fazy przerwania/odzyskiwania i błąd usuwania nowego celu; (3) końcowa macierz G0-01…09 z istniejącymi dowodami. Nie zmieniano Dockera ani zabronionej mapy; wspólne niezacommitowane zmiany zachowano.

## 2026-11-01 — rotacja i metody podium Lua: PASS w prywatnym profilu

Osobny moduł `g0_rme_item_methods_probe.cpp` zlinkowano z 408 aktualnymi obiektami RME (kontrola świeżości źródeł przez builder). Normalnego EXE nie zmieniano: baza `ed8c2b92…`; testowy EXE `fd35b2522005c85611fb071cb3889d1eb1aa79316624d961bd7cd588375b07f5`, źródło modułu `cffd0188e11f4142ec8b6a3b12c12bef3a8f2229a27c5e2f203ced406c6b5348`. Artefakty: `local-data/procedural-smoke/g0-item-methods-20261101` (manifest buildu, wejść, log, obserwowany exit 0, `verification.json` PASS).

Rzeczywisty silnik Lua i aktywny edytor 402-polowego fixture wykonały trzy przypadki: `rotate` 1442→1476 według załadowanej definicji, `setPodiumOutfit(128,3)` oraz `setPodiumOutfit(129)` przy wcześniejszym kierunku 3. Każdy objął referencję pobraną przed transakcją, wymuszony błąd po zmianie i rollback bez wpisu historii, jawny commit oraz Undo/Redo, a następnie niejawny commit oraz Undo/Redo. Read-only oracle C++ porównywał wszystkie 12 pól `Outfit`, kierunek i trzy flagi podium. Ujemny lookType, kierunek -1/256 oraz metoda na zwykłym przedmiocie zostały odrzucone bez zmiany stanu/historii. Końcowe Undo usunęły oba przedmioty testowe, przywróciły indeks 0, pusty stos i 402 pola.

**Ograniczenie:** dostępny profil 1098 nie definiuje podium. Wyłącznie w nowej prywatnej kopii XML typ istniejącego 1443 ustawiono na `podium`, używając normalnego loadera i fabryki `Item::Create`. Jest to dowód bindingu/historii prawdziwego obiektu `Podium`, nie obsługi podium przez standardowy profil ani wdrożony BlackTek. Mapa nie była zapisywana; nie sprawdzano serializacji podium ani globalnego shutdown (`_Exit` po zamknięciu testowego edytora). Odczytano i zamknięto zwykły dialog ostrzeżeń materiałów prywatnego profilu. Hashe trzech źródłowych plików i ich kopii, bazowego XML oraz skryptu pozostały niezmienione; hash celowo zmienionego prywatnego XML także sprawdzono po teście. Docker i zabroniona mapa nie były używane; cudze zmiany zachowano.

**G0 nadal IN PROGRESS. Następne 3 zadania:** (1) odtworzyć błąd alokacji w ścieżce przenoszenia własności domu i w commit/rollback Lua, zapewniając zachowanie danych oraz historii; zwykły `error()` tego nie dowodzi; (2) dokończyć otwarte fazy przerwania/odzyskiwania zapisu i błąd usuwania nowego celu na osobnych fixture; (3) zestawić końcową macierz G0-01…09 z istniejącymi artefaktami i formalną decyzją C++/Lua, bez ponawiania zaliczonych prób i bez deklaracji pełnej atomowości.

## 2026-11-01 — poprawny wybór zamykanej karty i odpięcie palet: PASS

Kanoniczny build aktualnego `editor_tabs.cpp` **PASS, kod 0**. Końcowy normalny EXE SHA-256 `ed8c2b926d8c9c676d0b6e607d010708b9657cbfd2764fe0272a7aff10cf0b8d`; źródło `06ae6fe77fabc02f32f9b9833578a26d4d8e6da663b282fc4c9a31c793e9f231`. Poprawiono dwa odtworzone błędy: wybór strony z `wxAuiNotebookEvent::GetSelection()` zamiast `GetInt()` oraz odpięcie pożyczonych obiektów palet przed usunięciem karty i ponowne przypisanie do pozostałego edytora po nim. Zmiana obejmuje wyłącznie obsługę zamknięcia, bez mutacji danych/formatu mapy.

**PASS rzeczywistego GUI**, świeży `g0-house-close-final-20261101`, kopia normalnego EXE bez modułu testowego:

- Kliknięcie x drugiej czystej karty zamknęło tę kartę; pozostał `house-roundtrip.otbm` z domem ID 1/rent 4321/size 1.
- Dodanie domu w kolejnej pustej drugiej karcie oznaczyło ją jako zmienioną. Próba zamknięcia pytała o `Untitled-3.otbm`. Cancel zachowało obie karty i nowy dom. Ponowienie i Nie zamknęły wyłącznie tę drugą kartę; fixture i jego dom pozostały.
- Zamknięcie ostatniego fixture opróżniło listę domów i wybór miasta. Ponowne otwarcie tej samej prywatnej kopii pokazało dom, kolejne zamknięcie ponownie wyczyściło palety. Edit bez mapy niczego nie otworzyło i nie spowodowało awarii.
- Normalne Alt+F4: kod 0 bez alertu. Launcher zakończył się PASS, utrwalił kod wyjścia przed weryfikacją; wszystkie hashe trzech wejść i ich kopii niezmienione, brak nowej mapy/markera. Niczego nie zapisano.

Manifest, `run/house-ui-observation.json`, `observed-exit.json` i `verification.json` zawierają zakres dowodu. Wcześniejsze przebiegi PARTIAL zachowano, bez zmiany ich oceny. Selector skillu `sorky-validate-change` nie ma trasy dla RME (review-only); wykonano więc kanoniczny build i powyższy test rzeczywistego GUI. Nie testowano zamykania współdzielonych widoków tej samej mapy ani aktywnych sesji live. Nadal **G0 IN PROGRESS**: pozostałe przypadki Lua/atomowości i przerwań zapisu oraz końcowa ocena wszystkich wymagań nie są zamknięte. `forgotten.otbm`, jej companiony i uruchomiony Docker BlackTek pozostały nietknięte; cudze niezacommitowane zmiany zachowano.

## 2026-11-01 — zamknięcie niewłaściwej karty podczas regresji

Build pierwszej poprawki palet zakończył się kodem 0, EXE `d7cb77055bdeb26fa41ac19bb552670c58985c4b03dfe34d03d45eb3978f0203`. Próba `g0-house-close-fixed-20261101` potwierdziła pustą paletę i pusty wybór miasta po zamknięciu ostatniej mapy, ponowne otwarcie prywatnego fixture z domem i ponowne wyczyszczenie po zamknięciu. Kliknięcie Edit bez mapy nie otworzyło dialogu ani nie zakończyło procesu. Jednak przy dwóch kartach kliknięcie x drugiej (`Untitled-2`) zamknęło pierwszą (`house-roundtrip`). Verifier odrzucił próbę z powodu jawnej diagnostyki; raport pozostaje **PARTIAL**, zwykły exit 0.

Źródłem drugiego błędu jest `evt.GetInt()` w `OnNotebookPageClose`. Lokalny wxWidgets 3.3.1 definiuje `wxAuiNotebookEvent` jako `wxBookCtrlEvent`, którego indeks strony jest w `GetSelection()`, niezależnie od pola integer zdarzenia komendy. Zmieniono handler na ten indeks z kontrolą zakresu, włącznie z wyborem strony do pytania o zapis. Build funkcjonalnej poprawki przeszedł; po korekcie samego wcięcia trwa końcowy build aktualnego pliku. Regresja obu poprawek na tym końcowym buildzie pozostaje do wykonania. Wszystkie próby dotyczą wyłącznie syntetycznych map i prywatnych procesów.

## 2026-11-01 — GUI Remove, edycja czynszu i pozostały wskaźnik palety

Druga świeża prywatna instancja normalnego RME (`g0-house-edit-gui-20261101`, stary hash `29691821…`) powtórzyła bazowy cykl Add/Undo/Redo/Undo, następnie dodała pusty dom ID 2. Dialog odrzucił edycję przy braku miasta (`Invalid town selected`); Cancel zachowało rent 0. Przycisk Remove i rzeczywiste potwierdzenie usunęły dom; Undo przywróciło ID 2, Redo usunęło go ponownie. Następnie otwarto wyłącznie `run/house-roundtrip.otbm` — prywatną kopię syntetycznego fixture (402 pola, G0 Town, G0 House). Sama zmiana rent przez dialog dała 4321 → 4322, Undo → 4321, Redo → 4322. Zamknięcie tej karty pokazało `Save changes` z nazwą `house-roundtrip.otbm`; wybrano Nie. Zmiany wcześniejszej pustej karty również odrzucono. Żadnej mapy nie zapisano.

Po zamknięciu ostatniej karty paleta nadal wyświetlała ID 1/rent 4322/size 1. Analiza kodu potwierdziła, że pożyczone referencje były ponownie przypisywane przed `DeletePage`, a ostatnia strona nie gwarantuje zdarzenia zmiany. To **PARTIAL, nie pełny PASS GUI**; raport `extra-gui-verification.json` jawnie zachowuje błąd. Normalne Alt+F4 zakończyło proces kodem 0. Runner wcześniej przekroczył 300 s; bez restartu otworzono uchwyt Win32 nadal żywego PID 17936, sprawdzono pełną ścieżkę i zaobserwowano zakończenie. Hashe oryginalnego fixture i wszystkich trzech kopii pozostały niezmienione.

Minimalna poprawka w `MapTabbook::DeleteTab` odpina wszystkie palety przed usunięciem i odświeża je po usunięciu względem pozostałego bieżącego edytora, również przy braku mapy. Build i regresja GUI tej poprawki są w toku; starszy przebieg pozostaje dowodem błędu. Nie zmieniano plików użytkownika, istniejących zmian innych osób ani Dockera BlackTek.

## 2026-11-01 — zwykłe GUI dodania domu, Undo/Redo i zamknięcie

Stan: **G0 IN PROGRESS**. Na świeżym prywatnym profilu uruchomiono kopię normalnego RME (`29691821bfcbd40da449162ec20015adee4471f4731e4c03b730d08d63907b8a`), bez modułu testowego. Nowa pusta mapa, paleta House: lista pusta → przycisk Add pokazał ID 1/rent 0/size 0 → Ctrl+Z usunęło wiersz → Edit/Redo przywróciło ten sam wiersz → Ctrl+Z ponownie opróżniło listę. Alt+F4 zakończyło proces normalnie kodem 0, bez pytania o zapis i bez alertu wxWidgets. Mapy nie zapisano. Zwykłe ostrzeżenia materiałów przy starcie zamknięto przed testem. Początkowa próba Ctrl+Y nie była Redo; menu pokazuje Ctrl+Shift+Z, a właściwy test wykonano poleceniem z menu. Nie potwierdzono działania skrótu Redo.

Artefakty: `local-data/procedural-smoke/g0-house-gui-20261101/{gui-build-manifest.json,observed-exit.json,verification.json,run/house-ui-observation.json}`. Launcher doszedł po `process.wait` i sprawdzeniu kodu 0 do odczytu obserwacji, ale pliku jeszcze nie było (pierwsze polecenie jego zapisu wskazało niedostępny alias Python). Zapisano rzeczywiste obserwacje i jawne pochodzenie kodu wyjścia z tej ścieżki launchera, następnie `--verify-existing-house-gui` zweryfikowało już zakończony przebieg bez restartu. Sam błąd launchera pozostaje odnotowany; przyszłe uruchomienia utrwalają kod wyjścia przed odczytem obserwacji. Weryfikator potwierdził hash kopii EXE, trzy hashe oryginalnego fixture i jego kopii oraz brak nowej mapy i znacznika.

**PASS tego scenariusza GUI i zwykłego shutdown**, nie wszystkich operacji domów. Nadal nie sprawdzono przycisków Edit/Remove, ostrzeżenia zapisu po zmianie czynszu przez dialog, peerów live, awarii alokacji, poprawnej rotacji/podium w Lua ani wszystkich faz przerwania zapisu/odzyskiwania. Dotychczasowe testy core domu i rzeczywistego `IOMap::loadMap` BlackTek pozostają osobnymi dowodami o opisanym wcześniej zakresie. Nie zmieniono produkcyjnego C++, cudzych zmian, map użytkownika ani działającego Dockera BlackTek.

## 2026-11-01 — rzeczywiste przerwanie first-save i restart normalnego GUI

Stan: **G0 IN PROGRESS**. Prywatny probe został przerwany przez Win32 `TerminateProcess` po odczytaniu faktycznego kompletnego znacznika V1 podczas `EditorPersistence::saveMap` nowej pustej mapy, przed jego usunięciem. Obserwator sprawdził ścieżkę EXE i PID 12416; oba procesy (obserwator i launcher) potwierdziły oczekiwany kod `18177` (`0x4701`). Standardowy runner zgłosił ten kod jako przerwanie zamiast PASS nieukończonego testu. Znacznik zachował bajty, trzy cele pozostały nieobecne. Nie ustalono dokładnej instrukcji serializatora w chwili zatrzymania. Pierwsza wcześniejsza próba ukończyła się normalnie przed przerwaniem i nie jest dowodem awarii.

Na tym samym prywatnym fixture uruchomiono kopię normalnego `build-ninja/rme.exe` (ten sam sprawdzony hash `29691821bfcbd40da449162ec20015adee4471f4731e4c03b730d08d63907b8a`), bez testowego modułu. Konfiguracja ma osobne dane i pustą listę ostatnich map; wyłączono wyłącznie dla tej instancji welcome, single-instance forwarding i updater. Startup pokazał rzeczywisty `Incomplete save` z trzema celami z przerwanego zapisu. Po zamknięciu utworzono pustą `Untitled-1.otbm`. Ctrl+S → nowy prywatny `blocked-after-crash.otbm` zakończyło się `Save failed` z pełną ścieżką zachowanego znacznika. Po zamknięciu komunikatu karta nadal miała tę samą, niezmodyfikowaną nazwę; nie powstał cel ani żaden companion.

**PASS w tym zakresie**, `g0-firstsave-crash-watch-20261101/{interruption.json,restart-verification.json,restart-ui-observation.json,restart-exit.json}`. Marker SHA-256 `b730eda50009ae9e36598bdb93c9fe481ee82aaa015ca61465093f8a577a7994`, zgodny przed przerwaniem, po nim i po próbie ponownego zapisu. Trzy kopie wejścia i oryginalny fixture zachowały hashe. Po weryfikacji launcher celowo zakończył tylko swój prywatny proces GUI; nie sprawdzano normalnego zamykania aplikacji. Skrypty i powtórzenie w `TEST_PLAN.md`.

Nie sprawdzono przerwania w innych fazach, częściowo zapisanych celów, istniejącej mapy z backupami, przerwania samego odzyskiwania ani zaniku zasilania. To realny test jednego przerwania i następującego po nim restartu/blokady zapisu, nie pełny PASS crash recovery. Nie zmieniono produkcyjnego C++ w tej próbie. `forgotten.otbm`, jej companiony, otwarta sesja użytkownika i Docker/świat/baza BlackTek pozostały nietknięte.

## 2026-11-01 — odzyskiwanie V1/legacy po odłożeniu otwarcia mapy

Stan: **G0 IN PROGRESS**. Build poprawki `OnEventLoopEnter` oraz linkowanie osobnego modułu z 408 obiektami zakończone kodem 0. Normalny EXE SHA-256 `29691821bfcbd40da449162ec20015adee4471f4731e4c03b730d08d63907b8a`; EXE testu `3a421d9feccf0e9b8074fa009a0cadb4264d1255a7754b2446fb09656d5710f8`, moduł `ed66061311da43a76c762cf332a599278ce721e8920fbabb77cff75b1997f9a3`. Zgodność aktualnego źródła i normalnego EXE z manifestem potwierdzono po wykonaniu prób.

- `g0-recovery-deferred-runtime-20261101`, pełny V1: **PASS**, kod 0. Rzeczywisty prompt wymienił wyłącznie trzy prywatne backupy; po potwierdzeniu przywrócono bajty wszystkich celów, usunięto znacznik i zakończone backupy, następnie otwarto 402 pola. Odczytano zwykłe ostrzeżenia materiałów; wcześniejszy alert destruktora dialogu postępu nie wystąpił.
- `g0-recovery-legacy-deferred-20261101`: **PASS**, kod 0. Rzeczywiste ostrzeżenie legacy i prompt odzyskiwania; trzy cele przywrócone, 402 pola wczytane, wszystkie trzy backupy i znacznik zachowane bajtowo do kontroli. Nie wystąpił wcześniejszy alert wxWidgets.
- `g0-firstsave-v1-20261101`: **PASS**, kod 0. Pierwszy zapis prawdziwego pustego edytora ponownie sprawdził odmowę przy istniejącym i pustym znaczniku, rzeczywisty writer V1 z poprawnymi danymi, zachowanie `noreplace`, błąd I/O do nieistniejącego katalogu, rollback całego sprawdzanego stanu i udane ponowienie. Podczas komunikatu błędu odczytano faktyczny V1: nagłówek, count 10, siedem pustych backupów, trzy nowe cele i footer; zapis w `observed-marker-lines.json`. Odczyt RME i niezależny parser potwierdziły pusty OTBM oraz oba XML.

Każdy katalog ma `verification.json`, log runtime, manifest; przypadki startup także `startup-ui-observation.json` z rzeczywiście odczytanymi oknami prywatnego EXE. Hashe źródłowego zestawu i jego trzech kopii pozostały niezmienione. Żadnego testu nie ponawiano na istniejącym `run`. To zamyka wskazaną regresję otwierania odzyskanej mapy i wymienione scenariusze protokołu, nie rzeczywiste przerwanie procesu ani trwałość po zaniku zasilania. Nadal otwarte są pozostałe próby GUI, błędy odzyskiwania i końcowy audyt bramek G0. `forgotten.otbm`, jej companiony oraz Docker/świat/baza BlackTek pozostały nietknięte.

## 2026-11-01 — kompletność znacznika odzyskiwania

Stan: **G0 IN PROGRESS**. Nowy `recovery_marker.h` definiuje prywatny format `.saving.txt` V1: nagłówek wersji, liczba rekordów, siedem pozycji backupów i ewentualne nowe cele oraz znacznik końca. OTBM/XML nie zmieniono. Writer sprawdza dane przed otwarciem pliku i zachowuje `noreplace`. Czytnik ogranicza rozmiar i liczbę rekordów, odrzuca niekompletne/nieznane formaty oraz nieprawidłowe ścieżki przed przywracaniem czegokolwiek. Stary format pozostaje rozpoznawany, ale nie pozwala udowodnić kompletności opcjonalnych celów: odzyskanie znanych backupów zachowuje wtedy wszystkie kopie i znacznik do ręcznej kontroli.

Build RME po zmianie protokołu **PASS**, EXE `1f60b352a87c9815a165f8d62422f57a01cdab0c4bac7754e469538a42510ba0`. Nowy CTest `test_recovery_marker` **PASS 1/1**: każdy ucięty prefiks poprawnego nowego znacznika, błędny count/footer/wersja, dodatkowe dane, stare CRLF, UTF-8, nieprawidłowe rekordy i limit strumienia. Źródła/hash/logi: `local-data/procedural-smoke/g0-recovery-marker-20261101/{unit-verification.json,CTest-LastTest.log}`.

Rzeczywisty startup z uciętym znacznikiem **PASS w zakresie odmowy**: odczytano dialog `Incomplete recovery marker`; wszystkie trzy cele, trzy backupy i znacznik pozostały bajtowo bez zmian. Artefakty: `g0-marker-startup-final-20261101`. Pierwszy wcześniejszy wariant `g0-versioned-marker-runtime-20261101` unieważniono: moduł idle mógł zakończyć proces wewnątrz startowego dialogu. Zachowano `verification-premature.json`, a aktualny status tego wariantu to UNVERIFIED. Poprawiony moduł wymaga `IsMainLoopRunning`, a verifier także odnotowanych rzeczywistych dialogów prywatnego EXE.

Startup legacy odtworzył trzy prywatne cele, wczytał 402 pola, zachował trzy backupy i znacznik. Wystąpiła jednak asercja wxWidgets o pętli `wxGenericProgressDialog` podczas otwierania mapy w `OnInit`; po Continue i zamknięciu znanych ostrzeżeń danych próba zakończyła się kodem 0. Runner przekroczył 180 s oczekiwania, więc nie uruchamiano go ponownie: otworzono uchwyt nadal działającego PID 18676, sprawdzono ścieżkę EXE i zaobserwowano końcowy kod 0. Tryb `--verify-existing-startup` wykonał wyłącznie kontrolę ukończonych artefaktów. Raport `g0-marker-legacy-final-20261101/verification.json` jawnie zawiera ograniczenie diagnostyczne — to **nie** czysty PASS GUI.

Odłożono otwarcie odzyskanej mapy do istniejącego `OnEventLoopEnter` zamiast `OnInit` i dodano warunek testu, aby nie startował przy wyłączonym głównym oknie. Build tej ostatniej poprawki **PASS**. Linkowanie nowego modułu testowego trwa w `g0-recovery-deferred-runtime-20261101`; runtime pełnego znacznika, legacy po tej poprawce oraz ponowna regresja first-save na V1 pozostają niewykonane. Nadal brak testu rzeczywistego przerwania procesu i zasilania. `forgotten.otbm`, jej companiony oraz Docker/świat/baza BlackTek pozostają nietknięte.

## 2026-11-01 — rzeczywisty pierwszy zapis, odmowa i ponowienie

Stan: **G0 IN PROGRESS**. Osobny `g0_rme_first_save_probe.cpp` zlinkowany z aktualnymi 408 obiektami RME przeszedł test **PASS, kod 0** w `local-data/procedural-smoke/g0-first-save-runtime-20261101`. Tworzy prawdziwy nowy `Editor` z pustą, nienazwaną mapą zgodnej wersji; nie przełącza flagi istniejącej mapy. Rzeczywisty `WriteCrashMarker` odmówił nadpisania istniejącego znacznika. `EditorPersistence::saveMap` odrzucił pierwszy zapis przy istniejącym i pustym znaczniku, bez zmiany mapy i bez pliku docelowego.

Zapis do nieistniejącego prywatnego katalogu wymusił rzeczywisty błąd I/O. W trakcie komunikatu odczytano rzeczywisty znacznik: siedem pustych pozycji backupów i trzy nowe cele OTBM/house/spawn. Zapis tego odczytu: `run/data/user/data/observed-marker-lines.json`. Zamknięto wyłącznie komunikat `Error` prywatnego EXE; normalnych okien RME nie obsługiwano. Po powrocie z funkcji metadane, unnamed i dirty były zgodne ze stanem wejściowym, znacznik usunięty, brak docelowego katalogu i plików. Kolejny pierwszy zapis do świeżego katalogu zakończył się sukcesem: unnamed/dirty wyczyszczone, nowe nazwy companion, brak backupów `~` i znacznika. Odczyt RME i niezależny parser potwierdziły pustą mapę oraz prawidłowe puste house/spawn XML. Hashe wejściowego zestawu i jego kopii pozostały niezmienione.

Manifest: EXE `31310090481d9f355defb8aa6cb53e335c6436deb3f6b601e673363d0763cbec`, moduł `f7ad5d0ccdea63511f03ba23e243fa1f7f0981a18be08f8b510e5236cf1b996f`; raport `verification.json`. To test pierwszego zapisu pustej mapy oraz błędu I/O i rollbacku, nie rzeczywistej awarii procesu, utraty zasilania ani startowego odzyskiwania tej próby. Runtime nadal pokazuje komunikat błędu mimo `showdialog=false`; test wymaga jego zamknięcia. Runner pozostawia PID do zbadania po timeout i nie uruchamia procesu ponownie automatycznie.

Runner usuwa ostatnie mapy z kopiowanej konfiguracji i odrzuca szablon zawierający `forgotten.otbm`. Nie otwierano ani nie zmieniano tej mapy/companionów, działającego Dockera BlackTek, jego świata ani bazy. Źródła produkcyjne po sprawdzonym buildzie nie zmieniły się w tej próbie. Otwarte pozostają rzeczywiste przerwanie zapisu/odzyskiwania, przypadki uszkodzonego/niekompletnego znacznika i pozostałe bramki z planu.

## 2026-11-01 — Undo dodania/usunięcia domu i blokada metadanych live

Stan: **G0 IN PROGRESS**. Zachowano wcześniejszy working tree. `HousePalette::OnAddHouse` i `OnRemoveHouse` tworzą teraz `Change`/`Action` zamiast bezpośrednio zmieniać zbiór domów. Własność tego samego `House` przechodzi między mapą a historią: Undo usunięcia zachowuje generację, właściwości, listę pól i wyjście, więc wcześniejsze akcje pozostają wykonalne. Pola zachowują pozostałą zawartość i flagi. Usuwanie czyści aktywne wskaźniki brushy; zmiany oznaczają mapę i właściwe regiony jako zmienione. Zmiana ID we właściwościach aktualizuje też indeks wyjścia, czego brak ujawniła rozszerzona kontrola. Metadane domów nie są rozsyłane obecnym protokołem live, dlatego dodanie/usunięcie/edycja są blokowane w UI i kolejce akcji w takim stanie.

Build `build_ninja.bat` **PASS** po dodaniu istniejącego Ninja do PATH i `VCPKG_ROOT` wyłącznie w procesie. Pierwsze linkowanie zatrzymał otwarty stary EXE; zachowano jego proces i stan, przenosząc plik do `build-ninja/rme.pre-house-lifecycle-20261030.exe`, a następny build utworzył nowy `rme.exe` (SHA-256 `f058557fda7acc87ba8635ea02bf164c408ef751eadc788d5c1100063df9f26f`). Nie zamykano cudzej sesji ani mapy.

Rozszerzony osobny moduł testowy z 408 aktualnymi obiektami RME **PASS, kod 0**, wykonany 01.11 (snapshot 08:43:42). Artefakty pozostają w katalogu nazwanym przy przygotowaniu 30.10: `local-data/procedural-smoke/g0-house-lifecycle-runtime-20261030`. `verification.json` wymaga ośmiu znaczników runtime, niezmienionych hashy trzech plików wejściowych oraz niezależnego porównania snapshotu po odtworzeniu domu. Potwierdzono:

- cały łańcuch dodanie → edycja ID/właściwości → usunięcie, trzy osobne Undo i trzy Redo; odtwarzany jest ten sam obiekt;
- usunięcie istniejącego domu, zachowanie flag pola, usunięcie i odtworzenie indeksu wyjścia bez duplikatów, odmowę Undo przy konflikcie ID i ponowienie po jego usunięciu;
- bezpieczne usunięcie historii posiadającej odłączony dom oraz dotychczasowe przypadki konfliktu i zastąpienia obiektu;
- odmowę dodania/usunięcia/edycji w rzeczywistym stanie `LiveManager`/`NetworkedActionQueue`, bez mutacji, dirty i wpisu historii, oraz powrót do edycji offline po zamknięciu stanu live. Test nie wywołuje `bind`, nie otwiera portu, nie tworzy peerów;
- odczyt przywróconego snapshotu przez RME oraz niezależne porównanie wszystkich 402 pozycji, pozostałych danych OTBM i house/spawn XML z wejściem.

EXE testu SHA-256 `6705c7a31d3c2401e446f4ae737febcfa447e904c851cf2cec5749cc92befb66`, moduł `702d8bae78972953ecb14524fb5ae32a2417811fa0c56e2a113074ef75e7699e`. Sprawdzono zgodność obu i normalnego EXE z manifestem przed wykonaniem. Nie testowano jeszcze przycisków GUI po tej zmianie, współpracy dwóch peerów ani błędów alokacji. Test pomija globalne zamykanie aplikacji przez `_Exit` po zniszczeniu lokalnych edytorów. Nie otwierano ani nie modyfikowano `forgotten.otbm` i jej plików pomocniczych; działający Docker BlackTek, jego świat i baza pozostały nietknięte.

## 2026-10-30 — ponowienie historii domu i zastąpienie obiektu

Stan: **G0 IN PROGRESS**. Nowy testowy moduł `g0_rme_house_history_probe.cpp`, zlinkowany osobno z tymi samymi 408 obiektami RME, przeszedł rzeczywiste operacje `Action`/`ActionQueue` na prywatnym fixture. Konflikt Undo dla ID 1 i konflikt Redo dla ID 2 zachowały oba domy, ID pola i indeks historii. Po usunięciu każdego konfliktu ponowienie operacji zadziałało, odtwarzając właściwości, generację, wyjście, liczbę pól i relację house ID; Undo po wyczyszczeniu dirty ponownie oznaczyło mapę zmienioną. Usunięcie edytowanego domu zatrzymało stare Undo; nowy dom z tym samym ID miał inną generację i również pozostał nietknięty przez starą akcję.

Build i runner **PASS, kod 0**. Artefakty: `local-data/procedural-smoke/g0-house-history-runtime-20261030`, log `run/snapshot-probe.log`, manifest i `verification.json`; hashe źródłowego fixture i kopii niezmienione. EXE SHA-256 `85fd8b952f006f5fb729499dac3c2c153a9f0af5ea93f212d7e0a4ed80a03c27`. Test nie zapisuje mapy i pomija globalne zamykanie aplikacji przez `_Exit` po zniszczeniu lokalnego edytora. Nie jest to dodatkowy test dialogu ani odtworzenia usuniętego domu: istniejące dodawanie/usuwanie nadal nie ma Undo. Live i wszystkie pola dialogu nadal wymagają pracy. Nie zmieniono źródeł produkcyjnych ani otwartej instancji RME; Docker BlackTek nietknięty.

## 2026-10-30 — rzeczywisty snapshot i Save As z BOTH

Stan: **G0 IN PROGRESS**. Bez zmian produkcyjnych zlinkowano osobny `snapshot_probe.exe`: 408 istniejących obiektów aktualnego buildu RME oraz testowy moduł wxWidgets. Moduł wywołuje rzeczywiste `MapBackupService::CreateSnapshot`, `EditorPersistence::saveMap` i `EditorFactory::LoadFromFile` na prywatnej kopii 402 pól z domu, spawnów, UID/AID/PZ i zagnieżdżonego kontenera. Nie uruchamia destrukcyjnego `UnreachableCleaner`. Normalne EXE, źródłowy fixture i działający Docker BlackTek pozostały nietknięte.

- **PASS snapshot:** cztery odrębne zestawy, także przy faktycznie zaobserwowanej kolizji nazw w tej samej sekundzie; stan clean/dirty i nazwy mapy/plików pomocniczych zachowane. Każdy wynik ponownie odczytano w RME. Nieprawidłowa nazwa katalogu została odrzucona. Blokada nowego house XML wymusiła rzeczywisty błąd serializacji; częściowy OTBM pozostał, komunikat wskazał katalog, a dirty i metadane przywrócono.
- **PASS Save As:** względne i absolutne wejściowe nazwy house/spawn, wyjście z nowymi względnymi nazwami omijającymi dwa sentinele. `BOTH` zachowało waypoint w OTBM i osobnym XML, zmienione text/description oraz UID/AID. Kolizja docelowego waypoint XML odrzuciła zapis bez nowego OTBM i bez zmiany stanu. Wszystkie sentinele pozostały.
- **PASS niezależnej kontroli:** komplet 402 pozycji, 401 niezmienionych pól, semantyka kontenera, pozostałe węzły i oba XML; dla Save As dokładnie oczekiwane zmiany ground i waypointu. Cztery negatywne kontrole wykryły zmieniony czynsz, szerokość mapy i uszkodzone waypointy w obu formatach. Runner sprawdził niezmienione hashe kopii i źródłowego fixture.

Powtarzalny runner zakończył się kodem 0 o 18:01:46; dowody w `local-data/procedural-smoke/g0-persistence-runner2-20261030/{build-manifest.json,verification.json,run/snapshot-probe.log}`. EXE SHA-256 `8dc5e943d334328f551a80b5d8a68fcf38ba5b29a102d8cbe845ae190d2530fa`. Pierwszy runner zatrzymał brak sidecar `snapshot_probe.exe.manifest` i okno wxWidgets; po odczycie ostrzeżenia zamknięto je, proces ukończył test. Poprawiony runner odrzuca brak manifestu przed uruchomieniem; drugi przebieg nie wymagał interakcji. Wcześniejszy poprawny snapshot został też odczytany przez izolowany loader BlackTek (0), a częściowy odrzucony przy house XML (5).

To osobny test integracyjny rzeczywistych funkcji, nie test całego GUI ani zamykania aplikacji: po zniszczeniu lokalnych edytorów moduł wykonuje `_Exit`. Nie sprawdzono awarii procesu/zasilania, wyjątków alokacji, konkurencyjnych pisarzy wewnątrz zarezerwowanego katalogu ani całego protokołu pierwszego zapisu/odzyskiwania. Pozostają otwarte scenariusze historii domu, pozostałe wymagania Lua i końcowe zestawienie wszystkich bramek. Szczegóły i komendy: `TEST_PLAN.md`.

## 2026-10-30 — ponowne otwarcie mapy i macierz setterów Lua

Stan: **G0 IN PROGRESS**. Zachowano working tree na `fix/save-as-companion-safety`. Kontynuowano poprzednią próbę zamiast powtarzać audyt loadera. Utworzono odrębny profil w `local-data/procedural-smoke/g0-reference-profile-20261030`: kopia `data/1098`, pliki wspólne danych oraz `clients.toml` z jedynym, niezmienionym profilem `1098-BlackTek`. Lokalny `config.toml` wskazuje ten katalog przez `data_directory`. Nie zmieniono katalogu danych normalnego buildu ani nagłówka mapy. Po zakończeniu poprzedniej testowej instancji uruchomiono ten sam RME EXE z 12:31:05, SHA-256 `2cd4a8ad93f6d0c7410ca2f200df56f2b6f467f9f4d11b5f39fbc6301d331f67`.

- `test_reference_map_reopen.lua`: **PASS**, uzbrojenie 17:35:44, wynik 17:36:17. Welcome otworzył 402 pola, zamknięto kartę, File → Open otworzyło tę samą kopię bez zmiany profilu i bez przeładowania Lua. Stare referencje tile/item zostały odrzucone; nowe odczytały wartości fixture. To zamyka tę konkretną próbę, nie dowodzi ponownego użycia tego samego adresu pamięci ani wszystkich cykli życia.
- Nowy `test_item_setter_matrix.lua`: **PASS**, 17:38:10. 17 kombinacji: AID/UID/tier/text/description dla ground, stack item 2148 i korzenia kontenera 1987 oraz count/subtype dla stack item. Każdy przypadek sprawdził referencję pobraną przed transakcją, propagację błędu i rollback bez zmiany historii, jawny commit z Undo/Redo oraz samodzielny setter z osobnym Undo/Redo. Dziesięć liczb poza zakresem i `setPodiumOutfit` na nie-podium odrzucono bez zmiany historii. Końcowe Undo usunęło tymczasowe przedmioty i przywróciło sprawdzane dane fixture. Nie testowano przedmiotów wewnątrz kontenera, poprawnej rotacji/podium, błędów alokacji ani serializacji zmienionych tier/text/description.
- Zapisano faktycznie odczytaną konsolę w `lua-tests.log` oraz hashe i kontrole w `verification.json` w katalogu profilu. OTBM i oba XML są bajtowo zgodne z wejściową kopią, a uruchomione skrypty z `build-ninja/scripts` są zgodne z plikami w `lua_tests`. Nie zapisywano mapy. RME pozostaje uruchomione na testowej kopii po Undo do indeksu historii 0.

Nie zmieniono źródeł produkcyjnych ani działającego BlackTek/Dockera. Nadal wymagają pracy testy Save As/odzyskiwania i rzeczywistego snapshotu, pozostałe przypadki historii domu oraz końcowy audyt wszystkich bramek G0. Następne trzy zakresy: (1) runtime snapshotu i błędu jego zapisu na kopii; (2) brakujące warianty Save As/odzyskiwania; (3) pozostałe wymagania Lua/historii domu i końcowa decyzja C++/Lua z pełną tabelą dowodów.

## 2026-10-30 — świeży izolowany loader BlackTek

Stan: **G0 IN PROGRESS**. Potwierdzono końcowe linkowanie w `g0-1098-20261026/blacktek-fresh/build.log`. Nowy harness zbudowano z 85 aktualnych obiektów per-source, wykluczając stare obiekty unity oraz `otserv.obj`. `otserv.cpp` skompilowano osobno ze zmienioną nazwą `main`, bez `/FORCE:MULTIPLE`. Narzędzie budujące odrzuca brakujące/nowsze źródła CPP i nagłówki; zapisuje hashe źródeł, obiektów i EXE. Istniejące niezacommitowane zmiany zachowano na `fix/save-as-companion-safety`.

Na prywatnych kopiach zapisanego przez RME fixture `g0-container-20261030` rzeczywiste `IOMap::loadMap`, `IOMap::loadHouses` i `IOMap::loadSpawns` przeszły asercje: dom ID 1, nazwa, czynsz 4321, miasto, wyjście i powiązane pole; piętra 6/7 z UID/AID/PZ; worek → worek → 7 monet z AID 12042; spawn Rat, pozycja, radius 3, 90 s i kierunek południowy. Rat ma minimalną syntetyczną definicję XML w katalogu testowym, ładowaną rzeczywistym loaderem potworów. Nie jest to test definicji potworów użytkownika ani ich uruchomienia w grze.

Runner wykonał **6/6 PASS**: poprawny zestaw oraz oczekiwane odrzucenie braku house XML, braku spawn XML, zmienionego czynszu, pustych spawnów i obciętego OTBM. Asercja pustych spawnów jest konieczna: sam sukces parsera XML nie dowodzi wczytania potwora. Porównania bajtów potwierdziły brak mutacji kopii podczas odczytu oraz zachowanie trzech źródłowych plików. Raport: `local-data/procedural-smoke/g0-1098-20261026/blacktek-fresh/iomap-cases.json`; manifest buildu: `probe_build_manifest.json` w tym samym katalogu. Źródła i instrukcja powtórzenia: `docs/procedural/smoke/g0_blacktek_*` i `TEST_PLAN.md`.

**Ograniczenie procesu:** pierwsza próba po udanych asercjach i poprawnym zniszczeniu lokalnej mapy zakończyła się access violation `0xC0000005` podczas końcowego niszczenia globalnych obiektów serwera; dokładnej przyczyny nie ustalono. Końcowy harness wywołuje `_Exit` dopiero po powrocie z funkcji testowej (po zniszczeniu lokalnej mapy i jej bufora), pomijając tę fazę. Wynik potwierdza odczyt wskazanego fixture przez aktualny loader Windows, nie pełny cykl start/stop serwera ani zgodność wdrożonego obrazu Linux/Docker. Działającego kontenera, jego bazy i świata nie uruchamiano ponownie ani nie modyfikowano.

Przygotowano `lua_tests/test_reference_map_reopen.lua` i rozpoczęto go w rzeczywistym RME na nowej kopii `g0-reference-reopen-20261030/g0-metadata.otbm`, build EXE z 12:31:05. Pierwszy etap zgłosił `REFERENCE REOPEN ARMED` o 17:31:08, następnie zamknięto kartę. Próba ponownego odczytu przez File → Open wybrała standardowy profil `10.98` i zatrzymała się na braku assets; anulowano wybór katalogu. Nie wykonano drugiego etapu, test **NIEZWERYFIKOWANY**. Plików mapy nie zapisywano ani nie zmieniano ich nagłówka. Instancja testowa pozostaje bez otwartej mapy; nie przeładowano Lua. Potrzebny jest sposób jawnego wyboru profilu przy ponownym otwarciu w tej samej sesji (np. izolowany katalog danych z jednoznacznym profilem), bez podstawiania niezgodnego OTB standardowego klienta.

`git diff --check` oraz istniejący lokalny walidator dokumentacji (UTF-8, domknięte bloki, linki i dotychczasowe skrypty archiwalne) przeszły. Nie przebudowywano RME w tym zakresie: nie zmieniono jego źródeł produkcyjnych. Pozostałe otwarte zadania G0 zachowują dotychczasowy status. Następne trzy zakresy: (1) dokończyć regresję referencji/setterów Lua i ponownego otwarcia mapy; (2) dokończyć testy Save As/odzyskiwania i snapshotu oraz otwarte scenariusze historii domu; (3) zestawić dowody wszystkich wymagań G0 i zamknąć decyzję C++/Lua bez uznawania testu loadera za zaliczenie całej bramki.

## 2026-10-30 — czas życia referencji i walidacja domu

Stan: **G0 IN PROGRESS**. Na izolowanej mapie z profilem `1098-BlackTek` test przełączania kart potwierdził, że referencje pola i przedmiotu są niedostępne na drugiej mapie i działają po powrocie na pierwszą, jeśli obiekty istnieją. Osobna próba po zamknięciu pierwszej mapy również odrzuciła dostęp na drugiej. Nie sprawdzono ponownego otwarcia tej samej mapy ani zagnieżdżonych przedmiotów w kontenerze; API Lua nie udostępnia jego zawartości.

Audyt dodatkowego mechanizmu snapshotów wykazał ryzyko kolizji: `MapBackupService::CreateSnapshot` tworzy nazwę z dokładnością do sekundy i bez rezerwacji zapisuje OTBM oraz XML; w razie błędu usuwa wyliczone ścieżki, nie sprawdzając, czy należały do wcześniejszego snapshotu. Jedyny znaleziony wywołujący to `UnreachableCleaner::Clean`, który po osobnym potwierdzeniu usuwa pola i czyści historię. To ustalenie z kodu, nie odtworzony przypadek utraty danych. Wymagana jest osobna poprawka bezpiecznej rezerwacji całego zestawu i testy kolizji oraz niepowodzenia zapisu; sam timestamp ani sprawdzenie istnienia przed zapisem nie zamykają wyścigu.

Przygotowano zmianę snapshotu: przed serializacją tworzony jest nowy katalog przez `create_directory`, z kolejnym sufiksem przy kolizji; zestaw OTBM/XML trafia do tego katalogu. Po błędzie zapisu pliki częściowe pozostają, a komunikat wskazuje katalog. Kompilacja i linkowanie RME Windows x64 **PASS**. Nie wykonano jeszcze testu rzeczywistego snapshotu, kolizji, niepowodzenia zapisu ani odczytu jego plików; poprawka pozostaje w toku i nie stanowi dowodu odporności na inne procesy zapisujące wewnątrz już utworzonego katalogu.

Rezerwację wydzielono do funkcji używanej przez `CreateSnapshot` i sprawdzono w `test_snapshot_directory`: osiem równoczesnych rezerwacji dało różne katalogi, wcześniej istniejące katalog i plik zachowały zawartość, a brak katalogu nadrzędnego zgłosił błąd. CTest Windows x64 **PASS 1/1**; ponowny build i link RME **PASS**. To test rezerwacji, nie serializacji całego snapshotu, błędu podczas zapisu ani jego ponownego odczytu.

W odrębnym profilu przetestowano syntetyczny znacznik przerwanego pierwszego zapisu: startup zgłosił dwa nowe cele i zachował znacznik oraz zawartość obu plików po zamknięciu programu. Ścieżki były wizualnie skrócone w dialogu; pełne znalazły się w drzewie dostępności. Szczegóły i ograniczenia w `TEST_PLAN.md`; nie symulowano rzeczywistej awarii.

Po zamianie natywnego komunikatu na istniejące pole tekstowe build Windows x64 przeszedł. Ponowny start z tym samym znacznikiem pokazał obie kompletne ścieżki zawinięte w oknie; Ctrl+A zaznaczyło pełną treść. Nie sprawdzano zawartości schowka, by go nie zmieniać. Znacznik pozostał w fixture.

Na tym samym izolowanym profilu otwarto kopię `guard-control.otbm` (400 pól) i użyto Ctrl+S przy znaczniku pierwszego zapisu. Edytor odmówił zapisu; SHA-256 OTBM, obu XML, znacznika i obu nowych celów nie zmieniły się. Ścieżka znacznika w komunikacie była początkowo skrócona, więc zastąpiono go polem tekstowym; build i ponowny test GUI potwierdziły widoczną pełną ścieżkę oraz tę samą blokadę. Nie testowano awarii podczas rzeczywistego zapisu.

Na syntetycznej kopii OTBM 3 sprawdzono zagnieżdżony kontener: worek 1987 → worek 1987 → 7 monet 2148 z AID 12042 na (100,100,7). RME z `1098-BlackTek` otworzył mapę, zapisał ją, po restarcie ponownie otworzył i zapisał; niezależny odczyt węzłów po każdym zapisie potwierdził te wartości. Pierwsze porównanie bajtowe nie przeszło, ponieważ RME zmienił kolejność atrybutów count/AID; porównanie semantyczne przeszło. Nie sprawdzono pełnego stanu mapy ani innych rodzajów zawartości kontenerów.

Dodatkowe porównanie przed pierwszym i po drugim zapisie potwierdziło 402 pozycje, identyczne węzły pozostałych 401 pól oraz zachowane drzewa XML domów i spawnów.

Następnie rozszerzono porównanie o nagłówek i pozostałe węzły OTBM: dla tego fixture również są identyczne. Pięć testów weryfikatora przeszło, w tym negatywne próby modyfikacji metadanych, pola, kontenera i XML domu. Zewnętrzny loader i nieobecne w fixture atrybuty pozostają niesprawdzone.

Próba uruchomienia `IOMap::loadMap` w odrębnym programie z istniejącymi obiektami buildu BlackTek nie dostarczyła wyniku: proces zakończył się przed potwierdzeniem wejścia do testu, a obiekt loadera okazał się starszy od źródła. Potrzebny jest świeży izolowany build; standardowa inicjalizacja serwera wykonuje połączenie i aktualizacje bazy przed wczytaniem mapy.

Po ograniczeniu kontroli konfliktów do akcji właściwości domu końcowy build Windows x64 przeszedł, a `test_item_implicit_undo.lua` na izolowanym `g0-metadata.otbm` potwierdził zmianę AID oraz Undo/Redo (`IMPLICIT ITEM UNDO PASS`, 07:25:14); test przywrócił pierwotną wartość, a następnie mapę zapisano. Na aktualnym buildzie ponowiono konflikt: na kopii `g0-house-conflict-final-20261030` zmieniono ID domu 1 → 2 i dodano nowy dom z ID 1. GUI odmówiło Undo z komunikatem o konflikcie, a `test_house_history_conflict.lua` potwierdził, że Lua Undo i przejście w historii nie zmieniają jej pozycji (`HOUSE HISTORY CONFLICT PASS`, 12:10:26). Kopię zapisano po teście. Dodawanie i usuwanie domu nadal nie ma obsługi Undo. Podczas zamykania poprzedniego procesu ponownie wystąpił alert wxWidgets `GetWindow() != nullptr`; jego przyczyny nie ustalono. Osobna próba wybrania `New Palette` po otwarciu kopii zakończyła się zniknięciem procesu; przyczyna nieustalona, nie potwierdzono powtarzalności.

**Praca w toku:** przygotowano niezacommitowaną operację historii dla edycji właściwości domu, również ze zmianą ID. Na kopii `local-data/procedural-smoke/g0-house-undo-20261030` build Windows x64 przeszedł, a zmiana czynszu 4321 → 6789, Undo, Redo i zapis XML dały oczekiwane wyniki; Undo po zapisie przywróciło 4321 i oznaczyło mapę jako zmienioną. Zmiana ID 1 → 2 oraz Undo/Redo działały w palecie, a po zapisie odczytano ID 2 w XML i w węźle pola (104,102,7); po Undo i kolejnym zapisie oba pliki wskazały ID 1.

**Wykryta regresja wymagająca naprawy:** po ponowieniu zmiany ID na 2 dodano nowy dom, który zajął ID 1. Undo nie nadpisało nowego domu, ale również nie cofnęło zmiany ID i przesunęło historię. Zmian po dodaniu domu nie zapisano. Przygotowany kod nie jest gotowy; należy zapewnić spójne zachowanie historii w sytuacji konfliktu oraz zbadać niecofalne dodawanie i usuwanie domów i tryb współpracy.

W kolejnej poprawce kolejka sprawdza warunek przed Undo/Redo, zachowując pozycję historii przy konflikcie; GUI zgłasza problem w pasku stanu, a Lua zwraca błąd. Build Windows x64 **PASS**, 197/197. Na tej samej izolowanej kopii ponownie utworzono konflikt ID: GUI zgłosiło zajęte ID i zachowało oba domy; `lua_tests/test_house_history_conflict.lua` **PASS** o 07:18:45 dla `undo()` i `goToHistory()`, z niezmienionym indeksem historii. Po teście stan dwóch domów zapisano w kopii testowej. Nie sprawdzono ponowienia po usunięciu konfliktu ani nie rozwiązano jeszcze niecofalnego dodawania/usuwania domów, przywrócenia usuniętego obiektu i synchronizacji live.

- Walidację właściwości domu wykonuje się przed zmianą ID: nieprawidłowe miasto lub czynsz przerywa operację, a ID innego domu jest odrzucane także przy wyłączonym ostrzeganiu o duplikatach. To ograniczona poprawka, nie naprawa Undo ani oznaczania zmodyfikowanej mapy; te problemy pozostają otwarte.
- Po poprawce build Windows x64 i linkowanie **PASS**; wybrane CTest `test_lua_object_registry`, `test_lua_object_registry_sol`, `test_rollback_file` **PASS 3/3**. Walidacji domu nie sprawdzono jeszcze w GUI. `git diff --check` bez błędów, z ostrzeżeniami o końcach linii.
- Podczas zamykania testowej instancji ponownie wystąpiła asercja wxWidgets `GetWindow() != nullptr` w `GetDescription()`; jej przyczyny nie ustalono. Mapy testowej nie zapisywano.

## 2026-10-29 — referencje Lua i regresja G0

Stan: **G0 IN PROGRESS**; zmiany pozostają niezacommitowane na gałęzi `fix/save-as-companion-safety`. Historyczne wyniki poniżej zachowują stan z dnia próby; aktualne szczegóły i ograniczenia są w [TEST_PLAN.md](procedural/TEST_PLAN.md).

- Wiązania Lua pól i przedmiotów używają referencji sprawdzających czas życia i aktualnego właściciela. Settery przedmiotów mapy uczestniczą w transakcji, a wywołane samodzielnie tworzą operację Undo; klon poza mapą pozostaje niezależny.
- Windows x64: ostatni build i linkowanie **PASS**. Testy CTest rejestru referencji i Sol2 **PASS 2/2**. W edytorze na izolowanej kopii z klientem `1098-BlackTek` przeszły testy rollbacku i historii ground, samodzielnego settera, przedmiotu na stosie, iteratorów i zaznaczenia. Są to testy opisanych scenariuszy, nie pełna weryfikacja API.
- Save As i odzyskiwanie są w toku: sprawdzono wybrane kolizje, Unicode oraz rollback pojedynczego pliku; pełna atomowość, awaria procesu i nowy wariant znacznika nie są potwierdzone. Odkryta utrata niezapisanej zmiany czynszu domu pozostaje nierozwiązana.
- Round-trip pięter, flag, UID/AID, spawnu, osobnego domu i jednej strefy jest częściowy. Zewnętrzny parser BlackTek odczytał węzły OTBM 3, lecz pełny loader i pliki pomocnicze nie zostały przetestowane; loader sprawdzanego BlackTek odrzuca OTBM 5.

Następne trzy zadania: (1) sprawdzić pozostałe operacje referencji, w tym kontenery i przełączanie map; (2) dokończyć i przetestować bezpieczny Save As oraz odzyskiwanie i rozwiązać utratę zmian domu; (3) wykonać pełny round-trip i test właściwego zewnętrznego odbiorcy, a następnie audyt wszystkich bramek G0.

## 2026-10-27 — ręczna historia i rozszerzony round-trip

Stan: **G0 IN PROGRESS**. Bazą kodu jest `a71fe4b`; wyniki poniżej z tego buildu. Powstała gałąź `docs/procedural-g0-validation` z `master`, następnie fast-forward do naprawy callbacka.

- **G0-06 PASS w badanym zakresie:** ręczny Draw gumką 1×1, transakcja Lua i ręczny Draw brushem `dried grass` utworzyły oddzielne wpisy. Trzy Undo i trzy Redo sprawdziły migawki ground, stosów ID, flag, house ID i obecności encji dla obszaru 24×24 oraz osobny tile Lua; po kolejnych Undo fixture przywrócono. `group_actions=true`, `use_automagic=true`, limit historii 400, 40 MiB. Pierwsza próba drugiej edycji gumką nie zmieniła zawartości, więc asercja testu słusznie ją odrzuciła; cofnięto tę próbę i użyto innego brusha.
- **Częściowy G0-08 PASS:** syntetyczne tile na piętrach 6 i 7 z ground UID/AID oraz flagą, spawnem radius 3 i `Rat` (90 s) utworzono, zapisano i odczytano w RME z `1098-BlackTek`. Kontrola objęła także 400 bazowych ground i halo; osobno sparsowano XML spawnu. Osobny fixture domu po restarcie zachował widoczne oznaczenie, ID, czynsz i rozmiar; XML zawiera też town ID i współrzędne wyjścia. Strefy, kontenery, pełne atrybuty i odbiorca zewnętrzny nie są zweryfikowane.
- **G0-07 FAIL dla istniejącego Item:** zmiana `ground.actionId` wewnątrz nieudanej transakcji nie została wycofana mimo propagacji błędu. Udana transakcja z tą samą zmianą również nie utworzyła wpisu historii. Diagnostyka przywróciła wartość bez zapisu. Potrzebna osobna naprawa śledzenia mutacji Item oraz testy regresyjne.
- **Zagnieżdżona transakcja:** odrzucenie wewnętrznego wywołania nie wycofuje już zewnętrznego. Test na syntetycznej mapie potwierdził jedną akcję oraz Undo/Redo obu zmian; problem atrybutów Item pozostaje otwarty.
- **Utrata zmiany domu:** na syntetycznym fixture zmiana samego czynszu nie oznaczyła mapy jako zmienionej; zamknięcie karty bez pytania pozostawiło w XML poprzednią wartość. Zapis Ctrl+S przed zamknięciem działał. Potrzebna osobna naprawa z uwzględnieniem Undo.
- **Częściowy test noise PASS:** 400 wartości simplex w RME zgadzało się pomiędzy siatką, powtórzeniem i pojedynczymi próbkami; inny seed zmienił wynik. To nie potwierdza deterministyczności brushy ani całego generatora.
- **Częściowy test algo/geo PASS:** przykładowa jaskinia z seedem powtórzyła się, a linia Bresenhama miała właściwe końce i ciągłość. Zaobserwowano w kodzie brak jawnego porządkowania tabel punktów dla niektórych operacji; potrzebne dalsze testy przed wykorzystaniem ich jako stabilnego kontraktu.
- **Save As:** poprzednio potwierdzono nadpisanie XML mapy źródłowej na syntetycznym zestawie. Przygotowana poprawka nadaje kopii nowe, wolne nazwy XML; w przypadku waypointów wymagana jest nazwa wynikająca z nazwy OTBM, a jej kolizja przerywa zapis. Test OTBM 3 potwierdził zachowanie źródła i istniejących plików, pierwszy zapis pustej mapy oraz odczyt waypointu z XML. Regresja OTBM 5, anulowania i rollbacku pozostaje przed G1; patrz `procedural/TEST_PLAN.md`.
- Podczas zamykania sesji testowej pojawiła się asercja wxWidgets `GetWindow() != nullptr` w `GetDescription()`; wybrano Stop po zapisaniu i zamknięciu mapy. Nie ustalono przyczyny ani związku z testem.
- Osobny target `test_universal_rotator` zbudowano z `--parallel 2`, następnie `ctest --test-dir build-ninja -R "^test_universal_rotator$" --output-on-failure`: **PASS 1/1**, 0.20 s, kod 0. Nie jest to pełna suita projektu.
- Szczegółowe kroki i wyniki: [TEST_PLAN.md](procedural/TEST_PLAN.md). Zachowane skrypty można uruchomić tylko na izolowanej kopii fixture.

## 2026-10-27 — G0-B: naprawa błędu callbacka Lua

Aktualny stan: **G0 IN PROGRESS**, konkretny bloker ignorowanego callbacka usunięty. Gałąź `fix/lua-transaction-callback` utworzona z `master` i uzupełniona fast-forwardem audytu `b2bc871`.

- `LuaAPI::transaction` sprawdza wynik `sol::protected_function`; błąd callbacka trafia do istniejącego rollbacku i wraca do Lua. Bez zmian implementacji snapshotów, `ActionQueue` i serializacji.
- Build końcowego źródła Windows x64 **PASS**, kod 0, przyrostowo z `--parallel 2`; EXE z 18:52:13 uruchomiono w GUI.
- Nowy test `lua_tests/test_transaction_rollback.lua` **PASS**, 18:53:46: trzy błędy z zachowanymi komunikatami, sześć mutacji wycofanych, historia bez nowych wpisów po błędzie. Udane transakcje i ich osobne Undo/Redo poprawne; fixture przywrócone do 400 pól.
- Oryginalna reprodukcja `g0_error_probe.lua` **PASS**, 18:54:08. Końcowy verifier **PASS**, 18:54:30. SHA-256 OTBM przed/po identyczny; test nie zapisywał mapy.
- Dokumentację API i instrukcję uruchamiania regresji zaktualizowano. CodeGraph i Context7 posłużyły do kontroli mechanizmu; selektor `sorky-validate-change` nie ma trasy dla RME, więc zastosowano build i testy rzeczywistego edytora.
- Kontrola diffu oraz UTF-8, bloków kodu i względnych linków dokumentacji **PASS**. Zachowane skrypty zgodne z wykonanymi po normalizacji CRLF/LF; początkowe porównanie bajtowe odrzuciło wyłącznie zmianę końców linii po przełączeniu gałęzi. Nowy skrypt regresji zgodny z kopią uruchomioną w RME.
- Nadal niezweryfikowane: ręczna edycja obok transakcji, wiele pięter i złożone metadane, zagnieżdżone transakcje, zamknięcie/przełączenie mapy w callbacku, awarie alokacji oraz zgodność z zewnętrznym RME/OTS. Pełnej suity Lua/CTest nie uruchamiano. Szczegółowe logi: [TEST_PLAN.md](procedural/TEST_PLAN.md#g0-b--naprawa-callbacka-i-regresja-2026-10-27).

Następne zadania: G0-06 z ręczną edycją; fixture z metadanymi/wieloma piętrami i round-trip; następnie zamknięcie decyzji adaptera przed G1-A.

## 2026-10-27 — wynik audytu i testów G0

Wpis historyczny audytu przed naprawą G0-B powyżej: **G0 IN PROGRESS**, G1 nierozpoczęte. Ówczesne zmiany obejmowały dokumentację i ręczne skrypty diagnostyczne, bez zmian kodu produkcyjnego. Szczegółowe polecenia, wersje, hashe oraz zakres dowodów: [TEST_PLAN.md](procedural/TEST_PLAN.md).

- Build Windows x64 **PASS**, kod 0: po problemie Ninja/PATH i błędzie pamięci PCH wznowiono ten sam target przez `cmake --build build-ninja --target rme --parallel 2`. EXE 8026112 bajtów, linkowanie `[382/382]`; uruchomiono GUI 4.1.2.
- Profil `1098-BlackTek` (`dat_only`) załadowany z assets wskazanych przez użytkownika i włączoną walidacją podpisów. Zakres `grass` działa; loader zgłasza błędne zakresy i brakujące ID innych materiałów. Standardowego OTB nie zatwierdzono dla tego DAT.
- Edycja syntetycznej mapy Lua: **400 ground + pojedyncze Undo + pojedyncze Redo PASS**. Halo pozostało nieobecne. Zapis OTBM i dwóch pustych XML, a następnie jawny odczyt z profilem BlackTek: **PASS dla tego fixture**. Verifier potwierdził wszystkie ground ID, puste stosy/encje/house ID/flagi i halo; trawa widoczna w rendererze.
- **Bloker: G0-07 FAIL.** Celowe `error()` po zmianie tile w `app.transaction` nie propaguje się i nie wycofuje zmiany: `propagated=false; tile_present=true; tile_count=401`. Końcowy assert diagnostyki zgłosił błąd. Jedno ręczne Undo oraz ponowny verifier przywróciły 400 pól; nie zapisano zmiany diagnostycznej.
- Zwykły File → Open dobiera standardowy profil `10.98` na podstawie nagłówka. Test odczytu wykonano przez Welcome z jawnym `1098-BlackTek`. Po zatwierdzeniu ścieżki w Preferences proces raz zniknął bez komunikatu; przyczyna nieustalona. Do dalszej próby użyto izolowanego `config.toml` i jawnego cwd.
- CodeGraph MCP był dostępny 26.10; 27.10 transport zamknięty, kontynuowano przez działający CLI `codegraph explore`. Context7 i `ast-grep` wykorzystano w audycie źródeł. Skill `sorky-validate-change` wskazał dla dokumentacji przegląd diffu.
- Nadal niewykonane: rozdzielenie historii z ręcznymi edycjami, pełne metadane UID/AID/domów/spawnów/stref i wiele pięter, zgodność z zewnętrznym RME/OTS. Dotychczasowe 33 PASS / 7 SKIP parsera i 34 PASS / 6 FAIL z niestandardowym klientem pozostają osobnymi wynikami.
- Końcowa kontrola dokumentacji: UTF-8, domknięte bloki kodu i względne linki w 5 plikach Markdown **PASS**; trzy zachowane skrypty są bajtowo zgodne z wykonanymi źródłami; oba puste XML pomocnicze poprawnie parsują się jako `houses`/`spawns`.

### Następne trzy zadania

1. G0-B: naprawić kontrolę wyniku callbacka i rollback `app.transaction`; zachowany [probe](procedural/smoke/g0_error_probe.lua) jest reprodukcją błędu, a nie zaliczonym testem.
2. Dokończyć G0-06 i round-trip fixture z metadanymi/wieloma piętrami; osobno zapisać wynik zgodności z docelowym odbiorcą.
3. Dopiero po G0 zamknąć decyzje adaptera C++ (RNG, snapshot, atomowy commit) i rozpocząć G1-A.

## 2026-10-26 — rozpoczęcie FAZY 0

Wpis historyczny: stan podczas trwającego buildu. Wynik końcowy i bieżące zadania opisuje wpis z 27.10 powyżej. Zastąpił wcześniejsze ograniczenia Git/CodeGraph z przeglądu przygotowawczego.

- Baza: `5a01a8fe1c34465559701d50681cc2210ef32c67`, lokalny initial snapshot. Nie ustalono równoważnego SHA upstream. Początkowy working tree czysty.
- Gałąź: `docs/procedural-phase0-audit`, utworzona z `master` przed zmianami.
- CodeGraph MCP działa; CLI zgłosił aktualny indeks 3328 plików / 21857 węzłów / 56553 krawędzi. Zbadano ścieżki rysowania, Undo, Lua, preview i zapisu; `ast-grep` potwierdził pobieranie brusha z GUI.
- Utworzono [architekturę i plan PR](procedural/ARCHITECTURE.md) oraz [plan testów z dowodami](procedural/TEST_PLAN.md). Szczególne ryzyka: natychmiastowy commit części batcha, GUI/RNG w rysowaniu, niepełna definicja pustego tile, zależność preview od kursora i obsługa błędu callbacka Lua. Wnioski runtime pozostają niepotwierdzone do smoke testu.
- Context7: dokumentacja Sol2 `/thephd/sol2`, obsługa wyniku `protected_function_result::valid()`. Jest to dokumentacja upstream; dokładne zachowanie wersji z vcpkg wymaga reprodukcji.
- Reprodukcja na zainstalowanych Lua 5.5.0#1 / Sol2 3.5.0#1 z konfiguracją projektu: ignorowane `sol::function` wraca bez wyjątku mimo `error()` i zachowuje wcześniejszy efekt uboczny; `valid()` wykrywa błąd. Lokalny probe C++ i polecenie opisuje `TEST_PLAN.md`. Potwierdza ryzyko wrappera transakcji, ale nie zastępuje testu prawdziwej mapy/rollbacku/Undo. Nie zmieniono kodu produkcyjnego.
- Build: pierwsze uruchomienie `build_ninja.bat` zatrzymane na Ninja poza `PATH`. Znaleziono lokalny Ninja 1.13.2 w vcpkg; ponowiono ten sam skrypt z tymczasowym `PATH` i `VCPKG_ROOT`. Wykryto CMake 4.3.0-rc3, Visual Studio 18 Community i MSVC 14.51.36231. Zbudowano zależności i skonfigurowano RME, ale kompilacja przy domyślnej równoległości zakończyła się `C3859/C1076` i Windows 1455 (brak pamięci wirtualnej dla PCH). Wznowiono ten sam target i build tree z `--parallel 2`; próba trwa. Nie zmieniano pagefile, źródeł ani kanonicznego skryptu.
- Testy `tools/test_tibia_data.py -v`: 40 przypadków, **33 PASS / 7 SKIP**, kod wyjścia 0. Pominięto testy integracyjne wymagające `Tests1098`.
- Odczyt `data/1098/items.otb`: **PASS**, 26282 itemy, wersja OTB 3.57.62-10.98. Poprawne CLI: `tools/tibia_data.py --otb data/1098/items.otb info`; początkowe umieszczenie `--otb` po `info` zostało odrzucone przez parser, a następnie poprawione.
- Początkowo nie znaleziono kompletu DAT/SPR, mapy OTBM ani gotowego EXE w checkout. Użytkownik wskazał następnie `C:\repo\ProjectSorky\otclient\data\things\1098` oraz `otclient_gl_x64.exe`. Inspekcja wskazanych DAT/SPR z lokalnym OTB 10.98 przeszła: podpisy `0x42A3` / `0x57BBD603`, 26282 itemy i 338944 sprite'y, brak ostrzeżeń. Dane tylko odczytano; EXE OTClient nie zastępuje buildu RME.
- Dalszy test ujawnił, że poprawne parsowanie i te same podpisy **nie dowodzą zgodności mapowania**: oryginalne testy uruchomione na kopiach wskazanych assets i lokalnego OTB w ignorowanym harnessie dały **34 PASS / 6 FAIL** (kod 1, bez SKIP). Błędy dotyczą oczekiwanego fixture, flag/ID i hasha sprite'ów. Nie zmieniano testów, oryginalnych assets ani mapowania, by uzyskać PASS. Szczegóły i log lokalny opisuje `TEST_PLAN.md`.
- CodeGraph potwierdził istniejący tryb `ItemDefinitionResolver::resolveDatOnly`; kandydatem do smoke jest profil `1098-BlackTek` z `data/clients.toml`. DAT ID 4526 odpowiada przechodniemu ground, a standardowy OTB mapuje server ID 4526 do innego client ID (4515). Wybór `dat_only` wymaga potwierdzenia ładowania i materiałów w GUI; nie zatwierdzono standardowego OTB dla tego klienta.
- GUI, Lua 20x20, rollback, Undo/Redo, round-trip RME i kompatybilność z zewnętrznym odbiorcą: **niewykonane**. Testy narzędzia danych nie zastępują tych dowodów.

### Historyczne zadania podczas buildu

1. Doprowadzić próbę kanonicznego buildu do wyniku końcowego i zapisać log/ograniczenia.
2. Potwierdzić kompletny zestaw assets i uruchomić smoke test zgodnie z G0-01–G0-09 w `docs/procedural/TEST_PLAN.md`.
3. Na podstawie reprodukcji zamknąć decyzje o granicy C++/Lua, RNG, snapshotach i atomowym commitowaniu; dopiero wtedy rozpocząć G1-A.

## 2026-10-26 — analiza i redakcja planu

Zakres sesji: przygotowanie `PROJECT_RME_PROCEDURAL_GENERATOR.md` do realizacji. Wersja planu: 1.1. Nie rozpoczęto implementacji generatora; przegląd źródeł jest częściowym przygotowaniem do G0, a nie zaliczeniem tej bramki.

### Wykonano

- Przeczytano plan, `AGENTS.md`, `RTK.md`, konfigurację CMake/vcpkg, fragmenty instrukcji skryptów i skryptu buildu.
- Sprawdzono punkty integracji: `DrawOperations`, `ActionQueue`, `LuaTransaction`, rejestrację `noise`/`algo`/`geo`, demo terenu, RNG ground/doodad, tożsamość mapy i zapis plików pomocniczych OTBM.
- Doprecyzowano MVP, minimalne GUI, snapshot i konflikty, deterministyczność adaptera, ochronę obszaru, rollback, budżet pamięci i kryteria testów. Szczegóły i lokalizacje źródeł: sekcja 1.0 planu.
- Context7: `/websites/wxwidgets/3.3`, dokumentacja komunikacji worker–GUI i życia odbiorcy zdarzeń. To weryfikacja dokumentacji, nie test RME.

### Dowody i ograniczenia

- `rtk git status --short --branch`: brak repozytorium Git; w katalogu nie ma `.git`, jest `.gito`. Nie zmieniano metadanych, nie utworzono gałęzi ani commita, SHA nieustalone.
- Brak `.codegraph/`: zgodnie z instrukcjami nie wywoływano zapytań do nieistniejącego indeksu ani nie uruchamiano indeksowania.
- `rtk proxy ast-grep --version`: 0.45.1. Zapytanie strukturalne `g_gui.GetCurrentBrush()` potwierdziło zależność trzech przeciążeń rysowania od aktualnego brusha GUI.
- Użyto `rg` i odczytów źródeł przez `rtk proxy`. Nie zakładano, że deklaracje API dowodzą poprawności runtime.
- Kontrola dokumentów w PowerShell: poprawne odczytanie UTF-8 bez znaków zastępczych, domknięte bloki kodu, sekcje 0–14 i 12 odwołań do istniejących plików źródłowych/skryptów — OK. Pierwsza próba kontroli przez `python` nie uruchomiła się (brak interpretera pod tą komendą); kontrolę wykonano przez PowerShell.
- Build, uruchomienie GUI, smoke test Lua 20x20, Undo/Redo, round-trip OTBM, testy regresyjne i benchmarki: **nie uruchomiono**. Zmiany obejmują wyłącznie dokumentację.

### Stan faz

| Faza | Status | Commit | Dowody/testy | Blokery lub zależności | Następne zadanie |
|---|---|---|---|---|---|
| 0 | IN PROGRESS | Audyt `b2bc871`, naprawa `a71fe4b` | Build, rollback tworzenia tile i mieszana historia PASS; częściowy round-trip pięter, UID/AID, flag i spawnu PASS | Bezpośrednia zmiana AID nie jest śledzona; Save As nadpisuje współdzielone XML; domy/strefy i odbiorca zewnętrzny niezweryfikowane | Naprawa obu błędów i pozostałe fixture |
| 1 | TODO | — | — | G0 | MVP jednego piętra |
| 2 | TODO | — | — | G1 | Teren i bordery |
| 3 | TODO | — | — | G2 oraz referencje | Analiza referencji |
| 4 | TODO | — | — | G3 oraz holdout | Profile L1/L2 |
| 5 | TODO | — | — | G4 | Topologia wielu pięter |
| 6 | TODO | — | — | G5 | Ranking i feedback, opcjonalny ML |
| 7 | TODO | — | — | G6 | Rozbudowa GUI |
| 8 | TODO | — | — | G7 | Szersze pomiary i niezawodność |
| 9 | TODO | — | — | G8 | Beta i wydanie |

### Historyczne zadania z przeglądu przygotowawczego (aktualny plan powyżej)

1. Ustalić pochodzenie kopii, dostęp do właściwego checkoutu Git i SHA; utworzyć gałąź z `master` i zapisać lokalne zmiany. CodeGraph wykorzystywać po udostępnieniu indeksu.
2. Potwierdzić toolchain, dostępne assets i mapę testową; wykonać kanoniczny build Windows x64 i zapisać wynik, bez deklarowania powodzenia z wyprzedzeniem.
3. Przeprowadzić smoke test na kopii kompletu plików (20x20, Undo/Redo, zapis/odczyt), a wyniki audytu i plan integracji umieścić w `docs/procedural/ARCHITECTURE.md` oraz `docs/procedural/TEST_PLAN.md`.
