"""Verify the bounded first-save interruption/restart case without modifying its marker."""
import argparse
import hashlib
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=lambda text: Path(text).resolve())
parser.add_argument('fixture', type=lambda text: Path(text).resolve())
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
if not args.output.is_relative_to(repo / 'local-data/procedural-smoke'):
    raise RuntimeError('Private output required')
run = args.output / 'run'
event = json.loads((args.output / 'interruption.json').read_text())
if event['result'] != 'EXPECTED_INTERRUPTION' or event['exit_code'] != 0x4701:
    raise AssertionError('Actual interruption was not verified')
marker = run / 'data/user/data/.saving.txt'
contents = marker.read_bytes()
if contents != (args.output / 'marker-at-interruption.bin').read_bytes() or hashlib.sha256(contents).hexdigest() != event['marker_sha256']:
    raise AssertionError('Marker changed after interruption or restart')
targets = [run / 'missing-parent' / name for name in ('first.otbm', 'first-house.xml', 'first-spawn.xml')]
rollback_removal = event.get('case') == 'rollback-removal'
expected_lines = ['RME_SAVE_RECOVERY_V1', '10'] + [''] * 7 + [str(path) for path in targets] + ['RME_SAVE_RECOVERY_END']
if rollback_removal:
    denied = run / 'rollback-denied'
    targets = [denied / name for name in ('partial.otbm', 'partial-spawn.xml')]
    backups = [''] * 7
    backups[1] = str(denied / 'partial-house.xml~')
    expected_lines = ['RME_SAVE_RECOVERY_V1', '9'] + backups + [str(path) for path in targets] + ['RME_SAVE_RECOVERY_END']
    for path, digest in event['retained_files_sha256'].items():
        resolved = Path(path).resolve()
        if not resolved.is_relative_to(denied) or hashlib.sha256(resolved.read_bytes()).hexdigest() != digest:
            raise AssertionError('Actual partial target or backup changed after restart')
if contents.decode('utf-8').splitlines() != expected_lines:
    raise AssertionError('Unexpected interrupted target set')
if ((not rollback_removal and (any(path.exists() for path in targets) or targets[0].parent.exists())) or
        list(run.glob('blocked-after-crash*'))):
    raise AssertionError('Restart or rejected save created target files')
names = ('house-roundtrip.otbm', 'house-roundtrip-house.xml', 'house-roundtrip-spawn.xml')
expected = json.loads((run / 'inputs-original.json').read_text())
for directory in (run, args.fixture):
    if {name: hashlib.sha256((directory / name).read_bytes()).hexdigest() for name in names} != expected:
        raise AssertionError('Original or copied fixture changed')
executable = args.output / 'restart/recovery_gui.exe'
observation = json.loads((args.output / 'restart-ui-observation.json').read_text())
if observation['executable'] != str(executable) or not {'Incomplete save', 'Save failed'}.issubset(observation['captions']):
    raise AssertionError('Actual startup and save refusal were not observed in the private GUI')
manifest = json.loads((args.output / 'build-manifest.json').read_text())
if hashlib.sha256(executable.read_bytes()).hexdigest() != manifest['base_rme_exe_sha256']:
    raise AssertionError('Restart did not use the verified canonical executable')
result = {'result': 'PASS', 'executable': str(executable),
          'scope': 'Actual process interruption after complete first-save V1 marker; canonical GUI restart retains marker and refuses subsequent save',
          'source_hashes_unchanged': expected, 'marker_sha256': event['marker_sha256'],
          'targets_remain_absent': not rollback_removal, 'observed_ui_captions': observation['captions'],
          'not_verified': ['exact serializer instruction at interruption', 'other save/recovery interruption phases',
                           'partial target files', 'power-loss durability', 'normal GUI shutdown']}
if rollback_removal:
    result.update(scope='Actual interruption during incomplete rollback with new OTBM/spawn and existing house backup; canonical restart retains files and marker and refuses another save',
        retained_files_sha256=event['retained_files_sha256'], case='rollback-removal',
        not_verified=['other save/recovery interruption phases', 'power-loss durability', 'normal GUI shutdown'])
# The waiting launcher owns only this private child and uses this report to end it.
(args.output / 'restart-verification.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
