/* VELOCITY - mode VAGUES : 100 vagues, selecteur de carte toutes les 10, challenges, boss, INFINI, credits
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   Courbe de puissance : vagues 1-33 faciles (peu d'ennemis, les types arrivent un a un) ; 34-66 le joueur est au
   sommet (beaucoup d'ennemis faibles, ses achats font le menage) ; 67-100 les ennemis rattrapent (plus de sentinelles
   et de bastions, des elites qui changent de comportement, PV x1 -> x1,5). Les degats des ennemis ne montent JAMAIS.
   Toutes les 10 vagues : trois choix (carte simple, carte difficile, challenge). Vague 100 terminee : temps et score,
   puis l'INFINI (vagues minutees, boss qui attendent, attaques secretes).
   Temps : vgRunT = temps de partie (pauses ECHAP exclues ; boutique et selecteur comptes). Le mode speedrun s'y branche
   par vagues_temps_100() et vagues_splits_ms() (voir vg_speedrun_begin / vg_speedrun_end). */

enum { VG_BREAK, VG_FIGHT, VG_SELECT, VG_CHALLENGE, VG_WIN };
enum { VGM_NONE, VGM_SHOP, VGM_SELECT, VGM_WIN };         /* ecran par-dessus le jeu (souris liberee) */
enum { CH_COURSE, CH_DEFENSE };
static int vgState, vgMenu, vgInfinite, vgStartMap, vgBlockHard, vgChal, vgChalOk;
static float vgRunT, vgT100 = -1, vgSplit[4], vgWaveT, vgChalT, vgDefP, vgChalSpawnT;
static double vgMenuT0;
static int vgQueue[64], vgQn, vgQi;                       /* ennemis de la vague a faire apparaitre */
static int vgPend = -1;                                   /* choix du selecteur a appliquer au prochain tick */
static int vgChoiceMap[3], vgChoiceChal, vgChoiceReward;
static int vgClearedAt;                                   /* derniere vague videe (credits de fin de vague une fois) */
static int vgKillsBoss;                                   /* boss vaincus pendant la partie */
static const int vgSimpleMaps[3] = { 0, 3, 4 };           /* ARENE, LA BOUCLE, LE DOJO */
static const int vgHardMaps[2] = { 1, 2 };                /* LE PUITS, LE CANYON (du vide) */

/* --- points d'entree pour le mode speedrun (classement "vague 100 la plus rapide") --- */
static float vagues_temps_100(void) { return vgT100; }  /* temps (s) a la fin de la vague 100, -1 si pas encore */
static int vagues_splits_ms(int out[4]) {                 /* temps cumules (ms) a la fin des vagues 25, 50, 75, 100 */
    for (int k = 0; k < 4; k++) out[k] = vgSplit[k] > 0 ? (int)(vgSplit[k] * 1000 + 0.5f) : -1;
    return vgT100 > 0 ? 4 : 0;
}
static void vg_speedrun_begin(void) {                     /* depart d'une partie VAGUES (vague 1) : ticket du serveur */
    speedrun_begin(SR_VAGUES100);
}
static void vg_speedrun_end(void) {                       /* vague 100 terminee : classement "vague 100 la plus rapide" */
    int sp[4], n = vagues_splits_ms(sp);
    for (int k = 0; k < n; k++) if (sp[k] <= 0 || (k && sp[k] <= sp[k - 1])) n = 0;   /* le serveur veut 0 ou 4 temps croissants */
    if (n == 4) speedrun_report_splits(SR_VAGUES100, sp[3], sp, 4);
    else speedrun_report(SR_VAGUES100, (int)(vgT100 * 1000 + 0.5f));
}

static int vg_menu_open(void) { return vagOn && vgMenu != VGM_NONE; }
static void vg_menu_set(int m) {                          /* ouvre / ferme un ecran ; le temps passe dedans compte */
    if (vgMenu == VGM_NONE && m != VGM_NONE) vgMenuT0 = nowT;
    if (vgMenu != VGM_NONE && m == VGM_NONE && vgT100 < 0) vgRunT += (float)(nowT - vgMenuT0);
    vgMenu = m;
    if (m != VGM_NONE && captured && !shotMode) set_capture(0);
}
static int vgWantResume;                                  /* ecran ferme : la souris se recapture (vg_draw_hud -> ACT_RESUME) */
static void vg_menu_close(void) {
    vg_menu_set(VGM_NONE);
    vgWantResume = !shotMode;
}

