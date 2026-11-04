/* VELOCITY - second ecran : carte vue de dessus en direct + statistiques de la partie
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* Dessine tout l'ecran de carte dans le contexte OpenGL courant (w x h pixels). Ne depend d'aucun etat laisse par
   render() : le second ecran a son propre contexte (textures et listes partagees avec le principal). La fenetre
   elle-meme est geree par plateforme/second_ecran.c ; --shot carte.bmp dessine cet ecran dans la fenetre du jeu. */

#define MAPS_TRAIL 28                                     /* points de trainee par joueur (un tous les MAPS_TRAIL_DT) */
#define MAPS_TRAIL_DT 0.07
static V3 mapsTrail[MAXP + 1][MAPS_TRAIL];                /* MAXP : le joueur local en solo */
static int mapsTrailN[MAXP + 1], mapsTrailHead[MAXP + 1];
static double mapsTrailT;
static float mapsMaxSpeed;                                /* vitesse max atteinte pendant la partie en cours */
static int shotMap;                                       /* --shot carte*.bmp : render() dessine cet ecran a la place du jeu */

typedef struct { float x0, y0, k; } MapView;              /* monde (x, z) -> ecran : x0 + (x - mn) * k */
static float mapsMnX, mapsMnZ;

static void maps_pt(const MapView *v, V3 p, float *sx, float *sy) {
    *sx = v->x0 + (p.x - mapsMnX) * v->k;
    *sy = v->y0 + (p.z - mapsMnZ) * v->k;
}

/* La carte ne doit pas servir de radar contre l'adversaire : pendant une partie on n'y voit que soi et ses equipiers
   (en chacun pour soi : soi seul). Tout le monde apparait en spectateur et une fois la partie finie. */
static int maps_sees(int i) {
    if (!dmMode || spectating || matchOver) return 1;
    return i == myId || same_team(i, myId);
}

/* un joueur affiche : position, cap, vivant, couleur, nom, PV */
typedef struct { V3 pos; float yaw, hp; int alive, self, id; const float *col; const char *name; } MapsPl;

static int maps_players(MapsPl *out) {
    static const float selfCol[3] = { 1, 1, 1 };
    int n = 0;
    if (!dmMode) {                                        /* solo : moi seulement */
        out[n].pos = P.pos; out[n].yaw = P.yaw; out[n].hp = P.hp; out[n].alive = !P.dead; out[n].self = 1;
        out[n].id = MAXP; out[n].col = selfCol; out[n].name = myName; n++;
        return n;
    }
    for (int i = 0; i < MAXP; i++) {
        if (!NP[i].active || !maps_sees(i)) continue;
        int me = i == myId && !spectating;
        out[n].pos = me ? P.pos : NP[i].disp; out[n].yaw = me ? P.yaw : NP[i].st.yaw;
        out[n].hp = me ? P.hp : NP[i].hp; out[n].alive = me ? !P.dead : NP[i].alive; out[n].self = me;
        out[n].id = i; out[n].col = pcolor(i); out[n].name = NP[i].name; n++;
    }
    return n;
}

static void maps_trail_tick(const MapsPl *pl, int n) {
    if (nowT - mapsTrailT < MAPS_TRAIL_DT && nowT >= mapsTrailT) return;
    mapsTrailT = nowT;
    for (int k = 0; k < n; k++) {
        int id = pl[k].id;
        if (!pl[k].alive) { mapsTrailN[id] = 0; continue; }
        int last = (mapsTrailHead[id] + MAPS_TRAIL - 1) % MAPS_TRAIL;
        if (mapsTrailN[id] && vlen(vsub(mapsTrail[id][last], pl[k].pos)) > 12) mapsTrailN[id] = 0;   /* respawn : pas de trait */
        mapsTrail[id][mapsTrailHead[id]] = pl[k].pos;
        mapsTrailHead[id] = (mapsTrailHead[id] + 1) % MAPS_TRAIL;
        if (mapsTrailN[id] < MAPS_TRAIL) mapsTrailN[id]++;
    }
}

static void maps_disc(float x, float y, float r, int seg) {
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for (int s = 0; s <= seg; s++) glVertex2f(x + r * cosf(s * 2 * PI / seg), y + r * sinf(s * 2 * PI / seg));
    glEnd();
}

static void maps_ring(float x, float y, float r, int seg) {
    glBegin(GL_LINE_LOOP);
    for (int s = 0; s < seg; s++) glVertex2f(x + r * cosf(s * 2 * PI / seg), y + r * sinf(s * 2 * PI / seg));
    glEnd();
}

