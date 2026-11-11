// Tests du lobby : node test.js
// Lance un lobby local et joue des scenarios avec le client WebSocket integre a Node.

import { startDevServer } from './dev-server.js';
import { eloDelta, tierName, rankLabel, handleRanked, RANK } from './src/ratings.js';
import { handleSpeedrun, runError, SR } from './src/speedrun.js';
import { dcName } from './src/discord.js';
import crypto from 'node:crypto';

const PORT = 8799;
const { server, db } = await startDevServer(PORT);
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

// heure du serveur : l'horloge de match des joueurs s'y cale (src/reseau/horloge.c)
const t0 = Date.now();
a.send('time');
const tm = ((await a.next()) || '').split(' ');
check('heure du serveur', tm[0] === 'time' && Number(tm[1]) >= t0 && Number(tm[1]) <= Date.now(), true);

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
await handleRanked(db, 'POST', '/ranked/report', new URLSearchParams(), body5(J), Date.now());
const [, late] = await handleRanked(db, 'POST', '/ranked/report', new URLSearchParams(), body5(K), Date.now() + 11 * 60000);
check('match expire', late.state, 'expired');

// D1 : plusieurs instances du Worker repondent en meme temps ; trois rapports simultanes d'un match a 3
// (dont deux suffisent a la majorite) n'appliquent l'Elo qu'une fois
const L = ident('Lea'), M = ident('Max'), N = ident('Noa');
for (const p of [L, M, N]) await post('/ranked/hello', p);
const m6 = hex(8), score6 = [[L, 15], [M, 8], [N, 4]];
const states6 = (await Promise.all([L, M, N].map((p) => rep(p, m6, L, L, score6)))).map((x) => x.state);
check('rapports simultanes : applique au moins une fois', states6.includes('applied'), true);
const after6 = await Promise.all([L, M, N].map((p) => rep(p, m6, L, L, score6)));
check('rapports simultanes : un seul match compte', after6.map((x) => [x.state, x.played]), [['applied', 1], ['applied', 1], ['applied', 1]]);
r = await post('/ranked/hello', L);
check('rapports simultanes : cote du gagnant appliquee une fois', [r.mmr, r.played, r.wins], [1000 + after6[0].delta, 1, 1]);
// deux matchs du meme joueur appliques en meme temps : aucune variation perdue
const O = ident('Oli'), P2 = ident('Pia'), Q = ident('Quy');
for (const p of [O, P2, Q]) await post('/ranked/hello', p);
const m7 = hex(8), m8 = hex(8);
await rep(O, m7, O, O, [[O, 15], [P2, 3]]);
await rep(O, m8, O, O, [[O, 15], [Q, 3]]);
const both = await Promise.all([rep(P2, m7, O, O, [[O, 15], [P2, 3]]), rep(Q, m8, O, O, [[O, 15], [Q, 3]])]);
r = await post('/ranked/hello', O);
check('deux matchs simultanes du meme joueur', [both.map((x) => x.state), r.played, r.wins, r.mmr],
  [['applied', 'applied'], 2, 2, 1000 - both[0].delta - both[1].delta]);

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

