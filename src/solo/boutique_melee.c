/* VELOCITY - mode VAGUES : effets de la boutique pour la Lance et l'Impulsion (groupe MELEE)
   Morceau de main.c : inclus dans l'ordre (apres ameliorations.c), ne se compile pas seul.
   Les fonctions vgm_* sont appelees depuis src/armes/lance.c, src/armes/poings.c et src/combat/degats.c (declarations
   en avance dans src/monde/entites.c). Tout est inerte hors du mode VAGUES (sh_sum / vgm_cap testent vagOn). */

static float vgmRueeT, vgmFreeT, vgmFrenT, vgmFollowT;
static int vgmStreak, vgmChainUsed, vgmFrenStacks, vgmFollowN, vgmFollowTgt;
static int vgmStat[8];                                    /* tests : 0 estocs en plus, 1 plombs, 2 rails, 3 roquettes, 4 pieces */

static void vgm_reset(void) {
    vgmRueeT = vgmFreeT = vgmFrenT = vgmFollowT = 0;
    vgmStreak = vgmChainUsed = vgmFrenStacks = vgmFollowN = 0; vgmFollowTgt = -1;
}
#define UP_LAN UP_MELEE(MK_LANCE)
#define UP_IMP UP_MELEE(MK_FIST)

/* plus grande valeur a (x echelle du niveau de l'etape) d'un genre parmi les etapes achetees du chemin u (0 si aucune) */
static float vgm_max(int u, int kind) {
    int sd = sh_seed_of_up(u);
    float m = 0;
    if (!vagOn || sd < 0 || !up_equipped(u)) return 0;
    for (int k = 0; k < 20; k++)
        if (sh_owns(sd, k) && shSteps[sd][k].kind == kind && (float)shSteps[sd][k].a * sh_scale(sd, k) > m) m = (float)shSteps[sd][k].a * sh_scale(sd, k);
    return m;
}
/* duree base abaissee par les etapes de ce genre : a = nouvelle duree en centiemes ; le niveau avance de (base - a) x echelle.
   Le meilleur resultat parmi les etapes achetees (base si aucune) */
static float vgm_lower(int u, int kind, float base) {
    int sd = sh_seed_of_up(u);
    float best = base;
    if (!vagOn || sd < 0 || !up_equipped(u)) return base;
    for (int k = 0; k < 20; k++) {
        if (!sh_owns(sd, k) || shSteps[sd][k].kind != kind) continue;
        float v = (float)shSteps[sd][k].a * 0.01f;
        if (v >= base) continue;
        v = base - (base - v) * sh_scale(sd, k);
        if (v < best) best = v;
    }
    return best;
}
static int vgm_step_b(int u, int kind) {                  /* parametre b de la premiere etape achetee de ce genre (brut) */
    int sd = sh_seed_of_up(u);
    if (!vagOn || sd < 0 || !up_equipped(u)) return 0;
    for (int k = 0; k < 20; k++) if (sh_owns(sd, k) && shSteps[sd][k].kind == kind) return shSteps[sd][k].b;
    return 0;
}
static float vgm_step_sc(int u, int kind) {               /* echelle de la premiere etape achetee de ce genre */
    int sd = sh_seed_of_up(u);
    if (!vagOn || sd < 0 || !up_equipped(u)) return 0;
    for (int k = 0; k < 20; k++) if (sh_owns(sd, k) && shSteps[sd][k].kind == kind) return sh_scale(sd, k);
    return 0;
}
/* capstone de la branche seed -> partner : achete, et les deux graines equipees */
static int vgm_cap(int seed, int partner, int kind) {
    int b = sh_branch(seed, partner);
    return vagOn && b >= 0 && shCap[b] && shBr[b].capKind == kind && sh_on(seed) && sh_on(partner);
}
static int vgm_cap_a(int seed, int partner, int kind) {
    int b = sh_branch(seed, partner);
    return vgm_cap(seed, partner, kind) ? shBr[b].capA : 0;
}
static void vgm_hit(int j, float dmg, V3 knock) {         /* degats comptes pour la melee equipee */
    int s0 = vgSrc;
    vgSrc = UP_MELEE(P.meleeKind);
    damage_enemy(j, dmg, knock);
    vgSrc = s0;
}

