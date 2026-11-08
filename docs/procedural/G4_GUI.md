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

## Czego NIE sprawdzono
- Samego okna (układ, kolejność tabulacji, mnemoniki, tooltip, zachowanie przy braku folderu), pełnej ścieżki Preview → Validate → Place → Undo/Redo z profilem, ani oferty dla klienta BlackTek (pokryta tylko testem jednostkowym).
- Wyglądu wyniku (rzeki wciąż prowadzą głównie po osiach, D6; profile sięgają krańców siatek parametrów).
- Że profil poprawia wynik na mapach niewidzianych (nie: patrz [G4_GATE_STATUS.md](G4_GATE_STATUS.md); to są profile dopasowane do własnych stylów).
- Foldery i profile nie są wersjonowane w repo (pochodzą z prywatnych map); instrukcja ich uzyskania to uruchomienie narzędzi z `docs/procedural/smoke` i `style_styles`.
