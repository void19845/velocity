/* VELOCITY - touches physiques, actions A_*, touches par defaut
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Entrees                                                             */
/* ------------------------------------------------------------------ */
enum { SC_ESC = 0x01, SC_1 = 0x02, SC_2 = 0x03, SC_W = 0x11, SC_E = 0x12, SC_R = 0x13, SC_LCTRL = 0x1D,
       SC_A = 0x1E, SC_S = 0x1F, SC_D = 0x20, SC_LSHIFT = 0x2A, SC_C = 0x2E, SC_RSHIFT = 0x36,
       SC_SPACE = 0x39, SC_F2 = 0x3C, SC_F3 = 0x3D, SC_F11 = 0x57, SC_F5 = 0x3F, SC_F = 0x21, SC_V = 0x2F, SC_G = 0x22 };
/* touches physiques (scancodes 0-255) + souris (256+) : etat tenu et appui du tick */
enum { K_LMB = 256, K_RMB, K_MMB, K_X1, K_X2, K_WHEEL, K_COUNT };
static unsigned char keys[K_COUNT], latch[K_COUNT];

/* Actions du jeu, chacune liee a deux touches au plus (0 = aucune). Le jeu ne lit que les actions. */
enum { A_FWD, A_BACK, A_LEFT, A_RIGHT, A_JUMP, A_CROUCH, A_DASH, A_FIRE, A_MELEE, A_HOOK, A_SKILL,
       A_WEAP1, A_WEAP2, A_SWAP, A_SCORES,
       A_DETONATE_ALL, A_DETONATE_ONE,                    /* Ventouse : toutes mes mines / la plus ancienne */
       A_FUSION,                                          /* VAGUES : active l'arme hybride (jauge de fusion pleine) */
       A_COUNT };
static const char *actId[A_COUNT] = { "avancer", "reculer", "gauche", "droite", "sauter", "accroupi", "dash", "tirer",
                                      "epee", "grappin", "piece", "arme1", "arme2", "changer_arme", "scores",
                                      "detoner_tout", "detoner_une", "fusion" };
static const char *actLabel[A_COUNT] = { "Avancer", "Reculer", "Gauche", "Droite", "Sauter (maintenir = bhop)",
                                         "S'accroupir / slide", "Dash", "Tirer", "Epee / parry", "Grappin (maintenir)",
                                         "Lancer une piece", "Railgun", "Roquettes", "Changer d'arme", "Tableau des scores",
                                         "Detoner toutes les mines", "Detoner la plus ancienne", "Fusion (arme hybride)" };
static const int bindDefault[A_COUNT][2] = {
    { 0x11, 0 }, { 0x1F, 0 }, { 0x1E, 0 }, { 0x20, 0 }, { 0x39, 0 }, { 0x1D, 0x2E }, { 0x2A, 0x12 },
    { K_LMB, 0 }, { K_RMB, 0x2F }, { 0x21, K_MMB }, { 0x22, K_X1 }, { 0x02, 0 }, { 0x03, 0 }, { K_WHEEL, 0 },
    { 0x0F, 0 },
    { K_X2, 0x10 }, { 0x2D, 0 },                          /* Ventouse : pouce arriere + Q/A physique ; X */
    { 0x14, 0 },                                          /* Fusion : T physique (libre ; R reste le redemarrage) */
};
static int binds[A_COUNT][2];
static unsigned char act[A_COUNT], actP[A_COUNT];          /* tenu / appuye pendant ce tick */
static unsigned char botHeld[A_COUNT], botPress[A_COUNT];  /* ce que les bots "appuient" */

/* reglages */
static float fovBase = BASE_HFOV;
static int secondScreen;                                  /* carte de la partie sur un deuxieme ecran s'il y en a un (opt-in) */
static int mapView3d;                                     /* carte du 2e ecran : 0 vue de dessus, 1 en 3D inclinee */
static int invertY, showFps = 1;
static int impactFrames = 1, screenFx = 1;               /* options : impact frames (flashs) et effets d'ecran */
static int gfxShadows = 1, gfxBloom = 1;                  /* graphismes : ombres des objets mobiles, halo lumineux (bloom) */
static int gfxQuality = 2, gfxAmbiance;                   /* qualite (0 basse, 1 moyenne, 2 ultra) ; ambiance (graphismes/ambiance_*.ini) */
static const char *qualityNames[3] = { "BASSE", "MOYENNE", "ULTRA" };
static const char *ambianceNames[4] = { "COUCHANT", "NUIT", "JOUR", "ORAGE" };
