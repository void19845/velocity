/* VELOCITY - boutons, menus, parametres, loadout, ecran SKIN
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static int button(float x, float y, float w, float h, const char *label) {
    int hover = uiMX >= x && uiMX < x + w && uiMY >= y && uiMY < y + h;
    rect(x, y, w, h, hover ? 0.2f : 0.1f, hover ? 0.42f : 0.12f, hover ? 0.62f : 0.2f, 0.9f);
    rect(x, y + h - 3, w, 3, 0.4f, 0.85f, 1.0f, hover ? 1.0f : 0.45f);
    text_c(1, x + w * 0.5f, y + h * 0.5f + fontH[1] * 0.3f, label, 1, 1, 1, 1);
    if (hover && uiClick) { uiClick = 0; play(S_HIT, 0.4f, 1.5f); return 1; }
    return 0;
}

/* le champ PSEUDO (id 1) est verrouille : il faut un double-clic (deux clics en moins de UI_DBLCLICK ms) pour
   le modifier, pour ne pas le changer par megarde ; les autres champs s'ouvrent d'un clic. 1 = ouvrir la saisie */
#define UI_DBLCLICK 450
static int fieldLastId;
static unsigned fieldLastMs;
static int field_click(int id, unsigned ms) {
    int dbl = fieldLastId == id && ms - fieldLastMs < UI_DBLCLICK;
    fieldLastId = id; fieldLastMs = ms;
    if (id != 1 || uiFocus == id || dbl) { fieldLastId = 0; return 1; }   /* un troisieme clic ne recompte pas */
    return 0;
}

static void field(float x, float y, float w, float h, const char *label, const char *val, int id) {
    int hover = uiMX >= x && uiMX < x + w && uiMY >= y && uiMY < y + h, focus = uiFocus == id;
    char buf[64];
    text(0, x, y - 8, label, 0.7f, 0.75f, 0.85f, 1);
    if (id == 1 && hover && !focus) {                     /* champ verrouille : l'astuce au survol */
        const char *tip = "double-clic pour modifier";
        text(0, x + w - fontW[0] * (float)strlen(tip), y - 8, tip, 0.55f, 0.6f, 0.7f, 1);
    }
    rect(x, y, w, h, 0.05f, 0.06f, 0.1f, 0.9f);
    rect(x, y + h - 2, w, 2, focus ? 1.0f : 0.4f, focus ? 0.8f : 0.4f, focus ? 0.35f : 0.5f, 1);
    snprintf(buf, sizeof buf, "%s%s", val, (focus && ((GetTickCount() / 400) & 1)) ? "_" : "");
    text(1, x + 10, y + h * 0.5f + fontH[1] * 0.3f, buf, 1, 1, 1, 1);
    if (hover && uiClick) { uiClick = 0; if (field_click(id, GetTickCount())) uiFocus = id; }
}

/* aide : les commandes actuelles (reaffectables dans PARAMETRES) et les astuces de jeu */
static void draw_controls(float x, float y) {
    static const char *tips[] = {
        "air-strafe : tourne la souris + gauche/droite en l'air",
        "epee : degats et portee selon la vitesse RELATIVE ; touche = +1 dash",
        "PARRY (debut du coup d'epee) : roquettes, orbes, kamikazes ET epees",
        "piece + tir dedans au rail = ricochet auto (x1,5) ; vole celles des autres",
        "F2/F3 sensibilite   F5 recharger graphismes   F11 plein ecran   ECHAP menu",
    };
    char buf[128];
    text(1, x, y, "COMMANDES", 0.5f, 0.9f, 1.0f, 1);
    float pitch = clampf(((float)winH - 24 - (y + 34)) / ((float)A_COUNT + 6.6f), 17.0f, 21.0f);   /* serre les lignes si la fenetre est basse */
    for (int a = 0; a < A_COUNT; a++) {
        text(0, x, y + 34 + a * pitch, actLabel[a], 0.75f, 0.78f, 0.88f, 1);
        text(0, x + 285, y + 34 + a * pitch, bind_text(a), 1, 0.9f, 0.55f, 1);
    }
    float ty = y + 44 + A_COUNT * pitch;
    for (int i = 0; i < (int)(sizeof tips / sizeof tips[0]); i++)
        text(0, x, ty + i * pitch, tips[i], 0.6f, 0.75f, 0.9f, 1);
    snprintf(buf, sizeof buf, "les commandes se changent dans PARAMETRES");
    text(0, x, ty + 5 * pitch + 6, buf, 0.5f, 0.55f, 0.65f, 1);
}

static float slider(int id, float x, float y, float w, const char *label, const char *valText, float v, float lo, float hi) {
    int hover = uiMX >= x - 6 && uiMX < x + w + 6 && uiMY >= y - 4 && uiMY < y + 26;
    text(0, x, y - 8, label, 0.75f, 0.78f, 0.88f, 1);
    text(0, x + w - fontW[0] * (float)strlen(valText), y - 8, valText, 1, 0.9f, 0.55f, 1);
    if (hover && uiClick) { uiClick = 0; uiDrag = id; }
    if (uiDrag == id && uiDown) v = lo + clampf((uiMX - x) / w, 0, 1) * (hi - lo);
    float k = (v - lo) / (hi - lo);
    rect(x, y + 8, w, 6, 0.12f, 0.14f, 0.22f, 0.9f);
    rect(x, y + 8, w * k, 6, 0.4f, 0.85f, 1.0f, 0.9f);
    rect(x + w * k - 6, y + 2, 12, 18, hover || uiDrag == id ? 1.0f : 0.8f, 1, 1, 1);
    return v;
}

/* PARAMETRES et GRAPHISMES : mise en page commune, calculee sur la fenetre. Compacte sous 760 px de haut :
 * tout tient dans 1024 x 640 (captures --shot settings_small / graphismes_small). Libelles complets partout. */
typedef struct { float ty, y0, x0, w, bh, step; int compact; } SetLay;
static SetLay settings_lay(float cx) {
    SetLay L;
    L.compact = winH < 760;
    L.w = fminf((float)winW - 48, 1320);
    L.x0 = cx - L.w * 0.5f;
    L.ty = L.compact ? 54.0f : 112.0f;                    /* ligne de base du titre */
    L.y0 = L.compact ? 98.0f : 178.0f;                    /* ligne de base des en-tetes de colonnes */
    L.bh = L.compact ? 36.0f : 44.0f;
    L.step = L.bh + (L.compact ? 6.0f : 8.0f);
    return L;
}
static const char *yes_no(int v) { return v ? "OUI" : "NON"; }

