"""Offline CLI contract tests. Run with py -I tests/test_candidate_rank.py EXE SCRATCH_DIR.

All fixtures are synthetic. No local-data, client assets or private maps are needed.
"""
import csv
import io
import subprocess
import sys
from pathlib import Path


def main():
    exe, scratch = Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve()
    scratch.mkdir(parents=True, exist_ok=True)
    profile = scratch / 'synthetic.profile'
    kinds = 'Grass Dirt Rock Shore Water Other Unmapped NoGround'.split()
    lines = ['g4-profile/6', 'client\tsynthetic', 'level\tsurface', 'floors\t0\t7',
             'assets\totb_sha256\tsynthetic', 'assets\tdat_sha256\tsynthetic',
             'stat\tkind_tiles' + '\t0' * 8]
    lines += ['stat\tpairs\t' + kind + '\t0' * 8 for kind in kinds]
    # Default SurfaceTuning, not a fitted production style or a ranking policy.
    lines += ['tuning\t24\t60\t12\t3\t20\t3\t80\t8\t300\t6000\t0']
    profile.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    band = scratch / 'synthetic.bands'
    band.write_text('g6-ranking-bands/1\nscale.E4 100 6700 15000\n', encoding='utf-8')

    def run(*options, code=0, side='100', first='1729', last='1729'):
        result = subprocess.run([str(exe), str(profile), side, first, last, *options], capture_output=True)
        assert result.returncode == code, (options, result.returncode, result.stderr.decode())
        return result.stdout

    def rows(report):
        text = report.decode().splitlines()
        header = next(i for i, line in enumerate(text) if line.startswith('userSeed\t'))
        return list(csv.DictReader(io.StringIO('\n'.join(line for line in text[header:] if not line.startswith('winner\t'))), delimiter='\t'))

    one = run('--count', '1')
    row = rows(one)[0]
    assert row['seed'] == '1729' and row['surfaceFingerprint'] == '17159159863505677800'
    assert 'mode=measure-only' in one.decode() and b'winner\t' not in one
    four = rows(run())
    assert len(four) == 4 and len({row['seed'] for row in four}) == 4
    ranking = run('--bands', str(band), '--count', '16', side='128', first='1001', last='1002')
    assert ranking == run('--bands', str(band), '--count', '16', side='128', first='1001', last='1002')
    records = rows(ranking)
    assert len(records) == 32
    for seed in ('1001', '1002'):
        candidates = [row for row in records if row['userSeed'] == seed]
        best = min(candidates, key=lambda r: (r['hardFail'] == 'true', -int(r['inBand']), int(r['distance']), int(r['index'])))
        assert f'winner\t{seed}\t{best["index"]}\t'.encode() in ranking
    run('--count', '0', code=2)
    run('--count', '17', code=2)
    run('--count', '-1', code=2)
    run('--count', '4junk', code=2)
    run('--unknown', '1', code=2)
    run('--count', code=2)
    run(side='0', code=2)
    run(side='1025', code=2)
    run(first='2', last='1', code=2)
    run('--count', '1', first=str(2**64 - 1), last=str(2**64 - 1))
    band.write_text('g6-ranking-bands/1\nscale.E4 0 0 0\n', encoding='utf-8')
    run('--bands', str(band), code=2)
    band.write_text('g6-ranking-bands/1\nscale.E4 100 6700 15000\nscale.E4 100 6700 15000\n', encoding='utf-8')
    run('--bands', str(band), code=2)
    band.write_text('g6-ranking-bands/1\nmade.up 100 6700 15000\n', encoding='utf-8')
    run('--bands', str(band), code=2)
    band.write_text('g6-ranking-bands/1\npaths.W 100 6700 15000\n', encoding='utf-8')
    missing = run('--bands', str(band), '--count', '1', code=4)
    assert b'score-error:' in missing and b'winner\t' not in missing
    level = run('--count', '1', '--g5-levels', '--rock-blocks', '1', side='128')
    assert len(rows(level)) == 1
    print('CANDIDATE_RANK_CLI_PASS (synthetic; ranking, repeatability, N=1 fingerprint, bounds, refusals, levels)')


if __name__ == '__main__':
    main()