/* --- capstones communs --- */
static void vgm_pellets(V3 o, V3 f, int n, float dmg) {   /* plombs du Shotgun : cone, perte avec la distance */
    for (int k = 0; k < n; k++) {
        V3 d = vnorm(vadd(f, vmul(rand_dir(), frange(0, SG_HALF_ANGLE))));
        float tw = ray_world(o, d, SG_RANGE, 0), bt = tw;
        int bj = -1;
        for (int j = 0; j < MAXE; j++) {
            float t;
            if (E[j].alive && E[j].summoned != 2 && ray_sphere(o, d, E[j].pos, enemy_radius(E[j].type), tw, &t) && t < bt) { bt = t; bj = j; }
        }
        add_beam(vadd(o, vmul(d, 0.8f)), vadd(o, vmul(d, bt)), v3(1, 0.85f, 0.35f), 0.08f);
        if (bj < 0) continue;
        float m = bt <= SG_NEAR ? 1 : bt >= SG_FAR ? SG_FAR_MULT : 1 + (SG_FAR_MULT - 1) * (bt - SG_NEAR) / (SG_FAR - SG_NEAR);
        fx_damage(bj, dmg * m, v3(0, 0, 0), UP_MELEE(P.meleeKind));
        vgmStat[1]++;
    }
}
static void vgm_rail(V3 o, V3 f) {                        /* un rail qui traverse tout */
    float tw = ray_world(o, f, 150, 0);
    for (int j = 0; j < MAXE; j++) {
        float t;
        if (E[j].alive && E[j].summoned != 2 && ray_sphere(o, f, E[j].pos, enemy_radius(E[j].type) + 0.3f, tw, &t))
            fx_damage(j, RAIL_DMG, vmul(f, 6), UP_MELEE(P.meleeKind));
    }
    add_beam(vadd(o, vmul(f, 0.6f)), vadd(o, vmul(f, tw)), v3(0.4f, 0.85f, 1), 0.25f);
    play(S_RAIL, 0.5f, 1.2f);
    vgmStat[2]++;
}
/* capstones de la Lance sur une touche (estoc ou joute) sur l'ennemi i */
static void vgm_lance_caps(int i, int joust) {
    int boom = vgm_cap_a(SH_LAN, SH_ROQ, BK_MEL_CAP_BOOM);
    if (boom) fx_blast(E[i].pos, (float)boom, 50, UP_LAN);
    if (vgm_cap(SH_LAN, SH_SHO, BK_MEL_CAP_PLOMB)) vgm_pellets(eye_pos(), view_dir(), vgm_cap_a(SH_LAN, SH_SHO, BK_MEL_CAP_PLOMB), SG_PELLET_DMG);
    if (joust && vgm_cap(SH_LAN, SH_RAIL, BK_MEL_CAP_RAIL)) vgm_rail(eye_pos(), view_dir());
}

