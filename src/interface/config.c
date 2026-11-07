/* VELOCITY - velocity.ini : reglages et touches du joueur
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Interface : menus, tableau des scores, joueurs distants             */
/* ------------------------------------------------------------------ */
enum { ACT_NONE, ACT_SOLO, ACT_HOST, ACT_BROWSE, ACT_QUIT, ACT_BACK, ACT_JOIN_FOUND, ACT_JOIN_IP, ACT_RESUME,
       ACT_RESTART, ACT_LEAVE, ACT_FULLSCREEN, ACT_QUICK, ACT_SPEC_FOUND, ACT_VSBOT, ACT_RACE, ACT_RANKED,
       ACT_SPEEDRUN };
static int settingsOpen, waitAct = -1, waitSlot, uiDown, uiDrag;
static int settingsPage;                                  /* ecran PARAMETRES : 0 = reglages et commandes, 1 = GRAPHISMES */
static int fullscreen;
static char cfgPath[MAX_PATH];

static void key_name(int code, char *out, int n) {
    static const char *mouseNames[] = { "Souris G", "Souris D", "Clic molette", "Souris 4", "Souris 5", "Molette" };
    char buf[64];
    if (code <= 0) { snprintf(out, n, "-"); return; }
    if (code >= K_LMB) { snprintf(out, n, "%s", mouseNames[code - K_LMB]); return; }
#ifdef _WIN32
    if (GetKeyNameTextA((LONG)code << 16, buf, sizeof buf) > 0) snprintf(out, n, "%s", buf);   /* nom selon le clavier */
    else snprintf(out, n, "touche %d", code);
#else
    mac_key_name(code, buf, sizeof buf);
    snprintf(out, n, "%s", buf);
#endif
}

static const char *bind_text(int a) {                    /* "Z / Fleche haut" : pour l'aide et le HUD */
    static char out[4][64];
    static int slot;
    char k0[32], k1[32];
    slot = (slot + 1) % 4;
    key_name(binds[a][0], k0, sizeof k0);
    key_name(binds[a][1], k1, sizeof k1);
    if (binds[a][1]) snprintf(out[slot], sizeof out[slot], "%s / %s", k0, k1);
    else snprintf(out[slot], sizeof out[slot], "%s", k0);
    return out[slot];
}

static void cfg_save(void) {
    if (botMode || shotMode || !cfgPath[0]) return;
    FILE *f = fopen(cfgPath, "w");
    if (!f) return;
    fprintf(f, "# VELOCITY - reglages (modifiable a la main)" "\n");
    fprintf(f, "name=%s" "\n" "sens=%.5f" "\n" "fov=%.0f" "\n" "volume=%.2f" "\n" "music=%.2f" "\n" "invert_y=%d" "\n" "show_fps=%d" "\n" "fullscreen=%d" "\n",
            myName, sens, fovBase, masterVol, musicVol, invertY, showFps, fullscreen);
    fprintf(f, "impact_frames=%d" "\n" "screen_fx=%d" "\n" "shadows=%d" "\n" "bloom=%d" "\n", impactFrames, screenFx, gfxShadows, gfxBloom);
    fprintf(f, "second_ecran=%d" "\n" "carte_vue=%d" "\n" "camera_mort=%d" "\n", secondScreen, mapView3d, killcamOn);
    fprintf(f, "quality=%d" "\n" "ambiance=%d" "\n", gfxQuality, gfxAmbiance);
    fprintf(f, "online=%d" "\n" "teams=%d" "\n" "update_check=%d" "\n", onlineOn, wantTeams, updCheck);
    if (sunDmg > 0) fprintf(f, "soleil_degats=%.0f" "\n", sunDmg);   /* avant skin : le skin POUSSIN depend de ce cumul */
    fprintf(f, "loadout=%d,%d" "\n" "melee=%d" "\n" "skill=%d" "\n" "skin=%d" "\n", wantLoadout[0], wantLoadout[1], wantMelee, wantSkill, wantSkin);
    fprintf(f, "map=%d" "\n" "map_rotate=%d" "\n", menuMap, mapRotate);
    fprintf(f, "regles=%d,%d,%d,%d,%d" "\n", wantRules.mode, wantRules.hook, wantRules.ammo, wantRules.shop, wantRules.minutes);
    if (lobbyIni[0] && strcmp(lobbyIni, LOBBY_DEFAULT)) fprintf(f, "lobby=%s" "\n", lobbyIni);   /* pas --lobby ; le defaut peut changer */
    if (hex_ok(rkId, 32) && hex_ok(rkKey, 32))           /* mode classe : identite, puis cache de la cote (le serveur fait foi) */
        fprintf(f, "ranked_id=%s" "\n" "ranked_key=%s" "\n", rkId, rkKey);
    if (rkDiscord[0]) fprintf(f, "discord=%s" "\n", rkDiscord);   /* compte Discord lie (affichage seulement) */
    fprintf(f, "mmr=%d" "\n" "ranked_played=%d" "\n" "ranked_wins=%d" "\n", myMmr, rankPlayed, rankWins);
    for (int a = 0; a < A_COUNT; a++) fprintf(f, "bind_%s=%d,%d" "\n", actId[a], binds[a][0], binds[a][1]);
    fclose(f);
}

