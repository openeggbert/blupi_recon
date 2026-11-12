#!/usr/bin/env node
// Browser check of the web build (port/docs/WEB.md). Node 22+ and Chrome, no npm packages.
//
// Serves the assembled site (port/tools/package_web.py) and, for the test only, a game folder as a
// listing (the loader's test-import). For each program given, one headless Chrome:
//   1. opens the program's page, which copies the game's files into IndexedDB and starts the program;
//   2. checks that the title screen is drawn and, with --attract, the attract-mode demo after it;
//   3. reloads the page: the files come from IndexedDB this time, and the program starts again.
// Screenshots go to --output. Never runs an original executable.
//
//   node port/tools/web_check.mjs --site build/port-web/site --output build/port-web/check \
//        --program blupi-sdl3=/path/to/Speedy_Blupi_I [--program ...] [--attract]
//
// With --relay <free-direct-server> --relay-client <native blupi-sdl3> it checks multiplayer between
// the browser and the desktop instead (the first --program, SB1): the server runs on this machine, the
// browser's game creates a room and a session (port/tests/net/sb1_browser_host.input), and the native
// program, started with that room (sb1_browser_client.input, SDL's offscreen video), joins it; both
// must reach PLAY.
import {spawn} from "node:child_process";
import {createServer} from "node:http";
import {readFile, readdir, stat, mkdir, writeFile, rm} from "node:fs/promises";
import {inflateSync} from "node:zlib";
import path from "node:path";
import {once} from "node:events";

if (typeof WebSocket === "undefined") throw new Error("Use Node 22+.");
const options = {site: "build/port-web/site", output: "build/port-web/check", chrome: "google-chrome", programs: [], attract: false,
                 inputs: "port/tests/net"};
for (let i = 2; i < process.argv.length; i++) {
  const a = process.argv[i];
  if (a === "--attract") options.attract = true;
  else if (a === "--program") { const [name, dir] = process.argv[++i].split("="); options.programs.push({name, dir}); }
  else if (a.startsWith("--")) options[a.slice(2)] = process.argv[++i];
  else throw new Error("unknown argument " + a);
}
const site = path.resolve(options.site), output = path.resolve(options.output);
await rm(output, {recursive: true, force: true});
await mkdir(output, {recursive: true});

async function walk(dir, prefix = "") {
  const out = [];
  for (const entry of await readdir(dir, {withFileTypes: true})) {
    const rel = prefix + entry.name;
    if (entry.isDirectory()) out.push(...await walk(path.join(dir, entry.name), rel + "/"));
    else out.push({path: rel, size: (await stat(path.join(dir, entry.name))).size});
  }
  return out;
}
const types = {".html": "text/html", ".js": "text/javascript", ".css": "text/css", ".wasm": "application/wasm", ".json": "application/json"};
const testData = Object.fromEntries(options.programs.map((p) => [p.name, path.resolve(p.dir)]));
const inputs = path.resolve(options.inputs);
const http = createServer(async (req, res) => {
  try {
    const url = decodeURIComponent(new URL(req.url, "http://x").pathname);
    let file;
    const m = url.match(/^\/test-data\/([^/]+)\/(.*)$/);
    const input = url.match(/^\/test-input\/([A-Za-z0-9_.-]+)$/);
    if (input) file = path.join(inputs, input[1]);
    else if (m) {
      const root = testData[m[1]];
      if (!root) throw new Error("no test data");
      if (m[2] === "index.json") {
        res.writeHead(200, {"content-type": "application/json"});
        res.end(JSON.stringify(await walk(root)));
        return;
      }
      file = path.join(root, m[2]);
      if (!file.startsWith(root)) throw new Error("outside");
    } else {
      file = path.join(site, url.endsWith("/") ? url + "index.html" : url);
      if (!file.startsWith(site)) throw new Error("outside");
    }
    const data = await readFile(file);
    res.writeHead(200, {"content-type": types[path.extname(file)] || "application/octet-stream"});
    res.end(data);
  } catch (error) {
    res.writeHead(404);
    res.end(String(error));
  }
});
http.listen(0, "127.0.0.1");
await once(http, "listening");
const base = `http://127.0.0.1:${http.address().port}`;
const browsers = [];

