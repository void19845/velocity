/* VELOCITY - boutique du mode VAGUES : FORMES FUSIONNEES (armes hybrides)
   Morceau de main.c : inclus apres boutique_rail.c, ne se compile pas seul (voir main.c).

   Acheter la FUSION d'une paire d'armes a distance (shFusion, une seule par partie) donne une jauge de fusion, remplie par les
   degats des deux armes (hy_charge, appele par vg_on_hit). Pleine, la touche A_FUSION active l'arme hybride HY_TIME secondes :
   elle remplace les deux armes (leurs munitions et jauges sont mises de cote puis rendues), herite des modificateurs des deux
   arbres stat par stat, puis les deux armes reviennent. La melee et la competence restent disponibles.
   La forme n'agit que si vagOn, si la fusion est achetee ET si les deux armes sont equipees ; elle prend fin si l'une des deux
   part (CHANGER D'ARME), a la mort, ou a la fin du temps. Rien n'est envoye sur le reseau (VAGUES est solo).
   Donnees (nom, texte) : shHy[] dans boutique_donnees.c (genere par outils/export_c.js depuis arbre_v2.json).

   Niveau de la fusion (sh_fusion_scale : 100 / 125 / 150 %) : multiplie les degats propres de la forme (souffles de l'ARTILLERIE,
   rails du CANON, mini-roquettes de l'OBUSIER), comme les autres fusions.
   Heritage stat par stat :
   - ARTILLERIE ORBITALE : un vrai tir de rail (fire_rail : tout l'arbre du Railgun s'applique) + des souffles de roquette
     (degats ROCKET_DMG, arbre des Roquettes : degats par vgSrc, zone par hy_zone_roq) a chaque cible traversee (3 m) et au
     bout de la course (8 m) ;
   - CANON SCIE : 8 + (plombs en plus du Shotgun) rails fins en gerbe (cone du Shotgun avec ses modificateurs d'ouverture et de
     recherche), chacun un tir perforant du rail a 50 % (arbre du Railgun par vgSrc, degats du Shotgun par hy_tree_dmg), portee
     120 m + portee du Shotgun, recul du Shotgun ;
   - OBUSIER : 6 + (plombs en plus) mini-roquettes en gerbe, chacune une roquette roqGen (souffle 0,55 x le rayon de la roquette,
     soit 2,5 m de base, vitesse / taille / carburant de roq_setup), degats de l'arbre du Shotgun en plus, recul vers le haut. */

#define HY_TIME        15.0f                              /* duree de l'arme hybride (s) */
#define HY_GAUGE_MAX   1500.0f                            /* degats des deux armes pour remplir la jauge */
#define HY_ART_CD      1.1f                               /* delai entre deux tirs (s) */
#define HY_CAN_CD      0.9f
#define HY_OBU_CD      1.1f
#define HY_CAN_RAYS    8
#define HY_CAN_HALF    (3.0f * PI / 180.0f)               /* cone du CANON SCIE : 6 deg au total (comme SG_HALF_ANGLE : demi-angle) */
#define HY_CAN_MULT    0.5f                               /* un rail fin = 50 % du rail */
#define HY_CAN_RANGE   120.0f
#define HY_OBU_ROCKETS 6
#define HY_OBU_HALF    (4.0f * PI / 180.0f)               /* cone de l'OBUSIER : 8 deg au total */
#define HY_OBU_MULT    0.5f                               /* degats d'une mini-roquette (x ROCKET_DMG) */
#define HY_OBU_KICK    16.0f                              /* recul de l'OBUSIER : m/s a l'oppose de la visee, au moins 10 m/s vers le haut */

static const char *bind_text(int a);                      /* src/interface/config.c */
static void text(int f, float x, float y, const char *s, float r, float g, float b, float a);   /* src/interface/texte.c */
static void text_c(int f, float cx, float y, const char *s, float r, float g, float b, float a);
static void rect(float x, float y, float w, float h, float r, float g, float b, float a);

static float hyGauge, hyT;                                /* jauge de fusion (0..HY_GAUGE_MAX), temps restant de l'arme hybride */
static int hyOn, hyAct = -1, hyFull;                      /* active, forme activee (index de shHy), message "pleine" deja donne */
static int hyWA, hyWB, hySaveAmmo[2];                     /* armes remplacees et leurs munitions mises de cote */
static float hySaveGauge[2];
static int hyStat[6];                                     /* tests : 0 tirs, 1 rails du canon, 2 souffles d'artillerie, 3 mini-roquettes, 4 activations, 5 fins */

