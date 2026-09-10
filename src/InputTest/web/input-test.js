(function start() {
    if(!window.inputTestClose || !window.inputTestCommand) {setTimeout(start,20);return;}
    const api=window.MeridianInput, root=document.querySelector('#root'), result=document.querySelector('#result');
    let count=0, modalDispose=null, custom=false, filtered=false;
    const close=()=>window.inputTestClose('');
    const countOnce=()=>{result.textContent='Accept count: '+(++count);};
    const populate=()=>{document.querySelector('#list').innerHTML=Array.from({length:filtered?5:30},(_,i)=>'<button id="item'+i+'">Item '+(i+1)+'</button>').join('');};
    populate();
    api?.attachNavigation({root,initialFocus:'accept',onBack:close,onAction:event=>{
        if(custom && event.control==='rightStick') {
            document.querySelector('#analogValue').textContent='Analog: '+event.x.toFixed(3)+', '+event.y.toFixed(3);
            return true;
        }
        return false;
    }});
    const dismissModal=()=>{modalDispose?.();modalDispose=null;document.querySelector('#modal').hidden=true;};
    document.querySelector('#modalOpen').onclick=()=>{
        document.querySelector('#modal').hidden=false;
        modalDispose=api?.attachNavigation({root:document.querySelector('#modal'),initialFocus:'modalAccept',onBack:dismissModal}) || null;
    };
    document.querySelector('#modalClose').onclick=dismissModal;
    document.querySelector('#modalAccept').onclick=countOnce;
    document.querySelector('#accept').onclick=countOnce;
    document.querySelector('#close').onclick=close;
    document.querySelector('#cursor').disabled=!api;
    document.querySelector('#cursor').onclick=()=>api?.setMode(api.getState().mode==='cursor'?'navigation':'cursor');
    document.querySelector('#filter').onclick=()=>{filtered=!filtered;populate();};
    document.querySelector('#analog').onclick=()=>{custom=!custom;document.querySelector('#analogValue').textContent='Analog: '+(custom?'custom scope':'default scroll');};
    document.querySelectorAll('[role=tab]').forEach(tab=>tab.onclick=()=>{
        document.querySelectorAll('[role=tab]').forEach(t=>t.setAttribute('aria-selected',String(t===tab)));
        result.textContent='Selected tab: '+tab.textContent;
    });
    document.querySelectorAll('[data-command]').forEach(button=>button.onclick=()=>window.inputTestCommand(button.dataset.command));
    api?.onStateChange(state=>document.querySelector('#state').textContent=JSON.stringify(state,null,2));
    api?.onAction(event=>{if(event.phase==='cancel')document.querySelector('#analogValue').textContent='Analog: canceled';});
    if(!api) document.querySelector('#state').textContent='Input/1 unavailable. Keyboard and mouse remain available.';
    document.addEventListener('keydown',event=>{if(event.key==='Escape'){event.preventDefault();if(!document.querySelector('#modal').hidden)dismissModal();else close();}});
    const drag=document.querySelector('#drag');let dragging=false,moves=0;
    drag.onpointerdown=e=>{dragging=true;drag.setPointerCapture(e.pointerId);drag.textContent='Dragging';};
    drag.onpointermove=()=>{if(dragging)drag.textContent='Drag moves: '+(++moves);};
    drag.onpointerup=drag.onpointercancel=()=>{dragging=false;drag.textContent='Drag released';};
})();
