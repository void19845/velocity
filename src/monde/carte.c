/* VELOCITY - carte en boites (AABB), recharges, collisions, rayons
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Monde : une liste d'AABB                                            */
/* ------------------------------------------------------------------ */
enum { B_SOLID, B_PAD, B_SPEED };                        /* B_SPEED : bande d'acceleration (launch = sens x vitesse max) */
typedef struct { V3 mn, mx, col, launch; int kind; } Box;
#define MAXB 256
static Box boxes[MAXB];
static int nb;

static void addbox(float x0, float y0, float z0, float x1, float y1, float z1, float r, float g, float b) {
    if (nb >= MAXB) return;
    Box *bx = &boxes[nb++];
    bx->mn = v3(x0, y0, z0); bx->mx = v3(x1, y1, z1); bx->col = v3(r, g, b);
    bx->kind = B_SOLID; bx->launch = v3(0, 0, 0);
}

static void addpad(float x, float z, V3 launch) {
    addbox(x - 1.5f, 0, z - 1.5f, x + 1.5f, 0.3f, z + 1.5f, 0.2f, 0.9f, 1.0f);
    boxes[nb - 1].kind = B_PAD;
    boxes[nb - 1].launch = launch;
}

static int near2(float x, float z, float cx, float cz, float r) {
    return (x - cx) * (x - cx) + (z - cz) * (z - cz) < r * r;
}

/* La carte est dessinee a l'echelle 1 puis agrandie horizontalement (les hauteurs ne changent pas) */
#define MAP_S 1.4f
static float ms(float v) { return v * MAP_S; }

/* cellules de munitions posees sur la carte ; t > 0 : deja ramassee, revient dans t secondes.
 * pts : points donnes a la jauge de chacune des deux armes du loadout (PICKUP_SMALL ou PICKUP_BIG) */
#define MAXPICK 16
typedef struct { V3 pos; float t, pending; int pts; } Pickup;
static Pickup PU[MAXPICK];
static int npu;

static void addpickup(float x, float y, float z, int pts) {
    if (npu >= MAXPICK) return;
    PU[npu].pos = v3(ms(x), y + 0.9f, ms(z));
    PU[npu].t = PU[npu].pending = 0;
    PU[npu].pts = pts;
    npu++;
}

/* Arene : la carte d'origine, inchangee (run_tests depend du couloir x = -20 et des coordonnees des pads) */
static void build_map_arene(void) {
    float W = ms(56);
    nb = 0; npu = 0;
    addbox(-W, -2, -W, W, 0, W, 0.52f, 0.54f, 0.60f);              /* sol */
    addbox(-W - 2, 0, -W - 2, W + 2, 24, -W, 0.40f, 0.42f, 0.52f);   /* murs exterieurs */
    addbox(-W - 2, 0, W, W + 2, 24, W + 2, 0.40f, 0.42f, 0.52f);
    addbox(-W - 2, 0, -W, -W, 24, W, 0.40f, 0.42f, 0.52f);
    addbox(W, 0, -W, W + 2, 24, W, 0.40f, 0.42f, 0.52f);

    addbox(-ms(8), 0, -ms(8), ms(8), 4, ms(8), 0.78f, 0.56f, 0.40f);   /* plateforme centrale */
    for (int i = 0; i < 10; i++) {                                  /* escaliers N et S */
        float h = 0.4f * (i + 1), z0 = ms(8 + (9 - i) * 0.6f), z1 = ms(8 + (10 - i) * 0.6f);
        float c = 0.62f + 0.012f * i;
        addbox(-ms(3), 0, z0, ms(3), h, z1, c, 0.5f, 0.38f);
        addbox(-ms(3), 0, -z1, ms(3), h, -z0, c, 0.5f, 0.38f);
    }
    addbox(-ms(2), 4, -ms(2), ms(2), 13, ms(2), 0.55f, 0.55f, 0.66f);   /* tour centrale */

    for (int sx = -1; sx <= 1; sx += 2)                             /* piliers */
        for (int sz = -1; sz <= 1; sz += 2)
            addbox(ms(sx * 24 - 2.0f), 0, ms(sz * 24 - 2.0f), ms(sx * 24 + 2.0f), 16, ms(sz * 24 + 2.0f), 0.5f, 0.5f, 0.62f);

    addbox(ms(28), 6, -ms(8), ms(44), 7, ms(8), 0.30f, 0.62f, 0.64f);   /* plateformes hautes */
    addbox(-ms(44), 6, -ms(8), -ms(28), 7, ms(8), 0.30f, 0.62f, 0.64f);
    addbox(-ms(8), 6, ms(28), ms(8), 7, ms(44), 0.30f, 0.62f, 0.64f);
    addbox(-ms(8), 6, -ms(44), ms(8), 7, -ms(28), 0.30f, 0.62f, 0.64f);

    for (int sx = -1; sx <= 1; sx += 2)                             /* perchoirs, accessibles au grappin */
        for (int sz = -1; sz <= 1; sz += 2)
            addbox(sx * ms(36) - 4, 12, sz * ms(36) - 4, sx * ms(36) + 4, 13, sz * ms(36) + 4, 0.85f, 0.7f, 0.35f);

    addbox(ms(30), 0, ms(38), ms(50), 8, ms(38) + 1, 0.75f, 0.42f, 0.62f);   /* murs de wall-run */
    addbox(-ms(50), 0, -ms(38) - 1, -ms(30), 8, -ms(38), 0.75f, 0.42f, 0.62f);
    addbox(ms(38), 0, -ms(50), ms(38) + 1, 8, -ms(30), 0.75f, 0.42f, 0.62f);
    addbox(-ms(38) - 1, 0, ms(30), -ms(38), 8, ms(50), 0.75f, 0.42f, 0.62f);

    addpad(ms(16), 0, v3(ms(13), 20, 0));                           /* jump pads */
    addpad(-ms(16), 0, v3(-ms(13), 20, 0));
    addpad(ms(9), ms(16), v3(-ms(1.8f), 20, ms(13)));
    addpad(-ms(9), -ms(16), v3(ms(1.8f), 20, -ms(13)));

    /* cellules : grandes aux endroits exposes (tour, perchoirs), petites ailleurs ; l'ordre fixe les index reseau */
    addpickup(0, 13, 0, PICKUP_BIG);                                /* sommet de la tour, */
    addpickup(36, 7, 0, PICKUP_SMALL); addpickup(-36, 7, 0, PICKUP_SMALL);   /* plateformes hautes, */
    addpickup(0, 7, 36, PICKUP_SMALL); addpickup(0, 7, -36, PICKUP_SMALL);
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) {
            addpickup(sx * 36.0f, 13, sz * 36.0f, PICKUP_BIG);      /* perchoirs */
            addpickup(sx * 46.0f, 0, sz * 46.0f, PICKUP_SMALL);     /* et coins au sol */
        }

    rng = 777;                                                      /* blocs de couverture */
    for (int k = 0, placed = 0; k < 400 && placed < 34; k++) {
        float x = frange(-ms(50), ms(50)), z = frange(-ms(50), ms(50));
        float w = frange(1.5f, 3.2f), d = frange(1.5f, 3.2f), h = frange(1.0f, 3.5f);
        if (fabsf(x) < ms(13) && fabsf(z) < ms(18)) continue;
        if (near2(x, z, ms(16), 0, 8) || near2(x, z, -ms(16), 0, 8) || near2(x, z, ms(9), ms(16), 8) || near2(x, z, -ms(9), -ms(16), 8)) continue;
        if (near2(x, z, ms(24), ms(24), 9) || near2(x, z, -ms(24), ms(24), 9) || near2(x, z, ms(24), -ms(24), 9) || near2(x, z, -ms(24), -ms(24), 9)) continue;
        if (near2(x, z, 0, ms(24), 6)) continue;
        if (fabsf(fabsf(x) - ms(38.5f)) < 6 && fabsf(z) > ms(26)) continue;
        if (fabsf(fabsf(z) - ms(38.5f)) < 6 && fabsf(x) > ms(26)) continue;
        if (fabsf(fabsf(x) - ms(46)) < 4 && fabsf(fabsf(z) - ms(46)) < 4) continue;   /* recharges des coins */
        if (fabsf(x + 20) < 6 && z > 12 && z < 70) continue;        /* couloir degage pour les tests automatiques */
        float t = frand();
        addbox(x - w, 0, z - d, x + w, h, z + d, 0.45f + 0.2f * t, 0.48f, 0.58f - 0.12f * t);
        placed++;
    }
    rng = (unsigned)GetTickCount();
}

