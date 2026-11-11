/* VELOCITY - Fleau a chaine
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- Fleau ------------------------------------------------------------- */
/* Fleau a chaine (Yoyo) : coup en arc horizontal de 140 degres, 6 m (+0,05 m par m/s relatif), sur toutes les cibles de
 * l'arc. APPUI COURT : faibles degats et AUCUNE traction, la cible touchee ne bouge pas. APPUI LONG (touche tenue FLAIL_HOLD_T
 * apres l'appui) : le coup part quand meme a l'appui (reactivite, fenetre de parry), puis la chaine s'accroche aux cibles de
 * l'arc a cet instant, sans nouveaux degats, et les TIRE vers le lanceur tant que la touche est tenue (flail_tick : section
 * « Traction » plus bas). La touche est instantanee comme l'epee, la chaine ne traverse pas le decor. Une de mes pieces dans
 * l'arc est frappee vers la cible visible la plus proche. Parry a 6 m, renvoi x0,8. Fleau pare : la chaine s'enroule, la
 * traction s'arrete et le LANCEUR est tire vers le pareur (FLAIL_PARRIED_PULL). */
static int flailSwings, flailHitSwings, flailTargets, flailCoins, flailCoinHits;   /* journal du bot */
static float flailRelSum;
static int flailPulls, flailPullTgt, flailPullPl, flailSnaps;   /* traction : accroches, cibles accrochees (dont joueurs), chaines arrachees */
static float flailPullTime;                               /* traction : secondes tenues au total (journal) */

static float flail_range(float rel) { return FLAIL_RANGE + rel * FLAIL_RANGE_PER_MS; }
static float flail_dmg(float rel) { return FLAIL_DMG + rel * FLAIL_DMG_PER_MS; }
static V3 flail_dir(float yaw, float pitch) { float cp = cosf(pitch); return v3(sinf(yaw) * cp, sinf(pitch), -cosf(yaw) * cp); }

/* c (rayon radius) est-il dans l'arc du coup ? Portee, arc horizontal, tranche verticale autour du pitch (sinon l'arc
 * toucherait un drone 6 m au-dessus de la tete), puis ligne de vue (checkLos) par deux rayons : un seul suffit, pour
 * ne pas rater une cible dont seule la tete depasse d'un muret */
static int flail_in_arc(V3 eye, V3 c, float radius, float rel, int checkLos) {
    V3 d = vsub(c, eye);
    float dist = vlen(d), reach = dist - radius;
    if (reach > flail_range(rel)) return 0;
    if (reach < 0.8f) return 1;                           /* contact : touche toujours, comme l'epee */
    V3 hd = vnorm(v3(d.x, 0, d.z)), hf = v3(sinf(P.yaw), 0, -cosf(P.yaw));
    if (vdot(hd, hf) < FLAIL_ARC_COS) return 0;
    if (fabsf(asinf(clampf(d.y / dist, -1, 1)) - P.pitch) > FLAIL_VSLAB) return 0;
    return !checkLos || los(eye, c) || los(eye, vadd(c, v3(0, 0.6f, 0)));
}

static void flail_mark_hit(V3 p) {                        /* la chaine s'accroche a la premiere cible touchee */
    if (P.flailHitN++ == 0) P.flailHitPos = p;
    burst(p, v3(0.7f, 0.72f, 0.8f), 14, 6, 0.3f, 0.1f);
}

/* cible k du fleau (ennemi k, ou joueur k - MAXE) : position, 0 si elle n'existe pas */
static int flail_target(int k, V3 *c) {
    int pl = k >= MAXE, i = pl ? k - MAXE : k;
    if (!pl) { if (!E[i].alive) return 0; *c = E[i].pos; }
    else { if (!np_target(i)) return 0; *c = np_center(i); }
    return 1;
}

