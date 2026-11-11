/* VELOCITY - mode VAGUES : boutique (arbre de graines, branches et fusions) et effets
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   Regle du proprietaire : AUCUN achat n'augmente une stat (degats, cadence, munitions, PV, vitesse...). Chaque etape
   ajoute un COMPORTEMENT : un kill fait partir le coup vers d'autres cibles, une touche gele, un parry repousse... Les
   effets ne s'appliquent qu'aux touches de l'arme (ou de la competence) du chemin, et seulement si elle est equipee.
   Chemins : UP = WP_* (armes a distance), WP_COUNT + MK_* (melee), WP_COUNT + MK_COUNT + SK_* (competences).
   (Boutique v2 : les etapes qui modifient une stat sont des lignes de donnees lues par le SAC DE STATS, boutique_stats.c ;
   les effets qui ne sont pas des stats restent des comportements, boutique_<famille>.c.) */

#define NUP (WP_COUNT + MK_COUNT + SK_COUNT)
#define UP_MELEE(mk) (WP_COUNT + (mk))
#define UP_SKILL(sk) (WP_COUNT + MK_COUNT + (sk))

/* declencheurs */
enum { TR_HIT, TR_KILL, TR_FIRE, TR_PARRY, TR_DASH, TR_LAND, TR_SKILL, TR_PASSIVE };
enum {
    FX_FOURCHE, FX_GEL, FX_SATELLITE, FX_FRACTURE, FX_SURCHARGE, FX_BRULURE, FX_VORTEX, FX_MINE, FX_ECLATS,
    FX_CONTAGION, FX_RECUL, FX_PROPULSION, FX_ONDE, FX_AERIEN, FX_IMPACT, FX_FRAPPE, FX_ARC, FX_SPECTRE, FX_ATTRAIT,
    FX_RESSAC, FX_P_ONDE, FX_P_MULTI, FX_P_STASE, FX_S_ONDE, FX_S_STASE, FX_COIN_AIMANT, FX_MARK_PROPAGE, FX_APPAT,
    FX_LEURRE_BOUM, FX_MIROIR, FX_LEURRE_VORTEX, FX_COUNT
};
typedef struct { const char *name; int trig; const char *desc; } FxDef;
static const FxDef fxDef[FX_COUNT] = {
    { "FOURCHE",     TR_KILL,    "un kill relance le coup vers les 2 ennemis les plus proches" },
    { "GEL",         TR_HIT,     "une touche fige l'ennemi 0,8 s : il ne bouge ni ne tire" },
    { "SATELLITE",   TR_FIRE,    "chaque tir lache un eclat qui file vers l'ennemi vise" },
    { "FRACTURE",    TR_HIT,     "3 touches en 2 s sur le meme ennemi le font exploser" },
    { "SURCHARGE",   TR_KILL,    "un kill recharge le tir sur-le-champ" },
    { "BRULURE",     TR_HIT,     "une touche enflamme l'ennemi 3 s" },
    { "VORTEX",      TR_KILL,    "un kill aspire les ennemis proches vers la victime" },
    { "MINE",        TR_KILL,    "un kill laisse une mine qui saute au passage d'un ennemi" },
    { "ECLATS",      TR_KILL,    "un kill projette 3 eclats chercheurs" },
    { "CONTAGION",   TR_KILL,    "un kill transmet gel, brulure et marque aux voisins" },
    { "RECUL",       TR_HIT,     "une touche repousse violemment l'ennemi" },
    { "PROPULSION",  TR_FIRE,    "chaque tir te pousse en arriere (saut au recul)" },
    { "DETONATION",  TR_KILL,    "un kill fait exploser la victime" },
    { "AERIEN",      TR_HIT,     "en l'air, une touche eclabousse les ennemis autour" },
    { "IMPACT",      TR_LAND,    "retomber de haut fait une onde de choc au sol" },
    { "FRAPPE",      TR_DASH,    "ton dash frappe les ennemis que tu traverses" },
    { "ARC",         TR_HIT,     "une touche saute sur l'ennemi le plus proche" },
    { "SPECTRE",     TR_KILL,    "un kill invoque un spectre qui avale une orbe ennemie" },
    { "ATTRAIT",     TR_HIT,     "une touche tire l'ennemi vers toi" },
    { "RESSAC",      TR_KILL,    "un kill repousse les ennemis autour de la victime" },
    { "ONDE",        TR_PARRY,   "un parry lache une onde de choc autour de toi" },
    { "MIROIR",      TR_PARRY,   "un parry renvoie aussi toutes les orbes proches" },
    { "STASE",       TR_PARRY,   "un parry fige les ennemis autour 1,2 s" },
    { "PULSION",     TR_SKILL,   "utiliser la competence lache une onde autour de toi" },
    { "ARRET",       TR_SKILL,   "utiliser la competence fige les ennemis devant toi" },
    { "AIMANTEE",    TR_PASSIVE, "tes pieces derivent vers l'ennemi le plus proche" },
    { "PROPAGATION", TR_KILL,    "une cible marquee qui meurt marque ses 2 voisins" },
    { "APPAT",       TR_PASSIVE, "les ennemis visent et chargent ton leurre" },
    { "EXPLOSIF",    TR_PASSIVE, "le leurre explose quand il disparait" },
    { "REFLET",      TR_PASSIVE, "les orbes qui touchent le leurre repartent vers les ennemis" },
    { "GOUFFRE",     TR_PASSIVE, "le leurre aspire les ennemis proches" },
};
/* --- boutique v2 (docs/boutique_vagues/) : 7 graines, une etape achetee agit pour toute branche qui la contient ---
   Donnees generees (boutique_donnees.c) : etapes par graine (4 profondeurs x 5), 38 branches (4 etapes + capstone vers un
   partenaire du loadout), 19 fusions. Une etape sans effet code (BK_NONE) n'est jamais proposee : "A VENIR". */
