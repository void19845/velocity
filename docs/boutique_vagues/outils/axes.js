// Axes des arbres de modificateur (liste du proprietaire) : ce que les etapes d'un pool peuvent modifier selon le type d'arme.
//   Armes a distance : taille du projectile, vitesse, zone d'effet, portee, nombre de munitions, nombre de projectiles,
//                      degats, effets de suivi des projectiles.
//   Melee            : cooldown, vitesse d'animation, longueur de combo, munition rendue, lifesteal.
//   Competences      : pas de liste donnee, elles gardent leurs pools (charges, duree, recharge, multiplicateur, portee).
// SOI = soin : l'etape de soin de la profondeur 3 des armes a distance (la melee a le lifesteal VOL).

const AXES = {
  TAI: 'taille du projectile', VIT: 'vitesse', ZON: 'zone d’effet', POR: 'portée', MUN: 'nombre de munitions',
  NBP: 'nombre de projectiles', DEG: 'dégâts', SUI: 'suivi', SOI: 'soin',
  CD: 'cooldown', ANI: 'vitesse d’animation', COM: 'longueur de combo', MUR: 'munition rendue', VOL: 'lifesteal',
};
const AXES_DISTANCE = ['TAI', 'VIT', 'ZON', 'POR', 'MUN', 'NBP', 'DEG', 'SUI'];
const AXES_MELEE = ['CD', 'ANI', 'COM', 'MUR', 'VOL'];

// Axes qui n'ont pas de sens pour une arme (rien a modifier) : le controle ne les exige pas.
const SANS_OBJET = {
  RAIL: ['VIT'],              // tir instantane
  TES: ['VIT'],               // faisceau continu
  SCI: ['MUN'],               // la scie est sa propre munition
};

// Ce qu'un partenaire attend des stats de la graine, selon le type de la graine.
// Graine a distance : poids sur les axes a distance.
const DEPUIS_DISTANCE = {
  RAIL: { DEG: 3, POR: 1, SUI: 1, TAI: 1, NBP: 1 },
  ROQ:  { ZON: 3, DEG: 2, MUN: 2, NBP: 1 },
  SHO:  { NBP: 3, DEG: 2, POR: 1, ZON: 1 },
  ELA:  { VIT: 3, DEG: 2, POR: 1 },
  SCI:  { VIT: 2, SUI: 2, TAI: 1, POR: 1 },
  VEN:  { ZON: 3, MUN: 2, DEG: 1, TAI: 1 },
  TES:  { NBP: 2, POR: 2, MUN: 2, SUI: 1 },
  EPE:  { DEG: 3, MUN: 1, VIT: 1 },
  ENC:  { ZON: 3, DEG: 2, NBP: 1 },
  LAN:  { DEG: 2, VIT: 2, POR: 1 },
  YOY:  { NBP: 2, DEG: 2, SUI: 1 },
  IMP:  { VIT: 2, SUI: 1, NBP: 1, DEG: 1 },
  EGI:  { DEG: 2, POR: 2, MUN: 1 },
  PIE:  { SUI: 3, NBP: 2, POR: 1, VIT: 1, TAI: 1 },
  MAR:  { SUI: 3, DEG: 2, POR: 1 },
  CHA:  { DEG: 3, TAI: 1, NBP: 1 },
  RIC:  { VIT: 3, POR: 2, NBP: 1, TAI: 1 },
  AIO:  { MUN: 3, NBP: 3, DEG: 1 },
  MIR:  { NBP: 2, SUI: 1, TAI: 1 },
  AIM:  { SUI: 3, POR: 2, TAI: 2 },
};
// Graine de melee : poids sur les axes de melee (la munition rendue nourrit les armes a distance).
const DEPUIS_MELEE = {
  RAIL: { MUR: 3, CD: 1, VOL: 1 },
  ROQ:  { MUR: 3, ANI: 1, CD: 1 },
  SHO:  { MUR: 3, COM: 2, ANI: 1 },
  ELA:  { ANI: 2, MUR: 2, CD: 1 },
  SCI:  { MUR: 2, CD: 2, COM: 1 },
  VEN:  { MUR: 3, CD: 1, COM: 1 },
  TES:  { MUR: 3, COM: 2, ANI: 1 },
  PIE:  { ANI: 2, CD: 2, COM: 1, MUR: 1 },
  MAR:  { COM: 2, CD: 2, VOL: 1 },
  CHA:  { CD: 3, VOL: 1, ANI: 1 },
  RIC:  { COM: 2, ANI: 2, CD: 1 },
  AIO:  { COM: 3, MUR: 2, CD: 1 },
  MIR:  { VOL: 2, CD: 2, ANI: 1 },
  AIM:  { ANI: 3, COM: 1, CD: 1 },
};

module.exports = { AXES, AXES_DISTANCE, AXES_MELEE, SANS_OBJET, DEPUIS_DISTANCE, DEPUIS_MELEE };
