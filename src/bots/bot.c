/* VELOCITY - bots : visee, decisions
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- bot de test : joue tout seul pour verifier le reseau ------------- */
static float botTime = 20, botT, botNextHook, botNextDash, botSpeedSum, botAliveT, botMaxSpeed;
static char botShot[260];
static int botSpec;                                       /* --spec : le bot client regarde la partie au lieu d'y jouer */
static int botRanked, botMmr0 = -1;                       /* --ranked : partie classee ; cote au debut (journal) */
static float bot_melee_reach(float rel) {
    return P.meleeKind == MK_ANVIL ? anvil_range(rel) : P.meleeKind == MK_LANCE ? LANCE_THRUST_RANGE :
           P.meleeKind == MK_FLAIL ? flail_range(rel) : P.meleeKind == MK_FIST ? fist_range(rel) :
           P.meleeKind == MK_SHIELD ? SHIELD_BASH_RANGE : melee_range(rel);
}
/* l'adversaire i vient-il de lancer un coup ? Epee et autres : minuteur >= 150 ; Poings : avancement < 20 sur 63 */
static int swing_start(int i) {
    unsigned char m = NP[i].st.melee;
    if ((NP[i].st.pad & 0x0F) == MK_FIST) return m && (m & 63) < 20;
    return m >= 150;
}

/* Aimant de visee : seulement pendant la competence AIMANT (P.aimT > 0, partout, PvP compris). Quand le viseur
 * passe pres d'une cible valide (ennemi solo, joueur adverse), il est tire vers son centre (#define AIMASSIST_*) :
 * une aide nette mais jamais un verrou ; l'equilibrage passe par la duree courte et la longue recharge (AIM_*). */
static void aim_assist(float dt) {
    if (P.dead || P.aimT <= 0) return;
    V3 eye = eye_pos();
    int haveTarget = 0;
    float bestAng = AIMASSIST_CONE;
    V3 bestC = v3(0, 0, 0);
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive) continue;
        V3 d = vsub(E[i].pos, eye);
        float dist = vlen(d);
        if (dist < 1 || dist > AIMASSIST_RANGE) continue;
        float dyaw = atan2f(d.x, -d.z) - P.yaw;
        while (dyaw > PI) dyaw -= 2 * PI;
        while (dyaw < -PI) dyaw += 2 * PI;
        float dpitch = asinf(clampf(d.y / dist, -1, 1)) - P.pitch;
        float ang = sqrtf(dyaw * dyaw + dpitch * dpitch);
        if (ang < bestAng && los(eye, E[i].pos)) { bestAng = ang; bestC = E[i].pos; haveTarget = 1; }
    }
    if (dmMode)
        for (int i = 0; i < MAXP; i++) {
            if (!np_target(i)) continue;
            V3 c = np_center(i), d = vsub(c, eye);
            float dist = vlen(d);
            if (dist < 1 || dist > AIMASSIST_RANGE) continue;
            float dyaw = atan2f(d.x, -d.z) - P.yaw;
            while (dyaw > PI) dyaw -= 2 * PI;
            while (dyaw < -PI) dyaw += 2 * PI;
            float dpitch = asinf(clampf(d.y / dist, -1, 1)) - P.pitch;
            float ang = sqrtf(dyaw * dyaw + dpitch * dpitch);
            if (ang < bestAng && los(eye, c)) { bestAng = ang; bestC = c; haveTarget = 1; }
        }
    if (!haveTarget) return;
    V3 d = vsub(bestC, eye);
    float dist = fmaxf(vlen(d), 0.01f);
    float dyaw = atan2f(d.x, -d.z) - P.yaw;
    while (dyaw > PI) dyaw -= 2 * PI;
    while (dyaw < -PI) dyaw += 2 * PI;
    float dpitch = asinf(clampf(d.y / dist, -1, 1)) - P.pitch;
    float w = fminf(0.5f, dt * AIMASSIST_RATE) * (1 - bestAng / AIMASSIST_CONE);   /* plus fort pres du centre */
    P.yaw += dyaw * w;
    P.pitch = clampf(P.pitch + dpitch * w, -1.55f, 1.55f);
}

/* Le bot joue comme un joueur de movement shooter : bhop permanent, air-strafe en balancant
 * la vue autour de la cible (en tenant le strafe du cote ou il tourne), dash et grappin pour
 * combler la distance, visee recalee au moment de tirer, corps a corps au contact. */
/* --- Competences du bot (la PIECE est jouee avec le rail, la Tesla et le fleau, dans bot_think) : un usage simple
 * par competence, avec le temps de reaction et l'imprecision du bot. Appele en dernier : la visee posee ici reste --- */
static int allin_ok(int w);
static int bot_near_wall(V3 c) {                          /* un mur a moins de 3 m de la cible (rebond possible) */
    static const float dx[4] = { 1, -1, 0, 0 }, dz[4] = { 0, 0, 1, -1 };
    for (int k = 0; k < 4; k++) if (ray_world(c, v3(dx[k], 0, dz[k]), 3, 0) < 3) return 1;
    return 0;
}
static void bot_skill(float dt, int best, float bd, int see, float seeT, float aimYaw, float aimPitch, float errY, float errP) {
    static float next, hurtT = -10, lastHp = 100, hiddenT;
    if (P.hp < lastHp - 0.5f) hurtT = botT;
    lastHp = P.hp;
    if (best >= 0 && !see) hiddenT += dt; else hiddenT = 0;
    if (P.skill == SK_COIN || (P.skillCh < 1 && !P.ricArmed)) return;
    int react = see && seeT > botReactT[botLevel];
    switch (P.skill) {
    case SK_MARK:                                         /* MARQUE : cible vue entre 10 et 60 m, pas deja marquee */
        if (best >= 0 && react && bd > 10 && bd < 60 && markP[best] <= 0 && botT > next) {
            P.yaw = aimYaw + errY; P.pitch = aimPitch + errP;
            botPress[A_SKILL] = 1; next = botT + 1.5f;
            netlog("competence : marque sur %s (%.1f m)", pname(best), bd);
        }
        break;
    case SK_CHARGE:                                       /* CHARGE : maintenue quand une cible approche, pour le prochain tir */
        if (best >= 0 && see && bd < 30 && P.chargeT <= 0 && wp_has_ammo(P.weapon)) botHeld[A_SKILL] = 1;
        break;
    case SK_RICOCHET: {
        int gun = P.weapon == WP_RAIL || P.weapon == WP_ROCKET || P.weapon == WP_KINETIC;
        if (!P.ricArmed) {                                /* armee si la cible est a couvert ou pres d'un mur */
            if (best >= 0 && gun && wp_has_ammo(P.weapon) && bd < 45 && botT > next &&
                (hiddenT > botReactT[botLevel] || bot_near_wall(np_center(best)))) {
                botPress[A_SKILL] = 1; next = botT + 1;
                netlog("competence : ricochet arme (%s %s)", pname(best), hiddenT > 0 ? "a couvert" : "pres d'un mur");
            }
        } else if (best >= 0 && !see && bd < 40 && hiddenT > botReactT[botLevel] + 0.3f && P.weapon != WP_ROCKET && gun &&
                   wp_has_ammo(P.weapon) && P.fireCd <= 0 && P.switchT <= 0) {
            /* cible cachee : tir par le sol, on vise son reflet sous le sol (un rebond) */
            V3 eye = eye_pos(), t = np_center(best);
            float down = ray_world(t, v3(0, -1, 0), 30, 0), fy = t.y - down;
            if (down >= 30 || eye.y <= fy + 0.3f) break;
            V3 tm = v3(t.x, 2 * fy - t.y, t.z), dm = vsub(tm, eye);
            float s = (eye.y - fy) / (eye.y - tm.y);
            V3 b = vadd(eye, vmul(dm, s)), up = v3(b.x, fy + 0.05f, b.z), n;
            float lb = vlen(vsub(b, eye));
            if (!los(eye, up) || !los(up, t) || ray_world(eye, vnorm(dm), lb + 0.2f, &n) < lb - 0.3f || n.y < 0.9f) break;
            P.yaw = atan2f(dm.x, -dm.z) + errY;
            P.pitch = asinf(clampf(dm.y / fmaxf(vlen(dm), 0.01f), -1, 1)) + errP;
            botHeld[A_FIRE] = 1;
            netlog("competence : ricochet par le sol vers %s (%.1f m)", pname(best), bd);
        }
        break;
    }
    case SK_ALLIN:                                        /* ALL IN ONE : de pres, au moins 3 munitions */
        if (best >= 0 && react && bd > 4 && bd < 14 && allin_ok(P.weapon) && P.ammo[P.weapon] >= 3 && P.switchT <= 0 && botT > next) {
            P.yaw = aimYaw + errY; P.pitch = aimPitch + errP;
            botPress[A_SKILL] = 1; next = botT + 1;
            netlog("competence : all in one sur %s (%.1f m, %d munitions)", pname(best), bd, P.ammo[P.weapon]);
        }
        break;
    case SK_AIM:                                          /* AIMANT : a l'engagement, cible vue a portee */
        if (best >= 0 && react && bd < AIMASSIST_RANGE * 0.7f && P.aimT <= 0 && botT > next) {
            botPress[A_SKILL] = 1; next = botT + 2;
            netlog("competence : aimant sur %s (%.1f m)", pname(best), bd);
        }
        break;
    case SK_DECOY: {                                      /* MIRAGE : bas en PV ou sous le feu (apres le temps de reaction) */
        float since = botT - hurtT;
        if (best >= 0 && bd < 50 && botT > next &&
            (P.hp < 45 || (since > botReactT[botLevel] && since < botReactT[botLevel] + 0.4f))) {
            botPress[A_SKILL] = 1; next = botT + 2.5f;
            if (P.dashCharge >= 1) {                      /* puis un pas de cote : le leurre reste a ma place */
                botPress[A_DASH] = 1; botHeld[((int)(botT * 10)) % 2 ? A_LEFT : A_RIGHT] = 1; botNextDash = botT + 1.2f;
            }
            netlog("competence : mirage (%.0f PV%s)", P.hp, P.hp < 45 ? "" : ", sous le feu");
        }
        break;
    }
    }
}