/* les graines SH_* (enum) et les types du sac de stats (StatMod, STAT_*) sont dans monde/entites.c : le code des armes les lit */
enum { BK_NONE, BK_FX, BK_STAT, BK_SHO, BK_ROQ, BK_COMP, BK_HYB };   /* genres d'effet : voir outils/effets_c.js ; BK_STAT = etape-stat (sac de stats : a = 1re ligne de shMods, b = nombre de lignes) */
enum { BK_MEL_JCD = 100, BK_MEL_VELO, BK_MEL_ESTOCS, BK_MEL_ECD, BK_MEL_EANI, BK_MEL_JPTS, BK_MEL_JSOIN, BK_MEL_TAP,
       BK_MEL_JCHAIN, BK_MEL_JELITE, BK_MEL_RUEE, BK_MEL_ECLAIR, BK_MEL_TOURNOI, BK_MEL_KPTS,       /* groupe MELEE : src/solo/boutique_melee.c */
       BK_MEL_FEND, BK_MEL_FGAP, BK_MEL_FN, BK_MEL_FWHIFF, BK_MEL_FAIR, BK_MEL_FAIRPTS, BK_MEL_FSOIN, BK_MEL_FCHAIN,
       BK_MEL_FPTS, BK_MEL_FFREE, BK_MEL_FFREN,
       BK_MEL_CAP_RAIL, BK_MEL_CAP_BOOM, BK_MEL_CAP_PLOMB, BK_MEL_CAP_ESTOCS, BK_MEL_CAP_UPRAIL, BK_MEL_CAP_ROQ,
       BK_MEL_CAP_PLOMB3, BK_MEL_CAP_PIECE, BK_MEL_CAP_COUP9, BK_MEL_RELEVE };
typedef struct { const char *name, *desc; unsigned char depth, kind; short a, b; } ShStep;
typedef struct { unsigned char seed, partner, s[4]; const char *capName, *capDesc; unsigned char capKind; short capA, capB; } ShBranch;
typedef struct { unsigned char a, b; const char *name, *desc; unsigned char kind; short ka, kb; } ShFusion;
/* graine RAILGUN (boutique_rail.c) : genre BK_RAIL, parametre a = RL_* */
enum { BK_RAIL = 100 };
enum { RL_RESERVE = 1, RL_LARGE, RL_COURBE, RL_IMPACT, RL_SURTENSION, RL_LUNETTE, RL_SANG, RL_DOUBLE, RL_LOURD, RL_EXPLOSIF,
       RL_JUMEAU, RL_COLOSSAL, RL_PERCE, RL_GUIDE, RL_C_ORBITAL, RL_C_JOUTE, RL_C_AGILE, RL_C_BANQUE, RL_C_MULTI,
       RL_F_CHEVALIER, RL_F_AERIEN, RL_F_FUSILLADE, RL_TENDU, RL_ELITE, RL_C_OUVERTURE };
#include "boutique_donnees.c"

static const int shSeedUp[SH_N] = { WP_RAIL, WP_ROCKET, WP_SHOTGUN, UP_MELEE(MK_LANCE), UP_MELEE(MK_FIST),
                                    UP_SKILL(SK_COIN), UP_SKILL(SK_ALLIN) };
static const int shPrice[5] = { VG_PRICE_1, VG_PRICE_2, VG_PRICE_3, VG_PRICE_4, VG_PRICE_5 };
static const int shWeight[5] = { 50, 30, 14, 5, 1 };       /* tirage par rarete (profondeur) */
static const char *const shRarity[5] = { "COMMUNE", "PEU COMMUNE", "RARE", "EPIQUE", "LEGENDAIRE" };
#define SH_FUSION_PRICE 1200
#define SH_REROLL_BASE  40
#define SH_REROLL_STEP  20
enum { SO_STEP, SO_CAP, SO_FUSION, SO_UP };
typedef struct { int type, br, seed, step, price; } ShOffer; /* br : branche (capstone) ou fusion (index) ; niveau vise pour SO_UP */
static unsigned int shOwn[SH_N];                          /* etapes achetees par graine (bit = profondeur * 5 + rang) */
static unsigned char shCap[SH_NBR];                       /* capstone achete par branche */
static int shFusion = -1;                                 /* fusion achetee (une seule par partie) */
static int shRerolls;                                     /* relances consecutives */
static ShOffer shOff[4];                                  /* 3 offres tirees + la fusion garantie */
static int shNOff;
static int shSig = -1;                                    /* loadout au dernier tirage des offres */
static int shSwapOpen;                                    /* panneau CHANGER D'ARME ouvert */
#define SH_SWAP_PRICE 300
static int sh_loadout_sig(void) { return P.loadout[0] | (P.loadout[1] << 4) | (P.meleeKind << 8) | (P.skill << 12); }
static int vgCredits;                                     /* credits du mode VAGUES (un kill en rapporte) */
static float vgRicT, vgAllinT;                            /* fenetres : tir RICOCHET parti, salve ALL IN ONE partie */
static int fxStat[FX_COUNT];                              /* declenchements par effet (tests) */
static void vg_credit_kill(int i);                        /* src/solo/vagues.c */
static void vgsg_tick(float dt);                          /* src/solo/boutique_shotgun.c */
static void hy_reset(void);                               /* src/solo/boutique_hybrides.c : formes fusionnees */
static void hy_tick(float dt);
static void hy_charge(float dmg);

static const char *up_name(int u) {
    if (u < WP_COUNT) return ammoDef[u].name;
    if (u < WP_COUNT + MK_COUNT) return meleeNames[u - WP_COUNT];
    return skillDef[u - WP_COUNT - MK_COUNT].name;
}
static int up_equipped(int u) {
    if (u < 0 || u >= NUP) return 0;
    if (u < WP_COUNT) return P.loadout[0] == u || P.loadout[1] == u;
    if (u < WP_COUNT + MK_COUNT) return P.meleeKind == u - WP_COUNT;
    return P.skill == u - WP_COUNT - MK_COUNT;
}
static int sh_seed_of_up(int u) { for (int s = 0; s < SH_N; s++) if (shSeedUp[s] == u) return s; return -1; }
static int sh_on(int seed) { return up_equipped(shSeedUp[seed]); }
static int sh_owns(int seed, int step) { return (shOwn[seed] >> step) & 1u; }
/* --- NIVEAUX D'AMELIORATION : une etape achetee est niveau 1 ; elle monte jusqu'au niveau max de sa profondeur (5 / 4 / 3 / 2).
   Niveau n = +20 % d'effet par niveau sur les parametres numeriques : sh_scale = 1 + 0,2 x (n - 1). shLvl a 0 vaut niveau 1
   (une etape posee directement dans shOwn par un test reste valide). Tous les lecteurs d'effets passent par sh_scale. --- */