async function launch(name) {
  const chrome = spawn(options.chrome, ["--headless=new", "--no-sandbox", "--disable-dev-shm-usage", "--no-first-run",
    "--no-default-browser-check", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--autoplay-policy=no-user-gesture-required",
    "--disable-background-timer-throttling", "--disable-renderer-backgrounding", "--disable-backgrounding-occluded-windows",
    "--remote-debugging-port=0", "--window-size=1100,1000", "--user-data-dir=" + path.join(output, "chrome-" + name)],
    {stdio: ["ignore", "ignore", "pipe"]});
  browsers.push(chrome);
  const endpoint = await new Promise((resolve, reject) => {
    let stderr = "";
    chrome.stderr.on("data", (chunk) => {
      stderr += chunk;
      const match = stderr.match(/DevTools listening on (ws:\/\/[^\s]+)/);
      if (match) resolve(match[1]);
    });
    chrome.once("exit", () => reject(new Error("Chrome exited before CDP startup: " + stderr)));
  });
  const socket = new WebSocket(endpoint);
  await once(socket, "open");
  let id = 0;
  const pending = new Map(), errors = [], consoleLines = [];
  socket.onmessage = ({data}) => {
    const message = JSON.parse(data);
    if (message.id) {
      const callbacks = pending.get(message.id);
      if (!callbacks) return;
      pending.delete(message.id);
      if (message.error) callbacks.reject(new Error(JSON.stringify(message.error)));
      else callbacks.resolve(message.result);
    } else if (message.method === "Runtime.exceptionThrown") errors.push(message.params.exceptionDetails);
    else if (message.method === "Runtime.consoleAPICalled")
      consoleLines.push(message.params.type + ": " + message.params.args.map((a) => a.value ?? a.description ?? "").join(" "));
  };
  const send = (method, params = {}, sessionId) => new Promise((resolve, reject) => {
    const callId = ++id;
    const timer = setTimeout(() => { pending.delete(callId); reject(new Error(name + ": " + method + " timed out")); }, 60000);
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
  return {name, errors, consoleLines, command, evaluate,
    async waitFor(expression, timeout = 60000) {
      const deadline = Date.now() + timeout;
      while (Date.now() < deadline) {
        try { if (await evaluate(expression)) return; } catch (error) { if (!/context|navigat/i.test(error.message)) throw error; }
        await new Promise((r) => setTimeout(r, 200));
      }
      const state = await evaluate("({status: document.querySelector('#status')?.textContent, log: document.querySelector('#log')?.textContent.slice(-2000)})");
      throw new Error(name + " timeout: " + expression + "\n" + JSON.stringify(state, null, 1) +
        "\nconsole:\n" + consoleLines.slice(-30).join("\n") + "\nexceptions: " + JSON.stringify(errors).slice(0, 1500));
    },
    async goto(url) {
      await command("Page.navigate", {url});
      await new Promise((r) => setTimeout(r, 500));
    },
    async canvasShot(label) {
      const r = await evaluate("(() => {const b=document.querySelector('#canvas').getBoundingClientRect();return {x:b.x,y:b.y,width:b.width,height:b.height}})()");
      const shot = await command("Page.captureScreenshot", {format: "png", clip: {...r, scale: 1}});
      const png = Buffer.from(shot.data, "base64");
      await writeFile(path.join(output, `${name}-${label}.png`), png);
      return png;
    },
  };
}

// Share of pixels that are neither black nor one colour, from an 8-bit RGB(A) PNG.
function variety(png) {
  let off = 8, width = 0, height = 0, channels = 0;
  const idat = [];
  while (off < png.length) {
    const len = png.readUInt32BE(off), type = png.toString("ascii", off + 4, off + 8);
    const body = png.subarray(off + 8, off + 8 + len);
    if (type === "IHDR") { width = body.readUInt32BE(0); height = body.readUInt32BE(4); channels = body[9] === 6 ? 4 : 3; }
    if (type === "IDAT") idat.push(body);
    off += 12 + len;
  }
  const raw = inflateSync(Buffer.concat(idat)), stride = width * channels, pixels = Buffer.alloc(height * stride);
  for (let y = 0; y < height; y++) {
    const filter = raw[y * (stride + 1)], line = raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1));
    for (let x = 0; x < stride; x++) {
      const a = x >= channels ? pixels[y * stride + x - channels] : 0, b = y ? pixels[(y - 1) * stride + x] : 0;
      const c = x >= channels && y ? pixels[(y - 1) * stride + x - channels] : 0;
      const p = a + b - c, pa = Math.abs(p - a), pb = Math.abs(p - b), pc = Math.abs(p - c);
      const pred = [0, a, b, (a + b) >> 1, pa <= pb && pa <= pc ? a : pb <= pc ? b : c][filter];
      pixels[y * stride + x] = (line[x] + pred) & 255;
    }
  }
  const colours = new Set();
  let lit = 0;
  for (let i = 0; i < pixels.length; i += channels) {
    if (pixels[i] + pixels[i + 1] + pixels[i + 2] > 30) lit++;
    if (colours.size < 64) colours.add((pixels[i] << 16) | (pixels[i + 1] << 8) | pixels[i + 2]);
  }
  return {lit: lit / (width * height), colours: colours.size};
}

