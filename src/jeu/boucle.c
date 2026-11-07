/* VELOCITY - ramassage, game_tick (un pas de simulation), fx_tick
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ramassage des recharges : en solo directement, en LAN l'hote arbitre */
static void pickup_tick(float dt) {
    for (int i = 0; i < npu; i++) {
        if (netMode != NET_CLIENT && PU[i].t > 0) PU[i].t -= dt;
        PU[i].pending -= dt;
        if (P.dead || PU[i].t > 0 || loadout_full() || PU[i].pending > 0) continue;
        if (vlen(vsub(pcenter(), PU[i].pos)) > 1.7f) continue;
        if (!dmMode) pickup_taken(i, myId);
        else if (netMode == NET_HOST) host_take_pickup(i, myId);
        else {
            NetEvent e = ev_new(EV_PICKUP);
            e.sub = (unsigned char)i;
            net_emit(&e);
            PU[i].pending = 0.5f;
        }
    }
}

static void fx_tick(float dt);

/* Spectateur : il ne simule rien. Camera libre (deplacement + dash = rapide, saut/accroupi = monter/descendre) ;
 * un tir passe au joueur suivant, qu'on suit en vue a la troisieme personne, puis revient a la camera libre. */
static void spec_tick(float dt) {
    input_poll();
    if (actP[A_FIRE]) {
        int n = specFollow;
        do n++; while (n < MAXP && !NP[n].active);
        specFollow = n >= MAXP ? -1 : n;
    }
    if (specFollow >= 0 && !NP[specFollow].active) specFollow = -1;
    V3 f = view_dir();
    P.prevPos = P.pos;
    P.vel = v3(0, 0, 0);
    if (specFollow >= 0) {
        P.pos = vsub(vsub(np_center(specFollow), vmul(f, 3.5f)), v3(0, P.eyeH, 0));
    } else {
        V3 right = v3(cosf(P.yaw), 0, sinf(P.yaw));
        V3 w = vadd(vmul(f, (float)(act[A_FWD] - act[A_BACK])), vmul(right, (float)(act[A_RIGHT] - act[A_LEFT])));
        w.y += (float)(act[A_JUMP] - act[A_CROUCH]);
        P.pos = vadd(P.pos, vmul(vnorm(w), (act[A_DASH] ? 40.0f : 14.0f) * dt));
    }
    proj_tick(dt);                                        /* roquettes et pieces des autres, sans degats pour nous (P.dead) */
    coin_tick(dt);
    anvil_remote_tick();                                  /* trainee des piques */
    lance_remote_tick();                                  /* son des lances couchees */
    shield_remote_tick(dt);                               /* Egide : jauges miroirs */
    if (matchOver) matchEndT -= dt;
    fx_tick(dt);
}

/* tir de l'arme en main (chaque fire_* retire lui-meme sa munition) */
static void fire_weapon(void) {
    switch (P.weapon) {
    case WP_RAIL:   fire_rail(); break;
    case WP_ROCKET: fire_rocket(); break;
    case WP_KINETIC: fire_kinetic(); break;
    case WP_SHOTGUN: fire_shotgun(); break;
    case WP_SAW:    fire_saw(); break;
    case WP_MINE:   fire_mine(); break;                   /* Ventouse */
    }
}
/* chargeur vide : clic sec, puis l'autre arme si elle peut tirer, sinon on pousse au contact */
static void dry_fire(void) {
    play(S_HIT, 0.5f, 0.45f);
    P.fireCd = DRYFIRE_CD;
    int o = other_wp();
    if (wp_has_ammo(o)) { snprintf(popText, sizeof popText, "%s A SEC", ammoDef[P.weapon].name); weapon_select(o); }
    else { snprintf(popText, sizeof popText, "A SEC : VA AU CONTACT !"); dryHintT = 2; }
    popT = 1.0f;
}
/* Scie dehors : clic sec seulement si elle est plantee (elle ne reviendra pas seule), puis bascule de l'infra ;
 * en vol, en retour ou perdue, rien : le clic tenu la relancera des qu'elle revient */