/* ecran GRAPHISMES (PARAMETRES > GRAPHISMES) : rendu a gauche, ecran a droite ; l'aide du reglage survole dessous */
static const char *gfxHelp;
static int gfx_button(float x, float y, float w, float h, const char *label, const char *help) {
    if (uiMX >= x && uiMX < x + w && uiMY >= y && uiMY < y + h) gfxHelp = help;
    return button(x, y, w, h, label);
}
static void draw_graphics_settings(float cx) {
    char buf[600];
    SetLay L = settings_lay(cx);
    gfxHelp = NULL;
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.82f);
    text_c(2, cx, L.ty, "GRAPHISMES", 0.5f, 0.9f, 1.0f, 1);
    float gap = L.compact ? 24.0f : 40.0f, cw = fminf((L.w - gap) * 0.5f, 520), x1 = cx - gap * 0.5f - cw, x2 = cx + gap * 0.5f;
    float y = L.y0 + (L.compact ? 16.0f : 22.0f);
    text(1, x1, L.y0, "RENDU", 0.5f, 0.9f, 1.0f, 1);
    text(1, x2, L.y0, "ECRAN", 0.5f, 0.9f, 1.0f, 1);

    snprintf(buf, sizeof buf, "QUALITE : %s", qualityNames[gfxQuality]);   /* preregle : refait l'eclairage du decor */
    if (gfx_button(x1, y, cw, L.bh, buf, "BASSE / MOYENNE : eclairage, particules et decor lointain alleges ; ULTRA : tout rendu.ini")) {
        gfxQuality = (gfxQuality + 1) % 3;
        gfxShadows = gfxQuality > 0; gfxBloom = gfxQuality == 2;
        gfx_reload(); popT = 0;
    }
    snprintf(buf, sizeof buf, "CIEL : %s", ambianceNames[gfxAmbiance]);
    if (gfx_button(x1, y + L.step, cw, L.bh, buf, "ambiance : COUCHANT (rendu.ini), NUIT, JOUR, ORAGE (graphismes/ambiance_*.ini)")) {
        gfxAmbiance = (gfxAmbiance + 1) % 4; gfx_reload(); popT = 0;
    }
    snprintf(buf, sizeof buf, "OMBRES : %s", yes_no(gfxShadows));
    if (gfx_button(x1, y + 2 * L.step, cw, L.bh, buf, "ombres des joueurs et des ennemis sur le decor")) gfxShadows = !gfxShadows;
    snprintf(buf, sizeof buf, "BLOOM : %s", yes_no(gfxBloom));
    if (gfx_button(x1, y + 3 * L.step, cw, L.bh, buf, "halo autour des neons et du soleil (cle bloom de rendu.ini)")) gfxBloom = !gfxBloom;
    snprintf(buf, sizeof buf, "EFFETS D'ECRAN : %s", yes_no(screenFx));
    if (gfx_button(x1, y + 4 * L.step, cw, L.bh, buf, "vignette, bords rouges quand la vie est basse, lignes de vitesse")) screenFx = !screenFx;
    snprintf(buf, sizeof buf, "FLASHS D'IMPACT : %s", yes_no(impactFrames));   /* impact frames */
    if (gfx_button(x1, y + 5 * L.step, cw, L.bh, buf, "quelques images en couleurs inversees sur les gros coups")) impactFrames = !impactFrames;

    snprintf(buf, sizeof buf, "PLEIN ECRAN : %s", yes_no(fullscreen));
    if (gfx_button(x2, y, cw, L.bh, buf, "aussi avec la touche F11")) pendingAction = ACT_FULLSCREEN;
    snprintf(buf, sizeof buf, "AFFICHER LES FPS : %s", yes_no(showFps));
    if (gfx_button(x2, y + L.step, cw, L.bh, buf, "images par seconde, en haut a droite")) showFps = !showFps;
    snprintf(buf, sizeof buf, "SECOND ECRAN : %s", yes_no(secondScreen));
    if (gfx_button(x2, y + 2 * L.step, cw, L.bh, buf, "carte de la partie et statistiques sur un deuxieme moniteur")) secondScreen = !secondScreen;
    snprintf(buf, sizeof buf, "VUE DU SECOND ECRAN : %s", mapView3d ? "3D" : "HAUT");
    if (gfx_button(x2, y + 3 * L.step, cw, L.bh, buf, "carte vue de HAUT ou en 3D (cle carte_3d de rendu.ini)")) mapView3d = !mapView3d;
    if (!secondScreen) rect(x2, y + 3 * L.step, cw, L.bh, 0.02f, 0.02f, 0.06f, 0.55f);   /* sans second ecran : vue inactive */

    float iy = y + 6 * L.step + (L.compact ? 26.0f : 30.0f), lh = L.compact ? 26.0f : 28.0f;
    int maxc = (int)((x2 + cw - x1) / fontW[0]) - 14, n = (int)strlen(gfxDir);
    text(0, x1, iy, "F5 : recharger graphismes/ (rendu.ini, textures .bmp)", 0.85f, 0.87f, 0.95f, 1);
    if (gfxDir[0]) {                                      /* chemin trop long : on garde la fin */
        snprintf(buf, sizeof buf, "dossier : %s%s", n > maxc ? "..." : "", gfxDir + (n > maxc ? n - maxc : 0));
        text(0, x1, iy + lh, buf, 0.6f, 0.75f, 0.9f, 1);
    } else text(0, x1, iy + lh, "dossier graphismes absent : aspect par defaut du jeu", 1, 0.65f, 0.3f, 1);
    if (gfxHelp) text(0, x1, iy + 2 * lh + 6, gfxHelp, 1, 0.9f, 0.55f, 1);
    if (button(x1, iy + 3 * lh + 10, 210, L.bh + 6, "RETOUR")) settingsPage = 0;
    if (uiClick) uiClick = 0;
}

static void draw_settings(float cx, float cy) {
    char buf[64];
    (void)cy;
    if (settingsPage == 1) { waitAct = -1; draw_graphics_settings(cx); return; }
    SetLay L = settings_lay(cx);
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.82f);
    text_c(2, cx, L.ty, "PARAMETRES", 0.5f, 0.9f, 1.0f, 1);
    float lw = clampf(L.w * 0.38f, 370, 440), lx = L.x0, y = L.y0, sstep = L.compact ? 44.0f : 54.0f;

    text(1, lx, y, "REGLAGES", 0.5f, 0.9f, 1.0f, 1);
    y += L.compact ? 36.0f : 44.0f;
    snprintf(buf, sizeof buf, "%.2f", sens * 1000);
    sens = slider(1, lx, y, lw, "Sensibilite de la souris", buf, sens, 0.0003f, 0.008f);
    y += sstep;
    snprintf(buf, sizeof buf, "%.0f deg", fovBase);
    fovBase = slider(2, lx, y, lw, "Champ de vision (horizontal)", buf, fovBase, 80, 130);
    y += sstep;
    snprintf(buf, sizeof buf, "%.0f %%", masterVol * 100);
    masterVol = slider(3, lx, y, lw, "Volume", buf, masterVol, 0, 1);
    y += sstep;
    snprintf(buf, sizeof buf, "%.0f %%", musicVol * 100);
    musicVol = slider(4, lx, y, lw, "Musique", buf, musicVol, 0, 1);
    y += L.compact ? 34.0f : 40.0f;
    snprintf(buf, sizeof buf, "AXE Y : %s", invertY ? "INVERSE" : "NORMAL");
    if (button(lx, y, lw, L.bh, buf)) invertY = !invertY;
    y += L.step;
    snprintf(buf, sizeof buf, "CAMERA DE MORT : %s", yes_no(killcamOn));
    if (button(lx, y, lw, L.bh, buf)) killcamOn = !killcamOn;
    y += L.step;
    snprintf(buf, sizeof buf, "PARTIES EN LIGNE : %s", yes_no(onlineOn));
    if (button(lx, y, lw, L.bh, buf)) { onlineOn = !onlineOn; cfg_save(); }
    y += L.step;
    snprintf(buf, sizeof buf, "MISES A JOUR AUTO : %s", yes_no(updCheck));
    if (button(lx, y, lw, L.bh, buf)) { updCheck = !updCheck; cfg_save(); }
    y += L.step + (L.compact ? 6.0f : 10.0f);
    if (button(lx, y, lw, L.bh, "GRAPHISMES  >")) { settingsPage = 1; waitAct = -1; }
    rect(lx, y + L.bh - 3, lw, 3, 1.0f, 0.8f, 0.3f, 0.8f);   /* liseret dore : ouvre un autre ecran */
    y += L.step + (L.compact ? 8.0f : 14.0f);
    if (button(lx, y, lw * 0.5f - 5, L.bh + 6, "RETOUR")) { settingsOpen = 0; settingsPage = 0; waitAct = -1; cfg_save(); }
    if (button(lx + lw * 0.5f + 5, y, lw * 0.5f - 5, L.bh + 6, "PAR DEFAUT")) { memcpy(binds, bindDefault, sizeof binds); cfg_save(); }

    float gap = L.compact ? 28.0f : 60.0f, rx = lx + lw + gap, rw = L.w - lw - gap, ry = L.y0;
    int lmax = 0;
    for (int a = 0; a < A_COUNT; a++) if ((int)strlen(actLabel[a]) > lmax) lmax = (int)strlen(actLabel[a]);
    float labW = fontW[0] * (float)lmax + 14, cw = fminf(230, (rw - labW - 10) * 0.5f);
    text(1, rx, ry, "COMMANDES", 0.5f, 0.9f, 1.0f, 1);
    text(0, rx, ry + 26, waitAct >= 0 ? "appuie sur une touche (ECHAP : annuler, SUPPR : effacer)"
                                      : "clique une case pour la changer", 0.7f, 0.75f, 0.85f, 1);
    float top = ry + 38, rowH = clampf(((float)winH - 16 - top) / A_COUNT, 24, 36), ch = rowH - 4;
    for (int a = 0; a < A_COUNT; a++) {
        float yy = top + a * rowH;
        text(0, rx, yy + ch * 0.5f + 6, actLabel[a], 0.85f, 0.87f, 0.95f, 1);
        for (int k = 0; k < 2; k++) {
            char kn[32];
            float bx = rx + labW + k * (cw + 10);
            if (waitAct == a && waitSlot == k) snprintf(kn, sizeof kn, "...");
            else key_name(binds[a][k], kn, sizeof kn);
            int hover = uiMX >= bx && uiMX < bx + cw && uiMY >= yy && uiMY < yy + ch;
            int waiting = waitAct == a && waitSlot == k;
            rect(bx, yy, cw, ch, waiting ? 0.45f : hover ? 0.2f : 0.08f, waiting ? 0.35f : hover ? 0.4f : 0.1f,
                 waiting ? 0.1f : hover ? 0.6f : 0.18f, 0.9f);
            text_c(0, bx + cw * 0.5f, yy + ch * 0.5f + 6, kn, 1, 1, 1, 1);
            if (hover && uiClick) { uiClick = 0; waitAct = a; waitSlot = k; }
        }
    }
    if (uiClick) uiClick = 0;
}