const failures = [];

// Multiplayer between a browser and the desktop through a Free Direct server (--relay).
async function relayCheck() {
  const {name, dir} = options.programs[0];
  const net = await import("node:net");
  const probe = net.createServer();
  probe.listen(0, "127.0.0.1");
  await once(probe, "listening");
  const port = probe.address().port;
  probe.close();
  const server = spawn(path.resolve(options.relay), ["--host", "127.0.0.1", "--port", String(port)], {stdio: ["ignore", "pipe", "pipe"]});
  browsers.push(server);
  let serverLog = "";
  server.stdout.on("data", (c) => serverLog += c);
  server.stderr.on("data", (c) => serverLog += c);
  for (let i = 0; i < 100 && !/listening/.test(serverLog); i++) await new Promise((r) => setTimeout(r, 100));
  const ws = `ws://127.0.0.1:${port}`;
  const page = await launch(name + "-relay");
  await page.goto(`${base}/index.html?program=${name}&test-import=/test-data/${name}&test-play=1` +
                  `&server=${encodeURIComponent(ws)}&test-input=/test-input/sb1_browser_host.input`);
  await page.waitFor("!document.querySelector('#canvas').hidden", 240000);
  await page.waitFor("!document.querySelector('#room').hidden", 120000);
  const room = await page.evaluate("document.querySelector('#room-id').textContent");
  const link = await page.evaluate("document.querySelector('#room-link').href");
  console.log(`${name}: the browser created room ${room}; link ${link}`);
  if (!link.includes("room=" + room)) failures.push("the room link does not name the room");
  const user = path.join(output, "relay-client-user");
  await mkdir(user, {recursive: true});
  const client = spawn(path.resolve(options["relay-client"]), ["--data", path.resolve(dir), "--user", user, "--windowed",
    "--net-server", ws, "--net-room", room, "--input", path.join(inputs, "sb1_browser_client.input"),
    "--frames", "1300", "--trace", path.join(output, "relay-client.trace")],
    {stdio: ["ignore", "pipe", "pipe"], env: {...process.env, SDL_VIDEODRIVER: "offscreen", SDL_AUDIODRIVER: "dummy"}});
  browsers.push(client);
  let clientLog = "";
  client.stdout.on("data", (c) => clientLog += c);
  client.stderr.on("data", (c) => clientLog += c);
  const exited = await Promise.race([once(client, "exit").then(([code]) => code), new Promise((r) => setTimeout(() => r("timeout"), 180000))]);
  await writeFile(path.join(output, "relay-client.log"), clientLog);
  await writeFile(path.join(output, "relay-server.log"), serverLog);
  const browserLog = await page.evaluate("document.querySelector('#log').textContent");
  await writeFile(path.join(output, "relay-browser-log.txt"), browserLog);
  await page.canvasShot("relay-play");
  const trace = await readFile(path.join(output, "relay-client.trace"), "utf8").catch(() => "");
  const clientPlayed = trace.split("\n").some((line) => line.split(" ")[1] === "1525");
  console.log(`native client: exit ${exited}; joined: ${clientLog.includes("joined session")}; reached PLAY: ${clientPlayed}`);
  console.log(`browser host: created session: ${browserLog.includes("created session")}; START: ${browserLog.includes("START")}`);
  if (exited !== 0) failures.push(`the native client exited with ${exited} (see relay-client.log)`);
  if (!clientLog.includes("server room joined") || !clientLog.includes("joined session")) failures.push("the native client did not join the browser's session");
  if (!clientPlayed) failures.push("the native client never reached PLAY");
  if (!browserLog.includes("created session") || !browserLog.includes("START")) failures.push("the browser did not host and start the game");
  if (page.errors.length) failures.push(`${page.errors.length} JavaScript exception(s): ` + JSON.stringify(page.errors[0]).slice(0, 400));
}

