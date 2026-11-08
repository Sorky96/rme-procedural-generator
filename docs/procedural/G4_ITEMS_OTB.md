# G4-0 — zgodność `data/1098/items.otb` z klientem zwykłego 10.98 (D11, część), 2026-11-07

**PARTIAL.** Zgodność `items.otb` z `Tibia.dat` zmierzona i spójna; nie dowodzi, że mapy powstały dokładnie z tym plikiem. Decyzja o zamknięciu tej części D11 należy do użytkownika.

## Co zmierzono
Skrypt [g4_check_items_otb.py](smoke/g4_check_items_otb.py) (tylko do odczytu, używa `tools/tibia_data.py`, niezależny od edytora) porównuje każdy item OTB, który ma client id, z rekordem tego client id w `Tibia.dat` zwykłego 10.98.

| Pozycja | Wynik |
| --- | --- |
| `items.otb` (SHA-256) | `9f73f8bf…67ee`, nagłówek OTB 3.57.62-10.98, 26 282 itemów, 734 „deprecated” |
| `Tibia.dat` (SHA-256) | `abb36b19…d8338`, 23 725 itemów (zakres 100–23 725) |
| Itemy OTB porównane z DAT | 25 548 (reszta: deprecated) |
| Itemy OTB bez client id / z client id poza zakresem DAT | 0 / 0 |
| Niezgodności: ground, unpassable, stackable, container, blockMissiles, pickupable | 0, 0, 0, 0, 0, 0 |
| Nagłówek 19 map (items major.minor) | 3.57 na wszystkich 19, wszystkie `compatible` |
| `unknown` na mapach | tylko M01 (43 ground + 47 itemów, 14 id) i M06 (48 itemów, 15 id „deprecated”); pozostałe 17 map: 0 |

Dwie serie skryptu dały bajtowo identyczny JSON (jedna maszyna).

## Wniosek i granice
- OTB i DAT są ze sobą spójne w sześciu testowanych cechach dla wszystkich 25 548 itemów. Przesunięty lub cudzy OTB dałby masowe niezgodności; zero niezgodności mocno wskazuje, że to właściwa para.
- **Nie zweryfikowano:** że mapy zostały zapisane z tym konkretnym `items.otb`. Obok map nie ma innego OTB do porównania; dowodem pośrednim jest wersja 3.57 w nagłówku wszystkich map oraz brak `unknown` na 17 mapach. Mapa zapisana z innym OTB o tych samych flagach, ale innych id, nie zostałaby wykryta.
- Nie sprawdzono innych cech (np. floorChange, hangable, elevation), nazw ani atrybutów XML.
- Nadal otwarte w D11: pliki towarzyszące, kontenery, HTML, druga maszyna.
