// Classements SPEEDRUN : meilleur temps de chaque joueur par categorie
//   course:<carte>  course d'une carte CHALLENGE (LA TOILE...), un temps intermediaire par porte
//   complet:<N>     toutes les cartes CHALLENGE a la suite (N cartes), un temps intermediaire par carte ; "complet" tout court
//                   est l'ancien classement a une carte (anciennes versions du jeu), il n'est plus alimente par la derniere
//   vagues100       mode VAGUES : vague 100 terminee, 0 ou 4 temps intermediaires (vagues de boss 25/50/75/100)
// Meme base D1 et meme identite que le mode CLASSE (ratings.js : table players, cle verifiee par son empreinte).
//
// Le serveur fait foi. Validation minimale des temps (anti-triche) :
//  - ticket : POST /speedrun/start au depart du run ; le rapport cite ce ticket (une seule fois, le dernier
//    demande pour cette categorie) et le temps annonce ne peut pas depasser le temps reel ecoule depuis
//    le ticket (+ TOL_MS) : un speedhack ou un temps invente sans avoir joue aussi longtemps est refuse ;
//  - categorie connue, nombre de temps intermediaires attendu, temps strictement croissants (chaque troncon
//    >= SEG_MIN_MS), le dernier egal au total ;
//  - total >= plancher de la categorie : chemin le plus court de porte en porte a la vitesse plafond VMAX.
//    Les planchers des courses sont calcules par le jeu (sr_floor_ms, src/jeu/speedrun.c) et recopies ici :
//    la ligne « speedrun : planchers » de velocity.exe --test les affiche. Nouvelle carte CHALLENGE =
//    nouvelle categorie ici, et le plancher de complet augmente d'autant (docs/CODE.md, section 4 « Speedrun »).

import { auth, all, one, run, stmt } from './ratings.js';

export const SR = {
  CATS: {
    'course:la_toile': { splits: [13], floorMs: 6975, maxAgeMs: 30 * 60 * 1000 },
    'course:le_stand': { splits: [5], floorMs: 2189, maxAgeMs: 30 * 60 * 1000 },    // precision : cibles de visee, 5 portes
    'course:le_flux': { splits: [6], floorMs: 2558, maxAgeMs: 30 * 60 * 1000 },     // grappin et cibles, 6 portes
    'complet:3': { splits: [3], floorMs: 11722, maxAgeMs: 2 * 3600 * 1000 },        // LA TOILE + LE STAND + LE FLUX (6975 + 2189 + 2558)
    complet: { splits: [1], floorMs: 6975, maxAgeMs: 2 * 3600 * 1000 },             // ancien classement (une carte), garde pour les anciennes versions du jeu
    vagues100: { splits: [0, 4], floorMs: 5 * 60 * 1000, maxAgeMs: 8 * 3600 * 1000 },   // plancher provisoire
  },
  SEG_MIN_MS: 250,        // troncon minimal entre deux temps intermediaires
  TOL_MS: 1500,           // tolerance (reseau, arrondis) sur le temps reel ecoule depuis le ticket
  RATE_PER_MIN: 60,       // requetes speedrun authentifiees par identite et par minute (compteur a part)
  TOP: 10,
};

const RUN_RE = /^[0-9a-f]{16}$/;
const NAME_RE = /^[\w.\-]{1,15}$/;
const HEX32 = /^[0-9a-f]{32}$/;

export function srSchema(db) {
  return db.batch([
    `CREATE TABLE IF NOT EXISTS sr_tickets (run TEXT PRIMARY KEY, id TEXT NOT NULL, cat TEXT NOT NULL,
     created INTEGER NOT NULL)`,
    'CREATE INDEX IF NOT EXISTS sr_tickets_owner ON sr_tickets (id, cat)',
    `CREATE TABLE IF NOT EXISTS sr_best (cat TEXT NOT NULL, id TEXT NOT NULL, ms INTEGER NOT NULL,
     splits TEXT NOT NULL, at INTEGER NOT NULL, PRIMARY KEY (cat, id))`,
    'CREATE INDEX IF NOT EXISTS sr_best_ms ON sr_best (cat, ms, at)',
  ].map((q) => db.prepare(q)));
}

const rate = (req) => ({ key: `sr:${req.id}`, perMin: SR.RATE_PER_MIN });
const maxAge = () => Math.max(...Object.values(SR.CATS).map((c) => c.maxAgeMs));

function randomHex(bytes) {
  const b = new Uint8Array(bytes);
  crypto.getRandomValues(b);
  return [...b].map((x) => x.toString(16).padStart(2, '0')).join('');
}

// rang : 1 + ceux qui ont mieux (ou autant, mais plus tot)
async function rankIn(db, cat, best) {
  return 1 + (await one(db, 'SELECT COUNT(*) AS n FROM sr_best WHERE cat = ? AND (ms < ? OR (ms = ? AND at < ?))',
    cat, best.ms, best.ms, best.at)).n;
}

// pourquoi ce run est refuse (null s'il est recevable)
export function runError(cat, ms, splits) {
  const c = SR.CATS[cat];
  if (!c) return 'categorie inconnue';
  if (!Number.isInteger(ms) || ms <= 0 || ms > c.maxAgeMs) return 'temps invalide';
  if (!Array.isArray(splits) || !c.splits.includes(splits.length)) return 'temps intermediaires';
  let prev = 0;
  for (const s of splits) {
    if (!Number.isInteger(s) || s - prev < SR.SEG_MIN_MS) return 'temps intermediaires';
    prev = s;
  }
  if (splits.length && prev !== ms) return 'temps intermediaires';
  if (ms < c.floorMs) return 'sous le plancher';
  return null;
}