static unsigned char shLvl[SH_N][20];
static const int shMaxLvl[4] = { 5, 4, 3, 2 };
static const int shUpPrice[4][4] = { { 60, 80, 100, 120 }, { 120, 160, 200, 0 }, { 200, 300, 0, 0 }, { 400, 0, 0, 0 } };
static int sh_lvl(int seed, int step) {                   /* 0 si l'etape n'est pas achetee */
    if (!sh_owns(seed, step)) return 0;
    return shLvl[seed][step] > 1 ? shLvl[seed][step] : 1;
}
static float sh_scale(int seed, int step) { int n = sh_lvl(seed, step); return n ? 1.0f + 0.2f * (float)(n - 1) : 0.0f; }
static int sh_max_lvl(int seed, int step) { return shMaxLvl[shSteps[seed][step].depth - 1]; }
static int sh_up_price(int seed, int step) {              /* prix du niveau suivant (0 si au maximum) */
    int n = sh_lvl(seed, step), d = shSteps[seed][step].depth - 1;
    return n >= 1 && n < shMaxLvl[d] ? shUpPrice[d][n - 1] : 0;
}
/* l'etape a-t-elle un parametre numerique qui monte avec le niveau ? (les declencheurs purs et les nombres entiers fixes non) */
static int sh_scalable(int seed, int step) {
    const ShStep *st = &shSteps[seed][step];
    switch (st->kind) {
    case BK_NONE: return 0;
    case BK_STAT:                                         /* etape-stat : monte si au moins un de ses modificateurs n'est pas a valeur fixe (SMF_FIXED) */
        for (int i = st->a; i < st->a + st->b; i++) if (!(shMods[i].flags & SMF_FIXED)) return 1;
        return 0;
    case BK_FX: return st->a == FX_FOURCHE || st->a == FX_ECLATS || st->a == FX_ONDE || st->a == FX_FRACTURE || st->a == FX_VORTEX ||
                       st->a == FX_RESSAC || st->a == FX_S_ONDE || st->a == FX_SPECTRE;
    case BK_RAIL: return st->a != RL_DOUBLE && st->a != RL_SURTENSION;
    case BK_ROQ: return step != 1 && step != 11;          /* Soute (+1 munition) et Recuperation (1 munition) : entiers fixes */
    case BK_COMP: return st->a != CP_RESERVE && st->a != CP_RECYCLEE && st->a != CP_REBOND && st->a != CP_PLUIE &&
                         st->a != CP_A_RESERVE && st->a != CP_A_DOUBLE;
    case BK_MEL_ESTOCS: case BK_MEL_FN: case BK_MEL_JCHAIN: return 0;   /* nombre d'estocs / de coups, joute sans recharge */
    default: return 1;
    }
}
static int sh_branch(int seed, int partner) {             /* branche seed -> partner, -1 si le couple n'existe pas */
    for (int b = 0; b < SH_NBR; b++) if (shBr[b].seed == seed && shBr[b].partner == partner) return b;
    return -1;
}
/* le chemin u a-t-il l'effet fx parmi ses etapes achetees (et la graine est-elle equipee) ? */
static float up_sc(int u, int fx) {                       /* echelle (niveau) de l'effet fx du chemin u, 0 s'il n'est pas actif */
    int sd = sh_seed_of_up(u);
    if (!vagOn || sd < 0 || !up_equipped(u)) return 0;
    for (int k = 0; k < 20; k++) if (sh_owns(sd, k) && shSteps[sd][k].kind == BK_FX && shSteps[sd][k].a == fx) return sh_scale(sd, k);
    return 0;
}
static int up_has(int u, int fx) { return up_sc(u, fx) > 0; }
static float sh_sc_of(int sd, int kind, int a) {          /* echelle de l'etape achetee (genre kind, parametre a) de la graine sd, sans test d'equipement */
    for (int k = 0; k < 20; k++) if (sh_owns(sd, k) && shSteps[sd][k].kind == kind && shSteps[sd][k].a == a) return sh_scale(sd, k);
    return 0;
}
static int up_any(int fx) {                               /* n'importe quel chemin equipe (dash, retombee, parry) */
    for (int u = 0; u < NUP; u++) if (up_has(u, fx)) return 1;
    return 0;
}
static float sh_sum(int u, int kind) {                    /* somme du parametre a (x echelle du niveau) des etapes achetees de ce genre */
    int sd = sh_seed_of_up(u);
    float t = 0;
    if (!vagOn || sd < 0 || !up_equipped(u)) return 0;
    for (int k = 0; k < 20; k++) if (sh_owns(sd, k) && shSteps[sd][k].kind == kind) t += (float)shSteps[sd][k].a * sh_scale(sd, k);
    return t;
}
static int sh_count_owned(void) { int n = 0; for (int s = 0; s < SH_N; s++) for (int k = 0; k < 20; k++) n += sh_owns(s, k); return n; }

/* prochaine offre d'une branche : l'etape suivante (dans l'ordre du chemin), puis le capstone. 0 si rien ou "a venir" */
static int sh_next(int b, ShOffer *o) {
    const ShBranch *br = &shBr[b];
    if (!sh_on(br->seed) || !sh_on(br->partner)) return 0;
    for (int j = 0; j < 4; j++) {
        int step = j * 5 + br->s[j];                      /* profondeur j : 5 etapes par profondeur */
        if (sh_owns(br->seed, step)) continue;
        if (shSteps[br->seed][step].kind == BK_NONE) return 0;
        *o = (ShOffer){ SO_STEP, b, br->seed, step, shPrice[j] };
        return 1;
    }
    if (shCap[b] || br->capKind == BK_NONE) return 0;
    *o = (ShOffer){ SO_CAP, b, br->seed, 0, shPrice[4] };
    return 1;
}
static int sh_cap_pair(int fu) {                          /* les deux capstones d'une fusion sont-ils achetes ? */
    int x = sh_branch(shFu[fu].a, shFu[fu].b), y = sh_branch(shFu[fu].b, shFu[fu].a);
    return x >= 0 && y >= 0 && shCap[x] && shCap[y];
}
/* niveau d'une fusion : (plus petit investissement des deux branches de la paire) / 5, arrondi inferieur, 1 au moins.
   Investissement d'une branche = somme des niveaux de ses 4 etapes + 1 par capstone. Effet : 1 = 100 %, 2 = 125 %, 3 = 150 %. */