// --- speedrun : classements des meilleurs temps (course par carte CHALLENGE, speedrun complet, vague 100) ---
const sr = (path, body, at) => handleSpeedrun(db, 'POST', path, new URLSearchParams(), body, at);
const toile = 'course:la_toile', gates = SR.CATS[toile].splits[0];
const splitsOf = (ms, n) => Array.from({ length: n }, (_, i) => Math.round(ms * (i + 1) / n));
const S1 = ident('Sam'), S2 = ident('Tia'), S3 = ident('Uma');
const now = Date.now();
let [st, sb] = await sr('/speedrun/start', { ...S1, cat: toile }, now);
check('speedrun : ticket (identite creee au premier usage)', [st, sb.ok, /^[0-9a-f]{16}$/.test(sb.run)], [200, true, true]);
const run1 = sb.run;
[st, sb] = await sr('/speedrun/report', { ...S1, cat: toile, run: run1, ms: 40000, splits: splitsOf(40000, gates) }, now + 30000);
check('speedrun : plus rapide que le temps reel ecoule depuis le ticket', [st, sb.err], [400, 'plus rapide que le temps reel']);
[st, sb] = await sr('/speedrun/report', { ...S1, cat: toile, run: run1, ms: 40000, splits: splitsOf(40000, gates) }, now + 45000);
check('speedrun : ticket consomme par le rapport refuse', sb.err, 'ticket inconnu');
[, sb] = await sr('/speedrun/start', { ...S1, cat: toile }, now);
[st, sb] = await sr('/speedrun/report', { ...S1, cat: toile, run: sb.run, ms: 40000, splits: splitsOf(40000, gates) }, now + 44000);
check('speedrun : premier temps -> record, rang 1', [st, sb.state, sb.best, sb.prev, sb.rank], [200, 'record', 40000, 0, 1]);
[, sb] = await sr('/speedrun/start', { ...S1, cat: toile }, now);
const runA = sb.run;
await sr('/speedrun/start', { ...S1, cat: toile }, now + 1000);   // R : nouveau depart, l'ancien ticket tombe
[st, sb] = await sr('/speedrun/report', { ...S1, cat: toile, run: runA, ms: 39000, splits: splitsOf(39000, gates) }, now + 60000);
check('speedrun : ticket remplace par un nouveau depart', [st, sb.err], [400, 'ticket inconnu']);
[, sb] = await sr('/speedrun/start', { ...S1, cat: toile }, now);
[, sb] = await sr('/speedrun/report', { ...S1, cat: toile, run: sb.run, ms: 45000, splits: splitsOf(45000, gates) }, now + 60000);
check('speedrun : temps moins bon -> record garde', [sb.state, sb.best, sb.prev], ['kept', 40000, 40000]);
[, sb] = await sr('/speedrun/start', { ...S2, cat: toile }, now);
[, sb] = await sr('/speedrun/report', { ...S2, cat: toile, run: sb.run, ms: 38500, splits: splitsOf(38500, gates) }, now + 60000);
check('speedrun : second joueur plus rapide -> rang 1', [sb.state, sb.rank], ['record', 1]);
[, sb] = await sr('/speedrun/start', { ...S2, cat: toile }, now);
[st, sb] = await sr('/speedrun/report', { ...S2, cat: toile, run: sb.run, ms: 5000, splits: splitsOf(5000, gates) }, now + 60000);
check('speedrun : sous le plancher', [st, sb.err], [400, 'sous le plancher']);
check('speedrun : planchers (course, complet:3 = somme des trois courses)',
  [SR.CATS[toile].floorMs, SR.CATS['course:le_stand'].floorMs, SR.CATS['course:le_flux'].floorMs, SR.CATS['complet:3'].floorMs],
  [6975, 2189, 2558, 6975 + 2189 + 2558]);
for (const cat of ['course:le_stand', 'course:le_flux']) {   // cartes a cibles : meme regles que LA TOILE
  const n = SR.CATS[cat].splits[0];
  [, sb] = await sr('/speedrun/start', { ...S3, cat }, now);
  [st, sb] = await sr('/speedrun/report', { ...S3, cat, run: sb.run, ms: 30000, splits: splitsOf(30000, n) }, now + 31000);
  check(`speedrun : ${cat} record, rang 1`, [st, sb.state, sb.rank], [200, 'record', 1]);
  [, sb] = await sr('/speedrun/start', { ...S3, cat }, now);
  [st, sb] = await sr('/speedrun/report', { ...S3, cat, run: sb.run, ms: 2000, splits: splitsOf(2000, n) }, now + 31000);
  check(`speedrun : ${cat} sous le plancher`, [st, sb.err], [400, 'sous le plancher']);
  [, sb] = await sr('/speedrun/start', { ...S3, cat }, now);
  [st, sb] = await sr('/speedrun/report', { ...S3, cat, run: sb.run, ms: 30000, splits: splitsOf(30000, n + 1) }, now + 31000);
  check(`speedrun : ${cat} mauvais nombre de temps intermediaires`, [st, sb.err], [400, 'temps intermediaires']);
}
[, sb] = await sr('/speedrun/start', { ...S3, cat: 'complet:3' }, now);
[st, sb] = await sr('/speedrun/report', { ...S3, cat: 'complet:3', run: sb.run, ms: 60000, splits: [20000, 40000, 60000] }, now + 61000);
check('speedrun : complet:3 (trois cartes) record, rang 1', [st, sb.state, sb.rank], [200, 'record', 1]);
check('speedrun : temps intermediaires', [
  runError(toile, 40000, splitsOf(40000, gates - 1)),
  runError(toile, 40000, [...splitsOf(39000, gates - 1), 40000].reverse()),
  runError(toile, 40000, splitsOf(39000, gates)),
  runError('vagues100', 1200000, []),
  runError('vagues100', 1200000, splitsOf(1200000, 4)),
  runError('vagues100', 1200000, splitsOf(1200000, 3)),
  runError('autre', 40000, []),
  runError(toile, 40000.5, splitsOf(40000, gates)),
], ['temps intermediaires', 'temps intermediaires', 'temps intermediaires', null, null, 'temps intermediaires', 'categorie inconnue',
  'temps invalide']);
