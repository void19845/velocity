/* VELOCITY - Ventouse (mines collantes)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- Ventouse : mines collantes ----------------------------------------- */
/* Lancee (24 m/s + vitesse du joueur, gravite 16), elle colle au decor, a un drone ou a un joueur. 2 actives au plus
 * (la 3e fait sauter la plus ancienne). Detonation a distance (toutes, ou la plus ancienne), sinon meche de 3 s depuis
 * le collage. Sur un joueur : armement de 0,6 s (le temps de l'arracher par un parry), puis 60 PvP garantis. Un rail
 * ou un ricochet la fait sauter x1,5, un plomb ou l'arc x1, meme celle d'un adversaire. Le poseur fait autorite sur
 * ses mines ; celui qui la fait sauter d'un tir calcule les degats. L'hote arbitre le sort de chaque mine (hostMine). */
static unsigned int mineCounter;
static int mineThrows, mineStickW, mineStickP, mineStickE, mineTorn, mineBoom[MX_COUNT], mineDetAll, mineDetOne;   /* journal */
static float mineArmedT;                                  /* somme des delais collage -> explosion (mines sur un joueur) */
static int mineArmedN;

static int mine_own(const Proj *p) { return p->alive && p->kind == PR_MINE && p->owner == myId; }
static int mine_count(void) {                             /* mes mines actives (sans detonation en attente) */
    int n = 0;
    for (int i = 0; i < MAXPR; i++) n += mine_own(&PR[i]) && !PR[i].mineDet;
    return n;
}
static Proj *mine_oldest(void) {                          /* la plus ancienne de mes mines sans detonation demandee */
    Proj *o = NULL;
    for (int i = 0; i < MAXPR; i++) {
        Proj *p = &PR[i];
        if (mine_own(p) && !p->mineDet && (!o || p->mineSeq < o->mineSeq)) o = p;
    }
    return o;
}
/* rail, plomb, arc : on peut tirer dedans, sauf une mine collee sur moi (il faut la parer) ou pas encore armee */
static int mine_shootable(const Proj *p) {
    if (!p->alive || p->kind != PR_MINE) return 0;
    if (p->mineState == MS_PLAYER && (p->mineArmT > 0 || p->mineAttach == myId + 1)) return 0;
    return 1;
}
static int mine_has_target(const Proj *p) {               /* ricochet : une cible a portee du souffle boost */
    for (int j = 0; j < MAXE; j++)
        if (E[j].alive && vlen(vsub(E[j].pos, p->pos)) < MINE_BOOST_RADIUS + enemy_radius(E[j].type)) return 1;
    for (int j = 0; j < MAXP; j++)
        if (np_target(j) && vlen(vsub(np_center(j), p->pos)) < MINE_BOOST_RADIUS + 0.5f) return 1;
    return 0;
}
static V3 mine_base(const Proj *p) {                      /* reference du porteur : pieds d'un joueur, centre d'un drone */
    int a = p->mineAttach - 1;
    if (p->mineState == MS_ENEMY && a >= 0 && a < MAXE) return E[a].pos;
    if (p->mineState == MS_PLAYER && a >= 0 && a < MAXP) return a == myId ? P.pos : NP[a].disp;
    return p->pos;
}
static V3 mine_col(const Proj *p) {                       /* cyan : la mienne ; rouge : celle d'un autre */
    return p->owner == myId && !spectating ? v3(0.3f, 1, 0.7f) : v3(1, 0.2f, 0.2f);
}
static void mine_remove_owner(int id) {                   /* depart d'un joueur : ses mines disparaissent */
    for (int i = 0; i < MAXPR; i++)
        if (PR[i].alive && PR[i].kind == PR_MINE && PR[i].owner == id) PR[i].alive = 0;
}

/* explosion calculee chez celui qui la declenche (poseur, ou tireur du rail / plomb / arc / ricochet).
 * byShot : declenchee par un tir (c'est le tir qui gere la chaine). Renvoie le nombre de cibles touchees */