static void draw_status(float x, float y) {
    if (netStatus[0] && nowT - netStatusT < 8) text(0, x, y, netStatus, 1, 0.65f, 0.3f, 1);
}

/* bouton de choix : cadre de la couleur de l'arme si elle est choisie, grise si l'arme n'est pas encore codee */
static int sel_button(float x, float y, float w, float h, const char *label, int chosen, int ready, V3 col) {
    int hover = ready && uiMX >= x && uiMX < x + w && uiMY >= y && uiMY < y + h;
    if (chosen) rect(x - 3, y - 3, w + 6, h + 6, col.x, col.y, col.z, 1);
    rect(x, y, w, h, hover ? 0.2f : 0.08f, hover ? 0.42f : 0.1f, hover ? 0.62f : 0.18f, 0.95f);
    float k = ready ? 1.0f : 0.4f;
    text_c(0, x + w * 0.5f, y + h * 0.5f + 5, ready ? label : vagOn ? "HORS VAGUES" : "BIENTOT", k, k, k, 1);
    if (hover && uiClick) { uiClick = 0; play(S_HIT, 0.4f, 1.5f); return 1; }
    return 0;
}

static void loadout_pick(int slot, int w) {               /* choisir pour un emplacement l'arme de l'autre les echange */
    if (w == wantLoadout[slot ^ 1]) wantLoadout[slot ^ 1] = wantLoadout[slot];
    wantLoadout[slot] = w;
}

static void draw_loadout(float cx, float cy) {
    char buf[160];
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.85f);
    /* mise en page du haut vers le bas : 4 rangees (etiquette 22 px, boutons 44 px, 18 px d'air), centree, jamais
     * au-dessus du bord (fenetre reduite : 640 px de haut suffisent) */
    const float R = 84;
    float top = fmaxf(10, cy - (370 + 3 * R) * 0.5f), y;
    text_c(2, cx, top + 50, "LOADOUT", 0.5f, 0.9f, 1.0f, 1);
    text_c(0, cx, top + 80, "1 arme de melee + 1 competence + 2 armes a distance ; la melee remplit les jauges de munitions",
           0.7f, 0.75f, 0.85f, 1);
    float bw = winW < 1300 ? 118.0f : 150.0f, step = bw + 10, x0 = cx - step * WP_COUNT * 0.5f;
    y = top + 128;
    text(1, x0, y - 8, "MELEE", 0.5f, 0.9f, 1.0f, 1);
    for (int m = 0; m < MK_COUNT; m++)
        if (sel_button(x0 + m * step, y, bw, 44, meleeNames[m], m == wantMelee, mkReady[m] && (!vagOn || vg_mk_ok(m)), v3(1, 0.85f, 0.4f)))
            wantMelee = m;
    y += R;
    text(1, x0, y - 8, "COMPETENCE", 0.5f, 0.9f, 1.0f, 1);
    for (int k = 0; k < SK_COUNT; k++)                    /* Competences : la piece est l'une d'elles */
        if (sel_button(x0 + k * step, y, bw, 44, skillDef[k].name, k == wantSkill, !vagOn || vg_sk_ok(k),
                       v3(skillDef[k].r, skillDef[k].g, skillDef[k].b)))
            wantSkill = k;
    for (int s = 0; s < 2; s++) {
        y += R;
        text(1, x0, y - 8, s ? "ARME 2" : "ARME 1", 0.5f, 0.9f, 1.0f, 1);
        for (int w = 0; w < WP_COUNT; w++)
            if (sel_button(x0 + w * step, y, bw, 44, ammoDef[w].name, w == wantLoadout[s], wpReady[w] && (!vagOn || vg_wp_ok(w)),
                           v3(ammoDef[w].r, ammoDef[w].g, ammoDef[w].b)))
                loadout_pick(s, w);
    }
    y += 72;
    for (int s = 0; s < 2; s++, y += 22) {
        const AmmoDef *d = &ammoDef[wantLoadout[s]];
        if (d->gauge <= 0) snprintf(buf, sizeof buf, "%s : objet unique, a recuperer", d->name);
        else snprintf(buf, sizeof buf, "%s : %d au spawn, %d max, %.0f pts par tir (%.1f coups d'epee)", d->name, d->start,
                      d->max, d->gauge, d->gauge / meleePts[MK_SWORD]);
        text(0, x0, y, buf, 0.8f, 0.82f, 0.9f, 1);
    }
    snprintf(buf, sizeof buf, "%s : %d pts par coup qui touche%s, +%d par kill, +%d par parry ; cellules : %d ou %d pts",
             meleeNames[wantMelee], meleePts[wantMelee], wantMelee == MK_FIST ? " (50 max par combo)" : "",   /* Poings */
             PTS_MELEE_KILL, PTS_PARRY, PICKUP_SMALL, PICKUP_BIG);
    text(0, x0, y, buf, 1, 0.85f, 0.5f, 1);
    y += 22;
    {                                                     /* Competences : description, charges sur la meme ligne si elle tient */
        const SkillDef *sd = &skillDef[wantSkill];
        char ch[48];
        snprintf(ch, sizeof ch, "%d charge%s, %.0f s par charge", sd->max, sd->max > 1 ? "s" : "", sd->recharge);
        snprintf(buf, sizeof buf, "%s : %s ; %s", sd->name, sd->desc, ch);
        if (x0 + fontW[0] * (float)strlen(buf) > winW - 10) {
            snprintf(buf, sizeof buf, "%s : %s", sd->name, sd->desc);
            text(0, x0, y, buf, sd->r, sd->g, sd->b, 1);
            y += 22;
            snprintf(buf, sizeof buf, "%*s%s", (int)strlen(sd->name) + 3, "", ch);
        }
        text(0, x0, y, buf, sd->r, sd->g, sd->b, 1);
        y += 22;
    }
    if (vagOn) text(0, x0, y, "VAGUES : Railgun, Roquettes, Shotgun, Lance, Impulsion, Piece et All in One seulement", 1, 0.85f, 0.5f, 1);
    else if (appState == APP_GAME) text(0, x0, y, "pris en compte au prochain respawn", 0.6f, 0.6f, 0.7f, 1);
    if (button(cx - 105, y + 14, 210, 50, "VALIDER")) { loadoutOpen = 0; cfg_save(); }
    if (uiClick) uiClick = 0;
}