/* --- Lance --- */
static float vgm_vc(float vc) { return vc * (1 + vgm_max(UP_LAN, BK_MEL_VELO) * 0.01f); }
static float vgm_joust_mul(void) {                        /* Tournoi : +20 % par joute d'affilee, 60 % au plus */
    float a = vgm_max(UP_LAN, BK_MEL_TOURNOI);
    if (a <= 0) return 1;
    return 1 + fminf((float)vgm_step_b(UP_LAN, BK_MEL_TOURNOI) * vgm_step_sc(UP_LAN, BK_MEL_TOURNOI), a * (float)vgmStreak) * 0.01f;
}
static void vgm_joust_whiff(void) { vgmStreak = 0; }
static void vgm_joust_hit(int i) {                        /* apres une joute reussie sur l'ennemi i */
    if (!vagOn || P.meleeKind != MK_LANCE) return;
    float pts = sh_sum(UP_LAN, BK_MEL_JPTS);
    if (E[i].elite) pts += sh_sum(UP_LAN, BK_MEL_JELITE);
    if (pts > 0) ammo_points(pts, AP_HIT);
    float soin = sh_sum(UP_LAN, BK_MEL_JSOIN);
    if (soin > 0) vg_heal(soin);
    if (vgm_max(UP_LAN, BK_MEL_TOURNOI) > 0) vgmStreak++;
    float cd = LANCE_HIT_CD - sh_sum(UP_LAN, BK_MEL_JCD) * 0.01f;
    float ecl = vgm_max(UP_LAN, BK_MEL_ECLAIR);
    if (ecl > 0) cd *= 1 - ecl * 0.01f;
    if (vgm_max(UP_LAN, BK_MEL_JCHAIN) > 0) { if (!vgmChainUsed) { cd = 0; vgmChainUsed = 1; } else vgmChainUsed = 0; }
    float ruee = vgm_max(UP_LAN, BK_MEL_RUEE);
    if (ruee > 0) vgmRueeT = ruee;
    P.meleeCd = fmaxf(0, cd);
    if (vgmRueeT > 0) P.meleeCd = 0;
    vgm_lance_caps(i, 1);
}
static int vgm_estoc_pts(void) { return (int)(st(SH_LAN, STAT_MUR, 0) + 0.5f); }   /* Moisson : stat MUR (points de jauge rendus par estoc) */
static void vgm_estoc_hit(int i) {                        /* estoc touche : estocs a la suite et capstones */
    if (!vagOn || P.meleeKind != MK_LANCE) return;
    int n = (int)(vgm_max(UP_LAN, BK_MEL_ESTOCS) + 0.5f), c = vgm_cap_a(SH_LAN, SH_AIO, BK_MEL_CAP_ESTOCS);
    if (c > n) n = c;
    if (n > 0 && vgmFollowN <= 0) { vgmFollowN = n; vgmFollowT = 0.12f; vgmFollowTgt = i; }
    vgm_lance_caps(i, 0);
}
static float vgm_thrust_cd(float cd) {
    cd *= 1 - vgm_max(UP_LAN, BK_MEL_ECD) * 0.01f;
    return cd * (1 - vgm_max(UP_LAN, BK_MEL_ECLAIR) * 0.01f);
}
static float vgm_whiff_cd(float cd) {                     /* RELEVE : un estoc dans le vide recharge en 0,25 s */
    return vgm_lower(UP_LAN, BK_MEL_RELEVE, cd);
}
static float vgm_estoc_anim(float t) {
    t *= 1 - vgm_max(UP_LAN, BK_MEL_EANI) * 0.01f;
    return t * (1 - vgm_max(UP_LAN, BK_MEL_ECLAIR) * 0.01f);
}
static float vgm_tap(float t) { return vgm_lower(UP_LAN, BK_MEL_TAP, t); }
static void vgm_follow(float dt) {                        /* estocs a 60 % dans la foulee d'un estoc touche */
    if (vgmFollowN <= 0) return;
    if (P.dead || P.meleeKind != MK_LANCE) { vgmFollowN = 0; return; }
    if ((vgmFollowT -= dt) > 0) return;
    vgmFollowT = 0.12f; vgmFollowN--;
    V3 eye = eye_pos();
    int j = vgmFollowTgt;
    if (j < 0 || j >= MAXE || !E[j].alive || vlen(vsub(E[j].pos, eye)) - enemy_radius(E[j].type) > LANCE_THRUST_RANGE + 1) j = fx_nearest(eye, LANCE_THRUST_RANGE + 1, -1, -1);
    if (j < 0) { vgmFollowN = 0; return; }
    vgmFollowTgt = j;
    Enemy *e = &E[j];
    burst(e->pos, v3(1, 0.85f, 0.6f), 10, 6, 0.25f, 0.08f);
    play(S_PUNCH, 0.7f, 0.9f);
    vgmStat[0]++;
    vgm_hit(j, lance_thrust_dmg(rel_speed(e->vel)) * 0.6f, vmul(view_dir(), 6));
    vgm_lance_caps(j, 0);
}

/* --- tick commun --- */
static void vgm_melee_tick(float dt) {
    if (!vagOn) { vgm_reset(); return; }
    if (vgmRueeT > 0) { vgmRueeT -= dt; if (P.meleeKind == MK_LANCE) P.meleeCd = 0; }
    if (vgmFreeT > 0) vgmFreeT -= dt;
    if (vgmFrenT > 0 && (vgmFrenT -= dt) <= 0) vgmFrenStacks = 0;
    vgm_follow(dt);
}
static void vgm_kill(int i) {
    (void)i;
    if (!vagOn) return;
    int mu = UP_MELEE(P.meleeKind);
    if (vgSrc == mu) { float p = sh_sum(mu, BK_MEL_KPTS); if (p > 0) ammo_points(p, AP_KILL); }
    float f = vgm_max(UP_IMP, BK_MEL_FFREE);
    if (f > 0) vgmFreeT = f;
}

