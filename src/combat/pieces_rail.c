/* VELOCITY - pieces, ricochets, tir instantane, railgun
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static V3 muzzle(void) {
    V3 f = view_dir(), r = v3(cosf(P.yaw), 0, sinf(P.yaw)), u = vcross(r, f);
    return vadd(eye_pos(), vadd(vmul(f, 0.6f), vadd(vmul(r, 0.22f), vmul(u, -0.2f))));
}

static unsigned int coinCounter;

static int throw_coin_one(V3 f, int pay) {
    for (int i = 0; i < MAXCOIN; i++) {
        Coin *c = &CO[i];
        if (c->alive) continue;
        if (pay) P.skillCh -= 1;
        c->alive = 1; c->owner = myId; c->life = cp_coin_life(); c->spin = 0; c->teslaT = 0; c->flailStruck = 0; c->decoy = 0;
        c->pos = vadd(eye_pos(), vadd(vmul(f, 0.7f), v3(0, -0.2f, 0)));
        c->vel = vadd(P.vel, vadd(vmul(f, 9), v3(0, 6, 0)));   /* la piece garde ta vitesse : elle reste devant toi */
        c->netId = ((unsigned int)(myId + 1) << 24) | (++coinCounter & 0xFFFFFF);
        play(S_HIT, 0.5f, 2.3f);
        if (dmMode) net_fx_coin(0, c->netId, c->pos, c->vel);
        cp_coin_new(i);
        return 1;
    }
    return 0;
}
static void throw_coin(void) {
    V3 dirs[8];
    int n = cp_coin_dirs(view_dir(), dirs);               /* VAGUES : eventail (PLUIE), cercle ; sinon une seule piece */
    for (int k = 0; k < n; k++) if (!throw_coin_one(dirs[k], k == 0) && k == 0) return;
}

static void tesla_relay_zap(int ci, int mine);          /* Tesla : definie avec l'arme */
static void coin_tick(float dt) {
    for (int i = 0; i < MAXCOIN; i++) {
        Coin *c = &CO[i];
        if (!c->alive) continue;
        if (c->pos.y < maps[curMap].killY - 2) { c->alive = 0; continue; }   /* tombee dans le vide */
        if (c->decoy) {                                   /* MIRAGE : immobile, sans gravite, puis se dissipe */
            if ((c->life -= dt) <= 0) { c->alive = 0; burst(c->pos, v3(0.4f, 0.85f, 1), 18, 4, 0.35f, 0.08f); }
            continue;
        }
        if (c->teslaT > 0) {                              /* --- Tesla : relais, la piece flotte et frappe tout autour --- */
            c->teslaT -= dt; c->spin += dt * 3;
            if ((c->teslaPulse -= dt) <= 0.5f * TICK) {
                c->teslaPulse = TESLA_PULSE;
                tesla_relay_zap(i, c->teslaBy == myId);   /* degats si c'est mon relais, sinon visuel seul */
            }
            if (c->teslaT <= 0.5f * TICK) {               /* la piece grille : un relais ne se renouvelle pas */
                c->alive = 0; c->teslaT = 0;
                burst(c->pos, v3(0.6f, 0.8f, 1), 16, 5, 0.3f, 0.08f);
                if (dmMode && c->teslaBy == myId) net_fx_coin(1, c->netId, c->pos, v3(0, 0, 0));
            }
            continue;
        }                                                 /* --- fin Tesla --- */
        if (c->flailStruck) {                             /* --- Fleau : piece frappee, vol droit sans gravite --- */
            c->life -= dt; c->spin += dt * 4;
            V3 step = vmul(c->vel, dt);
            float len = vlen(step);
            V3 dir = vmul(step, 1.0f / fmaxf(len, 1e-6f));
            if (len > 1e-5f && ray_world(c->pos, dir, len, 0) < len) c->life = 0;   /* le decor : local partout */
            else if (c->owner == myId && flail_coin_hit(c, dir, len)) {   /* seul le lanceur teste les cibles */
                c->alive = 0;
                if (dmMode) net_fx_coin(1, c->netId, c->pos, v3(0, 0, 0));
                continue;
            } else c->pos = vadd(c->pos, step);
            add_part(c->pos, v3(0, 0, 0), v3(1, 0.85f, 0.3f), 0.2f, 0.09f, 0);   /* trainee doree */
            if (c->life <= 0) { c->alive = 0; burst(c->pos, v3(1, 0.85f, 0.3f), 8, 4, 0.25f, 0.08f); }
            continue;
        }                                                 /* --- fin Fleau --- */
        if (vagOn && cp_coin_orbit(i, dt)) continue;      /* VAGUES : Piece orbitale */
        c->life -= dt; c->spin += dt;
        c->vel.y -= COIN_GRAVITY * dt;
        V3 step = vmul(c->vel, dt);
        float len = vlen(step);
        V3 wn = v3(0, 1, 0);
        if (len > 1e-5f && ray_world(c->pos, vmul(step, 1.0f / len), len, &wn) < len) { if (!cp_coin_bounce(i, wn)) c->life = 0; }   /* tombee */
        else c->pos = vadd(c->pos, step);
        if (c->life <= 0) {                               /* fin de vie : retombee, expiration */
            c->alive = 0;
            if (vagOn && c->owner == myId) cp_coin_pellets(c->pos);   /* VAGUES : capstone PIE>SHOTGUN, la piece explose en plombs */
            if (frand() < 0.5f) add_part(c->pos, v3(0, 2, 0), v3(1, 0.8f, 0.3f), 0.3f, 0.1f, 6);
        }
    }
}