/* --- vagues : combien, quoi --- */
static int vg_is_boss(int w) { return w % 25 == 0; }
static int vg_boss_type(int w) { return EN_PYRA + ((w / 25 - 1) % 4); }
static int vg_checkpoint(int w) { return w % VG_BLOCK == 0 || vg_is_boss(w); }   /* INFINI : celles-ci attendent */
static float vg_hp_mul(int w) {
    return 1 + (VG_HP_MAX - 1) * clampf((w - VG_HP_FROM) / (float)(VG_WAVES - VG_HP_FROM), 0, 1);
}
static float vg_elite_p(int w) { return VG_ELITE_MAX * clampf((w - VG_HP_FROM) / (float)(VG_WAVES - VG_HP_FROM), 0, 1); }
static int vg_count(int w) {
    if (w <= 33) return 3 + (int)(w * 0.36f);              /* 3 -> 14 */
    if (w <= 66) return 15 + (int)((w - 33) * 0.36f);      /* 15 -> 26 : la foule que tes achats balaient */
    if (w <= VG_WAVES) return 22 + (int)((w - 66) * 0.24f);   /* 22 -> 30, mais bien plus coriaces */
    int n = 30 + (w - VG_WAVES) / 5;
    return n > 40 ? 40 : n;
}
static int vg_pick_type(int w) {
    float wt[EN_PYRA] = { 0 };                            /* drone, kamikaze, sentinelle, essaim, bastion, bombe, carre, mortier (indices = EN_*) */
    if (w <= 33) {
        wt[0] = 1; wt[1] = w >= 2 ? 0.6f : 0; wt[2] = w >= 8 ? 0.25f + 0.01f * (w - 8) : 0;
        wt[3] = w >= 14 ? 0.3f : 0; wt[4] = w >= 22 ? 0.15f : 0;
        wt[EN_BOMBE] = w >= 12 ? 0.25f : 0; wt[EN_CARRE] = w >= 18 ? 0.12f : 0; wt[EN_MORTIER] = w >= 28 ? 0.12f : 0;
    } else if (w <= 66) {
        wt[0] = 1; wt[1] = 0.9f; wt[2] = 0.25f; wt[3] = 0.6f; wt[4] = 0.12f;
        wt[EN_BOMBE] = 0.4f; wt[EN_CARRE] = 0.15f; wt[EN_MORTIER] = 0.15f;
    } else {
        float k = clampf((w - 66) / 34.0f, 0, 1);
        wt[0] = 0.8f; wt[1] = 0.6f; wt[2] = 0.5f + 0.3f * k; wt[3] = 0.4f; wt[4] = 0.25f + 0.3f * k;
        wt[EN_BOMBE] = 0.5f; wt[EN_CARRE] = 0.2f; wt[EN_MORTIER] = 0.2f + 0.15f * k;
    }
    float tot = 0;
    for (int k = 0; k < EN_PYRA; k++) tot += wt[k];
    float r = frand() * tot;
    for (int k = 0; k < EN_PYRA; k++) { if (r < wt[k]) return k; r -= wt[k]; }
    return EN_DRONE;
}
static void vg_build_queue(int w) {
    vgQn = vgQi = 0;
    int n = vg_count(w);
    for (int k = 0; k < n && vgQn < 60; ) {
        int t = vg_pick_type(w);
        if (t == EN_SWARM) { for (int s = 0; s < 5 && vgQn < 64; s++) vgQueue[vgQn++] = EN_SWARM; k += 2; }
        else { vgQueue[vgQn++] = t; k++; }
    }
}
static int vg_alive_minions(void) {                       /* ennemis ordinaires vivants (hors boss) */
    int n = 0;
    for (int i = 0; i < MAXE; i++) n += E[i].alive && !EN_BOSS(E[i].type);
    return n;
}
static void vg_wave_start(int w) {
    wave = w;
    vgHpMul = vg_hp_mul(w); vgEliteP = vg_elite_p(w);
    vgSecret = vgInfinite;
    vgState = VG_FIGHT; spawnT = 0.6f; vgWaveT = VG_INF_T;
    if (vg_is_boss(w)) {
        vgQn = vgQi = 0;
        int b = boss_spawn(vg_boss_type(w));
        if (b >= 0 && vgHpMul > 1) { E[b].maxHp *= vgHpMul; E[b].hp = E[b].maxHp; }
    } else {
        vg_build_queue(w);
        snprintf(msg, sizeof msg, vgInfinite ? "INFINI : VAGUE %d" : "VAGUE %d", w);
        msgT = 2.5f;
        play(S_WAVE, 0.7f, 1.0f);
    }
}

