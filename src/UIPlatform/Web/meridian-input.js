// Shared runtime helper. Embedded into View/1 pages by Meridian.
(function(global) {
    'use strict';
    global.__meridianInstallInput = function(dispatch, token) {
        if (global.MeridianInput) return;
        const page = global.crypto?.randomUUID?.() || String(Date.now()) + '-' + Math.random().toString(36).slice(2);
        const actions = new Set(), stateHandlers = new Set(), scopes = [];
        let generation = -1, sequence = -1, editing = null;
        let state = Object.freeze({enabled:false, active:false, connected:false, mode:'navigation',
            device:'keyboardMouse', glyphFamily:'xbox', bindings:Object.freeze({})});
        const request = (op, extra={}) => dispatch(token, '__meridian_input', JSON.stringify({op,page,...extra}));
        const call = (fn, arg) => { try { return fn(arg) === true; } catch(error) { global.console?.error('MeridianInput handler',error); return true; } };
        const isText = element => element && (element.isContentEditable || element.tagName==='TEXTAREA' ||
            (element.tagName==='INPUT' && !['button','checkbox','radio','submit','reset','file','image','hidden','range','color'].includes(element.type)));
        const identity = element => element?.dataset?.meridianId || element?.id || '';
        const top = () => scopes[scopes.length-1];
        const visible = element => {
            if (!element?.isConnected || element.tabIndex < 0 || element.matches(':disabled,[aria-disabled=true]') ||
                element.closest('[hidden],[inert],[aria-hidden=true]')) return false;
            const style = global.getComputedStyle(element);
            return style.visibility !== 'hidden' && style.visibility !== 'collapse' && element.getClientRects().length > 0;
        };
        const candidates = scope => {
            if (!scope || !scope.root.isConnected) return [];
            const nodes = scope.getCandidates ? scope.getCandidates() :
                scope.root.querySelectorAll('button,input,select,textarea,a[href],[tabindex],[data-meridian-focusable]');
            return Array.from(nodes).filter(e => scope.root.contains(e) && visible(e));
        };
        const focus = (scope,element) => {
            if (!element || !visible(element)) return;
            if(editing && editing!==element) editing=null;
            scope.root.querySelectorAll('[data-meridian-focused]').forEach(e => e.removeAttribute('data-meridian-focused'));
            element.setAttribute('data-meridian-focused','');
            scope.selected = element; scope.selectedId = identity(element);
            element.focus({preventScroll:true});
            element.scrollIntoView?.({block:'nearest',inline:'nearest'});
        };
        const recover = scope => {
            if (!scope || scope !== top()) return;
            const items = candidates(scope);
            if (items.includes(scope.selected)) return;
            const desired = items.find(e => identity(e) === scope.selectedId && scope.selectedId);
            const initial = typeof scope.initialFocus === 'function' ? scope.initialFocus() :
                typeof scope.initialFocus === 'string' ? items.find(e => identity(e) === scope.initialFocus) : scope.initialFocus;
            focus(scope,desired || (items.includes(initial) ? initial : items[0]));
        };
        function navigate(scope, direction) {
            recover(scope);
            const items = candidates(scope), current = scope.selected;
            if (!current || !items.length) return;
            const explicit = current.getAttribute('data-meridian-' + direction);
            if (explicit) {
                const neighbor = items.find(e => identity(e) === explicit);
                if (neighbor) focus(scope,neighbor);
                return;
            }
            const rect=current.getBoundingClientRect(), x=rect.x+rect.width/2, y=rect.y+rect.height/2;
            const horizontal=direction==='left'||direction==='right', positive=direction==='right'||direction==='down';
            const ranked=items.filter(e=>e!==current).map((element,index)=>{
                const r=element.getBoundingClientRect(), dx=r.x+r.width/2-x, dy=r.y+r.height/2-y;
                const along=(horizontal?dx:dy)*(positive?1:-1), across=Math.abs(horizontal?dy:dx);
                return {element,index,along,score:along+across*2};
            }).filter(item=>item.along>1).sort((a,b)=>a.score-b.score||a.index-b.index);
            if (ranked.length) focus(scope,ranked[0].element);
            else if (scope.wrap) focus(scope,positive?items[0]:items[items.length-1]);
        }
        function adjust(element, direction) {
            const sign=direction==='left'||direction==='down'?-1:1;
            if (element.matches('input[type=range]')) {
                const min=Number(element.min||0), max=Number(element.max||100), step=Number(element.step||1);
                const value=Math.min(max,Math.max(min,Number(element.value)+sign*(Number.isFinite(step)?step:1)));
                if (value!==Number(element.value)) {
                    element.value=String(value);
                    element.dispatchEvent(new Event('input',{bubbles:true}));
                    element.dispatchEvent(new Event('change',{bubbles:true}));
                }
                return true;
            }
            if (element.matches('select')) {
                const step=direction==='up'||direction==='left'?-1:1;
                let next=element.selectedIndex+step;
                while(next>=0 && next<element.options.length && element.options[next].disabled) next+=step;
                if(next>=0 && next<element.options.length) {
                    element.selectedIndex=next; element.dispatchEvent(new Event('change',{bubbles:true}));
                }
                return true;
            }
            return false;
        }
        function scroll(scope,event) {
            if (!event.x && !event.y) return;
            let target;
            if(state.mode==='cursor') {
                target=global.document.elementFromPoint(lastPointer.x,lastPointer.y);
            } else target=scope.selected;
            while(target && target!==scope.root) {
                const style=global.getComputedStyle(target);
                if (/(auto|scroll)/.test(style.overflowY+style.overflowX) &&
                    (target.scrollHeight>target.clientHeight || target.scrollWidth>target.clientWidth)) break;
                target=target.parentElement;
            }
            (target || scope.root).scrollBy?.({left:event.x*600*event.dt,top:-event.y*600*event.dt,behavior:'instant'});
        }
        function defaultAction(event) {
            const scope=top(); if(!scope) return;
            recover(scope);
            if(scope.onAction && call(scope.onAction,event)) return;
            if(event.control==='rightStick' && event.phase==='change') { scroll(scope,event); return; }
            const action=event.action;
            if(event.phase!=='press' && event.phase!=='repeat') return;
            if((action==='previousTab' || action==='nextTab') && event.phase==='press') {
                const tabs=candidates(scope).filter(e=>e.getAttribute('role')==='tab');
                const index=tabs.findIndex(e=>e.getAttribute('aria-selected')==='true');
                const next=tabs[Math.max(0,Math.min(tabs.length-1,(index<0?0:index)+(action==='nextTab'?1:-1)))];
                if(next) {focus(scope,next);next.click();}
                return;
            }
            if(action==='cancel') {
                if(event.phase!=='press') return;
                if(editing) { editing.blur?.(); editing=null; return; }
                if(scope.onBack) call(scope.onBack,event);
                return;
            }
            if(['up','down','left','right'].includes(action)) {
                if(editing) {
                    if(isText(editing)) return;
                    adjust(editing,action); return;
                }
                navigate(scope,action); return;
            }
            if(action==='accept' && event.phase==='press' && state.mode==='navigation') {
                const selected=scope.selected;
                if(!selected || !visible(selected)) return;
                if(isText(selected) || selected.matches('input[type=range],select')) {
                    if(editing===selected) { editing=null; selected.blur(); }
                    else { editing=selected; selected.focus(); }
                } else selected.click();
            }
        }
        const lastPointer={x:0,y:0};
        global.document?.addEventListener('pointermove',e=>{lastPointer.x=e.clientX;lastPointer.y=e.clientY;});
        global.document?.addEventListener('pointerdown',e=>{editing=isText(e.target)?e.target:null;},true);
        global.document?.addEventListener('focusin',e=>{
            const scope=top();
            if(scope && candidates(scope).includes(e.target)) {
                scope.root.querySelectorAll('[data-meridian-focused]').forEach(el=>el.removeAttribute('data-meridian-focused'));
                scope.selected=e.target;scope.selectedId=identity(e.target);
                e.target.setAttribute('data-meridian-focused','');
                if(editing && editing!==e.target) editing=null;
            }
        });
        global.document?.addEventListener('keydown',()=>{if(isText(global.document.activeElement)) editing=global.document.activeElement;},true);
        global.addEventListener?.('blur',()=>{editing=null;});
        function receive(packet) {
            if(!packet || packet.version!==1 || packet.page!==page ||
                !Number.isSafeInteger(packet.generation) || packet.generation<generation ||
                !Number.isSafeInteger(packet.sequence) || packet.sequence<=sequence ||
                !Array.isArray(packet.events) || packet.events.length>128) return;
            if(generation>=0 && (packet.generation!==generation || packet.reset || (state.active && !packet.active))) {
                const cancel=Object.freeze({control:'none',action:'none',phase:'cancel',generation:packet.generation,
                    sequence:packet.sequence,x:0,y:0,value:0,dt:0});
                actions.forEach(handler=>call(handler,cancel));
                scopes.slice().forEach(scope=>{if(scope.onAction)call(scope.onAction,cancel);});
            }
            if(packet.generation!==generation || packet.reset || !packet.active) editing=null;
            generation=packet.generation;sequence=packet.sequence;
            const previous=state;
            const bindings=packet.bindings && typeof packet.bindings==='object' ? Object.freeze({...packet.bindings}) : state.bindings;
            state=Object.freeze({...state,...Object.fromEntries(['enabled','connected','active','mode','device','glyphFamily']
                .filter(k=>Object.prototype.hasOwnProperty.call(packet,k)).map(k=>[k,packet[k]])),bindings,generation});
            const doc=global.document;
            if(doc?.documentElement) {
                doc.documentElement.dataset.meridianInput=state.device;
                doc.documentElement.dataset.meridianMode=state.mode;
            }
            if(JSON.stringify(previous)!==JSON.stringify(state)) stateHandlers.forEach(h=>call(h,state));
            if(!state.active) return;
            recover(top());
            for(const raw of packet.events) {
                if(!raw || typeof raw.control!=='string' || typeof raw.action!=='string' ||
                    !['press','release','repeat','change','cancel'].includes(raw.phase)) continue;
                if(!Number.isFinite(raw.x) || !Number.isFinite(raw.y) || !Number.isFinite(raw.value)) continue;
                const event=Object.freeze({...raw,generation,sequence,dt:Math.max(0,Math.min(0.05,Number(packet.dt)||0))});
                let handled=false;
                for(const handler of actions) if(call(handler,event)) { handled=true; break; }
                if(!handled) defaultAction(event);
            }
        }
        function attachNavigation(options) {
            if(!options?.root?.querySelectorAll) throw new TypeError('A navigation root is required');
            const scope={...options,selected:null,selectedId:''};
            scopes.push(scope); recover(scope);
            const observer=new MutationObserver(()=>recover(scope));
            observer.observe(scope.root,{childList:true,subtree:true,attributes:true,
                attributeFilter:['disabled','hidden','inert','aria-disabled','class','style','tabindex']});
            let disposed=false;
            return function dispose() {
                if(disposed) return; disposed=true;
                observer.disconnect();
                const wasTop=top()===scope;
                const index=scopes.indexOf(scope); if(index>=0) scopes.splice(index,1);
                scope.root.querySelectorAll('[data-meridian-focused]').forEach(e=>e.removeAttribute('data-meridian-focused'));
                if(editing && scope.root.contains(editing)) editing=null;
                if(wasTop) {
                    const parent=top();
                    if(parent) { const selected=parent.selected;parent.selected=null;parent.initialFocus=selected;recover(parent); }
                }
            };
        }
        const labels={
            xbox:{south:'A',east:'B',west:'X',north:'Y',leftShoulder:'LB',rightShoulder:'RB',leftTrigger:'LT',rightTrigger:'RT',leftThumb:'LS',rightThumb:'RS',start:'Menu',back:'View'},
            playstation:{south:'Cross',east:'Circle',west:'Square',north:'Triangle',leftShoulder:'L1',rightShoulder:'R1',leftTrigger:'L2',rightTrigger:'R2',leftThumb:'L3',rightThumb:'R3',start:'Options',back:'Share'},
            generic:{south:'South',east:'East',west:'West',north:'North'}
        };
        const subscribe=(set,handler)=>{if(typeof handler!=='function')throw new TypeError('Handler required');set.add(handler);return()=>set.delete(handler);};
        Object.defineProperty(global,'MeridianInput',{configurable:true,value:Object.freeze({
            version:1,getState:()=>state,onAction:h=>subscribe(actions,h),onStateChange:h=>subscribe(stateHandlers,h),
            setMode:mode=>{if(mode!=='navigation'&&mode!=='cursor')throw new TypeError('Unknown mode');request('mode',{mode});},
            getPrompt:action=>{
                const control=state.bindings[action]||'none',family=labels[state.glyphFamily]||labels.generic;
                return Object.freeze({action,control,family:state.glyphFamily,label:control==='none'?'':family[control]||({dpadUp:'D-pad Up',dpadDown:'D-pad Down',dpadLeft:'D-pad Left',dpadRight:'D-pad Right',none:''})[control]||control});
            },
            attachNavigation,__receive:receive
        })});
        request('ready');
    };
})(window);
