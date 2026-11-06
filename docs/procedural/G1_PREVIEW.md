# G1: sandbox placementu i borderowanego diffu

Stan 2026-11-02: **G1 IN PROGRESS**. Ten etap nie zapisuje zmian do edytowanej mapy i nie zalicza commitu, Undo/Redo ani całego workflow G1.

**Nowszy wycinek 2026-11-02:** [G1_SELECTION.md](G1_SELECTION.md) zamyka izolację cache zaznaczenia i usuwa tymczasowe `SelectedContext`. Bieżący sandbox dopuszcza bezpieczny zaznaczony kontekst; chronione rzeczywiste zmiany nadal odrzuca. Poniższe dowody i opis pierwotnej odmowy są historią wcześniejszego etapu. Aktualny build i pełny wynik regresji dwóch edytorów znajdują się w nowym raporcie.

## Wykonany kontrakt

`DrawOperations::prepareTerrainPreview` przyjmuje wskazaną mapę, własny snapshot z halo dwóch pól, konkretny `GroundPlan`, jawne limity liczby zmian/pamięci i `stop_token`. Działa synchronicznie na wątku GUI, bez yield i bez przełączania aktywnego brusha. Sprawdza klienta, epokę assets, generację/rewizję mapy, pełną zawartość snapshotu i metadane lokalizacji, geometrię halo, maski, ochronę `empty_only`, profil i przynależność placement IDs do bieżących brushy.

Własny `BaseMap` otrzymuje głębokie kopie danych. Placement korzysta z istniejącego `TerrainPlacement`, w tym interpretacji `BorderBaseGroundId`. Borderowanie dotyka tylko placementów i ich ośmiu sąsiadów, przez rzeczywisty `GroundBorderCalculator`. Generator włącza deduplikację reguł szczególnych zachowującą pierwsze wystąpienie w kolejności sąsiadów. Domyślna ścieżka zwykłego malowania zachowuje sortowanie adresów.

Pełny diff porównuje ground, stos, atrybuty, zachowane nieznane dane, encje i metadane kafla. Przy klasyfikacji pomija wyłącznie znane pochodne flagi/cache minimapy; nie pomija nieznanych bitów, zaznaczenia, modified ani optional border. Każda rzeczywista zmiana poza maską lub na chronionym polu odrzuca cały kandydat. Niezmienione kopie są usuwane z bufora dopiero po zakończeniu borderowania. Źródło pozostaje tylko do odczytu.

`TerrainPreview` posiada bufor oraz uporządkowane row-major pozycje diffu. Fingerprint obejmuje rzeczywiste ID/subtype/selected wygenerowanych zwykłych itemów, kolejność stosu, pozycje i stan kafli; odrzuca nieoczekiwane klasy/item attributes/encje/metadane zamiast ukrywać je za hashem samych ID. Ponowne liczenie sprawdza epokę przed odczytem definicji. Dostęp do bufora obowiązuje dla niewyprowadzonego obiektu; po przeniesieniu `currentFingerprint` odmawia.

## Granice bezpieczeństwa i pamięci

Wspólne `TileOperations::update` powiadamia zaznaczenie bieżącego edytora również przy aktualizacji odłączonych kopii. Dlatego **każdy zaznaczony kafel/ground/item/creature/spawn w dotykanym kontekście powoduje `SelectedContext` przed klonowaniem i mutacją sandboxa**. To świadoma odmowa, nie rozwiązanie kompletnego workflow zaznaczenia G1. Nie zmieniono globalnego zachowania `TileOperations` i nie dodano wyciszania powiadomień.

