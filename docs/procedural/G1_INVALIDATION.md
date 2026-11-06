# G1 — zmiana wejścia i zamknięcie aktywnego Preview

2026-11-04. **PARTIAL_G1_GUI_PREVIEW_INVALIDATION_FOCUS_WARNING**. Oba scenariusze i ponowienia przeszły funkcjonalnie. RME zakończył pracę z kodem 0. Stderr zawierał pojedyncze ostrzeżenie wxWidgets `SetFocus` z kodem `0x00000057`. Przebieg nie otwierał pliku mapy ani komunikatu importu kreatur, więc do odtworzenia ostrzeżenia nie są konieczne.

Prywatny obserwator RME sprawdza dwie ścieżki usunięcia zadania Preview 500×500. Rozpoznaje natywne zdarzenia edycji seeda i zamknięcia okna przed ich obsługą. Wymaga wtedy stanu „Preparing preview”. Polecenia wprowadza operator. Po obu operacjach obserwator sprawdza przez dwie sekundy brak ponownej publikacji wyniku.

Mapa syntetyczna zawiera jeden kafel kontrolny poza obszarem generacji. Przez całą próbę obserwator porównuje jego stan, tożsamość edytora, rewizję mapy, flagę dirty i pustą historię. Ponowienie w tym samym oknie ma pokazać seed 1728. Po zamknięciu drugiego dialogu w trakcie pracy nowy dialog ma pokazać domyślne 100×100 z seedem 1729.

Pierwsza próba wejścia w tym procesie zmieniła seed bez potwierdzenia stanu „Preparing preview”, więc nie została zaliczona. Szybsze powtórzenie zarejestrowało `INVALIDATION_EDIT_IN_FLIGHT`, a następnie przez dwie sekundy Validate i Place pozostawały wyłączone. Ponowienie pokazało 250 000 zmian dla seeda 1728. Kolejna próba zarejestrowała `INVALIDATION_CLOSE_IN_FLIGHT` podczas Alt+F4, po którym dialog się zamknął. Po dwóch sekundach nowy dialog pokazał własny podgląd 10 000 kafli dla seeda 1729.

![RME po edycji seeda podczas przygotowania Preview 500×500](screenshots/g1-invalidation-edit-20261104.jpg)

`build_ninja.bat` przeszedł, bez zmian binarium produkcyjnego. Verifier przebiegu `g1-invalidation-rme-20261104` potwierdził pełną kolejność markerów, 421 obiektów, 421 wejść kompilacji i niezmienione wejściowe assets. Wynik obejmuje edycję seeda i zamknięcie Alt+F4 na jednej mapie. Inne przypadki unieważnienia celu, gęste mapy i testy wycieków pozostają poza zakresem. Pełne **G1 IN PROGRESS**.

## Odtworzenie

```text
rtk proxy <PY> docs/procedural/smoke/g1_run_dialog_probe.py local-data/procedural-smoke/NEW_RUN local-data/procedural-smoke/g0-reference-profile-20261030/data local-data/procedural-smoke/g0-reference-profile-20261030/config.toml --invalidation
rtk proxy <PY> docs/procedural/smoke/g1_verify_dialog_probe.py local-data/procedural-smoke/NEW_RUN --invalidation
```

`<PY>` to `C:/Users/Sorky/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe -X utf8`. Po `INVALIDATION_FIXTURE_READY` otwórz Map → Generate terrain, ustaw 500×500, uruchom Preview i szybko zmień seed z 1729 na 1728. Po `INVALIDATION_EDIT_PASS` uruchom Preview ponownie. Po `INVALIDATION_EDIT_RETRY_PASS` zamknij dialog i otwórz go ponownie. Ustaw 500×500, uruchom Preview i zamknij okno w trakcie pracy przez Alt+F4. Po `INVALIDATION_CLOSE_PASS` otwórz nowy dialog i wykonaj domyślne Preview. Zamknij dialog, a po `INVALIDATION_PASS` zamknij prywatną aplikację.
