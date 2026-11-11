/* VELOCITY - competences (touche A_SKILL) : PIECE, MARQUE, CHARGE, RICOCHET, ALL IN ONE, MIRAGE, AIMANT
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- Competences : utilisation (touche A_SKILL) ------------------------- */
static void net_fx_skill(int sub, int target, V3 a, V3 b);
static void skill_mark(void) {                            /* MARQUE : tir sans degats, la premiere cible touchee est marquee */
    V3 eye = eye_pos(), d = view_dir(), n;
    float tw = ray_world(eye, d, MARK_RANGE, &n), t;
    int te = -1, tp = -1;
    for (int i = 0; i < MAXE; i++)
        if (E[i].alive && ray_sphere(eye, d, E[i].pos, enemy_radius(E[i].type) + 0.3f, tw, &t)) { tw = t; te = i; tp = -1; }
    for (int i = 0; i < MAXP; i++) {
        V3 mn, mx;
        if (!np_target(i)) continue;
        np_aabb(i, 0.3f, &mn, &mx);
        if (ray_box(eye, d, mn, mx, tw, &t, NULL)) { tw = t; tp = i; te = -1; }
    }
    V3 end = vadd(eye, vmul(d, tw));
    P.skillCh -= 1; skillUse[SK_MARK]++;
    add_beam(muzzle(), end, v3(0.85f, 0.35f, 1), 0.3f);
    play(S_HIT, 0.45f, 0.7f);
    if (te >= 0) markE[te] = MARK_T;
    if (tp >= 0) markP[tp] = MARK_T;
    if (te >= 0 || tp >= 0) {
        burst(end, v3(0.85f, 0.35f, 1), 24, 5, 0.4f, 0.1f);
        snprintf(popText, sizeof popText, "MARQUE !"); popT = 0.8f;
    }
    if (botMode) netlog("marque : %s", tp >= 0 ? pname(tp) : te >= 0 ? "drone" : "ratee");
    if (dmMode) net_fx_skill(SK_MARK, tp, muzzle(), end);
}
static int allin_ok(int w) { return w == WP_RAIL || w == WP_ROCKET || w == WP_SHOTGUN || w == WP_KINETIC || w == WP_TESLA; }
static float allin_half(int n) {                          /* ALL IN ONE : demi-angle de l'eventail (radians) */
    return cp_half_deg(fminf(ALLIN_SPREAD_BASE + ALLIN_SPREAD_PER * (n - 1), ALLIN_SPREAD_MAX)) * PI / 180;
}
static void fire_weapon(void);
static int allinLast;                                     /* projectiles du dernier ALL IN ONE (tests, journal) */
static float allinSpan;                                   /* angle entre le premier et le dernier projectile (radians) */
static void allin_volley(int w, int n) {                  /* n tirs de l'arme w en eventail regulier (n >= 2), munition posee a chaque tir */
    float half = allin_half(n), gap = 2 * half / (n - 1), y0 = P.yaw, p0 = P.pitch;
    float jit = (frand() - 0.5f) * gap * 0.5f;            /* decalage commun d'au plus la moitie de l'ecart */
    V3 first = view_dir(), last = first;
    for (int k = 0; k < n; k++) {
        P.yaw = y0 + (-half + gap * k + jit) / fmaxf(cosf(p0), 0.3f);
        P.pitch = p0;
        P.ammo[w] = n - k;
        if (!k) first = view_dir();
        last = view_dir();
        fire_weapon();
    }
    P.yaw = y0; P.pitch = p0;
    allinSpan = acosf(clampf(vdot(first, last), -1, 1));
}
static void skill_allin(void) {                           /* toute la munition de l'arme en main, en eventail regulier */
    int w = P.weapon, n = P.ammo[w];
    int blocked = P.switchT > 0 || P.shieldState == SHS_UP || tesla_stunned();   /* comme un tir normal */
    if (w == WP_TESLA) {                                  /* Tesla : un eclair inevitable, toute la batterie (tesla.c) */
        if (blocked) {
            play(S_HIT, 0.5f, 0.45f);
            snprintf(popText, sizeof popText, "ALL IN ONE : PAS MAINTENANT"); popT = 1.0f;
        } else if (tesla_bolt()) { P.skillCh -= 1; skillUse[SK_ALLIN]++; }   /* sinon : message, la charge est gardee */
        return;
    }
    if (!allin_ok(w) || n < 2 || blocked) {
        play(S_HIT, 0.5f, 0.45f);
        snprintf(popText, sizeof popText, "%s", !allin_ok(w) ? "ALL IN ONE : ARME INCOMPATIBLE" :   /* 31 car. au plus */
                 n < 2 ? "ALL IN ONE : 2 MUNITIONS MIN." : "ALL IN ONE : PAS MAINTENANT");
        popT = 1.0f;
        return;
    }
    P.skillCh -= 1; skillUse[SK_ALLIN]++;
    n += cp_allin_extra(w, n);                            /* VAGUES : munition comptee en plus, salve complete */
    cp_salve_begin(w, n);
    allinActive = 1;
    allin_volley(w, n);
    allinActive = 0;
    P.ammo[w] = cp_salve_refund(w); allinLast = n;        /* VAGUES : jauge rendue (0 sans boutique) */
    P.fireCd = fmaxf(P.fireCd, cp_salve_cd(wpCd[w]));     /* l'arme est vide : pas de tir dans le meme tick */
    cp_salve_end(w, n);
    shake += 0.2f; P.recoil = 2;
    snprintf(popText, sizeof popText, "ALL IN ONE x%d", n); popT = 1.0f;
    if (botMode) netlog("all in one : %d projectiles (%s)", n, ammoDef[w].name);
    if (dmMode) net_fx_skill(SK_ALLIN, -1, eye_pos(), v3((float)n, 0, 0));
}
static void skill_decoy(void) {                           /* MIRAGE : leurre a ma place, je repars */
    for (int i = 0; i < MAXCOIN; i++) {
        Coin *c = &CO[i];
        if (c->alive) continue;
        memset(c, 0, sizeof *c);
        c->alive = 1; c->decoy = 1; c->owner = myId; c->life = DECOY_T; c->spin = P.yaw;
        c->pos = v3(P.pos.x, P.pos.y + pheight() * 0.5f, P.pos.z);
        c->netId = ((unsigned int)(myId + 1) << 24) | (++coinCounter & 0xFFFFFF);
        P.skillCh -= 1; skillUse[SK_DECOY]++;
        burst(c->pos, v3(0.4f, 0.85f, 1), 20, 4, 0.3f, 0.1f);
        play(S_DASH, 0.5f, 1.4f);
        if (dmMode) net_fx_coin(4, c->netId, c->pos, v3(P.yaw, 0, 0));
        return;
    }
}
static void skill_aim(void) {                             /* AIMANT : aimant de visee pendant AIM_T (aim_assist) */
    P.skillCh -= 1; P.aimT = AIM_T; skillUse[SK_AIM]++;
    play(S_PAD, 0.5f, 1.8f);
    burst(pcenter(), v3(1, 0.95f, 0.45f), 24, 4, 0.4f, 0.1f);
    snprintf(popText, sizeof popText, "AIMANT !"); popT = 0.8f;
    if (botMode) netlog("competence : aimant (%.0f s)", AIM_T);
    if (dmMode) net_fx_skill(SK_AIM, -1, pcenter(), v3(0, 0, 0));   /* les autres voient l'activation */
}
static void skill_ricochet(void) {                        /* RICOCHET : la fenetre s'ouvre, tout ce que je tire rebondit RIC_DURATION s */
    P.skillCh -= 1; P.ricT = RIC_DURATION; P.ricArmed = 1; skillUse[SK_RICOCHET]++;
    play(S_PAD, 0.5f, 2.0f);
    burst(pcenter(), v3(0.4f, 1, 0.7f), 26, 5, 0.4f, 0.1f);
    snprintf(popText, sizeof popText, "RICOCHET !"); popT = 0.8f;
    if (botMode) netlog("competence : ricochet ouvert (%.0f s)", RIC_DURATION);
    if (dmMode) net_fx_skill(SK_RICOCHET, -1, pcenter(), v3(RIC_DURATION, 0, 0));   /* les autres voient l'ouverture */
}
static void skill_tick(float dt, int press, int held) {
    const SkillDef *d = &skillDef[P.skill];
    if (P.ricT > 0) {                                     /* RICOCHET : fenetre ouverte (P.ricArmed la double pour le HUD, les bots, la boutique) */
        P.ricT -= dt;
        vgRicT = fmaxf(vgRicT, 0.3f);                     /* VAGUES : les touches de la fenetre reviennent au chemin RICOCHET */
        if (P.ricT <= 0) {
            P.ricT = 0;
            if (!P.dead) { snprintf(popText, sizeof popText, "RICOCHET FINI"); popT = 0.8f; }
        }
    }
    P.ricArmed = P.ricT > 0;
    int cmax = cp_max(P.skill, d->max);                  /* VAGUES : charges et recharge de la boutique */
    if (!P.ricArmed && P.skillCh < cmax) P.skillCh = fminf((float)cmax, P.skillCh + dt / cp_recharge(P.skill, d->recharge));   /* RICOCHET : la recharge repart a la fin */
    for (int i = 0; i < MAXP; i++) if (markP[i] > 0) { markP[i] -= dt; if (!np_target(i)) markP[i] = 0; }   /* mort : marque perdue */
    for (int i = 0; i < MAXE; i++) if (markE[i] > 0) { markE[i] -= dt; if (!E[i].alive) markE[i] = 0; }
    if (markedMeT > 0) markedMeT -= dt;
    if (P.chargeT > 0) P.chargeT -= dt;
    if (P.aimT > 0) P.aimT -= dt;
    if (P.hp < lastHpSeen - 0.5f && (P.chargeT > 0 || P.chargeHold > 0)) {   /* CHARGE : perdue si on est touche */
        P.chargeT = P.chargeHold = 0;
        snprintf(popText, sizeof popText, "CHARGE PERDUE"); popT = 0.8f;
        if (botMode) netlog("competence : charge perdue (touche)");
    }
    lastHpSeen = P.hp;
    if (P.dead) { P.chargeT = P.chargeHold = 0; P.aimT = 0; P.ricT = 0; P.ricArmed = 0; return; }
    switch (P.skill) {
    case SK_COIN:  if (press && P.skillCh >= 1) { throw_coin(); skillUse[SK_COIN]++; } break;
    case SK_MARK:  if (press && P.skillCh >= 1) skill_mark(); break;
    case SK_ALLIN: if (press && P.skillCh >= 1) skill_allin(); break;
    case SK_DECOY: if (press && P.skillCh >= 1) skill_decoy(); break;
    case SK_AIM:   if (press && P.skillCh >= 1 && P.aimT <= 0) skill_aim(); break;
    case SK_RICOCHET: if (press && P.skillCh >= 1 && !P.ricArmed) skill_ricochet(); break;   /* pas de 2e ouverture pendant la fenetre */
    case SK_CHARGE:
        if (held && P.skillCh >= 1 && P.chargeT <= 0) {
            P.chargeHold += dt;
            if (P.chargeHold >= CHARGE_HOLD) {
                P.skillCh -= 1; P.chargeHold = 0; P.chargeT = CHARGE_WINDOW; skillUse[SK_CHARGE]++;
                play(S_PAD, 0.6f, 1.3f);
                burst(pcenter(), v3(1, 0.55f, 0.2f), 30, 5, 0.4f, 0.1f);
                snprintf(popText, sizeof popText, "CHARGE !"); popT = 0.8f;
                if (botMode) netlog("competence : charge prete");
                if (dmMode) net_fx_skill(SK_CHARGE, -1, pcenter(), v3(0, 0, 0));
            }
        } else P.chargeHold = 0;
        break;
    }
}
/* --- fin Competences --- */
