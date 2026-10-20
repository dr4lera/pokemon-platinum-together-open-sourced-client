// SPDX-License-Identifier: GPL-3.0-or-later
import {resolve,join,sep,dirname} from 'node:path';
import {pathToFileURL,fileURLToPath} from 'node:url';
import {mkdirSync,existsSync} from 'node:fs';
import {spawn} from 'node:child_process';
const [serverRoot,fixtureRoot,executable]=process.argv.slice(2);
if(!serverRoot||!fixtureRoot||!executable)throw new Error('Expected real backend source, fresh private fixture directory, native executable');
const checkout=resolve(dirname(fileURLToPath(import.meta.url)),'..').toLowerCase(),target=resolve(fixtureRoot).toLowerCase();
if(target===checkout||target.startsWith(checkout+sep)||existsSync(fixtureRoot))throw new Error('Fixture must be a fresh private directory outside the source checkout');
mkdirSync(fixtureRoot,{recursive:false});
const {Hub}=await import(pathToFileURL(join(resolve(serverRoot),'hub.mjs')));
const hub=new Hub({dir:join(fixtureRoot,'server'),port:0,host:'127.0.0.1',log:false});
let conflict=false,art=false,pose=false,reconnect=false,invalid=false,absent=false,leaves=0;
const timers=[];
const lobby=hub.lobby.bind(hub);
hub.lobby=(c,m)=>{
 if(m.t==='chat.send'&&m.text==='fixture profile conflict'){
  const old=hub.store.trainer(c.player);hub.store.putTrainer(c.player,{...old,name:'OtherHost',look:23,outfit:400});conflict=true;
 }
 if(m.t==='chat.send'&&m.text==='fixture remove trainer'){hub.store.db.prepare('DELETE FROM trainers WHERE player=?').run(c.player);absent=true;}
 if(m.t==='chat.send'&&m.text==='fixture selected art and pose'){
  // Insert an owned synthetic reference, then use the actual ownership/selection
  // handler. This fixture does not install or render a proprietary art pack.
  const ref={creation:'fixture-art',version:'fixture-v1',template:'platinum-trainer-209-v1',hash:'a'.repeat(64)};
  hub.store.db.prepare('INSERT INTO trainer_art_entries(player,creation,version,template,hash,title,creator,label,bytes,at_ms) VALUES(?,?,?,?,?,?,?,?,?,?)').run(c.player,ref.creation,ref.version,ref.template,ref.hash,'Synthetic','Fixture','Test',0,Date.now());
  hub.trainerArt.handle(c,{t:'trainer.art',art:ref});art=true;
  // Actual native city route rewrites the sender's authenticated slot. The
  // adapter must tolerate this envelope without interpreting its eight bytes.
  hub.binary(c,Buffer.from([2,250,251,255,254,253,252,251,250,249,248]));pose=true;
 }
 const result=lobby(c,m);
 if(m.t==='city.leave')leaves++;
 if(m.t==='chat.send'&&m.text==='fixture reconnect city'){reconnect=true;timers.push(setTimeout(()=>c.ws.close(1001,'fixture reconnect'),25));}
 if(m.t==='chat.send'&&m.text==='fixture invalid appearance'){
  invalid=true;const peer=[...hub.connections].find(p=>p!==c&&p.kind==='lobby'&&p.authenticated&&p.city);
  hub.send(c,{t:'city.add',...hub.social.cityPlayer(peer),look:256});
 }
 return result;
};
// Delay only the terminal ack; a canceled enter's joined event still arrives
// first, exactly as the real backend sends it. Reentry must wait for this ack.
const send=hub.send.bind(hub);hub.send=(c,m)=>{if(m.t==='city.left')timers.push(setTimeout(()=>send(c,m),60));else send(c,m);};
// Fragment the backend's actual city envelope on authenticated connections.
const binary=hub.binary.bind(hub);hub.binary=(c,data)=>{
 const peers=hub.social.cityPlayers(c.city).filter(p=>p!==c);
 const originals=peers.map(p=>[p,p.ws.send]);
 try{for(const [p,original]of originals)p.ws.send=function(bytes,options,callback){if(Buffer.isBuffer(bytes)&&options?.binary){original.call(this,bytes.subarray(0,5),{binary:true,fin:false});original.call(this,bytes.subarray(5),{binary:true,fin:true},callback);}else original.call(this,bytes,options,callback);};binary(c,data);}
 finally{for(const [p,original]of originals)p.ws.send=original;}
};
try{
 await hub.start();
 const output=await new Promise((accept,reject)=>{
  const child=spawn(resolve(executable),[hub.url,hub.identity.pin,join(fixtureRoot,'profiles')],{stdio:['ignore','pipe','pipe'],windowsHide:true});let text='';
  child.stdout.on('data',b=>text+=b);child.stderr.on('data',b=>text+=b);
  const timer=setTimeout(()=>{child.kill();reject(new Error('Native presentation fixture timeout'));},90000);
  child.once('error',e=>{clearTimeout(timer);reject(e);});child.once('exit',code=>{clearTimeout(timer);code===0?accept(text):reject(new Error(`Native presentation failed (${code}): ${text}`));});
 });
 if(!conflict||!art||!pose||!reconnect||!invalid||!absent||leaves<2)throw new Error('A required real backend path was not exercised');
 console.log(output.trim());
}finally{for(const timer of timers)clearTimeout(timer);await hub.close();}
