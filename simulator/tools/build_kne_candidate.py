"""Build the local, uncompiled KNE menu candidate without running a compiler.

The preserved inventory supplies unchanged menus. Reviewed source-specific
corrections supply the explicit candidate below; provenance records both.
Only simulator.html and menu_data.js in this simulator folder may be written.
"""
import argparse
import copy
import hashlib
from itertools import product
import json
from pathlib import Path
import re
import runpy
import subprocess

SIM = Path(__file__).resolve().parent.parent
ROOT = SIM.parent
SOURCE = ROOT / 'firmware/source/Marlin'


def item(kind, en, es, **kw):
    return dict(k=kind, en=en, es=es, **kw)


def back():
    return item('back', 'Back', 'Atras')


def sub(en, es, to, **kw):
    return item('sub', en, es, to=to, **kw)


def edit(en, es, var, minimum, maximum, default, decimals=0, unit=None, **kw):
    value = item('edit', en, es, v=var, min=minimum, max=maximum,
                 df=default, d=decimals, **kw)
    if unit:
        value['u'] = unit
    return value


def verify_unique_routes(screens):
    """Reject simultaneous duplicate destinations, actions or edited settings.

    Back and information rows are navigation/text. A recipe is a saved target,
    distinct from changing the current heater/fan. Printing and idle controls
    are checked separately because Configuration is unavailable during a job.
    Guided screens have explicit semantic actions, checked alongside data rows.
    """
    dynamic = {'cal_extrusion', 'cal_temperature', 'zoffset', 'tramming', 'levelcenter', 'about'}
    dynamic.update('guide:' + str(n) for n in range(7))
    dynamic.update('move:' + n for n in ['x', 'y', 'z'])
    dynamic.update('dist:' + n for n in ['10', '1', '0.1', 'z'])
    dynamic.update(['filop:load', 'filop:unload'])
    for sid, screen in screens.items():
        for row in screen['items']:
            target = row.get('to')
            if target and target not in screens and target not in dynamic:
                raise ValueError('Unknown menu destination: ' + sid + ' -> ' + target)

    def semantic(sid, row):
        if row.get('k') in ('back', 'info'):
            return None
        if row.get('to'):
            if row['to'] == 'levelcenter':
                return 'action:level_center'
            return 'screen:' + row['to']
        if row.get('a'):
            return 'action:' + row['a']
        var = re.sub(r'\s+', '', row.get('v', ''))
        var = re.sub(r'E_AXIS_N\([^)]*\)', 'E_AXIS', var)
        for before, after in [('_AXIS(A)', 'X_AXIS'), ('_AXIS(B)', 'Y_AXIS'), ('_AXIS(C)', 'Z_AXIS')]:
            var = var.replace(before, after)
        if not var:
            return None
        if 'autotune_temp' in var:
            return 'tune:' + ('bed' if 'bed' in var else 'hotend')
        if var == '&probe.offset.z':
            return 'screen:zoffset'
        if var == '&editable.uint8':
            return 'value:recipe.fan' if sid == '_menu_configuration_preheat_settings' else 'value:fan.target'
        if var in ('&editable.celsius', '&thermalManager.temp_hotend[0].target'):
            return 'value:hotend.target'
        return 'value:' + var

    special = {
        'cal_extrusion': ['value:&planner.settings.axis_steps_per_mm[E_AXIS]', 'test:extrusion'],
        'cal_temperature': ['tune:hotend', 'tune:bed'],
    }
    flags = ['mesh', 'fan_auto', 'volumetric', 'card', 'reglines', 'subdir', 'repeat']
    checked = 0
    for state in ['idle', 'printing', 'paused']:
        for values in product([False, True], repeat=len(flags)):
            conditions = dict(zip(flags, values))
            conditions.update(idle=state == 'idle', busy=state != 'idle',
                              paused=state == 'paused', notpaused=state != 'paused',
                              nocard=not conditions['card'], nolog=not conditions['reglines'],
                              cardopen=conditions['card'] and state != 'idle', axis=True,
                              axisZ=True, isdir=True)
            for root in ['menu_main', 'menu_job_recovery']:
                seen, actions, todo = set(), {}, [(root, root)]

                def record(key, path):
                    if key in actions and actions[key] != path:
                        raise ValueError('Duplicate in %s (%s): %s: %s / %s' %
                                         (root, state, key, actions[key], path))
                    actions[key] = path

                while todo:
                    sid, path = todo.pop()
                    if sid in seen:
                        continue
                    seen.add(sid)
                    if sid in special:
                        for key in special[sid]:
                            record(key, path)
                    if sid not in screens:
                        continue
                    for i, row in enumerate(screens[sid]['items']):
                        if not all(conditions[c] for c in row.get('c', [])):
                            continue
                        row_path = path + '/' + str(i) + ':' + row.get('en', row.get('x', '?'))
                        key = semantic(sid, row)
                        if key:
                            record(key, row_path)
                        target = row.get('to')
                        if row.get('a', '').startswith('goto:'):
                            target = row['a'][5:]
                        if target:
                            todo.append((target, row_path))
                checked += 1
    return dict(duplicate_routes=0, scenarios=checked,
                scope='Standard menus by printer state; explicit user-approved home shortcuts are additional entries')


