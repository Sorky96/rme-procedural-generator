# G4: porównanie wizualne generatora z referencjami (2026-11-08)

Narzędzia: `style_render` (generator → PNG rodzajów, jeden piksel na kafel) i sonda `g3_reference_import_probe` (`kind-map-zNN.png` dla map referencyjnych, tylko lokalnie w `local-data/`, nie w repo). Profile v7, wycinki 256×256 z poziomu z=7 (wycinek referencji wybrany automatycznie jako obszar z największą ilością lądu), ten sam kolor na rodzaj. **To jest oględziny jednego wycinka na styl, nie pomiar**; obrazy referencji są prywatne i nie są w repo.

## Obserwacje (M06 i M19 vs profile m06 i m19)

1. **Skala struktur.** Referencje mają duże spójne struktury: kontynenty i wyspy otoczone morzem (M19), rozległe równiny trawy (M06). Generator daje jedną skalę szumu: równomierną mozaikę jezior, skał i ziemi na całej powierzchni (profil m19 szczególnie).
2. **Rzeki.** W M06 rzeki mają szerokość ok. 6–10 kafli, tworzą sieć z dopływami i łączą się, płynnie meandrują. W generatorze rzeki mają 2–3 kafle, zaczynają się i kończą na jeziorach i nie tworzą sieci; meandr 8 jest widoczny, ale skala i szerokość są inne.
3. **Ziemia.** W M06 to prostokątne i okrągłe plamy rozrzucone w trawie (ręcznie malowane). W generatorze ziemia występuje głównie jako pas przy skale i wodzie.
4. **Skała.** W M19 zwarte masywy z postrzępionym brzegiem; w generatorze mniejsze, pocięte plamy.
5. **Brzeg.** W M19 szerokie plaże; w generatorze zawsze cienki pas (ok. 1–2 kafle).
6. **Człowiek.** Referencje mają budynki, drogi, nietypowe grunty (kolory „Other”/„Unmapped”); generator ich nie robi (poza zakresem fazy).

## Wniosek

Dopasowanie udziałów rodzajów (obecna miara G4) nie daje podobieństwa kształtu. Brakuje przede wszystkim: wieloskalowej struktury (duże masy lądu i wody), szerokich rzek z siecią dopływów, plam ziemi w trawie i szerokości brzegów. Nie sprawdzono podobieństwa wyglądu metryką; ocena jest wizualna i subiektywna.
