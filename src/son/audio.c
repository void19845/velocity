/* VELOCITY - mixeur waveOut / AudioQueue, sons synthetises, musique
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

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
       S_SAW_THROW, S_SAW_PLANT, S_SAW_HIT, S_SAW_CATCH, S_SAW_BOUNCE,   /* Scie */
       S_TESLA_LOOP, S_TESLA_ZAP,                         /* Tesla */
       S_LANCE_COUCH, S_LANCE_THRUST, S_LANCE_HIT, S_LANCE_CLASH,        /* Lance */
       S_MINE_THROW, S_MINE_STICK, S_MINE_BEEP,           /* Ventouse */
       S_FLAIL_SWING, S_FLAIL_HIT, S_FLAIL_COIN,          /* Fleau */
       S_FIST_HIT, S_FIST_UPPER, S_FIST_PJ,               /* Poings */
       S_SHIELD_UP, S_SHIELD_BLOCK, S_SHIELD_BREAK, S_SHIELD_BASH,   /* Egide */
       S_HEADSHOT, S_CHAIN_FULL,                          /* tir a la tete, chaine de tirs pleine */
       S_COUNT };
typedef struct { short *data; int len; } Sound;
/* loop : voix en boucle (Tesla), jamais volee par play ; fade : baisse de volume par echantillon (arret sans clic) */
typedef struct { int snd, active, loop; float pos, step, vol, fade; } Voice;
static Sound sounds[S_COUNT];
static Voice voices[MAXV];
#ifdef _WIN32
static HWAVEOUT wout;
static WAVEHDR whdr[NBUF];
#endif
static short wbuf[NBUF][BUFLEN * 2];                      /* BUFLEN images, 2 voies entrelacees */
static int audio_ok;
static float masterVol = 1.0f;
#include "music.h"                                    /* design de la musique : morceaux, sequenceur, niveau d'intensite */

static int play_voice(int s, float vol, float pitch);
static void play(int s, float vol, float pitch) { play_voice(s, vol, pitch); }
static int play_voice(int s, float vol, float pitch) {    /* renvoie l'index de la voix, -1 si rien */
    if (!audio_ok) return -1;
    int best = 0; float oldest = -1;
    for (int i = 0; i < MAXV; i++) {
        if (!voices[i].active) { best = i; break; }
        if (voices[i].loop) continue;                     /* une boucle (Tesla) n'est jamais volee */
        if (voices[i].pos > oldest) { oldest = voices[i].pos; best = i; }
    }
    if (voices[best].active && voices[best].loop) return -1;
    voices[best].snd = s; voices[best].active = 1; voices[best].pos = 0;
    voices[best].step = pitch; voices[best].vol = vol; voices[best].loop = 0; voices[best].fade = 0;
    return best;
}

/* --- Tesla : voix en boucle (audio_fill tourne dans le thread principal : aucun verrou) --- */
static int play_loop(int s, float vol, float pitch) {     /* renvoie index + 1, 0 si rien */
    int v = play_voice(s, vol, pitch);
    if (v < 0) return 0;
    voices[v].loop = 1;
    return v + 1;
}
static void loop_set(int v, float vol, float pitch) {
    if (v > 0 && voices[v - 1].active && voices[v - 1].loop) { voices[v - 1].vol = vol; voices[v - 1].step = pitch; }
}
static void loop_stop(int v) {                            /* fin en 30 ms, sans clic */
    if (v <= 0 || !voices[v - 1].active || !voices[v - 1].loop) return;
    voices[v - 1].loop = 0;
    voices[v - 1].fade = fmaxf(voices[v - 1].vol, 0.001f) / (0.03f * SR);
}
/* --- fin Tesla --- */


/* n = nombre d'images audio ; out = n paires gauche / droite entrelacees. Les bruitages sont mono (au centre), la musique est stereo */
static void audio_fill(short *out, int n) {
    for (int i = 0; i < n; i++) {
        float ml, mr, acc = 0;
        music_frame(&ml, &mr);
        ml *= MUS_GAIN * musicVol; mr *= MUS_GAIN * musicVol;
        for (int v = 0; v < MAXV; v++) {
            Voice *vo = &voices[v];
            if (!vo->active) continue;
            Sound *s = &sounds[vo->snd];
            if (vo->fade > 0 && (vo->vol -= vo->fade) <= 0) { vo->active = 0; continue; }
            int p = (int)vo->pos;
            if (p >= s->len - 1) {
                if (!vo->loop) { vo->active = 0; continue; }
                vo->pos -= (float)(s->len - 1);           /* boucle : retour au debut */
                p = (int)vo->pos;
            }
            float f = vo->pos - p;
            acc += (s->data[p] * (1 - f) + s->data[p + 1] * f) * vo->vol;
            vo->pos += vo->step;
        }
        out[2 * i] = (short)(tanhf((acc + ml) / 32768.0f * 0.7f * masterVol) * 32000.0f);
        out[2 * i + 1] = (short)(tanhf((acc + mr) / 32768.0f * 0.7f * masterVol) * 32000.0f);
    }
}

