/* VELOCITY - point d'entree (WinMain / velocity_main) et boucle principale
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* Intensite de la musique : PvP = kills du leader (un round = un kill), solo = numero de vague.
 * Hors partie : niveau 0. Chaque joueur le calcule seul : les frags de tous sont deja dans NP[]. */
static void music_follow_game(void) {
    int lvl = 0;
    if (appState != APP_GAME) { if (mus.track >= 0) music_set(-1, 0); return; }   /* menus : silence */
    if (mus.track < 0) {                                  /* entree en partie : depart avec fondu, variations differentes a chaque partie */
        musSeed = (unsigned)(nowT * 1000.0) * 2654435761u + 1u;
        music_set(maps[curMap].music < NTRACKS ? maps[curMap].music : 0, 0);
    }
    if (appState == APP_GAME && dmMode) {
        int lead = 0, second = 0;
        for (int i = 0; i < MAXP; i++) {
            if (!NP[i].active) continue;
            if (NP[i].kills > lead) { second = lead; lead = NP[i].kills; }
            else if (NP[i].kills > second) second = NP[i].kills;
        }
        if (teamSize > 0) {                               /* equipes : les deux scores d'equipe */
            int a = team_score(0), b = team_score(1);
            lead = a > b ? a : b; second = a > b ? b : a;
        }
        lvl = music_pvp_level(lead, second, fragLimit);
    } else if (appState == APP_GAME && raceOn) {         /* course : la musique monte avec les portes */
        lvl = raceCount > 0 ? 1 : 2 + raceGate * 6 / (maps[curMap].ngates > 0 ? maps[curMap].ngates : 1);
    } else if (appState == APP_GAME) {
        lvl = vagOn ? vg_music_level() : wave - 1;
    }
    if (appState == APP_GAME) lvl += styleRank >= ST_S ? 2 : styleRank >= ST_A ? 1 : 0;   /* le style pousse la musique */
    music_level(lvl);
}

static void do_action(void) {
    uiFocus = 0;                                          /* une action quitte toujours la saisie de texte */
    int a = pendingAction;
    pendingAction = ACT_NONE;
    if (!myName[0]) snprintf(myName, sizeof myName, "joueur");
    if (a == ACT_SOLO || a == ACT_HOST || a == ACT_BROWSE || a == ACT_QUICK || a == ACT_VSBOT || a == ACT_RANKED) cfg_save();
    int wasRace = raceOn && !vagOn;                       /* le challenge COURSE des VAGUES ne ramene pas a l'ecran CHALLENGE */
    if (a != ACT_NONE && a != ACT_RESUME && a != ACT_RESTART && a != ACT_FULLSCREEN) { raceOn = srFull = 0; vagOn = 0; vgMenu = VGM_NONE; }   /* toute autre action quitte la course et les VAGUES */
    switch (a) {
    case ACT_RACE:                                        /* course au grappin d'une carte CHALLENGE (pendingArg : la carte) */
        net_shutdown();
        raceMap = pendingArg; challengeOpen = 0;
        race_start(); appState = APP_GAME; set_capture(1);
        break;
    case ACT_SPEEDRUN:                                    /* speedrun complet : toutes les cartes CHALLENGE a la suite */
        net_shutdown();
        challengeOpen = 0;
        sr_full_start(); appState = APP_GAME; set_capture(1);
        break;
    case ACT_SOLO:                                        /* mode VAGUES */
        net_shutdown();
        vg_start(); appState = APP_GAME; set_capture(1);
        break;
    case ACT_HOST:
        if (net_host_start()) { appState = APP_GAME; if (!botMode) set_capture(1); }
        hostQuick = 0; hostRanked = 0;
        break;
    case ACT_BROWSE:
        if (net_browse_start()) { appState = APP_JOIN; netStatus[0] = 0; }
        break;
    case ACT_VSBOT: {                                     /* heberge en local, puis lance les bots (autres processus) qui nous rejoignent */
        hostRanked = 0;                                   /* une partie contre des bots n'est jamais classee */
        if (!net_host_start()) break;
        if (mode_team(rules.mode)) { appState = APP_GAME; set_capture(1); netlog("pas de bots en %s", modeNames[rules.mode]); break; }   /* modes a objectif : sans bots */
        int lv = pendingArg < 0 ? 0 : pendingArg > 2 ? 2 : pendingArg;
        int nBots = teamSize > 0 ? host_max_players() - 1 : 1, ok = 0;   /* equipes : toutes les places libres */
        for (int k = 0; k < nBots && k < MAXP; k++) {
            char nm[48];
            if (nBots > 1) snprintf(nm, sizeof nm, "Bot%d_%s", k + 1, botLevelNames[lv]);
            else snprintf(nm, sizeof nm, "Bot_%s", botLevelNames[lv]);
#ifndef _WIN32
            char exe[MAX_PATH], lvs[8];
            uint32_t exeLen = sizeof exe;
            snprintf(lvs, sizeof lvs, "%d", lv);
            char *argv[] = { exe, "--bot", "join", "--local", "--hidden", "--nolan", "--level", lvs, "--name", nm,
                             "--time", "1000000", NULL };
            extern char **environ;
            if (_NSGetExecutablePath(exe, &exeLen) == 0 && posix_spawn(&botProc[k], exe, NULL, NULL, argv, environ) == 0) ok++;
            else botProc[k] = 0;
#else
            char exe[MAX_PATH], cmd[MAX_PATH + 160];
            STARTUPINFOA si = { sizeof si };
            PROCESS_INFORMATION pi;
            GetModuleFileNameA(NULL, exe, sizeof exe);
            snprintf(cmd, sizeof cmd, "\"%s\" --bot join --local --hidden --nolan --level %d --name %s --time 1000000", exe, lv, nm);
            if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hThread);
                botProc[k] = pi.hProcess; ok++;
            }
#endif
        }
        if (ok) { vsBot = ok; botLevel = lv; appState = APP_GAME; set_capture(1); }   /* vsBot : nombre de bots lances */
        else { set_status("impossible de lancer le bot"); net_shutdown(); }
        break;
    }
    case ACT_JOIN_FOUND:
        join_found(pendingArg, 0);
        break;
    case ACT_SPEC_FOUND:
        join_found(pendingArg, 1);
        break;
    case ACT_QUICK:
        quick_play();
        break;
    case ACT_RANKED:
        ranked_play();
        break;
    case ACT_JOIN_IP: {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof addr);
        addr.sin_family = AF_INET; addr.sin_port = htons(NET_PORT);
        addr.sin_addr.s_addr = inet_addr(ipField);
        if (!ipField[0] || addr.sin_addr.s_addr == INADDR_NONE) set_status("adresse IP invalide");
        else net_connect(addr);
        break;
    }
    case ACT_BACK:
        net_shutdown(); appState = APP_MENU;
        break;
    case ACT_RESUME:
        set_capture(1);
        break;
    case ACT_RESTART:
        if (vagOn) vg_restart(); else if (raceOn) race_start(); else reset_game();
        set_capture(1);
        break;
    case ACT_LEAVE:
        net_shutdown(); set_capture(0); appState = APP_MENU; menu_backdrop();
        if (wasRace) { challengeOpen = 1; rq_sr_top_all(); }   /* retour d'une course : l'ecran CHALLENGE et ses classements */
        break;
    case ACT_QUIT:
        PostQuitMessage(0);
        break;
    case ACT_FULLSCREEN:
        toggle_fullscreen();
        cfg_save();
        break;
    }
}

/* --shot bench_dm.bmp : temps de rendu moyen (ms) par reglage graphique, ecrit dans bench_dm.bmp.txt */
static void bench_render(const char *path) {
    char out[300];
    snprintf(out, sizeof out, "%s.txt", path);
    FILE *f = fopen(out, "w");
    if (!f) return;
    static const char *names[4] = { "tout", "sans bloom", "sans ombres", "sans les deux" };
    for (int cfg = 0; cfg < 4; cfg++) {
        gfxBloom = !(cfg & 1); gfxShadows = !(cfg & 2);
        LARGE_INTEGER fq, t0, t1;
        QueryPerformanceFrequency(&fq);
        for (int i = 0; i < 20; i++) { render(1, TICK); glFinish(); }
        QueryPerformanceCounter(&t0);
        for (int i = 0; i < 200; i++) { P.yaw += 0.01f; render(1, TICK); glFinish(); }
        QueryPerformanceCounter(&t1);
        fprintf(f, "%-14s %.2f ms/image (%dx%d)\n", names[cfg], (double)(t1.QuadPart - t0.QuadPart) * 1000.0 / fq.QuadPart / 200, winW, winH);
    }
    benchOn = 1; gfxBloom = gfxShadows = 1;
    for (int i = 0; i < 100; i++) render(1, TICK);
    static const char *sec[11] = { "", "debut/camera", "ciel", "ville", "monde", "ombres", "entites+additif", "arme", "bloom", "impact", "hud" };
    for (int k = 1; k <= 10; k++) fprintf(f, "  %-16s %.2f ms\n", sec[k], benchT[k] / 100);
    fprintf(f, "precalcul des lightmaps : %.0f ms\n", lmBakeMs);
    fclose(f);
}