/* le fleau frappe au plus une de mes pieces (la plus proche de l'axe de visee : quatre pieces frappees d'un coup
 * contourneraient le plafond PvP). Elle part a 45 m/s vers la cible la mieux alignee avec le viseur (aide de 9 degres),
 * sinon vers la cible visible la plus proche devant soi. Renvoie 1 si une piece est frappee */
static int flail_strike_coin(V3 eye) {
    V3 f = view_dir(), t;
    int ci = -1, tk = -1;
    float bestAx = -2, best = PARRY_AIM_COS;
    for (int i = 0; i < MAXCOIN; i++) {
        Coin *c = &CO[i];
        if (!c->alive || c->owner != myId || c->flailStruck || c->teslaT > 0 || c->decoy) continue;   /* seulement mes pieces */
        if (!flail_in_arc(eye, c->pos, COIN_RADIUS, rel_speed(c->vel), 1)) continue;
        float ax = vdot(vnorm(vsub(c->pos, eye)), f);
        if (ax > bestAx) { bestAx = ax; ci = i; }
    }
    if (ci < 0) return 0;
    Coin *c = &CO[ci];
    V3 aim = f;
    for (int k = 0; k < MAXE + MAXP; k++) {               /* 1. aide a la visee, comme try_parry */
        if (!flail_target(k, &t)) continue;
        V3 to = vnorm(vsub(t, c->pos));
        float dd = vdot(to, f);
        if (dd > best && los(c->pos, t)) { best = dd; aim = to; tk = k; }
    }
    if (tk < 0) {                                         /* 2. la cible visible la plus proche, devant soi */
        float bd = FLAIL_COIN_SEEK;
        for (int k = 0; k < MAXE + MAXP; k++) {
            if (!flail_target(k, &t)) continue;
            V3 to = vsub(t, c->pos);
            float d = vlen(to);
            if (d < 0.01f || d >= bd) continue;
            to = vmul(to, 1.0f / d);
            if (vdot(to, f) > FLAIL_ARC_COS && los(c->pos, t)) { bd = d; aim = to; tk = k; }
        }
    }
    c->flailStruck = 1; c->vel = vmul(aim, FLAIL_COIN_SPEED); c->life = FLAIL_COIN_LIFE;
    if (dmMode) net_fx_coin(2, c->netId, c->pos, c->vel);
    for (int k = 0; k < 12; k++) add_part(c->pos, vmul(rand_dir(), 5), v3(1, 0.85f, 0.3f), 0.25f, 0.08f, 0);
    impact_frame(c->pos, 0.04f);
    play(S_FLAIL_COIN, 0.8f, 1.0f);
    snprintf(popText, sizeof popText, "PIECE FRAPPEE"); popT = 0.8f;
    flailCoins++;
    if (botMode) netlog("piece frappee -> %s", tk < 0 ? "droit devant" : tk < MAXE ? "drone" : NP[tk - MAXE].name);
    return 1;
}

/* piece frappee (la mienne, seul le lanceur teste les cibles) : touche-t-elle quelqu'un sur ce pas ? On envoie W_COIN,
 * pas W_FLAIL : sinon l'hote appliquerait la regle de parry de melee a une piece venue de 30 m */