/* --- credits --- */
static void vg_credit_kill(int i) {
    static const int base[EN_COUNT] = { 10, 6, 14, 3, 25, 12, 20, 30, 400, 600, 800, 1200 };
    Enemy *e = &E[i];
    if (e->summoned == 2) return;                         /* fantome du PRISME */
    float c = (float)base[e->type];
    if (e->elite) c *= 1.5f;
    if (vgBlockHard) c *= VG_HARD_CREDITS;
    if (e->summoned == 1) c /= 3;                         /* invocations : pas de quoi farmer un boss */
    vgCredits += (int)fmaxf(1, c + 0.5f);
}
static void vg_boss_down(int i) {
    Enemy *e = &E[i];
    vgKillsBoss++;
    snprintf(msg, sizeof msg, "%s VAINCU", bossTitle[e->type - EN_PYRA]); msgT = 3.0f;
    for (int j = 0; j < MAXE; j++) if (j != i && E[j].alive && E[j].summoned) kill_enemy(j, 0);   /* ses invocations s'eteignent */
    memset(LZ, 0, sizeof LZ); SWP.on = 0; HYP.on = 0;
    if (vgBoss == i) vgBoss = -1;
    shake += 0.8f; hitstop = fmaxf(hitstop, 0.25f);
    play(S_WAVE, 1.0f, 0.4f);
}

