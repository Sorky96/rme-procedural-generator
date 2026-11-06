# G1: izolacja zaznaczenia podczas podglądu

Nowszy wycinek: [G1_BUDGET.md](G1_BUDGET.md) dodaje i weryfikuje wspólny konserwatywny budżet przygotowania. Poniższy stan jest historyczny w zakresie tej pozycji; RSS całej aplikacji i pamięć przyszłego Undo pozostają otwarte.

Stan 2026-11-02: **PASS_G1_PREVIEW_SELECTION_ISOLATION_ONLY; pełne G1 IN PROGRESS**. Ten wycinek zastępuje tymczasową odmowę `SelectedContext` opisaną we wcześniejszym stanie [G1_PREVIEW.md](G1_PREVIEW.md). Nie implementuje commitu generatora ani GUI wyboru maski.

## Zmiana

`TileOperations::markSelectionChanged` sprawdza teraz ważność pozycji oraz tożsamość: powiadamia zaznaczenie bieżącego edytora wyłącznie wtedy, gdy odczytany z jego mapy kafel jest dokładnie przekazanym obiektem. Kopia o tych samych współrzędnych, kopia korzystająca z TileLocation mapy oraz kafel innej mapy nie powiadamiają aktywnego edytora. Odczyt mapy jest const i nie tworzy struktur ani alokacji. Nie dodano globalnego wyciszania, TLS, nowych opcji operacji ani hooków testowych.

W sandboxie usunięto `selectedCell`, kontrolę `SelectedContext`, wartość tego enum i zbędne include. Placement i borderowanie nadal korzystają ze wspólnych implementacji. Zaznaczony ground w niezmienionym halo może zostać bezpiecznie skopiowany i sprawdzony. Flagi źródła i cache zaznaczenia obu edytorów pozostają niezmienione.

Ochrona pełnego diffu pozostaje aktywna. Zaznaczony border odtworzony przez wspólny kalkulator staje się niezaznaczonym itemem; również przy identycznych ID stanowi to rzeczywistą zmianę chronionych danych, więc cały kandydat otrzymuje `OutOfMask`. Nie odtwarza się flag sztucznie i nie przycina diffu. Próba kontrolna z tym samym niezaznaczonym borderem przechodzi.

Puste zapisywalne pole z samym bitem `TILESTATE_SELECTED` jest nadal dozwolonym wejściem `empty_only`. W buforze stan jest przeliczany przez wspólne operacje: nowy ground jest niezaznaczony. Źródłowy bit i zaznaczenie edytora pozostają bez zmian. Nie jest to jeszcze decyzja o zaznaczeniu wyniku po zatwierdzeniu.

## Weryfikacja rzeczywistego edytora

`tests/test_procedural_preview.cpp` uruchamiany jako testowy wxModule z normalnym Application tworzy dwie prywatne, puste karty. Nie otwiera map. Obie mapy mają po jednym kontrolnym kaflu w zaznaczeniu, a generacja działa także na mapie aktywnej i nieaktywnej.

Unieważnienie cache sprawdzono przez publiczne `minPosition`/`maxPosition`: po primingu cache test na moment zmienia pozycję jedynego kontrolnego TileLocation, odczytuje granice i natychmiast przywraca pozycję, również po wyjątku. Między zmianą i przywróceniem nie ma yield, operacji na mapie ani GUI. To oracle dla flagi dirtiness w prywatnym fixture, nie sposób edytowania map. Kontrole dodatnie dla prawdziwego kafla (`markSelectionChanged`, zmiana zaznaczenia ground oraz `Tile::modify`) potwierdziły czułość oracle. Nie dodano dostępu do prywatnych pól produkcji.

Rzeczywisty listener `app.events:on('selectionChange', ...)` potwierdził brak niechcianych zdarzeń dla kopii, innej mapy i podglądu. Kontrola dodatnia używa jawnego powiadomienia podczas pustej, zwykłej sesji selection. Ten test nie dowodzi emisji zdarzenia dla niepustych akcji zaznaczenia: zagnieżdżone sesje INTERNAL mogą wyczyścić `selectionChanged`; istniejące zachowanie nie było zmieniane.

Przeszły:

