/* VELOCITY - mode classe : etat, cote Elo, paliers, identite de classement
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Mode classe : la cote (Elo) est tenue par le serveur de classement  */
/* (worker/, Durable Object Ratings), qui la calcule a partir des      */
/* rapports de fin de match de l'hote et des clients. Le jeu garde un  */
/* cache d'affichage dans velocity.ini et applique la meme formule     */
/* (docs/CODE.md, "Mode classe") pour ses tests et pour l'estimation   */
/* affichee en attendant la reponse du serveur.                        */
/* Une partie classee est toujours en chacun pour soi, mode FRAGS,     */
/* regles par defaut, sans bots, sur une carte sans portes de course.  */
/* Suivi du match et rapport : src/reseau/classe_match.c ; requetes    */
/* au serveur : src/reseau/classe_serveur.c ; HTTP : plateforme/http.c */
/* ------------------------------------------------------------------ */
static char rkId[33], rkKey[33];                          /* identite sans compte (velocity.ini : ranked_id, ranked_key) */
static int myMmr = RANK_START, rankPlayed, rankWins;      /* cache (velocity.ini : mmr, ranked_played, ranked_wins) */
static int rkRank;                                        /* mon rang au classement (0 = en placement ou inconnu) */
static int ranked;                                        /* la partie en cours est classee (hote : decide ; client : PkWelcome) */
static int rankedWant, hostRanked;                        /* PARTIE CLASSEE en recherche / prochaine partie hebergee classee */
static int rkMmr[MAXP + 1], rkPlay[MAXP + 1];             /* cote et matchs annonces par chaque joueur (PkJoin -> PkRoster) */
static unsigned char rkRid[MAXP + 1][16];                 /* identite de classement de chaque joueur (zeros = aucune) */
static unsigned char rkMid[8];                            /* identifiant du match classe en cours (tire par l'hote) */
static unsigned char rkLeft[MAXP][16];                    /* joueurs partis pendant le match (liste de l'hote, diffusee) */
static int rkLeftN;
static int rkSeen[MAXP], rkHadOpp, rkPrevOver;            /* suivi du match : presents, adversaire vu, fin deja vue */
static float rkEndWait = -1;                              /* compte a rebours avant le rapport de fin */
enum { RS_NONE, RS_PENDING, RS_DONE, RS_FAILED };
static int rkRepState, rkRepTries, rkLastDelta, rkLastMmr, rkLastPlayed, rkEstimate;
static double rkRepRetryT;
static char rkRepBody[1600];                              /* dernier rapport envoye (renvoye tant qu'il est en attente) */
typedef struct { char name[16]; int mmr, played; char tier[24]; } RkTop;
static RkTop rkTop[3];
static int rkTopN, rkTopState, rkMeRank;                  /* rkTopState : 0 chargement, 1 pret, 2 indisponible */
static int rkOnline, rkHelloBusy, rkAfterHello;           /* serveur joint (hello reussi) ; hello en cours ; PARTIE CLASSEE en attente */
static double rkHelloAt;                                  /* hello + top a relancer a cet instant (0 = non) */
static const char *rankNames[RANK_TIERS] = { "BRONZE", "ARGENT", "OR", "PLATINE", "DIAMANT", "MAITRE" };
static const int rankFloor[RANK_TIERS] = { 0, RANK_ARGENT, RANK_OR, RANK_PLATINE, RANK_DIAMANT, RANK_MAITRE };

