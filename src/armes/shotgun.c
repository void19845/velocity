/* VELOCITY - SHOTGUN (fusil a pompe, ex-Eventail)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- SHOTGUN (fusil a pompe) -------------------------------------------- */
/* 10 plombs a motif deterministe (generateur local : le rng global n'est jamais touche, chaque client
 * recalcule les memes directions a partir de la graine d'EV_SHOT). Les degats sont agreges par cible :
 * un seul net_hit par joueur et par tir. Dispersion divisee par 2 en slide, un plomb qui touche une piece
 * la fait eclater (souffle de 3 m, ni degats ni recul sur soi).
 * Recul : la VITESSE ACTUELLE du tireur est AJOUTEE a sa vitesse a l'oppose de la visee (sg_push_speed : SG_KNOCK_RATIO m/s par m/s, sans
 * plafond), au sol comme en l'air ; la cible subit le meme recul (vitesse ajoutee, knock_apply) dans le sens du tir, x la part de plombs
 * qui l'ont touchee. Immobile : pas de recul.
 * En l'air, un residu de vitesse (sg_recoil) garde le saut au recul. RICOCHET ouvert : chaque plomb rebondit
 * (RIC_PELLET_CAP rebonds au plus, SG_RANGE par segment, degats selon la distance totale parcourue). */
static unsigned int sgCounter;                            /* compteur de graines */
static int sgBursts;                                      /* pieces eclatees au dernier tir (tests, journal) */
static int sgShots, sgPelletSum, sgStreakShots, sgJumps, sgBurstSum;   /* journal du bot */
static int sgPellRic, sgPellRicSum;                        /* rebonds de plombs au dernier tir (tests) et au total (journal) */
static float sgLastPush;                                  /* poussee (m/s) du dernier tir (tests) */
/* VAGUES : effets de la boutique. Les etapes qui sont des STATS (taille, cone, portee, perte a distance, plombs en plus, delai, cout de jauge,
 * courbure) sont lues dans le sac de stats (boutique_stats.c) par sg_stats, une seule fois par tir ; les evenements et comportements (rafale,
 * coup double, culasse, peau de plomb) et les capstones sont dans src/solo/boutique_shotgun.c (definis plus bas).
 * SG_PMAX : capacite en plombs d'une salve */
#define SG_PMAX 16
#define SG_PCAP 24                                        /* capacite des tableaux de fire_shotgun : SG_PMAX plombs, +50 % pour la salve All in One (capstone AIO>SHO) */
static int sgFree;                                       /* salve gratuite (rafale, coup double) : ni munition, ni recul, ni recharge */
static int vgsg_cap_near(void);
static float vgsg_salvo_mul(void);
static void vgsg_refund(float cost);
static void vgsg_pellet_hit(V3 at, int enemy);
static int vgsg_coin_mode(void);
static void vgsg_coins(const V3 *dir, int n);
static void vgsg_after(V3 eye, V3 d, int pellets, int kills);

