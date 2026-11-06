# G1-B — wejście semantyczne i warianty ground

Status: część integracji G1, nie pełny snapshot/resolver ani zaliczenie bramki.

`captureGenerationInput` w `source/procedural/adapter/generation_input.*` synchronicznie odczytuje wskazaną mapę na głównym wątku wx. Zwraca własne config/extent, identyfikator i rewizję mapy, rewizję VersionManager assets oraz dwa płaskie wektory maski i ochrony. Nie przechowuje referencji do żywych obiektów; wejścia można przenieść do workera rdzenia. Nie jest snapshotem tile/halo, pełnym dowodem nieaktualności preview ani zezwoleniem na commit. Odrzucenie inputu po reload assets sprawdzono osobno w [G1_RESOLVER.md](G1_RESOLVER.md).

Polityka `empty_only` chroni cały tile, jeżeli ma ground, stos przedmiotów, creature, spawn, house ID, mapflags, extra lub inne flagi stanu niż selected/modified. Chroni przez to także dowolne UID/AID, kontenery i nieznane dane, bez ich modyfikacji lub wybiórczego kopiowania. Oddzielnie sprawdza liczniki spawn/waypoint/town i niepuste house exits w TileLocation, również gdy nie istnieje Tile. Wyklucza cały zajęty tile zamiast próbować zachować tylko wybrane jego atrybuty.

Granice są przecięciem wymiarów nagłówka mapy i włącznie zdefiniowanych limitów Position, przeliczonych na granice wyłączne. Walidacja preceduje odczyty mapy i alokacje masek. `maxInputBytes` rozlicza `sizeof(GenerationInput) + 2 * cellCount`, czyli wyłącznie własny payload wejścia. Nie obejmuje allocator overhead, snapshotu/halo, sandboxu, rezultatu ani historii. Nie wolno traktować go jako pełnego budżetu G1. Anulowanie sprawdzane przed pracą i co 4096 pól; błędy nie zwracają częściowego wejścia i nie zapisują mapy.

`GroundBrush::getPlacementItemIdForRoll` udostępnia istniejący wybór wariantu dla jawnego roll z zakresu 1…totalChance. Zwykłe `draw` korzysta z tej samej funkcji i nadal pobiera roll z istniejącego RNG. Zachowano ścisłe `<` dla skumulowanych szans i historyczny fallback pierwszego ID; nie poprawiano rozkładu zwykłego malowania przy okazji G1. Nowa metoda nie tworzy itemów, nie borderuje i nie zmienia RNG. Resolver nadal musi zweryfikować aktywnego klienta, brush i wszystkie ID przez rzeczywiste definicje oraz wykonać placement/autoborder w sandboxie przez DrawOperations.

## Weryfikacja

Kanoniczny build i test rdzenia: `local-data/procedural-smoke/g1-input-canonical-20261102.log`, `g1-input-core-tests-20261102.log`. Wynik rdzenia PASS 1/1 w Release; niezmienione fingerprinty 100×100/500×500. Dodano przypadki domyślnych parametrów, malformed grid i anulowanej walidacji.

Probe `tests/test_procedural_ground_brush.cpp` korzysta z prawdziwych obiektów RME. Sprawdza mapę syntetyczną, własność masek, ochronę metadanych, brak mutacji, granicę budżetu, Cancel, odmowę z workera oraz dokładne granice wariantów i brak przesunięcia RNG edytora. Nie korzysta z klienta ani plików mapy. Runner:

```text
rtk proxy <python.exe> docs/procedural/smoke/g1_run_input_probe.py local-data/procedural-smoke/<nowy-katalog>
```

Pierwszy harness nie zlinkował się z powodu podwójnego `main`; log zachowano w `g1-input-rme-20261102/build.log`. Osobny builder G1 kompiluje wyłącznie prywatny obiekt application.cpp z przemianowanym wejściem, zachowując produkcyjne źródło, obiekt i EXE. Test ma własne console main i inicjalizuje wx bez Application::OnInit, okien, klienta ani głównej pętli. Build manifest wiąże aktualne źródła/obiekty i EXE probe. Nie jest to test GUI ani normalnego zamknięcia RME.

Poprawiony probe **PASS, exit 0, stderr pusty**: `g1-input-rme-entry-20261102/verification.json`, stdout i build manifest. Rzeczywisty test obejmuje 12 pozycji syntetycznej mapy, dziewięć wariantów ochrony oraz 10000 powtarzalnych wyborów brusha. Nie dowodzi pełnego snapshotu ani rozmiarów E2E. Test rdzenia dodatkowo przechwycił i obsłużył wszystkie pięć obserwowanych alokacji pojedynczej małej generacji; każdy z pięciu błędów zwrócił AllocationFailed, a ponowienie dokładny baseline. Log `g1-core-faults-tests-20261102.log`, CTest PASS 1/1 w Release. To jednorazowe błędy alokacji rdzenia, nie trwały OOM ani awaria resolvera/commitu.

Cały G1 nadal wymaga pełnego snapshotu z halo i assets, resolvera oraz kontroli konkretnego diffu, całkowitego budżetu, stale preview, atomowego commitu z iniekcją błędu, osobnego Undo/Redo, minimalnego dialogu i E2E z round-trip dla 100×100/500×500. G2 nierozpoczęte.
