// Lobby en local pour les tests : meme logique que le Durable Object, WebSocket ecrit a la main
// (RFC 6455, trames texte uniquement) pour ne dependre d'aucun paquet npm.
//   node dev-server.js [port]      (8787 par defaut)

import http from 'node:http';
import crypto from 'node:crypto';
import { pathToFileURL } from 'node:url';
import { handle, closed, newMeta } from './src/lobby.js';

const GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

function frame(text) {
  const p = Buffer.from(text);
  const n = p.length;
  const h = n < 126 ? Buffer.from([0x81, n]) : Buffer.from([0x81, 126, n >> 8, n & 255]);
  return Buffer.concat([h, p]);
}

export function startDevServer(port = 8787) {
  const conns = [];
  const server = http.createServer((req, res) => res.end('VELOCITY lobby (dev)'));

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

  return new Promise((resolve) => server.listen(port, '127.0.0.1', () => resolve({ server, conns })));
}

if (import.meta.url === pathToFileURL(process.argv[1]).href) {
  const port = parseInt(process.argv[2], 10) || 8787;
  await startDevServer(port);
  console.log(`lobby de test sur ws://127.0.0.1:${port}/`);
}