static void hy_reset(void) {                              /* nouvelle partie : tout a zero (les munitions sont remises par reset_game) */
    hyGauge = hyT = 0; hyOn = 0; hyAct = -1; hyFull = 0;
    memset(hyStat, 0, sizeof hyStat);
}

/* forme fusionnee de la fusion achetee (index de shHy), -1 si aucune ; hy_usable : en plus, les deux armes sont equipees */
static int hy_of_fusion(int fu) {                         /* forme fusionnee (codee) de la fusion fu de shFu[], -1 si aucune */
    if (fu < 0 || fu >= SH_NFU) return -1;
    for (int i = 0; i < SH_NHY; i++)
        if (shHy[i].id != HY_NONE && shHy[i].a == shFu[fu].a && shHy[i].b == shFu[fu].b) return i;
    return -1;
}
static int hy_find(void) { return vagOn ? hy_of_fusion(shFusion) : -1; }
static int hy_usable(void) {
    int i = hy_find();
    return i >= 0 && sh_on(shHy[i].a) && sh_on(shHy[i].b) ? i : -1;
}
static int hy_active_id(void) { return hyOn && hyAct >= 0 ? shHy[hyAct].id : HY_NONE; }

/* degats cumules (stat DEG du sac, sans condition) d'un arbre : pour heriter des degats d'une famille dont le chemin n'est pas celui de vgSrc */
static float hy_tree_dmg(int seed) { return st(seed, STAT_DEG, 1.0f); }
/* zone des Roquettes par rapport a la base (RAYON, GRAND SOUFFLE...) ; sans le capstone MINI-NUKE, qui est un cas a part */
static float hy_zone_roq(void) {
    Proj q;
    memset(&q, 0, sizeof q);
    q.roqGen = 1;
    return roq_radius(&q) / (0.55f * ROCKET_RADIUS);
}

/* jauge de fusion : chaque degat d'une des deux armes de la paire (hors forme active) */
static void hy_charge(float dmg) {
    if (!vagOn || hyOn || dmg <= 0 || vgSrc < 0) return;
    int i = hy_usable();
    if (i < 0 || (vgSrc != shSeedUp[shHy[i].a] && vgSrc != shSeedUp[shHy[i].b])) return;
    hyGauge = fminf(HY_GAUGE_MAX, hyGauge + dmg);
    if (hyGauge >= HY_GAUGE_MAX && !hyFull) {
        hyFull = 1;
        snprintf(popText, sizeof popText, "FUSION PRETE [%s]", bind_text(A_FUSION)); popT = 2.5f;
        play(S_PARRY, 0.7f, 1.5f);
    }
}

/* munitions : pendant la forme, les deux armes sont pleines (l'hybride ne les consomme pas) ; mises de cote a l'activation */
static void hy_peg(void) {
    P.ammo[hyWA] = wp_max(hyWA); P.ammo[hyWB] = wp_max(hyWB);
}
static void hy_start(int i) {
    hyWA = shSeedUp[shHy[i].a]; hyWB = shSeedUp[shHy[i].b];
    hySaveAmmo[0] = P.ammo[hyWA]; hySaveAmmo[1] = P.ammo[hyWB];
    hySaveGauge[0] = P.ammoGauge[hyWA]; hySaveGauge[1] = P.ammoGauge[hyWB];
    hyOn = 1; hyAct = i; hyT = HY_TIME; hyGauge = 0; hyFull = 0;
    hy_peg();
    P.fireCd = 0;
    hyStat[4]++;
    snprintf(popText, sizeof popText, "%s !", shHy[i].name); popT = 2.0f;
    play(S_PARRY, 0.8f, 0.8f);
    burst(pcenter(), v3(1, 0.55f, 0.95f), 40, 8, 0.4f, 0.12f);
}
static void hy_end(void) {                                /* les deux armes reviennent avec leurs munitions d'avant */
    if (!hyOn) return;
    hyOn = 0; hyT = 0; hyStat[5]++;
    int w[2] = { hyWA, hyWB };
    for (int k = 0; k < 2; k++) {
        P.ammo[w[k]] = hySaveAmmo[k] < wp_max(w[k]) ? hySaveAmmo[k] : wp_max(w[k]);
        P.ammoGauge[w[k]] = hySaveGauge[k];
    }
    if (!P.dead) { snprintf(popText, sizeof popText, "FUSION TERMINEE"); popT = 1.2f; }
    hyAct = -1;
}

