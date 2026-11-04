/* VELOCITY - lightmaps, occlusion, neons, liste du decor
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ---- eclairage precalcule (lightmaps) ----------------------------------------------------
 * Le decor ne bouge jamais : au chargement, chaque face de boite recoit une petite texture
 * d'eclairage (soleil avec ombres portees, ciel attenue par l'occlusion ambiante, lueur des
 * jump pads). Le rendu du monde la multiplie (x2, pour pouvoir eclaircir) sur la texture de base.
 * Pas de shader : tout se fait en pipeline fixe, avec des rayons contre les AABB. */
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#define LM_MAXT     512       /* cote maximal d'une lightmap */
#define LM_DENS     gxLmDens  /* texels de lightmap par metre (rendu.ini) */
#define AO_RAYS     ((int)gxAoRays)   /* rayons d'occlusion ambiante par texel */
#define AO_RANGE    gxAoRange /* portee des rayons d'occlusion (m) */
#define LMG_CELL    4.0f      /* grille d'acceleration des rayons courts (m) */
#define LMG_N       44
#define LMG_MAXB    48
static V3 sun_dir(void) { return vnorm(v3(gxSunDir[0], gxSunDir[1], gxSunDir[2])); }   /* vers le soleil (ombres, lumiere, ciel) */
static void cfg_save(void);
/* easter egg : un tir d'Elan qui part vers le soleil (rayon libre jusqu'au ciel) lui inflige ses degats ; 1000 cumules = skin POUSSIN */
static void sun_hit_check(V3 o, V3 end, int hit, float dmg) {
    if (hit || vlen(vsub(end, o)) < 290) return;          /* ennemi touche ou decor devant le ciel */
    V3 d = view_dir(), s = sun_dir();
    if (vdot(d, s) < cosf(gxSunSize[1] / 280.0f)) return;   /* hors du disque du soleil */
    int had = sunDmg >= SUN_UNLOCK_DMG;
    float before = sunDmg;
    sunDmg += dmg;
    add_part(vadd(o, vmul(s, 270)), vmul(rand_dir(), 20), v3(1, 0.9f, 0.4f), 0.6f, 0.5f, 0);
    if (!had && sunDmg >= SUN_UNLOCK_DMG) { snprintf(popText, sizeof popText, "SKIN POUSSIN DEBLOQUE !"); popT = 2.5f; }
    if (sunDmg >= SUN_UNLOCK_DMG ? !had : (int)(sunDmg / 50) != (int)(before / 50)) cfg_save();
}
typedef struct { GLuint tex; float su, sv; int on; } LMap;   /* su, sv : coordonnee de texture du bord oppose */
static LMap lmaps[MAXB][6];
static unsigned char lmgN[LMG_N][LMG_N], lmgB[LMG_N][LMG_N][LMG_MAXB];   /* indices de boites (MAXB <= 256) */
static double lmBakeMs;                                   /* duree du precalcul (journal, tests) */