/* MIRAGE : un tir adverse (rail, Elan, plomb) touche le leurre `ci` : il s'y arrete et le leurre disparait partout */
static int decoyHits;                                     /* journal : tirs absorbes par un leurre adverse */
static void decoy_absorb(int ci, const char *what) {
    Coin *c = &CO[ci];
    if (!c->alive) return;
    c->alive = 0; decoyHits++;
    if (dmMode) net_fx_coin(1, c->netId, c->pos, v3(0, 0, 0));
    burst(c->pos, v3(0.4f, 0.85f, 1), 30, 7, 0.4f, 0.1f);
    play(S_PARRY, 0.5f, 0.7f);
    snprintf(popText, sizeof popText, "LEURRE !"); popT = 1.0f;
    if (botMode) netlog("mirage : %s absorbe par le leurre de %s", what, c->owner < MAXP ? NP[c->owner].name : "?");
}
/* un leurre ne prend pas le tir qui part de l'interieur (MIRAGE pose sur soi : sinon chaque rail tire juste apres
 * serait renvoye vers la cible la plus proche, une visee automatique) */
static int decoy_skip(int ci, V3 eye) {
    return CO[ci].decoy && vlen(vsub(eye, CO[ci].pos)) < COIN_RADIUS + (CO[ci].owner == myId ? 0.5f : 0.0f);
}
/* RICOCHET => MIRAGE : si j'ai RICOCHET, un tir qui touche le leurre adverse `ci` ne s'y arrete pas, il repart vers le joueur qui
 * l'a pose. Renvoie son indice s'il est ciblable, en vue du leurre et a portee (COIN_SEEK_RANGE), sinon -1 (le leurre absorbe) */
static int decoyReflects, decoyLured;                     /* journal : tirs renvoyes au poseur ; ricochets de piece detournes */
static int decoy_reflect_owner(int ci) {
    if (!CO[ci].alive || !CO[ci].decoy || CO[ci].owner == myId || !ric_vs_decoy()) return -1;
    int o = CO[ci].owner;
    if (o < 0 || o >= MAXP || !np_target(o)) return -1;
    V3 pc = np_center(o);
    return vlen(vsub(pc, CO[ci].pos)) <= COIN_SEEK_RANGE && los(CO[ci].pos, pc) ? o : -1;
}
/* le tir (degats dmg, arme w) touche le leurre adverse `ci` : absorbe, ou renvoie au poseur avec RICOCHET. Renvoie 1 si le poseur est touche */
static int decoy_hit(int ci, float dmg, const char *what) {
    int owner = decoy_reflect_owner(ci);
    V3 o = CO[ci].pos;
    decoy_absorb(ci, what);
    if (owner < 0) return 0;
    V3 pc = np_center(owner), dir = vnorm(vsub(pc, o));
    add_beam(o, pc, v3(0.4f, 1, 0.7f), 0.7f);
    if (dmMode) net_fx_rail(o, pc, 0);
    if (!hit_player(owner, dmg * PVP_SCALE * DECOY_MULT, vmul(dir, 6), W_COIN, dir)) return 0;   /* Egide : bloque */
    decoyReflects++;
    snprintf(popText, sizeof popText, "LEURRE RENVOYE !"); popT = 1.0f;
    hitMarker = 0.25f;
    if (botMode) netlog("mirage : %s renvoye au poseur %s (RICOCHET)", what, NP[owner].name);
    return 1;
}

