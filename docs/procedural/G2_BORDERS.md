# G2-B — resolver powierzchni i istniejące bordery, stan na 2026-11-06

**PARTIAL.** Resolver v2 i istniejący `GroundBorderCalculator` działają razem na rzeczywistym kliencie 1098-BlackTek w próbniku. Nie ma jeszcze wpięcia w Preview, Commit ani dialog (G2-B2), więc nie ma wyniku w RME i nie zaliczono kryteriów „czytelny w RME” ani „brak krytycznych błędów borderów”.

## Zakres

- `source/procedural/adapter/surface_resolver.h/.cpp`: `SurfaceGrid` → identyfikatory itemów. Profil w kolejności rodzajów: `grass`, `dirt`, `rocky ground`, `sand`, `sea` (jezioro), `sea` (rzeka). Walidacja brushy jak w resolverze G1, osobny strumień losowania na rodzaj. Kod resolvera G1 i `GroundProfile` (trzy brushe) nie były zmieniane; część walidacji jest zduplikowana.
- `tests/test_procedural_surface_resolver.cpp`: próbnik (wxModule) budowany i uruchamiany przez `g1_run_resolver_probe.py --surface`. Mapa syntetyczna, żadnych plików map. Bordery liczy wyłącznie `GroundBorderCalculator::calculate(…, true)`, bez własnej logiki.

## Wynik

Przebieg `g2-surface-resolver-2-20261106` (profil `g0-reference-profile-20261030`, hashe wejść niezmienione, stderr pusty, exit 0): `SURFACE_RESOLVER_PASS`. Pierwszy przebieg `g2-surface-resolver-20261106` padł na wczytaniu klienta (`build-ninja/config.toml` ma puste ścieżki klienta), zanim wykonał się kod resolvera; zostaje w `local-data` bez zaliczenia.

Sprawdzone asercjami: powtarzalność planu, pochodzenie (klient, seed, rewizja assets), item każdej komórki należy do brusha przypisanego rodzajowi, maska nie zmienia wariantu innej komórki, bordery nie zastępują ani nie gubią położonego gruntu, drugie przejście borderów nie zmienia stosów, bordery nie sięgają poza jednokaflowe halo, a w obszarze są kafle z więcej niż jednym borderem.

| Rozmiar, seed | Kafle | Z borderem | Z ≥2 borderami | Maks. stos | Pary różnych brushy | Pary bez bordera | Zmienione kafle halo | Resolver | Bordery |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 100, 1 | 10 000 | 2151 | 1076 | 5 | 3365 | 0 | 95 | 0,50 ms | 6,2 ms |
| 100, 2 | 10 000 | 2325 | 1064 | 5 | 3112 | 0 | 0 | 0,47 ms | 5,4 ms |
| 100, 3 | 10 000 | 2378 | 1191 | 5 | 3476 | 0 | 0 | 0,48 ms | 5,5 ms |
| 100, 4 | 10 000 | 2265 | 979 | 5 | 3029 | 0 | 0 | 0,46 ms | 5,3 ms |
| 250, 1729 | 62 500 | 12 866 | 7075 | 5 | 20 943 | 0 | 50 | 2,7 ms | 38 ms |
| 500, 1729 | 250 000 | 47 699 | 26 106 | 6 | 77 147 | 0 | 0 | 10,3 ms | 165 ms |

Czasy to pojedyncze przebiegi, bez powtórzeń i bez narzutu `DrawOperations`.

## Znaleziska

- **Bordery na kaflach poza obszarem.** Pierścień trawy wokół obszaru był położony tylko w dwóch przebiegach (100×100 seed 1 i 250×250 seed 1729). W nich krawędź obszaru o innym rodzaju niż sąsiad dopisała bordery na kafle halo poza obszarem: 95 i 50 kafli. W pozostałych przebiegach nie było żadnych kafli halo, więc „0” tam nic nie mówi o zachowaniu przy sąsiedzie. Obecny potok G1 odrzuca taki podgląd (`OutOfMask`: „Borders would extend outside the allowed area”), więc przy wpięciu v2 generowanie obok istniejącego terenu będzie często odrzucane. Rozstrzygnięte 2026-11-07 (patrz [G2_GATE_STATUS.md](G2_GATE_STATUS.md), sekcja D5); dawniej: do rozstrzygnięcia w G2-B2 (np. kształtowanie pierścienia brzegowego obszaru); wpisano jako D5 w [TECH_DEBT.md](TECH_DEBT.md).
- Kafle poza jednokaflowym halo nie zmieniły się nigdy (asercja).

## Czego nie zweryfikowano

- Wyglądu w RME. „Pary bez bordera = 0” znaczy tylko, że przy każdej parze sąsiadów o różnych brushach przynajmniej jeden z dwóch kafli ma item border; nie dowodzi, że krawędź jest poprawna, ciągła ani ładna.
- Wąskiej rzeki o szerokości jednego kafla: nie wyodrębniono jej w metrykach.
- Ścieżki `DrawOperations` (budżety pamięci, sandbox, diff), zatwierdzania i Undo/Redo dla v2.
- Wpływu na różne seedy poza sześcioma przebiegami; tylko jedna konfiguracja parametrów (domyślna).
- Żadnych innych klientów niż 1098-BlackTek.
