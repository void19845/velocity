// Worker + Durable Objects de VELOCITY.
// - Lobby : une seule instance ("global") tient tous les salons ; ses WebSockets sont hibernables, donc
//   rien n'est facture tant que personne n'envoie de message.
// - Ratings : une seule instance ("global") tient le classement du mode CLASSE (SQLite) ; elle repond
//   aux requetes HTTPS /ranked/hello, /ranked/report et /ranked/top (voir ratings.js).

import { handle, closed, newMeta } from './lobby.js';
import { schema, rankedResponse } from './ratings.js';

export class Ratings {
  constructor(ctx) {
    this.sql = ctx.storage.sql;
    schema(this.sql);
  }

  fetch(request) {
    return rankedResponse(this.sql, request);
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
    if (new URL(request.url).pathname.startsWith('/ranked/')) {
      return env.RATINGS.get(env.RATINGS.idFromName('global')).fetch(request);
    }
    return env.LOBBY.get(env.LOBBY.idFromName('global')).fetch(request);
  },
};
