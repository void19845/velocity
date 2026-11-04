/* VELOCITY - impact frames, chiffres de degats, eclats ; lecture des entrees (input_poll)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Effets visuels de combat                                            */
/* ------------------------------------------------------------------ */
static float impactT, impactDur, killMarkT, muzzleT, hurtDirT;
static V3 impactAt, hurtFrom;

/* Impact frame : quelques images en couleurs inversees + traits radiaux, sur les gros coups */
static void impact_frame(V3 at, float dur) {
    if (!impactFrames) return;
    if (dur > impactT) { impactT = impactDur = dur; impactAt = at; }
}

enum { DN_NORMAL, DN_HEAD, DN_ONESHOT };                 /* genre du chiffre : corps, tete (dore), one-shot (rouge et or) */
typedef struct { V3 pos; float val, t; int kind; } DmgNum;   /* chiffres de degats flottants */
#define MAXDN 32
static DmgNum DN[MAXDN];
static int dnNext;
static void dmg_number_k(V3 pos, float val, int kind) {
    DmgNum *d = &DN[dnNext];
    dnNext = (dnNext + 1) % MAXDN;
    d->pos = v3(pos.x + frange(-0.3f, 0.3f), pos.y + 0.6f, pos.z + frange(-0.3f, 0.3f));
    d->val = val; d->t = kind ? 1.2f : 0.9f; d->kind = kind;
}
static void dmg_number(V3 pos, float val) { dmg_number_k(pos, val, DN_NORMAL); }

typedef struct { V3 pos, vel, col, axis; float ang, spin, life, size; } Shard;   /* eclats physiques */
#define MAXSH 256
static Shard SH[MAXSH];
static int shNext;
static void shards(V3 p, V3 col, int n, float speed) {
    for (int i = 0; i < n; i++) {
        Shard *h = &SH[shNext];
        shNext = (shNext + 1) % MAXSH;
        h->pos = p; h->col = col;
        h->vel = vadd(vmul(rand_dir(), speed * frange(0.4f, 1.0f)), v3(0, speed * 0.5f, 0));
        h->axis = rand_dir(); h->ang = frange(0, 360); h->spin = frange(200, 900);
        h->life = frange(1.5f, 2.5f); h->size = frange(0.08f, 0.2f);
    }
}
static void shard_tick(float dt) {
    for (int i = 0; i < MAXSH; i++) {
        Shard *h = &SH[i];
        if (h->life <= 0) continue;
        h->life -= dt;
        h->vel.y -= 22 * dt;
        h->ang += h->spin * dt;
        V3 step = vmul(h->vel, dt);
        float len = vlen(step);
        if (len < 1e-5f) continue;
        V3 n;
        if (ray_world(h->pos, vmul(step, 1.0f / len), len, &n) < len) {   /* rebond amorti */
            float vn = vdot(h->vel, n);
            h->vel = vmul(vsub(h->vel, vmul(n, 1.6f * vn)), 0.6f);
            h->spin *= 0.6f;
        } else h->pos = vadd(h->pos, step);
    }
}

static int bound_held(int a) { return (binds[a][0] && keys[binds[a][0]]) || (binds[a][1] && keys[binds[a][1]]); }

static void input_poll(void) {
    for (int a = 0; a < A_COUNT; a++) {
        act[a] = botHeld[a]; actP[a] = botPress[a];
        for (int k = 0; k < 2; k++) {
            int c = binds[a][k];
            if (c > 0) { act[a] |= keys[c]; actP[a] |= latch[c]; }
        }
    }
    memset(botPress, 0, sizeof botPress);
    memset(latch, 0, sizeof latch);
}
static int hookPressIn, hookHeldIn;                       /* entrees du grappin pour le tick en cours */
static float mdx, mdy, sens = 0.0022f;
static int captured, shotMode;

#ifdef _WIN32
static HWND hwnd;
static HDC hdc;
#endif
static int winW = 1600, winH = 900;

static void set_capture(int on) {
    if (on == captured) return;
    captured = on;
#ifndef _WIN32
    mac_set_capture(on);
    if (!on) memset(keys, 0, sizeof keys);
#else
    if (on) {
        RECT r; GetClientRect(hwnd, &r);
        POINT a = { r.left, r.top }, b = { r.right, r.bottom };
        ClientToScreen(hwnd, &a); ClientToScreen(hwnd, &b);
        RECT c = { a.x, a.y, b.x, b.y };
        ClipCursor(&c);
        while (ShowCursor(FALSE) >= 0) {}
        SetCapture(hwnd);
    } else {
        ClipCursor(NULL);
        while (ShowCursor(TRUE) < 0) {}
        ReleaseCapture();
        memset(keys, 0, sizeof keys);
    }
#endif
    mdx = mdy = 0;
}
