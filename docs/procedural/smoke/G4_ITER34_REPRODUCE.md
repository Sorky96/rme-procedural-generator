# D21: odtworzenie tabel G4 z iteracji 3 i 4

Jedno polecenie odtwarza tabele `style_styles` (iteracja 3) i `water_eval` (iteracja 4) z
[G4_GATE_STATUS.md](../G4_GATE_STATUS.md). Kryteria zapisano przed pracą w [G6_METRICS.md](../G6_METRICS.md) („D21”, commit `e3e6f8b`).

```
py -I docs/procedural/smoke/g4_reproduce_iter34.py <NOWY_KATALOG_W_SCRATCHPAD> [--local-data DIR] [--current-bin DIR] [--vcvars FILE]
```

Ostatnia linia wyjścia to `D21_PASS` albo `D21_FAIL`. Katalog wyjściowy musi być nowy albo pusty. Skrypt nic nie zapisuje w repo ani w `local-data`.

## Co robi

1. Sprawdza, czy są wszystkie wejścia i historyczne raporty. Jeśli czegoś brakuje, wypisuje brakujące ścieżki i kończy się przed budową.
2. Buduje narzędzia z commitów, które dały tabele: `style_styles` z `269da1b`, `water_eval` z `99f617c`. Pobiera `source/procedural/core` i plik `*_main.cpp` przez `git archive`, a potem kompiluje je jednym wywołaniem `cl` z flagami wydania z `build_ninja.bat` i z `/Brepro` (bez znacznika czasu PE). Nie używa CMake ani vcpkg; rdzeń w tych commitach korzysta tylko z biblioteki standardowej. MSVC znajduje przez `vswhere`, a `--vcvars` pozwala go wskazać.
3. Uruchamia każde narzędzie dwa razy na tych samych wejściach i porównuje raporty bajt po bajcie: przebiegi między sobą oraz z historycznym raportem.
4. Porównuje liczby cytowane w tabelach i tekście G4_GATE_STATUS.md z odtworzonym raportem: 15 sprawdzeń dla iteracji 3 i 10 dla wiersza 4b.
5. Zapisuje `params.json` (SHA-256 każdego wejścia, historycznych raportów i EXE, commit, źródła, flagi, wersja kompilatora, polecenie, HEAD repo) oraz `summary.json`.

`--current-bin DIR` uruchamia dodatkowo nowsze binaria (np. `build-ninja/source/procedural` głównego checkoutu) na tych samych wejściach. Ten przebieg ma charakter wyłącznie informacyjny i nie wpływa na werdykt.

## Wejścia (jawne, w `<local-data>/procedural-smoke`)

| Wejście | Ścieżka | Skąd wiadomo |
| --- | --- | --- |
| manifest | `g4-split/manifest.tsv` | jedyny manifest G4 |
| odciski | `g4-split/fp-c/<alias>.fp` | ostatni zestaw przed `styles-1.json` (fp-c 09:57, raport 10:42) |
| statystyki stylu | `g4-dood-20261108b/<alias>/run/analysis/` | jedyny katalog z `decor-context-surface.txt` sprzed 10:42 |
| kształty wody | `g4-water-20261108a/<alias>/run/analysis/water-shapes-surface.txt` | jedyny katalog kształtów sprzed `water-1.json` (12:22) |
| raporty historyczne | `g4-fit/styles-1.json`, `g4-fit/water-4b.json`, `g4-fit/water-4a.json` | zapisane w czasie iteracji |

Dobór katalogów wynika ze znaczników czasu plików, bo historyczne polecenia nie zostały zapisane. Trafność tego doboru potwierdza bajtowa zgodność wyniku z historycznym raportem. W `g4-dood-20261108b` brakuje `ground-keys-surface.csv`. `style_styles` w `269da1b` tego pliku nie czyta, a w `params.json` jest on zapisany jako `null`.

## Wynik (2026-11-19)

Pięć wywołań skryptu, każde z dwoma przebiegami narzędzi, w katalogach scratchpad tej sesji: wszystkie zakończyły się `D21_PASS`. Dwa ostatnie wywołania użyły ostatecznej wersji skryptu. Kompilator: MSVC 19.51.36257 x64 (Visual Studio 18 Community). Hash EXE `water_eval` jest stały między budowami. Hash EXE `style_styles` różni się między budowami w różnych katalogach mimo `/Brepro`, a przyczyny nie ustalono. Raporty są mimo to identyczne, a kryterium dotyczy raportów.

- `style_styles` 269da1b: oba przebiegi są identyczne i dają `styles-1.json` bajt w bajt (SHA-256 `de465dde…`); 15/15 wartości tabeli iteracji 3 się zgadza.
- `water_eval` 99f617c: oba przebiegi są identyczne i dają `water-4b.json` bajt w bajt (SHA-256 `c179f0e5…`); 10/10 wartości wiersza 4b i tekstu się zgadza.
- Wiersz 4a: **nie da się go uruchomić ponownie**. Model sprzed `99f617c` (pierwszy wariant meandrowania, 12:22–12:23) nie trafił do git. Skrypt sprawdza tylko zapisany `water-4a.json`.
- **Odchylenie (tabela bez zmian):** wiersz 4a w tabeli podaje P1 99,997 %, a zapisany raport daje 999 669 / 999 730 ppm = 99,994 %. Pozostałe wartości wiersza 4a (meander 8, T 1398, S 70, 18/9, 15/6, 0 wysp) się zgadzają. Sprawdzenie przechodzi jako „documented deviation” tylko wtedy, gdy raport dalej ma dokładnie 99,994 %.
- Bieżące binaria głównego checkoutu (`build-ninja`, zbudowane 2026-11-09 13:41, commit budowy nieznany): `water_eval` daje `water-4b.json` bajt w bajt, a `style_styles` daje inny raport, bo generator i narzędzie zmieniły się po iteracji 3. Bieżące binaria nie odtwarzają więc iteracji 3; potrzebna jest budowa z `269da1b`.

Czas jednego wywołania z `--current-bin` wynosi 3,5–7 min, zależnie od obciążenia maszyny. Składają się na to dwie budowy po około 20 s i trzy przebiegi `style_styles` po około 47 s.

## Czego to nie sprawdza

- Zdania „0 porażek w 1200/1200 seedach” z iteracji 4 nie ma w raporcie (`river_invariant_failures` = 0 dla 31 seedów w każdym foldzie), dlatego skrypt go nie weryfikuje.
- Czasy przebiegów z tabel („44 s”) nie są porównywane.
- Wejścia z `local-data` pochodzą ze starszych przebiegów sondy importu. Skrypt ich nie regeneruje, tylko zapisuje ich hashe.
- Zgodność sprawdzono z MSVC 18 Community z tej maszyny. Na innym kompilatorze bajtowej zgodności nie badano.
