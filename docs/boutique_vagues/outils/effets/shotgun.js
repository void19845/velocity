// Effets codes pour le groupe 'shotgun'. Cle des etapes : "GRAINE:Nom" ; capstones "GRAINE>PARTENAIRE" ; fusions "NOM".
// Deux formats d'etape (voir effets_c.js) :
//  - etape-STAT : { stat, op, val, cond?, par?, unit?, grp?, fixed? } (ou un tableau de ces objets) : une ligne de la table
//    shMods du sac de stats (src/solo/boutique_stats.c). AUCUN code C a ecrire : fire_shotgun lit deja st(SH_SHO, STAT_x, base).
//  - effet code a la main : [genre, a, b]. Genre BK_SHO : a = SHE_* (src/solo/boutique_shotgun.c), b = parametre ; ce sont les
//    evenements et comportements (Rafale, Peau de plomb, Souffle de culasse, Coup double) et les capstones.
// Les 14 etapes-stat du Shotgun (ordre du pool) :
//   Poing de plomb, Bout portant : DEG x1,10 sous 5 m, x1,25 sous 3 m (condition near : lus par vg_dmg_mul a chaque touche)
//   Chevrotine : PERTE x0,90 (la perte de degats a distance baisse de 10 %)     Pompe vive : CD x0,85 (delai entre deux salves)
//   Gros plomb : TAI x1,15                                                       Rechargement : PTK x1,20 (points de jauge par kill)
//   Gerbe large : CONE +1,5 deg  /  Cone serre : CONE -1 deg, PNEAR +5 m, PFAR +5 m  (demi-angle du cone ; courbe de perte)
//   Plombs chercheurs / traqueurs : SUI +3 / +8 deg, groupe 1 = le meilleur des deux l'emporte (ils ne s'additionnent pas)
//   Plombs en plus : NBP +2    Double canon : COUT x0,85 (cout de jauge d'un tir)    Plomb lourd : DEG x1,30
//   Pompe tendue : POR +20 m, PNEAR plancher 20 m, PFAR +20 m
module.exports = {
  steps: {
    'SHO:Poing de plomb': { stat: 'DEG', op: 'mul', val: 1.10, cond: 'near', par: 5 },
    'SHO:Chevrotine': { stat: 'PERTE', op: 'mul', val: 0.90 },
    'SHO:Pompe vive': { stat: 'CD', op: 'mul', val: 0.85 },
    'SHO:Gros plomb': { stat: 'TAI', op: 'mul', val: 1.15 },
    'SHO:Rechargement': { stat: 'PTK', op: 'mul', val: 1.20 },
    'SHO:Gerbe large': { stat: 'CONE', op: 'add', val: 1.5, unit: 'deg' },
    'SHO:Rafale': ['BK_SHO', 1, 50],
    'SHO:Plombs chercheurs': { stat: 'SUI', op: 'add', val: 3, grp: 1 },
    'SHO:Bout portant': { stat: 'DEG', op: 'mul', val: 1.25, cond: 'near', par: 3 },
    'SHO:Cône serré': [
      { stat: 'CONE', op: 'add', val: -1, unit: 'deg' },
      { stat: 'PNEAR', op: 'add', val: 5 },
      { stat: 'PFAR', op: 'add', val: 5 },
    ],
    'SHO:Peau de plomb': ['BK_SHO', 2, 8],
    'SHO:Plombs en plus': { stat: 'NBP', op: 'add', val: 2 },
    'SHO:Double canon': { stat: 'COUT', op: 'mul', val: 0.85 },
    'SHO:Souffle de culasse': ['BK_SHO', 3, 1],
    'SHO:Éclats': ['BK_FX', 'FX_ECLATS'],
    'SHO:Coup double': ['BK_SHO', 4, 50],
    'SHO:Plomb lourd': { stat: 'DEG', op: 'mul', val: 1.30 },
    'SHO:Détonation': ['BK_FX', 'FX_ONDE'],
    'SHO:Pompe tendue': [
      { stat: 'POR', op: 'add', val: 20 },
      { stat: 'PNEAR', op: 'max', val: 20 },
      { stat: 'PFAR', op: 'add', val: 20 },
    ],
    'SHO:Plombs traqueurs': { stat: 'SUI', op: 'add', val: 8, grp: 1 },
  },
  caps: {
    'SHO>ROQ': ['BK_SHO', 20],
    'SHO>LAN': ['BK_SHO', 21],
    'SHO>RAIL': ['BK_SHO', 22],
    'SHO>IMP': ['BK_SHO', 23],
    'SHO>PIE': ['BK_SHO', 24],
    'SHO>AIO': ['BK_SHO', 25],                     // Shotgun total : SHE_CAP_AIO, lu par cp_allin_extra (6 salves)
  },
  fus: {},
};
