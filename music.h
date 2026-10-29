/* ------------------------------------------------------------------ */
/* music.h : DESIGN DE LA MUSIQUE de VELOCITY (hard techno)            */
/*                                                                     */
/* Sequenceur synthetise, un morceau par carte. Ce fichier contient    */
/* les morceaux (tracks[]) ET le moteur ; main.c l'inclut, et          */
/* musique.c (musique.exe) permet de l'ecouter sans lancer le jeu.     */
/*                                                                     */
/* INTENSITE : le niveau (0 a MUS_LVMAX) monte avec les rounds (solo : */
/* vagues ; PvP : kills du leader, voir music_pvp_level). Il           */
/* change a chaque mesure : tempo, couches, saturation.                */
/*   niveau 0 : kick + rumble + hats (selon la mesure)                 */
/*   niveau 1 : + clap, crash toutes les 8 mesures                     */
/*   niveau 2 : + lead (un pas sur quatre), variations de hats         */
/*   niveau 3 : ordre des mesures sans intro (songHi), rolls de kick   */
/*   niveau 4 : + lead complet                                         */
/*   niveau 5 : + acid (ligne 303 en doubles croches)                  */
/*   niveau 6 : + kicks syncopes (pas 8 et 16)                         */
/*   a chaque niveau : tempo +bpmStep, kick plus sature, lead plus clair*/
/*                                                                     */
/* DIVERSITE : un morceau est une SUITE de 32 mesures (song / songHi)  */
/* piochees dans des cellules, avec une transposition par groupe de 4  */
/* mesures (prog), des breaks, des risers, des crashs, un filtre qui   */
/* respire, et des micro-variations tirees au hasard (hats fantomes,   */
/* octaves, notes voisines, rolls). Le hasard depend de musSeed : il   */
/* change a chaque partie mais reste identique si on le fixe (tests).  */
/* ------------------------------------------------------------------ */
#ifndef MUSIC_H
#define MUSIC_H

#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifndef SR
#define SR 44100
#endif
#ifndef PI
#define PI 3.14159265f
#endif

#define MUS_LVMAX 8
#define MUS_DLY   16384                                   /* tampon de l'echo (puissance de 2, > 3 pas au tempo le plus lent) */
#define MUS_CELLS 16
#define MUS_SONG  32

/* Une cellule = une mesure de 16 pas.
 * kick / clap / hat / ohat : masques de 16 bits (bit 0 = pas 1 de la mesure) ;
 * bass / lead : demi-tons au-dessus de la tonique (basse : tonique, lead : 2 octaves plus haut), RS = silence ;
 * fx : FX_RISER (bruit montant sur toute la mesure), FX_CRASH (cymbale au pas 1). */
#define RS (-100)
#define R16 RS,RS,RS,RS, RS,RS,RS,RS, RS,RS,RS,RS, RS,RS,RS,RS
#define FX_RISER 1
#define FX_CRASH 2
typedef struct { unsigned short kick, clap, hat, ohat; signed char bass[16], lead[16]; unsigned char fx; } MusBar;
/* Un morceau : tempo de depart, pas de tempo par niveau, tonique, ordre des cellules sur 32 mesures
 * (song : niveaux 0 a 2, avec une intro progressive ; songHi : des le niveau 3, sans intro),
 * transposition par groupe de 4 mesures. */
typedef struct {
    const char *name; float bpm, bpmStep, root;
    int nsong; unsigned char song[MUS_SONG], songHi[MUS_SONG]; signed char prog[8];
    MusBar cells[MUS_CELLS];
} Track;

