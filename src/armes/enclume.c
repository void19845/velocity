/* VELOCITY - Enclume : coup au sol, pique, onde de choc
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Enclume (marteau-pilon) : coup au sol, pique, onde de choc          */
/* Au sol : coup lourd frontal. En l'air (assez haut) : pique a 30 m/s */
/* minimum, puis onde de choc a l'atterrissage (plan 1b, PLAN_SUITE) : */
/* la hauteur de chute H regle sa taille et la hauteur de son mur, la  */
/* vitesse de chute ses degats, la vitesse horizontale l'etire en      */
/* ovale vers l'avant. L'onde se propage (28 m/s) : chaque cible est   */
/* touchee une fois, quand le front la passe, si ses pieds sont sous   */
/* le mur ; sauter au passage du front l'esquive. Touche : rebond +    */
/* dash ; rate : immobilise 0,6 s + 0,02 s par metre de chute.         */
/* ------------------------------------------------------------------ */
static int slamDives, slamWaves, slamWaveHits, slamStuns;   /* journal du bot */
static float slamVySum, slamDmgSum, slamHSum, slamRSum;

static float anvil_range(float rel) { return ANVIL_RANGE + rel * ANVIL_RANGE_PER_MS; }
static float anvil_dmg(float rel) { return ANVIL_DMG + rel * ANVIL_DMG_PER_MS; }
static float slam_kh(float fallH) { return SLAM_FULL_H > 0 ? clampf(fallH / SLAM_FULL_H, 0.5f, 1) : 1; }
static float slam_dmg(float vy, float fallH) { return (SLAM_DMG + SLAM_DMG_PER_MS * vy) * slam_kh(fallH); }
/* forme de l'onde : rayon et hauteur du mur selon la hauteur de chute ; etirement avant / arriere selon la vitesse horizontale */
static float slam_R(float H) { return clampf(SLAM_R_MIN + SLAM_R_PER_M * H, SLAM_R_MIN, SLAM_R_MAX); }
static float slam_hw(float H) { return clampf(SLAM_WH_MIN + SLAM_WH_PER_M * H, SLAM_WH_MIN, SLAM_WH_MAX); }
static float slam_fwd(float vh) { return fminf(1 + SLAM_FWD_PER_MS * vh, SLAM_FWD_MAX); }
static float slam_back(float vh) { return fmaxf(1 - SLAM_BACK_PER_MS * vh, SLAM_BACK_MIN); }
static float slam_stun(float H) { return fminf(SLAM_STUN + SLAM_STUN_PER_M * fmaxf(H, 0), SLAM_STUN_MAX); }
/* position d'un point (dx, dz depuis le centre) dans l'ovale : 0 au centre, 1 sur le bord. pad = rayon de la cible */
static float slam_ell(V3 dir, float R, float vh, float dx, float dz, float pad) {
    float a = dx * dir.x + dz * dir.z, b = dz * dir.x - dx * dir.z;
    float ra = R * (a >= 0 ? slam_fwd(vh) : slam_back(vh)) + pad, rb = R + pad;
    return sqrtf((a / ra) * (a / ra) + (b / rb) * (b / rb));
}

/* onde en cours (la mienne : degats) et ondes affichees (la mienne et celles des autres, EV_SLAM) */
typedef struct {
    int alive, hits, nHit;
    V3 c, dir, vel0;                                      /* vel0 : ma vitesse a l'impact, rendue au rebond */
    float R, vh, hw, D, vy, H, t;
    unsigned char doneP[MAXP], doneE[MAXE];               /* 1 = touchee, 2 = front passe sans toucher (esquive) */
} SlamWave;
static SlamWave SW;
static float slamStunFull;                                /* duree de l'immobilisation en cours (barre du HUD) */
typedef struct { int alive; V3 c, dir; float R, vh, hw, t; } SlamVis;
#define MAXSV 8
static SlamVis SV[MAXSV];
static void slam_vis_add(V3 c, V3 dir, float R, float vh, float hw) {
    int k = 0;
    for (int i = 0; i < MAXSV; i++) if (!SV[i].alive || SV[i].t > SV[k].t) { k = i; if (!SV[i].alive) break; }
    SV[k].alive = 1; SV[k].c = c; SV[k].dir = dir; SV[k].R = R; SV[k].vh = vh; SV[k].hw = hw; SV[k].t = 0;
}

