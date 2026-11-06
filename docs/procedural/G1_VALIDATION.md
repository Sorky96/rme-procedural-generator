# G1: walidacja przygotowanego wyniku

Nowszy wycinek: [G1_ASSETS.md](G1_ASSETS.md) dodaje sprawdzanie bieżących wartości katalogu definicji/ground rules/metadanych sprite także bez epoki VersionManager. Poniższy zakres z 2026-11-02 jest historyczny w tej części. Wyłączenia raw pixels/GPU/cache, końcowy guard edytora i atomowy commit pozostają otwarte.

Stan 2026-11-02: **PASS_G1_PREPARED_VALIDATION_ONLY; pełne G1 IN PROGRESS**. Wycinek uzupełnia [dokładną maskę Selection](G1_MASK.md) i [wspólny budżet przygotowania](G1_BUDGET.md). Dotyczy kontroli bieżącej zgodności wyniku; nie dodaje zatwierdzania mapy.

## Kontrakt

`validatePreparedTerrain` w `source/procedural/adapter/terrain_validation.{h,cpp}` przyjmuje jawną mapę, `PreparedTerrain`, aktualne config/profile/mask oraz stop token. Działa synchronicznie na głównym wątku, bez alokacji, yield, losowania, ponownej generacji i zmian mapy, brushes, RNG lub cache zaznaczenia. Nie przełącza kart i może sprawdzać mapę nieaktywnego edytora. Caller zapewnia życie obiektów.

Kolejność kontroli i pierwszego zgłoszonego błędu:

1. `WrongThread` przed odczytem mapy/assets, następnie `Cancelled` i `NoClient`.
2. `StaleAssets`, gdy epoka VersionManager lub identyfikator klienta różnią się od przechwyconych. ID porównuje się przez bezalokacyjny widok.
3. `StaleRequest`, gdy zmienił się region/Z, seed, dowolna waga, nazwy brushes lub dokładna maska żądania. Pusta przekazana maska oznacza cały prostokąt i jest równoważna pełnej masce jedynek. Porównywane są także bity pól wyłączonych przez protection.
4. `InvalidPrepared` dla niespójnych rozmiarów/masek/protection, nieprawidłowych przechwyconych parametrów lub niezgodnego powiązania z preview.
5. `StaleMap`, gdy `snapshotMatchesMap` odrzuci inną instancję, revision, nagłówek, kopie kafli lub metadane lokalizacji. Kontrola pełnych wartości wykrywa też bezpośrednią zmianę danych w kontekście bez podbicia revision.
6. `InvalidPrepared`, jeśli bieżący payload preview nie daje tego samego fingerprintu co przygotowany wynik.

Przy kilku równoczesnych niezgodnościach znaczenie ma ta kolejność. Kontrola stop następuje między etapami i co 4096 pól w pętli maski. Skanowanie źródła, fingerprintu i powiązania preview jest synchroniczne, bez wewnętrznego poll stop; nie gwarantuje responsywnego Cancel podczas tych przebiegów.

## Powiązanie preview ze źródłem

`TerrainPreview` przechowuje dodatkowy prywatny `sourceFingerprint_`, wyliczony raz podczas przygotowania. `matchesSource(snapshot,plan)` sprawdza go wraz z istniejącymi prywatnymi config/map/asset stamps i fingerprintem planu. Obejmuje jawne pola config, extent, map stamps, długości i każdy bajt mask/protection, kontekst i nagłówek snapshotu, liczbę jego komórek, pełny config/stamps/client/profile planu, semantic fingerprint i konkretne ID. Nie używa adresów, paddingu ani globalnego RNG.

Publicznie modyfikowalny snapshot i plan nie mogą przez przypadek zostać połączone z innym zachowanym preview. Zmiana obu kopii maski lub profilu żądania razem nie przepisuje prywatnego fingerprintu. Przeniesiony pusty preview jest odrzucany przed odczytem bufora. Fingerprint źródła nie obejmuje payloadu oryginalnych kafli; te wartości są niezależnie porównywane z żywą mapą przez snapshot. Faktyczny bufor diffu ma osobne `currentFingerprint()` porównywane z zapamiętanym fingerprintem wyniku.

To 64-bitowe kontrole spójności, nie kryptograficzna ochrona przed celowym fałszowaniem danych C++. Dotychczasowe fingerprinty konkretnych wyników nie zmieniają się. Dodatkowe pole mieści się w istniejącym rozliczeniu opartym na `sizeof`: wspólna estymata rośnie o 16 B (agregat przygotowania i etap preview), bez nowej alokacji per tile lub per frame.

## Granice bezpieczeństwa

Sukces opisuje stan w chwili wywołania. Nie tworzy zachowanego uprawnienia do commitu: przyszły commit musi przeprowadzić końcowe kontrole bez yield przed mutacją i osobno sprawdzić życie/właściwy edytor, tryb offline oraz transakcję. Dla żądania opartego na żywym zaznaczeniu trzeba dodatkowo wywołać `validateGenerationSelection`; przekazanie starego bufora maski nie odczytuje automatycznie GUI.

Nie wykazano wykrywania wszystkich niezależnych zmian brushes/items/materials bez podbicia epoki VersionManager. Istniejące `currentFingerprint` odrzuca niektóre nieprawidłowe bieżące definicje, ale nie jest pełnym stemplem katalogów assets. Ten punkt pozostaje otwarty. Podobnie pamięć Undo, całe RSS, GUI Cancel/harmonogram, minimalny dialog/renderowane preview, atomowy commit/awarie/Undo/Redo i niezależna historia ręcznych akcji oraz GUI/OTBM round-trip 100×100/500×500.

