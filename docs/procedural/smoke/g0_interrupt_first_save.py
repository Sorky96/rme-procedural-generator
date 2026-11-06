"""Interrupt only a live private G0 first-save probe after its complete V1 marker.

Default mode requires the exact Error dialog to be observed and recorded in
run/interruption-ui-observation.json. --watch can be armed before launch to
observe the actual complete marker without depending on dialog timing.
This is not a general process-kill tool.
"""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import time
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=lambda text: Path(text).resolve())
parser.add_argument('--watch', action='store_true', help='Arm before launch and observe the complete real V1 marker')
parser.add_argument('--rollback-removal', action='store_true', help='Interrupt at observed incomplete rollback with actual new files')
args = parser.parse_args()
if args.watch and args.rollback_removal:
    parser.error('Rollback removal requires an actually observed Save failed dialog')
output = args.output
repo = Path(__file__).resolve().parents[3]
if not output.is_relative_to(repo / 'local-data/procedural-smoke'):
    raise RuntimeError('Output must be a private procedural-smoke directory')
run = output / 'run'
deadline = time.monotonic() + 60
if args.watch:
    while not ((run / '.g0-snapshot-fixture').is_file() and (output / 'probe-pid.txt').is_file()):
        if time.monotonic() > deadline:
            raise RuntimeError('No private live probe appeared within the observation interval')
        time.sleep(0.01)
if not (run / '.g0-snapshot-fixture').is_file():
    raise RuntimeError('Private fixture guard missing')
executable = output / 'snapshot_probe.exe'
if not args.watch:
    observation = json.loads((run / 'interruption-ui-observation.json').read_text(encoding='utf-8'))
    if observation.get('executable') != str(executable) or observation.get('caption') != ('Save failed' if args.rollback_removal else 'Error'):
        raise RuntimeError('The exact private Error dialog must have been observed')
marker = run / 'data/user/data/.saving.txt'
targets = [run / 'missing-parent' / name for name in ('first.otbm', 'first-house.xml', 'first-spawn.xml')]
expected = ['RME_SAVE_RECOVERY_V1', '10'] + [''] * 7 + [str(p) for p in targets] + ['RME_SAVE_RECOVERY_END']
retained_files = {}
if args.rollback_removal:
    denied = run / 'rollback-denied'
    targets = [denied / name for name in ('partial.otbm', 'partial-spawn.xml')]
    backups = [''] * 7
    backups[1] = str(denied / 'partial-house.xml~')
    expected = ['RME_SAVE_RECOVERY_V1', '9'] + backups + [str(p) for p in targets] + ['RME_SAVE_RECOVERY_END']
    for path in targets + [denied / 'partial-house.xml', denied / 'partial-house.xml~']:
        retained_files[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
while True:
    try:
        contents = marker.read_bytes()
        lines = contents.decode('utf-8').splitlines()
        log = (run / 'snapshot-probe.log').read_text(encoding='utf-8')
    except (FileNotFoundError, PermissionError):
        lines, log = [], ''
    phase_observed = ('REMOVAL_FAILURE_BEGIN\t' in log and 'REMOVAL_FAILURE_PASS\t' not in log) if args.rollback_removal else (
        'FIRST_SAVE_FAILURE_BEGIN\t' in log and 'FIRST_SAVE_ROLLBACK_PASS' not in log)
    if lines == expected and phase_observed:
        break
    if not args.watch or time.monotonic() > deadline:
        raise RuntimeError('The complete actual first-save marker was not observed at the required phase')
    time.sleep(0.001)
if not args.rollback_removal and (any(path.exists() for path in targets) or targets[0].parent.exists()):
    raise RuntimeError('This case requires the missing-destination failure, not existing map files')
pid = int((output / 'probe-pid.txt').read_text())
api = ctypes.WinDLL('kernel32', use_last_error=True)
api.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
api.OpenProcess.restype = wintypes.HANDLE
api.QueryFullProcessImageNameW.argtypes = (wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD))
api.TerminateProcess.argtypes = (wintypes.HANDLE, wintypes.UINT)
api.WaitForSingleObject.argtypes = (wintypes.HANDLE, wintypes.DWORD)
api.GetExitCodeProcess.argtypes = (wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD))
api.CloseHandle.argtypes = (wintypes.HANDLE,)
handle = api.OpenProcess(0x100000 | 0x1000 | 0x0001, False, pid)
if not handle:
    raise ctypes.WinError(ctypes.get_last_error())
try:
    buffer = ctypes.create_unicode_buffer(32768)
    size = wintypes.DWORD(len(buffer))
    if not api.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
        raise ctypes.WinError(ctypes.get_last_error())
    if Path(buffer.value).resolve() != executable:
        raise RuntimeError('PID belongs to another executable; refusing interruption')
    if marker.read_bytes() != contents:
        raise RuntimeError('Marker changed before interruption; no process was terminated')
    (output / 'marker-at-interruption.bin').write_bytes(contents)
    expected_exit = 0x4701
    if not api.TerminateProcess(handle, expected_exit):
        raise ctypes.WinError(ctypes.get_last_error())
    if api.WaitForSingleObject(handle, 10000) != 0:
        raise RuntimeError('Process has not exited; do not restart it')
    status = wintypes.DWORD()
    if not api.GetExitCodeProcess(handle, ctypes.byref(status)) or status.value != expected_exit:
        raise RuntimeError('Unexpected process exit status')
finally:
    api.CloseHandle(handle)
if marker.read_bytes() != contents or (not args.rollback_removal and any(path.exists() for path in targets)):
    raise AssertionError('Unexpected file changes after interruption')
result = {'result': 'EXPECTED_INTERRUPTION', 'pid': pid, 'executable': str(executable),
          'exit_code': status.value, 'phase': 'complete V1 marker observed during actual first-save, before marker cleanup',
          'error_dialog_observed': not args.watch, 'exact_serializer_instruction_known': False,
          'marker_sha256': hashlib.sha256(contents).hexdigest(), 'targets_absent': not args.rollback_removal}
if args.rollback_removal:
    if any(hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest for path, digest in retained_files.items()):
        raise AssertionError('Actual partial files or backup changed during interruption')
    # The killed process cannot execute its ACL guard. Remove only our explicit
    # denial in this exact guarded private directory before the restart test.
    cleanup = subprocess.run(['icacls', str(denied), '/remove:d', '*S-1-1-0'], capture_output=True, text=True)
    (output / 'interruption-acl-cleanup.log').write_text(cleanup.stdout + cleanup.stderr, encoding='utf-8')
    cleanup.check_returncode()
    result.update(case='rollback-removal', retained_files_sha256=retained_files,
        phase='actual incomplete rollback Save failed dialog, new OTBM/spawn retained and house backup retained',
        exact_serializer_instruction_known=False, observed_save_phase='incomplete rollback modal',
        delete_denial_removed_after_process_exit=True)
(output / 'interruption.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
