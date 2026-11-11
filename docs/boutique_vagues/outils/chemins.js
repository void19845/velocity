// Attribution des chemins : pour chaque branche (graine -> partenaire), choisir 1 etape par profondeur.
// Recuit simule, deterministe (graine fixe). Objectifs :
//  - le chemin colle au partenaire (score de profil, voir profils.js) ;
//  - deux branches qui peuvent cohabiter dans un loadout (2 distances + 1 melee + 1 competence) partagent
//    peu d'etapes : jamais un chemin identique, rarement plus d'une etape commune ;
//  - chaque etape du pool sert au moins une branche, et aucune n'est utilisee partout.
const { etiquettes, PROFILS, LIBELLES } = require('./profils');
const { AXES, DEPUIS_DISTANCE, DEPUIS_MELEE } = require('./axes');

// Armes a distance et de melee : l'etiquette d'une etape est son axe (st.x) et le profil du partenaire est exprime en axes.
// Competences : etiquettes calculees (profils.js).
const LIB = Object.assign({}, LIBELLES, AXES);
function profilDe(D, graine, p) {
  const t = D.seeds[graine].t;
  return t === 'D' ? DEPUIS_DISTANCE[p] : t === 'M' ? DEPUIS_MELEE[p] : PROFILS[p];
}
function etiquettesDe(graine, d, i, st) { return st.x ? [st.x] : etiquettes(graine, d, i, st); }

