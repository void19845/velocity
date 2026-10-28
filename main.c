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

/* Elan (convertisseur cinetique) : la vitesse horizontale depensee devient des degats */
#define KIN_CD            0.5f    /* recharge de l'Elan (divisee par streak_rate) */
#define KIN_DMG_BASE      20.0f   /* degats a l'arret */
#define KIN_DMG_PER_MS    5.0f    /* degats en plus par m/s de vitesse depensee */
#define KIN_SPEED_CAP     15.0f   /* vitesse horizontale maximale convertie par tir */
#define KIN_KNOCK         3.0f    /* recul de la cible a l'arret (PvE ; moitie en PvP) */
#define KIN_KNOCK_PER_MS  0.4f    /* recul en plus par m/s depensee : 3 -> 9 m/s */
#define KIN_FULL          14.5f   /* au-dela : tir "plein" (hitstop, son et rayon max) */
#define KIN_DASH_WINDOW   0.10f   /* dash-tir : le tir doit partir dans les 12 ticks apres le dash */
#define KIN_DASH_FORGET   0.6f    /* jusque-la, hors fenetre, la vitesse du dash ne compte pas */

/* Fusil a pompe "Eventail" : 10 plombs, plein jusqu'a 10 m, x0.3 a 30 m */
#define SG_PELLETS        10
#define SG_HALF_ANGLE     0.0785f /* demi-angle du cone en radians (4.5 deg, cone total 9 deg) */
#define SG_SLIDE_SPREAD   0.5f    /* en slide : dispersion divisee par 2 */
#define SG_PELLET_DMG     8.0f    /* degats PvE par plomb */
#define SG_NEAR           10.0f   /* degats pleins jusqu'ici */
#define SG_FAR            30.0f   /* ... puis lineaire jusqu'a SG_FAR_MULT ici */
#define SG_FAR_MULT       0.3f
#define SG_RANGE          60.0f   /* portee max d'un plomb */
#define SG_CD             0.95f   /* recharge (divisee par streak_rate) */
#define SG_STREAK_MIN     3       /* plombs touches pour compter dans la chaine */
#define SG_PELLET_KNOCK   0.6f    /* recul par plomb sur la cible */
#define SG_RECOIL         7.0f    /* recul du tireur en l'air */
#define SG_RECOIL_VMAX    9.0f    /* le recul ne pousse pas vy au-dela (sauf si deja plus) */
#define SG_RECOIL_HCAP    16.0f   /* idem pour la vitesse horizontale */
#define SG_COIN_RADIUS    0.55f   /* zone de touche d'une piece pour un plomb (rail : 0.9) */
#define SG_BURST_RADIUS   3.0f    /* piece eclatee : souffle */
#define SG_BURST_DMG      45.0f   /* au centre, PvE (x PVP_SCALE contre un joueur) */
#define SG_BURST_KNOCK    8.0f
#define SG_BURST_CHAIN    3       /* une piece eclatee fait eclater les pieces voisines (profondeur max) */
#define SG_TRACER_SPEED   260.0f  /* vitesse visuelle d'un traceur */
#define MAXTR             64      /* traceurs de plombs */

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

/* --- Enclume (marteau-pilon) : la melee verticale, elle recompense la hauteur --- */
/* coup au sol (ou en l'air trop pres du sol) */
#define ANVIL_CD           0.8f    /* recharge du coup au sol */
#define ANVIL_WHIFF_CD     1.2f    /* coup au sol dans le vide */
#define ANVIL_SWING_T      0.3f    /* duree de l'animation du coup */
#define ANVIL_RANGE        2.6f    /* portee de base */
#define ANVIL_RANGE_PER_MS 0.10f   /* portee en plus par m/s relatif */
#define ANVIL_DMG          45.0f   /* degats de base */
#define ANVIL_DMG_PER_MS   3.0f    /* degats en plus par m/s relatif */
#define ANVIL_CONE         0.45f   /* cosinus du demi-angle (plus large que l'epee) */
#define ANVIL_KNOCK        14.0f   /* recul vers l'avant */
#define ANVIL_KNOCK_UP     6.0f    /* recul vers le haut */
#define ANVIL_PARRY_WINDOW 0.10f   /* fenetre de parry, plus courte que l'epee */
/* pique et onde de choc */
#define SLAM_VY            30.0f   /* vitesse de chute minimale du pique */
#define SLAM_VY_MAX        45.0f   /* plafond de |vy| */
#define SLAM_MIN_H         1.2f    /* hauteur minimale sous les pieds pour piquer (sinon : coup au sol) */
#define SLAM_FULL_H        4.0f    /* hauteur de chute pour 100% des degats (0 = desactive) */
#define SLAM_RADIUS        2.5f    /* rayon de base de l'onde */
#define SLAM_RADIUS_PER_MS 0.06f   /* rayon en plus par m/s de chute */
#define SLAM_DMG           25.0f   /* degats de base de l'onde */
#define SLAM_DMG_PER_MS    1.5f    /* degats en plus par m/s de chute */
#define SLAM_REACH_UP      1.0f    /* l'onde ne touche que les pieds a moins de 1 m du sol : un saut simple l'esquive */
#define SLAM_REACH_DOWN    1.0f    /* ni ce qui est plus bas que le sol d'impact */
#define SLAM_KNOCK_UP      12.0f   /* recul vertical des cibles */
#define SLAM_KNOCK_OUT     8.0f    /* recul horizontal au centre */
#define SLAM_BOUNCE        9.0f    /* rebond si l'onde touche */
#define SLAM_STUN          0.6f    /* immobilisation si l'onde rate */
#define SLAM_CD            0.8f    /* recharge apres l'atterrissage */
#define SLAM_BREAK_VY      -15.0f  /* poussee vers le haut qui casse le pique */
#define SLAM_PARRY_GRACE   0.2f    /* tolerance de l'hote pour parer l'onde */
#define SLAM_PARRIED_UP    12.0f   /* attaquant pare : renvoye en l'air */
#define ST_SLAM            0x10    /* NetPState.pad : pique en cours */
enum { ANVIL_IDLE, ANVIL_DIVE, ANVIL_STUN };

/* Axe tir : chaque touche successive augmente degats et cadence, un tir rate remet a zero */
#define STREAK_MAX        8
#define STREAK_DMG        0.15f   /* +15% de degats par touche */
#define STREAK_RATE       0.05f   /* +5% de cadence par touche */

/* Loadout : 1 arme de melee + 2 armes a distance. Aucune recharge passive : chaque arme a une jauge
 * de points, remplie par la melee et les cellules ; jauge pleine = 1 munition. */
enum { MK_SWORD, MK_ANVIL, MK_LANCE, MK_FLAIL, MK_FIST, MK_SHIELD, MK_COUNT };
enum { WP_RAIL, WP_ROCKET, WP_SHOTGUN, WP_KINETIC, WP_SAW, WP_MINE, WP_TESLA, WP_COUNT };
enum { AP_HIT, AP_KILL, AP_PARRY, AP_CELL, AP_LOST, AP_COUNT };   /* origine des points (journal) */
#define PTS_MELEE_KILL     100     /* kill en melee, toutes armes */
#define PTS_PARRY          50      /* parry reussi, une fois par coup */
#define PTS_LANCE_JOUST    100     /* joute de la Lance (l'estoc = meleePts) */
#define PTS_FIST_COMBO_MAX 50      /* Poings : au plus 50 points par combo */
#define PTS_SHIELD_PERFECT 100     /* Egide : parry parfait (remplace PTS_PARRY) */
#define PICKUP_SMALL       100     /* petite cellule : points a chaque arme du loadout */
#define PICKUP_BIG         150     /* grande cellule (endroits exposes) */
#define REFUND_UNDO_T      0.5f    /* coup declare pare par l'hote : on rend ses points s'il a moins de 0,5 s */
#define SWITCH_TIME        0.22f   /* changement d'arme */
#define DRYFIRE_CD         0.35f   /* delai entre deux clics a sec */
#define ST_MASK            0xF0    /* NetPState.pad : bits 4-7 = etats d'arme, bits 0-3 = meleeKind */

/* points de melee par coup qui touche (Lance : estoc ; Egide : coup de bouclier) */
static const int meleePts[MK_COUNT] = { 50, 75, 50, 35, 25, 50 };

/* gauge : points pour gagner 1 munition (0 = pas de jauge). Tesla : points par unite de batterie */
typedef struct { const char *name; int start, max; float gauge; float r, g, b; } AmmoDef;
static const AmmoDef ammoDef[WP_COUNT] = {
    { "RAILGUN",   2,  2,   250, 0.40f, 0.85f, 1.00f },   /* tres haut degats : 2 tirs, tres lent */
    { "ROQUETTES", 4,  8,   50,  1.00f, 0.60f, 0.20f },   /* 1 coup d'epee = 1 roquette */
    { "EVENTAIL",  3,  6,   100, 1.00f, 0.85f, 0.35f },
    { "ELAN",      3,  6,   80,  0.55f, 1.00f, 0.55f },
    { "SCIE",      1,  1,   0,   0.90f, 0.90f, 0.95f },   /* objet unique : aucune jauge */
    { "VENTOUSE",  1,  3,   150, 1.00f, 0.35f, 0.35f },
    { "TESLA",     60, 100, 2.5f, 0.60f, 0.70f, 1.00f },  /* 1 point = 0,4 de batterie */
};
static const char *meleeNames[MK_COUNT] = { "EPEE", "ENCLUME", "LANCE", "FLEAU", "IMPULSION", "EGIDE" };
static unsigned char wpReady[WP_COUNT] = { 1, 1, 1, 1, 0, 0, 0 };   /* chaque arme codee passe sa case a 1 */
static unsigned char mkReady[MK_COUNT] = { 1, 1, 0, 0, 0, 0 };
static const float wpCd[WP_COUNT] = { RAIL_CD, ROCKET_CD, SG_CD, KIN_CD, 0.4f, 0.5f, 0.1f };  /* barre de recharge du HUD */

/* ------------------------------------------------------------------ */
/* Audio : mini mixeur waveOut + sons synthetises                      */
/* ------------------------------------------------------------------ */
#define SR      44100
#define NBUF    4
#define BUFLEN  512
#define MAXV    32
enum { S_RAIL, S_ROCKET, S_EXPLO, S_JUMP, S_LAND, S_DASH, S_HIT, S_KILL, S_HURT,
       S_ESHOT, S_PAD, S_WAVE, S_WALLJUMP, S_SLIDE, S_HOOK, S_HOOKHIT, S_PUNCH, S_PARRY,
       S_KIN,                                             /* Elan */
       S_ANVIL_DIVE, S_ANVIL_SLAM, S_ANVIL_CLANG,
       S_SG_FIRE, S_SG_PUMP,                              /* Eventail */
       S_COUNT };
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
    /* Elan : "whump" grave balaye + souffle + tintement metallique (volume et hauteur selon l'energie) */
    b = sbuf(0.45f, &n); ph = 0; lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        ph += (45 + 115 * expf(-t * 12)) / SR;
        lp += 0.1f * (noise() - lp);
        b[i] = sinf(ph * 2 * PI) * 0.8f * expf(-t * 7) + lp * 1.6f * expf(-t * 18) +
               sinf(t * 2 * PI * 1800) * 0.15f * expf(-t * 30);
    }
    sounds[S_KIN] = mksound(b, n);

    /* --- Enclume --- */
    /* pique : sifflement descendant qui enfle (la duree couvre un pique typique) */
    b = sbuf(1.0f, &n); ph = 0; lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        ph += (500 + 1300 * expf(-t * 2.5f)) / SR;
        lp += 0.15f * (noise() - lp);
        float env = t / 1.0f * (t > 0.97f ? (1.0f - t) / 0.03f : 1.0f);
        b[i] = (sinf(ph * 2 * PI) * 0.25f + lp * 0.9f) * env;
    }
    sounds[S_ANVIL_DIVE] = mksound(b, n);
    /* onde de choc : grave qui tombe, souffle filtre, deux partiels metalliques */
    b = sbuf(1.0f, &n); ph = 0; lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        ph += (55 - 25 * t) / SR;
        float fc = 150 + 2500 * expf(-t * 7);
        a = 1 - expf(-2 * PI * fc / SR);
        lp += a * (noise() - lp);
        b[i] = sinf(ph * 2 * PI) * 0.9f * expf(-t * 4) + lp * 1.3f * expf(-t * 4.5f) +
               (sinf(t * 2 * PI * 220) + sinf(t * 2 * PI * 347)) * 0.15f * expf(-t * 6);
    }
    sounds[S_ANVIL_SLAM] = mksound(b, n);
    /* coup au sol qui touche : partiels inharmoniques + clic */
    b = sbuf(0.4f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        b[i] = sinf(t * 2 * PI * 520) * 0.35f * expf(-t * 12) + sinf(t * 2 * PI * 1310) * 0.25f * expf(-t * 18) +
               sinf(t * 2 * PI * 2150) * 0.18f * expf(-t * 25) + (t < 0.005f ? noise() * 0.8f : 0);
    }
    sounds[S_ANVIL_CLANG] = mksound(b, n);

    /* --- Eventail --- */
    /* tir : bruit passe-bas dont la coupure descend de 6000 a 300 Hz, coup grave, claquement */
    b = sbuf(0.5f, &n); lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        float fc = 300 + 5700 * expf(-t * 14);
        a = 1 - expf(-2 * PI * fc / SR);
        lp += a * (noise() - lp);
        b[i] = lp * 1.8f * expf(-t * 9) + sinf(2 * PI * (70 - 40 * t) * t) * 0.9f * expf(-t * 10) + noise() * 0.6f * expf(-t * 60);
    }
    sounds[S_SG_FIRE] = mksound(b, n);
    /* pompe : deux clics (t = 0 et 0,12 s) et un toc */
    b = sbuf(0.25f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        float s = sinf(2 * PI * 220 * t) * 0.2f * expf(-t * 30);
        for (int c = 0; c < 2; c++) {
            float dc = t - c * 0.12f;
            if (dc >= 0) s += noise() * expf(-dc * 400) * 0.5f + sinf(2 * PI * 1800 * dc) * 0.3f * expf(-dc * 300);
        }
        b[i] = s;
    }
    sounds[S_SG_PUMP] = mksound(b, n);
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

/* cellules de munitions posees sur la carte ; t > 0 : deja ramassee, revient dans t secondes.
 * pts : points donnes a la jauge de chacune des deux armes du loadout (PICKUP_SMALL ou PICKUP_BIG) */
#define MAXPICK 16
typedef struct { V3 pos; float t, pending; int pts; } Pickup;
static Pickup PU[MAXPICK];
static int npu;

