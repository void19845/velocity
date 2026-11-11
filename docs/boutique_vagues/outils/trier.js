// Tri des branches de l'arbre : montre une branche complete (4 etapes, capstone, fusion), propose de la valider ou de la supprimer,
// avec une note (1 a 5) et un commentaire facultatifs. Chaque decision est ecrite tout de suite dans donnees/revue.json.
//
// Usage : node trier.js [options]
//   (rien)              les branches pas encore triees (ou modifiees depuis leur tri), dans l'ordre des graines
//   --graine RAIL,ROQ   seulement ces graines            --vers EPE      seulement les branches qui menent a ce partenaire
//   --type D|M|C        seulement les armes a distance / de melee / les competences
//   --tout              toutes les branches, deja triees comprises
//   --statut validee|supprimee|modifiee|nouveau   seulement ce statut
//   --liste             affiche les decisions deja prises et s'arrete
//   --revue fichier     autre fichier de revue
// A l'invite : v|s [note 1-5] [commentaire], p (passer), b (retour), c texte (commentaire seul), n 4 (note seule), q (quitter), ? (aide).
const fs = require('fs');
const path = require('path');
const readline = require('readline');

const ARBRE = path.join(__dirname, 'donnees', 'arbre_v2.json');
const args = process.argv.slice(2);
const opt = nom => { const i = args.indexOf('--' + nom); return i >= 0 ? (args[i + 1] && !args[i + 1].startsWith('--') ? args[i + 1] : true) : null; };
const REVUE = opt('revue') && opt('revue') !== true ? path.resolve(opt('revue')) : path.join(__dirname, 'donnees', 'revue.json');

if (opt('aide') || opt('help')) {
  console.log(fs.readFileSync(__filename, 'utf8').split('\n').filter(l => l.startsWith('//')).map(l => l.slice(3)).join('\n'));
  process.exit(0);
}
if (!fs.existsSync(ARBRE)) { console.error('arbre_v2.json introuvable : lance d’abord node migrer_v2.js'); process.exit(1); }
const D = JSON.parse(fs.readFileSync(ARBRE, 'utf8'));

// ---- revue (fichier) -------------------------------------------------------------------------------------------------
let rev = { version: 1, branches: {} };
try { rev = JSON.parse(fs.readFileSync(REVUE, 'utf8')); } catch (e) { if (e.code !== 'ENOENT') { console.error('revue illisible : ' + e.message); process.exit(1); } }
function sauver() {
  fs.mkdirSync(path.dirname(REVUE), { recursive: true });
  fs.writeFileSync(REVUE + '.tmp', JSON.stringify(rev, null, 1));
  fs.renameSync(REVUE + '.tmp', REVUE);
}

// ---- couleurs (seulement dans un terminal) -----------------------------------------------------------------------------
const tty = process.stdout.isTTY && !process.env.NO_COLOR;
const c = (code, s) => (tty ? '\x1b[' + code + 'm' + s + '\x1b[0m' : s);
const vert = s => c(32, s), rouge = s => c(31, s), gris = s => c(90, s), gras = s => c(1, s), jaune = s => c(33, s);

// ---- branches ------------------------------------------------------------------------------------------------------------
const RAR = ['Commune', 'Peu commune', 'Rare', 'Épique'];
const nomsDe = (g, b) => b.s.map((i, d) => D.seeds[g].pool[d][i].n);
function etat(g, b) {
  const r = rev.branches[g + '>' + b.p];
  if (!r || !r.statut) return 'nouveau';
  if (r.traite) return 'modifiee';                         // retravaillee d'apres ton commentaire : a revalider
  return JSON.stringify(nomsDe(g, b)) === JSON.stringify(r.etapes) && b.c.n === r.capstone ? r.statut : 'modifiee';
}
const filtreG = opt('graine') && opt('graine') !== true ? opt('graine').toUpperCase().split(',') : null;
const filtreV = opt('vers') && opt('vers') !== true ? opt('vers').toUpperCase() : null;
const filtreT = opt('type') && opt('type') !== true ? opt('type').toUpperCase() : null;
const filtreS = opt('statut') && opt('statut') !== true ? opt('statut').toLowerCase() : null;
const toutes = [];
for (const g of Object.keys(D.seeds)) for (const b of D.br[g]) toutes.push([g, b]);
const selection = toutes.filter(([g, b]) =>
  (!filtreG || filtreG.includes(g)) && (!filtreV || b.p === filtreV) && (!filtreT || D.seeds[g].t === filtreT) &&
  (filtreS ? etat(g, b) === filtreS : opt('tout') ? true : ['nouveau', 'modifiee'].includes(etat(g, b))));

function resume() {
  const n = { nouveau: 0, validee: 0, supprimee: 0, modifiee: 0 };
  for (const [g, b] of toutes) n[etat(g, b)]++;
  return n.validee + ' validées, ' + n.supprimee + ' supprimées, ' + n.modifiee + ' modifiées depuis leur tri, ' + n.nouveau + ' à trier (sur ' + toutes.length + ')';
}
const etoiles = n => (n ? '★'.repeat(n) + '☆'.repeat(5 - n) : '');

