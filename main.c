/*
 * VELOCITY - movement shooter 3D en C
 * Win32 + OpenGL 1.1 + waveOut, aucune dependance externe.
 *
 * Mouvement : bhop (maintenir saut), air-strafe facon Quake, slide, dash,
 *             double saut, wall-run, wall-jump, jump pads, rocket-jump.
 * Armes     : 1 = Railgun (hitscan perforant), 2 = Lance-roquettes (splash + knockback).
 */
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <mmsystem.h>
#include <GL/gl.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "advapi32.lib")

#define PI   3.14159265f
#define TICK (1.0f / 120.0f)

/* ------------------------------------------------------------------ */
/* Maths                                                               */
/* ------------------------------------------------------------------ */
typedef struct { float x, y, z; } V3;
static V3 v3(float x, float y, float z) { V3 r = { x, y, z }; return r; }
static V3 vadd(V3 a, V3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static V3 vsub(V3 a, V3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static V3 vmul(V3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static float vdot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 vcross(V3 a, V3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static float vlen(V3 a) { return sqrtf(vdot(a, a)); }
static V3 vnorm(V3 a) { float l = vlen(a); return l > 1e-6f ? vmul(a, 1.0f / l) : v3(0, 0, 0); }
static V3 vlerp(V3 a, V3 b, float t) { return vadd(a, vmul(vsub(b, a), t)); }
static float *vc(V3 *v, int i) { return &v->x + i; }
static float clampf(float x, float a, float b) { return x < a ? a : x > b ? b : x; }
static float hlen(V3 v) { return sqrtf(v.x * v.x + v.z * v.z); }

static unsigned int rng = 12345;
static float frand(void) { rng = rng * 1664525u + 1013904223u; return (rng >> 8) * (1.0f / 16777216.0f); }
static float frange(float a, float b) { return a + (b - a) * frand(); }
static V3 rand_dir(void) {
    for (;;) {
        V3 v = v3(frange(-1, 1), frange(-1, 1), frange(-1, 1));
        float l = vdot(v, v);
        if (l > 0.01f && l <= 1.0f) return vmul(v, 1.0f / sqrtf(l));
    }
}

/* ------------------------------------------------------------------ */
/* Reglages du mouvement (metres, secondes)                            */
/* ------------------------------------------------------------------ */
#define HW             0.4f   /* demi-largeur joueur */
#define H_STAND        1.8f
#define H_CROUCH       1.1f
#define EYE_STAND      1.6f
#define EYE_CROUCH     0.95f
#define STEP_H         0.55f
#define GRAVITY        24.0f
#define JUMP_V         8.4f
#define RUN_SPEED      9.5f
#define CROUCH_SPEED   4.5f
#define GROUND_ACCEL   12.0f
#define FRICTION       6.0f
#define STOPSPEED      3.0f
#define AIR_ACCEL      14.0f
#define AIR_CAP        1.0f
#define SLIDE_MIN      6.5f
#define SLIDE_END      3.0f
#define SLIDE_BOOST    4.5f
#define SLIDE_FRICTION 0.7f
#define SLIDE_CD       1.0f
#define DASH_SPEED     18.0f
#define DASH_RECHARGE  1.6f
#define WALLJUMP_PUSH  8.0f
#define WALLJUMP_UP    8.8f
#define WALLRUN_MIN    6.0f
#define WALLRUN_MAX    1.8f
#define BASE_HFOV      105.0f

#define GRAPPLE_RANGE    55.0f   /* portee max du crochet */
#define GRAPPLE_SPEED    120.0f  /* vitesse de vol du crochet */
#define GRAPPLE_PULL     70.0f   /* acceleration vers le point d'accroche */
#define GRAPPLE_MAXSPEED 38.0f   /* plus de traction au-dela de cette vitesse vers l'ancre */
#define GRAPPLE_CD       0.5f
#define GRAPPLE_JUMP     5.0f    /* boost vertical quand on lache en sautant */
#define GRAPPLE_ENEMY_PULL     95.0f  /* traction plus forte vers un ennemi */
#define GRAPPLE_ENEMY_MAXSPEED 46.0f
#define GRAPPLE_STRIKE_DMG     50.0f  /* degats de l'impact... */
#define GRAPPLE_STRIKE_PER_MS  3.0f   /* ...plus 3 par m/s de vitesse d'impact */

#define RAIL_CD        0.85f
#define RAIL_AMMO_START 5       /* munitions du railgun au (re)spawn */
#define RAIL_AMMO_MAX   10
#define PICKUP_AMMO     3       /* une cellule = 3 tirs */
#define PICKUP_RESPAWN  12.0f

/* Pieces facon Marksman (Ultrakill) : on lance une piece, un tir de rail qui la touche ricoche
 * tout seul vers la cible la plus proche. Pense pour viser a pleine vitesse. */
#define COIN_MAX        4
#define COIN_RECHARGE   2.0f    /* secondes par piece */
#define COIN_RADIUS     0.9f    /* grosse zone de touche */
#define COIN_MULT       1.5f    /* chaque ricochet multiplie les degats */
#define COIN_GRAVITY    16.0f
#define COIN_SEEK_RANGE 90.0f
#define RAIL_DMG       100.0f
#define ROCKET_CD      0.75f
#define ROCKET_SPEED   38.0f
#define ROCKET_DMG     110.0f
#define ROCKET_RADIUS  4.5f
#define ROCKET_KNOCK   13.0f

/* Axe corps a corps : degats et portee montent avec la vitesse */
#define MELEE_CD          0.45f
#define MELEE_RANGE       2.4f
#define MELEE_RANGE_PER_MS 0.12f  /* +0.12 m de portee par m/s */
#define MELEE_DMG         35.0f
#define MELEE_DMG_PER_MS  4.0f    /* +4 degats par m/s */
#define MELEE_CONE        0.55f   /* cosinus du demi-angle du coup */
#define PARRY_SPEED       36.0f   /* renvoi esquivable (ou re-parable) */
#define PARRY_DMG         150.0f  /* orbes renvoyees en solo */
#define PARRY_HEAL        10.0f
#define PARRY_WINDOW      0.12f   /* le parry n'est actif qu'au tout debut du coup */
#define MELEE_WHIFF_CD    0.9f    /* coup qui ne touche ni ne pare rien : recharge plus longue */
#define PARRY_ROCKET_MULT 1.25f   /* roquette renvoyee : 1,25x les degats d'origine */
#define PARRY_PINGPONG    1.15f   /* chaque re-parry : +15 % de vitesse et de degats */
#define PARRY_RANGE_SHRINK 0.85f  /* ... et une zone de parry 15 % plus courte */
#define PARRY_AIM_COS     0.988f  /* aide a la visee du renvoi : ~9 degres */
#define PVP_HIT_CAP       60.0f   /* en PvP, aucune touche ne retire plus de 60 PV */
#define MELEE_PARRIED_CD  1.0f    /* attaquant dont l'epee est paree : epee bloquee */
#define MELEE_PARRY_GRACE 0.15f   /* tolerance de latence pour l'hote */

/* Axe tir : chaque touche successive augmente degats et cadence, un tir rate remet a zero */
#define STREAK_MAX        8
#define STREAK_DMG        0.15f   /* +15% de degats par touche */
#define STREAK_RATE       0.05f   /* +5% de cadence par touche */

/* ------------------------------------------------------------------ */
/* Audio : mini mixeur waveOut + sons synthetises                      */
/* ------------------------------------------------------------------ */
#define SR      44100
#define NBUF    4
#define BUFLEN  512
#define MAXV    32
enum { S_RAIL, S_ROCKET, S_EXPLO, S_JUMP, S_LAND, S_DASH, S_HIT, S_KILL, S_HURT,
       S_ESHOT, S_PAD, S_WAVE, S_WALLJUMP, S_SLIDE, S_HOOK, S_HOOKHIT, S_PUNCH, S_PARRY, S_COUNT };
typedef struct { short *data; int len; } Sound;
typedef struct { int snd, active; float pos, step, vol; } Voice;
static Sound sounds[S_COUNT];
static Voice voices[MAXV];
static HWAVEOUT wout;
static WAVEHDR whdr[NBUF];
static short wbuf[NBUF][BUFLEN];
static int audio_ok;
static float masterVol = 1.0f;

static void play(int s, float vol, float pitch) {
    if (!audio_ok) return;
    int best = 0; float oldest = -1;
    for (int i = 0; i < MAXV; i++) {
        if (!voices[i].active) { best = i; break; }
        if (voices[i].pos > oldest) { oldest = voices[i].pos; best = i; }
    }
    voices[best].snd = s; voices[best].active = 1; voices[best].pos = 0;
    voices[best].step = pitch; voices[best].vol = vol;
}

static void audio_fill(short *out, int n) {
    for (int i = 0; i < n; i++) {
        float acc = 0;
        for (int v = 0; v < MAXV; v++) {
            Voice *vo = &voices[v];
            if (!vo->active) continue;
            Sound *s = &sounds[vo->snd];
            int p = (int)vo->pos;
            if (p >= s->len - 1) { vo->active = 0; continue; }
            float f = vo->pos - p;
            acc += (s->data[p] * (1 - f) + s->data[p + 1] * f) * vo->vol;
            vo->pos += vo->step;
        }
        out[i] = (short)(tanhf(acc / 32768.0f * 0.7f * masterVol) * 32000.0f);
    }
}

static void audio_update(void) {
    if (!audio_ok) return;
    for (int i = 0; i < NBUF; i++)
        if (whdr[i].dwFlags & WHDR_DONE) {
            audio_fill(wbuf[i], BUFLEN);
            waveOutWrite(wout, &whdr[i], sizeof(WAVEHDR));
        }
}

static float noise(void) { return frand() * 2 - 1; }
static float *sbuf(float secs, int *n) { *n = (int)(secs * SR); return (float *)calloc(*n, sizeof(float)); }
static Sound mksound(float *f, int n) {
    Sound s; s.len = n; s.data = (short *)malloc(n * sizeof(short));
    for (int i = 0; i < n; i++) s.data[i] = (short)(clampf(f[i], -1, 1) * 32000.0f);
    free(f);
    return s;
}

static void make_sounds(void) {
    int n; float *b, ph, lp, t, a;
    /* railgun : zap descendant + crack */
    b = sbuf(0.6f, &n); ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        ph += (200 + 2200 * expf(-t * 9)) / SR;
        float sq = sinf(ph * 2 * PI) + 0.4f * sinf(ph * 4 * PI) + 0.25f * sinf(ph * 6 * PI);
        b[i] = sq * 0.3f * expf(-t * 6) + noise() * 0.45f * expf(-t * 28) + sinf(t * 2 * PI * 55) * 0.5f * expf(-t * 9);
    }
    sounds[S_RAIL] = mksound(b, n);
    /* roquette : souffle grave */
    b = sbuf(0.5f, &n); lp = 0; ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        lp += 0.08f * (noise() - lp);
        ph += (110 - 60 * t) / SR;
        b[i] = lp * 2.2f * expf(-t * 5) + sinf(ph * 2 * PI) * 0.45f * expf(-t * 8);
    }
    sounds[S_ROCKET] = mksound(b, n);
    /* explosion */
    b = sbuf(1.3f, &n); lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        float fc = 200 + 3500 * expf(-t * 5);
        a = 1 - expf(-2 * PI * fc / SR);
        lp += a * (noise() - lp);
        b[i] = lp * 1.6f * expf(-t * 3.2f) + sinf(t * 2 * PI * (48 - 10 * t)) * 0.8f * expf(-t * 5);
    }
    sounds[S_EXPLO] = mksound(b, n);
    /* saut */
    b = sbuf(0.12f, &n); ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR; ph += (280 + 2500 * t) / SR;
        b[i] = sinf(ph * 2 * PI) * 0.35f * (1 - t / 0.12f);
    }
    sounds[S_JUMP] = mksound(b, n);
    /* atterrissage */
    b = sbuf(0.18f, &n); lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR; lp += 0.05f * (noise() - lp);
        b[i] = (lp * 2.5f + sinf(t * 2 * PI * 70) * 0.6f) * expf(-t * 25);
    }
    sounds[S_LAND] = mksound(b, n);
    /* dash : souffle montant */
    b = sbuf(0.35f, &n); lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        a = 1 - expf(-2 * PI * (400 + 5000 * t) / SR);
        lp += a * (noise() - lp);
        float env = t < 0.03f ? t / 0.03f : expf(-(t - 0.03f) * 9);
        b[i] = lp * 0.9f * env;
    }
    sounds[S_DASH] = mksound(b, n);
    /* hitmarker */
    b = sbuf(0.07f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        b[i] = (sinf(t * 2 * PI * 1500) + 0.6f * sinf(t * 2 * PI * 2300)) * 0.3f * expf(-t * 60);
    }
    sounds[S_HIT] = mksound(b, n);
    /* kill : arpege */
    b = sbuf(0.4f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        float f = t < 0.08f ? 880.0f : (t < 0.16f ? 1175.0f : 1760.0f);
        b[i] = (sinf(t * 2 * PI * f) + 0.3f * sinf(t * 4 * PI * f)) * 0.25f * expf(-t * 7);
    }
    sounds[S_KILL] = mksound(b, n);
    /* degats joueur */
    b = sbuf(0.28f, &n); ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR; ph += 95.0f / SR;
        float saw = 2 * (ph - floorf(ph)) - 1;
        b[i] = (saw * 0.5f + noise() * 0.3f) * expf(-t * 12);
    }
    sounds[S_HURT] = mksound(b, n);
    /* tir ennemi */
    b = sbuf(0.22f, &n); ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR; ph += (900 - 2500 * t) / SR;
        b[i] = sinf(ph * 2 * PI) * 0.35f * expf(-t * 12);
    }
    sounds[S_ESHOT] = mksound(b, n);
    /* jump pad */
    b = sbuf(0.5f, &n); ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR; ph += (180 + 1100 * t + 30 * sinf(t * 60)) / SR;
        b[i] = sinf(ph * 2 * PI) * 0.4f * expf(-t * 4);
    }
    sounds[S_PAD] = mksound(b, n);
    /* nouvelle vague : accord */
    b = sbuf(1.0f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        float s = 0, fr[3] = { 220.0f, 277.2f, 329.6f };
        for (int k = 0; k < 3; k++) { float p = t * fr[k]; s += (p - floorf(p)) < 0.5f ? 1.0f : -1.0f; }
        b[i] = s * 0.07f * (t < 0.02f ? t / 0.02f : expf(-(t - 0.02f) * 3));
    }
    sounds[S_WAVE] = mksound(b, n);
    /* wall-jump */
    b = sbuf(0.16f, &n); ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR; ph += (420 + 3000 * t) / SR;
        b[i] = (sinf(ph * 2 * PI) * 0.3f + noise() * 0.25f * expf(-t * 40)) * (1 - t / 0.16f);
    }
    sounds[S_WALLJUMP] = mksound(b, n);
    /* slide */
    b = sbuf(0.45f, &n); lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR; lp += 0.15f * (noise() - lp);
        b[i] = lp * 1.2f * (t < 0.02f ? t / 0.02f : expf(-(t - 0.02f) * 5));
    }
    sounds[S_SLIDE] = mksound(b, n);
    /* grappin : zip du cable */
    b = sbuf(0.2f, &n); lp = 0; ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        a = 1 - expf(-2 * PI * (1500 + 6000 * t) / SR);
        lp += a * (noise() - lp);
        ph += (600 + 1800 * t) / SR;
        b[i] = (lp * 0.6f + sinf(ph * 2 * PI) * 0.15f) * (1 - t / 0.2f);
    }
    sounds[S_HOOK] = mksound(b, n);
    /* grappin : accroche metallique */
    b = sbuf(0.3f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        b[i] = (sinf(t * 2 * PI * 1250) * 0.3f + sinf(t * 2 * PI * 1870) * 0.22f + sinf(t * 2 * PI * 2710) * 0.15f) * expf(-t * 14) +
               noise() * 0.4f * expf(-t * 80);
    }
    sounds[S_HOOKHIT] = mksound(b, n);
    /* coup de poing : impact sourd */
    b = sbuf(0.25f, &n); lp = 0; ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        lp += 0.12f * (noise() - lp);
        ph += (140 - 250 * t) / SR;
        b[i] = (lp * 2.0f * expf(-t * 30) + sinf(ph * 2 * PI) * 0.8f * expf(-t * 14));
    }
    sounds[S_PUNCH] = mksound(b, n);
    /* parry : ding metallique brillant */
    b = sbuf(0.9f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        b[i] = (sinf(t * 2 * PI * 1568) * 0.3f + sinf(t * 2 * PI * 2349) * 0.2f + sinf(t * 2 * PI * 3136) * 0.12f) * expf(-t * 4) +
               noise() * 0.5f * expf(-t * 60);
    }
    sounds[S_PARRY] = mksound(b, n);
}

static void audio_init(void) {
    WAVEFORMATEX fmt = { 0 };
    fmt.wFormatTag = WAVE_FORMAT_PCM; fmt.nChannels = 1; fmt.nSamplesPerSec = SR;
    fmt.wBitsPerSample = 16; fmt.nBlockAlign = 2; fmt.nAvgBytesPerSec = SR * 2;
    if (waveOutOpen(&wout, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return;
    audio_ok = 1;
    for (int i = 0; i < NBUF; i++) {
        memset(&whdr[i], 0, sizeof(WAVEHDR));
        whdr[i].lpData = (LPSTR)wbuf[i];
        whdr[i].dwBufferLength = BUFLEN * sizeof(short);
        waveOutPrepareHeader(wout, &whdr[i], sizeof(WAVEHDR));
        audio_fill(wbuf[i], BUFLEN);
        waveOutWrite(wout, &whdr[i], sizeof(WAVEHDR));
    }
}

/* ------------------------------------------------------------------ */
/* Monde : une liste d'AABB                                            */
/* ------------------------------------------------------------------ */
enum { B_SOLID, B_PAD };
typedef struct { V3 mn, mx, col, launch; int kind; } Box;
#define MAXB 256
static Box boxes[MAXB];
static int nb;

static void addbox(float x0, float y0, float z0, float x1, float y1, float z1, float r, float g, float b) {
    if (nb >= MAXB) return;
    Box *bx = &boxes[nb++];
    bx->mn = v3(x0, y0, z0); bx->mx = v3(x1, y1, z1); bx->col = v3(r, g, b);
    bx->kind = B_SOLID; bx->launch = v3(0, 0, 0);
}

static void addpad(float x, float z, V3 launch) {
    addbox(x - 1.5f, 0, z - 1.5f, x + 1.5f, 0.3f, z + 1.5f, 0.2f, 0.9f, 1.0f);
    boxes[nb - 1].kind = B_PAD;
    boxes[nb - 1].launch = launch;
}

static int near2(float x, float z, float cx, float cz, float r) {
    return (x - cx) * (x - cx) + (z - cz) * (z - cz) < r * r;
}

/* La carte est dessinee a l'echelle 1 puis agrandie horizontalement (les hauteurs ne changent pas) */
#define MAP_S 1.4f
static float ms(float v) { return v * MAP_S; }

/* recharges du railgun posees sur la carte ; t > 0 : deja ramassee, revient dans t secondes */
#define MAXPICK 16
typedef struct { V3 pos; float t, pending; } Pickup;
static Pickup PU[MAXPICK];
static int npu;

static void addpickup(float x, float y, float z) {
    if (npu >= MAXPICK) return;
    PU[npu].pos = v3(ms(x), y + 0.9f, ms(z));
    PU[npu].t = PU[npu].pending = 0;
    npu++;
}

static void build_level(void) {
    float W = ms(56);
    nb = 0; npu = 0;
    addbox(-W, -2, -W, W, 0, W, 0.52f, 0.54f, 0.60f);              /* sol */
    addbox(-W - 2, 0, -W - 2, W + 2, 24, -W, 0.40f, 0.42f, 0.52f);   /* murs exterieurs */
    addbox(-W - 2, 0, W, W + 2, 24, W + 2, 0.40f, 0.42f, 0.52f);
    addbox(-W - 2, 0, -W, -W, 24, W, 0.40f, 0.42f, 0.52f);
    addbox(W, 0, -W, W + 2, 24, W, 0.40f, 0.42f, 0.52f);

    addbox(-ms(8), 0, -ms(8), ms(8), 4, ms(8), 0.78f, 0.56f, 0.40f);   /* plateforme centrale */
    for (int i = 0; i < 10; i++) {                                  /* escaliers N et S */
        float h = 0.4f * (i + 1), z0 = ms(8 + (9 - i) * 0.6f), z1 = ms(8 + (10 - i) * 0.6f);
        float c = 0.62f + 0.012f * i;
        addbox(-ms(3), 0, z0, ms(3), h, z1, c, 0.5f, 0.38f);
        addbox(-ms(3), 0, -z1, ms(3), h, -z0, c, 0.5f, 0.38f);
    }
    addbox(-ms(2), 4, -ms(2), ms(2), 13, ms(2), 0.55f, 0.55f, 0.66f);   /* tour centrale */

    for (int sx = -1; sx <= 1; sx += 2)                             /* piliers */
        for (int sz = -1; sz <= 1; sz += 2)
            addbox(ms(sx * 24 - 2.0f), 0, ms(sz * 24 - 2.0f), ms(sx * 24 + 2.0f), 16, ms(sz * 24 + 2.0f), 0.5f, 0.5f, 0.62f);

    addbox(ms(28), 6, -ms(8), ms(44), 7, ms(8), 0.30f, 0.62f, 0.64f);   /* plateformes hautes */
    addbox(-ms(44), 6, -ms(8), -ms(28), 7, ms(8), 0.30f, 0.62f, 0.64f);
    addbox(-ms(8), 6, ms(28), ms(8), 7, ms(44), 0.30f, 0.62f, 0.64f);
    addbox(-ms(8), 6, -ms(44), ms(8), 7, -ms(28), 0.30f, 0.62f, 0.64f);

    for (int sx = -1; sx <= 1; sx += 2)                             /* perchoirs, accessibles au grappin */
        for (int sz = -1; sz <= 1; sz += 2)
            addbox(sx * ms(36) - 4, 12, sz * ms(36) - 4, sx * ms(36) + 4, 13, sz * ms(36) + 4, 0.85f, 0.7f, 0.35f);

    addbox(ms(30), 0, ms(38), ms(50), 8, ms(38) + 1, 0.75f, 0.42f, 0.62f);   /* murs de wall-run */
    addbox(-ms(50), 0, -ms(38) - 1, -ms(30), 8, -ms(38), 0.75f, 0.42f, 0.62f);
    addbox(ms(38), 0, -ms(50), ms(38) + 1, 8, -ms(30), 0.75f, 0.42f, 0.62f);
    addbox(-ms(38) - 1, 0, ms(30), -ms(38), 8, ms(50), 0.75f, 0.42f, 0.62f);

    addpad(ms(16), 0, v3(ms(13), 20, 0));                           /* jump pads */
    addpad(-ms(16), 0, v3(-ms(13), 20, 0));
    addpad(ms(9), ms(16), v3(-ms(1.8f), 20, ms(13)));
    addpad(-ms(9), -ms(16), v3(ms(1.8f), 20, -ms(13)));

    addpickup(0, 13, 0);                                            /* recharges : sommet de la tour, */
    addpickup(36, 7, 0); addpickup(-36, 7, 0);                      /* plateformes hautes, */
    addpickup(0, 7, 36); addpickup(0, 7, -36);
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) {
            addpickup(sx * 36.0f, 13, sz * 36.0f);                  /* perchoirs */
            addpickup(sx * 46.0f, 0, sz * 46.0f);                   /* et coins au sol */
        }

    rng = 777;                                                      /* blocs de couverture */
    for (int k = 0, placed = 0; k < 400 && placed < 34; k++) {
        float x = frange(-ms(50), ms(50)), z = frange(-ms(50), ms(50));
        float w = frange(1.5f, 3.2f), d = frange(1.5f, 3.2f), h = frange(1.0f, 3.5f);
        if (fabsf(x) < ms(13) && fabsf(z) < ms(18)) continue;
        if (near2(x, z, ms(16), 0, 8) || near2(x, z, -ms(16), 0, 8) || near2(x, z, ms(9), ms(16), 8) || near2(x, z, -ms(9), -ms(16), 8)) continue;
        if (near2(x, z, ms(24), ms(24), 9) || near2(x, z, -ms(24), ms(24), 9) || near2(x, z, ms(24), -ms(24), 9) || near2(x, z, -ms(24), -ms(24), 9)) continue;
        if (near2(x, z, 0, ms(24), 6)) continue;
        if (fabsf(fabsf(x) - ms(38.5f)) < 6 && fabsf(z) > ms(26)) continue;
        if (fabsf(fabsf(z) - ms(38.5f)) < 6 && fabsf(x) > ms(26)) continue;
        if (fabsf(fabsf(x) - ms(46)) < 4 && fabsf(fabsf(z) - ms(46)) < 4) continue;   /* recharges des coins */
        if (fabsf(x + 20) < 6 && z > 12 && z < 70) continue;        /* couloir degage pour les tests automatiques */
        float t = frand();
        addbox(x - w, 0, z - d, x + w, h, z + d, 0.45f + 0.2f * t, 0.48f, 0.58f - 0.12f * t);
        placed++;
    }
    rng = (unsigned)GetTickCount();
}

static int box_overlap(V3 mn, V3 mx, const Box *b) {
    return mn.x < b->mx.x && mx.x > b->mn.x && mn.y < b->mx.y && mx.y > b->mn.y && mn.z < b->mx.z && mx.z > b->mn.z;
}

static int ray_box(V3 o, V3 d, V3 mn, V3 mx, float maxt, float *tout, V3 *nout) {
    float tmin = 0, tmax = maxt; int axis = -1; float sign = 0;
    for (int a = 0; a < 3; a++) {
        float oa = *vc(&o, a), da = *vc(&d, a), lo = *vc(&mn, a), hi = *vc(&mx, a);
        if (fabsf(da) < 1e-8f) {
            if (oa < lo || oa > hi) return 0;
        } else {
            float inv = 1.0f / da, t1 = (lo - oa) * inv, t2 = (hi - oa) * inv, s = -1;
            if (t1 > t2) { float tt = t1; t1 = t2; t2 = tt; s = 1; }
            if (t1 > tmin) { tmin = t1; axis = a; sign = s; }
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax) return 0;
        }
    }
    *tout = tmin;
    if (nout) { *nout = v3(0, 0, 0); if (axis >= 0) *vc(nout, axis) = sign; else *nout = vmul(d, -1); }
    return 1;
}

static float ray_world(V3 o, V3 d, float maxt, V3 *n) {
    float best = maxt; V3 bn = v3(0, 1, 0);
    for (int i = 0; i < nb; i++) {
        float t; V3 nn;
        if (ray_box(o, d, boxes[i].mn, boxes[i].mx, best, &t, &nn) && t < best) { best = t; bn = nn; }
    }
    if (n) *n = bn;
    return best;
}

static int ray_sphere(V3 o, V3 d, V3 c, float r, float maxt, float *t) {
    V3 oc = vsub(o, c);
    float b = vdot(oc, d), cc = vdot(oc, oc) - r * r, h = b * b - cc;
    if (h < 0) return 0;
    h = sqrtf(h);
    float t0 = -b - h;
    if (t0 < 0) t0 = -b + h;
    if (t0 < 0 || t0 > maxt) return 0;
    *t = t0;
    return 1;
}

static int los(V3 a, V3 b) {
    V3 d = vsub(b, a); float l = vlen(d);
    if (l < 1e-4f) return 1;
    d = vmul(d, 1.0f / l);
    return ray_world(a, d, l, 0) >= l - 0.01f;
}

static void sphere_pushout(V3 *p, V3 *v, float r) {
    for (int i = 0; i < nb; i++) {
        Box *b = &boxes[i];
        if (p->x + r <= b->mn.x || p->x - r >= b->mx.x || p->y + r <= b->mn.y || p->y - r >= b->mx.y ||
            p->z + r <= b->mn.z || p->z - r >= b->mx.z) continue;
        float pen[6] = { p->x + r - b->mn.x, b->mx.x - (p->x - r), p->y + r - b->mn.y,
                         b->mx.y - (p->y - r), p->z + r - b->mn.z, b->mx.z - (p->z - r) };
        int k = 0;
        for (int j = 1; j < 6; j++) if (pen[j] < pen[k]) k = j;
        int ax = k / 2; float s = (k % 2) ? 1.0f : -1.0f;
        *vc(p, ax) += s * pen[k];
        if (*vc(v, ax) * s < 0) *vc(v, ax) = 0;
    }
}

/* ------------------------------------------------------------------ */
/* Entites                                                             */
/* ------------------------------------------------------------------ */
typedef struct {
    V3 pos, prevPos, vel, wallN;
    float yaw, pitch;
    int grounded, crouch, sliding, airJumps, wallrun, wallTouch, dead, groundBox, weapon;
    float dashCharge, hp, hurtFlash, slideCd, wallTime, wallJumpCd, coyote;
    float landDip, stepOfs, roll, fovKick, fovCur, eyeH, bobT, bob;
    float fireCd, switchT, recoil, deathT, swayX, swayY;
    int hook;                                             /* HOOK_NONE / FLYING / ATTACHED */
    int hookEnemy;                                        /* 0 = accroche au decor, sinon index ennemi + 1 */
    V3 hookPos, hookDir, anchor;
    float ropeLen, hookCd, hookDist, hookBlockT;
    float meleeCd, meleeT;
    float parryWin;                                       /* temps restant de la fenetre de parry */
    int swingLanded;                                      /* ce coup a-t-il touche ou pare quelque chose ? */
    int railAmmo;
    float coins;                                          /* charges de pieces (0..COIN_MAX) */
    int hookPlayer;                                       /* 0 = aucun, sinon id du joueur accroche + 1 (LAN) */
} Player;
enum { HOOK_NONE, HOOK_FLYING, HOOK_ATTACHED };
static Player P;

enum { EN_DRONE, EN_RUSHER };
typedef struct { V3 pos, vel; float hp, fireT, t, flash; int type, alive; } Enemy;
#define MAXE 64
static Enemy E[MAXE];

enum { PR_ROCKET, PR_ORB, PR_PARRIED };

typedef struct { V3 pos, vel; float life, spin; int alive, owner; unsigned int netId; } Coin;
#define MAXCOIN 24
static Coin CO[MAXCOIN];
static int lastChain;                                     /* longueur du dernier ricochet (tests, journal) */
typedef struct { V3 pos, vel; float life, mult; int kind, alive, owner, parries, orb; unsigned int netId; } Proj;
#define MAXPR 128
static Proj PR[MAXPR];

typedef struct { V3 pos, vel, col; float life, max, size, grav; } Part;
#define MAXPA 4096
static Part PA[MAXPA];
static int paNext;

typedef struct { V3 a, b, col; float life, max; } Beam;
#define MAXBM 32
static Beam BM[MAXBM];
static int bmNext;

static int wave, toSpawn, score, kills, combo, bestScore;
static float waveDelay, spawnT, gameTime, hitMarker, comboT, msgT, popT, shake;
static int shotStreak;                                    /* touches successives au tir */
static float hitstop, parryFlash, streakLostT;
static char msg[64], popText[32];

/* ------------------------------------------------------------------ */
/* Multijoueur LAN : etat partage                                      */
/* ------------------------------------------------------------------ */
#define MAXP          8
#define MAXSPEC       4       /* spectateurs en direct sur l'hote (28 Ko/s chacun) */
#define NET_PORT      27960
#define FRAG_LIMIT    15
#define MATCH_TIME    600.0f
#define RESPAWN_TIME  2.5f
#define PVP_SCALE     0.7f    /* degats contre les joueurs : 70% de ceux contre les monstres */
#define KILL_HEAL     30.0f

enum { NET_OFF, NET_HOST, NET_CLIENT };
enum { APP_MENU, APP_JOIN, APP_GAME };
enum { W_RAIL, W_ROCKET, W_MELEE, W_HOOK, W_PARRY, W_SELF, W_COIN };
enum { NF_CROUCH = 1, NF_SLIDE = 2, NF_WALLRUN = 4, NF_HOOK = 8, NF_DEAD = 16, NF_PARRY = 32 };

typedef struct {                  /* ce que chaque joueur envoie de lui-meme */
    float pos[3], vel[3], hookPos[3];
    float yaw, pitch;
    unsigned char flags, weapon, melee, pad;
} NetPState;

typedef struct {
    int active, alive, kills, deaths;
    float hp, respawnT;           /* l'hote fait autorite sur les PV et les respawns */
    char name[16];
    NetPState st;
    V3 disp;                      /* position affichee, lissee entre deux paquets */
    double recvT, lastHeard, lastParryT;
    float flashT;                                         /* flash blanc quand il est touche */                 /* lastParryT : derniere fois vu en fenetre de parry */
    struct sockaddr_in addr;      /* hote seulement */
} NetPlayer;

static NetPlayer NP[MAXP + 1];                            /* NP[MAXP] : place vide du spectateur (myId = MAXP) */
static int netMode, appState, myId, dmMode, matchOver, matchWinner;
static int spectating, specFollow = -1;                   /* spectateur : suit le joueur specFollow, -1 = camera libre */
static float matchT, matchEndT;
static char myName[16] = "joueur";
static double nowT;

static V3 fv(const float *f) { return v3(f[0], f[1], f[2]); }
static void vf(float *f, V3 v) { f[0] = v.x; f[1] = v.y; f[2] = v.z; }
static float np_height(int i) { return (NP[i].st.flags & NF_CROUCH) ? H_CROUCH : H_STAND; }
static V3 np_center(int i) { return v3(NP[i].disp.x, NP[i].disp.y + np_height(i) * 0.5f, NP[i].disp.z); }
static V3 np_vel(int i) { return fv(NP[i].st.vel); }
static void np_aabb(int i, float grow, V3 *mn, V3 *mx) {
    V3 d = NP[i].disp;
    *mn = v3(d.x - HW - grow, d.y - grow, d.z - HW - grow);
    *mx = v3(d.x + HW + grow, d.y + np_height(i) + grow, d.z + HW + grow);
}
static int np_target(int i) { return dmMode && i != myId && NP[i].active && NP[i].alive; }
/* vitesse relative entre le joueur et une cible : c'est elle qui compte pour les coups */
static float rel_speed(V3 targetVel) { return vlen(vsub(P.vel, targetVel)); }

