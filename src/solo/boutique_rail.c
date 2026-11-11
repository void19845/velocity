/* VELOCITY - boutique du mode VAGUES, graine RAILGUN (effets codes : docs/boutique_vagues/outils/effets/rail.js)
   Morceau de main.c : inclus apres src/solo/ameliorations.c, ne se compile pas seul (voir main.c).
   Genre BK_RAIL : le parametre `a` de l'etape / du capstone / de la fusion est un RL_* (enum dans ameliorations.c).
   Tout n'agit que si vagOn et si le Railgun est equipe (rl_has) ; les crochets du combat (pieces_rail.c, degats.c,
   loadout.c, lance.c) sont declares en avance dans monde/entites.c. */

/* l'etape RL_* est-elle achetee (graine RAIL equipee, mode VAGUES) ? */
static float rl_sc(int id) {                              /* echelle (niveau) de l'etape RL_id, 0 si absente */
    if (!vagOn || !sh_on(SH_RAIL)) return 0;
    return sh_sc_of(SH_RAIL, BK_RAIL, id);
}
static int rl_has(int id) { return rl_sc(id) > 0; }
/* capstone RAIL>partner achete, les deux graines equipees */
static int rl_cap(int partner, int id) {
    if (!vagOn || !sh_on(SH_RAIL) || !sh_on(partner)) return 0;
    int b = sh_branch(SH_RAIL, partner);
    return b >= 0 && shCap[b] && shBr[b].capKind == BK_RAIL && shBr[b].capA == id;
}
/* fusion achetee dont l'effet est id (RAIL en premiere graine) */
static int rl_fus(int id) {
    if (!vagOn || shFusion < 0 || shFu[shFusion].a != SH_RAIL || shFu[shFusion].kind != BK_RAIL || shFu[shFusion].ka != id) return 0;
    return sh_on(shFu[shFusion].a) && sh_on(shFu[shFusion].b);
}

/* --- largeur du tir : ennemis (rayon x) et zone de la tete --- */
static float vg_rail_wide(int w) {
    if (w != W_RAIL || !vagOn) return 1;
    return rl_has(RL_COLOSSAL) ? 1 + 0.40f * rl_sc(RL_COLOSSAL) : rl_has(RL_LARGE) ? 1 + 0.15f * rl_sc(RL_LARGE) : 1;
}
static int vg_rail_head(int i, V3 at, int w) {
    if (w != W_RAIL || !vagOn) return head_hit_enemy(i, at);
    float hw = rl_has(RL_COLOSSAL) ? 0.40f * rl_sc(RL_COLOSSAL) : rl_has(RL_LUNETTE) ? 0.15f * rl_sc(RL_LUNETTE) : 0;
    float frac = 1 - (1 - HEAD_ENEMY_FRAC) * (1 + hw);       /* la zone de la tete est hw plus haute */
    return at.y > E[i].pos.y + enemy_radius(E[i].type) * frac;
}

/* --- visee courbe : le tir tourne de deg degres vers l'ennemi visible le plus proche de la ligne de mire --- */
static V3 vg_rail_bend(V3 o, V3 d) {
    float deg = rl_has(RL_GUIDE) ? 8.0f * rl_sc(RL_GUIDE) : rl_has(RL_COURBE) ? 1.0f * rl_sc(RL_COURBE) : 0;
    if (deg <= 0) return d;
    int best = -1; float ba = 0.6f;                       /* cone de recherche : 35 degres */
    for (int j = 0; j < MAXE; j++) {
        if (!E[j].alive || E[j].summoned == 2) continue;
        V3 to = vnorm(vsub(E[j].pos, o));
        float a = acosf(clampf(vdot(d, to), -1, 1));
        if (a < ba && los(o, E[j].pos)) { ba = a; best = j; }
    }
    if (best < 0 || ba < 1e-4f) return d;
    V3 to = vnorm(vsub(E[best].pos, o));
    V3 perp = vnorm(vsub(to, vmul(d, vdot(d, to))));
    float r = fminf(ba, deg * PI / 180);
    return vnorm(vadd(vmul(d, cosf(r)), vmul(perp, sinf(r))));
}

/* --- ordre de traversee : rang de chaque ennemi le long du segment (Calibre lourd, souffle de la 1re cible) --- */
static int rlRank[MAXE];
static void vg_rail_rank(V3 eye, V3 d, float tw) {
    float tt[MAXE], wide = vg_rail_wide(W_RAIL);
    for (int i = 0; i < MAXE; i++) {
        float t;
        tt[i] = E[i].alive && ray_sphere(eye, d, E[i].pos, enemy_radius(E[i].type) * wide, tw, &t) ? t : -1;
    }
    for (int i = 0; i < MAXE; i++) {
        rlRank[i] = 0;
        if (tt[i] < 0) continue;
        for (int j = 0; j < MAXE; j++) if (tt[j] >= 0 && tt[j] < tt[i]) rlRank[i]++;
    }
}
static float vg_rail_mul(int i) {                         /* Calibre lourd : +15 % par cible deja traversee */
    float m = rl_has(RL_LOURD) ? powf(1 + 0.15f * rl_sc(RL_LOURD), (float)rlRank[i]) : 1;
    if (rl_has(RL_TENDU)) m *= 1 + 0.05f * rl_sc(RL_TENDU) * (float)(rlRank[i] < 5 ? rlRank[i] : 5);   /* Rail tendu : +5 % par cible deja traversee (5 max) */
    if (rl_has(RL_ELITE) && E[i].elite) m *= 1 + 0.5f * rl_sc(RL_ELITE);   /* Tir d'elite : +50 % sur les elites */
    return m;
}