Budżet obejmuje estymatę własnego sandboxa: strukturę BaseMap/grid/cells/nodes/floors, kopie payloadu, wektory, zmiany i tymczasowy item placementu. Przed każdym borderowaniem sprawdzany jest zapas na maksymalnie 128 nowych itemów, wzrost buforów stosu i scratch. Usuwanie borderów nie zwalnia rozliczonego limitu, bo wektor może zachować capacity. Arytmetyka sprawdza limit przed mnożeniem. To konserwatywna estymata oparta na układzie standardowych kontenerów, **nie twardy limit RSS ani budżet całej pracy**. Snapshot/core/plan/Undo muszą zostać rozliczone wspólnie w kolejnej integracji.

Anulowanie z tokenu jest sprawdzane przed pracą i między wierszami/etapami. Nie ma jeszcze obsługi kliknięcia Cancel ani asynchronicznego harmonogramu sandboxa. Bieżące synchroniczne przygotowanie na GUI nie stanowi dowodu braku zamrożenia interfejsu. Sortowanie klastrów o równym z pozostaje wspólnym `ranges::sort`; nie zweryfikowano identyczności między różnymi bibliotekami standardowymi/platformami.

## Delegacja i przegląd

Claude implementował wyłącznie `ground_border_calculator.{h,cpp}` i osobny test rzeczywistych nakładających się reguł, w dedykowanym worktree. Run `91c89455-6c4e-4bcf-88b2-8472a6c83f7f` zakończony, bez odmów i bez testów po stronie workera. Codex przejrzał pełny diff/nowy plik, przeniósł tylko te pliki i niezależnie zbudował oraz uruchomił test.

Read-only review Claude `4a58a62d-c51c-48d2-b85f-056a069dcc3f` wykrył powiadamianie zaznaczenia przez kopie oraz rozbieżność resolver/preview dla base-ground variants. Dodano odmowę zaznaczonego kontekstu i obsługę zgodną z TerrainPlacement. Raport, stdout, pusty stderr oraz niezmieniony checkout workera sprawdzono; worker nie wykonywał komend ani testów. Nie traktuje się odpowiedzi CLI jako zaliczenia implementacji.

## Dotychczasowe dowody

Harnessy uruchamiane z głównego checkoutu, po kanonicznym buildzie `build_ninja.bat`, z `rtk proxy` i lokalnym Pythonem 3. Nowy `OUTPUT` musi znajdować się pod `local-data/procedural-smoke` i nie może już istnieć:

```text
rtk proxy python docs/procedural/smoke/g1_run_input_probe.py OUTPUT --border-order
rtk proxy python docs/procedural/smoke/g1_run_input_probe.py OUTPUT --snapshot
rtk proxy python docs/procedural/smoke/g1_run_resolver_probe.py OUTPUT PRIVATE_PROFILE_DATA PRIVATE_CONFIG --preview
rtk proxy python docs/procedural/smoke/g1_verify_preview_probe.py OUTPUT
```

Verifier jest przypisany do końcowego zestawu dowodów 2026-11-02, nie do dowolnego profilu/testu. Użyty profil źródłowy: `local-data/procedural-smoke/g0-reference-profile-20261030/data` i sąsiedni `config.toml`; wyłącznie wcześniej przygotowane prywatne assets 1098. Builder linkuje bieżące obiekty rzeczywistego RME; test preview korzysta z normalnego wejścia Application oraz testowego wxModule. Informacyjne okno ostrzeżeń assets zamknięto w tej prywatnej instancji przez Computer Use. Żadnej mapy nie otwierano i nic nie zapisywano przez OTBM.

