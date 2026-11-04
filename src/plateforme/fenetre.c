/* VELOCITY - fenetre, entrees clavier/souris (Windows et macOS)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Fenetre                                                             */
/* ------------------------------------------------------------------ */
#ifdef _WIN32
static WINDOWPLACEMENT prevPlacement = { sizeof(WINDOWPLACEMENT) };
#endif

static void toggle_fullscreen(void) {
#ifdef _WIN32
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);
    if (!fullscreen) {
        MONITORINFO mi = { sizeof mi };
        if (GetWindowPlacement(hwnd, &prevPlacement) && GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &prevPlacement);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
#else
    mac_set_fullscreen(!fullscreen);
#endif
    fullscreen = !fullscreen;
    if (captured) { set_capture(0); set_capture(1); }
}

/* --- gestionnaires d'entrees communs a Windows (wndproc) et macOS (mac_platform.m) --- */
static void in_resize(int w, int h) {
    winW = w; winH = h;
    if (captured) { set_capture(0); set_capture(1); }
}

/* renvoie 1 si la touche est consommee par l'attente d'une affectation (Alt+F4 ne passe alors pas) */
static int in_key_down(int sc, int repeat) {
    if (waitAct >= 0) {                                   /* on attend une touche pour une commande */
        if (!repeat) {
            if (sc == SC_ESC) waitAct = -1;
            else if (sc == 0x0E || sc == 0x53) assign_bind(0);   /* retour arriere / suppr : effacer */
            else assign_bind(sc);
        }
        return 1;
    }
    if (!repeat) {
        latch[sc] = 1;
        if (sc == SC_ESC) {
            if (uiFocus) uiFocus = 0;
            else if (settingsOpen) {                      /* GRAPHISMES -> PARAMETRES -> menu precedent */
                if (settingsPage) settingsPage = 0; else settingsOpen = 0;
                cfg_save();
            }
            else if (loadoutOpen) { loadoutOpen = 0; cfg_save(); }
            else if (hostOpen) { hostOpen = 0; cfg_save(); }
            else if (skinOpen) { skinOpen = 0; cfg_save(); }
            else if (appState == APP_JOIN) pendingAction = ACT_BACK;
            else if (vg_menu_open() && vgMenu == VGM_SHOP) {  /* VAGUES : ECHAP ferme la boutique */
                if (vgShopBack == VGM_SELECT) vgMenu = VGM_SELECT; else vg_menu_close();
                vgShopBack = VGM_NONE;
            }
            else set_capture(0);
        }
        if (sc == SC_F11) toggle_fullscreen();
        if (sc == SC_F5 && !botMode) gfx_reload();       /* graphismes/rendu.ini relu a chaud */
        if (sc == SC_F2) sens = fmaxf(0.0003f, sens - 0.0002f);
        if (sc == SC_F3) sens = fminf(0.01f, sens + 0.0002f);
        if (sc == SC_R && !captured && !uiFocus && appState == APP_GAME && !dmMode && !vg_menu_open()) reset_game();
    }
    keys[sc] = 1;
    return 0;
}

static void in_key_up(int sc) { keys[sc] = 0; }

static void in_char(int c) {                              /* saisie du pseudo et de l'IP */
    if (!uiFocus) return;
    if (appState == APP_GAME && captured) { uiFocus = 0; return; }   /* en jeu, les touches ne vont jamais au pseudo */
    char *f = uiFocus == 1 ? myName : ipField;
    int cap = uiFocus == 1 ? 15 : 31, len = (int)strlen(f);
    if (c == 8) { if (len) f[len - 1] = 0; }
    else if (c == 13) { if (uiFocus == 2) pendingAction = ACT_JOIN_IP; uiFocus = 0; }
    else if (c >= 32 && c < 127 && len < cap && (uiFocus == 1 || (c >= '0' && c <= '9') || c == '.')) {
        f[len] = (char)c; f[len + 1] = 0;
    }
}

static void in_mouse_move(int x, int y) { uiMX = x; uiMY = y; }

static void in_button_down(int code) {                    /* code = K_LMB .. K_X2 */
    if (waitAct >= 0) { assign_bind(code); return; }
    if (!captured) {                                      /* les menus traitent le clic gauche */
        if (code == K_LMB) { uiClick = 1; uiDown = 1; }
        return;
    }
    keys[code] = 1; latch[code] = 1;
}

static void in_button_up(int code) {
    keys[code] = 0;
    if (code == K_LMB) { uiDown = 0; uiDrag = 0; }
}

