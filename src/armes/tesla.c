/* VELOCITY - Tesla (arc electrique)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- Tesla (arc electrique) --------------------------------------------- */
/* Faisceau continu tant que le tir est tenu : 10 impulsions/s a 2 de batterie (aucune recharge passive ; la melee
 * et les cellules la remplissent, 1 point = 0,4). Rayon sur la visee avec des cibles grossies de 0,4 m, chaine
 * vers 2 cibles de plus a 6 m (60 %), relais sur les pieces traversees (2 s, 3 au plus, ne s'ajoute pas a l'arc).
 * Les degats sont cumules par cible et envoyes toutes les 0,2 s (un net_hit W_TESLA par cible), plus tot si la
 * cible va mourir ou pare. Ne compte pas dans la chaine de touches. Les autres clients recalculent l'arc (ST_ZAP). */
typedef struct { int kind, idx; V3 at; } TeslaTgt;
/* lien d'arc dessine : extremites resolues a chaque image (ap / bp : positions de secours) */
typedef struct { int ak, ai, bk, bi; V3 ap, bp; float life, max; unsigned int seed; int gold; } TeslaLink;
#define MAXTL 48
static TeslaLink TL[MAXTL];
static int tlNext;
static unsigned int tlSeed;
static float teslaArcE[MAXE], teslaRelE[MAXE], teslaArcP[MAXP], teslaRelP[MAXP];   /* cumuls de la fenetre */
static V3 teslaKnE[MAXE];                                 /* recul PvE cumule */
static int teslaVoice[MAXP];                              /* voix en boucle par joueur (index + 1, 0 = aucune) */
static int teslaPulses, teslaPulseHits, teslaSpent, teslaRelays, teslaChains3, teslaReflects, teslaStuns;   /* journal */
static int teslaArcPulses, teslaArcHits;                  /* arc en cours (journal du bot) */
static double teslaArcStartT;
static void dry_fire(void);

static float tesla_range(V3 vel) { return fminf(TESLA_RANGE + TESLA_RANGE_PER_MS * vlen(vel), TESLA_RANGE_MAX); }
/* etourdi : TESLA_STUN + un demi-tick a la reception, decompte avant le test : exactement 48 ticks sans tir */
static int tesla_stunned(void) { return P.teslaStunT > 0; }
static V3 yp_dir(float yaw, float pitch) { float cp = cosf(pitch); return v3(sinf(yaw) * cp, sinf(pitch), -cosf(yaw) * cp); }
static V3 np_eye(int i) {
    return v3(NP[i].disp.x, NP[i].disp.y + ((NP[i].st.flags & NF_CROUCH) ? EYE_CROUCH : EYE_STAND), NP[i].disp.z);
}
/* joueur ciblable par l'arc de `shooter` (moi compris si c'est un autre qui tire) */
static int tesla_player_ok(int i, int shooter) {
    if (i == shooter || !NP[i].active || !NP[i].alive || same_team(i, shooter)) return 0;
    return i != myId || !P.dead;
}
static V3 tesla_pcenter(int i) { return i == myId ? pcenter() : np_center(i); }
static int tesla_has(const TeslaTgt *t, int n, int kind, int idx) {
    for (int k = 0; k < n; k++) if (t[k].kind == kind && t[k].idx == idx) return 1;
    return 0;
}

/* trace de l'arc : cible principale sur la visee (decor qui arrete), pieces traversees (relais, l'arc continue),
 * puis la chaine (la plus proche a 6 m, en vue, deterministe). shooter : id exclu, -1 en solo.
 * Renvoie le nombre de cibles (0 : out[0].at est le point d'arret) */
