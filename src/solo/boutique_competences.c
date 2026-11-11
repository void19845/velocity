/* VELOCITY - boutique du mode VAGUES : effets des graines PIECE et ALL IN ONE (genre BK_COMP)
   Morceau de main.c : inclus apres ameliorations.c, ne se compile pas seul (voir main.c).

   Chaque etape ou capstone de ce groupe a un identifiant CP_* (champ a de l'entree dans
   docs/boutique_vagues/outils/effets/competences.js). Tout est inerte hors du mode VAGUES (vagOn) : aucune valeur de
   skillDef n'est modifiee, les charges / recharges sont recalculees a l'usage (cp_max, cp_recharge). */


static int cpStat[CP_K_AIO_SHO + 1];                      /* declenchements par effet (tests) */
static float cpOrbT[MAXCOIN], cpOrbA[MAXCOIN];           /* Piece orbitale : temps d'orbite restant, angle */
static int cpBounce[MAXCOIN];                             /* rebonds deja faits par chaque piece (PIE > IMP) */
static unsigned int cpPierce[MAXCOIN][2];                 /* ennemis deja traverses par chaque piece (PIE > LAN) */
static int cpFocus[MAXE], cpSalveId, cpSalveFull, cpSalveW;
static float cpSalveT;                                    /* fenetre de la salve ALL IN ONE (touches, kills) */
static int cpDblN, cpDblW;                                /* SALVE DOUBLE : 2e salve en attente (nombre de tirs, arme) */
static float cpDblT;

static void allin_volley(int w, int n);                   /* src/combat/competences.c */
static void fire_weapon(void);

/* l'etape d'identifiant id est-elle achetee, la graine de sa competence etant equipee ? */
static float cp_sc(int id) {                              /* echelle (niveau) de l'etape id, 0 si absente */
    if (!vagOn) return 0;
    for (int sd = SH_PIE; sd <= SH_AIO; sd++) {
        if (!sh_on(sd)) continue;
        float s = sh_sc_of(sd, BK_COMP, id);
        if (s > 0) return s;
    }
    return 0;
}
static int cp_has(int id) { return cp_sc(id) > 0; }
/* le capstone d'identifiant id est-il achete, sa graine et son partenaire etant equipes ? */
static int cp_cap(int id) {
    if (!vagOn) return 0;
    for (int b = 0; b < SH_NBR; b++)
        if (shCap[b] && shBr[b].capKind == BK_COMP && shBr[b].capA == id && sh_on(shBr[b].seed) && sh_on(shBr[b].partner)) return 1;
    return 0;
}
static V3 cp_rot_y(V3 f, float a) {                       /* rotation d'un vecteur autour de l'axe vertical */
    float c = cosf(a), s = sinf(a);
    return v3(f.x * c - f.z * s, f.y, f.x * s + f.z * c);
}

