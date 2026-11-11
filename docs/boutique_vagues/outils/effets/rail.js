// Effets codes pour le groupe 'rail' (un fichier par sous-agent). Cle des etapes : "GRAINE:Nom" ; capstones "GRAINE>PARTENAIRE" ; fusions "NOM".
// Genres : voir effets_c.js. Genre BK_RAIL : a = RL_* (enum dans src/solo/ameliorations.c, code dans src/solo/boutique_rail.c).
// Non codes (raison) : fusions FISSION, DOUBLE DETENTE, BILLARD.
module.exports = {
  steps: {
    'RAIL:Calibre': { stat: 'DEG', op: 'mul', val: 1.10 },          // etape-stat (sac de stats) : degats +10 %, lus par vg_dmg_mul
    'RAIL:Réserve': ['BK_RAIL', 'RL_RESERVE'],
    'RAIL:Rail large': ['BK_RAIL', 'RL_LARGE'],
    'RAIL:Visée courbe': ['BK_RAIL', 'RL_COURBE'],
    'RAIL:Éclat d’impact': ['BK_RAIL', 'RL_IMPACT'],
    'RAIL:Satellite': ['BK_FX', 'FX_SATELLITE'],
    'RAIL:Fourche': ['BK_FX', 'FX_FOURCHE'],
    'RAIL:Surtension': ['BK_RAIL', 'RL_SURTENSION'],
    'RAIL:Lunette': ['BK_RAIL', 'RL_LUNETTE'],
    'RAIL:Sang-froid': ['BK_RAIL', 'RL_SANG'],
    'RAIL:Doublé': ['BK_RAIL', 'RL_DOUBLE'],
    'RAIL:Calibre lourd': ['BK_RAIL', 'RL_LOURD'],
    'RAIL:Éclats': ['BK_FX', 'FX_ECLATS'],
    'RAIL:Tir explosif': ['BK_RAIL', 'RL_EXPLOSIF'],
    'RAIL:Rail jumeau': ['BK_RAIL', 'RL_JUMEAU'],
    'RAIL:Rail colossal': ['BK_RAIL', 'RL_COLOSSAL'],
    'RAIL:Perce-mur': ['BK_RAIL', 'RL_PERCE'],
    'RAIL:Tir guidé': ['BK_RAIL', 'RL_GUIDE'],
    'RAIL:Rail tendu': ['BK_RAIL', 'RL_TENDU'],
    'RAIL:Tir d’élite': ['BK_RAIL', 'RL_ELITE'],
  },
  caps: {
    'RAIL>ROQ': ['BK_RAIL', 'RL_C_ORBITAL'],
    'RAIL>LAN': ['BK_RAIL', 'RL_C_JOUTE'],
    'RAIL>IMP': ['BK_RAIL', 'RL_C_AGILE'],
    'RAIL>PIE': ['BK_RAIL', 'RL_C_BANQUE'],
    'RAIL>AIO': ['BK_RAIL', 'RL_C_MULTI'],
    'RAIL>SHO': ['BK_RAIL', 'RL_C_OUVERTURE'],     // Rail d'ouverture : rlOpenT (ici), lu par fire_shotgun via vg_rail_open_mul
  },
  fus: {
    'CHEVALIER': ['BK_RAIL', 'RL_F_CHEVALIER'],
    'COMBO AÉRIEN': ['BK_RAIL', 'RL_F_AERIEN'],
    'FUSILLADE': ['BK_RAIL', 'RL_F_FUSILLADE'],
  },
};
