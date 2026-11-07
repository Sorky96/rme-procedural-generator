# G3 — stan bramki „Import i analiza map referencyjnych”, 2026-11-07

**Nie zamykam bramki sam.** Kryterium bramki jest spełnione dla 20 map (19 zwykłego 10.98 i jednej BlackTek), ale zakres z planu nie jest wykonany w całości (patrz „Luki”). Propozycja do decyzji użytkownika: zamknąć G3 z długiem (jak G1/G2), z listą luk poniżej.

**Bramka z planu:** „dla każdej mapy referencyjnej generowany jest powtarzalny raport, statystyki i mapa semantyczna.”

## Kryteria i dowody
| Kryterium | Dowód | Status |
| --- | --- | --- |
| Powtarzalny raport | Dwie pełne serie 19 map: 190 plików (3 raporty JSON + 7 plików analizy na mapę) bajtowo identycznych, 0 różnic ([G3_REFERENCES.md](G3_REFERENCES.md)); `forgotten.otbm` identyczny przed i po zmianach ([G3_FEATURES.md](G3_FEATURES.md)) | PASS (jedna maszyna, jeden EXE, bez testu innego locale) |
| Statystyki | Zgodność z niezależnym oknem Map → Statistics w RME: kafle, itemy i kafle nieprzechodnie identyczne dla `forgotten.otbm` ([G3_REFERENCES.md](G3_REFERENCES.md#kontrola-niezależna-statystyki-rme-gui-ścieżka-edytora)). Import, `unknown` z pokryciem, cechy per piętro, regiony i składowe, sąsiedztwo gruntów, bloki 16/32/64, cechy vs odległość od gruntu nieprzechodniego; JSON, MD, CSV ([G3_NORMALIZER.md](G3_NORMALIZER.md), [G3_TOPOLOGY.md](G3_TOPOLOGY.md), [G3_FEATURES.md](G3_FEATURES.md)) | PASS w opisanym zakresie |
| Mapa semantyczna | `SemanticMap` w pamięci i zapisywany artefakt: deterministyczny PNG na piętro + `legend.json` ([G3_SEMANTIC_MAP.md](G3_SEMANTIC_MAP.md)). Weryfikator niezależny od kodu: 20 map × wszystkie piętra, wszystkie kontrole PASS; dwie serie 105 plików bajtowo identycznych | PASS |
| Lokalny import OTBM, zgodność item/asset | [G3_IMPORT.md](G3_IMPORT.md); osobni klienci „10.98” i „1098-BlackTek”, runner i próbnik odmawiają niewłaściwego klienta | PASS |
| `unknown` bez zgadywania | Fixture z brakującymi id (8/8) oraz prawdziwe `unknown` na M01 i M06, wyjaśnione: 14 id spoza items.otb i 15 id „deprecated” ([G3_REFERENCES.md](G3_REFERENCES.md)) | PASS |
| Import tylko do odczytu, mapy prywatne | Hashe map i `Tibia.dat/spr` przed i po każdym przebiegu; raporty tylko w `local-data/` (git-ignored); w repo agregaty | PASS |
| Limity pamięci, anulowanie od początku | D8 zamknięty z długiem resztkowym: limit egzekwowany także w trakcie ładowania (hak w `IOMapOTBM`), anulowanie w ładowaniu 78–310 ms; ładowanie `Map` to 94 % szczytu; ścieżka edytora sprawdzona ręcznie przez użytkownika | PASS z długiem |

## Luki (nie zaliczone)
- **Cechy z sekcji 4.2 planu:** brak rzek (długość, krętość, szerokość, rozwidlenia), jaskiń (komory, korytarze, cykle, przewężenia), lasu (klastry), gór (ściany do plateau), szlaków i mostów, osad i budowli, mikrostruktur, wąskich gardeł oraz połączeń między piętrami. Regiony gruntu to nie nazwane biomy (nazwy byłyby zgadywaniem).
- **Test podobieństwa/memoryzacji** (plan 4.3) i podział train/validation/test (4.4): nie wykonano; to wejście dla G4.
- **Ocena sensowności:** cechy i mapy semantyczne są mierzone; poza jednym obejrzanym obrazem (piętro 7 `forgotten.otbm`) nie oceniano ich wizualnie ani merytorycznie.
- **HTML** jako format raportu: nie ma (wybrano JSON, MD, CSV).
- **Pliki towarzyszące** (house, spawn, zones) nie są wczytywane; itemy w kontenerach nie wnoszą cech.
- **Items.otb:** nie potwierdzono, że `data/1098/items.otb` to dokładnie plik, z którym powstały mapy zwykłego 10.98 (zgodność wskazuje na to, nie dowodzi).
- **Dług techniczny z tego etapu:** D8 (resztki: ładowanie `Map` 94 % szczytu, anulowanie przy normalizacji ok. 1,2 s), patrz [TECH_DEBT.md](TECH_DEBT.md). Powtarzalność sprawdzono na jednej maszynie.
- **Zatwierdzenia i commity:** zmiany z G3-E, D8 i eksportu semantycznego nie są scommitowane (stan na 2026-11-07).

## Wymagany test w GUI (użytkownik)
G3 nie zmienia interfejsu RME, więc nie ma testu GUI w tym etapie. Do oceny przez użytkownika są pliki PNG w `local-data/…/run/analysis/semantic/` (piętra) wraz z `legend.json`; można je otworzyć dowolnym przeglądarką obrazów.
