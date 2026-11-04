/* VELOCITY - billboards, camera, ennemis, marteau
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static void billboard(V3 p, float s) {
    V3 r = vmul(camR, s), u = vmul(camU, s);
    glTexCoord2f(0, 0); glVertex3f(p.x - r.x - u.x, p.y - r.y - u.y, p.z - r.z - u.z);
    glTexCoord2f(1, 0); glVertex3f(p.x + r.x - u.x, p.y + r.y - u.y, p.z + r.z - u.z);
    glTexCoord2f(1, 1); glVertex3f(p.x + r.x + u.x, p.y + r.y + u.y, p.z + r.z + u.z);
    glTexCoord2f(0, 1); glVertex3f(p.x - r.x + u.x, p.y - r.y + u.y, p.z - r.z + u.z);
}

static void set_frustum(float vfovDeg, float aspect, float zn, float zf) {
    float t = zn * tanf(vfovDeg * PI / 360.0f), r = t * aspect;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-r, r, -t, t, zn, zf);
}


static void vg_draw_enemy(Enemy *e);                      /* src/rendu/vagues_rendu.c : types du mode VAGUES et boss */
static void draw_enemy(Enemy *e) {
    if (e->type >= EN_SNIPER) { vg_draw_enemy(e); return; }
    V3 col = e->flash > 0 ? v3(1, 1, 1) : enemy_color(e->type);
    if (e->frozenT > 0) col = vlerp(col, v3(0.6f, 0.9f, 1), 0.7f);   /* GEL (boutique) */
    glPushMatrix();
    glTranslatef(e->pos.x, e->pos.y, e->pos.z);
    if (e->type == EN_DRONE) {
        glRotatef(e->t * 70, 0, 1, 0);
        glRotatef(e->t * 35, 1, 0, 0.3f);
        glColor3f(col.x * 0.25f, col.y * 0.25f, col.z * 0.25f);
        box_faces(v3(-0.5f, -0.5f, -0.5f), v3(0.5f, 0.5f, 0.5f));
        glColor3f(col.x, col.y, col.z);
        glLineWidth(2);
        box_edges(v3(-0.52f, -0.52f, -0.52f), v3(0.52f, 0.52f, 0.52f));
        box_faces(v3(-0.22f, -0.22f, -0.22f), v3(0.22f, 0.22f, 0.22f));
        glRotatef(45, 0, 1, 0);
        box_edges(v3(-0.75f, -0.08f, -0.75f), v3(0.75f, 0.08f, 0.75f));
    } else {
        float s = 0.6f + 0.1f * sinf(e->t * 20);
        glRotatef(e->t * 400, 0, 1, 0);
        glScalef(s, s, s);
        glBegin(GL_TRIANGLES);
        for (int k = 0; k < 8; k++) {
            float sx = (k & 1) ? 1.0f : -1.0f, sy = (k & 2) ? 1.0f : -1.0f, sz = (k & 4) ? 1.0f : -1.0f;
            float sh = (k & 2) ? 1.0f : 0.6f;
            glColor3f(col.x * sh, col.y * sh, col.z * sh);
            glVertex3f(sx, 0, 0); glVertex3f(0, sy * 1.2f, 0); glVertex3f(0, 0, sz);
        }
        glEnd();
    }
    glPopMatrix();
}

/* Enclume : manche le long de -Z, tete au bout ; la bande chauffe du bleu a l'orange (heat 0..1) */
static void hammer_model(float heat) {
    glColor3f(0.22f, 0.2f, 0.19f);
    box_faces(v3(-0.03f, -0.03f, -0.9f), v3(0.03f, 0.03f, 0.1f));
    glColor3f(0.5f, 0.52f, 0.56f);
    box_faces(v3(-0.17f, -0.11f, -1.12f), v3(0.17f, 0.11f, -0.88f));
    glDisable(GL_LIGHTING);
    glColor3f(0.35f + 0.65f * heat, 0.6f - 0.05f * heat, 1.0f - 0.85f * heat);
    box_faces(v3(-0.176f, -0.116f, -1.03f), v3(0.176f, 0.116f, -0.97f));
    glEnable(GL_LIGHTING);
}

/* viewmodel de l'Enclume : bas a gauche au repos, leve pendant le pique, abattu par le coup au sol */
static void draw_anvil_vm(void) {
    float ang = 20, drop = 0, shk = 0, heat = 0;
    if (P.anvilState == ANVIL_DIVE) {
        ang = 65; shk = sinf(gameTime * 60) * 0.02f;
        heat = clampf((-P.vel.y - 25) / 20, 0.2f, 1);
    } else if (P.anvilState == ANVIL_STUN) {
        ang = -35; drop = 0.3f * clampf(P.anvilStunT / 0.15f, 0, 1);
    } else if (P.meleeT > 0) {
        float k = clampf(1 - P.meleeT / ANVIL_SWING_T, 0, 1);
        ang = 100 - 135 * k * k;
        heat = clampf(vlen(P.vel) / 30, 0, 1) * 0.6f;
    }
    glPushMatrix();
    glTranslatef(-0.7f + shk, -0.25f - drop, -0.2f);
    glRotatef(ang, 1, 0, 0);
    glRotatef(12, 0, 0, 1);
    glScalef(0.6f, 0.6f, 0.6f);
    hammer_model(heat);
    glPopMatrix();
}
