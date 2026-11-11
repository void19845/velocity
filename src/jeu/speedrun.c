/* VELOCITY - speedrun : categories de classement, planchers de temps, speedrun complet (toutes les cartes CHALLENGE)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). Apres src/jeu/course.c. */

/* ------------------------------------------------------------------ */
/* Classements de temps, tenus par le serveur (worker/src/speedrun.js) */
/* avec l'identite du mode classe (ranked_id / ranked_key) :           */
/*   SR_MAP0 + m   course de la carte CHALLENGE m ("course:la_toile")  */
/*   SR_COMPLET    toutes les cartes CHALLENGE a la suite ("complet:N") */
/*   SR_VAGUES100  mode VAGUES jusqu'a la vague 100 ("vagues100")      */
/* Un run : speedrun_begin (ticket) puis speedrun_report (temps), voir */
/* src/reseau/speedrun_serveur.c. Speedrun complet : chrono global =   */
/* somme des temps de course de chaque carte (les decomptes de depart  */
/* ne comptent pas), temps intermediaire a chaque arrivee, compare au  */
/* record perso (velocity_speedrun.txt a cote de velocity.ini).        */
/* Affichage : src/interface/course_hud.c, menu CHALLENGE : menus.c.   */
/* ------------------------------------------------------------------ */
#define SR_NCAT       (SR_MAP0 + NMAPS)
#define SR_VMAX       45.0f                               /* plancher : vitesse moyenne plafond (m/s) de porte en porte */
#define SR_FLOOR_VAGUES (5 * 60 * 1000)                   /* plancher de la vague 100 (ms), provisoire */
#define SR_NEXT_DELAY 2.0f                                /* speedrun complet : arrivee affichee avant la carte suivante */
#define SR_TOPN       10

static int srMaps[NMAPS], srMapN;                         /* cartes CHALLENGE a portes, dans l'ordre de maps[] */
static int srIdx, srDone, srNewBest, srPbLoaded;          /* speedrun complet : carte en cours, fini, record (1 premier, 2 battu) */
static float srNextT;                                     /* > 0 : carte suivante dans srNextT secondes */
static int srSplit[NMAPS];                                /* temps cumules (ms) a l'arrivee de chaque carte */
static int srPb[NMAPS], srPbN;                            /* record perso du speedrun complet : temps cumules */
static int srCmp[NMAPS], srCmpN;                          /* le record perso au depart du run : les ecarts s'y comparent */
static int challengeOpen, srSel = -1;                     /* ecran CHALLENGE ouvert ; categorie dont on voit le classement */

/* reponses du serveur, par categorie (remplies par src/reseau/speedrun_serveur.c) */
enum { SRR_NONE, SRR_WAIT, SRR_SENT, SRR_RECORD, SRR_KEPT, SRR_REFUSED, SRR_OFFLINE };
typedef struct {
    char run[17]; int seq;                                /* ticket du run en cours (vide : pas encore / refuse) */
    int state, ms, n, split[RACE_GATES];                  /* dernier rapport (SRR_*) et son contenu */
    int rank, best, prev; char err[40];                   /* reponse : rang, meilleur temps, ancien record */
} SrRun;
typedef struct {
    int state;                                            /* 0 jamais demande, 1 chargement, 2 pret, 3 indisponible */
    int n, runners; char name[SR_TOPN][16]; int ms[SR_TOPN];
    char wrName[16]; int wrMs, wrN, wrSplit[RACE_GATES];  /* record du monde et ses temps intermediaires */
    int meRank, meMs, meN, meSplit[RACE_GATES];           /* mon meilleur temps au serveur */
} SrTop;
static SrRun srRun[SR_NCAT];
static SrTop srTop[SR_NCAT];

