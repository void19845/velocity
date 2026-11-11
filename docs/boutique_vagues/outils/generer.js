// Genere, a partir de donnees/arbre_v2.json :
//   ../arbre_fusions_boutique_vagues_v2.html        (visionneuse, meme interface que la v1 + reperes des changements)
//   ../fusions_boutique_vagues_v2.xlsx              (memes 5 feuilles que la v1, colonnes en plus)
// Usage : node generer.js          (apres node migrer_v2.js)
// Aucune dependance : le zip du .xlsx est ecrit a la main (zlib du noyau de Node).
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const SORTIE = path.join(__dirname, '..');
const D = JSON.parse(fs.readFileSync(path.join(__dirname, 'donnees', 'arbre_v2.json'), 'utf8'));
const IDS = Object.keys(D.seeds);
const TYPES = { D: 'Distance', M: 'Mêlée', C: 'Compétence' };
const RAR = [['Commune', 5, 120, 50], ['Peu commune', 4, 240, 30], ['Rare', 3, 400, 14], ['Épique', 2, 600, 5], ['Légendaire', 1, 850, 1]];
const NIVEAUX = ['60/80/100/120', '120/160/200', '200/300', '400', 'aucune'];

// ---- statistiques --------------------------------------------------------------------------------------------------
const nbEtapesRemplacees = IDS.reduce((a, g) => a + D.seeds[g].pool.reduce((b, l) => b + l.filter(s => s.was || s.nv || s.av).length, 0), 0);
const nouveauteEtape = st => (st.was ? 'remplace « ' + st.was + ' »' : st.nv ? 'nouvelle étape' : st.av ? 'modifiée (avant : ' + st.av + ')' : '');
const nbFormes = Object.values(D.fu).filter(f => f.h).length;
const toutesBr = IDS.flatMap(g => D.br[g]);
const nbRevOk = toutesBr.filter(b => b.rv === 'ok').length, nbRevSup = toutesBr.filter(b => b.rv === 'sup').length;
const nbFusionsRevues = Object.values(D.fu).filter(f => f.rev && !f.ok).length;      // revues et pas encore validees
const nbValidees = Object.values(D.fu).filter(f => f.ok).length;
const nbCapstonesRevus = IDS.reduce((a, g) => a + D.br[g].filter(b => b.cr).length, 0);
let usageMax = 0;                                          // etape du pool utilisee par le plus de branches
for (const g of IDS) {
  const use = [0, 1, 2, 3].map(() => [0, 0, 0, 0, 0]);
  D.br[g].forEach(b => b.s.forEach((x, d) => use[d][x]++));
  use.forEach(u => u.forEach(x => { usageMax = Math.max(usageMax, x); }));
}

// ---- HTML ----------------------------------------------------------------------------------------------------------
{
  const legende = '✦ = nouveau ou revu dans la v2 (' + nbEtapesRemplacees + ' étapes nouvelles ou modifiées, ' + nbFusionsRevues + ' fusions et ' +
    nbCapstonesRevus + ' capstones retravaillés) · ✓ = fusion validée (' + nbValidees + ') · « chemin » indique ce que les ' +
    '4 étapes d’une branche apportent au partenaire · ⚙ = forme fusionnée (arme hybride) des paires à distance · Éventail s’appelle maintenant Shotgun.' +
    (nbRevOk || nbRevSup ? ' · Revue des branches : ' + nbRevOk + ' validée(s) ✓, ' + nbRevSup + ' supprimée(s) ✗ (script trier.js).' : '');
  let h = fs.readFileSync(path.join(__dirname, 'modele.html'), 'utf8');
  h = h.replace('/*DONNEES*/null', () => JSON.stringify(D)).replace('/*LEGENDE*/', () => legende).replace('/*CADRE*/', () => D.cadre || '');
  h = h.replace('<title>Arbre de fusions – Boutique Vagues</title>', '<title>Arbre de fusions – Boutique Vagues (v2)</title>');
  fs.writeFileSync(path.join(SORTIE, 'arbre_fusions_boutique_vagues_v2.html'), h);
}

