# G1-B — plan placementu ground i rewizja assets

Status: implementacja planu, bez sandboxu/diffu/commitu. Cała bramka G1 pozostaje otwarta.

`source/procedural/adapter/ground_resolver.*` mapuje Grass/Dirt/Water na trzy nazwy brushy bieżącego klienta. Na głównym wątku sprawdza załadowanego klienta, rewizję przechwyconą w GenerationInput, region/rozmiar/enum siatki, maskę/ochronę i budżet payloadu planu. Odrzuca brak brusha, nieprawidłowe szanse, brakujące/meta itemy, brak wizualnego client ID lub niespójny ground/brush. Nie zastępuje błędów pierwszym znalezionym brushem ani hardkodowanym item ID.

Plan przechowuje własne nazwy, klienta, config, tożsamość/rewizję mapy i assets, fingerprint siatki oraz płaski wektor placement IDs. Zero oznacza Preserve, nigdy usunięcie. Fingerprint planu obejmuje fingerprint semantyczny i jawne bajty IDs; nie jest pełnym provenance ani hashem konkretnego diffu po borderowaniu. Sam plan nie jest dopuszczeniem commitu.

Wybór wariantu korzysta z istniejącego `GroundBrush::getPlacementItemIdForRoll`, zachowując ścisłe `<` i fallback. Trzy niezależne strumienie v1 (grass/dirt/water) wykonują po jednym rollu każdego terenu na każdej pozycji, również wykluczonej. Zmiana samej maski/ochrony nie przesuwa wariantów pozostałych pól. Nie ma globalnego RNG ani mutacji mapy/aktywnego brusha. Warianty o wadze zero pozostają prawidłowymi członkami brusha; powtarzające się skumulowane szanse są dopuszczalne zgodnie z loaderem RME.

`maxPlanBytes` rozlicza strukturę, wektor IDs i skopiowane znaki nazw/klienta z terminatorami. Nie jest limitem całego zadania/RSS, allocator overhead, trzech silników RNG, snapshotu/halo, konkretnych tile ani Undo. Resolver jest synchroniczny na głównym wątku; nie stanowi gotowej obsługi przycisku Cancel w GUI. Token jest sprawdzany na wejściu, przy walidacji wariantów i co 4096 pozycji oraz przed zwrotem.

## Delegacja Claude

Claude wykonał implementację `VersionManager::GetAssetRevision()` w czystym osobnym worktree na gałęzi `feature/procedural-g1-asset-revision`, run `e14767b5-9984-4e39-8bf7-ddd8b20b88ce`. Własność obejmowała wyłącznie dwa pliki VersionManager. Raport, stdout/stderr, pełny diff i status sprawdzono; bez odmów, innych plików, commitów, powłoki lub testów workera. Codex zintegrował zmianę po niezależnym odczycie ścieżek load/unload, skracając komentarze.

Licznik startuje od 0 i rośnie przed pierwszą operacją każdego UnloadVersion. Zmiana klienta, wymuszony reload tego samego ID, unload pustego stanu i nieudane ładowanie po teardown unieważniają starą rewizję. No-op LoadVersion zachowuje ją. Licznik jest tylko dla operacji serializowanych na głównym wątku. Nie obejmuje niezależnych edycji definicji/brushy/creature poza VersionManager i sam nie dowodzi niezmienności wszystkich assets. Te ścieżki nadal wymagają kontroli resolvera i walidacji przed commit.

## Testy i stan dowodów

Pierwszy kanoniczny build resolvera PASS 0: `g1-resolver-canonical-20261102.log`. Pierwszy rzeczywisty probe z klientem 1098-BlackTek zakończył się **FAIL, exit 9**, `g1-resolver-rme-20261102/verification.json`. Wykrył błędną odmowę poprawnych wariantów o zerowej wadze (powtórzone szanse skumulowane). Poprawiono warunek. Przed testem trzeba było zamknąć rzeczywisty dialog ostrzeżeń startowych; nie uruchomiono kolejnej kopii po oczekiwaniu. Oryginalne assets pozostały identyczne.

Aktualna próba po poprawce i integracji rewizji **PASS, normalny exit 0, stderr pusty**: `g1-resolver-revision-rme-20261102/verification.json`. `tests/test_procedural_resolver.cpp` wykonał rzeczywiste RME/capture/core/brushes przy 100×100/500×500: powtarzalność IDs, przynależność do mapowanych brushy, niezależność od maski i globalnego RNG, budżet, brak klienta/brusha/ważnego itemu, tamper maski, pre-Cancel i odmowa z workera. Potwierdził zachowanie chronionego UID/AID/PZ i temple bez mutacji mapy. Osobno przeszedł no-op, udany wymuszony reload, nieudany reload oraz explicit/empty unload; rewizja 3→9, stare wejście odrzucone. Awaria ładowania dotyczyła wyłącznie nowej prywatnej kopii katalogu assets pod sprawdzonym markerem probe; źródłowy profil pozostał identyczny.

Kanoniczny build po integracji PASS 0, 178 kroków: `g1-resolver-revision-canonical-20261102.log`. Pojedynczy czas samego resolvera wyniósł 0,2923 ms / 10000 pozycji i 6,7105 ms / 250000 pozycji; fingerprinty 15173774556512220455 i 2039901658534265928. To nie pełny runtime/pamięć generatora ani teza o przyspieszeniu. Test uruchomił normalny startup RME i zamknął go normalnie; zamknięto wyłącznie jego dialog ostrzeżeń startowych przez computer-use. Nie wykonano wizualnej oceny wygenerowanego terenu, ponieważ ten pakiet nie rysuje mapy. Osobny worktree Claude zachowano: aplikacja oznaczyła go jako primary i odmówiła archiwizacji; źródłowy checkout nie jest managed worktree, więc attach również odmówiono. Nie usuwano go ręcznie.

Nadal niewykonane: snapshot tile/halo i związanie go z wynikiem, DrawOperations placement/border sandbox, kontrola całego konkretnego diffu, pełny budżet/peak, walidacja stale map/selection/relevant assets przed commit, atomowy commit i jego awarie, oddzielne Undo/Redo, dialog i pełne E2E/round-trip. Nie zaliczać testu samych placement IDs jako bramki G1. Nie rozpoczynać G2.
