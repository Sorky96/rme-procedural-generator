# I52 implementation report

Preregistration: `0a25e6c`, G6_METRICS I52. This report does not close I52 or G6.
I51 and I51b remain FAIL. Production selection uses A; B has no GUI path.

## Implementation

The existing terrain dialog offers 1–16 Surface candidates (default 4). Classic
continues to prepare one result. Candidate i starts at candidateSeed(userSeed,i).
The user seed field is preserved; the list reports actual seed, initial seed and
attempt count, including the existing retry/fallback sequence. The list supports
native keyboard navigation and focus, and labels Selected and Recommended in text.

Policy association is explicit provenance: exactly one profile source, alias M06
or M19, weight 1000. No filename, palette or tuning heuristic. Those sources use
the 26 frozen g6-bands-v1 bands and A ordering. Everything else shows no ranking
policy and defaults to the first admissible result. This baseline makes no claim
of improved quality. Profile names alone cannot activate a policy.

Each row is produced by the real loaded-client adapter, including sandbox,
autoborders, source validation and final concrete reachability. Unreachable
places, broken transitions, unrepaired results and removed places/caps/storeys/
dungeons are hardFail and cannot be placed, including forced button dispatch.
Invalid preparation/measurement or resource exhaustion refuses the whole series.
There is no partial Place and no silent reduction of N.

The gallery retains small native minimap thumbnails, scores, seeds, attempt counts
and concrete fingerprints for all five possible floors. It retains at most one
PreparedTerrain sandbox. Selecting a row lazily prepares its *actual* seed again,
without retry or ranking, and checks every floor fingerprint and hardFail status
against the gallery. Place stays disabled until that check succeeds. Place then
copies that owned sandbox through the existing DrawOperations commit; Place and
Redo do not regenerate. This trades selection latency for bounded gallery storage.

One series budget is the existing 20 GiB preparation policy minus the entire
gallery allowance, selection mask and image conversion allowance. Every serial
candidate/retry/rebuild uses the same remaining working budget after the previous
sandbox is destroyed. This is an estimate policy, not an RSS limit: source map,
client assets, rendering, unrelated history and thread runtime remain outside it,
as in the existing adapter contract. Commit retains the existing separate 24 GiB
policy. No N full maps or independent N preparation budgets are retained.

Core Surface generation, mountain geometry, world construction, semantic repair,
cave generation and metric measurement use joined workers with owned plain inputs.
Map/client/brush access, concrete validation, sandbox adaptation and commit remain
GUI-only. Cancel/input change/close destroy and join the continuation. No worker
posts callbacks to a window or holds a live map pointer. A timer checks map identity,
revision, client epoch and selection before continuation or retained-result use;
commit independently validates current request and source again.

No generator algorithm, mountain defaults, item IDs or map serialization changed.
There is no feedback/history implementation (I54).

## Manual GUI procedure (not executed by the implementation agent)

1. Finish all builds. Start the rebuilt RME with a loaded supported client and an
   empty disposable map. Open Generate Terrain, select Natural surface. Check N=4,
   minimum 1 and maximum 16. Check Theme text, focus and layout at normal/high DPI.
2. Set rectangle (100,100,7), size 100×100, seed 1729, retry 0. Generate with N=1.
   The row must show index 0, initial/actual seed 1729, one attempt. Place must stay
   disabled during work. This request does not itself assert the *core default*
   fingerprint: GUI shore smoothing and brushes affect the concrete preview.
3. Repeat with N=4 and N=16. Observe progress and scroll the complete list. With
   no associated policy, the first admissible row is selected and status says no
   ranking. Repeat identical inputs; seeds, validation, recommendation and previews
   must match. The source map and Undo count must remain unchanged.
4. Load the reviewed M06 then M19 profile matching the client assets. Generate.
   Check that status names A/g6-bands-v1 and its failed quality baseline. A profile
   with different/multiple/unknown source aliases must show no ranking instead.
5. Tab into the list and use Up/Down to select an admissible row other than
   Recommended. Selected must move in text; Recommended must remain on its row.
   During loading Place is disabled. Inspect Surface and available other floors.
   Use Alt+O to Place. Exactly the selected preview must appear. One Ctrl+Z restores
   the entire source; Ctrl+Y restores all floors identically. No new random choice.
6. With caves/mountains and repair disabled, find a hardFail row. Select it and
   inspect its status; Place and Alt+O must not write it. Do not accept an override.
   Enable retry 2/8, regenerate, and check actual versus initial seed and attempts;
   the user seed field must keep its original value.
7. Start a large N=16 request. Cancel while work is running, then wait. No result,
   enabled Place or map change may reappear. Repeat with a seed/size/profile change,
   then with closing the window. Reopen: no old series is retained.
8. Generate a completed series, change any parameter and verify immediate
   invalidation. Repeat with a new map/client. Modal map-switch restrictions may
   require closing/reopening; programmatic map-revision invalidation is covered by
   the private probe. Never reuse a result from the prior source.
9. Try an area/profile that exceeds the joint resource estimate. Expect a readable
   refusal for the entire series, with N unchanged and no partial Place. Test empty
   result/all-hardFail state. Escape must cancel; Tab focus must remain visible.
10. Check cave wall default mountain and multi-floor Place/Undo/Redo with the
    existing mountain controls. Do not save disposable maps/profiles into the repo.

## Validation results

Working tree: `C:/Users/Sorky/.t3/worktrees/remeres-map-editor-redux-master/t3-aaa1909a`.
Prepared branch: `t3/procedural-series-generation`; preregistration was its initial HEAD.