static int flail_coin_hit(Coin *c, V3 dir, float len) {
    float bt = len + 1, t;
    int be = -1, bp = -1;
    for (int i = 0; i < MAXE; i++)
        if (E[i].alive && ray_sphere(c->pos, dir, E[i].pos, enemy_radius(E[i].type) + FLAIL_COIN_HIT_R, len, &t) && t < bt) {
            bt = t; be = i; bp = -1;
        }
    for (int i = 0; i < MAXP; i++) {
        V3 mn, mx;
        if (!np_target(i)) continue;
        np_aabb(i, FLAIL_COIN_HIT_R, &mn, &mx);
        if (ray_box(c->pos, dir, mn, mx, len, &t, NULL) && t < bt) { bt = t; bp = i; be = -1; }
    }
    if (be < 0 && bp < 0) return 0;
    V3 at = vadd(c->pos, vmul(dir, bt));
    c->pos = at;
    if (be >= 0) damage_enemy(be, FLAIL_COIN_DMG, vmul(dir, 8));
    else hit_player(bp, FLAIL_COIN_DMG * PVP_SCALE, vmul(dir, 6), W_COIN, dir);   /* Egide : piece frappee, projectile */
    burst(at, v3(1, 0.85f, 0.3f), 22, 8, 0.35f, 0.12f);
    hitMarker = 0.2f;
    impact_frame(at, 0.05f);
    play(S_HIT, 0.6f, 1.3f);
    flailCoinHits++;
    if (botMode && bp >= 0) netlog("piece frappee : touche %s", NP[bp].name);
    return 1;
}

/* tete de la chaine : k = avancement du coup (0..1), depuis la main `hand`. Accrochee (hooked) : droit sur la cible
 * et retour avec elle, chaine tendue. Sinon extension (0 -> 0,4), balayage de +70 a -70 degres (0,1 -> 0,7), retour
 * (0,73 -> 1). *sag = mou de la chaine */
static V3 flail_head(V3 hand, float yaw, float pitch, float k, float range, int hooked, V3 hookAt, float *sag) {
    float ext = clampf(k < 0.4f ? k / 0.4f : k < 0.73f ? 1 : 1 - (k - 0.73f) / 0.27f, 0, 1);
    if (hooked) { *sag = 0; return vlerp(hand, hookAt, ext); }
    float ang = 1.2217f * (1 - 2 * clampf((k - 0.1f) / 0.6f, 0, 1));   /* 70 deg a droite -> 70 deg a gauche */
    *sag = 0.4f * (1 - ext);
    return vadd(hand, vmul(flail_dir(yaw + ang, pitch), range * ext));
}
/* main gauche (comme le cable du grappin) et tete de ma chaine */
static V3 flail_hand(V3 eye) {
    V3 f = view_dir(), r = v3(cosf(P.yaw), 0, sinf(P.yaw)), u = vcross(r, f);
    return vadd(eye, vadd(vmul(r, -0.3f), vadd(vmul(u, -0.28f), vmul(f, 0.6f))));
}
static V3 flail_my_head(V3 eye, float *sag) {
    float k = clampf(1 - P.flailT / FLAIL_ANIM, 0, 1);
    return flail_head(flail_hand(eye), P.yaw, P.pitch, k, flail_range(vlen(P.vel)), P.flailHitN > 0, P.flailHitPos, sag);
}

