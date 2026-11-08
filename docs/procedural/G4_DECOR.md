# G4-D — reguły kontekstowe wegetacji (klasyfikacja doodadów po nazwie brusha), 2026-11-08

**PARTIAL, dowody wegetacji są cienkie.** Mechanizm (klasyfikacja po nazwie, statystyki kontekstowe, strojenie dekoracji, dopasowanie zamknięte) działa i jest powtarzalny. Na walidacji dopasowanie wegetacji poprawia dystans w 2 z 4 foldów z porównywalnym kontekstem, w 2 pogarsza. Nic nie liczono na zbiorze testowym.

## Co jest
- `doodad_class`: reguły po całych słowach nazwy brusha → Tree / Bush / Stone / None (None = nie wegetacja i nie kamień dla generatora: meble, pola ognia, posągi, gruz). Wynik na 247 brushach stylu: Tree 13 brushy (20 032 szt. na z≤7), Bush 19 (148 919), Stone 13 (19 835), None 202 (44 291). Lista do przeglądu: `local-data/procedural-smoke/g4-keymap/d/doodad-review.md`.
- Trzy nowe bity cech w mapie semantycznej (`TreeDoodad`, `BushDoodad`, `StoneDoodad`, poza raportowanymi 16): raporty G3 bez zmian (liczą tylko 16 cech): 190 plików 19 map bajtowo identycznych z oryginałem (0 różnic) na końcowym binarium.
- `decoration`: `DecorTuning` (6 szans w ‰: skala drzew, drzewa przy wodzie, krzak, krzak przy wodzie, kamień na ziemi, kamień przy skale). Domyślny `DecorTuning` daje bajtowo ten sam wynik co dotychczasowa reguła (test na 3 seedach); wartości poza zakresem są odrzucane (`InvalidParameters`). Wzorce klastrów lasu (szum) i zasięg 2 kroków bez zmian.
- `decor_context`: liczniki kafli trawy i ziemi (rzeczywiste: z mapy semantycznej przez zaakceptowane mapowanie; generator: z `DecorGrid`) w 8 kontekstach (trawa/ziemia × blisko wody lub brzegu × blisko skały, zasięg 2 kroki 4-sąsiedztwa, ta sama definicja po obu stronach), cel z wielu map (każda rodzina tyle samo, każda mapa w rodzinie tyle samo, mapy poniżej 200 kafli w kontekście nie liczą się), odległość (średnia różnic 24 stawek w ‰ po kontekstach porównywalnych), dopasowanie zamknięte (każda szansa = stawka referencji w jej kontekście; skala drzew = stosunek stawki referencji do stawki wbudowanych reguł na tej samej powierzchni).
- Profil `g4-profile/3` (dodano `decor`: cel i strojenie). `style_fit` dopasowuje wegetację z map treningowych, a mapy walidacyjne tylko mierzą.
- Testy: `test_procedural_decor` PASS (m.in. dopasowanie zbliża się ≥ 4× do celu wygenerowanego znaną konfiguracją), `test_procedural_style_profile` PASS, `test_procedural_surface` PASS bez zmian. Dwie serie `style_fit` bajtowo identyczne (35 s każda).

## Dane wegetacji w referencjach (powierzchnia, mapy stylu)
Dekoracja istnieje w praktyce tylko w **M06, M19 i M05** (trawa); M12, M14–M18 mają łącznie < 400 kafli trawy/ziemi. Sumarycznie (‰ kafli): trawa daleko od wody i skał: drzewa 22, krzaki 154, kamienie 4 (626 tys. kafli); trawa blisko wody: drzewa 11, krzaki 78 (89 tys.); ziemia daleko od skał: kamienie 18 (119 tys.); ziemia blisko skał: kamienie 30 (5 tys.). Wbudowane reguły generatora: krzaki 40‰ (60‰ przy wodzie), kamienie na ziemi 30‰ (80‰ przy skale), drzewa 0 przy wodzie. Reguła „drzewa przy wodzie = 0” jest więc w referencjach słabo, ale nie zupełnie, prawdziwa (11‰ na trawie).

## Wynik dopasowania na walidacji (dystans = średnia różnic stawek w ‰, suma po kontekstach porównywalnych)
| Fold (test) | Walidacja | Wyrazów | Wbudowane → dopasowane |
| --- | --- | ---: | ---: |
| M01 | M05 | 6 | 176 → 414 (gorzej) |
| M05 | M06 | 21 | 550 → 1 008 (gorzej) |
| M17 | M19 | 24 | 1 839 → 1 559 (lepiej) |
| zima | caves (M14, M16) | 3 | 19 → 3 (lepiej) |
| pozostałe 4 foldy | – | 0 | brak porównywalnego kontekstu |

Dopasowane strojenia dla wszystkich map stylu: skala drzew 437, drzewa przy wodzie 11‰, krzaki 172‰, krzaki przy wodzie 107‰, kamienie na ziemi 13‰, przy skale 29‰.

## Czego NIE zrobiono / nie zweryfikowano
- **Klasyfikacja po nazwie to heurystyka.** Do akceptacji: krzaki obejmują grzyby, trawy wysokie, kwiaty, `bamboo plant`, `lilypads`; kamienie to m.in. `kamienie`, `staglamites`; `leaves`, `debris`, `ramp`, `sand pile` są None.
- Ścieżki (potrzebne do reguły „ścieżki”) i przejścia biomów: nie wyodrębniono (D9).
- Zasięg wody/skały to stałe 2 kroki; nie badano innych.
- Dopasowanie zamknięte pomija zależności drugiego rzędu (np. krzak losowany po drzewie), więc stawki wygenerowane są nieco niższe niż zadane.
- Wpływ na wygląd: brak oceny wizualnej. Strojenie nie jest podpięte do GUI/`GenerationConfig` (zmiana tej struktury zmieniłaby odciski podglądu).
- Mała liczba map z wegetacją (3) nie pozwala na sensowny przedział niepewności.
