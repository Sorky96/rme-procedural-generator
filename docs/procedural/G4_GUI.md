# G4 — profile stylu w oknie generatora, 2026-11-08

**PARTIAL: kod zbudowany i sprawdzony bez GUI, test ręczny w RME czeka na użytkownika.**

## Co jest
- Okno **Map → Generate terrain…** ma nową listę **Style profile** (pod „Terrain style”). Pozycja „Built-in (manual settings)” to dotychczasowe zachowanie. Lista jest aktywna tylko dla stylu „Natural surface”.
- Profile to pliki `*.profile` (`g4-profile/3`) w folderze `procedural/styles` w lokalnym katalogu danych RME (`FileSystem::GetLocalDataDirectory()`); dokładną ścieżkę pokazuje podpowiedź (tooltip) nad listą. Okno **nic nie zapisuje** i nie tworzy tego folderu (tworzy tylko istniejący już lokalny katalog danych, jak reszta RME).
- Profil jest oferowany tylko jeśli został nauczony dla **dokładnie tego klienta i tych assetów**: nazwa klienta, SHA-256 `items.otb` (z modularnych danych klienta) i SHA-256 metadanych klienta (`Tibia.dat`) muszą się zgadzać z profilem, profil musi dotyczyć powierzchni z fitowanym strojeniem, a wartości muszą mieścić się w zakresach generatora. Niepasujące profile nie są listowane; tooltip podaje, ile ukryto i dlaczego.
- Wybrany profil ustawia `GenerationConfig::surface` (strojenie powierzchni) i `GenerationConfig::decor` (szanse drzew, krzaków i kamieni; ma znaczenie przy zaznaczonej wegetacji). Dostały je istniejące ścieżki: podgląd, walidacja, Place, Undo. Zmiana profilu unieważnia podgląd jak każda inna zmiana ustawień.
- `DecorTuning` przeniesiony do `generation.h`, `GenerationConfig::decor` dodany. Odcisk podglądu (fingerprint) obejmuje strojenie dekoracji **tylko gdy różni się od wbudowanego** (i jest wegetacja), więc odciski dla dotychczasowych konfiguracji są bez zmian.
- Narzędzie `style_styles --profiles-out` zapisuje profile stylów (osiem dla 10.98: `caves`, `m01`, `m05`, `m06`, `m15`, `m19`, `swamp`, `winter`) w katalogu lokalnym `local-data/procedural-smoke/g4-profiles/10.98/` (nie w repo).

## Co sprawdzono
- `rme.exe` buduje się bez błędów.
- Testy rdzenia PASS: `test_procedural_core`, `…_surface` (domyślne strojenie dekoracji bez zmian wyniku), `…_decor`, `…_style_profile`, `…_style_profile_set` (oferta: zgodny klient/assets, odrzucenia: inny klient, inne `items.otb`, inne `Tibia.dat`, podziemia, brak strojenia, strojenie poza zakresem, śmieci, pusty plik), `…_style_fit`, `…_similarity`.
- Te same funkcje, które wywołuje okno (`loadedClientAssetIds`, `scanStyleProfiles`), uruchomione w próbniku z prawdziwie załadowanym klientem 10.98: hashe `items.otb` i `Tibia.dat` zgodne z hashami profili, **8 z 8 profili oferowanych**, 0 ukrytych (`g4-offer-20261108a`).
- Raporty G3 nie zmieniane tą zmianą.