static void saw_dry(int press) {
    Proj *s = saw_mine();
    if (!press || !s || s->sawState != SAW_PLANTED) return;
    dry_fire();
    snprintf(popText, sizeof popText, "SCIE PLANTEE (%d m)", (int)vlen(vsub(s->pos, pcenter())));
}

static void game_tick(float dt) {
    vg_clock(dt);                                         /* VAGUES : temps de la partie (hitstop compris) */
    /* hitstop : le monde se fige un instant sur les gros coups (les entrees attendent) ; jamais en ligne, ou le
       reste de la partie continue : seuls l'impact frame et les effets restent */
    if (hitstop > 0) { hitstop -= dt; if (!dmMode) { P.prevPos = P.pos; return; } }
    if (spectating) { spec_tick(dt); return; }

    int rp = latch[SC_R];                                 /* R : recommencer (solo), non reaffectable */
    if (raceOn && rp && !vagOn) { latch[SC_R] = 0; race_start(); return; }   /* course : R repart du depart */
    input_poll();
    int jp = actP[A_JUMP], dp = actP[A_DASH];
    int w1 = actP[A_WEAP1], w2 = actP[A_WEAP2];
    int lmbp = actP[A_FIRE], swap = actP[A_SWAP], meleePress = actP[A_MELEE], coinPress = actP[A_SKILL];
    hookPressIn = actP[A_HOOK];
    hookHeldIn = act[A_HOOK];

    if (P.dead) {
        P.deathT += dt;
        if (!dmMode && (rp || (lmbp && P.deathT > 1.0f))) { reset_game(); return; }
    } else gameTime += dt;
    if (eco_on()) {                                       /* boutique : curseur libre a la mort et au choix de l'arme de depart */
        static int shopWas;
        int shopping = (P.dead || ecoNeedStarter) && !spectating && !botMode;
        if (shopping && captured) set_capture(0);
        if (!shopping && shopWas && !captured) set_capture(1);
        shopWas = shopping; ecoShopping = shopping;
        if (ecoNeedStarter && !P.dead) {                  /* arme de depart : 10 s pour choisir, immobile en attendant */
            ecoStarterT -= dt;
            if (ecoStarterT <= 0) { eco_pick_starter(WP_RAIL); loadout_apply(); }
            P.vel = v3(0, 0, 0);
        }
    }
    if (dmMode && rules.ammo == AMMO_INFINI && !P.dead)   /* regle : munitions infinies */
        for (int s = 0; s < 2; s++) P.ammo[P.loadout[s]] = ammoDef[P.loadout[s]].max;

    if (!P.dead) {                                        /* mort : les touches ne changent plus le loadout (menu LOADOUT) */
        int nw = P.weapon;
        if (w1) nw = P.loadout[0];
        if (w2) nw = P.loadout[1];
        if (swap) nw = other_wp();
        weapon_select(nw);
    }

    vgSrc = UP_MELEE(P.meleeKind);                        /* VAGUES : chemin de la boutique des touches qui suivent */
    player_tick(dt, jp, dp);
    race_tick(dt);                                        /* course : decompte, portes, chrono */
    sr_full_tick(dt);                                     /* speedrun complet : carte suivante */
    style_tick(dt);
    if (maps[curMap].killY < -999) {                      /* carte fermee : sortie de la carte = chute (sinon void_check) */
        if (!P.dead && !fellOut && P.pos.y < FALL_DEATH_Y) {  /* sorti de la carte et tombe dans le vide : mort */
            fellOut = 1;
            damage_player(1000);
            netlog("chute hors de la carte");
        }
        if (P.pos.y > FALL_DEATH_Y + 5) fellOut = 0;      /* respawn : a nouveau sur la carte */
        if (P.dead && P.pos.y < FALL_DEATH_Y - 40) { P.vel = v3(0, 0, 0); P.pos.y = P.prevPos.y = FALL_DEATH_Y - 40; }   /* ne tombe pas a l'infini */
    }
    void_check();                                         /* kill zone : sous killY = mort */

    vgSrc = P.weapon;                                     /* ALL IN ONE tire l'arme en main */
    skill_tick(dt, coinPress, act[A_SKILL]);              /* Competences (la piece en est une) */
    vgSrc = UP_MELEE(P.meleeKind);
    coin_tick(dt);

    P.meleeCd -= dt; P.meleeT -= dt;
    if (P.lanceHitT < 10) P.lanceHitT += dt;
    if (P.meleeKind == MK_LANCE) {                        /* Lance : appui = estoc, maintien = lance couchee */
        if (!P.dead && meleePress && P.meleeCd <= 0 && P.lanceState == LS_IDLE) lance_press();
        lance_tick(dt, act[A_MELEE]);                     /* maintenu : act, pas actP */
    } else if (P.meleeKind == MK_FIST) fist_tick(dt, meleePress);   /* Poings : combo, tampon, parry de chaque coup */
    else if (P.meleeKind == MK_SHIELD) shield_tick(dt, meleePress, act[A_MELEE]);   /* Egide : maintenu = leve, tap = coup */
    else if (!P.dead && meleePress && P.meleeCd <= 0) melee();
    shield_remote_tick(dt);                               /* Egide : jauges miroirs (clients) */
    if (P.meleeKind != MK_FIST && P.parryWin > 0) {
        if (!P.dead) try_parry(P.meleeKind == MK_FLAIL ? FLAIL_PARRY_RANGE : MELEE_RANGE + 1.5f,   /* Fleau : 6 m, renvoi x0,8 */
                               P.meleeKind == MK_FLAIL ? FLAIL_PARRY_SPEED : 1);
        P.parryWin -= dt;
        /* coup dans le vide : on paie le risque (la Lance, elle, decide a 0,15 s : estoc seul ou lance couchee) */
        if (P.parryWin <= 0 && !P.swingLanded && P.lanceState != LS_THRUST && P.meleeKind != MK_SHIELD) {
            int anvil = P.meleeKind == MK_ANVIL, flail = P.meleeKind == MK_FLAIL;
            P.meleeCd = fmaxf(P.meleeCd, anvil ? ANVIL_WHIFF_CD - ANVIL_PARRY_WINDOW :
                                         flail ? FLAIL_WHIFF_CD - FLAIL_PARRY_WINDOW : MELEE_WHIFF_CD - PARRY_WINDOW);
        }
    }
    if (P.anvilLandT < 10) P.anvilLandT += dt;            /* Enclume : sert a reconnaitre un parry de l'onde */
    if (P.flailT > 0 && !P.dead) {                        /* --- Fleau : une particule par tick sur la tete de la chaine --- */
        float sag, spd = vlen(P.vel);
        add_part(flail_my_head(eye_pos(), &sag), v3(0, 0, 0), spd > 15 ? v3(1, 0.7f, 0.3f) : v3(0.75f, 0.78f, 0.85f), 0.15f, 0.07f, 0);
    }
    P.flailT -= dt; P.flailLastT -= dt; P.flailParriedT -= dt;   /* --- fin Fleau --- */
    anvil_remote_tick();
    lance_remote_tick();                                  /* Lance : son des lances couchees adverses */

    P.fireCd -= dt; P.switchT -= dt;
    P.kinT -= dt;                                         /* Elan : decharge du viewmodel */
    {                                                     /* SHOTGUN : clac de la pompe a 35 % de la recharge */
        float pv = P.sgPumpT;
        P.sgPumpT -= dt; P.sgLastT -= dt;
        if (P.weapon == WP_SHOTGUN && !P.dead && pv > 0.65f * P.sgPumpMax && P.sgPumpT <= 0.65f * P.sgPumpMax)
            play(S_SG_PUMP, 0.5f, 1);
    }
    P.sawCd -= dt;                                        /* Scie : delai entre deux lancers */
    P.recoil *= expf(-dt * 9);
    int shieldUp = P.shieldState == SHS_UP;              /* Egide : bouclier leve = pas de tir (pieces et detonation permises) */
    if (!P.dead && !shieldUp && P.weapon != WP_TESLA && !tesla_stunned() && (act[A_FIRE] || lmbp) && P.fireCd <= 0 && P.switchT <= 0) {
        if (wp_has_ammo(P.weapon)) { vgSrc = P.weapon; fire_weapon(); vg_on_fire(P.weapon); }
        else if (P.weapon == WP_SAW) saw_dry(lmbp);
        else dry_fire();
    }
    vgSrc = WP_TESLA;
    tesla_tick(dt, !P.dead && !shieldUp && P.weapon == WP_TESLA && (act[A_FIRE] || lmbp) && P.switchT <= 0);   /* Tesla : tir maintenu */
    vgSrc = WP_MINE;
    if (!P.dead && (actP[A_DETONATE_ALL] || actP[A_DETONATE_ONE])) mine_remote(actP[A_DETONATE_ALL]);   /* Ventouse : quelle que soit l'arme */
    vgSrc = -1;
    if (!P.dead && !wp_has_ammo(P.loadout[0]) && !wp_has_ammo(P.loadout[1])) dryT += dt;
    ammoGainT -= dt; dryHintT -= dt;

    if (!dmMode && !raceOn) enemy_tick(dt);
    proj_tick(dt);
    pickup_tick(dt);
    if (!dmMode && (!raceOn || vagOn)) wave_tick(dt);     /* VAGUES : aussi pendant le challenge COURSE */
    if (matchOver) matchEndT -= dt;
    if (netMode == NET_HOST) host_tick(dt);
    fx_tick(dt);
}