static void audio_update(void) {
    if (!audio_ok) return;
    for (int i = 0; i < NBUF; i++) {
#ifdef _WIN32
        if (whdr[i].dwFlags & WHDR_DONE) {
            audio_fill(wbuf[i], BUFLEN);
            waveOutWrite(wout, &whdr[i], sizeof(WAVEHDR));
        }
#else
        if (mac_audio_done(i)) {
            audio_fill(wbuf[i], BUFLEN);
            mac_audio_write(i, wbuf[i]);
        }
#endif
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
    /* --- Scie --- */
    /* lancer : bruit passe-bande qui monte de 800 a 2500 Hz + sinus de 1800 Hz vibre a 40 Hz (rejoue en vol) */
    b = sbuf(0.35f, &n); lp = 0; ph = 0;
    {
        float bp = 0;
        for (int i = 0; i < n; i++) {
            t = (float)i / SR;
            float fc = 800 + 1700 * (t / 0.35f);
            a = 1 - expf(-2 * PI * fc / SR);
            lp += a * (noise() - lp);
            bp += 0.2f * (lp - bp);                       /* passe-bande grossier : passe-haut sur le passe-bas */
            ph += (1800 + 60 * sinf(t * 2 * PI * 40)) / SR;
            b[i] = ((lp - bp) * 1.4f + sinf(ph * 2 * PI) * 0.22f) * expf(-t * 7);
        }
    }
    sounds[S_SAW_THROW] = mksound(b, n);
    /* plantee : choc grave + "twang" metallique module */
    b = sbuf(0.5f, &n); ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        ph += (2400 + 60 * sinf(t * 2 * PI * 30)) / SR;
        b[i] = sinf(t * 2 * PI * 90) * 0.8f * expf(-t * 30) + sinf(ph * 2 * PI) * 0.25f * expf(-t * 6) +
               (t < 0.004f ? noise() * 0.7f : 0);
    }
    sounds[S_SAW_PLANT] = mksound(b, n);
    /* touche : grincement de lame (dent de scie 300 Hz + bruit) */
    b = sbuf(0.25f, &n); ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR; ph += 300.0f / SR;
        float sw = 2 * (ph - floorf(ph)) - 1;
        b[i] = (sw * 0.45f + noise() * 0.35f) * expf(-t * 12);
    }
    sounds[S_SAW_HIT] = mksound(b, n);
    /* rattrapee : clic puis carillon montant */
    b = sbuf(0.15f, &n); ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR; ph += (1200 + 800 * (t / 0.15f)) / SR;
        b[i] = (t < 0.005f ? noise() * 0.6f : 0) + sinf(ph * 2 * PI) * 0.3f * expf(-t * 18);
    }
    sounds[S_SAW_CATCH] = mksound(b, n);
    /* renvoi : "ting" metallique sur une attaque de bruit */
    b = sbuf(0.3f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        b[i] = (sinf(t * 2 * PI * 3200) * 0.3f + sinf(t * 2 * PI * 4800) * 0.15f) * expf(-t * 10) +
               (t < 0.01f ? noise() * 0.5f : 0);
    }
    sounds[S_SAW_BOUNCE] = mksound(b, n);
    /* --- Tesla --- */
    /* boucle de 0,5 s : bourdonnement a 120 Hz (harmoniques 120, 240, 360 : nombre entier de cycles, raccord
     * sans clic) + crepitement (bruit passe-haut hache par des fenetres de 2 a 6 ms, 15 % des tranches de 5 ms) */
    b = sbuf(0.5f, &n); lp = 0;
    {
        int winEnd = -1, slice = (int)(0.005f * SR);
        for (int i = 0; i < n; i++) {
            t = (float)i / SR;
            float hum = sinf(t * 2 * PI * 120) + 0.5f * sinf(t * 2 * PI * 240) + 0.33f * sinf(t * 2 * PI * 360);
            if (i % slice == 0 && i > winEnd && frand() < 0.15f) {
                int len = (int)(frange(0.002f, 0.006f) * SR);
                if (i + len < n) winEnd = i + len;        /* jamais a cheval sur le raccord */
            }
            float x = noise();
            lp += 0.3f * (x - lp);
            b[i] = hum * 0.25f + (i <= winEnd ? (x - lp) * 0.35f : 0);
        }
    }
    sounds[S_TESLA_LOOP] = mksound(b, n);
    /* impulsion qui touche : bruit passe-haut + sinus a 1800 Hz, enveloppe tres courte */
    b = sbuf(0.08f, &n); lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        float x = noise();
        lp += 0.3f * (x - lp);
        b[i] = ((x - lp) * 0.6f + sinf(t * 2 * PI * 1800) * 0.3f) * expf(-t * 60);
    }
    sounds[S_TESLA_ZAP] = mksound(b, n);
    /* --- fin Tesla --- */

    /* --- Lance --- */
    /* lance couchee : glissando metallique 300 -> 900 Hz, deux sinus desaccordes de 1,5 % */
    b = sbuf(0.4f, &n); ph = 0;
    {
        float ph2 = 0;
        for (int i = 0; i < n; i++) {
            t = (float)i / SR;
            float fq = 300 + 600 * (t / 0.4f);
            ph += fq / SR; ph2 += fq * 1.015f / SR;
            float env = t < 0.05f ? t / 0.05f : expf(-(t - 0.05f) * 6);
            b[i] = (sinf(ph * 2 * PI) + sinf(ph2 * 2 * PI)) * 0.22f * env;
        }
    }
    sounds[S_LANCE_COUCH] = mksound(b, n);
    /* estoc : bruit passe-bande a montee rapide ("fsst") */
    b = sbuf(0.15f, &n); lp = 0;
    {
        float bp = 0;
        for (int i = 0; i < n; i++) {
            t = (float)i / SR;
            a = 1 - expf(-2 * PI * 4000 / SR);
            lp += a * (noise() - lp);
            bp += 0.15f * (lp - bp);
            float env = t < 0.02f ? t / 0.02f : expf(-(t - 0.02f) * 22);
            b[i] = (lp - bp) * 1.3f * env;
        }
    }
    sounds[S_LANCE_THRUST] = mksound(b, n);
    /* joute : coup grave 140 -> 60 Hz + cloche 620 / 930 / 1480 Hz + craquement de bois */
    b = sbuf(0.5f, &n); lp = 0; ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        lp += 0.12f * (noise() - lp);
        ph += (140 - 160 * t) / SR;
        b[i] = sinf(ph * 2 * PI) * 0.8f * expf(-t * 10) + lp * 1.6f * expf(-t * 35) +
               (sinf(t * 2 * PI * 620) * 0.25f + sinf(t * 2 * PI * 930) * 0.18f + sinf(t * 2 * PI * 1480) * 0.12f) * expf(-t * 7) +
               (t < 0.03f ? noise() * 0.6f * (1 - t / 0.03f) : 0);
    }
    sounds[S_LANCE_HIT] = mksound(b, n);
    /* choc de joute : deux cloches desaccordees decalees de 30 ms + bruit large */
    b = sbuf(0.8f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        float s = noise() * 0.7f * expf(-t * 25);
        for (int c = 0; c < 2; c++) {
            float dc = t - c * 0.03f, fb = c ? 1.03f : 1.0f;
            if (dc >= 0) s += (sinf(dc * 2 * PI * 880 * fb) * 0.22f + sinf(dc * 2 * PI * 1390 * fb) * 0.15f +
                               sinf(dc * 2 * PI * 2210 * fb) * 0.1f) * expf(-dc * 4.5f);
        }
        b[i] = s;
    }
    sounds[S_LANCE_CLASH] = mksound(b, n);
    /* --- fin Lance --- */

    /* --- Ventouse --- */
    /* lancer : "thump", sinus qui descend de 180 Hz + bruit filtre */
    b = sbuf(0.14f, &n); lp = 0; ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        ph += (180 - 600 * t) / SR;
        lp += 0.1f * (noise() - lp);
        b[i] = (sinf(ph * 2 * PI) * 0.6f + lp * 0.8f) * expf(-t * 30);
    }
    sounds[S_MINE_THROW] = mksound(b, n);
    /* collage : "plop" de ventouse, sinus de 700 a 180 Hz + clic de bruit de 5 ms */
    b = sbuf(0.08f, &n); ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        ph += (180 + 520 * expf(-t * 25)) / SR;
        b[i] = sinf(ph * 2 * PI) * 0.5f * expf(-t * 40) + (t < 0.005f ? noise() * 0.4f : 0);
    }
    sounds[S_MINE_STICK] = mksound(b, n);
    /* bip : carre a 1800 Hz, attaque et relache de 5 ms */
    b = sbuf(0.05f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        float env = fminf(1, fminf(t / 0.005f, (0.05f - t) / 0.005f));
        b[i] = (sinf(t * 2 * PI * 1800) > 0 ? 1.0f : -1.0f) * 0.25f * fmaxf(env, 0);
    }
    sounds[S_MINE_BEEP] = mksound(b, n);
    /* --- fin Ventouse --- */
    /* --- Fleau --- */
    /* lancer : souffle passe-bas dont la coupure monte de 300 a 1800 Hz + cliquetis de chaine (1 impulsion tous les 1/45 s) */
    b = sbuf(0.35f, &n); lp = 0;
    {
        int pulse = -1;
        float amp = 0.4f;
        for (int i = 0; i < n; i++) {
            t = (float)i / SR;
            a = 1 - expf(-2 * PI * (300 + 1500 * t / 0.35f) / SR);
            lp += a * (noise() - lp);
            float env = t < 0.02f ? t / 0.02f : expf(-(t - 0.02f) * 7);
            int k = (int)(t * 45);
            if (k != pulse) { pulse = k; amp = frange(0.3f, 0.6f); }
            float tau = t - k / 45.0f;
            b[i] = lp * 1.1f * env + noise() * amp * expf(-tau * 900) * expf(-t * 6);
        }
    }
    sounds[S_FLAIL_SWING] = mksound(b, n);
    /* touche : choc metallique inharmonique 820 / 1230 / 1960 Hz + coup sourd 70 Hz + bruit bref */
    b = sbuf(0.4f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        b[i] = (sinf(t * 2 * PI * 820) * 0.25f + sinf(t * 2 * PI * 1230) * 0.18f + sinf(t * 2 * PI * 1960) * 0.12f) * expf(-t * 10) +
               sinf(t * 2 * PI * 70) * 0.7f * expf(-t * 18) + noise() * 0.5f * expf(-t * 60);
    }
    sounds[S_FLAIL_HIT] = mksound(b, n);
    /* piece frappee : "ting" 2600 + 3900 Hz + souffle descendant court */
    b = sbuf(0.3f, &n); lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        a = 1 - expf(-2 * PI * fmaxf(200, 2500 - 8000 * t) / SR);
        lp += a * (noise() - lp);
        b[i] = (sinf(t * 2 * PI * 2600) * 0.3f + sinf(t * 2 * PI * 3900) * 0.18f) * expf(-t * 14) + lp * 0.5f * expf(-t * 20);
    }
    sounds[S_FLAIL_COIN] = mksound(b, n);
    /* --- fin Fleau --- */
    /* --- Poings --- */
    /* touche : bruit passe-bas tres bref + sinus 180 -> 120 Hz (plus sec et plus aigu que S_PUNCH) */
    b = sbuf(0.12f, &n); lp = 0; ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        lp += 0.25f * (noise() - lp);
        ph += (180 - 500 * t) / SR;
        b[i] = lp * 1.3f * expf(-t * 45) + sinf(ph * 2 * PI) * 0.7f * expf(-t * 25);
    }
    sounds[S_FIST_HIT] = mksound(b, n);
    /* uppercut : sinus montant 120 -> 420 Hz + claquement de bruit */
    b = sbuf(0.3f, &n); ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        ph += (120 + 1000 * t) / SR;
        b[i] = sinf(ph * 2 * PI) * 0.6f * expf(-t * 8) + noise() * 0.5f * expf(-t * 50);
    }
    sounds[S_FIST_UPPER] = mksound(b, n);
    /* punch-jump : "whoomp", sinus 70 -> 160 Hz + souffle filtre dont la coupure monte de 300 a 2500 Hz */
    b = sbuf(0.35f, &n); lp = 0; ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        ph += (70 + 257 * t) / SR;
        a = 1 - expf(-2 * PI * (300 + 2200 * t / 0.35f) / SR);
        lp += a * (noise() - lp);
        float env = t < 0.03f ? t / 0.03f : expf(-(t - 0.03f) * 8);
        b[i] = sinf(ph * 2 * PI) * 0.9f * expf(-t * 9) + lp * 0.6f * env;
    }
    sounds[S_FIST_PJ] = mksound(b, n);
    /* --- fin Poings --- */
    /* --- Egide --- */
    /* levee : souffle passe-bas tres court + clic sinus 900 Hz */
    b = sbuf(0.08f, &n); lp = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        lp += 0.15f * (noise() - lp);
        b[i] = lp * 1.2f * expf(-t * 60) + sinf(t * 2 * PI * 900) * 0.35f * expf(-t * 90);
    }
    sounds[S_SHIELD_UP] = mksound(b, n);
    /* blocage : "clang" 520 / 780 / 1170 Hz, plus grave que le parry, + bruit d'impact */
    b = sbuf(0.5f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        b[i] = (sinf(t * 2 * PI * 520) * 0.3f + sinf(t * 2 * PI * 780) * 0.2f + sinf(t * 2 * PI * 1170) * 0.12f) * expf(-t * 9) +
               noise() * 0.5f * expf(-t * 70);
    }
    sounds[S_SHIELD_BLOCK] = mksound(b, n);
    /* casse : bruit large + sinus qui glisse de 400 a 120 Hz */
    b = sbuf(0.6f, &n); ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        ph += (400 - 280 * fminf(t / 0.6f, 1)) / SR;
        b[i] = noise() * 0.45f * expf(-t * 8) + sinf(ph * 2 * PI) * 0.4f * expf(-t * 5);
    }
    sounds[S_SHIELD_BREAK] = mksound(b, n);
    /* coup de bouclier : coup sourd 110 -> 60 Hz + partiel metallique 640 Hz */
    b = sbuf(0.3f, &n); ph = 0;
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        ph += (60 + 50 * expf(-t * 20)) / SR;
        b[i] = sinf(ph * 2 * PI) * 0.8f * expf(-t * 14) + sinf(t * 2 * PI * 640) * 0.25f * expf(-t * 12) +
               noise() * 0.3f * expf(-t * 80);
    }
    sounds[S_SHIELD_BASH] = mksound(b, n);
    /* --- fin Egide --- */
    /* tir a la tete : tintement metallique aigu (2,6 et 3,9 kHz) sur un claquement, plus long que le hitmarker */
    b = sbuf(0.3f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        b[i] = (sinf(t * 2 * PI * 2600) + 0.55f * sinf(t * 2 * PI * 3900) + 0.25f * sinf(t * 2 * PI * 5200)) * 0.3f * expf(-t * 13) +
               noise() * 0.35f * expf(-t * 120);
    }
    sounds[S_HEADSHOT] = mksound(b, n);
    /* chaine de tirs pleine : deux notes montantes (quinte), son de verrou */
    b = sbuf(0.35f, &n);
    for (int i = 0; i < n; i++) {
        t = (float)i / SR;
        float f = t < 0.09f ? 1320.0f : 1980.0f, ta = t < 0.09f ? t : t - 0.09f;
        b[i] = (sinf(t * 2 * PI * f) + 0.35f * sinf(t * 4 * PI * f)) * 0.22f * expf(-ta * 10) * fminf(1, ta * 400);
    }
    sounds[S_CHAIN_FULL] = mksound(b, n);
}

static void audio_init(void) {
#ifndef _WIN32
    if (!mac_audio_open(SR, NBUF, BUFLEN)) return;
    audio_ok = 1;
    for (int i = 0; i < NBUF; i++) {
        audio_fill(wbuf[i], BUFLEN);
        mac_audio_write(i, wbuf[i]);
    }
    return;
#else
    WAVEFORMATEX fmt = { 0 };
    fmt.wFormatTag = WAVE_FORMAT_PCM; fmt.nChannels = 2; fmt.nSamplesPerSec = SR;
    fmt.wBitsPerSample = 16; fmt.nBlockAlign = 4; fmt.nAvgBytesPerSec = SR * 4;
    if (waveOutOpen(&wout, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return;
    audio_ok = 1;
    for (int i = 0; i < NBUF; i++) {
        memset(&whdr[i], 0, sizeof(WAVEHDR));
        whdr[i].lpData = (LPSTR)wbuf[i];
        whdr[i].dwBufferLength = BUFLEN * 2 * sizeof(short);
        waveOutPrepareHeader(wout, &whdr[i], sizeof(WAVEHDR));
        audio_fill(wbuf[i], BUFLEN);
        waveOutWrite(wout, &whdr[i], sizeof(WAVEHDR));
    }
#endif
}
