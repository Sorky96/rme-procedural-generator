# G6 — stan bramki

## I56 — lokalne uczenie z otwartej mapy

Kryteria `90ebf92`, implementacja `3c8d45c`. Polecenie **Learn from open
map…** analizuje jeden prostokąt powierzchni do 512×512. Użytkownik
przegląda klasy gruntu, potwierdza prawo użycia i zapisuje lokalny profil
zgodny z klientem. Generator pokazuje profil w zwykłej liście. Przepływ
działa na statystykach G4, bez zmiany mapy źródłowej i jej historii Undo.

Kanoniczny build integratora przeszedł z instalacją manifestu wyłączoną.
Niezależne SUITE_PASS obejmuje 23/23 i sondy Commit oraz Surface. Sonda
I56 z zasobami klienta zakończyła się LEARNING_DIALOG_PASS, z zapisem,
ofertą, I54, Place/Undo/Redo, anulowaniem i unieważnieniem próbki. Zasoby
wejściowe zachowały sumy kontrolne. G5_GATE_PASS daje 32/32 dla obu
profili i identyczne powtórzenia. Odcisk 1729 to 17159159863505677800.
Początkową próbę instalacji
zależności przez wykonawcę przerwano przed odnotowaniem ukończonej
instalacji pakietu. Katalog build-ninja worktree wykonawcy usunięto.

Ręczny odbiór wyglądu i klawiatury pozostaje otwarty. I55 i G6 nadal
wymagają ocen i niezależnych rodzin map. [Kroki GUI i zakres I56](I56_LOCAL_LEARNING.md).

## I55 — ślepy test gotowy do przeprowadzenia

Kryteria zapisano w `6251dbb`, narzędzia scalono jako `3bfe971`.
Anonimowy pakiet lokalny obejmuje 20 par i formularz z eksportem JSON.
Integrator potwierdził kompletność archiwum, odciski wszystkich 40 widoków
i brak klucza w paczce. Klucz pozostaje zamknięty do otrzymania 20 ocen.
Próg ślepego testu to 13 wygranych L3 na 20 par.

Porównanie wcześniejszych rodzin jest diagnostyczne. Audyt lokalnych
wejść nie znalazł nowych rodzin z udokumentowanym pochodzeniem, zgodą
i weryfikacją pokrewieństwa G4. Dane tych rodzin trzeba zamrozić przed
oceną ilościową. Próg to 23/32 ścisłych wygranych osobno dla obu profili.
Brak 20 ocen i nowych rodzin pozostawia I55 i G6 otwarte.

Build integratora i CTest 5/5 przeszły przy wyłączonej instalacji zależności.
Pełne `SUITE_PASS` obejmuje 22/22 testów rdzenia, sondy Commit i Surface
oraz niezmienione zasoby wejściowe. `G5_GATE_PASS` (--i47) daje 32/32
dla obu profili i identyczne powtórzenia. Odcisk 1729 nadal wynosi
`17159159863505677800`. Niezależne powtórzenie I55 i porównanie z macierzą
wykonawcy potwierdziło identyczność 532 plików PNG/TSV. Diagnostykę
odtworzono na głównym buildzie. Formularz sprawdzono pod kątem struktury
pakietu i logiki eksportu w kodzie. Ręczny test w przeglądarce pozostał otwarty.

Po scaleniu usunięto build-ninja worktree wykonawcy. Zgłoszoną
niezamierzoną instalację agenta przerwano. Raport odnotowuje częściowe
zapisy w jego buildzie i cache AppData.
[Instrukcja I55](smoke/g6_i55.md).

## I54 — lokalne oceny i wersje profili

Kryteria zapisano w `e9ca540`. Zmiany scalono jako `93426ed`. Approve,
Reject i Adjust zapisują ocenę wybranego kandydata w lokalnej historii.
Adjust zapisuje osiem obsługiwanych ustawień stylu jako nową wersję.
Powrót do wcześniejszej wersji zachowuje nowsze wersje i pozwala utworzyć
od niej kolejną gałąź. Zapis atomowy wykrywa konflikt wczytanych danych.
Po przeglądzie `1d5010b` wiąże ocenę z faktycznymi ustawieniami kandydata
z adaptera, w tym przechodniością skały ustaloną przez dane klienta.

Wykonawca potwierdził build Windows x64, 22/22 testów rdzenia,
7/7 wybranych testów CTest i sondę serii klienta. Katalog build-ninja
worktree wykonawcy usunięto po scaleniu.