static int cfg_load(void) {                               /* renvoie 1 si le plein ecran est demande */
    char line[256];
    int fs = 0;
    FILE *f = cfgPath[0] ? fopen(cfgPath, "r") : NULL;
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        char *eq = strchr(line, '='), *v;
        if (!eq || line[0] == '#') continue;
        *eq = 0; v = eq + 1;
        v[strcspn(v, "\r" "\n")] = 0;
        if (!strcmp(line, "name") && v[0]) snprintf(myName, sizeof myName, "%s", v);
        else if (!strcmp(line, "sens")) sens = clampf((float)atof(v), 0.0003f, 0.01f);
        else if (!strcmp(line, "fov")) fovBase = clampf((float)atof(v), 80, 130);
        else if (!strcmp(line, "volume")) masterVol = clampf((float)atof(v), 0, 1);
        else if (!strcmp(line, "music")) musicVol = clampf((float)atof(v), 0, 1);
        else if (!strcmp(line, "invert_y")) invertY = atoi(v) != 0;
        else if (!strcmp(line, "show_fps")) showFps = atoi(v) != 0;
        else if (!strcmp(line, "impact_frames")) impactFrames = atoi(v) != 0;
        else if (!strcmp(line, "screen_fx")) screenFx = atoi(v) != 0;
        else if (!strcmp(line, "second_ecran")) secondScreen = atoi(v) != 0;
        else if (!strcmp(line, "carte_vue")) mapView3d = atoi(v) != 0;
        else if (!strcmp(line, "camera_mort")) killcamOn = atoi(v) != 0;
        else if (!strcmp(line, "shadows")) gfxShadows = atoi(v) != 0;
        else if (!strcmp(line, "bloom")) gfxBloom = atoi(v) != 0;
        else if (!strcmp(line, "quality")) gfxQuality = atoi(v) < 0 ? 0 : atoi(v) > 2 ? 2 : atoi(v);
        else if (!strcmp(line, "ambiance")) gfxAmbiance = atoi(v) < 0 ? 0 : atoi(v) > 3 ? 3 : atoi(v);
        else if (!strcmp(line, "online")) onlineOn = atoi(v) != 0;
        else if (!strcmp(line, "teams")) wantTeams = atoi(v) < 0 ? 0 : atoi(v) > TEAM_MAX ? TEAM_MAX : atoi(v);
        else if (!strcmp(line, "update_check")) updCheck = atoi(v) != 0;
        else if (!strcmp(line, "lobby") && v[0]) { snprintf(lobbyAddr, sizeof lobbyAddr, "%s", v); snprintf(lobbyIni, sizeof lobbyIni, "%s", v); }
        else if (!strcmp(line, "ranked_id") && hex_ok(v, 32)) snprintf(rkId, sizeof rkId, "%s", v);
        else if (!strcmp(line, "ranked_key") && hex_ok(v, 32)) snprintf(rkKey, sizeof rkKey, "%s", v);
        else if (!strcmp(line, "discord")) dc_name_set(v);
        else if (!strcmp(line, "mmr")) myMmr = rank_clamp(atoi(v));
        else if (!strcmp(line, "ranked_played")) rankPlayed = atoi(v) < 0 ? 0 : atoi(v);
        else if (!strcmp(line, "ranked_wins")) rankWins = atoi(v) < 0 ? 0 : atoi(v);
        else if (!strcmp(line, "fullscreen")) fs = atoi(v) != 0;
        else if (!strcmp(line, "loadout")) sscanf(v, "%d,%d", &wantLoadout[0], &wantLoadout[1]);
        else if (!strcmp(line, "melee")) wantMelee = atoi(v);
        else if (!strcmp(line, "soleil_degats")) sunDmg = fmaxf(0, (float)atof(v));
        else if (!strcmp(line, "skin")) wantSkin = atoi(v) < 0 || atoi(v) >= SKN_COUNT || !skin_unlocked(atoi(v)) ? 0 : atoi(v);
        else if (!strcmp(line, "map")) menuMap = map_pvp(atoi(v)) ? atoi(v) : 0;   /* jamais une carte CHALLENGE */
        else if (!strcmp(line, "map_rotate")) mapRotate = atoi(v) != 0;
        else if (!strcmp(line, "regles")) {
            MatchRules r = wantRules;
            int x[6], n = sscanf(v, "%d,%d,%d,%d,%d,%d", &x[0], &x[1], &x[2], &x[3], &x[4], &x[5]);
            if (n >= 5) { r.mode = x[0]; r.hook = x[1]; r.ammo = x[2]; r.shop = x[3]; r.minutes = x[n - 1]; }
            if ((n == 5 || n == 6) &&                     /* 6 champs : ancien format, le 5e (specialisations) est ignore */
                r.mode >= 0 && r.mode < MODE_COUNT && r.ammo >= 0 && r.ammo < AMMO_COUNT && r.minutes > 0 && r.minutes <= 60) wantRules = r;
        }
        else if (!strcmp(line, "skill")) wantSkill = atoi(v) >= 0 && atoi(v) < SK_COUNT ? atoi(v) : SK_COIN;
        else if (!strncmp(line, "bind_", 5))
            for (int a = 0; a < A_COUNT; a++)
                if (!strcmp(line + 5, actId[a])) {
                    int b0 = 0, b1 = 0;
                    if (sscanf(v, "%d,%d", &b0, &b1) >= 1) {
                        binds[a][0] = (b0 >= 0 && b0 < K_COUNT) ? b0 : 0;
                        binds[a][1] = (b1 >= 0 && b1 < K_COUNT) ? b1 : 0;
                    }
                }
    }
    fclose(f);
    if (!loadout_valid(wantLoadout, wantMelee)) { wantLoadout[0] = WP_RAIL; wantLoadout[1] = WP_ROCKET; wantMelee = MK_SWORD; }
    return fs;
}

