// Profils utilises pour attribuer les 4 etapes de chaque branche (les "chemins").
// Une etape porte des etiquettes (ce qu'elle fait) ; un partenaire a un profil (ce qu'il attend d'un chemin
// qui mene a lui). Le score d'un chemin = somme des poids du profil du partenaire sur les etiquettes des 4 etapes.

const LIBELLES = {
  dmg: 'dégâts', rate: 'cadence', ammo: 'munitions', chain: 'chaîne de tirs', heal: 'soin', pierce: 'traversée',
  explo: 'explosions', force: 'déplacement des cibles', air: 'jeu aérien', speed: 'vitesse', parry: 'parry',
  bounce: 'rebonds', seek: 'guidage', trap: 'pièges', elec: 'arcs', decoy: 'leurres', head: 'précision',
  combo: 'enchaînements',
};

// Mots-cles (texte normalise : minuscules, sans accents) -> etiquette
const RX = {
  dmg: /\+\d+ ?% de degats|degats ?\+|fait x\d|\bx\d(,\d)?\b|execution|one-shot|\+\d+ ?% (sur|contre)|\+\d+ ?%/,
  rate: /recharge|cadence|delai|rechargement|rafale|vitesse de lancer|armement|\bvif|surcharge/,
  ammo: /munition|jauge|reserve|charges?\b|batterie|soute|poche|stock/,
  chain: /chaine (de|vive|pleine)|d.affilee|consecutif/,
  heal: /\bsoin\b/,
  pierce: /traverse|perfor|perce/,
  explo: /explos|souffle|rayon|detonation|\bonde\b|impact|fracture|cratere|grappe|nuke/,
  force: /repouss|recul|attire|traction|vortex|aspire|ressac|projette|\btire\b|\btiree?s?\b|entrave|souleve|propuls|lasso|attrait/,
  air: /en l.air|aerien|\bsaut|chute|suspen|uppercut|retombee|pique|plong|zenith|punch-jump/,
  speed: /vitesse|dash|course|m\/s|sprint|allure|croisiere|cinetique/,
  parry: /parry|\bpare|bouclier|garde|renvoi|renvoye|miroir|stase|bloque|contre/,
  bounce: /piece|rebond|ricoch|boucle/,
  seek: /chercheur|guid|suit |marque|verrou|cible|eclats|fourche|satellite|trajectoire|devie|\bvise|aimant|traque|chasse/,
  trap: /\bmine|piege|colle\b|pose\b/,
  elec: /\barc\b|eclair|tesla|relais|decharge|foudre|orage|conduct/,
  decoy: /leurre|spectre|clone|illusion|appat|permutation/,
  head: /tete|lunette|precision|viseur|crane|elite/,
  combo: /combo|enchain|crescendo|serie|tournoi|frenesie|meute/,
};

