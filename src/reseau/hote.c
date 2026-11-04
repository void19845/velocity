/* VELOCITY - hote : arbitrage (PV, kills, respawns, parry)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- hote : arbitrage ------------------------------------------------ */
static void mode_on_death(int v);                         /* src/reseau/modes.c */
static void mode_tick(float dt);
static void mode_reset(void);
static int mode_time_winner(void);
static float match_time_limit(void);
static int np_spec(int i);
static V3 np_feet(int i);
static void host_end_match(int winner) {
    if (matchOver) return;
    matchOver = 1; matchWinner = winner; matchEndT = 8;
    NetEvent e = ev_new(EV_MATCH);
    e.sub = 1; e.target = (unsigned char)winner;
    net_emit(&e);
    apply_event(&e);
}

static int hostKillFlags;                                 /* HF_* du coup qui tue (host_apply_damage_fl), joints a l'EV_KILL */
static void host_kill(int killer, int victim, int kind) {
    mode_on_death(victim);                                /* drapeau : tombe la ou le porteur meurt */
    NP[victim].alive = 0; NP[victim].hp = 0; NP[victim].deaths++; NP[victim].respawnT = RESPAWN_TIME;
    if (killer != victim && killer < MAXP && NP[killer].active) {
        NP[killer].kills++;
        if (NP[killer].alive) NP[killer].hp = fminf(100, NP[killer].hp + KILL_HEAL);
    } else if (NP[victim].kills > 0) {
        NP[victim].kills--;                               /* suicide */
    }
    NetEvent e = ev_new(EV_KILL);
    e.actor = (unsigned char)killer; e.target = (unsigned char)victim; e.sub = (unsigned char)kind;
    e.pad[0] = (unsigned char)hostKillFlags;              /* tete, one-shot : killfeed, camera de mort */
    net_emit(&e);
    apply_event(&e);
    if (killer == victim || killer >= MAXP) return;
    if (teamSize > 0) { if (team_score(NP[killer].team) >= fragLimit) host_end_match(NP[killer].team); }
    else if (NP[killer].kills >= fragLimit) host_end_match(killer);
}

static int hitCount[W_COUNT], parryCount;                       /* statistiques pour le journal du bot */
static int voidCredited, voidSuicides;                    /* kill zone (hote) : chutes creditees a un attaquant / suicides */
static double lifeFirstHitT[MAXP], ttkSum;                /* temps pour tuer : du premier degat (a pleine vie) au kill */
static int ttkN, oneShots, meleeParries, clashes;
static int burstKills;                                    /* tue depuis la pleine vie en moins de 0,1 s mais en plusieurs touches (ALL IN ONE) */
static int slamParried, slamRailCombos;                   /* Enclume : ondes parees, onde puis rail sur la meme cible */
static int sawHostStat[6], sawHostBounceHits;             /* Scie : EV_SAW par sous-type, touches apres renvoi */
static double slamLastHitT[MAXP];
static int slamLastBy[MAXP];

/* --- Tirs a la tete et chaine pleine : arbitrage -------------------------------------------
 * Tete : le tireur joint le point d'impact (f[4..6]) ; l'hote verifie qu'il est sur la tete de la cible telle qu'il la
 * voit (tolerance de latence). Chaine : l'hote tient pour chacun une chaine miroir, avancee par les touches qu'il arbitre
 * (+2 a la tete validee) et ramenee a la valeur annoncee (NetPState.spare[1]) quand celle-ci baisse (tir rate, chaine
 * consommee) : un plafond de la vraie chaine. Un tir "chaine pleine" n'est valide que si ce miroir est plein ET si l'etat
 * du tireur la montrait pleine il y a moins de CHAIN_SEEN_T. Valide, le coup tue (one-shot) ; sinon touche normale. */
static int hostChain[MAXP], hostChainSaid[MAXP];          /* miroir ; derniere valeur annoncee par le tireur */
static double hostChainSeenT[MAXP];                       /* derniere fois vue pleine dans son etat */
static int headHostHits[W_COUNT], headOneShots, chainOneShots, headRefused, chainRefused;   /* journal */
static int head_host_ok(const NetEvent *ev) {
    int v = ev->target;
    if (head_mult(ev->sub) <= 1 || v >= MAXP) return 0;
    V3 at = fv(ev->f + 4), h = np_head(v), vel = np_vel(v);
    float dh = hlen(vsub(at, h)), tolH = HW + HEAD_HOST_TOL + hlen(vel) * HEAD_HOST_LAG;
    float tolY = HEAD_R + HEAD_HOST_TOL * 0.25f + fabsf(vel.y) * HEAD_HOST_LAG;   /* en hauteur : bien plus strict */
    return dh <= tolH && fabsf(at.y - h.y) <= tolY;
}
static int chain_host_claim(int from, int w) {            /* le tir porte une chaine pleine : valide ? (elle est consommee) */
    if (from < 0 || from >= MAXP || !chain_direct_w(w)) return 0;
    int ok = hostChain[from] >= STREAK_MAX && nowT - hostChainSeenT[from] < CHAIN_SEEN_T;
    hostChain[from] = 0;
    return ok;
}
static void chain_host_said(int i, int said) {            /* chaine annoncee par i (son etat, ou la mienne) */
    if (said < hostChainSaid[i] && said < hostChain[i]) hostChain[i] = said;   /* elle a baisse : tir rate ou consommee */
    hostChainSaid[i] = said;
    if (said >= STREAK_MAX) hostChainSeenT[i] = nowT;
}
/* --- fin Tirs a la tete : arbitrage --- */

