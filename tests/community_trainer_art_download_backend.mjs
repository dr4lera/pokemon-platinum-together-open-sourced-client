// SPDX-License-Identifier: GPL-3.0-or-later
import {resolve,join,sep,dirname} from 'node:path';
import {pathToFileURL,fileURLToPath} from 'node:url';
import {mkdirSync,existsSync,writeFileSync,readFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {spawn,spawnSync} from 'node:child_process';
import {createServer} from 'node:https';
const [backend,fixture,executable]=process.argv.slice(2);
const checkout=resolve(dirname(fileURLToPath(import.meta.url)),'..').toLowerCase(),target=resolve(fixture??'').toLowerCase();
if(!backend||!fixture||!executable||target===checkout||target.startsWith(checkout+sep)||existsSync(fixture))throw new Error('Use real backend and a fresh private fixture outside source');
mkdirSync(fixture,{recursive:false});
const {Hub}=await import(pathToFileURL(join(resolve(backend),'hub.mjs')));
const hub=new Hub({dir:join(fixture,'server'),port:0,host:'127.0.0.1',log:false,communityLinkEnabled:true});
const hash=b=>createHash('sha256').update(b).digest('hex');
function pack(invalid=false){
 const palette=Buffer.from([2,0,0,0,0,0,0,0,23,45,67,255]),field=Buffer.alloc(36*1024,1),front=Buffer.alloc(25600),tiles=Buffer.alloc(26976);
 for(let i=0;i<front.length;i+=4){front[i]=23;front[i+1]=45;front[i+2]=67;front[i+3]=invalid?128:255;}
 const sections=[['PAL ',palette],['FELD',field],['FRNT',front],['TILE',tiles]],header=Buffer.alloc(64+12*sections.length);header.write('PTTR');header.writeUInt16LE(1,4);header[13]=23;header.writeUInt16LE(sections.length,14);header.write('platinum-trainer-209-v1',16);
 let offset=header.length;sections.forEach(([tag,data],i)=>{header.write(tag,64+i*12);header.writeUInt32LE(offset,68+i*12);header.writeUInt32LE(data.length,72+i*12);offset+=data.length;});header.writeUInt32LE(offset,8);return Buffer.concat([header,...sections.map(s=>s[1])]);
}
const valid=pack(),invalid=pack(true);let mode='valid',downloads=0,pings=0,frames=0,redirects=0;const modes=new Set(),timers=new Set();
const lobby=hub.lobby.bind(hub);hub.lobby=(c,m)=>{
 if(m.t==='ping')pings++;
 if(m.t==='chat.send'&&m.text.startsWith('art fixture ')){
  mode=m.text.split(' ')[2];modes.add(mode);
  if(mode==='clear')hub.trainerArt.handle(c,{t:'trainer.art',art:null});
  else{
   const bytes=mode==='invalid'?invalid:valid,art={creation:'synthetic-art',version:mode==='invalid'?'invalid':'valid',template:'platinum-trainer-209-v1',hash:hash(bytes)};
   hub.store.db.prepare('INSERT OR IGNORE INTO trainer_art_entries(player,creation,version,template,hash,title,creator,label,bytes,at_ms) VALUES(?,?,?,?,?,?,?,?,?,?)').run(c.player,art.creation,art.version,art.template,art.hash,'Synthetic','Fixture','Test',bytes.length,Date.now());
   writeFileSync(join(hub.trainerArt.dir,art.hash+'.pttrainer'),bytes);hub.trainerArt.handle(c,{t:'trainer.art',art});
  }
 }
 return lobby(c,m);
};
const binary=hub.communityLink.binary.bind(hub.communityLink);hub.communityLink.binary=(c,data)=>{frames++;return binary(c,data);};
const request=hub.request.bind(hub);hub.request=(req,res)=>{
 if(req.url==='/fixture-redirect-target'){redirects++;res.writeHead(200);res.end(valid);return;}
 if(req.url?.startsWith('/api/creators/v1/packs/')){
  downloads++;
  if(mode==='corrupt'){const b=Buffer.from(valid);b[b.length-1]^=1;res.writeHead(200,{'Content-Length':b.length});res.end(b);return;}
  if(mode==='huge'){res.writeHead(200,{'Content-Length':2097153});res.end(Buffer.alloc(1));return;}
  if(mode==='chunked'){res.writeHead(200);res.end(Buffer.alloc(2097153));return;}
  if(mode==='redirect'){res.writeHead(302,{'Location':hub.url+'/fixture-redirect-target'});res.end();return;}
  if(mode==='stall'){res.writeHead(200,{'Content-Length':valid.length});res.write(valid.subarray(0,16));return;}
  if(mode==='slow'){res.writeHead(200,{'Content-Length':valid.length});res.write(valid.subarray(0,16));const timer=setTimeout(()=>{timers.delete(timer);if(!res.destroyed)res.end(valid.subarray(16));},2500);timers.add(timer);res.once('close',()=>{clearTimeout(timer);timers.delete(timer);});return;}
 }
 return request(req,res);
};
try{
 await hub.start();
 const output=await new Promise((accept,reject)=>{const child=spawn(resolve(executable),[hub.url,hub.identity.pin,join(fixture,'profiles')],{stdio:['ignore','pipe','pipe'],windowsHide:true});let text='';child.stdout.on('data',b=>text+=b);child.stderr.on('data',b=>text+=b);const timer=setTimeout(()=>{child.kill();reject(new Error('Art native fixture timeout'));},90000);child.once('error',e=>{clearTimeout(timer);reject(e);});child.once('exit',code=>{clearTimeout(timer);code===0?accept(text):reject(new Error(`Art native fixture failed (${code}): ${text}`));});});
 if(downloads<8||pings<3||frames<1||redirects||modes.size<8)throw new Error(`Missing actual paths ${JSON.stringify({downloads,pings,frames,redirects,modes:[...modes]})}`);
 console.log(output.trim());
 const key=join(fixture,'tls-key.pem'),cert=join(fixture,'tls-cert.pem');
 const generated=spawnSync(process.env.COMMUNITY_TEST_OPENSSL||'openssl',['req','-x509','-newkey','rsa:2048','-nodes','-keyout',key,'-out',cert,'-subj','/CN=127.0.0.1','-addext','subjectAltName=IP:127.0.0.1','-days','1'],{stdio:'ignore',windowsHide:true});
 if(generated.status!==0)throw new Error('Cannot create disposable untrusted TLS certificate');
 let tlsRequests=0;const tls=createServer({key:readFileSync(key),cert:readFileSync(cert)},(req,res)=>{tlsRequests++;res.end(valid);});tls.on('tlsClientError',()=>{});await new Promise(done=>tls.listen(0,'127.0.0.1',done));
 try{
  await new Promise((accept,reject)=>{const child=spawn(resolve(executable),[`https://127.0.0.1:${tls.address().port}`,hub.identity.pin,join(fixture,'unused-tls-profile'),'--untrusted-art-tls'],{stdio:['ignore','pipe','pipe'],windowsHide:true});let text='';child.stdout.on('data',b=>text+=b);child.stderr.on('data',b=>text+=b);child.once('error',reject);child.once('exit',code=>code===0?accept(console.log(text.trim())):reject(new Error(text)));});
  if(tlsRequests)throw new Error('Untrusted art TLS reached HTTP');
 }finally{await new Promise(done=>tls.close(done));}
}finally{for(const timer of timers)clearTimeout(timer);await hub.close();}
