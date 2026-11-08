# G4-B — schemat profilu stylu i mapowanie klucz gruntu → rodzaj, 2026-11-07

**PARTIAL.** Schemat, walidacja, serializacja, odmowa niezgodnego klienta/assetów/poziomu i projekcja statystyk przez mapowanie są zbudowane i przetestowane. **Propozycja mapowania dla 10.98 czeka na Twoją akceptację** (lista niżej); do czasu akceptacji nie jest to dane, na których wolno dopasowywać generator. Profile nie są jeszcze wypełniane z referencji (to G4-C).

## Co jest
- `source/procedural/core/style_profile.*`:
  - `StyleKind`: Grass, Dirt, Rock, Shore, Water, Other (ważny grunt bez rodzaju generatora: podłogi, dachy, schody, zagrożenia, grunty biomów), Unmapped (żadna reguła nie pasuje; raportowane, nie zgadywane), NoGround.
  - `proposeKeyMap` z regułami po **całych słowach** nazwy brusha (bez ID, bez rozróżniania wielkości liter, w kolejności priorytetu: najpierw kwalifikatory „roof/floor/hole…”, potem Water, Rock, Dirt, Grass, Shore). Reguły są danymi w kodzie (`defaultKeyRules`), nie ID itemów.
  - `serializeKeyMap`/`parseKeyMap` (`g4-keymap/1`, ręcznie edytowalny plik), `StyleProfile` (`g4-profile/1`: klient, SHA-256 `items.otb` i `Tibia.dat`, poziom surface/underground z zakresem pięter, źródła z wagami, mapowanie, statystyki całkowitoliczbowe), `validateProfile`, `parseProfile`, `checkProfile`.
  - `accumulateKindStats`: kafle na rodzaj i pary sąsiadów (wschód/południe) dla zadanego zakresu pięter; klucz mapy nieobecny w mapowaniu to błąd, nie `Unmapped`.
- Narzędzie `style_keymap` (tylko rdzeń): z raportów G3 (`ground_keys.csv`) buduje `keymap.tsv` i tabelę przeglądową Markdown.
- Test `test_procedural_style_profile` PASS: reguły na 25 nazwach z danych (m.in. „dirt mountain ground” = Rock, „sandstone” ≠ „sand”, „grassland” ≠ „grass”, „grass pitfall” = Other, „snow” = Unmapped), pełny round-trip, jednoznaczność serializacji, odmowy (inna wersja schematu, brak sekcji, nieznany rodzaj/poziom, nadmiarowa linia, zdublowany klucz i klient, tab w kluczu, zakres pięter niezgodny z poziomem, puste pola), zgodność (klient, `items.otb`, `Tibia.dat`, piętro poza poziomem), statystyki policzone ręcznie na mapie 3×2 + piętro 8 (w tym kafel bez gruntu, brak sąsiada, anulowanie, odwrócony zakres).

## Propozycja mapowania 10.98 (do akceptacji)
108 kluczy z 19 map; tabela liczona na 12 mapach stylu, wszystkie piętra (nie tylko powierzchnia), plik `local-data/procedural-smoke/g4-keymap/a/keymap-review.md` (dwa przebiegi bajtowo identyczne).

| Rodzaj | Klucze | Kafle | Udział (‰) |
| --- | ---: | ---: | ---: |
| Grass | 7 | 790 325 | 80 |
| Dirt | 8 | 4 398 960 | 445 |
| Rock | 8 | 672 583 | 68 |
| Shore | 5 | 176 750 | 17 |
| Water | 3 | 1 649 192 | 167 |
| Other | 71 | 1 009 754 | 102 |
| Unmapped | 5 | 23 733 | 2 |
| NoGround | 1 | 1 149 443 | 116 |

**Decyzje do potwierdzenia (to założenia, nie fakty o klientach):**
1. `sand` (171 k kafli, głównie 2 mapy) = **Shore**. W mapach pustynnych to grunt główny, nie pas przy wodzie.
2. `sea`, `clear water`, `walkable sea` = **Water**. Klucz nie mówi, czy to jezioro czy rzeka; podział na Lake/River dopiero z cech kształtu (D9) w G4-C.
3. `cave`, `swamp`, `lava`, `tar pit` = **Other** (brak rodzaju w generatorze). `earth` i `dirt` = **Dirt**: `earth` ma 4,2 mln kafli w 3 mapach (głównie M06), co sprawia, że Dirt to 44,5 % kafli; wagi per mapa w G4-C to adresują.
4. **Unmapped** (23,7 k kafli, 2 ‰): `snow`, `ice`, `white gravel`, `melting ground`, `ice closed stone pile`. Czy `snow`/`ice` mają być rodzajem (biom zimowy), czy zostają poza modelem?
5. **NoGround = 11,6 % kafli** (kafel bez gruntu z brusha): nie zbadano, co to jest (np. itemy bez gruntu, ściany); do obejrzenia przed użyciem.

## Czego NIE zweryfikowano
- Reguły to heurystyka po nazwie; niczego nie sprawdzono w definicjach itemów ani materiałach (np. czy `earth` to w tym kliencie ten sam materiał co „dirt” w BlackTek). Mapowanie dla BlackTek nie istnieje.
- Statystyki w tabeli są z wszystkich pięter, nie z powierzchni; profil dla z≤7 liczy dopiero G4-C z `SemanticMap`.
- Nie ma profilu wypełnionego danymi, więc nie ma jeszcze testu odmowy na prawdziwym profilu ani pomiaru czasu/pamięci `accumulateKindStats` na dużej mapie.