/* distance au sol sous un point (les pieds), 50 si rien */
static float anvil_ground_dist(V3 feet) {
    return ray_world(vadd(feet, v3(0, 0.05f, 0)), v3(0, -1, 0), 50, 0) - 0.05f;
}

static void anvil_reset(void) { P.anvilState = ANVIL_IDLE; P.anvilStunT = 0; P.anvilLandT = 10; SW.alive = 0; }

/* Prevision du pique (sans controle aerien, la trajectoire est deterministe) : point d'impact,
 * |vy| a l'impact, rayon de l'onde (selon la hauteur de chute) et temps de chute. Repere d'impact, HUD, bot. */
static int slam_predict(V3 *at, float *vy, float *R, float *tFall) {
    float h = anvil_ground_dist(P.pos);
    if (h >= 50) return 0;
    float v0 = P.anvilState == ANVIL_DIVE ? fmaxf(-P.vel.y, 0) : fmaxf(-P.vel.y, SLAM_VY);
    float t = (-v0 + sqrtf(v0 * v0 + 2 * GRAVITY * h)) / GRAVITY;
    V3 p = vadd(P.pos, v3(P.vel.x * t, 0, P.vel.z * t));
    float drop = anvil_ground_dist(v3(p.x, P.pos.y, p.z));
    if (drop >= 50) drop = h;
    *at = v3(p.x, P.pos.y - drop, p.z);
    *vy = fminf(sqrtf(v0 * v0 + 2 * GRAVITY * drop), SLAM_VY_MAX);
    *R = slam_R((P.anvilState == ANVIL_DIVE ? P.anvilStartY : P.pos.y) - at->y);
    if (tFall) *tFall = t;
    return 1;
}

