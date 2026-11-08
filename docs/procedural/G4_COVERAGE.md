# G4 — pokrycie gruntu (D13), 2026-11-08

**PARTIAL.** Przyczyna D13 znaleziona i usunięta opcją normalizatora. Pokrycie wzrosło z 5 do 9 użytecznych map na powierzchni. Nowe przypisania (96 kluczy `item …`) są **propozycją czekającą na Twoją akceptację**; wyniki poniżej liczono na tej propozycji.

## Przyczyna
Klucz gruntu w mapie semantycznej to nazwa brusha, który posiada ground item (`getGroundBrush()`). W `data/` 10.98 wiele prawdziwych gruntów nie należy do żadnego brusha: warianty `earth` (id 8133 i 10164: razem 156 k kafli, każdy id w jednej mapie), `mountain`, `sand`, `gray sand`, `swamp`, `lava`, `flowing water`, a w mapach jaskiń „ścienne” grunty (`stone wall`, `dirt wall`, `sandstone`). Wszystkie dostawały klucz pusty („bez gruntu”). Zmierzono: 650 różnych item id, 788 876 kafli (z 14 mln) w 19 mapach, 88 % w M06 i M19.

## Naprawa
- `normalizeReference` ma opcję `NormalizeOptions::nameUnbrushedGrounds` (domyślnie wyłączona): ground bez brusha dostaje klucz `item <nazwa z definicji klienta>`. Nic nie jest zgadywane po id; nazwa pochodzi z załadowanego klienta. Podsumowanie importu zawiera też `groundWithoutBrush` (id, liczba, nazwa, client id, blokowanie); sonda zapisuje je jako `ground-without-brush.csv` (agregaty, bez współrzędnych).
- Reguła słowa `wall` → Other w `defaultKeyRules` (przed dirt/earth), bo „dirt wall” to ściana jaskini, nie ziemia. Test rozszerzony.
- **Brak regresji G3:** z opcją wyłączoną 190 plików raportów 19 map jest bajtowo identycznych z oryginałem G3 (0 różnic), na końcowym binarium. Z opcją włączoną dwa przebiegi (z mapowaniem i bez) dały identyczne pliki analizy i odciski.

## Wynik
- Kafle bez gruntu (NoGround) na mapach stylu: 1 149 443 (116 ‰) → 365 646 (37 ‰). Klucze: 108 → 204 (96 nowych `item …`; 108 starych bez zmiany rodzaju). Nowe Unmapped: 33 klucze (m.in. `item basalt`, `item rock soil`, `item small brook`, `item wasteland`, `item sandy tile`, `item stone tile`), razem 5 ‰ kafli.
- Mapy z ≥ 1000 kafli naturalnych na z≤7: M01 (15 153, sama woda), M05, M06, M11, M12 (3 436), M13 (6 808), M14, M15 (6 297), M19 = **9** (było 5). M16 (83), M17 (0 naturalnych: 8 774 Other i 7 305 Unmapped, m.in. miasto pod wodą), M18 (693) nadal poniżej progu.
- **Podział zmienił się** (odciski liczą nazwy gruntów, więc okna z pustego klucza przestały „łączyć” mapy): 0 scaleń po treści (było 4), 9 rodzin i 9 foldów, przeciek test–reszta ≤ 11 ‰ (było ≤ 45 ‰). Wcześniejszy wynik G4-A (siedmiomapowa rodzina) był w dużej części artefaktem pustych kluczy; zostaje w historii jako pierwszy przebieg.
- **Dopasowanie** (te same siatki, seedy 1–6, bok 160; dystans L1 do celu walidacji, ppm): poprawa na walidacji w 6 z 8 foldów z celem (−4 % do −78 %), w 2 foldach wygrywa ręczny baseline (fold M01: 1 648 968 vs 3 275 353; fold zima: 3 002 414 vs 3 876 681). Wcześniej poprawa 3–7 % w 2 z 4 foldów. Dwa przebiegi `style_fit` bajtowo identyczne. Fold testowy M15 nie ma celu walidacyjnego (walidacja M17 ma 0 naturalnych).

| Fold (test) | Walidacja: baseline → wybrany krok |
| --- | ---: |
| caves (M14, M16) | 3 710 733 → 2 815 964 |
| M01 | 1 648 968 → 1 648 968 (baseline) |
| M05 | 2 186 449 → 2 092 742 |
| M06 | 3 582 804 → 2 666 482 |
| M15 | brak celu walidacyjnego |
| M17 | 2 399 716 → 525 138 |
| M19 | 3 473 698 → 2 828 419 |
| swamp (M12, M13) | 3 407 994 → 2 377 098 |
| zima (M11, M18) | 3 002 414 → 3 002 414 (baseline) |

- Wartości dopasowane nadal sięgają krańców siatek (jezioro 300 ‰, skała 320 ‰); to dopasowanie udziałów, nie ocena wyglądu.

## Do akceptacji
96 kluczy `item …` w `local-data/procedural-smoke/g4-keymap/proposed-10.98-v2.tsv` (przegląd: `…/c/keymap-review.md`). Tak jak w G4-B: kafle w naturalnych rodzajach to Grass 790 733, Dirt 4 566 080, Rock 728 390, Shore 213 570, Water 1 666 777, Other 1 489 624, Unmapped 49 920. Do decyzji: `item gray sand` i `item sand` = Shore (jak `sand`), `item earth` = Dirt, `item mud` = Dirt, `item flowing water` = Water, `item swamp`/`item lava` = Other, `item rock soil` = Unmapped.

## Czego NIE zweryfikowano
- Że nazwa itemu z definicji klienta odpowiada rodzajowi gruntu (np. wszystkie „mountain” to skała); reguły to nadal heurystyka po słowach.
- Że 10.98 w `data/` ma rzeczywiście wszystkie brushe, jakich używały te mapy (brak brusha mógł być też świadomym wyborem autora).
- Nie zmieniono nic w ścieżce edytora; opcja działa tylko w imporcie referencji. Test GUI nie dotyczy.
- Nowe profile i podział policzono na jednej maszynie.
