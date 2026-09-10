import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
import {normalizeProject,validateProject,projectToXml} from '../src/renderer/src/project-schema.js';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const example=path.join(root,'assets/examples/photovoltaic_maize');
const base=JSON.parse(fs.readFileSync(path.join(example,'project.json')));
const output=path.join(root,'tmp/photovoltaic-regression');fs.mkdirSync(output,{recursive:true});
for(const o of base.configuration.objects.items)for(const k of ['fileName','positionFile'])o[k]=path.join(example,o[k]);
base.configuration.meteo.path=path.join(example,'meteo.txt');
const pv=p=>p.configuration.materials.find(m=>m.energyModel==='photovoltaic');
assert.equal(validateProject(base).valid,true);
assert.deepEqual(pv(normalizeProject(base)).params,pv(base).params);
assert.match(projectToXml(base),/<photovoltaic>1<\/photovoltaic>/);
const bad=structuredClone(base);pv(bad).params.eta25=-1;assert.equal(validateProject(bad).valid,false);
const mode=structuredClone(base);mode.mode='eVoxelEB';assert.equal(validateProject(mode).valid,false);
function run(name,modify=()=>{}) {
 const p=structuredClone(base);p.configuration.outDir=path.join(output,name);p.configuration.meteo.start=24;p.configuration.meteo.end=25;modify(p);
 fs.mkdirSync(p.configuration.outDir,{recursive:true});const input=path.join(p.configuration.outDir,'project.json');fs.writeFileSync(input,JSON.stringify(p,null,2));
 const exe=path.join(root,'models/bin_x64/Release/histream.exe');const r=spawnSync(exe,[p.mode,input],{cwd:path.dirname(exe),windowsHide:true,encoding:'utf8'});fs.writeFileSync(path.join(p.configuration.outDir,'run.log'),r.stdout+'\n'+r.stderr);
 if(name==='invalid-mode'||name==='transmissive'){assert.notEqual(r.status,0);return;}
 assert.equal(r.status,0,r.stderr);const data=JSON.parse(fs.readFileSync(path.join(p.configuration.outDir,'faceteb.json')));assert.ok(data.graph.directedEdges>0);assert.equal(data.graph.fragmentOverflow,0);assert.equal(data.graph.hashOverflow,0);
 const csv=fs.readFileSync(path.join(p.configuration.outDir,'photovoltaic_summary.csv'),'utf8').trim().split(/\r?\n/).map(s=>s.split(','));const row=Object.fromEntries(csv[0].map((k,i)=>[k,i===1?csv[1][i]:Number(csv[1][i])]));
 assert.ok(row.max_residual_W_m2<0.2);assert.ok(row.electric_W>=0 && row.electric_W<=row.absorbed_W);assert.ok(Math.abs(row.absorbed_W+row.net_longwave_W-row.sensible_W-row.storage_W-row.electric_W)<row.area_m2*0.2);
 const facets=fs.readFileSync(path.join(p.configuration.outDir,'photovoltaic_node_'+p.configuration.meteo.start+'.csv'),'utf8').trim().split(/\r?\n/).slice(1).map(l=>l.split(',').map(Number));assert.ok(facets.every(v=>v[6]===0));
 assert.equal(data.photovoltaicPower.length,data.facetCount*2);assert.ok(data.photovoltaicPower.slice(data.leafFacetCount*2).every(x=>x===0));
 assert.ok(fs.readdirSync(path.join(p.configuration.outDir,'process')).some(f=>f.includes('_pv_T=')&&f.endsWith('.json')));
 console.log(name,JSON.stringify(row));return row;
}
// Use a fresh output directory: an old process folder would hide this regression.
const disabledName='process-disabled-'+Date.now();
run(disabledName,p=>Object.assign(p.configuration.sensor,{process:false,radiationProcess:false,energyProcess:false}));
const processDir=path.join(output,disabledName,'process');
const processMetadata=fs.readdirSync(processDir).filter(f=>f.endsWith('.json'));
assert.ok(processMetadata.length>0);
for(const file of processMetadata){
 const m=JSON.parse(fs.readFileSync(path.join(processDir,file),'utf8'));
 assert.equal(m.processType,'photovoltaic');
 assert.equal(fs.statSync(path.join(processDir,m.dataFile)).size,m.surfaceCount*m.recordFloats*4);
}
if(process.argv.includes('--process-disabled-only')){
 console.log('PV output with optional processes disabled: passed');
 process.exit(0);
}
const normal=run('normal');const zero=run('zero-efficiency',p=>pv(p).params.eta25=0);assert.equal(zero.electric_W,0);assert.ok(zero.temperature_C>normal.temperature_C);
const bifacial=run('bifacial',p=>pv(p).params.bifaciality=1);assert.ok(bifacial.electric_W>=normal.electric_W);
const night=run('night',p=>{p.configuration.meteo.start=0;p.configuration.meteo.end=1});assert.equal(night.electric_W,0);
const inertia=run('no-inertia',p=>pv(p).params.heatCapacityPerArea=0);assert.equal(inertia.storage_W,0);assert.ok(inertia.temperature_C>normal.temperature_C);
run('invalid-mode',p=>p.mode='eVoxelEB');run('transmissive',p=>p.configuration.spectra.find(s=>s.name==='pv_flat').transmittance='0.1');
const mixed=run('mixed',p=>{
 const rows=fs.readFileSync(p.configuration.objects.items[0].positionFile,'utf8').trim().split(/\r?\n/);
 const o=p.configuration.objects.items[0], plant=structuredClone(o);
 for(const [item,lines,name] of [[o,rows.slice(0,8),'pv'],[plant,rows.slice(8),'plant']]){item.positionFile=path.join(output,name+'.txt');fs.writeFileSync(item.positionFile,lines.join('\n')+'\n');}
 plant.name='Living maize';plant.type='Vegetation';plant.materialName='leaf_c4';plant.meshes.forEach(m=>{m.materialName='leaf_c4';m.spectralName='maize_leaf'});
 p.configuration.objects.items.push(plant);
});assert.ok(Math.abs(mixed.area_m2*2-normal.area_m2)<0.01);
console.log('Photovoltaic regression passed');
