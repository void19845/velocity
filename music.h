/* ------------------------------------------------------------------ */
/* music.h : DESIGN DE LA MUSIQUE de VELOCITY (hard techno)            */
/*                                                                     */
/* Sequenceur synthetise, un morceau par carte. Ce fichier contient    */
/* les morceaux (tracks[]) ET le moteur ; main.c l'inclut, et          */
/* musique.c (musique.exe) permet de l'ecouter sans lancer le jeu.     */
/*                                                                     */
/* INTENSITE : le niveau (0 a MUS_LVMAX) monte avec les rounds (solo : */
/* vagues ; PvP : kills du leader, voir music_pvp_level). Il           */
/* change a chaque mesure : tempo, couches (clap, lead), saturation.   */
/*   niveau 0 : kick + rumble + hats (selon la mesure)                 */
/*   niveau 1 : + clap                                                 */
/*   niveau 2 : + lead (un pas sur quatre)                             */
/*   niveau 4 : + lead complet                                         */
/*   niveau 6 : + kicks syncopes (pas 8 et 16)                         */
/*   a chaque niveau : tempo +bpmStep, kick plus sature, lead plus clair*/
/* ------------------------------------------------------------------ */
#ifndef MUSIC_H
#define MUSIC_H

#include <math.h>
#include <string.h>
#ifndef SR
#define SR 44100
#endif
#ifndef PI
#define PI 3.14159265f
#endif

#define MUS_LVMAX 8

/* Un morceau = un tempo de depart, un pas de tempo par niveau, une tonique et jusqu'a 8 mesures
 * de 16 pas qui tournent en boucle.
 * kick / clap / hat / ohat : masques de 16 bits (bit 0 = pas 1 de la mesure) ;
 * bass / lead : demi-tons au-dessus de la tonique (basse : tonique, lead : 2 octaves plus haut), RS = silence. */
#define RS (-100)
typedef struct { unsigned short kick, clap, hat, ohat; signed char bass[16], lead[16]; } MusBar;
typedef struct { const char *name; float bpm, bpmStep, root; int nbars; MusBar bars[8]; } Track;

static const Track tracks[] = {
    /* Inspire de "Blood Rave" (Blade) : analyse du mp3 = 136 BPM, tonique do sans tierce (do re fa la, avec un do# de tension),
     * grosse basse continue, un break sans graves au milieu puis le retour du groove. Accelere x1,5 : 204 BPM au niveau 8. */
    { "Arene", 172.0f, 4.0f, 65.41f, 8, {                 /* do (do2), 172 BPM au niveau 0, 204 au niveau 8 */
        /* 1 intro : kick + rumble */
        { 0x1111, 0, 0, 0,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,0,RS, RS,RS,-5,RS }, { RS,RS,RS,RS, RS,RS,RS,RS, RS,RS,RS,RS, RS,RS,RS,RS } },
        /* 2 + hats */
        { 0x1111, 0, 0xAAAA, 0x4444,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,0,RS, RS,RS,2,RS }, { RS,RS,RS,RS, RS,RS,RS,RS, RS,RS,RS,RS, RS,RS,RS,RS } },
        /* 3 + clap */
        { 0x1111, 0x1010, 0xAAAA, 0x4444,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,0,RS, RS,RS,-5,RS }, { RS,RS,RS,RS, RS,RS,RS,RS, RS,RS,RS,RS, RS,RS,RS,RS } },
        /* 4 + premiers stabs */
        { 0x1111, 0x1010, 0xAAAA, 0x4444,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,0,RS, RS,RS,2,RS }, { 12,RS,RS,RS, RS,RS,RS,RS, 12,RS,RS,RS, RS,RS,9,RS } },
        /* 5 et 6 : plein pot */
        { 0x1111, 0x1010, 0xAAAA, 0x4444,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,0,RS, RS,RS,-5,RS }, { 12,RS,RS,12, RS,RS,9,RS, RS,12,RS,RS, 14,RS,12,RS } },
        { 0x1111, 0x1010, 0xAAAA, 0x4444,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,2,RS, RS,RS,2,RS }, { 12,RS,RS,12, RS,RS,9,RS, RS,12,RS,RS, 5,RS,9,RS } },
        /* 7 break : plus de kick ni de basse, roulement de clap, do# de tension */
        { 0x0000, 0xAAAA, 0xAAAA, 0, 
          { RS,RS,RS,RS, RS,RS,RS,RS, RS,RS,RS,RS, RS,RS,RS,RS }, { 12,RS,RS,RS, RS,RS,RS,RS, 13,RS,RS,RS, RS,RS,RS,RS } },
        /* 8 fill : kick en doubles croches sur la fin, retour du groove */
        { 0xFF11, 0x1010, 0xAAAA, 0x4444,
          { RS,RS,0,RS, RS,RS,0,RS, RS,RS,RS,RS, RS,RS,RS,RS }, { 12,RS,RS,12, RS,RS,14,RS, RS,RS,RS,RS, RS,RS,RS,RS } },
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
static struct {
    int track, bar, step, lvl, lvlWant;                   /* lvlWant : niveau demande, applique au debut de la mesure suivante */
    float cnt, sps, fade;                                 /* sps = echantillons par pas (double croche) */
    float drive, bright;                                  /* saturation du kick, ouverture du filtre du lead (selon le niveau) */
    float kEnv, kPh, kT;                                  /* kick (kT : temps depuis le dernier kick, pour le sidechain) */
    float bEnv, bPh, bFreq, bLp;                          /* basse rumble */
    float lEnv, lPh1, lPh2, lFreq, lLp;                   /* lead */
    float hEnv, oEnv, cEnv, hLp, cLp;                     /* hat, hat ouvert, clap */
    unsigned rng;
} mus = { -1 };