static void map_cycle(int d) {                            /* menu : carte suivante / precedente, montree en fond */
    menuMap = map_step(menuMap, d);                       /* les cartes CHALLENGE (LA TOILE) n'y sont pas : bouton CHALLENGE */
    if (appState == APP_MENU) menu_backdrop();
    cfg_save();
}
static void draw_map_row(float bx, float y, float bw) {   /* ligne CARTE  < NOM >  ROTATION */
    float aw = 46, rw = 128, mw = bw - rw - 8;
    text(0, bx, y - 8, "CARTE", 0.7f, 0.75f, 0.85f, 1);
    if (button(bx, y, aw, 46, "<")) map_cycle(-1);
    rect(bx + aw + 4, y, mw - 2 * aw - 8, 46, 0.05f, 0.06f, 0.1f, 0.9f);
    text_c(1, bx + mw * 0.5f, y + 23 + fontH[1] * 0.3f, maps[menuMap].name, 1, 0.9f, 0.6f, 1);
    if (button(bx + mw - aw, y, aw, 46, ">")) map_cycle(1);
    if (sel_button(bx + mw + 8, y, rw, 46, "ROTATION", mapRotate, 1, v3(1, 0.85f, 0.4f))) { mapRotate ^= 1; cfg_save(); }
}

/* ecran SKIN : 5 apparences, apercu 3D qui tourne ; la couleur reelle sera celle de l'equipe */
static void draw_skin_menu(float cx, float cy) {
    static const char *prevNames[3] = { "BLEU", "ROUGE", "LIBRE" };
    char buf[96];
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.82f);
    text_c(2, cx, cy - 300, "SKIN", 0.5f, 0.9f, 1.0f, 1);
    text_c(0, cx, cy - 266, "la couleur de ton perso est celle de ton equipe (ou la tienne en chacun pour soi)", 0.7f, 0.75f, 0.85f, 1);
    float bx = cx - 520, y = cy - 220;
    if (!skin_unlocked(wantSkin)) wantSkin = SKN_ROBOT;
    for (int k = 0; k < SKN_COUNT; k++) {
        if (!skin_unlocked(k)) continue;                  /* easter egg : POUSSIN cache tant qu'il n'est pas debloque */
        if (button(bx, y, 320, 54, skinNames[k])) { wantSkin = k; cfg_save(); }
        if (wantSkin == k) {
            rect(bx, y, 320, 3, 1, 0.85f, 0.4f, 1); rect(bx, y + 51, 320, 3, 1, 0.85f, 0.4f, 1);
            rect(bx, y, 3, 54, 1, 0.85f, 0.4f, 1); rect(bx + 317, y, 3, 54, 1, 0.85f, 0.4f, 1);
        }
        y += 64;
    }
    y += 16;
    text(0, bx, y, "APERCU DE LA COULEUR", 0.7f, 0.75f, 0.85f, 1);
    y += 14;
    for (int k = 0; k < 3; k++) if (button(bx + k * 108, y, 100, 44, prevNames[k])) skinPrevTeam = k;
    y += 70;
    if (button(bx, y, 320, 52, "VALIDER")) { skinOpen = 0; cfg_save(); }
    float px = cx - 140, py = cy - 230, pw = 640, ph = 520;   /* apercu 3D */
    rect(px, py, pw, ph, 0.05f, 0.06f, 0.1f, 0.7f);
    rect(px, py + ph - 3, pw, 3, 0.5f, 0.9f, 1.0f, 0.5f);
    {
        static const float freeCol[3] = { 1.0f, 0.85f, 0.3f };
        const float *c = skinPrevTeam < 2 ? teamCol[skinPrevTeam] : freeCol;
        SkinAnim an;
        an.spd = 0.6f; an.run = (float)nowT * 7; an.air = 0; an.slide = 0; an.pitch = 0; an.t = (float)nowT;
        glViewport((int)px, winH - (int)(py + ph), (int)pw, (int)ph);
        set_frustum(32, pw / ph, 0.1f, 50);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST); glDisable(GL_BLEND);
        set_dyn_light(1);
        glTranslatef(0, -1.0f, -4.6f);
        glRotatef(-10, 1, 0, 0);
        glRotatef(180 + (float)nowT * 40, 0, 1, 0);      /* de face au depart */
        glEnable(GL_LIGHTING);
        draw_skin(wantSkin, H_STAND, c, &an);
        glDisable(GL_LIGHTING);
        glViewport(0, 0, winW, winH);                     /* retour au HUD */
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, winW, winH, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glDisable(GL_DEPTH_TEST); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    snprintf(buf, sizeof buf, "%s", skinNames[wantSkin]);
    text_c(1, px + pw * 0.5f, py + ph - 20, buf, 1, 0.9f, 0.6f, 1);
    if (uiClick) uiClick = 0;
}

