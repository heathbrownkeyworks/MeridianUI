import {test} from 'node:test';
import assert from 'node:assert/strict';
import {createBridge} from './harness.mjs';
test('bootstrap has a token-bound unique page handshake',()=>{
  const {api,requests}=createBridge();
  assert.equal(api.version,1);
  assert.equal(requests[0].token,'view-token');
  assert.equal(requests[0].name,'__meridian_input');
  assert.equal(requests[0].payload.op,'ready');
});
test('only current page and generation receive ordered edges',()=>{
  const {api,send,event}=createBridge();const got=[];
  api.onAction(e=>{if(e.phase!=='cancel')got.push(e.action);});
  send([event()]);
  send([event('cancel')],{page:'another-view'});
  send([event()],{generation:0});
  send([event()],{sequence:1});
  send([event('secondary')],{generation:2});
  send([event()],{generation:1});
  assert.deepEqual(got,['accept','secondary']);
});
test('hide drops input and subscribers can unsubscribe',()=>{
  const {api,send,event}=createBridge();let n=0;
  const off=api.onAction(()=>++n);
  send([event()],{active:false});assert.equal(n,0);
  send([event()]);assert.equal(n,1);off();send([event()]);assert.equal(n,1);
});
test('malformed and oversized batches are rejected',()=>{
  const {api,send,event}=createBridge();let n=0;api.onAction(()=>++n);
  api.__receive(null);send([event()],{version:2});
  send(Array(129).fill(event()));
  send([{...event(),x:NaN}]);
  assert.equal(n,0);
});
test('mode requests do not locally override native policy',()=>{
  const {api,requests}=createBridge();
  api.setMode('cursor');
  assert.equal(requests.at(-1).payload.mode,'cursor');
  assert.equal(api.getState().mode,'navigation');
  assert.throws(()=>api.setMode('invalid'));
});
test('custom actions consume before subsequent handlers',()=>{
  const {api,send,event}=createBridge();let second=0;
  api.onAction(()=>true);api.onAction(()=>++second);send([event()]);
  assert.equal(second,0);
});
test('focus generation changes cancel custom held state before fresh input',()=>{
 const {api,send,event}=createBridge();const phases=[];
 api.onAction(e=>phases.push(e.phase));send([event()]);send([event()],{generation:2});send([],{active:false,generation:2});
 assert.deepEqual(phases,['press','cancel','press','cancel']);
});
