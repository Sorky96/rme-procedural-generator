# G2-A — rdzeń powierzchni (algorytm v2), stan na 2026-11-06

**PARTIAL.** Rdzeń C++ przechodzi własny test. Nie ma adaptera, resolvera, borderów ani wyniku w RME, więc nie jest to jeszcze zaliczenie żadnego kryterium bramki G2 („czytelny w RME”, „brak krytycznych błędów borderów”).

## Zakres

`source/procedural/core/surface.h/.cpp` to osobny generator obok v1 (`generation.h`). Typy, fingerprinty i resolver v1 nie zostały zmienione. v2 ma własne rodzaje (`Grass`, `Dirt`, `Rock`, `Shore`, `Lake`, `River`), własną wersję algorytmu i domenę fingerprintu.

- Pola wysokości i wilgotności: szum wartościowy z trzema oktawami, tylko arytmetyka całkowita, funkcja seeda i współrzędnych absolutnych.
- Jeziora: najniższe komórki, 4-spójne zbiorniki mniejsze niż `minLakeCells` wracają do lądu.
- Skały: najwyższe komórki, małe płaty usuwane.
- Rzeki: priority-flood od jezior i brzegu prostokąta. Trasa jest 4-spójna i bez pętli; rzeki krótsze niż `minRiverLength` odrzucane. Rzeka kończy się w jeziorze albo na brzegu generowanego prostokąta.
- Brzeg: pierścień `Shore` (szerokość 0–4) wokół jezior, nie nadpisuje skały.
- Trawa/ziemia: próg wilgotności z pasem przejściowym o regulowanej szerokości i losowym rozproszeniu per komórka.
- Maska i ochrona nie zmieniają wartości innych komórek.

## Wynik testu `test_procedural_surface`

Build: `build_ninja.bat` (rme.exe) i `ninja test_procedural_surface` z środowiskiem MSVC x64, Release. Exit 0, `PROCEDURAL SURFACE PASS`; `test_procedural_core` (v1) po zmianach również PASS.

| Sprawdzenie | Wynik |
| --- | --- |
| Walidacja parametrów, regionu, masek, limitów komórek/wyjścia/pamięci roboczej, anulowanie | PASS |
| Powtarzalność, wpływ seeda, brak `Preserve` przy pełnej masce | PASS |
| Nieregularna maska + ochrona nie zmienia innych komórek | PASS |
| 24 seedy 100×100: każdy kafel ma rodzaj; jezioro ≤ limit udziału i ≥ minimum; rzeka kończy się w jeziorze/na brzegu i ma ≥ minimum; ląd przy jeziorze to Shore/Rock/River; płaty skały ≥ minimum (przy `riverCount=0`) | PASS |
| 6 seedów 250×250: te same sprawdzenia | PASS |
| Udział ziemi w otwartym terenie (próg ustalony przed uruchomieniem: 10–60 %) | 25,6–38,1 % (100×100), 28,9–31,6 % (250×250) |
| Seedy z jeziorem / rzeką / skałą (100×100) | 24 / 23 / 24 z 24 |
| Pas przejściowy: krawędzie trawa–ziemia, szerokość 0 vs 16000 | 96 vs 3053 |
| Wstrzyknięcie `bad_alloc` w każdym z 92 punktów alokacji (24×24) | 92 z 92 zgłoszone jako `AllocationFailed`, bez crasha |
| Zapamiętane fingerprinty (układ bajtów, seed 1729 100×100) | PASS (wartości z pierwszego udanego przebiegu) |

Czasy (jedna seria, jedna maszyna, bez powtórzeń, nie wnioski o szybkości): 100×100 ≈ 1,7 ms, 500×500 ≈ 44–48 ms. Szacunek pamięci roboczej 500×500: ≈ 10,3 MB (górne oszacowanie z 40 B/komórkę, nie pomiar).

## Czego nie zweryfikowano

- Wyglądu terenu: test sprawdza własności strukturalne, nie czytelność. Ocena w RME dopiero po adapterze.
- Jeden z 24 seedów 100×100 nie miał rzeki (krótka trasa odrzucona); to dozwolone przez konstrukcję, nie błąd, ale oznacza, że „zawsze jest rzeka” nie jest gwarantowane.
- Rzeki kończą się na brzegu prostokąta, także gdy jest to tylko fragment większej mapy.
- Progi udziałów to założenia sprzed uruchomienia, nie wymagania z planu. Zmierzone zakresy zapisano wyżej.
- Rzeczywiste zużycie pamięci i czas w pełnej ścieżce Preview/Place (z resolverem i snapshotem) nie były mierzone.
- v2 nie jest podłączone do dialogu ani resolvera; resolver v1 indeksuje trzy brushe i nie przyjmuje nowych rodzajów.

Następny krok: G2-B (adapter: mapa rodzaj → brush, brzegi przez istniejący `GroundBorderCalculator`, testy wielokierunkowych krawędzi i okolicy).

## Zmiana po pierwszym teście GUI, 2026-11-07

Pierwszy ogląd w RME (Natural surface 250×250, kilka seedów): jeziora z piaskiem i płaty skały wyglądały dobrze, ale ziemia wśród trawy była „poszatkowana”, a rzeki cienkie. Zmiany w `surface.cpp`:

- Pas przejściowy trawa/ziemia to teraz gładkie przesunięcie progu wilgotności (szum wartościowy, krok 10), a nie szum per komórka. `transitionWidth` jest amplitudą tego przesunięcia. Płaty trawy i ziemi mniejsze niż 24 komórki (stała w kodzie, nie parametr) są wchłaniane przez otoczenie.
- Każda trasa rzeki jest poszerzana o sąsiada wschodniego i południowego, więc rzeka nie ma odcinków o szerokości jednego kafla (jeziora zostają nietknięte).
- Udział ziemi w otwartym terenie jest teraz przybliżony, bo przesunięcie i usuwanie płatów odchylają go od kwantyla `dirtPermille`.

`test_procedural_surface` po zmianach: PASS. Udział ziemi 24,4–38,6 % (100×100, 24 seedy) i 28,0–32,0 % (250×250), krawędzie trawa–ziemia przy szerokości 0 vs 16000: 88 vs 251, punkty alokacji 150 z 150 zgłoszone. Przypięte fingerprinty zmieniły się, bo wynik algorytmu jest inny: seed 1729 100×100 = 15556827831682428334, 500×500 = 18160721199455356238 (wersja algorytmu w fingerprincie pozostaje 2, v2 nie było wydane). Ponowny przebieg `--surface` na kliencie 1098 (`g2-fix1-surface-20261107`): wszystkie znaczniki PASS (resolver, `SURFACE_E2E_PASS` 100 i 250), `passed: false` z tego samego powodu co wcześniej (stderr). Wizualnie poprawek **nie oceniono**: czeka na kolejny ogląd w RME. Rzeki nie mają testu minimalnej szerokości.