static void flail_swing(void) {
    V3 eye = eye_pos(), f = view_dir();
    float sp = vlen(P.vel), rel0 = 0;
    int hits = 0, parried, coin, tgt[MAXP], nt = 0;
    float trel[MAXP];
    const char *first = "?";

    melee_start_swing();
    P.flailArm = FLAIL_HOLD_T;                            /* APPUI LONG : touche encore tenue dans FLAIL_HOLD_T, flail_tick accroche les
                                                             cibles. Armee AVANT les touches : chez l'hote un parry revient tout de suite
                                                             (flail_parried la desarme) */
    parried = try_parry(FLAIL_PARRY_RANGE, FLAIL_PARRY_SPEED);
    P.flailHitN = 0;
    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &E[i];
        if (!e->alive) continue;
        float rel = rel_speed(e->vel);
        if (!flail_in_arc(eye, e->pos, enemy_radius(e->type), rel, 1)) continue;
        V3 at = e->pos;
        if (!hits) { rel0 = rel; first = e->type == EN_RUSHER ? "kamikaze" : "drone"; }
        if (e->type == EN_RUSHER) damage_enemy(i, 9999, v3(0, 0, 0));   /* explose au bout de la chaine, sans soin de parry */
        else {                                            /* aucune traction : recul nul, l'ennemi reste ou il est */
            damage_enemy(i, flail_dmg(rel), v3(0, 0, 0));
            if (!e->alive) { if (!vagOn) P.hp = fminf(100, P.hp + 15); ammo_points(PTS_MELEE_KILL, AP_KILL); }   /* VAGUES : pas de soin sur kill */
        }
        flail_mark_hit(at); flailRelSum += rel;
        hits++;
    }
    for (int i = 0; i < MAXP; i++) {                     /* LAN : un seul net_hit par cible et par coup (envoye plus bas) */
        if (!np_target(i)) continue;
        float rel = rel_speed(np_vel(i));
        V3 c = np_center(i);
        if (!flail_in_arc(eye, c, 0.5f, rel, 1)) continue;
        if (shield_faces(i, vnorm(vsub(c, eye)), SHB_DIRECT)) {   /* Egide : bloque, ni dash ni points */
            tgt[nt] = i; trel[nt++] = rel;
            flail_mark_hit(c); parried++;
            continue;
        }
        if (!hits) { rel0 = rel; first = NP[i].name; }
        tgt[nt] = i; trel[nt++] = rel;
        flail_mark_hit(c); flailRelSum += rel;
        hits++;
    }
    coin = flail_strike_coin(eye);                        /* au plus une piece ; ce n'est pas une touche de melee */
    float range = flail_range(sp);
    for (float a = -1.2f; a <= 1.21f; a += 0.15f) {       /* l'arc balaye */
        V3 d = flail_dir(P.yaw + a, P.pitch);
        add_part(vadd(eye, vmul(d, range * 0.85f)), vmul(d, 2), sp > 15 ? v3(1, 0.7f, 0.3f) : v3(0.75f, 0.78f, 0.85f),
                 0.2f, 0.06f + sp * 0.002f, 0);
    }
    if (dmMode) net_fx_melee(eye, f, sp);                 /* EV_MELEE : le recepteur lit meleeKind dans pad */
    P.meleeCd = FLAIL_CD;
    P.meleeT = P.flailT = FLAIL_ANIM;
    P.parryWin = FLAIL_PARRY_WINDOW;
    P.swingLanded = hits || parried || coin;              /* une piece frappee n'est pas un coup dans le vide */
    flailSwings++;
    play(S_FLAIL_SWING, 0.6f, frange(0.95f, 1.05f));
    if (hits) {                                           /* effets locaux d'abord : chez l'hote, un parry revient tout de suite */
        flailHitSwings++; flailTargets += hits;
        P.dashCharge = fminf(2, P.dashCharge + 1);
        melee_hit_points(meleePts[P.meleeKind]);          /* une fois par coup, quel que soit le nombre de cibles */
        hitMarker = 0.18f; shake += 0.1f;
        hitstop = fmaxf(hitstop, 0.03f);                  /* court : la chaine tire, elle ne cogne pas */
        play(S_FLAIL_HIT, 0.8f, frange(0.9f, 1.1f));
        if (botMode) netlog("fleau : %d cible(s), v_rel %.1f, premiere : %s", hits, rel0, first);
    }
    for (int k = 0; k < nt; k++) {                        /* PvP : degats seuls, recul nul (la cible n'est pas tiree) */
        int i = tgt[k];
        hit_player(i, flail_dmg(trel[k]) * PVP_SCALE, v3(0, 0, 0), W_FLAIL, vnorm(vsub(np_center(i), eye)));
    }
}

/* --- Traction (appui long) ---------------------------------------------------------------------------------------------------
 * flail_swing arme P.flailArm ; flail_tick (appele chaque tick par game_tick avec la touche de melee tenue) decide : touche relachee
 * avant FLAIL_HOLD_T = appui court, rien de plus ; sinon flail_hook_start accroche les cibles de l'arc A CET INSTANT (sans degats),
 * puis flail_pull_tick les tire tant que la touche est tenue. Ennemis : leur vitesse est ramenee vers l'oeil (lissee, avant leur IA).
 * Joueurs (LAN) : le lanceur envoie EV_FLAIL toutes les FLAIL_PULL_REFRESH s, la victime se tire elle-meme sur SA vitesse
 * (flail_victim_tick) tant que son bail court ; l'hote arbitre (flail_host, hote.c) et divise la traction par deux face a une Egide. */