static int sh_branch_invest(int seed, int partner) {
    int b = sh_branch(seed, partner), t = 0;
    if (b < 0) return 0;
    for (int j = 0; j < 4; j++) t += sh_lvl(seed, j * 5 + shBr[b].s[j]);
    return t + (shCap[b] ? 1 : 0);
}
static int sh_fusion_lvl(void) {
    if (shFusion < 0) return 0;
    int x = sh_branch_invest(shFu[shFusion].a, shFu[shFusion].b), y = sh_branch_invest(shFu[shFusion].b, shFu[shFusion].a);
    int l = (x < y ? x : y) / 5;
    return l < 1 ? 1 : l;
}
static float sh_fusion_scale(void) { int l = sh_fusion_lvl(); return l ? 1.0f + 0.25f * (float)(l - 1) : 0.0f; }
static int sh_weight(const ShOffer *o) { return shWeight[o->type == SO_CAP ? 4 : shSteps[o->seed][o->step].depth - 1]; }
static void sh_roll(void) {                               /* tire 3 offres selon la rarete, plus la fusion garantie */
    ShOffer c[SH_NBR + SH_N * 20];
    int n = 0;
    shSig = sh_loadout_sig();
    for (int b = 0; b < SH_NBR; b++) {
        ShOffer o;
        if (!sh_next(b, &o)) continue;
        int dup = 0;
        for (int i = 0; i < n; i++)
            if (c[i].type == o.type && c[i].seed == o.seed && (o.type == SO_STEP ? c[i].step == o.step : c[i].br == o.br)) dup = 1;
        if (!dup) c[n++] = o;
    }
    for (int sd = 0; sd < SH_N; sd++) {                   /* amelioration de niveau des etapes possedees (graine equipee, effet qui monte) */
        if (!sh_on(sd)) continue;
        for (int k = 0; k < 20; k++)
            if (sh_scalable(sd, k) && sh_up_price(sd, k) > 0) c[n++] = (ShOffer){ SO_UP, sh_lvl(sd, k) + 1, sd, k, sh_up_price(sd, k) };
    }
    shNOff = 0;
    for (int pick = 0; pick < 3 && n > 0; pick++) {
        int tot = 0;
        for (int i = 0; i < n; i++) tot += sh_weight(&c[i]);
        int r = (int)(frand() * (float)tot), i = 0;
        for (; i < n - 1; i++) { r -= sh_weight(&c[i]); if (r < 0) break; }
        shOff[shNOff++] = c[i];
        c[i] = c[--n];
    }
    if (shFusion < 0)
        for (int f = 0; f < SH_NFU; f++)
            if (shFu[f].kind != BK_NONE && sh_on(shFu[f].a) && sh_on(shFu[f].b) && sh_cap_pair(f)) {
                shOff[shNOff++] = (ShOffer){ SO_FUSION, f, shFu[f].a, 0, SH_FUSION_PRICE };
                break;
            }
}
static int up_buy(int slot) {                             /* 1 si achete */
    if (slot < 0 || slot >= shNOff || vgCredits < shOff[slot].price) return 0;
    ShOffer o = shOff[slot];
    vgCredits -= o.price;
    if (o.type == SO_STEP) { shOwn[o.seed] |= 1u << o.step; shLvl[o.seed][o.step] = 1; }
    else if (o.type == SO_UP) shLvl[o.seed][o.step] = (unsigned char)o.br;
    else if (o.type == SO_CAP) shCap[o.br] = 1;
    else shFusion = o.br;
    sh_rebuild();                                         /* sac de stats : achat ou montee de niveau */
    shRerolls = 0;
    sh_roll();
    return 1;
}
static int sh_reroll_price(void) { return SH_REROLL_BASE + SH_REROLL_STEP * shRerolls; }
static int sh_reroll(void) {
    if (vgCredits < sh_reroll_price()) return 0;
    vgCredits -= sh_reroll_price(); shRerolls++;
    sh_roll();
    return 1;
}
/* CHANGER D'ARME (payant) : remplace une arme, la melee ou la competence du loadout. Les etapes achetees restent a la graine
   (inactives tant qu'elle n'est pas equipee) ; une fusion se desactive quand une de ses deux graines part et revient avec elle. */
static int sh_swap(int slot, int v) {                     /* slot : 0 arme 1, 1 arme 2, 2 melee, 3 competence ; 1 si fait */
    if (vgCredits < SH_SWAP_PRICE) return 0;
    if (slot < 2) {
        if (!vg_wp_ok(v) || P.loadout[0] == v || P.loadout[1] == v) return 0;
        P.loadout[slot] = v; P.ammo[v] = ammoDef[v].start; P.ammoGauge[v] = 0;
        if (P.weapon != P.loadout[0] && P.weapon != P.loadout[1]) P.weapon = P.loadout[0];
    } else if (slot == 2) {
        if (!vg_mk_ok(v) || P.meleeKind == v) return 0;
        P.meleeKind = v; P.meleeCd = 0; P.lanceState = LS_IDLE; P.lanceT = P.lanceVc = 0; P.lanceHitT = 10;
        P.fistStep = P.fistAnim = P.fistPts = P.fistUpTgt = 0;
    } else {
        if (!vg_sk_ok(v) || P.skill == v) return 0;
        P.skill = v; P.skillCh = (float)skillDef[v].max; P.ricArmed = 0; P.ricT = 0; P.chargeT = P.chargeHold = 0; P.aimT = 0;
    }
    vgCredits -= SH_SWAP_PRICE;
    vgPick[0] = P.loadout[0]; vgPick[1] = P.loadout[1]; vgPick[2] = P.meleeKind; vgPick[3] = P.skill; vgPickOn = 1;
    shSwapOpen = 0; shRerolls = 0;
    sh_rebuild();                                         /* sac de stats : le loadout a change, les graines equipees aussi */
    sh_roll();
    return 1;
}
static void sh_reset(void) {
    vgPickOn = 0; shSwapOpen = 0;
    memset(shOwn, 0, sizeof shOwn); memset(shCap, 0, sizeof shCap); memset(shLvl, 0, sizeof shLvl);
    shFusion = -1; shRerolls = 0; shNOff = 0; hy_reset();
    sh_rebuild();                                         /* sac de stats : plus aucune etape */
    sh_roll();
}

/* --- objets des effets : eclats chercheurs, mines, spectres, vortex --- */
typedef struct { V3 pos, vel; float life; int src, on; } VShard;
typedef struct { V3 pos; float life, arm; int src, on; } VMine;
typedef struct { float ang, life; int on; } VSpectre;
typedef struct { V3 pos; float life; int on; } VVortex;
#define MAXVS 32
#define MAXVM 12
#define MAXVP 6
#define MAXVV 8
static VShard VS[MAXVS];
static VMine VM[MAXVM];
static VSpectre VP[MAXVP];
static VVortex VV[MAXVV];
static float vgDashT, vgPrevDash, vgPrevSkill, vgPrevVy;
static unsigned int vgDashMask[2];
static int vgPrevGround, vgPrevRic, vgDecoyWas[MAXCOIN];
static V3 vgDecoyPos[MAXCOIN];

