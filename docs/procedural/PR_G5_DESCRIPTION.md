# PR: generator terenu, FAZA 5 część 1: osady, osiągalność, brody, szlaki, brzegi i układ okna

Szkic opisu PR przygotowany 2026-11-09. Gałąź `feature/procedural-g5-settlements` jest cięta z gałęzi PR #5 (scalonego w `master` jako `ec8664e`). Porównanie z `origin/master`: 62 commity, 33 pliki, +3 211 / −174 wierszy; z tego `source/` i `tests/` to 23 pliki (+2 313 / −167), `source/procedural/core` to 9 plików (+1 128 / −142), reszta to dokumentacja i skrypty.

## Co to jest

Dalszy ciąg generatora powierzchni (Natural surface). Wszystkie nowe opcje są **domyślnie wyłączone w rdzeniu** (wynik bajt w bajt jak dotąd); w oknie generatora włączone jest tylko „Smooth beaches” (patrz niżej).

1. **Osady (iteracje 12a–12h):** budynki w skupiskach zamiast losowych, jedne drzwi na budynek przez `DoorBrush` (`SurfaceGrid::doors`, `GroundProfile::door`, `GroundPlan::doors`), trasy do drzwi (`PathRouter`), metryki osad (`settlement_stats.*`), pola „Doors” i „Door kind” w oknie.
2. **Walidator osiągalności (13):** `reachabilityOf` mierzy składowe przechodnie, kieszenie i budynki zamknięte (`reachability.*`); wiersz `reach` w `style_render`.
3. **Brody (14, 14b–14d):** `SurfaceTuning::fordLength` łączy odcięty ląd piaszczystym przejściem przez rzekę lub jezioro (wlot na końcach, oś wychylona i zmienna szerokość dla dłuższych); pole „Ford length”.
4. **Szlaki przez skałę i rzekę (15, 16, 17):** `rockPasses` (szlaki mogą przechodzić przez pasma skały, cele po drugiej stronie), przy `fordLength` > 0 szlaki przechodzą przez rzeki piaszczystym brodem; pole „Rock passes”.
5. **Brzegi (18, 19, 20):** `shoreSmoothing` (filtr większościowy plaży, usuwanie wąskich języków piasku między wodą) i `shoreRadius`; pole „Smooth beaches”.
6. **Okno (21):** pola w zakładkach Area, Terrain, Paths, Buildings, Vegetation (jedna kolumna 35 pól nie mieściła się na 1080p).
7. **Narzędzia i dokumentacja:** `g5_run_suite.py` (wszystkie testy rdzenia i obie sondy G1 jednym poleceniem), `g5_shape_table.py` (tabele metryk, dwa przebiegi bajtowo identyczne), `g5_reference_settlements.py`; `G4_METRICS.md` (kryteria zapisane przed pomiarami), `G4_GATE_STATUS.md` (wyniki z zastrzeżeniami), `TECH_DEBT.md` (D19–D22).

Mapy referencyjne są prywatne: nie ma ich w repo, `local-data/` jest w `.gitignore`, w dokumentach są tylko zbiorcze liczby.

## Zmiany zachowania istniejącego kodu (do uwagi recenzenta)

- **Okno generatora:** układ w zakładkach; nowe pola „Doors”, „Door kind”, „Ford length”, „Rock passes”, „Smooth beaches”. **„Smooth beaches” jest domyślnie zaznaczone**, więc podgląd w oknie z ustawieniami domyślnymi wygląda inaczej niż przed PR (plaże wygładzone, wąskie języki piasku zamienione w wodę); w rdzeniu `shoreSmoothing` = 0 daje stary wynik.
- **Poprawiony błąd z 12h:** końce tras `reachEnds` były łączone na krzyż (trasa biegnie od celu do startu). Przy krótkich trasach do drzwi niewidoczne, przy długich szlakach dawało proste ukośne linie. **Zmienia to wyniki przy `buildingDensity` > 0** (trasy do drzwi mają prawidłowe końce); przy `buildingDensity` 0 nic się nie zmienia.
- **Nowe pola `SurfaceTuning`** (`rockPasses`, `fordLength`, `shoreSmoothing`, `shoreRadius`) dopisane na końcu struktury; nie są częścią profilu stylu. Odcisk konfiguracji planu dopisuje je tylko, gdy są ustawione. Odcisk `Seed 1729 100×100` (`17159159863505677800`) bez zmian.
- **`GroundProfile` i `GroundPlan`:** pole `door` w profilu, `doors` w planie (w odcisku tylko, gdy niepuste). Kontrola odcisku podglądu przyjmuje element klasy `Door` bez ID domu (RME zamienia drzwi w obiekt `Door`).
- **Limit pamięci roboczej** (`validateSurface`) rośnie o 28 B na komórkę tylko przy `fordLength` > 0.

