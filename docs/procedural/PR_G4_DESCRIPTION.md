# PR: generator terenu, etap G4 (uczenie stylu) i kształt terenu, szlaki, budynki

Szkic opisu PR przygotowany 2026-11-08. **PR nie został otwarty ani wypchnięty**; gałąź `feature/procedural-g4-style-learning` jest tylko lokalna. Porównanie z `origin/master` (punkt rozgałęzienia `33b5c88`, PR #4): 74 commity, 72 plików, +8 809 / −84 wierszy, z czego `source/` i `tests/` to 55 plików (+7 440 / −65), reszta to dokumentacja i skrypty sond.

## Co to jest

Generator powierzchni (Natural surface) uczy się stylu z prywatnych map referencyjnych i generuje teren zbliżony do nich. Zmiany w tej gałęzi:

1. **G4: profile stylu** (`source/procedural/core/style_*.cpp`, `reference_split.*`, `similarity.*`, narzędzia w `source/procedural/tools/`): odciski map, podział na rodziny, mapowanie klucz gruntu → rodzaj, dopasowanie parametrów generatora, profile per styl z paletami brushy i doodadów, oferta profili w oknie generatora.
2. **Kształt terenu** (`surface.cpp`, `water_shape.*`, `scale_stats.*`): meander rzek, duże struktury (`featureSize`, więcej oktaw szumu), szersza plaża, obwódka trawy wokół skały i plaży.
3. **Opcjonalne dodatki** (domyślnie wyłączone, ustawiane w oknie generatora): szlaki (`pathLength`, siódmy pędzel), budynki z podłogą (`buildingDensity`, ósmy pędzel) i ścianami (`GroundPlan::walls`, pędzel ściany wyrównywany przez `WallBrush::doWalls`).
3a. **Osady (FAZA 5 cz. 1):** budynki w skupiskach, drzwi (`SurfaceGrid::doors`, `GroundProfile::door`, `GroundPlan::doors`, pola „Doors” i „Door kind” w oknie generatora), szlaki do drzwi (`PathRouter`), metryki osad (`settlement_stats.*`), zestaw regresji `docs/procedural/smoke/g5_run_suite.py`. Wynik dla `buildingDensity` 0 bez zmian bajt w bajt.
4. **Metryki i dokumentacja:** `docs/procedural/G4_*.md`, `G4_METRICS.md` (kryteria zapisane przed pomiarami), `G4_GATE_STATUS.md` (wyniki z zastrzeżeniami), `TECH_DEBT.md` (D17–D21), narzędzie `style_render` (podgląd generatora jako PNG), rozszerzenia sondy referencji.

Mapy referencyjne są prywatne: nie ma ich w repo, katalog `local-data/` jest w `.gitignore`, nie ma kopii kafli w dokumentach (tylko zbiorcze liczby).

## Zmiany zachowania istniejącego kodu (do uwagi recenzenta)

- **Wynik Surface zmienił się dla wszystkich profili i ustawień domyślnych** (świadomie, opisane w `G4_GATE_STATUS.md`): reguła pierścieni plaży (próg szumu ≤ 40 %, jezioro ≥ 90 komórek, `shoreWidth` do 8), obwódka trawy wokół Rock i Shore (iteracja 10c), więcej oktaw szumu wysokości przy `featureSize` > 24 (dla ≤ 24 bez zmian). Przypięty odcisk `Seed 1729 100×100` w `tests/test_procedural_surface.cpp` zmieniony z `6783381837833077004` na `17159159863505677800`.
- **Rzeki z `meander` > 0** używają nowej ścieżki (`meander` 0 daje wynik bajt w bajt jak przed zmianą).
- **`GroundProfile`** ma do 8 pędzli (6 bez szlaków, 7 ze szlakami, 8 z podłogą) i pole `wall`; profile z mniejszą liczbą pędzli odrzucają siatki ze szlakami i podłogą (testy).
- **`SurfaceKind`** ma nowe wartości `Path` i `Floor`; `GroundPlan` nowe pole `walls` (puste = jak dotąd, fingerprint planu bez zmian).
- **Okno generatora**: pola „Style profile”, „Path length”, „Path brush”, „Buildings”, „Floor brush”, „Walls”, „Wall brush”.
- **Błąd naprawiony przed PR** (`fadc036`): kolejność pól `SurfaceTuning` powodowała, że `meander` z profilu trafiał do `pathLength`/`buildingDensity`; profile nie stosowały meandru od iteracji 9 (opis w `G4_GATE_STATUS.md`). To zaniedbanie procesu: pełny zestaw testów nie był uruchamiany po każdej zmianie.

## Co zweryfikowano

Uruchomione 2026-11-08 na bieżącym kodzie:

- **14 testów rdzenia** (`test_procedural_core`, `_surface`, `_reference_header`, `_semantic_map`, `_topology`, `_features`, `_semantic_image`, `_similarity`, `_style_profile`, `_style_fit`, `_decor`, `_style_profile_set`, `_water`, `_scale`): wszystkie PASS (kod wyjścia 0).
- **Sonda G1 `--commit`** na rzeczywistym kliencie 10.98: `passed: true`, `stderr` pusty.
- **Sonda G1 `--surface`**: wszystkie asercje PASS, w tym nowy test e2e struktur (`SURFACE_STRUCTURES_E2E_PASS` dla 100×100: 150 komórek ściany; 250×250: Preview, Validate, Commit, Undo, Redo, odmowy dla profili z niepasującą liczbą pędzli i pędzla ściany będącego gruntem). `passed: false` tylko z powodu niepustego `stderr` (ostrzeżenia wxWidgets `SetFocus` z D1/D2: 9 ostrzeżeń, po jednym dodatkowym na uruchomienie dodanych przebiegów e2e; wcześniej 7).
- **Ocena użytkownika w GUI** (subiektywna, bez metryki): profile stylów, plaża, duże struktury, rzeki z meandrem, skała, obwódka trawy, szlaki, budynki ze ścianami (w tym Validate, Undo/Redo ze szlakami i ścianami).

## Czego NIE zweryfikowano / znane ograniczenia

- **Bramka G4 z planu (poprawa nad ręcznymi presetami na mapach niewidzianych) nie jest spełniona**; opis i liczby w `G4_GATE_STATUS.md` (iteracje 1–3). Jedyny dowód na mapie niewidzianej: swamp, n = 2.
- Metryki kształtu (iteracje 4–11) opierają się na dwóch referencjach (M06, M19), in-sample (D17); kilka kryteriów jest świadomie niespełnionych: rozgałęzienia szlaków (artefakt szkieletowania, D18), skład granic dla m19, E64 przy boku 256, granica skały.
- Brak dachów i wnętrz budynków, mostów, podziemi i wielopiętrowych gór (D9, D19). Drzwi, skupiska i szlaki do drzwi są (FAZA 5 cz. 1, iteracje 12a–12h w `G4_GATE_STATUS.md`): jedne drzwi na budynek przez `DoorBrush`, trasy dochodzą do celu, bez kikutów; ocena wzrokowa użytkownika „lepiej”, ale długość szlaków w m19 jest poza zapisanym pasmem (174 wobec ≤ 117) i kryterium 12c jako całość nie jest spełnione; wrażliwość stałych nie była badana.
- Profile stylu są stare względem bieżącego generatora (D20): `style_styles` nie dopasowuje `featureSize`, `shoreWidth`, `transitionWidth`, `meander` na nowych metrykach.
- Wydajności nowych etapów (szlaki, budynki, obwódka) nie mierzono osobno; w oknie 500×500 podgląd działał w GUI.
- Tylko Windows x64 i klient 10.98 (profil prywatny `1098-BlackTek` w sondach).
- Skrypty sweepów użyte do tabel w `G4_GATE_STATUS.md` nie są w repo (D21).

## Jak przejrzeć

Proponuję czytać w tej kolejności: `docs/procedural/G4_GATE_STATUS.md` (co jest dowiedzione i jak), `G4_METRICS.md` (kryteria zapisane przed pomiarami; przy 9c zapis powstał po pomiarze, opisano), potem kod: `source/procedural/core/surface.cpp` (reguły terenu), `source/procedural/adapter/surface_resolver.cpp` i `source/editor/operations/procedural_draw_operations.cpp` (warstwa ścian), `source/ui/dialogs/terrain_generator_dialog.cpp` (nowe pola).

Podział na kilka PR rozważałem i **nie polecam**: zmiany w `surface.cpp`, resolverze i dialogu są poprzeplatane (te same pędzle i kinds), a testy rdzenia przypinają odcisk wyniku po wszystkich zmianach. Jeśli recenzja wymaga mniejszych kawałków, naturalne cięcia to: (1) profile stylu i narzędzia, (2) kształt terenu, (3) szlaki, (4) budynki i ściany.

## Jak przetestować ręcznie (GUI)

1. Skopiuj profile do `%APPDATA%\Remere's Map Editor\data\procedural\styles` (pliki `*.profile` z lokalnego `g4-profiles`, nie z repo).
2. Map → Generate terrain…, tryb Natural surface, wybierz profil (np. m06 lub m19), obszar 500×500.
3. Opcjonalnie: „Path length” 80, „Buildings” 30 i „Walls”.
4. Preview, Validate, Place on map, Undo, Redo.

## Wymagane polecenia do powtórzenia

```bash
cmd /c build_ninja.bat
# testy rdzenia: build-ninja/source/procedural/test_procedural_*.exe (każdy kończy kodem 0)
py -I docs/procedural/smoke/g1_run_resolver_probe.py <OUT> <PROFILE_DATA> <CONFIG> --commit
py -I docs/procedural/smoke/g1_run_resolver_probe.py <OUT> <PROFILE_DATA> <CONFIG> --surface
```
