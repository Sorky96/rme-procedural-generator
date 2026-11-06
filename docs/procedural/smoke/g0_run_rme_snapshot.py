"""Run the separate RME probe on a NEW private fixture directory.

Arguments: OUTPUT_DIR FIXTURE_DIR PROFILE_DATA CONFIG_TOML
OUTPUT_DIR must already contain snapshot_probe.exe from the matching builder.
PROFILE_DATA must contain the single 1098-BlackTek profile used in the G0 test.
The original fixture, normal rme.exe and normal editor data are never written.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser(description=__doc__)
for name in ('output', 'fixture', 'profile_data', 'config'):
    parser.add_argument(name, type=lambda value: Path(value).resolve())
modes = parser.add_mutually_exclusive_group()
modes.add_argument('--house-history', action='store_true',
                    help='Run the separately linked house history module instead of snapshot verification')
modes.add_argument('--item-methods', action='store_true',
                   help='Run Lua rotation/Podium history checks with an explicitly synthetic private XML definition')
modes.add_argument('--house-allocation', action='store_true',
                   help='Run the separately linked, test-only allocator replacement on a new empty Map')
modes.add_argument('--lua-allocation', action='store_true',
                   help='Sweep test-only allocation failures after a real Lua transaction callback')
modes.add_argument('--queue-allocation', action='store_true',
                   help='Sweep publication allocations with nonempty history, Redo, merging and limits')
modes.add_argument('--first-save', action='store_true',
                   help='Run first-save module; its private Error dialog needs dismissal')
modes.add_argument('--save-cancel', action='store_true',
                   help='Cancel actual saving through its wx dialog after private target creation')
modes.add_argument('--render-observe', action='store_true',
                   help='Observe real House GUI transitions and read only its presented GL buffer')
parser.add_argument('--rollback-removal', action='store_true',
                    help='With --first-save, deny delete only in a private new-target directory')
parser.add_argument('--nested-companions', action='store_true',
                    help='With --first-save, test relative/absolute nested XML and rollback paths')
modes.add_argument('--marker-startup', choices=('invalid', 'legacy', 'versioned'),
                   help='Run actual startup recovery on private files; inspect and answer its dialogs')
modes.add_argument('--house-gui', action='store_true',
                   help='Copy canonical RME into a new output, test house Add/Undo/Redo in GUI, then close normally')
parser.add_argument('--house-gui-case', choices=('history', 'close-fixture'), default='history',
                    help='GUI observation protocol: history, or open/close/reopen/close the private fixture')
parser.add_argument('--house-gui-baseline', type=lambda value: Path(value).resolve(),
                    help='Read a previously manifested private canonical GUI executable as a regression control')
parser.add_argument('--verify-existing-startup', action='store_true',
                    help='Verify a finished startup probe after timeout; requires observed-exit.json')
parser.add_argument('--verify-existing-house-gui', action='store_true',
                    help='Verify a finished GUI attempt without relaunching; requires observed-exit.json')
args = parser.parse_args()
if args.house_gui_baseline and not args.house_gui:
    parser.error('--house-gui-baseline requires --house-gui')
if args.rollback_removal and not args.first_save:
    parser.error('--rollback-removal requires --first-save')
if args.nested_companions and (not args.first_save or args.rollback_removal):
    parser.error('--nested-companions requires --first-save and excludes --rollback-removal')
repo = Path(__file__).resolve().parents[3]
executable = args.output / 'snapshot_probe.exe'
if args.house_gui:
    executable = args.output / 'house_gui.exe'
    if not args.verify_existing_house_gui:
        args.output.mkdir(parents=True)  # A GUI attempt must own a new directory too.
        normal = args.house_gui_baseline or repo / 'build-ninja/rme.exe'
        if args.house_gui_baseline:
            if not normal.is_relative_to(repo / 'local-data/procedural-smoke') or normal.name != 'house_gui.exe':
                raise AssertionError('Baseline must be a previously verified private GUI executable')
            previous = json.loads((normal.parent / 'gui-build-manifest.json').read_text())
            if hashlib.sha256(normal.read_bytes()).hexdigest() != previous['canonical_exe_sha256']:
                raise AssertionError('Historical baseline differs from its canonical manifest')
        shutil.copy2(normal, executable)
        (args.output / 'gui-build-manifest.json').write_text(json.dumps({
            'canonical_exe_sha256': hashlib.sha256(normal.read_bytes()).hexdigest(),
            'gui_exe_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
            'historical_baseline': bool(args.house_gui_baseline),
            'editor_tabs_source_sha256': hashlib.sha256((repo / 'source/editor/editor_tabs.cpp').read_bytes()).hexdigest()
        }, indent=2), encoding='utf-8')
# MSVC emits this sidecar for the separate probe; without it wxWidgets opens
# a modal warning before the test module can run.
if not executable.is_file() or (not args.house_gui and not executable.with_suffix('.exe.manifest').is_file()):
    raise RuntimeError('Build output must include snapshot_probe.exe and snapshot_probe.exe.manifest')
if not args.house_gui:
    build_manifest = json.loads((args.output / 'build-manifest.json').read_text(encoding='utf-8'))
    if hashlib.sha256(executable.read_bytes()).hexdigest() != build_manifest['exe_sha256']:
        raise AssertionError('Probe executable differs from its build manifest')
names = ('house-roundtrip.otbm', 'house-roundtrip-house.xml', 'house-roundtrip-spawn.xml')
def hashes(directory):
    return {name: hashlib.sha256((directory / name).read_bytes()).hexdigest() for name in names}

def verify_house_gui(code):
    manifest = json.loads((args.output / 'gui-build-manifest.json').read_text(encoding='utf-8'))
    actual_hash = hashlib.sha256(executable.read_bytes()).hexdigest()
    if actual_hash != manifest['gui_exe_sha256'] or actual_hash != manifest['canonical_exe_sha256']:
        raise AssertionError('GUI executable does not match the recorded canonical build')
    observation = json.loads((run / 'house-ui-observation.json').read_text(encoding='utf-8'))
    if code != 0 or observation.get('executable') != str(executable) or observation.get('house_counts') != [0, 1, 0, 1, 0]:
        raise AssertionError('Actual GUI Add/Undo/Redo states and successful exit were not recorded')
    if observation.get('diagnostic_alerts'):
        raise AssertionError('GUI diagnostics need investigation before this case can pass')
    if (data / 'user/data/.saving.txt').exists() or set(run.glob('*.otbm')) != {run / names[0]}:
        raise AssertionError('GUI test created an unexpected map or recovery marker')
    scope = ('Canonical GUI Add House, Undo, Redo, final Undo and ordinary application close'
             if args.house_gui_case == 'history' else
             'Canonical GUI private fixture open/close/reopen/close, empty house palette after each close')
    if args.house_gui_case == 'close-fixture':
        if observation.get('case') != 'close-fixture' or not all(observation.get(key) is True for key in (
                'closed_requested_tab', 'rebound_remaining_house', 'post_close_edit_noop')):
            raise AssertionError('Expected actual correct-tab close, remaining map rebind and no-map Edit observations')
    (args.output / 'verification.json').write_text(json.dumps({
        'result': 'PASS', 'gui_house_counts': observation['house_counts'], 'normal_exit_code': code,
        'source_hashes_unchanged': original, 'map_save_performed': False,
        'gui_exe_sha256': actual_hash,
        'scope': scope
    }, indent=2), encoding='utf-8')

def verify_startup():
    lines = (run / 'snapshot-probe.log').read_text(encoding='utf-8').splitlines()
    if 'MARKER_STARTUP_PASS\t' + args.marker_startup not in lines or any(line.startswith('FAILED\t') for line in lines):
        raise AssertionError('Startup recovery assertions did not finish')
    observation = json.loads((run / 'startup-ui-observation.json').read_text(encoding='utf-8'))
    captions = {'invalid': {'Incomplete recovery marker'}, 'legacy': {'Legacy recovery marker', 'Editor Crashed'},
                'versioned': {'Editor Crashed'}}[args.marker_startup]
    if observation.get('executable') != str(executable) or not captions.issubset(observation.get('captions', [])):
        raise AssertionError('Actual startup dialogs were not recorded for this private executable')
    retained = args.marker_startup != 'versioned'
    marker = data / 'user/data/.saving.txt'
    if retained and marker.read_bytes() != (run / 'expected-marker.txt').read_bytes():
        raise AssertionError('Startup marker bytes changed')
    if not retained and marker.exists():
        raise AssertionError('Completed marker still exists')
    for target, source in zip(('recovery-target.otbm', 'recovery-target-house.xml', 'recovery-target-spawn.xml'), names):
        expected = b'PARTIAL_TARGET' if args.marker_startup == 'invalid' else (run / source).read_bytes()
        if (run / target).read_bytes() != expected:
            raise AssertionError('Startup target mismatch')
        backup = run / (target + '~')
        if retained and backup.read_bytes() != (run / source).read_bytes():
            raise AssertionError('Retained startup backup changed')
        if not retained and backup.exists():
            raise AssertionError('Completed startup backup still exists')
    (args.output / 'verification.json').write_text(json.dumps({
        'result': 'PASS', 'startup_case': args.marker_startup, 'marker_and_backups_retained': retained,
        'source_hashes_unchanged': original, 'observed_startup_captions': observation['captions'],
        'diagnostic_limitations': observation.get('diagnostic_limitations', [])
    }, indent=2), encoding='utf-8')

original = hashes(args.fixture)
run = args.output / 'run'
if args.verify_existing_startup or args.verify_existing_house_gui:
    if args.verify_existing_startup and not args.marker_startup:
        parser.error('--verify-existing-startup requires --marker-startup')
    if args.verify_existing_house_gui and not args.house_gui:
        parser.error('--verify-existing-house-gui requires --house-gui')
    observed = json.loads((args.output / 'observed-exit.json').read_text(encoding='utf-8'))
    if observed['pid'] != int((args.output / 'probe-pid.txt').read_text()) or observed['exit_code'] != 0:
        raise AssertionError('The existing probe did not have an observed successful exit')
    if original != json.loads((run / 'inputs-original.json').read_text()) or hashes(run) != original:
        raise AssertionError('Source fixture changed after the existing probe')
    data = run / 'data'
    if args.house_gui:
        verify_house_gui(observed['exit_code'])
    else:
        verify_startup()
    print('PASS: existing terminal probe verified without restarting it')
    raise SystemExit(0)
run.mkdir()  # Refuse to reuse a previous run or its files.
data = run / 'data'
data.mkdir()
shutil.copytree(args.profile_data / '1098', data / '1098')
for path in args.profile_data.iterdir():
    if path.is_file():
        shutil.copy2(path, data / path.name)
for name in names:
    shutil.copy2(args.fixture / name, run / name)
config = args.config.read_text(encoding='utf-8')
if 'forgotten.otbm' in config.casefold():
    raise RuntimeError('Test configuration references forbidden forgotten.otbm; use an isolated fixture profile')
# A probe must not inherit a map to reopen from the profile used as a template.
config = re.sub(r'^recent_edited_map_path = .*?$', "recent_edited_map_path = ''", config, flags=re.M)
config = re.sub(r'^recent_files = \[[\s\S]*?^\]', 'recent_files = []', config, flags=re.M)
config = re.sub(r"^data_directory = .*?$", "data_directory = '" + data.as_posix() + "/'", config, flags=re.M)
config = re.sub(r'^use_custom_data_directory = .*?$', 'use_custom_data_directory = 1', config, flags=re.M)
config = re.sub(r'^indirectory_installation = .*?$', 'indirectory_installation = 1', config, flags=re.M)
if args.house_gui or args.item_methods or args.house_allocation or args.house_history or args.lua_allocation or args.queue_allocation or args.render_observe:
    for key in ('only_one_instance', 'welcome_dialog', 'use_updater', 'goto_website_on_boot'):
        config, count = re.subn(r'^' + key + r' = .*?$', key + ' = false', config, flags=re.M)
        if count != 1:
            raise RuntimeError('Expected one private GUI setting: ' + key)
(run / 'config.toml').write_text(config, encoding='utf-8')
(run / '.g0-snapshot-fixture').write_text('isolated synthetic fixture', encoding='utf-8')
(run / 'inputs-original.json').write_text(json.dumps(original, indent=2), encoding='utf-8')
if args.rollback_removal:
    (run / '.g0-rollback-removal').write_text('Private ACL rollback test', encoding='utf-8')
if args.nested_companions:
    (run / '.g0-nested-companions').write_text('Private nested companion paths', encoding='utf-8')
if args.save_cancel:
    (run / '.g0-save-cancel').write_text('Actual dialog Cancel event, private files only', encoding='utf-8')
if args.render_observe:
    (run / '.g0-render-observe').write_text('Observe only private active GUI and exact House GL drawable', encoding='utf-8')
if args.house_allocation:
    (run / '.g0-house-allocation').write_text('Test-only allocation failure on a local empty map', encoding='utf-8')
if args.queue_allocation:
    (run / '.g0-queue-allocation').write_text('Test-only publication allocation sweep', encoding='utf-8')
if args.lua_allocation:
    (run / '.g0-lua-allocation').write_text('Test-only allocation sweep after Lua callback', encoding='utf-8')
    smoke_scripts = {}
    for filename in ('g0_smoke.lua', 'g0_verify_saved.lua'):
        source = Path(__file__).with_name(filename)
        shutil.copy2(source, run / filename)
        smoke_scripts[filename] = hashlib.sha256(source.read_bytes()).hexdigest()
    (run / 'smoke-script-inputs.json').write_text(json.dumps(smoke_scripts, indent=2), encoding='utf-8')
if args.item_methods:
    source_xml = args.profile_data / '1098/items/items.xml'
    private_xml = data / '1098/items/items.xml'
    before_xml = private_xml.read_bytes()
    needle = b'<item id="1443" article="a" name="statue" />'
    replacement = b'<item id="1443" article="a" name="statue"><attribute key="type" value="podium" /></item>'
    if before_xml.count(needle) != 1:
        raise AssertionError('Expected exactly one unmodified 1443 definition in the approved profile')
    private_xml.write_bytes(before_xml.replace(needle, replacement))
    script = repo / 'lua_tests/test_item_methods.lua'
    shutil.copy2(script, run / script.name)
    (run / '.g0-item-methods').write_text('Synthetic Podium type for binding/history tests only', encoding='utf-8')
    method_inputs = {
        'source_xml': str(source_xml), 'source_xml_sha256': hashlib.sha256(before_xml).hexdigest(),
        'private_xml_sha256': hashlib.sha256(private_xml.read_bytes()).hexdigest(),
        'script_sha256': hashlib.sha256(script.read_bytes()).hexdigest()
    }
    (run / 'method-inputs.json').write_text(json.dumps(method_inputs, indent=2), encoding='utf-8')
if args.marker_startup:
    backups = []
    for target, source in zip(('recovery-target.otbm', 'recovery-target-house.xml', 'recovery-target-spawn.xml'), names):
        (run / target).write_bytes(b'PARTIAL_TARGET')
        backup = run / (target + '~')
        shutil.copy2(run / source, backup)
        backups.append(str(backup))
    records = backups + [''] * 4
    legacy = '\n'.join(records) + '\n'
    encoded = 'RME_SAVE_RECOVERY_V1\n7\n' + legacy + 'RME_SAVE_RECOVERY_END\n'
    contents = legacy if args.marker_startup == 'legacy' else encoded
    if args.marker_startup == 'invalid':
        contents = encoded[:-len('RME_SAVE_RECOVERY_END\n')]
    local_data = data / 'user/data'
    local_data.mkdir(parents=True)
    (local_data / '.saving.txt').write_bytes(contents.encode('utf-8'))
    (run / 'expected-marker.txt').write_bytes(contents.encode('utf-8'))
    (run / '.g0-marker-startup').write_text(args.marker_startup, encoding='utf-8')
if not (args.output / 'assets').exists():
    shutil.copytree(repo / 'build-ninja/assets', args.output / 'assets')
environment = dict(os.environ)
environment['PATH'] = str(repo / 'build-ninja') + os.pathsep + environment['PATH']
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
# Interactive GUI/readback tests require the ordinary visible startup path.
# Hidden startup can override the application's first ShowWindow request.
startup.wShowWindow = 1 if args.house_gui or args.render_observe else 0
with (args.output / 'stdout.log').open('w', encoding='utf-8') as stdout, \
        (args.output / 'stderr.log').open('w', encoding='utf-8') as stderr:
    process = subprocess.Popen([str(executable)], cwd=run, env=environment,
                               stdout=stdout, stderr=stderr, startupinfo=startup)
    (args.output / 'probe-pid.txt').write_text(str(process.pid))
    (args.output / 'launch-observation.json').write_text(json.dumps({
        'pid': process.pid, 'executable': str(executable),
        'startf_useshowwindow': True, 'wShowWindow': startup.wShowWindow
    }, indent=2), encoding='utf-8')
    try:
        code = process.wait(timeout=300 if args.house_gui or args.lua_allocation or args.queue_allocation or args.rollback_removal or args.nested_companions or args.render_observe else 180 if args.marker_startup or args.item_methods or args.house_allocation else 45)
    except subprocess.TimeoutExpired:
        raise RuntimeError(f'Probe still running as PID {process.pid}; inspect it before doing anything else')
if args.house_gui or args.item_methods or args.house_allocation or args.lua_allocation or args.house_history or args.queue_allocation or args.first_save or args.save_cancel or args.render_observe:
    (args.output / 'observed-exit.json').write_text(json.dumps({
        'pid': process.pid, 'exit_code': code, 'source': 'subprocess.Popen.wait'
    }, indent=2), encoding='utf-8')
if code != 0:
    raise RuntimeError(f'Probe exited with {code}; inspect snapshot-probe.log')
if hashes(run) != original or hashes(args.fixture) != original:
    raise AssertionError('Source fixture changed')
if args.house_gui:
    verify_house_gui(code)
elif args.render_observe:
    lines = (run / 'render-observer.log').read_text().splitlines()
    if 'RENDER_SEQUENCE_PASS' not in lines or any(line.startswith('FAILED\t') for line in lines):
        raise AssertionError('Render observation is incomplete; preserve diagnostics, not a GUI PASS')
    captures = [line.split('\t') for line in lines if line.startswith('RENDER_CAPTURE\t')]
    if [int(row[1]) for row in captures] != list(range(5)) or [int(row[2]) for row in captures] != [0, 1, 0, 1, 0]:
        raise AssertionError('Actual House sequence was not observed')
    paint_counts = [int(row[3]) for row in captures]
    if paint_counts[0] == 0 or any(after <= before for before, after in zip(paint_counts, paint_counts[1:])):
        raise AssertionError('Each observed transition requires a new House paint')
    if any(row[4] != '1' or row[5] != '0' for row in captures):
        raise AssertionError('Rendering disabled or existing GL error during observation')
    images = {}
    for row in captures:
        path = Path(row[8]).resolve()
        if not path.is_relative_to(run) or path.read_bytes()[:8] != b'\x89PNG\r\n\x1a\n':
            raise AssertionError('Private framebuffer PNG missing')
        images[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    observation = json.loads((run / 'render-ui-observation.json').read_text())
    if observation.get('image_sha256') != images or observation.get('visible_house_counts') != [0, 1, 0, 1, 0]:
        raise AssertionError('Independent visual inspection of exact readback files is required')
    (args.output / 'verification.json').write_text(json.dumps(dict(result='PASS',
        scope='Real GUI actions and presented House framebuffer on separate current-object observer executable; ordinary shutdown',
        observed_exit_code=code, image_sha256=images, source_hashes_unchanged=original,
        production_gui_hooks_added=False), indent=2), encoding='utf-8')
elif args.save_cancel:
    lines = (run / 'snapshot-probe.log').read_text().splitlines()
    required = {'SAVE_CANCEL_NEW_PASS', 'SAVE_CANCEL_NEW_RETRY_PASS', 'SAVE_CANCEL_EXISTING_PASS', 'SAVE_CANCEL_PASS'}
    if not required.issubset(lines) or not any(line.startswith('SAVE_CANCEL_EXISTING_RETRY_PASS\t') for line in lines):
        raise AssertionError('Actual save cancellation and retries did not finish')
    from g0_verify_snapshot import compare
    target = run / 'cancel-save/cancelled.otbm'
    compare(run / names[0], target)
    if (data / 'user/data/.saving.txt').exists() or list(target.parent.glob('*~')):
        raise AssertionError('Cancellation/retry left marker or backup')
    (args.output / 'verification.json').write_text(json.dumps(dict(result='PASS',
        observed_exit_code=code, runtime_assertions=sorted(required), independent_full_fixture_comparison=True,
        source_hashes_unchanged=original, scope='Synthetic wx Cancel command after actual target creation; new and existing targets, retry/readback'), indent=2), encoding='utf-8')
elif args.queue_allocation:
    lines = (run / 'snapshot-probe.log').read_text(encoding='utf-8').splitlines()
    if 'QUEUE_ALLOCATION_PASS' not in lines or any(line.startswith('FAILED\t') for line in lines):
        raise AssertionError('Queue publication sweep did not finish')
    from g0_verify_snapshot import compare
    scenarios = ('grow', 'count-limit', 'merge-redo', 'memory-limit')
    summary = {}
    committed_notification_failures = 0
    for scenario in scenarios:
        baselines = [line.split('\t') for line in lines if line.startswith('QUEUE_ALLOCATION_BASELINE\t' + scenario + '\t')]
        if len(baselines) != 1:
            raise AssertionError('Missing publication baseline: ' + scenario)
        baseline = int(baselines[0][2])
        expected = list(range(baseline + 5))
        cases = [line.split('\t') for line in lines if line.startswith('QUEUE_ALLOCATION_CASE\t' + scenario + '\t')]
        snapshots = [line.split('\t') for line in lines if line.startswith('QUEUE_ALLOCATION_SNAPSHOT\t' + scenario + '\t')]
        restored = [int(line.split('\t')[2]) for line in lines if line.startswith('QUEUE_ALLOCATION_RESTORED\t' + scenario + '\t')]
        cleared = [int(line.split('\t')[2]) for line in lines if line.startswith('QUEUE_ALLOCATION_CLEAR_PASS\t' + scenario + '\t')]
        if [int(row[2]) for row in cases] != expected or [int(row[2]) for row in snapshots] != expected or restored != expected or cleared != expected:
            raise AssertionError('Incomplete publication sweep: ' + scenario)
        for row in snapshots:
            compare(run / names[0], Path(row[3]))
        rejected = sum('fault=1' in row and 'success=0' in row for row in cases)
        notification_failures = sum('fault=1' in row and 'success=1' in row for row in cases)
        committed_notification_failures += notification_failures
        if scenario != 'memory-limit' and rejected == 0:
            raise AssertionError('Expected actual rejected storage allocation: ' + scenario)
        summary[scenario] = {'baseline_allocations': baseline, 'cases': len(cases),
                             'rejected_publications': rejected, 'reported_notification_failures': notification_failures}
    if committed_notification_failures > (args.output / 'stderr.log').read_text(encoding='utf-8').count(
            'Editor state notification failed: out of memory.'):
        raise AssertionError('Accepted fault cases require their GUI notification diagnostics')
    if (data / 'user/data/.saving.txt').exists() or set(run.glob('*.otbm')) != {run / names[0]}:
        raise AssertionError('Queue test unexpectedly saved a map or marker')
    (args.output / 'verification.json').write_text(json.dumps({
        'result': 'PASS', 'scenarios': summary, 'source_hashes_unchanged': original,
        'wx_setfocus_warnings': (args.output / 'stderr.log').read_text(encoding='utf-8').count('SetFocus'),
        'visual_gui_pass_claimed': False,
        'limitations': ['Test-only factory subclasses arm scalar allocator at actual publication boundary',
                        'Merge window forced through test subclass timestamp; no live peers or persistent OOM',
                        'Preserves existing one-entry count-pruning policy; not a strict resource-budget proof']
    }, indent=2), encoding='utf-8')
elif args.lua_allocation:
    lines = (run / 'snapshot-probe.log').read_text(encoding='utf-8').splitlines()
    if 'LUA_ALLOCATION_PASS' not in lines or any(line.startswith('FAILED\t') for line in lines):
        raise AssertionError('Lua allocation sweep did not finish')
    cases = [line.split('\t') for line in lines if line.startswith('LUA_ALLOCATION_CASE\t')]
    baseline = int(cases[0][2].split('=')[1])
    ordinals = list(range(baseline + 9))
    if [int(case[1]) for case in cases] != ordinals or not any('fault=1' in case for case in cases):
        raise AssertionError('Expected the complete bounded allocation sweep with actual failures')
    notified_failures = sum('fault=1' in case and 'success=1' in case for case in cases)
    notification_diagnostics = (args.output / 'stderr.log').read_text(encoding='utf-8').count(
        'Editor state notification failed: out of memory.')
    if notified_failures > notification_diagnostics:
        raise AssertionError('Committed fault cases require a reported GUI notification failure')
    snapshots = [line.split('\t') for line in lines if line.startswith('LUA_ALLOCATION_SNAPSHOT\t')]
    restored = [int(line.split('\t')[1]) for line in lines if line.startswith('LUA_ALLOCATION_RESTORED\t')]
    if [int(row[1]) for row in snapshots] != ordinals or restored != ordinals:
        raise AssertionError('Every case must restore and snapshot the map')
    from g0_verify_snapshot import compare
    for row in snapshots:
        compare(run / names[0], Path(row[2]))
    smoke = [line.split('\t') for line in lines if line.startswith('LUA_ALLOCATION_SMOKE_400_PASS\t')]
    if len(smoke) != 1 or Path(smoke[0][2]) != run / 'g0-smoke.otbm':
        raise AssertionError('Actual 400-tile smoke/save/readback did not finish')
    from g0_verify_snapshot import canonical
    from g0_compare_roundtrip import read_tiles
    ground = int(smoke[0][1])
    _, tree, companions = canonical(run / 'g0-smoke.otbm')
    actual_tiles = read_tiles(tree)
    expected_tiles = {(x, y, 7): (bytearray((5, x, y, 9)) + ground.to_bytes(2, 'little'), [])
                      for y in range(100, 120) for x in range(100, 120)}
    if actual_tiles != expected_tiles or len(companions) != 2:
        raise AssertionError('Independent 400-tile ground/stack/mask/metadata check failed')
    for suffix, tag in (('-house.xml', 'houses'), ('-spawn.xml', 'spawns')):
        document = ET.parse(run / ('g0-smoke' + suffix)).getroot()
        if document.tag != tag or len(document):
            raise AssertionError('Expected empty smoke companion: ' + suffix)
    for filename, expected_hash in smoke_scripts.items():
        for path in (Path(__file__).with_name(filename), run / filename):
            if hashlib.sha256(path.read_bytes()).hexdigest() != expected_hash:
                raise AssertionError('Smoke script changed: ' + str(path))
    if (data / 'user/data/.saving.txt').exists() or set(run.glob('*.otbm')) != {run / names[0], run / 'g0-smoke.otbm'}:
        raise AssertionError('Lua allocation test unexpectedly saved the fixture or marker')
    (args.output / 'verification.json').write_text(json.dumps({
        'result': 'PASS', 'baseline_allocations': baseline, 'cases': len(cases),
        'complete_commits_with_reported_notification_failure': notified_failures,
        'wx_setfocus_warnings': (args.output / 'stderr.log').read_text(encoding='utf-8').count('SetFocus'),
        'visual_gui_pass_claimed': False,
        'snapshots_independently_compared': len(snapshots), 'source_hashes_unchanged': original,
        'smoke_400_saved_readback_and_independent_comparison': True, 'smoke_script_hashes': smoke_scripts,
        'scope': 'Five existing ground setters, post-callback scalar C++ allocation sweep, original history empty',
        'limitations': ['No real memory exhaustion; no persistent or concurrent allocation failure',
                        'No other mutators or live peers; global shutdown omitted']
    }, indent=2), encoding='utf-8')
elif args.house_allocation:
    lines = (run / 'snapshot-probe.log').read_text(encoding='utf-8').splitlines()
    expected = {'HOUSE_ALLOCATION_THROW_OBSERVED', 'HOUSE_ALLOCATION_OWNER_PASS',
                'HOUSE_ALLOCATION_RETRY_PASS', 'HOUSE_ALLOCATION_CONFLICT_PASS', 'HOUSE_ALLOCATION_PASS'}
    if not expected.issubset(lines) or any(line.startswith('FAILED\t') for line in lines):
        raise AssertionError('House allocation assertions did not finish')
    if (data / 'user/data/.saving.txt').exists() or set(run.glob('*.otbm')) != {run / names[0]}:
        raise AssertionError('House allocation test unexpectedly saved a map or marker')
    (args.output / 'verification.json').write_text(json.dumps({
        'result': 'PASS', 'runtime_assertions': sorted(expected), 'source_hashes_unchanged': original,
        'map_save_performed': False,
        'limitations': ['Tests Houses insertion ownership and retry; not full Action/ActionQueue exception safety',
                        'Global application shutdown omitted after local test map destruction']
    }, indent=2), encoding='utf-8')
elif args.item_methods:
    lines = (run / 'snapshot-probe.log').read_text(encoding='utf-8').splitlines()
    expected = {'ITEM_METHOD_CASE_PASS\t' + name for name in (
        'rotate', 'podium-direction', 'podium-optional-direction')}
    expected.update(('ITEM_METHOD_INVALID_PASS', 'ITEM_METHOD_RESTORE_PASS', 'ITEM_METHODS_PASS'))
    if not expected.issubset(lines) or any(line.startswith('FAILED\t') for line in lines):
        raise AssertionError('Lua method assertions did not finish')
    for path, expected_hash in (
            (source_xml, method_inputs['source_xml_sha256']),
            (private_xml, method_inputs['private_xml_sha256']),
            (script, method_inputs['script_sha256']),
            (run / script.name, method_inputs['script_sha256'])):
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected_hash:
            raise AssertionError('Method input changed: ' + str(path))
    if (data / 'user/data/.saving.txt').exists() or set(run.glob('*.otbm')) != {run / names[0]}:
        raise AssertionError('Method test unexpectedly saved a map or marker')
    (args.output / 'verification.json').write_text(json.dumps({
        'result': 'PASS', 'runtime_assertions': sorted(expected), 'source_hashes_unchanged': original,
        'method_inputs': method_inputs, 'map_save_performed': False,
        'limitations': ['Synthetic private XML Podium type; no deployed BlackTek Podium compatibility proof',
                        'Global application shutdown omitted; separate test module calls _Exit after test editor close']
    }, indent=2), encoding='utf-8')
elif args.marker_startup:
    verify_startup()
elif args.first_save:
    lines = (run / 'snapshot-probe.log').read_text(encoding='utf-8').splitlines()
    expected = {'FIRST_SAVE_MARKER_GUARD_PASS', 'FIRST_SAVE_ROLLBACK_PASS', 'FIRST_SAVE_PASS',
                'FIRST_SAVE_VERSIONED_MARKER_PASS'}
    targets = [Path(line.split('\t')[1]) for line in lines if line.startswith('FIRST_SAVE_SUCCESS_PASS\t')]
    if not expected.issubset(lines) or len(targets) != 1 or any(line.startswith('FAILED\t') for line in lines):
        raise AssertionError('First-save assertions did not finish')
    if args.rollback_removal and not any(line.startswith('REMOVAL_FAILURE_PASS\t') for line in lines):
        raise AssertionError('Actual failed removal and retained marker guard did not finish')
    if args.nested_companions:
        from g0_verify_nested_companions import verify
        verify(args.output, args.fixture)
    from g0_verify_snapshot import canonical
    from g0_compare_roundtrip import read_tiles
    header, tree, companions = canonical(targets[0])
    if read_tiles(tree) or len(companions) != 2:
        raise AssertionError('Expected empty first-save map with both companions')
    for suffix, tag in (('-house.xml', 'houses'), ('-spawn.xml', 'spawns')):
        document = ET.parse(targets[0].with_name(targets[0].stem + suffix)).getroot()
        if document.tag != tag or len(document):
            raise AssertionError('Expected empty first-save companion')
    (args.output / 'verification.json').write_text(json.dumps({
        'result': 'PASS', 'runtime_assertions': sorted(expected), 'source_hashes_unchanged': original,
        'first_save_map': str(targets[0]), 'independent_empty_map_and_xml_check': True,
        'actual_new_target_removal_failure': args.rollback_removal,
        'nested_companion_paths': args.nested_companions
    }, indent=2), encoding='utf-8')
elif args.house_history:
    lines = (run / 'snapshot-probe.log').read_text(encoding='utf-8').splitlines()
    expected = {'HOUSE_UNDO_RETRY_PASS', 'HOUSE_REDO_RETRY_PASS',
                'HOUSE_REPLACEMENT_REJECTED_PASS', 'HOUSE_HISTORY_PASS',
                'HOUSE_ADD_REMOVE_CHAIN_PASS', 'HOUSE_DELETE_RESTORE_PASS', 'HOUSE_HISTORY_CLEANUP_PASS',
                'HOUSE_LIVE_GUARD_PASS'}
    if not expected.issubset(lines) or any(line.startswith('FAILED\t') for line in lines):
        raise AssertionError('House history assertions did not finish')
    from g0_verify_snapshot import compare
    roundtrips = [Path(line.split('\t')[1]) for line in lines if line.startswith('HOUSE_ROUNDTRIP\t')]
    if len(roundtrips) != 1:
        raise AssertionError('Restored house snapshot missing')
    compare(run / names[0], roundtrips[0])
    (args.output / 'verification.json').write_text(json.dumps({
        'result': 'PASS', 'runtime_assertions': sorted(expected), 'source_hashes_unchanged': original,
        'restored_snapshot_compared': str(roundtrips[0])
    }, indent=2), encoding='utf-8')
else:
    subprocess.run([os.sys.executable, str(Path(__file__).with_name('g0_verify_snapshot.py')),
                    str(run / names[0]), str(run / 'snapshot-probe.log'),
                    '--report', str(args.output / 'verification.json')], check=True)
if args.house_gui:
    print('PASS: observed GUI sequence, normal exit and unchanged source hashes')
else:
    print('PASS: runtime assertions and unchanged source hashes' if args.house_history or args.house_allocation or args.lua_allocation or args.queue_allocation or args.item_methods or args.first_save or args.marker_startup else
          'PASS: runtime, independent verification and unchanged source hashes')