static float flailLinkT[MAXP];                            /* chaines vues par tous : bail restant de la victime i... */
static int flailLinkBy[MAXP];                             /* ... et son lanceur + 1 (dessin des chaines) */

/* lache la cible h (le dernier de la liste prend sa place) ; notify : prevenir la victime LAN (EV_FLAIL, FLE_END) */
static void flail_hook_drop(int h, int notify) {
    int k = P.flailHk[h], last = --P.flailPullN;
    if (notify && k >= MAXE && dmMode && np_target(k - MAXE)) net_fx_flail(FLE_END, k - MAXE);
    P.flailHk[h] = P.flailHk[last]; P.flailHkLos[h] = P.flailHkLos[last]; P.flailHkSend[h] = P.flailHkSend[last];
}
/* fin de la traction (derniere cible lachee, ou coupure) : recharge de melee, son, journal */
static void flail_pull_finish(const char *why) {
    P.meleeCd = fmaxf(P.meleeCd, FLAIL_PULL_END_CD);
    if (!P.dead) play(S_HOOK, 0.3f, 1.8f);                /* la chaine se retracte */
    if (botMode) netlog("fleau : traction finie (%s), %.1f s au total", why, flailPullTime);
}
/* coupe tout (relachement, mort, parade...) : chaque victime LAN est prevenue, elle n'attend pas la fin de son bail */
static void flail_pull_end(const char *why) {
    P.flailArm = 0;
    if (P.flailPullN <= 0) return;
    while (P.flailPullN > 0) flail_hook_drop(P.flailPullN - 1, 1);
    flail_pull_finish(why);
}

/* FLAIL_HOLD_T apres l'appui, touche encore tenue : la chaine s'accroche aux cibles de l'arc (memes regles que le coup : portee, arc,
 * tranche verticale, ligne de vue), sans degats ; les FLAIL_HOOK_MAX plus proches. Un boss n'est jamais tire ; un kamikaze explose au
 * bout de la chaine comme au coup. Rien d'accroche : l'appui long dans le vide coute la recharge du coup dans le vide */
static void flail_hook_start(void) {
    V3 eye = eye_pos();
    int cand[MAXE + MAXP], nc = 0, boomed = 0;
    float cdist[MAXE + MAXP];
    for (int k = 0; k < MAXE + MAXP; k++) {
        V3 c;
        if (!flail_target(k, &c)) continue;
        int pl = k >= MAXE;
        if (!pl && EN_BOSS(E[k].type)) continue;
        if (!flail_in_arc(eye, c, pl ? 0.5f : enemy_radius(E[k].type), rel_speed(pl ? np_vel(k - MAXE) : E[k].vel), 1)) continue;
        if (!pl && E[k].type == EN_RUSHER) {              /* kamikaze : explose au bout de la chaine, sans soin de parry */
            damage_enemy(k, 9999, v3(0, 0, 0));
            burst(c, v3(0.7f, 0.72f, 0.8f), 14, 6, 0.3f, 0.1f);
            boomed = 1;
            continue;
        }
        cand[nc] = k; cdist[nc++] = vlen(vsub(c, eye));
    }
    for (int i = 1; i < nc; i++) {                        /* tri par distance (insertion) */
        int kk = cand[i], j = i - 1;
        float dd = cdist[i];
        for (; j >= 0 && cdist[j] > dd; j--) { cand[j + 1] = cand[j]; cdist[j + 1] = cdist[j]; }
        cand[j + 1] = kk; cdist[j + 1] = dd;
    }
    P.flailPullN = 0;
    for (int i = 0; i < nc && P.flailPullN < FLAIL_HOOK_MAX; i++) {
        int k = cand[i], h = P.flailPullN++;
        V3 c;
        flail_target(k, &c);
        P.flailHk[h] = k; P.flailHkLos[h] = 0; P.flailHkSend[h] = FLAIL_PULL_REFRESH;
        burst(c, v3(0.7f, 0.72f, 0.8f), 10, 5, 0.25f, 0.08f);
        flailPullTgt++;
        if (k >= MAXE) { flailPullPl++; net_fx_flail(FLE_START, k - MAXE); }
    }
    if (!P.flailPullN) {
        if (!boomed) P.meleeCd = fmaxf(P.meleeCd, FLAIL_WHIFF_CD - FLAIL_HOLD_T);
        return;
    }
    flailPulls++;
    P.flailT = P.meleeT = 0; P.flailHitN = 0;             /* le balayage s'arrete : la chaine est tendue sur les cibles */
    play(S_FLAIL_HIT, 0.4f, 1.3f);                        /* discret */
    if (botMode) netlog("fleau : traction, %d cible(s) accrochee(s)", P.flailPullN);
}

