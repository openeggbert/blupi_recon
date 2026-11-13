#!/usr/bin/env node
// Browser multiplayer check through a Free Direct server (Node 22+, Chrome, no npm deps).
//
// Starts a local free-direct-server with a password, serves the generated site, and runs two
// independent browsers (separate Chrome processes, so neither tab is throttled in the
// background). Both go through the game's own screens: choose player, the provider list
// ("Internet through a Free Direct server"), the session list, create / join, the lobby, ready, start, play.
// The host fills the connection dialog through Module.freeApiServerPrompt and learns the room
// ID through Module.freeApiShowRoomCreated; the client types into the real HTML dialog with
// keyboard events, which also proves those keys reach the form and not the game.
//
// With --native-probe (free-direct's relay_directplay_tests), a native Free Direct process also
// finds the browser host's session, joins it and leaves: WASM and native builds interoperate.
//
//   node freeapi/tools/net_web.mjs --server ../free-direct/build-probe/server/free-direct-server \
//        [--native-probe ../free-direct/build-probe/tests/relay_directplay_tests] \
//        [--game sb1-blupi] [--site build/freeapi-web/site] [--output build/freeapi-web/net]
import {spawn} from "node:child_process";
import {createServer} from "node:http";
import {createServer as createTcpServer} from "node:net";
import {readFile, mkdir, writeFile, stat, rm} from "node:fs/promises";
import path from "node:path";
import {once} from "node:events";

const options = {site: "build/freeapi-web/site", output: "build/freeapi-web/net", chrome: "google-chrome",
  game: "sb1-blupi", server: "free-direct-server", password: "net-web-check"};
for (let i = 2; i < process.argv.length; i++) {
  const argument = process.argv[i];
  if (!argument.startsWith("--")) throw new Error("Unknown argument " + argument);
  options[argument.slice(2)] = process.argv[++i];
}
if (typeof WebSocket === "undefined") throw new Error("Use Node 22+.");
const site = path.resolve(options.site), output = path.resolve(options.output);
await mkdir(output, {recursive: true});

// Button centres from the phase tables of src/event.cpp and games/sb2/src/event.cpp, as in
// port/tests/net/*.input. Phases: def.h.
const ui = {
  sb1: {toGamer: {click: [112, 430]}, service: [127, 431], provider: [160, 130], ok: [242, 407], create: [182, 407],
        createOk: [242, 346], hostReady: [308, 118], start: [527, 407], row0: [140, 130], join: [140, 407], clientReady: [308, 160]},
  sb2: {toGamer: {key: 13}, service: [127, 434], provider: [160, 130], ok: [242, 409], create: [182, 409],
        createOk: [242, 346], hostReady: [308, 118], start: [528, 409], row0: [140, 130], join: [140, 409], clientReady: [308, 160]},
}[options.game.slice(0, 3)];
if (!ui) throw new Error("--game must be sb1-blupi, sb1-eggbert, sb2-blupi or sb2-eggbert");
const PHASE = {gamer: 1545, service: 1568, session: 1571, create: 1579, multi: 1576, play: 1525};

const freePort = () => new Promise((resolve, reject) => {
  const probe = createTcpServer();
  probe.once("error", reject);
  probe.listen(0, "127.0.0.1", () => { const {port} = probe.address(); probe.close(() => resolve(port)); });
});

// Static site, with the cross-origin isolation headers the threaded build needs.
const http = createServer(async (request, response) => {
  try {
    let file = path.resolve(site, "." + decodeURIComponent(new URL(request.url, "http://localhost").pathname));
    if (!file.startsWith(site + path.sep) && file !== site) throw new Error("Invalid path");
    if ((await stat(file)).isDirectory()) file = path.join(file, "index.html");
    const content = await readFile(file);
    response.writeHead(200, {"Cross-Origin-Opener-Policy": "same-origin", "Cross-Origin-Embedder-Policy": "require-corp",
      "Content-Type": ({".wasm": "application/wasm", ".js": "text/javascript", ".html": "text/html", ".css": "text/css"})[path.extname(file)] || "application/octet-stream"});
    response.end(content);
  } catch { response.writeHead(404); response.end(); }
});
await new Promise((resolve, reject) => { http.once("error", reject); http.listen(0, "127.0.0.1", resolve); });
const base = "http://127.0.0.1:" + http.address().port;