- kopia z własnym TileLocation, kopia akcji korzystająca z lokalizacji żywej mapy, kafel innej mapy o tych samych współrzędnych, nullptr i nieprawidłowa pozycja;
- bezpieczny zaznaczony kontekst na aktywnej i nieaktywnej mapie, bez zmiany source/cache/eventów; zaznaczony border z UID/AID odrzucony, niezaznaczony identyczny border zaakceptowany;
- zwykłe usunięcie zaznaczenia, Undo/Redo i końcowe Undo, granice i tożsamość wskaźnika selected tile po wymianie;
- rzeczywisty Lua setter `app.map:getTile(...).ground.actionId`, Undo/Redo i końcowe Undo z zachowaniem selected tile i granic;
- pełne wcześniejsze core → resolver → borderowany sandbox 100×100/500×500, powtórzenia, limity, globalny RNG, Cancel, stale epoch i 79 faultów alokacji; fault fixture zawiera teraz zaznaczone puste pole, a każdy przypadek sprawdza też cache obu edytorów.

Prywatne historie i flagi dirty fixture wyczyszczono po regresji, zamknięto karty, wyrejestrowano listener i wykonano normalny shutdown aplikacji. Nie zapisywano OTBM. Okna informacyjnych ostrzeżeń assets obsłużono przez Computer Use wyłącznie w prywatnym procesie.

## Dowody i delegacja

- `local-data/procedural-smoke/g1-selection-canonical-20261102.log`: kanoniczny build Windows x64, exit 0. Normalny EXE `7048a947d419bd57dc8d1757548b9708172e5d813348d1c898765066ec17dc79`.
- `g1-selection-lua-rme-20261102/verification.json`: PASS, exit 0, pusty stderr, normalny shutdown i niezmienione źródłowe assets.
- `g1-selection-lua-rme-20261102/current-verification.json`: **PASS_G1_PREVIEW_SELECTION_ISOLATION_ONLY**, 415 aktualnych obiektów/źródeł/probe/normalny EXE; 877 źródeł bazy G0 ma wyłącznie dziesięć oczekiwanych różnic G1. Nową różnicą w istniejących źródłach tego wycinka jest wyłącznie `source/map/tile_operations.cpp`; wspólne zmiany G0 zachowano.
- Claude read-only review `1cc3531a-550e-430b-b56a-29ba93fe2d44`: zakończony bez odmów i bez zmian checkoutu. Sprawdził kolejność operacji Action/Selection/draw/Lua oraz const lookup, nie znalazł kontrprzykładu dla guardu. Wskazał granice selected-border/empty-tile/eventów, które uwzględniono. Raport/stdout/stderr i Git before/after sprawdzono. Claude nie kompilował ani nie wykonywał testów; testy przeprowadził niezależnie Codex.

Odtworzenie na nowym `OUTPUT`, po kanonicznym buildzie, z istniejącym prywatnym profilem assets:

```text
rtk proxy python docs/procedural/smoke/g1_run_resolver_probe.py OUTPUT PRIVATE_PROFILE_DATA PRIVATE_CONFIG --preview
rtk proxy python docs/procedural/smoke/g1_verify_preview_probe.py OUTPUT --selection
```

Zachowano próby nieudane: `g1-selection-rme-20261102` — brak include ActionQueue w harnessie; `g1-selection-observers-rme-20261102` — FAIL końcowego skryptu z nieistniejącym `app.getMap()`, mimo wcześniejszych zaliczonych przypadków. Poprawiono test na istniejące `app.map` i `actionId`, po czym wykonano nowy pełny przebieg. Nie przemianowano wcześniejszych prób na PASS.

## Pozostała bramka

Nie zweryfikowano GUI capture dokładnego zaznaczenia, jego staleness ani przynależności pustych kafli do zaznaczenia przy przyszłym commit. `Selection::add` może umieścić puste pole w kolekcji bez bitu selected; granica commitu musi uwzględniać membership i tożsamość, a nie tylko `Tile::isSelected`. Nie implementowano teraz takiej transakcji ani nie zaliczono Undo/Redo generatora na podstawie regresji zwykłych akcji.

Otwarte pozostają pełny budżet pracy/RSS, GUI Cancel i harmonogram, rewalidacja relevant assets/map/mask/selection, atomowy commit dokładnego diffu i jego błędy/historia wobec ręcznego rysowania, minimalny dialog/renderowany preview, pełny GUI smoke i OTBM round-trip 100×100/500×500 oraz przenośność kolejności równych z. G2 nierozpoczęte. Mapy użytkownika/companions, Docker, baza i świat nietknięte; bez commitów i push.