/* cible d'un ricochet de piece depuis o (rail, Elan, roquette). MIRAGE => PIECE : un leurre adverse en vue est vise AVANT le joueur
 * (la piece le suit, pas le joueur ; il absorbe le tir) ; sinon l'ennemi ou le joueur visible le plus proche (COIN_SEEK_RANGE).
 * Renvoie 1 s'il y a une cible : *te / *tp = ennemi / joueur, *dc = leurre (les autres a -1) */
static int coin_seek(V3 o, int *te, int *tp, int *dc) {
    float best = cp_seek_range();
    *te = *tp = *dc = -1;
#if DECOY_COIN_LURE
    for (int i = 0; i < MAXCOIN; i++) {
        if (!CO[i].alive || !CO[i].decoy || CO[i].owner == myId || same_team(CO[i].owner, myId)) continue;   /* leurre ADVERSE seulement */
        float d = vlen(vsub(CO[i].pos, o));
        if (d < best && los(o, CO[i].pos)) { best = d; *dc = i; }
    }
    if (*dc >= 0) return 1;
#endif
    best = cp_seek_range();
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive) continue;
        float d = vlen(vsub(E[i].pos, o));
        if (d < best && los(o, E[i].pos)) { best = d; *te = i; *tp = -1; }
    }
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i)) continue;
        float d = vlen(vsub(np_center(i), o));
        if (d < best && los(o, np_center(i))) { best = d; *tp = i; *te = -1; }
    }
    return *te >= 0 || *tp >= 0;
}

/* PIECE => RICOCHET (lecture par defaut, constante COIN_VS_RIC) : mon tir en rebond (fenetre RICOCHET ouverte) touche la piece de
 * l'adversaire ; au lieu de la voler (contre-jeu habituel), la piece le renvoie sur moi, qui l'ai tire. Le tir me touche comme un
 * rebond qui revient (moitie des degats en solo, x PVP_SCALE plafonne en LAN) si la piece me voit, sinon il s'eteint. Renvoie 0. */
static int coinBacks;                                     /* journal : tirs et roquettes renvoyes a leur tireur par une piece adverse */
static int coin_turns_back(int ci, float dmg) {
    Coin *c = &CO[ci];
    V3 o = c->pos, at = pcenter();
    c->alive = 0; coinBacks++;
    if (dmMode) net_fx_coin(1, c->netId, o, v3(0, 0, 0));
    for (int k = 0; k < 20; k++) add_part(o, vmul(rand_dir(), 6), v3(1, 0.85f, 0.3f), 0.35f, 0.1f, 0);
    play(S_PARRY, 0.55f, 1.2f);
    hitstop = fmaxf(hitstop, 0.03f);
    snprintf(popText, sizeof popText, "PIECE ADVERSE : RENVOYE !"); popT = 1.0f;
    if (P.dead || !los(o, at)) return 0;                  /* la piece ne me voit pas : le tir s'eteint */
    add_beam(o, at, v3(1, 0.85f, 0.3f), 0.6f);
    if (dmMode) net_fx_rail(o, at, 1);
    damage_player(dmMode ? fminf(dmg * PVP_SCALE, PVP_HIT_CAP) : dmg * 0.5f);   /* soi : pas de plafond de l'hote, on le pose ici */
    if (botMode) netlog("piece contre ricochet : le tir revient sur son tireur (piece de %s)", c->owner < MAXP ? NP[c->owner].name : "?");
    return 0;
}

/* Le rail touche la piece `ci` : il repart vers une autre piece (chaine) ou vers la cible visible la plus proche,
 * avec des degats multiplies. Renvoie 1 si une cible a ete touchee. */
