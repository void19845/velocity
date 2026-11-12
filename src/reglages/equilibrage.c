/* VELOCITY - TOUS les #define d'equilibrage : mouvement, armes, parry, PvP, points
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Reglages du mouvement (metres, secondes)                            */
/* ------------------------------------------------------------------ */
#define HW             0.4f   /* demi-largeur joueur */
#define H_STAND        1.8f
#define H_CROUCH       1.1f
#define EYE_STAND      1.6f
#define EYE_CROUCH     0.95f
#define STEP_H         0.55f
#define STAIR_SLOPE    0.45f   /* pente d'un escalier (marche 0,4 m / profondeur 0,84 m) : vy de rampe = vitesse x pente */
#define STAIR_RAMP_T   0.18f   /* un escalier agit comme une rampe pendant ce delai apres la derniere marche montee */
#define FRICTION_MOVE  0.12f   /* friction au sol x0,12 quand on tient une direction (sans sauter, la vitesse se perd moins) */
#define GRAVITY        24.0f
#define JUMP_V         8.4f
#define RUN_SPEED      9.5f
#define CROUCH_SPEED   4.5f
#define GROUND_ACCEL   12.0f
#define FRICTION       6.0f
#define STOPSPEED      3.0f
#define AIR_ACCEL      14.0f
#define AIR_CAP        1.0f
#define SLIDE_MIN      6.5f
#define SLIDE_END      3.0f
#define SLIDE_BOOST    4.5f
#define SLIDE_FRICTION 0.7f
#define SLIDE_CD       1.0f
#define DASH_SPEED     18.0f
#define DASH_RECHARGE  1.6f
#define WALLJUMP_PUSH  8.0f
#define WALLJUMP_UP    8.8f
#define WALLRUN_MIN    6.0f
#define WALLRUN_MAX    1.8f
#define BASE_HFOV      105.0f

/* Aimant de visee : competence AIMANT (plus d'aide passive) ; actif AIM_T secondes, recharge AIM_RECHARGE */
#define AIMASSIST_CONE  0.10f  /* rayon (rad, ~6 deg) du cone pres du viseur ou l'aimant agit */
#define AIMASSIST_RATE  3.0f   /* vitesse de correction au centre du cone (s'annule au bord) : aide, jamais verrou */
#define AIMASSIST_RANGE 70.0f  /* portee max */

#define GRAPPLE_RANGE    55.0f   /* portee max du crochet */
#define GRAPPLE_SPEED    120.0f  /* vitesse de vol du crochet */
#define GRAPPLE_PULL     70.0f   /* acceleration vers le point d'accroche */
#define GRAPPLE_MAXSPEED 38.0f   /* plus de traction au-dela de cette vitesse vers l'ancre */
#define GRAPPLE_CD       0.5f
#define GRAPPLE_JUMP     5.0f    /* boost vertical quand on lache en sautant */
#define GRAPPLE_ENEMY_PULL     95.0f  /* traction plus forte vers un ennemi */
#define GRAPPLE_ENEMY_MAXSPEED 46.0f
#define GRAPPLE_STRIKE_DMG     50.0f  /* degats de l'impact... */
#define GRAPPLE_STRIKE_PER_MS  3.0f   /* ...plus 3 par m/s de vitesse d'impact */

#define RAIL_CD        0.85f
#define PICKUP_RESPAWN  12.0f

/* Cartes (MapDef, docs/CODE.md) : bande d'acceleration de La Boucle, plan de mort des cartes ouvertes */
#define SPEED_ACCEL     30.0f   /* B_SPEED : poussee (m/s2) dans le sens de la bande, sans friction */
#define SPEED_MAX       28.0f   /* B_SPEED : vitesse jusqu'a laquelle la bande pousse */
#define VOID_CREDIT_T   4.0f    /* chute dans le vide moins de 4 s apres une touche : kill au dernier attaquant */
#define VOID_WARN_H     8.0f    /* vignette rouge sous killY + 8 m quand on tombe */

/* Pieces facon Marksman (Ultrakill) : on lance une piece, un tir de rail qui la touche ricoche
 * tout seul vers la cible la plus proche. Pense pour viser a pleine vitesse. */
#define COIN_MAX        2       /* patch d'equilibrage : 2 charges (4 avant) ... */
#define COIN_RECHARGE   5.0f    /* ... et 5 s par piece (2 s avant) : une piece se gagne, elle ne se jette plus a la chaine */
#define COIN_RADIUS     0.9f    /* grosse zone de touche */
#define COIN_MULT       1.5f    /* chaque ricochet multiplie les degats */
#define COIN_GRAVITY    16.0f
#define COIN_SEEK_RANGE 90.0f
#define COIN_ROCKET_SPEED 70.0f /* roquette redirigee par une piece : vitesse du vol vers la cible (ROCKET_SPEED : 38) */
#define COIN_ROCKET_CHAIN 3     /* ... pieces enchainees par la meme roquette au plus */

/* Competences : l'emplacement de la piece (touche A_SKILL) recoit une competence au choix du LOADOUT
 * (docs/COMPETENCES_MARKSMAN.md). Charges et recharge par competence : skillDef. */
enum { SK_COIN, SK_MARK, SK_CHARGE, SK_RICOCHET, SK_ALLIN, SK_DECOY, SK_AIM, SK_COUNT };
#define MARK_T            4.0f    /* MARQUE : duree de la marque */
#define MARK_RANGE        90.0f
#define MARK_MULT         1.25f   /* toute touche du marqueur sur une cible marquee */
#define CHARGE_HOLD       0.6f    /* CHARGE : maintenir la touche */
#define CHARGE_WINDOW     2.0f    /* puis fenetre ouverte pour la prochaine touche */
#define CHARGE_MULT       1.5f
#define CHARGE_GRACE      0.05f   /* une fois declenchee, les autres touches du meme tir en profitent (perforation, plombs) */
/* RICOCHET (patch d'equilibrage) : pendant RIC_DURATION secondes, TOUT ce que tu tires rebondit sans limite de rebonds ni de
 * portee, jusqu'a sortir de la carte (kill zone) ou toucher une cible. Les plafonds RIC_CAP_* ne sont que des garde-fous
 * techniques (cout CPU, evenements reseau, traces a l'ecran) : une carte fermee ne laisse sinon jamais sortir un tir. */
#define RIC_DURATION      6.0f    /* RICOCHET : duree de la fenetre (la recharge de skillDef ne repart qu'apres) */
#define RIC_CAP_BOUNCES   24      /* garde-fou : rebonds au plus par rail / Elan (0 : aucun) */
#define RIC_SEG_LEN       400.0f  /* longueur d'un segment : au-dela sans mur, le tir est sorti de la carte */
#define RIC_HITS_MAX      1       /* un meme tir ne touche chaque cible qu'une fois, meme s'il la recroise apres un rebond */
#define RIC_PELLET_CAP    6       /* garde-fou : rebonds au plus par plomb (10 plombs par tir) */
#define RIC_ROCKET_CAP    40      /* garde-fou : rebonds au plus par roquette (ils s'arretent aussi sur une cible) */
#define RIC_ROCKET_LIFE   15.0f   /* duree de vie d'une roquette qui rebondit (6 s sinon) */
#define RIC_ROCKET_SLOW   1.0f    /* vitesse gardee par une roquette a chaque rebond (1 : aucune perte) */
#define RIC_SAW_CAP       12      /* garde-fou : rebonds au plus pour la scie (SAW_FLY_MAX l'arrete aussi) */
/* Triangle pierre-feuille-ciseaux, a sens unique (pas une synergie de loadout) : MIRAGE contre PIECE, PIECE contre RICOCHET, RICOCHET
 * contre MIRAGE. Chaque fleche se coupe avec sa constante (0 = desactivee). */
#define DECOY_COIN_LURE   1       /* MIRAGE => PIECE : le ricochet d'une piece (rail, Elan, roquette) vise le leurre adverse avant le joueur */
#define COIN_VS_RIC       1       /* PIECE => RICOCHET (lecture par defaut, le proprietaire n'a rien precise) : un projectile en rebond
                                     (fenetre RICOCHET ouverte) qui touche une piece ADVERSE repart vers son tireur au lieu de la voler */