/* --- PIECE --- */
static float cp_coin_mult(int depth) {
    float m = COIN_MULT;
    if (!vagOn) return m;
    if (cp_has(CP_POLIE)) m = COIN_MULT + 0.1f * cp_sc(CP_POLIE);   /* PIECE POLIE : x1,5 -> x1,6 (le gain monte avec le niveau) */
    if (depth <= 0 && cp_has(CP_PRIME)) m = COIN_MULT + (2.0f - COIN_MULT) * cp_sc(CP_PRIME);   /* PRIME DE RICOCHET : le 1er ricochet fait x2 */
    if (depth >= 2 && cp_has(CP_PUR)) m *= powf(1 + 0.25f * cp_sc(CP_PUR), (float)(depth - 1));   /* RICOCHET PUR : des le 3e rebond, +25 % cumules par rebond */
    return m;
}
static float cp_coin_life(void) { return 4.0f + cp_sc(CP_VOLANTE) * 1.0f + cp_sc(CP_ORBITALE) * 4.0f; }   /* PIECE VOLANTE : +1 s ; ORBITALE : +4 s d'orbite */
static float cp_seek_range(void) { return COIN_SEEK_RANGE + cp_sc(CP_LESTEE) * 15.0f; }   /* PIECE LESTEE : +15 m */
/* PIECE ORBITALE : la piece tourne 4 s autour du joueur (un tir qui la touche ricoche), puis part vers l'ennemi le plus proche */
static int cp_coin_orbit(int ci, float dt) {
    Coin *c = &CO[ci];
    if (cpOrbT[ci] <= 0 || c->owner != myId) return 0;
    cpOrbT[ci] -= dt; cpOrbA[ci] += dt * 5; c->spin += dt * 3;
    c->life -= dt;
    if (c->life <= 0) { c->alive = 0; cp_coin_pellets(c->pos); return 1; }   /* fin de l'orbite : capstone PIE>SHOTGUN */
    V3 e = eye_pos();
    c->pos = v3(e.x + cosf(cpOrbA[ci]) * 2.5f, e.y - 0.3f, e.z + sinf(cpOrbA[ci]) * 2.5f);
    c->vel = v3(0, 0, 0);
    if (cpOrbT[ci] <= 0) {                                /* fin de l'orbite : elle part vers l'ennemi proche */
        int t = fx_nearest(c->pos, 60, -1, -1);
        c->vel = t >= 0 ? vmul(vnorm(vsub(E[t].pos, c->pos)), 18) : vadd(vmul(view_dir(), 9), v3(0, 6, 0));
        cpStat[CP_ORBITALE]++;
    }
    return 1;
}
static int cp_coin_dirs(V3 f, V3 *out) {
    out[0] = f;
    if (!vagOn) return 1;
    if (cp_cap(CP_K_PIE_RAIL)) {                          /* PLUIE DE PIECES : 8 pieces en cercle autour de toi */
        V3 h = v3(f.x, 0, f.z);
        h = vlen(h) < 0.01f ? v3(0, 0, 1) : vnorm(h);
        for (int k = 0; k < 8; k++) out[k] = cp_rot_y(h, k * (2 * PI / 8));
        cpStat[CP_K_PIE_RAIL]++;
        return 8;
    }
    if (cp_has(CP_PLUIE)) {                               /* PLUIE : 2 pieces en eventail */
        out[0] = cp_rot_y(f, 0.12f); out[1] = cp_rot_y(f, -0.12f);
        cpStat[CP_PLUIE]++;
        return 2;
    }
    return 1;
}
static void cp_coin_new(int ci) { cpOrbT[ci] = 4.0f * cp_sc(CP_ORBITALE); cpOrbA[ci] = ci * 1.3f; cpBounce[ci] = 0; cpPierce[ci][0] = cpPierce[ci][1] = 0; }
static int cp_coin_bounce(int ci, V3 n) {                 /* PIECE DE POING : la piece rebondit sur le decor (4 fois au plus) */
    if (!cp_cap(CP_K_PIE_IMP) || CO[ci].decoy || cpBounce[ci] >= 4) return 0;
    cpBounce[ci]++; cpStat[CP_K_PIE_IMP]++;
    CO[ci].vel = vmul(vsub(CO[ci].vel, vmul(n, 2 * vdot(CO[ci].vel, n))), 0.85f);
    CO[ci].pos = vadd(CO[ci].pos, vmul(n, 0.1f));
    burst(CO[ci].pos, v3(1, 0.85f, 0.3f), 6, 3, 0.2f, 0.07f);
    return 1;
}
/* PIECE EN PLOMBS (capstone PIE>SHOTGUN) : une piece qui finit (touchee par un de mes tirs au moment du ricochet, rail ou roquette, ou
   en fin de vie : elle retombe, expire ou finit son orbite) explose en SG_PELLETS plombs partis de sa position, vers les ennemis visibles
   les plus proches (PIE_PELLET_TARGETS au plus, un plomb chacun a tour de role), en eventail devant moi s'il n'y en a aucun. Chaque plomb
   vaut un plomb du Shotgun (perte a distance et taille de la boutique) et ses degats comptent pour le chemin du Shotgun (ses etapes de
   degats : PLOMB LOURD, POING DE PLOMB...) ; un seul chiffre par ennemi. Une piece touchee par un PLOMB garde son eclat habituel
   (sg_coin_burst) sans plombs en plus : pas de doublon. Les stats du Shotgun viennent de sg_stats (sac de stats). */