/* tolerance de latence de l'hote pour un parry, selon l'arme qui frappe (et la melee de la victime) */
static double parry_grace(int w, int victimMK) {
    /* Poings : la victime ouvre une fenetre toutes les 0,22 s ; a 0,15 s de tolerance, elle serait "en parry" 95 % du
     * combo. Cette regle passe avant celle de l'arme qui attaque */
    if (victimMK == MK_FIST) return FIST_PARRY_GRACE;
    if (w == W_SLAM) return SLAM_PARRY_GRACE;             /* l'onde de l'Enclume : 0,20 s */
    if (w == W_TESLA) return TESLA_PARRY_GRACE;           /* decharge de la Tesla : 0,25 s (fenetre de cumul) */
    return MELEE_PARRY_GRACE;
}

/* --- Lance : arbitrage du choc de joute ----------------------------- */
static double joustT[MAXP][MAXP];                         /* derniere joute resolue entre a et b */
static int lanceHostCouch, lanceHostThrust, lanceHostParried, lanceClashWins, lanceClashDouble;
static int flailHostParried;                              /* Fleau : fleaux pares (arbitre) */
static int fistHostParried;                               /* Poings : poings pares (arbitre) */
static float lanceHostVcSum;

static V3 dir_of(float yaw, float pitch) {                /* meme formule que view_dir */
    float cp = cosf(pitch);
    return v3(sinf(yaw) * cp, sinf(pitch), -cosf(yaw) * cp);
}

/* touche de lance couchee de a sur b : si b avait aussi sa lance couchee vers a, c'est un choc de joute.
 * Le plus rapide (vitesse propre, de plus de 2 m/s) gagne, sinon les deux sont desarconnes.
 * Renvoie 1 si la touche de a est annulee (b gagne, double desarconnement, ou touche reciproque d'une joute jugee). */
static int lance_host_joust(const NetEvent *ev) {
    int a = ev->from, b = ev->target;
    if (a >= MAXP || b >= MAXP) return 0;
    if (nowT - joustT[a][b] < LANCE_JOUST_LOCK) { netlog("joute reciproque ignoree : %s -> %s", pname(a), pname(b)); return 1; }
    V3 ca = np_center(a), cb = np_center(b);
    V3 fb = dir_of(NP[b].st.yaw, NP[b].st.pitch), ba = vnorm(vsub(ca, cb));
    if (nowT - NP[b].lastLanceT >= LANCE_CLASH_GRACE || vdot(fb, ba) < LANCE_CLASH_COS) return 0;
    float sa = ev->f[4], sb = vlen(np_vel(b));
    joustT[a][b] = joustT[b][a] = nowT;
    NetEvent e = ev_new(EV_LANCE);
    vf(e.f + 4, vlerp(ca, cb, 0.5f));                     /* point de choc */
    if (sa > sb + LANCE_CLASH_DIFF) {                     /* a gagne : sa touche s'applique normalement (degats et recul) */
        e.sub = 1; e.actor = (unsigned char)a; e.target = (unsigned char)b;
        lanceClashWins++;
        net_emit(&e);
        apply_event(&e);
        return 0;
    }
    if (sb > sa + LANCE_CLASH_DIFF) {                     /* b gagne : la touche de a est annulee, b le touche a sa place */
        float dmg = fminf(lance_dmg(ev->f[5]) * PVP_SCALE, PVP_HIT_CAP), before = NP[a].hp;
        V3 kn = vmul(np_vel(b), LANCE_GIVE);              /* v_c est symetrique : on reprend celui de a */
        kn.y += 3;
        if (vlen(kn) > LANCE_KNOCK_MAX) kn = vmul(vnorm(kn), LANCE_KNOCK_MAX);
        e.sub = 1; e.actor = (unsigned char)b; e.target = (unsigned char)a; e.f[0] = dmg; vf(e.f + 1, kn);
        lanceClashWins++;
        if (before >= 99.9f) lifeFirstHitT[a] = nowT;
        NP[a].hp -= dmg;
        net_emit(&e);
        apply_event(&e);
        if (NP[a].hp <= 0 && NP[a].alive) {
            ttkSum += nowT - lifeFirstHitT[a]; ttkN++;
            if (before >= 99.9f) oneShots++;
            host_kill(b, a, W_LANCE);
        }
        return 1;
    }
    e.sub = 2; e.actor = (unsigned char)a; e.target = (unsigned char)b;   /* ni l'un ni l'autre */
    clashes++; lanceClashDouble++;
    net_emit(&e);
    apply_event(&e);
    return 1;
}
/* --- fin Lance : arbitrage --- */

/* --- Ventouse : arbitrage. Le premier qui decide du sort d'une mine gagne (1 = explosee, 2 = arrachee) ;
 * les EV_HIT portant son netId et venant d'un autre sont ignores --- */
