# G2-C — roślinność i detale zależne od terenu, stan na 2026-11-07

**PARTIAL.** Warstwa dekoracji (drzewa, krzaki, kamienie) działa w rdzeniu, resolverze, podglądzie, commicie z jednym Undo i Redo na rzeczywistym kliencie 1098-BlackTek (100×100 i 250×250), a regresje G1 przeszły. Nie sprawdzono dialogu ani wyglądu w natywnym RME, więc kryterium „czytelny w RME” pozostaje otwarte.

## Zakres

- Tylko algorytm Surface, opcja `GenerationConfig::vegetation` (domyślnie wyłączona; dialog: „Add trees, bushes and stones”). Classic nie ma dekoracji; przygotowanie odrzuca Classic z włączoną roślinnością.
- Rdzeń `source/procedural/core/decoration.h/.cpp`: `generateDecoration(SurfaceGrid, seed)` zwraca `DecorKind` (Tree, Bush, Stone) dla każdej komórki. Tylko arytmetyka całkowita, funkcja seeda i współrzędnych absolutnych. Reguły (stałe w kodzie, bez parametrów):
  - Drzewa tylko na trawie, nie bliżej niż dwa kroki (4-sąsiedztwo) od jeziora, brzegu lub rzeki. Skupiska i polany z szumu (krok 20): w lesie 11 % komórek, na polanach 0,6 %.
  - Krzaki na trawie, 4 % (6 % przy wodzie).
  - Kamienie na ziemi (3 %) oraz przy skale w odległości do dwóch kroków (8 %, także na trawie).
  - Nic na brzegu, w jeziorze, w rzece, na skale ani na komórkach `Preserve`.
- Adapter: `resolveSurfacePlan(…, decor)` mapuje rodzaj na brush doodad z `GroundProfile::decor` (trees, bushes, small rocks, nazwy z dialogu). Używane są wyłącznie pojedyncze itemy wariantu 0 (`getRandomSingleItem`); wielokaflowe kompozyty pomijane. Każdy osiągalny item jest sprawdzany (definicja, client id, nie meta, nie ground, nie border). Brak wpisanych na sztywno identyfikatorów. Osobny strumień losowania na rodzaj.
- Potok: `GroundPlan::decor` (jedna wartość na komórkę, 0 = brak); fingerprint planu dopisuje warstwę tylko gdy jest niepusta, więc plany bez roślinności zachowały fingerprint. Podgląd kładzie itemy po przejściu borderów (kalkulator borderów ich nie widzi), sprawdza zgodność z planem i dopuszcza w buforze tylko ground, bordery i zadeklarowane itemy dekoracji. Commit kopiuje bufor, więc to nadal jedna akcja z jednym Undo.

## Wyniki

| Sprawdzenie | Wynik |
| --- | --- |
| `test_procedural_surface`: determinizm, brak dekoracji poza trawą/ziemią, brak drzew przy wodzie, wpływ seeda, kamienie tylko na ziemi poza trawą przy skale | PASS (24 seedy 100×100, 6 seedów 250×250) |
| Udział drzew w lądzie | 3,1 % (100×100), 3,2 % (250×250); krzaki i kamienie występują we wszystkich seriach |
| `bad_alloc` w każdym z 7 punktów alokacji dekoracji (24×24) | 7 z 7 zgłoszone jako `AllocationFailed`, bez crasha |
| `--surface` na kliencie 1098 (`g2-veg1-surface-20261107`) | `SURFACE_RESOLVER_PASS`; fingerprinty resolvera powierzchni takie same jak przed zmianą; `SURFACE_E2E_PASS` bez roślinności (100, 250) oraz `SURFACE_VEGETATION_E2E_PASS` 100×100 (698 udekorowanych, 13,7 MB szacunku) i 250×250 (3824 udekorowane, 80,5 MB); Undo przywraca stan bez dekoracji, Redo ją odtwarza; odrzucone: brak nazw brushy, Classic z roślinnością, brush ground jako doodad, przełączenie opcji po przygotowaniu |
| `--preview`, `--commit` (G1, `g2-veg1-*`) | `passed: true`, stderr pusty, wiersze identyczne z [G1_COMMIT.md](G1_COMMIT.md) |
| `test_procedural_core` | PASS |

Runner `--surface` zapisuje `passed: false` z powodu stderr, jak wcześniej (D1, nie śledzono).

## Czego nie zweryfikowano

- Dialogu (pole wyboru, pola brushy, komunikaty) i wyglądu w natywnym RME. Czy gęstość i skupiska wyglądają dobrze, nie wiadomo.
- Dostępność nazw brushy `trees`, `bushes`, `small rocks` w innych klientach niż 1098-BlackTek. Brak brusha kończy się odmową, nie zgadywaniem.
- Wielokaflowych doodadów (duże drzewa, kompozyty): celowo poza zakresem.
- Osiągalności terenu: drzewa i część kamieni blokują przejście, nie badano wpływu na przechodniość.
- Przy domyślnym limicie Undo 40 MiB granica rozmiaru Place spada (roślinność dokłada jeden item na udekorowany kafel; 250×250 to około 80,5 MB przy szacunku, wobec 78,8 MB bez roślinności).
- Zależności dekoracji przy masce: odległość od wody i skały liczona jest z siatki bez komórek `Preserve`, więc przy masce komórki obok wyłączonego fragmentu mogą dostać drzewo, którego nie dostałyby bez maski. Reszta reguł nie zależy od maski.
- Czasu i pamięci poza szacunkami.
