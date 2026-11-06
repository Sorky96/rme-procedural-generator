# Dług techniczny generatora terenu

Pozycje świadomie odłożone, z datą decyzji. Dług nie jest zaliczeniem kryterium: bramki opisują w [G1_GATE_STATUS.md](G1_GATE_STATUS.md), co jest dowiedzione.

| ID | Dług | Decyzja | Skutek | Jak zamknąć |
| --- | --- | --- | --- | --- |
| D1 | Ostrzeżenie wxWidgets `SetFocus` (błąd Win32 87) przy File → Exit. `MapTabbook::DeleteTab` usuwa kartę, a `wxAuiNotebook` ustawia fokus w ramce wyłączonej przez okno „Closing”. [Diagnoza](G1_FOCUS_TRACE.md#zamykanie-aplikacji-i-kod-wyjścia-próbników-2026-11-06). | 2026-11-06, użytkownik: dług | Komunikat w stderr przy zamykaniu; verifier zwraca `PARTIAL_…_FOCUS_WARNING`. Fokus nie ma skutku, bo aplikacja się zamyka; poza zamykaniem nie sprawdzono. | Zmiana w `MapTabbook::DeleteTab` (nie ustawiać fokusu w wyłączonej ramce); to kod poza generatorem. Potem verifier bez ostrzeżenia. |
| D2 | Niewyjaśniony stos `SetFocus` z aktywacji ramki przy otwartym generatorze (przebieg unieważniania, 2026-11-04). Brak stanu okien; przebiegi z 2026-11-06 go nie odtworzyły. | 2026-11-06, użytkownik: dług | Nieznana przyczyna jednego z dwóch zapisanych stosów. Hipoteza: ta sama zależność od wyłączonej ramki pod modalnym dialogiem. | Przebieg `--invalidation --focus-trace` z operatorem i stanem okien w śladzie. |
| D3 | Awarie alokacji sprawdzono tylko w opisanych miejscach: wstrzyknięcie w wektor halo przy Preview (natywnie) oraz próby sandboxa (80), przygotowania (108), commitu (112) i rezerwacji (15) w teście rdzenia ([G1_STAGED_CORE.md](G1_STAGED_CORE.md)). Pozostałych miejsc alokacji w ścieżce generatora nie wyliczono. | 2026-11-06, użytkownik: dług | Nieznane zachowanie przy awarii alokacji poza wymienionymi miejscami. | Wyliczyć miejsca alokacji w ścieżce Preview/Validate/Place/Undo/Redo, dla każdego dodać wstrzyknięcie lub uzasadnić pominięcie. |
| D4 | Pamięć prywatna dialogu rośnie skokami w seriach 30 i 200 cykli (≈0,31 MiB/cykl na 200 cyklach, brak plateau); przyczyna niezbadana. [Seria 200 cykli](G1_GATE_STATUS.md#seria-200-cykli-2026-11-06). | 2026-11-06, użytkownik: nie bloker G1 | Wzrost pamięci przy bardzo wielu cyklach Preview w jednej sesji. | Porównanie sterty między cyklami; rozróżnić rezerwy alokatora od wycieku. |

Poza zakresem G1 (nie dług): rzeczywiste sesje sieciowe live, decyzja użytkownika 2026-11-06.