def verify_source_routes():
    """Check reviewed menu boundaries directly; no preprocessor or compiler."""
    source = {name: (SOURCE / 'src/lcd/menu' / name).read_bytes().decode('latin1')
              for name in ['menu_main.cpp', 'menu_info.cpp', 'menu_configuration.cpp',
                           'menu_advanced.cpp', 'menu_motion.cpp']}

    def section(name, start, end):
        text = source[name]
        a = text.index(start)
        return text[a:text.index(end, a + len(start))]

    checks = {
        'no_hidden_height_double_click': not re.search(r'^\s*#define\s+DOUBLECLICK_FOR_Z_BABYSTEPPING\b', (SOURCE / 'Configuration_adv.h').read_text(encoding='utf-8'), re.M),
        'maintenance_main': len(re.findall(r'^\s*SUBMENU\(MSG_KNE_MAINTENANCE,', source['menu_main.cpp'], re.M)) == 1,
        'information_single_route': not re.search(r'^\s*SUBMENU\((?:MSG_ABOUT|MSG_KNE_MAINTENANCE),', source['menu_info.cpp'], re.M),
        'recipe_without_save': 'ACTION_ITEM(MSG_STORE_EEPROM' not in section('menu_configuration.cpp', 'void _menu_configuration_preheat_settings()', '    END_MENU();'),
        'calibration_without_save_shortcuts': 'MSG_STORE_EEPROM' not in section('menu_advanced.cpp', 'void kne_menu_calibrar_extrusor() {', 'void menu_advanced_settings()'),
        'extruder_steps_only_in_calibration': 'MSG_E_STEPS' not in section('menu_advanced.cpp', 'void menu_advanced_steps_per_mm()', '  END_MENU();') and 'MSG_EN_STEPS' not in section('menu_advanced.cpp', 'void menu_advanced_steps_per_mm()', '  END_MENU();'),
        'automatic_tuning_only_in_calibration': 'MSG_PID_AUTOTUNE_E' not in source['menu_advanced.cpp'],
        'cooldown_only_in_prepare': 'MSG_COOLDOWN' not in section('menu_main.cpp', 'static void menu_kne_manual_heat()', 'void menu_kne_filament_material();') and 'MSG_COOLDOWN' not in section('menu_advanced.cpp', 'void kne_menu_calibrar_extrusor() {', 'void menu_advanced_settings()') and 'ACTION_ITEM(MSG_COOLDOWN, lcd_cooldown)' in section('menu_main.cpp', 'void menu_ready() {', '  END_MENU();'),
        'home_all_only_in_move': 'MSG_AUTO_HOME,' not in section('menu_motion.cpp', 'void menu_xyz_home()', 'void menu_motion()'),
    }
    failed = [name for name, ok in checks.items() if not ok]
    if failed:
        raise ValueError('Source menu uniqueness check failed: ' + ', '.join(failed))
    return checks