#define DECOY_RIC_ALWAYS  1       /* RICOCHET => MIRAGE : un tir sur le leurre adverse repart vers son poseur ; 1 = des que RICOCHET est
                                     equipe, 0 = seulement pendant la fenetre */
#define ALLIN_SPREAD_BASE 0.8f    /* ALL IN ONE : demi-angle de l'eventail (degres) */
#define ALLIN_SPREAD_PER  0.7f    /* par munition au-dela de la premiere */
#define ALLIN_SPREAD_MAX  6.0f
#define DECOY_T           3.0f    /* MIRAGE : duree du leurre */
#define DECOY_MULT        1.0f    /* un de mes rails qui le traverse ricoche sans bonus */
#define AIM_T             4.0f    /* AIMANT : duree de l'aimant de visee (reglages AIMASSIST_*) */
#define AIM_RECHARGE      45.0f   /* longue recharge, non remplie au respawn (equilibrage) */
#define RAIL_DMG       100.0f
#define ROCKET_CD      0.75f
#define ROCKET_SPEED   38.0f
#define ROCKET_DMG     110.0f
#define ROCKET_RADIUS  4.5f
#define ROCKET_KNOCK   13.0f

/* Elan (convertisseur cinetique) : la vitesse horizontale depensee devient des degats */
#define KIN_CD            0.5f    /* recharge de l'Elan (divisee par streak_rate) */
#define KIN_DMG_BASE      20.0f   /* degats a l'arret */
#define KIN_DMG_PER_MS    5.0f    /* degats en plus par m/s de vitesse depensee */
#define KIN_SPEED_CAP     15.0f   /* vitesse horizontale maximale convertie en degats par tir */
#define KIN_BRAKE_ALL     1       /* 1 : le tir enleve TOUTE la vitesse horizontale du tireur (avant : seulement celle convertie, 15 m/s au
                                     plus) ; 0 : l'ancien freinage */
