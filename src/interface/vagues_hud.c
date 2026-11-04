/* VELOCITY - mode VAGUES : HUD (vague, credits, temps), barre du boss, boutique, selecteur de carte, ecrans de fin
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static int vgShopBack = VGM_NONE;                         /* ecran a rouvrir quand on ferme la boutique */
static int vgShopHover = -1;                              /* etape survolee (chemin * 5 + etape) : description */

static void vg_time_text(char *buf, size_t n, float t) {  /* m:ss.cc */
    if (t < 0) t = 0;
    int cs = (int)(t * 100 + 0.5f);
    snprintf(buf, n, "%d:%02d.%02d", cs / 6000, cs / 100 % 60, cs % 100);
}

/* panneau en haut a gauche (remplace celui de l'ancien solo) */
static void vg_draw_info(void) {
    char buf[96], tb[24];
    if (raceOn) return;                                   /* challenge COURSE : le HUD de la course */
    hud_panel(16, 14, fontW[0] * 34.0f + 28, 124);
    int left = vgQn - vgQi + vg_alive_minions();
    if (vgInfinite) snprintf(buf, sizeof buf, "INFINI  -  VAGUE %d", wave);
    else snprintf(buf, sizeof buf, "VAGUE %d / %d   ennemis %d", wave, VG_WAVES, left);
    text(0, 30, 40, buf, 0.85f, 0.85f, 0.95f, 1);
    snprintf(buf, sizeof buf, "SCORE %d", score);
    text(1, 30, 78, buf, 1, 1, 1, 1);
    vg_time_text(tb, sizeof tb, vgRunT);
    snprintf(buf, sizeof buf, "CREDITS %d   temps %s", vgCredits, tb);
    text(0, 30, 104, buf, 1, 0.85f, 0.35f, 0.95f);
    snprintf(buf, sizeof buf, "%s%s", maps[curMap].name, vgBlockHard ? "  (DIFFICILE : credits x1,5)" : "");
    text(0, 30, 126, buf, 0.6f, 0.65f, 0.75f, 0.9f);
    if (vgInfinite && vgState == VG_FIGHT && !vg_checkpoint(wave)) {   /* INFINI : chrono de la vague suivante */
        float f = clampf(vgWaveT / VG_INF_T, 0, 1);
        rect(30, 136, 260, 5, 0.1f, 0.1f, 0.15f, 0.8f);
        rect(30, 136, 260 * f, 5, 1, 0.35f + 0.5f * f, 0.3f, 0.95f);
    }
}

static void vg_draw_boss_bar(float cx) {
    char buf[96];
    if (vgBoss < 0 || !E[vgBoss].alive || !EN_BOSS(E[vgBoss].type)) return;
    Enemy *e = &E[vgBoss];
    float w = fminf(720.0f, winW - 420.0f), x = cx - w * 0.5f, y = 26, f = clampf(e->hp / e->maxHp, 0, 1);
    V3 c = enemy_color(e->type);
    rect(x - 4, y - 4, w + 8, 26, 0.02f, 0.02f, 0.05f, 0.75f);
    rect(x, y, w, 18, 0.12f, 0.06f, 0.08f, 0.9f);
    rect(x, y, w * f, 18, c.x, c.y, c.z, 0.95f);
    if (e->flash > 0) rect(x, y, w * f, 18, 1, 1, 1, 0.35f);
    int marks = e->type == EN_HEXA ? 3 : 1;               /* seuils de phase */
    for (int k = 1; k <= marks; k++) {
        float px = x + w * (marks == 1 ? 0.5f : k * 0.25f);
        rect(px - 1, y - 3, 2, 24, 1, 1, 1, 0.7f);
    }
    const char *st = "";
    if (e->st == BS_STUN) st = "  -  ETOURDI";
    else if (e->type == EN_TESS && boss_links(vgBoss) >= VG_LINKS) st = "  -  BOUCLIER";
    else if (e->type == EN_HEXA && e->st == BS_OPEN) st = "  -  COEUR OUVERT";
    snprintf(buf, sizeof buf, "%s%s%s", bossTitle[e->type - EN_PYRA], vgSecret ? "  (INFINI)" : "", st);
    text_c(1, cx, y + 50, buf, c.x, c.y, c.z, 1);
    if (vgHintT > 0) text_c(0, cx, y + 76, vgHint, 1, 0.9f, 0.6f, clampf(vgHintT, 0, 1));
}