static int ricochet(int ci, float dmg, int depth) {
    Coin *c = &CO[ci];
    V3 o = c->pos;
    int stolen = c->owner != myId;                        /* contre-jeu : la piece d'un adversaire se retourne contre lui */
    if (c->decoy && stolen) return decoy_hit(ci, dmg, "rail");   /* MIRAGE adverse : le leurre absorbe le tir (RICOCHET : il le renvoie) */
    if (stolen && !same_team(c->owner, myId) && coin_vs_ric()) return coin_turns_back(ci, dmg);   /* PIECE => RICOCHET : la piece adverse renvoie mon tir en rebond */
    c->alive = 0;
    if (dmMode) net_fx_coin(1, c->netId, o, v3(0, 0, 0));
    dmg *= c->decoy ? DECOY_MULT : stolen ? 1.0f : cp_coin_mult(depth);   /* piece volee : renvoie le tir, mais sans bonus */
    if (!depth) cpRicN = 0;
    cp_coin_pop(o);                                       /* VAGUES : piece piegee */
    lastChain = depth + 1;
    for (int k = 0; k < 20; k++) add_part(o, vmul(rand_dir(), 6), v3(1, 0.85f, 0.3f), 0.35f, 0.1f, 0);
    play(S_PARRY, 0.55f, 1.5f + 0.15f * depth);
    hitstop = fmaxf(hitstop, 0.03f);

    int next = -1;                                        /* 1. une autre de mes pieces : la chaine continue */
    float best = 40;
    for (int i = 0; depth < 6 && i < MAXCOIN; i++) {
        if (!CO[i].alive) continue;                       /* la chaine passe par les pieces de tout le monde */
        float d = vlen(vsub(CO[i].pos, o));
        if (d < best && los(o, CO[i].pos)) { best = d; next = i; }
    }
    if (next >= 0) {
        add_beam(o, CO[next].pos, v3(1, 0.85f, 0.3f), 0.6f);
        if (dmMode) net_fx_rail(o, CO[next].pos, 1);
        return ricochet(next, dmg, depth + 1);   /* chaque piece remultiplie */
    }

    {   /* --- Ventouse : une mine visible avec une cible a moins de 5 m passe avant les cibles directes (x1,5) --- */
        int mi = -1;
        float mb = cp_seek_range();
        for (int i = 0; i < MAXPR; i++) {
            float dm = vlen(vsub(PR[i].pos, o));
            if (!mine_shootable(&PR[i]) || dm >= mb || !mine_has_target(&PR[i]) || !los(o, PR[i].pos)) continue;
            mb = dm; mi = i;
        }
        if (mi >= 0) {
            V3 at = PR[mi].pos;
            int foreign = PR[mi].owner != myId;
            add_beam(o, at, v3(1, 0.85f, 0.3f), 0.7f);
            if (dmMode) net_fx_rail(o, at, 1);
            int h = mine_explode(&PR[mi], 1, 1);
            snprintf(popText, sizeof popText, foreign ? "MINE VOLEE !" : "RICOCHET MINE !");
            popT = 1.0f;
            if (botMode) netlog("ricochet sur une mine (%s), %d touche(s)", foreign ? "volee" : "a moi", h);
            return h > 0;
        }
    }                                                     /* --- fin Ventouse --- */

    int te = -1, tp = -1, dc = -1;                        /* 2. la cible visible la plus proche (le leurre adverse avant le joueur) */
    coin_seek(o, &te, &tp, &dc);
    if (dc >= 0) {                                        /* MIRAGE => PIECE : le ricochet suit le leurre, il est absorbe */
        add_beam(o, CO[dc].pos, v3(1, 0.85f, 0.3f), 0.7f);
        if (dmMode) net_fx_rail(o, CO[dc].pos, 1);
        decoyLured++;
        decoy_absorb(dc, "ricochet");
        return 0;
    }
    if (te < 0 && tp < 0 && botMode) {
        for (int i = 0; i < MAXP; i++)
            if (np_target(i))
                netlog("ricochet sans cible : %s a %.1f m, vue %d, piece en (%.1f %.1f %.1f)", NP[i].name,
                       vlen(vsub(np_center(i), o)), los(o, np_center(i)), o.x, o.y, o.z);
    }
    if (te < 0 && tp < 0) {                               /* rien a toucher : le tir part en l'air */
        add_beam(o, vadd(o, v3(frange(-3, 3), 25, frange(-3, 3))), v3(1, 0.85f, 0.3f), 0.4f);
        return 0;
    }
    V3 at = te >= 0 ? E[te].pos : np_center(tp), dir = vnorm(vsub(at, o));
    add_beam(o, at, v3(1, 0.85f, 0.3f), 0.7f);
    if (dmMode) net_fx_rail(o, at, 1);
    burst(at, v3(1, 0.9f, 0.5f), 25, 9, 0.4f, 0.14f);
    if (te >= 0) { vgCoinCtx++; damage_enemy(te, dmg, vmul(dir, 10)); vgCoinCtx--; cp_ric_after(te, o, dmg); }   /* VAGUES : chemin PIECE */
    else if (!hit_player(tp, dmg * PVP_SCALE, vmul(dir, 6), W_COIN, dir)) return 0;   /* Egide : ricochet de face bloque */
    if (stolen) snprintf(popText, sizeof popText, "PIECE VOLEE !");
    else snprintf(popText, sizeof popText, depth ? "RICOCHET x%d" : "RICOCHET", depth + 1);
    popT = 1.0f;
    hitMarker = 0.25f;
    impact_frame(at, 0.06f);
    return 1;
}

