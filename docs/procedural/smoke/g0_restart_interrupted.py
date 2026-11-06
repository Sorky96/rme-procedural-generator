"""Restart the canonical GUI on a private interrupted first-save fixture.

After inspecting startup and blocked-save dialogs, independently verify files
and write restart-verification.json. A matching report ends only this child.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=lambda value: Path(value).resolve())
output = parser.parse_args().output
repo = Path(__file__).resolve().parents[3]
if not output.is_relative_to(repo / 'local-data/procedural-smoke'):
    raise RuntimeError('Restart requires a private procedural-smoke directory')
event = json.loads((output / 'interruption.json').read_text())
if event['result'] != 'EXPECTED_INTERRUPTION' or event['exit_code'] != 0x4701:
    raise RuntimeError('No verified first-save interruption')
run = output / 'run'
marker = run / 'data/user/data/.saving.txt'
if hashlib.sha256(marker.read_bytes()).hexdigest() != event['marker_sha256']:
    raise RuntimeError('Interrupted marker changed; do not restart this fixture')
manifest = json.loads((output / 'build-manifest.json').read_text())
normal = repo / 'build-ninja/rme.exe'
if hashlib.sha256(normal.read_bytes()).hexdigest() != manifest['base_rme_exe_sha256']:
    raise RuntimeError('Canonical executable differs from the verified build')
restart = output / 'restart'
restart.mkdir()  # Never replace an earlier restart or its evidence.
executable = restart / 'recovery_gui.exe'
shutil.copy2(normal, executable)
shutil.copytree(output / 'assets', restart / 'assets')
config = (run / 'config.toml').read_text(encoding='utf-8')
if 'forgotten.otbm' in config.casefold():
    raise RuntimeError('Forbidden map referenced by restart configuration')
for key in ('only_one_instance', 'welcome_dialog', 'use_updater', 'goto_website_on_boot'):
    config, count = re.subn(r'^' + key + r' = .*?$', key + ' = false', config, flags=re.M)
    if count != 1:
        raise RuntimeError('Expected one private setting: ' + key)
(run / 'config.toml').write_text(config, encoding='utf-8')
environment = dict(os.environ)
environment['PATH'] = str(repo / 'build-ninja') + os.pathsep + environment['PATH']
startup = subprocess.STARTUPINFO()
startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
startup.wShowWindow = 0
with (output / 'restart-stdout.log').open('w', encoding='utf-8') as stdout, \
        (output / 'restart-stderr.log').open('w', encoding='utf-8') as stderr:
    process = subprocess.Popen([str(executable)], cwd=run, env=environment,
                               stdout=stdout, stderr=stderr, startupinfo=startup)
    (output / 'restart-pid.txt').write_text(str(process.pid))
    deadline = time.monotonic() + 300
    report = output / 'restart-verification.json'
    while not report.is_file():
        if process.poll() is not None:
            raise RuntimeError(f'Restart exited before verification: {process.returncode}')
        if time.monotonic() > deadline:
            raise RuntimeError(f'Restart PID {process.pid} remains running; inspect without restarting')
        time.sleep(0.1)
    result = json.loads(report.read_text())
    if result.get('result') != 'PASS' or result.get('executable') != str(executable):
        raise RuntimeError('Restart report does not authorize cleanup of this private child')
    if process.poll() is None:
        process.terminate()  # Deliberate cleanup after verification, not normal-shutdown coverage.
    code = process.wait(timeout=10)
    (output / 'restart-exit.json').write_text(json.dumps({
        'pid': process.pid, 'executable': str(executable), 'exit_code': code,
        'intentional_cleanup_after_verification': True
    }, indent=2))
print('PASS: verified private restart; child cleanup observed; normal shutdown not tested')