/* --- selecteur de carte : toutes les 10 vagues --- */
static void vg_make_choices(void) {
    int b = wave / VG_BLOCK;
    int s = vgSimpleMaps[b % 3];
    if (s == curMap) s = vgSimpleMaps[(b + 1) % 3];
    vgChoiceMap[0] = s;
    vgChoiceMap[1] = vgHardMaps[b % 2];
    vgChoiceChal = b % 2 ? CH_COURSE : CH_DEFENSE;
    vgChoiceMap[2] = vgChoiceChal == CH_COURSE ? race_map() : s;
    vgChoiceReward = 300 + 60 * b;
}
static void vg_open_select(void) {
    vg_make_choices();
    vgState = VG_SELECT;
    vg_menu_set(VGM_SELECT);
}
static void vg_go_map(int m) {
    if (curMap != m) load_map(m);
    P.pos = P.prevPos = maps[curMap].start; P.vel = v3(0, 0, 0);
    hook_release(0);
    memset(E, 0, sizeof E); memset(PR, 0, sizeof PR); memset(MB, 0, sizeof MB);
    for (int i = 0; i < npu; i++) PU[i].t = PU[i].pending = 0;
}
static void vg_challenge_start(int kind) {
    vgChal = kind; vgChalT = 0; vgDefP = 0; vgChalOk = 0; vgChalSpawnT = 1.5f;
    vgState = VG_CHALLENGE;
    if (kind == CH_COURSE) {                              /* course de LA TOILE contre la montre (sans record) */
        vg_go_map(race_map());
        if (!raceLoaded) { race_load(); raceLoaded = 1; }
        raceOn = 1; raceGate = 0; raceDone = 0; raceFalls = 0; raceNewBest = 0;
        raceT = 0; raceCount = RACE_COUNTDOWN; raceDeltaT = 0; raceGateFlash = 0; raceGhostAcc = 0; ghostCurN = 0;
        P.yaw = maps[curMap].startYaw; P.pitch = 0;
        snprintf(msg, sizeof msg, "CHALLENGE : COURSE"); msgT = 2.5f;
    } else {
        snprintf(msg, sizeof msg, "CHALLENGE : TIENS LA ZONE"); msgT = 2.5f;
    }
    play(S_WAVE, 0.8f, 1.2f);
}
static void vg_challenge_end(int ok) {
    vgChalOk = ok;
    if (ok) vgCredits += vgChoiceReward;
    snprintf(msg, sizeof msg, ok ? "CHALLENGE REUSSI  +%d" : "CHALLENGE RATE", vgChoiceReward); msgT = 3;
    play(S_WAVE, 0.9f, ok ? 1.4f : 0.6f);
    if (vgChal == CH_COURSE) { raceOn = 0; vg_go_map(vgChoiceMap[0]); }
    else for (int i = 0; i < MAXE; i++) if (E[i].alive) kill_enemy(i, 0);
    vgBlockHard = 0;
    vgState = VG_BREAK; waveDelay = VG_PAUSE_T;
}
/* applique le choix k du selecteur (0 simple, 1 difficile, 2 challenge) ; appele au tick suivant le clic */
static void vg_choose(int k) {
    vgBlockHard = k == 1;
    if (k == 2) {
        if (vgChoiceChal == CH_DEFENSE) vg_go_map(vgChoiceMap[0]);
        vg_challenge_start(vgChoiceChal);
        return;
    }
    vg_go_map(vgChoiceMap[k]);
    vgState = VG_BREAK; waveDelay = VG_PAUSE_T;
    snprintf(msg, sizeof msg, "%s", maps[curMap].name); msgT = 2.5f;
}
static V3 vg_zone(void) {                                 /* CHALLENGE defense : zone B de la carte */
    const MapDef *md = &maps[curMap];
    return md->zones ? v3(md->zones[1][0], md->zones[1][1], md->zones[1][2]) : md->start;
}
static float vg_zone_r(void) { return ZONE_R * map_obj_scale(curMap) * 1.4f; }
static int vg_in_zone(void) {
    V3 z = vg_zone();
    return !P.dead && hlen(vsub(P.pos, z)) < vg_zone_r() && fabsf(P.pos.y - z.y) < 2.5f;
}
static void vg_challenge_tick(float dt) {
    vgChalT += dt;
    if (vgChal == CH_COURSE) {
        if (raceCount > 0) { vgChalT = 0; return; }
        if (raceDone) vg_challenge_end(1);
        else if (raceT > VG_RACE_LIMIT) vg_challenge_end(0);
        return;
    }
    if (vg_in_zone()) vgDefP += dt;                        /* defense : tenir la zone, les assaillants arrivent */
    vgChalSpawnT -= dt;
    if (vgChalSpawnT <= 0 && vg_alive_minions() < VG_DEF_ALIVE) {
        vgChalSpawnT = 1.0f;
        vgHpMul = vg_hp_mul(wave); vgEliteP = vg_elite_p(wave);
        int t = vg_pick_type(wave);
        if (t == EN_SWARM) for (int s = 0; s < 3; s++) spawn_enemy_type(EN_SWARM);
        else spawn_enemy_type(t);
    }
    if (vgDefP >= VG_DEF_HOLD) vg_challenge_end(1);
    else if (vgChalT > VG_DEF_LIMIT) vg_challenge_end(0);
}

/* --- vague videe --- */
static void vg_wave_cleared(void) {
    if (vgClearedAt == wave) return;
    vgClearedAt = wave;
    vgCredits += 20 + wave;
    if (vg_is_boss(wave) && wave <= VG_WAVES) vgSplit[wave / 25 - 1] = vgRunT;
    if (wave == VG_WAVES && !vgInfinite) {                /* vague 100 : temps, score, et proposition de l'INFINI */
        vgT100 = vgRunT;
        vgState = VG_WIN;
        if (score > bestScore) bestScore = score;
        vg_speedrun_end();
        vg_menu_set(VGM_WIN);
        play(S_WAVE, 1.0f, 1.5f);
        return;
    }
    if (wave % VG_BLOCK == 0) { vg_open_select(); return; }
    vgState = VG_BREAK; waveDelay = VG_PAUSE_T;
    snprintf(msg, sizeof msg, "VAGUE %d TERMINEE", wave); msgT = 2.0f;
}

