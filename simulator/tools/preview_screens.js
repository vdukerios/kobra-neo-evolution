'use strict';
// Export the real simulator renderer offscreen. Not browser or hardware proof.
const fs=require('fs'),path=require('path'),vm=require('vm');
if(!process.argv[2])throw new Error('Pass a local @napi-rs/canvas module path.');
const {createCanvas,GlobalFonts}=require(path.resolve(process.argv[2]));
for(const file of ['cour.ttf','courbd.ttf']){
  const font=path.join(process.env.WINDIR||'C:/Windows','Fonts',file);
  if(fs.existsSync(font))GlobalFonts.registerFromPath(font,'Courier New');
}
const root=path.resolve(__dirname,'../..'),folder=path.join(root,'docs/images');
const html=fs.readFileSync(path.join(root,'simulator/simulator.html'),'utf8'),env={};vm.createContext(env);
for(const id of ['menu-data','model','render','ui'])vm.runInContext(html.match(new RegExp('<script id="'+id+'">([\\s\\S]*?)</script>'))[1],env);
const frames=[];
for(const [name,lang,screen,state] of [
  ['sim_home_es.png','es','home','idle'],
  ['sim_main_menu_en.png','en','menu_main','idle'],
  ['sim_pause_menu_en.png','en','menu_main','paused'],
  ['sim_maintenance_es.png','es','menu_mantenimiento','idle'],
  ['sim_confirm_print_es.png','es','confirm','idle'],
]){
  const s=env.Model.createSim();s.tick(3);s.S.lang=lang;s.setPrinterState(state);s.debugGo(screen);s.S.toast=null;
  const native=createCanvas(320,240);env.Render.draw(native.getContext('2d'),s);
  const zoom=createCanvas(640,480),c=zoom.getContext('2d');c.imageSmoothingEnabled=false;c.drawImage(native,0,0,640,480);
  fs.writeFileSync(path.join(folder,name),zoom.toBuffer('image/png'));frames.push(name);
}
console.log(JSON.stringify({scope:'Offscreen simulator exports; not browser or physical TFT screenshots',frames}));