static float sg_rand(unsigned int *s) { *s = *s * 1664525u + 1013904223u; return (*s >> 8) * (1.0f / 16777216.0f); }
static unsigned int sg_new_seed(void) {                   /* unique, deterministe, sans toucher rng */
    return (((unsigned int)myId + 1) << 24) ^ ((++sgCounter) * 2654435761u);
}
/* motif semi-fixe : 1 plomb au centre, 3 sur un anneau a 0.45, 6 sur un anneau a 0.9, tourne par la graine */
static void sg_pellet_dirs(V3 d, float half, unsigned int seed, V3 *out) {
    unsigned int s = seed;
    V3 r = vnorm(vcross(d, v3(0, 1, 0.01f))), u = vcross(r, d);   /* meme astuce que fire_rail : jamais nul */
    float rot = sg_rand(&s) * 2 * PI, th = tanf(half);
    for (int k = 0; k < SG_PELLETS; k++) {
        float rho, ang;
        if (k == 0) { rho = 0; ang = 0; }
        else if (k < 4) { rho = 0.45f; ang = rot + (k - 1) * (2 * PI / 3); }
        else { rho = 0.9f; ang = rot + PI / 6 + (k - 4) * (PI / 3); }
        rho += (sg_rand(&s) - 0.5f) * 0.2f;               /* +-0.1 */
        ang += (sg_rand(&s) - 0.5f) * 0.35f;              /* +-10 deg */
        if (rho > 1) rho = 1;
        out[k] = vnorm(vadd(d, vadd(vmul(r, cosf(ang) * rho * th), vmul(u, sinf(ang) * rho * th))));
    }
}
/* directions des plombs au-dela des SG_PELLETS du motif (VAGUES : Plombs en plus), tirees dans le meme cone */
static void sg_extra_dirs(V3 d, float half, unsigned int seed, V3 *dir, int n) {
    unsigned int s = seed ^ 0x9e3779b9u;
    V3 r = vnorm(vcross(d, v3(0, 1, 0.01f))), u = vcross(r, d);
    float th = tanf(half);
    for (int k = SG_PELLETS; k < n; k++) {
        float rho = sqrtf(sg_rand(&s)) * 0.9f, ang = sg_rand(&s) * 2 * PI;
        dir[k] = vnorm(vadd(d, vadd(vmul(r, cosf(ang) * rho * th), vmul(u, sinf(ang) * rho * th))));
    }
}
/* PLOMBS CHERCHEURS / TRAQUEURS (VAGUES, stat SUI) : chaque plomb se courbe de deg degres vers l'ennemi le plus proche de la visee */
static void sg_seek(V3 eye, V3 d, V3 *dir, int n, float deg) {
    if (deg <= 0) return;
    int best = -1;
    float bc = cosf(25 * PI / 180.0f);
    for (int j = 0; j < MAXE; j++) {
        if (!E[j].alive || E[j].summoned == 2) continue;
        float c = vdot(d, vnorm(vsub(E[j].pos, eye)));
        if (c > bc && los(eye, E[j].pos)) { bc = c; best = j; }
    }
    if (best < 0) return;
    V3 t = vnorm(vsub(E[best].pos, eye));
    float lim = deg * PI / 180.0f;
    for (int k = 0; k < n; k++) {
        float ang = acosf(clampf(vdot(dir[k], t), -1, 1));
        if (ang < 1e-4f) continue;
        dir[k] = vnorm(vlerp(dir[k], t, fminf(1, lim / ang)));
    }
}

/* Stats du Shotgun pour UN tir, lues une seule fois (fire_shotgun, piece en plombs, formes fusionnees). Hors VAGUES, ou Shotgun non equipe,
 * st() rend la base : ce sont les valeurs du PvP. `half` et `range` sont les bases du tireur (cone apres le slide, portee de l'arme).
 * ATTENTION : near et far sont des macros vides de windef.h, d'ou les noms fallA / fallB. */
typedef struct {
    float half;                                           /* demi-angle du cone (rad) : Gerbe large, Cone serre */
    int extra;                                            /* plombs en plus, au plus SG_PMAX - SG_PELLETS : Plombs en plus */
    float range;                                          /* portee d'un plomb (m) : Pompe tendue */
    float size;                                           /* taille des plombs (x) : Gros plomb */
    float fallA, fallB, fallMin;                          /* perte de degats : pleins jusqu'a fallA, puis lineaire jusqu'a fallMin a fallB : Cone serre, Pompe tendue, Chevrotine */
    float seek;                                           /* courbure des plombs vers la cible (deg) : Plombs chercheurs / traqueurs */
    float cd;                                             /* delai entre deux salves (x) : Pompe vive */
    float cost;                                           /* cout de jauge d'un tir (x) : Double canon */
    int capNear;                                          /* capstone SHO>LAN (Shotgun de joute) : x1,4 sous 3,5 m */
} SgStats;
static void sg_stats(SgStats *S, float half, float range) {
    S->half = half; S->extra = 0; S->range = range; S->size = 1; S->cd = 1; S->cost = 1; S->seek = 0;
    S->fallA = SG_NEAR; S->fallB = SG_FAR; S->fallMin = SG_FAR_MULT; S->capNear = 0;
    if (!vagOn) return;
    S->half = fmaxf(0.01f, st(SH_SHO, STAT_CONE, half));
    int ex = (int)(st(SH_SHO, STAT_NBP, 0) + 0.5f);
    S->extra = ex > SG_PMAX - SG_PELLETS ? SG_PMAX - SG_PELLETS : ex;
    S->range = st(SH_SHO, STAT_POR, range);
    S->size = st(SH_SHO, STAT_TAI, 1.0f);
    S->fallA = st(SH_SHO, STAT_PNEAR, SG_NEAR); S->fallB = st(SH_SHO, STAT_PFAR, SG_FAR);
    float loss = st(SH_SHO, STAT_PERTE, 1.0f);            /* part de la perte de degats a distance qui reste */
    if (loss != 1.0f) S->fallMin = 1 - (1 - SG_FAR_MULT) * loss;
    S->seek = st(SH_SHO, STAT_SUI, 0);
    S->cd = st(SH_SHO, STAT_CD, 1.0f);
    S->cost = st(SH_SHO, STAT_COUT, 1.0f);
    S->capNear = vgsg_cap_near();
}
static float sg_fall(const SgStats *S, float t) {         /* plein jusqu'a 10 m, puis lineaire jusqu'a x0.3 a 30 m (valeurs de base) */
    float m;
    if (t <= S->fallA) m = 1;
    else if (t >= S->fallB) m = S->fallMin;
    else m = 1 - (1 - S->fallMin) * (t - S->fallA) / (S->fallB - S->fallA);
    if (S->capNear && t < 3.5f) m *= 1.4f;
    return m;
}
/* poussee (m/s) d'un tir tire a la vitesse `speed` : proportionnelle (SG_KNOCK_RATIO), au moins SG_KNOCK_MIN, sans plafond (SG_KNOCK_MAX = 0) */
static float sg_push_speed(float speed) {
    float p = fmaxf(SG_KNOCK_MIN, SG_KNOCK_RATIO * speed);
    return SG_KNOCK_MAX > 0 ? fminf(SG_KNOCK_MAX, p) : p;
}
static V3 sg_tint(float k) { return vlerp(v3(1, 0.6f, 0.25f), v3(1, 0.9f, 0.4f), clampf(k, 0, 1)); }   /* cuivre -> or */