/* face f d'une boite (0 +x, 1 -x, 2 +y, 3 -y, 4 +z, 5 -z, comme FACE_*) : 4 coins dans l'ordre de box_faces_m */
static void face_corners(V3 a, V3 b, int f, V3 o[4]) {
    switch (f) {
    case 0: o[0] = v3(b.x, a.y, a.z); o[1] = v3(b.x, b.y, a.z); o[2] = v3(b.x, b.y, b.z); o[3] = v3(b.x, a.y, b.z); break;
    case 1: o[0] = v3(a.x, a.y, a.z); o[1] = v3(a.x, a.y, b.z); o[2] = v3(a.x, b.y, b.z); o[3] = v3(a.x, b.y, a.z); break;
    case 2: o[0] = v3(a.x, b.y, a.z); o[1] = v3(a.x, b.y, b.z); o[2] = v3(b.x, b.y, b.z); o[3] = v3(b.x, b.y, a.z); break;
    case 3: o[0] = v3(a.x, a.y, a.z); o[1] = v3(b.x, a.y, a.z); o[2] = v3(b.x, a.y, b.z); o[3] = v3(a.x, a.y, b.z); break;
    case 4: o[0] = v3(a.x, a.y, b.z); o[1] = v3(b.x, a.y, b.z); o[2] = v3(b.x, b.y, b.z); o[3] = v3(a.x, b.y, b.z); break;
    default: o[0] = v3(a.x, a.y, a.z); o[1] = v3(a.x, b.y, a.z); o[2] = v3(b.x, b.y, a.z); o[3] = v3(b.x, a.y, a.z); break;
    }
}
static V3 face_normal(int f) { V3 n = v3(0, 0, 0); *vc(&n, f / 2) = (f & 1) ? -1.0f : 1.0f; return n; }
static int face_u(int f) { return f / 2 == 0 ? 2 : 0; }   /* axe des s : z pour les faces x, x sinon */
static int face_v(int f) { return f / 2 == 1 ? 2 : 1; }   /* axe des t : z pour les faces y, y sinon */
static int pow2up(int n) { int p = 1; while (p < n) p <<= 1; return p; }

static int point_in_box(V3 p) {
    for (int i = 0; i < nb; i++)
        if (p.x > boxes[i].mn.x && p.x < boxes[i].mx.x && p.y > boxes[i].mn.y && p.y < boxes[i].mx.y &&
            p.z > boxes[i].mn.z && p.z < boxes[i].mx.z) return 1;
    return 0;
}
/* face jamais visible : sous le sol, hors de l'arene, ou entierement enfouie dans d'autres boites */
static int face_hidden(int bi, int f) {
    Box *b = &boxes[bi];
    V3 n = face_normal(f), c[4];
    if (f == 3 && b->mn.y <= 0.001f) return 1;
    face_corners(b->mn, b->mx, f, c);
    float W = ms(56) + 0.5f;
    V3 mid = vmul(vadd(vadd(c[0], c[1]), vadd(c[2], c[3])), 0.25f);
    V3 mo = vadd(mid, vmul(n, 0.05f));
    if (fabsf(mo.x) > W || fabsf(mo.z) > W) return 1;
    if (!point_in_box(mo)) return 0;
    for (int k = 0; k < 4; k++) {
        V3 q = vadd(vadd(vlerp(c[k], mid, 0.02f), vmul(n, 0.05f)), v3(0, 0, 0));
        if (!point_in_box(q)) return 0;
    }
    for (int i = 0; i < 5; i++)                           /* grille 5 x 5 : coins et centre enfouis ne suffisent pas (un sol */
        for (int j = 0; j < 5; j++) {                     /* dont les coins sont dans l'enceinte et le centre sous une place) */
            float u = 0.02f + 0.96f * i / 4, v = 0.02f + 0.96f * j / 4;
            V3 q = vadd(vlerp(vlerp(c[0], c[1], u), vlerp(c[3], c[2], u), v), vmul(n, 0.05f));
            if (!point_in_box(q)) return 0;
        }
    return 1;
}