#define PIE_PELLET_TARGETS 3
static void cp_coin_pellets(V3 o) {
    if (!vagOn || P.dead || !cp_cap(CP_K_PIE_SHO)) return;
    cpStat[CP_K_PIE_SHO]++;
    SgStats S;                                            /* portee, taille et perte a distance du Shotgun : sac de stats, lu une fois */
    sg_stats(&S, SG_HALF_ANGLE, SG_RANGE);
    float rng = S.range, size = S.size;
    int cand[MAXE], nc = 0;
    for (int j = 0; j < MAXE; j++)
        if (E[j].alive && E[j].summoned != 2 && vlen(vsub(E[j].pos, o)) < rng && los(o, E[j].pos)) cand[nc++] = j;
    for (int a = 1; a < nc; a++) {                        /* les plus proches d'abord (tri par insertion) */
        int j = cand[a], b = a;
        while (b > 0 && vlen(vsub(E[cand[b - 1]].pos, o)) > vlen(vsub(E[j].pos, o))) { cand[b] = cand[b - 1]; b--; }
        cand[b] = j;
    }
    if (nc > PIE_PELLET_TARGETS) nc = PIE_PELLET_TARGETS;
    V3 dir[SG_PELLETS];
    if (nc == 0) sg_pellet_dirs(view_dir(), 0.3f, sg_new_seed(), dir);   /* personne en vue : eventail de 17 deg devant moi */
    else for (int k = 0; k < SG_PELLETS; k++) dir[k] = vnorm(vadd(vnorm(vsub(E[cand[k % nc]].pos, o)), vmul(rand_dir(), 0.06f)));
    float acc[MAXE];
    V3 kn[MAXE], col = sg_tint(0.5f);
    memset(acc, 0, sizeof acc); memset(kn, 0, sizeof kn);
    for (int k = 0; k < SG_PELLETS; k++) {
        V3 nrm;
        float tw = ray_world(o, dir[k], rng, &nrm), t;
        int te = -1;
        for (int j = 0; j < MAXE; j++)
            if (E[j].alive && E[j].summoned != 2 && ray_sphere(o, dir[k], E[j].pos, enemy_radius(E[j].type) * size, tw, &t)) { tw = t; te = j; }
        if (te >= 0) { acc[te] += SG_PELLET_DMG * sg_fall(&S, tw); kn[te] = vadd(kn[te], vmul(dir[k], SG_PELLET_KNOCK)); }
        add_tracer(o, vadd(o, vmul(dir[k], tw)), col);
    }
    burst(o, v3(1, 0.82f, 0.3f), 30, 8, 0.4f, 0.12f);
    play(S_SG_FIRE, 0.5f, 1.4f);
    for (int j = 0; j < MAXE; j++)
        if (acc[j] > 0) { burst(E[j].pos, v3(1, 0.45f, 0.2f), 10, 6, 0.3f, 0.1f); fx_damage(j, acc[j], kn[j], WP_SHOTGUN); }
}
static void cp_coin_pop(V3 o) {                           /* PIECE PIEGEE : la piece explose (3 m) quand un tir la ricoche */
    cp_coin_pellets(o);                                   /* capstone PIE>SHOTGUN : et en plombs */
    if (!cp_has(CP_PIEGEE)) return;
    cpStat[CP_PIEGEE]++;
    fx_blast(o, 3.0f, 45 * cp_sc(CP_PIEGEE), UP_SKILL(SK_COIN));
}
static void cp_coin_rocket(V3 o) {                        /* PIECE EXPLOSIVE : explose quand une roquette la traverse (3 m) */
    cp_coin_pellets(o);                                   /* capstone PIE>SHOTGUN : la roquette ricoche aussi */
    if (!cp_cap(CP_K_PIE_ROQ)) return;
    cpStat[CP_K_PIE_ROQ]++;
    fx_blast(o, 3.0f, 45, UP_SKILL(SK_COIN));
}
static void cp_ric_after(int te, V3 o, float dmg) {       /* le ricochet vient de toucher te */
    if (!vagOn) return;
    cpRicN++;
    if (cp_has(CP_REBOND)) {                              /* REBOND DE PLUS : +1 cible */
        int j = fx_nearest(o, cp_seek_range(), te, -1);
        if (j >= 0) {
            cpStat[CP_REBOND]++;
            add_beam(o, E[j].pos, v3(1, 0.85f, 0.3f), 0.6f);
            burst(E[j].pos, v3(1, 0.9f, 0.5f), 15, 7, 0.3f, 0.12f);
            vgCoinCtx++; damage_enemy(j, dmg, vmul(vnorm(vsub(E[j].pos, o)), 10)); vgCoinCtx--;
            cpRicN++;
        }
    }
    if (!E[te].alive && cp_has(CP_RECYCLEE)) {            /* PIECE RECYCLEE : un ricochet qui tue rend 1 charge */
        cpStat[CP_RECYCLEE]++;
        P.skillCh = fminf((float)cp_max(SK_COIN, skillDef[SK_COIN].max), P.skillCh + 1);
    }
}
static void cp_after_ricochet(void) {                     /* DIVIDENDE : un tir qui touche 2 cibles par la piece rend 10 PV */
    if (!vagOn) return;
    if (cp_has(CP_DIVIDENDE)) {
        int n = cpRicN;
        for (int i = 0; i < MAXE; i++) if (ricHitE[i] > 0) n++;   /* cibles deja traversees par le tir avant la piece */
        if (cpRicN > 0 && n >= 2 && !P.dead) {
            cpStat[CP_DIVIDENDE]++;
            vg_heal(10 * cp_sc(CP_DIVIDENDE));
            snprintf(popText, sizeof popText, "DIVIDENDE +%d", (int)(10 * cp_sc(CP_DIVIDENDE) + 0.5f)); popT = 0.8f;
        }
    }
    cpRicN = 0;
}

