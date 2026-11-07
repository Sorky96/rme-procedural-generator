# G2-B2 — algorytm Surface w istniejącym potoku, stan na 2026-11-07

**PARTIAL.** Przebieg przygotowania, walidacji, podglądu, commitu z jednym Undo i Redo działa dla powierzchni v2 na rzeczywistym kliencie 1098-BlackTek (100×100 i 250×250), a regresje G1 (resolver, preview, commit) przeszły. Nie sprawdzono dialogu w natywnym GUI ani wyglądu terenu w RME, więc kryterium G2 „czytelny w RME” pozostaje otwarte.

## Zmiany

- `GenerationConfig` ma `algorithm` (`GroundRoll` domyślnie, `Surface`) i `surface` (`SurfaceTuning`, parametry v2) oraz `operator==`, który zastąpił dwie ręczne kopie porównania konfiguracji (podgląd, walidacja).
- `GroundProfile` ma sześć miejsc na nazwy i `count` (domyślnie 3, dla Surface 6), `active()` zwraca użyte nazwy. Pętle po nazwach w resolverze G1, podglądzie, przygotowaniu i commicie używają `active()`. Resolver G1 odrzuca `count != 3`. Inicjalizacje G1 trzema brushami kompilują się bez zmian.
- Hash źródła podglądu: dla `GroundRoll` bez zmiany układu bajtów; algorytm i parametry v2 dopisywane tylko dla Surface.
- `prepareTerrain`: rozgałęzienie po algorytmie, rezerwacja pamięci roboczej generatora (`validateSurface`) w jednym wspólnym liczniku, profil musi mieć 3 (GroundRoll) lub 6 (Surface) brushy. Nowy `PreparationInputError::InvalidParameters`.
- `resolveSurfacePlan` zwraca zwykły `GroundPlan` (profil: Grass, Dirt, Rock, Shore, Lake, River).
- Dialog: lista „Terrain style” (Classic domyślnie, Natural surface), pola brushy skały (`rocky ground`) i brzegu (`sand`); jezioro i rzeka dzielą pole wody. Parametry v2 nie mają kontrolek (wartości domyślne).

## Wyniki (profil `g0-reference-profile-20261030`, prywatne przebiegi, ukryte okno)

| Próbnik | Przebieg | Wynik |
| --- | --- | --- |
| resolver G1 | `g2-b2-resolver-b-20261106` | `RESOLVER_PASS`, fingerprinty 15173774556512220455 i 2039901658534265928 jak w [G1_RESOLVER.md](G1_RESOLVER.md) |
| `--preview` | `g2-b2-preview-c-20261107` | `passed: true`, stderr pusty |
| `--commit` | `g2-b2-commit-c-20261107` | `passed: true`, stderr pusty; wiersze 100×100 i 500×500 (8881480 / 3965894 / 80195 / 6608263949644166501 oraz 218763816 / 98785798 / 1987447 / 173161415496114986) zgodne z [G1_COMMIT.md](G1_COMMIT.md) |
| `--surface` | `g2-b2-surface-e-20261107` | exit 0, wszystkie znaczniki PASS (`SURFACE_RESOLVER_PASS`, `SURFACE_E2E_PASS` 100×100: 10 000 zmian, ok. 14,6 MB szacunku; 250×250: 62 500 zmian, ok. 84,2 MB). Runner zapisał `passed: false`, bo stderr zawiera dwa ostrzeżenia `SetFocus` 0x57 (patrz dług D1) |

Test end-to-end w `--surface` sprawdza: przygotowanie, walidację, odrzucenie zmienionych parametrów v2, zmienionego algorytmu i profilu o złej liczbie brushy (obie strony), commit jako jeden krok historii, równość położonego gruntu z planem dla każdej komórki, Undo (brak wygenerowanego gruntu, liczba kafli jak przed) i Redo.

Szczegóły wyników borderów dla tego samego resolvera: [G2_BORDERS.md](G2_BORDERS.md).

## Ustalenia

- **Limit pamięci Undo.** Domyślne `undo_mem_size` to 40 MiB. Pierwszy przebieg 250×250 przy domyślnym limicie zwrócił `HistoryRefused` (wartość 6 w `TerrainCommitFailure`, ustalona z kodu); po podniesieniu do 512 MiB, jak w próbnikach G1, commit przeszedł. Przyczynę wywnioskowałem z liczb (84 MB > 40 MiB), nie śledziłem kodu kolejki. Dialog już dziś pokazuje komunikat o limitach Undo w File → Preferences → General. Przy domyślnych 40 MiB graniczny rozmiar Place szacuję na około 170×170, to ekstrapolacja z jednego pomiaru.
- **Okno „Warnings” przy starcie klienta** blokowało próbniki do ręcznego zamknięcia. Dodano test-only `tests/procedural_probe_warnings.h` (zamyka tylko modalne „Warnings” i zapisuje `warnings-dialog-dismissed.txt`), użyty w próbniku `--surface` i w `test_procedural_preview.cpp` (zmieniono tylko obsługę bezczynności). Treści ostrzeżeń nie zapisano ani nie zbadano; w aplikacji nie ma ustawienia, które je wyłącza.
- **Okno „Save changes”** przy zamknięciu zawieszało próbnik po commicie; test `--surface` czyści teraz flagę zmian (`map.clearChanges()`). Próbnik commitu G1 nie był zmieniany.
- Poprzedni przebieg `g2-b2-surface-d-20261107` przekroczył 180 s na tym oknie i zostaje bez zaliczenia (log miał te same znaczniki PASS). Pierwsze uruchomienie czterech próbników (`*-20261106`) przerwano tracebackiem po limicie czasu; w tamtym przebiegu nie zebrano wyników preview i commit.

## Czego nie zweryfikowano

- Zachowania dialogu po zmianach (lista stylu, brushe skały i brzegu, komunikaty) w natywnym RME oraz tego, że tryb Classic działa w GUI jak w G1. Próbniki G1 nie sterują dialogiem w tym przebiegu.
- Wyglądu terenu i borderów w RME, w tym rzeki o szerokości jednego kafla.
- Dwóch ostrzeżeń `SetFocus` w `--surface`: ich pochodzenie uznaję za to samo co D1 (`MapTabbook::DeleteTab` przy zamykaniu), ale nie śledziłem stosu. Preview i commit nie miały ich w stderr.
- Zachowania dla innych klientów niż 1098-BlackTek, 500×500 w pełnym potoku v2 (próbnik end-to-end sprawdza 100 i 250), czasu i pamięci poza szacunkami, oraz D5 (bordery poza obszarem przy cudzym sąsiedzie), które pozostaje otwarte.