static int botMode;                                       /* 1 = bot hote, 2 = bot client (tests reseau) */
static void netlog(const char *fmt, ...);
static void net_hit(int target, float dmg, V3 knock, int weapon);
static void net_fx_rail(V3 a, V3 b, float k);
static void net_fx_rocket(V3 pos, V3 vel, unsigned int netId);
static void net_fx_rocket_end(unsigned int netId, V3 pos);
static void net_fx_parry(unsigned int netId, V3 pos, V3 vel, float mult, int parries);
static void net_fx_melee(V3 pos, V3 dir, float sp);
static void net_fx_coin(int sub, unsigned int netId, V3 pos, V3 vel);

/* ------------------------------------------------------------------ */
/* Entrees                                                             */
/* ------------------------------------------------------------------ */
enum { SC_ESC = 0x01, SC_1 = 0x02, SC_2 = 0x03, SC_W = 0x11, SC_E = 0x12, SC_R = 0x13, SC_LCTRL = 0x1D,
       SC_A = 0x1E, SC_S = 0x1F, SC_D = 0x20, SC_LSHIFT = 0x2A, SC_C = 0x2E, SC_RSHIFT = 0x36,
       SC_SPACE = 0x39, SC_F2 = 0x3C, SC_F3 = 0x3D, SC_F11 = 0x57, SC_F = 0x21, SC_V = 0x2F, SC_G = 0x22 };
/* touches physiques (scancodes 0-255) + souris (256+) : etat tenu et appui du tick */
enum { K_LMB = 256, K_RMB, K_MMB, K_X1, K_X2, K_WHEEL, K_COUNT };
static unsigned char keys[K_COUNT], latch[K_COUNT];

/* Actions du jeu, chacune liee a deux touches au plus (0 = aucune). Le jeu ne lit que les actions. */
enum { A_FWD, A_BACK, A_LEFT, A_RIGHT, A_JUMP, A_CROUCH, A_DASH, A_FIRE, A_MELEE, A_HOOK, A_COIN,
       A_WEAP1, A_WEAP2, A_SWAP, A_SCORES, A_COUNT };
static const char *actId[A_COUNT] = { "avancer", "reculer", "gauche", "droite", "sauter", "accroupi", "dash", "tirer",
                                      "epee", "grappin", "piece", "arme1", "arme2", "changer_arme", "scores" };
static const char *actLabel[A_COUNT] = { "Avancer", "Reculer", "Gauche", "Droite", "Sauter (maintenir = bhop)",
                                         "S'accroupir / slide", "Dash", "Tirer", "Epee / parry", "Grappin (maintenir)",
                                         "Lancer une piece", "Railgun", "Roquettes", "Changer d'arme", "Tableau des scores" };
static const int bindDefault[A_COUNT][2] = {
    { 0x11, 0 }, { 0x1F, 0 }, { 0x1E, 0 }, { 0x20, 0 }, { 0x39, 0 }, { 0x1D, 0x2E }, { 0x2A, 0x12 },
    { K_LMB, 0 }, { K_RMB, 0x2F }, { 0x21, K_MMB }, { 0x22, K_X1 }, { 0x02, 0 }, { 0x03, 0 }, { K_WHEEL, 0 },
    { 0x0F, 0 },
};
static int binds[A_COUNT][2];
static unsigned char act[A_COUNT], actP[A_COUNT];          /* tenu / appuye pendant ce tick */
static unsigned char botHeld[A_COUNT], botPress[A_COUNT];  /* ce que les bots "appuient" */

/* reglages */
static float fovBase = BASE_HFOV;
static int invertY, showFps = 1;
static int impactFrames = 1, screenFx = 1;               /* options : impact frames (flashs) et effets d'ecran */

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

typedef struct { V3 pos; float val, t; } DmgNum;           /* chiffres de degats flottants */
#define MAXDN 32
static DmgNum DN[MAXDN];
static int dnNext;
static void dmg_number(V3 pos, float val) {
    DmgNum *d = &DN[dnNext];
    dnNext = (dnNext + 1) % MAXDN;
    d->pos = v3(pos.x + frange(-0.3f, 0.3f), pos.y + 0.6f, pos.z + frange(-0.3f, 0.3f));
    d->val = val; d->t = 0.9f;
}

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

static HWND hwnd;
static HDC hdc;
static int winW = 1600, winH = 900;

static void set_capture(int on) {
    if (on == captured) return;
    captured = on;
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
    mdx = mdy = 0;
}

/* ------------------------------------------------------------------ */
/* Effets                                                              */
/* ------------------------------------------------------------------ */
static void add_part(V3 p, V3 v, V3 col, float life, float size, float grav) {
    Part *q = &PA[paNext]; paNext = (paNext + 1) % MAXPA;
    q->pos = p; q->vel = v; q->col = col; q->life = q->max = life; q->size = size; q->grav = grav;
}

static void burst(V3 p, V3 col, int n, float speed, float life, float size) {
    for (int i = 0; i < n; i++)
        add_part(p, vmul(rand_dir(), speed * frange(0.2f, 1.0f)), col, life * frange(0.5f, 1.0f), size * frange(0.6f, 1.2f), 6);
}

static void add_beam(V3 a, V3 b, V3 col, float life) {
    Beam *m = &BM[bmNext]; bmNext = (bmNext + 1) % MAXBM;
    m->a = a; m->b = b; m->col = col; m->life = m->max = life;
}

/* ------------------------------------------------------------------ */
/* Joueur                                                              */
/* ------------------------------------------------------------------ */
static float pheight(void) { return P.crouch ? H_CROUCH : H_STAND; }
static void paabb(V3 pos, float h, V3 *mn, V3 *mx) {
    *mn = v3(pos.x - HW, pos.y, pos.z - HW);
    *mx = v3(pos.x + HW, pos.y + h, pos.z + HW);
}
static int pos_free(V3 pos, float h) {
    V3 mn, mx; paabb(pos, h, &mn, &mx);
    for (int i = 0; i < nb; i++) if (box_overlap(mn, mx, &boxes[i])) return 0;
    return 1;
}
static V3 view_dir(void) {
    float cp = cosf(P.pitch);
    return v3(sinf(P.yaw) * cp, sinf(P.pitch), -cosf(P.yaw) * cp);
}
static V3 eye_pos(void) { return v3(P.pos.x, P.pos.y + P.eyeH, P.pos.z); }
static V3 pcenter(void) { return v3(P.pos.x, P.pos.y + pheight() * 0.5f, P.pos.z); }

static void move_axis(int ax, float d, int canStep) {
    if (d == 0) return;
    float h = pheight();
    V3 mn, mx;
    *vc(&P.pos, ax) += d;
    for (int i = 0; i < nb; i++) {
        Box *b = &boxes[i];
        paabb(P.pos, h, &mn, &mx);
        if (!box_overlap(mn, mx, b)) continue;
        if (ax == 1) {
            if (d < 0) { P.pos.y = b->mx.y; if (P.vel.y < 0) P.vel.y = 0; P.grounded = 1; P.groundBox = i; }
            else { P.pos.y = b->mn.y - h - 0.001f; if (P.vel.y > 0) P.vel.y = 0; }
        } else {
            float rise = b->mx.y - P.pos.y;
            if (canStep && rise > 0 && rise <= STEP_H) {
                V3 np = P.pos; np.y = b->mx.y + 0.001f;
                if (pos_free(np, h)) { P.stepOfs -= rise; P.pos = np; continue; }
            }
            if (d > 0) *vc(&P.pos, ax) = *vc(&b->mn, ax) - HW - 0.001f;
            else       *vc(&P.pos, ax) = *vc(&b->mx, ax) + HW + 0.001f;
            *vc(&P.vel, ax) = 0;
        }
    }
}

static void wall_check(void) {
    static const float dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    P.wallTouch = 0;
    if (P.grounded) return;
    float h = pheight(), best = 2;
    V3 hv = vnorm(v3(P.vel.x, 0, P.vel.z));
    for (int d = 0; d < 4; d++) {
        V3 p = P.pos, mn, mx;
        p.x += dirs[d][0] * 0.12f; p.z += dirs[d][1] * 0.12f;
        paabb(p, h, &mn, &mx);
        mn.y += 0.3f;
        for (int i = 0; i < nb; i++)
            if (box_overlap(mn, mx, &boxes[i])) {
                V3 n = v3(-dirs[d][0], 0, -dirs[d][1]);
                float along = fabsf(vdot(n, hv));   /* on prefere le mur longe */
                if (along < best) { best = along; P.wallTouch = 1; P.wallN = n; }
                break;
            }
    }
}

static void accelerate(V3 wish, float wishspeed, float accel, float cap, float dt) {
    float ws = wishspeed > cap ? cap : wishspeed;
    float add = ws - vdot(P.vel, wish);
    if (add <= 0) return;
    float a = accel * wishspeed * dt;
    if (a > add) a = add;
    P.vel.x += wish.x * a;
    P.vel.z += wish.z * a;
}

static void hook_release(int jumpBoost) {
    if (P.hook == HOOK_ATTACHED) {
        P.airJumps = 1;
        P.hookCd = GRAPPLE_CD;
        if (jumpBoost) {
            P.vel.y = fmaxf(P.vel.y, 0) + GRAPPLE_JUMP;
            P.fovKick += 4;
            play(S_JUMP, 0.5f, 1.2f);
        }
        play(S_HOOK, 0.35f, 1.7f);
    } else if (P.hook == HOOK_FLYING) {
        P.hookCd = 0.2f;
    }
    P.hook = HOOK_NONE;
    P.hookEnemy = 0;
    P.hookPlayer = 0;
}

static float enemy_radius(int type);
static void damage_enemy(int i, float dmg, V3 knock);

/* Apres l'impact sur la cible accrochee : rebond vers le haut avec le grappin recharge,
 * pour enchainer sur la suivante. */
static void grapple_bounce(V3 hit, V3 dir) {
    for (int k = 0; k < 40; k++)
        add_part(hit, vadd(vmul(dir, 6), vmul(rand_dir(), 8)), v3(1, 0.85f, 0.4f), 0.5f, 0.15f, 6);
    shake += 0.3f;
    hitMarker = 0.18f;
    P.fovKick += 8;
    play(S_HOOKHIT, 0.9f, 0.6f);
    play(S_HIT, 0.7f, 0.8f);
    P.vel = vadd(vmul(v3(P.vel.x, 0, P.vel.z), 0.35f), v3(0, 9, 0));
    P.grounded = 0;
    P.airJumps = 1;
    P.hook = HOOK_NONE;
    P.hookEnemy = 0;
    P.hookPlayer = 0;
    P.hookCd = 0;
}

/* Grappin : le crochet vole jusqu'a un mur, puis tire le joueur vers l'ancre.
 * La corde ne se rallonge jamais : on peut se balancer autour du point d'accroche. */
static void grapple_tick(float dt, int press, int held) {
    P.hookCd -= dt;
    if (P.dead) { P.hook = HOOK_NONE; return; }
    if (press && P.hook == HOOK_NONE && P.hookCd <= 0) {
        P.hook = HOOK_FLYING;
        P.hookPos = eye_pos();
        P.hookDir = view_dir();
        P.hookDist = 0;
        P.hookEnemy = 0;
        P.hookPlayer = 0;
        play(S_HOOK, 0.6f, 1.0f);
    }
    if (P.hook != HOOK_NONE && !held) { hook_release(0); return; }

    if (P.hook == HOOK_FLYING) {
        float len = GRAPPLE_SPEED * dt;
        V3 n;
        float t = ray_world(P.hookPos, P.hookDir, len, &n), te;
        int target = -1, ptarget = -1;
        for (int j = 0; j < MAXE; j++)                    /* un ennemi devant le mur ? */
            if (E[j].alive && ray_sphere(P.hookPos, P.hookDir, E[j].pos, enemy_radius(E[j].type) + 0.3f, t, &te)) {
                t = te; target = j;
            }
        for (int j = 0; j < MAXP; j++) {                  /* ou un autre joueur (LAN) ? */
            V3 mn, mx;
            if (!np_target(j)) continue;
            np_aabb(j, 0.3f, &mn, &mx);
            if (ray_box(P.hookPos, P.hookDir, mn, mx, t, &te, NULL) && te < t) { t = te; target = -1; ptarget = j; }
        }
        if (target >= 0 || ptarget >= 0) {
            P.hook = HOOK_ATTACHED;
            if (target >= 0) { P.hookEnemy = target + 1; P.hookPos = P.anchor = E[target].pos; E[target].flash = 0.12f; }
            else { P.hookPlayer = ptarget + 1; P.hookPos = P.anchor = np_center(ptarget); }
            P.ropeLen = vlen(vsub(P.anchor, pcenter()));
            P.hookBlockT = 0;
            P.airJumps = 1;
            P.fovKick += 6;
            play(S_HOOKHIT, 0.8f, 1.4f);
        } else if (t < len) {
            P.hookPos = P.anchor = vadd(P.hookPos, vmul(P.hookDir, t));
            P.hook = HOOK_ATTACHED;
            P.ropeLen = vlen(vsub(P.anchor, pcenter()));
            P.hookBlockT = 0;
            P.airJumps = 1;
            P.fovKick += 5;
            for (int i = 0; i < 18; i++)
                add_part(P.anchor, vadd(vmul(n, 3), vmul(rand_dir(), 4)), v3(1, 0.85f, 0.4f), 0.4f, 0.1f, 10);
            play(S_HOOKHIT, 0.7f, 1.0f);
        } else {
            P.hookPos = vadd(P.hookPos, vmul(P.hookDir, len));
            P.hookDist += len;
            if (P.hookDist > GRAPPLE_RANGE) { P.hook = HOOK_NONE; P.hookCd = 0.2f; }
        }
        return;
    }

    if (P.hook == HOOK_ATTACHED) {
        Enemy *e = P.hookEnemy ? &E[P.hookEnemy - 1] : NULL;
        int pl = P.hookPlayer - 1;
        if (e) {                                          /* l'ancre suit l'ennemi */
            if (!e->alive) { hook_release(0); return; }
            P.hookPos = P.anchor = e->pos;
        }
        if (pl >= 0) {                                    /* ou le joueur accroche */
            if (!np_target(pl)) { hook_release(0); return; }
            P.hookPos = P.anchor = np_center(pl);
        }
        int onTarget = e || pl >= 0;
        float tr = e ? enemy_radius(e->type) : (pl >= 0 ? 0.5f : 0.1f);
        V3 d = vsub(P.anchor, pcenter());
        float dist = vlen(d);
        V3 dir = vmul(d, 1.0f / fmaxf(dist, 0.01f));
        if (onTarget && dist < tr + 1.6f) {               /* impact : degats selon la vitesse relative */
            float impact = rel_speed(e ? e->vel : np_vel(pl));
            float dmg = GRAPPLE_STRIKE_DMG + impact * GRAPPLE_STRIKE_PER_MS;
            V3 hit = P.anchor;
            if (e) damage_enemy(P.hookEnemy - 1, dmg, vmul(dir, 14));
            else net_hit(pl, dmg * PVP_SCALE, vadd(vmul(dir, 14), v3(0, 4, 0)), W_HOOK);
            impact_frame(hit, 0.07f);
            grapple_bounce(hit, dir);
            return;
        }
        if (!onTarget && dist < 1.6f) { hook_release(0); return; }
        /* la corde casse si un obstacle la coupe trop longtemps */
        if (!los(eye_pos(), vsub(P.anchor, vmul(dir, tr)))) P.hookBlockT += dt;
        else P.hookBlockT = 0;
        if (P.hookBlockT > 0.15f) { hook_release(0); return; }

        float pull = onTarget ? GRAPPLE_ENEMY_PULL : GRAPPLE_PULL, maxSp = onTarget ? GRAPPLE_ENEMY_MAXSPEED : GRAPPLE_MAXSPEED;
        if (vdot(P.vel, dir) < maxSp) P.vel = vadd(P.vel, vmul(dir, pull * dt));
        if (dist < P.ropeLen) P.ropeLen = dist;
        float out = -vdot(P.vel, dir);                    /* vitesse qui s'eloigne de l'ancre */
        if (dist >= P.ropeLen - 0.01f && out > 0) P.vel = vadd(P.vel, vmul(dir, out));
        if (dir.y > 0.2f) P.grounded = 0;
    }
}

static void player_tick(float dt, int jumpPress, int dashPress) {
    P.prevPos = P.pos;
    int alive = !P.dead;
    float fw = alive ? (float)(act[A_FWD] - act[A_BACK]) : 0;
    float sd = alive ? (float)(act[A_RIGHT] - act[A_LEFT]) : 0;
    V3 f = v3(sinf(P.yaw), 0, -cosf(P.yaw)), r = v3(cosf(P.yaw), 0, sinf(P.yaw));
    V3 wish = vnorm(vadd(vmul(f, fw), vmul(r, sd)));
    int hasWish = fw != 0 || sd != 0;

    P.slideCd -= dt; P.wallJumpCd -= dt;
    P.coyote = P.grounded ? 0 : P.coyote + dt;
    if (P.dashCharge < 2) P.dashCharge = fminf(2, P.dashCharge + dt / DASH_RECHARGE);

    /* accroupi / slide */
    int wantC = alive && act[A_CROUCH];
    if (wantC) P.crouch = 1;
    else if (P.crouch && pos_free(P.pos, H_STAND)) P.crouch = 0;
    float hs = hlen(P.vel);
    if (P.sliding && (!P.crouch || !P.grounded || hs < SLIDE_END)) P.sliding = 0;
    if (P.grounded && P.crouch && !P.sliding && hs > SLIDE_MIN) {
        P.sliding = 1;
        if (P.slideCd <= 0) {
            V3 hd = vnorm(v3(P.vel.x, 0, P.vel.z));
            P.vel = vadd(P.vel, vmul(hd, SLIDE_BOOST));
            P.slideCd = SLIDE_CD;
            P.fovKick += 4;
        }
        play(S_SLIDE, 0.7f, 1.0f);
    }

    /* sauter pendant le grappin = lacher avec un boost, sans consommer le double saut */
    if (alive && jumpPress && P.hook == HOOK_ATTACHED) { hook_release(1); jumpPress = 0; }

    /* saut : sol (bhop auto en maintenant), wall-jump, double saut */
    if (alive && (jumpPress || (act[A_JUMP] && P.grounded))) {
        if (P.grounded || P.coyote < 0.1f) {
            P.vel.y = JUMP_V; P.grounded = 0; P.coyote = 1; P.sliding = 0;
            play(S_JUMP, 0.5f, frange(0.95f, 1.05f));
        } else if (jumpPress && P.wallTouch && P.wallJumpCd <= 0) {
            P.vel = vadd(P.vel, vmul(P.wallN, WALLJUMP_PUSH));
            P.vel.y = WALLJUMP_UP;
            P.wallrun = 0; P.wallJumpCd = 0.25f; P.airJumps = 1;
            play(S_WALLJUMP, 0.6f, 1.0f);
        } else if (jumpPress && P.airJumps > 0) {
            P.airJumps--;
            P.vel.y = JUMP_V * 0.95f;
            if (hasWish) {
                float h = hlen(P.vel);
                V3 hv = vlerp(v3(P.vel.x, 0, P.vel.z), vmul(wish, h), 0.6f);
                P.vel.x = hv.x; P.vel.z = hv.z;
            }
            for (int i = 0; i < 12; i++)
                add_part(vadd(P.pos, v3(frange(-.4f, .4f), 0, frange(-.4f, .4f))), v3(frange(-2, 2), -3, frange(-2, 2)),
                         v3(0.5f, 0.8f, 1.0f), 0.4f, 0.12f, 0);
            play(S_JUMP, 0.5f, 1.35f);
        }
    }

    /* dash */
    if (alive && dashPress && P.dashCharge >= 1) {
        P.dashCharge -= 1;
        V3 d = hasWish ? wish : f;
        float sp = fmaxf(hlen(P.vel) + 3, DASH_SPEED);
        P.vel.x = d.x * sp; P.vel.z = d.z * sp;
        {   /* poussee verticale selon l'inclinaison de la vue : viser en haut = monter, en bas = plonger */
            float vy = sinf(P.pitch) * sp;
            if (P.grounded) { if (vy > 0) P.vel.y = vy; }
            else P.vel.y = (vy > 2) ? vy : (vy < 0 ? vy : 2);
        }
        P.fovKick += 9;
        play(S_DASH, 0.7f, 1.0f);
    }

    grapple_tick(dt, alive && hookPressIn, alive && hookHeldIn);

    /* friction (aucune quand le grappin tire) */
    if (P.grounded && P.hook != HOOK_ATTACHED) {
        float sp = hlen(P.vel);
        if (sp > 1e-4f) {
            float fr = P.sliding ? SLIDE_FRICTION : FRICTION;
            float ctrl = sp < STOPSPEED ? STOPSPEED : sp;
            float ns = sp - ctrl * fr * dt;
            if (ns < 0) ns = 0;
            ns /= sp;
            P.vel.x *= ns; P.vel.z *= ns;
        }
    }

    /* acceleration */
    if (P.wallrun) {
        V3 wp = vnorm(vsub(wish, vmul(P.wallN, vdot(wish, P.wallN))));
        accelerate(wp, RUN_SPEED * 1.15f, 6, 1e9f, dt);
        float away = vdot(P.vel, P.wallN);
        if (away > 0) P.vel = vsub(P.vel, vmul(P.wallN, away));
        P.vel = vsub(P.vel, vmul(P.wallN, 0.5f));                   /* colle au mur */
    } else if (P.grounded && !P.sliding) {
        accelerate(wish, P.crouch ? CROUCH_SPEED : RUN_SPEED, GROUND_ACCEL, 1e9f, dt);
    } else if (P.sliding) {
        accelerate(wish, RUN_SPEED, 3, 0.5f, dt);
    } else {
        accelerate(wish, RUN_SPEED, AIR_ACCEL, AIR_CAP, dt);
    }

    /* gravite */
    P.vel.y -= (P.wallrun ? GRAVITY * 0.12f : P.hook == HOOK_ATTACHED ? GRAVITY * 0.5f : GRAVITY) * dt;
    if (P.wallrun && P.vel.y < -1.0f) P.vel.y = -1.0f;

    /* deplacement + collisions, sous-pas si on va vite */
    int wasG = P.grounded;
    float prevVy = P.vel.y;
    int steps = (int)(vlen(P.vel) * dt / 0.3f) + 1;
    P.grounded = 0;
    for (int s = 0; s < steps; s++) {
        move_axis(0, P.vel.x * dt / steps, wasG);
        move_axis(2, P.vel.z * dt / steps, wasG);
        move_axis(1, P.vel.y * dt / steps, 0);
    }

    if (P.grounded && !wasG) {
        P.airJumps = 1; P.wallTime = 0; P.wallrun = 0;
        if (prevVy < -10)
            for (int k = 0; k < 18; k++) {                /* anneau de poussiere */
                float ang = k * (2 * PI / 18);
                add_part(vadd(P.pos, v3(0, 0.1f, 0)), v3(cosf(ang) * 6, 0.5f, sinf(ang) * 6), v3(0.75f, 0.75f, 0.8f),
                         0.45f, 0.2f, 0);
            }
        if (prevVy < -6) {
            P.landDip = fminf(0.3f, -prevVy * 0.012f);
            play(S_LAND, clampf(-prevVy / 25, 0.2f, 1), 1.0f);
        }
    }

    if (P.grounded && boxes[P.groundBox].kind == B_PAD && alive) {
        V3 L = boxes[P.groundBox].launch;
        P.vel = L; P.grounded = 0; P.airJumps = 1; P.coyote = 1;
        P.fovKick += 6;
        burst(P.pos, v3(0.3f, 0.9f, 1.0f), 30, 6, 0.6f, 0.15f);
        play(S_PAD, 0.8f, 1.0f);
    }

    wall_check();
    if (alive && !P.grounded && P.hook != HOOK_ATTACHED && P.wallTouch && act[A_FWD] && hlen(P.vel) > WALLRUN_MIN && P.wallTime < WALLRUN_MAX) {
        if (!P.wallrun) { P.wallrun = 1; if (P.vel.y < 1) P.vel.y = 1; P.airJumps = 1; }
        P.wallTime += dt;
        if (frand() < 0.3f)
            add_part(vsub(vadd(P.pos, v3(0, 0.5f, 0)), vmul(P.wallN, HW)), v3(frange(-1, 1), frange(0, 1), frange(-1, 1)),
                     v3(1, 0.6f, 0.9f), 0.4f, 0.08f, 4);
    } else P.wallrun = 0;
    if (P.grounded) P.wallTime = 0;

    /* camera */
    float targetEye = P.dead ? 0.3f : (P.crouch ? EYE_CROUCH : EYE_STAND);
    P.eyeH += (targetEye - P.eyeH) * fminf(1, dt * 14);
    P.stepOfs *= expf(-dt * 14);
    P.landDip *= expf(-dt * 8);
    P.fovKick *= expf(-dt * 4);
    float targetRoll = P.dead ? 25.0f : (P.wallrun ? vdot(P.wallN, r) * 14.0f : sd * 1.8f);
    if (P.sliding) targetRoll += 3;
    P.roll += (targetRoll - P.roll) * fminf(1, dt * 8);
    hs = hlen(P.vel);
    if (P.grounded && !P.sliding && hs > 1) P.bobT += hs * dt * 1.15f;
    P.bob += ((P.grounded && !P.sliding) ? sinf(P.bobT) * 0.045f * fminf(hs / RUN_SPEED, 1) - P.bob : -P.bob) * fminf(1, dt * 12);

    if (P.sliding && frand() < 0.5f)
        add_part(vadd(P.pos, v3(frange(-.3f, .3f), 0.05f, frange(-.3f, .3f))), v3(frange(-1, 1), frange(1, 3), frange(-1, 1)),
                 v3(1.0f, 0.75f, 0.4f), 0.35f, 0.07f, 10);
}

/* ------------------------------------------------------------------ */
/* Combat                                                              */
/* ------------------------------------------------------------------ */
static float enemy_radius(int type) { return type == EN_DRONE ? 0.9f : 0.75f; }
static V3 enemy_color(int type) { return type == EN_DRONE ? v3(1.0f, 0.25f, 0.3f) : v3(1.0f, 0.6f, 0.15f); }

static void damage_player(float dmg) {
    if (P.dead) return;
    if (dmMode) { net_hit(myId, dmg, v3(0, 0, 0), W_SELF); return; }  /* en LAN, l'hote tient les PV */
    P.hp -= dmg; P.hurtFlash = 0.6f; shake += 0.25f;
    play(S_HURT, 0.8f, frange(0.9f, 1.1f));
    if (P.hp <= 0) {
        P.hp = 0; P.dead = 1; P.deathT = 0;
        if (score > bestScore) bestScore = score;
    }
}

static void kill_enemy(int i, int byPlayer) {
    Enemy *e = &E[i];
    e->alive = 0;
    burst(e->pos, enemy_color(e->type), 60, 12, 0.9f, 0.18f);
    shards(e->pos, enemy_color(e->type), 12, 10);
    if (byPlayer) { impact_frame(e->pos, 0.07f); killMarkT = 0.45f; }
    burst(e->pos, v3(1, 1, 0.8f), 20, 6, 0.4f, 0.25f);
    if (!byPlayer) { play(S_EXPLO, 0.6f, 1.4f); return; }
    play(S_KILL, 0.6f, 1.0f + 0.06f * (combo < 8 ? combo : 8));
    kills++;
    combo++; comboT = 3.0f;
    float mult = (1 + rel_speed(e->vel) / 15.0f) * (1 + 0.25f * (combo - 1));
    int pts = (int)((e->type == EN_DRONE ? 100 : 60) * mult);
    score += pts;
    P.hp = fminf(100, P.hp + (e->type == EN_DRONE ? 15 : 8));
    snprintf(popText, sizeof popText, "+%d", pts);
    popT = 1.2f;
}

static void damage_enemy(int i, float dmg, V3 knock) {
    Enemy *e = &E[i];
    if (!e->alive) return;
    e->hp -= dmg; e->flash = 0.12f;
    dmg_number(e->pos, dmg);
    e->vel = vadd(e->vel, knock);
    if (e->hp <= 0) kill_enemy(i, 1);
}

/* byPlayer : 0 = ennemi, 1 = joueur, 2 = joueur sans recul ni degat sur soi (projectile renvoye).
 * Renvoie le nombre d'ennemis touches. */
static int explode(V3 c, float radius, float dmg, int byPlayer) {
    burst(c, v3(1.0f, 0.55f, 0.15f), 90, 14, 0.7f, 0.3f);
    burst(c, v3(1.0f, 0.9f, 0.5f), 30, 7, 0.35f, 0.5f);
    play(S_EXPLO, 0.9f, frange(0.9f, 1.1f));
    float dist = vlen(vsub(pcenter(), c));
    shake += fmaxf(0, 0.5f - dist * 0.02f);
    int hit = 0;
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive) continue;
        V3 dv = vsub(E[i].pos, c);
        float d = vlen(dv), R = radius + enemy_radius(E[i].type);
        if (d < R) { float f = 1 - d / R; damage_enemy(i, dmg * (0.3f + 0.7f * f), vmul(vnorm(dv), 12 * f)); hit++; }
    }
    for (int i = 0; byPlayer && i < MAXP; i++) {         /* LAN : le tireur calcule le souffle sur les autres */
        if (!np_target(i)) continue;
        V3 dv = vsub(np_center(i), c);
        float d = vlen(dv), R = radius + 0.5f;
        if (d < R) {
            float f = 1 - d / R;
            net_hit(i, dmg * (0.3f + 0.7f * f) * PVP_SCALE, vmul(vnorm(dv), ROCKET_KNOCK * f + 3), byPlayer == 2 ? W_PARRY : W_ROCKET);
            hit++;
        }
    }
    if (hit && byPlayer) { hitMarker = 0.18f; play(S_HIT, 0.6f, 1.0f); }
    if (!P.dead && dist < radius && byPlayer != 2) {
        float f = 1 - dist / radius;
        V3 dir = vnorm(vsub(pcenter(), c));
        P.vel = vadd(P.vel, vmul(dir, ROCKET_KNOCK * f + 3));
        if (dir.y > 0.3f) P.grounded = 0;
        if (byPlayer) damage_player(5 * f);
        else damage_player(30 * f);
    }
    return hit;
}

static float atten(V3 p) { return clampf(1.2f - vlen(vsub(p, P.pos)) / 60.0f, 0.1f, 1.0f); }

/* explosion purement visuelle : roquette d'un autre joueur (c'est son tireur qui gere les degats) */
static void explode_fx(V3 c) {
    burst(c, v3(1.0f, 0.55f, 0.15f), 90, 14, 0.7f, 0.3f);
    burst(c, v3(1.0f, 0.9f, 0.5f), 30, 7, 0.35f, 0.5f);
    play(S_EXPLO, 0.9f * atten(c), frange(0.9f, 1.1f));
}

static float streak_mult(void) { return 1 + STREAK_DMG * (float)(shotStreak < STREAK_MAX ? shotStreak : STREAK_MAX); }
static float streak_rate(void) { return 1 + STREAK_RATE * (float)(shotStreak < STREAK_MAX ? shotStreak : STREAK_MAX); }
static void shot_result(int hit) {
    if (hit) shotStreak++;
    else { if (shotStreak >= 2) streakLostT = 1.0f; shotStreak = 0; }
}

static void parry_feedback(V3 where) {
    impact_frame(where, 0.1f);
    hitstop = fmaxf(hitstop, 0.12f);
    parryFlash = 0.35f;
    shake += 0.2f;
    if (!dmMode) P.hp = fminf(100, P.hp + PARRY_HEAL);    /* en LAN c'est l'hote qui soigne */
    P.dashCharge = fminf(2, P.dashCharge + 1);            /* recompense : un dash et le grappin recharges */
    P.hookCd = 0;
    snprintf(popText, sizeof popText, "PARRY");
    popT = 1.0f;
    for (int k = 0; k < 30; k++) add_part(where, vmul(rand_dir(), 9), v3(1, 1, 0.85f), 0.4f, 0.12f, 0);
    play(S_PARRY, 0.9f, 1.0f);
}

/* Coup de corps a corps (clic droit / V). Degats et portee montent avec la vitesse RELATIVE
 * entre le joueur et la cible : foncer sur quelqu'un qui fonce sur toi frappe tres fort.
 * Il pare aussi : orbes ennemies et roquettes adverses sont renvoyees, les kamikazes explosent. */
static float melee_range(float rel) { return MELEE_RANGE + rel * MELEE_RANGE_PER_MS; }
static float melee_dmg(float rel) { return MELEE_DMG + rel * MELEE_DMG_PER_MS; }

/* Parry : pendant PARRY_WINDOW apres le debut du coup, les projectiles proches sont renvoyes.
 * Chaque renvoi d'une meme roquette (ping-pong) l'accelere, la rend plus forte, et reduit la zone de parry. */