if (opt('liste')) {
  console.log(resume() + '\n');
  for (const [g, b] of toutes) {
    const r = rev.branches[g + '>' + b.p];
    if (!r || (!r.statut && !r.note && !r.commentaire)) continue;
    const e = etat(g, b);
    const marque = e === 'validee' ? vert('✓ validée') : e === 'supprimee' ? rouge('✗ supprimée') : e === 'modifiee' ? jaune(r.traite ? '~ retravaillée, à revalider' : '~ modifiée depuis le tri') : gris('· en attente');
    console.log(D.seeds[g].n + ' → ' + D.seeds[b.p].n + '  ' + marque + (r.note ? '  ' + etoiles(r.note) : '') + (r.commentaire ? '  « ' + r.commentaire + ' »' : ''));
  }
  process.exit(0);
}

// ---- affichage d'une branche ---------------------------------------------------------------------------------------------
function afficher(g, b, rang, total) {
  const S = D.seeds[g], P = D.seeds[b.p];
  const r = rev.branches[g + '>' + b.p];
  const e = etat(g, b);
  const cle = D.fu[g + '-' + b.p] ? g + '-' + b.p : b.p + '-' + g;
  const f = D.fu[cle];
  const inverse = D.br[b.p].find(q => q.p === g);
  const l = [];
  l.push('');
  l.push(gras('='.repeat(78)));
  l.push(gras(' [' + rang + '/' + total + ']  ' + S.n + ' → ' + P.n) + gris('   (' + S.a + ')'));
  if (b.w) l.push(gris(' chemin : ' + b.w));
  if (e !== 'nouveau') {
    const avant = r && r.statut === 'validee' ? 'validée' : 'supprimée';
    const m = e === 'validee' ? vert('déjà validée') : e === 'supprimee' ? rouge('déjà supprimée')
      : jaune((r && r.traite ? 'retravaillée d’après ton commentaire (' : 'triée mais modifiée depuis (') + avant + ' avant)');
    l.push(' ' + m + (r && r.note ? '  ' + etoiles(r.note) : '') + (r && r.commentaire ? '  « ' + r.commentaire + ' »' : ''));
    if (r && r.reponse) l.push(jaune(' Réponse : ') + r.reponse);
  } else if (r && (r.note || r.commentaire)) {
    l.push(gris(' en attente') + (r.note ? '  ' + etoiles(r.note) : '') + (r.commentaire ? '  « ' + r.commentaire + ' »' : ''));
  }
  l.push('-'.repeat(78));
  b.s.forEach((idx, d) => {
    const st = S.pool[d][idx];
    const axe = st.x && D.axes ? D.axes[st.x] : '';
    const nouveau = st.was || st.nv || st.av ? jaune(' ✦') : '';
    l.push('  ' + (d + 1) + '  ' + gras(st.n.padEnd(20)) + gris((RAR[d] + ' · ' + axe).padEnd(38)) + st.e + nouveau);
  });
  l.push('  ★  ' + gras(b.c.n.padEnd(20)) + gris('Capstone'.padEnd(38)) + b.c.e + (b.cr ? jaune(' ✦') : ''));
  l.push('-'.repeat(78));
  l.push(' Fusion : ' + gras(f.n) + (f.ok ? vert('  (validée)') : f.rev ? jaune('  (revue v2)') : gris('  (texte v1)')));
  l.push('   ' + f.e);
  if (f.h) l.push(' Forme fusionnée : ' + gras(f.h.n) + (f.h.ok ? vert(' (validée)') : '') + ' — ' + f.h.e);
  if (inverse) {
    const ri = rev.branches[b.p + '>' + g];
    l.push(gris(' Branche inverse ' + P.n + ' → ' + S.n + ' : ' + inverse.c.n + ' — ' + inverse.c.e +
      (ri && ri.statut ? ' [' + (ri.statut === 'validee' ? 'validée' : 'supprimée') + ']' : '')));
  }
  l.push(gras('='.repeat(78)));
  console.log(l.join('\n'));
}

// ---- saisie ----------------------------------------------------------------------------------------------------------------
// Les lignes recues sans question en attente (saisie collee ou redirigee) sont gardees dans une file.
const rl = readline.createInterface({ input: process.stdin, output: process.stdout, terminal: !!process.stdin.isTTY });
const file = [];
let ferme = false, attente = null;
rl.on('line', l => { if (attente) { const r = attente; attente = null; r(l); } else file.push(l); });
rl.on('close', () => { ferme = true; if (attente) { const r = attente; attente = null; r(null); } });
rl.on('SIGINT', () => rl.close());                         // Ctrl+C : on sauve (c'est deja fait a chaque decision) et on s'arrete
const lire = invite => new Promise(res => {
  if (file.length) { const l = file.shift(); if (!process.stdin.isTTY) console.log(invite + l); return res(l); }
  if (ferme) return res(null);
  rl.setPrompt(invite); rl.prompt();
  attente = res;
});