static int tesla_trace(V3 eye, V3 d, float range, int shooter, TeslaTgt out[3], int *coins, int *nc) {
    float tw = ray_world(eye, d, range, 0), t;
    int n = 0;
    out[0].kind = TK_POINT; out[0].idx = 0; out[0].at = vadd(eye, vmul(d, tw));
    for (int i = 0; i < MAXE; i++)                        /* solo : drones et kamikazes */
        if (E[i].alive && ray_sphere(eye, d, E[i].pos, enemy_radius(E[i].type) + TESLA_AIM_PAD, tw, &t)) {
            tw = t; out[0].kind = TK_ENEMY; out[0].idx = i; out[0].at = E[i].pos; n = 1;
        }
    for (int i = 0; dmMode && i < MAXP; i++) {
        V3 mn, mx, g = v3(TESLA_AIM_PAD, TESLA_AIM_PAD, TESLA_AIM_PAD);
        if (!tesla_player_ok(i, shooter)) continue;
        if (i == myId) { paabb(P.pos, pheight(), &mn, &mx); mn = vsub(mn, g); mx = vadd(mx, g); }
        else np_aabb(i, TESLA_AIM_PAD, &mn, &mx);
        if (ray_box(eye, d, mn, mx, tw, &t, NULL) && t < tw) {
            tw = t; out[0].kind = TK_PLAYER; out[0].idx = i; out[0].at = tesla_pcenter(i); n = 1;
        }
    }
    if (nc) {
        *nc = 0;
        for (int i = 0; i < MAXCOIN; i++)
            if (CO[i].alive && !CO[i].decoy && CO[i].teslaT <= 0 && ray_sphere(eye, d, CO[i].pos, COIN_RADIUS, tw, &t)) coins[(*nc)++] = i;   /* MIRAGE : pas de relais */
    }
    if (n && out[0].kind == TK_PLAYER && shield_faces(out[0].idx, d, SHB_DIRECT)) return n;   /* Egide : pas de chaine depuis un bouclier */
    for (int k = 0; n && k < TESLA_CHAIN_N; k++) {
        V3 from = out[n - 1].at;
        float best = TESLA_CHAIN_R + 1e-3f;
        TeslaTgt nx;
        nx.kind = TK_NONE; nx.idx = 0; nx.at = from;
        for (int i = 0; i < MAXE; i++) {
            if (!E[i].alive || tesla_has(out, n, TK_ENEMY, i)) continue;
            float dd = vlen(vsub(E[i].pos, from));
            if (dd < best && los(from, E[i].pos)) { best = dd; nx.kind = TK_ENEMY; nx.idx = i; nx.at = E[i].pos; }
        }
        for (int i = 0; dmMode && i < MAXP; i++) {
            if (!tesla_player_ok(i, shooter) || tesla_has(out, n, TK_PLAYER, i)) continue;
            V3 c = tesla_pcenter(i);
            float dd = vlen(vsub(c, from));
            if (dd < best && los(from, c)) { best = dd; nx.kind = TK_PLAYER; nx.idx = i; nx.at = c; }
        }
        if (nx.kind == TK_NONE) break;
        out[n++] = nx;
    }
    return n;
}

static void tesla_link(int ak, int ai, int bk, int bi, V3 ap, V3 bp, float life, int gold) {
    TeslaLink *l = &TL[tlNext];
    tlNext = (tlNext + 1) % MAXTL;
    l->ak = ak; l->ai = ai; l->bk = bk; l->bi = bi; l->ap = ap; l->bp = bp;
    l->life = l->max = life; l->seed = (++tlSeed) * 2654435761u; l->gold = gold;
}

