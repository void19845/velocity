/* VELOCITY - boutique du mode VAGUES : effets de la graine ROQUETTES (genre BK_ROQ).
   Morceau de main.c : inclus apres ameliorations.c, ne se compile pas seul.
   Les etapes sont des entrees [BK_ROQ, id] dans docs/boutique_vagues/outils/effets/roquettes.js (id = RQ_*, ordre du pool
   de l'arbre : profondeur * 5 + rang) ; charge creuse / roquette lourde (degats), eclats / detonation en chaine (FX) passent
   par le moteur de ameliorations.c. Capstones et fusions : roq_cap() / roq_fu() par partenaire.
   Tout agit uniquement si vagOn et si le lance-roquettes est equipe. Les crochets sont declares dans monde/entites.c et
   appeles par roquettes.c, projectiles.c, degats.c (courbe du souffle), loadout.c (munitions max), lance.c, poings.c. */

enum { RQ_CHARGE, RQ_SOUTE, RQ_PROP, RQ_RAYON, RQ_OGIVE, RQ_COMB, RQ_FUSEE, RQ_DOUBLE, RQ_SOUFFLE, RQ_DIRECT,
       RQ_CRATERE, RQ_RECUP, RQ_ECLATS, RQ_GRAND, RQ_COEUR, RQ_CHAIN, RQ_SALVE, RQ_COURBE, RQ_GRAPPE, RQ_LOURDE };

static float roqEdge = 0.3f, roqCore;                     /* courbe du souffle en cours (roq_blast_k) */
static int roqAllinDone;                                  /* ALL IN ONE : roquettes en plus deja parties pour cette salve */
static int roqStat[8];                                    /* tests : 0 mini-roquettes, 1 plombs, 2 artifice, 3 poing, 4 joute */

static float roq_sc(int id) { return vagOn && up_equipped(WP_ROCKET) ? sh_scale(SH_ROQ, id) : 0; }   /* echelle (niveau) de l'etape RQ_id, 0 si absente */
static int roq_has(int id) { return roq_sc(id) > 0; }
static int roq_cap(int partner) {                         /* capstone ROQ>partenaire achete, les deux graines equipees */
    if (!vagOn || !sh_on(SH_ROQ) || !sh_on(partner)) return 0;
    int b = sh_branch(SH_ROQ, partner);
    return b >= 0 && shCap[b] && shBr[b].capKind == BK_ROQ;
}
static int roq_fu(int partner) {                          /* fusion ROQ-partenaire achetee */
    return vagOn && shFusion >= 0 && shFu[shFusion].a == SH_ROQ && shFu[shFusion].b == partner && shFu[shFusion].kind == BK_ROQ &&
           sh_on(SH_ROQ) && sh_on(partner);
}

static int roq_max(int w, int base) {                     /* SOUTE : +1 ; MINI-NUKE : une seule munition */
    if (w != WP_ROCKET || !vagOn || !up_equipped(WP_ROCKET)) return base;
    if (roq_cap(SH_RAIL)) return 1;
    return base + (roq_has(RQ_SOUTE) ? 1 : 0);
}

static V3 roq_rot(V3 v, float a) {                        /* tourne la vitesse autour de la verticale */
    float c = cosf(a), s = sinf(a);
    return v3(v.x * c - v.z * s, v.y, v.x * s + v.z * c);
}
/* copie de la roquette p, tournee de a radians, a la puissance mult */
static Proj *roq_clone(Proj *p, float a, float mult) {
    Proj *q = spawn_proj(PR_ROCKET, p->pos, p->vel);
    if (!q) return NULL;
    *q = *p;
    q->vel = roq_rot(p->vel, a); q->mult = mult;
    return q;
}