/* regles de la partie (HEBERGER) : mode, grappin, munitions, boutique, duree */
static void draw_rules_panel(float x, float y, float w) {
    static const int mins[4] = { 3, 5, 10, 15 };
    char buf[96];
    rect(x - 20, y, w + 40, 540, 0.03f, 0.04f, 0.08f, 0.6f);
    rect(x - 20, y, w + 40, 2, 1.0f, 0.8f, 0.3f, 0.6f);
    y += 40;
    text(1, x, y, "REGLES", 1.0f, 0.8f, 0.3f, 1);
    y += 22;
    float aw = 46;
    if (button(x, y, aw, 46, "<"))                        /* mode precedent disponible sur la carte (FRAGS l'est toujours) */
        for (int k = 0; k < MODE_COUNT; k++) { wantRules.mode = (wantRules.mode + MODE_COUNT - 1) % MODE_COUNT; if (mode_ok(wantRules.mode, menuMap)) break; }
    rect(x + aw + 4, y, w - 2 * aw - 8, 46, 0.05f, 0.06f, 0.1f, 0.9f);
    snprintf(buf, sizeof buf, "MODE : %s", modeNames[wantRules.mode]);
    text_c(1, x + w * 0.5f, y + 23 + fontH[1] * 0.3f, buf, 1, 0.9f, 0.6f, 1);
    if (button(x + w - aw, y, aw, 46, ">"))
        for (int k = 0; k < MODE_COUNT; k++) { wantRules.mode = (wantRules.mode + 1) % MODE_COUNT; if (mode_ok(wantRules.mode, menuMap)) break; }
    if (!mode_ok(wantRules.mode, menuMap)) wantRules.mode = MODE_FRAGS;   /* changement de carte */
    y += 56;
    text(0, x, y + 6, modeHelp[wantRules.mode], 0.8f, 0.82f, 0.9f, 1);
    if (mode_team(wantRules.mode) && wantTeams == 0) { y += 20; text(0, x, y + 6, "mode d'equipe : 2 contre 2 au moins", 1, 0.6f, 0.4f, 1); }
    y += 34;
    float hw = (w - 10) * 0.5f;
    if (sel_button(x, y, hw, 44, wantRules.hook ? "GRAPPIN : OUI" : "GRAPPIN : NON", wantRules.hook, 1, v3(1, 0.85f, 0.4f))) wantRules.hook ^= 1;
    snprintf(buf, sizeof buf, "MUNITIONS : %s", ammoRuleNames[wantRules.ammo]);
    if (sel_button(x + hw + 10, y, hw, 44, buf, wantRules.ammo != AMMO_NORMAL, 1, v3(1, 0.85f, 0.4f))) wantRules.ammo = (wantRules.ammo + 1) % AMMO_COUNT;
    y += 54;
    if (sel_button(x, y, w, 44, wantRules.shop ? "BOUTIQUE : OUI" : "BOUTIQUE : NON", wantRules.shop, 1, v3(1, 0.85f, 0.4f))) wantRules.shop ^= 1;
    y += 54;
    text(0, x, y + 4, "DUREE", 0.7f, 0.75f, 0.85f, 1);
    y += 12;
    for (int k = 0; k < 4; k++) {
        snprintf(buf, sizeof buf, "%d MIN", mins[k]);
        if (sel_button(x + k * (w / 4), y, w / 4 - 8, 44, buf, wantRules.minutes == mins[k], 1, v3(1, 0.85f, 0.4f))) wantRules.minutes = mins[k];
    }
    y += 64;
    if (wantRules.shop) {
        text(0, x, y, "boutique : une arme de depart parmi 4, des pieces a chaque kill, mort et objectif,", 0.75f, 0.78f, 0.88f, 1);
        text(0, x, y + 20, "et on achete d'autres armes a chaque mort", 0.75f, 0.78f, 0.88f, 1);
    }
    if (uiClick) cfg_save();
}

/* ecran HEBERGER UNE PARTIE : nombre de joueurs (chacun pour soi ou equipes 1v1 a 4v4), bots pour completer */
static void draw_host_menu(float cx, float cy) {
    static const char *modeNames[TEAM_MAX + 1] = { "LIBRE", "1V1", "2V2", "3V3", "4V4" };
    static const char *botNames[4] = { "AUCUN", "FACILE", "NORMAL", "DIFFICILE" };
    char buf[128];
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.78f);
    text_c(2, cx, cy - 300, "HEBERGER UNE PARTIE", 0.5f, 0.9f, 1.0f, 1);
    float bw = 620, bx = (winW >= 1260 ? cx - bw - 40 : cx - bw * 0.5f), y = cy - 230;
    if (winW >= 1260) draw_rules_panel(cx + 20, y - 40, 600);
    rect(bx - 30, y - 40, bw + 60, 540, 0.03f, 0.04f, 0.08f, 0.6f);
    rect(bx - 30, y - 40, bw + 60, 2, 0.5f, 0.9f, 1.0f, 0.5f);
    y -= 6;
    draw_map_row(bx, y, bw);                              /* carte de la partie (ROTATION : la suivante a chaque manche) */
    y += 76;
    text(1, bx, y, "NOMBRE DE JOUEURS", 0.5f, 0.9f, 1.0f, 1);
    y += 18;
    for (int m = 0; m <= TEAM_MAX; m++) {                 /* chacun pour soi, puis 1v1 a 4v4 */
        float mx = bx + m * 126.0f;
        if (button(mx, y, 116, 50, modeNames[m])) wantTeams = m;
        if (wantTeams == m) {                             /* mode choisi : cadre lumineux */
            rect(mx, y, 116, 3, 1, 0.85f, 0.4f, 1); rect(mx, y + 47, 116, 3, 1, 0.85f, 0.4f, 1);
            rect(mx, y, 3, 50, 1, 0.85f, 0.4f, 1); rect(mx + 113, y, 3, 50, 1, 0.85f, 0.4f, 1);
        }
    }
    y += 82;
    if (wantTeams > 0)
        snprintf(buf, sizeof buf, "2 equipes de %d  -  %d joueurs max  -  premiere equipe a %d frags", wantTeams, wantTeams * 2,
                 team_frag_limit(wantTeams));
    else snprintf(buf, sizeof buf, "chacun pour soi  -  %d joueurs max  -  premier a %d frags", MAXP, FRAG_LIMIT);
    text(0, bx, y, buf, 0.85f, 0.85f, 0.95f, 1);
    y += 50;
    text(1, bx, y, "COMPLETER AVEC DES BOTS", 0.5f, 0.9f, 1.0f, 1);
    y += 18;
    int noBots = mode_team(wantRules.mode) && mode_ok(wantRules.mode, menuMap);   /* modes a objectif : entre humains seulement */
    if (noBots) hostBots = -1;
    for (int k = 0; k < 4; k++) {                         /* -1 = aucun bot, sinon leur niveau */
        float mx = bx + k * 157.0f;
        if (noBots) { if (sel_button(mx, y, 147, 50, botNames[k], k == 0, k == 0, v3(1, 0.85f, 0.4f))) hostBots = -1; continue; }
        if (button(mx, y, 147, 50, botNames[k])) hostBots = k - 1;
        if (hostBots == k - 1) {
            rect(mx, y, 147, 3, 1, 0.85f, 0.4f, 1); rect(mx, y + 47, 147, 3, 1, 0.85f, 0.4f, 1);
            rect(mx, y, 3, 50, 1, 0.85f, 0.4f, 1); rect(mx + 144, y, 3, 50, 1, 0.85f, 0.4f, 1);
        }
    }
    y += 82;
    if (noBots) snprintf(buf, sizeof buf, "pas de bots en %s : partie entre joueurs (LAN ou en ligne)", modeNames[wantRules.mode]);
    else if (hostBots < 0) snprintf(buf, sizeof buf, "les autres joueurs te trouvent en LAN ou en ligne");
    else if (wantTeams > 0)
        snprintf(buf, sizeof buf, "%d bot%s : toi + %d allie%s contre %d", wantTeams * 2 - 1, wantTeams * 2 - 1 > 1 ? "s" : "",
                 wantTeams - 1, wantTeams > 2 ? "s" : "", wantTeams);
    else snprintf(buf, sizeof buf, "1 bot, la partie reste ouverte en LAN");
    text(0, bx, y, buf, 0.85f, 0.85f, 0.95f, 1);
    y += 46;
    if (button(bx, y, bw, 56, "LANCER LA PARTIE")) {
        hostOpen = 0;
        if (hostBots < 0) pendingAction = ACT_HOST;
        else { pendingAction = ACT_VSBOT; pendingArg = hostBots; }
    }
    y += 70;
    if (button(bx, y, 240, 48, "RETOUR")) { hostOpen = 0; cfg_save(); }
    draw_status(bx, y + 90);
    if (uiClick) uiClick = 0;
}

