/* Browser UI concept only. No firmware, GPS, BLE, or live routing. */
'use strict';
(() => {
  const $ = id => document.getElementById(id);
  const demo = window.RPMLOOP_DEMO;
  if (!demo) { $('announcement').textContent = 'Demo data could not be loaded. Keep the data folder beside this HTML file.'; return; }
  const canvas = $('lcd'), ctx = canvas.getContext('2d', {alpha:false});
  const R = 6371000, rad = Math.PI / 180;
  const project = ([lon,lat]) => [(lon+180)/360, (1-Math.asinh(Math.tan(lat*rad))/Math.PI)/2];
  const dist = (a,b) => {const x=(b[0]-a[0])*rad*Math.cos((a[1]+b[1])*rad/2),y=(b[1]-a[1])*rad;return R*Math.hypot(x,y);};
  const coords = demo.route.geometry.coordinates, world = coords.map(project), cumulative = [0];
  coords.slice(1).forEach((p,i) => cumulative.push(cumulative[i]+dist(coords[i],p)));
  const total = cumulative.at(-1);
  const steps = demo.route.legs.flatMap(l => l.steps).map((s,i) => {
    const p=s.maneuver.location;
    const index=coords.reduce((best,c,j)=>dist(c,p)<dist(coords[best],p)?j:best,0);
    const modifier=s.maneuver.modifier||'straight';
    let title=i===0?'Head north':s.maneuver.type==='arrive'?'Arrive at Hawa Mahal':s.maneuver.type.includes('roundabout')?'Roundabout · exit '+s.maneuver.exit:modifier.includes('left')?'Turn left':modifier.includes('right')?'Turn right':'Continue straight';
    const symbol=i===0?'↑':s.maneuver.type==='arrive'?'⚑':s.maneuver.type.includes('roundabout')?'⟳':modifier.includes('left')?'↰':modifier.includes('right')?'↱':'↑';
    return {...s,at:cumulative[index],title,symbol,road:s.name||(i===0?'Albert Hall access':'Unnamed road')};
  });
  const palettes = {
    day:{land:'#e8eddd',park:'#bfd9a7',building:'#d4d9c8',water:'#abd9df',casing:'#b6c1ab',road:'#fffdf0',major:'#f5d38b',path:'#aab99c',label:'#617453',route:'#087b70',halo:'#fbffeb',done:'#8cbaaa',head:'#133e34',text:'#f4fae9',soft:'#b9d4bb'},
    night:{land:'#17291f',park:'#24472d',building:'#22392a',water:'#173c43',casing:'#102119',road:'#455b43',major:'#6c7147',path:'#344b34',label:'#84997a',route:'#c5ef86',halo:'#152d24',done:'#55705a',head:'#101f17',text:'#ecf6dc',soft:'#a1ba93'}
  };
  // Source geometry is indexed once. 256px Web Mercator tiles are rasterized on
  // demand and cached locally; no tile server or network requests are needed.
  const features=demo.features.map(f => {
    const points=f.points.map(project), xs=points.map(p=>p[0]),ys=points.map(p=>p[1]);
    return {...f,points,bbox:[Math.min(...xs),Math.min(...ys),Math.max(...xs),Math.max(...ys)]};
  });
  const cache=new Map(), MAX_TILES=100, MIN_ZOOM=14, MAX_ZOOM=18;
  const state={zoom:16,night:false,progress:total*.67,playing:false,follow:true,center:[0,0],lastTime:0,frame:0};
  const bounds=[project([demo.meta.bbox[0],demo.meta.bbox[3]]),project([demo.meta.bbox[2],demo.meta.bbox[1]])];
  const clamp=(v,a,b)=>Math.max(a,Math.min(b,v));
  const meters=m=>m>=1000?(m/1000).toFixed(1)+' km':Math.max(0,Math.round(m/10)*10)+' m';
  function tile(z,x,y) {
    const key=[state.night,z,x,y].join('/');
    if(cache.has(key))return cache.get(key);
    const p=palettes[state.night?'night':'day'],size=2**z,unit=256*size;
    const c=document.createElement('canvas');c.width=c.height=256;
    const g=c.getContext('2d',{alpha:false});g.fillStyle=p.land;g.fillRect(0,0,256,256);
    const local=features.filter(f=>f.bbox[0]<=(x+1)/size&&f.bbox[2]>=x/size&&f.bbox[1]<=(y+1)/size&&f.bbox[3]>=y/size);
    function path(f){g.beginPath();f.points.forEach(([a,b],i)=>i?g.lineTo(a*unit-x*256,b*unit-y*256):g.moveTo(a*unit-x*256,b*unit-y*256));}
    for(const f of local){const t=f.tags;if(t.highway)continue;
      const color=t.natural==='water'||t.waterway?p.water:t.leisure||t.landuse==='forest'||t.landuse==='grass'||t.natural==='wood'?p.park:t.building?p.building:null;
      if(color){path(f);g.fillStyle=color;g.fill();}
    }
    const roads=local.filter(f=>f.tags.highway);
    const major=t=>/^(primary|secondary|tertiary|trunk)/.test(t);
    const foot=t=>/^(footway|path|steps|pedestrian|cycleway|track)$/.test(t);
    const width=t=>(major(t)?7:foot(t)?1.2:3.3)*Math.max(.6,2**((z-16)*.5));
    g.lineCap=g.lineJoin='round';
    for(const f of roads){if(foot(f.tags.highway))continue;path(f);g.strokeStyle=p.casing;g.lineWidth=width(f.tags.highway)+1.4;g.stroke();}
    for(const f of roads){path(f);g.strokeStyle=foot(f.tags.highway)?p.path:major(f.tags.highway)?p.major:p.road;g.lineWidth=width(f.tags.highway);g.stroke();}
    cache.set(key,c);if(cache.size>MAX_TILES)cache.delete(cache.keys().next().value);
    return c;
  }
  function position(){
    let i=1;while(i<cumulative.length-1&&cumulative[i]<state.progress)i++;
    const t=clamp((state.progress-cumulative[i-1])/(cumulative[i]-cumulative[i-1]||1),0,1);
    return {point:world[i-1].map((v,k)=>v+(world[i][k]-v)*t),index:i,angle:Math.atan2(world[i][0]-world[i-1][0],-(world[i][1]-world[i-1][1]))};
  }
  function activeIndex(){let i=0;while(i<steps.length-1&&steps[i+1].at<=state.progress+.3)i++;return i;}
  function fitText(text,x,y,max,size=12,color='#fff',weight=600){ctx.font=`${weight} ${size}px "Segoe UI",Arial,sans-serif`;while(ctx.measureText(text).width>max&&text.length>1)text=text.slice(0,-2)+'…';ctx.fillStyle=color;ctx.fillText(text,x,y);}
  function routePath(points,toScreen){ctx.beginPath();points.forEach((p,i)=>{const [x,y]=toScreen(p);i?ctx.lineTo(x,y):ctx.moveTo(x,y);});}
  function draw(){
    const w=canvas.width,h=canvas.height,top=52,bottom=h-38,p=palettes[state.night?'night':'day'];
    const pos=position(),unit=256*2**state.zoom;
    if(state.follow)state.center=[pos.point[0],pos.point[1]-(h*.64-(top+bottom)/2)/unit];
    const origin=[state.center[0]*unit-w/2,state.center[1]*unit-(top+bottom)/2];
    const screen=pt=>[pt[0]*unit-origin[0],pt[1]*unit-origin[1]];
    ctx.fillStyle=p.land;ctx.fillRect(0,0,w,h);
    ctx.save();ctx.beginPath();ctx.rect(0,top,w,bottom-top);ctx.clip();
    for(let x=Math.floor(origin[0]/256);x<=Math.floor((origin[0]+w)/256);x++)for(let y=Math.floor((origin[1]+top)/256);y<=Math.floor((origin[1]+bottom)/256);y++)ctx.drawImage(tile(state.zoom,x,y),Math.round(x*256-origin[0]),Math.round(y*256-origin[1]));
    // Sparse labels keep a 2.8-inch display readable at its native pixel count.
    const occupied=[];ctx.font='9px "Segoe UI",Arial';
    for(const f of features){const name=f.tags['name:en']||f.tags.name;if(!name||!/^(primary|secondary|tertiary)$/.test(f.tags.highway))continue;
      const a=screen(f.points[Math.floor(f.points.length/2)]);const tw=ctx.measureText(name).width;
      if(a[0]<12||a[0]+tw>w-35||a[1]<top+12||a[1]>bottom-12||occupied.some(b=>Math.abs(a[0]-b[0])<95&&Math.abs(a[1]-b[1])<25))continue;
      occupied.push(a);ctx.lineWidth=3;ctx.strokeStyle=p.land;ctx.strokeText(name,...a);ctx.fillStyle=p.label;ctx.fillText(name,...a);
    }
    ctx.lineCap=ctx.lineJoin='round';routePath(world,screen);ctx.strokeStyle=p.halo;ctx.lineWidth=9;ctx.stroke();ctx.strokeStyle=p.route;ctx.lineWidth=5;ctx.stroke();
    routePath([...world.slice(0,pos.index),pos.point],screen);ctx.strokeStyle=p.done;ctx.lineWidth=5;ctx.stroke();
    const end=screen(world.at(-1));ctx.fillStyle=p.head;ctx.fillRect(end[0]-3,end[1]-15,2,16);ctx.fillRect(end[0]-1,end[1]-15,10,7);
    const xy=screen(pos.point);ctx.save();ctx.translate(...xy);ctx.rotate(pos.angle);
    ctx.beginPath();ctx.arc(0,0,17,0,Math.PI*2);ctx.fillStyle=state.night?'#c5ef8625':'#087b7020';ctx.fill();
    ctx.beginPath();ctx.moveTo(0,-11);ctx.lineTo(8,9);ctx.lineTo(0,5);ctx.lineTo(-8,9);ctx.closePath();ctx.lineWidth=2.2;ctx.strokeStyle=p.halo;ctx.stroke();ctx.fillStyle=p.route;ctx.fill();ctx.restore();
    const scaleMeters=2**(18-state.zoom)*25,scalePx=scaleMeters/(40075017*Math.cos(coords[pos.index][1]*rad)/unit);
    ctx.strokeStyle=p.label;ctx.lineWidth=1;ctx.beginPath();ctx.moveTo(9,bottom-14);ctx.lineTo(9,bottom-10);ctx.lineTo(9+scalePx,bottom-10);ctx.lineTo(9+scalePx,bottom-14);ctx.stroke();fitText(meters(scaleMeters),10,bottom-18,80,8,p.label,500);
    ctx.restore();
    const current=activeIndex(),next=steps[Math.min(current+1,steps.length-1)],arrived=state.progress>=total-.5;
    ctx.fillStyle=p.head;ctx.fillRect(0,0,w,top);
    fitText('DEMO',8,11,35,7,p.soft,600);fitText('PHONE ↔ TFT',w-89,11,81,7,p.soft,600);
    // Draw simple direction graphics instead of relying on OS-specific arrow glyphs.
    ctx.save();ctx.translate(22,35);ctx.strokeStyle=p.route===palettes.day.route?'#c6ec94':p.route;ctx.lineWidth=3;ctx.lineCap='round';ctx.lineJoin='round';ctx.beginPath();
    if(arrived){ctx.arc(0,0,8,0,Math.PI*2);}else if(next.symbol==='↱'){ctx.moveTo(-5,9);ctx.lineTo(-5,-5);ctx.lineTo(8,-5);ctx.moveTo(3,-10);ctx.lineTo(8,-5);ctx.lineTo(3,0);}else if(next.symbol==='↰'){ctx.moveTo(5,9);ctx.lineTo(5,-5);ctx.lineTo(-8,-5);ctx.moveTo(-3,-10);ctx.lineTo(-8,-5);ctx.lineTo(-3,0);}else if(next.symbol==='⟳'){ctx.arc(0,0,8,.5,5.5);ctx.moveTo(2,-10);ctx.lineTo(7,-6);ctx.lineTo(1,-4);}else{ctx.moveTo(0,10);ctx.lineTo(0,-10);ctx.moveTo(-6,-4);ctx.lineTo(0,-10);ctx.lineTo(6,-4);}ctx.stroke();ctx.restore();
    fitText(arrived?'You have arrived':meters(next.at-state.progress)+' · '+next.title,43,28,w-50,12,p.text,700);
    fitText(arrived?demo.meta.to:next.road,43,43,w-50,10,p.soft,500);
    ctx.fillStyle=p.head;ctx.fillRect(0,bottom,w,38);ctx.fillStyle=state.night?'#344d33':'#38624d';ctx.fillRect(0,bottom,w,1);
    const remaining=total-state.progress,minutes=Math.ceil(remaining/(demo.route.distance/demo.route.duration)/60);
    fitText(arrived?'0':state.playing?'36':'—',9,bottom+22,43,19,p.text,700);fitText('km/h',47,bottom+21,35,8,p.soft,500);
    fitText(arrived?'ARRIVED':minutes+' min',w*.38,bottom+17,74,13,p.text,700);fitText(meters(remaining)+' left',w*.38,bottom+30,80,8,p.soft,500);
    fitText(state.follow?'FOLLOW':'EXPLORE',w-62,bottom+14,57,8,p.soft,600);fitText('z'+state.zoom,w-30,bottom+29,25,9,p.text,600);
    // Keep PNG exports complete: accessible HTML buttons sit over these pixels.
    ['+','−','⌖'].forEach((symbol,i)=>{const y=66+i*31+(i===2?4:0);ctx.fillStyle=state.night?'#21342b':'#fffff2';ctx.fillRect(w-34,y,28,28);ctx.strokeStyle=state.night?'#47604a':'#bbc8b2';ctx.lineWidth=1;ctx.strokeRect(w-34+.5,y+.5,27,27);fitText(symbol,w-27,y+20,25,19,state.night?'#dcebcf':'#193e36',500);});
    $('zoom-in').disabled=state.zoom===MAX_ZOOM;$('zoom-out').disabled=state.zoom===MIN_ZOOM;
    $('recenter').setAttribute('aria-pressed',String(state.follow));
    $('progress').value=String(Math.round(state.progress/total*1000));$('progress-label').value=Math.round(state.progress/total*100)+'%';
    $('ride-state').textContent=arrived?'DESTINATION REACHED':state.playing?'SIMULATION RUNNING':'SIMULATION PAUSED';
    document.querySelectorAll('#turns li').forEach((li,i)=>{li.classList.toggle('active',i===current);li.querySelector('button').setAttribute('aria-current',i===current?'step':'false');});
    canvas.setAttribute('aria-label',`${arrived?'Arrived':next.title+' in '+meters(next.at-state.progress)+', '+next.road}. Map zoom ${state.zoom}. Drag to pan; plus and minus to zoom; R to recenter.`);
  }
  function announce(text){$('announcement').textContent=text;}
  function setPlay(value){state.playing=value;state.lastTime=0;$('play').innerHTML=value?'Ⅱ <span>Pause ride</span>':'▶ <span>Play ride</span>';$('play').setAttribute('aria-label',value?'Pause ride':'Play ride');draw();if(value){cancelAnimationFrame(state.frame);state.frame=requestAnimationFrame(tick);}}
  function tick(time){if(!state.playing)return;if(state.lastTime)state.progress=clamp(state.progress+Math.min((time-state.lastTime)/1000,.25)*10*Number($('speed').value),0,total);state.lastTime=time;if(state.progress>=total){setPlay(false);announce('Arrived at Hawa Mahal.');return;}draw();state.frame=requestAnimationFrame(tick);}
  function zoom(delta){state.zoom=clamp(state.zoom+delta,MIN_ZOOM,MAX_ZOOM);draw();}
  function recenter(){state.follow=true;draw();announce('Map following rider.');}
  $('play').onclick=()=>{if(state.progress>=total)state.progress=0;setPlay(!state.playing);};
  $('reset').onclick=()=>{state.progress=0;state.follow=true;state.zoom=16;setPlay(false);announce('Ride reset to Albert Hall Museum.');};
  $('zoom-in').onclick=()=>zoom(1);$('zoom-out').onclick=()=>zoom(-1);$('recenter').onclick=recenter;
  $('progress').oninput=e=>{state.progress=Number(e.target.value)/1000*total;state.follow=true;if(state.progress>=total)setPlay(false);else draw();};
  function theme(night){state.night=night;document.body.classList.toggle('night',night);$('day').setAttribute('aria-pressed',String(!night));$('night').setAttribute('aria-pressed',String(night));draw();}
  $('day').onclick=()=>theme(false);$('night').onclick=()=>theme(true);
  function fitCase(){const portrait=canvas.width===240;const factor=Math.min(Number($('scale').value),($('stage').clientWidth-56)/(portrait?310:430));$('device-wrap').style.transform=`scale(${factor})`;$('stage').style.height=Math.max(350,((portrait?390:310)+28)*factor+85)+'px';}
  $('orientation').onchange=e=>{const portrait=e.target.value==='portrait';canvas.width=portrait?240:320;canvas.height=portrait?320:240;$('stage').classList.toggle('portrait',portrait);$('resolution').textContent=portrait?'240 × 320 · PORTRAIT':'320 × 240 · LANDSCAPE';fitCase();draw();};
  $('scale').onchange=fitCase;
  window.addEventListener('resize',fitCase);
  $('export').onclick=()=>{draw();const a=$('lcd-download');a.download=`rpmloop-lcd-${state.night?'night':'day'}-${canvas.width}x${canvas.height}.png`;a.href=canvas.toDataURL('image/png');a.click();announce('Exported the native-resolution LCD image.');};
  let drag=null;
  canvas.onpointerdown=e=>{const r=canvas.getBoundingClientRect();const y=(e.clientY-r.top)*canvas.height/r.height;if(y<52||y>canvas.height-38)return;state.follow=false;drag={x:e.clientX,y:e.clientY,center:[...state.center]};canvas.setPointerCapture(e.pointerId);draw();};
  canvas.onpointermove=e=>{if(!drag)return;const r=canvas.getBoundingClientRect(),unit=256*2**state.zoom;state.center=[clamp(drag.center[0]-(e.clientX-drag.x)*canvas.width/r.width/unit,bounds[0][0],bounds[1][0]),clamp(drag.center[1]-(e.clientY-drag.y)*canvas.height/r.height/unit,bounds[0][1],bounds[1][1])];draw();};
  canvas.onpointerup=canvas.onpointercancel=canvas.onlostpointercapture=()=>{drag=null;};
  canvas.addEventListener('wheel',e=>{e.preventDefault();zoom(e.deltaY<0?1:-1);},{passive:false});
  canvas.onkeydown=e=>{if(['+','=','-','r','R','ArrowUp','ArrowDown','ArrowLeft','ArrowRight'].includes(e.key))e.preventDefault();else return;if(e.key==='r'||e.key==='R')recenter();else if(['+','=','-'].includes(e.key))zoom(e.key==='-'?-1:1);else{state.follow=false;const delta=35/(256*2**state.zoom);state.center[0]=clamp(state.center[0]+(e.key==='ArrowLeft'?-delta:e.key==='ArrowRight'?delta:0),bounds[0][0],bounds[1][0]);state.center[1]=clamp(state.center[1]+(e.key==='ArrowUp'?-delta:e.key==='ArrowDown'?delta:0),bounds[0][1],bounds[1][1]);draw();}};
  for(const [i,s] of steps.entries()){
    const li=document.createElement('li'),button=document.createElement('button'),symbol=document.createElement('span'),text=document.createElement('span'),title=document.createElement('span'),small=document.createElement('small');
    symbol.className='turn-symbol';symbol.textContent=s.symbol;symbol.setAttribute('aria-hidden','true');title.className='turn-text';title.textContent=s.title;small.textContent=s.road+' · '+meters(s.at);text.append(title,small);button.append(symbol,text);button.onclick=()=>{state.progress=s.at;state.follow=true;setPlay(false);announce(s.title+', '+s.road);};li.append(button);$('turns').append(li);
  }
  $('total-distance').textContent=(demo.route.distance/1000).toFixed(2);
  fitCase();draw();
})();