static void fx_reset(void) {
    memset(VS, 0, sizeof VS); memset(VM, 0, sizeof VM); memset(VP, 0, sizeof VP); memset(VV, 0, sizeof VV);
    memset(vgDecoyWas, 0, sizeof vgDecoyWas);
    vgDashT = 0; vgRicT = vgAllinT = 0; vgCoinCtx = 0; vgm_reset();
    vgPrevDash = P.dashCharge; vgPrevSkill = P.skillCh; vgPrevGround = 1; vgPrevRic = 0; vgPrevVy = 0;
}
/* degats d'un effet : comptes pour le chemin src, une seule generation de reaction en chaine (vgDepth) */
static void fx_damage(int i, float dmg, V3 knock, int src) {
    if (i < 0 || i >= MAXE || !E[i].alive) return;
    int s0 = vgSrc, k0 = vgProjKind;
    vgSrc = src; vgProjKind = -1; vgDepth++;
    damage_enemy(i, dmg, knock);
    vgDepth--; vgSrc = s0; vgProjKind = k0;
}
static void fx_blast(V3 c, float r, float dmg, int src) { /* explosion sans recul ni degat sur soi */
    int s0 = vgSrc, k0 = vgProjKind;
    vgSrc = src; vgProjKind = -1; vgDepth++;
    explode_ex(c, r, dmg, 2, 0, W_ROCKET, -1, -1, 0);
    vgDepth--; vgSrc = s0; vgProjKind = k0;
}
/* ennemi vivant le plus proche de p (hors skip, rayon r, ligne de vue) */
static int fx_nearest(V3 p, float r, int skip, int skip2) {
    int best = -1; float bd = r;
    for (int j = 0; j < MAXE; j++) {
        if (!E[j].alive || j == skip || j == skip2 || E[j].summoned == 2) continue;
        float d = vlen(vsub(E[j].pos, p)) - enemy_radius(E[j].type);
        if (d < bd && los(p, E[j].pos)) { bd = d; best = j; }
    }
    return best;
}
static void fx_shard(V3 at, V3 vel, int src) {
    for (int k = 0; k < MAXVS; k++) if (!VS[k].on) { VS[k] = (VShard){ at, vel, 2.5f, src, 1 }; return; }
}
static void fx_freeze(int j, float t) {
    if (j < 0 || EN_BOSS(E[j].type)) return;             /* les boss ne se figent pas */
    E[j].frozenT = fmaxf(E[j].frozenT, t);
}
/* onde autour d'un point : degats et recul vers l'exterieur (non boss) */
static void fx_pulse(V3 c, float r, float dmg, float knock, int src) {
    Ring *g = &RG[rgNext]; rgNext = (rgNext + 1) % MAXRG;
    g->p = v3(c.x, c.y - 0.8f, c.z); g->n = v3(0, 1, 0); g->col = v3(0.6f, 0.9f, 1); g->r = r; g->life = g->max = 0.4f;
    burst(c, v3(0.6f, 0.9f, 1), 40, r * 2, 0.35f, 0.12f);
    play(S_ANVIL_SLAM, 0.6f, 1.3f);
    for (int j = 0; j < MAXE; j++) {
        if (!E[j].alive) continue;
        V3 dv = vsub(E[j].pos, c);
        float d = vlen(dv);
        if (d > r + enemy_radius(E[j].type)) continue;
        fx_damage(j, dmg, EN_BOSS(E[j].type) ? v3(0, 0, 0) : vmul(vnorm(dv), knock), src);
    }
}

/* --- un effet sur une touche ou un kill --- */
static float fxSc = 1;                                    /* echelle (niveau) de l'etape dont l'effet se declenche */
static void fx_on_hit(int fx, int u, int i, float dmg) {
    Enemy *e = &E[i];
    fxStat[fx]++;
    switch (fx) {
    case FX_ARC: {
        int j = fx_nearest(e->pos, 9, i, -1);
        if (j >= 0) { add_beam(e->pos, E[j].pos, v3(0.5f, 0.75f, 1), 0.15f); fx_damage(j, dmg * 0.4f, v3(0, 0, 0), u); }
        break;
    }
    case FX_GEL: fx_freeze(i, 0.8f); break;
    case FX_BRULURE: e->burnT = 3; break;
    case FX_RECUL: if (!EN_BOSS(e->type)) e->vel = vadd(e->vel, vmul(vnorm(vsub(e->pos, eye_pos())), 14)); break;
    case FX_ATTRAIT: if (!EN_BOSS(e->type)) e->vel = vadd(e->vel, vadd(vmul(vnorm(vsub(eye_pos(), e->pos)), 10), v3(0, 2, 0))); break;
    case FX_FRACTURE:
        e->frac = e->fracT > 0 ? e->frac + 1 : 1; e->fracT = 2;
        if (e->frac >= 3) { e->frac = 0; e->fracT = 0; fx_blast(e->pos, 3, 50 * fxSc, u); }   /* FRACTURE : degats x niveau */
        break;
    case FX_AERIEN:
        if (!P.grounded)
            for (int j = 0; j < MAXE; j++) {
                if (j == i || !E[j].alive || vlen(vsub(E[j].pos, e->pos)) > 2.5f + enemy_radius(E[j].type)) continue;
                fx_damage(j, dmg * 0.3f, v3(0, 0, 0), u);
            }
        break;
    }
}
static void fx_on_kill(int fx, int u, int i) {
    Enemy *e = &E[i];
    V3 at = e->pos;
    fxStat[fx]++;
    switch (fx) {
    case FX_FOURCHE: {
        int a = fx_nearest(at, 25, i, -1), b = fx_nearest(at, 25, i, a);
        float d = fmaxf(30, e->lastDmg * 0.5f) * fxSc;      /* FOURCHE : degats x niveau */
        if (a >= 0) { add_beam(at, E[a].pos, v3(1, 0.85f, 0.4f), 0.2f); fx_damage(a, d, v3(0, 0, 0), u); }
        if (b >= 0) { add_beam(at, E[b].pos, v3(1, 0.85f, 0.4f), 0.2f); fx_damage(b, d, v3(0, 0, 0), u); }
        break;
    }
    case FX_ONDE: fx_blast(at, 3.5f, 45 * fxSc, u); break;   /* DETONATION : degats x niveau */
    case FX_ECLATS:
        for (int k = 0, nk = (int)(3 * fxSc + 0.5f); k < nk; k++) fx_shard(at, vadd(vmul(rand_dir(), 6), v3(0, 4, 0)), u);
        break;
    case FX_MINE:
        for (int k = 0; k < MAXVM; k++) if (!VM[k].on) { VM[k] = (VMine){ at, 10, 0.4f, u, 1 }; break; }
        break;
    case FX_SPECTRE:
        for (int k = 0; k < MAXVP; k++) if (!VP[k].on) { VP[k] = (VSpectre){ frange(0, 2 * PI), 8 * fxSc, 1 }; break; }
        break;
    case FX_RESSAC:
        for (int j = 0; j < MAXE; j++)
            if (j != i && E[j].alive && !EN_BOSS(E[j].type) && vlen(vsub(E[j].pos, at)) < 9 * fxSc)   /* RESSAC : portee x niveau */
                E[j].vel = vadd(E[j].vel, vmul(vnorm(vsub(E[j].pos, at)), 18));
        burst(at, v3(0.6f, 0.9f, 1), 30, 14, 0.3f, 0.1f);
        break;
    case FX_VORTEX:
        for (int k = 0; k < MAXVV; k++) if (!VV[k].on) { VV[k] = (VVortex){ at, 1.2f * fxSc, 1 }; break; }
        break;
    case FX_CONTAGION:
        for (int j = 0; j < MAXE; j++) {
            if (j == i || !E[j].alive || vlen(vsub(E[j].pos, at)) > 9) continue;
            if (e->burnT > 0) E[j].burnT = fmaxf(E[j].burnT, e->burnT);
            if (e->frozenT > 0) fx_freeze(j, e->frozenT);
            if (markE[i] > 0) markE[j] = fmaxf(markE[j], markE[i]);
            if (e->burnT > 0 || e->frozenT > 0 || markE[i] > 0) add_beam(at, E[j].pos, v3(0.6f, 1, 0.5f), 0.15f);
        }
        break;
    case FX_SURCHARGE:
        P.fireCd = 0;
        snprintf(popText, sizeof popText, "SURCHARGE"); popT = 0.8f;
        break;
    case FX_MARK_PROPAGE:
        if (markE[i] > 0) {
            int a = fx_nearest(at, 30, i, -1), b = fx_nearest(at, 30, i, a);
            if (a >= 0) markE[a] = MARK_T;
            if (b >= 0) markE[b] = MARK_T;
        }
        break;
    }
}