static const Track tracks[] = {
    /* Inspire de "Blood Rave" (Blade) : analyse du mp3 = 136 BPM, tonique do sans tierce (do re fa la, avec un do# de tension),
     * grosse basse continue, un break sans graves au milieu puis le retour du groove. Accelere x1,5 : 204 BPM au niveau 8. */
    { "Arene", 172.0f, 4.0f, 65.41f,                      /* do (do2), 172 BPM au niveau 0, 204 au niveau 8 */
      32,
      { 0,0,1,1, 2,2,3,7,   4,5,4,5, 8,8,11,7,   6,6,10,11, 12,4,5,7,   9,9,8,5, 4,5,11,7 },
      { 4,5,4,5, 8,8,9,7,   4,5,4,5, 8,8,11,7,   6,6,10,11, 12,4,5,7,   9,9,8,5, 4,5,11,7 },
      { 0, 0, -2, 0,  0, -4, -2, 0 },
      {
        /* 0 intro : kick + rumble */
        { 0x1111, 0, 0, 0,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,0,RS, RS,RS,-5,RS }, { R16 }, 0 },
        /* 1 + hats */
        { 0x1111, 0, 0xAAAA, 0x4444,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,0,RS, RS,RS,2,RS }, { R16 }, 0 },
        /* 2 + clap */
        { 0x1111, 0x1010, 0xAAAA, 0x4444,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,0,RS, RS,RS,-5,RS }, { R16 }, 0 },
        /* 3 + premiers stabs */
        { 0x1111, 0x1010, 0xAAAA, 0x4444,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,0,RS, RS,RS,2,RS }, { 12,RS,RS,RS, RS,RS,RS,RS, 12,RS,RS,RS, RS,RS,9,RS }, 0 },
        /* 4 plein pot A */
        { 0x1111, 0x1010, 0xAAAA, 0x4444,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,0,RS, RS,RS,-5,RS }, { 12,RS,RS,12, RS,RS,9,RS, RS,12,RS,RS, 14,RS,12,RS }, 0 },
        /* 5 plein pot B */
        { 0x1111, 0x1010, 0xAAAA, 0x4444,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,2,RS, RS,RS,2,RS }, { 12,RS,RS,12, RS,RS,9,RS, RS,12,RS,RS, 5,RS,9,RS }, 0 },
        /* 6 break : plus de kick ni de basse, roulement de clap, do# de tension */
        { 0x0000, 0xAAAA, 0xAAAA, 0,
          { R16 }, { 12,RS,RS,RS, RS,RS,RS,RS, 13,RS,RS,RS, RS,RS,RS,RS }, FX_CRASH },
        /* 7 fill : kick en doubles croches sur la fin */
        { 0xFF11, 0x1010, 0xAAAA, 0x4444,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,RS,RS, RS,RS,RS,RS }, { 12,RS,RS,12, RS,RS,14,RS, RS,RS,RS,RS, RS,RS,RS,RS }, 0 },
        /* 8 galop : rumble en doubles notes */
        { 0x1111, 0x1010, 0xAAAA, 0x4444,
          { RS,RS,0,0, RS,RS,0,0, RS,RS,0,0, RS,RS,-5,0 }, { 9,RS,RS,9, RS,12,RS,RS, 9,RS,RS,5, RS,RS,2,RS }, 0 },
        /* 9 stabs en rafale */
        { 0x1111, 0x1010, 0xFAFA, 0x4444,
          { RS,RS,2,RS, RS,RS,2,RS, RS,RS,0,RS, RS,RS,-5,RS }, { 12,12,RS,12, RS,14,RS,RS, 12,12,RS,9, RS,RS,14,17 }, 0 },
        /* 10 tension : presque que le kick, quelques notes longues */
        { 0x1111, 0x1010, 0x8888, 0x4444,
          { RS,RS,0,RS, RS,RS,RS,RS, RS,RS,RS,RS, RS,RS,RS,RS }, { 12,RS,RS,RS, RS,RS,RS,RS, 9,RS,RS,RS, RS,RS,RS,RS }, 0 },
        /* 11 riser : roulement de clap et bruit qui monte vers la mesure suivante */
        { 0x1111, 0xF000, 0xAAAA, 0,
          { R16 }, { 12,RS,RS,RS, RS,RS,RS,RS, RS,RS,RS,RS, RS,RS,RS,RS }, FX_RISER },
        /* 12 drop : basse lourde, crash */
        { 0x1111, 0x1010, 0xAAAA, 0x4444,
          { 0,RS,0,RS, RS,RS,0,RS, 0,RS,0,RS, RS,RS,-5,RS }, { 12,RS,RS,RS, 12,RS,RS,RS, 12,RS,RS,RS, 12,RS,14,RS }, FX_CRASH },
      } },
};
#define NTRACKS ((int)(sizeof tracks / sizeof tracks[0]))