// ---- XLSX ----------------------------------------------------------------------------------------------------------
const esc = s => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
function col(i) { let s = ''; i++; while (i > 0) { const m = (i - 1) % 26; s = String.fromCharCode(65 + m) + s; i = Math.floor((i - 1) / 26); } return s; }

// styles : 0 defaut, 1 en-tete, 2 corps, 3 corps vert (nouveau / revu), 4 corps violet (valide), 5 corps ambre (a valider)
function feuille(entetes, lignes, largeurs) {
  const nCols = entetes.length, nRows = lignes.length + 1;
  const cell = (r, c, v, s) => {
    const ref = col(c) + r;
    if (typeof v === 'number') return '<c r="' + ref + '" s="' + s + '" t="n"><v>' + v + '</v></c>';
    return '<c r="' + ref + '" s="' + s + '" t="inlineStr"><is><t xml:space="preserve">' + esc(v == null ? '' : v) + '</t></is></c>';
  };
  let x = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">' +
    '<sheetPr><outlinePr summaryBelow="1" summaryRight="1"/><pageSetUpPr/></sheetPr><dimension ref="A1:' + col(nCols - 1) + nRows + '"/>' +
    '<sheetViews><sheetView workbookViewId="0"><pane ySplit="1" topLeftCell="A2" activePane="bottomLeft" state="frozen"/>' +
    '<selection pane="bottomLeft" activeCell="A1" sqref="A1"/></sheetView></sheetViews><sheetFormatPr baseColWidth="8" defaultRowHeight="15"/><cols>' +
    largeurs.map((w, i) => '<col width="' + w + '" customWidth="1" min="' + (i + 1) + '" max="' + (i + 1) + '"/>').join('') + '</cols><sheetData>';
  x += '<row r="1">' + entetes.map((e, c) => cell(1, c, e, 1)).join('') + '</row>';
  lignes.forEach((l, i) => {
    const r = i + 2, st = l.s || 2;
    x += '<row r="' + r + '">' + l.v.map((v, c) => cell(r, c, v, st)).join('') + '</row>';
  });
  x += '</sheetData><autoFilter ref="A1:' + col(nCols - 1) + nRows + '"/><pageMargins left="0.75" right="0.75" top="1" bottom="1" header="0.5" footer="0.5"/></worksheet>';
  return x;
}

const nomPaire = (a, b) => TYPES[D.seeds[a].t] + ' × ' + TYPES[D.seeds[b].t];
const cleFu = (a, b) => (D.fu[a + '-' + b] ? a + '-' + b : b + '-' + a);