/* RICOCHET : journal (tirs reflechis, rebonds, retours sur le tireur) et trace du dernier tir (directions de chaque
 * segment, normale du decor a son extremite) pour les tests */
static int ricStat[3], ricSelfDone, ricSegs;
static V3 ricDir[RIC_CAP_BOUNCES + 2], ricNrm[RIC_CAP_BOUNCES + 2];
static int ricHitE[MAXE], ricHitP[MAXP];                  /* touches de ce tir par cible : au plus RIC_HITS_MAX (un tir qui rebondit recroise ses cibles) */
/* trajet hitscan perforant (rail, Elan) : piece (le tir s'y arrete), ennemis, joueurs LAN ; renvoie 1 si touche.
 * dmg : degats PvE (deja multiplies par la chaine), knockE / knockP : recul PvE / PvP, w : W_* des touches LAN */
static int hitscan_seg(V3 eye, V3 d, float maxLen, int selfHit, float dmg, float knockE, float knockP, int w,
                       V3 *endOut, V3 *nOut, int *stop) {
    V3 n;
    float tw = ray_world(eye, d, maxLen, &n);
    int hit = 0, coinHit = -1, shieldStop = 0, wall = tw < maxLen - 0.01f;
    for (int i = 0; i < MAXCOIN; i++) {                   /* une de mes pieces sur la trajectoire ? le tir s'y arrete */
        float t;
        if (CO[i].alive && !decoy_skip(i, eye) && ray_sphere(eye, d, CO[i].pos, COIN_RADIUS, tw, &t)) { tw = t; coinHit = i; }
    }
    int mineHit = -1;                                     /* --- Ventouse : une mine (meme adverse) arrete le tir --- */
    for (int i = 0; i < MAXPR; i++) {
        float t;
        if (mine_shootable(&PR[i]) && ray_sphere(eye, d, PR[i].pos, MINE_HIT_R, tw, &t)) { tw = t; mineHit = i; coinHit = -1; }
    }
    for (int i = 0; i < MAXP; i++) {                     /* --- Egide : le tir s'arrete sur un bouclier leve de face --- */
        V3 mn, mx; float t;
        if (!np_target(i) || !shield_faces(i, d, SHB_DIRECT)) continue;
        np_aabb(i, 0.05f, &mn, &mx);
        if (ray_box(eye, d, mn, mx, tw, &t, NULL) && t < tw) { tw = t; coinHit = mineHit = -1; shieldStop = 1; }
    }
    if (vagOn && w == W_RAIL) vg_rail_rank(eye, d, tw);   /* VAGUES : ordre de traversee */
    for (int i = 0; i < MAXE; i++) {
        float t;
        if (E[i].alive && ricHitE[i] < RIC_HITS_MAX && ray_sphere(eye, d, E[i].pos, enemy_radius(E[i].type) * vg_rail_wide(w), tw, &t)) {
            V3 at = vadd(eye, vmul(d, t));
            ricHitE[i]++;
            int head = head_mult(w) > 1 && vg_rail_head(i, at, w);   /* point faible : le sommet du drone */
            float de = dmg * (head ? head_mult(w) : 1);
            if (vagOn && w == W_RAIL) de *= vg_rail_mul(i);   /* VAGUES : Calibre lourd */
            int one = chainArmed && chain_direct_w(w) && !EN_BOSS(E[i].type);   /* VAGUES : jamais un boss d'un coup */
            if (one) de = fmaxf(de, E[i].hp);             /* chaine pleine : la touche tue (armure des VAGUES ensuite) */
            burst(at, v3(1, 1, 1), 15, 8, 0.3f, 0.12f);
            if (head) { head_fx(at); shotHeads++; headShotsMine++; }
            damage_enemy_k(i, de, vmul(d, knockE), one ? DN_ONESHOT : head ? DN_HEAD : DN_NORMAL);
            if (vagOn && w == W_RAIL) vg_rail_hit(i, at, de, head);   /* VAGUES : souffles, soin */
            hit = 1;
        }
    }
    for (int i = 0; i < MAXP; i++) {                     /* LAN : le tir traverse aussi les joueurs (un net_hit chacun) */
        V3 mn, mx, ha; float t;
        if (!np_target(i) || ricHitP[i] >= RIC_HITS_MAX) continue;
        np_aabb(i, 0.05f, &mn, &mx);
        if (ray_box(eye, d, mn, mx, tw, &t, NULL)) {
            int head = head_mult(w) > 1 && head_hit_player(i, eye, d, tw, &ha);   /* tete : x1,5 (rail, one-shot) ou x1,35 */
            V3 at = head ? ha : vadd(eye, vmul(d, t));
            ricHitP[i]++;
            burst(at, v3(1, 1, 1), 15, 8, 0.3f, 0.12f);
            if (hit_player_fl(i, dmg * PVP_SCALE * (head ? head_mult(w) : 1), vmul(d, knockP), w, d, hit_flags(w, head), at)) {
                hit = 1;                                  /* Egide : bloque = rate (chaine) */
                if (head) { head_fx(at); shotHeads++; headShotsMine++; }
            }
        }
    }
    if (coinHit >= 0) {
        if (vagOn && w == W_RAIL) rlCoinHit++;
        hit |= ricochet(coinHit, dmg, 0);
        cp_after_ricochet();                              /* VAGUES : dividende */
        if (botMode) netlog("ricochet : chaine de %d, %s", lastChain, hit ? "touche" : "dans le vide");
    }
    if (mineHit >= 0) {                                   /* rail : x1,5 ; Elan : x1 */
        int foreign = PR[mineHit].owner != myId, boosted = w == W_RAIL;
        int h = mine_explode(&PR[mineHit], boosted, 1);
        hit |= h > 0;
        snprintf(popText, sizeof popText, foreign ? "MINE VOLEE !" : boosted ? "MINE x1,5 !" : "MINE !");
        popT = 1.0f;
        if (botMode) netlog("tir dans une mine (%s), %d touche(s)", foreign ? "volee" : "a moi", h);
    }                                                     /* --- fin Ventouse --- */
    if (selfHit && !P.dead && !ricSelfDone) {             /* RICOCHET : a partir du 2e rebond, le tir peut revenir sur moi */
        V3 mn, mx; float t;                               /* (une seule fois par tir : on ne se traverse qu'une fois) */
        paabb(P.pos, pheight(), &mn, &mx);
        if (ray_box(eye, d, mn, mx, tw, &t, NULL)) {
            ricSelfDone = 1; ricStat[2]++;
            damage_player(dmMode ? fminf(dmg * PVP_SCALE, PVP_HIT_CAP) : dmg * 0.5f);
            if (botMode) netlog("ricochet : le rail revient sur le tireur");
        }
    }
    *endOut = vadd(eye, vmul(d, tw)); *nOut = n;
    *stop = coinHit >= 0 || mineHit >= 0 || shieldStop || !wall;
    return hit;
}
/* tir instantane (rail, Elan). RICOCHET ouvert : le tir est reflechi (r = d - 2 (d.n) n) a chaque mur, sans limite de rebonds ni de
 * portee ; il ne s'arrete que sur une cible qui l'arrete (piece, mine, bouclier), en sortant de la carte (un segment de RIC_SEG_LEN
 * sans mur) ou au garde-fou RIC_CAP_BOUNCES. Reseau : un seul evenement pour tous les rebonds (net_fx_ricochet), chaque client
 * recalcule la trace sur le decor. Chaque cible n'est touchee qu'RIC_HITS_MAX fois par tir. */