static int try_parry(void) {
    V3 eye = eye_pos(), f = view_dir();
    int parried = 0;
    for (int i = 0; i < MAXPR; i++) {
        Proj *p = &PR[i];
        int foreignRocket = dmMode && (p->kind == PR_ROCKET || p->kind == PR_PARRIED) && p->owner != myId;
        if (!p->alive || (p->kind != PR_ORB && !foreignRocket)) continue;
        V3 d = vsub(p->pos, eye);
        float dist = vlen(d), range = (MELEE_RANGE + 1.5f) * powf(PARRY_RANGE_SHRINK, (float)p->parries);
        if (dist > range || vdot(vmul(d, 1.0f / fmaxf(dist, 0.01f)), f) < 0.3f) continue;
        V3 aim = f; float best = PARRY_AIM_COS;
        for (int j = 0; j < MAXE; j++) {
            if (!E[j].alive) continue;
            V3 to = vnorm(vsub(E[j].pos, p->pos));
            float dd = vdot(to, f);
            if (dd > best) { best = dd; aim = to; }
        }
        for (int j = 0; j < MAXP; j++) {
            if (!np_target(j)) continue;
            V3 to = vnorm(vsub(np_center(j), p->pos));
            float dd = vdot(to, f);
            if (dd > best) { best = dd; aim = to; }
        }
        p->parries++;
        if (!p->orb) p->mult *= p->parries == 1 ? PARRY_ROCKET_MULT : PARRY_PINGPONG;
        p->kind = PR_PARRIED; p->life = 4; p->owner = myId;
        p->vel = vmul(aim, PARRY_SPEED * powf(PARRY_PINGPONG, (float)(p->parries - 1)));
        if (dmMode) net_fx_parry(p->netId, p->pos, p->vel, p->mult, p->parries);
        parried++;
        P.swingLanded = 1;
        parry_feedback(p->pos);
        if (p->parries > 1) { snprintf(popText, sizeof popText, "PARRY x%d", p->parries); popT = 1.0f; }
    }
    return parried;
}

static void melee(void) {
    float sp = vlen(P.vel);
    V3 eye = eye_pos(), f = view_dir();
    V3 r = v3(cosf(P.yaw), 0, sinf(P.yaw)), u = vcross(r, f);
    int hits = 0, parried = 0;

    parried = try_parry();

    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &E[i];
        if (!e->alive) continue;
        float rel = rel_speed(e->vel);
        V3 d = vsub(e->pos, eye);
        float dist = vlen(d), reach = dist - enemy_radius(e->type);
        if (reach > melee_range(rel)) continue;
        if (reach > 0.8f && vdot(vmul(d, 1.0f / dist), f) < MELEE_CONE) continue;
        if (e->type == EN_RUSHER) {                       /* kamikaze pare : explose dans sa face */
            V3 at = e->pos;
            damage_enemy(i, 9999, vmul(f, 10));
            parry_feedback(at);
            parried++;
        } else {
            damage_enemy(i, melee_dmg(rel), vmul(f, 8 + rel * 0.5f));
            if (!e->alive) P.hp = fminf(100, P.hp + 15);  /* bonus de soin des kills au corps a corps */
        }
        hits++;
    }

    for (int i = 0; i < MAXP; i++) {                     /* LAN : les autres joueurs */
        if (!np_target(i)) continue;
        float rel = rel_speed(np_vel(i));
        V3 c = np_center(i), d = vsub(c, eye);
        float dist = vlen(d), reach = dist - 0.5f;
        if (reach > melee_range(rel)) continue;
        if (reach > 0.8f && vdot(vmul(d, 1.0f / dist), f) < MELEE_CONE) continue;
        net_hit(i, melee_dmg(rel) * PVP_SCALE, vadd(vmul(f, 8 + rel * 0.5f), v3(0, 3, 0)), W_MELEE);
        burst(c, v3(1, 0.8f, 0.5f), 20, 8, 0.3f, 0.12f);
        hits++;
    }

    float range = melee_range(sp);
    for (float a = -0.9f; a <= 0.9f; a += 0.1f) {        /* arc de la frappe */
        V3 p = vadd(eye, vadd(vmul(f, range * 0.55f + 0.6f), vadd(vmul(r, sinf(a) * range * 0.45f), vmul(u, -a * 0.3f))));
        add_part(p, vmul(f, 3), sp > 15 ? v3(1, 0.7f, 0.3f) : v3(0.9f, 0.9f, 1), 0.15f, 0.07f + sp * 0.002f, 0);
    }
    if (dmMode) net_fx_melee(eye, f, sp);
    P.meleeCd = MELEE_CD;
    P.meleeT = 0.25f;
    P.parryWin = PARRY_WINDOW;                            /* la fenetre de parry reste ouverte un court instant */
    P.swingLanded = hits || parried;
    play(S_DASH, 0.4f, 1.8f);
    if (hits) {
        P.dashCharge = fminf(2, P.dashCharge + 1);        /* toucher au corps a corps rend un dash */
        if (sp > 18) impact_frame(vadd(eye, vmul(f, 2)), 0.05f);
        hitMarker = 0.18f;
        shake += 0.15f + sp * 0.006f;
        if (!parried) hitstop = fmaxf(hitstop, 0.04f + sp * 0.0015f);
        play(S_PUNCH, 0.9f, frange(0.9f, 1.1f));
    }
}

static V3 muzzle(void) {
    V3 f = view_dir(), r = v3(cosf(P.yaw), 0, sinf(P.yaw)), u = vcross(r, f);
    return vadd(eye_pos(), vadd(vmul(f, 0.6f), vadd(vmul(r, 0.22f), vmul(u, -0.2f))));
}

static unsigned int coinCounter;

static void throw_coin(void) {
    for (int i = 0; i < MAXCOIN; i++) {
        Coin *c = &CO[i];
        if (c->alive) continue;
        V3 f = view_dir();
        P.coins -= 1;
        c->alive = 1; c->owner = myId; c->life = 4; c->spin = 0;
        c->pos = vadd(eye_pos(), vadd(vmul(f, 0.7f), v3(0, -0.2f, 0)));
        c->vel = vadd(P.vel, vadd(vmul(f, 9), v3(0, 6, 0)));   /* la piece garde ta vitesse : elle reste devant toi */
        c->netId = ((unsigned int)(myId + 1) << 24) | (++coinCounter & 0xFFFFFF);
        play(S_HIT, 0.5f, 2.3f);
        if (dmMode) net_fx_coin(0, c->netId, c->pos, c->vel);
        return;
    }
}

static void coin_tick(float dt) {
    for (int i = 0; i < MAXCOIN; i++) {
        Coin *c = &CO[i];
        if (!c->alive) continue;
        c->life -= dt; c->spin += dt;
        c->vel.y -= COIN_GRAVITY * dt;
        V3 step = vmul(c->vel, dt);
        float len = vlen(step);
        if (len > 1e-5f && ray_world(c->pos, vmul(step, 1.0f / len), len, 0) < len) c->life = 0;   /* tombee */
        else c->pos = vadd(c->pos, step);
        if (c->life <= 0) { c->alive = 0; if (frand() < 0.5f) add_part(c->pos, v3(0, 2, 0), v3(1, 0.8f, 0.3f), 0.3f, 0.1f, 6); }
    }
}

/* Le rail touche la piece `ci` : il repart vers une autre piece (chaine) ou vers la cible visible la plus proche,
 * avec des degats multiplies. Renvoie 1 si une cible a ete touchee. */
static int ricochet(int ci, float dmg, int depth) {
    Coin *c = &CO[ci];
    V3 o = c->pos;
    int stolen = c->owner != myId;                        /* contre-jeu : la piece d'un adversaire se retourne contre lui */
    c->alive = 0;
    if (dmMode) net_fx_coin(1, c->netId, o, v3(0, 0, 0));
    dmg *= stolen ? 1.0f : COIN_MULT;                     /* piece volee : renvoie le tir, mais sans bonus */
    lastChain = depth + 1;
    for (int k = 0; k < 20; k++) add_part(o, vmul(rand_dir(), 6), v3(1, 0.85f, 0.3f), 0.35f, 0.1f, 0);
    play(S_PARRY, 0.55f, 1.5f + 0.15f * depth);
    hitstop = fmaxf(hitstop, 0.03f);

    int next = -1;                                        /* 1. une autre de mes pieces : la chaine continue */
    float best = 40;
    for (int i = 0; depth < 6 && i < MAXCOIN; i++) {
        if (!CO[i].alive) continue;                       /* la chaine passe par les pieces de tout le monde */
        float d = vlen(vsub(CO[i].pos, o));
        if (d < best && los(o, CO[i].pos)) { best = d; next = i; }
    }
    if (next >= 0) {
        add_beam(o, CO[next].pos, v3(1, 0.85f, 0.3f), 0.6f);
        if (dmMode) net_fx_rail(o, CO[next].pos, 1);
        return ricochet(next, dmg, depth + 1);   /* chaque piece remultiplie */
    }

    int te = -1, tp = -1;                                 /* 2. la cible visible la plus proche */
    best = COIN_SEEK_RANGE;
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive) continue;
        float d = vlen(vsub(E[i].pos, o));
        if (d < best && los(o, E[i].pos)) { best = d; te = i; tp = -1; }
    }
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i)) continue;
        float d = vlen(vsub(np_center(i), o));
        if (d < best && los(o, np_center(i))) { best = d; tp = i; te = -1; }
    }
    if (te < 0 && tp < 0 && botMode) {
        for (int i = 0; i < MAXP; i++)
            if (np_target(i))
                netlog("ricochet sans cible : %s a %.1f m, vue %d, piece en (%.1f %.1f %.1f)", NP[i].name,
                       vlen(vsub(np_center(i), o)), los(o, np_center(i)), o.x, o.y, o.z);
    }
    if (te < 0 && tp < 0) {                               /* rien a toucher : le tir part en l'air */
        add_beam(o, vadd(o, v3(frange(-3, 3), 25, frange(-3, 3))), v3(1, 0.85f, 0.3f), 0.4f);
        return 0;
    }
    V3 at = te >= 0 ? E[te].pos : np_center(tp), dir = vnorm(vsub(at, o));
    add_beam(o, at, v3(1, 0.85f, 0.3f), 0.7f);
    if (dmMode) net_fx_rail(o, at, 1);
    burst(at, v3(1, 0.9f, 0.5f), 25, 9, 0.4f, 0.14f);
    if (te >= 0) damage_enemy(te, dmg, vmul(dir, 10));
    else net_hit(tp, dmg * PVP_SCALE, vmul(dir, 6), W_COIN);
    if (stolen) snprintf(popText, sizeof popText, "PIECE VOLEE !");
    else snprintf(popText, sizeof popText, depth ? "RICOCHET x%d" : "RICOCHET", depth + 1);
    popT = 1.0f;
    hitMarker = 0.25f;
    impact_frame(at, 0.06f);
    return 1;
}

static void fire_rail(void) {
    P.railAmmo--;
    V3 eye = eye_pos(), d = view_dir(), n;
    float tw = ray_world(eye, d, 400, &n), mult = streak_mult();
    int hit = 0, coinHit = -1;
    for (int i = 0; i < MAXCOIN; i++) {                   /* une de mes pieces sur la trajectoire ? le tir s'y arrete */
        float t;
        if (CO[i].alive && ray_sphere(eye, d, CO[i].pos, COIN_RADIUS, tw, &t)) { tw = t; coinHit = i; }
    }
    for (int i = 0; i < MAXE; i++) {
        float t;
        if (E[i].alive && ray_sphere(eye, d, E[i].pos, enemy_radius(E[i].type), tw, &t)) {
            burst(vadd(eye, vmul(d, t)), v3(1, 1, 1), 15, 8, 0.3f, 0.12f);
            damage_enemy(i, RAIL_DMG * mult, vmul(d, 8));
            hit = 1;
        }
    }
    for (int i = 0; i < MAXP; i++) {                     /* LAN : le rail traverse aussi les joueurs */
        V3 mn, mx; float t;
        if (!np_target(i)) continue;
        np_aabb(i, 0.05f, &mn, &mx);
        if (ray_box(eye, d, mn, mx, tw, &t, NULL)) {
            burst(vadd(eye, vmul(d, t)), v3(1, 1, 1), 15, 8, 0.3f, 0.12f);
            net_hit(i, RAIL_DMG * mult * PVP_SCALE, vmul(d, 4), W_RAIL);
            hit = 1;
        }
    }
    if (coinHit >= 0) {
        hit |= ricochet(coinHit, RAIL_DMG * mult, 0);
        if (botMode) netlog("ricochet : chaine de %d, %s", lastChain, hit ? "touche" : "dans le vide");
    }
    shot_result(hit);
    if (botMode) netlog("tir rail : %s", hit ? "touche" : "rate");
    V3 end = vadd(eye, vmul(d, tw)), m = muzzle();
    float k = clampf(shotStreak / (float)STREAK_MAX, 0, 1);  /* le rayon vire a l'or avec la chaine */
    add_beam(m, end, v3(0.3f + 0.7f * k, 0.85f - 0.05f * k, 1.0f - 0.7f * k), 0.6f);
    if (dmMode) net_fx_rail(m, end, k);
    V3 bd = vsub(end, m); float bl = vlen(bd); bd = vmul(bd, 1.0f / bl);
    V3 a = vnorm(vcross(bd, v3(0, 1, 0.01f))), b = vcross(bd, a);
    for (float s = 0; s < bl && s < 120; s += 0.35f) {       /* spirale facon Quake */
        float ang = s * 2.2f;
        V3 off = vadd(vmul(a, cosf(ang) * 0.12f), vmul(b, sinf(ang) * 0.12f));
        add_part(vadd(vadd(m, vmul(bd, s)), off), vmul(off, 4), v3(0.3f, 0.6f, 1.0f), 0.7f, 0.06f, 0);
    }
    for (int i = 0; i < 18; i++)
        add_part(end, vadd(vmul(n, 4), vmul(rand_dir(), 4)), v3(0.6f, 0.9f, 1.0f), 0.5f, 0.1f, 12);
    P.fireCd = RAIL_CD / streak_rate(); P.recoil = 1; shake += 0.06f;
    muzzleT = 0.07f;
    play(S_RAIL, 0.7f, frange(0.97f, 1.03f));
    if (hit) { hitMarker = 0.18f; play(S_HIT, 0.6f, 1.0f + 0.05f * (shotStreak < STREAK_MAX ? shotStreak : STREAK_MAX)); }
}

static Proj *spawn_proj(int kind, V3 pos, V3 vel) {
    for (int i = 0; i < MAXPR; i++)
        if (!PR[i].alive) {
            Proj *p = &PR[i];
            p->alive = 1; p->kind = kind; p->pos = pos; p->vel = vel; p->life = 6; p->owner = myId; p->netId = 0;
            p->mult = 1; p->parries = 0; p->orb = kind == PR_ORB;
            return p;
        }
    return NULL;
}

static unsigned int rocketCounter;

static void fire_rocket(void) {
    Proj *p = spawn_proj(PR_ROCKET, eye_pos(), vmul(view_dir(), ROCKET_SPEED));
    if (p && dmMode) {
        p->netId = ((unsigned int)(myId + 1) << 24) | (++rocketCounter & 0xFFFFFF);
        net_fx_rocket(p->pos, p->vel, p->netId);
    }
    P.fireCd = ROCKET_CD / streak_rate(); P.recoil = 1.3f; shake += 0.08f;
    muzzleT = 0.08f;
    play(S_ROCKET, 0.8f, frange(0.95f, 1.05f));
}

static void proj_tick(float dt) {
    for (int i = 0; i < MAXPR; i++) {
        Proj *p = &PR[i];
        if (!p->alive) continue;
        p->life -= dt;
        V3 step = vmul(p->vel, dt);
        float len = vlen(step);
        V3 dir = vmul(step, 1.0f / len), n;
        float t = ray_world(p->pos, dir, len, &n);
        int hitEnemy = -1, hitPlayer = -1;
        if (p->kind == PR_ROCKET || p->kind == PR_PARRIED) {
            int mine = !dmMode || p->owner == myId;       /* seul le tireur calcule les degats */
            for (int j = 0; j < MAXE; j++) {
                float te;
                if (E[j].alive && ray_sphere(p->pos, dir, E[j].pos, enemy_radius(E[j].type) + 0.2f, t, &te)) { t = te; hitEnemy = j; }
            }
            for (int j = 0; dmMode && j < MAXP; j++) {    /* LAN : collision avec les joueurs */
                V3 mn, mx; float te;
                if (j == p->owner || !NP[j].active || !NP[j].alive) continue;
                if (j == myId) {
                    if (P.dead) continue;
                    paabb(P.pos, pheight(), &mn, &mx);
                    mn = vsub(mn, v3(0.2f, 0.2f, 0.2f)); mx = vadd(mx, v3(0.2f, 0.2f, 0.2f));
                } else np_aabb(j, 0.2f, &mn, &mx);
                if (ray_box(p->pos, dir, mn, mx, t, &te, NULL) && te < t) { t = te; hitPlayer = j; }
            }
            if (t < len || p->life <= 0) {
                V3 c = vadd(vadd(p->pos, vmul(dir, t)), vmul(n, 0.15f));
                p->alive = 0;
                if (!mine) { explode_fx(c); continue; }
                if (dmMode) net_fx_rocket_end(p->netId, c);
                /* contre un joueur, l'impact direct passe par le souffle (une seule touche, plafonnee) */
                if (p->kind == PR_PARRIED && p->orb) {
                    if (hitEnemy >= 0) damage_enemy(hitEnemy, PARRY_DMG, vmul(dir, 10));
                    explode(c, 3.0f, 80, 2);
                } else if (p->kind == PR_PARRIED) {
                    if (hitEnemy >= 0) damage_enemy(hitEnemy, 40 * p->mult, vmul(dir, 10));
                    explode(c, ROCKET_RADIUS, ROCKET_DMG * p->mult, 2);
                } else {
                    float mult = streak_mult();
                    int hits = hitEnemy >= 0 || hitPlayer >= 0;
                    if (hitEnemy >= 0) damage_enemy(hitEnemy, 40 * mult, vmul(dir, 6));
                    hits += explode(c, ROCKET_RADIUS, ROCKET_DMG * mult, 1);
                    /* un rocket jump qui ne touche personne ne casse pas la chaine */
                    if (hits || vlen(vsub(c, pcenter())) > ROCKET_RADIUS + 1) shot_result(hits);
                }
                p->alive = 0;
                continue;
            }
            if (p->kind == PR_PARRIED)
                add_part(p->pos, vmul(rand_dir(), 0.6f), v3(1.0f, 0.9f, 0.5f), 0.35f, 0.15f, 0);
            else if (p->life < 5.95f)                         /* pas de trainee dans l'oeil du joueur */
                add_part(p->pos, vmul(rand_dir(), 0.8f), v3(1.0f, 0.5f, 0.15f), 0.45f, 0.18f, -1);
        } else {
            V3 mn, mx; paabb(P.pos, pheight(), &mn, &mx);
            V3 q = vadd(p->pos, step);
            if (!P.dead && q.x > mn.x - 0.25f && q.x < mx.x + 0.25f && q.y > mn.y - 0.25f && q.y < mx.y + 0.25f &&
                q.z > mn.z - 0.25f && q.z < mx.z + 0.25f) {
                damage_player(12);
                P.vel = vadd(P.vel, vmul(dir, 3));
                burst(q, v3(1, 0.3f, 0.9f), 20, 5, 0.4f, 0.12f);
                p->alive = 0;
                continue;
            }
            if (t < len || p->life <= 0) {
                burst(vadd(p->pos, vmul(dir, t)), v3(1, 0.3f, 0.9f), 14, 4, 0.4f, 0.1f);
                p->alive = 0;
                continue;
            }
            if (frand() < 0.5f) add_part(p->pos, vmul(rand_dir(), 0.3f), v3(1, 0.3f, 0.9f), 0.3f, 0.12f, 0);
        }
        p->pos = vadd(p->pos, step);
    }
}

/* ------------------------------------------------------------------ */
/* Ennemis et vagues                                                   */
/* ------------------------------------------------------------------ */
static void spawn_enemy(void) {
    for (int i = 0; i < MAXE; i++) {
        if (E[i].alive) continue;
        Enemy *e = &E[i];
        memset(e, 0, sizeof *e);
        e->alive = 1;
        float rusherChance = fminf(0.55f, 0.25f + wave * 0.04f);
        e->type = (wave > 1 && frand() < rusherChance) ? EN_RUSHER : EN_DRONE;
        e->hp = e->type == EN_DRONE ? 100.0f : 50.0f;
        e->fireT = frange(1.5f, 3.0f);
        e->t = frange(0, 10);
        for (int tries = 0; tries < 20; tries++) {
            float ang = frange(0, 2 * PI), rad = frange(ms(28), ms(48));
            e->pos = v3(cosf(ang) * rad, frange(9, 18), sinf(ang) * rad);
            if (vlen(vsub(e->pos, P.pos)) > 22) break;
        }
        sphere_pushout(&e->pos, &e->vel, enemy_radius(e->type));
        burst(e->pos, enemy_color(e->type), 40, 8, 0.6f, 0.15f);
        return;
    }
}

static void enemy_tick(float dt) {
    V3 pc = pcenter();
    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &E[i];
        if (!e->alive) continue;
        e->t += dt; e->flash -= dt;
        V3 to = vsub(pc, e->pos);
        float dist = vlen(to);
        V3 dir = vmul(to, 1.0f / fmaxf(dist, 0.01f));
        if (e->type == EN_DRONE) {
            float ang = e->t * 0.35f + i * 1.7f;
            V3 target = vadd(pc, v3(cosf(ang) * 13, 4.5f + sinf(e->t * 0.7f + i) * 2, sinf(ang) * 13));
            V3 tv = vsub(target, e->pos);
            V3 want = vmul(vnorm(tv), 8.0f * clampf(vlen(tv) / 4, 0, 1));
            e->vel = vlerp(e->vel, want, fminf(1, dt * 2));
            e->fireT -= dt;
            if (e->fireT <= 0 && !P.dead) {
                if (dist < 70 && los(e->pos, pc)) {
                    spawn_proj(PR_ORB, vadd(e->pos, vmul(dir, 1.0f)), vmul(dir, 16));
                    play(S_ESHOT, clampf(1.2f - dist / 60, 0.15f, 0.8f), frange(0.9f, 1.1f));
                }
                e->fireT = frange(1.6f, 2.6f) - fminf(wave * 0.08f, 0.7f);
            }
        } else {
            float sp = 10.5f + fminf(wave * 0.3f, 3);
            e->vel = vlerp(e->vel, vmul(dir, sp), fminf(1, dt * 1.6f));
            if (!P.dead && dist < 1.4f && P.hookEnemy != i + 1) {
                damage_player(18);
                P.vel = vadd(P.vel, vmul(dir, 9));
                kill_enemy(i, 0);
                continue;
            }
            if (frand() < 0.4f) add_part(e->pos, vmul(rand_dir(), 0.5f), v3(1, 0.5f, 0.1f), 0.35f, 0.12f, 0);
        }
        for (int j = 0; j < MAXE; j++) {                   /* separation */
            if (j == i || !E[j].alive) continue;
            V3 dv = vsub(e->pos, E[j].pos);
            float d = vlen(dv);
            if (d < 1.8f && d > 1e-4f) e->vel = vadd(e->vel, vmul(dv, (1.8f - d) * 4 * dt / d));
        }
        e->pos = vadd(e->pos, vmul(e->vel, dt));
        sphere_pushout(&e->pos, &e->vel, enemy_radius(e->type));
        if (e->pos.y < 1.0f) { e->pos.y = 1.0f; if (e->vel.y < 0) e->vel.y = 0; }
    }
}

static int enemies_alive(void) {
    int n = 0;
    for (int i = 0; i < MAXE; i++) n += E[i].alive;
    return n;
}

static void wave_tick(float dt) {
    if (toSpawn > 0) {
        spawnT -= dt;
        if (spawnT <= 0) { spawn_enemy(); toSpawn--; spawnT = 0.35f; }
    } else if (enemies_alive() == 0) {
        waveDelay -= dt;
        if (waveDelay <= 0) {
            wave++;
            toSpawn = 2 + wave * 2;
            if (toSpawn > 40) toSpawn = 40;
            waveDelay = 3; spawnT = 0;
            snprintf(msg, sizeof msg, "VAGUE %d", wave);
            msgT = 2.5f;
            play(S_WAVE, 0.7f, 1.0f);
        }
    }
}

static void reset_game(void) {
    memset(&P, 0, sizeof P);
    P.pos = P.prevPos = v3(0, 0, 24);
    P.hp = 100; P.dashCharge = 2; P.airJumps = 1; P.eyeH = EYE_STAND; P.fovCur = fovBase;
    P.railAmmo = RAIL_AMMO_START;
    P.coins = COIN_MAX;
    memset(CO, 0, sizeof CO);
    for (int i = 0; i < npu; i++) PU[i].t = PU[i].pending = 0;
    memset(E, 0, sizeof E); memset(PR, 0, sizeof PR); memset(PA, 0, sizeof PA); memset(BM, 0, sizeof BM);
    wave = 0; toSpawn = 0; waveDelay = 1.5f; spawnT = 0;
    score = kills = combo = 0; gameTime = 0; hitMarker = comboT = msgT = popT = shake = 0;
    shotStreak = 0; hitstop = parryFlash = streakLostT = 0;
}

/* ------------------------------------------------------------------ */
/* Multijoueur LAN : reseau                                            */
/*                                                                     */
/* L'hote est aussi un joueur (id 0) et fait autorite sur PV, kills,   */
/* respawns et match. Chaque joueur simule son propre mouvement et     */
/* envoie son etat 60 fois/s ; l'hote renvoie un instantane de tous.   */
/* Les touches sont validees chez le tireur, qui envoie un evenement   */
/* HIT. Les evenements sont renvoyes 3 fois et dedoublonnes par id.    */
/* ------------------------------------------------------------------ */
#define NET_MAGIC 0x564C4F43u
#define NET_VER   4
enum { PK_QUERY = 1, PK_ANNOUNCE, PK_JOIN, PK_WELCOME, PK_STATE, PK_SNAP, PK_ROSTER, PK_EVENT, PK_LEAVE,
       PK_PUNCH, PK_KEEP };   /* PUNCH : ouvre le NAT vers un client ; KEEP : un spectateur dit qu'il est la */
enum { EV_RAIL = 1, EV_ROCKET, EV_ROCKET_END, EV_PARRY, EV_HIT, EV_MELEE, EV_KILL, EV_RESPAWN, EV_MATCH, EV_INFO, EV_PICKUP,
       EV_PICKUP_TAKEN, EV_COIN, EV_MELEE_PARRY };

#pragma pack(push, 1)
typedef struct { unsigned int magic; unsigned char type, from, ver, pad; } PkHead;
typedef struct {
    unsigned int eid, netId;
    unsigned char kind, from, target, sub, actor, pad[3];
    float f[7];
} NetEvent;
typedef struct { PkHead h; char name[16]; unsigned char players, maxp, pad[2]; } PkAnnounce;
typedef struct { PkHead h; char name[16]; unsigned char spec, pad[3]; } PkJoin;
typedef struct { PkHead h; unsigned char id, refused, pad[2]; } PkWelcome;
typedef struct { PkHead h; NetPState st; } PkState;
typedef struct { unsigned char active, alive; short hp, kills, deaths; NetPState st; } SnapP;
typedef struct { PkHead h; float matchT; unsigned char matchOver, winner; unsigned short pickMask; SnapP p[MAXP]; } PkSnap;
typedef struct { PkHead h; char names[MAXP][16]; } PkRoster;
typedef struct { PkHead h; NetEvent ev; } PkEvent;
#pragma pack(pop)

typedef struct {
    char name[16]; struct sockaddr_in addr; int players, maxp; double seen;
    int online, spec;                                     /* online : salon du lobby (addr = ip:port public) ; spec : spectateurs */
    char id[8];                                           /* identifiant du salon chez le lobby */
} Found;
#define MAXFOUND 12
static Found found[MAXFOUND];
static int nFound;

typedef struct { int active; struct sockaddr_in addr; double lastHeard; } Spectator;   /* hote seulement */
static Spectator SP[MAXSPEC];

typedef struct { NetEvent ev; int left; } OutEv;
#define OUTQ 128
static OutEv outq[OUTQ];
static int outqHead;
static unsigned int evCounter, seenIds[512];
static int seenHead;

typedef struct { int killer, victim, kind, info; float t; } KillFeed;
static KillFeed kfeed[6];
static int killerOfMe = -1;

static SOCKET sock = INVALID_SOCKET;
static int wsaReady, connected, joining, pktIn, pktOut;
static struct sockaddr_in hostAddr;
static float sendAcc, rosterAcc, queryAcc, joinAcc;
static double joinStartT, lastSnapT, netStatusT;
static char netStatus[96], hostIpText[96];
static FILE *botLog;
static int joinSpec, joinOnline;                          /* la connexion en cours est un spectateur / vient du lobby */
static int lanOff;                                        /* --nolan : pas de recherche par broadcast (tests du lobby) */
static void online_reset(void);
static void lb_update(float dt);
static int stun_parse(const unsigned char *b, int n, struct sockaddr_in *out);
static void stun_got(const struct sockaddr_in *a);

static void netlog(const char *fmt, ...) {
    if (!botLog) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(botLog, "[%7.2f] ", nowT);
    vfprintf(botLog, fmt, ap);
    fputc('\n', botLog);
    fflush(botLog);
    va_end(ap);
}

static void set_status(const char *s) {
    snprintf(netStatus, sizeof netStatus, "%s", s);
    netStatusT = nowT;
    netlog("statut : %s", s);
}

static const char *pname(int i) { return (i >= 0 && i < MAXP && NP[i].name[0]) ? NP[i].name : "?"; }

static int wsa_init(void) {
    if (!wsaReady) {
        WSADATA w;
        if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return 0;
        wsaReady = 1;
    }
    return 1;
}

static int net_open(unsigned short port) {
    if (!wsa_init()) return 0;
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) return 0;
    u_long nonBlocking = 1;
    ioctlsocket(sock, FIONBIO, &nonBlocking);
    BOOL yes = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char *)&yes, sizeof yes);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&a, sizeof a) != 0) { closesocket(sock); sock = INVALID_SOCKET; return 0; }
    return 1;
}

static void net_send(const void *d, int len, const struct sockaddr_in *to) {
    if (sock == INVALID_SOCKET) return;
    sendto(sock, (const char *)d, len, 0, (const struct sockaddr *)to, sizeof *to);
    pktOut++;
}

static void head(PkHead *h, int type) {
    h->magic = NET_MAGIC; h->type = (unsigned char)type; h->from = (unsigned char)myId; h->ver = NET_VER; h->pad = 0;
}

static int same_addr(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
}

static void host_send_all(const void *d, int len) {
    for (int i = 1; i < MAXP; i++) if (NP[i].active) net_send(d, len, &NP[i].addr);
    for (int i = 0; i < MAXSPEC; i++) if (SP[i].active) net_send(d, len, &SP[i].addr);
}

static int spec_count(void) {
    int n = 0;
    for (int i = 0; i < MAXSPEC; i++) n += SP[i].active;
    return n;
}

static void pack_state(NetPState *s) {
    vf(s->pos, P.pos); vf(s->vel, P.vel); vf(s->hookPos, P.hookPos);
    s->yaw = P.yaw; s->pitch = P.pitch;
    s->flags = (unsigned char)((P.crouch ? NF_CROUCH : 0) | (P.sliding ? NF_SLIDE : 0) | (P.wallrun ? NF_WALLRUN : 0) |
                               (P.hook != HOOK_NONE ? NF_HOOK : 0) | (P.dead ? NF_DEAD : 0) |
                               (P.parryWin > 0 ? NF_PARRY : 0));
    s->weapon = (unsigned char)P.weapon;
    s->melee = (unsigned char)(clampf(P.meleeT / 0.25f, 0, 1) * 255);
    s->pad = 0;
}

static void find_local_ip(void) {
    char hn[256];
    struct addrinfo hints, *res, *p;
    snprintf(hostIpText, sizeof hostIpText, "?");
    if (gethostname(hn, sizeof hn) != 0) return;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    if (getaddrinfo(hn, NULL, &hints, &res) != 0) return;
    hostIpText[0] = 0;
    for (p = res; p; p = p->ai_next) {
        const char *s = inet_ntoa(((struct sockaddr_in *)p->ai_addr)->sin_addr);
        if (strncmp(s, "127.", 4) == 0 || strlen(hostIpText) + strlen(s) + 4 >= sizeof hostIpText) continue;
        if (hostIpText[0]) strcat(hostIpText, "   ");
        strcat(hostIpText, s);
    }
    freeaddrinfo(res);
    if (!hostIpText[0]) snprintf(hostIpText, sizeof hostIpText, "127.0.0.1");
}

/* --- evenements ----------------------------------------------------- */
static int ev_seen(unsigned int id) {
    for (int i = 0; i < 512; i++) if (seenIds[i] == id) return 1;
    seenIds[seenHead] = id;
    seenHead = (seenHead + 1) % 512;
    return 0;
}

static void net_queue(const NetEvent *ev) {
    outq[outqHead].ev = *ev;
    outq[outqHead].left = 3;
    outqHead = (outqHead + 1) % OUTQ;
}

static void apply_event(const NetEvent *ev);
static int host_handle(const NetEvent *ev);

/* cree un evenement local : l'hote l'arbitre tout de suite, puis il part sur le reseau
 * (sauf si l'hote l'annule : coup pare, cible morte...) */
static void net_emit(NetEvent *ev) {
    if (netMode == NET_OFF || !connected) return;
    ev->eid = (((unsigned int)myId + 1) << 24) | (++evCounter & 0xFFFFFF);
    ev->from = (unsigned char)myId;
    ev_seen(ev->eid);
    if (netMode == NET_HOST && host_handle(ev)) return;
    net_queue(ev);
}

static NetEvent ev_new(int kind) {
    NetEvent e;
    memset(&e, 0, sizeof e);
    e.kind = (unsigned char)kind;
    return e;
}

