// Classement du mode CLASSE : le serveur tient les cotes et calcule l'Elo.
// Logique sans dependance, sur une base SQL au format de ctx.storage.sql (Durable Object SQLite) :
//   sql.exec(requete, ...valeurs) -> curseur avec toArray()
// dev-server.js fournit le meme objet au-dessus de node:sqlite (base en memoire).
//
// La formule (Elo multijoueur par paires) est documentee a un seul endroit : docs/CODE.md, section
// « Mode classe ». main.c (rank_delta, rank_tier) applique exactement les memes constantes pour ses tests
// et pour l'estimation affichee en attendant la reponse du serveur.

export const RANK = {
  START: 1000,          // cote de depart
  K: 32,                // facteur K apres le placement
  K_PLACE: 64,          // facteur K pendant les matchs de placement
  PLACEMENT: 10,        // nombre de matchs de placement
  MIN: 100, MAX: 3000,  // bornes de la cote
  FLOORS: [0, 900, 1050, 1200, 1350, 1500],
  NAMES: ['BRONZE', 'ARGENT', 'OR', 'PLATINE', 'DIAMANT', 'MAITRE'],
  EXPIRE_MS: 10 * 60 * 1000,      // un match non confirme dans ce delai est abandonne
  KEEP_MS: 7 * 24 * 3600 * 1000,  // les matchs termines sont gardes (anti-rejeu) puis purges
  RATE_PER_MIN: 30,               // requetes authentifiees par identite et par minute
  TOP: 10,
};

const HEX32 = /^[0-9a-f]{32}$/;
const MID_RE = /^[0-9a-f]{16}$/;
const NAME_RE = /^[\w.\-]{1,15}$/;

// --- Elo ---------------------------------------------------------------------------------------------
export function expectScore(ra, rb) { return 1 / (1 + Math.pow(10, (rb - ra) / 400)); }
export function kFactor(played) { return played < RANK.PLACEMENT ? RANK.K_PLACE : RANK.K; }
const roundAway = (x) => Math.sign(x) * Math.round(Math.abs(x));   // comme lroundf en C
export const clampMmr = (m) => Math.min(RANK.MAX, Math.max(RANK.MIN, m));

// variation contre n adversaires : K / n * somme(S - E), arrondie (0,5 s'eloigne de zero)
export function eloDelta(me, played, opps /* [{ mmr, s }] */) {
  if (!opps.length) return 0;
  let s = 0;
  for (const o of opps) s += o.s - expectScore(me, o.mmr);
  return roundAway(kFactor(played) * s / opps.length);
}

export function tierOf(m) {
  let t = 0;
  while (t + 1 < RANK.NAMES.length && m >= RANK.FLOORS[t + 1]) t++;
  return t;
}
// "OR II" : division III (bas du palier) a I (haut) ; MAITRE sans division
export function tierName(m) {
  const t = tierOf(m);
  if (t === RANK.NAMES.length - 1) return RANK.NAMES[t];
  const lo = t ? RANK.FLOORS[t] : RANK.FLOORS[1] - (RANK.FLOORS[2] - RANK.FLOORS[1]);
  const hi = RANK.FLOORS[t + 1];
  const d = Math.min(3, Math.max(1, 3 - Math.trunc(3 * (m - lo) / (hi - lo))));
  return `${RANK.NAMES[t]} ${['', 'I', 'II', 'III'][d]}`;
}
export function rankLabel(m, played) {
  return played < RANK.PLACEMENT ? `PLACEMENT ${played}/${RANK.PLACEMENT}` : tierName(m);
}