static int hitscan_shot(float dmg, float knockE, float knockP, int w, V3 *endOut, V3 *nOut) {
    V3 o = eye_pos(), d = view_dir(), o1 = o, d1 = d;
    if (vagOn && w == W_RAIL) { d = vg_rail_bend(o, d); d1 = d; }   /* VAGUES : visee courbe */
    int stop, bounces = ric_take(), nBounce = 0;
    ricSelfDone = 0;
    memset(ricHitE, 0, sizeof ricHitE); memset(ricHitP, 0, sizeof ricHitP);
    int hit = hitscan_seg(o, d, RIC_SEG_LEN, 0, dmg, knockE, knockP, w, endOut, nOut, &stop);
    if (vagOn && w == W_RAIL && !bounces && !stop) hit |= vg_rail_wall(d, dmg, knockE, knockP, w, endOut, nOut);   /* VAGUES : Perce-mur */
    V3 a = *endOut, n = *nOut;
    ricSegs = 1; ricDir[0] = d; ricNrm[0] = n;
    if (bounces) ricStat[0]++;
    for (int b = 1; b <= bounces && !stop; b++) {         /* RICOCHET : reflexion r = d - 2 (d.n) n */
        V3 e2, n2;
        d = vnorm(vsub(d, vmul(n, 2 * vdot(d, n))));
        o = vadd(a, vmul(n, 0.02f));
        if (b == 1) { o1 = o; d1 = d; }
        hit |= hitscan_seg(o, d, RIC_SEG_LEN, b >= 2, dmg, knockE, knockP, w, &e2, &n2, &stop);
        add_beam(o, e2, v3(0.4f, 1, 0.7f), 0.5f);
        for (int k = 0; k < 6; k++) add_part(a, vadd(vmul(n, 3), vmul(rand_dir(), 3)), v3(0.4f, 1, 0.7f), 0.35f, 0.08f, 8);
        a = e2; n = n2; nBounce++;
        if (ricSegs < RIC_CAP_BOUNCES + 2) { ricDir[ricSegs] = d; ricNrm[ricSegs] = n; }
        ricSegs++;
        ricStat[1]++;
    }
    if (nBounce && dmMode) net_fx_ricochet(o1, d1, nBounce);
    return hit;
}

