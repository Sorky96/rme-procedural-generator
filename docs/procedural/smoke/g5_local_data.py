"""Locate the private local-data folder shared by the G5 smoke scripts.

local-data is not tracked, so a worktree has none of its own. The main checkout is the first entry of
`git worktree list`; its local-data is the default source, and --local-data overrides it.
"""
import subprocess
from pathlib import Path


def main_checkout_local_data(repo):
    listing = subprocess.run(['git', '-C', str(repo), 'worktree', 'list', '--porcelain'], capture_output=True, text=True)
    first = next((line[len('worktree '):] for line in listing.stdout.splitlines() if line.startswith('worktree ')), None)
    return Path(first) / 'local-data' if listing.returncode == 0 and first else Path(repo) / 'local-data'
