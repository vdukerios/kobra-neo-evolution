// Smoke test: script syntax, boot, demo print to the end, pause/resume from the menu. Usage: node test_smoke.js "<simulator.html>"
const fs = require('fs');
const html = fs.readFileSync(process.argv[2], 'utf8');
const scripts = [...html.matchAll(/<script[^>]*>([\s\S]*?)<\/script>/g)].map(m => m[1]);
scripts.forEach((s, i) => { try { new Function(s); } catch (e) { console.log('SYNTAX ERROR in block', i, e.message); process.exit(1); } });
console.log('syntax OK,', scripts.length, 'blocks');
const md = html.match(/<script id="menu-data">([\s\S]*?)<\/script>/)[1];
const model = html.match(/<script id="model">([\s\S]*?)<\/script>/)[1];
const Model = new Function(md + model + '; return Model;')();
let bad = 0; const ok = (c, m) => { if (!c) { bad++; console.log('FAIL', m); } else console.log('ok  ', m); };
const sim = Model.createSim();
for (let i = 0; i < 40; i++) sim.tick(0.25);
ok(sim.S.screen === 'home', 'boot goes to the status screen (no language screen), got ' + sim.S.screen);
console.log(sim.simDemoPrint());
ok(sim.S.screen === 'print' && sim.S.job.phase === 'printing', 'demo print running');
sim.click(); ok(sim.S.screen === 'menu_main', 'click on the print screen opens the main menu');
sim.turn(1); sim.click(); // Pause Print is the second item while printing
ok(sim.S.job.paused && sim.S.screen === 'print', 'Pause Print from the menu');
sim.click(); ok(sim.S.screen === 'menu_main' && sim.menuDef().items.some(i => i.t === 'Resume Print'), 'Resume Print appears while paused');
sim.setPrinterState('printing');
sim.turn(1); sim.setSpeed(600); for (let i = 0; i < 700; i++) sim.tick(1);
ok(!sim.S.job || sim.S.job.phase === 'done' || sim.S.screen === 'done', 'print finishes, screen ' + sim.S.screen);
process.exit(bad ? 1 : 0);