static void net_hit(int target, float dmg, V3 knock, int weapon) {
    if (target != myId && target < MAXP) dmg_number(np_center(target), fminf(dmg, PVP_HIT_CAP));
    NetEvent e = ev_new(EV_HIT);
    e.target = (unsigned char)target; e.sub = (unsigned char)weapon;
    e.f[0] = dmg; vf(&e.f[1], knock);
    net_emit(&e);
}
static void net_fx_rail(V3 a, V3 b, float k) {
    NetEvent e = ev_new(EV_RAIL);
    vf(e.f, a); vf(e.f + 3, b); e.f[6] = k;
    net_emit(&e);
}
static void net_fx_rocket(V3 pos, V3 vel, unsigned int netId) {
    NetEvent e = ev_new(EV_ROCKET);
    e.netId = netId; vf(e.f, pos); vf(e.f + 3, vel);
    net_emit(&e);
}
static void net_fx_rocket_end(unsigned int netId, V3 pos) {
    NetEvent e = ev_new(EV_ROCKET_END);
    e.netId = netId; vf(e.f, pos);
    net_emit(&e);
}
static void net_fx_parry(unsigned int netId, V3 pos, V3 vel, float mult, int parries) {
    NetEvent e = ev_new(EV_PARRY);
    e.netId = netId; vf(e.f, pos); vf(e.f + 3, vel); e.f[6] = mult; e.sub = (unsigned char)parries;
    net_emit(&e);
}
static void net_fx_coin(int sub, unsigned int netId, V3 pos, V3 vel) {
    NetEvent e = ev_new(EV_COIN);
    e.sub = (unsigned char)sub; e.netId = netId; vf(e.f, pos); vf(e.f + 3, vel);
    net_emit(&e);
}
static void net_fx_melee(V3 pos, V3 dir, float sp) {
    NetEvent e = ev_new(EV_MELEE);
    vf(e.f, pos); vf(e.f + 3, dir); e.f[6] = sp;
    net_emit(&e);
}

static void kfeed_add(int killer, int victim, int kind, int info) {
    memmove(&kfeed[1], &kfeed[0], sizeof kfeed - sizeof kfeed[0]);
    kfeed[0].killer = killer; kfeed[0].victim = victim; kfeed[0].kind = kind; kfeed[0].info = info; kfeed[0].t = 6;
}

static Proj *find_proj(unsigned int netId) {
    for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].netId == netId) return &PR[i];
    return NULL;
}

static void pickup_taken(int i, int who) {
    PU[i].t = PICKUP_RESPAWN;
    netlog("recharge : %s prend la cellule %d", pname(who), i);
    burst(PU[i].pos, v3(0.4f, 0.9f, 1.0f), 25, 5, 0.5f, 0.12f);
    if (who == myId) {
        P.railAmmo = P.railAmmo + PICKUP_AMMO > RAIL_AMMO_MAX ? RAIL_AMMO_MAX : P.railAmmo + PICKUP_AMMO;
        snprintf(popText, sizeof popText, "+%d RAIL", PICKUP_AMMO);
        popT = 1.0f;
        play(S_PAD, 0.5f, 1.8f);
    } else {
        play(S_PAD, 0.3f * atten(PU[i].pos), 1.8f);
    }
}

/* effets d'un evenement venu d'ailleurs (ou d'une decision de l'hote) */
static void apply_event(const NetEvent *ev) {
    V3 a = fv(ev->f), b = fv(ev->f + 3);
    switch (ev->kind) {
    case EV_RAIL: {
        float k = ev->f[6];
        add_beam(a, b, v3(0.3f + 0.7f * k, 0.85f - 0.05f * k, 1.0f - 0.7f * k), 0.6f);
        for (int i = 0; i < 12; i++) add_part(b, vmul(rand_dir(), 4), v3(0.6f, 0.9f, 1.0f), 0.4f, 0.1f, 10);
        play(S_RAIL, 0.6f * atten(a), frange(0.97f, 1.03f));
        break;
    }
    case EV_ROCKET: {
        Proj *p = spawn_proj(PR_ROCKET, a, b);
        if (p) { p->owner = ev->from; p->netId = ev->netId; }
        play(S_ROCKET, 0.7f * atten(a), 1.0f);
        break;
    }
    case EV_ROCKET_END: {
        Proj *p = find_proj(ev->netId);
        if (p) { p->alive = 0; explode_fx(a); }
        break;
    }
    case EV_PARRY: {
        Proj *p = find_proj(ev->netId);
        if (!p) p = spawn_proj(PR_PARRIED, a, b);
        if (p) {
            p->kind = PR_PARRIED; p->owner = ev->from; p->netId = ev->netId; p->pos = a; p->vel = b; p->life = 4;
            p->mult = ev->f[6]; p->parries = ev->sub; p->orb = 0;
        }
        for (int k = 0; k < 20; k++) add_part(a, vmul(rand_dir(), 8), v3(1, 1, 0.85f), 0.35f, 0.1f, 0);
        play(S_PARRY, 0.8f * atten(a), 1.0f);
        break;
    }
    case EV_HIT: {
        int t = ev->target;
        if (t == myId) {
            if (ev->from != myId) {                       /* recul applique par la victime */
                P.vel = vadd(P.vel, fv(ev->f + 1));
                if (ev->f[2] > 2) P.grounded = 0;
            }
            P.hurtFlash = 0.6f; shake += 0.25f;
            play(S_HURT, 0.8f, frange(0.9f, 1.1f));
            if (ev->from != myId && ev->from < MAXP) { hurtFrom = np_center(ev->from); hurtDirT = 1.2f; }
        } else if (t < MAXP && NP[t].active) {
            burst(np_center(t), v3(1, 0.35f, 0.3f), 14, 6, 0.3f, 0.1f);
            NP[t].flashT = 0.12f;
        }
        break;
    }
    case EV_MELEE:
        for (int k = 0; k < 10; k++) add_part(vadd(a, vmul(b, 1.5f + k * 0.15f)), vmul(b, 3), v3(0.9f, 0.9f, 1), 0.15f, 0.08f, 0);
        play(S_DASH, 0.4f * atten(a), 1.8f);
        break;
    case EV_KILL: {
        int k = ev->actor, v = ev->target;
        if (v >= MAXP) break;
        kfeed_add(k, v, ev->sub, 0);
        V3 at = v == myId ? pcenter() : np_center(v);
        burst(at, v3(1, 0.4f, 0.3f), 70, 12, 0.9f, 0.18f);
        shards(at, v3(0.9f, 0.5f, 0.4f), 16, 11);
        if (k == myId && v != myId) { impact_frame(at, 0.08f); killMarkT = 0.45f; }
        play(S_EXPLO, 0.7f * atten(at), 1.3f);
        NP[v].alive = 0;
        if (v == myId) {
            P.dead = 1; P.deathT = 0; P.hook = HOOK_NONE; P.hookEnemy = P.hookPlayer = 0;
            killerOfMe = k;
        }
        if (k == myId && v != myId) {
            play(S_KILL, 0.7f, 1.0f);
            combo++; comboT = 3; hitMarker = 0.25f;
            snprintf(popText, sizeof popText, "ELIMINE %s", pname(v));
            popT = 1.5f;
        }
        netlog("kill : %s -> %s (arme %d)", pname(k), pname(v), ev->sub);
        break;
    }
    case EV_RESPAWN: {
        int t = ev->target;
        if (t >= MAXP) break;
        NP[t].alive = 1; NP[t].disp = a; vf(NP[t].st.pos, a);
        if (t == myId) {
            P.pos = P.prevPos = a; P.vel = v3(0, 0, 0);
            P.dead = 0; P.hp = 100; P.hook = HOOK_NONE; P.hookEnemy = P.hookPlayer = 0;
            P.dashCharge = 2; P.airJumps = 1; P.crouch = P.sliding = P.wallrun = 0;
            P.railAmmo = RAIL_AMMO_START;
            P.coins = COIN_MAX;
            P.yaw = atan2f(-a.x, a.z); P.pitch = 0; P.eyeH = EYE_STAND; P.roll = 0;
            shotStreak = 0;
        }
        burst(vadd(a, v3(0, 1, 0)), v3(0.4f, 0.9f, 1), 40, 6, 0.6f, 0.12f);
        netlog("respawn : %s en (%.0f, %.0f, %.0f)", pname(t), a.x, a.y, a.z);
        break;
    }
    case EV_MATCH:
        if (ev->sub == 0) {
            matchOver = 0;
            memset(kfeed, 0, sizeof kfeed);
            snprintf(msg, sizeof msg, "COMBAT !");
            msgT = 2;
            play(S_WAVE, 0.7f, 1.0f);
            netlog("debut du match");
        } else {
            matchOver = 1; matchWinner = ev->target; matchEndT = 8;
            snprintf(msg, sizeof msg, "%s GAGNE", pname(ev->target));
            msgT = 4;
            play(S_WAVE, 0.8f, 0.8f);
            netlog("fin du match, gagnant : %s", pname(ev->target));
        }
        break;
    case EV_COIN:                                         /* piece d'un autre joueur : visible, mais pas a nous */
        if (ev->sub == 0) {
            for (int i = 0; i < MAXCOIN; i++)
                if (!CO[i].alive) {
                    CO[i].alive = 1; CO[i].owner = ev->from; CO[i].netId = ev->netId;
                    CO[i].pos = a; CO[i].vel = b; CO[i].life = 4; CO[i].spin = 0;
                    break;
                }
        } else {
            for (int i = 0; i < MAXCOIN; i++)
                if (CO[i].alive && CO[i].netId == ev->netId) {
                    CO[i].alive = 0;
                    for (int k = 0; k < 16; k++) add_part(CO[i].pos, vmul(rand_dir(), 6), v3(1, 0.85f, 0.3f), 0.3f, 0.1f, 0);
                    play(S_PARRY, 0.45f * atten(CO[i].pos), 1.5f);
                }
        }
        break;
    case EV_MELEE_PARRY: {                                /* epee paree (ou choc de deux epees) */
        int def = ev->actor, att = ev->target, clash = ev->sub;
        if (def >= MAXP || att >= MAXP) break;
        V3 pd = def == myId ? pcenter() : np_center(def), pa = att == myId ? pcenter() : np_center(att);
        V3 mid = vlerp(pd, pa, 0.5f);
        for (int k = 0; k < 30; k++) add_part(mid, vmul(rand_dir(), 10), v3(1, 0.95f, 0.7f), 0.35f, 0.1f, 0);
        play(S_PUNCH, 0.8f * atten(mid), 1.6f);
        if (def == myId) {
            parry_feedback(mid);
            snprintf(popText, sizeof popText, clash ? "CHOC D'EPEES !" : "PARRY !");
            if (clash) P.vel = vadd(P.vel, vadd(vmul(vnorm(vsub(pd, pa)), 8), v3(0, 3, 0)));
        }
        if (att == myId) {                                /* l'attaquant est repousse, epee bloquee */
            P.vel = vadd(P.vel, vadd(vmul(vnorm(vsub(pa, pd)), clash ? 8.0f : 11.0f), v3(0, 3, 0)));
            P.grounded = 0;
            P.meleeCd = fmaxf(P.meleeCd, clash ? MELEE_CD : MELEE_PARRIED_CD);
            hitstop = fmaxf(hitstop, 0.1f);
            shake += 0.3f;
            snprintf(popText, sizeof popText, clash ? "CHOC D'EPEES !" : "PARE PAR %s", pname(def));
            popT = 1.2f;
        }
        netlog("%s : %s -> %s", clash ? "choc d'epees" : "epee paree", pname(att), pname(def));
        break;
    }
    case EV_PICKUP_TAKEN:
        if (ev->sub < npu) pickup_taken(ev->sub, ev->target);
        break;
    case EV_INFO:
        kfeed_add(-1, ev->target, 0, ev->sub ? 2 : 1);
        netlog("%s %s", pname(ev->target), ev->sub ? "a quitte la partie" : "a rejoint la partie");
        break;
    }
}

/* --- hote : arbitrage ------------------------------------------------ */
static void host_end_match(int winner) {
    if (matchOver) return;
    matchOver = 1; matchWinner = winner; matchEndT = 8;
    NetEvent e = ev_new(EV_MATCH);
    e.sub = 1; e.target = (unsigned char)winner;
    net_emit(&e);
    apply_event(&e);
}

static void host_kill(int killer, int victim, int kind) {
    NP[victim].alive = 0; NP[victim].hp = 0; NP[victim].deaths++; NP[victim].respawnT = RESPAWN_TIME;
    if (killer != victim && killer < MAXP && NP[killer].active) {
        NP[killer].kills++;
        if (NP[killer].alive) NP[killer].hp = fminf(100, NP[killer].hp + KILL_HEAL);
    } else if (NP[victim].kills > 0) {
        NP[victim].kills--;                               /* suicide */
    }
    NetEvent e = ev_new(EV_KILL);
    e.actor = (unsigned char)killer; e.target = (unsigned char)victim; e.sub = (unsigned char)kind;
    net_emit(&e);
    apply_event(&e);
    if (killer != victim && killer < MAXP && NP[killer].kills >= FRAG_LIMIT) host_end_match(killer);
}

static int hitCount[7], parryCount;                       /* statistiques pour le journal du bot */
static double lifeFirstHitT[MAXP], ttkSum;                /* temps pour tuer : du premier degat (a pleine vie) au kill */
static int ttkN, oneShots, meleeParries, clashes;

static void host_take_pickup(int i, int who) {
    if (i >= npu || PU[i].t > 0 || who >= MAXP || !NP[who].alive) return;   /* premier arrive, premier servi */
    NetEvent e = ev_new(EV_PICKUP_TAKEN);
    e.sub = (unsigned char)i; e.target = (unsigned char)who;
    net_emit(&e);
    apply_event(&e);
}

/* arbitre un evenement ; renvoie 1 s'il est annule : il ne doit alors ni s'appliquer ni etre relaye
 * (sinon la victime d'un coup pare subirait quand meme le recul et le flash) */
static int host_handle(const NetEvent *ev) {
    if (ev->kind == EV_PICKUP) { host_take_pickup(ev->sub, ev->from); return 0; }
    if (ev->kind == EV_HIT) {
        int v = ev->target;
        hitCount[ev->sub % 7]++;
        netlog("degats : %s -> %s, %.0f (arme %d), PV avant %.0f", pname(ev->from), pname(v), ev->f[0], ev->sub, NP[v < MAXP ? v : 0].hp);
        if (v >= MAXP || !NP[v].active || !NP[v].alive || matchOver) return 1;
        /* epee ou impact de grappin pendant la fenetre de parry de la victime : c'est pare */
        if ((ev->sub == W_MELEE || ev->sub == W_HOOK) && nowT - NP[v].lastParryT < MELEE_PARRY_GRACE) {
            int clash = ev->sub == W_MELEE && ev->from < MAXP && nowT - NP[ev->from].lastParryT < MELEE_PARRY_GRACE;
            NetEvent e = ev_new(EV_MELEE_PARRY);
            e.actor = (unsigned char)v; e.target = ev->from; e.sub = (unsigned char)clash;
            NP[v].hp = fminf(100, NP[v].hp + PARRY_HEAL);
            if (clash) clashes++; else meleeParries++;
            net_emit(&e);
            apply_event(&e);
            return 1;
        }
        float before = NP[v].hp;
        float dmg = ev->sub == W_SELF ? ev->f[0] : fminf(ev->f[0], PVP_HIT_CAP);   /* plafond par touche */
        if (before >= 99.9f) lifeFirstHitT[v] = nowT;
        NP[v].hp -= dmg;
        if (NP[v].hp <= 0) {
            if (ev->sub != W_SELF) {
                ttkSum += nowT - lifeFirstHitT[v]; ttkN++;
                if (before >= 99.9f) oneShots++;          /* tue d'un seul coup a pleine vie */
            }
            host_kill(ev->from, v, ev->sub);
        }
    } else if (ev->kind == EV_PARRY && ev->from < MAXP) {
        parryCount++;
        NP[ev->from].hp = fminf(100, NP[ev->from].hp + PARRY_HEAL);
    }
    return 0;
}

static V3 pick_spawn(int who) {
    static const float pts[][3] = {
        { 0, 0, 24 }, { 0, 0, -24 }, { 24, 0, 0 }, { -24, 0, 0 }, { -30, 0, -30 }, { 30, 0, -30 }, { 30, 0, 30 },
        { -30, 0, 30 }, { 36, 7, 0 }, { -36, 7, 0 }, { 0, 7, 36 }, { 0, 7, -36 }, { 5, 4, 5 }, { -46, 0, 0 }, { 46, 0, 0 },
    };
    float best = -1;
    V3 bp = v3(0, 0, 24);
    for (int k = 0; k < (int)(sizeof pts / sizeof pts[0]); k++) {
        V3 p = v3(ms(pts[k][0]), pts[k][1], ms(pts[k][2]));
        if (!pos_free(p, H_STAND)) continue;
        float md = 200;                                   /* le plus loin possible des autres */
        for (int i = 0; i < MAXP; i++) {
            if (i == who || !NP[i].active || !NP[i].alive) continue;
            V3 o = i == myId ? P.pos : NP[i].disp;
            md = fminf(md, vlen(vsub(p, o)));
        }
        md += frange(0, 6);
        if (md > best) { best = md; bp = p; }
    }
    return bp;
}

static void host_respawn(int i) {
    V3 p = pick_spawn(i);
    NP[i].alive = 1; NP[i].hp = 100;
    NetEvent e = ev_new(EV_RESPAWN);
    e.target = (unsigned char)i; vf(e.f, p);
    net_emit(&e);
    apply_event(&e);
}

static void host_start_match(void) {
    matchOver = 0; matchT = 0;
    for (int i = 0; i < MAXP; i++) NP[i].kills = NP[i].deaths = 0;
    NetEvent e = ev_new(EV_MATCH);
    net_emit(&e);
    apply_event(&e);
    for (int i = 0; i < MAXP; i++) if (NP[i].active) host_respawn(i);
}

static void host_drop(int i) {
    if (!NP[i].active) return;
    NetEvent e = ev_new(EV_INFO);
    e.sub = 1; e.target = (unsigned char)i;
    net_emit(&e);
    apply_event(&e);
    NP[i].active = 0; NP[i].alive = 0;
}

static void host_send_roster(void) {
    PkRoster r;
    head(&r.h, PK_ROSTER);
    for (int i = 0; i < MAXP; i++) memcpy(r.names[i], NP[i].name, 16);
    host_send_all(&r, sizeof r);
}

static void host_tick(float dt) {
    pack_state(&NP[myId].st);
    if (P.parryWin > 0) NP[myId].lastParryT = nowT;
    NP[myId].disp = P.pos;
    P.hp = NP[myId].hp;
    if (!matchOver) {
        matchT += dt;
        if (matchT >= MATCH_TIME) {
            int w = 0;
            for (int i = 0; i < MAXP; i++) if (NP[i].active && NP[i].kills > NP[w].kills) w = i;
            host_end_match(w);
        }
        for (int i = 0; i < MAXP; i++)
            if (NP[i].active && !NP[i].alive && (NP[i].respawnT -= dt) <= 0) host_respawn(i);
    } else if (matchEndT <= 0) {
        host_start_match();
    }
}

static void host_on_join(const PkJoin *pj, const struct sockaddr_in *from) {
    int id = -1;
    if (pj->spec) {                                       /* spectateur : pas de slot NP, il recoit juste le flux */
        int k = -1;
        for (int i = 0; i < MAXSPEC; i++) if (SP[i].active && same_addr(&SP[i].addr, from)) k = i;
        for (int i = 0; i < MAXSPEC && k < 0; i++) if (!SP[i].active) k = i;
        PkWelcome w;
        head(&w.h, PK_WELCOME); w.id = MAXP; w.refused = k < 0; w.pad[0] = w.pad[1] = 0;
        net_send(&w, sizeof w, from);
        if (k >= 0) {
            if (!SP[k].active) netlog("spectateur %d arrive", k);
            SP[k].active = 1; SP[k].addr = *from; SP[k].lastHeard = nowT;
            host_send_roster();
        }
        return;
    }
    for (int i = 1; i < MAXP; i++) if (NP[i].active && same_addr(&NP[i].addr, from)) id = i;
    if (id < 0) {
        for (int i = 1; i < MAXP; i++) if (!NP[i].active) { id = i; break; }
        if (id < 0) {
            PkWelcome w;
            head(&w.h, PK_WELCOME); w.id = 0; w.refused = 1; w.pad[0] = w.pad[1] = 0;
            net_send(&w, sizeof w, from);
            return;
        }
        memset(&NP[id], 0, sizeof NP[id]);
        NP[id].active = 1;
        memcpy(NP[id].name, pj->name, 15);
        NP[id].name[15] = 0;
        NP[id].addr = *from;
        NP[id].lastHeard = nowT;
        NP[id].respawnT = 1.0f;
        NetEvent e = ev_new(EV_INFO);
        e.sub = 0; e.target = (unsigned char)id;
        net_emit(&e);
        apply_event(&e);
    }
    PkWelcome w;
    head(&w.h, PK_WELCOME); w.id = (unsigned char)id; w.refused = 0; w.pad[0] = w.pad[1] = 0;
    net_send(&w, sizeof w, from);
    host_send_roster();
}

/* evenement recu d'un client : l'hote l'arbitre, l'applique chez lui et le relaie a tout le monde,
 * sauf s'il l'a annule */
static void host_event_in(const NetEvent *ev) {
    if (host_handle(ev)) return;
    apply_event(ev);
    net_queue(ev);
}

static void host_recv(PkHead *h, int n, struct sockaddr_in *from) {
    int id = -1, sp = -1;
    for (int i = 1; i < MAXP; i++) if (NP[i].active && same_addr(&NP[i].addr, from)) id = i;
    for (int i = 0; i < MAXSPEC; i++) if (SP[i].active && same_addr(&SP[i].addr, from)) sp = i;
    if (id >= 0) NP[id].lastHeard = nowT;
    if (sp >= 0) SP[sp].lastHeard = nowT;
    switch (h->type) {
    case PK_QUERY: {
        PkAnnounce a;
        int count = 0;
        for (int i = 0; i < MAXP; i++) count += NP[i].active;
        head(&a.h, PK_ANNOUNCE);
        memcpy(a.name, myName, 16);
        a.players = (unsigned char)count; a.maxp = MAXP; a.pad[0] = a.pad[1] = 0;
        net_send(&a, sizeof a, from);
        break;
    }
    case PK_JOIN:
        if (n >= (int)sizeof(PkJoin)) host_on_join((const PkJoin *)h, from);
        break;
    case PK_STATE:
        if (id >= 0 && n >= (int)sizeof(PkState)) {
            NP[id].st = ((PkState *)h)->st; NP[id].recvT = nowT;
            if (NP[id].st.flags & NF_PARRY) NP[id].lastParryT = nowT;
        }
        break;
    case PK_EVENT:
        if (id >= 0 && n >= (int)sizeof(PkEvent)) {
            NetEvent ev = ((PkEvent *)h)->ev;
            if (ev.from != id || ev_seen(ev.eid)) break;  /* un client ne parle qu'en son nom */
            host_event_in(&ev);
        }
        break;
    case PK_LEAVE:
        if (id >= 0) host_drop(id);
        if (sp >= 0) { SP[sp].active = 0; netlog("spectateur %d part", sp); }
        break;
    }
}

/* --- client ----------------------------------------------------------- */
static void dm_local_start(void) {
    reset_game();
    dmMode = 1;
    P.dead = 1; P.deathT = 0; P.hp = 0;
    memset(kfeed, 0, sizeof kfeed);
    killerOfMe = -1;
}

static void net_shutdown(void) {
    if (sock != INVALID_SOCKET) {
        PkHead l;
        head(&l, PK_LEAVE);
        for (int r = 0; r < 3; r++) {
            if (netMode == NET_HOST) host_send_all(&l, sizeof l);
            else if (connected) net_send(&l, sizeof l, &hostAddr);
        }
        closesocket(sock);
        sock = INVALID_SOCKET;
        netlog("deconnexion");
    }
    netMode = NET_OFF; connected = 0; joining = 0; dmMode = 0; matchOver = 0; myId = 0; nFound = 0;
    spectating = 0; specFollow = -1;
    memset(NP, 0, sizeof NP);
    memset(SP, 0, sizeof SP);
    online_reset();
    memset(outq, 0, sizeof outq);
}

static void menu_backdrop(void) {
    reset_game();
    P.pos = P.prevPos = v3(0, 7, ms(36));
    P.pitch = -0.12f;
}

static void net_disconnect_to_menu(const char *why) {
    net_shutdown();
    set_status(why);
    set_capture(0);
    appState = APP_MENU;
    menu_backdrop();
}

static void on_welcome(int id) {
    myId = id;
    spectating = id >= MAXP; specFollow = -1;
    connected = 1; joining = 0;
    lastSnapT = nowT;
    memset(NP, 0, sizeof NP);
    dm_local_start();
    if (spectating) { P.pos = P.prevPos = v3(0, 12, ms(36)); P.pitch = -0.2f; P.yaw = 0; }   /* vue d'ensemble de la carte */
    appState = APP_GAME;
    if (!botMode && GetForegroundWindow() == hwnd) set_capture(1);
    netlog("connecte a la partie, id %d", id);
}

static void apply_snap(const PkSnap *s) {
    lastSnapT = nowT;
    matchT = s->matchT;
    matchOver = s->matchOver;
    matchWinner = s->winner;
    for (int i = 0; i < npu; i++) {                       /* l'hote dit quelles recharges sont la */
        if (s->pickMask & (1 << i)) PU[i].t = 0;
        else if (PU[i].t <= 0) PU[i].t = PICKUP_RESPAWN;
    }
    for (int i = 0; i < MAXP; i++) {
        const SnapP *sp = &s->p[i];
        int wasActive = NP[i].active, wasAlive = NP[i].alive;
        NP[i].active = sp->active; NP[i].kills = sp->kills; NP[i].deaths = sp->deaths; NP[i].hp = sp->hp;
        if (i == myId) { P.hp = sp->hp; NP[i].alive = sp->alive; continue; }
        if (!sp->active) continue;
        NP[i].st = sp->st;
        NP[i].recvT = nowT;
        if (!wasActive || (!wasAlive && sp->alive)) NP[i].disp = fv(sp->st.pos);
        NP[i].alive = sp->alive;
    }
}

static void found_add(const PkAnnounce *a, const struct sockaddr_in *from) {
    int k = -1;
    for (int i = 0; i < nFound; i++) if (same_addr(&found[i].addr, from)) k = i;
    if (k < 0) {
        if (nFound >= MAXFOUND) return;
        k = nFound++;
    }
    memcpy(found[k].name, a->name, 16);
    found[k].name[15] = 0;
    found[k].addr = *from;
    found[k].players = a->players; found[k].maxp = a->maxp;
    found[k].seen = nowT;
}

static void client_recv(PkHead *h, int n, struct sockaddr_in *from) {
    int fromHost = connected && same_addr(from, &hostAddr);
    switch (h->type) {
    case PK_ANNOUNCE:
        if (!connected && n >= (int)sizeof(PkAnnounce)) found_add((const PkAnnounce *)h, from);
        break;
    case PK_WELCOME:
        if (joining && n >= (int)sizeof(PkWelcome)) {
            const PkWelcome *w = (const PkWelcome *)h;
            if (w->refused) { joining = 0; set_status(joinSpec ? "plus de place pour les spectateurs" : "partie pleine (8 joueurs max)"); }
            else on_welcome(w->id);
        }
        break;
    case PK_SNAP:
        if (fromHost && n >= (int)sizeof(PkSnap)) apply_snap((const PkSnap *)h);
        break;
    case PK_ROSTER:
        if (fromHost && n >= (int)sizeof(PkRoster)) {
            const PkRoster *r = (const PkRoster *)h;
            for (int i = 0; i < MAXP; i++) { memcpy(NP[i].name, r->names[i], 16); NP[i].name[15] = 0; }
        }
        break;
    case PK_EVENT:
        if (fromHost && n >= (int)sizeof(PkEvent)) {
            NetEvent ev = ((PkEvent *)h)->ev;
            if (!ev_seen(ev.eid)) apply_event(&ev);
        }
        break;
    case PK_LEAVE:
        if (fromHost) net_disconnect_to_menu("l'hote a ferme la partie");
        break;
    }
}

static void net_poll(void) {
    char buf[2048];
    for (;;) {
        if (sock == INVALID_SOCKET) return;
        struct sockaddr_in from;
        int fl = sizeof from;
        int n = recvfrom(sock, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
        if (n == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAECONNRESET || err == WSAEMSGSIZE) continue;  /* ICMP "port injoignable" : on ignore */
            return;
        }
        struct sockaddr_in mapped;
        if (n >= 20 && stun_parse((const unsigned char *)buf, n, &mapped)) { stun_got(&mapped); continue; }
        if (n < (int)sizeof(PkHead)) continue;
        PkHead *h = (PkHead *)buf;
        if (h->magic != NET_MAGIC || h->ver != NET_VER) continue;
        pktIn++;
        if (netMode == NET_HOST) host_recv(h, n, &from);
        else if (netMode == NET_CLIENT) client_recv(h, n, &from);
    }
}

static void flush_events(void) {
    for (int i = 0; i < OUTQ; i++) {
        if (outq[i].left <= 0) continue;
        PkEvent pe;
        head(&pe.h, PK_EVENT);
        pe.ev = outq[i].ev;
        if (netMode == NET_HOST) host_send_all(&pe, sizeof pe);
        else net_send(&pe, sizeof pe, &hostAddr);
        outq[i].left--;
    }
}

static void net_update(float dt) {
    if (sock == INVALID_SOCKET) return;
    net_poll();
    if (sock == INVALID_SOCKET) return;
    lb_update(dt);
    sendAcc += dt; rosterAcc += dt; queryAcc += dt; joinAcc += dt;
    if (netMode == NET_HOST) {
        for (int i = 1; i < MAXP; i++)
            if (NP[i].active && nowT - NP[i].lastHeard > 5) host_drop(i);
        for (int i = 0; i < MAXSPEC; i++)
            if (SP[i].active && nowT - SP[i].lastHeard > 5) { SP[i].active = 0; netlog("spectateur %d perdu", i); }
        if (sendAcc >= 1.0f / 60) {
            sendAcc = fminf(sendAcc - 1.0f / 60, 0.05f);
            PkSnap s;
            memset(&s, 0, sizeof s);
            head(&s.h, PK_SNAP);
            s.matchT = matchT; s.matchOver = (unsigned char)matchOver; s.winner = (unsigned char)matchWinner;
            for (int i = 0; i < npu; i++) if (PU[i].t <= 0) s.pickMask |= (unsigned short)(1 << i);
            for (int i = 0; i < MAXP; i++) {
                s.p[i].active = (unsigned char)NP[i].active; s.p[i].alive = (unsigned char)NP[i].alive;
                s.p[i].hp = (short)ceilf(NP[i].hp); s.p[i].kills = (short)NP[i].kills; s.p[i].deaths = (short)NP[i].deaths;
                s.p[i].st = NP[i].st;
            }
            host_send_all(&s, sizeof s);
            flush_events();
        }
        if (rosterAcc >= 0.5f) { rosterAcc = 0; host_send_roster(); }
    } else if (netMode == NET_CLIENT) {
        if (!connected) {
            if (queryAcc >= 1.0f) {                       /* qui heberge sur le reseau local ? */
                queryAcc = 0;
                if (!lanOff) {
                    PkHead q;
                    head(&q, PK_QUERY);
                    struct sockaddr_in b;
                    memset(&b, 0, sizeof b);
                    b.sin_family = AF_INET; b.sin_port = htons(NET_PORT);
                    b.sin_addr.s_addr = htonl(INADDR_BROADCAST);
                    net_send(&q, sizeof q, &b);
                    b.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                    net_send(&q, sizeof q, &b);
                }
                for (int i = 0; i < nFound; i++)          /* les salons du lobby partent avec "gone", pas avec le temps */
                    if (!found[i].online && nowT - found[i].seen > 3.5) { found[i] = found[--nFound]; i--; }
            }
            if (joining && joinAcc >= (joinOnline ? 0.2f : 0.4f)) {   /* en ligne : les paquets repetes percent le NAT */
                joinAcc = 0;
                PkJoin j;
                head(&j.h, PK_JOIN);
                memcpy(j.name, myName, 16);
                j.spec = (unsigned char)joinSpec; j.pad[0] = j.pad[1] = j.pad[2] = 0;
                net_send(&j, sizeof j, &hostAddr);
                if (nowT - joinStartT > (joinOnline ? 10 : 5)) {
                    joining = 0;
                    set_status(joinOnline ? "connexion impossible (NAT strict ?) - essaie la saisie d'IP" : "pas de reponse de l'hote");
                }
            }
        } else if (spectating) {
            if (sendAcc >= 0.5f) {                        /* le spectateur ne joue pas : il dit seulement qu'il est la */
                sendAcc = 0;
                PkHead k;
                head(&k, PK_KEEP);
                net_send(&k, sizeof k, &hostAddr);
            }
            if (nowT - lastSnapT > 5) { net_disconnect_to_menu("connexion perdue avec l'hote"); return; }
        } else {
            if (sendAcc >= 1.0f / 60) {
                sendAcc = fminf(sendAcc - 1.0f / 60, 0.05f);
                PkState s;
                head(&s.h, PK_STATE);
                pack_state(&s.st);
                net_send(&s, sizeof s, &hostAddr);
                flush_events();
            }
            if (nowT - lastSnapT > 5) { net_disconnect_to_menu("connexion perdue avec l'hote"); return; }
        }
    }
    for (int i = 0; i < MAXP; i++) {                     /* lissage des autres joueurs */
        if (i == myId || !NP[i].active) continue;
        float ahead = (float)fmin(nowT - NP[i].recvT, 0.1);
        V3 target = vadd(fv(NP[i].st.pos), vmul(fv(NP[i].st.vel), ahead));
        if (vlen(vsub(target, NP[i].disp)) > 4) NP[i].disp = target;
        else NP[i].disp = vlerp(NP[i].disp, target, fminf(1, dt * 25));
    }
}

