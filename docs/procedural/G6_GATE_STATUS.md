# G6 — stan bramki

## I51b — przygotowany eksperyment (2026-11-18)

Na prośbę użytkownika zapisano przed implementacją i pomiarem eksperyment
zamiany priorytetu inBand/distance. Jedyny wariant B i kontrola A będą
oceniane na wspólnej nowej macierzy seedów 2001–2032. Kryteria, próg
23/32 dla obu profili i wymóg poprawy wobec A: G6_METRICS, sekcja I51b.
Nie wykonano implementacji ani pomiaru B. To eksperyment rozwojowy na
ujawnionym wcześniej holdoucie; ewentualny PASS nie zamknie G6.
Wynik I51 pozostaje FAIL. Ostatnie wyniki regresji poniżej dotyczą I51.

## I51 — pełna polityka, wynik niezaliczony (2026-11-17)

Kryteria uzupełniono przed implementacją i oceną w `396e057`.
Implementację Codex (medium, osobny worktree) przejrzano i scalono
fast-forward jako `0a9c127`. Ranking obejmuje 26 metryk i wersjonowane
pasma, rozróżnia definicyjne N/A od błędów danych, stosuje zamrożoną
odległość i agregację trzech rodzin holdoutu.

**I51_HOLDOUT_FAIL: m06 18/32, m19 18/32; wymagane 23/32 osobno.**
Baseline losowy: m06 127/256, m19 1/2. Nie zmieniono progu, pasm ani
generatora po wyniku. I51 i G6 pozostają otwarte.
Raport: [g6_i51_report.md](smoke/g6_i51_report.md), szczegóły w
[g6_i51_holdout.json](smoke/g6_i51_holdout.json).

Integrator ponownie uruchomił ocenę surowych raportów agenta: potwierdzono
18/32 dla obu profili. Testy niezależnego ewaluatora: 8/8 PASS.
Wykonawca zgłosił CTest 21/21 PASS i dwa bajtowo identyczne pełne przebiegi
macierzy z --g5-levels. Integrator nie powtarzał generowania całej macierzy.
Pokrycie holdoutu jest małe: znane 5–8 pozycji na mapę z 26; ograniczenie
kary dodatkowo zmniejsza liczbę faktycznie różnicujących pozycji.

Niezależna walidacja integratora:

- Kanoniczny build: BUILD SUCCESSFUL.
- Zestaw z sondami: SUITE_PASS, 19/19 rdzenia, --commit i --surface PASS,
  zasoby wejściowe bez zmian; 157 znanych ostrzeżeń SetFocus w surface.
- Pomiar `g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1`:
  G5_GATE_PASS, 32/32 dla obu profili, identyczne powtórzenia.
- CLI na przebudowanym narzędziu: CANDIDATE_RANK_CLI_PASS, odcisk
  17159159863505677800 zachowany.
- Ewaluator holdoutu: 8/8 PASS; ponowne obliczenie rzeczywistych raportów
  potwierdza I51_HOLDOUT_FAIL. Nie wykonano ponownej generacji macierzy.

Worktree wykonawcy nie zawiera build-ninja, brak katalogu do usunięcia.
Ręczne kroki GUI poniżej pozostają aktualne; ręczny test nie został wykonany.

## I51, 2026-11-17 — częściowa implementacja

Kryteria zapisano przed implementacją w `9149923`. Zmiany agenta Codex
(reasoning medium, osobny worktree) przejrzano i scalono fast-forward jako
`b828824` do `feature/procedural-g6-ranking`.

Dodano czysty ranking leksykograficzny, deterministyczne seedy, pomiary kształtu,
CLI offline oraz testy. Generator, GUI i zapis map nie zmieniły zachowania.
Raport wykonawcy: `source/procedural/tools/candidate_rank.md`.

**I51 i G6 pozostają otwarte.** Nie ma zatwierdzonych pasm m06/m19 ani
zaliczonego porównania holdout. Trzeba doprecyzować listę metryk, N/A,
niedodatnie referencje i agregację wyników. `G4_SPLIT.md` zalicza M12–M17
do rodziny M19: tych map nie można uznać za niezależny holdout tej rodziny.
Nie zmieniono kryteriów po wynikach.

Wykonawca zgłosił 19/19 testów rdzenia, test CLI, zachowany odcisk
17159159863505677800 oraz powtarzalność macierzy pomiarowej. Powtarzalność
bez pasm nie zalicza rankingu.

Walidacja integratora:

- Kanoniczny `build_ninja.bat`: BUILD SUCCESSFUL (MSVC x64 Release).
- Test CLI na zbudowanym `candidate_rank.exe`: CANDIDATE_RANK_CLI_PASS,
  w tym zachowany odcisk 1729.
- `g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1`: G5_GATE_PASS,
  32/32 dla obu profili, identyczne powtórzenia, warianty lochów i retry 0/8.
- Pełny zestaw z sondami: SUITE_PASS; 19/19 testów rdzenia,
  sondy --commit i --surface PASS, wejściowe zasoby bez zmian.
  Sonda surface: 157 znanych ostrzeżeń SetFocus, brak innego stderr.

Pierwszy build nie znalazł Ninja; ponowiono z lokalnym Ninja vcpkg.
Pierwszy zestaw regresji zgłosił SUITE_FAIL: linker LNK1104, bo równoległy
pomiar używał style_render.exe. Po zakończeniu pomiaru uruchomiono zestaw ponownie.

Użytkownik zaakceptował korektę holdoutu (commit `70c1053`): M01, M05,
M11+M18 jako niezależne dane; M12–M17 osobno jako rodzina M19.

## Test ręczny GUI po zakończeniu buildu

1. Otwórz pustą mapę z załadowanym klientem i okno generatora.
2. Ustaw seed 1729, obszar 100×100 i domyślne parametry; sprawdź powtarzalność podglądu.
3. Wykonaj Place, potem jedno Undo: cały wynik powinien zniknąć; Redo powinno go odtworzyć.
4. W wariancie jaskini sprawdź domyślną ścianę mountain.

I51 nie dodaje galerii ani feedbacku do GUI. Test ręczny, holdout, ślepy test,
zgodność z oryginalnym RME i determinizm między kompilatorami nie zostały potwierdzone.
Worktree agenta nie zawierał build-ninja, więc nie było katalogu do usunięcia.