/* --- touche d'un ennemi par le rail : souffles, soin --- */
static int rlHeadHit, rlCoinHit, rlAnyHit;                /* ce tir : touche a la tete, piece traversee, cible touchee */
static V3 rlLastAt;                                       /* derniere cible touchee par ce tir */
/* --- capstone RAIL>SHOTGUN (Rail d'ouverture) : la touche du rail ouvre l'ennemi RL_OPEN_T s ; les plombs du Shotgun qui le
   touchent pendant ce temps font x1,3 et ne perdent plus rien avec la distance (lu par fire_shotgun : vg_rail_open_mul) --- */
#define RL_OPEN_T    3.0f
#define RL_OPEN_MUL  1.3f
static float rlOpenT[MAXE];                               /* ouverture restante de chaque ennemi (s) */
static int rlOpenStat, rlOpenPellets;                     /* ouvertures posees, plombs renforces (tests) */
static int rl_open_on(void) { return rl_cap(SH_SHO, RL_C_OUVERTURE); }
static void rl_open(int i) {                              /* le rail vient de toucher l'ennemi i */
    if (!E[i].alive || !rl_open_on()) return;
    if (rlOpenT[i] <= 0) { snprintf(popText, sizeof popText, "CIBLE OUVERTE"); popT = 0.8f; }
    rlOpenT[i] = RL_OPEN_T; rlOpenStat++;
    burst(E[i].pos, v3(1, 0.6f, 0.2f), 14, 5, 0.3f, 0.1f);
}
static float vg_rail_open_mul(int i, float fo) {          /* fo : perte a distance habituelle du plomb ; renvoie son multiplicateur de degats */
    if (i < 0 || i >= MAXE || rlOpenT[i] <= 0 || !E[i].alive || !rl_open_on()) return fo;
    rlOpenPellets++;
    return fmaxf(fo, 1.0f) * RL_OPEN_MUL;                 /* sans perte : jamais moins de x1 (le bonus de proximite reste), puis x1,3 */
}
static void vg_rail_hit(int i, V3 at, float dmg, int head) {
    (void)at;
    rlAnyHit = 1; rlLastAt = E[i].pos;
    rl_open(i);                                           /* capstone RAIL>SHOTGUN : la cible est ouverte */
    if (head) {
        rlHeadHit++;
        if (rl_has(RL_SANG)) vg_heal(6 * rl_sc(RL_SANG));
    }
    if (rlRank[i] == 0 && rl_has(RL_IMPACT)) fx_blast(E[i].pos, 1.5f, dmg * 0.30f * rl_sc(RL_IMPACT), WP_RAIL);
    if (head && rl_has(RL_EXPLOSIF)) fx_blast(E[i].pos, 3.0f, dmg * 0.50f * rl_sc(RL_EXPLOSIF), WP_RAIL);
}

/* --- chaine : Surtension, une touche a la tete avance la chaine de +3 au lieu de +2 --- */
static int vg_rail_chain(void) { return vgSrc == WP_RAIL && rl_has(RL_SURTENSION) ? 1 : 0; }

/* --- Reserve : +20 % de points de jauge par kill --- */
static float vg_rail_gauge_mul(void) { return 1 + 0.2f * rl_sc(RL_RESERVE); }

/* --- Perce-mur : un mur de moins de 1 m est traverse, le tir repart a 70 % (l'epaisseur ET les degats montent avec le niveau) --- */
static int rl_inside(V3 p) {
    for (int b = 0; b < nb; b++)
        if (p.x >= boxes[b].mn.x && p.x <= boxes[b].mx.x && p.y >= boxes[b].mn.y && p.y <= boxes[b].mx.y &&
            p.z >= boxes[b].mn.z && p.z <= boxes[b].mx.z) return 1;
    return 0;
}
static int vg_rail_wall(V3 d, float dmg, float knockE, float knockP, int w, V3 *endOut, V3 *nOut) {
    if (!rl_has(RL_PERCE)) return 0;
    V3 a = *endOut, o2;
    int ok = 0;
    float thick = 1.0f * rl_sc(RL_PERCE);                 /* epaisseur max traversee : 1 m x niveau */
    for (float s = 0.05f; s <= thick && !ok; s += 0.05f) {
        o2 = vadd(a, vmul(d, s));
        if (!rl_inside(o2)) ok = 1;
    }
    if (!ok) return 0;                                    /* mur trop epais */
    V3 e2, n2; int st;
    int hit = hitscan_seg(o2, d, RIC_SEG_LEN, 0, dmg * fminf(1.0f, 0.7f * rl_sc(RL_PERCE)), knockE, knockP, w, &e2, &n2, &st);   /* 70 % x niveau, 100 % au plus */
    add_beam(o2, e2, v3(0.4f, 0.85f, 1), 0.5f);
    burst(a, v3(0.6f, 0.9f, 1), 10, 5, 0.25f, 0.08f);
    *endOut = e2; *nOut = n2;
    return hit;
}