/* cumul d'un tir : degats, recul, plombs et dernier point touche par cible ; dd : degats des plombs arretes par un leurre */
typedef struct {
    float e[MAXE], p[MAXP], dd[MAXCOIN];
    V3 ke[MAXE], kp[MAXP], he[MAXE], hp[MAXP];
    int ne[MAXE], np[MAXP], burstHit;
} SgAcc;

/* eclat de piece : mine = 1 chez le tireur (impact frame, hitstop), 0 pour celui d'un autre joueur */
static void sg_burst_fx(V3 o, int mine) {
    burst(o, v3(1, 0.82f, 0.3f), 40, 10, 0.6f, 0.18f);
    shards(o, v3(1, 0.82f, 0.3f), 6, 7);
    play(S_EXPLO, 0.6f * (mine ? 1 : atten(o)), 1.7f);
    if (mine) { impact_frame(o, 0.05f); hitstop = fmaxf(hitstop, 0.04f); }
}

static void sg_coin_burst(int ci, int depth, SgAcc *A) {
    Coin *c = &CO[ci];
    if (!c->alive) return;                                /* deja eclatee par un autre plomb */
    if (c->decoy) {                                       /* MIRAGE adverse : arrete le plomb, n'eclate pas ; RICOCHET : repart vers le poseur */
        if (depth) return;
        int owner = decoy_reflect_owner(ci);
        float acc = A->dd[ci];
        V3 o = c->pos;
        decoy_absorb(ci, "plomb");
        if (owner >= 0 && acc > 0) {                      /* les plombs arretes par le leurre sont ajoutes a ceux du poseur */
            A->p[owner] += acc;
            A->kp[owner] = vadd(A->kp[owner], vmul(vnorm(vsub(np_center(owner), o)), 4));
            A->burstHit = 1; decoyReflects++;
            snprintf(popText, sizeof popText, "LEURRE RENVOYE !"); popT = 1.0f;
            add_beam(o, np_center(owner), v3(0.4f, 1, 0.7f), 0.6f);
        }
        return;
    }
    V3 o = c->pos;
    c->alive = 0; sgBursts++;
    if (dmMode) net_fx_coin(1, c->netId, o, v3(1, 0, 0));   /* f[3] = 1 : detruite EN ECLAT */
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive) continue;
        V3 dv = vsub(E[i].pos, o);
        float dd = vlen(dv), R = SG_BURST_RADIUS + enemy_radius(E[i].type);
        if (dd < R) {
            float f = 1 - dd / R;
            A->e[i] += SG_BURST_DMG * (0.3f + 0.7f * f);
            A->ke[i] = vadd(A->ke[i], vmul(vnorm(dv), SG_BURST_KNOCK * f)); A->burstHit = 1;
        }
    }
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i)) continue;
        V3 dv = vsub(np_center(i), o);
        float dd = vlen(dv), R = SG_BURST_RADIUS + 0.5f;
        if (dd < R) {
            float f = 1 - dd / R;
            A->p[i] += SG_BURST_DMG * (0.3f + 0.7f * f);
            A->kp[i] = vadd(A->kp[i], vmul(vnorm(dv), SG_BURST_KNOCK * f + 2)); A->burstHit = 1;
        }
    }
    /* soi : ni degats ni recul (comme explode byPlayer == 2) */
    sg_burst_fx(o, 1);
    for (int j = 0; depth < SG_BURST_CHAIN && j < MAXCOIN; j++)   /* reaction en chaine */
        if (CO[j].alive && vlen(vsub(CO[j].pos, o)) < SG_BURST_RADIUS && los(o, CO[j].pos))
            sg_coin_burst(j, depth + 1, A);
}

