/* VELOCITY - Fleau a chaine
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- Fleau ------------------------------------------------------------- */
/* Fleau a chaine (Yoyo) : coup en arc horizontal de 140 degres, 6 m (+0,05 m par m/s relatif), sur toutes les cibles de
 * l'arc. Faibles degats et AUCUNE traction : la cible touchee ne bouge pas (le Yoyo ne l'attire plus vers soi, ni en PvP
 * ni sur les drones et ennemis des VAGUES). La touche est instantanee comme l'epee, la chaine ne traverse pas le decor.
 * Une de mes pieces dans l'arc est frappee vers la cible visible la plus proche. Parry a 6 m, renvoi x0,8. Fleau pare :
 * la chaine s'enroule et le LANCEUR est tire vers le pareur (seule traction restante, FLAIL_PARRIED_PULL). */
static int flailSwings, flailHitSwings, flailTargets, flailCoins, flailCoinHits;   /* journal du bot */
static float flailRelSum;

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
            if (!e->alive) { P.hp = fminf(100, P.hp + 15); ammo_points(PTS_MELEE_KILL, AP_KILL); }
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

/* mon fleau a ete pare (EV_MELEE_PARRY, arme W_FLAIL) : la chaine s'enroule, je suis tire vers le pareur (une seule
 * traction par coup, meme si deux victimes parent). C'est la seule traction du Yoyo : celle du lanceur pare */
static void flail_parried(V3 pd, V3 pa) {
    if (P.flailParriedT <= 0) {
        V3 h = vnorm(v3(pd.x - pa.x, 0, pd.z - pa.z));
        P.vel = vadd(P.vel, vadd(vmul(h, FLAIL_PARRIED_PULL), v3(0, FLAIL_PARRIED_UP, 0)));
        P.flailParriedT = 0.2f;
        play(S_FLAIL_HIT, 0.8f, 0.6f);
    }
    P.meleeCd = fmaxf(P.meleeCd, MELEE_PARRIED_CD);
}
/* --- fin Fleau --- */
