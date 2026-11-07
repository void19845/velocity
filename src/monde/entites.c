/* VELOCITY - structures du jeu : joueur P, ennemis, pieces, projectiles, particules
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Entites                                                             */
/* ------------------------------------------------------------------ */
typedef struct {
    V3 pos, prevPos, vel, wallN;
    float yaw, pitch;
    int grounded, crouch, sliding, airJumps, wallrun, wallTouch, dead, groundBox, weapon;
    float dashCharge, hp, hurtFlash, slideCd, wallTime, wallJumpCd, coyote;
    float stairT;                                         /* temps restant "je viens de monter une marche" (comportement de rampe) */
    float landDip, stepOfs, roll, fovKick, fovCur, eyeH, bobT, bob;
    float fireCd, switchT, recoil, deathT, swayX, swayY;
    int hook;                                             /* HOOK_NONE / FLYING / ATTACHED */
    int hookEnemy;                                        /* 0 = accroche au decor, sinon index ennemi + 1 */
    V3 hookPos, hookDir, anchor;
    float ropeLen, hookCd, hookDist, hookBlockT;
    float meleeCd, meleeT;
    float parryWin;                                       /* temps restant de la fenetre de parry */
    int swingLanded;                                      /* ce coup a-t-il touche ou pare quelque chose ? */
    float skillCh;                                        /* charges de la competence (0..skillDef[skill].max) */
    int skill;                                            /* SK_* */
    int ricArmed;                                         /* RICOCHET : fenetre ouverte (ricT > 0), tout ce que je tire rebondit */
    float ricT;                                           /* RICOCHET : temps restant de la fenetre (RIC_DURATION) */
    V3 pushV; float pushT;                                /* SHOTGUN : poussee forcee (m/s, secondes restantes), sans toucher a vel */
    float chargeHold, chargeT;                            /* CHARGE : maintien en cours, fenetre ouverte restante */
    float aimT;                                           /* AIMANT : temps restant de l'aimant de visee */
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
    float sgPumpT, sgPumpMax;                             /* SHOTGUN : animation de pompe (temps restant / duree totale) */
    int sgLastPellets; float sgLastT;                     /* SHOTGUN : HUD "7/10" apres un tir qui touche */
    int sawIdx;                                           /* Scie : 0 = en main (ou pas de scie), sinon index PR + 1 */
    float sawCd;                                          /* Scie : delai avant le prochain lancer */
    /* Tesla : arc actif ce tick (ST_ZAP, son en boucle), clic sec deja joue pour cet appui, "CHAINE x3" deja affiche ;
     * etourdissement (decharge paree), temps avant l'envoi des cumuls, derniere impulsion qui touche (HUD, son) */
    int teslaOn, teslaDry, teslaChain3;
    float teslaStunT, teslaAggT, teslaHitT;
    /* --- Lance : etat LS_*, temps depuis l'appui, v_c de la meilleure cible du cone (HUD),
     * temps depuis la derniere joute et joueur touche + 1 (anti-doublon EV_LANCE) --- */
    int lanceState, lanceHitWho;
    float lanceT, lanceVc, lanceHitT;
    /* --- Fleau : animation restante de la chaine, point ou elle s'est accrochee (premiere cible), cibles touchees
     * par le dernier coup (0 = chaine libre), garde anti double traction apres un parry, derniere cible LAN touchee
     * (id + 1) et temps restant de la fenetre du combo fleau -> rail --- */
    float flailT, flailParriedT, flailLastT;
    V3 flailHitPos;
    int flailHitN, flailLastTgt;
    /* --- Poings : coups donnes dans le combo (0..3), coup anime (1..3, rendu et NetPState.melee), punch-jump disponible,
     * points de jauge deja donnes par le combo (0..50), joueur touche par le dernier uppercut + 1 (bot : rail) ;
     * temps depuis le dernier coup, appui memorise, temps en l'air, garde (arme a distance abaissee), fenetre
     * uppercut -> rail --- */
    int fistStep, fistAnim, fistPJ, fistPts, fistUpTgt;
    float fistT, fistBuf, fistAirT, fistGuard, fistUpT;
    /* --- Egide : etat SHS_*, jauge (copie locale, l'hote fait autorite en LAN), duree tenue / baissee (rearmement de la
     * fenetre parfaite), brise 3 s, arrache 1 s, un seul belier par accroche ; animation de levee (0..1), eclat recent --- */
    int shieldState, shieldRamDone;
    float shieldGauge, shieldHeldT, shieldDownT, shieldBrokenT, shieldRipT;
    float shieldAnim, shieldHitT;
} Player;
enum { HOOK_NONE, HOOK_FLYING, HOOK_ATTACHED };
static Player P;