## Co zweryfikowano

Uruchomione 2026-11-09 na bieżącym kodzie:

- **16 testów rdzenia** (w tym nowe `test_procedural_settlement`, `test_procedural_reachability` z ręcznie policzonymi układami i porównaniem z niezależnym union-find: 160 z 160): wszystkie PASS.
- **Sonda G1 `--commit`:** PASS. **Sonda G1 `--surface`:** PASS (przypadki `SURFACE_DOORS_E2E_PASS` 6 i 38 drzwi, `SURFACE_FORDS_E2E_PASS` z Preview, Validate, Commit, Undo, Redo; po Commit 0 blokujących kafli brodu i obok niego, kieszenie połączone na zatwierdzonej mapie; pomiar blokowania kafli według rodzaju: Rock 19 z 4983, Shore 0 z 7705). `passed: false` w runnerze tylko z powodu znanych ostrzeżeń `SetFocus` (12).
- **Powtarzalność:** dwa przebiegi tabel metryk bajtowo identyczne.
- **Ocena użytkownika w GUI** (subiektywna): drzwi i szlaki (12f–12h), brody (po 14d „jest dobrze”), szlaki przez skałę i rzekę („wyglądają ok”), brzegi po 19 („lepiej”), układ okna po 21 („wygląda ok”).

## Czego NIE zweryfikowano / znane ograniczenia

- **Kilka kryteriów z zapisu jest świadomie niespełnionych** (pełne liczby w `G4_GATE_STATUS.md`): 12c (cechy osad), 14 (zamknięte budynki dla zasięgu {8, 16, 32}: 5 z 32 seedów przy 32; zasięg ≥ 48 spełnia kryterium tylko po fakcie), 15/16/17 (miara szlaków przez skałę i „czubków” okazała się zawodna), 18 (reguła filtra zmieniona po pomiarze), 20 (schodki granicy to schodki siatki kafli, promień 5 daje −10 % narożników za +2 s, w oknie wyłączony).
- **Schodki i prostokątne wcięcia trawy w piasek** na granicy nie są rozwiązane.
- **Przechodność w grze nie była sprawdzana** (tylko flagi blokowania z danych klienta 10.98); nie ma mostów z osobnym przedmiotem; skała jest przechodnia, więc „przełęcze” nie są potrzebą osiągalności (skała blokująca z przełęczami: nie zrobiona).
- **Brak POI, „ustalonego startu”, pięter, przejść Z, grafu regionów, odrzucania seedów lub naprawy** (D22).
- **Bez pomiaru w referencjach:** szlaki przez skałę, brody, wygładzanie brzegów (nie ma ich w referencjach; kryteria bezwzględne).
- Tylko Windows x64 i klient 10.98 (prywatny profil w sondach). Układ okna sprawdzony wzrokowo przez użytkownika, ale bez osobnego opisu Preview / Validate / Place / Undo po zmianie układu; `test_universal_rotator` nie linkuje się przy pełnym `ninja` (inny cel, nie ruszany, nie badany).
- Wydajność nowych etapów (brody, szlaki przez rzekę) nie była mierzona osobno; `shoreRadius` 5 wydłuża generowanie 512×512 o około 2 s (jeden pomiar).

## Jak przejrzeć

Proponuję: `docs/procedural/G4_GATE_STATUS.md` (iteracje 12–21), `G4_METRICS.md` (kryteria zapisane przed pomiarami), potem kod: `source/procedural/core/surface.cpp` (router szlaków, brody, wygładzanie), `reachability.*`, `settlement_stats.*`, `source/editor/operations/procedural_draw_operations.cpp` (drzwi), `source/ui/dialogs/terrain_generator_dialog.cpp` (zakładki i nowe pola). Podział na kilka PR: naturalne cięcia to (1) osady i drzwi, (2) walidator i brody, (3) szlaki przez skałę i rzekę, (4) brzegi, (5) układ okna; zmiany w `surface.cpp` są jednak poprzeplatane, więc nie polecam.

## Jak przetestować ręcznie (GUI)

1. Map → Generate terrain…, tryb Natural surface, wybierz profil (np. m19), obszar 500×500.
2. Zakładka **Paths:** „Path length” 80, „Ford length” 48, zaznacz „Rock passes”. Zakładka **Buildings:** „Buildings” 30, Walls i Doors. Zakładka **Terrain:** „Smooth beaches” zaznaczone.
3. Preview, Validate, Place on map, Undo, Redo.

## Wymagane polecenia do powtórzenia

```bash
cmd /c build_ninja.bat
py -I docs/procedural/smoke/g5_run_suite.py --probes <PROFILE_DATA> <CONFIG>
py -I docs/procedural/smoke/g5_shape_table.py <PROFILE_DIR> <OUT_FILE> --extra fordLength=48
```
