"""Read-only checks of the maintenance source contract; no C++ execution."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
source = root / 'firmware/source/Marlin'
counter = (source / 'src/module/printcounter.cpp').read_text(encoding='utf-8')
reset = counter.split('bool PrintCounter::resetServiceInterval', 1)[1].split('bool PrintCounter::needsService', 1)[0]
assert 'return saveStats();' in reset, 'Reset must report the actual storage result'
for n in [1, 2, 3]:
    branch = re.search(r'case ' + str(n) + r':(.*?)(?=case |\n\s*})', reset, re.S)[1]
    assert re.search(r'data.nextService' + str(n) + r'\s*=\s*SERVICE_INTERVAL_SEC_' + str(n) + r';\s*break;', branch)
    assert len(re.findall(r'nextService\d\s*=', branch)) == 1, 'Counters must reset independently'
for name, end in [('tick', '// @Override'), ('stop', '// @Override')]:
    body = counter.split('PrintCounter::' + name + '()', 1)[1].split(end, 1)[0]
    assert 'deltaDuration()' in body
    assert 'updateServiceIntervals(delta)' in body, name + ': missing final elapsed time'
for n in [1, 2, 3]:
    assert f'data.nextService{n} -= _MIN(delta, data.nextService{n});' in counter
config = (source / 'Configuration.h').read_text(encoding='utf-8')
assert re.search(r'^\s*#define\s+PRINTJOB_TIMER_AUTOSTART\b', config, re.M)
advanced = (source / 'Configuration_adv.h').read_text(encoding='utf-8')
assert not re.search(r'^\s*#define\s+DOUBLECLICK_FOR_Z_BABYSTEPPING\b', advanced, re.M)
print('Source contract: independent resets, last partial interval, bounded subtraction, job timer and no hidden height shortcut. No C++ compiled or executed.')
