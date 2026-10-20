// SPDX-License-Identifier: GPL-3.0-or-later
import {resolve,join,dirname,sep} from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
import {mkdirSync,existsSync,writeFileSync} from 'node:fs';
import {spawn} from 'node:child_process';
import {randomBytes} from 'node:crypto';
const [backend,fixture,executable]=process.argv.slice(2);
if(!backend||!fixture||!executable)throw new Error('Expected backend, fresh private output and controller executable');
const checkout=resolve(dirname(fileURLToPath(import.meta.url)),'..').toLowerCase(),destination=resolve(fixture).toLowerCase();
if(destination===checkout||destination.startsWith(checkout+sep)||existsSync(fixture))throw new Error('Refusing existing/checkout fixture');
mkdirSync(fixture,{recursive:false});
const nonce=randomBytes(16).toString('hex');
for(const role of ['host','guest']){
 const dir=join(fixture,`community-link-qa-${nonce}-${role}`);mkdirSync(dir);
 writeFileSync(join(dir,'.community-link-qa'),`community-link-qa-v1\nnonce=${nonce}\nrole=${role}\n`,{flag:'wx'});
}
const {Hub}=await import(pathToFileURL(join(resolve(backend),'hub.mjs')));
const hub=new Hub({dir:join(fixture,'server'),port:0,host:'127.0.0.1',log:false,pluginsEnabled:false,communityLinkEnabled:true});
let signedJoins=0;
const authenticate=hub.communityLink.authenticate.bind(hub.communityLink);
hub.communityLink.authenticate=(c,m)=>{const before=c.authenticated,result=authenticate(c,m);if(!before&&c.authenticated)signedJoins++;return result;};
const children=[];
try{
 await hub.start();
 const run=role=>new Promise((accept,reject)=>{
  const dir=join(fixture,`community-link-qa-${nonce}-${role}`);
  const child=spawn(resolve(executable),[],{cwd:dir,windowsHide:true,stdio:['ignore','pipe','pipe'],env:{...process.env,PLATINUM_COMMUNITY_LINK_QA:'union-room',PLATINUM_COMMUNITY_QA_CONFIRM_FRESH:'1',PLATINUM_COMMUNITY_QA_SCENE:'',PLATINUM_COMMUNITY_LINK_QA_DIRECTORY:resolve(dir),PLATINUM_COMMUNITY_LINK_QA_ROLE:role,PLATINUM_COMMUNITY_SERVER_URL:hub.url,PLATINUM_COMMUNITY_SERVER_KEY:hub.identity.pin}});
  children.push(child);let output='';
  child.stdout.on('data',b=>output+=b);child.stderr.on('data',b=>output+=b);
  const timer=setTimeout(()=>{child.kill();reject(new Error(`${role} controller timeout`));},45000);
  child.once('error',error=>{clearTimeout(timer);reject(error);});child.once('exit',code=>{clearTimeout(timer);if(code!==0)reject(new Error(`${role} controller failed (${code}): ${output}`));else accept(output);});
 });
 const output=await Promise.all([run('host'),run('guest')]);
 if(signedJoins!==2||hub.store.db.prepare('SELECT COUNT(*) AS n FROM players').get().n!==2)throw new Error('Actual two-account signed session not verified');
 console.log(output.map(x=>x.trim()).join('\n'));
 console.log('Actual QA setup controller and Service passed against a real loopback backend. Engine map/WM/menu behavior remains a separate whole-client test.');
}finally{for(const child of children)if(child.exitCode===null)child.kill();await hub.close();}
