/* Boldinbeauty site — edit SITE, NEWS and OPERATORS below to update the page. */

/* Paste a full URL ("https://...") to switch a link from "Coming soon" to live. */
const SITE = {
  support: {
    patreon: "https://www.patreon.com/cw/BoldInBeauty",
    buyMeACoffee: "https://buymeacoffee.com/boldinbeau9",
  },
  merchStore: "",      // store URL once it opens
  socials: [
    { name: "Discord",   mark: "D",  url: "https://discord.gg/S3ZyeZgFg" },
    { name: "X",         mark: "X",  url: "https://x.com/Bolddinbeauty" },
    { name: "Instagram", mark: "IG", url: "https://www.instagram.com/bolddinbeauty/" },
    { name: "TikTok",    mark: "TT", url: "https://www.tiktok.com/@bolddinbeauty?lang=en-GB" },
    { name: "YouTube",   mark: "YT", url: "https://www.youtube.com/@Boldinbeauty" },
  ],
};

/* Newest first. type: "news" | "dev" | "art" */
const NEWS = [
  {
    type: "news", date: "2026-10-06",
    title: "The official site is live",
    body: "This is the new home for Boldinbeauty. Development updates, operator reveals and ways to support the game will be posted here first.",
  },
  {
    type: "art", date: "2026-10-04",
    title: "All 20 operator designs drafted",
    body: "Every operator in the launch roster now has a full-body front design, from the five Rare operators up to the six Secrets. Designs are still drafts and may change before release.",
  },
  {
    type: "dev", date: "2026-10-02",
    title: "First playable demo in Unreal Engine 5",
    body: "A working demo now runs with enemies, bosses and a playable team of Luna, Judge and Narrator, along with early summoning, progression and equipment systems.",
  },
];

/* r = summon rarity (3-6). Concept names are intentional. */
const OPERATORS = [
  { id: "luna", name: "Luna", r: 4, badge: "Central trio" },
  { id: "plack", name: "Plack", r: 4, badge: "Central trio" },
  { id: "kuu", name: "Kuu", r: 4, badge: "Central trio" },
  { id: "myrcia", name: "Myrcia", r: 3 },
  { id: "fujita", name: "Fujita", r: 3 },
  { id: "ishii", name: "Ishii", r: 3 },
  { id: "shiraishi", name: "Shiraishi", r: 3 },
  { id: "arita", name: "Arita", r: 3 },
  { id: "alchemist", name: "Alchemist", r: 5 },
  { id: "blood", name: "Blood", r: 5 },
  { id: "stargazer", name: "Stargazer", r: 5 },
  { id: "gambler", name: "Gambler", r: 5 },
  { id: "weather", name: "Weather", r: 5 },
  { id: "judge", name: "Judge", r: 5 },
  { id: "narrator", name: "Narrator", r: 6 },
  { id: "jester", name: "Jester", r: 6 },
  { id: "media", name: "Media", r: 6 },
  { id: "timekeeper", name: "Timekeeper", r: 6 },
  { id: "death", name: "Death", r: 6 },
  { id: "adapt", name: "Adapt", r: 6 },
];

const RARITY = { 3: "Rare", 4: "Epic", 5: "Legendary", 6: "Secret" };
const NEWS_LABEL = { news: "Announcement", dev: "Dev update", art: "Art" };

const $ = (s, el = document) => el.querySelector(s);
const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
const isUrl = (u) => /^https?:\/\//i.test(u || "");

function fmtDate(iso) {
  const d = new Date(iso + "T00:00:00");
  return d.toLocaleDateString("en-US", { year: "numeric", month: "short", day: "numeric" });
}

function card(op, { lazy = true } = {}) {
  const stars = "★".repeat(op.r);
  const badge = op.badge ? `<span class="op-badge">${esc(op.badge)}</span>` : "";
  return `<figure class="op-card" data-r="${op.r}">
    <img src="assets/operators/${op.id}.jpg" alt="${esc(op.name)}, ${op.r}-star ${RARITY[op.r]} operator" ${lazy ? 'loading="lazy"' : ""} decoding="async">
    ${badge}
    <figcaption class="op-plate"><span class="op-stars" aria-hidden="true">${stars}</span><span class="op-name">${esc(op.name)}</span></figcaption>
  </figure>`;
}

/* ---------- hero trio ---------- */
function renderTrio() {
  const pick = ["plack", "luna", "kuu"].map((id) => OPERATORS.find((o) => o.id === id));
  $("#trio").innerHTML = pick.map((o) => card({ ...o, badge: null }, { lazy: false })).join("");
}

