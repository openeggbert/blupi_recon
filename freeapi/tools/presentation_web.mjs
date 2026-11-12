// Exercise actual fullscreen transitions and compare stable title-screen points.
// Screenshot sampling uses a separate OffscreenCanvas, never the game's canvas.
import {writeFile} from 'node:fs/promises';
import path from 'node:path';

export async function checkPresentation({command, evaluate, waitFor, output, game}) {
  const point = async (x,y) => evaluate(`(() => {
    const c=Module.canvas,r=c.getBoundingClientRect();
    const scale=Math.min(r.width/c.width,r.height/c.height)*Math.min(c.width/640,c.height/480);
    return {x:r.x+(r.width-640*scale)/2+${x}*scale,y:r.y+(r.height-480*scale)/2+${y}*scale};
  })()`);
  const click = async (x,y) => {
    const p=await point(x,y);
    await command('Input.dispatchMouseEvent',{type:'mouseMoved',...p});
    await command('Input.dispatchMouseEvent',{type:'mousePressed',button:'left',clickCount:1,...p});
    await command('Input.dispatchMouseEvent',{type:'mouseReleased',button:'left',clickCount:1,...p});
  };
  const escape = async () => {
    await command('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});
    await command('Input.dispatchKeyEvent',{type:'keyUp',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});
  };
  const capture = async (label) => {
    await evaluate("if (!document.fullscreenElement) Module.canvas.scrollIntoView({block:'start'})");
    await command('Input.dispatchMouseEvent',{type:'mouseMoved',...await point(512,336)});
    await new Promise(resolve => setTimeout(resolve, 100));
    const metrics = await evaluate(`(() => {
      const canvas = Module.canvas, bounds = canvas.getBoundingClientRect();
      const gl = typeof GL !== 'undefined' ? GL.currentContext?.GLctx : null;
      return {bounds:{x:bounds.x,y:bounds.y,width:bounds.width,height:bounds.height},
        buffer:[canvas.width,canvas.height], viewport:gl ? Array.from(gl.getParameter(gl.VIEWPORT)) : null,
        inner:[innerWidth,innerHeight], screen:[screen.width,screen.height,devicePixelRatio], fullscreen:document.fullscreenElement?.id || null};
    })()`);
    const screenshot = await command('Page.captureScreenshot', {format:'png'});
    await writeFile(path.join(output, `${game}-${label}.png`), Buffer.from(screenshot.data, 'base64'));
    metrics.colors = await evaluate(`(async () => {
      const image = await createImageBitmap(await (await fetch('data:image/png;base64,${screenshot.data}')).blob());
      const copy = new OffscreenCanvas(image.width,image.height), context = copy.getContext('2d');
      context.drawImage(image,0,0);
      const m = ${JSON.stringify(metrics)}, b = m.bounds;
      const cssScale = Math.min(b.width/m.buffer[0],b.height/m.buffer[1]);
      const gameScale = Math.min(m.buffer[0]/640,m.buffer[1]/480)*cssScale;
      const left = b.x+(b.width-640*gameScale)/2, top = b.y+(b.height-480*gameScale)/2;
      const sx=image.width/m.inner[0], sy=image.height/m.inner[1];
      const normalized=new OffscreenCanvas(640,480), frame=normalized.getContext('2d');
      frame.drawImage(image,left*sx,top*sy,640*gameScale*sx,480*gameScale*sy,0,0,640,480);
      // Compare small patches instead of individual antialiased letter edges:
      // fractional window scaling changes their subpixel sampling legitimately.
      return [[20,20],[320,20],[620,20],[20,240],[320,240],[620,240],[20,460],[320,460],[620,460]].map(([x,y]) => {
        const pixels=frame.getImageData(x-4,y-4,8,8).data, sum=[0,0,0];
        for(let i=0;i<pixels.length;i+=4) for(let c=0;c<3;c++) sum[c]+=pixels[i+c];
        return sum.map(value=>Math.round(value/64));
      });
    })()`);
    return metrics;
  };
  await evaluate("Module.canvas.scrollIntoView({block:'start'})");
  const cursor = await evaluate("(() => {const r=Module.canvas.getBoundingClientRect(); return {x:r.x+r.width*0.8,y:r.y+r.height*0.7}})()");
  await command('Input.dispatchMouseEvent', {type:'mouseMoved',...cursor});
  await new Promise(resolve => setTimeout(resolve, 200));
  const normal = await capture('windowed');
  const transitions = [];
  for (let attempt=0; attempt<2; ++attempt) {
    await command('Runtime.evaluate', {expression:"document.querySelector('#fullscreen').click()",userGesture:true});
    await waitFor("document.fullscreenElement === Module.canvas");
    await new Promise(resolve => setTimeout(resolve, 500));
    // Verify that the same logical controls remain clickable with letterboxing.
    await click(112,430);
    await waitFor('Module._BlupiWebPhase() === 1545');
    await click(560,430);
    await waitFor('Module._BlupiWebPhase() === 1524');
    await new Promise(resolve => setTimeout(resolve, 100));
    const fullscreen = await capture('fullscreen-'+attempt);
    await evaluate('document.exitFullscreen()');
    await waitFor('!document.fullscreenElement');
    await new Promise(resolve => setTimeout(resolve, 300));
    const restored = await capture('restored-'+attempt);
    const difference = sample => Math.max(...sample.colors.flatMap((color,i) => color.map((value,c) => Math.abs(value-normal.colors[i][c]))));
    transitions.push({fullscreen, restored, fullscreenDifference:difference(fullscreen), restoredDifference:difference(restored)});
  }
  const result = {normal, transitions};
  await writeFile(path.join(output, `${game}-presentation.json`), JSON.stringify(result,null,2));
  console.log(game,'fullscreen comparisons',transitions.map(t=>[t.fullscreenDifference,t.restoredDifference]));
  for (const t of transitions) {
    if (t.fullscreenDifference > 35 || t.restoredDifference > 35) throw new Error(game+' fullscreen changed/cropped the rendered image');
  }
  // Visit the editor used in the reported screenshot, then move the software
  // cursor through disjoint regions. Only ordinary UI input is used.
  await click(112,430);
  await waitFor('Module._BlupiWebPhase() === 1545');
  await click(169,431);
  await waitFor('Module._BlupiWebPhase() === 1529');
  await click(game.startsWith('sb2') ? 462 : 442, game.startsWith('sb2') ? 130 : 124);
  await waitFor('Module._BlupiWebPhase() === 1526');
  for (const [x,y] of [[250,180],[420,300],[300,240],[500,350]]) {
    await command('Input.dispatchMouseEvent',{type:'mouseMoved',...await point(x,y)});
    await new Promise(resolve => setTimeout(resolve, 25));
  }
  await new Promise(resolve => setTimeout(resolve, 100));
  const editor = await command('Page.captureScreenshot',{format:'png'});
  await writeFile(path.join(output,`${game}-editor-cursor.png`),Buffer.from(editor.data,'base64'));
  await escape(); await waitFor('Module._BlupiWebPhase() === 1529');
  await escape(); await waitFor('Module._BlupiWebPhase() === 1545');
  await escape(); await waitFor('Module._BlupiWebPhase() === 1524');
  result.editor = true;
  await writeFile(path.join(output, `${game}-presentation.json`), JSON.stringify(result,null,2));
  return result;
}
