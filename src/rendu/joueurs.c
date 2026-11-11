/* VELOCITY - joueurs distants, scies et mines en monde, trainees, contours
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* Scie : disques en monde */
static const float sawWhite[3] = { 0.9f, 0.9f, 0.9f };
/* scies en monde (passe opaque, sans eclairage) ; une scie plantee vibre a l'impact */
static void draw_saws(void) {
    for (int i = 0; i < MAXPR; i++) {
        Proj *p = &PR[i];
        if (!p->alive || p->kind != PR_SAW || p->sawState == SAW_LOST) continue;
        V3 ax = p->sawAxis;
        if (p->sawState == SAW_PLANTED) {
            float ang = 0.15f * sinf(p->sawT * 60) * expf(-p->sawT * 8), c = cosf(ang), sn = sinf(ang);
            V3 k = p->sawN;                               /* rotation de l'axe autour de la normale (Rodrigues) */
            ax = vadd(vadd(vmul(ax, c), vmul(vcross(k, ax), sn)), vmul(k, vdot(k, ax) * (1 - c)));
        }
        draw_saw_disc(p->pos, ax, SAW_RADIUS, p->sawSpin, saw_col(p), p->owner >= 0 && p->owner < MAXP ? pcolor(p->owner) : sawWhite);
    }
}

/* --- Ventouse : rendu --- */
/* passe opaque : petit cube sombre, bande de couleur (cyan : la mienne, rouge : adverse), ventouse contre la face */
static void draw_mines(void) {
    glEnable(GL_LIGHTING);
    for (int i = 0; i < MAXPR; i++) {
        Proj *p = &PR[i];
        if (!p->alive || p->kind != PR_MINE) continue;
        V3 c = mine_col(p);
        glPushMatrix();
        glTranslatef(p->pos.x, p->pos.y, p->pos.z);
        if (p->mineState == MS_FLY) glRotatef(p->mineT * 540, 1, 0.3f, 0.2f);
        glColor3f(0.22f, 0.22f, 0.25f);
        box_faces(v3(-0.11f, -0.11f, -0.11f), v3(0.11f, 0.11f, 0.11f));
        glColor3f(c.x * 0.8f, c.y * 0.8f, c.z * 0.8f);
        box_faces(v3(-0.115f, -0.03f, -0.115f), v3(0.115f, 0.03f, 0.115f));
        if (p->mineState == MS_WORLD) {                   /* disque "ventouse" aplati contre la face (normales du decor : axes) */
            V3 n = p->mineN, h, m = vmul(n, -0.12f);
            h = v3(fabsf(n.x) > 0.5f ? 0.02f : 0.15f, fabsf(n.y) > 0.5f ? 0.02f : 0.15f, fabsf(n.z) > 0.5f ? 0.02f : 0.15f);
            glColor3f(0.12f, 0.12f, 0.14f);
            box_faces(vsub(m, h), vadd(m, h));
        }
        glPopMatrix();
    }
    glDisable(GL_LIGHTING);
}
/* passe additive (dans un glBegin(GL_QUADS)) : voyant qui clignote de 2 Hz au collage a 12 Hz en fin de meche */
static void draw_mine_glow(const Proj *p) {
    V3 c = mine_col(p);
    if (p->mineState == MS_FLY) { glColor4f(c.x, c.y, c.z, 0.5f); billboard(p->pos, 0.25f); return; }
    float k = clampf(p->mineT / MINE_FUSE, 0, 1), hz = 2 + 10 * k;
    int lit = fmodf(p->mineT * hz, 1.0f) < 0.5f;
    glColor4f(c.x, c.y, c.z, lit ? 0.95f : 0.25f);
    billboard(p->pos, lit ? 0.35f : 0.22f);
}
/* --- fin Ventouse : rendu --- */

static SkinAnim np_anim(int i) {
    NetPlayer *n = &NP[i];
    SkinAnim a;
    float hs = sqrtf(n->st.vel[0] * n->st.vel[0] + n->st.vel[2] * n->st.vel[2]);
    a.spd = clampf(hs / 10, 0, 1);
    a.run = n->runPh;
    a.air = fabsf(n->st.vel[1]) > 2.5f && !(n->st.flags & NF_WALLRUN) ? 1.0f : 0.0f;
    a.slide = (n->st.flags & NF_SLIDE) ? 1.0f : 0.0f;
    a.pitch = n->st.pitch;
    a.t = (float)nowT + i * 1.3f;
    return a;
}