static int mine_explode(Proj *p, int boosted, int byShot) {
    V3 c = p->pos;
    float dmg = MINE_DMG * (boosted ? MINE_BOOST_MULT : 1), rad = boosted ? MINE_BOOST_RADIUS : MINE_RADIUS;
    int se = -1, sp = -1, hits = 0, a = p->mineAttach - 1;
    p->alive = 0;
    if (dmMode) net_fx_mine(MINE_EV_BOOM, p->netId, c, v3(boosted ? 1.0f : 0.0f, 0, 0), 0, 0);
    if (p->mineState == MS_ENEMY && a >= 0 && a < MAXE && E[a].alive) {   /* porteur : degats garantis, sans degressivite */
        damage_enemy(a, dmg, vmul(vnorm(vsub(E[a].pos, c)), 12));
        se = a; hits++;
    }
    if (p->mineState == MS_PLAYER && a >= 0 && a < MAXP && np_target(a)) {
        V3 kd = vnorm(vadd(vnorm(vsub(np_center(a), c)), v3(0, 0.6f, 0)));
        V3 sd = p->mineShield ? vmul(np_face(a), -1) : vnorm(vsub(np_center(a), c));   /* Egide : collee au bouclier = de face */
        hit_player_ex(a, dmg * PVP_SCALE, vmul(kd, MINE_KNOCK + 3), W_MINE, sd, SHB_BLAST, p->netId, 0, 0, 0);
        sp = a; hits++;
        impact_frame(c, 0.06f); hitstop = fmaxf(hitstop, 0.05f);
        if (p->owner == myId) { mineArmedT += p->mineT; mineArmedN++; }
    }
    hits += explode_ex(c, rad, dmg, 1, MINE_KNOCK, W_MINE, se, sp, p->netId);   /* souffle, recul et 5 x f PV sur soi */
    if (hits && !byShot) shot_result(1);                  /* une mine ne casse jamais la chaine (arme de preparation) */
    if (hits) { hitMarker = 0.2f; play(S_HIT, 0.6f, 0.9f); }
    if (boosted) {
        impact_frame(c, 0.08f); hitstop = fmaxf(hitstop, 0.05f);
        burst(c, v3(1, 0.85f, 0.4f), 60, 16, 0.6f, 0.35f);
        play(S_EXPLO, 1.0f, 0.8f);
    }
    if (byShot) mineBoom[MX_SHOT]++;
    if (botMode) netlog("ventouse : explosion%s (%s), %d touche(s)", boosted ? " x1,5" : "",
                        p->mineState == MS_PLAYER ? "sur un joueur" : p->mineState == MS_WORLD ? "posee" : "autre", hits);
    return hits;
}

/* detonation demandee : tout de suite, ou des que la mine est armee (0,1 s de vol, 0,6 s sur un joueur) */
static void mine_detonate(Proj *p, int why) {
    if ((p->mineState == MS_FLY && p->mineT < MINE_ARM_FLY - 0.5f * TICK) || (p->mineState == MS_PLAYER && p->mineArmT > 0)) {
        p->mineDet = why + 1;
        return;
    }
    mineBoom[why]++;
    mine_explode(p, 0, 0);
}

/* telecommande (quelle que soit l'arme en main) : all = toutes mes mines, sinon la plus ancienne */
static void mine_remote(int all) {
    Proj *old = mine_oldest();
    if (!old) { play(S_HIT, 0.5f, 0.45f); return; }      /* rien a faire sauter : clic sec */
    if (all) {
        for (int i = 0; i < MAXPR; i++)
            if (mine_own(&PR[i]) && !PR[i].mineDet) mine_detonate(&PR[i], MX_REMOTE);
        mineDetAll++;
    } else { mine_detonate(old, MX_REMOTE); mineDetOne++; }
    P.recoil = fmaxf(P.recoil, 0.5f);
}

static void fire_mine(void) {
    if (P.ammo[WP_MINE] < 1) return;
    if (mine_count() >= MINE_MAX_ACTIVE) {                /* la 3e fait sauter la plus ancienne */
        Proj *o = mine_oldest();
        if (o) mine_detonate(o, MX_THIRD);
    }
    V3 f = view_dir();
    Proj *p = spawn_proj(PR_MINE, vadd(eye_pos(), vmul(f, 0.5f)), vadd(vmul(f, MINE_SPEED), P.vel));
    if (!p) return;
    P.ammo[WP_MINE]--;
    p->mineState = MS_FLY; p->life = MINE_FLY_LIFE; p->mineT = 0; p->mineN = v3(0, 1, 0);
    p->netId = ((unsigned int)(myId + 1) << 24) | ((unsigned int)PR_MINE << 20) | (++mineCounter & 0xFFFFF);
    p->mineSeq = mineCounter;
    if (dmMode) net_fx_mine(MINE_EV_THROW, p->netId, p->pos, p->vel, 0, 0);
    P.fireCd = MINE_CD; P.recoil = 0.8f; muzzleT = 0.05f;
    mineThrows++;
    play(S_MINE_THROW, 0.7f, frange(0.95f, 1.05f));
}

