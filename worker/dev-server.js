// Lobby en local pour les tests : meme logique que le Durable Object, WebSocket ecrit a la main
// (RFC 6455, trames texte uniquement) pour ne dependre d'aucun paquet npm.
// Sert aussi le classement (/ranked/...), les classements speedrun (/speedrun/...) et la connexion Discord
// (/auth/discord/...) avec la meme logique que le Worker et sa base D1, sur une base SQLite
// en memoire (node:sqlite, integre a Node 22.5+ ; vide a chaque lancement).
// Discord est remplace par un faux local : /fake-discord/authorize accepte tout de suite, avec le compte
// DISCORD_FAKE_USER (variable d'environnement, « identifiant.nom », 424242.testeur par defaut).
//   node dev-server.js [port]      (8787 par defaut)

import http from 'node:http';
import crypto from 'node:crypto';
import { pathToFileURL } from 'node:url';
import { handle, closed, newMeta } from './src/lobby.js';
import { schema, rankedResponse } from './src/ratings.js';
import { srSchema, handleSpeedrun } from './src/speedrun.js';
import { dcSchema, handleDiscord } from './src/discord.js';

// meme interface (partielle) qu'une base D1 : prepare(q).bind(...v).all() / first() / run(), batch([...])
// Chaque appel rend la main (await) comme D1 : les tests peuvent entrelacer des requetes simultanees.
export async function memoryD1() {
  const { DatabaseSync } = await import('node:sqlite');
  const db = new DatabaseSync(':memory:');
  const exec = (q, b) => {
    const results = db.prepare(q).all(...b);
    return { results, meta: { changes: db.prepare('SELECT changes() AS n').get().n } };
  };
  const tick = () => new Promise((r) => setImmediate(r));
  const statement = (q, b = []) => ({
    q, b,
    bind: (...v) => statement(q, v),
    all: async () => { await tick(); return exec(q, b); },
    run: async () => { await tick(); return exec(q, b); },
    first: async () => { await tick(); return exec(q, b).results[0] || null; },
  });
  return {
    prepare: (q) => statement(q),
    async batch(list) {   // une transaction, comme D1
      await tick();
      db.exec('BEGIN');
      try {
        const out = list.map((s) => exec(s.q, s.b));
        db.exec('COMMIT');
        return out;
      } catch (e) {
        db.exec('ROLLBACK');
        throw e;
      }
    },
  };
}

// faux Discord : le code d'autorisation est « fake.<identifiant>.<nom> », le jeton d'acces aussi
export function fakeDiscordEnv(port, user = process.env.DISCORD_FAKE_USER || '424242.testeur') {
  const reply = (o) => new Response(JSON.stringify(o), { headers: { 'Content-Type': 'application/json' } });
  return {
    DISCORD_CLIENT_ID: 'dev', DISCORD_CLIENT_SECRET: 'dev',
    DISCORD_AUTHORIZE: `http://127.0.0.1:${port}/fake-discord/authorize`,
    DISCORD_API: 'http://fake-discord',
    fakeUser: user,
    fetch: async (url, opts = {}) => {
      if (url.endsWith('/oauth2/token')) {
        const code = new URLSearchParams(opts.body).get('code') || '';
        return reply(code.startsWith('fake.') ? { access_token: code } : { error: 'invalid_grant' });
      }
      const [, id, name] = ((opts.headers || {}).Authorization || '').replace('Bearer ', '').split('.');
      return reply(id ? { id, username: name } : { message: '401: Unauthorized' });
    },
  };
}

const GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

function frame(text) {
  const p = Buffer.from(text);
  const n = p.length;
  const h = n < 126 ? Buffer.from([0x81, n]) : Buffer.from([0x81, 126, n >> 8, n & 255]);
  return Buffer.concat([h, p]);
}