const relayPort = await freePort();
const passwordFile = path.join(output, "server-password.txt");
await writeFile(passwordFile, options.password + "\n");
const relayLog = [];
const relay = spawn(path.resolve(options.server), ["--host", "127.0.0.1", "--port", String(relayPort), "--password-file", passwordFile],
  {stdio: ["ignore", "ignore", "pipe"]});
relay.stderr.on("data", (chunk) => relayLog.push(String(chunk)));
await new Promise((resolve, reject) => {
  const timer = setTimeout(() => reject(new Error("free-direct-server did not start: " + relayLog.join(""))), 10000);
  relay.stderr.on("data", () => { if (relayLog.join("").includes("listening on")) { clearTimeout(timer); resolve(); } });
  relay.once("exit", (code) => reject(new Error("free-direct-server exited " + code + ": " + relayLog.join(""))));
});

const browsers = [];
async function launch(name) {
  const process_ = spawn(options.chrome, ["--headless=new", "--no-sandbox", "--disable-dev-shm-usage", "--no-first-run",
    "--no-default-browser-check", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--autoplay-policy=no-user-gesture-required",
    "--disable-background-timer-throttling", "--disable-renderer-backgrounding", "--disable-backgrounding-occluded-windows",
    "--remote-debugging-port=0", "--window-size=900,900", "--user-data-dir=" + path.join(output, "chrome-" + name)],
    {stdio: ["ignore", "ignore", "pipe"]});
  browsers.push(process_);
  const endpoint = await new Promise((resolve, reject) => {
    let stderr = "";
    process_.stderr.on("data", (chunk) => {
      stderr += chunk;
      const match = stderr.match(/DevTools listening on (ws:\/\/[^\s]+)/);
      if (match) resolve(match[1]);
    });
    process_.once("exit", () => reject(new Error("Chrome exited before CDP startup: " + stderr)));
  });
  const socket = new WebSocket(endpoint);
  await once(socket, "open");
  let id = 0;
  const pending = new Map(), errors = [];
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
    const timer = setTimeout(() => { pending.delete(callId); reject(new Error(name + ": " + method + " timed out")); }, 30000);
    pending.set(callId, {resolve: (v) => { clearTimeout(timer); resolve(v); }, reject: (e) => { clearTimeout(timer); reject(e); }});
    socket.send(JSON.stringify({id: callId, method, params, sessionId}));
  });
  const {targetId} = await send("Target.createTarget", {url: "about:blank"});
  const {sessionId} = await send("Target.attachToTarget", {targetId, flatten: true});
  const command = (method, params) => send(method, params, sessionId);
  await command("Runtime.enable");
  await command("Page.enable");
  const evaluate = async (expression) => {
    const result = await command("Runtime.evaluate", {expression, awaitPromise: true, returnByValue: true});
    if (result.exceptionDetails) throw new Error(name + ": " + JSON.stringify(result.exceptionDetails));
    return result.result.value;
  };
  const page = {
    name, errors, command, evaluate,
    async waitFor(expression, timeout = 30000) {
      const deadline = Date.now() + timeout;
      while (Date.now() < deadline) {
        try { if (await evaluate(expression)) return; } catch (error) { if (!/context|navigat/i.test(error.message)) throw error; }
        await new Promise((resolve) => setTimeout(resolve, 100));
      }
      const state = await evaluate("({phase: typeof Module !== 'undefined' && Module._BlupiWebPhase ? Module._BlupiWebPhase() : null, status: document.querySelector('#status')?.textContent, dialog: document.querySelector('.free-direct-dialog')?.innerText, log: document.querySelector('#log')?.textContent.slice(-1500)})");
      await page.screenshot("timeout");
      throw new Error(name + " timeout: " + expression + "\n" + JSON.stringify(state, null, 1));
    },
    phase: (value, timeout) => page.waitFor("Module._BlupiWebPhase() === " + value, timeout),
    async click([x, y]) {
      const point = await evaluate(`(() => {const r=Module.canvas.getBoundingClientRect();return {x:r.x+r.width*${x}/640,y:r.y+r.height*${y}/480}})()`);
      await command("Input.dispatchMouseEvent", {type: "mouseMoved", ...point});
      await command("Input.dispatchMouseEvent", {type: "mousePressed", button: "left", clickCount: 1, ...point});
      await command("Input.dispatchMouseEvent", {type: "mouseReleased", button: "left", clickCount: 1, ...point});
      await new Promise((resolve) => setTimeout(resolve, 150));
    },
    async key(code, text) {
      const key = text || (code === 13 ? "Enter" : String.fromCharCode(code));
      // SDL derives the scancode from the DOM code, so a key without one never reaches the game.
      const domCode = code === 13 ? "Enter" : /[a-z]/i.test(key) ? "Key" + key.toUpperCase()
        : /[0-9]/.test(key) ? "Digit" + key : ({".": "Period", "-": "Minus", " ": "Space"})[key] || "";
      const keyText = code === 13 ? "\r" : text;
      await command("Input.dispatchKeyEvent", {type: "keyDown", key, code: domCode, windowsVirtualKeyCode: code, ...(keyText ? {text: keyText} : {})});
      await command("Input.dispatchKeyEvent", {type: "keyUp", key, code: domCode, windowsVirtualKeyCode: code});
      await new Promise((resolve) => setTimeout(resolve, 60));
    },
    async type(text) {
      // Windows virtual-key codes: '.' and '-' are VK_OEM_PERIOD/VK_OEM_MINUS, not their ASCII.
      for (const c of text) await page.key(({".": 190, "-": 189})[c] || c.toUpperCase().charCodeAt(0), c);
    },
    async screenshot(label) {
      const shot = await command("Page.captureScreenshot", {format: "png"});
      await writeFile(path.join(output, `${options.game}-${name}-${label}.png`), Buffer.from(shot.data, "base64"));
    },
  };
  return page;
}