/* coup au sol : copie de l'epee avec les reglages de l'Enclume */
static void anvil_strike(void) {
    float sp = vlen(P.vel);
    V3 eye = eye_pos(), f = view_dir();
    V3 r = v3(cosf(P.yaw), 0, sinf(P.yaw)), u = vcross(r, f);
    int hits = 0, parried = 0;

    melee_start_swing();
    parried = try_parry(MELEE_RANGE + 1.5f, 1);
    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &E[i];
        if (!e->alive) continue;
        float rel = rel_speed(e->vel);
        V3 d = vsub(e->pos, eye);
        float dist = vlen(d), reach = dist - enemy_radius(e->type);
        if (reach > anvil_range(rel)) continue;
        if (reach > 0.8f && vdot(vmul(d, 1.0f / dist), f) < ANVIL_CONE) continue;
        if (e->type == EN_RUSHER) {                       /* kamikaze pare : explose dans sa face */
            V3 at = e->pos;
            damage_enemy(i, 9999, vmul(f, 10));
            parry_feedback(at);
            melee_parry_points(PTS_PARRY);
            parried++;
        } else {
            damage_enemy(i, anvil_dmg(rel), vadd(vmul(f, ANVIL_KNOCK), v3(0, ANVIL_KNOCK_UP, 0)));
            if (!e->alive) { P.hp = fminf(100, P.hp + 15); ammo_points(PTS_MELEE_KILL, AP_KILL); }
        }
        hits++;
    }
    for (int i = 0; i < MAXP; i++) {                     /* LAN : les autres joueurs */
        if (!np_target(i)) continue;
        float rel = rel_speed(np_vel(i));
        V3 c = np_center(i), d = vsub(c, eye);
        float dist = vlen(d), reach = dist - 0.5f;
        if (reach > anvil_range(rel)) continue;
        if (reach > 0.8f && vdot(vmul(d, 1.0f / dist), f) < ANVIL_CONE) continue;
        if (!hit_player(i, anvil_dmg(rel) * PVP_SCALE, vadd(vmul(f, ANVIL_KNOCK), v3(0, ANVIL_KNOCK_UP, 0)), W_SLAM, vnorm(d))) {
            parried++;                                    /* Egide : coup au sol bloque de face */
            continue;
        }
        burst(c, v3(1, 0.7f, 0.35f), 24, 9, 0.35f, 0.14f);
        hits++;
    }
    float range = anvil_range(sp);
    for (float a = -0.6f; a <= 0.6f; a += 0.1f) {        /* arc vertical : la tete tombe de haut en bas */
        V3 p = vadd(eye, vadd(vmul(f, range * 0.6f + 0.5f), vadd(vmul(u, -a * range * 0.5f), vmul(r, a * 0.15f))));
        add_part(p, vadd(vmul(f, 2), v3(0, -4, 0)), v3(1, 0.75f, 0.45f), 0.18f, 0.09f + sp * 0.002f, 0);
    }
    if (dmMode) net_fx_melee(eye, f, sp);                 /* EV_MELEE : le recepteur lit meleeKind dans pad */
    P.meleeCd = ANVIL_CD;
    P.meleeT = ANVIL_SWING_T;
    P.parryWin = ANVIL_PARRY_WINDOW;
    P.swingLanded = hits || parried;
    play(S_DASH, 0.45f, 0.8f);                            /* souffle plus grave que l'epee */
    if (hits) {
        melee_hit_points(meleePts[P.meleeKind]);
        P.dashCharge = fminf(2, P.dashCharge + 1);
        hitMarker = 0.18f;
        shake += 0.25f;
        if (!parried) hitstop = fmaxf(hitstop, 0.06f + sp * 0.002f);
        play(S_ANVIL_CLANG, 0.9f, frange(0.9f, 1.05f));
    }
}

static void slam_start(void) {
    if (P.hook != HOOK_NONE) hook_release(0);
    P.wallrun = P.sliding = 0;
    P.anvilState = ANVIL_DIVE; P.anvilStartY = P.pos.y;
    P.vel.y = fminf(P.vel.y, -SLAM_VY);
    P.fovKick += 6; shake += 0.1f;
    slamDives++;
    play(S_ANVIL_DIVE, 0.8f, 1.0f);
    if (botMode) netlog("enclume : pique depuis %.1f m", anvil_ground_dist(P.pos));
}

/* appui sur la melee avec l'Enclume : au sol (ou trop pres du sol) coup lourd, sinon pique */
static void anvil_press(void) {
    if (P.anvilState != ANVIL_IDLE) return;
    if (P.grounded || anvil_ground_dist(P.pos) < SLAM_MIN_H) anvil_strike();
    else slam_start();
}

/* la premiere touche d'une onde : rebond, dash, points ; l'immobilisation (deja commencee a l'impact) est annulee.
 * Le rebond est decide chez le frappeur avant l'arbitrage : chez l'hote, une onde paree (EV_MELEE_PARRY, arbitre tout
 * de suite) ecrase donc bien le rebond et rend les points */
static void slam_first_hit(V3 c, float vy) {
    slamWaveHits++;
    if (SW.alive) { P.vel.x = SW.vel0.x; P.vel.z = SW.vel0.z; }   /* vitesse horizontale d'avant l'impact, conservee */
    P.vel.y = SLAM_BOUNCE; P.grounded = 0; P.airJumps = 1;
    P.dashCharge = fminf(2, P.dashCharge + 1);
    melee_hit_points(meleePts[P.meleeKind]);             /* une fois par onde */
    hitMarker = 0.18f; play(S_HIT, 0.7f, 0.8f);
    hitstop = fmaxf(hitstop, fminf(0.12f, 0.05f + vy * 0.0015f));
    if (vy > 35) impact_frame(c, 0.07f);
    if (P.anvilState == ANVIL_STUN) { P.anvilState = ANVIL_IDLE; P.anvilStunT = 0; }
}