static void maps_draw_map(const MapView *v, float S, const MapsPl *pl, int n) {
    float sx, sy, ex, ey;
    rect(v->x0 - 10, v->y0 - 10, S + 20, S + 20, 0.02f, 0.025f, 0.06f, 1);   /* cadre */
    rect(v->x0, v->y0, S, S, 0.07f, 0.08f, 0.13f, 1);                          /* sol */
    glLineWidth(1);
    glColor4f(0.3f, 0.4f, 0.6f, 0.12f);                   /* quadrillage tous les 10 m */
    glBegin(GL_LINES);
    for (float g = ceilf(mapsMnX / 10) * 10; (g - mapsMnX) * v->k <= S; g += 10) {
        glVertex2f(v->x0 + (g - mapsMnX) * v->k, v->y0); glVertex2f(v->x0 + (g - mapsMnX) * v->k, v->y0 + S);
    }
    for (float g = ceilf(mapsMnZ / 10) * 10; (g - mapsMnZ) * v->k <= S; g += 10) {
        glVertex2f(v->x0, v->y0 + (g - mapsMnZ) * v->k); glVertex2f(v->x0 + S, v->y0 + (g - mapsMnZ) * v->k);
    }
    glEnd();

    int ord[MAXB], no = 0;                                /* blocs, du plus bas au plus haut : le haut recouvre le bas */
    for (int i = mapFirst; i < nb; i++) ord[no++] = i;
    for (int a = 1; a < no; a++)
        for (int b = a; b > 0 && boxes[ord[b]].mx.y < boxes[ord[b - 1]].mx.y; b--) { int t = ord[b]; ord[b] = ord[b - 1]; ord[b - 1] = t; }
    for (int o = 0; o < no; o++) {
        const Box *b = &boxes[ord[o]];
        maps_pt(v, b->mn, &sx, &sy); maps_pt(v, b->mx, &ex, &ey);
        float w = ex - sx, h = ey - sy;
        if (b->kind == B_PAD) {                           /* jump pad : disque cyan qui pulse */
            float pl2 = 0.6f + 0.4f * sinf((float)nowT * 5 + b->mn.x);
            glColor4f(0.2f, 0.95f, 1.0f, 0.25f + 0.35f * pl2);
            maps_disc(sx + w * 0.5f, sy + h * 0.5f, fmaxf(w, h) * 0.5f + 4, 20);
            glColor4f(0.6f, 1, 1, 0.9f);
            maps_ring(sx + w * 0.5f, sy + h * 0.5f, fmaxf(w, h) * 0.5f + 4 + 3 * pl2, 20);
            continue;
        }
        float hk = clampf(b->mx.y / 16, 0, 1), sh = 0.45f + 0.55f * hk;   /* plus c'est haut, plus c'est clair */
        if (b->mn.y > 0.5f) rect(sx + 5, sy + 5, w, h, 0, 0, 0, 0.35f);   /* plateforme flottante : ombre portee */
        rect(sx, sy, w, h, b->col.x * sh, b->col.y * sh, b->col.z * sh, 1);
        glColor4f(0.75f + 0.25f * hk, 0.85f + 0.15f * hk, 1, 0.35f + 0.3f * hk);
        glBegin(GL_LINE_LOOP);
        glVertex2f(sx, sy); glVertex2f(ex, sy); glVertex2f(ex, ey); glVertex2f(sx, ey);
        glEnd();
    }

    for (int i = 0; i < npu; i++) {                       /* recharges : losange dore si disponible */
        maps_pt(v, PU[i].pos, &sx, &sy);
        float a = PU[i].t > 0 ? 0.2f : 0.95f, r = PU[i].pts >= PICKUP_BIG ? 8.0f : 6.0f;
        glColor4f(1, 0.82f, 0.3f, a);
        glBegin(GL_QUADS);
        glVertex2f(sx, sy - r); glVertex2f(sx + r, sy); glVertex2f(sx, sy + r); glVertex2f(sx - r, sy);
        glEnd();
    }
    for (int i = 0; i < MAXCOIN; i++)                     /* pieces en vol */
        if (CO[i].alive && maps_sees(CO[i].owner)) { maps_pt(v, CO[i].pos, &sx, &sy); glColor4f(1, 0.9f, 0.4f, 1); maps_disc(sx, sy, 3, 8); }
    for (int i = 0; i < MAXPR; i++) {                     /* projectiles : roquettes oranges, le reste blanc */
        if (!PR[i].alive || !maps_sees(PR[i].owner)) continue;
        maps_pt(v, PR[i].pos, &sx, &sy);
        if (PR[i].kind == PR_ROCKET) glColor4f(1, 0.55f, 0.15f, 1); else glColor4f(0.9f, 0.9f, 1, 0.9f);
        maps_disc(sx, sy, PR[i].kind == PR_ROCKET ? 4.5f : 3.5f, 10);
    }
    if (!dmMode)
        for (int i = 0; i < MAXE; i++) {                  /* solo : ennemis en carres rouges */
            if (!E[i].alive) continue;
            maps_pt(v, E[i].pos, &sx, &sy);
            rect(sx - 5, sy - 5, 10, 10, 1, 0.2f + 0.6f * clampf(E[i].flash, 0, 1), 0.25f, 0.95f);
        }

    glLineWidth(3);                                       /* trainees : 2 s de trajet, a la couleur du joueur */
    for (int k = 0; k < n; k++) {
        int id = pl[k].id, cnt = mapsTrailN[id];
        if (cnt < 2) continue;
        glBegin(GL_LINE_STRIP);
        for (int j = 0; j < cnt; j++) {
            V3 p = mapsTrail[id][(mapsTrailHead[id] - cnt + j + 2 * MAPS_TRAIL) % MAPS_TRAIL];
            maps_pt(v, p, &sx, &sy);
            glColor4f(pl[k].col[0], pl[k].col[1], pl[k].col[2], 0.55f * j / cnt);
            glVertex2f(sx, sy);
        }
        glEnd();
    }
    glLineWidth(1);

    for (int k = 0; k < n; k++) {                         /* joueurs : fleche orientee, plus grosse en hauteur */
        const MapsPl *p = &pl[k];
        maps_pt(v, p->pos, &sx, &sy);
        const float *c = p->col;
        if (!p->alive) {                                  /* mort : croix */
            glLineWidth(3); glColor4f(c[0], c[1], c[2], 0.5f);
            glBegin(GL_LINES);
            glVertex2f(sx - 7, sy - 7); glVertex2f(sx + 7, sy + 7); glVertex2f(sx + 7, sy - 7); glVertex2f(sx - 7, sy + 7);
            glEnd();
            glLineWidth(1);
            continue;
        }
        float s1 = sinf(p->yaw), c1 = -cosf(p->yaw), sz = 11 * (1 + clampf(p->pos.y / 20, 0, 0.8f));
        if (p->self) {                                    /* moi : cone de vision */
            float half = 0.6f, L = 70;
            glColor4f(1, 1, 1, 0.10f);
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(sx, sy);
            for (int s = 0; s <= 8; s++) {
                float a = p->yaw - half + 2 * half * s / 8;
                glVertex2f(sx + sinf(a) * L, sy - cosf(a) * L);
            }
            glEnd();
        }
        glColor4f(0, 0, 0, 0.6f);
        maps_disc(sx, sy, sz * 0.9f, 16);
        glColor4f(c[0], c[1], c[2], 1);
        glBegin(GL_TRIANGLES);
        glVertex2f(sx + s1 * sz, sy + c1 * sz);
        glVertex2f(sx - s1 * sz * 0.6f + c1 * sz * 0.65f, sy - c1 * sz * 0.6f - s1 * sz * 0.65f);
        glVertex2f(sx - s1 * sz * 0.6f - c1 * sz * 0.65f, sy - c1 * sz * 0.6f + s1 * sz * 0.65f);
        glEnd();
        if (p->self) { glLineWidth(2); glColor4f(1, 1, 1, 0.9f); maps_ring(sx, sy, sz + 4, 20); glLineWidth(1); }
        float tw = fontW[0] * (float)strlen(p->name);
        text(0, sx - tw * 0.5f, sy - sz - 26, p->name, c[0] * 0.5f + 0.5f, c[1] * 0.5f + 0.5f, c[2] * 0.5f + 0.5f, 1);
        rect(sx - 16, sy + sz + 4, 32, 4, 0.1f, 0.1f, 0.15f, 0.8f);
        float hk = clampf(p->hp / 100, 0, 1);
        rect(sx - 16, sy + sz + 4, 32 * hk, 4, 1 - hk * 0.7f, 0.25f + hk * 0.6f, 0.35f, 1);
    }
}

