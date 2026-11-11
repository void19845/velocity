/* GENERE par docs/boutique_vagues/outils/export_c.js depuis arbre_v2.json : ne pas modifier a la main.
   Boutique du mode VAGUES : 7 graines (Railgun, Roquettes, Shotgun, Lance, Impulsion, Piece, All in One). */

static const ShStep shSteps[SH_N][20] = {
    { /* RAIL */
        { "Calibre", "+10 % de degats", 1, BK_STAT, 0, 1 },
        { "Reserve", "+20 % de points de jauge par kill", 1, BK_RAIL, RL_RESERVE, 0 },
        { "Rail large", "le tir est 15 % plus large", 1, BK_RAIL, RL_LARGE, 0 },
        { "Rail tendu", "+5 % de degats par cible deja traversee (5 max)", 1, BK_RAIL, RL_TENDU, 0 },
        { "Visee courbe", "le tir se courbe de 1 deg vers l'ennemi proche", 1, BK_RAIL, RL_COURBE, 0 },
        { "Eclat d'impact", "la 1re cible touchee subit un souffle de 1,5 m (30 %)", 2, BK_RAIL, RL_IMPACT, 0 },
        { "Satellite", "chaque tir lache un eclat chercheur", 2, BK_FX, FX_SATELLITE, 0 },
        { "Fourche", "le kill repart vers 2 ennemis (25 m)", 2, BK_FX, FX_FOURCHE, 0 },
        { "Surtension", "touche a la tete : chaine +3 au lieu de +2", 2, BK_RAIL, RL_SURTENSION, 0 },
        { "Lunette", "la tete est 15 % plus large", 2, BK_RAIL, RL_LUNETTE, 0 },
        { "Sang-froid", "SOIN : +6 PV par touche a la tete", 3, BK_RAIL, RL_SANG, 0 },
        { "Double", "tuer 2 ennemis avec un meme tir rend 1 munition", 3, BK_RAIL, RL_DOUBLE, 0 },
        { "Calibre lourd", "le tir traversant : +15 % sur la cible suivante", 3, BK_RAIL, RL_LOURD, 0 },
        { "Eclats", "le kill libere 3 eclats chercheurs", 3, BK_FX, FX_ECLATS, 0 },
        { "Tir explosif", "la touche a la tete fait un souffle de 3 m (50 %)", 3, BK_RAIL, RL_EXPLOSIF, 0 },
        { "Rail jumeau", "20 % de chance de tirer deux rails", 4, BK_RAIL, RL_JUMEAU, 0 },
        { "Rail colossal", "le tir est 40 % plus large, tete comprise", 4, BK_RAIL, RL_COLOSSAL, 0 },
        { "Tir d'elite", "+50 % de degats du rail sur les elites", 4, BK_RAIL, RL_ELITE, 0 },
        { "Perce-mur", "le tir traverse les murs de moins de 1 m (70 %)", 4, BK_RAIL, RL_PERCE, 0 },
        { "Tir guide", "le tir se courbe de 8 deg vers l'ennemi proche", 4, BK_RAIL, RL_GUIDE, 0 },
    },
    { /* ROQ */
        { "Charge creuse", "+10 % de degats", 1, BK_STAT, 1, 1 },
        { "Soute", "munition maximale +1", 1, BK_ROQ, 1, 0 },
        { "Propulseur", "vitesse du projectile +10 %", 1, BK_ROQ, 2, 0 },
        { "Rayon", "rayon d'explosion +0,4 m", 1, BK_ROQ, 3, 0 },
        { "Ogive large", "roquette 20 % plus grosse : touche directe facilitee", 1, BK_ROQ, 4, 0 },
        { "Combustible", "la roquette vole 20 m de plus avant de s'eteindre", 2, BK_ROQ, 5, 0 },
        { "Fusee de proximite", "la roquette explose a 2 m d'un ennemi", 2, BK_ROQ, 6, 0 },
        { "Double depart", "10 % de chance de tirer 2 roquettes", 2, BK_ROQ, 7, 0 },
        { "Souffle plein", "degats au bord du souffle x0,3 -> x0,45", 2, BK_ROQ, 8, 0 },
        { "Impact direct", "+15 % sur la cible touchee en plein", 2, BK_ROQ, 9, 0 },
        { "Cratere vital", "SOIN : +6 PV par ennemi dans le souffle (3 max)", 3, BK_ROQ, 10, 0 },
        { "Recuperation", "un kill avec 2 ennemis dans le souffle rend 1 munition", 3, BK_ROQ, 11, 0 },
        { "Eclats", "le kill libere 3 eclats chercheurs", 3, BK_FX, FX_ECLATS, 0 },
        { "Grand souffle", "rayon d'explosion +0,8 m", 3, BK_ROQ, 13, 0 },
        { "Coeur du souffle", "degats au centre du souffle +25 %", 3, BK_ROQ, 14, 0 },
        { "Detonation en chaine", "le kill explose 3,5 m, 45 degats", 4, BK_FX, FX_ONDE, 0 },
        { "Salve", "tire 2 roquettes a 60 %", 4, BK_ROQ, 16, 0 },
        { "Trajectoire courbe", "la roquette devie vers l'ennemi proche", 4, BK_ROQ, 17, 0 },
        { "Grappe", "se divise en 3 mini-roquettes (30 %)", 4, BK_ROQ, 18, 0 },
        { "Roquette lourde", "+40 % de degats, vitesse -10 %", 4, BK_STAT, 2, 1 },
    },
    { /* SHO */
        { "Poing de plomb", "x1,1 sous 5 m", 1, BK_STAT, 3, 1 },
        { "Chevrotine", "perte de degats a distance -10 %", 1, BK_STAT, 4, 1 },
        { "Pompe vive", "delai entre deux salves -15 %", 1, BK_STAT, 5, 1 },
        { "Gros plomb", "taille des plombs +15 %", 1, BK_STAT, 6, 1 },
        { "Rechargement", "+20 % de points de jauge par kill", 1, BK_STAT, 7, 1 },
        { "Gerbe large", "cone +1,5 deg : la salve touche plus d'ennemis", 2, BK_STAT, 8, 1 },
        { "Rafale", "deux salves a 0,15 s d'ecart, 50 %", 2, BK_SHO, 1, 50 },
        { "Plombs chercheurs", "les plombs se courbent de 3 deg vers la cible", 2, BK_STAT, 9, 1 },
        { "Bout portant", "sous 3 m : x1,25", 2, BK_STAT, 10, 1 },
        { "Cone serre", "cone -1 deg, portee utile +5 m", 2, BK_STAT, 11, 3 },
        { "Peau de plomb", "SOIN : +8 PV si 6 plombs ou plus touchent", 3, BK_SHO, 2, 8 },
        { "Plombs en plus", "+2 plombs par salve", 3, BK_STAT, 14, 1 },
        { "Double canon", "cout de jauge -15 %", 3, BK_STAT, 15, 1 },
        { "Souffle de culasse", "le recul projette une onde derriere toi (3 m, 40)", 3, BK_SHO, 3, 1 },
        { "Eclats", "le kill libere 3 eclats chercheurs", 3, BK_FX, FX_ECLATS, 0 },
        { "Coup double", "un kill lache une 2e salve gratuite a 50 %", 4, BK_SHO, 4, 50 },
        { "Plomb lourd", "degats des plombs +30 %", 4, BK_STAT, 16, 1 },
        { "Detonation", "le kill explose 3,5 m, 45 degats", 4, BK_FX, FX_ONDE, 0 },
        { "Pompe tendue", "portee +20 m, aucune perte de degats avant 20 m", 4, BK_STAT, 17, 3 },
        { "Plombs traqueurs", "les plombs se courbent de 8 deg vers la cible", 4, BK_STAT, 20, 1 },
    },
    { /* LAN */
        { "Recharge", "joute reussie : -0,2 s", 1, BK_MEL_JCD, 20, 0 },
        { "Velocite", "vitesse de joute +10 %", 1, BK_MEL_VELO, 10, 0 },
        { "Second estoc", "un 2e estoc a 60 % dans la foulee", 1, BK_MEL_ESTOCS, 1, 0 },
        { "Moisson", "+10 points de jauge par estoc", 1, BK_STAT, 21, 1 },
        { "Seve de lance", "vol de vie : 2 % des degats", 1, BK_STAT, 22, 1 },
        { "Estoc vif", "recharge de l'estoc -15 %", 2, BK_MEL_ECD, 15, 0 },
        { "Lance vive", "animation de l'estoc 20 % plus rapide", 2, BK_MEL_EANI, 20, 0 },
        { "Estocs en chaine", "combo de 3 estocs", 2, BK_MEL_ESTOCS, 2, 0 },
        { "Butin de joute", "+25 points de jauge par joute reussie", 2, BK_MEL_JPTS, 25, 0 },
        { "Sang de cavalier", "vol de vie : 4 % des degats", 2, BK_STAT, 23, 1 },
        { "Armure de fer", "SOIN : +10 PV par joute reussie", 3, BK_MEL_JSOIN, 10, 0 },
        { "Releve", "un estoc dans le vide : recharge 0,5 -> 0,25 s", 3, BK_MEL_RELEVE, 25, 0 },
        { "Charge eclair", "la lance couchee est prete en 0,10 s au lieu de 0,15 s", 3, BK_MEL_TAP, 10, 0 },
        { "Joute en chaine", "une seconde joute sans recharge", 3, BK_MEL_JCHAIN, 1, 0 },
        { "Pillage d'elite", "+40 points de jauge si la joute touche une elite", 3, BK_MEL_JELITE, 40, 0 },
        { "Ruee", "aucune recharge pendant 2 s apres une joute reussie", 4, BK_MEL_RUEE, 2, 0 },
        { "Lance-eclair", "estoc et joute 35 % plus rapides", 4, BK_MEL_ECLAIR, 35, 0 },
        { "Tournoi", "chaque joute reussie d'affilee : +20 % (60 % max)", 4, BK_MEL_TOURNOI, 20, 60 },
        { "Tresor de guerre", "un kill rend 100 points de jauge de plus", 4, BK_MEL_KPTS, 100, 0 },
        { "Lance vorace", "vol de vie : 10 % des degats", 4, BK_STAT, 24, 1 },
    },
    { /* IMP */
        { "Cadence", "combo -10 % de recharge", 1, BK_MEL_FEND, 10, 0 },
        { "Poings vifs", "coups espaces de 0,20 s au lieu de 0,22 s", 1, BK_MEL_FGAP, 20, 0 },
        { "Quatrieme coup", "combo de 4 coups", 1, BK_MEL_FN, 4, 0 },
        { "Cueillette", "+10 points de jauge par coup", 1, BK_STAT, 25, 1 },
        { "Souffle de poing", "vol de vie : 2 % des degats", 1, BK_STAT, 26, 1 },
        { "Souffle court", "recharge apres un combo sans touche -50 %", 2, BK_MEL_FWHIFF, 50, 0 },
        { "Jambes vives", "animation 20 % plus rapide en l'air", 2, BK_MEL_FAIR, 20, 0 },
        { "Cinquieme coup", "combo de 5 coups", 2, BK_MEL_FN, 5, 0 },
        { "Prise aerienne", "+15 points de jauge par coup porte en l'air", 2, BK_MEL_FAIRPTS, 15, 0 },
        { "Sang vif", "vol de vie : 4 % des degats", 2, BK_STAT, 27, 1 },
        { "Souffle acrobate", "SOIN : +8 PV par uppercut touche", 3, BK_MEL_FSOIN, 8, 0 },
        { "Combo enchaine", "un combo complet reduit la recharge suivante de 0,2 s", 3, BK_MEL_FCHAIN, 20, 0 },
        { "Rafale", "coups espaces de 0,16 s", 3, BK_MEL_FGAP, 16, 0 },
        { "Sixieme coup", "combo de 6 coups", 3, BK_MEL_FN, 6, 0 },
        { "Butin de combo", "+40 points de jauge par combo complet", 3, BK_MEL_FPTS, 40, 0 },
        { "Poings infatigables", "aucune recharge pendant 3 s apres un kill", 4, BK_MEL_FFREE, 3, 0 },
        { "Frenesie", "chaque combo complet : coups 10 % plus vifs (5 max, 6 s)", 4, BK_MEL_FFREN, 10, 5 },
        { "Huit coups", "combo de 8 coups, un uppercut tous les 4", 4, BK_MEL_FN, 8, 0 },
        { "Pillage de poings", "un kill rend 100 points de jauge de plus", 4, BK_MEL_KPTS, 100, 0 },
        { "Acrobate vorace", "vol de vie : 10 % des degats", 4, BK_STAT, 28, 1 },
    },
    { /* PIE */
        { "Piece lestee", "portee de recherche +15 m", 1, BK_COMP, CP_LESTEE, 0 },
        { "Piece polie", "ricochet x1,5 -> x1,6", 1, BK_COMP, CP_POLIE, 0 },
        { "Recharge", "delai de recharge d'une charge -10 %", 1, BK_COMP, CP_RECHARGE, 0 },
        { "Reserve", "charges de 2 a 3", 1, BK_COMP, CP_RESERVE, 0 },
        { "Piece aimantee", "tes pieces derivent vers les ennemis", 1, BK_FX, FX_COIN_AIMANT, 0 },
        { "Piece recyclee", "un ricochet qui tue rend 1 charge de piece", 2, BK_COMP, CP_RECYCLEE, 0 },
        { "Eclats", "le kill libere 3 eclats chercheurs", 2, BK_FX, FX_ECLATS, 0 },
        { "Piece volante", "duree de vie +1 s", 2, BK_COMP, CP_VOLANTE, 0 },
        { "Rebond de plus", "+1 cible de ricochet", 2, BK_COMP, CP_REBOND, 0 },
        { "Aimant doux", "derive plus vite", 2, BK_COMP, CP_AIMANT_DOUX, 0 },
        { "Dividende", "SOIN : +10 PV si un ricochet touche 2 cibles", 3, BK_COMP, CP_DIVIDENDE, 0 },
        { "Prime de ricochet", "le 1er ricochet de chaque piece fait x2 au lieu de x1,5", 3, BK_COMP, CP_PRIME, 0 },
        { "Piece paratonnerre", "les orbes ennemies a 6 m devient vers tes pieces", 3, BK_COMP, CP_PARATONNERRE, 0 },
        { "Detonation", "le kill explose 3,5 m, 45 degats", 3, BK_FX, FX_ONDE, 0 },
        { "Vortex", "le kill ouvre un vortex de 1,2 s", 3, BK_FX, FX_VORTEX, 0 },
        { "Fourche", "le kill repart vers 2 ennemis (25 m)", 4, BK_FX, FX_FOURCHE, 0 },
        { "Pluie", "2 pieces en eventail", 4, BK_COMP, CP_PLUIE, 0 },
        { "Piece piegee", "explose 3 m", 4, BK_COMP, CP_PIEGEE, 0 },
        { "Ricochet pur", "a partir du 3e rebond, chaque rebond fait +25 % cumules", 4, BK_COMP, CP_PUR, 0 },
        { "Piece orbitale", "tourne autour de toi 4 s", 4, BK_COMP, CP_ORBITALE, 0 },
    },
    { /* AIO */
        { "Salve lourde", "+10 % de degats sur les tirs de la salve", 1, BK_STAT, 29, 1 },
        { "Rechargement", "de 20 a 17 s", 1, BK_COMP, CP_A_RECHARGEMENT, 0 },
        { "Salve large", "demi-angle +0,3 deg", 1, BK_COMP, CP_A_LARGE, 0 },
        { "Cadence", "delai entre tirs -30 %", 1, BK_COMP, CP_A_CADENCE, 0 },
        { "Reserve", "+1 munition comptee", 1, BK_COMP, CP_A_RESERVE, 0 },
        { "Pulsion", "competence : onde de 6 m", 2, BK_FX, FX_S_ONDE, 0 },
        { "Spectre", "le kill fait orbiter un spectre 8 s", 2, BK_FX, FX_SPECTRE, 0 },
        { "Salve precise", "dispersion -30 %", 2, BK_COMP, CP_A_PRECISE, 0 },
        { "Rechargement jauge", "rend 25 % de jauge", 2, BK_COMP, CP_A_JAUGE, 0 },
        { "Prime de salve", "un kill pendant la salve reduit la recharge de 2 s", 2, BK_COMP, CP_A_PRIME, 0 },
        { "Dernier souffle", "SOIN : +15 PV si la salve est complete", 3, BK_COMP, CP_A_DERNIER, 0 },
        { "Eclats", "le kill libere 3 eclats chercheurs", 3, BK_FX, FX_ECLATS, 0 },
        { "Fracture", "3 touches en 2 s : explosion 3 m", 3, BK_FX, FX_FRACTURE, 0 },
        { "Fourche", "le kill repart vers 2 ennemis (25 m)", 3, BK_FX, FX_FOURCHE, 0 },
        { "Vortex", "le kill ouvre un vortex de 1,2 s", 3, BK_FX, FX_VORTEX, 0 },
        { "Ressac", "le kill repousse les ennemis a 9 m", 4, BK_FX, FX_RESSAC, 0 },
        { "Salve complete", "salve avec chargeur plein : +30 % et +1 tir", 4, BK_COMP, CP_A_COMPLETE, 0 },
        { "Salve double", "2 salves successives", 4, BK_COMP, CP_A_DOUBLE, 0 },
        { "Cible unique", "focus x1,3", 4, BK_COMP, CP_A_UNIQUE, 0 },
        { "Detonation", "le kill explose 3,5 m", 4, BK_FX, FX_ONDE, 0 },
    },
};

