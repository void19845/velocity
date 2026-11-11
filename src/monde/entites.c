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
     * par le dernier coup (0 = chaine libre) et garde anti double traction du lanceur apres un parry --- */
    float flailT, flailParriedT;
    V3 flailHitPos;
    int flailHitN;
    /* --- Fleau, traction (appui long) : temps restant avant l'accroche (0 = aucun appui a suivre), cibles accrochees (indice
     * fleau : ennemi i, ou MAXE + joueur) avec leur temps sans ligne de vue et le delai avant leur prochain EV_FLAIL ; victime
     * en LAN : qui me tire (id + 1, 0 = personne), bail restant, intensite (Egide : 1/2), temps sans ligne de vue --- */
    float flailArm;
    int flailPullN, flailHk[FLAIL_HOOK_MAX];
    float flailHkLos[FLAIL_HOOK_MAX], flailHkSend[FLAIL_HOOK_MAX];
    int flailBy;
    float flailLeaseT, flailScale, flailLosT;
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

/* ennemis : drone et kamikaze de toujours ; sentinelle, essaim et bastion du mode VAGUES ; CIBLE et MINI (cibles de visee des
 * cartes CHALLENGE, 1 PV, jamais en vague) ; les quatre boss (toujours a la fin : EN_BOSS compare a EN_PYRA) */
enum { EN_DRONE, EN_RUSHER, EN_SNIPER, EN_SWARM, EN_BASTION, EN_BOMBE, EN_CARRE, EN_MORTIER, EN_CIBLE, EN_MINI, EN_PYRA, EN_TESS, EN_BALL, EN_HEXA, EN_COUNT };
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
    int roqGen, roqCoinLast;                              /* VAGUES (boutique_roquettes.c) : 0 roquette, 1 mini-roquette ; derniere piece traversee + 1 */
    float roqSpd, roqSize;                                /* vitesse de croisiere, taille de la roquette (0 : normale) */
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
static float vg_dmg_mul(int i, int charged, int marked);   /* boutique : degats */
static void vg_lifesteal(float dmg);
static float vg_heal(float pv);                           /* src/solo/boutique_soin.c : seul chemin de soin de la boutique */
static void vg_on_kill(int i);
/* boutique VAGUES, SAC DE STATS (src/solo/boutique_stats.c) : chaque etape-stat de la boutique est une ligne de donnees (shMods[], generee
 * par outils/export_c.js) et le code des armes lit la valeur effective d'une stat avec st(graine, stat, base), UNE fois par tir. Les enums et
 * les types sont ici parce que les armes, compilees avant la boutique, lisent le sac. Hors VAGUES, ou si la graine n'est pas equipee,
 * st() rend la base telle quelle : PvP et LAN ne changent pas. Detail et recette : docs/boutique_vagues/ARCHITECTURE_ARMES.md. */
enum { SH_RAIL, SH_ROQ, SH_SHO, SH_LAN, SH_IMP, SH_PIE, SH_AIO, SH_N };   /* graines de la boutique v2 : un indice par arme / melee / competence jouable */
enum {
    STAT_DEG, STAT_TAI, STAT_VIT, STAT_ZON, STAT_POR, STAT_MUN, STAT_NBP, STAT_SUI,   /* axes du design : armes a distance et competences */
    STAT_CD, STAT_ANI, STAT_COM, STAT_MUR, STAT_VOL,                                   /* axes de la melee (le cooldown sert aussi aux competences) */
    STAT_CONE, STAT_PERTE, STAT_PNEAR, STAT_PFAR, STAT_COUT, STAT_PTK,                 /* stats nommees du Shotgun et des points de jauge par kill */
    STAT_TETE, STAT_DEGC,                                                              /* Railgun : zone de la tete, degats par cible traversee */
    STAT_BORD, STAT_CENTRE, STAT_DIRECT,                                               /* Roquettes : degats au bord, au centre du souffle, impact direct */
    STAT_CDJ, STAT_CDE, STAT_CDV, STAT_TCH, STAT_MURJ,                                 /* Lance : recharges (joute, estoc, vide), charge couchee, points de joute */
    STAT_CDC, STAT_ECA, STAT_CDW, STAT_MURC,                                           /* Impulsion : recharges (combo, combo rate), ecart des coups, points de combo */
    STAT_RIC, STAT_DUREE,                                                              /* Piece : multiplicateur de ricochet, duree de vie */
    STAT_CDT, STAT_DEGS, STAT_ANGLE,                                                   /* All in One : delai entre tirs, degats de la salve, demi-angle (degres) */
    STAT_COUNT
};
enum { SOP_MUL, SOP_ADD, SOP_MAX, SOP_MULL, SOP_MIN, SOP_TO, SOP_COUNT };              /* operations d'un modificateur (outils/effets_c.js) */
enum { COND_NONE, COND_NEAR, COND_HEAD, COND_AIR, COND_ELITE, COND_PIERCED,            /* conditions d'un modificateur (StatCtx) */
       COND_DIRECT, COND_BOUNCE, COND_FIRST, COND_SALVEFULL, COND_COUNT };