/* PvP : un round = un kill. La musique suit le LEADER du match, en 3 phases (tiers de la limite de frags :
 * 0-4 / 5-9 / 10-14 pour 15 frags), puis un FINAL quand les deux premiers sont a limite-1 (14-14).
 * musPhaseLvl : niveau d'intensite de chaque phase (0, 1, 2) puis du final. */
static const int musPhaseLvl[4] = { 0, 3, 6, 8 };
static int music_pvp_level(int leader, int second, int limit) {
    if (limit > 1 && leader >= limit - 1 && second >= limit - 1) return musPhaseLvl[3];
    int ph = limit > 0 ? leader * 3 / limit : 0;
    return musPhaseLvl[ph < 0 ? 0 : ph > 2 ? 2 : ph];
}

#define MUS_GAIN 26000.0f                                 /* gain du mixage musique (jeu et musique.exe) */
static float musicVol = 0.8f;
static unsigned musSeed = 1;                              /* graine des variations : le jeu la change a chaque partie */
static struct {
    int track, barCount, step, lvl, lvlWant;              /* lvlWant : niveau demande, applique au debut de la mesure suivante */
    int prog;                                             /* transposition du groupe de 4 mesures en cours */
    float cnt, sps, fade;                                 /* sps = echantillons par pas (double croche) */
    float drive, bright, brightMod, acidGain;             /* selon le niveau et la position dans le morceau */
    float kEnv, kPh, kT;                                  /* kick (kT : temps depuis le dernier kick, pour le sidechain) */
    float bEnv, bPh, bFreq, bLp;                          /* basse rumble */
    float lEnv, lDecay, lPh[7], lFreq, lLp, lLp2, detune; /* lead supersaw (detune : ecart des 7 scies, grandit avec le niveau) */
    float lpl[7], lpr[7];                                 /* gains gauche / droite de chaque scie */
    float hpL, hpR, opL, opR, cLp2, crLp2, rLp2;          /* stereo : position des hats, deuxieme voie des bruits larges */
    float dly[2][MUS_DLY]; int dw;                        /* echo ping-pong du lead */
    float aEnv, aAmp, aAcc, aPh, aFreq, aLow, aBand;      /* acid */
    float hEnv, oEnv, cEnv, hLp, cLp;                     /* hat, hat ouvert, clap */
    float crEnv, crLp, rise, rLp; int riseOn;             /* crash, riser */
    unsigned rng, seed;
} mus = { -1 };

static float mus_noise(void) { mus.rng = mus.rng * 1664525u + 1013904223u; return (mus.rng >> 8) * (2.0f / 16777216.0f) - 1; }

/* nombre pseudo-aleatoire entre 0 et 1, fixe pour (mesure, pas, sel) : meme partie = meme musique */
static float mus_hash(int bar, int step, int sel) {
    unsigned h = mus.seed ^ ((unsigned)bar * 374761393u) ^ ((unsigned)step * 668265263u) ^ ((unsigned)sel * 2246822519u);
    h = (h ^ (h >> 13)) * 1274126177u; h ^= h >> 16;
    return (h & 0xFFFFFF) / 16777216.0f;
}

static const float musDet[7] = { -1.0f, -0.62f, -0.25f, 0.0f, 0.25f, 0.62f, 1.0f };   /* desaccord et position des 7 scies */
static const signed char musScale[] = { 0, 2, 5, 9, 12, 14, 17, 21, 24 };   /* do ré fa la, sans tierce */
static int mus_scale_step(int n, int dir) {               /* note voisine dans la gamme */
    int best = 0;
    for (int i = 1; i < (int)sizeof musScale; i++)
        if (abs(n - musScale[i]) < abs(n - musScale[best])) best = i;
    best += dir;
    return musScale[best < 0 ? 0 : best >= (int)sizeof musScale ? (int)sizeof musScale - 1 : best];
}

