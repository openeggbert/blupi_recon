#!/usr/bin/env node
// Bounded browser smoke check, using Chrome's CDP and Node 22+ (no npm deps).
// Load only the generated site: no local game folders or asset file pickers.
import {spawn} from "node:child_process";
import {createServer} from "node:http";
import {readFile, mkdir, writeFile, stat} from "node:fs/promises";
import path from "node:path";
import {once} from "node:events";

const options = {site: "build/freeapi-web/site", output: "build/freeapi-web/smoke", chrome: "google-chrome"};
const knownGames = ["sb1-blupi", "sb1-eggbert", "sb2-blupi", "sb2-eggbert"];
const games = [];
for (let i = 2; i < process.argv.length; i++) {
  const argument = process.argv[i];
  if (argument.startsWith("--")) options[argument.slice(2)] = process.argv[++i];
  else {
    if (!knownGames.includes(argument)) throw new Error("Supply a game ID, e.g. sb1-blupi. Data is already bundled.");
    games.push(argument);
  }
}
if (!games.length) games.push(...knownGames);
if (typeof WebSocket === "undefined") throw new Error("Use Node 22+.");
const site = path.resolve(options.site), output = path.resolve(options.output);
await mkdir(output, {recursive: true});
const requests = [];
const server = createServer(async (request, response) => {
  try {
    requests.push({method: request.method, path: request.url});
    const relative = decodeURIComponent(new URL(request.url, "http://localhost").pathname);
    let file = path.resolve(site, "." + relative);
    if (!file.startsWith(site + path.sep) && file !== site) throw new Error("Invalid path");
    if ((await stat(file)).isDirectory()) file = path.join(file, "index.html");
    const content = await readFile(file);
    response.writeHead(200, {"Cross-Origin-Opener-Policy": "same-origin", "Cross-Origin-Embedder-Policy": "require-corp",
      "Content-Type": ({".wasm": "application/wasm", ".js": "text/javascript", ".html": "text/html", ".css": "text/css"})[path.extname(file)] || "application/octet-stream"});
    response.end(content);
  } catch { response.writeHead(404); response.end(); }
});
await new Promise((resolve, reject) => {
  server.once("error", reject);
  server.listen(0, "127.0.0.1", resolve);
});
const base = "http://127.0.0.1:" + server.address().port;
const browser = spawn(options.chrome, ["--headless=new", "--no-sandbox", "--disable-dev-shm-usage",
  ...(options.presentation ? ["--screen-info={1920x1080 devicePixelRatio=" + Number(options.dpr || 1) + "}", "--window-size=1008,1000"] : []),
  "--no-first-run", "--no-default-browser-check", "--use-angle=swiftshader", "--enable-unsafe-swiftshader",
  "--autoplay-policy=no-user-gesture-required", "--remote-debugging-port=0", "--user-data-dir=" + path.join(output, "chrome")],
  {stdio: ["ignore", "ignore", "pipe"]});
