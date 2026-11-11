/* VELOCITY - boutique du mode VAGUES : effets de la graine SHOTGUN qui ne sont PAS des stats (genre BK_SHO).
   Morceau de main.c : inclus apres ameliorations.c, ne se compile pas seul.
   Les 14 etapes qui sont des stats (Gros plomb, Pompe vive, Plombs en plus, Pompe tendue...) sont des lignes de donnees du sac de stats
   (docs/boutique_vagues/outils/effets/shotgun.js, boutique_stats.c) lues par sg_stats dans src/armes/shotgun.c : rien ici.
   Restent ici les EVENEMENTS et COMPORTEMENTS (Rafale, Peau de plomb, Souffle de culasse, Coup double) et les capstones : chacun est une entree
   [BK_SHO, id, b] dans effets/shotgun.js ; id = SHE_*, b = parametre multiplie par l'echelle du niveau (vgsg_get).
   Tout agit uniquement si vagOn et si le Shotgun est equipe. fire_shotgun (src/armes/shotgun.c) appelle les vgsg_* ci-dessous. */

enum { SHE_NONE, SHE_RAFALE, SHE_PEAU, SHE_CULASSE, SHE_COUP2,
       SHE_CAP_ROQ = 20, SHE_CAP_LAN, SHE_CAP_RAIL, SHE_CAP_IMP, SHE_CAP_PIE, SHE_CAP_AIO };

/* valeur b de l'effet id s'il est possede (0 sinon) x echelle du niveau de l'etape (b = 1 : l'echelle elle-meme),
   0 hors VAGUES ou sans Shotgun equipe */
static float vgsg_get(int id) {
    if (!vagOn || !up_equipped(WP_SHOTGUN)) return 0;
    float r = 0;
    for (int k = 0; k < 20; k++)
        if (sh_owns(SH_SHO, k) && shSteps[SH_SHO][k].kind == BK_SHO && shSteps[SH_SHO][k].a == id) {
            float v = (float)shSteps[SH_SHO][k].b * sh_scale(SH_SHO, k);
            if (v > r) r = v;
        }
    return r;
}
/* capstone SHO>partenaire achete et effet id (SHE_CAP_*), les deux graines equipees */
static int vgsg_cap(int partner, int id) {
    if (!vagOn || !sh_on(SH_SHO) || !sh_on(partner)) return 0;
    int b = sh_branch(SH_SHO, partner);
    return b >= 0 && shCap[b] && shBr[b].capKind == BK_SHO && shBr[b].capA == id;
}

/* capstone SHO>LAN (Shotgun de joute) : lu une fois par tir par sg_stats (src/armes/shotgun.c), qui applique le x1,4 sous 3,5 m dans sg_fall */
static int vgsg_cap_near(void) { return vgsg_cap(SH_LAN, SHE_CAP_LAN); }

/* capstone ALL IN ONE (Shotgun total) : l'All in One au Shotgun tire 6 salves au moins (jamais moins qu'avec RESERVE et SALVE
   COMPLETE). La munition depensee ne change pas : toujours toute celle qu'on tient, mise a zero comme pour toute salve All in One.
   Les salves au-dela sont comptees (comme RESERVE, cp_allin_extra), jamais prises sur la reserve, et DOUBLE CANON ne les rembourse pas. */
#define SHO_TOTAL_SALVES 6
static int vgsgReal;                                      /* salves payees de la salve All in One en cours (0 : pas de salve comptee en plus) */
static int vgsg_allin_extra(int n) {                      /* n : salves deja comptees ; renvoie celles a ajouter */
    vgsgReal = 0;
    if (!vgsg_cap(SH_AIO, SHE_CAP_AIO) || n >= SHO_TOTAL_SALVES) return 0;
    vgsgReal = n;
    return SHO_TOTAL_SALVES - n;
}