// 1. Regles
const regles = [
  ['Structure', '20 graines (7 distance, 6 mêlée, 7 compétence). Un loadout = 2 distances + 1 mêlée + 1 compétence : seules les paires jouables existent, et pour l’instant il n’y a AUCUN lien entre une compétence et une mêlée (112 fusions : distance×distance 21, distance×mêlée 42, distance×compétence 49, mêlée×compétence 0).'],
  ['Branches', 'Une branche par partenaire : 4 étapes piochées dans le pool de la graine (une par profondeur 1 à 4) + 1 capstone unique lié au partenaire. Mêlée : 7 branches, compétence : 7 (sans les mêlées), distance : 19.'],
  ['Pools', '5 étapes par profondeur, 20 par graine. Chaque étape modifie une stat (colonne « Axe »). Armes à distance : taille du projectile, vitesse, zone d’effet, portée, nombre de munitions, nombre de projectiles, dégâts, suivi, plus une étape de soin en profondeur 3. Mêlée : une étape par axe et par profondeur parmi cooldown, vitesse d’animation, longueur de combo, munition rendue, lifesteal. Compétences : charges, durée, recharge, multiplicateur, portée (pas de liste donnée). Les effets spéciaux (parry, joute, mines, ondes…) sont dans les capstones et les fusions. Une étape achetée compte pour toutes les branches qui la contiennent.'],
  ['Chemins (v2)', 'Les 4 étapes de chaque branche ne suivent plus une rotation fixe : chacune est choisie pour ce qu’elle apporte au partenaire (colonne « Logique du chemin »). Deux branches qui peuvent cohabiter dans un loadout ne partagent jamais plus d’une étape (en v1, 297 paires sur 1701 avaient un chemin identique). Chaque étape du pool sert au moins une branche et aucune n’en sert plus de ' + usageMax + '. Les colonnes Indice restent modifiables à la main.'],
  ['Étapes retirées (v2)', 'Gel, Brûlure et Contagion n’existent plus dans aucun pool, ni Fracture pour le Railgun, ni les variantes de feu (Sol brûlant, Pique de feu, Combo de feu, Sillage ×2). ' + nbEtapesRemplacees + ' étapes sont nouvelles ou modifiées par rapport à la v1 (colonne « Nouveauté (v2) » du pool) : les pools des armes à distance et de mêlée sont réécrits autour des axes, sans effet d’état.'],
  ['Rareté = profondeur', '1 Commune (5 niveaux, 120), 2 Peu commune (4, 240), 3 Rare (3, 400), 4 Épique (2, 600), 5 Légendaire (1, 850). Poids de tirage 50 / 30 / 14 / 5 / 1. Améliorations de niveau : 60/80/100/120, 120/160/200, 200/300, 400, aucune. +20 % d’effet par niveau (capstone : niveau fixe).'],
  ['Boutique', 'Mode Vagues, loadout seulement : 3 branches proposées par arme (vers les 3 autres armes du loadout). 3 offres tirées selon les poids ; acheter une offre rafraîchit les 3. Reroll payant (proposition : 40 crédits, +20 par reroll consécutif).'],
  ['Fusion', 'Débloquée quand les 5 étapes (4 + capstone) des deux branches de la paire sont achetées. Une seule fusion par run. Emplacement garanti dans la boutique (bord violet) jusqu’à l’achat. Niveau = min(niveaux investis des deux branches) ÷ 5, arrondi inférieur : 1 = 100 %, 2 = 125 %, 3 = 150 %.'],
  ['Changement d’arme', 'Option payante de la boutique. Une fusion acquise se désactive si une des deux armes quitte le loadout et se réactive gratuitement au retour de la paire.'],
  ['Soin', 'Le soin sur kill en PvE est supprimé. Chaque pool contient un effet de soin en profondeur 3 (rare). Proposition : plafond de 20 PV/s de soin total, aucun soin sur les dégâts de réaction en chaîne (vgDepth).'],
  ['Règle des étapes (v2)', 'Une étape d’un pool sert dans tous les loadouts : elle ne dépend jamais d’une autre graine (pièce, marque, leurre, rail…) ni d’une autre étape. Seuls les capstones et les fusions parlent du partenaire (exemple de ce qu’il ne faut pas faire : « Pièces explosives » dans le pool du Shotgun, qui ne sert à rien sans la Pièce). Le script de génération refuse une étape qui nomme une autre graine.'],
  ['Formes fusionnées (proposition)', D.cadre || ''],
  ['Revue des branches', 'Le script outils/trier.js montre une branche complète et propose de la valider ou de la supprimer, avec une note (1 à 5) et un commentaire facultatifs ; les décisions sont dans outils/donnees/revue.json. Feuille Branches : violet = validée, rouge = supprimée. Une branche validée est figée (son chemin ne change plus à la régénération), un chemin supprimé n’est pas reproposé. Aujourd’hui : ' + nbRevOk + ' validée(s), ' + nbRevSup + ' supprimée(s).'],
  ['Statuts des fusions', 'Validée (violet) : validée par le propriétaire, ne pas modifier (' + nbValidees + ', dont DOUBLE DÉTENTE validée pendant la v2). Revue v2 (vert) : retravaillée dans cette version, à valider (' + nbFusionsRevues + '). À valider (ambre) : texte de la v1, jamais relu.'],
  ['Équilibrage lié (v2)', 'Le patch d’équilibrage (session séparée, NET_VER 21, pas encore commité) change : Éventail renommé Shotgun (20 dégâts par plomb au lieu de 8, recul proportionnel à ta vitesse : 1 m/s de poussée par m/s, plafond 60 m/s, 0,10 s, aucun recul à l’arrêt ; la cible reçoit la même poussée en PvP), Élan à 4 munitions (tir qui retire toute la vitesse horizontale), Scie : sphère de rappel du crochet 1,2 m, Pièce à 2 charges de 5 s, compétence Ricochet réécrite (fenêtre de 6 s où tout ce que tu tires rebondit sans limite, recharge 20 s à la fermeture), Tesla × All in One (éclair instantané sur la cible visée, vide la batterie : 130 dégâts à pleine batterie), Roquettes × Pièce (redirigée vers l’ennemi proche, x1,5 par pièce), Sentinelle (un tir sur deux est un laser parable, sinon une orbe à 110 m/s), triangle Mirage ⇒ Pièce ⇒ Ricochet ⇒ Mirage (pierre-feuille-ciseaux). Demandé ensuite et en cours dans la même session : le Yoyo n’a plus de traction (il devient « Balayage de zone »). Les pools Pièce, Ricochet, Shotgun et Yoyo et les fusions Ricochet et Yoyo suivent ces règles.'],
  ['Choix à valider', 'Les valeurs chiffrées sont des propositions à calibrer en jeu.'],
];
const fRegles = feuille(['Sujet', 'Règle'], regles.map(r => ({ v: r })), [22, 130]);