typedef struct { unsigned int id; int fate, who; } MineFate;
static MineFate hostMine[32];
static int hostMineHead, mineCancels, mineHitsIgnored, mineHostBooms, mineHostTorn;
static MineFate *mine_fate(unsigned int id) {
    for (int k = 0; k < 32; k++) if (id && hostMine[k].id == id) return &hostMine[k];
    return NULL;
}
static void mine_fate_set(unsigned int id, int fate, int who) {
    MineFate *m = &hostMine[hostMineHead];
    hostMineHead = (hostMineHead + 1) % 32;
    m->id = id; m->fate = fate; m->who = who;
}
/* renvoie 1 si l'evenement est annule (ni applique ni relaye) */
static int mine_host(const NetEvent *ev) {
    MineFate *m = mine_fate(ev->netId);
    if (ev->sub == MINE_EV_BOOM) {
        if (m && m->who != ev->from) { netlog("ventouse : explosion de %s ignoree (sort deja decide)", pname(ev->from)); return 1; }
        if (!m) { mine_fate_set(ev->netId, 1, ev->from); mineHostBooms++; }
    } else if (ev->sub == MINE_EV_TORN && ev->from < MAXP) {
        if (m && m->fate == 1) {                          /* deja explosee : l'arrachage est annule, pas de soin */
            NetEvent e = ev_new(EV_MINE);
            e.sub = MINE_EV_CANCEL; e.netId = ev->netId; e.target = ev->from;
            mineCancels++;
            net_emit(&e);
            apply_event(&e);
            return 1;
        }
        if (!m) mine_fate_set(ev->netId, 2, ev->from);
        mineHostTorn++; parryCount++;
        NP[ev->from].hp = fminf(100, NP[ev->from].hp + PARRY_HEAL);
    }
    return 0;
}
/* --- fin Ventouse : arbitrage --- */

static void host_take_pickup(int i, int who) {
    if (i >= npu || PU[i].t > 0 || who >= MAXP || !NP[who].alive) return;   /* premier arrive, premier servi */
    NetEvent e = ev_new(EV_PICKUP_TAKEN);
    e.sub = (unsigned char)i; e.target = (unsigned char)who;
    net_emit(&e);
    apply_event(&e);
}

/* degats arbitres : PV, plafond par touche, temps pour tuer, kill (EV_HIT, et les decisions du bouclier).
 * hf : drapeaux HF_* deja valides par host_handle. Les seules exceptions au plafond PVP_HIT_CAP : la tete (rail, Elan), et
 * le one-shot (rail a la tete, chaine pleine) qui retire au moins tous les PV restants, specialisations comprises */
static void host_apply_damage_fl(int from, int v, float dmgIn, int w, int hf) {
    float before = NP[v].hp;
    if (from != v && from >= 0 && from < MAXP) {          /* specialisations : ASSAUT tape plus fort, REMPART encaisse */
        if (np_spec(from) == SPEC_ASSAUT) dmgIn *= SPEC_ASSAUT_MUL;
        if (np_spec(v) == SPEC_REMPART) dmgIn *= SPEC_REMPART_MUL;
    }
    int self = w == W_SELF || w == W_VOID;
    int oneShot = !self && ((hf & HF_CHAIN) || ((hf & HF_HEAD) && head_oneshot(w)));
    float dmg = self || (hf & HF_HEAD) ? dmgIn : fminf(dmgIn, PVP_HIT_CAP);   /* plafond par touche, sauf a la tete */
    if (oneShot) dmg = fmaxf(dmg, before);
    if (hf & HF_HEAD) headHostHits[w]++;
    if (chain_w(w) && from != v && from >= 0 && from < MAXP && !(hf & HF_CHAIN)) {   /* chaine miroir du tireur */
        hostChain[from] += (hf & HF_HEAD) ? CHAIN_HEAD_STEP : 1;
        if (hostChain[from] > STREAK_MAX) hostChain[from] = STREAK_MAX;
    }
    if (w != W_SELF && w != W_VOID && from != v && from < MAXP) { NP[v].lastHitBy = from; NP[v].lastHitT = nowT; }   /* kill zone */
    if (before >= 99.9f) lifeFirstHitT[v] = nowT;
    if (w == W_SLAM) { slamLastHitT[v] = nowT; slamLastBy[v] = from; }   /* onde puis rail : le combo vise */
    else if (w == W_RAIL && slamLastHitT[v] > 0 && slamLastBy[v] == from && nowT - slamLastHitT[v] < 1.5) {
        slamRailCombos++; slamLastHitT[v] = 0;
    }
    NP[v].hp -= dmg;
    if (NP[v].hp <= 0 && NP[v].alive) {
        if (w == W_VOID) {                                /* chute : kill au dernier attaquant s'il a touche moins de 4 s avant */
            int k = NP[v].lastHitBy;
            int credit = NP[v].lastHitT > 0 && nowT - NP[v].lastHitT < VOID_CREDIT_T && k >= 0 && k < MAXP && k != v && NP[k].active;
            if (credit) voidCredited++; else voidSuicides++;
            netlog("vide : %s tombe%s%s", pname(v), credit ? ", kill a " : " (suicide)", credit ? pname(k) : "");
            host_kill(credit ? k : v, v, W_VOID);
            return;
        }
        if (w != W_SELF) {
            ttkSum += nowT - lifeFirstHitT[v]; ttkN++;
            if (before >= 99.9f) oneShots++;              /* tue d'un seul coup a pleine vie */
            else if (nowT - lifeFirstHitT[v] < 0.1) burstKills++;   /* une salve (ALL IN ONE) */
        }
        if (oneShot) { if (hf & HF_CHAIN) chainOneShots++; else headOneShots++; }
        hostKillFlags = (hf & (HF_HEAD | HF_CHAIN)) | (oneShot ? HF_ONESHOT : 0);
        host_kill(from, v, w);
        hostKillFlags = 0;
    }
}
static void host_apply_damage(int from, int v, float dmgIn, int w) { host_apply_damage_fl(from, v, dmgIn, w, 0); }