- Windows x64 Release build using MSVC 19.51, wxWidgets 3.3.1 and the supplied
  Ninja: PASS. The initial canonical invocation entered manifest dependency setup
  for the empty worktree and was stopped. Subsequent configuration used
  `VCPKG_MANIFEST_INSTALL=OFF` and the main checkout's existing
  `build-ninja/vcpkg_installed`; no dependency installation was requested and no
  manual operation targeted `C:/repo/vcpkg/buildtrees`. The final canonical
  `build_ninja.bat` then reported BUILD SUCCESSFUL using that configured cache.
  The integrator should still run it independently.
- Focused CTest `-R 'test_procedural_|test_g6_|test_candidate_rank_cli'`: 23/23 PASS,
  including the new series test, I51/I51b tests and CLI golden fingerprint
  `17159159863505677800`. An earlier unrestricted CTest also listed six unrelated
  executables that had not been built. An earlier focused run could not start two
  executables because Windows temporarily locked them; both passed on retry and
  the complete focused run then passed.
- `g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1 --output scratchpad/i47`:
  G5_GATE_PASS. All eight profile/dungeon/retry configurations have 32/32 seeds and
  byte-identical repeated reports. These remain G5 checks, not an I51 quality pass.
- Real loaded-client `g1_run_resolver_probe.py --series`: CANDIDATE_DIALOG_PASS,
  N=1/4/16, fallback selection, both explicit policies, independent adapter A
  ordering, selecting a different admissible candidate, unchanged user seed,
  payload equality after Place and Redo, exactly one Undo action, cancel/input/
  close/stale-map safety, and forced hardFail Place refusal. Input assets unchanged;
  7 known hidden-window SetFocus warnings, no other stderr. Evidence is private in
  `scratchpad/i52-series-5/verification.json` and `run/resolver-probe.log`.
  This is automated production control/handler coverage, not manual keyboard or
  visual acceptance. Offline profile copies did not match the loaded-client offer
  filter, so the test creates explicitly named private fixtures with their original
  tuning/provenance and the loaded test client's asset identity. The production
  filter is unchanged. Fixture profiles stay in scratchpad.
- Probe development failures were investigated: one build was invalidated when
  its source changed during linking; the first payload mismatch came from setting
  a value not present in a lazy read-only combobox (the fixture now appends/selects
  it); an initial profile test stopped when its unbound offline profiles were not
  offered. The corrected probe passes; none of these failures were ignored.
- Python syntax checks for all three modified runners: PASS. `git diff --check`:
  PASS. No generator core, mountain, serialization or gate-status edits.

- Full suite probes: `--commit` PREVIEW_PASS, exit 0, assets unchanged, no stderr;
  `--surface` SURFACE_RESOLVER_PASS, exit 0, assets unchanged, 157 known SetFocus
  warnings and no foreign stderr. All nine required Surface markers pass.
  The Surface probe also confirms `LEVEL_RETRY_E2E_PASS initial=1 used=2 attempts=2`
  while the user seed stays 1. Evidence: private
  `scratchpad/suite-{commit,surface}-20261118-101758`.
- Final `g5_run_suite.py` core rebuild/run: 20/20 PASS and SUITE_PASS (zero rebuild
  steps after validation). The final added shared-budget boundary test passes.
- Real-client series evidence: private `scratchpad/i52-series-5`. The reported
  `scope` in that run's JSON was inherited from the old runner; its full marker log
  records the actual production-dialog tests. The runner's scope text is corrected
  for future runs. Offline profile fixtures were explicitly rebound to the private
  loaded client's asset IDs, using the original m06/m19 tuning and source aliases;
  the production offer filter was not weakened. Main profiles were read only.

All raw outputs and temporary build scripts remain under scratchpad. Two final
changes after the application probes were the memory-error wording (removing
obsolete “512 MiB” text) and preserving the pre-existing AllocationFailed error
classification from cave-exit generation in the worker. They do not change the
successful generation/selection/commit path. The final canonical build includes
them; allocation-failure injection was not rerun. A duplicate suite run was stopped;
the completed reports above were used, not partial output from the stopped run.

Reproduction from this worktree, with Ninja on PATH and dependencies configured:

```powershell
rtk proxy cmd /c build_ninja.bat
rtk proxy ctest --test-dir build-ninja -R 'test_procedural_|test_g6_|test_candidate_rank_cli' --output-on-failure
rtk proxy py -I docs/procedural/smoke/g5_run_suite.py --probes procedural-smoke/suite-commit-20261118-090105/run/data procedural-smoke/suite-commit-20261118-090105/run/config.toml --local-data C:/repo/remeres-map-editor-redux-master/local-data --output-root scratchpad
rtk proxy py -I docs/procedural/smoke/g1_run_resolver_probe.py scratchpad/i52-review C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/suite-commit-20261118-090105/run/data C:/repo/remeres-map-editor-redux-master/local-data/procedural-smoke/suite-commit-20261118-090105/run/config.toml --series
rtk proxy py -I docs/procedural/smoke/g5_measure_levels.py --i47 --rock-blocks m06=0 m19=1 --output scratchpad/i47-review
```

Not verified: manual GUI procedure above, physical keyboard focus/contrast at
different DPI, actual process RSS/OS allocation failure at the 20 GiB estimate,
original RME load/save compatibility or cross-compiler determinism. Resource tests
cover subtract-only series accounting/refusal and count boundaries; the real-client
probe uses small synthetic maps rather than an out-of-memory stress run.

The map serialization implementation and generator are unchanged. G6 remains open.