- **Końcowy aktualny przebieg:** `g1-preview-reviewed-rme-20261102/verification.json` PASS/exit 0/pusty stderr/normalny shutdown/niezmienione źródłowe assets. Zawiera też Cancel w trakcie 500×500 (token z osobnego wątku, nie przycisk GUI), dokładną maskę z ochroną temple (42 zmiany), reload epoki, odmowę zaznaczonego pustego pola i zaznaczonego ground w halo oraz rzeczywisty wariant `BorderBaseGroundId` przez fabrykę i TerrainPlacement.
- `g1-preview-reviewed-rme-20261102/current-verification.json`: **PASS_G1_OWNED_BORDERED_PREVIEW_ONLY**, 415 bieżących obiektów/źródła/probe/normalny EXE; 877 źródeł bazy G0 ma wyłącznie dziewięć zamierzonych różnic G1. Kanoniczny `g1-preview-reviewed-canonical-20261102.log` PASS 0; normalny EXE `fdfc4632a0d2aef94c4ea1bf3fe50d4bd16c6931f82e91954e827891b560c6e9`. Dodatkowy CTest core PASS 1/1 Release. Nie wznowiono audytów G0.
- `g1-border-order-current-rme-20261102/verification.json`: PASS, exit 0, rzeczywisty silnik i kontrolowany dolny/górny adres reguły X, powtórzenie i pojedyncze wykonanie duplikatu; domyślna kolejność zachowana.
- `g1-preview-snapshot-regression-20261102/verification.json`: PASS, exit 0, regresja niezmienionych funkcji snapshotu/estymatora/pełnego porównania, w tym 24 błędy alokacji capture. Późniejsze poprawki sandboxa nie są objęte tym historycznym binarium.
- `g1-preview-protected-rme-20261102/verification.json`: PASS, exit 0, normalny shutdown, pusty stderr, źródłowe assets niezmienione. Rzeczywisty core → resolver → sandbox dla 100×100/500×500, jednolitej wody i mieszanki grass/dirt/water bez modyfikowania siatki semantycznej; powtarzalny pełny diff, globalny RNG, dokładny/za mały budżet, 79 faultów alokacji, ochrona UID/AID/PZ przy borderze. To przebieg sprzed końcowego rozszerzenia testów i poprawek po review.

Końcowe liczby dla mieszanki: 100×100 — 9983 border items, 10,8362 ms, estymata 6684400 B, fingerprint `18297947229083653543`; 500×500 — 247236 border items, 332,764 ms, estymata 157330560 B, fingerprint `5645408562528222048`. Są to pomiary samego przygotowania sandboxa w tej prywatnej próbie, nie porównanie wydajności, peak RSS lub czas całego workflow. Pełne konkretne kafle powtórzeń porównano oprócz fingerprintów.

Zachowane nieudane próby: pierwszy `g1-border-order-rme-20261102` odrzucony przez kontrolę nieaktualnego obiektu, pierwszy preview zakończony FAIL źle dobranego scenariusza ochrony pustego kafla, `g1-preview-real-mixed-rme-20261102` błąd brakującego include w harnessie. `g1-preview-final-rme-20261102` przekroczył timeout runnera w oczekiwaniu na informacyjny dialog assets; odblokowano ten sam proces, ale nie zaliczono tej próby jako kontrolowanego PASS. Wszystkie prywatne okna testowe zakończyły pracę. Nie przemianowano nieudanych/niepełnych prób na PASS. Builder sprawdza teraz także, czy probe lub źródło linkowanego obiektu nie zmieniło się w trakcie budowy. Pierwsze dwa wywołania końcowego verifiera odrzuciły zbyt ścisły parser znacznika z liczbą oraz nieuwzględniony plik zasobów `.rc`; po poprawie harnessu sprawdzono ponownie te same artefakty, bez powtarzania runtime.

## Pozostała bramka G1

Przyjęcie całej pracy wymaga jeszcze integracji pełnego budżetu i stale mask/selection/relevant assets, rozwiązania zaznaczonego kontekstu, atomowego commitu dokładnie zatwierdzonego diffu przez Action/Change/ActionQueue, niezależnej historii wobec ręcznego rysowania i jej błędów/Undo/Redo, minimalnego dialogu/preview i GUI Cancel, rzeczywistego GUI smoke oraz OTBM round-trip 100×100/500×500. Nie rozpoczęto G2. Nie otwierano map użytkownika ani companionów, nie ingerowano w Docker, bazę lub świat.
