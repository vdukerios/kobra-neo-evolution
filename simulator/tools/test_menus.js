'use strict';
// Walk actual menu clicks, including dynamic destinations. No browser or printer.
const fs=require('fs'),vm=require('vm'),assert=require('assert');const path=require('path');
const html=fs.readFileSync(path.join(__dirname,'../simulator.html'),'utf8'),ctx={};vm.createContext(ctx);
for(const id of ['menu-data','model','render','ui'])vm.runInContext(html.match(new RegExp('<script id="'+id+'">([\\s\\S]*?)</script>'))[1],ctx);
const M=ctx.Model,covered=new Set(),scenarios=[];
for(const lang of ['en','es'])for(const state of ['idle','printing','paused'])for(const configured of [false,true]){
  function replay(path,root){
    const s=M.createSim();s.S.lang=lang;s.debugGo('menu_main');s.setPrinterState(state);
    s.S.meshValid=true;s.S.tg['&parser.volumetric_enabled']=configured;s.S.tg['&controllerFan.settings.auto_mode']=configured;
    s.debugGo(root);
    for(const step of path){
      const d=s.menuDef();assert(d,'Missing menu along click path');
      const i=d.items.findIndex(x=>x.key===step.key&&x.t===step.label);assert(i>=0,'Missing option '+step.label);
      s.turn(i-s.S.sel);s.click();assert(!s.S.toast||s.S.toast.t!=='n/a','Unhandled option '+step.label);
    }
    return s;
  }
  let visits=0;
  for(const root of state==='idle'?['menu_main','menu_job_recovery']:['menu_main']){
    const queue=[[]],seen=new Set();
    while(queue.length){
      const path=queue.shift(),s=replay(path,root),d=s.menuDef();
      const stamp=[s.S.screen,s.S.axis,s.S.preIdx,s.S.filOp,s.S.calibration&&s.S.calibration.heater,s.S.dir].join('|');
      if(seen.has(stamp))continue;seen.add(stamp);assert(++visits<200,'Menu cycle or unbounded walk');
      if(s.S.screen==='menu_media')covered.add('menu_media');
      if(d&&d.sid)covered.add(d.sid);if(M.MD.screens[s.S.screen])covered.add(s.S.screen);
      if(!d||d.stat)continue;
      d.items.forEach((row,index)=>{
        const match=/^(.*)#(\d+)$/.exec(row.key||''),data=match&&M.MD.screens[match[1]],raw=data&&data.items[Number(match[2])];
        const dynamic=s.S.screen==='filament_material'&&index>0||s.S.screen==='cal_temperature'&&!s.S.calibration.heater&&index>0;
        const media=d.sid==='menu_media_filelist'&&raw&&['folder','file'].includes(raw.k);
        if(row.k==='sub'||raw&&/^goto:/.test(raw.a||'')||dynamic||media){
          queue.push(path.concat({key:row.key,label:row.t}));
        }
      });
    }
  }
  scenarios.push({lang,state,configured,visits});
}
const missing=Object.keys(M.MD.screens).filter(id=>!covered.has(id));
assert.deepEqual(missing,[],'Unvisited generated screens: '+missing.join(', '));
const out={date:'2026-11-06',scope:'Actual model menu clicks; not browser rendering or compiled firmware',generatedScreens:covered.size,scenarios};
fs.writeFileSync(path.join(__dirname,'../../testing/SIMULATOR_ROUTES.json'),JSON.stringify(out,null,2)+'\n');
console.log('Runtime menu walk: '+covered.size+' generated screens reached; 12 language/state/configuration scenarios; no unhandled destinations.');