/* un tick : touche d'activation, duree, fin si une arme part (la jauge reste : elle ne se remplit que paire equipee) */
static void hy_tick(float dt) {
    if (!vagOn) { if (hyOn) hy_end(); return; }
    int i = hy_usable();
    if (hyOn) {
        hyT -= dt;
        if (hyT <= 0 || P.dead || i != hyAct) { hy_end(); return; }
        hy_peg();
        return;
    }
    if (i < 0 || !actP[A_FUSION] || P.dead) return;
    if (hyGauge >= HY_GAUGE_MAX) hy_start(i);
    else { snprintf(popText, sizeof popText, "FUSION : %d %%", (int)(100 * hyGauge / HY_GAUGE_MAX)); popT = 1.0f; }
}

/* --- tirs --- */
static void hy_blast(V3 c, float r) {                     /* souffle de la roquette, sans recul ni degat sur soi */
    hyStat[2]++;
    fx_blast(c, r, ROCKET_DMG * sh_fusion_scale(), WP_ROCKET);   /* niveau de la fusion : 100 / 125 / 150 % */
}
static void hy_artillerie(void) {
    V3 o = eye_pos(), d = vg_rail_bend(o, view_dir());
    float wall = ray_world(o, d, RIC_SEG_LEN, 0), wide = vg_rail_wide(W_RAIL);
    V3 at[MAXE];
    int n = 0;
    for (int i = 0; i < MAXE; i++) {                      /* cibles traversees : relevees avant le tir (les morts ne bougent plus) */
        float t;
        if (E[i].alive && E[i].summoned != 2 && ray_sphere(o, d, E[i].pos, enemy_radius(E[i].type) * wide, wall, &t)) at[n++] = E[i].pos;
    }
    vgSrc = WP_RAIL;
    fire_rail();                                          /* le trait : degats, chaine et etapes du Railgun */
    float z = hy_zone_roq();
    vgSrc = WP_ROCKET;
    for (int k = 0; k < n; k++) hy_blast(at[k], 3.0f * z);
    hy_blast(vadd(o, vmul(d, wall)), 8.0f * z);           /* au bout de la course */
    P.fireCd = fmaxf(P.fireCd, HY_ART_CD / streak_rate());
}

/* gerbe : un rayon au centre, les autres sur un anneau (motif tourne par une graine locale, comme le Shotgun) */
static void hy_dirs(V3 d, float half, int n, V3 *dir) {
    unsigned int s = sg_new_seed();
    V3 r = vnorm(vcross(d, v3(0, 1, 0.01f))), u = vcross(r, d);
    float th = tanf(half), rot = sg_rand(&s) * 2 * PI;
    dir[0] = d;
    for (int k = 1; k < n; k++) {
        float ang = rot + (float)(k - 1) * (2 * PI / (float)(n - 1)), rho = 0.85f + (sg_rand(&s) - 0.5f) * 0.2f;
        dir[k] = vnorm(vadd(d, vadd(vmul(r, cosf(ang) * rho * th), vmul(u, sinf(ang) * rho * th))));
    }
}
static void hy_canon(void) {
    V3 eye = eye_pos(), d = view_dir(), dir[SG_PMAX];
    SgStats S;                                            /* cone, plombs en plus, portee et recherche du Shotgun : sac de stats, lu une fois */
    sg_stats(&S, HY_CAN_HALF, HY_CAN_RANGE);
    int n = HY_CAN_RAYS + S.extra;
    if (n > SG_PMAX) n = SG_PMAX;
    hy_dirs(d, S.half, n, dir);
    sg_seek(eye, d, dir, n, S.seek);
    float range = S.range;
    float dmg = RAIL_DMG * HY_CAN_MULT * streak_mult() * hy_tree_dmg(SH_SHO) * sh_fusion_scale();   /* l'arbre du Railgun s'applique par vgSrc */
    int hit = 0;
    vgSrc = WP_RAIL;
    for (int k = 0; k < n; k++) {
        V3 e, nn;
        int stop;
        memset(ricHitE, 0, sizeof ricHitE); memset(ricHitP, 0, sizeof ricHitP);   /* chaque rayon peut toucher chaque cible */
        hit |= hitscan_seg(eye, dir[k], range, 0, dmg, 6, 3, W_RAIL, &e, &nn, &stop);
        add_beam(muzzle(), e, v3(0.5f, 0.85f, 1), 0.35f);
        hyStat[1]++;
    }
    shot_result(hit);
    shotHeads = 0;
    float push = sg_push_speed(vlen(P.vel));              /* recul du Shotgun : proportionnel a la vitesse, au sol comme en l'air */
    sgLastPush = push;
    if (!P.grounded) sg_recoil(d);
    if (push > 0) P.vel = vadd(P.vel, vmul(d, -push));    /* vitesse actuelle ajoutee a l'oppose de la visee */
    P.fireCd = HY_CAN_CD / streak_rate();
    P.recoil = 1.6f; shake += 0.12f; muzzleT = 0.09f;
    play(S_RAIL, 0.7f, 1.2f); play(S_SG_FIRE, 0.7f, 1.1f);
    if (hit) { hitMarker = 0.18f; play(S_HIT, 0.6f, 1.0f); }
}
static void hy_obusier(void) {
    V3 eye = eye_pos(), d = view_dir(), dir[SG_PMAX];
    SgStats S;                                            /* cone, plombs en plus et recherche du Shotgun : sac de stats */
    sg_stats(&S, HY_OBU_HALF, SG_RANGE);
    int n = HY_OBU_ROCKETS + S.extra;
    if (n > SG_PMAX) n = SG_PMAX;
    hy_dirs(d, S.half, n, dir);
    sg_seek(eye, d, dir, n, S.seek);
    float mult = HY_OBU_MULT * hy_tree_dmg(SH_SHO) * sh_fusion_scale();
    for (int k = 0; k < n; k++) {
        Proj *q = spawn_proj(PR_ROCKET, eye, vmul(dir[k], ROCKET_SPEED));
        if (!q) break;
        q->roqGen = 1; q->mult = mult; q->life = 4;
        roq_setup(q);                                     /* vitesse, taille, carburant de l'arbre des Roquettes */
        hyStat[3]++;
    }
    V3 kick = vmul(d, -HY_OBU_KICK);                      /* le recul t'envoie en l'air */
    kick.y = fmaxf(kick.y, 10.0f);
    P.vel = vadd(P.vel, kick);
    P.grounded = 0;
    sgLastPush = vlen(kick);
    P.fireCd = HY_OBU_CD / streak_rate();
    P.recoil = 1.6f; shake += 0.14f; muzzleT = 0.09f;
    play(S_ROCKET, 0.8f, 0.8f); play(S_SG_FIRE, 0.7f, 0.9f);
}