[, sb] = await sr('/speedrun/start', { ...S3, cat: toile }, now);
[st, sb] = await sr('/speedrun/report', { ...S1, cat: toile, run: sb.run, ms: 39000, splits: splitsOf(39000, gates) }, now + 60000);
check('speedrun : ticket d\'un autre joueur refuse', [st, sb.err], [400, 'ticket inconnu']);
[, sb] = await sr('/speedrun/start', { ...S3, cat: 'complet' }, now);
[st, sb] = await sr('/speedrun/report', { ...S3, cat: toile, run: sb.run, ms: 39000, splits: splitsOf(39000, gates) }, now + 60000);
check('speedrun : ticket d\'une autre categorie refuse', [st, sb.err], [400, 'ticket inconnu']);
[, sb] = await sr('/speedrun/start', { ...S3, cat: toile }, now);
[st, sb] = await sr('/speedrun/report', { ...S3, cat: toile, run: sb.run, ms: 39000, splits: splitsOf(39000, gates) }, now + 31 * 60000);
check('speedrun : ticket expire (30 min pour une course)', [st, sb.err], [400, 'ticket expire']);
[st, sb] = await sr('/speedrun/start', { ...S3, key: hex(16), cat: toile }, now);
check('speedrun : mauvaise cle', [st, sb.ok], [403, false]);
[st, sb] = await sr('/speedrun/start', { ...S3, cat: 'course:inconnue' }, now);
check('speedrun : categorie inconnue', [st, sb.err], [400, 'categorie inconnue']);
// le meme ticket envoye deux fois en meme temps : un seul rapport passe
[, sb] = await sr('/speedrun/start', { ...S3, cat: toile }, now);
const twice = await Promise.all([0, 1].map(() =>
  sr('/speedrun/report', { ...S3, cat: toile, run: sb.run, ms: 42000, splits: splitsOf(42000, gates) }, now + 60000)));
check('speedrun : ticket rejoue en meme temps', twice.map(([s, x]) => [s, x.state || x.err]).sort(),
  [[200, 'record'], [400, 'ticket inconnu']]);
// vague 100 (point d'entree du mode VAGUES) et speedrun complet : memes regles, leurs propres classements
[, sb] = await sr('/speedrun/start', { ...S3, cat: 'vagues100' }, now);
[, sb] = await sr('/speedrun/report', { ...S3, cat: 'vagues100', run: sb.run, ms: 1500000, splits: splitsOf(1500000, 4) }, now + 1501000);
check('speedrun : vague 100', [sb.state, sb.rank], ['record', 1]);
[, sb] = await sr('/speedrun/start', { ...S3, cat: 'complet' }, now);
[, sb] = await sr('/speedrun/report', { ...S3, cat: 'complet', run: sb.run, ms: 41000, splits: [41000] }, now + 50000);
check('speedrun : complet', [sb.state, sb.rank], ['record', 1]);
// classement par HTTP (meme route que le Durable Object)
const top1 = await (await fetch(`http://127.0.0.1:${PORT}/speedrun/top?cat=${encodeURIComponent(toile)}&id=${S1.id}`)).json();
check('speedrun : top du plus rapide au plus lent', top1.top.map((p) => [p.name, p.ms]), [['Tia', 38500], ['Sam', 40000], ['Uma', 42000]]);
check('speedrun : record du monde et son detail', [top1.wr.name, top1.wr.splits.length, top1.wr.splits[gates - 1]], ['Tia', gates, 38500]);
check('speedrun : mon rang et mon meilleur temps', [top1.me.rank, top1.me.ms, top1.me.splits.length, top1.runners], [2, 40000, gates, 3]);
const top2 = await (await fetch(`http://127.0.0.1:${PORT}/speedrun/top?cat=complet&id=${S1.id}`)).json();
check('speedrun : classement complet a part, sans temps pour moi', [top2.top.length, top2.me], [1, null]);
r = await post('/speedrun/start', { ...ident('Vic'), cat: 'complet' });
check('speedrun : depart par HTTP', [r.status, r.ok], [200, true]);
let srLimited = false;
for (let n = 0; n < SR.RATE_PER_MIN + 2 && !srLimited; n++) srLimited = (await sr('/speedrun/start', { ...S2, cat: toile }, now))[0] === 429;
check('speedrun : limite de debit (compteur a part du mode classe)', srLimited, true);

