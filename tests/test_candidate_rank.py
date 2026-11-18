"""Offline CLI contract tests. Run with py -I tests/test_candidate_rank.py EXE SCRATCH_DIR.

All fixtures are synthetic. No local-data, client assets or private maps are needed.
"""
import csv
import importlib.util
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
    frozen = (Path(__file__).resolve().parents[1] / 'docs/procedural/g6-bands-v1/m06.bands').read_text(encoding='utf-8')
    band.write_text(frozen, encoding='utf-8')

    def run(*options, code=0, side='100', first='1729', last='1729'):
        result = subprocess.run([str(exe), str(profile), side, first, last, *options], capture_output=True)
        assert result.returncode == code, (options, result.returncode, result.stderr.decode())
        return result.stdout

    def rows(report):
        text = report.decode().splitlines()
        header = next(i for i, line in enumerate(text) if line.startswith('userSeed\t'))
        return list(csv.DictReader(io.StringIO('\n'.join(line for line in text[header:] if not line.startswith(('winner\t', 'winnerB\t')))), delimiter='\t'))

    one = run('--count', '1')
    assert b'similarity\tnot-evaluated (no index)' in one
    run('--copy-index', str(scratch / 'missing.i53'), code=2)
    bad_index = scratch / 'broken.i53'
    bad_index.write_text('i53-index/99\ncorrupt\n')
    run('--copy-index', str(bad_index), code=2)
    row = rows(one)[0]
    assert row['seed'] == '1729' and row['surfaceFingerprint'] == '17159159863505677800'
    assert 'mode=measure-only' in one.decode() and b'winner\t' not in one
    four = rows(run())
    assert len(four) == 4 and len({row['seed'] for row in four}) == 4
    ranking = run('--bands', str(band), '--count', '16', side='128', first='1001', last='1002')
    assert ranking == run('--bands', str(band), '--count', '16', side='128', first='1001', last='1002')
    spec = importlib.util.spec_from_file_location('matrix', Path(__file__).resolve().parents[1] / 'docs/procedural/smoke/g6_measure_candidates.py')
    matrix = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(matrix)
    blocks = [run('--bands', str(band), '--count', '16', side='128', first=seed, last=seed) for seed in ('1001', '1002')]
    assert matrix.merge_reports(blocks) == ranking  # Partitioning never changes bytes or ordering.
    try:
        matrix.merge_reports([blocks[0], blocks[1].replace(b'count=16', b'count=15', 1)])
        raise AssertionError('inconsistent block header accepted')
    except RuntimeError:
        pass
    records = rows(ranking)
    experiment = run('--bands', str(band), '--count', '16', '--i51b', side='128', first='1001', last='1002')
    assert rows(experiment) == records  # No regeneration or scoring change between orders.
    assert b'experiment\tI51b' in experiment and b'winnerB\t' not in ranking
    assert len(records) == 32
    for seed in ('1001', '1002'):
        candidates = [row for row in records if row['userSeed'] == seed]
        best = min(candidates, key=lambda r: (r['hardFail'] == 'true', -int(r['inBand']), int(r['distance']), int(r['index'])))
        assert f'winner\t{seed}\t{best["index"]}\t'.encode() in ranking
        assert f'winner\t{seed}\t{best["index"]}\t'.encode() in experiment
        best_b = min(candidates, key=lambda r: (r['hardFail'] == 'true', int(r['distance']), -int(r['inBand']), int(r['index'])))
        assert f'winnerB\t{seed}\t{best_b["index"]}\t'.encode() in experiment
    run('--i51b', code=2)
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
    band.write_text(frozen.replace('channels.L 384 25728 57600', 'channels.L -1 0 0'), encoding='utf-8')
    run('--bands', str(band), code=2)
    band.write_text('g6-ranking-bands/1\nscale.E4 100 6700 15000\nscale.E4 100 6700 15000\n', encoding='utf-8')
    run('--bands', str(band), code=2)
    band.write_text('g6-ranking-bands/1\nmade.up 100 6700 15000\n', encoding='utf-8')
    run('--bands', str(band), code=2)
    band.write_text(frozen + 'scale.E4 100 6700 15000\n', encoding='utf-8')
    run('--bands', str(band), code=2)
    band.write_text('g6-ranking-bands/1\npaths.W 100 6700 15000\n', encoding='utf-8')
    run('--bands', str(band), '--count', '1', code=2)  # Missing 25 columns is a policy error.
    band.write_text(frozen, encoding='utf-8')
    missing = run('--bands', str(band), '--count', '1')
    assert rows(missing)[0]['paths.W'] == 'NA' and b'winner\t' in missing
    level = run('--count', '1', '--g5-levels', '--rock-blocks', '1', side='128')
    assert len(rows(level)) == 1
    print('CANDIDATE_RANK_CLI_PASS (synthetic; ranking, repeatability, N=1 fingerprint, bounds, refusals, levels)')


if __name__ == '__main__':
    main()