static void mus_apply_level(void) {                       /* tempo, saturation et clarte du niveau courant */
    const Track *tr = &tracks[mus.track];
    mus.lvl = mus.lvlWant;
    mus.sps = SR * 60.0f / (tr->bpm + tr->bpmStep * mus.lvl) / 4;
    mus.drive = 2.2f + 0.3f * mus.lvl;
    mus.bright = 0.04f + 0.01f * mus.lvl;
    mus.detune = 0.006f + 0.0015f * mus.lvl;
    mus.acidGain = mus.lvl >= 5 ? 0.18f + 0.05f * (mus.lvl - 5) : 0;
}

static void music_level(int lvl) {                        /* appele a chaque round : prend effet a la prochaine mesure */
    mus.lvlWant = lvl < 0 ? 0 : lvl > MUS_LVMAX ? MUS_LVMAX : lvl;
}

static void music_set(int t, int lvl) {                   /* t = -1 : silence ; le morceau repart du debut avec un fondu */
    memset(&mus, 0, sizeof mus);
    mus.rng = 12345u; mus.seed = musSeed;
    if (t < 0 || t >= NTRACKS) { mus.track = -1; return; }
    mus.track = t;
    music_level(lvl);
    mus_apply_level();
    mus.cnt = mus.sps;                                    /* le premier pas part tout de suite */
    mus.step = 15; mus.barCount = -1;                     /* le premier pas ouvre la mesure 0 */
    mus.kT = 9; mus.brightMod = 1; mus.lDecay = 0.99993f;
    for (int i = 0; i < 7; i++) {
        float a = (musDet[i] * 0.85f + 1) * PI / 4;       /* position de la scie i : puissance constante */
        mus.lPh[i] = mus_hash(0, i, 99);                  /* phases de depart etalees */
        mus.lpl[i] = cosf(a) * 1.4142f; mus.lpr[i] = sinf(a) * 1.4142f;
    }
    mus.hpL = mus.hpR = mus.opL = mus.opR = 1;
}

static void music_step(void) {
    const Track *tr = &tracks[mus.track];
    if (++mus.step >= 16) {
        mus.step = 0; mus.barCount++;
        if (mus.lvl != mus.lvlWant) mus_apply_level();
        mus.prog = tr->prog[(mus.barCount / 4) % 8];
        mus.brightMod = 1 + 0.6f * sinf(2 * PI * mus.barCount / 16.0f);     /* le filtre du lead respire sur 16 mesures */
    }
    int bc = mus.barCount, s = mus.step, lv = mus.lvl, pos = bc % tr->nsong;
    const MusBar *b = &tr->cells[(lv >= 3 ? tr->songHi : tr->song)[pos]];

    unsigned kick = b->kick;
    if (kick && lv >= 6) kick |= 0x8080;
    if (kick && lv >= 3 && bc % 4 == 3 && s >= 12 && mus_hash(bc, s, 1) < 0.5f) kick |= 1u << s;   /* roll de fin de phrase */
    if (kick >> s & 1) { mus.kEnv = 1; mus.kPh = 0; mus.kT = 0; }

    int hat = b->hat >> s & 1;
    if (hat && mus_hash(bc, s, 2) < 0.12f) hat = 0;       /* hat qui saute */
    else if (!hat && b->hat && lv >= 2 && mus_hash(bc, s, 3) < 0.08f + 0.01f * lv) hat = 1;   /* hat fantome */
    if (hat) {
        float a = ((mus_hash(bc, s, 13) - 0.5f) * 0.9f + 1) * PI / 4;      /* chaque hat tombe a un endroit different */
        mus.hpL = cosf(a) * 1.4142f; mus.hpR = sinf(a) * 1.4142f;
        mus.hEnv = 0.6f + 0.4f * mus_hash(bc, s, 4);
    }
    if (b->ohat >> s & 1) {
        float a = ((mus_hash(bc, s, 14) - 0.5f) * 0.9f + 1) * PI / 4;
        mus.opL = cosf(a) * 1.4142f; mus.opR = sinf(a) * 1.4142f;
        mus.oEnv = 1;
    }
    if (lv >= 1 && (b->clap >> s & 1)) mus.cEnv = 1;

    if (b->bass[s] != RS) {
        int n = b->bass[s] + mus.prog;
        if (n < -7) n += 12;                              /* pas plus grave que le sub audible */
        if (lv >= 2 && mus_hash(bc, s, 5) < 0.12f) n += 12;   /* saut d'octave */
        mus.bEnv = 1; mus.bFreq = tr->root * powf(2.0f, n / 12.0f);
    }
    if (lv >= 2 && b->lead[s] != RS && (lv >= 4 || s % 4 == 0)) {
        int n = b->lead[s];
        float r = mus_hash(bc, s, 6);
        if (lv >= 3 && r < 0.15f) n = mus_scale_step(n, mus_hash(bc, s, 7) < 0.5f ? -1 : 1);   /* note voisine */
        else if (lv >= 4 && r > 0.93f) n += 12;           /* octave au-dessus */
        mus.lEnv = 0.75f + 0.25f * mus_hash(bc, s, 8);    /* accent */
        mus.lDecay = mus_hash(bc, s, 9) < 0.3f ? 0.9997f : 0.99993f;   /* note courte ou longue */
        mus.lFreq = tr->root * 4 * powf(2.0f, (n + mus.prog) / 12.0f);
    }
    if (lv >= 5 && b->kick) {                             /* acid : meme motif sur 2 mesures */
        int p2 = bc / 2;
        if (mus_hash(p2, s, 10) < 0.55f) {
            static const signed char an[6] = { 0, 0, 2, -5, 5, 12 };
            int n = an[(int)(mus_hash(p2, s, 11) * 6) % 6];
            mus.aFreq = tr->root * 2 * powf(2.0f, (n + mus.prog) / 12.0f);
            mus.aEnv = 1; mus.aAmp = 1; mus.aAcc = mus_hash(p2, s, 12) < 0.3f ? 1.6f : 1;
        }
    }
    if (s == 0) {
        mus.rise = 0;
        if ((b->fx & FX_CRASH) || (lv >= 1 && b->kick && bc % 8 == 0)) mus.crEnv = 1;
    }
    mus.riseOn = (b->fx & FX_RISER) != 0;
}

