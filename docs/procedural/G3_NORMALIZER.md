# G3-B — normalizator semantyczny, 2026-11-07

**PARTIAL (G3-B), ścieżka `unknown` zweryfikowana na syntetycznym fixture.** Mapa referencyjna jest zamieniana na semantyczną reprezentację strukturalną (klucz gruntu + maska cech na kafel, per piętro). Regiony, biomy i topologia to G3-C; bramka G3 nie jest zaliczona.

## Model
- Rdzeń `source/procedural/core/semantic_map.*` (bez zależności od edytora): `SemanticMap` trzyma per piętro gęsty prostokąt (klucz gruntu `uint16`, maska cech `uint32`, ok. 6 B/komórkę). `create` odmawia zakresów spoza pięter 0–15, duplikatów pięter i przekroczenia budżetu komórek; `set` odmawia komórek poza prostokątem. Bit `Present` ustawia `set()`, więc kafel bez żadnej cechy nadal istnieje.
- 16 cech: `has_ground, blocking, blocks_missiles, blocks_path, wall, door, table, carpet, border, floor_change, teleport, magic_field, container, doodad, loose_item, unknown`.
- Adapter `reference_import.*` (`normalizeReference`): cechy z definicji itemów załadowanego klienta (`ItemFlag`, `isWall/isDoor/isTable/isCarpet/isBorder`, `getDoodadBrush`, kontener, teleport, pole magiczne). **Żadnego wpisanego item ID.** Klucz gruntu to nazwa pędzla gruntu (`getGroundBrush()->getName()`) z klienta; grunt bez pędzla ma klucz pusty. Klucze są internowane alfabetycznie, więc tablica nie zależy od kolejności iteracji kafli. Itemy bez definicji lub nieprawidłowe w OTBM dają cechę `unknown` i trafiają do raportu `unknown_ids` z G3-A.
- Test rdzenia `test_procedural_semantic_map`: PASS (budżet dokładny i przekroczony, kolejność pięter, duplikat/pusty/zły zakres, kolejność kluczy, granice, kafel bez cech, powtarzalność podsumowania).

## Wynik na `forgotten.otbm` (klient 1098-BlackTek)
Źródło: SHA-256 `45031779…2304`. Raport lokalny `semantic-report.json` (SHA-256 `5C9C1024…2B4C`) w `local-data/`, nie w repo.

- 3 028 669 komórek w prostokątach 16 pięter (11 % pokrycia kafli); budżet próbnika 200 mln komórek.
- Cechy (kafle): has_ground 329 479, blocking 244 283, blocks_missiles 176 850, blocks_path 1 178, wall 13 572, door 683, table 494, carpet 2 678, border 52 045, floor_change 613, teleport 27, magic_field 152, container 431, doodad 16 916, loose_item 1 238, unknown 0.
- 77 kluczy gruntu (w tym pusty: 20 196 kafli, z czego 11 116 bez gruntu); suma kafli po kluczach 340 595 = liczba kafli.
- Cecha `blocking` oddziela wodę i lawę bez nazw: `sea` 45 033/45 034, `clear water` 1 289/1 289, `lava` 3 232/3 232 kafli blokujących, a `grass` 6 374/24 405 (blokują postawione na nim przeszkody).

## Znaleziony i naprawiony błąd
Pierwsza wersja uznawała kafel bez żadnej cechy za pustą komórkę; suma po kluczach (340 502) nie zgadzała się z liczbą kafli (340 595; brakowało 93). Naprawa: jawny bit `Present` ustawiany w `set()` i test „kafel bez cech nadal istnieje”. Po naprawie suma się zgadza.

## Powtarzalność
Dwa przebiegi przed naprawą i dwa po niej dają bajtowo identyczne `semantic-report.json` i `reference-report.json` (`0D538B97…77A0`, ten sam co w G3-A). Przebiegi kończą się same, stderr i `wx-log.txt` puste, 4,6–5,0 s wall (jeden komplet pomiarów, bez rozbicia).

## Czego NIE zweryfikowano
- Ścieżka `unknown` na prawdziwej mapie: `forgotten.otbm` ma 0 nieznanych itemów, więc `unknown` sprawdzono tylko na fixture syntetycznym (sekcja niżej), nie na mapie z realnymi brakami.
- Itemy wewnątrz kontenerów nie wnoszą cech (tylko sam kontener). Pliki towarzyszące (house/spawn/zones) nie są wczytywane.
- Dwa różne pędzle o tej samej nazwie zlałyby się w jeden klucz; nie sprawdzono, czy to występuje.
- `doodad` opiera się na `doodad_brush` definicji, którą kod opisuje jako „niekoniecznie doodad brush”; trafność tej cechy nie była oceniana ręcznie.
- Nie oceniano, czy 16 cech wystarcza dla G3-C/D ani czy klucze gruntu (77 nazw) dobrze grupują biomy; to zadanie G3-C.
- Pamięć: model ok. 18 MB wyliczone z liczby komórek, ale RSS procesu i szczyt wczytania `Map` nie były mierzone. Czas jest jednym pomiarem, nie benchmarkiem. Anulowanie przez `stop_token` nie istnieje (patrz G3_IMPORT.md).

## Fixture z `unknown` (2026-11-07)
`g3_make_unknown_fixture.py` buduje z syntetycznego `g0-metadata.otbm` (400 kafli trawy, piętro 7) nowy OTBM 3 `g3-unknown.otbm` z id 60000–60003, których klient nie ma: dwa grunty (60000, 60002), trzy razy ten sam nieznany item na stosie (60001, na trzech kaflach, w tym jednym razem z nieznanym gruntem), jeden kafel bez gruntu z samym nieznanym itemem (60003). Wartości oczekiwane zapisuje do `g3-unknown-expected.json`, a `g3_verify_unknown.py` porównuje je z raportami. Pliki fixture leżą w `local-data/` (nie w repo).

Wynik (klient 1098-BlackTek, dwa przebiegi, raporty bajtowo identyczne, SHA-256 `semantic-report.json` `4EDB6690…CF6A`):
- 8/8 kontroli weryfikatora PASS: 400 kafli na piętrze 7; `unknown_ids` dokładnie 60000×1, 60001×3, 60002×1, 60003×1; `unknown_ground` 2 + `unknown_items` 4 = 6; cecha `unknown` na 5 kaflach; `has_ground` 399; suma kafli po kluczach gruntu = 400 (trzy z kluczem pustym: dwa nieznane grunty i kafel bez gruntu).
- Kontrola negatywna: ten sam weryfikator na raporcie `forgotten` zgłasza 7 błędów (kod 1), więc potrafi wykryć niezgodność.
- Obserwacja: nieznany grunt jest zliczany jako grunt (`unknown_ground`), bo `Item::isGroundTile()` uwzględnia nieprawidłowe itemy „podobne do gruntu”. To zachowanie loadera, nie decyzja normalizatora.

Granice tego dowodu: id 60000+ są nieznane tylko dlatego, że klient 1098 ich nie ma (potwierdza to sam wynik, ale to nie jest niezależna kontrola), a fixture ma jedną postać brakującego itemu (inline w tile). Brakujące itemy zagnieżdżone w kontenerach i w węzłach `ITEM` z atrybutami nie były testowane.