## Paleta brushy w profilu (schemat `g4-profile/4`, po uwadze z testu ręcznego)
Pierwszy test w RME pokazał, że profile różnią się tylko rozkładem rodzajów gruntu, a wyglądem prawie wcale (winter i swamp podobne, caves to jeden grunt). Profil niesie teraz także **paletę**: dla każdego z pięciu rodzajów brush, którego dany styl używał najczęściej na powierzchni. Wybór profilu w oknie wpisuje te brushe w pola brushy (pola nadal można zmienić); brush, którego klient nie ma, jest pominięty i wymieniony w statusie, niczego się nie zgaduje.
- Zasady wyboru, zapisane z góry: brush z kluczem bez prefiksu `item ` (czyli prawdziwy brush, nie ground item bez brusha), co najmniej 200 kafli, co najmniej 25 % kafli tego rodzaju w stylu; inaczej brak wpisu (pole brusha zostaje bez zmian). Dane: kafle na klucz dla z ≤ 7 z map niosących cel stylu.
- Wynik dla 10.98: `m06` (trawa, `jungle dirt`, `mountain top ground`, `sand`, `sea`), `m19` (`grass`, `dirt`, `mountain`, `sand`, `sea`), `m05` (`grass`, `mountain`), `winter` (`snowy mountain`, `sea`), `caves` (`earth`); **`swamp`, `m01`, `m15` bez palety**: ich grunty to prawie wyłącznie ground items bez brusha (D13), więc nie da się ich wyrazić brushami. Różnica widoczna po wyborze profilu jest więc skromna: wyraźna dla zimy (`snowy mountain`), mała dla reszty, żadna dla bagna.
- Zmiana schematu na v4 unieważnia pliki v3 (okno ukryje je jako „unusable”); trzeba skopiować nowe.
- Palety nie zmieniają statystyk, dopasowania ani wyników iteracji G4-F (raport `style_styles` bajtowo identyczny).
- Testy: `test_procedural_style_profile` (wybór palety, remisy, kolejność, próg udziału i kafli, zapis i odczyt) PASS; oferta v4 w próbniku 8 z 8.

## Błąd z testu ręcznego: „Borders would extend outside the allowed area” (2026-11-08)
Po wyborze profilu każdy Preview kończył się tym błędem. Dwie przyczyny:
1. **Brushe z palety, których generator nie umie położyć.** Pełne przygotowanie (to samo, co robi okno) na pustej mapie 10.98 odrzuca (`PreviewError::OutOfMask`) brushe `earth`, `earth (stone border)`, `mountain`, `snowy mountain`, `icy mountain`, `dirt mountain ground` (+ thick border), `frozen mud (thick border)`, `grass (alternate border)`, `sand (thick border)`: ich krawędzie wychodzą poza wygenerowany obszar, a okno zmienia tylko dozwolony obszar. `snowy mountain` jest dodatkowo pustym stubem w danych (bez itemów). Przechodzą m.in. `grass`, `jungle grass`, `dirt`, `jungle dirt`, `mountain top ground`, `rocky ground`, `sand`, `sea`, `clear water`. Dotyczy to też ręcznego wyboru tych brushy w dotychczasowym oknie (lista ich nie odfiltrowuje).
2. **Brushe poprzedniego profilu zostawały w polach** po zmianie profilu (np. `snowy mountain` z `winter` psuło `swamp`).
Poprawki: (a) każdy wybór profilu (także „Built-in”) zaczyna od brushy domyślnych, a dopiero potem wpisuje paletę; wpis stosowany tylko, gdy klient ma brush i ma on itemy; (b) narzędzie dobiera paletę wyłącznie spośród brushy, które przeszły to samo przygotowanie na tym kliencie (lista 21 brushy, `--usable-brushes`, wynik próbnika `--palette-probe`); wynik: dla wszystkich 8 profili pełne przygotowanie przechodzi (`ok`), oferta 8 z 8.
**Skutek uboczny:** po tym filtrze palety prawie nie różnicują stylów. `m06`: `grass`, `jungle dirt`, `mountain top ground`, `sand`, `sea`; `m19`: `grass`, `dirt`, `sand`, `sea`; `m05`: `grass`; `winter`: `sea`; `caves`, `m01`, `m15`, `swamp`: brak. Zima nie dostanie `snowy mountain`, bo generator takiego brusha nie położy. Prawdziwe zróżnicowanie wyglądu wymagałoby albo zmiany zasady „zmieniamy tylko dozwolony obszar” dla krawędzi brushy z zewnętrznymi bordurami (decyzja poza tym etapem), albo nowych rodzajów gruntu.

## Czego NIE sprawdzono
- Samego okna (układ, kolejność tabulacji, mnemoniki, tooltip, zachowanie przy braku folderu), pełnej ścieżki Preview → Validate → Place → Undo/Redo z profilem, ani oferty dla klienta BlackTek (pokryta tylko testem jednostkowym).
- Wyglądu wyniku (rzeki wciąż prowadzą głównie po osiach, D6; profile sięgają krańców siatek parametrów).
- Że profil poprawia wynik na mapach niewidzianych (nie: patrz [G4_GATE_STATUS.md](G4_GATE_STATUS.md); to są profile dopasowane do własnych stylów).
- Foldery i profile nie są wersjonowane w repo (pochodzą z prywatnych map); instrukcja ich uzyskania to uruchomienie narzędzi z `docs/procedural/smoke` i `style_styles`.