Niezależny zestaw `SUITE_PASS` (22/22, Commit, Surface) oraz pomiar
`G5_GATE_PASS` (32/32 obu profili, identyczne powtórzenia) przeszły.
Odcisk 1729 100×100: `17159159863505677800`. Sonda I54 wykryła błąd
żywotności bufora UTF-8 przy zapisie notatki. Wadliwy zapis odrzucono,
a wcześniejsza ocena pozostała w pliku. Poprawka `68a1fc4` zachowuje
źródłowy tekst podczas konwersji. Po niej przeszły kanoniczny build
i przebudowana sonda serii `CANDIDATE_DIALOG_PASS`, z porównaniem
notatki bajt w bajt i sprawdzeniem rollbacku, rozgałęzienia historii,
odmowy nieaktualnego wyniku, Place/Undo/Redo oraz blokad I53. Zasoby
wejściowe pozostały bez zmian. Sonda zgłosiła wyłącznie dziewięć znanych
ostrzeżeń SetFocus. Commit/Surface wykonano przed poprawką UTF-8.

Ręczny odbiór wizualny i klawiaturowy pozostaje niesprawdzony. Ślepy test
jakości należy do I55. G6 pozostaje otwarte.
[Kroki GUI i raport wykonawcy](smoke/g6_i54_report.md).

## I53 — kontrola zgodnych fragmentów

Kryteria zapisano w `87e2246`. Implementację przejrzano i scalono jako
`561ef2f`. Jawny indeks lokalny M06/M19 uruchamia kontrolę w CLI i Surface.
Kandydat z co najmniej 16 wspólnymi unikatowymi oknami i zawieraniem co
najmniej 100‰ dla jednej referencji dostaje hardFail i blokuje Place.
Brak indeksu albo mała próbka mają osobny komunikat. Uszkodzony lub
niezgodny indeks odrzuca serię.

Integrator odbudował indeks M06/M19 i dwukrotnie uruchomił pełną macierz.
Indeks oraz raporty obu przebiegów mają takie same SHA-256 jak u wykonawcy.
Trafienia I53: m06 0/32, m19 0/32. Wszystkie próbki były wystarczające.
m06 seed 1032 nadal ma hardFail z wcześniejszej kontroli.
Raport opisuje pochodzenie indeksu i zakres pięciu klas powierzchni.
[Wyniki i instrukcja GUI](smoke/g6_i53_report.md).

Kanoniczny build Windows x64 i CTest 25/25 PASS, w tym odcisk
`17159159863505677800`. Zestaw G5 SUITE_PASS: 21/21 testów rdzenia i obie
sondy, zasoby wejściowe bez zmian. Świeża sonda serii potwierdziła sześć
przypadków I53, w tym blokadę wymuszonego Place oraz Place → jedno Undo →
Redo dla wyniku poniżej progów. G5 --i47 z rockBlocks m06=0,m19=1:
G5_GATE_PASS, 32/32 obu profili we wszystkich konfiguracjach i identyczne
powtórzenia.

Do ręcznego testu można użyć `scratchpad/i53-index-integrator/references.i53`
z pasującym klientem albo syntetycznych indeksów `copy-fixture.i53` i
`clean-fixture.i53` z `scratchpad/i53-integrator/run` w profilu sondy.
Ręczny odbiór GUI i rzeczywiste wyczerpanie RAM pozostają niesprawdzone.
Po scaleniu usunięto build-ninja worktree agenta. G6 pozostaje otwarte.

## I52 — galeria i wybór kandydata (2026-11-18)

Kryteria prerejestrowano w `0a25e6c`. Implementację Codex medium w osobnym
worktree przejrzano i scalono ff jako `f610cde`. Dodano N=1–16 (domyślnie 4),
miniatury, jawny seed początkowy/rzeczywisty i liczbę prób, wybór wyniku
oraz rekomendację A tylko dla jawnie powiązanych profili M06/M19.
Bez polityki wybierany jest pierwszy dopuszczalny wynik. B nie trafia do GUI.
Ranking nadal ma niezaliczoną bramkę jakości I51.

Galeria przechowuje metadane i miniatury; pełny sandbox wybranego wyniku
jest odtwarzany bez retry i porównywany z zapisanymi odciskami pięter.
Place zapisuje zwalidowany sandbox przez DrawOperations jako jedno Undo.
Seria ma wspólny budżet szacunkowy, anulowanie i kontrolę aktualności mapy,
parametrów i danych klienta. Nie zweryfikowano rzeczywistego OOM/RSS.

Raport wykonawcy i szczegółowe kroki ręczne:
[g6_i52_report.md](smoke/g6_i52_report.md). Wykonawca zgłosił build PASS,
CTest 23/23, sondy Commit/Surface/serii PASS i G5 32/32 obu profili.
Dwie końcowe korekty błędów przebudowano u wykonawcy bez ponownego
linkowania sond, dlatego integrator uruchamia świeże sondy z końcowych źródeł.

