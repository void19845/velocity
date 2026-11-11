/* VELOCITY - Lance de joute
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* --- Lance --- de joute : estoc a l'appui (fenetre de parry normale) ; */
/* touche tenue 0,15 s : lance couchee (2 s max, sans parry). A chaque   */
/* tick, la premiere cible du cone qui se rapproche a 10 m/s ou plus est */
/* touchee toute seule : 10 + 5 x v_c. Elle recoit 60 % de ta vitesse,   */
/* tu en gardes la moitie. Le grappin sert de monture : la joute part    */
/* avant l'impact du crochet et le consomme.                             */
/* ------------------------------------------------------------------ */
static int lanceThrusts, lanceThrustHits, lanceCouches, lanceJousts, lanceJoustHook, lanceWhiffs;   /* journal du bot */
static float lanceVcSum;

static float lance_dmg(float vc) { return LANCE_DMG + LANCE_DMG_PER_MS * fminf(vc, LANCE_VC_CAP); }
static float lance_thrust_dmg(float rel) { return LANCE_THRUST_DMG + LANCE_THRUST_PER_MS * rel; }

/* effets visuels et sonores d'une joute reussie */
static void lance_juice(V3 c, V3 dir, float vc) {
    if (vc >= 15) impact_frame(c, 0.08f);
    hitstop = fmaxf(hitstop, fminf(0.06f + 0.002f * vc, 0.14f));
    shake += 0.3f; P.fovKick += 10; hitMarker = 0.18f;
    for (int k = 0; k < 40; k++)
        add_part(c, vadd(vmul(dir, frange(4, 14)), vmul(rand_dir(), 4)), (k & 1) ? v3(1, 0.7f, 0.3f) : v3(1, 0.95f, 0.8f),
                 0.4f, 0.1f, 4);
    shards(c, v3(0.75f, 0.6f, 0.4f), 10, 12);
    play(S_LANCE_HIT, 0.95f, frange(0.95f, 1.05f));
}

/* la lance se releve apres une joute : tu gardes la moitie de ta vitesse, recharge, +1 dash, points de joute */
static void lance_win(void) {
    P.vel = vmul(P.vel, LANCE_KEEP);
    P.lanceState = LS_IDLE; P.meleeCd = LANCE_HIT_CD; P.lanceVc = 0;
    P.dashCharge = fminf(2, P.dashCharge + 1);
    melee_hit_points(PTS_LANCE_JOUST);
    if (vagOn) roq_joust();                               /* VAGUES : LANCE-ROQUETTES */
}

/* joute annulee par l'hote (choc perdu, double desarconnement) : la touche ne compte plus, on rend ses points */
static void lance_refund(void) {
    if (nowT - P.undoT >= REFUND_UNDO_T) return;
    gauge_remove(P.loadout[0], P.undoPts[0]); gauge_remove(P.loadout[1], P.undoPts[1]);
    P.undoPts[0] = P.undoPts[1] = 0;
}

/* appui : l'estoc part tout de suite (une seule cible, la plus proche du cone). Si la touche reste
 * tenue 0,15 s, lance_tick couche la lance : sa fenetre de parry (0,12 s) est deja fermee. */