/* --- cumuls : pas d'addition arc + relais, au plus 2 impulsions de relais par fenetre --- */
static float tesla_total(float arc, float rel) { return fmaxf(arc, fminf(rel, 2 * TESLA_RELAY_DMG)); }
static void tesla_flush_one(int kind, int idx) {          /* envoie et remet a zero le cumul d'une cible */
    if (kind == TK_ENEMY) {
        float tot = tesla_total(teslaArcE[idx], teslaRelE[idx]);
        V3 kn = teslaKnE[idx];
        teslaArcE[idx] = teslaRelE[idx] = 0; teslaKnE[idx] = v3(0, 0, 0);
        if (tot > 0 && E[idx].alive) damage_enemy(idx, tot, kn);
    } else if (kind == TK_PLAYER && idx >= 0 && idx < MAXP) {
        float tot = tesla_total(teslaArcP[idx], teslaRelP[idx]);
        teslaArcP[idx] = teslaRelP[idx] = 0;
        if (tot > 0 && np_target(idx))                    /* Egide : une tranche de 0,2 s bloquee de face (~10 sur la jauge) */
            hit_player(idx, tot * PVP_SCALE, v3(0, 0, 0), W_TESLA, vnorm(vsub(np_center(idx), eye_pos())));
    }
}
static void tesla_flush(void) {                           /* fin de la fenetre de 0,2 s, arret de l'arc, mort */
    for (int i = 0; i < MAXE; i++) if (teslaArcE[i] > 0 || teslaRelE[i] > 0) tesla_flush_one(TK_ENEMY, i);
    for (int i = 0; i < MAXP; i++) if (teslaArcP[i] > 0 || teslaRelP[i] > 0) tesla_flush_one(TK_PLAYER, i);
    P.teslaAggT = TESLA_AGG;
}
static void tesla_cancel(void) {                          /* decharge renvoyee : les cumuls ne partent pas */
    memset(teslaArcE, 0, sizeof teslaArcE); memset(teslaRelE, 0, sizeof teslaRelE);
    memset(teslaArcP, 0, sizeof teslaArcP); memset(teslaRelP, 0, sizeof teslaRelP);
    memset(teslaKnE, 0, sizeof teslaKnE);
}
/* ajoute une impulsion au cumul ; envoi anticipe si la cible va mourir (PV connus) ou si elle pare */
static void tesla_add(int kind, int idx, float dmg, int relay, V3 dir) {
    if (kind == TK_ENEMY) {
        if (relay) teslaRelE[idx] += dmg; else teslaArcE[idx] += dmg;
        teslaKnE[idx] = vadd(teslaKnE[idx], vmul(dir, TESLA_KNOCK_PVE));
        if (tesla_total(teslaArcE[idx], teslaRelE[idx]) >= E[idx].hp) tesla_flush_one(kind, idx);
    } else if (kind == TK_PLAYER && idx >= 0 && idx < MAXP && idx != myId) {
        if (relay) teslaRelP[idx] += dmg; else teslaArcP[idx] += dmg;
        if (tesla_total(teslaArcP[idx], teslaRelP[idx]) * PVP_SCALE >= NP[idx].hp || (NP[idx].st.flags & NF_PARRY))
            tesla_flush_one(kind, idx);
    }
}
static void tesla_reset(void) {                           /* (re)spawn : ni arc, ni cumuls, ni liens */
    P.teslaOn = P.teslaDry = P.teslaChain3 = 0;
    P.teslaStunT = P.teslaAggT = P.teslaHitT = 0;
    tesla_cancel();
    memset(TL, 0, sizeof TL);
}