Niezależna walidacja integratora na końcowych źródłach:

- Kanoniczny build: BUILD SUCCESSFUL.
- Zestaw: SUITE_PASS, 20/20 testów rdzenia; świeżo przebudowane sondy
  --commit i --surface PASS, wejściowe zasoby bez zmian. Surface: 157
  znanych ostrzeżeń SetFocus, bez innych stderr.
- Świeżo przebudowana sonda --series: CANDIDATE_DIALOG_PASS, exit 0,
  zasoby wejściowe bez zmian. N=1/4/16, oba profile i fallback, wybór
  innego dopuszczalnego kandydata, Place/Undo/Redo, anulowanie, zmiana
  parametrów/zamknięcie/nieaktualna mapa oraz wymuszony hardFail Place PASS.
  Siedem znanych ostrzeżeń SetFocus, bez innych stderr.
  To automatyczne sterowanie produkcyjnymi kontrolkami, nie ręczny odbiór.
- Pomiar G5 --i47 rockBlocks m06=0,m19=1: G5_GATE_PASS, 32/32 obu profili.

Prywatne dowody: scratchpad/g6-i52-{build,suite,series,g5}.log oraz
scratchpad/i52-integrator/verification.json. Sondy przebudowano po obu
końcowych korektach wykonawcy; rzeczywistego OOM i wstrzykiwania awarii
alokacji w tych ścieżkach nie testowano ponownie.
Po ff-merge usunięto wyłącznie build-ninja worktree t3-aaa1909a po
sprawdzeniu docelowej ścieżki. Nie wykonano ręcznego testu GUI/DPI/klawiatury.
I52 pozostaje częściowe do odbioru; I51 i I51b FAIL, G6 nadal otwarte.

Test ręczny: wygeneruj N=1,4,16; wybierz innego dopuszczalnego kandydata
niż Recommended, sprawdź piętra, Place -> jedno Undo -> Redo. Sprawdź
blokadę Place dla hardFail, anulowanie serii, zmianę parametrów i domyślną
ścianę mountain. Zmiana wyboru ani Preview nie mogą zmieniać mapy.

## I51b — eksperyment niezaliczony (2026-11-18)

Prerejestracja `9ab1ca0`; implementacja Codex medium w osobnym worktree,
przegląd i ff-merge `9b79919`. Domyślny ranking A zachowano; B dostępny
wyłącznie w jawnym trybie offline --i51b. Bez zmian generatora, GUI i pasm.

**I51B_FAIL:** B nie przewyższa A w żadnym profilu. Seedy 2001–2032:
m06 A=29/32, B=28/32; m19 A=27/32, B=21/32. Wymagane B≥23/32 i B>A
osobno dla obu profili. Nie dostrajano kryteriów ani modelu po wyniku.
Historyczne I51 nadal FAIL 18/32 obu profili; nie zastępujemy jego wyniku
nowymi seedami. G6 pozostaje otwarte; ten holdout był wcześniej ujawniony.

Raport wykonawcy: [g6_i51b_report.md](smoke/g6_i51b_report.md),
pełne wyniki [g6_i51b_holdout.json](smoke/g6_i51b_holdout.json).
Integrator powtórzył ocenę surowych raportów i potwierdził wynik.
Testy integratora: I51b 3/3 PASS, ewaluator I51 8/8 PASS.
Wykonawca: CTest 22/22 PASS i dwa identyczne pełne przebiegi macierzy.
Integrator nie generował ponownie pełnej macierzy; powtarzalność oceniono
na raportach wykonawcy.

Niezależna walidacja integratora: kanoniczny build PASS; SUITE_PASS,
19/19 testów rdzenia, sondy --commit i --surface PASS, zasoby wejściowe
bez zmian (surface: 157 znanych ostrzeżeń SetFocus, bez innych stderr).
Pomiar G5 --i47 z rockBlocks m06=0,m19=1: G5_GATE_PASS, 32/32 obu
profili i identyczne powtórzenia. Test CLI na przebudowanym narzędziu:
CANDIDATE_RANK_CLI_PASS, w tym fingerprint 17159159863505677800 i --i51b.
Worktree nie zawierał build-ninja, brak katalogu do usunięcia.

Po zakończeniu buildu można sprawdzić offline CLI --i51b: dla każdego seeda
powinny pojawić się winner (A) i winnerB (B), obliczone z tych samych wierszy.
Powtórzenie polecenia musi dać ten sam raport. B nie trafia do GUI.
Ręczny test GUI: kroki Place/Undo/Redo i mountain opisane poniżej;
nie został wykonany przez integratora. Nie potwierdzono oryginalnego RME
ani determinizmu między kompilatorami.

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
