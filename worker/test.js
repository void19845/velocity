// Tests du lobby : node test.js
// Lance un lobby local et joue des scenarios avec le client WebSocket integre a Node.

import { startDevServer } from './dev-server.js';
import { eloDelta, tierName, rankLabel, handleRanked, RANK } from './src/ratings.js';
import crypto from 'node:crypto';

const PORT = 8799;
const { server, sql } = await startDevServer(PORT);
let fails = 0;

function check(label, got, want) {
  const ok = JSON.stringify(got) === JSON.stringify(want);
  if (!ok) fails++;
  console.log(`${ok ? 'ok  ' : 'ECHEC'} ${label}${ok ? '' : ` : obtenu ${JSON.stringify(got)}, attendu ${JSON.stringify(want)}`}`);
}

function client() {
  const ws = new WebSocket(`ws://127.0.0.1:${PORT}/`);
  const inbox = [];
  const waiters = [];
  ws.onmessage = (e) => {
    inbox.push(e.data);
    for (const w of waiters.splice(0)) w();
  };
  const opened = new Promise((r) => { ws.onopen = r; });
  const next = async (ms = 1000) => {
    const t0 = Date.now();
    while (!inbox.length) {
      if (Date.now() - t0 > ms) return null;
      await new Promise((r) => { waiters.push(r); setTimeout(r, 50); });
    }
    return inbox.shift();
  };
  const drain = async () => { const o = []; for (let m; (m = await next(150)) !== null;) o.push(m); return o; };
  return { ws, opened, next, drain, send: (t) => ws.send(t), inbox };
}

const hello = async (c) => { await c.opened; return (await c.next()).split(' ')[0]; };

const host = client(), a = client(), b = client();
check('hello hote', await hello(host), 'hello');
check('hello a', await hello(a), 'hello');
check('hello b', await hello(b), 'hello');

a.send('list 4');
check('liste vide', await a.next(), 'end');

host.send('host 4 1 8 0 82.1.2.3:27960 192.168.1.5:27960 Hote');
check('hote accuse', await host.next(), 'ok');
const pushed = await a.next();
check('push vers abonne', pushed.replace(/ [0-9a-f]{6} /, ' ID '), 'room ID 1 8 0 82.1.2.3:27960 Hote 0 0');   // 0 0 : salon non classe, cote
const hostId = pushed.split(' ')[1];

b.send('list 4');
check('liste avec salon', ((await b.next()) || '').startsWith(`room ${hostId} 1 8 0`), true);
check('fin de liste', await b.next(), 'end');

// version differente : invisible
const old = client();
await hello(old);
old.send('list 3');
check('autre version : rien', await old.next(), 'end');

// rejoindre depuis une autre IP publique
b.send(`join ${hostId} 90.9.9.9:5000 0`);
check('go public', await b.next(), 'go 82.1.2.3:27960 0');
check('punch vers hote', await host.next(), 'punch 90.9.9.9:5000');

// meme IP publique : adresse LAN
a.send(`join ${hostId} 82.1.2.3:6000 0`);
check('go LAN (meme box)', await a.next(), 'go 192.168.1.5:27960 0');
check('punch meme box', await host.next(), 'punch 82.1.2.3:6000');

// spectateur
b.send(`join ${hostId} 90.9.9.9:5000 1`);
check('go spectateur', await b.next(), 'go 82.1.2.3:27960 1');
await host.next();

// salon plein : refus, sauf spectateur
host.send('host 4 8 8 0 82.1.2.3:27960 192.168.1.5:27960 Hote');
await a.drain(); await b.drain();
b.send(`join ${hostId} 90.9.9.9:5000 0`);
check('salon plein', await b.next(), 'err full');
b.send(`join ${hostId} 90.9.9.9:5000 1`);
check('spectateur accepte meme plein', await b.next(), 'go 82.1.2.3:27960 1');
await host.drain();

// quick play : prefere le salon le plus rempli et non plein
const host2 = client(), host3 = client();
await hello(host2); await hello(host3);
host2.send('host 4 2 8 0 5.5.5.5:27960 10.0.0.2:27960 Petit');
host3.send('host 4 5 8 0 6.6.6.6:27960 10.0.0.3:27960 Gros');
await host2.next(); await host3.next();
const q = client();
await hello(q);
q.send('quick 4 90.1.1.1:7000');
check('quick : salon le plus rempli', await q.next(), 'go 6.6.6.6:27960 0');
check('quick : punch', await host3.next(), 'punch 90.1.1.1:7000');
q.send('quick 9 90.1.1.1:7000');
check('quick : aucune partie', await q.next(), 'none');

