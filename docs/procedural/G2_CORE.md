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