/* ---- vue 3D inclinee : meme contenu, vu de biais (hauteurs lisibles : poteau et ombre sous chaque joueur) ---- */
static GLfloat maps3Mv[16], maps3Pr[16];
static float maps3X, maps3Y, maps3S;                      /* viewport de la vue 3D, en pixels (origine en haut) */

static int maps_proj3(V3 p, float *sx, float *sy) {       /* monde -> ecran de la vue 3D */
    float v[4] = { p.x, p.y, p.z, 1 }, e[4], c[4];
    for (int r = 0; r < 4; r++) e[r] = maps3Mv[r] * v[0] + maps3Mv[4 + r] * v[1] + maps3Mv[8 + r] * v[2] + maps3Mv[12 + r] * v[3];
    for (int r = 0; r < 4; r++) c[r] = maps3Pr[r] * e[0] + maps3Pr[4 + r] * e[1] + maps3Pr[8 + r] * e[2] + maps3Pr[12 + r] * e[3];
    if (c[3] <= 0.05f) return 0;
    *sx = maps3X + (c[0] / c[3] * 0.5f + 0.5f) * maps3S;
    *sy = maps3Y + (1 - (c[1] / c[3] * 0.5f + 0.5f)) * maps3S;
    return 1;
}

static void maps_box3(V3 a, V3 b, float r, float g, float bl, float al) {   /* ombrage fixe par face, sans eclairage GL */
    static const float shade[5] = { 1.0f, 0.62f, 0.78f, 0.7f, 0.55f };      /* dessus, +x, -x, +z (vers nous), -z */
    glBegin(GL_QUADS);
    glColor4f(r * shade[0], g * shade[0], bl * shade[0], al);
    glVertex3f(a.x, b.y, a.z); glVertex3f(a.x, b.y, b.z); glVertex3f(b.x, b.y, b.z); glVertex3f(b.x, b.y, a.z);
    glColor4f(r * shade[1], g * shade[1], bl * shade[1], al);
    glVertex3f(b.x, a.y, a.z); glVertex3f(b.x, b.y, a.z); glVertex3f(b.x, b.y, b.z); glVertex3f(b.x, a.y, b.z);
    glColor4f(r * shade[2], g * shade[2], bl * shade[2], al);
    glVertex3f(a.x, a.y, a.z); glVertex3f(a.x, a.y, b.z); glVertex3f(a.x, b.y, b.z); glVertex3f(a.x, b.y, a.z);
    glColor4f(r * shade[3], g * shade[3], bl * shade[3], al);
    glVertex3f(a.x, a.y, b.z); glVertex3f(b.x, a.y, b.z); glVertex3f(b.x, b.y, b.z); glVertex3f(a.x, b.y, b.z);
    glColor4f(r * shade[4], g * shade[4], bl * shade[4], al);
    glVertex3f(a.x, a.y, a.z); glVertex3f(a.x, b.y, a.z); glVertex3f(b.x, b.y, a.z); glVertex3f(b.x, a.y, a.z);
    glEnd();
}

