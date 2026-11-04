/* VELOCITY - tests automatiques du mode VAGUES (appeles par run_tests, src/tests/tests.c)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* partie VAGUES neuve sur la carte m, sans entrees, sans hitstop */
static void vg_test_begin(int m) {
    dmMode = 0; netMode = NET_OFF; raceOn = 0;
    vagOn = 1; vgStartMap = m;
    if (curMap != m) load_map(m);
    reset_game();
    hitstop = 0; vgMenu = VGM_NONE;
    memset(act, 0, sizeof act); memset(actP, 0, sizeof actP);
    memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
}
static int vg_test_enemy(int t, V3 pos, float hp) {      /* ennemi pose a la main, immobile */
    int i = spawn_enemy_at(t, pos, 0);
    if (i >= 0) { E[i].hp = E[i].maxHp = hp; E[i].vel = v3(0, 0, 0); E[i].fireT = 100; E[i].elite = 0; }
    return i;
}
static void vg_test_kill_all(void) {
    for (int i = 0; i < MAXE; i++) if (E[i].alive) { E[i].hp = 0; kill_enemy(i, 0); }
}

static void vagues_tests(FILE *f) {
    float sNow = (float)nowT;
    {   /* courbe : nombre d'ennemis, PV (seuls a monter, x1,5 au plus), elites */
        fprintf(f, "vagues : ennemis aux vagues 1/33/50/66/100 = %d %d %d %d %d (attendu 3 14 21 26 30)\n",
                vg_count(1), vg_count(33), vg_count(50), vg_count(66), vg_count(100));
        fprintf(f, "vagues : PV x%.2f x%.2f x%.2f x%.2f aux vagues 1/66/100/180, elites %.2f %.2f a 50/100 (attendu 1.00 1.00 1.50 1.50, 0.00 0.60)\n",
                vg_hp_mul(1), vg_hp_mul(66), vg_hp_mul(100), vg_hp_mul(180), vg_elite_p(50), vg_elite_p(100));
        int bosses = vg_is_boss(25) + vg_is_boss(50) + vg_is_boss(75) + vg_is_boss(100) + vg_is_boss(30);
        fprintf(f, "vagues : boss aux vagues 25/50/75/100 = %d (types %d %d %d %d, 125 -> %d) (attendu 4, %d %d %d %d, %d)\n",
                bosses, vg_boss_type(25), vg_boss_type(50), vg_boss_type(75), vg_boss_type(100), vg_boss_type(125),
                EN_PYRA, EN_TESS, EN_BALL, EN_HEXA, EN_PYRA);
    }
    {   /* les degats des ennemis ne montent jamais : une orbe a la vague 1 et a la vague 95, PV d'un drone */
        float hit[2], hp[2];
        for (int k = 0; k < 2; k++) {
            vg_test_begin(0);
            vg_wave_start(k ? 95 : 1);
            memset(E, 0, sizeof E); vgQn = vgQi = 0;
            P.pos = P.prevPos = v3(-20, 0, 30);
            spawn_proj(PR_ORB, vadd(pcenter(), v3(0, 0, -3)), v3(0, 0, 16));
            for (int i = 0; i < 60; i++) proj_tick(TICK);
            hit[k] = 100 - P.hp;
            hp[k] = E[spawn_enemy_type(EN_DRONE)].maxHp;
        }
        fprintf(f, "vagues : orbe recue vague 1=%.0f vague 95=%.0f, drone vague 1=%.0f vague 95=%.0f (attendu 12 12, 100 143)\n",
                hit[0], hit[1], hp[0], hp[1]);
    }
    {   /* deroule : vagues qui s'enchainent, selecteur apres la 10, choix d'une carte difficile */
        vg_test_begin(0);
        int w0 = 0, wMax = 0;
        for (int i = 0; i < 120 * 40 && wave < 3; i++) {
            nowT += TICK; P.hp = 100;
            game_tick(TICK); hitstop = 0;
            if (wave == 1 && !w0) w0 = vgQn;
            if (i % 30 == 0) vg_test_kill_all();
            if (wave > wMax) wMax = wave;
        }
        fprintf(f, "vagues : vague 1 de %d ennemis, vague atteinte en jouant=%d (attendu 3, 3)\n", w0, wMax);
        wave = 10; vgState = VG_FIGHT; vgQn = vgQi = 0; vg_test_kill_all();
        wave_tick(TICK);
        int sel = vgMenu == VGM_SELECT, ch0 = vgChoiceMap[0], ch1 = vgChoiceMap[1];
        vgPend = 1; vg_menu_close();
        wave_tick(TICK);
        fprintf(f, "vagues : apres la vague 10 selecteur=%d (simple %s, difficile %s), choix difficile -> carte %s credits x%.1f (attendu 1, %s, LE CANYON)\n",
                sel, maps[ch0].name, maps[ch1].name, maps[curMap].name, vgBlockHard ? VG_HARD_CREDITS : 1.0f,
                maps[ch0].name);
        int c0 = vgCredits; kill_enemy(vg_test_enemy(EN_DRONE, v3(0, 10, 0), 10), 1);
        int hard = vgCredits - c0;
        vgBlockHard = 0; c0 = vgCredits; kill_enemy(vg_test_enemy(EN_DRONE, v3(0, 10, 0), 10), 1);
        int simple = vgCredits - c0;
        int s = vg_test_enemy(EN_DRONE, v3(0, 10, 0), 10); E[s].summoned = 1;
        c0 = vgCredits; kill_enemy(s, 1);
        fprintf(f, "vagues : credits d'un drone simple=%d difficile=%d invoque=%d (attendu 10 15 3)\n", simple, hard, vgCredits - c0);
    }
    {   /* boutique : etapes dans l'ordre, prix, et aucun achat ne change les degats d'une touche */
        vg_test_begin(0);
        vgCredits = 1000;
        int a = up_buy(WP_RAIL), b = up_buy(WP_RAIL), lv = vgLvl[WP_RAIL], left = vgCredits;
        vgCredits = 100; int poor = up_buy(WP_ROCKET);
        float d[2];
        for (int k = 0; k < 2; k++) {
            vgLvl[WP_RAIL] = k ? 5 : 0;
            memset(E, 0, sizeof E);
            int i = vg_test_enemy(EN_DRONE, v3(0, 10, 0), 1000);
            vgSrc = WP_RAIL; damage_enemy(i, 100, v3(0, 0, 0)); vgSrc = -1;
            d[k] = 1000 - E[i].hp;
        }
        fprintf(f, "boutique vagues : achats %d %d niveau %d reste %d, sans assez %d, touche au rail etape 0=%.0f etape 5=%.0f (attendu 1 1 2 640 0, 100 100)\n",
                a, b, lv, left, poor, d[0], d[1]);
    }
    {   /* effets : FOURCHE, GEL, FRACTURE, ONDE de parry, PROPULSION */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        vgLvl[WP_RAIL] = 4;                               /* FOURCHE, GEL, SATELLITE, FRACTURE */
        int v = vg_test_enemy(EN_DRONE, v3(-20, 5, 10), 50), n1 = vg_test_enemy(EN_DRONE, v3(-16, 5, 10), 500),
            n2 = vg_test_enemy(EN_DRONE, v3(-20, 5, 14), 500);
        vgSrc = WP_RAIL; damage_enemy(v, 100, v3(0, 0, 0));
        int fork = (E[n1].hp < 500) + (E[n2].hp < 500);
        float hp1 = E[n1].hp;
        damage_enemy(n1, 10, v3(0, 0, 0));
        int frozen = E[n1].frozenT > 0;
        damage_enemy(n1, 10, v3(0, 0, 0)); damage_enemy(n1, 10, v3(0, 0, 0));
        int frac = fxStat[FX_FRACTURE] > 0 && E[n1].hp < hp1 - 30 - 40;
        vgSrc = -1;
        vgLvl[UP_MELEE(MK_SWORD)] = 1; P.meleeKind = MK_SWORD;   /* ONDE de parry */
        P.pos = v3(-20, 0, 12); float h2 = E[n2].hp;
        parry_feedback(pcenter());
        int pulse = E[n2].hp < h2;
        vgLvl[WP_SHOTGUN] = 2; P.loadout[1] = WP_SHOTGUN; P.vel = v3(0, 0, 0); P.yaw = 0; P.pitch = 0;
        vg_on_fire(WP_SHOTGUN);
        fprintf(f, "effets vagues : fourche sur %d voisins, gel=%d, fracture=%d, onde de parry=%d, propulsion vz=%.0f (attendu 2, 1, 1, 1, 7)\n",
                fork, frozen, frac, pulse, P.vel.z);
    }
    {   /* bastion : bouclier de face, nu de dos */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        P.pos = v3(0, 0, 30);
        int b = vg_test_enemy(EN_BASTION, v3(0, 1.4f, 20), 1000);
        E[b].yaw = 0;                                     /* face vers +z : vers le joueur */
        damage_enemy(b, 100, v3(0, 0, 0)); float front = 1000 - E[b].hp;
        E[b].yaw = PI; damage_enemy(b, 100, v3(0, 0, 0)); float back = 1000 - front - E[b].hp;
        fprintf(f, "bastion : touche de face=%.0f de dos=%.0f (attendu 15 100)\n", front, back);
    }
    {   /* PYRAMIDE : armure, phase de charges a 50 %, charge paree = etourdie, charge encaissee */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        P.pos = P.prevPos = v3(0, 0, 30); P.yaw = 0; P.pitch = 0;
        int p = boss_spawn(EN_PYRA);
        Enemy *e = &E[p];
        float h0 = e->hp; damage_enemy(p, 100, v3(0, 0, 0)); float armor = h0 - e->hp;
        h0 = e->hp; vgProjKind = PR_PARRIED; damage_enemy(p, 100, v3(0, 0, 0)); vgProjKind = -1; float parried = h0 - e->hp;
        for (int i = 0; i < 240; i++) { P.hp = 100; enemy_tick(TICK); boss_fx_tick(TICK); }
        int moved = e->st != BS_MOVE || e->combo > 0 || e->frac != 0;
        e->hp = e->maxHp * 0.49f; enemy_tick(TICK);
        int phase = e->phase;
        memset(PR, 0, sizeof PR);
        e->st = BS_RAM; e->stT = 1.0f; e->pos = vadd(eye_pos(), v3(0, 0, -(enemy_radius(EN_PYRA) + 2.5f)));
        e->aim = v3(0, 0, 1); P.parryWin = 0.1f; h0 = e->hp;
        enemy_tick(TICK);
        int stunned = e->st == BS_STUN; float rammed = h0 - e->hp;
        hitstop = 0;
        h0 = e->hp; damage_enemy(p, 100, v3(0, 0, 0)); float stunDmg = h0 - e->hp;
        e->st = BS_RAM; e->stT = 1.0f; e->pos = vadd(pcenter(), v3(0, 0, -(enemy_radius(EN_PYRA) + 0.3f)));
        e->aim = v3(0, 0, 1); P.parryWin = 0; P.hp = 100;
        enemy_tick(TICK);
        fprintf(f, "pyramide : armure %.0f, orbe paree %.0f, bouge et tire=%d, phase charges a 49%%=%d, charge paree etourdit=%d (-%.0f), etourdie %.0f, charge encaissee -%.0f PV (attendu 35, 100, 1, 1, 1 (-250), 150, 22)\n",
                armor, parried, moved, phase, stunned, rammed, stunDmg, 100 - P.hp);
        vgSecret = 1; e->st = BS_RECOVER; e->stT = 0; e->combo = 3; e->phase = 1;
        int before = enemies_alive();
        enemy_tick(TICK);
        fprintf(f, "pyramide INFINI : PRISME, fantomes apparus=%d (attendu 3)\n", enemies_alive() - before);
        vgSecret = 0;
    }
    {   /* TESSERACT : immobile, invulnerable tant qu'il a 4 invocations, invocations qui arrivent seules */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        P.pos = P.prevPos = v3(0, 0, 40);
        int t = boss_spawn(EN_TESS);
        Enemy *e = &E[t];
        V3 home = e->home;
        e->fracT = 0;
        for (int i = 0; i < 60; i++) { P.hp = 100; enemy_tick(TICK); }
        int links = boss_links(t);
        float h0 = e->hp; damage_enemy(t, 100, v3(0, 0, 0)); float shielded = h0 - e->hp;
        for (int j = 0; j < MAXE; j++) if (E[j].alive && E[j].summoned) kill_enemy(j, 0);
        h0 = e->hp; damage_enemy(t, 100, v3(0, 0, 0)); float open = h0 - e->hp;
        fprintf(f, "tesseract : invocations=%d (>= %d), degats sous bouclier=%.0f, sans invocations=%.0f, deplacement=%.2f, libre dans le decor=%d (attendu >= 4, 0, 100, 0.00, 1)\n",
                links, VG_LINKS, shielded, open, vlen(vsub(e->pos, home)), sphere_free(e->pos, enemy_radius(EN_TESS)));
    }
    {   /* SPHERE : coups depuis le sol presque nuls, onde de choc qui touche au sol et pas en l'air */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        P.pos = P.prevPos = v3(-20, 0, 30);
        int b = boss_spawn(EN_BALL);
        Enemy *e = &E[b];
        P.grounded = 1; float h0 = e->hp; damage_enemy(b, 100, v3(0, 0, 0)); float ground = h0 - e->hp;
        P.grounded = 0; h0 = e->hp; damage_enemy(b, 100, v3(0, 0, 0)); float air = h0 - e->hp;
        memset(GW, 0, sizeof GW);
        gwave_add(v3(-20, 0, 20), 30);
        P.hp = 100; P.pos = v3(-20, 0, 30);
        for (int i = 0; i < 120; i++) { P.pos = P.prevPos = v3(-20, 0, 30); P.vel = v3(0, 0, 0); boss_fx_tick(TICK); }
        float onGround = 100 - P.hp;
        memset(GW, 0, sizeof GW);
        gwave_add(v3(-20, 0, 20), 30);
        P.hp = 100;
        for (int i = 0; i < 120; i++) { P.pos = P.prevPos = v3(-20, 2.5f, 30); P.vel = v3(0, 0, 0); boss_fx_tick(TICK); }
        float jumped = 100 - P.hp;
        for (int i = 0; i < 1200; i++) { P.pos = P.prevPos = v3(-20, 30, 30); P.hp = 100; enemy_tick(TICK); boss_fx_tick(TICK); }
        fprintf(f, "sphere : touche depuis le sol=%.0f en l'air=%.0f, onde au sol=%.0f PV, en sautant=%.0f, bonds=%d (attendu 15, 100, 18, 0, >= 1)\n",
                ground, air, onGround, jumped, bossJumps);
    }
    {   /* HEXAEDRE : quatre phases, armure de la phase PRECISION et coeur ouvert */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        P.pos = P.prevPos = v3(0, 0, 40);
        int h = boss_spawn(EN_HEXA);
        Enemy *e = &E[h];
        float h0 = e->hp; damage_enemy(h, 100, v3(0, 0, 0)); float armor = h0 - e->hp;
        e->st = BS_OPEN; e->stT = 2; h0 = e->hp; damage_enemy(h, 100, v3(0, 0, 0)); float open = h0 - e->hp;
        int ph[3];
        float fr[3] = { 0.7f, 0.4f, 0.2f };
        for (int k = 0; k < 3; k++) { e->hp = e->maxHp * fr[k]; P.hp = 100; enemy_tick(TICK); ph[k] = e->phase; }
        for (int i = 0; i < 1200; i++) { P.hp = 100; enemy_tick(TICK); boss_fx_tick(TICK); }
        fprintf(f, "hexaedre : armure %.0f, coeur ouvert %.0f, phases a 70/40/20 %% = %d %d %d, vivant apres 10 s de phase TOUT=%d (attendu 30, 150, 1 2 3, 1)\n",
                armor, open, ph[0], ph[1], ph[2], e->alive);
    }
    {   /* boss vaincu : ses invocations s'eteignent, la vague se vide */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        int t = boss_spawn(EN_TESS);
        for (int k = 0; k < 3; k++) { int s = vg_test_enemy(EN_DRONE, v3(5.0f * k, 12, 0), 100); E[s].summoned = 1; }
        int c0 = vgCredits;
        E[t].hp = 1; bossBypass = 1; damage_enemy(t, 10, v3(0, 0, 0)); bossBypass = 0;
        fprintf(f, "boss vaincu : ennemis restants=%d, credits +%d, barre du boss=%d (attendu 0, 600, -1)\n", enemies_alive(), vgCredits - c0, vgBoss);
    }
    {   /* vague 100 : temps (point d'entree du speedrun), splits, puis l'INFINI */
        vg_test_begin(0);
        vgSplit[0] = 300; vgSplit[1] = 650; vgSplit[2] = 1000;
        wave = 100; vgState = VG_FIGHT; vgQn = vgQi = 0; memset(E, 0, sizeof E);
        vgRunT = 1234.5f;
        srTicket[SR_VAGUES100] = SRT_WAIT;                /* ticket du depart attendu : le rapport part a son arrivee */
        wave_tick(TICK);
        const SrRun *sr = &srRun[SR_VAGUES100];
        fprintf(f, "vague 100 -> speedrun : rapport en attente du ticket %d, %d ms en %d splits (attendu 1, 1234500 ms en 4 splits)\n",
                sr->state == SRR_WAIT, sr->ms, sr->n);
        srTicket[SR_VAGUES100] = SRT_NONE; srRun[SR_VAGUES100].state = SRR_NONE;
        int sp[4], n = vagues_splits_ms(sp);
        int win = vgState == VG_WIN && vgMenu == VGM_WIN;
        vg_clock(1); float frozenT = vagues_temps_100();
        fprintf(f, "vague 100 : ecran=%d temps=%.1f (fige %.1f) splits %d : %d %d %d %d (attendu 1, 1234.5 (1234.5), 4 : 300000 650000 1000000 1234500)\n",
                win, vagues_temps_100(), frozenT, n, sp[0], sp[1], sp[2], sp[3]);
        vg_go_infinite();
        vgPend = 0; vg_menu_close();
        wave_tick(TICK);
        for (int i = 0; i < (int)(VG_PAUSE_T * 120) + 2; i++) wave_tick(TICK);
        int w101 = wave;
        vgWaveT = 0.05f; vgQi = 0;                        /* chrono de la vague 101 ecoule, ennemis encore la */
        for (int i = 0; i < 12; i++) wave_tick(TICK);
        int w102 = wave, carried = vgQn;
        wave = 124; vgWaveT = 0.01f; vgQn = vgQi = 0; memset(E, 0, sizeof E);
        for (int i = 0; i < 12; i++) wave_tick(TICK);     /* 124 videe : 125 (boss) arrive au bout du chrono */
        int w125 = wave, boss = vgBoss >= 0;
        for (int i = 0; i < 120 * 40; i++) { P.hp = 100; enemy_tick(TICK); wave_tick(TICK); hitstop = 0; }
        fprintf(f, "INFINI : vague %d puis %d au chrono (ennemis en file %d > 0), boss %d=%d qui attend 40 s : vague %d, secret=%d (attendu 101, 102, 125=1, 125, 1)\n",
                w101, w102, carried, w125, boss, wave, vgSecret);
    }
    {   /* temps de partie : les pauses ECHAP ne comptent pas, la boutique oui */
        vg_test_begin(0);
        vgRunT = 0;
        for (int i = 0; i < 120; i++) vg_clock(TICK);
        nowT = 500; vg_menu_set(VGM_SHOP); vg_clock(1); nowT = 503; vg_menu_set(VGM_NONE);
        fprintf(f, "temps vagues : 1 s de jeu + 3 s de boutique = %.2f (attendu 4.00)\n", vgRunT);
    }
    {   /* challenges : defense de zone (reussie en restant dedans), course (arrivee) */
        vg_test_begin(0);
        vgChoiceMap[0] = 0; vgChoiceReward = 500; wave = 20;
        vg_challenge_start(CH_DEFENSE);
        int c0 = vgCredits;
        for (int i = 0; i < 120 * 45 && vgState == VG_CHALLENGE; i++) {
            P.pos = P.prevPos = vg_zone(); P.vel = v3(0, 0, 0); P.hp = 100;
            enemy_tick(TICK); proj_tick(TICK); wave_tick(TICK); hitstop = 0;
        }
        int defOk = vgState == VG_BREAK && vgChalOk, gain = vgCredits - c0;
        vg_challenge_start(CH_DEFENSE);
        for (int i = 0; i < 120 * 101 && vgState == VG_CHALLENGE; i++) {
            P.pos = P.prevPos = v3(-60, 0, -60); P.hp = 100; wave_tick(TICK);   /* hors de la zone */
        }
        int defFail = vgState == VG_BREAK && !vgChalOk;
        vg_challenge_start(CH_COURSE);
        int onRace = raceOn && curMap == race_map();
        raceCount = 0; raceDone = 1;
        wave_tick(TICK);
        fprintf(f, "challenges : defense reussie=%d (+%d), ratee hors zone=%d, course sur LA TOILE=%d, arrivee -> retour %s=%d (attendu 1 (+500), 1, 1, ARENE=1)\n",
                defOk, gain, defFail, onRace, maps[curMap].name, !raceOn && vgChalOk && curMap == 0);
    }
    vagOn = 0; vgMenu = VGM_NONE; raceOn = 0;
    if (curMap != 0) load_map(0);
    reset_game();
    nowT = sNow;
}

