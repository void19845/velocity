// Genere src/solo/boutique_donnees.c depuis donnees/arbre_v2.json (les 7 graines du mode VAGUES).
// Usage : cd docs/boutique_vagues/outils && node export_c.js
// Un pas (etape) sans entree dans EFFETS est marque BK_NONE : la boutique l'affiche "A VENIR" et ne le propose pas.
const fs = require('fs'), path = require('path');
const A = require('./donnees/arbre_v2.json');

const SEEDS = ['RAIL', 'ROQ', 'SHO', 'LAN', 'IMP', 'PIE', 'AIO'];         // ordre = SH_* dans boutique.c
const CNAME = { RAIL: 'SH_RAIL', ROQ: 'SH_ROQ', SHO: 'SH_SHO', LAN: 'SH_LAN', IMP: 'SH_IMP', PIE: 'SH_PIE', AIO: 'SH_AIO' };

// effets implementes : "GRAINE:Nom de l'etape" -> [genre, a, b]  (BK_FX : a = FX_* de ameliorations.c ; BK_SHO, BK_ROQ... : voir effets/)
//   ou une ETAPE-STAT { stat, op, val, cond, par, unit, grp, fixed } (ou un tableau) : genre BK_STAT, une ligne de shMods[] par modificateur
const EFFETS = require('./effets_c.js');
const CAPS = require('./effets_c.js').CAPS;      // "GRAINE>PARTENAIRE" -> [genre, a, b]
const FUS = require('./effets_c.js').FUS;        // "NOM DE FUSION" -> [genre, a, b]

