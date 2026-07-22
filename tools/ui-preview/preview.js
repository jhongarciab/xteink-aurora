(() => {
  "use strict";

  const defaults = { device: "x3", screen: "home", title: "The Left Hand of Darkness", selectedRow: 0, darkMode: false, showGrid: false };
  const devices = {
    x3: { width: 528, height: 792, label: "Xteink X3" },
    x4: { width: 480, height: 800, label: "Xteink X4" }
  };
  const $ = (id) => document.getElementById(id);
  const controls = ["device", "screen", "title", "selectedRow", "darkMode", "showGrid"];

  function escapeHtml(value) {
    return value.replace(/[&<>"']/g, (character) => ({
      "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"
    })[character]);
  }

  function readState() {
    return {
      device: $("device").value,
      screen: $("screen").value,
      title: $("title").value.trim() || defaults.title,
      selectedRow: Number($("selectedRow").value),
      darkMode: $("darkMode").checked,
      showGrid: $("showGrid").checked
    };
  }

  function shortcutIcon(type) {
    const paths = {
      folder: '<path d="M3 7h7l2 3h9v10H3z"/><path d="M3 10h18"/>',
      wifi: '<path d="M3 9c5-4 13-4 18 0"/><path d="M6 13c3-3 9-3 12 0"/><path d="M9.5 16.5c1.4-1.2 3.6-1.2 5 0"/><path d="M12 20h.01"/>',
      settings: '<circle cx="12" cy="12" r="3"/><path d="M12 2v3M12 19v3M2 12h3M19 12h3M5 5l2 2M17 17l2 2M19 5l-2 2M7 17l-2 2"/>',
      library: '<path d="M4 4h4v16H4zM10 4h4v16h-4zM16 5l3-1 3 15-3 1z"/>',
      apps: '<circle cx="7" cy="7" r="2"/><circle cx="17" cy="7" r="2"/><circle cx="7" cy="17" r="2"/><circle cx="17" cy="17" r="2"/>'
    };
    return `<svg viewBox="0 0 24 24" aria-hidden="true">${paths[type]}</svg>`;
  }

  function firmwareHeader(title, context = "", showVersion = false) {
    return `<header class="fw-header">
      <span class="fw-header-date">22/07/2026 14:35</span><i class="fw-header-battery"></i><span class="fw-header-battery-text">84%</span>
      <div class="fw-header-title">${title}${context ? `<span class="fw-header-context">/ ${context}</span>` : ""}</div>
      ${showVersion ? '<span class="fw-version">1.3.0</span>' : ""}
    </header>`;
  }

  function listIcon(type) {
    return `<span class="list-icon">${shortcutIcon(type)}</span>`;
  }

  function renderLyraHome(state) {
    const shortcutTypes = ["folder", "wifi", "settings", "library", "apps"];
    const selectedShortcut = state.selectedRow - 1;
    const bookTitle = escapeHtml(state.title);
    return `<div class="lyra-home${state.selectedRow === 0 ? " carousel-selected" : ""}">
      <div class="lyra-topline"><span class="lyra-power"><i class="lyra-battery"></i><span>84%</span></span><span>22/07/2026 14:35</span></div>
      <div class="carousel-zone">
        <div class="cover-tile side left"><div class="cover-fallback">A Wizard<br>of Earthsea</div></div>
        <div class="cover-tile side right"><div class="cover-fallback">The<br>Dispossessed</div></div>
        <div class="cover-tile center">
          <div class="cover-fallback">${bookTitle}</div>
          <span class="progress-badge">67%</span>
        </div>
        <div class="carousel-copy">
          <div class="carousel-dots"><i></i><i class="active"></i><i></i></div>
          <div class="carousel-author">Ursula K. Le Guin</div>
          <div class="carousel-title">${bookTitle}</div>
        </div>
      </div>
      <div class="shortcut-strip">
        ${shortcutTypes.map((type, index) => `<div class="shortcut-tile${selectedShortcut === index ? " selected" : ""}"><span class="shortcut-icon">${shortcutIcon(type)}</span></div>`).join("")}
      </div>
    </div>`;
  }

  function renderReadingStats(state) {
    const books = [
      ["The Left Hand of Darkness", "Ursula K. Le Guin", "67%", "12h 08m", 67],
      ["A Wizard of Earthsea", "Ursula K. Le Guin", "34%", "4h 22m", 34],
      ["The Dispossessed", "Ursula K. Le Guin", "18%", "2h 41m", 18]
    ];
    const cards = [["Streak", "12"], ["Max Streak", "19"], ["Daily Goal", "42m / 60m", true], ["Reading Time", "83h 14m"], ["Books Finished", "9"], ["Books Started", "14"]];
    return `<div class="firmware-screen">
      ${firmwareHeader("Reading Stats")}
      <div class="stats-summary">${cards.map((card) => `<div class="metric-card"><strong>${card[1]}</strong><span>${card[0]}</span>${card[2] ? '<i class="metric-check">✓</i>' : ""}</div>`).join("")}</div>
      <div class="details-button${state.selectedRow === 0 ? " selected" : ""}">More Details</div>
      <div class="stats-subheader"><span>Started Books (14)</span><span>1/5</span></div>
      <div class="book-list">${books.map((book, index) => `<div class="book-row${state.selectedRow === index + 1 ? " selected" : ""}">
        <strong>${book[0]}</strong><small>${book[1]}</small><span class="book-meta"><b>${book[2]}</b><small>${book[3]}</small></span>
        <span class="book-progress"><i style="width:${book[4]}%"></i></span></div>`).join("")}</div>
    </div>`;
  }

  function renderSyncDay(state) {
    const items = [
      ["Get Day", "22/07/2026", "Not connected", "wifi"],
      ["Set Date", "Manual", "", "library"],
      ["Auto Sync Day", "Automatic", "✓", "settings"],
      ["Choose WiFi", "Automatic", "", "settings"],
      ["Time Zone", "America / Bogotá", "", "settings"],
      ["Date Format", "DD/MM/YYYY", "", "library"]
    ];
    return `<div class="firmware-screen">
      ${firmwareHeader("Sync Day")}
      <div class="fw-list sync-list">${items.map((item, index) => `<div class="fw-list-row${state.selectedRow === index ? " selected" : ""}">
        ${listIcon(item[3])}<strong>${item[0]}</strong><small>${item[1]}</small>${item[2] ? `<span class="fw-list-value">${item[2]}</span>` : ""}</div>`).join("")}</div>
      <div class="sync-help">${state.selectedRow === 0 ? '<div class="status">Connect Wi-Fi to set the current date and time.</div>' : ""}</div>
    </div>`;
  }

  function renderSettings(state) {
    const categories = ["Display", "Reader", "Controls", "System", "Apps"];
    const items = [
      ["Sleep Screen", "Dark"], ["Sleep Cover Mode", "Fit"], ["Sleep Cover Filter", "None"],
      ["Clean Sleep Refresh", "On"], ["Hide Battery", "Never"], ["Refresh Frequency", "5 pages"],
      ["UI Theme", "Lyra Carousel"], ["Home Book Source", "Recents"], ["Anti-Ghosting", "Off"],
      ["Dark Mode", "Off"], ["Sunlight Fading Fix", "Off"]
    ];
    return `<div class="firmware-screen">
      ${firmwareHeader("Settings", "Display", true)}
      <div class="settings-tabs${state.selectedRow === 0 ? " focused" : ""}">${categories.map((category, index) => `<span class="${index === 0 ? "active" : ""}">${category}</span>`).join("")}</div>
      <div class="fw-list settings-list">${items.map((item, index) => `<div class="fw-list-row${state.selectedRow === index + 1 ? " selected" : ""}"><strong>${item[0]}</strong><span class="fw-list-value">${item[1]}</span></div>`).join("")}</div>
    </div>`;
  }

  function renderScreen(state) {
    if (state.screen === "home") {
      return renderLyraHome(state);
    }
    if (state.screen === "stats") {
      return renderReadingStats(state);
    }
    if (state.screen === "sync") {
      return renderSyncDay(state);
    }
    return renderSettings(state);
  }

  function render() {
    const state = readState();
    const maxSelectedRows = { home: 5, stats: 3, sync: 5, settings: 11 };
    const maxSelectedRow = maxSelectedRows[state.screen];
    if (state.selectedRow > maxSelectedRow) {
      state.selectedRow = maxSelectedRow;
      $("selectedRow").value = String(maxSelectedRow);
    }
    $("selectedRow").max = String(maxSelectedRow);
    const device = devices[state.device];
    const display = $("display");
    const frame = $("deviceFrame");
    display.style.setProperty("--screen-width", device.width);
    display.style.setProperty("--screen-height", device.height);
    display.classList.toggle("dark", state.darkMode);
    display.classList.toggle("grid", state.showGrid);
    frame.className = `device-frame ${state.device}`;
    display.innerHTML = renderScreen(state);
    $("resolution").textContent = `${device.width} × ${device.height} · ${device.label}`;
    localStorage.setItem("xteinkAuroraUiPreview", JSON.stringify(state));
  }

  function applyState(state) {
    const merged = { ...defaults, ...state };
    $("device").value = merged.device;
    $("screen").value = merged.screen;
    $("title").value = merged.title;
    $("selectedRow").value = merged.selectedRow;
    $("darkMode").checked = merged.darkMode;
    $("showGrid").checked = merged.showGrid;
    render();
  }

  controls.forEach((id) => $(id).addEventListener("input", render));
  $("reset").addEventListener("click", () => applyState(defaults));
  $("download").addEventListener("click", () => {
    const state = readState();
    const blob = new Blob([JSON.stringify(state, null, 2)], { type: "application/json" });
    const link = document.createElement("a");
    link.href = URL.createObjectURL(blob);
    link.download = `aurora-${state.device}-${state.screen}-layout.json`;
    link.click();
    URL.revokeObjectURL(link.href);
  });

  let saved = {};
  try { saved = JSON.parse(localStorage.getItem("xteinkAuroraUiPreview") || "{}"); } catch (_) { saved = {}; }
  applyState(saved);
})();