static void bot_think(float dt) {
    botT += dt;
    if (appState == APP_JOIN && botLocal && !join_busy()) {
        struct sockaddr_in a;
        memset(&a, 0, sizeof a);
        a.sin_family = AF_INET; a.sin_port = htons(NET_PORT); a.sin_addr.s_addr = inet_addr("127.0.0.1");
        net_connect(a);
    } else if (appState == APP_JOIN && nFound > 0 && !join_busy()) join_found(0, botSpec);
    if (appState != APP_GAME) return;
    memset(botHeld, 0, sizeof botHeld);
    botHeld[A_FIRE] = 0;
    if (P.dead) return;
    float hs = hlen(P.vel);
    botSpeedSum += hs * dt; botAliveT += dt; botMaxSpeed = fmaxf(botMaxSpeed, hs);

    /* --- Egide : bouclier leve face a un rail (ou un Elan) qui me vise de loin, ou a une roquette qui arrive (une fois sur
     * deux) : 0,6 s au plus, puis 0,8 s baisse pour regenerer ; jamais sous 40 de jauge. Grappin sur un joueur : belier --- */
    if (P.meleeKind == MK_SHIELD) {
        static double shUntil, shRest;
        static unsigned int shRocket;
        static int shRocketGo, shWho = -1;
        V3 eye = eye_pos(), at = v3(0, 0, 0);
        int threat = -1;
        if (P.shieldState != SHS_BROKEN && P.shieldRipT <= 0 && P.shieldGauge >= 40 && nowT >= shRest) {
            for (int i = 0; i < MAXP && threat < 0; i++) {
                if (!np_target(i) || (NP[i].st.weapon != WP_RAIL && NP[i].st.weapon != WP_KINETIC)) continue;
                V3 e = np_eye(i), to = vsub(pcenter(), e);
                float dl = vlen(to);
                if (dl < 8 || vdot(np_face(i), vmul(to, 1.0f / dl)) < 0.97f || !los(eye, e)) continue;
                threat = i; at = np_center(i);
            }
            for (int i = 0; i < MAXPR && threat < 0; i++) {
                Proj *p = &PR[i];
                if (!p->alive || (p->kind != PR_ROCKET && p->kind != PR_PARRIED) || p->owner == myId) continue;
                V3 to = vsub(eye, p->pos);
                float d = vlen(to);
                if (d > 8 || d < MELEE_RANGE + 1.2f || vdot(p->vel, to) <= 0) continue;
                if (shRocket != p->netId) { shRocket = p->netId; shRocketGo = frand() < 0.5f; }
                if (shRocketGo) { threat = MAXP; at = p->pos; }
            }
        }
        if (threat >= 0 && nowT >= shUntil && P.shieldState != SHS_UP) {
            shUntil = nowT + 0.6; shWho = threat;
            netlog("bouclier leve contre %s", threat < MAXP ? pname(threat) : "une roquette");
        }
        if (nowT < shUntil && P.shieldState != SHS_BROKEN && P.shieldRipT <= 0 && P.shieldGauge >= 20) {
            if (threat >= 0) {                            /* face a la menace */
                V3 d = vsub(at, eye);
                P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(clampf(d.y / fmaxf(vlen(d), 0.01f), -1, 1));
            }
            botHeld[A_MELEE] = 1; botHeld[A_FWD] = 1; botHeld[A_JUMP] = 1;
            if (P.shieldState == SHS_DOWN) botPress[A_MELEE] = 1;
            return;
        }
        if (shUntil > 0 && nowT >= shUntil) {             /* fin du blocage : on baisse pour regenerer */
            shUntil = 0; shRest = nowT + 0.8;
            netlog("bouclier baisse (contre %s), jauge %.0f", shWho >= 0 && shWho < MAXP ? pname(shWho) : "roquette", P.shieldGauge);
        }
        if (P.hook == HOOK_ATTACHED && P.hookPlayer && P.shieldState != SHS_BROKEN && P.shieldRipT <= 0) {
            botHeld[A_MELEE] = 1;                         /* belier : bouclier tenu pendant la traction */
            if (P.shieldState == SHS_DOWN) botPress[A_MELEE] = 1;
        }
    }
    /* --- fin Egide --- */

    /* un adversaire tout proche lance son epee : une fois sur deux, on pare (ou on frappe en meme temps) */
    static double swingSeenT;
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i) || !swing_start(i) || nowT - swingSeenT < 0.4) continue;
        if (vlen(vsub(NP[i].disp, P.pos)) > ((NP[i].st.pad & 0x0F) == MK_FLAIL ? 7 : 5)) continue;   /* Fleau : 7 m */
        swingSeenT = nowT;
        if (frand() < botParryP[botLevel] * ((NP[i].st.pad & 0x0F) == MK_FIST ? 0.68f : 1.0f) && P.meleeCd <= 0) {   /* Poings : fenetre courte, parry plus rare */
            V3 d = vsub(np_center(i), eye_pos());
            P.yaw = atan2f(d.x, -d.z);
            botPress[A_MELEE] = 1;
            netlog("parry d'epee tente contre %s", pname(i));
            return;
        }
    }

    /* Tesla adverse : son arc est sur moi. Une fois toutes les 1,5 s, 35 % de chances de tenter un parry
     * (apres 0,25 s de reaction) pour renvoyer la decharge */
    {
        static double zapSeenT, zapAt;
        static int zapGo;
        for (int i = 0; i < MAXP; i++) {
            if (!np_target(i) || !(NP[i].st.pad & ST_ZAP) || NP[i].st.weapon != WP_TESLA) continue;
            V3 e = np_eye(i), to = vsub(pcenter(), e);
            float dl = vlen(to);
            if (dl > tesla_range(np_vel(i)) || vdot(yp_dir(NP[i].st.yaw, NP[i].st.pitch), vmul(to, 1.0f / fmaxf(dl, 0.01f))) < 0.98f) continue;
            if (nowT - zapSeenT > 1.5) { zapSeenT = nowT; zapGo = frand() < 0.35f; zapAt = nowT + 0.25; }
            if (zapGo && nowT >= zapAt && P.meleeCd <= 0) {
                V3 d = vsub(np_center(i), eye_pos());
                P.yaw = atan2f(d.x, -d.z);
                botPress[A_MELEE] = 1; zapGo = 0;
                netlog("parry tesla tente contre %s", pname(i));
                return;
            }
        }
    }
    /* --- Lance --- adverse couchee qui me vise : une fois sur deux on pare sur l'approche (la pointe brille),
     * sinon 3 fois sur 10 un pas de cote (dash lateral) pour sortir du cone */
    {
        static double lanceSeenT[MAXP];
        static int lancePlan[MAXP], lanceSide[MAXP];      /* 1 = parer, 2 = pas de cote, 3 = rien, 4 = pas de cote en cours */
        for (int i = 0; i < MAXP; i++) {
            if (!np_target(i) || !(NP[i].st.pad & ST_LANCE)) continue;
            V3 d = vsub(pcenter(), np_center(i));
            float l = fmaxf(vlen(d), 0.01f), reach = l - 0.5f;
            V3 dir = vmul(d, 1.0f / l);
            if (l > 14 || vdot(dir_of(NP[i].st.yaw, NP[i].st.pitch), dir) < LANCE_CLASH_COS) continue;
            if (nowT - lanceSeenT[i] > 1.5) {
                float r = frand();
                lancePlan[i] = r < 0.5f ? 1 : r < 0.8f ? 3 : 2;
                lanceSide[i] = frand() < 0.5f ? A_LEFT : A_RIGHT;
            }
            lanceSeenT[i] = nowT;
            float vc = vdot(vsub(np_vel(i), P.vel), dir);
            if (P.meleeKind == MK_LANCE && P.lanceState == LS_COUCH) continue;   /* deux lances couchees : choc de joute */
            if (lancePlan[i] == 1 && vc > 8 && reach - LANCE_REACH < vc * 0.12f && P.meleeCd <= 0 && P.lanceState == LS_IDLE &&
                (P.meleeKind != MK_ANVIL || P.grounded || anvil_ground_dist(P.pos) < SLAM_MIN_H)) {
                V3 back = vmul(dir, -1);
                P.yaw = atan2f(back.x, -back.z); P.pitch = asinf(clampf(back.y, -1, 1));
                botPress[A_MELEE] = 1; lancePlan[i] = 0;
                netlog("parry de la lance tente contre %s (v_c %.1f, %.1f m)", pname(i), vc, reach);
                return;
            }
            if ((lancePlan[i] == 2 || lancePlan[i] == 4) && reach < 8 && vc > 4) {
                V3 back = vmul(dir, -1);
                P.yaw = atan2f(back.x, -back.z); P.pitch = 0;
                botHeld[lanceSide[i]] = 1; botHeld[A_JUMP] = 1;
                if (P.dashCharge >= 1 && botT > botNextDash) { botPress[A_DASH] = 1; botNextDash = botT + 1.2f; }
                if (lancePlan[i] == 2) { lancePlan[i] = 4; netlog("pas de cote devant la lance de %s (%.1f m)", pname(i), reach); }
                return;
            }
        }
    }
    /* --- fin Lance --- */

    /* Enclume adverse : un pique tombe pres de nous. Juste avant que le front arrive, une fois sur deux on pare l'onde ;
     * sinon on saute (saut puis double saut au sommet, pour passer le mur) ou, deja en l'air, on sort de l'onde par un dash */
    static double slamSeenT[MAXP];
    static int slamPlan[MAXP];                            /* 1 = parer, 2 = esquiver, 3 = esquive en cours */
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i) || !(NP[i].st.pad & ST_SLAM)) continue;
        V3 d = NP[i].disp, away = v3(P.pos.x - d.x, 0, P.pos.z - d.z);
        if (hlen(away) > 10) continue;                    /* onde du plan 1b : jusqu'a 9 m de rayon (plus en ovale) */
        float t = (d.y - P.pos.y) / fmaxf(-NP[i].st.vel[1], SLAM_VY) + hlen(away) / SLAM_WAVE_SPEED;   /* arrivee du front */
        if (nowT - slamSeenT[i] > 1.5) { slamSeenT[i] = nowT; slamPlan[i] = frand() < 0.5f ? 1 : 2; }
        if (slamPlan[i] == 1 && t < 0.12f && P.meleeCd <= 0 &&
            (P.meleeKind != MK_ANVIL || P.grounded || anvil_ground_dist(P.pos) < SLAM_MIN_H)) {
            botPress[A_MELEE] = 1; slamPlan[i] = 0;
            netlog("parry de l'onde tente contre %s", pname(i));
            return;
        }
        if (slamPlan[i] >= 2 && t < 0.35f) {
            if (slamPlan[i] == 2) { slamPlan[i] = 3; netlog("esquive de l'onde de %s (%.2f s avant l'impact)", pname(i), t); }
            botHeld[A_JUMP] = 1;
            if (P.grounded || (P.airJumps > 0 && P.vel.y < 1)) botPress[A_JUMP] = 1;   /* saut, puis double saut au sommet */
            else if (P.vel.y < 0 && P.dashCharge >= 1) {
                P.yaw = atan2f(away.x, -away.z); P.pitch = 0;
                botHeld[A_FWD] = 1; botPress[A_DASH] = 1;
            }
            return;
        }
    }

    /* une roquette adverse arrive : on tente un parry au bon moment, mais pas a tous les coups */
    static unsigned int parryDecided;
    static int parryGo;
    for (int i = 0; i < MAXPR; i++) {
        Proj *p = &PR[i];
        int sawIn = p->kind == PR_SAW && p->sawState == SAW_FLY;   /* Scie adverse en vol : meme reflexe */
        if (!p->alive || (p->kind != PR_ROCKET && p->kind != PR_PARRIED && !sawIn) || p->owner == myId) continue;
        V3 to = vsub(eye_pos(), p->pos);
        float d = vlen(to);
        if (d > 10 || vdot(p->vel, to) <= 0) continue;
        if (parryDecided != p->netId) { parryDecided = p->netId; parryGo = frand() < 0.6f; }
        if (parryGo && d < MELEE_RANGE + 1.2f && P.meleeCd <= 0) {
            V3 back = vmul(to, -1.0f / d);
            P.yaw = atan2f(back.x, -back.z);
            P.pitch = asinf(clampf(back.y, -1, 1));
            botPress[A_MELEE] = 1;
            botHeld[A_JUMP] = 1;
            netlog("parry tente sur une %s de %s (%.1f m)", sawIn ? "scie" : "roquette", pname(p->owner), d);
            return;
        }
    }

    /* --- Ventouse : une mine adverse collee sur moi. On decide une fois par mine (60 %), puis, apres 0,15 a 0,45 s
     * de reaction, parry en visant l'adversaire le plus proche (la mine arrachee part dans la direction de visee) --- */
    {
        static unsigned int mineKey;
        static int mineGo;
        static double mineAt;
        for (int i = 0; i < MAXPR; i++) {
            Proj *p = &PR[i];
            if (!p->alive || p->kind != PR_MINE || p->mineState != MS_PLAYER || p->mineAttach != myId + 1) continue;
            if (mineKey != p->netId) {
                mineKey = p->netId; mineGo = frand() < 0.6f; mineAt = nowT + frange(0.15f, 0.45f);
                netlog("ventouse : mine de %s sur moi, %s", pname(p->owner), mineGo ? "arrachage prevu" : "pas de reaction");
            }
            if (mineGo && nowT >= mineAt && P.meleeCd <= 0) {
                int tg = -1;
                float td = 1e9f;
                for (int j = 0; j < MAXP; j++)
                    if (np_target(j) && vlen(vsub(NP[j].disp, P.pos)) < td) { td = vlen(vsub(NP[j].disp, P.pos)); tg = j; }
                if (tg >= 0) {
                    V3 d = vsub(np_center(tg), eye_pos());
                    P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(clampf(d.y / fmaxf(vlen(d), 0.01f), -1, 1));
                }
                botPress[A_MELEE] = 1; mineGo = 0;
                netlog("ventouse : arrachage tente");
                return;
            }
            break;
        }
    }
    /* --- fin Ventouse --- */

    /* Scie : ma scie revient ; une fois sur deux, on se tourne vers une cible visible pour la renvoyer */
    {
        static unsigned int bounceKey;
        static int bounceGo;
        Proj *s = saw_mine();
        int st = s ? s->sawState : -1;
        if (s && (st == SAW_RETURN || st == SAW_RECALL || st == SAW_PARRIED) && s->sawBounces < SAW_BOUNCE_CHAIN && P.meleeCd <= 0) {
            V3 eye = eye_pos(), to = vsub(s->pos, eye);
            float d = vlen(to);
            unsigned int key = s->netId ^ ((unsigned int)s->sawBounces << 28) ^ ((unsigned int)st << 25) ^ sawCounter;
            if (d < 6) {
                if (bounceKey != key) { bounceKey = key; bounceGo = frand() < 0.5f; }
                int tg = -1;
                float td = SAW_BOUNCE_AIM_RANGE;
                for (int i = 0; i < MAXP; i++) {
                    if (!np_target(i)) continue;
                    float dd = vlen(vsub(np_center(i), eye));
                    if (dd < td && los(eye, np_center(i))) { td = dd; tg = i; }
                }
                if (bounceGo && tg >= 0 && d < MELEE_RANGE + 1.7f) {
                    V3 c = np_center(tg), dt2 = vnorm(vsub(c, eye));
                    if (vdot(vmul(to, 1.0f / fmaxf(d, 0.01f)), dt2) >= 0.34f) {   /* la scie arrive dans un cone de 70 degres */
                        float err = 0.02f * sinf(botT * 7.3f + botMode);
                        P.yaw = atan2f(dt2.x, -dt2.z) + err; P.pitch = asinf(clampf(dt2.y, -1, 1)) + err * 0.6f;
                        botPress[A_MELEE] = 1; bounceGo = 0;
                        sawStat[SS_TRY]++;
                        netlog("scie : renvoi tente (%d) vers %s", s->sawBounces + 1, pname(tg));
                        return;
                    }
                }
            }
        }
    }

    int best = -1;
    float bd = 1e9f;
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i)) continue;
        float d = vlen(vsub(NP[i].disp, P.pos));
        if (d < bd) { bd = d; best = i; }
    }
    /* bhop : on ne touche jamais le sol longtemps -- sauf en facile, qui perd son chainage une bonne partie
     * du temps (sinon, meme avec une mauvaise visee, un bot qui garde un bhop parfait reste trop fuyant) */
    botHeld[A_JUMP] = botLevel > 0 || fmodf(botT, 1.4f) < 0.8f;
    static float searchYaw, stuckT, escapeT, escapeDir = 1;
    static int escapeHook;
    static float seeT;
    float aimYaw, aimPitch = 0;
    int see = 0;
    if (best >= 0) {
        V3 eye = eye_pos(), c = np_center(best), d = vsub(c, eye);
        aimYaw = atan2f(d.x, -d.z);
        aimPitch = asinf(clampf(d.y / fmaxf(vlen(d), 0.01f), -1, 1));
        see = los(eye, c);
        searchYaw = aimYaw;
    } else {
        aimYaw = (searchYaw += dt * 0.6f);                /* personne : on tourne en cherchant */
    }
    {   /* MIRAGE adverse : comme un humain, le bot s'y laisse prendre une fois sur deux, 0,4 a 1 s puis il comprend */
        static unsigned int decoyKey;
        static float decoyUntil;
        for (int i = 0; best >= 0 && i < MAXCOIN; i++) {
            Coin *c = &CO[i];
            if (!c->alive || !c->decoy || c->owner != best) continue;
            if (decoyKey != c->netId) {
                decoyKey = c->netId;
                decoyUntil = frand() < 0.5f ? botT + frange(0.4f, 1.0f) : 0;
                netlog("mirage de %s : %s", pname(best), decoyUntil > 0 ? "le bot vise le leurre" : "le bot n'y croit pas");
            }
            V3 eye = eye_pos(), d = vsub(c->pos, eye);
            if (botT < decoyUntil && los(eye, c->pos)) {
                aimYaw = atan2f(d.x, -d.z);
                aimPitch = asinf(clampf(d.y / fmaxf(vlen(d), 0.01f), -1, 1));
                see = 1;
            }
            break;
        }
    }

    /* une arme a sec et personne de proche : on va chercher une cellule (les grandes d'abord) */
    int seek = -1;
    int teslaLow = (P.loadout[0] == WP_TESLA || P.loadout[1] == WP_TESLA) && P.ammo[WP_TESLA] < 10;   /* Tesla : batterie < 10 */
    if (((P.loadout[0] != WP_SAW && !wp_has_ammo(P.loadout[0])) || (P.loadout[1] != WP_SAW && !wp_has_ammo(P.loadout[1])) || teslaLow) &&
        (best < 0 || bd > 25)) {                          /* la Scie n'a pas de jauge : les cellules ne la rendent pas */
        float sd = 90;
        for (int i = 0; i < npu; i++) {
            if (PU[i].t > 0 || PU[i].pos.y - P.pos.y > 3) continue;   /* seulement celles a portee de saut */
            float d = vlen(vsub(PU[i].pos, P.pos)) - (PU[i].pts == PICKUP_BIG ? 8 : 0);
            if (d < sd) { sd = d; seek = i; }
        }
    }
    if (seek >= 0) {
        V3 d = vsub(PU[seek].pos, eye_pos());
        aimYaw = atan2f(d.x, -d.z);
        aimPitch = asinf(clampf(d.y / fmaxf(vlen(d), 0.01f), -1, 1));
        best = -1; bd = 1e9f; see = 0;
    }
    {   /* Scie plantee et personne a moins de 8 m : rappel au grappin si on la voit, sinon on va la ramasser */
        Proj *bs = saw_mine();
        if (bs && bs->sawState == SAW_PLANTED && (best < 0 || bd > 8)) {
            V3 eye = eye_pos(), d = vsub(bs->pos, eye);
            float l = vlen(d), sy2 = atan2f(d.x, -d.z), sp2 = asinf(clampf(d.y / fmaxf(l, 0.01f), -1, 1));
            if (l < GRAPPLE_RANGE - 2 && P.hook == HOOK_NONE && P.hookCd <= 0 && los(eye, vadd(bs->pos, vmul(bs->sawN, 0.25f)))) {
                P.yaw = sy2; P.pitch = sp2;
                botPress[A_HOOK] = 1; botHeld[A_HOOK] = 1;
                netlog("scie : rappel tente (%.1f m)", l);
                return;
            }
            aimYaw = sy2; aimPitch = sp2;
            best = -1; bd = 1e9f; see = 0; seek = -1;
        }
    }

    /* --- Navigation : cible cachee, recharge a aller chercher, ou de l'autre cote d'un vide : on suit le graphe
     * (sauts, pads, grappin d'ancre en ancre). En vue directe, on alterne : tir quand l'arme est prete, sinon trajet. --- */
    if (see) seeT += dt; else seeT = 0;
    {
        static float navHiddenT;
        navHiddenT = see ? 0 : navHiddenT + dt;
        static int patrol = -1;                           /* personne en vue ni recharge : ronde vers un point du graphe */
        static float patrolT;
        if (seek < 0 && best < 0) {
            patrolT -= dt;
            if (nnav > 0 && (patrol < 0 || patrol >= nnav || patrolT <= 0 || hlen(vsub(NV[patrol].p, P.pos)) < 3)) {
                for (int k = 0; k < 20; k++) { patrol = (int)(frand() * nnav) % nnav; if (NV[patrol].kind == NN_SURF) break; }
                patrolT = 10;
            }
        } else patrol = -1;
        int hasGoal = seek >= 0 || best >= 0 || (patrol >= 0 && patrol < nnav);
        V3 goal = seek >= 0 ? PU[seek].pos : best >= 0 ? NP[best].disp : hasGoal ? NV[patrol].p : P.pos;
        V3 og;                                            /* modes : l'objectif d'abord, sauf ennemi vu de pres */
        int objective = mode_bot_goal(&og) && !(best >= 0 && see && bd < 22) && seek < 0;
        if (objective) {
            goal = og; hasGoal = 1;
            float hr = rules.mode == MODE_ZONES ? zone_r() * 0.6f : rules.mode == MODE_BOUCLIER ? shield_r() * 0.5f : 0.8f;
            float od = hlen(vsub(og, P.pos));
            int shoot = best >= 0 && see && seeT > botReactT[botLevel] && P.fireCd <= 0 && P.switchT <= 0 && wp_has_ammo(P.weapon);
            if (od < hr && fabsf(og.y - P.pos.y) < 2.5f) {   /* sur l'objectif : on tient, on surveille */
                static float holdYaw;
                holdYaw += dt * 0.9f;
                P.yaw = best >= 0 && see ? aimYaw : holdYaw; P.pitch = best >= 0 && see ? aimPitch : 0;
                if (shoot) botHeld[A_FIRE] = 1;
                return;
            }
            if (od < 10 && los(eye_pos(), vadd(og, v3(0, 1, 0)))) {   /* tout pres et en vue : droit dessus (le graphe s'arrete parfois avant) */
                V3 d = vsub(og, P.pos);
                P.yaw = atan2f(d.x, -d.z); P.pitch = 0;
                botHeld[A_FWD] = 1;
                if (P.grounded && og.y > P.pos.y + 0.5f) botPress[A_JUMP] = 1;
                if (shoot) { P.yaw = aimYaw; P.pitch = aimPitch; botHeld[A_FIRE] = 1; }
                return;
            }
        }
        if (best < 0 && seek < 0 && hasGoal && navMap != curMap) hasGoal = 0;   /* graphe pas encore construit */
        int navOn = 0, wasBusy = navBusy;
        if (hasGoal && !P.hookPlayer && nav_plan(goal, 0, dt)) {
            int cur = navStep > 0 ? navPath[navStep - 1] : navFrom, t0 = nav_edge_type(cur, navPath[navStep]);
            int canShoot = see && seeT > botReactT[botLevel] && bd < 55 && P.fireCd <= 0 && P.switchT <= 0;
            int midAir = !P.grounded && (t0 != NE_WALK || navLanding);   /* en plein trajet (pad, saut, grappin) : on le finit */
            navOn = seek >= 0 || best < 0 || objective || navHiddenT > 0.35f || wasBusy || midAir || t0 != NE_WALK;   /* (on tire en route, plus bas) */
            (void)canShoot;
            if (best >= 0 && see && bd < 9 && t0 == NE_WALK && !midAir) navOn = 0;   /* au contact : on se bat */
        } else navBusy = 0;
        if (navOn) {
            static int lastType = -1;
            int ty = nav_follow(dt);
            if (ty != lastType && ty >= 0) {
                static const char *tn[5] = { "marche", "saut", "pad", "grappin", "lacher" };
                netlog("nav : %s vers (%.0f %.0f %.0f), %d etapes", tn[ty], NV[navPath[navStep < navLen ? navStep : navLen - 1]].p.x,
                       NV[navPath[navStep < navLen ? navStep : navLen - 1]].p.y, NV[navPath[navStep < navLen ? navStep : navLen - 1]].p.z, navLen - navStep);
            }
            lastType = ty;
            if (ty >= 0) {
                if (P.weapon != P.loadout[0] && wp_has_ammo(P.loadout[0])) P.weapon = P.loadout[0];
                /* tir en route (pas pendant la traction du grappin) : la visee ne sert qu'un tick, la trajectoire continue */
                int canShoot = best >= 0 && see && seeT > botReactT[botLevel] && bd < 55 && P.fireCd <= 0 && P.switchT <= 0 &&
                               P.weapon != WP_SAW && P.weapon != WP_MINE && wp_has_ammo(P.weapon);
                if (canShoot && ty != NE_HOOK && !navLanding) {
                    float e = (0.012f + bd * 0.0014f) * botAimMul[botLevel];
                    P.yaw = aimYaw + e * sinf(botT * 7.3f + botMode); P.pitch = aimPitch + e * 0.6f * cosf(botT * 5.1f);
                    botHeld[A_FIRE] = 1;
                }
                return;
            }
        }
    }

    /* comme un humain : temps de reaction, et une visee moins precise de loin */
    float aimErr = (0.012f + (best >= 0 ? bd : 0) * 0.0014f) * botAimMul[botLevel];
    float aimErrYaw = aimErr * sinf(botT * 7.3f + botMode), aimErrPitch = aimErr * 0.6f * cosf(botT * 5.1f);
    if (!botAimTest) {                                    /* un tirage par tir (refait apres chaque coup) + leger tremblement */
        static float offX, offY;
        static int rolled;
        if (P.fireCd > 0) rolled = 0;
        else if (!rolled) { bot_roll_aim(best >= 0 ? bd : 20, &offX, &offY); rolled = 1; }
        float d = fmaxf(best >= 0 ? bd : 20, 2);
        aimErrYaw = offX / d + 0.003f * sinf(botT * 7.3f + botMode);
        aimErrPitch = offY / d + 0.002f * cosf(botT * 5.1f);
    }

    /* --- Fleau : combo fleau -> tir. Moins de 0,3 s apres une touche, la cible vole vers nous (parabole connue) :
     * on la tire, sans erreur de visee supplementaire, avant toute autre manoeuvre (le dash rendu lancerait un cross-up) --- */
    if (P.meleeKind == MK_FLAIL && P.flailLastT > FLAIL_COMBO_T - 0.3f && P.flailLastTgt > 0 && np_target(P.flailLastTgt - 1)) {
        V3 eye = eye_pos(), c = np_center(P.flailLastTgt - 1), d = vsub(c, eye);
        if ((P.loadout[0] == WP_RAIL || P.loadout[1] == WP_RAIL) && wp_has_ammo(WP_RAIL)) P.weapon = WP_RAIL;
        if (P.weapon != WP_SAW && wp_has_ammo(P.weapon) && los(eye, c)) {
            P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(clampf(d.y / fmaxf(vlen(d), 0.01f), -1, 1));
            if (P.fireCd <= 0 && P.switchT <= 0) botHeld[A_FIRE] = 1;
            return;
        }
    }                                                     /* --- fin Fleau --- */

    /* --- Poings : apres un uppercut qui touche, la cible suit une trajectoire balistique : tir immediat (rail de
     * preference). Puis punch-jump vers une cible plus haute : en chute, accroupi (l'oeil descend), coup vers le sol --- */
    if (P.meleeKind == MK_FIST && P.fistUpT > 0 && P.fistUpTgt > 0 && np_target(P.fistUpTgt - 1)) {
        int tg = P.fistUpTgt - 1;
        V3 eye = eye_pos(), c = np_center(tg), d = vsub(c, eye);
        if ((P.loadout[0] == WP_RAIL || P.loadout[1] == WP_RAIL) && wp_has_ammo(WP_RAIL)) P.weapon = WP_RAIL;
        if (P.weapon != WP_SAW && P.weapon != WP_MINE && wp_has_ammo(P.weapon) && los(eye, c)) {
            P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(clampf(d.y / fmaxf(vlen(d), 0.01f), -1, 1));
            if (P.fireCd <= 0 && P.switchT <= 0) {
                botHeld[A_FIRE] = 1; P.fistUpT = 0; fistUpRails++;
                netlog("poings : uppercut -> tir sur %s", pname(tg));
            }
            return;
        }
    }
    if (P.meleeKind == MK_FIST && best >= 0 && !P.grounded && P.vel.y < -3 && P.fistPJ && P.fistStep == 0 &&
        P.wallJumpCd <= 0 && P.hook == HOOK_NONE && NP[best].disp.y - P.pos.y > 2 && P.fistAirT >= FIST_PJ_FLOOR_DELAY &&
        anvil_ground_dist(P.pos) < 2.5f) {
        botHeld[A_CROUCH] = 1;
        if (P.meleeCd <= 0 && ray_world(eye_pos(), v3(0, -1, 0), FIST_PJ_FLOOR_RANGE, 0) < FIST_PJ_FLOOR_RANGE - 0.05f) {
            P.pitch = -1.5f;
            botPress[A_MELEE] = 1;
            netlog("punch-jump tente vers %s (%.1f m plus haut)", pname(best), NP[best].disp.y - P.pos.y);
            return;
        }
    }                                                     /* --- fin Poings --- */

    /* CROSS-UP : monter au-dessus de l'adversaire (saut + double saut), dasher par-dessus,
     * se retourner et tirer dans son dos. */
    static int crossPhase;
    static float crossT, crossCd;
    static V3 crossDir;
    crossCd -= dt;

    /* Enclume : assez haut au-dessus d'une cible proche du sol, on pique si l'impact prevu tombe sur elle */
    if (best >= 0 && P.meleeKind == MK_ANVIL && P.anvilState == ANVIL_IDLE && P.meleeCd <= 0 && !P.grounded &&
        anvil_ground_dist(P.pos) >= 2.5f && anvil_ground_dist(NP[best].disp) < SLAM_REACH_UP) {
        V3 at; float vy, R, t;
        if (slam_predict(&at, &vy, &R, &t)) {
            V3 tp = vadd(NP[best].disp, v3(NP[best].st.vel[0] * t, 0, NP[best].st.vel[2] * t));
            float miss = hlen(vsub(at, tp));
            if (miss < 0.5f * R) {
                botPress[A_MELEE] = 1;
                crossPhase = 0; crossCd = 2;
                netlog("pique sur %s, %.1f m", pname(best), miss);
                return;
            }
        }
    }

    if (best >= 0) {
        V3 tp = NP[best].disp, hd = v3(tp.x - P.pos.x, 0, tp.z - P.pos.z);
        float dy = P.pos.y - tp.y, hdist = hlen(hd);
        if (crossPhase == 0 && crossCd <= 0 && see && hdist > 4 && hdist < 12 && P.dashCharge >= 1 && P.hook == HOOK_NONE) {
            crossPhase = 1; crossT = 0;
            netlog("cross-up : elan vers %s (%.1f m, %.1f m plus haut)", pname(best), hdist, dy);
        }
        if (crossPhase == 1) {                            /* 1. prendre de la hauteur */
            crossT += dt;
            botHeld[A_FWD] = botHeld[A_JUMP] = 1;
            P.yaw = aimYaw; P.pitch = aimPitch;
            if (!P.grounded && P.vel.y < 1.5f && P.airJumps > 0) botPress[A_JUMP] = 1;
            if (dy > 1.8f && !P.grounded) {
                crossPhase = 2; crossT = 0;
                crossDir = vnorm(hd);
                P.yaw = atan2f(crossDir.x, -crossDir.z);
                botPress[A_DASH] = 1;                     /* 2. dash par-dessus */
                netlog("cross-up : dash au-dessus de %s (%.1f m plus haut)", pname(best), dy);
            } else if (crossT > 1.2f) { crossPhase = 0; crossCd = 2; netlog("cross-up : abandon (%.1f m plus haut)", dy); }
        } else if (crossPhase == 2) {
            crossT += dt;
            botHeld[A_FWD] = 1;
            P.yaw = atan2f(crossDir.x, -crossDir.z);
            if (vdot(v3(tp.x - P.pos.x, 0, tp.z - P.pos.z), crossDir) < -1.0f || crossT > 0.7f) { crossPhase = 3; crossT = 0; }
        } else if (crossPhase == 3) {                     /* 3. se retourner et tirer dans le dos */
            crossT += dt;
            botHeld[A_JUMP] = 1;
            P.yaw = aimYaw; P.pitch = aimPitch;
            if (see && P.fireCd <= 0 && P.switchT <= 0) { P.yaw += aimErrYaw; botHeld[A_FIRE] = 1; }
            if (bd < bot_melee_reach(rel_speed(np_vel(best))) + 0.5f && P.meleeCd <= 0) botPress[A_MELEE] = 1;
            if (crossT > 0.5f) {
                crossPhase = 0; crossCd = 3;
                netlog("cross-up : retourne, tir sur %s", pname(best));
            }
        }
        if (crossPhase) { botHeld[A_HOOK] = 0; return; }
    } else crossPhase = 0;

    /* coince contre un obstacle : on part en biais, puis de l'autre cote, puis demi-tour */
    /* Poings : en combo, ou une cible a portee de poing, on n'est pas coince : on reste au contact */
    int fistClose = P.meleeKind == MK_FIST && (P.fistStep > 0 || (best >= 0 && bd < fist_range(rel_speed(np_vel(best))) + 0.3f));
    if (fistClose) escapeT = 0;
    if (hs < 3 && P.hook == HOOK_NONE && P.kinT <= 0 && !fistClose) stuckT += dt;   /* un tir d'Elan arrete net : pas coince */
    else stuckT = 0;
    if (stuckT > 0.4f) {
        static int escapeStep;
        static const float dirs[3] = { 1.3f, -1.3f, PI };
        escapeDir = dirs[escapeStep++ % 3];
        escapeT = 1.0f; stuckT = 0;
        escapeHook = P.hookCd <= 0;
    }

    /* air-strafe : la vue oscille de +-0.45 rad autour de la cible, strafe du cote ou elle tourne */
    float phase = botT * 2.6f, swing = 0.45f * sinf(phase);
    if (P.grounded || P.hook != HOOK_NONE || bd < 6) { swing = 0; botHeld[A_FWD] = 1; }
    else botHeld[cosf(phase) > 0 ? A_RIGHT : A_LEFT] = 1;
    P.yaw = aimYaw + swing;
    P.pitch = aimPitch;
    if (escapeT > 0) {
        escapeT -= dt;
        memset(botHeld, 0, sizeof botHeld);
        botHeld[A_FWD] = botHeld[A_JUMP] = 1;
        P.yaw = aimYaw + escapeDir;
        if (escapeHook) { P.pitch = 0.7f; botPress[A_HOOK] = 1; escapeHook = 0; }   /* grappin vers le haut pour s'extraire */
        else if (P.dashCharge >= 1 && botT > botNextDash) { botPress[A_DASH] = 1; botNextDash = botT + 1.2f; }
    }

    /* cible cachee derriere un obstacle (ou recharge en hauteur) : grappin vers le haut pour y aller */
    int seekHigh = seek >= 0 && PU[seek].pos.y - P.pos.y > 3;
    if (((best >= 0 && !see) || seekHigh) && P.hook == HOOK_NONE && P.hookCd <= 0 && botT > botNextHook) {
        P.yaw = aimYaw; P.pitch = seekHigh ? aimPitch + 0.25f : 0.6f;
        botPress[A_HOOK] = 1;
        botNextHook = botT + 1.5f;
    }

    /* le bot alterne ses deux armes (la 2e seulement de pres), et prend l'autre si l'une est a sec */
    int la = P.loadout[0], lb = P.loadout[1];
    int pref = (bd < 20 && ((int)(botT / 5 + botMode)) % 2) ? lb : la;
    if (pref == WP_SAW && (bd >= 30 || !see)) pref = pref == la ? lb : la;   /* Scie : seulement a moins de 30 m */
    if ((la == WP_SAW || lb == WP_SAW) && see && bd < 30 && wp_has_ammo(WP_SAW) && P.sawCd <= 0) pref = WP_SAW;
    if (!wp_has_ammo(pref)) pref = pref == la ? lb : la;
    if (la == WP_SHOTGUN || lb == WP_SHOTGUN) {           /* Eventail : seulement de pres (3 touches jusqu'a ~11 m) */
        int o = la == WP_SHOTGUN ? lb : la;
        if (wp_has_ammo(WP_SHOTGUN) && (bd < 10 || !wp_has_ammo(o))) pref = WP_SHOTGUN;
        else if (wp_has_ammo(o)) pref = o;
    }
    if (la == WP_TESLA || lb == WP_TESLA) {               /* Tesla : jusqu'a 20 m (pas a la place de l'Eventail tout pres) */
        int o = la == WP_TESLA ? lb : la;
        if (wp_has_ammo(WP_TESLA) && bd < 20 && !(pref == WP_SHOTGUN && bd < 10)) pref = WP_TESLA;
        else if (pref == WP_TESLA && wp_has_ammo(o)) pref = o;
    }
    if (la == WP_MINE || lb == WP_MINE) {                 /* --- Ventouse : piege sur une cible vue entre 6 et 20 m --- */
        int o = la == WP_MINE ? lb : la;
        int trap = wp_has_ammo(WP_MINE) && best >= 0 && see && bd > 6 && bd < 20 && mine_count() < MINE_MAX_ACTIVE;
        if (trap) pref = WP_MINE;
        else if (pref == WP_MINE && wp_has_ammo(o)) pref = o;
    }
    if (P.meleeKind == MK_ANVIL && P.anvilLandT < 1.0f && P.swingHitPts && wp_has_ammo(WP_RAIL) && (la == WP_RAIL || lb == WP_RAIL))
        pref = WP_RAIL;                                   /* onde qui touche : la cible vole ~1 s, le rail l'acheve */
    P.weapon = pref;
    int dry = !wp_has_ammo(la) && !wp_has_ammo(lb);    /* a sec : il ne tire plus et va au contact (la melee recharge) */
    /* contre-jeu : une piece adverse a portee ? on tire dedans pour la retourner contre son lanceur
     * (seulement avec les 2 rails : on ne gaspille pas le dernier) */
    if (P.weapon == WP_RAIL && P.ammo[WP_RAIL] >= 2 && P.fireCd <= 0 && P.switchT <= 0) {
        for (int i = 0; i < MAXCOIN; i++) {
            if (!CO[i].alive || CO[i].decoy || CO[i].owner == myId || CO[i].life > 3.7f) continue;   /* reaction de 0,3 s */
            if (CO[i].netId % 2) continue;                /* le bot ne contre qu'une piece sur deux */
            V3 eye = eye_pos(), d = vsub(CO[i].pos, eye);
            float l = vlen(d);
            if (l > 45 || !los(eye, CO[i].pos)) continue;
            P.yaw = atan2f(d.x, -d.z);
            P.pitch = asinf(clampf(d.y / l, -1, 1));
            botHeld[A_FIRE] = 1;
            netlog("contre : tir sur la piece de %s", pname(CO[i].owner));
            return;
        }
    }

    static int coinPlan;                                  /* a pleine vitesse : piece, puis tir dedans */
    static float coinPlanT, nextCoin;
    if (!coinPlan && P.skill == SK_COIN && see && bd > 10 && bd < 55 && hs > 11 && P.skillCh >= 1 && P.ammo[WP_RAIL] > 0 && P.weapon == WP_RAIL &&
        P.fireCd < 0.3f && botT > nextCoin) {
        P.yaw = aimYaw; P.pitch = aimPitch;
        botPress[A_SKILL] = 1;
        coinPlan = 1; coinPlanT = 0; nextCoin = botT + 2.5f;
    } else if (coinPlan) {
        coinPlanT += dt;
        int mine = -1;
        for (int i = 0; i < MAXCOIN; i++) if (CO[i].alive && CO[i].owner == myId) mine = i;
        if (mine < 0 || coinPlanT > 0.9f) coinPlan = 0;
        else if (coinPlanT > 0.3f && see && P.fireCd <= 0 && P.switchT <= 0) {   /* seulement si la cible est encore en vue */
            V3 d = vsub(CO[mine].pos, eye_pos());
            P.yaw = atan2f(d.x, -d.z);
            P.pitch = asinf(clampf(d.y / fmaxf(vlen(d), 0.01f), -1, 1));
            botHeld[A_FIRE] = 1;
            coinPlan = 0;
        }
    }
    if (!dry && !coinPlan && see && seeT > botReactT[botLevel] && bd < 55 && P.fireCd <= 0 && P.switchT <= 0 &&
        P.weapon != WP_KINETIC && P.weapon != WP_SAW && P.weapon != WP_TESLA &&   /* recale la visee (Elan, Scie, Tesla : leur regle) */
        (P.weapon != WP_SHOTGUN || bd < (P.sliding ? 13.0f : 11.0f))) {   /* Eventail : au-dela, moins de 4 plombs */
        P.yaw = aimYaw + aimErrYaw; P.pitch = aimPitch + aimErrPitch; botHeld[A_FIRE] = 1;
    }
    {   /* --- Ventouse : lancer (anticipation grossiere de 0,4 s, visee relevee pour la gravite), detonation, contre-jeu --- */
        if (P.weapon == WP_MINE) {
            botHeld[A_FIRE] = 0;
            if (best >= 0 && see && seeT > botReactT[botLevel] && bd > 6 && bd < 20 && P.fireCd <= 0 && P.switchT <= 0 &&
                wp_has_ammo(WP_MINE) && mine_count() < MINE_MAX_ACTIVE) {
                V3 eye = eye_pos(), c = vadd(np_center(best), vmul(np_vel(best), 0.4f)), d = vsub(c, eye);
                float l = vlen(d), sp = fmaxf(MINE_SPEED + vdot(P.vel, vmul(d, 1.0f / fmaxf(l, 0.01f))), 8), tt = l / sp;
                V3 lead = vadd(d, v3(0, 0.5f * MINE_GRAVITY * tt * tt, 0));
                P.yaw = atan2f(lead.x, -lead.z) + aimErrYaw;
                P.pitch = asinf(clampf(lead.y / fmaxf(vlen(lead), 0.01f), -1, 1)) + aimErrPitch;
                botHeld[A_FIRE] = 1;
                netlog("ventouse : mine lancee vers %s (%.1f m)", pname(best), bd);
            }
        }
        /* detonation : une mine armee sur un joueur, ou un adversaire a moins de 2,5 m d'une mine posee (moi a plus
         * de 2 m), apres 0,15 s de reaction. Seule la plus ancienne est concernee : DETONER UNE garde l'autre en piege */
        static double nearSince;
        Proj *old = mine_oldest();
        int one = 0, all = 0;
        for (int i = 0; i < MAXPR; i++) {
            Proj *p = &PR[i];
            if (!mine_own(p) || p->mineDet || p->mineState < MS_WORLD) continue;
            int hot = p->mineState == MS_PLAYER && p->mineArmT <= 0;
            for (int j = 0; j < MAXP && !hot; j++) hot = np_target(j) && vlen(vsub(np_center(j), p->pos)) < 2.5f;
            if (!hot || vlen(vsub(pcenter(), p->pos)) < 2) continue;
            if (p == old) one = 1; else all = 1;
        }
        if (!one && !all) nearSince = 0;
        else if (nearSince <= 0) nearSince = nowT;
        else if (nowT - nearSince >= 0.15) {
            botPress[all ? A_DETONATE_ALL : A_DETONATE_ONE] = 1;
            nearSince = 0;
            netlog("ventouse : detonation (%s)", all ? "toutes" : "la plus ancienne");
        }
        /* contre-jeu : une fois sur deux, rail dans une mine adverse posee a moins de 4 m de son poseur */
        if (P.weapon == WP_RAIL && P.ammo[WP_RAIL] >= 1 && P.fireCd <= 0 && P.switchT <= 0) {
            for (int i = 0; i < MAXPR; i++) {
                Proj *p = &PR[i];
                if (!mine_shootable(p) || p->owner == myId || p->owner >= MAXP || p->netId % 2 || p->mineState < MS_WORLD) continue;
                if (!np_target(p->owner) || vlen(vsub(np_center(p->owner), p->pos)) > 4) continue;
                V3 eye = eye_pos(), d = vsub(p->pos, eye);
                float l = vlen(d);
                if (l > 45 || !los(eye, p->pos)) continue;
                P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(clampf(d.y / fmaxf(l, 0.01f), -1, 1));
                botHeld[A_FIRE] = 1;
                netlog("ventouse : tir dans la mine de %s", pname(p->owner));
                break;
            }
        }
    }   /* --- fin Ventouse --- */
    if (P.weapon == WP_SAW && best >= 0 && see && seeT > botReactT[botLevel] && bd < 30 && P.sawCd <= 0 && P.fireCd <= 0 &&
        wp_has_ammo(WP_SAW)) {                            /* Scie : on vise devant la cible, et un peu au-dessus (gravite) */
        V3 eye = eye_pos(), c = np_center(best), d = vsub(c, eye);
        float l = vlen(d), sp = fmaxf(SAW_SPEED + vdot(P.vel, vmul(d, 1.0f / fmaxf(l, 0.01f))), SAW_MIN_SPEED), tt = l / sp;
        V3 lead = vsub(vadd(vadd(c, vmul(np_vel(best), tt)), v3(0, 0.5f * SAW_GRAVITY * tt * tt, 0)), eye);
        P.yaw = atan2f(lead.x, -lead.z) + aimErrYaw;
        P.pitch = asinf(clampf(lead.y / fmaxf(vlen(lead), 0.01f), -1, 1)) + aimErrPitch;
        botHeld[A_FIRE] = 1;
    }

    if (best >= 0 && bd > (dry ? 4 : 14) && P.dashCharge >= 1 && botT > botNextDash) {    /* dash vers la cible */
        memset(botHeld, 0, sizeof botHeld);
        botHeld[A_FWD] = botHeld[A_JUMP] = 1;
        P.yaw = aimYaw;
        botPress[A_DASH] = 1;
        botNextDash = botT + 1.2f;
    }
    /* Tesla : clic tenu en continu tant que la cible est a portee ; l'imprecision sinusoidale simule un suivi
     * imparfait. Une piece lancee vers la cible toutes les 2,5 s devient un relais quand l'arc la traverse. */
    if (P.weapon == WP_TESLA && best >= 0 && see && seeT > botReactT[botLevel] && P.switchT <= 0 && wp_has_ammo(WP_TESLA) &&
        bd < tesla_range(P.vel) - 1) {
        static float teslaCoinT;
        P.yaw = aimYaw + aimErrYaw; P.pitch = aimPitch + aimErrPitch;
        botHeld[A_FIRE] = 1;
        if (P.skill == SK_COIN && bd < 12 && P.skillCh >= 1 && botT > teslaCoinT) { botPress[A_SKILL] = 1; teslaCoinT = botT + 2.5f; netlog("tesla : piece relais lancee"); }
    }

    /* Elan : tire quand sa vitesse suffit ; sinon dash-tir, fenetre reussie une fois sur deux
     * (sinon il tire 0,15 a 0,3 s apres le dash, comme un humain en retard). Le bhop et le grappin relancent. */
    static float kinLateT = -1;                           /* tir en retard programme (fenetre ratee) */
    if (P.weapon != WP_KINETIC || dry) kinLateT = -1;
    else if (see && seeT > botReactT[botLevel] && P.switchT <= 0 && bd < 55) {
        float v = kin_vdep(), need = bd > 20 ? 14.0f : 10.5f;   /* de loin : tir plein ; de pres : 2 touches */
        if (kinLateT >= 0 && botT >= kinLateT && P.fireCd <= 0) {
            P.yaw = aimYaw + aimErrYaw; P.pitch = aimPitch + aimErrPitch; botHeld[A_FIRE] = 1; kinLateT = -1;
        } else if (P.fireCd <= 0 && v >= need && kinLateT < 0) {
            P.yaw = aimYaw + aimErrYaw; P.pitch = aimPitch + aimErrPitch; botHeld[A_FIRE] = 1;
        } else if (P.fireCd <= 0 && kinLateT < 0 && (botPress[A_DASH] || (P.dashCharge >= 1 && botT > botNextDash))) {
            if (!botPress[A_DASH]) { botPress[A_DASH] = 1; botHeld[A_FWD] = 1; botNextDash = botT + 1.2f; }
            P.yaw = aimYaw + aimErrYaw; P.pitch = aimPitch + aimErrPitch;   /* le dash part vers la cible */
            if (frand() < 0.5f) botHeld[A_FIRE] = 1;      /* dash-tir dans le meme tick : fenetre reussie */
            else kinLateT = botT + frange(0.15f, 0.3f);   /* fenetre ratee : tir en retard */
        }
    }
    if (see && bd > (dry ? 6 : 10) && bd < GRAPPLE_RANGE && P.hook == HOOK_NONE && P.hookCd <= 0 && botT > botNextHook) {
        P.yaw = aimYaw;                                   /* grappin sur la cible pour fondre dessus */
        botPress[A_HOOK] = 1;
        botNextHook = botT + 2.5f;
    }
    /* on garde le grappin tant qu'il tire, et on lache des qu'on revoit la cible (avec l'elan) */
    static float hookHoldT;                               /* ne jamais rester pendu au grappin */
    hookHoldT = (P.hook == HOOK_ATTACHED && !P.hookPlayer) ? hookHoldT + dt : 0;
    botHeld[A_HOOK] = (unsigned char)(botPress[A_HOOK] || P.hook == HOOK_FLYING || P.hookPlayer ||
                                 (P.hook == HOOK_ATTACHED && !see && hookHoldT < 1.5f &&
                                  !(seek >= 0 && P.pos.y > PU[seek].pos.y - 1)));
    if (P.hookPlayer) P.yaw = aimYaw;
    /* Eventail : au sol et rapide pres d'une cible, il glisse pour tirer en slide (dispersion / 2) */
    if (P.weapon == WP_SHOTGUN && best >= 0 && bd < 13 && (P.sliding || (P.grounded && hs > SLIDE_MIN))) {
        botHeld[A_CROUCH] = 1; botHeld[A_JUMP] = 0;
    }
    /* Eventail : shotgun jump vers une recharge en hauteur (une fois par sejour en l'air, jamais les 2 dernieres cartouches) */
    static int sgJumpDone;
    if (P.grounded) sgJumpDone = 0;
    if (seekHigh && !sgJumpDone && !P.grounded && P.hook == HOOK_NONE && P.vel.y < 1 && P.fireCd <= 0 &&
        (la == WP_SHOTGUN || lb == WP_SHOTGUN) && P.ammo[WP_SHOTGUN] >= 3) {
        P.weapon = WP_SHOTGUN; P.switchT = 0; P.pitch = -1.45f;
        botHeld[A_FIRE] = 1; sgJumpDone = 1;
        netlog("shotgun jump vers la cellule %d", seek);
    }
    if (P.meleeKind == MK_LANCE) {                        /* --- Lance --- : joute (le grappin sert de monture), estoc au contact */
        if (best >= 0) {
            V3 dl = vsub(np_center(best), eye_pos());
            V3 dirB = vmul(dl, 1.0f / fmaxf(vlen(dl), 0.01f));
            float vc = vdot(vsub(P.vel, np_vel(best)), dirB);
            int mounted = P.hookPlayer == best + 1;
            int holding = P.lanceState != LS_IDLE && see && bd < 25;
            int start = P.lanceState == LS_IDLE && P.meleeCd <= 0 && see && bd < 20 && bd > LANCE_THRUST_RANGE + 1 &&
                        (vc > 6 || mounted);
            if (holding || start) {                       /* lance couchee : on vise la cible, on fonce */
                float err = 0.03f * sinf(botT * 3.1f + botMode);
                P.yaw = aimYaw + err; P.pitch = aimPitch;
                botHeld[A_LEFT] = botHeld[A_RIGHT] = 0; botHeld[A_FWD] = 1;
                botHeld[A_MELEE] = 1;
                if (start) botPress[A_MELEE] = 1;
                if (P.lanceState == LS_COUCH && bd < 12 && vc < 12 && P.dashCharge >= 1 && P.hook == HOOK_NONE) {
                    botPress[A_DASH] = 1;                 /* dash droit sur la cible pour armer la lance */
                    botNextDash = botT + 1.2f;
                }
            } else if (P.lanceState == LS_IDLE && bd < LANCE_THRUST_RANGE + 0.5f && vc < LANCE_VC_MIN && P.meleeCd <= 0) {
                P.yaw = aimYaw; botPress[A_MELEE] = 1;   /* estoc au contact (tap) */
            }
        }
    } else if (P.meleeKind == MK_FLAIL) {                 /* --- Fleau --- : attirer la cible de loin, puis la tirer */
        static int fcPlan;                                /* piece lancee, frappee au fleau 0,3 s plus tard */
        static float fcT, fcNext;
        if (best >= 0 && P.meleeCd <= 0 && bd < flail_range(rel_speed(np_vel(best))) + 0.3f && (see || bd < 2.5f)) {
            P.yaw = aimYaw; P.pitch = aimPitch;           /* au contact aussi : c'est la seule melee (jauges) */
            botPress[A_MELEE] = 1;
        } else if (!fcPlan && !coinPlan && P.skill == SK_COIN && best >= 0 && see && bd > 8 && bd < 25 && P.skillCh >= 1 &&
                   P.meleeCd <= 0 && botT > fcNext) {
            P.yaw = aimYaw; P.pitch = aimPitch;
            botPress[A_SKILL] = 1;
            fcPlan = 1; fcT = 0; fcNext = botT + 3;
        } else if (fcPlan) {
            fcT += dt;
            if (fcT > 0.6f) fcPlan = 0;
            else if (fcT >= 0.3f && P.meleeCd <= 0) {     /* la piece est ~3 m devant : un coup de fleau l'envoie sur la cible */
                if (best >= 0) { P.yaw = aimYaw; P.pitch = aimPitch; }
                botPress[A_MELEE] = 1; fcPlan = 0;
            }
        }                                                 /* (le combo fleau -> tir est plus haut, avant le cross-up) */
        /* --- fin Fleau --- */
    } else if (P.meleeKind == MK_FIST) {                  /* --- Poings --- : au contact, appui a chaque tick (le tampon enchaine) */
        if (best >= 0 && bd < fist_range(rel_speed(np_vel(best))) + 0.3f && P.meleeCd <= FIST_BUFFER && (see || bd < 2.5f)) {
            P.yaw = aimYaw; P.pitch = aimPitch;
            botPress[A_MELEE] = 1;
        }                                                 /* (uppercut -> rail et punch-jump : plus haut, avant le cross-up) */
        /* --- fin Poings --- */
    } else if (best >= 0 && bd < bot_melee_reach(rel_speed(np_vel(best))) + 0.5f && P.meleeCd <= 0) { P.yaw = aimYaw; botPress[A_MELEE] = 1; }
    /* --- fin Lance --- */
    bot_skill(dt, best, bd, see, seeT, aimYaw, aimPitch, aimErrYaw, aimErrPitch);   /* Competences (hors piece, plus haut) */
    static float logT;
    if ((logT += dt) > 1) {
        logT = 0;
        netlog("bot pos (%.1f %.1f %.1f) vit %.1f sol %d crochet %d cible %.1f m vue %d", P.pos.x, P.pos.y, P.pos.z, hs,
               P.grounded, P.hook, bd, see);
    }
}