const norm = s => s.replace(/œ/g, 'oe').replace(/Œ/g, 'OE').replace(/[’‘]/g, "'").replace(/[«»“”]/g, '"')
  .replace(/×/g, 'x').replace(/→/g, '->').replace(/≥/g, '>=').replace(/°/g, ' deg')
  .normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/[^\x20-\x7e]/g, '?').replace(/\\/g, '/').replace(/"/g, '\\"');
const str = s => '"' + norm(s) + '"';
const eff = e => e ? `${e[0]}, ${typeof e[1] === 'string' ? e[1] : (e[1] || 0)}, ${e[2] || 0}` : 'BK_NONE, 0, 0';
const key = n => norm(n).toLowerCase();

// --- etapes-stat : validation contre les enums de src/monde/entites.c (une seule source de verite : le C) ---
const ENT = fs.readFileSync(path.join(__dirname, '../../../src/monde/entites.c'), 'utf8');
const enumNames = (prefix, marker) => {                    // identifiants PREFIXE_x de l'enum qui contient `marker`
  const m = ENT.match(new RegExp('enum\\s*\\{[^}]*\\b' + marker + '\\b[^}]*\\}'));
  if (!m) throw new Error('enum ' + marker + ' introuvable dans src/monde/entites.c');
  return new Set([...m[0].matchAll(new RegExp('\\b' + prefix + '_([A-Z0-9_]+)\\b', 'g'))].map(x => x[1]).filter(n => n !== 'COUNT' && n !== 'N'));
};
const STATS = enumNames('STAT', 'STAT_COUNT'), OPS = enumNames('SOP', 'SOP_MUL'), CONDS = enumNames('COND', 'COND_NONE');
const isStatEntry = e => !!e && (!Array.isArray(e) || (e.length > 0 && typeof e[0] === 'object'));
const modsOf = e => Array.isArray(e) ? e : [e];
const checkMod = (m, where) => {
  const bad = msg => { throw new Error(`effets : ${where} : ${msg}`); };
  const stat = String(m.stat), op = String(m.op || 'mul').toUpperCase(), cond = String(m.cond || 'none').toUpperCase();
  if (!STATS.has(stat)) bad(`stat '${m.stat}' inconnue (enum STAT_* de src/monde/entites.c : ${[...STATS].join(' ')})`);
  if (!OPS.has(op)) bad(`op '${m.op}' inconnue (${[...OPS].join(' ').toLowerCase()})`);
  if (!CONDS.has(cond)) bad(`cond '${m.cond}' inconnue (${[...CONDS].join(' ').toLowerCase()})`);
  if (typeof m.val !== 'number' || !isFinite(m.val)) bad('val doit etre un nombre');
  if (cond === 'NEAR' && !(m.par > 0)) bad("cond 'near' : par (distance en metres) obligatoire");
  if (cond === 'BOUNCE' && !(Number.isInteger(m.par) && m.par >= 1)) bad("cond 'bounce' : par (entier >= 1, rebond a partir duquel la condition compte, 1 = premier) obligatoire");
  if (cond === 'PIERCED' && m.par !== undefined && !(Number.isInteger(m.par) && m.par >= 0)) bad("cond 'pierced' : par (plafond, entier >= 0, 0 = aucun)");
  if (m.grp !== undefined && !(Number.isInteger(m.grp) && m.grp > 0 && m.grp < 256)) bad('grp : entier de 1 a 255');
  if (m.grp !== undefined && op === 'TO') bad("grp : inutile avec op 'to' (elle garde deja le meilleur candidat)");
  if (m.phase !== undefined && !(Number.isInteger(m.phase) && m.phase >= 0 && m.phase <= 3)) bad('phase : entier de 0 a 3');
  if (m.unit !== undefined && m.unit !== 'deg') bad("unit : seulement 'deg'");
  return { stat, op, cond, par: m.par || 0, val: m.val, unit: m.unit, grp: m.grp || 0, phase: m.phase || 0, fixed: !!m.fixed };
};
const fl = x => (Number.isInteger(x) ? x.toFixed(1) : String(+x.toPrecision(7))) + 'f';   // litteral float C
const modRows = [];                                       // { s (graine), step, name, m } dans l'ordre d'emission

let o = `/* GENERE par docs/boutique_vagues/outils/export_c.js depuis arbre_v2.json : ne pas modifier a la main.
   Boutique du mode VAGUES : 7 graines (Railgun, Roquettes, Shotgun, Lance, Impulsion, Piece, All in One). */\n\n`;
let nImpl = 0, nTot = 0, nStat = 0;
o += 'static const ShStep shSteps[SH_N][20] = {\n';
for (const s of SEEDS) {
  o += `    { /* ${s} */\n`;
  A.seeds[s].pool.forEach((d, di) => d.forEach((p, pi) => {
    const e = EFFETS[s + ':' + p.n];
    nTot++; if (e) nImpl++;
    if (isStatEntry(e)) {                                 // etape-stat : a = indice de sa premiere ligne dans shMods, b = nombre de lignes
      const first = modRows.length, ms = modsOf(e);
      ms.forEach(m => modRows.push({ s, step: di * 5 + pi, name: p.n, m: checkMod(m, s + ':' + p.n) }));
      nStat++;
      o += `        { ${str(p.n)}, ${str(p.e)}, ${di + 1}, BK_STAT, ${first}, ${ms.length} },\n`;
    } else o += `        { ${str(p.n)}, ${str(p.e)}, ${di + 1}, ${eff(e)} },\n`;
  }));
  o += '    },\n';
}
o += '};\n\n';

// table des modificateurs du sac de stats (src/solo/boutique_stats.c) : une ligne par modificateur d'une etape-stat
const perKey = {};
for (const r of modRows) { const k = r.s + ':' + r.m.stat; perKey[k] = (perKey[k] || 0) + 1; if (perKey[k] > 16) throw new Error('trop de modificateurs pour ' + k); }
o += `#define SH_NMOD ${modRows.length}\n`;
o += '/* seed, step, stat, op, cond, grp, flags, phase, val, par : voir StatMod (src/monde/entites.c) ; val = facteur (mul), increment (add), plancher (max), plafond (min) ou cible (to) au niveau 1 */\n';
o += 'static const StatMod shMods[SH_NMOD] = {\n';
for (const r of modRows) {
  const m = r.m, val = m.unit === 'deg' ? `${fl(m.val)} * (PI / 180.0f)` : fl(m.val);
  o += `    { ${CNAME[r.s]}, ${r.step}, STAT_${m.stat}, SOP_${m.op}, COND_${m.cond}, ${m.grp}, ${m.fixed ? 'SMF_FIXED' : '0'}, ${m.phase}, ${val}, ${fl(m.par)} },   /* ${norm(r.name)} */\n`;
}
o += '};\n\n';

const brs = [];
for (const s of SEEDS) for (const b of A.br[s]) if (SEEDS.includes(b.p)) brs.push([s, b]);
o += `#define SH_NBR ${brs.length}\n`;
o += 'static const ShBranch shBr[SH_NBR] = {\n';
let capImpl = 0;
for (const [s, b] of brs) {
  const e = CAPS[s + '>' + b.p];
  if (e) capImpl++;
  o += `    { ${CNAME[s]}, ${CNAME[b.p]}, { ${b.s.join(', ')} }, ${str(b.c.n)}, ${str(b.c.e)}, ${eff(e)} },\n`;
}
o += '};\n\n';

const fus = [];
for (const k of Object.keys(A.fu)) {
  const [x, y] = k.split('-');
  if (SEEDS.includes(x) && SEEDS.includes(y)) fus.push([x, y, A.fu[k]]);
}
o += `#define SH_NFU ${fus.length}\n`;
o += 'static const ShFusion shFu[SH_NFU] = {\n';
let fuImpl = 0;
for (const [x, y, f] of fus) {
  const e = FUS[f.n];
  if (e) fuImpl++;
  o += `    { ${CNAME[x]}, ${CNAME[y]}, ${str(f.n)}, ${str(f.e)}, ${eff(e)} },\n`;
}
o += '};\n';

// formes fusionnees (armes hybrides) des paires dont les deux graines sont jouables ; id = HY_* (HY_NONE : pas encore codee)
const HYID = { 'artillerie orbitale': 'HY_ARTILLERIE', 'canon scie': 'HY_CANON', 'obusier': 'HY_OBUSIER' };
const hys = [];
for (const k of Object.keys(A.fu)) {
  const [x, y] = k.split('-');
  if (SEEDS.includes(x) && SEEDS.includes(y) && A.fu[k].h) hys.push([x, y, A.fu[k].h]);
}
o += '\nenum { HY_NONE, HY_ARTILLERIE, HY_CANON, HY_OBUSIER, HY_N };\n';
o += 'typedef struct { unsigned char a, b, id; const char *name, *base, *desc; } ShHybrid;\n';
o += `#define SH_NHY ${hys.length}\n`;
o += 'static const ShHybrid shHy[SH_NHY] = {\n';
for (const [x, y, h] of hys)
  o += `    { ${CNAME[x]}, ${CNAME[y]}, ${HYID[key(h.n)] || 'HY_NONE'}, ${str(h.n)}, ${str(h.b)}, ${str(h.e)} },\n`;
o += '};\n';
fs.writeFileSync(path.join(__dirname, '../../../src/solo/boutique_donnees.c'), o);
console.log(`etapes implementees ${nImpl}/${nTot} (dont ${nStat} etapes-stat, ${modRows.length} modificateurs) ; capstones ${capImpl}/${brs.length} ; fusions ${fuImpl}/${fus.length}`);