/* trainee de vitesse : ruban colore derriere les joueurs rapides (passe additive) */
static void trail_tick(int i, float dt) {
    NetPlayer *n = &NP[i];
    float hs = sqrtf(n->st.vel[0] * n->st.vel[0] + n->st.vel[2] * n->st.vel[2] + n->st.vel[1] * n->st.vel[1]);
    for (int k = 0; k < 16; k++) n->trailAge[k] += dt;
    n->runPh += dt * clampf(sqrtf(n->st.vel[0] * n->st.vel[0] + n->st.vel[2] * n->st.vel[2]), 0, 14) * 1.25f;
    if ((n->trailAcc += dt) < 0.025f) return;
    n->trailAcc = 0;
    n->trailHead = (n->trailHead + 1) % 16;
    n->trail[n->trailHead] = vadd(n->disp, v3(0, np_height(i) * 0.55f, 0));
    n->trailAge[n->trailHead] = hs > gxTrail[0] ? 0.0f : 99.0f;   /* trop lent : pas de ruban */
}
static void draw_trails(void) {
    if (gxTrail[1] <= 0) return;
    for (int i = 0; dmMode && i < MAXP; i++) {
        if (!np_present(i)) continue;
        NetPlayer *n = &NP[i];
        const float *c = pcolor(i);
        glBegin(GL_QUADS);
        for (int k = 0; k < 15; k++) {
            int a0 = (n->trailHead - k + 16) % 16, a1 = (n->trailHead - k - 1 + 16) % 16;
            float al0 = clampf(1 - n->trailAge[a0] / 0.4f, 0, 1) * (1 - k / 15.0f), al1 = clampf(1 - n->trailAge[a1] / 0.4f, 0, 1) * (1 - (k + 1) / 15.0f);
            if (al0 <= 0 || al1 <= 0) continue;
            V3 p0 = n->trail[a0], p1 = n->trail[a1], d = vsub(p0, p1);
            if (vlen(d) < 0.01f || vlen(d) > 6) continue;
            V3 side = vmul(vnorm(vcross(d, vsub(camEye, p0))), gxTrail[2] * 0.5f);
            glColor4f(c[0], c[1], c[2], al0 * gxTrail[1]);
            glVertex3f(p0.x - side.x, p0.y - side.y, p0.z - side.z); glVertex3f(p0.x + side.x, p0.y + side.y, p0.z + side.z);
            glColor4f(c[0], c[1], c[2], al1 * gxTrail[1]);
            glVertex3f(p1.x + side.x, p1.y + side.y, p1.z + side.z); glVertex3f(p1.x - side.x, p1.y - side.y, p1.z - side.z);
        }
        glEnd();
    }
}

/* contour de chaque joueur dans sa couleur, lisible de loin : le modele un peu agrandi, faces arriere seules
 * (coque inversee ; pas de glPolygonMode : sur macOS, un mode different avant/arriere passe en rendu logiciel) */
static void draw_outlines(void) {
    if (gxOutline <= 0) return;
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
    glEnable(GL_CULL_FACE); glCullFace(GL_FRONT);
    skinFlat = 1;
    for (int i = 0; dmMode && i < MAXP; i++) {
        if (!np_present(i)) continue;
        NetPlayer *n = &NP[i];
        const float *c = pcolor(i);
        SkinAnim a = np_anim(i);
        skinFlatCol[0] = c[0]; skinFlatCol[1] = c[1]; skinFlatCol[2] = c[2];
        glPushMatrix();
        glTranslatef(n->disp.x, n->disp.y, n->disp.z);
        glRotatef(-n->st.yaw * 180 / PI, 0, 1, 0);
        if (n->st.flags & NF_SLIDE) glRotatef(-25, 1, 0, 0);
        float k = 1 + 0.025f * gxOutline, hh = np_height(i) * 0.5f;   /* grossi autour du centre du corps */
        glTranslatef(0, hh, 0); glScalef(k, 1 + 0.012f * gxOutline, k); glTranslatef(0, -hh, 0);
        draw_skin(n->st.skin < SKN_COUNT ? n->st.skin : 0, np_height(i), c, &a);
        glPopMatrix();
    }
    skinFlat = 0;
    glCullFace(GL_BACK); glDisable(GL_CULL_FACE);
}