static int net_host_start(void) {
    net_shutdown();
    if (!net_open(NET_PORT)) { set_status("impossible d'heberger : le port UDP 27960 est deja utilise"); return 0; }
    netMode = NET_HOST; myId = 0; connected = 1;
    memset(NP, 0, sizeof NP);
    NP[0].active = 1;
    memcpy(NP[0].name, myName, 16);
    find_local_ip();
    dm_local_start();
    matchOver = 0; matchT = 0;
    host_respawn(0);
    netlog("partie hebergee, ip %s", hostIpText);
    return 1;
}

static int net_browse_start(void) {
    net_shutdown();
    if (!net_open(0)) { set_status("reseau indisponible"); return 0; }
    netMode = NET_CLIENT;
    queryAcc = 1.0f;
    netlog("recherche de parties");
    return 1;
}

static void net_connect_ex(struct sockaddr_in addr, int spec, int online) {
    if (netMode != NET_CLIENT && !net_browse_start()) return;
    hostAddr = addr;
    joining = 1; joinStartT = nowT; joinAcc = 1.0f;
    joinSpec = spec; joinOnline = online;
    char buf[96];
    snprintf(buf, sizeof buf, "connexion a %s ...", inet_ntoa(addr.sin_addr));
    set_status(buf);
}

static void net_connect(struct sockaddr_in addr) { net_connect_ex(addr, 0, 0); }

/* ------------------------------------------------------------------ */
/* Parties en ligne : lobby (WebSocket WinHTTP, dans un fil) + STUN     */
/*                                                                     */
/* Le lobby (worker/) tient la liste des salons et met hote et client  */
/* en relation ; le jeu, lui, reste en UDP direct entre joueurs. Les   */
/* adresses publiques viennent d'une requete STUN faite depuis le      */
/* socket du jeu (meme port => meme mapping NAT). Voir docs/PLAN_EN_LIGNE.md */
/* ------------------------------------------------------------------ */
#define LOBBY_DEFAULT "velocity-lobby.CHANGE-ME.workers.dev"   /* a remplacer apres `wrangler deploy` (ou lobby= dans velocity.ini) */
#define STUN_HOST     "stun.l.google.com"
#define STUN_PORT     19302
#define LB_Q          48
#define LB_LINE       256

enum { LB_OFF, LB_CONNECTING, LB_ON, LB_FAILED };
static char lobbyAddr[96] = LOBBY_DEFAULT;                /* "hote" (wss, 443) ou "hote:port" (ws si le port n'est pas 443) */
static int onlineOn = 1;                                  /* chercher et publier les parties en ligne */
static int lanEp;                                         /* --lanep : adresses locales a la place de STUN (tests sur une machine) */
static int lbWantHost;                                    /* le lobby n'a trouve aucune partie : do_action() en heberge une */

static volatile LONG lbState, lbStop;
static CRITICAL_SECTION lbCs;
static int lbCsReady;
static char lbIn[LB_Q][LB_LINE], lbOut[LB_Q][LB_LINE];
static int lbInN, lbOutN;
static HANDLE lbEvt, lbReaderTh, lbWriterTh;
static HINTERNET lbSes, lbCon, lbWs;
static struct sockaddr_in stunAddr, myPub;
static volatile LONG stunReady;
static int myPubOk, lbGreeted, lbLastPlayers = -1, lbLastSpec = -1;
static double lbFailT, stunT, lbHostT, reqT, reqSentT;
static char reqKind, reqId[8];                            /* demande en attente de STUN : 'j' = rejoindre reqId, 'q' = partie rapide */
static int reqSpec;
typedef struct { struct sockaddr_in addr; float left, acc; } Punch;
static Punch punchQ[4];                                   /* clients a qui l'hote ouvre son NAT */

static int parse_ep(const char *s, struct sockaddr_in *a) {
    int p[5];
    if (sscanf(s, "%d.%d.%d.%d:%d", &p[0], &p[1], &p[2], &p[3], &p[4]) != 5) return 0;
    for (int i = 0; i < 4; i++) if (p[i] < 0 || p[i] > 255) return 0;
    if (p[4] <= 0 || p[4] > 65535) return 0;
    memset(a, 0, sizeof *a);
    a->sin_family = AF_INET; a->sin_port = htons((unsigned short)p[4]);
    a->sin_addr.s_addr = htonl(((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | (unsigned)p[3]);
    return 1;
}

static void ep_text(const struct sockaddr_in *a, char *out, int n) {
    snprintf(out, n, "%s:%d", inet_ntoa(a->sin_addr), ntohs(a->sin_port));
}

/* --- STUN (RFC 5389) : "quelle est mon adresse vue d'Internet ?" ------- */
static void stun_send(void) {
    if (sock == INVALID_SOCKET || !stunReady) return;
    unsigned char b[20] = { 0, 1, 0, 0, 0x21, 0x12, 0xA4, 0x42 };
    for (int i = 8; i < 20; i++) b[i] = (unsigned char)(rand() & 255);
    sendto(sock, (const char *)b, 20, 0, (const struct sockaddr *)&stunAddr, sizeof stunAddr);
}

/* reponse "Binding Success" -> adresse publique (XOR-MAPPED-ADDRESS, ou MAPPED-ADDRESS) */
static int stun_parse(const unsigned char *b, int n, struct sockaddr_in *out) {
    if (n < 20 || b[0] != 0x01 || b[1] != 0x01 || b[4] != 0x21 || b[5] != 0x12 || b[6] != 0xA4 || b[7] != 0x42) return 0;
    int len = (b[2] << 8) | b[3];
    if (20 + len > n) len = n - 20;
    for (int p = 20; p + 4 <= 20 + len;) {
        int t = (b[p] << 8) | b[p + 1], l = (b[p + 2] << 8) | b[p + 3];
        if (p + 4 + l > 20 + len) break;
        const unsigned char *v = b + p + 4;
        if ((t == 0x0020 || t == 0x0001) && l >= 8 && v[1] == 1) {
            unsigned short port = (unsigned short)((v[2] << 8) | v[3]);
            unsigned char ip[4] = { v[4], v[5], v[6], v[7] };
            if (t == 0x0020) { port ^= 0x2112; ip[0] ^= 0x21; ip[1] ^= 0x12; ip[2] ^= 0xA4; ip[3] ^= 0x42; }
            memset(out, 0, sizeof *out);
            out->sin_family = AF_INET; out->sin_port = htons(port); memcpy(&out->sin_addr, ip, 4);
            return 1;
        }
        p += 4 + ((l + 3) & ~3);
    }
    return 0;
}

static void stun_got(const struct sockaddr_in *a) {
    char t[40];
    if (myPubOk && same_addr(&myPub, a)) return;
    myPub = *a; myPubOk = 1;
    lbLastPlayers = -1;                                   /* l'hote republie son salon avec la nouvelle adresse */
    ep_text(a, t, sizeof t);
    netlog("adresse publique (STUN) : %s", t);
}

/* adresse LAN de cette machine (route par defaut) : pour deux joueurs derriere la meme box */
static void lan_ep_text(char *out, int n, int port) {
    snprintf(out, n, "127.0.0.1:%d", port);
    if (lanEp || !wsa_init()) return;
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return;
    struct sockaddr_in to, me;
    int ml = sizeof me;
    memset(&to, 0, sizeof to);
    to.sin_family = AF_INET; to.sin_port = htons(53); to.sin_addr.s_addr = inet_addr("8.8.8.8");
    if (connect(s, (struct sockaddr *)&to, sizeof to) == 0 && getsockname(s, (struct sockaddr *)&me, &ml) == 0 &&
        me.sin_addr.s_addr != 0)                          /* connect() sur UDP n'envoie rien : il choisit juste la route */
        snprintf(out, n, "%s:%d", inet_ntoa(me.sin_addr), port);
    closesocket(s);
}

/* --- fil WinHTTP : une connexion WebSocket texte vers le lobby ---------- */
static DWORD WINAPI lb_writer(LPVOID arg) {
    (void)arg;
    while (!lbStop && lbState == LB_ON) {
        DWORD w = WaitForSingleObject(lbEvt, 25000);
        if (lbStop || lbState != LB_ON) break;
        for (;;) {
            char line[LB_LINE];
            EnterCriticalSection(&lbCs);
            int have = lbOutN > 0;
            if (have) {
                memcpy(line, lbOut[0], LB_LINE);
                memmove(lbOut[0], lbOut[1], (size_t)(lbOutN - 1) * LB_LINE);
                lbOutN--;
            }
            LeaveCriticalSection(&lbCs);
            if (!have) break;
            if (WinHttpWebSocketSend(lbWs, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, line, (DWORD)strlen(line)) != NO_ERROR) {
                lbState = LB_FAILED;
                return 0;
            }
        }
        if (w == WAIT_TIMEOUT && WinHttpWebSocketSend(lbWs, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, "ping", 4) != NO_ERROR) {
            lbState = LB_FAILED;
            return 0;
        }
    }
    return 0;
}

static DWORD WINAPI lb_reader(LPVOID arg) {
    (void)arg;
    char addr[96];
    wchar_t whost[96];
    snprintf(addr, sizeof addr, "%s", lobbyAddr);
    int port = 443;
    char *colon = strchr(addr, ':');
    if (colon) { *colon = 0; port = atoi(colon + 1); }
    MultiByteToWideChar(CP_UTF8, 0, addr, -1, whost, 96);

    if (!lanEp && !stunReady) {                           /* resolu ici pour ne jamais bloquer le jeu sur le DNS */
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(STUN_HOST, NULL, &hints, &res) == 0) {
            stunAddr = *(struct sockaddr_in *)res->ai_addr;
            stunAddr.sin_port = htons(STUN_PORT);
            stunReady = 1;
            freeaddrinfo(res);
        }
    }

    HINTERNET ses = WinHttpOpen(L"VELOCITY", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET con = NULL, req = NULL, ws = NULL;
    if (ses) {
        WinHttpSetTimeouts(ses, 5000, 5000, 5000, 60000);   /* le serveur repond a nos "ping" : 60 s sans rien = connexion morte */
        con = WinHttpConnect(ses, whost, (INTERNET_PORT)port, 0);
    }
    if (con) req = WinHttpOpenRequest(con, L"GET", L"/", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                      port == 443 ? WINHTTP_FLAG_SECURE : 0);
    if (req && WinHttpSetOption(req, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0) &&
        WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, NULL))
        ws = WinHttpWebSocketCompleteUpgrade(req, 0);
    if (req) WinHttpCloseHandle(req);

    EnterCriticalSection(&lbCs);
    int stopped = lbStop;
    if (!stopped) { lbSes = ses; lbCon = con; lbWs = ws; }
    LeaveCriticalSection(&lbCs);
    if (stopped || !ws) {
        if (ws) WinHttpCloseHandle(ws);
        if (con) WinHttpCloseHandle(con);
        if (ses) WinHttpCloseHandle(ses);
        if (!stopped) lbState = LB_FAILED;
        return 0;
    }

    lbState = LB_ON;
    lbWriterTh = CreateThread(NULL, 0, lb_writer, NULL, 0, NULL);
    char buf[LB_LINE];
    int len = 0;
    for (;;) {
        DWORD got = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE bt;
        if (WinHttpWebSocketReceive(ws, buf + len, (DWORD)(sizeof buf - 1 - len), &got, &bt) != NO_ERROR) break;
        if (bt == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) break;
        len += (int)got;
        if (bt == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) {
            buf[len] = 0;
            EnterCriticalSection(&lbCs);
            if (lbInN < LB_Q) memcpy(lbIn[lbInN++], buf, (size_t)len + 1);
            LeaveCriticalSection(&lbCs);
            len = 0;
        } else if (len >= (int)sizeof buf - 8) len = 0;   /* ligne beaucoup trop longue : on jette */
    }
    if (!lbStop) lbState = LB_FAILED;
    SetEvent(lbEvt);                                      /* reveille le fil d'ecriture pour qu'il se termine */
    return 0;
}

static void lb_stop(void) {
    if (!lbCsReady) return;
    lbStop = 1;
    SetEvent(lbEvt);
    EnterCriticalSection(&lbCs);
    HINTERNET ws = lbWs, con = lbCon, ses = lbSes;
    lbWs = lbCon = lbSes = NULL;
    LeaveCriticalSection(&lbCs);
    if (ws) WinHttpCloseHandle(ws);                       /* fait echouer le WinHttpWebSocketReceive en cours */
    if (con) WinHttpCloseHandle(con);
    if (ses) WinHttpCloseHandle(ses);
    if (lbReaderTh) { WaitForSingleObject(lbReaderTh, 1500); CloseHandle(lbReaderTh); lbReaderTh = NULL; }
    if (lbWriterTh) { WaitForSingleObject(lbWriterTh, 1500); CloseHandle(lbWriterTh); lbWriterTh = NULL; }
    lbState = LB_OFF; lbStop = 0; lbInN = lbOutN = 0; lbGreeted = 0;
}

static void lb_start(void) {
    if (lbState == LB_CONNECTING || lbState == LB_ON) return;
    if (!lbCsReady) {
        InitializeCriticalSection(&lbCs);
        lbEvt = CreateEventA(NULL, FALSE, FALSE, NULL);
        lbCsReady = 1;
    }
    if (lbReaderTh || lbWriterTh) lb_stop();
    wsa_init();
    lbStop = 0; lbInN = lbOutN = 0;
    lbState = LB_CONNECTING;
    lbReaderTh = CreateThread(NULL, 0, lb_reader, NULL, 0, NULL);
    if (!lbReaderTh) lbState = LB_FAILED;
    netlog("lobby : connexion a %s", lobbyAddr);
}

static void lb_send(const char *fmt, ...) {
    char b[LB_LINE];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    if (!lbCsReady || lbState != LB_ON) return;
    EnterCriticalSection(&lbCs);
    if (lbOutN < LB_Q) memcpy(lbOut[lbOutN++], b, strlen(b) + 1);
    LeaveCriticalSection(&lbCs);
    SetEvent(lbEvt);
    netlog("lobby >> %s", b);
}

static void online_reset(void) {
    lb_stop();
    myPubOk = 0; reqKind = 0; reqSentT = -100;
    lbLastPlayers = lbLastSpec = -1; stunT = 0; lbHostT = 0;
    memset(punchQ, 0, sizeof punchQ);
}

static const char *lb_status_text(void) {
    if (!onlineOn) return "parties en ligne desactivees (PARAMETRES)";
    switch (lbState) {
    case LB_CONNECTING: return "connexion au lobby en ligne ...";
    case LB_ON: return myPubOk ? "en ligne" : "en ligne - recherche de ton adresse publique (STUN) ...";
    case LB_FAILED: return "lobby injoignable - nouvel essai ...";
    }
    return "";
}

static void lb_safe_name(char *out, int n) {                 /* le lobby n'accepte que [A-Za-z0-9_.-] */
    int k = 0;
    for (const char *s = myName; *s && k < n - 1; s++)
        out[k++] = (isalnum((unsigned char)*s) || *s == '.' || *s == '-') ? *s : '_';
    out[k] = 0;
    if (!k) snprintf(out, n, "joueur");
}

static void lb_room(const char *id, int players, int maxp, int spec, const char *ep, const char *name) {
    struct sockaddr_in a;
    if (!parse_ep(ep, &a)) return;
    int k = -1;
    for (int i = 0; i < nFound; i++) if (found[i].online && !strcmp(found[i].id, id)) k = i;
    if (k < 0) {
        if (nFound >= MAXFOUND) return;
        k = nFound++;
        memset(&found[k], 0, sizeof found[k]);
    }
    snprintf(found[k].id, sizeof found[k].id, "%s", id);
    snprintf(found[k].name, sizeof found[k].name, "%s", name);
    found[k].addr = a; found[k].players = players; found[k].maxp = maxp; found[k].spec = spec;
    found[k].online = 1; found[k].seen = nowT;
}

static void lb_line(const char *s) {
    char a[64], b[64], c[64];
    int n1, n2, n3;
    netlog("lobby << %s", s);
    if (sscanf(s, "room %7s %d %d %d %63s %63s", a, &n1, &n2, &n3, b, c) == 6) lb_room(a, n1, n2, n3, b, c);
    else if (sscanf(s, "gone %7s", a) == 1) {
        for (int i = 0; i < nFound; i++) if (found[i].online && !strcmp(found[i].id, a)) { found[i] = found[--nFound]; break; }
    } else if (sscanf(s, "go %63s %d", a, &n1) == 2) {
        struct sockaddr_in t;
        if (parse_ep(a, &t)) net_connect_ex(t, n1, 1);
    } else if (sscanf(s, "punch %63s", a) == 1) {
        struct sockaddr_in t;
        if (netMode != NET_HOST || !parse_ep(a, &t)) return;
        int k = 0;                                        /* un emplacement libre, sinon le plus ancien */
        for (int i = 0; i < 4; i++) if (punchQ[i].left <= 0) { k = i; break; }
        punchQ[k].addr = t; punchQ[k].left = 4; punchQ[k].acc = 1;
    } else if (!strcmp(s, "none")) {
        set_status("aucune partie en ligne : tu en crees une");
        lbWantHost = 1;
    } else if (!strcmp(s, "err full")) set_status("partie pleine (8 joueurs max)");
    else if (!strcmp(s, "err unknown")) set_status("cette partie n'existe plus");
}

static void lb_update(float dt) {
    int hosting = netMode == NET_HOST, browsing = netMode == NET_CLIENT && !connected;
    if (!onlineOn || (!hosting && !browsing)) { if (lbState != LB_OFF) lb_stop(); return; }

    if (lbState == LB_FAILED) {
        if (lbFailT == 0) lbFailT = nowT;
        if (nowT - lbFailT > 4) { lbFailT = 0; lb_stop(); lb_start(); }
    } else if (lbState == LB_OFF) lb_start();

    if (lbState == LB_ON && !lbGreeted) {                 /* (re)connecte : on remet le lobby au courant */
        lbGreeted = 1;
        for (int i = 0; i < nFound; i++) if (found[i].online) { found[i] = found[--nFound]; i--; }
        lbLastPlayers = -1;
        if (browsing) lb_send("list %d", NET_VER);
    }
    if (lbState == LB_FAILED) lbGreeted = 0;

    if (lbCsReady) {                                      /* lignes recues */
        static char tmp[LB_Q][LB_LINE];
        int n;
        EnterCriticalSection(&lbCs);
        n = lbInN;
        memcpy(tmp, lbIn, (size_t)n * LB_LINE);
        lbInN = 0;
        LeaveCriticalSection(&lbCs);
        for (int i = 0; i < n; i++) lb_line(tmp[i]);
    }

    if (!myPubOk) {                                       /* notre adresse vue d'Internet */
        if (lanEp) {
            struct sockaddr_in me;
            int ml = sizeof me;
            if (getsockname(sock, (struct sockaddr *)&me, &ml) == 0) {
                me.sin_addr.s_addr = inet_addr("127.0.0.1");
                stun_got(&me);
            }
        } else if (stunReady && nowT - stunT > 0.7) { stunT = nowT; stun_send(); }
    } else if (hosting && !lanEp && nowT - stunT > 30) { stunT = nowT; stun_send(); }

    if (lbState == LB_ON && hosting && myPubOk) {
        int pl = 0, sp = spec_count();
        for (int i = 0; i < MAXP; i++) pl += NP[i].active;
        if (pl != lbLastPlayers || sp != lbLastSpec || nowT - lbHostT > 20) {
            char pub[40], lan[40], nm[16];
            lbLastPlayers = pl; lbLastSpec = sp; lbHostT = nowT;
            ep_text(&myPub, pub, sizeof pub);
            lan_ep_text(lan, sizeof lan, NET_PORT);
            lb_safe_name(nm, sizeof nm);
            lb_send("host %d %d %d %d %s %s %s", NET_VER, pl, MAXP, sp, pub, lan, nm);
        }
    }

    if (reqKind) {
        if (lbState == LB_ON && myPubOk) {
            char me[40];
            ep_text(&myPub, me, sizeof me);
            if (reqKind == 'j') lb_send("join %s %s %d", reqId, me, reqSpec);
            else lb_send("quick %d %s", NET_VER, me);
            reqKind = 0; reqSentT = nowT;
        } else if (nowT - reqT > 10) {
            set_status(lbState == LB_ON ? "adresse publique introuvable (STUN bloque ?)" : "lobby injoignable");
            reqKind = 0;
        }
    }

    for (int i = 0; i < 4; i++) {                         /* l'hote ouvre son NAT vers les clients annonces */
        if (punchQ[i].left <= 0) continue;
        punchQ[i].left -= dt; punchQ[i].acc += dt;
        if (punchQ[i].acc >= 0.1f) {
            PkHead p;
            punchQ[i].acc = 0;
            head(&p, PK_PUNCH);
            net_send(&p, sizeof p, &punchQ[i].addr);
        }
    }
}

/* rejoindre / regarder la partie found[i] : directement (LAN) ou via le lobby (en ligne) */
static void join_found(int i, int spec) {
    if (i < 0 || i >= nFound) return;
    if (found[i].online) {
        reqKind = 'j'; reqSpec = spec; reqT = nowT;
        snprintf(reqId, sizeof reqId, "%s", found[i].id);
        set_status(spec ? "demande pour regarder la partie ..." : "demande de connexion ...");
    } else net_connect_ex(found[i].addr, spec, 0);
}

static void quick_play(void) {
    if (!net_browse_start()) return;
    appState = APP_JOIN;
    reqKind = 'q'; reqT = nowT;
    set_status("recherche d'une partie ...");
}

static int join_busy(void) { return joining || reqKind || nowT - reqSentT < 4; }

/* --- bot de test : joue tout seul pour verifier le reseau ------------- */
static float botTime = 20, botT, botNextHook, botNextDash, botSpeedSum, botAliveT, botMaxSpeed;
static char botShot[260];
static int botSpec;                                       /* --spec : le bot client regarde la partie au lieu d'y jouer */

/* Le bot joue comme un joueur de movement shooter : bhop permanent, air-strafe en balancant
 * la vue autour de la cible (en tenant le strafe du cote ou il tourne), dash et grappin pour
 * combler la distance, visee recalee au moment de tirer, corps a corps au contact. */
static void bot_think(float dt) {
    botT += dt;
    if (appState == APP_JOIN && nFound > 0 && !join_busy()) join_found(0, botSpec);
    if (appState != APP_GAME) return;
    memset(botHeld, 0, sizeof botHeld);
    botHeld[A_FIRE] = 0;
    if (P.dead) return;
    float hs = hlen(P.vel);
    botSpeedSum += hs * dt; botAliveT += dt; botMaxSpeed = fmaxf(botMaxSpeed, hs);

    /* un adversaire tout proche lance son epee : une fois sur deux, on pare (ou on frappe en meme temps) */
    static double swingSeenT;
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i) || NP[i].st.melee < 150 || nowT - swingSeenT < 0.4) continue;
        if (vlen(vsub(NP[i].disp, P.pos)) > 5) continue;
        swingSeenT = nowT;
        if (frand() < 0.5f && P.meleeCd <= 0) {
            V3 d = vsub(np_center(i), eye_pos());
            P.yaw = atan2f(d.x, -d.z);
            botPress[A_MELEE] = 1;
            netlog("parry d'epee tente contre %s", pname(i));
            return;
        }
    }

    /* une roquette adverse arrive : on tente un parry au bon moment, mais pas a tous les coups */
    static unsigned int parryDecided;
    static int parryGo;
    for (int i = 0; i < MAXPR; i++) {
        Proj *p = &PR[i];
        if (!p->alive || (p->kind != PR_ROCKET && p->kind != PR_PARRIED) || p->owner == myId) continue;
        V3 to = vsub(eye_pos(), p->pos);
        float d = vlen(to);
        if (d > 10 || vdot(p->vel, to) <= 0) continue;
        if (parryDecided != p->netId) { parryDecided = p->netId; parryGo = frand() < 0.6f; }
        if (parryGo && d < MELEE_RANGE + 1.2f && P.meleeCd <= 0) {
            V3 back = vmul(to, -1.0f / d);
            P.yaw = atan2f(back.x, -back.z);
            P.pitch = asinf(clampf(back.y, -1, 1));
            botPress[A_MELEE] = 1;
            botHeld[A_JUMP] = 1;
            netlog("parry tente sur une roquette de %s (%.1f m)", pname(p->owner), d);
            return;
        }
    }

    int best = -1;
    float bd = 1e9f;
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i)) continue;
        float d = vlen(vsub(NP[i].disp, P.pos));
        if (d < bd) { bd = d; best = i; }
    }
    botHeld[A_JUMP] = 1;                                   /* bhop : on ne touche jamais le sol longtemps */
    static float searchYaw, stuckT, escapeT, escapeDir = 1;
    static int escapeHook;
    static float seeT;
    float aimYaw, aimPitch = 0;
    int see = 0;
    if (best >= 0) {
        V3 eye = eye_pos(), c = np_center(best), d = vsub(c, eye);
        aimYaw = atan2f(d.x, -d.z);
        aimPitch = asinf(clampf(d.y / fmaxf(vlen(d), 0.01f), -1, 1));
        see = los(eye, c);
        searchYaw = aimYaw;
    } else {
        aimYaw = (searchYaw += dt * 0.6f);                /* personne : on tourne en cherchant */
    }

    /* presque plus de munitions rail et personne de proche : on va chercher une recharge */
    int seek = -1;
    if (P.railAmmo < 2 && (best < 0 || bd > 25)) {
        float sd = 90;
        for (int i = 0; i < npu; i++) {
            if (PU[i].t > 0 || PU[i].pos.y - P.pos.y > 3) continue;   /* seulement celles a portee de saut */
            float d = vlen(vsub(PU[i].pos, P.pos));
            if (d < sd) { sd = d; seek = i; }
        }
    }
    if (seek >= 0) {
        V3 d = vsub(PU[seek].pos, eye_pos());
        aimYaw = atan2f(d.x, -d.z);
        aimPitch = asinf(clampf(d.y / fmaxf(vlen(d), 0.01f), -1, 1));
        best = -1; bd = 1e9f; see = 0;
    }

    /* comme un humain : temps de reaction, et une visee moins precise de loin */
    if (see) seeT += dt; else seeT = 0;
    float aimErr = 0.012f + (best >= 0 ? bd : 0) * 0.0014f;
    float aimErrYaw = aimErr * sinf(botT * 7.3f + botMode), aimErrPitch = aimErr * 0.6f * cosf(botT * 5.1f);

    /* CROSS-UP : monter au-dessus de l'adversaire (saut + double saut), dasher par-dessus,
     * se retourner et tirer dans son dos. */
    static int crossPhase;
    static float crossT, crossCd;
    static V3 crossDir;
    crossCd -= dt;
    if (best >= 0) {
        V3 tp = NP[best].disp, hd = v3(tp.x - P.pos.x, 0, tp.z - P.pos.z);
        float dy = P.pos.y - tp.y, hdist = hlen(hd);
        if (crossPhase == 0 && crossCd <= 0 && see && hdist > 4 && hdist < 12 && P.dashCharge >= 1 && P.hook == HOOK_NONE) {
            crossPhase = 1; crossT = 0;
            netlog("cross-up : elan vers %s (%.1f m, %.1f m plus haut)", pname(best), hdist, dy);
        }
        if (crossPhase == 1) {                            /* 1. prendre de la hauteur */
            crossT += dt;
            botHeld[A_FWD] = botHeld[A_JUMP] = 1;
            P.yaw = aimYaw; P.pitch = aimPitch;
            if (!P.grounded && P.vel.y < 1.5f && P.airJumps > 0) botPress[A_JUMP] = 1;
            if (dy > 1.8f && !P.grounded) {
                crossPhase = 2; crossT = 0;
                crossDir = vnorm(hd);
                P.yaw = atan2f(crossDir.x, -crossDir.z);
                botPress[A_DASH] = 1;                     /* 2. dash par-dessus */
                netlog("cross-up : dash au-dessus de %s (%.1f m plus haut)", pname(best), dy);
            } else if (crossT > 1.2f) { crossPhase = 0; crossCd = 2; netlog("cross-up : abandon (%.1f m plus haut)", dy); }
        } else if (crossPhase == 2) {
            crossT += dt;
            botHeld[A_FWD] = 1;
            P.yaw = atan2f(crossDir.x, -crossDir.z);
            if (vdot(v3(tp.x - P.pos.x, 0, tp.z - P.pos.z), crossDir) < -1.0f || crossT > 0.7f) { crossPhase = 3; crossT = 0; }
        } else if (crossPhase == 3) {                     /* 3. se retourner et tirer dans le dos */
            crossT += dt;
            botHeld[A_JUMP] = 1;
            P.yaw = aimYaw; P.pitch = aimPitch;
            if (see && P.fireCd <= 0 && P.switchT <= 0) { P.yaw += aimErrYaw; botHeld[A_FIRE] = 1; }
            if (bd < melee_range(rel_speed(np_vel(best))) + 0.5f && P.meleeCd <= 0) botPress[A_MELEE] = 1;
            if (crossT > 0.5f) {
                crossPhase = 0; crossCd = 3;
                netlog("cross-up : retourne, tir sur %s", pname(best));
            }
        }
        if (crossPhase) { botHeld[A_HOOK] = 0; return; }
    } else crossPhase = 0;

    /* coince contre un obstacle : on part en biais, puis de l'autre cote, puis demi-tour */
    if (hs < 3 && P.hook == HOOK_NONE) stuckT += dt; else stuckT = 0;
    if (stuckT > 0.4f) {
        static int escapeStep;
        static const float dirs[3] = { 1.3f, -1.3f, PI };
        escapeDir = dirs[escapeStep++ % 3];
        escapeT = 1.0f; stuckT = 0;
        escapeHook = P.hookCd <= 0;
    }

    /* air-strafe : la vue oscille de +-0.45 rad autour de la cible, strafe du cote ou elle tourne */
    float phase = botT * 2.6f, swing = 0.45f * sinf(phase);
    if (P.grounded || P.hook != HOOK_NONE || bd < 6) { swing = 0; botHeld[A_FWD] = 1; }
    else botHeld[cosf(phase) > 0 ? A_RIGHT : A_LEFT] = 1;
    P.yaw = aimYaw + swing;
    P.pitch = aimPitch;
    if (escapeT > 0) {
        escapeT -= dt;
        memset(botHeld, 0, sizeof botHeld);
        botHeld[A_FWD] = botHeld[A_JUMP] = 1;
        P.yaw = aimYaw + escapeDir;
        if (escapeHook) { P.pitch = 0.7f; botPress[A_HOOK] = 1; escapeHook = 0; }   /* grappin vers le haut pour s'extraire */
        else if (P.dashCharge >= 1 && botT > botNextDash) { botPress[A_DASH] = 1; botNextDash = botT + 1.2f; }
    }

    /* cible cachee derriere un obstacle (ou recharge en hauteur) : grappin vers le haut pour y aller */
    int seekHigh = seek >= 0 && PU[seek].pos.y - P.pos.y > 3;
    if (((best >= 0 && !see) || seekHigh) && P.hook == HOOK_NONE && P.hookCd <= 0 && botT > botNextHook) {
        P.yaw = aimYaw; P.pitch = seekHigh ? aimPitch + 0.25f : 0.6f;
        botPress[A_HOOK] = 1;
        botNextHook = botT + 1.5f;
    }

    /* le bot client alterne rail / roquettes (roquettes seulement de pres), et roquettes si le rail est vide */
    P.weapon = (P.railAmmo <= 0 || (bd < 20 && ((int)(botT / 5 + botMode)) % 2)) ? 1 : 0;
    /* contre-jeu : une piece adverse a portee ? on tire dedans pour la retourner contre son lanceur */
    if (P.weapon == 0 && P.railAmmo > 0 && P.fireCd <= 0 && P.switchT <= 0) {
        for (int i = 0; i < MAXCOIN; i++) {
            if (!CO[i].alive || CO[i].owner == myId || CO[i].life > 3.7f) continue;   /* reaction de 0,3 s */
            if (CO[i].netId % 2) continue;                /* le bot ne contre qu'une piece sur deux */
            V3 eye = eye_pos(), d = vsub(CO[i].pos, eye);
            float l = vlen(d);
            if (l > 45 || !los(eye, CO[i].pos)) continue;
            P.yaw = atan2f(d.x, -d.z);
            P.pitch = asinf(clampf(d.y / l, -1, 1));
            botHeld[A_FIRE] = 1;
            netlog("contre : tir sur la piece de %s", pname(CO[i].owner));
            return;
        }
    }

    static int coinPlan;                                  /* a pleine vitesse : piece, puis tir dedans */
    static float coinPlanT, nextCoin;
    if (!coinPlan && see && bd > 10 && bd < 55 && hs > 11 && P.coins >= 1 && P.railAmmo > 0 && P.weapon == 0 &&
        P.fireCd < 0.3f && botT > nextCoin) {
        P.yaw = aimYaw; P.pitch = aimPitch;
        botPress[A_COIN] = 1;
        coinPlan = 1; coinPlanT = 0; nextCoin = botT + 2.5f;
    } else if (coinPlan) {
        coinPlanT += dt;
        int mine = -1;
        for (int i = 0; i < MAXCOIN; i++) if (CO[i].alive && CO[i].owner == myId) mine = i;
        if (mine < 0 || coinPlanT > 0.9f) coinPlan = 0;
        else if (coinPlanT > 0.3f && see && P.fireCd <= 0 && P.switchT <= 0) {   /* seulement si la cible est encore en vue */
            V3 d = vsub(CO[mine].pos, eye_pos());
            P.yaw = atan2f(d.x, -d.z);
            P.pitch = asinf(clampf(d.y / fmaxf(vlen(d), 0.01f), -1, 1));
            botHeld[A_FIRE] = 1;
            coinPlan = 0;
        }
    }
    if (!coinPlan && see && seeT > 0.3f && bd < 55 && P.fireCd <= 0 && P.switchT <= 0) {   /* recale la visee pour tirer */
        P.yaw = aimYaw + aimErrYaw; P.pitch = aimPitch + aimErrPitch; botHeld[A_FIRE] = 1;
    }

    if (best >= 0 && bd > 14 && P.dashCharge >= 1 && botT > botNextDash) {    /* dash vers la cible */
        memset(botHeld, 0, sizeof botHeld);
        botHeld[A_FWD] = botHeld[A_JUMP] = 1;
        P.yaw = aimYaw;
        botPress[A_DASH] = 1;
        botNextDash = botT + 1.2f;
    }
    if (see && bd > 10 && bd < GRAPPLE_RANGE && P.hook == HOOK_NONE && P.hookCd <= 0 && botT > botNextHook) {
        P.yaw = aimYaw;                                   /* grappin sur la cible pour fondre dessus */
        botPress[A_HOOK] = 1;
        botNextHook = botT + 2.5f;
    }
    /* on garde le grappin tant qu'il tire, et on lache des qu'on revoit la cible (avec l'elan) */
    static float hookHoldT;                               /* ne jamais rester pendu au grappin */
    hookHoldT = (P.hook == HOOK_ATTACHED && !P.hookPlayer) ? hookHoldT + dt : 0;
    botHeld[A_HOOK] = (unsigned char)(botPress[A_HOOK] || P.hook == HOOK_FLYING || P.hookPlayer ||
                                 (P.hook == HOOK_ATTACHED && !see && hookHoldT < 1.5f &&
                                  !(seek >= 0 && P.pos.y > PU[seek].pos.y - 1)));
    if (P.hookPlayer) P.yaw = aimYaw;
    if (best >= 0 && bd < melee_range(rel_speed(np_vel(best))) + 0.5f && P.meleeCd <= 0) { P.yaw = aimYaw; botPress[A_MELEE] = 1; }
    static float logT;
    if ((logT += dt) > 1) {
        logT = 0;
        netlog("bot pos (%.1f %.1f %.1f) vit %.1f sol %d crochet %d cible %.1f m vue %d", P.pos.x, P.pos.y, P.pos.z, hs,
               P.grounded, P.hook, bd, see);
    }
}