/* un tick de traction, touche tenue : chaque cible est verifiee (vivante, a portee FLAIL_RANGE + FLAIL_PULL_SLACK, pas coupee par un mur
 * plus de FLAIL_PULL_LOS_T), puis tiree. Ennemi : sa vitesse est ramenee vers l'oeil (consigne FLAIL_PULL_SPEED, qui baisse en
 * approchant de FLAIL_DRONE_STOP, lissee par FLAIL_PULL_RESPONSE) ; appele avant enemy_tick. Joueur : EV_FLAIL toutes les
 * FLAIL_PULL_REFRESH s, la victime fait le reste. Touche relachee : fin. Sans cible : fin */
static void flail_pull_tick(float dt, int held) {
    if (!held) { flail_pull_end("relache"); return; }
    V3 eye = eye_pos();
    flailPullTime += dt;
    for (int h = 0; h < P.flailPullN;) {
        int k = P.flailHk[h];
        V3 c;
        if (!flail_target(k, &c)) { flail_hook_drop(h, 1); continue; }   /* tuee, partie, devenue alliee */
        V3 to = vsub(eye, c);                             /* de la cible vers moi */
        float dist = vlen(to);
        P.flailHkLos[h] = (los(eye, c) || los(eye, vadd(c, v3(0, 0.6f, 0)))) ? 0 : P.flailHkLos[h] + dt;
        if (dist > FLAIL_RANGE + FLAIL_PULL_SLACK || P.flailHkLos[h] > FLAIL_PULL_LOS_T) { flail_hook_drop(h, 1); continue; }
        V3 dir = vmul(to, 1.0f / fmaxf(dist, 0.01f));
        if (k < MAXE) {
            Enemy *e = &E[k];
            float sp = dist > FLAIL_DRONE_STOP ? fminf(FLAIL_PULL_SPEED, FLAIL_PULL_GAIN * (dist - FLAIL_DRONE_STOP)) : 0;
            e->vel = vlerp(e->vel, vmul(dir, sp), fminf(1, dt * FLAIL_PULL_RESPONSE));
        } else {
            if (-vdot(np_vel(k - MAXE), dir) > FLAIL_PULL_SNAP) { flail_hook_drop(h, 1); continue; }   /* il s'enfuit trop vite : arrachee */
            if ((P.flailHkSend[h] -= dt) <= 0) { net_fx_flail(FLE_PULL, k - MAXE); P.flailHkSend[h] += FLAIL_PULL_REFRESH; }
        }
        h++;
    }
    if (P.flailPullN <= 0) { flail_pull_finish("plus de cible"); return; }
    P.meleeCd = fmaxf(P.meleeCd, FLAIL_PULL_END_CD);      /* pas de nouveau coup tant que je tire ; la recharge repart du relachement */
}

