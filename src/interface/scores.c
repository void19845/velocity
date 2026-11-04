/* VELOCITY - tableau des scores, killfeed, pseudos
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static void draw_team_scoreboard(float cx, float cy) {
    char buf[64];
    float w = 440, gap = 70, y = cy - 120;
    int most = team_count(0) > team_count(1) ? team_count(0) : team_count(1);
    for (int t = 0; t < 2; t++) {
        int order[MAXP], n = 0;
        for (int i = 0; i < MAXP; i++) if (NP[i].active && NP[i].team == t) order[n++] = i;
        for (int a = 1; a < n; a++)
            for (int b = a; b > 0 && NP[order[b]].kills > NP[order[b - 1]].kills; b--) {
                int tt = order[b]; order[b] = order[b - 1]; order[b - 1] = tt;
            }
        float x = t == 0 ? cx - gap * 0.5f - w : cx + gap * 0.5f;
        const float *tc = teamCol[t];
        rect(x - 20, y - 96, w + 40, 130 + most * 38.0f, 0.03f, 0.04f, 0.08f, 0.88f);
        rect(x - 20, y - 96, w + 40, 4, tc[0], tc[1], tc[2], 0.9f);
        snprintf(buf, sizeof buf, "EQUIPE %s", teamNames[t]);
        text(1, x, y - 58, buf, tc[0], tc[1], tc[2], 1);
        snprintf(buf, sizeof buf, "%d", team_score(t));
        text(2, x + w - fontW[2] * (float)strlen(buf), y - 46, buf, 1, 1, 1, 1);
        text(0, x, y - 14, "JOUEUR", 0.6f, 0.7f, 0.85f, 1);
        text(0, x + 270, y - 14, "FRAGS", 0.6f, 0.7f, 0.85f, 1);
        text(0, x + 360, y - 14, "MORTS", 0.6f, 0.7f, 0.85f, 1);
        for (int k = 0; k < n; k++) {
            int i = order[k];
            float yy = y + 30 + k * 38.0f;
            if (i == myId) rect(x - 10, yy - 26, w + 20, 34, 1, 1, 1, 0.08f);
            text(1, x, yy, NP[i].name, pcolor(i)[0], pcolor(i)[1], pcolor(i)[2], NP[i].alive ? 1.0f : 0.5f);
            snprintf(buf, sizeof buf, "%d", NP[i].kills);
            text(1, x + 270, yy, buf, 1, 1, 1, 1);
            snprintf(buf, sizeof buf, "%d", NP[i].deaths);
            text(1, x + 360, yy, buf, 0.8f, 0.8f, 0.85f, 1);
        }
    }
    snprintf(buf, sizeof buf, "%dV%d   -   premiere equipe a %d frags", teamSize, teamSize, fragLimit);
    text_c(0, cx, y + 70 + most * 38.0f, buf, 0.75f, 0.8f, 0.9f, 1);
}

static void draw_scoreboard(float cx, float cy) {
    int order[MAXP], n = 0;
    char buf[64];
    if (teamSize > 0) { draw_team_scoreboard(cx, cy); return; }
    for (int i = 0; i < MAXP; i++) if (NP[i].active) order[n++] = i;
    for (int a = 1; a < n; a++)                           /* tri par frags */
        for (int b = a; b > 0 && NP[order[b]].kills > NP[order[b - 1]].kills; b--) {
            int t = order[b]; order[b] = order[b - 1]; order[b - 1] = t;
        }
    float w = ranked ? 820.0f : 600.0f, x = cx - w * 0.5f, y = cy - 170;
    rect(x - 24, y - 56, w + 48, 90 + n * 38.0f, 0.03f, 0.04f, 0.08f, 0.88f);
    text(1, x, y - 14, "JOUEUR", 0.6f, 0.7f, 0.85f, 1);
    text(1, x + 360, y - 14, "FRAGS", 0.6f, 0.7f, 0.85f, 1);
    text(1, x + 480, y - 14, "MORTS", 0.6f, 0.7f, 0.85f, 1);
    if (ranked) text(1, x + 610, y - 14, "CLASSE", 1, 0.8f, 0.3f, 1);
    for (int k = 0; k < n; k++) {
        int i = order[k];
        float yy = y + 30 + k * 38.0f;
        if (i == myId) rect(x - 10, yy - 26, w + 20, 34, 1, 1, 1, 0.08f);
        text(1, x, yy, NP[i].name, pcolor(i)[0], pcolor(i)[1], pcolor(i)[2], NP[i].alive ? 1.0f : 0.5f);
        snprintf(buf, sizeof buf, "%d", NP[i].kills);
        text(1, x + 360, yy, buf, 1, 1, 1, 1);
        snprintf(buf, sizeof buf, "%d", NP[i].deaths);
        text(1, x + 480, yy, buf, 0.8f, 0.8f, 0.85f, 1);
        if (ranked) {                                     /* palier et cote annonces (moi : mon cache) */
            int m = i == myId ? myMmr : rkMmr[i] ? rkMmr[i] : RANK_START, pl = i == myId ? rankPlayed : rkPlay[i];
            const float *c = rank_col(m);
            char tl[32];
            rank_label(m, pl, tl, sizeof tl);
            snprintf(buf, sizeof buf, "%s  %d", tl, m);
            text(0, x + 610, yy - 4, buf, c[0], c[1], c[2], 1);
        }
    }
}