/* --- captures --shot vagues_* (appelees par velocity_main) : un boss, la boutique, le selecteur, la fin --- */
static void save_bmp(const char *path);
static void vg_shot_look(V3 at) {                         /* le joueur regarde le point at */
    V3 d = vsub(at, eye_pos());
    P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(clampf(d.y / fmaxf(vlen(d), 0.01f), -1, 1));
}
static int vagues_shot(const char *path) {
    if (!strstr(path, "vagues")) return 0;
    appState = APP_GAME;
    vg_test_begin(0);
    P.pos = P.prevPos = v3(0, 0, 34); P.grounded = 1;
    score = 48250; kills = 212; vgCredits = 1460; vgRunT = 1311.4f; wave = 24; vgState = VG_FIGHT;
    if (strstr(path, "pyramide")) {                       /* vague 25 : grosse orbe en charge ("charge" : phase 2, charge annoncee) */
        wave = 25;
        int b = boss_spawn(EN_PYRA);
        Enemy *e = &E[b];
        e->pos = v3(0, 9, 8); e->vel = v3(0, 0, 0);
        vg_shot_look(vadd(e->pos, v3(0, -1, 0)));
        if (strstr(path, "charge")) { e->phase = 1; e->hp = e->maxHp * 0.42f; e->st = BS_AIM; e->stT = 0.35f; e->aim = vnorm(vsub(pcenter(), e->pos)); }
        else {
            e->hp = e->maxHp * 0.81f; e->st = BS_CHARGE; e->stT = 0.6f;
            for (int k = 0; k < 4; k++) spawn_proj(PR_ORB, vadd(e->pos, v3(-3 + 2.0f * k, -1, 6 + 3.0f * k)), v3(0, -1, 18));
            for (int i = 0; i < 20; i++) { add_part(vadd(e->pos, vmul(rand_dir(), 4)), v3(0, 0, 0), v3(1, 0.8f, 0.3f), 0.4f, 0.18f, 0); }
        }
        snprintf(vgHint, sizeof vgHint, strstr(path, "charge") ? "PARE LA CHARGE AU DERNIER MOMENT" : "GROSSE ORBE : PARE-LA"); vgHintT = 2;
    } else if (strstr(path, "tesseract")) {               /* vague 50 : invocations reliees (bouclier) et anneau d'orbes */
        wave = 50;
        int b = boss_spawn(EN_TESS);
        Enemy *e = &E[b];
        e->fracT = 0; e->fireT = 100;
        for (int i = 0; i < 40; i++) enemy_tick(TICK);
        e->fireT = 0; e->st = 2;                          /* le prochain barrage est l'anneau */
        enemy_tick(TICK);
        for (int i = 0; i < 50; i++) proj_tick(TICK);
        e->hp = e->maxHp * 0.66f;
        P.pos = P.prevPos = v3(-10, 0, 22);
        vg_shot_look(e->pos);
        snprintf(vgHint, sizeof vgHint, "BOUCLIER : TUE SES INVOCATIONS"); vgHintT = 2;
    } else if (strstr(path, "boule")) {                   /* vague 75 : la sphere vient de frapper le sol, on saute l'onde */
        wave = 75;
        int b = boss_spawn(EN_BALL);
        Enemy *e = &E[b];
        float R = enemy_radius(EN_BALL);
        e->pos = v3(-15, R, 14); e->vel = v3(6, 0, 4); e->yaw = 1.3f; e->st = BS_MOVE; e->stT = 4;
        gwave_add(v3(-15, 0, 14), 30); GW[0].r = 13;
        gwave_add(v3(-15, 0, 14), 30); GW[1].r = 6;
        P.pos = P.prevPos = v3(2, 3.2f, 30); P.grounded = 0; P.vel = v3(0, 4, -9);
        e->hp = e->maxHp * 0.57f;
        vg_shot_look(vadd(e->pos, v3(0, -1.5f, 0)));
        snprintf(vgHint, sizeof vgHint, "SAUTE PAR-DESSUS LES ONDES"); vgHintT = 2;
    } else if (strstr(path, "hexaedre")) {                /* vague 100 : rayons annonces, un rayon parti, coeur ouvert */
        wave = 100;
        int b = boss_spawn(EN_HEXA);
        Enemy *e = &E[b];
        e->pos = v3(0, 11, 6); e->vel = v3(0, 0, 0); e->st = BS_OPEN; e->stT = 2; e->hp = e->maxHp * 0.79f; e->yaw = 0.6f;
        laser_add(e->pos, vnorm(vsub(vadd(pcenter(), v3(2.5f, 0, 0)), e->pos)), 0.5f);
        laser_add(e->pos, vnorm(vsub(vadd(pcenter(), v3(-4, 0, 1)), e->pos)), 0);
        boss_fx_tick(TICK); LZ[1].life = 0.2f;
        vg_shot_look(e->pos);
        snprintf(vgHint, sizeof vgHint, "COEUR OUVERT : TIRE !"); vgHintT = 2;
    } else if (strstr(path, "boutique")) {                /* boutique : quelques etapes achetees */
        vgState = VG_BREAK; wave = 30;
        vgLvl[P.loadout[0]] = 3; vgLvl[P.loadout[1]] = 1; vgLvl[UP_MELEE(P.meleeKind)] = 2;
        vgMenu = VGM_SHOP; vgShopHover = P.loadout[0] * 5 + 3;
    } else if (strstr(path, "selecteur")) {               /* apres la vague 20 : simple, difficile, challenge */
        wave = 20; vgCredits = 2210;
        vg_make_choices();
        vgState = VG_SELECT; vgMenu = VGM_SELECT;
    } else if (strstr(path, "fin") && !strstr(path, "infini")) {   /* vague 100 terminee */
        wave = 100; vgT100 = vgRunT = 2843.27f; vgSplit[0] = 611.2f; vgSplit[1] = 1290.8f; vgSplit[2] = 2034.5f; vgSplit[3] = vgT100;
        score = 412830; kills = 2316; vgKillsBoss = 4; bestScore = score;
        vgState = VG_WIN; vgMenu = VGM_WIN;
    } else {                                              /* "vagues" / "vagues_infini" : une vague ordinaire en cours */
        if (strstr(path, "infini")) { vgInfinite = 1; vgSecret = 1; wave = 113; vgWaveT = 11.5f; }
        vgHpMul = vg_hp_mul(wave); vgEliteP = 0.4f;
        static const int ty[8] = { EN_DRONE, EN_SNIPER, EN_BASTION, EN_SWARM, EN_SWARM, EN_SWARM, EN_RUSHER, EN_DRONE };
        static const float at[8][3] = { { -8, 9, 6 }, { 10, 13, 0 }, { -13, 1.4f, 24 }, { -3, 4, 18 }, { -2, 4.5f, 19 },
                                        { -4, 3.8f, 19 }, { 6, 2, 20 }, { 12, 7, 10 } };
        for (int k = 0; k < 8; k++) { int i = spawn_enemy_at(ty[k], v3(at[k][0], at[k][1], at[k][2]), 0); if (i >= 0) E[i].fireT = 100; }
        E[1].st = 1; E[1].stT = 0.4f; E[1].aim = vnorm(vsub(pcenter(), E[1].pos));
        E[2].yaw = atan2f(P.pos.x - E[2].pos.x, P.pos.z - E[2].pos.z);
        vgQn = 12; vgQi = 4;
        vg_shot_look(v3(0, 5, 10));
    }
    msgT = 0;                                             /* le grand titre du boss cacherait la scene */
    gameTime = 3;
    render(1, TICK);
    save_bmp(path);
    return 1;
}
