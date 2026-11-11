/* VELOCITY - mode COURSE : affichage (chrono, portes en 3D, fantome) et sauvegarde du record
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- record : a cote de velocity.ini (jamais en --bot / --shot / --test) : velocity_course.txt pour la premiere carte
 * CHALLENGE (LA TOILE, nom historique), velocity_course_<carte>.txt pour les suivantes --- */
static int cfg_side_file(char *out, int n, const char *name) {   /* fichier du joueur a cote de velocity.ini */
    if (!cfgPath[0]) return 0;
    snprintf(out, n, "%s", cfgPath);
    char *s = strrchr(out, '/'), *b = strrchr(out, '\\');
    if (b > s) s = b;
    if (!s) return 0;
    snprintf(s + 1, n - (int)(s + 1 - out), "%s", name);
    return 1;
}
static int race_file(char *out, int n) {
    char name[64], slug[32];
    if (curMap == race_map()) snprintf(name, sizeof name, "velocity_course.txt");
    else { map_slug(curMap, slug, sizeof slug); snprintf(name, sizeof name, "velocity_course_%s.txt", slug); }
    return cfg_side_file(out, n, name);
}
static void race_save(void) {
    char path[MAX_PATH + 32];
    if (botMode || shotMode || !race_file(path, sizeof path)) return;
    FILE *f = fopen(path, "w");
    if (!f) return;
    const MapDef *md = &maps[curMap];
    fprintf(f, "carte=%s\nrecord=%.4f\nportes=%d\n", md->name, raceBest, md->ngates);
    for (int i = 0; i < md->ngates; i++) fprintf(f, "%.4f\n", raceBestSplit[i]);
    fprintf(f, "fantome=%d\n", ghostBestN);
    for (int i = 0; i < ghostBestN; i++) fprintf(f, "%.2f %.2f %.2f %.3f\n", ghostBest[i][0], ghostBest[i][1], ghostBest[i][2], ghostBest[i][3]);
    fclose(f);
}
static void race_load(void) {
    char path[MAX_PATH + 32], line[128], name[32] = "";
    if (botMode || shotMode || !race_file(path, sizeof path)) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    int ng = 0, gn = 0;
    float best = 0;
    if (fgets(line, sizeof line, f)) sscanf(line, "carte=%31[^\n]", name);
    if (fgets(line, sizeof line, f)) sscanf(line, "record=%f", &best);
    if (fgets(line, sizeof line, f)) sscanf(line, "portes=%d", &ng);
    const MapDef *md = &maps[curMap];
    if (strcmp(name, md->name) || ng != md->ngates || ng > RACE_GATES || best <= 0) { fclose(f); return; }   /* autre carte, autre trace */
    for (int i = 0; i < ng && fgets(line, sizeof line, f); i++) raceBestSplit[i] = (float)atof(line);
    if (fgets(line, sizeof line, f)) sscanf(line, "fantome=%d", &gn);
    if (gn < 0 || gn > RACE_GMAX) gn = 0;
    int k = 0;
    while (k < gn && fgets(line, sizeof line, f))
        if (sscanf(line, "%f %f %f %f", &ghostBest[k][0], &ghostBest[k][1], &ghostBest[k][2], &ghostBest[k][3]) == 4) k++;
    fclose(f);
    raceBest = best; ghostBestN = k;
}

/* --- record perso du speedrun complet : velocity_speedrun.txt (les cartes dans l'ordre, temps cumules) --- */
static void sr_pb_save(void) {
    char path[MAX_PATH + 32];
    if (botMode || shotMode || !cfg_side_file(path, sizeof path, "velocity_speedrun.txt")) return;
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "cartes=%d\n", srPbN);
    for (int k = 0; k < srPbN; k++) fprintf(f, "%s=%d\n", maps[srMaps[k]].name, srPb[k]);
    fclose(f);
}
static void sr_pb_load(void) {
    char path[MAX_PATH + 32], line[96], name[32];
    int n = 0, v[NMAPS];
    srPbLoaded = 1;
    if (botMode || shotMode || !cfg_side_file(path, sizeof path, "velocity_speedrun.txt")) return;
    FILE *f = fopen(path, "r");
    if (!f) return;
    if (fgets(line, sizeof line, f)) sscanf(line, "cartes=%d", &n);
    int ok = n == srMapN;                                 /* autres cartes CHALLENGE : ancien record ignore */
    for (int k = 0; k < n && ok; k++) {
        ok = fgets(line, sizeof line, f) && sscanf(line, "%31[^=]=%d", name, &v[k]) == 2 && !strcmp(name, maps[srMaps[k]].name) &&
             v[k] > (k ? v[k - 1] : 0);
    }
    fclose(f);
    if (!ok) return;
    memcpy(srPb, v, sizeof v[0] * (size_t)n);
    srPbN = n;
}

