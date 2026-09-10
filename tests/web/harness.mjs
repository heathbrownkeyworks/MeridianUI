import fs from 'node:fs';
import vm from 'node:vm';
export const source=fs.readFileSync(new URL('../../src/UIPlatform/Web/meridian-input.js',import.meta.url),'utf8');
export function createBridge(page='test-page') {
  const requests=[];
  const context=vm.createContext({console,Date,Math,Set,Object,Array,Number,JSON,
    crypto:{randomUUID:()=>page},document:{addEventListener(){},documentElement:{dataset:{}}},addEventListener(){}});
  context.window=context;
  vm.runInContext(source,context);
  context.__meridianInstallInput((token,name,payload)=>requests.push({token,name,payload:JSON.parse(payload)}),'view-token');
  const api=context.MeridianInput;
  let sequence=0;
  const send=(events=[],extra={})=>api.__receive({version:1,page,generation:1,sequence:++sequence,
    enabled:true,active:true,connected:true,mode:'navigation',device:'gamepad',glyphFamily:'xbox',
    events,dt:0.016,...extra});
  const event=(action='accept',phase='press',control='south')=>({action,phase,control,value:1,x:0,y:0});
  return {api,requests,send,event};
}