const alarm = setTimeout(() => { console.error("web_check exceeded 15 minutes"); cleanup(); process.exit(1); }, 900000);
function cleanup() { for (const b of browsers) b.kill("SIGKILL"); http.close(); }
try {
  if (options.relay) await relayCheck();
  else for (const {name} of options.programs) {
    const page = await launch(name);
    const url = `${base}/index.html?program=${name}`;
    // 1. First visit: import from the test listing, then play.
    await page.goto(`${url}&test-import=/test-data/${name}&test-play=1`);
    await page.waitFor("!document.querySelector('#canvas').hidden", 240000);
    await page.waitFor(`document.querySelector('#log').textContent.includes('${name} ')`, 60000);
    await new Promise((r) => setTimeout(r, 8000));
    const title = variety(await page.canvasShot("title"));
    console.log(`${name}: title screen ${(title.lit * 100).toFixed(0)} % lit, ${title.colours}+ colours`);
    if (title.lit < 0.3 || title.colours < 16) failures.push(`${name}: title screen not drawn`);
    if (options.attract) {
      await new Promise((r) => setTimeout(r, 60000));
      const demo = variety(await page.canvasShot("attract"));
      console.log(`${name}: after 60 s ${(demo.lit * 100).toFixed(0)} % lit, ${demo.colours}+ colours`);
    }
    // 2. Reload: the files come from IndexedDB now.
    await page.goto(`${url}&test-play=1`);
    await page.waitFor("!document.querySelector('#canvas').hidden", 120000);
    const status = await page.evaluate("document.querySelector('#status').textContent");
    await new Promise((r) => setTimeout(r, 8000));
    const again = variety(await page.canvasShot("reloaded"));
    console.log(`${name}: after reload (stored files) ${(again.lit * 100).toFixed(0)} % lit`);
    if (again.lit < 0.3) failures.push(`${name}: not started from the stored files (${status})`);
    const log = await page.evaluate("document.querySelector('#log').textContent");
    await writeFile(path.join(output, `${name}-log.txt`), log);
    if (page.errors.length) failures.push(`${name}: ${page.errors.length} JavaScript exception(s): ` +
      JSON.stringify(page.errors[0]).slice(0, 400));
  }
} catch (error) {
  failures.push(String(error.stack || error));
} finally {
  clearTimeout(alarm);
  cleanup();
}
console.log(failures.length ? "FAIL\n" + failures.join("\n") : options.relay
  ? "PASS: a browser and a native program played one game through a Free Direct server"
  : "PASS: every program imported its files, started, and started again from the stored files");
process.exit(failures.length ? 1 : 0);
