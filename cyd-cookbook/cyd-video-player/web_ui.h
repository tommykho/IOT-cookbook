/*
 *  cyd-video-player Version 2026.10A
 *  web_ui.h: upload page served by Receive mode (no CDN, the AP has no internet)
 *  Board: ESP32-3248S035, ESP32-2432S028
 *  Author: tommyho510@gmail.com
 *
 *  Photos (any format the phone browser decodes) are resized to fit the screen and
 *  re-encoded as JPEG in the browser. GIF, MP3, WAV and MJPEG upload as-is.
 *  Each file is a raw POST /upload?name=..&ts=.. ; POST /done ends the session.
 */
#pragma once
#include <Arduino.h>

static const char WEB_UI[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>Send to CYD</title><style>
body{font-family:sans-serif;margin:0;background:#111;color:#eee}main{max-width:520px;margin:auto;padding:16px}
h1{font-size:22px}button{width:100%;padding:18px;font-size:20px;border:0;border-radius:10px;background:#2b7cff;color:#fff}
button:disabled{background:#555}.f{background:#222;border-radius:8px;padding:10px;margin:10px 0}
.n{font-size:14px;word-break:break-all}progress{width:100%;height:14px}.s{font-size:13px;color:#aaa}
.ok{color:#5d5}.err{color:#f66}small{color:#888;display:block;margin-top:16px}</style></head><body><main>
<h1>Send to CYD</h1><button id=b>Upload</button>
<input id=i type=file multiple hidden accept="image/*,audio/mpeg,audio/wav,audio/x-wav,.jpg,.jpeg,.gif,.mp3,.wav,.mjpeg,.mjpg">
<p id=m class=s></p><div id=l></div>
<small>Photos are resized to fit the screen. GIFs up to 480 px wide. Video must be .mjpeg.<br>
If the button does nothing, open http://192.168.4.1 in Chrome.</small></main><script>
let L={};const $=i=>document.getElementById(i);
fetch('/info').then(r=>r.json()).then(j=>L=j).catch(()=>{});
$('b').onclick=()=>$('i').click();
$('i').onchange=async e=>{const fs=[...e.target.files];if(!fs.length)return;$('b').disabled=true;$('m').textContent='';
let ok=0;for(const f of fs)ok+=await one(f)?1:0;$('i').value='';$('b').disabled=false;
if(ok){$('m').textContent='Done. The device shows your media and turns its Wi-Fi off.';fetch('/done',{method:'POST'}).catch(()=>{})}
else $('m').textContent='Nothing was sent.'};
function kind(f){const n=f.name.toLowerCase();
if(/\.gif$/.test(n)||f.type=='image/gif')return'gif';
if(/\.(mp3|wav)$/.test(n)||/^audio\//.test(f.type))return'audio';
if(/\.mjpe?g$/.test(n))return'mjpeg';
if(/^image\//.test(f.type)||/\.(jpe?g|png|webp)$/.test(n))return'jpg';return''}
function row(t){const d=document.createElement('div');d.className='f';
d.innerHTML='<div class=n></div><progress max=1 value=0></progress><div class=s>Waiting</div>';
d.firstChild.textContent=t;$('l').append(d);return{p:d.children[1],s:d.children[2]}}
function mb(n){return(n/1048576).toFixed(2)+' MB'}
function stamp(){const d=new Date(),z=n=>String(n).padStart(2,'0');
return d.getFullYear()+z(d.getMonth()+1)+z(d.getDate())+'-'+z(d.getHours())+z(d.getMinutes())+z(d.getSeconds())}
async function one(f){const k=kind(f),r=row(f.name);let body=f,name=f.name;
try{if(!k)throw'Unsupported file type';
if(k=='jpg'){r.s.textContent='Resizing';body=await shrink(f);name=name.replace(/\.[^.]*$/,'')+'.jpg'}
if(k=='gif'){const w=new DataView(await f.slice(0,10).arrayBuffer()).getUint16(6,true),g=L.gifw||480;
if(w>g)throw'GIF is '+w+' px wide, max '+g}
if(k=='audio'&&!/\.(mp3|wav)$/i.test(name))name+=/wav/.test(f.type)?'.wav':'.mp3';
const max=L[k];if(max&&body.size>max)throw'Too large ('+mb(body.size)+', max '+mb(max)+')';
await send(body,name,r);r.s.textContent='Sent';r.s.className='s ok';r.p.value=1;return true}
catch(x){r.s.textContent=String(x);r.s.className='s err';return false}}
async function shrink(f){const b=await createImageBitmap(f,{imageOrientation:'from-image'});
const W=L.w||480,H=L.h||320,s=Math.min(W/b.width,H/b.height,1),c=document.createElement('canvas');
c.width=Math.round(b.width*s);c.height=Math.round(b.height*s);c.getContext('2d').drawImage(b,0,0,c.width,c.height);
let q=.85,o;do{o=await new Promise(r=>c.toBlob(r,'image/jpeg',q));q-=.15}while(L.jpg&&o.size>L.jpg&&q>.3);return o}
function send(b,name,r){return new Promise((ok,no)=>{const x=new XMLHttpRequest();
x.open('POST','/upload?name='+encodeURIComponent(name)+'&ts='+stamp());
x.setRequestHeader('Content-Type','application/octet-stream');
x.upload.onprogress=e=>{if(e.lengthComputable){r.p.value=e.loaded/e.total;
r.s.textContent=Math.round(100*e.loaded/e.total)+'% of '+mb(e.total)}};
x.onload=()=>x.status==200?ok():no(x.responseText||'HTTP '+x.status);x.onerror=()=>no('Connection lost');x.send(b)})}
</script></body></html>)HTML";
