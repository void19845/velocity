/* VELOCITY - Poings a impulsion
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- Poings ------------------------------------------------------------ */
/* Poings a impulsion : combo de 3 coups rapides (0,22 s), le 3e est un uppercut, puis 0,7 s de recharge. Une seule
 * cible par coup : la plus proche du cone (~53 degres) ou au contact. Un coup qui ne touche personne mais frappe le
 * decor a moins de 1,9 m donne un punch-jump : la composante normale de la vitesse passe a au moins 11 m/s (sans
 * cumul), sans pousser la vitesse horizontale au-dela de max(avant, 20 m/s). Une charge par sejour en l'air, rendue
 * au sol et au debut d'un wall-run ; recharge partagee avec le wall-jump. Fenetre de parry de 0,06 s par coup ;
 * combo sans touche ni parry : 0,6 s de plus ; poing pare : combo casse, 1 s. Jauge : 25 points par touche, 50 au
 * plus par combo (kill +100 et parry +50 : regles communes). */
static int fistPunches, fistHits, fistUppers, fistCombos, fistFullCombos, fistPJs, fistBreaks, fistUpRails;   /* journal */
static float fistRelSum;

static float fist_range(float rel) { return FIST_RANGE + rel * FIST_RANGE_PER_MS; }
static float fist_dmg(float rel, int up) { return up ? FIST_UP_DMG + rel * FIST_UP_DMG_PER_MS : FIST_DMG + rel * FIST_DMG_PER_MS; }

/* fin normale du combo (delai depasse, ou fenetre du 3e coup finie) : un combo qui n'a rien touche ni pare coute 0,6 s */
static void fist_combo_end(void) {
    if (!P.swingLanded) P.meleeCd = fmaxf(P.meleeCd, 0) + vgm_fist_whiff(FIST_WHIFF_CD);
    if (P.fistStep == vgm_fist_n()) { fistFullCombos++; vgm_fist_full(P.swingLanded); }
    P.fistStep = 0; P.swingLanded = 0; P.fistPts = 0;
}
/* combo casse : poing pare (1 s), choc (0,7 s), gros recul recu (0,3 s) */
static void fist_combo_break(float cd) {
    if (P.fistStep) fistBreaks++;
    P.fistStep = 0; P.swingLanded = 0; P.fistPts = 0; P.fistBuf = 0;
    P.meleeCd = fmaxf(P.meleeCd, cd); P.parryWin = 0;
}

/* anneau du punch-jump dans le plan de la surface (n = normale), poussiere ; local : secousse, FOV, recul de l'arme */
static void fist_pj_fx(V3 at, V3 n, int local) {
    V3 a = vnorm(vcross(n, fabsf(n.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0))), b = vcross(n, a);
    for (int k = 0; k < 16; k++) {
        float an = k * (2 * PI / 16);
        V3 d = vadd(vmul(a, cosf(an)), vmul(b, sinf(an)));
        add_part(vadd(at, vmul(n, 0.05f)), vadd(vmul(d, 7), n), v3(0.45f, 0.9f, 1), 0.3f, 0.08f, 0);
    }
    for (int k = 0; k < 6; k++) add_part(at, vadd(vmul(n, 2), vmul(rand_dir(), 1.5f)), v3(0.7f, 0.7f, 0.75f), 0.5f, 0.12f, 4);
    if (local) { shake += 0.12f; P.fovKick += 5; P.recoil += 0.6f; play(S_FIST_PJ, 0.8f, 1.0f); }
    else play(S_FIST_PJ, 0.7f * atten(at), 1.0f);
}

/* impulsion du punch-jump sur une surface de normale n, avec ses garde-fous (testee seule par run_tests) */
static void fist_pj_push(V3 n) {
    float hs0 = hlen(P.vel), vn = vdot(P.vel, n);
    if (vn < FIST_PJ_PUSH) P.vel = vadd(P.vel, vmul(n, FIST_PJ_PUSH - vn));   /* au moins 11 le long de n, sans cumul */
    float hs = hlen(P.vel), cap = fmaxf(hs0, FIST_PJ_HCAP);
    if (hs > cap) { P.vel.x *= cap / hs; P.vel.z *= cap / hs; }   /* au-dela de 20 m/s : reoriente sans accelerer */
    if (n.y > 0.7f) { P.grounded = 0; P.coyote = 1; }
    P.fistPJ = 0; P.wallJumpCd = FIST_PJ_CD;              /* ni wall-jump ni second punch-jump sur le meme contact */
}

