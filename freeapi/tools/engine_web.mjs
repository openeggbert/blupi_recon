// Exercise the reconstructed game's real motor path via keyboard/UI input.
// No game-state writes: IOVERCRAFT and FUNSKATE are the game's own cheat codes.
import {writeFile} from 'node:fs/promises';
import path from 'node:path';

export async function checkEngine({command, evaluate, waitFor, output, game}) {
  const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
  const key = async (type, key, code, windowsVirtualKeyCode) =>
    command('Input.dispatchKeyEvent', {type, key, code, windowsVirtualKeyCode});
  const tap = async (name, code, number) => {
    await key('keyDown', name, code, number);
    await key('keyUp', name, code, number);
  };
  const cheat = async text => {
    for (const letter of text) {
      await tap(letter, 'Key'+letter, letter.charCodeAt(0));
      await delay(25);
    }
  };
  const click = async (x,y) => {
    const point = await evaluate(`(() => {const r=Module.canvas.getBoundingClientRect();
      return {x:r.x+r.width*${x}/640,y:r.y+r.height*${y}/480}})()`);
    await command('Input.dispatchMouseEvent',{type:'mouseMoved',...point});
    await command('Input.dispatchMouseEvent',{type:'mousePressed',button:'left',clickCount:1,...point});
    await command('Input.dispatchMouseEvent',{type:'mouseReleased',button:'left',clickCount:1,...point});
  };
  const screenshot = async label => {
    const result = await command('Page.captureScreenshot',{format:'png'});
    await writeFile(path.join(output,`${game}-engine-${label}.png`),Buffer.from(result.data,'base64'));
  };
  const music = async up => {
    await tap('F2','F2',113);
    await waitFor('Module._BlupiWebPhase() === 1541');
    for (let n=0;n<(up ? 10 : 20);n++) { await click(up ? 130 : 90,240); await delay(25); }
    if (!up) await screenshot('settings');
    await tap('Escape','Escape',27);
    await waitFor('Module._BlupiWebPhase() === 1525');
  };
  const sample = async milliseconds => evaluate(`(async () => {
    const values=[], end=performance.now()+${milliseconds};
    while(performance.now()<end) {
      let sum=0,count=0;
      for(const analyser of smokeAudio.analysers) {
        const pcm=new Float32Array(analyser.fftSize); analyser.getFloatTimeDomainData(pcm);
        for(const value of pcm) sum+=value*value;
        count+=pcm.length;
      }
      values.push({time:performance.now(),rms:Math.sqrt(sum/Math.max(1,count))});
      await new Promise(resolve=>setTimeout(resolve,50));
    }
    return values;
  })()`);

  await evaluate("Module.canvas.scrollIntoView({block:'start'})");
  await music(false);
  await delay(1400);
  const quiet = await sample(800);
  await cheat('IOVERCRAFT');
  await delay(1400); // exclude the one-shot start/confirmation sounds
  const low = await sample(3000); // more than five copies of SOUND018
  await screenshot('low');
  await key('keyDown','ArrowUp','ArrowUp',38);
  await delay(700);
  const high = await sample(2000); // more than five copies of SOUND016
  await screenshot('high');
  await key('keyUp','ArrowUp','ArrowUp',38);
  await delay(700);
  const lowAgain = await sample(2000);
  await cheat('FUNSKATE'); // ordinary game input stops both vehicle buffers
  await delay(1400);
  const stopped = await sample(800);
  await screenshot('stopped');
  const result = {quiet,low,high,lowAgain,stopped};
  await writeFile(path.join(output,`${game}-engine.json`),JSON.stringify(result,null,2));
  const range = values => [Math.min(...values.map(x=>x.rms)),Math.max(...values.map(x=>x.rms))];
  console.log(game,'engine RMS',Object.fromEntries(Object.entries(result).map(([name,values])=>[name,range(values)])));
  await music(true); // restore the initial MIDI volume for the regular demo check
  for (const [name,values] of Object.entries(result)) {
    if (values.length < 8) throw new Error(game+' insufficient engine samples: '+name);
    if (name==='quiet' || name==='stopped') {
      if (range(values)[1] > 0.0001) throw new Error(game+' motor test has unexpected audio: '+name);
    } else if (range(values)[0] < 0.0001) throw new Error(game+' motor loop became silent: '+name);
  }
  return result;
}