static void maps_draw_3d(float x0, float y0, float S, int scrW, int scrH, const MapsPl *pl, int n) {
    const Box *fl = &mapBox;
    V3 c = v3((fl->mn.x + fl->mx.x) * 0.5f, 0, (fl->mn.z + fl->mx.z) * 0.5f);
    float span = fmaxf(fl->mx.x - fl->mn.x, fl->mx.z - fl->mn.z), sx, sy;
    rect(x0 - 10, y0 - 10, S + 20, S + 20, 0.02f, 0.025f, 0.06f, 1);
    rect_vgrad(x0, y0, S, S, 0.10f, 0.11f, 0.2f, 1, 0.3f);   /* fond : ciel sombre */

    maps3X = x0; maps3Y = y0; maps3S = S;
    glViewport((int)x0, scrH - (int)(y0 + S), (int)S, (int)S);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    float zn = 5, fh = zn * tanf(gxMap3d[0] * PI / 360);
    glFrustum(-fh, fh, -fh, fh, zn, span * 4);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glTranslatef(0, -span * 0.04f, -span * gxMap3d[1]);
    glRotatef(gxMap3d[2], 1, 0, 0);                     /* inclinaison : on regarde du sud, en plongee */
    glTranslatef(-c.x, 0, -c.z);
    glGetFloatv(GL_MODELVIEW_MATRIX, maps3Mv);
    glGetFloatv(GL_PROJECTION_MATRIX, maps3Pr);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);

    glColor4f(0.07f, 0.08f, 0.13f, 1);                    /* sol + quadrillage */
    glBegin(GL_QUADS);
    glVertex3f(fl->mn.x, 0, fl->mn.z); glVertex3f(fl->mn.x, 0, fl->mx.z); glVertex3f(fl->mx.x, 0, fl->mx.z); glVertex3f(fl->mx.x, 0, fl->mn.z);
    glEnd();
    glColor4f(0.3f, 0.4f, 0.6f, 0.15f);
    glBegin(GL_LINES);
    for (float g = ceilf(fl->mn.x / 10) * 10; g <= fl->mx.x; g += 10) { glVertex3f(g, 0.02f, fl->mn.z); glVertex3f(g, 0.02f, fl->mx.z); }
    for (float g = ceilf(fl->mn.z / 10) * 10; g <= fl->mx.z; g += 10) { glVertex3f(fl->mn.x, 0.02f, g); glVertex3f(fl->mx.x, 0.02f, g); }
    glEnd();

    for (int i = mapFirst; i < nb; i++) {                 /* blocs ; les murs exterieurs, tres hauts, sont rabotes */
        const Box *b = &boxes[i];
        V3 a = b->mn, m = b->mx;
        int border = m.x <= fl->mn.x + 0.1f || a.x >= fl->mx.x - 0.1f || m.z <= fl->mn.z + 0.1f || a.z >= fl->mx.z - 0.1f;
        if (border) m.y = fminf(m.y, 1.5f);
        if (b->kind == B_PAD) {
            float pl2 = 0.6f + 0.4f * sinf((float)nowT * 5 + a.x);
            maps_box3(a, v3(m.x, a.y + 0.3f, m.z), 0.2f * pl2, 0.95f * pl2, pl2, 1);
            continue;
        }
        float hk = 0.55f + 0.45f * clampf(m.y / 16, 0, 1);
        maps_box3(a, m, b->col.x * hk, b->col.y * hk, b->col.z * hk, 1);
    }

    for (int i = 0; i < npu; i++) {                       /* recharges : petits cubes dores qui flottent */
        V3 p = PU[i].pos; float s = PU[i].pts >= PICKUP_BIG ? 0.9f : 0.6f, bob = 0.3f * sinf((float)nowT * 3 + i);
        if (PU[i].t > 0) continue;
        maps_box3(v3(p.x - s, p.y - s + bob, p.z - s), v3(p.x + s, p.y + s + bob, p.z + s), 1, 0.82f, 0.3f, 1);
    }
    for (int i = 0; i < MAXPR; i++) {
        if (!PR[i].alive || !maps_sees(PR[i].owner)) continue;
        V3 p = PR[i].pos;
        if (PR[i].kind == PR_ROCKET) maps_box3(v3(p.x - 0.6f, p.y - 0.6f, p.z - 0.6f), v3(p.x + 0.6f, p.y + 0.6f, p.z + 0.6f), 1, 0.55f, 0.15f, 1);
        else maps_box3(v3(p.x - 0.4f, p.y - 0.4f, p.z - 0.4f), v3(p.x + 0.4f, p.y + 0.4f, p.z + 0.4f), 0.9f, 0.9f, 1, 1);
    }
    if (!dmMode)
        for (int i = 0; i < MAXE; i++) {
            if (!E[i].alive) continue;
            V3 p = E[i].pos;
            maps_box3(v3(p.x - 0.9f, p.y - 0.9f, p.z - 0.9f), v3(p.x + 0.9f, p.y + 0.9f, p.z + 0.9f), 1, 0.25f, 0.25f, 1);
        }


    glLineWidth(3);                                       /* trainees en 3D : on voit les sauts */
    for (int k = 0; k < n; k++) {
        int id = pl[k].id, cnt = mapsTrailN[id];
        if (cnt < 2) continue;
        glBegin(GL_LINE_STRIP);
        for (int j = 0; j < cnt; j++) {
            V3 p = mapsTrail[id][(mapsTrailHead[id] - cnt + j + 2 * MAPS_TRAIL) % MAPS_TRAIL];
            glColor4f(pl[k].col[0], pl[k].col[1], pl[k].col[2], 0.6f * j / cnt);
            glVertex3f(p.x, p.y + 1, p.z);
        }
        glEnd();
    }
    for (int k = 0; k < n; k++) {                         /* joueurs : pion a la couleur, poteau et ombre jusqu'au sol */
        const MapsPl *p = &pl[k];
        if (!p->alive) continue;
        const float *col = p->col;
        V3 q = p->pos;
        float gh = ray_world(v3(q.x, q.y + 0.2f, q.z), v3(0, -1, 0), 200, 0), gy = q.y + 0.2f - gh, s = 2.2f;
        glColor4f(0, 0, 0, 0.5f);                         /* ombre au sol */
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(q.x, gy + 0.05f, q.z);
        for (int a = 0; a <= 16; a++) glVertex3f(q.x + cosf(a * PI / 8) * 3, gy + 0.05f, q.z + sinf(a * PI / 8) * 3);
        glEnd();
        if (q.y - gy > 0.8f) {                            /* en l'air : poteau pointille jusqu'au sol */
            glLineWidth(2);
            glColor4f(col[0], col[1], col[2], 0.7f);
            glBegin(GL_LINES);
            for (float yy = gy; yy < q.y; yy += 1.2f) { glVertex3f(q.x, yy, q.z); glVertex3f(q.x, fminf(yy + 0.6f, q.y), q.z); }
            glEnd();
        }
        maps_box3(v3(q.x - s, q.y, q.z - s), v3(q.x + s, q.y + 6.5f, q.z + s), col[0], col[1], col[2], 1);
        float s1 = sinf(p->yaw), c1 = -cosf(p->yaw), ty = q.y + 6.7f;   /* fleche du regard sur la tete */
        glColor4f(p->self ? 1 : col[0] * 0.5f + 0.5f, p->self ? 1 : col[1] * 0.5f + 0.5f, p->self ? 1 : col[2] * 0.5f + 0.5f, 1);
        glBegin(GL_TRIANGLES);
        glVertex3f(q.x + s1 * 6, ty, q.z + c1 * 6);
        glVertex3f(q.x - s1 * 2.5f + c1 * 3.2f, ty, q.z - c1 * 2.5f - s1 * 3.2f);
        glVertex3f(q.x - s1 * 2.5f - c1 * 3.2f, ty, q.z - c1 * 2.5f + s1 * 3.2f);
        glEnd();
    }
    glLineWidth(1);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();
    glViewport(0, 0, scrW, scrH);

    for (int k = 0; k < n; k++) {                         /* noms et PV, projetes au-dessus des pions */
        const MapsPl *p = &pl[k];
        if (!p->alive || !maps_proj3(v3(p->pos.x, p->pos.y + 8.0f, p->pos.z), &sx, &sy)) continue;
        const float *col = p->col;
        float tw = fontW[0] * (float)strlen(p->name), hk = clampf(p->hp / 100, 0, 1);
        text(0, sx - tw * 0.5f, sy - 10, p->name, col[0] * 0.5f + 0.5f, col[1] * 0.5f + 0.5f, col[2] * 0.5f + 0.5f, 1);
        rect(sx - 16, sy, 32, 4, 0.1f, 0.1f, 0.15f, 0.8f);
        rect(sx - 16, sy, 32 * hk, 4, 1 - hk * 0.7f, 0.25f + hk * 0.6f, 0.35f, 1);
    }
}

