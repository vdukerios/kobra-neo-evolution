"""Validate source-derived simulator data without a firmware compiler or printer."""
from pathlib import Path
import re
import subprocess
import sys

here = Path(__file__).resolve().parent
root = here.parents[1]
checks = [
    [sys.executable, '-I', '-B', str(here / 'build_kne_candidate.py'), '--check'],
    [sys.executable, '-I', '-B', str(here / 'test_uniqueness.py')],
    [sys.executable, '-I', '-B', str(here / 'test_maintenance_source.py')],
    ['node', str(here / 'test_menus.js')],
    ['node', str(here / 'test_smoke.js')],
]
for command in checks:
    subprocess.run(command, cwd=root, check=True)
html = (here.parent / 'simulator.html').read_bytes()
assert html == (root / 'docs/index.html').read_bytes(), 'The hosted demo is stale'
for name, script in re.findall(r'<script id="([^"]+)">([\s\S]*?)</script>', html.decode('utf-8')):
    subprocess.run(['node', '--check', '-'], input=script, encoding='utf-8', check=True)
source = root / 'firmware/source/Marlin'
configuration = (source / 'Configuration.h').read_text(encoding='utf-8')
url = 'https://github.com/vdukerios/kobra-neo-evolution'
for key in ['SOURCE_CODE_URL', 'WEBSITE_URL']:
    assert re.search(r'^#define\s+' + key + r'\s+"' + re.escape(url) + r'"', configuration, re.M)
assert not list((root / 'releases').rglob('*.bin')), 'This is a source-only update'
print('Candidate data, routes, behavior, JavaScript and identical hosted demo checked. Firmware remains uncompiled and untested on hardware.')