/* fin d'un match classe, sous le tableau des scores : "+18 MMR  ->  OR II" une fois le serveur d'accord */
static void draw_rank_result(float cx, float cy) {
    char buf[128], tl[32];
    int n = 0;
    for (int i = 0; i < MAXP; i++) n += NP[i].active;
    float y = cy - 136 + n * 38.0f + 46;
    rect(cx - 330, y - 30, 660, 46, 0.03f, 0.04f, 0.08f, 0.8f);
    if (rkHadOpp || rkRepState == RS_PENDING) {
        snprintf(buf, sizeof buf, "envoi du resultat au serveur de classement ...   (estimation %+d)", rkHadOpp ? 0 : rkEstimate);
        text_c(0, cx, y, buf, 0.8f, 0.8f, 0.9f, 1);
    } else if (rkRepState == RS_DONE) {
        rank_label(rkLastMmr, rkLastPlayed, tl, sizeof tl);
        snprintf(buf, sizeof buf, "%+d MMR  ->  %s   (%d)", rkLastDelta, tl, rkLastMmr);
        text_c(1, cx, y + 8, buf, rkLastDelta >= 0 ? 0.4f : 1.0f, rkLastDelta >= 0 ? 1.0f : 0.4f, rkLastDelta >= 0 ? 0.5f : 0.35f, 1);
    } else if (rkRepState == RS_FAILED) {
        text_c(0, cx, y, "resultat non confirme par le serveur : cote inchangee", 1, 0.6f, 0.3f, 1);
    }
}

