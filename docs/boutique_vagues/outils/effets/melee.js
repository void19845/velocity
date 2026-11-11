// Effets codes pour le groupe 'melee' (Lance et Impulsion). Cle des etapes : "GRAINE:Nom" ; capstones "GRAINE>PARTENAIRE" ; fusions "NOM".
// Genres : voir effets_c.js. Les noms sont lus dans arbre_v2.json par (graine, profondeur, rang) pour eviter toute faute de frappe.
// Code : src/solo/boutique_melee.c (genres BK_MEL_*, enum dans src/solo/ameliorations.c).
const A = require('../donnees/arbre_v2.json');
const nm = (s, d, r) => `${s}:${A.seeds[s].pool[d][r].n}`;
const steps = {};
const put = (s, d, r, e) => { steps[nm(s, d, r)] = e; };
// --- Lance (profondeur 0..3, rang 0..4)
put('LAN', 0, 0, ['BK_MEL_JCD', 20]);          // Recharge : joute reussie -0,2 s
put('LAN', 0, 1, ['BK_MEL_VELO', 10]);         // Velocite : vitesse de rapprochement +10 %
put('LAN', 0, 2, ['BK_MEL_ESTOCS', 1]);        // Second estoc
put('LAN', 0, 3, { stat: 'MUR', op: 'add', val: 10 });   // Moisson : +10 points de jauge par estoc (sac de stats : MUR)
put('LAN', 0, 4, { stat: 'VOL', op: 'add', val: 0.02 });   // vol de vie 2 % (sac de stats : VOL, fraction des degats)
put('LAN', 1, 0, ['BK_MEL_ECD', 15]);          // Estoc vif
put('LAN', 1, 1, ['BK_MEL_EANI', 20]);         // Lance vive
put('LAN', 1, 2, ['BK_MEL_ESTOCS', 2]);        // Estocs en chaine
put('LAN', 1, 3, ['BK_MEL_JPTS', 25]);         // Butin de joute
put('LAN', 1, 4, { stat: 'VOL', op: 'add', val: 0.04 });
put('LAN', 2, 0, ['BK_MEL_JSOIN', 10]);        // Armure de fer
put('LAN', 2, 1, ['BK_MEL_RELEVE', 25]);       // Releve : estoc dans le vide, recharge 0,25 s
put('LAN', 2, 2, ['BK_MEL_TAP', 10]);          // Charge eclair (0,10 s)
put('LAN', 2, 3, ['BK_MEL_JCHAIN', 1]);        // Joute en chaine
put('LAN', 2, 4, ['BK_MEL_JELITE', 40]);       // Pillage d'elite
put('LAN', 3, 0, ['BK_MEL_RUEE', 2]);          // Ruee
put('LAN', 3, 1, ['BK_MEL_ECLAIR', 35]);       // Lance-eclair
put('LAN', 3, 2, ['BK_MEL_TOURNOI', 20, 60]);  // Tournoi
put('LAN', 3, 3, ['BK_MEL_KPTS', 100]);        // Tresor de guerre
put('LAN', 3, 4, { stat: 'VOL', op: 'add', val: 0.10 });
// --- Impulsion
put('IMP', 0, 0, ['BK_MEL_FEND', 10]);         // Cadence
put('IMP', 0, 1, ['BK_MEL_FGAP', 20]);         // Poings vifs
put('IMP', 0, 2, ['BK_MEL_FN', 4]);            // Quatrieme coup
put('IMP', 0, 3, { stat: 'MUR', op: 'add', val: 10 });   // Cueillette : +10 points de jauge par coup (sac de stats : MUR)
put('IMP', 0, 4, { stat: 'VOL', op: 'add', val: 0.02 });
put('IMP', 1, 0, ['BK_MEL_FWHIFF', 50]);       // Souffle court
put('IMP', 1, 1, ['BK_MEL_FAIR', 20]);         // Jambes vives
put('IMP', 1, 2, ['BK_MEL_FN', 5]);            // Cinquieme coup
put('IMP', 1, 3, ['BK_MEL_FAIRPTS', 15]);      // Prise aerienne
put('IMP', 1, 4, { stat: 'VOL', op: 'add', val: 0.04 });
put('IMP', 2, 0, ['BK_MEL_FSOIN', 8]);         // Souffle acrobate
put('IMP', 2, 1, ['BK_MEL_FCHAIN', 20]);       // Combo enchaine
put('IMP', 2, 2, ['BK_MEL_FGAP', 16]);         // Rafale
put('IMP', 2, 3, ['BK_MEL_FN', 6]);            // Sixieme coup
put('IMP', 2, 4, ['BK_MEL_FPTS', 40]);         // Butin de combo
put('IMP', 3, 0, ['BK_MEL_FFREE', 3]);         // Poings infatigables
put('IMP', 3, 1, ['BK_MEL_FFREN', 10, 5]);     // Frenesie
put('IMP', 3, 2, ['BK_MEL_FN', 8]);            // Huit coups
put('IMP', 3, 3, ['BK_MEL_KPTS', 100]);        // Pillage de poings
put('IMP', 3, 4, { stat: 'VOL', op: 'add', val: 0.10 });
module.exports = {
  steps,
  caps: {
    'LAN>RAIL': ['BK_MEL_CAP_RAIL', 1],
    'LAN>ROQ': ['BK_MEL_CAP_BOOM', 4],
    'LAN>SHO': ['BK_MEL_CAP_PLOMB', 6],
    'IMP>RAIL': ['BK_MEL_CAP_UPRAIL', 1],
    'IMP>ROQ': ['BK_MEL_CAP_ROQ', 60],
    'IMP>SHO': ['BK_MEL_CAP_PLOMB3', 6],
  },
  fus: {},
};