/* saut au recul : residu de vitesse de 7 m/s a l'oppose de la visee, en l'air ; peut freiner, ou pousser jusqu'a un seuil, jamais
 * au-dela de la vitesse qu'on avait deja (9 m/s vertical, 16 m/s horizontal). La vitesse actuelle s'ajoute ensuite (fire_shotgun). */
static void sg_recoil(V3 d) {
    V3 v0 = P.vel, v = vadd(P.vel, vmul(d, -SG_RECOIL));
    if (v.y > v0.y && v.y > fmaxf(v0.y, SG_RECOIL_VMAX)) v.y = fmaxf(v0.y, SG_RECOIL_VMAX);
    float h0 = hlen(v0), h = hlen(v), lim = fmaxf(h0, SG_RECOIL_HCAP);
    if (h > lim) { v.x *= lim / h; v.z *= lim / h; }
    if (d.y < -0.7f) sgJumps++;                           /* stat : shotgun jump */
    P.vel = v;
}

static void fire_shotgun(void) {
    int fr = sgFree;
    SgStats S;                                            /* VAGUES : tout ce que la boutique change est lu ici, une fois par tir (sac de stats) */
    sg_stats(&S, SG_HALF_ANGLE * (P.sliding ? SG_SLIDE_SPREAD : 1), SG_RANGE);
    if (!fr && P.ammo[WP_SHOTGUN] > 0) { P.ammo[WP_SHOTGUN]--; if (vagOn) vgsg_refund(S.cost); }
    enum { SGSEG = SG_PCAP * (RIC_PELLET_CAP + 1) };   /* segments de plomb au plus (un par rebond avec RICOCHET) */
    V3 eye = eye_pos(), d = view_dir(), dir[SG_PCAP];
    float half = S.half, mult = streak_mult();
    int nP = SG_PELLETS + S.extra, kills = 0;
    float rng = S.range, gros = S.size;
    if (vagOn) { mult *= vgsg_salvo_mul(); cp_aio_sho_fire(&half, &nP); }
    unsigned int seed = sg_new_seed();
    static SgAcc A;
    static V3 segA[SGSEG], segB[SGSEG], segN[SGSEG];      /* trace de chaque segment : depart, arrivee, normale du mur */
    static unsigned char segWall[SGSEG], segFirst[SGSEG]; /* le segment finit sur un mur (etincelles) ; premier segment d'un plomb */
    static int burstList[SGSEG], mineList[SGSEG];
    int pellets = 0, nBurst = 0, nMineHit = 0, nSeg = 0, ricMax = (!fr && ric_take()) ? RIC_PELLET_CAP : 0;
    float push = sg_push_speed(vlen(P.vel));           /* poussee de ce tir : ma vitesse actuelle, AVANT le residu de sg_recoil */
    memset(&A, 0, sizeof A);
    sgPellRic = 0; sgLastPush = push;
    sg_pellet_dirs(d, half, seed, dir);
    if (nP > SG_PELLETS) sg_extra_dirs(d, half, seed, dir, nP);
    sg_seek(eye, d, dir, nP, S.seek);                     /* Plombs chercheurs / traqueurs (S.seek = 0 : rien) */
    if (vagOn && vgsg_coin_mode()) { vgsg_coins(dir, nP); nP = 0; }   /* capstone PIECES : plus de plomb, des pieces */
    for (int k = 0; k < nP; k++) {                /* le plomb s'arrete sur le plus proche : piece, drone ou joueur */
        V3 o = eye, dk = dir[k];
        float travelled = 0;                              /* distance deja parcourue (les degats baissent avec elle) */
        for (int sgm = 0; sgm <= ricMax; sgm++) {         /* RICOCHET : un segment par rebond (un seul sans lui) */
            V3 nrm;
            float tw = ray_world(o, dk, rng, &nrm), t;
            int ci = -1, te = -1, tp = -1, mi = -1, wall = tw < rng;
            for (int i = 0; i < MAXCOIN; i++)             /* toute piece, la mienne ou non (MIRAGE : pas mon leurre) */
                if (CO[i].alive && !(CO[i].decoy && CO[i].owner == myId) && !(sgm == 0 && decoy_skip(i, eye)) &&
                    ray_sphere(o, dk, CO[i].pos, SG_COIN_RADIUS, tw, &t)) { tw = t; ci = i; }
            for (int i = 0; i < MAXE; i++)
                if (E[i].alive && ray_sphere(o, dk, E[i].pos, enemy_radius(E[i].type) * gros, tw, &t)) { tw = t; te = i; ci = -1; }
            for (int i = 0; i < MAXP; i++) {
                V3 mn, mx;
                if (!np_target(i)) continue;
                np_aabb(i, 0.05f, &mn, &mx);
                if (ray_box(o, dk, mn, mx, tw, &t, NULL) && t < tw) { tw = t; tp = i; te = ci = -1; }
            }
            for (int i = 0; i < MAXPR; i++)               /* --- Ventouse : le plomb s'arrete sur une mine (x1) --- */
                if (mine_shootable(&PR[i]) && ray_sphere(o, dk, PR[i].pos, MINE_HIT_R, tw, &t)) { tw = t; te = tp = ci = -1; mi = i; }
            V3 e = vadd(o, vmul(dk, tw));
            float fo = sg_fall(&S, travelled + tw);
            if (vagOn && te >= 0) fo = vg_rail_open_mul(te, fo);   /* capstone RAIL>SHO : cible ouverte par le rail (x1,3, sans perte a distance) */
            float dmg = SG_PELLET_DMG * fo * mult;
            int stopped = 1;
            if (te >= 0) {
                A.e[te] += dmg; A.ke[te] = vadd(A.ke[te], vmul(dk, SG_PELLET_KNOCK)); A.ne[te]++; A.he[te] = e;
                pellets++;
                if (vagOn) vgsg_pellet_hit(e, te);
            } else if (tp >= 0) {
                A.p[tp] += dmg; A.kp[tp] = vadd(A.kp[tp], vmul(dk, push / SG_PELLETS)); A.np[tp]++; A.hp[tp] = e;
                pellets++;
            } else if (ci >= 0) {                         /* eclate apres la boucle (une fois par piece) */
                burstList[nBurst++] = ci;
                if (CO[ci].decoy) A.dd[ci] += dmg;        /* leurre adverse : ces degats repartent vers son poseur si j'ai RICOCHET */
            } else if (mi >= 0) mineList[nMineHit++] = mi;   /* --- fin Ventouse --- */
            else stopped = 0;
            if (nSeg < SGSEG) {
                segA[nSeg] = o; segB[nSeg] = e; segN[nSeg] = nrm; segFirst[nSeg] = (unsigned char)(sgm == 0);
                segWall[nSeg] = (unsigned char)(wall && !stopped); nSeg++;
            }
            if (stopped || !wall || sgm == ricMax) break; /* cible touchee, sortie de la carte (aucun mur a SG_RANGE) ou garde-fou */
            travelled += tw;
            dk = vnorm(vsub(dk, vmul(nrm, 2 * vdot(dk, nrm))));   /* reflexion r = d - 2 (d.n) n */
            o = vadd(e, vmul(nrm, 0.02f));
            sgPellRic++;
        }
    }
    sgBursts = 0;
    for (int b = 0; b < nBurst; b++) sg_coin_burst(burstList[b], 0, &A);
    int juice = 0; V3 juiceAt = eye;
    for (int i = 0; i < MAXE; i++) {                      /* un seul chiffre de degats par cible */
        if (A.e[i] <= 0) continue;
        V3 at = A.ne[i] ? A.he[i] : E[i].pos;
        if (A.ne[i] >= 8 && vlen(vsub(E[i].pos, eye)) < 6) { juice = 1; juiceAt = at; }
        burst(at, v3(1, 0.45f, 0.2f), 12, 7, 0.3f, 0.1f);
        damage_enemy(i, A.e[i], A.ke[i]);
        if (!E[i].alive) kills++;
    }
    for (int i = 0; i < MAXP; i++) {                      /* AGREGATION : un seul net_hit par joueur et par tir */
        if (A.p[i] <= 0) continue;
        V3 at = A.np[i] ? A.hp[i] : np_center(i);
        if (A.np[i] >= 8 && vlen(vsub(np_center(i), eye)) < 6) { juice = 1; juiceAt = at; }
        burst(at, v3(1, 0.45f, 0.2f), 12, 7, 0.3f, 0.1f);
        if (!hit_player_ex(i, A.p[i] * PVP_SCALE, A.kp[i], W_SHOTGUN, A.np[i] ? d : vnorm(vsub(np_center(i), eye)),
                           A.np[i] ? SHB_DIRECT : SHB_BLAST, 0, 0, 0, 0) && A.np[i])
            pellets -= A.np[i];                           /* Egide : plombs bloques = rates (eclat seul : souffle, 50 %) */
    }
    for (int b = 0; b < nMineHit; b++)                    /* --- Ventouse : chaque mine touchee explose une fois --- */
        if (PR[mineList[b]].alive && PR[mineList[b]].kind == PR_MINE) {
            int foreign = PR[mineList[b]].owner != myId;
            if (mine_explode(&PR[mineList[b]], 0, 1)) A.burstHit = 1;
            snprintf(popText, sizeof popText, foreign ? "MINE VOLEE !" : "MINE !"); popT = 1.0f;
        }                                                 /* --- fin Ventouse --- */
    if (vagOn) vgsg_after(eye, d, pellets, fr ? 0 : kills);
    int hit = pellets >= SG_STREAK_MIN || A.burstHit;
    shot_result(hit);
    sgShots++; sgPelletSum += pellets; sgStreakShots += hit; sgBurstSum += sgBursts; sgPellRicSum += sgPellRic;
    if (botMode) netlog("tir pompe : %d plomb(s), %d eclat(s), %s", pellets, sgBursts, hit ? "touche" : "rate");
    if (!P.grounded && !fr) sg_recoil(d);                        /* en l'air : le residu de vitesse (saut au recul) */
    if (push > 0 && !fr) P.vel = vadd(P.vel, vmul(d, -push));    /* recul du tireur : sa vitesse actuelle AJOUTEE a l'oppose de la visee (vraie vitesse), sol et air */

    V3 m = muzzle(), col = sg_tint(shotStreak / (float)STREAK_MAX);   /* effets locaux : traceurs, etincelles, son */
    for (int s = 0; s < nSeg; s++) {
        add_tracer(segFirst[s] ? m : segA[s], segB[s], col);
        if (segWall[s])
            for (int j = 0; j < 3; j++)
                add_part(segB[s], vadd(vmul(segN[s], 3), vmul(rand_dir(), 2)), v3(1, 0.75f, 0.4f), 0.35f, 0.07f, 12);
    }
    if (dmMode) net_fx_shot(eye, d, half, seed);
    if (!fr) P.fireCd = P.sgPumpMax = P.sgPumpT = SG_CD / streak_rate() * S.cd;   /* VAGUES : Pompe vive (S.cd = 1 sinon) */
    P.recoil = 1.6f; shake += 0.1f + 0.012f * pellets; muzzleT = 0.09f;
    P.sgLastPellets = pellets; P.sgLastT = pellets ? 0.5f : 0;
    play(S_SG_FIRE, 0.85f, frange(0.96f, 1.04f));
    if (pellets || A.burstHit) { hitMarker = 0.18f; play(S_HIT, 0.6f, 0.9f + 0.03f * pellets); }
    if (juice) { hitstop = fmaxf(hitstop, 0.05f); impact_frame(juiceAt, 0.05f); }
    if (sgBursts) {
        if (sgBursts > 1) snprintf(popText, sizeof popText, "ECLAT x%d", sgBursts);
        else snprintf(popText, sizeof popText, "ECLAT !");
        popT = 1.0f;
    }
}
/* --- fin SHOTGUN -------------------------------------------------------- */