/* vitesse, taille et portee d'une roquette qui part (tir normal, joute, etc.) */
static void roq_setup(Proj *p) {
    float m = 1;
    if (roq_has(RQ_PROP)) m *= 1 + 0.1f * roq_sc(RQ_PROP);   /* PROPULSEUR */
    if (roq_has(RQ_LOURDE)) m *= 1 - 0.1f * roq_sc(RQ_LOURDE);   /* ROQUETTE LOURDE : vitesse -10 % (les degats +40 % : stat DEG du sac de stats ; la vitesse sera VIT x0,90 a la migration de la famille) */
    if (roq_cap(SH_LAN) && P.lanceState == LS_COUCH) m *= 2;   /* ROQUETTE MONTEE : vitesse de joute (+100 %) */
    p->vel = vmul(p->vel, m);
    p->roqSpd = vlen(p->vel);
    if (roq_has(RQ_OGIVE)) p->roqSize = 1 + 0.2f * roq_sc(RQ_OGIVE);   /* OGIVE LARGE */
    if (roq_has(RQ_COMB)) p->life += 20.0f * roq_sc(RQ_COMB) / fmaxf(p->roqSpd, 1.0f);   /* COMBUSTIBLE : 20 m de plus */
}

/* un tir de roquette (fire_rocket) : cadence des roquettes en plus */
static void roq_fire(Proj *p) {
    V3 d = view_dir();
    if (roq_cap(SH_LAN) && P.lanceState == LS_COUCH && ray_world(eye_pos(), d, 2.0f, 0) >= 2.0f)
        p->pos = vadd(eye_pos(), vmul(d, 2.0f));          /* ROQUETTE MONTEE : elle part de la pointe de la lance couchee */
    roq_setup(p);
    if (roq_has(RQ_SALVE)) {                              /* SALVE : 2 roquettes a 60 % (x niveau, 100 % au plus) */
        float sm = fminf(1.0f, 0.6f * roq_sc(RQ_SALVE));
        p->mult = sm;
        roq_clone(p, 0.03f, sm);
        p->vel = roq_rot(p->vel, -0.03f);
    }
    if (roq_has(RQ_DOUBLE) && frand() < 0.10f * roq_sc(RQ_DOUBLE)) roq_clone(p, 0.07f, p->mult);   /* DOUBLE DEPART : 10 % */
    if (allinActive) {
        if (roq_cap(SH_AIO) && !roqAllinDone) {           /* ROQUETTE SALVE : l'All in One tire 8 roquettes en plus */
            roqAllinDone = 1;
            for (int k = 0; k < 8; k++) roq_clone(p, -0.3f + 0.6f * (float)k / 7.0f, p->mult);
        }
        if (roq_fu(SH_AIO)) {                             /* TAPIS DE BOMBES : 3 roquettes en eventail par munition */
            roq_clone(p, -0.12f, p->mult);
            roq_clone(p, 0.12f, p->mult);
        }
    }
}

/* vol : trajectoire courbe, ralentissement pres de soi */
static void roq_tick_proj(Proj *p, float dt) {
    if (p->seekKind || !up_equipped(WP_ROCKET)) return;
    int curve = roq_has(RQ_COURBE), slow = roq_cap(SH_IMP);
    if (!curve && !slow) return;
    if (p->roqSpd <= 0) p->roqSpd = vlen(p->vel);
    V3 dir = vnorm(p->vel);
    if (curve) {                                          /* TRAJECTOIRE COURBE : elle devie vers l'ennemi proche */
        int t = fx_nearest(p->pos, 14, -1, -1);
        if (t >= 0) {
            V3 to = vnorm(vsub(E[t].pos, p->pos));
            if (vdot(to, dir) > 0.3f) dir = vnorm(vlerp(dir, to, fminf(1, 3 * roq_sc(RQ_COURBE) * dt)));   /* vitesse de virage x niveau */
        }
    }
    float f = slow && vlen(vsub(p->pos, pcenter())) < 5 ? 0.6f : 1;   /* ROQUETTE LENTE : -40 % a moins de 5 m de toi */
    p->vel = vmul(dir, p->roqSpd * f);
}

static int roq_prox(Proj *p) {                            /* FUSEE DE PROXIMITE : un ennemi a moins de 2 m */
    if (!roq_has(RQ_FUSEE)) return 0;
    for (int j = 0; j < MAXE; j++)
        if (E[j].alive && E[j].summoned != 2 && vlen(vsub(E[j].pos, p->pos)) < 2.0f * roq_sc(RQ_FUSEE) + enemy_radius(E[j].type)) return 1;
    return 0;
}
static float roq_hitpad(Proj *p) {                        /* OGIVE LARGE : la roquette (0,55 m) 20 % plus grosse */
    if (vagOn && p->kind == PR_ROCKET && p->roqSize > 1) return 0.2f + 0.55f * (p->roqSize - 1);
    return 0.2f;
}
static float roq_blast_k(float f) {                       /* SOUFFLE PLEIN / COEUR DU SOUFFLE ; reactions en chaine : courbe normale */
    if (vgDepth) return 0.3f + 0.7f * f;
    return (roqEdge + (1 - roqEdge) * f) * (1 + roqCore * f);
}
static float roq_seek_speed(Proj *p) {                    /* ROQUETTE-PIECE : la roquette redirigee garde sa vitesse */
    if (vagOn && p->roqSpd > 0 && roq_cap(SH_PIE)) return p->roqSpd;
    return COIN_ROCKET_SPEED;
}