static void addpickup(float x, float y, float z, int pts) {
    if (npu >= MAXPICK) return;
    PU[npu].pos = v3(ms(x), y + 0.9f, ms(z));
    PU[npu].t = PU[npu].pending = 0;
    PU[npu].pts = pts;
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

    /* cellules : grandes aux endroits exposes (tour, perchoirs), petites ailleurs ; l'ordre fixe les index reseau */
    addpickup(0, 13, 0, PICKUP_BIG);                                /* sommet de la tour, */
    addpickup(36, 7, 0, PICKUP_SMALL); addpickup(-36, 7, 0, PICKUP_SMALL);   /* plateformes hautes, */
    addpickup(0, 7, 36, PICKUP_SMALL); addpickup(0, 7, -36, PICKUP_SMALL);
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sz = -1; sz <= 1; sz += 2) {
            addpickup(sx * 36.0f, 13, sz * 36.0f, PICKUP_BIG);      /* perchoirs */
            addpickup(sx * 46.0f, 0, sz * 46.0f, PICKUP_SMALL);     /* et coins au sol */
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
    float coins;                                          /* charges de pieces (0..COIN_MAX) */
    int hookPlayer;                                       /* 0 = aucun, sinon id du joueur accroche + 1 (LAN) */
    int loadout[2];                                       /* deux WP_* distincts ; weapon = WP_* en main */
    int meleeKind;                                        /* MK_* */
    int ammo[WP_COUNT];                                   /* munitions (batterie Tesla 0..100) */
    float ammoGauge[WP_COUNT];                            /* points vers la prochaine munition, 0 au maximum */
    int swingHitPts, swingParryPts;                       /* ce coup a deja donne ses points de touche / de parry */
    float undoPts[2];                                     /* points de la derniere touche par emplacement (annulables) */
    double undoT;
    /* Elan : decharge du viewmodel, energie du dernier tir (0..1), temps depuis le dernier dash,
     * vitesse horizontale d'avant le dash encore convertible */
    float kinT, kinE, kinDashT, kinPreDash;
    int anvilState;                                       /* Enclume : ANVIL_IDLE / DIVE / STUN */
    float anvilStartY, anvilStunT;                        /* y au debut du pique, immobilisation restante */
    float anvilLandT, anvilLastVy;                        /* temps depuis la derniere onde, |vy| de la derniere onde */
    float sgPumpT, sgPumpMax;                             /* Eventail : animation de pompe (temps restant / duree totale) */
    int sgLastPellets; float sgLastT;                     /* Eventail : HUD "7/10" apres un tir qui touche */
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

typedef struct { V3 a, b, col; float t, life; } Tracer;   /* Eventail : traceurs de plombs (pas add_beam : MAXBM 32) */
static Tracer TR[MAXTR];
static int trNext;

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
enum { W_RAIL, W_ROCKET, W_MELEE, W_HOOK, W_PARRY, W_SELF, W_COIN, W_SLAM, W_LANCE, W_FLAIL, W_FIST,
       W_SHIELD, W_SHOTGUN, W_KINETIC, W_SAW, W_MINE, W_TESLA, W_COUNT };
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
    unsigned char anvilPrevPad;                           /* Enclume : pad du tick precedent (debut d'un pique adverse) */
    float sgFireT;                                        /* Eventail : recul visuel du fusil d'un joueur distant */
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
static void net_fx_kinetic(V3 a, V3 b, float e);
static void net_fx_rocket(V3 pos, V3 vel, unsigned int netId);
static void net_fx_rocket_end(unsigned int netId, V3 pos);
static void net_fx_parry(unsigned int netId, V3 pos, V3 vel, float mult, int parries);
static void net_fx_melee(V3 pos, V3 dir, float sp);
static void net_fx_slam(V3 pos, float radius, float vy, int hits);
static void net_fx_coin(int sub, unsigned int netId, V3 pos, V3 vel);
static void net_fx_shot(V3 eye, V3 dir, float half, unsigned int seed);

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

/* Eventail : traceur de plomb, la tete avance a SG_TRACER_SPEED puis la ligne s'eteint en 0,12 s */
static void add_tracer(V3 a, V3 b, V3 col) {
    Tracer *r = &TR[trNext]; trNext = (trNext + 1) % MAXTR;
    r->a = a; r->b = b; r->col = col; r->t = 0;
    r->life = vlen(vsub(b, a)) / SG_TRACER_SPEED + 0.12f;
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
static void slam_land(float vyImpact);                    /* Enclume : definies avec la melee */
static void slam_contact(void);

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

    /* Enclume : pendant le pique et l'immobilisation, plus de controle (ni saut ni grappin) */
    if (P.dead && P.anvilState != ANVIL_IDLE) { P.anvilState = ANVIL_IDLE; P.anvilStunT = 0; }
    int diving = P.anvilState == ANVIL_DIVE, stun = P.anvilState == ANVIL_STUN;
    V3 wishIn = wish;
    int hasWishIn = hasWish;
    if (diving || stun) { fw = sd = 0; wish = v3(0, 0, 0); hasWish = 0; jumpPress = 0; }
    if (stun) dashPress = 0;
    if (diving && dashPress && P.dashCharge >= 1) {       /* le dash annule le pique : l'echappatoire payante */
        P.anvilState = ANVIL_IDLE; P.meleeCd = SLAM_CD; diving = 0;
        wish = wishIn; hasWish = hasWishIn;
    }

    P.slideCd -= dt; P.wallJumpCd -= dt;
    P.kinDashT = fminf(P.kinDashT + dt, 10);              /* Elan : temps depuis le dernier dash */
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
    if (alive && !diving && !stun && (jumpPress || (act[A_JUMP] && P.grounded))) {
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
        {   /* Elan : vitesse d'avant le dash ; pendant le malus d'un dash precedent, elle ne peut pas monter */
            float hv = hlen(P.vel);
            P.kinPreDash = P.kinDashT >= KIN_DASH_FORGET ? hv : fminf(P.kinPreDash, hv);
            P.kinDashT = 0;
        }
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

    grapple_tick(dt, alive && hookPressIn && !diving && !stun, alive && hookHeldIn);

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

    /* Enclume : chute plafonnee ; une poussee vers le haut (roquette, recul) casse le pique */
    if (diving) {
        if (P.vel.y < -SLAM_VY_MAX) P.vel.y = -SLAM_VY_MAX;
        if (P.vel.y > SLAM_BREAK_VY) { P.anvilState = ANVIL_IDLE; P.meleeCd = fmaxf(P.meleeCd, SLAM_CD); diving = 0; }
        else for (int k = 0; k < 2; k++)                  /* trainee du meteore (au-dessus de la tete : hors de la vue) */
            add_part(vadd(P.pos, v3(frange(-.3f, .3f), frange(1.9f, 2.4f), frange(-.3f, .3f))), v3(frange(-1, 1), 4, frange(-1, 1)),
                     v3(1, 0.6f, 0.25f), 0.35f, 0.14f, 0);
    }
    if (stun) {                                           /* immobilise : seul un recul vers le haut libere */
        if (P.vel.y > 2) { P.anvilState = ANVIL_IDLE; P.anvilStunT = 0; stun = 0; }
        else {
            P.vel.x = P.vel.z = 0;
            if ((P.anvilStunT -= dt) <= 0) { P.anvilState = ANVIL_IDLE; P.anvilStunT = 0; }
        }
    }

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

    /* Enclume : contact direct en l'air, puis onde de choc a l'atterrissage (avant le jump pad) */
    int onPad = P.grounded && boxes[P.groundBox].kind == B_PAD;
    int slammed = 0;
    if (diving && alive) slam_contact();
    if (P.grounded && !wasG && P.anvilState == ANVIL_DIVE && alive) { slam_land(-prevVy); slammed = 1; }

    if (P.grounded && !wasG && !slammed) {
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

    if (onPad && alive) {
        V3 L = boxes[P.groundBox].launch;
        P.vel = L; P.grounded = 0; P.airJumps = 1; P.coyote = 1;
        P.anvilState = ANVIL_IDLE; P.anvilStunT = 0;      /* le pad relance apres l'onde : pas d'immobilisation */
        P.fovKick += 6;
        burst(P.pos, v3(0.3f, 0.9f, 1.0f), 30, 6, 0.6f, 0.15f);
        play(S_PAD, 0.8f, 1.0f);
    }

    wall_check();
    if (alive && P.anvilState != ANVIL_DIVE && !P.grounded && P.hook != HOOK_ATTACHED && P.wallTouch && act[A_FWD] &&
        hlen(P.vel) > WALLRUN_MIN && P.wallTime < WALLRUN_MAX) {
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

/* ------------------------------------------------------------------ */
/* Loadout et munitions a jauge                                        */
/* ------------------------------------------------------------------ */
/* le choix vit hors de P (reset_game remet P a zero) et ne s'applique qu'au (re)spawn */
static int wantLoadout[2] = { WP_RAIL, WP_ROCKET }, wantMelee = MK_SWORD;
static int loadoutOpen;                                   /* ecran LOADOUT ouvert */
static float ammoGainT, dryHintT, dryT;                   /* HUD : "+50" vert, panneau de melee qui clignote ; temps a sec */
static int ammoGainSlot, ammoGainPts;                     /* emplacement qui clignote (2 = les deux) */
static float apStat[AP_COUNT];                            /* journal : points par origine */
static int cellsTaken;

static int cur_slot(void) { return P.weapon == P.loadout[1]; }
static int other_wp(void) { return P.loadout[cur_slot() ^ 1]; }
static int wp_has_ammo(int w) { return P.ammo[w] > 0; }
static int is_melee_w(int w) { return w == W_MELEE || (w >= W_SLAM && w <= W_SHIELD); }
static int is_parryable_w(int w) { return is_melee_w(w) || w == W_HOOK; }
static int wp_full(int w) { return ammoDef[w].gauge <= 0 || P.ammo[w] >= ammoDef[w].max; }
static int loadout_full(void) { return wp_full(P.loadout[0]) && wp_full(P.loadout[1]); }
static float gauge_frac(int w) {                          /* HUD : 0..1, pleine au maximum */
    if (ammoDef[w].gauge <= 0) return 0;
    return P.ammo[w] >= ammoDef[w].max ? 1 : P.ammoGauge[w] / ammoDef[w].gauge;
}

/* ajoute des points a la jauge de w ; chaque jauge pleine donne 1 munition. Renvoie les points inutilises */
static float gauge_add(int w, float pts) {
    const AmmoDef *d = &ammoDef[w];
    if (d->gauge <= 0 || P.ammo[w] >= d->max) return pts;
    P.ammoGauge[w] += pts;
    while (P.ammoGauge[w] >= d->gauge) {
        P.ammoGauge[w] -= d->gauge;
        if (++P.ammo[w] >= d->max) { float rest = P.ammoGauge[w]; P.ammoGauge[w] = 0; return rest; }
    }
    return 0;
}
static void gauge_remove(int w, float pts) {              /* annulation : peut reprendre une munition */
    if (ammoDef[w].gauge <= 0 || pts <= 0) return;
    P.ammoGauge[w] -= pts;
    while (P.ammoGauge[w] < 0 && P.ammo[w] > 0) { P.ammo[w]--; P.ammoGauge[w] += ammoDef[w].gauge; }
    if (P.ammoGauge[w] < 0) P.ammoGauge[w] = 0;
}

/* points de melee : l'arme en main d'abord, le surplus a l'autre arme du loadout */
static void ammo_points(float pts, int src) {
    if (P.dead || pts <= 0) return;
    int s = cur_slot(), a0 = P.ammo[P.loadout[0]], a1 = P.ammo[P.loadout[1]];
    float rest = gauge_add(P.loadout[s], pts), lost = gauge_add(P.loadout[s ^ 1], rest);
    apStat[src] += pts - lost; apStat[AP_LOST] += lost;
    if (src == AP_HIT) {                                  /* memorise pour une annulation eventuelle */
        P.undoPts[s] = pts - rest; P.undoPts[s ^ 1] = rest - lost; P.undoT = nowT;
    }
    if (pts - lost <= 0) return;
    ammoGainT = 0.8f; ammoGainPts = (int)pts; ammoGainSlot = pts - rest > 0 ? s : s ^ 1;
    if (P.ammo[P.loadout[0]] > a0 || P.ammo[P.loadout[1]] > a1) play(S_PAD, 0.5f, 3.0f);   /* munition gagnee */
    else play(S_PAD, 0.35f, 2.4f);
}
static void melee_start_swing(void) { P.swingHitPts = P.swingParryPts = 0; }
static void melee_hit_points(int pts) { if (!P.swingHitPts) { P.swingHitPts = 1; ammo_points((float)pts, AP_HIT); } }
static void melee_parry_points(int pts) { if (!P.swingParryPts) { P.swingParryPts = 1; ammo_points((float)pts, AP_PARRY); } }

static int loadout_valid(const int *l, int mk) {
    return l[0] >= 0 && l[0] < WP_COUNT && l[1] >= 0 && l[1] < WP_COUNT && l[0] != l[1] &&
           wpReady[l[0]] && wpReady[l[1]] && mk >= 0 && mk < MK_COUNT && mkReady[mk];
}
static void loadout_apply(void) {                         /* (re)spawn : munitions de depart, jauges vides, arme 1 en main */
    if (!loadout_valid(wantLoadout, wantMelee)) { wantLoadout[0] = WP_RAIL; wantLoadout[1] = WP_ROCKET; wantMelee = MK_SWORD; }
    P.loadout[0] = wantLoadout[0]; P.loadout[1] = wantLoadout[1]; P.meleeKind = wantMelee;
    for (int w = 0; w < WP_COUNT; w++) { P.ammo[w] = ammoDef[w].start; P.ammoGauge[w] = 0; }
    P.weapon = P.loadout[0]; P.switchT = 0; P.undoPts[0] = P.undoPts[1] = 0;
    P.kinT = 0; P.kinDashT = 1; P.kinPreDash = 0;          /* Elan : ni fenetre de dash-tir ni malus */
    P.sgPumpT = P.sgPumpMax = 0; P.sgLastT = 0;           /* Eventail : pompe au repos */
    melee_start_swing();
}
static void weapon_select(int w) { if (w != P.weapon) { P.weapon = w; P.switchT = SWITCH_TIME; } }

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
        melee_parry_points(PTS_PARRY);                    /* une fois par coup, meme si plusieurs renvois */
        if (p->parries > 1) { snprintf(popText, sizeof popText, "PARRY x%d", p->parries); popT = 1.0f; }
    }
    return parried;
}

/* ------------------------------------------------------------------ */
/* Enclume (marteau-pilon) : coup au sol, pique, onde de choc          */
/* Au sol : coup lourd frontal. En l'air (assez haut) : pique a 30 m/s */
/* minimum, puis onde de choc a l'atterrissage dont rayon et degats    */
/* montent avec la vitesse de chute. Touche : rebond + dash ; rate :   */
/* immobilise 0,6 s. L'onde rampe au sol : sauter l'esquive.           */
/* ------------------------------------------------------------------ */
static int slamDives, slamWaves, slamWaveHits, slamStuns;   /* journal du bot */
static float slamVySum, slamDmgSum;

static float anvil_range(float rel) { return ANVIL_RANGE + rel * ANVIL_RANGE_PER_MS; }
static float anvil_dmg(float rel) { return ANVIL_DMG + rel * ANVIL_DMG_PER_MS; }
static float slam_radius(float vy) { return SLAM_RADIUS + SLAM_RADIUS_PER_MS * vy; }
static float slam_kh(float fallH) { return SLAM_FULL_H > 0 ? clampf(fallH / SLAM_FULL_H, 0.5f, 1) : 1; }
static float slam_dmg(float vy, float fallH) { return (SLAM_DMG + SLAM_DMG_PER_MS * vy) * slam_kh(fallH); }

/* distance au sol sous un point (les pieds), 50 si rien */
static float anvil_ground_dist(V3 feet) {
    return ray_world(vadd(feet, v3(0, 0.05f, 0)), v3(0, -1, 0), 50, 0) - 0.05f;
}

static void anvil_reset(void) { P.anvilState = ANVIL_IDLE; P.anvilStunT = 0; P.anvilLandT = 10; }

/* Prevision du pique (sans controle aerien, la trajectoire est deterministe) : point d'impact,
 * |vy| a l'impact, rayon de l'onde et temps de chute. Sert au repere d'impact, au HUD et au bot. */
static int slam_predict(V3 *at, float *vy, float *R, float *tFall) {
    float h = anvil_ground_dist(P.pos);
    if (h >= 50) return 0;
    float v0 = P.anvilState == ANVIL_DIVE ? fmaxf(-P.vel.y, 0) : fmaxf(-P.vel.y, SLAM_VY);
    float t = (-v0 + sqrtf(v0 * v0 + 2 * GRAVITY * h)) / GRAVITY;
    V3 p = vadd(P.pos, v3(P.vel.x * t, 0, P.vel.z * t));
    float drop = anvil_ground_dist(v3(p.x, P.pos.y, p.z));
    if (drop >= 50) drop = h;
    *at = v3(p.x, P.pos.y - drop, p.z);
    *vy = fminf(sqrtf(v0 * v0 + 2 * GRAVITY * drop), SLAM_VY_MAX);
    *R = slam_radius(*vy);
    if (tFall) *tFall = t;
    return 1;
}

/* coup au sol : copie de l'epee avec les reglages de l'Enclume */
static void anvil_strike(void) {
    float sp = vlen(P.vel);
    V3 eye = eye_pos(), f = view_dir();
    V3 r = v3(cosf(P.yaw), 0, sinf(P.yaw)), u = vcross(r, f);
    int hits = 0, parried = 0;

    melee_start_swing();
    parried = try_parry();
    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &E[i];
        if (!e->alive) continue;
        float rel = rel_speed(e->vel);
        V3 d = vsub(e->pos, eye);
        float dist = vlen(d), reach = dist - enemy_radius(e->type);
        if (reach > anvil_range(rel)) continue;
        if (reach > 0.8f && vdot(vmul(d, 1.0f / dist), f) < ANVIL_CONE) continue;
        if (e->type == EN_RUSHER) {                       /* kamikaze pare : explose dans sa face */
            V3 at = e->pos;
            damage_enemy(i, 9999, vmul(f, 10));
            parry_feedback(at);
            melee_parry_points(PTS_PARRY);
            parried++;
        } else {
            damage_enemy(i, anvil_dmg(rel), vadd(vmul(f, ANVIL_KNOCK), v3(0, ANVIL_KNOCK_UP, 0)));
            if (!e->alive) { P.hp = fminf(100, P.hp + 15); ammo_points(PTS_MELEE_KILL, AP_KILL); }
        }
        hits++;
    }
    for (int i = 0; i < MAXP; i++) {                     /* LAN : les autres joueurs */
        if (!np_target(i)) continue;
        float rel = rel_speed(np_vel(i));
        V3 c = np_center(i), d = vsub(c, eye);
        float dist = vlen(d), reach = dist - 0.5f;
        if (reach > anvil_range(rel)) continue;
        if (reach > 0.8f && vdot(vmul(d, 1.0f / dist), f) < ANVIL_CONE) continue;
        net_hit(i, anvil_dmg(rel) * PVP_SCALE, vadd(vmul(f, ANVIL_KNOCK), v3(0, ANVIL_KNOCK_UP, 0)), W_SLAM);
        burst(c, v3(1, 0.7f, 0.35f), 24, 9, 0.35f, 0.14f);
        hits++;
    }
    float range = anvil_range(sp);
    for (float a = -0.6f; a <= 0.6f; a += 0.1f) {        /* arc vertical : la tete tombe de haut en bas */
        V3 p = vadd(eye, vadd(vmul(f, range * 0.6f + 0.5f), vadd(vmul(u, -a * range * 0.5f), vmul(r, a * 0.15f))));
        add_part(p, vadd(vmul(f, 2), v3(0, -4, 0)), v3(1, 0.75f, 0.45f), 0.18f, 0.09f + sp * 0.002f, 0);
    }
    if (dmMode) net_fx_melee(eye, f, sp);                 /* EV_MELEE : le recepteur lit meleeKind dans pad */
    P.meleeCd = ANVIL_CD;
    P.meleeT = ANVIL_SWING_T;
    P.parryWin = ANVIL_PARRY_WINDOW;
    P.swingLanded = hits || parried;
    play(S_DASH, 0.45f, 0.8f);                            /* souffle plus grave que l'epee */
    if (hits) {
        melee_hit_points(meleePts[P.meleeKind]);
        P.dashCharge = fminf(2, P.dashCharge + 1);
        hitMarker = 0.18f;
        shake += 0.25f;
        if (!parried) hitstop = fmaxf(hitstop, 0.06f + sp * 0.002f);
        play(S_ANVIL_CLANG, 0.9f, frange(0.9f, 1.05f));
    }
}

static void slam_start(void) {
    if (P.hook != HOOK_NONE) hook_release(0);
    P.wallrun = P.sliding = 0;
    P.anvilState = ANVIL_DIVE; P.anvilStartY = P.pos.y;
    P.vel.y = fminf(P.vel.y, -SLAM_VY);
    P.fovKick += 6; shake += 0.1f;
    slamDives++;
    play(S_ANVIL_DIVE, 0.8f, 1.0f);
    if (botMode) netlog("enclume : pique depuis %.1f m", anvil_ground_dist(P.pos));
}

/* appui sur la melee avec l'Enclume : au sol (ou trop pres du sol) coup lourd, sinon pique */
static void anvil_press(void) {
    if (P.anvilState != ANVIL_IDLE) return;
    if (P.grounded || anvil_ground_dist(P.pos) < SLAM_MIN_H) anvil_strike();
    else slam_start();
}

/* onde de choc (calquee sur explode, mais au sol et sans traverser les murs).
 * directP / directE : cible d'un contact direct en l'air (-1 = onde au sol) : elle seule prend tout. */
static void slam_resolve(float vyImpact, int directP, int directE) {
    float vy = fminf(fabsf(vyImpact), SLAM_VY_MAX), H = P.anvilStartY - P.pos.y;
    float R = slam_radius(vy), D = slam_dmg(vy, H);
    V3 c = P.pos, eyeC = vadd(c, v3(0, 0.5f, 0));
    int hits = 0, nt = 0, tgt[MAXP];
    float tdmg[MAXP];
    V3 tkn[MAXP];

    melee_start_swing();                                  /* une onde = un coup (points de jauge) */
    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &E[i];
        float er = enemy_radius(e->type), f = 1;
        if (!e->alive || (directE >= 0 && i != directE) || directP >= 0) continue;
        if (directE < 0) {
            if (e->pos.y - er - c.y > SLAM_REACH_UP || e->pos.y + er < c.y - SLAM_REACH_DOWN) continue;
            float d = hlen(v3(e->pos.x - c.x, 0, e->pos.z - c.z)), Rc = R + er;
            if (d >= Rc || !los(eyeC, e->pos)) continue;
            f = 1 - d / Rc;
        }
        V3 out = vnorm(v3(e->pos.x - c.x, 0.001f, e->pos.z - c.z));
        float dmg = D * (0.3f + 0.7f * f);
        damage_enemy(i, dmg, v3(out.x * SLAM_KNOCK_OUT * f, SLAM_KNOCK_UP, out.z * SLAM_KNOCK_OUT * f));
        if (!e->alive && e->type == EN_DRONE) ammo_points(PTS_MELEE_KILL, AP_KILL);
        slamDmgSum += dmg;
        hits++;
    }
    for (int i = 0; i < MAXP; i++) {                     /* LAN : les touches partent apres le rebond (voir plus bas) */
        V3 d = NP[i].disp;
        float f = 1;
        if (!np_target(i) || (directP >= 0 && i != directP) || directE >= 0) continue;
        if (directP < 0) {
            if (d.y - c.y > SLAM_REACH_UP || d.y < c.y - SLAM_REACH_DOWN) continue;
            float dh = hlen(v3(d.x - c.x, 0, d.z - c.z)), Rc = R + 0.5f;
            if (dh >= Rc || !los(eyeC, np_center(i))) continue;
            f = 1 - dh / Rc;
        }
        V3 out = vnorm(v3(d.x - c.x, 0.001f, d.z - c.z));
        tgt[nt] = i; tdmg[nt] = D * (0.3f + 0.7f * f) * PVP_SCALE;
        tkn[nt] = v3(out.x * SLAM_KNOCK_OUT * f, SLAM_KNOCK_UP, out.z * SLAM_KNOCK_OUT * f);
        slamDmgSum += tdmg[nt];
        nt++; hits++;
    }

    /* effets locaux */
    for (int k = 0; k < 36; k++) {
        float a = k * (2 * PI / 36);
        add_part(vadd(c, v3(0, 0.15f, 0)), v3(cosf(a) * R * 2.2f, 0.6f, sinf(a) * R * 2.2f), v3(1, 0.7f, 0.35f), 0.4f, 0.22f, 0);
    }
    burst(c, v3(0.75f, 0.72f, 0.7f), 40, 6, 0.6f, 0.2f);
    shards(c, v3(0.55f, 0.52f, 0.5f), 8 + (int)(vy / 4), 9);
    shake += 0.3f + vy * 0.01f; P.landDip = 0.3f;
    play(S_ANVIL_SLAM, clampf(vy / 40, 0.5f, 1), 1.1f - vy * 0.005f);
    if (dmMode) net_fx_slam(c, R, vy, hits);
    P.anvilLastVy = vy; P.anvilLandT = 0; P.meleeCd = fmaxf(P.meleeCd, SLAM_CD);
    slamWaves++; slamVySum += vy;
    if (hits) {
        slamWaveHits++;
        P.vel.y = SLAM_BOUNCE; P.grounded = 0; P.airJumps = 1;   /* vitesse horizontale conservee */
        P.dashCharge = fminf(2, P.dashCharge + 1);
        melee_hit_points(meleePts[P.meleeKind]);         /* une fois par onde */
        hitMarker = 0.18f; play(S_HIT, 0.7f, 0.8f);
        hitstop = fmaxf(hitstop, fminf(0.12f, 0.05f + vy * 0.0015f));
        if (vy > 35) impact_frame(c, 0.07f);
        P.anvilState = ANVIL_IDLE;
    } else {
        P.anvilState = ANVIL_STUN; P.anvilStunT = SLAM_STUN; P.vel.x = P.vel.z = 0;
        slamStuns++;
    }
    if (botMode) netlog("enclume : onde |vy| %.1f, rayon %.1f, degats %.0f, %d touche(s)", vy, R, D, hits);
    /* le rebond est decide chez le frappeur avant l'arbitrage : chez l'hote, une onde paree
     * (EV_MELEE_PARRY, arbitre tout de suite) ecrase donc bien le rebond et rend les points */
    for (int k = 0; k < nt; k++) net_hit(tgt[k], tdmg[k], tkn[k], W_SLAM);
}
static void slam_land(float vyImpact) { slam_resolve(vyImpact, -1, -1); }

/* contact direct pendant le pique avec une cible hors d'atteinte de l'onde (en l'air) : elle prend tout */
static void slam_contact(void) {
    V3 mn, mx, omn, omx;
    paabb(P.pos, pheight(), &mn, &mx);
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i)) continue;
        np_aabb(i, 0, &omn, &omx);
        if (mn.x < omx.x && mx.x > omn.x && mn.y < omx.y && mx.y > omn.y && mn.z < omx.z && mx.z > omn.z &&
            anvil_ground_dist(NP[i].disp) > SLAM_REACH_UP) { slam_resolve(-P.vel.y, i, -1); return; }
    }
    for (int j = 0; j < MAXE; j++) {
        if (!E[j].alive) continue;
        float er = enemy_radius(E[j].type);
        V3 q = v3(clampf(E[j].pos.x, mn.x, mx.x), clampf(E[j].pos.y, mn.y, mx.y), clampf(E[j].pos.z, mn.z, mx.z));
        if (vlen(vsub(q, E[j].pos)) < er && anvil_ground_dist(v3(E[j].pos.x, E[j].pos.y - er, E[j].pos.z)) > SLAM_REACH_UP) {
            slam_resolve(-P.vel.y, -1, j);
            return;
        }
    }
}

