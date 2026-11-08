/* global FS, IDBFS, ENV */
"use strict";
const $ = (id) => document.getElementById(id);
const game = document.body.dataset.game;
const titles = {"sb1-blupi": "Speedy Blupi 1", "sb1-eggbert": "Speedy Eggbert 1",
  "sb2-blupi": "Speedy Blupi 2", "sb2-eggbert": "Speedy Eggbert 2"};
$("title").textContent = titles[game];
document.title = titles[game];
let ready = false, running = false;
let syncPromise = Promise.resolve();
const status = (message) => { $("status").textContent = message; };
const log = (message) => {
  $("log").textContent = ($("log").textContent + message + "\n").slice(-16000);
};
var Module = {
  canvas: $("canvas"), noInitialRun: true,
  print: log, printErr: log,
  setStatus: (message) => { if (message && !ready && !window.BlupiWebStartup.failed) status(message); },
  preRun: [() => {
    ENV.FREE_API_SOUNDFONT = "/soundfont/default.sf2";
    ENV.FREE_DIRECT_ENABLE_VSYNC = "0";
  }],
  onRuntimeInitialized: async () => {
    try {
      FS.mkdirTree("/persistent/" + game);
      FS.mount(IDBFS, {}, "/persistent/" + game);
      await new Promise((resolve, reject) => FS.syncfs(true, (error) => error ? reject(error) : resolve()));
      FS.mkdirTree("/persistent/" + game + "/data");
      FS.symlink("/persistent/" + game + "/data", "/data");
      if (!FS.analyzePath("/data_seed/config.def").exists) throw new Error("Bundled game data is missing. Rebuild the web package.");
      seedData("/data_seed", "/data");
      status("Preparing music…");
      const font = await fetch(new URL("../soundfont/default.sf2", location.href));
      if (!font.ok) throw new Error("Bundled SoundFont could not be loaded (HTTP " + font.status + ").");
      FS.mkdirTree("/soundfont");
      FS.writeFile("/soundfont/default.sf2", new Uint8Array(await font.arrayBuffer()));
      ready = true;
      $("start").disabled = false;
      $("import").disabled = false;
      $("export").disabled = false;
      status("Ready to play — " + titles[game]);
    } catch (error) { window.BlupiWebStartup.fail(error); }
  },
  onAbort: (reason) => window.BlupiWebStartup.fail(new Error("Game stopped: " + reason)),
  onExit: (code) => { running = false; save().catch(log); status("Game closed (" + code + "). Reload to play again."); }
};

function seedData(source, destination) {
  for (const name of FS.readdir(source)) {
    if (name === "." || name === "..") continue;
    const input = source + "/" + name, output = destination + "/" + name;
    if (FS.isDir(FS.stat(input).mode)) {
      FS.mkdirTree(output);
      seedData(input, output);
    } else if (!FS.analyzePath(output).exists) {
      // Bundled levels seed the browser store; existing saves take precedence.
      FS.writeFile(output, FS.readFile(input));
    }
  }
}
function save() {
  if (!ready) return Promise.resolve();
  syncPromise = syncPromise.catch(() => {}).then(() => new Promise((resolve, reject) => {
    FS.syncfs(false, (error) => error ? reject(error) : resolve());
  }));
  return syncPromise;
}
$("start").onclick = async () => {
  if (!ready || running) return;
  $("start").disabled = true;
  try {
    // Change only the virtual copy. -nocd is the games' own command-line flag.
    let config = FS.readFile("/data/config.def", {encoding: "utf8"});
    for (const [key, value] of [["FullScreen", "0"], ["CD-Rom", "C:\\"], ["CDAudio", "0"]]) {
      const expression = new RegExp("^" + key + "=[^\\r\\n]*", "mi");
      config = expression.test(config) ? config.replace(expression, () => key + "=" + value) : config + "\r\n" + key + "=" + value + "\r\n";
    }
    FS.writeFile("/data/config.def", config);
    await save();
    running = true;
    $("setup").hidden = true;
    for (const id of ["fullscreen", "save", "export"]) $(id).disabled = false;
    $("import").disabled = true;
    $("canvas").focus();
    status("Running — " + titles[game]);
    setInterval(() => save().catch((error) => status("Save failed: " + error.message)), 3000);
    Module.callMain([]);
  } catch (error) { status(error.message); if (!running) $("start").disabled = false; }
};
$("fullscreen").onclick = () => {
  // SDL installs this bridge when it creates the window. A direct DOM
  // requestFullscreen bypasses SDL's framebuffer sizing and restore callback.
  try {
    Module.requestFullscreen(false, true);
    $("canvas").focus();
  } catch (error) { log("Fullscreen failed: " + error.message); }
};
$("save").onclick = () => save().then(() => status("Saved in this browser.")).catch((error) => status("Save failed: " + error.message));
document.addEventListener("visibilitychange", () => { if (document.hidden) save().catch(log); });

function dataFiles(directory = "/data", prefix = "") {
  const result = {};
  for (const name of FS.readdir(directory)) {
    if (name === "." || name === "..") continue;
    const path = directory + "/" + name;
    if (FS.isDir(FS.stat(path).mode)) Object.assign(result, dataFiles(path, prefix + name + "/"));
    else {
      const bytes = FS.readFile(path);
      let binary = "";
      for (let i = 0; i < bytes.length; i += 16384) binary += String.fromCharCode(...bytes.subarray(i, i + 16384));
      result[prefix + name] = btoa(binary);
    }
  }
  return result;
}
$("export").onclick = () => {
  try {
    const content = JSON.stringify({format: "blupi-recon-saves-v1", game, files: dataFiles()});
    const url = URL.createObjectURL(new Blob([content], {type: "application/json"}));
    const link = document.createElement("a"); link.href = url; link.download = game + "-saves.json"; link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  } catch (error) { status("Export failed: " + error.message); }
};
$("import").onchange = async () => {
  try {
    const backup = JSON.parse(await $("import").files[0].text());
    if (backup.format !== "blupi-recon-saves-v1" || backup.game !== game || !backup.files || typeof backup.files !== "object") throw new Error("Save file is for a different game or format.");
    // Validate all entries before writing anything into DATA.
    const entries = Object.entries(backup.files).map(([path, encoded]) => {
      if (!/^[a-z0-9_. /-]+$/i.test(path) || path.split("/").some((part) => !part || part === "." || part === "..") || typeof encoded !== "string") throw new Error("Invalid save path.");
      return [path, Uint8Array.from(atob(encoded), (character) => character.charCodeAt(0))];
    });
    for (const [path, bytes] of entries) {
      const slash = path.lastIndexOf("/");
      if (slash >= 0) FS.mkdirTree("/data/" + path.slice(0, slash));
      FS.writeFile("/data/" + path, bytes);
    }
    await save(); status("Saves imported. Press Start game to play.");
  } catch (error) { status("Import failed: " + error.message); }
};