/* --- elements des nouvelles cartes (coordonnees du monde, sans MAP_S) --- */
static void addpad_at(float x, float y, float z, V3 launch) {   /* pad pose sur une plateforme de hauteur y */
    addbox(x - 1.5f, y, z - 1.5f, x + 1.5f, y + 0.3f, z + 1.5f, 0.2f, 0.9f, 1.0f);
    boxes[nb - 1].kind = B_PAD;
    boxes[nb - 1].launch = launch;
}
/* vitesse de lancement d'un pad en `from` (dessus du pad) pour retomber sur `to` avec une vitesse verticale vy */
static V3 pad_aim(V3 from, V3 to, float vy) {
    float disc = vy * vy - 2 * GRAVITY * (to.y - from.y);
    float t = (vy + sqrtf(fmaxf(disc, 0))) / GRAVITY;
    return v3((to.x - from.x) / t, vy, (to.z - from.z) / t);
}
static void addpad_to(float x, float y, float z, V3 to, float vy) { addpad_at(x, y, z, pad_aim(v3(x, y + 0.3f, z), to, vy)); }
/* bande d'acceleration au sol y : rectangle x0..x1, z0..z1, poussee vers (dx, dz) jusqu'a SPEED_MAX */
static void addspeed(float x0, float z0, float x1, float z1, float y, float dx, float dz) {
    addbox(x0, y, z0, x1, y + 0.12f, z1, 0.95f, 0.2f, 0.7f);
    boxes[nb - 1].kind = B_SPEED;
    boxes[nb - 1].launch = vmul(vnorm(v3(dx, 0, dz)), SPEED_MAX);
}
static void addanchor(float x, float y, float z) {      /* ancre de grappin : petit cube flottant, couleur dediee */
    addbox(x - 0.6f, y - 0.6f, z - 0.6f, x + 0.6f, y + 0.6f, z + 0.6f, 1.0f, 0.85f, 0.2f);
}
static void addswing(float x, float y, float z) {       /* point de balancement (course) : gros cube dore de 3 m, aimante en course */
    addbox(x - SWING_HALF, y - SWING_HALF, z - SWING_HALF, x + SWING_HALF, y + SWING_HALF, z + SWING_HALF, 1.0f, 0.82f, 0.25f);
}
static void addcell(float x, float y, float z, int pts) {   /* addpickup sans l'agrandissement MAP_S */
    if (npu >= MAXPICK) return;
    PU[npu].pos = v3(x, y + 0.9f, z);
    PU[npu].t = PU[npu].pending = 0;
    PU[npu].pts = pts;
    npu++;
}

/* Le Puits : puits de 40 x 40 m, vide au fond (killY -30), spirale de 8 plateformes reliees par des pads,
 * plateforme du sommet (y 50) avec les grandes cellules, 12 ancres au centre, 2 ilots de secours en bas */
static const float puitsPlat[8][3] = {                   /* centre x, z et dessus de chaque plateforme 8 x 8 */
    { 16, 16, 0 }, { 0, 16, 6 }, { -16, 16, 12 }, { -16, 0, 18 }, { -16, -16, 24 }, { 0, -16, 30 }, { 16, -16, 36 }, { 16, 0, 42 },
};
static void build_map_puits(void) {
    addbox(-22, -34, -22, 22, 70, -20, 0.16f, 0.20f, 0.36f);   /* parois : wall-run sur toute la hauteur */
    addbox(-22, -34, 20, 22, 70, 22, 0.16f, 0.20f, 0.36f);
    addbox(-22, -34, -20, -20, 70, 20, 0.19f, 0.23f, 0.40f);
    addbox(20, -34, -20, 22, 70, 20, 0.19f, 0.23f, 0.40f);
    for (int i = 0; i < 8; i++) {
        const float *p = puitsPlat[i];
        float t = (float)i / 7;
        addbox(p[0] - 4, p[2] - 1, p[1] - 4, p[0] + 4, p[2], p[1] + 4, 0.28f + 0.1f * t, 0.38f + 0.12f * t, 0.62f + 0.1f * t);
    }
    addbox(-6, 49, -6, 6, 50, 6, 0.55f, 0.62f, 0.88f);       /* sommet */
    addbox(-1, 50, 3.5f, 1, 51.2f, 4.5f, 0.42f, 0.48f, 0.72f); /* couvertures du sommet */
    addbox(-1, 50, -4.5f, 1, 51.2f, -3.5f, 0.42f, 0.48f, 0.72f);
    /* balcons d'objectif (zones A et C, bouclier) : a mi-hauteur au centre, meme hauteur des deux cotes ; on y monte au
     * grappin ou en se laissant tomber du sommet, on en descend sur la plateforme ouest (y 18) ou sur les ilots du fond */
    addbox(-13, 24, -6, -1, 25, 6, 0.30f, 0.62f, 0.70f);
    addbox(1, 24, -6, 13, 25, 6, 0.30f, 0.62f, 0.70f);
    for (int i = 0; i < 8; i++) {                         /* pads : chaque plateforme lance vers la suivante (la 8e vers le sommet) */
        const float *p = puitsPlat[i];
        V3 next = i < 7 ? v3(puitsPlat[i + 1][0], puitsPlat[i + 1][2], puitsPlat[i + 1][1]) : v3(0, 50, 0);
        V3 d = vnorm(v3(next.x - p[0], 0, next.z - p[1]));
        V3 land = i < 7 ? vsub(next, vmul(d, 2.5f)) : v3(3, 50, 0);
        addpad_to(p[0] + d.x * 2, p[2], p[1] + d.z * 2, land, 24);
    }
    addbox(3.5f, -11, 3.5f, 8.5f, -10, 8.5f, 0.40f, 0.26f, 0.52f);     /* ilots de secours au fond, pads vers le bas de la spirale */
    addbox(-8.5f, -11, 3.5f, -3.5f, -10, 8.5f, 0.40f, 0.26f, 0.52f);
    addpad_to(6, -10, 6, v3(17, 0, 13), 26);
    addpad_to(-6, -10, 6, v3(2, 6, 14), 30);
    for (int k = 0; k < 4; k++)                           /* 12 ancres suspendues au centre */
        for (int j = 0; j < 3; j++) {
            float a = k * 0.5f + j * 2.0944f;
            addanchor(cosf(a) * 6, 10 + 10.0f * k, sinf(a) * 6);
        }
    addcell(puitsPlat[1][0], puitsPlat[1][2], puitsPlat[1][1], PICKUP_SMALL);   /* recharges : surtout en haut */
    addcell(puitsPlat[3][0], puitsPlat[3][2], puitsPlat[3][1], PICKUP_SMALL);
    addcell(puitsPlat[5][0], puitsPlat[5][2], puitsPlat[5][1], PICKUP_SMALL);
    addcell(puitsPlat[7][0], puitsPlat[7][2], puitsPlat[7][1] + 2.5f, PICKUP_SMALL);
    addcell(3, 50, 3, PICKUP_BIG); addcell(-3, 50, -3, PICKUP_BIG);
}
static const float puitsSpawns[][3] = {
    { 17.8f, 0, 17.8f }, { 0, 6, 18.5f }, { -17.8f, 12, 17.8f }, { -18.5f, 18, 0 }, { -17.8f, 24, -17.8f },
    { 0, 30, -18.5f }, { 17.8f, 36, -17.8f }, { 18.5f, 42, 0 }, { -3, 50, 0 },
};