static void lm_grid_build(void) {                         /* boites proches de chaque case, pour les rayons courts */
    float org = -ms(56) - 4;
    memset(lmgN, 0, sizeof lmgN);
    for (int gx = 0; gx < LMG_N; gx++)
        for (int gz = 0; gz < LMG_N; gz++) {
            float x0 = org + gx * LMG_CELL, z0 = org + gz * LMG_CELL;
            for (int i = 0; i < nb && lmgN[gx][gz] < LMG_MAXB; i++) {
                Box *b = &boxes[i];
                if (b->mx.x + AO_RANGE < x0 || b->mn.x - AO_RANGE > x0 + LMG_CELL ||
                    b->mx.z + AO_RANGE < z0 || b->mn.z - AO_RANGE > z0 + LMG_CELL) continue;
                lmgB[gx][gz][lmgN[gx][gz]++] = (unsigned char)i;
            }
        }
}
static int lmHit;                                         /* boite touchee par le dernier ray_near (-1 : aucune) */
static float ray_near(V3 o, V3 d, float maxt) {           /* comme ray_world, limite aux boites voisines de o */
    float org = -ms(56) - 4;
    int gx = (int)((o.x - org) / LMG_CELL), gz = (int)((o.z - org) / LMG_CELL);
    if (gx < 0 || gz < 0 || gx >= LMG_N || gz >= LMG_N) { float t = ray_world(o, d, maxt, 0); lmHit = rayBox; return t; }
    float best = maxt;
    lmHit = -1;
    for (int k = 0; k < lmgN[gx][gz]; k++) {
        Box *b = &boxes[lmgB[gx][gz][k]];
        float t;
        if (ray_box(o, d, b->mn, b->mx, best, &t, NULL) && t < best) { best = t; lmHit = lmgB[gx][gz][k]; }
    }
    return best;
}

/* neons : bandes lumineuses sur le haut des plateformes (cyan), des murs de wall-run (magenta) et du centre (orange).
 * Decor seulement ; leur lueur est precalculee dans les lightmaps (points tous les 2 m, listes par case) */
typedef struct { V3 a, b; int kind; } Neon;
#define MAXNEON 96
#define MAXNPT 600
#define LMN_MAX 48
static Neon NEO[MAXNEON];
static int nneon, nNeonPt;
static V3 neonPt[MAXNPT];
static unsigned char neonPtK[MAXNPT];
static unsigned char lmnN[LMG_N][LMG_N];
static short lmnP[LMG_N][LMG_N][LMN_MAX];
static const float *neon_col(int k) { return k == 0 ? gxNeonC : k == 1 ? gxNeonM : gxNeonO; }
static void build_neon(void) {
    nneon = nNeonPt = 0;
    for (int i = 1; i < nb; i++) {
        Box *b = &boxes[i];
        int k = -1;
        if (b->kind != B_SOLID) continue;
        if (b->col.x < 0.35f && b->col.y > 0.6f) k = 0;                       /* plateformes hautes */
        else if (b->col.x > 0.7f && b->col.z > 0.55f && b->col.y < 0.5f) k = 1;   /* murs de wall-run */
        else if (b->col.x > 0.75f && b->col.y > 0.5f && b->col.y < 0.6f) k = 2;   /* plateforme centrale */
        if (k < 0) continue;
        float y = b->mx.y + 0.01f, e = 0.04f;
        V3 c[4] = { v3(b->mn.x - e, y, b->mn.z - e), v3(b->mx.x + e, y, b->mn.z - e), v3(b->mx.x + e, y, b->mx.z + e),
                    v3(b->mn.x - e, y, b->mx.z + e) };
        for (int s = 0; s < 4 && nneon < MAXNEON; s++) {
            Neon *nn = &NEO[nneon++];
            nn->a = c[s]; nn->b = c[(s + 1) % 4]; nn->kind = k;
            float len = vlen(vsub(nn->b, nn->a));
            for (float t = 1; t < len && nNeonPt < MAXNPT; t += 2) {
                neonPt[nNeonPt] = vadd(vlerp(nn->a, nn->b, t / len), v3(0, -0.15f, 0));
                neonPtK[nNeonPt++] = (unsigned char)k;
            }
        }
    }
    float org = -ms(56) - 4, R = gxNeon[2];               /* listes par case, pour la lueur precalculee */
    memset(lmnN, 0, sizeof lmnN);
    for (int gx = 0; gx < LMG_N; gx++)
        for (int gz = 0; gz < LMG_N; gz++) {
            float x0 = org + gx * LMG_CELL, z0 = org + gz * LMG_CELL;
            for (int q = 0; q < nNeonPt && lmnN[gx][gz] < LMN_MAX; q++)
                if (neonPt[q].x > x0 - R && neonPt[q].x < x0 + LMG_CELL + R && neonPt[q].z > z0 - R && neonPt[q].z < z0 + LMG_CELL + R)
                    lmnP[gx][gz][lmnN[gx][gz]++] = (short)q;
        }
}