/* --- relais sur piece --- */
static int tesla_relays_of(int id) {
    int n = 0;
    for (int i = 0; i < MAXCOIN; i++) n += CO[i].alive && CO[i].teslaT > 0 && CO[i].teslaBy == id;
    return n;
}
static void tesla_relay_start(int ci) {                   /* n'importe quelle piece : une piece adverse devient MON relais */
    if (tesla_relays_of(myId) >= TESLA_RELAY_MAX) return;
    Coin *c = &CO[ci];
    c->teslaT = TESLA_RELAY_T; c->teslaPulse = 0; c->teslaBy = myId; c->vel = v3(0, 0, 0);
    teslaRelays++;
    burst(c->pos, v3(0.5f, 0.75f, 1), 20, 6, 0.3f, 0.1f);
    play(S_TESLA_ZAP, 0.5f, 0.7f);
    if (dmMode) net_fx_tesla(0, c->netId, c->pos, myId, 0);
}
/* une impulsion du relais : toute cible a moins de 6 m en vue, sauf son activateur ; degats seulement si mine */
static void tesla_relay_zap(int ci, int mine) {
    Coin *c = &CO[ci];
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive || vlen(vsub(E[i].pos, c->pos)) >= TESLA_RELAY_R || !los(c->pos, E[i].pos)) continue;
        tesla_link(TK_COIN, ci, TK_ENEMY, i, c->pos, E[i].pos, 0.12f, 0);
        if (mine) tesla_add(TK_ENEMY, i, TESLA_RELAY_DMG, 1, vnorm(vsub(E[i].pos, c->pos)));
    }
    for (int i = 0; dmMode && i < MAXP; i++) {
        if (i == c->teslaBy || !tesla_player_ok(i, -1)) continue;
        V3 pc = tesla_pcenter(i);
        if (vlen(vsub(pc, c->pos)) >= TESLA_RELAY_R || !los(c->pos, pc)) continue;
        tesla_link(TK_COIN, ci, TK_PLAYER, i, c->pos, pc, 0.12f, 0);
        if (mine && np_target(i)) tesla_add(TK_PLAYER, i, TESLA_RELAY_DMG, 1, v3(0, 0, 0));
    }
    add_part(c->pos, vmul(rand_dir(), 3), v3(0.6f, 0.8f, 1), 0.15f, 0.08f, 0);
}

/* --- une impulsion de l'arc (10 par seconde) --- */
static void tesla_pulse(void) {
    V3 eye = eye_pos(), d = view_dir(), m = muzzle();
    TeslaTgt t[3];
    int coins[MAXCOIN], nc = 0;
    int n = tesla_trace(eye, d, tesla_range(P.vel), dmMode ? myId : -1, t, coins, &nc);
    int cost = P.ammo[WP_TESLA] < TESLA_COST ? P.ammo[WP_TESLA] : TESLA_COST;   /* le tir coute, pas la touche */
    P.ammo[WP_TESLA] -= cost; teslaSpent += cost;
    P.fireCd = TESLA_PULSE;
    teslaPulses++; teslaArcPulses++;
    for (int k = 0; k < nc; k++) tesla_relay_start(coins[k]);
    {                                                     /* --- Ventouse : l'arc fait exploser les mines qu'il traverse (x1) --- */
        float reach = vlen(vsub(t[0].at, eye)), to;
        for (int i = 0; i < MAXPR; i++)
            if (mine_shootable(&PR[i]) && ray_sphere(eye, d, PR[i].pos, MINE_HIT_R + TESLA_AIM_PAD, reach, &to)) {
                int foreign = PR[i].owner != myId;
                mine_explode(&PR[i], 0, 1);
                snprintf(popText, sizeof popText, foreign ? "MINE VOLEE !" : "MINE !"); popT = 1.0f;
            }
    }                                                     /* --- fin Ventouse --- */
    if (!dmMode) {                                        /* solo : l'arc grille les orbes des drones qu'il traverse */
        float reach = vlen(vsub(t[0].at, eye)), to;
        for (int i = 0; i < MAXPR; i++)
            if (PR[i].alive && PR[i].kind == PR_ORB && ray_sphere(eye, d, PR[i].pos, 0.5f, reach, &to)) {
                PR[i].alive = 0;
                burst(PR[i].pos, v3(0.8f, 0.4f, 1), 14, 5, 0.3f, 0.1f);
            }
    }
    tesla_link(TK_MUZZLE, 0, n ? t[0].kind : TK_POINT, t[0].idx, m, t[0].at, 0.12f, 0);
    for (int k = 0; k < n; k++) {
        V3 from = k ? t[k - 1].at : eye;
        tesla_add(t[k].kind, t[k].idx, k ? TESLA_DMG * TESLA_CHAIN_MULT : TESLA_DMG, 0, vnorm(vsub(t[k].at, from)));
        if (k) tesla_link(t[k - 1].kind, t[k - 1].idx, t[k].kind, t[k].idx, t[k - 1].at, t[k].at, 0.12f, 0);
        for (int s = 0; s < 3; s++) add_part(t[k].at, vmul(rand_dir(), 5), v3(0.6f, 0.8f, 1), 0.2f, 0.07f, 0);
    }
    if (!n && vlen(vsub(t[0].at, eye)) < tesla_range(P.vel) - 0.05f)   /* l'arc meurt sur le mur */
        add_part(t[0].at, vmul(rand_dir(), 3), v3(0.6f, 0.8f, 1), 0.15f, 0.06f, 0);
    if (n) {
        hitMarker = fmaxf(hitMarker, 0.06f); P.teslaHitT = 0.15f;
        teslaPulseHits++; teslaArcHits++;
        play(S_TESLA_ZAP, 0.3f, frange(0.9f, 1.25f));
    }
    if (n == 3 && !P.teslaChain3) {                       /* une fois par appui */
        snprintf(popText, sizeof popText, "CHAINE x3"); popT = 1.0f;
        impact_frame(t[2].at, 0.04f);
        P.teslaChain3 = 1; teslaChains3++;
    }
    P.recoil = fmaxf(P.recoil, 0.15f); shake += 0.01f; muzzleT = 0.03f;
}