/* Le Canyon : deux falaises (y 20) separees par un gouffre de 70 m (killY -15), 6 ilots-piliers,
 * des ancres espacees de 20 m au plus, des pads au bord qui lancent vers les ilots */
static void build_map_canyon(void) {
    for (int s = -1; s <= 1; s += 2) {                    /* s = -1 : falaise ouest, +1 : falaise est (miroir) */
        float e = (float)s;
        addbox(s < 0 ? -75.0f : 35.0f, -40, -30, s < 0 ? -35.0f : 75.0f, 20, 30, 0.60f, 0.40f, 0.24f);   /* falaise */
        addbox(s < 0 ? -77.0f : 75.0f, 20, -30, s < 0 ? -75.0f : 77.0f, 28, 30, 0.50f, 0.33f, 0.20f);    /* mur du fond (wall-run) */
        addbox(s < 0 ? -75.0f : 35.0f, 20, 29.4f, s < 0 ? -35.0f : 75.0f, 21, 30, 0.55f, 0.37f, 0.22f);  /* parapets */
        addbox(s < 0 ? -75.0f : 35.0f, 20, -30, s < 0 ? -35.0f : 75.0f, 21, -29.4f, 0.55f, 0.37f, 0.22f);
        addbox(e * 60 - 3, 20, -3, e * 60 + 3, 30, 3, 0.48f, 0.30f, 0.19f);                  /* tour de tir (10 m) */
        addpad_to(e * 52.5f, 20, 0, v3(e * 58.5f, 30, 0), 24);
        for (int z = -1; z <= 1; z += 2) {
            float fz = (float)z;
            addbox(s < 0 ? -52.0f : 44.0f, 20, z < 0 ? -18.0f : 12.0f, s < 0 ? -44.0f : 52.0f, 24, z < 0 ? -12.0f : 18.0f, 0.70f, 0.53f, 0.36f);  /* abris */
            addbox(s < 0 ? -70.0f : 64.0f, 20, z < 0 ? -22.0f : 14.0f, s < 0 ? -64.0f : 70.0f, 26, z < 0 ? -14.0f : 22.0f, 0.66f, 0.49f, 0.33f);
            addbox(e * 20 - 3, -40, fz * 12 - 3, e * 20 + 3, 16, fz * 12 + 3, 0.54f, 0.35f, 0.21f);   /* ilots exterieurs */
            addpad_to(e * 38, 20, fz * 12, v3(e * 21.5f, 16, fz * 12), 26);                     /* bord -> ilot */
            addpad_to(e * 18.5f, 16, fz * 12, v3(e * 1.5f, 18, fz * 12), 26);                   /* ilot -> ilot central */
        }
        addbox(s < 0 ? -42.0f : 41.0f, 20, -5, s < 0 ? -41.0f : 42.0f, 21.2f, 5, 0.66f, 0.49f, 0.33f);    /* muret au bord */
    }
    addbox(-3, -40, -15, 3, 18, -9, 0.58f, 0.38f, 0.23f);  /* ilots centraux */
    addbox(-3, -40, 9, 3, 18, 15, 0.58f, 0.38f, 0.23f);
    /* plateau central (zone B des 3 ZONES) : 20 x 16 m a y 10, sous les ilots centraux ; on y descend des ilots
     * (ou au grappin), deux pads le relient au bord de chaque falaise */
    addbox(-10, -40, -8, 10, 9.7f, 8, 0.56f, 0.37f, 0.22f);
    addbox(-10, 9.7f, -8, 10, 10, 8, 0.30f, 0.62f, 0.64f);   /* dessus neon */
    addpad_to(-8.5f, 10, 0, v3(-47, 20, 0), 26);
    addpad_to(8.5f, 10, 0, v3(47, 20, 0), 26);
    addanchor(-28, 30, 0); addanchor(28, 30, 0);           /* ancres : on traverse toujours en 3 accroches */
    addanchor(-10, 31, 0); addanchor(10, 31, 0);
    addanchor(0, 33, -20); addanchor(0, 33, 20);
    for (int s = -1; s <= 1; s += 2)                      /* recharges : 6 sur les ilots, 2 par falaise */
        for (int z = -1; z <= 1; z += 2) addcell(s * 21.5f, 16, z * 12.0f, PICKUP_BIG);
    addcell(-1.5f, 18, -12, PICKUP_BIG); addcell(-1.5f, 18, 12, PICKUP_BIG);
    addcell(-67, 20, 0, PICKUP_SMALL); addcell(-60.5f, 30, 0, PICKUP_SMALL);
    addcell(67, 20, 0, PICKUP_SMALL); addcell(60.5f, 30, 0, PICKUP_SMALL);
}
static const float canyonSpawns[][3] = {
    { -60, 20, 10 }, { 60, 20, -10 }, { -60, 20, -10 }, { 60, 20, 10 }, { -47, 20, 0 }, { 47, 20, 0 }, { -72, 20, 0 }, { 72, 20, 0 },
};

/* La Boucle : anneau de 120 x 60 m autour d'un bloc de 10 m (coupe au milieu), deux voies par ligne droite
 * avec des bandes d'acceleration a contresens, passerelles a 10 m, pads de coin et pads vers le toit */