/* ---------- news ---------- */
let newsFilter = "all";
function renderNews() {
  const list = NEWS.filter((n) => newsFilter === "all" || n.type === newsFilter);
  $("#news-grid").innerHTML = list.length
    ? list.map((n, i) => `<article class="news-card${i === 0 && newsFilter === "all" ? " lead" : ""}">
        <div class="news-top"><span class="tag ${n.type}">${NEWS_LABEL[n.type]}</span><time datetime="${n.date}">${fmtDate(n.date)}</time></div>
        <div style="display:grid;gap:10px"><h3>${esc(n.title)}</h3><p>${esc(n.body)}</p></div>
      </article>`).join("")
    : `<p class="lede">No posts in this category yet.</p>`;
}
function bindNewsFilters() {
  $("#news-filters").addEventListener("click", (e) => {
    const b = e.target.closest("button[data-f]");
    if (!b) return;
    newsFilter = b.dataset.f;
    document.querySelectorAll("#news-filters button").forEach((x) => x.setAttribute("aria-pressed", String(x === b)));
    renderNews();
  });
}

/* ---------- operators ---------- */
let opFilter = "all";
function renderOps() {
  const list = OPERATORS
    .filter((o) => opFilter === "all" || String(o.r) === opFilter)
    .sort((a, b) => b.r - a.r);
  $("#ops-grid").innerHTML = list.map((o) => card(o)).join("");
  $("#ops-count").textContent = `${list.length} of ${OPERATORS.length} operators`;
}
function bindOpFilters() {
  $("#ops-filters").addEventListener("click", (e) => {
    const b = e.target.closest("button[data-f]");
    if (!b) return;
    opFilter = b.dataset.f;
    document.querySelectorAll("#ops-filters button").forEach((x) => x.setAttribute("aria-pressed", String(x === b)));
    renderOps();
  });
}

/* ---------- support + merch ---------- */
function linkBtn(url, label, cls = "") {
  return isUrl(url)
    ? `<a class="btn ${cls}" href="${esc(url)}" target="_blank" rel="noopener">${label}</a>`
    : `<span class="btn ${cls}" aria-disabled="true">Coming soon</span>`;
}
function renderSupport() {
  const { patreon, buyMeACoffee } = SITE.support;
  $("#patreon-status").outerHTML = `<span class="status${isUrl(patreon) ? " live" : ""}" id="patreon-status">${isUrl(patreon) ? "Open" : "Launching soon"}</span>`;
  $("#coffee-status").outerHTML = `<span class="status${isUrl(buyMeACoffee) ? " live" : ""}" id="coffee-status">${isUrl(buyMeACoffee) ? "Open" : "Launching soon"}</span>`;
  $("#patreon-btn").innerHTML = linkBtn(patreon, "Join on Patreon");
  $("#coffee-btn").innerHTML = linkBtn(buyMeACoffee, "Buy a coffee", "gold");

  $("#social-grid").innerHTML = SITE.socials.map((s) => isUrl(s.url)
    ? `<a class="social" href="${esc(s.url)}" target="_blank" rel="noopener"><span class="mark">${esc(s.mark)}</span><span class="who"><b>${esc(s.name)}</b><span>Follow</span></span></a>`
    : `<div class="social"><span class="mark">${esc(s.mark)}</span><span class="who"><b>${esc(s.name)}</b><span>Coming soon</span></span></div>`
  ).join("");

  $("#merch-btn").innerHTML = linkBtn(SITE.merchStore, "Visit the store");
  $("#merch-status").textContent = isUrl(SITE.merchStore) ? "Store open" : "Store opening later";
}

/* ---------- views + nav ---------- */
const SUPPORT_VIEW = new Set(["support", "merch", "socials"]);
function route() {
  const id = (location.hash || "#home").slice(1);
  const support = SUPPORT_VIEW.has(id);
  $("#view-home").hidden = support;
  $("#view-support").hidden = !support;
  document.querySelectorAll(".nav-links a").forEach((a) => a.setAttribute("aria-current", String(a.getAttribute("href") === "#" + id)));
  $(".nav").classList.remove("open");
  $(".menu-btn").setAttribute("aria-expanded", "false");
  const target = document.getElementById(id === "support" ? "view-support" : id);
  if (target && id !== "home") target.scrollIntoView({ block: "start" });
  else window.scrollTo(0, 0);
}

function init() {
  renderTrio();
  renderNews(); bindNewsFilters();
  renderOps(); bindOpFilters();
  renderSupport();
  $("#year").textContent = new Date().getFullYear();
  $(".menu-btn").addEventListener("click", () => {
    const open = $(".nav").classList.toggle("open");
    $(".menu-btn").setAttribute("aria-expanded", String(open));
  });
  window.addEventListener("hashchange", route);
  route();
}

if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
else init();