/* --- ALL IN ONE : l'eclair inevitable ----------------------------------------------------------------------------------
 * Instantane (pas de vol, donc rien a esquiver), il s'accroche a la cible la plus proche du viseur dans un cone de ~10 deg, en vue, a
 * TESLA_BOLT_RANGE, puis saute comme l'arc (TESLA_CHAIN_N cibles a TESLA_CHAIN_MULT). Il vide TOUTE la batterie (TESLA_BOLT_MIN au moins)
 * et fait TESLA_BOLT_DMG x batterie / 100 ; en PvP x PVP_SCALE puis le plafond par touche de l'hote. Un parry de la cible le renvoie
 * comme une decharge (tesla_host_reflect). Renvoie 1 s'il est parti ; sinon rien n'est depense (message). */
static int teslaBolts;                                    /* journal : eclairs lances */
static int tesla_bolt(void) {
    V3 eye = eye_pos(), d = view_dir();
    int n = P.ammo[WP_TESLA], bk = TK_NONE, bi = -1;
    float best = TESLA_BOLT_CONE;
    if (n < TESLA_BOLT_MIN) {
        play(S_HIT, 0.5f, 0.45f);
        snprintf(popText, sizeof popText, "ALL IN ONE : BATTERIE < %d", TESLA_BOLT_MIN); popT = 1.0f;
        return 0;
    }
    for (int i = 0; i < MAXE; i++) {                      /* la cible la plus proche du viseur (cosinus le plus grand), en vue */
        if (!E[i].alive) continue;
        V3 to = vsub(E[i].pos, eye);
        float l = vlen(to);
        if (l < 0.5f || l > TESLA_BOLT_RANGE + enemy_radius(E[i].type)) continue;
        float c = vdot(to, d) / l;
        if (c > best && los(eye, E[i].pos)) { best = c; bk = TK_ENEMY; bi = i; }
    }
    for (int i = 0; dmMode && i < MAXP; i++) {
        if (!tesla_player_ok(i, myId)) continue;
        V3 pc = tesla_pcenter(i), to = vsub(pc, eye);
        float l = vlen(to);
        if (l < 0.5f || l > TESLA_BOLT_RANGE) continue;
        float c = vdot(to, d) / l;
        if (c > best && los(eye, pc)) { best = c; bk = TK_PLAYER; bi = i; }
    }
    TeslaTgt t[3];
    int k = 0;
    if (bi >= 0) k = tesla_trace(eye, vnorm(vsub(bk == TK_ENEMY ? E[bi].pos : tesla_pcenter(bi), eye)), TESLA_BOLT_RANGE + 2,
                                 dmMode ? myId : -1, t, NULL, NULL);
    if (k <= 0) {
        play(S_HIT, 0.5f, 0.45f);
        snprintf(popText, sizeof popText, "ALL IN ONE : PAS DE CIBLE"); popT = 1.0f;
        return 0;
    }
    float base = TESLA_BOLT_DMG * (float)n / (float)ammoDef[WP_TESLA].max;
    P.ammo[WP_TESLA] = 0; teslaSpent += n; teslaBolts++;
    tesla_flush();                                        /* les cumuls de l'arc partent avant : pas de melange */
    tesla_link(TK_MUZZLE, 0, t[0].kind, t[0].idx, muzzle(), t[0].at, 0.35f, 0);
    for (int j = 0; j < k; j++) {
        V3 from = j ? t[j - 1].at : eye, dj = vnorm(vsub(t[j].at, from));
        float dmg = base * (j ? TESLA_CHAIN_MULT : 1);
        if (j) tesla_link(t[j - 1].kind, t[j - 1].idx, t[j].kind, t[j].idx, t[j - 1].at, t[j].at, 0.35f, 0);
        burst(t[j].at, v3(0.6f, 0.8f, 1), 30, 8, 0.4f, 0.1f);
        if (t[j].kind == TK_ENEMY) damage_enemy(t[j].idx, dmg, vmul(dj, 6));
        else if (t[j].kind == TK_PLAYER && np_target(t[j].idx))
            hit_player(t[j].idx, dmg * PVP_SCALE, v3(0, 0, 0), W_TESLA, dj);
    }
    if (dmMode) net_fx_bolt(muzzle(), t[0].at);
    P.fireCd = fmaxf(P.fireCd, 0.5f); P.teslaHitT = 0.3f; P.recoil = 1.5f; shake += 0.3f; muzzleT = 0.1f;
    hitMarker = 0.25f; impact_frame(t[0].at, 0.08f); hitstop = fmaxf(hitstop, 0.05f);
    play(S_TESLA_ZAP, 0.9f, 0.6f);
    if (k > 1) snprintf(popText, sizeof popText, "ECLAIR x%d", k);
    else snprintf(popText, sizeof popText, "ECLAIR !");
    popT = 1.0f;
    if (botMode) netlog("tesla : eclair ALL IN ONE, batterie %d -> %.0f degats, %d cible(s)", n, base, k);
    return 1;
}
/* --- fin ALL IN ONE --- */

