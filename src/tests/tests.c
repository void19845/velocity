/* VELOCITY - captures --shot et tests automatiques --test
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static void save_bmp(const char *path) {
    int rowSize = (winW * 3 + 3) & ~3;
    unsigned char *px = (unsigned char *)malloc(rowSize * winH);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, winW, winH, 0x80E0 /* GL_BGR_EXT */, GL_UNSIGNED_BYTE, px);
    FILE *f = fopen(path, "wb");
    if (f) {
        BITMAPFILEHEADER fh = { 0 }; BITMAPINFOHEADER ih = { 0 };
        fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + rowSize * winH;
        ih.biSize = sizeof ih; ih.biWidth = winW; ih.biHeight = winH; ih.biPlanes = 1; ih.biBitCount = 24;
        fwrite(&fh, sizeof fh, 1, f); fwrite(&ih, sizeof ih, 1, f); fwrite(px, rowSize * winH, 1, f);
        fclose(f);
    }
    free(px);
}

/* tests de l'Enclume : pique depuis `from` (vitesse `vel`), ticks jusqu'a la fin du pique puis de la propagation de l'onde */
static void test_slam_drop(V3 from, V3 vel) {
    P.meleeKind = MK_ANVIL; P.anvilState = ANVIL_IDLE; P.meleeCd = 0;
    P.pos = P.prevPos = from; P.vel = vel; P.grounded = 0;
    anvil_press();
    for (int i = 0; i < 600 && P.anvilState == ANVIL_DIVE; i++) player_tick(TICK, 0, 0);
    for (int i = 0; i < 120 && SW.alive; i++) player_tick(TICK, 0, 0);
}
/* degats attendus de l'onde sur une cible a d m du centre (onde ronde), pad = rayon de la cible */
static float test_slam_expect(float H, float vy, float d, float pad) {
    float f = 1 - d / (slam_R(H) + pad);
    return slam_dmg(vy, H) * (0.3f + 0.7f * f);
}

/* tests de la Lance : remise a zero, Lance en melee, immobile en pos, vue vers -z */
static void lance_test_reset(V3 pos) {
    reset_game();
    hitstop = 0;
    memset(act, 0, sizeof act); memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
    P.meleeKind = MK_LANCE; P.pos = P.prevPos = pos; P.yaw = 0; P.pitch = 0; P.vel = v3(0, 0, 0);
}
/* LAN simule : moi (hote, 0) et NP[1] a la position t, ni parry ni lance couchee recents */
static void lance_test_lan(V3 t) {
    dmMode = 1; matchOver = 0;
    memset(NP, 0, sizeof NP);
    memset(joustT, 0, sizeof joustT);
    NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
    NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible");
    NP[1].disp = t; vf(NP[1].st.pos, t);
    NP[0].disp = P.pos;
    nowT += 1;
}

/* tests de la Scie : remise a zero, scie dans le loadout, en main */
static void saw_test_reset(V3 pos, float yaw, float pitch) {
    reset_game();
    dmMode = 0; hitstop = 0;
    memset(act, 0, sizeof act); memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);   /* entrees des tests d'avant */
    P.pos = P.prevPos = pos; P.yaw = yaw; P.pitch = pitch; P.vel = v3(0, 0, 0);
    P.loadout[0] = WP_SAW; P.loadout[1] = WP_RAIL; P.weapon = WP_SAW; P.ammo[WP_SAW] = 1; P.switchT = 0;
}
/* place une scie (la mienne si owner == myId) dans l'etat st */
static Proj *saw_test_put(V3 pos, V3 vel, int st, int owner) {
    Proj *p = spawn_proj(PR_SAW, pos, vel);
    p->sawState = st; p->owner = owner; p->sawAxis = v3(0, 1, 0); p->sawN = v3(0, 1, 0);
    p->netId = ((unsigned int)(owner + 1) << 24) | ((unsigned int)PR_SAW << 20) | 77;
    if (owner == myId) { P.sawIdx = (int)(p - PR) + 1; P.ammo[WP_SAW] = 0; }
    return p;
}

/* --- Tesla : outils des tests --- */
/* remise a zero : solo, Tesla + rail en main, batterie 60, regard vers -z (ou yaw) depuis pos */
static void tesla_test_reset(V3 pos, float yaw) {
    reset_game();
    dmMode = 0; hitstop = 0;
    memset(act, 0, sizeof act); memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
    memset(DN, 0, sizeof DN);
    P.pos = P.prevPos = pos; P.yaw = yaw; P.pitch = 0; P.vel = v3(0, 0, 0);
    P.loadout[0] = WP_TESLA; P.loadout[1] = WP_RAIL; P.weapon = WP_TESLA; P.switchT = 0; P.fireCd = 0;
    P.ammo[WP_TESLA] = 60; P.ammo[WP_RAIL] = 2;
}
/* `ticks` ticks comme game_tick (pieces, recharge, Tesla), sans deplacer les drones ; held = tir tenu */
static void tesla_test_run(int ticks, int held) {
    for (int i = 0; i < ticks; i++) {
        nowT += TICK;
        coin_tick(TICK);
        P.fireCd -= TICK; P.switchT -= TICK;
        tesla_tick(TICK, held && !P.dead && P.weapon == WP_TESLA && P.switchT <= 0);
    }
}
static Enemy *tesla_test_drone(int i, V3 pos, float hp) {
    E[i].alive = 1; E[i].type = EN_DRONE; E[i].hp = hp; E[i].pos = pos; E[i].vel = v3(0, 0, 0);
    return &E[i];
}
/* --- fin Tesla : outils des tests --- */

/* --- Ventouse : outils des tests --- */
/* remise a zero : solo, Ventouse + rail, Ventouse en main avec 1 mine, sans drones */
static void mine_test_reset(V3 pos, float yaw, float pitch) {
    reset_game();
    dmMode = 0; hitstop = 0; waveDelay = 1000;
    memset(act, 0, sizeof act); memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
    memset(DN, 0, sizeof DN);
    P.pos = P.prevPos = pos; P.yaw = yaw; P.pitch = pitch; P.vel = v3(0, 0, 0);
    P.loadout[0] = WP_MINE; P.loadout[1] = WP_RAIL; P.weapon = WP_MINE; P.switchT = 0; P.fireCd = 0;
    P.ammo[WP_MINE] = 1; P.ammoGauge[WP_MINE] = 0;
}
static Proj *mine_test_last(void) {                       /* ma derniere mine lancee */
    Proj *r = NULL;
    for (int i = 0; i < MAXPR; i++) if (mine_own(&PR[i]) && (!r || PR[i].mineSeq > r->mineSeq)) r = &PR[i];
    return r;
}
static int mine_test_stick(Proj *p, int moveP) {          /* ticks jusqu'au collage (ou l'explosion) */
    int n = 0;
    while (n < 600 && p->alive && p->kind == PR_MINE && p->mineState == MS_FLY) {
        nowT += TICK; proj_tick(TICK);
        if (moveP) player_tick(TICK, 0, 0);
        n++;
    }
    return n;
}
static Proj *mine_test_put(V3 pos, int st, int owner) {   /* mine posee a la main (armee) */
    Proj *p = spawn_proj(PR_MINE, pos, v3(0, 0, 0));
    p->owner = owner; p->mineState = st; p->mineN = v3(0, 1, 0); p->mineSeq = ++mineCounter;
    p->netId = ((unsigned int)(owner + 1) << 24) | ((unsigned int)PR_MINE << 20) | (mineCounter & 0xFFFFF);
    return p;
}
/* LAN simule : moi (hote, 0) et NP[1] en t ; table d'arbitrage vide */
static void mine_test_lan(V3 t) {
    dmMode = 1; matchOver = 0;
    memset(NP, 0, sizeof NP);
    memset(hostMine, 0, sizeof hostMine);
    NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
    NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible");
    NP[1].disp = t; vf(NP[1].st.pos, t);
    NP[0].disp = P.pos;
    nowT += 1;
}
/* --- fin Ventouse : outils des tests --- */
/* --- Fleau : outils des tests --- */
/* remise a zero : solo, Fleau en melee, immobile en pos, vue vers -z */
static void flail_test_reset(V3 pos) {
    reset_game();
    dmMode = 0; hitstop = 0;
    memset(act, 0, sizeof act); memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
    P.meleeKind = MK_FLAIL; P.pos = P.prevPos = pos; P.yaw = 0; P.pitch = 0; P.vel = v3(0, 0, 0);
}
static Enemy *flail_test_enemy(int i, V3 pos, int type) {
    E[i].alive = 1; E[i].type = type; E[i].hp = type == EN_DRONE ? 100.0f : 50.0f; E[i].pos = pos; E[i].vel = v3(0, 0, 0);
    E[i].fireT = 10;                                      /* pas de tir d'orbe pendant le test */
    return &E[i];
}
/* dernier EV_HIT de la file d'envoi (NULL si aucun) */
static const NetEvent *flail_test_last_hit(void) {
    for (int k = 1; k <= OUTQ; k++) {
        const NetEvent *e = &outq[(outqHead - k + OUTQ) % OUTQ].ev;
        if (e->kind == EV_HIT) return e;
    }
    return NULL;
}
/* Yoyo, traction (appui long) : `ticks` ticks de jeu complets (game_tick) ; press = appui au premier tick, held = touche de melee tenue */
static void flail_test_run(int ticks, int press, int held) {
    for (int i = 0; i < ticks; i++) {
        nowT += TICK;
        botPress[A_MELEE] = (unsigned char)(press && i == 0); botHeld[A_MELEE] = (unsigned char)held;
        game_tick(TICK);
        hitstop = 0;                                      /* le hitstop du coup ne saute pas de ticks */
    }
    botPress[A_MELEE] = 0; botHeld[A_MELEE] = 0;
}
/* EV_FLAIL fabrique, comme s'il venait du reseau */
static NetEvent flail_test_ev(int from, int target, int sub, float scale) {
    NetEvent e = ev_new(EV_FLAIL);
    e.from = (unsigned char)from; e.target = (unsigned char)target; e.sub = (unsigned char)sub; e.f[0] = scale;
    return e;
}
/* EV_FLAIL de sous-type sub restes dans la file d'envoi (target t, ou tous si t < 0) ; scale : f[0] du dernier trouve */
static int flail_test_count(int sub, int t, float *scale) {
    int n = 0;
    for (int k = 0; k < OUTQ; k++)
        if (outq[k].left > 0 && outq[k].ev.kind == EV_FLAIL && outq[k].ev.sub == sub && (t < 0 || outq[k].ev.target == t)) {
            n++;
            if (scale) *scale = outq[k].ev.f[0];
        }
    return n;
}
/* victime simulee : `ticks` ticks (flail_victim_tick + player_tick) ; le lanceur NP[1] rafraichit le bail toutes les 0,1 s si refresh, avec
 * l'intensite scale (l'hote l'ecrit dans chaque START et PULL). Renvoie par pointeurs la vitesse maximale, la distance minimale et la
 * distance finale au lanceur (centres) */
static void flail_test_victim(int ticks, int refresh, float scale, float *vmax, float *dmin, float *dEnd) {
    NetEvent pu = flail_test_ev(1, 0, FLE_PULL, scale);
    *vmax = 0; *dmin = 1e9f;
    for (int k = 0; k < ticks; k++) {
        if (refresh && k % 12 == 6) apply_event(&pu);
        nowT += TICK;
        hookPressIn = hookHeldIn = 0;
        flail_victim_tick(TICK); player_tick(TICK, 0, 0); flail_fx_tick(TICK);
        *vmax = fmaxf(*vmax, vlen(P.vel));
        *dmin = fminf(*dmin, vlen(vsub(pcenter(), np_center(1))));
    }
    *dEnd = vlen(vsub(pcenter(), np_center(1)));
}
/* --- fin Fleau : outils des tests --- */
/* --- Poings : outils des tests --- */
/* remise a zero : solo, Poings en melee, immobile en pos, vue vers -z */
static void fist_test_reset(V3 pos) {
    reset_game();
    dmMode = 0; hitstop = 0;
    memset(act, 0, sizeof act); memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
    P.meleeKind = MK_FIST; P.pos = P.prevPos = pos; P.yaw = 0; P.pitch = 0; P.vel = v3(0, 0, 0);
    P.grounded = pos.y < 0.01f;                           /* pose au sol (sinon un coup sur un drone "suspend" en l'air) */
}
/* un tick de melee comme dans game_tick, sans le reste du monde */
static void fist_test_tick(int press) { P.meleeCd -= TICK; P.meleeT -= TICK; fist_tick(TICK, press); hitstop = 0; }
/* un coup tout de suite ; le drone e reste immobile (son recul ne compte pas dans v_rel) */
static void fist_test_punch(Enemy *e) { if (e) e->vel = v3(0, 0, 0); P.meleeCd = 0; fist_punch(); hitstop = 0; }
/* --- fin Poings : outils des tests --- */
/* --- Egide : outils des tests --- */
/* remise a zero : Egide en melee, rail en main (2 munitions), immobile en (-20, 0, 41), vue vers -z */
static void egide_test_reset(void) {
    reset_game();
    hitstop = 0;
    memset(act, 0, sizeof act); memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
    memset(DN, 0, sizeof DN);
    P.meleeKind = MK_SHIELD; P.pos = P.prevPos = v3(-20, 0, 41); P.yaw = 0; P.pitch = 0; P.vel = v3(0, 0, 0); P.grounded = 1;
    P.weapon = WP_RAIL; P.ammo[WP_RAIL] = 2; P.fireCd = 0; P.switchT = 0; shotStreak = 0;
}
/* LAN simule (je suis l'hote, 0) : NP[1] en t ; shield = son bouclier leve vers yaw (jauge pleine), sinon Egide baissee */
static void egide_test_lan(V3 t, int shield, float yaw) {
    lance_test_lan(t);
    NP[0].shieldGauge = NP[1].shieldGauge = SHIELD_GAUGE_MAX;
    NP[1].st.pad = (unsigned char)(MK_SHIELD | (shield ? ST_SHIELD : 0)); NP[1].st.yaw = yaw;
    NP[1].lastShieldT = shield ? nowT : 0;
}
/* --- fin Egide : outils des tests --- */
/* tirs a la tete : les tests des degats au corps visent le centre (a hauteur des yeux, un tir horizontal touche la tete) */
static float test_pitch_to(V3 at) { V3 d = vsub(at, eye_pos()); return asinf(d.y / vlen(d)); }

/* Tests du mouvement sans fenetre : velocity.exe --test resultat.txt */
/* --- cartes a cibles (LE STAND, LE FLUX) : pilote simple qui court / se balance et tire sur la cible vivante la plus proche de la
 * porte en cours (rail : le tir part au tick, la cible est touchee a l'endroit ou elle est). Renvoie le temps (s) ou -1 sans arrivee --- */
static float test_race_pilot(int m, int *hooks, int *falls) {
    FILE *logSave = botLog; botLog = NULL;
    race_start_map(m); appState = APP_GAME; hitstop = 0;
    memset(act, 0, sizeof act); memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
    raceBest = 0; ghostBestN = 0;
    int tick = 0; *hooks = 0;
    float t = 0;
    for (; t < 150 && !raceDone; t += TICK, tick++) {
        memset(botHeld, 0, sizeof botHeld);
        const MapDef *md = &maps[curMap];
        V3 g = race_gate(raceGate < md->ngates ? raceGate : md->ngates - 1), e = eye_pos(), d = vsub(g, e);
        float hd = fmaxf(hlen(d), 0.1f);
        P.yaw = atan2f(d.x, -d.z);
        P.pitch = clampf(atan2f(d.y, hd) + (md->challenge && md->killY > -999 ? 0.45f : 0), -1.2f, 1.2f);
        botHeld[A_FWD] = 1;
        if (P.grounded) botPress[A_JUMP] = 1;
        int tg = -1;                                      /* cible vivante de la porte en cours, la plus proche */
        for (int i = 0; i < MAXE; i++)
            if (race_is_target(i) && E[i].phase == raceGate && (tg < 0 || vlen(vsub(E[i].pos, e)) < vlen(vsub(E[tg].pos, e)))) tg = i;
        if (tg >= 0 && tick % 6 == 0 && raceCount <= 0) {  /* un tick sur six : on vise la cible et on tire */
            V3 dt = vsub(E[tg].pos, e);
            P.yaw = atan2f(dt.x, -dt.z); P.pitch = atan2f(dt.y, fmaxf(hlen(dt), 0.1f));
            botHeld[A_FIRE] = 1;
        } else if (md->killY > -999) {                    /* carte aerienne : grappin comme le pilote de LA TOILE */
            if (P.hook == HOOK_NONE && P.hookCd <= 0 && raceCount <= 0) {
                V3 at;
                if (swing_target(e, view_dir(), &at) >= 0) { botPress[A_HOOK] = 1; botHeld[A_HOOK] = 1; (*hooks)++; }
            } else if (P.hook != HOOK_NONE) {
                V3 toA = vsub(P.anchor, P.pos), fwd = vmul(v3(d.x, 0, d.z), 1 / hd);
                float past = -vdot(v3(toA.x, 0, toA.z), fwd);
                int passed = P.hook == HOOK_ATTACHED && ((past > 1 && P.vel.y > 2) || past > 9);
                botHeld[A_HOOK] = !passed;
            }
        }
        game_tick(TICK);
    }
    botLog = logSave;
    *falls = raceFalls;
    return raceDone ? raceT : -1;
}

static void run_tests(const char *path) {
    botAimTest = 1;
    FILE *f = fopen(path, "w");
    if (!f) return;
    build_level(0);

    {                                                     /* molette : une rafale (inertie) = un seul changement */
        int burst = 0, two = 0;
        for (int i = 0; i < 120; i++) burst += wheel_accept(100.0 + i * 0.016);   /* 2 s d'evenements toutes les 16 ms */
        two += wheel_accept(200.0); two += wheel_accept(200.6);                      /* deux crans espaces */
        int quick = wheel_accept(300.0) + wheel_accept(300.2 + WHEEL_QUIET);        /* second cran trop tot */
        fprintf(f, "molette : rafale de 2 s=%d, deux crans a 0,6 s=%d, deux crans a 0,35 s=%d (attendu 1, 2, 2)\n",
                burst, two, quick);
        wheelLastEv = wheelLastOk = -1e9;
    }

    {                                                     /* --export-textures : le .bmp relu = la texture generee, octet pour octet */
        char tp[600];
        int same = 0, sizes = 0;
        snprintf(tp, sizeof tp, "%s.texture.bmp", path);
        for (int k = 0; k < TEXG_N; k++) {
            int n = 0, m = 0;
            unsigned char *gen = tex_generate(k, &n), *back = NULL;
            if (gen && save_bmp_rgb(tp, gen, n)) back = load_bmp_square(tp, &m);
            if (back && m == n) { sizes++; same += !memcmp(gen, back, (size_t)n * n * 3); }
            free(gen); free(back);
            remove(tp);
        }
        fprintf(f, "export des textures : relues a la bonne taille=%d, identiques=%d (attendu %d, %d)\n", sizes, same, TEXG_N, TEXG_N);
    }

    {                                                     /* changement d'arme : le nom s'affiche, pas si on garde la meme */
        reset_game();
        nowT = 50; weapon_select(P.loadout[1]);
        double a = wpFlashAt;
        nowT = 51; weapon_select(P.loadout[1]);
        fprintf(f, "changement d'arme : nom affiche a t=%.0f, re-choix de la meme arme t=%.0f (attendu 50, 50)\n", a, wpFlashAt);
        nowT = 0; wpFlashAt = -10;
    }

    {                                                     /* barre de vie : la trainee attend puis rattrape les PV */
        hp_ghost_tick(100, 10.0); hp_ghost_tick(40, 10.016);
        float held = hpGhost;
        for (int i = 2; i <= 20; i++) hp_ghost_tick(40, 10.0 + i * 0.016);      /* 0,32 s : encore tenue */
        float stillHeld = hpGhost;
        for (int i = 21; i <= 120; i++) hp_ghost_tick(40, 10.0 + i * 0.016);    /* 1,9 s : videe */
        float drained = hpGhost;
        hp_ghost_tick(70, 12.0);
        fprintf(f, "trainee de vie : apres le coup=%.0f a 0,3 s=%.0f a 1,9 s=%.0f, soin=%d (attendu 100, 100, 40, 1)\n",
                held, stillHeld, drained, hpHealAt == 12.0);
        hpGhost = hpLast = 100; hpHitAt = hpHealAt = -10;
    }

    {                                                     /* second ecran : trainee de la carte, coupee au respawn */
        MapsPl pl = { v3(0, 0, 0), 0, 100, 1, 0, 0, NULL, "t" };
        for (int i = 0; i < 40; i++) { pl.pos = v3(i * 0.5f, 0, 0); nowT = 300 + i * 0.1; maps_trail_tick(&pl, 1); }
        int full = mapsTrailN[0];
        pl.pos = v3(-60, 0, 0); nowT = 305; maps_trail_tick(&pl, 1);   /* teleporte (respawn) */
        fprintf(f, "carte 2e ecran : points de trainee=%d, apres un respawn=%d (attendu %d, 1)\n", full, mapsTrailN[0], MAPS_TRAIL);
        mapsTrailN[0] = 0; nowT = 0; mapsTrailT = 0;
    }

    {                                                     /* second ecran : en partie, seulement moi et mes equipiers */
        NetPlayer save[MAXP];
        memcpy(save, NP, sizeof save);
        int sDm = dmMode, sTs = teamSize, sId = myId, sOver = matchOver;
        for (int i = 0; i < MAXP; i++) NP[i].active = 0;
        for (int i = 0; i < 4; i++) { NP[i].active = 1; NP[i].team = i & 1; }   /* 0 et 2 bleus, 1 et 3 rouges */
        dmMode = 1; myId = 0; matchOver = 0; spectating = 0;
        MapsPl pl[MAXP + 1];
        teamSize = 2; int team = maps_players(pl);
        teamSize = 0; int ffa = maps_players(pl);
        matchOver = 1; int over = maps_players(pl);
        fprintf(f, "carte 2e ecran : joueurs visibles en equipes=%d, chacun pour soi=%d, partie finie=%d (attendu 2, 1, 4)\n",
                team, ffa, over);
        memcpy(NP, save, sizeof save);
        dmMode = sDm; teamSize = sTs; myId = sId; matchOver = sOver;
    }

    {                                                     /* second ecran : courbe de vitesse, un point toutes les 0,5 s */
        reset_game();
        int sDm = dmMode; dmMode = 0;
        nowT = 400; maps_speed_reset();
        P.vel = v3(20, 0, 0);
        for (int i = 1; i <= 120; i++) { nowT = 400 + i * (1.0 / 60); maps_speed_tick(); }   /* 2 s a 60 images/s */
        int pts = mapsSpdN; float v = mapsSpd[MAXP][0] / 4.0f;
        fprintf(f, "courbe de vitesse : points en 2 s=%d, vitesse notee=%.0f (attendu 4, 20)\n", pts, v);
        maps_speed_reset(); dmMode = sDm; nowT = 0; P.vel = v3(0, 0, 0);
    }

    {                                                     /* carte de chaleur : position du mort et du tireur */
        V3 sDisp = NP[1].disp; int sId = myId, sSpec = spectating;
        myId = 0; spectating = 0; mapsHeatN = 0;
        reset_game(); P.pos = v3(5, 0, 7); NP[1].disp = v3(-20, 4, 30);
        maps_heat_add(0, 1); maps_heat_add(1, 1);            /* un kill, puis un suicide */
        fprintf(f, "carte de chaleur : %d points, mort x=%.0f tireur x=%.0f, suicide sans tireur=%d (attendu 2, -20, 5, 1)\n",
                mapsHeatN, mapsHeat[0].death.x, mapsHeat[0].kill.x, mapsHeat[1].self);
        mapsHeatN = 0; NP[1].disp = sDisp; myId = sId; spectating = sSpec;
    }

    {                                                     /* camera de mort : la vue finit tournee vers le tueur */
        int sDm = dmMode; dmMode = 0;
        reset_game();
        for (int i = 0; i < MAXE; i++) E[i].alive = 0;
        E[0].alive = 1; E[0].pos = v3(-20, 2, 20);         /* couloir degage x = -20 : l'ennemi est devant (-z) */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 1.5f; P.pitch = 0;
        killcam_start(-1, 0); P.dead = 1; P.deathT = 1.0f;
        V3 eye; float yaw = 0, pitch = 0, zoom = 1;
        int on = killcam_active();
        killcam_view(&eye, &yaw, &pitch, &zoom);
        P.deathT = KILLCAM_T + 0.1f;
        fprintf(f, "camera de mort : active=%d cap vers le tueur=%.2f zoom=%.2f, finie apres %.1f s=%d (attendu 1, 0.00, 0.65, 1.6, 0)\n",
                on, yaw, zoom, KILLCAM_T, killcam_active());
        kcOn = 0; dmMode = sDm; reset_game();
    }

    reset_game();                                         /* escalier -> plateforme centrale */
    P.pos = P.prevPos = v3(0, 0, ms(14) + 0.6f); P.yaw = 0;
    act[A_FWD] = 1;
    for (int i = 0; i < 240; i++) player_tick(TICK, 0, 0);
    act[A_FWD] = 0;
    fprintf(f, "escalier : y=%.2f z=%.2f (attendu y=4)\n", P.pos.y, P.pos.z);

    reset_game();                                         /* escalier a 14 m/s : la vitesse se conserve comme sur une rampe */
    P.pos = P.prevPos = v3(0, 0, ms(14) + 0.6f); P.yaw = 0; P.vel = v3(0, 0, -14);
    P.grounded = 1;
    act[A_FWD] = 1;
    for (int i = 0; i < 30; i++) player_tick(TICK, 0, 0);
    act[A_FWD] = 0;
    fprintf(f, "escalier a 14 m/s : y=%.2f vitesse apres 0,25 s=%.1f (attendu y > 1, vitesse > 11)\n", P.pos.y, hlen(P.vel));

    reset_game();                                         /* au sol, direction tenue : friction reduite */
    P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = PI * 0.5f; P.vel = v3(15, 0, 0);
    P.grounded = 1;
    act[A_FWD] = 1;
    for (int i = 0; i < 30; i++) player_tick(TICK, 0, 0);
    act[A_FWD] = 0;
    fprintf(f, "course a 15 m/s sans sauter : vitesse apres 0,25 s=%.1f (attendu > 12)\n", hlen(P.vel));

    reset_game();                                         /* jump pad -> plateforme haute */
    P.pos = P.prevPos = v3(ms(16), 0.31f, 0);
    float maxY = 0;
    for (int i = 0; i < 360; i++) { player_tick(TICK, 0, 0); maxY = fmaxf(maxY, P.pos.y); }
    fprintf(f, "jump pad est : apogee=%.2f final x=%.2f y=%.2f grounded=%d (attendu y=7, x entre 39 et 62)\n",
            maxY, P.pos.x, P.pos.y, P.grounded);
    reset_game();
    P.pos = P.prevPos = v3(ms(9), 0.31f, ms(16));
    for (int i = 0; i < 360; i++) player_tick(TICK, 0, 0);
    fprintf(f, "jump pad nord : final x=%.2f y=%.2f z=%.2f (attendu y=7, z entre 39 et 62)\n", P.pos.x, P.pos.y, P.pos.z);

    reset_game();                                         /* jump pad aborde au sol a grande vitesse (bhop) : meme vecteur attendu */
    P.pos = P.prevPos = v3(ms(16) - 6, 0, 0); P.yaw = PI * 0.5f; P.vel = v3(45, 0, 0); P.grounded = 1;
    act[A_FWD] = 1;                                       /* tenu seulement le temps de l'approche (20 ticks ~0,17 s @ 45 m/s) */
    for (int i = 0; i < 20; i++) player_tick(TICK, 0, 0);
    act[A_FWD] = 0;
    maxY = 0;
    for (int i = 0; i < 360; i++) { player_tick(TICK, 0, 0); maxY = fmaxf(maxY, P.pos.y); }
    fprintf(f, "jump pad a grande vitesse au sol : apogee=%.2f final x=%.2f y=%.2f (attendu proches de la vitesse normale : apogee=8.55 x=46.73)\n",
            maxY, P.pos.x, P.pos.y);

    reset_game();                                         /* jump pad aborde EN L'AIR (bhop qui retombe dessus) : meme chose doit marcher */
    P.pos = P.prevPos = v3(ms(16) - 1.2f, 0.15f, 0); P.yaw = PI * 0.5f; P.vel = v3(45, -3, 0); P.grounded = 0;
    maxY = 0;
    for (int i = 0; i < 360; i++) { player_tick(TICK, 0, 0); maxY = fmaxf(maxY, P.pos.y); }
    fprintf(f, "jump pad a grande vitesse en l'air : apogee=%.2f final x=%.2f y=%.2f (attendu proches de la vitesse normale : apogee=8.55 x=46.73)\n",
            maxY, P.pos.x, P.pos.y);

    reset_game();                                         /* bhop + air-strafe */
    P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = PI * 0.5f;
    act[A_FWD] = 1;
    for (int i = 0; i < 60; i++) player_tick(TICK, 0, 0);
    act[A_FWD] = 0;
    float startSpeed = hlen(P.vel), maxSpeed = 0;
    act[A_JUMP] = 1;
    for (int i = 0; i < 300; i++) {
        int side = (i / 50) % 2 ? 1 : -1;                 /* alterne gauche/droite */
        act[A_RIGHT] = side > 0; act[A_LEFT] = side < 0;
        if (!P.grounded) P.yaw += side * 2.2f * TICK;
        player_tick(TICK, 0, 0);
        maxSpeed = fmaxf(maxSpeed, hlen(P.vel));
    }
    memset(act, 0, sizeof act);
    fprintf(f, "bhop : vitesse depart=%.1f max=%.1f (attendu max > %.1f)\n", startSpeed, maxSpeed, RUN_SPEED + 3);

    {   /* ecran de mort : les touches d'arme, d'epee et de competence ne changent plus le loadout (menu LOADOUT) */
        reset_game();
        int l0 = wantLoadout[0], l1 = wantLoadout[1], mk = wantMelee, sk = wantSkill;
        P.dead = 1; P.deathT = 0.5f;
        botPress[A_WEAP1] = botPress[A_WEAP2] = botPress[A_MELEE] = botPress[A_SKILL] = 1;
        game_tick(TICK);
        int same = wantLoadout[0] == l0 && wantLoadout[1] == l1 && wantMelee == mk && wantSkill == sk;
        memset(botPress, 0, sizeof botPress);
        fprintf(f, "ecran de mort : loadout inchange apres les touches %d (attendu 1)\n", same);
        reset_game();
    }
    {   /* jamais de pause en ligne : le hitstop ne fige la simulation qu'en solo */
        float moved[2];
        for (int k = 0; k < 2; k++) {
            reset_game(); dmMode = k;
            P.pos = P.prevPos = v3(-20, 0, 40); P.vel = v3(10, 0, 0); P.grounded = 0;
            V3 p0 = P.pos;
            hitstop = 0.1f;
            for (int i = 0; i < 6; i++) game_tick(TICK);
            moved[k] = hlen(vsub(P.pos, p0));
        }
        dmMode = 0; hitstop = 0; reset_game();
        fprintf(f, "hitstop : solo fige %d, en ligne la partie continue %d (attendu 1 1)\n", moved[0] < 0.01f, moved[1] > 0.3f);
    }

    reset_game();                                         /* aimant de visee (competence AIMANT active) : corrige un petit ecart */
    P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0; P.grounded = 1;   /* couloir degage des tests */
    P.aimT = AIM_T;
    for (int i = 0; i < MAXE; i++) E[i].alive = 0;
    V3 eyeNow = eye_pos();
    E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(eyeNow.x + 0.7f, eyeNow.y, eyeNow.z - 20);
    aim_assist(TICK);
    float yawOneTick = P.yaw;
    for (int i = 0; i < 60; i++) aim_assist(TICK);
    float targetYaw = atan2f(0.7f, 20.0f);
    fprintf(f, "aimant de visee : apres 1 tick=%.5f (attendu petit, < 0.01), apres 0.5 s=%.4f sur %.4f vise (attendu entre 0 et %.4f, sans depasser)\n",
            yawOneTick, P.yaw, targetYaw, targetYaw);
    {   /* AIMANT : rien sans la competence ; activation, duree, pas de 2e activation, longue recharge */
        P.yaw = 0; P.aimT = 0;
        for (int i = 0; i < 60; i++) aim_assist(TICK);
        float yawOff = P.yaw;
        P.skill = SK_AIM; P.skillCh = 1;
        skill_tick(TICK, 1, 0);
        float t0 = P.aimT, ch0 = P.skillCh;
        P.skillCh = 1; skill_tick(TICK, 1, 0);            /* deja actif : la charge n'est pas reprise */
        int again = P.skillCh < 0.5f;
        for (int i = 0; i < (int)((AIM_T + 0.2f) / TICK); i++) skill_tick(TICK, 0, 0);
        float tEnd = P.aimT;
        P.skillCh = 0;
        for (int i = 0; i < (int)(10 / TICK); i++) skill_tick(TICK, 0, 0);
        fprintf(f, "competence AIMANT : sans elle yaw=%.4f, active %.1f s charge %.2f, 2e appui %d, fini %d, charge apres 10 s %.2f (attendu 0, %.1f, ~0, 0, 1, %.2f)\n",
                yawOff, t0, ch0, again, tEnd <= 0, P.skillCh, AIM_T, 10 / AIM_RECHARGE);
        P.skill = SK_COIN;
    }
    E[0].alive = 0;

    reset_game();                                         /* rocket jump */
    P.pos = P.prevPos = v3(-20, 0, 40); P.pitch = -1.5f;
    fire_rocket();
    player_tick(TICK, 1, 0);
    maxY = 0;
    for (int i = 0; i < 200; i++) { proj_tick(TICK); player_tick(TICK, 0, 0); maxY = fmaxf(maxY, P.pos.y); }
    fprintf(f, "rocket jump : apogee=%.2f hp=%.0f (attendu apogee > 3)\n", maxY, P.hp);

    reset_game();                                         /* grappin vers le mur nord */
    P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = PI; P.pitch = 0.35f;
    hookPressIn = 1; hookHeldIn = 1;
    int attachedTicks = 0;
    float maxSp = 0;
    V3 anchor = v3(0, 0, 0);
    for (int i = 0; i < 150; i++) {
        player_tick(TICK, 0, 0);
        hookPressIn = 0;
        if (P.hook == HOOK_ATTACHED) { attachedTicks++; anchor = P.anchor; }
        maxSp = fmaxf(maxSp, vlen(P.vel));
    }
    fprintf(f, "grappin : ancre=(%.1f, %.1f, %.1f) accroche %d ticks, vitesse max=%.1f, pos=(%.1f, %.1f, %.1f) (attendu ancre z=78, pos proche du mur)\n",
            anchor.x, anchor.y, anchor.z, attachedTicks, maxSp, P.pos.x, P.pos.y, P.pos.z);

    reset_game();                                         /* lacher en plein vol : l'elan est conserve */
    P.pos = P.prevPos = v3(-20, 0, 30); P.yaw = PI; P.pitch = 0.35f;
    hookPressIn = 1; hookHeldIn = 1;
    for (int i = 0; i < 60; i++) { player_tick(TICK, 0, 0); hookPressIn = 0; }
    float spBefore = vlen(P.vel);
    int stillAttached = P.hook == HOOK_ATTACHED;
    player_tick(TICK, 1, 0);                              /* saut = lacher + boost */
    float spAfter = vlen(P.vel), vyAfter = P.vel.y;
    hookHeldIn = 0;
    fprintf(f, "lacher : accroche=%d vitesse avant=%.1f apres=%.1f vy=%.1f hook=%d (attendu vitesse conservee, hook=0)\n",
            stillAttached, spBefore, spAfter, vyAfter, P.hook);

    /* grappin sur un drone : traction, impact, rebond */
    reset_game();
    P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0.16f;
    E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 4, 25);
    hookPressIn = 1; hookHeldIn = 1;
    int wasHooked = 0;
    float impactSp = 0;
    for (int i = 0; i < 120 && E[0].alive && E[0].hp == 100; i++) {
        player_tick(TICK, 0, 0);
        hookPressIn = 0;
        if (P.hookEnemy == 1) { wasHooked = 1; impactSp = vlen(P.vel); }
    }
    hookHeldIn = 0;
    fprintf(f, "grappin ennemi : accroche=%d vitesse a l'impact=%.1f drone vivant=%d hp=%.0f vy apres=%.1f hookCd=%.2f (attendu impact, drone mort, rebond)\n",
            wasHooked, impactSp, E[0].alive, E[0].hp, P.vel.y, P.hookCd);

    /* corps a corps : a l'arret puis lance a 20 m/s, avec recharge du dash */
    for (int pass = 0; pass < 2; pass++) {
        reset_game();
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        P.vel = pass ? v3(0, 0, -20) : v3(0, 0, 0);
        P.dashCharge = 0.2f;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.6f, 38);
        melee();
        fprintf(f, "corps a corps a %.0f m/s : hp drone=%.0f vivant=%d dash=%.1f (attendu %s, dash +1)\n",
                vlen(P.vel), E[0].hp, E[0].alive, P.dashCharge, pass ? "drone mort" : "hp 65");
    }

    /* parry d'une orbe : elle repart vers le drone vise et le detruit */
    reset_game();
    P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0.05f; P.hp = 50;
    E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 3, 20);
    spawn_proj(PR_ORB, v3(-20, 1.7f, 38), v3(0, 0, 16));
    melee();
    int kindAfter = PR[0].kind;
    hitstop = 0;
    for (int i = 0; i < 120; i++) proj_tick(TICK);
    fprintf(f, "parry : projectile renvoye=%d hp joueur=%.0f drone vivant=%d (attendu renvoye=1, hp 75 = 50 + 10 parry + 15 kill, drone mort)\n",
            kindAfter == PR_PARRIED, P.hp, E[0].alive);

    /* chaine de tir : 3 touches puis un rate */
    reset_game();
    P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
    float dmgSeen[3];
    for (int s = 0; s < 3; s++) {
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 30);
        fire_rail();
        dmgSeen[s] = 1000 - E[0].hp;
    }
    int before = shotStreak;
    P.yaw = PI;                                           /* on vise le mur derriere */
    fire_rail();
    fprintf(f, "chaine : degats %.0f / %.0f / %.0f, chaine=%d puis apres un rate=%d (attendu degats croissants, 3 puis 0)\n",
            dmgSeen[0], dmgSeen[1], dmgSeen[2], before, shotStreak);

    /* loadout et munitions a jauge : spawn, conversion des points, surplus, maximum */
    reset_game();
    {
        int spawnOk = P.ammo[WP_RAIL] == 2 && P.ammo[WP_ROCKET] == 4 && P.ammoGauge[WP_RAIL] == 0 &&
                      P.weapon == WP_RAIL && P.meleeKind == MK_SWORD;
        P.weapon = WP_ROCKET; P.ammo[WP_ROCKET] = 0;
        ammo_points(50, AP_HIT);
        int conv1 = P.ammo[WP_ROCKET]; float g1 = P.ammoGauge[WP_ROCKET];
        ammo_points(70, AP_HIT);
        int conv2 = P.ammo[WP_ROCKET]; float g2 = P.ammoGauge[WP_ROCKET];
        P.weapon = WP_RAIL; P.ammo[WP_RAIL] = 2; P.ammo[WP_ROCKET] = 4; P.ammoGauge[WP_ROCKET] = 0;
        ammo_points(100, AP_HIT);                          /* rail plein : tout part aux roquettes */
        int spill = P.ammo[WP_ROCKET];
        P.weapon = WP_ROCKET; P.ammo[WP_ROCKET] = 7; P.ammoGauge[WP_ROCKET] = 30;
        float lostBefore = apStat[AP_LOST];
        ammo_points(100, AP_HIT);                          /* 30 + 100 : une roquette, puis le maximum */
        int atMax = P.ammo[WP_ROCKET]; float gMax = P.ammoGauge[WP_ROCKET], fMax = gauge_frac(WP_ROCKET);
        float lost = apStat[AP_LOST] - lostBefore;
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = PI; P.pitch = -0.5f;
        fire_rocket();                                     /* un rocket jump coute une roquette */
        fprintf(f, "jauges : spawn ok=%d, 50 pts -> %d (jauge %.0f), +70 -> %d (jauge %.0f), surplus -> roquettes %d, "
                   "maximum %d jauge %.0f affichee %.0f perdus %.0f, apres un tir %d (attendu 1, 1 0, 2 20, 6, 8 0 1 80, 7)\n",
                spawnOk, conv1, g1, conv2, g2, spill, atMax, gMax, fMax, lost, P.ammo[WP_ROCKET]);
    }

    /* l'epee remplit la jauge de l'arme en main : touche +50, kill +100 */
    reset_game();
    {
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        P.weapon = WP_ROCKET; P.ammo[WP_ROCKET] = 0;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.6f, 38);
        melee();
        int afterHit = P.ammo[WP_ROCKET];
        E[0].hp = 10;
        melee();
        fprintf(f, "epee -> munitions : touche %d roquette(s), touche + kill %d (attendu 1, 4)\n", afterHit, P.ammo[WP_ROCKET]);
    }

    /* cellules : deux tailles, points aux deux armes, refusee si tout est plein, reapparition */
    reset_game();
    {
        int cell = 6;                                      /* une petite cellule au sol, dans un coin */
        P.ammo[WP_RAIL] = 0; P.ammo[WP_ROCKET] = 2;
        P.pos = P.prevPos = v3(PU[cell].pos.x, 0, PU[cell].pos.z);
        pickup_tick(TICK);
        float railG = P.ammoGauge[WP_RAIL]; int rockets = P.ammo[WP_ROCKET], gone = PU[cell].t > 0;
        P.ammo[WP_RAIL] = 2; P.ammo[WP_ROCKET] = 8; PU[cell].t = 0;
        pickup_tick(TICK);
        int refused = PU[cell].t <= 0;
        PU[cell].t = PICKUP_RESPAWN;
        P.pos = P.prevPos = v3(0, 0, 30);
        for (int i = 0; i < (int)(PICKUP_RESPAWN / TICK) + 2; i++) pickup_tick(TICK);
        fprintf(f, "cellules : %d cellules (grandes %d %d), rail jauge 0 -> %.0f, roquettes 2 -> %d, prise=%d, pleine refusee=%d, "
                   "revenue apres 12 s=%d (attendu 13, 150 150, 100, 4, 1, 1, 1)\n",
                npu, PU[0].pts, PU[5].pts, railG, rockets, gone, refused, PU[cell].t <= 0);
    }

    /* pieces : un drone hors de la ligne de mire, on tire dans la piece, le ricochet le trouve */
    for (int pass = 0; pass < 2; pass++) {
        reset_game();
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0.1f;
        P.vel = v3(0, 0, -20);                             /* a pleine vitesse */
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-34, 3, 22);
        throw_coin();
        if (pass) { P.yaw = 0.15f; throw_coin(); P.yaw = 0; }   /* deuxieme piece : chaine */
        for (int i = 0; i < 30; i++) { coin_tick(TICK); P.pos = vadd(P.pos, vmul(P.vel, TICK)); }
        int c0 = -1;
        for (int i = 0; i < MAXCOIN; i++) if (CO[i].alive && c0 < 0) c0 = i;
        V3 d = vsub(CO[c0].pos, eye_pos());
        P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
        lastChain = 0;
        float coinsLeft = P.skillCh;
        fire_rail();
        fprintf(f, "piece%s : chaine %d, degats au drone %.0f (attendu %.0f), pieces restantes %.0f" "\n",
                pass ? "s x2" : "", lastChain, 1000 - E[0].hp, pass ? RAIL_DMG * 2.25f : RAIL_DMG * 1.5f, coinsLeft);
    }

    /* --- Elan : degats selon la vitesse depensee, freinage, chaine, piece, slide, munitions, fenetre du dash --- */
    {
        static const float vs[4] = { 0, 9.5f, 15, 25 };
        float dm[4], hl[4];
        for (int s = 0; s < 4; s++) {                      /* 1. tir droit devant a 0 / 9,5 / 15 / 25 m/s */
            reset_game();
            P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
            P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1;
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 30);
            P.vel = v3(0, 0, -vs[s]);
            fire_kinetic();
            dm[s] = 1000 - E[0].hp; hl[s] = hlen(P.vel);
        }
        fprintf(f, "elan a 0 / 9.5 / 15 / 25 m/s : degats %.1f / %.1f / %.1f / %.1f, vitesse apres %.1f / %.1f / %.1f / %.1f "
                   "(attendu 20 / 67.5 / 95 / 95, 0 / 0 / 0 / 0 : le tir enleve toute la vitesse horizontale, plus seulement les 15 m/s convertis)\n",
                dm[0], dm[1], dm[2], dm[3], hl[0], hl[1], hl[2], hl[3]);

        reset_game();                                      /* 2. vitesse sur le cote et en montee */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 30);
        P.vel = v3(12, 6, 5);
        fire_kinetic();
        fprintf(f, "elan sur le cote a 13 m/s : degats %.1f, vitesse apres %.1f, vy %.1f (attendu 85, 0, 6)\n",
                1000 - E[0].hp, hlen(P.vel), P.vel.y);

        reset_game();                                      /* 3. tir rate : le freinage s'applique quand meme */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 1.2f;
        P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 30);
        P.vel = v3(0, 0, -15);
        fire_kinetic();
        fprintf(f, "elan rate a 15 m/s : vitesse apres %.1f, chaine %d (attendu 0, 0)\n", hlen(P.vel), shotStreak);

        reset_game();                                      /* 4. chaine : 3 tirs a 15 m/s */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1;
        for (int s = 0; s < 3; s++) {
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 30);
            P.vel = v3(0, 0, -15);
            fire_kinetic();
            dm[s] = 1000 - E[0].hp;
        }
        fprintf(f, "elan chaine a 15 m/s : degats %.2f / %.2f / %.2f (attendu 95 / 109.25 / 123.5)\n", dm[0], dm[1], dm[2]);

        reset_game();                                      /* 5. piece : ricochet x1,5 sur les degats convertis */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0.1f;
        P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1;
        P.vel = v3(0, 0, -20);
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-34, 3, 22);
        throw_coin();
        for (int i = 0; i < 30; i++) { coin_tick(TICK); P.pos = vadd(P.pos, vmul(P.vel, TICK)); }
        {
            int c0 = -1;
            for (int i = 0; i < MAXCOIN; i++) if (CO[i].alive && c0 < 0) c0 = i;
            V3 d = vsub(CO[c0].pos, eye_pos());
            P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
        }
        fire_kinetic();
        fprintf(f, "elan piece a 20 m/s : degats au drone %.1f, vitesse apres %.1f (attendu 142.5, 0 : toute la vitesse part avec le tir)\n",
                1000 - E[0].hp, hlen(P.vel));

        reset_game();                                      /* 6. slide : degats pleins, le tir arrete le slide */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        player_tick(TICK, 0, 0);                           /* pose au sol */
        P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 30);
        act[A_CROUCH] = 1; P.vel = v3(0, 0, -12);
        player_tick(TICK, 0, 0);
        int slid = P.sliding;
        fire_kinetic();
        player_tick(TICK, 0, 0);
        memset(act, 0, sizeof act);
        fprintf(f, "elan en slide : slide avant=%d, degats %.1f, slide apres=%d (attendu 1, 95, 0)\n", slid, 1000 - E[0].hp, P.sliding);

        reset_game();                                      /* 7. chargeur vide : clic sec, bascule, aucun freinage */
        P.pos = P.prevPos = v3(-20, 6, 40); P.yaw = 0; P.pitch = 0;
        P.loadout[0] = WP_KINETIC; P.loadout[1] = WP_RAIL; P.weapon = WP_KINETIC;
        P.ammo[WP_KINETIC] = 0; P.fireCd = 0; P.switchT = 0;
        P.vel = v3(0, 0, -15);
        botHeld[A_FIRE] = 1;
        game_tick(TICK);
        botHeld[A_FIRE] = 0;
        fprintf(f, "elan a sec : munitions %d, arme %s, vitesse %.1f (attendu 0, RAILGUN, 15)\n", P.ammo[WP_KINETIC],
                ammoDef[P.weapon].name, hlen(P.vel));

        /* 10 a 14. fenetre du dash-tir (en l'air, sans friction) */
        float wv[7];
        static const int pre[7] = { 0, 12, 13, 24, 73, 30, -1 };   /* ticks apres le dash ; -1 = cas 14 */
        for (int s = 0; s < 6; s++) {
            reset_game();
            memset(act, 0, sizeof act);
            P.pos = P.prevPos = v3(-20, 8, 40); P.yaw = 0; P.pitch = 0;
            P.dashCharge = 2; P.kinDashT = 1;
            if (s == 3) P.vel = v3(0, 0, -9.5f);
            player_tick(TICK, 0, 1);                       /* dash */
            for (int i = 0; i < pre[s]; i++) player_tick(TICK, 0, 0);
            if (s == 5) {                                  /* 14. deuxieme dash pendant le malus, puis 24 ticks */
                player_tick(TICK, 0, 1);
                for (int i = 0; i < 24; i++) player_tick(TICK, 0, 0);
            }
            wv[s] = kin_vdep();
            if (s == 3) wv[6] = hlen(P.vel);
        }
        fprintf(f, "dash-tir : vdep %.1f apres 0 tick (attendu 15)\n", wv[0]);
        fprintf(f, "dash-tir : vdep %.1f apres 12 ticks, %.1f apres 13 ticks (attendu 15, 0)\n", wv[1], wv[2]);
        fprintf(f, "dash-tir : vdep %.1f apres 24 ticks depuis 9.5 m/s, vitesse %.1f (attendu 9.5, 18)\n", wv[3], wv[6]);
        fprintf(f, "dash-tir : vdep %.1f apres 73 ticks (attendu 15)\n", wv[4]);
        fprintf(f, "dash-tir : vdep %.1f apres un 2e dash pendant le malus + 24 ticks (attendu 0)\n", wv[5]);

        reset_game();                                      /* 15. la melee remplit la jauge de l'Elan (80 pts) */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        P.loadout[0] = WP_KINETIC; P.loadout[1] = WP_RAIL; P.weapon = WP_KINETIC;
        P.ammo[WP_KINETIC] = 3; P.ammoGauge[WP_KINETIC] = 0;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 38);
        melee();
        P.meleeCd = 0; P.parryWin = 0; hitstop = 0;
        melee();
        fprintf(f, "elan jauge : 2 coups d'epee a 3 munitions -> munitions %d, jauge %.0f (attendu 4, 0 : plafond de %d munitions, le surplus de 20 va a l'autre arme)\n",
                P.ammo[WP_KINETIC], P.ammoGauge[WP_KINETIC], ammoDef[WP_KINETIC].max);
        P.ammo[WP_KINETIC] = 2; P.ammoGauge[WP_KINETIC] = 0; P.meleeCd = 0; P.parryWin = 0; hitstop = 0;
        melee();
        P.meleeCd = 0; P.parryWin = 0; hitstop = 0;
        melee();
        fprintf(f, "elan jauge : a 2 munitions, 2 coups d'epee -> munitions %d, jauge %.0f (attendu 3, 20 : 100 points = une munition (80) + 20)\n",
                P.ammo[WP_KINETIC], P.ammoGauge[WP_KINETIC]);
        reset_game();
        fprintf(f, "elan munitions : au depart %d, plafond %d (attendu 4, 4 : 3 et 6 avant)\n", P.ammo[WP_KINETIC], ammoDef[WP_KINETIC].max);
    }

    /* grappin plus nerveux : vitesse atteinte en 0,5 s vers un mur */
    reset_game();
    P.pos = P.prevPos = v3(-20, 0, 30); P.yaw = PI; P.pitch = 0.35f;
    hookPressIn = 1; hookHeldIn = 1;
    float sp05 = 0;
    for (int i = 0, since = 0; i < 240 && since < 60; i++) {   /* mesure 0,5 s apres l'accroche */
        player_tick(TICK, 0, 0); hookPressIn = 0; sp05 = vlen(P.vel);
        if (P.hook == HOOK_ATTACHED) since++;
    }
    hookHeldIn = 0;
    fprintf(f, "grappin : vitesse 0,5 s apres l'accroche = %.1f m/s (attendu > 25)\n", sp05);

    /* --- Enclume (solo) --- */
    {   /* 1. pique depuis 13 m, drone a 2 m du point d'impact : onde, rebond, dash, points de jauge */
        reset_game();
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.6f, 38);
        P.dashCharge = 0.5f;
        P.meleeKind = MK_ANVIL; P.pos = P.prevPos = v3(-20, 13, 40); P.grounded = 0; P.meleeCd = 0;
        anvil_press();
        float dashBefore = P.dashCharge;
        int hits0 = slamWaveHits;
        for (int i = 0; i < 700 && (P.anvilState == ANVIL_DIVE || (SW.alive && slamWaveHits == hits0)); i++) {
            dashBefore = P.dashCharge; player_tick(TICK, 0, 0);
        }
        fprintf(f, "enclume pique 13 m : |vy| %.1f, PV drone %.1f, vy apres %.1f, dash %+.2f, roquettes %d jauge %.0f "
                   "(attendu ~39.0, ~%.1f, 9.0, +1.00, 5 25)\n",
                P.anvilLastVy, E[0].hp, P.vel.y, P.dashCharge - dashBefore - TICK / DASH_RECHARGE,
                P.ammo[WP_ROCKET], P.ammoGauge[WP_ROCKET], 100 - test_slam_expect(13, P.anvilLastVy, 2, enemy_radius(EN_DRONE)));
    }
    {   /* 2. pique rate : immobilise 0,6 s + 0,02 s par metre de chute (ni deplacement, ni saut, ni dash), puis libre */
        reset_game();
        test_slam_drop(v3(-20, 13, 40), v3(0, 0, -5));
        int st0 = P.anvilState; float hv = hlen(P.vel), left = P.anvilStunT;
        V3 p0 = P.pos;
        act[A_FWD] = 1;
        for (int i = 0; i < 36; i++) player_tick(TICK, 1, 1);
        float moved = vlen(vsub(P.pos, p0)); int st1 = P.anvilState;
        for (int i = 0; i < 48; i++) player_tick(TICK, 0, 0);
        act[A_FWD] = 0;
        fprintf(f, "enclume rate : etat %d, vitesse horizontale %.1f, deplacement en 0,3 s (saut et dash appuyes) %.2f m etat %d, "
                   "etat apres 0,7 s %d ; immobilisation %.2f s, restante a la fin de l'onde %.2f (attendu 2, 0.0, 0.00 2, 0 ; %.2f, %.2f)\n",
                st0, hv, moved, st1, P.anvilState, slamStunFull, left, slam_stun(13), slam_stun(13) - slam_R(13) / SLAM_WAVE_SPEED);
    }
    {   /* 3. trop pres du sol : c'est le coup au sol, meme en l'air */
        reset_game();
        P.meleeKind = MK_ANVIL; P.pos = P.prevPos = v3(-20, 0.8f, 40); P.grounded = 0;
        anvil_press();
        fprintf(f, "enclume a 0,8 m du sol : coup au sol=%d, etat %d, recharge %.2f (attendu 1, 0, 0.80)\n",
                P.meleeT > 0, P.anvilState, P.meleeCd);
    }
    {   /* 4. pique bas (2 m) : facteur de hauteur kh = 0,5 */
        reset_game();
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-18.6f, 1.0f, 40);
        test_slam_drop(v3(-20, 2, 40), v3(0, 0, 0));
        fprintf(f, "enclume pique bas (2 m) : |vy| %.1f, kh %.2f, degats %.1f (attendu 31.6, 0.50, %.1f)\n",
                P.anvilLastVy, slam_kh(2), 100 - E[0].hp, test_slam_expect(2, P.anvilLastVy, 1.4f, enemy_radius(EN_DRONE)));
    }
    {   /* 5. plafond de la vitesse de chute */
        reset_game();
        test_slam_drop(v3(-20, 40, 40), v3(0, 0, 0));
        fprintf(f, "enclume pique de 40 m : |vy| %.1f (attendu 45.0)\n", P.anvilLastVy);
    }
    for (int pass = 0; pass < 2; pass++) {   /* 6. coup au sol : a l'arret, puis a 10 m/s */
        reset_game();
        P.meleeKind = MK_ANVIL; P.pos = P.prevPos = v3(-20, 0, 40); P.grounded = 1; P.yaw = 0; P.pitch = 0;
        P.vel = pass ? v3(0, 0, -10) : v3(0, 0, 0);
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.6f, 38);
        melee();
        fprintf(f, "enclume coup au sol a %.0f m/s : PV drone %.0f, recharge %.2f, fenetre de parry %.2f (attendu %s, 0.80, 0.10)\n",
                vlen(P.vel), E[0].hp, P.meleeCd, P.parryWin, pass ? "25" : "55");
    }
    {   /* 7. le dash annule le pique ; pack_state annonce le pique (ST_SLAM) */
        reset_game();
        P.meleeKind = MK_ANVIL; P.pos = P.prevPos = v3(-20, 13, 40); P.grounded = 0; P.pitch = 0;
        anvil_press();
        for (int i = 0; i < 5; i++) player_tick(TICK, 0, 0);
        NetPState s;
        pack_state(&s);
        float dc = P.dashCharge;
        player_tick(TICK, 0, 1);
        fprintf(f, "enclume dash pendant le pique : pad %d, etat %d, vy %.1f, charges %.2f -> %.2f, recharge %.2f "
                   "(attendu %d, 0, 1.8 (2 moins un tick de gravite), 2.00 -> 1.00, 0.80)\n",
                s.pad, P.anvilState, P.vel.y, dc, P.dashCharge, P.meleeCd, MK_ANVIL | ST_SLAM);
    }
    {   /* 7b. onde du plan 1b : taille et mur selon la hauteur, ovale selon la vitesse, propagation, une touche par onde */
        static const float hs[4] = { 2, 7, 13, 20 };
        char b[160]; int n = 0;
        for (int k = 0; k < 4; k++) n += snprintf(b + n, sizeof b - n, "H %.0f : R %.1f mur %.1f ; ", hs[k], slam_R(hs[k]), slam_hw(hs[k]));
        fprintf(f, "enclume onde : %s (attendu 3.1/0.8, 4.6/1.3, 6.4/1.9, 8.5/2.6)\n", b);
        fprintf(f, "enclume ovale a 0 / 20 m/s : avant x%.2f / x%.2f, arriere x%.2f / x%.2f (attendu 1.00/1.80, 1.00/0.60)\n",
                slam_fwd(0), slam_fwd(20), slam_back(0), slam_back(20));
        /* drone a 6 m : touche quand le front passe (vers 0,2 s), pas avant ; il n'est touche qu'une fois */
        reset_game();
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 0.95f, 34);
        P.meleeKind = MK_ANVIL; P.pos = P.prevPos = v3(-20, 13, 40); P.vel = v3(0, 0, 0); P.grounded = 0; P.meleeCd = 0;
        anvil_press();
        for (int i = 0; i < 600 && P.anvilState == ANVIL_DIVE; i++) player_tick(TICK, 0, 0);
        float tHit = -1, hpAt = E[0].hp;
        for (int i = 0; i < 120 && SW.alive; i++) {
            player_tick(TICK, 0, 0);
            if (tHit < 0 && E[0].hp < hpAt) tHit = SW.t;
        }
        float dOnce = 1000 - E[0].hp;
        fprintf(f, "enclume propagation : drone a 6 m touche a %.2f s, degats %.1f, une seule touche %d (attendu ~%.2f, %.1f, 1)\n",
                tHit, dOnce, fabsf(dOnce - test_slam_expect(13, P.anvilLastVy, 6, enemy_radius(EN_DRONE))) < 0.5f,
                6 / (slam_R(13) + enemy_radius(EN_DRONE)) * slam_R(13) / SLAM_WAVE_SPEED,
                test_slam_expect(13, P.anvilLastVy, 6, enemy_radius(EN_DRONE)));
        /* cible en l'air (pieds a 1,5 m) : un petit pique (2 m, mur 0,8 m) passe dessous, un gros (13 m, mur 1,9 m) la touche */
        float hpSmall, hpBig;
        reset_game();
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.5f + enemy_radius(EN_DRONE), 38.5f);
        test_slam_drop(v3(-20, 2, 40), v3(0, 0, 0));
        hpSmall = E[0].hp;
        reset_game();
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.5f + enemy_radius(EN_DRONE), 38.5f);
        test_slam_drop(v3(-20, 13, 40), v3(0, 0, 0));
        hpBig = E[0].hp;
        fprintf(f, "enclume mur : cible a 1,5 m du sol, petit pique PV %.0f, gros pique touche %d (attendu 1000, 1)\n", hpSmall, hpBig < 1000);
        /* ovale : en courant a 20 m/s vers -z, un drone a 9 m devant est touche, le meme a 9 m derriere non */
        float hpFront, hpBack;
        for (int side = 0; side < 2; side++) {
            reset_game();
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000;
            P.meleeKind = MK_ANVIL; P.pos = P.prevPos = v3(-20, 13, 46); P.vel = v3(0, 0, -20); P.grounded = 0; P.meleeCd = 0;
            V3 at; float vy, R, tf;
            P.anvilState = ANVIL_DIVE; P.anvilStartY = P.pos.y; P.vel.y = -SLAM_VY;
            slam_predict(&at, &vy, &R, &tf);
            E[0].pos = v3(at.x, 0.95f, at.z + (side ? 9.0f : -9.0f));
            P.anvilState = ANVIL_IDLE; P.vel = v3(0, 0, -20);
            test_slam_drop(P.pos, P.vel);
            if (side) hpBack = E[0].hp; else hpFront = E[0].hp;
        }
        fprintf(f, "enclume ovale : drone a 9 m devant touche %d, derriere touche %d (attendu 1, 0)\n", hpFront < 1000, hpBack < 1000);
    }
    {   /* 8. pique sur le jump pad est : onde, puis le pad relance (pas d'immobilisation) */
        reset_game();
        P.meleeKind = MK_ANVIL; P.anvilState = ANVIL_IDLE; P.meleeCd = 0;   /* sans test_slam_drop : vitesse lue au rebond */
        P.pos = P.prevPos = v3(ms(16), 5, 0); P.vel = v3(0, 0, 0); P.grounded = 0;
        anvil_press();
        for (int i = 0; i < 600 && P.anvilState == ANVIL_DIVE; i++) player_tick(TICK, 0, 0);
        V3 L = v3(ms(13), 20, 0);
        fprintf(f, "enclume sur un jump pad : onde=%d, vitesse (%.1f, %.1f, %.1f), etat %d (attendu 1, (%.1f, %.1f, %.1f), 0)\n",
                P.anvilLandT == 0, P.vel.x, P.vel.y, P.vel.z, P.anvilState, L.x, L.y, L.z);
    }

    /* --- SHOTGUN (solo) : motif, cone, degats selon la distance, recul, pieces, munitions --- */
    {
        V3 dA[SG_PELLETS], dB[SG_PELLETS], dC[SG_PELLETS], d0 = vnorm(v3(0.3f, -0.2f, -0.93f));
        unsigned int rng0 = rng;                           /* 1-2. meme graine = memes plombs, rng global intact */
        sg_pellet_dirs(d0, SG_HALF_ANGLE, 1234u, dA);
        sg_pellet_dirs(d0, SG_HALF_ANGLE, 1234u, dB);
        sg_pellet_dirs(d0, SG_HALF_ANGLE, 98765u, dC);
        fprintf(f, "shotgun motif : meme graine identique %d, autre graine differente %d, rng global intact %d (attendu 1, 1, 1)\n",
                !memcmp(dA, dB, sizeof dA), memcmp(dA, dC, sizeof dA) != 0, rng == rng0);
        float amax[2] = { 0, 0 };                          /* 3. angle max sur 200 graines, debout puis en slide */
        for (int pass = 0; pass < 2; pass++)
            for (unsigned int sd = 1; sd <= 200; sd++) {
                sg_pellet_dirs(d0, SG_HALF_ANGLE * (pass ? SG_SLIDE_SPREAD : 1), sd * 7919u, dA);
                for (int k = 0; k < SG_PELLETS; k++)
                    amax[pass] = fmaxf(amax[pass], atan2f(vlen(vcross(d0, dA[k])), vdot(d0, dA[k])) * 180 / PI);
            }
        fprintf(f, "shotgun cone : angle max %.2f deg debout, %.2f en slide (attendu <= 4.50, <= 2.25)\n", amax[0], amax[1]);

        static const float dz[3] = { 5, 10, 20 };          /* 4, 5 b, 5. drone a 5 / 10 / 20 m */
        int pel[3]; float hpD[3];
        for (int s = 0; s < 3; s++) {
            reset_game();
            P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0; P.grounded = 1;
            P.ammo[WP_SHOTGUN] = 6; shotStreak = 0;
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 40 - dz[s]);
            fire_shotgun();
            pel[s] = P.sgLastPellets; hpD[s] = 1000 - E[0].hp; hitstop = 0;   /* degats subis (PV 1000 : le drone ne meurt pas) */
        }
        fprintf(f, "shotgun drone a 5 / 10 / 20 m : plombs %d / %d / %d, degats %.1f / %.1f / %.1f "
                   "(attendu 10 / 10 / 4, %.0f / %.0f / ~53.8 : 10 plombs x %.0f, x2,5 par rapport a avant)\n",
                pel[0], pel[1], pel[2], hpD[0], hpD[1], hpD[2], SG_PELLETS * SG_PELLET_DMG, SG_PELLETS * SG_PELLET_DMG, SG_PELLET_DMG);

        reset_game();                                      /* 8 bis. poussee du tireur : PROPORTIONNELLE a sa vitesse actuelle, plafonnee, sol et air */
        {
            static const float spd[5] = { 0, 10, 20, 50, 100 };   /* m/s juste avant le tir, vers l'avant (-z), tir vers l'avant */
            float push[5]; int okDir = 0;
            for (int k = 0; k < 5; k++) {
                reset_game();
                P.pos = P.prevPos = v3(-20, 8, 40); P.grounded = 0; P.yaw = 0; P.pitch = 0;
                P.vel = v3(0, 0, -spd[k]); P.ammo[WP_SHOTGUN] = 6; P.grounded = 1;
                fire_shotgun(); hitstop = 0;
                push[k] = sgLastPush;
                okDir += P.pushT <= 0 && fabsf(P.vel.z - (-spd[k] + push[k])) < 0.01f;   /* vitesse ajoutee a l'oppose du tir (sol : 0 m/s de residu) */
            }
            fprintf(f, "shotgun poussee proportionnelle a la vitesse : a 0 / 10 / 20 / 50 / 100 m/s -> %.1f / %.1f / %.1f / %.1f / %.1f m/s "
                       "(attendu %.0f / %.0f / %.0f / %.0f / %.0f : x%.1f, AUCUN plafond (SG_KNOCK_MAX %.0f)), sens oppose au tir et aucune poussee a l'arret : %d / 5 (attendu 5)\n",
                    push[0], push[1], push[2], push[3], push[4], SG_KNOCK_RATIO * 0, SG_KNOCK_RATIO * 10, SG_KNOCK_RATIO * 20, SG_KNOCK_RATIO * 50,
                    SG_KNOCK_RATIO * 100, SG_KNOCK_RATIO, SG_KNOCK_MAX, okDir);

            float zv, yv;                                  /* vitesse apres le tir : la vitesse actuelle est AJOUTEE a l'oppose de la visee */
            reset_game();                                  /* sol : 30 m/s vers l'avant, tir devant : la vitesse est annulee */
            P.pos = P.prevPos = v3(-20, 0, 60); P.grounded = 1; P.yaw = 0; P.pitch = 0; P.vel = v3(0, 0, -30); P.ammo[WP_SHOTGUN] = 6;
            fire_shotgun(); hitstop = 0; zv = P.vel.z;
            reset_game();                                  /* air : chute a -20 m/s, tir vers le bas (le residu de 7 m/s s'ajoute) */
            P.pos = P.prevPos = v3(-20, 30, 40); P.grounded = 0; P.yaw = 0; P.pitch = -PI / 2; P.vel = v3(0, -20, 0); P.ammo[WP_SHOTGUN] = 6;
            fire_shotgun(); hitstop = 0; yv = P.vel.y;
            fprintf(f, "shotgun vitesse ajoutee : au sol a 30 m/s, tir devant, vz %.2f ; en l'air a -20 m/s, tir vers le bas, vy %.2f "
                       "(attendu 0.00 ; 7.00 = -20 + residu de 7 + 20 ajoutes vers le haut)\n", zv, yv);
        }

        reset_game();                                      /* 9-10. recul en l'air, plafonds, au sol */
        P.pos = P.prevPos = v3(-20, 8, 40); P.grounded = 0; P.yaw = 0; P.pitch = -PI / 2; P.ammo[WP_SHOTGUN] = 6;
        float rv[4];
        P.vel = v3(0, 0, 0); fire_shotgun(); rv[0] = P.vel.y;
        P.vel = v3(0, 8.4f, 0); fire_shotgun(); rv[1] = P.vel.y;
        P.vel = v3(0, -10, 0); fire_shotgun(); rv[2] = P.vel.y;
        P.vel = v3(0, 0, -20); P.yaw = PI; P.pitch = 0; fire_shotgun(); rv[3] = hlen(P.vel);   /* tir arriere apres un dash */
        P.grounded = 1; P.vel = v3(3, 0, -5); P.pitch = -PI / 2; fire_shotgun();
        fprintf(f, "shotgun recul : vy %.1f depuis 0, %.1f depuis 8.4, %.1f en chute a -10, vitesse %.1f (tir arriere a 20), "
                   "au sol (%.1f %.1f %.1f) (attendu 7.0, 17.4, 7.0, 40.0, (3.0 5.8 -5.0) : residu de recul puis vitesse actuelle ajoutee)\n",
                rv[0], rv[1], rv[2], rv[3], P.vel.x, P.vel.y, P.vel.z);
        hitstop = 0;

        reset_game();                                      /* 11. piece eclatee : drone a 2 m d'elle, tireur a 2 m */
        P.pos = P.prevPos = v3(-20, 0, 38); P.yaw = 0; P.pitch = 0; P.grounded = 1; P.ammo[WP_SHOTGUN] = 6; shotStreak = 0;
        CO[0].alive = 1; CO[0].owner = myId; CO[0].life = 3; CO[0].pos = v3(-20, 1.6f, 36);
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-18, 1.6f, 36);
        fire_shotgun();
        fprintf(f, "shotgun eclat : pieces eclatees %d, plombs %d, PV drone %.1f, PV joueur %.0f, chaine %d (attendu 1, 0, 71.2, 100, 1)\n",
                sgBursts, P.sgLastPellets, E[0].hp, P.hp, shotStreak);
        hitstop = 0;

        reset_game();                                      /* 12. deux pieces a 2 m : reaction en chaine */
        P.pos = P.prevPos = v3(-20, 0, 38); P.yaw = 0; P.pitch = 0; P.grounded = 1; P.ammo[WP_SHOTGUN] = 6;
        CO[0].alive = 1; CO[0].owner = myId; CO[0].life = 3; CO[0].pos = v3(-20, 1.6f, 36);
        CO[1].alive = 1; CO[1].owner = myId; CO[1].life = 3; CO[1].pos = v3(-18, 1.6f, 36);
        fire_shotgun();
        fprintf(f, "shotgun eclat en chaine : pieces eclatees %d, message \"%s\" (attendu 2, ECLAT x2)\n", sgBursts, popText);
        hitstop = 0;

        int wl0 = wantLoadout[0], wl1 = wantLoadout[1];    /* 14. loadout SHOTGUN + rail : 3 au spawn, 3 tirs, bascule */
        wantLoadout[0] = WP_SHOTGUN; wantLoadout[1] = WP_RAIL;
        reset_game();
        waveDelay = 1000;                                  /* aucun drone pendant le test */
        int spawnAmmo = P.ammo[WP_SHOTGUN], spawnW = P.weapon, shots = 0;
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = PI; P.pitch = 0;
        botHeld[A_FIRE] = 1;
        for (int i = 0; i < 480 && P.weapon == WP_SHOTGUN; i++) {
            int a0 = P.ammo[WP_SHOTGUN];
            game_tick(TICK); hitstop = 0;
            shots += P.ammo[WP_SHOTGUN] < a0;
        }
        botHeld[A_FIRE] = 0;
        fprintf(f, "shotgun munitions : au spawn %d (%s), %d tirs puis a sec : munitions %d, arme %s (attendu 3 (SHOTGUN), 3, 0, RAILGUN)\n",
                spawnAmmo, ammoDef[spawnW].name, shots, P.ammo[WP_SHOTGUN], ammoDef[P.weapon].name);

        P.weapon = WP_SHOTGUN; P.switchT = 0;              /* 15. a vide pendant 10 s : aucune recharge passive */
        for (int i = 0; i < 1200; i++) game_tick(TICK);
        fprintf(f, "shotgun a vide 10 s : munitions %d, jauge %.0f (attendu 0, 0)\n", P.ammo[WP_SHOTGUN], P.ammoGauge[WP_SHOTGUN]);

        reset_game();                                      /* 16. a vide, SHOTGUN en main : 2 coups d'epee = 1 cartouche */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        P.weapon = WP_SHOTGUN; P.ammo[WP_SHOTGUN] = 0; P.ammoGauge[WP_SHOTGUN] = 0;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 38);
        melee();
        P.meleeCd = 0; P.parryWin = 0; hitstop = 0;
        melee();
        hitstop = 0;
        int afterSword = P.ammo[WP_SHOTGUN]; float gSword = P.ammoGauge[WP_SHOTGUN];
        P.ammo[WP_SHOTGUN] = 5; P.ammoGauge[WP_SHOTGUN] = 0;   /* 17. a 5 : grande cellule (150) -> 6, jauge pleine */
        pickup_taken(0, myId);
        fprintf(f, "shotgun jauge : 2 coups d'epee -> %d cartouche(s), jauge %.0f ; a 5 + grande cellule (%d) -> %d, jauge affichee %.0f "
                   "(attendu 1, 0 ; 150, 6, 100)\n", afterSword, gSword, PU[0].pts, P.ammo[WP_SHOTGUN], 100 * gauge_frac(WP_SHOTGUN));
        PU[0].t = 0;
        wantLoadout[0] = wl0; wantLoadout[1] = wl1;
    }

    /* LAN simule (hote sans socket) : un bot contre une cible immobile */
    netMode = NET_HOST; connected = 1; myId = 0; botMode = 2; appState = APP_GAME;
    botLog = f;
    for (int pass = 0; pass < 2; pass++) {
        reset_game();
        dmMode = 1;
        memset(NP, 0, sizeof NP);
        NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Bot");
        NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible");
        if (pass == 0) {                                   /* cross-up : cible a 10 m devant */
            NP[1].disp = v3(-20, 0, 31);
            P.pos = P.prevPos = v3(-20, 0, 41);
        } else {                                           /* epee : la cible fonce sur le bot */
            NP[1].disp = v3(-20, 0, 37.5f);
            vf(NP[1].st.vel, v3(0, 0, 10));
            P.pos = P.prevPos = v3(-20, 0, 41);
            P.vel = v3(0, 0, -10);
        }
        vf(NP[1].st.pos, NP[1].disp);
        P.yaw = 0; P.dashCharge = 2; P.hp = 100; P.dead = 0;
        float maxY = 0;
        int behind = 0, hitBehind = 0, swordHits = 0;
        float hpBefore = NP[1].hp;
        P.dashCharge = pass ? 0.2f : 2;
        if (pass) P.fireCd = 10;                           /* test de l'epee seule : pas de rail */
        for (int i = 0; i < (pass ? 30 : 360); i++) {
            nowT += TICK;
            bot_think(TICK);
            game_tick(TICK);
            maxY = fmaxf(maxY, P.pos.y);
            if (P.pos.z < NP[1].disp.z - 1) behind = 1;
            if (NP[1].hp < hpBefore) {
                if (behind) hitBehind = 1;
                if (P.meleeT > 0.1f) swordHits++;
                hpBefore = NP[1].hp;
            }
            if (!NP[1].alive) break;
        }
        if (pass == 0)
            fprintf(f, "cross-up : hauteur max %.1f m, passe derriere=%d, touche dans le dos=%d, PV cible=%.0f (attendu 1, 1)\n",
                    maxY, behind, hitBehind, NP[1].hp);
        else
            fprintf(f, "epee a %.0f m/s relatifs : %d coup(s), PV cible=%.0f (attendu ~%.0f), dash=%.1f (attendu +1)\n",
                    20.0f, swordHits, NP[1].hp, 100 - fminf(melee_dmg(20) * PVP_SCALE, PVP_HIT_CAP), P.dashCharge);
    }
    /* parry : coup dans le vide = recharge longue */
    reset_game();
    dmMode = 0;
    melee();
    for (int i = 0; i < 24; i++) game_tick(TICK);
    fprintf(f, "coup dans le vide : recharge restante %.2f s apres 0,2 s (attendu ~0,7 au lieu de 0,25)" "\n", P.meleeCd);

    /* ping-pong : une roquette adverse renvoyee deux fois, puis le plafond de degats par touche */
    reset_game();
    dmMode = 1;
    memset(NP, 0, sizeof NP);
    NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
    NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Tireur");
    NP[1].disp = v3(-20, 0, 24); vf(NP[1].st.pos, NP[1].disp);
    P.pos = P.prevPos = v3(-20, 0, 41); P.yaw = 0; P.pitch = 0;
    {
        Proj *r = spawn_proj(PR_ROCKET, v3(-20, 1.6f, 38.5f), v3(0, 0, 38));
        r->owner = 1; r->netId = 0x02000009;
        melee();
        float m1 = r->mult, s1 = vlen(r->vel);
        int ok1 = r->kind == PR_PARRIED && r->owner == myId;
        r->owner = 1; r->pos = v3(-20, 1.6f, 38.5f); r->vel = v3(0, 0, s1);   /* l'adversaire la re-pare */
        P.meleeCd = 0; P.parryWin = 0;
        melee();
        fprintf(f, "ping-pong : renvoi 1 ok=%d x%.2f a %.0f m/s, renvoi 2 x%.2f a %.0f m/s (attendu x1.25 a 36, x1.44 a 41)" "\n",
                ok1, m1, s1, r->mult, vlen(r->vel));
        r->alive = 0;
    }
    NP[1].hp = 100;
    net_hit(1, 200, v3(0, 0, 0), W_MELEE);
    fprintf(f, "plafond PvP : une touche de 200 laisse %.0f PV (attendu 40)" "\n", NP[1].hp);

    /* Elan en LAN simule : x0,7 et plafond 60 par l'hote, puis aucune recharge passive */
    {
        static const float vs[3] = { 0, 9.5f, 15 };
        float hpLeft[3];
        NP[1].disp = v3(-20, 0, 31); vf(NP[1].st.pos, NP[1].disp);
        for (int s = 0; s < 3; s++) {
            P.pos = P.prevPos = v3(-20, 0, 41); P.yaw = 0; P.pitch = test_pitch_to(np_center(1));
            P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1; hitstop = 0;
            NP[1].hp = 100; NP[1].alive = 1;
            P.vel = v3(0, 0, -vs[s]);
            fire_kinetic();
            hpLeft[s] = NP[1].hp;
        }
        fprintf(f, "elan PvP a 0 / 9.5 / 15 m/s : PV cible %.2f / %.2f / %.2f (attendu 86 / 52.75 / 40)\n",
                hpLeft[0], hpLeft[1], hpLeft[2]);
        reset_game();
        dmMode = 1;
        NP[1].hp = 100;
        P.pos = P.prevPos = v3(-20, 0, 41); P.yaw = PI; P.pitch = 0;   /* vers le mur : aucune touche */
        P.loadout[0] = WP_KINETIC; P.loadout[1] = WP_RAIL; P.weapon = WP_KINETIC;
        P.ammo[WP_KINETIC] = 6; P.ammoGauge[WP_KINETIC] = 0; P.fireCd = 0; P.switchT = 0;
        botHeld[A_FIRE] = 1;
        for (int i = 0; i < 119; i++) { nowT += TICK; game_tick(TICK); }
        botHeld[A_FIRE] = 0;
        fprintf(f, "elan tenu 0.99 s : munitions %d, jauge %.0f (attendu 4, 0)\n", P.ammo[WP_KINETIC], P.ammoGauge[WP_KINETIC]);
    }

    /* parry d'epee : je suis en fenetre de parry quand l'epee adverse arrive, puis plus */
    {
        reset_game();
        dmMode = 1;
        memset(NP, 0, sizeof NP);
        NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
        NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Bretteur");
        NP[1].disp = v3(-20, 0, 38.5f); vf(NP[1].st.pos, NP[1].disp);
        P.pos = P.prevPos = v3(-20, 0, 41); P.dashCharge = 0.5f;
        P.parryWin = 0.1f;
        host_tick(TICK);
        NetEvent e = ev_new(EV_HIT);
        e.from = 1; e.target = 0; e.sub = W_MELEE; e.f[0] = 60;
        vf(&e.f[1], v3(0, 0, 10));                        /* recul porte par le coup */
        P.vel = v3(0, 0, 0);
        host_event_in(&e);                                /* comme un paquet recu du client 1 */
        float hpParried = NP[0].hp, dash = P.dashCharge;
        V3 velParried = P.vel;
        P.parryWin = 0; nowT += 1;
        host_tick(TICK);
        e.eid++;
        P.vel = v3(0, 0, 0);
        host_event_in(&e);
        fprintf(f, "parry d'epee : PV apres coup pare=%.0f, dash=%.1f ; sans parry PV=%.0f (attendu 100, 1.5, 40)" "\n",
                hpParried, dash, NP[0].hp);
        fprintf(f, "recul d'un coup pare : %.1f m/s ; sans parry %.1f m/s (attendu 0 et 10 : le coup pare n'est pas rejoue)" "\n",
                velParried.z, P.vel.z);
    }

    /* Enclume en LAN : l'onde sur un joueur au sol, pas sur un joueur en l'air ; puis l'onde paree par la victime */
    {
        reset_game();
        dmMode = 1;
        memset(NP, 0, sizeof NP);
        NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
        NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible");
        NP[1].disp = v3(-20, 0, 38); vf(NP[1].st.pos, NP[1].disp);
        nowT += 1;
        test_slam_drop(v3(-20, 13, 40), v3(0, 0, 0));      /* 9. cible au sol a 2 m, x 0,7 */
        float hpGround = NP[1].hp, vy13 = P.anvilLastVy;
        NP[1].hp = 100; NP[1].disp.y = 2.2f; vf(NP[1].st.pos, NP[1].disp);
        test_slam_drop(v3(-20, 13, 40), v3(0, 0, 0));      /* pieds a 2,2 m : au-dessus du mur (1,9 m pour 13 m de chute) */
        fprintf(f, "enclume LAN : PV apres l'onde %.1f, cible en l'air (pieds a 2,2 m) %.0f, etat %d (attendu %.1f, 100, 2)\n",
                hpGround, NP[1].hp, P.anvilState, 100 - test_slam_expect(13, vy13, 2, 0.5f) * PVP_SCALE);

        reset_game();                                      /* 10. la victime pare l'onde (0,18 s plus tot : tolerance 0,20 s) */
        dmMode = 1;
        NP[1].hp = 100; NP[1].disp.y = 0; vf(NP[1].st.pos, NP[1].disp);
        nowT += 1;
        NP[1].lastParryT = nowT - 0.18;
        int parriesBefore = meleeParries;
        P.meleeKind = MK_ANVIL; P.anvilState = ANVIL_IDLE; P.meleeCd = 0;   /* jusqu'au passage du front (parry), pas plus */
        P.pos = P.prevPos = v3(-20, 13, 40); P.vel = v3(0, 0, 0); P.grounded = 0;
        anvil_press();
        for (int i = 0; i < 700 && (P.anvilState == ANVIL_DIVE || (SW.alive && meleeParries == parriesBefore)); i++) player_tick(TICK, 0, 0);
        float hpParried = NP[1].hp, vyAtt = P.vel.y, cdAtt = P.meleeCd;
        int rockets = P.ammo[WP_ROCKET];
        net_hit(1, 10, v3(0, 0, 0), W_MELEE);              /* une epee a la meme distance n'est plus paree (0,15 s) */
        fprintf(f, "enclume onde paree : PV cible %.0f, vy attaquant %.1f, recharge %.2f, parries +%d, roquettes %d (points rendus) ; "
                   "epee a 0,18 s : PV %.0f (attendu 100, 12.0, 1.00, +1, 4 ; 90)\n",
                hpParried, vyAtt, cdAtt, meleeParries - parriesBefore, rockets, NP[1].hp);

        reset_game();                                      /* 11. bot a l'Enclume, 4 m au-dessus d'une cible au sol : il pique */
        dmMode = 1;
        NP[1].hp = 100; NP[1].lastParryT = 0; NP[1].disp = v3(-20, 0, 38); vf(NP[1].st.pos, NP[1].disp);
        P.meleeKind = MK_ANVIL; P.pos = P.prevPos = v3(-20, 4, 40); P.grounded = 0;
        int wavesBefore = slamWaveHits, dived = 0;
        for (int i = 0; i < 120 && slamWaveHits == wavesBefore; i++) {
            nowT += TICK;
            bot_think(TICK);
            game_tick(TICK);
            dived |= P.anvilState == ANVIL_DIVE;
        }
        fprintf(f, "bot enclume : pique=%d, onde qui touche=%d, PV cible %.0f (attendu 1, 1, < 100)\n",
                dived, slamWaveHits - wavesBefore, NP[1].hp);
    }

    /* SHOTGUN en LAN simule : un seul EV_HIT par tir (agregation), x0,7, plafond 60, chaine a 3 plombs, traceurs recus */
    {
        static const float dist[6] = { 3, 10, 3, 30, 10, 14 };
        static const int streak0[6] = { 0, 0, 8, 2, 2, 0 };
        float hpLeft[6]; int evHits[6], streakAfter[6], pel[6];
        for (int s = 0; s < 6; s++) {
            reset_game();
            dmMode = 1;
            memset(NP, 0, sizeof NP);
            NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
            NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible");
            P.pos = P.prevPos = v3(-20, 0, 60); P.grounded = 1;
            NP[1].disp = v3(-20, 0, 60 - dist[s]); vf(NP[1].st.pos, NP[1].disp);
            V3 d = vsub(np_center(1), eye_pos());
            P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
            P.ammo[WP_SHOTGUN] = 6; shotStreak = streak0[s]; hitstop = 0;
            int h0 = hitCount[W_SHOTGUN];
            fire_shotgun();
            hpLeft[s] = NP[1].hp; evHits[s] = hitCount[W_SHOTGUN] - h0; streakAfter[s] = shotStreak; pel[s] = P.sgLastPellets;
        }
        hitstop = 0;
        fprintf(f, "shotgun PvP a 3 m : %d EV_HIT, %d plombs, PV %.1f (attendu 1, 10, 40 : %.0f demandes, plafonnes a %.0f)\n",
                evHits[0], pel[0], hpLeft[0], pel[0] * SG_PELLET_DMG * PVP_SCALE, PVP_HIT_CAP);
        fprintf(f, "shotgun PvP a 10 m : %d EV_HIT, %d plombs, PV %.1f (attendu 1, 6 a 8 selon la rotation, %.1f = 100 - min(plombs x 14, 60))\n",
                evHits[1], pel[1], hpLeft[1], 100 - fminf(pel[1] * SG_PELLET_DMG * PVP_SCALE, PVP_HIT_CAP));
        fprintf(f, "shotgun PvP a 3 m, chaine 8 : PV %.1f (attendu 40 : plafond de 60)\n", hpLeft[2]);
        fprintf(f, "shotgun PvP a 30 m, chaine 2 : %d plomb(s), chaine apres %d (attendu 1 a 2, 0 : moins de 3 plombs)\n", pel[3], streakAfter[3]);
        fprintf(f, "shotgun PvP a 10 m, chaine 2 : %d plombs, chaine apres %d, PV %.1f (attendu 6 a 8, 3, %.1f = 100 - min(plombs x 18.2, 60))\n",
                pel[4], streakAfter[4], hpLeft[4], 100 - fminf(pel[4] * SG_PELLET_DMG * 1.3f * PVP_SCALE, PVP_HIT_CAP));
        fprintf(f, "shotgun PvP a 14 m : %d plombs, PV %.1f (attendu 4 a 6 plombs, PV 40 a 51 selon la rotation : avant, 5 plombs x 5.6 = PV 72 ; maintenant le kill en 2 tirs va a 14 m)\n",
                pel[5], hpLeft[5]);

        {   /* recul de la victime : la MEME poussee que celle du tireur (proportionnelle a la vitesse du tireur), x part de plombs touches */
            float kn[3], ps[3]; int nh[3];
            static const float vsh[3] = { 0, 30, 100 };    /* vitesse du tireur vers la cible a 3 m : a l'arret, 30 m/s, au-dela du plafond */
            for (int pass = 0; pass < 3; pass++) {
                reset_game();
                dmMode = 1;
                memset(NP, 0, sizeof NP);
                NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
                NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible");
                P.pos = P.prevPos = v3(-20, 0, 60); P.grounded = 1; P.vel = v3(0, 0, -vsh[pass]);
                NP[1].disp = v3(-20, 0, 57); vf(NP[1].st.pos, NP[1].disp);
                V3 d = vsub(np_center(1), eye_pos());
                P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
                P.ammo[WP_SHOTGUN] = 6; shotStreak = 0; hitstop = 0;
                int q0 = outqHead;
                fire_shotgun();
                hitstop = 0;
                kn[pass] = -1; nh[pass] = P.sgLastPellets; ps[pass] = sgLastPush;
                for (int k = 0; k < OUTQ; k++) {           /* l'EV_HIT W_SHOTGUN que le tireur vient d'emettre : norme du recul */
                    int idx = (q0 + k) % OUTQ;
                    if (k < (outqHead - q0 + OUTQ) % OUTQ && outq[idx].ev.kind == EV_HIT && outq[idx].ev.sub == W_SHOTGUN)
                        kn[pass] = vlen(fv(outq[idx].ev.f + 1));
                }
            }
            fprintf(f, "shotgun recul de la victime a 3 m, tireur a 0 / 30 / 100 m/s : %d / %d / %d plombs, recul emis %.1f / %.1f / %.1f m/s, poussee du tireur "
                       "%.1f / %.1f / %.1f m/s (attendu 10 plombs, 0 / 30 / %.0f sans plafond, identique a la poussee du tireur)\n",
                    nh[0], nh[1], nh[2], kn[0], kn[1], kn[2], ps[0], ps[1], ps[2], SG_KNOCK_RATIO * 100);
        }                                                  /* (dmMode et NP restent comme apres le test precedent : la suite en depend) */

        memset(TR, 0, sizeof TR);                          /* 13. EV_SHOT recu d'un autre joueur : 10 traceurs recalcules */
        NetEvent e = ev_new(EV_SHOT);
        e.from = 1; e.sub = SG_PELLETS; e.netId = 4242u; e.f[6] = SG_HALF_ANGLE;
        vf(e.f, v3(-20, 1.6f, 30)); vf(e.f + 3, v3(0, 0, 1));
        apply_event(&e);
        int live = 0;
        for (int i = 0; i < MAXTR; i++) live += TR[i].t < TR[i].life;
        fprintf(f, "shotgun EV_SHOT recu : %d traceurs, recul visuel du tireur %.2f (attendu 10, 0.12)\n", live, NP[1].sgFireT);

        reset_game();                                      /* bot au loadout SHOTGUN + rail, cible immobile a 8 m */
        dmMode = 1;
        NP[1].active = NP[1].alive = 1; NP[1].hp = 100; NP[1].disp = v3(-20, 0, 52); vf(NP[1].st.pos, NP[1].disp);
        P.pos = P.prevPos = v3(-20, 0, 60); P.yaw = 0; P.dashCharge = 0;   /* pas de cross-up : on veut le tir direct */
        P.loadout[0] = WP_RAIL; P.loadout[1] = WP_SHOTGUN; P.weapon = WP_RAIL;
        int hs0 = hitCount[W_SHOTGUN], hr0 = hitCount[W_RAIL];
        for (int i = 0; i < 90 && NP[1].alive; i++) { nowT += TICK; bot_think(TICK); game_tick(TICK); hitstop = 0; }
        fprintf(f, "bot shotgun a 8 m : arme %s, touches shotgun %d, rail %d (attendu SHOTGUN, >= 1, 0)\n",
                ammoDef[P.weapon].name, hitCount[W_SHOTGUN] - hs0, hitCount[W_RAIL] - hr0);
    }

    /* --- Scie --- */
    {
        int savedBot = botMode;
        botMode = 0;                                       /* pas de journal du bot dans les resultats */
        float pv[6];
        static const float rels[4] = { 0, 32, 41.5f, 51 };
        for (int k = 0; k < 4; k++) pv[k] = saw_dmg(rels[k]);
        fprintf(f, "scie formule : %.2f/%.2f %.2f/%.2f %.2f/%.2f %.2f/%.2f (attendu 30.00/21.00 60.00/42.00 74.25/51.97 88.50/60.00)\n",
                pv[0], fminf(pv[0] * PVP_SCALE, PVP_HIT_CAP), pv[1], fminf(pv[1] * PVP_SCALE, PVP_HIT_CAP),
                pv[2], fminf(pv[2] * PVP_SCALE, PVP_HIT_CAP), pv[3], fminf(pv[3] * PVP_SCALE, PVP_HIT_CAP));
        pv[4] = saw_dmg(48) * SAW_RETURN_PARRY_DMG_MULT; pv[5] = saw_dmg(57.5f) * SAW_RETURN_PARRY_DMG_MULT;
        fprintf(f, "scie renvoi : %.2f/%.2f a 48 m/s, %.2f/%.2f a 57.5 m/s (attendu 75.60/52.92 88.42/60.00)\n",
                pv[4], fminf(pv[4] * PVP_SCALE, PVP_HIT_CAP), pv[5], fminf(pv[5] * PVP_SCALE, PVP_HIT_CAP));

        /* 2. lancer et plantage dans le mur nord (18,4 m) */
        saw_test_reset(v3(-20, 0, 60), PI, 0);
        fire_saw();
        Proj *sp = saw_mine();
        V3 from = eye_pos();
        for (int i = 0; i < 600 && sp->sawState == SAW_FLY; i++) proj_tick(TICK);
        float dz = sp->pos.z - SAW_EMBED - from.z, drop = from.y - sp->pos.y;
        fprintf(f, "scie plantee : etat %d, normale (%.0f %.0f %.0f), distance %.1f m, chute %.2f m (calcul %.2f), munition %d, cd %.2f "
                   "(attendu %d, (0 0 -1), 18.4, chute ~ calcul, 0, 0.40)\n",
                sp->sawState, sp->sawN.x, sp->sawN.y, sp->sawN.z, dz, drop, 0.5f * SAW_GRAVITY * (dz / SAW_SPEED) * (dz / SAW_SPEED),
                P.ammo[WP_SAW], P.sawCd, SAW_PLANTED);

        /* 6. rappel au grappin : le joueur ne bouge pas */
        for (int i = 0; i < 30; i++) player_tick(TICK, 0, 0);
        {
            V3 p0 = P.pos, d = vsub(sp->pos, eye_pos());
            float dist0 = vlen(vsub(sp->pos, pcenter()));
            P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
            hookPressIn = 1; hookHeldIn = 1;
            int recallTick = -1, caughtTick = -1, hookAfter = -1;
            float vAt = -1, moved = -1, cdAt = 0;
            for (int i = 0; i < 240 && caughtTick < 0; i++) {
                player_tick(TICK, 0, 0); hookPressIn = 0;
                if (recallTick < 0 && saw_mine() && saw_mine()->sawState == SAW_RECALL) {
                    recallTick = i; hookAfter = P.hook; vAt = vlen(P.vel); moved = vlen(vsub(P.pos, p0)); cdAt = P.hookCd;
                }
                proj_tick(TICK);
                if (recallTick >= 0 && !saw_mine()) caughtTick = i;
            }
            hookHeldIn = 0;
            fprintf(f, "scie rappel : crochet %d, vitesse %.2f, deplacement %.3f m, recharge %.2f, rattrapee apres %.2f s (calcul %.2f), "
                       "munition %d (attendu 0, 0.00, 0.000, 0.50, ~calcul, 1)\n",
                    hookAfter, vAt, moved, cdAt, (caughtTick - recallTick) * TICK, (dist0 - SAW_CATCH_R) / SAW_RECALL_SPEED,
                    P.ammo[WP_SAW]);
        }
        /* 6 bis. hitbox du crochet (patch d'equilibrage) : la scie plantee est rappelee si le crochet passe a ~1 m de son centre
         * (SAW_HOOK_R 1,2 ; a 0,6 avant, il s'accrochait au mur a cote) */
        for (int pass = 0; pass < 2; pass++) {
            saw_test_reset(v3(-20, 0, 60), PI, 0);
            Proj *o = saw_test_put(v3(-20, 1.0f, 78.4f + SAW_EMBED), v3(0, 0, 0), SAW_PLANTED, myId);   /* dans la face -z du mur */
            o->sawN = v3(0, 0, -1);
            V3 aimAt = v3(-20 + (pass ? 2.0f : 1.0f), 1.0f, 78.4f), d = vsub(aimAt, eye_pos());   /* a 1 m, puis a 2 m du centre de la scie */
            P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
            hookPressIn = 1; hookHeldIn = 1;
            int recalled = 0, attached = 0;
            for (int i = 0; i < 40; i++) {
                player_tick(TICK, 0, 0); hookPressIn = 0;
                if (o->sawState == SAW_RECALL) { recalled = 1; break; }
                if (P.hook == HOOK_ATTACHED) { attached = 1; break; }
            }
            hookHeldIn = 0;
            fprintf(f, "scie hitbox du crochet a %.0f m du centre : rappelee %d, crochet accroche au mur %d (attendu %s, SAW_HOOK_R = %.1f m ; avant 0.6 : "
                       "0, 1 des 1 m)\n", pass ? 2.0f : 1.0f, recalled, attached, pass ? "0, 1" : "1, 0", SAW_HOOK_R);
        }
        /* 7. la scie plantee d'un autre est ignoree : le crochet s'accroche au mur derriere */
        saw_test_reset(v3(-20, 0, 60), PI, 0);
        {
            Proj *o = saw_test_put(v3(-20, 1.0f, 78.4f + SAW_EMBED), v3(0, 0, 0), SAW_PLANTED, 1);   /* dans la face -z du mur */
            o->sawN = v3(0, 0, -1);
            V3 d = vsub(o->pos, eye_pos());
            P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
            hookPressIn = 1; hookHeldIn = 1;
            for (int i = 0; i < 30 && P.hook != HOOK_ATTACHED; i++) { player_tick(TICK, 0, 0); hookPressIn = 0; }
            int hk = P.hook;
            hookHeldIn = 0;
            player_tick(TICK, 0, 0);
            fprintf(f, "scie d'un autre au grappin : crochet %d, scie toujours plantee %d (attendu %d, 1)\n", hk,
                    o->alive && o->sawState == SAW_PLANTED, HOOK_ATTACHED);
        }

        /* 3. touche sur un drone immobile a 10 m, retour, rattrapage (+0,5 dash) ; 4. lancer en courant a 10 m/s */
        for (int pass = 0; pass < 2; pass++) {
            saw_test_reset(v3(-20, 0, 40), 0, 0);
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.7f, 30);
            P.dashCharge = 1;
            if (pass) P.vel = v3(0, 0, -10);
            fire_saw();
            float speed0 = vlen(saw_mine()->vel), hpHit = -1;
            int retTick = -1, catchTick = -1;
            for (int i = 0; i < 240 && catchTick < 0; i++) {
                proj_tick(TICK);
                Proj *q = saw_mine();
                if (retTick < 0 && q && q->sawState == SAW_RETURN) { retTick = i; hpHit = E[0].hp; }
                if (!q) catchTick = i;
            }
            if (!pass)
                fprintf(f, "scie touche un drone a 10 m : PV %.1f, retour=%d, rattrapee=%d, munition %d, sawIdx %d, dash %.1f "
                           "(attendu ~39.9, 1, 1, 1, 0, 1.5)\n",
                        hpHit, retTick >= 0, catchTick >= 0, P.ammo[WP_SAW], P.sawIdx, P.dashCharge);
            else
                fprintf(f, "scie lancee a 10 m/s : vitesse %.1f, degats %.1f (attendu 42.0, ~75)\n", speed0, 100 - hpHit);
        }

        /* 5. ramassage a pied : 1,4 m -> rattrapee, 1,6 m -> toujours plantee */
        {
            int got[2];
            for (int k = 0; k < 2; k++) {
                float r = k ? 1.6f : 1.4f, dy = 0.9f + SAW_EMBED;
                saw_test_reset(v3(-20, 0, 35 + sqrtf(r * r - dy * dy)), 0, 0);
                saw_test_put(v3(-20, -SAW_EMBED, 35), v3(0, 0, 0), SAW_PLANTED, myId);
                proj_tick(TICK);
                got[k] = P.ammo[WP_SAW] == 1 && !saw_mine();
            }
            fprintf(f, "scie ramassee a pied : a 1.4 m %d, a 1.6 m %d (attendu 1, 0)\n", got[0], got[1]);
        }

        /* 9. renvoi avec aide a la visee : drone a 18 m, ma scie revient a 3 m */
        {
            float hp0 = 0, dash0 = 0, hpAfter = -1;
            int stAfter = -1, bnc = 0, stEnd = -1;
            float spd = 0;
            saw_test_reset(v3(-20, 0, 40), 0, 0);
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.7f, 22);
            { V3 d = vsub(E[0].pos, eye_pos()); P.pitch = asinf(d.y / vlen(d)); }
            P.hp = 50; P.dashCharge = 1; hp0 = P.hp; dash0 = P.dashCharge;
            Proj *q = saw_test_put(v3(-20, 1.6f, 37), v3(0, 0, 36), SAW_RETURN, myId);
            melee();
            stAfter = q->sawState; spd = vlen(q->vel); bnc = q->sawBounces;
            for (int i = 0; i < 60 && q->alive && q->sawState == SAW_FLY; i++) proj_tick(TICK);
            hpAfter = E[0].hp; stEnd = q->sawState;
            fprintf(f, "scie renvoi : etat %d, vitesse %.1f, renvois %d, PV joueur %+.0f, dash %+.1f, PV drone %.1f, etat %d "
                       "(attendu %d, 48.0, 1, +0, +0.0, ~24.4, %d)\n",
                    stAfter, spd, bnc, P.hp - hp0, P.dashCharge - dash0, hpAfter, stEnd, SAW_FLY, SAW_RETURN);
        }
        /* 10. cone de l'aide : drone a 5 degres (aide), puis a 10 degres (tout droit dans le viseur) */
        {
            float res[2];
            for (int k = 0; k < 2; k++) {
                float ang = (k ? 10.0f : 5.0f) * PI / 180;
                saw_test_reset(v3(-20, 0, 40), 0, 0);
                E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20 + 18 * tanf(ang), 1.6f, 22);
                Proj *q = saw_test_put(v3(-20, 1.6f, 37), v3(0, 0, 36), SAW_RETURN, myId);
                melee();
                V3 v = vnorm(q->vel), td = vsub(E[0].pos, q->pos);
                if (!k) res[k] = fabsf(atan2f(v.x, -v.z) - atan2f(td.x, -td.z)) * 180 / PI;   /* ecart horizontal avec le drone */
                else res[k] = acosf(clampf(vdot(v, view_dir()), -1, 1)) * 180 / PI;            /* ecart avec le viseur */
            }
            fprintf(f, "scie aide au renvoi : drone a 5 deg -> ecart %.2f deg avec le drone ; a 10 deg -> ecart %.2f deg avec le viseur "
                       "(attendu < 0.3, 0.00)\n", res[0], res[1]);
        }
        /* 11. garde-fous : 3 renvois deja faits, scie plantee, scie rappelee */
        {
            saw_test_reset(v3(-20, 0, 40), 0, 0);
            Proj *q = saw_test_put(v3(-20, 1.6f, 37), v3(0, 0, 36), SAW_RETURN, myId);
            q->sawBounces = SAW_BOUNCE_CHAIN;
            melee();
            int st3 = q->sawState;
            for (int i = 0; i < 60 && saw_mine(); i++) proj_tick(TICK);
            int caught = !saw_mine() && P.ammo[WP_SAW] == 1;
            saw_test_reset(v3(-20, 0, 40), 0, 0);
            q = saw_test_put(v3(-20, 1.6f, 38), v3(0, 0, 0), SAW_PLANTED, myId);
            melee();
            int stP = q->sawState;
            saw_test_reset(v3(-20, 0, 40), 0, 0);
            q = saw_test_put(v3(-20, 1.6f, 37), v3(0, 0, 45), SAW_RECALL, myId);
            melee();
            fprintf(f, "scie garde-fous : apres 3 renvois etat %d puis rattrapee %d ; plantee %d ; rappelee -> etat %d "
                       "(attendu %d 1, %d, %d)\n", st3, caught, stP, q->sawState, SAW_RETURN, SAW_PLANTED, SAW_FLY);
        }
        /* 12. piece frolee : relancee vers le haut, +2 s de vie, la scie ne devie pas */
        {
            saw_test_reset(v3(-20, 0, 40), 0, 0);
            CO[0].alive = 1; CO[0].owner = myId; CO[0].life = 3; CO[0].pos = v3(-19.5f, 1.55f, 35); CO[0].vel = v3(0, 0, 0);
            fire_saw();
            Proj *q = saw_mine();
            V3 v0 = q->vel;
            for (int i = 0; i < 30 && !(q->sawCoins & 1u); i++) proj_tick(TICK);
            fprintf(f, "scie et piece : vy piece %.1f, vie %.1f, scie etat %d, vx %.2f (attendu 11.0, 5.0, %d, %.2f)\n",
                    CO[0].vel.y, CO[0].life, q->sawState, q->vel.x, SAW_FLY, v0.x);
        }
        /* 13. perdue sous le plan de mort, rendue apres 3 s */
        {
            saw_test_reset(v3(-20, 0, 40), 0, 0);
            Proj *q = saw_test_put(v3(-20, -25, 30), v3(0, 0, -10), SAW_FLY, myId);
            proj_tick(TICK);
            int stL = q->sawState;
            int t2 = -1;
            for (int i = 0; i < 400 && t2 < 0; i++) { proj_tick(TICK); if (!saw_mine()) t2 = i + 1; }
            fprintf(f, "scie perdue : etat %d, rendue apres %.2f s, munition %d (attendu %d, 3.00, 1)\n", stL, t2 * TICK,
                    P.ammo[WP_SAW], SAW_LOST);
        }
        /* clic avec la scie plantee : clic sec, message, bascule sur l'autre arme */
        {
            saw_test_reset(v3(-20, 0, 40), 0, 0);
            saw_test_put(v3(-20, -SAW_EMBED, 22), v3(0, 0, 0), SAW_PLANTED, myId);
            P.ammo[WP_RAIL] = 2; P.fireCd = 0;
            botPress[A_FIRE] = 1;
            game_tick(TICK);
            fprintf(f, "scie plantee, clic : arme %s, message \"%s\" (attendu RAILGUN, SCIE PLANTEE (18 m))\n", ammoDef[P.weapon].name, popText);
        }

        /* LAN simule (hote) */
        reset_game();
        dmMode = 1;
        memset(NP, 0, sizeof NP);
        NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
        NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Lanceur");
        NP[1].disp = v3(-20, 0, 24); vf(NP[1].st.pos, NP[1].disp);
        matchOver = 0;
        {   /* 1 bis. combo lancer + renvoi, arbitre par l'hote */
            net_hit_saw(1, saw_dmg(32) * PVP_SCALE, v3(0, 0, 0), W_SAW, 0);
            net_hit_saw(1, saw_dmg(48) * SAW_RETURN_PARRY_DMG_MULT * PVP_SCALE, v3(0, 0, 0), W_SAW, 1);
            float hpStatic = NP[1].hp; int aliveStatic = NP[1].alive;
            NP[1].hp = 100;
            net_hit_saw(1, saw_dmg(32) * PVP_SCALE, v3(0, 0, 0), W_SAW, 0);
            net_hit_saw(1, saw_dmg(57.5f) * SAW_RETURN_PARRY_DMG_MULT * PVP_SCALE, v3(0, 0, 0), W_SAW, 1);
            fprintf(f, "scie combo PvP : lancer + renvoi a l'arret -> PV %.1f vivant %d ; renvoi en course -> vivant %d (attendu 5.1 1, 0)\n",
                    hpStatic, aliveStatic, NP[1].alive);
            NP[1].alive = 1; NP[1].hp = 100;
        }
        {   /* 8. parry d'une scie adverse : elle repart vers son lanceur a 40 m/s ; celui qui a pare juge la touche */
            P.pos = P.prevPos = v3(-20, 0, 41); P.yaw = 0; P.pitch = 0; hitstop = 0;
            Proj *q = saw_test_put(v3(-20, 1.6f, 39), v3(0, 0, SAW_SPEED), SAW_FLY, 1);
            melee();
            V3 to = vnorm(vsub(np_center(1), q->pos));
            int st8 = q->sawState, by = q->sawBy;
            float sp8 = vlen(q->vel), al = vdot(vnorm(q->vel), to);
            for (int i = 0; i < 120 && q->alive; i++) proj_tick(TICK);
            fprintf(f, "scie paree par moi : etat %d, par %d, vitesse %.1f, alignement vers le lanceur %.3f, PV du lanceur %.1f "
                       "(attendu %d, 0, 40.0, 1.000, ~49.5)\n", st8, by, sp8, al, NP[1].hp, SAW_PARRIED);
            NP[1].hp = 100;
        }
        {   /* reception : scie plantee d'un autre, puis rattrapee */
            NetEvent e = ev_new(EV_SAW);
            e.from = 1; e.sub = SAWE_PLANT; e.netId = (2u << 24) | ((unsigned int)PR_SAW << 20) | 9;
            vf(e.f, v3(-10, 1, 78.3f)); vf(e.f + 3, v3(1, 0, 0)); e.f[6] = 4;   /* face -z */
            apply_event(&e);
            Proj *q = find_proj(e.netId);
            int okP = q && q->kind == PR_SAW && q->owner == 1 && q->sawState == SAW_PLANTED && q->sawN.z < -0.5f;
            e.sub = SAWE_CATCH; e.eid++;
            apply_event(&e);
            fprintf(f, "scie reseau : copie plantee %d, retiree au rattrapage %d (attendu 1, 1)\n", okP, find_proj(e.netId) == NULL);
        }
        {   /* 15. netId distinct de la roquette, type PR_SAW dans les bits 20-23 */
            P.loadout[0] = WP_SAW; P.loadout[1] = WP_ROCKET; P.ammo[WP_SAW] = 1; P.ammo[WP_ROCKET] = 4; P.sawCd = 0; P.sawIdx = 0;
            P.pos = P.prevPos = v3(-20, 0, 41); P.yaw = PI; P.pitch = 0;
            fire_rocket();
            unsigned int rid = 0;
            for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].kind == PR_ROCKET) rid = PR[i].netId;
            fire_saw();
            unsigned int sid = saw_mine() ? saw_mine()->netId : 0;
            /* 14. mort du lanceur (EV_KILL sur moi) : la scie disparait */
            NetEvent e = ev_new(EV_KILL);
            e.actor = 1; e.target = 0; e.sub = W_SAW;
            apply_event(&e);
            int sawsLeft = 0;
            for (int i = 0; i < MAXPR; i++) sawsLeft += PR[i].alive && PR[i].kind == PR_SAW && PR[i].owner == myId;
            fprintf(f, "scie netId : roquette %08X, scie %08X, type %u ; apres ma mort : scies %d, sawIdx %d (attendu differents, %d ; 0, 0)\n",
                    rid, sid, (sid >> 20) & 0xF, sawsLeft, P.sawIdx, PR_SAW);
        }
        memset(NP, 0, sizeof NP);
        dmMode = 0;
        botMode = savedBot;
    }

    /* --- Tesla --- (couloir x = -20, regard vers -z ; drones a hauteur des yeux, immobiles) */
    {
        int savedBot = botMode;
        botMode = 0;
        const float EY = EYE_STAND;
        /* 1. portee selon la vitesse */
        fprintf(f, "tesla portee a 0 / 20 / 60 m/s : %.1f / %.1f / %.1f (attendu 14.0 / 20.0 / 28.0)\n",
                tesla_range(v3(0, 0, 0)), tesla_range(v3(0, 0, 20)), tesla_range(v3(60, 0, 0)));

        /* 2. batterie 60, tir tenu 1 s dans le vide */
        tesla_test_reset(v3(-20, 0, 40), PI);
        int p0 = teslaPulses;
        tesla_test_run(120, 1);
        fprintf(f, "tesla tir tenu 1 s dans le vide : batterie %d, %d impulsions (attendu 40, 10)\n", P.ammo[WP_TESLA], teslaPulses - p0);

        /* 3. tir tenu 4 s avec 60, puis 5 s sans tirer : aucune recharge passive */
        tesla_test_reset(v3(-20, 0, 40), PI);
        tesla_test_run(480, 1);
        int b4 = P.ammo[WP_TESLA];
        const char *w4 = ammoDef[P.weapon].name;
        tesla_test_run(600, 0);
        fprintf(f, "tesla tenu 4 s : batterie %d, arme %s ; 5 s plus tard : batterie %d, jauge %.0f (attendu 0, RAILGUN ; 0, 0)\n",
                b4, w4, P.ammo[WP_TESLA], P.ammoGauge[WP_TESLA]);

        /* 4. drone de 100 PV a 10 m, 1 s : 10 impulsions x 7, au plus 5 chiffres de degats */
        tesla_test_reset(v3(-20, 0, 40), 0);
        Enemy *d0 = tesla_test_drone(0, v3(-20, EY, 30), 100);
        tesla_test_run(120, 1);
        tesla_test_run(1, 0);                              /* relache : les cumuls partent */
        int dn = 0;
        for (int i = 0; i < MAXDN; i++) dn += DN[i].t > 0;
        fprintf(f, "tesla drone a 10 m, 1 s : PV %.1f, %d chiffres de degats (attendu 30.0, <= 5)\n", d0->hp, dn);

        /* 5. meme drone, tir jusqu'au kill : envoi anticipe, sans attendre la fenetre */
        tesla_test_reset(v3(-20, 0, 40), 0);
        d0 = tesla_test_drone(0, v3(-20, EY, 30), 100);
        p0 = teslaPulses;
        int killTick = -1;
        for (int i = 0; i < 300 && killTick < 0; i++) { tesla_test_run(1, 1); if (!d0->alive) killTick = i + 1; }
        fprintf(f, "tesla jusqu'au kill : tue a l'impulsion %d, apres %.3f s (attendu 15, 1.400 +- 1 tick)\n", teslaPulses - p0,
                killTick * TICK);

        /* 6. drone a 16 m : hors de portee a l'arret, touche a 10 m/s (portee 17) */
        float hp6[2];
        for (int k = 0; k < 2; k++) {
            tesla_test_reset(v3(-20, 0, 40), 0);
            d0 = tesla_test_drone(0, v3(-20, EY, 24), 100);
            P.vel = k ? v3(0, 0, -10) : v3(0, 0, 0);
            tesla_test_run(120, 1); tesla_test_run(1, 0);
            hp6[k] = 100 - d0->hp;
        }
        fprintf(f, "tesla drone a 16 m : degats %.1f a l'arret, %.1f a 10 m/s (attendu 0.0, 70.0)\n", hp6[0], hp6[1]);

        /* 7. chaine : drones a 10, 15, 20 et 27 m dans l'axe ; 8. une boite entre le 2e et le 3e */
        float ch[2][4];
        for (int k = 0; k < 2; k++) {
            static const float dz[4] = { 10, 15, 20, 27 };
            tesla_test_reset(v3(-20, 0, 40), 0);
            for (int j = 0; j < 4; j++) tesla_test_drone(j, v3(-20, EY, 40 - dz[j]), 1000);
            if (k) {
                boxes[nb].mn = v3(-21, 0, 22); boxes[nb].mx = v3(-19, 3, 23); boxes[nb].kind = B_SOLID; nb++;
            }
            tesla_test_run(120, 1); tesla_test_run(1, 0);
            if (k) nb--;
            for (int j = 0; j < 4; j++) ch[k][j] = 1000 - E[j].hp;
        }
        fprintf(f, "tesla chaine : %.1f / %.1f / %.1f / %.1f (attendu 70.0 / 42.0 / 42.0 / 0.0)\n", ch[0][0], ch[0][1], ch[0][2], ch[0][3]);
        fprintf(f, "tesla chaine coupee par un mur : %.1f / %.1f / %.1f (attendu 70.0 / 42.0 / 0.0)\n", ch[1][0], ch[1][1], ch[1][2]);

        /* 9. relais : piece a 8 m dans l'axe, drone a 4 m de la piece hors de l'axe ; 1 s puis 2,1 s */
        tesla_test_reset(v3(-20, 0, 40), 0);
        d0 = tesla_test_drone(0, v3(-16, EY, 32), 100);
        CO[0].alive = 1; CO[0].owner = myId; CO[0].life = 4; CO[0].pos = v3(-20, EY, 32); CO[0].vel = v3(0, 0, 0);
        tesla_test_run(1, 1);
        float y9 = CO[0].pos.y;
        tesla_test_run(119, 1);
        tesla_flush();
        float t9 = CO[0].teslaT, dy9 = CO[0].pos.y - y9, hp9 = d0->hp;
        tesla_test_run(132, 0);
        fprintf(f, "tesla relais : actif %d (%.2f s restantes), ecart de hauteur %.3f, PV drone %.1f ; a 2,1 s piece vivante %d "
                   "(attendu 1, ~1.0, 0.000, 50.0 ; 0)\n", t9 > 0, t9, dy9, hp9, CO[0].alive);

        /* 10. pas de cumul : drone dans l'arc ET a 3 m d'un relais, 1 s */
        tesla_test_reset(v3(-20, 0, 40), 0);
        d0 = tesla_test_drone(0, v3(-20, EY, 31), 100);
        CO[0].alive = 1; CO[0].owner = myId; CO[0].life = 4; CO[0].pos = v3(-20, EY, 34); CO[0].vel = v3(0, 0, 0);
        tesla_test_run(120, 1); tesla_test_run(1, 0);
        fprintf(f, "tesla arc + relais sur la meme cible, 1 s : degats %.1f, relais actif %d (attendu 70.0 et pas 120, 1)\n",
                100 - d0->hp, CO[0].teslaT > 0);

        /* 11. plafond : 4 pieces traversees -> 3 relais */
        tesla_test_reset(v3(-20, 0, 40), 0);
        for (int j = 0; j < 4; j++) { CO[j].alive = 1; CO[j].owner = myId; CO[j].life = 4; CO[j].pos = v3(-20, EY, 36 - 2.0f * j); }
        tesla_test_run(1, 1);
        fprintf(f, "tesla 4 pieces traversees : %d relais actifs (attendu 3)\n", tesla_relays_of(myId));

        /* 12. la chaine de touches n'est ni augmentee ni remise a zero */
        tesla_test_reset(v3(-20, 0, 40), 0);
        tesla_test_drone(0, v3(-20, EY, 30), 1000);
        shotStreak = 3;
        tesla_test_run(120, 1); tesla_test_run(1, 0);
        int st12 = shotStreak;
        tesla_test_reset(v3(-20, 0, 40), PI);
        shotStreak = 3;
        tesla_test_run(120, 1); tesla_test_run(1, 0);
        fprintf(f, "tesla chaine de tir : %d apres 1 s sur un drone, %d apres 1 s dans le vide (attendu 3, 3)\n", st12, shotStreak);

        /* 17. rendu : arc_rand ne touche pas le rng global */
        unsigned int r0 = rng;
        for (int k = 0; k < 50; k++) arc_rand();
        fprintf(f, "tesla rendu : rng global intact %d (attendu 1)\n", rng == r0);

        /* 13. LAN simule : cible a 10 m, 1 s -> 5 net_hit de 9,8 */
        tesla_test_reset(v3(-20, 0, 40), 0);
        dmMode = 1; matchOver = 0;
        memset(NP, 0, sizeof NP);
        NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
        NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible");
        NP[1].disp = v3(-20, 0, 30); vf(NP[1].st.pos, NP[1].disp);
        int h13 = hitCount[W_TESLA];
        tesla_test_run(60, 1);
        NetPState ps;
        pack_state(&ps);
        tesla_test_run(60, 1); tesla_test_run(1, 0);
        fprintf(f, "tesla LAN a 10 m, 1 s : %d EV_HIT, PV %.1f, ST_ZAP envoye %d (attendu 5, 51.0, 1)\n", hitCount[W_TESLA] - h13,
                NP[1].hp, (ps.pad & ST_ZAP) != 0);

        /* 14. parry : la victime pare, l'hote renvoie la decharge ; le tireur (moi) est etourdi */
        NP[1].hp = 100; NP[0].hp = 100; NP[1].lastParryT = nowT; NP[0].teslaReflT = 0;
        int refl0 = teslaReflects;
        net_hit(1, 9.8f, v3(0, 0, 0), W_TESLA);
        float stun14 = P.teslaStunT, hpV = NP[1].hp, hpS = NP[0].hp;
        net_hit(1, 9.8f, v3(0, 0, 0), W_TESLA);            /* deja en vol : jete */
        hitstop = 0;
        p0 = teslaPulses;
        tesla_test_run(48, 1);
        int during = teslaPulses - p0;
        tesla_test_run(1, 1);
        fprintf(f, "tesla parry : PV victime %.0f, PV tireur %.0f, etourdi %.2f s, renvois %d, 2e touche : PV %.0f / %.0f, "
                   "impulsions pendant 48 ticks %d, au 49e %d (attendu 100, 85, 0.40, 1, 100 / 85, 0, 1)\n",
                hpV, hpS, stun14, teslaReflects - refl0, NP[1].hp, NP[0].hp, during, teslaPulses - p0 - during);

        /* EV_TESLA sous-type 0 recu : la piece d'un autre devient son relais */
        {
            CO[5].alive = 1; CO[5].owner = 1; CO[5].netId = 0x02000042; CO[5].pos = v3(0, 5, 0); CO[5].vel = v3(0, -3, 0); CO[5].teslaT = 0;
            NetEvent e = ev_new(EV_TESLA);
            e.from = 1; e.sub = 0; e.netId = 0x02000042; vf(e.f, v3(1, 6, 1));
            apply_event(&e);
            fprintf(f, "tesla relais recu : actif %d, par %d, vitesse %.1f (attendu 1, 1, 0.0)\n", CO[5].teslaT > 0, CO[5].teslaBy,
                    vlen(CO[5].vel));
            CO[5].alive = 0;
        }

        /* 16. recalcul chez les autres : NP[1] rejoue mon tir (etat de pack_state) -> memes cibles */
        tesla_test_reset(v3(-20, 0, 40), 0);
        dmMode = 1;
        memset(NP, 0, sizeof NP);
        NP[0].active = NP[0].alive = 1; NP[0].hp = 100;
        for (int j = 2; j < 4; j++) { NP[j].active = NP[j].alive = 1; NP[j].hp = 100; }
        NP[2].disp = v3(-20.3f, 0, 29); NP[3].disp = v3(-16, 0, 27);
        P.pitch = -0.03f; P.vel = v3(0, 0, -5);
        TeslaTgt ta[3], tb[3];
        int na = tesla_trace(eye_pos(), view_dir(), tesla_range(P.vel), myId, ta, NULL, NULL);
        pack_state(&NP[1].st);
        NP[1].disp = P.pos; NP[1].active = NP[1].alive = 1;
        P.dead = 1;                                        /* moi hors du jeu : je ne masque plus la copie */
        int nb2 = tesla_trace(np_eye(1), yp_dir(NP[1].st.yaw, NP[1].st.pitch), tesla_range(np_vel(1)), 1, tb, NULL, NULL);
        int same = na == nb2;
        for (int k = 0; same && k < na; k++) same = ta[k].kind == tb[k].kind && ta[k].idx == tb[k].idx;
        fprintf(f, "tesla recalcul distant : %d cible(s) chez moi, %d chez l'autre, identiques %d, premiere %d (attendu 2, 2, 1, 2)\n",
                na, nb2, same, na ? ta[0].idx : -1);

        /* 15. points avec la Tesla en main (1 point = 0,4 de batterie) */
        tesla_test_reset(v3(-20, 0, 40), 0);
        P.ammo[WP_TESLA] = 0; P.ammo[WP_RAIL] = 0;
        int b15[6];
        ammo_points(50, AP_HIT); b15[0] = P.ammo[WP_TESLA];
        ammo_points(PTS_MELEE_KILL, AP_KILL); b15[1] = P.ammo[WP_TESLA];
        ammo_points(PTS_PARRY, AP_PARRY); b15[2] = P.ammo[WP_TESLA];
        {
            int pts0 = PU[0].pts;
            PU[0].pts = PICKUP_BIG; pickup_taken(0, myId); PU[0].t = 0; PU[0].pts = pts0;
        }
        b15[3] = P.ammo[WP_TESLA];
        P.ammo[WP_TESLA] = 90; P.ammoGauge[WP_RAIL] = 0;
        ammo_points(100, AP_HIT); b15[4] = P.ammo[WP_TESLA];
        fprintf(f, "tesla points : epee %d, + kill %d, + parry %d, + grande cellule %d ; a 90 + 100 points %d, jauge du rail %.0f "
                   "(attendu 20, 60, 80, 100 ; 100, 75)\n", b15[0], b15[1], b15[2], b15[3], b15[4], P.ammoGauge[WP_RAIL]);

        /* bot au loadout Tesla + rail, cible immobile a 12 m : il tient l'arc */
        botMode = 2;
        tesla_test_reset(v3(-20, 0, 40), 0);
        dmMode = 1;
        memset(NP, 0, sizeof NP);
        NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Bot");
        NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible");
        NP[1].disp = v3(-20, 0, 28); vf(NP[1].st.pos, NP[1].disp);
        P.dashCharge = 0; P.skillCh = 0;
        P.loadout[0] = WP_RAIL; P.loadout[1] = WP_TESLA; P.weapon = WP_RAIL;
        FILE *savedLog = botLog;
        botLog = NULL;                                     /* journal du bot hors des resultats */
        int ht0 = hitCount[W_TESLA], hr0 = hitCount[W_RAIL];
        for (int i = 0; i < 120 && NP[1].alive; i++) { nowT += TICK; bot_think(TICK); game_tick(TICK); hitstop = 0; }
        botLog = savedLog;
        fprintf(f, "bot tesla a 12 m : arme %s, touches tesla %d, rail %d (attendu TESLA, >= 2, 0)\n", ammoDef[P.weapon].name,
                hitCount[W_TESLA] - ht0, hitCount[W_RAIL] - hr0);

        memset(NP, 0, sizeof NP);
        memset(CO, 0, sizeof CO);
        dmMode = 0;
        botMode = savedBot;
    }
    /* --- fin Tesla --- */
    /* --- Lance (solo) : couloir degage x = -20, lance_press / lance_tick appeles directement --- */
    dmMode = 0;
    {   /* 1. joute contre un drone immobile a 20 m/s */
        lance_test_reset(v3(-20, 0, 40));
        P.vel = v3(0, 0, -20); P.dashCharge = 0.2f;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.6f, 30); E[0].vel = v3(0, 0, 0);
        float reachHit = -1;
        lance_press();
        for (int i = 0; i < 300 && P.lanceState != LS_IDLE; i++) {
            float reach = vlen(vsub(E[0].pos, eye_pos())) - enemy_radius(EN_DRONE);
            lance_tick(TICK, 1);
            if (!E[0].alive && reachHit < 0) reachHit = reach;
            P.pos = P.prevPos = vadd(P.pos, vmul(P.vel, TICK));
        }
        hitstop = 0;
        fprintf(f, "lance joute drone a 20 m/s : portee a la touche %.2f, drone vivant %d, vitesse apres %.1f, recharge %.2f, dash %.1f "
                   "(attendu <= 3.50, 0, 10.0, 1.00, 1.2)\n", reachHit, E[0].alive, vlen(P.vel), P.meleeCd, P.dashCharge);
    }
    {   /* 2. v_c insuffisante (8 m/s) : aucune touche en 2 s, puis relevee avec 0,5 s de recharge */
        lance_test_reset(v3(-20, 0, 40));
        P.vel = v3(0, 0, -8);
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.6f, 30); E[0].vel = v3(0, 0, 0);
        lance_press();
        int ticks = 0;
        for (; ticks < 300 && P.lanceState != LS_IDLE; ticks++) {
            lance_tick(TICK, 1);
            P.pos = P.prevPos = vadd(P.pos, vmul(P.vel, TICK));
        }
        fprintf(f, "lance a 8 m/s : PV drone %.0f, relevee apres %.2f s, etat %d, recharge %.2f (attendu 100, 2.00, 0, 0.50)\n",
                E[0].hp, ticks * TICK, P.lanceState, P.meleeCd);
    }
    {   /* 3. cone : drone decale de 2 m a 3 m devant (33,7 degres), v_c et portee suffisantes */
        lance_test_reset(v3(-20, 0, 40));
        P.vel = v3(0, 0, -20); P.lanceState = LS_COUCH; P.lanceT = 0.5f;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-18, 1.6f, 37); E[0].vel = v3(0, 0, 0);
        lance_tick(TICK, 1);
        fprintf(f, "lance hors du cone (33.7 deg) : PV drone %.0f, etat %d (attendu 100, 2)\n", E[0].hp, P.lanceState);
    }
    {   /* 4. tap = estoc : drone a 2,5 m, touche relachee au tick suivant */
        lance_test_reset(v3(-20, 0, 40));
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.6f, 37.5f); E[0].vel = v3(0, 0, 0);
        lance_press();
        float pw = P.parryWin;
        lance_tick(TICK, 0);
        hitstop = 0;
        fprintf(f, "lance estoc (tap) : PV drone %.0f, fenetre de parry %.2f, etat %d, recharge %.2f (attendu 70, 0.12, 0, 0.50)\n",
                E[0].hp, pw, P.lanceState, P.meleeCd);
    }
    {   /* 5. kamikaze a 10,5 m/s contre une lance couchee a l'arret */
        lance_test_reset(v3(-20, 0, 40));
        E[0].alive = 1; E[0].type = EN_RUSHER; E[0].hp = 50; E[0].pos = v3(-20, 1.6f, 32); E[0].vel = v3(0, 0, 10.5f);
        lance_press();
        int jo = lanceJousts;
        for (int i = 0; i < 240 && E[0].alive; i++) {
            lance_tick(TICK, 1);
            if (E[0].alive) E[0].pos = vadd(E[0].pos, vmul(E[0].vel, TICK));
        }
        hitstop = 0;
        fprintf(f, "lance contre kamikaze : kamikaze vivant %d, joute %d, PV joueur %.0f (attendu 0, 1, 100)\n",
                E[0].alive, lanceJousts - jo, P.hp);
    }
    memset(E, 0, sizeof E);

    /* --- Lance en LAN simule (je suis l'hote) --- */
    {   /* 6. joute a v_c 15 puis 20 : x 0,7, plafond 60 */
        float hpL[2];
        for (int s = 0; s < 2; s++) {
            lance_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 37));
            V3 dir = vnorm(vsub(np_center(1), eye_pos()));
            P.vel = vmul(dir, s ? 20.0f : 15.0f); P.lanceState = LS_COUCH; P.lanceT = 0.5f;
            lance_tick(TICK, 1);
            hpL[s] = NP[1].hp; hitstop = 0;
        }
        fprintf(f, "lance PvP a v_c 15 / 20 : PV cible %.1f / %.1f (attendu 40.5 / 40.0)\n", hpL[0], hpL[1]);
    }
    {   /* 7. la victime pare la lance couchee ; puis je pare une lance couchee : pas de recul */
        lance_test_reset(v3(-20, 0, 40));
        lance_test_lan(v3(-20, 0, 37));
        NP[1].lastParryT = nowT;
        int mp = meleeParries;
        V3 dir = vnorm(vsub(np_center(1), eye_pos()));
        P.vel = vmul(dir, 15); P.lanceState = LS_COUCH; P.lanceT = 0.5f;
        lance_tick(TICK, 1);
        hitstop = 0;
        float hpP = NP[1].hp, cdP = P.meleeCd;
        int stP = P.lanceState;
        mp = meleeParries - mp;
        lance_test_reset(v3(-20, 0, 40));
        lance_test_lan(v3(-20, 0, 37));
        P.parryWin = 0.1f;
        host_tick(TICK);
        NetEvent e = ev_new(EV_HIT);
        e.eid = 0x0277AA01u; e.from = 1; e.target = 0; e.sub = W_LANCE; e.f[0] = 59.5f; vf(&e.f[1], v3(0, 3, 20));
        e.f[4] = 15; e.f[5] = 15; e.f[6] = 1;
        P.vel = v3(0, 0, 0);
        host_event_in(&e);
        hitstop = 0;
        fprintf(f, "lance paree : PV cible %.0f, parries +%d, recharge attaquant %.2f, etat %d ; je pare : PV %.0f, recul %.1f m/s "
                   "(attendu 100, +1, 1.50, 0 ; 100, 0.0)\n", hpP, mp, cdP, stP, NP[0].hp, vlen(P.vel));
    }
    {   /* 8. choc de joute : NP[1] lance couchee vers moi, vitesse propre 15 / 19 / 25 contre mes 20 m/s */
        static const float sb[3] = { 15, 19, 25 };
        float hpOther[3], hpMe[3];
        int cl[3];
        lance_test_reset(v3(-20, 0, 41));
        lance_test_lan(v3(-20, 0, 38));
        for (int s = 0; s < 3; s++) {
            nowT += 1;
            NP[0].hp = 100; NP[1].hp = 100; NP[1].alive = 1; NP[0].alive = 1;
            NP[0].disp = P.pos; vf(NP[0].st.vel, v3(0, 0, -20));
            NP[1].st.yaw = PI; NP[1].st.pitch = 0; vf(NP[1].st.vel, v3(0, 0, sb[s]));
            NP[1].lastLanceT = nowT;
            int c0 = clashes;
            NetEvent e = ev_new(EV_HIT);
            e.target = 1; e.sub = W_LANCE; e.f[0] = 60; vf(&e.f[1], v3(0, 3, -12));
            e.f[4] = 20; e.f[5] = s == 2 ? 15.0f : 20.0f; e.f[6] = 1;
            net_emit(&e);
            hitstop = 0;
            hpOther[s] = NP[1].hp; hpMe[s] = NP[0].hp; cl[s] = clashes - c0;
        }
        NetEvent r = ev_new(EV_HIT);                      /* touche reciproque de NP[1] juste apres sa victoire */
        r.eid = 0x0277AA02u; r.from = 1; r.target = 0; r.sub = W_LANCE; r.f[0] = 60; r.f[4] = 25; r.f[5] = 15; r.f[6] = 1;
        host_event_in(&r);
        hitstop = 0;
        fprintf(f, "choc de joute 20 contre 15 : PV %.0f / %.0f ; 20 contre 19 : PV %.0f / %.0f, chocs +%d ; 20 contre 25 : PV %.0f / %.1f ; "
                   "reciproque ignoree : mes PV %.1f (attendu 40 / 100 ; 100 / 100, +1 ; 100 / 40.5 ; 40.5)\n",
                hpOther[0], hpMe[0], hpOther[1], hpMe[1], cl[1], hpOther[2], hpMe[2], NP[0].hp);
        (void)cl[0];
    }
    {   /* 9. grappin + lance : la joute part avant l'impact et consomme le crochet ; sans viser : impact du grappin */
        int dL[2], dH[2], hk[2];
        float cd[2];
        for (int s = 0; s < 2; s++) {
            lance_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 25));
            V3 d = vsub(np_center(1), eye_pos());
            P.pitch = asinf(d.y / vlen(d));
            P.lanceState = LS_COUCH; P.lanceT = 0.2f;
            int l0 = hitCount[W_LANCE], h0 = hitCount[W_HOOK], attached = 0;
            hookPressIn = 1; hookHeldIn = 1;
            for (int i = 0; i < 240; i++) {
                nowT += TICK;
                player_tick(TICK, 0, 0);
                hookPressIn = 0;
                if (P.hook == HOOK_ATTACHED) { attached = 1; if (s) P.yaw = 1.2f; }   /* 2e passe : on regarde ailleurs */
                lance_tick(TICK, 1);
                if (attached && P.hook == HOOK_NONE) break;
            }
            hookHeldIn = 0; hitstop = 0;
            dL[s] = hitCount[W_LANCE] - l0; dH[s] = hitCount[W_HOOK] - h0; hk[s] = P.hook; cd[s] = P.hookCd;
        }
        fprintf(f, "grappin + lance : touches lance %d, grappin %d, crochet %d, recharge %.2f ; sans viser : lance %d, grappin %d "
                   "(attendu 1, 0, 0, 0.00 ; 0, 1)\n", dL[0], dH[0], hk[0], cd[0], dL[1], dH[1]);
    }
    {   /* 10. bot a la Lance : la cible fonce a 10 m/s, le bot a 10 m/s depuis 10 m */
        lance_test_reset(v3(-20, 0, 41));
        lance_test_lan(v3(-20, 0, 31));
        vf(NP[1].st.vel, v3(0, 0, 10));
        P.vel = v3(0, 0, -10); P.dashCharge = 0.2f; P.fireCd = 10;
        int l0 = hitCount[W_LANCE];
        for (int i = 0; i < 180 && hitCount[W_LANCE] == l0; i++) {
            nowT += TICK;
            bot_think(TICK);
            game_tick(TICK);
        }
        hitstop = 0;
        fprintf(f, "bot lance : touches lance %d, PV cible %.0f (attendu >= 1, 40)\n", hitCount[W_LANCE] - l0, NP[1].hp);
        memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
        reset_game();
        dmMode = 0;
        memset(NP, 0, sizeof NP);
    }
    /* --- fin Lance --- */

    /* --- Ventouse --- */
    {   /* 1. vol et collage au sol, puis contre un mur (8 m) */
        mine_test_reset(v3(-20, 0, 40), PI, -0.3f);
        fire_mine();
        Proj *m = mine_test_last();
        int ammo = P.ammo[WP_MINE];
        for (int i = 0; i < 120; i++) proj_tick(TICK);
        fprintf(f, "ventouse sol : etat %d, normale (%.0f %.0f %.0f), y %.2f, z %.1f, munitions %d, jauge %.0f "
                   "(attendu 2, (0 1 0), 0.06, entre 43 et 46, 0, 0)\n",
                m->mineState, m->mineN.x, m->mineN.y, m->mineN.z, m->pos.y, m->pos.z, ammo, P.ammoGauge[WP_MINE]);
        mine_test_reset(v3(-20, 0, 70), PI, 0.1f);
        fire_mine();
        m = mine_test_last();
        mine_test_stick(m, 0);
        fprintf(f, "ventouse mur : etat %d, normale (%.0f %.0f %.0f), z %.2f (attendu 2, (0 0 -1), face du mur moins 0.06)\n",
                m->mineState, m->mineN.x, m->mineN.y, m->mineN.z, m->pos.z);
    }
    {   /* 2. meche : 3 s depuis le collage */
        mine_test_reset(v3(-20, 0, 40), PI, -0.3f);
        fire_mine();
        Proj *m = mine_test_last();
        mine_test_stick(m, 0);
        int n = 0;
        while (n < 1000 && m->alive) { proj_tick(TICK); n++; }
        fprintf(f, "ventouse meche : explosion %d ticks apres le collage (attendu 360 +- 1)\n", n);
    }
    {   /* 3. mine-jump (sans sauter), compare au rocket jump sans saut */
        mine_test_reset(v3(-20, 0, 40), 0, -1.5f);
        fire_mine();
        Proj *m = mine_test_last();
        mine_test_stick(m, 1);
        int st = m->mineState;
        mine_detonate(m, MX_REMOTE);
        float top = 0;
        for (int i = 0; i < 200; i++) { proj_tick(TICK); player_tick(TICK, 0, 0); top = fmaxf(top, P.pos.y); }
        float hpM = P.hp;
        reset_game();
        P.pos = P.prevPos = v3(-20, 0, 40); P.pitch = -1.5f;
        fire_rocket();
        float topR = 0;
        for (int i = 0; i < 200; i++) { proj_tick(TICK); player_tick(TICK, 0, 0); topR = fmaxf(topR, P.pos.y); }
        fprintf(f, "ventouse mine-jump : collee %d, apogee %.2f, PV %.0f ; rocket jump sans saut %.2f (attendu 2, > 4.3 (~4.6), 96 ; ~4.0)\n",
                st, top, hpM, topR);
    }
    {   /* 4. drone colle : MINE_DMG garantis ; un 2e drone a 3 m de la mine */
        mine_test_reset(v3(-20, 0, 40), 0, 0.13f);
        tesla_test_drone(0, v3(-20, 1.6f, 30), 1000);
        fire_mine();
        Proj *m = mine_test_last();
        mine_test_stick(m, 0);
        int st = m->mineState;
        tesla_test_drone(1, vadd(m->pos, v3(3, 0, 0)), 1000);
        mine_detonate(m, MX_REMOTE);
        fprintf(f, "ventouse drone colle : etat %d, degats %.2f, 2e drone a 3 m %.2f (attendu 3, %.2f, %.2f)\n",
                st, 1000 - E[0].hp, 1000 - E[1].hp, MINE_DMG,
                MINE_DMG * (0.3f + 0.7f * (1 - 3 / (MINE_RADIUS + enemy_radius(EN_DRONE)))));
    }
    {   /* 5. rail dans une mine posee a 2 m d'un drone (le drone derriere la mine n'est pas touche par le rail) */
        mine_test_reset(v3(-20, 0, 40), 0, 0);
        Proj *m = mine_test_put(v3(-20, 1.6f, 30), MS_WORLD, myId);
        tesla_test_drone(0, v3(-20, 1.6f, 28), 1000);
        P.weapon = WP_RAIL; P.ammo[WP_RAIL] = 2;
        fire_rail();
        fprintf(f, "ventouse rail : mine %d, degats au drone %.2f, message \"%s\", chaine %d (attendu 0, %.2f, MINE x1,5 !, 1)\n",
                m->alive, 1000 - E[0].hp, popText, shotStreak,
                MINE_DMG * MINE_BOOST_MULT * (0.3f + 0.7f * (1 - 2 / (MINE_BOOST_RADIUS + enemy_radius(EN_DRONE)))));
        hitstop = 0;
    }
    {   /* 5b. un plomb du SHOTGUN, puis l'arc de la Tesla, dans une mine a 2 m d'un drone (x1) */
        float dm[2];
        for (int k = 0; k < 2; k++) {
            mine_test_reset(v3(-20, 0, 40), 0, 0);
            mine_test_put(v3(-20, 1.6f, 34), MS_WORLD, myId);
            tesla_test_drone(0, v3(-18, 1.6f, 34), 1000);
            if (k == 0) { P.weapon = WP_SHOTGUN; P.ammo[WP_SHOTGUN] = 3; fire_shotgun(); }
            else { P.weapon = WP_TESLA; P.ammo[WP_TESLA] = 60; tesla_pulse(); tesla_flush(); }
            dm[k] = 1000 - E[0].hp;
            hitstop = 0;
        }
        float dx1 = MINE_DMG * (0.3f + 0.7f * (1 - 2 / (MINE_RADIUS + enemy_radius(EN_DRONE))));
        fprintf(f, "ventouse plomb / arc : degats au drone %.2f / %.2f (attendu %.2f / %.2f)\n", dm[0], dm[1], dx1, dx1);
    }
    {   /* 5c. piege : une mine posee explose seule quand un drone passe a moins de 2 m, une fois armee (0,4 s) ; pas a 4 m */
        int n[2];
        for (int k = 0; k < 2; k++) {
            mine_test_reset(v3(-20, 0, 40), 0, 0);
            Proj *m = mine_test_put(v3(-20, 1.6f, 30), MS_WORLD, myId);
            m->mineT = 0;
            tesla_test_drone(0, v3(-20 + (k ? 4.0f + enemy_radius(EN_DRONE) : 1.5f), 1.6f, 30), 1000);
            n[k] = 0;
            while (n[k] < 1000 && m->alive) { proj_tick(TICK); n[k]++; }
        }
        fprintf(f, "ventouse piege : drone a 1,5 m -> explosion apres %d ticks ; a 4 m du bord -> %d ticks (attendu %d, meche %d)\n",
                n[0], n[1], (int)(MINE_PROX_ARM / TICK + 0.5f), (int)(MINE_FUSE / TICK + 0.5f));
    }
    {   /* 6. limite de 2 : la 3e fait sauter la plus ancienne */
        mine_test_reset(v3(-20, 0, 40), PI, -0.3f);
        P.ammo[WP_MINE] = 3;
        unsigned int first = 0;
        for (int k = 0; k < 3; k++) {
            P.fireCd = 0;
            fire_mine();
            if (!k) first = mine_test_last()->mineSeq;
            for (int i = 0; i < 30; i++) proj_tick(TICK);
        }
        int n = 0, firstAlive = 0;
        for (int i = 0; i < MAXPR; i++)
            if (mine_own(&PR[i])) { n++; firstAlive |= PR[i].mineSeq == first; }
        fprintf(f, "ventouse limite : %d mines actives, la 1re vivante %d, munitions %d (attendu 2, 0, 0)\n", n, firstAlive, P.ammo[WP_MINE]);
    }
    {   /* 7. deux commandes : DETONER UNE (la plus ancienne), DETONER TOUT, puis sans mine (clic sec) */
        mine_test_reset(v3(-20, 0, 40), PI, -0.3f);
        P.ammo[WP_MINE] = 2;
        Proj *ma, *mb;
        fire_mine(); ma = mine_test_last();
        for (int i = 0; i < 30; i++) proj_tick(TICK);
        P.fireCd = 0;
        fire_mine(); mb = mine_test_last();
        for (int i = 0; i < 30; i++) proj_tick(TICK);
        botPress[A_DETONATE_ONE] = 1; game_tick(TICK); hitstop = 0;
        int aOne = ma->alive, bOne = mb->alive;
        botPress[A_DETONATE_ALL] = 1; game_tick(TICK); hitstop = 0;
        int bAll = mb->alive;
        float hp0 = P.hp;
        botPress[A_DETONATE_ONE] = 1; game_tick(TICK); hitstop = 0;
        fprintf(f, "ventouse commandes : UNE -> A %d B %d ; TOUT -> B %d ; sans mine : mines %d, PV %+.0f (attendu 0 1 ; 0 ; 0, +0)\n",
                aOne, bOne, bAll, mine_count(), P.hp - hp0);
    }
    {   /* 8. munitions par jauge : 3 coups d'epee = 1 mine, grande cellule = +1, plein a 3, aucune recharge passive */
        mine_test_reset(v3(-20, 0, 40), 0, 0);
        P.ammo[WP_MINE] = 0;
        tesla_test_drone(0, v3(-20, 1.6f, 38), 1000);
        for (int k = 0; k < 3; k++) { P.meleeCd = 0; P.parryWin = 0; hitstop = 0; melee(); }
        hitstop = 0; P.parryWin = 0;
        int sword = P.ammo[WP_MINE]; float gSword = P.ammoGauge[WP_MINE];
        int big = 0;
        for (int i = 0; i < npu; i++) if (PU[i].pts == PICKUP_BIG) { big = i; break; }
        pickup_taken(big, myId); PU[big].t = 0;
        int cell = P.ammo[WP_MINE];
        P.ammo[WP_MINE] = 3; P.ammoGauge[WP_MINE] = 0;
        pickup_taken(big, myId); PU[big].t = 0;
        int full = P.ammo[WP_MINE]; float gFull = gauge_frac(WP_MINE);
        E[0].alive = 0;
        P.ammo[WP_MINE] = 0; P.ammoGauge[WP_MINE] = 0;
        for (int i = 0; i < 3600; i++) { game_tick(TICK); hitstop = 0; }
        fprintf(f, "ventouse jauge : 3 coups d'epee -> %d mine(s), jauge %.0f ; grande cellule (%d) -> %d ; a 3 + cellule -> %d, "
                   "jauge affichee %.0f%% ; 30 s a vide -> %d, jauge %.0f (attendu 1, 0 ; 150, 2 ; 3, 100%% ; 0, 0)\n",
                sword, gSword, PU[big].pts, cell, full, 100 * gFull, P.ammo[WP_MINE], P.ammoGauge[WP_MINE]);
    }
    {   /* 9. LAN : mine collee sur un joueur, detonation demandee a 0,3 s, explosion a l'armement (0,6 s), 60 garantis */
        mine_test_reset(v3(-20, 0, 40), 0, 0);
        mine_test_lan(v3(-20, 0, 34));
        fire_mine();
        Proj *m = mine_test_last();
        mine_test_stick(m, 0);
        int st = m->mineState, at = m->mineAttach;
        int t = 0;
        for (; t < 36; t++) { nowT += TICK; proj_tick(TICK); }
        mine_detonate(m, MX_REMOTE);
        float hpMid = NP[1].hp;
        while (t < 600 && m->alive) { nowT += TICK; proj_tick(TICK); t++; }
        hitstop = 0;
        fprintf(f, "ventouse LAN : etat %d sur %d, PV a 0,3 s %.0f, explosion a %.2f s, PV %.0f (attendu 4 sur 2, 100, 0.60, 40)\n",
                st, at, hpMid, t * TICK, NP[1].hp);
    }
    {   /* 7b. LAN : A collee sur un joueur (pas armee), B au sol : deux DETONER UNE dans le meme 0,1 s */
        mine_test_reset(v3(-20, 0, 40), 0, 0);
        mine_test_lan(v3(-20, 0, 30));
        Proj *ma = mine_test_put(v3(-20, 1, 30.4f), MS_PLAYER, myId), *mb = mine_test_put(v3(-14, 0.06f, 30), MS_WORLD, myId);
        ma->mineAttach = 2; ma->mineOfs = v3(0, 1, 0.4f); ma->mineArmT = MINE_ARM_PLAYER;
        mine_remote(0);
        mine_remote(0);
        hitstop = 0;
        fprintf(f, "ventouse UNE x2 : A vivante %d (detonation en attente %d), B vivante %d (attendu 1 (1), 0)\n",
                ma->alive, ma->mineDet != 0, mb->alive);
        ma->alive = 0;
    }
    {   /* 10. arrachage : mine adverse collee sur moi, parry -> renvoyee dans ma visee a 36 m/s */
        mine_test_reset(v3(-20, 0, 40), 0.3f, 0.1f);
        mine_test_lan(v3(-20, 0, 30));
        Proj *m = mine_test_put(vadd(P.pos, v3(0, 1, 0.3f)), MS_PLAYER, 1);
        m->mineAttach = myId + 1; m->mineOfs = v3(0, 1, 0.3f); m->mineArmT = 0.3f;
        P.parryWin = 0.1f;
        int n = try_parry(MELEE_RANGE + 1.5f, 1);
        fprintf(f, "ventouse arrachage : pares %d, kind %d, owner %d, vitesse %.1f, alignement %.4f, mult %.2f, message \"%s\" "
                   "(attendu 1, %d, 0, 36.0, > 0.999, 1.25, ARRACHEE !)\n",
                n, m->kind, m->owner, vlen(m->vel), vdot(vnorm(m->vel), view_dir()), m->mult, popText, PR_PARRIED);
        m->alive = 0; P.parryWin = 0; hitstop = 0;
    }
    {   /* 11. arbitrage de l'hote : arrachage puis touche du poseur ; puis explosion, touche, arrachage tardif */
        mine_test_reset(v3(-20, 0, 40), 0, 0);
        mine_test_lan(v3(-20, 0, 30));
        unsigned int id1 = (1u << 24) | ((unsigned int)PR_MINE << 20) | 0x9001, id2 = id1 + 1;
        NetEvent e = ev_new(EV_MINE);
        e.sub = MINE_EV_TORN; e.from = 1; e.target = 1; e.netId = id1;
        int r1 = host_handle(&e);
        NetEvent h = ev_new(EV_HIT);
        h.from = 0; h.target = 1; h.sub = W_MINE; h.f[0] = 63; h.netId = id1;
        int r2 = host_handle(&h);
        float hpA = NP[1].hp;
        NP[1].hp = 100;
        int c0 = mineCancels;
        e.sub = MINE_EV_BOOM; e.from = 0; e.netId = id2;
        int r3 = host_handle(&e);
        h.netId = id2;
        int r4 = host_handle(&h);
        float hpB = NP[1].hp;
        e.sub = MINE_EV_TORN; e.from = 1;
        int r5 = host_handle(&e);
        fprintf(f, "ventouse arbitrage : arrachage %d puis touche du poseur annulee %d, PV %.0f ; explosion %d, touche %d, PV %.0f, "
                   "arrachage tardif annule %d, CANCEL %d (attendu 0 1, 100 ; 0 0, 40, 1, 1)\n",
                r1, r2, hpA, r3, r4, hpB, r5, mineCancels - c0);
    }
    {   /* 12. mine adverse volee au rail : son poseur pres de la mine prend le souffle (plafonne a 60) */
        mine_test_reset(v3(-20, 0, 41), 0, 0);
        mine_test_lan(v3(-18.5f, 0, 33));
        Proj *m = mine_test_put(v3(-20, 1.6f, 33), MS_WORLD, 1);
        P.weapon = WP_RAIL; P.ammo[WP_RAIL] = 2;
        fire_rail();
        hitstop = 0;
        fprintf(f, "ventouse volee : mine %d, PV du poseur %.0f, message \"%s\" (attendu 0, 40, MINE VOLEE !)\n", m->alive, NP[1].hp, popText);
    }
    {   /* 13. reception : copie d'une mine adverse, collee sur moi (alerte), puis explosee */
        mine_test_reset(v3(-20, 0, 40), 0, 0);
        mine_test_lan(v3(-20, 0, 30));
        unsigned int id = (2u << 24) | ((unsigned int)PR_MINE << 20) | 0x9100;
        NetEvent e = ev_new(EV_MINE);
        e.sub = MINE_EV_THROW; e.from = 1; e.netId = id; vf(e.f, v3(-20, 1.6f, 31)); vf(e.f + 3, v3(0, 0, 24));
        apply_event(&e);
        Proj *p = find_proj(id);
        int copy = p && p->kind == PR_MINE && p->owner == 1 && p->mineState == MS_FLY;
        e.sub = MINE_EV_PLAYER; e.target = 0; vf(e.f, v3(0, 1.2f, -0.4f));
        apply_event(&e);
        int onMe = p && p->mineState == MS_PLAYER && p->mineAttach == 1;
        char alert[32];
        snprintf(alert, sizeof alert, "%s", popText);
        e.sub = MINE_EV_BOOM; vf(e.f, v3(-20, 1.2f, 39.6f)); vf(e.f + 3, v3(0, 0, 0));
        apply_event(&e);
        fprintf(f, "ventouse reseau : copie %d, collee sur moi %d, alerte \"%s\", retiree %d (attendu 1, 1, MINE SUR TOI ! PARE !, 1)\n",
                copy, onMe, alert, find_proj(id) == NULL);
    }
    {   /* 14. bot a la Ventouse : cible immobile a 12 m, il lance, la mine colle, il la fait sauter une fois armee */
        mine_test_reset(v3(-20, 0, 41), 0, 0);
        mine_test_lan(v3(-20, 0, 29));
        P.ammo[WP_MINE] = 2; P.dashCharge = 0.2f; P.hookCd = 100;
        int t0 = mineThrows, h0 = hitCount[W_MINE], i = 0;
        for (; i < 480 && hitCount[W_MINE] == h0; i++) {
            nowT += TICK;
            bot_think(TICK);
            game_tick(TICK);
            P.hookCd = 100;
        }
        hitstop = 0;
        fprintf(f, "bot ventouse : lancers %d, touches ventouse %d apres %.2f s, PV cible %.0f (attendu >= 1, >= 1, < 100)\n",
                mineThrows - t0, hitCount[W_MINE] - h0, i * TICK, NP[1].hp);
        memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
        reset_game();
        dmMode = 0;
        memset(NP, 0, sizeof NP);
        memset(hostMine, 0, sizeof hostMine);
    }
    /* --- fin Ventouse --- */

    /* --- Fleau (solo) : couloir degage x = -20, joueur en (-20, 0, 40), vue vers -z --- */
    {   /* 1. degats d'un drone a 5,5 m, AUCUNE traction : sa vitesse n'est pas modifiee par le coup (avant le patch : 6 m/s
         * vers le joueur, le drone venait a 2,5 m) et sa position ne change pas pendant le coup */
        flail_test_reset(v3(-20, 0, 40));
        Enemy *e = flail_test_enemy(0, v3(-20, 1.6f, 34.5f), EN_DRONE);
        V3 p0 = e->pos;
        flail_swing();
        float hp = e->hp, v = vlen(e->vel), vz = e->vel.z, moved = vlen(vsub(e->pos, p0));
        for (int i = 0; i < 60; i++) enemy_tick(TICK);
        float dEnd = vlen(vsub(e->pos, eye_pos()));
        hitstop = 0;
        fprintf(f, "fleau drone a 5.5 m : PV %.0f, vitesse apres le coup %.2f (vel.z %.2f), deplacement %.2f, distance apres 0.5 s %.2f "
                   "(attendu 82, 0.00 (0.00), 0.00, > 4.0 : plus de traction ni d'arret a 2,5 m)\n", hp, v, vz, moved, dEnd);
    }
    {   /* 2. a 20 m/s : portee 7 m (drone a 7,6 m, rayon 0,9), 18 + 1,5 x 20 = 48, dash +1 */
        flail_test_reset(v3(-20, 0, 40));
        P.vel = v3(0, 0, -20); P.dashCharge = 0.5f;
        Enemy *e = flail_test_enemy(0, v3(-20, 1.6f, 32.4f), EN_DRONE);
        flail_swing();
        hitstop = 0;
        fprintf(f, "fleau a 20 m/s, drone a 7.6 m : PV %.0f, dash %.1f, cibles %d (attendu 52, 1.5, 1)\n", e->hp, P.dashCharge, P.flailHitN);
    }
    {   /* 3. arc de 140 degres : drones a 0, +60, -60 et 80 degres a 5 m ; un seul gain de points pour le coup */
        static const float ang[4] = { 0, 1.0472f, -1.0472f, 1.3963f };
        flail_test_reset(v3(-20, 0, 40));
        P.loadout[0] = WP_ROCKET; P.loadout[1] = WP_RAIL; P.weapon = WP_ROCKET; P.ammo[WP_ROCKET] = 4; P.ammoGauge[WP_ROCKET] = 0;
        for (int k = 0; k < 4; k++) flail_test_enemy(k, v3(-20 + 5 * sinf(ang[k]), 1.6f, 40 - 5 * cosf(ang[k])), EN_DRONE);
        flail_swing();
        hitstop = 0;
        int n = 0;
        for (int k = 0; k < 4; k++) n += E[k].hp < 100;
        fprintf(f, "fleau arc : %d drones touches, celui a 80 deg PV %.0f, roquettes %d jauge %.0f (attendu 3, 100, 4, 35)\n",
                n, E[3].hp, P.ammo[WP_ROCKET], P.ammoGauge[WP_ROCKET]);
    }
    {   /* 4. ligne de vue : un muret entre le joueur et un drone a 5 m ; coup dans le vide : 0,9 s depuis l'appui */
        flail_test_reset(v3(-20, 0, 40));
        Enemy *e = flail_test_enemy(0, v3(-20, 1.6f, 35), EN_DRONE);
        int nb0 = nb;
        addbox(-21, 0, 36.5f, -19, 3.5f, 37.5f, 0.5f, 0.5f, 0.5f);
        P.meleeCd = 0;
        botPress[A_MELEE] = 1;                            /* comme un appui : game_tick -> melee -> flail_swing */
        int ticks = 0;
        do { game_tick(TICK); ticks++; } while (P.parryWin > 0 && ticks < 60);
        float hp = e->hp;
        int landed = P.swingLanded;
        nb = nb0;
        memset(E, 0, sizeof E);
        fprintf(f, "fleau derriere un muret : PV drone %.0f, touche %d, recharge %.2f apres %.2f s (attendu 100, 0, 0.80, ~0.1)\n",
                hp, landed, P.meleeCd, ticks * TICK);
    }
    {   /* 5. kamikaze a 5 m : il explose au bout de la chaine, sans soin de parry (seulement le soin du kill, 8) */
        flail_test_reset(v3(-20, 0, 40));
        P.hp = 80;
        Enemy *e = flail_test_enemy(0, v3(-20, 1.6f, 35), EN_RUSHER);
        flail_swing();
        hitstop = 0;
        fprintf(f, "fleau kamikaze : vivant %d, PV joueur %.0f (attendu 0, 88)\n", e->alive, P.hp);
    }
    {   /* 6. piece frappee : ma piece devant, drone a 22 m plus loin -> 45 m/s, 40 degats */
        flail_test_reset(v3(-20, 0, 40));
        Enemy *e = flail_test_enemy(0, v3(-20, 2, 15), EN_DRONE);
        Coin *c = &CO[0];
        c->alive = 1; c->owner = myId; c->life = 3; c->pos = v3(-20, 2.5f, 37); c->vel = v3(0, 0, 0); c->netId = 0x01000077;
        P.dashCharge = 0.5f;                              /* une piece frappee ne rend pas de dash */
        flail_swing();
        int struck = c->flailStruck;
        float sp = vlen(c->vel);
        for (int i = 0; i < 90; i++) coin_tick(TICK);
        hitstop = 0;
        fprintf(f, "fleau piece frappee : frappee %d a %.0f m/s, PV drone %.0f, piece vivante %d, dash %.1f (attendu 1, 45, 60, 0, 0.5)\n",
                struck, sp, e->hp, c->alive, P.dashCharge);
    }
    {   /* 7. piece frappee + rail : le rail dans la piece en vol ricoche (x1,5), ses 40 degats sont remplaces */
        flail_test_reset(v3(-20, 0, 40));
        Enemy *e = flail_test_enemy(0, v3(-20, 2, 15), EN_DRONE);
        Coin *c = &CO[0];
        c->alive = 1; c->owner = myId; c->life = 3; c->pos = v3(-20, 2.5f, 37); c->vel = v3(0, 0, 0); c->netId = 0x01000078;
        flail_swing();
        for (int i = 0; i < 3; i++) coin_tick(TICK);
        V3 d = vsub(c->pos, eye_pos());
        P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
        lastChain = 0;
        fire_rail();
        hitstop = 0;
        fprintf(f, "fleau piece frappee + rail : chaine %d, drone vivant %d (attendu 1, 0)\n", lastChain, e->alive);
    }
    {   /* 8. parry a 6 m : orbe a 5,5 m renvoyee a 36 x 0,8 ; l'epee, elle, ne l'atteint pas */
        flail_test_reset(v3(-20, 0, 40));
        Proj *o = spawn_proj(PR_ORB, v3(-20, 1.6f, 34.5f), v3(0, 0, 16));
        flail_swing();
        int okF = o->kind == PR_PARRIED;
        float spF = vlen(o->vel);
        flail_test_reset(v3(-20, 0, 40));
        P.meleeKind = MK_SWORD;
        o = spawn_proj(PR_ORB, v3(-20, 1.6f, 34.5f), v3(0, 0, 16));
        melee();
        hitstop = 0;
        fprintf(f, "fleau parry a 5.5 m : renvoyee %d a %.1f m/s ; epee : renvoyee %d (attendu 1 a 28.8 ; 0)\n", okF, spF, o->kind == PR_PARRIED);
        memset(PR, 0, sizeof PR);
    }
    /* --- Fleau en LAN simule (je suis l'hote) --- */
    {   /* 9. touche : 18 x 0,7 = 12,6 ; recul NUL (avant le patch : traction de 12 m/s + 3 vers le haut, (0, 3, 12)) */
        flail_test_reset(v3(-20, 0, 40));
        lance_test_lan(v3(-20, 0, 35));
        flail_swing();
        hitstop = 0;
        const NetEvent *h = flail_test_last_hit();
        V3 kn = h ? fv(h->f + 1) : v3(0, 0, 0);
        fprintf(f, "fleau PvP : PV cible %.1f, arme %d, recul (%.1f, %.1f, %.1f) (attendu 87.4, %d, (0.0, 0.0, 0.0) : cible non tiree)\n",
                NP[1].hp, h ? h->sub : -1, kn.x, kn.y, kn.z, W_FLAIL);
    }
    {   /* 10. la cible pare : pas de degats, le LANCEUR est tire vers elle (seule traction restante), fleau bloque 1 s ;
         * deux pareurs : une seule traction */
        flail_test_reset(v3(-20, 0, 40));
        lance_test_lan(v3(-20, 0, 35));
        NP[1].lastParryT = nowT;
        int fp = flailHostParried;
        flail_swing();
        hitstop = 0;
        float hp1 = NP[1].hp, vz = P.vel.z, vy = P.vel.y, cd = P.meleeCd;
        fp = flailHostParried - fp;
        flail_test_reset(v3(-20, 0, 40));
        lance_test_lan(v3(-20, 0, 35));
        NP[2].active = NP[2].alive = 1; NP[2].hp = 100; snprintf(NP[2].name, 16, "Cible2");
        NP[2].disp = v3(-21, 0, 35); vf(NP[2].st.pos, NP[2].disp);
        NP[1].lastParryT = NP[2].lastParryT = nowT;
        flail_swing();
        hitstop = 0;
        fprintf(f, "fleau pare : PV cible %.0f, fleaux pares +%d, vel attaquant (y %.1f, z %.1f), recharge %.2f ; deux pareurs : |v| %.1f "
                   "(attendu 100, +1, (4.0, -16.0), 1.00 ; 16.5)\n", hp1, fp, vy, vz, cd, vlen(P.vel));
    }
    {   /* 11. bot au Fleau : cible a 5 m, armes a distance vides -> il frappe a la chaine, la cible n'est pas tiree et aucun
         * combo fleau -> rail n'existe plus (avant le patch : le coup puis le rail dans les 0,3 s, "RAPPEL + RAIL") */
        flail_test_reset(v3(-20, 0, 40));
        lance_test_lan(v3(-20, 0, 35));
        P.dashCharge = 0.2f;                              /* ni cross-up ni dash */
        P.ammo[P.loadout[0]] = P.ammo[P.loadout[1]] = 0;  /* seule la melee est possible */
        int h0 = hitCount[W_FLAIL], i = 0;
        for (; i < 120 && hitCount[W_FLAIL] == h0; i++) {
            nowT += TICK;
            bot_think(TICK);
            game_tick(TICK);
        }
        hitstop = 0;
        const NetEvent *h = flail_test_last_hit();
        V3 kn = h ? fv(h->f + 1) : v3(9, 9, 9);
        fprintf(f, "bot fleau : touches fleau %d en %.2f s, PV cible %.1f, recul envoye (%.1f, %.1f, %.1f) (attendu 1, < 0.35, 86 a 87.4 selon v_rel, (0.0, 0.0, 0.0))\n",
                hitCount[W_FLAIL] - h0, i * TICK, NP[1].hp, kn.x, kn.y, kn.z);
        memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
        reset_game();
        dmMode = 0;
        memset(NP, 0, sizeof NP);
    }
    /* --- fin Fleau --- */

    /* --- Yoyo : traction sur appui long (touche de melee tenue plus de FLAIL_HOLD_T) : les cibles de l'arc sont tirees tant qu'elle est tenue.
     * Solo : couloir degage x = -20, joueur en (-20, 0, 40), vue vers -z ; LAN simule : moi (hote) et NP[1] --- */
    {
        FILE *savedLog = botLog;
        double nowSave = nowT;
        unsigned int rngSave = rng;
        botLog = NULL;                                    /* pas de lignes du journal du bot au milieu des resultats */
        memset(flailLinkT, 0, sizeof flailLinkT); memset(flailLinkBy, 0, sizeof flailLinkBy);
        {   /* 12. appui long sur un drone a 5,5 m : le coup part a l'appui (82), la chaine s'accroche a 0,15 s et le tire vers l'oeil tant que la
             * touche est tenue (il s'arrete vers 2,5 m) ; relache : plus de traction, recharge de 0,5 s, son IA le reprend */
            flail_test_reset(v3(-20, 0, 40)); waveDelay = 1000;
            Enemy *e = flail_test_enemy(0, v3(-20, 1.6f, 34.5f), EN_DRONE);
            float tHook = -1, d06 = 0, dmin = 1e9f, vmax = 0;
            int pulls0 = flailPulls;
            for (int i = 0; i < 180; i++) {               /* 1,5 s, touche tenue */
                flail_test_run(1, i == 0, 1);
                float d = vlen(vsub(e->pos, eye_pos()));
                if (tHook < 0 && P.flailPullN > 0) tHook = (i + 1) * TICK;
                if (i == 71) d06 = d;
                dmin = fminf(dmin, d);
                vmax = fmaxf(vmax, vdot(e->vel, vnorm(vsub(eye_pos(), e->pos))));
            }
            float hp = e->hp, dEnd = vlen(vsub(e->pos, eye_pos()));
            int held = P.flailPullN;
            flail_test_run(1, 0, 0);                      /* relache */
            float cd = P.meleeCd, dRel = vlen(vsub(e->pos, eye_pos()));
            int still = P.flailPullN;
            flail_test_run(120, 0, 0);                    /* 1 s sans traction : son IA l'emmene ailleurs */
            fprintf(f, "fleau traction drone a 5.5 m : PV %.0f, accroche a %.2f s (%d traction), distance a 0.6 s %.2f, mini %.2f, a 1.5 s %.2f, "
                       "vitesse max vers moi %.1f, encore accroche %d ; relache : accroche %d, recharge %.2f, distance %.2f puis %.2f 1 s plus tard "
                       "(attendu 82, 0.15, 1, < 4.0, >= 2.3, 2.4 a 3.0, <= 13, 1 ; 0, 0.50, 2.4 a 3.0 puis > 4.0)\n",
                    hp, tHook, flailPulls - pulls0, d06, dmin, dEnd, vmax, held, still, cd, dRel, vlen(vsub(e->pos, eye_pos())));
        }
        {   /* 13. appui court : le coup part, aucune traction (touche relachee avant FLAIL_HOLD_T) ; tenue 0,10 s : encore un appui court */
            int pulled[2]; float dm[2], hpc[2];
            for (int v = 0; v < 2; v++) {
                flail_test_reset(v3(-20, 0, 40)); waveDelay = 1000;
                Enemy *e = flail_test_enemy(0, v3(-20, 1.6f, 34.5f), EN_DRONE);
                int p0 = flailPulls;
                dm[v] = 1e9f;
                flail_test_run(v ? 12 : 1, 1, 1);         /* tenue 1 tick, ou 0,10 s */
                for (int i = 0; i < 120; i++) { flail_test_run(1, 0, 0); dm[v] = fminf(dm[v], vlen(vsub(e->pos, eye_pos()))); }
                pulled[v] = flailPulls - p0 + P.flailPullN; hpc[v] = e->hp;
            }
            fprintf(f, "fleau appui court : PV %.0f et %.0f, tractions %d et %d (tenue 1 tick / 0.10 s), distance mini %.2f et %.2f "
                       "(attendu 82, 82, 0, 0, > 4.0, > 4.0)\n", hpc[0], hpc[1], pulled[0], pulled[1], dm[0], dm[1]);
        }
        {   /* 14. un boss n'est jamais tire (boss et drone dans l'arc : seul le drone est accroche) ; un kamikaze dans l'arc a ce moment explose */
            flail_test_reset(v3(-20, 0, 40)); waveDelay = 1000;
            Enemy *b = flail_test_enemy(0, v3(-20, 1.6f, 35), EN_PYRA);
            b->hp = b->maxHp = 99999;
            flail_test_enemy(1, v3(-20, 1.6f, 36), EN_DRONE);
            Enemy *r = flail_test_enemy(2, v3(-19, 1.6f, 35.5f), EN_RUSHER);
            flail_hook_start();
            int n = P.flailPullN, first = P.flailPullN ? P.flailHk[0] : -1;
            for (int i = 0; i < 60; i++) flail_pull_tick(TICK, 1);
            fprintf(f, "fleau traction boss : cibles accrochees %d (le drone, indice %d), vitesse du boss %.2f, kamikaze vivant %d "
                       "(attendu 1, 1, 0.00, 0)\n", n, first, vlen(b->vel), r->alive);
        }
        {   /* 15. coupures : cible a plus de 8 m, mur sur la ligne de vue (plus de 0,25 s), lanceur mort : la cible est lachee, la recharge de 0,5 s demarre */
            float tc[3], cdc[3];
            int ok[3];
            for (int c = 0; c < 3; c++) {
                flail_test_reset(v3(-20, 0, 40)); waveDelay = 1000;
                Enemy *e = flail_test_enemy(0, v3(-20, 1.6f, 34.5f), EN_DRONE);
                int nb0 = nb;
                flail_test_run(30, 1, 1);                 /* accroche */
                ok[c] = P.flailPullN;
                if (c == 0) { e->pos = v3(-20, 1.6f, 30.5f); e->vel = v3(0, 0, 0); }   /* 9,5 m */
                if (c == 1) addbox(-21, 0, 37.5f, -19, 3.5f, 38.5f, 0.5f, 0.5f, 0.5f);   /* un mur entre nous */
                if (c == 2) P.dead = 1;
                int t = 0;
                for (; t < 120 && P.flailPullN > 0; t++) flail_test_run(1, 0, 1);
                tc[c] = t * TICK; cdc[c] = P.meleeCd;
                nb = nb0;
            }
            fprintf(f, "fleau traction coupee : accroche %d %d %d ; lachee apres %.3f s (distance), %.3f s (mur), %.3f s (mort), recharge %.2f %.2f %.2f "
                       "(attendu 1 1 1 ; 0.008, 0.25 a 0.28, 0.008, 0.50 0.50 0.50)\n", ok[0], ok[1], ok[2], tc[0], tc[1], tc[2], cdc[0], cdc[1], cdc[2]);
        }
        {   /* 16. tant que la touche est tenue : aucun nouveau coup, meme en rappuyant a chaque tick ; le rail part pendant la traction et tue le
             * drone ramene (combo fleau -> rail) */
            flail_test_reset(v3(-20, 0, 40)); waveDelay = 1000;
            P.loadout[0] = WP_RAIL; P.weapon = WP_RAIL; P.ammo[WP_RAIL] = 2; P.fireCd = 0; P.switchT = 0;
            Enemy *e = flail_test_enemy(0, v3(-20, 1.6f, 34.5f), EN_DRONE);
            int s0 = flailSwings;
            for (int i = 0; i < 90; i++) flail_test_run(1, 1, 1);   /* 0,75 s : appui a chaque tick, touche tenue */
            int swings = flailSwings - s0, pulling = P.flailPullN;
            float d = vlen(vsub(e->pos, eye_pos()));
            V3 dv = vsub(e->pos, eye_pos());
            P.yaw = atan2f(dv.x, -dv.z); P.pitch = asinf(dv.y / vlen(dv));
            botPress[A_FIRE] = 1; botHeld[A_FIRE] = 1;
            flail_test_run(1, 0, 1);
            botPress[A_FIRE] = 0; botHeld[A_FIRE] = 0;
            int dead = !e->alive;
            flail_test_run(3, 0, 1);
            fprintf(f, "fleau traction + rail : coups de fleau %d, accroche %d a %.2f m, rail : drone mort %d, traction encore %d "
                       "(attendu 1, 1, 2.4 a 3.2, 1, 0)\n", swings, pulling, d, dead, P.flailPullN);
        }
        /* --- LAN simule (je suis l'hote) --- */
        {   /* 17. je tire (hote) : START a l'accroche (0,15 s), un PULL tous les 0,1 s, END au relachement ; la cible n'est touchee qu'une fois */
            flail_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 34));
            memset(outq, 0, sizeof outq);
            flail_test_run(1, 1, 1);
            flail_test_run(59, 0, 1);                     /* 0,5 s tenue */
            float sc = 0;
            int st = flail_test_count(FLE_START, 1, &sc), pu = flail_test_count(FLE_PULL, 1, NULL), n1 = P.flailPullN;
            float hp = NP[1].hp;
            flail_test_run(1, 0, 0);                      /* relache */
            int en = flail_test_count(FLE_END, 1, NULL);
            fprintf(f, "fleau traction LAN (je tire) : START %d (intensite %.1f), PULL %d, accroche %d, PV cible %.1f ; relache : END %d, accroche %d "
                       "(attendu 1 (1.0), 3, 1, 87.4 ; 1, 0)\n", st, sc, pu, n1, hp, en, P.flailPullN);
        }
        {   /* 18. victime : un lanceur a 6 m me tire a 12 m/s au plus et je m'arrete vers 2 m ; sans rafraichissement, libre au bout du bail (0,3 s) */
            flail_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 34));
            P.grounded = 1;
            NetEvent st = flail_test_ev(1, 0, FLE_START, 1);
            apply_event(&st);
            int hooked = P.flailBy;
            float vmax, dmin, dEnd, v2, d2, e2;
            flail_test_victim(120, 1, 1, &vmax, &dmin, &dEnd);       /* 1 s, bail rafraichi toutes les 0,1 s */
            int still = P.flailBy;
            flail_test_victim(25, 0, 1, &v2, &d2, &e2);              /* dernier rafraichissement il y a 0,23 s */
            int inLease = P.flailBy;
            flail_test_victim(15, 0, 1, &v2, &d2, &e2);              /* 0,33 s : bail fini */
            fprintf(f, "fleau traction victime : accrochee par %d, vitesse max %.1f, distance mini %.2f, finale %.2f, encore accrochee %d ; sans rafraichissement : "
                       "%d a 0.23 s, %d a 0.33 s (attendu 2, <= 12.3, >= 1.7, 1.9 a 2.5, 2 ; 2, 0)\n",
                    hooked, vmax, dmin, dEnd, still, inLease, P.flailBy);
        }
        {   /* 19. contre-jeu : courir a l'oppose (touche arriere tenue) ne bat pas la traction ; un dash a 18 m/s vers l'exterieur arrache la chaine (le lanceur
             * est prevenu : FLE_BREAK) ; l'Egide (intensite 1/2) divise la vitesse de traction par deux */
            flail_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 35.5f));
            P.grounded = 1; act[A_BACK] = 1;
            NetEvent st = flail_test_ev(1, 0, FLE_START, 1);
            apply_event(&st);
            float vmax, dmin, dEnd;
            flail_test_victim(100, 1, 1, &vmax, &dmin, &dEnd);       /* 0,83 s en courant en arriere */
            float vzRun = P.vel.z, dRun = dEnd; int tiedRun = P.flailBy;
            memset(outq, 0, sizeof outq);
            int sn0 = flailSnaps;
            P.vel = v3(0, 0, 18);                         /* dash vers l'exterieur (+z : je m'eloigne du lanceur, en -z) */
            flail_test_victim(1, 1, 1, &vmax, &dmin, &dEnd);
            int tiedDash = P.flailBy, brk = flail_test_count(FLE_BREAK, 1, NULL);
            act[A_BACK] = 0; P.vel = v3(0, 0, 0);
            lance_test_lan(v3(-20, 0, 33));               /* lanceur a 7 m, Egide de face : intensite 0,5 */
            P.pos = P.prevPos = v3(-20, 0, 40); P.flailBy = 0; P.grounded = 1;
            NetEvent sh = flail_test_ev(1, 0, FLE_START, SHIELD_KNOCK_MULT);
            apply_event(&sh);
            flail_test_victim(60, 1, SHIELD_KNOCK_MULT, &vmax, &dmin, &dEnd);
            fprintf(f, "fleau traction contre-jeu : course arriere (depart a 4.5 m) : vitesse z %.1f, distance %.2f, accrochee %d ; dash a 18 m/s : accrochee %d, "
                       "FLE_BREAK %d, arrachees +%d ; Egide : vitesse max %.1f (attendu <= 0 (jamais vers l'exterieur), < 3.0, 2 ; 0, 1, +1 ; <= 6.2)\n",
                    vzRun, dRun, tiedRun, tiedDash, brk, flailSnaps - sn0, vmax);
        }
        {   /* 20. l'hote arbitre : Egide de face = intensite 0,5 dans le START relaye, sans Egide 1 ; lanceur sans Yoyo, trop loin ou allie : refuse */
            float sc[2] = { 0, 0 };
            for (int sh = 0; sh < 2; sh++) {
                flail_test_reset(v3(-20, 0, 40));
                lance_test_lan(v3(-20, 0, 34));
                memset(outq, 0, sizeof outq);
                for (int i = 0; i < 30; i++) {
                    if (sh) { NP[1].st.pad = (unsigned char)(MK_SHIELD | ST_SHIELD); NP[1].st.yaw = PI; NP[1].shieldGauge = SHIELD_GAUGE_MAX; NP[1].lastShieldT = nowT; }
                    flail_test_run(1, i == 0, 1);
                }
                flail_test_count(FLE_START, 1, &sc[sh]);
            }
            flail_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 34));
            memset(outq, 0, sizeof outq);
            int d0 = flailHostDenied;
            NP[0].st.pad = MK_SWORD; net_fx_flail(FLE_START, 1);                       /* pas de Yoyo */
            NP[0].st.pad = MK_FLAIL; NP[1].disp = v3(-20, 0, 20); net_fx_flail(FLE_START, 1);   /* 20 m : trop loin */
            NP[1].disp = v3(-20, 0, 34); teamSize = 2; NP[0].team = NP[1].team = 0; net_fx_flail(FLE_START, 1);   /* allie */
            teamSize = 0;
            int refused = flailHostDenied - d0, queued0 = flail_test_count(FLE_START, 1, NULL);
            net_fx_flail(FLE_START, 1);                   /* en regle */
            fprintf(f, "fleau traction arbitre : intensite %.1f sans Egide, %.1f face a une Egide ; refus (sans Yoyo, trop loin) %d, relayes avant %d, apres %d "
                       "(attendu 1.0, 0.5 ; 2, 0, 1)\n", sc[0], sc[1], refused, queued0, flail_test_count(FLE_START, 1, NULL));
        }
        {   /* 21. fleau pare : la chaine est retournee et l'appui long est desarme (touche tenue : aucune traction) ; parade pendant une traction : coupee, END envoye */
            flail_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 34));
            NP[1].lastParryT = nowT;
            memset(outq, 0, sizeof outq);
            int p0 = flailPulls;
            flail_test_run(1, 1, 1);
            flail_test_run(40, 0, 1);
            int pulledAfterParry = flailPulls - p0 + P.flailPullN, st0 = flail_test_count(FLE_START, 1, NULL);
            float cd = P.meleeCd;
            memset(outq, 0, sizeof outq);
            net_fx_flail(FLE_START, 1);                   /* START en retard (client lent) : refuse par l'hote, la parade date de 0,34 s */
            int late = flail_test_count(FLE_START, 1, NULL);
            nowT += 0.5;
            net_fx_flail(FLE_START, 1);                   /* 0,84 s apres la parade : accepte */
            int later = flail_test_count(FLE_START, 1, NULL);
            flail_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 34));
            memset(outq, 0, sizeof outq);
            flail_test_run(1, 1, 1);
            flail_test_run(40, 0, 1);
            int before = P.flailPullN;
            flail_parried(np_center(1), pcenter());       /* EV_MELEE_PARRY recu en pleine traction */
            fprintf(f, "fleau traction et parry : parade au coup : tractions %d, START %d, recharge %.2f, START en retard refuse par l'hote %d puis accepte %d ; "
                       "parade en pleine traction : avant %d, apres %d, END %d (attendu 0, 0, 0.6 a 1.0, 0, 1 ; 1, 0, 1)\n",
                    pulledAfterParry, st0, cd, late, later, before, P.flailPullN, flail_test_count(FLE_END, 1, NULL));
        }
        {   /* 22. observateur : la chaine d'un autre joueur sur un autre joueur est gardee pour le dessin (bail 0,3 s) ; un rafraichissement sans accroche
             * ne raccroche pas la victime (moi) */
            flail_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 34));
            NP[2].active = NP[2].alive = 1; NP[2].hp = 100; snprintf(NP[2].name, 16, "Autre"); NP[2].disp = v3(-24, 0, 36);
            memset(flailLinkT, 0, sizeof flailLinkT); memset(flailLinkBy, 0, sizeof flailLinkBy);
            NetEvent st = flail_test_ev(2, 1, FLE_START, 1);      /* NP[2] accroche NP[1] */
            apply_event(&st);
            int by = flailLinkBy[1]; float t0 = flailLinkT[1];
            for (int i = 0; i < 42; i++) flail_fx_tick(TICK);
            float t1 = flailLinkT[1];
            NetEvent pu = flail_test_ev(1, 0, FLE_PULL, 1);       /* NP[1] me rafraichit sans m'avoir accroche */
            apply_event(&pu);
            fprintf(f, "fleau traction observateur : chaine de %d sur 1, bail %.2f puis %.2f apres 0.35 s ; rafraichissement sans accroche sur moi : accrochee par %d "
                       "(attendu 2, 0.30, 0.00 ; 0)\n", by - 1, t0, fmaxf(t1, 0), P.flailBy);
        }
        memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress); memset(act, 0, sizeof act);
        memset(flailLinkT, 0, sizeof flailLinkT); memset(flailLinkBy, 0, sizeof flailLinkBy);
        reset_game();
        dmMode = 0; teamSize = 0;
        memset(NP, 0, sizeof NP);
        memset(outq, 0, sizeof outq);
        botLog = savedLog; nowT = nowSave; rng = rngSave;  /* les lignes qui suivent gardent leurs horodatages et leur hasard d'avant ce bloc */
    }
    /* --- fin Yoyo : traction --- */

    /* --- Poings (solo) : couloir degage x = -20, joueur en (-20, 0, 40), vue vers -z --- */
    {
        FILE *savedLog = botLog;
        botLog = NULL;                                    /* pas de lignes du journal du bot au milieu des resultats */
        {   /* 1. cadence : appui a chaque tick, drone immobile a 1,5 m. Temps de jeu (hitstop exclu), recharges arrondies au tick */
            fist_test_reset(v3(-20, 0, 40));
            Enemy *e = flail_test_enemy(0, v3(-20, 1.6f, 38.5f), EN_DRONE);
            float tp[4] = { -1, -1, -1, -1 }, hp3 = -1;
            int n = 0, prev = fistPunches;
            gameTime = 0;
            for (int i = 0; i < 400 && gameTime < 1.5f; i++) {
                e->pos = v3(-20, 1.6f, 38.5f); e->vel = v3(0, 0, 0); e->fireT = 10;   /* enemy_tick court-circuite */
                botPress[A_MELEE] = 1;
                float t0 = gameTime;
                game_tick(TICK);
                if (fistPunches != prev) { prev = fistPunches; if (n < 4) tp[n] = t0; if (++n == 3) hp3 = e->hp; }
            }
            hitstop = 0;
            memset(botPress, 0, sizeof botPress);
            fprintf(f, "poings cadence : coups a %.3f %.3f %.3f %.3f s, PV du drone apres 3 coups %.0f "
                       "(attendu 0.000 0.225 0.450 1.158 : 0,22 / 0,44 / 1,14 arrondis au tick ; 52)\n", tp[0], tp[1], tp[2], tp[3], hp3);
            memset(E, 0, sizeof E);
        }
        {   /* 2. pause de 0,5 s apres le coup 1 : le combo revient au coup 1 (14 degats, pas 20) */
            fist_test_reset(v3(-20, 0, 40));
            Enemy *e = flail_test_enemy(0, v3(-20, 1.6f, 38.5f), EN_DRONE);
            fist_test_tick(1);
            for (int i = 0; i < 60; i++) fist_test_tick(0);
            float hp1 = e->hp;
            e->vel = v3(0, 0, 0);                         /* drone immobile : le recul du coup 1 ne compte pas dans v_rel */
            fist_test_tick(1);
            fprintf(f, "poings retour au coup 1 : etape %d, degats du 2e appui %.0f, recharge du combo %.2f (attendu 1, 14, 0.22)\n",
                    P.fistStep, hp1 - e->hp, P.meleeCd);
            memset(E, 0, sizeof E);
        }
        {   /* 3. 3 coups dans le vide : 0,70 + 0,60 - 0,06 = ~1,24 s de recharge a la fin de la fenetre du 3e coup */
            fist_test_reset(v3(-20, 0, 40));
            int i = 0;
            for (; i < 120 && P.fistStep < 3; i++) fist_test_tick(1);
            for (; i < 240 && P.fistStep == 3; i++) fist_test_tick(0);
            fprintf(f, "poings dans le vide : recharge %.2f a la fin du 3e coup, combo %d, punch-jumps %d (attendu 1.24, 0, 0)\n",
                    P.meleeCd, P.fistStep, P.fistPJ ? 0 : 1);
        }
        {   /* 4. mini-saut : pieds a 0,25 m, chute a -12, coup vers le sol -> vy 11 ; 2e coup du meme sejour : rien ; au sol : rendu */
            fist_test_reset(v3(-20, 0.25f, 40));
            P.grounded = 0; P.coyote = 0.5f; P.fistAirT = 0.5f; P.vel = v3(0, -12, 0); P.pitch = -PI / 2;
            fist_punch();
            float vy1 = P.vel.y;
            P.vel.y = -12; P.wallJumpCd = 0; P.meleeCd = 0;
            fist_punch();
            float vy2 = P.vel.y;
            int pj2 = P.fistPJ, land = 0;
            for (int i = 0; i < 360 && !land; i++) { player_tick(TICK, 0, 0); land = P.grounded; }
            fprintf(f, "poings mini-saut : vy %.1f ; 2e coup vy %.1f, charge %d ; apres l'atterrissage (%d) charge %d (attendu 11.0 ; -12.0, 0 ; 1, 1)\n",
                    vy1, vy2, pj2, land, P.fistPJ);
        }
        {   /* 5. super-saut : saut depuis le sol puis coup vers le sol au tick suivant -> refuse (vy 8,4 - 2 ticks de gravite) ;
             *    au sol, coup vers le sol -> rien */
            fist_test_reset(v3(-20, 0, 40));
            P.pitch = -PI / 2;
            player_tick(TICK, 0, 0);
            player_tick(TICK, 1, 0);
            player_tick(TICK, 0, 0);
            fist_punch();
            float vyJ = P.vel.y;
            int chJ = P.fistPJ;
            fist_test_reset(v3(-20, 0, 40));
            P.pitch = -PI / 2;
            player_tick(TICK, 0, 0);
            int g = P.grounded;
            fist_punch();
            fprintf(f, "poings super-saut : apres un saut vy %.1f, charge %d ; au sol (%d) vy %.1f, charge %d (attendu 8.0, 1 ; 1, 0.0, 1)\n",
                    vyJ, chJ, g, P.vel.y, P.fistPJ);
        }
        {   /* 6. mur a 1,5 m de l'oeil, vel (0, 0, -5) vers le mur : composante normale 11 ; wall-jump refuse 0,25 s */
            fist_test_reset(v3(-20, 3, 40));
            int nb0 = nb;
            addbox(-21, 0, 38, -19, 6, 38.5f, 0.5f, 0.5f, 0.5f);
            P.grounded = 0; P.coyote = 1; P.fistAirT = 0.5f; P.vel = v3(0, 0, -5);
            fist_punch();
            float vz = P.vel.z, cd = P.wallJumpCd;
            P.wallTouch = 1; P.wallN = v3(0, 0, 1); P.airJumps = 0;
            player_tick(TICK, 1, 0);                      /* appui de saut contre le mur, tout de suite apres */
            int wj = P.vel.y > 5;
            nb = nb0;
            fprintf(f, "poings mur : vel.z %.1f, recharge partagee %.2f, wall-jump juste apres %d (attendu 11.0, 0.25, 0)\n", vz, cd, wj);
        }
        {   /* 7. plafond horizontal : 25 m/s le long du mur, coup au mur -> |v_h| reste 25, direction tournee ;
             *    20 impulsions sur des normales qui tournent de 70 degres, depuis 5 m/s -> jamais plus de 20 */
            fist_test_reset(v3(-20, 3, 40));
            int nb0 = nb;
            addbox(-21, 0, 38, -19, 6, 38.5f, 0.5f, 0.5f, 0.5f);
            P.grounded = 0; P.fistAirT = 0.5f; P.vel = v3(25, 0, 0);
            fist_punch();
            float h1 = hlen(P.vel), ang = atan2f(P.vel.z, P.vel.x) * 180 / PI;
            nb = nb0;
            P.vel = v3(5, 0, 0);
            float hmax = 0, hfree = 5;
            for (int k = 0; k < 20; k++) {
                float a = k * 1.2217f;
                V3 n = v3(cosf(a), 0, sinf(a));
                P.fistPJ = 1; P.wallJumpCd = 0;
                hfree += FIST_PJ_PUSH;                    /* sans garde-fou : +11 a chaque mur */
                fist_pj_push(n);
                hmax = fmaxf(hmax, hlen(P.vel));
            }
            fprintf(f, "poings plafond horizontal : |v_h| %.2f tournee de %.0f deg ; 20 murs : max %.2f (sans garde-fou jusqu'a %.0f) "
                       "(attendu 25.00, ~24 ; <= 20.00)\n", h1, ang, hmax, hfree);
        }
        {   /* 8. LAN simule (je suis l'hote) : coup 1 a v_rel 0 = 14 x 0,7 = 9,8 ; uppercut : recul vertical 11 ;
             *    combo a v_rel 20 : 30,8 + 30,8 + 49 = 110,6 -> mort */
            fist_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 38.5f));
            fist_punch();
            float hp1 = NP[1].hp;
            const NetEvent *h = flail_test_last_hit();
            int w = h ? h->sub : -1;
            P.meleeCd = 0; fist_punch();
            P.meleeCd = 0; fist_punch();
            h = flail_test_last_hit();
            float ky = h ? h->f[2] : 0;
            fist_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 38.5f));
            P.vel = v3(0, 0, -20);
            for (int k = 0; k < 3; k++) { P.meleeCd = 0; fist_punch(); }
            hitstop = 0;
            fprintf(f, "poings PvP : PV apres le coup 1 %.1f, arme %d, recul vertical de l'uppercut %.1f ; combo a v_rel 20 : "
                       "cible vivante %d (attendu 90.2, %d, 11.0 ; 0)\n", hp1, w, ky, NP[1].alive, W_FIST);
        }
        {   /* 9. parry de l'hote : victime aux Poings, tolerance 0,05 s (coup d'epee 0,04 s apres sa fenetre : pare ; 0,08 s :
             *    touche ; victime a l'epee a 0,08 s : paree). Attaquant aux Poings pare : combo casse, 1 s, points rendus */
            fist_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 38.5f));
            NP[1].st.pad = MK_FIST;
            NP[1].lastParryT = nowT - 0.04;
            net_hit(1, 20, v3(0, 0, -5), W_MELEE);
            float hpA = NP[1].hp;
            NP[1].lastParryT = nowT - 0.08;
            net_hit(1, 20, v3(0, 0, -5), W_MELEE);
            float hpB = NP[1].hp;
            NP[1].hp = 100; NP[1].st.pad = MK_SWORD;
            net_hit(1, 20, v3(0, 0, -5), W_MELEE);
            float hpC = NP[1].hp;
            fist_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 38.5f));
            NP[1].lastParryT = nowT;
            int fp = fistHostParried;
            fist_punch();
            hitstop = 0;
            fprintf(f, "poings parry hote : victime aux Poings a +0,04 s PV %.0f, a +0,08 s PV %.0f ; a l'epee a +0,08 s PV %.0f ; "
                       "mon poing pare : combo %d, recharge %.2f, pares +%d, points rendus : jauges %.0f %.0f (attendu 100, 80, 100 ; 0, 1.00, +1, 0 0)\n",
                    hpA, hpB, hpC, P.fistStep, P.meleeCd, fistHostParried - fp, P.ammoGauge[WP_RAIL], P.ammoGauge[WP_ROCKET]);
        }
        {   /* 10. recul recu pendant le combo : 4 m/s (rail) ne casse rien, 10 m/s (roquette) casse le combo (0,3 s) */
            fist_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 30));
            P.fistStep = 1; P.meleeCd = 0.1f;
            NetEvent e = ev_new(EV_HIT);
            e.from = 1; e.target = (unsigned char)myId; e.sub = W_RAIL; vf(e.f + 1, v3(4, 0, 0));
            apply_event(&e);
            int s1 = P.fistStep;
            e.sub = W_ROCKET; vf(e.f + 1, v3(10, 0, 0));
            apply_event(&e);
            fprintf(f, "poings recul recu : 4 m/s -> combo %d ; 10 m/s -> combo %d, recharge %.2f (attendu 1 ; 0, 0.30)\n",
                    s1, P.fistStep, P.meleeCd);
        }
        {   /* 11. jauge (roquettes en main, 0 munition, jauge de 50) : combo de 3 touches = 25 + 25 + 0 -> 1 roquette ;
             *     seul l'uppercut touche -> +25 ; combo qui tue au 3e coup = 50 + 100 -> 3 roquettes ; parry d'une orbe pendant
             *     le 3e coup -> +50 hors plafond (2 roquettes) ; 10 s sans rien faire -> rien */
            fist_test_reset(v3(-20, 0, 40));
            dmMode = 0;
            memset(NP, 0, sizeof NP);
            P.loadout[0] = WP_ROCKET; P.loadout[1] = WP_RAIL; P.weapon = WP_ROCKET;
            P.ammo[WP_ROCKET] = 0; P.ammoGauge[WP_ROCKET] = 0; P.ammo[WP_RAIL] = 2;
            Enemy *e = flail_test_enemy(0, v3(-20, 1.6f, 38.5f), EN_DRONE);
            e->hp = 1000;
            for (int k = 0; k < 3; k++) fist_test_punch(e);
            int a1 = P.ammo[WP_ROCKET]; float g1 = P.ammoGauge[WP_ROCKET];
            for (int i = 0; i < 30 && P.fistStep; i++) fist_test_tick(0);
            e->pos = v3(-20, 1.6f, 20);                   /* coups 1 et 2 dans le vide */
            for (int k = 0; k < 2; k++) fist_test_punch(e);
            e->pos = v3(-20, 1.6f, 38.5f);
            fist_test_punch(e);
            int a2 = P.ammo[WP_ROCKET]; float g2 = P.ammoGauge[WP_ROCKET];
            for (int i = 0; i < 30 && P.fistStep; i++) fist_test_tick(0);
            P.ammo[WP_ROCKET] = 0; P.ammoGauge[WP_ROCKET] = 0; e->hp = 48;
            for (int k = 0; k < 3; k++) fist_test_punch(e);
            int a3 = P.ammo[WP_ROCKET], dead = !e->alive;
            for (int i = 0; i < 30 && P.fistStep; i++) fist_test_tick(0);
            P.ammo[WP_ROCKET] = 0; P.ammoGauge[WP_ROCKET] = 0;
            e = flail_test_enemy(0, v3(-20, 1.6f, 38.5f), EN_DRONE);
            e->hp = 1000;
            for (int k = 0; k < 2; k++) fist_test_punch(e);
            Proj *o = spawn_proj(PR_ORB, v3(-20, 1.6f, 37.5f), v3(0, 0, 16));
            fist_test_punch(e);
            int a4 = P.ammo[WP_ROCKET], par = o && o->kind == PR_PARRIED;
            memset(PR, 0, sizeof PR); memset(E, 0, sizeof E);
            float before = P.ammo[WP_ROCKET] * 1000.0f + P.ammoGauge[WP_ROCKET] + P.ammo[WP_RAIL] * 1000000.0f + P.ammoGauge[WP_RAIL];
            for (int i = 0; i < 1200; i++) fist_test_tick(0);
            float after = P.ammo[WP_ROCKET] * 1000.0f + P.ammoGauge[WP_ROCKET] + P.ammo[WP_RAIL] * 1000000.0f + P.ammoGauge[WP_RAIL];
            hitstop = 0;
            fprintf(f, "poings jauge : 3 touches -> %d roquette(s) jauge %.0f ; uppercut seul -> %d jauge %.0f ; kill au 3e coup (%d) -> %d ; "
                       "parry (%d) -> %d ; 10 s sans rien : inchange %d (attendu 1 0 ; 1 25 ; 1, 3 ; 1, 2 ; 1)\n",
                    a1, g1, a2, g2, dead, a3, par, a4, before == after);
        }
        {   /* 12. NetPState.melee : coup 2 a mi-animation -> etape 1, avancement ~32 ; swing_start au debut du coup seulement */
            fist_test_reset(v3(-20, 0, 40));
            NetPState s;
            P.fistAnim = 2; P.meleeT = FIST_ANIM * 0.5f;
            pack_state(&s);
            int stp = s.melee >> 6, adv = s.melee & 63;
            NP[1].st = s;
            int mid = swing_start(1);
            P.meleeT = FIST_ANIM - TICK;
            pack_state(&NP[1].st);
            int start = swing_start(1);
            P.meleeT = 0;
            pack_state(&NP[1].st);
            fprintf(f, "poings NetPState.melee : etape %d avancement %d, debut de coup vu %d, mi-coup vu %d, repos %d (attendu 1 32, 1, 0, 0)\n",
                    stp, adv, start, mid, NP[1].st.melee);
            memset(NP, 0, sizeof NP);
        }
        {   /* 13. bot aux Poings : cible a 1,5 m, il appuie a chaque tick : le combo complet part tout seul (3 touches) */
            fist_test_reset(v3(-20, 0, 40));
            lance_test_lan(v3(-20, 0, 38.5f));
            P.dashCharge = 0.2f;                          /* ni cross-up ni dash */
            int h0 = hitCount[W_FIST], i = 0;
            for (; i < 120 && hitCount[W_FIST] - h0 < 3; i++) {
                nowT += TICK;
                bot_think(TICK);
                game_tick(TICK);
            }
            hitstop = 0;
            fprintf(f, "bot poings : %d touches au poing en %.2f s (attendu 3, ~0.5)\n", hitCount[W_FIST] - h0, i * TICK);
            memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
            reset_game();
            dmMode = 0;
            memset(NP, 0, sizeof NP);
        }
        botLog = savedLog;
    }
    /* --- fin Poings --- */
    /* --- Egide (bouclier) --- */
    {
        double savedNow = nowT;
        {   /* 1. penalites : au sol a 12 m/s, bouclier leve, 1 s en avancant -> 7 m/s ; ni dash ni slide ; au grappin : aucune */
            egide_test_reset();
            P.shieldState = SHS_UP; P.vel = v3(0, 0, -12); act[A_FWD] = 1;
            for (int i = 0; i < 120; i++) player_tick(TICK, 0, 0);
            float h = hlen(P.vel);
            P.dashCharge = 2;
            player_tick(TICK, 0, 1);
            float dc = P.dashCharge, hd = hlen(P.vel);
            act[A_CROUCH] = 1;
            player_tick(TICK, 0, 0);
            int sl = P.sliding;
            act[A_CROUCH] = 0;
            egide_test_reset();
            P.shieldState = SHS_UP; P.vel = v3(0, 0, -12); act[A_FWD] = 1;
            P.hook = HOOK_ATTACHED; P.anchor = P.hookPos = v3(-20, 1.5f, 15); P.ropeLen = 100;
            hookPressIn = 0; hookHeldIn = 1;
            for (int i = 0; i < 30; i++) player_tick(TICK, 0, 0);
            float hh = hlen(P.vel);
            hookHeldIn = 0; act[A_FWD] = 0;
            fprintf(f, "egide penalites : vitesse %.2f, dash %.1f (vitesse %.1f), slide %d ; au grappin %.1f (attendu 7.00, 2.0 (7.0), 0 ; > 7)\n",
                    h, dc, hd, sl, hh);
        }
        {   /* 2. air-strafe 0,4 s : vue qui tourne a 2,2 rad/s (comme le test du bhop), puis strafe optimal a chaque tick */
            float gain[2][2];
            for (int m = 0; m < 2; m++)
                for (int s = 0; s < 2; s++) {
                    egide_test_reset();
                    P.shieldState = s ? SHS_UP : SHS_DOWN;
                    P.pos = P.prevPos = v3(-20, 4, 41); P.grounded = 0; P.vel = v3(0, 0, -10); act[A_RIGHT] = 1;
                    for (int i = 0; i < 48; i++) {
                        if (!m) P.yaw += 2.2f * TICK;
                        else {                            /* souhait a l'angle qui maximise le gain : vdot(vel, wish) = cap - a */
                            float v = hlen(P.vel), a = AIR_ACCEL * (s ? SHIELD_AIR_MULT : 1) * RUN_SPEED * TICK;
                            float c = clampf(fmaxf(0, AIR_CAP - a) / v, -1, 1), sn = sqrtf(1 - c * c);
                            V3 vh = v3(P.vel.x / v, 0, P.vel.z / v), w = v3(vh.x * c - vh.z * sn, 0, vh.z * c + vh.x * sn);
                            P.yaw = atan2f(w.z, w.x);             /* la droite (A_RIGHT) = (cos yaw, 0, sin yaw) */
                        }
                        player_tick(TICK, 0, 0);
                    }
                    gain[m][s] = hlen(P.vel) - 10;
                    act[A_RIGHT] = 0;
                }
            fprintf(f, "egide air-strafe : vue a 2,2 rad/s gain %.2f / %.2f (rapport %.2f) ; optimal %.2f / %.2f (rapport %.2f) "
                       "(attendu rapports < 1 ; le plafond AIR_CAP limite l'effet, voir la fiche, question 4)\n",
                    gain[0][0], gain[0][1], gain[0][1] / fmaxf(gain[0][0], 1e-3f),
                    gain[1][0], gain[1][1], gain[1][1] / fmaxf(gain[1][0], 1e-3f));
        }
        {   /* 3. tireur, cible bouclier leve de face : rail 1 -> jauge 60, rail 2 -> brise, rail 3 -> 60 de degats */
            egide_test_reset();
            egide_test_lan(v3(-20, 0, 31), 1, PI);
            P.pitch = test_pitch_to(np_center(1));
            fire_rail(); hitstop = 0;
            float hp1 = NP[1].hp, g1 = NP[1].shieldGauge;
            P.ammo[WP_RAIL] = 2; P.fireCd = 0; shotStreak = 0;
            fire_rail(); hitstop = 0;
            float g2 = NP[1].shieldGauge, b2 = NP[1].shieldBrokenT;
            P.ammo[WP_RAIL] = 2; P.fireCd = 0; shotStreak = 0;
            fire_rail(); hitstop = 0;
            fprintf(f, "egide de face : PV %.0f jauge %.0f ; jauge %.0f brise %.1f s ; PV %.0f, chaine %d (attendu 100 60 ; 0 3.0 ; 40, 1)\n",
                    hp1, g1, g2, b2, NP[1].hp, shotStreak);
        }
        {   /* 4. de flanc (bouclier tourne de 90 degres) : touche */
            egide_test_reset();
            egide_test_lan(v3(-20, 0, 31), 1, PI * 0.5f);
            P.pitch = test_pitch_to(np_center(1));
            fire_rail(); hitstop = 0;
            fprintf(f, "egide de flanc : PV %.0f, jauge %.0f (attendu 40, 120)\n", NP[1].hp, NP[1].shieldGauge);
        }
        {   /* 5. le rail s'arrete sur le bouclier : la cible derriere n'est pas touchee */
            egide_test_reset();
            egide_test_lan(v3(-20, 0, 31), 1, PI);
            NP[2].active = NP[2].alive = 1; NP[2].hp = 100; snprintf(NP[2].name, 16, "Derriere");
            NP[2].disp = v3(-20, 0, 24); vf(NP[2].st.pos, NP[2].disp);
            fire_rail(); hitstop = 0;
            fprintf(f, "egide rail arrete : PV porteur %.0f, PV derriere %.0f (attendu 100, 100)\n", NP[1].hp, NP[2].hp);
        }
        {   /* 6. parry parfait (je suis le porteur, levee a l'instant) : le rail de 1 lui revient a 60, jauge intacte */
            egide_test_reset();
            egide_test_lan(v3(-20, 0, 31), 0, 0);
            NP[0].hp = 95;
            shield_tick(TICK, 1, 1);                       /* levee : fenetre parfaite (baisse depuis 1 s) */
            host_tick(TICK);
            NetEvent e = ev_new(EV_HIT);
            e.from = 1; e.target = 0; e.sub = W_RAIL; e.f[0] = 70; e.eid = 0x02000501;
            vf(&e.f[1], v3(0, 0, 4));
            host_event_in(&e);
            hitstop = 0;
            fprintf(f, "egide parfait : PV tireur %.0f, PV porteur %.0f, jauge %.0f, parfaits %d (attendu 40, 100, 120, 1)\n",
                    NP[1].hp, NP[0].hp, NP[0].shieldGauge, shieldPerfects);
        }
        {   /* 7. revendication refusee : bouclier baisse depuis 1 s chez l'hote -> degats complets */
            egide_test_reset();
            egide_test_lan(v3(-20, 0, 31), 0, 0);
            P.shieldState = SHS_UP;
            host_tick(TICK);
            P.shieldState = SHS_DOWN; nowT += 1;
            host_tick(TICK);
            NetEvent e = ev_new(EV_SHIELD);
            e.from = 1; e.target = 0; e.sub = SHE_CLAIM; e.actor = W_RAIL; e.f[0] = 70; e.eid = 0x02000502;
            vf(&e.f[1], v3(0, 0, 1)); vf(&e.f[4], v3(0, 0, 4));
            int den = shieldDenied;
            host_event_in(&e);
            fprintf(f, "egide refus : PV %.0f, refus +%d (attendu 40, +1)\n", NP[0].hp, shieldDenied - den);
        }
        {   /* 8. conversion : EV_HIT du rail, mon bouclier leve de face hors fenetre parfaite -> bloque par l'hote */
            egide_test_reset();
            egide_test_lan(v3(-20, 0, 31), 0, 0);
            P.shieldState = SHS_UP; P.shieldDownT = 0;
            host_tick(TICK);
            NetEvent e = ev_new(EV_HIT);
            e.from = 1; e.target = 0; e.sub = W_RAIL; e.f[0] = 70; e.eid = 0x02000503;
            vf(&e.f[1], v3(0, 0, 4));
            int cv = shieldConverted;
            host_event_in(&e);
            hitstop = 0;
            fprintf(f, "egide conversion : PV %.0f, jauge hote %.0f / locale %.0f, conversions +%d (attendu 100, 60 / 60, +1)\n",
                    NP[0].hp, NP[0].shieldGauge, P.shieldGauge, shieldConverted - cv);
        }
        {   /* 9. souffle a 1 m : de face 50 % bloques ; de dos et sous les pieds rien */
            float loss[3];
            static const float cz[3] = { 32, 30, 31 }, cy[3] = { 0.9f, 0.9f, -0.2f };
            for (int s = 0; s < 3; s++) {
                egide_test_reset();
                egide_test_lan(v3(-20, 0, 31), 1, PI);
                explode(v3(-20, cy[s], cz[s]), ROCKET_RADIUS, ROCKET_DMG, 1);
                hitstop = 0;
                loss[s] = 100 - NP[1].hp;
            }
            fprintf(f, "egide souffle : perte de face %.1f, de dos %.1f, sous les pieds %.1f (attendu ~33, 60, 60)\n", loss[0], loss[1], loss[2]);
        }
        {   /* 10. coup de bouclier (tap) : cible a 1,5 m, v_rel 10 -> (25 + 30) x 0,7 = 38,5 ; dash +1 */
            egide_test_reset();
            egide_test_lan(v3(-20, 0, 39), 0, 0);
            NP[1].st.pad = 0;
            P.vel = v3(0, 0, -10); P.dashCharge = 0.5f;
            shield_tick(TICK, 1, 0);                       /* appui et relache dans le meme tick */
            hitstop = 0;
            fprintf(f, "egide coup de bouclier : PV %.1f, dash %.1f, recharge %.2f (attendu 61.5, 1.5, 0.50)\n",
                    NP[1].hp, P.dashCharge, P.meleeCd);
        }
        lance_test_lan(v3(-20, 0, 31));                    /* fin du LAN simule */
        memset(NP, 0, sizeof NP);
        {   /* 11. regeneration (solo) : jauge vide -> brise 3 s, puis 0 -> 120 en 4 s */
            egide_test_reset();
            dmMode = 0;
            P.shieldState = SHS_UP; P.shieldGauge = 0;
            shield_tick(TICK, 0, 1);
            int st0 = P.shieldState;
            for (int i = 0; i < 360; i++) shield_tick(TICK, 0, 0);
            int st3 = P.shieldState; float g3 = P.shieldGauge;
            for (int i = 0; i < 480; i++) shield_tick(TICK, 0, 0);
            fprintf(f, "egide regeneration : etat %d ; a 3 s etat %d jauge %.1f ; a 7 s jauge %.1f (attendu 2 ; 0, < 0.5 ; 120.0)\n",
                    st0, st3, g3, P.shieldGauge);
        }
        {   /* 12. orbe (solo) : de face sur le bouclier (12 sur la jauge), de dos dans le corps */
            float hp[2], g[2];
            for (int s = 0; s < 2; s++) {
                egide_test_reset();
                dmMode = 0;
                P.shieldState = SHS_UP; P.yaw = s ? PI : 0;
                spawn_proj(PR_ORB, v3(-20, 1.7f, 37), v3(0, 0, 16));
                for (int i = 0; i < 60; i++) proj_tick(TICK);
                hp[s] = P.hp; g[s] = P.shieldGauge;
            }
            fprintf(f, "egide orbe : de face PV %.0f jauge %.0f ; de dos PV %.0f (attendu 100 108 ; 88)\n", hp[0], g[0], hp[1]);
        }
        {   /* 13. arrache par le grappin adverse : levee impossible pendant 1 s */
            egide_test_reset();
            egide_test_lan(v3(-20, 0, 31), 0, 0);
            NetEvent e = ev_new(EV_SHIELD);
            e.from = 1; e.target = 0; e.sub = SHE_RIP; e.eid = 0x02000504;
            host_event_in(&e);
            shield_tick(TICK, 1, 1);
            int up0 = P.shieldState == SHS_UP;
            for (int i = 0; i < 120; i++) shield_tick(TICK, 0, 0);
            shield_tick(TICK, 1, 1);
            fprintf(f, "egide arrache : levee pendant %d, apres 1 s %d, arrache chez l'hote %.2f s (attendu 0, 1, 1.00)\n",
                    up0, P.shieldState == SHS_UP, NP[0].shieldRipT);
        }
        {   /* 14. Tesla de face : une tranche de 0,2 s (2 impulsions, 9,8) bloquee, pas de chaine vers la cible voisine */
            egide_test_reset();
            P.meleeKind = MK_SWORD;
            egide_test_lan(v3(-20, 0, 31), 1, PI);
            NP[2].active = NP[2].alive = 1; NP[2].hp = 100; snprintf(NP[2].name, 16, "Voisin");
            NP[2].disp = v3(-17, 0, 31); vf(NP[2].st.pos, NP[2].disp);
            P.loadout[0] = WP_TESLA; P.loadout[1] = WP_RAIL; P.weapon = WP_TESLA; P.ammo[WP_TESLA] = 60;
            for (int i = 0; i < 25; i++) { NP[1].lastShieldT = nowT; tesla_test_run(1, i < 24); }   /* ST_SHIELD recu a chaque tick */
            fprintf(f, "egide tesla : PV porteur %.0f, jauge %.1f, PV voisin %.0f (attendu 100, 110.2, 100)\n",
                    NP[1].hp, NP[1].shieldGauge, NP[2].hp);
        }
        {   /* 15. belier : bouclier leve au grappin sur un joueur -> coup de bouclier gratuit (W_SHIELD), rebond */
            egide_test_reset();
            egide_test_lan(v3(-20, 0, 33), 0, 0);
            NP[1].st.pad = 0;
            P.shieldState = SHS_UP; P.shieldDownT = 0;
            P.hook = HOOK_ATTACHED; P.hookPlayer = 2; P.anchor = P.hookPos = np_center(1); P.ropeLen = 10;
            hookHeldIn = 1;
            int h0 = hitCount[W_SHIELD], i = 0;
            for (; i < 120 && P.hook == HOOK_ATTACHED; i++) {
                nowT += TICK;
                player_tick(TICK, 0, 0);
                shield_tick(TICK, 0, 1);
            }
            hookHeldIn = 0; hitstop = 0;
            fprintf(f, "egide belier : touches bouclier +%d, PV cible %.0f, grappin %d, vy %.1f (attendu +1, < 70, 0, > 5)\n",
                    hitCount[W_SHIELD] - h0, NP[1].hp, P.hook, P.vel.y);
        }
        lance_test_lan(v3(-20, 0, 31));
        memset(NP, 0, sizeof NP);
        memset(act, 0, sizeof act);
        reset_game();
        dmMode = 0; hitstop = 0;
        nowT = savedNow;
    }
    /* --- fin Egide --- */

    /* reglages : sauvegarde puis rechargement de velocity.ini */
    {
        int savedBot = botMode;
        botMode = 0;                                      /* les bots ne sauvegardent jamais : on simule un joueur */
        snprintf(cfgPath, sizeof cfgPath, "cfg_test.ini");
        binds[A_SKILL][1] = K_X2; sens = 0.003f; fovBase = 115; invertY = 1;
        cfg_save();
        memcpy(binds, bindDefault, sizeof binds); sens = 0.0022f; fovBase = 105; invertY = 0;
        cfg_load();
        fprintf(f, "reglages : piece -> %d (attendu %d), sens %.4f, fov %.0f, axe Y inverse %d (attendu 0.0030, 115, 1)%s",
                binds[A_SKILL][1], K_X2, sens, fovBase, invertY, "\n");
        {                                                 /* ancien velocity.ini : regles= a 6 champs (specialisations) et spec= */
            MatchRules sR = wantRules;
            FILE *g = fopen(cfgPath, "w");
            if (g) { fputs("regles=2,0,1,1,1,5\nspec=3\n", g); fclose(g); }
            wantRules = (MatchRules){ MODE_FRAGS, 1, AMMO_NORMAL, 0, 10 };
            cfg_load();
            MatchRules o = wantRules;
            wantRules = (MatchRules){ MODE_ZONES, 1, AMMO_DOUBLE, 0, 15 };
            cfg_save();
            wantRules = sR;
            cfg_load();
            fprintf(f, "reglages : ancien regles= (6 champs) -> mode %d, grappin %d, munitions %d, boutique %d, %d min ; nouveau -> "
                       "mode %d, %d min (attendu 2, 0, 1, 1, 5 ; %d, 15)\n", o.mode, o.hook, o.ammo, o.shop, o.minutes,
                    wantRules.mode, wantRules.minutes, MODE_ZONES);
            wantRules = sR;
        }
        remove(cfgPath);
        cfgPath[0] = 0;
        botMode = savedBot;
        memcpy(binds, bindDefault, sizeof binds); sens = 0.0022f; fovBase = 105; invertY = 0;
    }

    /* contre-jeu : je tire dans la piece de l'adversaire, le ricochet le touche lui */
    reset_game();
    dmMode = 1;
    memset(NP, 0, sizeof NP);
    NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
    NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Lanceur");
    NP[1].disp = v3(-20, 0, 24); vf(NP[1].st.pos, NP[1].disp);
    CO[0].alive = 1; CO[0].owner = 1; CO[0].life = 3; CO[0].netId = 0x02000001;
    CO[0].pos = v3(-18, 4, 28);
    P.pos = P.prevPos = v3(-20, 0, 41);
    {
        V3 d = vsub(CO[0].pos, eye_pos());
        P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
    }
    fire_rail();
    fprintf(f, "contre-jeu : piece adverse detruite=%d, PV du lanceur=%.0f (attendu 1, %.0f), message \"%s\"\n",
            !CO[0].alive, NP[1].hp, 100 - fminf(RAIL_DMG * PVP_SCALE, PVP_HIT_CAP), popText);

    /* Tirs a la tete et chaine de tirs pleine (LAN, je suis l'hote) : rail a la tete = one-shot qui passe le plafond, au
     * corps = plafond ; la tete avance la chaine de 2 (miroir de l'hote aussi) ; pleine, le rail suivant tue ; l'hote refuse
     * une chaine ou une tete qu'il ne peut pas confirmer */
    {
        reset_game();
        dmMode = 1;
        memset(NP, 0, sizeof NP);
        memset(hostChain, 0, sizeof hostChain); memset(hostChainSaid, 0, sizeof hostChainSaid);
        NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
        NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible");
        NP[1].disp = v3(-20, 0, 30); vf(NP[1].st.pos, NP[1].disp);
        P.pos = P.prevPos = v3(-20, 0, 41); shotStreak = 0;
        host_tick(TICK);
#define AIM_AT(pt) do { V3 dd_ = vsub((pt), eye_pos()); P.yaw = atan2f(dd_.x, -dd_.z); P.pitch = asinf(dd_.y / vlen(dd_)); } while (0)
#define CIBLE_NEUVE() do { NP[1].alive = 1; NP[1].hp = 100; } while (0)
        int hr0 = headRefused, cr0 = chainRefused;
        AIM_AT(np_head(1));
        fire_rail();
        int deadHead = !NP[1].alive, kfHead = kfeed[0].info, cHead = shotStreak, mHead = hostChain[0];
        CIBLE_NEUVE(); AIM_AT(np_center(1));
        fire_rail();
        float hpBody = NP[1].hp;
        fprintf(f, "tete : rail a la tete mort=%d, killfeed tete+one-shot=%d ; au corps PV=%.0f (attendu 1, 1, %.0f)\n",
                deadHead, kfHead == (KF_HEAD | KF_ONESHOT), hpBody, 100 - PVP_HIT_CAP);
        fprintf(f, "chaine de tirs : apres une tete %d (miroir hote %d), puis un corps %d (attendu %d, %d, %d)\n",
                cHead, mHead, shotStreak, CHAIN_HEAD_STEP, CHAIN_HEAD_STEP, CHAIN_HEAD_STEP + 1);
        shotStreak = hostChain[0] = hostChainSaid[0] = STREAK_MAX - 1;   /* comme apres 7 touches */
        CIBLE_NEUVE(); fire_rail();                       /* 8e touche : chaine pleine */
        int cFull = shotStreak;
        host_tick(TICK);                                  /* l'hote voit ma chaine pleine */
        CIBLE_NEUVE(); fire_rail();                       /* au corps, chaine pleine : one-shot */
        fprintf(f, "chaine pleine (%d) : tir au corps mort=%d, killfeed one-shot=%d, chaine apres %d (attendu %d, 1, 1, 0)\n",
                cFull, !NP[1].alive, (kfeed[0].info & KF_ONESHOT) != 0, shotStreak, STREAK_MAX);
        shotStreak = STREAK_MAX; hostChain[0] = 0;        /* chaine "pleine" que l'hote n'a jamais vue se remplir */
        host_tick(TICK);
        CIBLE_NEUVE(); fire_rail();
        fprintf(f, "chaine pleine non confirmee par l'hote : PV=%.0f, refus %d (attendu %.0f, 1)\n",
                NP[1].hp, chainRefused - cr0, 100 - PVP_HIT_CAP);
        hostChain[0] = hostChainSaid[0] = STREAK_MAX; shotStreak = 0;   /* un tir rate : la chaine annoncee retombe */
        host_tick(TICK);
        fprintf(f, "chaine retombee a 0 chez le tireur : miroir de l'hote %d (attendu 0)\n", hostChain[0]);
        /* Elan a la tete : x1,35 sans plafond, pas de one-shot ; a l'arret 20 PvE, a 15 m/s 95 PvE */
        CIBLE_NEUVE(); shotStreak = 0; P.vel = v3(0, 0, 0); P.kinDashT = 10; AIM_AT(np_head(1));
        P.ammo[WP_KINETIC] = 3;
        fire_kinetic();
        float hpKin0 = NP[1].hp;
        CIBLE_NEUVE(); shotStreak = 0; P.vel = v3(0, 0, -KIN_SPEED_CAP); P.kinDashT = 10; P.kinPreDash = KIN_SPEED_CAP;
        fire_kinetic();
        fprintf(f, "elan a la tete : a l'arret PV=%.1f, a 15 m/s PV=%.1f, vivant=%d (attendu %.1f, %.1f, 1 : au-dela du plafond, sans tuer)\n",
                hpKin0, NP[1].hp, NP[1].alive, 100 - KIN_DMG_BASE * PVP_SCALE * HEAD_MULT_KIN,
                100 - (KIN_DMG_BASE + KIN_DMG_PER_MS * KIN_SPEED_CAP) * PVP_SCALE * HEAD_MULT_KIN);
        P.vel = v3(0, 0, 0);
        /* l'hote arbitre une tete declaree par un client : point sur ma tete = one-shot, point aux pieds = refuse */
        nowT += 1; host_tick(TICK);
        NetEvent e = ev_new(EV_HIT);
        e.from = 1; e.target = 0; e.sub = W_RAIL; e.f[0] = RAIL_DMG * PVP_SCALE * HEAD_MULT_RAIL; e.pad[0] = HF_HEAD;
        e.eid = 0x02F00001; vf(e.f + 4, v3(P.pos.x, P.pos.y + 0.1f, P.pos.z));   /* "tete" aux pieds */
        host_event_in(&e);
        float hpForged = NP[0].hp;
        NP[0].hp = 100; e.eid++; vf(e.f + 4, np_head(0)); e.pad[0] = HF_HEAD;   /* host_handle a retire le drapeau refuse */
        host_event_in(&e);
        fprintf(f, "hote : tete declaree aux pieds PV=%.0f (refus %d), tete sur la tete mort=%d (attendu %.0f, 1, 1)\n",
                hpForged, headRefused - hr0, !NP[0].alive, 100 - PVP_HIT_CAP);
        /* hauteur de la tete : debout et accroupi */
        V3 hs = np_head(1);
        NP[1].st.flags = NF_CROUCH;
        V3 hc = np_head(1);
        NP[1].st.flags = 0;
        fprintf(f, "tete : centre debout %.2f m, accroupi %.2f m au-dessus des pieds (attendu %.2f, %.2f)\n",
                hs.y - NP[1].disp.y, hc.y - NP[1].disp.y, H_STAND - HEAD_DROP, H_CROUCH - HEAD_DROP);
#undef AIM_AT
#undef CIBLE_NEUVE
    }
    /* solo : point faible au sommet d'un drone (x1,5 au rail) ; visee des bots */
    {
        reset_game();
        memset(E, 0, sizeof E);
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 3, 30);
        P.pos = P.prevPos = v3(-20, 0, 41); shotStreak = 0;
        V3 top = vadd(E[0].pos, v3(0, enemy_radius(EN_DRONE) * 0.8f, 0)), d0 = vsub(top, eye_pos());
        P.yaw = atan2f(d0.x, -d0.z); P.pitch = asinf(d0.y / vlen(d0));
        fire_rail();
        float hpTop = E[0].hp;
        shotStreak = 0; E[0].hp = 1000;
        d0 = vsub(vadd(E[0].pos, v3(0, -0.3f, 0)), eye_pos());
        P.yaw = atan2f(d0.x, -d0.z); P.pitch = asinf(d0.y / vlen(d0));
        fire_rail();
        fprintf(f, "drone : rail au point faible %.0f, au centre %.0f degats (attendu %.0f, %.0f)\n",
                1000 - hpTop, 1000 - E[0].hp, RAIL_DMG * HEAD_MULT_RAIL, RAIL_DMG);
        /* chaine pleine : un drone meurt d'un coup, un boss des VAGUES prend une touche normale */
        int deadE[2]; float hpE[2];
        for (int k = 0; k < 2; k++) {
            E[0].alive = 1; E[0].type = k ? EN_PYRA : EN_DRONE; E[0].hp = 5000; shotStreak = STREAK_MAX;
            d0 = vsub(E[0].pos, eye_pos());
            P.yaw = atan2f(d0.x, -d0.z); P.pitch = asinf(d0.y / vlen(d0));
            P.ammo[WP_RAIL] = 2; fire_rail();
            deadE[k] = !E[0].alive; hpE[k] = E[0].hp;
        }
        fprintf(f, "chaine pleine contre les ennemis : drone mort=%d, boss mort=%d (PV %.0f), chaine apres %d (attendu 1, 0, < 5000, 0)\n",
                deadE[0], deadE[1], hpE[1], shotStreak);
        int heads = 0;
        int lv0 = botLevel;
        botLevel = 1;
        for (int i = 0; i < 20000; i++) { float ox, oy; if (bot_roll_aim(10, &ox, &oy) && oy > 0.4f) heads++; }
        botLevel = lv0;
        fprintf(f, "bots (normal, 10 m) : tirs vises a la tete %.1f %% (attendu ~%.1f)\n", heads / 200.0f,
                100 * botHitP[1] * botHeadP[1] * clampf(1.15f - 10 / 80.0f, 0.5f, 1));
    }

    /* en ligne : analyse STUN, lignes du lobby, adresses */
    {
        static const unsigned char stunOk[32] = { 0x01, 0x01, 0, 12, 0x21, 0x12, 0xA4, 0x42, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                                  0x00, 0x20, 0, 8, 0, 1, 0xF5, 0x23, 0x73, 0x13, 0xA6, 0x41 };
        struct sockaddr_in m, a;
        char t[40] = "?";
        int ok = stun_parse(stunOk, 32, &m);
        if (ok) ep_text(&m, t, sizeof t);
        PkHead ph;
        memset(&ph, 0, sizeof ph);
        ph.magic = NET_MAGIC; ph.type = PK_SNAP;
        fprintf(f, "STUN : reponse -> %s, paquet de jeu pris pour du STUN=%d (attendu 82.1.2.3:54321 et 0)\n", t,
                stun_parse((const unsigned char *)&ph, 20, &m));
        fprintf(f, "adresses : valide=%d, sans port=%d, octet 300=%d (attendu 1, 0, 0)\n", parse_ep("10.0.0.7:27960", &a),
                parse_ep("10.0.0.7", &a), parse_ep("10.0.0.300:5", &a));

        nFound = 0;
        lb_line("room abc123 3 8 1 5.6.7.8:27960 Bob");
        int okRoom = nFound == 1 && found[0].online && found[0].players == 3 && found[0].maxp == 8 && found[0].spec == 1 &&
                     !strcmp(found[0].id, "abc123") && !strcmp(found[0].name, "Bob") && ntohs(found[0].addr.sin_port) == 27960;
        lb_line("room abc123 4 8 1 5.6.7.8:27960 Bob");
        int updated = nFound == 1 && found[0].players == 4;
        lb_line("room zzz999 1 8 0 9.9.9.9:27960 Eve");
        lb_line("gone abc123");
        fprintf(f, "lobby : salon lu=%d, mise a jour sans doublon=%d, apres gone: %d salon(s), reste %s (attendu 1, 1, 1, Eve)\n",
                okRoom, updated, nFound, nFound ? found[0].name : "-");
        nFound = 0;

        netMode = NET_HOST; myId = 0;                     /* spectateurs : des places a part, sans toucher aux joueurs */
        memset(NP, 0, sizeof NP); memset(SP, 0, sizeof SP);
        NP[0].active = 1;
        int accepted = 0;
        for (int k = 0; k < MAXSPEC + 2; k++) {
            PkJoin pj;
            struct sockaddr_in from;
            memset(&pj, 0, sizeof pj);
            memset(&from, 0, sizeof from);
            from.sin_family = AF_INET; from.sin_port = htons((unsigned short)(40000 + k)); from.sin_addr.s_addr = htonl(0x0A000001);
            snprintf(pj.name, sizeof pj.name, "spec%d", k);
            pj.spec = 1;
            host_on_join(&pj, &from);
            accepted = spec_count();
            if (k == 0) host_on_join(&pj, &from);         /* le meme spectateur qui reessaie ne prend pas une 2e place */
        }
        fprintf(f, "spectateurs : %d acceptes sur %d tentatives (max %d), joueurs actifs=%d (attendu %d, %d, %d, 1)\n", accepted,
                MAXSPEC + 2, MAXSPEC, NP[1].active + NP[0].active, MAXSPEC, MAXSPEC + 2, MAXSPEC);
        memset(SP, 0, sizeof SP);
        memset(NP, 0, sizeof NP);
    }

    {                                                     /* visee des bots : tirs cadres selon le niveau, sinon ils frolent */
        int lv0 = botLevel;
        for (int lv = 0; lv < 3; lv++) {
            int hits = 0, nearMiss = 0;
            botLevel = lv;
            for (int k = 0; k < 2000; k++) {
                float ox, oy;
                if (bot_roll_aim(20, &ox, &oy)) hits++;
                else if (fabsf(ox) > 0.45f && fabsf(ox) < 1.5f) nearMiss++;
            }
            fprintf(f, "visee bot niveau %d a 20 m : cadres %.0f%%, rates qui frolent (0,45 a 1,5 m) %.0f%% des rates (attendu ~%.0f%%, 100%%)\n",
                    lv, hits / 20.0f, 100.0f * nearMiss / fmaxf(1, (float)(2000 - hits)), botHitP[lv] * 90);
        }
        botLevel = lv0;
    }
    {                                                     /* chute hors de la carte : mort (solo) */
        reset_game();
        dmMode = 0; netMode = NET_OFF;
        P.pos = P.prevPos = v3(ms(70), 30, 0);              /* au-dessus du vide, derriere le mur est */
        int i = 0;
        for (; i < 600 && !P.dead; i++) game_tick(TICK);
        fprintf(f, "chute hors de la carte : mort=%d apres %.1f s, y=%.0f, PV=%.0f (attendu 1, ~2.4, < -25, 0)\n", P.dead, i * TICK, P.pos.y, P.hp);
        reset_game();
        fprintf(f, "chute : apres respawn, plus marque=%d (attendu 0)\n", P.pos.y > FALL_DEATH_Y + 5 ? 0 : 1);
    }
    {                                                     /* skin : voyage dans l'etat du joueur (NetPState), borne a l'affichage */
        int ws = wantSkin;
        NetPState st;
        memset(&st, 0, sizeof st);
        wantSkin = SKN_CHAT;
        pack_state(&st);
        fprintf(f, "skin : envoye=%d (%s), taille NetPState=%d, 5 skins=%d (attendu %d, CHAT, 52, 5)\n", st.skin,
                skinNames[st.skin < SKN_COUNT ? st.skin : 0], (int)sizeof st, SKN_COUNT, SKN_CHAT);
        wantSkin = ws;
    }
    {                                                     /* defauts adoucis (HUD leger, ombres lisibles, bloom et soleil discrets) */
        fprintf(f, "graphismes defauts : hud_fonds=%.2f occlusion_min=%.2f ciel=%.2f soleil_force=%.1f bloom=%.1f/%.0f "
                   "soleil_taille=%.0f/%.0f/%.0f halo=%.2f reflets=%.2f (attendu 0.22 0.45 0.64 1.2 0.5/4 6/16/50 0.16 0.25)\n",
                gxHudPanel, gxAoMin, skyCol[0], gxSunPow, gxBloom[0], gxBloom[1], gxSunSize[0], gxSunSize[1], gxSunSize[2],
                gxSunGlow[0], gxFlare);
    }
    {                                                     /* PARAMETRES <-> GRAPHISMES : ECHAP revient d'un ecran a la fois */
        int so = settingsOpen, sp = settingsPage, wa = waitAct, uf = uiFocus;
        settingsOpen = 1; settingsPage = 1; waitAct = -1; uiFocus = 0;
        in_key_down(SC_ESC, 0);
        int p1 = settingsPage, o1 = settingsOpen;
        in_key_down(SC_ESC, 0);
        fprintf(f, "menus : ECHAP depuis GRAPHISMES -> page %d ouvert %d, puis -> ouvert %d (attendu 0 1, 0)\n", p1, o1, settingsOpen);
        keys[SC_ESC] = 0; latch[SC_ESC] = 0;
        settingsOpen = so; settingsPage = sp; waitAct = wa; uiFocus = uf;
    }
    {                                                     /* graphismes/rendu.ini : lecture d'une ligne, cle inconnue, bornes */
        float d0 = gxSunDir[1], f0 = gxFog[1], b0 = gxBloom[1], a0 = gxAoRays, bf0 = gxBloom[0];
        char l1[] = "  soleil_direction = 0.1 0.9 -0.2   # commentaire", l2[] = "cle_inconnue = 4", l3[] = "bloom = 0.5 9";
        char l4[] = "occlusion_rayons = 3";
        int k1 = gfx_parse_line(l1), k2 = gfx_parse_line(l2), k3 = gfx_parse_line(l3);
        gfx_parse_line(l4);
        gfx_clamp();
        fprintf(f, "graphismes : soleil y=%.1f, cle inconnue=%d, bloom seuil borne=%.0f, rayons=%d, lue=%d%d (attendu 0.9, 0, 4, 3, 11)\n",
                gxSunDir[1], k2, gxBloom[1], AO_RAYS, k1, k3);
        gxSunDir[0] = 0.55f; gxSunDir[1] = d0; gxSunDir[2] = -0.82f; gxFog[1] = f0; gxBloom[0] = bf0; gxBloom[1] = b0; gxAoRays = a0;
    }
    {                                                     /* eclairage precalcule : ombre portee de la tour, soleil */
        float lit[3], sh[3];
        lm_grid_build();
        lm_light(v3(ms(6), 4, -ms(6)), v3(0, 1, 0), 1, lit);    /* plateforme centrale, cote soleil */
        lm_light(v3(-4, 4, 6), v3(0, 1, 0), 1, sh);             /* meme plateforme, derriere la tour */
        fprintf(f, "eclairage : sol au soleil %.2f, a l'ombre de la tour %.2f, soleil visible %.0f / %.0f (attendu > 1.5x, 1 / 0)\n",
                lit[1], sh[1], sun_vis(v3(ms(6), 4.1f, -ms(6))), sun_vis(v3(-4, 4.1f, 6)));
    }
    {                                                     /* equipes 2v2 : arrivee equilibree, pas de tir allie, victoire d'equipe */
        int nm0 = netMode, dm0 = dmMode, id0 = myId;
        netMode = NET_HOST; dmMode = 1; myId = 0; matchOver = 0; teamSize = 2; fragLimit = team_frag_limit(2);
        memset(NP, 0, sizeof NP);
        NP[0].active = NP[0].alive = 1; NP[0].hp = 100; NP[0].team = 0; snprintf(NP[0].name, 16, "Hote");
        for (int k = 0; k < 4; k++) {                     /* 3 places libres, la 4e arrivee est refusee */
            PkJoin pj;
            struct sockaddr_in from;
            memset(&pj, 0, sizeof pj); memset(&from, 0, sizeof from);
            from.sin_family = AF_INET; from.sin_port = htons((unsigned short)(41000 + k)); from.sin_addr.s_addr = htonl(0x0A000002);
            snprintf(pj.name, sizeof pj.name, "J%d", k + 1);
            host_on_join(&pj, &from);
        }
        int act = 0;
        for (int i = 0; i < MAXP; i++) act += NP[i].active;
        fprintf(f, "equipes arrivee : actifs=%d equipes %d%d%d%d, bleus=%d rouges=%d (attendu 4, 0101, 2, 2)\n", act,
                NP[0].team, NP[1].team, NP[2].team, NP[3].team, team_count(0), team_count(1));
        for (int i = 0; i < 4; i++) { NP[i].alive = 1; NP[i].hp = 100; NP[i].disp = v3(-20, 0, 20.0f + 8 * i); }
        NetEvent e;
        memset(&e, 0, sizeof e);
        e.kind = EV_HIT; e.from = 2; e.target = 0; e.sub = W_RAIL; e.f[0] = 50;
        int cancel = host_handle(&e);
        float hpAlly = NP[0].hp;
        e.from = 1;
        host_handle(&e);
        float hpEnemy = NP[0].hp;
        e.from = 0; e.sub = W_SELF; e.f[0] = 10;
        host_handle(&e);
        fprintf(f, "equipes tir allie : annule=%d PV=%.0f ; tir ennemi PV=%.0f ; degats sur soi PV=%.0f (attendu 1, 100, 50, 40)\n",
                cancel, hpAlly, hpEnemy, NP[0].hp);
        fprintf(f, "equipes cibles : allie cible=%d dessine=%d, ennemi cible=%d (attendu 0, 1, 1)\n", np_target(2), np_present(2), np_target(1));
        V3 sb = pick_spawn(2), sr = pick_spawn(3);
        fprintf(f, "equipes spawns : bleu z=%.0f, rouge z=%.0f (attendu < 0, > 0)\n", sb.z, sr.z);
        load_map(2);                                      /* autre carte : chaque camp sur sa falaise (axe x du Canyon) */
        sb = pick_spawn(2); sr = pick_spawn(3);
        fprintf(f, "equipes spawns %s : bleu x=%.0f, rouge x=%.0f (attendu < 0, > 0)\n", maps[2].name, sb.x, sr.x);
        load_map(0);
        NP[0].kills = 10; NP[2].kills = 9; NP[1].kills = 12;   /* aucun joueur n'a 20, mais l'equipe bleue y arrive */
        host_kill(2, 1, W_RAIL);
        fprintf(f, "equipes victoire : limite=%d score bleu=%d rouge=%d fin=%d gagnant=%d (attendu 20, 20, 12, 1, 0)\n",
                fragLimit, team_score(0), team_score(1), matchOver, matchWinner);
        PkSnap sn;                                         /* le mode et les equipes voyagent dans le snapshot */
        memset(&sn, 0, sizeof sn);
        sn.teamSize = 3; sn.fragLimit = 25;
        for (int i = 0; i < 4; i++) { sn.p[i].active = 1; sn.p[i].team = (unsigned char)(i & 1 ? 0 : 1); }
        myId = 7;
        apply_snap(&sn);
        fprintf(f, "equipes snapshot : mode=%d equipes %d%d%d%d (attendu 3, 1010)\n", teamSize, NP[0].team, NP[1].team, NP[2].team, NP[3].team);
        memset(NP, 0, sizeof NP);                         /* un depart laisse 3 contre 1 : reequilibre au match suivant */
        teamSize = 2; myId = 0;
        for (int i = 0; i < 4; i++) { NP[i].active = 1; NP[i].team = (unsigned char)(i == 3); }
        host_balance_teams();
        fprintf(f, "equipes reequilibrage : bleus=%d rouges=%d (attendu 2, 2)\n", team_count(0), team_count(1));
        memset(NP, 0, sizeof NP);
        teamSize = 0; matchOver = 0; fragLimit = FRAG_LIMIT;
        netMode = nm0; dmMode = dm0; myId = id0;
    }

    {                                                     /* un morceau par carte : chacun joue, en stereo, sans silence ni NaN */
        char mb[512]; int n = 0, okN = 0;
        for (int t = 0; t < NTRACKS; t++) {
            double sum = 0, side = 0; int bad = 0;
            float l, r;
            musSeed = 3; music_set(t, 4);
            for (int i = 0; i < 8 * SR; i++) { music_frame(&l, &r); if (l != l || r != r) bad = 1; sum += l * l + r * r; side += fabsf(l - r); }
            float rms = (float)sqrt(sum / (16.0 * SR)), st = (float)(side / (8.0 * SR));
            int ok = !bad && rms > 0.15f && rms < 0.8f && st > 0.005f;
            okN += ok;
            n += snprintf(mb + n, sizeof mb - n, "%s%s %.2f", t ? ", " : "", tracks[t].name, rms);
        }
        music_set(-1, 0);
        int mapsOk = 1;
        for (int m = 0; m < NMAPS; m++) mapsOk &= maps[m].music >= 0 && maps[m].music < NTRACKS;
        fprintf(f, "musiques : %d morceaux (%s), corrects %d, chaque carte a le sien %d (attendu %d, rms 0,15 a 0,8, %d, 1)\n",
                NTRACKS, mb, okN, mapsOk, NTRACKS, NTRACKS);
    }
    {                                                     /* stereo : le lead et les hats ne sont pas identiques a gauche et a droite */
        float l, r, diff = 0, tot = 0;
        musSeed = 1; music_set(0, 5);
        for (int i = 0; i < 20 * SR; i++) { music_frame(&l, &r); diff += fabsf(l - r); tot += fabsf(l) + fabsf(r); }
        music_set(-1, 0);
        fprintf(f, "musique stereo : ecart gauche/droite = %.0f %% du signal (attendu entre 5 et 80)\n", 100 * 2 * diff / (tot + 1e-9f));
    }
    for (int lv = 0; lv <= 4; lv += 4) {                  /* musique : 4 s de kicks (niveau 0 = 172 BPM, niveau 4 = 188 BPM), signal borne */
        int kicks = 0, bad = 0; float peak = 0, prevK = 0;
        music_set(0, lv);
        for (int i = 0; i < 4 * SR - 100; i++) {
            float s = music_sample();
            if (!(fabsf(s) < 8)) bad = 1;
            peak = fmaxf(peak, fabsf(s));
            if (mus.kEnv >= 0.9996f && prevK < 0.9996f) kicks++;
            prevK = mus.kEnv;
        }
        music_set(-1, 0);
        fprintf(f, "musique niveau %d : kicks en 4 s=%d pic=%.2f valeurs invalides=%d (attendu %d, pic entre 0.5 et 3, 0)\n",
                lv, kicks, peak, bad, lv ? 13 : 12);
    }
    {                                                     /* le niveau demande ne s'applique qu'a la mesure suivante (1,4 s a 172 BPM) */
        music_set(0, 0);
        for (int i = 0; i < SR / 2; i++) music_sample();
        music_level(8);
        int before = mus.lvl;
        for (int i = 0; i < 2 * SR; i++) music_sample();
        fprintf(f, "musique changement de niveau : avant la mesure=%d apres=%d tempo=%.0f BPM (attendu 0, 8, 204)\n",
                before, mus.lvl, SR * 60.0f / 4 / mus.sps);
        music_set(-1, 0);
    }
    {                                                     /* la limite de frags voyage dans PkSnap (partie rapide = 7) */
        PkSnap sn;
        memset(&sn, 0, sizeof sn);
        sn.fragLimit = FRAG_LIMIT_QUICK; apply_snap(&sn); int recu = fragLimit;
        sn.fragLimit = 0; apply_snap(&sn);
        {                                                     /* diversite : 40 s au niveau max (acid, riser, crash), graine, cellules utilisees */
        int bad = 0; float peak = 0;
        double sum[3] = { 0 };
        music_set(0, MUS_LVMAX);
        for (int i = 0; i < 40 * SR; i++) {
            float s = music_sample();
            if (!(fabsf(s) < 8)) bad = 1;
            peak = fmaxf(peak, fabsf(s));
        }
        for (int run = 0; run < 3; run++) {               /* graines 1, 1, 2 : les deux premieres sont identiques, la 3e differe */
            musSeed = run == 2 ? 2 : 1;
            music_set(0, 5);
            for (int i = 0; i < 15 * SR; i++) sum[run] += (double)music_sample() * (1 + (i & 7));
        }
        musSeed = 1;
        music_set(-1, 0);
        int used = 0;
        for (int c = 0; c < MUS_CELLS; c++) {
            int in = 0;
            for (int p = 0; p < tracks[0].nsong; p++) if (tracks[0].song[p] == c || tracks[0].songHi[p] == c) in = 1;
            used += in;
        }
        fprintf(f, "musique diversite : niveau max 40 s pic=%.2f invalides=%d, cellules utilisees=%d, meme graine identique=%d, autre graine differente=%d "
                   "(attendu pic entre 0.5 et 3, 0, >= 10, 1, 1)\n",
                peak, bad, used, sum[0] == sum[1], sum[0] != sum[2]);
    }
    fprintf(f, "limite de frags : recue=%d, snapshot sans limite=%d, taille PkSnap=%d (attendu %d, %d)\n",
                recu, fragLimit, (int)sizeof sn, FRAG_LIMIT_QUICK, FRAG_LIMIT);
        memset(NP, 0, sizeof NP);
    }
    fprintf(f, "musique PvP (limite 15) : 0-0=%d 4-2=%d 5-1=%d 9-9=%d 10-3=%d 14-13=%d 14-14=%d (attendu 0 0 3 3 6 6 8)\n",
            music_pvp_level(0, 0, 15), music_pvp_level(4, 2, 15), music_pvp_level(5, 1, 15), music_pvp_level(9, 9, 15),
            music_pvp_level(10, 3, 15), music_pvp_level(14, 13, 15), music_pvp_level(14, 14, 15));
    fprintf(f, "musique PvP (limite 7) : 2-0=%d 3-0=%d 5-2=%d 6-6=%d (attendu 0 3 6 8)\n",
            music_pvp_level(2, 0, 7), music_pvp_level(3, 0, 7), music_pvp_level(5, 2, 7), music_pvp_level(6, 6, 7));

    {   /* mise a jour : lecture de version.txt, comparaison des builds, SHA-256, chemins voisins de l'exe */
        UpdInfo in, bad;
        Sha256 sh;
        char h1[65], h2[65], h3[65], nw[64], old[64];
        int ok = upd_parse("build=52\r\nsha=1A2B3C4\nwin_size=1834496\nwin_sha256=9F86D081884C7D659A2FEAA0C55AD015A3BF4F1B2B0B822CD15D6C15B0F00A08\n"
                           "mac_size=2310144\nmac_sha256=60303ae22b998861bce3b28f33eec1be758a213c86c93c076dbe9f558c11c752\n", &in);
        int nok = upd_parse("bonjour\n<html>404</html>\n", &bad);
        sha_init(&sh); sha_final(&sh, h1);
        sha_init(&sh); sha_update(&sh, "abc", 3); sha_final(&sh, h2);
        sha_init(&sh); sha_update(&sh, "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56); sha_final(&sh, h3);
        upd_paths("C:/jeu/velocity.exe", nw, old, sizeof nw);
        fprintf(f, "mise a jour : lu=%d build=%d sha=%s win=%ld mac=%ld hash_min=%d illisible=%d (attendu 1 52 1a2b3c4 1834496 2310144 1 0)\n",
                ok, in.build, in.sha, in.winSize, in.macSize, !strcmp(in.winHash, "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08"), nok);
        fprintf(f, "mise a jour : plus recente que build %d=%d, que build 52=%d, que build 60=%d (attendu %d 0 0)\n",
                VELOCITY_BUILD, in.build > VELOCITY_BUILD, in.build > 52, in.build > 60, 52 > VELOCITY_BUILD);
        fprintf(f, "sha256 : vide=%d abc=%d 2 blocs=%d (attendu 1 1 1)\n",
                !strcmp(h1, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
                !strcmp(h2, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
                !strcmp(h3, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
        fprintf(f, "mise a jour : chemins %s | %s (attendu C:/jeu/velocity.exe.new | C:/jeu/velocity.exe.old)\n", nw, old);
    }

    {   /* --- Cartes (MapDef) : limites, departs, pads, kill zone solo et LAN, bande d'acceleration, prudence des bots --- */
        double savedNow = nowT;
        netMode = NET_OFF; dmMode = 0; connected = 0; spectating = 0; matchOver = 0;
        for (int m = 0; m < NMAPS; m++) {
            const MapDef *md = &maps[m];
            load_map(m);
            int freeN = 0, aboveN = 0, groundN = 0, cellBad = 0, padN = 0, padOk = 0, nbox = nb, ncell = npu;
            for (int k = 0; k < md->nspawns; k++) {
                V3 s = v3(md->spawns[k][0], md->spawns[k][1], md->spawns[k][2]);
                freeN += pos_free(s, H_STAND);
                aboveN += s.y > md->killY + 1;
                groundN += ray_world(v3(s.x, s.y + 0.1f, s.z), v3(0, -1, 0), 0.6f, 0) < 0.6f;
            }
            for (int i = 0; i < npu; i++)                 /* une cellule n'est pas dans un solide */
                for (int b = 0; b < nb; b++) {
                    V3 c = PU[i].pos, lo = boxes[b].mn, hi = boxes[b].mx;
                    if (boxes[b].kind == B_SOLID && c.x > lo.x && c.x < hi.x && c.y > lo.y && c.y < hi.y && c.z > lo.z && c.z < hi.z) cellBad++;
                }
            for (int i = 0; m > 0 && i < nbox; i++) {     /* chaque pad retombe sur un sol, et on y reste (l'Arene a ses tests) */
                if (boxes[i].kind != B_PAD) continue;
                padN++;
                reset_game(); hitstop = 0;
                memset(act, 0, sizeof act); memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
                V3 c = vlerp(boxes[i].mn, boxes[i].mx, 0.5f);
                P.pos = P.prevPos = v3(c.x, boxes[i].mx.y + 0.01f, c.z); P.vel = v3(0, 0, 0);
                int landed = -1;
                for (int t = 0; t < 960 && P.pos.y > md->killY; t++) {
                    player_tick(TICK, 0, 0);
                    if (landed < 0 && P.grounded && boxes[P.groundBox].kind != B_PAD) landed = t;
                    if (landed >= 0 && t > landed + 120) break;   /* 1 s apres l'atterrissage, sans toucher a rien */
                }
                padOk += landed >= 0 && P.grounded && P.pos.y > md->killY;
            }
            fprintf(f, "carte %s : %d boites (< %d), %d cellules (<= %d), departs libres %d, au-dessus du vide %d, au sol %d, "
                       "cellules dans un solide %d, pads qui retombent sur un sol %d/%d (attendu %d %d %d, 0, %d/%d)\n",
                    md->name, nbox, MAXB, ncell, MAXPICK, freeN, aboveN, groundN, cellBad, padOk, padN,
                    md->nspawns, md->nspawns, md->nspawns, padN, padN);
        }

        load_map(1);                                      /* kill zone solo : sous killY -> mort au tick suivant */
        reset_game(); hitstop = 0;
        memset(act, 0, sizeof act); memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
        P.pos = P.prevPos = v3(17.8f, 0, 17.8f);
        game_tick(TICK);
        int aliveOn = !P.dead;
        P.pos = P.prevPos = v3(0, maps[1].killY - 0.5f, 0); P.vel = v3(0, -20, 0);
        game_tick(TICK);
        fprintf(f, "kill zone solo (%s, killY %.0f) : vivant sur la plateforme %d, mort sous le plan %d, PV %.0f (attendu 1 1 0)\n",
                maps[1].name, maps[1].killY, aliveOn, P.dead, P.hp);
        for (int i = 0; i < 300; i++) game_tick(TICK);    /* le corps s'arrete 6 m sous le plan */
        fprintf(f, "kill zone : corps arrete a y=%.1f (attendu %.1f)\n", P.pos.y, maps[1].killY - 6);

        /* LAN simule (moi = hote 0, Cible = 1) : touche puis chute -> kill a l'attaquant ; chute sans touche -> suicide */
        reset_game(); hitstop = 0;
        netMode = NET_HOST; connected = 1; myId = 0; dmMode = 1; matchOver = 0;
        memset(NP, 0, sizeof NP);
        for (int i = 0; i < 2; i++) { NP[i].active = NP[i].alive = 1; NP[i].hp = 100; NP[i].lastHitBy = -1; }
        snprintf(NP[0].name, 16, "Moi"); snprintf(NP[1].name, 16, "Cible");
        P.pos = P.prevPos = v3(17.8f, 0, 17.8f); NP[0].disp = P.pos; NP[1].disp = v3(0, 6, 18.5f);
        nowT += 10;
        int c0 = voidCredited, s0 = voidSuicides, kills[4], deaths[4];
        NetEvent hit = ev_new(EV_HIT), fall = ev_new(EV_HIT);
        hit.from = 0; hit.target = 1; hit.sub = W_RAIL; hit.f[0] = 30;
        fall.from = 1; fall.target = 1; fall.sub = W_VOID; fall.f[0] = 999;
        host_handle(&hit); nowT += 1.5; host_handle(&fall);           /* 1. touche, chute 1,5 s apres : kill a Moi */
        kills[0] = NP[0].kills; deaths[0] = NP[1].deaths;
        host_respawn(1); nowT += 1; host_handle(&fall);               /* 2. chute sans touche : suicide */
        kills[1] = NP[0].kills; deaths[1] = NP[1].deaths;
        host_respawn(1); host_handle(&hit); nowT += VOID_CREDIT_T + 1; host_handle(&fall);   /* 3. touche trop ancienne : suicide */
        kills[2] = NP[0].kills; deaths[2] = NP[1].deaths;
        fprintf(f, "kill zone LAN : touche + chute -> kills %d morts %d ; chute seule -> kills %d morts %d ; touche il y a 5 s -> kills %d morts %d ; "
                   "creditees %d suicides %d (attendu 1 1 ; 1 2 ; 1 3 ; 1 2)\n", kills[0], deaths[0], kills[1], deaths[1], kills[2], deaths[2],
                voidCredited - c0, voidSuicides - s0);
        P.pos = P.prevPos = v3(0, maps[1].killY - 0.5f, 0); P.vel = v3(0, -20, 0);   /* 4. l'hote lui-meme tombe : EV_HIT W_VOID */
        nowT += 1;
        game_tick(TICK);
        fprintf(f, "kill zone LAN (hote) : mort %d, morts de Moi %d, suicides %d (attendu 1 1 3)\n", P.dead, NP[0].deaths, voidSuicides - s0);
        netMode = NET_OFF; connected = 0; dmMode = 0; memset(NP, 0, sizeof NP); memset(outq, 0, sizeof outq);

        {   /* bots : depuis la 1re plateforme (y 0), un saut vers le vide est detecte, un saut sur place non (depuis la 8e,
             * le saut vers le centre retombe maintenant sur un balcon) */
            V3 s = v3(17.8f, 0, 17.8f), s8 = v3(18.5f, 42, 0);
            fprintf(f, "bot prudent (%s) : saut sur place retombe %d, saut a 15 m/s vers l'ouest retombe %d, depuis la 8e vers le "
                       "centre (balcon) %d (attendu 1 0 1)\n", maps[1].name,
                    bot_lands(s, v3(0, JUMP_V, 0), maps[1].killY), bot_lands(s, v3(-15, JUMP_V, 0), maps[1].killY),
                    bot_lands(s8, v3(-15, JUMP_V, 0), maps[1].killY));
        }

        {   /* --- Course (LA TOILE) : portes libres, passage dans l'ordre, chute = retour a la porte, arrivee, record --- */
            int rm = race_map(), freeN = 0, outN = 0;
            race_start();
            const MapDef *md = &maps[curMap];
            for (int i = 0; i < md->ngates; i++) {        /* chaque porte : le centre et 8 points a mi-rayon sont libres */
                V3 c = race_gate(i);
                int ok = pos_free(v3(c.x, c.y - 0.9f, c.z), 1.8f);
                for (int s = 0; s < 8 && ok; s++) {
                    float a = s * PI / 4, r = md->gates[i][3] * 0.5f;
                    ok = pos_free(v3(c.x + cosf(a) * r, c.y - 0.9f + sinf(a) * r, c.z), 1.8f) || pos_free(v3(c.x, c.y - 0.9f + sinf(a) * r, c.z + cosf(a) * r), 1.8f);
                }
                freeN += ok;
                outN += fabsf(c.x) > 78 || fabsf(c.z) > 78;
            }
            int lineN = 0;                                /* ligne droite libre d'une porte a la suivante (depuis le depart pour la 1re) */
            for (int i = 0; i < md->ngates; i++) lineN += los(i ? race_gate(i - 1) : vadd(md->start, v3(0, 1, 0)), race_gate(i));
            raceCount = 0; raceBest = 0; ghostBestN = 0;
            float skipped = 0;
            P.prevPos = P.pos = vsub(race_gate(1), v3(0, 0.9f, 0));   /* la porte 2 avant la 1 : ne compte pas */
            race_tick(0.5f);
            skipped = (float)raceGate;
            for (int i = 0; i < md->ngates; i++) {
                V3 c = vsub(race_gate(i), v3(0, 0.9f, 0)), prev = i ? vsub(race_gate(i - 1), v3(0, 0.9f, 0)) : md->start;
                V3 d = vnorm(vsub(c, prev));
                P.prevPos = vsub(c, vmul(d, 3)); P.pos = vadd(c, vmul(d, 0.5f));
                race_tick(0.5f);
                if (i == 4) {                             /* chute apres la 5e porte : retour a la porte 5 */
                    P.pos = P.prevPos = v3(0, md->killY - 1, 0);
                    void_check();
                }
            }
            V3 back = P.pos;
            fprintf(f, "course (%s) : %d portes, libres %d, lignes droites libres %d, hors carte %d, porte sautee comptee %.0f, arrivee %d en %.1f s, "
                       "chutes %d, record %.1f, fantome %d positions (attendu %d, %d, %d, 0, 0, 1, %.1f, 1, %.1f, > 0)\n",
                    maps[rm].name, md->ngates, freeN, lineN, outN, skipped, raceDone, raceT, raceFalls, raceBest, ghostCurN,
                    md->ngates, md->ngates, md->ngates, 0.5f * (md->ngates + 1), 0.5f * (md->ngates + 1));
            (void)back;
            raceOn = 0;
            load_map(0); reset_game();
        }
        {   /* --- Course jouee par un pilote simple : regarde la porte (un peu au-dessus), grappin des que l'aimant trouve un
             * point de balancement, lache une fois le point depasse, avance tout le temps. Mesure : arrivee, temps, chutes --- */
            FILE *logSave = botLog; botLog = NULL;
            race_start(); appState = APP_GAME; hitstop = 0;
            memset(act, 0, sizeof act); memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
            raceBest = 0; ghostBestN = 0;
            int hooks = 0;
            float t = 0;
            for (; t < 150 && !raceDone; t += TICK) {
                memset(botHeld, 0, sizeof botHeld);
                const MapDef *md = &maps[curMap];
                V3 g = race_gate(raceGate < md->ngates ? raceGate : md->ngates - 1), e = eye_pos(), d = vsub(g, e);
                float hd = fmaxf(hlen(d), 0.1f);
                P.yaw = atan2f(d.x, -d.z);
                P.pitch = clampf(atan2f(d.y, hd) + 0.45f, -1.2f, 1.2f);
                botHeld[A_FWD] = 1;
                if (P.grounded) botPress[A_JUMP] = 1;
                if (P.hook == HOOK_NONE && P.hookCd <= 0 && raceCount <= 0) {
                    V3 at;
                    if (swing_target(e, view_dir(), &at) >= 0) { botPress[A_HOOK] = 1; botHeld[A_HOOK] = 1; hooks++; }
                } else if (P.hook != HOOK_NONE) {
                    V3 toA = vsub(P.anchor, P.pos), fwd = vmul(v3(d.x, 0, d.z), 1 / hd);
                    float past = -vdot(v3(toA.x, 0, toA.z), fwd);   /* metres depuis le passage sous le point */
                    int passed = P.hook == HOOK_ATTACHED && ((past > 1 && P.vel.y > 2) || past > 9);   /* on lache dans la remontee */
                    botHeld[A_HOOK] = !passed;
                }
                for (int i = 0; i < MAXE; i++) E[i].alive = 0;
                game_tick(TICK);
                if (getenv("RACEDBG") && fmodf(t, 0.5f) < TICK)
                    fprintf(f, "  [%.1f] porte %d pos %.1f %.1f %.1f v %.1f crochet %d ancre %.0f %.0f %.0f sol %d\n", t, raceGate, P.pos.x, P.pos.y,
                            P.pos.z, vlen(P.vel), P.hook, P.anchor.x, P.anchor.y, P.anchor.z, P.grounded);
            }
            botLog = logSave;
            fprintf(f, "course pilotee (%s) : arrivee %d, portes %d / %d, temps %.1f s, chutes %d, grappins %d (attendu 1, %d / %d, < 120 s)\n",
                    maps[curMap].name, raceDone, raceGate, maps[curMap].ngates, t, raceFalls, hooks, maps[curMap].ngates, maps[curMap].ngates);
            raceOn = 0; appState = APP_MENU;
            load_map(0); reset_game();
        }
        {   /* --- Cartes a cibles : portes et cibles libres et visibles, porte verrouillee tant que ses cibles vivent, pilote tireur --- */
            for (int m = 0; m < NMAPS; m++) {
                if (!maps[m].challenge || maps[m].ntargets <= 0) continue;
                race_start_map(m);
                const MapDef *md = &maps[m];
                int solidN = 0, seenN = 0, gateTot = 0, noTarget = 0, aliveN = 0, typeOk = 0;
                for (int k = 0; k < md->ntargets; k++) {
                    int g = (int)md->targets[k][4];
                    float R = md->targets[k][3] > 0.5f ? 0.4f : 0.8f, per = md->targets[k][8];
                    for (int s = 0; s < 8; s++) {         /* la cible ne traverse aucun solide sur toute sa course */
                        V3 p = race_target_at(md->targets[k], k, per > 0 ? per * s / 8 : 0);
                        for (int b = 0; b < nb; b++)
                            solidN += boxes[b].kind == B_SOLID && p.x > boxes[b].mn.x - R && p.x < boxes[b].mx.x + R &&
                                      p.y > boxes[b].mn.y - R && p.y < boxes[b].mx.y + R && p.z > boxes[b].mn.z - R && p.z < boxes[b].mx.z + R;
                    }
                    V3 from = g ? race_gate(g - 1) : vadd(md->start, v3(0, 1, 0));   /* visible depuis la porte d'avant */
                    seenN += los(from, race_target_at(md->targets[k], k, 0));
                }
                for (int g = 0; g < md->ngates; g++) { gateTot += race_targets_total(g); noTarget += race_targets_total(g) == 0; }
                for (int i = 0; i < MAXE; i++) { aliveN += race_is_target(i); typeOk += race_is_target(i) && E[i].hp == 1 && enemy_radius(E[i].type) > 0.3f; }
                /* porte verrouillee : on la traverse cibles debout -> elle ne compte pas ; cibles abattues -> elle compte */
                raceCount = 0;
                V3 c = vsub(race_gate(0), v3(0, 0.9f, 0)), prev = md->start, d = vnorm(vsub(c, prev));
                P.prevPos = vsub(c, vmul(d, 3)); P.pos = vadd(c, vmul(d, 0.5f));
                race_tick(0.5f);
                int lockedStay = raceGate == 0, left0 = race_targets_left(0);
                for (int i = 0; i < MAXE; i++) if (race_is_target(i) && E[i].phase == 0) damage_enemy(i, 10, v3(0, 0, 0));
                P.prevPos = vsub(c, vmul(d, 3)); P.pos = vadd(c, vmul(d, 0.5f));
                race_tick(0.5f);
                int opened = raceGate == 1;
                fprintf(f, "cibles (%s) : %d cibles (%d/%d vivantes, 1 PV %d), dans un solide %d, visibles depuis la porte d'avant %d, portes sans cible %d, "
                           "porte 1 verrouillee (%d cibles) %d puis ouverte %d (attendu %d, %d/%d, %d, 0, %d, 0, 1 puis 1)\n",
                        md->name, md->ntargets, aliveN, md->ntargets, typeOk, solidN, seenN, noTarget, left0, lockedStay, opened,
                        md->ntargets, md->ntargets, md->ntargets, md->ntargets, md->ntargets);
                (void)gateTot;
                int hooks, falls;
                float tm = test_race_pilot(m, &hooks, &falls);
                int left = 0;
                for (int i = 0; i < MAXE; i++) left += race_is_target(i);
                fprintf(f, "cibles pilotees (%s) : arrivee %d, portes %d / %d, temps %.1f s, chutes %d, grappins %d, cibles restantes %d (attendu 1, %d / %d, < 150 s, 0)\n",
                        md->name, tm >= 0, raceGate, md->ngates, tm >= 0 ? tm : 150.0f, falls, hooks, left, md->ngates, md->ngates);
                raceOn = 0; appState = APP_MENU;
                load_map(0); reset_game();
            }
        }
        {   /* --- Speedrun : categories, planchers (recopies dans worker/src/speedrun.js), ticket puis rapport, classement,
             * speedrun complet (enchainement des cartes CHALLENGE, chrono global, record perso) --- */
            char k0[40], k1[40], k2[40];
            int rm = race_map(), so = onlineOn, savedBot = botMode;
            char sid[33], skey[33];
            FILE *logSave = botLog; botLog = NULL;
            botMode = 0;                                  /* les bots n'envoient pas de temps : on simule un joueur */
            memcpy(sid, rkId, 33); memcpy(skey, rkKey, 33);
            sr_key(SR_MAP0 + rm, k0, sizeof k0); sr_key(SR_COMPLET, k1, sizeof k1); sr_key(SR_VAGUES100, k2, sizeof k2);
            fprintf(f, "speedrun categories : %s, %s, %s, carte de partie refusee %d (attendu course:la_toile, complet:3, vagues100, 1)\n",
                    k0, k1, k2, !sr_cat_ok(SR_MAP0 + 0));
            fprintf(f, "speedrun planchers : course %s %d ms, complet %d ms, vague 100 %d ms (attendu 6975, 11722, 300000 : "
                       "les memes que worker/src/speedrun.js)\n", maps[rm].name, sr_floor_ms(SR_MAP0 + rm), sr_floor_ms(SR_COMPLET),
                    sr_floor_ms(SR_VAGUES100));
            for (int m = 0; m < NMAPS; m++)
                if (m != rm && maps[m].challenge && maps[m].ngates > 0)
                    fprintf(f, "speedrun plancher %s : %d ms (a recopier dans worker/src/speedrun.js)\n", maps[m].name, sr_floor_ms(SR_MAP0 + m));
            /* ticket demande au depart ; temps fini avant le ticket : il part a son arrivee, avec le ticket */
            onlineOn = 1; snprintf(rkId, 33, "%032x", 7); snprintf(rkKey, 33, "%032x", 9);
            rkQN = 0;
            int cat = SR_MAP0 + rm, sp[3] = { 9000, 21000, 40000 };
            speedrun_begin(cat);
            int q1 = rkQN, k1ok = rkQN && rkQ[0].kind == RQ_SR_START && strstr(rkQ[0].body, "\"cat\":\"course:la_toile\"");
            speedrun_begin(cat);                          /* R : le ticket en attente est remplace, pas double */
            int q2 = rkQN, arg = rkQ[0].arg;
            speedrun_report_splits(cat, 40000, sp, 3);
            int waitSt = srRun[cat].state, q3 = rkQN;
            snprintf(rkResp, sizeof rkResp, "{\"ok\":true,\"run\":\"00112233445566aa\",\"cat\":\"course:la_toile\"}");
            sr_on_response(RQ_SR_START, arg - 256, 1);    /* ticket d'un depart remplace : ignore */
            int stale = rkQN;
            sr_on_response(RQ_SR_START, arg, 1);
            const char *rb = rkQN > 1 ? rkQ[1].body : "";
            fprintf(f, "speedrun ticket : demande %d (%d), une seule apres R %d, temps en attente du ticket %d sans requete %d, "
                       "ancien ticket ignore %d, rapport parti %d avec ticket %d temps %d splits %d (attendu 1 1 1 1 1 1 1 1 1 1)\n",
                    q1, k1ok, q2 == 1, waitSt == SRR_WAIT, q3 == 1, stale == 1, rkQN == 2 && rkQ[1].kind == RQ_SR_REPORT,
                    strstr(rb, "\"run\":\"00112233445566aa\"") != NULL, strstr(rb, "\"ms\":40000") != NULL,
                    strstr(rb, "\"splits\":[9000,21000,40000]") != NULL);
            snprintf(rkResp, sizeof rkResp, "{\"ok\":true,\"state\":\"record\",\"best\":40000,\"prev\":41200,\"rank\":3}");
            sr_on_response(RQ_SR_REPORT, cat, 1);
            int recSt = srRun[cat].state, recRank = srRun[cat].rank, topAsked = rkQN == 3 && rkQ[2].kind == RQ_SR_TOP;
            snprintf(rkResp, sizeof rkResp, "{\"ok\":false,\"err\":\"sous le plancher\"}");
            sr_on_response(RQ_SR_REPORT, cat, 1);
            fprintf(f, "speedrun reponse : record %d rang %d, classement redemande %d, refus %d (%s) (attendu 1 3 1 1, sous le plancher)\n",
                    recSt == SRR_RECORD, recRank, topAsked, srRun[cat].state == SRR_REFUSED, srRun[cat].err);
            onlineOn = 0;
            speedrun_begin(cat); speedrun_report(cat, 40000);
            fprintf(f, "speedrun hors ligne : aucune requete %d, etat hors ligne %d (attendu 1 1)\n", rkQN == 3, srRun[cat].state == SRR_OFFLINE);
            sr_parse_top(cat, "{\"ok\":true,\"cat\":\"course:la_toile\",\"runners\":37,\"top\":[{\"name\":\"Tia\",\"ms\":38500},"
                              "{\"name\":\"Sam\",\"ms\":40000}],\"wr\":{\"name\":\"Tia\",\"ms\":38500,\"splits\":[1000,2000,38500]},"
                              "\"me\":{\"rank\":2,\"ms\":40000,\"splits\":[9000,21000,40000]}}");
            SrTop *t = &srTop[cat];
            fprintf(f, "speedrun top : %d temps, 1er %s %d, %d coureurs, record %s %d ms en %d splits, moi #%d %d ms %d splits "
                       "(attendu 2, Tia 38500, 37, Tia 38500 en 3, #2 40000 3)\n", t->n, t->name[0], t->ms[0], t->runners, t->wrName,
                    t->wrMs, t->wrN, t->meRank, t->meMs, t->meN);
            memset(t, 0, sizeof *t); memset(srRun, 0, sizeof srRun); memset(srTicket, 0, sizeof srTicket);
            rkQN = 0;
#ifdef _WIN32
#define SR_SLEEP(ms) Sleep(ms)
#else
#define SR_SLEEP(ms) usleep((useconds_t)(ms) * 1000)
#endif
            if (getenv("SPEEDRUN_LOBBY")) {               /* de bout en bout avec un lobby local (cd worker && node dev-server.js 8787),
                                                           * SPEEDRUN_LOBBY=127.0.0.1:8787 : ticket, 7,5 s de vrai temps, rapport, top */
                char sLobby[96];
                snprintf(sLobby, sizeof sLobby, "%s", lobbyAddr); snprintf(lobbyAddr, sizeof lobbyAddr, "%s", getenv("SPEEDRUN_LOBBY"));
                onlineOn = 1; rkId[0] = rkKey[0] = 0; rk_identity();
                int fl = sr_floor_ms(cat), lsp[RACE_GATES], ngt = maps[rm].ngates;
                for (int i = 0; i < ngt; i++) lsp[i] = (fl + 300) * (i + 1) / ngt;
                speedrun_begin(cat);
                for (int k = 0; k < 100 && srTicket[cat] == SRT_WAIT; k++) { SR_SLEEP(50); rk_net_poll(); }
                int gotTicket = srTicket[cat] == SRT_OK;
                speedrun_report_splits(cat, fl + 300, lsp, ngt);   /* trop tot : refuse (plus rapide que le temps reel) */
                for (int k = 0; k < 100 && srRun[cat].state == SRR_SENT; k++) { SR_SLEEP(50); rk_net_poll(); }
                int early = srRun[cat].state == SRR_REFUSED;
                char earlyErr[40];
                snprintf(earlyErr, sizeof earlyErr, "%s", srRun[cat].err);
                speedrun_begin(cat);
                for (int k = 0; k < 100 && srTicket[cat] == SRT_WAIT; k++) { SR_SLEEP(50); rk_net_poll(); }
                SR_SLEEP(fl + 600);                          /* on "court" vraiment */
                speedrun_report_splits(cat, fl + 300, lsp, ngt);
                for (int k = 0; k < 100 && (srRun[cat].state == SRR_SENT || rkTh || rkQN); k++) { SR_SLEEP(50); rk_net_poll(); }
                int rec = srRun[cat].state == SRR_RECORD, rank = srRun[cat].rank;
                for (int k = 0; k < 100 && srTop[cat].state != 2; k++) { SR_SLEEP(50); rk_net_poll(); }
                fprintf(f, "speedrun de bout en bout (%s) : ticket %d, temps trop tot refuse %d (%s), apres %d ms record %d rang %d, "
                           "classement lu %d, mon temps %d ms (attendu 1, 1 (plus rapide que le temps reel), 1, 1, 1, %d)\n",
                        lobbyAddr, gotTicket, early, earlyErr, fl + 600, rec, rank, srTop[cat].state == 2, srTop[cat].meMs, fl + 300);
                snprintf(lobbyAddr, sizeof lobbyAddr, "%s", sLobby);
                memset(srTop, 0, sizeof srTop); memset(srRun, 0, sizeof srRun); memset(srTicket, 0, sizeof srTicket); rkQN = 0;
            }
            if (getenv("DISCORD_LOBBY")) {                /* connexion Discord de bout en bout avec le faux Discord du lobby local
                                                           * (cd worker && node dev-server.js 8787), DISCORD_LOBBY=127.0.0.1:8787 */
                char sLobby[96], sName[16], sDc[33], page[2048], path[96], id0[33], key0[33];
                int sMm = myMmr, sPl = rankPlayed, sWi = rankWins, uf = uiFocus;
                snprintf(sLobby, sizeof sLobby, "%s", lobbyAddr); snprintf(lobbyAddr, sizeof lobbyAddr, "%s", getenv("DISCORD_LOBBY"));
                memcpy(sName, myName, 16); memcpy(sDc, rkDiscord, 33);
                onlineOn = 1; uiFocus = 0; rkId[0] = rkKey[0] = 0; rk_identity(); rkDiscord[0] = 0; dcState = DC_IDLE;
                memcpy(id0, rkId, 33); memcpy(key0, rkKey, 33);
                snprintf(myName, sizeof myName, "Avant");
                int waiting = 0, nav = 0, linked = 0, same = 0, ownKey = 0;
                for (int dev = 0; dev < 2; dev++) {       /* 1er PC, puis un 2e PC (identite neuve) avec le meme compte Discord */
                    if (dev) { rkId[0] = rkKey[0] = 0; rk_identity(); rkDiscord[0] = 0; snprintf(myName, sizeof myName, "Autre"); }
                    dc_begin();
                    for (int k = 0; k < 100 && dcState == DC_START; k++) { SR_SLEEP(50); rk_net_poll(); }
                    waiting += dcState == DC_WAITING;
                    snprintf(path, sizeof path, "/auth/discord/go?s=%s", dcStateHex);   /* le navigateur : redirections suivies */
                    nav += http_request(lobbyAddr, path, NULL, page, sizeof page) && strstr(page, "Connecte : testeur") != NULL;
                    for (int k = 0; k < 200 && dcState == DC_WAITING; k++) { SR_SLEEP(20); nowT += 0.05; rk_net_poll(); }
                    if (!dev) {
                        linked = dcState == DC_IDLE && !strcmp(rkId, id0) && !strcmp(rkDiscord, "testeur");
                        snprintf(myName, sizeof myName, "Apres");   /* pseudo change : le serveur le note pour ce compte */
                    } else {
                        same = !strcmp(rkId, id0);
                        ownKey = hex_ok(rkKey, 32) && strcmp(rkKey, key0) != 0;
                    }
                    for (int k = 0; k < 100 && (k < 5 || rkTh || rkQN || rkHelloBusy); k++) { SR_SLEEP(20); nowT += 0.05; rk_net_poll(); }
                }
                fprintf(f, "discord de bout en bout (%s) : attente navigateur %d, page %d, lie %d, 2e PC meme identite %d, cle propre %d, "
                           "pseudo du compte %s (attendu 2, 2, 1, 1, 1, Apres)\n", lobbyAddr, waiting, nav, linked, same, ownKey, myName);
                snprintf(lobbyAddr, sizeof lobbyAddr, "%s", sLobby);
                memcpy(myName, sName, 16); memcpy(rkDiscord, sDc, 33);
                myMmr = sMm; rankPlayed = sPl; rankWins = sWi; uiFocus = uf; dcState = DC_IDLE; rkQN = 0; rkNameSent[0] = 0;
            }
#undef SR_SLEEP
            onlineOn = so; memcpy(rkId, sid, 33); memcpy(rkKey, skey, 33);
            botMode = savedBot; botLog = logSave;

            /* speedrun complet sur deux cartes (la meme deux fois : une seule carte CHALLENGE a ce jour) : 0,5 s par porte */
            sr_maps_init();
            int saveN = srMapN, save0 = srMaps[0], ng = maps[rm].ngates;
            srMapN = 2; srMaps[0] = srMaps[1] = rm; srPbN = 0; srPbLoaded = 1;
            sr_full_start(); appState = APP_GAME;
            int ms1 = 0, idxMid = -1, doneMid = -1, idxNext = -1, cdNext = -1, doneEnd = 0, totEnd = 0, split0 = 0, nb = 0, pb1 = 0;
            int restart = 0, nb2 = 0, d0 = 0, d1 = 0, pb2 = 0;
            for (int run = 0; run < 2; run++) {           /* 2e run, 0,4 s par porte : record battu, ecarts a l'ancien record */
                float per = run ? 0.4f : 0.5f;
                for (int lap = 0; lap < 2; lap++) {
                    const MapDef *md = &maps[curMap];
                    raceCount = 0;
                    for (int i = 0; i < md->ngates; i++) {
                        V3 c = vsub(race_gate(i), v3(0, 0.9f, 0)), prev = i ? vsub(race_gate(i - 1), v3(0, 0.9f, 0)) : md->start;
                        V3 d = vnorm(vsub(c, prev));
                        P.prevPos = vsub(c, vmul(d, 3)); P.pos = vadd(c, vmul(d, 0.5f));
                        race_tick(per);
                    }
                    if (!lap) {
                        if (!run) { ms1 = sr_full_ms(); idxMid = srIdx; doneMid = srDone; }
                        sr_full_tick(SR_NEXT_DELAY * 0.5f);
                        sr_full_tick(SR_NEXT_DELAY * 0.5f + 0.01f);   /* carte suivante : nouveau depart, decompte */
                        if (!run) { idxNext = srIdx; cdNext = raceCount > 0 && !raceDone && raceT == 0; }
                    }
                }
                if (!run) {
                    doneEnd = srDone; totEnd = sr_full_ms(); split0 = srSplit[0]; nb = srNewBest; pb1 = srPb[1];
                    race_start();                         /* R : tout le speedrun depuis la premiere carte */
                    restart = srIdx == 0 && !srDone && srFull;
                } else { nb2 = srNewBest; d0 = sr_full_delta(0); d1 = sr_full_delta(1); pb2 = srPb[1]; }
            }
            fprintf(f, "speedrun complet : carte 1 en %d ms (carte %d, fini %d), puis carte %d avec decompte %d, fini %d en %d ms, "
                       "temps a la carte 1 %d, record perso %d (%d), [R] repart de la carte 1 : %d (attendu %d (0, 0), 1, 1, 1 en %d, %d, 1 (%d), 1)\n",
                    ms1, idxMid, doneMid, idxNext, cdNext, doneEnd, totEnd, split0, nb, pb1, restart,
                    ng * 500, ng * 1000, ng * 500, ng * 1000);
            fprintf(f, "speedrun complet, 2e run plus rapide : record battu %d, ecarts a l'ancien record %d et %d ms, nouveau record %d "
                       "(attendu 2, %d et %d, %d)\n", nb2, d0, d1, pb2, -ng * 100, -ng * 200, ng * 800);
            srFull = 0; raceOn = 0; srMapN = saveN; srMaps[0] = save0; srPbN = 0; srPbLoaded = 0;
            memset(srRun, 0, sizeof srRun); rkQN = 0; appState = APP_MENU;
            load_map(0); reset_game();
        }
        {   /* --- Modes, specialisations, boutique (l'hote arbitre ; 4 joueurs : 0 et 2 BLEUS, 1 et 3 ROUGES ; l'Arene) --- */
            int cm = 0;
            load_map(cm); reset_game();
            int nm0 = netMode, dm0 = dmMode, id0 = myId, ts0 = teamSize;
            MatchRules r0 = rules;
            FILE *logSave = botLog; botLog = NULL;            /* pas de journal dans le fichier de test */
            netMode = NET_HOST; dmMode = 1; myId = 0; teamSize = 2; matchOver = 0; matchT = 0;
            memset(NP, 0, sizeof NP);
            for (int i = 0; i < 4; i++) { NP[i].active = NP[i].alive = 1; NP[i].hp = 100; NP[i].team = (unsigned char)(i & 1); NP[i].disp = v3(60, 0, 60.0f + i); }
            P.pos = v3(60, 0, 60);
            #define TICKS(sec) for (int _k = 0; _k < (int)((sec) * 120); _k++) { nowT += TICK; mode_tick(TICK); }
            rules = (MatchRules){ MODE_ZONES, 1, AMMO_NORMAL, 0, 10 }; mode_reset();
            NP[1].disp = zone_pos(0);
            TICKS(ZONE_CAP_T + 0.2f);
            int ownA = obj.z[0].owner;
            NP[2].disp = zone_pos(1); NP[3].disp = zone_pos(1);   /* zone B contestee : personne ne la prend */
            TICKS(10);
            int ownB = obj.z[1].owner, ptsR = obj.pts[1];
            fprintf(f, "mode 3 ZONES : A prise par l'equipe %d en %.0f s, B contestee -> %d, points ROUGES apres 10 s %d (attendu 1, %.0f, 2, ~10)\n",
                    ownA, ZONE_CAP_T, ownB, ptsR, ZONE_CAP_T);
            rules.mode = MODE_DRAPEAU; mode_reset();
            for (int i = 0; i < 4; i++) NP[i].disp = v3(60, 0, 60.0f + i);
            NP[1].disp = flag_home(0);                    /* un ROUGE prend le drapeau BLEU */
            TICKS(0.1f);
            int carried = obj.fl[0].state == FLAG_CARRIED && obj.fl[0].carrier == 1;
            NP[1].disp = v3(-20, 0, 0);
            host_kill(0, 1, W_RAIL);                      /* il meurt : le drapeau tombe ou il est */
            int dropped = obj.fl[0].state == FLAG_DROPPED, at = fabsf(obj.fl[0].pos[0] + 20) < 0.1f;
            NP[2].disp = v3(-20.5f, 0, 0.5f); NP[2].lastHitT = -100;   /* un BLEU reste a cote sans etre touche */
            TICKS(FLAG_RETURN_T * 0.6f);
            NP[2].lastHitT = nowT;                        /* touche : le retour repart de zero */
            TICKS(0.1f);
            int retReset = obj.fl[0].ret == 0;
            NP[2].lastHitT = -100;
            TICKS(FLAG_RETURN_T + 0.3f);
            int returned = obj.fl[0].state == FLAG_HOME;
            NP[2].disp = v3(60, 0, 62);
            NP[3].disp = flag_home(0); TICKS(0.1f);       /* capture : le ROUGE 3 prend, ramene chez lui */
            NP[3].disp = flag_home(1); TICKS(0.1f);
            fprintf(f, "mode DRAPEAU : pris %d, tombe a la mort %d (sur place %d), retour remis a zero si touche %d, rendu %d, capture -> %d point (attendu 1, 1 1, 1, 1, 1)\n",
                    carried, dropped, at, retReset, returned, obj.pts[1]);
            rules.mode = MODE_BOUCLIER; mode_reset();
            for (int i = 0; i < 4; i++) { NP[i].alive = 1; NP[i].disp = v3(60, 0, 60.0f + i); }
            NP[1].disp = shield_pos(); TICKS(10);
            int sh1 = obj.shield;
            NP[0].disp = shield_pos(); P.pos = shield_pos(); TICKS(5);   /* un BLEU dessus : plus de surcharge */
            fprintf(f, "mode BOUCLIER : un ROUGE seul 10 s -> %d %%, un BLEU arrive -> %d %% 5 s plus tard (attendu ~%d, meme valeur)\n",
                    sh1, obj.shield, 100 - (int)(SHIELD_DRAIN * 10));
            rules.mode = MODE_FRAGS;
            rules.shop = 1; eco_reset();                  /* boutique */
            int c0 = credits, need = ecoNeedStarter;
            eco_pick_starter(WP_SHOTGUN);
            int noMoney = eco_buy(WP_ROCKET);
            eco_gain(ECO_KILL, NULL);
            int bought = eco_buy(WP_ROCKET);
            int lo[2]; eco_loadout(lo);
            fprintf(f, "boutique : depart %d pieces, arme a choisir %d, ROQUETTES trop cheres %d puis achetees %d, reste %d, armes %s + %s (attendu %d, 1, 0, 1, %d, SHOTGUN + ROQUETTES)\n",
                    c0, need, noMoney, bought, credits, ammoDef[lo[0]].name, ammoDef[lo[1]].name, ECO_START, ECO_START + ECO_KILL - eco_price(WP_ROCKET));
            #undef TICKS
            botLog = logSave;
            rules = r0; netMode = nm0; dmMode = dm0; myId = id0; teamSize = ts0; matchOver = 0;
            memset(NP, 0, sizeof NP); memset(&obj, 0, sizeof obj); ecoNeedStarter = 0;
            load_map(0); reset_game();
        }
        {   /* --- Cartes CHALLENGE : LA TOILE n'est que la carte du mode COURSE (ni menu des cartes, ni rotation, ni partie
             * rapide / classee / hebergee, ni solo par vagues) ; ROTATION : seulement les cartes qui ont le mode en cours --- */
            int rm = race_map(), inMenu = 0, inRot = 0, rotBad = 0, rotN = 0, chalModes = 0, pvpN = 0;
            for (int m = 0, k = 0; k < 2 * NMAPS; k++) { m = map_step(m, k & 1 ? -1 : 1); inMenu += m == rm; }
            for (int m = 0, k = 0; k < NMAPS; k++) { m = map_step(m, 1); inMenu += m == rm; }
            for (int m = 0; m < NMAPS; m++) pvpN += map_pvp(m);
            for (int md = 0; md < MODE_COUNT; md++) {
                chalModes += mode_ok(md, rm);
                for (int m = 0; m < NMAPS; m++) {
                    if (!map_pvp(m)) continue;
                    int nx = map_rotate_next(m, md);
                    rotN++; inRot += nx == rm; rotBad += !mode_ok(md, nx) || nx == m;
                }
            }
            int sMenu = menuMap, sQuick = hostQuick, sFfa = hostFfa, sTeams = wantTeams, sFrag = fragLimit, sTs = teamSize;
            MatchRules sWant = wantRules, sRules = rules;
            menuMap = rm; hostQuick = 0; hostFfa = 0; ranked = 0; wantTeams = 0; wantRules.mode = MODE_FRAGS;
            int hostMap = host_setup_rules();             /* HEBERGER avec LA TOILE choisie (--map 5) : une carte de partie */
            hostQuick = 1;
            int quickMap = host_setup_rules();
            menuMap = sMenu; hostQuick = sQuick; hostFfa = sFfa; wantTeams = sTeams; wantRules = sWant; rules = sRules;
            fragLimit = sFrag; teamSize = sTs;
            fprintf(f, "carte CHALLENGE %s : challenge %d, cartes de partie %d, dans le menu des cartes %d, rotation %d/%d, "
                       "modes acceptes %d, classee %d, solo %d, heberger -> %s, partie rapide -> %s, COURSE -> %s "
                       "(attendu 1, %d, 0, 0/%d, 0, 0, 0, pas LA TOILE x2, %s)\n",
                    maps[rm].name, maps[rm].challenge, pvpN, inMenu, inRot, rotN, chalModes, rk_map_ok(rm), maps[rm].solo,
                    maps[hostMap].name, maps[quickMap].name, maps[race_map()].name, NMAPS - 1, rotN, maps[rm].name);
            fprintf(f, "rotation : %d passages carte -> carte suivante sans le mode en cours ou sans changer (attendu 0) ; "
                       "3 ZONES depuis %s -> %s, FRAGS depuis %s -> %s (attendu %s, %s)\n", rotBad,
                    maps[4].name, maps[map_rotate_next(4, MODE_ZONES)].name,
                    maps[4].name, maps[map_rotate_next(4, MODE_FRAGS)].name, maps[0].name, maps[0].name);
        }
        {   /* --- Objectifs des cartes neon (3 ZONES, DRAPEAU, BOUCLIER, departs par equipe) : chaque point au-dessus d'un sol
             * (rayon vers le bas < 1,5 m, pas un pad), hors des boites, au-dessus du plan de mort ; cartes ouvertes : du sol a
             * 3 m tout autour ; disque des zones et du bouclier entierement sur le sol, a l'ecart des pads ; zones a plus de
             * 2 rayons, drapeaux a plus de 40 m, symetrie (miroir ou demi-tour) ; chemins des bots vers chaque objectif --- */
            char ob[2048]; int on = 0, modesOk = 0, mapsN = 0;
            for (int m = 0; m < NMAPS; m++) {
                if (!map_pvp(m)) continue;
                const MapDef *md = &maps[m];
                load_map(m); reset_game();
                mapsN++;
                for (int md2 = MODE_ZONES; md2 <= MODE_BOUCLIER; md2++) modesOk += mode_ok(md2, m);
                float zr = zone_r(), sr = shield_r();
                int open = md->killY > -999, nTs = md->nTeamSpawn;
                V3 pts[48]; float prad[48]; int npt = 0;       /* points a verifier : 3 zones, 2 drapeaux, bouclier, departs */
                for (int z = 0; z < NZONES; z++) { pts[npt] = zone_pos(z); prad[npt++] = zr; }
                for (int t = 0; t < 2; t++) { pts[npt] = flag_home(t); prad[npt++] = FLAG_TOUCH_R; }
                pts[npt] = shield_pos(); prad[npt++] = sr;
                for (int k = 0; k < 2 * nTs && npt < 48; k++) { pts[npt] = v3(md->teamSpawn[k][0], md->teamSpawn[k][1], md->teamSpawn[k][2]); prad[npt++] = 0; }
                int ground = 0, inside = 0, above = 0, edge = 0, disc = 0, discN = 0, padNear = 0;
                for (int i = 0; i < npt; i++) {
                    V3 p = pts[i];
                    float t = ray_world(v3(p.x, p.y + 0.5f, p.z), v3(0, -1, 0), 2.0f, 0);
                    ground += t < 2.0f && rayBox >= 0 && boxes[rayBox].kind != B_PAD;
                    inside += i < 6 ? !pos_free(v3(p.x, p.y + 0.05f, p.z), H_STAND) : !pos_free(p, H_STAND);
                    above += p.y > md->killY + 1;
                    int e = 1;                            /* carte ouverte : du sol a 3 m dans 8 directions */
                    for (int a = 0; a < 8 && open; a++) {
                        V3 q = v3(p.x + cosf(a * PI / 4) * 3, p.y + 0.5f, p.z + sinf(a * PI / 4) * 3);
                        e &= ray_world(q, v3(0, -1, 0), 2.0f, 0) < 2.0f;
                    }
                    edge += e;
                    for (int b = 0; b < nb; b++) {        /* a l'ecart des pads : pas un pad dans le rayon de l'objectif */
                        if (boxes[b].kind != B_PAD || fabsf(boxes[b].mx.y - p.y) > 3) continue;
                        float dx = fmaxf(0, fmaxf(boxes[b].mn.x - p.x, p.x - boxes[b].mx.x)), dz = fmaxf(0, fmaxf(boxes[b].mn.z - p.z, p.z - boxes[b].mx.z));
                        if (sqrtf(dx * dx + dz * dz) < fmaxf(prad[i], 1.0f) - 0.01f) padNear++;
                    }
                    if (i < 6 && i != 3 && i != 4) {      /* zones et bouclier : le disque entier sur un sol plat, libre */
                        for (int s = 0; s < 24; s++) {
                            float r = prad[i] * (s < 16 ? 0.95f : 0.5f), a = s * 2 * PI / (s < 16 ? 16 : 8);
                            V3 q = v3(p.x + cosf(a) * r, p.y + 0.3f, p.z + sinf(a) * r);
                            float tq = ray_world(v3(q.x, p.y + 1.6f, q.z), v3(0, -1, 0), 3.2f, 0);
                            int ok = tq < 3.2f && fabsf(1.6f - tq) < 1.5f && boxes[rayBox].kind != B_PAD;
                            q.y = p.y + 1.6f - tq + 0.3f;     /* 30 cm au-dessus du sol trouve (le ring du Dojo est une marche) */
                            for (int b = 0; b < nb && ok; b++)
                                if (q.x > boxes[b].mn.x && q.x < boxes[b].mx.x && q.y > boxes[b].mn.y && q.y < boxes[b].mx.y &&
                                    q.z > boxes[b].mn.z && q.z < boxes[b].mx.z) ok = 0;
                            disc += ok; discN++;
                        }
                    }
                }
                float dz01 = vlen(vsub(zone_pos(0), zone_pos(1))), dz12 = vlen(vsub(zone_pos(1), zone_pos(2))), dz02 = vlen(vsub(zone_pos(0), zone_pos(2)));
                float dzMin = fminf(dz01, fminf(dz12, dz02)), dFlag = vlen(vsub(flag_home(0), flag_home(1)));
                V3 mb = v3(0, 0, 0), mr = v3(0, 0, 0);        /* axe des equipes : ecart moyen des departs BLEUS et ROUGES */
                for (int k = 0; k < nTs; k++) {
                    mb = vadd(mb, v3(md->teamSpawn[k][0], 0, md->teamSpawn[k][2]));
                    mr = vadd(mr, v3(md->teamSpawn[nTs + k][0], 0, md->teamSpawn[nTs + k][2]));
                }
                int ax = fabsf(mr.x - mb.x) > fabsf(mr.z - mb.z) ? 0 : 2, side = 0, sideN = 0;
                #define AXC(v) ((ax) == 0 ? (v).x : (v).z)
                side += AXC(zone_pos(0)) < -1; side += AXC(zone_pos(2)) > 1; side += AXC(flag_home(0)) < -1; side += AXC(flag_home(1)) > 1;
                side += AXC(shield_pos()) < -1; sideN = 5;
                for (int k = 0; k < nTs; k++) { side += md->teamSpawn[k][ax] < -1; side += md->teamSpawn[nTs + k][ax] > 1; sideN += 2; }
                #undef AXC
                /* symetrie horizontale : miroir sur l'axe des equipes, ou demi-tour autour du centre (LE PUITS) */
                #define SYM(a, b) ((fabsf((a).x + (b).x) < 0.1f || (ax == 2 && fabsf((a).x - (b).x) < 0.1f)) && \
                                   (fabsf((a).z + (b).z) < 0.1f || (ax == 0 && fabsf((a).z - (b).z) < 0.1f)))
                int sym = SYM(zone_pos(0), zone_pos(2)) + SYM(flag_home(0), flag_home(1)), symN = 2;
                for (int k = 0; k < nTs; k++, symN++)
                    sym += SYM(v3(md->teamSpawn[k][0], 0, md->teamSpawn[k][2]), v3(md->teamSpawn[nTs + k][0], 0, md->teamSpawn[nTs + k][2]));
                #undef SYM
                int navOk = 0, navN = 0, navOkNh = 0, ruleHook0 = rules.hook, dm0 = dmMode;   /* chemins : grappin permis, puis interdit */
                for (int pass = 0; pass < 2; pass++) {
                    dmMode = pass; rules.hook = 0;        /* pass 1 : partie sans grappin (hook_allowed() = 0) */
                    navMap = -1; nav_build();
                    for (int t = 0; t < 2; t++)
                        for (int k = 0; k < 6; k++) {
                            V3 from = v3(md->teamSpawn[t * nTs][0], md->teamSpawn[t * nTs][1], md->teamSpawn[t * nTs][2]);
                            V3 to = k < 3 ? zone_pos(k) : k < 5 ? flag_home(k - 3) : shield_pos();
                            int a = nav_nearest(from, 0), b = nav_nearest(to, 0), path[24];
                            float tg = ray_world(v3(to.x, to.y + 0.5f, to.z), v3(0, -1, 0), 2.0f, 0);   /* sol de l'objectif */
                            int gb = tg < 2.0f ? rayBox : -1;
                            int ok = a >= 0 && b >= 0 && (a == b || nav_path(a, b, path, 24) > 0) &&
                                     (NV[b].box == gb || hlen(vsub(NV[b].p, to)) < fmaxf(6, prad[k] + 2)) && fabsf(NV[b].p.y - to.y) < 2.5f;
                            if (pass == 0) { navOk += ok; navN++; } else navOkNh += ok;
                            if (!ok && getenv("OBJDBG"))  /* OBJDBG=1 : detail des chemins manques dans le fichier de test */
                                fprintf(f, "  [%s grappin %d] equipe %d -> objectif %d : noeuds %d -> %d, au but %.1f m (dy %.1f)\n", md->name, !pass,
                                        t, k, a, b, b >= 0 ? hlen(vsub(NV[b].p, to)) : -1.0f, b >= 0 ? NV[b].p.y - to.y : 0.0f);
                        }
                }
                for (int i = 0; i < nnav && getenv("OBJDBG"); i++)   /* OBJDBG : ou retombe chaque pad (sans grappin) */
                    for (int e = 0; e < NV[i].ne; e++)
                        if (NV[i].type[e] == NE_PAD)
                            fprintf(f, "  [%s] pad %d (%.1f %.1f %.1f) -> noeud %d (%.1f %.1f %.1f)\n", md->name, i, NV[i].p.x, NV[i].p.y,
                                    NV[i].p.z, NV[i].to[e], NV[NV[i].to[e]].p.x, NV[NV[i].to[e]].p.y, NV[NV[i].to[e]].p.z);
                dmMode = dm0; rules.hook = ruleHook0; navMap = -1;
                on += snprintf(ob + on, sizeof ob - on, "%s%s : sol %d/%d, dans une boite %d, au-dessus du vide %d/%d, bords %d/%d, "
                               "disques %d/%d, pads %d, zones a %.0f m (> %.0f), drapeaux a %.0f m, cotes %d/%d, symetrie %d/%d, "
                               "%d+%d departs, chemins %d/%d (sans grappin %d/%d)", mapsN > 1 ? " ; " : "", md->name, ground, npt, inside,
                               above, npt, edge, npt, disc, discN, padNear, dzMin, 2 * zr, dFlag, side, sideN, sym, symN, nTs, nTs,
                               navOk, navN, navOkNh, navN);
                if (ground != npt || inside || above != npt || edge != npt || disc != discN || padNear || dzMin <= 2 * zr || dFlag <= 40 ||
                    side != sideN || sym != symN || nTs < 4 || navOk != navN || navOkNh != navN)
                    on += snprintf(ob + on, sizeof ob - on, " [ECHEC]");
            }
            fprintf(f, "objectifs : %s (attendu partout : tout le sol, 0 boite, tout au-dessus, bords tous, disques tous, 0 pad, zones > 2 rayons, "
                       "drapeaux > 40 m, cotes tous, symetrie toute, >= 4 departs, chemins tous, pas de [ECHEC])\n", ob);
            fprintf(f, "objectifs : modes a objectif acceptes %d sur %d cartes neon (attendu %d)\n", modesOk, mapsN, 3 * mapsN);
            load_map(0); reset_game();
        }
        {   /* --- Jauge de style : seulement en CHALLENGE ; hors course, rien ne monte et les points restent x1 --- */
            reset_game(); gameTime = 100; raceOn = 0;
            P.grounded = 0; P.vel = v3(25, 0, 0);
            style_kill(W_RAIL); style_tick(TICK);
            float off = style; float offMul = style_mult();
            fprintf(f, "style hors CHALLENGE (PvP, vagues) : jauge %.0f, points x%.2f (attendu 0, x1.00)\n", off, offMul);
            reset_game(); gameTime = 100; raceOn = 1;   /* --- en CHALLENGE : kill aerien a pleine vitesse, enchaine, coup recu, oubli --- */
            P.grounded = 0; P.vel = v3(25, 0, 0);
            style_kill(W_RAIL);
            float s1 = style; int r1 = styleRank;
            gameTime += 1;
            style_kill(W_RAIL);
            float s2 = style; int r2 = styleRank;
            P.hp = 60; style_tick(TICK);
            float s3 = style;
            P.vel = v3(0, 0, 0);
            for (int i = 0; i < 20 * 120; i++) style_tick(TICK);
            fprintf(f, "style : kill aerien rapide %.0f (%s), enchaine %.0f (%s), coup de 40 PV -> %.0f, 20 s a l'arret -> %.0f (%s), "
                       "points x%.2f en S (attendu 105 C, %.0f A, %.0f, 0 D, x2.00)\n",
                    s1, styleName[r1], s2, styleName[r2], s3, style, styleName[styleRank], styleMultTab[ST_S],
                    105 + 105 + STYLE_BONUS, 105 + 105 + STYLE_BONUS - 40 * STYLE_HURT_K);
            raceOn = 0; reset_game();
        }
        {   /* --- Navigation des bots : depart -> but sur chaque carte, avec le graphe et les vraies commandes --- */
            static const float nv[8][7] = {               /* carte, depart x y z, but x y z */
                { 0, -66, 0, 66, 66, 0, -66 }, { 1, 17.8f, 0, 17.8f, -3, 50, 0 }, { 2, -60, 20, 10, 60, 20, -10 },
                { 2, -60, 20, 0, 0, 18, 12 }, { 3, 10, 0, 26, 32, 10, 0 }, { 4, 16, 0, 16, -16, 0, -16 },
                { 1, -3, 50, 0, -7, 25, 0 },                  /* objectifs : du sommet du Puits au balcon (on tombe du bord) */
                { 2, -60, 20, 10, 0, 10, 0 },                 /* de la falaise ouest au plateau central du Canyon (zone B) */
            };
            char nb2[768]; int nn = 0;
            FILE *logSave = botLog; botLog = NULL;        /* pas de journal des bots dans le fichier de test */
            for (int k = 0; k < 8; k++) {
                load_map((int)nv[k][0]); reset_game(); hitstop = 0; appState = APP_GAME;
                memset(act, 0, sizeof act); memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
                P.pos = P.prevPos = v3(nv[k][1], nv[k][2], nv[k][3]);
                V3 goal = v3(nv[k][4], nv[k][5], nv[k][6]);
                navLen = 0; navReplanT = 0; navMap = -1;
                int plan = nav_plan(goal, 1, TICK), plen = navLen, ok = 0, falls = 0;
                float t = 0;
                for (; t < 40 && !ok; t += TICK) {
                    memset(botHeld, 0, sizeof botHeld);
                    if (nav_plan(goal, 0, TICK)) nav_follow(TICK); else { V3 d = vsub(goal, P.pos); P.yaw = atan2f(d.x, -d.z); botHeld[A_FWD] = 1; }
                    botMode = 1; bot_void_guard(TICK); botMode = 0;
                    for (int i = 0; i < MAXE; i++) E[i].alive = 0;
                    game_tick(TICK); P.hp = 100;
                    if (P.dead || P.pos.y < maps[curMap].killY) { falls++; P.dead = 0; P.pos = P.prevPos = v3(nv[k][1], nv[k][2], nv[k][3]); P.vel = v3(0, 0, 0); navLen = 0; }
                    ok = hlen(vsub(goal, P.pos)) < 4 && fabsf(goal.y - P.pos.y) < 2.5f;
                    if (getenv("NAVDBG") && fmodf(t, 0.5f) < TICK)   /* NAVDBG=1 : trace du trajet dans le fichier de test */
                        fprintf(f, "  [%d %.1f] pos %.1f %.1f %.1f v %.1f etape %d/%d crochet %d sol %d atterr %d\n", k, t, P.pos.x, P.pos.y,
                                P.pos.z, hlen(P.vel), navStep, navLen, P.hook, P.grounded, navLanding);
                }
                nn += snprintf(nb2 + nn, sizeof nb2 - nn, "%s%s %d noeuds chemin %d -> %s en %.1f s chutes %d", k ? " ; " : "",
                               maps[curMap].name, nnav, plan ? plen : 0, ok ? "arrive" : "PERDU", t, falls);
            }
            botLog = logSave;
            fprintf(f, "navigation : %s (attendu arrive partout)\n", nb2);
            appState = APP_MENU;
        }

        load_map(3);                                      /* La Boucle : bande d'acceleration (voie nord exterieure, vers +x) */
        reset_game(); hitstop = 0;
        memset(act, 0, sizeof act); memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
        float sv[3];
        static const float sx0[3] = { -30, -35, -20 }, sv0[3] = { 0, 26, -10 };
        static const int sticks[3] = { 60, 36, 36 };
        for (int k = 0; k < 3; k++) {
            P.pos = P.prevPos = v3(sx0[k], 0.12f, 26.5f); P.vel = v3(sv0[k], 0, 0); P.grounded = 1;
            for (int i = 0; i < sticks[k]; i++) player_tick(TICK, 0, 0);
            sv[k] = P.vel.x;
        }
        fprintf(f, "bande d'acceleration : arret -> %.1f m/s en 0,5 s, 26 m/s -> %.1f (plafond), a contresens -10 -> %.1f en 0,3 s "
                   "(attendu ~15, %.0f, ~-1)\n", sv[0], sv[1], sv[2], SPEED_MAX);

        load_map(0);
        reset_game();
        nowT = savedNow;
    }
    /* --- fin Cartes --- */
    /* --- Competences : PIECE, MARQUE, CHARGE, RICOCHET, ALL IN ONE, MIRAGE (docs/COMPETENCES_MARKSMAN.md). Labo dans le
     * ciel (y = 100), loin de la carte : deux murs paralleles en z = 90 et z = 110 (normales +z et -z), libre en x --- */
    {
        const float SKY = 100;
        int nb0 = nb;
        FILE *savedLog = botLog;
        botLog = NULL;
        netMode = NET_HOST; connected = 1; myId = 0; dmMode = 0;
        memset(NP, 0, sizeof NP);
        addbox(-60, SKY - 20, 110, 60, SKY + 20, 112, 0.5f, 0.5f, 0.5f);
        addbox(-60, SKY - 20, 88, 60, SKY + 20, 90, 0.5f, 0.5f, 0.5f);
#define SK_LAB(sk, ch) do { reset_game(); P.pos = P.prevPos = v3(0, SKY, 100); P.vel = v3(0, 0, 0); P.yaw = PI * 0.5f; P.pitch = 0; \
                            P.skill = (sk); P.skillCh = (ch); shotStreak = 0; } while (0)
#define SK_DRONE(x, z) do { E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3((x), eye_pos().y, (z)); } while (0)

        reset_game();                                     /* PIECE par defaut, recharge propre a chaque competence */
        fprintf(f, "competences : par defaut %s, %.0f charges (attendu PIECE, %d : 2 charges au lieu de 4)\n", skillDef[P.skill].name, P.skillCh, COIN_MAX);
        {
            char rb[256];
            int rn = 0;
            for (int k = 0; k < SK_COUNT; k++) {
                SK_LAB(k, 0);
                int ticks = 0;
                while (P.skillCh < 1 && ticks < 60 * 120) { skill_tick(TICK, 0, 0); ticks++; }
                rn += snprintf(rb + rn, sizeof rb - rn, "%s %.1f s, ", skillDef[k].name, ticks * TICK);
            }
            fprintf(f, "competences : recharge d'une charge : %s(attendu 5.0, 6.0, 8.0, 20.0, 20.0, 7.0, 45.0)\n", rb);
        }

        SK_LAB(SK_MARK, 2); SK_DRONE(20, 100);            /* MARQUE : drone a 20 m, x1,25 pendant 4 s */
        skill_tick(TICK, 1, 1);
        {
            int marked = markE[0] > 0;
            for (int i = 0; i < 468; i++) skill_tick(TICK, 0, 0);   /* 3,9 s */
            int still = markE[0] > 0;
            fire_rail();
            float d1 = 1000 - E[0].hp;
            for (int i = 0; i < 24; i++) skill_tick(TICK, 0, 0);    /* 4,1 s */
            int after = markE[0] > 0;
            E[0].hp = 1000; shotStreak = 0;
            fire_rail();
            float d2 = 1000 - E[0].hp;
            SK_LAB(SK_MARK, 2); SK_DRONE(95, 100);
            skill_tick(TICK, 1, 1);
            fprintf(f, "marque : drone a 20 m marque %d, encore marque a 3,9 s %d, rail %.0f (attendu 1, 1, %.0f) ; a 4,1 s marque %d, "
                       "rail %.0f (attendu 0, %.0f) ; drone a 95 m marque %d (attendu 0), charges %.0f (attendu 1)\n",
                    marked, still, d1, RAIL_DMG * MARK_MULT, after, d2, RAIL_DMG, markE[0] > 0, P.skillCh);
        }

        SK_LAB(SK_CHARGE, 1); SK_DRONE(20, 100);          /* CHARGE : maintien 0,6 s, une seule touche x1,5, perdue si touche */
        {
            for (int i = 0; i < 60; i++) skill_tick(TICK, 0, 1);    /* 0,5 s */
            int at05 = P.chargeT > 0;
            for (int i = 0; i < 14; i++) skill_tick(TICK, 0, 1);    /* 0,62 s */
            int at06 = P.chargeT > 0;
            skill_tick(TICK, 0, 0);
            fire_rail();
            float d1 = 1000 - E[0].hp;
            for (int i = 0; i < 12; i++) skill_tick(TICK, 0, 0);
            E[0].hp = 1000; shotStreak = 0;
            fire_rail();
            float d2 = 1000 - E[0].hp;
            SK_LAB(SK_CHARGE, 1); SK_DRONE(20, 100);
            for (int i = 0; i < 36; i++) skill_tick(TICK, 0, 1);    /* 0,3 s, relache, 0,3 s : rien */
            skill_tick(TICK, 0, 0);
            for (int i = 0; i < 36; i++) skill_tick(TICK, 0, 1);
            int split = P.chargeT > 0;
            for (int i = 0; i < 80; i++) skill_tick(TICK, 0, 1);
            int ready = P.chargeT > 0;
            damage_player(10);                            /* un drone me touche avant le tir */
            skill_tick(TICK, 0, 0);
            fire_rail();
            fprintf(f, "charge : prete a 0,5 s %d, a 0,62 s %d (attendu 0, 1) ; 1re touche %.0f, 2e %.0f (attendu %.0f, %.0f) ; "
                       "maintien coupe 0,3 + 0,3 s %d (attendu 0) ; prete %d puis touche -> rail %.0f (attendu 1, %.0f)\n",
                    at05, at06, d1, d2, RAIL_DMG * CHARGE_MULT, RAIL_DMG, split, ready, 1000 - E[0].hp, RAIL_DMG);
        }

        {                                                 /* RICOCHET : fenetre de RIC_DURATION s, tout rebondit sans fin, recharge apres la fermeture */
            SK_LAB(SK_RICOCHET, 1); P.yaw = PI - 0.5f;    /* 28,6 degres d'incidence sur le mur z = 110 */
            skill_tick(TICK, 1, 1);
            int armed = P.ricArmed, b0 = ricStat[1];
            float chOpen = P.skillCh;
            for (int i = 0; i < 600; i++) skill_tick(TICK, 0, 0);   /* 5 s : encore ouverte (6 s) */
            int open5 = P.ricArmed;
            fire_rail();
            float inc = acosf(clampf(-vdot(ricDir[0], ricNrm[0]), -1, 1)) * 180 / PI;
            float out = acosf(clampf(vdot(ricDir[1], ricNrm[0]), -1, 1)) * 180 / PI;
            int segs = ricSegs, bounces = ricStat[1] - b0, armedAfter = P.ricArmed;
            for (int i = 0; i < 132; i++) skill_tick(TICK, 0, 0);   /* 6,1 s depuis l'ouverture */
            int closed = !P.ricArmed; float chClosed = P.skillCh;
            for (int i = 0; i < 20 * 120 - 12; i++) skill_tick(TICK, 0, 0);   /* 20 s apres la fermeture */
            fprintf(f, "ricochet : ouvert %d, charge %.2f (attendu 1, 0.00) ; a 5 s encore ouvert %d (attendu 1) ; incidence %.2f, sortie %.2f, ecart %.2f "
                       "(attendu ecart < 0.5) ; segments %d, rebonds %d (attendu 6, 5 : le tir sort du labo de 120 m, plus de plafond a 3) ; "
                       "encore ouvert apres le tir %d (attendu 1) ; ferme a 6,1 s %d, charge juste apres %.2f, 20 s apres la fermeture %.2f "
                       "(attendu 1, ~0.00, 1.00)\n", armed, chOpen, open5, inc, out, fabsf(inc - out), segs, bounces, armedAfter, closed, chClosed,
                    P.skillCh);

            float dxw = 10 * tanf(0.5f);                  /* touche le mur z = 110 en x = 10 tan(0,5), repasse en z = 100 en x = 2 x */
            SK_LAB(SK_RICOCHET, 1); P.yaw = PI - 0.5f; SK_DRONE(2 * dxw, 100);
            fire_rail();                                  /* non arme : comportement d'avant */
            float dUn = 1000 - E[0].hp; int segUn = ricSegs;
            SK_LAB(SK_RICOCHET, 1); P.yaw = PI - 0.5f; SK_DRONE(2 * dxw, 100);
            skill_tick(TICK, 1, 1);
            fire_rail();
            fprintf(f, "ricochet : drone hors de vue derriere le rebond, non arme %.0f (segments %d), arme %.0f (attendu 0 (1), %.0f : x1)\n",
                    dUn, segUn, 1000 - E[0].hp, RAIL_DMG);

            SK_LAB(SK_RICOCHET, 1); P.yaw = PI;           /* tir perpendiculaire : le 1er rebond me traverse sans degats, le 2e me touche */
            skill_tick(TICK, 1, 1);
            int self0 = ricStat[2];
            fire_rail();
            fprintf(f, "ricochet : tir perpendiculaire entre deux murs, PV %.0f, retours sur soi %d, segments %d (attendu %.0f, 1 : une seule fois, %d = 1 + "
                       "RIC_CAP_BOUNCES : le garde-fou technique arrete le tir)\n", P.hp, ricStat[2] - self0, ricSegs, 100 - RAIL_DMG * 0.5f,
                    1 + RIC_CAP_BOUNCES);

            SK_LAB(SK_RICOCHET, 1); P.yaw = PI; SK_DRONE(0, 105);   /* un drone sur la ligne : le tir le croise a chaque aller-retour, mais ne le touche qu'une fois */
            skill_tick(TICK, 1, 1);
            fire_rail();
            fprintf(f, "ricochet : drone sur la ligne d'un tir de %d segments (il la croise a chaque aller-retour), degats %.0f (attendu %.0f : "
                       "RIC_HITS_MAX = %d touche par cible et par tir)\n", ricSegs, 1000 - E[0].hp, RAIL_DMG, RIC_HITS_MAX);

            for (int pass = 0; pass < 2; pass++) {        /* roquette : rebondit jusqu'a sortir du labo ou toucher un drone (vie 15 s au plus) */
                SK_LAB(SK_RICOCHET, 1); P.yaw = PI - 0.5f; P.weapon = WP_ROCKET;
                if (pass) SK_DRONE(2 * dxw, 100);
                skill_tick(TICK, 1, 1);
                memset(PR, 0, sizeof PR);
                fire_rocket();
                Proj *rk = &PR[0];
                int bounces = 0, prev = rk->ricLeft, leftAtEnd = -1, endOk = 0;
                float v1 = 0, vEnd = 0;
                for (int i = 0; i < 16 * 120 && rk->alive; i++) {
                    proj_tick(TICK);
                    if (rk->ricLeft != prev) { bounces++; prev = rk->ricLeft; if (bounces == 1) v1 = vlen(rk->vel); }
                    vEnd = vlen(rk->vel);
                    if (!rk->alive) { leftAtEnd = rk->ricLeft; endOk = rk->life <= 0; }
                }
                if (!pass)
                    fprintf(f, "ricochet roquette : rebonds %d, vitesse apres le 1er %.1f, a la fin %.1f, explose en fin de vie (%.0f s) %d "
                               "(attendu 5 a 6, %.1f, %.1f, %.0f s, 1 : sortie du labo, plus de 3 rebonds)\n",
                            bounces, v1, vEnd, RIC_ROCKET_LIFE, endOk, ROCKET_SPEED * RIC_ROCKET_SLOW, ROCKET_SPEED * RIC_ROCKET_SLOW, RIC_ROCKET_LIFE);
                else
                    fprintf(f, "ricochet roquette : drone sur le rebond touche %d, rebonds restants %d (attendu 1, %d = RIC_ROCKET_CAP - 1)\n",
                            E[0].hp < 1000, leftAtEnd, RIC_ROCKET_CAP - 1);
            }
            memset(PR, 0, sizeof PR);

            {                                             /* plombs : le meme drone, cache derriere le rebond, n'est touche que fenetre ouverte */
                float dmg[2]; int pr[2];
                for (int pass = 0; pass < 2; pass++) {
                    SK_LAB(SK_RICOCHET, 1); P.yaw = PI - 0.5f; P.weapon = WP_SHOTGUN; P.ammo[WP_SHOTGUN] = 6; SK_DRONE(2 * dxw, 100);
                    if (pass) skill_tick(TICK, 1, 1);
                    fire_shotgun(); hitstop = 0;
                    dmg[pass] = 1000 - E[0].hp; pr[pass] = sgPellRic;
                }
                fprintf(f, "ricochet plombs : drone derriere le rebond, fenetre fermee %.1f (rebonds %d), ouverte %.1f (rebonds de plombs %d) "
                           "(attendu 0 (0), > 0 (> 0))\n", dmg[0], pr[0], dmg[1], pr[1]);
            }
            {                                             /* scie : fenetre ouverte, elle rebondit sur le mur au lieu de s'y planter */
                SK_LAB(SK_RICOCHET, 1); P.yaw = PI - 0.5f; P.weapon = WP_SAW; P.ammo[WP_SAW] = 1;
                skill_tick(TICK, 1, 1);
                memset(PR, 0, sizeof PR);
                int rb0 = ricStat[1];
                fire_saw();
                Proj *sw = &PR[0];
                for (int i = 0; i < 264 && sw->alive; i++) proj_tick(TICK);   /* 2,2 s de vol : 1er mur a 0,36 s, 2e a 1,07 s, 3e a 1,8 s */
                fprintf(f, "ricochet scie : rebonds %d, etat %d, rebonds restants %d (attendu >= 2, %d = SAW_FLY, < %d)\n",
                        ricStat[1] - rb0, sw->sawState, sw->ricLeft, SAW_FLY, RIC_SAW_CAP);
                memset(PR, 0, sizeof PR);
            }
        }

        {                                                 /* ALL IN ONE : n projectiles, munition a 0, eventail, refus */
            SK_LAB(SK_ALLIN, 1); P.weapon = WP_ROCKET; P.ammo[WP_ROCKET] = 4;
            memset(PR, 0, sizeof PR);
            skill_tick(TICK, 1, 1);
            int rockets = 0;
            for (int i = 0; i < MAXPR; i++) rockets += PR[i].alive && PR[i].kind == PR_ROCKET;
            fprintf(f, "all in one : 4 roquettes -> %d projectiles, munition %d, charges %.0f (attendu 4, 0, 0)\n",
                    rockets, P.ammo[WP_ROCKET], P.skillCh);
            memset(PR, 0, sizeof PR);
            float hm[3];
            for (int k = 0; k < 3; k++) {
                SK_LAB(SK_ALLIN, 1); P.weapon = WP_RAIL; P.ammo[WP_RAIL] = 2 + 2 * k;
                skill_tick(TICK, 1, 1);
                hm[k] = allinSpan * 0.5f * 180 / PI;
            }
            fprintf(f, "all in one : demi-angle n=2 / 4 / 6 : %.2f / %.2f / %.2f (attendu %.2f / %.2f / %.2f)\n", hm[0], hm[1], hm[2],
                    allin_half(2) * 180 / PI, allin_half(4) * 180 / PI, allin_half(6) * 180 / PI);
            SK_LAB(SK_ALLIN, 1); P.weapon = WP_RAIL; P.ammo[WP_RAIL] = 1;
            skill_tick(TICK, 1, 1);
            int a1 = P.ammo[WP_RAIL]; float c1 = P.skillCh;
            char why[64];
            snprintf(why, sizeof why, "%s", popText);
            SK_LAB(SK_ALLIN, 1); P.weapon = WP_TESLA;
            skill_tick(TICK, 1, 1);
            fprintf(f, "all in one : refus a 1 munition (munition %d, charges %.0f, \"%s\"), tesla en main sans cible charges %.0f \"%s\" "
                       "(attendu 1, 1, 1, \"ALL IN ONE : PAS DE CIBLE\")\n", a1, c1, why, P.skillCh, popText);

            SK_LAB(SK_ALLIN, 1); dmMode = 1;              /* PvP : 2 rails a 8 m, plafond par projectile : one-shot */
            NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
            NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible");
            NP[1].disp = v3(8, SKY, 100); vf(NP[1].st.pos, NP[1].disp);
            {
                V3 d = vsub(np_center(1), eye_pos());
                P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
            }
            P.weapon = WP_RAIL; P.ammo[WP_RAIL] = 2;
            int bk0 = burstKills;
            skill_tick(TICK, 1, 1);
            fprintf(f, "all in one PvP : 2 rails a 8 m -> cible en vie %d, salves mortelles +%d (attendu 0, 1 : %.0f par projectile)\n",
                    NP[1].alive, burstKills - bk0, fminf(RAIL_DMG * PVP_SCALE, PVP_HIT_CAP));
            dmMode = 0; memset(NP, 0, sizeof NP);
        }

        {                                                 /* MIRAGE : immobile, sans gravite, 3 s ; absorbe ; mon rail y ricoche */
            SK_LAB(SK_DECOY, 2);
            skill_tick(TICK, 1, 1);
            int ci = -1;
            for (int i = 0; i < MAXCOIN; i++) if (CO[i].alive && CO[i].decoy) ci = i;
            V3 p0 = ci >= 0 ? CO[ci].pos : v3(0, 0, 0);
            for (int i = 0; i < 348; i++) coin_tick(TICK);           /* 2,9 s */
            int alive29 = ci >= 0 && CO[ci].alive;
            float moved = ci >= 0 ? vlen(vsub(CO[ci].pos, p0)) : -1;
            for (int i = 0; i < 24; i++) coin_tick(TICK);            /* 3,1 s */
            fprintf(f, "mirage : leurre pose %d, charges %.0f, a 2,9 s en vie %d deplacement %.3f, a 3,1 s en vie %d (attendu 1, 1, 1, 0.000, 0)\n",
                    ci >= 0, P.skillCh, alive29, moved, ci >= 0 && CO[ci].alive);

            SK_LAB(SK_DECOY, 2); dmMode = 1;              /* le rail adverse s'arrete sur le leurre (une piece volee l'aurait renvoye) */
            NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
            NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Lanceur");
            NP[1].disp = v3(20, SKY, 100); vf(NP[1].st.pos, NP[1].disp);
            memset(CO, 0, sizeof CO);
            CO[0].alive = 1; CO[0].decoy = 1; CO[0].owner = 1; CO[0].life = 3; CO[0].netId = 0x02000077;
            CO[0].pos = vadd(np_center(1), v3(-10, 0, 0));
            {
                V3 d = vsub(np_center(1), eye_pos());
                P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
            }
            fire_rail();
            fprintf(f, "mirage : rail adverse dans le leurre -> leurre detruit %d, PV du lanceur %.0f, message \"%s\" (attendu 1, 100, LEURRE !)\n",
                    !CO[0].alive, NP[1].hp, popText);
            dmMode = 0; memset(NP, 0, sizeof NP);

            SK_LAB(SK_DECOY, 2); SK_DRONE(10, 94);        /* mon leurre a 10 m devant : mon rail y ricoche vers le drone, x1 */
            memset(CO, 0, sizeof CO);
            CO[0].alive = 1; CO[0].decoy = 1; CO[0].owner = myId; CO[0].life = 3; CO[0].pos = vadd(eye_pos(), v3(10, 0, 0));
            lastChain = 0;
            fire_rail();
            float dr = 1000 - E[0].hp; int ch = lastChain;
            SK_LAB(SK_DECOY, 2); SK_DRONE(20, 100);       /* leurre pose a mes pieds : mon tir le traverse (pas de visee auto) */
            memset(CO, 0, sizeof CO);
            skill_tick(TICK, 1, 1);
            fire_rail();
            fprintf(f, "mirage : mon rail ricoche sur mon leurre -> drone %.0f, chaine %d (attendu %.0f, 1) ; leurre a mes pieds traverse : "
                       "drone en face %.0f, leurre intact %d (attendu %.0f, 1)\n", dr, ch, RAIL_DMG * DECOY_MULT, 1000 - E[0].hp,
                    CO[0].alive, RAIL_DMG);
        }

        {   /* --- Triangle MIRAGE => PIECE => RICOCHET => MIRAGE, TESLA x ALL IN ONE, ROQUETTE x PIECE (patch d'equilibrage) --- */
            double nt0 = nowT;
            nowT = 1000;                                  /* loin de lastParryT = 0 : aucun parry fortuit de l'hote */
#define SK_PLAYERS() do { memset(NP, 0, sizeof NP); \
                          NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi"); \
                          NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible"); \
                          NP[2].active = NP[2].alive = 1; NP[2].hp = 100; snprintf(NP[2].name, 16, "Poseur"); } while (0)
            /* MIRAGE => PIECE : le ricochet de ma piece suit le leurre adverse (62 m) et non le joueur (12 m) */
            float hpLure[2]; int lureGone = 0, lured0 = decoyLured;
            for (int pass = 0; pass < 2; pass++) {
                SK_LAB(SK_COIN, 2); dmMode = 1; SK_PLAYERS();
                NP[1].disp = v3(20, SKY, 105); vf(NP[1].st.pos, NP[1].disp);
                NP[2].disp = v3(80, SKY, 100); vf(NP[2].st.pos, NP[2].disp);
                memset(CO, 0, sizeof CO);
                CO[0].alive = 1; CO[0].owner = myId; CO[0].life = 3; CO[0].netId = 0x01000001; CO[0].pos = vadd(eye_pos(), v3(8, 0, 0));
                if (pass == 0) { CO[1].alive = 1; CO[1].decoy = 1; CO[1].owner = 2; CO[1].life = 3; CO[1].netId = 0x03000001; CO[1].pos = vadd(eye_pos(), v3(70, 0, 0)); }
                fire_rail();
                hpLure[pass] = NP[1].hp; if (pass == 0) lureGone = !CO[1].alive;
            }
            fprintf(f, "mirage => piece : ricochet de ma piece avec un leurre adverse a 62 m et un joueur a 12 m : PV du joueur %.0f, leurre detruit %d, "
                       "ricochets detournes +%d (attendu 100, 1, 1) ; sans le leurre : PV %.0f (attendu 40 : 105 plafonnes a %.0f)\n",
                    hpLure[0], lureGone, decoyLured - lured0, hpLure[1], PVP_HIT_CAP);
            dmMode = 0; memset(NP, 0, sizeof NP);

            /* RICOCHET => MIRAGE : si j'ai RICOCHET, mon rail sur le leurre adverse repart vers son poseur (derriere lui, en vue) */
            float hpRef[2]; int refl0 = decoyReflects, gone2 = 0; char pop2[32] = "";
            for (int pass = 0; pass < 2; pass++) {
                SK_LAB(pass ? SK_COIN : SK_RICOCHET, 1); dmMode = 1; SK_PLAYERS();
                NP[1].disp = v3(20, SKY, 100); vf(NP[1].st.pos, NP[1].disp);
                memset(CO, 0, sizeof CO);
                CO[0].alive = 1; CO[0].decoy = 1; CO[0].owner = 1; CO[0].life = 3; CO[0].netId = 0x02000078; CO[0].pos = vadd(eye_pos(), v3(10, 0, 0));
                fire_rail();
                hpRef[pass] = NP[1].hp;
                if (!pass) { gone2 = !CO[0].alive; snprintf(pop2, sizeof pop2, "%s", popText); }
            }
            fprintf(f, "ricochet => mirage : avec RICOCHET, mon rail sur le leurre de Poseur : PV du poseur %.0f, leurre detruit %d, renvois +%d, \"%s\" "
                       "(attendu 40, 1, 1, LEURRE RENVOYE !) ; avec PIECE : PV %.0f (attendu 100 : absorbe)\n",
                    hpRef[0], gone2, decoyReflects - refl0, pop2, hpRef[1]);
            dmMode = 0; memset(NP, 0, sizeof NP);

            /* PIECE => RICOCHET (lecture par defaut, COIN_VS_RIC) : mon tir en rebond qui touche la piece adverse repart vers moi, au lieu de la voler */
            {
                float hpBack[2], hpFoe[2]; int cb0 = coinBacks, goneBack = 0;
                for (int pass = 0; pass < 2; pass++) {    /* pass 0 : fenetre RICOCHET ouverte ; pass 1 : fermee (contre-jeu habituel : je vole la piece) */
                    SK_LAB(SK_RICOCHET, 1); dmMode = 1; SK_PLAYERS();
                    NP[1].disp = v3(20, SKY, 105); vf(NP[1].st.pos, NP[1].disp);
                    memset(CO, 0, sizeof CO);
                    CO[0].alive = 1; CO[0].owner = 1; CO[0].life = 3; CO[0].netId = 0x02000091; CO[0].pos = vadd(eye_pos(), v3(8, 0, 0));
                    if (pass == 0) skill_tick(TICK, 1, 1);
                    fire_rail();
                    hpBack[pass] = NP[0].hp; hpFoe[pass] = NP[1].hp; if (pass == 0) goneBack = !CO[0].alive;
                }
                fprintf(f, "piece => ricochet : ma fenetre RICOCHET ouverte, mon rail dans la piece de Cible : mes PV %.0f, ceux de Cible %.0f, piece detruite %d, "
                           "renvois +%d (attendu 40, 100, 1, 1 : 70 plafonnes a %.0f) ; fenetre fermee : mes PV %.0f, ceux de Cible %.0f (attendu 100, 40 : "
                           "je vole la piece)\n", hpBack[0], hpFoe[0], goneBack, coinBacks - cb0, PVP_HIT_CAP, hpBack[1], hpFoe[1]);
                dmMode = 0; memset(NP, 0, sizeof NP);

                SK_LAB(SK_RICOCHET, 1); dmMode = 1; SK_PLAYERS(); P.weapon = WP_ROCKET; P.ammo[WP_ROCKET] = 4;   /* roquette en rebond dans la piece adverse */
                NP[1].disp = v3(60, SKY, 100); vf(NP[1].st.pos, NP[1].disp);   /* hors du chemin, hors de portee du souffle */
                memset(CO, 0, sizeof CO); memset(PR, 0, sizeof PR);
                CO[0].alive = 1; CO[0].owner = 1; CO[0].life = 3; CO[0].netId = 0x02000092; CO[0].pos = vadd(eye_pos(), v3(10, 0, 0));
                skill_tick(TICK, 1, 1);
                int cb1 = coinBacks;
                fire_rocket();
                for (int i = 0; i < 3 * 120 && PR[0].alive; i++) proj_tick(TICK);
                fprintf(f, "piece => ricochet : ma roquette en rebond dans la piece de Cible : piece detruite %d, renvois +%d, roquette finie %d, mes PV %.0f, "
                           "ceux de Cible %.0f (attendu 1, 1, 1, 70 a 85 : 30 x f sur moi, 100)\n", !CO[0].alive, coinBacks - cb1, !PR[0].alive,
                        NP[0].hp, NP[1].hp);
                memset(PR, 0, sizeof PR); hitstop = 0;
                dmMode = 0; memset(NP, 0, sizeof NP);
            }

            /* TESLA x ALL IN ONE : un eclair instantane sur la cible du viseur (drone decale de 7 deg : l'arc normal le rate), toute la batterie */
            {
                SK_LAB(SK_ALLIN, 1); P.weapon = WP_TESLA; P.ammo[WP_TESLA] = 100; SK_DRONE(20, 102.5f);
                tesla_pulse(); tesla_flush();             /* l'arc normal rate (cible a 2,5 m du viseur, marge de 1,3 m) */
                float arc = 1000 - E[0].hp;
                P.ammo[WP_TESLA] = 100; P.fireCd = 0; E[0].hp = 1000;
                skill_tick(TICK, 1, 1);
                fprintf(f, "tesla all in one : arc normal sur le drone decale %.0f (attendu 0) ; eclair %.0f (attendu %.0f = batterie 100), batterie %d, charges %.0f, "
                           "\"%s\" (attendu 0, 0, \"ECLAIR !\")\n", arc, 1000 - E[0].hp, TESLA_BOLT_DMG, P.ammo[WP_TESLA], P.skillCh, popText);

                SK_LAB(SK_ALLIN, 1); P.weapon = WP_TESLA; P.ammo[WP_TESLA] = 50; SK_DRONE(20, 102.5f);   /* batterie 50 : moitie des degats ; + une chaine */
                E[1].alive = 1; E[1].type = EN_DRONE; E[1].hp = 1000; E[1].pos = v3(20, eye_pos().y, 106.5f);
                skill_tick(TICK, 1, 1);
                fprintf(f, "tesla all in one : batterie 50 -> drone %.0f (attendu %.0f), 2e drone a 4 m (chaine) %.0f (attendu %.0f)\n",
                        1000 - E[0].hp, TESLA_BOLT_DMG * 0.5f, 1000 - E[1].hp, TESLA_BOLT_DMG * 0.5f * TESLA_CHAIN_MULT);
                E[1].alive = 0;

                SK_LAB(SK_ALLIN, 1); P.weapon = WP_TESLA; P.ammo[WP_TESLA] = TESLA_BOLT_MIN - 1; SK_DRONE(20, 100);
                skill_tick(TICK, 1, 1);
                char r1[40]; snprintf(r1, sizeof r1, "%s", popText);
                float c1 = P.skillCh; int b1 = P.ammo[WP_TESLA], hp1 = E[0].hp < 1000;
                SK_LAB(SK_ALLIN, 1); P.weapon = WP_TESLA; P.ammo[WP_TESLA] = 100; SK_DRONE(20, 94);   /* a 17 degres du viseur : hors du cone de 10 */
                skill_tick(TICK, 1, 1);
                fprintf(f, "tesla all in one : refus batterie %d \"%s\" charges %.0f touche %d ; hors du cone \"%s\" charges %.0f batterie %d "
                           "(attendu %d, ALL IN ONE : BATTERIE < %d, 1, 0 ; ALL IN ONE : PAS DE CIBLE, 1, 100)\n",
                        b1, r1, c1, hp1, popText, P.skillCh, P.ammo[WP_TESLA], TESLA_BOLT_MIN - 1, TESLA_BOLT_MIN);

                SK_LAB(SK_ALLIN, 1); dmMode = 1; SK_PLAYERS(); P.weapon = WP_TESLA; P.ammo[WP_TESLA] = 100;   /* PvP : x0,7, plafond de 60 */
                NP[1].disp = v3(20, SKY, 102.5f); vf(NP[1].st.pos, NP[1].disp);
                skill_tick(TICK, 1, 1);
                fprintf(f, "tesla all in one PvP : PV de la cible %.0f (attendu 40 : %.0f x 0,7 plafonnes a %.0f)\n", NP[1].hp, TESLA_BOLT_DMG, PVP_HIT_CAP);
                dmMode = 0; memset(NP, 0, sizeof NP);
            }

            /* ROQUETTE x PIECE (comme Ultrakill) : la roquette qui traverse ma piece part vers le drone hors de la ligne, x1,5 */
            {
                float dd[2]; int rc0 = rocketCoin, coinGone = 0;
                for (int pass = 0; pass < 2; pass++) {
                    SK_LAB(SK_COIN, 2); P.weapon = WP_ROCKET; P.ammo[WP_ROCKET] = 4;
                    memset(CO, 0, sizeof CO); memset(PR, 0, sizeof PR);
                    if (pass == 0) { CO[0].alive = 1; CO[0].owner = myId; CO[0].life = 4; CO[0].pos = vadd(eye_pos(), v3(10, 0, 0)); SK_DRONE(12, 93); }
                    else SK_DRONE(12, 100);               /* temoin : le meme drone, tir direct */
                    fire_rocket();
                    for (int i = 0; i < 3 * 120 && PR[0].alive; i++) proj_tick(TICK);
                    dd[pass] = 1000 - E[0].hp; if (pass == 0) coinGone = !CO[0].alive;
                    hitstop = 0;
                }
                fprintf(f, "roquette x piece : drone hors de la ligne, degats %.0f, piece consommee %d, redirigees +%d (attendu 1) ; tir direct %.0f ; "
                           "rapport %.2f (attendu 1,50 = COIN_MULT)\n", dd[0], coinGone, rocketCoin - rc0, dd[1], dd[1] > 0 ? dd[0] / dd[1] : 0.0f);
                memset(PR, 0, sizeof PR);
            }

            /* reseau : EV_RAIL sous-type 2 (un seul evenement pour tous les rebonds), EV_HIT W_SHOTGUN (poussee), EV_SHOT avec rebonds */
            {
                SK_LAB(SK_RICOCHET, 1); P.yaw = PI - 0.5f; dmMode = 1; SK_PLAYERS();
                skill_tick(TICK, 1, 1);
                memset(BM, 0, sizeof BM);
                int q0 = outqHead;
                fire_rail();
                int own = 0, remote = 0, nev = 0, act = 0;
                for (int i = 0; i < MAXBM; i++) own += BM[i].life > 0;
                for (int k = 0; k < OUTQ; k++) {
                    int idx = (q0 + k) % OUTQ;
                    if (k < (outqHead - q0 + OUTQ) % OUTQ && outq[idx].ev.kind == EV_RAIL && outq[idx].ev.sub == 2) {
                        nev++; act = outq[idx].ev.actor;
                        memset(BM, 0, sizeof BM);
                        apply_event(&outq[idx].ev);
                        for (int i = 0; i < MAXBM; i++) remote += BM[i].life > 0;
                    }
                }
                fprintf(f, "ricochet reseau : %d evenement EV_RAIL sous-type 2 pour %d rebonds, traits chez le tireur %d, recalcules chez les autres %d "
                           "(attendu 1, 5, 6, 5)\n", nev, act, own, remote);
                dmMode = 0; memset(NP, 0, sizeof NP);

                SK_LAB(SK_COIN, 2);                        /* EV_HIT W_SHOTGUN : poussee forcee de la victime, vitesse intacte ; autre arme : vitesse ajoutee */
                P.pos = P.prevPos = v3(0, SKY, 100); P.grounded = 1; P.vel = v3(0, 0, 0);
                NetEvent hv = ev_new(EV_HIT);
                hv.from = 1; hv.target = (unsigned char)myId; hv.sub = W_SHOTGUN; vf(hv.f + 1, v3(0, 0, 50));
                apply_event(&hv);
                float pt0 = P.pushT, vs0 = vlen(P.vel);
                P.pushT = 0;
                hv.sub = W_ROCKET; vf(hv.f + 1, v3(0, 0, 10));
                apply_event(&hv);
                fprintf(f, "shotgun recul recu : EV_HIT 50 m/s -> poussee %.2f s, vitesse %.1f (attendu %.2f, 0.0) ; EV_HIT roquette 10 m/s -> vitesse %.1f, "
                           "poussee %.2f (attendu 10.0, 0.00)\n", pt0, vs0, SG_KNOCK_T, vlen(P.vel), P.pushT);
                P.vel = v3(0, 0, 0); P.pushT = 0;
            }
            nowT = nt0;
#undef SK_PLAYERS
        }
#undef SK_LAB
#undef SK_DRONE
        nb = nb0;
        memset(CO, 0, sizeof CO); memset(PR, 0, sizeof PR); memset(E, 0, sizeof E);
        reset_game();
        botLog = savedLog;
    }

    {   /* mode classe : Elo (meme formule que worker/src/ratings.js), paliers, placement, identite, velocity.ini, JSON */
        FILE *savedLog = botLog;
        botLog = NULL;                                    /* pas de journal au milieu des resultats */
        int o1000[2] = { 1000, 1000 }, o1200 = 1200;
        float s1 = 1, s0[2] = { 0, 0 };
        fprintf(f, "classe Elo : 1000 bat 1000 %+d, placement %+d, perd contre 1200 %+d (attendu %+d %+d -8)\n",
                rank_delta(1000, 20, 1, o1000, &s1), rank_delta(1000, 0, 1, o1000, &s1), rank_delta(1000, 20, 1, &o1200, s0),
                (int)RANK_K / 2, (int)RANK_K_PLACE / 2);
        fprintf(f, "classe abandon : defaite contre 2 joueurs a 1000 %+d, en placement %+d (attendu %+d %+d)\n",
                rank_delta(1000, 20, 2, o1000, s0), rank_delta(1000, 3, 2, o1000, s0), -(int)RANK_K / 2, -(int)RANK_K_PLACE / 2);
        char t[6][32], pl[32];
        int ms[6] = { 899, RANK_ARGENT, 1000, RANK_OR, 1349, RANK_MAITRE };
        for (int k = 0; k < 6; k++) rank_name(ms[k], t[k], sizeof t[k]);
        fprintf(f, "classe paliers : %s | %s | %s | %s | %s | %s (attendu BRONZE I | ARGENT III | ARGENT I | OR III | PLATINE I | MAITRE)\n",
                t[0], t[1], t[2], t[3], t[4], t[5]);
        rank_label(1000, 3, pl, sizeof pl);
        fprintf(f, "classe placement : %s, K %.0f au 10e match puis %.0f (attendu PLACEMENT 3/10, 64 puis 32)\n", pl,
                rank_k(RANK_PLACEMENT - 1), rank_k(RANK_PLACEMENT));

        char sid[33], skey[33];                           /* identite : 2 x 128 bits, hexadecimal minuscule */
        int sm = myMmr, sp = rankPlayed, sw = rankWins, savedBot = botMode;
        memcpy(sid, rkId, 33); memcpy(skey, rkKey, 33);
        rkId[0] = rkKey[0] = 0;
        int made = rk_identity(), again = rk_identity();
        char id1[33];
        memcpy(id1, rkId, 33);
        fprintf(f, "classe identite : creee=%d, recreee au 2e appel=%d, format=%d, cle differente=%d (attendu 1 0 1 1)\n", made, again,
                hex_ok(rkId, 32) && hex_ok(rkKey, 32), strcmp(rkId, rkKey) != 0);
        botMode = 0;                                      /* velocity.ini de test (jamais le vrai) */
        snprintf(cfgPath, sizeof cfgPath, "cfg_rank_test.ini");
        myMmr = 1234; rankPlayed = 12; rankWins = 5;
        cfg_save();
        myMmr = RANK_START; rankPlayed = rankWins = 0; rkId[0] = rkKey[0] = 0;
        cfg_load();
        fprintf(f, "classe velocity.ini : cote %d, matchs %d, victoires %d, identite relue %d (attendu 1234 12 5 1)\n", myMmr,
                rankPlayed, rankWins, !strcmp(rkId, id1) && hex_ok(rkKey, 32));
        remove(cfgPath);
        cfgPath[0] = 0;
        botMode = savedBot;

        {                                                 /* connexion Discord : reponse du serveur, pseudo, velocity.ini */
            char sId[33], sKey[33], sName[16], sDc[33];
            int sMm = myMmr, sPl = rankPlayed, sWi = rankWins;
            memcpy(sId, rkId, 33); memcpy(sKey, rkKey, 33); memcpy(sName, myName, 16); memcpy(sDc, rkDiscord, 33);
            snprintf(myName, sizeof myName, "Avant");
            int bad = dc_take("{\"ok\":true,\"state\":\"done\",\"id\":\"xyz\",\"key\":\"k\",\"name\":\"Pirate\"}");
            char afterBad[16];
            memcpy(afterBad, myName, 16);
            int good = dc_take("{\"ok\":true,\"state\":\"done\",\"id\":\"0123456789abcdef0123456789abcdef\","
                               "\"key\":\"fedcba9876543210fedcba9876543210\",\"name\":\"Dana\",\"discord\":\"dana<b>_dc\"}");
            fprintf(f, "discord : reponse invalide %d (pseudo %s), valide %d, identite %.6s, pseudo %s, compte %s (attendu 0 (Avant), 1, 012345, Dana, danab_dc)\n",
                    bad, afterBad, good, rkId, myName, rkDiscord);
            dc_take("{\"ok\":true,\"state\":\"done\",\"id\":\"0123456789abcdef0123456789abcdef\","
                    "\"key\":\"fedcba9876543210fedcba9876543210\",\"name\":\"a b\",\"discord\":\"dana_dc\"}");
            int keep = !strcmp(myName, "Dana");
            botMode = 0;                                  /* velocity.ini de test (jamais le vrai) */
            snprintf(cfgPath, sizeof cfgPath, "cfg_dc_test.ini");
            cfg_save();
            rkDiscord[0] = 0;
            cfg_load();
            fprintf(f, "discord : pseudo invalide ignore %d, compte relu de velocity.ini %s (attendu 1, dana_dc)\n", keep, rkDiscord);
            remove(cfgPath);
            cfgPath[0] = 0;
            botMode = savedBot;
            memcpy(rkId, sId, 33); memcpy(rkKey, sKey, 33); memcpy(myName, sName, 16); memcpy(rkDiscord, sDc, 33);
            myMmr = sMm; rankPlayed = sPl; rankWins = sWi;
        }
        {                                                 /* champ PSEUDO verrouille : double-clic pour le modifier */
            int uf = uiFocus;
            uiFocus = 0; fieldLastId = 0;
            int c1 = field_click(1, 1000), c2 = field_click(1, 1300);
            uiFocus = 0;
            int c3 = field_click(1, 5000), c4 = field_click(1, 5600), c5 = field_click(2, 9000);
            uiFocus = 1;
            int c6 = field_click(1, 20000);
            fprintf(f, "menus : PSEUDO 1er clic %d, double-clic %d, clics lents %d %d, champ IP %d, deja en saisie %d (attendu 0 1, 0 0, 1, 1)\n",
                    c1, c2, c3, c4, c5, c6);
            uiFocus = uf; fieldLastId = 0;
        }

        rk_parse_top("{\"ok\":true,\"top\":[{\"name\":\"Alice\",\"mmr\":1420,\"tier\":\"PLATINE III\",\"played\":31},"
                     "{\"name\":\"Bob\",\"mmr\":1301,\"tier\":\"PLATINE III\",\"played\":12}],"
                     "\"me\":{\"rank\":7,\"mmr\":1093,\"played\":14,\"wins\":6,\"tier\":\"OR II\"}}");
        fprintf(f, "classe top : %d joueurs, 1er %s %d, 2e %s, mon rang %d, ma cote %d (attendu 2, Alice 1420, Bob, 7, 1093)\n",
                rkTopN, rkTop[0].name, rkTop[0].mmr, rkTop[1].name, rkMeRank, myMmr);

        /* rapport de fin : 3 joueurs, le 3e est parti (liste de l'hote) ; le parti revenu ne figure qu'une fois */
        MatchRules savedRules = rules;
        int savedTeams = teamSize;
        memset(NP, 0, sizeof NP);
        ranked = 1; teamSize = 0; rules = (MatchRules){ MODE_FRAGS, 1, AMMO_NORMAL, 0, 10 };
        myId = 1; myMmr = 1000; rankPlayed = 20;
        for (int i = 0; i < 2; i++) { NP[i].active = 1; rkRid[i][0] = (unsigned char)(0x10 + i); rkMmr[i] = 1000; }
        NP[0].kills = 15; NP[1].kills = 9; matchWinner = 0;
        hex_of(rkRid[1], 16, rkId);
        rkLeftN = 2; memset(rkLeft, 0, sizeof rkLeft); rkLeft[0][0] = 0x33; rkLeft[1][0] = 0x10;   /* 0x10 = l'hote, present */
        rkQN = 0;
        rk_send_report(0);
        const char *rb = rkQN ? rkQ[0].body : "";
        fprintf(f, "classe rapport : envoye=%d, gagnant=hote %d, presents=%d, partis=%d, estimation %+d (attendu 1 1 1 1 +0)\n",
                rkQN == 1, strstr(rb, "\"winner\":\"10") != NULL, strstr(rb, "\"kills\":15") && strstr(rb, "\"kills\":9"),
                strstr(rb, "\"left\":[\"33") != NULL && !strstr(rb, "\"left\":[\"33000000000000000000000000000000\",\""), rkEstimate);
        rk_send_report(1);                                /* l'hote part en plein match : il rejoint les partis, pas de gagnant */
        rb = rkQN > 1 ? rkQ[1].body : "";
        fprintf(f, "classe hote parti : sans gagnant=%d, hote une fois dans les partis=%d (attendu 1 1)\n", strstr(rb, "\"winner\":\"\"") != NULL,
                strstr(rb, "\"left\":[\"33000000000000000000000000000000\",\"10000000000000000000000000000000\"]") != NULL);
        rkRid[2][0] = 0x33; rkMmr[2] = 1400; rkQN = 0;      /* le parti a garde sa place (inactive) : sa cote annoncee sert */
        rk_send_report(0);
        fprintf(f, "classe rapport : estimation avec la cote du parti (1400) %+d (attendu +7, comme le serveur)\n", rkEstimate);
        rkRid[2][0] = 0; rkMmr[2] = 0;
        /* aucun rapport hors chacun pour soi FRAGS (equipes, autre mode) ni hors partie classee */
        rkQN = 0;
        teamSize = 2; rk_send_report(0);
        int qTeams = rkQN;
        teamSize = 0; rules.mode = MODE_CHRONO; rk_send_report(0);
        int qMode = rkQN;
        rules.mode = MODE_FRAGS; ranked = 0; rk_send_report(0);
        int qOff = rkQN;
        fprintf(f, "classe sans rapport : equipes %d, mode CHRONO %d, partie non classee %d (attendu 0 0 0)\n", qTeams, qMode, qOff);

        /* une partie hebergee classee impose ses regles, quoi qu'on ait choisi dans HEBERGER */
        int sWant = wantTeams, sMenu = menuMap, sQuick = hostQuick, sFfa = hostFfa, sFrag = fragLimit, sTest = rkFragsTest;
        MatchRules sWantR = wantRules;
        int rm = race_map();
        wantTeams = 2; wantRules = (MatchRules){ MODE_CHRONO, 0, AMMO_INFINI, 1, 5 }; menuMap = rm;
        hostQuick = 1; hostFfa = 0; rkFragsTest = 0; ranked = 1;
        int hm = host_setup_rules();
        fprintf(f, "classe regles : equipes %d, mode %s, grappin %d, munitions %s, boutique %d, %d min, %d frags, "
                   "carte de course evitee %d (attendu 0, FRAGS, 1, NORMALES, 0, 10 min, %d frags, 1)\n",
                teamSize, modeNames[rules.mode], rules.hook, ammoRuleNames[rules.ammo], rules.shop, rules.minutes,
                fragLimit, hm != rm && rk_map_ok(hm), FRAG_LIMIT);
        ranked = 0; hostQuick = 0; menuMap = 0;
        host_setup_rules();                               /* sans classe : les choix de HEBERGER restent */
        fprintf(f, "classe regles (non classee) : equipes %d, mode %s, boutique %d (attendu 2, CHRONO, 1) ; rotation classee "
                   "apres la carte avant la course -> %s (attendu une carte sans portes)\n",
                teamSize, modeNames[rules.mode], rules.shop, rk_map_ok(rk_map_pick(rm)) ? "sans portes" : maps[rk_map_pick(rm)].name);
        wantTeams = sWant; wantRules = sWantR; menuMap = sMenu; hostQuick = sQuick; hostFfa = sFfa; fragLimit = sFrag;
        rkFragsTest = sTest;

        rkQN = 0; rkRepState = RS_NONE; rkTopN = 0; rkMeRank = 0; rkLeftN = 0; myId = 0; ranked = 0;
        rules = savedRules; teamSize = savedTeams;
        memset(NP, 0, sizeof NP); memset(rkRid, 0, sizeof rkRid); memset(rkMmr, 0, sizeof rkMmr); memset(rkLeft, 0, sizeof rkLeft);
        memcpy(rkId, sid, 33); memcpy(rkKey, skey, 33);
        myMmr = sm; rankPlayed = sp; rankWins = sw;
        botLog = savedLog;
    }
    {   /* fusion : 2e ecran opt-in ; protocole */
        fprintf(f, "second ecran : actif par defaut %d (attendu 0)\n", secondScreen);
        PkAnnounce pa;
        fprintf(f, "protocole : NET_VER %d, PkAnnounce %d octets avec ranked a l'octet %d (attendu 22, %d, %d)\n", NET_VER,
                (int)sizeof(PkAnnounce), (int)((unsigned char *)&pa.ranked - (unsigned char *)&pa), (int)sizeof(PkHead) + 16 + 8, (int)sizeof(PkHead) + 20);
        double sNow = nowT;                               /* chargement de carte : l'arrivant a NET_LOAD_GRACE de plus */
        nowT = 100;
        double joinedAt = nowT + NET_LOAD_GRACE;          /* comme host_on_join */
        nowT = 100 + NET_TIMEOUT + 3; int okLoad = !net_silent(joinedAt);
        nowT = 100 + NET_LOAD_GRACE + NET_TIMEOUT + 1; int goneLate = net_silent(joinedAt);
        nowT = 100 + NET_TIMEOUT + 1; int goneNormal = net_silent(100);
        fprintf(f, "reseau : arrivant qui charge 8 s garde %d, coupe apres la grace %d, joueur muet 6 s coupe %d (attendu 1 1 1)\n",
                okLoad, goneLate, goneNormal);
        {   /* horloge de match : temps reel chez l'hote, calage du client sur le serveur ou sur l'hote */
            float sMatch = matchT; double sOff = clkSrvOff, sRtt = clkSrvRtt, sHost = clkHostOff, sClk = matchClkT;
            int sOk = clkSrvOk, sN = clkSamples, sHOk = clkHostOk, sHSrv = clkHostSrv;
            nowT = 200; match_clock_start();
            nowT += 0.2; for (int i = 0; i < 12; i++) match_clock_tick();   /* 5 images/s : 12 ticks pour 0,2 s reelles */
            float slow = matchT;
            nowT += 0.3; float before = matchT;                 /* hitstop : pas de tick pendant 0,3 s, rattrape au suivant */
            match_clock_tick(); float hit = matchT - before;
            nowT += 6; match_clock_tick(); float load = matchT - before - hit;   /* chargement de carte : 1 s au plus */
            clkSrvOk = 0; clkSamples = 0;
            clk_srv_sample(300.0, 300.2, 5000.0e3 + 100);        /* aller-retour 200 ms */
            clk_srv_sample(310.0, 310.04, 5010.0e3 + 20);        /* 40 ms : plus juste, gardee */
            clk_srv_sample(320.0, 320.5, 5020.0e3 + 999);        /* 500 ms : ignoree */
            double off = clkSrvOff;                               /* 5010,02 - (310 + 310,04) / 2 = 4700 */
            nowT = 400; clkHostOk = 0;                            /* les deux cales : instantane parti il y a 50 ms (heure serveur) */
            float both = clk_match_from_snap(100, nowT + clkSrvOff - 0.05, 1, 0);
            clkHostOk = 0;                                        /* LAN : heure de l'hote = la mienne + 1000 s, trajets 30, 10 puis 30 ms */
            clk_match_from_snap(0, 1000 + nowT - 0.03, 0, 0); clk_match_from_snap(0, 1000 + nowT - 0.01, 0, 0);
            float lan = clk_match_from_snap(100, 1000 + nowT - 0.03, 0, 0);
            float over = clk_match_from_snap(100, 1000 + nowT - 0.03, 0, 1);
            fprintf(f, "horloge de match : 0,2 s a 5 images/s -> %.2f, apres hitstop 0,3 s +%.2f, chargement 6 s +%.2f, "
                       "calage serveur %.2f (rtt %.0f ms), client cale %.2f, LAN %.2f, fini %.0f (attendu 0.20, 0.30, 1.00, 4700.00 (42 : 40 + 2 de vieillissement), 100.05, 100.02, 100)\n",
                    slow, hit, load, off, clkSrvRtt * 1000, both, lan, over);
            matchT = sMatch; clkSrvOff = sOff; clkSrvRtt = sRtt; clkHostOff = sHost; matchClkT = sClk;
            clkSrvOk = sOk; clkSamples = sN; clkHostOk = sHOk; clkHostSrv = sHSrv;
        }
        nowT = sNow;
    }

    botLog = NULL;
    netMode = NET_OFF; dmMode = 0;
    vagues_tests(f);                                      /* mode VAGUES : src/tests/vagues_tests.c */
    fclose(f);
}