/* Une image de la boucle principale : reseau, bots, simulation, rendu, son. Appelee par la boucle de velocity_main,
   et aussi pendant les boucles modales du systeme (fenetre deplacee ou redimensionnee, menu de fenetre) : la partie
   ne s'arrete jamais, en ligne l'hote couperait les joueurs au bout de NET_TIMEOUT. 0 = fin (bots). */
static LARGE_INTEGER frmFreq, frmStart, frmLast;
static float frmAcc, frmFpsAcc;
static int frmFpsFrames, frmBusy;
static int main_frame(void) {
    LARGE_INTEGER frmNow;
    QueryPerformanceCounter(&frmNow);
    float dt = (float)(frmNow.QuadPart - frmLast.QuadPart) / (float)frmFreq.QuadPart;
    nowT = (double)(frmNow.QuadPart - frmStart.QuadPart) / (double)frmFreq.QuadPart;
    frmLast = frmNow;
    if (dt > 0.1f) dt = 0.1f;
    frmFpsAcc += dt; frmFpsFrames++;
    if (frmFpsAcc > 0.5f) { fps = frmFpsFrames / frmFpsAcc; frmFpsAcc = 0; frmFpsFrames = 0; }

    net_update(dt);
    if (botMode) {
        bot_think(dt);
        bot_void_guard(dt);                           /* kill zone : pas de saut dans le vide, grappin de secours */
        if (nowT > botTime) return 0;
        if (botLocal && appState != APP_GAME && botT > 12) return 0;   /* l'hote est parti : le bot adverse se ferme */
        if (appState == APP_GAME) aim_assist(dt);     /* AIMANT du bot : tire sa visee imprecise vers la cible */
    }
    if (appState == APP_GAME) {
        if (captured) {
            if (!P.dead) {
                P.yaw += mdx * sens;
                P.pitch = clampf(P.pitch - mdy * sens * (invertY ? -1.0f : 1.0f), -1.55f, 1.55f);
                P.swayX = clampf(P.swayX - mdx * 0.00004f, -0.04f, 0.04f);
                P.swayY = clampf(P.swayY + mdy * 0.00004f, -0.04f, 0.04f);
                if (!botMode) aim_assist(dt);             /* AIMANT (les bots l'appliquent apres bot_think) */
            }
            P.swayX *= expf(-dt * 10); P.swayY *= expf(-dt * 10);
        }
        mdx = mdy = 0;
        if (captured || dmMode) {             /* en LAN la partie ne se met jamais en pause */
            frmAcc += dt;
            while (frmAcc >= TICK) { game_tick(TICK); frmAcc -= TICK; }
        } else {
            memset(latch, 0, sizeof latch);
        }
    } else {
        P.yaw += dt * 0.05f;                  /* l'arene tourne doucement derriere le menu */
        mdx = mdy = 0;
        memset(latch, 0, sizeof latch);
    }
    if (botHidden) {                                  /* bot cache : personne ne le regarde, pas de rendu ; */
#ifdef _WIN32                                             /* boucle limitee (~250 Hz) pour laisser la machine au joueur */
        Sleep(4);
#else
        usleep(4000);
#endif
    } else {
        render((captured || dmMode) && appState == APP_GAME ? frmAcc / TICK : 1.0f, dt);
        uiClick = 0;                                  /* apres le rendu : les boutons (mode immediat) ont vu le clic */
#ifdef _WIN32
        SwapBuffers(hdc);
#else
        mac_swap();
#endif
        second_screen_frame(dt);                      /* carte sur le deuxieme ecran, s'il y en a un */
    }
    tesla_audio();                                    /* Tesla : boucles sonores des arcs */
    music_follow_game();
    rk_net_poll();                                    /* mode classe : requetes au serveur de classement */
    audio_update();
    if (lbWantHost) { lbWantHost = 0; if (appState == APP_JOIN) { pendingAction = ACT_HOST; hostQuick = 1; hostRanked = rankedWant; } }
    if (pendingAction) do_action();
    return 1;
}