/* tir maintenu : held = A_FIRE tenu, Tesla en main, vivant, pas en train de changer d'arme */
static void tesla_tick(float dt, int held) {
    int was = P.teslaOn;
    P.teslaOn = 0;
    if (P.teslaStunT > 0) P.teslaStunT -= dt;
    P.teslaHitT -= dt;
    if ((P.teslaAggT -= dt) <= 0.5f * TICK) tesla_flush();   /* fenetre de 0,2 s (relais compris, arc coupe ou non) */
    if (!held) { P.teslaDry = 0; P.teslaChain3 = 0; }
    else if (!tesla_stunned()) {
        if (P.ammo[WP_TESLA] <= 0) {                      /* batterie vide : clic sec et bascule, une fois par appui */
            if (!P.teslaDry && P.fireCd <= 0.5f * TICK) { P.teslaDry = 1; dry_fire(); }
        } else {
            P.teslaOn = 1;
            if (!was) { P.teslaAggT = TESLA_AGG; teslaArcPulses = teslaArcHits = 0; teslaArcStartT = nowT; }
            if (P.fireCd <= 0.5f * TICK) tesla_pulse();   /* premiere impulsion au tick de l'appui */
        }
    }
    if (was && !P.teslaOn) {                              /* arret, changement d'arme, mort : les cumuls partent */
        tesla_flush();
        if (botMode && teslaArcPulses)
            netlog("tesla : arc de %.1f s, %d impulsion(s) dont %d qui touchent, batterie %d", nowT - teslaArcStartT,
                   teslaArcPulses, teslaArcHits, P.ammo[WP_TESLA]);
    }
    if (tesla_stunned() && ((int)(P.teslaStunT * 120)) % 5 == 0)   /* surcharge : fumee */
        add_part(muzzle(), v3(0, 1.2f, 0), v3(0.35f, 0.35f, 0.38f), 0.5f, 0.1f, -1);
    if (P.dead)                                           /* mort : mes relais s'eteignent (coin_tick les detruit) */
        for (int i = 0; i < MAXCOIN; i++)
            if (CO[i].alive && CO[i].teslaT > 0.5f * TICK && CO[i].teslaBy == myId) CO[i].teslaT = 0.001f;
}