#define KIN_BRAKE_VERT    0       /* 1 : le tir enleve aussi la vitesse verticale (le tireur se fige en l'air) ; 0 : on finit son saut/sa chute
                                     (choix par defaut : seule la vitesse horizontale sert a l'Elan) */
#define KIN_KNOCK         3.0f    /* recul de la cible a l'arret (PvE ; moitie en PvP) */
#define KIN_KNOCK_PER_MS  0.4f    /* recul en plus par m/s depensee : 3 -> 9 m/s */
#define KIN_FULL          14.5f   /* au-dela : tir "plein" (hitstop, son et rayon max) */
#define KIN_DASH_WINDOW   0.10f   /* dash-tir : le tir doit partir dans les 12 ticks apres le dash */
#define KIN_DASH_FORGET   0.6f    /* jusque-la, hors fenetre, la vitesse du dash ne compte pas */

/* Fusil a pompe "SHOTGUN" (ex-EVENTAIL) : 10 plombs, plein jusqu'a 10 m, x0.3 a 30 m */
#define SG_PELLETS        10
#define SG_HALF_ANGLE     0.0785f /* demi-angle du cone en radians (4.5 deg, cone total 9 deg) */
#define SG_SLIDE_SPREAD   0.5f    /* en slide : dispersion divisee par 2 */
#define SG_PELLET_DMG     20.0f   /* degats PvE par plomb (8 avant : x2,5) : 200 a bout portant. En PvP : x PVP_SCALE puis plafond
                                     PVP_HIT_CAP (60) par tir, inchange : la hausse etend surtout la portee du kill en 2 tirs */
#define SG_NEAR           10.0f   /* degats pleins jusqu'ici */
#define SG_FAR            30.0f   /* ... puis lineaire jusqu'a SG_FAR_MULT ici */
#define SG_FAR_MULT       0.3f
#define SG_RANGE          60.0f   /* portee max d'un plomb */
#define SG_CD             0.95f   /* recharge (divisee par streak_rate) */
#define SG_STREAK_MIN     3       /* plombs touches pour compter dans la chaine */
#define SG_PELLET_KNOCK   1.0f    /* recul PvE par plomb sur un ennemi (0.6 avant) : ~10 m/s a bout portant, environ 5 m pour un drone */
/* Recul : la VITESSE ACTUELLE du tireur (vitesse 3D juste avant le tir) est AJOUTEE a sa vitesse, a l'oppose de la visee : SG_KNOCK_RATIO m/s
 * par m/s, au sol comme en l'air ; immobile -> rien ; AUCUN PLAFOND (choix du proprietaire). Un joueur touche recoit une poussee forcee
 * de meme valeur dans le sens du tir (pendant SG_KNOCK_T s), x la part de plombs qui l'ont touche. */
#define SG_KNOCK_RATIO   1.0f     /* m/s de poussee par m/s de vitesse du tireur ("de meme vitesse") */
#define SG_KNOCK_MIN     0.0f     /* plancher de la poussee (m/s) : 0 = proportionnelle pure, un tireur immobile ne recule pas */
#define SG_KNOCK_MAX     0.0f     /* plafond de la poussee (m/s) : 0 = AUCUN plafond (choix du proprietaire) ; ex. 60 pour borner a 6 m */
#define SG_KNOCK_T       0.10f    /* duree de la poussee (s) ; le tireur garde sa vitesse ensuite */
#define SG_RECOIL         7.0f    /* residu de vitesse du tireur en l'air (le saut au recul : tir vers le bas) */
#define SG_RECOIL_VMAX    9.0f    /* le residu ne pousse pas vy au-dela (sauf si deja plus) */
#define SG_RECOIL_HCAP    16.0f   /* idem pour la vitesse horizontale */
#define SG_COIN_RADIUS    0.55f   /* zone de touche d'une piece pour un plomb (rail : 0.9) */
#define SG_BURST_RADIUS   3.0f    /* piece eclatee : souffle */
#define SG_BURST_DMG      45.0f   /* au centre, PvE (x PVP_SCALE contre un joueur) */
#define SG_BURST_KNOCK    8.0f
#define SG_BURST_CHAIN    3       /* une piece eclatee fait eclater les pieces voisines (profondeur max) */
#define SG_TRACER_SPEED   260.0f  /* vitesse visuelle d'un traceur */
#define MAXTR             160     /* traceurs de plombs (RICOCHET : un par segment de plomb) */

/* Axe corps a corps : degats et portee montent avec la vitesse */
#define MELEE_CD          0.45f
#define MELEE_RANGE       2.4f
#define MELEE_RANGE_PER_MS 0.12f  /* +0.12 m de portee par m/s */
#define MELEE_DMG         35.0f
#define MELEE_DMG_PER_MS  4.0f    /* +4 degats par m/s */
#define MELEE_CONE        0.55f   /* cosinus du demi-angle du coup */
#define PARRY_SPEED       36.0f   /* renvoi esquivable (ou re-parable) */
#define PARRY_DMG         150.0f  /* orbes renvoyees en solo */
#define PARRY_HEAL        10.0f
#define PARRY_WINDOW      0.12f   /* le parry n'est actif qu'au tout debut du coup */
#define MELEE_WHIFF_CD    0.9f    /* coup qui ne touche ni ne pare rien : recharge plus longue */
#define PARRY_ROCKET_MULT 1.25f   /* roquette renvoyee : 1,25x les degats d'origine */
#define PARRY_PINGPONG    1.15f   /* chaque re-parry : +15 % de vitesse et de degats */
#define PARRY_RANGE_SHRINK 0.85f  /* ... et une zone de parry 15 % plus courte */
#define PARRY_AIM_COS     0.988f  /* aide a la visee du renvoi : ~9 degres */
#define PVP_HIT_CAP       60.0f   /* en PvP, aucune touche ne retire plus de 60 PV (sauf tete et one-shot, arbitres par l'hote) */

/* Tirs a la tete (src/combat/degats.c, head_*) : seules les armes de precision a tir direct (rail, Elan) ont une zone tete,
 * et a la tete elles passent outre PVP_HIT_CAP. Le rail a la tete est un one-shot. L'hote valide la tete (host_handle) */
#define HEAD_R            0.24f   /* rayon de la sphere de tete d'un joueur */
#define HEAD_DROP         0.26f   /* centre de la tete sous le haut du joueur : debout a 1,54 m, accroupi a 0,84 m */
#define HEAD_MULT_RAIL    1.5f    /* rail a la tete : x1,5 et one-shot */
#define HEAD_MULT_KIN     1.35f   /* Elan a la tete : x1,35 sans plafond PvP (pas de one-shot : 90 PV au plus a pleine vitesse) */
#define HEAD_ENEMY_FRAC   0.45f   /* drones et rushers : touche au-dessus de 45 % du rayon = point faible (memes multiplicateurs) */
#define HEAD_HOST_TOL     0.6f    /* hote : ecart tolere entre le point d'impact declare et la tete qu'il voit (m)... */
#define HEAD_HOST_LAG     0.15f   /* ...plus le deplacement de la cible pendant 0,15 s (latence) */
#define HEAD_BOT_OFS      0.62f   /* bots : hauteur visee au-dessus du centre quand ils tentent la tete */

/* Chaine de tirs pleine (STREAK_MAX touches, src/combat/degats.c, chain_*) : une touche a la tete compte double ; pleine,
 * le prochain tir de rail ou d'Elan qui touche un joueur tue (one-shot arbitre par l'hote), puis la chaine repart de zero */
#define CHAIN_HEAD_STEP   2       /* une touche a la tete avance la chaine de 2 */
#define CHAIN_SEEN_T      0.5     /* hote : chaine vue pleine dans l'etat du tireur il y a moins de 0,5 s */

/* Jauge de style (src/jeu/style.c) : seuils des rangs C, B, A, S et plafond */
#define STYLE_C          60.0f
#define STYLE_B          130.0f
#define STYLE_A          220.0f
#define STYLE_S          330.0f
#define STYLE_MAX        420.0f
#define STYLE_KILL       45.0f    /* un kill */
#define STYLE_BONUS      30.0f    /* chaque bonus d'un kill (en l'air, vitesse, grappin, ricochet, enchaine...) */
#define STYLE_PARRY      55.0f    /* un parry reussi */
#define STYLE_FAST       20.0f    /* m/s : kill "PLEINE VITESSE" */
#define STYLE_CHAIN_T    3.0f     /* kills enchaines en moins de 3 s */
#define STYLE_SPEED_MIN  12.0f    /* au-dela, la vitesse remplit la jauge */
#define STYLE_SPEED_K    1.6f     /* points par seconde et par m/s au-dela de STYLE_SPEED_MIN */
#define STYLE_WALL_K     8.0f     /* points par seconde en wall-run ou au grappin a pleine vitesse */
#define STYLE_HURT_K     1.8f     /* points perdus par PV perdu */
#define STYLE_DECAY      10.0f    /* baisse par seconde (rang D) ... */
#define STYLE_DECAY_RANK 6.0f     /* ... plus 6 par rang au-dessus */

/* Modes de jeu (src/reseau/modes.c) */
#define ZONE_R           5.0f     /* 3 ZONES : rayon d'une zone (a l'horizontale) */
#define ZONE_CAP_T       6.0f     /* 3 ZONES : secondes pour prendre une zone seul (plus vite a plusieurs) */
#define ZONE_SCORE       200      /* 3 ZONES : points pour gagner (1 par seconde et par zone tenue) */
#define FLAG_TOUCH_R     2.2f     /* DRAPEAU : distance pour le prendre ou capturer */
#define FLAG_RETURN_R    3.5f     /* DRAPEAU : distance pour le rendre (rester a cote) */
#define FLAG_RETURN_T    5.0f     /* DRAPEAU : secondes a cote sans etre touche pour le rendre */
#define FLAG_RETURN_HIT  0.6      /* DRAPEAU : touche depuis moins de 0,6 s = le retour repart de zero */
#define FLAG_AUTO_RETURN 30.0f    /* DRAPEAU : tombe et oublie 30 s = rendu tout seul */
#define FLAG_CAPS        3        /* DRAPEAU : captures pour gagner */
#define SHIELD_ZONE_R    6.0f     /* BOUCLIER : rayon de la zone du generateur */
#define SHIELD_DRAIN     1.6f     /* BOUCLIER : % par seconde avec un attaquant seul dessus (+35 % par attaquant en plus) */

/* Economie et boutique (src/jeu/economie.c) */
#define ECO_START        400      /* pieces au debut du match */
#define ECO_KILL         200
#define ECO_DEATH        100      /* consolation a chaque mort */
#define ECO_OBJ          300      /* objectif personnel (zone prise, drapeau pris ou rendu, capture) */
#define ECO_TEAM_OBJ     100      /* objectif de l'equipe */
#define ECO_AMMO_PRICE   250      /* MUNITIONS + : jauges pleines a chaque respawn (pour le match) */
#define ECO_STARTER_T    10.0f    /* secondes pour choisir l'arme de depart */

/* Mode COURSE (src/jeu/course.c) */
#define RACE_COUNTDOWN   3.0f     /* decompte avant le depart */
#define SWING_HALF       1.5f     /* points de balancement : cubes de 3 m */
#define RACE_MAGNET_COS  0.70f    /* course : le grappin part vers le point de balancement a moins de ~45 deg du viseur */
#define RACE_SWING_PULL  0.25f    /* course : une fois la corde a RACE_ROPE, traction x0,25 (on se balance plus qu'on ne file) */
#define RACE_ROPE        18.0f    /* course : le grappin ramene la corde a 18 m (bas de l'arc ~ hauteur des portes), puis balancier */
#define RACE_SWING_PUMP  14.0f    /* course : avancer au grappin pousse de 14 m/s2 le long du balancement */
#define MELEE_PARRIED_CD  1.0f    /* attaquant dont l'epee est paree : epee bloquee */
#define MELEE_PARRY_GRACE 0.15f   /* tolerance de latence pour l'hote */

/* --- Enclume (marteau-pilon) : la melee verticale, elle recompense la hauteur --- */
/* coup au sol (ou en l'air trop pres du sol) */
#define ANVIL_CD           0.8f    /* recharge du coup au sol */
#define ANVIL_WHIFF_CD     1.2f    /* coup au sol dans le vide */
#define ANVIL_SWING_T      0.3f    /* duree de l'animation du coup */
#define ANVIL_RANGE        2.6f    /* portee de base */
#define ANVIL_RANGE_PER_MS 0.10f   /* portee en plus par m/s relatif */
#define ANVIL_DMG          45.0f   /* degats de base */
#define ANVIL_DMG_PER_MS   3.0f    /* degats en plus par m/s relatif */
#define ANVIL_CONE         0.45f   /* cosinus du demi-angle (plus large que l'epee) */
#define ANVIL_KNOCK        14.0f   /* recul vers l'avant */
#define ANVIL_KNOCK_UP     6.0f    /* recul vers le haut */
#define ANVIL_PARRY_WINDOW 0.10f   /* fenetre de parry, plus courte que l'epee */
/* pique et onde de choc */
#define SLAM_VY            30.0f   /* vitesse de chute minimale du pique */
#define SLAM_VY_MAX        45.0f   /* plafond de |vy| */
#define SLAM_MIN_H         1.2f    /* hauteur minimale sous les pieds pour piquer (sinon : coup au sol) */
#define SLAM_FULL_H        4.0f    /* hauteur de chute pour 100% des degats (0 = desactive) */
/* onde (plan 1b, PLAN_SUITE.md) : H = hauteur de chute, vh = vitesse horizontale a l'impact */
#define SLAM_R_MIN         2.5f    /* rayon : 2,5 + 0,3 x H, de 2,5 a 9 m */
#define SLAM_R_PER_M       0.3f
#define SLAM_R_MAX         9.0f
#define SLAM_WH_MIN        0.6f    /* hauteur du mur : 0,6 + 0,1 x H, de 0,6 a 2,6 m (pieds au-dessus : esquive) */
#define SLAM_WH_PER_M      0.1f
#define SLAM_WH_MAX        2.6f
#define SLAM_FWD_PER_MS    0.04f   /* ovale : vers l'avant x(1 + 0,04 x vh), 1,8 au plus... */
#define SLAM_FWD_MAX       1.8f
#define SLAM_BACK_PER_MS   0.02f   /* ...vers l'arriere x(1 - 0,02 x vh), 0,6 au moins ; sur les cotes : le rayon */
#define SLAM_BACK_MIN      0.6f
#define SLAM_WAVE_SPEED    28.0f   /* propagation du front (m/s ; 0,3 s pour 8,5 m) */
#define SLAM_DMG           25.0f   /* degats de base de l'onde */
#define SLAM_DMG_PER_MS    1.5f    /* degats en plus par m/s de chute */
#define SLAM_REACH_UP      1.0f    /* contact direct : une cible a plus de 1 m du sol est touchee par le marteau, pas par l'onde */
#define SLAM_REACH_DOWN    1.0f    /* l'onde ne touche pas ce qui est plus bas que le sol d'impact */
#define SLAM_KNOCK_UP_MIN  6.0f    /* recul vertical des cibles : 6 + 3 x hauteur du mur */
#define SLAM_KNOCK_OUT     8.0f    /* recul horizontal au centre */
#define SLAM_BOUNCE        9.0f    /* rebond si l'onde touche */
#define SLAM_STUN          0.6f    /* immobilisation si l'onde rate : 0,6 s + 0,02 s par metre de chute, 1 s au plus */
#define SLAM_STUN_PER_M    0.02f
#define SLAM_STUN_MAX      1.0f
#define SLAM_CD            0.8f    /* recharge apres l'atterrissage */
#define SLAM_BREAK_VY      -15.0f  /* poussee vers le haut qui casse le pique */
#define SLAM_PARRY_GRACE   0.2f    /* tolerance de l'hote pour parer l'onde */
#define SLAM_PARRIED_UP    12.0f   /* attaquant pare : renvoye en l'air */
#define ST_SLAM            0x10    /* NetPState.pad : pique en cours */
enum { ANVIL_IDLE, ANVIL_DIVE, ANVIL_STUN };

/* --- Scie : disque-boomerang (une seule, ni jauge ni recharge : la scie est sa munition) --- */
#define SAW_SPEED        32.0f   /* vitesse de lancer, plus la vitesse du joueur dans l'axe */
#define SAW_MIN_SPEED    20.0f   /* plancher si on lance en reculant */
#define SAW_GRAVITY      4.5f    /* gravite legere : ~1 m de chute sur 22 m */
#define SAW_SPAWN_UP     0.25f   /* la scie part au-dessus des yeux (elle partait trop bas) */
#define SAW_DMG          30.0f   /* degats de base... */
#define SAW_DMG_PER_MS   1.5f    /* ...plus 1,5 par m/s de vitesse relative au-dela de l'offset */
#define SAW_REL_OFFSET   12.0f   /* cible immobile (rel 32) : 60 PvE / 42 PvP, 3 touches (choix du 27/10) */
#define SAW_CD           0.4f    /* delai minimal entre deux lancers */
#define SAW_RETURN_SPEED 36.0f   /* retour guide apres une touche */
#define SAW_RECALL_SPEED 45.0f   /* rappel au grappin */
#define SAW_HOME_MAX     2.5f    /* retour ou rappel plus long : rattrapee d'office */
#define SAW_CATCH_R      1.5f    /* distance de ramassage / rattrapage */
#define SAW_HOOK_R       1.2f    /* sphere que le crochet doit toucher pour rappeler la scie plantee (0,6 avant : hitbox x2) */
#define SAW_RADIUS       0.35f   /* rayon du disque (rendu) */
#define SAW_EMBED        0.12f   /* profondeur d'enfoncement dans la face */
#define SAW_HIT_GROW     0.25f   /* AABB des joueurs agrandie */
#define SAW_DASH         0.5f    /* dash rendu au rattrapage apres une touche */
#define SAW_COIN_R       0.8f    /* frolement d'une piece */
#define SAW_COIN_KICK    11.0f   /* vitesse verticale donnee a la piece */
#define SAW_COIN_LIFE    2.0f    /* vie ajoutee a la piece */
#define SAW_PARRY_MULT   1.25f   /* scie paree par un adversaire : 1,25x sa vitesse (au moins PARRY_SPEED) */
#define SAW_BOUNCE_MULT  1.5f    /* renvoi : 1,5x la vitesse de lancer (48 m/s), plus la vitesse du joueur */
#define SAW_BOUNCE_MAX   70.0f   /* vitesse maximale d'un renvoi */
#define SAW_BOUNCE_AIM_COS 0.9925f /* aide a la visee du renvoi : 7 degres (parry adverse : 0.988) */
#define SAW_BOUNCE_AIM_RANGE 40.0f /* portee de l'aide a la visee */
#define SAW_BOUNCE_CHAIN 3       /* renvois consecutifs max sans rattraper la scie */
#define SAW_RETURN_PARRY_DMG_MULT 0.9f /* renvoi par parry : lancer + renvoi ne tue plus une cible immobile (choix du 27/10) */
#define SAW_LOST_T       3.0f    /* sous le plan de mort : revient apres 3 s */
#define SAW_LOST_Y       -20.0f  /* plan de mort des cartes fermees (sinon killY de la carte, map_lost_y) */
#define SAW_FLY_MAX      12.0f   /* garde-fou : vol trop long = perdue */
#define SAW_RESYNC       2.0f    /* reemission periodique de l'etat plante (joueurs arrives en cours) */
#define SAW_WHIRR        0.18f   /* vrombissement rejoue en vol */
#define SAW_RECALL_HURTS 0       /* option : la scie rappelee blesse le premier adversaire traverse */
enum { SAW_FLY, SAW_PARRIED, SAW_PLANTED, SAW_RETURN, SAW_RECALL, SAW_LOST };
enum { SAWE_THROW, SAWE_PLANT, SAWE_HIT, SAWE_CATCH, SAWE_RECALL, SAWE_BOUNCE };       /* EV_SAW.sub */
enum { SAW_HOW_RETURN, SAW_HOW_FOOT, SAW_HOW_RECALL, SAW_HOW_LOST, SAW_HOW_PARRIED };   /* EV_SAW.actor (rattrapage) */

/* --- Tesla : arc electrique, faisceau continu, chaine, relais sur piece --- */
#define TESLA_PULSE        0.1f    /* 10 impulsions par seconde (P.fireCd) */
#define TESLA_COST         2       /* batterie par impulsion : 20 par seconde, aucune recharge passive */
#define TESLA_RANGE        14.0f   /* portee de base */
#define TESLA_RANGE_PER_MS 0.3f    /* portee en plus par m/s de vitesse (vitesse totale) */
#define TESLA_RANGE_MAX    28.0f
#define TESLA_AIM_PAD      0.4f    /* aide a la visee : les cibles grossissent de 0,4 m (cylindre, pas un cone) */
#define TESLA_DMG          7.0f    /* par impulsion, cible principale */
#define TESLA_CHAIN_N      2       /* sauts en plus */
#define TESLA_CHAIN_R      6.0f    /* distance max d'un saut */
#define TESLA_CHAIN_MULT   0.6f
#define TESLA_RELAY_T      2.0f    /* duree du relais, puis la piece grille */
#define TESLA_RELAY_R      6.0f
#define TESLA_RELAY_DMG    5.0f
#define TESLA_RELAY_MAX    3       /* relais actifs par joueur */
#define TESLA_AGG          0.2f    /* cumul : un envoi (net_hit / damage_enemy) par cible toutes les 0,2 s */
#define TESLA_KNOCK_PVE    0.3f    /* petit recul par impulsion sur les drones (aucun en PvP) */
#define TESLA_STUN         0.4f    /* tireur dont la decharge est paree : plus de tir a distance */
#define TESLA_REFLECT_DMG  15.0f   /* PvP direct, ni multiplie ni plafonne */
#define TESLA_PARRY_GRACE  0.25f   /* tolerance de l'hote : fenetre de cumul + latence */
#define TESLA_LOCK_T       0.3f    /* victime : l'arc dessine finit sur moi apres un EV_HIT W_TESLA */
/* ALL IN ONE avec la Tesla : un eclair inevitable (instantane, il s'accroche a la cible la plus proche du viseur, en vue) qui
 * vide la batterie ; degats proportionnels a la batterie depensee, chaine comme l'arc (TESLA_CHAIN_N sauts a TESLA_CHAIN_MULT) */
#define TESLA_BOLT_RANGE   45.0f   /* portee de l'eclair */
#define TESLA_BOLT_CONE    0.985f  /* cosinus du demi-angle (~10 deg) autour du viseur ou il choisit sa cible */
#define TESLA_BOLT_MIN     20      /* batterie minimale : en dessous, ALL IN ONE refuse */
#define TESLA_BOLT_DMG     130.0f  /* degats PvE a pleine batterie (100) : x batterie / 100 ; PvP : x PVP_SCALE puis plafond 60 */
#define ST_ZAP             0x80    /* NetPState.pad : arc Tesla actif */
enum { TK_NONE, TK_MUZZLE, TK_HAND, TK_ENEMY, TK_PLAYER, TK_COIN, TK_POINT };   /* extremites d'un lien d'arc */
/* --- fin Tesla --- */
/* --- fin Scie --- */

/* --- Lance de joute : estoc a l'appui, lance couchee si la touche reste tenue ; la joute part toute seule --- */
#define LANCE_TAP          0.15f   /* relache avant : estoc seul ; tenu apres : lance couchee */
#define LANCE_HOLD_MAX     2.0f    /* duree maximale de la lance couchee (depuis l'appui) */
#define LANCE_REACH        3.5f    /* portee de la lance couchee (bord de la cible) */
#define LANCE_CONE         0.92f   /* cos du demi-angle (~23 degres), autour de la vue */
#define LANCE_VC_MIN       10.0f   /* vitesse de rapprochement minimale pour toucher */
#define LANCE_VC_CAP       30.0f   /* plafond de v_c pour les degats (160 PvE) */
#define LANCE_DMG          10.0f
#define LANCE_DMG_PER_MS   5.0f    /* degats en plus par m/s de rapprochement */
#define LANCE_GIVE         0.6f    /* part de ta vitesse transmise a la cible */
#define LANCE_KEEP         0.5f    /* part de ta vitesse que tu gardes */
#define LANCE_KNOCK_MAX    28.0f   /* norme maximale du recul transmis */
#define LANCE_HIT_CD       1.0f    /* recharge apres une joute reussie */
#define LANCE_MISS_CD      0.5f    /* recharge apres une lance couchee sans touche */
#define LANCE_PARRIED_CD   1.5f    /* lance paree ou joute perdue */
#define LANCE_THRUST_DMG   30.0f   /* estoc */
#define LANCE_THRUST_PER_MS 2.0f
#define LANCE_THRUST_RANGE 3.0f
#define LANCE_THRUST_CONE  0.80f   /* estoc : cone plus etroit que l'epee */
#define LANCE_THRUST_CD    0.5f
#define LANCE_CLASH_DIFF   2.0f    /* ecart de vitesse pour gagner un choc de joute */
#define LANCE_CLASH_COS    0.85f   /* hote : la lance adverse doit viser l'attaquant (tolerance) */
#define LANCE_CLASH_GRACE  0.15f   /* hote : tolerance de latence sur ST_LANCE */
#define LANCE_JOUST_LOCK   0.3f    /* hote : une joute resolue ignore la touche reciproque */
#define LANCE_UNHORSE      11.0f   /* recul des deux joueurs desarconnes */
#define LANCE_PREVIEW      30.0f   /* HUD : distance max de l'apercu de v_c */
#define ST_LANCE           0x20    /* NetPState.pad : lance couchee */
enum { LS_IDLE, LS_THRUST, LS_COUCH };
/* --- fin Lance --- */

/* --- Ventouse : mines collantes, detonation a distance (deux commandes : toutes, ou la plus ancienne) --- */
#define MINE_SPEED         24.0f   /* vitesse de lancer, ajoutee a la vitesse du joueur */
#define MINE_GRAVITY       16.0f   /* comme les pieces */
#define MINE_CD            0.55f   /* entre deux lancers */
#define MINE_MAX_ACTIVE    2       /* la 3e fait sauter la plus ancienne */
#define MINE_FLY_LIFE      3.0f    /* une mine qui ne colle a rien explose en l'air */
#define MINE_FUSE          3.0f    /* explosion automatique, comptee depuis le collage */
#define MINE_ARM_FLY       0.10f   /* detonation possible apres 0,1 s de vol */
#define MINE_ARM_PLAYER    0.6f    /* collee sur un joueur : temps laisse pour parer */
#define MINE_RADIUS        4.0f
#define MINE_DMG           90.0f   /* PvE au centre, 30 % au bord */
#define MINE_KNOCK         15.0f   /* recul (roquette : 13) */
#define MINE_BOOST_MULT    1.5f    /* rail ou piece sur une mine */
#define MINE_BOOST_RADIUS  5.0f
#define MINE_HIT_R         0.45f   /* rayon de touche d'une mine (rail, plomb, arc) */
#define MINE_STICK_GROW    0.45f   /* AABB des joueurs et rayon des drones agrandis pour coller (0,15 avant) */
#define MINE_PROX_R        2.0f    /* piege : une mine posee (decor) explose seule si un adversaire passe a moins de 2 m... */
#define MINE_PROX_ARM      0.4f    /* ...une fois armee, 0,4 s apres le collage (le poseur fait autorite) */
#define MINE_SKIN          0.06f   /* decollement de la surface */
#define MINE_BEEP_SLOW     0.5f    /* intervalle du bip au collage */
#define MINE_BEEP_FAST     0.08f   /* intervalle du bip juste avant l'explosion */
enum { MS_NONE, MS_FLY, MS_WORLD, MS_ENEMY, MS_PLAYER };
enum { MINE_EV_THROW, MINE_EV_WORLD, MINE_EV_PLAYER, MINE_EV_BOOM, MINE_EV_TORN, MINE_EV_CANCEL };   /* EV_MINE.sub */
enum { MX_REMOTE, MX_FUSE, MX_THIRD, MX_SHOT, MX_FLY, MX_PROX, MX_COUNT };   /* origine des explosions (journal) */
/* --- fin Ventouse --- */
/* --- Fleau a chaine (Yoyo) : arc de 140 degres a longue portee. APPUI COURT : le coup en arc, SANS traction (la cible touchee ne
 * bouge pas). APPUI LONG (touche de melee tenue FLAIL_HOLD_T apres l'appui) : la chaine s'accroche aux cibles de l'arc et les TIRE
 * vers le lanceur tant que la touche est tenue (aucune limite de duree). Le lanceur pare est tire vers le pareur (FLAIL_PARRIED_*) --- */
#define FLAIL_RANGE         6.0f    /* portee de base */
#define FLAIL_RANGE_PER_MS  0.05f   /* +0.05 m par m/s de vitesse relative */
#define FLAIL_ARC_COS       0.34f   /* cos du demi-angle horizontal (70 deg, arc de 140) */
#define FLAIL_VSLAB         0.6f    /* ecart vertical max autour du pitch (rad) */
#define FLAIL_CD            0.6f
#define FLAIL_WHIFF_CD      0.9f    /* coup dans le vide */
#define FLAIL_DMG           18.0f
#define FLAIL_DMG_PER_MS    1.5f
#define FLAIL_ANIM          0.30f   /* duree de l'animation de la chaine */
#define FLAIL_PARRY_WINDOW  0.10f
#define FLAIL_PARRY_RANGE   6.0f    /* portee de parry des projectiles */
#define FLAIL_PARRY_SPEED   0.8f    /* renvoi plus lent */
#define FLAIL_PARRIED_PULL  16.0f   /* fleau pare : le lanceur est tire vers le pareur (0 = plus de traction du tout) */
#define FLAIL_PARRIED_UP    4.0f
#define FLAIL_COIN_SPEED    45.0f   /* piece frappee */
#define FLAIL_COIN_DMG      40.0f   /* x PVP_SCALE = 28 en PvP */
#define FLAIL_COIN_LIFE     1.5f    /* 67 m de vol */
#define FLAIL_COIN_SEEK     60.0f   /* portee de recherche de cible */
#define FLAIL_COIN_HIT_R    0.4f    /* marge de touche de la piece frappee */
/* traction (appui long) : le coup en arc part a l'appui comme avant ; si la touche est encore tenue FLAIL_HOLD_T apres l'appui, la
 * chaine s'accroche aux cibles de l'arc A CET INSTANT (memes regles de portee, d'arc et de ligne de vue), sans nouveaux degats */
#define FLAIL_HOLD_T        0.15f   /* tenue plus longtemps apres l'appui : traction (comme la Lance couchee et le tap de l'Egide) */
#define FLAIL_HOOK_MAX      8       /* cibles accrochees au plus, les plus proches d'abord */
#define FLAIL_PULL_SPEED    12.0f   /* vitesse de traction vers le lanceur (m/s) : plus vite que la course (RUN_SPEED 9,5) */
#define FLAIL_PULL_RESPONSE 14.0f   /* ennemis : leur vitesse rejoint la consigne en ~70 ms (1/s), comme le VORTEX mais lisse */
#define FLAIL_PULL_ACCEL    140.0f  /* joueurs : acceleration de la traction (m/s2) ; plus que la course au sol (GROUND_ACCEL x RUN_SPEED = 114) */
#define FLAIL_DRONE_STOP    2.5f    /* ennemis : un ennemi tire s'arrete a cette distance de l'oeil */
#define FLAIL_PULL_STOP     2.0f    /* joueurs : une victime tiree s'arrete a cette distance du lanceur */
#define FLAIL_PULL_GAIN     6.0f    /* on ralentit en arrivant : vitesse de consigne <= GAIN x (distance - arret) */
#define FLAIL_PULL_SLACK    2.0f    /* la traction se coupe si la cible s'eloigne au-dela de FLAIL_RANGE + SLACK (m) */
#define FLAIL_PULL_LOS_T    0.25f   /* un mur qui coupe la ligne de vue plus longtemps que cela coupe la traction (s) */
#define FLAIL_PULL_SNAP     16.0f   /* PvP : une victime qui s'eloigne plus vite que cela (dash 18, slide ou bhop rapide, grappin) arrache la chaine */
#define FLAIL_PULL_REFRESH  0.10f   /* PvP : EV_FLAIL renvoye toutes les 0,1 s par le lanceur tant que la touche est tenue */
#define FLAIL_PULL_LEASE    0.30f   /* PvP : bail cote victime : sans nouvelle depuis 0,3 s, la traction s'arrete (3 rafraichissements perdus) */
#define FLAIL_PULL_END_CD   0.5f    /* recharge de melee apres la traction (a partir du relachement ou de la coupure) */
#define FLAIL_HOST_TOL      3.0f    /* PvP : tolerance de latence de l'hote sur la portee de l'accroche (m) */
#define FLAIL_HOST_PARRY_T  0.6f    /* PvP : l'hote refuse l'accroche d'un fleau pare dans ce delai (la parade peut arriver apres l'appui long chez un client lent) (s) */
enum { FLE_START, FLE_PULL, FLE_END, FLE_BREAK };         /* EV_FLAIL.sub : accroche, rafraichissement, fin (lanceur), chaine rompue (victime) */
/* --- fin Fleau --- */
/* --- Poings a impulsion : combo de 3 coups (le 3e est un uppercut), punch-jump sur le decor --- */
#define FIST_GAP            0.22f   /* entre deux coups du combo */
#define FIST_END_CD         0.70f   /* recharge apres le 3e coup */
#define FIST_COMBO_WIN      0.45f   /* au-dela, sans nouveau coup, le combo revient au coup 1 */
#define FIST_WHIFF_CD       0.60f   /* combo sans touche ni parry : recharge en plus */
#define FIST_BUFFER         0.12f   /* un appui un peu en avance est memorise */
#define FIST_ANIM           0.16f   /* duree de l'animation d'un coup */
#define FIST_RANGE          1.9f
#define FIST_RANGE_PER_MS   0.06f
#define FIST_CONE           0.6f    /* cosinus du demi-angle (~53 degres), une seule cible */
#define FIST_DMG            14.0f   /* coups 1 et 2 */
#define FIST_DMG_PER_MS     1.5f
#define FIST_UP_DMG         20.0f   /* coup 3 : uppercut */
#define FIST_UP_DMG_PER_MS  2.5f
#define FIST_KNOCK          2.5f    /* recul faible : la cible reste a portee */
#define FIST_UP_KNOCK       11.0f   /* uppercut : recul vertical */
#define FIST_AIR_HANG       2.0f    /* touche en l'air sur une cible en l'air : on reste suspendu */
#define FIST_DASH           0.35f   /* dash rendu par coup qui touche */
#define FIST_PARRY_WIN      0.06f   /* fenetre de parry de chaque coup */
#define FIST_PARRY_GRACE    0.05f   /* tolerance de l'hote quand la victime joue les Poings */
#define FIST_STAGGER_KNOCK  8.0f    /* un recul recu au moins aussi fort casse le combo */
#define FIST_STAGGER_CD     0.3f
#define FIST_PJ_RANGE       1.9f    /* coup dans le decor : distance maximale */
#define FIST_PJ_FLOOR_RANGE 1.9f    /* idem pour le sol (2.6 propose, voir fiche) */
#define FIST_PJ_FLOOR_DELAY 0.1f    /* pas de coup au sol dans les 0,1 s qui suivent le decollage (super-saut) */
#define FIST_PJ_PUSH        11.0f   /* composante normale minimale apres l'impulsion */
#define FIST_PJ_HCAP        20.0f   /* le punch-jump n'accelere plus au-dela (il reoriente) */
#define FIST_PJ_CD          0.25f   /* partage avec P.wallJumpCd */
#define FIST_UP_COMBO_T     0.5f    /* bot : apres un uppercut qui touche, le rail suit */
/* --- fin Poings --- */
/* --- Egide (bouclier) : maintenir = leve, cone frontal, jauge tenue par l'hote, parry parfait qui renvoie le rail --- */
#define SHIELD_COS           -0.34f  /* bloque si dot(dir du coup, face) < -0.34 : demi-angle 70 deg */
#define SHIELD_COS_HOST      -0.17f  /* tolerance de l'hote pour confirmer un blocage : demi-angle 80 deg */
#define SHIELD_GAUGE_MAX     120.0f  /* jauge : degats PvP absorbables */
#define SHIELD_REGEN         30.0f   /* regeneration par seconde, bouclier baisse */
#define SHIELD_BROKEN_T      3.0f    /* bouclier brise : indisponible 3 s */
#define SHIELD_BLAST_FRAC    0.5f    /* part d'un souffle bloquee de face */
#define SHIELD_KNOCK_MULT    0.5f    /* recul d'un coup bloque */
#define SHIELD_GROUND_SPEED  7.0f    /* vitesse au sol bouclier leve */
#define SHIELD_BRAKE         40.0f   /* freinage au sol au-dessus de 7 m/s */
#define SHIELD_AIR_MULT      0.5f    /* acceleration aerienne bouclier leve */
#define SHIELD_TAP           0.15f   /* relache avant : coup de bouclier */
#define SHIELD_PARRY_WIN     0.15f   /* fenetre du parry parfait apres la levee */
#define SHIELD_REARM         0.3f    /* baisse au moins 0.3 s pour rouvrir la fenetre parfaite */
#define SHIELD_GRACE         0.15f   /* tolerance de latence de l'hote (comme MELEE_PARRY_GRACE) */
#define SHIELD_REFLECT_PVP   60.0f   /* rail renvoye par un parry parfait */
#define SHIELD_RIP_T         1.0f    /* bouclier arrache par un grappin adverse */
#define SHIELD_BASH_DMG      25.0f
#define SHIELD_BASH_PER_MS   3.0f
#define SHIELD_BASH_RANGE    2.2f
#define SHIELD_BASH_KNOCK    14.0f
#define SHIELD_BASH_CD       0.5f
#define SHIELD_BASH_WHIFF_CD 0.8f    /* coup de bouclier dans le vide */
#define SHIELD_BASH_ANIM     0.18f   /* animation de poussee */
#define SHIELD_HOST_CONVERT  1       /* l'hote convertit en bloc un coup direct qu'il voit arriver de face (favorise le defenseur) */
#define ST_SHIELD            0x40    /* NetPState.pad : bouclier leve */
enum { SHS_DOWN, SHS_UP, SHS_BROKEN };
enum { SHE_CLAIM, SHE_CLAIM_BLAST, SHE_BLOCK, SHE_BREAK, SHE_PERFECT, SHE_DENIED, SHE_RIP };   /* EV_SHIELD.sub */
enum { SHB_DIRECT, SHB_BLAST, SHB_NEVER };                /* hit_player : coup direct, souffle (50 %), jamais bloque */
/* --- fin Egide --- */

/* Axe tir : chaque touche successive augmente degats et cadence, un tir rate remet a zero */
#define STREAK_MAX        8
#define STREAK_DMG        0.15f   /* +15% de degats par touche */
#define STREAK_RATE       0.05f   /* +5% de cadence par touche */

/* Loadout : 1 arme de melee + 2 armes a distance. Aucune recharge passive : chaque arme a une jauge
 * de points, remplie par la melee et les cellules ; jauge pleine = 1 munition. */
enum { MK_SWORD, MK_ANVIL, MK_LANCE, MK_FLAIL, MK_FIST, MK_SHIELD, MK_COUNT };
enum { WP_RAIL, WP_ROCKET, WP_SHOTGUN, WP_KINETIC, WP_SAW, WP_MINE, WP_TESLA, WP_COUNT };
enum { AP_HIT, AP_KILL, AP_PARRY, AP_CELL, AP_LOST, AP_COUNT };   /* origine des points (journal) */
#define PTS_MELEE_KILL     100     /* kill en melee, toutes armes */
#define PTS_PARRY          50      /* parry reussi, une fois par coup */
#define PTS_LANCE_JOUST    100     /* joute de la Lance (l'estoc = meleePts) */
#define PTS_FIST_COMBO_MAX 50      /* Poings : au plus 50 points par combo */
#define PTS_SHIELD_PERFECT 100     /* Egide : parry parfait (remplace PTS_PARRY) */
#define PICKUP_SMALL       100     /* petite cellule : points a chaque arme du loadout */
#define PICKUP_BIG         150     /* grande cellule (endroits exposes) */
#define REFUND_UNDO_T      0.5f    /* coup declare pare par l'hote : on rend ses points s'il a moins de 0,5 s */
#define SWITCH_TIME        0.22f   /* changement d'arme */
#define WHEEL_QUIET        0.15    /* molette : repos (s) qui separe deux gestes ; une rafale = un seul changement */
#define WHEEL_COOLDOWN     0.30    /* molette : delai minimal (s) entre deux changements */
#define WP_FLASH_T         0.9     /* nom de l'arme affiche (s) apres un changement */
#define SCREEN_POLL_T      1.0f    /* second ecran : on recompte les moniteurs toutes les secondes */
#define SCREEN_MAP_FPS     30.0f   /* images par seconde de la carte sur le second ecran */
#define MAPS_HEAT_R        9.0f    /* carte de chaleur : rayon d'une tache (m) */
#define KILLCAM_T          1.6f    /* camera de mort : duree (s), avant l'ecran de respawn (RESPAWN_TIME 2,5 s) */
#define KILLCAM_TURN       0.45f   /* ...temps pour pivoter vers le tueur (s) */
#define KILLCAM_BACK       2.5f    /* ...recul (m) et montee (m) de la camera */
#define KILLCAM_UP         1.2f
#define KILLCAM_ZOOM       0.35f   /* ...zoom sur le tueur (part du champ de vision retiree) */
#define KILLCAM_SOLO_R     60.0f   /* solo : ennemi le plus proche qui me voit, dans ce rayon (m) */
#define HP_GHOST_HOLD      0.4     /* barre de vie : la partie perdue reste affichee (s)... */
#define HP_GHOST_RATE      80.0f   /* ...puis se vide a cette vitesse (PV/s) */
#define DRYFIRE_CD         0.35f   /* delai entre deux clics a sec */
#define ST_MASK            0xF0    /* NetPState.pad : bits 4-7 = etats d'arme, bits 0-3 = meleeKind */

/* points de melee par coup qui touche (Lance : estoc ; Egide : coup de bouclier) */
static const int meleePts[MK_COUNT] = { 50, 75, 50, 35, 25, 50 };

/* gauge : points pour gagner 1 munition (0 = pas de jauge). Tesla : points par unite de batterie */
typedef struct { const char *name; int start, max; float gauge; float r, g, b; } AmmoDef;
static const AmmoDef ammoDef[WP_COUNT] = {
    { "RAILGUN",   2,  2,   250, 0.40f, 0.85f, 1.00f },   /* tres haut degats : 2 tirs, tres lent */
    { "ROQUETTES", 4,  8,   50,  1.00f, 0.60f, 0.20f },   /* 1 coup d'epee = 1 roquette */
    { "SHOTGUN",   3,  6,   100, 1.00f, 0.85f, 0.35f },   /* ex-EVENTAIL */
    { "ELAN",      4,  4,   80,  0.55f, 1.00f, 0.55f },   /* 4 munitions, au depart comme au plus (3 au depart et 6 au plus avant) */
    { "SCIE",      1,  1,   0,   0.90f, 0.90f, 0.95f },   /* objet unique : aucune jauge */
    { "VENTOUSE",  2,  3,   150, 1.00f, 0.35f, 0.35f },   /* 2 mines au depart (1 avant la retouche) */
    { "TESLA",     60, 100, 2.5f, 0.60f, 0.70f, 1.00f },  /* 1 point = 0,4 de batterie */
};
static const char *meleeNames[MK_COUNT] = { "EPEE", "ENCLUME", "LANCE", "YOYO", "IMPULSION", "EGIDE" };
static unsigned char wpReady[WP_COUNT] = { 1, 1, 1, 1, 1, 1, 1 };   /* chaque arme codee passe sa case a 1 */
static unsigned char mkReady[MK_COUNT] = { 1, 1, 1, 1, 1, 1 };
/* competences : nom, charges max, secondes par charge, couleur, description (LOADOUT) */
typedef struct { const char *name; int max; float recharge; float r, g, b; const char *desc; } SkillDef;
static const SkillDef skillDef[SK_COUNT] = {
    { "PIECE",      COIN_MAX, COIN_RECHARGE, 1.00f, 0.82f, 0.30f, "lance une piece : rail, elan ou roquette qui la touche ricoche (x1,5)" },
    { "MARQUE",     2, 6,  0.85f, 0.35f, 1.00f, "marque une cible 4 s, visible a travers les murs : tes touches x1,25" },
    { "CHARGE",     1, 8,  1.00f, 0.55f, 0.20f, "maintenir 0,6 s : ta prochaine touche dans les 2 s fait x1,5 (perdue si touche)" },
    { "RICOCHET",   1, 20, 0.40f, 1.00f, 0.70f, "6 s : tout ce que tu tires rebondit sans fin, jusqu'a sortir de la carte" },
    { "ALL IN ONE", 1, 20, 1.00f, 0.30f, 0.30f, "toute la munition de l'arme en main d'un coup (Tesla : un eclair inevitable)" },
    { "MIRAGE",     2, 7,  0.40f, 0.85f, 1.00f, "leurre immobile 3 s : absorbe un tir, ton rail y ricoche, attire les pieces adverses" },
    { "AIMANT",     1, AIM_RECHARGE, 1.00f, 0.95f, 0.45f, "4 s : le viseur est attire vers la cible proche du centre" },
};
static int wantSkill = SK_COIN;
static int botSkillSet;                                   /* --skill / --loadout a,b,m,s : competence imposee au bot */
static const float wpCd[WP_COUNT] = { RAIL_CD, ROCKET_CD, SG_CD, KIN_CD, 0.4f, MINE_CD, 0.1f };  /* barre de recharge du HUD */

/* --- Mode VAGUES (src/solo/, docs/CODE.md 3.5) ---------------------------------------------------------------------
 * 100 vagues, une carte choisie toutes les 10, boss aux vagues 25 / 50 / 75 / 100, puis l'INFINI.
 * Regle du proprietaire : AUCUNE inflation. Les degats des ennemis sont fixes (ceux de leur type) ; seuls leurs PV
 * montent, et peu (VG_HP_MAX) ; les achats de la boutique changent un comportement, jamais une stat. */
#define VG_WAVES          100     /* vague du dernier boss ; au-dela : l'INFINI */
#define VG_BLOCK          10      /* selecteur de carte toutes les 10 vagues */
#define VG_PAUSE_T        3.0f    /* pause entre deux vagues */
#define VG_SPAWN_T        0.35f   /* un ennemi toutes les 0,35 s */
#define VG_ALIVE_MAX      16      /* ennemis vivants a la fois (hors boss) : le reste attend */
#define VG_HP_FROM        66      /* les PV des ennemis montent a partir de cette vague... */
#define VG_HP_MAX         1.5f    /* ... jusqu'a x1,5 a la vague 100 (plafond, aussi en INFINI) */
#define VG_ELITE_MAX      0.6f    /* part d'elites a la vague 100 (elles changent de comportement, pas de degats) */
#define VG_INF_T          30.0f   /* INFINI : la vague suivante arrive au bout de 30 s (sauf boss et vagues de 10) */
#define VG_INF_CLEAR_T    2.0f    /* INFINI : ... ou 2 s apres avoir tout tue */
#define VG_RACE_LIMIT     75.0f   /* CHALLENGE course : temps limite (le pilote des tests met 41 s) */
#define VG_DEF_HOLD       40.0f   /* CHALLENGE defense : secondes a tenir dans la zone... */
#define VG_DEF_LIMIT      100.0f  /* ... avant ce temps */
#define VG_DEF_ALIVE      12      /* CHALLENGE defense : assaillants vivants a la fois */
#define VG_HARD_CREDITS   1.5f    /* carte difficile : credits x1,5 */
/* degats des ennemis (fixes : aucune vague ne les augmente) */
#define VG_ORB_DMG        12.0f   /* orbe (drone, sentinelle, bastion, barrages) : comme avant */
#define VG_RUSH_DMG       18.0f   /* kamikaze au contact */
#define VG_SWARM_DMG      6.0f    /* essaim au contact */
#define VG_BIG_ORB_DMG    30.0f   /* grosse orbe chargee (pyramide, hexaedre), plus le souffle */
#define VG_RAM_DMG        22.0f   /* charge de la pyramide / de l'hexaedre non paree */
#define VG_ROLL_DMG       20.0f   /* la boule qui roule sur toi */
#define VG_WAVE_DMG       18.0f   /* onde de choc au sol */
#define VG_LASER_DMG      14.0f   /* rayon de l'hexaedre */
/* SENTINELLE (ennemi vert) : elle tire un laser parryable ou une orbe tres rapide, avec les degats de l'orbe du type (VG_ORB_DMG) */
#define VG_SNIPER_LASER_P    0.5f    /* part des tirs au laser (le reste : orbe rapide) */
#define VG_SNIPER_FAST_SPEED 110.0f  /* vitesse de l'orbe rapide (34 m/s avant) ; au dela de ~150 elle traverserait le joueur entre deux pas */
#define VG_LASER_R           0.7f    /* le joueur est touche par le laser a moins de 0,7 m de la ligne */
/* BOMBE (kamikaze rapide qui s'arrete, tique puis explose), CARRE (mini tesseract : bouclier des autres), MORTIER (orbe lente a grosse zone) */
#define VG_BOMBE_SPEED    20.0f   /* vitesse de la bombe (le kamikaze : 12) */
#define VG_BOMBE_STOP     8.0f    /* distance a laquelle elle s'arrete pour tiquer */
#define VG_BOMBE_FUSE     2.0f    /* secondes de tic avant l'explosion */
#define VG_BOMBE_R        5.0f    /* rayon de l'explosion (m) */
#define VG_BOMBE_DMG      30.0f   /* degats de l'explosion (fixes) */
#define VG_CARRE_MULT     0.1f    /* degats subis par les ennemis tant qu'un carre vit */
#define VG_MORTIER_SPEED  7.0f    /* vitesse de l'orbe a pulsation */
#define VG_MORTIER_LIFE   4.0f    /* l'orbe detonne seule au bout de ce temps */
#define VG_MORTIER_R      8.0f    /* rayon de l'explosion de l'orbe (m) */
#define VG_MORTIER_DMG    30.0f   /* degats de l'explosion (fixes) */
#define VG_MORTIER_T      5.0f    /* delai entre deux tirs (3,5 s pour l'elite) */
/* boss */
#define VG_BOSS_PYRA_HP   2400.0f
#define VG_BOSS_TESS_HP   3600.0f
#define VG_BOSS_BOULE_HP  4200.0f
#define VG_BOSS_HEXA_HP   8000.0f
#define VG_BIG_ORB_RETURN 450.0f  /* grosse orbe paree qui revient sur le boss */
#define VG_RAM_PARRY_DMG  250.0f  /* charge paree : le boss encaisse et reste etourdi */
#define VG_STUN_T         2.2f    /* boss etourdi apres un parry */
#define VG_ARMOR          0.35f   /* pyramide (et hexaedre en phase PARRY) : armure hors etourdissement */
#define VG_STUN_MULT      1.5f    /* ... et degats subis etourdi */
#define VG_GROUND_MULT    0.15f   /* boule : coups portes depuis le sol */
#define VG_LINKS          4       /* tesseract : bouclier tant qu'il reste au moins 4 invocations */
/* credits (boutique de modificateurs) */
#define VG_PRICE_1        120     /* prix des 5 etapes d'un chemin */
#define VG_PRICE_2        240
#define VG_PRICE_3        400
#define VG_PRICE_4        600
#define VG_PRICE_5        850

/* Mode classe : formule et paliers documentes dans docs/CODE.md ("Mode classe") ; worker/src/ratings.js
 * (le serveur, qui fait autorite) utilise exactement les memes valeurs */
#define RANK_START     1000   /* cote de depart */
#define RANK_K         32.0f  /* facteur K apres le placement */
#define RANK_K_PLACE   64.0f  /* facteur K pendant le placement */
#define RANK_PLACEMENT 10     /* matchs de placement */
#define RANK_MIN       100
#define RANK_MAX       3000
#define RANK_TIERS     6      /* BRONZE, ARGENT, OR, PLATINE, DIAMANT, MAITRE */
#define RANK_ARGENT    900    /* seuils des paliers (chaque palier sauf MAITRE : divisions III, II, I) */
#define RANK_OR        1050
#define RANK_PLATINE   1200
#define RANK_DIAMANT   1350
#define RANK_MAITRE    1500
#define RANK_END_WAIT  0.5f   /* fin de match : attente des derniers frags (PK_SNAP) avant le rapport */
#define RANK_RETRY     2.5f   /* rapport en attente des autres joueurs : renvoye toutes les 2,5 s ... */
#define RANK_TRIES     24     /* ... pendant 60 s au plus */
#define RANK_MENU_RETRY   15.0 /* menu : cote et TOP 3 redemandes 15 s apres un echec (serveur qui se reveille) ... */
#define RANK_MENU_REFRESH 60.0 /* ... et toutes les 60 s sinon, tant que le menu est affiche */