/* piques des autres joueurs : trainee, et sifflement au debut */
static void anvil_remote_tick(void) {
    for (int i = 0; i < MAXP; i++) {
        unsigned char pad = NP[i].st.pad;
        if (!np_target(i)) { NP[i].anvilPrevPad = 0; continue; }
        if (pad & ST_SLAM) {
            V3 p = NP[i].disp;
            for (int k = 0; k < 2; k++)
                add_part(vadd(p, v3(frange(-.3f, .3f), frange(0.2f, 1.6f), frange(-.3f, .3f))), v3(frange(-1, 1), 6, frange(-1, 1)),
                         v3(1, 0.6f, 0.25f), 0.35f, 0.14f, 0);
            if (!(NP[i].anvilPrevPad & ST_SLAM)) play(S_ANVIL_DIVE, 0.8f * atten(p), 1.0f);
        }
        NP[i].anvilPrevPad = pad;
    }
}

static void melee(void) {
    if (P.meleeKind == MK_ANVIL) { anvil_press(); return; }
    float sp = vlen(P.vel);
    V3 eye = eye_pos(), f = view_dir();
    V3 r = v3(cosf(P.yaw), 0, sinf(P.yaw)), u = vcross(r, f);
    int hits = 0, parried = 0;

    melee_start_swing();
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
            melee_parry_points(PTS_PARRY);                /* compte comme un parry, sans bonus de kill */
            parried++;
        } else {
            damage_enemy(i, melee_dmg(rel), vmul(f, 8 + rel * 0.5f));
            if (!e->alive) {
                P.hp = fminf(100, P.hp + 15);             /* bonus de soin des kills au corps a corps */
                ammo_points(PTS_MELEE_KILL, AP_KILL);
            }
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
        melee_hit_points(meleePts[P.meleeKind]);          /* la melee remplit les jauges des armes a distance */
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

/* trajet hitscan perforant (rail, Elan) : piece (le tir s'y arrete), ennemis, joueurs LAN ; renvoie 1 si touche.
 * dmg : degats PvE (deja multiplies par la chaine), knockE / knockP : recul PvE / PvP, w : W_* des touches LAN */
static int hitscan_shot(float dmg, float knockE, float knockP, int w, V3 *endOut, V3 *nOut) {
    V3 eye = eye_pos(), d = view_dir(), n;
    float tw = ray_world(eye, d, 400, &n);
    int hit = 0, coinHit = -1;
    for (int i = 0; i < MAXCOIN; i++) {                   /* une de mes pieces sur la trajectoire ? le tir s'y arrete */
        float t;
        if (CO[i].alive && ray_sphere(eye, d, CO[i].pos, COIN_RADIUS, tw, &t)) { tw = t; coinHit = i; }
    }
    for (int i = 0; i < MAXE; i++) {
        float t;
        if (E[i].alive && ray_sphere(eye, d, E[i].pos, enemy_radius(E[i].type), tw, &t)) {
            burst(vadd(eye, vmul(d, t)), v3(1, 1, 1), 15, 8, 0.3f, 0.12f);
            damage_enemy(i, dmg, vmul(d, knockE));
            hit = 1;
        }
    }
    for (int i = 0; i < MAXP; i++) {                     /* LAN : le tir traverse aussi les joueurs (un net_hit chacun) */
        V3 mn, mx; float t;
        if (!np_target(i)) continue;
        np_aabb(i, 0.05f, &mn, &mx);
        if (ray_box(eye, d, mn, mx, tw, &t, NULL)) {
            burst(vadd(eye, vmul(d, t)), v3(1, 1, 1), 15, 8, 0.3f, 0.12f);
            net_hit(i, dmg * PVP_SCALE, vmul(d, knockP), w);
            hit = 1;
        }
    }
    if (coinHit >= 0) {
        hit |= ricochet(coinHit, dmg, 0);
        if (botMode) netlog("ricochet : chaine de %d, %s", lastChain, hit ? "touche" : "dans le vide");
    }
    *endOut = vadd(eye, vmul(d, tw)); *nOut = n;
    return hit;
}

static void fire_rail(void) {
    if (P.ammo[WP_RAIL] > 0) P.ammo[WP_RAIL]--;
    V3 end, n;
    int hit = hitscan_shot(RAIL_DMG * streak_mult(), 8, 4, W_RAIL, &end, &n);
    shot_result(hit);
    if (botMode) netlog("tir rail : %s", hit ? "touche" : "rate");
    V3 m = muzzle();
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

/* --- Elan (convertisseur cinetique) ------------------------------------ */
/* Le tir convertit la vitesse horizontale (15 m/s au plus) en degats, et le tireur la perd.
 * Dash-tir : dans les 0,10 s apres un dash, toute la vitesse compte ; ensuite et jusqu'a 0,6 s,
 * la vitesse ajoutee par le dash ne compte plus (on prend la vitesse d'avant le dash). */
static int kin_in_window(void) { return P.kinDashT <= KIN_DASH_WINDOW + TICK * 0.5f; }   /* 12 ticks */
static float kin_vdep(void) {
    float v = fminf(hlen(P.vel), KIN_SPEED_CAP);
    if (!kin_in_window() && P.kinDashT < KIN_DASH_FORGET) v = fminf(v, P.kinPreDash);   /* trop tard */
    return v;
}
static float kin_dmg(float v) { return KIN_DMG_BASE + KIN_DMG_PER_MS * v; }
static V3 kin_col(float e) { return vlerp(v3(0.45f, 0.45f, 0.55f), v3(1.0f, 0.3f, 0.9f), clampf(e, 0, 1)); }

/* retire v a la norme horizontale, direction conservee, vel.y intact */
static void kin_brake(float v) {
    float hs = hlen(P.vel);
    if (hs < 1e-4f) return;
    float k = fmaxf(0, hs - v) / hs;
    P.vel.x *= k; P.vel.z *= k;
}

/* rayon de l'Elan (local et evenement reseau) : trait gris -> magenta, spirale et etincelles selon l'energie */
static void kin_beam_fx(V3 a, V3 b, V3 n, float e) {
    V3 col = kin_col(e);
    add_beam(a, b, col, 0.25f + 0.55f * e);
    if (e >= KIN_FULL / KIN_SPEED_CAP) add_beam(a, b, v3(1, 1, 1), 0.15f);   /* tir plein : rayon double */
    V3 bd = vsub(b, a); float bl = vlen(bd);
    if (e >= 0.15f && bl > 0.01f) {                       /* spirale facon Quake, plus large et plus dense avec l'energie */
        bd = vmul(bd, 1.0f / bl);
        V3 u = vnorm(vcross(bd, v3(0, 1, 0.01f))), w = vcross(bd, u);
        float rad = 0.06f + 0.14f * e, step = 0.6f - 0.3f * e;
        for (float s = 0; s < bl && s < 120; s += step) {
            float ang = s * 2.2f;
            V3 off = vadd(vmul(u, cosf(ang) * rad), vmul(w, sinf(ang) * rad));
            add_part(vadd(vadd(a, vmul(bd, s)), off), vmul(off, 4), col, 0.6f, 0.06f, 0);
        }
    }
    float sp = 3 + 5 * e;
    for (int i = 0; i < (int)(6 + 20 * e); i++)
        add_part(b, vadd(vmul(n, sp), vmul(rand_dir(), sp)), col, 0.45f, 0.1f, 12);
}

static void fire_kinetic(void) {
    if (P.ammo[WP_KINETIC] > 0) P.ammo[WP_KINETIC]--;
    int dashShot = kin_in_window();                       /* pour le HUD et le journal */
    float v = kin_vdep(), e = v / KIN_SPEED_CAP, mult = streak_mult();
    float dmg = kin_dmg(v) * mult, kn = KIN_KNOCK + KIN_KNOCK_PER_MS * v;
    V3 h0 = v3(P.vel.x, 0, P.vel.z), end, n, m = muzzle();
    int hit = hitscan_shot(dmg, kn, kn * 0.5f, W_KINETIC, &end, &n);
    kin_brake(v);                                         /* le cout, touche ou non */
    P.kinPreDash = fmaxf(0, P.kinPreDash - v);            /* la vitesse d'avant le dash est depensee */
    shot_result(hit);
    if (botMode) netlog("tir elan : v=%.1f degats=%.0f %s%s", v, dmg, hit ? "touche" : "rate",
                        P.kinDashT < KIN_DASH_FORGET ? (dashShot ? " (dash-tir)" : " (dash trop tot)") : "");
    kin_beam_fx(m, end, n, e);
    if (dmMode) net_fx_kinetic(m, end, e);
    for (int i = 0; i < (int)(6 + 18 * e); i++)           /* ton elan continue sans toi */
        add_part(m, vadd(vmul(h0, frange(0.5f, 0.9f)), vmul(rand_dir(), 2)), kin_col(e), 0.4f, 0.08f, 0);
    P.kinE = e; P.kinT = 0.3f;
    P.fireCd = KIN_CD / streak_rate();
    P.recoil = 0.6f + 0.8f * e; shake += 0.04f + 0.12f * e; P.fovKick -= 8 * e;
    muzzleT = 0.07f;
    play(S_KIN, 0.45f + 0.45f * e, 0.85f + 0.3f * e);
    if (hit) {
        hitMarker = 0.18f;
        play(S_HIT, 0.6f, 1.0f + 0.05f * (shotStreak < STREAK_MAX ? shotStreak : STREAK_MAX));
        if (v >= KIN_FULL) { hitstop = fmaxf(hitstop, 0.05f); impact_frame(end, 0.05f); }
        if (dashShot) { snprintf(popText, sizeof popText, "DASH-TIR"); popT = 1.0f; }
    }
}

/* degats du prochain tir d'Elan (PvP si LAN, plafonnes), *e = energie 0..1 */
static float kin_preview(float *e) {
    float v = kin_vdep(), d = kin_dmg(v) * streak_mult();
    *e = v / KIN_SPEED_CAP;
    return dmMode ? fminf(d * PVP_SCALE, PVP_HIT_CAP) : d;
}
/* --- fin Elan ----------------------------------------------------------- */

/* --- Eventail (fusil a pompe) ------------------------------------------- */
/* 10 plombs a motif deterministe (generateur local : le rng global n'est jamais touche, chaque client
 * recalcule les memes directions a partir de la graine d'EV_SHOT). Les degats sont agreges par cible :
 * un seul net_hit par joueur et par tir. Recul en l'air plafonne, dispersion divisee par 2 en slide,
 * un plomb qui touche une piece la fait eclater (souffle de 3 m, ni degats ni recul sur soi). */
static unsigned int sgCounter;                            /* compteur de graines */
static int sgBursts;                                      /* pieces eclatees au dernier tir (tests, journal) */
static int sgShots, sgPelletSum, sgStreakShots, sgJumps, sgBurstSum;   /* journal du bot */

static float sg_rand(unsigned int *s) { *s = *s * 1664525u + 1013904223u; return (*s >> 8) * (1.0f / 16777216.0f); }
static unsigned int sg_new_seed(void) {                   /* unique, deterministe, sans toucher rng */
    return (((unsigned int)myId + 1) << 24) ^ ((++sgCounter) * 2654435761u);
}
/* motif semi-fixe : 1 plomb au centre, 3 sur un anneau a 0.45, 6 sur un anneau a 0.9, tourne par la graine */
static void sg_pellet_dirs(V3 d, float half, unsigned int seed, V3 *out) {
    unsigned int s = seed;
    V3 r = vnorm(vcross(d, v3(0, 1, 0.01f))), u = vcross(r, d);   /* meme astuce que fire_rail : jamais nul */
    float rot = sg_rand(&s) * 2 * PI, th = tanf(half);
    for (int k = 0; k < SG_PELLETS; k++) {
        float rho, ang;
        if (k == 0) { rho = 0; ang = 0; }
        else if (k < 4) { rho = 0.45f; ang = rot + (k - 1) * (2 * PI / 3); }
        else { rho = 0.9f; ang = rot + PI / 6 + (k - 4) * (PI / 3); }
        rho += (sg_rand(&s) - 0.5f) * 0.2f;               /* +-0.1 */
        ang += (sg_rand(&s) - 0.5f) * 0.35f;              /* +-10 deg */
        if (rho > 1) rho = 1;
        out[k] = vnorm(vadd(d, vadd(vmul(r, cosf(ang) * rho * th), vmul(u, sinf(ang) * rho * th))));
    }
}
static float sg_falloff(float t) {                        /* plein jusqu'a 10 m, puis lineaire jusqu'a x0.3 a 30 m */
    if (t <= SG_NEAR) return 1;
    if (t >= SG_FAR) return SG_FAR_MULT;
    return 1 - (1 - SG_FAR_MULT) * (t - SG_NEAR) / (SG_FAR - SG_NEAR);
}
static V3 sg_tint(float k) { return vlerp(v3(1, 0.6f, 0.25f), v3(1, 0.9f, 0.4f), clampf(k, 0, 1)); }   /* cuivre -> or */

/* cumul d'un tir : degats, recul, plombs et dernier point touche par cible */
typedef struct {
    float e[MAXE], p[MAXP];
    V3 ke[MAXE], kp[MAXP], he[MAXE], hp[MAXP];
    int ne[MAXE], np[MAXP], burstHit;
} SgAcc;

/* eclat de piece : mine = 1 chez le tireur (impact frame, hitstop), 0 pour celui d'un autre joueur */
static void sg_burst_fx(V3 o, int mine) {
    burst(o, v3(1, 0.82f, 0.3f), 40, 10, 0.6f, 0.18f);
    shards(o, v3(1, 0.82f, 0.3f), 6, 7);
    play(S_EXPLO, 0.6f * (mine ? 1 : atten(o)), 1.7f);
    if (mine) { impact_frame(o, 0.05f); hitstop = fmaxf(hitstop, 0.04f); }
}

static void sg_coin_burst(int ci, int depth, SgAcc *A) {
    Coin *c = &CO[ci];
    if (!c->alive) return;                                /* deja eclatee par un autre plomb */
    V3 o = c->pos;
    c->alive = 0; sgBursts++;
    if (dmMode) net_fx_coin(1, c->netId, o, v3(1, 0, 0));   /* f[3] = 1 : detruite EN ECLAT */
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive) continue;
        V3 dv = vsub(E[i].pos, o);
        float dd = vlen(dv), R = SG_BURST_RADIUS + enemy_radius(E[i].type);
        if (dd < R) {
            float f = 1 - dd / R;
            A->e[i] += SG_BURST_DMG * (0.3f + 0.7f * f);
            A->ke[i] = vadd(A->ke[i], vmul(vnorm(dv), SG_BURST_KNOCK * f)); A->burstHit = 1;
        }
    }
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i)) continue;
        V3 dv = vsub(np_center(i), o);
        float dd = vlen(dv), R = SG_BURST_RADIUS + 0.5f;
        if (dd < R) {
            float f = 1 - dd / R;
            A->p[i] += SG_BURST_DMG * (0.3f + 0.7f * f);
            A->kp[i] = vadd(A->kp[i], vmul(vnorm(dv), SG_BURST_KNOCK * f + 2)); A->burstHit = 1;
        }
    }
    /* soi : ni degats ni recul (comme explode byPlayer == 2) */
    sg_burst_fx(o, 1);
    for (int j = 0; depth < SG_BURST_CHAIN && j < MAXCOIN; j++)   /* reaction en chaine */
        if (CO[j].alive && vlen(vsub(CO[j].pos, o)) < SG_BURST_RADIUS && los(o, CO[j].pos))
            sg_coin_burst(j, depth + 1, A);
}

/* shotgun jump : 7 m/s a l'oppose de la visee ; peut freiner, ou pousser jusqu'a un seuil, jamais au-dela
 * de la vitesse qu'on avait deja (9 m/s vertical, 16 m/s horizontal) */
static void sg_recoil(V3 d) {
    V3 v0 = P.vel, v = vadd(P.vel, vmul(d, -SG_RECOIL));
    if (v.y > v0.y && v.y > fmaxf(v0.y, SG_RECOIL_VMAX)) v.y = fmaxf(v0.y, SG_RECOIL_VMAX);
    float h0 = hlen(v0), h = hlen(v), lim = fmaxf(h0, SG_RECOIL_HCAP);
    if (h > lim) { v.x *= lim / h; v.z *= lim / h; }
    if (d.y < -0.7f) sgJumps++;                           /* stat : shotgun jump */
    P.vel = v;
}