/* contact direct en l'air pendant le pique : la cible (joueur directP ou ennemi directE) prend tout, sans onde */
static void slam_direct(float vyImpact, int directP, int directE) {
    float vy = fminf(fabsf(vyImpact), SLAM_VY_MAX), H = P.anvilStartY - P.pos.y, D = slam_dmg(vy, H);
    V3 c = P.pos, up = v3(0, SLAM_KNOCK_UP_MIN + 3 * slam_hw(H), 0);
    melee_start_swing();                                  /* une onde = un coup (points de jauge) */
    burst(c, v3(0.75f, 0.72f, 0.7f), 30, 6, 0.5f, 0.2f);
    shake += 0.3f + vy * 0.01f;
    play(S_ANVIL_SLAM, clampf(vy / 40, 0.5f, 1), 1.1f - vy * 0.005f);
    P.anvilLastVy = vy; P.anvilLandT = 0; P.meleeCd = fmaxf(P.meleeCd, SLAM_CD);
    slamWaves++; slamVySum += vy; slamDmgSum += D;
    slam_first_hit(c, vy);
    P.anvilState = ANVIL_IDLE;
    if (botMode) netlog("enclume : contact direct |vy| %.1f, degats %.0f", vy, D);
    if (directE >= 0) {
        damage_enemy(directE, D, up);
        if (!E[directE].alive && E[directE].type == EN_DRONE) ammo_points(PTS_MELEE_KILL, AP_KILL);
    } else if (directP >= 0)                              /* Egide : le contact direct est bloque de face */
        hit_player_ex(directP, D * PVP_SCALE, up, W_SLAM, vnorm(vsub(np_center(directP), pcenter())), SHB_DIRECT, 0, 0, 0, 0);
}

/* impact au sol : l'onde part du point d'impact. Immobilise tout de suite (annule si le front touche quelqu'un) */
static void slam_land(float vyImpact) {
    float vy = fminf(fabsf(vyImpact), SLAM_VY_MAX), H = fmaxf(P.anvilStartY - P.pos.y, 0), vh = hlen(P.vel);
    V3 dir = vh > 1 ? v3(P.vel.x / vh, 0, P.vel.z / vh) : v3(sinf(P.yaw), 0, -cosf(P.yaw));
    if (vh <= 1) vh = 0;
    memset(&SW, 0, sizeof SW);
    SW.alive = 1; SW.c = P.pos; SW.dir = dir; SW.vh = vh; SW.vy = vy; SW.H = H; SW.vel0 = v3(P.vel.x, 0, P.vel.z);
    SW.R = slam_R(H); SW.hw = slam_hw(H); SW.D = slam_dmg(vy, H);
    melee_start_swing();                                  /* une onde = un coup (points de jauge) */
    slam_vis_add(SW.c, dir, SW.R, vh, SW.hw);
    burst(SW.c, v3(0.75f, 0.72f, 0.7f), 40, 6, 0.6f, 0.2f);
    shards(SW.c, v3(0.55f, 0.52f, 0.5f), 8 + (int)(vy / 4), 9);
    shake += 0.3f + vy * 0.01f; P.landDip = 0.3f;
    play(S_ANVIL_SLAM, clampf(vy / 40, 0.5f, 1), 1.1f - vy * 0.005f);
    if (dmMode) net_fx_slam(SW.c, SW.R, SW.hw, dir, vh, vy);
    P.anvilLastVy = vy; P.anvilLandT = 0; P.meleeCd = fmaxf(P.meleeCd, SLAM_CD);
    P.anvilState = ANVIL_STUN; P.anvilStunT = slamStunFull = slam_stun(H); P.vel.x = P.vel.z = 0;
    slamWaves++; slamVySum += vy; slamHSum += H; slamRSum += SW.R;
}
static void slam_resolve(float vyImpact, int directP, int directE) {
    if (directP >= 0 || directE >= 0) slam_direct(vyImpact, directP, directE);
    else slam_land(vyImpact);
}