#define SH_NMOD 30
/* seed, step, stat, op, cond, grp, flags, phase, val, par : voir StatMod (src/monde/entites.c) ; val = facteur (mul), increment (add), plancher (max), plafond (min) ou cible (to) au niveau 1 */
static const StatMod shMods[SH_NMOD] = {
    { SH_RAIL, 0, STAT_DEG, SOP_MUL, COND_NONE, 0, 0, 0, 1.1f, 0.0f },   /* Calibre */
    { SH_ROQ, 0, STAT_DEG, SOP_MUL, COND_NONE, 0, 0, 0, 1.1f, 0.0f },   /* Charge creuse */
    { SH_ROQ, 19, STAT_DEG, SOP_MUL, COND_NONE, 0, 0, 0, 1.4f, 0.0f },   /* Roquette lourde */
    { SH_SHO, 0, STAT_DEG, SOP_MUL, COND_NEAR, 0, 0, 0, 1.1f, 5.0f },   /* Poing de plomb */
    { SH_SHO, 1, STAT_PERTE, SOP_MUL, COND_NONE, 0, 0, 0, 0.9f, 0.0f },   /* Chevrotine */
    { SH_SHO, 2, STAT_CD, SOP_MUL, COND_NONE, 0, 0, 0, 0.85f, 0.0f },   /* Pompe vive */
    { SH_SHO, 3, STAT_TAI, SOP_MUL, COND_NONE, 0, 0, 0, 1.15f, 0.0f },   /* Gros plomb */
    { SH_SHO, 4, STAT_PTK, SOP_MUL, COND_NONE, 0, 0, 0, 1.2f, 0.0f },   /* Rechargement */
    { SH_SHO, 5, STAT_CONE, SOP_ADD, COND_NONE, 0, 0, 0, 1.5f * (PI / 180.0f), 0.0f },   /* Gerbe large */
    { SH_SHO, 7, STAT_SUI, SOP_ADD, COND_NONE, 1, 0, 0, 3.0f, 0.0f },   /* Plombs chercheurs */
    { SH_SHO, 8, STAT_DEG, SOP_MUL, COND_NEAR, 0, 0, 0, 1.25f, 3.0f },   /* Bout portant */
    { SH_SHO, 9, STAT_CONE, SOP_ADD, COND_NONE, 0, 0, 0, -1.0f * (PI / 180.0f), 0.0f },   /* Cone serre */
    { SH_SHO, 9, STAT_PNEAR, SOP_ADD, COND_NONE, 0, 0, 0, 5.0f, 0.0f },   /* Cone serre */
    { SH_SHO, 9, STAT_PFAR, SOP_ADD, COND_NONE, 0, 0, 0, 5.0f, 0.0f },   /* Cone serre */
    { SH_SHO, 11, STAT_NBP, SOP_ADD, COND_NONE, 0, 0, 0, 2.0f, 0.0f },   /* Plombs en plus */
    { SH_SHO, 12, STAT_COUT, SOP_MUL, COND_NONE, 0, 0, 0, 0.85f, 0.0f },   /* Double canon */
    { SH_SHO, 16, STAT_DEG, SOP_MUL, COND_NONE, 0, 0, 0, 1.3f, 0.0f },   /* Plomb lourd */
    { SH_SHO, 18, STAT_POR, SOP_ADD, COND_NONE, 0, 0, 0, 20.0f, 0.0f },   /* Pompe tendue */
    { SH_SHO, 18, STAT_PNEAR, SOP_MAX, COND_NONE, 0, 0, 0, 20.0f, 0.0f },   /* Pompe tendue */
    { SH_SHO, 18, STAT_PFAR, SOP_ADD, COND_NONE, 0, 0, 0, 20.0f, 0.0f },   /* Pompe tendue */
    { SH_SHO, 19, STAT_SUI, SOP_ADD, COND_NONE, 1, 0, 0, 8.0f, 0.0f },   /* Plombs traqueurs */
    { SH_LAN, 3, STAT_MUR, SOP_ADD, COND_NONE, 0, 0, 0, 10.0f, 0.0f },   /* Moisson */
    { SH_LAN, 4, STAT_VOL, SOP_ADD, COND_NONE, 0, 0, 0, 0.02f, 0.0f },   /* Seve de lance */
    { SH_LAN, 9, STAT_VOL, SOP_ADD, COND_NONE, 0, 0, 0, 0.04f, 0.0f },   /* Sang de cavalier */
    { SH_LAN, 19, STAT_VOL, SOP_ADD, COND_NONE, 0, 0, 0, 0.1f, 0.0f },   /* Lance vorace */
    { SH_IMP, 3, STAT_MUR, SOP_ADD, COND_NONE, 0, 0, 0, 10.0f, 0.0f },   /* Cueillette */
    { SH_IMP, 4, STAT_VOL, SOP_ADD, COND_NONE, 0, 0, 0, 0.02f, 0.0f },   /* Souffle de poing */
    { SH_IMP, 9, STAT_VOL, SOP_ADD, COND_NONE, 0, 0, 0, 0.04f, 0.0f },   /* Sang vif */
    { SH_IMP, 19, STAT_VOL, SOP_ADD, COND_NONE, 0, 0, 0, 0.1f, 0.0f },   /* Acrobate vorace */
    { SH_AIO, 0, STAT_DEG, SOP_MUL, COND_NONE, 0, 0, 0, 1.1f, 0.0f },   /* Salve lourde */
};