static void fire_shotgun(void) {
    if (P.ammo[WP_SHOTGUN] > 0) P.ammo[WP_SHOTGUN]--;
    V3 eye = eye_pos(), d = view_dir(), dir[SG_PELLETS], end[SG_PELLETS], nrm[SG_PELLETS];
    float half = SG_HALF_ANGLE * (P.sliding ? SG_SLIDE_SPREAD : 1), mult = streak_mult();
    unsigned int seed = sg_new_seed();
    static SgAcc A;
    int pellets = 0, burstList[SG_PELLETS], nBurst = 0, onWall[SG_PELLETS];
    memset(&A, 0, sizeof A);
    sg_pellet_dirs(d, half, seed, dir);
    for (int k = 0; k < SG_PELLETS; k++) {                /* le plomb s'arrete sur le plus proche : piece, drone ou joueur */
        float tw = ray_world(eye, dir[k], SG_RANGE, &nrm[k]), t;
        int ci = -1, te = -1, tp = -1;
        onWall[k] = tw < SG_RANGE;
        for (int i = 0; i < MAXCOIN; i++)                 /* toute piece, la mienne ou non */
            if (CO[i].alive && ray_sphere(eye, dir[k], CO[i].pos, SG_COIN_RADIUS, tw, &t)) { tw = t; ci = i; }
        for (int i = 0; i < MAXE; i++)
            if (E[i].alive && ray_sphere(eye, dir[k], E[i].pos, enemy_radius(E[i].type), tw, &t)) { tw = t; te = i; ci = -1; }
        for (int i = 0; i < MAXP; i++) {
            V3 mn, mx;
            if (!np_target(i)) continue;
            np_aabb(i, 0.05f, &mn, &mx);
            if (ray_box(eye, dir[k], mn, mx, tw, &t, NULL) && t < tw) { tw = t; tp = i; te = ci = -1; }
        }
        end[k] = vadd(eye, vmul(dir[k], tw));
        float dmg = SG_PELLET_DMG * sg_falloff(tw) * mult;
        if (te >= 0) {
            A.e[te] += dmg; A.ke[te] = vadd(A.ke[te], vmul(dir[k], SG_PELLET_KNOCK)); A.ne[te]++; A.he[te] = end[k];
            pellets++; onWall[k] = 0;
        } else if (tp >= 0) {
            A.p[tp] += dmg; A.kp[tp] = vadd(A.kp[tp], vmul(dir[k], SG_PELLET_KNOCK)); A.np[tp]++; A.hp[tp] = end[k];
            pellets++; onWall[k] = 0;
        } else if (ci >= 0) { burstList[nBurst++] = ci; onWall[k] = 0; }   /* eclate apres la boucle (une fois par piece) */
    }
    sgBursts = 0;
    for (int b = 0; b < nBurst; b++) sg_coin_burst(burstList[b], 0, &A);
    int juice = 0; V3 juiceAt = eye;
    for (int i = 0; i < MAXE; i++) {                      /* un seul chiffre de degats par cible */
        if (A.e[i] <= 0) continue;
        V3 at = A.ne[i] ? A.he[i] : E[i].pos;
        if (A.ne[i] >= 8 && vlen(vsub(E[i].pos, eye)) < 6) { juice = 1; juiceAt = at; }
        burst(at, v3(1, 0.45f, 0.2f), 12, 7, 0.3f, 0.1f);
        damage_enemy(i, A.e[i], A.ke[i]);
    }
    for (int i = 0; i < MAXP; i++) {                      /* AGREGATION : un seul net_hit par joueur et par tir */
        if (A.p[i] <= 0) continue;
        V3 at = A.np[i] ? A.hp[i] : np_center(i);
        if (A.np[i] >= 8 && vlen(vsub(np_center(i), eye)) < 6) { juice = 1; juiceAt = at; }
        burst(at, v3(1, 0.45f, 0.2f), 12, 7, 0.3f, 0.1f);
        net_hit(i, A.p[i] * PVP_SCALE, vadd(A.kp[i], v3(0, 2, 0)), W_SHOTGUN);
    }
    int hit = pellets >= SG_STREAK_MIN || A.burstHit;
    shot_result(hit);
    sgShots++; sgPelletSum += pellets; sgStreakShots += hit; sgBurstSum += sgBursts;
    if (botMode) netlog("tir pompe : %d plomb(s), %d eclat(s), %s", pellets, sgBursts, hit ? "touche" : "rate");
    if (!P.grounded) sg_recoil(d);

    V3 m = muzzle(), col = sg_tint(shotStreak / (float)STREAK_MAX);   /* effets locaux : traceurs, etincelles, son */
    for (int k = 0; k < SG_PELLETS; k++) {
        add_tracer(m, end[k], col);
        if (onWall[k])
            for (int j = 0; j < 3; j++)
                add_part(end[k], vadd(vmul(nrm[k], 3), vmul(rand_dir(), 2)), v3(1, 0.75f, 0.4f), 0.35f, 0.07f, 12);
    }
    if (dmMode) net_fx_shot(eye, d, half, seed);
    P.fireCd = P.sgPumpMax = P.sgPumpT = SG_CD / streak_rate();
    P.recoil = 1.6f; shake += 0.1f + 0.012f * pellets; muzzleT = 0.09f;
    P.sgLastPellets = pellets; P.sgLastT = pellets ? 0.5f : 0;
    play(S_SG_FIRE, 0.85f, frange(0.96f, 1.04f));
    if (pellets || A.burstHit) { hitMarker = 0.18f; play(S_HIT, 0.6f, 0.9f + 0.03f * pellets); }
    if (juice) { hitstop = fmaxf(hitstop, 0.05f); impact_frame(juiceAt, 0.05f); }
    if (sgBursts) {
        if (sgBursts > 1) snprintf(popText, sizeof popText, "ECLAT x%d", sgBursts);
        else snprintf(popText, sizeof popText, "ECLAT !");
        popT = 1.0f;
    }
}
/* --- fin Eventail ------------------------------------------------------- */

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
    if (P.ammo[WP_ROCKET] > 0) P.ammo[WP_ROCKET]--;      /* limitees : un rocket jump coute une roquette */
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
    loadout_apply();
    anvil_reset();
    P.coins = COIN_MAX;
    memset(CO, 0, sizeof CO);
    for (int i = 0; i < npu; i++) PU[i].t = PU[i].pending = 0;
    memset(E, 0, sizeof E); memset(PR, 0, sizeof PR); memset(PA, 0, sizeof PA); memset(BM, 0, sizeof BM);
    memset(TR, 0, sizeof TR);
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
#define NET_VER   7
enum { PK_QUERY = 1, PK_ANNOUNCE, PK_JOIN, PK_WELCOME, PK_STATE, PK_SNAP, PK_ROSTER, PK_EVENT, PK_LEAVE,
       PK_PUNCH, PK_KEEP };   /* PUNCH : ouvre le NAT vers un client ; KEEP : un spectateur dit qu'il est la */
