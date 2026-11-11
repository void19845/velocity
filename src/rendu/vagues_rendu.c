/* VELOCITY - mode VAGUES : rendu des ennemis (sentinelle, essaim, bastion), des quatre boss et des effets
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   vg_draw_enemy est appele par draw_enemy (src/rendu/modeles.c) ; vg_draw_world_fx par render (passe additive). */

static void vg_tri(V3 a, V3 b, V3 c) {                    /* triangle avec sa normale */
    V3 n = vnorm(vcross(vsub(b, a), vsub(c, a)));
    glNormal3f(n.x, n.y, n.z);
    glVertex3f(a.x, a.y, a.z); glVertex3f(b.x, b.y, b.z); glVertex3f(c.x, c.y, c.z);
}
static void vg_line(V3 a, V3 b) { glVertex3f(a.x, a.y, a.z); glVertex3f(b.x, b.y, b.z); }
static void vg_ring(V3 c, float r, int n) {               /* cercle horizontal (dans un glBegin(GL_LINE_LOOP) a part) */
    glBegin(GL_LINE_LOOP);
    for (int k = 0; k < n; k++) { float a = k * 2 * PI / n; glVertex3f(c.x + cosf(a) * r, c.y, c.z + sinf(a) * r); }
    glEnd();
}
/* couleur d'un ennemi : flash blanc a la touche, glace s'il est gele, orange s'il brule */
static V3 vg_enemy_col(const Enemy *e) {
    V3 c = enemy_color(e->type);
    if (e->frozenT > 0) c = vlerp(c, v3(0.6f, 0.9f, 1), 0.7f);
    else if (e->burnT > 0) c = vlerp(c, v3(1, 0.5f, 0.1f), 0.35f + 0.25f * sinf(e->t * 30));
    if (e->flash > 0) c = v3(1, 1, 1);
    return c;
}
/* pyramide a base carree : base a y = -h0, sommet a y = h1, demi-cote s ; faces sombres, aretes vives */
static void vg_pyramid(float s, float h0, float h1, V3 col, int lit) {
    V3 b[4] = { v3(-s, -h0, -s), v3(s, -h0, -s), v3(s, -h0, s), v3(-s, -h0, s) }, top = v3(0, h1, 0);
    glColor3f(col.x * 0.22f, col.y * 0.22f, col.z * 0.22f);
    glBegin(GL_TRIANGLES);
    for (int k = 0; k < 4; k++) vg_tri(b[k], top, b[(k + 1) % 4]);
    vg_tri(b[0], b[1], b[2]); vg_tri(b[0], b[2], b[3]);
    glEnd();
    if (lit) glDisable(GL_LIGHTING);
    glColor3f(col.x, col.y, col.z);
    glLineWidth(3);
    glBegin(GL_LINES);
    for (int k = 0; k < 4; k++) { vg_line(b[k], b[(k + 1) % 4]); vg_line(b[k], top); }
    glEnd();
    glLineWidth(1);
    if (lit) glEnable(GL_LIGHTING);
}
/* tesseract : les 16 sommets du cube 4D tournent dans les plans XW, YW et ZW, projetes en perspective (4D -> 3D) */
static void vg_tesseract(float R, float t, V3 col, int lit) {
    V3 p[16]; float w[16];
    float a = t * 0.6f, b = t * 0.37f, c = t * 0.23f;
    for (int i = 0; i < 16; i++) {
        float x = i & 1 ? 1.0f : -1.0f, y = i & 2 ? 1.0f : -1.0f, z = i & 4 ? 1.0f : -1.0f, ww = i & 8 ? 1.0f : -1.0f;
        float x1 = x * cosf(a) - ww * sinf(a), w1 = x * sinf(a) + ww * cosf(a);
        float y1 = y * cosf(b) - w1 * sinf(b), w2 = y * sinf(b) + w1 * cosf(b);
        float z1 = z * cosf(c) - w2 * sinf(c), w3 = z * sinf(c) + w2 * cosf(c);
        float k = R * 1.0f / (2.4f - w3);                 /* cube exterieur ~0,7 R, interieur ~0,3 R */
        p[i] = v3(x1 * k, y1 * k, z1 * k); w[i] = w3;
    }
    if (lit) glDisable(GL_LIGHTING);
    glLineWidth(3);
    glBegin(GL_LINES);
    for (int i = 0; i < 16; i++)
        for (int bit = 1; bit < 16; bit <<= 1) {
            int j = i | bit;
            if (j == i) continue;
            float d = 0.55f + 0.45f * clampf((w[i] + w[j]) * 0.25f + 0.5f, 0, 1);   /* le cote "loin" en 4D plus sombre */
            glColor3f(col.x * d, col.y * d, col.z * d);
            vg_line(p[i], p[j]);
        }
    glEnd();
    glLineWidth(1);
    if (lit) glEnable(GL_LIGHTING);
    for (int i = 0; i < 16; i++) {                         /* sommets */
        float s = 0.18f + 0.06f * (w[i] + 1);
        glColor3f(col.x, col.y, col.z);
        box_faces(vsub(p[i], v3(s, s, s)), vadd(p[i], v3(s, s, s)));
    }
}
/* sphere a facettes (quadrillage lat / long) */
static void vg_sphere(float R, V3 col, int bands) {
    glBegin(GL_QUADS);
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 16; j++) {
            float t0 = PI * i / 8 - PI / 2, t1 = PI * (i + 1) / 8 - PI / 2, p0 = 2 * PI * j / 16, p1 = 2 * PI * (j + 1) / 16;
            V3 q[4] = { v3(cosf(t0) * cosf(p0), sinf(t0), cosf(t0) * sinf(p0)), v3(cosf(t0) * cosf(p1), sinf(t0), cosf(t0) * sinf(p1)),
                        v3(cosf(t1) * cosf(p1), sinf(t1), cosf(t1) * sinf(p1)), v3(cosf(t1) * cosf(p0), sinf(t1), cosf(t1) * sinf(p0)) };
            float k = ((i + j) & 1) ? 0.25f : 0.16f;
            if (bands && (i == 3 || i == 4) && (j & 3) == 0) k = 0.9f;   /* plaques lumineuses : on voit le roulement */
            glColor3f(col.x * k, col.y * k, col.z * k);
            V3 m = vnorm(vadd(q[0], q[2]));
            glNormal3f(m.x, m.y, m.z);
            for (int v = 0; v < 4; v++) glVertex3f(q[v].x * R, q[v].y * R, q[v].z * R);
        }
    glEnd();
}