// Score de a contre b dans un rapport : le gagnant designe par l'hote passe devant, puis les frags (egalite 0,5) ;
// tout present bat tout parti ; entre partis, le dernier parti passe devant le premier.
export function pairScore(body, a, b) {
  const ia = body.left.indexOf(a), ib = body.left.indexOf(b);
  if (ia < 0 && ib >= 0) return 1;
  if (ia >= 0 && ib < 0) return 0;
  if (ia >= 0) return ia > ib ? 1 : ia < ib ? 0 : 0.5;
  const pa = body.players.find((p) => p.id === a), pb = body.players.find((p) => p.id === b);
  const wa = body.winner === a, wb = body.winner === b;
  if (wa !== wb) return wa ? 1 : 0;
  return pa.kills > pb.kills ? 1 : pa.kills < pb.kills ? 0 : 0.5;
}

// variations de tous les participants (presents et partis) ; rows : id -> { mmr, played }
export function matchDeltas(body, rows) {
  const ids = [...body.players.map((p) => p.id), ...body.left];
  const out = {};
  for (const a of ids) {
    const opps = ids.filter((b) => b !== a).map((b) => ({ mmr: rows[b].mmr, s: pairScore(body, a, b) }));
    out[a] = eloDelta(rows[a].mmr, rows[a].played, opps);
  }
  return out;
}

// --- base ----------------------------------------------------------------------------------------------
export function schema(sql) {
  sql.exec(`CREATE TABLE IF NOT EXISTS players (id TEXT PRIMARY KEY, keyhash TEXT NOT NULL, name TEXT NOT NULL,
            mmr INTEGER NOT NULL, played INTEGER NOT NULL, wins INTEGER NOT NULL, updated INTEGER NOT NULL)`);
  sql.exec('CREATE INDEX IF NOT EXISTS players_mmr ON players (played, mmr)');
  sql.exec(`CREATE TABLE IF NOT EXISTS matches (mid TEXT PRIMARY KEY, created INTEGER NOT NULL, done INTEGER NOT NULL,
            result TEXT)`);
  sql.exec(`CREATE TABLE IF NOT EXISTS reports (mid TEXT NOT NULL, reporter TEXT NOT NULL, body TEXT NOT NULL,
            at INTEGER NOT NULL, PRIMARY KEY (mid, reporter))`);
  sql.exec('CREATE TABLE IF NOT EXISTS rate (id TEXT PRIMARY KEY, win INTEGER NOT NULL, n INTEGER NOT NULL)');
}

const rows = (sql, q, ...b) => sql.exec(q, ...b).toArray();
const one = (sql, q, ...b) => rows(sql, q, ...b)[0] || null;

export async function sha256hex(text) {
  const d = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(text));
  return [...new Uint8Array(d)].map((x) => x.toString(16).padStart(2, '0')).join('');
}

function limited(sql, id, now) {
  const win = Math.floor(now / 60000);
  const r = one(sql, 'SELECT win, n FROM rate WHERE id = ?', id);
  if (!r || r.win !== win) { sql.exec('INSERT OR REPLACE INTO rate (id, win, n) VALUES (?, ?, 1)', id, win); return false; }
  if (r.n >= RANK.RATE_PER_MIN) return true;
  sql.exec('UPDATE rate SET n = n + 1 WHERE id = ?', id);
  return false;
}

function playerView(p) {
  return { mmr: p.mmr, played: p.played, wins: p.wins, tier: rankLabel(p.mmr, p.played) };
}

// identite : premiere requete = enregistrement de l'empreinte de la cle (confiance au premier usage)
async function auth(sql, req, now, create) {
  if (!HEX32.test(req.id || '') || !HEX32.test(req.key || '')) return { err: 'identite invalide', status: 400 };
  if (limited(sql, req.id, now)) return { err: 'trop de requetes', status: 429 };
  const kh = await sha256hex(req.key);
  let p = one(sql, 'SELECT * FROM players WHERE id = ?', req.id);
  if (!p) {
    if (!create) return { err: 'inconnu', status: 404 };
    const name = NAME_RE.test(req.name || '') ? req.name : 'joueur';
    // OR IGNORE : deux premieres requetes simultanees ne creent qu'une ligne, la seconde est verifiee ci-dessous
    sql.exec('INSERT OR IGNORE INTO players (id, keyhash, name, mmr, played, wins, updated) VALUES (?, ?, ?, ?, 0, 0, ?)',
      req.id, kh, name, RANK.START, now);
    p = one(sql, 'SELECT * FROM players WHERE id = ?', req.id);
  }
  if (p.keyhash !== kh) return { err: 'mauvaise cle', status: 403 };
  return { p };
}

