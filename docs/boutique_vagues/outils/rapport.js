// Ecrit ../CHANGEMENTS_v2.md : la liste de tout ce qui a change entre la v1 (recue) et la v2.
// Usage : node rapport.js   (apres node migrer_v2.js)
const fs = require('fs');
const path = require('path');
const { rapport } = require('./chemins');

const V1 = JSON.parse(fs.readFileSync(path.join(__dirname, 'donnees', 'arbre_v1.json'), 'utf8'));
const D = JSON.parse(fs.readFileSync(path.join(__dirname, 'donnees', 'arbre_v2.json'), 'utf8'));
const RAR = ['Commune', 'Peu commune', 'Rare', 'Épique'];
const TYPES = { D: 'distance', M: 'mêlée', C: 'compétence' };
const idV1 = g => (g === 'SHO' ? 'EVE' : g);

const L = [];
L.push('# Arbre de la boutique Vagues : changements de la v2', '');
L.push('Fichier généré par `outils/rapport.js` (ne pas modifier à la main). Les fusions validées par le propriétaire ne sont pas touchées.', '');

// ---- étapes : armes à distance et mêlée (pools réécrits autour des axes)
L.push('## Pools des armes à distance et de mêlée', '');
L.push('Réécrits autour des axes du propriétaire : une étape = une stat. Distance : taille du projectile, vitesse, zone d’effet, portée, munitions, nombre de projectiles, dégâts, suivi (+ le soin de la profondeur 3). Mêlée : une étape par axe et par profondeur parmi cooldown, vitesse d’animation, longueur de combo, munition rendue, lifesteal. La liste complète est dans l’Excel (feuille « Pool d’étapes », colonnes Axe et Nouveauté) et dans le HTML (✦).', '');
L.push('| Graine | Type | Nouvelles | Modifiées | Gardées | Retirées de la v1 |', '|---|---|---|---|---|---|');
for (const g of Object.keys(D.seeds)) {
  const s = D.seeds[g];
  if (s.t === 'C') continue;
  const nouveaux = new Set(s.pool.flat().map(x => x.n));
  const anciens = V1.seeds[idV1(g)].pool.flat().map(x => x.n);
  const retirees = [...new Set(anciens)].filter(n => !nouveaux.has(n));
  const nv = s.pool.flat().filter(x => x.nv).length, av = s.pool.flat().filter(x => x.av).length;
  L.push('| ' + s.n + ' | ' + TYPES[s.t] + ' | ' + nv + ' | ' + av + ' | ' + (20 - nv - av) + ' | ' + (retirees.join(', ') || '—') + ' |');
}
L.push('');

// ---- étapes de compétence remplacées une par une
const etapes = [];
for (const g of Object.keys(D.seeds)) D.seeds[g].pool.forEach((lvl, d) => lvl.forEach((st, i) => { if (st.was) etapes.push([g, d, i, st]); }));
L.push('## Étapes de compétence remplacées (' + etapes.length + ')', '');
L.push('Gel, Brûlure, Contagion et les variantes de feu partent de tous les pools ; Pièce et Ricochet suivent le patch d’équilibrage.', '');
L.push('| Graine | Prof. | Rareté | Avant | Après | Effet |', '|---|---|---|---|---|---|');
for (const [g, d, i, st] of etapes) L.push('| ' + D.seeds[g].n + ' | ' + (d + 1) + ' | ' + RAR[d] + ' | ' + st.was + ' | **' + st.n + '** | ' + st.e + ' |');
L.push('');

// ---- fusions retravaillées
const fus = Object.entries(D.fu).filter(([, f]) => f.rev);
L.push('## Fusions retravaillées (' + fus.length + ')', '');
L.push('| Paire | Fusion | Effet |', '|---|---|---|');
for (const [k, f] of fus) {
  const [a, b] = k.split('-');
  const v1 = V1.fu[k.replace('SHO', 'EVE')] || V1.fu[k.split('-').reverse().join('-').replace('SHO', 'EVE')];
  const ancien = v1 && v1.n !== f.n ? ' (avant : ' + v1.n + ')' : '';
  L.push('| ' + D.seeds[a].n + ' × ' + D.seeds[b].n + ' | **' + f.n + '**' + ancien + (f.ok ? ' ✓ validée' : '') + ' | ' + f.e + ' |');
}
L.push('');