static void vg_draw_enemy(Enemy *e) {
    V3 col = vg_enemy_col(e);
    int lit = glIsEnabled(GL_LIGHTING);
    float R = enemy_radius(e->type);
    glPushMatrix();
    glTranslatef(e->pos.x, e->pos.y, e->pos.z);
    switch (e->type) {
    case EN_SNIPER: {                                     /* octaedre allonge, anneau qui tourne */
        glRotatef(e->t * 40, 0, 1, 0);
        V3 v[6] = { v3(0, 1.1f, 0), v3(0, -1.1f, 0), v3(0.6f, 0, 0), v3(0, 0, 0.6f), v3(-0.6f, 0, 0), v3(0, 0, -0.6f) };
        glColor3f(col.x * 0.25f, col.y * 0.25f, col.z * 0.25f);
        glBegin(GL_TRIANGLES);
        for (int k = 0; k < 4; k++) { vg_tri(v[2 + k], v[0], v[2 + (k + 1) % 4]); vg_tri(v[2 + (k + 1) % 4], v[1], v[2 + k]); }
        glEnd();
        if (lit) glDisable(GL_LIGHTING);
        glColor3f(col.x, col.y, col.z);
        glLineWidth(2);
        glBegin(GL_LINES);
        for (int k = 0; k < 4; k++) { vg_line(v[2 + k], v[0]); vg_line(v[2 + k], v[1]); vg_line(v[2 + k], v[2 + (k + 1) % 4]); }
        glEnd();
        glRotatef(e->t * 160, 1, 0, 0);
        vg_ring(v3(0, 0, 0), 0.95f, 20);
        glLineWidth(1);
        if (lit) glEnable(GL_LIGHTING);
        break;
    }
    case EN_SWARM: {                                      /* petit tetraedre qui tourne vite */
        glRotatef(e->t * 500, 0.3f, 1, 0.2f);
        V3 v[4] = { v3(0, 0.55f, 0), v3(0.5f, -0.3f, 0.3f), v3(-0.5f, -0.3f, 0.3f), v3(0, -0.3f, -0.55f) };
        glColor3f(col.x * 0.8f, col.y * 0.8f, col.z * 0.8f);
        glBegin(GL_TRIANGLES);
        vg_tri(v[0], v[1], v[2]); vg_tri(v[0], v[2], v[3]); vg_tri(v[0], v[3], v[1]); vg_tri(v[1], v[3], v[2]);
        glEnd();
        break;
    }
    case EN_BOMBE: {                                      /* sphere qui gonfle (+30 %) et clignote de plus en plus vite pendant le tic */
        float k = e->st == 1 ? clampf(1 - e->stT / VG_BOMBE_FUSE, 0, 1) : 0;
        float pulse = e->st == 1 ? 0.5f + 0.5f * sinf(e->t * (10 + 40 * k)) : 0.3f;
        float sc = (1 + 0.3f * k) * (1 + 0.06f * pulse);
        V3 hot = vlerp(col, v3(1, 1, 0.85f), pulse * k);
        glRotatef(e->t * 90, 0.2f, 1, 0.1f);
        vg_sphere(R * sc, v3(hot.x * (1 + 2.5f * pulse * k), hot.y * (1 + 2.5f * pulse * k), hot.z * (1 + 2.5f * pulse * k)), 1);
        if (lit) glDisable(GL_LIGHTING);
        glColor3f(hot.x, hot.y, hot.z);
        glLineWidth(2);
        vg_ring(v3(0, 0, 0), R * sc * 1.08f, 20);
        glLineWidth(1);
        if (lit) glEnable(GL_LIGHTING);
        break;
    }
    case EN_CARRE:                                        /* mini tesseract (cube 4D qui tourne) */
        vg_tesseract(R * 1.15f, e->t * 1.4f, col, lit);
        break;
    case EN_MORTIER: {                                    /* caisson massif, canon tourne vers le joueur, pointe lumineuse pendant la charge */
        V3 d = e->st == 1 ? e->aim : vnorm(vsub(eye_pos(), e->pos));
        glRotatef(atan2f(d.x, d.z) * 180 / PI, 0, 1, 0);
        glRotatef(-asinf(clampf(d.y, -1, 1)) * 180 / PI, 1, 0, 0);
        glColor3f(col.x * 0.22f, col.y * 0.22f, col.z * 0.25f);
        box_faces(v3(-0.75f, -0.75f, -0.75f), v3(0.75f, 0.75f, 0.75f));
        box_faces(v3(-0.28f, -0.28f, 0.7f), v3(0.28f, 0.28f, 1.7f));
        if (lit) glDisable(GL_LIGHTING);
        glColor3f(col.x, col.y, col.z);
        glLineWidth(2);
        box_edges(v3(-0.77f, -0.77f, -0.77f), v3(0.77f, 0.77f, 0.77f));
        box_edges(v3(-0.3f, -0.3f, 0.7f), v3(0.3f, 0.3f, 1.72f));
        float ch = e->st == 1 ? clampf(1 - e->stT / 0.9f, 0, 1) : 0;
        glColor3f(1, 0.6f + 0.4f * ch, 1);
        box_faces(v3(-0.2f - 0.1f * ch, -0.2f - 0.1f * ch, 1.6f), v3(0.2f + 0.1f * ch, 0.2f + 0.1f * ch, 1.8f + 0.3f * ch));
        glLineWidth(1);
        if (lit) glEnable(GL_LIGHTING);
        break;
    }
    case EN_BASTION: {                                    /* corps massif, bouclier lumineux devant (cote de e->yaw) */
        glRotatef(e->yaw * 180 / PI, 0, 1, 0);
        glColor3f(col.x * 0.22f, col.y * 0.22f, col.z * 0.25f);
        box_faces(v3(-0.8f, -1.2f, -0.8f), v3(0.8f, 0.9f, 0.8f));
        if (lit) glDisable(GL_LIGHTING);
        glColor3f(col.x, col.y, col.z);
        glLineWidth(2);
        box_edges(v3(-0.82f, -1.22f, -0.82f), v3(0.82f, 0.92f, 0.82f));
        glColor3f(0.45f + 0.3f * (e->combo > 0), 0.6f, 1);  /* le bouclier (z positif = face) */
        box_faces(v3(-1.1f, -1.1f, 1.0f), v3(1.1f, 1.2f, 1.15f));
        glColor3f(1, 0.9f, 0.5f);
        box_faces(v3(-0.18f, 0.35f, 0.82f), v3(0.18f, 0.6f, 0.95f));   /* l'oeil, au-dessus du bouclier */
        glLineWidth(1);
        if (lit) glEnable(GL_LIGHTING);
        break;
    }
    case EN_PYRA: {
        float sc = vgBossScale;
        glScalef(sc, sc, sc);
        if (e->st == BS_STUN) glRotatef(sinf(e->t * 9) * 18, 1, 0, 0.4f);   /* etourdie : elle tangue */
        glRotatef(e->t * (e->st == BS_RAM ? 400.0f : 35.0f), 0, 1, 0);
        float s = enemyRad[EN_PYRA];
        if (e->summoned == 2) col = v3(col.x * 0.6f, col.y * 0.6f, col.z);   /* fantome : plus froid */
        vg_pyramid(s * 0.85f, s * 0.6f, s * 1.05f, col, lit);
        break;
    }
    case EN_TESS:
        glScalef(vgBossScale, vgBossScale, vgBossScale);
        vg_tesseract(enemyRad[EN_TESS], e->t, col, lit);
        break;
    case EN_BALL: {
        glScalef(vgBossScale, vgBossScale, vgBossScale);
        V3 hv = v3(e->vel.x, 0, e->vel.z), ax = hlen(hv) > 0.5f ? vnorm(vcross(v3(0, 1, 0), hv)) : v3(1, 0, 0);
        glRotatef(e->yaw * 180 / PI, ax.x, ax.y, ax.z);
        vg_sphere(enemyRad[EN_BALL], col, 1);
        break;
    }
    case EN_HEXA: {
        float sc = vgBossScale, s = enemyRad[EN_HEXA] * 0.62f, open = e->st == BS_OPEN ? 0.9f : e->st == BS_STUN ? 0.4f : 0;
        glScalef(sc, sc, sc);
        glRotatef(e->yaw * 57.3f, 0.3f, 1, 0.2f);
        static const float phaseCol[4][3] = { { 0.9f, 0.95f, 1 }, { 1, 0.62f, 0.2f }, { 0.55f, 0.45f, 1 }, { 1, 0.3f, 0.45f } };
        V3 pc = v3(phaseCol[e->phase & 3][0], phaseCol[e->phase & 3][1], phaseCol[e->phase & 3][2]);
        if (e->flash > 0) pc = v3(1, 1, 1);
        for (int f = 0; f < 6; f++) {                     /* six faces, ecartees quand le coeur est ouvert */
            int ax = f >> 1; float sg = f & 1 ? 1.0f : -1.0f;
            V3 lo = v3(-s, -s, -s), hi = v3(s, s, s);
            *vc(&lo, ax) = sg > 0 ? s * 0.8f : -s; *vc(&hi, ax) = sg > 0 ? s : -s * 0.8f;
            V3 off = v3(0, 0, 0); *vc(&off, ax) = sg * open * s;
            lo = vadd(lo, off); hi = vadd(hi, off);
            glColor3f(pc.x * 0.2f, pc.y * 0.2f, pc.z * 0.22f);
            box_faces(lo, hi);
            if (lit) glDisable(GL_LIGHTING);
            glColor3f(pc.x, pc.y, pc.z);
            glLineWidth(2.5f);
            box_edges(lo, hi);
            glLineWidth(1);
            if (lit) glEnable(GL_LIGHTING);
        }
        float k = 0.5f + 0.5f * sinf(e->t * 6), cs = s * (0.35f + 0.1f * k);   /* le coeur */
        if (lit) glDisable(GL_LIGHTING);
        glColor3f(1, 0.85f + 0.15f * k, 0.6f + 0.4f * open);
        box_faces(v3(-cs, -cs, -cs), v3(cs, cs, cs));
        if (lit) glEnable(GL_LIGHTING);
        break;
    }
    }
    glPopMatrix();
    (void)R;
}