/* ramassage des recharges : en solo directement, en LAN l'hote arbitre */
static void pickup_tick(float dt) {
    for (int i = 0; i < npu; i++) {
        if (netMode != NET_CLIENT && PU[i].t > 0) PU[i].t -= dt;
        PU[i].pending -= dt;
        if (P.dead || PU[i].t > 0 || P.railAmmo >= RAIL_AMMO_MAX || PU[i].pending > 0) continue;
        if (vlen(vsub(pcenter(), PU[i].pos)) > 1.7f) continue;
        if (!dmMode) pickup_taken(i, myId);
        else if (netMode == NET_HOST) host_take_pickup(i, myId);
        else {
            NetEvent e = ev_new(EV_PICKUP);
            e.sub = (unsigned char)i;
            net_emit(&e);
            PU[i].pending = 0.5f;
        }
    }
}

static void fx_tick(float dt);

/* Spectateur : il ne simule rien. Camera libre (deplacement + dash = rapide, saut/accroupi = monter/descendre) ;
 * un tir passe au joueur suivant, qu'on suit en vue a la troisieme personne, puis revient a la camera libre. */
static void spec_tick(float dt) {
    input_poll();
    if (actP[A_FIRE]) {
        int n = specFollow;
        do n++; while (n < MAXP && !NP[n].active);
        specFollow = n >= MAXP ? -1 : n;
    }
    if (specFollow >= 0 && !NP[specFollow].active) specFollow = -1;
    V3 f = view_dir();
    P.prevPos = P.pos;
    P.vel = v3(0, 0, 0);
    if (specFollow >= 0) {
        P.pos = vsub(vsub(np_center(specFollow), vmul(f, 3.5f)), v3(0, P.eyeH, 0));
    } else {
        V3 right = v3(cosf(P.yaw), 0, sinf(P.yaw));
        V3 w = vadd(vmul(f, (float)(act[A_FWD] - act[A_BACK])), vmul(right, (float)(act[A_RIGHT] - act[A_LEFT])));
        w.y += (float)(act[A_JUMP] - act[A_CROUCH]);
        P.pos = vadd(P.pos, vmul(vnorm(w), (act[A_DASH] ? 40.0f : 14.0f) * dt));
    }
    proj_tick(dt);                                        /* roquettes et pieces des autres, sans degats pour nous (P.dead) */
    coin_tick(dt);
    if (matchOver) matchEndT -= dt;
    fx_tick(dt);
}

static void game_tick(float dt) {
    /* hitstop : le monde se fige un instant sur les gros coups (les entrees attendent) */
    if (hitstop > 0) { hitstop -= dt; P.prevPos = P.pos; return; }
    if (spectating) { spec_tick(dt); return; }

    int rp = latch[SC_R];                                 /* R : recommencer (solo), non reaffectable */
    input_poll();
    int jp = actP[A_JUMP], dp = actP[A_DASH];
    int w1 = actP[A_WEAP1], w2 = actP[A_WEAP2];
    int lmbp = actP[A_FIRE], swap = actP[A_SWAP], meleePress = actP[A_MELEE], coinPress = actP[A_COIN];
    hookPressIn = actP[A_HOOK];
    hookHeldIn = act[A_HOOK];

    if (P.dead) {
        P.deathT += dt;
        if (!dmMode && (rp || (lmbp && P.deathT > 1.0f))) { reset_game(); return; }
    } else gameTime += dt;

    int nw = P.weapon;
    if (w1) nw = 0;
    if (w2) nw = 1;
    if (swap) nw = 1 - P.weapon;
    if (nw != P.weapon && !P.dead) { P.weapon = nw; P.switchT = 0.22f; }

    player_tick(dt, jp, dp);

    if (P.coins < COIN_MAX) P.coins = fminf(COIN_MAX, P.coins + dt / COIN_RECHARGE);
    if (!P.dead && coinPress && P.coins >= 1) throw_coin();
    coin_tick(dt);

    P.meleeCd -= dt; P.meleeT -= dt;
    if (!P.dead && meleePress && P.meleeCd <= 0) melee();
    if (P.parryWin > 0) {
        if (!P.dead) try_parry();
        P.parryWin -= dt;
        if (P.parryWin <= 0 && !P.swingLanded)            /* coup dans le vide : on paie le risque */
            P.meleeCd = fmaxf(P.meleeCd, MELEE_WHIFF_CD - PARRY_WINDOW);
    }

    P.fireCd -= dt; P.switchT -= dt;
    P.recoil *= expf(-dt * 9);
    if (!P.dead && (act[A_FIRE] || lmbp) && P.fireCd <= 0 && P.switchT <= 0) {
        if (P.weapon == 1) fire_rocket();
        else if (P.railAmmo > 0) fire_rail();
        else {                                            /* chargeur vide : clic, et on passe aux roquettes */
            play(S_HIT, 0.5f, 0.45f);
            snprintf(popText, sizeof popText, "PLUS DE MUNITIONS RAIL");
            popT = 1.0f;
            P.weapon = 1; P.switchT = 0.22f; P.fireCd = 0.2f;
        }
    }

    if (!dmMode) enemy_tick(dt);
    proj_tick(dt);
    pickup_tick(dt);
    if (!dmMode) wave_tick(dt);
    if (matchOver) matchEndT -= dt;
    if (netMode == NET_HOST) host_tick(dt);
    fx_tick(dt);
}

/* decroissance des effets (flux de kills, particules, chiffres...) : joueur et spectateur */
static void fx_tick(float dt) {
    for (int k = 0; k < 6; k++) kfeed[k].t -= dt;

    for (int i = 0; i < MAXPA; i++) {
        Part *q = &PA[i];
        if (q->life <= 0) continue;
        q->life -= dt;
        q->vel.y -= q->grav * dt;
        q->vel = vmul(q->vel, 1 - dt * 1.5f);
        q->pos = vadd(q->pos, vmul(q->vel, dt));
    }
    for (int i = 0; i < MAXBM; i++) BM[i].life -= dt;
    for (int i = 0; i < nb; i++)                         /* particules des jump pads */
        if (boxes[i].kind == B_PAD && frand() < 0.15f) {
            V3 c = vlerp(boxes[i].mn, boxes[i].mx, 0.5f);
            add_part(v3(c.x + frange(-1.3f, 1.3f), 0.35f, c.z + frange(-1.3f, 1.3f)), v3(0, frange(3, 7), 0),
                     v3(0.2f, 0.8f, 1.0f), 0.8f, 0.08f, 0);
        }

    shard_tick(dt);
    for (int i = 0; i < MAXDN; i++) DN[i].t -= dt;
    for (int i = 0; i < MAXP; i++) NP[i].flashT -= dt;
    killMarkT -= dt; hurtDirT -= dt; muzzleT -= dt;
    hitMarker -= dt; P.hurtFlash -= dt; msgT -= dt; popT -= dt; parryFlash -= dt; streakLostT -= dt;
    shake *= expf(-dt * 6);
    if (comboT > 0) { comboT -= dt; if (comboT <= 0) combo = 0; }
}

/* ------------------------------------------------------------------ */
/* Rendu                                                               */
/* ------------------------------------------------------------------ */
static GLuint texGrid, texDot, worldList, fontBase[3];
static int fontW[3], fontH[3];
static V3 camR, camU;
static float fps;

static void make_textures(void) {
    int N = 128;
    unsigned char *img = (unsigned char *)malloc(N * N * 3);
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++) {
            float v = 0.84f + frange(-0.025f, 0.025f);
            if (x < 3 || y < 3 || x >= N - 2 || y >= N - 2) v = 0.5f;
            else if (x == 63 || x == 64 || y == 63 || y == 64) v = 0.7f;
            unsigned char c = (unsigned char)(clampf(v, 0, 1) * 255);
            img[(y * N + x) * 3 + 0] = img[(y * N + x) * 3 + 1] = img[(y * N + x) * 3 + 2] = c;
        }
    glGenTextures(1, &texGrid);
    glBindTexture(GL_TEXTURE_2D, texGrid);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    for (int level = 0, s = N; ; level++) {                /* mipmaps faits main */
        glTexImage2D(GL_TEXTURE_2D, level, GL_RGB, s, s, 0, GL_RGB, GL_UNSIGNED_BYTE, img);
        if (s == 1) break;
        int h = s / 2;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < h; x++)
                for (int c = 0; c < 3; c++) {
                    int sum = img[((2 * y) * s + 2 * x) * 3 + c] + img[((2 * y) * s + 2 * x + 1) * 3 + c] +
                              img[((2 * y + 1) * s + 2 * x) * 3 + c] + img[((2 * y + 1) * s + 2 * x + 1) * 3 + c];
                    img[(y * h + x) * 3 + c] = (unsigned char)(sum / 4);
                }
        s = h;
    }
    free(img);

    int D = 32;
    unsigned char *dot = (unsigned char *)malloc(D * D * 4);
    for (int y = 0; y < D; y++)
        for (int x = 0; x < D; x++) {
            float dx = (x + 0.5f) / D * 2 - 1, dy = (y + 0.5f) / D * 2 - 1;
            float a = clampf(1 - sqrtf(dx * dx + dy * dy), 0, 1);
            unsigned char *p = &dot[(y * D + x) * 4];
            p[0] = p[1] = p[2] = 255; p[3] = (unsigned char)(a * a * 255);
        }
    glGenTextures(1, &texDot);
    glBindTexture(GL_TEXTURE_2D, texDot);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, D, D, 0, GL_RGBA, GL_UNSIGNED_BYTE, dot);
    free(dot);
}

static void quadf(V3 n, V3 a, V3 b, V3 c, V3 d) {
    V3 vs[4] = { a, b, c, d };
    glNormal3f(n.x, n.y, n.z);
    for (int i = 0; i < 4; i++) {
        V3 p = vs[i]; float u, v;
        if (fabsf(n.x) > 0.5f) { u = p.z; v = p.y; }
        else if (fabsf(n.y) > 0.5f) { u = p.x; v = p.z; }
        else { u = p.x; v = p.y; }
        glTexCoord2f(u * 0.5f, v * 0.5f);
        glVertex3f(p.x, p.y, p.z);
    }
}

static void box_faces(V3 a, V3 b) {
    glBegin(GL_QUADS);
    quadf(v3(1, 0, 0),  v3(b.x, a.y, a.z), v3(b.x, b.y, a.z), v3(b.x, b.y, b.z), v3(b.x, a.y, b.z));
    quadf(v3(-1, 0, 0), v3(a.x, a.y, a.z), v3(a.x, a.y, b.z), v3(a.x, b.y, b.z), v3(a.x, b.y, a.z));
    quadf(v3(0, 1, 0),  v3(a.x, b.y, a.z), v3(a.x, b.y, b.z), v3(b.x, b.y, b.z), v3(b.x, b.y, a.z));
    quadf(v3(0, -1, 0), v3(a.x, a.y, a.z), v3(b.x, a.y, a.z), v3(b.x, a.y, b.z), v3(a.x, a.y, b.z));
    quadf(v3(0, 0, 1),  v3(a.x, a.y, b.z), v3(b.x, a.y, b.z), v3(b.x, b.y, b.z), v3(a.x, b.y, b.z));
    quadf(v3(0, 0, -1), v3(a.x, a.y, a.z), v3(a.x, b.y, a.z), v3(b.x, b.y, a.z), v3(b.x, a.y, a.z));
    glEnd();
}

static void box_edges(V3 a, V3 b) {
    float xs[2] = { a.x, b.x }, ys[2] = { a.y, b.y }, zs[2] = { a.z, b.z };
    glBegin(GL_LINES);
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 2; j++) {
            glVertex3f(a.x, ys[i], zs[j]); glVertex3f(b.x, ys[i], zs[j]);
            glVertex3f(xs[i], a.y, zs[j]); glVertex3f(xs[i], b.y, zs[j]);
            glVertex3f(xs[i], ys[j], a.z); glVertex3f(xs[i], ys[j], b.z);
        }
    glEnd();
}

static void build_world_list(void) {
    worldList = glGenLists(1);
    glNewList(worldList, GL_COMPILE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1, 1);
    glEnable(GL_LIGHTING);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texGrid);
    for (int i = 0; i < nb; i++) {
        glColor3f(boxes[i].col.x, boxes[i].col.y, boxes[i].col.z);
        box_faces(boxes[i].mn, boxes[i].mx);
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.5f);
    for (int i = 1; i < nb; i++) {
        if (boxes[i].kind == B_PAD) glColor4f(0.6f, 1.0f, 1.0f, 0.9f);
        else glColor4f(0.05f, 0.05f, 0.1f, 0.45f);
        box_edges(boxes[i].mn, boxes[i].mx);
    }
    glDisable(GL_BLEND);
    glEndList();
}

static void make_font(int i, int px) {
    HFONT f = CreateFontA(-px, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                          ANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    HGDIOBJ old = SelectObject(hdc, f);
    fontBase[i] = glGenLists(224);
    if (!wglUseFontBitmapsA(hdc, 32, 224, fontBase[i])) wglUseFontBitmapsA(hdc, 32, 224, fontBase[i]);
    TEXTMETRICA tm; GetTextMetricsA(hdc, &tm);
    fontW[i] = tm.tmAveCharWidth; fontH[i] = tm.tmHeight;
    SelectObject(hdc, old);
    DeleteObject(f);
}

static void text(int f, float x, float y, const char *s, float r, float g, float b, float a) {
    int n = (int)strlen(s);
    glColor4f(0, 0, 0, a * 0.7f);
    glRasterPos2f(x + 2, y + 2);
    glListBase(fontBase[f] - 32);
    glCallLists(n, GL_UNSIGNED_BYTE, s);
    glColor4f(r, g, b, a);
    glRasterPos2f(x, y);
    glCallLists(n, GL_UNSIGNED_BYTE, s);
}
static void text_c(int f, float cx, float y, const char *s, float r, float g, float b, float a) {
    text(f, cx - fontW[f] * (float)strlen(s) * 0.5f, y, s, r, g, b, a);
}
static void rect(float x, float y, float w, float h, float r, float g, float b, float a) {
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x + w, y); glVertex2f(x + w, y + h); glVertex2f(x, y + h);
    glEnd();
}

static void billboard(V3 p, float s) {
    V3 r = vmul(camR, s), u = vmul(camU, s);
    glTexCoord2f(0, 0); glVertex3f(p.x - r.x - u.x, p.y - r.y - u.y, p.z - r.z - u.z);
    glTexCoord2f(1, 0); glVertex3f(p.x + r.x - u.x, p.y + r.y - u.y, p.z + r.z - u.z);
    glTexCoord2f(1, 1); glVertex3f(p.x + r.x + u.x, p.y + r.y + u.y, p.z + r.z + u.z);
    glTexCoord2f(0, 1); glVertex3f(p.x - r.x + u.x, p.y - r.y + u.y, p.z - r.z + u.z);
}

static void set_frustum(float vfovDeg, float aspect, float zn, float zf) {
    float t = zn * tanf(vfovDeg * PI / 360.0f), r = t * aspect;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-r, r, -t, t, zn, zf);
}

static void draw_sky(float vfov) {
    float horizon = -tanf(P.pitch) / tanf(vfov * PI / 360.0f);   /* y NDC de l'horizon */
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glBegin(GL_QUADS);
    glColor3f(0.04f, 0.05f, 0.12f); glVertex2f(-1, 1); glVertex2f(1, 1);
    glColor3f(0.34f, 0.30f, 0.44f); glVertex2f(1, horizon); glVertex2f(-1, horizon);
    glVertex2f(-1, horizon); glVertex2f(1, horizon); glVertex2f(1, -1); glVertex2f(-1, -1);
    glEnd();
    glEnable(GL_DEPTH_TEST);
}

static void draw_enemy(Enemy *e) {
    V3 col = e->flash > 0 ? v3(1, 1, 1) : enemy_color(e->type);
    glPushMatrix();
    glTranslatef(e->pos.x, e->pos.y, e->pos.z);
    if (e->type == EN_DRONE) {
        glRotatef(e->t * 70, 0, 1, 0);
        glRotatef(e->t * 35, 1, 0, 0.3f);
        glColor3f(col.x * 0.25f, col.y * 0.25f, col.z * 0.25f);
        box_faces(v3(-0.5f, -0.5f, -0.5f), v3(0.5f, 0.5f, 0.5f));
        glColor3f(col.x, col.y, col.z);
        glLineWidth(2);
        box_edges(v3(-0.52f, -0.52f, -0.52f), v3(0.52f, 0.52f, 0.52f));
        box_faces(v3(-0.22f, -0.22f, -0.22f), v3(0.22f, 0.22f, 0.22f));
        glRotatef(45, 0, 1, 0);
        box_edges(v3(-0.75f, -0.08f, -0.75f), v3(0.75f, 0.08f, 0.75f));
    } else {
        float s = 0.6f + 0.1f * sinf(e->t * 20);
        glRotatef(e->t * 400, 0, 1, 0);
        glScalef(s, s, s);
        glBegin(GL_TRIANGLES);
        for (int k = 0; k < 8; k++) {
            float sx = (k & 1) ? 1.0f : -1.0f, sy = (k & 2) ? 1.0f : -1.0f, sz = (k & 4) ? 1.0f : -1.0f;
            float sh = (k & 2) ? 1.0f : 0.6f;
            glColor3f(col.x * sh, col.y * sh, col.z * sh);
            glVertex3f(sx, 0, 0); glVertex3f(0, sy * 1.2f, 0); glVertex3f(0, 0, sz);
        }
        glEnd();
    }
    glPopMatrix();
}

static void draw_viewmodel(float aspect) {
    glClear(GL_DEPTH_BUFFER_BIT);
    set_frustum(60, aspect, 0.01f, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    float hs = hlen(P.vel);
    float bx = (P.grounded && !P.sliding) ? cosf(P.bobT * 0.5f) * 0.012f * fminf(hs / RUN_SPEED, 1) : 0;
    float lower = P.switchT > 0 ? P.switchT / 0.22f : 0;
    if (P.dead) lower = 1.5f;
    glTranslatef(0.2f + bx + P.swayX, -0.19f + P.bob * 0.3f - lower * 0.25f + P.swayY - P.landDip * 0.2f,
                 -0.42f + P.recoil * 0.06f);
    glScalef(0.5f, 0.5f, 0.5f);
    glRotatef(P.recoil * 7, 1, 0, 0);
    glRotatef(-P.roll * 0.5f, 0, 0, 1);
    if (P.sliding) glRotatef(-10, 0, 0, 1);

    GLfloat lp[4] = { -0.3f, 1, 0.6f, 0 };
    glLightfv(GL_LIGHT0, GL_POSITION, lp);
    glEnable(GL_LIGHTING);
    if (P.meleeT > 0) {                                   /* coup d'epee : balayage de droite a gauche */
        float k = clampf(1 - P.meleeT / 0.25f, 0, 1), heat = clampf(vlen(P.vel) / 30, 0, 1);
        glPushMatrix();
        glTranslatef(-0.5f, 0.08f, 0.14f);
        glRotatef(-80 + k * 160, 0, 1, 0);
        glRotatef(-12, 1, 0, 0);
        glRotatef(-35, 0, 0, 1);                          /* lame inclinee */
        glColor3f(0.2f, 0.18f, 0.16f);                    /* poignee + garde */
        box_faces(v3(-0.03f, -0.03f, -0.02f), v3(0.03f, 0.03f, 0.22f));
        box_faces(v3(-0.16f, -0.025f, -0.06f), v3(0.16f, 0.025f, -0.02f));
        glColor3f(0.78f, 0.8f, 0.86f);                    /* lame */
        box_faces(v3(-0.045f, -0.012f, -1.6f), v3(0.045f, 0.012f, -0.06f));
        glDisable(GL_LIGHTING);
        glColor3f(0.4f + heat * 0.6f, 0.8f - heat * 0.3f, 1.0f - heat * 0.8f);   /* le fil chauffe avec la vitesse */
        box_faces(v3(-0.052f, -0.004f, -1.62f), v3(-0.036f, 0.004f, -0.06f));
        glEnable(GL_LIGHTING);
        glPopMatrix();
    }
    if (muzzleT > 0) {                                    /* flash du canon */
        float tip = P.weapon == 0 ? -0.64f : -0.6f, ms2 = 0.12f + 0.25f * (muzzleT / 0.08f);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texDot);
        if (P.weapon == 0) glColor4f(0.5f, 0.9f, 1.0f, 1); else glColor4f(1.0f, 0.6f, 0.2f, 1);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex3f(-ms2, -ms2, tip); glTexCoord2f(1, 0); glVertex3f(ms2, -ms2, tip);
        glTexCoord2f(1, 1); glVertex3f(ms2, ms2, tip); glTexCoord2f(0, 1); glVertex3f(-ms2, ms2, tip);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glEnable(GL_LIGHTING);
    }
    float ready = P.fireCd <= 0 ? 1.0f : 0.35f;
    if (P.weapon == 0) {
        glColor3f(0.25f, 0.27f, 0.32f);
        box_faces(v3(-0.05f, -0.06f, -0.42f), v3(0.05f, 0.04f, 0.12f));
        glColor3f(0.18f, 0.19f, 0.22f);
        box_faces(v3(-0.025f, -0.02f, -0.62f), v3(0.025f, 0.03f, -0.42f));
        box_faces(v3(-0.03f, -0.16f, 0.0f), v3(0.03f, -0.06f, 0.07f));
        glDisable(GL_LIGHTING);
        glColor3f(0.3f * ready, 0.85f * ready, 1.0f * ready);
        box_faces(v3(-0.055f, -0.012f, -0.4f), v3(0.055f, 0.0f, 0.05f));
        box_faces(v3(-0.03f, 0.03f, -0.6f), v3(0.03f, 0.036f, -0.43f));
    } else {
        glColor3f(0.32f, 0.36f, 0.22f);
        box_faces(v3(-0.055f, -0.055f, -0.5f), v3(0.055f, 0.055f, 0.05f));
        glColor3f(0.2f, 0.2f, 0.18f);
        box_faces(v3(-0.068f, -0.068f, -0.56f), v3(0.068f, 0.068f, -0.48f));
        box_faces(v3(-0.025f, -0.17f, -0.1f), v3(0.025f, -0.055f, -0.03f));
        glDisable(GL_LIGHTING);
        glColor3f(1.0f * ready, 0.5f * ready, 0.1f * ready);
        box_faces(v3(-0.058f, 0.015f, -0.3f), v3(0.058f, 0.03f, -0.02f));
    }
}

/* ------------------------------------------------------------------ */
/* Interface : menus, tableau des scores, joueurs distants             */
/* ------------------------------------------------------------------ */
enum { ACT_NONE, ACT_SOLO, ACT_HOST, ACT_BROWSE, ACT_QUIT, ACT_BACK, ACT_JOIN_FOUND, ACT_JOIN_IP, ACT_RESUME,
       ACT_RESTART, ACT_LEAVE, ACT_FULLSCREEN, ACT_QUICK, ACT_SPEC_FOUND };
static int settingsOpen, waitAct = -1, waitSlot, uiDown, uiDrag;
static int fullscreen;
static char cfgPath[MAX_PATH];

static void key_name(int code, char *out, int n) {
    static const char *mouseNames[] = { "Souris G", "Souris D", "Clic molette", "Souris 4", "Souris 5", "Molette" };
    char buf[64];
    if (code <= 0) { snprintf(out, n, "-"); return; }
    if (code >= K_LMB) { snprintf(out, n, "%s", mouseNames[code - K_LMB]); return; }
    if (GetKeyNameTextA((LONG)code << 16, buf, sizeof buf) > 0) snprintf(out, n, "%s", buf);   /* nom selon le clavier */
    else snprintf(out, n, "touche %d", code);
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
    fprintf(f, "name=%s" "\n" "sens=%.5f" "\n" "fov=%.0f" "\n" "volume=%.2f" "\n" "invert_y=%d" "\n" "show_fps=%d" "\n" "fullscreen=%d" "\n",
            myName, sens, fovBase, masterVol, invertY, showFps, fullscreen);
    fprintf(f, "impact_frames=%d" "\n" "screen_fx=%d" "\n", impactFrames, screenFx);
    fprintf(f, "online=%d" "\n", onlineOn);
    if (strcmp(lobbyAddr, LOBBY_DEFAULT)) fprintf(f, "lobby=%s" "\n", lobbyAddr);   /* le defaut peut changer avec une mise a jour */
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
        else if (!strcmp(line, "invert_y")) invertY = atoi(v) != 0;
        else if (!strcmp(line, "show_fps")) showFps = atoi(v) != 0;
        else if (!strcmp(line, "impact_frames")) impactFrames = atoi(v) != 0;
        else if (!strcmp(line, "screen_fx")) screenFx = atoi(v) != 0;
        else if (!strcmp(line, "online")) onlineOn = atoi(v) != 0;
        else if (!strcmp(line, "lobby") && v[0]) snprintf(lobbyAddr, sizeof lobbyAddr, "%s", v);
        else if (!strcmp(line, "fullscreen")) fs = atoi(v) != 0;
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
static GLfloat mvMat[16], prMat[16];
static V3 camEye;

static const float pcol[MAXP][3] = {
    { 0.3f, 0.8f, 1.0f }, { 1.0f, 0.45f, 0.3f }, { 0.5f, 1.0f, 0.4f }, { 1.0f, 0.85f, 0.3f },
    { 0.8f, 0.45f, 1.0f }, { 1.0f, 0.4f, 0.7f }, { 0.4f, 1.0f, 0.9f }, { 0.9f, 0.9f, 0.9f },
};
static const char *weaponNames[] = { "RAIL", "ROQUETTE", "EPEE", "GRAPPIN", "PARRY", "SOLO", "RICOCHET" };

static int button(float x, float y, float w, float h, const char *label) {
    int hover = uiMX >= x && uiMX < x + w && uiMY >= y && uiMY < y + h;
    rect(x, y, w, h, hover ? 0.2f : 0.1f, hover ? 0.42f : 0.12f, hover ? 0.62f : 0.2f, 0.9f);
    rect(x, y + h - 3, w, 3, 0.4f, 0.85f, 1.0f, hover ? 1.0f : 0.45f);
    text_c(1, x + w * 0.5f, y + h * 0.5f + fontH[1] * 0.3f, label, 1, 1, 1, 1);
    if (hover && uiClick) { uiClick = 0; play(S_HIT, 0.4f, 1.5f); return 1; }
    return 0;
}

static void field(float x, float y, float w, float h, const char *label, const char *val, int id) {
    int hover = uiMX >= x && uiMX < x + w && uiMY >= y && uiMY < y + h, focus = uiFocus == id;
    char buf[64];
    text(0, x, y - 8, label, 0.7f, 0.75f, 0.85f, 1);
    rect(x, y, w, h, 0.05f, 0.06f, 0.1f, 0.9f);
    rect(x, y + h - 2, w, 2, focus ? 1.0f : 0.4f, focus ? 0.8f : 0.4f, focus ? 0.35f : 0.5f, 1);
    snprintf(buf, sizeof buf, "%s%s", val, (focus && ((GetTickCount() / 400) & 1)) ? "_" : "");
    text(1, x + 10, y + h * 0.5f + fontH[1] * 0.3f, buf, 1, 1, 1, 1);
    if (hover && uiClick) { uiClick = 0; uiFocus = id; }
}

/* aide : les commandes actuelles (reaffectables dans PARAMETRES) et les astuces de jeu */
static void draw_controls(float x, float y) {
    static const char *tips[] = {
        "air-strafe : tourne la souris + gauche/droite en l'air",
        "epee : degats et portee selon la vitesse RELATIVE ; touche = +1 dash",
        "PARRY (debut du coup d'epee) : roquettes, orbes, kamikazes ET epees",
        "piece + tir dedans au rail = ricochet auto (x1,5) ; vole celles des autres",
        "F2/F3 sensibilite   F11 plein ecran   ECHAP menu",
    };
    char buf[128];
    text(1, x, y, "COMMANDES", 0.5f, 0.9f, 1.0f, 1);
    for (int a = 0; a < A_COUNT; a++) {
        text(0, x, y + 34 + a * 21.0f, actLabel[a], 0.75f, 0.78f, 0.88f, 1);
        text(0, x + 285, y + 34 + a * 21.0f, bind_text(a), 1, 0.9f, 0.55f, 1);
    }
    float ty = y + 44 + A_COUNT * 21.0f;
    for (int i = 0; i < (int)(sizeof tips / sizeof tips[0]); i++)
        text(0, x, ty + i * 21.0f, tips[i], 0.6f, 0.75f, 0.9f, 1);
    snprintf(buf, sizeof buf, "les commandes se changent dans PARAMETRES");
    text(0, x, ty + 5 * 21.0f + 6, buf, 0.5f, 0.55f, 0.65f, 1);
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

static void draw_settings(float cx, float cy) {
    char buf[64];
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.82f);
    text_c(2, cx, cy - 330, "PARAMETRES", 0.5f, 0.9f, 1.0f, 1);
    float lx = cx - 660, lw = 440, y = cy - 250;

    text(1, lx, y - 20, "REGLAGES", 0.5f, 0.9f, 1.0f, 1);
    y += 30;
    snprintf(buf, sizeof buf, "%.2f", sens * 1000);
    sens = slider(1, lx, y, lw, "Sensibilite de la souris", buf, sens, 0.0003f, 0.008f);
    y += 62;
    snprintf(buf, sizeof buf, "%.0f deg", fovBase);
    fovBase = slider(2, lx, y, lw, "Champ de vision (horizontal)", buf, fovBase, 80, 130);
    y += 62;
    snprintf(buf, sizeof buf, "%.0f %%", masterVol * 100);
    masterVol = slider(3, lx, y, lw, "Volume", buf, masterVol, 0, 1);
    y += 50;
    snprintf(buf, sizeof buf, "Inverser l'axe Y : %s", invertY ? "OUI" : "NON");
    if (button(lx, y, lw, 46, buf)) invertY = !invertY;
    y += 58;
    snprintf(buf, sizeof buf, "Afficher les FPS : %s", showFps ? "OUI" : "NON");
    if (button(lx, y, lw, 46, buf)) showFps = !showFps;
    y += 58;
    snprintf(buf, sizeof buf, "Impact frames (flashs) : %s", impactFrames ? "OUI" : "NON");
    if (button(lx, y, lw, 46, buf)) impactFrames = !impactFrames;
    y += 58;
    snprintf(buf, sizeof buf, "Effets d'ecran : %s", screenFx ? "OUI" : "NON");
    if (button(lx, y, lw, 46, buf)) screenFx = !screenFx;
    y += 58;
    snprintf(buf, sizeof buf, "Plein ecran : %s", fullscreen ? "OUI" : "NON");
    if (button(lx, y, lw, 46, buf)) pendingAction = ACT_FULLSCREEN;
    y += 58;
    snprintf(buf, sizeof buf, "Parties en ligne : %s", onlineOn ? "OUI" : "NON");
    if (button(lx, y, lw, 46, buf)) { onlineOn = !onlineOn; cfg_save(); }
    y += 80;
    if (button(lx, y, 210, 50, "RETOUR")) { settingsOpen = 0; waitAct = -1; cfg_save(); }
    if (button(lx + 230, y, 210, 50, "PAR DEFAUT")) { memcpy(binds, bindDefault, sizeof binds); cfg_save(); }

    float rx = cx - 150, cw = 230, ry = cy - 270;
    text(1, rx, ry, "COMMANDES", 0.5f, 0.9f, 1.0f, 1);
    text(0, rx + 200, ry, waitAct >= 0 ? "appuie sur une touche ou un bouton (ECHAP : annuler, SUPPR : effacer)"
                                       : "clique une case pour la changer", 0.7f, 0.75f, 0.85f, 1);
    for (int a = 0; a < A_COUNT; a++) {
        float yy = ry + 22 + a * 36.0f;
        text(0, rx, yy + 24, actLabel[a], 0.85f, 0.87f, 0.95f, 1);
        for (int k = 0; k < 2; k++) {
            char kn[32];
            float bx = rx + 270 + k * (cw + 12);
            if (waitAct == a && waitSlot == k) snprintf(kn, sizeof kn, "...");
            else key_name(binds[a][k], kn, sizeof kn);
            int hover = uiMX >= bx && uiMX < bx + cw && uiMY >= yy && uiMY < yy + 32;
            int waiting = waitAct == a && waitSlot == k;
            rect(bx, yy, cw, 32, waiting ? 0.45f : hover ? 0.2f : 0.08f, waiting ? 0.35f : hover ? 0.4f : 0.1f,
                 waiting ? 0.1f : hover ? 0.6f : 0.18f, 0.9f);
            text_c(0, bx + cw * 0.5f, yy + 22, kn, 1, 1, 1, 1);
            if (hover && uiClick) { uiClick = 0; waitAct = a; waitSlot = k; }
        }
    }
    if (uiClick) uiClick = 0;
}

static void draw_status(float x, float y) {
    if (netStatus[0] && nowT - netStatusT < 8) text(0, x, y, netStatus, 1, 0.65f, 0.3f, 1);
}

static void draw_main_menu(float cx, float cy) {
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.7f);
    text_c(2, cx, cy - 300, "V E L O C I T Y", 0.5f, 0.9f, 1.0f, 1);
    text_c(0, cx, cy - 268, "movement shooter en C", 0.7f, 0.7f, 0.8f, 1);
    float bx = cx - 620, bw = 420, y = cy - 200;
    field(bx, y, bw, 48, "PSEUDO", myName, 1);
    y += 80;
    if (button(bx, y, bw, 54, "SOLO  -  VAGUES")) pendingAction = ACT_SOLO;
    y += 68;
    if (button(bx, y, bw, 54, "PARTIE RAPIDE  -  EN LIGNE")) pendingAction = ACT_QUICK;
    y += 68;
    if (button(bx, y, bw, 54, "HEBERGER UNE PARTIE")) pendingAction = ACT_HOST;
    y += 68;
    if (button(bx, y, bw, 54, "REJOINDRE / REGARDER")) pendingAction = ACT_BROWSE;
    y += 68;
    if (button(bx, y, bw, 54, "PARAMETRES")) settingsOpen = 1;
    y += 68;
    if (button(bx, y, bw, 54, "QUITTER")) pendingAction = ACT_QUIT;
    draw_status(bx, y + 90);
    draw_controls(cx - 130, cy - 200);
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
            snprintf(buf, sizeof buf, "[EN LIGNE]  %s    %d/%d joueurs%s", found[i].name, found[i].players, found[i].maxp,
                     found[i].spec ? "  (spectateurs)" : "");
        else snprintf(buf, sizeof buf, "[LAN]  %s    %d/%d joueurs    %s", found[i].name, found[i].players, found[i].maxp,
                      inet_ntoa(found[i].addr.sin_addr));
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
    if (!dmMode) {
        if (button(bx, y, bw, 54, "RECOMMENCER")) pendingAction = ACT_RESTART;
        y += 68;
    }
    if (button(bx, y, bw, 54, "PARAMETRES")) settingsOpen = 1;
    y += 68;
    if (button(bx, y, bw, 54, dmMode ? "QUITTER LA PARTIE" : "MENU PRINCIPAL")) pendingAction = ACT_LEAVE;
    draw_controls(cx - 130, cy - 200);
    if (uiClick) { uiClick = 0; pendingAction = ACT_RESUME; }  /* clic ailleurs = reprendre */
}