/* ---- carte de chaleur : ou l'on tue, ou l'on meurt (affichee a la fin du match) ---- */
#define MAPS_HEAT_N 400
typedef struct { V3 kill, death; int self; } MapsHeat;   /* kill : position du tueur (absente si suicide) */
static MapsHeat mapsHeat[MAPS_HEAT_N];
static int mapsHeatN;

static V3 maps_pos_of(int i) { return i == myId && !spectating ? P.pos : NP[i].disp; }

static void maps_heat_add(int killer, int victim) {
    if (mapsHeatN >= MAPS_HEAT_N || victim < 0 || victim >= MAXP) return;
    MapsHeat *h = &mapsHeat[mapsHeatN++];
    h->death = maps_pos_of(victim);
    h->self = killer < 0 || killer >= MAXP || killer == victim;
    h->kill = h->self ? h->death : maps_pos_of(killer);
}

static void maps_blob(float x, float y, float r, float cr, float cg, float cb, float a) {   /* tache degradee */
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(cr, cg, cb, a);
    glVertex2f(x, y);
    glColor4f(cr, cg, cb, 0);
    for (int s = 0; s <= 24; s++) glVertex2f(x + r * cosf(s * PI / 12), y + r * sinf(s * PI / 12));
    glEnd();
}

static void maps_draw_heat(const MapView *v, float S) {
    float sx, sy, r = MAPS_HEAT_R * v->k;
    char buf[64];
    rect(v->x0, v->y0, S, S, 0, 0, 0.02f, 0.45f);         /* le decor s'efface un peu sous la chaleur */
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);                    /* additif : les taches qui se recouvrent chauffent */
    for (int i = 0; i < mapsHeatN; i++) {
        maps_pt(v, mapsHeat[i].death, &sx, &sy); maps_blob(sx, sy, r, 1, 0.22f, 0.15f, 0.45f);
        if (!mapsHeat[i].self) { maps_pt(v, mapsHeat[i].kill, &sx, &sy); maps_blob(sx, sy, r * 0.8f, 1, 0.8f, 0.25f, 0.35f); }
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    rect(v->x0, v->y0 + S - 40, S, 40, 0.02f, 0.02f, 0.05f, 0.8f);
    snprintf(buf, sizeof buf, "CARTE DE CHALEUR - %d kills", mapsHeatN);
    text(0, v->x0 + 14, v->y0 + S - 14, buf, 1, 1, 1, 1);
    float lx = v->x0 + S - 330;
    rect(lx, v->y0 + S - 28, 14, 14, 1, 0.3f, 0.2f, 1);  text(0, lx + 22, v->y0 + S - 14, "morts", 1, 0.6f, 0.5f, 1);
    rect(lx + 130, v->y0 + S - 28, 14, 14, 1, 0.8f, 0.3f, 1); text(0, lx + 152, v->y0 + S - 14, "tireurs", 1, 0.85f, 0.5f, 1);
}

