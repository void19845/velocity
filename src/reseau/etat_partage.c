/* VELOCITY - ce que le jeu doit savoir du reseau avant le reseau : joueurs NP, equipes, declarations
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Multijoueur LAN : etat partage                                      */
/* ------------------------------------------------------------------ */
#define MAXP          8
#define MAXSPEC       4       /* spectateurs en direct sur l'hote (28 Ko/s chacun) */
#define NET_PORT      27960
#define NET_TIMEOUT    5.0    /* silence (s) au-dela duquel un joueur ou l'hote est considere comme parti */
#define NET_LOAD_GRACE 10.0   /* delai en plus pendant le chargement d'une carte (arrivee, changement de carte) :
                                 load_map est synchrone, le joueur n'envoie rien pendant ce temps */
#define FRAG_LIMIT    15      /* frags pour gagner : partie hebergee a la main ou en LAN */
#define FRAG_LIMIT_QUICK 7    /* partie rapide : l'hote l'annonce a tous dans PkSnap.fragLimit */
#define MATCH_TIME    600.0f
#define RESPAWN_TIME  2.5f
#define PVP_SCALE     0.7f    /* degats contre les joueurs : 70% de ceux contre les monstres */
#define KILL_HEAL     30.0f

enum { NET_OFF, NET_HOST, NET_CLIENT };
enum { APP_MENU, APP_JOIN, APP_GAME };
enum { W_RAIL, W_ROCKET, W_MELEE, W_HOOK, W_PARRY, W_SELF, W_COIN, W_SLAM, W_LANCE, W_FLAIL, W_FIST,
       W_SHIELD, W_SHOTGUN, W_KINETIC, W_SAW, W_MINE, W_TESLA, W_VOID, W_COUNT };   /* W_VOID : chute sous killY */
enum { NF_CROUCH = 1, NF_SLIDE = 2, NF_WALLRUN = 4, NF_HOOK = 8, NF_DEAD = 16, NF_PARRY = 32 };

typedef struct {                  /* ce que chaque joueur envoie de lui-meme */
    float pos[3], vel[3], hookPos[3];
    float yaw, pitch;
    unsigned char flags, weapon, melee, pad;
    unsigned char skin, spare[3];                         /* skin (SKN_*) ; spare : libre */
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
    /* Tesla : hote = derniere decharge renvoyee vers ce tireur ; client = dernier EV_HIT W_TESLA de ce tireur sur moi
     * (verrou visuel) ; prochain recalcul de son arc ; son arc touche-t-il quelqu'un (hauteur du son) */
    double teslaReflT, teslaHitMeT;
    float teslaVisT;
    int teslaVisHit;
    double lastLanceT;                                    /* Lance : derniere fois vu lance couchee (hote) */
    unsigned char lanceSeen;                              /* Lance : ST_LANCE au tick precedent (front montant -> son) */
    unsigned char team;                                   /* equipe 0 (bleue) ou 1 (rouge), si teamSize > 0 */
    float runPh, trailAcc;                                /* skins : phase de course ; trainee de vitesse */
    V3 trail[16];
    float trailAge[16];
    int trailHead;
    /* --- Egide : derniere fois vu bouclier leve (hote) ; jauge, brise, arrache (hote : autorite ; clients : miroir) --- */
    double lastShieldT;
    float shieldGauge, shieldBrokenT, shieldRipT;
    int lastHitBy; double lastHitT;                       /* kill zone (hote) : dernier attaquant et heure de sa touche */
    struct sockaddr_in addr;      /* hote seulement */
} NetPlayer;

static NetPlayer NP[MAXP + 1];                            /* NP[MAXP] : place vide du spectateur (myId = MAXP) */
static int netMode, appState, myId, dmMode, matchOver, matchWinner;
static int fragLimit = FRAG_LIMIT;                        /* limite de la partie en cours (l'hote decide, les clients la recoivent) */
/* --- equipes : 0 = chacun pour soi, 1 a 4 = NvN (l'hote decide, les clients le recoivent dans PkSnap) --- */
#define TEAM_MAX      4       /* 4 contre 4 au plus (MAXP = 8) */
#define TEAM_FRAG_BASE 10     /* limite d'equipe = TEAM_FRAG_BASE + TEAM_FRAG_PER x joueurs par equipe */
#define TEAM_FRAG_PER  5
#define TEAM_DRAW      2      /* matchWinner en equipes : 0, 1, ou egalite */
static int teamSize;                                      /* mode de la partie en cours */
static int wantTeams;                                     /* mode choisi au menu pour la prochaine partie hebergee */
static const char *teamNames[2] = { "BLEUE", "ROUGE" };
static const float teamCol[2][3] = { { 0.35f, 0.65f, 1.0f }, { 1.0f, 0.38f, 0.30f } };
static int hostQuick;                                     /* 1 : la prochaine partie hebergee vient d'une PARTIE RAPIDE */