let socket;
let failCalls = () => {};
const alarm = setTimeout(() => {
  failCalls(new Error("Browser smoke check exceeded 240 seconds"));
  browser.kill("SIGKILL");
  process.exitCode = 1;
}, 240000);
try {
  const endpoint = await new Promise((resolve, reject) => {
    let stderr = "";
    browser.stderr.on("data", (chunk) => {
      stderr += chunk;
      const match = stderr.match(/DevTools listening on (ws:\/\/[^\s]+)/);
      if (match) resolve(match[1]);
    });
    browser.once("exit", () => reject(new Error("Chrome exited before CDP startup: " + stderr)));
  });
  socket = new WebSocket(endpoint);
  await once(socket, "open");
  let id = 0;
  const pending = new Map(), errors = [];
  failCalls = (error) => {
    for (const callbacks of pending.values()) callbacks.reject(error);
    pending.clear();
  };
  socket.onclose = () => failCalls(new Error("Chrome CDP connection closed"));
  socket.onmessage = ({data}) => {
    const message = JSON.parse(data);
    if (message.id) {
      const callbacks = pending.get(message.id);
      if (!callbacks) return;
      pending.delete(message.id);
      if (message.error) callbacks.reject(new Error(JSON.stringify(message.error)));
      else callbacks.resolve(message.result);
    } else if (message.method === "Runtime.exceptionThrown") errors.push(message.params.exceptionDetails);
  };
  const send = (method, params = {}, sessionId) => new Promise((resolve, reject) => {
    const callId = ++id;
    const timer = setTimeout(() => {
      pending.delete(callId);
      reject(new Error(method + " timed out"));
    }, 20000);
    pending.set(callId, {
      resolve: (value) => { clearTimeout(timer); resolve(value); },
      reject: (error) => { clearTimeout(timer); reject(error); }
    });
    socket.send(JSON.stringify({id: callId, method, params, sessionId}));
  });
  const results = [];
  await mkdir(path.join(output, "downloads"), {recursive: true});
  await send("Browser.setDownloadBehavior", {behavior: "allow", downloadPath: path.join(output, "downloads")});
  for (const game of games) {
    errors.length = 0;
    const {targetId} = await send("Target.createTarget", {url: "about:blank"});
    const {sessionId} = await send("Target.attachToTarget", {targetId, flatten: true});
    const command = (method, params) => send(method, params, sessionId);
    const evaluate = async (expression) => {
      const result = await command("Runtime.evaluate", {expression, awaitPromise: true, returnByValue: true});
      if (result.exceptionDetails) throw new Error(JSON.stringify(result.exceptionDetails));
      return result.result.value;
    };
    const waitFor = async (expression, timeout = 30000) => {
      const deadline = Date.now() + timeout;
      while (Date.now() < deadline) {
        try {
          if (await evaluate(expression)) return;
        } catch (error) {
          if (!/context|navigat/i.test(error.message)) throw error;
        }
        await new Promise((resolve) => setTimeout(resolve, 100));
      }
      throw new Error(game + " timeout: " + expression + "\n" + await evaluate("document.body.innerText"));
    };
    await command("Runtime.enable");
    await command("Page.enable");
    // Fullscreen tests use an actual virtual monitor, so screen size, window
    // size and framebuffer transitions stay consistent (including high DPI).
    if (!options.presentation) {
      await command("Emulation.setDeviceMetricsOverride", {width: 1008, height: 1100, deviceScaleFactor: Number(options.dpr || 1), mobile: false});
    }
    await command("Page.navigate", {url: base + "/" + game + "/"});
    await waitFor("typeof ready !== 'undefined' && ready");
    if (await evaluate("FS.analyzePath('/data/web-smoke.txt').exists")) {
      const marker = await evaluate("FS.readFile('/data/web-smoke.txt',{encoding:'utf8'})");
      if (marker !== game) throw new Error("Another edition's saves leaked into " + game);
    }
    const bundled = await evaluate(`({
      pickers: !!document.querySelector('#folder, #soundfont, [webkitdirectory]'),
      seeds: FS.readdir('/data_seed').length - 2,
      image: Array.from(FS.readFile('/image08/init.blp').subarray(0,2)),
      music: Array.from(FS.readFile('/sound/music000.blp').subarray(0,4)),
      fontBytes: FS.stat('/soundfont/default.sf2').size
    })`);
    if (bundled.pickers || !bundled.seeds || bundled.fontBytes <= 0 || bundled.image.join() !== '66,77' || bundled.music.join() !== '77,84,104,100') {
      throw new Error(game + ' bundled game assets are invalid: ' + JSON.stringify(bundled));
    }
    await evaluate(`(() => {
      globalThis.smokeAudio = {peak:0, analysers:[]};
      const connect = AudioNode.prototype.connect;
      AudioNode.prototype.connect = function(destination, ...args) {
        if (destination === this.context.destination) {
          const analyser = this.context.createAnalyser();
          smokeAudio.analysers.push(analyser);
          connect.call(this, analyser, ...args);
          connect.call(analyser, destination);
          return destination;
        }
        return connect.call(this, destination, ...args);
      };
      setInterval(() => {
        for (const analyser of smokeAudio.analysers) {
          const samples = new Float32Array(analyser.fftSize);
          analyser.getFloatTimeDomainData(samples);
          for (const sample of samples) smokeAudio.peak = Math.max(smokeAudio.peak, Math.abs(sample));
        }
      }, 50);
    })()`);
    await evaluate("document.querySelector('#start').click()");
    await waitFor("document.querySelector('#setup').hidden");
    await new Promise((resolve) => setTimeout(resolve, 5000));
    const runtime = await evaluate("({status:document.querySelector('#status').textContent, phase:Module._BlupiWebPhase(), log:document.querySelector('#log').textContent, canvas:[Module.canvas.width,Module.canvas.height], files:FS.readdir('/data')})");
    runtime.bundled = bundled;
    console.log(game, JSON.stringify({status: runtime.status, phase: runtime.phase, canvas: runtime.canvas, files: runtime.files.length}));
    const screenshot = await command("Page.captureScreenshot", {format: "png"});
    await writeFile(path.join(output, game + ".png"), Buffer.from(screenshot.data, "base64"));
    if (errors.length || !runtime.status.startsWith("Running")) throw new Error(game + " runtime failed: " + JSON.stringify(errors));
    if (options.presentation) {
      const {checkPresentation} = await import('./presentation_web.mjs');
      runtime.presentation = await checkPresentation({command, evaluate, waitFor, output, game});
    }
    if (options.navigate) {
      const click = async (x, y) => {
        const point = await evaluate(`(() => {const r=Module.canvas.getBoundingClientRect();return {x:r.x+r.width*${x}/640,y:r.y+r.height*${y}/480}})()`);
        await command("Input.dispatchMouseEvent", {type: "mouseMoved", ...point});
        await command("Input.dispatchMouseEvent", {type: "mousePressed", button: "left", clickCount: 1, ...point});
        await command("Input.dispatchMouseEvent", {type: "mouseReleased", button: "left", clickCount: 1, ...point});
      };
      await click(112, 430);
      await waitFor("Module._BlupiWebPhase() === 1545");
      const chooser = await command("Page.captureScreenshot", {format: "png"});
      await writeFile(path.join(output, game + "-players.png"), Buffer.from(chooser.data, "base64"));
      await click(85, 430);
      await waitFor("Module._BlupiWebPhase() === 1525");
      await new Promise((resolve) => setTimeout(resolve, 1500));
      const mission = await command("Page.captureScreenshot", {format: "png"});
      await writeFile(path.join(output, game + "-play.png"), Buffer.from(mission.data, "base64"));
      runtime.playPhase = await evaluate("Module._BlupiWebPhase()");
      runtime.playLog = await evaluate("document.querySelector('#log').textContent");
      console.log(game, "play phase", runtime.playPhase);
      if (options.engine && game === 'sb2-eggbert') {
        const {checkEngine} = await import('./engine_web.mjs');
        runtime.engine = await checkEngine({command,evaluate,waitFor,output,game});
      }
      const key = async (key, code, number) => {
        await command("Input.dispatchKeyEvent", {type: "keyDown", key, code, windowsVirtualKeyCode: number});
        await command("Input.dispatchKeyEvent", {type: "keyUp", key, code, windowsVirtualKeyCode: number});
      };
      await key("Escape", "Escape", 27);
      await waitFor("Module._BlupiWebPhase() === 1545");
      await key("Escape", "Escape", 27);
      await waitFor("Module._BlupiWebPhase() === 1524");
      // Use the game's own recorded demo, never a bot or injected game state.
      await click(154, 430);
      await waitFor("Module._BlupiWebPhase() === 1525");
      await new Promise((resolve) => setTimeout(resolve, 3000));
      const demo = await command("Page.captureScreenshot", {format: "png"});
      await writeFile(path.join(output, game + "-demo.png"), Buffer.from(demo.data, "base64"));
      runtime.demoLog = await evaluate("document.querySelector('#log').textContent");
      runtime.audioPeak = await evaluate("smokeAudio.peak");
      if (!(runtime.audioPeak > 0)) throw new Error(game + " produced no audio samples");
      if (errors.length) throw new Error(game + " navigation errors: " + JSON.stringify(errors));
    }
    await evaluate("FS.writeFile('/data/web-smoke.txt','" + game + "')");
    await evaluate("save()");
    await evaluate("globalThis.smokeOldDocument = true");
    await command("Page.reload");
    await waitFor("typeof smokeOldDocument === 'undefined' && typeof ready !== 'undefined' && ready");
    const persisted = await evaluate("FS.readFile('/data/web-smoke.txt',{encoding:'utf8'})");
    if (persisted !== game) throw new Error("IDBFS persistence failed for " + game);
    await evaluate("FS.writeFile('/data/web-smoke.txt', 'changed after reload')");
    // Exercise the save-import UI, including rejection before any file writes.
    const importBackup = async (backup) => {
      await evaluate(`(() => {
        const files = new DataTransfer();
        files.items.add(new File([${JSON.stringify(JSON.stringify(backup))}], 'backup.json', {type:'application/json'}));
        document.querySelector('#import').files = files.files;
        document.querySelector('#import').dispatchEvent(new Event('change'));
      })()`);
    };
    const backup = {format: "blupi-recon-saves-v1", game, files: {"web-smoke.txt": Buffer.from(game).toString("base64")}};
    await importBackup({...backup, game: "wrong-edition"});
    await waitFor("document.querySelector('#status').textContent.startsWith('Import failed:')");
    if (await evaluate("FS.readFile('/data/web-smoke.txt',{encoding:'utf8'})") !== "changed after reload") throw new Error("Wrong-edition import modified saves");
    await importBackup({...backup, files: {"../escape.txt": "eA=="}});
    await waitFor("document.querySelector('#status').textContent.includes('Invalid save path')");
    if (await evaluate("FS.analyzePath('/escape.txt').exists")) throw new Error("Save path escaped DATA");
    await importBackup(backup);
    await waitFor("document.querySelector('#status').textContent.startsWith('Saves imported.')");
    // Export through the actual button, capturing the Blob before download.
    const exported = await evaluate(`(async () => {
      let content;
      const createURL = URL.createObjectURL;
      URL.createObjectURL = blob => { content = blob; return createURL(blob); };
      document.querySelector('#export').click();
      URL.createObjectURL = createURL;
      return JSON.parse(await content.text());
    })()`);
    if (exported.game !== game || exported.files["web-smoke.txt"] !== backup.files["web-smoke.txt"]) throw new Error("Save export failed");
    runtime.backupRoundtrip = true;
    results.push({game, ...runtime, persisted, errors: [...errors]});
    await command("Target.closeTarget", {targetId});
  }
  if (requests.some(request => !['GET', 'HEAD'].includes(request.method))) throw new Error('Unexpected upload request');
  for (const game of games) {
    if (!requests.some(request => request.path === '/' + game + '/game.data')) throw new Error(game + ' did not load its packaged assets');
  }
  await writeFile(path.join(output, "report.json"), JSON.stringify(results, null, 2));
  await writeFile(path.join(output, "requests.json"), JSON.stringify(requests, null, 2));
  await send("Browser.close");
} finally {
  clearTimeout(alarm);
  socket?.close();
  if (browser.exitCode === null) {
    browser.kill("SIGTERM");
    const kill = setTimeout(() => browser.kill("SIGKILL"), 3000);
    await once(browser, "exit"); clearTimeout(kill);
  }
  await new Promise((resolve) => server.close(resolve));
}
