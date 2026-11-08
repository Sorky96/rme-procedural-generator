"""G4-A: writes the reference manifest (g4-manifest/1) for the 19 maps of the plain 10.98 client.

Arguments: ALIASES_JSON OUT_FILE
ALIASES_JSON is the local, git-ignored alias table of the set (alias -> file name and sha256).
The manifest carries aliases and hashes only, never file names. Roles follow the user's decision of
2026-11-07: tileset samples (Klank_Jungle_Tree_Samples, Klank_Tile_sets, Klank_Tilesets_3) are samples, the
four 'shape' maps are shape-only, everything else teaches style. Declared families come from the file
names (same author/theme) and are only a prior: the fingerprint test merges further families by content.
"""
import json
import sys
from pathlib import Path

ROLE = {'M02': 'sample', 'M03': 'sample', 'M04': 'sample',
        'M07': 'shape', 'M08': 'shape', 'M09': 'shape', 'M10': 'shape'}
# Declared by name: the shape pairs are one shape, the swamp, winter and cave maps are variants of one theme.
FAMILY = {'M07': 'shape', 'M08': 'shape', 'M09': 'shape', 'M10': 'shape',
          'M12': 'swamp', 'M13': 'swamp', 'M11': 'winter', 'M18': 'winter', 'M14': 'caves', 'M16': 'caves',
          'M02': 'klank-samples', 'M03': 'klank-samples', 'M04': 'klank-samples'}

aliases = json.loads(Path(sys.argv[1]).read_text(encoding='utf-8'))
lines = ['g4-manifest/1', '# alias\tsha256\tclient\trole\tfamily']
for alias, info in sorted(aliases.items()):
    lines.append('\t'.join([alias, info['sha256'], '10.98', ROLE.get(alias, 'style'), FAMILY.get(alias, alias.lower())]))
Path(sys.argv[2]).write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
