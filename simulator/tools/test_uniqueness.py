"""Ensure the menu guard rejects regressions, even under renamed labels."""
import copy
import json
from pathlib import Path
import re
import runpy

here = Path(__file__).resolve().parent
guard = runpy.run_path(str(here / 'build_kne_candidate.py'))['verify_unique_routes']
html = (here.parent / 'simulator.html').read_text(encoding='utf-8')
screens = json.loads(re.search(r'var MENU_DATA = (.*?);\n', html)[1])['screens']
guard(screens)
cases = [
    ('maintenance', 'menu_info', dict(k='sub', en='Other name', to='menu_mantenimiento')),
    ('save', '_menu_configuration_preheat_settings', dict(k='act', en='Confirm everything', a='store')),
    ('home', 'menu_xyz_home', dict(k='act', en='Reset position', a='gcode:home')),
    ('cooldown', 'menu_kne_manual_heat', dict(k='act', en='Stop heat', a='cooldown')),
    ('extruder', 'menu_advanced_steps_per_mm', dict(k='edit', en='Filament factor', v='&planner.settings.axis_steps_per_mm[E_AXIS]')),
    ('tuning', 'menu_advanced_temperature', dict(k='edit', en='Other process', v='&autotune_temp_bed')),
    ('same_label_same_screen', 'menu_kne_saved_settings', dict(k='act', en='Save current settings', a='store')),
    ('indexed_extruder_alias', 'menu_advanced_steps_per_mm', dict(k='edit', en='Other extrusion control', v='&planner.settings.axis_steps_per_mm[E_AXIS_N(active_extruder)]')),
    ('height_alias', 'menu_probe_offsets', dict(k='edit', en='Other height control', v='&probe.offset.z')),
    ('leveling_alias', 'menu_kne_service', dict(k='sub', en='Other measuring process', to='levelcenter')),
]
for name, sid, row in cases:
    candidate = copy.deepcopy(screens)
    candidate[sid]['items'].append(row)
    try:
        guard(candidate)
    except ValueError as error:
        assert 'Duplicate' in str(error), error
    else:
        raise AssertionError('A duplicate escaped the guard: ' + name)

# Exclusive printing/idle access is allowed; the same idle access is rejected.
candidate = copy.deepcopy(screens)
candidate['menu_main']['items'].append(dict(k='act', en='Print-only cooling', a='cooldown', c=['busy']))
guard(candidate)
candidate['menu_main']['items'][-1]['c'] = ['idle']
try:
    guard(candidate)
except ValueError:
    pass
else:
    raise AssertionError('Idle duplicate escaped the state-aware guard')
print('Uniqueness: current routes pass; 11 intentional regressions rejected, including aliases and identical labels.')
