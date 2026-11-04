/* VELOCITY - mode VAGUES : boutique de modificateurs (chemins de 5 etapes par arme, melee et competence) et effets
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   Regle du proprietaire : AUCUN achat n'augmente une stat (degats, cadence, munitions, PV, vitesse...). Chaque etape
   ajoute un COMPORTEMENT : un kill fait partir le coup vers d'autres cibles, une touche gele, un parry repousse... Les
   effets ne s'appliquent qu'aux touches de l'arme (ou de la competence) du chemin, et seulement si elle est equipee.
   Chemins : UP = WP_* (armes a distance), WP_COUNT + MK_* (melee), WP_COUNT + MK_COUNT + SK_* (competences). */

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
/* chemins : nom, et les 5 etapes dans l'ordre d'achat (on ne peut acheter l'etape k qu'apres k - 1) */
typedef struct { int fx[5]; } UpPath;
static const UpPath upPath[NUP] = {
    /* armes a distance (WP_*) */
    { { FX_FOURCHE, FX_GEL, FX_SATELLITE, FX_FRACTURE, FX_SURCHARGE } },        /* RAILGUN */
    { { FX_BRULURE, FX_VORTEX, FX_MINE, FX_ECLATS, FX_CONTAGION } },             /* ROQUETTES */
    { { FX_RECUL, FX_PROPULSION, FX_ECLATS, FX_BRULURE, FX_ONDE } },             /* EVENTAIL */
    { { FX_AERIEN, FX_IMPACT, FX_FRAPPE, FX_FOURCHE, FX_ARC } },                 /* ELAN */
    { { FX_BRULURE, FX_SPECTRE, FX_GEL, FX_ECLATS, FX_VORTEX } },                /* SCIE */
    { { FX_MINE, FX_VORTEX, FX_GEL, FX_CONTAGION, FX_ECLATS } },                 /* VENTOUSE */
    { { FX_BRULURE, FX_FOURCHE, FX_SPECTRE, FX_FRACTURE, FX_CONTAGION } },       /* TESLA */
    /* melee (MK_*) */
    { { FX_P_ONDE, FX_FRAPPE, FX_P_MULTI, FX_ARC, FX_P_STASE } },                /* EPEE */
    { { FX_IMPACT, FX_RECUL, FX_RESSAC, FX_P_ONDE, FX_VORTEX } },                /* ENCLUME */
    { { FX_FRAPPE, FX_FOURCHE, FX_P_MULTI, FX_GEL, FX_ONDE } },                  /* LANCE */
    { { FX_ATTRAIT, FX_P_MULTI, FX_ARC, FX_VORTEX, FX_P_STASE } },               /* YOYO */
    { { FX_FRACTURE, FX_P_ONDE, FX_FRAPPE, FX_IMPACT, FX_ECLATS } },             /* IMPULSION */
    { { FX_P_MULTI, FX_SPECTRE, FX_P_ONDE, FX_RECUL, FX_P_STASE } },             /* EGIDE */
    /* competences (SK_*) */
    { { FX_GEL, FX_ECLATS, FX_COIN_AIMANT, FX_ONDE, FX_CONTAGION } },            /* PIECE : touches des ricochets */
    { { FX_MARK_PROPAGE, FX_BRULURE, FX_S_STASE, FX_FOURCHE, FX_VORTEX } },      /* MARQUE : touches sur une cible marquee */
    { { FX_RECUL, FX_ONDE, FX_S_ONDE, FX_ECLATS, FX_CONTAGION } },               /* CHARGE : touche chargee */
    { { FX_ARC, FX_MINE, FX_GEL, FX_FOURCHE, FX_VORTEX } },                      /* RICOCHET : tir qui a rebondi */
    { { FX_BRULURE, FX_S_ONDE, FX_SPECTRE, FX_RESSAC, FX_CONTAGION } },          /* ALL IN ONE : la salve */
    { { FX_APPAT, FX_LEURRE_BOUM, FX_MIROIR, FX_S_STASE, FX_LEURRE_VORTEX } },   /* MIRAGE : le leurre */
    { { FX_GEL, FX_ECLATS, FX_ARC, FX_SATELLITE, FX_FOURCHE } },                 /* AIMANT : touches sous l'aimant */
};
static const int upPrice[5] = { VG_PRICE_1, VG_PRICE_2, VG_PRICE_3, VG_PRICE_4, VG_PRICE_5 };
static int vgLvl[NUP];                                    /* etapes achetees par chemin (0..5) */
static int vgCredits;                                     /* credits du mode VAGUES (un kill en rapporte) */
static float vgRicT, vgAllinT;                            /* fenetres : tir RICOCHET parti, salve ALL IN ONE partie */
static int fxStat[FX_COUNT];                              /* declenchements par effet (tests) */
static void vg_credit_kill(int i);                        /* src/solo/vagues.c */

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
/* le chemin u a-t-il l'effet fx parmi ses etapes achetees (et l'arme est-elle equipee) ? */
static int up_has(int u, int fx) {
    if (!vagOn || !up_equipped(u)) return 0;
    for (int k = 0; k < vgLvl[u]; k++) if (upPath[u].fx[k] == fx) return 1;
    return 0;
}
static int up_any(int fx) {                               /* n'importe quel chemin equipe (dash, retombee, parry) */
    for (int u = 0; u < NUP; u++) if (up_has(u, fx)) return 1;
    return 0;
}
static int up_price(int u) { return vgLvl[u] < 5 ? upPrice[vgLvl[u]] : 0; }
static int up_buy(int u) {                                /* 1 si achete */
    if (u < 0 || u >= NUP || vgLvl[u] >= 5 || vgCredits < up_price(u)) return 0;
    vgCredits -= up_price(u);
    vgLvl[u]++;
    return 1;
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
    vgDashT = 0; vgRicT = vgAllinT = 0; vgCoinCtx = 0;
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
        if (e->frac >= 3) { e->frac = 0; e->fracT = 0; fx_blast(e->pos, 3, 50, u); }
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
        float d = fmaxf(30, e->lastDmg * 0.5f);
        if (a >= 0) { add_beam(at, E[a].pos, v3(1, 0.85f, 0.4f), 0.2f); fx_damage(a, d, v3(0, 0, 0), u); }
        if (b >= 0) { add_beam(at, E[b].pos, v3(1, 0.85f, 0.4f), 0.2f); fx_damage(b, d, v3(0, 0, 0), u); }
        break;
    }
    case FX_ONDE: fx_blast(at, 3.5f, 45, u); break;
    case FX_ECLATS:
        for (int k = 0; k < 3; k++) fx_shard(at, vadd(vmul(rand_dir(), 6), v3(0, 4, 0)), u);
        break;
    case FX_MINE:
        for (int k = 0; k < MAXVM; k++) if (!VM[k].on) { VM[k] = (VMine){ at, 10, 0.4f, u, 1 }; break; }
        break;
    case FX_SPECTRE:
        for (int k = 0; k < MAXVP; k++) if (!VP[k].on) { VP[k] = (VSpectre){ frange(0, 2 * PI), 8, 1 }; break; }
        break;
    case FX_RESSAC:
        for (int j = 0; j < MAXE; j++)
            if (j != i && E[j].alive && !EN_BOSS(E[j].type) && vlen(vsub(E[j].pos, at)) < 9)
                E[j].vel = vadd(E[j].vel, vmul(vnorm(vsub(E[j].pos, at)), 18));
        burst(at, v3(0.6f, 0.9f, 1), 30, 14, 0.3f, 0.1f);
        break;
    case FX_VORTEX:
        for (int k = 0; k < MAXVV; k++) if (!VV[k].on) { VV[k] = (VVortex){ at, 1.2f, 1 }; break; }
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
    if (vgDepth >= 2) return;
    int paths[2], n = fx_paths(paths, charged, marked);
    for (int k = 0; k < n; k++)
        for (int s = 0; s < vgLvl[paths[k]] && up_equipped(paths[k]); s++) {
            int fx = upPath[paths[k]].fx[s];
            if (fxDef[fx].trig == TR_HIT && E[i].alive) fx_on_hit(fx, paths[k], i, dmg);
        }
}
static void vg_boss_down(int i);                          /* src/solo/vagues.c */
static void vg_on_kill(int i) {
    vg_credit_kill(i);
    if (EN_BOSS(E[i].type) && E[i].summoned != 2) vg_boss_down(i);
    if (vgDepth >= 2) return;
    int paths[2], n = fx_paths(paths, vgLastCharged, vgLastMarked);
    for (int k = 0; k < n; k++)
        for (int s = 0; s < vgLvl[paths[k]] && up_equipped(paths[k]); s++) {
            int fx = upPath[paths[k]].fx[s];
            if (fxDef[fx].trig == TR_KILL && (fx != FX_SURCHARGE || vgSrc == WP_RAIL)) fx_on_kill(fx, paths[k], i);
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
    vgRicT -= dt; vgAllinT -= dt;
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
        if (up_has(us, FX_S_ONDE)) { fxStat[FX_S_ONDE]++; fx_pulse(pc, 6, 25, 14, us); }
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