/* --- Regles de la partie (l'hote les choisit dans HEBERGER, les clients les recoivent dans PkSnap) : src/reseau/modes.c --- */
enum { MODE_FRAGS, MODE_CHRONO, MODE_ZONES, MODE_DRAPEAU, MODE_BOUCLIER, MODE_COUNT };
static const char *modeNames[MODE_COUNT] = { "FRAGS", "CHRONO", "3 ZONES", "DRAPEAU", "BOUCLIER" };
static const char *modeHelp[MODE_COUNT] = {
    "premier a la limite de frags",
    "le plus de kills quand le temps est fini",
    "3 zones : reste dedans pour la prendre, l'equipe qui en tient le plus marque",
    "vole le drapeau adverse et ramene-le chez toi ; mort = il tombe sur place",
    "les ROUGES surchargent le bouclier des BLEUS en restant dessus ; les BLEUS tiennent jusqu'a la fin",
};
enum { AMMO_NORMAL, AMMO_DOUBLE, AMMO_INFINI, AMMO_COUNT };
static const char *ammoRuleNames[AMMO_COUNT] = { "NORMALES", "x2", "INFINIES" };
typedef struct { int mode, hook, ammo, shop, specs, minutes; } MatchRules;
static MatchRules rules = { MODE_FRAGS, 1, AMMO_NORMAL, 0, 0, 10 };     /* partie en cours */
static MatchRules wantRules = { MODE_FRAGS, 1, AMMO_NORMAL, 0, 0, 10 }; /* choix du menu HEBERGER (velocity.ini) */
static int mode_team(int m) { return m >= MODE_ZONES; }   /* modes qui demandent des equipes */

/* --- Specialisations (un bonus passif chacune ; l'hote applique degats et soins, le joueur sa vitesse et ses jauges) --- */
enum { SPEC_AUCUNE, SPEC_ASSAUT, SPEC_REMPART, SPEC_ECLAIREUR, SPEC_SOUTIEN, SPEC_ARTIFICIER, SPEC_COUNT };
static const char *specNames[SPEC_COUNT] = { "AUCUNE", "ASSAUT", "REMPART", "ECLAIREUR", "SOUTIEN", "ARTIFICIER" };
static const char *specHelp[SPEC_COUNT] = {
    "pas de specialisation",
    "+15 % de degats infliges",
    "-20 % de degats subis, un peu plus lent",
    "+12 % de vitesse, un dash de plus, ennemis proches sur le radar",
    "regenere hors combat et soigne les allies proches",
    "munitions : jauges qui se remplissent 50 % plus vite",
};
static const float specCol[SPEC_COUNT][3] = {
    { 0.6f, 0.6f, 0.65f }, { 1.0f, 0.45f, 0.3f }, { 0.4f, 0.7f, 1.0f }, { 0.4f, 1.0f, 0.5f }, { 1.0f, 0.9f, 0.4f }, { 1.0f, 0.6f, 0.15f },
};
static int wantSpec = SPEC_AUCUNE;                         /* choisie dans LOADOUT (velocity.ini : spec=) */
static int my_spec(void) { return rules.specs || netMode == NET_OFF ? wantSpec : SPEC_AUCUNE; }
static int hook_allowed(void) { return !dmMode || rules.hook; }   /* grappin : regles de la partie */
static int mode_ok(int m, int map);                       /* src/reseau/modes.c : la carte a-t-elle les objectifs du mode ? */
/* etat des objectifs (l'hote le tient, PkSnap le transporte) */
#define NZONES 3
typedef struct { unsigned char owner, capTeam, prog, pad; } ZoneSt;                    /* owner / capTeam : 0, 1, 2 = personne */
typedef struct { unsigned char state, carrier, ret, pad; float pos[3]; } FlagSt;       /* state : FLAG_* ; ret : retour 0..255 */
typedef struct { ZoneSt z[NZONES]; FlagSt fl[2]; short pts[2]; unsigned char shield, pad[3]; } ObjState;
static ObjState obj;
enum { OBJ_ZONE, OBJ_FLAG_TAKE, OBJ_FLAG_DROP, OBJ_FLAG_RETURN, OBJ_FLAG_CAPTURE, OBJ_SHIELD };   /* EV_OBJ.sub */
enum { FLAG_HOME, FLAG_CARRIED, FLAG_DROPPED };
static int spectating, specFollow = -1;                   /* spectateur : suit le joueur specFollow, -1 = camera libre */
static float matchT, matchEndT;
static char myName[16] = "joueur";
static double nowT;