/* plombs tires depuis c : sur les ennemis visibles (sinon au hasard), le premier touche ou tous les traverses (pierce) */
static void roq_pellets(V3 c, int n, float dmg, int pierce) {
    int cand[MAXE], nc = 0;
    for (int j = 0; j < MAXE; j++)
        if (E[j].alive && E[j].summoned != 2 && vlen(vsub(E[j].pos, c)) < 18 && los(c, E[j].pos)) cand[nc++] = j;
    for (int k = 0; k < n; k++) {
        V3 d = nc > 0 ? vnorm(vadd(vsub(E[cand[k % nc]].pos, c), vmul(rand_dir(), 0.4f))) : rand_dir();
        float wall = fminf(18, ray_world(c, d, 18, 0));
        int hit[MAXE], nh = 0;
        float ht[MAXE];
        for (int j = 0; j < MAXE; j++) {
            float te;
            if (E[j].alive && E[j].summoned != 2 && ray_sphere(c, d, E[j].pos, enemy_radius(E[j].type) + 0.15f, wall, &te)) {
                hit[nh] = j; ht[nh++] = te;
            }
        }
        int first = -1;
        for (int q = 0; q < nh; q++) if (first < 0 || ht[q] < ht[first]) first = q;
        float end = wall;
        if (!pierce && first >= 0) end = ht[first];
        add_beam(c, vadd(c, vmul(d, end)), v3(1, 0.8f, 0.4f), 0.15f);
        roqStat[1]++;
        if (pierce) { for (int q = 0; q < nh; q++) fx_damage(hit[q], dmg, vmul(d, 3), WP_ROCKET); }
        else if (first >= 0) fx_damage(hit[first], dmg, vmul(d, 3), WP_ROCKET);
    }
}

static float roq_radius(Proj *p) {
    float r = ROCKET_RADIUS;
    if (roq_has(RQ_RAYON)) r += 0.4f * roq_sc(RQ_RAYON);  /* RAYON */
    if (roq_has(RQ_GRAND)) r += 0.8f * roq_sc(RQ_GRAND);  /* GRAND SOUFFLE */
    if (p->roqGen) r *= 0.55f;                            /* mini-roquette de la GRAPPE */
    else if (roq_cap(SH_RAIL)) r = 14.0f;                 /* MINI-NUKE */
    return r;
}