static void build_map_boucle(void) {
    addbox(-62, -2, -32, 62, 0, 32, 0.20f, 0.19f, 0.30f);    /* sol */
    addbox(-62, 0, 30, 62, 8, 32, 0.22f, 0.16f, 0.36f);      /* murs exterieurs (wall-run dans les virages) */
    addbox(-62, 0, -32, 62, 8, -30, 0.22f, 0.16f, 0.36f);
    addbox(60, 0, -30, 62, 8, 30, 0.26f, 0.18f, 0.40f);
    addbox(-62, 0, -30, -60, 8, 30, 0.26f, 0.18f, 0.40f);
    addbox(-45, 0, -15, -5, 10, 15, 0.30f, 0.22f, 0.46f);    /* bloc central, coupe en x = -5..5 */
    addbox(5, 0, -15, 45, 10, 15, 0.30f, 0.22f, 0.46f);
    addbox(-5, 9, -15, 5, 10, 15, 0.85f, 0.55f, 0.15f);      /* toit de la coupe */
    addbox(-3, 9, 15, 3, 10, 30, 0.90f, 0.60f, 0.15f);       /* passerelles a 10 m */
    addbox(-3, 9, -30, 3, 10, -15, 0.90f, 0.60f, 0.15f);
    for (int s = -1; s <= 1; s += 2) {
        float e = (float)s;
        for (int k = 0; k < 5; k++) {                     /* separateurs bas (1 m) entre les deux voies */
            float x0 = -42 + 18.0f * k;
            addbox(x0, 0, e * 22.5f - 0.3f, x0 + 12, 1, e * 22.5f + 0.3f, 0.2f, 0.85f, 0.8f);
        }
        /* bandes : voie exterieure vers +x au nord, -x au sud ; voie interieure a contresens */
        addspeed(-36, s > 0 ? 24.0f : -29.0f, -16, s > 0 ? 29.0f : -24.0f, 0, e, 0);
        addspeed(16, s > 0 ? 24.0f : -29.0f, 36, s > 0 ? 29.0f : -24.0f, 0, e, 0);
        addspeed(-36, s > 0 ? 16.0f : -21.0f, -16, s > 0 ? 21.0f : -16.0f, 0, -e, 0);
        addspeed(16, s > 0 ? 16.0f : -21.0f, 36, s > 0 ? 21.0f : -16.0f, 0, -e, 0);
        addpad_to(e * 52.5f, 0, 0, v3(e * 41, 10, 0), 24);    /* pads vers le toit */
        addbox(e * 25 - 1, 10, -3, e * 25 + 1, 11.5f, 3, 0.3f, 0.25f, 0.45f);   /* couvertures du toit */
        addbox(e * 36 - 3, 10, -9, e * 36 + 3, 11.5f, -8, 0.3f, 0.25f, 0.45f);
    }
    addpad_to(53, 0, 23, v3(30, 0, 18.5f), 14);              /* pads de coin : on coupe le virage vers la voie suivante */
    addpad_to(-53, 0, -23, v3(-30, 0, -18.5f), 14);
    addpad_to(-53, 0, 23, v3(-30, 0, 26.5f), 14);
    addpad_to(53, 0, -23, v3(30, 0, -26.5f), 14);
    addcell(0, 0, 26, PICKUP_SMALL); addcell(0, 0, -26, PICKUP_SMALL);    /* milieu des lignes droites, */
    addcell(0, 10, 22, PICKUP_BIG); addcell(0, 10, -22, PICKUP_BIG);      /* passerelles, */
    addcell(32, 10, 0, PICKUP_BIG); addcell(-32, 10, 0, PICKUP_BIG);      /* toit, */
    addcell(55, 0, 12, PICKUP_SMALL); addcell(-55, 0, -12, PICKUP_SMALL); /* bouts de l'anneau */
}
static const float boucleSpawns[][3] = {
    { 10, 0, 26 }, { -10, 0, -26 }, { -10, 0, 26 }, { 10, 0, -26 }, { 55, 0, 5 }, { -55, 0, -5 }, { 55, 0, -5 }, { -55, 0, 5 },
};

/* Le Dojo : salle de 36 x 36 m, plafond a 7 m, 16 piliers, ring central sureleve de 0,5 m, aucune cellule */
static void build_map_dojo(void) {
    addbox(-20, -2, -20, 20, 0, 20, 0.52f, 0.36f, 0.21f);    /* plancher */
    addbox(-20, 0, -20, 20, 7, -18, 0.40f, 0.27f, 0.17f);    /* murs */
    addbox(-20, 0, 18, 20, 7, 20, 0.40f, 0.27f, 0.17f);
    addbox(-20, 0, -18, -18, 7, 18, 0.44f, 0.30f, 0.19f);
    addbox(18, 0, -18, 20, 7, 18, 0.44f, 0.30f, 0.19f);
    addbox(-20, 7, -20, 20, 9, 20, 0.30f, 0.20f, 0.13f);     /* plafond : on s'y accroche au grappin */
    addbox(-6, 0, -6, 6, 0.5f, 6, 0.78f, 0.64f, 0.42f);      /* ring */
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) {
            addbox(sx * 5.8f - 0.2f, 0.5f, sz * 5.8f - 0.2f, sx * 5.8f + 0.2f, 2.0f, sz * 5.8f + 0.2f, 0.85f, 0.2f, 0.15f);   /* poteaux du ring */
            addbox(sx * 9.0f - 1, 0, sz * 9.0f - 1, sx * 9.0f + 1, 7, sz * 9.0f + 1, 0.34f, 0.21f, 0.13f);
        }
    static const float g[4] = { -13.5f, -4.5f, 4.5f, 13.5f };
    for (int i = 0; i < 4; i++)                           /* piliers : grille 4 x 4 sans les 4 du ring, + 4 en diagonale */
        for (int j = 0; j < 4; j++) {
            if (fabsf(g[i]) < 5 && fabsf(g[j]) < 5) continue;
            addbox(g[i] - 1, 0, g[j] - 1, g[i] + 1, 7, g[j] + 1, 0.34f, 0.21f, 0.13f);
        }
}
static const float dojoSpawns[][3] = {
    { 16, 0, 0 }, { -16, 0, 0 }, { 0, 0, 16 }, { 0, 0, -16 }, { 16, 0, 16 }, { -16, 0, -16 }, { -16, 0, 16 }, { 16, 0, -16 },
};

/* La Toile : course au grappin. Quatre avenues de gratte-ciel au-dessus du vide (killY -10), en serpentin :
 * nord le long de x = -60, est le long de z = -62, sud par x = 0,
 * est par z = 62, nord par x = 60 jusqu'au toit d'arrivee (y 50). Les tours servent d'ancres partout ; quelques
 * cubes flottants au milieu des avenues pour se balancer. Portes de la course : toileGates (MapDef.gates). */