static void draw_killfeed(void) {
    char buf[96];
    for (int k = 0; k < 6; k++) {
        KillFeed *f = &kfeed[k];
        if (f->t <= 0) continue;
        int joinLeave = f->info & (KF_JOIN | KF_LEAVE);
        if (joinLeave) snprintf(buf, sizeof buf, "%s %s", pname(f->victim), f->info == KF_JOIN ? "a rejoint" : "a quitte");
        else if (f->killer == f->victim && f->kind == W_VOID) snprintf(buf, sizeof buf, "%s est tombe dans le vide", pname(f->victim));
        else if (f->killer == f->victim) snprintf(buf, sizeof buf, "%s s'est fait sauter", pname(f->victim));
        else snprintf(buf, sizeof buf, "%s  [%s%s]  %s", pname(f->killer), weaponNames[f->kind % W_COUNT],
                      (f->info & KF_ONESHOT) ? " ONE-SHOT" : (f->info & KF_HEAD) ? " TETE" : "", pname(f->victim));
        int mine = f->killer == myId || f->victim == myId;
        float r = 1, g = mine ? 0.85f : 1, b = mine ? 0.4f : 1;
        if (teamSize > 0 && !joinLeave && f->killer >= 0 && f->killer < MAXP && !mine) {   /* equipes : couleur du tueur */
            const float *c = teamCol[NP[f->killer].team & 1];
            r = 0.55f + 0.45f * c[0]; g = 0.55f + 0.45f * c[1]; b = 0.55f + 0.45f * c[2];
        }
        float tw = fontW[0] * (float)strlen(buf);
        if (gxHudPanel > 0) rect(winW - 40.0f - tw, 64 + k * 26.0f - 19, tw + 20, 25, 0.02f, 0.025f, 0.06f, gxHudPanel * clampf(f->t, 0, 1));
        text(0, winW - 30.0f - tw, 64 + k * 26.0f, buf, r, g, b, clampf(f->t, 0, 1));
    }
}

/* projette un point du monde a l'ecran avec les matrices de la camera */
static int project(V3 p, float *sx, float *sy) {
    float v[4] = { p.x, p.y, p.z, 1 }, e[4], c[4];
    for (int r = 0; r < 4; r++) e[r] = mvMat[r] * v[0] + mvMat[4 + r] * v[1] + mvMat[8 + r] * v[2] + mvMat[12 + r] * v[3];
    for (int r = 0; r < 4; r++) c[r] = prMat[r] * e[0] + prMat[4 + r] * e[1] + prMat[8 + r] * e[2] + prMat[12 + r] * e[3];
    if (c[3] <= 0.05f) return 0;
    *sx = (c[0] / c[3] * 0.5f + 0.5f) * winW;
    *sy = (1 - (c[1] / c[3] * 0.5f + 0.5f)) * winH;
    return 1;
}

static void draw_name_tags(void) {
    for (int i = 0; i < MAXP; i++) {
        if (!np_present(i)) continue;
        int ally = same_team(i, myId);                    /* equipes : un allie se voit a travers les murs */
        V3 head = vadd(NP[i].disp, v3(0, np_height(i) + 0.45f, 0));
        float sx, sy;
        int seen = los(camEye, head);
        if ((!ally && !seen) || !project(head, &sx, &sy)) continue;
        const float *c = pcolor(i);
        if (ally) {                                       /* chevron d'allie au-dessus du nom */
            float a = seen ? 0.95f : 0.6f;
            glColor4f(c[0], c[1], c[2], a);
            glBegin(GL_TRIANGLES);
            glVertex2f(sx - 7, sy - 26); glVertex2f(sx + 7, sy - 26); glVertex2f(sx, sy - 17);
            glEnd();
            text_c(0, sx, sy, NP[i].name, c[0], c[1], c[2], a);
        } else if (ranked) {                              /* mode classe : palier a cote du nom */
            char tag[64], tl[32];
            rank_label(rkMmr[i] ? rkMmr[i] : RANK_START, rkPlay[i], tl, sizeof tl);
            snprintf(tag, sizeof tag, "%s  [%s]", NP[i].name, tl);
            text_c(0, sx, sy, tag, c[0], c[1], c[2], 0.95f);
        } else text_c(0, sx, sy, NP[i].name, c[0], c[1], c[2], 0.95f);
    }
}

/* texte de fin de match : "X REMPORTE LE MATCH", "EQUIPE BLEUE GAGNE", "EGALITE" */
static void winner_text(char *buf, int n) {
    if (teamSize > 0 && matchWinner >= TEAM_DRAW) snprintf(buf, n, "EGALITE");
    else if (teamSize > 0) snprintf(buf, n, "EQUIPE %s GAGNE", teamNames[matchWinner & 1]);
    else snprintf(buf, n, "%s REMPORTE LE MATCH", pname(matchWinner));
}