// 2. Archetypes
const fArch = feuille(['Graine', 'Type', 'Archétype', 'Branches'],
  IDS.map(g => ({ v: [D.seeds[g].n, TYPES[D.seeds[g].t], D.seeds[g].a, D.br[g].length] })), [16, 12, 30, 10]);

// 3. Pool d'etapes
const lignesPool = [];
for (const g of IDS) {
  const S = D.seeds[g];
  S.pool.forEach((lvl, d) => lvl.forEach((st, i) => {
    const nbBranches = D.br[g].filter(b => b.s[d] === i).length;
    lignesPool.push({
      s: st.was || st.nv || st.av ? 3 : 2,
      v: [S.n, S.a, d + 1, RAR[d][0], i + 1, st.n, st.e, st.x && D.axes ? D.axes[st.x] : '', RAR[d][1], RAR[d][2], RAR[d][3], /^SOIN/.test(st.e) ? 'oui' : '', nouveauteEtape(st), nbBranches],
    });
  }));
}
const fPool = feuille(['Graine', 'Archétype', 'Profondeur', 'Rareté', 'Indice', 'Étape', 'Effet (niveau 1)', 'Axe', 'Niveaux max', 'Prix d’achat', 'Poids', 'Soin', 'Nouveauté (v2)', 'Branches qui l’utilisent'],
  lignesPool, [14, 26, 11, 13, 8, 22, 58, 22, 11, 11, 8, 7, 30, 12]);

// 4. Branches
const lignesBr = [];
for (const g of IDS) {
  for (const b of D.br[g]) {
    const S = D.seeds[g];
    lignesBr.push({
      s: b.rv === 'ok' ? 4 : b.rv === 'sup' ? 6 : b.cr ? 3 : 2,
      v: [S.n, D.seeds[b.p].n, S.a, ...b.s.map((idx, d) => S.pool[d][idx].n), b.c.n, b.c.e, b.f, ...b.s.map(x => x + 1), b.w || '', b.cr ? 'revu (v2)' : '',
        b.rv === 'ok' ? 'Validée' : b.rv === 'sup' ? 'Supprimée' : b.rv === 'mod' ? (b.rp ? 'Retravaillée, à revalider' : 'Modifiée depuis le tri') : '', b.nt || '', b.cm || '', b.rp || ''],
    });
  }
}
const fBr = feuille(['Graine', 'Vers', 'Archétype', 'Étape 1', 'Étape 2', 'Étape 3', 'Étape 4', 'Capstone (étape 5)', 'Effet du capstone', 'Fusion', 'Indice 1', 'Indice 2', 'Indice 3', 'Indice 4', 'Logique du chemin', 'Capstone', 'Revue', 'Note (1-5)', 'Commentaire', 'Réponse'],
  lignesBr, [14, 14, 24, 20, 20, 20, 20, 24, 50, 22, 8, 8, 8, 8, 34, 11, 22, 10, 50, 60]);