// entrees invalides
host.send('host 4 1 8 0 pas-une-ip 192.168.1.5:27960 Hote');
check('ep invalide', await host.next(), 'err host');
host.send('host 4 1 8 0 1.2.3.4:1 1.2.3.4:1 nom avec espace');
check('nom invalide ignore (tronque au mot)', ['ok', 'err host', null].includes(await host.next()), true);
host.send('blabla');
check('commande inconnue', await host.next(), 'err cmd');
host.send('x'.repeat(500));
check('ligne trop longue ignoree', await host.next(300), null);

// fermeture de l'hote : gone
await a.drain(); await b.drain();
host3.ws.close();
check('gone a la fermeture', ((await a.next(1500)) || '').startsWith('gone '), true);

// --- mode classe : salons classes dans le lobby ---
const rk1 = client(), rk2 = client(), rq = client();
await hello(rk1); await hello(rk2); await hello(rq);
rk1.send('host 4 3 8 0 7.7.7.1:27960 10.0.0.7:27960 Fort 1 1400');
rk2.send('host 4 1 8 0 7.7.7.2:27960 10.0.0.8:27960 Proche 1 1020');
await rk1.next(); await rk2.next();
rq.send('quick 4 90.2.2.2:7000 1 1000');
check('quick classe : la cote la plus proche', await rq.next(), 'go 7.7.7.2:27960 0');
await rk2.drain();
rq.send('quick 4 90.2.2.2:7000');
check('quick normal : evite les salons classes', await rq.next(), 'go 5.5.5.5:27960 0');
rq.send('list 4');
const listed = (await rq.drain()).filter((l) => l.startsWith('room ') && l.includes(' Fort '));
check('salon classe liste avec sa cote', listed.length === 1 && listed[0].endsWith(' Fort 1 1400'), true);
rq.send('quick 4 90.2.2.2:7000 1 1000');
check('quick classe : pas de salon normal', (await rq.next()).startsWith('go 7.7.7.'), true);

// --- mode classe : formule Elo et paliers (meme formule que main.c, voir docs/CODE.md) ---
check('Elo : 1000 bat 1000 apres placement', eloDelta(1000, 20, [{ mmr: 1000, s: 1 }]), RANK.K / 2);
check('Elo : placement (K double)', eloDelta(1000, 0, [{ mmr: 1000, s: 1 }]), RANK.K_PLACE / 2);
check('Elo : defaite contre plus fort', eloDelta(1000, 20, [{ mmr: 1200, s: 0 }]), -8);
check('Elo : 3 joueurs (gagne 1, perd 1)', eloDelta(1000, 20, [{ mmr: 1000, s: 1 }, { mmr: 1000, s: 0 }]), 0);
check('paliers', [899, 900, 1000, 1050, 1349, 1500].map(tierName),
  ['BRONZE I', 'ARGENT III', 'ARGENT I', 'OR III', 'PLATINE I', 'MAITRE']);
check('placement affiche', rankLabel(1000, 3), 'PLACEMENT 3/10');

// --- mode classe : API HTTP du classement ---
const hex = (n) => crypto.randomBytes(n).toString('hex');
const ident = (name) => ({ id: hex(16), key: hex(16), name });
const post = async (path, body) => {
  const r = await fetch(`http://127.0.0.1:${PORT}${path}`, { method: 'POST', body: JSON.stringify(body) });
  return { status: r.status, ...(await r.json()) };
};
const rep = (who, match, host, winner, players, left = []) =>
  post('/ranked/report', { id: who.id, key: who.key, match, host: host.id, winner: winner ? winner.id : '',
    players: players.map(([p, kills]) => ({ id: p.id, kills })), left: left.map((p) => p.id) });

const A = ident('Alice'), B = ident('Bob');
let r = await post('/ranked/hello', A);
check('hello : enregistrement', [r.ok, r.mmr, r.played, r.tier], [true, 1000, 0, 'PLACEMENT 0/10']);
r = await post('/ranked/hello', { ...A, key: hex(16) });
check('hello : mauvaise cle refusee', [r.status, r.ok], [403, false]);
r = await post('/ranked/hello', { ...A, name: 'Alice2' });
check('hello : pseudo change, cle gardee', r.ok, true);
await post('/ranked/hello', B);