/* Molette : un geste (une rafale d'evenements, inertie comprise) = un seul appui. Un evenement n'est garde que
   si la molette etait au repos depuis WHEEL_QUIET et que le dernier appui date d'au moins WHEEL_COOLDOWN. */
static double wheelLastEv = -1e9, wheelLastOk = -1e9;
static int wheel_accept(double t) {
    int ok = t - wheelLastEv >= WHEEL_QUIET && t - wheelLastOk >= WHEEL_COOLDOWN;
    wheelLastEv = t;
    if (ok) wheelLastOk = t;
    return ok;
}

static void in_wheel(void) {
    LARGE_INTEGER n, fq;
    QueryPerformanceCounter(&n); QueryPerformanceFrequency(&fq);
    if (!wheel_accept((double)n.QuadPart / (double)fq.QuadPart)) return;
    if (waitAct >= 0) assign_bind(K_WHEEL);
    else if (captured) latch[K_WHEEL] = 1;
}

static void modal_frame(void);                           /* plateforme/demarrage.c */
#ifdef _WIN32
#define MODAL_TIMER 1                                     /* boucle modale (deplacement, redimensionnement, menu) : ~120 images/s */
static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_ENTERSIZEMOVE: case WM_ENTERMENULOOP: SetTimer(h, MODAL_TIMER, 8, NULL); return 0;
    case WM_EXITSIZEMOVE: case WM_EXITMENULOOP: KillTimer(h, MODAL_TIMER); return 0;
    case WM_TIMER: if (wp == MODAL_TIMER) modal_frame(); return 0;
    case WM_SIZE:
        in_resize(LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_INPUT: {
        BYTE data[sizeof(RAWINPUT) + 16];
        UINT sz = sizeof data;
        if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, data, &sz, sizeof(RAWINPUTHEADER)) != (UINT)-1) {
            RAWINPUT *ri = (RAWINPUT *)data;
            if (ri->header.dwType == RIM_TYPEMOUSE && !(ri->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
                mdx += (float)ri->data.mouse.lLastX;
                mdy += (float)ri->data.mouse.lLastY;
            }
        }
        break;
    }
    case WM_KEYDOWN: case WM_SYSKEYDOWN:
        if (in_key_down((lp >> 16) & 0xFF, (lp & (1 << 30)) != 0)) return 0;
        if (m == WM_SYSKEYDOWN && wp == VK_F4) break;      /* Alt+F4 */
        return 0;
    case WM_KEYUP: case WM_SYSKEYUP:
        in_key_up((lp >> 16) & 0xFF);
        return 0;
    case WM_CHAR:
        in_char((int)wp);
        return 0;
    case WM_MOUSEMOVE:
        in_mouse_move((short)LOWORD(lp), (short)HIWORD(lp));
        return 0;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: case WM_XBUTTONDOWN: {
        int code = m == WM_LBUTTONDOWN ? K_LMB : m == WM_RBUTTONDOWN ? K_RMB : m == WM_MBUTTONDOWN ? K_MMB :
                   (HIWORD(wp) == XBUTTON1 ? K_X1 : K_X2);
        in_button_down(code);
        return m == WM_XBUTTONDOWN;
    }
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: case WM_XBUTTONUP: {
        int code = m == WM_LBUTTONUP ? K_LMB : m == WM_RBUTTONUP ? K_RMB : m == WM_MBUTTONUP ? K_MMB :
                   (HIWORD(wp) == XBUTTON1 ? K_X1 : K_X2);
        in_button_up(code);
        return m == WM_XBUTTONUP;
    }
    case WM_MOUSEWHEEL:
        in_wheel();
        return 0;
    case WM_ACTIVATE: if (LOWORD(wp) == WA_INACTIVE) set_capture(0); return 0;
    case WM_KILLFOCUS: set_capture(0); return 0;
    case WM_CLOSE: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(h, m, wp, lp);
}
#else
/* rappels appeles par mac_platform.m */
void plat_key(int sc, int down, int repeat) { if (down) in_key_down(sc, repeat); else in_key_up(sc); }
void plat_char(int ch) { in_char(ch); }
void plat_mouse_move(int x, int y) { in_mouse_move(x, y); }
void plat_button(int id, int down) { if (down) in_button_down(K_LMB + id); else in_button_up(K_LMB + id); }
void plat_wheel(void) { in_wheel(); }
void plat_focus_lost(void) { set_capture(0); }
void plat_resize(int w, int h) { in_resize(w, h); }
void plat_raw_motion(float dx, float dy) { mdx += dx; mdy += dy; }
#endif