static void sr_maps_init(void) {
    if (srMapN) return;
    for (int m = 0; m < NMAPS; m++) if (maps[m].challenge && maps[m].ngates > 0) srMaps[srMapN++] = m;
}
static int sr_cat_ok(int cat) {
    if (cat == SR_COMPLET || cat == SR_VAGUES100) return 1;
    return cat >= SR_MAP0 && cat < SR_NCAT && maps[cat - SR_MAP0].challenge && maps[cat - SR_MAP0].ngates > 0;
}
static void map_slug(int m, char *out, int n) {           /* "LA TOILE" -> "la_toile" */
    int k = 0;
    for (const char *p = maps[m].name; *p && k < n - 1; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z') out[k++] = (char)(c - 'A' + 'a');
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out[k++] = c;
        else if (c == ' ' && k && out[k - 1] != '_') out[k++] = '_';
    }
    out[k] = 0;
}
static void sr_key(int cat, char *out, int n) {           /* nom de la categorie au serveur */
    char slug[32];
    if (cat == SR_COMPLET) { sr_maps_init(); snprintf(out, n, "complet:%d", srMapN); }   /* le nombre de cartes : un nouveau parcours = un nouveau classement */
    else if (cat == SR_VAGUES100) snprintf(out, n, "vagues100");
    else { map_slug(cat - SR_MAP0, slug, sizeof slug); snprintf(out, n, "course:%s", slug); }
}
static const char *sr_label(int cat) {                    /* nom a l'ecran */
    if (cat == SR_COMPLET) return "SPEEDRUN COMPLET";
    if (cat == SR_VAGUES100) return "VAGUE 100";
    return maps[cat - SR_MAP0].name;
}
/* plancher d'une course : de porte en porte (du depart a la 1re), chaque troncon raccourci du rayon de la porte
 * d'arrivee, a SR_VMAX. Recopie dans worker/src/speedrun.js (SR.CATS.<cat>.floorMs) : ligne de test "planchers" */
static int sr_floor_ms(int cat) {
    if (cat == SR_VAGUES100) return SR_FLOOR_VAGUES;
    if (cat == SR_COMPLET) {
        int s = 0;
        sr_maps_init();
        for (int k = 0; k < srMapN; k++) s += sr_floor_ms(SR_MAP0 + srMaps[k]);
        return s;
    }
    const MapDef *md = &maps[cat - SR_MAP0];
    V3 a = vadd(md->start, v3(0, 0.9f, 0));
    float d = 0;
    for (int i = 0; i < md->ngates; i++) {
        V3 b = v3(md->gates[i][0], md->gates[i][1], md->gates[i][2]);
        d += fmaxf(0, vlen(vsub(b, a)) - md->gates[i][3]);
        a = b;
    }
    return (int)(d / SR_VMAX * 1000);
}
static void sr_time_str(char *b, int n, int ms) {         /* 1:02.37 (ms arrondies au centieme) */
    int cs = (ms + 5) / 10;
    snprintf(b, n, "%d:%02d.%02d", cs / 6000, (cs / 100) % 60, cs % 100);
}

/* record perso du speedrun complet : velocity_speedrun.txt, a cote de velocity.ini (src/interface/course_hud.c) */
static void sr_pb_save(void);
static void sr_pb_load(void);

/* --- speedrun complet --- */
static void sr_full_start(void) {                         /* depart (ou R) : premiere carte, chrono global a zero */
    sr_maps_init();
    if (!srMapN) return;
    if (!srPbLoaded) sr_pb_load();
    srFull = 1; srIdx = 0; srDone = 0; srNewBest = 0; srNextT = 0;
    memcpy(srCmp, srPb, sizeof srPb); srCmpN = srPbN;
    memset(srSplit, 0, sizeof srSplit);
    speedrun_begin(SR_COMPLET);
    race_start_map(srMaps[0]);
}
static int sr_full_ms(void) {                             /* chrono global (ms) */
    if (srDone) return srSplit[srMapN - 1];
    return (srIdx ? srSplit[srIdx - 1] : 0) + (raceDone ? srSplit[srIdx] - (srIdx ? srSplit[srIdx - 1] : 0) : (int)lroundf(raceT * 1000));
}
static void sr_full_map_done(int ms) {                    /* arrivee sur la carte srIdx (appele par race_tick) */
    srSplit[srIdx] = (srIdx ? srSplit[srIdx - 1] : 0) + ms;
    if (srIdx + 1 < srMapN) { srNextT = SR_NEXT_DELAY; return; }
    srDone = 1;
    int total = srSplit[srMapN - 1];
    if (srPbN != srMapN || total < srPb[srMapN - 1]) {
        srNewBest = srPbN == srMapN ? 2 : 1;
        memcpy(srPb, srSplit, sizeof srSplit[0] * (size_t)srMapN);
        srPbN = srMapN;
        sr_pb_save();
    }
    speedrun_report_splits(SR_COMPLET, total, srSplit, srMapN);
}
static void sr_full_tick(float dt) {                      /* apres race_tick : enchainement vers la carte suivante */
    if (!srFull || !raceOn || srNextT <= 0) return;
    srNextT -= dt;
    if (srNextT <= 0) { srNextT = 0; srIdx++; race_start_map(srMaps[srIdx]); }
}
/* ecart (ms) au record perso a l'arrivee de la carte k du speedrun complet ; 0 sans record */
static int sr_full_delta(int k) { return srCmpN == srMapN && k < srCmpN ? srSplit[k] - srCmp[k] : 0; }