/* boucle modale du systeme (minuteur Windows, NSTimer du suivi d'evenements sur macOS) : une image si on n'y est pas deja */
static void modal_frame(void) {
    if (frmBusy || !frmFreq.QuadPart) return;
    frmBusy = 1;
    if (!main_frame()) PostQuitMessage(0);
    frmBusy = 0;
}
#ifndef _WIN32
void plat_modal_frame(void) { modal_frame(); }
#endif

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    (void)prev;
#else
static int velocity_main(const char *cmd) {
#endif
    memcpy(binds, bindDefault, sizeof binds);
    if (cmd && strncmp(cmd, "--test ", 7) == 0) { run_tests(cmd + 7); return 0; }
    if (cmd && strncmp(cmd, "--export-textures ", 18) == 0) {   /* --export-textures dossier : textures generees -> .bmp, sans fenetre */
        char dir[512];
        const char *q = cmd + 18;
        while (*q == ' ' || *q == '"') q++;
        snprintf(dir, sizeof dir, "%s", q);               /* tout le reste : le dossier peut contenir des espaces */
        for (size_t n = strlen(dir); n > 0 && (dir[n - 1] == ' ' || dir[n - 1] == '"' || dir[n - 1] == '/' || dir[n - 1] == '\\'); n--)
            dir[n - 1] = 0;
        if (!dir[0]) return 1;
#ifdef _WIN32
        CreateDirectoryA(dir, NULL);                      /* deja present : sans effet */
#else
        mkdir(dir, 0755);
#endif
        gfx_load_all();                                   /* reglages de graphismes/rendu.ini, comme le jeu */
        return export_textures(dir) == TEXG_N ? 0 : 1;
    }
    const char *shotPath = NULL;
    static char shotBuf[512];
    float shotYaw = 0, shotPitch = -1000;
    V3 shotPos = v3(0, 0, 0);
    int shotPosSet = 0;                                   /* --pos x,y,z[,inclinaison] : point de vue d'une capture */
    if (cmd && strncmp(cmd, "--shot ", 7) == 0) {
        shotMode = 1;
        snprintf(shotBuf, sizeof shotBuf, "%s", cmd + 7);
        shotBuf[strcspn(shotBuf, " ")] = 0;               /* le nom s'arrete au premier espace (--map N, --yaw A ensuite) */
        shotPath = shotBuf;
        shotMap = strstr(shotPath, "carte") != NULL;      /* carte*.bmp : le second ecran */
        if (strstr(shotPath, "carte3d")) mapView3d = 1;
    }

    /* mode bot : velocity.exe --bot host|join [--name N] [--time S] [--log fichier] [--botshot fichier.bmp] [--loadout a,b[,m]] */
    {
        static char args[1024];
        snprintf(args, sizeof args, "%s", cmd ? cmd : "");
#ifdef _WIN32
        DWORD un = sizeof myName;
        if (!GetUserNameA(myName, &un) || !myName[0]) snprintf(myName, sizeof myName, "joueur");
#else
        struct passwd *pw = getpwuid(getuid());
        snprintf(myName, sizeof myName, "%s", pw && pw->pw_name[0] ? pw->pw_name : "joueur");
#endif
        myName[15] = 0;
        for (char *t = strtok(args, " "); t; t = strtok(NULL, " ")) {
            if (!strcmp(t, "--bot") && (t = strtok(NULL, " "))) botMode = !strcmp(t, "host") ? 1 : 2;
            else if (!strcmp(t, "--name") && (t = strtok(NULL, " "))) snprintf(myName, sizeof myName, "%s", t);
            else if (!strcmp(t, "--time") && (t = strtok(NULL, " "))) botTime = (float)atof(t);
            else if (!strcmp(t, "--log") && (t = strtok(NULL, " "))) botLog = fopen(t, "w");
            else if (!strcmp(t, "--botshot") && (t = strtok(NULL, " "))) snprintf(botShot, sizeof botShot, "%s", t);
            else if (!strcmp(t, "--lobby") && (t = strtok(NULL, " "))) snprintf(lobbyAddr, sizeof lobbyAddr, "%s", t);
            else if (!strcmp(t, "--lanep")) lanEp = 1;
            else if (!strcmp(t, "--nolan")) lanOff = 1;
            else if (!strcmp(t, "--spec")) botSpec = 1;
            else if (!strcmp(t, "--ranked")) botRanked = 1;   /* mode classe : partie classee (identite temporaire) */
            else if (!strcmp(t, "--frags") && (t = strtok(NULL, " "))) rkFragsTest = atoi(t);
            else if (!strcmp(t, "--local")) botLocal = 1;
            else if (!strcmp(t, "--hidden")) botHidden = 1;
            else if (!strcmp(t, "--teams") && (t = strtok(NULL, " "))) wantTeams = atoi(t) < 0 ? 0 : atoi(t) > TEAM_MAX ? TEAM_MAX : atoi(t);
            else if (!strcmp(t, "--regles") && (t = strtok(NULL, " "))) {   /* --regles mode,grappin,munitions,boutique,minutes */
                MatchRules r = wantRules;
                if (sscanf(t, "%d,%d,%d,%d,%d", &r.mode, &r.hook, &r.ammo, &r.shop, &r.minutes) >= 1 && r.mode >= 0 && r.mode < MODE_COUNT &&
                    r.ammo >= 0 && r.ammo < AMMO_COUNT && r.minutes > 0 && r.minutes <= 60) wantRules = r;   /* memes bornes que velocity.ini */
            }
            else if (!strcmp(t, "--updated")) updAfter = 1;
            else if (!strcmp(t, "--level") && (t = strtok(NULL, " "))) botLevel = atoi(t) < 0 ? 0 : atoi(t) > 2 ? 2 : atoi(t);
            else if (!strcmp(t, "--map") && (t = strtok(NULL, " "))) menuMap = atoi(t) >= 0 && atoi(t) < NMAPS ? atoi(t) : 0;
            else if (!strcmp(t, "--yaw") && (t = strtok(NULL, " "))) shotYaw = (float)atof(t);
            else if (!strcmp(t, "--pos") && (t = strtok(NULL, " "))) shotPosSet = sscanf(t, "%f,%f,%f,%f", &shotPos.x, &shotPos.y, &shotPos.z, &shotPitch) >= 3;
            else if (!strcmp(t, "--loadout") && (t = strtok(NULL, " "))) {   /* --loadout arme1,arme2[,melee[,competence]] (WP_*, WP_*, MK_*, SK_*) */
                int l0, l1, mk, sk, n = sscanf(t, "%d,%d,%d,%d", &l0, &l1, &mk, &sk);
                if (n >= 2) { wantLoadout[0] = l0; wantLoadout[1] = l1; }
                if (n >= 3) wantMelee = mk;
                if (n == 4) { wantSkill = sk >= 0 && sk < SK_COUNT ? sk : SK_COIN; botSkillSet = 1; }
            }
            else if (!strcmp(t, "--skill") && (t = strtok(NULL, " "))) { wantSkill = atoi(t) >= 0 && atoi(t) < SK_COUNT ? atoi(t) : SK_COIN; botSkillSet = 1; }
        }
    }

    if (shotMode && strstr(shotPath, "small")) { winW = 1024; winH = 640; }   /* capture en fenetre reduite (mise en page) */
#ifndef _WIN32
    if (!mac_window_create(winW, winH, "VELOCITY", !botHidden)) return 1;
    if (botMode) mac_window_place(botMode == 1 ? 0 : 820, 40, 800, 450);   /* deux bots cote a cote, fenetres reduites */
    mac_window_size(&winW, &winH);
#else
    SetProcessDPIAware();
    WNDCLASSA wc = { 0 };
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "velocity";
    RegisterClassA(&wc);
    RECT r = { 0, 0, winW, winH };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    int ww = r.right - r.left, wh = r.bottom - r.top;
    hwnd = CreateWindowA("velocity", "VELOCITY", WS_OVERLAPPEDWINDOW,
                         (GetSystemMetrics(SM_CXSCREEN) - ww) / 2, (GetSystemMetrics(SM_CYSCREEN) - wh) / 2,
                         ww, wh, NULL, NULL, inst, NULL);
    hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = { sizeof pfd, 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                                  PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 24, 8, 0,
                                  PFD_MAIN_PLANE, 0, 0, 0, 0 };
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    HGLRC rc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, rc);
    typedef BOOL(WINAPI * SwapFn)(int);
    SwapFn swapInterval = (SwapFn)wglGetProcAddress("wglSwapIntervalEXT");
    if (swapInterval) swapInterval(1);

    if (botMode) {                        /* deux bots cote a cote, fenetres reduites */
        winW = 800; winH = 450;
        SetWindowPos(hwnd, NULL, botMode == 1 ? 0 : 820, 40, 816, 489, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    ShowWindow(hwnd, botHidden ? SW_SHOWMINNOACTIVE : (shotMode || botMode) ? SW_SHOWNOACTIVATE : show);
    UpdateWindow(hwnd);
    { RECT cr; GetClientRect(hwnd, &cr); winW = cr.right; winH = cr.bottom; }

    RAWINPUTDEVICE rid = { 0x01, 0x02, 0, hwnd };
    RegisterRawInputDevices(&rid, 1, sizeof rid);
#endif

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);                                      /* lumiere d'appoint froide, cf. render/draw_viewmodel */
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);                                   /* l'arme en main est mise a l'echelle : sinon le reflet explose */
    {                                                          /* leger reflet specular, decors et armes */
        GLfloat spec[4] = { 0.22f, 0.22f, 0.25f, 1 };
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec);
        glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 20);
        GLfloat noSpec[4] = { 0, 0, 0, 1 };
        glLightfv(GL_LIGHT1, GL_SPECULAR, noSpec);            /* le reflet ne vient que du soleil (LIGHT0) */
    }
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glHint(GL_FOG_HINT, GL_NICEST);
    make_font(0, 18); make_font(1, 28); make_font(2, 56);
    for (int i = 0; i < 3; i++) make_font_atlas(i);
    gfx_load_all();                                       /* graphismes/rendu.ini (sinon : valeurs par defaut) */
    make_textures();
    build_level(curMap);
    if (!botHidden || botShot[0]) build_world_list();       /* un bot cache ne dessine rien : pas de lightmaps */
    glReady = !botHidden || botShot[0];                   /* load_map : reconstruire la liste d'affichage */
    make_sounds();
    {                                                     /* velocity.ini, a cote de l'executable */
#ifdef _WIN32
        char *slash;
        GetModuleFileNameA(NULL, cfgPath, sizeof cfgPath);
        slash = strrchr(cfgPath, '\\');
        if (slash) strcpy(slash + 1, "velocity.ini"); else cfgPath[0] = 0;
#else                                                     /* le .app peut etre en lecture seule : reglages dans ~/Library */
        const char *home = getenv("HOME");
        if (home) {
            snprintf(cfgPath, sizeof cfgPath, "%s/Library/Application Support/Velocity", home);
            mkdir(cfgPath, 0755);
            snprintf(cfgPath, sizeof cfgPath, "%s/Library/Application Support/Velocity/velocity.ini", home);
        } else cfgPath[0] = 0;
#endif
        if (!botMode && !shotMode && cfg_load()) toggle_fullscreen();
        if (!botMode && !shotMode && (gfxAmbiance || gfxQuality != 2)) { gfx_reload(); popT = 0; }   /* ambiance / qualite du joueur */
    }
    if (rk_identity() && !botMode && !shotMode) cfg_save();   /* mode classe : identite creee au premier lancement */
    if (!shotMode && (!botMode || botRanked)) { rq_hello(); rq_top(); }   /* cote et TOP 3 en arriere-plan */
    if (!botMode && !shotMode) {                          /* mise a jour : restes de la precedente, puis verification discrete */
        upd_cleanup();
        if (updAfter) {
            char ub[64];
            snprintf(ub, sizeof ub, "mise a jour installee : build %d", VELOCITY_BUILD);
            set_status(ub);
        }
        if (updCheck) { updQuiet = 1; upd_start(upd_check_th, UPD_CHECKING); }
    }
    if (!shotMode && !botHidden) audio_init();
    if (curMap != menuMap) load_map(menuMap);             /* --map N ou map= de velocity.ini */
    if (botMode) {                                        /* Competences : sans --skill, le bot en tire une au hasard */
        if (!botSkillSet) wantSkill = (int)(frand() * SK_COUNT) % SK_COUNT;
        netlog("competence du bot : %s%s", skillDef[wantSkill].name, botSkillSet ? " (imposee)" : " (au hasard)");
    }
    reset_game();

    if (shotMode && vagues_shot(shotPath)) return 0;      /* mode VAGUES : vagues_pyramide, _boutique, _selecteur... */
    if (shotMode && (strstr(shotPath, "settings") || strstr(shotPath, "menu") || strstr(shotPath, "loadout") ||
                     strstr(shotPath, "heberger") || strstr(shotPath, "graphismes") ||
                     strstr(shotPath, "update") || strstr(shotPath, "challenge"))) {   /* "skinmenu" contient "menu" */   /* captures des menus */
        appState = APP_MENU;
        menu_backdrop();
        if (strstr(shotPath, "update")) {                 /* bouton de mise a jour en plein telechargement */
            updInfo.build = VELOCITY_BUILD + 5; updState = updSeen = UPD_DOWNLOADING; updTotal = 1000; updGot = 630;
            set_status("nouvelle version disponible : build 5");
        }
        if (strstr(shotPath, "settings")) { settingsOpen = 1; waitAct = A_SKILL; waitSlot = 1; }
        if (strstr(shotPath, "graphismes")) {             /* PARAMETRES > GRAPHISMES, aide du reglage QUALITE survole */
            settingsOpen = 1; settingsPage = 1;
            SetLay L = settings_lay(winW * 0.5f);
            uiMX = (int)(winW * 0.5f - 100); uiMY = (int)(L.y0 + (L.compact ? 16.0f : 22.0f) + 10);
        }
        if (strstr(shotPath, "loadout")) { loadoutOpen = 1; if (strstr(shotPath, "charge")) wantSkill = SK_CHARGE; }
        if (strstr(shotPath, "heberger")) { hostOpen = 1; wantTeams = 2; hostBots = 1; }
        if (strstr(shotPath, "skinmenu")) { sunDmg = SUN_UNLOCK_DMG; skinOpen = 1; wantSkin = SKN_POUSSIN; skinPrevTeam = 1; nowT = 0.4; }
        if (strstr(shotPath, "challenge")) {              /* ecran CHALLENGE : classement factice de la course */
            static const char *nm[8] = { "Vortex", "Kestrel", "Nyx", "Moi", "Orbit", "Pixel", "Zephyr", "Lumen" };
            static const int tm[8] = { 31870, 33420, 34050, 36210, 38990, 41300, 44720, 52080 };
            sr_maps_init();
            challengeOpen = 1; srSel = SR_MAP0 + srMaps[0];
            SrTop *t = &srTop[srSel];
            t->state = 2; t->n = 8; t->runners = 37; t->meRank = 4; t->meMs = tm[3];
            for (int i = 0; i < 8; i++) { snprintf(t->name[i], 16, "%s", nm[i]); t->ms[i] = tm[i]; }
            snprintf(t->wrName, 16, "%s", nm[0]); t->wrMs = tm[0];
            srTop[SR_COMPLET].state = 2; srTop[SR_COMPLET].meMs = 36900;
            srTop[SR_VAGUES100].state = 3;
            uiMX = (int)(winW * 0.5f - 400); uiMY = (int)(winH * 0.5f - 200);   /* survol de la 1re categorie */
        }
        if (strstr(shotPath, "menu")) {                   /* mode classe : donnees factices pour l'encadre TOP 3 */
            static const RkTop fake[3] = { { "Vortex", 1612, 41, "MAITRE" }, { "Kestrel", 1418, 33, "PLATINE I" },
                                           { "Nyx", 1377, 25, "DIAMANT III" } };
            memcpy(rkTop, fake, sizeof fake); rkTopN = 3; rkTopState = 1;
            myMmr = 1093; rankPlayed = 14; rankWins = 6; rkMeRank = 12;
        }
        render(1, TICK);
        save_bmp(shotPath);
        return 0;
    }
    if (shotMode && strstr(shotPath, "ranked")) {         /* mode classe : fin de match classe, tableau des scores */
        appState = APP_GAME; dmMode = 1; myId = 0; ranked = 1; connected = 1;
        memset(NP, 0, sizeof NP);
        static const char *nm[3] = { "Moi", "Rival", "Ninja" };
        static const int kl[3] = { 15, 11, 4 }, mm[3] = { 1093, 1188, 962 }, pp[3] = { 14, 30, 6 };
        for (int i = 0; i < 3; i++) {
            snprintf(NP[i].name, 16, "%s", nm[i]); NP[i].active = NP[i].alive = 1; NP[i].hp = 100;
            NP[i].kills = kl[i]; NP[i].deaths = 3 + i * 2; rkMmr[i] = mm[i]; rkPlay[i] = pp[i];
        }
        P.pos = P.prevPos = v3(0, 0, 24); P.yaw = -0.1f; P.pitch = 0.08f; P.hp = 100;
        myMmr = 1093; rankPlayed = 14;
        rkRepState = RS_DONE; rkLastDelta = 18; rkLastMmr = 1093; rkLastPlayed = 14;
        matchOver = 1; matchWinner = 0; matchEndT = 6; matchT = 340;
        render(1, TICK);
        save_bmp(shotPath);
        return 0;
    }
    if (shotMode && strstr(shotPath, "sansbloom")) gfxBloom = 0;   /* captures de comparaison des graphismes */
    if (shotMode && strstr(shotPath, "sansombre")) gfxShadows = 0;
    if (shotMode && strstr(shotPath, "skins")) {          /* les 5 skins alignes, equipes bleue / rouge */
        appState = APP_GAME; reset_game(); dmMode = 1; myId = 7; teamSize = 2;
        memset(NP, 0, sizeof NP);
        for (int i = 0; i < SKN_COUNT; i++) {
            NetPlayer *n = &NP[i];
            n->active = n->alive = 1; n->hp = 100; n->team = (unsigned char)(i & 1);
            snprintf(n->name, 16, "%s", skinNames[i]);
            n->disp = v3(-ms(8) + 3.2f * i, 4, 3.5f + (i == 2 ? -1.0f : 0.0f)); vf(n->st.pos, n->disp);
            n->st.yaw = PI + (i - 2) * 0.35f; n->st.skin = (unsigned char)i; n->st.weapon = (unsigned char)(i % 3);
            if (strstr(shotPath, "course")) { vf(n->st.vel, v3(0, 0, 9)); n->runPh = 0.6f + i; }
        }
        P.pos = P.prevPos = v3(-ms(8) + 6.4f, 4, 8.6f); P.yaw = 0; P.pitch = -0.12f; P.dead = 1;
        nowT = 2; spectating = 1;
        render(1, TICK);
        save_bmp(shotPath);
        return 0;
    }
    if (shotMode && strstr(shotPath, "arene")) {          /* vue d'ensemble de la carte (eclairage, ombres) */
        appState = APP_GAME;
        reset_game();
        for (int k = 1; k < 4; k++) {                     /* arene_nuit / arene_jour / arene_orage : ambiance */
            char nm[16];
            int j = 0;
            for (const char *q = ambianceNames[k]; *q && j < 15; q++) nm[j++] = (char)(*q - 'A' + 'a');
            nm[j] = 0;
            if (strstr(shotPath, nm)) { gfxAmbiance = k; gfx_reload(); popT = 0; }
        }
        P.pos = P.prevPos = v3(-ms(46), 20, ms(46)); P.yaw = 0.75f; P.pitch = -0.38f; P.hp = 100;
        if (strstr(shotPath, "2")) { P.pos = P.prevPos = v3(ms(40), 9, -ms(20)); P.yaw = -2.2f; P.pitch = -0.25f; }
        render(1, TICK);
        save_bmp(shotPath);
        return 0;
    }
    if (shotMode && (strstr(shotPath, "dm") || strstr(shotPath, "skills"))) {   /* capture d'une scene de deathmatch */
        appState = APP_GAME; dmMode = 1; myId = 0;
        memset(NP, 0, sizeof NP);
        snprintf(NP[0].name, 16, "Moi"); NP[0].active = NP[0].alive = 1; NP[0].hp = 100; NP[0].kills = 3;
        snprintf(NP[1].name, 16, "Rival"); NP[1].active = NP[1].alive = 1; NP[1].kills = 5; NP[1].deaths = 2;
        NP[1].disp = v3(-6.5f, 0, 17); NP[1].st.yaw = PI - 0.4f; NP[1].st.weapon = 1; NP[1].st.melee = 120;
        NP[1].st.flags = NF_HOOK; vf(NP[1].st.hookPos, v3(-24, 14, -24));
        snprintf(NP[2].name, 16, "Ninja"); NP[2].active = NP[2].alive = 1; NP[2].kills = 1; NP[2].deaths = 4;
        NP[2].disp = v3(5, 4, 6); NP[2].st.yaw = PI + 0.6f; NP[2].st.pitch = -0.2f;
        kfeed_add(1, 2, W_MELEE, 0); kfeed_add(0, 2, W_RAIL, 0); kfeed_add(-1, 1, 0, 1);
        if (strstr(shotPath, "equipes")) {                /* 2v2 : Ninja est mon allie (bleu), Rival et Ombre en face */
            teamSize = 2; fragLimit = team_frag_limit(2);
            NP[0].team = 0; NP[1].team = 1; NP[2].team = 0;
            snprintf(NP[3].name, 16, "Ombre"); NP[3].active = NP[3].alive = 1; NP[3].team = 1; NP[3].kills = 2;
            NP[3].disp = v3(9, 0, 4); NP[3].st.yaw = PI + 0.3f;
            if (strstr(shotPath, "fin")) { matchOver = 1; matchWinner = 0; matchEndT = 6; NP[0].kills = 11; NP[2].kills = 9; }
        }
        if (strstr(shotPath, "enclume")) {                /* adversaires a l'Enclume : coup au sol et pique */
            NP[1].st.pad = MK_ANVIL;
            NP[2].st.pad = MK_ANVIL | ST_SLAM; NP[2].disp = v3(-2, 5, 12); vf(NP[2].st.vel, v3(0, -34, 0));
            V3 sd = vnorm(v3(1, 0, -0.4f));               /* onde d'un pique de 13 m en courant : mur de 1,9 m etire vers l'avant */
            slam_vis_add(v3(-3, 0, 17), sd, slam_R(13), 18, slam_hw(13));
            SV[0].t = 0.7f * slam_R(13) / SLAM_WAVE_SPEED;
        }
        if (strstr(shotPath, "lance")) {                  /* Lance : Rival lance couchee vers moi (pointe orange), Ninja au repos */
            NP[1].st.pad = MK_LANCE | ST_LANCE; NP[1].st.melee = 0; NP[1].st.flags = 0;
            NP[1].st.yaw = atan2f(6.5f, -7.0f);
            NP[2].st.pad = MK_LANCE;
            P.meleeKind = MK_LANCE;                       /* et ma lance levee */
        }
        if (strstr(shotPath, "fleau")) {                  /* Fleau : Rival en plein balayage vers moi, Ninja au repos */
            NP[1].st.pad = MK_FLAIL; NP[1].st.melee = 110; NP[1].st.flags = 0;
            NP[1].st.yaw = atan2f(6.5f, -7.0f);
            NP[2].st.pad = MK_FLAIL; NP[2].st.melee = 0;
            P.meleeKind = MK_FLAIL;                       /* et mon fleau au repos */
        }
        if (strstr(shotPath, "poings")) {                 /* Poings : Rival en plein uppercut vers moi, Ninja au repos */
            NP[1].st.pad = MK_FIST; NP[1].st.melee = (2 << 6) | 30; NP[1].st.flags = 0;
            NP[1].st.yaw = atan2f(6.5f, -7.0f);
            NP[2].st.pad = MK_FIST; NP[2].st.melee = 0;
            P.meleeKind = MK_FIST;                        /* et mes gants au repos */
        }
        if (strstr(shotPath, "egide")) {                  /* Egide : Rival bouclier leve vers moi (jauge entamee), Ninja baisse */
            NP[1].st.pad = MK_SHIELD | ST_SHIELD; NP[1].st.melee = 0; NP[1].st.flags = 0; NP[1].shieldGauge = 50;
            NP[1].st.yaw = atan2f(6.5f, -7.0f);
            NP[2].st.pad = MK_SHIELD; NP[2].shieldGauge = 120;
            P.meleeKind = MK_SHIELD; P.shieldState = SHS_UP; P.shieldAnim = 1; P.shieldGauge = 80;   /* et le mien leve */
        }
        P.pos = P.prevPos = v3(0, 0, 24); P.yaw = -0.1f; P.pitch = 0.08f; P.hp = 100;
        if (strstr(shotPath, "bas")) { P.yaw = -0.45f; P.pitch = -0.32f; }   /* regard vers le sol : ombres des joueurs */
        if (strstr(shotPath, "mort")) {                   /* camera de mort : Rival m'a eu au rail, la vue se tourne vers lui */
            P.yaw = 1.2f; P.pitch = -0.1f; NP[1].hp = 45;
            killcam_start(1, W_RAIL); P.dead = 1; P.deathT = 0.9f;
        }
        if (strstr(shotPath, "eventail")) {               /* Rival a l'Eventail : son tir recu (traceurs recalcules) */
            NP[1].st.weapon = WP_SHOTGUN; NP[1].st.flags = 0; NP[1].st.melee = 0;
            NetEvent e = ev_new(EV_SHOT);
            V3 o = v3(NP[1].disp.x, NP[1].disp.y + EYE_STAND, NP[1].disp.z), to = vnorm(vsub(v3(1.5f, 1.2f, 24), o));
            e.from = 1; e.sub = SG_PELLETS; e.netId = 777u; e.f[6] = SG_HALF_ANGLE; e.actor = 3;
            vf(e.f, o); vf(e.f + 3, to);
            apply_event(&e);
            for (int i = 0; i < 3; i++) fx_tick(TICK);
        }
        if (strstr(shotPath, "tesla")) {                  /* Tesla : le Rival tient son arc sur moi (recalcule ici) */
            NP[1].st.weapon = WP_TESLA; NP[1].st.flags = 0; NP[1].st.melee = 0; NP[1].st.pad |= ST_ZAP;
            V3 d = vsub(pcenter(), np_eye(1));
            NP[1].st.yaw = atan2f(d.x, -d.z); NP[1].st.pitch = asinf(d.y / vlen(d));
            NP[1].teslaHitMeT = nowT = 1;
            tesla_fx_tick(TICK);
        }
        if (strstr(shotPath, "skills")) {                 /* Competences : Rival marque (losange), leurre MIRAGE de Ninja, le mien */
            P.skill = SK_MARK; P.skillCh = 1.4f; markP[1] = 3;
            memset(CO, 0, sizeof CO);
            CO[0].alive = 1; CO[0].decoy = 1; CO[0].owner = 2; CO[0].life = 2; CO[0].spin = PI + 0.6f;
            CO[0].pos = v3(7, 0.9f, 15);
            CO[1].alive = 1; CO[1].decoy = 1; CO[1].owner = 0; CO[1].life = 0.5f; CO[1].spin = 0.3f;
            CO[1].pos = v3(-3, 0.9f, 19);
            add_beam(v3(0.3f, 1.4f, 23.4f), np_center(1), v3(0.85f, 0.35f, 1), 0.3f);
        }
        matchT = 95;
        if (strstr(shotPath, "bench")) { bench_render(shotPath); return 0; }
        render(1, TICK);
        save_bmp(shotPath);
        return 0;
    }
    if (shotMode && strstr(shotPath, "objectifs")) {      /* objectifs d'un mode sur la carte --map N : objectifs_zones,
                                                             objectifs_drapeau, objectifs_bouclier (vue : --pos, --yaw) */
        const MapDef *md = &maps[curMap];
        int mode = strstr(shotPath, "drapeau") ? MODE_DRAPEAU : strstr(shotPath, "bouclier") ? MODE_BOUCLIER : MODE_ZONES;
        appState = APP_GAME; dmMode = 1; netMode = NET_HOST; myId = 0; teamSize = 2;
        NP[0].active = NP[0].alive = 1; NP[0].team = 0;
        rules = (MatchRules){ mode, 1, AMMO_NORMAL, 0, 10 }; mode_reset();
        if (mode == MODE_ZONES) { obj.z[0].owner = 0; obj.z[2].owner = 1; obj.z[1].capTeam = 1; obj.z[1].prog = 120; obj.pts[0] = 31; obj.pts[1] = 27; }
        P.pos = md->menuCam; P.pitch = md->menuPitch; P.yaw = shotYaw;
        if (shotPosSet) { P.pos = shotPos; if (shotPitch > -999) P.pitch = shotPitch; }
        P.prevPos = P.pos; P.vel = v3(0, 0, 0);
        matchT = 40; gameTime = 1.3f;
        render(1, TICK);
        save_bmp(shotPath);
        return 0;
    }
    if (shotMode && (strstr(shotPath, "boutique") || strstr(shotPath, "armedepart"))) {   /* boutique a la mort, choix de l'arme */
        load_map(0);                                      /* l'Arene en 3 ZONES */
        reset_game(); appState = APP_GAME; dmMode = 1; netMode = NET_HOST; myId = 0; teamSize = 2;
        NP[0].active = NP[0].alive = 1; NP[0].team = 0;
        rules = (MatchRules){ MODE_ZONES, 0, AMMO_NORMAL, 1, 10 }; mode_reset(); eco_reset();
        obj.z[0].owner = 0; obj.z[2].owner = 1; obj.z[1].capTeam = 1; obj.z[1].prog = 150; obj.pts[0] = 64; obj.pts[1] = 41; matchT = 140;
        P.pos = P.prevPos = v3(0, 0, -30); P.yaw = 0; P.pitch = 0.05f;
        if (strstr(shotPath, "boutique")) {
            eco_pick_starter(WP_SHOTGUN); credits = 1150; eco_buy(WP_ROCKET); credits = 850;
            P.dead = 1; NP[0].alive = 0;
        }
        loadout_apply();
        gameTime = 3;
        render(1, TICK);
        save_bmp(shotPath);
        return 0;
    }
    if (shotMode && (strstr(shotPath, "course") || strstr(shotPath, "speedrun"))) {   /* mode COURSE : course.bmp (en route, fantome
                                                       * devant), course_depart, course_fin ; speedrun complet : speedrun, speedrun_fin */
        int full = strstr(shotPath, "speedrun") != NULL;
        if (full) sr_full_start(); else race_start();
        appState = APP_GAME;
        const MapDef *md = &maps[curMap];
        raceBest = 61.37f;
        for (int i = 0; i < md->ngates; i++) raceBestSplit[i] = 4.5f * (i + 1);
        ghostBestN = 0;                                   /* fantome : tout droit de porte en porte, 4,5 s par porte */
        for (int i = 0; i < md->ngates && ghostBestN < RACE_GMAX - 200; i++) {
            V3 a = i ? race_gate(i - 1) : md->start, b = race_gate(i);
            for (int k = 0; k < 135; k++) {
                V3 p = vlerp(a, b, k / 135.0f);
                ghostBest[ghostBestN][0] = p.x; ghostBest[ghostBestN][1] = p.y - 1.2f; ghostBest[ghostBestN][2] = p.z;
                ghostBest[ghostBestN][3] = atan2f(b.x - a.x, -(b.z - a.z)); ghostBestN++;
            }
        }
        if (!strstr(shotPath, "depart")) {
            raceCount = 0; raceGate = 2; raceT = 11.2f; raceDelta = -0.42f; raceDeltaT = 1.5f;
            V3 g = race_gate(2);
            P.pos = P.prevPos = v3(-62, 31, 2 + 18); P.vel = v3(0, -2, -24); P.grounded = 0;
            race_face(P.pos, 2); P.pitch = asinf(clampf((g.y - P.pos.y - 1.6f) / vlen(vsub(g, P.pos)), -1, 1));
            P.hook = HOOK_ATTACHED; P.anchor = P.hookPos = v3(-46 - 4, 50, -6); P.ropeLen = 30;
            style = 250; styleRank = ST_A;
        } else raceCount = 2.4f;
        if (strstr(shotPath, "aimant")) {                 /* course_aimant : en l'air, le repere sur le point que prendra le grappin */
            P.hook = HOOK_NONE; P.pos = P.prevPos = v3(-60, 30, 30); P.yaw = 0; P.pitch = 0.35f;
        }
        if (strstr(shotPath, "fin")) { raceDone = 1; raceT = 58.94f; raceNewBest = 2; raceFalls = 1; styleTopRank = ST_S; P.hook = HOOK_NONE; }
        SrRun *sr = &srRun[SR_MAP0 + curMap];             /* classement en ligne : reponse factice du serveur */
        sr->state = SRR_RECORD; sr->rank = 4; srTop[SR_MAP0 + curMap].state = 2; srTop[SR_MAP0 + curMap].runners = 37;
        if (full) {                                       /* speedrun complet : record perso, puis bilan a l'arrivee */
            for (int k = 0; k < srMapN; k++) srPb[k] = srCmp[k] = 61370 * (k + 1);
            srPbN = srCmpN = srMapN;
            if (raceDone) {
                srSplit[srMapN - 1] = 58940; srDone = 1; srNewBest = 2;
                srRun[SR_COMPLET].state = SRR_RECORD; srRun[SR_COMPLET].rank = 2;
                srTop[SR_COMPLET].state = 2; srTop[SR_COMPLET].runners = 12;
            }
        }
        gameTime = 3;
        render(1, TICK);
        save_bmp(shotPath);
        return 0;
    }
    if (shotMode && curMap != 0) {        /* autres cartes : vue du menu, d'un point de depart ("spawn"), ou chute vers le vide ("chute") */
        const MapDef *md = &maps[curMap];
        appState = APP_GAME;
        P.pos = md->menuCam; P.pitch = md->menuPitch; P.yaw = shotYaw;
        if (strstr(shotPath, "spawn")) { P.pos = v3(md->spawns[0][0], md->spawns[0][1], md->spawns[0][2]); P.pitch = -0.05f; }
        if (strstr(shotPath, "chute")) { P.pos = v3(md->menuCam.x + 2, md->killY + 3, md->menuCam.z + 2); P.vel = v3(0, -20, 0); P.pitch = -0.7f; }
        if (shotPosSet) { P.pos = shotPos; if (shotPitch > -999) P.pitch = shotPitch; }
        P.prevPos = P.pos;
        if (md->solo && strstr(shotPath, "solo")) { wave = 3; for (int i = 0; i < 8; i++) spawn_enemy(); }
        gameTime = 1.3f;
        render(1, TICK);
        save_bmp(shotPath);
        return 0;
    }
    if (shotMode) {                       /* capture auto : une scene de combat scriptee */
        appState = APP_GAME;
        wave = 3;
        for (int i = 0; i < 8; i++) spawn_enemy();
        P.pos = P.prevPos = v3(-6, 0, 30); P.yaw = 0.25f; P.pitch = 0.12f;
        P.vel = v3(4, 0, -12);
        for (int i = 0; i < 90; i++) game_tick(TICK);
        P.fireCd = 0; botHeld[A_FIRE] = 1; game_tick(TICK); botHeld[A_FIRE] = 0;
        P.weapon = 1; for (int i = 0; i < 20; i++) game_tick(TICK);
        P.fireCd = 0; botHeld[A_FIRE] = 1; game_tick(TICK); botHeld[A_FIRE] = 0;
        botPress[A_HOOK] = 1; botHeld[A_HOOK] = 1;        /* grappin sur la tour centrale */
        for (int i = 0; i < 12; i++) game_tick(TICK);
        snprintf(msg, sizeof msg, "VAGUE %d", wave); msgT = 2;
        if (strstr(shotPath, "rail")) { P.weapon = 0; P.switchT = 0; P.fireCd = 0; }
        if (strstr(shotPath, "changearme")) {             /* changement d'arme : nom au centre, arme qui remonte */
            msgT = 0; P.weapon = P.loadout[1]; weapon_select(P.loadout[0]); P.switchT = SWITCH_TIME * 0.5f; wpFlashAt = nowT - 0.25;
        }
        if (strstr(shotPath, "vie")) { P.hp = hpLast = 45; hpGhost = 100; hpHitAt = nowT - 0.1; }   /* barre de vie juste apres un coup */
        shotStreak = 4; P.meleeT = 0.12f; P.vel = v3(0, 0, -14);   /* pour voir le HUD des deux axes */
        if (strstr(shotPath, "elan")) {                   /* Elan : un tir plein, puis la fenetre du dash-tir ouverte */
            P.loadout[1] = WP_KINETIC; P.weapon = WP_KINETIC; P.switchT = 0; P.fireCd = 0; P.meleeT = 0;
            P.ammo[WP_KINETIC] = 5; P.ammoGauge[WP_KINETIC] = 30;
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 300; E[0].pos = vadd(eye_pos(), vmul(view_dir(), 12));
            P.vel = v3(0, 0, -16); P.kinDashT = 1;
            fire_kinetic();
            P.fireCd = 0; P.kinT = 0; muzzleT = 0; P.vel = v3(0, 0, -16); P.kinDashT = 0.03f;
            impactT = 0; hitstop = 0;                     /* on veut voir le rayon, pas l'impact frame */
        }
        if (strstr(shotPath, "eventail")) {               /* Eventail : un tir sur un drone, traceurs en vol, pompe, cercle */
            hook_release(0);
            P.loadout[1] = WP_SHOTGUN; P.weapon = WP_SHOTGUN; P.switchT = 0; P.fireCd = 0; P.meleeT = 0;
            P.ammo[WP_SHOTGUN] = 4; P.ammoGauge[WP_SHOTGUN] = 60;
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 300; E[0].pos = vadd(eye_pos(), vmul(view_dir(), 12));
            P.vel = v3(0, 0, -8);
            fire_shotgun();
            hitstop = 0; impactT = 0; muzzleT = 0.06f;
            for (int i = 0; i < 4; i++) fx_tick(TICK);
            P.sgPumpT = P.sgPumpMax * 0.45f;              /* pompe en plein recul */
        }
        if (strstr(shotPath, "tete")) {                   /* tirs a la tete : chiffres TETE et ONE-SHOT, chaine de tirs pleine */
            P.weapon = WP_RAIL; P.ammo[WP_RAIL] = 2; P.fireCd = 0; P.meleeT = 0; msgT = 0;
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = vadd(eye_pos(), vmul(view_dir(), 10));
            E[0].pos.y -= enemy_radius(EN_DRONE) * 0.75f;   /* le viseur sur le point faible, au sommet */
            fire_rail();
            dmg_number_k(vadd(E[0].pos, v3(1.6f, 0.2f, 0)), 105, DN_ONESHOT);
            shotStreak = STREAK_MAX; impactT = 0; hitstop = 0;
        }
        if (strstr(shotPath, "impact") || strstr(shotPath, "fx")) {   /* un kill au rail a pleine vitesse */
            P.weapon = WP_RAIL; P.ammo[WP_RAIL] = 2; P.fireCd = 0; P.meleeT = 0;
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 60; E[0].pos = vadd(eye_pos(), vmul(view_dir(), 9));
            fire_rail();
            if (strstr(shotPath, "fx")) {
                for (int i = 0; i < 14; i++) game_tick(TICK);
                impactT = 0;
            }
            P.vel = v3(0, 0, -32);
        }
        if (strstr(shotPath, "style")) {                  /* jauge de style en S, trois bonus qui defilent */
            styleShow = 1;                                /* la jauge n'existe qu'en CHALLENGE : forcee pour la capture */
            P.grounded = 0; P.vel = v3(0, 0, -26);
            style_kill(W_COIN); style_kill(W_RAIL); style_add(60, "PARRY");
            styleFlash = 0.4f;
        }
        if (strstr(shotPath, "enclume")) {                /* pique de l'Enclume au-dessus d'un drone : repere d'impact */
            hook_release(0);
            P.meleeKind = MK_ANVIL; P.meleeT = 0; P.meleeCd = 0; P.weapon = WP_RAIL; P.switchT = 0; shotStreak = 0;
            P.pos = P.prevPos = v3(-20, 9, 40); P.yaw = 0; P.pitch = -0.7f; P.vel = v3(0, 0, -6); P.grounded = 0;
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.2f, 35);
            memset(PA, 0, sizeof PA); memset(PR, 0, sizeof PR); memset(BM, 0, sizeof BM);
            anvil_press();
            for (int i = 0; i < 6; i++) player_tick(TICK, 0, 0);
        }
        if (strstr(shotPath, "lance")) {                  /* Lance couchee armee vers un drone : anneau du cone, pointe orange */
            hook_release(0);
            memset(PA, 0, sizeof PA); memset(PR, 0, sizeof PR); memset(BM, 0, sizeof BM); memset(E, 0, sizeof E);
            P.meleeKind = MK_LANCE; P.meleeT = 0; P.weapon = P.loadout[0]; P.switchT = 0; shotStreak = 0;
            P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0.05f; P.pitch = -0.05f; P.vel = v3(0, 0, -14);
            P.lanceState = LS_COUCH; P.lanceT = 0.8f;
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.4f, 31);
            P.lanceVc = vdot(P.vel, vnorm(vsub(E[0].pos, eye_pos())));
        }
        if (strstr(shotPath, "fleau")) {                  /* Fleau : chaine accrochee a un drone (ou balayage a vide : "vide") */
            hook_release(0);
            memset(PA, 0, sizeof PA); memset(PR, 0, sizeof PR); memset(BM, 0, sizeof BM); memset(E, 0, sizeof E);
            P.meleeKind = MK_FLAIL; P.meleeT = 0; P.meleeCd = 0; P.weapon = P.loadout[0]; P.switchT = 0; shotStreak = 0;
            P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0.05f; P.pitch = -0.05f; P.vel = v3(0, 0, -8);
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-19, 1.8f, 35);
            if (strstr(shotPath, "vide")) E[0].pos = v3(-20, 6, 25);
            flail_swing();
            P.flailT = FLAIL_ANIM * (strstr(shotPath, "vide") ? 0.5f : 0.7f);
            hitstop = 0; impactT = 0; P.meleeT = 0;
        }
        if (strstr(shotPath, "poings")) {                 /* Poings : direct droit (ou uppercut : "upper") sur un drone, mur proche */
            hook_release(0);
            memset(PA, 0, sizeof PA); memset(PR, 0, sizeof PR); memset(BM, 0, sizeof BM); memset(E, 0, sizeof E);
            P.meleeKind = MK_FIST; P.meleeT = 0; P.meleeCd = 0; P.weapon = P.loadout[0]; P.switchT = 0; shotStreak = 0;
            P.fistStep = 0; P.fistPJ = 1; P.recoil = 0;
            P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0.05f; P.pitch = -0.05f; P.vel = v3(0, 0, -8); P.grounded = 1;
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 300; E[0].pos = v3(-20, 1.6f, 38.3f); E[0].fireT = 10;
            int n = strstr(shotPath, "upper") ? 3 : 2;
            for (int k = 0; k < n; k++) { P.meleeCd = 0; fist_punch(); }
            P.meleeT = FIST_ANIM * 0.45f; P.fistGuard = 1;
            hitstop = 0; impactT = 0; P.vel = v3(0, 0, -8);
        }
        if (strstr(shotPath, "egide")) {                  /* Egide : bouclier baisse ("bas") ou leve, jauge a 45 (fissures) */
            hook_release(0);
            memset(PA, 0, sizeof PA); memset(BM, 0, sizeof BM);
            P.meleeKind = MK_SHIELD; P.meleeT = 0; P.weapon = WP_RAIL; P.switchT = 0; shotStreak = 0;
            P.shieldGauge = 45;
            if (!strstr(shotPath, "bas")) { P.shieldState = SHS_UP; P.shieldAnim = 1; }
        }
        if (strstr(shotPath, "scie")) {                   /* Scie : la mienne plantee, une adverse en vol (2 renvois) */
            hook_release(0);
            memset(PA, 0, sizeof PA); memset(PR, 0, sizeof PR); memset(BM, 0, sizeof BM); memset(E, 0, sizeof E);
            P.loadout[0] = WP_SAW; P.weapon = WP_SAW; P.switchT = 0; P.meleeT = 0; P.recoil = 0; shotStreak = 0;
            P.pos = P.prevPos = v3(-20, 0, 50); P.yaw = 0.15f; P.pitch = -0.12f; P.vel = v3(0, 0, -6);
            P.sawIdx = 0;
            Proj *a = saw_test_put(v3(-18.5f, -SAW_EMBED, 43), v3(0, 0, 0), SAW_PLANTED, myId);
            a->sawAxis = vnorm(v3(1, 0, 1)); a->sawT = 5;
            Proj *b = saw_test_put(v3(-21.2f, 1.3f, 45.5f), v3(0, 0, 30), SAW_FLY, 1);
            b->sawAxis = vnorm(v3(0, 1, 0.5f)); b->sawBounces = 2; b->sawSpin = 0.3f;
            if (strstr(shotPath, "main")) { a->alive = 0; P.sawIdx = 0; P.ammo[WP_SAW] = 1; }   /* variante : la scie en main */
        }
        if (strstr(shotPath, "tesla")) {                  /* Tesla : arc tenu, chaine sur 3 drones, une piece en relais */
            hook_release(0);
            memset(PA, 0, sizeof PA); memset(PR, 0, sizeof PR); memset(BM, 0, sizeof BM); memset(E, 0, sizeof E);
            memset(CO, 0, sizeof CO);
            P.loadout[1] = WP_TESLA; P.weapon = WP_TESLA; P.switchT = 0; P.fireCd = 0; P.meleeT = 0; P.recoil = 0;
            shotStreak = 0; P.ammo[WP_TESLA] = 66;
            P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0.1f; P.pitch = 0.02f; P.vel = v3(0, 0, -8);
            V3 a = vadd(eye_pos(), vmul(view_dir(), 9));
            tesla_test_drone(0, a, 900);
            tesla_test_drone(1, vadd(a, v3(3.5f, 1, -3)), 900);
            tesla_test_drone(2, vadd(a, v3(5.5f, -0.5f, -7)), 900);
            tesla_test_drone(3, v3(-27, 2.5f, 31), 900);
            CO[0].alive = 1; CO[0].owner = myId; CO[0].life = 4; CO[0].pos = v3(-25.5f, 2.2f, 34);
            for (int i = 0; i < 30; i++) {
                if (i == 12) CO[0].pos = vadd(eye_pos(), vmul(view_dir(), 4));   /* la piece passe dans l'arc */
                tesla_test_run(1, 1);
                if (i == 12) CO[0].pos = v3(-25.5f, 2.2f, 34);
                tesla_fx_tick(TICK);
            }
            hitstop = 0; impactT = 0;
        }
        render(1, TICK);
        save_bmp(shotPath);
        return 0;
    }

    appState = APP_MENU;
    menu_backdrop();
    if (botMode) {                                        /* chaque bot prend un skin au hasard */
        LARGE_INTEGER q;
        QueryPerformanceCounter(&q);
        wantSkin = (int)((unsigned long long)q.QuadPart % SKN_COUNT);
    }
    if (botMode == 1) pendingAction = ACT_HOST;
    if (botMode == 2) pendingAction = ACT_BROWSE;
    if (botMode && botRanked) pendingAction = ACT_RANKED;   /* recherche classee ; sans partie, l'hote en cree une */

    QueryPerformanceFrequency(&frmFreq);
    QueryPerformanceCounter(&frmStart);
    frmLast = frmStart;