/* passe additive : rayons visibles, ondes, impacts annonces, liens du tesseract, objets de la boutique, zone */
static void vg_draw_world_fx(void) {
    if (!vagOn) return;
    float t = (float)nowT;
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &E[i];
        if (!e->alive) continue;
        if (e->type == EN_SNIPER && e->st == 1) {         /* SENTINELLE : rayon de visee qui s'intensifie ; laser (phase 1) : rouge, plus epais, blanc une fois fige */
            float k = clampf(1 - e->stT / 1.4f, 0, 1), len = ray_world(e->pos, e->aim, 120, 0);
            int las = e->phase == 1;
            glLineWidth(1 + (las ? 5 : 3) * k);
            glBegin(GL_LINES);
            if (las && e->stT <= 0.35f) glColor4f(1, 0.92f, 0.92f, 0.95f);
            else if (las) glColor4f(1, 0.35f, 0.4f, 0.25f + 0.6f * k);
            else glColor4f(0.4f, 1, 0.5f, 0.25f + 0.6f * k);
            vg_line(e->pos, vadd(e->pos, vmul(e->aim, len)));
            glEnd();
        }
        if ((e->type == EN_PYRA || e->type == EN_HEXA) && e->st == BS_AIM) {   /* charge annoncee : ligne rouge */
            float k = clampf(1 - e->stT / 0.9f, 0, 1), len = fminf(ray_world(e->pos, e->aim, 60, 0), 60);
            glLineWidth(2 + 6 * k);
            glBegin(GL_LINES);
            glColor4f(1, 0.2f, 0.15f, 0.3f + 0.6f * k * (0.6f + 0.4f * sinf(t * 30)));
            vg_line(e->pos, vadd(e->pos, vmul(e->aim, len)));
            glEnd();
        }
        if (e->type == EN_BOMBE && e->st == 1) {          /* zone d'explosion (VG_BOMBE_R) : anneau au sol + anneau qui se resserre */
            float k = clampf(e->stT / VG_BOMBE_FUSE, 0, 1), gy = ground_below(e->pos, 30) + 0.12f;
            float fl = 0.5f + 0.5f * sinf(t * (10 + 40 * (1 - k)));
            glLineWidth(3);
            glColor4f(1, 0.3f, 0.6f, 0.5f + 0.4f * fl);
            vg_ring(v3(e->pos.x, gy, e->pos.z), VG_BOMBE_R, 48);
            glColor4f(1, 0.8f, 0.9f, 0.4f);
            vg_ring(v3(e->pos.x, gy, e->pos.z), VG_BOMBE_R * (1 - k), 48);
            vg_ring(e->pos, VG_BOMBE_R, 48);              /* equateur de la sphere de souffle */
        }
        if (e->type == EN_CARRE) {                        /* liens vers les ennemis proteges */
            glLineWidth(2);
            glBegin(GL_LINES);
            for (int j = 0; j < MAXE; j++) {
                if (!E[j].alive || E[j].type == EN_CARRE || EN_BOSS(E[j].type)) continue;
                glColor4f(0.3f, 0.9f, 1, 0.55f); glVertex3f(e->pos.x, e->pos.y, e->pos.z);
                glColor4f(0.3f, 0.9f, 1, 0.1f); glVertex3f(E[j].pos.x, E[j].pos.y, E[j].pos.z);
            }
            glEnd();
        }
        if (e->elite) {                                   /* elite : couronne doree */
            glLineWidth(2); glColor4f(1, 0.85f, 0.3f, 0.7f);
            vg_ring(vadd(e->pos, v3(0, enemy_radius(e->type) + 0.35f, 0)), 0.45f, 14);
        }
        if (e->type == EN_TESS && boss_links(i) >= VG_LINKS) {   /* liens vers les invocations : le bouclier */
            glLineWidth(2);
            glBegin(GL_LINES);
            for (int j = 0; j < MAXE; j++) {
                if (!E[j].alive || E[j].summoned != 1) continue;
                glColor4f(0.6f, 0.5f, 1, 0.6f); glVertex3f(e->pos.x, e->pos.y, e->pos.z);
                glColor4f(0.6f, 0.5f, 1, 0.1f); glVertex3f(E[j].pos.x, E[j].pos.y, E[j].pos.z);
            }
            glEnd();
        }
    }
    for (int k = 0; k < MAXMB; k++) {                     /* MORTIER : orbe a pulsation, sphere de souffle qui bat de plus en plus vite */
        MortOrb *m = &MB[k];
        if (!m->on) continue;
        float u = 1 - clampf(m->t / VG_MORTIER_LIFE, 0, 1), pu = 0.5f + 0.5f * sinf(t * (6 + 30 * u));
        glLineWidth(2 + 2 * pu);
        for (int ax = 0; ax < 3; ax++) {
            glColor4f(0.8f, 0.35f, 1, 0.15f + 0.35f * pu * (0.4f + u));
            glBegin(GL_LINE_LOOP);
            for (int a = 0; a < 40; a++) {
                float c = cosf(a * 2 * PI / 40), s = sinf(a * 2 * PI / 40), r = VG_MORTIER_R * (0.97f + 0.03f * pu);
                V3 o = ax == 0 ? v3(c, s, 0) : ax == 1 ? v3(c, 0, s) : v3(0, c, s);
                glVertex3f(m->pos.x + o.x * r, m->pos.y + o.y * r, m->pos.z + o.z * r);
            }
            glEnd();
        }
        glColor4f(1, 0.7f, 1, 0.9f);
        for (int ax = 0; ax < 2; ax++) {                  /* le noyau */
            glBegin(GL_LINE_LOOP);
            for (int a = 0; a < 16; a++) {
                float c = cosf(a * 2 * PI / 16) * (0.5f + 0.2f * pu), s = sinf(a * 2 * PI / 16) * (0.5f + 0.2f * pu);
                glVertex3f(m->pos.x + c, m->pos.y + (ax ? s : 0), m->pos.z + (ax ? 0 : s));
            }
            glEnd();
        }
    }
    glLineWidth(4);                                       /* ondes de choc au sol */
    for (int k = 0; k < MAXGW; k++) {
        GWave *g = &GW[k];
        if (!g->on) continue;
        float a = 1 - g->r / g->max;
        glColor4f(1, 0.35f, 0.4f, 0.9f * a);
        vg_ring(v3(g->c.x, g->c.y + 0.15f, g->c.z), g->r, 64);
        glColor4f(1, 0.7f, 0.6f, 0.5f * a);
        vg_ring(v3(g->c.x, g->c.y + 0.9f, g->c.z), g->r, 64);   /* hauteur de l'onde : il faut sauter plus haut */
    }
    for (int k = 0; k < MAXMT; k++) {                     /* METEORES : cercle qui se resserre */
        Meteor *m = &MT[k];
        if (!m->on) continue;
        float f = clampf(m->t / m->max, 0, 1);
        glColor4f(1, 0.25f, 0.3f, 0.9f - 0.5f * f);
        vg_ring(v3(m->c.x, m->c.y + 0.1f, m->c.z), 4.5f, 40);
        vg_ring(v3(m->c.x, m->c.y + 0.1f, m->c.z), 4.5f * f, 40);
    }
    for (int k = 0; k < MAXLZ; k++) {                     /* rayons de l'hexaedre : fil rouge annonce, puis le tir */
        Laser *l = &LZ[k];
        if (!l->on) continue;
        float len = l->fired ? l->len : ray_world(l->a, l->dir, 150, 0);
        V3 b = vadd(l->a, vmul(l->dir, len));
        glBegin(GL_LINES);
        if (!l->fired) { glLineWidth(1.5f); glColor4f(1, 0.2f, 0.2f, 0.5f + 0.4f * sinf(t * 40)); }
        else { glLineWidth(9); glColor4f(1, 0.95f, 0.8f, l->life / 0.25f); }
        vg_line(l->a, b);
        glEnd();
    }
    if (SWP.on) {                                         /* DEPLIAGE : quatre rayons au ras du sol */
        glLineWidth(SWP.tele > 0 ? 2.0f : 7.0f);
        glBegin(GL_LINES);
        for (int b = 0; b < 4; b++) {
            float a = SWP.ang + b * PI * 0.5f;
            V3 p0 = v3(SWP.c.x, SWP.y, SWP.c.z), p1 = vadd(p0, v3(cosf(a) * 60, 0, sinf(a) * 60));
            glColor4f(1, 0.3f, 0.3f, SWP.tele > 0 ? 0.4f + 0.3f * sinf(t * 30) : 0.9f);
            vg_line(p0, p1);
        }
        glEnd();
    }
    if (HYP.on) {                                         /* HYPERCUBE : la sphere d'orbes s'annonce */
        glPointSize(6);
        glBegin(GL_POINTS);
        for (int k = 0; k < 26; k++) {
            float u = (k + 0.5f) / 26, th = acosf(1 - 2 * u), ph = k * 2.39996f;
            V3 d = v3(sinf(th) * cosf(ph), cosf(th), sinf(th) * sinf(ph)), q = vadd(HYP.c, vmul(d, 15));
            glColor4f(1, 0.3f, 0.9f, 0.5f + 0.5f * sinf(t * 25)); glVertex3f(q.x, q.y, q.z);
        }
        glEnd();
        glPointSize(1);
    }
    glLineWidth(2);                                       /* mines et spectres de la boutique */
    for (int k = 0; k < MAXVM; k++) {
        if (!VM[k].on) continue;
        float p = 0.5f + 0.5f * sinf(t * (VM[k].arm > 0 ? 30.0f : 6.0f));
        glColor4f(1, 0.5f, 0.2f, 0.5f + 0.4f * p);
        vg_ring(VM[k].pos, 0.5f, 12); vg_ring(VM[k].pos, 3.2f, 32);
    }
    if (vgState == VG_CHALLENGE && vgChal == CH_DEFENSE) {   /* zone a tenir */
        V3 z = vg_zone();
        float r = vg_zone_r(), in = (float)vg_in_zone();
        glLineWidth(4);
        glColor4f(0.3f + 0.6f * in, 1, 0.5f, 0.6f + 0.3f * sinf(t * 4));
        vg_ring(v3(z.x, z.y + 0.08f, z.z), r, 48);
        glLineWidth(2);
        glBegin(GL_LINES);
        glColor4f(0.3f, 1, 0.5f, 0.5f); glVertex3f(z.x, z.y, z.z);
        glColor4f(0.3f, 1, 0.5f, 0.0f); glVertex3f(z.x, z.y + 30, z.z);
        glEnd();
    }
    glLineWidth(1);
    glEnable(GL_TEXTURE_2D);                              /* halos : eclats, spectres, pointe de la pyramide */
    glBindTexture(GL_TEXTURE_2D, texDot);
    glBegin(GL_QUADS);
    for (int k = 0; k < MAXVS; k++) if (VS[k].on) { glColor4f(1, 0.85f, 0.4f, 1); billboard(VS[k].pos, 0.35f); }
    V3 pc = pcenter();
    for (int k = 0; k < MAXVP; k++) {
        if (!VP[k].on) continue;
        V3 at = vadd(pc, v3(cosf(VP[k].ang) * 1.8f, 0.4f, sinf(VP[k].ang) * 1.8f));
        glColor4f(0.6f, 0.85f, 1, 0.8f); billboard(at, 0.45f);
    }
    for (int k = 0; k < MAXVV; k++) if (VV[k].on) { glColor4f(0.7f, 0.4f, 1, 0.5f * VV[k].life); billboard(VV[k].pos, 3.0f); }
    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &E[i];
        if (!e->alive) continue;
        if (e->type == EN_PYRA && e->st == BS_CHARGE) {
            float k = clampf(1 - e->stT / 2.5f, 0, 1);
            glColor4f(1, 0.8f, 0.35f, 0.4f + 0.6f * k);
            billboard(vadd(e->pos, v3(0, enemy_radius(e->type) * 0.95f, 0)), 0.6f + 2.4f * k);
        }
        if (e->type == EN_HEXA && e->st == BS_CHARGE) {
            float k = clampf(1 - e->stT / 1.8f, 0, 1);
            glColor4f(1, 0.7f, 0.3f, 0.5f + 0.5f * k); billboard(e->pos, 1 + 3 * k);
        }
        if (e->frozenT > 0) { glColor4f(0.5f, 0.85f, 1, 0.35f); billboard(e->pos, enemy_radius(e->type) * 1.8f); }
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
}
