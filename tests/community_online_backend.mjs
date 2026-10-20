// SPDX-License-Identifier: GPL-3.0-or-later
// Disposable real backend; no public listeners or original profiles.
import {resolve,join,sep,dirname} from 'node:path';
import {pathToFileURL,fileURLToPath} from 'node:url';
import {mkdirSync,existsSync,readFileSync} from 'node:fs';
import {spawn,spawnSync} from 'node:child_process';
import {createServer} from 'node:https';
const [serverRoot,fixtureRoot,executable]=process.argv.slice(2);
if(!serverRoot||!fixtureRoot||!executable)throw new Error('Expected backend source root, fresh private fixture directory and native test executable');
const checkout=resolve(dirname(fileURLToPath(import.meta.url)),'..').toLowerCase(),destination=resolve(fixtureRoot).toLowerCase();
if(destination===checkout||destination.startsWith(checkout+sep))throw new Error('Private backend/profile fixtures must stay outside the source checkout');
if(existsSync(fixtureRoot))throw new Error('Refusing to replace existing fixture');
mkdirSync(fixtureRoot,{recursive:false});
const {Hub}=await import(pathToFileURL(join(resolve(serverRoot),'hub.mjs')));
const {newClient,issueToken}=await import(pathToFileURL(join(resolve(serverRoot),'identity.mjs')));
const hub=new Hub({dir:join(fixtureRoot,'server'),port:0,host:'127.0.0.1',log:false,communityLinkEnabled:true});
const controlTraffic=[];
async function native(args){return new Promise((accept,reject)=>{
 const child=spawn(resolve(executable),args,{stdio:['ignore','pipe','pipe'],windowsHide:true});let output='';
 child.stdout.on('data',b=>output+=b);child.stderr.on('data',b=>output+=b);
 const timer=setTimeout(()=>{child.kill();reject(new Error('Native adapter test timeout'));},90000);
 child.once('error',error=>{clearTimeout(timer);reject(error);});child.once('exit',code=>{clearTimeout(timer);accept({code,output});});
});}
try{
 // Issue authentic nearly-expired fixture tokens to exercise the real renewal
 // handler immediately. Only the fixture's clock/transport delivery changes.
 const credentials=hub.credentials.bind(hub);let renewals=0;
 let linkJoins=0,linkFrames=0;
 const linkAuthenticate=hub.communityLink.authenticate.bind(hub.communityLink);hub.communityLink.authenticate=(c,m)=>{const before=c.authenticated;const result=linkAuthenticate(c,m);if(!before&&c.authenticated){linkJoins++;const send=c.ws.send.bind(c.ws);c.ws.send=(data,options,callback)=>{if(Buffer.isBuffer(data)&&options?.binary){const middle=Math.floor(data.length/2);send(data.subarray(0,middle),{binary:true,fin:false});send(data.subarray(middle),{binary:true,fin:true},callback);}else send(data,options,callback);};}return result;};
 const linkBinary=hub.communityLink.binary.bind(hub.communityLink);hub.communityLink.binary=(c,data)=>{linkBinary(c,data);linkFrames++;};
 hub.credentials=player=>({...credentials(player),...issueToken(hub.identity,player,hub.store.player(player).key,Date.now()-3590000)});
 let forcedReconnect=false,dropPong=false,droppedPong=false,deepJson=false,controlFrames=0;
 const lobby=hub.lobby.bind(hub);hub.lobby=(c,m)=>{
  if(m.t==='ping'&&dropPong&&hub.store.playerInfo(c.player).name==='OwnHost'){
   dropPong=false;droppedPong=true;
   // Control pongs are unrelated to the suppressed application pong. Keep
   // delivering them until this connection closes; its deadline must still fire.
   const timer=setInterval(()=>{if(c.ws.readyState!==1){clearInterval(timer);return;}for(let i=0;i<64;i++){c.ws.pong('fixture-control');controlFrames++;}},5);
   controlTraffic.push(timer);return;
  }
  if(m.t==='auth.renew')renewals++;
  const result=lobby(c,m);
  if(m.t==='chat.send'&&m.text==='fixture liveness probe')dropPong=true;
  if(m.t==='chat.send'&&m.text==='fixture deep JSON probe'){deepJson=true;c.ws.send('{"a":'+'['.repeat(30000)+'0'+']'.repeat(30000)+'}');}
  if(m.t==='chat.send'&&m.text==='own-client guest native reply'&&!forcedReconnect){
   forcedReconnect=true;const peer=[...hub.connections].find(p=>p.kind==='lobby'&&p.authenticated&&hub.store.playerInfo(p.player).name==='OwnHost');
   setTimeout(()=>peer?.ws.close(1001,'fixture reconnect'),10);
  }
  return result;
 };
 const send=hub.send.bind(hub);hub.send=(c,m)=>{
  if(m.t==='community.link.cancelled')setTimeout(()=>send(c,m),50);
  else if(m.t==='chat.new') {const text=JSON.stringify(m),middle=Math.floor(text.length/2);c.ws.send(text.slice(0,middle),{binary:false,fin:false});c.ws.send(text.slice(middle),{binary:false,fin:true});}
  else send(c,m);
 };
 hub.enroll('FixtureOwner',newClient().key);await hub.start();
 const result=await native([hub.url,hub.identity.pin,join(fixtureRoot,'profiles')]);
 if(result.code!==0)throw new Error(`Native adapter failed (${result.code}): ${result.output}`);
 const rows=hub.store.db.prepare('SELECT name FROM players ORDER BY id').all();
 if(rows.length!==4||!rows.some(p=>p.name==='OwnHost')||!rows.some(p=>p.name==='OwnGuest')||!rows.some(p=>p.name==='OwnRecovered'))throw new Error('Native registration/relogin created the wrong account set');
 if(renewals<2)throw new Error('Real signed lobby renewal was not exercised');
 if(linkJoins!==4||linkFrames<3||hub.communityLink.sessions.size!==0||hub.relay.sessions.size!==0)throw new Error('Real signed link lifecycle/binary fixture was not exercised or retained a session');
 if(!forcedReconnect)throw new Error('Automatic reconnect fixture was not exercised');
 if(!droppedPong||!controlFrames||!deepJson)throw new Error('Liveness/control/deep JSON fixture was not exercised');
 console.log(result.output.trim());console.log('Real loopback backend verified two primary accounts plus one pending-name recovery account, signed renewal, fragmented delivery and liveness/JSON negative cases; original saves/profiles untouched.');
 const key=join(fixtureRoot,'tls-test-key.pem'),certificate=join(fixtureRoot,'tls-test-cert.pem');
 const generated=spawnSync(process.env.COMMUNITY_TEST_OPENSSL||'openssl',['req','-x509','-newkey','rsa:2048','-nodes','-keyout',key,'-out',certificate,'-subj','/CN=127.0.0.1','-addext','subjectAltName=IP:127.0.0.1','-days','1'],{stdio:'ignore',windowsHide:true});
 if(generated.status!==0)throw new Error('Could not generate isolated untrusted TLS fixture certificate');
 let requests=0;const tls=createServer({key:readFileSync(key),cert:readFileSync(certificate)},(req,res)=>{requests++;res.setHeader('Content-Type','application/json');res.end(JSON.stringify({hub_key:hub.identity.pin}));});
 tls.on('tlsClientError',()=>{});await new Promise(accept=>tls.listen(0,'127.0.0.1',accept));
 try{const refused=await native([`https://127.0.0.1:${tls.address().port}`,hub.identity.pin,join(fixtureRoot,'tls-profile'),'--reject-untrusted-tls']);if(refused.code!==0||requests!==0)throw new Error('Untrusted TLS fixture was not refused before HTTP');console.log(refused.output.trim());}
 finally{await new Promise(accept=>tls.close(accept));}
}finally{for(const timer of controlTraffic)clearInterval(timer);await hub.close();}