/* --- ALL IN ONE --- */
static float cp_half_deg(float base) {
    if (!vagOn) return base;
    if (cp_has(CP_A_LARGE)) base += 0.3f * cp_sc(CP_A_LARGE);   /* SALVE LARGE : demi-angle +0,3 deg */
    if (cp_has(CP_A_PRECISE)) base *= fmaxf(0.1f, 1 - 0.3f * cp_sc(CP_A_PRECISE));   /* SALVE PRECISE : dispersion -30 % */
    return base;
}
/* SALVE EXPLOSIVE (AIO>ROQUETTES) : chaque touche de la salve fait un souffle aux degats et au rayon de celui du Shotgun */
static void cp_aio_roq_hit(int i) {
    if (!vagOn || vgDepth != 0 || vgAllinT <= 0 || P.skill != SK_ALLIN || !cp_cap(CP_K_AIO_ROQ) || !E[i].alive) return;
    cpStat[CP_K_AIO_ROQ]++;
    fx_blast(E[i].pos, SG_BURST_RADIUS, SG_BURST_DMG, UP_SKILL(SK_ALLIN));
}
/* SALVE DE SHOTGUN (capstone AIO>SHOTGUN) : pendant la salve avec le Shotgun en main, chaque salve de plombs a un cone x3 et +50 % de
   plombs (fire_shotgun : half = demi-angle du cone, nP = nombre de plombs, plafonne a SG_PCAP) */
