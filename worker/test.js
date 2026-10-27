// Tests du lobby : node test.js
// Lance un lobby local et joue des scenarios avec le client WebSocket integre a Node.

import { startDevServer } from './dev-server.js';

const PORT = 8799;
const { server } = await startDevServer(PORT);
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
check('push vers abonne', pushed.replace(/ [0-9a-f]{6} /, ' ID '), 'room ID 1 8 0 82.1.2.3:27960 Hote');
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

console.log(fails ? `${fails} echec(s)` : 'tous les tests passent');
server.close();
process.exit(fails ? 1 : 0);
