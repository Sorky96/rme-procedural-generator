"""G3-B: compare the reports of a run on g3-unknown.otbm with g3-unknown-expected.json.

Usage: py -I g3_verify_unknown.py RUN_OUTPUT_DIR EXPECTED_JSON
Exit code 0 only when every expected total matches. Reads only; writes nothing.
"""
import json
import sys
from pathlib import Path

run, expected_path = (Path(a).resolve() for a in sys.argv[1:3])
expected = json.loads(expected_path.read_text(encoding='utf-8'))
reference = json.loads((run / 'run/reference-report.json').read_text(encoding='utf-8'))
semantic = json.loads((run / 'run/semantic-report.json').read_text(encoding='utf-8'))
verification = json.loads((run / 'verification.json').read_text(encoding='utf-8'))

actual_ids = {str(entry['id']): entry['count'] for entry in reference['unknown_ids']}
floors = {str(z): n for z, n in enumerate(reference['tiles_per_floor']) if n}
checks = {
    'run passed with unchanged inputs': verification['result'] == 'PASS',
    'tile count': reference['tiles'] == expected['tiles'],
    'tiles per floor': floors == expected['floors'],
    'unknown ids and counts': actual_ids == expected['unknown_ids'],
    'unknown total': reference['unknown_ground'] + reference['unknown_items'] == expected['unknown_total'],
    'tiles with unknown trait': semantic['tiles_per_trait']['unknown'] == expected['tiles_with_unknown_trait'],
    'has_ground at most': semantic['tiles_per_trait']['has_ground'] <= expected['tiles_with_ground_trait_at_most'],
    'ground key tiles sum to tile count': sum(k['tiles'] for k in semantic['ground_keys']) == expected['tiles'],
}
for name, ok in checks.items():
    print(('PASS ' if ok else 'FAIL ') + name)
sys.exit(0 if all(checks.values()) else 1)