/* coup qui n'a touche aucune cible : le decor a portee donne un punch-jump. Renvoie 1 s'il a lieu (*dist = distance) */
static int fist_pj(V3 eye, V3 f, float *dist) {
    V3 n;
    if (!P.fistPJ || P.wallJumpCd > 0) return 0;
    float t = ray_world(eye, f, fmaxf(FIST_PJ_RANGE, FIST_PJ_FLOOR_RANGE), &n);
    int floor = n.y > 0.7f;
    if (t >= (floor ? FIST_PJ_FLOOR_RANGE : FIST_PJ_RANGE)) return 0;
    if (floor && (P.grounded || P.fistAirT < FIST_PJ_FLOOR_DELAY)) return 0;   /* pas de super-saut depuis le sol */
    fist_pj_push(n);
    *dist = t;
    fist_pj_fx(vadd(eye, vmul(f, t)), n, 1);
    fistPJs++;
    if (botMode) netlog("punch-jump : %s, vitesse %.1f (vy %.1f)", floor ? "sol" : n.y < -0.7f ? "plafond" : "mur", vlen(P.vel), P.vel.y);
    return 1;
}

/* un coup du combo (P.fistStep = coups deja donnes : 0, 1 ou 2) */
static void fist_punch(void) {
    int step = P.fistStep, nmax = vgm_fist_n(), up = vgm_fist_up(step, nmax), parried, bestE = -1, bestP = -1, hit = 0, tgtAir = 0, pj = 0;
    float sp = vlen(P.vel), bestR = 1e9f, rel = 0, pjDist = 0;
    V3 eye = eye_pos(), f = view_dir(), fh = vnorm(v3(f.x, 0, f.z)), at = eye;
    V3 r = v3(cosf(P.yaw), 0, sinf(P.yaw));

    if (step == 0) { P.swingLanded = 0; P.fistPts = 0; fistCombos++; }
    melee_start_swing();                                  /* parry : +50 une fois par coup */
    parried = try_parry(MELEE_RANGE + 1.5f, 1);
    if (vagOn) parried += roq_fist(eye, f);               /* VAGUES : ACROBATIE EXPLOSIVE */
    for (int i = 0; i < MAXE; i++) {                      /* une seule cible : la plus proche, ennemi ou joueur */
        Enemy *e = &E[i];
        if (!e->alive) continue;
        float rl = rel_speed(e->vel);
        V3 d = vsub(e->pos, eye);
        float dist = vlen(d), reach = dist - enemy_radius(e->type);
        if (reach > fist_range(rl)) continue;
        if (reach > 0.8f && vdot(vmul(d, 1.0f / dist), f) < FIST_CONE) continue;
        if (reach < bestR) { bestR = reach; bestE = i; rel = rl; }
    }
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i)) continue;
        float rl = rel_speed(np_vel(i));
        V3 d = vsub(np_center(i), eye);
        float dist = vlen(d), reach = dist - 0.5f;
        if (reach > fist_range(rl)) continue;
        if (reach > 0.8f && vdot(vmul(d, 1.0f / dist), f) < FIST_CONE) continue;
        if (reach < bestR) { bestR = reach; bestP = i; bestE = -1; rel = rl; }
    }
    V3 knock = up ? vadd(vmul(fh, 3), v3(0, FIST_UP_KNOCK, 0)) : vadd(vmul(fh, FIST_KNOCK), v3(0, 1, 0));
    if (bestE >= 0) {
        Enemy *e = &E[bestE];
        at = e->pos; tgtAir = e->type == EN_DRONE;
        if (e->type == EN_RUSHER) {                       /* kamikaze : explose dans sa face, comme avec l'epee */
            damage_enemy(bestE, 9999, vmul(f, 10));
            parry_feedback(at);
            melee_parry_points(PTS_PARRY);
            parried++;
        } else {
            damage_enemy(bestE, fist_dmg(rel, up), knock);
            if (!e->alive) { if (!vagOn) P.hp = fminf(100, P.hp + 15); ammo_points(PTS_MELEE_KILL, AP_KILL); }   /* kill en melee (solo) ; VAGUES : pas de soin sur kill */
        }
        hit = 1;
    } else if (bestP >= 0) {
        at = np_center(bestP);
        tgtAir = fabsf(NP[bestP].st.vel[1]) > 0.5f;       /* pas de drapeau "au sol" : on lit vel.y */
        hit = 1;
    }
    /* effets locaux d'abord : chez l'hote, un parry revient tout de suite (dans net_hit, a la fin) */
    if (hit) {
        int pts = PTS_FIST_COMBO_MAX - P.fistPts < meleePts[MK_FIST] ? PTS_FIST_COMBO_MAX - P.fistPts : meleePts[MK_FIST];
        P.dashCharge = fminf(2, P.dashCharge + FIST_DASH);
        if (pts > 0) { P.fistPts += pts; ammo_points((float)pts, AP_HIT); }   /* 25 par touche, 50 au plus par combo */
        if (!up && tgtAir && !P.grounded) P.vel.y = fmaxf(P.vel.y, FIST_AIR_HANG);   /* combo possible contre un drone */
        hitMarker = 0.18f; shake += up ? 0.2f : 0.08f;
        if (!parried) hitstop = fmaxf(hitstop, up ? 0.06f + sp * 0.0015f : 0.025f);
        if (up && sp > 15) impact_frame(vadd(eye, vmul(f, 1.5f)), 0.05f);
        burst(at, v3(0.6f, 0.95f, 1), 10, 7, 0.25f, 0.09f);
        if (up)                                           /* uppercut : colonne de particules vers le haut */
            for (int k = 0; k < 12; k++)
                add_part(vadd(at, v3(frange(-0.3f, 0.3f), -0.6f + k * 0.12f, frange(-0.3f, 0.3f))), v3(0, 7 + k * 0.5f, 0),
                         v3(0.7f, 0.95f, 1), 0.35f, 0.08f, 0);
        play(up ? S_FIST_UPPER : S_FIST_HIT, 0.9f, frange(0.95f, 1.05f));
        fistHits++; fistRelSum += rel;
        if (up) fistUppers++;
        vgm_fist_hit(up);
        if (up && bestP >= 0) { P.fistUpTgt = bestP + 1; P.fistUpT = FIST_UP_COMBO_T; }   /* bot : la cible vole, le rail suit */
    }
    if (!hit && !parried) pj = fist_pj(eye, f, &pjDist);   /* une cible touchee passe toujours avant le decor */
    P.swingLanded |= hit || parried || pj;                /* le combo entier : un punch-jump compte comme utile */
    P.fistStep = step + 1; P.fistAnim = step % 3 + 1; P.fistT = 0;
    P.meleeCd = step == nmax - 1 ? vgm_fist_end(FIST_END_CD) : vgm_fist_gap(FIST_GAP);
    vgm_fist_swing(step);
    P.meleeT = FIST_ANIM; P.parryWin = FIST_PARRY_WIN;
    fistPunches++;
    play(S_DASH, 0.3f, 2.2f);                             /* souffle du coup */
    {                                                     /* petite trainee devant le poing */
        V3 hand = vadd(eye, vadd(vmul(r, step == 1 ? 0.15f : -0.2f), v3(0, up ? -0.35f : -0.2f, 0)));
        for (int k = 0; k < 6; k++)
            add_part(vadd(hand, vmul(f, 0.4f + k * 0.18f)), vmul(f, 3), sp > 15 ? v3(1, 0.85f, 0.6f) : v3(0.55f, 0.9f, 1),
                     0.12f, 0.05f + sp * 0.001f, 0);
    }
    if (dmMode) net_fx_fist(eye, f, pj ? pjDist : sp, (step + 1) | (pj ? 0x10 : 0));
    if (botMode && hit) netlog("poings : coup %d sur %s, v_rel %.1f, %.0f degats", step + 1,
                               bestP >= 0 ? NP[bestP].name : "drone", rel, fist_dmg(rel, up) * (bestP >= 0 ? PVP_SCALE : 1));
    if (bestP >= 0)                                       /* une seule touche par coup ; l'Egide peut la bloquer de face */
        hit_player(bestP, fist_dmg(rel, up) * PVP_SCALE, knock, W_FIST, vnorm(vsub(np_center(bestP), eye)));
}