/* lumiere recue en p (normale n) : RGB, environ 0 a 2 */
static void lm_light(V3 p, V3 n, unsigned int seed, float out[3]) {
    V3 L = sun_dir(), o = vadd(p, vmul(n, 0.04f));
    float ndl = vdot(n, L), sun = 0;
    if (ndl > 0 && ray_world(o, L, 400, 0) >= 399) sun = ndl;
    V3 t1 = fabsf(n.y) > 0.9f ? v3(1, 0, 0) : vnorm(vcross(v3(0, 1, 0), n)), t2 = vcross(n, t1);
    float rot = (float)((seed * 2654435761u) >> 8) * (1.0f / 16777216.0f) * 2 * PI, occ = 0, bleed[3] = { 0, 0, 0 };
    for (int k = 0; k < AO_RAYS; k++) {                   /* hemisphere, poids en cosinus (spirale doree) */
        float r = sqrtf((k + 0.5f) / AO_RAYS), ph = k * 2.39996f + rot, h = sqrtf(1 - r * r);
        V3 d = vadd(vadd(vmul(t1, r * cosf(ph)), vmul(t2, r * sinf(ph))), vmul(n, h));
        float t = ray_near(o, d, AO_RANGE);
        if (t < AO_RANGE) {
            occ += 1 - t / AO_RANGE;
            if (lmHit > 0) {                              /* rebond colore : un mur rouge teinte le sol voisin */
                float w = 1 - t / AO_RANGE;
                bleed[0] += boxes[lmHit].col.x * w; bleed[1] += boxes[lmHit].col.y * w; bleed[2] += boxes[lmHit].col.z * w;
            }
        }
    }
    float ao = AO_RAYS > 0 ? 1 - occ / AO_RAYS : 1;
    ao = ao * ao * (1 - gxAoMin) + gxAoMin;
    float sky = (0.78f + 0.22f * n.y) * ao, bounce = (0.5f - 0.5f * n.y) * ao;
    for (int c = 0; c < 3; c++)
        out[c] = skyCol[c] * sky + sunCol[c] * sun * gxSunPow + bounceCol[c] * bounce +
                 (AO_RAYS > 0 ? bleed[c] / AO_RAYS * gxBleed * (skyCol[c] + sunCol[c] * 0.5f) : 0);
    if (gxNeon[1] > 0) {                                  /* lueur des neons proches */
        float org = -ms(56) - 4, R = gxNeon[2];
        int gx = (int)((o.x - org) / LMG_CELL), gz = (int)((o.z - org) / LMG_CELL);
        if (gx >= 0 && gz >= 0 && gx < LMG_N && gz < LMG_N)
            for (int q = 0; q < lmnN[gx][gz]; q++) {
                V3 dv = vsub(neonPt[lmnP[gx][gz][q]], o);
                float dist = vlen(dv);
                if (dist > R || dist < 0.05f) continue;
                V3 dn = vmul(dv, 1 / dist);
                float lam = vdot(n, dn), fall = (1 - dist / R) * (1 - dist / R);
                if (lam <= 0 || ray_near(o, dn, dist - 0.2f) < dist - 0.25f) continue;
                const float *nc = neon_col(neonPtK[lmnP[gx][gz][q]]);
                for (int k = 0; k < 3; k++) out[k] += nc[k] * gxNeon[1] * lam * fall;
            }
    }
    for (int i = 0; i < nb; i++) {                        /* jump pads : lueur cyan sur le decor proche */
        if (boxes[i].kind != B_PAD) continue;
        V3 c = vlerp(boxes[i].mn, boxes[i].mx, 0.5f);
        c.y = boxes[i].mx.y + 0.3f;
        V3 dv = vsub(c, o);
        float dist = vlen(dv);
        if (dist > gxPad[1] || dist < 1e-3f) continue;
        float lam = vdot(n, vmul(dv, 1 / dist)), fall = (1 - dist / gxPad[1]) * (1 - dist / gxPad[1]);
        if (lam <= 0 || ray_near(o, vmul(dv, 1 / dist), dist - 0.4f) < dist - 0.45f) continue;
        for (int k = 0; k < 3; k++) out[k] += padGlowCol[k] * gxPad[0] * lam * fall;
    }
}

