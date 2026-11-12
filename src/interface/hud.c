/* VELOCITY - HUD : viseur, vie, munitions, radar
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* vitesse relative avec la cible visee (ou sa propre vitesse si rien n'est vise) */
static float melee_preview(int *hasTarget) {
    V3 eye = eye_pos(), f = view_dir();
    float best = 0.92f, rel = vlen(P.vel);
    *hasTarget = 0;
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive) continue;
        V3 d = vsub(E[i].pos, eye);
        float l = vlen(d);
        if (l > 30 || vdot(vmul(d, 1 / l), f) < best) continue;
        best = vdot(vmul(d, 1 / l), f); rel = rel_speed(E[i].vel); *hasTarget = 1;
    }
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i)) continue;
        V3 d = vsub(np_center(i), eye);
        float l = vlen(d);
        if (l > 30 || vdot(vmul(d, 1 / l), f) < best) continue;
        best = vdot(vmul(d, 1 / l), f); rel = rel_speed(np_vel(i)); *hasTarget = 1;
    }
    return rel;
}
/* Lance : vitesse de rapprochement v_c vers la cible visee dans le cone de la lance (sinon sa propre vitesse) */
static float lance_preview(int *hasTarget) {
    V3 eye = eye_pos(), f = view_dir();
    float best = LANCE_CONE, vc = vlen(P.vel);
    *hasTarget = 0;
    for (int k = 0; k < MAXE + MAXP; k++) {
        int pl = k >= MAXE, i = pl ? k - MAXE : k;
        V3 c, tv;
        if (!pl) { if (!E[i].alive) continue; c = E[i].pos; tv = E[i].vel; }
        else { if (!np_target(i)) continue; c = np_center(i); tv = np_vel(i); }
        V3 d = vsub(c, eye);
        float l = fmaxf(vlen(d), 0.01f), cs = vdot(vmul(d, 1 / l), f);
        if (l > LANCE_PREVIEW || cs < best) continue;
        best = cs; vc = vdot(vsub(P.vel, tv), vmul(d, 1 / l)); *hasTarget = 1;
    }
    return vc;
}

/* interface du spectateur : classement en direct, qui on suit, minuteur */
static void draw_spec_hud(float cx, float cy) {
    char buf[128];
    int order[MAXP], n = 0, left = (int)fmaxf(0, MATCH_TIME - matchT);
    for (int i = 0; i < MAXP; i++) if (NP[i].active) order[n++] = i;
    for (int a = 1; a < n; a++)
        for (int b = a; b > 0 && NP[order[b]].kills > NP[order[b - 1]].kills; b--) {
            int t = order[b]; order[b] = order[b - 1]; order[b - 1] = t;
        }
    text_c(1, cx, 44, "SPECTATEUR", 0.5f, 0.9f, 1.0f, 1);
    if (specFollow >= 0) snprintf(buf, sizeof buf, "tu suis %s   -   [%s] joueur suivant", pname(specFollow), bind_text(A_FIRE));
    else snprintf(buf, sizeof buf, "camera libre   -   [%s] suivre un joueur   [%s] vite", bind_text(A_FIRE), bind_text(A_DASH));
    text_c(0, cx, 70, buf, 0.8f, 0.85f, 0.95f, 1);
    snprintf(buf, sizeof buf, "%d:%02d   frags %d", left / 60, left % 60, fragLimit);
    text(0, 30, 40, buf, 0.85f, 0.85f, 0.95f, 1);
    for (int k = 0; k < n; k++) {
        int i = order[k];
        snprintf(buf, sizeof buf, "%d  %s", NP[i].kills, NP[i].name);
        text(1, 30, 78 + k * 30.0f, buf, pcolor(i)[0], pcolor(i)[1], pcolor(i)[2], NP[i].alive ? 1.0f : 0.5f);
    }
    draw_killfeed();
    if (matchOver) {
        winner_text(buf, sizeof buf);
        text_c(2, cx, cy - 250, buf, 1, 0.85f, 0.3f, 1);
        draw_scoreboard(cx, cy);
    }
}

