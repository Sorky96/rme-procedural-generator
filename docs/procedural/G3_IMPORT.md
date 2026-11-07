# G3-A — import tylko do odczytu i zgodność item/asset, 2026-11-07

**PARTIAL (G3-A).** Zrobione: nagłówek, SHA-256, zgodność z klientem, wczytanie kopii mapy, zliczenie `unknown`. Niezrobione: normalizator semantyczny (G3-B), regiony/biomy/topologia (G3-C), feature extraction i raport końcowy (G3-D). Bramka G3 nie jest zaliczona.

## Co jest
- Rdzeń: `source/procedural/core/reference_header.*` (SHA-256, `ReferenceHeader`, `checkCompatibility`). Test: `test_procedural_reference_header` (wektory NIST przy 6 rozmiarach porcji, 7 przypadków zgodności) PASS.
- Adapter: `source/procedural/adapter/reference_import.*`. Czyta nagłówek przez `IOMapOTBM::peekStartupInfo`, liczy SHA-256 pliku, ładuje mapę przez `IOMapOTBM::loadMap` na samym `Map` (bez `Editor`, bez zapisu; `Map::open` jest chroniona i ustawia ścieżkę zapisu, więc nie jest używana) i zlicza kafle, itemy i `unknown` (item `isInvalidOTBMItem()` lub `!typeExists()`).
- Próbnik: `docs/procedural/smoke/g3_reference_import_probe.cpp`, runner `g3_run_reference_import.py`. Runner kopiuje mapę do `local-data/` (git-ignored), hashuje źródło i towarzyszące `*-*.xml` przed i po, odmawia katalogu wyjścia poza `local-data/`.

## Wyniki (klient 1098-BlackTek)
| Mapa | Zgodność | Kafle | Itemy | unknown (ground/items) | Ostrzeżenia loadera |
| --- | --- | --- | --- | --- | --- |
| syntetyczny `g0-metadata.otbm` | compatible | 402 (zgodne ze znanym fixture) | 402 | 0/0 | 0 |
| `forgotten.otbm` (3 424 937 B, OTBM 3, 2048×2048, items 3.57) | compatible | 340 595 | 429 037 | 0/0 | 0 |

`forgotten.otbm`, kafle na piętrach 0–15: 8, 52, 373, 1828, 4724, 11199, 23397, 118595, 28193, 35280, 28225, 24652, 25550, 18649, 14131, 5739. SHA-256 `45031779…2304`.

- Źródłowa mapa i `forgotten-house.xml` mają identyczne hashe przed i po (runner). Dwa niezależne przebiegi dały bajtowo identyczny `reference-report.json` (SHA-256 `0D538B97…77A0`).
- Czas całego runnera (kopiowanie profilu, start EXE, wczytanie klienta, import, hashowanie): 4,4–4,5 s wall dla `forgotten.otbm` w trzech przebiegach po poprawce (jeden pomiar to nie benchmark; bez rozbicia). Samo `FastOTBM` wczytanie kafli: ok. 51 ms (log loadera). Pamięci nie mierzono.

## Czego NIE zweryfikowano
- Do importu skopiowano tylko `.otbm`, więc companion XML (house/spawn/zones) nie były wczytywane; ich wpływu nie sprawdzono. Katalog `forgotten-zones` nie jest hashowany przez runner (tylko pliki `*-*.xml`).
- Itemy wewnątrz kontenerów nie są liczone ani sprawdzane pod kątem `unknown`.
- Reguła zgodności („starszy minor OK, nowszy zgłaszany”) to założenie projektu, sprawdzone tylko dla przypadku równych wersji i testów jednostkowych; brak fixture z niezgodną wersją na prawdziwym pliku.
- Anulowanie i limit pamięci: jest tylko limit rozmiaru pliku (64 MiB w próbniku); `loadMap` nie jest przerywalny, szczyt pamięci niezmierzony. Do zamknięcia przed G3-E.
- Test GUI nie dotyczy tego kroku (brak zmian w interfejsie).

## Korekta: blokujący dialog wxWidgets (zgłoszenie użytkownika, 2026-11-07)
Pierwsze trzy przebiegi (syntetyczny, `forgotten` ×2) **nie były bez interakcji**: prywatny EXE pokazywał modalne okno „wxWidgets Warning: This application doesn't use a correct manifest… Common Controls Library v6” i przebieg szedł dalej dopiero po ręcznym kliknięciu OK. Przyczyna: `g0_build_rme_snapshot.py` linkuje próbnik bez manifestu `rme.exe`. Ten sam brak manifestu dawał 19× `SendMessage(BCM_SETIMAGELIST) failed` na stderr. Czasu 9,3 s z pierwszej wersji nie należy używać (zawierał oczekiwanie na kliknięcie); logi nie miały przerwy po starcie, bo okno pojawiało się przed zalogowaniem pierwszej linii.

Poprawka: runner wbudowuje manifest z `build-ninja/rme.exe` w kopię próbnika (`mt.exe` z Windows SDK; brak SDK = błąd, nie ciche pominięcie), a próbnik przekierowuje logi wx do `wx-log.txt`, żeby żaden komunikat nie otworzył okna. Po poprawce cztery przebiegi (syntetyczny, `forgotten` ×3) kończą się same, stderr i `wx-log.txt` mają 0 bajtów, raport `forgotten` ma ten sam SHA-256 co przed poprawką. Pierwsze trzy przebiegi traktuj jako unieważnione dowody; ważne są przebiegi `…-d`, `…-e`, `…-f`. Nie sprawdzono, czy dialog może wrócić przy innej konfiguracji GUI.
