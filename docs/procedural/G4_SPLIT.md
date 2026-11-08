# G4-A — manifest referencji, podział po rodzinach, test podobieństwa (D10), 2026-11-07

**PARTIAL.** Zbudowano i zmierzono test podobieństwa fragmentów między mapami oraz podział leave-one-family-out dla 19 map zwykłego 10.98. **Nie** zbudowano jeszcze testu memoryzacji dla wyniku generatora (wymaga mapowania klucz gruntu → rodzaj powierzchni z G4-B) ani podziału dla BlackTek (jedna mapa).

## Co jest
- `source/procedural/core/similarity.*`: odcisk okien 16×16 (hash nazw gruntu i obecności kafla, bez pozycji i piętra, więc kopia przesunięta lub przeniesiona na inne piętro jest wykrywana; próbkowanie po treści, `hash % 4 == 0`; pomija okna rzadkie (<50 % kafli) i jednolite). `compareFingerprints` (zawieranie w ‰), `withoutGenericWindows` (okna obecne w ≥ N/4 map to wzorce ogólne, nie dowód kopiowania), serializacja tekstowa.
- `source/procedural/core/reference_split.*`: manifest `g4-manifest/1` (alias, sha256, klient, rola style/sample/shape, rodzina; jeden klient na manifest), rodziny = deklarowane ∪ zawieranie ≥ 100 ‰ przy ≥ 16 wspólnych oknach (jedno-powiązanie), foldy leave-one-family-out: test = rodzina, walidacja = następna rodzina (cyklicznie), reszta = trening; maski wyłączone: sample i shape nigdy nie trafiają do foldów.
- Narzędzie `reference_split` (tylko rdzeń, bez GUI) i `docs/procedural/smoke/g4_make_manifest.py`; próbnik G3 zapisuje dodatkowo `analysis/fingerprint.fp`.
- Test: `test_procedural_similarity` PASS (kopia przesunięta, na innym piętrze i z innym układem kluczy: dokładnie 225 wspólnych okien z bloku 30×30; mapy niezależne: 0; jednolite i rzadkie pominięte; odmowy; okrągła serializacja; manifest; rodziny po treści; foldy bez przecieku i bez map spoza stylu; niezależność od kolejności wpisów; filtr okien ogólnych; minimalna liczba wspólnych okien).

## Parametry ustalone przed wynikami (żaden nie dotyczy metryk stylu)
okno 16, próbkowanie 1/4, rzadkie < 50 %, scalanie ≥ 100 ‰ i ≥ 16 wspólnych okien. **Zmiana po pierwszym przebiegu (jawnie):** pierwszy przebieg (bez filtra okien ogólnych) połączył 9 z 12 map stylu w jedną rodzinę, bo małe mapy mają wspólne wzorce ogólne (np. M13 w 82 % zawarta jednocześnie w M17 i M19), co dawało 3 foldy. Dodałem filtr „okno obecne w ≥ N/4 map jest ogólne” (N = 19, próg 5) i minimum 16 wspólnych okien, zanim obliczono jakąkolwiek metrykę stylu. Wartość progu 1/4 wybrałem z danych o rozkładzie nakładania, nie z zasady niezależnej od nich; to założenie, nie dowód.

## Wynik (19 map, klient 10.98, role: 12 style, 3 sample, 4 shape)
- **Kontrola pozytywna:** pary shape M07–M09 i M08–M10 mają 1000 ‰ w obu kierunkach (jedna rodzina, jak zakładano z nazw).
- **Rodziny po treści (poza deklarowanymi):** M13 jest w 70–71 % zawarta w M17 i w M19 (po 83–84 wspólne okna swoiste), M15 i M16 łączą się z M17 (141 i 104 ‰). Powstaje jedna rodzina 7 map (M12–M17, M19) obok M01, M05, M06 i zimy (M11+M18). Pierwszy, nieskorygowany przebieg: [wynik](#pierwszy-przebieg-bez-filtra) poniżej.
- **5 foldów** (testowa rodzina → walidacja): rodzina 7 map → M01; M01 → M05; M05 → M06; M06 → zima; zima → rodzina 7 map. `max_leak` między testem a resztą: 45, 45, 9, 29, 0 ‰ (poniżej 100 ‰ z konstrukcji).
- **Obserwacja pod progiem:** M06 (origmap) i M19 (world) mają 2 072 wspólnych okien swoistych (2 ‰ M06, 5 ‰ M19); M06 i M08/M10 po 1 809. To mała część, ale w liczbach bezwzględnych nie zero; w foldzie testowym M06 przeciek wynosi 29 ‰. Nie rozstrzygnięto, czy to wspólne źródło, czy powtarzalny wzorzec terenu.
- **Nie do porównania (brak okien):** M02, M03 (mniejsze niż okno 16 po odfiltrowaniu rzadkich). M04 ma 182 okna.
- **Powtarzalność:** dwie serie 19 fingerprintów bajtowo identyczne (0 różnic), raport podziału bajtowo identyczny; raporty G3 (190 plików) bez zmian po rozszerzeniu próbnika.

## Pierwszy przebieg (bez filtra)
Rodzina „caves” = M01, M06, M12–M17, M19 (9 map), 3 foldy, jeden z testem na 9 mapach. Odrzucone jako zdegenerowane; zachowane dla przejrzystości (plik `split1.json` lokalnie).

## Czego NIE zweryfikowano
- Czy okna 16×16 to właściwa skala (testowano jedną); czy ground-only wystarcza (bez traitów i itemów).
- Czy pokrewieństwo treści = pokrewieństwo stylu; rodziny deklarowane z nazw plików (swamp, winter, caves) są założeniem.
- Foldy są niezrównoważone (7 map vs 1); ważenie per mapa/rodzina dopiero w G4-C/F.
- Testu memoryzacji wyniku generatora; BlackTek; jedna maszyna.
