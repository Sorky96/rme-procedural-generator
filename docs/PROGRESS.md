# Procedural World Generator — postęp

## 2026-10-26 — analiza i redakcja planu

Zakres sesji: przygotowanie `PROJECT_RME_PROCEDURAL_GENERATOR.md` do realizacji. Wersja planu: 1.1. Nie rozpoczęto implementacji generatora; przegląd źródeł jest częściowym przygotowaniem do G0, a nie zaliczeniem tej bramki.

### Wykonano

- Przeczytano plan, `AGENTS.md`, `RTK.md`, konfigurację CMake/vcpkg, fragmenty instrukcji skryptów i skryptu buildu.
- Sprawdzono punkty integracji: `DrawOperations`, `ActionQueue`, `LuaTransaction`, rejestrację `noise`/`algo`/`geo`, demo terenu, RNG ground/doodad, tożsamość mapy i zapis plików pomocniczych OTBM.
- Doprecyzowano MVP, minimalne GUI, snapshot i konflikty, deterministyczność adaptera, ochronę obszaru, rollback, budżet pamięci i kryteria testów. Szczegóły i lokalizacje źródeł: sekcja 1.0 planu.
- Context7: `/websites/wxwidgets/3.3`, dokumentacja komunikacji worker–GUI i życia odbiorcy zdarzeń. To weryfikacja dokumentacji, nie test RME.

### Dowody i ograniczenia

- `rtk git status --short --branch`: brak repozytorium Git; w katalogu nie ma `.git`, jest `.gito`. Nie zmieniano metadanych, nie utworzono gałęzi ani commita, SHA nieustalone.
- Brak `.codegraph/`: zgodnie z instrukcjami nie wywoływano zapytań do nieistniejącego indeksu ani nie uruchamiano indeksowania.
- `rtk proxy ast-grep --version`: 0.45.1. Zapytanie strukturalne `g_gui.GetCurrentBrush()` potwierdziło zależność trzech przeciążeń rysowania od aktualnego brusha GUI.
- Użyto `rg` i odczytów źródeł przez `rtk proxy`. Nie zakładano, że deklaracje API dowodzą poprawności runtime.
- Kontrola dokumentów w PowerShell: poprawne odczytanie UTF-8 bez znaków zastępczych, domknięte bloki kodu, sekcje 0–14 i 12 odwołań do istniejących plików źródłowych/skryptów — OK. Pierwsza próba kontroli przez `python` nie uruchomiła się (brak interpretera pod tą komendą); kontrolę wykonano przez PowerShell.
- Build, uruchomienie GUI, smoke test Lua 20x20, Undo/Redo, round-trip OTBM, testy regresyjne i benchmarki: **nie uruchomiono**. Zmiany obejmują wyłącznie dokumentację.

### Stan faz

| Faza | Status | Commit | Dowody/testy | Blokery lub zależności | Następne zadanie |
|---|---|---|---|---|---|
| 0 | IN PROGRESS — tylko przegląd przygotowawczy | brak | Analiza źródeł i dokumentacji, bez testów runtime | Brak Git/SHA; dostępność zgodnych assets i mapy smoke jeszcze nieustalona | Pełny audyt G0 |
| 1 | TODO | — | — | G0 | MVP jednego piętra |
| 2 | TODO | — | — | G1 | Teren i bordery |
| 3 | TODO | — | — | G2 oraz referencje | Analiza referencji |
| 4 | TODO | — | — | G3 oraz holdout | Profile L1/L2 |
| 5 | TODO | — | — | G4 | Topologia wielu pięter |
| 6 | TODO | — | — | G5 | Ranking i feedback, opcjonalny ML |
| 7 | TODO | — | — | G6 | Rozbudowa GUI |
| 8 | TODO | — | — | G7 | Szersze pomiary i niezawodność |
| 9 | TODO | — | — | G8 | Beta i wydanie |

### Następne trzy zadania po rozpoczęciu FAZY 0

1. Ustalić pochodzenie kopii, dostęp do właściwego checkoutu Git i SHA; utworzyć gałąź z `master` i zapisać lokalne zmiany. CodeGraph wykorzystywać po udostępnieniu indeksu.
2. Potwierdzić toolchain, dostępne assets i mapę testową; wykonać kanoniczny build Windows x64 i zapisać wynik, bez deklarowania powodzenia z wyprzedzeniem.
3. Przeprowadzić smoke test na kopii kompletu plików (20x20, Undo/Redo, zapis/odczyt), a wyniki audytu i plan integracji umieścić w `docs/procedural/ARCHITECTURE.md` oraz `docs/procedural/TEST_PLAN.md`.