static float sgFreeMul = 1;                               /* degats de la salve gratuite en cours */
static float vgsg_salvo_mul(void) { return sgFree ? sgFreeMul : 1; }
static void vgsg_refund(float cost) {                     /* DOUBLE CANON : cost = stat COUT (x0,85) lue par sg_stats ; la part du cout de jauge qui manque est rendue */
    if (allinActive && vgsgReal > 0 && P.ammo[WP_SHOTGUN] >= vgsgReal) return;   /* salve comptee en plus (SHO>AIO) : rien de depense, rien de rendu */
    if (cost < 1) gauge_add(WP_SHOTGUN, ammoDef[WP_SHOTGUN].gauge * (1 - cost));
}
#define SHO_AMORCAGE_PTS 5.0f                            /* points de jauge du rail par plomb qui touche (le rail : 250 points par munition) */
/* capstone PIECES : la salve lance une piece par plomb (meme eventail), sans charge de pieces et sans plomb */
static int vgsg_coin_mode(void) { return vgsg_cap(SH_PIE, SHE_CAP_PIE); }
static void vgsg_coins(const V3 *dir, int n) {
    for (int k = 0; k < n; k++) throw_coin_one(dir[k], 0);
}
static void vgsg_pellet_hit(V3 at, int enemy) {           /* capstone ROQUETTES : tout plomb qui touche explose 2 m */
    (void)enemy;
    if (vgsg_cap(SH_ROQ, SHE_CAP_ROQ)) fx_blast(at, 2, 12, WP_SHOTGUN);
    if (vgsg_cap(SH_RAIL, SHE_CAP_RAIL)) gauge_add(WP_RAIL, SHO_AMORCAGE_PTS);   /* capstone RAILGUN : la jauge de recharge du rail, pas son delai */
}

/* salves gratuites en attente (RAFALE, COUP DOUBLE) */
typedef struct { float t, mul; } SgPend;
static SgPend sgPend[4];
static int vgsgFreeShots;                                 /* salves gratuites tirees (tests) */
static void vgsg_queue(float t, float mul) {
    for (int k = 0; k < 4; k++) if (sgPend[k].t <= 0) { sgPend[k].t = t; sgPend[k].mul = mul; return; }
}
static void vgsg_after(V3 eye, V3 d, int pellets, int kills) {
    if (pellets >= 6 && vgsg_get(SHE_PEAU)) vg_heal((float)vgsg_get(SHE_PEAU));   /* PEAU DE PLOMB */
    if (sgFree) return;
    if (sgLastPush > 0 && vgsg_cap(SH_IMP, SHE_CAP_IMP)) P.airJumps = 1;   /* capstone IMPULSION : le recul rend le double saut */
    if (vgsg_get(SHE_CULASSE))                            /* SOUFFLE DE CULASSE : onde derriere soi */
        fx_pulse(vadd(eye, vmul(d, -2.0f)), 3, 40 * vgsg_get(SHE_CULASSE), 8, WP_SHOTGUN);   /* degats x niveau (b = 1) */
    if (vgsg_get(SHE_RAFALE)) vgsg_queue(0.15f, vgsg_get(SHE_RAFALE) * 0.01f);
    if (kills > 0 && vgsg_get(SHE_COUP2)) vgsg_queue(0.12f, vgsg_get(SHE_COUP2) * 0.01f);
}
static void vgsg_fire_free(float mul) {
    int s0 = vgSrc;
    sgFree = 1; sgFreeMul = mul; vgSrc = WP_SHOTGUN;
    fire_shotgun();
    sgFree = 0; sgFreeMul = 1; vgSrc = s0; vgsgFreeShots++;
}
static void vgsg_tick(float dt) {
    for (int k = 0; k < 4; k++) {
        if (sgPend[k].t <= 0) continue;
        sgPend[k].t -= dt;
        if (sgPend[k].t > 0) continue;
        if (vagOn && !P.dead && P.weapon == WP_SHOTGUN) vgsg_fire_free(sgPend[k].mul);
        sgPend[k].t = 0;
    }
}