/* Tesla : la victime v qui pare renvoie la decharge du tireur s ; renvoie 1 si la touche est annulee */
static int tesla_host_reflect(int v, int s) {
    if (NP[s].teslaReflT > 0 && nowT - NP[s].teslaReflT < TESLA_STUN) return 1;   /* deja en vol apres un renvoi */
    if (nowT - NP[v].lastParryT < parry_grace(W_TESLA, NP[v].st.pad & 15)) {
        NP[s].teslaReflT = nowT; teslaReflects++;
        NP[v].hp = fminf(100, NP[v].hp + PARRY_HEAL);
        NetEvent e = ev_new(EV_TESLA);
        e.sub = 1; e.actor = (unsigned char)v; e.target = (unsigned char)s;
        vf(e.f, v == myId ? pcenter() : np_center(v));
        net_emit(&e);
        apply_event(&e);
        if (NP[s].alive && (NP[s].hp -= TESLA_REFLECT_DMG) <= 0) host_kill(v, s, W_TESLA);
        return 1;
    }
    return 0;
}

/* coup de melee (ou impact de grappin) de `from` pare par v : EV_MELEE_PARRY (choc si les deux paraient) */
static void host_melee_parry(int v, int from, int w, int couched) {
    int clash = is_melee_w(w) && w != W_FLAIL && !couched && from < MAXP &&   /* Fleau : chaines croisees */
                nowT - NP[from].lastParryT <   /* Poings : la tolerance de l'attaquant est aussi de 0,05 s */
                ((NP[from].st.pad & 15) == MK_FIST ? FIST_PARRY_GRACE : MELEE_PARRY_GRACE);
    if (w == W_SLAM) slamParried++;
    if (w == W_LANCE) lanceHostParried++;
    if (w == W_FLAIL) flailHostParried++;                 /* Fleau */
    if (w == W_FIST) fistHostParried++;                   /* Poings */
    NetEvent e = ev_new(EV_MELEE_PARRY);
    e.actor = (unsigned char)v; e.target = (unsigned char)from; e.sub = (unsigned char)clash;
    e.f[0] = (float)w;                                    /* arme paree : l'attaquant rend ses points de melee */
    NP[v].hp = fminf(100, NP[v].hp + PARRY_HEAL);
    if (clash) clashes++; else meleeParries++;
    net_emit(&e);
    apply_event(&e);
}

/* --- Egide : arbitrage ------------------------------------------------- */
static int is_hitscan_w(int w) { return w == W_RAIL || w == W_COIN || w == W_KINETIC || w == W_SHOTGUN; }
/* coups directs que l'hote peut convertir en bloc (il ne connait ni la piece ni le centre d'un souffle ; l'onde passe dessous) */
static int shield_direct_w(int w) {
    return (is_hitscan_w(w) && w != W_COIN) || (is_melee_w(w) && w != W_SLAM) || w == W_HOOK || w == W_SAW || w == W_TESLA;
}
static int host_shield_ok(int v, V3 dir, float cosLim) {   /* bouclier leve (tolerance de latence), ni brise ni arrache, de face */
    return (NP[v].st.pad & 0x0F) == MK_SHIELD && NP[v].shieldBrokenT <= 0 && NP[v].shieldRipT <= 0 &&
           nowT - NP[v].lastShieldT < SHIELD_GRACE && vdot(dir, np_face(v)) < cosLim;
}
static int host_shield_perfect(int v) { return nowT - NP[v].lastParryT < MELEE_PARRY_GRACE; }
/* parry parfait d'un tir hitscan : renvoye au tireur (60 au plus), jauge intacte, +10 PV ; un kill revient au porteur */
static void host_shield_reflect(int v, int from, float dmg) {
    float back = fminf(dmg, SHIELD_REFLECT_PVP);
    shieldPerfects++; parryCount++;
    NP[v].hp = fminf(100, NP[v].hp + PARRY_HEAL);
    NetEvent e = ev_new(EV_SHIELD);
    e.sub = SHE_PERFECT; e.actor = (unsigned char)v; e.target = (unsigned char)from;
    vf(e.f, np_center(v)); vf(e.f + 3, np_center(from)); e.f[6] = back;
    net_emit(&e);
    apply_event(&e);
    if (from < MAXP && NP[from].active && NP[from].alive) host_apply_damage(v, from, back, W_SHIELD);
}
/* coup bloque : la jauge prend le coup (un souffle : la moitie, l'autre moitie passe), recul / 2, casse a 0 */
static void host_shield_block(int v, int from, int w, float dmg, V3 kn, int blast) {
    float absorbed = fminf(blast ? dmg * SHIELD_BLAST_FRAC : dmg, PVP_HIT_CAP);
    float passed = blast ? fminf(dmg * (1 - SHIELD_BLAST_FRAC), PVP_HIT_CAP) : 0;
    NP[v].shieldGauge = fmaxf(0, NP[v].shieldGauge - absorbed);
    shieldBlocks++;
    NetEvent e = ev_new(EV_SHIELD);
    e.sub = SHE_BLOCK; e.actor = (unsigned char)from; e.target = (unsigned char)v;
    e.f[0] = absorbed; e.f[1] = NP[v].shieldGauge; e.f[2] = passed; vf(e.f + 3, vmul(kn, SHIELD_KNOCK_MULT)); e.f[6] = (float)w;
    net_emit(&e);
    apply_event(&e);
    if (passed > 0) host_apply_damage(from, v, passed, w);
    if (NP[v].shieldGauge <= 0 && NP[v].alive) {         /* le coup qui brise est absorbe en entier */
        NP[v].shieldBrokenT = SHIELD_BROKEN_T; shieldBreaks++;
        NetEvent b = ev_new(EV_SHIELD);
        b.sub = SHE_BREAK; b.target = (unsigned char)v; b.actor = (unsigned char)from;
        net_emit(&b);
        apply_event(&b);
    }
}
/* EV_SHIELD : revendications des tireurs (CLAIM), arrachage (RIP). Les decisions ne viennent que de l'hote */
static int shield_host(const NetEvent *ev) {
    int v = ev->target, from = ev->from, w = ev->actor, blast = ev->sub == SHE_CLAIM_BLAST;
    if (ev->sub == SHE_RIP) {
        if (v >= MAXP) return 1;
        NP[v].shieldRipT = SHIELD_RIP_T; shieldRips++;
        return 0;
    }
    if (ev->sub != SHE_CLAIM && !blast) return from != myId;
    if (v >= MAXP || from >= MAXP || w >= W_COUNT || !NP[v].active || !NP[v].alive || matchOver) return 1;
    if (ev->netId && mine_fate(ev->netId) && mine_fate(ev->netId)->who != from) {   /* Ventouse : decision perdue */
        mineHitsIgnored++;
        return 1;
    }
    float dmg = ev->f[0];
    V3 dir = fv(ev->f + 1), kn = fv(ev->f + 4);
    if (w == W_TESLA && tesla_host_reflect(v, from)) return 1;
    if (!host_shield_ok(v, dir, SHIELD_COS_HOST)) {      /* refus : le coup suit le chemin normal (parry, degats complets) */
        NetEvent h = ev_new(EV_HIT);
        h.from = (unsigned char)from; h.target = (unsigned char)v; h.sub = (unsigned char)w; h.netId = ev->netId;
        h.f[0] = dmg; vf(h.f + 1, kn);
        h.pad[0] = (unsigned char)(ev->pad[0] & HF_CHAIN);/* chaine pleine : elle compte ; la tete, sans point d'impact, non */
        shieldDenied++;
        if (!host_handle(&h)) {
            NetEvent d = ev_new(EV_SHIELD);
            d.sub = SHE_DENIED; d.actor = (unsigned char)from; d.target = (unsigned char)v;
            d.f[0] = dmg; vf(d.f + 1, kn); d.f[4] = (float)w;
            net_emit(&d);
            apply_event(&d);
        }
        return 1;
    }
    if (!blast && host_shield_perfect(v)) {               /* fenetre parfaite */
        if (is_hitscan_w(w)) { host_shield_reflect(v, from, dmg); return 1; }
        if (is_parryable_w(w)) { host_melee_parry(v, from, w, 0); return 1; }
    }
    host_shield_block(v, from, w, dmg, kn, blast);
    return 1;
}
/* --- fin Egide : arbitrage --- */