// POST /speedrun/start { id, key, name, cat } -> ticket du run (le precedent de cette categorie est annule)
export async function start(db, req, now) {
  const a = await auth(db, req, now, true, rate(req));
  if (a.err) return [a.status, { ok: false, err: a.err }];
  if (!SR.CATS[req.cat]) return [400, { ok: false, err: 'categorie inconnue' }];
  if (NAME_RE.test(req.name || '') && req.name !== a.p.name) await run(db, 'UPDATE players SET name = ? WHERE id = ?', req.name, a.p.id);
  const ticket = randomHex(8);
  await db.batch([
    stmt(db, 'DELETE FROM sr_tickets WHERE created < ?', now - maxAge()),
    stmt(db, 'DELETE FROM sr_tickets WHERE id = ? AND cat = ?', a.p.id, req.cat),
    stmt(db, 'INSERT INTO sr_tickets (run, id, cat, created) VALUES (?, ?, ?, ?)', ticket, a.p.id, req.cat, now),
  ]);
  return [200, { ok: true, run: ticket, cat: req.cat }];
}

// POST /speedrun/report { id, key, cat, run, ms, splits: [ms cumules] } -> record / garde
export async function report(db, req, now) {
  const a = await auth(db, req, now, false, rate(req));
  if (a.err) return [a.status, { ok: false, err: a.err }];
  const bad = runError(req.cat, req.ms, req.splits);
  if (bad) return [400, { ok: false, err: bad }];
  if (!RUN_RE.test(req.run || '')) return [400, { ok: false, err: 'ticket invalide' }];
  // un ticket ne sert qu'une fois : lu et efface en une requete (deux rapports simultanes : un seul le trouve)
  const t = await one(db, 'DELETE FROM sr_tickets WHERE run = ? AND id = ? AND cat = ? RETURNING created',
    req.run, a.p.id, req.cat);
  if (!t) return [400, { ok: false, err: 'ticket inconnu' }];
  const c = SR.CATS[req.cat];
  if (now - t.created > c.maxAgeMs) return [400, { ok: false, err: 'ticket expire' }];
  if (req.ms > now - t.created + SR.TOL_MS) return [400, { ok: false, err: 'plus rapide que le temps reel' }];
  const old = await one(db, 'SELECT * FROM sr_best WHERE cat = ? AND id = ?', req.cat, a.p.id);
  const state = !old || req.ms < old.ms ? 'record' : 'kept';
  // le meilleur temps n'est remplace que par plus rapide (condition dans la requete elle-meme)
  const [, cur] = await db.batch([
    stmt(db, `INSERT INTO sr_best (cat, id, ms, splits, at) VALUES (?, ?, ?, ?, ?) ON CONFLICT (cat, id) DO UPDATE
              SET ms = excluded.ms, splits = excluded.splits, at = excluded.at WHERE excluded.ms < sr_best.ms`,
      req.cat, a.p.id, req.ms, JSON.stringify(req.splits), now),
    stmt(db, 'SELECT * FROM sr_best WHERE cat = ? AND id = ?', req.cat, a.p.id),
  ]);
  const best = cur.results[0];
  return [200, { ok: true, state, best: best.ms, prev: old ? old.ms : 0, rank: await rankIn(db, req.cat, best) }];
}

// GET /speedrun/top?cat=...&id=... -> les meilleurs temps, le record (avec ses temps intermediaires) et le mien
export async function top(db, cat, id) {
  if (!SR.CATS[cat]) return [400, { ok: false, err: 'categorie inconnue' }];
  const list = await all(db, `SELECT p.name, b.ms, b.splits FROM sr_best b JOIN players p ON p.id = b.id WHERE b.cat = ?
                              ORDER BY b.ms ASC, b.at ASC LIMIT ?`, cat, SR.TOP);
  const wr = list.length ? { name: list[0].name, ms: list[0].ms, splits: JSON.parse(list[0].splits) } : null;
  let me = null;
  if (HEX32.test(id || '')) {
    const b = await one(db, 'SELECT * FROM sr_best WHERE cat = ? AND id = ?', cat, id);
    if (b) me = { rank: await rankIn(db, cat, b), ms: b.ms, splits: JSON.parse(b.splits) };
  }
  const n = (await one(db, 'SELECT COUNT(*) AS n FROM sr_best WHERE cat = ?', cat)).n;
  return [200, { ok: true, cat, runners: n, top: list.map((r) => ({ name: r.name, ms: r.ms })), wr, me }];
}

// Point d'entree commun (Worker et dev-server) : renvoie [statut, objet JSON]
export async function handleSpeedrun(db, method, path, query, json, now) {
  if (method === 'GET' && path === '/speedrun/top') return top(db, query.get('cat'), query.get('id'));
  if (method !== 'POST') return [405, { ok: false, err: 'methode' }];
  if (!json || typeof json !== 'object') return [400, { ok: false, err: 'json' }];
  if (path === '/speedrun/start') return start(db, json, now);
  if (path === '/speedrun/report') return report(db, json, now);
  return [404, { ok: false, err: 'inconnu' }];
}