## Delegacja i review

Claude wykonał wyłącznie dwa nowe pliki walidatora w osobnym checkout, run `b66c62c4-7051-40bd-a31e-d9d6509495d8`. Completed/exit 0, pusty stderr, brak odmów; report i stdout zgodne. Przejrzano nowe pliki i pełny tracked diff, który pozostaje identyczny z poprzednim przebiegiem workera. Worker nie uruchamiał builda ani testów. Codex dodał powiązanie w istniejącym preview/DrawOperations, CMake, niezależne testy, verifier i dokumentację. Dwa zaakceptowane pliki skopiowano byte-for-byte; manifest `local-data/procedural-smoke/g1-validation-integration-20261102.json`.

## Niezależna weryfikacja

Kanoniczny Windows x64 `build_ninja.bat`: exit 0, sześć kroków builda, `local-data/procedural-smoke/g1-validation-canonical-20261102.log`. Normalny EXE SHA-256: `47ab14b0cffa58270c3363f4d63610685246b47fe6413d1528e512b45041c300`.

Pełna próba normalnego Application z testowym wxModule: `local-data/procedural-smoke/g1-validation-rme-20261102`. **PASS, exit 0, normalny shutdown, pusty stderr, źródłowe assets bez zmian**. Verifier `g1_verify_preview_probe.py --validation` potwierdził 418 aktualnych obiektów RME, 877 źródeł bazy G0 i dziesięć oczekiwanych różnic G1. Dowody: `verification.json`, `current-verification.json`, log i manifest builda. Probe EXE SHA-256: `e16105a46e615b69c74126b18c077957996da26208aaf25e1dcfce3f3a238871`.

| Rozmiar | Alokacje wywołania walidacji | Fingerprint wyniku | Wspólna estymata przygotowania (B) |
|---|---:|---|---:|
| 100×100 | 0 | 18297947229083653543 | 7 234 214 |
| 500×500 | 0 | 5645408562528222048 | 168 808 374 |

Próba jednorazowego błędu pierwszego new podczas walidacji nie została wyzwolona: w obu rozmiarach licznik pozostał zerowy, a aktualny przeniesiony wynik przeszedł. Moved-from wynik odmówił. Każdy fingerprint jest identyczny ze ścieżką etapów i wcześniejszymi przebiegami. Piki śledzonych żądań new przygotowania pozostały 2 923 649 i 69 507 073 B, po destrukcji zero śledzonych bajtów. Estymaty są o 16 B większe, zgodnie z `sizeof`; nie jest to pomiar RSS ani teza o przyspieszeniu.

Przeszło **29 przypadków odmowy**: inna instancja mapy; zmiana każdego z dziewięciu pól config; zmiana profilu; zmieniona/niebinarna/krótka maska; pre-cancel i worker; zmienione konkretne ID, profil i semantic fingerprint planu; równoczesna zmiana przechwyconej i przekazanej maski; zmienione protection/extent i ucięty bufor wejścia; modyfikacja item-selection w wygenerowanym payloadzie; podmieszany preview innego seeda; zmiana kopii oryginalnego UID; bezpośrednia zmiana AID żywego kafla halo przy niezmienionej revision; zmiana town count lokalizacji; oraz podbita revision. Po odtworzeniu kontrolowanych wartości świeży wynik ponownie przechodzi, a źródło, globalny RNG i cache obu obserwowanych edytorów pozostają poprawne.

Oddzielnie przeszły brak klienta po unload oraz `StaleAssets` po ponownym załadowaniu tego samego klienta, a także walidacja wyników z dokładnych masek rzeczywistych aktywnego i nieaktywnego edytora. Powtórzono cały dotychczasowy probe: cztery sandboxy 100×100/500×500, pełne diff/protection/epoch, 79 faultów sandboxa, 106 faultów przygotowania i granice limitu, dokładne maski z otworami, cache/zdarzenia dwóch edytorów, selected-context/selected-border oraz zwykłe zaznaczenie i setter Lua z Undo/Redo. Te regresje nie zaliczają commitu generatora. Historyczne próby snapshotu i border-order mają niezmienione właściwe źródła, lecz starsze linkowane EXE; nie nazwano ich nowymi pełnymi przebiegami.

Startup czekał na informacyjny dialog `Warnings` prywatnego profilu: odwrócone ranges tileset i brakujące referencje ID. Przez skill computer-use odczytano rzeczywiste okno i zamknięto ten dialog przyciskiem OK. Kontynuowano ten sam proces, bez restartu i bez zmiany assets/produkcji. Dopiero potem moduł testowy rozpoczął pełną próbę. To przebieg z ręcznie obsłużonym startupem, nie unattended test. Końcowa inspekcja okien potwierdziła brak prywatnej instancji po normalnym wyjściu.

`git diff --check` przeszedł. Nie sprawdzono wszystkich mutacji niezależnych katalogów assets, zamknięcia źródłowego edytora przy późniejszym commit, staleness po rzeczywistym Undo/Redo generatora, wymuszonego stop w środku walidacji, GUI Cancel, trwałego OOM ani pełnego RSS. Nie rozpoczęto G2. Nie używano forgotten.otbm/companionów ani map użytkownika, Dockera, bazy lub świata. Zachowano wspólne niezacommitowane zmiany; bez commit/push.
