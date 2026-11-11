// Effets codes dans le jeu : tout ce qui n'est pas liste reste "A VENIR" dans la boutique (jamais propose).
// Un fichier par groupe dans effets/ ; chacun exporte { steps, caps, fus }.
// Deux formats d'entree :
//  1. ETAPE-STAT (sac de stats, src/solo/boutique_stats.c) : un objet { stat, op, val, cond?, par?, unit?, grp?, phase?, fixed? } ou un
//     tableau de ces objets (une etape qui modifie plusieurs stats). Le genre est alors BK_STAT et chaque objet devient une
//     ligne de la table shMods[] de boutique_donnees.c. Les noms de stats, d'operations et de conditions sont ceux des enums
//     STAT_* / SOP_* / COND_* de src/monde/entites.c, sans prefixe et en minuscules pour op et cond (le generateur les verifie). Champs :
//       stat  nom d'une stat (DEG, TAI, VIT, ZON, POR, MUN, NBP, SUI, CD, ANI, COM, MUR, VOL, CONE, ... : liste et sens dans docs/boutique_vagues/)
//       op    'mul' (val = facteur : 1.15 = +15 %), 'add' (val = increment), 'max' (plancher : la valeur ne descend pas sous val),
//             'min' (plafond : ne monte pas au-dessus de val), 'to' (val = valeur cible : v = base - (base - val) x echelle, ignoree si elle
//             n'abaisse pas la valeur, le meilleur candidat gagne), 'mull' (comme mul, mais quand la condition se repete (pierced : N cibles)
//             les deltas s'ADDITIONNENT : 1 + (val - 1) x N, au lieu de val^N)
//       val   valeur de l'etape au niveau 1 ; au niveau n le DELTA est multiplie par sh_scale (gain ET penalite montent)
//       cond  'near' (par = distance en m, strictement sous), 'head', 'air', 'elite', 'pierced' (par = plafond, 0 = aucun),
//             'direct' (cible touchee en plein), 'bounce' (par = rebond a partir duquel on compte, >= 1 ; une application par rebond),
//             'first' (premier rebond), 'salvefull' (salve lancee chargeur plein) ; absent = toujours
//       unit  'deg' : val est en degres (converti en radians dans la table)
//       grp   numero de groupe (> 0) : parmi les modificateurs APPLICABLES d'une meme stat et d'un meme groupe, SEUL le plus fort compte
//       phase 0 a 3 (defaut 0) : ordre d'application, phase par phase ; dans une phase : to, puis (valeur + add) x mul, puis max, puis min
//       fixed true : la valeur ne monte pas avec le niveau (entier fixe, plafond absolu) ; l'etape n'est alors pas proposee en amelioration
//  2. EFFET CODE A LA MAIN : [genre, a, b] (BK_*, enum dans src/solo/ameliorations.c) :
//       BK_FX (a = FX_*), BK_SHO / BK_ROQ / BK_RAIL / BK_COMP / BK_MEL_* (a = identifiant de la famille, b = parametre)
const fs = require('fs'), path = require('path');
const E = {}, CAPS = {}, FUS = {};
for (const f of fs.readdirSync(path.join(__dirname, 'effets')).filter(f => f.endsWith('.js'))) {
  const m = require('./effets/' + f);
  Object.assign(E, m.steps || {}); Object.assign(CAPS, m.caps || {}); Object.assign(FUS, m.fus || {});
}
module.exports = new Proxy(E, { get: (t, k) => k === 'CAPS' ? CAPS : k === 'FUS' ? FUS : t[k] });