/* arbitre un evenement ; renvoie 1 s'il est annule : il ne doit alors ni s'appliquer ni etre relaye
 * (sinon la victime d'un coup pare subirait quand meme le recul et le flash) */
static int host_handle(NetEvent *ev) {
    if (ev->kind == EV_PICKUP) { host_take_pickup(ev->sub, ev->from); return 0; }
    if (ev->kind == EV_MINE) return mine_host(ev);        /* Ventouse */
    if (ev->kind == EV_SHIELD) return shield_host(ev);    /* Egide */
    if (ev->kind == EV_HIT) {
        int v = ev->target;
        if (ev->sub >= W_COUNT) return 1;
        if (ev->sub != W_SELF && same_team(ev->from, v)) return 1;   /* equipes : jamais de degats entre allies */
        if (ev->netId && mine_fate(ev->netId) && mine_fate(ev->netId)->who != ev->from) {   /* Ventouse : decision perdue */
            mineHitsIgnored++;
            netlog("ventouse : touche de %s ignoree (mine arrachee ou deja explosee)", pname(ev->from));
            return 1;
        }
        hitCount[ev->sub]++;
        if (ev->sub == W_SAW && ev->f[4] > 0) sawHostBounceHits++;   /* Scie : touche apres un renvoi */
        netlog("degats : %s -> %s, %.0f (arme %d), PV avant %.0f", pname(ev->from), pname(v), ev->f[0], ev->sub, NP[v < MAXP ? v : 0].hp);
        int couched = ev->sub == W_LANCE && ev->f[6] > 0.5f;   /* Lance couchee : pas de fenetre de parry chez l'attaquant */
        if (ev->sub == W_LANCE) {
            if (couched) { lanceHostCouch++; lanceHostVcSum += ev->f[5]; } else lanceHostThrust++;
            netlog("lance %s : v %.1f, vitesse %.1f", couched ? "couchee" : "estoc", ev->f[5], ev->f[4]);
        }
        if (v >= MAXP || !NP[v].active || !NP[v].alive || matchOver) return 1;
        if (ev->sub == W_TESLA && ev->from < MAXP && tesla_host_reflect(v, ev->from)) return 1;   /* Tesla : decharge renvoyee */
        int hf = ev->pad[0] & (HF_HEAD | HF_CHAIN);       /* --- tete et chaine : l'hote valide, le relais porte sa decision --- */
        if ((hf & HF_HEAD) && !head_host_ok(ev)) {
            hf &= ~HF_HEAD; headRefused++;
            netlog("tete refusee : %s -> %s (arme %d)", pname(ev->from), pname(v), ev->sub);
        }
        if ((hf & HF_CHAIN) && !chain_host_claim(ev->from, ev->sub)) {
            hf &= ~HF_CHAIN; chainRefused++;
            netlog("chaine pleine refusee : %s", pname(ev->from));
        }
        ev->pad[0] = (unsigned char)hf;                   /* --- fin tete et chaine --- */
        int shieldFace = 1;                               /* --- Egide : face exigee pour parer, renvoi du hitscan, conversion --- */
        if ((NP[v].st.pad & 0x0F) == MK_SHIELD && ev->sub != W_SELF && ev->sub != W_VOID && ev->from < MAXP) {
            V3 sd = vnorm(vsub(np_center(v), np_center(ev->from)));   /* ligne attaquant -> victime vue par l'hote */
            int perfect = host_shield_perfect(v), face = vdot(sd, np_face(v)) < SHIELD_COS;
            if (is_parryable_w(ev->sub) && ev->sub != W_SLAM) shieldFace = face;   /* l'onde se pare toujours au timing */
            if (is_hitscan_w(ev->sub) && perfect && face) { host_shield_reflect(v, ev->from, ev->f[0]); return 1; }
            if (SHIELD_HOST_CONVERT && shield_direct_w(ev->sub) && !(perfect && is_parryable_w(ev->sub) && face) &&
                host_shield_ok(v, sd, SHIELD_COS)) {
                shieldConverted++;
                hitCount[ev->sub]--;                      /* bloque : pas une touche */
                host_shield_block(v, ev->from, ev->sub, ev->f[0], fv(ev->f + 1), 0);
                return 1;
            }
        }                                                 /* --- fin Egide --- */
        /* coup de melee ou impact de grappin pendant la fenetre de parry de la victime : c'est pare */
        if (is_parryable_w(ev->sub) && nowT - NP[v].lastParryT < parry_grace(ev->sub, NP[v].st.pad & 15) && shieldFace) {
            host_melee_parry(v, ev->from, ev->sub, couched);
            return 1;
        }
        if (couched && lance_host_joust(ev)) return 1;   /* Lance : choc de joute */
        host_apply_damage_fl(ev->from, v, ev->f[0], ev->sub, hf);
    } else if (ev->kind == EV_SAW && ev->sub < 6) {
        sawHostStat[ev->sub]++;                           /* Scie : compteurs du journal, rien a arbitrer */
    } else if (ev->kind == EV_PARRY && ev->from < MAXP) {
        parryCount++;
        NP[ev->from].hp = fminf(100, NP[ev->from].hp + PARRY_HEAL);
    }
    return 0;
}