export async function startDevServer(port = 8787, env = fakeDiscordEnv(port)) {
  const conns = [];
  const db = await memoryD1();
  await schema(db);
  await srSchema(db);
  await dcSchema(db);
  const server = http.createServer(async (req, res) => {
    if (req.url.startsWith('/fake-discord/authorize')) {   // accepte tout de suite, retour vers le callback
      const q = new URL(req.url, 'http://x').searchParams;
      res.writeHead(302, { Location: `${q.get('redirect_uri')}?code=fake.${env.fakeUser}&state=${q.get('state')}` });
      res.end();
      return;
    }
    if (req.url.startsWith('/auth/discord/')) {
      const chunks = [];
      for await (const ch of req) chunks.push(ch);
      const r = await handleDiscord(db, env, new Request(`http://127.0.0.1:${port}${req.url}`,
        { method: req.method, body: req.method === 'POST' ? Buffer.concat(chunks) : undefined }));
      const headers = { 'Content-Type': r.headers.get('Content-Type') || 'text/plain' };
      if (r.headers.get('Location')) headers.Location = r.headers.get('Location');
      res.writeHead(r.status, headers);
      res.end(await r.text());
      return;
    }
    const sr = req.url.startsWith('/speedrun/');
    if (!req.url.startsWith('/ranked/') && !sr) { res.end('VELOCITY lobby (dev)'); return; }
    const chunks = [];
    for await (const ch of req) chunks.push(ch);
    const body = req.method === 'POST' ? Buffer.concat(chunks) : undefined;
    const r = await rankedResponse(db, new Request(`http://127.0.0.1${req.url}`, { method: req.method, body }),
      sr ? handleSpeedrun : undefined);
    res.writeHead(r.status, { 'Content-Type': 'application/json' });
    res.end(await r.text());
  });

  server.on('upgrade', (req, socket) => {
    const key = req.headers['sec-websocket-key'];
    if (!key) { socket.destroy(); return; }
    const accept = crypto.createHash('sha1').update(key + GUID).digest('base64');
    socket.write('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n' +
                 `Sec-WebSocket-Accept: ${accept}\r\n\r\n`);
    const id = crypto.randomBytes(3).toString('hex');
    const c = { meta: newMeta(id), send: (t) => { if (!socket.destroyed) socket.write(frame(t)); } };
    conns.push(c);
    c.send(`hello ${id}`);

    let buf = Buffer.alloc(0);
    const drop = () => {
      const i = conns.indexOf(c);
      if (i < 0) return;
      conns.splice(i, 1);
      closed(conns, c);
    };
    socket.on('data', (d) => {
      buf = Buffer.concat([buf, d]);
      for (;;) {
        if (buf.length < 2) return;
        const op = buf[0] & 15;
        let len = buf[1] & 127, off = 2;
        if (len === 126) { if (buf.length < 4) return; len = buf.readUInt16BE(2); off = 4; }
        else if (len === 127) { socket.destroy(); return; }
        const masked = (buf[1] & 128) !== 0;
        if (buf.length < off + (masked ? 4 : 0) + len) return;
        const mask = masked ? buf.subarray(off, off + 4) : null;
        if (masked) off += 4;
        const payload = Buffer.from(buf.subarray(off, off + len));
        if (mask) for (let i = 0; i < len; i++) payload[i] ^= mask[i & 3];
        buf = buf.subarray(off + len);
        if (op === 8) { socket.end(); return; }
        if (op === 1) handle(conns, c, payload.toString());
      }
    });
    socket.on('close', drop);
    socket.on('error', drop);
  });

  return new Promise((resolve) => server.listen(port, '127.0.0.1', () => resolve({ server, conns, db })));
}

if (import.meta.url === pathToFileURL(process.argv[1]).href) {
  const port = parseInt(process.argv[2], 10) || 8787;
  await startDevServer(port);
  console.log(`lobby de test sur ws://127.0.0.1:${port}/ (classement : http://127.0.0.1:${port}/ranked/..., speedrun : /speedrun/..., faux Discord : /auth/discord/...)`);
}