static void race_time_str(char *b, int n, float t) {      /* 1:02.37 */
    int ms = (int)(t * 100 + 0.5f);
    snprintf(b, n, "%d:%02d.%02d", ms / 6000, (ms / 100) % 60, ms % 100);
}

/* --- monde : portes (anneaux), colonne de lumiere sur la suivante, fantome du record --- */
static void draw_race_world(void) {
    if (!raceOn) return;
    const MapDef *md = &maps[curMap];
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D); glDisable(GL_FOG);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    for (int i = raceGate; i < md->ngates && i < raceGate + 3; i++) {
        V3 c = race_gate(i), prev = i > 0 ? race_gate(i - 1) : md->start;
        float r = md->gates[i][3];
        V3 n = vnorm(vsub(c, prev)), u = vnorm(vcross(n, fabsf(n.y) > 0.9f ? v3(1, 0, 0) : v3(0, 1, 0))), v = vcross(n, u);
        int next = i == raceGate, fin = i == md->ngates - 1;
        int shut = md->ntargets > 0 && race_targets_left(i) > 0;   /* cibles encore debout : l'anneau est ferme (rouge) */
        float pulse = 0.75f + 0.25f * sinf((float)nowT * 6), a = next ? 0.95f : 0.35f;
        float col[3] = { fin ? 1.0f : 0.3f, fin ? 0.8f : 0.85f, fin ? 0.25f : 1.0f };
        if (shut) { col[0] = 1.0f; col[1] = 0.25f; col[2] = 0.25f; }
        for (int ring = 0; ring < (next ? 3 : 1); ring++) {   /* la porte suivante : trois anneaux qui respirent */
            float rr = r * (1 - ring * 0.08f) * (next ? pulse * 0.1f + 0.95f : 1);
            glLineWidth(next ? 5.0f - ring : 2.5f);
            glColor4f(col[0], col[1], col[2], a * (1 - ring * 0.3f));
            glBegin(GL_LINE_LOOP);
            for (int s = 0; s < 48; s++) {
                float t = s * 2 * PI / 48;
                V3 p = vadd(c, vadd(vmul(u, cosf(t) * rr), vmul(v, sinf(t) * rr)));
                glVertex3f(p.x, p.y, p.z);
            }
            glEnd();
        }
        if (next) {                                       /* voile leger dans l'anneau + colonne de lumiere */
            glColor4f(col[0], col[1], col[2], 0.08f * pulse);
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(c.x, c.y, c.z);
            for (int s = 0; s <= 48; s++) {
                float t = s * 2 * PI / 48;
                V3 p = vadd(c, vadd(vmul(u, cosf(t) * r), vmul(v, sinf(t) * r)));
                glVertex3f(p.x, p.y, p.z);
            }
            glEnd();
            glLineWidth(3);
            glBegin(GL_LINES);
            glColor4f(col[0], col[1], col[2], 0.0f); glVertex3f(c.x, c.y + r + 60, c.z);
            glColor4f(col[0], col[1], col[2], 0.35f); glVertex3f(c.x, c.y + r, c.z);
            glEnd();
        }
    }
    {                                                     /* points de balancement : contour dore qui brille, plus fort sur la cible de l'aimant */
        V3 at; int tgt = P.hook == HOOK_NONE ? swing_target(eye_pos(), view_dir(), &at) : -1;
        float pulse = 0.6f + 0.4f * sinf((float)nowT * 4);
        for (int i = 0; i < nb; i++) {
            if (!swing_box(i)) continue;
            V3 c = vmul(vadd(boxes[i].mn, boxes[i].mx), 0.5f);
            if (vlen(vsub(c, camEye)) > 140) continue;
            int hot = i == tgt || (P.hook == HOOK_ATTACHED && vlen(vsub(P.anchor, c)) < 2.5f);
            for (int k = 0; k < 2; k++) {
                float g = k ? 0.9f + 0.3f * pulse : 0.05f;
                glLineWidth(k ? 2.5f : (hot ? 5.0f : 3.0f));
                glColor4f(1, 0.8f, 0.3f, hot ? 0.95f : k ? 0.35f + 0.25f * pulse : 0.5f);
                box_edges(vsub(boxes[i].mn, v3(g, g, g)), vadd(boxes[i].mx, v3(g, g, g)));
            }
        }
    }
    glLineWidth(1);
    V3 gp; float gy;
    if (raceCount <= 0 && race_ghost_at(raceT, &gp, &gy)) {   /* fantome : silhouette bleutee, additive */
        SkinAnim an;
        V3 gp2; float gy2;
        race_ghost_at(raceT + 0.1f, &gp2, &gy2);
        float sp = hlen(vsub(gp2, gp)) * 10;
        an.spd = clampf(sp / 12, 0, 1); an.run = raceT * 9; an.air = 1; an.slide = 0; an.pitch = 0; an.t = raceT;
        skinFlat = 1; skinFlatCol[0] = 0.25f; skinFlatCol[1] = 0.6f; skinFlatCol[2] = 0.9f;
        static const float gc[3] = { 0.3f, 0.7f, 1.0f };
        glPushMatrix();
        glTranslatef(gp.x, gp.y, gp.z);
        glRotatef(-gy * 180 / PI, 0, 1, 0);
        draw_skin(wantSkin, H_STAND, gc, &an);
        glPopMatrix();
        skinFlat = 0;
    }
    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_FOG);
}

