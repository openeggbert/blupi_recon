// Web loader of the SDL3 port (port/docs/WEB.md). It contains no game data: the player picks the
// folder of their own copy of the game once; the needed files are copied into the browser's storage
// (IndexedDB, through Emscripten's IDBFS mounted at /persist) and the program starts from there.
// Saves and settings are kept beside them. Nothing is uploaded anywhere.
"use strict";
(() => {   // its own scope: the program's script defines globals such as FS

const PROGRAMS = {
  "blupi-sdl3": {title: "Speedy Blupi / Speedy Eggbert", folder: "Speedy Blupi 1 or Speedy Eggbert 1",
    titleImage: "image08/init.blp"},
  "blupi2-sdl3": {title: "Speedy Blupi 2 / Speedy Eggbert 2", folder: "Speedy Blupi 2 or Speedy Eggbert 2",
    titleImage: "image08/init.blp"},
  "eggbert-demo-sdl3": {title: "Speedy Eggbert Demo", folder: "the Speedy Eggbert Demo (eGames, 1999)",
    titleImage: "image08/init.blp"},
  "eggbert-value-sdl3": {title: "Speedy Eggbert SE Valueware", folder: "Speedy Eggbert SE Valueware v1.0",
    titleImage: "image08/init.blp"},
  "blupi-demo-sdl3": {title: "Speedy Blupi Demo (June 1998)", folder: "the Speedy Blupi Demo v0.8 (June 1998)",
    titleImage: "image/init.blp"},
};
// The game's folders the port reads (the CD's language folders hold a SOUND folder of their own).
const GAME_DIRS = new Set(["data", "image", "image08", "image16", "sound", "music"]);
const MAX_EXE = 4 << 20;   // executables are read for their size and hash only (the installed language)

const params = new URLSearchParams(location.search);
// The Free Direct server the game's server screen offers until the player has used another one
// (port/docs/MULTIPLAYER.md; on an https:// page a plain name means wss:// on port 443).
const DEFAULT_SERVER = "fds.openeggbert.com";
const program = params.get("program");
const info = PROGRAMS[program];
const $ = (id) => document.getElementById(id);

function status(text) { $("status").textContent = text; }
function log(text) { const l = $("log"); l.textContent += text + "\n"; l.scrollTop = l.scrollHeight; }

const ROOT = "/persist/" + program;
const DATA = ROOT + "/data";
const USER = ROOT + "/user";
let fs = null;
let syncing = false, pendingSync = false;

function sync(populate = false) {
  return new Promise((resolve) => {
    if (syncing && !populate) { pendingSync = true; resolve(); return; }
    syncing = true;
    fs.syncfs(populate, (error) => {
      syncing = false;
      if (error) log("storage: " + error);
      if (pendingSync) { pendingSync = false; sync().then(resolve); } else resolve();
    });
  });
}

function exists(path) { try { fs.stat(path); return true; } catch (e) { return false; } }

function mkdirTree(path) {
  let current = "";
  for (const part of path.split("/").filter(Boolean)) {
    current += "/" + part;
    if (!exists(current)) fs.mkdir(current);
  }
}

function removeTree(path) {
  if (!exists(path)) return;
  if (fs.isDir(fs.stat(path).mode)) {
    for (const name of fs.readdir(path)) if (name !== "." && name !== "..") removeTree(path + "/" + name);
    fs.rmdir(path);
  } else fs.unlink(path);
}

// Every file below dir, as lower-case relative paths.
function listLower(dir, prefix = "", out = new Set()) {
  if (!exists(dir)) return out;
  for (const name of fs.readdir(dir)) {
    if (name === "." || name === "..") continue;
    const full = dir + "/" + name;
    if (fs.isDir(fs.stat(full).mode)) listLower(full, prefix + name.toLowerCase() + "/", out);
    else out.add(prefix + name.toLowerCase());
  }
  return out;
}

function stored() {
  const files = listLower(DATA);
  return files.has("data/config.def") && files.has(info.titleImage);
}

// The game's root among the picked files: the shortest folder holding DATA/config.def and the title
// image (the player may pick the game folder itself or a folder that contains it, like a CD).
function findRoot(paths) {
  const lower = new Set(paths.map((p) => p.toLowerCase()));
  let best = null;
  for (const p of paths) {
    const l = p.toLowerCase();
    if (!l.endsWith("data/config.def")) continue;
    const root = p.slice(0, p.length - "data/config.def".length);
    if (!lower.has(root.toLowerCase() + info.titleImage)) continue;
    if (best === null || root.length < best.length) best = root;
  }
  return best;
}

function wanted(relative, size) {
  const parts = relative.toLowerCase().split("/");
  if (parts[parts.length - 1].endsWith(".avi")) return false;
  if (parts.length === 1) return /\.(exe)$/.test(parts[0]) && size <= MAX_EXE;
  if (GAME_DIRS.has(parts[0])) return true;
  // A CD's language folder: its SOUND folder and its executable.
  if (parts.length >= 3 && GAME_DIRS.has(parts[1])) return parts[1] === "sound";
  return parts.length === 2 && /\.(exe)$/.test(parts[1]) && size <= MAX_EXE;
}

// files: [{path, size, read: () => Promise<ArrayBuffer>}] with paths relative to what was picked.
async function importFiles(files) {
  const root = findRoot(files.map((f) => f.path));
  if (root === null) {
    status(`This folder does not contain ${info.folder} (a DATA folder with config.def and the game's ` +
           `images). Choose the game's own folder.`);
    return false;
  }
  const chosen = files.filter((f) => f.path.startsWith(root)).map((f) => ({...f, relative: f.path.slice(root.length)}))
    .filter((f) => wanted(f.relative, f.size));
  const total = chosen.reduce((sum, f) => sum + f.size, 0);
  removeTree(DATA);
  mkdirTree(DATA);
  let done = 0;
  for (const f of chosen) {
    const target = DATA + "/" + f.relative;
    mkdirTree(target.slice(0, target.lastIndexOf("/")));
    fs.writeFile(target, new Uint8Array(await f.read()));
    done += f.size;
    status(`Copying the game's files into the browser: ${Math.round(100 * done / Math.max(1, total))} %`);
  }
  await sync();
  status(`${chosen.length} files (${(total / 1048576).toFixed(1)} MB) kept in this browser.`);
  return true;
}

async function pickFolder(event) {
  const list = Array.from(event.target.files);
  if (!list.length) return;
  $("pick").disabled = true;
  const files = list.map((f) => ({path: f.webkitRelativePath.slice(f.webkitRelativePath.indexOf("/") + 1),
    size: f.size, read: () => f.arrayBuffer()}));
  if (await importFiles(files)) ready();
  $("pick").disabled = false;
}

// Local tests only (port/tools/web_check.mjs): import from a server's listing instead of a folder.
async function importFromUrl(url) {
  const listing = await (await fetch(url + "/index.json")).json();
  const files = listing.map((f) => ({path: f.path, size: f.size,
    read: async () => (await fetch(url + "/" + f.path.split("/").map(encodeURIComponent).join("/"))).arrayBuffer()}));
  return importFiles(files);
}

function ready() {
  const has = stored();
  $("play").disabled = !has;
  $("forget").disabled = !has;
  if (has) status("Your game's files are in this browser. Press Play.");
  else status(`Choose the folder of your own copy of ${info.folder}. Its files stay in this browser.`);
}

async function loadSoundFont() {
  if (exists("/soundfont.sf2")) return;
  try {
    const response = await fetch("soundfont/soundfont.sf2");
    if (!response.ok) throw new Error(response.status);
    fs.writeFile("/soundfont.sf2", new Uint8Array(await response.arrayBuffer()));
  } catch (error) {
    log("no SoundFont (" + error + "): music is off");
  }
}

let running = false;
async function play() {
  if (running || !stored()) return;
  running = true;
  $("controls").hidden = true;
  status("Loading…");
  await loadSoundFont();
  mkdirTree(USER);
  $("canvas").hidden = false;
  $("canvas").focus();
  status("");
  setInterval(() => sync(), 5000);   // saves and settings go to IndexedDB
  const args = ["--data", DATA, "--user", USER];
  // Multiplayer through a Free Direct server (port/docs/MULTIPLAYER.md): ?server= and ?room= fill the
  // game's server screen, so a link can bring a friend straight to a room.
  if (params.get("server")) args.push("--net-server", params.get("server"));
  else args.push("--net-server-default", DEFAULT_SERVER);
  if (params.get("room")) args.push("--net-room", params.get("room"));
  const testInput = params.get("test-input");   // tests: a scripted input (port/tools/web_check.mjs)
  if (testInput) {
    const response = await fetch(testInput);
    fs.writeFile("/test.input", new Uint8Array(await response.arrayBuffer()));
    args.push("--input", "/test.input");
  }
  Module.callMain(args);
}

// The game created a room on the server: show a link the other players can open.
function roomCreated(room, server) {
  console.log("room created: " + room);
  const link = new URL(location.href);
  for (const key of [...link.searchParams.keys()]) if (key.startsWith("test-")) link.searchParams.delete(key);
  link.searchParams.set("server", server);
  link.searchParams.set("room", room);
  $("room-id").textContent = room;
  $("room-link").href = link.href;
  $("room-link").textContent = link.href;
  $("room").hidden = false;
}

function start() {
  window.Module = {
    noInitialRun: true,
    canvas: $("canvas"),
    blupiRoomCreated: roomCreated,
    print: (text) => log(text),
    printErr: (text) => log(text),
    locateFile: (path) => program + "/" + path,
    preRun: [() => {
      fs = Module.FS;
      fs.mkdir("/persist");
      fs.mount(fs.filesystems.IDBFS, {}, "/persist");
      Module.addRunDependency("idbfs");
      sync(true).then(() => Module.removeRunDependency("idbfs"));
    }],
    onRuntimeInitialized: async () => {
      $("pick-input").addEventListener("change", pickFolder);
      $("pick").addEventListener("click", () => $("pick-input").click());
      $("play").addEventListener("click", play);
      $("forget").addEventListener("click", async () => {
        removeTree(DATA);
        await sync();
        ready();
      });
      ready();
      const testImport = params.get("test-import");
      if (testImport && !stored()) await importFromUrl(testImport) && ready();
      if (params.has("test-play")) play();
    },
    onExit: async (code) => {
      await sync();
      $("canvas").hidden = true;
      status(code === 0 ? "The game has ended. Reload the page to play again."
                        : "The game stopped (" + code + "). See the log below; reload to try again.");
      $("log").hidden = false;
    },
  };
  window.addEventListener("pagehide", () => fs && sync());
  document.addEventListener("visibilitychange", () => fs && document.hidden && sync());
  const script = document.createElement("script");
  script.src = program + "/" + program + ".js";
  script.onerror = () => status("The program " + program + " is missing from this site.");
  document.body.appendChild(script);
}

// The page: the list of programs, or one program.
if (!info) {
  // The list of programs.
  $("chooser").hidden = false;
  for (const [name, p] of Object.entries(PROGRAMS)) {
    const a = document.createElement("a");
    a.href = "?program=" + name;
    a.innerHTML = `<b>${p.title}</b><span>${name}</span>`;
    $("programs").appendChild(a);
  }
} else {
  document.title = info.title + " — SDL3 port";
  $("player").hidden = false;
  $("heading").textContent = info.title;
  $("needs").textContent = info.folder;
  start();
}
})();
