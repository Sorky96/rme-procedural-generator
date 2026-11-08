# G4: porównanie wizualne generatora z referencjami (2026-11-08)

Narzędzia: `style_render` (generator → PNG rodzajów, jeden piksel na kafel) i sonda `g3_reference_import_probe` (`kind-map-zNN.png` dla map referencyjnych, tylko lokalnie w `local-data/`, nie w repo). Profile v7, wycinki 256×256 z poziomu z=7 (wycinek referencji wybrany automatycznie jako obszar z największą ilością lądu), ten sam kolor na rodzaj. **To jest oględziny jednego wycinka na styl, nie pomiar**; obrazy referencji są prywatne i nie są w repo.

## Obserwacje (M06 i M19 vs profile m06 i m19)

1. **Skala struktur.** Referencje mają duże spójne struktury: kontynenty i wyspy otoczone morzem (M19), rozległe równiny trawy (M06). Generator daje jedną skalę szumu: równomierną mozaikę jezior, skał i ziemi na całej powierzchni (profil m19 szczególnie).
2. **Rzeki (poprawione 2026-11-08 po pomiarze).** Wstępny opis „w generatorze 2–3 kafle, referencje 6–10” był błędny. Pomiar `style_render` (profil m06/m19, 6 seedów, 256×256): szerokość W rzek generatora 4,4–6,6 kafla, szkielet rzek 53–613 kafli na wycinek. W referencji M06 ten sam pomiar daje W ≈ 1,2–2,2 kafla i średnio ok. 50 kafli szkieletu na wycinek 256×256 (1829 na całą mapę). Wycinek M06 użyty do oględzin został wybrany jako „najwięcej lądu”, więc trafił na rzadki, wyjątkowo bogaty w rzeki fragment: nie reprezentuje mapy.
   Wniosek z pomiaru: generator ma **więcej** i **szersze** rzeki niż to, co mierzy metryka iteracji 4. Ale ta metryka pomija duże sieci rzeczne połączone z morzem (składowa wody ≥ 20 000 kafli jest klasyfikowana jako morze i pomijana), więc **szerokie rzeki widoczne na wycinku M06 nie weszły do pomiaru**. Metryka iteracji 4 (krętość, odcinki proste, szerokość) opisuje więc tylko wąskie, krótkie strumienie referencji; wnioski D6 z iteracji 4 dotyczą tylko ich.
3. **Ziemia.** W M06 to prostokątne i okrągłe plamy rozrzucone w trawie (ręcznie malowane). W generatorze ziemia występuje głównie jako pas przy skale i wodzie.
4. **Skała.** W M19 zwarte masywy z postrzępionym brzegiem; w generatorze mniejsze, pocięte plamy.
5. **Brzeg.** W M19 szerokie plaże; w generatorze zawsze cienki pas (ok. 1–2 kafle).
6. **Człowiek.** Referencje mają budynki, drogi, nietypowe grunty (kolory „Other”/„Unmapped”); generator ich nie robi (poza zakresem fazy).

## Wniosek

Dopasowanie udziałów rodzajów (obecna miara G4) nie daje podobieństwa kształtu. Brakuje przede wszystkim: wieloskalowej struktury (duże masy lądu i wody), sieci rzecznej połączonej z morzem (rzeki nie kończą się na jeziorach, mają dopływy), plam ziemi w trawie i szerokości brzegów. Nie sprawdzono podobieństwa wyglądu metryką; ocena jest wizualna i subiektywna.