const AIDE = [
  '  v [note] [commentaire]   valider la branche        ex : v 5 très bon chemin',
  '  s [note] [commentaire]   supprimer la branche      ex : s 2 trop faible, étapes banales',
  '  p ou Entrée              passer (reste à trier)    b   revenir à la branche précédente',
  '  c texte                  ajouter un commentaire seul        n 4   mettre une note seule',
  '  q                        sauver et quitter          ?   cette aide        (note : 1 à 5, facultative)',
].join('\n');

function analyser(ligne) {
  const t = ligne.trim();
  if (!t) return { cmd: 'p' };
  const m = t.match(/^(\S+)\s*([\s\S]*)$/);
  let cmd = m[1].toLowerCase();
  const reste = m[2].trim();
  if (cmd === 'valider' || cmd === 'o' || cmd === 'oui') cmd = 'v';
  if (cmd === 'supprimer' || cmd === 'x') cmd = 's';
  if (cmd === 'passer') cmd = 'p';
  if (cmd === 'retour') cmd = 'b';
  if (cmd === 'quitter') cmd = 'q';
  if (cmd === 'v' || cmd === 's') {
    const mn = reste.match(/^(\d+)(?:\s+([\s\S]*))?$/);
    if (mn) {
      const note = +mn[1];
      if (note < 1 || note > 5) return { cmd: '?', erreur: 'la note va de 1 à 5' };
      return { cmd, note, commentaire: (mn[2] || '').trim() };
    }
    return { cmd, commentaire: reste };
  }
  if (cmd === 'n') {
    const note = +reste;
    if (!(note >= 1 && note <= 5)) return { cmd: '?', erreur: 'la note va de 1 à 5' };
    return { cmd, note };
  }
  if (cmd === 'c') return reste ? { cmd, commentaire: reste } : { cmd: '?', erreur: 'écris le commentaire après c' };
  if (['p', 'b', 'q', '?'].includes(cmd)) return { cmd };
  return { cmd: '?', erreur: 'commande inconnue : ' + cmd };
}

(async () => {
  console.log('Revue : ' + resume());
  if (!selection.length) { console.log('Rien à trier avec ces filtres. (--tout pour tout revoir, --liste pour les décisions)'); rl.close(); return; }
  console.log(selection.length + ' branche(s) à parcourir. ? pour l’aide.');
  let i = 0, faites = 0;
  while (i < selection.length) {
    const [g, b] = selection[i];
    const cle = g + '>' + b.p;
    afficher(g, b, i + 1, selection.length);
    const ligne = await lire(' v/s [note] [commentaire], p, b, q, ? > ');
    if (ligne === null) { console.log('\n(fin de la saisie)'); break; }
    const a = analyser(ligne);
    if (a.cmd === '?') { console.log(a.erreur ? ' ' + rouge(a.erreur) : '\n' + AIDE + '\n'); if (a.erreur) console.log(AIDE); continue; }
    if (a.cmd === 'q') break;
    if (a.cmd === 'p') { i++; continue; }
    if (a.cmd === 'b') { i = Math.max(0, i - 1); continue; }
    const r = rev.branches[cle] || {};
    if (a.cmd === 'n') { r.note = a.note; rev.branches[cle] = r; sauver(); console.log(' note enregistrée ' + etoiles(a.note)); continue; }
    if (a.cmd === 'c') { r.commentaire = a.commentaire; rev.branches[cle] = r; sauver(); console.log(' commentaire enregistré'); continue; }
    // v ou s
    r.statut = a.cmd === 'v' ? 'validee' : 'supprimee';
    if (a.note) r.note = a.note;
    if (a.commentaire) r.commentaire = a.commentaire;
    r.date = new Date().toISOString().slice(0, 10);
    r.etapes = nomsDe(g, b);                  // photo de la branche au moment du tri (sert a figer un chemin valide)
    r.capstone = b.c.n;
    delete r.traite; delete r.reponse; delete r.chemin;   // nouvelle decision : l'ancienne reponse ne vaut plus
    rev.branches[cle] = r;
    sauver();
    console.log(' ' + (a.cmd === 'v' ? vert('✓ validée') : rouge('✗ supprimée')) + (r.note ? '  ' + etoiles(r.note) : '') + (r.commentaire ? '  « ' + r.commentaire + ' »' : ''));
    faites++; i++;
  }
  console.log('\n' + faites + ' décision(s) enregistrée(s) dans ' + path.relative(process.cwd(), REVUE) + '.');
  console.log('Revue : ' + resume());
  if (faites) console.log('Pour les voir dans le HTML et l’Excel : node migrer_v2.js && node generer.js');
  rl.close();
})();