static void draw_scoreboard(float cx, float cy) {
    int order[MAXP], n = 0;
    char buf[64];
    for (int i = 0; i < MAXP; i++) if (NP[i].active) order[n++] = i;
    for (int a = 1; a < n; a++)                           /* tri par frags */
        for (int b = a; b > 0 && NP[order[b]].kills > NP[order[b - 1]].kills; b--) {
            int t = order[b]; order[b] = order[b - 1]; order[b - 1] = t;
        }
    float w = 600, x = cx - w * 0.5f, y = cy - 170;
    rect(x - 24, y - 56, w + 48, 90 + n * 38.0f, 0.03f, 0.04f, 0.08f, 0.88f);
    text(1, x, y - 14, "JOUEUR", 0.6f, 0.7f, 0.85f, 1);
    text(1, x + 360, y - 14, "FRAGS", 0.6f, 0.7f, 0.85f, 1);
    text(1, x + 480, y - 14, "MORTS", 0.6f, 0.7f, 0.85f, 1);
    for (int k = 0; k < n; k++) {
        int i = order[k];
        float yy = y + 30 + k * 38.0f;
        if (i == myId) rect(x - 10, yy - 26, w + 20, 34, 1, 1, 1, 0.08f);
        text(1, x, yy, NP[i].name, pcol[i][0], pcol[i][1], pcol[i][2], NP[i].alive ? 1.0f : 0.5f);
        snprintf(buf, sizeof buf, "%d", NP[i].kills);
        text(1, x + 360, yy, buf, 1, 1, 1, 1);
        snprintf(buf, sizeof buf, "%d", NP[i].deaths);
        text(1, x + 480, yy, buf, 0.8f, 0.8f, 0.85f, 1);
    }
}

static void draw_killfeed(void) {
    char buf[96];
    for (int k = 0; k < 6; k++) {
        KillFeed *f = &kfeed[k];
        if (f->t <= 0) continue;
        if (f->info) snprintf(buf, sizeof buf, "%s %s", pname(f->victim), f->info == 1 ? "a rejoint" : "a quitte");
        else if (f->killer == f->victim) snprintf(buf, sizeof buf, "%s s'est fait sauter", pname(f->victim));
        else snprintf(buf, sizeof buf, "%s  [%s]  %s", pname(f->killer), weaponNames[f->kind % 7], pname(f->victim));
        int mine = f->killer == myId || f->victim == myId;
        text(0, winW - 30.0f - fontW[0] * (float)strlen(buf), 64 + k * 26.0f, buf, 1, mine ? 0.85f : 1, mine ? 0.4f : 1,
             clampf(f->t, 0, 1));
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
        if (!np_target(i)) continue;
        V3 head = vadd(NP[i].disp, v3(0, np_height(i) + 0.45f, 0));
        float sx, sy;
        if (!los(camEye, head) || !project(head, &sx, &sy)) continue;
        text_c(0, sx, sy, NP[i].name, pcol[i][0], pcol[i][1], pcol[i][2], 0.95f);
    }
}

static void draw_remote_players(void) {
    glEnable(GL_LIGHTING);
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i)) continue;
        NetPlayer *n = &NP[i];
        float h = np_height(i);
        const float *c = pcol[i];
        glPushMatrix();
        glTranslatef(n->disp.x, n->disp.y, n->disp.z);
        glRotatef(-n->st.yaw * 180 / PI, 0, 1, 0);
        if (n->st.flags & NF_SLIDE) glRotatef(-25, 1, 0, 0);
        if (n->flashT > 0) glColor3f(1, 1, 1); else glColor3f(c[0] * 0.75f, c[1] * 0.75f, c[2] * 0.75f);
        box_faces(v3(-0.36f, 0, -0.24f), v3(0.36f, h - 0.48f, 0.24f));
        if (n->flashT > 0) glColor3f(1, 1, 1); else glColor3f(c[0], c[1], c[2]);
        box_faces(v3(-0.23f, h - 0.46f, -0.23f), v3(0.23f, h, 0.23f));
        glDisable(GL_LIGHTING);
        glColor3f(1, 1, 1);                               /* visiere : montre ou il regarde */
        box_faces(v3(-0.19f, h - 0.3f, -0.26f), v3(0.19f, h - 0.18f, -0.22f));
        glEnable(GL_LIGHTING);
        glPushMatrix();
        glTranslatef(0.42f, h - 0.6f, -0.1f);
        glRotatef(n->st.pitch * 180 / PI, 1, 0, 0);
        if (n->st.weapon == 0) glColor3f(0.3f, 0.8f, 1.0f); else glColor3f(0.9f, 0.5f, 0.15f);
        box_faces(v3(-0.06f, -0.06f, -0.7f), v3(0.06f, 0.06f, 0.05f));
        glPopMatrix();
        if (n->st.melee) {                                /* epee en plein balayage */
            float k = 1 - n->st.melee / 255.0f;
            glPushMatrix();
            glTranslatef(-0.4f, h - 0.6f, -0.1f);
            glRotatef(-80 + k * 160, 0, 1, 0);
            glColor3f(0.85f, 0.88f, 0.95f);
            box_faces(v3(-0.04f, -0.01f, -1.3f), v3(0.04f, 0.01f, 0));
            glPopMatrix();
        }
        glPopMatrix();
    }
    glDisable(GL_LIGHTING);
    glLineWidth(3);                                       /* cables des grappins adverses */
    glBegin(GL_LINES);
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i) || !(NP[i].st.flags & NF_HOOK)) continue;
        glColor3f(0.95f, 0.8f, 0.45f);
        glVertex3f(NP[i].disp.x, NP[i].disp.y + np_height(i) - 0.5f, NP[i].disp.z);
        glVertex3f(NP[i].st.hookPos[0], NP[i].st.hookPos[1], NP[i].st.hookPos[2]);
    }
    glEnd();
}

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
    snprintf(buf, sizeof buf, "%d:%02d   frags %d", left / 60, left % 60, FRAG_LIMIT);
    text(0, 30, 40, buf, 0.85f, 0.85f, 0.95f, 1);
    for (int k = 0; k < n; k++) {
        int i = order[k];
        snprintf(buf, sizeof buf, "%d  %s", NP[i].kills, NP[i].name);
        text(1, 30, 78 + k * 30.0f, buf, pcol[i][0], pcol[i][1], pcol[i][2], NP[i].alive ? 1.0f : 0.5f);
    }
    draw_killfeed();
    if (matchOver) {
        snprintf(buf, sizeof buf, "%s REMPORTE LE MATCH", pname(matchWinner));
        text_c(2, cx, cy - 250, buf, 1, 0.85f, 0.3f, 1);
        draw_scoreboard(cx, cy);
    }
}

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
        else if (appState == APP_MENU) draw_main_menu(cx, cy);
        else draw_join_menu(cx, cy);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        return;
    }
    if (dmMode) draw_name_tags();
    if (spectating) {
        draw_spec_hud(cx, cy);
        if (!captured && !shotMode && !botMode) {
            if (settingsOpen) draw_settings(cx, cy); else draw_pause(cx, cy);
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
    for (int i = 0; i < MAXDN; i++) {                     /* chiffres de degats */
        DmgNum *d = &DN[i];
        float sx, sy;
        if (d->t <= 0 || !project(vadd(d->pos, v3(0, (0.9f - d->t) * 1.4f, 0)), &sx, &sy)) continue;
        char nb[16];
        snprintf(nb, sizeof nb, "%.0f", d->val);
        float a = clampf(d->t * 2, 0, 1);
        if (d->val >= 60) text_c(1, sx, sy, nb, 1, 0.45f, 0.2f, a);
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

    /* vitesse */
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
    text(0, mx, by - 78.0f, aimed ? "EPEE (vs cible)" : "EPEE", 0.75f, 0.75f, 0.85f, 0.9f);
    float pvp = dmMode ? PVP_SCALE : 1.0f;
    snprintf(buf, sizeof buf, "%d dmg %.1fm", (int)(melee_dmg(sp3) * pvp), melee_range(sp3));
    text(1, mx, by - 48.0f, buf, 1, 1 - mk * 0.45f, 1 - mk * 0.85f, 1);
    rect(mx, by - 36.0f, 180, 6, 0.1f, 0.1f, 0.15f, 0.7f);
    rect(mx, by - 36.0f, 180 * clampf(1 - P.meleeCd / MELEE_CD, 0, 1), 6, 1, 0.55f + 0.45f * (1 - mk), 0.3f, 0.95f);

    /* axe tir : chaine de touches successives */
    float sx = compact ? winW - 214.0f : cx + 130;
    text(0, sx, by - 78.0f, "CHAINE DE TIR", 0.75f, 0.75f, 0.85f, 0.9f);
    float sk = clampf(shotStreak / (float)STREAK_MAX, 0, 1);
    snprintf(buf, sizeof buf, "x%.2f", streak_mult());
    text(1, sx, by - 48.0f, buf, 1, 1 - sk * 0.2f, 1 - sk * 0.8f, 1);
    for (int i = 0; i < STREAK_MAX; i++)
        rect(sx + i * 23.0f, by - 36.0f, 19, 6, i < shotStreak ? 1.0f : 0.1f, i < shotStreak ? 0.8f : 0.1f,
             i < shotStreak ? 0.3f : 0.15f, i < shotStreak ? 0.95f : 0.7f);
    if (streakLostT > 0) text(0, sx, by - 100.0f, "CHAINE PERDUE", 1, 0.3f, 0.3f, clampf(streakLostT, 0, 1));

    if (P.hook == HOOK_ATTACHED)
        text_c(0, cx, winH - 110.0f, (P.hookEnemy || P.hookPlayer) ? "GRAPPIN -> CIBLE" : "GRAPPIN", 1,
               (P.hookEnemy || P.hookPlayer) ? 0.35f : 0.8f, 0.35f, 0.9f);
    else if (P.wallrun) text_c(0, cx, winH - 110.0f, "WALL-RUN", 1, 0.6f, 0.9f, 0.9f);
    else if (P.sliding) text_c(0, cx, winH - 110.0f, "SLIDE", 1, 0.8f, 0.4f, 0.9f);

    /* vie */
    float hpk = P.hp / 100.0f;
    rect(30, winH - 50.0f, 300, 20, 0.1f, 0.1f, 0.15f, 0.7f);
    rect(30, winH - 50.0f, 300 * hpk, 20, 1 - hpk * 0.7f, 0.25f + hpk * 0.6f, 0.35f, 0.95f);
    snprintf(buf, sizeof buf, "%d", (int)ceilf(P.hp));
    text(1, 30, winH - 60.0f, buf, 1, 1, 1, 1);

    /* arme */
    char wnBuf[32];
    if (P.weapon == 0) snprintf(wnBuf, sizeof wnBuf, "RAILGUN %d/%d", P.railAmmo, RAIL_AMMO_MAX);
    else snprintf(wnBuf, sizeof wnBuf, "ROQUETTES  (rail %d)", P.railAmmo);
    const char *wn = wnBuf;
    float cdMax = P.weapon == 0 ? RAIL_CD : ROCKET_CD;
    text(1, winW - 30.0f - fontW[1] * (float)strlen(wn), winH - 60.0f, wn, P.weapon == 0 ? 0.4f : 1.0f,
         P.weapon == 0 ? 0.85f : 0.6f, P.weapon == 0 ? 1.0f : 0.2f, 1);
    rect(winW - 230.0f, winH - 40.0f, 200, 8, 0.1f, 0.1f, 0.15f, 0.7f);
    rect(winW - 230.0f, winH - 40.0f, 200 * clampf(1 - P.fireCd / cdMax, 0, 1), 8, 1, 1, 1, 0.8f);
    for (int i = 0; i < COIN_MAX; i++) {                 /* pieces disponibles */
        float fill = clampf(P.coins - i, 0, 1), x = winW - 30.0f - (COIN_MAX - i) * 20.0f, y = winH - 96.0f;
        rect(x, y, 14, 14, 0.15f, 0.12f, 0.05f, 0.7f);
        rect(x, y + 14 * (1 - fill), 14, 14 * fill, 1, 0.82f, 0.3f, fill >= 1 ? 1.0f : 0.5f);
    }
    {
        char kb[40];
        snprintf(kb, sizeof kb, "[%s]", bind_text(A_COIN));
        text(0, winW - 36.0f - COIN_MAX * 20.0f - fontW[0] * (float)strlen(kb), winH - 83.0f, kb, 0.8f, 0.7f, 0.4f, 0.9f);
    }
    {                                                     /* le viseur s'allume sur une piece : doree = la mienne, rouge = adverse */
        V3 eye = eye_pos(), vd = view_dir();
        float tw = ray_world(eye, vd, 400, 0), t;
        for (int i = 0; i < MAXCOIN; i++)
            if (!P.dead && CO[i].alive && ray_sphere(eye, vd, CO[i].pos, COIN_RADIUS, tw, &t)) {
                glLineWidth(2);
                if (CO[i].owner == myId) glColor4f(1, 0.85f, 0.3f, 1); else glColor4f(1, 0.25f, 0.25f, 1);
                glBegin(GL_LINE_LOOP);
                glVertex2f(cx, cy - 11); glVertex2f(cx + 11, cy); glVertex2f(cx, cy + 11); glVertex2f(cx - 11, cy);
                glEnd();
                break;
            }
    }
    char hintBuf[96];
    snprintf(hintBuf, sizeof hintBuf, "[%s] rail  [%s] roquettes", bind_text(A_WEAP1), bind_text(A_WEAP2));
    const char *hint = hintBuf;
    text(0, winW - 30.0f - fontW[0] * (float)strlen(hint), winH - 14.0f, hint, 0.6f, 0.6f, 0.7f, 0.8f);

    /* infos de partie */
    if (dmMode) {
        int count = 0, rank = 1;
        for (int i = 0; i < MAXP; i++) {
            if (!NP[i].active) continue;
            count++;
            if (i != myId && NP[i].kills > NP[myId].kills) rank++;
        }
        int left = (int)fmaxf(0, MATCH_TIME - matchT);
        snprintf(buf, sizeof buf, "DEATHMATCH LAN   %d joueur%s   %d:%02d", count, count > 1 ? "s" : "", left / 60, left % 60);
        text(0, 30, 40, buf, 0.85f, 0.85f, 0.95f, 1);
        snprintf(buf, sizeof buf, "FRAGS %d / %d", NP[myId].kills, FRAG_LIMIT);
        text(1, 30, 78, buf, 1, 1, 1, 1);
        snprintf(buf, sizeof buf, "rang %d / %d   morts %d   [%s] scores", rank, count, NP[myId].deaths, bind_text(A_SCORES));
        text(0, 30, 104, buf, 0.7f, 0.7f, 0.8f, 0.9f);
        draw_killfeed();
    } else {
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
    if (msgT > 0) text_c(2, cx, cy - 120, msg, 1, 1, 1, clampf(msgT, 0, 1));

    if (dmMode && matchOver) {
        snprintf(buf, sizeof buf, "%s REMPORTE LE MATCH", pname(matchWinner));
        text_c(2, cx, cy - 250, buf, 1, 0.85f, 0.3f, 1);
        draw_scoreboard(cx, cy);
        snprintf(buf, sizeof buf, "nouveau match dans %d s", (int)ceilf(fmaxf(matchEndT, 0)));
        text_c(1, cx, winH - 150.0f, buf, 0.9f, 0.9f, 0.9f, 1);
    } else if (dmMode && P.dead) {
        rect(0, 0, (float)winW, (float)winH, 0.15f, 0, 0, 0.4f);
        if (killerOfMe >= 0 && killerOfMe != myId) snprintf(buf, sizeof buf, "ELIMINE PAR %s", pname(killerOfMe));
        else snprintf(buf, sizeof buf, killerOfMe == myId ? "TU T'ES FAIT SAUTER" : "EN ATTENTE DU SPAWN");
        text_c(2, cx, cy - 40, buf, 1, 0.35f, 0.3f, 1);
        snprintf(buf, sizeof buf, "respawn dans %.1f s", fmaxf(0, RESPAWN_TIME - P.deathT));
        if (killerOfMe >= 0) text_c(1, cx, cy + 10, buf, 0.9f, 0.9f, 0.9f, 1);
    } else if (dmMode && bound_held(A_SCORES)) {
        draw_scoreboard(cx, cy);
    } else if (P.dead) {
        rect(0, 0, (float)winW, (float)winH, 0.15f, 0, 0, 0.45f);
        text_c(2, cx, cy - 40, "T'ES MORT", 1, 0.3f, 0.3f, 1);
        snprintf(buf, sizeof buf, "score %d  -  vague %d  -  %d kills", score, wave, kills);
        text_c(1, cx, cy + 10, buf, 1, 1, 1, 1);
        snprintf(buf, sizeof buf, "record %d", bestScore);
        text_c(0, cx, cy + 44, buf, 0.8f, 0.8f, 0.9f, 1);
        if (P.deathT > 1.0f) text_c(1, cx, cy + 100, "[R] ou clic pour recommencer", 0.9f, 0.9f, 0.9f, 1);
    }

    if (!captured && !shotMode && !botMode) {
        if (settingsOpen) draw_settings(cx, cy); else draw_pause(cx, cy);
    }
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

static void render(float alpha, float frameDt) {
    glViewport(0, 0, winW, winH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    float aspect = (float)winW / (float)(winH > 0 ? winH : 1);

    float hs = hlen(P.vel);
    float hfov = fovBase + clampf((hs - RUN_SPEED) * 0.7f, 0, 20) + P.fovKick;
    P.fovCur += (hfov - P.fovCur) * fminf(1, frameDt * 6);
    float hf = fminf(P.fovCur, 150.0f);
    float vfov = 2 * atanf(tanf(hf * PI / 360.0f) / aspect) * 180.0f / PI;

    draw_sky(vfov);

    V3 pos = vlerp(P.prevPos, P.pos, alpha);
    V3 eye = v3(pos.x, pos.y + P.eyeH + P.bob + P.stepOfs - P.landDip, pos.z);
    float sy = frange(-1, 1) * shake * 2, sp = frange(-1, 1) * shake * 2;

    set_frustum(vfov, aspect, 0.05f, 500);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRotatef(P.roll, 0, 0, 1);
    glRotatef(-P.pitch * 180 / PI + sp, 1, 0, 0);
    glRotatef(P.yaw * 180 / PI + sy, 0, 1, 0);
    glTranslatef(-eye.x, -eye.y, -eye.z);

    V3 fwd = view_dir();
    camR = v3(cosf(P.yaw), 0, sinf(P.yaw));
    camU = vcross(camR, fwd);
    camEye = eye;
    glGetFloatv(GL_MODELVIEW_MATRIX, mvMat);
    glGetFloatv(GL_PROJECTION_MATRIX, prMat);

    GLfloat fogc[4] = { 0.34f, 0.30f, 0.44f, 1 };
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, fogc);
    glFogf(GL_FOG_START, 50);
    glFogf(GL_FOG_END, 240);

    GLfloat lp[4] = { 0.35f, 1.0f, 0.22f, 0 }, amb[4] = { 0.42f, 0.42f, 0.5f, 1 }, dif[4] = { 0.7f, 0.66f, 0.6f, 1 };
    glLightfv(GL_LIGHT0, GL_POSITION, lp);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    {                                                     /* etoiles et soleil, accroches a la camera */
        glDisable(GL_FOG); glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        unsigned int seed = 99;
        glPointSize(2);
        glBegin(GL_POINTS);
        for (int i = 0; i < 160; i++) {
            float a, b, c;
            seed = seed * 1664525u + 1013904223u; a = (seed >> 8) / 16777216.0f * 2 - 1;
            seed = seed * 1664525u + 1013904223u; b = (seed >> 8) / 16777216.0f;
            seed = seed * 1664525u + 1013904223u; c = (seed >> 8) / 16777216.0f * 2 - 1;
            V3 d = vnorm(v3(a, b * 0.8f + 0.12f, c));
            float tw = 0.5f + 0.5f * sinf(gameTime * 2 + i);
            glColor4f(0.8f, 0.85f, 1.0f, 0.35f + 0.4f * tw * (d.y));
            glVertex3f(eye.x + d.x * 300, eye.y + d.y * 300, eye.z + d.z * 300);
        }
        glEnd();
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texDot);
        V3 sun = vadd(eye, vmul(vnorm(v3(0.55f, 0.16f, -0.82f)), 300));
        glBegin(GL_QUADS);
        glColor4f(1.0f, 0.55f, 0.35f, 0.35f); billboard(sun, 120);
        glColor4f(1.0f, 0.75f, 0.5f, 0.8f); billboard(sun, 30);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST); glEnable(GL_FOG);
    }
    glCallList(worldList);

    /* ombres sous les ennemis */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texDot);
    glBegin(GL_QUADS);
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive) continue;
        float h = ray_world(E[i].pos, v3(0, -1, 0), 40, 0);
        float a = clampf(0.55f - h / 40, 0, 0.55f), s = 1.1f, gy = E[i].pos.y - h + 0.03f;
        glColor4f(0, 0, 0, a);
        glTexCoord2f(0, 0); glVertex3f(E[i].pos.x - s, gy, E[i].pos.z - s);
        glTexCoord2f(1, 0); glVertex3f(E[i].pos.x + s, gy, E[i].pos.z - s);
        glTexCoord2f(1, 1); glVertex3f(E[i].pos.x + s, gy, E[i].pos.z + s);
        glTexCoord2f(0, 1); glVertex3f(E[i].pos.x - s, gy, E[i].pos.z + s);
    }
    glEnd();
    glDepthMask(GL_TRUE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    for (int i = 0; i < MAXE; i++) if (E[i].alive) draw_enemy(&E[i]);
    if (dmMode) draw_remote_players();
    glEnable(GL_LIGHTING);
    for (int i = 0; i < MAXSH; i++) {                    /* eclats qui rebondissent */
        Shard *h = &SH[i];
        if (h->life <= 0) continue;
        float sz = h->size * fminf(1, h->life * 2);
        glPushMatrix();
        glTranslatef(h->pos.x, h->pos.y, h->pos.z);
        glRotatef(h->ang, h->axis.x, h->axis.y, h->axis.z);
        glColor3f(h->col.x, h->col.y, h->col.z);
        box_faces(v3(-sz, -sz * 0.6f, -sz), v3(sz, sz * 0.6f, sz));
        glPopMatrix();
    }
    glDisable(GL_LIGHTING);
    for (int i = 0; i < MAXCOIN; i++) {                  /* pieces : disques dores qui tournent */
        if (!CO[i].alive) continue;
        glPushMatrix();
        glTranslatef(CO[i].pos.x, CO[i].pos.y, CO[i].pos.z);
        glRotatef(CO[i].spin * 900, 0, 1, 0);
        glRotatef(CO[i].spin * 400, 1, 0, 0);
        glColor3f(1.0f, 0.82f, 0.3f);
        box_faces(v3(-0.16f, -0.16f, -0.025f), v3(0.16f, 0.16f, 0.025f));
        glPopMatrix();
    }
    for (int i = 0; i < npu; i++) {                      /* cellules de recharge du railgun */
        if (PU[i].t > 0) continue;
        glPushMatrix();
        glTranslatef(PU[i].pos.x, PU[i].pos.y + 0.15f * sinf(gameTime * 3 + i), PU[i].pos.z);
        glRotatef(gameTime * 90 + i * 40, 0, 1, 0);
        glColor3f(0.75f, 0.95f, 1.0f);
        box_faces(v3(-0.22f, -0.35f, -0.22f), v3(0.22f, 0.35f, 0.22f));
        glColor3f(0.2f, 0.6f, 1.0f);
        box_edges(v3(-0.26f, -0.4f, -0.26f), v3(0.26f, 0.4f, 0.26f));
        glPopMatrix();
    }

    if (P.hook != HOOK_NONE) {                            /* cable du grappin, part de la main gauche */
        V3 hand = vadd(eye, vadd(vmul(camR, -0.3f), vadd(vmul(camU, -0.28f), vmul(fwd, 0.6f))));
        glLineWidth(3);
        glColor3f(0.95f, 0.8f, 0.45f);
        glBegin(GL_LINES);
        glVertex3f(hand.x, hand.y, hand.z);
        glVertex3f(P.hookPos.x, P.hookPos.y, P.hookPos.z);
        glEnd();
    }

    /* tout ce qui brille : additif */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glLineWidth(1);
    for (int i = 0; i < MAXBM; i++) {
        Beam *m = &BM[i];
        if (m->life <= 0) continue;
        float a = m->life / m->max;
        glLineWidth(7); glColor4f(m->col.x, m->col.y, m->col.z, a * 0.35f);
        glBegin(GL_LINES); glVertex3f(m->a.x, m->a.y, m->a.z); glVertex3f(m->b.x, m->b.y, m->b.z); glEnd();
        glLineWidth(2); glColor4f(1, 1, 1, a);
        glBegin(GL_LINES); glVertex3f(m->a.x, m->a.y, m->a.z); glVertex3f(m->b.x, m->b.y, m->b.z); glEnd();
    }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texDot);
    glBegin(GL_QUADS);
    for (int i = 0; i < MAXPA; i++) {
        Part *q = &PA[i];
        if (q->life <= 0) continue;
        float a = q->life / q->max;
        glColor4f(q->col.x, q->col.y, q->col.z, a);
        billboard(q->pos, q->size * (0.5f + 0.5f * a));
    }
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive) continue;
        V3 c = enemy_color(E[i].type);
        glColor4f(c.x, c.y, c.z, 0.35f);
        billboard(E[i].pos, 2.2f);
    }
    for (int i = 0; i < MAXPR; i++) {
        if (!PR[i].alive) continue;
        if (PR[i].kind == PR_ROCKET) { glColor4f(1, 0.6f, 0.2f, 1); billboard(PR[i].pos, 0.55f); glColor4f(1, 1, 0.8f, 1); billboard(PR[i].pos, 0.2f); }
        else if (PR[i].kind == PR_PARRIED) { glColor4f(1, 0.85f, 0.4f, 1); billboard(PR[i].pos, 1.0f); glColor4f(1, 1, 1, 1); billboard(PR[i].pos, 0.3f); }
        else { glColor4f(1, 0.25f, 0.9f, 1); billboard(PR[i].pos, 0.7f); glColor4f(1, 0.9f, 1, 1); billboard(PR[i].pos, 0.22f); }
    }
    for (int i = 0; i < MAXCOIN; i++) {                  /* reflet des pieces */
        if (!CO[i].alive) continue;
        glColor4f(1, 0.85f, 0.4f, 0.5f + 0.4f * sinf(CO[i].spin * 25));
        billboard(CO[i].pos, 0.7f);
    }
    if (P.hook != HOOK_NONE) {
        glColor4f(1, 0.8f, 0.35f, 1); billboard(P.hookPos, 0.4f);
        glColor4f(1, 1, 0.9f, 1); billboard(P.hookPos, 0.15f);
    }
    for (int i = 0; i < npu; i++) {
        if (PU[i].t > 0) continue;
        glColor4f(0.3f, 0.8f, 1.0f, 0.55f + 0.2f * sinf(gameTime * 5 + i));
        billboard(PU[i].pos, 1.3f);
    }
    float pulse = 0.5f + 0.5f * sinf(gameTime * 4);
    for (int i = 0; i < nb; i++) {
        if (boxes[i].kind != B_PAD) continue;
        V3 c = vlerp(boxes[i].mn, boxes[i].mx, 0.5f);
        float s = 1.4f + pulse * 0.4f, y = boxes[i].mx.y + 0.02f;
        glColor4f(0.2f, 0.8f, 1.0f, 0.5f + 0.3f * pulse);
        glTexCoord2f(0, 0); glVertex3f(c.x - s, y, c.z - s);
        glTexCoord2f(1, 0); glVertex3f(c.x + s, y, c.z - s);
        glTexCoord2f(1, 1); glVertex3f(c.x + s, y, c.z + s);
        glTexCoord2f(0, 1); glVertex3f(c.x - s, y, c.z + s);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_FOG);

    if (appState == APP_GAME && !spectating) draw_viewmodel(aspect);
    if (impactT > 0 && appState == APP_GAME) {           /* IMPACT FRAME : couleurs inversees + traits radiaux */
        float sx = winW * 0.5f, sy = winH * 0.5f;
        project(impactAt, &sx, &sy);
        glMatrixMode(GL_PROJECTION); glLoadIdentity();
        glOrtho(0, winW, winH, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glDisable(GL_DEPTH_TEST); glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);
        glColor4f(1, 1, 1, 1);
        rect(0, 0, (float)winW, (float)winH, 1, 1, 1, 1);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        float big = (float)(winW + winH);
        glBegin(GL_TRIANGLES);
        for (int i = 0; i < 46; i++) {
            float ang = frange(0, 2 * PI), wid = frange(0.004f, 0.02f), r0 = frange(40, 160);
            glColor4f(0, 0, 0, 0.85f);
            glVertex2f(sx + cosf(ang) * r0, sy + sinf(ang) * r0);
            glVertex2f(sx + cosf(ang - wid) * big, sy + sinf(ang - wid) * big);
            glVertex2f(sx + cosf(ang + wid) * big, sy + sinf(ang + wid) * big);
        }
        glEnd();
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        impactT -= frameDt;
    }
    draw_hud();
}

/* ------------------------------------------------------------------ */
/* Fenetre                                                             */
/* ------------------------------------------------------------------ */
static WINDOWPLACEMENT prevPlacement = { sizeof(WINDOWPLACEMENT) };

