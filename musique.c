/* musique.c : ecoute rapide du design musical (music.h) sans lancer le jeu.
 *
 *   musique.exe [piste] [niveau]                      joue en direct
 *        touches : 0 a 8 = niveau d'intensite, + / - = niveau suivant / precedent,
 *                  P = piste suivante, Q = quitter
 *   musique.exe --wav sortie.wav [piste] [niveau] [secondes]   ecrit un fichier WAV
 *
 * Compiler : build_music.bat. Meme mixage que le jeu (volume musique 80 %, volume general 100 %). */
#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#pragma comment(lib, "winmm.lib")

#include "music.h"

#define BUFLEN 2048
#define NBUF   4

static short pcm(void) {                                  /* identique a audio_fill (sans les effets sonores) */
    float acc = music_sample() * MUS_GAIN * musicVol;
    return (short)(tanhf(acc / 32768.0f * 0.7f) * 32000.0f);
}

static void write_wav(const char *path, int track, int lvl, float secs) {
    int n = (int)(secs * SR);
    FILE *f = fopen(path, "wb");
    if (!f) { printf("impossible d'ecrire %s\n", path); return; }
    unsigned data = n * 2, riff = 36 + data, rate = SR, brate = SR * 2, fmtLen = 16;
    unsigned short pcmTag = 1, ch = 1, align = 2, bits = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fmtLen, 4, 1, f);
    fwrite(&pcmTag, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&brate, 4, 1, f);
    fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
    music_set(track, lvl);
    for (int i = 0; i < n; i++) { short s = pcm(); fwrite(&s, 2, 1, f); }
    fclose(f);
    printf("%s : piste %d (%s), niveau %d, %.1f s\n", path, track, tracks[track].name, lvl, secs);
}

int main(int argc, char **argv) {
    int track = 0, lvl = 0;
    if (argc >= 3 && !strcmp(argv[1], "--wav")) {
        if (argc >= 4) track = atoi(argv[3]);
        if (argc >= 5) lvl = atoi(argv[4]);
        if (track < 0 || track >= NTRACKS) track = 0;
        write_wav(argv[2], track, lvl, argc >= 6 ? (float)atof(argv[5]) : 20.0f);
        return 0;
    }
    if (argc >= 2) track = atoi(argv[1]);
    if (argc >= 3) lvl = atoi(argv[2]);
    if (track < 0 || track >= NTRACKS) track = 0;

    HWAVEOUT wout; WAVEFORMATEX fmt = { 0 };
    static WAVEHDR hdr[NBUF]; static short buf[NBUF][BUFLEN];
    fmt.wFormatTag = WAVE_FORMAT_PCM; fmt.nChannels = 1; fmt.nSamplesPerSec = SR;
    fmt.wBitsPerSample = 16; fmt.nBlockAlign = 2; fmt.nAvgBytesPerSec = SR * 2;
    if (waveOutOpen(&wout, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) { printf("pas de sortie audio\n"); return 1; }
    music_set(track, lvl);
    for (int i = 0; i < NBUF; i++) {
        hdr[i].lpData = (LPSTR)buf[i]; hdr[i].dwBufferLength = BUFLEN * 2;
        waveOutPrepareHeader(wout, &hdr[i], sizeof(WAVEHDR));
        for (int k = 0; k < BUFLEN; k++) buf[i][k] = pcm();
        waveOutWrite(wout, &hdr[i], sizeof(WAVEHDR));
    }
    printf("piste %d (%s) niveau %d   [0-8 niveau, +/- , P piste, Q quitter]\n", track, tracks[track].name, lvl);
    for (;;) {
        for (int i = 0; i < NBUF; i++)
            if (hdr[i].dwFlags & WHDR_DONE) {
                for (int k = 0; k < BUFLEN; k++) buf[i][k] = pcm();
                waveOutWrite(wout, &hdr[i], sizeof(WAVEHDR));
            }
        if (_kbhit()) {
            int c = _getch(), want = mus.lvlWant;
            if (c == 'q' || c == 'Q') break;
            if (c >= '0' && c <= '8') want = c - '0';
            if (c == '+') want++;
            if (c == '-') want--;
            if (c == 'p' || c == 'P') { track = (track + 1) % NTRACKS; music_set(track, want); printf("piste %d (%s)\n", track, tracks[track].name); }
            else { music_level(want); printf("niveau %d (applique a la mesure suivante)\n", mus.lvlWant); }
        }
        Sleep(5);
    }
    waveOutReset(wout); waveOutClose(wout);
    return 0;
}