/* --- Scie : HUD ------------------------------------------------------- */
static void saw_hud_name(char *out, int n) {              /* panneau d'arme : ou est la scie */
    Proj *s = saw_mine();
    if (!s) { snprintf(out, n, "SCIE  EN MAIN"); return; }
    switch (s->sawState) {
    case SAW_PLANTED: snprintf(out, n, "SCIE  PLANTEE %d m", (int)vlen(vsub(s->pos, pcenter()))); break;
    case SAW_RETURN: case SAW_RECALL: snprintf(out, n, "SCIE  RETOUR"); break;
    case SAW_PARRIED: snprintf(out, n, "SCIE  PAREE"); break;
    case SAW_LOST: snprintf(out, n, "SCIE  PERDUE %.1f s", fmaxf(0, SAW_LOST_T - s->sawT)); break;
    default: snprintf(out, n, "SCIE  EN VOL"); break;
    }
}
/* renvois restants, indicateur de renvoi autour du viseur, direction de la scie plantee, rappel au grappin */
static void draw_saw_hud(float cx, float cy) {
    if (P.dead || (P.loadout[0] != WP_SAW && P.loadout[1] != WP_SAW)) return;
    Proj *s = saw_mine();
    int left = SAW_BOUNCE_CHAIN - (s ? s->sawBounces : 0);
    if (P.weapon == WP_SAW)                               /* pastilles : renvois encore possibles */
        for (int i = 0; i < SAW_BOUNCE_CHAIN; i++)
            rect(winW - 276.0f + i * 14.0f, winH - 41.0f, 10, 10, i < left ? 1.0f : 0.15f, i < left ? 0.6f : 0.12f,
                 i < left ? 0.3f : 0.15f, 0.9f);
    if (!s) return;
    V3 eye = eye_pos(), vd = view_dir();
    int st = s->sawState;
    if ((st == SAW_RETURN || st == SAW_RECALL || st == SAW_PARRIED) && left > 0 && P.meleeCd <= 0) {
        float r = (MELEE_RANGE + 1.5f) * powf(PARRY_RANGE_SHRINK, (float)s->sawBounces);
        float win = r + (st == SAW_RECALL ? SAW_RECALL_SPEED : SAW_RETURN_SPEED) * PARRY_WINDOW;
        V3 d = vsub(s->pos, eye);
        float dist = vlen(d);
        if (dist < win) {                                 /* le cercle se resserre ; blanc = a portee de parry */
            int inRange = dist <= r && vdot(vmul(d, 1.0f / fmaxf(dist, 0.01f)), vd) >= 0.3f;
            float rr = 14 + 26 * clampf((dist - r) / (win - r), 0, 1);
            glLineWidth(2);
            if (inRange) glColor4f(1, 1, 1, 1); else glColor4f(0.4f, 0.85f, 1, 0.9f);
            glBegin(GL_LINE_LOOP);
            for (int k = 0; k < 24; k++) glVertex2f(cx + rr * cosf(k * PI / 12), cy + rr * sinf(k * PI / 12));
            glEnd();
        }
    }
    if (st != SAW_PLANTED) return;
    float sx, sy, dm = vlen(vsub(s->pos, pcenter()));
    char b[24];
    snprintf(b, sizeof b, "%d m", (int)dm);
    if (project(s->pos, &sx, &sy) && sx > 20 && sx < winW - 20 && sy > 20 && sy < winH - 20) {
        glLineWidth(2);                                   /* losange dore, meme a travers les murs */
        glColor4f(1, 0.8f, 0.3f, 0.95f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(sx, sy - 9); glVertex2f(sx + 9, sy); glVertex2f(sx, sy + 9); glVertex2f(sx - 9, sy);
        glEnd();
        text_c(0, sx, sy + 24, b, 1, 0.8f, 0.3f, 0.95f);
    } else {                                              /* hors de l'ecran : fleche doree sur un cercle */
        V3 d = vsub(s->pos, P.pos);
        float ang = atan2f(d.x, -d.z) - P.yaw, dx = sinf(ang), dy = -cosf(ang);
        glColor4f(1, 0.8f, 0.3f, 0.9f);
        glBegin(GL_TRIANGLES);
        glVertex2f(cx + dx * 150, cy + dy * 150);
        glVertex2f(cx + dx * 128 - dy * 14, cy + dy * 128 + dx * 14);
        glVertex2f(cx + dx * 128 + dy * 14, cy + dy * 128 - dx * 14);
        glEnd();
        text_c(0, cx + dx * 110, cy + dy * 110 + 6, b, 1, 0.8f, 0.3f, 0.9f);
    }
    if (P.hook == HOOK_NONE && P.hookCd <= 0) {           /* le grappin rappellerait la scie */
        float t;
        if (ray_sphere(eye, vd, s->pos, SAW_HOOK_R, GRAPPLE_RANGE, &t) && los(eye, vadd(s->pos, vmul(s->sawN, 0.25f))))
            text_c(0, cx, cy + 46, "GRAPPIN -> RAPPEL SCIE", 1, 0.8f, 0.3f, 1);
    }
}
/* --- fin Scie : HUD --- */

/* --- Tesla : HUD --- */
static void tesla_hud_bar(float x, float y) {             /* barre de batterie : 200 px, graduee tous les 20 (1 s de tir) */
    float b = P.ammo[WP_TESLA] / (float)ammoDef[WP_TESLA].max;
    int blink = P.ammo[WP_TESLA] < 20 && fmodf(gameTime, 0.4f) < 0.2f;
    if (tesla_stunned()) rect(x, y, 200, 8, 1, 0.2f, 0.15f, 0.9f);
    else rect(x, y, 200 * b, 8, blink ? 1.0f : 0.3f, blink ? 0.25f : 0.9f, blink ? 0.2f : 1.0f, 0.95f);
    for (int k = 1; k < 5; k++) rect(x + k * 40.0f - 1, y, 2, 8, 0.05f, 0.05f, 0.1f, 0.9f);
}
static void draw_tesla_hud(float cx, float cy) {
    char buf[48];
    float range = tesla_range(P.vel);
    snprintf(buf, sizeof buf, "PORTEE %d M", (int)(range + 0.5f));
    text(0, winW - 236.0f - fontW[0] * (float)strlen(buf), winH - 47.0f, buf, 0.5f, 0.8f, 1, 0.9f);
    if (tesla_stunned()) text_c(1, cx, cy + 46, "SURCHARGE", 1, 0.3f, 0.2f, 0.95f);
    else if (P.ammo[WP_TESLA] <= 0) text_c(0, cx, cy + 46, "BATTERIE VIDE", 1, 0.35f, 0.3f, 0.95f);
    TeslaTgt t[3];                                        /* viseur : anneau cyan si l'arc trouverait une cible */
    int n = tesla_trace(eye_pos(), view_dir(), range, dmMode ? myId : -1, t, NULL, NULL);
    glLineWidth(2);
    if (n) glColor4f(0.4f, 0.85f, 1, 0.95f); else glColor4f(0.6f, 0.6f, 0.65f, 0.5f);
    glBegin(GL_LINE_LOOP);
    for (int s = 0; s < 24; s++) glVertex2f(cx + 16 * cosf(s * PI / 12), cy + 16 * sinf(s * PI / 12));
    glEnd();
    int nr = tesla_relays_of(myId);
    if (nr) {
        snprintf(buf, sizeof buf, "RELAIS %d/%d", nr, TESLA_RELAY_MAX);
        text(0, winW - 30.0f - fontW[0] * (float)strlen(buf), winH - 104.0f, buf, 0.45f, 0.75f, 1, 0.95f);
    }
}
/* --- fin Tesla : HUD --- */

/* --- Ventouse : HUD. Alerte "mine sur toi" (cadre rouge pulsant, barre d'armement puis de meche) ; pastilles de mes
 * mines actives (la plus ancienne encadree : c'est elle que vise DETONER UNE) et rappel des deux touches --- */
static void draw_mine_hud(float cx, float cy) {
    char buf[96];
    if (P.dead) return;
    for (int i = 0; i < MAXPR; i++) {
        Proj *p = &PR[i];
        if (!p->alive || p->kind != PR_MINE || p->mineState != MS_PLAYER || p->mineAttach != myId + 1 || p->owner == myId) continue;
        float a = 0.4f + 0.4f * sinf(gameTime * 18), W = (float)winW, H = (float)winH;
        rect(0, 0, W, 10, 1, 0.1f, 0.1f, a); rect(0, H - 10, W, 10, 1, 0.1f, 0.1f, a);
        rect(0, 10, 10, H - 20, 1, 0.1f, 0.1f, a); rect(W - 10, 10, 10, H - 20, 1, 0.1f, 0.1f, a);
        text_c(1, cx, cy + 74, "MINE SUR TOI ! PARE !", 1, 0.3f, 0.25f, 0.95f);
        int arming = p->mineArmT > 0;
        float k = arming ? p->mineArmT / MINE_ARM_PLAYER : 1 - p->mineT / MINE_FUSE;
        rect(cx - 80, cy + 84, 160, 5, 0.1f, 0.1f, 0.15f, 0.7f);
        rect(cx - 80, cy + 84, 160 * clampf(k, 0, 1), 5, 1, arming ? 0.85f : 0.25f, 0.2f, 0.95f);
        break;
    }
    Proj *own[MAXPR];
    int n = 0;
    for (int i = 0; i < MAXPR && n < MAXPR; i++) if (mine_own(&PR[i])) own[n++] = &PR[i];
    if (!n && P.weapon != WP_MINE) return;
    for (int i = 1; i < n; i++)                           /* tri par ordre de lancer */
        for (int j = i; j > 0 && own[j]->mineSeq < own[j - 1]->mineSeq; j--) { Proj *t = own[j]; own[j] = own[j - 1]; own[j - 1] = t; }
    Proj *old = mine_oldest();
    float y = winH - 150.0f;
    text(0, winW - 36.0f - MINE_MAX_ACTIVE * 20.0f - fontW[0] * 5, y + 12, "MINES", 0.3f, 1, 0.7f, 0.9f);
    for (int s = 0; s < MINE_MAX_ACTIVE; s++) {
        float x = winW - 30.0f - (MINE_MAX_ACTIVE - s) * 20.0f;
        rect(x, y, 14, 14, 0.08f, 0.15f, 0.12f, 0.7f);
        if (s < n) {
            Proj *p = own[s];
            int blink = p->mineState == MS_FLY || p->mineDet ? fmodf(gameTime, 0.2f) < 0.1f : 1;
            rect(x + 2, y + 2, 10, 10, 0.3f, 1, 0.7f, blink ? 0.95f : 0.3f);
            if (p == old) {                               /* cadre : la prochaine a sauter avec DETONER UNE */
                rect(x - 2, y - 2, 18, 2, 1, 1, 1, 0.9f); rect(x - 2, y + 14, 18, 2, 1, 1, 1, 0.9f);
                rect(x - 2, y, 2, 14, 1, 1, 1, 0.9f); rect(x + 14, y, 2, 14, 1, 1, 1, 0.9f);
            }
        }
    }
    if (n) {
        snprintf(buf, sizeof buf, "[%s] DETONER TOUT", bind_text(A_DETONATE_ALL));
        text(0, winW - 30.0f - fontW[0] * (float)strlen(buf), y - 10, buf, 0.75f, 0.95f, 0.85f, 0.9f);
        snprintf(buf, sizeof buf, "[%s] DETONER UNE", bind_text(A_DETONATE_ONE));
        text(0, winW - 30.0f - fontW[0] * (float)strlen(buf), y - 30, buf, 0.75f, 0.95f, 0.85f, 0.9f);
    }
}
/* --- fin Ventouse : HUD --- */

/* radar des allies (mode equipes) : carte vue de dessus, nord en haut, moi = fleche, allies = points */
static void draw_radar(float x, float y, float sz) {
    float W = fmaxf(fmaxf(-mapBox.mn.x, mapBox.mx.x), fmaxf(-mapBox.mn.z, mapBox.mx.z)), k = sz / (2 * W);
    rect(x, y, sz, sz, 0.02f, 0.03f, 0.07f, 0.6f);
    rect(x, y, sz, 2, 0.5f, 0.9f, 1.0f, 0.5f);
    for (int i = mapFirst; i < nb; i++) {                 /* reperes : grandes structures seulement */
        Box *b = &boxes[i];
        if (b->mx.y - b->mn.y < 0.9f || (b->mx.x - b->mn.x) * (b->mx.z - b->mn.z) < 30) continue;
        rect(x + (b->mn.x + W) * k, y + (b->mn.z + W) * k, (b->mx.x - b->mn.x) * k, (b->mx.z - b->mn.z) * k, 0.4f, 0.45f, 0.6f, 0.35f);
    }
    for (int i = 0; i < MAXP; i++) {
        if (i == myId || !NP[i].active || !NP[i].alive || !same_team(i, myId)) continue;
        const float *c = pcolor(i);
        float px = x + (NP[i].disp.x + W) * k, py = y + (NP[i].disp.z + W) * k;
        rect(px - 3, py - 3, 7, 7, c[0], c[1], c[2], 1);
    }
    if (!P.dead) {
        float px = x + (P.pos.x + W) * k, py = y + (P.pos.z + W) * k, s1 = sinf(P.yaw), c1 = -cosf(P.yaw);
        glColor4f(1, 1, 1, 1);
        glBegin(GL_TRIANGLES);
        glVertex2f(px + s1 * 8, py + c1 * 8);
        glVertex2f(px - s1 * 4 + c1 * 5, py - c1 * 4 - s1 * 5);
        glVertex2f(px - s1 * 4 - c1 * 5, py - c1 * 4 + s1 * 5);
        glEnd();
    }
}

/* trainee de la barre de vie : apres un coup, la partie perdue reste HP_GHOST_HOLD s puis se vide */
static float hpGhost = 100, hpLast = 100;
static double hpHitAt = -10, hpHealAt = -10;
static void hp_ghost_tick(float hp, double t) {
    static double lastT;
    float dt = clampf((float)(t - lastT), 0, 0.1f);
    lastT = t;
    if (hp < hpLast) hpHitAt = t;
    if (hp > hpLast + 0.5f && hpLast > 0) hpHealAt = t;   /* soin (pas le respawn depuis 0) */
    hpLast = hp;
    if (hp >= hpGhost) hpGhost = hp;
    else if (t - hpHitAt > HP_GHOST_HOLD) hpGhost = fmaxf(hp, hpGhost - HP_GHOST_RATE * dt);
}

static void draw_killcam_hud(float cx, float cy) {
    char buf[96];
    V3 tg = kcLast;
    float k = clampf(P.deathT / KILLCAM_TURN, 0, 1), sx, sy;
    const float *c = dmMode ? pcolor(kcKiller) : NULL;
    float cr = c ? c[0] : 1, cg = c ? c[1] : 0.3f, cb = c ? c[2] : 0.25f;
    rect(0, 0, (float)winW, 70, 0, 0, 0, 0.55f * k);      /* bandes de cinema */
    rect(0, winH - 70.0f, (float)winW, 70, 0, 0, 0, 0.55f * k);
    if (project(tg, &sx, &sy)) {                          /* crochets autour du tueur, qui se resserrent */
        float r = 70 - 30 * k, l = 16;
        glLineWidth(3);
        glColor4f(cr, cg, cb, k);
        glBegin(GL_LINES);
        for (int q = 0; q < 4; q++) {
            float qx = q & 1 ? 1.0f : -1.0f, qy = q & 2 ? 1.0f : -1.0f, x = sx + qx * r, y = sy + qy * r * 1.3f;
            glVertex2f(x, y); glVertex2f(x - qx * l, y); glVertex2f(x, y); glVertex2f(x, y - qy * l);
        }
        glEnd();
        glLineWidth(1);
    }
    if (dmMode) snprintf(buf, sizeof buf, "ELIMINE PAR %s", pname(kcKiller));
    else snprintf(buf, sizeof buf, "TUE PAR : %s", kcEnemy >= 0 ? enemyName[E[kcEnemy].type] : "?");
    float tw = fontW[2] * (float)strlen(buf);
    hud_panel(cx - tw * 0.5f - 24, 96, tw + 48, 112);
    text_c(2, cx, 150, buf, cr, cg, cb, 1);
    if (dmMode) {
        snprintf(buf, sizeof buf, "[%s%s]   il lui reste %d PV", weaponNames[kcWeapon % W_COUNT],
                 (kcOneShot & HF_ONESHOT) ? ((kcOneShot & HF_HEAD) ? " - ONE-SHOT A LA TETE" : " - ONE-SHOT") :
                 (kcOneShot & HF_HEAD) ? " - TETE" : "", (int)ceilf(NP[kcKiller].hp));
        text_c(1, cx, 192, buf, 0.9f, (kcOneShot & HF_ONESHOT) ? 0.75f : 0.9f, (kcOneShot & HF_ONESHOT) ? 0.35f : 0.95f, 1);
        snprintf(buf, sizeof buf, "respawn dans %.1f s", fmaxf(0, RESPAWN_TIME - P.deathT));
        text_c(0, cx, winH - 30.0f, buf, 0.8f, 0.8f, 0.85f, 1);
    }
    (void)cy;
}

/* jauge de style : rang (lettre), mot, barre du rang en cours, derniers bonus ; a droite, a mi-hauteur */
static void draw_style_hud(void) {
    if (!style_on() || P.dead || spectating || (style <= 0.5f && styleFeed[0].t <= 0)) return;
    int r = styleRank;
    const float *c = styleCol[r];
    float x = winW - 250.0f, y = winH * 0.36f, fl = fmaxf(styleFlash, 0), hurt = fmaxf(-styleFlash, 0);
    float lo = styleAt[r], hi = r + 1 < ST_COUNT ? styleAt[r + 1] : STYLE_MAX, k = clampf((style - lo) / (hi - lo), 0, 1);
    float a = clampf(style / 20, 0.35f, 1);
    hud_panel(x - 14, y - 10, 236, 82 + 18.0f * STYLE_FEED + 8);
    float sh = 1 + 0.5f * fl, jx = hurt * 6 * sinf((float)nowT * 90);
    for (int g = 0; g < 3 && r >= ST_A; g++)              /* A et S : halo qui pulse autour de la lettre */
        text(2, x + jx - 2 + g * 2, y + 52 - g, styleName[r], c[0], c[1], c[2], 0.12f + 0.08f * sinf((float)nowT * 8));
    text(2, x + jx, y + 52, styleName[r], c[0] * sh, c[1] * sh, c[2] * sh, a);
    text(1, x + 54, y + 20, styleWord[r], c[0], c[1], c[2], a);
    char buf[24];
    snprintf(buf, sizeof buf, "x%.2g", styleMultTab[r]);
    if (!dmMode) text(0, x + 54, y + 44, buf, 0.85f, 0.85f, 0.9f, a * 0.8f);
    rect(x + 54, y + 56, 150, 6, 0.1f, 0.1f, 0.15f, 0.8f);
    rect(x + 54, y + 56, 150 * k, 6, c[0], c[1], c[2], 0.95f);
    if (hurt > 0) rect(x + 54, y + 56, 150, 6, 1, 0.2f, 0.2f, hurt * 0.6f);
    for (int i = 0; i < STYLE_FEED; i++) {                /* +BONUS qui glissent et s'effacent */
        if (styleFeed[i].t <= 0) continue;
        char fb[40];
        snprintf(fb, sizeof fb, "+ %s", styleFeed[i].txt);
        float t = styleFeed[i].t, al = clampf(t / 0.5f, 0, 1);
        text(0, x + 54 + (1.6f - t) * 8, y + 82 + i * 18.0f, fb, 1, 0.9f, 0.6f, al);
    }
}

static void draw_race_hud(void);                          /* course_hud.c */
static void draw_modes_hud(void);                         /* modes_hud.c */
static void draw_shop(void);
static void vg_draw_info(void);                           /* src/interface/vagues_hud.c */
static void vg_draw_hud(float cx, float cy);
static void vg_draw_death(float cx, float cy);
static void draw_hud(void) {
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0, winW, winH, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    float cx = winW * 0.5f, cy = winH * 0.5f;
    char buf[128];

    if (appState != APP_GAME) {
        if (settingsOpen && appState == APP_MENU) draw_settings(cx, cy);
        else if (loadoutOpen && appState == APP_MENU) draw_loadout(cx, cy);
        else if (hostOpen && appState == APP_MENU) draw_host_menu(cx, cy);
        else if (skinOpen && appState == APP_MENU) draw_skin_menu(cx, cy);
        else if (challengeOpen && appState == APP_MENU) draw_challenge_menu(cx, cy);
        else if (appState == APP_MENU) draw_main_menu(cx, cy);
        else draw_join_menu(cx, cy);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        return;
    }
    if (dmMode) draw_name_tags();
    if (spectating) {
        draw_spec_hud(cx, cy);
        if (!captured && !shotMode && !botMode && !ecoShopping && !vg_menu_open()) {
            if (settingsOpen) draw_settings(cx, cy); else if (loadoutOpen) draw_loadout(cx, cy); else draw_pause(cx, cy);
        }
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        return;
    }

    float hsNow = hlen(P.vel);
    if (screenFx) {
        /* vignette : bords sombres, rouges et pulsants quand on est bas en PV */
        float low = (!P.dead && P.hp < 35) ? (0.5f + 0.25f * sinf(gameTime * 6)) * (1 - P.hp / 35) : 0;
        float vr = low * 0.8f, va = 0.45f + low * 0.35f, hx = winW * 0.5f, hy = winH * 0.5f;
        float rin = sqrtf(hx * hx + hy * hy) * 0.62f, rout = sqrtf(hx * hx + hy * hy) * 1.05f;
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= 40; i++) {
            float ang = i * (2 * PI / 40);
            glColor4f(vr, 0, 0, 0); glVertex2f(hx + cosf(ang) * rin, hy + sinf(ang) * rin);
            glColor4f(vr, 0, 0, va); glVertex2f(hx + cosf(ang) * rout, hy + sinf(ang) * rout);
        }
        glEnd();
        if (hsNow > 16 && !P.dead) {                      /* lignes de vitesse sur les bords */
            float kk = clampf((hsNow - 16) / 24, 0, 1), hd = sqrtf(hx * hx + hy * hy);
            glBegin(GL_TRIANGLES);
            for (int i = 0; i < (int)(10 + kk * 30); i++) {
                float ang = frange(0, 2 * PI), r0 = hd * frange(0.55f, 0.8f), wid = frange(0.002f, 0.006f);
                glColor4f(1, 1, 1, 0); glVertex2f(hx + cosf(ang) * r0, hy + sinf(ang) * r0);
                glColor4f(1, 1, 1, 0.12f + 0.3f * kk);
                glVertex2f(hx + cosf(ang - wid) * hd * 1.1f, hy + sinf(ang - wid) * hd * 1.1f);
                glVertex2f(hx + cosf(ang + wid) * hd * 1.1f, hy + sinf(ang + wid) * hd * 1.1f);
            }
            glEnd();
        }
    } else if (P.hp < 30 && !P.dead) {
        rect(0, 0, (float)winW, (float)winH, 0.6f, 0, 0, 0.12f + 0.06f * sinf(gameTime * 6));
    }
    if (P.hurtFlash > 0) rect(0, 0, (float)winW, (float)winH, 0.8f, 0.0f, 0.05f, P.hurtFlash * 0.35f);
    if (maps[curMap].killY > -999 && !P.dead && !spectating && P.vel.y < 0 && P.pos.y < maps[curMap].killY + VOID_WARN_H) {
        float k = clampf(1 - (P.pos.y - maps[curMap].killY) / VOID_WARN_H, 0, 1), W = (float)winW, H = (float)winH, m = H * 0.3f;
        glBegin(GL_QUADS);                                /* kill zone : vignette rouge, on a encore le temps d'un grappin */
        for (int e = 0; e < 4; e++) {                     /* bord haut, bas, gauche, droite : rouge au bord, transparent vers le centre */
            float x0 = e == 3 ? W - m : 0, y0 = e == 1 ? H - m : 0, x1 = e == 2 ? m : W, y1 = e == 0 ? m : H;
            float ao[4] = { k * 0.7f, k * 0.7f, k * 0.7f, k * 0.7f };   /* alpha des coins : (x0,y0) (x1,y0) (x1,y1) (x0,y1) */
            if (e == 0) ao[2] = ao[3] = 0;
            if (e == 1) ao[0] = ao[1] = 0;
            if (e == 2) ao[1] = ao[2] = 0;
            if (e == 3) ao[0] = ao[3] = 0;
            glColor4f(0.9f, 0, 0.05f, ao[0]); glVertex2f(x0, y0);
            glColor4f(0.9f, 0, 0.05f, ao[1]); glVertex2f(x1, y0);
            glColor4f(0.9f, 0, 0.05f, ao[2]); glVertex2f(x1, y1);
            glColor4f(0.9f, 0, 0.05f, ao[3]); glVertex2f(x0, y1);
        }
        glEnd();
    }
    for (int i = 0; i < MAXDN; i++) {                     /* chiffres de degats */
        DmgNum *d = &DN[i];
        float sx, sy;
        if (d->t <= 0 || !project(vadd(d->pos, v3(0, (0.9f - d->t) * 1.4f, 0)), &sx, &sy)) continue;
        char nb[16];
        snprintf(nb, sizeof nb, "%.0f", d->val);
        float a = clampf(d->t * 2, 0, 1);
        if (d->kind == DN_ONESHOT) {                      /* one-shot : gros chiffre or et rouge, "ONE-SHOT" au-dessus */
            text_c(2, sx, sy, nb, 1, 0.8f, 0.25f, a);
            text_c(0, sx, sy - 34, "ONE-SHOT", 1, 0.3f, 0.25f, a);
        } else if (d->kind == DN_HEAD) {                  /* tete : chiffre dore, "TETE" au-dessus */
            text_c(1, sx, sy, nb, 1, 0.9f, 0.3f, a);
            text_c(0, sx, sy - 26, "TETE", 1, 0.95f, 0.6f, a);
        } else if (d->val >= 60) text_c(1, sx, sy, nb, 1, 0.45f, 0.2f, a);
        else if (d->val >= 35) text_c(0, sx, sy, nb, 1, 0.85f, 0.3f, a);
        else text_c(0, sx, sy, nb, 1, 1, 1, a);
    }
    if (hurtDirT > 0 && !P.dead) {                        /* d'ou viennent les degats */
        V3 d = vsub(hurtFrom, P.pos);
        float ang = atan2f(d.x, -d.z) - P.yaw, hx = winW * 0.5f, hy = winH * 0.5f;
        float dx = sinf(ang), dy = -cosf(ang), a = clampf(hurtDirT, 0, 1);
        glColor4f(1, 0.15f, 0.1f, 0.85f * a);
        glBegin(GL_TRIANGLES);
        glVertex2f(hx + dx * 170, hy + dy * 170);
        glVertex2f(hx + dx * 140 - dy * 22, hy + dy * 140 + dx * 22);
        glVertex2f(hx + dx * 140 + dy * 22, hy + dy * 140 - dx * 22);
        glEnd();
    }
    if (parryFlash > 0) rect(0, 0, (float)winW, (float)winH, 1, 1, 0.9f, parryFlash * 0.8f);

    if (!P.dead) {                                         /* viseur */
        glLineWidth(2);
        glColor4f(1, 1, 1, 0.9f);
        glBegin(GL_LINES);
        glVertex2f(cx - 12, cy); glVertex2f(cx - 4, cy); glVertex2f(cx + 4, cy); glVertex2f(cx + 12, cy);
        glVertex2f(cx, cy - 12); glVertex2f(cx, cy - 4); glVertex2f(cx, cy + 4); glVertex2f(cx, cy + 12);
        glEnd();
        rect(cx - 1, cy - 1, 2, 2, 1, 1, 1, 1);
        if (killMarkT > 0) {                              /* grosse croix rouge : kill confirme */
            float a = clampf(killMarkT / 0.45f, 0, 1), r1 = 30 - 8 * a;
            glLineWidth(5);
            glColor4f(1, 0.15f, 0.1f, a);
            glBegin(GL_LINES);
            glVertex2f(cx - r1, cy - r1); glVertex2f(cx - 8, cy - 8); glVertex2f(cx + r1, cy - r1); glVertex2f(cx + 8, cy - 8);
            glVertex2f(cx - r1, cy + r1); glVertex2f(cx - 8, cy + 8); glVertex2f(cx + r1, cy + r1); glVertex2f(cx + 8, cy + 8);
            glEnd();
        }
        if (hitMarker > 0) {
            float a = hitMarker / 0.18f;
            glLineWidth(3);
            glColor4f(1, 0.3f, 0.3f, a);
            glBegin(GL_LINES);
            glVertex2f(cx - 16, cy - 16); glVertex2f(cx - 7, cy - 7); glVertex2f(cx + 16, cy - 16); glVertex2f(cx + 7, cy - 7);
            glVertex2f(cx - 16, cy + 16); glVertex2f(cx - 7, cy + 7); glVertex2f(cx + 16, cy + 16); glVertex2f(cx + 7, cy + 7);
            glEnd();
        }
    }

    if (!killcam_active()) {                              /* camera de mort : le bas du HUD laisse voir la scene */
        /* vitesse */
        hud_panel(cx - 92, winH - 120.0f, 184, 108);
        float hs = hlen(P.vel), k = clampf((hs - RUN_SPEED) / 20, 0, 1);
        snprintf(buf, sizeof buf, "%.1f", hs);
        text_c(2, cx, winH - 70.0f, buf, 1 - k * 0.6f, 1 - k * 0.2f, 1, 1);
        text_c(0, cx, winH - 46.0f, "m/s", 0.7f, 0.7f, 0.8f, 0.9f);
        for (int i = 0; i < 2; i++) {                         /* charges de dash */
            float fill = clampf(P.dashCharge - i, 0, 1);
            float x = cx - 52 + i * 56.0f, y = winH - 32.0f;
            rect(x, y, 48, 8, 0.1f, 0.1f, 0.15f, 0.7f);
            rect(x, y, 48 * fill, 8, fill >= 1 ? 0.4f : 0.3f, fill >= 1 ? 0.9f : 0.5f, 1, 0.95f);
        }
        float hookFill = clampf(1 - P.hookCd / GRAPPLE_CD, 0, 1);  /* recharge du grappin */
        rect(cx - 52, winH - 18.0f, 104, 5, 0.1f, 0.1f, 0.15f, 0.7f);
        rect(cx - 52, winH - 18.0f, 104 * (P.hook == HOOK_NONE ? hookFill : 0), 5, 1, 0.8f, 0.35f, 0.95f);
        int hookable = 0;                                     /* 1 = mur a portee, 2 = ennemi a portee */
        if (!P.dead && P.hook == HOOK_NONE && P.hookCd <= 0) {
            V3 eye = eye_pos(), vd = view_dir();
            float tw = ray_world(eye, vd, GRAPPLE_RANGE, 0), te;
            if (tw < GRAPPLE_RANGE) hookable = 1;
            for (int i = 0; i < MAXE; i++)
                if (E[i].alive && ray_sphere(eye, vd, E[i].pos, enemy_radius(E[i].type) + 0.3f, tw, &te)) hookable = 2;
            for (int i = 0; i < MAXP; i++) {
                V3 mn, mx;
                if (!np_target(i)) continue;
                np_aabb(i, 0.3f, &mn, &mx);
                if (ray_box(eye, vd, mn, mx, tw, &te, NULL)) hookable = 2;
            }
        }
        if (hookable) {
            glLineWidth(2);                                   /* le viseur montre qu'on peut s'accrocher */
            if (hookable == 2) glColor4f(1, 0.25f, 0.25f, 0.95f); else glColor4f(1, 0.8f, 0.35f, 0.9f);
            glBegin(GL_LINES);
            for (int k = 0; k < 4; k++) {
                float sx = (k & 1) ? 1.0f : -1.0f, sy = (k & 2) ? 1.0f : -1.0f;
                glVertex2f(cx + sx * 22, cy + sy * 22); glVertex2f(cx + sx * 15, cy + sy * 22);
                glVertex2f(cx + sx * 22, cy + sy * 22); glVertex2f(cx + sx * 22, cy + sy * 15);
            }
            glEnd();
        }
        /* axe corps a corps : degats et portee selon la vitesse relative avec la cible visee */
        int aimed;
        /* fenetre etroite : les deux panneaux montent au-dessus de la vie et de l'arme */
        int compact = winW < 1300;
        float by = compact ? winH - 90.0f : (float)winH;
        float sp3 = melee_preview(&aimed), mk = clampf(sp3 / 30, 0, 1), mx = compact ? 30 : cx - 330;
        {
            char mt[64];
            int blink = dryHintT > 0 && fmodf(dryHintT, 0.4f) < 0.2f;   /* a sec : le panneau de melee clignote */
            if (dryHintT > 0) snprintf(mt, sizeof mt, "A SEC : MELEE = MUNITIONS");
            else if (P.meleeKind == MK_LANCE)                 /* Lance : estoc / joute */
                snprintf(mt, sizeof mt, P.lanceState == LS_COUCH ? "LANCE COUCHEE +%d" : "LANCE  +%d pts",
                         P.lanceState == LS_COUCH ? PTS_LANCE_JOUST : meleePts[MK_LANCE]);
            else if (P.meleeKind == MK_FIST)                  /* Poings : points du combo deja gagnes */
                snprintf(mt, sizeof mt, "%s  +%d pts (%d/%d)", meleeNames[MK_FIST], meleePts[MK_FIST], P.fistPts, PTS_FIST_COMBO_MAX);
            else snprintf(mt, sizeof mt, "%s%s  +%d pts", meleeNames[P.meleeKind], aimed ? " (vs cible)" : "", meleePts[P.meleeKind]);
            hud_panel(mx - 12, by - 104.0f, fmaxf(204, fontW[0] * (float)strlen(mt) + 24), 78);
            text(0, mx, by - 78.0f, mt, blink ? 1.0f : 0.75f, blink ? 0.6f : 0.75f, blink ? 0.2f : 0.85f, 0.9f);
        }
        float pvp = dmMode ? PVP_SCALE : 1.0f, mcd = MELEE_CD;
        if (P.meleeKind == MK_ANVIL) {                        /* Enclume : coup au sol, ou pique prevu en l'air */
            V3 at; float vy, R;
            int air = !P.grounded && (P.anvilState == ANVIL_DIVE || anvil_ground_dist(P.pos) >= SLAM_MIN_H);
            mcd = ANVIL_CD;
            if (air && slam_predict(&at, &vy, &R, NULL)) {
                float fall = (P.anvilState == ANVIL_DIVE ? P.anvilStartY : P.pos.y) - at.y;
                snprintf(buf, sizeof buf, "PIQUE %d dmg R %.1fm mur %.1fm", (int)(slam_dmg(vy, fall) * pvp), R, slam_hw(fall));
            } else snprintf(buf, sizeof buf, "%d dmg %.1fm", (int)(anvil_dmg(sp3) * pvp), anvil_range(sp3));
        } else if (P.meleeKind == MK_LANCE) {                 /* Lance : v_c vers la cible visee, degats de la joute (plafond PvP) */
            int la;
            float vc = lance_preview(&la), jd = lance_dmg(fmaxf(vc, 0)) * pvp;
            if (dmMode) jd = fminf(jd, PVP_HIT_CAP);
            mk = clampf(vc / 20, 0, 1); mcd = LANCE_HIT_CD;
            if (vc >= LANCE_VC_MIN) snprintf(buf, sizeof buf, "JOUTE v%.0f %d dmg", vc, (int)jd);
            else snprintf(buf, sizeof buf, "v%.0f/%.0f estoc %d", fmaxf(vc, 0), LANCE_VC_MIN, (int)(lance_thrust_dmg(sp3) * pvp));
        } else if (P.meleeKind == MK_FLAIL) {                 /* --- Fleau : degats et portee de l'arc, crochets si une cible y est --- */
            int inArc = 0;
            V3 eye = eye_pos(), c;
            mcd = FLAIL_CD;
            snprintf(buf, sizeof buf, "%d dmg %.1fm", (int)(flail_dmg(sp3) * pvp), flail_range(sp3));
            for (int k = 0; k < MAXE + MAXP && !inArc && !P.dead; k++)   /* meme test que le coup, sans ligne de vue (cout) */
                if (flail_target(k, &c))
                    inArc = flail_in_arc(eye, c, k < MAXE ? enemy_radius(E[k].type) : 0.5f,
                                         rel_speed(k < MAXE ? E[k].vel : np_vel(k - MAXE)), 0);
            if (inArc) {
                float al = P.meleeCd <= 0 ? 0.9f : 0.35f;
                glLineWidth(2);
                glColor4f(0.75f, 0.8f, 0.9f, al);
                glBegin(GL_LINES);
                for (int s = -1; s <= 1; s += 2) {            /* [ ] autour du viseur */
                    glVertex2f(cx + s * 34, cy - 10); glVertex2f(cx + s * 34, cy + 10);
                    glVertex2f(cx + s * 34, cy - 10); glVertex2f(cx + s * 28, cy - 10);
                    glVertex2f(cx + s * 34, cy + 10); glVertex2f(cx + s * 28, cy + 10);
                }
                glEnd();
            }                                                 /* --- fin Fleau --- */
        } else if (P.meleeKind == MK_FIST) {                  /* --- Poings : coup suivant, pastilles du combo, aide au punch-jump --- */
            int nxt = P.fistStep < 3 ? P.fistStep : 0;
            float fd = fist_dmg(sp3, nxt == 2) * pvp;
            if (dmMode) fd = fminf(fd, PVP_HIT_CAP);
            mcd = FIST_END_CD;
            snprintf(buf, sizeof buf, "%s%d dmg %.1fm", nxt == 2 ? "UPPERCUT " : "", (int)fd, fist_range(sp3));
            for (int k = 0; k < 3; k++) {                     /* coups du combo deja donnes, le 3e en or */
                int on = k < P.fistStep;
                if (k == 2) rect(mx + k * 16.0f, by - 26.0f, 12, 10, on ? 1.0f : 0.25f, on ? 0.8f : 0.22f, on ? 0.25f : 0.1f, 0.95f);
                else rect(mx + k * 16.0f, by - 26.0f, 12, 10, on ? 0.45f : 0.12f, on ? 0.9f : 0.2f, on ? 1.0f : 0.25f, 0.95f);
            }
            if (P.fistPJ && !P.dead) {                        /* le decor est a portee de poing : anneau cyan autour du viseur */
                V3 n;
                float t = ray_world(eye_pos(), view_dir(), FIST_PJ_RANGE, &n);
                int floorNo = n.y > 0.7f && (P.grounded || P.fistAirT < FIST_PJ_FLOOR_DELAY);
                if (t < FIST_PJ_RANGE && !floorNo) {
                    glLineWidth(2);
                    glColor4f(0.4f, 0.9f, 1, P.wallJumpCd <= 0 && P.meleeCd <= 0 ? 0.9f : 0.35f);
                    glBegin(GL_LINE_LOOP);
                    for (int k = 0; k < 24; k++) glVertex2f(cx + 30 * cosf(k * (2 * PI / 24)), cy + 30 * sinf(k * (2 * PI / 24)));
                    glEnd();
                }
            }                                                 /* --- fin Poings --- */
        } else if (P.meleeKind == MK_SHIELD) {                /* --- Egide : etat, apercu du coup de bouclier, jauge en 4 segments --- */
            mcd = SHIELD_BASH_CD;
            if (P.shieldState == SHS_BROKEN) snprintf(buf, sizeof buf, "BRISE %.1f s", fmaxf(P.shieldBrokenT, 0));
            else if (P.shieldRipT > 0) snprintf(buf, sizeof buf, "ARRACHE");
            else if (P.shieldState == SHS_UP) snprintf(buf, sizeof buf, "LEVE %d", (int)P.shieldGauge);
            else snprintf(buf, sizeof buf, "%d dmg %.1fm", (int)((SHIELD_BASH_DMG + SHIELD_BASH_PER_MS * sp3) * pvp), SHIELD_BASH_RANGE);
            V3 sc = P.shieldState == SHS_BROKEN ? v3(0.5f, 0.2f, 0.2f) : shield_col(P.shieldGauge);
            for (int s = 0; s < 4; s++) {
                float fill = clampf((P.shieldGauge - s * 30) / 30, 0, 1);
                rect(mx + s * 46.0f, by - 100.0f, 42, 7, 0.1f, 0.1f, 0.15f, 0.7f);
                if (fill > 0) rect(mx + s * 46.0f, by - 100.0f, 42 * fill, 7, sc.x, sc.y, sc.z, P.shieldState == SHS_UP ? 1.0f : 0.75f);
            }
            if (P.parryWin > 0 && P.shieldState == SHS_UP) text_c(0, cx, cy + 40, "PARFAIT", 1, 1, 0.85f, 0.9f);
        } else snprintf(buf, sizeof buf, "%d dmg %.1fm", (int)(melee_dmg(sp3) * pvp), melee_range(sp3));
        text(1, mx, by - 48.0f, buf, 1, 1 - mk * 0.45f, 1 - mk * 0.85f, 1);
        rect(mx, by - 36.0f, 180, 6, 0.1f, 0.1f, 0.15f, 0.7f);
        rect(mx, by - 36.0f, 180 * clampf(1 - P.meleeCd / mcd, 0, 1), 6, 1, 0.55f + 0.45f * (1 - mk), 0.3f, 0.95f);

        /* axe tir : chaine de touches successives */
        float sx = compact ? winW - 214.0f : cx + 130;
        hud_panel(sx - 12, by - 104.0f, STREAK_MAX * 23.0f + 20, 78);
        text(0, sx, by - 78.0f, "CHAINE DE TIR", 0.75f, 0.75f, 0.85f, 0.9f);
        float sk = clampf(shotStreak / (float)STREAK_MAX, 0, 1);
        snprintf(buf, sizeof buf, "x%.2f", streak_mult());
        text(1, sx, by - 48.0f, buf, 1, 1 - sk * 0.2f, 1 - sk * 0.8f, 1);
        for (int i = 0; i < STREAK_MAX; i++)
            rect(sx + i * 23.0f, by - 36.0f, 19, 6, i < shotStreak ? 1.0f : 0.1f, i < shotStreak ? 0.8f : 0.1f,
                 i < shotStreak ? 0.3f : 0.15f, i < shotStreak ? 0.95f : 0.7f);
        if (streakLostT > 0) text(0, sx, by - 100.0f, "CHAINE PERDUE", 1, 0.3f, 0.3f, clampf(streakLostT, 0, 1));
        if (shotStreak >= STREAK_MAX && !P.dead && (P.loadout[0] == WP_RAIL || P.loadout[1] == WP_RAIL || P.loadout[0] == WP_KINETIC ||
            P.loadout[1] == WP_KINETIC)) {                /* chaine pleine : le prochain rail ou Elan qui touche tue */
            float pulse = 0.75f + 0.25f * sinf((float)nowT * 12);
            for (int i = 0; i < STREAK_MAX; i++) rect(sx + i * 23.0f, by - 37.0f, 19, 8, 1, 0.85f, 0.3f, 0.35f * pulse);
            text(0, sx, by - 100.0f, "ONE-SHOT PRET", 1, 0.85f * pulse, 0.3f, 0.95f);
        }

        if (P.anvilState == ANVIL_DIVE) text_c(0, cx, winH - 110.0f, "PIQUE !", 1, 0.6f, 0.25f, 0.95f);
        else if (P.anvilState == ANVIL_STUN) {                /* Enclume : onde ratee, barre d'immobilisation */
            text_c(0, cx, winH - 110.0f, "IMMOBILISE", 1, 0.35f, 0.3f, 0.95f);
            rect(cx - 60, winH - 102.0f, 120, 5, 0.1f, 0.1f, 0.15f, 0.7f);
            rect(cx - 60, winH - 102.0f, 120 * clampf(P.anvilStunT / fmaxf(slamStunFull, 0.01f), 0, 1), 5, 1, 0.35f, 0.3f, 0.95f);
        } else if (P.hook == HOOK_ATTACHED)
            text_c(0, cx, winH - 110.0f, (P.hookEnemy || P.hookPlayer) ? "GRAPPIN -> CIBLE" : "GRAPPIN", 1,
                   (P.hookEnemy || P.hookPlayer) ? 0.35f : 0.8f, 0.35f, 0.9f);
        else if (P.wallrun) text_c(0, cx, winH - 110.0f, "WALL-RUN", 1, 0.6f, 0.9f, 0.9f);
        else if (P.sliding) text_c(0, cx, winH - 110.0f, "SLIDE", 1, 0.8f, 0.4f, 0.9f);

        /* vie : barre, trainee des degats qui se vide, eclat au soin, graduations tous les 25 PV */
        hp_ghost_tick(P.hp, nowT);
        float hpk = clampf(P.hp / 100.0f, 0, 1), ghk = clampf(hpGhost / 100.0f, 0, 1), hx = 30, hy = winH - 50.0f;
        rect(hx - 3, hy - 3, 306, 26, 0.02f, 0.02f, 0.05f, 0.55f);
        rect(hx, hy, 300, 20, 0.1f, 0.1f, 0.15f, 0.7f);
        if (ghk > hpk) {
            float fresh = clampf(1 - (float)((nowT - hpHitAt) / HP_GHOST_HOLD), 0, 1);   /* blanc juste apres le coup, puis rouge */
            rect(hx + 300 * hpk, hy, 300 * (ghk - hpk), 20, 1, 0.3f + 0.7f * fresh, 0.25f + 0.75f * fresh, 0.85f);
        }
        rect(hx, hy, 300 * hpk, 20, 1 - hpk * 0.7f, 0.25f + hpk * 0.6f, 0.35f, 0.95f);
        rect_vgrad(hx, hy, 300 * hpk, 9, 1, 1, 1, 0.28f, 0.0f);   /* reflet en haut de la barre */
        {
            float heal = clampf(1 - (float)(nowT - hpHealAt) / 0.5f, 0, 1);
            if (heal > 0) rect(hx - 3, hy - 3, 306, 26, 0.35f, 1, 0.5f, 0.35f * heal);
        }
        for (int i = 1; i < 4; i++) rect(hx + 75.0f * i - 1, hy, 2, 20, 0.02f, 0.02f, 0.05f, 0.6f);
        snprintf(buf, sizeof buf, "%d", (int)ceilf(P.hp));
        text(1, 30, winH - 60.0f, buf, 1, 1, 1, 1);

        /* arme */
        char wnBuf[32];
        const AmmoDef *ad = &ammoDef[P.weapon];
        if (P.weapon == WP_TESLA) snprintf(wnBuf, sizeof wnBuf, "TESLA %d%%", P.ammo[WP_TESLA]);
        else if (P.weapon == WP_SAW) saw_hud_name(wnBuf, (int)sizeof wnBuf);
        else snprintf(wnBuf, sizeof wnBuf, "%s %d/%d%s", ad->name, P.ammo[P.weapon], ad->max,
                      P.weapon == WP_SHOTGUN && !P.ammo[WP_SHOTGUN] ? " - MELEE !" : "");   /* SHOTGUN a sec */
        const char *wn = wnBuf;
        int dryW = !wp_has_ammo(P.weapon);
        if (P.weapon == WP_SAW) dryW = saw_mine() && saw_mine()->sawState == SAW_PLANTED;   /* elle ne reviendra pas seule */
        {                                                     /* fond du panneau d'arme (pieces, nom, emplacements) */
            float pw = fmaxf(fontW[1] * (float)strlen(wn), fontW[0] * 40.0f) + 24;
            hud_panel(winW - 18.0f - pw, winH - 122.0f, pw, 116);
        }
        text(1, winW - 30.0f - fontW[1] * (float)strlen(wn), winH - 60.0f, wn, dryW ? 1.0f : ad->r, dryW ? 0.3f : ad->g,
             dryW ? 0.3f : ad->b, 1);
        rect(winW - 230.0f, winH - 40.0f, 200, 8, 0.1f, 0.1f, 0.15f, 0.7f);
        {
            float cdMax = P.weapon == WP_SHOTGUN && P.sgPumpMax > 0 ? P.sgPumpMax : wpCd[P.weapon];   /* SHOTGUN : recharge reelle */
            float cdLeft = P.weapon == WP_SAW ? P.sawCd / SAW_CD : P.fireCd / cdMax;                    /* Scie : delai de lancer */
            if (P.weapon == WP_TESLA) tesla_hud_bar(winW - 230.0f, winH - 40.0f);                      /* Tesla : batterie */
            else rect(winW - 230.0f, winH - 40.0f, 200 * clampf(1 - cdLeft, 0, 1), 8, 1, 1, 1, 0.8f);
        }
        if (ad->gauge > 0) {                                  /* jauge : la melee et les cellules la remplissent */
            float gf = P.weapon == WP_TESLA ? P.ammo[WP_TESLA] / 100.0f : gauge_frac(P.weapon);
            int gain = ammoGainT > 0 && (ammoGainSlot == 2 || ammoGainSlot == cur_slot());
            rect(winW - 230.0f, winH - 28.0f, 200, 5, 0.1f, 0.1f, 0.15f, 0.7f);
            rect(winW - 230.0f, winH - 28.0f, 200 * gf, 5, gain ? 1 : ad->r, gain ? 1 : ad->g, gain ? 1 : ad->b,
                 wp_full(P.weapon) ? 0.45f : 0.95f);
            if (gain) {
                snprintf(buf, sizeof buf, "+%d", ammoGainPts);
                text(0, winW - 236.0f - fontW[0] * (float)strlen(buf), winH - 20.0f, buf, 0.4f, 1, 0.4f, clampf(ammoGainT / 0.3f, 0, 1));
            }
        }
        if (P.weapon == WP_KINETIC && !P.dead) {              /* Elan : apercu des degats et fenetre du dash-tir */
            float e, kd = kin_preview(&e), ky = compact ? winH - 250.0f : winH - 172.0f;
            int win = kin_in_window(), late = !win && P.kinDashT < KIN_DASH_FORGET;
            V3 c = kin_col(e);
            if (win && fmodf((float)nowT, 0.06f) < 0.03f) c = v3(1, 1, 1);   /* fenetre ouverte : le chiffre clignote en blanc */
            else if (late) c = v3(0.5f, 0.5f, 0.55f);         /* malus : la vitesse du dash ne compte pas */
            snprintf(buf, sizeof buf, "%.0f dmg", kd);
            text(1, winW - 30.0f - fontW[1] * (float)strlen(buf), ky, buf, c.x, c.y, c.z, 1);
            if (e >= 1) text(0, winW - 230.0f, ky, "PLEIN", 1, 0.3f, 0.9f, 1);
            rect(winW - 230.0f, ky + 12, 200, 6, 0.1f, 0.1f, 0.15f, 0.7f);
            rect(winW - 230.0f, ky + 12, 200 * clampf(e, 0, 1), 6, c.x, c.y, c.z, 0.95f);
            if (dmMode) {                                     /* repere "2 touches" : 50 PvP sans chaine (10,3 m/s) */
                float e2 = ((50 / PVP_SCALE - KIN_DMG_BASE) / KIN_DMG_PER_MS) / KIN_SPEED_CAP;
                rect(winW - 230.0f + 200 * e2 - 1, ky + 9, 2, 12, 1, 1, 1, 0.9f);
            }
            if (win) {                                        /* anneau magenta qui se resserre de 28 a 10 px en 12 ticks */
                float r = 10 + 18 * (1 - clampf(P.kinDashT / KIN_DASH_WINDOW, 0, 1));
                glLineWidth(2);
                glColor4f(1, 0.3f, 0.9f, 0.95f);
                glBegin(GL_LINE_LOOP);
                for (int s = 0; s < 32; s++) glVertex2f(cx + r * cosf(s * PI / 16), cy + r * sinf(s * PI / 16));
                glEnd();
            } else if (late) {                                /* barre grise sous le viseur : le tir plein revient a 0,6 s */
                float k2 = 1 - clampf((P.kinDashT - KIN_DASH_WINDOW) / (KIN_DASH_FORGET - KIN_DASH_WINDOW), 0, 1);
                rect(cx - 30, cy + 20, 60 * k2, 3, 0.6f, 0.6f, 0.65f, 0.85f);
            }
        }
        if (P.weapon == WP_SHOTGUN && !P.dead) {             /* SHOTGUN : cercle de dispersion (moitie en slide), plombs touches */
            float half = SG_HALF_ANGLE * (P.sliding ? SG_SLIDE_SPREAD : 1);
            float r = tanf(half) / tanf(fminf(P.fovCur, 150.0f) * PI / 360.0f) * winW * 0.5f;
            glLineWidth(P.sliding ? 2.0f : 1.5f);
            glColor4f(1, 0.65f, 0.3f, P.fireCd <= 0 ? 0.75f : 0.3f);
            glBegin(GL_LINE_LOOP);
            for (int s = 0; s < 24; s++) glVertex2f(cx + r * cosf(s * PI / 12), cy + r * sinf(s * PI / 12));
            glEnd();
            if (P.sgLastT > 0) {
                snprintf(buf, sizeof buf, "%d/%d", P.sgLastPellets, SG_PELLETS);
                text_c(0, cx, cy + r + 10, buf, 1, 0.75f, 0.35f, clampf(P.sgLastT / 0.2f, 0, 1));
            }
        }
        if (P.lanceState == LS_COUCH && !P.dead) {            /* Lance couchee : anneau du cone (plein si une cible y est armee), duree */
            float r = tanf(acosf(LANCE_CONE)) / tanf(fminf(P.fovCur, 150.0f) * PI / 360.0f) * winW * 0.5f;
            int armed = P.lanceVc >= LANCE_VC_MIN;
            V3 tc = lance_tip_col(P.lanceVc);
            if (armed) {
                glColor4f(tc.x, tc.y, tc.z, 0.12f);
                glBegin(GL_TRIANGLE_FAN);
                glVertex2f(cx, cy);
                for (int s = 0; s <= 40; s++) glVertex2f(cx + r * cosf(s * PI / 20), cy + r * sinf(s * PI / 20));
                glEnd();
            }
            glLineWidth(armed ? 3.0f : 1.5f);
            glColor4f(tc.x, tc.y, tc.z, armed ? 0.9f : 0.5f);
            glBegin(GL_LINE_LOOP);
            for (int s = 0; s < 40; s++) glVertex2f(cx + r * cosf(s * PI / 20), cy + r * sinf(s * PI / 20));
            glEnd();
            rect(cx - 40, cy + 26, 80, 4, 0.1f, 0.1f, 0.15f, 0.7f);
            rect(cx - 40, cy + 26, 80 * clampf(1 - P.lanceT / LANCE_HOLD_MAX, 0, 1), 4, 1, 0.6f, 0.2f, 0.9f);
            snprintf(buf, sizeof buf, "v%.0f", P.lanceVc);
            text_c(0, cx, cy + 50, buf, tc.x, tc.y, tc.z, 0.9f);
        }
        draw_saw_hud(cx, cy);                                 /* Scie */
        if (P.weapon == WP_TESLA && !P.dead) draw_tesla_hud(cx, cy);   /* Tesla */
        draw_mine_hud(cx, cy);                                /* Ventouse */
        {                                                     /* Competences : charges, nom, etat */
            const SkillDef *sd = &skillDef[P.skill];
            char kb[64];
            int cmax = cp_max(P.skill, sd->max);              /* VAGUES : charges de la boutique */
            for (int i = 0; i < cmax; i++) {
                float fill = clampf(P.skillCh - i, 0, 1), x = winW - 30.0f - (cmax - i) * 20.0f, y = winH - 96.0f;
                rect(x, y, 14, 14, sd->r * 0.15f, sd->g * 0.15f, sd->b * 0.15f, 0.7f);
                rect(x, y + 14 * (1 - fill), 14, 14 * fill, sd->r, sd->g, sd->b, fill >= 1 ? 1.0f : 0.5f);
            }
            snprintf(kb, sizeof kb, "%s [%s]", sd->name, bind_text(A_SKILL));
            text(0, winW - 36.0f - cmax * 20.0f - fontW[0] * (float)strlen(kb), winH - 83.0f, kb, sd->r * 0.8f, sd->g * 0.8f, sd->b * 0.8f, 0.9f);
            char aimBuf[24], ricBuf[24];
            snprintf(aimBuf, sizeof aimBuf, "AIMANT %.1f s", P.aimT);
            snprintf(ricBuf, sizeof ricBuf, "RICOCHET %.1f s", P.ricT);
            const char *st = P.ricArmed ? ricBuf : P.chargeT > 0 ? "CHARGE PRETE x1,5" : P.aimT > 0 ? aimBuf : NULL;
            if (st) text_c(0, cx, cy + 74, st, sd->r, sd->g, sd->b, 0.6f + 0.4f * sinf(gameTime * 12));
            if (P.chargeHold > 0) {                           /* CHARGE : jauge de maintien */
                rect(cx - 40, cy + 62, 80, 4, 0.1f, 0.1f, 0.15f, 0.7f);
                rect(cx - 40, cy + 62, 80 * clampf(P.chargeHold / CHARGE_HOLD, 0, 1), 4, sd->r, sd->g, sd->b, 0.9f);
            }
            if (markedMeT > 0) text_c(1, cx, 120, "MARQUE : TU ES VISIBLE", 0.85f, 0.35f, 1, 0.5f + 0.5f * sinf(gameTime * 10));
            for (int i = 0; i < MAXP + MAXE; i++) {          /* MARQUE : losange au-dessus des cibles, a travers les murs */
                float sx, sy, mt = i < MAXP ? markP[i] : markE[i - MAXP];
                if (mt <= 0) continue;
                if (i < MAXP && !np_target(i)) continue;
                V3 at = i < MAXP ? vadd(np_center(i), v3(0, 1.4f, 0)) : vadd(E[i - MAXP].pos, v3(0, 1.2f, 0));
                if (!project(at, &sx, &sy)) continue;
                float r = 10 + 2 * sinf(gameTime * 8);
                if (i < MAXP) sy -= 26;                       /* au-dessus du nom du joueur */
                for (int k = 0; k < 2; k++) {                 /* losange violet, coeur blanc */
                    float q = k ? r * 0.45f : r;
                    if (k) glColor4f(1, 1, 1, 0.95f); else glColor4f(0.85f, 0.35f, 1, 0.9f);
                    glBegin(GL_QUADS);
                    glVertex2f(sx, sy - q); glVertex2f(sx + q, sy); glVertex2f(sx, sy + q); glVertex2f(sx - q, sy);
                    glEnd();
                }
            }
        }
        {                                                     /* le viseur s'allume sur une piece : doree = la mienne, rouge = adverse */
            V3 eye = eye_pos(), vd = view_dir();
            float tw = ray_world(eye, vd, 400, 0), t;
            for (int i = 0; i < MAXCOIN; i++)
                if (!P.dead && CO[i].alive && !(CO[i].decoy && CO[i].owner != myId) && !decoy_skip(i, eye) &&
                    ray_sphere(eye, vd, CO[i].pos, COIN_RADIUS, tw, &t)) {   /* MIRAGE adverse : le viseur ne le trahit pas */
                    glLineWidth(2);
                    if (CO[i].decoy) glColor4f(0.4f, 0.85f, 1, 1);    /* mon leurre : cyan */
                    else if (CO[i].owner == myId) glColor4f(1, 0.85f, 0.3f, 1); else glColor4f(1, 0.25f, 0.25f, 1);
                    glBegin(GL_LINE_LOOP);
                    glVertex2f(cx, cy - 11); glVertex2f(cx + 11, cy); glVertex2f(cx, cy + 11); glVertex2f(cx - 11, cy);
                    glEnd();
                    break;
                }
        }
        float wpFl = P.dead ? 0 : clampf(1 - (float)((nowT - wpFlashAt) / WP_FLASH_T), 0, 1);   /* changement d'arme recent */
        if (wpFl > 0) {                                       /* nom de l'arme sous le viseur : monte et s'efface */
            const AmmoDef *wd = &ammoDef[P.weapon];
            float a = fminf(1, wpFl * 2.5f), y = cy + 78 + 14 * wpFl * wpFl, nw = fontW[1] * (float)strlen(wd->name);
            rect(cx - nw * 0.5f - 14, y - 4, nw + 28, (float)fontH[1] + 8, 0.03f, 0.03f, 0.08f, 0.45f * a);
            rect(cx - nw * 0.5f - 14, y + fontH[1] + 4.0f, (nw + 28) * wpFl, 3, wd->r, wd->g, wd->b, 0.9f * a);
            text_c(1, cx, y, wd->name, wd->r * 0.6f + 0.4f, wd->g * 0.6f + 0.4f, wd->b * 0.6f + 0.4f, a);
        }
        {                                                     /* emplacements : [touche] ARME munitions, l'actif en blanc */
            char sb[2][48];
            for (int s = 0; s < 2; s++)
                snprintf(sb[s], sizeof sb[s], "[%s] %s %d", bind_text(s ? A_WEAP2 : A_WEAP1), ammoDef[P.loadout[s]].name,
                         P.ammo[P.loadout[s]]);
            float x = winW - 30.0f - fontW[0] * (float)(strlen(sb[0]) + strlen(sb[1]) + 3);
            int nslot = P.loadout[1] == P.loadout[0] ? 1 : 2;   /* une seule arme (boutique) : un seul emplacement */
            if (nslot == 1) x = winW - 30.0f - fontW[0] * (float)strlen(sb[0]);
            for (int s = 0; s < nslot; s++) {
                int w = P.loadout[s], act0 = w == P.weapon, emp = !wp_has_ammo(w);
                float k = act0 ? 1.0f : 0.55f;
                text(0, x, winH - 13.0f, sb[s], emp ? 1.0f : k, emp ? 0.35f : k, emp ? 0.35f : k * 1.05f, 0.9f);
                float gw = fontW[0] * (float)strlen(sb[s]), gf = w == WP_TESLA ? P.ammo[w] / 100.0f : gauge_frac(w);
                if (act0 && wpFl > 0)                         /* emplacement choisi : eclat a la couleur de l'arme */
                    rect(x - 4, winH - 30.0f, gw + 8, 24, ammoDef[w].r, ammoDef[w].g, ammoDef[w].b, 0.25f * wpFl);
                if (ammoDef[w].gauge > 0) {
                    rect(x, winH - 5.0f, gw, 3, 0.1f, 0.1f, 0.15f, 0.7f);
                    rect(x, winH - 5.0f, gw * gf, 3, ammoDef[w].r, ammoDef[w].g, ammoDef[w].b, 0.9f);
                }
                x += gw + fontW[0] * 3;
            }
            if (!eco_on() && !vagOn && (wantLoadout[0] != P.loadout[0] || wantLoadout[1] != P.loadout[1] || wantMelee != P.meleeKind || wantSkill != P.skill))
                text(0, winW - 30.0f - fontW[0] * 32, winH - 120.0f, "nouveau loadout au prochain respawn", 0.6f, 0.6f, 0.65f, 0.8f);
        }
    }

    /* infos de partie */
    if (!raceOn && !vagOn) hud_panel(16, 14, fontW[0] * (dmMode ? 40.0f : 26.0f) + 28, dmMode && teamSize > 0 ? 64.0f : 100.0f);
    if (dmMode) {
        int count = 0, rank = 1;
        for (int i = 0; i < MAXP; i++) {
            if (!NP[i].active) continue;
            count++;
            if (i != myId && NP[i].kills > NP[myId].kills) rank++;
        }
        int left = (int)fmaxf(0, MATCH_TIME - matchT);
        if (teamSize > 0) {                               /* equipes : score des deux camps en haut au centre */
            int me = NP[myId].team & 1;
            snprintf(buf, sizeof buf, "EQUIPES %dV%d   %d:%02d   tu es %s", teamSize, teamSize, left / 60, left % 60,
                     me ? "ROUGE" : "BLEU");
            text(0, 30, 40, buf, 0.85f, 0.85f, 0.95f, 1);
            snprintf(buf, sizeof buf, "FRAGS %d   morts %d   [%s] scores", NP[myId].kills, NP[myId].deaths, bind_text(A_SCORES));
            text(0, 30, 66, buf, 0.7f, 0.7f, 0.8f, 0.9f);
            float bw = 150, bh = 46, by = 18;
            for (int t = 0; t < 2; t++) {
                const float *tc = teamCol[t];
                float bx = t == 0 ? cx - bw - 6 : cx + 6, f = clampf(team_score(t) / (float)(fragLimit > 0 ? fragLimit : 1), 0, 1);
                rect(bx, by, bw, bh, 0.03f, 0.04f, 0.08f, 0.7f);
                rect(t == 0 ? bx + bw * (1 - f) : bx, by + bh - 5, bw * f, 5, tc[0], tc[1], tc[2], 0.95f);
                if (t == me) rect(bx, by, bw, 2, 1, 1, 1, 0.7f);
                snprintf(buf, sizeof buf, "%d", team_score(t));
                text_c(1, bx + bw * 0.5f, by + 33, buf, tc[0], tc[1], tc[2], 1);
            }
            snprintf(buf, sizeof buf, "%d", fragLimit);
            text_c(0, cx, by + bh + 20, buf, 0.6f, 0.6f, 0.7f, 0.9f);
            draw_radar(30, 84, winH < 800 ? 120.0f : 160.0f);
        } else {
            snprintf(buf, sizeof buf, "%s   %d joueur%s   %d:%02d", ranked ? "PARTIE CLASSEE" : "DEATHMATCH LAN", count,
                     count > 1 ? "s" : "", left / 60, left % 60);
            text(0, 30, 40, buf, 0.85f, 0.85f, 0.95f, 1);
            snprintf(buf, sizeof buf, "FRAGS %d / %d", NP[myId].kills, fragLimit);
            text(1, 30, 78, buf, 1, 1, 1, 1);
            snprintf(buf, sizeof buf, "rang %d / %d   morts %d   [%s] scores", rank, count, NP[myId].deaths, bind_text(A_SCORES));
            text(0, 30, 104, buf, 0.7f, 0.7f, 0.8f, 0.9f);
        }
        draw_killfeed();
    } else if (vagOn) {
        vg_draw_info();                                   /* VAGUES : vague, score, credits, temps */
    } else if (!raceOn) {                                 /* (course : son propre chrono, course_hud.c) */
        int remaining = enemies_alive() + toSpawn;
        snprintf(buf, sizeof buf, "VAGUE %d   ennemis %d", wave, remaining);
        text(0, 30, 40, buf, 0.85f, 0.85f, 0.95f, 1);
        snprintf(buf, sizeof buf, "SCORE %d", score);
        text(1, 30, 78, buf, 1, 1, 1, 1);
        snprintf(buf, sizeof buf, "kills %d   temps %d:%02d", kills, (int)gameTime / 60, (int)gameTime % 60);
        text(0, 30, 104, buf, 0.7f, 0.7f, 0.8f, 0.9f);
    }
    if (combo > 1) {
        snprintf(buf, sizeof buf, "COMBO x%d", combo);
        text(1, 30, 140, buf, 1, 0.8f, 0.2f, clampf(comboT, 0, 1));
    }
    if (showFps) snprintf(buf, sizeof buf, "%.0f fps", fps); else buf[0] = 0;
    text(0, winW - 110.0f, 30, buf, 0.5f, 0.5f, 0.6f, 0.8f);

    if (popT > 0) text_c(1, cx, cy - 40 - (1.2f - popT) * 30, popText, 1, 0.85f, 0.3f, clampf(popT, 0, 1));
    draw_style_hud();                                     /* jauge de style */
    draw_race_hud();                                      /* course : chrono, decompte, arrivee */
    draw_modes_hud();                                     /* modes : objectifs, temps, pieces */
    draw_shop();                                          /* boutique (mort) et arme de depart */
    vg_draw_hud(cx, cy);                                  /* VAGUES : boss, entre-deux, boutique, selecteur, vague 100 */
    if (msgT > 0 && !vg_menu_open()) text_c(2, cx, cy - 120, msg, 1, 1, 1, clampf(msgT, 0, 1));   /* VAGUES : "VAGUE N TERMINEE" ne passe pas par-dessus la boutique */

    if (dmMode && matchOver) {
        winner_text(buf, sizeof buf);
        const float *wc = teamSize > 0 && matchWinner < TEAM_DRAW ? teamCol[matchWinner & 1] : NULL;
        text_c(2, cx, cy - 250, buf, wc ? wc[0] : 1, wc ? wc[1] : 0.85f, wc ? wc[2] : 0.3f, 1);
        draw_scoreboard(cx, cy);
        if (ranked && !spectating) draw_rank_result(cx, cy);
        snprintf(buf, sizeof buf, "nouveau match dans %d s", (int)ceilf(fmaxf(matchEndT, 0)));
        text_c(1, cx, winH - 150.0f, buf, 0.9f, 0.9f, 0.9f, 1);
    } else if (P.dead && killcam_active()) {              /* camera de mort : qui, avec quoi, combien de PV il lui reste */
        draw_killcam_hud(cx, cy);
    } else if (dmMode && P.dead) {
        rect(0, 0, (float)winW, (float)winH, 0.15f, 0, 0, 0.4f);
        if (killerOfMe >= 0 && killerOfMe != myId) snprintf(buf, sizeof buf, "ELIMINE PAR %s", pname(killerOfMe));
        else snprintf(buf, sizeof buf, fellOut ? "TOMBE DANS LE VIDE" : killerOfMe == myId ? "TU T'ES FAIT SAUTER" : "EN ATTENTE DU SPAWN");
        float ty = eco_on() ? cy - 190 : cy - 40;          /* boutique : le titre au-dessus du panneau */
        text_c(2, cx, ty, buf, 1, 0.35f, 0.3f, 1);
        snprintf(buf, sizeof buf, "respawn dans %.1f s", fmaxf(0, RESPAWN_TIME - P.deathT));
        if (killerOfMe >= 0) text_c(1, cx, ty + 50, buf, 0.9f, 0.9f, 0.9f, 1);
    } else if (dmMode && bound_held(A_SCORES)) {
        draw_scoreboard(cx, cy);
    } else if (P.dead && vagOn) {
        vg_draw_death(cx, cy);
    } else if (P.dead) {
        rect(0, 0, (float)winW, (float)winH, 0.15f, 0, 0, 0.45f);
        text_c(2, cx, cy - 40, "T'ES MORT", 1, 0.3f, 0.3f, 1);
        snprintf(buf, sizeof buf, "score %d  -  vague %d  -  %d kills", score, wave, kills);
        text_c(1, cx, cy + 10, buf, 1, 1, 1, 1);
        snprintf(buf, sizeof buf, "record %d", bestScore);
        text_c(0, cx, cy + 44, buf, 0.8f, 0.8f, 0.9f, 1);
        if (P.deathT > 1.0f) text_c(1, cx, cy + 100, "[R] ou clic pour recommencer", 0.9f, 0.9f, 0.9f, 1);
    }

    if (!captured && !shotMode && !botMode && !ecoShopping && !vg_menu_open()) {
        if (settingsOpen) draw_settings(cx, cy); else if (loadoutOpen) draw_loadout(cx, cy); else draw_pause(cx, cy);
    }
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

/* point de l'ovale de l'onde (centre c, cap dir) a l'angle ang, a la fraction k de sa taille */
static V3 slam_ell_pt(V3 c, V3 dir, float R, float vh, float ang, float k) {
    float ca = cosf(ang), sa = sinf(ang), ra = R * (ca >= 0 ? slam_fwd(vh) : slam_back(vh)) * k, rb = R * k;
    return v3(c.x + dir.x * ca * ra - dir.z * sa * rb, c.y, c.z + dir.z * ca * ra + dir.x * sa * rb);
}
/* Enclume : ovale de l'onde prevue au sol (passe additive), avec la hauteur du mur. Orange si une cible est dedans. */
static void draw_slam_marker(void) {
    if (appState != APP_GAME || spectating || P.dead || P.meleeKind != MK_ANVIL || P.grounded) return;
    int diving = P.anvilState == ANVIL_DIVE;
    if (!diving && (P.anvilState != ANVIL_IDLE || anvil_ground_dist(P.pos) < SLAM_MIN_H)) return;
    V3 at; float vy, R;
    if (!slam_predict(&at, &vy, &R, NULL)) return;
    float H = (diving ? P.anvilStartY : P.pos.y) - at.y, hw = slam_hw(H), vh = hlen(P.vel);
    V3 dir = vh > 1 ? v3(P.vel.x / vh, 0, P.vel.z / vh) : v3(sinf(P.yaw), 0, -cosf(P.yaw));
    if (vh <= 1) vh = 0;
    int hot = 0;
    for (int i = 0; i < MAXE && !hot; i++) {
        if (!E[i].alive) continue;
        float er = enemy_radius(E[i].type), feet = E[i].pos.y - er - at.y;
        hot = feet <= hw && feet >= -SLAM_REACH_DOWN - 2 * er && slam_ell(dir, R, vh, E[i].pos.x - at.x, E[i].pos.z - at.z, er) < 1;
    }
    for (int i = 0; i < MAXP && !hot; i++) {
        if (!np_target(i)) continue;
        float feet = NP[i].disp.y - at.y;
        hot = feet <= hw && feet >= -SLAM_REACH_DOWN && slam_ell(dir, R, vh, NP[i].disp.x - at.x, NP[i].disp.z - at.z, 0.5f) < 1;
    }
    float a = diving ? 0.9f : 0.35f, pulse = 0.75f + 0.25f * sinf(gameTime * 14);
    glLineWidth(diving ? 4.0f : 2.0f);
    if (hot) glColor4f(1, 0.5f, 0.15f, a * pulse); else glColor4f(0.55f, 0.75f, 1, a * pulse);
    for (int ring = 0; ring < 3; ring++) {                /* bord au sol, bord a la hauteur du mur, petit ovale central */
        float k = ring == 2 ? 0.25f : 1, y = at.y + (ring == 1 ? hw : 0.06f);
        if (ring == 1) glColor4f(hot ? 1 : 0.55f, hot ? 0.5f : 0.75f, hot ? 0.15f : 1, a * pulse * 0.35f);
        glBegin(GL_LINE_LOOP);
        for (int n = 0; n < 40; n++) {
            V3 p = slam_ell_pt(at, dir, R, vh, n * (2 * PI / 40), k);
            glVertex3f(p.x, y, p.z);
        }
        glEnd();
    }
}

/* Enclume : murs d'onde qui s'etendent (la mienne et celles des autres) : bande de quads de la hauteur du mur, opaque
 * en bas, transparente en haut ; puis il retombe */
static void draw_slam_waves(void) {
    for (int i = 0; i < MAXSV; i++) {
        SlamVis *s = &SV[i];
        if (!s->alive) continue;
        float k = s->t * SLAM_WAVE_SPEED / s->R, h = s->hw, al = 0.55f;
        if (k > 1) { h *= clampf(1 - (k - 1) / 0.4f, 0, 1); al *= clampf(1 - (k - 1) / 0.4f, 0, 1); k = 1; }
        if (h <= 0.01f) continue;
        glBegin(GL_QUAD_STRIP);
        for (int n = 0; n <= 48; n++) {
            V3 p = slam_ell_pt(s->c, s->dir, s->R, s->vh, n * (2 * PI / 48), k);
            glColor4f(1, 0.6f, 0.25f, al); glVertex3f(p.x, p.y + 0.05f, p.z);
            glColor4f(1, 0.75f, 0.4f, 0); glVertex3f(p.x, p.y + h, p.z);
        }
        glEnd();
    }
}
