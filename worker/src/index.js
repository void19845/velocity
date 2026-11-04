// Worker + Durable Object de VELOCITY.
// - Lobby : une seule instance ("global") tient tous les salons ; ses WebSockets sont hibernables, donc
//   rien n'est facture tant que personne n'envoie de message.
// - Classements : base D1 « leaderboard » (binding LEADERBOARD), interrogee directement par le Worker :
//   mode CLASSE /ranked/hello, /ranked/report et /ranked/top (voir ratings.js), et SPEEDRUN
//   /speedrun/start, /speedrun/report et /speedrun/top (voir speedrun.js, memes identites).

import { handle, closed, newMeta } from './lobby.js';
import { schema, rankedResponse } from './ratings.js';
import { srSchema, handleSpeedrun } from './speedrun.js';

// tables creees une fois par instance du Worker (CREATE ... IF NOT EXISTS) ; nouvel essai si D1 a echoue
let ready = null;
function ensureSchema(db) {
  if (!ready) ready = schema(db).then(() => srSchema(db)).catch((e) => { ready = null; throw e; });
  return ready;
}

async function leaderboard(request, db) {
  try {
    await ensureSchema(db);
    const sr = new URL(request.url).pathname.startsWith('/speedrun/');
    return await rankedResponse(db, request, sr ? handleSpeedrun : undefined);
  } catch (e) {
    return new Response('{"ok":false,"err":"base indisponible"}', { status: 503, headers: { 'Content-Type': 'application/json' } });
  }
}

export class Lobby {
  constructor(ctx) {
    this.ctx = ctx;
  }

  // Les connexions sont reconstruites depuis les WebSockets vivants (l'etat survit a l'hibernation).
  conns() {
    return this.ctx.getWebSockets().map((ws) => {
      const c = {
        ws,
        meta: ws.deserializeAttachment() || newMeta('?'),
        send: (t) => { try { ws.send(t); } catch (e) { /* socket deja ferme */ } },
      };
      return c;
    });
  }

  fetch(request) {
    if (request.headers.get('Upgrade') !== 'websocket') return new Response('VELOCITY lobby', { status: 200 });
    const pair = new WebSocketPair();
    const id = crypto.randomUUID().slice(0, 6);
    this.ctx.acceptWebSocket(pair[1]);
    pair[1].serializeAttachment(newMeta(id));
    pair[1].send(`hello ${id}`);
    return new Response(null, { status: 101, webSocket: pair[0] });
  }

  webSocketMessage(ws, message) {
    if (typeof message !== 'string') return;
    const conns = this.conns();
    const c = conns.find((x) => x.ws === ws);
    if (!c) return;
    handle(conns, c, message);
    ws.serializeAttachment(c.meta);
  }

  webSocketClose(ws) {
    const conns = this.conns();
    const c = conns.find((x) => x.ws === ws);
    if (c) closed(conns.filter((x) => x !== c), c);
    try { ws.close(); } catch (e) { /* deja ferme */ }
  }

  webSocketError(ws) {
    this.webSocketClose(ws);
  }
}

export default {
  fetch(request, env) {
    const path = new URL(request.url).pathname;
    if (path.startsWith('/ranked/') || path.startsWith('/speedrun/')) {
      return leaderboard(request, env.LEADERBOARD);
    }
    return env.LOBBY.get(env.LOBBY.idFromName('global')).fetch(request);
  },
};