/* encadre du menu : ma cote, puis le TOP 3 (podium or / argent / bronze) et mon rang */
static void draw_rank_box(float cx, float cy) {
    static const float medal[3][3] = { { 1.0f, 0.8f, 0.25f }, { 0.78f, 0.82f, 0.88f }, { 0.8f, 0.5f, 0.28f } };
    char buf[128], tl[32];
    const float *c = rank_col(myMmr);
    rank_label(myMmr, rankPlayed, tl, sizeof tl);
    float w = 380, x = cx + 380, y = cy - 214;
    if (x + w > winW - 8) {                               /* fenetre etroite : une seule ligne sous le titre */
        int k = snprintf(buf, sizeof buf, "%s %d", tl, myMmr);
        if (rkTopState == 1)
            for (int i = 0; i < rkTopN && k < (int)sizeof buf - 30; i++)
                k += snprintf(buf + k, sizeof buf - k, "   %d. %s %d", i + 1, rkTop[i].name, rkTop[i].mmr);
        text_c(0, cx, cy - 240, buf, c[0], c[1], c[2], 1);
        return;
    }
    rect(x, y, w, 290, 0.03f, 0.04f, 0.08f, 0.82f);
    rect(x, y, 4, 290, c[0], c[1], c[2], 1);
    text(1, x + 18, y + 34, "CLASSEMENT", 0.5f, 0.9f, 1.0f, 1);
    snprintf(buf, sizeof buf, "%s  -  %d", tl, myMmr);
    text(1, x + 18, y + 70, buf, c[0], c[1], c[2], 1);
    if (rkRank > 0 || rkMeRank > 0) snprintf(buf, sizeof buf, "#%d  -  %d match%s, %d victoire%s", rkMeRank ? rkMeRank : rkRank,
                                           rankPlayed, rankPlayed > 1 ? "s" : "", rankWins, rankWins > 1 ? "s" : "");
    else snprintf(buf, sizeof buf, "%d match%s classe%s, %d victoire%s", rankPlayed, rankPlayed > 1 ? "s" : "",
                  rankPlayed > 1 ? "s" : "", rankWins, rankWins > 1 ? "s" : "");
    text(0, x + 18, y + 96, buf, 0.7f, 0.75f, 0.85f, 1);
    text(0, x + 18, y + 132, "TOP 3", 0.9f, 0.9f, 1.0f, 1);
    rect(x + 18, y + 140, w - 36, 2, 0.4f, 0.85f, 1.0f, 0.35f);
    if (rkTopState != 1) {
        text(0, x + 18, y + 176, rkTopState == 2 ? "classement indisponible" : "chargement ...", 0.6f, 0.6f, 0.7f, 1);
        return;
    }
    if (!rkTopN) text(0, x + 18, y + 176, "personne n'a fini son placement", 0.6f, 0.6f, 0.7f, 1);
    for (int i = 0; i < rkTopN; i++) {
        float yy = y + 150 + i * 32.0f;
        const float *m = medal[i];
        rect(x + 18, yy, 26, 26, m[0], m[1], m[2], 1);
        snprintf(buf, sizeof buf, "%d", i + 1);
        text_c(0, x + 31, yy + 19, buf, 0.08f, 0.08f, 0.1f, 1);
        text(0, x + 54, yy + 19, rkTop[i].name, m[0], m[1], m[2], 1);
        snprintf(buf, sizeof buf, "%d  %s", rkTop[i].mmr, rkTop[i].tier);
        text(0, x + w - 18 - fontW[0] * (float)strlen(buf), yy + 19, buf, 0.85f, 0.85f, 0.9f, 1);
    }
    if (rkMeRank > 3 || (!rkMeRank && rankPlayed < RANK_PLACEMENT)) {   /* 4e ligne : moi, si je ne suis pas sur le podium */
        if (rkMeRank) snprintf(buf, sizeof buf, "toi : #%d, cote %d", rkMeRank, myMmr);
        else snprintf(buf, sizeof buf, "toi : en placement (%d/%d), cote %d", rankPlayed, RANK_PLACEMENT, myMmr);
        text(0, x + 18, y + 270, buf, c[0], c[1], c[2], 1);
    }
}

/* ecran CHALLENGE : une course par carte CHALLENGE, le speedrun complet, la vague 100 ; a droite le classement de la
 * categorie choisie (srSel) : record, top 10, mon rang et mon meilleur temps (serveur), mon record local */