static V3 fv(const float *f) { return v3(f[0], f[1], f[2]); }
static void vf(float *f, V3 v) { f[0] = v.x; f[1] = v.y; f[2] = v.z; }
static float np_height(int i) { return (NP[i].st.flags & NF_CROUCH) ? H_CROUCH : H_STAND; }
static V3 np_center(int i) { return v3(NP[i].disp.x, NP[i].disp.y + np_height(i) * 0.5f, NP[i].disp.z); }
static V3 np_vel(int i) { return fv(NP[i].st.vel); }
static V3 np_head(int i) {                                /* centre de la sphere de tete (tirs a la tete) */
    return v3(NP[i].disp.x, NP[i].disp.y + np_height(i) - HEAD_DROP, NP[i].disp.z);
}
static void np_aabb(int i, float grow, V3 *mn, V3 *mx) {
    V3 d = NP[i].disp;
    *mn = v3(d.x - HW - grow, d.y - grow, d.z - HW - grow);
    *mx = v3(d.x + HW + grow, d.y + np_height(i) + grow, d.z + HW + grow);
}
/* a et b sont-ils allies ? (jamais en chacun pour soi, jamais soi-meme) */
static int same_team(int a, int b) {
    return teamSize > 0 && a != b && a >= 0 && b >= 0 && a < MAXP && b < MAXP && NP[a].team == NP[b].team;
}
static int team_score(int t) {
    int s = 0;
    for (int i = 0; i < MAXP; i++) if (NP[i].active && NP[i].team == t) s += NP[i].kills;
    return s;
}
static int team_count(int t) {
    int n = 0;
    for (int i = 0; i < MAXP; i++) n += NP[i].active && NP[i].team == t;
    return n;
}
static int team_frag_limit(int ts) { return ts > 0 ? TEAM_FRAG_BASE + TEAM_FRAG_PER * ts : FRAG_LIMIT; }
static int host_max_players(void) { return teamSize > 0 ? teamSize * 2 : MAXP; }
/* np_present : un autre joueur vivant (a dessiner) ; np_target : un autre joueur vivant qu'on peut toucher (pas un allie) */
static int np_present(int i) { return dmMode && i != myId && NP[i].active && NP[i].alive; }
static int np_target(int i) { return np_present(i) && !same_team(i, myId); }
/* vitesse relative entre le joueur et une cible : c'est elle qui compte pour les coups */
static float rel_speed(V3 targetVel) { return vlen(vsub(P.vel, targetVel)); }
/* --- Egide : face d'un joueur distant (comme view_dir, en 3D : regarder en l'air protege d'un pique), bouclier leve --- */
static V3 np_face(int i) {
    float cp = cosf(NP[i].st.pitch);
    return v3(sinf(NP[i].st.yaw) * cp, sinf(NP[i].st.pitch), -cosf(NP[i].st.yaw) * cp);
}
static int np_shield_up(int i) { return (NP[i].st.pad & 0x0F) == MK_SHIELD && (NP[i].st.pad & ST_SHIELD); }
static V3 shield_front_ofs(int i) {                       /* devant le torse de i, depuis ses pieds (mine collee au bouclier) */
    float yaw = i == myId ? P.yaw : NP[i].st.yaw;
    return v3(sinf(yaw) * 0.55f, 1.05f, -cosf(yaw) * 0.55f);
}
/* --- fin Egide --- */