// --- connexion Discord : un compte Discord = une identite (faux Discord du dev-server) ---
const dcGet = (path, follow = false) => fetch(`http://127.0.0.1:${PORT}${path}`, { redirect: follow ? 'follow' : 'manual' });
const dcBack = (code, state) => dcGet(`/auth/discord/callback?code=${code}&state=${state}`);
const D1 = ident('Dana'), E1 = ident('Autre'), s1 = hex(16), s2 = hex(16), s3 = hex(16), s4 = hex(16);
await post('/ranked/hello', D1);
r = await post('/auth/discord/start', { ...D1, state: s1 });
check('discord : demande enregistree', r.ok, true);
r = await post('/auth/discord/start', { ...E1, state: s1 });
check('discord : state deja pris par un autre', r.status, 409);
r = await post('/auth/discord/poll', { ...D1, state: s1 });
check('discord : en attente du navigateur', r.state, 'pending');
let hr = await dcGet(`/auth/discord/go?s=${s1}`);
const loc = new URL(hr.headers.get('location') || 'http://x/');
check('discord : redirection vers la page d\'autorisation',
  [hr.status, loc.pathname, loc.searchParams.get('state'), loc.searchParams.get('scope')], [302, '/fake-discord/authorize', s1, 'identify']);
hr = await dcBack('fake.111.dana_dc', s1);
check('discord : retour de Discord', [hr.status, (await hr.text()).includes('Connecte : dana_dc')], [200, true]);
r = await post('/auth/discord/poll', { ...D1, key: hex(16), state: s1 });
check('discord : poll avec une mauvaise cle', r.status, 403);
r = await post('/auth/discord/poll', { ...E1, state: s1 });
check('discord : poll par une autre identite', r.state, 'expired');
r = await post('/auth/discord/poll', { ...D1, state: s1 });
check('discord : premiere connexion, l\'identite est liee', [r.state, r.id, r.key, r.name, r.discord], ['done', D1.id, D1.key, 'Dana', 'dana_dc']);
r = await post('/auth/discord/poll', { ...D1, state: s1 });
check('discord : la demande ne sert qu\'une fois', r.state, 'expired');
// second appareil : meme compte Discord -> meme identite, cle a lui, l'ancienne cle marche toujours
await post('/ranked/hello', E1);
await post('/auth/discord/start', { ...E1, state: s2 });
await dcBack('fake.111.dana_dc', s2);
r = await post('/auth/discord/poll', { ...E1, state: s2 });
check('discord : second appareil, meme identite, cle neuve', [r.state, r.id, r.key !== E1.key, r.name], ['done', D1.id, true, 'Dana']);
const D2 = { id: r.id, key: r.key };
r = await post('/ranked/hello', { ...D2, name: 'DanaPC2' });
check('discord : la cle du second appareil est acceptee', r.ok, true);
check('discord : pseudo change pour ce compte Discord', (await db.prepare('SELECT name FROM players WHERE id = ?').bind(D1.id).first()).name, 'DanaPC2');
r = await post('/ranked/hello', D1);
check('discord : la cle du premier appareil reste valable', r.ok, true);
// identite deja liee, autre compte Discord : identite neuve pour ce compte
await post('/auth/discord/start', { ...D1, state: s3 });
await dcBack('fake.222.zed', s3);
r = await post('/auth/discord/poll', { ...D1, state: s3 });
check('discord : autre compte -> identite neuve', [r.state, r.id !== D1.id, r.name, r.discord], ['done', true, 'zed', 'zed']);
r = await post('/ranked/hello', { id: r.id, key: r.key });
check('discord : identite neuve utilisable', [r.ok, r.mmr, r.played], [true, RANK.START, 0]);
// tout le chemin par le navigateur : go -> (faux) Discord -> callback
const F1 = ident('Fab');
await post('/auth/discord/start', { ...F1, state: s4 });
hr = await dcGet(`/auth/discord/go?s=${s4}`, true);
check('discord : chemin complet dans le navigateur', [hr.status, (await hr.text()).includes('Connecte : testeur')], [200, true]);
r = await post('/auth/discord/poll', { ...F1, state: s4 });
check('discord : chemin complet, compte lie', [r.state, r.id, r.discord], ['done', F1.id, 'testeur']);
hr = await dcBack('fake.333.x', hex(16));
check('discord : retour pour une demande inconnue', hr.status, 400);
check('discord : nom du compte nettoye', dcName('ab c<>é_.-9'), 'abc_.-9');

console.log(fails ? `${fails} echec(s)` : 'tous les tests passent');
server.close();
process.exit(fails ? 1 : 0);