/* decroissance des effets (flux de kills, particules, chiffres...) : joueur et spectateur */
static void fx_tick(float dt) {
    for (int k = 0; k < 6; k++) kfeed[k].t -= dt;

    for (int i = 0; i < MAXPA; i++) {
        Part *q = &PA[i];
        if (q->life <= 0) continue;
        q->life -= dt;
        q->vel.y -= q->grav * dt;
        q->vel = vmul(q->vel, 1 - dt * 1.5f);
        q->pos = vadd(q->pos, vmul(q->vel, dt));
    }
    for (int i = 0; i < MAXBM; i++) BM[i].life -= dt;
    for (int i = 0; i < MAXTR; i++) TR[i].t += dt;       /* SHOTGUN : traceurs */
    slam_vis_tick(dt);                                    /* Enclume : murs d'onde */
    tesla_fx_tick(dt);                                    /* Tesla : liens d'arc, arcs des autres */
    for (int i = 0; i < nb; i++)                         /* particules des jump pads */
        if (boxes[i].kind == B_PAD && frand() < 0.15f) {
            V3 c = vlerp(boxes[i].mn, boxes[i].mx, 0.5f);
            add_part(v3(c.x + frange(-1.3f, 1.3f), 0.35f, c.z + frange(-1.3f, 1.3f)), v3(0, frange(3, 7), 0),
                     v3(0.2f, 0.8f, 1.0f), 0.8f, 0.08f, 0);
        }

    shard_tick(dt);
    for (int i = 0; i < MAXDN; i++) DN[i].t -= dt;
    for (int i = 0; i < MAXP; i++) { NP[i].flashT -= dt; NP[i].sgFireT -= dt; }
    killMarkT -= dt; hurtDirT -= dt; muzzleT -= dt;
    hitMarker -= dt; P.hurtFlash -= dt; msgT -= dt; popT -= dt; parryFlash -= dt; streakLostT -= dt;
    shake *= expf(-dt * 6);
    if (comboT > 0) { comboT -= dt; if (comboT <= 0) combo = 0; }
}
