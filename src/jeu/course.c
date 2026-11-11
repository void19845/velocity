/* VELOCITY - mode COURSE : course au grappin contre la montre (portes, chrono, fantome du record)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Une carte a portes (MapDef.gates, LA TOILE) : depart arrete avec un */
/* decompte de 3 s, puis on traverse les portes dans l'ordre ; la      */
/* derniere est l'arrivee. Une chute ramene a la derniere porte passee */
/* (le chrono continue). R recommence. Le meilleur temps, ses temps    */
/* intermediaires et son fantome (position 30 fois par seconde) sont   */
/* gardes a cote de velocity.ini (un fichier par carte, race_file).    */
/* Chaque arrivee est envoyee au classement de la carte (speedrun).    */
/* Cartes a cibles (MapDef.targets) : des ennemis CIBLE / MINI a 1 PV, sur  */
/* lesquels toute arme porte ; une porte ne compte que si ses cibles sont  */
/* abattues (course_locked), elles peuvent bouger (sinus). Les armes       */
/* regagnent une munition par seconde (sans cela, un tir rate bloquerait). */
/* Speedrun complet (toutes les cartes CHALLENGE) : src/jeu/speedrun.c */
/* Affichage : src/interface/course_hud.c.                             */
/* ------------------------------------------------------------------ */
#define RACE_GATES  24
#define RACE_GMAX   (30 * 300)                            /* fantome : 5 min a 30 Hz */
#define RACE_GHZ    30.0f
static int raceGate, raceDone, raceFalls, raceNewBest, raceLoaded = -1;   /* raceLoaded : carte du record en memoire */
static int raceMap = -1;                                  /* carte CHALLENGE de la course (-1 : la premiere) */
static int srFull;                                        /* speedrun complet en cours (src/jeu/speedrun.c) */
static void sr_full_start(void);
static void sr_full_map_done(int ms);
static float raceT, raceCount, raceDeltaT, raceDelta, raceGateFlash, raceGhostAcc;
static float raceSplit[RACE_GATES], raceBest, raceBestSplit[RACE_GATES];
static float ghostCur[RACE_GMAX][4], ghostBest[RACE_GMAX][4];   /* x y z yaw */
static int ghostCurN, ghostBestN;
static void race_save(void);
static void race_load(void);
static float raceTgT, raceLockMsgT, raceAmmoT;            /* temps des cibles mobiles ; message "verrouillee" ; prochaine munition rendue */