static int sr_local_best(int cat) {                       /* record local (ms) : course de la carte en memoire, speedrun complet */
    if (cat == SR_COMPLET) return srPbN == srMapN && srMapN ? srPb[srMapN - 1] : 0;
    if (cat >= SR_MAP0 && raceLoaded == cat - SR_MAP0 && raceBest > 0) return (int)lroundf(raceBest * 1000);
    return 0;
}
static void draw_sr_board(int cat, float x, float y, float w) {   /* classement d'une categorie */
    static const float medal[3][3] = { { 1.0f, 0.8f, 0.25f }, { 0.78f, 0.82f, 0.88f }, { 0.8f, 0.5f, 0.28f } };
    const SrTop *t = &srTop[cat];
    char buf[128], tb[24], tb2[24];
    rect(x, y, w, 470, 0.03f, 0.04f, 0.08f, 0.82f);
    rect(x, y, 4, 470, 1.0f, 0.8f, 0.3f, 1);
    snprintf(buf, sizeof buf, "CLASSEMENT  -  %s", sr_label(cat));
    text(1, x + 18, y + 34, buf, 0.5f, 0.9f, 1.0f, 1);
    if (t->state == 2 && t->wrMs > 0) {
        sr_time_str(tb, sizeof tb, t->wrMs);
        snprintf(buf, sizeof buf, "RECORD  %s  par %s   -   %d coureur%s", tb, t->wrName, t->runners, t->runners > 1 ? "s" : "");
    } else snprintf(buf, sizeof buf, "plancher du serveur : %d s", sr_floor_ms(cat) / 1000);
    text(0, x + 18, y + 62, buf, 1.0f, 0.82f, 0.35f, 1);
    rect(x + 18, y + 74, w - 36, 2, 0.4f, 0.85f, 1.0f, 0.35f);
    if (t->state != 2) {
        text(0, x + 18, y + 110, t->state == 3 ? "classement indisponible (hors ligne ?)" : "chargement ...", 0.6f, 0.6f, 0.7f, 1);
    } else if (!t->n) text(0, x + 18, y + 110, "personne n'a encore de temps : le premier record est a prendre", 0.6f, 0.6f, 0.7f, 1);
    for (int i = 0; i < t->n; i++) {
        float yy = y + 86 + i * 30.0f;
        int me = t->meRank == i + 1;
        const float *m = i < 3 ? medal[i] : NULL;
        if (me) rect(x + 14, yy - 2, w - 28, 28, 0.3f, 0.85f, 1.0f, 0.12f);
        if (m) rect(x + 18, yy, 24, 24, m[0], m[1], m[2], 1);
        snprintf(buf, sizeof buf, "%d", i + 1);
        if (m) text_c(0, x + 30, yy + 18, buf, 0.08f, 0.08f, 0.1f, 1); else text_c(0, x + 30, yy + 18, buf, 0.7f, 0.75f, 0.85f, 1);
        text(0, x + 54, yy + 18, t->name[i], m ? m[0] : 0.9f, m ? m[1] : 0.9f, m ? m[2] : 0.95f, 1);
        sr_time_str(tb, sizeof tb, t->ms[i]);
        int gap = t->ms[i] - t->ms[0];                   /* ecart au premier : +1.55, ou +1:02.37 au-dela d'une minute */
        if (gap >= 60000) sr_time_str(tb2, sizeof tb2, gap); else snprintf(tb2, sizeof tb2, "%.2f", gap / 1000.0f);
        if (i) { char g[32]; snprintf(g, sizeof g, "+%s", tb2); snprintf(buf, sizeof buf, "%s  %9s", tb, g); }   /* colonnes alignees */
        else snprintf(buf, sizeof buf, "%s  %9s", tb, "");
        text(0, x + w - 18 - fontW[0] * (float)strlen(buf), yy + 18, buf, 0.85f, 0.85f, 0.9f, 1);
    }
    float yb = y + 400;
    rect(x + 18, yb - 14, w - 36, 2, 0.4f, 0.85f, 1.0f, 0.35f);
    if (t->state == 2 && t->meMs > 0) {
        sr_time_str(tb, sizeof tb, t->meMs);
        snprintf(buf, sizeof buf, "toi : #%d sur %d  -  meilleur temps %s", t->meRank, t->runners, tb);
    } else snprintf(buf, sizeof buf, "toi : pas encore de temps au classement");
    text(0, x + 18, yb + 12, buf, 0.4f, 1.0f, 0.6f, 1);
    int lb = sr_local_best(cat);
    if (lb > 0) { sr_time_str(tb, sizeof tb, lb); snprintf(buf, sizeof buf, "record sur cette machine : %s", tb); }
    else snprintf(buf, sizeof buf, "%s", cat == SR_VAGUES100 ? "se joue en SOLO - VAGUES : finis la vague 100" :
                                          "aucun temps sur cette machine");
    text(0, x + 18, yb + 40, buf, 0.65f, 0.7f, 0.8f, 1);
}
static void draw_challenge_menu(float cx, float cy) {
    char buf[96], tb[24];
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.78f);
    text_c(2, cx, cy - 280, "CHALLENGE", 1.0f, 0.85f, 0.35f, 1);
    text_c(0, cx, cy - 250, "contre la montre : chaque temps compte au classement en ligne", 0.7f, 0.75f, 0.85f, 1);
    sr_maps_init();
    if (!sr_cat_ok(srSel)) srSel = srMapN ? SR_MAP0 + srMaps[0] : SR_COMPLET;
    int cats[NMAPS + 2], nc = 0;
    for (int k = 0; k < srMapN; k++) cats[nc++] = SR_MAP0 + srMaps[k];
    cats[nc++] = SR_COMPLET; cats[nc++] = SR_VAGUES100;
    float bw = 360, bx = cx - 480, y = cy - 214;
    for (int k = 0; k < nc; k++) {
        int c = cats[k];
        if (c >= SR_MAP0) snprintf(buf, sizeof buf, "COURSE  -  %s", sr_label(c));
        else if (c == SR_COMPLET) snprintf(buf, sizeof buf, "SPEEDRUN COMPLET  (%d carte%s)", srMapN, srMapN > 1 ? "s" : "");
        else snprintf(buf, sizeof buf, "%s  (mode VAGUES)", sr_label(c));
        if (sel_button(bx, y, bw, 46, buf, srSel == c, 1, v3(1, 0.85f, 0.35f))) srSel = c;
        int lb = sr_local_best(c), sb = srTop[c].state == 2 ? srTop[c].meMs : 0;
        if (sb > 0 && (lb <= 0 || sb < lb)) lb = sb;
        if (lb > 0) { sr_time_str(tb, sizeof tb, lb); snprintf(buf, sizeof buf, "ton record %s", tb); }
        else snprintf(buf, sizeof buf, "pas encore de temps");
        text(0, bx + 4, y + 64, buf, 0.65f, 0.7f, 0.8f, 1);
        y += 82;
    }
    y += 6;
    if (srSel == SR_VAGUES100) text(0, bx, y + 30, "lance une partie SOLO - VAGUES depuis le menu", 0.8f, 0.8f, 0.9f, 1);
    else if (button(bx, y, bw, 56, srSel == SR_COMPLET ? "LANCER LE SPEEDRUN" : "LANCER LA COURSE")) {
        if (srSel == SR_COMPLET) pendingAction = ACT_SPEEDRUN;
        else { pendingAction = ACT_RACE; pendingArg = srSel - SR_MAP0; }
    }
    y += 70;
    if (button(bx, y, 200, 46, "RETOUR")) challengeOpen = 0;
    draw_status(bx, y + 80);
    draw_sr_board(srSel, cx - 90, cy - 214, 570);
    if (uiClick) uiClick = 0;
}

static void draw_main_menu(float cx, float cy) {
    /* assombri surtout en haut (lisibilite du titre) ; vers le bas, le decor (sol, arene) reste visible */
    rect_vgrad(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.78f, 0.30f);
    for (int k = 0; k < 7; k++) {                             /* halo doux derriere le titre (passes decalees, faux flou) */
        float t = (float)nowT;                                /* gameTime est gele hors partie : horloge murale */
        float a = k * (2 * PI / 7) + t * 0.3f, rr = 3.5f + 1.5f * sinf(t * 0.7f + k);
        text_c(2, cx + cosf(a) * rr, cy - 300 + sinf(a) * rr, "V E L O C I T Y", 0.4f, 0.85f, 1.0f, 0.05f);
    }
    text_c(2, cx, cy - 300, "V E L O C I T Y", 0.5f, 0.9f, 1.0f, 1);
    text_c(0, cx, cy - 268, "movement shooter en C", 0.7f, 0.7f, 0.8f, 1);
    rect(cx - 220, cy - 252, 440, 2, 0.5f, 0.9f, 1.0f, 0.5f);   /* trait d'accent sous le sous-titre */
    float bx = cx - 620, bw = 420, y = cy - 205;
    rect(bx - 30, y - 30, bw + 60, 610, 0.03f, 0.04f, 0.08f, 0.55f);        /* panneau derriere les boutons */
    rect(bx - 30, y - 30, bw + 60, 2, 0.5f, 0.9f, 1.0f, 0.5f);
    rect(bx - 30, y + 578, bw + 60, 2, 0.5f, 0.9f, 1.0f, 0.25f);
    field(bx, y, bw - 136, 48, "PSEUDO", myName, 1);
    {                                                     /* compte Discord : meme identite sur tous les PC */
        float dx = bx + bw - 128;
        const char *dl = dcState != DC_IDLE ? "navigateur ..." : rkDiscord[0] ? rkDiscord : "COMPTE";
        char dlb[20];
        snprintf(dlb, sizeof dlb, "%.*s", (int)(128 / (fontW[0] > 0 ? fontW[0] : 8)), dl);   /* tronque a la largeur */
        text(0, dx, y - 8, dlb, rkDiscord[0] ? 0.55f : 0.7f, rkDiscord[0] ? 0.95f : 0.75f, rkDiscord[0] ? 0.6f : 0.85f, 1);
        if (button(dx, y, 128, 48, dcState != DC_IDLE ? "..." : "DISCORD")) dc_begin();
        if (rkDiscord[0]) rect(dx, y + 45, 128, 3, 0.35f, 0.95f, 0.5f, 0.9f);   /* liseret vert : compte lie */
        else rect(dx, y + 45, 128, 3, 0.45f, 0.5f, 1.0f, 0.9f);                /* liseret Discord : a connecter */
    }
    y += 76;
    draw_map_row(bx, y, bw);                              /* carte : solo (cartes solo seulement), duel, heberger */
    y += 54;
    char soloLbl[48];
    snprintf(soloLbl, sizeof soloLbl, maps[menuMap].solo ? "VAGUES" : "VAGUES (%s)", maps[0].name);
    if (button(bx, y, bw * 0.62f - 4, 52, soloLbl)) pendingAction = ACT_SOLO;
    if (button(bx + bw * 0.62f + 4, y, bw * 0.38f - 4, 52, "CHALLENGE")) { challengeOpen = 1; rq_sr_top_all(); }   /* courses, speedrun */
    y += 52 + 24;                                         /* le libelle sous le bouton, sans toucher son liseret */
    text(0, bx, y, "JOUEUR CONTRE BOT  (duel)", 0.7f, 0.75f, 0.85f, 1);
    y += 8;
    for (int lv = 0; lv < 3; lv++)                        /* un duel contre un bot : trois niveaux */
        if (button(bx + lv * 142, y, 136, 46, botLevelNames[lv])) { pendingAction = ACT_VSBOT; pendingArg = lv; hostFfa = 1; }
    y += 54;
    if (button(bx, y, 200, 46, "PARTIE RAPIDE")) pendingAction = ACT_QUICK;
    if (button(bx + 208, y, bw - 208, 46, "PARTIE CLASSEE")) pendingAction = ACT_RANKED;
    rect(bx + 208, y + 43, bw - 208, 3, 1.0f, 0.8f, 0.3f, 0.9f);   /* liseret dore : mode classe */
    y += 54;
    if (button(bx, y, bw, 46, "HEBERGER UNE PARTIE")) { hostOpen = 1; uiFocus = 0; }
    y += 54;
    if (button(bx, y, bw, 46, "REJOINDRE / REGARDER")) pendingAction = ACT_BROWSE;
    y += 54;
    if (button(bx, y, bw / 3 - 6, 46, "LOADOUT")) loadoutOpen = 1;
    if (button(bx + bw / 3 + 3, y, bw / 3 - 6, 46, "SKIN")) skinOpen = 1;
    if (button(bx + 2 * bw / 3 + 6, y, bw / 3 - 6, 46, "PARAMETRES")) { settingsOpen = 1; settingsPage = 0; }
    y += 54;
    upd_poll();
    if (updState == UPD_AVAILABLE || updState == UPD_READY)   /* nouvelle version : cadre dore */
        rect(bx - 3, y - 3, bw + 6, 52, 1.0f, 0.8f, 0.3f, 0.5f + 0.4f * sinf((float)nowT * 5));
    if (button(bx, y, bw, 46, upd_label())) upd_click();
    if (updState == UPD_DOWNLOADING && updTotal > 0)      /* progression dans le liseret du bouton */
        rect(bx, y + 40, bw * clampf((float)updGot / updTotal, 0, 1), 6, 1.0f, 0.8f, 0.3f, 1);
    y += 54;
    if (button(bx, y, bw, 46, "QUITTER")) pendingAction = ACT_QUIT;
    draw_status(bx, y + 72);
    draw_controls(cx - 130, cy - 200);
    draw_rank_box(cx, cy);
    if (uiClick) { uiClick = 0; uiFocus = 0; }
}