// 5. Fusions
const lignesFu = [];
for (const [k, f] of Object.entries(D.fu)) {
  const [a, b] = k.split('-');
  const cab = D.br[a].find(x => x.p === b), cba = D.br[b].find(x => x.p === a);
  lignesFu.push({
    s: f.ok ? 4 : f.rev ? 3 : 5,
    v: [f.n, D.seeds[a].n, D.seeds[b].n, nomPaire(a, b), f.e, cab.c.n + ' : ' + cab.c.e, cba.c.n + ' : ' + cba.c.e,
      f.ok ? 'Validée' : f.rev ? 'Revue (v2)' : 'À valider', f.np || '', f.h ? f.h.n + (f.h.ok ? ' ✓ validée' : '') + ' (' + f.h.b + ') : ' + f.h.e : ''],
  });
}
const fFu = feuille(['Fusion', 'Graine A', 'Graine B', 'Type de paire', 'Effet de la fusion', 'Capstone A → B', 'Capstone B → A', 'Statut', 'Adaptation Vagues (à confirmer)', 'Forme fusionnée (arme hybride temporaire, distance × distance)'],
  lignesFu, [22, 14, 14, 24, 70, 50, 50, 12, 50, 70]);

const feuilles = [['Règles', fRegles], ['Archétypes', fArch], ['Pool d\'étapes', fPool], ['Branches', fBr], ['Fusions', fFu]];
const dims = [regles.length + 1, IDS.length + 1, lignesPool.length + 1, lignesBr.length + 1, lignesFu.length + 1];
const lastCol = ['B', 'D', 'N', 'T', 'J'];

const styles = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><numFmts count="0"/>' +
  '<fonts count="3"><font><name val="Calibri"/><family val="2"/><sz val="11"/></font><font><name val="Arial"/><b val="1"/><color rgb="00FFFFFF"/><sz val="10"/></font><font><name val="Arial"/><sz val="10"/></font></fonts>' +
  '<fills count="7"><fill><patternFill/></fill><fill><patternFill patternType="gray125"/></fill><fill><patternFill patternType="solid"><fgColor rgb="002F2A4A"/></patternFill></fill>' +
  '<fill><patternFill patternType="solid"><fgColor rgb="00E3F4E8"/></patternFill></fill><fill><patternFill patternType="solid"><fgColor rgb="00EFE7FF"/></patternFill></fill><fill><patternFill patternType="solid"><fgColor rgb="00FFF4D6"/></patternFill></fill><fill><patternFill patternType="solid"><fgColor rgb="00FDE2E2"/></patternFill></fill></fills>' +
  '<borders count="1"><border><left/><right/><top/><bottom/><diagonal/></border></borders><cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>' +
  '<cellXfs count="7"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>' +
  '<xf numFmtId="0" fontId="1" fillId="2" borderId="0" applyAlignment="1" xfId="0"><alignment vertical="center" wrapText="1"/></xf>' +
  '<xf numFmtId="0" fontId="2" fillId="0" borderId="0" applyAlignment="1" xfId="0"><alignment vertical="top" wrapText="1"/></xf>' +
  '<xf numFmtId="0" fontId="2" fillId="3" borderId="0" applyFill="1" applyAlignment="1" xfId="0"><alignment vertical="top" wrapText="1"/></xf>' +
  '<xf numFmtId="0" fontId="2" fillId="4" borderId="0" applyFill="1" applyAlignment="1" xfId="0"><alignment vertical="top" wrapText="1"/></xf>' +
  '<xf numFmtId="0" fontId="2" fillId="5" borderId="0" applyFill="1" applyAlignment="1" xfId="0"><alignment vertical="top" wrapText="1"/></xf>' +
  '<xf numFmtId="0" fontId="2" fillId="6" borderId="0" applyFill="1" applyAlignment="1" xfId="0"><alignment vertical="top" wrapText="1"/></xf></cellXfs>' +
  '<cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles></styleSheet>';

