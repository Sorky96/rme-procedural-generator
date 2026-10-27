# Ręczne skrypty G0

To skrypty diagnostyczne istniejącego API Lua RME, wykonane podczas audytu z 26–27.10.2026. Nie są implementacją generatora ani automatycznym zestawem testów CTest.

Przygotowanie klienta, kolejność uruchamiania, wyniki i ograniczenia: [TEST_PLAN.md](../TEST_PLAN.md#powtórzenie-ręcznego-smoke). Kopiować do `scripts/` obok testowego EXE i uruchamiać ręcznie z Script Manager, wyłącznie na syntetycznym fixture. Nie włączać autorun.

- `g0_smoke.lua`: wymaga pustej mapy; kolejne wywołania sprawdzają umieszczenie 400 ground, ręczne Undo i ręczne Redo. Reload Scripts resetuje stan etapów.
- `g0_verify_saved.lua`: kontrola tylko do odczytu zapisanej mapy `g0-smoke.otbm`.
- `g0_error_probe.lua`: celowy błąd po mutacji pustego pola (140,140,7). Na audytowanym buildzie odtwarza **FAIL rollbacku** i pozostawia zmianę w historii. Po próbie wykonać jedno Undo i verifier. Nie zapisywać diagnostycznej zmiany.

Brak assets i map binarnych w repozytorium. Nazwa fixture i liczba pól ograniczają przypadkowe uruchomienie, ale nie zastępują sprawdzenia aktywnej mapy przez operatora.