// ---- renommages simples
const renom = [];
for (const [k, f] of Object.entries(D.fu)) {
  const v1 = V1.fu[k.replace('SHO', 'EVE')];
  if (v1 && v1.n !== f.n && !f.rev) renom.push([k, v1.n, f.n]);
}
L.push('## Fusions renommées (doublons de nom)', '');
for (const [k, avant, apres] of renom) L.push('- ' + k.split('-').map(x => D.seeds[x].n).join(' × ') + ' : ' + avant + ' → **' + apres + '**');
L.push('');

// ---- formes fusionnées
const formes = Object.entries(D.fu).filter(([, f]) => f.h);
L.push('## Formes fusionnées (' + formes.length + ' paires à distance, proposition à valider)', '');
L.push(D.cadre || '', '');
L.push('| Paire | Fusion (synergie passive) | Forme fusionnée | Prend à chaque parent | Effet |', '|---|---|---|---|---|');
for (const [k, f] of formes) {
  const [a, b] = k.split('-');
  L.push('| ' + D.seeds[a].n + ' × ' + D.seeds[b].n + ' | ' + f.n + (f.ok ? ' ✓' : '') + ' | **' + f.h.n + '** | ' + f.h.b + ' | ' + f.h.e + ' |');
}
L.push('');

// ---- capstones
const caps = [];
for (const g of Object.keys(D.br)) for (const b of D.br[g]) if (b.cr) caps.push([g, b]);
L.push('## Capstones réécrits (' + caps.length + ')', '');
L.push('| Branche | Capstone | Effet |', '|---|---|---|');
for (const [g, b] of caps) L.push('| ' + D.seeds[g].n + ' → ' + D.seeds[b.p].n + ' | **' + b.c.n + '** | ' + b.c.e + ' |');
L.push('');

// ---- revue des branches (outils/trier.js)
{
  const toutes = Object.entries(D.br).flatMap(([g, l]) => l.map(b => [g, b]));
  const ok = toutes.filter(([, b]) => b.rv === 'ok'), sup = toutes.filter(([, b]) => b.rv === 'sup'), mod = toutes.filter(([, b]) => b.rv === 'mod');
  L.push('## Revue des branches', '');
  if (!toutes.some(([, b]) => b.rv || b.nt || b.cm)) L.push('Aucune branche triée pour l’instant (`trier.bat`).', '');
  else {
    L.push(ok.length + ' validée(s), ' + sup.length + ' supprimée(s), ' + mod.length + ' modifiée(s) depuis leur tri, ' + (toutes.length - ok.length - sup.length - mod.length) + ' à trier.', '');
    L.push('| Branche | Revue | Note | Commentaire |', '|---|---|---|---|');
    for (const [g, b] of toutes) if (b.rv || b.nt || b.cm) {
      L.push('| ' + D.seeds[g].n + ' → ' + D.seeds[b.p].n + ' | ' + ({ ok: '✓ validée', sup: '✗ supprimée', mod: '~ modifiée depuis le tri' }[b.rv] || 'en attente') + ' | ' + (b.nt || '') + ' | ' + (b.cm || '') + ' |');
    }
    L.push('');
  }
}

// ---- chemins
L.push('## Chemins', '');
L.push('```', rapport(D), '```', '');
L.push('En v1 : 297 paires de branches cohabitant dans un loadout avaient un chemin identique (les 4 étapes), les 1404 autres n’avaient rien en commun. En v2 : aucun chemin identique, au plus une étape commune.', '');
fs.writeFileSync(path.join(__dirname, '..', 'CHANGEMENTS_v2.md'), L.join('\n'));
console.log('écrit : CHANGEMENTS_v2.md (' + fus.length + ' fusions, ' + formes.length + ' formes fusionnées, ' + caps.length + ' capstones, ' + renom.length + ' renommages)');
