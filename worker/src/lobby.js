// Logique du lobby, sans aucune dependance : utilisee telle quelle par le Durable Object
// (Cloudflare) et par dev-server.js (tests en local).
//
// Une "connexion" est un objet { send(texte), meta } ou meta est un objet simple que l'on peut
// remplacer (le Durable Object le garde dans l'attachement du WebSocket, qui survit a l'hibernation).
// meta = { id, sub: ver|0, room: { ver, players, max, spec, ep, lan, name, ranked, mmr }|null }
// Mode classe : un salon classe (ranked = 1) porte la cote moyenne de ses joueurs (mmr) ; la partie rapide
// classee ne choisit que des salons classes, le plus proche en cote ; la partie rapide normale les evite.

export const MAX_LINE = 200;

const EP_RE = /^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}:\d{1,5}$/;
const NAME_RE = /^[\w.\-]{1,15}$/;

export function newMeta(id) {
  return { id, sub: 0, room: null };
}

function ipOf(ep) {
  return ep.slice(0, ep.lastIndexOf(':'));
}

function roomLine(c) {
  const r = c.meta.room;
  return `room ${c.meta.id} ${r.players} ${r.max} ${r.spec} ${r.ep} ${r.name} ${r.ranked ? 1 : 0} ${r.mmr || 0}`;
}

function sendAll(conns, pred, line) {
  for (const c of conns) if (pred(c.meta)) c.send(line);
}

// Traite une ligne recue de `c`. `conns` = toutes les connexions ouvertes (c comprise).
export function handle(conns, c, raw) {
  if (typeof raw !== 'string' || raw.length > MAX_LINE) return;
  const w = raw.trim().split(/\s+/);
  const m = c.meta;
  switch (w[0]) {
    case 'ping':
      c.send('pong');
      break;

    case 'list': {
      m.sub = parseInt(w[1], 10) || 0;
      c.meta = m;
      for (const o of conns) {
        if (o.meta.room && o.meta.room.ver === m.sub && o !== c) c.send(roomLine(o));
      }
      c.send('end');
      break;
    }

    case 'host': {
      // host <ver> <joueurs> <max> <spec> <ep_public> <ep_lan> <nom> [<classe 0/1> <cote>]
      const ver = parseInt(w[1], 10), players = parseInt(w[2], 10), max = parseInt(w[3], 10);
      const spec = parseInt(w[4], 10), ep = w[5], lan = w[6], name = w[7];
      if (![ver, players, max, spec].every(Number.isInteger) || players < 0 || players > 32 || max < 1 || max > 32 ||
          spec < 0 || spec > 32 || !EP_RE.test(ep || '') || !EP_RE.test(lan || '') || !NAME_RE.test(name || '')) {
        c.send('err host');
        return;
      }
      const ranked = w[8] === '1' ? 1 : 0;
      const mmr = Math.min(9999, Math.max(0, parseInt(w[9], 10) || 0));
      const isNew = !m.room;
      m.room = { ver, players, max, spec, ep, lan, name, ranked, mmr };
      c.meta = m;
      sendAll(conns, (x) => x.sub === ver && x.id !== m.id, roomLine(c));
      if (isNew) c.send('ok');
      break;
    }

    case 'join':
    case 'quick': {
      // join <id> <mon_ep> <spec>      quick <ver> <mon_ep> [<classe 0/1> <cote>]
      const quick = w[0] === 'quick';
      const myEp = w[2];
      if (!EP_RE.test(myEp || '')) { c.send('err ep'); return; }
      let host = null;
      let spec = 0;
      if (quick) {
        const ver = parseInt(w[1], 10);
        const ranked = w[3] === '1' ? 1 : 0, mmr = parseInt(w[4], 10) || 0;
        const gap = (o) => Math.abs(o.meta.room.mmr - mmr);
        for (const o of conns) {
          const r = o.meta.room;
          if (!r || r.ver !== ver || o === c || r.players >= r.max || (r.ranked ? 1 : 0) !== ranked) continue;
          if (!host) host = o;
          else if (ranked) {                // classee : la cote la plus proche, puis le salon le plus rempli
            if (gap(o) < gap(host) || (gap(o) === gap(host) && r.players > host.meta.room.players)) host = o;
          } else if (r.players > host.meta.room.players) host = o;
        }
      } else {
        spec = w[3] === '1' ? 1 : 0;
        host = conns.find((o) => o.meta.id === w[1] && o.meta.room) || null;
        if (host && !spec && host.meta.room.players >= host.meta.room.max) { c.send('err full'); return; }
      }
      if (!host) { c.send(quick ? 'none' : 'err unknown'); return; }
      const r = host.meta.room;
      const sameNat = ipOf(r.ep) === ipOf(myEp);
      // meme box : on vise l'adresse LAN de l'hote, le perçage serait inutile (hairpin NAT)
      c.send(`go ${sameNat ? r.lan : r.ep} ${spec}`);
      host.send(`punch ${myEp}`);
      break;
    }

    default:
      c.send('err cmd');
  }
}

// Une connexion se ferme : son salon disparait.
export function closed(conns, c) {
  if (c.meta.room) sendAll(conns, (x) => x.sub === c.meta.room.ver && x.id !== c.meta.id, `gone ${c.meta.id}`);
}