/* effets (joueur et spectateur) : duree des liens, arcs des autres recalcules ici toutes les 0,1 s */
static void tesla_fx_tick(float dt) {
    for (int i = 0; i < MAXTL; i++) TL[i].life -= dt;
    for (int i = 0; dmMode && i < MAXP; i++) {
        NetPlayer *n = &NP[i];
        if (i == myId || !n->active || !n->alive || !(n->st.pad & ST_ZAP) || n->st.weapon != WP_TESLA) {
            n->teslaVisT = 0; n->teslaVisHit = 0;
            continue;
        }
        if ((n->teslaVisT -= dt) > 0.5f * TICK) continue;
        n->teslaVisT = TESLA_PULSE;
        TeslaTgt t[3];
        V3 eye = np_eye(i), d = yp_dir(n->st.yaw, n->st.pitch);
        float range = tesla_range(np_vel(i));
        int k = tesla_trace(eye, d, range, i, t, NULL, NULL);
        /* verrou : celui qui perd des PV voit toujours l'arc qui le touche */
        if (myId < MAXP && !P.dead && n->teslaHitMeT > 0 && nowT - n->teslaHitMeT < TESLA_LOCK_T &&
            vlen(vsub(pcenter(), eye)) < range + 1 && !(k && t[0].kind == TK_PLAYER && t[0].idx == myId)) {
            t[0].kind = TK_PLAYER; t[0].idx = myId; t[0].at = pcenter(); k = 1;
        }
        tesla_link(TK_HAND, i, k ? t[0].kind : TK_POINT, t[0].idx, eye, t[0].at, 0.12f, 0);
        for (int j = 1; j < k; j++) tesla_link(t[j - 1].kind, t[j - 1].idx, t[j].kind, t[j].idx, t[j - 1].at, t[j].at, 0.12f, 0);
        for (int j = 0; j < k; j++) add_part(t[j].at, vmul(rand_dir(), 4), v3(0.6f, 0.8f, 1), 0.18f, 0.07f, 0);
        n->teslaVisHit = k > 0;
    }
}

/* boucles sonores : la mienne et celle de chaque joueur distant dont l'arc est actif (appelee a chaque image) */
static void tesla_audio(void) {
    int run = appState == APP_GAME && (dmMode || captured);
    for (int i = 0; i < MAXP; i++) {
        int on, hit;
        float vol;
        if (i == myId) { on = P.teslaOn && !P.dead && !spectating; hit = P.teslaHitT > 0; vol = 0.4f; }
        else {
            on = dmMode && NP[i].active && NP[i].alive && (NP[i].st.pad & ST_ZAP) && NP[i].st.weapon == WP_TESLA;
            hit = NP[i].teslaVisHit; vol = 0.5f * atten(NP[i].disp);
        }
        on = on && run;
        if (on && !teslaVoice[i]) teslaVoice[i] = play_loop(S_TESLA_LOOP, vol, 1);
        else if (!on && teslaVoice[i]) { loop_stop(teslaVoice[i]); teslaVoice[i] = 0; }
        if (on) loop_set(teslaVoice[i], vol, hit ? 1.08f : 1.0f);
    }
}
/* --- fin Tesla ---------------------------------------------------------- */