/* --- Competences : bonus de degats du joueur local (MARQUE, CHARGE), applique a chaque point d'entree des degats --- */
static float markP[MAXP], markE[MAXE];                   /* cibles que j'ai marquees : temps restant */
static float markedMeT;                                   /* un adversaire m'a marque (HUD) */
static int skillUse[SK_COUNT], skillBonus;                /* journal : utilisations, touches bonifiees */
static float lastHpSeen = 100;                            /* CHARGE : PV du tick precedent (perdue si touche) */
static int allinActive;                                   /* ALL IN ONE en cours : un seul armement de RICOCHET pour l'eventail */
static float skill_out(int player, int enemy, float dmg) {
    float m = 1;
    if (player >= 0 && player < MAXP && markP[player] > 0) m *= MARK_MULT;
    if (enemy >= 0 && enemy < MAXE && markE[enemy] > 0) m *= MARK_MULT;
    if (P.chargeT > 0) { m *= CHARGE_MULT; if (P.chargeT > CHARGE_GRACE) P.chargeT = CHARGE_GRACE; }
    if (m > 1) skillBonus++;
    return dmg * m;
}
static int ric_take(void) {                               /* RICOCHET arme : rebonds du projectile tire maintenant */
    if (!P.ricArmed) return 0;
    if (!allinActive) P.ricArmed = 0;
    return RIC_BOUNCES;
}
/* --- fin Competences --- */

static int botMode;                                       /* 1 = bot hote, 2 = bot client (tests reseau) */
static int botLevel = 1;                                  /* difficulte du bot : 0 facile, 1 normal, 2 difficile */
static int botLocal, botHidden;                           /* --local : le bot rejoint 127.0.0.1 ; --hidden : fenetre reduite et muette */
static int vsBot;                                         /* partie joueur contre bots : nombre de bots lances (0 = non) ; jamais annoncee en ligne */
#ifdef _WIN32
static HANDLE botProc[MAXP];                              /* processus des bots (partie contre des bots, un par place) */
#else
static pid_t botProc[MAXP];
#endif
static const char *botLevelNames[3] = { "FACILE", "NORMAL", "DIFFICILE" };
static const float botAimMul[3]  = { 6.5f, 1.3f, 0.45f };   /* imprecision de visee */
static const float botReactT[3]  = { 1.1f, 0.32f, 0.14f };  /* delai avant de tirer apres avoir vu la cible */
static const float botHitP[3]    = { 0.18f, 0.38f, 0.60f };  /* chance qu'un tir soit cadre (sinon il frole la cible) */
static const float botHeadP[3]   = { 0.03f, 0.12f, 0.25f };  /* tir cadre : chance de viser la tete (comme un humain) */
static int botAimTest;                                    /* run_tests : ancienne visee sinusoidale, deterministe */
/* tirage d'un tir du bot : decalage lateral (m) au niveau de la cible. Cadre : moins de 0,2 m du centre ; rate : il
 * frole la cible (0,55 a 1,2 m sur le cote, un peu plus de loin) au lieu de partir n'importe ou. Renvoie 1 si cadre */