function rankOf(sql, p) {
  if (p.played < RANK.PLACEMENT) return 0;
  return 1 + one(sql, 'SELECT COUNT(*) AS n FROM players WHERE played >= ? AND mmr > ?', RANK.PLACEMENT, p.mmr).n;
}

// POST /ranked/hello { id, key, name } -> cote, palier, rang
export async function hello(sql, req, now) {
  const a = await auth(sql, req, now, true);
  if (a.err) return [a.status, { ok: false, err: a.err }];
  if (NAME_RE.test(req.name || '') && req.name !== a.p.name) {
    sql.exec('UPDATE players SET name = ? WHERE id = ?', req.name, a.p.id);
  }
  return [200, { ok: true, ...playerView(a.p), rank: rankOf(sql, a.p) }];
}

// GET /ranked/top?id=... -> les meilleurs (placement termine) et le rang de celui qui demande
export function top(sql, id) {
  const list = rows(sql, 'SELECT name, mmr, played FROM players WHERE played >= ? ORDER BY mmr DESC, updated ASC LIMIT ?',
    RANK.PLACEMENT, RANK.TOP).map((p) => ({ name: p.name, mmr: p.mmr, tier: tierName(p.mmr), played: p.played }));
  let me = null;
  if (HEX32.test(id || '')) {
    const p = one(sql, 'SELECT * FROM players WHERE id = ?', id);
    if (p) me = { rank: rankOf(sql, p), ...playerView(p) };
  }
  return [200, { ok: true, top: list, me }];
}

// forme canonique d'un rapport : deux rapports concordent si elle est identique
function canonical(req) {
  const players = [...req.players].map((p) => ({ id: p.id, kills: p.kills })).sort((a, b) => (a.id < b.id ? -1 : 1));
  return JSON.stringify({ host: req.host, winner: req.winner || '', players, left: [...req.left] });
}

function validReport(req) {
  if (!MID_RE.test(req.match || '') || !HEX32.test(req.host || '')) return false;
  if (!Array.isArray(req.players) || !Array.isArray(req.left)) return false;
  if (req.players.length < 1 || req.players.length > 8 || req.left.length > 8) return false;
  const ids = [...req.players.map((p) => p && p.id), ...req.left];
  if (ids.length < 2 || !ids.every((x) => HEX32.test(x || '')) || new Set(ids).size !== ids.length) return false;
  if (!req.players.every((p) => Number.isInteger(p.kills) && p.kills >= -99 && p.kills <= 999)) return false;
  if (!ids.includes(req.host)) return false;
  if (req.winner && !req.players.some((p) => p.id === req.winner)) return false;
  return req.players.some((p) => p.id === req.id);   // on ne rapporte qu'un match auquel on a participe
}

// Un groupe de rapports identiques est applique si :
//  - une majorite stricte des joueurs presents (dans ce rapport) l'a envoye (1 contre 1 : les deux) ;
//  - s'il declare des joueurs partis, l'hote fait partie du groupe (c'est lui qui les liste),
//    ou bien l'hote est lui-meme parti et au moins deux joueurs confirment.
function accepted(body, reporters) {
  const present = body.players.map((p) => p.id);
  const agree = reporters.filter((r) => present.includes(r));
  if (agree.length * 2 <= present.length) return false;
  if (!body.left.length) return true;
  if (agree.includes(body.host)) return true;
  return body.left.includes(body.host) && agree.length >= 2;
}