/* propagation de mon onde (a chaque tick de player_tick) : le front avance a SLAM_WAVE_SPEED ; une cible est jugee une
 * seule fois, quand le front la passe : touchee si ses pieds sont sous le mur et au-dessus du sol d'impact, sinon esquivee */
static void slam_wave_tick(float dt) {
    if (!SW.alive) return;
    SW.t += dt;
    float k = fminf(SW.t * SLAM_WAVE_SPEED / SW.R, 1);   /* avancement du front, 1 = bord de l'onde */
    V3 c = SW.c, eyeC = vadd(c, v3(0, 0.5f, 0));
    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &E[i];
        float er = enemy_radius(e->type);
        if (!e->alive || SW.doneE[i]) continue;
        float el = slam_ell(SW.dir, SW.R, SW.vh, e->pos.x - c.x, e->pos.z - c.z, er);
        if (el > k) continue;
        SW.doneE[i] = 2;
        if (e->pos.y - er - c.y > SW.hw || e->pos.y + er < c.y - SLAM_REACH_DOWN || !los(eyeC, e->pos)) continue;
        float f = clampf(1 - el, 0, 1), dmg = SW.D * (0.3f + 0.7f * f);
        V3 out = vnorm(v3(e->pos.x - c.x, 0.001f, e->pos.z - c.z));
        SW.doneE[i] = 1;
        if (!SW.hits++) slam_first_hit(c, SW.vy);
        damage_enemy(i, dmg, v3(out.x * SLAM_KNOCK_OUT * f, SLAM_KNOCK_UP_MIN + 3 * SW.hw, out.z * SLAM_KNOCK_OUT * f));
        if (!e->alive && e->type == EN_DRONE) ammo_points(PTS_MELEE_KILL, AP_KILL);
        slamDmgSum += dmg;
    }
    for (int i = 0; i < MAXP; i++) {                     /* LAN : chaque touche part quand le front passe la cible */
        V3 d = NP[i].disp;
        if (!np_target(i) || SW.doneP[i]) continue;
        float el = slam_ell(SW.dir, SW.R, SW.vh, d.x - c.x, d.z - c.z, 0.5f);
        if (el > k) continue;
        SW.doneP[i] = 2;
        if (d.y - c.y > SW.hw || d.y < c.y - SLAM_REACH_DOWN || !los(eyeC, np_center(i))) {
            if (botMode && d.y - c.y > SW.hw) netlog("enclume : %s saute par-dessus l'onde (pieds a %.1f m, mur %.1f m)", NP[i].name,
                                                     d.y - c.y, SW.hw);
            continue;
        }
        float f = clampf(1 - el, 0, 1), dmg = SW.D * (0.3f + 0.7f * f) * PVP_SCALE;
        V3 out = vnorm(v3(d.x - c.x, 0.001f, d.z - c.z));
        SW.doneP[i] = 1;
        if (!SW.hits++) slam_first_hit(c, SW.vy);
        slamDmgSum += dmg;
        hit_player_ex(i, dmg, v3(out.x * SLAM_KNOCK_OUT * f, SLAM_KNOCK_UP_MIN + 3 * SW.hw, out.z * SLAM_KNOCK_OUT * f), W_SLAM,
                      vnorm(vsub(np_center(i), c)), SHB_NEVER, 0, 0, 0, 0);   /* Egide : l'onde passe sous le bouclier */
    }
    if (k >= 1) {                                         /* l'onde est finie : sans touche, l'immobilisation continue */
        SW.alive = 0;
        if (!SW.hits) slamStuns++;
        if (botMode) netlog("enclume : onde H %.1f m, |vy| %.1f, vh %.1f, rayon %.1f (avant %.1f, arriere %.1f), mur %.1f m, "
                            "degats %.0f, %d touche(s)", SW.H, SW.vy, SW.vh, SW.R, SW.R * slam_fwd(SW.vh), SW.R * slam_back(SW.vh),
                            SW.hw, SW.D, SW.hits);
    }
}