/* boutique : les 4 chemins du loadout (2 armes, melee, competence), 5 etapes chacun */
static void vg_draw_shop(float cx, float cy) {
    char buf[128];
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.86f);
    int paths[4] = { P.loadout[0], P.loadout[1], UP_MELEE(P.meleeKind), UP_SKILL(P.skill) };
    float colW = fminf(300.0f, (winW - 60.0f) / 4 - 12), rowH = winH < 760 ? 50.0f : 60.0f;
    float total = 4 * colW + 3 * 12, x0 = cx - total * 0.5f, top = fmaxf(16, cy - (150 + 6 * rowH) * 0.5f);
    snprintf(buf, sizeof buf, "BOUTIQUE  -  %d CREDITS", vgCredits);
    text_c(2, cx, top + 44, buf, 1, 0.85f, 0.3f, 1);
    text_c(0, cx, top + 74, "aucun achat n'augmente une stat : chaque etape ajoute un comportement a cette arme",
           0.7f, 0.75f, 0.85f, 1);
    int hover = -1;
    for (int c = 0; c < 4; c++) {
        int u = paths[c];
        float x = x0 + c * (colW + 12), y = top + 100;
        V3 hc = u < WP_COUNT ? v3(ammoDef[u].r, ammoDef[u].g, ammoDef[u].b) : u < WP_COUNT + MK_COUNT ? v3(0.9f, 0.9f, 1) :
                v3(skillDef[u - WP_COUNT - MK_COUNT].r, skillDef[u - WP_COUNT - MK_COUNT].g, skillDef[u - WP_COUNT - MK_COUNT].b);
        rect(x, y, colW, 34, 0.06f, 0.07f, 0.12f, 0.95f);
        rect(x, y + 31, colW, 3, hc.x, hc.y, hc.z, 1);
        snprintf(buf, sizeof buf, "%s  %d/5", up_name(u), vgLvl[u]);
        text_c(1, x + colW * 0.5f, y + 24, buf, hc.x, hc.y, hc.z, 1);
        y += 44;
        for (int k = 0; k < 5; k++) {
            int fx = upPath[u].fx[k], own = k < vgLvl[u], next = k == vgLvl[u], can = next && vgCredits >= upPrice[k];
            int hv = uiMX >= x && uiMX < x + colW && uiMY >= y && uiMY < y + rowH - 6;
            if (hv) hover = u * 5 + k;
            float bg = hv && can ? 0.28f : 0.1f;
            rect(x, y, colW, rowH - 6, own ? 0.08f : bg * 0.5f, own ? 0.2f : bg, own ? 0.12f : bg * 1.4f, 0.95f);
            if (own) rect(x, y, 4, rowH - 6, 0.3f, 1, 0.5f, 1);
            else if (next) rect(x, y, 4, rowH - 6, can ? 1.0f : 0.6f, can ? 0.85f : 0.3f, 0.3f, 1);
            snprintf(buf, sizeof buf, "%d. %s", k + 1, fxDef[fx].name);
            float a = own || next ? 1.0f : 0.45f;
            text(0, x + 12, y + rowH * 0.42f, buf, a, a, a, 1);
            if (own) snprintf(buf, sizeof buf, "ACQUIS");
            else if (next) snprintf(buf, sizeof buf, "%d credits", upPrice[k]);
            else snprintf(buf, sizeof buf, "etape %d d'abord", k);
            text(0, x + 12, y + rowH * 0.42f + 19, buf, own ? 0.4f : next ? (can ? 1.0f : 1.0f) : 0.5f,
                 own ? 1.0f : next ? (can ? 0.85f : 0.4f) : 0.5f, own ? 0.55f : next ? (can ? 0.35f : 0.35f) : 0.55f, 1);
            if (hv && uiClick && can) { uiClick = 0; up_buy(u); play(S_PAD, 0.6f, 2.0f); }
            y += rowH;
        }
    }
    if (hover >= 0) vgShopHover = hover;
    float by = top + 100 + 44 + 5 * rowH + 6;
    if (vgShopHover >= 0) {
        int u = vgShopHover / 5, k = vgShopHover % 5, fx = upPath[u].fx[k];
        snprintf(buf, sizeof buf, "%s (%s) : %s", fxDef[fx].name, up_name(u), fxDef[fx].desc);
        text_c(0, cx, by + 6, buf, 1, 0.95f, 0.8f, 1);
    }
    if (button(cx - 160, by + 22, 320, 48, vgShopBack == VGM_SELECT ? "RETOUR AU CHOIX" : "RETOUR AU JEU")) {
        if (vgShopBack == VGM_SELECT) vgMenu = VGM_SELECT;
        else vg_menu_close();
        vgShopBack = VGM_NONE;
    }
    if (uiClick) uiClick = 0;
}