static void fire_rail(void) {
    if (P.ammo[WP_RAIL] > 0) P.ammo[WP_RAIL]--;
    V3 end, n;
    chain_begin();                                        /* chaine de tirs pleine : ce tir peut tuer */
    float rdmg = RAIL_DMG * streak_mult() * (vagOn ? vg_rail_before() : 1.0f);   /* VAGUES : joute, orbital */
    int k0 = kills;
    int hit = hitscan_shot(rdmg, 8, 4, W_RAIL, &end, &n);
    shot_result(hit);
    if (botMode) netlog("tir rail : %s%s", hit ? "touche" : "rate", shotHeads ? " (tete)" : "");
    chain_end(hit);
    if (vagOn) vg_rail_after(hit, kills - k0, rdmg, end);   /* VAGUES : boutique du Railgun */
    V3 m = muzzle();
    float k = clampf(shotStreak / (float)STREAK_MAX, 0, 1);  /* le rayon vire a l'or avec la chaine */
    add_beam(m, end, v3(0.3f + 0.7f * k, 0.85f - 0.05f * k, 1.0f - 0.7f * k), 0.6f);
    if (dmMode) net_fx_rail(m, end, k);
    V3 bd = vsub(end, m); float bl = vlen(bd); bd = vmul(bd, 1.0f / bl);
    V3 a = vnorm(vcross(bd, v3(0, 1, 0.01f))), b = vcross(bd, a);
    for (float s = 0; s < bl && s < 120; s += 0.35f) {       /* spirale facon Quake */
        float ang = s * 2.2f;
        V3 off = vadd(vmul(a, cosf(ang) * 0.12f), vmul(b, sinf(ang) * 0.12f));
        add_part(vadd(vadd(m, vmul(bd, s)), off), vmul(off, 4), v3(0.3f, 0.6f, 1.0f), 0.7f, 0.06f, 0);
    }
    for (int i = 0; i < 18; i++)
        add_part(end, vadd(vmul(n, 4), vmul(rand_dir(), 4)), v3(0.6f, 0.9f, 1.0f), 0.5f, 0.1f, 12);
    P.fireCd = RAIL_CD / streak_rate(); P.recoil = 1; shake += 0.06f;
    muzzleT = 0.07f;
    play(S_RAIL, 0.7f, frange(0.97f, 1.03f));
    if (hit) { hitMarker = 0.18f; play(S_HIT, 0.6f, 1.0f + 0.05f * (shotStreak < STREAK_MAX ? shotStreak : STREAK_MAX)); }
}