/* game_tick : appui memorise 0,12 s, fin du combo (delai ou fenetre du 3e coup finie), parry, garde du viewmodel */
static void fist_tick(float dt, int press) {
    P.fistBuf -= dt; P.fistT += dt; P.fistUpT -= dt;
    vgm_melee_tick(dt);
    if (vgm_fist_free() && (P.fistStep == 0 || P.fistStep == vgm_fist_n())) P.meleeCd = 0;
    if (press) P.fistBuf = FIST_BUFFER;
    if (P.fistStep >= 1 && P.fistStep < vgm_fist_n() && P.fistT > FIST_COMBO_WIN) fist_combo_end();
    if (!P.dead && P.fistBuf > 0 && P.meleeCd <= 0) { P.fistBuf = 0; fist_punch(); }
    if (P.parryWin > 0) {
        if (!P.dead) try_parry(MELEE_RANGE + 1.5f, 1);   /* met swingLanded a 1 s'il pare */
        P.parryWin -= dt;
    }
    if (P.fistStep == vgm_fist_n() && P.parryWin <= 0) fist_combo_end();
    if (P.meleeT > 0 || P.fistStep > 0) P.fistGuard = fminf(1, P.fistGuard + dt / 0.05f);   /* arme a distance abaissee */
    else P.fistGuard = fmaxf(0, P.fistGuard - dt / 0.15f);
}
/* --- fin Poings --- */
