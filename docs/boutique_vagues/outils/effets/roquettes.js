// Effets codes pour le groupe 'roquettes'. Cle des etapes : "GRAINE:Nom" ; capstones "GRAINE>PARTENAIRE" ; fusions "NOM".
// Genre BK_ROQ : a = identifiant RQ_* (src/solo/boutique_roquettes.c), l'effet est code la-bas.
module.exports = {
  steps: {
    'ROQ:Charge creuse': { stat: 'DEG', op: 'mul', val: 1.10 },     // etape-stat (sac de stats) : degats +10 %, lus par vg_dmg_mul
    'ROQ:Soute': ['BK_ROQ',1],
    'ROQ:Propulseur': ['BK_ROQ',2],
    'ROQ:Rayon': ['BK_ROQ',3],
    'ROQ:Ogive large': ['BK_ROQ',4],
    'ROQ:Combustible': ['BK_ROQ',5],
    'ROQ:Fusée de proximité': ['BK_ROQ',6],
    'ROQ:Double départ': ['BK_ROQ',7],
    'ROQ:Souffle plein': ['BK_ROQ',8],
    'ROQ:Impact direct': ['BK_ROQ',9],
    'ROQ:Cratère vital': ['BK_ROQ',10],
    'ROQ:Récupération': ['BK_ROQ',11],
    'ROQ:Éclats': ['BK_FX','FX_ECLATS'],
    'ROQ:Grand souffle': ['BK_ROQ',13],
    'ROQ:Cœur du souffle': ['BK_ROQ',14],
    'ROQ:Détonation en chaîne': ['BK_FX','FX_ONDE'],
    'ROQ:Salve': ['BK_ROQ',16],
    'ROQ:Trajectoire courbe': ['BK_ROQ',17],
    'ROQ:Grappe': ['BK_ROQ',18],
    'ROQ:Roquette lourde': { stat: 'DEG', op: 'mul', val: 1.40 },   // degats +40 % (sac de stats) ; la penalite de vitesse -10 % reste dans roq_setup (a migrer avec la famille : VIT x0,90)
  },
  caps: {
    'ROQ>RAIL': ['BK_ROQ', 1],
    'ROQ>SHO': ['BK_ROQ', 1],
    'ROQ>LAN': ['BK_ROQ', 1],
    'ROQ>IMP': ['BK_ROQ', 1],
    'ROQ>PIE': ['BK_ROQ', 1],
    'ROQ>AIO': ['BK_ROQ', 1],
  },
  fus: {
    "SALVE D’ARTILLERIE": ['BK_ROQ', 1],
    "LANCE-ROQUETTES": ['BK_ROQ', 1],
    "ACROBATIE EXPLOSIVE": ['BK_ROQ', 1],
    "FEU D’ARTIFICE": ['BK_ROQ', 1],
    "TAPIS DE BOMBES": ['BK_ROQ', 1],
  },
};