static int race_map(void) {                               /* premiere carte CHALLENGE (a portes) : LA TOILE */
    for (int m = 0; m < NMAPS; m++) if (maps[m].challenge && maps[m].ngates > 0) return m;
    return 0;
}
static V3 race_gate(int i) { const float *g = maps[curMap].gates[i]; return v3(g[0], g[1], g[2]); }
static void race_face(V3 from, int gate) {                /* regarder vers la porte suivante */
    if (gate >= maps[curMap].ngates) return;
    V3 d = vsub(race_gate(gate), from);
    P.yaw = atan2f(d.x, -d.z); P.pitch = 0;
}
/* cibles de la carte : memes champs que MapDef.targets ; la position d'une cible mobile a l'instant t */
static V3 race_target_at(const float *t, int idx, float time) {
    V3 p = v3(t[0], t[1], t[2]);
    if (t[8] > 0) { float s = sinf(time * 2 * PI / t[8] + idx * 1.7f); p = vadd(p, v3(t[5] * s, t[6] * s, t[7] * s)); }
    return p;
}
static void race_targets_spawn(void) {
    const MapDef *md = &maps[curMap];
    for (int k = 0; k < md->ntargets; k++) {
        const float *t = md->targets[k];
        int i = spawn_enemy_at(t[3] > 0.5f ? EN_MINI : EN_CIBLE, race_target_at(t, k, 0), 0);
        if (i < 0) continue;
        E[i].hp = E[i].maxHp = 1; E[i].elite = 0; E[i].phase = (int)t[4]; E[i].combo = k + 1;   /* combo : indice dans la carte (+1) */
        E[i].vel = v3(0, 0, 0); E[i].t = (float)k;
    }
}
static int race_is_target(int i) { return E[i].alive && (E[i].type == EN_CIBLE || E[i].type == EN_MINI); }
static int race_targets_left(int gate) {                  /* cibles encore debout qui verrouillent cette porte */
    int n = 0;
    for (int i = 0; i < MAXE; i++) n += race_is_target(i) && E[i].phase == gate;
    return n;
}
static int race_targets_total(int gate) {
    const MapDef *md = &maps[curMap];
    int n = 0;
    for (int k = 0; k < md->ntargets; k++) n += (int)md->targets[k][4] == gate;
    return n;
}
static void race_targets_tick(float dt) {
    const MapDef *md = &maps[curMap];
    raceTgT += dt;
    for (int i = 0; i < MAXE; i++) {
        if (!race_is_target(i)) continue;
        const float *t = md->targets[E[i].combo - 1];
        V3 p = race_target_at(t, E[i].combo - 1, raceTgT);
        E[i].vel = vmul(vsub(p, E[i].pos), dt > 1e-5f ? 1 / dt : 0);
        E[i].pos = p; E[i].t += dt;
    }
}
static void race_start_map(int m) {                     /* depart de la course de la carte m (une carte a portes) */
    if (m < 0 || m >= NMAPS || !maps[m].ngates) m = race_map();
    if (curMap != m) load_map(m);
    if (raceLoaded != m) { raceBest = 0; ghostBestN = 0; race_load(); raceLoaded = m; }
    reset_game();
    memset(E, 0, sizeof E);
    raceTgT = raceLockMsgT = raceAmmoT = 0;
    race_targets_spawn();
    styleTopRank = 0;
    raceOn = 1; raceGate = 0; raceDone = 0; raceFalls = 0; raceNewBest = 0;
    raceT = 0; raceCount = RACE_COUNTDOWN; raceDeltaT = 0; raceGateFlash = 0; raceGhostAcc = 0; ghostCurN = 0;
    P.pos = P.prevPos = maps[curMap].start;
    P.yaw = maps[curMap].startYaw; P.pitch = 0;
    speedrun_begin(SR_MAP0 + curMap);                     /* classement de la carte : ticket du serveur */
}
/* R, RECOMMENCER : la meme course, ou tout le speedrun complet depuis la premiere carte */
static void race_start(void) {
    if (srFull) sr_full_start();
    else race_start_map(raceMap);
}
/* chute sous killY : retour a la derniere porte passee (ou au depart), immobile, face a la suivante */
static void race_respawn(void) {
    V3 at = raceGate > 0 ? race_gate(raceGate - 1) : maps[curMap].start;
    if (raceGate > 0) at.y -= 0.9f;
    P.pos = P.prevPos = at; P.vel = v3(0, 0, 0);
    hook_release(0); P.dashCharge = 2; P.airJumps = 1;
    race_face(at, raceGate);
    raceFalls++;
    shake += 0.3f;
    play(S_WAVE, 0.5f, 0.7f);
}
/* le segment a -> b passe-t-il dans la sphere (c, r) ? */
static int race_cross(V3 a, V3 b, V3 c, float r) {
    V3 ab = vsub(b, a);
    float l2 = vdot(ab, ab), t = l2 > 1e-6f ? clampf(vdot(vsub(c, a), ab) / l2, 0, 1) : 0;
    return vlen(vsub(vadd(a, vmul(ab, t)), c)) < r;
}
static void race_tick(float dt) {
    if (!raceOn) return;
    if (raceCount > 0) {                                  /* decompte : on peut regarder, pas bouger */
        float before = raceCount;
        raceCount -= dt;
        P.pos = P.prevPos = maps[curMap].start; P.vel = v3(0, 0, 0);
        if (ceilf(before) != ceilf(raceCount)) play(raceCount <= 0 ? S_PAD : S_HIT, 0.7f, raceCount <= 0 ? 1.4f : 0.8f);
        return;
    }
    if (raceDeltaT > 0) raceDeltaT -= dt;
    if (raceGateFlash > 0) raceGateFlash -= dt;
    if (raceLockMsgT > 0) raceLockMsgT -= dt;
    if (maps[curMap].ntargets > 0) {
        race_targets_tick(dt);
        raceAmmoT += dt;
        if (raceAmmoT >= 1.0f && !raceDone) {             /* une munition par seconde a chaque arme a jauge du loadout */
            raceAmmoT = 0;
            for (int s = 0; s < 2; s++) {
                int w = P.loadout[s];
                if (ammoDef[w].gauge > 0 && P.ammo[w] < wp_max(w)) P.ammo[w]++;
            }
        }
    }
    if (raceDone) return;
    raceT += dt;
    raceGhostAcc += dt;
    while (raceGhostAcc >= 1 / RACE_GHZ && ghostCurN < RACE_GMAX) {   /* fantome : une position tous les 1/30 s */
        raceGhostAcc -= 1 / RACE_GHZ;
        ghostCur[ghostCurN][0] = P.pos.x; ghostCur[ghostCurN][1] = P.pos.y; ghostCur[ghostCurN][2] = P.pos.z; ghostCur[ghostCurN][3] = P.yaw;
        ghostCurN++;
    }
    const MapDef *md = &maps[curMap];
    V3 a = vadd(P.prevPos, v3(0, 0.9f, 0)), b = vadd(P.pos, v3(0, 0.9f, 0));
    int locked = raceGate < md->ngates && md->ntargets > 0 && race_targets_left(raceGate) > 0;
    if (locked && race_cross(a, b, race_gate(raceGate), md->gates[raceGate][3]) && raceLockMsgT <= 0) {
        raceLockMsgT = 1.2f;                              /* on a passe l'anneau sans avoir abattu ses cibles */
        play(S_HURT, 0.5f, 1.6f);
    }
    if (!locked && raceGate < md->ngates && race_cross(a, b, race_gate(raceGate), md->gates[raceGate][3])) {
        raceSplit[raceGate] = raceT;
        if (raceBest > 0) { raceDelta = raceT - raceBestSplit[raceGate]; raceDeltaT = 2.0f; }
        raceGateFlash = 0.5f;
        raceGate++;
        style_add(STYLE_BONUS, "PORTE");
        if (raceGate >= md->ngates) {                     /* arrivee */
            int ms = (int)lroundf(raceT * 1000), sp[RACE_GATES];
            for (int i = 0; i < md->ngates; i++) sp[i] = (int)lroundf(raceSplit[i] * 1000);
            raceDone = 1;
            play(S_WAVE, 0.9f, 1.0f);
            if (!vagOn) speedrun_report_splits(SR_MAP0 + curMap, ms, sp, md->ngates);   /* classement de la carte */
            if (srFull) sr_full_map_done(ms);
            if (!vagOn && (raceBest <= 0 || raceT < raceBest)) {   /* le challenge des VAGUES ne touche pas au record */
                raceNewBest = raceBest > 0 ? 2 : 1;       /* 1 : premier temps ; 2 : record battu */
                raceBest = raceT;
                memcpy(raceBestSplit, raceSplit, sizeof raceSplit);
                memcpy(ghostBest, ghostCur, sizeof ghostCur[0] * ghostCurN);
                ghostBestN = ghostCurN;
                race_save();
            }
        } else play(S_PAD, 0.6f, 1.2f + 0.04f * raceGate);
    }
}
/* position du fantome du record au temps t (interpolee) ; 0 si pas de fantome */
static int race_ghost_at(float t, V3 *pos, float *yaw) {
    if (ghostBestN < 2) return 0;
    float f = t * RACE_GHZ;
    int i = (int)f;
    if (i >= ghostBestN - 1) { i = ghostBestN - 2; f = (float)(ghostBestN - 1); }
    float k = clampf(f - i, 0, 1);
    const float *g0 = ghostBest[i], *g1 = ghostBest[i + 1];
    *pos = v3(g0[0] + (g1[0] - g0[0]) * k, g0[1] + (g1[1] - g0[1]) * k, g0[2] + (g1[2] - g0[2]) * k);
    *yaw = g0[3];
    return 1;
}
