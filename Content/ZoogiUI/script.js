/* =====================================================================
   ZOOGI · Roll Arena — UI logic
   ---------------------------------------------------------------------
   Structure (mirrors how the screens map to Unreal):
     DATA      → DataTables (DT_Cards, DT_Arenas, DT_Episodes, DT_Shop…)
     LAYOUT    → Canvas Panel slot positions on the 941×1672 canvas
     Bridge    → UI ⇄ engine events (works inside the UE WebBrowser widget)
     screens   → one render/bind function per WBP_* widget
   Engine → UI : window.ZoogiUI.*   (call from UE with ExecuteJavascript)
   UI → Engine : Bridge.emit(name, payload)  (see HANDOFF.md for the list)
   ===================================================================== */
(() => {
  "use strict";

  const W = 941, H = 1672;
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => [...r.querySelectorAll(s)];
  const fmt = (n) => n.toLocaleString("en-US");
  const el = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html != null) e.innerHTML = html; return e; };
  const pos = (e, x, y, w, h) => { e.style.setProperty("--x", x + "px"); e.style.setProperty("--y", y + "px"); if (w != null) e.style.setProperty("--w", w + "px"); if (h != null) e.style.setProperty("--h", h + "px"); return e; };
  const store = {
    get(k, d) { try { const v = localStorage.getItem("zoogi." + k); return v ? JSON.parse(v) : d; } catch { return d; } },
    set(k, v) { try { localStorage.setItem("zoogi." + k, JSON.stringify(v)); } catch { /* storage unavailable */ } },
  };

  /* ------------------------------------------------------------------
     BRIDGE  (UI → Unreal)
     Inside Unreal's WebBrowser widget, bind a UObject as "interface"
     (BindUObject("interface", MyBridge)). Its UFUNCTIONs appear as
     ue.interface.<lowercase name>. We call ue.interface.broadcast(name, json)
     if present, otherwise ue.interface.<name>(json).
  ------------------------------------------------------------------ */
  const Bridge = {
    log: [],
    emit(name, data = {}) {
      const payload = JSON.stringify(data);
      this.log.push({ t: Date.now(), name, data });
      try {
        const ui = window.ue && window.ue.interface;
        if (ui) {
          if (typeof ui.broadcast === "function") ui.broadcast(name, payload);
          else if (typeof ui[name.toLowerCase()] === "function") ui[name.toLowerCase()](payload);
        }
      } catch (e) { console.warn("[ZoogiUI] bridge error", e); }
      window.dispatchEvent(new CustomEvent("zoogi:" + name, { detail: data }));
      if (location.search.includes("debug")) console.log("[ZoogiUI]", name, data);
    },
  };

  /* ------------------------------------------------------------------
     DATA  (DataTables)
  ------------------------------------------------------------------ */
  const CATS = {
    all: { name: "All" }, infusion: { name: "Infusion" }, technique: { name: "Technique" }, defense: { name: "Defense" },
    utility: { name: "Utility" }, terrain: { name: "Terrain" }, control: { name: "Control" }, impact: { name: "Impact" },
  };
  const S_AREA = "assets/icons/stat-area.png", S_KB = "assets/icons/stat-knockback.png";
  const CARDS = [
    { id: "storm", name: "Storm", cost: 2, cat: "technique", desc: "Unleash a swirling tempest that hits all enemy Zoogies, dealing damage and pushing them back.", stats: [["Area Damage", "250% Power"], ["Knockback", "Medium"]] },
    { id: "bank-strike", name: "Bank Strike", cost: 3, cat: "impact", desc: "Ricochet off the arena wall and slam into the nearest rival with burning force.", stats: [["Impact Damage", "320% Power"], ["Bounces", "1"]] },
    { id: "guard", name: "Guard", cost: 2, cat: "defense", desc: "Raise a golden shield that absorbs the next hit and steadies your roll.", stats: [["Shield", "400 HP"], ["Duration", "6 sec"]] },
    { id: "orb-relay", name: "Orb Relay", cost: 3, cat: "utility", desc: "Launch linked orbs that bounce between allies, restoring energy on every pass.", stats: [["Energy Restore", "+1 / Ally"], ["Range", "Long"]] },
    { id: "heavy-roll", name: "Heavy Roll", cost: 4, cat: "impact", desc: "Turn to stone and crush everything in your path with unstoppable weight.", stats: [["Impact Damage", "380% Power"], ["Knockback", "High"]] },
    { id: "ember", name: "Ember", cost: 3, cat: "infusion", desc: "Infuse your shell with fire, leaving a burning trail behind every roll.", stats: [["Burn", "40 / sec"], ["Trail", "4 sec"]] },
    { id: "titan", name: "Titan", cost: 5, cat: "terrain", desc: "Grow to titanic size and shake the arena floor, staggering every Zoogi nearby.", stats: [["Quake Damage", "300% Power"], ["Stagger", "2 sec"]] },
    { id: "frost", name: "Frost", cost: 3, cat: "control", desc: "Encase the area in crystal ice, slowing enemy rolls to a crawl.", stats: [["Slow", "60%"], ["Duration", "4 sec"]] },
    { id: "precision", name: "Precision", cost: 3, cat: "technique", desc: "Lock onto a target and line up a perfect, critical strike.", stats: [["Crit Chance", "+100%"], ["Aim Assist", "Max"]] },
    { id: "momentum-steal", name: "Momentum Steal", cost: 4, cat: "control", desc: "Siphon the speed of the closest rival and add it to your own roll.", stats: [["Speed Stolen", "50%"], ["Duration", "5 sec"]] },
    { id: "crystal-collapse", name: "Crystal Collapse", cost: 5, cat: "terrain", desc: "Summon crystal spires that erupt beneath your foes, then shatter.", stats: [["Area Damage", "350% Power"], ["Spires", "5"]] },
    { id: "orb-magnet", name: "Orb Magnet", cost: 4, cat: "utility", desc: "Pull every energy orb and loose rival toward you with magnetic force.", stats: [["Pull Radius", "Large"], ["Duration", "3 sec"]] },
    { id: "double-impact", name: "Double Impact", cost: 4, cat: "impact", desc: "Strike twice in a blink — fire then frost — for a devastating combo.", stats: [["Hits", "2"], ["Damage", "2 × 180%"]] },
    { id: "anchor-guard", name: "Anchor Guard", cost: 3, cat: "defense", desc: "Drop anchor and become immovable, reflecting knockback to attackers.", stats: [["Knockback Immune", "Yes"], ["Reflect", "50%"]] },
    { id: "recovery-roll", name: "Recovery Roll", cost: 2, cat: "infusion", desc: "Roll through a healing bloom that restores you and nearby allies.", stats: [["Heal", "300 HP"], ["Allies", "Nearby"]] },
  ];
  const CARD = Object.fromEntries(CARDS.map((c) => [c.id, c]));
  const DECK_ART = new Set(CARDS.slice(0, 10).map((c) => c.id));
  const artDeck = (id) => `assets/cards/${id}${DECK_ART.has(id) ? "" : "-wide"}.jpg`;
  const artWide = (id) => `assets/cards/${id}-wide.jpg`;
  const typeIco = (id) => `assets/icons/type-${id}.png`;

  const ARENAS = [
    { id: "skyfall", name: "Skyfall Isles", img: "assets/arenas/skyfall-featured.jpg", desc: "Majestic floating islands where the wind guides the bravest Zoogies.",
      feats: [["Wind Gusts", "Strong winds can push Zoogies!", "assets/icons/feature-wind.png"], ["Floating Platforms", "Platforms move and shift.", "assets/icons/feature-platforms.png"], ["Bottomless Falls", "Watch your step!", "assets/icons/feature-falls.png"]] },
    { id: "frozen", name: "Frozen Ring", desc: "An icy coliseum where every roll slides further than you expect.", feats: [["Slick Ice", "Rolls slide farther."], ["Ice Shards", "Shards burst from the floor."], ["Frost Dome", "Freezing zones slow you."]] },
    { id: "volcanic", name: "Volcanic Pit", desc: "A molten crater ringed with eruptions. Keep moving or get scorched.", feats: [["Lava Flows", "Molten rivers burn on contact."], ["Eruptions", "Vents blast Zoogies upward."], ["Cracked Floor", "Tiles crumble over time."]] },
    { id: "pharaoh", name: "Pharaoh's Tomb", desc: "Ancient sands hide whirling dust devils and golden traps.", feats: [["Sandstorms", "Gusts curve your roll."], ["Pyramid Ramps", "Launch off the slopes."], ["Sun Beams", "Beams scorch the sand."]] },
    { id: "night", name: "Night Circuit", desc: "A neon speedway with boost lanes and electric hazards.", feats: [["Boost Pads", "Chevrons give a burst of speed."], ["Shock Zones", "Charged tiles stun you."], ["Jump Gates", "Gates teleport across the track."]] },
    { id: "meadow", name: "Meadow Sanctum", desc: "A peaceful glade where nature rewards the bold.", feats: [["Leaf Gusts", "Breezes nudge your path."], ["Overgrowth", "Vines slow heavy rolls."], ["Healing Blooms", "Flowers restore health."]] },
    { id: "cosmic", name: "Cosmic Platform", desc: "Roll among the stars where gravity bends around every planet.", feats: [["Gravity Wells", "Swirls pull you in."], ["Orbiting Planets", "Planets sweep the ring."], ["Star Shards", "Grab shards for energy."]] },
    { id: "training", name: "Training Arena", desc: "Practice your rolls against targets, cones and drills.", feats: [["Target Practice", "Hit targets for points."], ["Cone Course", "Weave through the cones."], ["Strength Drills", "Test your heavy rolls."]] },
  ];
  ARENAS.forEach((a) => { a.thumb = `assets/arenas/${a.id}.jpg`; a.img = a.img || a.thumb; a.icons = [1, 2, 3].map((k) => `assets/icons/arena-${a.id}-${k}.png`); });

  const EPISODES = [
    { id: "ep1", n: 1, name: "Skyfall Isles", chapter: 1, img: "assets/story/ep1.jpg", hero: "assets/story/chapter-skyfall.jpg", desc: "A new horizon awaits! Join Sera and the Zoogies as they discover the floating isles and the secrets that lie beyond the clouds." },
    { id: "ep2", n: 2, name: "Verdant Ruins", chapter: 1, img: "assets/story/ep2.jpg", desc: "Overgrown temples hum with ancient magic. Something stirs beneath the vines." },
    { id: "ep3", n: 3, name: "Ember Peaks", chapter: 1, img: "assets/story/ep3.jpg", desc: "The mountains are waking up. Roll through fire to reach the summit forge." },
    { id: "ep4", n: 4, name: "Crystal Depths", chapter: 1, img: "assets/story/ep4.jpg", desc: "Deep below the isles, glowing caverns guard a forgotten power." },
    { id: "ep5", n: 5, name: "Nova Spire", chapter: 1, img: "assets/story/ep5.jpg", desc: "At the top of the spire, the final trial of the season awaits." },
  ];
  const REPLAYS = [
    { id: "r1", k: "Ranked Match", v: "Apr 21, 2025  2v2 Victory", dur: "02:15", img: "assets/story/replay-ranked.jpg", ico: "assets/icons/replay-ranked.png" },
    { id: "r2", k: "CPU Battle", v: "Apr 20, 2025  1v1 Victory", dur: "03:42", img: "assets/story/replay-cpu.jpg", ico: "assets/icons/replay-cpu.png" },
    { id: "r3", k: "Friend Match", v: "Apr 19, 2025  2v2 Victory", dur: "05:18", img: "assets/story/replay-friend.jpg", ico: "assets/icons/replay-friend.png" },
  ];
  const HIGHLIGHTS = [
    { id: "h1", k: "Epic Comeback!", v: "Turn the tide in an intense 2v2 battle!", dur: "01:30", img: "assets/story/hl-comeback.jpg", ico: "assets/icons/hl-comeback.png" },
    { id: "h2", k: "Perfect Roll", v: "Clean 4-hit combo!", dur: "00:45", img: "assets/story/hl-perfect.jpg" },
    { id: "h3", k: "Clutch Win", v: "Last second victory!", dur: "00:52", img: "assets/story/hl-clutch.jpg" },
  ];
  const CINEMATICS = [
    { id: "c1", k: "The Awakening", v: "A new journey begins...", dur: "02:36", img: "assets/story/cin-awakening.jpg" },
    { id: "c2", k: "The Floating Isles", v: "Discover the world of Zoogi!", dur: "01:48", img: "assets/story/cin-floating.jpg" },
  ];

  const PERK = { model: ["Custom model", "assets/icons/perk-model.png"], effects: ["Unique effects", "assets/icons/perk-effects.png"], audio: ["Special roll audio", "assets/icons/perk-audio.png"], anim: ["Exclusive animation", "assets/icons/perk-anim.png"] };
  const SHOP_FEATURED = {
    featured: { sku: "skin_celestial_sera", ribbon: "Featured", name: "Celestial<br>Sera", rarity: "Epic Skin", desc: "A radiant spirit that illuminates every arena with starlight.", price: 1200, cur: "gems",
      perks: ["model", "effects", "audio", "anim"], gallery: ["assets/shop/celestial-sera.jpg", "assets/shop/sera-thumb-1.jpg", "assets/shop/sera-thumb-2.jpg", "assets/shop/sera-thumb-3.jpg"] },
    gems: { sku: "gems_vault", ribbon: "Best Value", name: "Gem<br>Vault", rarity: "2,500 Gems", desc: "A treasure chest overflowing with gems. Our best value pack.", iap: "$19.99", gems: 2500,
      perks: ["model", "effects"], perkText: ["+25% bonus gems", "Instant delivery"], gallery: ["assets/shop/gem-vault.jpg", "assets/shop/gem-pack.jpg", "assets/shop/gem-vault.jpg", "assets/shop/coin-pack.jpg"] },
    skins: { sku: "skin_forest_sprout", ribbon: "New Skin", name: "Forest<br>Sprout", rarity: "Rare Skin", desc: "A cheerful sprout that blooms with every roll.", price: 800, cur: "gems",
      perks: ["model", "effects", "audio"], gallery: ["assets/shop/forest-sprout.jpg", "assets/shop/forest-sprout.jpg", "assets/shop/sera-thumb-1.jpg", "assets/shop/sera-thumb-3.jpg"] },
    effects: { sku: "fx_starlight_swirl", ribbon: "Effect", name: "Starlight<br>Swirl", rarity: "Epic Effect", desc: "Leave a spiral of starlight behind every roll.", price: 450, cur: "gems",
      perks: ["effects", "audio"], gallery: ["assets/shop/sera-thumb-3.jpg", "assets/shop/sera-thumb-3.jpg", "assets/shop/sera-thumb-2.jpg", "assets/shop/sera-thumb-1.jpg"] },
    bundles: { sku: "bundle_elemental", ribbon: "30% Off", name: "Elemental<br>Bundle", rarity: "4 Skins", desc: "Four elemental skins plus matching effects and emotes.", price: 2800, cur: "gems",
      perks: ["model", "effects", "audio", "anim"], gallery: ["assets/shop/bundle-elemental.jpg", "assets/shop/bundle-elemental.jpg", "assets/shop/bundle-spirit.jpg", "assets/shop/sera-thumb-3.jpg"] },
  };
  const SHOP_TABS = [["featured", "Featured"], ["gems", "Gems"], ["skins", "Skins"], ["effects", "Effects"], ["bundles", "Bundles"]];
  const POPULAR = [
    { sku: "gems_500", k: "Gem Pack", amt: 500, cur: "gems", iap: "$4.99", img: "assets/shop/gem-pack.jpg" },
    { sku: "gems_2500", k: "Gem Vault", amt: 2500, cur: "gems", iap: "$19.99", img: "assets/shop/gem-vault.jpg" },
    { sku: "coins_10000", k: "Coin Pack", amt: 10000, cur: "coins", iap: "$4.99", img: "assets/shop/coin-pack.jpg" },
    { sku: "skin_forest_sprout", k: "Forest Sprout", rarity: "Rare Skin", price: 800, cur: "gems", img: "assets/shop/forest-sprout.jpg" },
  ];
  const BUNDLES = [
    { sku: "bundle_arena_spirit", k: "Arena Spirit Bundle", v: "Skin + Effects + Avatar<br>+ Emotes", price: 1500, off: 25, img: "assets/shop/bundle-spirit.jpg", minis: [1, 2, 3, 4].map((k) => `assets/shop/spirit-mini-${k}.jpg`) },
    { sku: "bundle_elemental", k: "Elemental Bundle", v: "4 Skins + Effects + Emotes", price: 2800, off: 30, img: "assets/shop/bundle-elemental.jpg", elems: [1, 2, 3, 4].map((k) => `assets/icons/element-mini-${k}.png`) },
  ];

  const SETTINGS_DEFAULT = {
    music: 70, sfx: 80, voice: 60, vibration: true, sensitivity: 70, invertY: false, aimAssist: true,
    quality: "high", fps: "60", updates: true, events: true, friends: true, marketing: false,
    language: "en-US", linked: { google: true, apple: false, facebook: false },
  };
  const LANGS = [["en-US", "English (US)"], ["en-GB", "English (UK)"], ["es-ES", "Español"], ["fr-FR", "Français"], ["de-DE", "Deutsch"], ["pt-BR", "Português (BR)"], ["ja-JP", "日本語"], ["ko-KR", "한국어"], ["zh-CN", "简体中文"]];

  /* ------------------------------------------------------------------
     STATE
  ------------------------------------------------------------------ */
  const S = {
    screen: "deck", history: [],
    player: { name: "Player123", level: 28, xp: 0.7 },
    gems: 1240, coins: 56320, notifications: 3,
    decks: store.get("decks", [
      { name: "My Deck 1", cards: CARDS.map((c) => c.id) },
      { name: "My Deck 2", cards: ["guard", "frost", "anchor-guard", "precision", "orb-relay", "recovery-roll"] },
      { name: "My Deck 3", cards: [] },
    ]),
    deckIdx: 0, deckMax: 5, filter: "all", selCard: "storm", selFrom: "coll",
    arena: "skyfall", loadArena: "frozen", loadNext: "board", loadLoop: true,
    episode: "ep1", unlocked: 1,
    shopTab: "featured", shopThumb: 0, owned: new Set(store.get("owned", [])),
    settings: Object.assign({}, SETTINGS_DEFAULT, store.get("settings", {})),
  };

  /* ------------------------------------------------------------------
     STAGE SCALING
  ------------------------------------------------------------------ */
  const stage = $("#stage"), letterbox = $("#letterbox");
  function fit() {
    const vw = innerWidth, vh = innerHeight;
    const scale = Math.min(vw / W, vh / H);
    stage.style.setProperty("--scale", scale);
    stage.style.setProperty("--tx", (vw - W * scale) / 2 + "px");
    stage.style.setProperty("--ty", (vh - H * scale) / 2 + "px");
  }
  addEventListener("resize", fit); addEventListener("orientationchange", fit); fit();
  // focus()/scrollIntoView can scroll clipped containers; keep the canvas pinned
  [$("#viewport"), stage].forEach((n) => n.addEventListener("scroll", () => { n.scrollTop = 0; n.scrollLeft = 0; }));

  /* ------------------------------------------------------------------
     TOAST / MODAL
  ------------------------------------------------------------------ */
  let toastT;
  function toast(msg) { const t = $("#toast"); t.textContent = msg; t.classList.add("show"); clearTimeout(toastT); toastT = setTimeout(() => t.classList.remove("show"), 1400); }
  function confirmBox(title, msg, yes = "Confirm", no = "Cancel") {
    return new Promise((res) => {
      const m = $("#modal"); $("#mTitle").textContent = title; $("#mMsg").innerHTML = msg;
      $("#mYes .disp").textContent = yes; $("#mNo .disp").textContent = no;
      m.classList.add("show");
      const done = (v) => { m.classList.remove("show"); $("#mYes").onclick = $("#mNo").onclick = null; res(v); };
      $("#mYes").onclick = () => done(true); $("#mNo").onclick = () => done(false);
    });
  }

  /* ------------------------------------------------------------------
     NAVIGATION
  ------------------------------------------------------------------ */
  const SCREENS = ["title", "deck", "arena", "story", "shop", "settings", "loading", "board", "battle"];
  const MENUS = new Set(["deck", "arena", "story", "shop", "settings"]);
  const HASH_IN = { options: "settings", menu: "deck", start: "title" }, HASH_OUT = { settings: "options" };
  const fromHash = () => {
    const h = location.hash.slice(1).replace("-dev", "");
    const m = /^load-([a-z]+)$/.exec(h);
    if (m && ARENAS.some((a) => a.id === m[1])) { S.loadArena = m[1]; S.loadLoop = true; return "loading"; }
    return HASH_IN[h] || h;
  };
  function show(name, { push = true, silent = false } = {}) {
    if (!SCREENS.includes(name)) return;
    if (name !== "loading") Loading.stop();
    if (name === "title") $("#scr-title").classList.remove("leaving");
    if (push && S.screen !== name) S.history.push(S.screen);
    S.screen = name;
    $$(".screen").forEach((s) => s.classList.toggle("active", s.id === "scr-" + name));
    const scr = $("#scr-" + name);
    const menu = MENUS.has(name);
    $("#topbar").classList.toggle("hidden", !menu);
    $("#logo").classList.toggle("hidden", !menu);
    if (menu) $("#logo").style.setProperty("--logo-y", (scr.dataset.logoY || 98) + "px");
    const bg = scr.querySelector(".bg");
    letterbox.style.setProperty("--lb", `url("${bg ? bg.getAttribute("src") : "assets/battle/arena.jpg"}")`);
    const tok = name === "loading" ? "load-" + S.loadArena : (HASH_OUT[name] || name);
    if (location.hash.slice(1) !== tok) history.replaceState(null, "", "#" + tok + (devOn ? "-dev" : ""));
    $$("#devnav button").forEach((b) => b.classList.toggle("on", b.dataset.s === name));
    Battle.setActive(name === "battle");
    Board.setActive(name === "board");
    if (name === "loading") { Loading.start(S.loadArena, S.loadLoop ? null : S.loadNext); letterbox.style.setProperty("--lb", `url("assets/load/${S.loadArena}.jpg")`); }
    if (!silent) Bridge.emit("ScreenChanged", { screen: name });
  }
  function back() { const prev = S.history.pop(); if (prev) show(prev, { push: false }); }

  /* ------------------------------------------------------------------
     TOP BAR
  ------------------------------------------------------------------ */
  function renderTop() {
    $("#playerName").textContent = S.player.name;
    $("#playerLevel").textContent = S.player.level;
    $("#xpFill").style.setProperty("--v", Math.round(S.player.xp * 100) + "%");
    $("#gemVal").textContent = fmt(S.gems);
    $("#coinVal").textContent = fmt(S.coins);
    const b = $("#notifBadge"); b.textContent = S.notifications; b.classList.toggle("hidden", !S.notifications);
  }
  function bindTop() {
    $('[data-ui="TopBar.Avatar"]').onclick = () => { Bridge.emit("OpenProfile"); toast(`${S.player.name} · Lv. ${S.player.level}`); };
    $('[data-ui="TopBar.GemsPlus"]').onclick = () => { setShopTab("gems"); show("shop"); Bridge.emit("OpenStore", { tab: "gems" }); };
    $('[data-ui="TopBar.CoinsPlus"]').onclick = () => { setShopTab("featured"); show("shop"); Bridge.emit("OpenStore", { tab: "coins" }); };
    $('[data-ui="TopBar.Bell"]').onclick = () => { Bridge.emit("OpenNotifications", { unread: S.notifications }); toast(S.notifications ? `${S.notifications} new notifications` : "No new notifications"); S.notifications = 0; renderTop(); };
    $('[data-ui="TopBar.Settings"]').onclick = () => show("settings");
  }

  /* ------------------------------------------------------------------
     DECK BUILDER
  ------------------------------------------------------------------ */
  const DECK_COL_X = [14, 188, 361, 535, 711];       // relative to the scroll box (31,386)
  const DECK_ROW_Y = 31, DECK_ROW_H = 156;
  const COLL_X = [40, 218, 392, 566, 746], COLL_Y = [892, 1017, 1147];
  const FILTERS = [
    ["all", 20, 754, 227, 61, 65, 147], ["infusion", 252, 754, 218, 61, 55, 135], ["technique", 477, 754, 220, 61, 50, 136], ["defense", 703, 754, 219, 61, 52, 139],
    ["utility", 20, 820, 227, 58, 68, 147], ["terrain", 252, 820, 218, 58, 56, 135], ["control", 477, 820, 220, 58, 55, 136], ["impact", 703, 820, 219, 58, 57, 139],
  ];
  const deck = () => S.decks[S.deckIdx];
  const nameSize = (n, base) => (n.length > 13 ? base - 4 : n.length > 10 ? base - 2 : base);

  function cardEl(c, { x, y, w, h, sm, art }) {
    const b = el("button", "card" + (sm ? " sm" : ""));
    pos(b, x, y, w, h);
    b.dataset.id = c.id;
    b.setAttribute("aria-label", `${c.name}, cost ${c.cost}`);
    b.style.setProperty("--nfs", nameSize(c.name, sm ? 17 : 18) + "px");
    if (sm) { b.style.setProperty("--cx", "1px"); b.style.setProperty("--cy", "2px"); } else { b.style.setProperty("--cx", "-3px"); b.style.setProperty("--cy", "7px"); }
    b.innerHTML = `<img class="art" src="${art}" alt="" loading="lazy">
      <span class="lb"></span><span class="ti"><img src="${typeIco(c.id)}" alt=""></span>
      <span class="nm">${c.name}</span><span class="cost"><b>${c.cost}</b></span>`;
    return b;
  }

  function renderDeck() {
    const d = deck();
    $("#deckName").textContent = d.name;
    $("#deckCount").textContent = d.cards.length;
    $("#deckMax").textContent = S.deckMax;
    positionPen();
    const g = $("#deckGrid"); g.innerHTML = "";
    d.cards.forEach((id, i) => {
      const c = CARD[id]; if (!c) return;
      const b = cardEl(c, { x: DECK_COL_X[i % 5], y: DECK_ROW_Y + Math.floor(i / 5) * DECK_ROW_H, w: 160, h: 147, art: artDeck(id) });
      b.setAttribute("role", "listitem");
      if (S.selFrom === "deck" && S.selCard === id) b.classList.add("sel");
      b.onclick = () => onDeckCard(id);
      g.appendChild(b);
    });
    const rows = Math.max(2, Math.ceil(d.cards.length / 5));
    const spacer = el("div"); spacer.style.cssText = `position:absolute;left:0;top:${DECK_ROW_Y + rows * DECK_ROW_H}px;width:1px;height:1px`; g.appendChild(spacer);
    renderColl();
  }

  function renderColl() {
    const g = $("#collGrid"); g.innerHTML = "";
    const list = CARDS.filter((c) => S.filter === "all" || c.cat === S.filter);
    list.slice(0, 15).forEach((c, i) => {
      const b = cardEl(c, { x: COLL_X[i % 5], y: COLL_Y[Math.floor(i / 5)], w: 157, h: 120, sm: true, art: artWide(c.id) });
      b.setAttribute("role", "listitem");
      if (S.selFrom === "coll" && S.selCard === c.id) b.classList.add("sel");
      if (deck().cards.includes(c.id)) b.classList.add("indeck");
      b.onclick = () => onCollCard(c.id);
      g.appendChild(b);
    });
    if (!list.length) { const e = el("div", "disp shadow", "No cards in this category"); e.style.cssText = "position:absolute;left:0;right:0;top:1060px;text-align:center;font-size:24px;color:#cfd4de"; g.appendChild(e); }
  }

  function renderDetail() {
    const c = CARD[S.selCard];
    $("#dArt").src = c.id === "storm" ? "assets/cards/storm-detail.jpg" : artDeck(c.id);
    $("#dSplash").src = c.id === "storm" ? "assets/cards/storm-splash.jpg" : artWide(c.id);
    $("#dTypeIco").src = typeIco(c.id);
    $("#dIco").src = c.id === "storm" ? "assets/icons/detail-swirl.png" : typeIco(c.id);
    $("#dCardName").textContent = c.name;
    $("#dCardName").style.fontSize = nameSize(c.name, 24) + "px";
    $("#dTitle").textContent = c.name;
    $("#dTitle").style.fontSize = (c.name.length > 12 ? 26 : 33) + "px";
    const tag = $("#dTag");
    tag.querySelector("span").textContent = CATS[c.cat].name;
    tag.querySelector("img").src = c.cat === "technique" ? "assets/icons/tag-technique.png" : `assets/icons/filter-${c.cat}.png`;
    tag.style.left = Math.max(431, 309 + $("#dTitle").offsetWidth + 22) + "px";
    $("#dDesc").textContent = c.desc;
    $("#dS1k").textContent = c.stats[0][0]; $("#dS1v").textContent = c.stats[0][1];
    $("#dS2k").textContent = c.stats[1][0]; $("#dS2v").textContent = c.stats[1][1];
    $("#dCost1").textContent = c.cost; $("#dCost2").textContent = c.cost;
  }

  function positionPen() {
    const n = $("#deckName");
    $("#deckRename").style.left = 120 + n.offsetWidth + 18 + "px";
  }

  function select(id, from) {
    S.selCard = id; S.selFrom = from;
    $$("#deckGrid .card").forEach((b) => b.classList.toggle("sel", from === "deck" && b.dataset.id === id));
    $$("#collGrid .card").forEach((b) => b.classList.toggle("sel", from === "coll" && b.dataset.id === id));
    renderDetail();
    Bridge.emit("CardSelected", { id, from });
  }
  function onCollCard(id) {
    if (S.selCard === id && S.selFrom === "coll") {             // second tap → add to deck
      const d = deck();
      if (d.cards.includes(id)) return toast("Already in deck");
      if (d.cards.length >= S.deckMax) return toast(`Deck is full (${S.deckMax}/${S.deckMax})`);
      d.cards.push(id); renderDeck(); toast(`${CARD[id].name} added`);
      Bridge.emit("DeckCardAdded", { deck: S.deckIdx, id });
      return;
    }
    select(id, "coll");
  }
  function onDeckCard(id) {
    if (S.selCard === id && S.selFrom === "deck") {             // second tap → remove
      const d = deck(); d.cards = d.cards.filter((x) => x !== id);
      S.selFrom = "coll"; renderDeck(); toast(`${CARD[id].name} removed`);
      Bridge.emit("DeckCardRemoved", { deck: S.deckIdx, id });
      return;
    }
    select(id, "deck");
  }

  function bindDeck() {
    const f = $("#filters");
    FILTERS.forEach(([id, x, y, w, h, ix, lx]) => {
      const b = el("button", "filt");
      pos(b, x, y, w, h);
      b.style.setProperty("--ix", ix + "px"); b.style.setProperty("--lx", lx + "px");
      b.dataset.f = id; b.setAttribute("aria-pressed", id === S.filter);
      b.setAttribute("data-ui", "Deck.Filter." + id);
      b.innerHTML = `<span class="stud" style="--x:50%;--y:0"></span><span class="stud" style="--x:50%;--y:${h}px"></span>
        <img src="assets/icons/filter-${id}.png" alt=""><span class="disp shadow">${CATS[id].name}</span>`;
      b.onclick = () => { S.filter = id; $$(".filt").forEach((x) => x.setAttribute("aria-pressed", x.dataset.f === id)); renderColl(); Bridge.emit("DeckFilter", { category: id }); };
      f.appendChild(b);
    });
    const go = (dir) => { S.deckIdx = (S.deckIdx + dir + S.decks.length) % S.decks.length; S.selFrom = "coll"; renderDeck(); Bridge.emit("DeckSwitched", { deck: S.deckIdx }); };
    $('[data-ui="Deck.PrevDeck"]').onclick = () => go(-1);
    $('[data-ui="Deck.NextDeck"]').onclick = () => go(1);
    const nm = $("#deckName");
    $("#deckRename").onclick = () => {
      nm.contentEditable = "true"; nm.focus({ preventScroll: true });
      const r = document.createRange(); r.selectNodeContents(nm); const s = getSelection(); s.removeAllRanges(); s.addRange(r);
    };
    const commit = () => {
      if (nm.contentEditable !== "true") return;
      nm.contentEditable = "false";
      const v = nm.textContent.trim().slice(0, 16) || deck().name;
      deck().name = v; nm.textContent = v; positionPen();
      Bridge.emit("DeckRenamed", { deck: S.deckIdx, name: v });
    };
    nm.addEventListener("blur", commit);
    nm.addEventListener("keydown", (e) => { if (e.key === "Enter") { e.preventDefault(); nm.blur(); } if (e.key === "Escape") { nm.textContent = deck().name; nm.blur(); } });
    $("#btnSaveDeck").onclick = () => {
      store.set("decks", S.decks);
      Bridge.emit("DeckSaved", { deck: S.deckIdx, name: deck().name, cards: deck().cards });
      toast("Deck saved");
    };
  }

  /* ------------------------------------------------------------------
     ARENA SELECT
  ------------------------------------------------------------------ */
  const ARENA_SLOTS = [
    [24, 829, 296, 223, 124, 141, 15, 34, 28], [331, 829, 286, 223, 124, 141, 15, 34, 28], [628, 829, 290, 223, 124, 141, 15, 40, 28],
    [24, 1063, 296, 222, 124, 141, 15, 36, 26], [331, 1063, 286, 222, 124, 141, 15, 36, 26], [628, 1063, 290, 222, 124, 141, 15, 40, 28],
    [86, 1289, 374, 214, 113, 130, 18, 37, 34], [476, 1289, 374, 214, 113, 130, 18, 37, 34],
  ];
  function renderArenas() {
    const g = $("#arenaGrid"); g.innerHTML = "";
    ARENAS.forEach((a, i) => {
      const [x, y, w, h, ih, nt, ib, il, ig] = ARENA_SLOTS[i];
      const b = el("button", "acard" + (a.id === S.arena ? " sel" : ""));
      pos(b, x, y, w, h);
      b.style.setProperty("--ih", ih + "px"); b.style.setProperty("--lh", h - ih - 21 + "px"); b.style.setProperty("--ig", ig + "px");
      b.dataset.id = a.id; b.setAttribute("role", "radio"); b.setAttribute("aria-checked", a.id === S.arena); b.setAttribute("aria-label", a.name);
      b.setAttribute("data-ui", "Arena.Card." + a.id);
      b.innerHTML = `<img class="img" src="${a.thumb}" alt="" loading="lazy"><span class="lb"></span>
        <span class="nm disp" style="top:${nt}px;bottom:auto">${a.name}</span>
        <span class="ics" style="left:${il - 18 + 12 - 12}px;bottom:${ib}px">${a.icons.map((s) => `<img src="${s}" alt="">`).join("")}</span>
        <span class="go"><span class="chev"></span></span><span class="stud"></span>`;
      b.onclick = () => selectArena(a.id);
      g.appendChild(b);
    });
    renderFeatured();
  }
  function renderFeatured() {
    const a = ARENAS.find((x) => x.id === S.arena);
    $("#fImg").src = a.img; $("#fName").textContent = a.name;
    $("#fName").style.fontSize = (a.name.length > 13 ? 28 : 33) + "px";
    $("#fDesc").textContent = a.desc;
    const f = $("#fFeats"); f.innerHTML = "";
    a.feats.forEach(([k, v, ico], i) => {
      const d = el("div", "feat"); d.style.top = 159 + 13 + i * 75 - 0 + "px"; d.style.left = "21px";
      const icon = ico ? `<img src="${ico}" alt="">`
        : `<span style="position:absolute;left:0;top:0;width:64px;height:64px;clip-path:polygon(30% 0,70% 0,100% 30%,100% 70%,70% 100%,30% 100%,0 70%,0 30%);background:var(--gold)"></span>
           <span style="position:absolute;left:4px;top:4px;width:56px;height:56px;clip-path:polygon(30% 0,70% 0,100% 30%,100% 70%,70% 100%,30% 100%,0 70%,0 30%);background:radial-gradient(circle,#123064,#0a1630)"></span>
           <img src="${a.icons[i]}" alt="" style="left:12px;top:12px;width:40px;height:40px">`;
      d.innerHTML = `${icon}<div class="k disp">${k}</div><div class="v">${v}</div>`;
      f.appendChild(d);
    });
  }
  function selectArena(id) {
    S.arena = id;
    $$("#arenaGrid .acard").forEach((b) => { const on = b.dataset.id === id; b.classList.toggle("sel", on); b.setAttribute("aria-checked", on); });
    renderFeatured();
    Bridge.emit("ArenaSelected", { id });
  }
  function bindArena() {
    $('[data-ui="Arena.Back"]').onclick = back;
    $("#btnSelectArena").onclick = () => {
      Bridge.emit("ArenaConfirmed", { id: S.arena, deck: deck().cards });
      toast(ARENAS.find((a) => a.id === S.arena).name);
      S.loadArena = S.arena; S.loadLoop = false;
      S.loadNext = S.arena === "skyfall" ? "battle" : "board";
      setTimeout(() => show("loading"), 350);
    };
  }

  /* ------------------------------------------------------------------
     STORY
  ------------------------------------------------------------------ */
  const EP_SLOTS = [[28, 180], [220, 162], [392, 162], [565, 165], [741, 172]];
  const NODE_X = [117, 297, 472, 646, 824];
  const LOCK_SVG = `<svg class="lock" viewBox="0 0 32 36"><use href="#iLock"/></svg>`;
  function renderStory() {
    const row = $("#epRow"); row.innerHTML = ""; const nodes = $("#epNodes"); nodes.innerHTML = "";
    EPISODES.forEach((e, i) => {
      const locked = e.n > S.unlocked;
      const [x, w] = EP_SLOTS[i];
      const b = el("button", "ep" + (locked ? " locked" : "") + (e.id === S.episode ? " sel" : ""));
      pos(b, x, 707, w, 159);
      b.dataset.id = e.id; b.setAttribute("role", "radio"); b.setAttribute("aria-checked", e.id === S.episode);
      b.setAttribute("aria-label", `Episode ${e.n}: ${e.name}${locked ? " (locked)" : ""}`); b.setAttribute("data-ui", "Story.Episode." + e.n);
      b.innerHTML = `<img class="img" src="${e.img}" alt="" loading="lazy">${locked ? LOCK_SVG : ""}
        <span class="lb"></span><span class="t1 disp">Ep ${e.n}</span><span class="t2 disp" style="font-size:${e.name.length > 13 ? 15 : 17}px">${e.name}</span><span class="stud"></span>`;
      b.onclick = () => selectEpisode(e.id);
      row.appendChild(b);
      const n = el("span", "node" + (e.id === S.episode ? " on" : ""));
      n.style.left = NODE_X[i] + "px";
      if (locked) n.innerHTML = `<svg viewBox="0 0 32 36"><use href="#iLockW"/></svg>`;
      nodes.appendChild(n);
    });
    const ep = EPISODES.find((x) => x.id === S.episode);
    $("#chKick").textContent = "Chapter " + ep.chapter;
    $("#chName").textContent = ep.name;
    $("#chDesc").textContent = ep.desc;
    $("#chHero").src = ep.hero || ep.img;
    $("#watchLbl").textContent = "Watch Episode " + ep.n;

    const rp = $("#replays"); rp.innerHTML = "";
    const RS = [[30, 295, 18, 74, 50], [335, 280, 15, 70, 43], [628, 282, 12, 69, 48]];
    REPLAYS.forEach((r, i) => {
      const [x, w, il, kl, bl] = RS[i];
      const c = el("div", "gf vcard glow"); pos(c, x, 985, w, 230);
      c.innerHTML = `<img class="img" src="${r.img}" alt="" style="height:115px"><span class="dur" style="top:96px;bottom:auto;right:14px">${r.dur}</span>
        <img class="ico" src="${r.ico}" alt="" style="left:${il}px;top:127px">
        <div class="k disp" style="left:${kl}px;top:129px">${r.k}</div><div class="v" style="left:${kl}px;top:154px">${r.v}</div>
        <button class="bbtn" style="--x:${bl}px;--y:177px;--w:220px;--h:43px;--p:18px;--fs:19px" data-ui="Story.Replay.${r.id}"><span class="disp">View Replay</span><span class="chev" style="width:8px;height:8px;color:#ffd65a"></span></button>`;
      c.querySelector("button").onclick = () => { Bridge.emit("PlayReplay", { id: r.id }); toast("Loading replay…"); };
      rp.appendChild(c);
    });
    const hl = $("#highlights"); hl.innerHTML = "";
    const HS = [[30, 432, 74], [472, 218, 16], [702, 208, 13]];
    HIGHLIGHTS.forEach((h, i) => {
      const [x, w, kl] = HS[i];
      const c = el("button", "gf vcard glow"); pos(c, x, 1282, w, 145);
      c.setAttribute("data-ui", "Story.Highlight." + h.id);
      c.innerHTML = `<img class="img" src="${h.img}" alt="" style="height:86px"><span class="dur" style="top:66px;bottom:auto;right:14px">${h.dur}</span>
        ${h.ico ? `<img class="ico" src="${h.ico}" alt="" style="left:18px;top:94px">` : ""}
        <div class="k disp" style="left:${kl}px;top:96px;font-size:19px">${h.k}</div><div class="v" style="left:${kl}px;top:120px">${h.v}</div>
        <span class="go" style="top:102px"><span class="chev"></span></span>`;
      c.onclick = () => { Bridge.emit("PlayHighlight", { id: h.id }); toast(h.k); };
      hl.appendChild(c);
    });
    const cn = $("#cinematics"); cn.innerHTML = "";
    const CS = [[30, 432], [472, 438]];
    CINEMATICS.forEach((m, i) => {
      const [x, w] = CS[i];
      const c = el("button", "gf vcard glow"); pos(c, x, 1490, w, 158);
      c.setAttribute("data-ui", "Story.Cinematic." + m.id);
      c.innerHTML = `<img class="img" src="${m.img}" alt="" style="height:90px"><span class="dur" style="top:72px;bottom:auto;right:14px">${m.dur}</span>
        <div class="k disp" style="left:16px;top:104px">${m.k}</div><div class="v" style="left:16px;top:131px">${m.v}</div>
        <span class="go" style="top:114px"><span class="chev"></span></span>`;
      c.onclick = () => { Bridge.emit("PlayCinematic", { id: m.id }); toast(m.k); };
      cn.appendChild(c);
    });
  }
  function selectEpisode(id) {
    const e = EPISODES.find((x) => x.id === id);
    if (e.n > S.unlocked) { toast(`Complete Ep ${e.n - 1} to unlock`); Bridge.emit("EpisodeLocked", { id }); return; }
    S.episode = id; renderStory(); Bridge.emit("EpisodeSelected", { id });
  }
  function bindStory() {
    $("#btnWatch").onclick = () => { Bridge.emit("WatchEpisode", { id: S.episode }); toast("Starting episode…"); };
    $$('#scr-story .seeall').forEach((b) => (b.onclick = () => { Bridge.emit("SeeAll", { section: b.dataset.ui }); toast("See all"); }));
  }

  /* ------------------------------------------------------------------
     SHOP
  ------------------------------------------------------------------ */
  const TAB_SLOTS = [[25, 180], [210, 173], [388, 172], [565, 173], [742, 176]];
  function renderShopTabs() {
    const t = $("#shopTabs"); t.innerHTML = "";
    SHOP_TABS.forEach(([id, name], i) => {
      const [x, w] = TAB_SLOTS[i];
      const b = el("button", "gf tab glow"); pos(b, x, 425, w, 97);
      b.dataset.tab = id; b.setAttribute("role", "tab"); b.setAttribute("aria-selected", id === S.shopTab); b.setAttribute("data-ui", "Shop.Tab." + id);
      b.innerHTML = `<img src="assets/icons/tab-${id}.png" alt=""><span class="disp shadow">${name}</span>`;
      b.onclick = () => setShopTab(id);
      t.appendChild(b);
    });
  }
  function setShopTab(id) {
    S.shopTab = id; S.shopThumb = 0;
    $$("#shopTabs .tab").forEach((b) => b.setAttribute("aria-selected", b.dataset.tab === id));
    renderShopFeatured();
    Bridge.emit("ShopTab", { tab: id });
  }
  function renderShopFeatured() {
    const f = SHOP_FEATURED[S.shopTab];
    $("#sRibbon").textContent = f.ribbon;
    $("#sName").innerHTML = f.name;
    $("#sRarity").textContent = f.rarity;
    $("#sDesc").textContent = f.desc;
    $("#sHero").src = f.gallery[S.shopThumb];
    const p = $("#sPerks"); p.innerHTML = "";
    f.perks.forEach((k, i) => {
      const d = el("div", "perk"); d.style.top = 274 + i * 39 + "px";
      d.innerHTML = `<img src="${PERK[k][1]}" alt=""><span>${(f.perkText && f.perkText[i]) || PERK[k][0]}</span>`;
      p.appendChild(d);
    });
    const th = $("#sThumbs"); th.innerHTML = "";
    [1, 2, 3].forEach((gi, i) => {
      const b = el("button", "gf sthumb glow"); pos(b, 783, 25 + i * 109, 95, 102);
      b.setAttribute("role", "tab"); b.setAttribute("aria-selected", S.shopThumb === gi); b.setAttribute("aria-label", "Preview " + gi);
      b.innerHTML = `<img src="${f.gallery[gi]}" alt="">`;
      b.onclick = () => { S.shopThumb = S.shopThumb === gi ? 0 : gi; renderShopFeatured(); };
      th.appendChild(b);
    });
    const owned = S.owned.has(f.sku);
    $("#sPriceBtn").style.display = f.iap ? "none" : "";
    $("#sPrice").textContent = f.price ? fmt(f.price) : "";
    const buy = $("#sBuyBtn");
    buy.style.setProperty("--x", f.iap ? "396px" : "610px"); buy.style.setProperty("--w", f.iap ? "496px" : "282px");
    $("#sBuyLbl").textContent = owned ? "Owned" : f.iap ? `Buy ${f.iap}` : "Buy Now";
    buy.disabled = owned; buy.style.opacity = owned ? .75 : 1;
  }
  async function purchase(item) {
    if (S.owned.has(item.sku)) return toast("Already owned");
    if (item.iap) {
      const ok = await confirmBox("Purchase", `Buy <b>${item.k || item.name.replace("<br>", " ")}</b> for <b>${item.iap}</b>?`, "Buy");
      if (!ok) return;
      Bridge.emit("IAPRequest", { sku: item.sku, price: item.iap });
      // Browser preview: grant immediately. In-engine, wait for ZoogiUI.purchaseResult().
      if (!window.ue) grant(item);
      return;
    }
    const cur = item.cur || "gems";
    if (S[cur] < item.price) { toast(`Not enough ${cur}`); Bridge.emit("InsufficientFunds", { sku: item.sku, need: item.price, have: S[cur] }); return; }
    const ok = await confirmBox("Purchase", `Buy <b>${(item.k || item.name).replace("<br>", " ")}</b> for <b>${fmt(item.price)}</b> ${cur}?`, "Buy");
    if (!ok) return;
    S[cur] -= item.price; S.owned.add(item.sku); store.set("owned", [...S.owned]);
    renderTop(); renderShopFeatured(); renderPopular(); renderBundles();
    toast("Purchased!");
    Bridge.emit("Purchase", { sku: item.sku, price: item.price, currency: cur });
  }
  function grant(item) {
    if (item.gems) S.gems += item.gems;
    if (item.amt) S[item.cur] += item.amt;
    if (!item.amt && !item.gems) S.owned.add(item.sku);
    renderTop(); toast("Purchase complete");
  }
  function renderPopular() {
    const g = $("#popular"); g.innerHTML = "";
    const PS = [[25, 207], [243, 217], [472, 216], [700, 212]];
    POPULAR.forEach((it, i) => {
      const [x, w] = PS[i];
      const c = el("div", "gf item glow"); pos(c, x, 1060, w, 242);
      const owned = S.owned.has(it.sku);
      const curIco = it.cur === "coins" ? "assets/icons/coin.png" : "assets/icons/gem.png";
      const head = it.rarity
        ? `<img src="assets/icons/badge-leaf.png" alt="" style="position:absolute;left:12px;top:132px;width:38px;height:38px">
           <div class="k disp" style="top:141px;left:52px;right:2px;font-size:17px;text-align:center;letter-spacing:-.02em">${it.k}</div>
           <div class="raritytag rare" style="left:63px;top:164px;height:22px;padding:0 12px;font-size:12px">${it.rarity}</div>`
        : `<div class="k disp" style="top:131px">${it.k}</div><div class="amt" style="top:157px"><img src="${curIco}" alt="">${fmt(it.amt)}</div>`;
      const priceInner = it.iap ? `<span class="disp">${it.iap}</span>` : `<span class="disp"><img src="assets/icons/gem.png" alt="">${owned ? "Owned" : fmt(it.price)}</span>`;
      c.innerHTML = `<img class="img" src="${it.img}" alt="">${head}
        <button class="bbtn price" style="--x:10px;--y:190px;--w:${w - 20}px;--h:45px" data-ui="Shop.Popular.${it.sku}">${priceInner}</button>`;
      c.querySelector("button").onclick = () => purchase(it);
      g.appendChild(c);
    });
  }
  function renderBundles() {
    const g = $("#bundles"); g.innerHTML = "";
    BUNDLES.forEach((b, i) => {
      const [x, w] = i ? [480, 435] : [25, 443];
      const c = el("div", "gf bundle glow"); pos(c, x, 1363, w, 257);
      const owned = S.owned.has(b.sku);
      const extra = b.minis
        ? b.minis.map((m, k) => `<span class="mini" style="left:${[206, 273, 333, 381][k]}px;top:70px;--s:${[59, 52, 46, 48][k]}px"><img src="${m}" alt=""></span>`).join("")
        : b.elems.map((m, k) => `<img src="${m}" alt="" style="position:absolute;left:${30 + k * 42}px;top:207px;width:34px;height:34px">`).join("");
      const text = i
        ? `<div class="disp" style="position:absolute;left:27px;top:160px;font-size:18px;color:#fff;letter-spacing:-.01em">${b.k}</div><div style="position:absolute;left:27px;top:182px;font:500 16px/1 var(--sans);color:#e4e7ee">${b.v}</div>`
        : `<div class="disp" style="position:absolute;left:25px;top:162px;font-size:18px;color:#fff;letter-spacing:-.01em">${b.k}</div><div style="position:absolute;left:25px;top:186px;font:500 16px/20px var(--sans);color:#e4e7ee">${b.v}</div>`;
      c.innerHTML = `<img class="img" src="${b.img}" alt="">
        <span class="burst" style="left:${i ? 367 : 368}px;top:${i ? -3 : 0}px">${b.off}%<small>Off</small></span>
        ${extra}${text}
        <button class="bbtn pricebtn" style="--x:${i ? 238 : 243}px;--y:${i ? 152 : 149}px;--w:${i ? 186 : 186}px;--h:70px;--p:20px" data-ui="Shop.Bundle.${b.sku}">
          <img class="gemi" src="assets/icons/gem.png" alt="" style="width:36px;height:44px"><span class="disp" style="font-family:var(--serif);font-size:${owned ? 22 : 30}px">${owned ? "Owned" : fmt(b.price)}</span></button>`;
      c.querySelector("button").onclick = () => purchase({ ...b, cur: "gems" });
      g.appendChild(c);
    });
  }
  function bindShop() {
    $("#sPriceBtn").onclick = $("#sBuyBtn").onclick = () => purchase({ ...SHOP_FEATURED[S.shopTab], k: SHOP_FEATURED[S.shopTab].name });
    $$('#scr-shop .seeall').forEach((b) => (b.onclick = () => { Bridge.emit("SeeAll", { section: b.dataset.ui }); toast("See all"); }));
  }

  /* ------------------------------------------------------------------
     SETTINGS
  ------------------------------------------------------------------ */
  const SECTIONS = [
    { id: "audio", y: 418, h: 237, hy: 9, rows: [
      { key: "music", label: "Music Volume", ico: "music", type: "slider", cy: 73 },
      { key: "sfx", label: "Sound Effects", ico: "sfx", type: "slider", cy: 114 },
      { key: "voice", label: "Voice / Announcements", ico: "voice", type: "slider", cy: 154 },
      { key: "vibration", label: "Vibration", ico: "vibration", type: "toggle", cy: 199 } ] },
    { id: "controls", y: 662, h: 181, hy: 8, rows: [
      { key: "sensitivity", label: "Control Sensitivity", ico: "sensitivity", type: "slider", cy: 68 },
      { key: "invertY", label: "Invert Y-Axis", ico: "invert", type: "toggle", cy: 110 },
      { key: "aimAssist", label: "Auto Aim Assist", ico: "aim", type: "toggle", cy: 150 } ] },
    { id: "graphics", y: 852, h: 166, hy: 11, rows: [
      { key: "quality", label: "Graphics Quaility", ico: "quality", type: "seg", cy: 69, opts: [["low", "Low", 465, 89], ["medium", "Medium", 566, 89], ["high", "High", 667, 89], ["ultra", "Ultra", 768, 89]] },
      { key: "fps", label: "Frame Rate", ico: "fps", type: "seg", cy: 125, opts: [["30", "30 FPS", 467, 122], ["60", "60 FPS", 599, 122], ["120", "120 FPS", 734, 122]] } ] },
    { id: "notifications", y: 1024, h: 219, hy: 10, rows: [
      { key: "updates", label: "Game Updates", ico: "updates", type: "toggle", cy: 61 },
      { key: "events", label: "Events & Rewards", ico: "events", type: "toggle", cy: 105 },
      { key: "friends", label: "Friend Activity", ico: "friends", type: "toggle", cy: 148 },
      { key: "marketing", label: "Marketing & Promotions", ico: "marketing", type: "toggle", cy: 189 } ] },
    { id: "language", y: 1250, h: 86, hy: 22, rows: [] },
    { id: "account", y: 1344, h: 152, hy: 13, rows: [] },
  ];
  const ACCTS = [["google", "Google Play", 572], ["apple", "Apple ID", 675], ["facebook", "Facebook", 777]];
  function renderSettings() {
    const root = $("#setSections"); root.innerHTML = "";
    const st = S.settings;
    SECTIONS.forEach((sec) => {
      const g = el("div", "gf setsec stone glow"); pos(g, 28, sec.y, 884, sec.h);
      g.setAttribute("data-ui", "Settings." + sec.id);
      let html = `<img class="art" src="assets/settings/${sec.id}.jpg" alt=""><span class="div"></span>
        <img class="hico" src="assets/icons/set-${sec.id}.png" alt="" style="top:${sec.hy}px">
        <div class="h disp goldtxt" style="top:${sec.hy + 9}px">${sec.id}</div>`;
      sec.rows.forEach((r) => {
        html += `<div class="setrow" style="top:${r.cy - 18}px;width:300px"><img src="assets/icons/row-${r.ico}.png" alt=""><span class="k">${r.label}</span></div>`;
        if (r.type === "slider") {
          html += `<div class="slider" role="slider" tabindex="0" aria-label="${r.label}" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${st[r.key]}" data-key="${r.key}" style="--x:494px;--y:${r.cy - 17}px;--w:286px;--v:${st[r.key]}%">
              <span class="track"></span><span class="fill"></span><span class="thumb"></span></div>
            <div class="pct" data-pct="${r.key}" style="--x:788px;--y:${r.cy - 9}px">${st[r.key]}%</div>`;
        } else if (r.type === "toggle") {
          html += `<button class="toggle" role="switch" aria-label="${r.label}" aria-checked="${!!st[r.key]}" data-key="${r.key}" style="--x:740px;--y:${r.cy - 18}px"><span class="s">${st[r.key] ? "ON" : "OFF"}</span><span class="k"></span></button>`;
        } else if (r.type === "seg") {
          html += r.opts.map(([v, lbl, x, w]) => `<button class="seg" role="radio" aria-checked="${String(st[r.key]) === v}" data-key="${r.key}" data-val="${v}" style="--x:${x}px;--y:${r.cy - 18}px;--w:${w}px">${lbl}</button>`).join("");
        }
      });
      if (sec.id === "language") {
        const cur = LANGS.find((l) => l[0] === st.language) || LANGS[0];
        html += `<div class="dropdown" style="left:514px;top:21px;width:335px;height:41px"><span class="v" id="langVal">${cur[1]}</span><span class="chev"></span>
          <select aria-label="Language" id="langSel">${LANGS.map(([v, n]) => `<option value="${v}" ${v === st.language ? "selected" : ""}>${n}</option>`).join("")}</select></div>`;
      }
      if (sec.id === "account") {
        html += `<p style="position:absolute;left:237px;top:58px;width:320px;margin:0;font:500 16px/22px var(--sans);color:#e4e7ee">Link your account to keep your progress safe across all devices!</p>`;
        html += ACCTS.map(([id, nm, x]) => `<button class="acct ${st.linked[id] ? "linked" : ""}" style="left:${x}px;top:20px" data-acct="${id}" aria-label="${nm}">
            <img src="assets/icons/acct-${id}.png" alt=""><span class="k">${nm}</span><span class="st">${st.linked[id] ? "Linked" : "Link"}</span></button>`).join("");
      }
      g.innerHTML = html;
      root.appendChild(g);
    });
    bindSettingsControls();
  }
  function bindSettingsControls() {
    const st = S.settings;
    $$("#setSections .toggle").forEach((t) => (t.onclick = () => {
      const k = t.dataset.key; st[k] = !st[k];
      t.setAttribute("aria-checked", st[k]); t.querySelector(".s").textContent = st[k] ? "ON" : "OFF";
      if (k === "vibration" && st[k] && navigator.vibrate) navigator.vibrate(30);
      Bridge.emit("SettingChanged", { key: k, value: st[k] });
    }));
    $$("#setSections .seg").forEach((b) => (b.onclick = () => {
      const k = b.dataset.key; st[k] = b.dataset.val;
      $$(`#setSections .seg[data-key="${k}"]`).forEach((x) => x.setAttribute("aria-checked", x === b));
      Bridge.emit("SettingChanged", { key: k, value: st[k] });
    }));
    $$("#setSections .slider").forEach((s) => {
      const k = s.dataset.key;
      const setV = (v) => {
        v = Math.max(0, Math.min(100, Math.round(v))); st[k] = v;
        s.style.setProperty("--v", v + "%"); s.setAttribute("aria-valuenow", v);
        $(`[data-pct="${k}"]`).textContent = v + "%";
      };
      const fromEvt = (e) => { const r = s.getBoundingClientRect(); setV(((e.clientX - r.left) / r.width) * 100); };
      s.addEventListener("pointerdown", (e) => { s.setPointerCapture(e.pointerId); fromEvt(e); });
      s.addEventListener("pointermove", (e) => { if (s.hasPointerCapture(e.pointerId)) fromEvt(e); });
      s.addEventListener("pointerup", () => Bridge.emit("SettingChanged", { key: k, value: st[k] }));
      s.addEventListener("keydown", (e) => {
        const d = { ArrowRight: 5, ArrowUp: 5, ArrowLeft: -5, ArrowDown: -5 }[e.key];
        if (d) { e.preventDefault(); setV(st[k] + d); Bridge.emit("SettingChanged", { key: k, value: st[k] }); }
      });
    });
    const sel = $("#langSel");
    sel.onchange = () => { st.language = sel.value; $("#langVal").textContent = sel.options[sel.selectedIndex].text; Bridge.emit("SettingChanged", { key: "language", value: st.language }); };
    $$("#setSections .acct").forEach((b) => (b.onclick = async () => {
      const id = b.dataset.acct;
      if (st.linked[id]) {
        if (!(await confirmBox("Unlink", `Unlink your <b>${b.getAttribute("aria-label")}</b> account?`, "Unlink"))) return;
        st.linked[id] = false;
      } else {
        Bridge.emit("AccountLinkRequest", { provider: id });
        if (window.ue) return toast("Linking…");       // engine answers with ZoogiUI.accountLinked()
        st.linked[id] = true;
      }
      renderSettings();
      Bridge.emit("AccountLinkChanged", { provider: id, linked: st.linked[id] });
    }));
  }
  function bindSettings() {
    $("#btnSaveSettings").onclick = () => { store.set("settings", S.settings); Bridge.emit("SettingsSaved", S.settings); toast("Settings saved"); };
  }

  /* ------------------------------------------------------------------
     BATTLE HUD
  ------------------------------------------------------------------ */
  const Battle = (() => {
    const HAND = [
      { id: "storm", name: "Storm", cost: 2, tag: "Control", desc: "Create a whirling gust that knocks nearby opponents away and disrupts their roll.", c: ["#3a2b78", "#23194d", "#8c6cf0", "rgba(140,108,240,.5)"] },
      { id: "guard", name: "Guard", cost: 1, tag: "Defense", desc: "Raise a crystal shield that absorbs the next impact and steadies your roll.", c: ["#1f3a78", "#132550", "#5fa8ff", "rgba(95,168,255,.5)"] },
      { id: "orb-relay", name: "Orb Relay", cost: 3, tag: "Support", desc: "Launch linked orbs that bounce between allies, restoring energy with each pass.", c: ["#2c2f80", "#191b52", "#8f8cff", "rgba(143,140,255,.5)"] },
      { id: "precision", name: "Precision", cost: 2, tag: "Focus", desc: "Lock onto a lane and line up a perfectly aimed strike on your next roll.", c: ["#16466e", "#0d2b47", "#5fd0ff", "rgba(95,208,255,.5)"] },
      { id: "heavy-roll", name: "Heavy Roll", cost: 4, tag: "Power", desc: "Slam forward with crushing weight, smashing through rivals in your path.", c: ["#6e2a12", "#43170a", "#ff9a4a", "rgba(255,154,74,.5)"] },
    ];
    const UNITS = [
      { side: "ally", x: 102, ring: "#7a5cff", img: "p-purple", el: "arcane", hp: 100, arc: 275, pips: 3 },
      { side: "ally", x: 247, ring: "#2fa6ff", img: "p-blue", el: "water", hp: 100, arc: 250, pips: 1 },
      { side: "enemy", x: 693, ring: "#5a1f22", img: "p-red", el: "fire", hp: 50, arc: 200, pips: 2 },
      { side: "enemy", x: 840, ring: "#4b2b2b", img: "p-green", el: "nature", hp: 50, arc: 215, pips: 2 },
    ];
    const EL_SVG = {
      arcane: '<circle cx="12" cy="12" r="7" fill="none" stroke="#f0e6ff" stroke-width="2"/><path d="M8 12a4 4 0 0 1 8 0" fill="none" stroke="#fff" stroke-width="2"/>',
      water: '<path d="M12 3 C15 8 18 11 18 15 a6 6 0 0 1 -12 0 C6 11 9 8 12 3Z" fill="#dff4ff"/>',
      fire: '<path d="M12 2c1 4 6 6 6 12a6 6 0 0 1-12 0c0-3 2-5 3-6 0 2 1 3 2 3 0-3-1-6 1-9z" fill="#ffd36b"/><path d="M12 11c1 2 3 3 3 5.5a3 3 0 0 1-6 0c0-1.5 1-2.5 1.6-3 .2 1 .8 1.5 1.4 1.5 0-1.4-.4-2.6 0-4z" fill="#ff5a1f"/>',
      nature: '<path d="M12 20V11" stroke="#fff" stroke-width="2"/><path d="M12 13C12 8 8 5 4 5c0 5 3 8 8 8z" fill="#fff"/><path d="M12 13c0-5 4-8 8-8 0 5-3 8-8 8z" fill="#fff"/>',
    };
    const B = { energy: 3, maxEnergy: 6, sel: "storm", combo: 12, awaken: 3, awakenMax: 4, time: 84, paused: false, ep: 3, epMax: 6, roll: 63, active: false };
    let tick = null, regen = 0;

    function build() {
      $("#bUnits").innerHTML = UNITS.map((u, i) => `
        <div class="unit ${u.side}" data-i="${i}" style="--ux:${u.x}px;--ring:${u.ring};--hp:${u.hp}%;--arc:${u.arc}deg">
          <div class="portrait"><img src="assets/battle/${u.img}.png" alt=""></div>
          <span class="element el-${u.el}"><svg viewBox="0 0 24 24">${EL_SVG[u.el]}</svg></span>
          <div class="hpbar"><i></i></div>
          <div class="pips">${"<b></b>".repeat(u.pips)}</div>
        </div>`).join("");
      const crown = '<svg class="crown" viewBox="0 0 40 30" aria-hidden="true"><path d="M20 0 L28 14 L20 30 L12 14Z" fill="url(#gGold)" stroke="#6b4a0c"/><path d="M0 22 L12 14 M40 22 L28 14" stroke="#ffd36a" stroke-width="3"/></svg>';
      $("#bCards").innerHTML = "";
      HAND.forEach((c, i) => {
        const b = el("button", "bcard"); b.style.left = [30, 210, 390, 570, 750][i] + "px";
        b.dataset.id = c.id; b.setAttribute("role", "radio"); b.setAttribute("aria-label", `${c.name}, cost ${c.cost}`);
        b.innerHTML = `${crown}<span class="bcost"><span>${c.cost}</span></span><span class="inner"><span class="bart"><img src="assets/battle/card-${c.id}.jpg" alt=""></span><span class="title">${c.name}</span></span><span class="tab2"></span>`;
        b.onclick = () => pick(c.id);
        $("#bCards").appendChild(b);
      });
      $("#bPause").onclick = togglePause;
      $("#bActivate").onclick = activate;
      $("#bAwaken").onclick = awaken;
    }
    function render() {
      $("#bGauge").innerHTML = `${'<span class="seg2"></span>'.repeat(Math.min(B.ep, 3))}<svg class="ep-marker" viewBox="0 0 60 90"><g filter="url(#fGlow)"><use href="#iStar4" x="0" y="15" width="60" height="60" fill="url(#gGold)"/><path d="M30 26 L38 45 L30 64 L22 45 Z" fill="#cfefff"/></g></svg><span class="dot gold"></span>${'<span class="dot"></span>'.repeat(Math.max(0, B.epMax - B.ep))}`;
      $("#bEp").textContent = B.energy; $("#bEpMax").textContent = B.maxEnergy;
      $("#scr-battle .crystals").innerHTML = Array.from({ length: 3 }, (_, i) => `<svg class="${i < Math.ceil(B.energy / 2) ? "" : "off"}"><use href="#iCrystal"/></svg>`).join("");   // 3 slots, 2 energy each
      $("#bComboNum").textContent = B.combo;
      $("#bAwkPips").innerHTML = Array.from({ length: B.awakenMax }, (_, i) => `<b class="${i < B.awaken ? "on" : ""}"></b>`).join("");
      $$("#bCards .bcard").forEach((b) => {
        const c = HAND.find((h) => h.id === b.dataset.id);
        b.classList.toggle("selected", b.dataset.id === B.sel); b.setAttribute("aria-checked", b.dataset.id === B.sel);
        b.classList.toggle("unaffordable", c.cost > B.energy);
      });
      const c = HAND.find((h) => h.id === B.sel);
      $("#bdImg").src = `assets/battle/card-${c.id}.jpg`; $("#bdTitle").textContent = c.name; $("#bdDesc").textContent = c.desc; $("#bdCost").textContent = c.cost;
      const t = $("#bdTag"); t.textContent = c.tag; ["a", "b", "c", "g"].forEach((k, i) => t.style.setProperty("--tag-" + k, c.c[i]));
      $("#bActivate").disabled = c.cost > B.energy;
      const m = String(Math.floor(B.time / 60)).padStart(2, "0"), s = String(B.time % 60).padStart(2, "0");
      $("#bTimer").textContent = `${m}:${s}`;
    }
    function pick(id) { B.sel = id; render(); Bridge.emit("BattleCardSelected", { id }); }
    function activate() {
      const c = HAND.find((h) => h.id === B.sel);
      if (c.cost > B.energy) return toast("Not enough energy");
      B.energy -= c.cost; B.combo += 1; B.awaken = Math.min(B.awakenMax, B.awaken + 1);
      const btn = $("#bActivate"); btn.classList.remove("fired"); void btn.offsetWidth; btn.classList.add("fired");
      const cb = $("#bCombo"); cb.classList.remove("bump"); void cb.offsetWidth; cb.classList.add("bump");
      render(); toast(`${c.name}!`);
      Bridge.emit("AbilityActivated", { id: c.id, cost: c.cost, energyLeft: B.energy });
    }
    function awaken() {
      if (B.awaken < B.awakenMax) return toast(`Charge ${B.awakenMax - B.awaken} more`);
      B.awaken = 0; const b = $("#bAwaken"); b.classList.remove("fired"); void b.offsetWidth; b.classList.add("fired");
      render(); toast("Awaken!"); Bridge.emit("AwakenActivated", {});
    }
    async function togglePause() {
      B.paused = true; $("#bPause").setAttribute("aria-pressed", "true"); $("#bVeil").classList.add("show");
      Bridge.emit("BattlePaused", {});
      const resume = await confirmBox("Paused", "The match is paused.", "Resume", "Quit");
      $("#bVeil").classList.remove("show"); $("#bPause").setAttribute("aria-pressed", "false"); B.paused = false;
      if (resume) Bridge.emit("BattleResumed", {});
      else { Bridge.emit("BattleQuit", {}); show("arena"); }
    }
    function loop() {
      if (B.paused || !B.active) return;
      B.roll = (B.roll + 2) % 102; $("#bRollFill").style.width = B.roll + "%";
      regen += 1;
      if (regen % 16 === 0 && B.energy < B.maxEnergy) { B.energy += 1; render(); }   // +1 energy every 4 s
      if (regen % 4 === 0 && B.time > 0) { B.time -= 1; render(); if (B.time === 0) { toast("Time!"); Bridge.emit("BattleTimeUp", {}); } }
    }
    function setActive(on) {
      B.active = on;
      clearInterval(tick); tick = on ? setInterval(loop, 250) : null;
    }
    function reset() { Object.assign(B, { energy: 3, combo: 12, awaken: 3, time: 84, roll: 63, sel: "storm" }); render(); }
    return { build, render, setActive, reset, state: B, units: UNITS, hand: HAND };
  })();


  /* ------------------------------------------------------------------
     TITLE
  ------------------------------------------------------------------ */
  $("#btnTapStart").onclick = () => {
    const t = $("#scr-title"); if (t.classList.contains("leaving")) return;
    t.classList.add("leaving"); Bridge.emit("TitleTapped", {});
    setTimeout(() => show("deck"), 420);
  };

  /* ------------------------------------------------------------------
     LOADING  (one widget, art + bar geometry per arena)
  ------------------------------------------------------------------ */
  const Loading = (() => {
    const BARS = {
      skyfall: [195, 1458, 554, 41, .652, "20px"], frozen: [182, 1472, 577, 45, .529, "4px"], volcanic: [157, 1460, 618, 40, .6, "8px"],
      pharaoh: [194, 1468, 554, 47, .474, "22px"], night: [217, 1491, 504, 38, .644, "19px"], meadow: [153, 1478, 635, 41, .692, "8px"],
      cosmic: [185, 1482, 575, 44, .65, "hex"], training: [195, 1474, 552, 46, .806, "23px"],
    };
    let raf = 0, timer = 0, pct = 0;
    const tr = $("#ldTrough"), fill = $("#ldFill");
    function layout(id) {
      const [x, y, w, h, p0, r] = BARS[id];
      $("#ldBg").src = `assets/load/${id}.jpg`;
      Object.assign(tr.style, { left: x + "px", top: y + "px", width: w + "px", height: h + "px", backgroundImage: `url("assets/load/${id}-trough.png")` });
      const hex = r === "hex" ? "polygon(16px 0, calc(100% - 16px) 0, 100% 50%, calc(100% - 16px) 100%, 16px 100%, 0 50%)" : "";
      tr.style.clipPath = hex; tr.style.borderRadius = hex ? "0" : r;
      fill.style.borderRadius = hex ? "0" : r; fill.style.clipPath = hex ? "polygon(16px 0, calc(100% - 16px) 0, 100% 50%, calc(100% - 16px) 100%, 16px 100%, 0 50%)" : "";
      return p0;
    }
    function set(v) { pct = Math.max(0, Math.min(1, v)); fill.style.width = (pct * 100).toFixed(2) + "%"; tr.setAttribute("aria-valuenow", Math.round(pct * 100)); }
    function start(id, next) {
      stop(); const p0 = layout(id);
      set(next ? 0 : p0);                                 // direct link opens at the reference fill
      Bridge.emit("LoadingStarted", { arena: id, next: next || null });
      let last = performance.now(), hold = 0;
      const step = (now) => {
        const dt = Math.min(64, now - last); last = now;
        if (pct < 1) {
          const speed = next ? 0.00034 : 0.00022;          // ≈3 s when travelling, slower loop on a direct view
          set(pct + dt * speed * (0.6 + Math.random() * 0.9));
        } else {
          hold += dt;
          if (next && hold > 350) { Bridge.emit("LoadingComplete", { arena: id }); if (next === "battle") Battle.reset(); if (next === "board") Board.reset(id); return show(next, { push: false }); }
          if (!next && hold > 900) { hold = 0; set(0); }
        }
        raf = requestAnimationFrame(step);
      };
      raf = requestAnimationFrame(step);
    }
    function stop() { cancelAnimationFrame(raf); clearTimeout(timer); }
    return { start, stop, set, bars: BARS };
  })();

  /* ------------------------------------------------------------------
     BOARD BATTLE  (4 players, turn phases)
  ------------------------------------------------------------------ */
  const Board = (() => {
    const PHASES = [["draw", "Draw", 96, 146], ["awaken", "Awaken", 240, 137], ["battle", "Battle", 368, 203], ["resolve", "Resolve", 564, 140], ["end", "End", 704, 146]];
    const PL = [
      { id: "p1", name: "P1 Blue", ctl: "CPU", pc: "#1e8fff", pc2: "#9fe0ff", tag: "#1d5fd6", x: 21, y: 184, home: [328, 714] },
      { id: "p2", name: "P2 Tavi", ctl: "CPU", pc: "#f0a812", pc2: "#ffe68a", tag: "#d99a0f", x: 651, y: 184, home: [662, 714] },
      { id: "p3", name: "P3 Vex", ctl: "CPU", pc: "#e01e2b", pc2: "#ff9a9a", tag: "#d22a1f", x: 21, y: 367, home: [319, 936] },
      { id: "p4", name: "P4 Moss", ctl: "CPU", pc: "#25b336", pc2: "#a6f59f", tag: "#1f9b2c", x: 651, y: 367, home: [675, 945] },
    ];
    const CARDS = Battle.hand;
    const CENTER = [470, 806], RING = [372, 300];
    const HAND_X = [178, 299, 419, 537, 653];
    const B = { arena: "frozen", phase: 2, round: 1, rounds: 8, deck: 24, disc1: 0, disc2: 0, awaken: 3, awakenMax: 4, sel: "storm",
      hand: CARDS.map((c) => ({ id: c.id, used: false })), aim: null, busy: false, active: false,
      players: PL.map((p) => ({ score: 0, ep: 0, dots: 4, pos: p.home.slice() })) };
    let timers = [];
    const later = (fn, ms) => timers.push(setTimeout(fn, ms));

    function build() {
      const ph = $("#kPhases");
      PHASES.forEach(([id, label, x, w]) => {
        const b = el("button", "phase"); b.dataset.p = id; b.style.left = x - 48 + "px"; b.style.width = w + "px";
        b.setAttribute("role", "tab"); b.setAttribute("data-ui", "Board.Phase." + id);
        b.innerHTML = `<span class="stud" style="--x:50%;--y:2px"></span>${label}<span class="stud" style="--x:50%;--y:66px"></span>`;
        b.onclick = () => { Bridge.emit("PhaseTapped", { phase: id }); toast(PHASE_HELP[id]); };
        ph.appendChild(b);
      });
      $("#kPanels").innerHTML = PL.map((p, i) => {
        const right = p.x > 400;
        return `<div class="gf ppanel glow" data-i="${i}" style="--x:${p.x}px;--y:${p.y}px;--w:268px;--h:170px;--pc:${p.pc};--pc2:${p.pc2}">
          <img class="gemc" src="assets/ui/cta-gem-left.png" alt="" style="${right ? "right:-22px" : "left:-22px"}">
          <img class="por" src="assets/board/portrait-${p.id}.png" alt="" style="left:${right ? 8 : 12}px">
          <span class="nm" style="left:${right ? 117 : 127}px">${p.name}</span>
          <span class="ctl" style="left:${right ? 117 : 126}px">${p.ctl}</span>
          <span class="score">0</span>
          <span class="pdots" style="left:${right ? 117 : 121}px"><b></b><b></b><b></b><b></b></span>
          <span class="epl">EPISODE <span class="epv">0</span>/100</span>
          <span class="epbar"><i></i></span>
        </div>`;
      }).join("");
      $("#kMarbles").innerHTML = PL.map((p, i) => `<div class="marble" data-i="${i}"><span class="mtag" style="background:${p.tag};border-top-color:${p.tag}">${p.id.toUpperCase()}</span><img src="assets/board/marble-${p.id}.png" alt="${p.name}"></div>`).join("");
      $("#kBoard").addEventListener("pointerdown", aimAt);
      $("#kActivate").onclick = activate;
      $("#kAwaken").onclick = awaken;
      $('[data-ui="Board.Settings"]').onclick = () => show("settings");
      $('[data-ui="Board.Deck"]').onclick = () => toast(`${B.deck} cards left in deck`);
      $('[data-ui="Board.Discard"]').onclick = () => toast(`${B.disc1} cards played`);
    }
    const PHASE_HELP = { draw: "Draw: refill your hand", awaken: "Awaken: charge your Zoogi", battle: "Battle: aim and activate a card", resolve: "Resolve: rivals take their rolls", end: "End: round wrap-up" };

    function render() {
      $$("#kPhases .phase").forEach((b, i) => { b.classList.toggle("on", i === B.phase); b.setAttribute("aria-selected", i === B.phase); });
      $("#kRound").textContent = B.round; $("#kRounds").textContent = B.rounds;
      $("#kArena").textContent = (ARENAS.find((a) => a.id === B.arena) || ARENAS[1]).name;
      B.players.forEach((st, i) => {
        const pn = $(`#kPanels .ppanel[data-i="${i}"]`);
        pn.querySelector(".score").textContent = st.score;
        pn.querySelector(".epv").textContent = st.ep;
        pn.querySelector(".epbar i").style.setProperty("--v", Math.min(100, st.ep) + "%");
        $$(".pdots b", pn).forEach((d, k) => d.classList.toggle("on", k < st.dots));
        const m = $(`#kMarbles .marble[data-i="${i}"]`); m.style.left = st.pos[0] + "px"; m.style.top = st.pos[1] + "px";
      });
      const h = $("#kHand"); h.innerHTML = "";
      B.hand.forEach((hc, i) => {
        const c = CARDS.find((x) => x.id === hc.id);
        const b = el("button", "kcard" + (hc.id === B.sel && !hc.used ? " sel" : "") + (hc.used ? " used" : ""));
        b.style.left = HAND_X[i] + "px"; b.dataset.id = hc.id; b.setAttribute("role", "radio"); b.setAttribute("aria-checked", hc.id === B.sel);
        b.setAttribute("aria-label", `${c.name}, cost ${c.cost}`); b.setAttribute("data-ui", "Board.Card." + c.id);
        b.innerHTML = `<span class="stud" style="--x:50%;--y:-4px"></span><img class="art" src="assets/battle/card-${c.id}.jpg" alt=""><span class="kc"><b>${c.cost}</b></span>
          <span class="t" style="font-size:${c.name.length > 9 ? 13 : 15}px">${c.name}</span><span class="ptr"></span><span class="stud" style="--x:50%;--y:164px"></span>`;
        b.onclick = () => { B.sel = hc.id; render(); Bridge.emit("BoardCardSelected", { id: hc.id }); };
        h.appendChild(b);
      });
      const c = CARDS.find((x) => x.id === B.sel);
      $("#kdImg").src = `assets/battle/card-${c.id}.jpg`; $("#kdTitle").textContent = c.name; $("#kdDesc").textContent = c.desc; $("#kdCost").textContent = c.cost;
      const t = $("#kdTag"); t.textContent = c.tag; ["a", "b", "c", "g"].forEach((k, i) => t.style.setProperty("--tag-" + k, c.c[i]));
      $("#kDeckN").textContent = B.deck; $("#kDisc1").textContent = Math.min(5, B.disc1); $("#kDisc2").textContent = Math.min(5, B.disc2);
      $("#kAwkPips").innerHTML = Array.from({ length: B.awakenMax }, (_, i) => `<b class="${i < B.awaken ? "on" : ""}"></b>`).join("");
      const selUsed = (B.hand.find((x) => x.id === B.sel) || {}).used;
      $("#kActivate").disabled = B.busy || B.phase !== 2 || !!selUsed;
    }
    function drawAim(x1, y1, x2, y2) {
      const l = $("#kAimLine"); l.setAttribute("x1", x1); l.setAttribute("y1", y1); l.setAttribute("x2", x2); l.setAttribute("y2", y2); l.style.display = "";
    }
    function canvasPoint(e) {
      const r = stage.getBoundingClientRect(), k = r.width / W;
      return [Math.round((e.clientX - r.left) / k), Math.round((e.clientY - r.top) / k)];
    }
    function aimAt(e) {
      if (B.busy) return;
      const [x, y] = canvasPoint(e); B.aim = [x, y];
      const p = B.players[0].pos, dx = x - p[0], dy = y - p[1], d = Math.hypot(dx, dy) || 1;
      drawAim(p[0] + dx / d * 30, p[1] + dy / d * 30, x, y);
      Bridge.emit("BoardAim", { x, y });
      if (B.phase !== 2) toast("Aim set. Activate in the Battle phase");
    }
    const inRing = ([x, y]) => ((x - CENTER[0]) / RING[0]) ** 2 + ((y - CENTER[1]) / RING[1]) ** 2 <= 1;
    const points = ([x, y]) => { const d = Math.hypot(x - CENTER[0], (y - CENTER[1]) * 1.25); return d < 70 ? 3 : d < 150 ? 2 : d < 240 ? 1 : 0; };
    function roll(i, to, ms = 900) {
      const m = $(`#kMarbles .marble[data-i="${i}"]`); m.classList.remove("rolling"); void m.offsetWidth; m.classList.add("rolling");
      B.players[i].pos = to; m.style.left = to[0] + "px"; m.style.top = to[1] + "px";
    }
    function award(i, pts, why) {
      const st = B.players[i]; if (!pts) return;
      st.score += pts; st.ep = Math.min(100, st.ep + pts * 4);
      const sc = $(`#kPanels .ppanel[data-i="${i}"] .score`); sc.classList.remove("bump"); void sc.offsetWidth; sc.classList.add("bump");
      Bridge.emit("BoardScore", { player: PL[i].id, points: pts, total: st.score, reason: why || "" });
    }
    function settle(i) {
      const st = B.players[i];
      if (!inRing(st.pos)) { st.dots = Math.max(0, st.dots - 1); later(() => { roll(i, PL[i].home.slice(), 600); render(); }, 950); return 0; }
      return points(st.pos);
    }
    function banner(txt) { const b = $("#kBanner"); b.textContent = txt; b.classList.remove("show"); void b.offsetWidth; b.classList.add("show"); }
    function setPhase(k) {
      B.phase = k; render(); Bridge.emit("PhaseChanged", { phase: PHASES[k][0], round: B.round });
      $$("#kPanels .ppanel").forEach((p) => p.classList.toggle("turn", k === 2 && p.dataset.i === "0"));
    }
    function activate() {
      const hc = B.hand.find((x) => x.id === B.sel);
      if (B.phase !== 2) return toast("Wait for the Battle phase");
      if (!hc || hc.used) return toast("Pick a card first");
      const c = CARDS.find((x) => x.id === B.sel);
      B.busy = true; hc.used = true; B.disc1 += 1; B.awaken = Math.min(B.awakenMax, B.awaken + (c.id === "orb-relay" ? 2 : 1));
      const btn = $("#kActivate"); btn.classList.remove("fired"); void btn.offsetWidth; btn.classList.add("fired");
      const aim = B.aim || CENTER; const spread = c.id === "precision" ? 0 : 26;
      const to = [aim[0] + (Math.random() - .5) * spread, aim[1] + (Math.random() - .5) * spread];
      $("#kAimLine").style.display = "none";
      Bridge.emit("BoardActivate", { card: c.id, aim: to });
      if (c.id !== "guard") roll(0, to);
      later(() => {
        // card effects on rivals
        if (c.id === "storm" || c.id === "heavy-roll") {
          const push = c.id === "heavy-roll" ? 150 : 110, reach = c.id === "heavy-roll" ? 160 : 220;
          B.players.forEach((st, i) => {
            if (!i) return; const dx = st.pos[0] - to[0], dy = st.pos[1] - to[1], d = Math.hypot(dx, dy) || 1;
            if (d < reach) roll(i, [st.pos[0] + dx / d * push, st.pos[1] + dy / d * push], 700);
          });
        }
        if (c.id === "guard") { B.players[0].dots = Math.min(4, B.players[0].dots + 1); toast("Shield up"); }
        const pts = c.id === "guard" ? 0 : settle(0) * (c.id === "heavy-roll" ? 2 : 1);
        award(0, pts, c.id);
        toast(pts ? `${c.name}: +${pts}` : c.id === "guard" ? "Guarded" : `${c.name}: no points`);
        render();
        later(() => setPhase(3), 700);
        later(resolveCPUs, 1100);
      }, c.id === "guard" ? 200 : 950);
      render();
    }
    function resolveCPUs() {
      [1, 2, 3].forEach((i, k) => later(() => {
        const ang = Math.random() * Math.PI * 2, r = 30 + Math.random() * 260;
        roll(i, [CENTER[0] + Math.cos(ang) * r * 1.25, CENTER[1] + Math.sin(ang) * r]);
        later(() => { award(i, settle(i), "cpu"); render(); }, 950);
      }, k * 420));
      later(() => setPhase(4), 2500);
      later(endRound, 3200);
    }
    function endRound() {
      const unused = B.hand.find((x) => !x.used && x.id !== B.sel);
      if (unused) { unused.used = true; B.disc2 += 1; }
      Bridge.emit("RoundEnded", { round: B.round, scores: B.players.map((p) => p.score) });
      if (B.round >= B.rounds) return finish();
      B.round += 1; banner("Round " + B.round);
      later(() => {
        setPhase(0);
        B.hand.forEach((h) => { if (h.used && B.deck > 0) { h.used = false; B.deck -= 1; } });
        if (B.hand.every((h) => h.used)) toast("Deck empty");
        render();
      }, 900);
      later(() => setPhase(1), 1700);
      later(() => { B.busy = false; setPhase(2); }, 2500);
    }
    async function finish() {
      const best = B.players.reduce((a, p, i) => (p.score > B.players[a].score ? i : a), 0);
      banner(best === 0 ? "Victory!" : "Defeat");
      Bridge.emit("MatchEnded", { winner: PL[best].id, scores: B.players.map((p) => p.score) });
      await new Promise((r) => later(r, 1300));
      const again = await confirmBox("Match Over", `<b>${PL[best].name}</b> wins with <b>${B.players[best].score}</b> points.`, "Rematch", "Exit");
      if (again) reset(B.arena); else show("arena");
    }
    function awaken() {
      if (B.awaken < B.awakenMax) return toast(`Charge ${B.awakenMax - B.awaken} more`);
      B.awaken = 0; const b = $("#kAwaken"); b.classList.remove("fired"); void b.offsetWidth; b.classList.add("fired");
      award(0, 3, "awaken"); banner("Awaken!"); render(); Bridge.emit("BoardAwaken", {});
    }
    function reset(arena) {
      timers.forEach(clearTimeout); timers = [];
      Object.assign(B, { arena: arena || B.arena, phase: 2, round: 1, deck: 24, disc1: 0, disc2: 0, awaken: 3, sel: "storm", aim: null, busy: false });
      B.hand = CARDS.map((c) => ({ id: c.id, used: false }));
      B.players.forEach((st, i) => Object.assign(st, { score: 0, ep: 0, dots: 4, pos: PL[i].home.slice() }));
      drawAim(350, 932, 424, 904);                          // reference state: P3 lining up a shot
      render();
    }
    function setActive(on) { B.active = on; if (!on) { timers.forEach(clearTimeout); timers = []; B.busy = false; } }
    return { build, render, reset, setActive, state: B, players: PL };
  })();

  /* ------------------------------------------------------------------
     ENGINE → UI API  (call from Unreal: ExecuteJavascript("ZoogiUI.show('shop')"))
  ------------------------------------------------------------------ */
  window.ZoogiUI = {
    show: (s) => show(s),
    back,
    setPlayer: (p) => { Object.assign(S.player, p); renderTop(); },
    setCurrency: ({ gems, coins } = {}) => { if (gems != null) S.gems = gems; if (coins != null) S.coins = coins; renderTop(); },
    setNotifications: (n) => { S.notifications = n; renderTop(); },
    setDecks: (decks) => { S.decks = decks; S.deckIdx = 0; renderDeck(); },
    setUnlockedEpisodes: (n) => { S.unlocked = n; renderStory(); },
    purchaseResult: ({ sku, ok, gems, coins }) => { const it = [...POPULAR, ...Object.values(SHOP_FEATURED), ...BUNDLES].find((x) => x.sku === sku); if (ok && it) grant(it); if (gems != null || coins != null) window.ZoogiUI.setCurrency({ gems, coins }); if (!ok) toast("Purchase failed"); },
    accountLinked: ({ provider, linked }) => { S.settings.linked[provider] = !!linked; renderSettings(); },
    setBattle: (o = {}) => { Object.assign(Battle.state, o); Battle.render(); },
    startLoading: (arena, next = "board") => { S.loadArena = arena; S.loadNext = next; S.loadLoop = false; show("loading"); },
    setBoard: (o = {}) => { Object.assign(Board.state, o); Board.render(); },
    setBoardPlayer: (i, o = {}) => { Object.assign(Board.state.players[i], o); Board.render(); },
    setUnitHP: (i, hp) => { const u = $(`#bUnits .unit[data-i="${i}"]`); if (u) u.style.setProperty("--hp", hp + "%"); },
    toast,
    getState: () => JSON.parse(JSON.stringify({ ...S, owned: [...S.owned] })),
    events: Bridge.log,
  };

  /* ------------------------------------------------------------------
     DEV NAVIGATOR  (index.html?dev or #deck-dev)
  ------------------------------------------------------------------ */
  const devOn = /dev/.test(location.search + location.hash);
  if (devOn) {
    const n = $("#devnav"); n.classList.add("show");
    SCREENS.forEach((s) => { const b = el("button", "", s[0].toUpperCase() + s.slice(1)); b.dataset.s = s; b.onclick = () => show(s); n.appendChild(b); });
  }
  addEventListener("keydown", (e) => {
    if (e.target.isContentEditable || e.target.tagName === "SELECT") return;
    const i = "123456789".indexOf(e.key); if (i >= 0) show(SCREENS[i]);
    if (e.key === "Escape" && !$("#modal").classList.contains("show")) back();
  });
  addEventListener("hashchange", () => { const prev = S.loadArena, h = fromHash(); if (SCREENS.includes(h) && (h !== S.screen || (h === "loading" && S.loadArena !== prev))) show(h, { push: h !== S.screen }); });

  /* ------------------------------------------------------------------
     BOOT
  ------------------------------------------------------------------ */
  bindTop(); renderTop();
  bindDeck(); renderDeck(); renderDetail();
  renderArenas(); bindArena();
  renderStory(); bindStory();
  renderShopTabs(); renderShopFeatured(); renderPopular(); renderBundles(); bindShop();
  renderSettings(); bindSettings();
  Battle.build(); Battle.render();
  Board.build(); Board.reset("frozen");
  const start = fromHash();
  show(SCREENS.includes(start) ? start : (location.hash ? "deck" : "title"), { push: false, silent: true });
  (document.fonts ? document.fonts.ready : Promise.resolve()).then(() => { positionPen(); renderDetail(); });
  Bridge.emit("UIReady", { screens: SCREENS });
  // Android / iOS inject window.ue only after the page finishes loading: say UIReady again once it shows up
  if (!window.ue) {
    let tries = 0;
    const wait = setInterval(() => {
      if (window.ue && window.ue.interface) { clearInterval(wait); Bridge.emit("UIReady", { screens: SCREENS }); }
      else if (++tries > 100) clearInterval(wait);
    }, 100);
  }
})();