static int bot_roll_aim(float dist, float *offX, float *offY) {
    float p = botHitP[botLevel] * clampf(1.15f - dist / 80, 0.5f, 1);
    if (frand() < p) {
        if (frand() < botHeadP[botLevel]) { *offX = frange(-0.1f, 0.1f); *offY = HEAD_BOT_OFS + frange(-0.1f, 0.1f); return 1; }
        *offX = frange(-0.2f, 0.2f); *offY = frange(-0.25f, 0.25f); return 1;
    }
    float side = frand() < 0.5f ? -1.0f : 1.0f;
    *offX = side * (frange(0.55f, 1.2f) + dist * 0.008f);
    *offY = frange(-0.35f, 0.35f);
    return 0;
}
static const float botParryP[3]  = { 0.15f, 0.5f, 0.85f };  /* chance de tenter un parry quand l'epee adverse arrive */
static void netlog(const char *fmt, ...);
static void net_hit(int target, float dmg, V3 knock, int weapon);
static void net_fx_rail(V3 a, V3 b, float k);
static void net_fx_kinetic(V3 a, V3 b, float e);
static void net_fx_rocket(V3 pos, V3 vel, unsigned int netId);
static void net_fx_rocket_end(unsigned int netId, V3 pos);
static void net_fx_parry(unsigned int netId, V3 pos, V3 vel, float mult, int parries);
static void net_fx_melee(V3 pos, V3 dir, float sp);
static void net_fx_fist(V3 pos, V3 dir, float x, int sub);   /* Poings : EV_MELEE, sub = coup (1..3) | 0x10 punch-jump */
static void net_fx_slam(V3 pos, float R, float hw, V3 dir, float vh, float vy);
static void net_fx_coin(int sub, unsigned int netId, V3 pos, V3 vel);
static void net_fx_shot(V3 eye, V3 dir, float half, unsigned int seed);
/* Scie : definies apres les roquettes */
static void net_fx_saw(int sub, Proj *p, int target, int actor);
static int saw_hook_test(float *t);                       /* grappin : le crochet touche ma scie plantee ? */
static void saw_recall_start(void);
static int saw_parry(Proj *p, V3 eye, V3 f, float range, float speedMult);
static void saw_remove_owner(int id);
/* Tesla : EV_TESLA (definie avec les autres net_fx_*) */
static void net_fx_tesla(int sub, unsigned int netId, V3 pos, int actor, int target);
static void maps_heat_add(int killer, int victim);         /* carte de chaleur du 2e ecran (interface/carte_ecran.c) */
/* --- Ventouse : definies apres la Scie (mines) et avec les autres net_fx_* (reseau) --- */
static int mine_explode(Proj *p, int boosted, int byShot);
static int mine_shootable(const Proj *p);
static int mine_has_target(const Proj *p);
static int mine_tear(Proj *p, V3 f);
static void net_fx_mine(int sub, unsigned int netId, V3 a, V3 b, int target, float x);
static void net_hit_id(int target, float dmg, V3 knock, int weapon, unsigned int netId);   /* net_hit + netId de l'objet */
/* --- fin Ventouse --- */
/* --- Egide : point d'entree unique des degats sur un joueur (definis avec les autres net_*) ---
 * hit_player : le bouclier d'abord (shield_intercept), puis l'EV_HIT. dir = direction du coup vers la cible. Renvoie 1 si la
 * touche part, 0 si elle est revendiquee comme bloquee (l'hote arbitre). hit_player_ex : blast = SHB_* ; netId, f4..f6 =
 * champs propres a l'arme (Scie : renvois ; Lance : vitesse, v_c, couchee ; Ventouse : netId de la mine) */
static int hit_player(int i, float dmg, V3 knock, int w, V3 dir);
static int hit_player_ex(int i, float dmg, V3 knock, int w, V3 dir, int blast, unsigned int netId, float f4, float f5, float f6);
/* Tirs a la tete et chaine de tirs pleine : comme hit_player, avec les drapeaux HF_* (pad[0] de l'EV_HIT) ; tete : f[4..6] = point
 * d'impact sur la tete, que l'hote verifie */
static int hit_player_fl(int i, float dmg, V3 knock, int w, V3 dir, int flags, V3 at);
static int shield_faces(int i, V3 dir, int blast);        /* le joueur i bloquerait-il ce coup ? (sans effet) */
static void shield_rip(int i);                            /* mon grappin arrache le bouclier de i */
/* --- fin Egide --- */
/* --- Speedrun : classements des meilleurs temps (src/jeu/speedrun.c ; serveur : src/reseau/speedrun_serveur.c) ---
 * Categories : SR_COMPLET (toutes les cartes CHALLENGE a la suite), SR_VAGUES100 (mode VAGUES, vague 100 terminee),
 * SR_MAP0 + m (course de la carte CHALLENGE m). Un run = speedrun_begin au depart (ticket horodate par le serveur),
 * puis speedrun_report a l'arrivee (temps de jeu en ms ; splitMs : temps intermediaires cumules, le dernier = ms).
 * Rien n'est envoye en --bot / --shot, hors ligne ou sans ticket. */
enum { SR_COMPLET, SR_VAGUES100, SR_MAP0 };
static void speedrun_begin(int cat);
static void speedrun_report(int cat, int ms);
static void speedrun_report_splits(int cat, int ms, const int *splitMs, int n);
/* --- fin Speedrun --- */
