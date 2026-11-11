/* VELOCITY - second ecran : fenetre plein ecran sur l'autre moniteur, avec la carte (interface/carte_ecran.c)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* Si un deuxieme ecran est branche (et le reglage secondScreen actif), une fenetre sans bordure le couvre. Elle a son
   propre contexte OpenGL qui partage textures et listes avec celui du jeu ; elle ne prend jamais le focus ni la souris.
   Toutes les SCREEN_POLL_T s on recompte les ecrans : brancher / debrancher / deplacer la fenetre du jeu est suivi. */

#ifdef _WIN32
static HWND mapWnd;
static HDC mapDC;
static HGLRC mapRC, mainRC;
static RECT mapRect;

typedef struct { HMONITOR self; RECT r; int found; } MonFind;
static BOOL CALLBACK map_mon_enum(HMONITOR m, HDC d, LPRECT r, LPARAM lp) {
    MonFind *f = (MonFind *)lp;
    (void)d;
    if (m != f->self && !f->found) { f->r = *r; f->found = 1; }
    return TRUE;
}

static LRESULT CALLBACK map_wndproc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    if (m == WM_CLOSE) return 0;                          /* elle se ferme avec le jeu */
    if (m == WM_MOUSEACTIVATE) return MA_NOACTIVATE;      /* un clic ne vole pas le focus au jeu */
    return DefWindowProcA(h, m, wp, lp);
}

static void map_close(void) {
    if (!mapWnd) return;
    wglMakeCurrent(hdc, mainRC);
    wglDeleteContext(mapRC);
    ReleaseDC(mapWnd, mapDC);
    DestroyWindow(mapWnd);
    mapWnd = NULL; mapDC = NULL; mapRC = NULL;
}

static int map_update(int want) {                         /* ouvre, deplace ou ferme ; 1 si la fenetre est ouverte */
    MonFind f = { MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), { 0, 0, 0, 0 }, 0 };
    if (want) EnumDisplayMonitors(NULL, NULL, map_mon_enum, (LPARAM)&f);
    if (!f.found) { map_close(); return 0; }
    int w = f.r.right - f.r.left, h = f.r.bottom - f.r.top;
    if (mapWnd) {
        if (memcmp(&f.r, &mapRect, sizeof f.r))
            SetWindowPos(mapWnd, NULL, f.r.left, f.r.top, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
        mapRect = f.r;
        return 1;
    }
    static int registered;
    if (!registered) {
        WNDCLASSA wc = { 0 };
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = map_wndproc;
        wc.hInstance = GetModuleHandleA(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(1));
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = "velocity_carte";
        RegisterClassA(&wc);
        registered = 1;
    }
    mainRC = wglGetCurrentContext();
    mapWnd = CreateWindowExA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, "velocity_carte", "VELOCITY - carte", WS_POPUP,
                             f.r.left, f.r.top, w, h, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!mapWnd) return 0;
    mapDC = GetDC(mapWnd);
    PIXELFORMATDESCRIPTOR pfd = { sizeof pfd, 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
                                  PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 24, 8, 0,
                                  PFD_MAIN_PLANE, 0, 0, 0, 0 };   /* meme format que la fenetre du jeu (partage) */
    SetPixelFormat(mapDC, ChoosePixelFormat(mapDC, &pfd), &pfd);
    mapRC = wglCreateContext(mapDC);
    if (!mapRC || !wglShareLists(mainRC, mapRC)) {        /* sans partage : pas de police ni de textures, on renonce */
        if (mapRC) wglDeleteContext(mapRC);
        ReleaseDC(mapWnd, mapDC); DestroyWindow(mapWnd);
        mapWnd = NULL; mapRC = NULL;
        return 0;
    }
    ShowWindow(mapWnd, SW_SHOWNOACTIVATE);
    mapRect = f.r;
    wglMakeCurrent(mapDC, mapRC);                         /* pas de synchro verticale ici : elle ralentirait le jeu */
    typedef BOOL(WINAPI * SwapFn)(int);
    SwapFn swapInterval = (SwapFn)wglGetProcAddress("wglSwapIntervalEXT");
    if (swapInterval) swapInterval(0);
    wglMakeCurrent(hdc, mainRC);
    return 1;
}

static int map_begin(int *w, int *h) {
    if (!mapWnd) return 0;
    RECT cr;
    GetClientRect(mapWnd, &cr);
    *w = cr.right; *h = cr.bottom;
    wglMakeCurrent(mapDC, mapRC);
    return 1;
}

static void map_end(void) {
    SwapBuffers(mapDC);
    wglMakeCurrent(hdc, mainRC);
}
#else
static int map_update(int want) { return mac_map_update(want); }
static int map_begin(int *w, int *h) { return mac_map_begin(w, h); }
static void map_end(void) { mac_map_end(); }
#endif

/* appele a chaque image par la boucle principale */
static void second_screen_frame(float dt) {
    static float pollT, drawT;
    static int open;
    if (shotMode || botMode) return;
    pollT -= dt;
    if (pollT <= 0) { pollT = SCREEN_POLL_T; open = map_update(secondScreen); }
    if (!open) return;
    drawT -= dt;
    if (drawT > 0) return;
    drawT = 1.0f / SCREEN_MAP_FPS;
    int w, h;
    if (!map_begin(&w, &h)) return;
    draw_map_screen(w, h);
    map_end();
}