#define SH_NBR 30
static const ShBranch shBr[SH_NBR] = {
    { SH_RAIL, SH_ROQ, { 1, 0, 1, 3 }, "Rail orbital", "1 seule munition, x4 degats, explosion d'impact 12 m", BK_RAIL, RL_C_ORBITAL, 0 },
    { SH_RAIL, SH_SHO, { 3, 4, 1, 1 }, "Rail d'ouverture", "la touche du rail ouvre la cible 3 s : plombs x1,3 sans perte a distance", BK_RAIL, RL_C_OUVERTURE, 0 },
    { SH_RAIL, SH_LAN, { 0, 0, 2, 2 }, "Rail de joute", "+40 % pendant la lance couchee", BK_RAIL, RL_C_JOUTE, 0 },
    { SH_RAIL, SH_IMP, { 4, 3, 0, 2 }, "Rail agile", "chaque rail qui touche rend 1 dash", BK_RAIL, RL_C_AGILE, 0 },
    { SH_RAIL, SH_PIE, { 4, 2, 3, 4 }, "Banque", "un rail qui traverse une piece ne consomme pas de munition (3 par vague)", BK_RAIL, RL_C_BANQUE, 0 },
    { SH_RAIL, SH_AIO, { 1, 1, 3, 0 }, "Rail multiple", "l'All in One tire 3 rails a 50 %", BK_RAIL, RL_C_MULTI, 0 },
    { SH_ROQ, SH_RAIL, { 0, 0, 4, 0 }, "Mini-nuke", "1 seule munition, x5 degats, rayon 14 m", BK_ROQ, 1, 0 },
    { SH_ROQ, SH_SHO, { 3, 2, 0, 3 }, "Roquette cluster", "la roquette se divise en 6 plombs au souffle", BK_ROQ, 1, 0 },
    { SH_ROQ, SH_LAN, { 2, 4, 4, 4 }, "Roquette montee", "lance couchee, la roquette part de la pointe a la vitesse de joute (+100 %)", BK_ROQ, 1, 0 },
    { SH_ROQ, SH_IMP, { 2, 4, 3, 3 }, "Roquette lente", "la roquette ralentit de 40 % a moins de 5 m de toi (pour la frapper)", BK_ROQ, 1, 0 },
    { SH_ROQ, SH_PIE, { 4, 2, 2, 2 }, "Roquette-piece", "la roquette redirigee garde sa vitesse et son rayon d'explosion", BK_ROQ, 1, 0 },
    { SH_ROQ, SH_AIO, { 1, 2, 2, 1 }, "Roquette salve", "l'All in One tire 8 roquettes en plus", BK_ROQ, 1, 0 },
    { SH_SHO, SH_RAIL, { 0, 0, 4, 3 }, "Plomb d'amorcage", "chaque plomb qui touche ajoute 5 points a la jauge de recharge du rail", BK_SHO, 22, 0 },
    { SH_SHO, SH_ROQ, { 4, 0, 2, 2 }, "Plomb explosif", "tout plomb qui touche explose 2 m", BK_SHO, 20, 0 },
    { SH_SHO, SH_LAN, { 0, 4, 3, 1 }, "Shotgun de joute", "salve a 3,5 m x1,4", BK_SHO, 21, 0 },
    { SH_SHO, SH_IMP, { 2, 3, 3, 4 }, "Shotgun acrobate", "le recul du Shotgun rend le double saut", BK_SHO, 23, 0 },
    { SH_SHO, SH_PIE, { 3, 2, 1, 4 }, "Shotgun a pieces", "remplace tous les plombs par des pieces (sans charge de piece)", BK_SHO, 24, 0 },
    { SH_SHO, SH_AIO, { 4, 1, 1, 0 }, "Shotgun total", "l'All in One tire 6 salves", BK_SHO, 25, 0 },
    { SH_LAN, SH_RAIL, { 3, 4, 4, 4 }, "Lance au rail", "la joute tire un rail a la fin", BK_MEL_CAP_RAIL, 1, 0 },
    { SH_LAN, SH_ROQ, { 4, 4, 2, 3 }, "Lance explosive", "la pointe explose 4 m", BK_MEL_CAP_BOOM, 4, 0 },
    { SH_LAN, SH_SHO, { 1, 3, 4, 2 }, "Lance a plombs", "la lance tire en cone", BK_MEL_CAP_PLOMB, 6, 0 },
    { SH_IMP, SH_RAIL, { 4, 4, 0, 3 }, "Impulsion au rail", "un uppercut recharge ton rail", BK_MEL_CAP_UPRAIL, 1, 0 },
    { SH_IMP, SH_ROQ, { 1, 3, 4, 3 }, "Poing de roquette", "le coup envoie une roquette (60 %)", BK_MEL_CAP_ROQ, 60, 0 },
    { SH_IMP, SH_SHO, { 3, 3, 2, 2 }, "Impulsion a plombs", "le 3e coup lache 6 plombs", BK_MEL_CAP_PLOMB3, 6, 0 },
    { SH_PIE, SH_RAIL, { 4, 1, 1, 1 }, "Pluie de pieces", "la charge lance 8 pieces en cercle autour de toi", BK_COMP, CP_K_PIE_RAIL, 0 },
    { SH_PIE, SH_ROQ, { 3, 0, 0, 2 }, "Piece explosive", "la piece explose quand une roquette la traverse (3 m)", BK_COMP, CP_K_PIE_ROQ, 0 },
    { SH_PIE, SH_SHO, { 1, 3, 1, 3 }, "Piece en plombs", "la piece explose en plombs", BK_COMP, CP_K_PIE_SHO, 0 },
    { SH_AIO, SH_RAIL, { 0, 2, 3, 3 }, "Salve rail", "la salve tire 2 projectiles par munition de jauge (2 rails)", BK_COMP, CP_K_AIO_RAIL, 0 },
    { SH_AIO, SH_ROQ, { 4, 3, 2, 4 }, "Salve explosive", "chaque projectile de la salve qui touche explose : degats et souffle du Shotgun (3 m, 45)", BK_COMP, CP_K_AIO_ROQ, 0 },
    { SH_AIO, SH_SHO, { 3, 2, 2, 1 }, "Salve de Shotgun", "la salve au Shotgun a un cone x3 et +50 % de plombs", BK_COMP, CP_K_AIO_SHO, 0 },
};