static float mus_noise(void) { mus.rng = mus.rng * 1664525u + 1013904223u; return (mus.rng >> 8) * (2.0f / 16777216.0f) - 1; }

static void mus_apply_level(void) {                       /* tempo, saturation et clarte du niveau courant */
    const Track *tr = &tracks[mus.track];
    mus.lvl = mus.lvlWant;
    mus.sps = SR * 60.0f / (tr->bpm + tr->bpmStep * mus.lvl) / 4;
    mus.drive = 2.2f + 0.3f * mus.lvl;
    mus.bright = 0.04f + 0.01f * mus.lvl;
}

static void music_level(int lvl) {                        /* appele a chaque round : prend effet a la prochaine mesure */
    mus.lvlWant = lvl < 0 ? 0 : lvl > MUS_LVMAX ? MUS_LVMAX : lvl;
}

static void music_set(int t, int lvl) {                   /* t = -1 : silence ; le morceau repart du debut avec un fondu */
    memset(&mus, 0, sizeof mus);
    mus.rng = 12345u;
    if (t < 0 || t >= NTRACKS) { mus.track = -1; return; }
    mus.track = t;
    music_level(lvl);
    mus_apply_level();
    mus.cnt = mus.sps;                                    /* le premier pas part tout de suite */
    mus.step = 15; mus.bar = tracks[t].nbars - 1;
    mus.kT = 9;
}

static void music_step(void) {
    const Track *tr = &tracks[mus.track];
    if (++mus.step >= 16) {
        mus.step = 0; mus.bar = (mus.bar + 1) % tr->nbars;
        if (mus.lvl != mus.lvlWant) mus_apply_level();
    }
    const MusBar *b = &tr->bars[mus.bar];
    int s = mus.step, lv = mus.lvl;
    unsigned kick = b->kick | (lv >= 6 ? 0x8080 : 0);
    if (kick >> s & 1) { mus.kEnv = 1; mus.kPh = 0; mus.kT = 0; }
    if (b->hat >> s & 1) mus.hEnv = 1;
    if (b->ohat >> s & 1) mus.oEnv = 1;
    if (lv >= 1 && (b->clap >> s & 1)) mus.cEnv = 1;
    if (b->bass[s] != RS) { mus.bEnv = 1; mus.bFreq = tr->root * powf(2.0f, b->bass[s] / 12.0f); }
    if (lv >= 2 && b->lead[s] != RS && (lv >= 4 || s % 4 == 0)) {
        mus.lEnv = 1; mus.lFreq = tr->root * 4 * powf(2.0f, b->lead[s] / 12.0f);
    }
}

static float music_sample(void) {                         /* un echantillon de musique, entre -1 et 1 environ */
    if (mus.track < 0) return 0;
    if (++mus.cnt >= mus.sps) { mus.cnt -= mus.sps; music_step(); }
    mus.fade = fminf(mus.fade + 1.0f / (1.5f * SR), 1.0f);
    float out = 0, duck;
    mus.kT += 1.0f / SR;
    duck = 1 - 0.85f * expf(-mus.kT * 14);                /* sidechain : tout ce qui n'est pas le kick se creuse a chaque coup */
    /* kick : sinus qui plonge de 250 a 48 Hz, sature, plus un clic */
    mus.kPh += (48 + 200 * expf(-mus.kT * 40)) / SR;
    out += tanhf(sinf(mus.kPh * 2 * PI) * mus.drive) * mus.kEnv * 0.9f + mus_noise() * mus.kEnv * expf(-mus.kT * 400) * 0.25f;
    mus.kEnv *= 0.99985f;
    /* basse rumble : scie filtree + sub */
    mus.bPh += mus.bFreq / SR; if (mus.bPh >= 1) mus.bPh -= 1;
    mus.bLp += 0.06f * ((2 * mus.bPh - 1) - mus.bLp);
    out += (mus.bLp * 0.7f + sinf(mus.bPh * 2 * PI) * 0.5f) * mus.bEnv * duck * 0.55f;
    mus.bEnv *= 0.99992f;
    /* hats : bruit passe-haut */
    float n = mus_noise();
    mus.hLp += 0.35f * (n - mus.hLp);
    out += (n - mus.hLp) * (mus.hEnv * 0.16f + mus.oEnv * 0.12f);
    mus.hEnv *= 0.9985f; mus.oEnv *= 0.99985f;
    /* clap : bruit medium */
    mus.cLp += 0.5f * (n - mus.cLp);
    out += mus.cLp * mus.cEnv * 0.35f;
    mus.cEnv *= 0.9992f;
    /* lead : deux scies desaccordees, saturees, filtre qui se ferme */
    mus.lPh1 += mus.lFreq / SR; if (mus.lPh1 >= 1) mus.lPh1 -= 1;
    mus.lPh2 += mus.lFreq * 1.012f / SR; if (mus.lPh2 >= 1) mus.lPh2 -= 1;
    mus.lLp += (mus.bright + 0.55f * mus.lEnv * mus.lEnv) * ((mus.lPh1 + mus.lPh2 - 1) - mus.lLp);
    out += tanhf(mus.lLp * 3.0f) * mus.lEnv * duck * 0.3f;
    mus.lEnv *= 0.99993f;                              /* note de lead : ~0,3 s */
    return out * mus.fade;
}

#endif