/* remplace le tir de l'arme en main (fire_weapon) : 1 si l'hybride a tire */
static int hy_fire(void) {
    if (!vagOn || !hyOn || (P.weapon != hyWA && P.weapon != hyWB)) return 0;
    int s0 = vgSrc;
    hyStat[0]++;
    switch (shHy[hyAct].id) {
    case HY_ARTILLERIE: hy_artillerie(); break;
    case HY_CANON: hy_canon(); break;
    case HY_OBUSIER: hy_obusier(); break;
    default: vgSrc = s0; return 0;
    }
    vgSrc = s0;
    hy_peg();                                             /* l'hybride ne consomme pas les munitions des deux armes */
    return 1;
}

/* --- HUD : jauge de fusion (bas droite) et, pendant la forme, nom et temps restant --- */
static float hy_label_x(float x, float w, const char *s) {   /* centre du texte sur la jauge, sans sortir de l'ecran (police 0 : ~10 px par lettre) */
    return fminf(x + w * 0.5f, (float)winW - 12.0f - 5.0f * (float)strlen(s));
}
static void hy_draw_hud(float cx, float cy) {
    int i = hyOn ? hyAct : hy_usable();
    if (!vagOn || i < 0) return;
    char buf[96];
    float w = 150, x = winW - 30.0f - w, y = winH - 128.0f, pulse = 0.65f + 0.35f * sinf(gameTime * 10);
    if (hyOn) {
        float f = clampf(hyT / HY_TIME, 0, 1);
        rect(x, y, w, 8, 0.12f, 0.06f, 0.14f, 0.8f);
        rect(x, y, w * f, 8, 1.0f, 0.55f, 0.95f, 0.95f);
        snprintf(buf, sizeof buf, "%s  %.1f s", shHy[i].name, hyT);
        text_c(0, hy_label_x(x, w, buf), y - 16, buf, 1.0f, 0.65f, 0.95f, 0.95f);
        text_c(1, cx, cy + 96, shHy[i].name, 1.0f, 0.6f, 0.95f, pulse);
    } else {
        int full = hyGauge >= HY_GAUGE_MAX;
        float f = clampf(hyGauge / HY_GAUGE_MAX, 0, 1);
        rect(x, y, w, 8, 0.12f, 0.06f, 0.14f, 0.7f);
        rect(x, y, w * f, 8, 1.0f, 0.55f, 0.95f, full ? pulse : 0.55f);
        snprintf(buf, sizeof buf, full ? "FUSION PRETE [%s]" : "FUSION [%s]", bind_text(A_FUSION));
        text_c(0, hy_label_x(x, w, buf), y - 16, buf, 1.0f, 0.65f, 0.95f, full ? pulse : 0.6f);
        if (full) text_c(1, cx, cy + 96, shHy[i].name, 1.0f, 0.6f, 0.95f, pulse);
    }
}