static V3 pick_spawn(int who) {
    const MapDef *md = &maps[curMap];                    /* points de depart de la carte (coordonnees du monde) */
    int team = teamSize > 0 && who >= 0 && who < MAXP;
    if (team && md->teamSpawn && md->nTeamSpawn > 0) {    /* equipes : les departs de son camp (MapDef.teamSpawn) */
        int t = NP[who].team & 1, best = 0;
        float bd = -1;
        for (int k = 0; k < md->nTeamSpawn; k++) {
            const float *q = md->teamSpawn[t * md->nTeamSpawn + k];
            V3 p = v3(q[0], q[1], q[2]);
            if (!pos_free(p, H_STAND)) continue;
            float d = 200;                                /* le plus loin possible des ennemis */
            for (int i = 0; i < MAXP; i++) {
                if (i == who || !NP[i].active || !NP[i].alive || same_team(i, who)) continue;
                d = fminf(d, vlen(vsub(i == myId ? P.pos : NP[i].disp, p)));
            }
            d += frange(0, 3);
            if (d > bd) { bd = d; best = k; }
        }
        const float *q = md->teamSpawn[t * md->nTeamSpawn + best];
        return v3(q[0], q[1], q[2]);
    }
    float best = -1;
    V3 bp = md->start;
    for (int k = 0; k < md->nspawns; k++) {
        V3 p = v3(md->spawns[k][0], md->spawns[k][1], md->spawns[k][2]);
        if (!pos_free(p, H_STAND)) continue;
        float d = 200;                                    /* le plus loin possible des autres */
        for (int i = 0; i < MAXP; i++) {
            if (i == who || !NP[i].active || !NP[i].alive) continue;
            d = fminf(d, vlen(vsub(p, i == myId ? P.pos : NP[i].disp)));
        }
        d += frange(0, 6);
        if (d > best) { best = d; bp = p; }
    }
    return bp;
}

static void host_respawn(int i) {
    V3 p = pick_spawn(i);
    NP[i].alive = 1; NP[i].hp = 100;
    NP[i].lastHitT = 0; NP[i].lastHitBy = -1;            /* kill zone : une nouvelle vie ne doit rien a personne */
    hostChain[i] = hostChainSaid[i] = 0;                  /* chaine de tirs : vide a chaque vie, comme chez le joueur */
    NetEvent e = ev_new(EV_RESPAWN);
    e.target = (unsigned char)i; vf(e.f, p);
    net_emit(&e);
    apply_event(&e);
}

/* equipes : un depart a laisse un ecart de 2 ou plus -> le dernier arrive de la grosse equipe change de camp */
static void host_balance_teams(void) {
    if (teamSize <= 0) return;
    for (int guard = 0; guard < MAXP; guard++) {
        int a = team_count(0), b = team_count(1), big = a > b ? 0 : 1;
        if (abs(a - b) < 2) break;
        for (int i = MAXP - 1; i >= 1; i--)
            if (NP[i].active && NP[i].team == big) { NP[i].team = (unsigned char)(1 - big); netlog("%s passe chez les %s", pname(i), teamNames[1 - big]); break; }
    }
}