/* ---- courbe de vitesse : un point toutes les MAPS_SPD_DT s par joueur, toute la partie ---- */
#define MAPS_SPD_DT 0.5
#define MAPS_SPD_N  1300                                  /* 650 s : un match (MATCH_TIME) et sa fin */
static unsigned char mapsSpd[MAXP + 1][MAPS_SPD_N];       /* vitesse horizontale x 4 (0 a 63 m/s) ; MAXP : moi en solo */
static unsigned char mapsSpdOn[MAXP + 1][MAPS_SPD_N];     /* le joueur etait-il la (et vivant) a ce moment ? */
static int mapsSpdN;
static double mapsSpdT;

static void maps_speed_reset(void) { mapsSpdN = 0; mapsSpdT = nowT; }

static void maps_speed_tick(void) {
    if (nowT - mapsSpdT < MAPS_SPD_DT || mapsSpdN >= MAPS_SPD_N || matchOver) return;
    mapsSpdT = nowT;
    for (int i = 0; i <= MAXP; i++) {
        float v = 0; int on = 0;
        if (!dmMode) { if (i == MAXP) { v = hlen(P.vel); on = !P.dead; } }
        else if (i < MAXP && NP[i].active) {
            int me = i == myId && !spectating;
            v = me ? hlen(P.vel) : hlen(np_vel(i));
            on = me ? !P.dead : NP[i].alive;
        }
        mapsSpd[i][mapsSpdN] = (unsigned char)clampf(v * 4, 0, 255);
        mapsSpdOn[i][mapsSpdN] = (unsigned char)on;
    }
    mapsSpdN++;
}

/* courbes des joueurs visibles : 60 dernieres secondes pendant la partie, toute la partie a la fin */
static void maps_draw_speed(float x, float y, float w, float h) {
    char buf[32];
    int n1 = mapsSpdN, n0 = matchOver ? 0 : (int)fmaxf(0, (float)(mapsSpdN - 60 / MAPS_SPD_DT));
    float vmax = 30;
    for (int i = 0; i <= MAXP; i++)
        for (int k = n0; k < n1; k++) vmax = fmaxf(vmax, mapsSpd[i][k] / 4.0f);
    vmax = ceilf(vmax / 10) * 10;
    rect(x, y, w, h, 0.05f, 0.06f, 0.1f, 1);
    for (float g = 10; g < vmax; g += 10) {               /* graduations tous les 10 m/s */
        float gy = y + h - h * g / vmax;
        rect(x, gy, w, 1, 0.4f, 0.5f, 0.7f, 0.18f);
        snprintf(buf, sizeof buf, "%.0f", g);
        text(0, x + 4, gy - 2, buf, 0.5f, 0.6f, 0.75f, 0.7f);
    }
    if (n1 - n0 < 2) return;
    float span = matchOver ? (float)(n1 - 1) : (float)(60 / MAPS_SPD_DT);
    glLineWidth(2);
    for (int i = 0; i <= MAXP; i++) {
        if (dmMode ? (i == MAXP || !NP[i].active || !maps_sees(i)) : i != MAXP) continue;
        const float *c = dmMode ? pcolor(i) : NULL;
        int self = dmMode ? i == myId : 1;
        float al = self ? 1.0f : 0.75f;
        if (self) glLineWidth(3);
        glColor4f(c ? c[0] : 0.6f, c ? c[1] : 0.95f, c ? c[2] : 1, al);
        glBegin(GL_LINE_STRIP);
        for (int k = n0; k < n1; k++) {
            if (!mapsSpdOn[i][k]) { glEnd(); glBegin(GL_LINE_STRIP); continue; }   /* mort : la courbe s'interrompt */
            glVertex2f(x + w * (k - n0) / fmaxf(span, 1), y + h - h * (mapsSpd[i][k] / 4.0f) / vmax);
        }
        glEnd();
        glLineWidth(2);
    }
    glLineWidth(1);
}

static float maps_title(float x, float y, const char *s) {   /* intitule de rubrique, renvoie la ligne suivante */
    text(0, x, y, s, 0.5f, 0.75f, 1, 0.9f);
    rect(x, y + 8, 60, 2, 0.5f, 0.75f, 1, 0.6f);
    return y + 40;
}