static void cp_aio_sho_fire(float *half, int *nP) {
    if (!vagOn || !allinActive || P.weapon != WP_SHOTGUN || !cp_cap(CP_K_AIO_SHO)) return;
    cpStat[CP_K_AIO_SHO]++;
    *half *= 3;
    *nP += *nP / 2;
    if (*nP > SG_PCAP) *nP = SG_PCAP;
}
static int cp_allin_extra(int w, int n) {
    if (!vagOn) return 0;
    int x = 0;
    if (cp_has(CP_A_RESERVE)) x++;                        /* RESERVE : +1 munition comptee */
    if (cp_has(CP_A_COMPLETE) && P.ammo[w] >= wp_max(w)) x++;   /* SALVE COMPLETE : chargeur plein, +1 tir */
    if (w == WP_RAIL && cp_cap(CP_K_AIO_RAIL)) { x += n; cpStat[CP_K_AIO_RAIL]++; }   /* SALVE RAIL : 2 rails par munition */
    if (w == WP_SHOTGUN) x += vgsg_allin_extra(n + x);    /* SHOTGUN TOTAL (SHO>AIO) : 6 salves au moins, sans munition en plus */
    return x;
}
static void cp_salve_begin(int w, int n) {
    (void)n;
    if (!vagOn) return;
    cpSalveW = w; cpSalveT = 2.0f; cpSalveId++; cpSalveFull = P.ammo[w] >= wp_max(w);
    if (cpSalveFull && cp_has(CP_A_DERNIER) && !P.dead) {   /* DERNIER SOUFFLE : salve complete (chargeur plein), +15 PV */
        vg_heal(15 * cp_sc(CP_A_DERNIER)); cpStat[CP_A_DERNIER]++;
    }
    vgAllinT = fmaxf(vgAllinT, 1.5f);                     /* les touches de la salve vont au chemin ALL IN ONE des cet instant */
    if (cp_cap(CP_K_AIO_LAN)) {                           /* SALVE DE LANCE : 5 estocs en eventail */
        V3 eye = eye_pos(), f = view_dir();
        cpStat[CP_K_AIO_LAN]++;
        for (int k = 0; k < 5; k++) {
            V3 d = cp_rot_y(f, (k - 2) * 0.22f);
            add_beam(vadd(eye, vmul(d, 0.8f)), vadd(eye, vmul(d, 3.8f)), v3(0.85f, 0.9f, 1), 0.25f);
            for (int j = 0; j < MAXE; j++) {
                if (!E[j].alive) continue;
                V3 v = vsub(E[j].pos, eye);
                float l = vlen(v);
                if (l - enemy_radius(E[j].type) > LANCE_THRUST_RANGE || vdot(vmul(v, 1 / fmaxf(l, 0.01f)), d) < 0.93f) continue;
                fx_damage(j, LANCE_THRUST_DMG, vmul(d, 6), UP_MELEE(MK_LANCE));
            }
        }
    }
}
static int cp_salve_refund(int w) { (void)w; return 0; }  /* la salve vide l'arme (la jauge rendue arrive dans cp_salve_end) */
static float cp_salve_cd(float cd) { return vagOn && cp_has(CP_A_CADENCE) ? cd * fmaxf(0.1f, 1 - 0.3f * cp_sc(CP_A_CADENCE)) : cd; }   /* CADENCE : delai -30 % */
static void cp_salve_end(int w, int n) {
    if (!vagOn) return;
    if (cp_has(CP_A_JAUGE)) {                             /* RECHARGEMENT JAUGE : rend 25 % de la jauge */
        cpStat[CP_A_JAUGE]++;
        gauge_add(w, 0.25f * cp_sc(CP_A_JAUGE) * (float)wp_max(w) * ammoDef[w].gauge);
    }
    if (cp_has(CP_A_DOUBLE)) { cpDblN = n; cpDblW = w; cpDblT = 0.3f; }   /* SALVE DOUBLE : la 2e part 0,3 s plus tard */
}
static float cp_dmg_mul(int i) {
    if (!vagOn || cpSalveT <= 0 || P.skill != SK_ALLIN || vgSrc != cpSalveW || i < 0 || i >= MAXE) return 1;
    float m = 1;
    if (cpSalveFull && cp_has(CP_A_COMPLETE)) m *= 1 + 0.3f * cp_sc(CP_A_COMPLETE);   /* SALVE COMPLETE : +30 % */
    if (cp_has(CP_A_UNIQUE)) {                            /* CIBLE UNIQUE : les tirs qui se concentrent sur une cible x1,3 */
        if (cpFocus[i] == cpSalveId) { m *= 1 + 0.3f * cp_sc(CP_A_UNIQUE); cpStat[CP_A_UNIQUE]++; }
        cpFocus[i] = cpSalveId;
    }
    return m;
}
static void cp_on_kill(int i) {
    (void)i;
    if (cpSalveT > 0 && P.skill == SK_ALLIN && cp_has(CP_A_PRIME)) {   /* PRIME DE SALVE : un kill pendant la salve, -2 s de recharge */
        cpStat[CP_A_PRIME]++;
        P.skillCh = fminf((float)cp_max(SK_ALLIN, skillDef[SK_ALLIN].max),
                          P.skillCh + 2.0f * cp_sc(CP_A_PRIME) / cp_recharge(SK_ALLIN, skillDef[SK_ALLIN].recharge));
    }
}

