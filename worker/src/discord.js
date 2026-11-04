// Connexion Discord : un compte Discord = une seule identite de classement (players.id), sur tous les appareils.
// Le jeu n'embarque pas de navigateur : il ouvre la page de connexion dans celui du systeme, puis interroge le serveur.
//   POST /auth/discord/start { id, key, name, state } -> demande enregistree (state : 32 hex tires par le jeu)
//   GET  /auth/discord/go?s=STATE                     -> redirection vers la page d'autorisation de Discord
//   GET  /auth/discord/callback?code=..&state=..       -> code echange (secret du Worker), compte Discord note
//   POST /auth/discord/poll { id, key, state }        -> pending | expired | done { id, key, name, discord }
// Le state seul ne suffit pas a recuperer une identite (il passe par l'historique du navigateur) : poll exige aussi
// l'identite qui a lance la demande, et la demande ne sert qu'une fois.
// A la fin de la connexion :
//   - compte Discord deja lie a une identite P : le jeu passe sur P avec une cle neuve (table player_keys) ;
//     les autres appareils de P gardent la leur ;
//   - compte Discord nouveau : l'identite qui a lance la demande lui est liee (cote et temps gardes) ; si elle est
//     deja liee a un autre compte Discord, une identite neuve est creee pour ce compte.
// Le pseudo reste celui du jeu : /ranked/hello le met a jour pour l'identite (donc pour le compte Discord).
// Configuration (proprietaire) : DISCORD_CLIENT_ID dans wrangler.toml ([vars]), le secret par
// « npx wrangler secret put DISCORD_CLIENT_SECRET », et l'URL de redirection https://<worker>/auth/discord/callback
// declaree dans l'application Discord (OAuth2 > Redirects). Pour les tests, env.DISCORD_AUTHORIZE, env.DISCORD_API
// et env.fetch remplacent Discord (dev-server.js : faux Discord local).

import { RANK, HEX32, NAME_RE, auth, one, run, stmt, sha256hex } from './ratings.js';

export const DC = {
  EXPIRE_MS: 10 * 60 * 1000,   // une demande non terminee dans ce delai est abandonnee
  RATE_PER_MIN: 40,            // start + poll (toutes les 2 s) par identite et par minute
  AUTHORIZE: 'https://discord.com/oauth2/authorize',
  API: 'https://discord.com/api',
};

export function dcSchema(db) {
  return db.batch([
    `CREATE TABLE IF NOT EXISTS discord (did TEXT PRIMARY KEY, pid TEXT NOT NULL UNIQUE, dname TEXT NOT NULL,
     updated INTEGER NOT NULL)`,
    `CREATE TABLE IF NOT EXISTS dc_logins (state TEXT PRIMARY KEY, pid TEXT NOT NULL, created INTEGER NOT NULL,
     did TEXT, dname TEXT)`,
  ].map((q) => db.prepare(q)));
}

const DID_RE = /^\d{1,20}$/;
const rate = (req) => ({ key: `dc:${req.id}`, perMin: DC.RATE_PER_MIN });
const hexRandom = (n) => [...crypto.getRandomValues(new Uint8Array(n))].map((x) => x.toString(16).padStart(2, '0')).join('');
// nom du compte Discord tel que le jeu l'affiche : [A-Za-z0-9_.-], 32 caracteres au plus
export const dcName = (u) => String(u || '').replace(/[^\w.\-]/g, '').slice(0, 32);
// pseudo d'une identite creee par Discord : celui du compte s'il convient au jeu
const gameName = (u) => { const n = dcName(u).slice(0, 15); return NAME_RE.test(n) ? n : 'joueur'; };