function rng(seed) {
  let a = seed | 0;
  return function () {
    a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function cohabitent(typeGraine, ta, tb) {
  const n = { D: 0, M: 0, C: 0 };
  n[typeGraine]++; n[ta]++; n[tb]++;
  return n.D <= 2 && n.M <= 1 && n.C <= 1;
}

const PEN = [0, 2.5, 10, 25, 50];                    // branches qui cohabitent : penalite selon le nombre d'etapes communes (0 a 4)
const PEN_TOUTES = [0, 0.6, 2.5, 7, 15];              // toutes les autres paires de branches d'une graine (diversite)
const POIDS_AFF = 2;                                  // poids du score de profil

function attribuerGraine(D, graine, opts) {
  const S = D.seeds[graine];
  const partenaires = D.br[graine].map(b => b.p);
  const n = partenaires.length;
  const tags = S.pool.map((lvl, d) => lvl.map((st, i) => etiquettesDe(graine, d, i, st)));
  const aff = partenaires.map(p => S.pool.map((lvl, d) => lvl.map((st, i) =>
    tags[d][i].reduce((a, t) => a + (profilDe(D, graine, p)[t] || 0), 0))));
  const voisins = Array.from({ length: n }, () => []);            // toutes les autres branches
  const tab = Array.from({ length: n }, () => Array(n).fill(PEN_TOUTES));  // table de penalite de chaque paire
  for (let i = 0; i < n; i++) for (let j = i + 1; j < n; j++) {
    voisins[i].push(j); voisins[j].push(i);
    if (cohabitent(S.t, D.seeds[partenaires[i]].t, D.seeds[partenaires[j]].t)) { tab[i][j] = PEN; tab[j][i] = PEN; }
  }
  const CAP = Math.ceil(n / 3) + 1;
  const usageCout = u => (u === 0 ? 15 : 0) + (u > CAP ? (u - CAP) * 3 : 0);
  // revue du proprietaire : chemins valides = figes ; chemins supprimes = interdits (l'optimiseur en propose un autre)
  const fixes = opts.fixes || {}, refuses = opts.refuses || {};
  const fixe = partenaires.map(p => fixes[graine + '>' + p] || null);
  const interdits = partenaires.map(p => new Set((refuses[graine + '>' + p] || []).map(t => t.join(','))));
  const libres = partenaires.map((_, i) => i).filter(i => !fixe[i]);
  const PEN_REFUS = 500;
  // contraintes par etape (revue) : sans = etapes interdites dans ce chemin, avec = etape imposee a une profondeur
  const sansOpt = opts.sans || {}, avecOpt = opts.avec || {};
  const sansSet = partenaires.map(p => new Set((sansOpt[graine + '>' + p] || []).map(([d, i]) => d + ':' + i)));
  const avec = partenaires.map(p => avecOpt[graine + '>' + p] || {});

  let meilleur = null, meilleurCout = Infinity;
  for (let essai = 0; essai < (opts.essais || 3); essai++) {
    const rand = rng(opts.graine * 1000 + essai * 77 + graine.charCodeAt(0));
    const etat = partenaires.map((_, pi) => {
      const e = fixe[pi] ? fixe[pi].slice() : [0, 1, 2, 3].map(() => Math.floor(rand() * 5));
      for (const [d, i] of Object.entries(avec[pi])) e[d] = i;
      return e;
    });
    const use = [0, 1, 2, 3].map(() => [0, 0, 0, 0, 0]);
    etat.forEach(e => e.forEach((idx, d) => use[d][idx]++));
    const commun = (a, b) => { let k = 0; for (let d = 0; d < 4; d++) if (etat[a][d] === etat[b][d]) k++; return k; };
    const coutTotal = () => {
      let c = 0;
      for (let p = 0; p < n; p++) for (let d = 0; d < 4; d++) c -= POIDS_AFF * aff[p][d][etat[p][d]];
      for (let p = 0; p < n; p++) for (const q of voisins[p]) if (q > p) c += tab[p][q][commun(p, q)];
      for (let d = 0; d < 4; d++) for (let i = 0; i < 5; i++) c += usageCout(use[d][i]);
      for (let p = 0; p < n; p++) if (interdits[p].has(etat[p].join(','))) c += PEN_REFUS;
      for (let p = 0; p < n; p++) for (let d = 0; d < 4; d++) if (sansSet[p].has(d + ':' + etat[p][d])) c += PEN_REFUS;
      return c;
    };
    let cout = coutTotal();
    const ITER = opts.iterations || 150000, T0 = 4, T1 = 0.05;
    for (let it = 0; it < ITER && libres.length; it++) {
      const T = T0 * Math.pow(T1 / T0, it / ITER);
      const p = libres[Math.floor(rand() * libres.length)], d = Math.floor(rand() * 4);
      if (avec[p][d] !== undefined) continue;                  // etape imposee a cette profondeur
      const ancien = etat[p][d];
      let nouveau = Math.floor(rand() * 4); if (nouveau >= ancien) nouveau++;
      let delta = -POIDS_AFF * (aff[p][d][nouveau] - aff[p][d][ancien]);
      if (sansSet[p].size) delta += (sansSet[p].has(d + ':' + nouveau) ? PEN_REFUS : 0) - (sansSet[p].has(d + ':' + ancien) ? PEN_REFUS : 0);
      if (interdits[p].size) {
        const apres = etat[p].slice(); apres[d] = nouveau;
        delta += (interdits[p].has(apres.join(',')) ? PEN_REFUS : 0) - (interdits[p].has(etat[p].join(',')) ? PEN_REFUS : 0);
      }
      for (const q of voisins[p]) {
        let k = commun(p, q);
        const avant = k;
        if (etat[q][d] === ancien) k--;
        if (etat[q][d] === nouveau) k++;
        delta += tab[p][q][k] - tab[p][q][avant];
      }
      delta += usageCout(use[d][ancien] - 1) - usageCout(use[d][ancien]);
      delta += usageCout(use[d][nouveau] + 1) - usageCout(use[d][nouveau]);
      if (delta < 0 || rand() < Math.exp(-delta / T)) {
        etat[p][d] = nouveau; use[d][ancien]--; use[d][nouveau]++; cout += delta;
      }
    }
    cout = coutTotal();
    if (cout < meilleurCout) { meilleurCout = cout; meilleur = etat.map(e => e.slice()); }
  }
  // libelle du chemin : les deux etiquettes qui comptent le plus pour ce partenaire
  const resultat = partenaires.map((p, pi) => {
    const poids = {};
    const prof = profilDe(D, graine, p);
    meilleur[pi].forEach((idx, d) => {
      for (const t of tags[d][idx]) if (prof[t]) poids[t] = (poids[t] || 0) + prof[t];
    });
    const top = Object.entries(poids).sort((a, b) => b[1] - a[1]).slice(0, 2).map(x => LIB[x[0]]);
    return { p, s: meilleur[pi], w: top.join(' · ') };
  });
  return resultat;
}

// Libelle d'un chemin donne (utilise aussi pour les chemins fixes a la main)
function libelleChemin(D, graine, partenaire, s) {
  const poids = {};
  const prof = profilDe(D, graine, partenaire);
  s.forEach((idx, d) => {
    for (const t of etiquettesDe(graine, d, idx, D.seeds[graine].pool[d][idx])) if (prof[t]) poids[t] = (poids[t] || 0) + prof[t];
  });
  return Object.entries(poids).sort((a, b) => b[1] - a[1]).slice(0, 2).map(x => LIB[x[0]]).join(' · ');
}

function attribuer(D, opts) {
  const sortie = {};
  for (const g of Object.keys(D.seeds)) sortie[g] = attribuerGraine(D, g, opts || { graine: 1 });
  return sortie;
}

// Rapport : chevauchements, etapes inutilisees, utilisation maximale
function rapport(D) {
  const lignes = [];
  const hist = [0, 0, 0, 0, 0];
  let paires = 0, inutilisees = 0, maxUse = 0;
  for (const g of Object.keys(D.seeds)) {
    const brs = D.br[g];
    for (let i = 0; i < brs.length; i++) for (let j = i + 1; j < brs.length; j++) {
      if (!cohabitent(D.seeds[g].t, D.seeds[brs[i].p].t, D.seeds[brs[j].p].t)) continue;
      let k = 0; for (let d = 0; d < 4; d++) if (brs[i].s[d] === brs[j].s[d]) k++;
      hist[k]++; paires++;
    }
    const use = [0, 1, 2, 3].map(() => [0, 0, 0, 0, 0]);
    brs.forEach(b => b.s.forEach((idx, d) => use[d][idx]++));
    use.forEach(u => u.forEach(x => { if (x === 0) inutilisees++; maxUse = Math.max(maxUse, x); }));
  }
  lignes.push('paires de branches qui cohabitent : ' + paires + ' ; étapes communes 0/1/2/3/4 = ' + hist.join(' / '));
  lignes.push('étapes inutilisées : ' + inutilisees + ' ; utilisation maximale d’une étape : ' + maxUse + ' branches');
  return lignes.join('\n');
}

module.exports = { attribuer, rapport, cohabitent, libelleChemin };