enum { EV_RAIL = 1, EV_ROCKET, EV_ROCKET_END, EV_PARRY, EV_HIT, EV_MELEE, EV_KILL, EV_RESPAWN, EV_MATCH, EV_INFO, EV_PICKUP,
       EV_PICKUP_TAKEN, EV_COIN, EV_MELEE_PARRY,
       EV_SHOT = 15,                                      /* Eventail : un tir = un evenement (graine des plombs) */
       EV_SLAM = 21 };                                    /* identifiants figes des armes : PLAN.md (15 a 20 reserves) */

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
    s->melee = (unsigned char)(clampf(P.meleeT / (P.meleeKind == MK_ANVIL ? ANVIL_SWING_T : 0.25f), 0, 1) * 255);
    s->pad = (unsigned char)(P.meleeKind & 0x0F);      /* bits 4-7 : etats d'arme (ST_*), ajoutes par chaque arme */
    if (P.anvilState == ANVIL_DIVE) s->pad |= ST_SLAM;
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
static void net_fx_kinetic(V3 a, V3 b, float e) {          /* Elan : EV_RAIL sous-type 1, actor = energie x 255 */
    NetEvent ev = ev_new(EV_RAIL);
    vf(ev.f, a); vf(ev.f + 3, b); ev.f[6] = 0;
    ev.sub = 1; ev.actor = (unsigned char)(clampf(e, 0, 1) * 255);
    net_emit(&ev);
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
static void net_fx_shot(V3 eye, V3 dir, float half, unsigned int seed) {   /* Eventail : chaque client recalcule les plombs */
    NetEvent e = ev_new(EV_SHOT);
    vf(e.f, eye); vf(e.f + 3, dir); e.f[6] = half;        /* demi-angle : encode le slide */
    e.netId = seed; e.sub = SG_PELLETS;
    e.actor = (unsigned char)(shotStreak < STREAK_MAX ? shotStreak : STREAK_MAX);   /* teinte des traceurs */
    net_emit(&e);
}
static void net_fx_slam(V3 pos, float radius, float vy, int hits) {   /* onde de l'Enclume : purement visuelle */
    NetEvent e = ev_new(EV_SLAM);
    vf(e.f, pos); e.f[3] = radius; e.f[4] = vy; e.sub = (unsigned char)(hits < 255 ? hits : 255);
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
    if (who == myId) {                                    /* chaque arme du loadout recoit les points de la cellule */
        float pts = (float)PU[i].pts;
        for (int s = 0; s < 2; s++) { float lost = gauge_add(P.loadout[s], pts); apStat[AP_CELL] += pts - lost; apStat[AP_LOST] += lost; }
        snprintf(popText, sizeof popText, "CELLULE +%d", PU[i].pts);
        popT = 1.0f;
        ammoGainT = 0.8f; ammoGainPts = PU[i].pts; ammoGainSlot = 2;
        cellsTaken++;
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
        if (ev->sub == 1) {                               /* rayon d'Elan */
            float e = ev->actor / 255.0f;
            kin_beam_fx(a, b, v3(0, 1, 0), e);
            play(S_KIN, (0.45f + 0.45f * e) * atten(a), 0.85f + 0.3f * e);
            break;
        }
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
        if (ev->from < MAXP && (NP[ev->from].st.pad & 15) == MK_ANVIL) {   /* coup au sol de l'Enclume : plus lourd */
            for (int k = 0; k < 12; k++)
                add_part(vadd(a, vmul(b, 1.5f + k * 0.15f)), v3(b.x * 2, -4, b.z * 2), v3(1, 0.75f, 0.45f), 0.18f, 0.1f, 0);
            play(S_DASH, 0.45f * atten(a), 0.8f);
            break;
        }
        for (int k = 0; k < 10; k++) add_part(vadd(a, vmul(b, 1.5f + k * 0.15f)), vmul(b, 3), v3(0.9f, 0.9f, 1), 0.15f, 0.08f, 0);
        play(S_DASH, 0.4f * atten(a), 1.8f);
        break;
    case EV_SHOT: {                                       /* Eventail d'un autre joueur : memes plombs, traceurs seulement */
        V3 dir[SG_PELLETS], n;
        sg_pellet_dirs(b, ev->f[6], ev->netId, dir);
        for (int k = 0; k < SG_PELLETS && k < ev->sub; k++) {
            float tw = ray_world(a, dir[k], SG_RANGE, &n), t;
            V3 mn, mx;
            for (int i = 0; i < MAXP; i++) {              /* arret visuel sur les corps affiches */
                if (i == ev->from || !NP[i].active || !NP[i].alive) continue;
                if (i == myId) { if (P.dead) continue; paabb(P.pos, pheight(), &mn, &mx); }
                else np_aabb(i, 0.05f, &mn, &mx);
                if (ray_box(a, dir[k], mn, mx, tw, &t, NULL) && t < tw) tw = t;
            }
            add_tracer(vadd(a, vmul(b, 0.7f)), vadd(a, vmul(dir[k], tw)), sg_tint(ev->actor / (float)STREAK_MAX));
        }
        if (ev->from < MAXP) NP[ev->from].sgFireT = 0.12f;
        play(S_SG_FIRE, 0.75f * atten(a), 1.0f);
        break;
    }
    case EV_SLAM: {                                       /* onde de choc d'un autre joueur : effets seulement */
        float R = ev->f[3], vy = ev->f[4];
        for (int k = 0; k < 36; k++) {
            float ang = k * (2 * PI / 36);
            add_part(vadd(a, v3(0, 0.15f, 0)), v3(cosf(ang) * R * 2.2f, 0.6f, sinf(ang) * R * 2.2f), v3(1, 0.7f, 0.35f), 0.4f, 0.22f, 0);
        }
        burst(a, v3(0.75f, 0.72f, 0.7f), 30, 6, 0.6f, 0.2f);
        shards(a, v3(0.55f, 0.52f, 0.5f), 6 + (int)(vy / 5), 9);
        play(S_ANVIL_SLAM, clampf(vy / 40, 0.5f, 1) * atten(a), 1.1f - vy * 0.005f);
        if (!spectating && vlen(vsub(P.pos, a)) < R + 3) shake += 0.15f;
        break;
    }
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
            if (is_melee_w(ev->sub)) ammo_points(PTS_MELEE_KILL, AP_KILL);   /* kill en melee : +100 points */
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
            loadout_apply();
            anvil_reset();
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
        } else if (ev->sub == 1 && ev->f[3] == 1) {       /* Eventail : piece eclatee par un plomb */
            for (int i = 0; i < MAXCOIN; i++)
                if (CO[i].alive && CO[i].netId == ev->netId) CO[i].alive = 0;
            sg_burst_fx(a, 0);
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
        int wave = (int)ev->f[0] == W_SLAM && att == myId && P.anvilLandT < 0.5f;   /* c'est mon onde qui a ete paree */
        if (def >= MAXP || att >= MAXP) break;
        V3 pd = def == myId ? pcenter() : np_center(def), pa = att == myId ? pcenter() : np_center(att);
        V3 mid = vlerp(pd, pa, 0.5f);
        for (int k = 0; k < 30; k++) add_part(mid, vmul(rand_dir(), 10), v3(1, 0.95f, 0.7f), 0.35f, 0.1f, 0);
        play(S_PUNCH, 0.8f * atten(mid), 1.6f);
        if (def == myId) {
            parry_feedback(mid);
            ammo_points(PTS_PARRY, AP_PARRY);             /* decision de l'hote : souvent apres la fin du coup */
            snprintf(popText, sizeof popText, clash ? "CHOC D'EPEES !" : "PARRY !");
            if (clash) P.vel = vadd(P.vel, vadd(vmul(vnorm(vsub(pd, pa)), 8), v3(0, 3, 0)));
        }
        if (att == myId) {                                /* l'attaquant est repousse, epee bloquee */
            if (is_melee_w((int)ev->f[0]) && nowT - P.undoT < REFUND_UNDO_T) {   /* le coup ne compte plus : on rend ses points */
                gauge_remove(P.loadout[0], P.undoPts[0]); gauge_remove(P.loadout[1], P.undoPts[1]);
                P.undoPts[0] = P.undoPts[1] = 0;
            }
            if (wave) {                                   /* Enclume : onde paree, renvoye en l'air, marteau bloque */
                P.anvilState = ANVIL_IDLE; P.anvilStunT = 0;
                P.vel = vadd(v3(P.vel.x, 0, P.vel.z), vadd(vmul(vnorm(vsub(pa, pd)), 6), v3(0, SLAM_PARRIED_UP, 0)));
                P.meleeCd = fmaxf(P.meleeCd, MELEE_PARRIED_CD);
            } else {
                P.vel = vadd(P.vel, vadd(vmul(vnorm(vsub(pa, pd)), clash ? 8.0f : 11.0f), v3(0, 3, 0)));
                P.meleeCd = fmaxf(P.meleeCd, clash ? MELEE_CD : MELEE_PARRIED_CD);
            }
            P.grounded = 0;
            hitstop = fmaxf(hitstop, 0.1f);
            shake += 0.3f;
            snprintf(popText, sizeof popText, clash ? "CHOC D'EPEES !" : wave ? "ONDE PAREE PAR %s" : "PARE PAR %s", pname(def));
            popT = 1.2f;
        }
        netlog("%s : %s -> %s", clash ? "choc d'epees" : (int)ev->f[0] == W_SLAM ? "enclume paree" : "epee paree", pname(att), pname(def));
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

static int hitCount[W_COUNT], parryCount;                       /* statistiques pour le journal du bot */
static double lifeFirstHitT[MAXP], ttkSum;                /* temps pour tuer : du premier degat (a pleine vie) au kill */
static int ttkN, oneShots, meleeParries, clashes;
static int slamParried, slamRailCombos;                   /* Enclume : ondes parees, onde puis rail sur la meme cible */
static double slamLastHitT[MAXP];
static int slamLastBy[MAXP];

/* tolerance de latence de l'hote pour un parry, selon l'arme qui frappe (et la melee de la victime) */
static double parry_grace(int w, int victimMK) {
    (void)victimMK;                                       /* les Poings (0,05 s) s'ajouteront ici */
    if (w == W_SLAM) return SLAM_PARRY_GRACE;             /* l'onde de l'Enclume : 0,20 s */
    return MELEE_PARRY_GRACE;
}

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
        if (ev->sub >= W_COUNT) return 1;
        hitCount[ev->sub]++;
        netlog("degats : %s -> %s, %.0f (arme %d), PV avant %.0f", pname(ev->from), pname(v), ev->f[0], ev->sub, NP[v < MAXP ? v : 0].hp);
        if (v >= MAXP || !NP[v].active || !NP[v].alive || matchOver) return 1;
        /* coup de melee ou impact de grappin pendant la fenetre de parry de la victime : c'est pare */
        if (is_parryable_w(ev->sub) && nowT - NP[v].lastParryT < parry_grace(ev->sub, NP[v].st.pad & 15)) {
            int clash = is_melee_w(ev->sub) && ev->from < MAXP && nowT - NP[ev->from].lastParryT < MELEE_PARRY_GRACE;
            if (ev->sub == W_SLAM) slamParried++;
            NetEvent e = ev_new(EV_MELEE_PARRY);
            e.actor = (unsigned char)v; e.target = ev->from; e.sub = (unsigned char)clash;
            e.f[0] = (float)ev->sub;                      /* arme paree : l'attaquant rend ses points de melee */
            NP[v].hp = fminf(100, NP[v].hp + PARRY_HEAL);
            if (clash) clashes++; else meleeParries++;
            net_emit(&e);
            apply_event(&e);
            return 1;
        }
        float before = NP[v].hp;
        float dmg = ev->sub == W_SELF ? ev->f[0] : fminf(ev->f[0], PVP_HIT_CAP);   /* plafond par touche */
        if (before >= 99.9f) lifeFirstHitT[v] = nowT;
        if (ev->sub == W_SLAM) { slamLastHitT[v] = nowT; slamLastBy[v] = ev->from; }   /* onde puis rail : le combo vise */
        else if (ev->sub == W_RAIL && slamLastHitT[v] > 0 && slamLastBy[v] == ev->from && nowT - slamLastHitT[v] < 1.5) {
            slamRailCombos++; slamLastHitT[v] = 0;
        }
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
static float bot_melee_reach(float rel) { return P.meleeKind == MK_ANVIL ? anvil_range(rel) : melee_range(rel); }

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

    /* Enclume adverse : un pique tombe pres de nous. Juste avant l'impact, une fois sur deux on pare l'onde ;
     * sinon on saute (un saut simple passe au-dessus de l'onde) ou, deja en l'air, on sort du rayon par un dash */
    static double slamSeenT[MAXP];
    static int slamPlan[MAXP];                            /* 1 = parer, 2 = esquiver, 3 = esquive en cours */
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i) || !(NP[i].st.pad & ST_SLAM)) continue;
        V3 d = NP[i].disp, away = v3(P.pos.x - d.x, 0, P.pos.z - d.z);
        if (hlen(away) > 6) continue;
        float t = (d.y - P.pos.y) / fmaxf(-NP[i].st.vel[1], SLAM_VY);
        if (nowT - slamSeenT[i] > 1.5) { slamSeenT[i] = nowT; slamPlan[i] = frand() < 0.5f ? 1 : 2; }
        if (slamPlan[i] == 1 && t < 0.12f && P.meleeCd <= 0 &&
            (P.meleeKind != MK_ANVIL || P.grounded || anvil_ground_dist(P.pos) < SLAM_MIN_H)) {
            botPress[A_MELEE] = 1; slamPlan[i] = 0;
            netlog("parry de l'onde tente contre %s", pname(i));
            return;
        }
        if (slamPlan[i] >= 2 && t < 0.35f) {
            if (slamPlan[i] == 2) { slamPlan[i] = 3; netlog("esquive de l'onde de %s (%.2f s avant l'impact)", pname(i), t); }
            botHeld[A_JUMP] = 1;
            if (P.grounded || (P.airJumps > 0 && P.vel.y < 1)) botPress[A_JUMP] = 1;   /* saut, puis double saut au sommet */
            else if (P.vel.y < 0 && P.dashCharge >= 1) {
                P.yaw = atan2f(away.x, -away.z); P.pitch = 0;
                botHeld[A_FWD] = 1; botPress[A_DASH] = 1;
            }
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

    /* une arme a sec et personne de proche : on va chercher une cellule (les grandes d'abord) */
    int seek = -1;
    if ((!wp_has_ammo(P.loadout[0]) || !wp_has_ammo(P.loadout[1])) && (best < 0 || bd > 25)) {
        float sd = 90;
        for (int i = 0; i < npu; i++) {
            if (PU[i].t > 0 || PU[i].pos.y - P.pos.y > 3) continue;   /* seulement celles a portee de saut */
            float d = vlen(vsub(PU[i].pos, P.pos)) - (PU[i].pts == PICKUP_BIG ? 8 : 0);
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

    /* Enclume : assez haut au-dessus d'une cible proche du sol, on pique si l'impact prevu tombe sur elle */
    if (best >= 0 && P.meleeKind == MK_ANVIL && P.anvilState == ANVIL_IDLE && P.meleeCd <= 0 && !P.grounded &&
        anvil_ground_dist(P.pos) >= 2.5f && anvil_ground_dist(NP[best].disp) < SLAM_REACH_UP) {
        V3 at; float vy, R, t;
        if (slam_predict(&at, &vy, &R, &t)) {
            V3 tp = vadd(NP[best].disp, v3(NP[best].st.vel[0] * t, 0, NP[best].st.vel[2] * t));
            float miss = hlen(vsub(at, tp));
            if (miss < 0.5f * R) {
                botPress[A_MELEE] = 1;
                crossPhase = 0; crossCd = 2;
                netlog("pique sur %s, %.1f m", pname(best), miss);
                return;
            }
        }
    }

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
            if (bd < bot_melee_reach(rel_speed(np_vel(best))) + 0.5f && P.meleeCd <= 0) botPress[A_MELEE] = 1;
            if (crossT > 0.5f) {
                crossPhase = 0; crossCd = 3;
                netlog("cross-up : retourne, tir sur %s", pname(best));
            }
        }
        if (crossPhase) { botHeld[A_HOOK] = 0; return; }
    } else crossPhase = 0;

    /* coince contre un obstacle : on part en biais, puis de l'autre cote, puis demi-tour */
    if (hs < 3 && P.hook == HOOK_NONE && P.kinT <= 0) stuckT += dt; else stuckT = 0;   /* un tir d'Elan arrete net : pas coince */
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

    /* le bot alterne ses deux armes (la 2e seulement de pres), et prend l'autre si l'une est a sec */
    int la = P.loadout[0], lb = P.loadout[1];
    int pref = (bd < 20 && ((int)(botT / 5 + botMode)) % 2) ? lb : la;
    if (!wp_has_ammo(pref)) pref = pref == la ? lb : la;
    if (la == WP_SHOTGUN || lb == WP_SHOTGUN) {           /* Eventail : seulement de pres (3 touches jusqu'a ~11 m) */
        int o = la == WP_SHOTGUN ? lb : la;
        if (wp_has_ammo(WP_SHOTGUN) && (bd < 10 || !wp_has_ammo(o))) pref = WP_SHOTGUN;
        else if (wp_has_ammo(o)) pref = o;
    }
    if (P.meleeKind == MK_ANVIL && P.anvilLandT < 1.0f && P.swingHitPts && wp_has_ammo(WP_RAIL) && (la == WP_RAIL || lb == WP_RAIL))
        pref = WP_RAIL;                                   /* onde qui touche : la cible vole ~1 s, le rail l'acheve */
    P.weapon = pref;
    int dry = !wp_has_ammo(la) && !wp_has_ammo(lb);    /* a sec : il ne tire plus et va au contact (la melee recharge) */
    /* contre-jeu : une piece adverse a portee ? on tire dedans pour la retourner contre son lanceur
     * (seulement avec les 2 rails : on ne gaspille pas le dernier) */
    if (P.weapon == WP_RAIL && P.ammo[WP_RAIL] >= 2 && P.fireCd <= 0 && P.switchT <= 0) {
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
    if (!coinPlan && see && bd > 10 && bd < 55 && hs > 11 && P.coins >= 1 && P.ammo[WP_RAIL] > 0 && P.weapon == WP_RAIL &&
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
    if (!dry && !coinPlan && see && seeT > 0.3f && bd < 55 && P.fireCd <= 0 && P.switchT <= 0 &&
        P.weapon != WP_KINETIC &&                         /* recale la visee pour tirer (l'Elan a sa regle plus bas) */
        (P.weapon != WP_SHOTGUN || bd < (P.sliding ? 13.0f : 11.0f))) {   /* Eventail : au-dela, moins de 4 plombs */
        P.yaw = aimYaw + aimErrYaw; P.pitch = aimPitch + aimErrPitch; botHeld[A_FIRE] = 1;
    }

    if (best >= 0 && bd > (dry ? 4 : 14) && P.dashCharge >= 1 && botT > botNextDash) {    /* dash vers la cible */
        memset(botHeld, 0, sizeof botHeld);
        botHeld[A_FWD] = botHeld[A_JUMP] = 1;
        P.yaw = aimYaw;
        botPress[A_DASH] = 1;
        botNextDash = botT + 1.2f;
    }

    /* Elan : tire quand sa vitesse suffit ; sinon dash-tir, fenetre reussie une fois sur deux
     * (sinon il tire 0,15 a 0,3 s apres le dash, comme un humain en retard). Le bhop et le grappin relancent. */
    static float kinLateT = -1;                           /* tir en retard programme (fenetre ratee) */
    if (P.weapon != WP_KINETIC || dry) kinLateT = -1;
    else if (see && seeT > 0.3f && P.switchT <= 0 && bd < 55) {
        float v = kin_vdep(), need = bd > 20 ? 14.0f : 10.5f;   /* de loin : tir plein ; de pres : 2 touches */
        if (kinLateT >= 0 && botT >= kinLateT && P.fireCd <= 0) {
            P.yaw = aimYaw + aimErrYaw; P.pitch = aimPitch + aimErrPitch; botHeld[A_FIRE] = 1; kinLateT = -1;
        } else if (P.fireCd <= 0 && v >= need && kinLateT < 0) {
            P.yaw = aimYaw + aimErrYaw; P.pitch = aimPitch + aimErrPitch; botHeld[A_FIRE] = 1;
        } else if (P.fireCd <= 0 && kinLateT < 0 && (botPress[A_DASH] || (P.dashCharge >= 1 && botT > botNextDash))) {
            if (!botPress[A_DASH]) { botPress[A_DASH] = 1; botHeld[A_FWD] = 1; botNextDash = botT + 1.2f; }
            P.yaw = aimYaw + aimErrYaw; P.pitch = aimPitch + aimErrPitch;   /* le dash part vers la cible */
            if (frand() < 0.5f) botHeld[A_FIRE] = 1;      /* dash-tir dans le meme tick : fenetre reussie */
            else kinLateT = botT + frange(0.15f, 0.3f);   /* fenetre ratee : tir en retard */
        }
    }
    if (see && bd > (dry ? 6 : 10) && bd < GRAPPLE_RANGE && P.hook == HOOK_NONE && P.hookCd <= 0 && botT > botNextHook) {
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
    /* Eventail : au sol et rapide pres d'une cible, il glisse pour tirer en slide (dispersion / 2) */
    if (P.weapon == WP_SHOTGUN && best >= 0 && bd < 13 && (P.sliding || (P.grounded && hs > SLIDE_MIN))) {
        botHeld[A_CROUCH] = 1; botHeld[A_JUMP] = 0;
    }
    /* Eventail : shotgun jump vers une recharge en hauteur (une fois par sejour en l'air, jamais les 2 dernieres cartouches) */
    static int sgJumpDone;
    if (P.grounded) sgJumpDone = 0;
    if (seekHigh && !sgJumpDone && !P.grounded && P.hook == HOOK_NONE && P.vel.y < 1 && P.fireCd <= 0 &&
        (la == WP_SHOTGUN || lb == WP_SHOTGUN) && P.ammo[WP_SHOTGUN] >= 3) {
        P.weapon = WP_SHOTGUN; P.switchT = 0; P.pitch = -1.45f;
        botHeld[A_FIRE] = 1; sgJumpDone = 1;
        netlog("shotgun jump vers la cellule %d", seek);
    }
    if (best >= 0 && bd < bot_melee_reach(rel_speed(np_vel(best))) + 0.5f && P.meleeCd <= 0) { P.yaw = aimYaw; botPress[A_MELEE] = 1; }
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
        if (P.dead || PU[i].t > 0 || loadout_full() || PU[i].pending > 0) continue;
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
    anvil_remote_tick();                                  /* trainee des piques */
    if (matchOver) matchEndT -= dt;
    fx_tick(dt);
}

/* tir de l'arme en main (chaque fire_* retire lui-meme sa munition) */
static void fire_weapon(void) {
    switch (P.weapon) {
    case WP_RAIL:   fire_rail(); break;
    case WP_ROCKET: fire_rocket(); break;
    case WP_KINETIC: fire_kinetic(); break;
    case WP_SHOTGUN: fire_shotgun(); break;
    }
}
/* chargeur vide : clic sec, puis l'autre arme si elle peut tirer, sinon on pousse au contact */
static void dry_fire(void) {
    play(S_HIT, 0.5f, 0.45f);
    P.fireCd = DRYFIRE_CD;
    int o = other_wp();
    if (wp_has_ammo(o)) { snprintf(popText, sizeof popText, "%s A SEC", ammoDef[P.weapon].name); weapon_select(o); }
    else { snprintf(popText, sizeof popText, "A SEC : VA AU CONTACT !"); dryHintT = 2; }
    popT = 1.0f;
}
static void cfg_save(void);
static void loadout_cycle(int slot) {                     /* ecran de mort : arme prete suivante, echange si doublon */
    int w = wantLoadout[slot];
    do w = (w + 1) % WP_COUNT; while (!wpReady[w]);
    if (w == wantLoadout[slot ^ 1]) wantLoadout[slot ^ 1] = wantLoadout[slot];
    wantLoadout[slot] = w;
    cfg_save();
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

    if (P.dead) {                                         /* ecran de mort : les touches d'arme changent le loadout */
        if (w1) loadout_cycle(0);
        if (w2) loadout_cycle(1);
        if (meleePress) { do wantMelee = (wantMelee + 1) % MK_COUNT; while (!mkReady[wantMelee]); cfg_save(); }
    } else {
        int nw = P.weapon;
        if (w1) nw = P.loadout[0];
        if (w2) nw = P.loadout[1];
        if (swap) nw = other_wp();
        weapon_select(nw);
    }

    player_tick(dt, jp, dp);

    if (P.coins < COIN_MAX) P.coins = fminf(COIN_MAX, P.coins + dt / COIN_RECHARGE);
    if (!P.dead && coinPress && P.coins >= 1) throw_coin();
    coin_tick(dt);

    P.meleeCd -= dt; P.meleeT -= dt;
    if (!P.dead && meleePress && P.meleeCd <= 0) melee();
    if (P.parryWin > 0) {
        if (!P.dead) try_parry();
        P.parryWin -= dt;
        if (P.parryWin <= 0 && !P.swingLanded) {          /* coup dans le vide : on paie le risque */
            int anvil = P.meleeKind == MK_ANVIL;
            P.meleeCd = fmaxf(P.meleeCd, anvil ? ANVIL_WHIFF_CD - ANVIL_PARRY_WINDOW : MELEE_WHIFF_CD - PARRY_WINDOW);
        }
    }
    if (P.anvilLandT < 10) P.anvilLandT += dt;            /* Enclume : sert a reconnaitre un parry de l'onde */
    anvil_remote_tick();

    P.fireCd -= dt; P.switchT -= dt;
    P.kinT -= dt;                                         /* Elan : decharge du viewmodel */
    {                                                     /* Eventail : clac de la pompe a 35 % de la recharge */
        float pv = P.sgPumpT;
        P.sgPumpT -= dt; P.sgLastT -= dt;
        if (P.weapon == WP_SHOTGUN && !P.dead && pv > 0.65f * P.sgPumpMax && P.sgPumpT <= 0.65f * P.sgPumpMax)
            play(S_SG_PUMP, 0.5f, 1);
    }
    P.recoil *= expf(-dt * 9);
    if (!P.dead && (act[A_FIRE] || lmbp) && P.fireCd <= 0 && P.switchT <= 0) {
        if (wp_has_ammo(P.weapon)) fire_weapon(); else dry_fire();
    }
    if (!P.dead && !wp_has_ammo(P.loadout[0]) && !wp_has_ammo(P.loadout[1])) dryT += dt;
    ammoGainT -= dt; dryHintT -= dt;

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
    for (int i = 0; i < MAXTR; i++) TR[i].t += dt;       /* Eventail : traceurs */
    for (int i = 0; i < nb; i++)                         /* particules des jump pads */
        if (boxes[i].kind == B_PAD && frand() < 0.15f) {
            V3 c = vlerp(boxes[i].mn, boxes[i].mx, 0.5f);
            add_part(v3(c.x + frange(-1.3f, 1.3f), 0.35f, c.z + frange(-1.3f, 1.3f)), v3(0, frange(3, 7), 0),
                     v3(0.2f, 0.8f, 1.0f), 0.8f, 0.08f, 0);
        }

    shard_tick(dt);
    for (int i = 0; i < MAXDN; i++) DN[i].t -= dt;
    for (int i = 0; i < MAXP; i++) { NP[i].flashT -= dt; NP[i].sgFireT -= dt; }
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

/* Enclume : manche le long de -Z, tete au bout ; la bande chauffe du bleu a l'orange (heat 0..1) */
static void hammer_model(float heat) {
    glColor3f(0.22f, 0.2f, 0.19f);
    box_faces(v3(-0.03f, -0.03f, -0.9f), v3(0.03f, 0.03f, 0.1f));
    glColor3f(0.5f, 0.52f, 0.56f);
    box_faces(v3(-0.17f, -0.11f, -1.12f), v3(0.17f, 0.11f, -0.88f));
    glDisable(GL_LIGHTING);
    glColor3f(0.35f + 0.65f * heat, 0.6f - 0.05f * heat, 1.0f - 0.85f * heat);
    box_faces(v3(-0.176f, -0.116f, -1.03f), v3(0.176f, 0.116f, -0.97f));
    glEnable(GL_LIGHTING);
}

/* viewmodel de l'Enclume : bas a gauche au repos, leve pendant le pique, abattu par le coup au sol */
static void draw_anvil_vm(void) {
    float ang = 20, drop = 0, shk = 0, heat = 0;
    if (P.anvilState == ANVIL_DIVE) {
        ang = 65; shk = sinf(gameTime * 60) * 0.02f;
        heat = clampf((-P.vel.y - 25) / 20, 0.2f, 1);
    } else if (P.anvilState == ANVIL_STUN) {
        ang = -35; drop = 0.3f * clampf(P.anvilStunT / 0.15f, 0, 1);
    } else if (P.meleeT > 0) {
        float k = clampf(1 - P.meleeT / ANVIL_SWING_T, 0, 1);
        ang = 100 - 135 * k * k;
        heat = clampf(vlen(P.vel) / 30, 0, 1) * 0.6f;
    }
    glPushMatrix();
    glTranslatef(-0.7f + shk, -0.25f - drop, -0.2f);
    glRotatef(ang, 1, 0, 0);
    glRotatef(12, 0, 0, 1);
    glScalef(0.6f, 0.6f, 0.6f);
    hammer_model(heat);
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
    if (P.meleeKind == MK_ANVIL) draw_anvil_vm();
    else if (P.meleeT > 0) {                              /* coup d'epee : balayage de droite a gauche */
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
        float tip = P.weapon == WP_RAIL || P.weapon == WP_SHOTGUN ? -0.64f : P.weapon == WP_KINETIC ? -0.57f : -0.6f;
        float ms2 = (0.12f + 0.25f * (muzzleT / 0.08f)) * (P.weapon == WP_SHOTGUN ? 1.5f : 1.0f);
        glDisable(GL_LIGHTING);
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texDot);
        if (P.weapon == WP_KINETIC) { V3 kc = kin_col(P.kinE); glColor4f(kc.x, kc.y, kc.z, 1); }   /* Elan : couleur du tir */
        else if (P.weapon == WP_SHOTGUN) glColor4f(1, 0.75f, 0.35f, 1);
        else glColor4f(0.5f + 0.5f * ammoDef[P.weapon].r, 0.5f + 0.5f * ammoDef[P.weapon].g, 0.5f + 0.5f * ammoDef[P.weapon].b, 1);
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
    if (P.weapon == WP_RAIL) {
        glColor3f(0.25f, 0.27f, 0.32f);
        box_faces(v3(-0.05f, -0.06f, -0.42f), v3(0.05f, 0.04f, 0.12f));
        glColor3f(0.18f, 0.19f, 0.22f);
        box_faces(v3(-0.025f, -0.02f, -0.62f), v3(0.025f, 0.03f, -0.42f));
        box_faces(v3(-0.03f, -0.16f, 0.0f), v3(0.03f, -0.06f, 0.07f));
        glDisable(GL_LIGHTING);
        glColor3f(0.3f * ready, 0.85f * ready, 1.0f * ready);
        box_faces(v3(-0.055f, -0.012f, -0.4f), v3(0.055f, 0.0f, 0.05f));
        box_faces(v3(-0.03f, 0.03f, -0.6f), v3(0.03f, 0.036f, -0.43f));
    } else if (P.weapon == WP_KINETIC) {                  /* Elan : corps trapu, trois anneaux = compteur de vitesse */
        float back = P.kinT > 0 ? P.kinT * 0.1f : 0;      /* decharge : recul vers l'arriere */
        glPushMatrix();
        glTranslatef(0, 0, back);
        glColor3f(0.3f, 0.26f, 0.32f);
        box_faces(v3(-0.06f, -0.06f, -0.3f), v3(0.06f, 0.06f, 0.1f));
        glColor3f(0.17f, 0.16f, 0.2f);
        box_faces(v3(-0.028f, -0.028f, -0.55f), v3(0.028f, 0.028f, -0.3f));
        box_faces(v3(-0.03f, -0.17f, -0.02f), v3(0.03f, -0.06f, 0.06f));
        glDisable(GL_LIGHTING);
        float e = kin_vdep() / KIN_SPEED_CAP;
        V3 c = kin_in_window() ? v3(1, 1, 1) : kin_col(e);   /* fenetre du dash-tir : anneaux blancs */
        float lit = P.kinT > 0 ? 0.15f : ready * (0.35f + 0.65f * e);   /* eteints pendant la decharge */
        if (kin_in_window() && P.kinT <= 0) lit = 1;
        for (int r = 0; r < 3; r++) {                     /* volant d'inertie : 3 anneaux autour du canon */
            float z = -0.36f - r * 0.065f, s = 0.05f + 0.006f * r;
            glColor3f(c.x * lit, c.y * lit, c.z * lit);
            box_faces(v3(-s, s - 0.012f, z - 0.018f), v3(s, s, z));
            box_faces(v3(-s, -s, z - 0.018f), v3(s, -s + 0.012f, z));
            box_faces(v3(-s, -s, z - 0.018f), v3(-s + 0.012f, s, z));
            box_faces(v3(s - 0.012f, -s, z - 0.018f), v3(s, s, z));
        }
        glColor3f(c.x * lit * 0.8f, c.y * lit * 0.8f, c.z * lit * 0.8f);
        box_faces(v3(-0.062f, 0.02f, -0.26f), v3(0.062f, 0.032f, 0.0f));
        glPopMatrix();
    } else if (P.weapon == WP_SHOTGUN) {                  /* Eventail : carcasse brune, deux canons, pompe qui recule */
        float k = P.sgPumpMax > 0 && P.sgPumpT > 0 ? 1 - P.sgPumpT / P.sgPumpMax : 1;
        float pz = 0.12f * sinf(PI * clampf((k - 0.35f) / 0.5f, 0, 1));
        glColor3f(0.30f, 0.24f, 0.20f);
        box_faces(v3(-0.06f, -0.06f, -0.30f), v3(0.06f, 0.05f, 0.10f));
        box_faces(v3(-0.03f, -0.17f, 0.0f), v3(0.03f, -0.06f, 0.07f));
        glColor3f(0.42f, 0.44f, 0.48f);                   /* canons, acier */
        box_faces(v3(-0.05f, -0.005f, -0.62f), v3(-0.005f, 0.04f, -0.30f));
        box_faces(v3(0.005f, -0.005f, -0.62f), v3(0.05f, 0.04f, -0.30f));
        glColor3f(0.22f, 0.17f, 0.14f);                   /* pompe */
        box_faces(v3(-0.045f, -0.06f, -0.52f + pz), v3(0.045f, -0.015f, -0.36f + pz));
        glDisable(GL_LIGHTING);
        glColor3f(1.0f * ready, 0.6f * ready, 0.25f * ready);   /* bande cuivre */
        box_faces(v3(-0.04f, 0.05f, -0.28f), v3(0.04f, 0.058f, 0.06f));
        for (int i = 0; i < 6; i++) {                     /* 6 temoins de cartouches ; le suivant se remplit avec la jauge */
            float z = -0.26f + i * 0.05f, fill = i < P.ammo[WP_SHOTGUN] ? 1 : i == P.ammo[WP_SHOTGUN] ? gauge_frac(WP_SHOTGUN) : 0;
            glColor3f(0.15f, 0.12f, 0.1f);
            box_faces(v3(-0.068f, -0.03f, z), v3(-0.06f, 0.006f, z + 0.036f));
            if (fill > 0) {
                glColor3f(1.0f, 0.55f + 0.3f * (fill >= 1), 0.2f);
                box_faces(v3(-0.07f, -0.03f, z + 0.004f), v3(-0.061f, -0.03f + 0.036f * fill, z + 0.032f));
            }
        }
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
    fprintf(f, "loadout=%d,%d" "\n" "melee=%d" "\n", wantLoadout[0], wantLoadout[1], wantMelee);
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
        else if (!strcmp(line, "loadout")) sscanf(v, "%d,%d", &wantLoadout[0], &wantLoadout[1]);
        else if (!strcmp(line, "melee")) wantMelee = atoi(v);
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
static GLfloat mvMat[16], prMat[16];
static V3 camEye;

static const float pcol[MAXP][3] = {
    { 0.3f, 0.8f, 1.0f }, { 1.0f, 0.45f, 0.3f }, { 0.5f, 1.0f, 0.4f }, { 1.0f, 0.85f, 0.3f },
    { 0.8f, 0.45f, 1.0f }, { 1.0f, 0.4f, 0.7f }, { 0.4f, 1.0f, 0.9f }, { 0.9f, 0.9f, 0.9f },
};
static const char *weaponNames[W_COUNT] = { "RAIL", "ROQUETTE", "EPEE", "GRAPPIN", "PARRY", "SOLO", "RICOCHET",
                                            "ENCLUME", "LANCE", "FLEAU", "POINGS", "EGIDE", "EVENTAIL", "ELAN",
                                            "SCIE", "VENTOUSE", "TESLA" };

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

/* bouton de choix : cadre de la couleur de l'arme si elle est choisie, grise si l'arme n'est pas encore codee */
static int sel_button(float x, float y, float w, float h, const char *label, int chosen, int ready, V3 col) {
    int hover = ready && uiMX >= x && uiMX < x + w && uiMY >= y && uiMY < y + h;
    if (chosen) rect(x - 3, y - 3, w + 6, h + 6, col.x, col.y, col.z, 1);
    rect(x, y, w, h, hover ? 0.2f : 0.08f, hover ? 0.42f : 0.1f, hover ? 0.62f : 0.18f, 0.95f);
    float k = ready ? 1.0f : 0.4f;
    text_c(0, x + w * 0.5f, y + h * 0.5f + 5, ready ? label : "BIENTOT", k, k, k, 1);
    if (hover && uiClick) { uiClick = 0; play(S_HIT, 0.4f, 1.5f); return 1; }
    return 0;
}

static void loadout_pick(int slot, int w) {               /* choisir pour un emplacement l'arme de l'autre les echange */
    if (w == wantLoadout[slot ^ 1]) wantLoadout[slot ^ 1] = wantLoadout[slot];
    wantLoadout[slot] = w;
}

static void draw_loadout(float cx, float cy) {
    char buf[128];
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.85f);
    text_c(2, cx, cy - 300, "LOADOUT", 0.5f, 0.9f, 1.0f, 1);
    text_c(0, cx, cy - 266, "1 arme de melee + 2 armes a distance ; la melee remplit les jauges de munitions",
           0.7f, 0.75f, 0.85f, 1);
    float bw = winW < 1300 ? 118.0f : 150.0f, step = bw + 10, x0 = cx - step * WP_COUNT * 0.5f;
    text(1, x0, cy - 215, "MELEE", 0.5f, 0.9f, 1.0f, 1);
    for (int m = 0; m < MK_COUNT; m++)
        if (sel_button(x0 + m * step, cy - 195, bw, 44, meleeNames[m], m == wantMelee, mkReady[m], v3(1, 0.85f, 0.4f)))
            wantMelee = m;
    for (int s = 0; s < 2; s++) {
        float y = cy - 70 + s * 112.0f;
        text(1, x0, y - 22, s ? "ARME 2" : "ARME 1", 0.5f, 0.9f, 1.0f, 1);
        for (int w = 0; w < WP_COUNT; w++)
            if (sel_button(x0 + w * step, y, bw, 44, ammoDef[w].name, w == wantLoadout[s], wpReady[w],
                           v3(ammoDef[w].r, ammoDef[w].g, ammoDef[w].b)))
                loadout_pick(s, w);
    }
    for (int s = 0; s < 2; s++) {
        const AmmoDef *d = &ammoDef[wantLoadout[s]];
        if (d->gauge <= 0) snprintf(buf, sizeof buf, "%s : objet unique, a recuperer", d->name);
        else snprintf(buf, sizeof buf, "%s : %d au spawn, %d max, %.0f pts par tir (%.1f coups d'epee)", d->name, d->start,
                      d->max, d->gauge, d->gauge / meleePts[MK_SWORD]);
        text(0, x0, cy + 120 + s * 24.0f, buf, 0.8f, 0.82f, 0.9f, 1);
    }
    snprintf(buf, sizeof buf, "%s : %d pts par coup qui touche, +%d par kill, +%d par parry ; cellules : %d ou %d pts",
             meleeNames[wantMelee], meleePts[wantMelee], PTS_MELEE_KILL, PTS_PARRY, PICKUP_SMALL, PICKUP_BIG);
    text(0, x0, cy + 168, buf, 1, 0.85f, 0.5f, 1);
    if (appState == APP_GAME) text(0, x0, cy + 194, "pris en compte au prochain respawn", 0.6f, 0.6f, 0.7f, 1);
    if (button(cx - 105, cy + 220, 210, 50, "VALIDER")) { loadoutOpen = 0; cfg_save(); }
    if (uiClick) uiClick = 0;
}

/* ecran de mort : les touches d'arme et de melee font defiler le loadout du prochain respawn */
static void draw_death_loadout(float cx, float y) {
    char buf[160];
    snprintf(buf, sizeof buf, "[%s] < %s >    [%s] < %s >    [%s] < %s >", bind_text(A_WEAP1), ammoDef[wantLoadout[0]].name,
             bind_text(A_WEAP2), ammoDef[wantLoadout[1]].name, bind_text(A_MELEE), meleeNames[wantMelee]);
    text_c(0, cx, y, "LOADOUT DU PROCHAIN RESPAWN", 0.5f, 0.9f, 1.0f, 1);
    text_c(1, cx, y + 32, buf, 1, 0.9f, 0.6f, 1);
}

static void draw_main_menu(float cx, float cy) {
    rect(0, 0, (float)winW, (float)winH, 0.02f, 0.02f, 0.06f, 0.7f);
    text_c(2, cx, cy - 300, "V E L O C I T Y", 0.5f, 0.9f, 1.0f, 1);
    text_c(0, cx, cy - 268, "movement shooter en C", 0.7f, 0.7f, 0.8f, 1);
    float bx = cx - 620, bw = 420, y = cy - 200;
    field(bx, y, bw, 48, "PSEUDO", myName, 1);
    y += 76;
    if (button(bx, y, bw, 52, "SOLO  -  VAGUES")) pendingAction = ACT_SOLO;
    y += 62;
    if (button(bx, y, bw, 52, "PARTIE RAPIDE  -  EN LIGNE")) pendingAction = ACT_QUICK;
    y += 62;
    if (button(bx, y, bw, 52, "HEBERGER UNE PARTIE")) pendingAction = ACT_HOST;
    y += 62;
    if (button(bx, y, bw, 52, "REJOINDRE / REGARDER")) pendingAction = ACT_BROWSE;
    y += 62;
    if (button(bx, y, bw, 52, "LOADOUT")) loadoutOpen = 1;
    y += 62;
    if (button(bx, y, bw, 52, "PARAMETRES")) settingsOpen = 1;
    y += 62;
    if (button(bx, y, bw, 52, "QUITTER")) pendingAction = ACT_QUIT;
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
    if (button(bx, y, bw, 54, "LOADOUT")) loadoutOpen = 1;
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
        else snprintf(buf, sizeof buf, "%s  [%s]  %s", pname(f->killer), weaponNames[f->kind % W_COUNT], pname(f->victim));
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
        if (n->st.pad & ST_SLAM) glRotatef(-15, 1, 0, 0);  /* pique de l'Enclume : penche vers l'avant */
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
        { const AmmoDef *wd = &ammoDef[n->st.weapon < WP_COUNT ? n->st.weapon : WP_RAIL]; glColor3f(wd->r * 0.9f, wd->g * 0.9f, wd->b * 0.9f); }
        if (n->st.weapon == WP_KINETIC) glColor3f(0.95f, 0.3f, 0.85f);   /* Elan : magenta, comme son rayon */
        if (n->st.weapon == WP_SHOTGUN) {                 /* Eventail : court, large, cuivre, recul au tir */
            glColor3f(0.85f, 0.5f, 0.22f);
            glTranslatef(0, 0, 0.15f * clampf(n->sgFireT / 0.12f, 0, 1));
            box_faces(v3(-0.08f, -0.07f, -0.55f), v3(0.08f, 0.07f, 0.05f));
        } else box_faces(v3(-0.06f, -0.06f, -0.7f), v3(0.06f, 0.06f, 0.05f));
        glPopMatrix();
        if ((n->st.pad & 15) == MK_ANVIL) {               /* Enclume : leve pendant le pique, abattu par le coup */
            int slam = (n->st.pad & ST_SLAM) != 0;
            float ang = 20;
            if (slam) ang = 150;
            else if (n->st.melee) { float k = 1 - n->st.melee / 255.0f; ang = 100 - 135 * k * k; }
            glPushMatrix();
            glTranslatef(-0.4f, h - 0.6f, -0.1f);
            glRotatef(ang, 1, 0, 0);
            hammer_model(slam ? 1.0f : 0.0f);
            glPopMatrix();
        } else if (n->st.melee) {                         /* epee en plein balayage */
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
        else if (loadoutOpen && appState == APP_MENU) draw_loadout(cx, cy);
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
    {
        char mt[64];
        int blink = dryHintT > 0 && fmodf(dryHintT, 0.4f) < 0.2f;   /* a sec : le panneau de melee clignote */
        if (dryHintT > 0) snprintf(mt, sizeof mt, "A SEC : MELEE = MUNITIONS");
        else snprintf(mt, sizeof mt, "%s%s  +%d pts", meleeNames[P.meleeKind], aimed ? " (vs cible)" : "", meleePts[P.meleeKind]);
        text(0, mx, by - 78.0f, mt, blink ? 1.0f : 0.75f, blink ? 0.6f : 0.75f, blink ? 0.2f : 0.85f, 0.9f);
    }
    float pvp = dmMode ? PVP_SCALE : 1.0f, mcd = MELEE_CD;
    if (P.meleeKind == MK_ANVIL) {                        /* Enclume : coup au sol, ou pique prevu en l'air */
        V3 at; float vy, R;
        int air = !P.grounded && (P.anvilState == ANVIL_DIVE || anvil_ground_dist(P.pos) >= SLAM_MIN_H);
        mcd = ANVIL_CD;
        if (air && slam_predict(&at, &vy, &R, NULL)) {
            float fall = (P.anvilState == ANVIL_DIVE ? P.anvilStartY : P.pos.y) - at.y;
            snprintf(buf, sizeof buf, "PIQUE %d dmg R %.1fm", (int)(slam_dmg(vy, fall) * pvp), R);
        } else snprintf(buf, sizeof buf, "%d dmg %.1fm", (int)(anvil_dmg(sp3) * pvp), anvil_range(sp3));
    } else snprintf(buf, sizeof buf, "%d dmg %.1fm", (int)(melee_dmg(sp3) * pvp), melee_range(sp3));
    text(1, mx, by - 48.0f, buf, 1, 1 - mk * 0.45f, 1 - mk * 0.85f, 1);
    rect(mx, by - 36.0f, 180, 6, 0.1f, 0.1f, 0.15f, 0.7f);
    rect(mx, by - 36.0f, 180 * clampf(1 - P.meleeCd / mcd, 0, 1), 6, 1, 0.55f + 0.45f * (1 - mk), 0.3f, 0.95f);

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

    if (P.anvilState == ANVIL_DIVE) text_c(0, cx, winH - 110.0f, "PIQUE !", 1, 0.6f, 0.25f, 0.95f);
    else if (P.anvilState == ANVIL_STUN) {                /* Enclume : onde ratee, barre d'immobilisation */
        text_c(0, cx, winH - 110.0f, "IMMOBILISE", 1, 0.35f, 0.3f, 0.95f);
        rect(cx - 60, winH - 102.0f, 120, 5, 0.1f, 0.1f, 0.15f, 0.7f);
        rect(cx - 60, winH - 102.0f, 120 * clampf(P.anvilStunT / SLAM_STUN, 0, 1), 5, 1, 0.35f, 0.3f, 0.95f);
    } else if (P.hook == HOOK_ATTACHED)
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
    const AmmoDef *ad = &ammoDef[P.weapon];
    if (P.weapon == WP_TESLA) snprintf(wnBuf, sizeof wnBuf, "TESLA %d%%", P.ammo[WP_TESLA]);
    else snprintf(wnBuf, sizeof wnBuf, "%s %d/%d%s", ad->name, P.ammo[P.weapon], ad->max,
                  P.weapon == WP_SHOTGUN && !P.ammo[WP_SHOTGUN] ? " - MELEE !" : "");   /* Eventail a sec */
    const char *wn = wnBuf;
    int dryW = !wp_has_ammo(P.weapon);
    text(1, winW - 30.0f - fontW[1] * (float)strlen(wn), winH - 60.0f, wn, dryW ? 1.0f : ad->r, dryW ? 0.3f : ad->g,
         dryW ? 0.3f : ad->b, 1);
    rect(winW - 230.0f, winH - 40.0f, 200, 8, 0.1f, 0.1f, 0.15f, 0.7f);
    {
        float cdMax = P.weapon == WP_SHOTGUN && P.sgPumpMax > 0 ? P.sgPumpMax : wpCd[P.weapon];   /* Eventail : recharge reelle */
        rect(winW - 230.0f, winH - 40.0f, 200 * clampf(1 - P.fireCd / cdMax, 0, 1), 8, 1, 1, 1, 0.8f);
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
    if (P.weapon == WP_SHOTGUN && !P.dead) {             /* Eventail : cercle de dispersion (moitie en slide), plombs touches */
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
    {                                                     /* emplacements : [touche] ARME munitions, l'actif en blanc */
        char sb[2][48];
        for (int s = 0; s < 2; s++)
            snprintf(sb[s], sizeof sb[s], "[%s] %s %d", bind_text(s ? A_WEAP2 : A_WEAP1), ammoDef[P.loadout[s]].name,
                     P.ammo[P.loadout[s]]);
        float x = winW - 30.0f - fontW[0] * (float)(strlen(sb[0]) + strlen(sb[1]) + 3);
        for (int s = 0; s < 2; s++) {
            int w = P.loadout[s], act0 = w == P.weapon, emp = !wp_has_ammo(w);
            float k = act0 ? 1.0f : 0.55f;
            text(0, x, winH - 13.0f, sb[s], emp ? 1.0f : k, emp ? 0.35f : k, emp ? 0.35f : k * 1.05f, 0.9f);
            float gw = fontW[0] * (float)strlen(sb[s]), gf = w == WP_TESLA ? P.ammo[w] / 100.0f : gauge_frac(w);
            if (ammoDef[w].gauge > 0) {
                rect(x, winH - 5.0f, gw, 3, 0.1f, 0.1f, 0.15f, 0.7f);
                rect(x, winH - 5.0f, gw * gf, 3, ammoDef[w].r, ammoDef[w].g, ammoDef[w].b, 0.9f);
            }
            x += gw + fontW[0] * 3;
        }
        if (wantLoadout[0] != P.loadout[0] || wantLoadout[1] != P.loadout[1] || wantMelee != P.meleeKind)
            text(0, winW - 30.0f - fontW[0] * 32, winH - 120.0f, "nouveau loadout au prochain respawn", 0.6f, 0.6f, 0.65f, 0.8f);
    }

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
        draw_death_loadout(cx, cy + 60);
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
        draw_death_loadout(cx, cy + 150);
    }

    if (!captured && !shotMode && !botMode) {
        if (settingsOpen) draw_settings(cx, cy); else if (loadoutOpen) draw_loadout(cx, cy); else draw_pause(cx, cy);
    }
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

/* Enclume : cercle de l'onde prevue au sol (passe additive). Orange si une cible est dedans. */
static void draw_slam_marker(void) {
    if (appState != APP_GAME || spectating || P.dead || P.meleeKind != MK_ANVIL || P.grounded) return;
    int diving = P.anvilState == ANVIL_DIVE;
    if (!diving && (P.anvilState != ANVIL_IDLE || anvil_ground_dist(P.pos) < SLAM_MIN_H)) return;
    V3 at; float vy, R;
    if (!slam_predict(&at, &vy, &R, NULL)) return;
    int hot = 0;
    for (int i = 0; i < MAXE && !hot; i++) {
        if (!E[i].alive) continue;
        float er = enemy_radius(E[i].type), feet = E[i].pos.y - er - at.y;
        hot = feet <= SLAM_REACH_UP && feet >= -SLAM_REACH_DOWN - 2 * er && hlen(vsub(E[i].pos, at)) < R + er;
    }
    for (int i = 0; i < MAXP && !hot; i++) {
        if (!np_target(i)) continue;
        float feet = NP[i].disp.y - at.y;
        hot = feet <= SLAM_REACH_UP && feet >= -SLAM_REACH_DOWN && hlen(vsub(NP[i].disp, at)) < R + 0.5f;
    }
    float a = diving ? 0.9f : 0.35f, pulse = 0.75f + 0.25f * sinf(gameTime * 14);
    glLineWidth(diving ? 4.0f : 2.0f);
    if (hot) glColor4f(1, 0.5f, 0.15f, a * pulse); else glColor4f(0.55f, 0.75f, 1, a * pulse);
    for (int ring = 0; ring < 2; ring++) {
        float rr = ring ? R * 0.25f : R;
        glBegin(GL_LINE_LOOP);
        for (int k = 0; k < 32; k++) {
            float ang = k * (2 * PI / 32);
            glVertex3f(at.x + cosf(ang) * rr, at.y + 0.06f, at.z + sinf(ang) * rr);
        }
        glEnd();
    }
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
    for (int i = 0; i < npu; i++) {                      /* cellules de munitions : les grandes sont plus grosses et violettes */
        if (PU[i].t > 0) continue;
        int big = PU[i].pts >= PICKUP_BIG;
        float k = big ? 1.4f : 1.0f;
        glPushMatrix();
        glTranslatef(PU[i].pos.x, PU[i].pos.y + 0.15f * sinf(gameTime * 3 + i), PU[i].pos.z);
        glRotatef(gameTime * 90 + i * 40, 0, 1, 0);
        glScalef(k, k, k);
        glColor3f(big ? 0.9f : 0.75f, big ? 0.8f : 0.95f, 1.0f);
        box_faces(v3(-0.22f, -0.35f, -0.22f), v3(0.22f, 0.35f, 0.22f));
        if (big) glColor3f(0.75f, 0.45f, 1.0f); else glColor3f(0.2f, 0.6f, 1.0f);
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
    for (int i = 0; i < MAXTR; i++) {                    /* Eventail : traceurs (tete de 2,5 m, puis trace qui s'eteint) */
        Tracer *r = &TR[i];
        if (r->t >= r->life) continue;
        V3 dv = vsub(r->b, r->a);
        float len = vlen(dv), travel = len / SG_TRACER_SPEED;
        if (len < 1e-3f) continue;
        dv = vmul(dv, 1.0f / len);
        if (r->t < travel) {
            float hd = r->t * SG_TRACER_SPEED;
            V3 h = vadd(r->a, vmul(dv, hd)), q = vadd(r->a, vmul(dv, fmaxf(0, hd - 2.5f)));
            glLineWidth(4); glColor4f(r->col.x, r->col.y, r->col.z, 0.3f);
            glBegin(GL_LINES); glVertex3f(q.x, q.y, q.z); glVertex3f(h.x, h.y, h.z); glEnd();
            glLineWidth(1.5f); glColor4f(1, 1, 1, 0.9f);
            glBegin(GL_LINES); glVertex3f(q.x, q.y, q.z); glVertex3f(h.x, h.y, h.z); glEnd();
        } else {
            float a = 0.15f * (1 - (r->t - travel) / 0.12f);
            glLineWidth(2); glColor4f(r->col.x, r->col.y, r->col.z, a);
            glBegin(GL_LINES); glVertex3f(r->a.x, r->a.y, r->a.z); glVertex3f(r->b.x, r->b.y, r->b.z); glEnd();
        }
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
        int big = PU[i].pts >= PICKUP_BIG;
        glColor4f(big ? 0.75f : 0.3f, big ? 0.45f : 0.8f, 1.0f, 0.55f + 0.2f * sinf(gameTime * 5 + i));
        billboard(PU[i].pos, big ? 1.9f : 1.3f);
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
    draw_slam_marker();
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
                else if (loadoutOpen) { loadoutOpen = 0; cfg_save(); }
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

/* tests de l'Enclume : pique depuis `from` (vitesse `vel`), ticks jusqu'a la fin du pique */
static void test_slam_drop(V3 from, V3 vel) {
    P.meleeKind = MK_ANVIL; P.anvilState = ANVIL_IDLE; P.meleeCd = 0;
    P.pos = P.prevPos = from; P.vel = vel; P.grounded = 0;
    anvil_press();
    for (int i = 0; i < 600 && P.anvilState == ANVIL_DIVE; i++) player_tick(TICK, 0, 0);
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

    /* loadout et munitions a jauge : spawn, conversion des points, surplus, maximum */
    reset_game();
    {
        int spawnOk = P.ammo[WP_RAIL] == 2 && P.ammo[WP_ROCKET] == 4 && P.ammoGauge[WP_RAIL] == 0 &&
                      P.weapon == WP_RAIL && P.meleeKind == MK_SWORD;
        P.weapon = WP_ROCKET; P.ammo[WP_ROCKET] = 0;
        ammo_points(50, AP_HIT);
        int conv1 = P.ammo[WP_ROCKET]; float g1 = P.ammoGauge[WP_ROCKET];
        ammo_points(70, AP_HIT);
        int conv2 = P.ammo[WP_ROCKET]; float g2 = P.ammoGauge[WP_ROCKET];
        P.weapon = WP_RAIL; P.ammo[WP_RAIL] = 2; P.ammo[WP_ROCKET] = 4; P.ammoGauge[WP_ROCKET] = 0;
        ammo_points(100, AP_HIT);                          /* rail plein : tout part aux roquettes */
        int spill = P.ammo[WP_ROCKET];
        P.weapon = WP_ROCKET; P.ammo[WP_ROCKET] = 7; P.ammoGauge[WP_ROCKET] = 30;
        float lostBefore = apStat[AP_LOST];
        ammo_points(100, AP_HIT);                          /* 30 + 100 : une roquette, puis le maximum */
        int atMax = P.ammo[WP_ROCKET]; float gMax = P.ammoGauge[WP_ROCKET], fMax = gauge_frac(WP_ROCKET);
        float lost = apStat[AP_LOST] - lostBefore;
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = PI; P.pitch = -0.5f;
        fire_rocket();                                     /* un rocket jump coute une roquette */
        fprintf(f, "jauges : spawn ok=%d, 50 pts -> %d (jauge %.0f), +70 -> %d (jauge %.0f), surplus -> roquettes %d, "
                   "maximum %d jauge %.0f affichee %.0f perdus %.0f, apres un tir %d (attendu 1, 1 0, 2 20, 6, 8 0 1 80, 7)\n",
                spawnOk, conv1, g1, conv2, g2, spill, atMax, gMax, fMax, lost, P.ammo[WP_ROCKET]);
    }

    /* l'epee remplit la jauge de l'arme en main : touche +50, kill +100 */
    reset_game();
    {
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        P.weapon = WP_ROCKET; P.ammo[WP_ROCKET] = 0;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.6f, 38);
        melee();
        int afterHit = P.ammo[WP_ROCKET];
        E[0].hp = 10;
        melee();
        fprintf(f, "epee -> munitions : touche %d roquette(s), touche + kill %d (attendu 1, 4)\n", afterHit, P.ammo[WP_ROCKET]);
    }

    /* cellules : deux tailles, points aux deux armes, refusee si tout est plein, reapparition */
    reset_game();
    {
        int cell = 6;                                      /* une petite cellule au sol, dans un coin */
        P.ammo[WP_RAIL] = 0; P.ammo[WP_ROCKET] = 2;
        P.pos = P.prevPos = v3(PU[cell].pos.x, 0, PU[cell].pos.z);
        pickup_tick(TICK);
        float railG = P.ammoGauge[WP_RAIL]; int rockets = P.ammo[WP_ROCKET], gone = PU[cell].t > 0;
        P.ammo[WP_RAIL] = 2; P.ammo[WP_ROCKET] = 8; PU[cell].t = 0;
        pickup_tick(TICK);
        int refused = PU[cell].t <= 0;
        PU[cell].t = PICKUP_RESPAWN;
        P.pos = P.prevPos = v3(0, 0, 30);
        for (int i = 0; i < (int)(PICKUP_RESPAWN / TICK) + 2; i++) pickup_tick(TICK);
        fprintf(f, "cellules : %d cellules (grandes %d %d), rail jauge 0 -> %.0f, roquettes 2 -> %d, prise=%d, pleine refusee=%d, "
                   "revenue apres 12 s=%d (attendu 13, 150 150, 100, 4, 1, 1, 1)\n",
                npu, PU[0].pts, PU[5].pts, railG, rockets, gone, refused, PU[cell].t <= 0);
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

    /* --- Elan : degats selon la vitesse depensee, freinage, chaine, piece, slide, munitions, fenetre du dash --- */
    {
        static const float vs[4] = { 0, 9.5f, 15, 25 };
        float dm[4], hl[4];
        for (int s = 0; s < 4; s++) {                      /* 1. tir droit devant a 0 / 9,5 / 15 / 25 m/s */
            reset_game();
            P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
            P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1;
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 30);
            P.vel = v3(0, 0, -vs[s]);
            fire_kinetic();
            dm[s] = 1000 - E[0].hp; hl[s] = hlen(P.vel);
        }
        fprintf(f, "elan a 0 / 9.5 / 15 / 25 m/s : degats %.1f / %.1f / %.1f / %.1f, vitesse apres %.1f / %.1f / %.1f / %.1f "
                   "(attendu 20 / 67.5 / 95 / 95, 0 / 0 / 0 / 10)\n", dm[0], dm[1], dm[2], dm[3], hl[0], hl[1], hl[2], hl[3]);

        reset_game();                                      /* 2. vitesse sur le cote et en montee */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 30);
        P.vel = v3(12, 6, 5);
        fire_kinetic();
        fprintf(f, "elan sur le cote a 13 m/s : degats %.1f, vitesse apres %.1f, vy %.1f (attendu 85, 0, 6)\n",
                1000 - E[0].hp, hlen(P.vel), P.vel.y);

        reset_game();                                      /* 3. tir rate : le freinage s'applique quand meme */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 1.2f;
        P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 30);
        P.vel = v3(0, 0, -15);
        fire_kinetic();
        fprintf(f, "elan rate a 15 m/s : vitesse apres %.1f, chaine %d (attendu 0, 0)\n", hlen(P.vel), shotStreak);

        reset_game();                                      /* 4. chaine : 3 tirs a 15 m/s */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1;
        for (int s = 0; s < 3; s++) {
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 30);
            P.vel = v3(0, 0, -15);
            fire_kinetic();
            dm[s] = 1000 - E[0].hp;
        }
        fprintf(f, "elan chaine a 15 m/s : degats %.2f / %.2f / %.2f (attendu 95 / 109.25 / 123.5)\n", dm[0], dm[1], dm[2]);

        reset_game();                                      /* 5. piece : ricochet x1,5 sur les degats convertis */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0.1f;
        P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1;
        P.vel = v3(0, 0, -20);
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-34, 3, 22);
        throw_coin();
        for (int i = 0; i < 30; i++) { coin_tick(TICK); P.pos = vadd(P.pos, vmul(P.vel, TICK)); }
        {
            int c0 = -1;
            for (int i = 0; i < MAXCOIN; i++) if (CO[i].alive && c0 < 0) c0 = i;
            V3 d = vsub(CO[c0].pos, eye_pos());
            P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
        }
        fire_kinetic();
        fprintf(f, "elan piece a 20 m/s : degats au drone %.1f, vitesse apres %.1f (attendu 142.5, 5)\n", 1000 - E[0].hp, hlen(P.vel));

        reset_game();                                      /* 6. slide : degats pleins, le tir arrete le slide */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        player_tick(TICK, 0, 0);                           /* pose au sol */
        P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 30);
        act[A_CROUCH] = 1; P.vel = v3(0, 0, -12);
        player_tick(TICK, 0, 0);
        int slid = P.sliding;
        fire_kinetic();
        player_tick(TICK, 0, 0);
        memset(act, 0, sizeof act);
        fprintf(f, "elan en slide : slide avant=%d, degats %.1f, slide apres=%d (attendu 1, 95, 0)\n", slid, 1000 - E[0].hp, P.sliding);

        reset_game();                                      /* 7. chargeur vide : clic sec, bascule, aucun freinage */
        P.pos = P.prevPos = v3(-20, 6, 40); P.yaw = 0; P.pitch = 0;
        P.loadout[0] = WP_KINETIC; P.loadout[1] = WP_RAIL; P.weapon = WP_KINETIC;
        P.ammo[WP_KINETIC] = 0; P.fireCd = 0; P.switchT = 0;
        P.vel = v3(0, 0, -15);
        botHeld[A_FIRE] = 1;
        game_tick(TICK);
        botHeld[A_FIRE] = 0;
        fprintf(f, "elan a sec : munitions %d, arme %s, vitesse %.1f (attendu 0, RAILGUN, 15)\n", P.ammo[WP_KINETIC],
                ammoDef[P.weapon].name, hlen(P.vel));

        /* 10 a 14. fenetre du dash-tir (en l'air, sans friction) */
        float wv[7];
        static const int pre[7] = { 0, 12, 13, 24, 73, 30, -1 };   /* ticks apres le dash ; -1 = cas 14 */
        for (int s = 0; s < 6; s++) {
            reset_game();
            memset(act, 0, sizeof act);
            P.pos = P.prevPos = v3(-20, 8, 40); P.yaw = 0; P.pitch = 0;
            P.dashCharge = 2; P.kinDashT = 1;
            if (s == 3) P.vel = v3(0, 0, -9.5f);
            player_tick(TICK, 0, 1);                       /* dash */
            for (int i = 0; i < pre[s]; i++) player_tick(TICK, 0, 0);
            if (s == 5) {                                  /* 14. deuxieme dash pendant le malus, puis 24 ticks */
                player_tick(TICK, 0, 1);
                for (int i = 0; i < 24; i++) player_tick(TICK, 0, 0);
            }
            wv[s] = kin_vdep();
            if (s == 3) wv[6] = hlen(P.vel);
        }
        fprintf(f, "dash-tir : vdep %.1f apres 0 tick (attendu 15)\n", wv[0]);
        fprintf(f, "dash-tir : vdep %.1f apres 12 ticks, %.1f apres 13 ticks (attendu 15, 0)\n", wv[1], wv[2]);
        fprintf(f, "dash-tir : vdep %.1f apres 24 ticks depuis 9.5 m/s, vitesse %.1f (attendu 9.5, 18)\n", wv[3], wv[6]);
        fprintf(f, "dash-tir : vdep %.1f apres 73 ticks (attendu 15)\n", wv[4]);
        fprintf(f, "dash-tir : vdep %.1f apres un 2e dash pendant le malus + 24 ticks (attendu 0)\n", wv[5]);

        reset_game();                                      /* 15. la melee remplit la jauge de l'Elan (80 pts) */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        P.loadout[0] = WP_KINETIC; P.loadout[1] = WP_RAIL; P.weapon = WP_KINETIC;
        P.ammo[WP_KINETIC] = 3; P.ammoGauge[WP_KINETIC] = 0;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 38);
        melee();
        P.meleeCd = 0; P.parryWin = 0; hitstop = 0;
        melee();
        fprintf(f, "elan jauge : 2 coups d'epee -> munitions %d, jauge %.0f (attendu 4, 20)\n", P.ammo[WP_KINETIC],
                P.ammoGauge[WP_KINETIC]);
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

    /* --- Enclume (solo) --- */
    {   /* 1. pique depuis 13 m, drone a 2 m du point d'impact : onde, rebond, dash, points de jauge */
        reset_game();
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.6f, 38);
        P.dashCharge = 0.5f;
        P.meleeKind = MK_ANVIL; P.pos = P.prevPos = v3(-20, 13, 40); P.grounded = 0; P.meleeCd = 0;
        anvil_press();
        float dashBefore = P.dashCharge;
        for (int i = 0; i < 600 && P.anvilState == ANVIL_DIVE; i++) { dashBefore = P.dashCharge; player_tick(TICK, 0, 0); }
        fprintf(f, "enclume pique 13 m : |vy| %.1f, PV drone %.1f, vy apres %.1f, dash %+.2f, roquettes %d jauge %.0f "
                   "(attendu ~39.0, ~36.8, 9.0, +1.00, 5 25)\n",
                P.anvilLastVy, E[0].hp, P.vel.y, P.dashCharge - dashBefore - TICK / DASH_RECHARGE,
                P.ammo[WP_ROCKET], P.ammoGauge[WP_ROCKET]);
    }
    {   /* 2. pique rate : immobilise 0,6 s (ni deplacement, ni saut, ni dash), puis libre */
        reset_game();
        test_slam_drop(v3(-20, 13, 40), v3(0, 0, -5));
        int st0 = P.anvilState; float hv = hlen(P.vel);
        V3 p0 = P.pos;
        act[A_FWD] = 1;
        for (int i = 0; i < 36; i++) player_tick(TICK, 1, 1);
        float moved = vlen(vsub(P.pos, p0)); int st1 = P.anvilState;
        for (int i = 0; i < 48; i++) player_tick(TICK, 0, 0);
        act[A_FWD] = 0;
        fprintf(f, "enclume rate : etat %d, vitesse horizontale %.1f, deplacement en 0,3 s (saut et dash appuyes) %.2f m etat %d, "
                   "etat apres 0,7 s %d (attendu 2, 0.0, 0.00 2, 0)\n", st0, hv, moved, st1, P.anvilState);
    }
    {   /* 3. trop pres du sol : c'est le coup au sol, meme en l'air */
        reset_game();
        P.meleeKind = MK_ANVIL; P.pos = P.prevPos = v3(-20, 0.8f, 40); P.grounded = 0;
        anvil_press();
        fprintf(f, "enclume a 0,8 m du sol : coup au sol=%d, etat %d, recharge %.2f (attendu 1, 0, 0.80)\n",
                P.meleeT > 0, P.anvilState, P.meleeCd);
    }
    {   /* 4. pique bas (2 m) : facteur de hauteur kh = 0,5 */
        reset_game();
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-18.6f, 1.0f, 40);
        test_slam_drop(v3(-20, 2, 40), v3(0, 0, 0));
        fprintf(f, "enclume pique bas (2 m) : |vy| %.1f, kh %.2f, degats %.1f (attendu 31.6, 0.50, 29.5)\n",
                P.anvilLastVy, slam_kh(2), 100 - E[0].hp);
    }
    {   /* 5. plafond de la vitesse de chute */
        reset_game();
        test_slam_drop(v3(-20, 40, 40), v3(0, 0, 0));
        fprintf(f, "enclume pique de 40 m : |vy| %.1f (attendu 45.0)\n", P.anvilLastVy);
    }
    for (int pass = 0; pass < 2; pass++) {   /* 6. coup au sol : a l'arret, puis a 10 m/s */
        reset_game();
        P.meleeKind = MK_ANVIL; P.pos = P.prevPos = v3(-20, 0, 40); P.grounded = 1; P.yaw = 0; P.pitch = 0;
        P.vel = pass ? v3(0, 0, -10) : v3(0, 0, 0);
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.6f, 38);
        melee();
        fprintf(f, "enclume coup au sol a %.0f m/s : PV drone %.0f, recharge %.2f, fenetre de parry %.2f (attendu %s, 0.80, 0.10)\n",
                vlen(P.vel), E[0].hp, P.meleeCd, P.parryWin, pass ? "25" : "55");
    }
    {   /* 7. le dash annule le pique ; pack_state annonce le pique (ST_SLAM) */
        reset_game();
        P.meleeKind = MK_ANVIL; P.pos = P.prevPos = v3(-20, 13, 40); P.grounded = 0; P.pitch = 0;
        anvil_press();
        for (int i = 0; i < 5; i++) player_tick(TICK, 0, 0);
        NetPState s;
        pack_state(&s);
        float dc = P.dashCharge;
        player_tick(TICK, 0, 1);
        fprintf(f, "enclume dash pendant le pique : pad %d, etat %d, vy %.1f, charges %.2f -> %.2f, recharge %.2f "
                   "(attendu %d, 0, 1.8 (2 moins un tick de gravite), 2.00 -> 1.00, 0.80)\n",
                s.pad, P.anvilState, P.vel.y, dc, P.dashCharge, P.meleeCd, MK_ANVIL | ST_SLAM);
    }
    {   /* 8. pique sur le jump pad est : onde, puis le pad relance (pas d'immobilisation) */
        reset_game();
        test_slam_drop(v3(ms(16), 5, 0), v3(0, 0, 0));
        V3 L = v3(ms(13), 20, 0);
        fprintf(f, "enclume sur un jump pad : onde=%d, vitesse (%.1f, %.1f, %.1f), etat %d (attendu 1, (%.1f, %.1f, %.1f), 0)\n",
                P.anvilLandT == 0, P.vel.x, P.vel.y, P.vel.z, P.anvilState, L.x, L.y, L.z);
    }

    /* --- Eventail (solo) : motif, cone, degats selon la distance, recul, pieces, munitions --- */
    {
        V3 dA[SG_PELLETS], dB[SG_PELLETS], dC[SG_PELLETS], d0 = vnorm(v3(0.3f, -0.2f, -0.93f));
        unsigned int rng0 = rng;                           /* 1-2. meme graine = memes plombs, rng global intact */
        sg_pellet_dirs(d0, SG_HALF_ANGLE, 1234u, dA);
        sg_pellet_dirs(d0, SG_HALF_ANGLE, 1234u, dB);
        sg_pellet_dirs(d0, SG_HALF_ANGLE, 98765u, dC);
        fprintf(f, "eventail motif : meme graine identique %d, autre graine differente %d, rng global intact %d (attendu 1, 1, 1)\n",
                !memcmp(dA, dB, sizeof dA), memcmp(dA, dC, sizeof dA) != 0, rng == rng0);
        float amax[2] = { 0, 0 };                          /* 3. angle max sur 200 graines, debout puis en slide */
        for (int pass = 0; pass < 2; pass++)
            for (unsigned int sd = 1; sd <= 200; sd++) {
                sg_pellet_dirs(d0, SG_HALF_ANGLE * (pass ? SG_SLIDE_SPREAD : 1), sd * 7919u, dA);
                for (int k = 0; k < SG_PELLETS; k++)
                    amax[pass] = fmaxf(amax[pass], atan2f(vlen(vcross(d0, dA[k])), vdot(d0, dA[k])) * 180 / PI);
            }
        fprintf(f, "eventail cone : angle max %.2f deg debout, %.2f en slide (attendu <= 4.50, <= 2.25)\n", amax[0], amax[1]);

        static const float dz[3] = { 5, 10, 20 };          /* 4, 5 b, 5. drone a 5 / 10 / 20 m */
        int pel[3]; float hpD[3];
        for (int s = 0; s < 3; s++) {
            reset_game();
            P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0; P.grounded = 1;
            P.ammo[WP_SHOTGUN] = 6; shotStreak = 0;
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.6f, 40 - dz[s]);
            fire_shotgun();
            pel[s] = P.sgLastPellets; hpD[s] = E[0].hp; hitstop = 0;
        }
        fprintf(f, "eventail drone a 5 / 10 / 20 m : plombs %d / %d / %d, PV %.1f / %.1f / %.1f (attendu 10 / 10 / 4, 20 / 20 / ~78.5)\n",
                pel[0], pel[1], pel[2], hpD[0], hpD[1], hpD[2]);

        reset_game();                                      /* 9-10. recul en l'air, plafonds, au sol */
        P.pos = P.prevPos = v3(-20, 8, 40); P.grounded = 0; P.yaw = 0; P.pitch = -PI / 2; P.ammo[WP_SHOTGUN] = 6;
        float rv[4];
        P.vel = v3(0, 0, 0); fire_shotgun(); rv[0] = P.vel.y;
        P.vel = v3(0, 8.4f, 0); fire_shotgun(); rv[1] = P.vel.y;
        P.vel = v3(0, -10, 0); fire_shotgun(); rv[2] = P.vel.y;
        P.vel = v3(0, 0, -20); P.yaw = PI; P.pitch = 0; fire_shotgun(); rv[3] = hlen(P.vel);   /* tir arriere apres un dash */
        P.grounded = 1; P.vel = v3(3, 0, -5); P.pitch = -PI / 2; fire_shotgun();
        fprintf(f, "eventail recul : vy %.1f depuis 0, %.1f depuis 8.4, %.1f en chute a -10, vitesse %.1f (tir arriere a 20), "
                   "au sol (%.1f %.1f %.1f) (attendu 7.0, 9.0, -3.0, 20.0, (3.0 0.0 -5.0))\n",
                rv[0], rv[1], rv[2], rv[3], P.vel.x, P.vel.y, P.vel.z);
        hitstop = 0;

        reset_game();                                      /* 11. piece eclatee : drone a 2 m d'elle, tireur a 2 m */
        P.pos = P.prevPos = v3(-20, 0, 38); P.yaw = 0; P.pitch = 0; P.grounded = 1; P.ammo[WP_SHOTGUN] = 6; shotStreak = 0;
        CO[0].alive = 1; CO[0].owner = myId; CO[0].life = 3; CO[0].pos = v3(-20, 1.6f, 36);
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-18, 1.6f, 36);
        fire_shotgun();
        fprintf(f, "eventail eclat : pieces eclatees %d, plombs %d, PV drone %.1f, PV joueur %.0f, chaine %d (attendu 1, 0, 71.2, 100, 1)\n",
                sgBursts, P.sgLastPellets, E[0].hp, P.hp, shotStreak);
        hitstop = 0;

        reset_game();                                      /* 12. deux pieces a 2 m : reaction en chaine */
        P.pos = P.prevPos = v3(-20, 0, 38); P.yaw = 0; P.pitch = 0; P.grounded = 1; P.ammo[WP_SHOTGUN] = 6;
        CO[0].alive = 1; CO[0].owner = myId; CO[0].life = 3; CO[0].pos = v3(-20, 1.6f, 36);
        CO[1].alive = 1; CO[1].owner = myId; CO[1].life = 3; CO[1].pos = v3(-18, 1.6f, 36);
        fire_shotgun();
        fprintf(f, "eventail eclat en chaine : pieces eclatees %d, message \"%s\" (attendu 2, ECLAT x2)\n", sgBursts, popText);
        hitstop = 0;

        int wl0 = wantLoadout[0], wl1 = wantLoadout[1];    /* 14. loadout Eventail + rail : 3 au spawn, 3 tirs, bascule */
        wantLoadout[0] = WP_SHOTGUN; wantLoadout[1] = WP_RAIL;
        reset_game();
        waveDelay = 1000;                                  /* aucun drone pendant le test */
        int spawnAmmo = P.ammo[WP_SHOTGUN], spawnW = P.weapon, shots = 0;
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = PI; P.pitch = 0;
        botHeld[A_FIRE] = 1;
        for (int i = 0; i < 480 && P.weapon == WP_SHOTGUN; i++) {
            int a0 = P.ammo[WP_SHOTGUN];
            game_tick(TICK); hitstop = 0;
            shots += P.ammo[WP_SHOTGUN] < a0;
        }
        botHeld[A_FIRE] = 0;
        fprintf(f, "eventail munitions : au spawn %d (%s), %d tirs puis a sec : munitions %d, arme %s (attendu 3 (EVENTAIL), 3, 0, RAILGUN)\n",
                spawnAmmo, ammoDef[spawnW].name, shots, P.ammo[WP_SHOTGUN], ammoDef[P.weapon].name);

        P.weapon = WP_SHOTGUN; P.switchT = 0;              /* 15. a vide pendant 10 s : aucune recharge passive */
        for (int i = 0; i < 1200; i++) game_tick(TICK);
        fprintf(f, "eventail a vide 10 s : munitions %d, jauge %.0f (attendu 0, 0)\n", P.ammo[WP_SHOTGUN], P.ammoGauge[WP_SHOTGUN]);

        reset_game();                                      /* 16. a vide, Eventail en main : 2 coups d'epee = 1 cartouche */
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        P.weapon = WP_SHOTGUN; P.ammo[WP_SHOTGUN] = 0; P.ammoGauge[WP_SHOTGUN] = 0;
        E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 1000; E[0].pos = v3(-20, 1.6f, 38);
        melee();
        P.meleeCd = 0; P.parryWin = 0; hitstop = 0;
        melee();
        hitstop = 0;
        int afterSword = P.ammo[WP_SHOTGUN]; float gSword = P.ammoGauge[WP_SHOTGUN];
        P.ammo[WP_SHOTGUN] = 5; P.ammoGauge[WP_SHOTGUN] = 0;   /* 17. a 5 : grande cellule (150) -> 6, jauge pleine */
        pickup_taken(0, myId);
        fprintf(f, "eventail jauge : 2 coups d'epee -> %d cartouche(s), jauge %.0f ; a 5 + grande cellule (%d) -> %d, jauge affichee %.0f "
                   "(attendu 1, 0 ; 150, 6, 100)\n", afterSword, gSword, PU[0].pts, P.ammo[WP_SHOTGUN], 100 * gauge_frac(WP_SHOTGUN));
        PU[0].t = 0;
        wantLoadout[0] = wl0; wantLoadout[1] = wl1;
    }

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

    /* Elan en LAN simule : x0,7 et plafond 60 par l'hote, puis aucune recharge passive */
    {
        static const float vs[3] = { 0, 9.5f, 15 };
        float hpLeft[3];
        NP[1].disp = v3(-20, 0, 31); vf(NP[1].st.pos, NP[1].disp);
        for (int s = 0; s < 3; s++) {
            P.pos = P.prevPos = v3(-20, 0, 41); P.yaw = 0; P.pitch = 0;
            P.ammo[WP_KINETIC] = 6; shotStreak = 0; P.kinDashT = 1; hitstop = 0;
            NP[1].hp = 100; NP[1].alive = 1;
            P.vel = v3(0, 0, -vs[s]);
            fire_kinetic();
            hpLeft[s] = NP[1].hp;
        }
        fprintf(f, "elan PvP a 0 / 9.5 / 15 m/s : PV cible %.2f / %.2f / %.2f (attendu 86 / 52.75 / 40)\n",
                hpLeft[0], hpLeft[1], hpLeft[2]);
        reset_game();
        dmMode = 1;
        NP[1].hp = 100;
        P.pos = P.prevPos = v3(-20, 0, 41); P.yaw = PI; P.pitch = 0;   /* vers le mur : aucune touche */
        P.loadout[0] = WP_KINETIC; P.loadout[1] = WP_RAIL; P.weapon = WP_KINETIC;
        P.ammo[WP_KINETIC] = 6; P.ammoGauge[WP_KINETIC] = 0; P.fireCd = 0; P.switchT = 0;
        botHeld[A_FIRE] = 1;
        for (int i = 0; i < 119; i++) { nowT += TICK; game_tick(TICK); }
        botHeld[A_FIRE] = 0;
        fprintf(f, "elan tenu 0.99 s : munitions %d, jauge %.0f (attendu 4, 0)\n", P.ammo[WP_KINETIC], P.ammoGauge[WP_KINETIC]);
    }

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

    /* Enclume en LAN : l'onde sur un joueur au sol, pas sur un joueur en l'air ; puis l'onde paree par la victime */
    {
        reset_game();
        dmMode = 1;
        memset(NP, 0, sizeof NP);
        NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
        NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible");
        NP[1].disp = v3(-20, 0, 38); vf(NP[1].st.pos, NP[1].disp);
        nowT += 1;
        test_slam_drop(v3(-20, 13, 40), v3(0, 0, 0));      /* 9. cible au sol a 2 m : f = 0,626, x 0,7 */
        float hpGround = NP[1].hp;
        NP[1].hp = 100; NP[1].disp.y = 1.6f; vf(NP[1].st.pos, NP[1].disp);
        test_slam_drop(v3(-20, 13, 40), v3(0, 0, 0));      /* pieds a 1,6 m : au-dessus de l'onde */
        fprintf(f, "enclume LAN : PV apres l'onde %.1f, cible en l'air (pieds a 1,6 m) %.0f, etat %d (attendu ~56.8, 100, 2)\n",
                hpGround, NP[1].hp, P.anvilState);

        reset_game();                                      /* 10. la victime pare l'onde (0,18 s plus tot : tolerance 0,20 s) */
        dmMode = 1;
        NP[1].hp = 100; NP[1].disp.y = 0; vf(NP[1].st.pos, NP[1].disp);
        nowT += 1;
        NP[1].lastParryT = nowT - 0.18;
        int parriesBefore = meleeParries;
        test_slam_drop(v3(-20, 13, 40), v3(0, 0, 0));
        float hpParried = NP[1].hp, vyAtt = P.vel.y, cdAtt = P.meleeCd;
        int rockets = P.ammo[WP_ROCKET];
        net_hit(1, 10, v3(0, 0, 0), W_MELEE);              /* une epee a la meme distance n'est plus paree (0,15 s) */
        fprintf(f, "enclume onde paree : PV cible %.0f, vy attaquant %.1f, recharge %.2f, parries +%d, roquettes %d (points rendus) ; "
                   "epee a 0,18 s : PV %.0f (attendu 100, 12.0, 1.00, +1, 4 ; 90)\n",
                hpParried, vyAtt, cdAtt, meleeParries - parriesBefore, rockets, NP[1].hp);

        reset_game();                                      /* 11. bot a l'Enclume, 4 m au-dessus d'une cible au sol : il pique */
        dmMode = 1;
        NP[1].hp = 100; NP[1].lastParryT = 0; NP[1].disp = v3(-20, 0, 38); vf(NP[1].st.pos, NP[1].disp);
        P.meleeKind = MK_ANVIL; P.pos = P.prevPos = v3(-20, 4, 40); P.grounded = 0;
        int wavesBefore = slamWaveHits, dived = 0;
        for (int i = 0; i < 120 && slamWaveHits == wavesBefore; i++) {
            nowT += TICK;
            bot_think(TICK);
            game_tick(TICK);
            dived |= P.anvilState == ANVIL_DIVE;
        }
        fprintf(f, "bot enclume : pique=%d, onde qui touche=%d, PV cible %.0f (attendu 1, 1, < 100)\n",
                dived, slamWaveHits - wavesBefore, NP[1].hp);
    }

    /* Eventail en LAN simule : un seul EV_HIT par tir (agregation), x0,7, plafond 60, chaine a 3 plombs, traceurs recus */
    {
        static const float dist[5] = { 3, 10, 3, 30, 10 };
        static const int streak0[5] = { 0, 0, 8, 2, 2 };
        float hpLeft[5]; int evHits[5], streakAfter[5], pel[5];
        for (int s = 0; s < 5; s++) {
            reset_game();
            dmMode = 1;
            memset(NP, 0, sizeof NP);
            NP[0].active = NP[0].alive = 1; NP[0].hp = 100; snprintf(NP[0].name, 16, "Moi");
            NP[1].active = NP[1].alive = 1; NP[1].hp = 100; snprintf(NP[1].name, 16, "Cible");
            P.pos = P.prevPos = v3(-20, 0, 60); P.grounded = 1;
            NP[1].disp = v3(-20, 0, 60 - dist[s]); vf(NP[1].st.pos, NP[1].disp);
            V3 d = vsub(np_center(1), eye_pos());
            P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
            P.ammo[WP_SHOTGUN] = 6; shotStreak = streak0[s]; hitstop = 0;
            int h0 = hitCount[W_SHOTGUN];
            fire_shotgun();
            hpLeft[s] = NP[1].hp; evHits[s] = hitCount[W_SHOTGUN] - h0; streakAfter[s] = shotStreak; pel[s] = P.sgLastPellets;
        }
        hitstop = 0;
        fprintf(f, "eventail PvP a 3 m : %d EV_HIT, %d plombs, PV %.1f (attendu 1, 10, 44)\n", evHits[0], pel[0], hpLeft[0]);
        fprintf(f, "eventail PvP a 10 m : %d EV_HIT, %d plombs, PV %.1f (attendu 1, 6 a 8 selon la rotation, %.1f = 100 - plombs x 5.6)\n",
                evHits[1], pel[1], hpLeft[1], 100 - pel[1] * SG_PELLET_DMG * PVP_SCALE);
        fprintf(f, "eventail PvP a 3 m, chaine 8 : PV %.1f (attendu 40 : plafond de 60)\n", hpLeft[2]);
        fprintf(f, "eventail PvP a 30 m, chaine 2 : %d plomb(s), chaine apres %d (attendu 1, 0 : moins de 3 plombs)\n", pel[3], streakAfter[3]);
        fprintf(f, "eventail PvP a 10 m, chaine 2 : %d plombs, chaine apres %d, PV %.1f (attendu 6 a 8, 3, %.1f = 100 - plombs x 7.28)\n",
                pel[4], streakAfter[4], hpLeft[4], 100 - pel[4] * SG_PELLET_DMG * 1.3f * PVP_SCALE);

        memset(TR, 0, sizeof TR);                          /* 13. EV_SHOT recu d'un autre joueur : 10 traceurs recalcules */
        NetEvent e = ev_new(EV_SHOT);
        e.from = 1; e.sub = SG_PELLETS; e.netId = 4242u; e.f[6] = SG_HALF_ANGLE;
        vf(e.f, v3(-20, 1.6f, 30)); vf(e.f + 3, v3(0, 0, 1));
        apply_event(&e);
        int live = 0;
        for (int i = 0; i < MAXTR; i++) live += TR[i].t < TR[i].life;
        fprintf(f, "eventail EV_SHOT recu : %d traceurs, recul visuel du tireur %.2f (attendu 10, 0.12)\n", live, NP[1].sgFireT);

        reset_game();                                      /* bot au loadout Eventail + rail, cible immobile a 8 m */
        dmMode = 1;
        NP[1].active = NP[1].alive = 1; NP[1].hp = 100; NP[1].disp = v3(-20, 0, 52); vf(NP[1].st.pos, NP[1].disp);
        P.pos = P.prevPos = v3(-20, 0, 60); P.yaw = 0; P.dashCharge = 0;   /* pas de cross-up : on veut le tir direct */
        P.loadout[0] = WP_RAIL; P.loadout[1] = WP_SHOTGUN; P.weapon = WP_RAIL;
        int hs0 = hitCount[W_SHOTGUN], hr0 = hitCount[W_RAIL];
        for (int i = 0; i < 90 && NP[1].alive; i++) { nowT += TICK; bot_think(TICK); game_tick(TICK); hitstop = 0; }
        fprintf(f, "bot eventail a 8 m : arme %s, touches eventail %d, rail %d (attendu EVENTAIL, >= 1, 0)\n",
                ammoDef[P.weapon].name, hitCount[W_SHOTGUN] - hs0, hitCount[W_RAIL] - hr0);
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

    /* mode bot : velocity.exe --bot host|join [--name N] [--time S] [--log fichier] [--botshot fichier.bmp] [--loadout a,b[,m]] */
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
            else if (!strcmp(t, "--loadout") && (t = strtok(NULL, " "))) {   /* --loadout arme1,arme2[,melee] (WP_*, WP_*, MK_*) */
                int l0, l1, mk, n = sscanf(t, "%d,%d,%d", &l0, &l1, &mk);
                if (n >= 2) { wantLoadout[0] = l0; wantLoadout[1] = l1; }
                if (n == 3) wantMelee = mk;
            }
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

    if (shotMode && (strstr(shotPath, "settings") || strstr(shotPath, "menu") || strstr(shotPath, "loadout"))) {   /* captures des menus */
        appState = APP_MENU;
        menu_backdrop();
        if (strstr(shotPath, "settings")) { settingsOpen = 1; waitAct = A_COIN; waitSlot = 1; }
        if (strstr(shotPath, "loadout")) loadoutOpen = 1;
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
        if (strstr(shotPath, "enclume")) {                /* adversaires a l'Enclume : coup au sol et pique */
            NP[1].st.pad = MK_ANVIL;
            NP[2].st.pad = MK_ANVIL | ST_SLAM; NP[2].disp = v3(-2, 5, 12); vf(NP[2].st.vel, v3(0, -34, 0));
        }
        P.pos = P.prevPos = v3(0, 0, 24); P.yaw = -0.1f; P.pitch = 0.08f; P.hp = 100;
        if (strstr(shotPath, "eventail")) {               /* Rival a l'Eventail : son tir recu (traceurs recalcules) */
            NP[1].st.weapon = WP_SHOTGUN; NP[1].st.flags = 0; NP[1].st.melee = 0;
            NetEvent e = ev_new(EV_SHOT);
            V3 o = v3(NP[1].disp.x, NP[1].disp.y + EYE_STAND, NP[1].disp.z), to = vnorm(vsub(v3(1.5f, 1.2f, 24), o));
            e.from = 1; e.sub = SG_PELLETS; e.netId = 777u; e.f[6] = SG_HALF_ANGLE; e.actor = 3;
            vf(e.f, o); vf(e.f + 3, to);
            apply_event(&e);
            for (int i = 0; i < 3; i++) fx_tick(TICK);
        }
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
        if (strstr(shotPath, "enclume")) {                /* pique de l'Enclume au-dessus d'un drone : repere d'impact */
            hook_release(0);
            P.meleeKind = MK_ANVIL; P.meleeT = 0; P.meleeCd = 0; P.weapon = WP_RAIL; P.switchT = 0; shotStreak = 0;
            P.pos = P.prevPos = v3(-20, 9, 40); P.yaw = 0; P.pitch = -0.7f; P.vel = v3(0, 0, -6); P.grounded = 0;
            E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.2f, 35);
            memset(PA, 0, sizeof PA); memset(PR, 0, sizeof PR); memset(BM, 0, sizeof BM);
            anvil_press();
            for (int i = 0; i < 6; i++) player_tick(TICK, 0, 0);
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
        if (netMode == NET_HOST) {
            char hb[512]; int n = 0;
            for (int w = 0; w < W_COUNT && n < (int)sizeof hb - 40; w++)
                if (hitCount[w]) n += snprintf(hb + n, sizeof hb - n, "%s %d, ", weaponNames[w], hitCount[w]);
            netlog("touches arbitrees : %s parries %d", n ? hb : "aucune, ", parryCount);
        }
        netlog("munitions : points touche %.0f, kill %.0f, parry %.0f, cellules %.0f (%d prises), perdus %.0f ; a sec %.0f%% du temps",
               apStat[AP_HIT], apStat[AP_KILL], apStat[AP_PARRY], apStat[AP_CELL], cellsTaken, apStat[AP_LOST],
               100 * dryT / fmaxf(botAliveT, 0.01f));
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
    }
    net_shutdown();
    if (botLog) fclose(botLog);
    set_capture(0);
    if (audio_ok) { waveOutReset(wout); waveOutClose(wout); }
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(rc);
    return 0;
}