/* --- partie : debut, remise a zero (reset_game), tick --- */
static void vg_reset(void) {
    if (curMap != vgStartMap) { load_map(vgStartMap); P.pos = P.prevPos = maps[curMap].start; }
    wave = 0; vgState = VG_BREAK; waveDelay = 2.0f; vgQn = vgQi = 0;
    vgCredits = 0; sh_reset(); memset(fxStat, 0, sizeof fxStat);
    vgRunT = 0; vgT100 = -1; memset(vgSplit, 0, sizeof vgSplit);
    vgInfinite = 0; vgSecret = 0; vgBlockHard = 0; vgClearedAt = 0; vgKillsBoss = 0; vgPend = -1;
    vgHpMul = 1; vgEliteP = 0;
    raceOn = 0;
    if (vgMenu != VGM_NONE) vgMenu = VGM_NONE;
    boss_fx_reset();
    fx_reset();
    vg_speedrun_begin();
}
static void vg_start(void) {                              /* bouton VAGUES du menu */
    vgStartMap = maps[menuMap].solo && !maps[menuMap].challenge ? menuMap : 0;
    vagOn = 1;
    if (curMap != vgStartMap) load_map(vgStartMap);
    reset_game();                                         /* -> vg_reset */
}
static void vg_restart(void) { reset_game(); }
static void vg_go_infinite(void) {                        /* bouton INFINI de l'ecran de la vague 100 */
    vgInfinite = 1; vgSecret = 1;
    vg_make_choices();
    vgState = VG_SELECT;
    vgMenu = VGM_SELECT;
}
static void vg_clock(float dt) {                          /* chaque tick (meme pendant le hitstop) */
    if (vagOn && !P.dead && vgState != VG_WIN && vgMenu == VGM_NONE) vgRunT += dt;
}
static int vg_music_level(void) {
    if (vgBoss >= 0) return 8;
    if (vgState == VG_CHALLENGE) return 5;
    int seg = wave <= 33 ? 0 : wave <= 66 ? 2 : 3;
    return seg + ((wave - 1) % VG_BLOCK) / 3;
}
static void wave_tick(float dt) {
    if (!vagOn) return;
    if (vgMenu != VGM_NONE) { if (captured && !shotMode) set_capture(0); return; }   /* ecran ouvert : rien ne bouge */
    if (vgPend >= 0) { int k = vgPend; vgPend = -1; vg_choose(k); }
    boss_fx_tick(dt);
    vg_fx_tick(dt);
    if (P.dead) return;
    switch (vgState) {
    case VG_BREAK:
        waveDelay -= dt;
        if (actP[A_SCORES]) { vg_menu_set(VGM_SHOP); break; }   /* [TAB] : boutique entre deux vagues */
        if (waveDelay <= 0) vg_wave_start(wave + 1);
        break;
    case VG_FIGHT:
        spawnT -= dt;
        if (vgQi < vgQn && spawnT <= 0 && vg_alive_minions() < VG_ALIVE_MAX) {
            spawn_enemy_type(vgQueue[vgQi++]);
            spawnT = VG_SPAWN_T;
        }
        if (vgInfinite && !vg_checkpoint(wave)) {         /* INFINI : la suivante arrive au bout du chrono */
            vgWaveT -= dt;
            if (vgQi >= vgQn && enemies_alive() == 0) {
                if (vgClearedAt != wave) { vgClearedAt = wave; vgCredits += 20 + wave; }
                vgWaveT = fminf(vgWaveT, VG_INF_CLEAR_T);
            }
            if (vgWaveT <= 0) {
                int left = vgQn - vgQi;                   /* le reste de la vague s'ajoute a la suivante */
                int keep[64], nk = 0;
                for (int k = vgQi; k < vgQn && nk < 64; k++) keep[nk++] = vgQueue[k];
                vg_wave_start(wave + 1);
                for (int k = 0; k < left && vgQn < 64; k++) vgQueue[vgQn++] = keep[k];
            }
        } else if (vgQi >= vgQn && enemies_alive() == 0) vg_wave_cleared();
        break;
    case VG_CHALLENGE:
        vg_challenge_tick(dt);
        break;
    }
}