/* ennemis : drone et kamikaze de toujours ; sentinelle, essaim et bastion du mode VAGUES ; les quatre boss */
enum { EN_DRONE, EN_RUSHER, EN_SNIPER, EN_SWARM, EN_BASTION, EN_PYRA, EN_TESS, EN_BALL, EN_HEXA, EN_COUNT };
#define EN_BOSS(t) ((t) >= EN_PYRA)
typedef struct {
    V3 pos, vel; float hp, fireT, t, flash; int type, alive;
    /* --- mode VAGUES (src/solo/) : PV de depart, etat de l'IA et son minuteur, phase (boss), elite, invocation
     * (1 = appelee par un boss), gel / brulure (comportements de la boutique), cumul de touches (FRACTURE),
     * cap visee ou fige (charges, rayons), orientation (bastion), derniere touche (FOURCHE) --- */
    float maxHp, stT, frozenT, burnT, fracT, yaw, lastDmg;
    int st, phase, elite, summoned, frac, combo;
    V3 aim, home;
} Enemy;
#define MAXE 64
static Enemy E[MAXE];

enum { PR_ROCKET, PR_ORB, PR_PARRIED, PR_SAW,
       PR_MINE };                                         /* Ventouse */

/* Tesla : teslaT > 0 = relais actif (temps restant), prochaine impulsion du relais, joueur qui l'a active */
/* Fleau : flailStruck = 1 si la piece a ete frappee au fleau (vol droit, sans gravite) */
typedef struct { V3 pos, vel; float life, spin; int alive, owner; unsigned int netId; float teslaT, teslaPulse; int teslaBy; int flailStruck;
                 int decoy; } Coin;                     /* decoy : MIRAGE (leurre immobile, sans gravite) */
#define MAXCOIN 24
static Coin CO[MAXCOIN];
static int lastChain;                                     /* longueur du dernier ricochet (tests, journal) */
typedef struct {
    V3 pos, vel; float life, mult; int kind, alive, owner, parries, orb; unsigned int netId;
    int ricLeft;                                          /* RICOCHET : rebonds restants (roquette, scie) */
    int coinHits, seekKind, seekIdx;                      /* roquette redirigee par une piece : pieces enchainees ; cible qu'elle suit
                                                             (0 aucune, 1 ennemi, 2 joueur LAN) et son indice */
    /* Scie (kind == PR_SAW) : etat SAW_*, temps dans l'etat, rotation, normale de la face (plantee), axe du disque,
     * qui l'a paree (-1), boite ou elle est plantee (-1), renvois consecutifs, pieces relancees (bits),
     * minuterie (reemission si plantee, vrombissement en vol) */
    int sawState, sawBy, sawBox, sawHurt, sawBounces;
    float sawT, sawSpin, sawSync;
    V3 sawN, sawAxis;
    unsigned int sawCoins;
    /* --- Ventouse (kind == PR_MINE, ou PR_PARRIED si renvoyee) : etat MS_* (0 = pas une mine), porteur (ennemi + 1
     * ou joueur + 1), detonation demandee pendant l'armement (MX_* + 1), retombee d'un porteur (meche conservee),
     * ordre de lancer, position relative au porteur (pieds / centre du drone), normale de la face, temps depuis le
     * lancer ou le collage, armement restant (joueur), prochain bip --- */
    int mineState, mineAttach, mineDet, mineFell;
    unsigned int mineSeq;
    V3 mineOfs, mineN;
    float mineT, mineArmT, mineBeepT;
    int mineShield;                                       /* Egide : mine collee sur la face d'un bouclier leve */
    int big;                                              /* VAGUES : grosse orbe chargee d'un boss (1) */
} Proj;
#define MAXPR 128
static Proj PR[MAXPR];

typedef struct { V3 pos, vel, col; float life, max, size, grav; } Part;
#define MAXPA 4096
static Part PA[MAXPA];
static int paNext;

typedef struct { V3 a, b, col; float life, max; } Beam;
#define MAXBM 64                                          /* un tir RICOCHET dessine un trait par segment (32 avant) */
static Beam BM[MAXBM];
static int bmNext;

typedef struct { V3 a, b, col; float t, life; } Tracer;   /* SHOTGUN : traceurs de plombs (pas add_beam : MAXBM 32) */
static Tracer TR[MAXTR];
static int trNext;

static int wave, toSpawn, score, kills, combo, bestScore;
static float waveDelay, spawnT, gameTime, hitMarker, comboT, msgT, popT, shake;
static int shotStreak;                                    /* touches successives au tir */
static float hitstop, parryFlash, streakLostT;
static char msg[64], popText[32];
static int raceOn;                                        /* mode COURSE en cours (src/jeu/course.c) : grappin aimante */
/* mode VAGUES (src/solo/vagues.c) : en cours ; chemin de la boutique a qui reviennent les touches en cours (UP_*, -1
 * aucun) ; profondeur des effets en chaine ; projectile en cours de traitement (PR_*, -1 : aucun) */
static int vagOn, vgSrc = -1, vgDepth, vgProjKind = -1;
static int vgCoinCtx;                                     /* touches d'un ricochet de piece en cours (chemin PIECE) */
static float vg_dmg_filter(int i, float dmg);             /* src/solo/ameliorations.c : armure des boss, bouclier du bastion */
static void vg_on_hit(int i, float dmg, int charged, int marked);
static void vg_on_kill(int i);
static void vg_on_parry(V3 at);
static void vg_boss_stun(int i, float dmg);              /* src/solo/boss.c : grosse orbe renvoyee sur un boss */
