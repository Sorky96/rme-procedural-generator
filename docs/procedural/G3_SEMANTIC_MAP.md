# G3 — eksport mapy semantycznej, 2026-11-07

**PASS dla 20 map (zakres opisany niżej); G3 jako całość: patrz [G3_GATE_STATUS.md](G3_GATE_STATUS.md).**

## Co jest
- Rdzeń `source/procedural/core/png_writer.*` (bez zależności): koder 8-bitowego indeksowanego PNG ze „stored” blokami deflate. Wynik zależy tylko od wejścia, więc te same dane dają te same bajty; plik ma rozmiar zbliżony do surowych pikseli.
- Rdzeń `source/procedural/core/semantic_image.*`: `renderSemanticImages` zamienia `SemanticMap` i `TopologyReport` w jeden PNG na piętro (jeden piksel na kafel, piksel (0,0) to róg prostokąta piętra) i legendę. Klasy w kolejności priorytetu: nieznana zawartość, ściana, drzwi, teleport, zmiana piętra, blokada przedmiotem na przechodnim gruncie, potem kolor klucza gruntu (klucze nieprzechodnie ciemniejsze), kafel bez gruntu, grunt bez pędzla, brak kafla. Kolory z całkowitoliczbowego HSV, więc paleta jest dokładna. Powyżej 247 kluczy gruntu sloty się powtarzają, a legenda wymienia każdy klucz przy jego slocie.
- `renderSemanticLegendJson`: legenda, rozmiary i przesunięcia prostokątów pięter (współrzędne mapy: tylko do użytku lokalnego, nie do repo). Anulowanie `stop_token` co wiersz; pamięć robocza ok. 2 B na komórkę największego piętra (wliczona w `analysisWorkingBytes`, teraz 16 B na komórkę największego piętra).
- Próbnik zapisuje `analysis/semantic/floor-NN.png` i `legend.json`.

## Testy i dowody
- `test_procedural_semantic_image` PASS: koder sprawdzony niezależnymi implementacjami CRC i Adler w teście (znana wartość CRC dla `IEND`), obraz wielobłokowy (>65 535 B), odmowy (zły rozmiar, paleta, indeks poza paletą), ręcznie policzona siatka 7×4 z każdą klasą, przesunięcie z ujemnym x, ponowne użycie slotów przy 250 kluczach, anulowanie, niezgodna topologia, powtarzalność.
- `g3_verify_semantic_png.py` dekoduje PNG biblioteką standardową Pythona (`zlib`, `struct`), a nie kodem, który jest testowany, i porównuje z raportem: CRC wszystkich chunków, filtry, wymiary równe prostokątowi piętra, piksele niepuste = kafle, piksele „unknown” = komórki unknown, piksele ścian/drzwi/teleportów/zmian pięter nie przekraczają liczników cech, paleta zgodna z legendą. Wynik: wszystkie kontrole PASS dla 19 map zwykłego 10.98 (łącznie 707 kontroli, od 9 do 129 na mapę) i `forgotten.otbm` (129/129).
- Dwie pełne serie (19 map): 105 plików w `semantic/` bajtowo identycznych. Pozostałe raporty (190 plików) bez zmian względem serii sprzed eksportu.
- Szczyt pamięci mapy 71 MiB po dodaniu eksportu: 1 899 MiB (bez zmiany względem 1 898–1 899 MiB wcześniej, w granicach pomiaru).
- Obejrzano jeden obraz (piętro 7 `forgotten.otbm`): czytelne wyspy, morze jako obszar nieprzechodni, budynki i oznaczone elementy.

## Czego NIE zweryfikowano
- Wizualnie obejrzano tylko jedno piętro jednej mapy; pozostałych obrazów nie oceniano. Poprawność klasyfikacji pikseli potwierdza zgodność liczników i test ręczny, nie ocena wzrokowa.
- Niezależny weryfikator sprawdza sumy i strukturę, nie położenie pojedynczych pikseli na prawdziwych mapach (to sprawdza tylko test ręcznie policzonej siatki).
- PNG ma rozmiar zbliżony do surowych pikseli (bez kompresji): mapy 5000×5000 dają ok. 25 MB na piętro; nie badano, czy to przeszkadza przy większych mapach.
- Powtarzalność: jedna maszyna, jeden EXE.