static void lance_press(void) {
    V3 eye = eye_pos(), f = view_dir();
    float sp = vlen(P.vel), bd = 1e9f;
    int best = -1, bestP = 0, hit = 0, parried;

    melee_start_swing();
    parried = try_parry(MELEE_RANGE + 1.5f, 1);
    for (int k = 0; k < MAXE + MAXP; k++) {
        int pl = k >= MAXE, i = pl ? k - MAXE : k;
        V3 c; float rad;
        if (!pl) { if (!E[i].alive) continue; c = E[i].pos; rad = enemy_radius(E[i].type); }
        else { if (!np_target(i)) continue; c = np_center(i); rad = 0.5f; }
        V3 d = vsub(c, eye);
        float dist = fmaxf(vlen(d), 0.01f), reach = dist - rad;
        if (reach > LANCE_THRUST_RANGE || reach >= bd) continue;
        if (reach > 0.8f && vdot(vmul(d, 1.0f / dist), f) < LANCE_THRUST_CONE) continue;
        bd = reach; best = i; bestP = pl;
    }
    P.lanceState = LS_THRUST; P.lanceT = 0; P.lanceVc = 0;
    P.meleeT = vgm_estoc_anim(0.2f); P.parryWin = PARRY_WINDOW;
    lanceThrusts++;
    if (best >= 0 && !bestP) {                            /* drone ou kamikaze */
        Enemy *e = &E[best];
        float rel = rel_speed(e->vel);
        if (e->type == EN_RUSHER) {                       /* kamikaze pare : explose dans sa face */
            V3 at = e->pos;
            damage_enemy(best, 9999, vmul(f, 10));
            parry_feedback(at);
            melee_parry_points(PTS_PARRY);
            parried++;
        } else {
            damage_enemy(best, lance_thrust_dmg(rel), vmul(f, 8 + rel * 0.5f));
            if (!e->alive) { if (!vagOn) P.hp = fminf(100, P.hp + 15); ammo_points(PTS_MELEE_KILL, AP_KILL); }   /* VAGUES : pas de soin sur kill */
        }
        hit = 1;
    } else if (best >= 0) {
        hit = !shield_faces(best, vnorm(vsub(np_center(best), eye)), SHB_DIRECT);   /* Egide : estoc bloque = ni dash ni points */
        parried += !hit;
    }
    for (int k = 0; k < 14; k++)                          /* trait de l'estoc */
        add_part(vadd(eye, vmul(f, 0.8f + k * 0.18f)), vmul(f, 4), v3(0.9f, 0.9f, 1), 0.12f, 0.06f, 0);
    if (dmMode) net_fx_melee(eye, f, sp);                 /* EV_MELEE : le recepteur lit meleeKind dans pad */
    P.swingLanded = hit || parried;
    play(S_LANCE_THRUST, 0.6f, 1.0f);
    if (hit) {                                            /* effets locaux d'abord : chez l'hote, un parry revient tout de suite */
        lanceThrustHits++;
        P.dashCharge = fminf(2, P.dashCharge + 1);
        melee_hit_points(meleePts[P.meleeKind] + vgm_estoc_pts());
        if (best >= 0 && !bestP) vgm_estoc_hit(best);   /* VAGUES : estocs a la suite, capstones */
        hitMarker = 0.18f; shake += 0.15f;
        if (!parried) hitstop = fmaxf(hitstop, 0.04f);
        play(S_PUNCH, 0.9f, 0.8f);
    }
    if (best >= 0 && bestP) {                             /* LAN : un autre joueur */
        float rel = rel_speed(np_vel(best));
        V3 c = np_center(best);
        burst(c, v3(1, 0.85f, 0.6f), 16, 8, 0.3f, 0.1f);
        hit_player_ex(best, lance_thrust_dmg(rel) * PVP_SCALE, vadd(vmul(f, 8 + rel * 0.5f), v3(0, 3, 0)), W_LANCE,
                      vnorm(vsub(c, eye)), SHB_DIRECT, 0, sp, rel, 0);
    }
}

/* joute : la cible i (joueur si isP) est touchee par la lance couchee a la vitesse de rapprochement vc */
static void lance_strike(int i, int isP, float vc) {
    V3 eye = eye_pos(), c = isP ? np_center(i) : E[i].pos, tv = isP ? np_vel(i) : E[i].vel;
    V3 dir = vnorm(vsub(c, eye));
    float dmg = lance_dmg(vgm_vc(vc)) * vgm_joust_mul(), spd = vlen(P.vel);
    /* la cible recoit 60 % de ta vitesse, et perd sa composante d'approche : elle est desarconnee */
    V3 knock = vsub(vmul(P.vel, LANCE_GIVE), vmul(dir, fminf(0, vdot(tv, dir))));
    knock.y += 3;
    if (vlen(knock) > LANCE_KNOCK_MAX) knock = vmul(vnorm(knock), LANCE_KNOCK_MAX);
    if ((isP && P.hookPlayer == i + 1) || (!isP && P.hookEnemy == i + 1)) {   /* la joute consomme le grappin */
        P.hook = HOOK_NONE; P.hookEnemy = P.hookPlayer = 0; P.hookCd = 0; P.airJumps = 1;
        lanceJoustHook++;
    }
    lanceJousts++; lanceVcSum += vc;
    if (vagOn) vg_rail_joust();                           /* VAGUES : CHEVALIER */
    if (isP && shield_faces(i, dir, SHB_DIRECT)) {        /* Egide : joute bloquee, lance relevee, ni dash ni points */
        P.vel = vmul(P.vel, LANCE_KEEP);                  /* (la cible ne recoit que 30 % : l'hote divise le recul par 2) */
        P.lanceState = LS_IDLE; P.meleeCd = LANCE_HIT_CD; P.lanceVc = 0;
    } else lance_win();                                   /* effets locaux avant l'envoi (voir lance_press) */
    lance_juice(c, dir, vc);
    if (!isP) {
        damage_enemy(i, dmg, knock);
        if (!E[i].alive) { if (!vagOn) P.hp = fminf(100, P.hp + 15); ammo_points(PTS_MELEE_KILL, AP_KILL); }   /* VAGUES : pas de soin sur kill */
        vgm_joust_hit(i);                                 /* VAGUES : recharge, soin, points, capstones */
    } else {
        if (botMode) netlog("joute : v_c %.1f -> %s", vc, NP[i].name);
        P.lanceHitT = 0; P.lanceHitWho = i + 1;           /* avant l'envoi : chez l'hote, EV_LANCE revient tout de suite */
        hit_player_ex(i, dmg * PVP_SCALE, knock, W_LANCE, dir, SHB_DIRECT, 0, spd, vc, 1);
    }
}

