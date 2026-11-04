/* VELOCITY - rechargement F5 et render() : l'image complete
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* F5 : relit graphismes/rendu.ini et les textures, puis refait textures, lightmaps et ville */
static void gfx_reload(void) {
    GLuint t[4] = { texGrid, texWall, texGrime, texDot };
    gfx_load_all();
    glDeleteTextures(4, t);
    make_textures();
    for (int i = 0; i < MAXB; i++)
        for (int f = 0; f < 6; f++)
            if (lmaps[i][f].on) { glDeleteTextures(1, &lmaps[i][f].tex); lmaps[i][f].on = 0; }
    glDeleteLists(worldList, 1);
    glDeleteLists(skylineList, 1);
    build_world_list();
    snprintf(popText, sizeof popText, gfxDir[0] ? "GRAPHISMES RECHARGES" : "GRAPHISMES : DOSSIER ABSENT");
    popT = 1.5f;
}

/* --shot bench_* : chronometre des etapes du rendu (BT(k) : temps depuis l'etape precedente, ajoute a benchT[k]) */
static double benchT[12];
static int benchOn;
static void BT(int k) {
    static LARGE_INTEGER last;
    LARGE_INTEGER n, fq;
    if (!benchOn) return;
    glFinish();
    QueryPerformanceCounter(&n); QueryPerformanceFrequency(&fq);
    if (k) benchT[k] += (double)(n.QuadPart - last.QuadPart) * 1000.0 / (double)fq.QuadPart;
    last = n;
}
/* elements animes des cartes, hors liste d'affichage : chevrons des bandes d'acceleration, plan de mort rouge */
static void draw_map_fx(void) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(4);
    glBegin(GL_LINES);
    for (int i = 0; i < nb; i++) {
        if (boxes[i].kind != B_SPEED) continue;
        Box *b = &boxes[i];
        V3 d = vnorm(b->launch), s = v3(-d.z, 0, d.x);
        V3 c = vlerp(b->mn, b->mx, 0.5f);
        float len = fabsf(vdot(vsub(b->mx, b->mn), d)), wid = fabsf(vdot(vsub(b->mx, b->mn), s)), y = b->mx.y + 0.03f;
        float ofs = fmodf(gameTime * 9, 3);
        for (float u = -len * 0.5f + ofs; u < len * 0.5f - 1; u += 3) {   /* chevrons qui defilent dans le sens de la poussee */
            V3 tip = vadd(c, vmul(d, u + 1)), l = vadd(vadd(c, vmul(d, u)), vmul(s, wid * 0.35f)), r = vsub(vadd(c, vmul(d, u)), vmul(s, wid * 0.35f));
            float a = 0.35f + 0.5f * clampf(1 - fabsf(u) / (len * 0.5f), 0, 1);
            glColor4f(1.0f, 0.35f, 0.85f, a);
            glVertex3f(l.x, y, l.z); glVertex3f(tip.x, y, tip.z);
            glVertex3f(tip.x, y, tip.z); glVertex3f(r.x, y, r.z);
        }
    }
    glEnd();
    float ky = maps[curMap].killY;
    if (ky > -999) {                                      /* plan de mort : grille rouge qui respire, 3 m sous killY */
        float py = ky - 3, R = 150, pulse = 0.5f + 0.5f * sinf(gameTime * 2.2f), sc = fmodf(gameTime * 2, 6);
        glDisable(GL_FOG);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_QUADS);
        glColor4f(0.25f, 0.0f, 0.02f, 0.75f);
        glVertex3f(-R, py - 0.05f, -R); glVertex3f(R, py - 0.05f, -R); glVertex3f(R, py - 0.05f, R); glVertex3f(-R, py - 0.05f, R);
        glEnd();
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glLineWidth(2);
        glBegin(GL_LINES);
        for (float u = -R + sc; u <= R; u += 6) {
            glColor4f(1.0f, 0.15f, 0.1f, 0.25f + 0.25f * pulse);
            glVertex3f(u, py, -R); glVertex3f(u, py, R);
            glVertex3f(-R, py, u); glVertex3f(R, py, u);
        }
        glEnd();
        glEnable(GL_FOG);
    }
    glLineWidth(1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void render(float alpha, float frameDt) {
    if (shotMap) { maps_shot_fill(); draw_map_screen(winW, winH); return; }   /* --shot carte*.bmp : capture du second ecran */
    BT(0);
    glViewport(0, 0, winW, winH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    float aspect = (float)winW / (float)(winH > 0 ? winH : 1);

    float hs = hlen(P.vel);
    float hfov = fovBase + clampf((hs - RUN_SPEED) * 0.7f, 0, 20) + P.fovKick;
    P.fovCur += (hfov - P.fovCur) * fminf(1, frameDt * 6);
    float hf = fminf(P.fovCur, 150.0f);
    V3 pos = vlerp(P.prevPos, P.pos, alpha);
    V3 eye = v3(pos.x, pos.y + P.eyeH + P.bob + P.stepOfs - P.landDip, pos.z);
    float camYaw = P.yaw, camPitch = P.pitch, camRoll = P.roll, kcZoom = 1;
    if (killcam_active()) { killcam_view(&eye, &camYaw, &camPitch, &kcZoom); camRoll = 0; hf *= kcZoom; }   /* camera de mort */
    float vfov = 2 * atanf(tanf(hf * PI / 360.0f) / aspect) * 180.0f / PI;

    float sy = frange(-1, 1) * shake * 2, sp = frange(-1, 1) * shake * 2;

    set_frustum(vfov, aspect, 0.05f, 500);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(camRoll, 0, 0, 1);
    glRotatef(-camPitch * 180 / PI + sp, 1, 0, 0);
    glRotatef(camYaw * 180 / PI + sy, 0, 1, 0);
    glTranslatef(-eye.x, -eye.y, -eye.z);

    V3 fwd = v3(sinf(camYaw) * cosf(camPitch), sinf(camPitch), -cosf(camYaw) * cosf(camPitch));
    camR = v3(cosf(camYaw), 0, sinf(camYaw));
    camU = vcross(camR, fwd);
    camEye = eye;
    glGetFloatv(GL_MODELVIEW_MATRIX, mvMat);
    glGetFloatv(GL_PROJECTION_MATRIX, prMat);

    const float *fgc = map_fog_col();
    GLfloat fogc[4] = { fgc[0], fgc[1], fgc[2], 1 };
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fogc);
    glFogf(GL_FOG_START, gxFog[0]);
    glFogf(GL_FOG_END, gxFog[1]);

    set_dyn_light(1);
    GLfloat lp1[4] = { -0.4f, 0.3f, -0.5f, 0 }, dif1[4] = { 0.16f, 0.18f, 0.24f, 1 };   /* lumiere d'appoint, froide */
    glLightfv(GL_LIGHT1, GL_POSITION, lp1);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, dif1);
    viewSun += (sun_vis(eye) - viewSun) * fminf(1, frameDt * 5);
    BT(1);
    draw_sky_dome(eye);
    BT(2);
    if (curMap == 0) glCallList(skylineList);             /* la ville entoure l'Arene ; les autres cartes ont leur ciel seul */
    BT(3);
    glCallList(worldList);
    BT(4);
    fx_world_tick(frameDt);
    storm_tick(frameDt);
    draw_world_marks();
    draw_dyn_shadows();
    BT(5);

    /* ombres sous les ennemis (sans les ombres dynamiques) */
    if (!gfxShadows) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texDot);
    glBegin(GL_QUADS);
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive) continue;
        float h = ray_world(E[i].pos, v3(0, -1, 0), 40, 0);
        float a = clampf(0.55f - h / 40, 0, 0.55f), s = 1.1f, gy = E[i].pos.y - h + 0.03f;
        glColor4f(0, 0, 0, a);
        glTexCoord2f(0, 0); glVertex3f(E[i].pos.x - s, gy, E[i].pos.z - s);
        glTexCoord2f(1, 0); glVertex3f(E[i].pos.x + s, gy, E[i].pos.z - s);
        glTexCoord2f(1, 1); glVertex3f(E[i].pos.x + s, gy, E[i].pos.z + s);
        glTexCoord2f(0, 1); glVertex3f(E[i].pos.x - s, gy, E[i].pos.z + s);
    }
    glEnd();
    glDepthMask(GL_TRUE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    }

    for (int i = 0; i < MAXE; i++) if (E[i].alive) { set_dyn_light(sun_vis(E[i].pos)); draw_enemy(&E[i]); }
    for (int i = 0; dmMode && i < MAXP; i++) if (np_present(i)) trail_tick(i, frameDt);
    draw_outlines();
    if (dmMode) draw_remote_players();
    set_dyn_light(1);
    glEnable(GL_LIGHTING);
    for (int i = 0; i < MAXSH; i++) {                    /* eclats qui rebondissent */
        Shard *h = &SH[i];
        if (h->life <= 0) continue;
        float sz = h->size * fminf(1, h->life * 2);
        glPushMatrix();
        glTranslatef(h->pos.x, h->pos.y, h->pos.z);
        glRotatef(h->ang, h->axis.x, h->axis.y, h->axis.z);
        glColor3f(h->col.x, h->col.y, h->col.z);
        box_faces(v3(-sz, -sz * 0.6f, -sz), v3(sz, sz * 0.6f, sz));
        glPopMatrix();
    }
    glDisable(GL_LIGHTING);
    for (int i = 0; i < MAXCOIN; i++) {                  /* pieces : disques dores qui tournent */
        if (!CO[i].alive) continue;
        if (CO[i].decoy) {                                /* MIRAGE : silhouette du lanceur, qui clignote en se dissipant */
            const float *pc = pcol[CO[i].owner < MAXP ? CO[i].owner : 0];
            float k = CO[i].life < 0.6f && fmodf(CO[i].life * 12, 1) < 0.5f ? 0.4f : 0.85f;
            glPushMatrix();
            glTranslatef(CO[i].pos.x, CO[i].pos.y, CO[i].pos.z);
            glRotatef(-CO[i].spin * 180 / PI, 0, 1, 0);
            glColor3f(pc[0] * k, pc[1] * k, pc[2] * k);
            box_faces(v3(-HW, -0.9f, -HW * 0.6f), v3(HW, 0.55f, HW * 0.6f));
            box_faces(v3(-0.22f, 0.55f, -0.22f), v3(0.22f, 0.9f, 0.22f));
            glPopMatrix();
            continue;
        }
        glPushMatrix();
        glTranslatef(CO[i].pos.x, CO[i].pos.y, CO[i].pos.z);
        glRotatef(CO[i].spin * 900, 0, 1, 0);
        glRotatef(CO[i].spin * 400, 1, 0, 0);
        if (CO[i].teslaT > 0) glColor3f(0.55f, 0.75f, 1.0f); else glColor3f(1.0f, 0.82f, 0.3f);   /* Tesla : relais bleu */
        box_faces(v3(-0.16f, -0.16f, -0.025f), v3(0.16f, 0.16f, 0.025f));
        glPopMatrix();
    }
    draw_saws();                                          /* Scie */
    draw_mines();                                         /* Ventouse */
    for (int i = 0; i < npu; i++) {                      /* cellules de munitions : les grandes sont plus grosses et violettes */
        if (PU[i].t > 0) continue;
        int big = PU[i].pts >= PICKUP_BIG;
        float k = big ? 1.4f : 1.0f;
        glPushMatrix();
        glTranslatef(PU[i].pos.x, PU[i].pos.y + 0.15f * sinf(gameTime * 3 + i), PU[i].pos.z);
        glRotatef(gameTime * 90 + i * 40, 0, 1, 0);
        glScalef(k, k, k);
        glColor3f(big ? 0.9f : 0.75f, big ? 0.8f : 0.95f, 1.0f);
        box_faces(v3(-0.22f, -0.35f, -0.22f), v3(0.22f, 0.35f, 0.22f));
        if (big) glColor3f(0.75f, 0.45f, 1.0f); else glColor3f(0.2f, 0.6f, 1.0f);
        box_edges(v3(-0.26f, -0.4f, -0.26f), v3(0.26f, 0.4f, 0.26f));
        glPopMatrix();
    }

    if (P.hook != HOOK_NONE) {                            /* cable du grappin, part de la main gauche */
        V3 hand = vadd(eye, vadd(vmul(camR, -0.3f), vadd(vmul(camU, -0.28f), vmul(fwd, 0.6f))));
        glLineWidth(3);
        glColor3f(0.95f, 0.8f, 0.45f);
        glBegin(GL_LINES);
        glVertex3f(hand.x, hand.y, hand.z);
        glVertex3f(P.hookPos.x, P.hookPos.y, P.hookPos.z);
        glEnd();
    }

    draw_flails(eye);                                     /* Fleau : chaines (la mienne et celles des autres) */

    /* tout ce qui brille : additif */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glLineWidth(1);
    for (int i = 0; i < MAXBM; i++) {
        Beam *m = &BM[i];
        if (m->life <= 0) continue;
        float a = m->life / m->max;
        glLineWidth(7); glColor4f(m->col.x, m->col.y, m->col.z, a * 0.35f);
        glBegin(GL_LINES); glVertex3f(m->a.x, m->a.y, m->a.z); glVertex3f(m->b.x, m->b.y, m->b.z); glEnd();
        glLineWidth(2); glColor4f(1, 1, 1, a);
        glBegin(GL_LINES); glVertex3f(m->a.x, m->a.y, m->a.z); glVertex3f(m->b.x, m->b.y, m->b.z); glEnd();
    }
    draw_world_glow();                                    /* neons, lumieres des armes, ondes de choc */
    draw_weather(eye);                                    /* poussiere, pluie */
    draw_tesla_arcs();                                    /* Tesla : liens d'arc */
    draw_trails();                                        /* trainees de vitesse des joueurs */
    for (int i = 0; i < MAXTR; i++) {                    /* Eventail : traceurs (tete de 2,5 m, puis trace qui s'eteint) */
        Tracer *r = &TR[i];
        if (r->t >= r->life) continue;
        V3 dv = vsub(r->b, r->a);
        float len = vlen(dv), travel = len / SG_TRACER_SPEED;
        if (len < 1e-3f) continue;
        dv = vmul(dv, 1.0f / len);
        if (r->t < travel) {
            float hd = r->t * SG_TRACER_SPEED;
            V3 h = vadd(r->a, vmul(dv, hd)), q = vadd(r->a, vmul(dv, fmaxf(0, hd - 2.5f)));
            glLineWidth(4); glColor4f(r->col.x, r->col.y, r->col.z, 0.3f);
            glBegin(GL_LINES); glVertex3f(q.x, q.y, q.z); glVertex3f(h.x, h.y, h.z); glEnd();
            glLineWidth(1.5f); glColor4f(1, 1, 1, 0.9f);
            glBegin(GL_LINES); glVertex3f(q.x, q.y, q.z); glVertex3f(h.x, h.y, h.z); glEnd();
        } else {
            float a = 0.15f * (1 - (r->t - travel) / 0.12f);
            glLineWidth(2); glColor4f(r->col.x, r->col.y, r->col.z, a);
            glBegin(GL_LINES); glVertex3f(r->a.x, r->a.y, r->a.z); glVertex3f(r->b.x, r->b.y, r->b.z); glEnd();
        }
    }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texDot);
    glBegin(GL_QUADS);
    for (int i = 0; i < MAXPA; i++) {
        Part *q = &PA[i];
        if (q->life <= 0) continue;
        float a = q->life / q->max;
        glColor4f(q->col.x, q->col.y, q->col.z, a);
        billboard(q->pos, q->size * (0.5f + 0.5f * a));
    }
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive) continue;
        V3 c = enemy_color(E[i].type);
        glColor4f(c.x, c.y, c.z, 0.35f);
        billboard(E[i].pos, 2.2f);
    }
    for (int i = 0; i < MAXPR; i++) {
        if (!PR[i].alive) continue;
        if (PR[i].kind == PR_SAW) {                       /* Scie : halo selon l'etat */
            Proj *q = &PR[i];
            int st = q->sawState;
            if (st == SAW_LOST || (st == SAW_PLANTED && (q->owner != myId || spectating))) continue;
            if (st == SAW_PLANTED) glColor4f(1, 0.8f, 0.3f, 0.5f + 0.3f * sinf(gameTime * 4));   /* la mienne : a retrouver de loin */
            else if (st == SAW_RETURN || st == SAW_RECALL) glColor4f(0.4f, 0.85f, 1, 0.8f);
            else if (q->sawBounces || st == SAW_PARRIED) glColor4f(1, 0.95f, 0.7f, 0.9f);
            else glColor4f(1, 0.6f, 0.2f, 0.8f);
            billboard(q->pos, st == SAW_PLANTED ? 0.9f : 0.6f);
            continue;
        }
        if (PR[i].kind == PR_MINE) { draw_mine_glow(&PR[i]); continue; }   /* Ventouse */
        if (PR[i].kind == PR_ROCKET) { glColor4f(1, 0.6f, 0.2f, 1); billboard(PR[i].pos, 0.55f); glColor4f(1, 1, 0.8f, 1); billboard(PR[i].pos, 0.2f); }
        else if (PR[i].kind == PR_PARRIED) { glColor4f(1, 0.85f, 0.4f, 1); billboard(PR[i].pos, 1.0f); glColor4f(1, 1, 1, 1); billboard(PR[i].pos, 0.3f); }
        else { glColor4f(1, 0.25f, 0.9f, 1); billboard(PR[i].pos, 0.7f); glColor4f(1, 0.9f, 1, 1); billboard(PR[i].pos, 0.22f); }
    }
    for (int i = 0; i < MAXCOIN; i++) {                  /* reflet des pieces */
        if (!CO[i].alive) continue;
        if (CO[i].decoy) { glColor4f(0.4f, 0.85f, 1, 0.25f + 0.15f * sinf(gameTime * 20)); billboard(CO[i].pos, 2.4f); continue; }
        if (CO[i].teslaT > 0) glColor4f(0.45f, 0.7f, 1, 0.6f + 0.4f * sinf(CO[i].spin * 40));   /* Tesla : lueur bleue */
        else glColor4f(1, 0.85f, 0.4f, 0.5f + 0.4f * sinf(CO[i].spin * 25));
        billboard(CO[i].pos, 0.7f);
    }
    if (P.hook != HOOK_NONE) {
        glColor4f(1, 0.8f, 0.35f, 1); billboard(P.hookPos, 0.4f);
        glColor4f(1, 1, 0.9f, 1); billboard(P.hookPos, 0.15f);
    }
    for (int i = 0; i < npu; i++) {
        if (PU[i].t > 0) continue;
        int big = PU[i].pts >= PICKUP_BIG;
        glColor4f(big ? 0.75f : 0.3f, big ? 0.45f : 0.8f, 1.0f, 0.55f + 0.2f * sinf(gameTime * 5 + i));
        billboard(PU[i].pos, big ? 1.9f : 1.3f);
    }
    float pulse = 0.5f + 0.5f * sinf(gameTime * 4);
    for (int i = 0; i < nb; i++) {
        if (boxes[i].kind != B_PAD) continue;
        V3 c = vlerp(boxes[i].mn, boxes[i].mx, 0.5f);
        float s = 1.4f + pulse * 0.4f, y = boxes[i].mx.y + 0.02f;
        glColor4f(0.2f, 0.8f, 1.0f, 0.5f + 0.3f * pulse);
        glTexCoord2f(0, 0); glVertex3f(c.x - s, y, c.z - s);
        glTexCoord2f(1, 0); glVertex3f(c.x + s, y, c.z - s);
        glTexCoord2f(1, 1); glVertex3f(c.x + s, y, c.z + s);
        glTexCoord2f(0, 1); glVertex3f(c.x - s, y, c.z + s);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    draw_map_fx();                                        /* bandes d'acceleration, plan de mort */
    draw_race_world();                                    /* course : portes et fantome */
    draw_modes_world();                                   /* modes : zones, drapeaux, bouclier, salles de depart */
    vg_draw_world_fx();                                   /* VAGUES : rayons, ondes, liens, objets de la boutique */
    draw_slam_marker();
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_FOG);

    BT(6);
    if (appState == APP_GAME && !spectating) draw_viewmodel(aspect);
    BT(7);
    if (appState == APP_GAME) draw_flare(eye);
    bloom_apply();
    if (stormFlash > 0) {                                 /* eclair : tout l'ecran blanchit un instant */
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, winW, winH, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        rect(0, 0, (float)winW, (float)winH, 0.75f, 0.8f, 1.0f, 0.45f * stormFlash);
        glDisable(GL_BLEND); glEnable(GL_DEPTH_TEST);
    }
    BT(8);
    if (impactT > 0 && appState == APP_GAME) {           /* IMPACT FRAME : couleurs inversees + traits radiaux */
        float sx = winW * 0.5f, sy = winH * 0.5f;
        project(impactAt, &sx, &sy);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glOrtho(0, winW, winH, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
        glColor4f(1, 1, 1, 1);
        rect(0, 0, (float)winW, (float)winH, 1, 1, 1, 1);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        float big = (float)(winW + winH);
        glBegin(GL_TRIANGLES);
        for (int i = 0; i < 46; i++) {
            float ang = frange(0, 2 * PI), wid = frange(0.004f, 0.02f), r0 = frange(40, 160);
            glColor4f(0, 0, 0, 0.85f);
            glVertex2f(sx + cosf(ang) * r0, sy + sinf(ang) * r0);
            glVertex2f(sx + cosf(ang - wid) * big, sy + sinf(ang - wid) * big);
            glVertex2f(sx + cosf(ang + wid) * big, sy + sinf(ang + wid) * big);
        }
        glEnd();
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        impactT -= frameDt;
    }
    BT(9);
    draw_hud();
    BT(10);
}