/* mine collee sur MOI par un adversaire : un parry l'arrache et la renvoie dans ma direction de visee */
static int mine_tear(Proj *p, V3 f) {
    if (!dmMode || p->owner == myId || p->mineState != MS_PLAYER || p->mineAttach != myId + 1) return 0;
    int from = p->owner;
    p->kind = PR_PARRIED; p->mineState = MS_FLY; p->owner = myId; p->parries = 1; p->orb = 0;
    p->mult = PARRY_ROCKET_MULT; p->life = 4;
    p->pos = vadd(eye_pos(), vmul(f, 0.8f)); p->vel = vmul(f, PARRY_SPEED);   /* visee exacte, pas d'aide */
    net_fx_mine(MINE_EV_TORN, p->netId, p->pos, p->vel, myId, p->mult);
    P.swingLanded = 1;
    parry_feedback(p->pos);
    melee_parry_points(PTS_PARRY);
    snprintf(popText, sizeof popText, "ARRACHEE !"); popT = 1.2f;
    mineTorn++;
    if (botMode) netlog("ventouse : mine de %s arrachee", NP[from < MAXP ? from : 0].name);
    return 1;
}

/* piege : mine posee sur le decor, armee depuis MINE_PROX_ARM, un adversaire (ou un drone) a moins de MINE_PROX_R */
static int mine_prox(const Proj *p) {
    if (p->mineT < MINE_PROX_ARM - 0.5f * TICK) return 0;
    for (int j = 0; j < MAXE; j++)
        if (E[j].alive && vlen(vsub(E[j].pos, p->pos)) < MINE_PROX_R + enemy_radius(E[j].type)) return 1;
    for (int j = 0; j < MAXP; j++)
        if (np_target(j) && vlen(vsub(np_center(j), p->pos)) < MINE_PROX_R + 0.5f) return 1;
    return 0;
}

static void mine_stuck_fx(Proj *p, float vol) {
    p->mineT = p->mineFell ? p->mineT : 0;                /* retombee d'un porteur : la meche continue */
    p->vel = v3(0, 0, 0); p->mineBeepT = MINE_BEEP_SLOW;
    for (int k = 0; k < 8; k++) add_part(p->pos, vmul(rand_dir(), 2), mine_col(p), 0.2f, 0.06f, 0);
    play(S_MINE_STICK, vol, 1.0f);
}