/* ROTATION : la carte suivante qui accueille le mode en cours (jamais une carte CHALLENGE) ; sinon la meme */
static int map_rotate_next(int from, int mode) {
    for (int k = 1; k <= NMAPS; k++) {
        int m = (from + k) % NMAPS;
        if (map_pvp(m) && mode_ok(mode, m)) return m;
    }
    return from;
}

static void host_start_match(void) {
    matchOver = 0; match_clock_start();
    mode_reset();
    host_balance_teams();
    for (int i = 0; i < MAXP; i++) NP[i].kills = NP[i].deaths = 0;
    NetEvent e = ev_new(EV_MATCH);
    e.target = (unsigned char)(mapRotate ? map_rotate_next(curMap, rules.mode) : curMap);   /* ROTATION : carte suivante a chaque match */
    if (ranked) e.target = (unsigned char)rk_map_pick(e.target);   /* classe : jamais une carte de course */
    if (e.target != curMap)                               /* changement de carte : chacun la charge, silence attendu */
        for (int i = 1; i < MAXP; i++) if (NP[i].active) NP[i].lastHeard = fmax(NP[i].lastHeard, nowT + NET_LOAD_GRACE);
    net_emit(&e);
    apply_event(&e);
    for (int i = 0; i < MAXP; i++) if (NP[i].active) host_respawn(i);
}

static void host_drop(int i) {
    if (!NP[i].active) return;
    NetEvent e = ev_new(EV_INFO);
    e.sub = 1; e.target = (unsigned char)i;
    net_emit(&e);
    apply_event(&e);
    NP[i].active = 0; NP[i].alive = 0;
}

static void host_send_roster(void) {
    PkRoster r;
    head(&r.h, PK_ROSTER);
    for (int i = 0; i < MAXP; i++) memcpy(r.names[i], NP[i].name, 16);
    for (int i = 0; i < MAXP; i++) {                      /* mode classe */
        r.mmr[i] = (unsigned short)rkMmr[i]; r.rplay[i] = (unsigned char)(rkPlay[i] > 255 ? 255 : rkPlay[i]);
        memcpy(r.rid[i], rkRid[i], 16); memcpy(r.left[i], rkLeft[i], 16);
    }
    r.ranked = (unsigned char)ranked; r.nleft = (unsigned char)rkLeftN; memcpy(r.mid, rkMid, 8);
    host_send_all(&r, sizeof r);
}

static void host_tick(float dt) {
    pack_state(&NP[myId].st);
    if (P.parryWin > 0) NP[myId].lastParryT = nowT;
    if (P.lanceState == LS_COUCH) NP[myId].lastLanceT = nowT;   /* Lance : choc de joute */
    NP[myId].disp = P.pos;
    P.hp = NP[myId].hp;
    if ((NP[myId].st.pad & ST_SHIELD) && NP[myId].shieldBrokenT <= 0 && NP[myId].shieldRipT <= 0) NP[myId].lastShieldT = nowT;
    for (int i = 0; i < MAXP; i++) {                      /* Egide : l'hote tient la jauge de chacun */
        NetPlayer *n = &NP[i];
        if (!n->active) continue;
        n->shieldBrokenT -= dt; n->shieldRipT -= dt;
        if (n->alive && !(n->st.pad & ST_SHIELD) && n->shieldBrokenT <= 0)
            n->shieldGauge = fminf(SHIELD_GAUGE_MAX, n->shieldGauge + SHIELD_REGEN * dt);
    }
    chain_host_said(myId, shotStreak);                    /* chaine miroir de l'hote lui-meme */
    if (!matchOver) {
        match_clock_tick();                               /* temps reel, pas les ticks (horloge.c) */
        mode_tick(dt);                                    /* zones, drapeaux, bouclier, salles de depart, SOUTIEN */
        if (!matchOver && matchT >= match_time_limit()) host_end_match(mode_time_winner());   /* temps ecoule : selon le mode */
        for (int i = 0; i < MAXP; i++)
            if (NP[i].active && !NP[i].alive && (NP[i].respawnT -= dt) <= 0) host_respawn(i);
    } else if (matchEndT <= 0) {
        host_start_match();
    }
}

