// Etapes de COMPETENCE remplacees dans la v2 (les pools des armes a distance et de melee sont dans pools_v3.js).
// Format : SEED: { profondeur (1 a 4): { indice (0 a 4): [nom, effet] } }
// Raisons : le proprietaire juge GEL, BRULURE et CONTAGION mauvaises "tout court" ; les variantes de feu partent avec elles.
// PIE et RIC suivent aussi le patch d'equilibrage (Piece : 2 charges ; Ricochet : duree).
// Une etape de pool ne depend jamais d'une autre graine. Les noms font 20 caracteres au plus.

module.exports = {
  PIE: {
    1: {
      2: ['Recharge', 'délai de recharge d’une charge -10 %'],
      3: ['Réserve', 'charges de 2 à 3'],
    },
    2: { 0: ['Pièce recyclée', 'un ricochet qui tue rend 1 charge de pièce'] },
    3: {
      1: ['Prime de ricochet', 'le 1er ricochet de chaque pièce fait x2 au lieu de x1,5'],
      2: ['Pièce paratonnerre', 'les orbes ennemies à 6 m dévient vers tes pièces'],
    },
  },
  MAR: {
    2: { 1: ['Marque tenace', 'la marque tient 3 s de plus hors de ta vue'] },
    3: { 3: ['Prime de chasse', 'un kill sur cible marquée rend 1 charge de marque'] },
    4: { 0: ['Meute', 'chaque kill marqué : +10 % de dégâts 5 s (5 max)'] },
  },
  CHA: {
    2: { 4: ['Relance', 'un tir chargé qui tue relance la fenêtre (+1 s)'] },
    3: {
      1: ['Charge cinétique', 'la touche chargée gagne +1 % par m/s de vitesse'],
      2: ['Charge de secours', 'perdre la charge rend 50 % de sa recharge'],
    },
  },
  RIC: {
    1: {
      1: ['Durée', '+1 s de rebonds sans limite'],
      2: ['Cadence rebondie', 'pendant la compétence : délai entre tirs -10 %'],
    },
    2: { 1: ['Rebond vorace', 'chaque kill pendant la compétence : +0,5 s'] },
    3: {
      2: ['Munition boomerang', 'un kill par un tir rebondissant rend sa munition'],
      4: ['Rebond fatal', 'un projectile ayant rebondi 4 fois ou plus : x1,5'],
    },
    4: { 3: ['Tir sans délai', 'pendant la compétence : aucun délai entre tes tirs'] },
  },
  AIO: {
    1: { 0: ['Salve lourde', '+10 % de dégâts sur les tirs de la salve'] },
    2: { 4: ['Prime de salve', 'un kill pendant la salve réduit la recharge de 2 s'] },
    4: { 1: ['Salve complète', 'salve avec chargeur plein : +30 % et +1 tir'] },
  },
  MIR: {
    3: {
      1: ['Leurre blindé', 'le leurre absorbe 3 tirs avant de disparaître'],
      2: ['Permutation', 'réappuie pour échanger ta place avec le leurre'],
    },
    4: { 1: ['Illusion totale', 'tant que le leurre existe, les ennemis t’ignorent'] },
  },
  AIM: {
    1: { 0: ['Verrou lourd', '+10 % de dégâts sur la cible verrouillée'] },
    3: {
      2: ['Œil de lynx', 'la cible verrouillée est visible à travers les murs'],
      3: ['Rafale guidée', 'chaque touche verrouillée : délai de tir -0,1 s'],
    },
  },
};