#ifdef _WIN32
    MSG msgw;
#endif
    for (;;) {
#ifdef _WIN32
        while (PeekMessageA(&msgw, NULL, 0, 0, PM_REMOVE)) {
            if (msgw.message == WM_QUIT) goto done;
            TranslateMessage(&msgw);
            DispatchMessageA(&msgw);
        }
#else
        if (!mac_pump()) goto done;
#endif
        if (!main_frame()) break;
    }
done:
    if (botMode) {
        if (botShot[0]) { render(1, 0.016f); save_bmp(botShot); }
        netlog("=== resume (%s, id %d) ===", netMode == NET_HOST ? "hote" : "client", myId);
        for (int i = 0; i < MAXP; i++)
            if (NP[i].active) netlog("%s : %d frags, %d morts, %.0f PV%s", NP[i].name, NP[i].kills, NP[i].deaths, NP[i].hp,
                                     i == myId ? " (moi)" : "");
        netlog("paquets recus %d, envoyes %d", pktIn, pktOut);
        netlog("vitesse du bot : moyenne %.1f m/s, max %.1f m/s", botSpeedSum / fmaxf(botAliveT, 0.01f), botMaxSpeed);
        if (netMode == NET_HOST)
            netlog("equilibre : temps pour tuer moyen %.2f s sur %d kills, one-shots %d, parries reussis %d, epees parees %d, chocs %d, "
                   "salves mortelles %d", ttkN ? ttkSum / ttkN : 0.0, ttkN, oneShots, parryCount, meleeParries, clashes, burstKills);
        if (netMode == NET_HOST) {
            char hb[512]; int n = 0;
            for (int w = 0; w < W_COUNT && n < (int)sizeof hb - 40; w++)
                if (hitCount[w]) n += snprintf(hb + n, sizeof hb - n, "%s %d, ", weaponNames[w], hitCount[w]);
            netlog("touches arbitrees : %s parries %d", n ? hb : "aucune, ", parryCount);
            n = 0;                                        /* tirs a la tete et chaine pleine */
            for (int w = 0; w < W_COUNT && n < (int)sizeof hb - 40; w++)
                if (headHostHits[w]) n += snprintf(hb + n, sizeof hb - n, "%s %d, ", weaponNames[w], headHostHits[w]);
            netlog("tetes (arbitre) : %s one-shots a la tete %d, chaine pleine %d ; refus : tete %d, chaine %d",
                   n ? hb : "aucune, ", headOneShots, chainOneShots, headRefused, chainRefused);
        }
        netlog("tetes (moi) : %d ; chaine pleine %d fois, %d tirs chaine pleine dont %d touches", headShotsMine, chainFulls,
               chainShots, chainShotHits);
        if (maps[curMap].killY > -999)                    /* kill zone */
            netlog("carte %s : chutes dans le vide %d (moi)%s", maps[curMap].name, voidFalls, "");
        if (netMode == NET_HOST && maps[curMap].killY > -999)
            netlog("vide (arbitre) : %d chutes, %d creditees a l'attaquant, %d suicides", voidCredited + voidSuicides, voidCredited,
                   voidSuicides);
        netlog("navigation : graphe %d noeuds, %d chemins, aretes suivies marche %d saut %d pad %d grappin %d lacher %d, arrivees %d",
               nnav, navPaths, navStat[NE_WALK], navStat[NE_JUMP], navStat[NE_PAD], navStat[NE_HOOK], navStat[NE_DROP], navArrive);
        netlog("munitions : points touche %.0f, kill %.0f, parry %.0f, cellules %.0f (%d prises), perdus %.0f ; a sec %.0f%% du temps",
               apStat[AP_HIT], apStat[AP_KILL], apStat[AP_PARRY], apStat[AP_CELL], cellsTaken, apStat[AP_LOST],
               100 * dryT / fmaxf(botAliveT, 0.01f));
        netlog("competences : %s ; utilisations piece %d, marque %d, charge %d, ricochet %d, all in one %d, mirage %d, aimant %d ; "
               "touches bonifiees %d ; ricochets : %d tirs, %d rebonds, %d retours sur soi ; dernier all in one %d projectiles ; "
               "tirs absorbes par un leurre adverse %d",
               skillDef[P.skill].name, skillUse[SK_COIN], skillUse[SK_MARK], skillUse[SK_CHARGE], skillUse[SK_RICOCHET],
               skillUse[SK_ALLIN], skillUse[SK_DECOY], skillUse[SK_AIM], skillBonus, ricStat[0], ricStat[1], ricStat[2], allinLast, decoyHits);
        if (slamDives || slamWaves || slamParried || hitCount[W_SLAM])
            netlog("enclume : %d piques, ondes qui touchent %d / %d, |vy| moyen %.1f, degats moyens par onde %.1f, "
                   "immobilisations %d ; arbitre : touches %d, ondes parees %d, combos onde + rail %d",
                   slamDives, slamWaveHits, slamWaves, slamVySum / (slamWaves ? slamWaves : 1),
                   slamDmgSum / (slamWaves ? slamWaves : 1), slamStuns, hitCount[W_SLAM], slamParried, slamRailCombos);
        if (sgShots || hitCount[W_SHOTGUN])
            netlog("eventail : %d tirs, plombs moyens %.1f, tirs qui comptent pour la chaine %.0f%%, shotgun jumps %d, "
                   "pieces eclatees %d ; arbitre : touches %d",
                   sgShots, sgPelletSum / (float)(sgShots ? sgShots : 1), 100.0f * sgStreakShots / (sgShots ? sgShots : 1),
                   sgJumps, sgBurstSum, hitCount[W_SHOTGUN]);
        if (sawStat[SS_THROW] || sawStat[SS_BOUNCE])
            netlog("scie (moi) : %d lancers, %d touches (rel moyenne %.1f m/s), %d plantages, %d rappels, %d ramassages a pied, "
                   "%d pertes ; renvois tentes %d, reussis %d, qui touchent %d",
                   sawStat[SS_THROW], sawStat[SS_HIT], sawRelSum / (sawStat[SS_HIT] ? sawStat[SS_HIT] : 1), sawStat[SS_PLANT],
                   sawStat[SS_RECALL], sawStat[SS_FOOT], sawStat[SS_LOST], sawStat[SS_TRY], sawStat[SS_BOUNCE], sawStat[SS_BHIT]);
        if (teslaPulses || hitCount[W_TESLA] || teslaReflects)   /* Tesla */
            netlog("tesla (moi) : %d impulsions, %d qui touchent (accroche %.0f%%), batterie depensee %d, relais %d, chaines x3 %d, "
                   "surcharges subies %d ; arbitre : touches %d, decharges renvoyees %d",
                   teslaPulses, teslaPulseHits, 100.0f * teslaPulseHits / (teslaPulses ? teslaPulses : 1), teslaSpent, teslaRelays,
                   teslaChains3, teslaStuns, hitCount[W_TESLA], teslaReflects);
        if (netMode == NET_HOST && (sawHostStat[SAWE_THROW] || hitCount[W_SAW]))
            netlog("scie (arbitre) : lancers %d, touches %d (dont %d apres renvoi), plantages %d, rattrapages %d, rappels %d, renvois %d",
                   sawHostStat[SAWE_THROW], hitCount[W_SAW], sawHostBounceHits, sawHostStat[SAWE_PLANT], sawHostStat[SAWE_CATCH],
                   sawHostStat[SAWE_RECALL], sawHostStat[SAWE_BOUNCE]);
        if (lanceThrusts || hitCount[W_LANCE])            /* --- Lance --- */
            netlog("lance (moi) : %d estocs (%d touchent), %d lances couchees, %d joutes (v_c moyen %.1f, %d au grappin), "
                   "%d relevees sans toucher",
                   lanceThrusts, lanceThrustHits, lanceCouches, lanceJousts, lanceVcSum / (lanceJousts ? lanceJousts : 1),
                   lanceJoustHook, lanceWhiffs);
        if (netMode == NET_HOST && hitCount[W_LANCE])
            netlog("lance (arbitre) : touches couchees %d (v_c moyen %.1f), estocs %d, parees %d, chocs de joute gagnes %d, "
                   "doubles desarconnements %d",
                   lanceHostCouch, lanceHostVcSum / (lanceHostCouch ? lanceHostCouch : 1), lanceHostThrust, lanceHostParried,
                   lanceClashWins, lanceClashDouble);
        if (mineThrows || hitCount[W_MINE] || mineTorn)  /* --- Ventouse --- */
            netlog("ventouse (moi) : %d lancers, collees decor %d / joueur %d / drone %d ; explosions telecommande %d, meche %d, "
                   "3e mine %d, tir %d, en vol %d ; DETONER TOUT %d, DETONER UNE %d ; arrachees %d ; delai collage -> explosion "
                   "sur un joueur %.2f s",
                   mineThrows, mineStickW, mineStickP, mineStickE, mineBoom[MX_REMOTE], mineBoom[MX_FUSE], mineBoom[MX_THIRD],
                   mineBoom[MX_SHOT], mineBoom[MX_FLY], mineDetAll, mineDetOne, mineTorn, mineArmedT / (mineArmedN ? mineArmedN : 1));
        if (mineThrows && mineBoom[MX_PROX]) netlog("ventouse (moi) : pieges declenches %d", mineBoom[MX_PROX]);
        if (netMode == NET_HOST && (mineHostBooms || mineHostTorn || hitCount[W_MINE]))
            netlog("ventouse (arbitre) : touches %d, explosions %d, arrachages %d, annulations %d, touches ignorees %d",
                   hitCount[W_MINE], mineHostBooms, mineHostTorn, mineCancels, mineHitsIgnored);
        if (flailSwings || hitCount[W_FLAIL])             /* --- Fleau --- */
            netlog("fleau (moi) : %d coups, %d qui touchent (%.1f cible(s) par coup, v_rel moyen %.1f), pieces frappees %d "
                   "(%d touchent), combos fleau -> rail %d",
                   flailSwings, flailHitSwings, flailTargets / (float)(flailHitSwings ? flailHitSwings : 1),
                   flailRelSum / (flailTargets ? flailTargets : 1), flailCoins, flailCoinHits, flailCombos);
        if (netMode == NET_HOST && (hitCount[W_FLAIL] || flailHostParried))
            netlog("fleau (arbitre) : touches %d, fleaux pares %d", hitCount[W_FLAIL], flailHostParried);
        if (fistPunches || hitCount[W_FIST])              /* --- Poings --- */
            netlog("poings (moi) : %d coups, %d touchent (v_rel moyen %.1f), %.2f touche(s) par combo, combos complets %d / "
                   "commences %d, casses %d, uppercuts qui touchent %d (puis tir %d), punch-jumps %d (%.1f par minute)",
                   fistPunches, fistHits, fistRelSum / (fistHits ? fistHits : 1), fistHits / (float)(fistCombos ? fistCombos : 1),
                   fistFullCombos, fistCombos, fistBreaks, fistUppers, fistUpRails, fistPJs, fistPJs * 60.0f / fmaxf(botAliveT, 1));
        if (netMode == NET_HOST && (hitCount[W_FIST] || fistHostParried))
            netlog("poings (arbitre) : touches %d, poings pares %d", hitCount[W_FIST], fistHostParried);
        if (shieldRaises || shieldBashes)                 /* --- Egide --- */
            netlog("egide (moi) : %d levees (%.1f s leve au total), %d coups de bouclier (%d touchent), %d beliers",
                   shieldRaises, shieldUpSum, shieldBashes, shieldBashHits, shieldRams);
        if (netMode == NET_HOST && (shieldBlocks || shieldDenied || shieldPerfects || hitCount[W_SHIELD]))
            netlog("egide (arbitre) : blocages %d, casses %d, parfaits %d, refus %d, conversions %d, arrachages %d, touches %d",
                   shieldBlocks, shieldBreaks, shieldPerfects, shieldDenied, shieldConverted, shieldRips, hitCount[W_SHIELD]);
    }
    net_shutdown();
    if (botMode && botRanked) rk_bot_final();
    if (botLog) fclose(botLog);
    set_capture(0);
#ifdef _WIN32
    if (audio_ok) { waveOutReset(wout); waveOutClose(wout); }
    map_update(0);                                        /* ferme la fenetre du second ecran */
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
#else
    if (audio_ok) mac_audio_close();
    mac_shutdown();
#endif
    return 0;
}

#ifndef _WIN32
int main(int argc, char **argv) {
    static char cmd[1024];                                /* la ligne de commande entiere, comme sous Windows */
    cmd[0] = 0;
    for (int i = 1; i < argc; i++) {
        if (!strncmp(argv[i], "-psn_", 5)) continue;      /* argument ajoute par Finder sur les anciens macOS */
        size_t n = strlen(cmd);
        snprintf(cmd + n, sizeof cmd - n, "%s%s", n ? " " : "", argv[i]);
    }
    return velocity_main(cmd);
}
#endif