/* chaque tick (game_tick, apres le coup) ; held = touche de melee tenue */
static void flail_tick(float dt, int held) {
    int stun = P.teslaStunT > 0 || P.anvilState == ANVIL_STUN;
    if (P.dead || P.meleeKind != MK_FLAIL || stun) { flail_pull_end(P.dead ? "mort" : stun ? "etourdi" : "arme changee"); return; }
    if (P.flailArm > 0) {
        if (!held) P.flailArm = 0;                        /* relachee avant FLAIL_HOLD_T : appui court, rien de plus */
        else if ((P.flailArm -= dt) <= 0) { P.flailArm = 0; flail_hook_start(); }
    }
    if (P.flailPullN > 0) flail_pull_tick(dt, held);
}

/* victime : la chaine est rompue de mon cote (trop loin, mur, arrachee) ; le lanceur est prevenu (FLE_BREAK) et retire sa cible */
static void flail_victim_cut(int snapped) {
    int by = P.flailBy - 1;
    P.flailBy = 0;
    if (myId >= 0 && myId < MAXP) flailLinkT[myId] = 0;
    if (by >= 0 && by < MAXP) net_fx_flail(FLE_BREAK, by);
    if (snapped) {
        flailSnaps++;
        P.fovKick += 4;
        play(S_HOOK, 0.5f, 1.6f);
        snprintf(popText, sizeof popText, "CHAINE ARRACHEE"); popT = 0.8f;
    }
}

/* victime (LAN), chaque tick avant player_tick : tant que le bail court, j'acquiers de la vitesse VERS le lanceur jusqu'a
 * FLAIL_PULL_SPEED (jamais plus : une vitesse deja superieure n'est pas touchee), a FLAIL_PULL_ACCEL ; pres de l'arret
 * (FLAIL_PULL_STOP) je freine ce qui depasse la consigne. Ma propre vitesse fait le contre-jeu : la course au sol ne bat pas la
 * traction, un dash, un bhop rapide ou un grappin (plus de FLAIL_PULL_SNAP en m/s vers l'exterieur) arrache la chaine. Le decor
 * arrete le mouvement comme d'habitude (player_tick). Coupee aussi : bail fini, lanceur mort, trop loin, mur > FLAIL_PULL_LOS_T */
static void flail_victim_tick(float dt) {
    if (!P.flailBy) return;
    int by = P.flailBy - 1;
    P.flailLeaseT -= dt;
    if (P.dead || !dmMode || matchOver || P.flailLeaseT <= 0 || by >= MAXP || !np_target(by)) { P.flailBy = 0; return; }
    V3 pc = pcenter(), c = np_center(by), to = vsub(c, pc);
    float d = vlen(to);
    V3 dir = vmul(to, 1.0f / fmaxf(d, 0.01f));
    if (d > FLAIL_RANGE + FLAIL_PULL_SLACK) { flail_victim_cut(0); return; }
    if (los(pc, c) || los(pc, vadd(c, v3(0, 0.6f, 0)))) P.flailLosT = 0;
    else if ((P.flailLosT += dt) > FLAIL_PULL_LOS_T) { flail_victim_cut(0); return; }
    float along = vdot(P.vel, dir);                       /* vitesse vers le lanceur (negative : je m'en eloigne) */
    if (-along > FLAIL_PULL_SNAP) { flail_victim_cut(1); return; }
    float k = P.flailScale > 0 ? P.flailScale : 1;        /* Egide face a la chaine : l'hote ecrit 1/2 */
    float cap = k * fminf(FLAIL_PULL_SPEED, FLAIL_PULL_GAIN * fmaxf(0, d - FLAIL_PULL_STOP));
    if (along < cap) P.vel = vadd(P.vel, vmul(dir, fminf(FLAIL_PULL_ACCEL * k * dt, cap - along)));
    else if (d < FLAIL_PULL_STOP + 2 && along > cap) P.vel = vsub(P.vel, vmul(dir, fminf(FLAIL_PULL_ACCEL * dt, along - cap)));
}