static void bake_lightmaps(void) {
    LARGE_INTEGER f0, t0, t1;
    QueryPerformanceFrequency(&f0); QueryPerformanceCounter(&t0);
    lm_grid_build();
    build_neon();
    float *buf = (float *)malloc(LM_MAXT * LM_MAXT * 3 * sizeof(float)), *tmp = (float *)malloc(LM_MAXT * LM_MAXT * 3 * sizeof(float));
    unsigned char *img = NULL;
    int imgCap = 0;
    for (int bi = 0; bi < nb; bi++)
        for (int f = 0; f < 6; f++) {
            LMap *m = &lmaps[bi][f];
            m->on = 0;
            if (face_hidden(bi, f)) continue;
            Box *b = &boxes[bi];
            int ua = face_u(f), va = face_v(f);
            float u0 = *vc(&b->mn, ua), u1 = *vc(&b->mx, ua), v0 = *vc(&b->mn, va), v1 = *vc(&b->mx, va);
            int nu = (int)ceilf((u1 - u0) * LM_DENS), nv = (int)ceilf((v1 - v0) * LM_DENS);
            nu = nu < 2 ? 2 : nu > LM_MAXT ? LM_MAXT : nu; nv = nv < 2 ? 2 : nv > LM_MAXT ? LM_MAXT : nv;
            V3 n = face_normal(f), p = (f & 1) ? b->mn : b->mx;
            for (int j = 0; j < nv; j++)
                for (int i = 0; i < nu; i++) {
                    *vc(&p, ua) = u0 + (i + 0.5f) / nu * (u1 - u0);
                    *vc(&p, va) = v0 + (j + 0.5f) / nv * (v1 - v0);
                    lm_light(p, n, (unsigned)(bi * 7919 + f * 104729 + j * 1031 + i), &buf[(j * nu + i) * 3]);
                }
            for (int pass = 0; pass < 2 * (int)gxLmBlur; pass++)   /* flou 1-2-1 (s puis t) : adoucit les bords d'ombre et le bruit */
                for (int j = 0; j < nv; j++)
                    for (int i = 0; i < nu; i++)
                        for (int c = 0; c < 3; c++) {
                            int vert = pass & 1;
                            int a0 = vert ? (j > 0 ? j - 1 : j) * nu + i : j * nu + (i > 0 ? i - 1 : i);
                            int a2 = vert ? (j < nv - 1 ? j + 1 : j) * nu + i : j * nu + (i < nu - 1 ? i + 1 : i);
                            float *src = vert ? tmp : buf, *dst = vert ? buf : tmp;
                            dst[(j * nu + i) * 3 + c] = 0.25f * src[a0 * 3 + c] + 0.5f * src[(j * nu + i) * 3 + c] + 0.25f * src[a2 * 3 + c];
                        }
            int tw = pow2up(nu), th = pow2up(nv);
            if (tw * th * 3 > imgCap) { imgCap = tw * th * 3; img = (unsigned char *)realloc(img, imgCap); }
            memset(img, 0, tw * th * 3);
            for (int j = 0; j < nv; j++)
                for (int i = 0; i < nu; i++)
                    for (int c = 0; c < 3; c++)
                        img[(j * tw + i) * 3 + c] = (unsigned char)(clampf(buf[(j * nu + i) * 3 + c] * 0.5f, 0, 1) * 255 + 0.5f);
            glGenTextures(1, &m->tex);
            glBindTexture(GL_TEXTURE_2D, m->tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tw, th, 0, GL_RGB, GL_UNSIGNED_BYTE, img);
            m->su = (float)nu / tw; m->sv = (float)nv / th; m->on = 1;
        }
    free(buf); free(tmp); free(img);
    QueryPerformanceCounter(&t1);
    lmBakeMs = (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / (double)f0.QuadPart;
}

/* une face, coordonnees de lightmap (0..su, 0..sv) */
static void lm_face(int bi, int f) {
    Box *b = &boxes[bi];
    LMap *m = &lmaps[bi][f];
    V3 c[4], n = face_normal(f);
    int ua = face_u(f), va = face_v(f);
    float u0 = *vc(&b->mn, ua), du = *vc(&b->mx, ua) - u0, v0 = *vc(&b->mn, va), dv = *vc(&b->mx, va) - v0;
    face_corners(b->mn, b->mx, f, c);
    glNormal3f(n.x, n.y, n.z);
    for (int k = 0; k < 4; k++) {
        glTexCoord2f((*vc(&c[k], ua) - u0) / du * m->su, (*vc(&c[k], va) - v0) / dv * m->sv);
        glVertex3f(c[k].x, c[k].y, c[k].z);
    }
}

/* lumiere du soleil en p : 1 au soleil, 0 a l'ombre (eclairage des objets mobiles) */
static float sun_vis(V3 p) { return ray_world(p, sun_dir(), 400, 0) >= 399 ? 1.0f : 0.0f; }
/* eclairage des objets mobiles : soleil (attenue a l'ombre) + ciel, dans le repere courant */
static void set_dyn_light(float vis) {
    V3 L = sun_dir();
    float k = gxObjSun[0] + gxObjSun[1] * vis;
    GLfloat lp[4] = { L.x, L.y, L.z, 0 }, amb[4] = { gxObjAmb[0], gxObjAmb[1], gxObjAmb[2], 1 };
    GLfloat dif[4] = { sunCol[0] * k, sunCol[1] * k, sunCol[2] * k, 1 };
    glLightfv(GL_LIGHT0, GL_POSITION, lp);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
}


/* dessine les faces visibles du monde, passe par passe (sol/plafond ou murs, ou toutes) */
static void world_faces(int mask, float scale) {
    glBegin(GL_QUADS);
    for (int i = 0; i < nb; i++) {
        glColor3f(boxes[i].col.x, boxes[i].col.y, boxes[i].col.z);
        for (int f = 0; f < 6; f++) {
            if (!(mask & (1 << f)) || !lmaps[i][f].on) continue;
            V3 c[4], n = face_normal(f);
            face_corners(boxes[i].mn, boxes[i].mx, f, c);
            quad_tex(n, c[0], c[1], c[2], c[3], scale);
        }
    }
    glEnd();
}

static GLuint skylineList;
static void build_skyline_list(void);
static void gfx_reload(void);
static void build_world_list(void) {
    bake_lightmaps();
    const float *fgc = map_fog_col();
    GLfloat fogScene[4] = { fgc[0], fgc[1], fgc[2], 1 }, fogGray[4] = { 0.5f, 0.5f, 0.5f, 1 }, fogWhite[4] = { 1, 1, 1, 1 };
    worldList = glGenLists(1);
    glNewList(worldList, GL_COMPILE);
    glEnable(GL_CULL_FACE);                                   /* normales sortantes verifiees : la face interieure de
                                                                * chaque boite (jamais vue) n'est pas dessinee */
    glCullFace(GL_BACK);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1, 1);
    glDisable(GL_LIGHTING);                                   /* l'eclairage vient des lightmaps (passe 3) */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texGrid);                    /* passe 1 : sol/plafond (faces horizontales) */
    world_faces(FACE_UD, gxTexScale[0]);
    glBindTexture(GL_TEXTURE_2D, texWall);                    /* passe 2 : murs (faces verticales) */
    world_faces(FACE_SIDES, gxTexScale[1]);
    glDepthFunc(GL_LEQUAL);                                   /* passes suivantes : meme profondeur que 1/2 */
    glEnable(GL_BLEND);
    glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);                  /* passe 3 : lightmaps, x2 (0,5 = inchange) */
    glFogfv(GL_FOG_COLOR, fogGray);                           /* au loin, la lightmap s'efface dans le brouillard */
    glColor3f(1, 1, 1);
    for (int i = 0; i < nb; i++)
        for (int f = 0; f < 6; f++) {
            if (!lmaps[i][f].on) continue;
            glBindTexture(GL_TEXTURE_2D, lmaps[i][f].tex);
            glBegin(GL_QUADS);
            lm_face(i, f);
            glEnd();
        }
    glBlendFunc(GL_DST_COLOR, GL_ZERO);                       /* passe 4 : grain fin, multiplicatif */
    glFogfv(GL_FOG_COLOR, fogWhite);
    glBindTexture(GL_TEXTURE_2D, texGrime);
    glColor3f(1, 1, 1);
    glBegin(GL_QUADS);
    for (int i = 0; i < nb; i++)
        for (int f = 0; f < 6; f++) {
            if (!lmaps[i][f].on) continue;
            V3 c[4];
            face_corners(boxes[i].mn, boxes[i].mx, f, c);
            quad_tex(face_normal(f), c[0], c[1], c[2], c[3], gxTexScale[2]);
        }
    glEnd();
    glFogfv(GL_FOG_COLOR, fogScene);
    glDisable(GL_BLEND);
    glDepthFunc(GL_LESS);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.5f);
    for (int i = 1; i < nb; i++) {
        if (boxes[i].kind == B_PAD) glColor4f(0.6f, 1.0f, 1.0f, 0.9f);
        else if (boxes[i].kind == B_SPEED) glColor4f(1.0f, 0.5f, 0.9f, 0.9f);
        else if (gxEdges > 0.001f) glColor4f(0.05f, 0.05f, 0.1f, gxEdges);
        else continue;
        box_edges(boxes[i].mn, boxes[i].mx);
    }
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);                                  /* n'affecte que ce display list, pas le reste du rendu */
    glEndList();
    build_skyline_list();
}