const m1 = hex(8);
r = await rep(A, m1, A, A, [[A, 15], [B, 9]]);
check('1 contre 1 : un seul rapport -> en attente', r.state, 'pending');
r = await rep(B, m1, A, A, [[A, 15], [B, 9]]);
check('1 contre 1 : deux rapports concordants -> applique', [r.state, r.delta, r.mmr, r.played], ['applied', -32, 968, 1]);
r = await rep(A, m1, A, A, [[A, 15], [B, 9]]);
check('rapport apres coup : resultat deja calcule', [r.state, r.delta, r.mmr, r.wins], ['applied', 32, 1032, 1]);

const C = ident('Cid'), D = ident('Dan');
await post('/ranked/hello', C); await post('/ranked/hello', D);
const m2 = hex(8);
await rep(C, m2, C, C, [[C, 15], [D, 3]]);
r = await rep(D, m2, C, D, [[C, 3], [D, 15]]);
check('desaccord : rien n\'est applique', r.state, 'pending');
r = await post('/ranked/hello', D);
check('desaccord : cote inchangee', [r.mmr, r.played], [1000, 0]);

// abandon : G quitte un match a 3 ; l'hote E le liste parti, F confirme -> defaite de G
const E = ident('Eve'), F = ident('Fay'), G = ident('Gus');
for (const p of [E, F, G]) await post('/ranked/hello', p);
const m3 = hex(8);
r = await rep(E, m3, E, E, [[E, 15], [F, 10]], [G]);
check('abandon : hote seul sur 2 presents -> en attente', r.state, 'pending');
r = await rep(F, m3, E, E, [[E, 15], [F, 10]], [G]);
check('abandon : majorite des presents -> applique', r.state, 'applied');
r = await post('/ranked/hello', G);
check('abandon : defaite du parti', [r.mmr < 1000, r.played], [true, 1]);

// l'hote part : un client seul ne peut pas le declarer parti (1 contre 1)
const H = ident('Hal'), I = ident('Ivy');
await post('/ranked/hello', H); await post('/ranked/hello', I);
const m4 = hex(8);
r = await rep(I, m4, H, null, [[I, 4]], [H]);
check('hote parti, client seul : refuse', r.state, 'pending');
r = await rep(H, m4, H, null, [[I, 4]], [H]);
check('rapport d\'un joueur absent : invalide', r.status, 400);

r = await post('/ranked/report', { id: A.id, key: A.key, match: 'xyz', host: A.id, players: [], left: [] });
check('rapport mal forme', r.status, 400);
r = await rep(ident('Zoe'), hex(8), A, A, [[A, 1], [B, 0]]);
check('rapport d\'une identite inconnue', r.status, 404);

// expiration : un rapport 11 minutes apres le premier ne compte plus
const J = ident('Jo'), K = ident('Kim');
await post('/ranked/hello', J); await post('/ranked/hello', K);
const m5 = hex(8), body5 = (who) => ({ id: who.id, key: who.key, match: m5, host: J.id, winner: J.id,
  players: [{ id: J.id, kills: 15 }, { id: K.id, kills: 2 }], left: [] });
await handleRanked(sql, 'POST', '/ranked/report', new URLSearchParams(), body5(J), Date.now());
const [, late] = await handleRanked(sql, 'POST', '/ranked/report', new URLSearchParams(), body5(K), Date.now() + 11 * 60000);
check('match expire', late.state, 'expired');

// classement : seuls les joueurs sortis du placement ; rang de celui qui demande
for (let n = 0; n < 9; n++) {
  const m = hex(8);
  await rep(A, m, A, A, [[A, 15], [B, 5]]);
  await rep(B, m, A, A, [[A, 15], [B, 5]]);
}
const t = await (await fetch(`http://127.0.0.1:${PORT}/ranked/top?id=${B.id}`)).json();
check('top : les deux joueurs places, du meilleur au moins bon', t.top.map((p) => [p.name, p.played]), [['Alice2', 10], ['Bob', 10]]);
check('top : palier et rang du demandeur', [typeof t.top[0].tier, t.me.rank, t.me.played], ['string', 2, 10]);
const t2 = await (await fetch(`http://127.0.0.1:${PORT}/ranked/top?id=${C.id}`)).json();
check('top : joueur en placement sans rang', t2.me.rank, 0);

// limite de debit
let limitedHit = false;
for (let n = 0; n < RANK.RATE_PER_MIN + 2 && !limitedHit; n++) limitedHit = (await post('/ranked/hello', K)).status === 429;
check('limite de debit par identite', limitedHit, true);

console.log(fails ? `${fails} echec(s)` : 'tous les tests passent');
server.close();
process.exit(fails ? 1 : 0);