static void maps_draw_panel(float x, float y, float w, float h) {
    char buf[96];
    float yMax = y + h;
    rect(x - 20, y - 20, w + 40, h + 40, 0.03f, 0.035f, 0.07f, 1);
    text(2, x, y + 44, "VELOCITY", 1, 1, 1, 1);
    y += 90;
    if (dmMode) {                                         /* mode, temps, limite */
        if (teamSize > 0) snprintf(buf, sizeof buf, "EQUIPES %dv%d  -  %d frags", teamSize, teamSize, team_frag_limit(teamSize));
        else snprintf(buf, sizeof buf, "DEATHMATCH  -  %d frags", fragLimit);
        text(1, x, y, buf, 0.85f, 0.88f, 0.95f, 1);
        int left = (int)fmaxf(0, MATCH_TIME - matchT);
        snprintf(buf, sizeof buf, "%d:%02d", left / 60, left % 60);
        text(2, x + w - fontW[2] * (float)strlen(buf), y - 46, buf, left < 60 ? 1.0f : 0.9f, left < 60 ? 0.45f : 0.9f, 0.4f, 1);   /* sur la ligne du titre */
        y += 50;
        if (matchOver) {
            winner_text(buf, (int)sizeof buf);
            text(1, x, y, buf, 1, 0.85f, 0.3f, 1);
            y += 40;
        }
        if (teamSize > 0) {                               /* scores d'equipe en grand */
            for (int t = 0; t < 2; t++) {
                const float *c = teamCol[t];
                float bx = x + t * (w * 0.5f + 6), bw = w * 0.5f - 6;
                rect(bx, y, bw, 58, c[0] * 0.25f, c[1] * 0.25f, c[2] * 0.25f, 1);
                rect(bx, y, bw * clampf(team_score(t) / (float)team_frag_limit(teamSize), 0, 1), 6, c[0], c[1], c[2], 1);
                snprintf(buf, sizeof buf, "%s  %d", teamNames[t], team_score(t));
                text(1, bx + 14, y + 42, buf, c[0], c[1], c[2], 1);
            }
            y += 80;
        }
        y = maps_title(x, y, "CLASSEMENT");
        int order[MAXP], n = 0;
        for (int i = 0; i < MAXP; i++) if (NP[i].active) order[n++] = i;
        for (int a = 1; a < n; a++)
            for (int b = a; b > 0 && NP[order[b]].kills > NP[order[b - 1]].kills; b--) { int t = order[b]; order[b] = order[b - 1]; order[b - 1] = t; }
        text(0, x + w * 0.52f, y, "FRAGS", 0.6f, 0.65f, 0.75f, 1);
        text(0, x + w * 0.68f, y, "MORTS", 0.6f, 0.65f, 0.75f, 1);
        text(0, x + w * 0.84f, y, "RATIO", 0.6f, 0.65f, 0.75f, 1);
        y += 34;
        int lim = teamSize > 0 ? team_frag_limit(teamSize) : fragLimit;
        for (int k = 0; k < n && y < yMax - 260; k++) {   /* garder la place pour MOI */
            int i = order[k];
            const float *c = pcolor(i);
            float al = NP[i].alive ? 1.0f : 0.5f;
            if (i == myId) rect(x - 8, y - 25, w + 16, 32, 1, 1, 1, 0.07f);
            rect(x - 8, y + 6, (w + 16) * clampf(NP[i].kills / (float)(lim > 0 ? lim : 1), 0, 1), 3, c[0], c[1], c[2], 0.7f);
            snprintf(buf, sizeof buf, "%d. %s", k + 1, NP[i].name);
            text(1, x, y, buf, c[0], c[1], c[2], al);
            snprintf(buf, sizeof buf, "%d", NP[i].kills);  text(1, x + w * 0.52f, y, buf, 1, 1, 1, al);
            snprintf(buf, sizeof buf, "%d", NP[i].deaths); text(1, x + w * 0.68f, y, buf, 0.8f, 0.8f, 0.85f, al);
            snprintf(buf, sizeof buf, "%.1f", NP[i].kills / (float)(NP[i].deaths > 0 ? NP[i].deaths : 1));
            text(1, x + w * 0.84f, y, buf, 0.8f, 0.8f, 0.85f, al);
            y += 34;
        }
        y += 16;
    } else {                                              /* solo : vague, score */
        snprintf(buf, sizeof buf, "SOLO  -  VAGUE %d", wave);
        text(1, x, y, buf, 0.85f, 0.88f, 0.95f, 1);
        y += 50;
        y = maps_title(x, y, "PARTIE");
        int alive = 0;
        for (int i = 0; i < MAXE; i++) alive += E[i].alive;
        snprintf(buf, sizeof buf, "score %d   (record %d)", score, bestScore); text(1, x, y, buf, 1, 1, 1, 1); y += 40;
        snprintf(buf, sizeof buf, "kills %d   ennemis %d", kills, alive); text(1, x, y, buf, 0.85f, 0.85f, 0.9f, 1); y += 56;
    }

    if (!spectating) {                                    /* mes chiffres */
        y = maps_title(x, y, "MOI");
        float sp = hlen(P.vel);
        snprintf(buf, sizeof buf, "%.1f m/s", sp);
        text(2, x, y + 16, buf, 0.6f, 0.95f, 1, 1);
        y += 44;
        snprintf(buf, sizeof buf, "max %.1f m/s", mapsMaxSpeed);
        text(0, x, y, buf, 0.75f, 0.8f, 0.9f, 1);
        snprintf(buf, sizeof buf, "chaine x%.2f", streak_mult());
        text(0, x + w - fontW[0] * (float)strlen(buf), y, buf, 1, 0.85f, 0.4f, 1);
        y += 18;
        rect(x, y, w, 6, 0.1f, 0.1f, 0.15f, 1);
        rect(x, y, w * clampf(sp / 40, 0, 1), 6, 0.4f, 0.9f, 1, 1);
        y += 30;
        const AmmoDef *ad = &ammoDef[P.weapon];
        snprintf(buf, sizeof buf, "%s  %d/%d", ad->name, P.ammo[P.weapon], ad->max);
        text(1, x, y, buf, ad->r, ad->g, ad->b, 1);
        snprintf(buf, sizeof buf, "%d PV", (int)ceilf(P.hp));
        text(1, x + w - fontW[1] * (float)strlen(buf), y, buf, P.hp < 35 ? 1.0f : 0.8f, P.hp < 35 ? 0.35f : 1.0f, 0.5f, 1);
        y += 46;
    }

    if (yMax - y > 140) {                                 /* courbe de vitesse */
        y = maps_title(x, y, matchOver ? "VITESSE (m/s) - TOUTE LA PARTIE" : "VITESSE (m/s) - 60 DERNIERES s");
        maps_draw_speed(x, y - 18, w, 100);
        y += 110;
    }

    if (dmMode && yMax - y > 60) {                        /* derniers evenements */
        y = maps_title(x, y, "DERNIERS KILLS");
        for (int k = 0; k < 6 && y < yMax; k++) {
            KillFeed *f = &kfeed[k];
            if (f->t <= 0) continue;
            int joinLeave = f->info & (KF_JOIN | KF_LEAVE);
            if (joinLeave) snprintf(buf, sizeof buf, "%s %s", pname(f->victim), f->info == KF_JOIN ? "a rejoint" : "a quitte");
            else if (f->killer == f->victim) snprintf(buf, sizeof buf, "%s s'est fait sauter", pname(f->victim));
            else snprintf(buf, sizeof buf, "%s  [%s%s]  %s", pname(f->killer), weaponNames[f->kind % W_COUNT],
                          (f->info & KF_ONESHOT) ? " ONE-SHOT" : (f->info & KF_HEAD) ? " TETE" : "", pname(f->victim));
            const float *c = f->killer >= 0 && f->killer < MAXP && !joinLeave ? pcolor(f->killer) : NULL;
            text(0, x, y, buf, c ? c[0] * 0.5f + 0.5f : 0.85f, c ? c[1] * 0.5f + 0.5f : 0.85f, c ? c[2] * 0.5f + 0.5f : 0.85f,
                 clampf(f->t, 0.3f, 1));
            y += 28;
        }
    }
}

