# G1 — regresja rdzenia po etapowaniu Preview

Próba `g1-staged-core-rme-20261104` wykryła regresję. Synchroniczne przygotowanie poprawnie odrzuciło budżet poniżej kosztu infrastruktury, lecz zdążyło zaalokować ramkę korutyny. Istniejący test wymagał odmowy bez alokacji. Przebieg zakończył się kodem 9 na `Joint infrastructure limit allocated or ran`. Przed odmową fingerprinty obu rozmiarów były zgodne z historycznym przebiegiem.

Kontrole wstępne wykonują się teraz przed utworzeniem kontynuacji w obu ścieżkach wywołania. Kontynuacja dostaje pozostały budżet, rozmiary rezerwacji i rewizję assets. Pierwsze wznowienie odrzuca zmianę tej rewizji przed odczytem mapy. Dialog obsługuje odmowę przed zaplanowaniem pierwszego timera.

## Wynik po poprawce

`build_ninja.bat` zakończył się sukcesem. Przebieg `g1-staged-core-final-rme-20261104` odtworzył wszystkie markery testu rdzenia i zakończył się kodem 0. Przeszło 80 prób awarii sandboxa, 108 prób awarii przygotowania, 112 prób awarii commitu i 15 prób awarii rezerwacji. Kontrole obejmowały też 29 odmów walidacji i 39 mutacji katalogu zasobów. Fingerprinty podglądu, maski i commitu dla 100×100 oraz 500×500 są identyczne z historycznymi wynikami. Verifier potwierdził 421 obiektów i 421 źródeł kompilacji oraz niezmienione wejściowe assets.

Stderr zawierał jedno odłożone ostrzeżenie `SetFocus`. Status przebiegu to **PARTIAL_G1_STAGED_CORE_FOCUS_WARNING**. Na zwykłym, nowo zbudowanym RME z osobnym prywatnym profilem sprawdzono także odmowę prostokąta poza mapą. Validate i Place były wyłączone. Po przywróceniu X=100 Preview pokazało 10 000 kafli, a Cancel zamknął dialog. Ten krótki test GUI nie powtarzał round-trip ani pomiarów procesu.

## Stan dowodów G1

| Kryterium | Dowód |
|---|---|
| Dokładny diff, deterministyczność i jeden wpis Undo | Bieżący test rdzenia dla obu rozmiarów. Natywne Place/Undo/Redo sprawdzono na poprzednim buildzie. [Round-trip](G1_ROUNDTRIP.md) |
| Maska, ochrona i odrzucanie zmian celu | Bieżący test rdzenia obejmuje dokładną maskę, katalog assets i odmowy celu. Natywne ścieżki odmów i unieważniania mają osobne przebiegi. [Odmowy](G1_REFUSALS.md), [Unieważnienie](G1_INVALIDATION.md) |
| Budżet przygotowania i commitu | Bieżący test rdzenia obejmuje dokładne limity i odmowy. Pomiary pamięci procesu i przerw GUI są sprzed poprawki kontroli wstępnej. [Pomiar GUI](G1_CANCEL.md) |
| Zapis i ponowne otwarcie | 100×100 i 500×500 dla OTBM 5 w poprzednim buildzie, z pięcioma XML i porównaniem danych po odczycie. [Round-trip](G1_ROUNDTRIP.md) |

Pełne **G1 IN PROGRESS**. Nie powtórzono pełnej macierzy GUI ani pomiarów procesu na nowym binarium.

## Odtworzenie

```text
rtk proxy <PY> docs/procedural/smoke/g1_run_resolver_probe.py local-data/procedural-smoke/NEW_RUN local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --commit --visible
rtk proxy <PY> docs/procedural/smoke/g1_verify_staged_core.py local-data/procedural-smoke/NEW_RUN
```

`<PY>` to `C:/Users/Sorky/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe -X utf8`. Po starcie zamknij komunikat zasobów prywatnego klienta. Probe wykonuje testy na syntetycznych edytorach.