static void draw_join_menu(float cx, float cy) {
    char buf[128];
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.7f);
    text_c(2, cx, cy - 300, "REJOINDRE / REGARDER UNE PARTIE", 0.5f, 0.9f, 1.0f, 1);
    float bx = cx - 420, bw = 840, y = cy - 250;
    snprintf(buf, sizeof buf, "%s   |   %s", joining ? "connexion en cours..." : "parties du reseau local et en ligne",
             lb_status_text());
    text(0, bx, y, buf, 0.7f, 0.75f, 0.85f, 1);
    y += 14;
    int shown = 0;
    for (int i = 0; i < nFound && shown < 7; i++) {
        int dup = 0;                                      /* meme partie vue en LAN et en ligne : on garde la LAN */
        if (found[i].online)
            for (int j = 0; j < nFound; j++) if (!found[j].online && !strcmp(found[j].name, found[i].name)) dup = 1;
        if (dup) continue;
        if (found[i].online)
            snprintf(buf, sizeof buf, "[EN LIGNE]%s  %s    %d/%d joueurs%s", found[i].ranked ? " [CLASSEE]" : "", found[i].name,
                     found[i].players, found[i].maxp, found[i].spec ? "  (spectateurs)" : "");
        else {
            char mode[16];
            if (found[i].teams > 0) snprintf(mode, sizeof mode, "%dV%d", found[i].teams, found[i].teams); else snprintf(mode, sizeof mode, "LIBRE");
            snprintf(buf, sizeof buf, "[LAN %s]%s  %s  -  %s    %d/%d    %s", mode, found[i].ranked ? " [CLASSEE]" : "", found[i].name,
                     found[i].map >= 0 ? maps[found[i].map].name : "?", found[i].players, found[i].maxp,
                     inet_ntoa(found[i].addr.sin_addr));
        }
        if (button(bx, y, bw - 170, 46, buf)) { pendingAction = ACT_JOIN_FOUND; pendingArg = i; }
        if (button(bx + bw - 160, y, 160, 46, "REGARDER")) { pendingAction = ACT_SPEC_FOUND; pendingArg = i; }
        y += 54;
        shown++;
    }
    if (!shown) {
        text(0, bx, y + 30, "aucune partie pour l'instant - lance une PARTIE RAPIDE ou heberge la tienne", 0.6f, 0.6f, 0.7f, 1);
        y += 54;
    }
    y += 24;
    if (button(bx, y, 300, 46, "PARTIE RAPIDE")) pendingAction = ACT_QUICK;
    y += 60;
    field(bx, y, 520, 48, "OU TAPER L'IP DE L'HOTE", ipField, 2);
    if (button(bx + 540, y, 300, 48, "REJOINDRE")) pendingAction = ACT_JOIN_IP;
    draw_status(bx, y + 90);
    if (button(bx, y + 130, 300, 50, "RETOUR")) pendingAction = ACT_BACK;
    if (uiClick) { uiClick = 0; uiFocus = 0; }
}

static void draw_pause(float cx, float cy) {
    char buf[96];
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.72f);
    text_c(2, cx, cy - 300, dmMode ? "MENU" : "PAUSE", 0.5f, 0.9f, 1.0f, 1);
    if (dmMode) text_c(0, cx, cy - 268, "la partie continue pendant que tu es ici", 0.8f, 0.6f, 0.4f, 1);
    float bx = cx - 620, bw = 420, y = cy - 200;
    if (netMode == NET_HOST) {
        text(0, bx, y, "TU HEBERGES - les autres te trouvent tout seuls,", 0.7f, 0.75f, 0.85f, 1);
        text(0, bx, y + 22, "ou peuvent taper ton IP :", 0.7f, 0.75f, 0.85f, 1);
        text(1, bx, y + 56, hostIpText, 1, 0.85f, 0.4f, 1);
        snprintf(buf, sizeof buf, "%s   |   %d spectateur%s", lb_status_text(), spec_count(), spec_count() > 1 ? "s" : "");
        text(0, bx, y + 84, buf, 0.5f, 0.85f, 0.6f, 1);
        y += 112;
    }
    if (button(bx, y, bw, 54, "REPRENDRE")) pendingAction = ACT_RESUME;
    y += 68;
    if (button(bx, y, bw, 54, "LOADOUT")) loadoutOpen = 1;
    y += 68;
    if (!dmMode) {
        if (button(bx, y, bw, 54, "RECOMMENCER")) pendingAction = ACT_RESTART;
        y += 68;
    }
    if (button(bx, y, bw, 54, "PARAMETRES")) { settingsOpen = 1; settingsPage = 0; }
    y += 68;
    if (button(bx, y, bw, 54, dmMode ? "QUITTER LA PARTIE" : "MENU PRINCIPAL")) pendingAction = ACT_LEAVE;
    draw_controls(cx - 130, cy - 200);
    if (uiClick) { uiClick = 0; pendingAction = ACT_RESUME; }  /* clic ailleurs = reprendre */
}
