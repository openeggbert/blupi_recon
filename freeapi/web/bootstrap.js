// Start the threaded runtime only after the document is cross-origin isolated.
// The worker's default scope covers this package, including all four game pages.
(() => {
  "use strict";
  const root = new URL("./", document.currentScript.src);
  const reloadKey = "blupi-isolation-reload:" + root.pathname;
  const startup = window.BlupiWebStartup = {failed: false};
  const status = message => { document.getElementById("status").textContent = message; };
  startup.fail = error => {
    if (startup.failed) return;
    startup.failed = true;
    status("Could not start game: " + (error?.message || String(error)));
    for (const id of ["start", "fullscreen", "save", "export", "import"]) {
      document.getElementById(id).disabled = true;
    }
    const log = document.getElementById("log");
    log.textContent += (error?.stack || String(error)) + "\n";
    const retry = document.createElement("button");
    retry.textContent = "Retry loading";
    retry.onclick = () => {
      try { sessionStorage.removeItem(reloadKey); } catch {}
      location.reload();
    };
    document.getElementById("setup").hidden = false;
    document.getElementById("setup").append(retry);
  };
  window.addEventListener("error", event => startup.fail(event.error || event.message));
  window.addEventListener("unhandledrejection", event => startup.fail(event.reason));

  const timeout = (promise, milliseconds, message) => {
    let timer;
    return Promise.race([promise, new Promise((_, reject) => {
      timer = setTimeout(() => reject(new Error(message)), milliseconds);
    })]).finally(() => clearTimeout(timer));
  };
  const activated = registration => new Promise((resolve, reject) => {
    const watch = () => {
      if (registration.active?.state === "activated") return resolve();
      const worker = registration.installing || registration.waiting || registration.active;
      if (!worker) return reject(new Error("Isolation worker was not installed. Please retry."));
      if (worker.state === "redundant") return reject(new Error("Isolation worker installation failed. Please retry."));
      worker.addEventListener("statechange", watch, {once: true});
    };
    registration.addEventListener("updatefound", watch, {once: true});
    watch();
  });
  const isolate = async () => {
    if (window.crossOriginIsolated && typeof SharedArrayBuffer !== "undefined") {
      try { sessionStorage.removeItem(reloadKey); } catch {}
      return true;
    }
    if (!window.isSecureContext) throw new Error("Open this game over HTTPS or localhost.");
    if (!navigator.serviceWorker) throw new Error("Allow service workers, or open the game in a regular browser window.");
    // A failed controlled navigation must not cause an endless reload loop.
    const previous = Number(sessionStorage.getItem(reloadKey));
    if (previous && Date.now() - previous < 60000) {
      throw new Error("This browser could not enable WebAssembly threads. Allow service workers and select Retry loading.");
    }
    status("Preparing browser for game…");
    const registration = await timeout(navigator.serviceWorker.register(
      new URL("coi-serviceworker.js", root), {scope: root.pathname, updateViaCache: "none"}
    ), 15000, "Isolation worker registration timed out. Please retry.");
    await timeout(activated(registration), 15000, "Isolation worker activation timed out. Please retry.");
    sessionStorage.setItem(reloadKey, String(Date.now()));
    status("Preparing game — reloading once…");
    // A browser/devtools reload can bypass a service worker. Make a fresh
    // navigation after this document has finished loading, so the active
    // registration handles the replacement document (without another history entry).
    await new Promise(resolve => {
      if (document.readyState === "complete") setTimeout(resolve, 0);
      else window.addEventListener("load", () => setTimeout(resolve, 0), {once: true});
    });
    location.replace(location.href);
    return false;
  };
  const script = filename => new Promise((resolve, reject) => {
    const element = document.createElement("script");
    element.src = new URL(filename, location.href);
    element.onload = resolve;
    element.onerror = () => reject(new Error("Could not load " + filename + ". Please retry."));
    document.body.append(element);
  });
  (async () => {
    if (!await isolate() || startup.failed) return;
    status("Loading game data…");
    await script("player.js");
    if (!startup.failed) await script("game.js");
  })().catch(startup.fail);
})();