static void toile_tower(float x, float z, float hw, float hd, float h, int k) {
    static const float tc[4][3] = { { 0.13f, 0.15f, 0.28f }, { 0.17f, 0.14f, 0.30f }, { 0.11f, 0.19f, 0.27f }, { 0.20f, 0.17f, 0.34f } };
    const float *c = tc[k & 3];
    addbox(x - hw, -12, z - hd, x + hw, h, z + hd, c[0], c[1], c[2]);
    if (k % 3 == 0) {                                     /* enseigne neon sur une face (magenta : lueur des neons) */
        float y = h * 0.55f + 4;
        addbox(x - hw - 0.3f, y, z - 1.5f, x - hw, y + 6, z + 1.5f, 0.85f, 0.3f, 0.75f);
    }
    addbox(x - hw + 0.5f, h, z - hd + 0.5f, x + hw - 0.5f, h + 1.2f, z + hd - 0.5f, c[0] * 1.3f, c[1] * 1.3f, c[2] * 1.3f);   /* toit */
}
static void build_map_toile(void) {
    static const float tw[][4] = {                        /* centre x, z, hauteur, teinte : tours 8 x 8 */
        /* avenue 1 (x = -60, vers le nord) */
        { -74, 44, 46, 0 }, { -74, 24, 54, 1 }, { -74, 4, 42, 2 }, { -74, -16, 58, 3 }, { -74, -36, 48, 0 },
        { -46, 34, 50, 1 }, { -46, 14, 40, 2 }, { -46, -6, 56, 3 }, { -46, -26, 44, 0 },
        /* avenue 2 (z = -62, vers l'est) */
        { -48, -74, 52, 1 }, { -28, -74, 46, 2 }, { -8, -74, 56, 3 }, { -38, -50, 40, 0 }, { -20, -50, 48, 1 },
        /* avenue 3 (x = 0, vers le sud) */
        { -14, -36, 50, 2 }, { -14, -16, 38, 3 }, { -14, 4, 44, 0 }, { -14, 24, 58, 1 }, { -14, 44, 46, 2 },
        { 14, -46, 44, 3 }, { 14, -26, 54, 0 }, { 14, -6, 36, 1 }, { 14, 14, 50, 2 }, { 14, 34, 42, 3 },
        /* avenue 4 (z = 62, vers l'est) */
        { 10, 74, 44, 0 }, { 30, 74, 52, 1 }, { 50, 74, 46, 2 }, { 26, 46, 40, 3 }, { 38, 42, 50, 0 },
        /* avenue 5 (x = 60, vers le nord) */
        { 74, 44, 50, 1 }, { 74, 24, 58, 2 }, { 74, 4, 48, 3 }, { 74, -16, 62, 0 }, { 74, -36, 54, 1 },
        { 46, 30, 44, 2 }, { 46, 10, 52, 3 }, { 46, -10, 46, 0 }, { 46, -28, 56, 1 },
    };
    for (int i = 0; i < (int)(sizeof tw / sizeof tw[0]); i++) toile_tower(tw[i][0], tw[i][1], 4, 4, tw[i][2], (int)tw[i][3]);
    addbox(-66, -12, 58, -54, 30, 72, 0.22f, 0.24f, 0.36f);     /* toit de depart */
    addbox(-64, 30, 71, -56, 33, 72, 0.9f, 0.75f, 0.2f);        /* bandeau de depart */
    addbox(-70, -12, -78, -54, 22, -64, 0.18f, 0.20f, 0.32f);   /* toits de repos aux virages */
    addbox(-6, -12, -78, 6, 24, -68, 0.18f, 0.20f, 0.32f);
    addbox(-6, -12, 66, 8, 20, 78, 0.18f, 0.20f, 0.32f);
    addbox(54, -12, 66, 68, 24, 78, 0.18f, 0.20f, 0.32f);
    addbox(54, -12, -46, 66, 50, -34, 0.30f, 0.26f, 0.12f);     /* tour d'arrivee */
    addbox(55, 50, -45, 65, 51, -35, 0.95f, 0.8f, 0.25f);       /* toit d'arrivee dore */
    /* points de balancement : au milieu des avenues, tous les 14 m environ, 15 a 20 m au-dessus des portes */
    static const float sw[][3] = {
        { -60, 50, 50 }, { -60, 52, 36 }, { -60, 50, 22 }, { -60, 52, 8 }, { -60, 50, -6 }, { -60, 48, -20 },
        { -60, 50, -34 }, { -60, 50, -48 }, { -54, 50, -62 },                                          /* avenue 1 et virage */
        { -42, 48, -62 }, { -28, 48, -62 }, { -16, 46, -62 }, { -2, 48, -58 },                           /* avenue 2 */
        { 0, 46, -46 }, { 0, 44, -32 }, { 0, 42, -18 }, { 0, 42, 4 }, { 0, 50, 16 }, { 0, 52, 30 }, { 0, 50, 44 },   /* avenue 3 */
        { 6, 48, 60 }, { 18, 48, 62 }, { 32, 48, 62 }, { 46, 50, 60 },                                  /* avenue 4 */
        { 60, 52, 52 }, { 60, 54, 38 }, { 60, 56, 24 }, { 60, 58, 10 }, { 60, 62, -4 }, { 60, 66, -18 },   /* avenue 5 */
        { 60, 70, -30 },
    };
    for (int i = 0; i < (int)(sizeof sw / sizeof sw[0]); i++) addswing(sw[i][0], sw[i][1], sw[i][2]);
    addcell(60, 51, -40, PICKUP_BIG);
    addcell(-62, 22, -70, PICKUP_SMALL); addcell(0, 24, -72, PICKUP_SMALL); addcell(1, 20, 72, PICKUP_SMALL); addcell(61, 24, 72, PICKUP_SMALL);
}
static const float toileSpawns[][3] = {
    { -60, 30, 64 }, { -62, 22, -70 }, { 0, 24, -72 }, { 1, 20, 72 }, { 61, 24, 72 }, { 60, 51, -40 }, { -74, 59.2f, -16 }, { 74, 63.2f, -16 },
};
/* portes de la course : centre x, y, z et rayon (la derniere = arrivee) */
static const float toileGates[][4] = {
    { -60, 30, 38, 8 }, { -60, 32, 8, 8 }, { -60, 30, -22, 8 }, { -58, 32, -50, 8 }, { -34, 32, -62, 8 }, { -12, 32, -62, 8 },
    { 0, 32, -36, 8 }, { 0, 30, -6, 8 }, { 0, 36, 24, 8 }, { 24, 34, 60, 8 }, { 58, 36, 40, 8 }, { 60, 42, 6, 8 }, { 60, 56, -40, 8 },
};

/* LE STAND : course de precision (CHALLENGE). Couloir ferme de 20 x 140 m, cinq portes dans l'axe (z -1), des cibles de visee
 * (MapDef.targets) devant chacune : une porte ne s'ouvre que cibles abattues ; fixes, petites, puis mobiles. Pas de vide. */
static void build_map_stand(void) {
    addbox(-10, -2, -72, 10, 0, 72, 0.14f, 0.16f, 0.30f);             /* sol */
    addbox(-12, 0, -72, -10, 16, 72, 0.18f, 0.20f, 0.38f);            /* murs */
    addbox(10, 0, -72, 12, 16, 72, 0.18f, 0.20f, 0.38f);
    addbox(-12, 0, 72, 12, 16, 74, 0.18f, 0.20f, 0.38f);
    addbox(-12, 0, -74, 12, 16, -72, 0.18f, 0.20f, 0.38f);
    for (int k = 0; k < 6; k++) {                                     /* bandeaux neon sur les murs, tous les 24 m */
        float z = 60 - 24.0f * k;
        addbox(-10.2f, 4, z - 3, -10, 5, z + 3, 0.2f, 0.9f, 1.0f);      /* plus de 3,4 m : pas pris pour un point de balancement */
        addbox(10, 4, z - 3, 10.2f, 5, z + 3, 1.0f, 0.3f, 0.75f);
    }
    addspeed(-3, 52, 3, 62, 0, 0, -1);                                /* bandes d'acceleration : on arrive vite a chaque stand */
    addspeed(-3, 24, 3, 32, 0, 0, -1);
    addspeed(-3, -2, 3, 6, 0, 0, -1);
    addspeed(-3, -28, 3, -20, 0, 0, -1);
    addspeed(-3, -52, 3, -44, 0, 0, -1);
    addbox(-3, 0, 44, 3, 1.1f, 46, 0.30f, 0.26f, 0.12f);              /* quelques obstacles bas a sauter */
    addbox(-8, 0, 8, -4, 1.1f, 10, 0.30f, 0.26f, 0.12f);
    addbox(4, 0, -16, 8, 1.1f, -14, 0.30f, 0.26f, 0.12f);
    addbox(-3, 0, -40, 3, 1.1f, -38, 0.30f, 0.26f, 0.12f);
}
static const float standSpawns[][3] = { { 0, 0, 64 }, { -6, 0, 62 }, { 6, 0, 62 } };
static const float standGates[][4] = {
    { 0, 2, 40, 5.5f }, { 0, 2, 14, 5.5f }, { 0, 2, -12, 5.5f }, { 0, 2, -38, 5.5f }, { 0, 2, -62, 5.5f },
};
/* x y z, mini, porte, mouvement (x y z), periode : cibles fixes (porte 0), petites (1), mobiles (2, 3), tout ensemble (4) */
static const float standTargets[][9] = {
    { -6, 4, 34, 0, 0, 0, 0, 0, 0 },     { 6, 2.5f, 36, 0, 0, 0, 0, 0, 0 },
    { -6, 5, 8, 1, 1, 0, 0, 0, 0 },      { 0, 7, 4, 1, 1, 0, 0, 0, 0 },      { 6, 3, 10, 1, 1, 0, 0, 0, 0 },
    { 0, 4, -6, 0, 2, 6, 0, 0, 2.6f },   { 0, 6, -2, 0, 2, 0, 3, 0, 2.0f },
    { 0, 4, -32, 1, 3, 5, 2, 0, 2.6f },  { -3, 5, -28, 1, 3, 5, 0, 3, 2.2f },  { 0, 9, -36, 0, 3, 0, 0, 0, 0 },
    { 0, 4, -56, 0, 4, 6, 3, 0, 2.2f },  { 0, 6, -52, 0, 4, -6, 0, 0, 2.4f },
    { -8, 8, -58, 1, 4, 0, 0, 0, 0 },    { 8, 8, -58, 1, 4, 0, 0, 0, 0 },
};