/* EV_FLAIL recu (apply_event) : from = lanceur et target = victime, sauf FLE_BREAK (from = victime, target = lanceur). Renvoie 1 si
 * c'est une nouvelle accroche sur moi. FLE_START accroche ; FLE_PULL ne fait que prolonger un bail existant du meme lanceur (un
 * rafraichissement en retard apres la fin ne raccroche pas) ; tout le monde garde la chaine en memoire pour la dessiner */
static int flail_net_event(int from, int target, int sub, float scale) {
    int fresh = 0;
    if (from < 0 || from >= MAXP || target < 0 || target >= MAXP || from == myId) return 0;
    if (sub == FLE_BREAK) {                               /* la victime `from` a rompu la chaine du lanceur `target` */
        if (flailLinkBy[from] == target + 1) flailLinkT[from] = 0;
        if (target == myId)
            for (int h = 0; h < P.flailPullN; h++)
                if (P.flailHk[h] == MAXE + from) { flail_hook_drop(h, 0); break; }
        return 0;
    }
    if (sub == FLE_END) {
        if (flailLinkBy[target] == from + 1) flailLinkT[target] = 0;
        if (target == myId && P.flailBy == from + 1) P.flailBy = 0;
        return 0;
    }
    flailLinkBy[target] = from + 1; flailLinkT[target] = FLAIL_PULL_LEASE;
    if (target == myId && !P.dead) {
        if (sub == FLE_START) { fresh = P.flailBy != from + 1; P.flailBy = from + 1; P.flailLosT = 0; }
        if (P.flailBy == from + 1) { P.flailLeaseT = FLAIL_PULL_LEASE; P.flailScale = scale > 0 ? scale : 1; }
        if (fresh) { play(S_HOOK, 0.6f, 0.7f); P.fovKick += 3; }   /* zip de cable */
    } else if (sub == FLE_START && target != myId) play(S_HOOK, 0.4f * atten(np_center(target)), 0.9f);
    return fresh;
}

/* decroissance des baux des chaines vues (joueur et spectateur, appele par fx_tick) */
static void flail_fx_tick(float dt) {
    for (int i = 0; i < MAXP; i++) if (flailLinkT[i] > 0) flailLinkT[i] -= dt;
}
/* le joueur distant i tient-il une chaine tendue sur quelqu'un (bail en cours) ? (rendu : la tete n'est plus pendue a sa main) */
static int flail_pulling(int i) {
    for (int v = 0; v < MAXP; v++) if (flailLinkT[v] > 0 && flailLinkBy[v] == i + 1) return 1;
    return 0;
}
/* --- fin Traction --- */

/* mon fleau a ete pare (EV_MELEE_PARRY, arme W_FLAIL) : la chaine s'enroule, la traction s'arrete et je suis tire vers le pareur (une
 * seule traction par coup, meme si deux victimes parent). C'est la traction du LANCEUR pare, sans rapport avec celle de l'appui long */
static void flail_parried(V3 pd, V3 pa) {
    flail_pull_end("fleau pare");                         /* plus de traction, ni d'accroche en attente */
    if (P.flailParriedT <= 0) {
        V3 h = vnorm(v3(pd.x - pa.x, 0, pd.z - pa.z));
        P.vel = vadd(P.vel, vadd(vmul(h, FLAIL_PARRIED_PULL), v3(0, FLAIL_PARRIED_UP, 0)));
        P.flailParriedT = 0.2f;
        play(S_FLAIL_HIT, 0.8f, 0.6f);
    }
    P.meleeCd = fmaxf(P.meleeCd, MELEE_PARRIED_CD);
}
/* --- fin Fleau --- */
