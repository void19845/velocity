/* VELOCITY - Egide (bouclier)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- Egide (bouclier) ---------------------------------------------------- */
/* Maintenir la melee = bouclier leve (bloque les coups de face, jauge de 120 tenue par l'hote) ; un tap = coup de bouclier.
 * La levee ouvre une fenetre de parry ordinaire (0,15 s) si le bouclier etait baisse depuis 0,3 s : c'est le parry parfait.
 * Leve pendant la traction du grappin : arriver au contact donne un coup de bouclier gratuit (belier). */
/* coup de bouclier : ram = belier (sans recharge, degats de l'impact du grappin, la cible accrochee touchee d'office) */
static void shield_bash(int ram) {
    V3 eye = eye_pos(), f = view_dir();
    float sp = vlen(P.vel), base = ram ? GRAPPLE_STRIKE_DMG : SHIELD_BASH_DMG;
    int hits = 0, blk = 0;
    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &E[i];
        if (!e->alive) continue;
        V3 d = vsub(e->pos, eye);
        float dist = vlen(d), reach = dist - enemy_radius(e->type);
        int forced = ram && P.hookEnemy == i + 1;
        if (!forced && (reach > SHIELD_BASH_RANGE || (reach > 0.8f && vdot(vmul(d, 1.0f / dist), f) < MELEE_CONE))) continue;
        float rel = rel_speed(e->vel);
        if (e->type == EN_RUSHER) {                       /* kamikaze : explose contre le bouclier, comme un parry */
            V3 at = e->pos;
            damage_enemy(i, 9999, vmul(f, 10));
            parry_feedback(at);
            melee_parry_points(PTS_PARRY);
        } else {
            damage_enemy(i, base + SHIELD_BASH_PER_MS * rel, vmul(f, SHIELD_BASH_KNOCK));
            if (!e->alive) { if (!vagOn) P.hp = fminf(100, P.hp + 15); ammo_points(PTS_MELEE_KILL, AP_KILL); }   /* VAGUES : pas de soin sur kill */
        }
        hits++;
    }
    for (int i = 0; i < MAXP; i++) {                     /* LAN : les autres joueurs */
        if (!np_target(i)) continue;
        V3 c = np_center(i), d = vsub(c, eye);
        float dist = vlen(d), reach = dist - 0.5f;
        int forced = ram && P.hookPlayer == i + 1;
        if (!forced && (reach > SHIELD_BASH_RANGE || (reach > 0.8f && vdot(vmul(d, 1.0f / dist), f) < MELEE_CONE))) continue;
        float rel = rel_speed(np_vel(i));
        if (!hit_player(i, (base + SHIELD_BASH_PER_MS * rel) * PVP_SCALE, vadd(vmul(f, SHIELD_BASH_KNOCK), v3(0, 3, 0)), W_SHIELD,
                        vnorm(d))) { blk++; continue; }   /* bouclier contre bouclier */
        burst(c, v3(0.6f, 0.85f, 1), 20, 8, 0.3f, 0.12f);
        hits++;
    }
    for (int k = 0; k < 14; k++)                          /* poussee : un mur de particules droit devant */
        add_part(vadd(eye, vadd(vmul(f, 0.9f + 0.05f * k), vmul(rand_dir(), 0.35f))), vmul(f, 5), v3(0.6f, 0.85f, 1), 0.15f, 0.07f, 0);
    if (dmMode) net_fx_melee(eye, f, sp);                 /* EV_MELEE : le recepteur lit meleeKind dans pad */
    P.meleeT = SHIELD_BASH_ANIM;
    shieldBashes++;
    if (hits) {
        shieldBashHits++;
        P.dashCharge = fminf(2, P.dashCharge + 1);
        melee_hit_points(meleePts[MK_SHIELD]);
        hitMarker = 0.18f; shake += 0.2f;
        hitstop = fmaxf(hitstop, 0.05f);
        play(S_SHIELD_BASH, 0.9f, frange(0.95f, 1.05f));
    } else play(S_SHIELD_BASH, 0.45f, 1.4f);
    if (!ram) P.meleeCd = hits || blk ? SHIELD_BASH_CD : SHIELD_BASH_WHIFF_CD;
    if (botMode) netlog("bouclier : %s, %d touche(s), %d bloque(s)", ram ? "belier" : "coup de bouclier", hits, blk);
}
/* belier : bouclier leve au contact de la cible accrochee ; remplace l'impact du grappin (une seule touche, W_SHIELD) */
static void shield_ram(void) {
    V3 hit = P.anchor, dir = vnorm(vsub(P.anchor, pcenter()));
    P.shieldRamDone = 1; shieldRams++;
    shield_bash(1);
    impact_frame(hit, 0.07f);
    grapple_bounce(hit, dir);
}
static void shield_ram_check(void) {
    V3 eye = eye_pos(), c;
    float r;
    if (P.hookEnemy) { Enemy *e = &E[P.hookEnemy - 1]; if (!e->alive) return; c = e->pos; r = enemy_radius(e->type); }
    else if (P.hookPlayer) { if (!np_target(P.hookPlayer - 1)) return; c = np_center(P.hookPlayer - 1); r = 0.5f; }
    else return;
    if (vlen(vsub(c, eye)) - r <= SHIELD_BASH_RANGE) shield_ram();
}
/* machine a etats (a la place de melee()) : press = appui du tick, held = touche tenue */
static void shield_tick(float dt, int press, int held) {
    P.shieldBrokenT -= dt; P.shieldRipT -= dt; P.shieldHitT -= dt;
    P.shieldAnim = clampf(P.shieldAnim + (P.shieldState == SHS_UP ? dt : -dt) / 0.08f, 0, 1);   /* levee : 0,08 s (visuel) */
    if (P.dead) { P.shieldState = SHS_DOWN; P.shieldHeldT = 0; return; }
    if (P.shieldState == SHS_BROKEN && P.shieldBrokenT <= 0) { P.shieldState = SHS_DOWN; P.shieldDownT = 0; }
    if (P.shieldState == SHS_UP && (P.shieldRipT > 0 || P.shieldBrokenT > 0)) { P.shieldState = SHS_DOWN; P.shieldDownT = 0; }
    if (!dmMode && P.shieldState == SHS_UP && P.shieldGauge <= 0) shield_local_break();   /* solo : jauge vide (LAN : SHE_BREAK) */
    if (press && P.shieldState == SHS_DOWN && P.shieldRipT <= 0 && P.shieldBrokenT <= 0) {   /* levee instantanee */
        P.shieldState = SHS_UP; P.shieldHeldT = 0; P.sliding = 0; shieldRaises++;
        melee_start_swing();                              /* une levee = un "coup" : points de parry et de touche une fois */
        if (P.shieldDownT >= SHIELD_REARM) { P.parryWin = SHIELD_PARRY_WIN; P.swingLanded = 1; }   /* fenetre parfaite */
        play(S_SHIELD_UP, 0.5f, 1.0f);
    } else if (press && P.shieldState != SHS_UP) play(S_HIT, 0.4f, 0.5f);   /* brise ou arrache : clic sec */
    if (P.shieldState == SHS_UP) {
        P.shieldHeldT += dt; shieldUpSum += dt;
        if (!held) {                                      /* relache : coup de bouclier si c'etait un tap */
            P.shieldState = SHS_DOWN; P.shieldDownT = 0;
            if (P.shieldHeldT < SHIELD_TAP && P.meleeCd <= 0) shield_bash(0);
        } else if (P.hook == HOOK_ATTACHED && !P.shieldRamDone) shield_ram_check();
    } else {
        P.shieldDownT += dt;
        if (P.shieldState == SHS_DOWN) P.shieldGauge = fminf(SHIELD_GAUGE_MAX, P.shieldGauge + SHIELD_REGEN * dt);   /* LAN : prediction */
    }
    if (P.hook != HOOK_ATTACHED) P.shieldRamDone = 0;
}
/* clients : jauge miroir des autres porteurs (rendu), regeneree d'apres ST_SHIELD ; l'hote le fait dans host_tick */
static void shield_remote_tick(float dt) {
    if (netMode == NET_HOST) return;
    for (int i = 0; i < MAXP; i++) {
        NetPlayer *n = &NP[i];
        if (i == myId || !n->active) continue;
        n->shieldBrokenT -= dt; n->shieldRipT -= dt;
        if (n->alive && !(n->st.pad & ST_SHIELD) && n->shieldBrokenT <= 0)
            n->shieldGauge = fminf(SHIELD_GAUGE_MAX, n->shieldGauge + SHIELD_REGEN * dt);
    }
}
/* --- fin Egide --- */