def source_context_help(inventory):
    """Only offer wheel-hold help for labels present in the real lookup table."""
    table = (SOURCE / 'src/lcd/menu/menu_guia_textos.h').read_text(encoding='utf-8')
    pattern = r'\bX\((MSG_\w+),\s*("(?:\\.|[^"\\])*")\s*,\s*\\?\s*("(?:\\.|[^"\\])*")\s*\)'
    messages = {tag: [json.loads(en), json.loads(es)] for tag, en, es in re.findall(pattern, table)}
    labels = {}
    for screen in inventory['pantallas'].values():
        for row in screen.get('elementos', []):
            tag = row.get('etiqueta', {}).get('clave')
            if tag in messages:
                for lang in ['en', 'es']:
                    value = row['etiqueta'].get(lang)
                    if value:
                        labels[value.strip()] = messages[tag]
    for lang in ['en', 'es']:
        text = (SOURCE / ('src/lcd/language/language_' + lang + '.h')).read_text(encoding='utf-8', errors='replace')
        for tag, message in messages.items():
            match = re.search(r'\b' + tag + r'\s*=\s*_UxGT\(("(?:\\.|[^"\\])*")\)', text)
            if match:
                labels[json.loads(match[1]).strip()] = message
    if not messages or 'Back' not in labels or 'Atras' not in labels:
        raise ValueError('Cannot establish the source contextual help contract.')
    return labels


