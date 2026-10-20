// SPDX-License-Identifier: GPL-3.0-or-later
// Real Hub and account/link protocols. No reference profiles, saves or assets.
import {resolve,join,dirname,sep} from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {mkdirSync,existsSync} from 'node:fs';
import {spawn} from 'node:child_process';
const [backend,fixture,executable]=process.argv.slice(2);
if(!backend||!fixture||!executable)throw new Error('Expected backend root, fresh private fixture root and provider executable');
const checkout=resolve(dirname(fileURLToPath(import.meta.url)),'..').toLowerCase(),destination=resolve(fixture).toLowerCase();
if(destination===checkout||destination.startsWith(checkout+sep)||existsSync(fixture))throw new Error('Refusing existing/checkout fixture path');
mkdirSync(fixture,{recursive:false});
const {Hub}=await import(pathToFileURL(join(resolve(backend),'hub.mjs')));
const hub=new Hub({dir:join(fixture,'server'),port:0,host:'127.0.0.1',log:false,pluginsEnabled:false,communityLinkEnabled:true});
let joins=0,frames=0;
const authenticate=hub.communityLink.authenticate.bind(hub.communityLink);hub.communityLink.authenticate=(c,m)=>{const before=c.authenticated,result=authenticate(c,m);if(!before&&c.authenticated)joins++;return result;};
const binary=hub.communityLink.binary.bind(hub.communityLink);hub.communityLink.binary=(c,data)=>{binary(c,data);frames++;};
try{
 await hub.start();
 const result=await new Promise((accept,reject)=>{
  const child=spawn(resolve(executable),[hub.url,hub.identity.pin,join(fixture,'profiles')],{stdio:['ignore','pipe','pipe'],windowsHide:true});let output='';
  child.stdout.on('data',b=>output+=b);child.stderr.on('data',b=>output+=b);
  const timer=setTimeout(()=>{child.kill();reject(new Error('Actual provider fixture timed out'));},90000);
  child.once('error',error=>{clearTimeout(timer);reject(error);});child.once('exit',code=>{clearTimeout(timer);accept({code,output});});
 });
 if(result.code!==0)throw new Error(`Actual provider failed (${result.code}): ${result.output}`);
 const accounts=hub.store.db.prepare('SELECT name FROM players ORDER BY id').all();
 if(accounts.length!==2||!accounts.some(p=>p.name==='WMLeader')||!accounts.some(p=>p.name==='WMGuest'))throw new Error('Unexpected actual adapter accounts');
 if(joins!==4||frames<16)throw new Error('Actual provider join/metadata/data path not exercised');
 if(hub.communityLink.sessions.size||hub.relay.sessions.size||hub.store.db.prepare('SELECT COUNT(*) AS n FROM relay_worlds').get().n)throw new Error('Fixture retained a session or allocated a world');
 console.log(result.output.trim());console.log('Real loopback backend verified two own-client identities, four signed link joins and actual provider envelopes; no game saves or public service changed.');
}finally{await hub.close();}