#define SH_NFU 15
static const ShFusion shFu[SH_NFU] = {
    { SH_RAIL, SH_ROQ, "FISSION", "un rail qui traverse une roquette en vol la convertit en nuke (rayon x2, degats roquette + rail x1,5) ; le kill rend 1 munition de chaque", BK_HYB, 0, 0 },
    { SH_RAIL, SH_SHO, "DOUBLE DETENTE", "un ennemi touche par ton rail est \" ouvert \" 3 s : tes plombs sur lui ne perdent plus de degats avec la distance et font x1,3 ; chaque plomb qui touche reduit la recharge du rail de 0,05 s (1,5 s au plus)", BK_HYB, 0, 0 },
    { SH_ROQ, SH_SHO, "SALVE D'ARTILLERIE", "tes explosions tirent 3 plombs perforants (40 %) ; ta salve de Shotgun fait +1 m de souffle", BK_ROQ, 1, 0 },
    { SH_RAIL, SH_LAN, "CHEVALIER", "une joute reussie recharge instantanement 1 rail", BK_RAIL, RL_F_CHEVALIER, 0 },
    { SH_RAIL, SH_IMP, "COMBO AERIEN", "un rail touche en l'air suspend ta chute 1 s", BK_RAIL, RL_F_AERIEN, 0 },
    { SH_ROQ, SH_LAN, "LANCE-ROQUETTES", "une joute reussie lache une roquette gratuite a x1,5", BK_ROQ, 1, 0 },
    { SH_ROQ, SH_IMP, "ACROBATIE EXPLOSIVE", "un punch-jump sur une roquette te propulse de 25 m/s et l'explose", BK_ROQ, 1, 0 },
    { SH_SHO, SH_LAN, "CHARGE DE PLOMB", "une joute reussie recharge le Shotgun et le tir qui suit est garanti a 100 %", BK_NONE, 0, 0 },
    { SH_SHO, SH_IMP, "ACROBATE", "le recul du Shotgun se combine au punch-jump : double impulsion, +40 % de vitesse horizontale max", BK_NONE, 0, 0 },
    { SH_RAIL, SH_PIE, "BILLARD", "tes ricochets s'enchainent tant qu'une cible reste sous 90 m ; le rail gagne +10 % par cible touchee par la chaine", BK_NONE, 0, 0 },
    { SH_RAIL, SH_AIO, "FUSILLADE", "chaque munition de jauge ajoute un rail a 30 % dans la salve", BK_RAIL, RL_F_FUSILLADE, 0 },
    { SH_ROQ, SH_PIE, "FEU D'ARTIFICE", "une roquette redirigee par une piece (comme dans Ultrakill) explose a chaque piece traversee (souffle 3 m, 60 degats) avant de repartir x1,5 vers l'ennemi le plus proche ; les pieces touchees ne sont pas consommees", BK_ROQ, 1, 0 },
    { SH_ROQ, SH_AIO, "TAPIS DE BOMBES", "l'All in One tire une salve de 3 roquettes en eventail par munition de jauge", BK_ROQ, 1, 0 },
    { SH_SHO, SH_PIE, "PIECE MITRAILLEE", "ta salve de Shotgun lance 6 pieces en eventail sans consommer de charge de piece ; une piece touchee par un plomb ricoche x1,5 sur 3 cibles", BK_NONE, 0, 0 },
    { SH_SHO, SH_AIO, "FEU A VOLONTE", "l'All in One tire toutes tes salves en 0,5 s ; chaque salve garde x1", BK_NONE, 0, 0 },
};

enum { HY_NONE, HY_ARTILLERIE, HY_CANON, HY_OBUSIER, HY_N };
typedef struct { unsigned char a, b, id; const char *name, *base, *desc; } ShHybrid;
#define SH_NHY 3
static const ShHybrid shHy[SH_NHY] = {
    { SH_RAIL, SH_ROQ, HY_ARTILLERIE, "ARTILLERIE ORBITALE", "tir du Railgun x explosion des Roquettes", "un trait instantane qui traverse tout, explose (8 m) au bout de sa course et a chaque cible traversee (3 m) ; degats du rail, souffle de la roquette" },
    { SH_RAIL, SH_SHO, HY_CANON, "CANON SCIE", "motif du Shotgun x projectile du Railgun", "8 rails fins en gerbe (cone de 6 deg), chacun traverse les ennemis a 50 % des degats du rail ; portee 120 m ; le recul du Shotgun te propulse" },
    { SH_ROQ, SH_SHO, HY_OBUSIER, "OBUSIER", "motif du Shotgun x projectile des Roquettes", "6 mini-roquettes en gerbe (cone de 8 deg), souffle de 2,5 m chacune ; le recul t'envoie en l'air" },
};