/* Un echantillon de musique STEREO (entre -1 et 1 environ). c = centre (kick, basse, acid), l / r = cotes.
 * Hats places au hasard dans l'image, clap / crash / riser larges (deux bruits differents), lead supersaw
 * etale (une scie par position) avec un echo ping-pong d'une croche pointee. */
static void music_frame(float *outL, float *outR) {
    *outL = *outR = 0;
    if (mus.track < 0) return;
    if (++mus.cnt >= mus.sps) { mus.cnt -= mus.sps; music_step(); }
    mus.fade = fminf(mus.fade + 1.0f / (1.5f * SR), 1.0f);
    float c = 0, l = 0, r = 0, duck;
    mus.kT += 1.0f / SR;
    duck = 1 - 0.85f * expf(-mus.kT * 14);                /* sidechain : tout ce qui n'est pas le kick se creuse a chaque coup */
    /* kick : sinus qui plonge de 250 a 48 Hz, sature, plus un clic */
    mus.kPh += (48 + 200 * expf(-mus.kT * 40)) / SR;
    c += tanhf(sinf(mus.kPh * 2 * PI) * mus.drive) * mus.kEnv * 0.9f + mus_noise() * mus.kEnv * expf(-mus.kT * 400) * 0.25f;
    mus.kEnv *= 0.99985f;
    /* basse rumble : scie filtree + sub */
    mus.bPh += mus.bFreq / SR; if (mus.bPh >= 1) mus.bPh -= 1;
    mus.bLp += 0.06f * ((2 * mus.bPh - 1) - mus.bLp);
    c += (mus.bLp * 0.7f + sinf(mus.bPh * 2 * PI) * 0.5f) * mus.bEnv * duck * 0.55f;
    mus.bEnv *= 0.99992f;
    /* hats : bruit passe-haut, chaque coup a sa position */
    float n = mus_noise(), n2 = mus_noise();
    mus.hLp += 0.35f * (n - mus.hLp);
    float hs = n - mus.hLp;
    l += hs * (mus.hEnv * 0.16f * mus.hpL + mus.oEnv * 0.12f * mus.opL);
    r += hs * (mus.hEnv * 0.16f * mus.hpR + mus.oEnv * 0.12f * mus.opR);
    mus.hEnv *= 0.9985f; mus.oEnv *= 0.99985f;
    /* clap : bruit medium, large */
    mus.cLp += 0.5f * (n - mus.cLp); mus.cLp2 += 0.5f * (n2 - mus.cLp2);
    l += mus.cLp * mus.cEnv * 0.35f; r += mus.cLp2 * mus.cEnv * 0.35f;
    mus.cEnv *= 0.9992f;
    /* crash : bruit tres aigu qui s'eteint en ~1 s, large */
    mus.crLp += 0.6f * (n - mus.crLp); mus.crLp2 += 0.6f * (n2 - mus.crLp2);
    l += (n - mus.crLp) * mus.crEnv * 0.2f; r += (n2 - mus.crLp2) * mus.crEnv * 0.2f;
    mus.crEnv *= 0.99996f;
    /* riser : bruit dont la brillance et le volume montent sur la mesure, large */
    if (mus.riseOn) mus.rise = fminf(1, mus.rise + 1.0f / (16 * mus.sps)); else mus.rise = 0;
    float rk = 0.03f + 0.55f * mus.rise, rg = mus.rise * mus.rise * 0.7f;
    mus.rLp += rk * (n - mus.rLp); mus.rLp2 += rk * (n2 - mus.rLp2);
    l += mus.rLp * rg; r += mus.rLp2 * rg;
    /* lead : SUPERSAW, 7 scies desaccordees (celle du centre plus forte), chacune a sa place, saturees, filtre qui se ferme */
    float swL = 0, swR = 0;
    for (int i = 0; i < 7; i++) {
        mus.lPh[i] += mus.lFreq * (1 + musDet[i] * mus.detune) / SR; if (mus.lPh[i] >= 1) mus.lPh[i] -= 1;
        float s = (2 * mus.lPh[i] - 1) * (i == 3 ? 1.0f : 0.7f);
        swL += s * mus.lpl[i]; swR += s * mus.lpr[i];
    }
    float lk = mus.bright * mus.brightMod + 0.55f * mus.lEnv * mus.lEnv;
    mus.lLp += lk * (swL * 0.3f - mus.lLp); mus.lLp2 += lk * (swR * 0.3f - mus.lLp2);
    float ld = mus.lEnv * duck * 0.34f, leadL = tanhf(mus.lLp * 3.0f) * ld, leadR = tanhf(mus.lLp2 * 3.0f) * ld;
    mus.lEnv *= mus.lDecay;
    /* echo ping-pong du lead (3 pas) : l'entree va dans la ligne gauche, chaque rebond passe de l'autre cote */
    {
        int d = (int)(3 * mus.sps); if (d > MUS_DLY - 1) d = MUS_DLY - 1;
        float dl = mus.dly[0][(mus.dw - d) & (MUS_DLY - 1)], dr = mus.dly[1][(mus.dw - d) & (MUS_DLY - 1)];
        mus.dly[0][mus.dw & (MUS_DLY - 1)] = (leadL + leadR) * 0.5f + dr * 0.5f;
        mus.dly[1][mus.dw & (MUS_DLY - 1)] = dl * 0.5f;
        mus.dw++;
        l += leadL + dl * 0.45f; r += leadR + dr * 0.45f;
    }
    /* acid : scie dans un filtre resonant (variable d'etat), le filtre s'ouvre a chaque note */
    if (mus.acidGain > 0) {
        mus.aPh += mus.aFreq / SR; if (mus.aPh >= 1) mus.aPh -= 1;
        float f = 0.05f + 0.28f * mus.aEnv * mus.aAcc;
        mus.aLow += f * mus.aBand;
        float hi = (2 * mus.aPh - 1) - mus.aLow - 0.3f * mus.aBand;
        mus.aBand += f * hi;
        c += tanhf(mus.aLow * 2.0f) * mus.aAmp * duck * mus.acidGain;
        mus.aEnv *= 0.9996f; mus.aAmp *= 0.99985f;
    }
    *outL = (c + l) * mus.fade; *outR = (c + r) * mus.fade;
}

static float music_sample(void) {                         /* version mono (moyenne des deux voies) : tests et outils */
    float l, r;
    music_frame(&l, &r);
    return (l + r) * 0.5f;
}

#endif