/* machine a etats de la lance, apres player_tick (donc apres le grappin). held = touche de melee tenue */
static void lance_tick(float dt, int held) {
    vgm_melee_tick(dt);
    if (P.dead || P.lanceState == LS_IDLE) { P.lanceState = LS_IDLE; P.lanceVc = 0; return; }
    P.lanceT += dt;
    if (P.lanceState == LS_THRUST) {
        if (P.swingLanded || !held) {                     /* estoc seul (ou estoc qui a touche) */
            P.lanceState = LS_IDLE;
            P.meleeCd = fmaxf(P.meleeCd, P.swingLanded ? vgm_thrust_cd(LANCE_THRUST_CD) : vgm_whiff_cd(MELEE_WHIFF_CD) - P.lanceT);
            return;
        }
        if (P.lanceT < vgm_tap(LANCE_TAP)) return;
        P.lanceState = LS_COUCH;                          /* l'estoc dans le vide devient la lance couchee, sans penalite */
        lanceCouches++;
        play(S_LANCE_COUCH, 0.7f, 1.0f);
    }
    if (!held || P.lanceT >= LANCE_HOLD_MAX - 1e-4f) {    /* relevee sans toucher (marge : somme de 240 ticks en float) */
        P.lanceState = LS_IDLE; P.meleeCd = fmaxf(P.meleeCd, vgm_whiff_cd(LANCE_MISS_CD)); P.lanceVc = 0;
        lanceWhiffs++; vgm_joust_whiff();
        return;
    }
    V3 eye = eye_pos(), f = view_dir();
    int bi = -1, isP = 0;
    float br = 1e9f, bvc = 0;
    P.lanceVc = 0;
    for (int k = 0; k < MAXE + MAXP; k++) {               /* une seule cible par joute : la premiere sur la lance */
        int pl = k >= MAXE, i = pl ? k - MAXE : k;
        V3 c, tv; float rad;
        if (!pl) { if (!E[i].alive) continue; c = E[i].pos; tv = E[i].vel; rad = enemy_radius(E[i].type); }
        else { if (!np_target(i)) continue; c = np_center(i); tv = np_vel(i); rad = 0.5f; }
        V3 d = vsub(c, eye);
        float dist = fmaxf(vlen(d), 0.01f), reach = dist - rad;
        V3 dir = vmul(d, 1.0f / dist);
        if (vdot(dir, f) < LANCE_CONE) continue;          /* le cone suit la vue, pas la vitesse */
        float vc = vdot(vsub(P.vel, tv), dir);
        if (reach < LANCE_PREVIEW) P.lanceVc = fmaxf(P.lanceVc, vc);   /* apercu (HUD, pointe) */
        if (reach > LANCE_REACH || vc < LANCE_VC_MIN || reach >= br) continue;
        br = reach; bi = i; isP = pl; bvc = vc;
    }
    if (bi >= 0) lance_strike(bi, isP, bvc);
}

/* lances couchees des autres joueurs : le son de la lance qui s'abaisse (front montant de ST_LANCE) */
static void lance_remote_tick(void) {
    for (int i = 0; i < MAXP; i++) {
        int on = np_present(i) && (NP[i].st.pad & ST_LANCE) != 0;
        if (on && !NP[i].lanceSeen) play(S_LANCE_COUCH, 0.7f * atten(NP[i].disp), 1.0f);
        NP[i].lanceSeen = (unsigned char)on;
    }
}
/* --- fin Lance --- */