static void assign_bind(int code) {
    if (waitAct < 0) return;
    if (code > 0)                                         /* une touche ne sert qu'a une seule commande */
        for (int a = 0; a < A_COUNT; a++)
            for (int k = 0; k < 2; k++)
                if (binds[a][k] == code) binds[a][k] = 0;
    binds[waitAct][waitSlot] = code;
    waitAct = -1;
    play(S_HIT, 0.4f, 1.5f);
    cfg_save();
}
static int uiMX, uiMY, uiClick, uiFocus, pendingAction, pendingArg;   /* uiFocus : 1 = pseudo, 2 = IP */
static char ipField[32];

static const float pcol[MAXP][3] = {
    { 0.3f, 0.8f, 1.0f }, { 1.0f, 0.45f, 0.3f }, { 0.5f, 1.0f, 0.4f }, { 1.0f, 0.85f, 0.3f },
    { 0.8f, 0.45f, 1.0f }, { 1.0f, 0.4f, 0.7f }, { 0.4f, 1.0f, 0.9f }, { 0.9f, 0.9f, 0.9f },
};
/* couleur d'un joueur : la sienne en chacun pour soi ; en equipes, celle de l'equipe, eclaircie selon le rang */
static const float *pcolor(int i) {
    static float c[MAXP + 1][3];
    if (i < 0 || i > MAXP) i = MAXP;
    if (teamSize <= 0 || i >= MAXP) return pcol[i < MAXP ? i : MAXP - 1];
    int t = NP[i].team & 1, rank = 0;
    for (int j = 0; j < i; j++) rank += NP[j].active && NP[j].team == t;
    float k = 0.18f * rank;
    for (int a = 0; a < 3; a++) c[i][a] = teamCol[t][a] + (1 - teamCol[t][a]) * k * 0.6f;
    return c[i];
}
static const char *weaponNames[W_COUNT] = { "RAIL", "ROQUETTE", "EPEE", "GRAPPIN", "PARRY", "SOLO", "RICOCHET",
                                            "ENCLUME", "LANCE", "YOYO", "POINGS", "EGIDE", "SHOTGUN", "ELAN",
                                            "SCIE", "VENTOUSE", "TESLA", "VIDE" };