/* chemins concernes par une touche : celui de l'arme (vgSrc) et, selon le contexte, celui de la competence */
static int fx_paths(int *out, int charged, int marked) {
    int n = 0;
    if (vgSrc < 0 || vgSrc >= NUP) return 0;              /* degats sans source (brulure, ennemis) : aucun effet */
    out[n++] = vgSrc;
    int sk = -1;
    if (vgCoinCtx) sk = SK_COIN;
    else if (marked) sk = SK_MARK;
    else if (charged) sk = SK_CHARGE;
    else if (vgRicT > 0) sk = SK_RICOCHET;
    else if (vgAllinT > 0) sk = SK_ALLIN;
    else if (P.aimT > 0) sk = SK_AIM;
    if (sk >= 0 && P.skill == sk) out[n++] = UP_SKILL(sk);
    return n;
}
static int vgLastCharged, vgLastMarked;                   /* contexte de la derniere touche (le kill suit dans le meme appel) */
static void vg_on_hit(int i, float dmg, int charged, int marked) {
    vgLastCharged = charged; vgLastMarked = marked;
    hy_charge(dmg);                                       /* jauge de fusion (boutique_hybrides.c) */
    if (vgDepth >= 2) return;
    cp_aio_roq_hit(i);
    int paths[2], n = fx_paths(paths, charged, marked);
    for (int k = 0; k < n; k++) {
        int sd = sh_seed_of_up(paths[k]);
        if (sd < 0 || !up_equipped(paths[k])) continue;
        for (int s = 0; s < 20; s++) {
            if (!sh_owns(sd, s) || shSteps[sd][s].kind != BK_FX) continue;
            int fx = shSteps[sd][s].a;
            if (fxDef[fx].trig == TR_HIT && E[i].alive) { fxSc = sh_scale(sd, s); fx_on_hit(fx, paths[k], i, dmg); fxSc = 1; }
        }
    }
}
static void vg_boss_down(int i);                          /* src/solo/vagues.c */
static void vg_on_kill(int i) {
    vg_credit_kill(i);
    if (EN_BOSS(E[i].type) && E[i].summoned != 2) vg_boss_down(i);
    if (vgDepth >= 2) return;
    int paths[2], n = fx_paths(paths, vgLastCharged, vgLastMarked);
    for (int k = 0; k < n; k++) {
        int sd = sh_seed_of_up(paths[k]);
        if (sd < 0 || !up_equipped(paths[k])) continue;
        for (int s = 0; s < 20; s++) {
            if (!sh_owns(sd, s) || shSteps[sd][s].kind != BK_FX) continue;
            int fx = shSteps[sd][s].a;
            if (fxDef[fx].trig == TR_KILL) { fxSc = sh_scale(sd, s); fx_on_kill(fx, paths[k], i); fxSc = 1; }
        }
    }
}
/* parry (n'importe quelle melee) : effets du chemin de la melee equipee */
static void vg_on_parry(V3 at) {
    (void)at;
    if (!vagOn || vgDepth >= 2) return;
    int u = UP_MELEE(P.meleeKind);
    V3 pc = pcenter();
    if (up_has(u, FX_P_ONDE)) { fxStat[FX_P_ONDE]++; fx_pulse(pc, 7, 40, 16, u); }
    if (up_has(u, FX_P_STASE)) {
        fxStat[FX_P_STASE]++;
        for (int j = 0; j < MAXE; j++) if (E[j].alive && vlen(vsub(E[j].pos, pc)) < 15) fx_freeze(j, 1.2f);
    }
    if (up_has(u, FX_P_MULTI)) {                          /* MIROIR : les autres orbes proches repartent aussi */
        fxStat[FX_P_MULTI]++;
        for (int k = 0; k < MAXPR; k++) {
            Proj *p = &PR[k];
            if (!p->alive || p->kind != PR_ORB || vlen(vsub(p->pos, pc)) > 9) continue;
            int t = fx_nearest(p->pos, 80, -1, -1);
            V3 d = t >= 0 ? vnorm(vsub(E[t].pos, p->pos)) : vmul(view_dir(), 1);
            p->kind = PR_PARRIED; p->life = 4; p->owner = myId; p->parries++;
            p->vel = vmul(d, PARRY_SPEED);
        }
    }
}
/* MIRAGE (APPAT) : le leurre attire les ennemis */
static int vg_lure(V3 *at) {
    if (!vagOn || !up_has(UP_SKILL(SK_DECOY), FX_APPAT)) return 0;
    for (int k = 0; k < MAXCOIN; k++)
        if (CO[k].alive && CO[k].decoy && CO[k].owner == myId) { *at = CO[k].pos; return 1; }
    return 0;
}
/* degats recus : armure des boss, bouclier de face du bastion (comportement des ennemis, pas de PV en plus) */
static float vg_dmg_filter(int i, float dmg) {
    Enemy *e = &E[i];
    if (EN_BOSS(e->type)) return boss_filter(i, dmg);
    if (e->type != EN_CARRE) {                            /* CARRE : tant qu'un carre vit, les autres encaissent x VG_CARRE_MULT */
        for (int j = 0; j < MAXE; j++)
            if (E[j].alive && E[j].type == EN_CARRE) {
                burst(e->pos, enemy_color(EN_CARRE), 5, 4, 0.2f, 0.08f);
                dmg *= VG_CARRE_MULT;
                break;
            }
    }
    if (e->type == EN_BASTION && vgProjKind != PR_PARRIED) {
        V3 fwd = v3(sinf(e->yaw), 0, cosf(e->yaw)), to = vnorm(vsub(eye_pos(), e->pos));
        if (vdot(fwd, to) > 0.45f) {                      /* de face : le bouclier prend presque tout */
            burst(vadd(e->pos, vmul(fwd, 1.3f)), v3(0.6f, 0.7f, 1), 6, 5, 0.2f, 0.08f);
            return dmg * 0.15f;
        }
    }
    return dmg;
}