static void host_on_join(const PkJoin *pj, const struct sockaddr_in *from) {
    int id = -1;
    if (pj->spec) {                                       /* spectateur : pas de slot NP, il recoit juste le flux */
        int k = -1;
        for (int i = 0; i < MAXSPEC; i++) if (SP[i].active && same_addr(&SP[i].addr, from)) k = i;
        for (int i = 0; i < MAXSPEC && k < 0; i++) if (!SP[i].active) k = i;
        PkWelcome w;
        head(&w.h, PK_WELCOME); w.id = MAXP; w.refused = k < 0; w.pad[0] = (unsigned char)curMap;
        w.pad[1] = (unsigned char)ranked;                 /* mode classe : le spectateur affiche les paliers */
        net_send(&w, sizeof w, from);
        if (k >= 0) {
            if (!SP[k].active) netlog("spectateur %d arrive", k);
            SP[k].active = 1; SP[k].addr = *from; SP[k].lastHeard = nowT;
            host_send_roster();
        }
        return;
    }
    if (ranked && !rid_set(pj->rid)) {                    /* mode classe : il faut une identite de classement */
        PkWelcome w;
        head(&w.h, PK_WELCOME); w.id = 0; w.refused = 2; w.pad[0] = w.pad[1] = 0;
        net_send(&w, sizeof w, from);
        return;
    }
    for (int i = 1; i < MAXP; i++) if (NP[i].active && same_addr(&NP[i].addr, from)) id = i;
    int fresh = id < 0;
    if (id < 0) {
        int count = 0;
        for (int i = 0; i < MAXP; i++) count += NP[i].active;
        for (int i = 1; i < MAXP && count < host_max_players(); i++) if (!NP[i].active) { id = i; break; }
        if (id < 0) {
            PkWelcome w;
            head(&w.h, PK_WELCOME); w.id = 0; w.refused = 1; w.pad[0] = (unsigned char)curMap; w.pad[1] = 0;
            net_send(&w, sizeof w, from);
            return;
        }
        memset(&NP[id], 0, sizeof NP[id]);
        NP[id].active = 1;
        memcpy(NP[id].name, pj->name, 15);
        NP[id].name[15] = 0;
        NP[id].addr = *from;
        NP[id].lastHeard = nowT + NET_LOAD_GRACE;         /* il charge la carte avant de repondre */
        NP[id].respawnT = 1.0f;
        NP[id].team = 0;                                  /* equipes : la moins nombreuse, bleue a egalite (id deja compte en bleu) */
        if (teamSize > 0) NP[id].team = (unsigned char)(team_count(0) - 1 > team_count(1) ? 1 : 0);
        NetEvent e = ev_new(EV_INFO);
        e.sub = 0; e.target = (unsigned char)id;
        net_emit(&e);
        apply_event(&e);
    }
    rkMmr[id] = pj->mmr ? rank_clamp(pj->mmr) : RANK_START; rkPlay[id] = pj->rplay;   /* mode classe : cache annonce */
    memcpy(rkRid[id], pj->rid, 16);
    PkWelcome w;
    head(&w.h, PK_WELCOME); w.id = (unsigned char)id; w.refused = 0; w.pad[0] = (unsigned char)curMap;
    w.pad[1] = (unsigned char)ranked;                     /* mode classe : la partie est classee */
    net_send(&w, sizeof w, from);
    host_send_roster();
    if (ranked && fresh && !matchOver && !rkHadOpp) {     /* classe : le match commence quand arrive le premier adversaire */
        int n = 0;
        for (int i = 0; i < MAXP; i++) n += NP[i].active;
        if (n == 2) { rk_new_match(); host_start_match(); netlog("classe : premier adversaire, le match recommence"); }
    }
}

/* evenement recu d'un client : l'hote l'arbitre, l'applique chez lui et le relaie a tout le monde,
 * sauf s'il l'a annule */
static void host_event_in(NetEvent *ev) {
    if (host_handle(ev)) return;
    apply_event(ev);
    net_queue(ev);
}

static void host_recv(PkHead *h, int n, struct sockaddr_in *from) {
    int id = -1, sp = -1;
    for (int i = 1; i < MAXP; i++) if (NP[i].active && same_addr(&NP[i].addr, from)) id = i;
    for (int i = 0; i < MAXSPEC; i++) if (SP[i].active && same_addr(&SP[i].addr, from)) sp = i;
    if (id >= 0) NP[id].lastHeard = nowT;
    if (sp >= 0) SP[sp].lastHeard = nowT;
    switch (h->type) {
    case PK_QUERY: {
        PkAnnounce a;
        int count = 0;
        for (int i = 0; i < MAXP; i++) count += NP[i].active;
        head(&a.h, PK_ANNOUNCE);
        memcpy(a.name, myName, 16);
        a.players = (unsigned char)count; a.maxp = (unsigned char)host_max_players(); a.teams = (unsigned char)teamSize; a.map = (unsigned char)curMap;
        a.ranked = (unsigned char)ranked; a.pad[0] = a.pad[1] = a.pad[2] = 0;   /* mode classe : [CLASSEE] dans la liste */
        net_send(&a, sizeof a, from);
        break;
    }
    case PK_JOIN:
        if (n >= (int)sizeof(PkJoin)) host_on_join((const PkJoin *)h, from);
        break;
    case PK_STATE:
        if (id >= 0 && n >= (int)sizeof(PkState)) {
            NP[id].st = ((PkState *)h)->st; NP[id].recvT = nowT;
            if (NP[id].st.flags & NF_PARRY) NP[id].lastParryT = nowT;
            if (NP[id].st.pad & ST_LANCE) NP[id].lastLanceT = nowT;   /* Lance couchee */
            chain_host_said(id, NP[id].st.spare[1]);     /* chaine de tirs annoncee */
            if ((NP[id].st.pad & ST_SHIELD) && NP[id].shieldBrokenT <= 0 && NP[id].shieldRipT <= 0)
                NP[id].lastShieldT = nowT;                /* Egide : un client ne prolonge pas un bouclier brise ou arrache */
        }
        break;
    case PK_EVENT:
        if (id >= 0 && n >= (int)sizeof(PkEvent)) {
            NetEvent ev = ((PkEvent *)h)->ev;
            if (ev.from != id || ev_seen(ev.eid)) break;  /* un client ne parle qu'en son nom */
            host_event_in(&ev);
        }
        break;
    case PK_LEAVE:
        if (id >= 0) host_drop(id);
        if (sp >= 0) { SP[sp].active = 0; netlog("spectateur %d part", sp); }
        break;
    }
}