function apply(sql, mid, body, now) {
  const ids = [...body.players.map((p) => p.id), ...body.left];
  const byId = {};
  for (const id of ids) byId[id] = one(sql, 'SELECT * FROM players WHERE id = ?', id);
  const d = matchDeltas(body, byId);
  const result = {};
  for (const id of ids) {
    const p = byId[id];
    const mmr = clampMmr(p.mmr + d[id]), played = p.played + 1, wins = p.wins + (body.winner === id ? 1 : 0);
    sql.exec('UPDATE players SET mmr = ?, played = ?, wins = ?, updated = ? WHERE id = ?', mmr, played, wins, now, id);
    result[id] = { delta: mmr - p.mmr, mmr, played, wins, tier: rankLabel(mmr, played) };
  }
  sql.exec('UPDATE matches SET done = 1, result = ? WHERE mid = ?', JSON.stringify(result), mid);
  return result;
}

// POST /ranked/report { id, key, match, host, winner, players: [{ id, kills }], left: [id] }
export async function report(sql, req, now) {
  const a = await auth(sql, req, now, false);
  if (a.err) return [a.status, { ok: false, err: a.err }];
  if (!validReport(req)) return [400, { ok: false, err: 'rapport invalide' }];
  sql.exec('DELETE FROM reports WHERE at < ?', now - RANK.KEEP_MS);
  sql.exec('DELETE FROM matches WHERE created < ?', now - RANK.KEEP_MS);
  let m = one(sql, 'SELECT * FROM matches WHERE mid = ?', req.match);
  if (!m) {
    sql.exec('INSERT INTO matches (mid, created, done, result) VALUES (?, ?, 0, NULL)', req.match, now);
    m = one(sql, 'SELECT * FROM matches WHERE mid = ?', req.match);
  }
  const answer = (result) => {
    const me = result && result[req.id];
    return [200, me ? { ok: true, state: 'applied', ...me } : { ok: true, state: 'ignored' }];
  };
  if (m.done) return answer(JSON.parse(m.result));
  if (now - m.created > RANK.EXPIRE_MS) return [200, { ok: true, state: 'expired' }];
  const ids = [...req.players.map((p) => p.id), ...req.left];
  const known = one(sql, `SELECT COUNT(*) AS n FROM players WHERE id IN (${ids.map(() => '?').join(',')})`, ...ids).n;
  if (known !== ids.length) return [400, { ok: false, err: 'joueur inconnu' }];
  const body = canonical(req);
  sql.exec('INSERT OR REPLACE INTO reports (mid, reporter, body, at) VALUES (?, ?, ?, ?)', req.match, req.id, body, now);
  const groups = new Map();
  for (const r of rows(sql, 'SELECT reporter, body FROM reports WHERE mid = ?', req.match)) {
    if (!groups.has(r.body)) groups.set(r.body, []);
    groups.get(r.body).push(r.reporter);
  }
  for (const [b, reporters] of groups) {
    if (accepted(JSON.parse(b), reporters)) return answer(apply(sql, req.match, JSON.parse(b), now));
  }
  return [200, { ok: true, state: 'pending' }];
}

// Point d'entree commun (Durable Object et dev-server) : renvoie [statut, objet JSON]
export async function handleRanked(sql, method, path, query, json, now) {
  if (method === 'GET' && path === '/ranked/top') return top(sql, query.get('id'));
  if (method !== 'POST') return [405, { ok: false, err: 'methode' }];
  if (!json || typeof json !== 'object') return [400, { ok: false, err: 'json' }];
  if (path === '/ranked/hello') return hello(sql, json, now);
  if (path === '/ranked/report') return report(sql, json, now);
  return [404, { ok: false, err: 'inconnu' }];
}

export async function rankedResponse(sql, request) {
  const url = new URL(request.url);
  let json = null;
  if (request.method === 'POST') {
    const text = await request.text();
    if (text.length > 4096) return new Response('{"ok":false,"err":"trop long"}', { status: 413 });
    try { json = JSON.parse(text); } catch (e) { json = null; }
  }
  const [status, body] = await handleRanked(sql, request.method, url.pathname, url.searchParams, json, Date.now());
  return new Response(JSON.stringify(body), { status, headers: { 'Content-Type': 'application/json' } });
}
