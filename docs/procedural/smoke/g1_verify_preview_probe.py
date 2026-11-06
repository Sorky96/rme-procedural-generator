"""Verify current owned-preview evidence; this never grants the whole G1 gate."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from collections import Counter

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--selection', action='store_true', help='Verify preview isolation with two real editors and live selection/Lua regression')
parser.add_argument('--preparation', action='store_true', help='Verify joint preparation budget, tracked new peak and phase faults; includes selection checks')
parser.add_argument('--exact-selection', action='store_true', help='Verify exact live Selection masks and revalidation; includes preparation checks')
parser.add_argument('--validation', action='store_true', help='Verify read-only prepared validation; includes exact selection and preparation checks')
parser.add_argument('--assets', action='store_true', help='Verify independent generation catalog mutations; includes all preceding checks')
parser.add_argument('--target', action='store_true', help='Verify current offline editor target and live selection/closure checks; includes all preceding checks')
parser.add_argument('--commit', action='store_true', help='Verify atomic concrete terrain history core; includes all preceding checks, not GUI E2E')
parser.add_argument('run', type=lambda value: Path(value).resolve())
args = parser.parse_args()
if args.commit:
    args.target = True
if args.target:
    args.assets = True
if args.assets:
    args.validation = True
if args.validation:
    args.exact_selection = True
if args.exact_selection:
    args.preparation = True
if args.preparation:
    args.selection = True
repo = Path(__file__).resolve().parents[3]
base = repo / 'local-data/procedural-smoke'
assert args.run.is_relative_to(base)
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
verification = json.loads((args.run / 'verification.json').read_text())
assert verification['passed'] and verification['exit_code'] == 0
assert verification['input_assets_unchanged'] and verification['stderr_empty']
assert (args.run / 'stderr.log').stat().st_size == 0
report = (args.run / 'run/resolver-probe.log').read_text()
markers = ['PREVIEW_PASS', 'PREVIEW_EXCLUDED_PASS', 'PREVIEW_BASE_GROUND_PASS', 'PREVIEW_MASK_EPOCH_PASS']
markers += (['SELECTION_OBSERVERS_PASS', 'SELECTION_DETACHED_NOTIFICATIONS_PASS',
             'PREVIEW_SELECTED_CONTEXT_ISOLATION_PASS', 'PREVIEW_SELECTED_BORDER_REFUSAL_PASS',
             'SELECTION_LIVE_LUA_UNDO_REDO_PASS'] if args.selection else ['PREVIEW_SELECTED_CONTEXT_PASS'])
if args.preparation:
    markers += ['PREPARATION_SIZE_PASS', 'PREPARATION_FAILURES_PASS']
if args.exact_selection:
    markers += ['EXACT_SELECTION_SIZE_PASS', 'EXACT_SELECTION_FAILURES_PASS']
if args.validation:
    markers += ['PREPARED_VALIDATION_SIZE_PASS','PREPARED_VALIDATION_FAILURES_PASS','PREPARED_VALIDATION_EPOCH_PASS']
if args.assets:
    markers += ['GENERATION_ASSET_MUTATIONS_PASS']
if args.target:
    markers += ['TERRAIN_TARGET_SIZE_PASS','TERRAIN_TARGET_LIFECYCLE_PASS']
if args.commit:
    markers += ['TERRAIN_RESERVATION_FAILURES_PASS','TERRAIN_COMMIT_FAILURES_PASS','TERRAIN_COMMIT_SIZE_PASS','TERRAIN_SELECTED_COMMIT_PASS','TERRAIN_SELECTION_HISTORY_PASS']
for marker in markers:
    assert any(line == marker or line.startswith(marker + '\t') for line in report.splitlines()), marker
assert 'FAILED\t' not in report
rows = [line.split('\t') for line in report.splitlines() if line.startswith('PREVIEW_SIZE_PASS\t')]
assert {(int(row[1]), int(row[2])) for row in rows} == {(100,0),(100,1),(500,0),(500,1)}
assert len(rows) == 4 and all(int(row[3]) > 0 for row in rows if row[2] == '1')
faults = int(re.search(r'PREVIEW_FAILURES_PASS\t(\d+)', report)[1])
assert faults >= 79
if args.selection:
    assert 'SELECTION_OBSERVERS_PASS\t2\n' in report
    assert 'PREVIEW_SELECTED_CONTEXT_ISOLATION_PASS\t2\n' in report
preparation_rows = []
preparation_faults = None
if args.preparation:
    preparation_rows = [line.split('\t') for line in report.splitlines() if line.startswith('PREPARATION_SIZE_PASS\t')]
    assert len(preparation_rows) == 2 and {int(row[1]) for row in preparation_rows} == {100,500}
    for row in preparation_rows:
        assert 0 < int(row[3]) <= int(row[2]) <= 512*1024*1024
        assert 0 <= int(row[4]) <= 64*1024
        corresponding = next(value for value in rows if value[1] == row[1] and value[2] == '1')
        assert row[5] == corresponding[6]
    preparation_faults = int(re.search(r'PREPARATION_FAILURES_PASS\t(\d+)',report)[1])
    assert preparation_faults > faults
exact_selection_rows = []
if args.exact_selection:
    exact_selection_rows = [line.split('\t') for line in report.splitlines() if line.startswith('EXACT_SELECTION_SIZE_PASS\t')]
    assert len(exact_selection_rows) == 2 and {int(row[1]) for row in exact_selection_rows} == {100,500}
    for row in exact_selection_rows:
        side, members, memory, fingerprint = map(int,row[1:])
        assert members == side*side-1 and side*side < memory < side*side+1024 and fingerprint > 0
    assert 'EXACT_SELECTION_FAILURES_PASS\t1\n' in report
validation_rows = []
validation_cases = None
if args.validation:
    validation_rows = [line.split('\t') for line in report.splitlines() if line.startswith('PREPARED_VALIDATION_SIZE_PASS\t')]
    assert len(validation_rows) == 2 and {int(row[1]) for row in validation_rows} == {100,500}
    for row in validation_rows:
        assert row[2] == '0'
        corresponding = next(value for value in preparation_rows if value[1] == row[1])
        assert row[3] == corresponding[5]
    validation_cases = int(re.search(r'PREPARED_VALIDATION_FAILURES_PASS\t(\d+)',report)[1])
    assert validation_cases >= 25
asset_mutation_row = None
if args.assets:
    asset_mutation_row = next(line.split('\t') for line in report.splitlines() if line.startswith('GENERATION_ASSET_MUTATIONS_PASS\t'))
    assert int(asset_mutation_row[1]) >= 35 and asset_mutation_row[2] == '0' and float(asset_mutation_row[3]) >= 0
target_rows = []
target_lifecycle = None
if args.target:
    target_rows = [line.split('\t') for line in report.splitlines() if line.startswith('TERRAIN_TARGET_SIZE_PASS\t')]
    assert len(target_rows) == 2 and {int(row[1]) for row in target_rows} == {100,500}
    assert all(row[2] == '0' and int(row[3]) > 0 for row in target_rows)
    target_lifecycle = next(line.split('\t') for line in report.splitlines() if line.startswith('TERRAIN_TARGET_LIFECYCLE_PASS\t'))
    assert int(target_lifecycle[1]) >= 20 and target_lifecycle[2:] == ['2','0']
manifest = json.loads((args.run / 'build-manifest.json').read_text())
assert manifest['entry_override'] == 'normal application entry'
assert sha(Path(manifest['probe_source'])) == manifest['probe_source_sha256']
for name, value in manifest['sources'].items():
    path = (repo / name).resolve()
    assert path.is_relative_to(repo) and path.suffix in ('.cpp', '.c', '.cxx', '.cc', '.rc')
    assert sha(path) == value, name
for name, value in manifest['objects'].items():
    path = (repo / 'build-ninja' / name if '/' in name or '\\' in name else args.run / name).resolve()
    assert path.is_relative_to(repo / 'build-ninja') or path.is_relative_to(args.run)
    assert sha(path) == value, name
assert sha(repo / 'build-ninja/rme.exe') == manifest['normal_exe_sha256']
assert sha(args.run / 'snapshot_probe.exe') == manifest['exe_sha256']
assert 'Z_VCPKG_ROOT_DIR:INTERNAL=C:/repo/vcpkg' in (repo / 'build-ninja/CMakeCache.txt').read_text()
assert 'BUILD SUCCESSFUL' in (base / ('g1-commit-complete-canonical-20261103.log' if args.commit else
                                    'g1-target-canonical-20261103.log' if args.target else
                                    'g1-assets-canonical-20261103.log' if args.assets else
                                    'g1-validation-canonical-20261102.log' if args.validation else
                                    'g1-exact-selection-canonical-20261102.log' if args.exact_selection else
                                    'g1-preparation-canonical-20261102.log' if args.preparation else
                                    'g1-selection-canonical-20261102.log' if args.selection else
                                    'g1-preview-reviewed-canonical-20261102.log')).read_text()
old = json.loads((base / 'g0-draw-lua-clean-20261102/manifest.json').read_text())
changed = [name for name, value in old['sources'].items() if sha(repo / name) != value]
allowed = {'source/brushes/ground/ground_brush.cpp', 'source/brushes/ground/ground_brush.h',
           'source/app/managers/version_manager.cpp', 'source/app/managers/version_manager.h',
           'source/game/item_attributes.h', 'source/game/creature.h',
           'source/brushes/ground/ground_border_calculator.cpp', 'source/brushes/ground/ground_border_calculator.h',
           'source/editor/operations/draw_operations.h'}
if args.selection:
    allowed.add('source/map/tile_operations.cpp')
if args.commit:
    allowed.update({'source/editor/action.cpp','source/editor/action.h',
                    'source/editor/action_queue.cpp','source/editor/action_queue.h',
                    'source/editor/selection.cpp','source/editor/selection.h',
                    'source/lua/lua_api.h','source/lua/lua_api_app.cpp',
                    'source/ui/managers/minimap_manager.h','source/ui/managers/minimap_manager.cpp',
                    'source/map/basemap.h','source/map/basemap.cpp'})
assert set(changed) == allowed, changed
bridge_root = Path('C:/Users/Sorky/.codex/claude-bridge/runs')
bridge_ids = ['91c89455-6c4e-4bcf-88b2-8472a6c83f7f', '4a58a62d-c51c-48d2-b85f-056a069dcc3f']
if args.selection:
    bridge_ids.append('1cc3531a-550e-430b-b56a-29ba93fe2d44')
if args.preparation:
    bridge_ids += ['231dc92b-7e46-4d8c-844b-725c2d5fd266','817abf8b-36b6-46f5-8d75-a9c290090c11']
if args.exact_selection:
    bridge_ids += ['b57245cc-6c6f-4cd5-9ce7-67bff4ec9fb1','fdf3a1aa-14bf-407c-a0b0-967a19c46e31']
if args.validation:
    bridge_ids.append('b66c62c4-7051-40bd-a31e-d9d6509495d8')
if args.assets:
    bridge_ids.append('14ea4969-9641-4259-add4-6a0b0e291666')
if args.target:
    bridge_ids += ['1ab3e529-7bf2-4610-81b3-f3031eec2bb4','ead98698-19d3-4c0e-929b-b2fe4ce034dc']
if args.commit:
    bridge_ids += ['f9c68d5b-ef2d-4fed-b323-3bf49f91ca77','f1a70830-50b7-4919-b699-fc2e39353beb',
                   '46601207-d478-4391-aaa3-1a175876b5ee','1c06e930-0bca-4366-a11c-c2ebfd40000b','f9d6ea26-ebe7-4a57-8243-ce26b04b041e']
for run_id in bridge_ids:
    bridge = bridge_root / run_id
    worker = json.loads((bridge / 'report.json').read_text())
    stdout = json.loads((bridge / 'stdout.json').read_text())
    assert worker['status'] == 'completed' and worker['exit_code'] == 0
    assert not worker['result']['permission_denials']
    assert worker['result']['result'] == stdout['result']
    assert (bridge / 'stderr.log').stat().st_size == 0
    if worker['mode'] == 'read':
        assert worker['git_before'] == worker['git_after']
if args.preparation:
    assert (bridge_root / '817abf8b-36b6-46f5-8d75-a9c290090c11' / 'changes.diff').read_bytes() == (bridge_root / '231dc92b-7e46-4d8c-844b-725c2d5fd266' / 'changes.diff').read_bytes()
    integration = json.loads((base / 'g1-preparation-integration-20261102.json').read_text())
    assert all(sha(repo / name) == value for name, value in integration.items())
if args.exact_selection:
    assert (bridge_root / 'fdf3a1aa-14bf-407c-a0b0-967a19c46e31' / 'changes.diff').read_bytes() == (bridge_root / 'b57245cc-6c6f-4cd5-9ce7-67bff4ec9fb1' / 'changes.diff').read_bytes()
    integration = json.loads((base / 'g1-exact-selection-integration-20261102.json').read_text())
    assert all(sha(repo / name) == value for name, value in integration.items())
if args.validation:
    assert (bridge_root / 'b66c62c4-7051-40bd-a31e-d9d6509495d8' / 'changes.diff').read_bytes() == (bridge_root / 'fdf3a1aa-14bf-407c-a0b0-967a19c46e31' / 'changes.diff').read_bytes()
    integration = json.loads((base / 'g1-validation-integration-20261102.json').read_text())
    if args.assets:
        # The coordinator has now extended these two files. Preserve proof of
        # the previous byte-for-byte integration in its frozen verified run.
        previous = json.loads((base / 'g1-validation-rme-20261102/current-verification.json').read_text())
        assert all(previous['sources'][name] == value for name, value in integration.items())
    else:
        assert all(sha(repo / name) == value for name, value in integration.items())
if args.assets:
    assert (bridge_root / '14ea4969-9641-4259-add4-6a0b0e291666' / 'changes.diff').read_bytes() == (bridge_root / 'b66c62c4-7051-40bd-a31e-d9d6509495d8' / 'changes.diff').read_bytes()
    integration = json.loads((base / 'g1-assets-integration-20261103.json').read_text())
    assert all(sha(repo / name) == value for name, value in integration.items())
if args.target:
    assert (bridge_root / 'ead98698-19d3-4c0e-929b-b2fe4ce034dc' / 'changes.diff').read_bytes() == (bridge_root / '1ab3e529-7bf2-4610-81b3-f3031eec2bb4' / 'changes.diff').read_bytes()
    assert (bridge_root / '1ab3e529-7bf2-4610-81b3-f3031eec2bb4' / 'changes.diff').read_bytes() == (bridge_root / '14ea4969-9641-4259-add4-6a0b0e291666' / 'changes.diff').read_bytes()
    integration = json.loads((base / 'g1-target-integration-20261103.json').read_text())
    assert all(sha(repo / name) == value for name, value in integration.items())
supplemental = {}
for name in (('g1-assets-border-order-rme-20261103','g1-assets-snapshot-rme-20261103') if args.assets else
             ('g1-border-order-current-rme-20261102', 'g1-preview-snapshot-regression-20261102')):
    result = json.loads((base / name / 'verification.json').read_text())
    assert result['passed'] and result['exit_code'] == 0
    assert (base / name / 'stderr.log').stat().st_size == 0
    if args.commit:
        frozen = json.loads((base / 'g1-target-rme-20261103/current-verification.json').read_text())
        assert name in frozen['supplemental']
        focused = json.loads((base / name / 'build-manifest.json').read_text())
        assert sha(base / name / 'snapshot_probe.exe') == focused['exe_sha256']
        supplemental[name] = 'Historical accepted PASS before atomic commit; not a current executable/object claim. Current cumulative probe repeats border fingerprints and snapshot faults.'
        continue
    # Historical full executable; all sources relevant to these specific probes
    # remain current. Do not assert the entire older linked program is current.
    assert all(sha(repo / path) == value for path, value in result['sources'].items())
    if args.assets:
        focused = json.loads((base / name / 'build-manifest.json').read_text())
        assert sha(base / name / 'snapshot_probe.exe') == focused['exe_sha256']
        if not args.target:
            assert sha(repo / 'build-ninja/rme.exe') == focused['normal_exe_sha256']
        assert all(sha(repo / path) == value for path, value in focused['sources'].items())
        for path, value in focused['objects'].items():
            current = repo / 'build-ninja' / path if '/' in path or '\\' in path else base / name / path
            assert sha(current) == value
        supplemental[name] = ('PASS; relevant sources/objects unchanged; historical full executable before target adapter' if args.target else
                              'PASS; freshly linked current RME objects and headers; no GUI/client startup')
    else:
        supplemental[name] = 'PASS; relevant sources unchanged; historical linked executable'
paths = [repo / 'source/procedural/adapter/terrain_preview.h',
         repo / 'source/editor/operations/procedural_draw_operations.cpp',
         repo / 'source/procedural/CMakeLists.txt', Path(manifest['probe_source']),
         Path(__file__), Path(__file__).with_name('g1_build_input_probe.py'),
         Path(__file__).with_name('g1_run_resolver_probe.py')]
if args.selection:
    paths.append(repo / 'source/map/tile_operations.cpp')
if args.preparation:
    paths += [repo / 'source/procedural/adapter/terrain_preparation.h',repo / 'source/procedural/adapter/terrain_preparation.cpp',
              repo / 'source/app/managers/version_manager.h']
if args.exact_selection:
    paths += [repo / 'source/procedural/adapter/generation_selection.h',repo / 'source/procedural/adapter/generation_selection.cpp']
if args.validation:
    paths += [repo / 'source/procedural/adapter/terrain_validation.h',repo / 'source/procedural/adapter/terrain_validation.cpp']
if args.assets:
    paths += [repo / 'source/procedural/adapter/generation_assets.h',repo / 'source/procedural/adapter/generation_assets.cpp',
              repo / 'source/brushes/ground/ground_brush.h']
if args.target:
    paths += [repo / 'source/procedural/adapter/terrain_target.h',repo / 'source/procedural/adapter/terrain_target.cpp']
if args.commit:
    paths += [repo / name for name in sorted(allowed) if (repo / name) not in paths]
    paths += [repo / 'source/procedural/adapter/terrain_commit.h',
              repo / 'source/editor/operations/procedural_commit_operations.cpp']
for path in paths:
    source = path.read_text(encoding='utf-8')
    before = base / 'g1-commit-before-20261103' / path.relative_to(repo)
    bad = lambda text: Counter(line for line in text.splitlines() if '\ufffd' in line or line.rstrip() != line)
    if args.commit and before.is_file():
        # Preserve unrelated pre-existing formatting in shared dirty G0 sources.
        assert not bad(source) - bad(before.read_text(encoding='utf-8')), str(path)
    else:
        assert not bad(source), str(path)
result = {'result': ('PASS_G1_ATOMIC_TERRAIN_HISTORY_CORE_ONLY' if args.commit else
                     'PASS_G1_CURRENT_OFFLINE_TARGET_ONLY' if args.target else
                     'PASS_G1_GENERATION_ASSET_CATALOG_ONLY' if args.assets else
                     'PASS_G1_PREPARED_VALIDATION_ONLY' if args.validation else
                     'PASS_G1_EXACT_SELECTION_MASK_ONLY' if args.exact_selection else
                     'PASS_G1_JOINT_PREPARATION_BUDGET_ONLY' if args.preparation else
                     'PASS_G1_PREVIEW_SELECTION_ISOLATION_ONLY' if args.selection else
                     'PASS_G1_OWNED_BORDERED_PREVIEW_ONLY'), 'g1_gate': 'IN PROGRESS',
          'normal_exe_sha256': manifest['normal_exe_sha256'], 'probe_exe_sha256': manifest['exe_sha256'],
          'rme_objects_checked': len(manifest['objects']), 'baseline_sources_compared': len(old['sources']),
          'baseline_differences': changed, 'preview_allocation_fault_cases': faults,
          'size_rows': rows, 'supplemental': supplemental,
          'preparation_rows': preparation_rows, 'preparation_allocation_fault_cases': preparation_faults,
          'exact_selection_rows': exact_selection_rows,
          'validation_rows': validation_rows, 'validation_refusal_cases': validation_cases,
          'asset_mutation_row': asset_mutation_row,
          'target_rows': target_rows, 'target_lifecycle': target_lifecycle,
          'sources': {path.relative_to(repo).as_posix(): sha(path) for path in paths},
          'not_verified': (['atomic commit/Undo/apply reentrancy and active Lua/history transaction exclusion',
                            'GUI commit wiring and selection publication for empty tiles',
                            'raw image/pixel/GPU/atlas/animation-runtime edits and non-ground brush internals',
                            'general corruption of catalog ownership or GUI notebook registration',
                            'Lua selectionChange emission for nonempty selection action sessions'] if args.target else
                           ['raw image/pixel/GPU/atlas/animation-runtime edits and non-ground brush internals',
                            'general corruption of catalog container ownership or internal store tables',
                            'GUI/commit integration and final map/editor lifetime guard',
                            'selection membership publication for empty tiles at commit/Undo',
                            'Lua selectionChange emission for nonempty selection action sessions'] if args.assets else
                           ['independent brush/item/material edits without asset epoch advancement',
                            'GUI/commit integration and final map/editor lifetime guard',
                            'selection membership publication for empty tiles at commit/Undo',
                            'Lua selectionChange emission for nonempty selection action sessions'] if args.validation else
                           ['GUI dialog integration of exact mask/commit selection membership for empty tiles',
                            'Lua selectionChange emission for nonempty selection action sessions'] if args.exact_selection else
                           ['GUI exact-selection mask capture/commit selection membership for empty tiles',
                            'Lua selectionChange emission for nonempty selection action sessions'] if args.selection else
                           ['selected-context support/current-editor selection cache observation']) + [
                           ('RSS/whole-app/source/assets/simultaneous jobs/future Undo memory' if args.preparation else
                            'full-job memory/RSS budget'), 'GUI Cancel and responsive scheduling',
                           'stale relevant assets/mask/selection before commit',
                           'atomic commit/faults/Undo/Redo/separate manual drawing history',
                           'minimal dialog/rendered preview and G1 GUI/OTBM round-trip',
                           'cross-platform equal-z border order']}
if args.commit:
    commit_rows = [line.split('\t') for line in report.splitlines() if line.startswith('TERRAIN_COMMIT_SIZE_PASS\t')]
    assert len(commit_rows) == 2 and {int(row[1]) for row in commit_rows} == {100,500}
    assert all(int(row[2]) >= int(row[3]) > 0 and int(row[4]) > 0 for row in commit_rows)
    commit_faults = next(line.split('\t') for line in report.splitlines() if line.startswith('TERRAIN_COMMIT_FAILURES_PASS\t'))
    assert int(commit_faults[1]) >= 100 and int(commit_faults[2]) >= int(commit_faults[1])
    reservation_trials = int(re.search(r'TERRAIN_RESERVATION_FAILURES_PASS\t(\d+)',report)[1])
    assert reservation_trials >= 14
    assert 'TERRAIN_SELECTED_COMMIT_PASS\t5\t2\t1\n' in report
    assert 'TERRAIN_SELECTION_HISTORY_PASS\t2\t1\t1\n' in report
    integration = json.loads((base / 'g1-commit-reservation-integration-20261103.json').read_text())
    assert all(sha(repo / name) == value for name,value in integration.items())
    result.update(commit_rows=commit_rows,commit_allocation_faults=int(commit_faults[1]),reservation_trials=reservation_trials)
    result['not_verified'] = ['GUI Commit/Cancel wiring, rendered preview, responsive scheduling and GUI/OTBM round-trip',
                             'persistent/system-wide OOM, aligned/C/DLL/GPU allocations and RSS',
                             'arbitrary concurrent/out-of-band writes, corrupt asset ownership and non-ground brush internals',
                             'notification delivery under persistent OOM; post-publication observer failures do not undo accepted history',
                             'cross-platform equal-z border order']
    result['allocation_fault_scope'] = 'Ordinary C++ new/new[] through history publication; observer allocations occur after acceptance. Empty grid vector capacity may remain after failed reservation, but cells/nodes/floors/metadata/history/revision return unchanged.'
(args.run / 'current-verification.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