/* souffle d'une roquette du joueur (remplace explode() dans proj_tick) : renvoie le nombre d'ennemis touches */
static int roq_explode(Proj *p, V3 c, int hitEnemy, float mult) {
    float r = roq_radius(p), dmg = ROCKET_DMG * mult;
    if (!p->roqGen && roq_cap(SH_RAIL)) dmg *= 5;         /* MINI-NUKE : x5 */
    roqEdge = roq_has(RQ_SOUFFLE) ? fminf(0.95f, 0.3f + 0.15f * roq_sc(RQ_SOUFFLE)) : 0.3f;
    roqCore = roq_has(RQ_COEUR) ? 0.25f * roq_sc(RQ_COEUR) : 0;
    char was[MAXE];
    int inB = 0;
    for (int i = 0; i < MAXE; i++) {
        was[i] = E[i].alive && vlen(vsub(E[i].pos, c)) < r + enemy_radius(E[i].type);
        inB += was[i];
    }
    roqBlastOn = 1;
    int hits = explode(c, r, dmg, 1);
    roqBlastOn = 0;
    int kills = 0;
    for (int i = 0; i < MAXE; i++) kills += was[i] && !E[i].alive;
    if (hitEnemy >= 0 && roq_has(RQ_DIRECT)) damage_enemy(hitEnemy, dmg * 0.15f * roq_sc(RQ_DIRECT), v3(0, 0, 0));   /* IMPACT DIRECT : +15 % */
    if (roq_has(RQ_CRATERE) && hits > 0) vg_heal(6.0f * roq_sc(RQ_CRATERE) * (float)(hits < 3 ? hits : 3));   /* CRATERE VITAL */
    if (roq_has(RQ_RECUP) && kills > 0 && inB >= 2 && P.ammo[WP_ROCKET] < wp_max(WP_ROCKET)) P.ammo[WP_ROCKET]++;   /* RECUPERATION */
    if (!p->roqGen) {
        if (roq_has(RQ_GRAPPE))                           /* GRAPPE : 3 mini-roquettes a 30 % */
            for (int k = 0; k < 3; k++) {
                V3 d = rand_dir();
                d.y = fabsf(d.y) * 0.5f + 0.3f;
                Proj *q = spawn_proj(PR_ROCKET, c, vmul(vnorm(d), ROCKET_SPEED * 0.7f));
                if (!q) break;
                q->mult = 0.3f * roq_sc(RQ_GRAPPE); q->roqGen = 1; q->life = 2.5f; q->roqSpd = vlen(q->vel);
                roqStat[0]++;
            }
        if (roq_cap(SH_SHO)) roq_pellets(c, 6, 40, 0);    /* ROQUETTE CLUSTER : 6 plombs au souffle */
        if (roq_fu(SH_SHO)) roq_pellets(c, 3, ROCKET_DMG * 0.4f * sh_fusion_scale(), 1);   /* SALVE D'ARTILLERIE : 3 plombs perforants (40 %) */
    }
    return hits;
}

/* ROQUETTE-PIECE x FEU D'ARTIFICE : la roquette redirigee explose a chaque piece, qui n'est pas consommee */
static int roq_coin(Proj *p, int ci) {
    if (!roq_fu(SH_PIE) || p->kind != PR_ROCKET) return 0;
    p->roqCoinLast = ci + 1;
    roqStat[2]++;
    fx_blast(CO[ci].pos, 3.0f, 60 * sh_fusion_scale(), WP_ROCKET);
    return 1;
}

/* LANCE-ROQUETTES : une joute reussie lache une roquette gratuite a x1,5 */
static void roq_joust(void) {
    if (!roq_fu(SH_LAN)) return;
    Proj *q = spawn_proj(PR_ROCKET, eye_pos(), vmul(view_dir(), ROCKET_SPEED));
    if (!q) return;
    roq_setup(q);
    q->mult = 1 + 0.5f * sh_fusion_scale();
    roqStat[4]++;
    play(S_ROCKET, 0.8f, 1.0f);
}

/* ACROBATIE EXPLOSIVE : un coup de poing sur une de tes roquettes l'explose et te propulse de 25 m/s */
static int roq_fist(V3 eye, V3 f) {
    if (!roq_fu(SH_IMP)) return 0;
    for (int k = 0; k < MAXPR; k++) {
        Proj *p = &PR[k];
        if (!p->alive || p->kind != PR_ROCKET || p->owner != myId) continue;
        V3 d = vsub(p->pos, eye);
        float dist = vlen(d);
        if (dist > MELEE_RANGE + 1.5f || vdot(vmul(d, 1.0f / fmaxf(dist, 0.01f)), f) < 0.3f) continue;
        int s0 = vgSrc, k0 = vgProjKind;
        vgSrc = WP_ROCKET; vgProjKind = PR_ROCKET;
        p->alive = 0;
        roq_explode(p, p->pos, -1, p->mult);
        vgSrc = s0; vgProjKind = k0;
        V3 away = vsub(pcenter(), p->pos);
        away.y = fmaxf(away.y, 0.6f);
        P.vel = vmul(vnorm(away), 25.0f * sh_fusion_scale());
        P.grounded = 0;
        roqStat[3]++;
        return 1;
    }
    return 0;
}

static void roq_fx_tick(float dt) {
    (void)dt;
    if (!vagOn) return;
    if (!allinActive) roqAllinDone = 0;
    if (roq_cap(SH_RAIL) && P.ammo[WP_ROCKET] > 1) P.ammo[WP_ROCKET] = 1;   /* MINI-NUKE : 1 seule munition */
}
