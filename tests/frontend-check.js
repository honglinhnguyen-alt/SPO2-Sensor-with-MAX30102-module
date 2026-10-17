const fs=require('fs'),vm=require('vm'),assert=require('assert');
const html=fs.readFileSync(process.argv[2],'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
const elements=new Map();
const ctx=new Proxy({}, {get:(target,name)=>target[name]||(()=>{}),set:(target,name,value)=>(target[name]=value,true)});
function getElementById(id){if(!elements.has(id))elements.set(id,{textContent:'',className:'',style:{},getBoundingClientRect:()=>({width:390,height:230}),getContext:()=>ctx});return elements.get(id);}
const sandbox={document:{getElementById},window:{devicePixelRatio:2,addEventListener(){}},setTimeout(){return 1;},clearTimeout(){},setInterval(){},fetch:()=>new Promise(()=>{}),AbortController,Date,console};
vm.createContext(sandbox);vm.runInContext(script,sandbox);
function update(overrides={}){sandbox.data={uptime_ms:10000,sequence:1,status:'ok',message:'Live',bpm:75,spo2:97,chip_temperature_c:25.5,ir:60000,finger_present:true,samples_collected:75,sample_target:100,...overrides};vm.runInContext('update(data)',sandbox);}
update();assert.equal(getElementById('bpm').textContent,'75');assert.equal(getElementById('spo2').textContent,'97');assert.equal(getElementById('temp').textContent,'25.50');
assert.equal(vm.runInContext('history.length',sandbox),1);
update();assert.equal(vm.runInContext('history.length',sandbox),1); // Same calculation is not plotted twice.
update({sequence:2,spo2:98});assert.equal(vm.runInContext('history.length',sandbox),2);
update({status:'no_finger',bpm:null,spo2:null,finger_present:false});assert.equal(getElementById('spo2').textContent,'--');assert.equal(getElementById('contact').textContent,'Absent');
assert.equal(vm.runInContext('history[history.length-1].v',sandbox),null);
vm.runInContext('disconnected()',sandbox);for(const id of ['bpm','spo2','temp','ir','contact'])assert.equal(getElementById(id).textContent,'--');
assert.equal(getElementById('connection').textContent,'Disconnected');
update({uptime_ms:50,sequence:0,status:'collecting',bpm:null,spo2:null});assert.equal(vm.runInContext('history.length',sandbox),1); // Board reboot clears old graph.
update({uptime_ms:5000,sequence:1});assert.equal(getElementById('connection').textContent,'Connected');
assert.equal(getElementById('spo2').textContent,'97');
assert(!/<(?:script|link)[^>]+(?:src|href)="https?:/i.test(html));
console.log('PASS: dashboard JavaScript syntax, readings, duplicate suppression, removal gaps, connection loss, reconnection, board restart and offline assets.');
