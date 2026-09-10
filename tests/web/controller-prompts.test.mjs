import {test} from 'node:test';
import assert from 'node:assert/strict';
import {createBridge} from './harness.mjs';
test('prompts derive from the binding and selected family',()=>{
 const {api,send}=createBridge();
 send([],{bindings:{accept:'south',cancel:'east'}});
 assert.equal(api.getPrompt('accept').label,'A');
 send([],{bindings:{accept:'east'},glyphFamily:'playstation'});
 assert.equal(api.getPrompt('accept').label,'Circle');
 send([],{bindings:{accept:'west'},glyphFamily:'generic'});
 assert.equal(api.getPrompt('accept').label,'West');
 assert.equal(api.getPrompt('missing').label,'');
});
test('state snapshot and binding map are immutable',()=>{
 const {api,send}=createBridge();let updates=0;api.onStateChange(()=>++updates);
 send([],{bindings:{accept:'south'}});
 assert.ok(Object.isFrozen(api.getState()));assert.ok(Object.isFrozen(api.getState().bindings));
 assert.equal(updates,1);
});
