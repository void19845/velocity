// Formes fusionnees (armes hybrides) : src/solo/boutique_hybrides.c.
// FISSION et DOUBLE DETENTE n'ont pas encore leur effet passif de code : BK_HYB = fusion achetable dont le seul effet est,
// pour l'instant, la forme fusionnee (jauge, touche, arme hybride de 15 s). Quand le passif est code, passer a son genre.
module.exports = {
  steps: {}, caps: {},
  fus: {
    'FISSION': ['BK_HYB', 0],
    'DOUBLE DÉTENTE': ['BK_HYB', 0],
  },
};