#define SMF_FIXED 1                                       /* StatMod.flags : la valeur ne monte pas avec le niveau (entier fixe, plafond absolu) */
typedef struct { unsigned char seed, step, stat, op, cond, grp, flags, phase; float val, par; } StatMod;   /* une ligne de shMods[] : step = rang dans shSteps[seed] ; par = parametre de la condition ; phase 0 a 3 = ordre d'application */
typedef struct { float dist; int head, air, elite, pierced, bounce, direct, full; } StatCtx;   /* contexte d'un coup, pour les modificateurs conditionnels (0 = inconnu / faux) */
static float st(int seed, int stat, float base);          /* valeur effective de la stat (sans contexte : les modificateurs conditionnels ne comptent pas) */
static float st_ctx(int seed, int stat, float base, const StatCtx *c);   /* idem avec contexte */
static int st_int(int seed, int stat, int base);          /* entier le plus proche de st(...) : munitions, nombre de coups, de plombs, de pieces */
static int st_int_ctx(int seed, int stat, int base, const StatCtx *c);   /* idem avec contexte */
static void sh_rebuild(void);                             /* recalcule le sac (achat, niveau, changement d'arme, reset ; st() le refait seul si l'etat a change) */
/* boutique VAGUES, groupe MELEE (src/solo/boutique_melee.c) : Lance et Impulsion */
static void vgm_reset(void);
static void vgm_melee_tick(float dt);
static void vgm_kill(int i);
static float vgm_vc(float vc);
static float vgm_joust_mul(void);
static void vgm_joust_hit(int i);
static void vgm_estoc_hit(int i);
static int vgm_estoc_pts(void);
static float vgm_thrust_cd(float cd);
static float vgm_whiff_cd(float cd);                      /* Releve : recharge d'un estoc dans le vide */
static float vgm_estoc_anim(float t);
static float vgm_tap(float t);
static void vgm_joust_whiff(void);
static int vgm_fist_n(void);
static int vgm_fist_up(int step, int n);
static float vgm_fist_gap(float g);
static float vgm_fist_end(float cd);
static float vgm_fist_whiff(float cd);
static void vgm_fist_hit(int up);
static void vgm_fist_swing(int step);
static void vgm_fist_full(int landed);
static int vgm_fist_free(void);
static void vg_on_parry(V3 at);
/* boutique VAGUES, graine RAILGUN (src/solo/boutique_rail.c) : crochets du rail, tous sans effet hors vagOn */
static float vg_rail_wide(int w);
static int vg_rail_head(int i, V3 at, int w);
static V3 vg_rail_bend(V3 o, V3 d);
static void vg_rail_rank(V3 eye, V3 d, float tw);
static float vg_rail_mul(int i);
static void vg_rail_hit(int i, V3 at, float dmg, int head);
static int vg_rail_chain(void);
static float vg_rail_gauge_mul(void);
static int vg_rail_wall(V3 d, float dmg, float knockE, float knockP, int w, V3 *endOut, V3 *nOut);
static float vg_rail_before(void);
static void vg_rail_after(int hit, int nk, float dmg, V3 end);
static void vg_rail_joust(void);
static void vg_rail_tick(float dt);
static int rlCoinHit;
/* boutique VAGUES, graine ROQUETTES (src/solo/boutique_roquettes.c) : crochets appeles par le combat, tous sans effet hors vagOn */
static int roq_max(int w, int base);
static void roq_fire(Proj *p);
static void roq_setup(Proj *p);
static void roq_tick_proj(Proj *p, float dt);
static int roq_prox(Proj *p);
static float roq_hitpad(Proj *p);
static int roq_explode(Proj *p, V3 c, int hitEnemy, float mult);
static float roq_blast_k(float f);
static float roq_seek_speed(Proj *p);
static int roq_coin(Proj *p, int ci);
static void roq_fx_tick(float dt);
static void roq_joust(void);
static int roq_fist(V3 eye, V3 f);
static int roqBlastOn;
static void vg_boss_stun(int i, float dmg);              /* src/solo/boss.c : grosse orbe renvoyee sur un boss */
/* boutique VAGUES, groupe COMPETENCES (src/solo/boutique_competences.c) : PIECE et ALL IN ONE ; tous sans effet hors VAGUES */
enum {
    CP_RIEN,
    /* PIECE */
    CP_POLIE, CP_RECHARGE, CP_RESERVE, CP_RECYCLEE, CP_VOLANTE, CP_REBOND, CP_AIMANT_DOUX, CP_DIVIDENDE, CP_PRIME,
    CP_PARATONNERRE, CP_PLUIE, CP_PIEGEE, CP_LESTEE, CP_PUR, CP_ORBITALE,
    /* ALL IN ONE */
    CP_A_LARGE, CP_A_RECHARGEMENT, CP_A_PRECISE, CP_A_CADENCE, CP_A_RESERVE, CP_A_JAUGE, CP_A_PRIME, CP_A_COMPLETE,
    CP_A_DOUBLE, CP_A_UNIQUE, CP_A_DERNIER,
    /* capstones */
    CP_K_PIE_RAIL, CP_K_PIE_ROQ, CP_K_PIE_LAN, CP_K_PIE_IMP, CP_K_AIO_LAN, CP_K_AIO_RAIL, CP_K_AIO_ROQ, CP_K_PIE_SHO, CP_K_AIO_SHO
};
static int cpRicN;                                        /* cibles touchees par le dernier ricochet de piece */
static float cp_coin_mult(int depth);                     /* multiplicateur d'un ricochet de piece (COIN_MULT sans boutique) */
static float cp_coin_life(void);                          /* vie d'une piece lancee */
static int cp_coin_dirs(V3 f, V3 *out);                   /* directions des pieces d'un lancer (1 sans boutique) */
static float cp_seek_range(void);                         /* portee de recherche d'un ricochet de piece */
static int cp_coin_orbit(int ci, float dt);               /* piece orbitale : 1 tant qu'elle tourne autour du joueur */
static void cp_coin_pop(V3 o);                            /* piece piegee : explose au ricochet */
static void cp_coin_rocket(V3 o);                         /* piece traversee par une roquette */
static void cp_coin_new(int ci);                          /* piece lancee : etat de la boutique remis a zero */
static int cp_coin_bounce(int ci, V3 n);                  /* piece qui rebondit sur le decor : 1 si elle rebondit */
static void cp_ric_after(int te, V3 o, float dmg);        /* apres la touche d'un ricochet de piece */
static void cp_after_ricochet(void);                      /* apres un ricochet de tir instantane : dividende */
static float cp_dmg_mul(int i);                           /* degats de la salve ALL IN ONE */
static void cp_on_kill(int i);
static void cp_tick(float dt);
static int cp_max(int sk, int base);                      /* charges d'une competence */
static float cp_recharge(int sk, float base);
static float cp_half_deg(float base);                     /* demi-angle de la salve (degres) */
static void cp_aio_roq_hit(int i);                        /* capstone AIO>ROQUETTES : souffle du Shotgun sur chaque touche de la salve */
static int cp_allin_extra(int w, int n);                  /* tirs en plus de la salve */
static void cp_salve_begin(int w, int n);
static int cp_salve_refund(int w);                        /* munitions rendues apres la salve */
static float cp_salve_cd(float cd);
static void cp_salve_end(int w, int n);
static void cp_coin_pellets(V3 o);                        /* capstone PIECE>SHOTGUN : une piece qui finit (ricochet, roquette, fin de vie) lache des plombs */
static void cp_aio_sho_fire(float *half, int *nP);        /* capstone AIO>SHOTGUN : cone x3 et +50 % de plombs d'une salve de Shotgun (fire_shotgun) */
static float vg_rail_open_mul(int i, float fo);           /* capstone RAIL>SHOTGUN (boutique_rail.c) : plomb sur une cible ouverte par le rail (fire_shotgun) */