/* selecteur : carte simple, carte difficile, challenge */
static void vg_draw_select(float cx, float cy) {
    char buf[128];
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.8f);
    int b = wave / VG_BLOCK;
    snprintf(buf, sizeof buf, "VAGUES %d A %d", b * VG_BLOCK + 1, b * VG_BLOCK + VG_BLOCK);
    text_c(2, cx, cy - 230, buf, 0.5f, 0.9f, 1.0f, 1);
    text_c(0, cx, cy - 196, "choisis la suite : une carte simple, une carte difficile, ou un challenge", 0.75f, 0.78f, 0.88f, 1);
    float w = fminf(310.0f, (winW - 80.0f) / 3 - 16), h = 250, x0 = cx - (3 * w + 32) * 0.5f, y = cy - 160;
    static const char *kinds[3] = { "VAGUES SIMPLES", "VAGUES DIFFICILES", "CHALLENGE" };
    static const float kc[3][3] = { { 0.4f, 0.85f, 1 }, { 1, 0.45f, 0.35f }, { 1, 0.85f, 0.3f } };
    for (int k = 0; k < 3; k++) {
        float x = x0 + k * (w + 16);
        int hv = uiMX >= x && uiMX < x + w && uiMY >= y && uiMY < y + h;
        rect(x, y, w, h, hv ? 0.14f : 0.06f, hv ? 0.2f : 0.07f, hv ? 0.3f : 0.12f, 0.95f);
        rect(x, y, w, 4, kc[k][0], kc[k][1], kc[k][2], 1);
        text_c(1, x + w * 0.5f, y + 40, kinds[k], kc[k][0], kc[k][1], kc[k][2], 1);
        const char *l1 = "", *l2 = "", *l3 = "";
        char r1[64], r2[64];
        if (k == 0) { l1 = maps[vgChoiceMap[0]].name; l2 = "carte fermee"; l3 = "credits x1"; }
        else if (k == 1) { l1 = maps[vgChoiceMap[1]].name; l2 = "du vide : attention aux chutes"; l3 = "credits x1,5"; }
        else {
            snprintf(r1, sizeof r1, vgChoiceChal == CH_COURSE ? "COURSE : %s" : "DEFENSE : %s", maps[vgChoiceMap[2]].name);
            l1 = r1;
            l2 = vgChoiceChal == CH_COURSE ? "les portes en moins de 75 s" : "tiens la zone 40 s en 100 s";
            snprintf(r2, sizeof r2, "+%d credits si reussi", vgChoiceReward);
            l3 = r2;
        }
        text_c(1, x + w * 0.5f, y + 100, l1, 1, 1, 1, 1);
        text_c(0, x + w * 0.5f, y + 136, l2, 0.75f, 0.78f, 0.88f, 1);
        text_c(0, x + w * 0.5f, y + 162, l3, 1, 0.85f, 0.35f, 1);
        if (k == 2) {
            snprintf(r2, sizeof r2, "puis les vagues sur %s", maps[vgChoiceMap[0]].name);
            text_c(0, x + w * 0.5f, y + 188, r2, 0.6f, 0.65f, 0.75f, 1);
        }
        snprintf(buf, sizeof buf, "[ %d ]", k + 1);
        text_c(1, x + w * 0.5f, y + h - 22, buf, hv ? 1.0f : 0.5f, hv ? 1.0f : 0.5f, hv ? 1.0f : 0.55f, 1);
        if (hv && uiClick) { uiClick = 0; vgPend = k; vg_menu_close(); play(S_HIT, 0.5f, 1.4f); return; }
    }
    snprintf(buf, sizeof buf, "BOUTIQUE  (%d CREDITS)", vgCredits);
    if (button(cx - 180, y + h + 30, 360, 52, buf)) { vgShopBack = VGM_SELECT; vgMenu = VGM_SHOP; }
    if (uiClick) uiClick = 0;
}