/* --- avant / apres le tir de rail (fire_rail) --- */
static int rlBanque, rlBanqueWave = -1, rlAllinStamp;
static float rlHoverT;
static float vg_rail_before(void) {                       /* multiplicateur de degats du tir */
    rlHeadHit = rlCoinHit = rlAnyHit = 0;
    float m = 1;
    if (rl_cap(SH_LAN, RL_C_JOUTE) && P.lanceState == LS_COUCH) m *= 1.4f;          /* Rail de joute */
    if (rl_cap(SH_ROQ, RL_C_ORBITAL)) m *= 4;                                       /* Rail orbital */
    return m;
}
static int rl_extra(float dmg, float dyaw) {              /* un rail de plus, sans munition ni chaine */
    float y = P.yaw; V3 e, n;
    P.yaw += dyaw;
    int h = hitscan_shot(dmg, 8, 4, W_RAIL, &e, &n);
    P.yaw = y;
    add_beam(muzzle(), e, v3(0.4f, 0.85f, 1), 0.5f);
    return h;
}
static void rl_ammo_plus(void) {
    if (P.ammo[WP_RAIL] < ammoDef[WP_RAIL].max) P.ammo[WP_RAIL]++;
}
static void vg_rail_after(int hit, int nk, float dmg, V3 end) {
    if (rl_has(RL_DOUBLE) && nk >= 2) rl_ammo_plus();                               /* Double */
    if (hit && rl_cap(SH_IMP, RL_C_AGILE)) P.dashCharge = fminf(2, P.dashCharge + 1);   /* Rail agile */
    if (hit && !P.grounded && rl_fus(RL_F_AERIEN)) rlHoverT = 1.0f * sh_fusion_scale();                /* COMBO AERIEN */
    if (rlCoinHit && rl_cap(SH_PIE, RL_C_BANQUE)) {                                 /* Banque : 3 par vague */
        if (rlBanqueWave != wave) { rlBanqueWave = wave; rlBanque = 0; }
        if (rlBanque < 3) { rlBanque++; rl_ammo_plus(); snprintf(popText, sizeof popText, "BANQUE !"); popT = 1.0f; }
    }
    if (rl_cap(SH_ROQ, RL_C_ORBITAL)) fx_blast(rlAnyHit ? rlLastAt : end, 12, dmg * 0.25f, WP_RAIL);   /* explosion d'impact 12 m sur la derniere cible (dmg = x4 / 4) */
    if (rl_has(RL_JUMEAU) && frand() < 0.2f * rl_sc(RL_JUMEAU)) rl_extra(dmg, 0.012f);                 /* Rail jumeau : 20 % */
    if (allinActive && rlAllinStamp != skillUse[SK_ALLIN]) {                        /* une fois par salve All in One */
        rlAllinStamp = skillUse[SK_ALLIN];
        if (rl_cap(SH_AIO, RL_C_MULTI))                                             /* Rail multiple : 3 rails a 50 % */
            for (int k = 0; k < 3; k++) rl_extra(dmg * 0.5f, (k - 1) * 0.05f);
        if (rl_fus(RL_F_FUSILLADE)) {                                               /* FUSILLADE : un rail a 30 % par munition */
            int n = P.ammo[WP_RAIL] + 1;
            for (int k = 0; k < n; k++) rl_extra(dmg * 0.3f * sh_fusion_scale(), ((float)k - (float)(n - 1) * 0.5f) * 0.03f);
        }
    }
    shotHeads = 0;
}
/* joute reussie de la lance (CHEVALIER) */
static void vg_rail_joust(void) {
    if (rl_fus(RL_F_CHEVALIER)) rl_ammo_plus();
}
/* tick : suspension de chute (COMBO AERIEN), Rail orbital = une seule munition en reserve */
static void vg_rail_tick(float dt) {
    if (rlHoverT > 0) {
        rlHoverT -= dt;
        if (!P.grounded && !P.dead && P.vel.y < 0) P.vel.y = 0;
    }
    if (rl_cap(SH_ROQ, RL_C_ORBITAL) && P.ammo[WP_RAIL] > 1) P.ammo[WP_RAIL] = 1;
    for (int j = 0; j < MAXE; j++) {                      /* capstone RAIL>SHOTGUN : l'ouverture dure RL_OPEN_T s et s'eteint avec l'ennemi */
        if (rlOpenT[j] <= 0) continue;
        rlOpenT[j] -= dt;
        if (!E[j].alive || rlOpenT[j] <= 0) { rlOpenT[j] = 0; continue; }
        if (frand() < 0.3f)
            add_part(vadd(E[j].pos, vmul(rand_dir(), enemy_radius(E[j].type))), v3(0, 1.5f, 0), v3(1, 0.6f, 0.2f), 0.35f, 0.09f, 0);
    }
}