def build_candidate(inventory):
    source_uniqueness = verify_source_routes()
    legacy = runpy.run_path(str(SIM / 'tools/gen_menu_data.py'), run_name='kne_base')
    legacy['build'].__globals__['APPLY_PENDING_RESTRUCTURE'] = False
    screens, old_notes, _, unknown = legacy['build'](inventory)
    screens = copy.deepcopy(screens)
    config = (SOURCE / 'Configuration.h').read_text(encoding='utf-8')
    advanced = (SOURCE / 'Configuration_adv.h').read_text(encoding='utf-8')

    def number(name, fallback):
        m = re.search(r'^\s*#define\s+' + re.escape(name) + r'\s+(-?\d+(?:\.\d+)?)',
                      config + '\n' + advanced, re.M)
        return float(m[1]) if m else fallback

    def array(name, fallback):
        m = re.search(r'^\s*#define\s+' + name + r'\s+\{([^}]+)\}', config, re.M)
        return [float(x) for x in re.findall(r'-?\d+(?:\.\d+)?', m[1])] if m else fallback

    def screen(sid, en, es, rows, **kw):
        screens[sid] = dict(id=sid, t=[en, es], items=rows, **kw)

    original_config = screens['menu_configuration']['items']
    material = next(copy.deepcopy(x) for x in original_config
                    if x.get('to') == '_menu_configuration_preheat_settings')
    material.update(en='Edit $ recipe', es='Editar receta $', lp=1)
    screen('menu_configuration', 'Configuration', 'Configuracion', [back(),
        sub('Calibration', 'Calibracion', 'menu_kne_calibration'),
        sub('Materials', 'Materiales', 'menu_kne_materials'),
        sub('Motor mode', 'Modo de motores', 'menu_modos_motor'),
        sub('Preferences', 'Preferencias', 'menu_kne_preferences'),
        sub('Saved settings', 'Ajustes guardados', 'menu_kne_saved_settings'),
        sub('Information', 'Informacion', 'menu_info'),
        sub('Service', 'Servicio', 'menu_kne_service')])
    screen('menu_kne_calibration', 'Calibration', 'Calibracion', [back(),
        sub('Z offset', 'Z offset', 'zoffset'),
        sub('Calibrate extrusion', 'Calibrar extrusor', 'cal_extrusion'),
        sub('Calibrate temperature', 'Calibrar temperatura', 'cal_temperature')])
    screen('menu_kne_materials', 'Materials', 'Materiales', [back(), material])
    screen('menu_kne_preferences', 'Preferences', 'Preferencias', [back(),
        sub('Language', 'Idioma', 'menu_language'),
        item('tog', 'Sound', 'Sonido', v='&ui.buzzer_enabled')])
    screen('menu_kne_saved_settings', 'Saved settings', 'Ajustes guardados', [back(),
        item('act', 'Save current settings', 'Guardar ajustes', a='store'),
        item('act', 'Load saved settings', 'Recuperar guardados', a='load', c=['idle']),
        item('conf', 'Restore factory values', 'Restaurar iniciales', a='restore', c=['idle'],
             bt=[['Restore', 'Restaurar'], ['Cancel', 'Cancelar']],
             pr=[['Erase calibration and saved settings', 'Borra calibracion y ajustes guardados']])])
    screen('menu_kne_service', 'Service', 'Servicio', [back(),
        item('info', 'Service: verify setup', 'Servicio: revisar montaje'),
        sub('Manual adjustments', 'Ajustes manuales', 'menu_advanced_settings'),
        sub('Board cooling', 'Refrigeracion placa', 'menu_controller_fan'),
        item('tog', 'Filament sensor (if fitted)', 'Sensor segun montaje', v='&runout.enabled'),
        item('tog', 'Power-loss recovery', 'Recuperar tras corte', v='&recovery.enabled'),
        sub('Edit bed mesh', 'Editar malla', 'menu_edit_mesh', c=['mesh']),
        sub('Inspect bed corners', 'Inspeccionar esquinas', 'tramming'),
        sub('Service information', 'Datos de servicio', 'menu_kne_service_data')])
    screens['menu_advanced_settings']['t'] = ['Manual adjustments', 'Ajustes manuales']
    screens['menu_advanced_settings']['items'] = [x for x in screens['menu_advanced_settings']['items']
        if x.get('en') != 'Initialize EEPROM']
    screens.pop('menu_bed_leveling', None)
    screens.pop('menu_maintenance', None)
    screens['menu_probe_offsets']['items'] = [x for x in screens['menu_probe_offsets']['items']
        if x.get('v') != '&probe.offset.z']
    for x in screens['menu_probe_offsets']['items']:
        if x.get('v') in ('&probe.offset.x', '&probe.offset.y'):
            x['df'] = 37.5 if x['v'].endswith('.x') else 4.5
            x['d'] = 2

    # Source uses indexed macros; the older extraction lost whole axes.
    axes = ['X', 'Y', 'Z', 'E']
    source_axes = ['_AXIS(A)', '_AXIS(B)', '_AXIS(C)', 'E_AXIS']
    feed = array('DEFAULT_MAX_FEEDRATE', [300, 300, 4, 50])
    feed_max = array('MAX_FEEDRATE_EDIT_VALUES', [600, 600, 10, 50])
    accel = array('DEFAULT_MAX_ACCELERATION', [1000, 1000, 100, 500])
    accel_max = array('MAX_ACCEL_EDIT_VALUES', [6000, 6000, 200, 10000])
    steps = array('DEFAULT_AXIS_STEPS_PER_UNIT', [80, 80, 400, 390])
    screen('menu_advanced_velocity', 'Motion speed limits', 'Limites de velocidad', [back()] + [
        edit('Max ' + a + ' speed', 'Vel. maxima ' + a,
             '&planner.settings.max_feedrate_mm_s[' + source_axes[i] + ']',
             1, feed_max[i], feed[i], unit='mm/s') for i, a in enumerate(axes)] + [
        edit('Minimum print speed', 'Vel. minima impresion', '&planner.settings.min_feedrate_mm_s',
             0, 9999, number('DEFAULT_MINIMUMFEEDRATE', 0), unit='mm/s'),
        edit('Minimum travel speed', 'Vel. minima viaje', '&planner.settings.min_travel_feedrate_mm_s',
             0, 9999, number('DEFAULT_MINTRAVELFEEDRATE', 0), unit='mm/s')])
    screen('menu_advanced_acceleration', 'Acceleration', 'Aceleracion', [back(),
        edit('Print acceleration', 'Aceleracion impresion', '&planner.settings.acceleration',
             25, max(accel[:2]), number('DEFAULT_ACCELERATION', 500), unit='mm/s2', step=25, max_from='xy_acceleration'),
        edit('Retract acceleration', 'Aceleracion retraccion', '&planner.settings.retract_acceleration',
             100, accel[3], number('DEFAULT_RETRACT_ACCELERATION', 500), unit='mm/s2', max_from='e_acceleration'),
        edit('Travel acceleration', 'Aceleracion viaje', '&planner.settings.travel_acceleration',
             25, max(accel[:2]), number('DEFAULT_TRAVEL_ACCELERATION', 1000), unit='mm/s2', step=25, max_from='xy_acceleration')] + [
        edit('Max ' + a + ' acceleration', 'Acel. maxima ' + a,
             '&planner.settings.max_acceleration_mm_per_s2[' + source_axes[i] + ']',
             10 if a == 'Z' else 100, accel_max[i], accel[i], unit='mm/s2', step=25) for i, a in enumerate(axes)])
    screen('menu_advanced_steps_per_mm', 'Steps per mm', 'Pasos por mm', [back()] + [
        edit(a + ' steps', 'Pasos ' + a, '&planner.settings.axis_steps_per_mm[' + source_axes[i] + ']',
             5, 9999, steps[i], decimals=1, unit='steps/mm') for i, a in enumerate(axes[:3])])
    currents = [number(a + '_CURRENT', 800) for a in axes[:3]] + [number('E0_CURRENT', 600)]
    screen('menu_tmc_current', 'Motor current', 'Corriente de motores', [back()] + [
        edit(a, a, '&stepper' + ('E0' if a == 'E' else a) + '.val_mA',
             400, 1200, currents[i], unit='mA') for i, a in enumerate(axes)])
    thresholds = [number(a + '_HYBRID_THRESHOLD', 255 if i < 2 else 3)
                  for i, a in enumerate(axes[:3])] + [number('E0_HYBRID_THRESHOLD', 30)]
    screen('menu_tmc_hybrid_thrs', 'Hybrid threshold', 'Cambio de modo', [back()] + [
        edit(a, a, '&stepper' + ('E0' if a == 'E' else a) + '.stored.hybrid_thrs',
             0, 255, thresholds[i], unit='mm/s') for i, a in enumerate(axes)])
    screen('menu_tmc_step_mode', 'Quiet mode by motor', 'Modo silencioso por motor', [back()] + [
        item('tog', a, a, v='&stepper' + ('E0' if a == 'E' else a) + '.stored.stealthChop_enabled')
        for a in axes])
    thermal = [back(), item('tog', 'Automatic temperature', 'Temperatura automatica',
                            v='&planner.autotemp_enabled'),
        edit('Automatic minimum', 'Minimo automatico', '&planner.autotemp_min', 0, 260, 210, unit='C'),
        edit('Automatic maximum', 'Maximo automatico', '&planner.autotemp_max', 0, 260, 250, unit='C'),
        edit('Automatic factor', 'Factor automatico', '&planner.autotemp_factor', 0, 10, .1, decimals=2)]
    for heater, es, prefix in [('hotend', 'Boquilla', 'DEFAULT_'), ('bed', 'Cama', 'DEFAULT_bed')]:
        for letter in ['p', 'i', 'd']:
            var = '&PID_PARAM(K' + letter + ',0)' if heater == 'hotend' else '&thermalManager.temp_bed.pid.K' + letter
            fallback = {'hotend': [18.09, 1.14, 71.47], 'bed': [97.1, 1.41, 1675.16]}[heater]['pid'.index(letter)]
            thermal.append(edit(heater.title() + ' PID ' + letter.upper(), es + ' PID ' + letter.upper(),
                                var, .01 if letter == 'i' else 1, 9990,
                                number(prefix + 'K' + letter, fallback), decimals=2))
    screen('menu_advanced_temperature', 'Temperature service', 'Servicio de temperatura', thermal)
    for x in screens['menu_controller_fan']['items']:
        if x.get('v', '').endswith('.idle_speed'):
            x.update(df=0, u='%', min=0, max=100, pct=1)
        elif x.get('v', '').endswith('.active_speed'):
            x.update(df=100, u='%', min=0, max=100, pct=1)
        elif x.get('v', '').endswith('.duration'):
            x.update(df=60, u='s')
    for x in screens['menu_advanced_filament']['items']:
        var = x.get('v', '')
        if 'filament_size' in var:
            x.update(df=number('DEFAULT_NOMINAL_FILAMENT_DIA', 1.75), d=2, u='mm')
        elif var.endswith('.load_length') or var.endswith('.unload_length'):
            x.update(max=number('EXTRUDE_MAXLENGTH', 300), df=number('FILAMENT_CHANGE_FAST_LOAD_LENGTH' if var.endswith('.load_length') else 'FILAMENT_CHANGE_UNLOAD_LENGTH', 100), u='mm')
    for x in screens['_menu_configuration_preheat_settings']['items']:
        if x.get('v') == '&editable.uint8':
            x['df'] = 0
    screens['_menu_configuration_preheat_settings']['items'] = [x for x in
        screens['_menu_configuration_preheat_settings']['items'] if x.get('a') != 'store']
    screens['_menu_configuration_preheat_settings']['items'].append(
        item('info', 'Save in Saved settings', 'Guarda en Ajustes guardados'))

    # Keep distinct idle/printing conditions; only the idle duplicate moves.
    main = screens['menu_main']['items']
    relocated = {'Bed Leveling', 'Language', 'About', 'About Printer', 'Lubricate axes', 'Check belts'}
    service_rows = [copy.deepcopy(x) for x in main if x.get('en') in ('Lubricate axes', 'Check belts')]
    for x in service_rows:
        x.pop('c', None)
    main[:] = [x for x in main if not ('idle' in x.get('c', []) and x.get('en') in relocated)]
    main[:] = [x for x in main if not (x.get('a') == 'resume' and 'idle' in x.get('c', []))]
    main.append(sub('Maintenance', 'Mantenimiento', 'menu_mantenimiento', c=['idle']))
    screen('menu_mantenimiento', 'Maintenance', 'Mantenimiento', [back()] + service_rows)
    screen('menu_ready', 'Prepare', 'Preparar', [back(),
        item('act', 'Preheat $', 'Precalentar $', a='preheat_both', lp=1),
        sub('Load filament', 'Cargar filamento', 'filop:load'),
        sub('Unload filament', 'Descargar filamento', 'filop:unload'),
        sub('Manual heat', 'Temperaturas', 'menu_kne_manual_heat'),
        item('act', 'Cooldown', 'Enfriar', a='cooldown')])
    screen('menu_kne_manual_heat', 'Manual heat', 'Temperaturas', copy.deepcopy(screens['menu_temperature']['items']))
    screens['menu_kne_manual_heat']['items'] = [x for x in
        screens['menu_kne_manual_heat']['items'] if x.get('a') != 'cooldown']
    screens['menu_xyz_home']['items'] = [x for x in
        screens['menu_xyz_home']['items'] if x.get('a') != 'gcode:home']
    service_info = [copy.deepcopy(x) for x in screens['menu_info']['items']
                    if x.get('to') in ('menu_info_board', 'menu_info_thermistors')]
    screen('menu_kne_service_data', 'Service information', 'Datos de servicio', [back()] + service_info)
    screens['menu_info']['items'] = [x for x in screens['menu_info']['items']
        if x.get('to') not in ('menu_info_board', 'menu_info_thermistors')]
    screens['menu_info']['items'][0] = back()
    screens['menu_info']['items'] = [x for x in screens['menu_info']['items']
        if x.get('to') not in ('about', 'menu_mantenimiento')]
    volume = 'x'.join(str(int(number(n, v))) for n, v in
                      [('X_BED_SIZE', 220), ('Y_BED_SIZE', 220), ('Z_MAX_POS', 250)]) + ' mm'
    screens['menu_info_printer']['items'].insert(3, item('info', volume, volume))
    for row in screens['menu_info_printer']['items']:
        if row.get('en') == 'github.com/vdukerios':
            row['en'] = row['es'] = 'github.com/vdukerios/'
    screen('menu_confirmar_impresion', 'Confirm print', 'Confirmar impresion', [
        item('info', 'Selected file', 'Archivo seleccionado'),
        item('info', 'Print estimate', 'Estimacion de impresion'),
        item('act', 'Mode: Full / Express', 'Modo: Completo / Express', a='print:mode'),
        item('act', 'Print', 'Imprimir', a='print:start'), back()])
    for x in screens['menu_modos_motor']['items']:
        if x.get('a') == 'motor:high':
            x.update(en='Force', es='Fuerza')
    # Fixed product term: language changes must not rename this setting.
    for sc in screens.values():
        for row in sc['items']:
            if row.get('to') == 'zoffset' or row.get('v') == '&probe.offset.z':
                row.update(en='Z offset', es='Z offset')
    notes = [n for n in old_notes if not n.startswith('PENDING OVERLAY')]
    notes += ['Local candidate reviewed against current source; firmware not compiled or tested on the printer.',
              'Calibration processes, thermal response and mechanical actions are simulations; actual printer results require validation.',
              'Service current bounds follow project policy, not electrical certification.']
    if unknown:
        notes.append('Legacy conditional templates retained where relevant: ' + '; '.join(unknown))
    # Follow real submenus, action destinations and documented dynamic edges.
    seen, todo = set(), ['menu_main', 'menu_job_recovery']
    edges = dict(legacy['EXTRA_EDGES'])
    # Filament now always goes through the reviewed material/temperature workflow.
    edges.pop('filop:load', None)
    edges.pop('filop:unload', None)
    while todo:
        sid = todo.pop()
        if sid in seen or sid not in screens:
            continue
        seen.add(sid)
        for x in screens[sid]['items']:
            to = x.get('to')
            if to:
                todo.append(edges.get(to, to))
            if x.get('a', '').startswith('goto:'):
                todo.append(x['a'][5:])
        if sid == 'menu_media':
            todo.extend(['menu_media_filelist', 'menu_confirmar_impresion'])
    screens = {sid: sc for sid, sc in screens.items() if sid in seen}
    uniqueness = verify_unique_routes(screens)
    uniqueness['reviewed_source_checks'] = source_uniqueness
    paths = [SOURCE / 'Configuration.h', SOURCE / 'Configuration_adv.h']
    paths += list((SOURCE / 'src/lcd/menu').glob('menu_*.cpp'))
    paths += [SOURCE / 'src/module/settings.cpp', SOURCE / 'src/module/printcounter.cpp',
              SOURCE / 'src/module/printcounter.h', SOURCE / 'src/inc/valentin/kne_limits.h']
    paths += [SOURCE / 'src/lcd/menu/menu_guia_textos.h', SOURCE / 'src/lcd/menu/menu_guia.cpp',
              SOURCE / 'src/lcd/language/language_en.h', SOURCE / 'src/lcd/language/language_es.h']
    paths += [SOURCE / 'src/lcd/marlinui.cpp', SOURCE / 'src/lcd/tft/ui_320x240.cpp',
              SOURCE / 'src/lcd/tft/ui_320x240.h', SOURCE / 'src/lcd/tft/ui_common.cpp',
              SOURCE / 'src/lcd/tft/tft_color.h']
    hashes = {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths if p.exists()}
    revision = '0bc2e73'  # Original working-firmware base; source_hashes identify the candidate.
    # Do not use publication HEAD: committing generated data would invalidate it.
    meta = dict(screens=len(screens), items=sum(len(sc['items']) for sc in screens.values()),
                home_shortcuts=dict(focus_order=['menu','preheat','load','unload','level_center','zoffset'],
                                    default_focus='menu', default_material='PLA',
                                    allowed_duplicates=['preheat','load','unload','level_center','zoffset'],
                                    preserve_original_menu_entries=True,
                                    authorization='User explicitly requested additional home shortcuts; keep original entries'),
                maintenance=dict(interval_hours=[number('SERVICE_INTERVAL_1', 100), number('SERVICE_INTERVAL_2', 200)],
                                 clock='Print job, including preparation; excludes pause and idle',
                                 save_interval_seconds=3600),
                context_help=source_context_help(inventory),
                active_items=sum(len(sc['items']) for sc in screens.values()), overlay=False,
                unreachable=[], inventory_screens=len(inventory['pantallas']), uniqueness=uniqueness,
                provenance=dict(state='local_source_candidate_not_compiled', firmware_commit=revision,
                                source_hashes=hashes, source_dirty=True,
                                inventory_sha256=hashlib.sha256(json.dumps(inventory, sort_keys=True).encode()).hexdigest(),
                                generator='build_kne_candidate.py', inventory_regenerated=False))
    return dict(meta=meta, screens=screens, notes=notes)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inventory', nargs='?', type=Path, default=SIM / 'tools/inventory_base.json')
    parser.add_argument('html', nargs='?', type=Path, default=SIM / 'simulator.html')
    parser.add_argument('--check', action='store_true', help='Validate and build in memory only')
    args = parser.parse_args()
    html_path = args.html.resolve()
    if html_path.parent != SIM or html_path.name != 'simulator.html':
        parser.error('Only the local candidate simulator.html is an allowed output.')
    html = html_path.read_text(encoding='utf-8')
    pattern = re.compile(r'<script id="menu-data">.*?</script>', re.S)
    if len(pattern.findall(html)) != 1:
        parser.error('Exactly one menu-data block is required before any output is written.')
    raw_inventory = args.inventory.read_bytes()
    data = build_candidate(json.loads(raw_inventory.decode('utf-8')))
    data['meta']['provenance']['inventory_sha256'] = hashlib.sha256(raw_inventory).hexdigest()
    if args.check:
        current = re.search(r'var MENU_DATA = (.*?);\n', html)
        if not current or json.loads(current[1]) != data:
            parser.error('The local simulator menu data is stale. Regenerate the local candidate first.')
    js = 'var MENU_DATA = ' + json.dumps(data, ensure_ascii=False, separators=(',', ':')) + ';\n'
    block = '<script id="menu-data">\n/* LOCAL CANDIDATE: SOURCE NOT COMPILED */\n' + js + '</script>'
    candidate = pattern.sub(lambda _: block, html)
    if not args.check:
        (SIM / 'menu_data.js').write_bytes(('/* KNE source candidate; do not edit generated data. */\n' + js).encode('utf-8'))
        html_path.write_bytes(candidate.encode('utf-8'))
    print(('Checked' if args.check else 'Built') + ': %d screens, %d entries; uncompiled local source candidate.' % (data['meta']['screens'], data['meta']['items']))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