/* LE FLUX : course au grappin avec cibles (CHALLENGE). Les deux premieres avenues de LA TOILE (memes tours, memes points de
 * balancement, memes six portes, la derniere est l'arrivee) ; cibles dans la rue, en l'air, fixes puis mobiles. */
static void build_map_flux(void) {
    static const float tw[][4] = {
        { -74, 44, 46, 0 }, { -74, 24, 54, 1 }, { -74, 4, 42, 2 }, { -74, -16, 58, 3 }, { -74, -36, 48, 0 },
        { -46, 34, 50, 1 }, { -46, 14, 40, 2 }, { -46, -6, 56, 3 }, { -46, -26, 44, 0 },
        { -48, -74, 52, 1 }, { -28, -74, 46, 2 }, { -8, -74, 56, 3 }, { -38, -50, 40, 0 }, { -20, -50, 48, 1 },
    };
    for (int i = 0; i < (int)(sizeof tw / sizeof tw[0]); i++) toile_tower(tw[i][0], tw[i][1], 4, 4, tw[i][2], (int)tw[i][3]);
    addbox(-66, -12, 58, -54, 30, 72, 0.22f, 0.24f, 0.36f);           /* toit de depart */
    addbox(-64, 30, 71, -56, 33, 72, 0.9f, 0.75f, 0.2f);
    addbox(-70, -12, -78, -54, 22, -64, 0.18f, 0.20f, 0.32f);         /* toit de repos au virage */
    static const float sw[][3] = {
        { -60, 50, 50 }, { -60, 52, 36 }, { -60, 50, 22 }, { -60, 52, 8 }, { -60, 50, -6 }, { -60, 48, -20 },
        { -60, 50, -34 }, { -60, 50, -48 }, { -54, 50, -62 },
        { -42, 48, -62 }, { -28, 48, -62 }, { -16, 46, -62 }, { -2, 48, -58 },
    };
    for (int i = 0; i < (int)(sizeof sw / sizeof sw[0]); i++) addswing(sw[i][0], sw[i][1], sw[i][2]);
    addcell(-62, 22, -70, PICKUP_SMALL);
}
static const float fluxSpawns[][3] = { { -60, 30, 64 }, { -62, 22, -70 } };
static const float fluxGates[][4] = {
    { -60, 30, 38, 8 }, { -60, 32, 8, 8 }, { -60, 30, -22, 8 }, { -58, 32, -50, 8 }, { -34, 32, -62, 8 }, { -12, 32, -62, 8 },
};
static const float fluxTargets[][9] = {
    { -66, 36, 30, 0, 0, 0, 0, 0, 0 },     { -54, 38, 26, 0, 0, 0, 0, 0, 0 },
    { -66, 40, 10, 1, 1, 0, 0, 0, 0 },     { -54, 36, 2, 1, 1, 0, 0, 0, 0 },     { -60, 38, 14, 0, 1, 0, 4, 0, 2.0f },
    { -60, 38, -14, 0, 2, 0, 0, 5, 2.2f }, { -62, 34, -26, 1, 2, 0, 0, 0, 0 },
    { -58, 38, -44, 1, 3, 4, 3, 0, 2.0f }, { -64, 40, -40, 1, 3, 0, 0, 0, 0 },
    { -40, 38, -62, 0, 4, 0, 4, 0, 1.8f }, { -30, 36, -58, 1, 4, 0, 0, 0, 0 },
    { -18, 38, -62, 0, 5, 0, 0, 4, 2.0f }, { -10, 36, -56, 1, 5, 0, 0, 0, 0 },     { -10, 40, -68, 1, 5, 0, 0, 0, 0 },
    { -4, 36, -62, 0, 5, 0, 4, 0, 1.8f },
};

/* Arene : points de depart (anciennement dans pick_spawn, agrandis par MAP_S) */
#define MS(v) ((v) * MAP_S)
static const float areneSpawns[][3] = {
    { 0, 0, MS(24) }, { 0, 0, MS(-24) }, { MS(24), 0, 0 }, { MS(-24), 0, 0 }, { MS(-30), 0, MS(-30) }, { MS(30), 0, MS(-30) },
    { MS(30), 0, MS(30) }, { MS(-30), 0, MS(30) }, { MS(36), 7, 0 }, { MS(-36), 7, 0 }, { 0, 7, MS(36) }, { 0, 7, MS(-36) },
    { MS(5), 4, MS(5) }, { MS(-46), 0, 0 }, { MS(46), 0, 0 },
};

/* Description d'une carte. killY : plan de mort (-1000 = carte fermee). Les drones du solo apparaissent entre
 * enemyRMin et enemyRMax du centre, entre enemyMinY et enemyMaxY, et ne descendent jamais sous enemyFloor.
 * challenge = 1 : carte du mode COURSE seulement (LA TOILE) ; jamais en partie (HEBERGER, rotation, partie rapide,
 * classee) ni en solo par vagues. */
typedef struct {
    const char *name;                                     /* sans accents */
    void (*build)(void);
    const float (*spawns)[3]; int nspawns;
    V3 start;                                             /* depart du solo (reset_game) */
    float killY;
    float enemyMinY, enemyMaxY, enemyRMin, enemyRMax, enemyFloor;
    V3 menuCam; float menuPitch, specUp;                  /* camera du menu ; spectateur : specUp plus haut */
    int solo, music;
    float sky[3], fog[3];                                 /* haut du ciel ; horizon et brouillard */
    const float (*gates)[4]; int ngates;                  /* course (mode COURSE) : portes x y z rayon, la derniere = arrivee */
    float startYaw;                                       /* orientation au depart de la course */
    int challenge;                                        /* 1 : carte CHALLENGE (mode COURSE seulement) */
    float objScale;                                       /* echelle des zones et du bouclier (ZONE_R, SHIELD_ZONE_R) ; 0 = 1 */
    /* objectifs des modes (src/reseau/modes.c) ; NULL = mode indisponible sur cette carte. BLEUS du cote negatif de
     * l'axe des equipes, ROUGES du cote positif ; zones dans l'ordre A (cote BLEU), B (centre), C (cote ROUGE) */
    const float (*zones)[3];                              /* 3 zones (MODE_ZONES) */
    const float (*flags)[3];                              /* drapeau BLEU puis ROUGE, chez eux (MODE_DRAPEAU) */
    const float *shield;                                  /* bouclier des BLEUS (MODE_BOUCLIER) */
    const float (*teamSpawn)[3]; int nTeamSpawn;          /* departs par equipe : nTeamSpawn BLEUS puis nTeamSpawn ROUGES */
    /* cibles de visee d'une carte CHALLENGE (src/jeu/course.c) : x y z, 0 = CIBLE (0,8 m) / 1 = MINI (0,4 m), porte qu'elle verrouille
     * (la porte ne compte que cibles abattues), mouvement mx my mz (amplitude en m) et periode en s (0 : fixe) */
    const float (*targets)[9]; int ntargets;
} MapDef;
#define NSP(a) (int)(sizeof a / sizeof a[0])
/* --- objectifs des cartes neon (docs/CODE.md 3.23) --- */
/* ARENE : BLEUS au nord (z < 0). A et C sur les plateformes hautes nord et sud (pad depuis le centre, grappin),
 * B au sol a l'est entre le pad et la plateforme haute est ; drapeaux au fond de chaque moitie ; bouclier sur la
 * plateforme haute nord. Departs : la moitie de chaque camp, a l'ecart des blocs de couverture. */
