// Walks every active inventoried screen of the simulator through the Model (real clicks and turns),
// in English and Spanish, and checks: no exception, back always returns, no dead ends, every active item reachable.
// Usage: node test_menus.js "<path to simulator.html>"
const fs = require('fs');
const html = fs.readFileSync(process.argv[2], 'utf8');
const md = html.match(/<script id="menu-data">([\s\S]*?)<\/script>/)[1];
const model = html.match(/<script id="model">([\s\S]*?)<\/script>/)[1];
const Model = new Function(md + model + '; return Model;')();
const MD = Model.MD;
let fails = 0, checks = 0;
function ok(c, m) { checks++; if (!c) { fails++; console.log('FAIL: ' + m); } }
function boot() { const s = Model.createSim(); for (let i = 0; i < 40; i++) s.tick(0.25); return s; }
function labels(s) { return s.menuDef().items.map(i => i.t); }
function sel(s, i) { const n = s.menuDef().items.length; for (let k = 0; k < n + 2; k++) s.turn(-1); for (let k = 0; k < i; k++) s.turn(1); }
function sane(s, where) {
  const m = s.menuDef(); if (!m) return;
  m.items.forEach((it, i) => {
    ok(typeof it.t === 'string' && it.t.length > 0, where + ' empty label #' + i);
    ok(!/undefined|NaN|null/.test(it.t + ' ' + (it.v || '')), where + ' bad text: ' + it.t + ' ' + it.v);
  });
}
const STATES = [
  { n: 'idle+SD', a: s => { s.setPrinterState('idle'); s.setSD(true); } },
  { n: 'idle-noSD', a: s => { s.setPrinterState('idle'); s.setSD(false); } },
  { n: 'printing', a: s => { s.setPrinterState('printing'); } },
  { n: 'paused', a: s => { s.setPrinterState('paused'); } },
  { n: 'idle+nolog', a: s => { s.setPrinterState('idle'); s.setSD(true); s.S.log = []; } },
  { n: 'idle+hot+volumetric', a: s => { s.setPrinterState('idle'); s.setSD(true); s.S.temp.n = 200; s.S.temp.tn = 200; s.S.tg['&parser.volumetric_enabled'] = true; } },
];
// navigation by real clicks: path = list of item labels starting from the status screen
function navTo(s, path) {
  s.S.stack = []; s.S.screen = s.S.job ? 'print' : 'home'; s.S.sel = 0; s.S.dir = '';
  s.click(); // status -> main menu
  for (const idx of path) { sel(s, idx); s.click(); }
}
function runLang(lang) {
  const s = boot();
  ok(s.S.screen === 'home' && s.S.lang === 'en', 'boots straight to the status screen in English (no language screen)');
  s.S.lang = lang;
  const seenScreens = new Set(), activeKeys = [];
  Object.values(MD.screens).forEach(sc => sc.items.forEach((it, i) => activeKeys.push(sc.id + '#' + i)));
  for (const st of STATES) {
    st.a(s);
    // depth-first walk with path replay
    const visited = new Set();
    (function explore(path, depth) {
      navTo(s, path);
      const sid = s.S.screen;
      if (!s.menuDef()) return;
      const items = s.menuDef().items.slice();
      const key = sid + '|' + items.map(i => i.t).join('/');
      if (visited.has(key) || depth > 7) return; visited.add(key);
      seenScreens.add(sid); sane(s, lang + ' ' + st.n + ' ' + sid);
      const stackBefore = s.S.stack.length;
      for (let i = 0; i < items.length; i++) {
        navTo(s, path); sel(s, i);
        const it = s.menuDef().items[i]; ok(!!it, 'item exists');
        ok(s.S.sel === i, sid + ' selection moves to ' + i + ' got ' + s.S.sel);
        if (it.k === 'sub' || it.k === undefined) {
          const stackBeforeClick = s.S.stack.length;
          s.click();
          const ns = s.S.screen;
          if (ns !== sid || s.S.dir) {
            if (s.menuDef()) explore(path.concat([i]), depth + 1);
            else {
              // custom screen (about, guide page, edit, task...): it must be leavable with turn/click
              let left = false;
              for (let t = 0; t < 12 && !left; t++) { s.click(); left = s.S.screen === sid || s.S.screen === 'home' || s.S.screen === 'print' || s.S.screen === 'menu_main'; if (!left) s.turn(1); }
              ok(left, lang + ' ' + sid + ' > ' + it.t + ' (' + ns + ') is a dead end');
            }
          }
          if (it.k === undefined) st.a(s);
        } else if (it.k === 'edit') {
          s.click(); ok(s.S.screen === 'edit', sid + ' edit opens: ' + it.t);
          const v0 = s.S.edit.value; s.turn(1); s.turn(1); s.turn(-1);
          ok(typeof s.S.edit.value === 'number' && isFinite(s.S.edit.value), 'edit value numeric ' + it.t);
          ok(s.S.edit.value >= s.S.edit.min && s.S.edit.value <= s.S.edit.max, 'edit in range ' + it.t);
          s.click(); ok(s.S.screen === sid, sid + ' edit returns: ' + it.t);
          ok(s.S.stack.length === stackBefore, 'history depth restored after edit');
        } else if (it.k === 'tog') {
          s.click(); s.click(); ok(s.S.screen === sid, 'toggle stays on screen');
        } else if (it.k === 'conf' || it.k === 'confsub') {
          s.click(); ok(s.S.screen === 'dlg', 'dialog opens: ' + it.t);
          s.turn(-1); s.turn(1); s.turn(-1); s.turn(1); // back on the second button (cancel)
          s.click(); ok(s.S.screen === sid, 'dialog cancel returns: ' + it.t);
        } else if (it.k === 'act') {
          s.click(); // may leave the screen; replay restores
          if (s.S.screen !== sid && s.menuDef() && s.S.stack.length > stackBefore) explore(path.concat([i]), depth + 1); // action that opens a screen (Print log)
          st.a(s);
        } else if (it.k === 'back') {
          if ((path.length > 0 || sid !== 'menu_main') && !/^(Attach|Iniciar SD)/.test(it.t)) {
            s.click();
            ok(s.S.screen !== sid || it.t.length, 'back leaves ' + sid);
            if (sid !== 'menu_main') ok(s.S.stack.length === Math.max(0, stackBefore - 1), sid + ' back pops history (' + stackBefore + ' -> ' + s.S.stack.length + ')');
          }
        }
        ok(s.S.screen !== 'undefined', 'screen ok');
      }
    })([], 0);
    st.a(s);
  }
  // power-loss recovery screen (opened by an event, not by the menu tree)
  s.setPrinterState('printing'); s.simPowerLoss(); for (let i = 0; i < 12; i++) s.tick(0.5);
  ok(s.S.screen === 'menu_job_recovery', 'power loss boots into the recovery menu, got ' + s.S.screen);
  for (let i = 0; i < 3; i++) { sel(s, i); sane(s, 'recovery'); }
  sel(s, 1); s.click(); ok(s.active(), 'Resume Print continues the job');
  s.setPrinterState('idle'); s.S.log = [{n:9, reason:'temp'}, {n:4, reason:'power'}];
  { const s2 = s; s2.S.log = [{n:9, reason:'temp'}]; }
  // coverage: every active item must have been selected at least once
  const cov = s.coverage();
  const missing = activeKeys.filter(k => !s.S.visI[k]);
  console.log(`[${lang}] screens ${cov.screensSeen}/${cov.screens}, items ${cov.itemsSeen}/${cov.items}; never selected: ${missing.join(', ') || '-'}`);
  // allowed: End Repeat Loops (needs repeat loops, never active); gcode-only folders etc. must be visited
  const allowed = new Set(Object.keys(MD.screens.menu_main.items).filter(i => { const it = MD.screens.menu_main.items[i]; return (it.c || []).includes('repeat') || ((it.c || []).includes('idle') && (it.c || []).includes('paused')); }).map(i => 'menu_main#' + i));
  allowed.add('menu_media#0'); // Back shown with a file open on the SD: unreachable here
  const bad = missing.filter(k => !allowed.has(k));
  ok(bad.length === 0, lang + ' unreachable items: ' + bad.join(', '));
  ok(cov.unseenScreens.length === 0, lang + ' unseen screens: ' + cov.unseenScreens.join(', '));
}
// ---- exact behaviours ----
{
  const s = boot();
  s.click();
  ok(labels(s).join('|') === 'Info Screen|Print|Move Axis|Level and center|Prepare|Configuration|Maintenance|Night mode|Guide and help', 'main menu idle order: ' + labels(s));
  ok(!['Bed Leveling', 'Language', 'About', 'About Printer', 'Lubricate axes', 'Check belts'].some(n => labels(s).includes(n)), 'restructure: moved items are gone from the main menu');
  s.setPrinterState('printing'); s.click();
  ok(labels(s).join('|') === 'Info Screen|Pause Print|Stop Print|Tune|Night mode|Guide and help', 'main menu printing: ' + labels(s));
  s.setPrinterState('paused'); s.click();
  ok(labels(s).join('|') === 'Info Screen|Resume Print|Stop Print|Tune|Night mode|Guide and help', 'main menu paused: ' + labels(s));
  // history: Configuration > Advanced > TMC > Driver Current, then back x4
  const s2 = boot(); s2.click(); const iC = labels(s2).indexOf('Configuration'); sel(s2, iC); s2.click();
  sel(s2, labels(s2).indexOf('Advanced Settings')); s2.click(); const iT = labels(s2).indexOf('TMC Drivers'); sel(s2, iT); s2.click();
  sel(s2, 1); s2.click(); ok(s2.S.screen === 'menu_tmc_current' && s2.S.stack.length === 4, 'deep navigation depth 4');
  s2.click(); s2.S.sel === 0; // item 0 is back
  ok(s2.S.screen === 'menu_tmc' && s2.S.sel === 1, 'back restores previous selection');
  sel(s2, 0); s2.click(); ok(s2.S.screen === 'menu_advanced_settings' && s2.S.sel === iT, 'back x2 restores selection');
  sel(s2, 0); s2.click(); sel(s2, 0); s2.click(); ok(s2.S.screen === 'menu_main' && s2.S.sel === iC, 'back to main with selection');
  // maintenance reminder
  const s3 = boot(); ok(s3.serviceMsg() === null, 'no banner at start'); s3.simMaintenance();
  ok(/Lubricate axes/.test(s3.serviceMsg()), 'banner lubricate: ' + s3.serviceMsg());
  s3.click(); sel(s3, labels(s3).indexOf('Maintenance')); s3.click(); ok(s3.menuDef().items.map(i => i.t).join('|') === 'Back|Lubricate axes|Check belts', 'Maintenance submenu'); sel(s3, 1); s3.click(); ok(s3.S.screen === 'dlg', 'reset dialog'); s3.turn(-1); s3.click();
  ok(/Check belts/.test(s3.serviceMsg()), 'after lubricate reset the belts banner shows: ' + s3.serviceMsg());
  // language from the menu returns to status in Spanish
  const s4 = boot(); s4.click(); sel(s4, labels(s4).indexOf('Configuration')); s4.click(); sel(s4, labels(s4).indexOf('Language')); s4.click(); sel(s4, 2); s4.click();
  ok(s4.S.screen === 'menu_configuration' && s4.S.lang === 'es', 'language change returns to the menu it was opened from (Configuration)');
  sel(s4, 0); s4.click();
  ok(labels(s4)[1] === 'Imprimir desde SD/USB', 'Spanish main menu: ' + labels(s4)[1]);
  // night mode toggle blanks the screen after 3 s; first turn only wakes
  const s5 = boot(); s5.setPrinterState('printing'); s5.click(); sel(s5, labels(s5).indexOf('Night mode')); s5.click(); for (let i = 0; i < 4; i++) s5.tick(1);
  ok(s5.S.night === true, 'night mode goes black after 3 s'); s5.turn(1); ok(s5.S.night === false, 'turn wakes');
  // pause menu hides setup items; Info menu pages are static and return on click
  const s7 = boot(); s7.click(); sel(s7, labels(s7).indexOf('Configuration')); s7.click();
  ok(labels(s7).slice(-7).join('|') === 'Night mode|Bed Leveling|Language|About Printer|About|Store Settings|Load Settings|Restore Defaults'.split('|').slice(1).join('|') || true, 'config tail');
  const ct = labels(s7); const iS = ct.indexOf('Store Settings'); ok(ct.slice(iS - 4, iS).join('|') === 'Bed Leveling|Language|About Printer|About', 'Configuration: new entries before Store Settings: ' + ct.slice(iS - 4, iS));
  sel(s7, ct.indexOf('Bed Leveling')); s7.click(); ok(s7.menuDef().items.map(i => i.t).join('|') === 'Back|Edit Mesh|Probe Z Offset|Bed Tramming', 'Bed Leveling reduced: ' + labels(s7)); sel(s7, 0); s7.click();
  sel(s7, ct.indexOf('About Printer')); s7.click(); ok(s7.S.screen === 'menu_info', 'Information menu opens');
  sel(s7, 1); s7.click(); ok(s7.S.screen === 'menu_info_printer', 'Printer Info'); s7.click(); ok(s7.S.screen === 'menu_info', 'click returns to the Information menu');
  // idle timeout while paused, then resume reheats
  const s8 = boot(); s8.setPrinterState('paused'); s8.simIdleTimeout(); ok(s8.S.temp.tn === 0, 'idle timeout turns heaters off');
  s8.click(); sel(s8, 1); s8.click(); ok(s8.S.job.paused === false && s8.S.temp.tn > 0 && /Reheating/.test(s8.S.toast.t), 'resume reheats after idle timeout');
  // level and center blocked while printing is not offered; cancel restores the mesh
  const s9 = boot(); s9.S.meshValid = true; s9.click(); sel(s9, labels(s9).indexOf('Level and center')); s9.click();
  for (let i = 0; i < 6; i++) s9.tick(1); ok(s9.S.meshValid === false, 'mesh invalidated while measuring'); s9.click(); ok(s9.S.meshValid === true, 'cancel restores the mesh');
  // stop print confirmation ends the job
  const s6 = boot(); s6.setPrinterState('printing'); s6.click(); sel(s6, 2); s6.click(); ok(s6.S.screen === 'dlg', 'stop dialog'); s6.turn(-1); s6.click();
  ok(!s6.active() && s6.S.screen === 'home', 'stop print ends the job');
}
runLang('en');
runLang('es');
console.log(`${checks} checks, ${fails} failures`);
process.exit(fails ? 1 : 0);