/* --- charges et recharge (recalculees a l'usage : skillDef n'est jamais modifie) --- */
static int cp_max(int sk, int base) {
    if (vagOn && sk == SK_COIN && cp_has(CP_RESERVE)) return base + 1;   /* RESERVE : 2 charges -> 3 */
    return base;
}
static float cp_recharge(int sk, float base) {
    if (!vagOn) return base;
    if (sk == SK_COIN && cp_has(CP_RECHARGE)) base *= fmaxf(0.1f, 1 - 0.1f * cp_sc(CP_RECHARGE));             /* RECHARGE : -10 % */
    if (sk == SK_ALLIN && cp_has(CP_A_RECHARGEMENT)) base *= fmaxf(0.1f, 1 - 0.15f * cp_sc(CP_A_RECHARGEMENT));  /* RECHARGEMENT : 20 s -> 17 s */
    return base;
}

/* --- tick : pieces (aimant doux, paratonnerre, piece lancee), 2e salve --- */
static void cp_tick(float dt) {
    if (!vagOn) return;
    if (cpSalveT > 0) cpSalveT -= dt;
    if (cpDblN > 0 && (cpDblT -= dt) <= 0) {              /* SALVE DOUBLE : la 2e salve, munition fictive restituee ensuite */
        int w = cpDblW, n = cpDblN, save = P.ammo[w], s0 = vgSrc;
        cpDblN = 0;
        if (!P.dead && P.weapon == w && P.switchT <= 0) {
            cpStat[CP_A_DOUBLE]++;
            vgSrc = w; allinActive = 1;
            allin_volley(w, n);
            allinActive = 0; vgSrc = s0;
            P.ammo[w] = save;
            cpSalveT = 2.0f;
        }
    }
    int soft = cp_has(CP_AIMANT_DOUX), para = cp_has(CP_PARATONNERRE), pierce = cp_cap(CP_K_PIE_LAN);
    if (!soft && !para && !pierce) return;
    for (int k = 0; k < MAXCOIN; k++) {
        Coin *c = &CO[k];
        if (!c->alive || c->owner != myId || c->decoy) continue;
        if (soft) {                                       /* AIMANT DOUX : la piece derive vers l'ennemi, plus vite */
            int t = fx_nearest(c->pos, 14, -1, -1);
            if (t >= 0) c->vel = vlerp(c->vel, vmul(vnorm(vsub(E[t].pos, c->pos)), 12 * cp_sc(CP_AIMANT_DOUX)), fminf(1, dt * 5));
        }
        if (para)                                         /* PARATONNERRE : les orbes ennemies a 6 m vont vers la piece et s'y eteignent */
            for (int q = 0; q < MAXPR; q++) {
                Proj *p = &PR[q];
                if (!p->alive || p->kind != PR_ORB) continue;
                V3 d = vsub(c->pos, p->pos);
                float l = vlen(d);
                if (l > 6 * cp_sc(CP_PARATONNERRE)) continue;
                if (l < 1.0f) {
                    p->alive = 0; cpStat[CP_PARATONNERRE]++;
                    burst(c->pos, v3(1, 0.85f, 0.3f), 14, 6, 0.3f, 0.1f);
                    play(S_PARRY, 0.4f, 1.8f);
                } else p->vel = vmul(vmul(d, 1 / l), fmaxf(vlen(p->vel), 12.0f));
            }
        if (pierce)                                       /* PIECE LANCEE : la piece transperce les ennemis qu'elle croise */
            for (int j = 0; j < MAXE; j++) {
                if (!E[j].alive || ((cpPierce[k][j >> 5] >> (j & 31)) & 1u)) continue;
                if (vlen(vsub(E[j].pos, c->pos)) > enemy_radius(E[j].type) + 1.0f) continue;
                cpPierce[k][j >> 5] |= 1u << (j & 31);
                cpStat[CP_K_PIE_LAN]++;
                burst(E[j].pos, v3(1, 0.85f, 0.3f), 12, 6, 0.3f, 0.1f);
                fx_damage(j, LANCE_THRUST_DMG, vmul(vnorm(c->vel), 6), UP_SKILL(SK_COIN));
            }
    }
}