static const float areneZones[3][3] = { { 0, 7, MS(-36) }, { 32, 0, 0 }, { 0, 7, MS(36) } };
static const float areneFlags[2][3] = { { 0, 0, -70 }, { 0, 0, 70 } };
static const float areneShield[3] = { 0, 7, MS(-36) };
static const float areneTeam[][3] = {
    { 0, 0, -36 }, { -42, 0, -42 }, { 42, 0, -42 }, { -24, 0, -66 }, { 24, 0, -66 }, { -30, 0, -16 }, { 30, 0, -16 },
    { MS(-46), 0, MS(-14) }, { MS(46), 0, MS(-14) },
    { 0, 0, 36 }, { -42, 0, 42 }, { 42, 0, 42 }, { -24, 0, 66 }, { 24, 0, 66 }, { -30, 0, 16 }, { 30, 0, 16 },
    { MS(-46), 0, MS(14) }, { MS(46), 0, MS(14) },
};
/* LE PUITS : BLEUS a l'ouest (x < 0 : plateformes 2, 3, 4), ROUGES a l'est (6, 7, 0). Spirale : symetrie impossible,
 * les paires sont symetriques par rapport a l'axe du puits (x, z -> -x, -z) et les departs ont la meme hauteur
 * moyenne. A et C sur les balcons (y 25), B au sommet ; drapeaux sur les plateformes 2 et 6, du cote oppose au pad ;
 * bouclier sur le balcon ouest. Plateformes de 8 m : zones et bouclier a l'echelle 0,6. */
static const float puitsZones[3][3] = { { -7, 25, 0 }, { 0, 50, 0 }, { 7, 25, 0 } };
static const float puitsFlags[2][3] = { { -16, 12, 17.8f }, { 16, 36, -17.8f } };
static const float puitsShield[3] = { -7, 25, 0 };
static const float puitsTeam[][3] = {
    { -18.8f, 12, 19 }, { -18.8f, 18, 0.6f }, { -17.8f, 24, -17.8f }, { -18.5f, 24, -15.2f },
    { 18.8f, 36, -19 }, { 18.8f, 42, -0.6f }, { 17.8f, 0, 17.8f }, { 18.5f, 0, 15.2f },
};
/* LE CANYON : BLEUS a l'ouest. A et C sur chaque falaise entre les deux abris sud, B sur le plateau central (y 10) ;
 * drapeaux derriere la tour de tir, devant le mur du fond ; bouclier sur la falaise ouest, entre les abris. */
static const float canyonZones[3][3] = { { -58, 20, -20 }, { 0, 10, 0 }, { 58, 20, -20 } };
static const float canyonFlags[2][3] = { { -70, 20, 0 }, { 70, 20, 0 } };
static const float canyonShield[3] = { -58, 20, -20 };
static const float canyonTeam[][3] = {
    { -60, 20, 10 }, { -60, 20, -10 }, { -47, 20, 0 }, { -67, 20, 8 }, { -67, 20, -8 }, { -50, 20, 24 },
    { 60, 20, 10 }, { 60, 20, -10 }, { 47, 20, 0 }, { 67, 20, 8 }, { 67, 20, -8 }, { 50, 20, 24 },
};
/* LA BOUCLE : BLEUS a l'ouest. A et C sur le toit du bloc central, B dans le passage sous le toit de la coupe ;
 * drapeaux au bout de l'anneau, entre le pad du toit et le mur ; bouclier sur le toit ouest. */
static const float boucleZones[3][3] = { { -32, 10, 0 }, { 0, 0, 0 }, { 32, 10, 0 } };
static const float boucleFlags[2][3] = { { -57, 0, 0 }, { 57, 0, 0 } };
static const float boucleShield[3] = { -32, 10, 0 };
static const float boucleTeam[][3] = {
    { -56, 0, -6 }, { -56, 0, 6 }, { -48, 0, -10 }, { -48, 0, 10 }, { -48, 0, -24 }, { -48, 0, 24 },
    { 56, 0, -6 }, { 56, 0, 6 }, { 48, 0, -10 }, { 48, 0, 10 }, { 48, 0, -24 }, { 48, 0, 24 },
};
/* LE DOJO : BLEUS au nord (z < 0). B sur le ring, A et C entre le ring et le mur du fond ; drapeaux dans deux coins
 * opposes (deux couloirs d'acces chacun) ; bouclier devant le ring, cote nord. Salle de 36 m : echelle 0,8. */
static const float dojoZones[3][3] = { { 0, 0, -9 }, { 0, 0.5f, 0 }, { 0, 0, 9 } };
static const float dojoFlags[2][3] = { { -16, 0, -16 }, { 16, 0, 16 } };
static const float dojoShield[3] = { 0, 0, -9 };
static const float dojoTeam[][3] = {
    { 0, 0, -16 }, { -9, 0, -16 }, { 9, 0, -16 }, { -16, 0, -9 }, { 16, 0, -9 },
    { 0, 0, 16 }, { -9, 0, 16 }, { 9, 0, 16 }, { -16, 0, 9 }, { 16, 0, 9 },
};
static const MapDef maps[] = {
    { "ARENE", build_map_arene, areneSpawns, NSP(areneSpawns), { 0, 0, 24 }, -1000,
      9, 18, MS(28), MS(48), 1.0f, { 0, 7, MS(36) }, -0.12f, 5, 1, 0, { 0.04f, 0.05f, 0.12f }, { 0.34f, 0.30f, 0.44f },
      NULL, 0, 0, 0, 1.0f, areneZones, areneFlags, areneShield, areneTeam, NSP(areneTeam) / 2 },
    { "LE PUITS", build_map_puits, puitsSpawns, NSP(puitsSpawns), { 17.8f, 0, 17.8f }, -30,
      10, 40, 4, 14, -8, { 0, 25, 0 }, -0.3f, 5, 0, 1, { 0.01f, 0.02f, 0.07f }, { 0.07f, 0.11f, 0.24f },
      NULL, 0, 0, 0, 0.6f, puitsZones, puitsFlags, puitsShield, puitsTeam, NSP(puitsTeam) / 2 },
    { "LE CANYON", build_map_canyon, canyonSpawns, NSP(canyonSpawns), { -60, 20, 10 }, -15,
      24, 34, 10, 50, 18, { 0, 30, 0 }, -0.25f, 5, 0, 2, { 0.18f, 0.11f, 0.09f }, { 0.72f, 0.46f, 0.30f },
      NULL, 0, 0, 0, 1.0f, canyonZones, canyonFlags, canyonShield, canyonTeam, NSP(canyonTeam) / 2 },
    { "LA BOUCLE", build_map_boucle, boucleSpawns, NSP(boucleSpawns), { 10, 0, 26 }, -1000,
      12, 18, 20, 50, 1.0f, { 0, 24, 0 }, -0.5f, 4, 1, 3, { 0.03f, 0.0f, 0.08f }, { 0.30f, 0.08f, 0.40f },
      NULL, 0, 0, 0, 1.0f, boucleZones, boucleFlags, boucleShield, boucleTeam, NSP(boucleTeam) / 2 },
    { "LE DOJO", build_map_dojo, dojoSpawns, NSP(dojoSpawns), { 0, 0.5f, 4 }, -1000,
      2.5f, 5.5f, 6, 15, 1.0f, { 0, 5, 15 }, -0.15f, 0, 1, 4, { 0.10f, 0.07f, 0.05f }, { 0.24f, 0.17f, 0.11f },
      NULL, 0, 0, 0, 0.8f, dojoZones, dojoFlags, dojoShield, dojoTeam, NSP(dojoTeam) / 2 },
    { "LA TOILE", build_map_toile, toileSpawns, NSP(toileSpawns), { -60, 30, 64 }, -10,
      30, 50, 20, 60, 5, { -60, 40, 78 }, -0.12f, 6, 0, 5, { 0.02f, 0.02f, 0.09f }, { 0.28f, 0.13f, 0.36f },
      toileGates, NSP(toileGates), 0, 1 },                /* CHALLENGE : course seulement */
    { "LE STAND", build_map_stand, standSpawns, NSP(standSpawns), { 0, 0, 64 }, -1000,
      3, 9, 5, 20, 1.0f, { 0, 12, 82 }, -0.15f, 4, 0, 5, { 0.02f, 0.03f, 0.10f }, { 0.12f, 0.20f, 0.36f },
      standGates, NSP(standGates), 0, 1, 0, NULL, NULL, NULL, NULL, 0, standTargets, NSP(standTargets) },   /* CHALLENGE : precision */
    { "LE FLUX", build_map_flux, fluxSpawns, NSP(fluxSpawns), { -60, 30, 64 }, -10,
      30, 50, 20, 60, 5, { -60, 40, 78 }, -0.12f, 6, 0, 5, { 0.02f, 0.05f, 0.08f }, { 0.10f, 0.30f, 0.34f },
      fluxGates, NSP(fluxGates), 0, 1, 0, NULL, NULL, NULL, NULL, 0, fluxTargets, NSP(fluxTargets) },       /* CHALLENGE : grappin et cibles */
};
#define NMAPS NSP(maps)
static int curMap;                                        /* carte chargee */
static int menuMap, mapRotate;                            /* choix du menu (velocity.ini : map=, map_rotate=) */
static float map_lost_y(void) { return maps[curMap].killY > -999 ? maps[curMap].killY : SAW_LOST_Y; }
/* carte jouable en partie (HEBERGER, rotation, partie rapide, classee, duel) : pas une carte CHALLENGE */
static int map_pvp(int m) { return m >= 0 && m < NMAPS && !maps[m].challenge; }
static float map_obj_scale(int m) { return maps[m].objScale > 0 ? maps[m].objScale : 1.0f; }
static int map_step(int m, int d) {                       /* menu : carte jouable suivante (d = 1) ou precedente (d = -1) */
    for (int k = 0; k < NMAPS; k++) {
        m = (m + d + NMAPS) % NMAPS;
        if (map_pvp(m)) return m;
    }
    return 0;
}