/* vague 100 terminee : temps et score, puis l'INFINI */
static void vg_draw_win(float cx, float cy) {
    char buf[128], tb[24];
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.8f);
    text_c(2, cx, cy - 220, "VAGUE 100 TERMINEE", 1, 0.85f, 0.3f, 1);
    vg_time_text(tb, sizeof tb, vgT100);
    snprintf(buf, sizeof buf, "TEMPS %s", tb);
    text_c(2, cx, cy - 150, buf, 1, 1, 1, 1);
    snprintf(buf, sizeof buf, "score %d   -   %d kills   -   %d boss   -   record %d", score, kills, vgKillsBoss, bestScore);
    text_c(1, cx, cy - 100, buf, 0.85f, 0.88f, 0.95f, 1);
    int sp[4];
    vagues_splits_ms(sp);
    char s1[24], s2[24], s3[24];
    vg_time_text(s1, sizeof s1, sp[0] / 1000.0f); vg_time_text(s2, sizeof s2, sp[1] / 1000.0f); vg_time_text(s3, sizeof s3, sp[2] / 1000.0f);
    snprintf(buf, sizeof buf, "vague 25 : %s    vague 50 : %s    vague 75 : %s", s1, s2, s3);
    text_c(0, cx, cy - 66, buf, 0.65f, 0.7f, 0.8f, 1);
    float col[3];
    sr_result_line(SR_VAGUES100, buf, sizeof buf, col);   /* classement en ligne "vague 100 la plus rapide" */
    if (buf[0]) text_c(0, cx, cy - 43, buf, col[0], col[1], col[2], 1);
    text_c(0, cx, cy - 20, "INFINI : les vagues arrivent au bout du chrono, les boss attendent, et ils ont une attaque secrete",
           0.8f, 0.82f, 0.9f, 1);
    if (button(cx - 210, cy + 10, 420, 56, "LANCER L'INFINI")) vg_go_infinite();
    if (button(cx - 210, cy + 80, 420, 52, "RECOMMENCER")) { vg_menu_close(); vg_restart(); }
    if (button(cx - 210, cy + 146, 420, 52, "MENU PRINCIPAL")) { vg_menu_set(VGM_NONE); pendingAction = ACT_LEAVE; }
    if (uiClick) uiClick = 0;
}

/* ecran de mort (remplace "T'ES MORT") */
static void vg_draw_death(float cx, float cy) {
    char buf[128], tb[24];
    rect(0, 0, (float)winW, (float)winH, 0.15f, 0, 0, 0.45f);
    snprintf(buf, sizeof buf, vgInfinite ? "TOMBE A LA VAGUE %d (INFINI)" : "TOMBE A LA VAGUE %d", wave);
    text_c(2, cx, cy - 60, buf, 1, 0.3f, 0.3f, 1);
    vg_time_text(tb, sizeof tb, vgRunT);
    snprintf(buf, sizeof buf, "score %d   -   %d kills   -   temps %s", score, kills, tb);
    text_c(1, cx, cy - 6, buf, 1, 1, 1, 1);
    snprintf(buf, sizeof buf, "record %d", bestScore);
    text_c(0, cx, cy + 28, buf, 0.8f, 0.8f, 0.9f, 1);
    if (P.deathT > 1.0f) text_c(1, cx, cy + 84, "[R] ou clic pour recommencer a la vague 1", 0.9f, 0.9f, 0.9f, 1);
}

/* tout le HUD du mode VAGUES qui passe par-dessus le jeu (appele par draw_hud) */
static void vg_draw_hud(float cx, float cy) {
    char buf[128];
    if (!vagOn) return;
    if (vgWantResume) { vgWantResume = 0; if (!pendingAction) pendingAction = ACT_RESUME; }   /* apres un ecran ferme */
    vg_draw_boss_bar(cx);
    if (vgState == VG_BREAK && !P.dead && vgMenu == VGM_NONE && wave > 0) {
        snprintf(buf, sizeof buf, "[%s] BOUTIQUE  (%d credits)   -   vague suivante dans %.0f s", bind_text(A_SCORES), vgCredits,
                 ceilf(fmaxf(0, waveDelay)));
        text_c(1, cx, winH - 150.0f, buf, 1, 0.85f, 0.35f, 1);
    }
    if (vgState == VG_CHALLENGE && !P.dead) {
        if (vgChal == CH_COURSE && raceCount <= 0 && !raceDone) {
            snprintf(buf, sizeof buf, "CHALLENGE : %.0f s restantes", ceilf(fmaxf(0, VG_RACE_LIMIT - raceT)));
            text_c(1, cx, 130, buf, raceT > VG_RACE_LIMIT - 15 ? 1.0f : 1.0f, raceT > VG_RACE_LIMIT - 15 ? 0.4f : 0.85f, 0.35f, 1);
        } else if (vgChal == CH_DEFENSE) {
            float w = 360, x = cx - w * 0.5f, f = clampf(vgDefP / VG_DEF_HOLD, 0, 1);
            snprintf(buf, sizeof buf, "TIENS LA ZONE  %.0f / %.0f s   (reste %.0f s)", vgDefP, VG_DEF_HOLD, fmaxf(0, VG_DEF_LIMIT - vgChalT));
            text_c(1, cx, 118, buf, vg_in_zone() ? 0.4f : 1.0f, 1, vg_in_zone() ? 0.5f : 0.4f, 1);
            rect(x, 128, w, 8, 0.1f, 0.1f, 0.15f, 0.8f);
            rect(x, 128, w * f, 8, 0.3f, 1, 0.5f, 0.95f);
        }
    }
    if (vgMenu == VGM_SHOP) vg_draw_shop(cx, cy);
    else if (vgMenu == VGM_SELECT) vg_draw_select(cx, cy);
    else if (vgMenu == VGM_WIN) vg_draw_win(cx, cy);
}