static int rank_clamp(int m) { return m < RANK_MIN ? RANK_MIN : m > RANK_MAX ? RANK_MAX : m; }
static float rank_expect(float ra, float rb) { return 1.0f / (1.0f + powf(10.0f, (rb - ra) / 400.0f)); }
static float rank_k(int played) { return played < RANK_PLACEMENT ? RANK_K_PLACE : RANK_K; }
/* score de a contre b : le gagnant designe par l'hote passe devant, puis les frags ; egalite = 0,5 */
static float rank_score(int ka, int kb, int wa, int wb) {
    if (wa != wb) return wa ? 1.0f : 0.0f;
    return ka > kb ? 1.0f : ka < kb ? 0.0f : 0.5f;
}
/* variation de cote contre n adversaires (cotes opp[], scores sc[]) : K / n * somme(S - E), arrondie */
static int rank_delta(int me, int played, int n, const int *opp, const float *sc) {
    if (n <= 0) return 0;
    float s = 0;
    for (int j = 0; j < n; j++) s += sc[j] - rank_expect((float)me, (float)opp[j]);
    return (int)lroundf(rank_k(played) * s / n);
}
static int rank_tier(int m) {
    int t = 0;
    while (t + 1 < RANK_TIERS && m >= rankFloor[t + 1]) t++;
    return t;
}
static int rank_div(int m) {                              /* division III (bas du palier) a I (haut) ; 0 pour MAITRE */
    int t = rank_tier(m);
    if (t == RANK_TIERS - 1) return 0;
    int lo = t ? rankFloor[t] : rankFloor[1] - (rankFloor[2] - rankFloor[1]), hi = rankFloor[t + 1];
    int d = 3 - (int)(3.0f * (float)(m - lo) / (float)(hi - lo));
    return d < 1 ? 1 : d > 3 ? 3 : d;
}
static void rank_name(int m, char *out, int n) {          /* "OR II", "MAITRE" */
    static const char *roman[4] = { "", "I", "II", "III" };
    int d = rank_div(m);
    snprintf(out, n, "%s%s%s", rankNames[rank_tier(m)], d ? " " : "", roman[d]);
}
static void rank_label(int m, int played, char *out, int n) {   /* palier, ou PLACEMENT x/10 pendant les premiers matchs */
    if (played < RANK_PLACEMENT) snprintf(out, n, "PLACEMENT %d/%d", played, RANK_PLACEMENT);
    else rank_name(m, out, n);
}
static const float *rank_col(int m) {                     /* couleur du palier */
    static const float c[RANK_TIERS][3] = { { 0.8f, 0.5f, 0.3f }, { 0.75f, 0.8f, 0.85f }, { 1.0f, 0.82f, 0.3f },
                                            { 0.45f, 0.95f, 0.85f }, { 0.55f, 0.75f, 1.0f }, { 1.0f, 0.45f, 0.85f } };
    return c[rank_tier(m)];
}

static void hex_of(const unsigned char *b, int n, char *out) {
    for (int i = 0; i < n; i++) snprintf(out + 2 * i, 3, "%02x", b[i]);
    out[2 * n] = 0;
}
static int hex_ok(const char *s, int n) {                 /* exactement n chiffres hexadecimaux minuscules */
    for (int i = 0; i < n; i++) if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return 0;
    return s[n] == 0;
}
static void bin_of(const char *hex, unsigned char *b, int n) {
    for (int i = 0; i < n; i++) {
        unsigned int v = 0;
        sscanf(hex + 2 * i, "%2x", &v);
        b[i] = (unsigned char)v;
    }
}
static int rid_set(const unsigned char *r) { for (int i = 0; i < 16; i++) if (r[i]) return 1; return 0; }
#ifdef _WIN32
BOOLEAN WINAPI SystemFunction036(PVOID buf, ULONG len);  /* RtlGenRandom (advapi32) : aleatoire du systeme */
#endif
static void rk_random(unsigned char *b, int n) {
    int ok;
#ifdef _WIN32
    ok = SystemFunction036(b, (ULONG)n) != 0;
#else
    arc4random_buf(b, (size_t)n);
    ok = 1;
#endif
    if (!ok) {                                            /* secours : horloges (ne devrait jamais servir) */
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        srand((unsigned)t.QuadPart ^ (unsigned)GetTickCount());
        for (int i = 0; i < n; i++) b[i] = (unsigned char)(rand() >> 3);
    }
}
/* identite de classement : 128 bits d'identifiant + 128 bits de cle secrete ; 1 si elle vient d'etre creee */
static int rk_identity(void) {
    unsigned char b[16];
    if (hex_ok(rkId, 32) && hex_ok(rkKey, 32)) return 0;
    rk_random(b, 16); hex_of(b, 16, rkId);
    rk_random(b, 16); hex_of(b, 16, rkKey);
    return 1;
}
enum { RQ_HELLO = 1, RQ_TOP, RQ_REPORT,                  /* requetes au serveur de classement */
       RQ_SR_START, RQ_SR_REPORT, RQ_SR_TOP };            /* speedrun : ticket, temps, classement (speedrun_serveur.c) */
static int rkFragsTest;                                   /* --frags N : limite de frags d'un match classe de bots (tests) */
static void rk_new_match(void) {                          /* hote : un nouveau match classe commence */
    rk_random(rkMid, 8);
    rkLeftN = 0;
    memset(rkLeft, 0, sizeof rkLeft);
}

/* regles imposees : une carte classee n'est jamais une carte CHALLENGE (LA TOILE : course seulement) */
static int rk_map_ok(int m) { return map_pvp(m); }
static int rk_map_pick(int m) {                           /* la carte demandee si elle convient, sinon la suivante qui convient */
    for (int k = 0; k < NMAPS; k++) if (rk_map_ok((m + k) % NMAPS)) return (m + k) % NMAPS;
    return 0;
}
/* la partie en cours compte-t-elle pour le classement ? (classee, chacun pour soi, mode FRAGS) */
static int rk_counts(void) { return ranked && teamSize <= 0 && rules.mode == MODE_FRAGS; }