/* --- Impulsion --- */
static int vgm_fist_n(void) {
    int n = 3, m = (int)(vgm_max(UP_IMP, BK_MEL_FN) + 0.5f), c = vgm_cap_a(SH_IMP, SH_AIO, BK_MEL_CAP_COUP9);
    if (m > n) n = m;
    if (c > n) n = c;
    return n;
}
static int vgm_fist_up(int step, int n) { return n >= 8 ? (step + 1) % 4 == 0 : step == n - 1; }
static float vgm_fist_gap(float g) {
    g = vgm_lower(UP_IMP, BK_MEL_FGAP, g);
    if (!P.grounded) g *= 1 - vgm_max(UP_IMP, BK_MEL_FAIR) * 0.01f;
    float fr = vgm_max(UP_IMP, BK_MEL_FFREN);
    if (fr > 0 && vgmFrenStacks > 0) g *= 1 - fr * 0.01f * vgmFrenStacks;
    return g;
}
static float vgm_fist_end(float cd) {
    if (vgmFreeT > 0) return 0;
    cd *= 1 - vgm_max(UP_IMP, BK_MEL_FEND) * 0.01f;
    float ch = vgm_max(UP_IMP, BK_MEL_FCHAIN);
    if (ch > 0) cd = fmaxf(0, cd - ch * 0.01f);
    return cd;
}
static float vgm_fist_whiff(float cd) {
    if (vgmFreeT > 0) return 0;
    return cd * (1 - vgm_max(UP_IMP, BK_MEL_FWHIFF) * 0.01f);
}
static int vgm_fist_free(void) { return vagOn && vgmFreeT > 0; }
static void vgm_fist_full(int landed) {                   /* combo complet (N coups) */
    if (!vagOn || P.meleeKind != MK_FIST) return;
    float fr = vgm_max(UP_IMP, BK_MEL_FFREN);
    if (fr > 0) { int mx = vgm_step_b(UP_IMP, BK_MEL_FFREN); if (vgmFrenStacks < mx) vgmFrenStacks++; vgmFrenT = 6; }
    float pts = sh_sum(UP_IMP, BK_MEL_FPTS);
    if (landed && pts > 0) ammo_points(pts, AP_HIT);
}
static void vgm_fist_hit(int up) {                        /* un coup touche */
    if (!vagOn || P.meleeKind != MK_FIST) return;
    float pts = st(SH_IMP, STAT_MUR, 0);                  /* Cueillette : stat MUR (points de jauge rendus par coup) */
    if (!P.grounded) pts += sh_sum(UP_IMP, BK_MEL_FAIRPTS);
    if (pts > 0) ammo_points(pts, AP_HIT);
    if (up) {
        float soin = sh_sum(UP_IMP, BK_MEL_FSOIN);
        if (soin > 0) vg_heal(soin);
        if (vgm_cap(SH_IMP, SH_RAIL, BK_MEL_CAP_UPRAIL) && (P.loadout[0] == WP_RAIL || P.loadout[1] == WP_RAIL)) {
            P.ammo[WP_RAIL] = ammoDef[WP_RAIL].max; play(S_PAD, 0.5f, 2.6f);
        }
    }
}
static void vgm_fist_swing(int step) {                    /* chaque coup donne (touche ou non) */
    if (!vagOn || P.meleeKind != MK_FIST) return;
    int rk = vgm_cap_a(SH_IMP, SH_ROQ, BK_MEL_CAP_ROQ);
    if (rk) {
        Proj *p = spawn_proj(PR_ROCKET, eye_pos(), vmul(view_dir(), ROCKET_SPEED));
        if (p) { p->mult = rk * 0.01f; vgmStat[3]++; play(S_ROCKET, 0.5f, 1.3f); }
    }
    int pl = vgm_cap_a(SH_IMP, SH_SHO, BK_MEL_CAP_PLOMB3);
    if (pl && step % 3 == 2) vgm_pellets(eye_pos(), view_dir(), pl, SG_PELLET_DMG);
    if (vgm_cap(SH_IMP, SH_PIE, BK_MEL_CAP_PIECE)) {      /* le coup frappe une piece proche : elle file vers l'ennemi */
        V3 eye = eye_pos(), f = view_dir();
        int ci = -1; float bestAx = 0.5f;
        for (int i = 0; i < MAXCOIN; i++) {
            Coin *c = &CO[i];
            if (!c->alive || c->decoy || c->owner != myId || c->flailStruck || c->teslaT > 0) continue;
            V3 d = vsub(c->pos, eye);
            if (vlen(d) > FIST_RANGE + 1.5f) continue;
            float ax = vdot(vnorm(d), f);
            if (ax > bestAx) { bestAx = ax; ci = i; }
        }
        if (ci >= 0) {
            Coin *c = &CO[ci];
            int t = fx_nearest(c->pos, FLAIL_COIN_SEEK, -1, -1);
            V3 aim = t >= 0 ? vnorm(vsub(E[t].pos, c->pos)) : f;
            c->flailStruck = 1; c->vel = vmul(aim, FLAIL_COIN_SPEED); c->life = FLAIL_COIN_LIFE;
            for (int k = 0; k < 12; k++) add_part(c->pos, vmul(rand_dir(), 5), v3(1, 0.85f, 0.3f), 0.25f, 0.08f, 0);
            play(S_FLAIL_COIN, 0.8f, 1.0f);
            vgmStat[4]++;
        }
    }
}