/* --- tick : declencheurs continus (tir, dash, retombee, competence, leurre) et objets des effets --- */
static void vg_on_fire(int w) {
    if (!vagOn) return;
    if (up_has(w, FX_SATELLITE)) {
        fxStat[FX_SATELLITE]++;
        fx_shard(vadd(eye_pos(), vmul(view_dir(), 1)), vmul(view_dir(), 20), w);
    }
    if (up_has(w, FX_PROPULSION)) {
        fxStat[FX_PROPULSION]++;
        P.vel = vadd(P.vel, vmul(view_dir(), -7));
        if (view_dir().y < -0.3f) P.grounded = 0;
    }
}
static void vg_fx_tick(float dt) {
    if (!vagOn) return;
    V3 pc = pcenter();
    vg_rail_tick(dt);                                     /* groupe RAILGUN */
    vgRicT -= dt; vgAllinT -= dt;
    cp_tick(dt);                                          /* groupe COMPETENCES : pieces, salve ALL IN ONE */
    roq_fx_tick(dt);                                      /* boutique_roquettes.c */
    hy_tick(dt);                                          /* formes fusionnees : touche, duree, fin (boutique_hybrides.c) */
    vgsg_tick(dt);                                        /* SHOTGUN : salves gratuites en attente (boutique_shotgun.c) */
    /* dash */
    if (P.dashCharge < vgPrevDash - 0.5f && up_any(FX_FRAPPE)) { vgDashT = 0.35f; vgDashMask[0] = vgDashMask[1] = 0; fxStat[FX_FRAPPE]++; }
    vgPrevDash = P.dashCharge;
    if (vgDashT > 0) {
        vgDashT -= dt;
        for (int j = 0; j < MAXE; j++) {
            if (!E[j].alive || ((vgDashMask[j >> 5] >> (j & 31)) & 1u)) continue;
            if (vlen(vsub(E[j].pos, pc)) < enemy_radius(E[j].type) + 1.4f) {
                vgDashMask[j >> 5] |= 1u << (j & 31);
                burst(E[j].pos, v3(1, 1, 1), 16, 8, 0.3f, 0.1f);
                fx_damage(j, 45, vmul(vnorm(P.vel), 8), UP_MELEE(P.meleeKind));
            }
        }
    }
    /* retombee de haut */
    if (P.grounded && !vgPrevGround && vgPrevVy < -13 && up_any(FX_IMPACT)) {
        fxStat[FX_IMPACT]++;
        fx_pulse(vadd(P.pos, v3(0, 0.8f, 0)), 6, 50, 10, UP_MELEE(P.meleeKind));
    }
    vgPrevGround = P.grounded; vgPrevVy = P.vel.y;
    /* competence utilisee : charge depensee */
    int us = UP_SKILL(P.skill);
    if (P.skillCh < vgPrevSkill - 0.5f) {
        if (P.skill == SK_ALLIN) vgAllinT = 1.5f;
        if (up_has(us, FX_S_ONDE)) { fxStat[FX_S_ONDE]++; fx_pulse(pc, 6, 25 * up_sc(us, FX_S_ONDE), 14, us); }   /* PULSION : degats x niveau */
        if (up_has(us, FX_S_STASE)) {
            fxStat[FX_S_STASE]++;
            for (int j = 0; j < MAXE; j++) {
                if (!E[j].alive) continue;
                V3 d = vsub(E[j].pos, eye_pos());
                float l = vlen(d);
                if (l < 30 && vdot(vmul(d, 1 / fmaxf(l, 0.01f)), view_dir()) > 0.8f) fx_freeze(j, 1.2f);
            }
        }
    }
    vgPrevSkill = P.skillCh;
    if (vgPrevRic && !P.ricArmed) vgRicT = 1.5f;          /* RICOCHET : le tir qui rebondit vient de partir */
    vgPrevRic = P.ricArmed;
    /* pieces AIMANTEES, leurre MIRAGE */
    int coinMag = up_has(UP_SKILL(SK_COIN), FX_COIN_AIMANT), ud = UP_SKILL(SK_DECOY);
    for (int k = 0; k < MAXCOIN; k++) {
        Coin *c = &CO[k];
        int mine = c->alive && c->owner == myId;
        if (mine && !c->decoy && coinMag) {
            int t = fx_nearest(c->pos, 14, -1, -1);
            if (t >= 0) c->vel = vlerp(c->vel, vmul(vnorm(vsub(E[t].pos, c->pos)), 9), fminf(1, dt * 2.5f));
        }
        if (mine && c->decoy) {
            vgDecoyPos[k] = c->pos;
            if (up_has(ud, FX_MIROIR))                    /* REFLET : les orbes qui le touchent repartent */
                for (int q = 0; q < MAXPR; q++) {
                    Proj *p = &PR[q];
                    if (!p->alive || p->kind != PR_ORB || vlen(vsub(p->pos, c->pos)) > 1.4f) continue;
                    int t = fx_nearest(p->pos, 80, -1, -1);
                    p->kind = PR_PARRIED; p->life = 4; p->owner = myId; p->parries++;
                    p->vel = vmul(t >= 0 ? vnorm(vsub(E[t].pos, p->pos)) : vmul(p->vel, -1.0f / fmaxf(vlen(p->vel), 0.01f)), PARRY_SPEED);
                    fxStat[FX_MIROIR]++;
                }
            if (up_has(ud, FX_LEURRE_VORTEX))
                for (int j = 0; j < MAXE; j++)
                    if (E[j].alive && !EN_BOSS(E[j].type) && vlen(vsub(E[j].pos, c->pos)) < 10)
                        E[j].vel = vadd(E[j].vel, vmul(vnorm(vsub(c->pos, E[j].pos)), 14 * dt));
        }
        if (vgDecoyWas[k] && !(mine && c->decoy) && up_has(ud, FX_LEURRE_BOUM)) {   /* le leurre disparait : il saute */
            fxStat[FX_LEURRE_BOUM]++;
            fx_blast(vgDecoyPos[k], 4.5f, 70, ud);
        }
        vgDecoyWas[k] = mine && c->decoy;
    }
    /* eclats chercheurs */
    for (int k = 0; k < MAXVS; k++) {
        VShard *s = &VS[k];
        if (!s->on) continue;
        if ((s->life -= dt) <= 0) { s->on = 0; continue; }
        int t = fx_nearest(s->pos, 40, -1, -1);
        if (t >= 0) {
            V3 want = vmul(vnorm(vsub(E[t].pos, s->pos)), 30);
            s->vel = vlerp(s->vel, want, fminf(1, dt * 5));
            if (vlen(vsub(E[t].pos, s->pos)) < enemy_radius(E[t].type) + 0.4f) {
                burst(s->pos, v3(1, 0.9f, 0.5f), 10, 5, 0.25f, 0.08f);
                fx_damage(t, 22, vmul(vnorm(s->vel), 3), s->src);
                s->on = 0; continue;
            }
        } else s->vel.y -= 6 * dt;
        s->pos = vadd(s->pos, vmul(s->vel, dt));
        if (frand() < 0.6f) add_part(s->pos, v3(0, 0, 0), v3(1, 0.85f, 0.4f), 0.2f, 0.08f, 0);
    }
    /* mines */
    for (int k = 0; k < MAXVM; k++) {
        VMine *m = &VM[k];
        if (!m->on) continue;
        m->life -= dt; m->arm -= dt;
        if (m->life <= 0) { m->on = 0; continue; }
        if (m->arm > 0) continue;
        for (int j = 0; j < MAXE; j++)
            if (E[j].alive && E[j].summoned != 2 && vlen(vsub(E[j].pos, m->pos)) < 3.2f + enemy_radius(E[j].type)) {
                m->on = 0; fx_blast(m->pos, 4, 60, m->src); break;
            }
    }
    /* spectres : tournent autour de toi et avalent une orbe ennemie */
    for (int k = 0; k < MAXVP; k++) {
        VSpectre *v = &VP[k];
        if (!v->on) continue;
        if ((v->life -= dt) <= 0) { v->on = 0; continue; }
        v->ang += 3 * dt;
        V3 at = vadd(pc, v3(cosf(v->ang) * 1.8f, 0.4f, sinf(v->ang) * 1.8f));
        for (int q = 0; q < MAXPR; q++) {
            Proj *p = &PR[q];
            if (p->alive && p->kind == PR_ORB && vlen(vsub(p->pos, at)) < 1.6f) {
                p->alive = 0; v->on = 0;
                burst(at, v3(0.7f, 0.9f, 1), 20, 6, 0.3f, 0.1f);
                play(S_PARRY, 0.4f, 1.6f);
                break;
            }
        }
    }
    /* vortex */
    for (int k = 0; k < MAXVV; k++) {
        VVortex *v = &VV[k];
        if (!v->on) continue;
        if ((v->life -= dt) <= 0) { v->on = 0; continue; }
        for (int j = 0; j < MAXE; j++) {
            if (!E[j].alive || EN_BOSS(E[j].type)) continue;
            V3 d = vsub(v->pos, E[j].pos);
            float l = vlen(d);
            if (l < 12 && l > 0.5f) E[j].vel = vadd(E[j].vel, vmul(d, 30 * dt / l));
        }
        if (frand() < 0.8f) { V3 o = vadd(v->pos, vmul(rand_dir(), 5)); add_part(o, vmul(vsub(v->pos, o), 3), v3(0.7f, 0.4f, 1), 0.3f, 0.1f, 0); }
    }
    /* brulures, compteurs de FRACTURE */
    for (int j = 0; j < MAXE; j++) {
        Enemy *e = &E[j];
        if (!e->alive) continue;
        if (e->fracT > 0) e->fracT -= dt;
        if (e->burnT > 0) {
            float b0 = e->burnT;
            e->burnT -= dt;
            if (frand() < 0.5f) add_part(vadd(e->pos, vmul(rand_dir(), enemy_radius(e->type))), v3(0, 2, 0), v3(1, 0.5f, 0.1f), 0.4f, 0.12f, -2);
            if (floorf(b0 * 2) != floorf(e->burnT * 2)) fx_damage(j, 4, v3(0, 0, 0), -1);   /* 8 PV/s, deux fois par seconde */
        }
    }
}

/* modificateurs de degats de la boutique pour la source de la touche : stat DEG du sac de stats (Calibre, Poing de plomb, Roquette lourde...),
   avec le contexte de la touche pour les etapes conditionnelles (distance, en l'air, elite) ; puis la salve ALL IN ONE */
static float vg_dmg_mul(int i, int charged, int marked) {
    int paths[2], n = fx_paths(paths, charged, marked);
    StatCtx c = { 0 };
    c.dist = vlen(vsub(E[i].pos, eye_pos())); c.air = !P.grounded; c.elite = E[i].elite;
    float m = 1;
    for (int k = 0; k < n; k++) m = st_ctx(sh_seed_of_up(paths[k]), STAT_DEG, m, &c);   /* chemin sans graine ou graine non equipee : m inchange */
    m *= cp_dmg_mul(i);                                   /* groupe COMPETENCES : salve complete, cible unique */
    return m;
}
static void vg_lifesteal(float dmg) {                     /* vol de vie de la melee equipee : stat VOL du sac (fraction des degats rendue en PV) */
    if (vgSrc != UP_MELEE(P.meleeKind)) return;
    float pc = st(sh_seed_of_up(vgSrc), STAT_VOL, 0);
    if (pc > 0) vg_heal(dmg * pc);
}