function norm(s) {
  return s.normalize('NFD').replace(/[̀-ͯ]/g, '').toLowerCase().replace(/[’']/g, "'");
}

// Corrections a la main : "SEED:profondeur:indice" -> etiquettes (remplace le calcul automatique)
const SURCHARGES = {
  // RAIL
  'RAIL:1:0': ['dmg'], 'RAIL:1:1': ['rate'], 'RAIL:1:2': ['ammo'], 'RAIL:1:3': ['head', 'dmg'], 'RAIL:1:4': ['chain'],
  'RAIL:2:0': ['speed', 'air'], 'RAIL:2:1': ['seek'], 'RAIL:2:2': ['seek'], 'RAIL:2:3': ['pierce'], 'RAIL:2:4': ['chain', 'head'],
  'RAIL:3:0': ['heal', 'head'], 'RAIL:3:1': ['pierce', 'ammo'], 'RAIL:3:2': ['seek'], 'RAIL:3:3': ['pierce', 'dmg'], 'RAIL:3:4': ['chain'],
  'RAIL:4:0': ['rate'], 'RAIL:4:1': ['pierce'], 'RAIL:4:2': ['chain', 'dmg'], 'RAIL:4:3': ['dmg', 'rate'], 'RAIL:4:4': ['explo'],
  // ROQ
  'ROQ:2:0': ['explo', 'seek'], 'ROQ:3:4': ['parry', 'explo'],
  // SHO
  'SHO:2:1': ['speed', 'air'], 'SHO:2:4': ['force', 'explo'], 'SHO:3:4': ['ammo', 'combo'], 'SHO:4:1': ['seek', 'combo'],
  // ELA
  'ELA:3:3': ['speed'], 'ELA:3:4': ['speed', 'combo'], 'ELA:4:1': ['speed', 'dmg'], 'ELA:4:3': ['speed'],
  // SCI
  'SCI:2:0': ['trap', 'dmg'], 'SCI:2:2': ['pierce', 'dmg'], 'SCI:3:4': ['combo', 'dmg'],
  // VEN
  'VEN:2:2': ['ammo', 'trap'], 'VEN:3:1': ['air', 'speed', 'trap'], 'VEN:3:4': ['trap', 'ammo'],
  // TES
  'TES:2:0': ['elec', 'dmg'], 'TES:3:2': ['ammo', 'elec'], 'TES:3:4': ['dmg', 'seek'],
  // EPE
  'EPE:3:4': ['parry', 'dmg'],
  // ENC
  'ENC:3:2': ['dmg', 'explo'], 'ENC:3:3': ['force', 'air'], 'ENC:4:2': ['speed', 'air'], 'ENC:4:3': ['rate', 'combo'],
  // LAN
  'LAN:3:1': ['force', 'explo'], 'LAN:3:3': ['combo', 'dmg'], 'LAN:4:0': ['dmg', 'pierce'],
  // YOY
  'YOY:3:2': ['force', 'dmg'], 'YOY:3:3': ['force', 'dmg'], 'YOY:4:4': ['force', 'speed', 'ammo'],
  // IMP
  'IMP:3:2': ['air', 'dmg'], 'IMP:3:3': ['air', 'combo'], 'IMP:4:1': ['air', 'dmg'], 'IMP:4:2': ['combo', 'rate'],
  // EGI
  'EGI:3:2': ['parry', 'dmg'], 'EGI:3:3': ['parry', 'ammo'],
  // PIE
  'PIE:1:2': ['rate'], 'PIE:1:3': ['ammo'], 'PIE:2:0': ['bounce', 'ammo'], 'PIE:3:1': ['bounce', 'dmg'], 'PIE:3:2': ['bounce', 'parry'],
  // MAR
  'MAR:2:1': ['seek'], 'MAR:3:3': ['seek', 'ammo'], 'MAR:4:0': ['combo', 'dmg'],
  // CHA
  'CHA:2:4': ['combo', 'rate'], 'CHA:3:1': ['speed', 'dmg'], 'CHA:3:2': ['ammo', 'rate'],
  // RIC
  'RIC:1:1': ['bounce', 'rate'], 'RIC:1:2': ['rate'], 'RIC:2:1': ['bounce', 'combo'], 'RIC:3:2': ['ammo', 'bounce'],
  'RIC:3:4': ['bounce', 'dmg'], 'RIC:4:3': ['rate', 'combo'],
  // AIO
  'AIO:1:0': ['dmg'], 'AIO:2:4': ['rate', 'combo'], 'AIO:4:1': ['ammo', 'dmg'],
  // MIR
  'MIR:3:1': ['decoy', 'parry'], 'MIR:3:2': ['decoy', 'speed'], 'MIR:4:1': ['decoy'],
  // AIM
  'AIM:1:0': ['dmg', 'seek'], 'AIM:3:2': ['seek', 'head'], 'AIM:3:3': ['rate', 'seek'],
};

// Etapes de la v1 dont le calcul automatique est faux ou vide (relues une par une)
Object.assign(SURCHARGES, {
  // ROQ
  'ROQ:1:3': ['head', 'rate'], 'ROQ:1:4': ['air', 'speed'], 'ROQ:2:4': ['air', 'speed'], 'ROQ:4:1': ['dmg', 'combo'],
  'ROQ:4:2': ['force', 'explo'], 'ROQ:4:4': ['explo', 'seek'], 'ROQ:1:1': ['force', 'dmg'],
  // SHO
  'SHO:1:0': ['dmg', 'pierce'], 'SHO:1:1': ['speed', 'air'], 'SHO:1:3': ['ammo'], 'SHO:1:4': ['head'], 'SHO:2:2': ['head', 'speed'],
  'SHO:3:2': ['combo', 'explo'],
  // ELA
  'ELA:1:1': ['speed'], 'ELA:1:3': ['rate'], 'ELA:1:4': ['head', 'speed'], 'ELA:2:4': ['force', 'speed'], 'ELA:3:0': ['heal', 'speed'],
  // SCI
  'SCI:1:0': ['dmg'], 'SCI:1:2': ['rate'], 'SCI:1:3': ['speed', 'head'], 'SCI:1:4': ['speed', 'rate'], 'SCI:2:3': ['parry', 'dmg'],
  'SCI:4:1': ['pierce'], 'SCI:4:2': ['combo', 'dmg'], 'SCI:4:3': ['combo', 'rate'],
  // VEN
  'VEN:1:3': ['trap', 'rate'], 'VEN:4:2': ['trap', 'dmg'], 'VEN:4:0': ['trap', 'explo'], 'VEN:2:4': ['explo'],
  // TES
  'TES:1:1': ['ammo'], 'TES:1:2': ['head'], 'TES:1:4': ['head', 'seek'], 'TES:4:1': ['explo', 'elec'], 'TES:4:0': ['dmg', 'combo'],
  // EPE
  'EPE:1:1': ['dmg'], 'EPE:1:2': ['rate'], 'EPE:3:3': ['dmg'], 'EPE:3:2': ['parry', 'force'], 'EPE:4:0': ['parry', 'force'],
  // ENC
  'ENC:1:2': ['force'], 'ENC:1:3': ['air', 'speed'], 'ENC:1:4': ['rate'], 'ENC:2:4': ['combo', 'explo'], 'ENC:4:4': ['force'],
  'ENC:3:0': ['heal', 'explo'],
  // LAN
  'LAN:1:1': ['dmg'], 'LAN:1:2': ['speed'], 'LAN:1:3': ['rate'], 'LAN:1:4': ['speed', 'rate'], 'LAN:2:3': ['force'], 'LAN:2:4': ['speed', 'force'],
  'LAN:4:1': ['combo', 'rate'], 'LAN:4:3': ['pierce'], 'LAN:4:4': ['speed', 'dmg'],
  // YOY
  'YOY:1:1': ['force'], 'YOY:1:2': ['force', 'dmg'], 'YOY:1:3': ['force'], 'YOY:1:4': ['rate'], 'YOY:2:2': ['combo', 'rate'],
  'YOY:2:4': ['force'], 'YOY:3:0': ['heal', 'force'], 'YOY:4:1': ['force', 'explo'], 'YOY:4:2': ['force'], 'YOY:4:3': ['force'],
  // IMP
  'IMP:1:0': ['combo', 'explo'], 'IMP:1:1': ['rate', 'combo'], 'IMP:2:4': ['air', 'speed'], 'IMP:4:3': ['explo', 'combo'], 'IMP:4:4': ['air', 'speed'],
  // EGI
  'EGI:1:1': ['heal', 'ammo'], 'EGI:2:4': ['heal', 'parry'], 'EGI:1:4': ['dmg', 'force'], 'EGI:2:3': ['force', 'parry'], 'EGI:4:1': ['parry', 'explo'],
  // PIE
  'PIE:4:1': ['bounce', 'dmg'],
  // MAR
  'MAR:1:0': ['seek'], 'MAR:1:1': ['seek', 'head'], 'MAR:1:4': ['rate'], 'MAR:2:2': ['force'],
  // CHA
  'CHA:1:0': ['force', 'dmg'], 'CHA:1:1': ['rate'], 'CHA:1:2': ['rate'], 'CHA:2:3': ['parry'], 'CHA:4:0': ['rate'], 'CHA:4:2': ['dmg'], 'CHA:4:4': ['combo', 'dmg'],
  // RIC
  'RIC:1:0': ['elec'], 'RIC:1:1': ['bounce'], 'RIC:4:1': ['bounce', 'dmg'],
  // AIO
  'AIO:1:2': ['seek'], 'AIO:2:2': ['head'], 'AIO:2:3': ['ammo'], 'AIO:3:0': ['heal', 'ammo'], 'AIO:4:2': ['combo', 'ammo'],
  // MIR
  'MIR:1:4': ['decoy', 'head'], 'MIR:2:2': ['force'],
  // AIM
  'AIM:1:1': ['seek'], 'AIM:1:2': ['seek', 'head'], 'AIM:1:4': ['seek'], 'AIM:2:4': ['seek'],
  // etapes ajoutees pour que le pool ne depende d'aucune autre graine
  'SHO:4:2': ['speed', 'explo'], 'TES:2:4': ['elec', 'seek'], 'YOY:2:3': ['force', 'air'], 'EGI:4:4': ['parry', 'dmg'], 'ENC:2:4': ['air', 'explo'],
});

function etiquettes(seed, d, i, st) {
  const k = seed + ':' + (d + 1) + ':' + i;
  if (SURCHARGES[k]) return SURCHARGES[k];
  const t = norm(st.n + ' ' + st.e);
  const out = [];
  for (const [tag, rx] of Object.entries(RX)) if (rx.test(t)) out.push(tag);
  return out;
}

// Profil de chaque partenaire : ce qu'un chemin qui mene a lui doit apporter.
const PROFILS = {
  RAIL: { dmg: 2, pierce: 3, head: 2, chain: 2, ammo: 1, seek: 1 },
  ROQ:  { explo: 3, force: 2, air: 1, speed: 1, ammo: 2, seek: 1 },
  SHO:  { force: 2, air: 1, rate: 1, explo: 1, dmg: 2, speed: 1, ammo: 1 },
  ELA:  { speed: 3, air: 1, dmg: 1, combo: 1 },
  SCI:  { bounce: 2, pierce: 1, seek: 2, rate: 1, combo: 2, speed: 1 },
  VEN:  { trap: 3, explo: 2, ammo: 1, force: 1 },
  TES:  { elec: 3, rate: 2, combo: 1, ammo: 2, chain: 1 },
  EPE:  { parry: 3, speed: 1, combo: 1, dmg: 1, heal: 1 },
  ENC:  { force: 3, air: 2, explo: 2, dmg: 1 },
  LAN:  { speed: 3, force: 2, dmg: 1, pierce: 1, combo: 1 },
  YOY:  { force: 3, seek: 1, combo: 1, speed: 1 },
  IMP:  { air: 3, speed: 1, combo: 2, rate: 1 },
  EGI:  { parry: 3, heal: 1, ammo: 1 },
  PIE:  { bounce: 3, seek: 2, ammo: 1 },
  MAR:  { seek: 3, head: 1, dmg: 1 },
  CHA:  { dmg: 3, rate: 1, speed: 1, combo: 1 },
  RIC:  { bounce: 3, pierce: 1, seek: 1, rate: 1 },
  AIO:  { ammo: 3, rate: 2, dmg: 1, combo: 1 },
  MIR:  { decoy: 3, trap: 1, seek: 1, parry: 1 },
  AIM:  { seek: 3, head: 2, dmg: 1, rate: 1 },
};

module.exports = { LIBELLES, etiquettes, PROFILS, norm };