/* change de carte : boites, liste d'affichage, et plus rien de la carte precedente (projectiles, pieces, drones...) */
static int glReady;                                       /* contexte OpenGL cree (les tests --test n'en ont pas) */
static void load_map(int m) {
    if (m < 0 || m >= NMAPS) m = 0;
    for (int i = 0; i <= MAXP; i++) saw_remove_owner(i);  /* Scie : rendue a chacun */
    curMap = m;
    build_level(m);
    if (glReady) {
        if (worldList) glDeleteLists(worldList, 1);
        if (skylineList) glDeleteLists(skylineList, 1);
        for (int i = 0; i < MAXB; i++)                    /* lightmaps de la carte precedente : liberees (sinon fuite) */
            for (int f = 0; f < 6; f++)
                if (lmaps[i][f].on) { glDeleteTextures(1, &lmaps[i][f].tex); lmaps[i][f].on = 0; }
        build_world_list();
    }
    memset(PR, 0, sizeof PR); memset(CO, 0, sizeof CO); memset(E, 0, sizeof E);
    memset(PA, 0, sizeof PA); memset(BM, 0, sizeof BM); memset(TR, 0, sizeof TR);
    for (int i = 0; i < npu; i++) PU[i].t = PU[i].pending = 0;
    voidPending = 0;
}