static void draw_remote_players(void) {
    glEnable(GL_LIGHTING);
    for (int i = 0; i < MAXP; i++) {
        if (!np_present(i)) continue;
        NetPlayer *n = &NP[i];
        float h = np_height(i);
        const float *c = pcolor(i);
        set_dyn_light(sun_vis(np_center(i)));             /* plus sombre a l'ombre d'un mur */
        glPushMatrix();
        glTranslatef(n->disp.x, n->disp.y, n->disp.z);
        glRotatef(-n->st.yaw * 180 / PI, 0, 1, 0);
        if (n->st.flags & NF_SLIDE) glRotatef(-25, 1, 0, 0);
        if (n->st.pad & ST_SLAM) glRotatef(-15, 1, 0, 0);  /* pique de l'Enclume : penche vers l'avant */
        {                                                 /* le skin choisi par le joueur, a la couleur de son equipe */
            SkinAnim an = np_anim(i);
            skinFlash = n->flashT > 0 ? 1.0f : 0.0f;
            draw_skin(n->st.skin < SKN_COUNT ? n->st.skin : 0, h, c, &an);
            skinFlash = 0;
        }
        glPushMatrix();
        glTranslatef(0.42f, h - 0.6f, -0.1f);
        glRotatef(n->st.pitch * 180 / PI, 1, 0, 0);
        { const AmmoDef *wd = &ammoDef[n->st.weapon < WP_COUNT ? n->st.weapon : WP_RAIL]; glColor3f(wd->r * 0.9f, wd->g * 0.9f, wd->b * 0.9f); }
        if (n->st.weapon == WP_KINETIC) glColor3f(0.95f, 0.3f, 0.85f);   /* Elan : magenta, comme son rayon */
        if (n->st.weapon == WP_SHOTGUN) {                 /* SHOTGUN : court, large, cuivre, recul au tir */
            glColor3f(0.85f, 0.5f, 0.22f);
            glTranslatef(0, 0, 0.15f * clampf(n->sgFireT / 0.12f, 0, 1));
            box_faces(v3(-0.08f, -0.07f, -0.55f), v3(0.08f, 0.07f, 0.05f));
        } else if (n->st.weapon == WP_SAW) {                     /* Scie : petit disque dans la main, rien si elle est dehors */
            if (!saw_out_of(i)) {
                glDisable(GL_LIGHTING);
                draw_saw_disc(v3(0, 0, -0.35f), v3(0, 1, 0), 0.22f, gameTime * 3, v3(0.78f, 0.8f, 0.86f), c);
                glEnable(GL_LIGHTING);
            }
        } else if (n->st.weapon == WP_TESLA) {            /* Tesla : corps bleu nuit, deux fourches (lueur : draw_tesla_arcs) */
            glColor3f(0.2f, 0.25f, 0.45f);
            box_faces(v3(-0.06f, -0.06f, -0.55f), v3(0.06f, 0.06f, 0.05f));
            glColor3f(0.5f, 0.8f, 1.0f);
            box_faces(v3(-0.05f, -0.015f, -0.7f), v3(-0.02f, 0.015f, -0.55f));
            box_faces(v3(0.02f, -0.015f, -0.7f), v3(0.05f, 0.015f, -0.55f));
        } else box_faces(v3(-0.06f, -0.06f, -0.7f), v3(0.06f, 0.06f, 0.05f));
        glPopMatrix();
        if ((n->st.pad & 15) == MK_ANVIL) {               /* Enclume : leve pendant le pique, abattu par le coup */
            int slam = (n->st.pad & ST_SLAM) != 0;
            float ang = 20;
            if (slam) ang = 150;
            else if (n->st.melee) { float k = 1 - n->st.melee / 255.0f; ang = 100 - 135 * k * k; }
            glPushMatrix();
            glTranslatef(-0.4f, h - 0.6f, -0.1f);
            glRotatef(ang, 1, 0, 0);
            hammer_model(slam ? 1.0f : 0.0f);
            glPopMatrix();
        } else if ((n->st.pad & 15) == MK_LANCE) {        /* Lance : couchee (pointe orange = signal pour parer), sinon au flanc */
            int couch = (n->st.pad & ST_LANCE) != 0;
            float k = n->st.melee ? 1 - n->st.melee / 255.0f : 0;
            glPushMatrix();
            if (couch || n->st.melee) {                   /* horizontale, suit la visee ; estoc : translation avant */
                glTranslatef(-0.35f, h - 0.65f, -0.1f - (couch ? 0 : 0.6f * sinf(PI * k)));
                glRotatef(n->st.pitch * 180 / PI, 1, 0, 0);
            } else {                                      /* levee, verticale au flanc */
                glTranslatef(-0.45f, 0.45f, 0.05f);
                glRotatef(80, 1, 0, 0);
            }
            float pl = 0.75f + 0.25f * sinf(gameTime * 18);
            lance_model(2.6f, couch ? v3(1, 0.55f * pl, 0.15f) : v3(0.75f, 0.78f, 0.85f), couch ? gameTime : -1, 0);
            if (couch) {                                  /* halo additif autour de la pointe */
                glDisable(GL_LIGHTING);
                glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
                float s = 0.12f + 0.05f * pl;
                glColor4f(1, 0.5f, 0.1f, 0.55f);
                box_faces(v3(-s, -s, -2.6f - s), v3(s, s, -2.6f + 0.3f));
                glDepthMask(GL_TRUE); glDisable(GL_BLEND);
                glEnable(GL_LIGHTING);
            }
            glPopMatrix();
        } else if ((n->st.pad & 15) == MK_FLAIL) {        /* Fleau : manche a la main gauche ; au repos, tete pendue */
            int out = n->st.melee || flail_pulling(i);    /* coup ou traction en cours : la chaine est dans le monde (draw_flails) */
            glPushMatrix();
            glTranslatef(-0.4f, h - 0.6f, -0.1f);
            glRotatef(out ? n->st.pitch * 180 / PI - 60 : -20, 1, 0, 0);
            glColor3f(0.36f, 0.26f, 0.17f);
            box_faces(v3(-0.03f, -0.03f, -0.35f), v3(0.03f, 0.03f, 0.02f));
            if (!out) {                                   /* pendant le coup, la chaine est dans le monde (draw_flails) */
                glColor3f(0.30f, 0.31f, 0.35f);
                box_faces(v3(-0.012f, -0.32f, -0.36f), v3(0.012f, 0, -0.33f));
                flail_head_model(v3(0, -0.38f, -0.345f), 0.08f);
            }
            glPopMatrix();
        } else if ((n->st.pad & 15) == MK_FIST) {         /* --- Poings : deux gants, celui du coup en cours avance --- */
            int stp = n->st.melee ? (n->st.melee >> 6) + 1 : 0;   /* 1, 2, 3 : gauche, droit, gauche (uppercut) */
            float e = n->st.melee ? sinf(PI * (n->st.melee & 63) / 63.0f) : 0;
            for (int g = 0; g < 2; g++) {                 /* 0 = gauche (-x), 1 = droit */
                int on = (g == 0 && (stp == 1 || stp == 3)) || (g == 1 && stp == 2);
                glPushMatrix();
                glTranslatef(g ? 0.38f : -0.38f, h - 0.75f + (on && stp == 3 ? 0.5f * e : 0), -0.25f - (on ? 0.7f * e : 0));
                glColor3f(0.55f, 0.58f, 0.63f);
                box_faces(v3(-0.07f, -0.07f, -0.07f), v3(0.07f, 0.07f, 0.07f));
                glDisable(GL_LIGHTING);
                glColor3f(c[0], c[1], c[2]);              /* bobine de la couleur du joueur */
                box_faces(v3(-0.075f, -0.02f, -0.06f), v3(0.075f, 0.02f, -0.02f));
                glEnable(GL_LIGHTING);
                glPopMatrix();
            }                                             /* --- fin Poings --- */
        } else if ((n->st.pad & 15) == MK_SHIELD) {       /* --- Egide : leve devant le torse (bord = jauge), sinon au flanc --- */
            V3 pc = v3(c[0] * 0.6f, c[1] * 0.6f, c[2] * 0.6f);
            if (n->st.pad & ST_SHIELD) {
                float push = n->st.melee ? 0.25f * sinf(PI * (1 - n->st.melee / 255.0f)) : 0;
                glPushMatrix();
                glTranslatef(0, (0.3f + h - 0.25f) * 0.5f, -0.48f - push);
                shield_plate(0.9f, h - 0.55f, 0.04f, pc, 1, (n->st.flags & NF_PARRY) ? v3(1, 1, 1) : shield_col(n->shieldGauge),
                             n->shieldGauge);
                glPopMatrix();
            } else if (n->shieldBrokenT <= 0) {           /* baisse (brise : rien) */
                float push = n->st.melee ? 0.5f * sinf(PI * (1 - n->st.melee / 255.0f)) : 0;   /* coup de bouclier */
                glPushMatrix();
                glTranslatef(-0.42f, h * 0.5f, -0.05f - push);
                glRotatef(push > 0 ? 0.0f : 75.0f, 0, 1, 0);
                shield_plate(0.5f, 0.7f, 0.04f, pc, 1, shield_col(n->shieldGauge), n->shieldGauge);
                glPopMatrix();
            }
        } else if (n->st.melee) {                         /* epee en plein balayage */
            float k = 1 - n->st.melee / 255.0f;
            glPushMatrix();
            glTranslatef(-0.4f, h - 0.6f, -0.1f);
            glRotatef(-80 + k * 160, 0, 1, 0);
            glColor3f(0.85f, 0.88f, 0.95f);
            box_faces(v3(-0.04f, -0.01f, -1.3f), v3(0.04f, 0.01f, 0));
            glPopMatrix();
        }
        glPopMatrix();
    }
    glDisable(GL_LIGHTING);
    glLineWidth(3);                                       /* cables des grappins adverses */
    glBegin(GL_LINES);
    for (int i = 0; i < MAXP; i++) {
        if (!np_present(i) || !(NP[i].st.flags & NF_HOOK)) continue;
        glColor3f(0.95f, 0.8f, 0.45f);
        glVertex3f(NP[i].disp.x, NP[i].disp.y + np_height(i) - 0.5f, NP[i].disp.z);
        glVertex3f(NP[i].st.hookPos[0], NP[i].st.hookPos[1], NP[i].st.hookPos[2]);
    }
    glEnd();
}