const classeur = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><workbookPr/>' +
  '<bookViews><workbookView activeTab="0"/></bookViews><sheets>' +
  feuilles.map((f, i) => '<sheet name="' + esc(f[0]) + '" sheetId="' + (i + 1) + '" r:id="rId' + (i + 1) + '"/>').join('') + '</sheets><definedNames>' +
  feuilles.map((f, i) => '<definedName name="_xlnm._FilterDatabase" localSheetId="' + i + '" hidden="1">\'' + f[0].replace(/'/g, "''") + '\'!$A$1:$' + lastCol[i] + '$' + dims[i] + '</definedName>').join('') +
  '</definedNames><calcPr calcId="124519" fullCalcOnLoad="1"/></workbook>';

const rels = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">' +
  feuilles.map((f, i) => '<Relationship Id="rId' + (i + 1) + '" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet' + (i + 1) + '.xml"/>').join('') +
  '<Relationship Id="rId' + (feuilles.length + 1) + '" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>';

const types = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>' +
  '<Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>' +
  '<Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/>' +
  feuilles.map((f, i) => '<Override PartName="/xl/worksheets/sheet' + (i + 1) + '.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>').join('') + '</Types>';

const racine = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>';

function zip(fichiers) {
  const parts = [], central = [];
  let offset = 0;
  const date = ((2026 - 1980) << 9) | (1 << 5) | 1, heure = 0;
  for (const [nom, contenu] of fichiers) {
    const nomB = Buffer.from(nom, 'utf8'), data = Buffer.from(contenu, 'utf8');
    const comp = zlib.deflateRawSync(data), crc = zlib.crc32(data);
    const lh = Buffer.alloc(30);
    lh.writeUInt32LE(0x04034b50, 0); lh.writeUInt16LE(20, 4); lh.writeUInt16LE(0x0800, 6); lh.writeUInt16LE(8, 8);
    lh.writeUInt16LE(heure, 10); lh.writeUInt16LE(date, 12); lh.writeUInt32LE(crc, 14); lh.writeUInt32LE(comp.length, 18);
    lh.writeUInt32LE(data.length, 22); lh.writeUInt16LE(nomB.length, 26); lh.writeUInt16LE(0, 28);
    parts.push(lh, nomB, comp);
    const ch = Buffer.alloc(46);
    ch.writeUInt32LE(0x02014b50, 0); ch.writeUInt16LE(20, 4); ch.writeUInt16LE(20, 6); ch.writeUInt16LE(0x0800, 8); ch.writeUInt16LE(8, 10);
    ch.writeUInt16LE(heure, 12); ch.writeUInt16LE(date, 14); ch.writeUInt32LE(crc, 16); ch.writeUInt32LE(comp.length, 20);
    ch.writeUInt32LE(data.length, 24); ch.writeUInt16LE(nomB.length, 28); ch.writeUInt32LE(offset, 42);
    central.push(ch, nomB);
    offset += lh.length + nomB.length + comp.length;
  }
  const cd = Buffer.concat(central);
  const eocd = Buffer.alloc(22);
  eocd.writeUInt32LE(0x06054b50, 0); eocd.writeUInt16LE(fichiers.length, 8); eocd.writeUInt16LE(fichiers.length, 10);
  eocd.writeUInt32LE(cd.length, 12); eocd.writeUInt32LE(offset, 16);
  return Buffer.concat([...parts, cd, eocd]);
}

const fichiers = [
  ['[Content_Types].xml', types], ['_rels/.rels', racine], ['xl/workbook.xml', classeur], ['xl/_rels/workbook.xml.rels', rels], ['xl/styles.xml', styles],
  ...feuilles.map((f, i) => ['xl/worksheets/sheet' + (i + 1) + '.xml', f[1]]),
];
fs.writeFileSync(path.join(SORTIE, 'fusions_boutique_vagues_v2.xlsx'), zip(fichiers));
console.log('écrit : arbre_fusions_boutique_vagues_v2.html, fusions_boutique_vagues_v2.xlsx');
console.log(' lignes : pool ' + lignesPool.length + ', branches ' + lignesBr.length + ', fusions ' + lignesFu.length);
