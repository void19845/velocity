// Migration de l'arbre v1 (recu du proprietaire) vers la v2.
// Usage : node migrer_v2.js            -> ecrit donnees/arbre_v2.json et affiche le rapport
//         node migrer_v2.js --chemins  -> affiche en plus tous les chemins attribues
const fs = require('fs');
const path = require('path');
const { attribuer, rapport } = require('./chemins');
const REMPLACEMENTS = require('./etapes_v2');
const POOLS = require('./pools_v3');
const { AXES, AXES_DISTANCE, AXES_MELEE, SANS_OBJET } = require('./axes');
let FUSIONS = {};
try { FUSIONS = require('./fusions_v2'); } catch (e) { if (e.code !== 'MODULE_NOT_FOUND') throw e; }

const D = JSON.parse(fs.readFileSync(path.join(__dirname, 'donnees', 'arbre_v1.json'), 'utf8'));

// ---- 1. Eventail -> Shotgun (patch d'equilibrage demande par le proprietaire) --------------------------------------
function renommerTexte(s) {
  if (s === 'Éventail large') return 'Salve large';       // etape de l'All in One : l'eventail est ici la forme de la salve, pas l'arme
  return s
    .replace(/de l[’']Éventail/g, 'du Shotgun').replace(/à l[’']Éventail/g, 'au Shotgun')
    .replace(/L[’']Éventail/g, 'Le Shotgun').replace(/l[’']Éventail/g, 'le Shotgun')
    .replace(/d[’']Éventail/g, 'de Shotgun').replace(/Éventail/g, 'Shotgun');
}
function renommerId(id) { return id === 'EVE' ? 'SHO' : id; }
function renommerCle(k) { return k.split('-').map(renommerId).join('-'); }

const seeds = {};
for (const [k, s] of Object.entries(D.seeds)) {
  const copie = JSON.parse(JSON.stringify(s));
  copie.n = renommerTexte(copie.n);
  copie.pool.forEach(lvl => lvl.forEach(st => { st.n = renommerTexte(st.n); st.e = renommerTexte(st.e); }));
  seeds[renommerId(k)] = copie;
}
const br = {};
for (const [k, list] of Object.entries(D.br)) {
  br[renommerId(k)] = list.map(b => ({
    p: renommerId(b.p), s: b.s.slice(), c: { n: renommerTexte(b.c.n), e: renommerTexte(b.c.e) }, f: renommerTexte(b.f),
  }));
}
const fu = {};
for (const [k, f] of Object.entries(D.fu)) {
  const g = { n: renommerTexte(f.n), e: renommerTexte(f.e) };
  if (f.ok) g.ok = 1;
  if (f.np) g.np = renommerTexte(f.np);
  fu[renommerCle(k)] = g;
}
seeds.YOY.a = 'Balayage de zone';                         // le Yoyo n'a plus de traction (decision du proprietaire) : il ne controle plus la position
const N = { axes: AXES, seeds, br, fu };
const V1S = JSON.parse(JSON.stringify(seeds));             // pools de la v1 (apres renommage), pour reperer le nouveau

// ---- 2. Etapes ------------------------------------------------------------------------------------------------------
// 2a. competences : quelques etapes remplacees une par une (etapes_v2.js)
for (const [g, parProf] of Object.entries(REMPLACEMENTS)) {
  for (const [prof, parIdx] of Object.entries(parProf)) {
    for (const [idx, [nom, effet]] of Object.entries(parIdx)) {
      const st = N.seeds[g].pool[prof - 1][idx];
      if (nom.length > 20) throw new Error('nom trop long : ' + g + ' ' + nom);
      st.was = st.n; st.n = nom; st.e = effet;
    }
  }
}
// 2b. armes a distance et de melee : pools entiers ecrits autour des axes (pools_v3.js)
for (const [g, pool] of Object.entries(POOLS)) {
  if (!N.seeds[g]) throw new Error('graine inconnue : ' + g);
  const anciens = V1S[g].pool.flat();
  N.seeds[g].pool = pool.map(lvl => lvl.map(([nom, effet, axe]) => {
    if (nom.length > 20) throw new Error('nom trop long : ' + g + ' ' + nom);
    const st = { n: nom, e: effet, x: axe };
    const ancien = anciens.find(o => o.n === nom);
    if (!ancien) st.nv = 1;                                // etape nouvelle
    else if (ancien.e !== effet) st.av = ancien.e;         // meme nom, effet change
    return st;
  }));
}
const estNouvelle = st => !!(st.was || st.nv || st.av);
const nRempl = Object.values(N.seeds).reduce((a, s) => a + s.pool.reduce((b, l) => b + l.filter(estNouvelle).length, 0), 0);

// ---- 3. Revue du proprietaire (outils/trier.js -> donnees/revue.json) et chemins -------------------------------------
let REVUE = { branches: {} };
try { REVUE = JSON.parse(fs.readFileSync(path.join(__dirname, 'donnees', 'revue.json'), 'utf8')); } catch (e) { if (e.code !== 'ENOENT') throw e; }
const indicesDe = (g, noms) => {                           // noms d'etapes -> indices du pool (null si une etape n'existe plus)
  const idx = noms.map((n, d) => N.seeds[g].pool[d].findIndex(st => st.n === n));
  return idx.length === 4 && idx.every(i => i >= 0) ? idx : null;
};
const fixes = {}, refuses = {};
let nFiges = 0, nEcartes = 0, nPerdus = 0;
for (const [cle, r] of Object.entries(REVUE.branches)) {
  if (!r.statut) continue;
  const [g, p] = cle.split('>');
  if (!N.br[g] || !N.br[g].some(q => q.p === p)) continue;
  const idx = indicesDe(g, r.etapes || []);
  if (!idx) { nPerdus++; continue; }                       // une etape de la branche triee a disparu : le chemin est recalcule
  // chemin : "ok" = a garder (meme si la branche est a retravailler), "refaire" = ecarte ; sans precision : valide = garde, supprime = ecarte
  const garde = r.chemin === 'ok' || (!r.chemin && r.statut === 'validee');
  const ecarte = r.chemin === 'refaire' || (!r.chemin && r.statut === 'supprimee');
  if (garde) { fixes[cle] = idx; nFiges++; } else if (ecarte) { (refuses[cle] = refuses[cle] || []).push(idx); nEcartes++; }
}
// chemins fixes a la main (outils/chemins_manuels.js, par noms d'etapes ou par indices) : prioritaires sur la revue
const MANUELS = require('./chemins_manuels');
const sans = {}, avec = {};
const trouver = (g, nom) => {                              // nom d'etape -> [profondeur, indice]
  for (let d = 0; d < 4; d++) { const i = N.seeds[g].pool[d].findIndex(st => st.n === nom); if (i >= 0) return [d, i]; }
  return null;
};
for (const [cle, s] of Object.entries(MANUELS)) {
  const [g, p] = cle.split('>');
  if (!N.br[g] || !N.br[g].some(q => q.p === p)) throw new Error('chemin manuel inconnu : ' + cle);
  if (Array.isArray(s)) {                                  // chemin complet
    const idx = typeof s[0] === 'string' ? indicesDe(g, s) : s.slice();
    if (!idx) throw new Error('chemin manuel : etape inconnue dans ' + cle + ' (' + s.join(', ') + ')');
    fixes[cle] = idx; delete refuses[cle];
  } else {                                                 // contraintes : sans = etapes interdites, avec = etapes imposees
    for (const nom of s.sans || []) { const t = trouver(g, nom); if (!t) throw new Error('chemin manuel : etape inconnue ' + nom + ' dans ' + cle); (sans[cle] = sans[cle] || []).push(t); }
    for (const nom of s.avec || []) { const t = trouver(g, nom); if (!t) throw new Error('chemin manuel : etape inconnue ' + nom + ' dans ' + cle); (avec[cle] = avec[cle] || {})[t[0]] = t[1]; }
    delete fixes[cle];                                     // le chemin precedent ne suffit plus : l'optimiseur respecte les contraintes
  }
}
// chemins : les branches validees sont figees, les chemins supprimes ne sont pas reproposes
const chemins = attribuer(N, { graine: 1, essais: 3, iterations: 150000, fixes, refuses, sans, avec });
for (const g of Object.keys(N.br)) {
  N.br[g].forEach((b, i) => {
    if (chemins[g][i].p !== b.p) throw new Error('ordre des partenaires incoherent : ' + g);
    b.s = chemins[g][i].s; b.w = chemins[g][i].w;
  });
}

// ---- 4. Capstones et fusions retravailles --------------------------------------------------------------------------
const cleFu = (a, b) => (N.fu[a + '-' + b] ? a + '-' + b : b + '-' + a);
let nFus = 0;
if (FUSIONS.fusions) {
  for (const [paire, def] of Object.entries(FUSIONS.fusions)) {
    const [a, b] = paire.split('-');
    const k = cleFu(a, b);
    const f = N.fu[k];
    if (!f) throw new Error('fusion inconnue : ' + paire);
    if (f.ok && !def.force && !def.valide) throw new Error('fusion validee, pas touchee : ' + paire);
    if (def.n) f.n = def.n;
    if (def.e) f.e = def.e;
    if (def.sansNote) delete f.np;                        // la note d'adaptation PvP -> Vagues n'a plus d'objet
    f.rev = 1; nFus++;
    if (def.force && !def.valide) delete f.ok;            // texte change a la demande du proprietaire : a revalider
    if (def.valide) f.ok = 1;                             // validee par le proprietaire
    for (const [x, y, cap] of [[a, b, def.ab], [b, a, def.ba]]) {
      if (!cap) continue;
      const br1 = N.br[x].find(q => q.p === y);
      br1.c = { n: cap[0], e: cap[1] };
      br1.cr = 1;
    }
    for (const [x, y] of [[a, b], [b, a]]) N.br[x].find(q => q.p === y).f = N.fu[k].n;
  }
}
if (FUSIONS.capstones) {                                  // capstones seuls (la fusion reste telle quelle)
  for (const [cle, cap] of Object.entries(FUSIONS.capstones)) {
    const [x, y] = cle.split('>');
    const b1 = N.br[x].find(q => q.p === y);
    if (!b1) throw new Error('branche inconnue : ' + cle);
    b1.c = { n: cap[0], e: cap[1] }; b1.cr = 1;
  }
}
if (FUSIONS.noms) {                                       // renommages de fusion (doublons)
  for (const [paire, nom] of Object.entries(FUSIONS.noms)) {
    const [a, b] = paire.split('-');
    const k = cleFu(a, b);
    N.fu[k].n = nom;                                      // simple renommage : le texte reste celui de la v1
    for (const [x, y] of [[a, b], [b, a]]) N.br[x].find(q => q.p === y).f = nom;
  }
}

// ---- 5. Controles --------------------------------------------------------------------------------------------------
// marques de revue sur les branches (apres les capstones : la revue a ete faite sur l'arbre complet)
// ok = validee, sup = supprimee, mod = triee mais modifiee depuis (a re-trier)
let nMarques = 0;
for (const [cle, r] of Object.entries(REVUE.branches)) {
  const [g, p] = cle.split('>');
  const b = N.br[g] && N.br[g].find(q => q.p === p);
  if (!b) continue;
  if (r.statut) {
    const memes = JSON.stringify(b.s.map((idx, d) => N.seeds[g].pool[d][idx].n)) === JSON.stringify(r.etapes) && b.c.n === r.capstone;
    b.rv = r.traite ? 'mod' : memes ? (r.statut === 'validee' ? 'ok' : 'sup') : 'mod';
    nMarques++;
  }
  if (r.note) b.nt = r.note;
  if (r.commentaire) b.cm = r.commentaire;
  if (r.reponse) b.rp = r.reponse;
}

// ---- Paires supprimees (decision du proprietaire) : aucune branche ni fusion entre une competence et une mêlee ------------------
// Le filtre passe en dernier : les chemins des autres branches restent exactement ceux deja attribues.
// Pour l'instant : aucun lien melee x competence (ni All in One, ni les autres). Pour les rouvrir, vider cette fonction.
const sansLien = (a, b) => [N.seeds[a].t, N.seeds[b].t].sort().join('') === 'CM';
let nBrSupprimees = 0, nFuSupprimees = 0;
for (const g of Object.keys(N.br)) { const av = N.br[g].length; N.br[g] = N.br[g].filter(b => !sansLien(g, b.p)); nBrSupprimees += av - N.br[g].length; }
for (const k of Object.keys(N.fu)) { const [a, b] = k.split('-'); if (sansLien(a, b)) { delete N.fu[k]; nFuSupprimees++; } }
console.log('paires mêlée x compétence supprimées : ' + nBrSupprimees + ' branches, ' + nFuSupprimees + ' fusions');
const erreurs = [];
// formes fusionnees des paires distance x distance (hybrides_v3.js)
const { CADRE, hybrides: HYBRIDES } = require('./hybrides_v3');
N.cadre = CADRE;
for (const [paire, h] of Object.entries(HYBRIDES)) {
  const [a, b] = paire.split('-');
  const k = cleFu(a, b);
  if (!N.fu[k]) { erreurs.push('forme fusionnée : paire inconnue ' + paire); continue; }
  if (N.seeds[a].t !== 'D' || N.seeds[b].t !== 'D') { erreurs.push('forme fusionnée réservée aux paires distance x distance : ' + paire); continue; }
  N.fu[k].h = { n: h.n, b: h.b, e: h.e };
  if (h.ok) N.fu[k].h.ok = 1;                              // forme fusionnee validee par le proprietaire
}
const DEPEND = { PIE: /pi[eè]ce/i, MAR: /marqu/i, MIR: /leurre/i, RAIL: /\brail\b/i, ROQ: /roquette/i, TES: /tesla/i, SCI: /\bscie\b/i };
for (const [g, s] of Object.entries(N.seeds)) {
  if (s.pool.length !== 4) erreurs.push(g + ' : 4 profondeurs attendues');
  const noms = new Set();
  s.pool.forEach((lvl, d) => {
    if (lvl.length !== 5) erreurs.push(g + ' d' + (d + 1) + ' : 5 etapes attendues');
    lvl.forEach(st => {
      if (/^(Gel|Brûlure|Contagion)$/.test(st.n) || (g === 'RAIL' && st.n === 'Fracture')) erreurs.push(g + ' : etape interdite ' + st.n);
      if (/brûlante?|de feu|enflamme|traînée brûlante/i.test(st.n + ' ' + st.e) && !/Bord brûlant/.test(st.n)) erreurs.push(g + ' : variante de feu ' + st.n + ' : ' + st.e);
      if (/gel\b|fige/i.test(st.e) && !/Stase|Arrêt|Immobilisation|Lasso|Marque/.test(st.n)) erreurs.push(g + ' : effet de gel ' + st.n + ' : ' + st.e);
      if (/contagion|propage/i.test(st.e)) erreurs.push(g + ' : contagion ' + st.n);
      // une etape du pool sert dans tous les loadouts : elle ne depend jamais d'une autre graine
      for (const [autre, rx] of Object.entries(DEPEND)) if (autre !== g && rx.test(st.n + ' ' + st.e)) erreurs.push(g + ' : etape qui depend de ' + autre + ' : ' + st.n + ' : ' + st.e);
      if (/compte comme touche pour|pour fracture/i.test(st.e)) erreurs.push(g + ' : etape qui depend d\'une autre etape : ' + st.n);
      if (noms.has(st.n)) erreurs.push(g + ' : nom d\'etape en double ' + st.n);
      noms.add(st.n);
      if (st.n.length > 20) erreurs.push(g + ' : nom trop long ' + st.n);
    });
  });
  N.br[g].forEach(b => {
    if (b.s.length !== 4 || b.s.some(x => x < 0 || x > 4)) erreurs.push(g + '>' + b.p + ' : chemin invalide');
    if (noms.has(b.c.n)) erreurs.push(g + '>' + b.p + ' : capstone du meme nom qu\'une etape : ' + b.c.n);
  });
  // soin en profondeur 3
  if (!/^SOIN/.test(s.pool[2][0].e)) erreurs.push(g + ' : pas de soin en d3[0]');
  // axes du proprietaire
  if (s.t === 'D') {
    const vus = new Set();
    s.pool.forEach((lvl, d) => lvl.forEach(st => {
      if (!st.x || ![...AXES_DISTANCE, 'SOI'].includes(st.x)) erreurs.push(g + ' : axe invalide ' + st.n + ' (' + st.x + ')');
      vus.add(st.x);
    }));
    for (const a of AXES_DISTANCE) if (!vus.has(a) && !(SANS_OBJET[g] || []).includes(a)) erreurs.push(g + ' : axe jamais modifie : ' + AXES[a]);
    if (s.pool[2][0].x !== 'SOI') erreurs.push(g + ' : d3[0] doit etre le soin');
  } else if (s.t === 'M') {
    s.pool.forEach((lvl, d) => {
      const axes = lvl.map(st => st.x).sort().join(',');
      if (axes !== [...AXES_MELEE].sort().join(',')) erreurs.push(g + ' d' + (d + 1) + ' : un pas de chaque axe de melee attendu, ' + axes);
    });
    if (s.pool[2][0].x !== 'VOL') erreurs.push(g + ' : d3[0] doit etre le lifesteal');
  }
}
// grammaire autour de Shotgun et reliquats de l'ancien nom
const toutesLesChaines = [];
for (const s of Object.values(N.seeds)) { toutesLesChaines.push(s.n, s.a); s.pool.forEach(l => l.forEach(st => toutesLesChaines.push(st.n, st.e))); }
for (const l of Object.values(N.br)) l.forEach(b => toutesLesChaines.push(b.c.n, b.c.e, b.f));
for (const f of Object.values(N.fu)) toutesLesChaines.push(f.n, f.e, f.np || '');
for (const t of toutesLesChaines) {
  if (/Éventail/.test(t)) erreurs.push('reliquat de l’ancien nom : ' + t);
  if (/\b(de|à|d’|l’) ?(le|l’)? ?Shotgun/.test(t) && /\bde le |\bà le |l’Shotgun|d’Shotgun/.test(t)) erreurs.push('grammaire : ' + t);
}
const nomsFu = {};
for (const [k, f] of Object.entries(N.fu)) (nomsFu[f.n] = nomsFu[f.n] || []).push(k);
for (const [n, ks] of Object.entries(nomsFu)) if (ks.length > 1) erreurs.push('nom de fusion en double : ' + n + ' ' + ks.join(', '));
const nTot = Object.keys(N.fu).length;
if (nTot !== 112) erreurs.push('112 fusions attendues (154 moins les 42 mêlée x compétence), ' + nTot);
// formes fusionnees : une par paire distance x distance, noms uniques et differents de ceux des fusions
const ddPaires = Object.entries(N.fu).filter(([k]) => k.split('-').every(x => N.seeds[x].t === 'D'));
if (ddPaires.length !== 21) erreurs.push('21 paires distance x distance attendues, ' + ddPaires.length);
const nomsH = {};
for (const [k, f] of ddPaires) {
  if (!f.h) { erreurs.push('forme fusionnée manquante : ' + k); continue; }
  if (f.h.n.length > 24) erreurs.push('nom de forme fusionnée trop long : ' + f.h.n);
  (nomsH[f.h.n] = nomsH[f.h.n] || []).push(k);
  if (nomsFu[f.h.n]) erreurs.push('forme fusionnée qui porte le nom d’une fusion : ' + f.h.n + ' (' + nomsFu[f.h.n].join(', ') + ')');
}
for (const [n, ks] of Object.entries(nomsH)) if (ks.length > 1) erreurs.push('nom de forme fusionnée en double : ' + n);

console.log('étapes nouvelles ou modifiées : ' + nRempl + ' ; fusions retravaillées : ' + nFus);
if (nMarques || nPerdus) console.log('revue : ' + nFiges + ' chemin(s) validé(s) figé(s), ' + nEcartes + ' chemin(s) supprimé(s) écarté(s)' + (nPerdus ? ', ' + nPerdus + ' perdu(s) (étape disparue)' : '') + ', ' + nMarques + ' branche(s) marquée(s)');
console.log(rapport(N));
if (erreurs.length) { console.log('\nERREURS (' + erreurs.length + ') :\n  ' + erreurs.join('\n  ')); }
else console.log('contrôles : OK');

if (process.argv.includes('--chemins')) {
  for (const g of Object.keys(N.br)) {
    console.log('\n=== ' + g + ' (' + N.seeds[g].n + ')');
    for (const b of N.br[g]) {
      console.log('  -> ' + b.p.padEnd(4) + b.s.map((idx, d) => N.seeds[g].pool[d][idx].n).join(' / ') + '   [' + b.w + ']');
    }
  }
}
fs.writeFileSync(path.join(__dirname, 'donnees', 'arbre_v2.json'), JSON.stringify(N));
process.exitCode = erreurs.length ? 1 : 0;