/* --- speedrun : resultat du classement (serveur) a l'arrivee, panneau et bilan du speedrun complet --- */
static void sr_result_line(int cat, char *buf, int n, float *col) {   /* "CLASSEMENT : #3 sur 41 - nouveau record perso" */
    const SrRun *r = &srRun[cat];
    const SrTop *t = &srTop[cat];
    char tb[24];
    col[0] = 0.7f; col[1] = 0.75f; col[2] = 0.85f;
    switch (r->state) {
    case SRR_WAIT: case SRR_SENT: snprintf(buf, n, "CLASSEMENT : envoi du temps ..."); break;
    case SRR_RECORD:
        col[0] = 0.4f; col[1] = 1; col[2] = 0.6f;
        if (r->rank == 1) snprintf(buf, n, "CLASSEMENT : #1  -  RECORD DU MONDE !");
        else if (t->state == 2 && t->runners > 0) snprintf(buf, n, "CLASSEMENT : #%d sur %d  -  record perso enregistre", r->rank, t->runners);
        else snprintf(buf, n, "CLASSEMENT : #%d  -  record perso enregistre", r->rank);
        break;
    case SRR_KEPT:
        sr_time_str(tb, sizeof tb, r->best);
        snprintf(buf, n, "CLASSEMENT : #%d  -  ton meilleur temps reste %s", r->rank, tb);
        break;
    case SRR_REFUSED: col[0] = 1; col[1] = 0.5f; col[2] = 0.4f; snprintf(buf, n, "CLASSEMENT : temps refuse (%s)", r->err); break;
    case SRR_OFFLINE: snprintf(buf, n, "CLASSEMENT : %s, temps garde sur cette machine", r->err[0] ? r->err : "hors ligne"); break;
    default: buf[0] = 0;
    }
}
/* speedrun complet en cours : chrono global, temps a l'arrivee de chaque carte et ecart au record perso */
static void draw_sr_full_panel(void) {
    char buf[96], tb[24];
    float x = 16, y = 14, w = 400, h = 92 + 26.0f * srMapN;
    hud_panel(x, y, w, h);
    text(0, x + 14, y + 24, "SPEEDRUN COMPLET", 1.0f, 0.85f, 0.35f, 1);
    sr_time_str(tb, sizeof tb, sr_full_ms());
    text(2, x + 14, y + 66, tb, 1, 1, 1, 1);
    if (srCmpN == srMapN) {
        sr_time_str(tb, sizeof tb, srCmp[srMapN - 1]);
        snprintf(buf, sizeof buf, "record %s", tb);
        text(0, x + w - 14 - fontW[0] * (float)strlen(buf), y + 24, buf, 1, 0.8f, 0.3f, 0.85f);
    }
    for (int k = 0; k < srMapN; k++) {
        float yy = y + 92 + 26.0f * k;
        int done = k < srIdx || (k == srIdx && raceDone), cur = k == srIdx && !done;
        snprintf(buf, sizeof buf, "%d. %s", k + 1, maps[srMaps[k]].name);
        float a = done || cur ? 1.0f : 0.5f;
        text(0, x + 14, yy, buf, cur ? 0.5f : 0.85f, cur ? 0.9f : 0.85f, 1, a);
        int pb = srCmpN == srMapN;
        if (!done) {                                      /* pas encore arrive : le temps du record a cette arrivee */
            if (!pb) continue;
            sr_time_str(tb, sizeof tb, srCmp[k]);
            text(0, x + w - 14 - fontW[0] * (float)strlen(tb), yy, tb, 0.6f, 0.62f, 0.7f, 0.8f);
            continue;
        }
        sr_time_str(tb, sizeof tb, srSplit[k]);
        int d = sr_full_delta(k);
        if (pb) snprintf(buf, sizeof buf, "%s  %+.2f", tb, d / 1000.0f); else snprintf(buf, sizeof buf, "%s", tb);
        float g = !pb ? 0.9f : d <= 0 ? 1.0f : 0.45f, rr = !pb ? 0.9f : d <= 0 ? 0.4f : 1.0f;
        text(0, x + w - 14 - fontW[0] * (float)strlen(buf), yy, buf, rr, g, !pb ? 0.95f : 0.45f, 1);
    }
}
static void draw_sr_full_end(float cx, float cy) {        /* bilan : temps total, chaque carte, classement */
    char buf[128], tb[24];
    float col[3], h = 250 + 24.0f * srMapN, y0 = cy - h * 0.5f;
    rect(cx - 300, y0, 600, h, 0.02f, 0.03f, 0.07f, 0.88f);
    rect(cx - 300, y0, 600, 3, 1, 0.8f, 0.3f, 1);
    text_c(1, cx, y0 + 40, "SPEEDRUN COMPLET", 1, 0.85f, 0.3f, 1);
    sr_time_str(tb, sizeof tb, srSplit[srMapN - 1]);
    text_c(2, cx, y0 + 96, tb, 1, 1, 1, 1);
    if (srNewBest == 2) text_c(1, cx, y0 + 134, "NOUVEAU RECORD PERSO !", 0.4f, 1, 0.5f, 0.6f + 0.4f * sinf((float)nowT * 8));
    else if (srNewBest == 1) text_c(1, cx, y0 + 134, "PREMIER SPEEDRUN COMPLET", 0.5f, 0.85f, 1, 1);
    else {
        snprintf(buf, sizeof buf, "%+.2f sur ton record", sr_full_delta(srMapN - 1) / 1000.0f);
        text_c(1, cx, y0 + 134, buf, 1, 0.5f, 0.4f, 1);
    }
    for (int k = 0; k < srMapN; k++) {                    /* temps de chaque carte (pas les cumules) */
        int seg = srSplit[k] - (k ? srSplit[k - 1] : 0);
        sr_time_str(tb, sizeof tb, seg);
        if (srCmpN == srMapN) {                           /* et l'ecart au record sur cette carte seule */
            int d = seg - (srCmp[k] - (k ? srCmp[k - 1] : 0));
            snprintf(buf, sizeof buf, "%-14s %s   %+.2f", maps[srMaps[k]].name, tb, d / 1000.0f);
        } else snprintf(buf, sizeof buf, "%-14s %s", maps[srMaps[k]].name, tb);
        text_c(0, cx, y0 + 168 + 24.0f * k, buf, 0.8f, 0.85f, 0.95f, 1);
    }
    sr_result_line(SR_COMPLET, buf, sizeof buf, col);
    text_c(0, cx, y0 + h - 56, buf, col[0], col[1], col[2], 1);
    text_c(0, cx, y0 + h - 26, "[R] recommencer le speedrun        [ECHAP] menu", 0.9f, 0.9f, 1, 1);
}