/* emprise de la carte (vue de dessus) : boite englobante horizontale ; mapFirst = 1 si boxes[0] est le sol qui la couvre */
static Box mapBox;
static int mapFirst;
static void build_level(int m) {
    nb = 0; npu = 0;
    maps[m >= 0 && m < NMAPS ? m : 0].build();
    mapBox.mn = boxes[0].mn; mapBox.mx = boxes[0].mx;
    for (int i = 1; i < nb; i++) {
        mapBox.mn = v3(fminf(mapBox.mn.x, boxes[i].mn.x), fminf(mapBox.mn.y, boxes[i].mn.y), fminf(mapBox.mn.z, boxes[i].mn.z));
        mapBox.mx = v3(fmaxf(mapBox.mx.x, boxes[i].mx.x), fmaxf(mapBox.mx.y, boxes[i].mx.y), fmaxf(mapBox.mx.z, boxes[i].mx.z));
    }
    mapBox.mx.y = 0;
    mapFirst = (boxes[0].mx.x - boxes[0].mn.x) * (boxes[0].mx.z - boxes[0].mn.z) >=
               0.9f * (mapBox.mx.x - mapBox.mn.x) * (mapBox.mx.z - mapBox.mn.z);
}
static void load_map(int m);                              /* reconstruit aussi la liste d'affichage (apres le rendu) */

static int box_overlap(V3 mn, V3 mx, const Box *b) {
    return mn.x < b->mx.x && mx.x > b->mn.x && mn.y < b->mx.y && mx.y > b->mn.y && mn.z < b->mx.z && mx.z > b->mn.z;
}

static int ray_box(V3 o, V3 d, V3 mn, V3 mx, float maxt, float *tout, V3 *nout) {
    float tmin = 0, tmax = maxt; int axis = -1; float sign = 0;
    for (int a = 0; a < 3; a++) {
        float oa = *vc(&o, a), da = *vc(&d, a), lo = *vc(&mn, a), hi = *vc(&mx, a);
        if (fabsf(da) < 1e-8f) {
            if (oa < lo || oa > hi) return 0;
        } else {
            float inv = 1.0f / da, t1 = (lo - oa) * inv, t2 = (hi - oa) * inv, s = -1;
            if (t1 > t2) { float tt = t1; t1 = t2; t2 = tt; s = 1; }
            if (t1 > tmin) { tmin = t1; axis = a; sign = s; }
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return 0;
        }
    }
    *tout = tmin;
    if (nout) { *nout = v3(0, 0, 0); if (axis >= 0) *vc(nout, axis) = sign; else *nout = vmul(d, -1); }
    return 1;
}

static int rayBox = -1;                                   /* boite touchee par le dernier ray_world (-1 : aucune) */
static float ray_world(V3 o, V3 d, float maxt, V3 *n) {
    float best = maxt; V3 bn = v3(0, 1, 0);
    rayBox = -1;
    for (int i = 0; i < nb; i++) {
        float t; V3 nn;
        if (ray_box(o, d, boxes[i].mn, boxes[i].mx, best, &t, &nn) && t < best) { best = t; bn = nn; rayBox = i; }
    }
    if (n) *n = bn;
    return best;
}

static int ray_sphere(V3 o, V3 d, V3 c, float r, float maxt, float *t) {
    V3 oc = vsub(o, c);
    float b = vdot(oc, d), cc = vdot(oc, oc) - r * r, h = b * b - cc;
    if (h < 0) return 0;
    h = sqrtf(h);
    float t0 = -b - h;
    if (t0 < 0) t0 = -b + h;
    if (t0 < 0 || t0 > maxt) return 0;
    *t = t0;
    return 1;
}

static int los(V3 a, V3 b) {
    V3 d = vsub(b, a); float l = vlen(d);
    if (l < 1e-4f) return 1;
    d = vmul(d, 1.0f / l);
    return ray_world(a, d, l, 0) >= l - 0.01f;
}

static void sphere_pushout(V3 *p, V3 *v, float r) {
    for (int i = 0; i < nb; i++) {
        Box *b = &boxes[i];
        if (p->x + r <= b->mn.x || p->x - r >= b->mx.x || p->y + r <= b->mn.y || p->y - r >= b->mx.y ||
            p->z + r <= b->mn.z || p->z - r >= b->mx.z) continue;
        float pen[6] = { p->x + r - b->mn.x, b->mx.x - (p->x - r), p->y + r - b->mn.y,
                         b->mx.y - (p->y - r), p->z + r - b->mn.z, b->mx.z - (p->z - r) };
        int k = 0;
        for (int j = 1; j < 6; j++) if (pen[j] < pen[k]) k = j;
        int ax = k / 2; float s = (k % 2) ? 1.0f : -1.0f;
        *vc(p, ax) += s * pen[k];
        if (*vc(v, ax) * s < 0) *vc(v, ax) = 0;
    }
}