static void maps_shot_fill(void) {                       /* --shot carte*.bmp : une minute de vitesses inventees */
    mapsSpdN = 120; mapsSpdT = nowT + 1e6;
    mapsHeatN = 0;                                        /* ...et des kills autour du centre et des pads */
    for (int i = 0; i < 46; i++) {
        float a = i * 2.4f, d = i % 3 ? 14 + 25 * frand() : 50 + 10 * frand();
        mapsHeat[mapsHeatN].death = v3(cosf(a) * d, 0, sinf(a) * d);
        mapsHeat[mapsHeatN].kill = v3(cosf(a + 0.5f) * (d + 15), 0, sinf(a + 0.5f) * (d + 15));
        mapsHeat[mapsHeatN++].self = i % 9 == 0;
    }
    for (int i = 0; i <= MAXP; i++)
        for (int k = 0; k < mapsSpdN; k++) {
            float v = 14 + 9 * sinf(k * 0.11f + i * 1.7f) + 6 * sinf(k * 0.37f + i) + (k % 40 > 34 ? 18 : 0);
            mapsSpd[i][k] = (unsigned char)clampf(v * 4, 0, 255);
            mapsSpdOn[i][k] = !(i == 1 && k > 70 && k < 82);   /* une mort au milieu */
        }
}

static void draw_map_screen(int w, int h) {
    glViewport(0, 0, w, h);
    glClearColor(0.015f, 0.018f, 0.04f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_CULL_FACE); glDisable(GL_TEXTURE_2D); glDisable(GL_FOG);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (appState != APP_GAME || nb < 1) {                 /* hors partie : ecran d'attente */
        mapsMaxSpeed = 0;
        for (int i = 0; i <= MAXP; i++) mapsTrailN[i] = 0;
        maps_speed_reset();
        mapsHeatN = 0;
        text_c(2, w * 0.5f, h * 0.5f, "VELOCITY", 1, 1, 1, 1);
        text_c(0, w * 0.5f, h * 0.5f + 50, "la carte de la partie s'affichera ici", 0.6f, 0.7f, 0.85f, 0.6f + 0.4f * sinf((float)nowT * 2));
        return;
    }
    {                                                     /* nouveau match : chrono revenu en arriere */
        static float lastMatchT;
        if (dmMode && matchT + 1 < lastMatchT) { maps_speed_reset(); mapsMaxSpeed = 0; mapsHeatN = 0; }
        lastMatchT = matchT;
    }
    if (!P.dead) mapsMaxSpeed = fmaxf(mapsMaxSpeed, hlen(P.vel));
    maps_speed_tick();

    MapsPl pl[MAXP + 1];
    int n = maps_players(pl);
    maps_trail_tick(pl, n);

    float m = 40;                                         /* carte carree a gauche, panneau a droite (ou dessous si l'ecran est etroit) */
    int wide = w >= h * 1.25f;
    float S = wide ? fminf(h - 2 * m, w * 0.62f - 2 * m) : fminf(w - 2 * m, h * 0.58f - 2 * m);
    mapsMnX = mapBox.mn.x; mapsMnZ = mapBox.mn.z;
    float span = fmaxf(mapBox.mx.x - mapBox.mn.x, mapBox.mx.z - mapBox.mn.z);
    MapView v = { m, wide ? (h - S) * 0.5f : m, S / fmaxf(span, 1) };
    int heat = dmMode && matchOver;                       /* fin de match : carte de chaleur, toujours vue de dessus */
    if (mapView3d && !heat) maps_draw_3d(v.x0, v.y0, S, w, h, pl, n);
    else maps_draw_map(&v, S, pl, n);
    if (heat) maps_draw_heat(&v, S);
    if (wide) maps_draw_panel(m + S + 60, m, w - S - 3 * m - 60, h - 2 * m);
    else maps_draw_panel(m, m + S + 60, w - 2 * m, h - S - 3 * m - 60);
}