/* --- HUD : chrono, portes, ecart au record, decompte, fleche vers la porte, arrivee --- */
static void draw_race_hud(void) {
    if (!raceOn) return;
    const MapDef *md = &maps[curMap];
    float cx = winW * 0.5f, cy = winH * 0.5f;
    char buf[64], tb[24];
    race_time_str(tb, sizeof tb, raceT);
    hud_panel(cx - 130, 14, 260, 92);
    text_c(2, cx, 66, tb, 1, 1, 1, 1);
    snprintf(buf, sizeof buf, "PORTE %d / %d", raceGate < md->ngates ? raceGate + 1 : md->ngates, md->ngates);
    text_c(0, cx, 94, buf, 0.5f, 0.85f, 1, 0.9f);
    if (raceBest > 0) {
        race_time_str(tb, sizeof tb, raceBest);
        snprintf(buf, sizeof buf, "RECORD %s", tb);
        text(0, cx + 140, 40, buf, 1, 0.8f, 0.3f, 0.85f);
    }
    if (md->ntargets > 0 && !raceDone && raceGate < md->ngates) {   /* cibles de la porte en cours */
        int left = race_targets_left(raceGate), tot = race_targets_total(raceGate);
        if (tot > 0) {
            snprintf(buf, sizeof buf, left ? "CIBLES %d / %d" : "PORTE OUVERTE", tot - left, tot);
            if (left) text_c(1, cx, 152, buf, 1, 0.45f, 0.35f, 0.95f); else text_c(1, cx, 152, buf, 0.4f, 1, 0.5f, 0.95f);
        }
        if (raceLockMsgT > 0) text_c(1, cx, cy + 90, "PORTE VERROUILLEE : ABATS LES CIBLES", 1, 0.4f, 0.3f, clampf(raceLockMsgT, 0, 1));
    }
    if (raceFalls) { snprintf(buf, sizeof buf, "chutes %d", raceFalls); text(0, cx + 140, 62, buf, 0.8f, 0.5f, 0.5f, 0.8f); }
    if (raceDeltaT > 0 && raceBest > 0 && !raceDone) {    /* ecart au record a la porte */
        snprintf(buf, sizeof buf, "%+.2f", raceDelta);
        float a = clampf(raceDeltaT, 0, 1);
        if (raceDelta <= 0) text_c(1, cx, 132, buf, 0.4f, 1, 0.5f, a); else text_c(1, cx, 132, buf, 1, 0.4f, 0.35f, a);
    }
    if (raceGateFlash > 0) rect(0, 0, (float)winW, (float)winH, 0.3f, 0.85f, 1, raceGateFlash * 0.12f);
    if (raceCount > 0) {                                  /* decompte */
        snprintf(buf, sizeof buf, "%d", (int)ceilf(raceCount));
        float k = raceCount - floorf(raceCount);
        text_c(2, cx, cy - 60, buf, 1, 0.85f, 0.3f, 0.4f + 0.6f * k);
        text_c(0, cx, cy + 10, "traverse les portes dans l'ordre  -  grappin, wall-run, dash", 0.8f, 0.85f, 0.95f, 0.9f);
        snprintf(buf, sizeof buf, "[R] recommencer   %s", ghostBestN ? "le fantome bleu = ton record" : "");
        text_c(0, cx, cy + 34, buf, 0.6f, 0.65f, 0.75f, 0.9f);
    } else if (raceT < 0.7f) text_c(2, cx, cy - 60, "GO !", 0.4f, 1, 0.5f, 1 - raceT / 0.7f);
    if (!raceDone && raceCount <= 0 && P.hook == HOOK_NONE && !P.dead) {   /* aimant : le point que prendra le grappin */
        V3 at; float sx, sy;
        if (swing_target(eye_pos(), view_dir(), &at) >= 0 && project(at, &sx, &sy)) {
            float r = 22 + 4 * sinf((float)nowT * 9), a0 = (float)nowT * 2;
            glLineWidth(3);
            glColor4f(1, 0.85f, 0.3f, 0.95f);
            glBegin(GL_LINES);                            /* quatre crochets qui tournent autour du cube */
            for (int k = 0; k < 4; k++) {
                float a = a0 + k * PI / 2;
                glVertex2f(sx + cosf(a) * r, sy + sinf(a) * r);
                glVertex2f(sx + cosf(a + 0.5f) * r, sy + sinf(a + 0.5f) * r);
            }
            glEnd();
            glLineWidth(1);
            snprintf(buf, sizeof buf, "[%s]", bind_text(A_HOOK));
            text_c(0, sx, sy + r + 18, buf, 1, 0.85f, 0.3f, 0.9f);
        }
    }
    if (!raceDone && raceGate < md->ngates) {             /* fleche au bord de l'ecran si la porte est hors champ */
        float sx, sy;
        V3 g = race_gate(raceGate);
        int on = project(g, &sx, &sy) && sx > 40 && sx < winW - 40 && sy > 40 && sy < winH - 40;
        if (!on) {
            V3 d = vsub(g, eye_pos());
            float rel = atan2f(d.x, -d.z) - P.yaw;
            while (rel > PI) rel -= 2 * PI;
            while (rel < -PI) rel += 2 * PI;
            float ax = cx + sinf(rel) * (winH * 0.36f), ay = cy - cosf(rel) * (winH * 0.36f);
            glColor4f(0.3f, 0.85f, 1, 0.9f);
            glBegin(GL_TRIANGLES);
            glVertex2f(ax + sinf(rel) * 18, ay - cosf(rel) * 18);
            glVertex2f(ax + cosf(rel) * 10, ay + sinf(rel) * 10);
            glVertex2f(ax - cosf(rel) * 10, ay - sinf(rel) * 10);
            glEnd();
        }
    }
    if (srFull) draw_sr_full_panel();
    if (srFull && srDone) { draw_sr_full_end(cx, cy); return; }
    if (raceDone) {                                       /* arrivee */
        float col[3];
        rect(cx - 260, cy - 130, 520, 246, 0.02f, 0.03f, 0.07f, 0.85f);
        rect(cx - 260, cy - 130, 520, 3, 1, 0.8f, 0.3f, 1);
        text_c(2, cx, cy - 70, "ARRIVEE", 1, 0.85f, 0.3f, 1);
        race_time_str(tb, sizeof tb, raceT);
        text_c(2, cx, cy - 10, tb, 1, 1, 1, 1);
        if (raceNewBest == 2) text_c(1, cx, cy + 34, "NOUVEAU RECORD !", 0.4f, 1, 0.5f, 0.6f + 0.4f * sinf((float)nowT * 8));
        else if (raceNewBest == 1) text_c(1, cx, cy + 34, "PREMIER TEMPS : ton fantome t'attend", 0.5f, 0.85f, 1, 1);
        else {
            snprintf(buf, sizeof buf, "+%.2f sur le record", raceT - raceBest);
            text_c(1, cx, cy + 34, buf, 1, 0.5f, 0.4f, 1);
        }
        snprintf(buf, sizeof buf, "chutes %d   -   style max %s", raceFalls, styleName[styleTopRank]);
        text_c(0, cx, cy + 62, buf, 0.7f, 0.75f, 0.85f, 1);
        sr_result_line(SR_MAP0 + curMap, buf, sizeof buf, col);   /* classement en ligne de la carte */
        text_c(0, cx, cy + 84, buf, col[0], col[1], col[2], 1);
        if (srFull && srIdx + 1 < srMapN) {
            snprintf(buf, sizeof buf, "carte suivante : %s dans %.0f s        [R] tout recommencer", maps[srMaps[srIdx + 1]].name, ceilf(srNextT));
            text_c(0, cx, cy + 108, buf, 0.9f, 0.9f, 1, 1);
        } else text_c(0, cx, cy + 108, "[R] recommencer        [ECHAP] menu", 0.9f, 0.9f, 1, 1);
    }
}