static void toggle_fullscreen(void) {
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
    fullscreen = !fullscreen;
    if (captured) { set_capture(0); set_capture(1); }
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_SIZE:
        winW = LOWORD(lp); winH = HIWORD(lp);
        if (captured) { set_capture(0); set_capture(1); }
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
    case WM_KEYDOWN: case WM_SYSKEYDOWN: {
        int sc = (lp >> 16) & 0xFF;
        if (waitAct >= 0) {                               /* on attend une touche pour une commande */
            if (!(lp & (1 << 30))) {
                if (sc == SC_ESC) waitAct = -1;
                else if (sc == 0x0E || sc == 0x53) assign_bind(0);   /* retour arriere / suppr : effacer */
                else assign_bind(sc);
            }
            return 0;
        }
        if (!(lp & (1 << 30))) {
            latch[sc] = 1;
            if (sc == SC_ESC) {
                if (uiFocus) uiFocus = 0;
                else if (settingsOpen) { settingsOpen = 0; cfg_save(); }
                else if (appState == APP_JOIN) pendingAction = ACT_BACK;
                else set_capture(0);
            }
            if (sc == SC_F11) toggle_fullscreen();
            if (sc == SC_F2) sens = fmaxf(0.0003f, sens - 0.0002f);
            if (sc == SC_F3) sens = fminf(0.01f, sens + 0.0002f);
            if (sc == SC_R && !captured && !uiFocus && appState == APP_GAME && !dmMode) reset_game();
        }
        keys[sc] = 1;
        if (m == WM_SYSKEYDOWN && wp == VK_F4) break;      /* Alt+F4 */
        return 0;
    }
    case WM_KEYUP: case WM_SYSKEYUP:
        keys[(lp >> 16) & 0xFF] = 0;
        return 0;
    case WM_CHAR:                                         /* saisie du pseudo et de l'IP */
        if (uiFocus) {
            char *f = uiFocus == 1 ? myName : ipField;
            int cap = uiFocus == 1 ? 15 : 31, len = (int)strlen(f);
            if (wp == 8) { if (len) f[len - 1] = 0; }
            else if (wp == 13) { if (uiFocus == 2) pendingAction = ACT_JOIN_IP; uiFocus = 0; }
            else if (wp >= 32 && wp < 127 && len < cap && (uiFocus == 1 || (wp >= '0' && wp <= '9') || wp == '.')) {
                f[len] = (char)wp; f[len + 1] = 0;
            }
        }
        return 0;
    case WM_MOUSEMOVE:
        uiMX = (short)LOWORD(lp); uiMY = (short)HIWORD(lp);
        return 0;
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: case WM_XBUTTONDOWN: {
        int code = m == WM_LBUTTONDOWN ? K_LMB : m == WM_RBUTTONDOWN ? K_RMB : m == WM_MBUTTONDOWN ? K_MMB :
                   (HIWORD(wp) == XBUTTON1 ? K_X1 : K_X2);
        if (waitAct >= 0) { assign_bind(code); return m == WM_XBUTTONDOWN; }
        if (!captured) {                                  /* les menus traitent le clic gauche */
            if (code == K_LMB) { uiClick = 1; uiDown = 1; }
            return m == WM_XBUTTONDOWN;
        }
        keys[code] = 1; latch[code] = 1;
        return m == WM_XBUTTONDOWN;
    }
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: case WM_XBUTTONUP: {
        int code = m == WM_LBUTTONUP ? K_LMB : m == WM_RBUTTONUP ? K_RMB : m == WM_MBUTTONUP ? K_MMB :
                   (HIWORD(wp) == XBUTTON1 ? K_X1 : K_X2);
        keys[code] = 0;
        if (code == K_LMB) { uiDown = 0; uiDrag = 0; }
        return m == WM_XBUTTONUP;
    }
    case WM_MOUSEWHEEL:
        if (waitAct >= 0) assign_bind(K_WHEEL);
        else if (captured) latch[K_WHEEL] = 1;
        return 0;
    case WM_ACTIVATE: if (LOWORD(wp) == WA_INACTIVE) set_capture(0); return 0;
    case WM_KILLFOCUS: set_capture(0); return 0;
    case WM_CLOSE: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(h, m, wp, lp);
}

static void save_bmp(const char *path) {
    int rowSize = (winW * 3 + 3) & ~3;
    unsigned char *px = (unsigned char *)malloc(rowSize * winH);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, winW, winH, 0x80E0 /* GL_BGR_EXT */, GL_UNSIGNED_BYTE, px);
    FILE *f = fopen(path, "wb");
    if (f) {
        BITMAPFILEHEADER fh = { 0 }; BITMAPINFOHEADER ih = { 0 };
        fh.bfType = 0x4D42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + rowSize * winH;
        ih.biSize = sizeof ih; ih.biWidth = winW; ih.biHeight = winH; ih.biPlanes = 1; ih.biBitCount = 24;
        fwrite(&fh, sizeof fh, 1, f); fwrite(&ih, sizeof ih, 1, f); fwrite(px, rowSize * winH, 1, f);
        fclose(f);
    }
    free(px);
}

/* Tests du mouvement sans fenetre : velocity.exe --test resultat.txt */
static void run_tests(const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    build_level();

    reset_game();                                         /* escalier -> plateforme centrale */
    P.pos = P.prevPos = v3(0, 0, ms(14) + 0.6f); P.yaw = 0;
    act[A_FWD] = 1;
    for (int i = 0; i < 240; i++) player_tick(TICK, 0, 0);
    act[A_FWD] = 0;
    fprintf(f, "escalier : y=%.2f z=%.2f (attendu y=4)\n", P.pos.y, P.pos.z);

    reset_game();                                         /* jump pad -> plateforme haute */
    P.pos = P.prevPos = v3(ms(16), 0.31f, 0);
    float maxY = 0;
    for (int i = 0; i < 360; i++) { player_tick(TICK, 0, 0); maxY = fmaxf(maxY, P.pos.y); }
    fprintf(f, "jump pad est : apogee=%.2f final x=%.2f y=%.2f grounded=%d (attendu y=7, x entre 39 et 62)\n",
            maxY, P.pos.x, P.pos.y, P.grounded);
    reset_game();
    P.pos = P.prevPos = v3(ms(9), 0.31f, ms(16));
    for (int i = 0; i < 360; i++) player_tick(TICK, 0, 0);
    fprintf(f, "jump pad nord : final x=%.2f y=%.2f z=%.2f (attendu y=7, z entre 39 et 62)\n", P.pos.x, P.pos.y, P.pos.z);

    reset_game();                                         /* bhop + air-strafe */
    P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = PI * 0.5f;
    act[A_FWD] = 1;
    for (int i = 0; i < 60; i++) player_tick(TICK, 0, 0);
    act[A_FWD] = 0;
    float startSpeed = hlen(P.vel), maxSpeed = 0;
    act[A_JUMP] = 1;
    for (int i = 0; i < 300; i++) {
        int side = (i / 50) % 2 ? 1 : -1;                 /* alterne gauche/droite */
        act[A_RIGHT] = side > 0; act[A_LEFT] = side < 0;
        if (!P.grounded) P.yaw += side * 2.2f * TICK;
        player_tick(TICK, 0, 0);
        maxSpeed = fmaxf(maxSpeed, hlen(P.vel));
    }
    memset(act, 0, sizeof act);
    fprintf(f, "bhop : vitesse depart=%.1f max=%.1f (attendu max > %.1f)\n", startSpeed, maxSpeed, RUN_SPEED + 3);

    reset_game();                                         /* rocket jump */
    P.pos = P.prevPos = v3(-20, 0, 40); P.pitch = -1.5f;
    fire_rocket();
    player_tick(TICK, 1, 0);
    maxY = 0;
    for (int i = 0; i < 200; i++) { proj_tick(TICK); player_tick(TICK, 0, 0); maxY = fmaxf(maxY, P.pos.y); }
    fprintf(f, "rocket jump : apogee=%.2f hp=%.0f (attendu apogee > 3)\n", maxY, P.hp);

    reset_game();                                         /* grappin vers le mur nord */
    P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = PI; P.pitch = 0.35f;
    hookPressIn = 1; hookHeldIn = 1;
    int attachedTicks = 0;
    float maxSp = 0;
    V3 anchor = v3(0, 0, 0);
    for (int i = 0; i < 150; i++) {
        player_tick(TICK, 0, 0);
        hookPressIn = 0;
        if (P.hook == HOOK_ATTACHED) { attachedTicks++; anchor = P.anchor; }
        maxSp = fmaxf(maxSp, vlen(P.vel));
    }
    fprintf(f, "grappin : ancre=(%.1f, %.1f, %.1f) accroche %d ticks, vitesse max=%.1f, pos=(%.1f, %.1f, %.1f) (attendu ancre z=78, pos proche du mur)\n",
            anchor.x, anchor.y, anchor.z, attachedTicks, maxSp, P.pos.x, P.pos.y, P.pos.z);

    reset_game();                                         /* lacher en plein vol : l'elan est conserve */
    P.pos = P.prevPos = v3(-20, 0, 30); P.yaw = PI; P.pitch = 0.35f;
    hookPressIn = 1; hookHeldIn = 1;
    for (int i = 0; i < 60; i++) { player_tick(TICK, 0, 0); hookPressIn = 0; }
    float spBefore = vlen(P.vel);
    int stillAttached = P.hook == HOOK_ATTACHED;
    player_tick(TICK, 1, 0);                              /* saut = lacher + boost */
    float spAfter = vlen(P.vel), vyAfter = P.vel.y;
    hookHeldIn = 0;
    fprintf(f, "lacher : accroche=%d vitesse avant=%.1f apres=%.1f vy=%.1f hook=%d (attendu vitesse conservee, hook=0)\n",
            stillAttached, spBefore, spAfter, vyAfter, P.hook);

    /* grappin sur un drone : traction, impact, rebond */
    reset_game();
    P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0.16f;
    E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 4, 25);
    hookPressIn = 1; hookHeldIn = 1;
    int wasHooked = 0;
    float impactSp = 0;
    for (int i = 0; i < 120 && E[0].alive && E[0].hp == 100; i++) {
        player_tick(TICK, 0, 0);
        hookPressIn = 0;
        if (P.hookEnemy == 1) { wasHooked = 1; impactSp = vlen(P.vel); }
    }
    hookHeldIn = 0;
    fprintf(f, "grappin ennemi : accroche=%d vitesse a l'impact=%.1f drone vivant=%d hp=%.0f vy apres=%.1f hookCd=%.2f (attendu impact, drone mort, rebond)\n",
            wasHooked, impactSp, E[0].alive, E[0].hp, P.vel.y, P.hookCd);

    /* corps a corps : a l'arret puis lance a 20 m/s, avec recharge du dash */
    for (int pass = 0; pass < 2; pass++) {
        reset_game();
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        P.vel = pass ? v3(0, 0, -20) : v3(0, 0, 0);
        P.dashCharge = 0.2f;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.6f, 38);
        melee();
        fprintf(f, "corps a corps a %.0f m/s : hp drone=%.0f vivant=%d dash=%.1f (attendu %s, dash +1)\n",
                vlen(P.vel), E[0].hp, E[0].alive, P.dashCharge, pass ? "drone mort" : "hp 65");
    }

    /* parry d'une orbe : elle repart vers le drone vise et le detruit */
    reset_game();
    P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0.05f; P.hp = 50;
    E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 3, 20);
    spawn_proj(PR_ORB, v3(-20, 1.7f, 38), v3(0, 0, 16));
    melee();
    int kindAfter = PR[0].kind;
    hitstop = 0;
    for (int i = 0; i < 120; i++) proj_tick(TICK);
    fprintf(f, "parry : projectile renvoye=%d hp joueur=%.0f drone vivant=%d (attendu renvoye=1, hp 75 = 50 + 10 parry + 15 kill, drone mort)\n",
            kindAfter == PR_PARRIED, P.hp, E[0].alive);

    /* chaine de tir : 3 touches puis un rate */
    reset_game();
    P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
    float dmgSeen[3];
    for (int s = 0; s < 3; s++) {
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 30);
        fire_rail();
        dmgSeen[s] = 1000 - E[0].hp;
    }
    int before = shotStreak;
    P.yaw = PI;                                           /* on vise le mur derriere */
    fire_rail();
    fprintf(f, "chaine : degats %.0f / %.0f / %.0f, chaine=%d puis apres un rate=%d (attendu degats croissants, 3 puis 0)\n",
            dmgSeen[0], dmgSeen[1], dmgSeen[2], before, shotStreak);

    /* recharges du railgun : ramassage, plafond, reapparition */
    reset_game();
    {
        int cell = 6;                                      /* une cellule au sol, dans un coin */
        P.railAmmo = 1;
        P.pos = P.prevPos = v3(PU[cell].pos.x, 0, PU[cell].pos.z);
        pickup_tick(TICK);
        int afterOne = P.railAmmo, gone = PU[cell].t > 0;
        P.railAmmo = 9; PU[cell].t = 0;
        pickup_tick(TICK);
        int capped = P.railAmmo;
        P.pos = P.prevPos = v3(0, 0, 30);
        for (int i = 0; i < (int)(PICKUP_RESPAWN / TICK) + 2; i++) pickup_tick(TICK);
        fprintf(f, "recharges : %d cellules, munitions 1 -> %d, cellule disparue=%d, plafond 9 -> %d, revenue apres 12 s=%d (attendu 4, 1, 10, 1)\n",
                npu, afterOne, gone, capped, PU[cell].t <= 0);
    }

    /* pieces : un drone hors de la ligne de mire, on tire dans la piece, le ricochet le trouve */
    for (int pass = 0; pass < 2; pass++) {
        reset_game();
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0.1f;
        P.vel = v3(0, 0, -20);                             /* a pleine vitesse */
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-34, 3, 22);
        throw_coin();
        if (pass) { P.yaw = 0.15f; throw_coin(); P.yaw = 0; }   /* deuxieme piece : chaine */
        for (int i = 0; i < 30; i++) { coin_tick(TICK); P.pos = vadd(P.pos, vmul(P.vel, TICK)); }
        int c0 = -1;
        for (int i = 0; i < MAXCOIN; i++) if (CO[i].alive && c0 < 0) c0 = i;
        V3 d = vsub(CO[c0].pos, eye_pos());
        P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
        lastChain = 0;
        float coinsLeft = P.coins;
        fire_rail();
        fprintf(f, "piece%s : chaine %d, degats au drone %.0f (attendu %.0f), pieces restantes %.0f" "\n",
                pass ? "s x2" : "", lastChain, 1000 - E[0].hp, pass ? RAIL_DMG * 2.25f : RAIL_DMG * 1.5f, coinsLeft);
    }

    /* grappin plus nerveux : vitesse atteinte en 0,5 s vers un mur */
    reset_game();
    P.pos = P.prevPos = v3(-20, 0, 30); P.yaw = PI; P.pitch = 0.35f;
    hookPressIn = 1; hookHeldIn = 1;
    float sp05 = 0;
    for (int i = 0, since = 0; i < 240 && since < 60; i++) {   /* mesure 0,5 s apres l'accroche */
        player_tick(TICK, 0, 0); hookPressIn = 0; sp05 = vlen(P.vel);
        if (P.hook == HOOK_ATTACHED) since++;
    }
    hookHeldIn = 0;
    fprintf(f, "grappin : vitesse 0,5 s apres l'accroche = %.1f m/s (attendu > 25)\n", sp05);

    /* LAN simule (hote sans socket) : un bot contre une cible immobile */
    netMode = NET_HOST; connected = 1; myId = 0; botMode = 2; appState = APP_GAME;
    botLog = f;
    for (int pass = 0; pass < 2; pass++) {
        reset_game();
        dmMode = 1;
        memset(NP, 0, sizeof NP);
        NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Bot");
        NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible");
        if (pass == 0) {                                   /* cross-up : cible a 10 m devant */
            NP[1].disp = v3(-20, 0, 31);
            P.pos = P.prevPos = v3(-20, 0, 41);
        } else {                                           /* epee : la cible fonce sur le bot */
            NP[1].disp = v3(-20, 0, 37.5f);
            vf(NP[1].st.vel, v3(0, 0, 10));
            P.pos = P.prevPos = v3(-20, 0, 41);
            P.vel = v3(0, 0, -10);
        }
        vf(NP[1].st.pos, NP[1].disp);
        P.yaw = 0; P.dashCharge = 2; P.hp = 100; P.dead = 0;
        float maxY = 0;
        int behind = 0, hitBehind = 0, swordHits = 0;
        float hpBefore = NP[1].hp;
        P.dashCharge = pass ? 0.2f : 2;
        if (pass) P.fireCd = 10;                           /* test de l'epee seule : pas de rail */
        for (int i = 0; i < (pass ? 30 : 360); i++) {
            nowT += TICK;
            bot_think(TICK);
            game_tick(TICK);
            maxY = fmaxf(maxY, P.pos.y);
            if (P.pos.z < NP[1].disp.z - 1) behind = 1;
            if (NP[1].hp < hpBefore) {
                if (behind) hitBehind = 1;
                if (P.meleeT > 0.1f) swordHits++;
                hpBefore = NP[1].hp;
            }
            if (!NP[1].alive) break;
        }
        if (pass == 0)
            fprintf(f, "cross-up : hauteur max %.1f m, passe derriere=%d, touche dans le dos=%d, PV cible=%.0f (attendu 1, 1)\n",
                    maxY, behind, hitBehind, NP[1].hp);
        else
            fprintf(f, "epee a %.0f m/s relatifs : %d coup(s), PV cible=%.0f (attendu ~%.0f), dash=%.1f (attendu +1)\n",
                    20.0f, swordHits, NP[1].hp, 100 - fminf(melee_dmg(20) * PVP_SCALE, PVP_HIT_CAP), P.dashCharge);
    }
    /* parry : coup dans le vide = recharge longue */
    reset_game();
    dmMode = 0;
    melee();
    for (int i = 0; i < 24; i++) game_tick(TICK);
    fprintf(f, "coup dans le vide : recharge restante %.2f s apres 0,2 s (attendu ~0,7 au lieu de 0,25)" "\n", P.meleeCd);

    /* ping-pong : une roquette adverse renvoyee deux fois, puis le plafond de degats par touche */
    reset_game();
    dmMode = 1;
    memset(NP, 0, sizeof NP);
    NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
    NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Tireur");
    NP[1].disp = v3(-20, 0, 24); vf(NP[1].st.pos, NP[1].disp);
    P.pos = P.prevPos = v3(-20, 0, 41); P.yaw = 0; P.pitch = 0;
    {
        Proj *r = spawn_proj(PR_ROCKET, v3(-20, 1.6f, 38.5f), v3(0, 0, 38));
        r->owner = 1; r->netId = 0x02000009;
        melee();
        float m1 = r->mult, s1 = vlen(r->vel);
        int ok1 = r->kind == PR_PARRIED && r->owner == myId;
        r->owner = 1; r->pos = v3(-20, 1.6f, 38.5f); r->vel = v3(0, 0, s1);   /* l'adversaire la re-pare */
        P.meleeCd = 0; P.parryWin = 0;
        melee();
        fprintf(f, "ping-pong : renvoi 1 ok=%d x%.2f a %.0f m/s, renvoi 2 x%.2f a %.0f m/s (attendu x1.25 a 36, x1.44 a 41)" "\n",
                ok1, m1, s1, r->mult, vlen(r->vel));
        r->alive = 0;
    }
    NP[1].hp = 100;
    net_hit(1, 200, v3(0, 0, 0), W_MELEE);
    fprintf(f, "plafond PvP : une touche de 200 laisse %.0f PV (attendu 40)" "\n", NP[1].hp);

    /* parry d'epee : je suis en fenetre de parry quand l'epee adverse arrive, puis plus */
    {
        reset_game();
        dmMode = 1;
        memset(NP, 0, sizeof NP);
        NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
        NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Bretteur");
        NP[1].disp = v3(-20, 0, 38.5f); vf(NP[1].st.pos, NP[1].disp);
        P.pos = P.prevPos = v3(-20, 0, 41); P.dashCharge = 0.5f;
        P.parryWin = 0.1f;
        host_tick(TICK);
        NetEvent e = ev_new(EV_HIT);
        e.from = 1; e.target = 0; e.sub = W_MELEE; e.f[0] = 60;
        vf(&e.f[1], v3(0, 0, 10));                        /* recul porte par le coup */
        P.vel = v3(0, 0, 0);
        host_event_in(&e);                                /* comme un paquet recu du client 1 */
        float hpParried = NP[0].hp, dash = P.dashCharge;
        V3 velParried = P.vel;
        P.parryWin = 0; nowT += 1;
        host_tick(TICK);
        e.eid++;
        P.vel = v3(0, 0, 0);
        host_event_in(&e);
        fprintf(f, "parry d'epee : PV apres coup pare=%.0f, dash=%.1f ; sans parry PV=%.0f (attendu 100, 1.5, 40)" "\n",
                hpParried, dash, NP[0].hp);
        fprintf(f, "recul d'un coup pare : %.1f m/s ; sans parry %.1f m/s (attendu 0 et 10 : le coup pare n'est pas rejoue)" "\n",
                velParried.z, P.vel.z);
    }

    /* reglages : sauvegarde puis rechargement de velocity.ini */
    {
        int savedBot = botMode;
        botMode = 0;                                      /* les bots ne sauvegardent jamais : on simule un joueur */
        snprintf(cfgPath, sizeof cfgPath, "cfg_test.ini");
        binds[A_COIN][1] = K_X2; sens = 0.003f; fovBase = 115; invertY = 1;
        cfg_save();
        memcpy(binds, bindDefault, sizeof binds); sens = 0.0022f; fovBase = 105; invertY = 0;
        cfg_load();
        fprintf(f, "reglages : piece -> %d (attendu %d), sens %.4f, fov %.0f, axe Y inverse %d (attendu 0.0030, 115, 1)%s",
                binds[A_COIN][1], K_X2, sens, fovBase, invertY, "\n");
        remove(cfgPath);
        cfgPath[0] = 0;
        botMode = savedBot;
        memcpy(binds, bindDefault, sizeof binds); sens = 0.0022f; fovBase = 105; invertY = 0;
    }

    /* contre-jeu : je tire dans la piece de l'adversaire, le ricochet le touche lui */
    reset_game();
    dmMode = 1;
    memset(NP, 0, sizeof NP);
    NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
    NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Lanceur");
    NP[1].disp = v3(-20, 0, 24); vf(NP[1].st.pos, NP[1].disp);
    CO[0].alive = 1; CO[0].owner = 1; CO[0].life = 3; CO[0].netId = 0x02000001;
    CO[0].pos = v3(-18, 4, 28);
    P.pos = P.prevPos = v3(-20, 0, 41);
    {
        V3 d = vsub(CO[0].pos, eye_pos());
        P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
    }
    fire_rail();
    fprintf(f, "contre-jeu : piece adverse detruite=%d, PV du lanceur=%.0f (attendu 1, %.0f), message \"%s\"\n",
            !CO[0].alive, NP[1].hp, 100 - fminf(RAIL_DMG * PVP_SCALE, PVP_HIT_CAP), popText);

    /* en ligne : analyse STUN, lignes du lobby, adresses */
    {
        static const unsigned char stunOk[32] = { 0x01, 0x01, 0, 12, 0x21, 0x12, 0xA4, 0x42, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
                                                  0x00, 0x20, 0, 8, 0, 1, 0xF5, 0x23, 0x73, 0x13, 0xA6, 0x41 };
        struct sockaddr_in m, a;
        char t[40] = "?";
        int ok = stun_parse(stunOk, 32, &m);
        if (ok) ep_text(&m, t, sizeof t);
        PkHead ph;
        memset(&ph, 0, sizeof ph);
        ph.magic = NET_MAGIC; ph.type = PK_SNAP;
        fprintf(f, "STUN : reponse -> %s, paquet de jeu pris pour du STUN=%d (attendu 82.1.2.3:54321 et 0)\n", t,
                stun_parse((const unsigned char *)&ph, 20, &m));
        fprintf(f, "adresses : valide=%d, sans port=%d, octet 300=%d (attendu 1, 0, 0)\n", parse_ep("10.0.0.7:27960", &a),
                parse_ep("10.0.0.7", &a), parse_ep("10.0.0.300:5", &a));

        nFound = 0;
        lb_line("room abc123 3 8 1 5.6.7.8:27960 Bob");
        int okRoom = nFound == 1 && found[0].online && found[0].players == 3 && found[0].maxp == 8 && found[0].spec == 1 &&
                     !strcmp(found[0].id, "abc123") && !strcmp(found[0].name, "Bob") && ntohs(found[0].addr.sin_port) == 27960;
        lb_line("room abc123 4 8 1 5.6.7.8:27960 Bob");
        int updated = nFound == 1 && found[0].players == 4;
        lb_line("room zzz999 1 8 0 9.9.9.9:27960 Eve");
        lb_line("gone abc123");
        fprintf(f, "lobby : salon lu=%d, mise a jour sans doublon=%d, apres gone: %d salon(s), reste %s (attendu 1, 1, 1, Eve)\n",
                okRoom, updated, nFound, nFound ? found[0].name : "-");
        nFound = 0;

        netMode = NET_HOST; myId = 0;                     /* spectateurs : des places a part, sans toucher aux joueurs */
        memset(NP, 0, sizeof NP); memset(SP, 0, sizeof SP);
        NP[0].active = 1;
        int accepted = 0;
        for (int k = 0; k < MAXSPEC + 2; k++) {
            PkJoin pj;
            struct sockaddr_in from;
            memset(&pj, 0, sizeof pj);
            memset(&from, 0, sizeof from);
            from.sin_family = AF_INET; from.sin_port = htons((unsigned short)(40000 + k)); from.sin_addr.s_addr = htonl(0x0A000001);
            snprintf(pj.name, sizeof pj.name, "spec%d", k);
            pj.spec = 1;
            host_on_join(&pj, &from);
            accepted = spec_count();
            if (k == 0) host_on_join(&pj, &from);         /* le meme spectateur qui reessaie ne prend pas une 2e place */
        }
        fprintf(f, "spectateurs : %d acceptes sur %d tentatives (max %d), joueurs actifs=%d (attendu %d, %d, %d, 1)\n", accepted,
                MAXSPEC + 2, MAXSPEC, NP[1].active + NP[0].active, MAXSPEC, MAXSPEC + 2, MAXSPEC);
        memset(SP, 0, sizeof SP);
        memset(NP, 0, sizeof NP);
    }

    botLog = NULL;
    netMode = NET_OFF; dmMode = 0;
    fclose(f);
}

static void do_action(void) {
    int a = pendingAction;
    pendingAction = ACT_NONE;
    if (!myName[0]) snprintf(myName, sizeof myName, "joueur");
    if (a == ACT_SOLO || a == ACT_HOST || a == ACT_BROWSE || a == ACT_QUICK) cfg_save();
    switch (a) {
    case ACT_SOLO:
        net_shutdown(); reset_game(); appState = APP_GAME; set_capture(1);
        break;
    case ACT_HOST:
        if (net_host_start()) { appState = APP_GAME; if (!botMode) set_capture(1); }
        break;
    case ACT_BROWSE:
        if (net_browse_start()) { appState = APP_JOIN; netStatus[0] = 0; }
        break;
    case ACT_JOIN_FOUND:
        join_found(pendingArg, 0);
        break;
    case ACT_SPEC_FOUND:
        join_found(pendingArg, 1);
        break;
    case ACT_QUICK:
        quick_play();
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
        reset_game(); set_capture(1);
        break;
    case ACT_LEAVE:
        net_shutdown(); set_capture(0); appState = APP_MENU; menu_backdrop();
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

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show) {
    (void)prev;
    memcpy(binds, bindDefault, sizeof binds);
    if (cmd && strncmp(cmd, "--test ", 7) == 0) { run_tests(cmd + 7); return 0; }
    const char *shotPath = NULL;
    if (cmd && strncmp(cmd, "--shot ", 7) == 0) { shotMode = 1; shotPath = cmd + 7; }

    /* mode bot : velocity.exe --bot host|join [--name N] [--time S] [--log fichier] [--botshot fichier.bmp] */
    {
        static char args[1024];
        snprintf(args, sizeof args, "%s", cmd ? cmd : "");
        DWORD un = sizeof myName;
        if (!GetUserNameA(myName, &un) || !myName[0]) snprintf(myName, sizeof myName, "joueur");
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
        }
    }

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
    ShowWindow(hwnd, (shotMode || botMode) ? SW_SHOWNOACTIVATE : show);
    UpdateWindow(hwnd);
    { RECT cr; GetClientRect(hwnd, &cr); winW = cr.right; winH = cr.bottom; }

    RAWINPUTDEVICE rid = { 0x01, 0x02, 0, hwnd };
    RegisterRawInputDevices(&rid, 1, sizeof rid);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glHint(GL_FOG_HINT, GL_NICEST);
    make_font(0, 18); make_font(1, 28); make_font(2, 56);
    make_textures();
    build_level();
    build_world_list();
    make_sounds();
    {                                                     /* velocity.ini, a cote de l'executable */
        char *slash;
        GetModuleFileNameA(NULL, cfgPath, sizeof cfgPath);
        slash = strrchr(cfgPath, '\\');
        if (slash) strcpy(slash + 1, "velocity.ini"); else cfgPath[0] = 0;
        if (!botMode && !shotMode && cfg_load()) toggle_fullscreen();
    }
    if (!shotMode) audio_init();
    reset_game();

    if (shotMode && (strstr(shotPath, "settings") || strstr(shotPath, "menu"))) {   /* captures des menus */
        appState = APP_MENU;
        menu_backdrop();
        if (strstr(shotPath, "settings")) { settingsOpen = 1; waitAct = A_COIN; waitSlot = 1; }
        render(1, TICK);
        save_bmp(shotPath);
        return 0;
    }
    if (shotMode && strstr(shotPath, "dm")) {   /* capture d'une scene de deathmatch */
        appState = APP_GAME; dmMode = 1; myId = 0;
        memset(NP, 0, sizeof NP);
        snprintf(NP[0].name, 16, "Moi"); NP[0].active = NP[0].alive = 1; NP[0].hp = 100; NP[0].kills = 3;
        snprintf(NP[1].name, 16, "Rival"); NP[1].active = NP[1].alive = 1; NP[1].kills = 5; NP[1].deaths = 2;
        NP[1].disp = v3(-6.5f, 0, 17); NP[1].st.yaw = PI - 0.4f; NP[1].st.weapon = 1; NP[1].st.melee = 120;
        NP[1].st.flags = NF_HOOK; vf(NP[1].st.hookPos, v3(-24, 14, -24));
        snprintf(NP[2].name, 16, "Ninja"); NP[2].active = NP[2].alive = 1; NP[2].kills = 1; NP[2].deaths = 4;
        NP[2].disp = v3(5, 4, 6); NP[2].st.yaw = PI + 0.6f; NP[2].st.pitch = -0.2f;
        kfeed_add(1, 2, W_MELEE, 0); kfeed_add(0, 2, W_RAIL, 0); kfeed_add(-1, 1, 0, 1);
        P.pos = P.prevPos = v3(0, 0, 24); P.yaw = -0.1f; P.pitch = 0.08f; P.hp = 100;
        matchT = 95;
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
        shotStreak = 4; P.meleeT = 0.12f; P.vel = v3(0, 0, -14);   /* pour voir le HUD des deux axes */
        if (strstr(shotPath, "impact") || strstr(shotPath, "fx")) {   /* un kill au rail a pleine vitesse */
            P.weapon = 0; P.railAmmo = 5; P.fireCd = 0; P.meleeT = 0;
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 60; E[0].pos = vadd(eye_pos(), vmul(view_dir(), 9));
            fire_rail();
            if (strstr(shotPath, "fx")) {
                for (int i = 0; i < 14; i++) game_tick(TICK);
                impactT = 0;
            }
            P.vel = v3(0, 0, -32);
        }
        render(1, TICK);
        save_bmp(shotPath);
        return 0;
    }

    appState = APP_MENU;
    menu_backdrop();
    if (botMode == 1) pendingAction = ACT_HOST;
    if (botMode == 2) pendingAction = ACT_BROWSE;

    LARGE_INTEGER freq, start, last, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    last = start;
    float acc = 0, fpsAcc = 0; int fpsFrames = 0;
    MSG msgw;
    for (;;) {
        while (PeekMessageA(&msgw, NULL, 0, 0, PM_REMOVE)) {
            if (msgw.message == WM_QUIT) goto done;
            TranslateMessage(&msgw);
            DispatchMessageA(&msgw);
        }
        QueryPerformanceCounter(&now);
        float dt = (float)(now.QuadPart - last.QuadPart) / (float)freq.QuadPart;
        nowT = (double)(now.QuadPart - start.QuadPart) / (double)freq.QuadPart;
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        fpsAcc += dt; fpsFrames++;
        if (fpsAcc > 0.5f) { fps = fpsFrames / fpsAcc; fpsAcc = 0; fpsFrames = 0; }

        net_update(dt);
        if (botMode) {
            bot_think(dt);
            if (nowT > botTime) break;
        }
        if (appState == APP_GAME) {
            if (captured) {
                if (!P.dead) {
                    P.yaw += mdx * sens;
                    P.pitch = clampf(P.pitch - mdy * sens * (invertY ? -1.0f : 1.0f), -1.55f, 1.55f);
                    P.swayX = clampf(P.swayX - mdx * 0.00004f, -0.04f, 0.04f);
                    P.swayY = clampf(P.swayY + mdy * 0.00004f, -0.04f, 0.04f);
                }
                P.swayX *= expf(-dt * 10); P.swayY *= expf(-dt * 10);
            }
            mdx = mdy = 0;
            if (captured || dmMode) {             /* en LAN la partie ne se met jamais en pause */
                acc += dt;
                while (acc >= TICK) { game_tick(TICK); acc -= TICK; }
            } else {
                memset(latch, 0, sizeof latch);
            }
        } else {
            P.yaw += dt * 0.05f;                  /* l'arene tourne doucement derriere le menu */
            mdx = mdy = 0;
            memset(latch, 0, sizeof latch);
        }
        render((captured || dmMode) && appState == APP_GAME ? acc / TICK : 1.0f, dt);
        uiClick = 0;
        SwapBuffers(hdc);
        audio_update();
        if (lbWantHost) { lbWantHost = 0; if (appState == APP_JOIN) pendingAction = ACT_HOST; }
        if (pendingAction) do_action();
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
            netlog("equilibre : temps pour tuer moyen %.2f s sur %d kills, one-shots %d, parries reussis %d, epees parees %d, chocs %d",
                   ttkN ? ttkSum / ttkN : 0.0, ttkN, oneShots, parryCount, meleeParries, clashes);
        if (netMode == NET_HOST)
            netlog("touches arbitrees : rail %d, roquette %d, epee %d, grappin %d, parry %d, sur soi %d, ricochet %d ; parries %d",
                   hitCount[0], hitCount[1], hitCount[2], hitCount[3], hitCount[4], hitCount[5], hitCount[6], parryCount);
    }
    net_shutdown();
    if (botLog) fclose(botLog);
    set_capture(0);
    if (audio_ok) { waveOutReset(wout); waveOutClose(wout); }
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
    return 0;
}