/* ondes affichees : mur qui s'etend (rendu dans draw_slam_waves), poussiere sur le front */
static void slam_vis_tick(float dt) {
    for (int i = 0; i < MAXSV; i++) {
        SlamVis *s = &SV[i];
        if (!s->alive) continue;
        s->t += dt;
        float k = s->t * SLAM_WAVE_SPEED / s->R;
        if (k > 1.4f) { s->alive = 0; continue; }         /* le mur retombe pendant 0,4 x son trajet */
        if (k > 1) continue;
        for (int n = 0; n < 4; n++) {                     /* poussiere et eclats sur le front */
            float a = frange(0, 2 * PI), ca = cosf(a), sa = sinf(a);
            float ra = s->R * (ca >= 0 ? slam_fwd(s->vh) : slam_back(s->vh)) * k, rb = s->R * k;
            V3 side = v3(-s->dir.z, 0, s->dir.x);
            V3 p = vadd(s->c, vadd(vmul(s->dir, ca * ra), vmul(side, sa * rb)));
            add_part(vadd(p, v3(0, 0.15f, 0)), v3(0, frange(1, 2 + 2 * s->hw), 0), v3(1, 0.7f, 0.35f), 0.35f, 0.14f, 6);
        }
    }
}

/* contact direct pendant le pique avec une cible hors d'atteinte de l'onde (en l'air) : elle prend tout */
static void slam_contact(void) {
    V3 mn, mx, omn, omx;
    paabb(P.pos, pheight(), &mn, &mx);
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i)) continue;
        np_aabb(i, 0, &omn, &omx);
        if (mn.x < omx.x && mx.x > omn.x && mn.y < omx.y && mx.y > omn.y && mn.z < omx.z && mx.z > omn.z &&
            anvil_ground_dist(NP[i].disp) > SLAM_REACH_UP) { slam_direct(-P.vel.y, i, -1); return; }
    }
    for (int j = 0; j < MAXE; j++) {
        if (!E[j].alive) continue;
        float er = enemy_radius(E[j].type);
        V3 q = v3(clampf(E[j].pos.x, mn.x, mx.x), clampf(E[j].pos.y, mn.y, mx.y), clampf(E[j].pos.z, mn.z, mx.z));
        if (vlen(vsub(q, E[j].pos)) < er && anvil_ground_dist(v3(E[j].pos.x, E[j].pos.y - er, E[j].pos.z)) > SLAM_REACH_UP) {
            slam_direct(-P.vel.y, -1, j);
            return;
        }
    }
}

/* piques des autres joueurs : trainee, et sifflement au debut */
static void anvil_remote_tick(void) {
    for (int i = 0; i < MAXP; i++) {
        unsigned char pad = NP[i].st.pad;
        if (!np_present(i)) { NP[i].anvilPrevPad = 0; continue; }
        if (pad & ST_SLAM) {
            V3 p = NP[i].disp;
            for (int k = 0; k < 2; k++)
                add_part(vadd(p, v3(frange(-.3f, .3f), frange(0.2f, 1.6f), frange(-.3f, .3f))), v3(frange(-1, 1), 6, frange(-1, 1)),
                         v3(1, 0.6f, 0.25f), 0.35f, 0.14f, 0);
            if (!(NP[i].anvilPrevPad & ST_SLAM)) play(S_ANVIL_DIVE, 0.8f * atten(p), 1.0f);
        }
        NP[i].anvilPrevPad = pad;
    }
}
