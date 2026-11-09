"""Reproduce the shape-metric tables of G4_GATE_STATUS.md (iterations 4-11) with one command.

Runs build-ninja/source/procedural/style_render.exe for every profile and seed, aggregates the per-seed lines it prints
into medians (min..max across seeds), runs everything TWICE and fails unless both aggregated reports are byte-identical.

  py -I docs/procedural/smoke/g5_shape_table.py PROFILE_DIR OUT_FILE [--side 512] [--seeds 1001-1032] [--profiles m06,m19]

PROFILE_DIR holds <style>.profile files (private, local-data, git-ignored). No reference map is read.
Metric legend (as printed by style_render):
  rivers: T tortuosity, S straight, W width (milli); channels: L length/64k, W width, B branches/100, C connected
  scale: E4/E16/E64 lag excess; trans: rock/dirt composition (4 values); patch: D F P RB RF RS + 5 shares
  paths: count L W B cells rockadj rocknear; outline: median wall cells; build: count per64k long short
"""
import argparse
import re
import statistics
import subprocess
import sys
import tempfile
from collections import defaultdict
from pathlib import Path

repo = Path(__file__).resolve().parents[3]
exe = repo / 'build-ninja/source/procedural/style_render.exe'

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument('profile_dir', type=Path)
parser.add_argument('out_file', type=Path)
parser.add_argument('--side', type=int, default=512)
parser.add_argument('--seeds', default='1001-1032')
parser.add_argument('--profiles', default='m06,m19')
args = parser.parse_args()
first, last = (int(part) for part in args.seeds.split('-'))
seeds = [str(seed) for seed in range(first, last + 1)]

NUMBER = r'(-?\d+|NA)'

# Per-profile settings that are NOT part of the profile (D20): path length target and building density.
DEFAULT_SETTINGS = {'m06': ['pathLength=40', 'buildingDensity=30'], 'm19': ['pathLength=80', 'buildingDensity=40']}
LINE = re.compile(r'^(seed|channels seed|scale seed|trans seed|patch seed|paths seed|outline seed|build seed|settle seed) (\d+): (.*)$')
PAIRS = re.compile(r'([A-Za-z0-9]+) (-?\d+|NA)(?= |$)')
TRANS = re.compile(r'^R ' + ' '.join([NUMBER] * 4) + r' \| D ' + ' '.join([NUMBER] * 4) + '$')
SHARES = re.compile(r'shares ' + ' '.join([NUMBER] * 5))


def value(text):
    return None if text == 'NA' else int(text)


def parse_line(line):
    """One style_render output line -> {metric: int | None}. Unknown lines are ignored."""
    match = LINE.match(line)
    if not match:
        return {}
    kind = {'seed': 'rivers'}.get(match[1].split()[0], match[1].split()[0])
    body = match[3]
    if kind == 'trans':
        groups = TRANS.match(body)
        names = [f'R{i}' for i in range(4)] + [f'D{i}' for i in range(4)]
        return {f'trans.{n}': value(g) if groups else None for n, g in zip(names, groups.groups() if groups else [None] * 8)}
    out = {}
    shares = SHARES.search(body)
    if shares:
        for i, g in enumerate(shares.groups()):
            out[f'{kind}.share{i}'] = value(g)
        body = body[:shares.start()]
    for name, v in PAIRS.findall(body):
        out[f'{kind}.{name}'] = value(v)
    return out


def run_once(profile: Path, scratch: Path, settings):
    result = subprocess.run([str(exe), str(profile), str(scratch), str(args.side), *settings, *seeds], capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(f'style_render failed for {profile.name}: {result.stderr[-500:]}')
    samples = defaultdict(list)
    for line in result.stdout.splitlines():
        for key, v in parse_line(line.strip()).items():
            samples[key].append(v)
    return samples


def report():
    lines = [f'# side {args.side}, seeds {args.seeds} ({len(seeds)}), median [min..max]; NA = not measurable']
    for style in args.profiles.split(','):
        profile = args.profile_dir / f'{style}.profile'
        if not profile.is_file():
            sys.exit(f'missing profile {profile}')
        with tempfile.TemporaryDirectory() as scratch:
            samples = run_once(profile, Path(scratch), DEFAULT_SETTINGS.get(style, []))
        lines.append(f'## {style} (settings: ' + (' '.join(DEFAULT_SETTINGS.get(style, [])) or 'profile only') + ')')
        for key in sorted(samples):
            values = [v for v in samples[key] if v is not None]
            if not values:
                lines.append(f'{key}: NA')
                continue
            lines.append(f'{key}: {statistics.median(values):g} [{min(values)}..{max(values)}] n={len(values)}/{len(samples[key])}')
    return '\n'.join(lines) + '\n'


first_report = report()
second_report = report()
args.out_file.parent.mkdir(parents=True, exist_ok=True)
args.out_file.write_text(first_report, encoding='utf-8')
print(first_report)
if first_report != second_report:
    print('REPEATABILITY_FAIL: two runs differ')
    sys.exit(1)
print('REPEATABILITY_PASS: two runs byte-identical')