const json = (status, body) => new Response(JSON.stringify(body), { status, headers: { 'Content-Type': 'application/json' } });
const esc = (s) => String(s).replace(/[&<>"']/g, (c) => `&#${c.charCodeAt(0)};`);
function page(status, title, msg) {
  const html = `<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>VELOCITY</title><body style="background:#0a0b14;color:#e8ecff;font:18px system-ui;text-align:center;padding:12vh 16px">
<h1 style="color:#7fe6ff;letter-spacing:.3em">V E L O C I T Y</h1><h2>${esc(title)}</h2><p>${esc(msg)}</p></body>`;
  return new Response(html, { status, headers: { 'Content-Type': 'text/html; charset=utf-8' } });
}

const configured = (env) => !!(env.DISCORD_CLIENT_ID && env.DISCORD_CLIENT_SECRET);
const redirectUri = (url) => `${url.origin}/auth/discord/callback`;

// demande en cours (non expiree) pour ce state
async function login(db, state, now) {
  if (!HEX32.test(state || '')) return null;
  const l = await one(db, 'SELECT * FROM dc_logins WHERE state = ?', state);
  return l && now - l.created <= DC.EXPIRE_MS ? l : null;
}

async function start(db, env, req, now) {
  if (!configured(env)) return [503, { ok: false, err: 'discord non configure' }];
  const a = await auth(db, req, now, true, rate(req));
  if (a.err) return [a.status, { ok: false, err: a.err }];
  if (!HEX32.test(req.state || '')) return [400, { ok: false, err: 'state invalide' }];
  const [, , cur] = await db.batch([
    stmt(db, 'DELETE FROM dc_logins WHERE created < ?', now - DC.EXPIRE_MS),
    stmt(db, 'INSERT OR IGNORE INTO dc_logins (state, pid, created) VALUES (?, ?, ?)', req.state, a.p.id, now),
    stmt(db, 'SELECT pid FROM dc_logins WHERE state = ?', req.state),
  ]);
  if (cur.results[0].pid !== a.p.id) return [409, { ok: false, err: 'state deja pris' }];
  return [200, { ok: true }];
}

// fin de la connexion : l'identite du compte Discord (cle neuve si ce n'est pas celle qui a fait la demande)
async function poll(db, req, now) {
  const a = await auth(db, req, now, false, rate(req));
  if (a.err) return [a.status, { ok: false, err: a.err }];
  const l = await login(db, req.state, now);
  if (!l || l.pid !== a.p.id) return [200, { ok: true, state: 'expired' }];
  if (!l.did) return [200, { ok: true, state: 'pending' }];
  // la demande ne sert qu'une fois (deux poll simultanes : un seul la consomme)
  const used = await run(db, 'DELETE FROM dc_logins WHERE state = ? AND did IS NOT NULL', req.state);
  if (!used.meta.changes) return [200, { ok: true, state: 'expired' }];
  let link = await one(db, 'SELECT pid FROM discord WHERE did = ?', l.did);
  if (!link) {
    let pid = a.p.id;
    if (await one(db, 'SELECT did FROM discord WHERE pid = ?', a.p.id)) {   // deja lie a un autre compte : identite neuve
      pid = hexRandom(16);
      await run(db, 'INSERT INTO players (id, keyhash, name, mmr, played, wins, updated) VALUES (?, ?, ?, ?, 0, 0, ?)',
        pid, await sha256hex(hexRandom(16)), gameName(l.dname), RANK.START, now);
    }
    // OR IGNORE : deux connexions simultanees du meme compte Discord ne le lient qu'une fois
    await run(db, 'INSERT OR IGNORE INTO discord (did, pid, dname, updated) VALUES (?, ?, ?, ?)', l.did, pid, l.dname, now);
    link = await one(db, 'SELECT pid FROM discord WHERE did = ?', l.did);
  } else {
    await run(db, 'UPDATE discord SET dname = ?, updated = ? WHERE did = ?', l.dname, now, l.did);
  }
  if (link.pid === a.p.id) return [200, { ok: true, state: 'done', id: a.p.id, key: req.key, name: a.p.name, discord: l.dname }];
  const key = hexRandom(16);
  await run(db, 'INSERT INTO player_keys (keyhash, pid, created) VALUES (?, ?, ?)', await sha256hex(key), link.pid, now);
  const p = await one(db, 'SELECT name FROM players WHERE id = ?', link.pid);
  return [200, { ok: true, state: 'done', id: link.pid, key, name: p.name, discord: l.dname }];
}

async function go(db, env, url, now) {
  if (!configured(env)) return page(503, 'Connexion Discord indisponible', 'Le serveur n\'est pas encore configure pour Discord.');
  const state = url.searchParams.get('s');
  const l = await login(db, state, now);
  if (!l || l.did) return page(400, 'Lien expire', 'Relance la connexion depuis le menu de VELOCITY.');
  const q = new URLSearchParams({ client_id: env.DISCORD_CLIENT_ID, response_type: 'code', scope: 'identify',
    redirect_uri: redirectUri(url), state, prompt: 'consent' });
  return new Response(null, { status: 302, headers: { Location: `${env.DISCORD_AUTHORIZE || DC.AUTHORIZE}?${q}` } });
}

async function callback(db, env, url, now) {
  const state = url.searchParams.get('state'), code = url.searchParams.get('code');
  const l = await login(db, state, now);
  if (!l || l.did) return page(400, 'Lien expire', 'Relance la connexion depuis le menu de VELOCITY.');
  if (!code) return page(400, 'Connexion annulee', 'Tu peux fermer cette page et revenir au jeu.');
  if (!configured(env)) return page(503, 'Connexion Discord indisponible', 'Le serveur n\'est pas configure pour Discord.');
  const api = env.DISCORD_API || DC.API, f = env.fetch || fetch;
  let user = null;
  try {
    const tok = await (await f(`${api}/oauth2/token`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: new URLSearchParams({ client_id: env.DISCORD_CLIENT_ID, client_secret: env.DISCORD_CLIENT_SECRET,
        grant_type: 'authorization_code', code, redirect_uri: redirectUri(url) }).toString(),
    })).json();
    if (tok && tok.access_token) {
      user = await (await f(`${api}/users/@me`, { headers: { Authorization: `Bearer ${tok.access_token}` } })).json();
    }
  } catch (e) { user = null; }
  if (!user || !DID_RE.test(String(user.id || ''))) return page(502, 'Discord n\'a pas repondu', 'Reessaie depuis le menu du jeu.');
  const dname = dcName(user.username) || 'discord';
  const done = await run(db, 'UPDATE dc_logins SET did = ?, dname = ? WHERE state = ? AND did IS NULL', String(user.id), dname, state);
  if (!done.meta.changes) return page(400, 'Lien expire', 'Relance la connexion depuis le menu de VELOCITY.');
  const p = await one(db, 'SELECT name FROM players WHERE id = ?', l.pid);
  return page(200, `Connecte : ${dname}`,
    `Demande faite depuis le jeu par ${p ? p.name : '?'}. Tu peux fermer cette page et revenir dans VELOCITY.`);
}

// Point d'entree (Worker et dev-server) : /auth/discord/...
export async function handleDiscord(db, env, request, now = Date.now()) {
  const url = new URL(request.url), path = url.pathname;
  if (request.method === 'GET' && path === '/auth/discord/go') return go(db, env, url, now);
  if (request.method === 'GET' && path === '/auth/discord/callback') return callback(db, env, url, now);
  if (request.method !== 'POST') return json(405, { ok: false, err: 'methode' });
  const text = await request.text();
  if (text.length > 4096) return json(413, { ok: false, err: 'trop long' });
  let req = null;
  try { req = JSON.parse(text); } catch (e) { req = null; }
  if (!req || typeof req !== 'object') return json(400, { ok: false, err: 'json' });
  const [status, body] = path === '/auth/discord/start' ? await start(db, env, req, now)
    : path === '/auth/discord/poll' ? await poll(db, req, now) : [404, { ok: false, err: 'inconnu' }];
  return json(status, body);
}