const alarm = setTimeout(() => { console.error("net_web exceeded 300 seconds"); cleanup(); process.exit(1); }, 300000);
function cleanup() {
  for (const b of browsers) b.kill("SIGKILL");
  relay.kill("SIGTERM");
  http.close();
}

try {
  const [host, client] = await Promise.all([launch("host"), launch("client")]);
  const url = `${base}/${options.game}/index.html`;
  for (const page of [host, client]) {
    await page.command("Page.navigate", {url});
    await page.waitFor("document.querySelector('#start') && !document.querySelector('#start').disabled", 120000);
  }
  // The host's dialog answers itself and reports the new room's ID.
  await host.evaluate(`Module.freeApiServerPrompt = async () => ({server: '127.0.0.1', port: '${relayPort}',
      password: '${options.password}', roomId: '', createRoom: true});
    Module.freeApiShowRoomCreated = async (id, url) => { window.netRoom = id; window.netServer = url; };`);
  for (const page of [host, client]) {
    await page.evaluate("document.querySelector('#start').click()");
    await page.waitFor("document.querySelector('#setup').hidden");
  }
  const toService = async (page) => {
    await new Promise((resolve) => setTimeout(resolve, 2500));
    if (ui.toGamer.click) await page.click(ui.toGamer.click); else await page.key(ui.toGamer.key);
    await page.phase(PHASE.gamer);
    await page.click(ui.service);
    await page.phase(PHASE.service);
    await page.screenshot("providers");
    await page.click(ui.provider);
    await page.click(ui.ok);
  };

  // Host: create a room, then a game session in it.
  await toService(host);
  await host.phase(PHASE.session);
  const room = await host.evaluate("window.netRoom");
  if (!/^[0-9A-Z]{5}(-[0-9A-Z]{5}){3}$/.test(room || "")) throw new Error("no room ID: " + room);
  console.log("host created room", room, "on", await host.evaluate("window.netServer"));
  await host.click(ui.create);
  await host.phase(PHASE.create);
  for (const vk of [82, 79, 79, 77]) await host.key(vk, String.fromCharCode(vk));
  await host.click(ui.createOk);
  await host.phase(PHASE.multi);
  console.log("host is in its lobby");

  if (options["native-probe"]) {
    // The game's identity, as freeapi/CMakeLists.txt forms FREE_DIRECT_RELAY_COMPAT.
    const rc = {"sb1-blupi": "blupi", "sb1-eggbert": "sb1-eggbert-en", "sb2-blupi": "sb2-blupi-en", "sb2-eggbert": "sb2-eggbert-en"}[options.game];
    const probe = spawn(path.resolve(options["native-probe"]), ["--child", "probe"], {stdio: ["ignore", "pipe", "pipe"],
      env: {...process.env, FREE_DIRECT_SERVER: "127.0.0.1:" + relayPort, FREE_DIRECT_SERVER_PASSWORD: options.password,
            FREE_DIRECT_ROOM: room, FREE_DIRECT_RELAY_COMPAT: `blupi_recon/${options.game}/${rc}/1`,
            NET_PROBE_SESSION: "ROOM", SDL_VIDEODRIVER: "dummy", SDL_AUDIODRIVER: "dummy"}});
    let probeOutput = "";
    probe.stdout.on("data", (chunk) => probeOutput += chunk);
    probe.stderr.on("data", (chunk) => probeOutput += chunk);
    const [code] = await Promise.race([once(probe, "exit"), new Promise((_, reject) => setTimeout(() => reject(new Error("native probe timed out")), 30000))]);
    if (code !== 0) throw new Error("native probe failed:\n" + probeOutput);
    console.log("native " + probeOutput.trim().split("\n").pop());
  }

  // Client: the real HTML dialog, typed into with keyboard events.
  await toService(client);
  await client.waitFor("!!document.querySelector('.free-direct-dialog form')");
  await client.screenshot("dialog");
  const field = async (name, text) => {
    await client.evaluate(`(() => {const e=document.querySelector('.free-direct-dialog [name=${name}]'); e.focus(); e.select && e.select();})()`);
    await client.type(text);
  };
  await field("server", "127.0.0.1");
  await field("port", String(relayPort));
  await field("password", options.password);
  await field("roomId", room.toLowerCase());
  const typed = await client.evaluate("(() => {const f=document.querySelector('.free-direct-dialog form'); return {server:f.server.value, port:f.port.value, room:f.roomId.value, join:f.querySelectorAll('input[name=mode]')[1].checked};})()");
  if (typed.server !== "127.0.0.1" || typed.port !== String(relayPort) || typed.room !== room.toLowerCase() || !typed.join) {
    throw new Error("typing into the dialog failed: " + JSON.stringify(typed));
  }
  await client.key(13);
  await client.phase(PHASE.session);
  await client.waitFor("!document.querySelector('.free-direct-dialog')");
  await client.screenshot("sessions");
  await client.click(ui.row0);
  await client.click(ui.join);
  await client.phase(PHASE.multi);
  console.log("client joined the host's lobby");

  await new Promise((resolve) => setTimeout(resolve, 1500));
  await host.click(ui.hostReady);
  await client.click(ui.clientReady);
  await new Promise((resolve) => setTimeout(resolve, 1500));
  await host.screenshot("lobby");
  await client.screenshot("lobby");
  await host.click(ui.start);
  await host.phase(PHASE.play, 20000);
  await client.phase(PHASE.play, 20000);
  console.log("both play");

  // The host walks right; the client's screen must change where the host's Blupi was.
  await new Promise((resolve) => setTimeout(resolve, 3000));
  await client.screenshot("before-host-moves");
  const clientBefore = await client.command("Page.captureScreenshot", {format: "png"});
  await host.command("Input.dispatchKeyEvent", {type: "keyDown", key: "ArrowRight", code: "ArrowRight", windowsVirtualKeyCode: 39});
  await new Promise((resolve) => setTimeout(resolve, 2000));
  await host.command("Input.dispatchKeyEvent", {type: "keyUp", key: "ArrowRight", code: "ArrowRight", windowsVirtualKeyCode: 39});
  await new Promise((resolve) => setTimeout(resolve, 1000));
  await host.screenshot("play");
  await client.screenshot("play");
  const clientAfter = await client.command("Page.captureScreenshot", {format: "png"});
  if (clientBefore.data === clientAfter.data) throw new Error("the client's screen did not change while the host walked");
  const stillPlaying = [await host.evaluate("Module._BlupiWebPhase()"), await client.evaluate("Module._BlupiWebPhase()")];
  if (stillPlaying.some((p) => p !== PHASE.play)) throw new Error("left the game: " + stillPlaying);
  for (const page of [host, client]) {
    if (page.errors.length) throw new Error(page.name + " exceptions: " + JSON.stringify(page.errors));
  }
  await writeFile(path.join(output, options.game + "-server.log"), relayLog.join(""));
  console.log(options.game, "OK: browser host and browser client played through the Free Direct server");
} catch (error) {
  await writeFile(path.join(output, options.game + "-server.log"), relayLog.join("")).catch(() => {});
  console.error(error.stack || error);
  process.exitCode = 1;
} finally {
  clearTimeout(alarm);
  cleanup();
  // The two browser profiles hold the game data and compiled code, about 0.7 GB each.
  await new Promise((resolve) => setTimeout(resolve, 500));
  for (const name of ["host", "client"]) await rm(path.join(output, "chrome-" + name), {recursive: true, force: true});
  await rm(passwordFile, {force: true});
}
