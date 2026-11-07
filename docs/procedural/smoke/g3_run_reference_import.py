"""G3 runner: analyse a private COPY of a reference OTBM with the private probe EXE.

Arguments: OUTPUT_DIR MAP_FILE PROFILE_DATA CONFIG_TOML --exe PROBE_EXE
Optional: --client-id NAME      client entry to load (default 1098-BlackTek)
          --client-path DIR     folder with that client's Tibia.dat/spr; hashed before and after
          --max-file-mib N      refuse map files larger than N MiB (default 64)
          --timeout S           seconds before the run is declared hung (default 600)
The source map and its companion XML are hashed before and after and never opened for writing.
OUTPUT_DIR must be new and live under local-data/ (git-ignored); it receives the copy and the reports.
The run always writes verification.json (PASS or FAIL with the probe's reason); exit code 0 only on PASS.
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import hashlib
import json
import os
import re
import shutil
import subprocess
import time
from pathlib import Path

repo = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser()
for name in ('output', 'map', 'profile_data', 'config'):
    parser.add_argument(name, type=lambda value: Path(value).resolve())
parser.add_argument('--exe', type=lambda value: Path(value).resolve(), required=True)
parser.add_argument('--client-id', default='1098-BlackTek')
parser.add_argument('--client-path', type=lambda value: Path(value).resolve())
parser.add_argument('--max-file-mib', type=int, default=64)
parser.add_argument('--timeout', type=int, default=600)
parser.add_argument('--memory-limit-mib', type=int, default=2048, help='working set the whole process may reach (default 2048)')
parser.add_argument('--estimate-factor', type=int, help='override the pre-load estimator factor (tests of the post-load check)')
parser.add_argument('--cancel-after-ms', type=int, help='cancellation test: request stop after this delay; PASS means the import was cancelled')
parser.add_argument('--nohook-check', action='store_true', help='also load the file without the abort hook and compare tile counts (doubles memory)')
parser.add_argument('--expect-refusal', help='refusal test: PASS means the import was refused with this error code')
args = parser.parse_args()

if not args.output.is_relative_to(repo / 'local-data'):
    raise RuntimeError('Output must be under local-data/ (git-ignored)')
if args.output.exists():
    raise RuntimeError('Refusing to reuse an output directory')

stem = args.map.stem
companions = [p for p in args.map.parent.iterdir() if p.is_file() and p.name.startswith(stem + '-') and p.suffix == '.xml']
client_files = [args.client_path / n for n in ('Tibia.dat', 'Tibia.spr')] if args.client_path else []


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(1 << 22), b''):
            h.update(chunk)
    return h.hexdigest()


def inputs():
    return {p.name: digest(p) for p in [args.map, *sorted(companions), *client_files]}


class MemoryCounters(ctypes.Structure):
    _fields_ = [('cb', wt.DWORD), ('PageFaultCount', wt.DWORD), ('PeakWorkingSetSize', ctypes.c_size_t),
                ('WorkingSetSize', ctypes.c_size_t), ('QuotaPeakPagedPoolUsage', ctypes.c_size_t),
                ('QuotaPagedPoolUsage', ctypes.c_size_t), ('QuotaPeakNonPagedPoolUsage', ctypes.c_size_t),
                ('QuotaNonPagedPoolUsage', ctypes.c_size_t), ('PagefileUsage', ctypes.c_size_t),
                ('PeakPagefileUsage', ctypes.c_size_t)]


def peak_working_set(pid):
    """Peak working set (bytes) of a live process, or None. Polled until the process exits."""
    kernel, psapi = ctypes.windll.kernel32, ctypes.windll.psapi
    kernel.OpenProcess.restype = wt.HANDLE
    handle = kernel.OpenProcess(0x1000 | 0x0400, False, pid)  # QUERY_LIMITED_INFORMATION | QUERY_INFORMATION
    if not handle:
        return None
    counters = MemoryCounters()
    counters.cb = ctypes.sizeof(counters)
    best = None
    try:
        while True:
            if psapi.GetProcessMemoryInfo(handle, ctypes.byref(counters), counters.cb):
                best = max(best or 0, counters.PeakWorkingSetSize)
            code = wt.DWORD()
            kernel.GetExitCodeProcess(handle, ctypes.byref(code))
            if code.value != 259:  # STILL_ACTIVE
                return best
            time.sleep(0.05)
    finally:
        kernel.CloseHandle(handle)


original = inputs()
run = args.output / 'run'
run.mkdir(parents=True)
data = run / 'data'
shutil.copytree(args.profile_data / '1098', data / '1098')
for path in args.profile_data.iterdir():
    if path.is_file():
        shutil.copy2(path, data / path.name)
clients_db = (data / 'clients.toml').read_text(encoding='utf-8') if (data / 'clients.toml').exists() else ''
if f"name = '{args.client_id}'" not in clients_db:
    # Otherwise RME silently falls back to the first client in the file and loads that one.
    raise RuntimeError(f"Client '{args.client_id}' is not defined in the profile's clients.toml; use a profile that lists it")
shutil.copy2(args.map, run / 'reference.otbm')
config = args.config.read_text(encoding='utf-8')
config = re.sub(r'^recent_edited_map_path = .*?$', "recent_edited_map_path = ''", config, flags=re.M)
config = re.sub(r'^recent_files = \[[\s\S]*?^\]', 'recent_files = []', config, flags=re.M)
config = re.sub(r"^data_directory = .*?$", "data_directory = '" + data.as_posix() + "/'", config, flags=re.M)
config = re.sub(r'^use_custom_data_directory = .*?$', 'use_custom_data_directory = 1', config, flags=re.M)
config = re.sub(r'^indirectory_installation = .*?$', 'indirectory_installation = 1', config, flags=re.M)
for key in ('only_one_instance', 'welcome_dialog', 'use_updater', 'goto_website_on_boot'):
    config = re.sub(r'^' + key + r' = .*?$', key + ' = false', config, flags=re.M)
if args.client_path:
    # The template's last section names the client and its asset folder; point it at the requested client.
    config = config[:config.rindex('[[clients]]')] + (
        "[[clients]]\nclientPath = '" + str(args.client_path) + "\\'\ndefault = true\nname = '" + args.client_id + "'\n")
(run / 'config.toml').write_text(config, encoding='utf-8')
(run / '.g3-reference-run').write_text('private copy; read-only import', encoding='utf-8')
(run / 'client-id.txt').write_text(args.client_id, encoding='utf-8')
(run / 'limits.txt').write_text(str(args.max_file_mib << 20), encoding='utf-8')
(run / 'memory-limit.txt').write_text(str(args.memory_limit_mib << 20), encoding='utf-8')
if args.nohook_check:
    (run / 'nohook-check.txt').write_text('1', encoding='utf-8')
if args.estimate_factor is not None:
    (run / 'estimate-factor.txt').write_text(str(args.estimate_factor), encoding='utf-8')
if args.cancel_after_ms is not None:
    (run / 'cancel-after-ms.txt').write_text(str(args.cancel_after_ms), encoding='utf-8')
if not (args.output / 'assets').exists():
    shutil.copytree(repo / 'build-ninja/assets', args.output / 'assets')
shutil.copy2(args.exe, args.output / args.exe.name)
# The builder links the probe without rme.exe's manifest; without Common Controls v6 wxWidgets
# shows a modal warning at startup that blocks an unattended run. Copy the manifest from rme.exe.
manifest = args.output / 'rme.manifest'
mt = sorted(Path('C:/Program Files (x86)/Windows Kits/10/bin').glob('*/x64/mt.exe'))
if not mt:
    raise RuntimeError('mt.exe (Windows SDK) not found; the probe would show a blocking wx dialog')
probe_exe = args.output / args.exe.name
for command in ([str(mt[-1]), '-nologo', f'-inputresource:{repo / "build-ninja/rme.exe"};#1', f'-out:{manifest}'],
                [str(mt[-1]), '-nologo', '-manifest', str(manifest), f'-outputresource:{probe_exe};#1']):
    embed = subprocess.run(command, capture_output=True, text=True)
    if embed.returncode != 0:
        raise RuntimeError('Manifest embedding failed: ' + embed.stdout + embed.stderr)
environment = dict(os.environ)
environment['PATH'] = str(repo / 'build-ninja') + os.pathsep + environment['PATH']
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
started = time.monotonic()
with (args.output / 'stdout.log').open('w') as out, (args.output / 'stderr.log').open('w') as err:
    process = subprocess.Popen([str(probe_exe)], cwd=run, env=environment, stdout=out, stderr=err, startupinfo=startup)
    # Polling ends when the probe exits; a hung probe is reported, never killed here.
    import threading
    peak = {}
    watcher = threading.Thread(target=lambda: peak.setdefault('bytes', peak_working_set(process.pid)), daemon=True)
    watcher.start()
    try:
        code = process.wait(timeout=args.timeout)
    except subprocess.TimeoutExpired:
        raise RuntimeError(f'Probe still running as PID {process.pid}; inspect it before doing anything else')
    watcher.join(timeout=5)
wall = time.monotonic() - started
unchanged = inputs() == original
if not unchanged:
    raise AssertionError('Source map, companions or client files changed')
log_path = run / 'g3-probe.log'
log = log_path.read_text(encoding='utf-8') if log_path.exists() else ''
if args.cancel_after_ms is not None:
    passed = code == 0 and 'CANCEL_PASS' in log
elif args.expect_refusal:
    passed = code == 9 and f'IMPORT_ERROR\t{args.expect_refusal}' in log
else:
    passed = code == 0 and 'IMPORT_PASS' in log
(args.output / 'verification.json').write_text(json.dumps({
    'result': 'PASS' if passed else 'FAIL', 'exit_code': code, 'probe_log': log.strip(),
    'client_id': args.client_id, 'wall_seconds': round(wall, 1), 'peak_working_set_bytes': peak.get('bytes'),
    'source_hashes_unchanged': original}, indent=2), encoding='utf-8')
print('PASS' if passed else 'FAIL', json.dumps({'wall_s': round(wall, 1), 'peak_ws_mib': round((peak.get('bytes') or 0) / 2**20), 'log': log.strip()[:120]}))
raise SystemExit(0 if passed else 2)
