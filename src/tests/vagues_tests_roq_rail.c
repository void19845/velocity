/* VELOCITY - tests du mode VAGUES : zone reservee a la migration des Roquettes et du Railgun vers le sac de stats
   Morceau inclus par vagues_tests.c (avant vagues_tests), ne se compile pas seul.

   Y ajouter les NOUVELLES lignes de test des etapes migrees (une ligne par etape ou par groupe d'etapes : valeur lue dans le sac, niveau 1 et niveau 3,
   comparee a l'ancienne formule). Modeles : le bloc "SAC DE STATS" de vagues_tests.c (decor : vg_test_begin, rl_t_setup, vg_test_enemy ; formules
   a la main : st_act + st_fold). La fonction est appelee a la fin de ce bloc ; elle doit laisser l'etat propre (shOwn, shLvl a zero). Les lignes
   d'avant migration (blocs "boutique ROQUETTES", "boutique RAILGUN", "niveaux") doivent rester identiques a la reference. */
static void vg_tests_roq_rail(FILE *f) {
    (void)f;
}