static void mine_tick(Proj *p, float dt) {
    int mine = !dmMode || p->owner == myId;               /* seul le poseur decide */
    int a = p->mineAttach - 1;
    p->mineT += dt;
    switch (p->mineState) {
    case MS_FLY: {
        p->vel.y -= MINE_GRAVITY * dt;
        V3 step = vmul(p->vel, dt), dir = v3(0, -1, 0), n;
        float len = vlen(step);
        if (len > 1e-6f) dir = vmul(step, 1.0f / len); else len = 1e-6f;
        float t = ray_world(p->pos, dir, len, &n), te;
        if (mine) {
            int he = -1, hp = -1;
            for (int j = 0; j < MAXE; j++)
                if (E[j].alive && ray_sphere(p->pos, dir, E[j].pos, enemy_radius(E[j].type) + MINE_STICK_GROW, t, &te)) { t = te; he = j; }
            for (int j = 0; j < MAXP; j++) {
                V3 mn, mx;
                if (!np_target(j) || j == p->owner) continue;
                np_aabb(j, MINE_STICK_GROW, &mn, &mx);
                if (ray_box(p->pos, dir, mn, mx, t, &te, NULL) && te < t) { t = te; hp = j; he = -1; }
            }
            V3 hit = vadd(p->pos, vmul(dir, t));
            if (he >= 0) {                                /* drone (solo) : il l'emporte avec lui */
                p->mineState = MS_ENEMY; p->mineAttach = he + 1; p->mineOfs = vsub(hit, E[he].pos); p->pos = hit;
                mineStickE++;
            } else if (hp >= 0) {                         /* joueur : position relative a ses pieds, bornee a son corps */
                V3 o = vsub(hit, NP[hp].disp);
                o.x = clampf(o.x, -(HW + 0.05f), HW + 0.05f); o.z = clampf(o.z, -(HW + 0.05f), HW + 0.05f);
                o.y = clampf(o.y, 0.1f, np_height(hp));
                p->mineShield = shield_faces(hp, dir, SHB_DIRECT);   /* Egide : collee sur la face du bouclier */
                if (p->mineShield) o = shield_front_ofs(hp);
                p->mineState = MS_PLAYER; p->mineAttach = hp + 1; p->mineOfs = o; p->mineArmT = MINE_ARM_PLAYER;
                p->pos = vadd(NP[hp].disp, o); p->mineFell = 0;
                if (dmMode) net_fx_mine(MINE_EV_PLAYER, p->netId, o, v3(0, 0, 0), hp, (float)p->mineShield);
                mineStickP++;
                if (botMode) netlog("ventouse : mine collee sur %s", NP[hp].name);
            } else if (t < len) {                         /* decor : point + normale */
                p->mineState = MS_WORLD; p->pos = vadd(hit, vmul(n, MINE_SKIN)); p->mineN = n;
                if (dmMode) net_fx_mine(MINE_EV_WORLD, p->netId, p->pos, n, 0, 0);
                mineStickW++;
            }
            if (p->mineState != MS_FLY) { mine_stuck_fx(p, 0.6f); return; }
            if (p->mineT >= MINE_FLY_LIFE) { mineBoom[MX_FLY]++; mine_explode(p, 0, 0); return; }
            if (p->mineDet && p->mineT >= MINE_ARM_FLY - 0.5f * TICK) { mineBoom[p->mineDet - 1]++; mine_explode(p, 0, 0); return; }
        } else if (t < len) {                             /* copie : prevision visuelle, corrigee par EV_MINE */
            p->mineState = MS_WORLD; p->pos = vadd(vadd(p->pos, vmul(dir, t)), vmul(n, MINE_SKIN)); p->mineN = n;
            mine_stuck_fx(p, 0.6f * atten(p->pos));
            return;
        }
        p->pos = vadd(p->pos, step);
        if (((int)(p->mineT * 120)) % 2 == 0)             /* petite trainee, une fois sur deux */
            add_part(p->pos, v3(0, 0, 0), mine_col(p), 0.2f, 0.06f, 0);
        break;
    }
    case MS_ENEMY:
        if (a < 0 || a >= MAXE || !E[a].alive) {          /* le drone meurt avant : elle retombe */
            p->mineState = MS_FLY; p->vel = (a >= 0 && a < MAXE) ? E[a].vel : v3(0, 0, 0); p->mineAttach = 0; p->mineFell = 1;
            break;
        }
        p->pos = vadd(E[a].pos, p->mineOfs);
        break;
    case MS_PLAYER: {
        int gone = a < 0 || a >= MAXP || !NP[a].active || (a == myId ? P.dead : !NP[a].alive);
        if (gone) {                                       /* porteur mort ou parti : elle retombe, la meche continue */
            V3 v = (a >= 0 && a < MAXP && a != myId) ? vmul(np_vel(a), 0.5f) : v3(0, 0, 0);
            p->mineState = MS_FLY; p->vel = v; p->mineAttach = 0; p->mineFell = 1;
            if (mine && dmMode) net_fx_mine(MINE_EV_THROW, p->netId, p->pos, p->vel, 0, 0);
            break;
        }
        float h = a == myId ? pheight() : np_height(a);
        if (p->mineShield) p->mineOfs = shield_front_ofs(a);   /* Egide : elle suit la face du bouclier */
        if (p->mineOfs.y > h) p->mineOfs.y = h;           /* porteur accroupi : la mine ne flotte pas */
        p->pos = vadd(mine_base(p), p->mineOfs);
        if ((p->mineArmT -= dt) < 0.5f * TICK) p->mineArmT = 0;   /* armee au 72e tick, sans erreur d'arrondi */
        break;
    }
    default:
        break;
    }
    if (p->mineState >= MS_WORLD) {                       /* bip : plus rapide vers la fin de la meche */
        float k = clampf(p->mineT / MINE_FUSE, 0, 1);
        if ((p->mineBeepT -= dt) <= 0) {
            int onMe = p->mineState == MS_PLAYER && a == myId;
            p->mineBeepT = MINE_BEEP_SLOW + (MINE_BEEP_FAST - MINE_BEEP_SLOW) * k;
            play(S_MINE_BEEP, onMe ? 0.9f : 0.5f * atten(p->pos), onMe ? 1.2f : 1.0f);
        }
    }
    if (mine && p->mineState >= MS_WORLD) {
        int armed = p->mineState != MS_PLAYER || p->mineArmT <= 0;
        if (armed && p->mineDet) { mineBoom[p->mineDet - 1]++; mine_explode(p, 0, 0); }
        else if (armed && p->mineT >= MINE_FUSE - 0.5f * TICK) { mineBoom[MX_FUSE]++; mine_explode(p, 0, 0); }
        else if (p->mineState == MS_WORLD && mine_prox(p)) {   /* piege : un adversaire passe a cote */
            mineBoom[MX_PROX]++;
            if (botMode) netlog("ventouse : piege declenche");
            mine_explode(p, 0, 0);
        }
    } else if (!mine && p->mineState >= MS_WORLD && p->mineT > MINE_FUSE + 1) {
        p->alive = 0;                                     /* EV_MINE BOOM perdu : suppression silencieuse */
    }
}
/* --- fin Ventouse --------------------------------------------------------- */
