# Fiche 04 — Poings à impulsion « Impulsion »

> Source : [`../IDEES_ARMES_CARTES.md`](../IDEES_ARMES_CARTES.md) § 2.4 (validé), § 1.2 et § 1.5. Interfaces : [`PLAN.md`](PLAN.md) § 3.
> Identifiants : `MK_FIST` (4), `W_FIST` (10), événement `EV_MELEE` (réutilisé). Infrastructure (loadout, `P.meleeKind`, `P.ammo[]` et jauges de points, fonction à points `ammo_points(pts)`, `W_COUNT`, `NET_VER` 4) : voir `00_infra_loadout_munitions.md`, non redéfinie ici.
> Document de design : rien n'est implémenté, `main.c` n'est pas modifié.

---

## 1. Résumé validé

- **Rôle** : boxeur cinétique. Petits coups rapides, et les poings **servent aussi à se déplacer** (punch-jump).
- **Combo de 3 coups** : 0,22 s entre deux coups, 0,7 s de recharge après le 3e. Une pause trop longue ramène le combo au coup 1.
- **Portée** `1,9 + 0,06 × v_rel` m (`v_rel` = `rel_speed`).
- **Dégâts** : coups 1 et 2 = `14 + 1,5 × v_rel` ; coup 3 = `20 + 2,5 × v_rel` + **uppercut** (recul vertical 11).
- **+0,35 dash** par coup qui touche (≈ 1 dash par combo, comme l'épée).
- **Recharge du tir** (§ 1.5, validé le 27/10) : **25 points** de jauge par coup qui touche, **50 au plus par combo**. Comme toutes les mêlées : +100 par kill en mêlée, +50 par parry réussi. Pas de recharge passive.
- **Punch-jump** : coup dans le décor (`ray_world` ≤ 1,9 m) → +11 m/s le long de la normale de la surface. Frapper le sol en l'air = mini-saut, une fois par séjour en l'air (remis à zéro au sol et en wall-run).
- **Parry** : fenêtre de 0,06 s par coup. Combo sans aucune touche ni parry → recharge anti-spam de 0,6 s. Poing paré → combo cassé, 1,0 s (`MELEE_PARRIED_CD`).
- **Contre-jeu** : portée courte ; le recul d'une roquette casse le combo.

**Corrections apportées par cette fiche** (détails aux § 2.4, 7 et 11) :
- « Le plafond de 60 ne joue jamais » est **faux pour l'uppercut** au-delà de `v_rel` ≈ 26,3 m/s ((20 + 2,5·26,3) × 0,7 = 60). Les coups 1-2 n'y arrivent qu'à 44,5 m/s.
- Le punch-jump « +11 m/s » devient « la composante normale vaut **au moins** 11 m/s » (pas de cumul), avec un plafond horizontal. Sans ce garde-fou, un saut suivi d'un coup au sol donne 19,4 m/s vers le haut (7,8 m de hauteur).
- En LAN, la tolérance `MELEE_PARRY_GRACE` (0,15 s) rendrait un joueur en plein combo **imparable à 95 %**. Il faut une tolérance propre aux poings (§ 6).

---

## 2. Règles détaillées

### 2.1 Machine à états du combo

`P.fistStep` = nombre de coups déjà donnés dans le combo en cours (0 à 3). `P.fistT` = temps écoulé depuis le dernier coup. `P.swingLanded` (champ existant) indique si le combo **entier** a touché ou paré.

| État | Entrée | Sortie |
|---|---|---|
| `fistStep = 0` (repos) | appui (ou appui mémorisé) et `meleeCd ≤ 0` | coup 1 → `fistStep = 1`, `meleeCd = 0,22` |
| `fistStep = 1` | appui et `meleeCd ≤ 0` avant que `fistT > 0,45` | coup 2 → `fistStep = 2`, `meleeCd = 0,22` |
| `fistStep = 2` | appui et `meleeCd ≤ 0` avant que `fistT > 0,45` | coup 3 (uppercut) → `fistStep = 3`, `meleeCd = 0,70` |
| `fistStep = 1 ou 2` | `fistT > FIST_COMBO_WIN` (0,45 s) | **fin de combo** (délai dépassé) |
| `fistStep = 3` | fenêtre de parry du coup 3 terminée | **fin de combo** |
| tout état | poing paré, recul ≥ 8 m/s reçu, mort | **combo cassé** (§ 2.3) |

**Fin de combo** (`fist_combo_end`) : si `!P.swingLanded`, `P.meleeCd = fmaxf(P.meleeCd, 0) + FIST_WHIFF_CD`. Puis `fistStep = 0`, `swingLanded = 0`, `fistPts = 0`.
- 3 coups dans le vide : 0,22 + 0,22 + 0,70 + 0,60 = **1,74 s** pour un cycle (au lieu de 1,14 s).
- 1 coup dans le vide puis rien : le combo se ferme à 0,45 s, puis 0,6 s de blocage : 1 coup toutes les 1,05 s.
- Le délai de 0,45 s laisse 0,23 s pour enchaîner après la fin des 0,22 s : on ne peut pas « garder » l'uppercut en réserve.

**Tampon d'entrée** : `actP[A_MELEE]` n'existe que pendant le tick de l'appui. Avec une cadence de 0,22 s, un appui un peu en avance serait perdu. On mémorise l'appui pendant `FIST_BUFFER` = 0,12 s (`P.fistBuf`) et on le consomme dès que `meleeCd ≤ 0`.

### 2.2 Un coup

1. `try_parry()` (projectiles, comme l'épée) ; s'il pare quelque chose, `swingLanded = 1`.
2. **Une seule cible** : la plus proche dans le cône `FIST_CONE` (0,6, soit environ 53°) ou à moins de 0,8 m, parmi les ennemis (`E`) et les joueurs (`np_target`). L'épée balaie tout l'arc ; les poings frappent une seule cible.
3. Dégâts `fist_dmg(rel, up)` ; recul : coups 1-2 = `fh × 2,5 + (0, 1, 0)` (faible, pour garder la cible à portée : 2,5 × 0,22 = 0,55 m entre deux coups) ; coup 3 = `fh × 3 + (0, 11, 0)`. `fh` = direction de visée à l'horizontale.
4. **Touche** : +0,35 dash, +25 points de jauge tant que le combo n'a pas déjà rapporté 50 (§ 7.4), hitstop, son, `swingLanded = 1`.
5. **Rien touché** : tentative de punch-jump (§ 2.4). Une cible touchée est toujours prioritaire sur le décor (jamais les deux à la fois).
6. `P.parryWin = FIST_PARRY_WIN` (0,06 s), `P.meleeT = FIST_ANIM` (0,16 s, animation).

**Kamikaze** (`EN_RUSHER`) touché : il explose, comme avec l'épée (`damage_enemy(i, 9999)` + `parry_feedback`).
**Suspension en l'air** : un coup 1 ou 2 qui touche une cible **en l'air** pendant qu'on est en l'air fixe `P.vel.y = fmaxf(P.vel.y, FIST_AIR_HANG)` (2 m/s). Ainsi, le combo complet reste possible contre un drone (voir § 9 et question 8). Ce n'est pas exploitable pour la vitesse : il faut une cible, et il n'y a pas de gain horizontal.

### 2.3 Combo cassé

`fist_combo_break(cd)` : `fistStep = 0`, `swingLanded = 0`, `fistBuf = 0`, `meleeCd = fmaxf(meleeCd, cd)`. Cas :
- **Poing paré** (`EV_MELEE_PARRY`, je suis l'attaquant) : `cd = MELEE_PARRIED_CD` (1,0 s), recul 11 existant.
- **Choc** (les deux en fenêtre de parry) : `cd = FIST_END_CD` (0,7 s).
- **Gros recul reçu** (`EV_HIT` sur moi avec `|knock| ≥ FIST_STAGGER_KNOCK` = 8 m/s) : `cd = FIST_STAGGER_CD` (0,3 s). Sont concernés : la roquette à partir de f ≥ 0,38 (13·f + 3 ≥ 8), l'épée (≥ 8 + 3 vers le haut), l'impact du grappin (14 + 4) et l'uppercut adverse (11). Ne cassent pas le combo : le rail (4), le ricochet (6), les poings 1-2 (2,7). Mes propres roquettes (rocket jump) ne passent pas par `EV_HIT` : elles ne cassent rien.
- **Mort / respawn** : `reset_game` (solo) et `EV_RESPAWN` remettent tous les champs `fist*` à zéro.

### 2.4 Punch-jump et garde-fous anti-vitesse infinie

Fonction `fist_pj(eye, f)`, appelée seulement si le coup n'a touché aucune cible :

| Règle | Valeur | Pourquoi |
|---|---|---|
| Rayon `ray_world(eye, f, …)` depuis l'œil | 1,9 m (`FIST_PJ_RANGE`) | validé |
| Charge : `P.fistPJ` = 1, consommée par l'impulsion | rendue quand `P.grounded` (chaque tick au sol) et au **début** d'un wall-run | « 1 fois par séjour en l'air » |
| Sol (`n.y > 0,7`) refusé si `P.grounded` ou `P.coyote < 0,1` | — | pas de super-saut depuis le sol |
| **Composante normale portée à au moins 11**, sans addition : `vn = dot(v, n); if (vn < 11) v += n × (11 − vn)` | `FIST_PJ_PUSH` 11 | aucun cumul sur un même axe |
| **Plafond horizontal** : après l'impulsion, `hlen(v) ≤ max(hlen avant, 20)` (on rééchelonne x et z) | `FIST_PJ_HCAP` 20 m/s | au-dessus de 20 m/s, le punch-jump **réoriente** sans accélérer |
| Partage la recharge du wall-jump : exige `P.wallJumpCd ≤ 0`, puis `P.wallJumpCd = 0,25` | `FIST_PJ_CD` | interdit wall-jump et punch-jump sur le même contact |
| Pas de dash, pas de points de jauge, pas d'étape de combo en plus | — | le décor n'est pas une cible |
| Compte comme coup « utile » (pas de pénalité anti-spam) | — | question 3 |

**Combinaisons étudiées** (vitesses en m/s) :

| Enchaînement | Sans garde-fou | Avec garde-fous |
|---|---|---|
| Saut (8,4) puis coup au sol immédiat | vy = 19,4 → 7,8 m de haut | refusé (`coyote < 0,1`) ; et de toute façon max(8,4 ; 11) = 11 |
| Chute à −20 puis coup au sol | −9 (pas de rebond) | +11 : rebond de 2,52 m (le saut normal fait 1,47 m) |
| Double saut (7,98) puis coup au sol | 19 | 11 (+0,6 m de hauteur) ; le double saut reste indépendant (`airJumps`) |
| Coup au mur puis wall-jump (même contact) | 11 + 8 = 19 en normal | wall-jump bloqué 0,25 s ; au bout de 0,25 s on est à 2,75 m du mur, donc `wallTouch = 0` |
| Wall-jump puis coup au mur | 8 + 11 = 19 | max(8 ; 11) = 11, et `wallJumpCd` le bloque déjà |
| Va-et-vient entre deux murs parallèles | la normale s'annule à chaque collision | idem : aucun gain tangentiel |
| Suite de murs non parallèles (chaque wall-run rend la charge) | +11 dans une nouvelle direction à chaque mur, donc croissance sans limite | plafond horizontal : jamais au-delà de max(v avant, 20) |
| Course à 30 le long d'un mur, coup au mur | 31,95 | 30 (réorienté de 20°) : « redresser une trajectoire » |
| Punch-jump puis dash | dash = max(hs + 3 ; 18) | au plus 23 en partant de presque 0 ; le dash reste limité par les charges (le décor ne rend pas de dash) |
| Bhop + coup au sol juste avant l'atterrissage | rebond de 11 au lieu de 8,4 ; pas de contact au sol, donc pas de recharge | 1 charge, rendue seulement au sol : impossible d'enchaîner sans atterrir |
| Plafond (`n.y < −0,7`) | vy ≤ −11 (piqué) | autorisé : plonger sur une cible |

Bilan : le punch-jump apporte au plus 11 m/s par séjour en l'air et **ne pousse jamais la vitesse horizontale au-delà de 20 m/s**. Il n'y a pas de boucle de vitesse infinie. Au-delà de 20 m/s, la vitesse continue de venir de l'air-strafe, du dash, du grappin et des rampes, comme aujourd'hui.

**Fenêtre du mini-saut** : depuis l'œil, le sol est à moins de 1,9 m si les pieds sont à ≤ 0,30 m (debout) ou ≤ 0,95 m (accroupi : `EYE_CROUCH`). À −10 m/s de chute, cela laisse 30 ms debout et 95 ms accroupi. C'est très serré debout : voir la question 2 (portée de 2,6 m pour le sol).

### 2.5 Cas limites

- **Hitstop** : `game_tick` sort avant `fist_tick`, donc tous les minuteurs du combo sont gelés en même temps. Il n'y a pas de dérive.
- **Changement d'arme à distance** : sans effet sur le combo. On peut **tirer pendant le combo** (pour « uppercut puis rail ») ; l'arme est seulement abaissée pendant l'animation (§ 8).
- **Grappin accroché** : les poings et le punch-jump restent permis (la corde ne s'allonge jamais : pas d'exploit).
- **Fin de match** : l'hôte ignore les `EV_HIT` (`matchOver`) ; le combo local continue sans effet.
- **Mort pendant le combo** : `fist_tick` ne frappe pas si `P.dead`, et le respawn remet le combo à zéro.

---

## 3. Constantes

```c
/* Poings a impulsion (MK_FIST) : combo de 3 coups, punch-jump sur le decor */
#define FIST_GAP           0.22f   /* entre deux coups du combo */
#define FIST_END_CD        0.70f   /* recharge apres le 3e coup */
#define FIST_COMBO_WIN     0.45f   /* au-dela, sans nouveau coup, le combo revient au coup 1 */
#define FIST_WHIFF_CD      0.60f   /* combo sans touche ni parry : recharge en plus */
#define FIST_BUFFER        0.12f   /* un appui un peu en avance est memorise */
#define FIST_ANIM          0.16f   /* duree de l'animation d'un coup */
#define FIST_RANGE         1.9f
#define FIST_RANGE_PER_MS  0.06f
#define FIST_CONE          0.6f    /* cosinus du demi-angle (~53 degres), une seule cible */
#define FIST_DMG           14.0f   /* coups 1 et 2 */
#define FIST_DMG_PER_MS    1.5f
#define FIST_UP_DMG        20.0f   /* coup 3 : uppercut */
#define FIST_UP_DMG_PER_MS 2.5f
#define FIST_KNOCK         2.5f    /* recul faible : la cible reste a portee */
#define FIST_UP_KNOCK      11.0f   /* uppercut : recul vertical */
#define FIST_AIR_HANG      2.0f    /* touche en l'air sur une cible en l'air : on reste suspendu */
#define FIST_DASH          0.35f   /* dash rendu par coup qui touche */
#define FIST_PTS           25      /* points de jauge par coup qui touche */
#define FIST_PTS_MAX       50      /* points de jauge par combo au plus (kill +100 et parry +50 : regles communes) */
#define FIST_PARRY_WIN     0.06f   /* fenetre de parry de chaque coup */
#define FIST_PARRY_GRACE   0.05f   /* tolerance de l'hote pour une fenetre ouverte par un poing */
#define FIST_STAGGER_KNOCK 8.0f    /* un recul recu au moins aussi fort casse le combo */
#define FIST_STAGGER_CD    0.3f
#define FIST_PJ_RANGE      1.9f    /* coup dans le decor : distance maximale */
#define FIST_PJ_FLOOR_RANGE 1.9f   /* idem pour le sol (2.6 propose, voir fiche) */
#define FIST_PJ_PUSH       11.0f   /* composante normale minimale apres l'impulsion */
#define FIST_PJ_HCAP       20.0f   /* le punch-jump n'accelere plus au-dela (il reoriente) */
#define FIST_PJ_CD         0.25f   /* partage avec P.wallJumpCd */
```

---

## 4. Données

| Structure | Champ | Type | Rôle |
|---|---|---|---|
| `Player` | `fistStep` | `int` | coups donnés dans le combo (0 à 3) |
| `Player` | `fistT` | `float` | temps depuis le dernier coup |
| `Player` | `fistBuf` | `float` | appui mémorisé (s restantes) |
| `Player` | `fistPJ` | `int` | 1 = punch-jump disponible |
| `Player` | `fistPts` | `int` | points de jauge déjà donnés par ce combo (0 à 50) |
| `Player` | `swingLanded` | existant | « le combo a touché ou paré » (au lieu de « le coup ») |
| `NetPState` | `pad` bits 0-3 | infra | `MK_FIST` ; aucun bit `ST_*` utilisé |
| `NetPState` | `melee` | existant | **pour `MK_FIST`** : `(étape << 6) | avancement` (étape 0 à 2, avancement 1 à 63) |
| `NetEvent` (`EV_MELEE`) | `sub` | existant | 0 = épée ; poings : étape 1 à 3, bit `0x10` = punch-jump |
| hôte | `fistCombos`, `fistPJs` | `int` | statistiques du journal |

`reset_game` remet tout à zéro par son `memset`, puis `P.fistPJ = 1`. Il faut faire de même à `EV_RESPAWN`.

---

## 5. Algorithme (pseudo-code, fonction par fonction)

```c
static float fist_range(float rel) { return FIST_RANGE + rel * FIST_RANGE_PER_MS; }
static float fist_dmg(float rel, int up) {
    return up ? FIST_UP_DMG + rel * FIST_UP_DMG_PER_MS : FIST_DMG + rel * FIST_DMG_PER_MS;
}

/* game_tick : remplace le bloc epee si P.meleeKind == MK_FIST */
static void fist_tick(float dt, int press) {
    P.fistBuf -= dt; P.fistT += dt;
    if (press) P.fistBuf = FIST_BUFFER;
    if ((P.fistStep == 1 || P.fistStep == 2) && P.fistT > FIST_COMBO_WIN) fist_combo_end();
    if (!P.dead && P.fistBuf > 0 && P.meleeCd <= 0) { P.fistBuf = 0; fist_punch(); }
    if (P.parryWin > 0) {
        if (!P.dead) try_parry();                 /* met swingLanded a 1 s'il pare */
        P.parryWin -= dt;
        if (P.parryWin <= 0 && P.fistStep == 3) fist_combo_end();
    }
}
/* dans game_tick : P.meleeCd -= dt; P.meleeT -= dt; puis
   if (P.meleeKind == MK_FIST) fist_tick(dt, meleePress); else { bloc epee actuel } */

static void fist_combo_end(void) {
    if (!P.swingLanded) P.meleeCd = fmaxf(P.meleeCd, 0) + FIST_WHIFF_CD;
    P.fistStep = 0; P.swingLanded = 0; P.fistPts = 0;
}
static void fist_combo_break(float cd) {
    P.fistStep = 0; P.swingLanded = 0; P.fistPts = 0; P.fistBuf = 0;
    P.meleeCd = fmaxf(P.meleeCd, cd); P.parryWin = 0;
}

static void fist_punch(void) {
    int step = P.fistStep, up = step == 2;
    if (step == 0) { P.swingLanded = 0; P.fistPts = 0; }
    float sp = vlen(P.vel);
    V3 eye = eye_pos(), f = view_dir(), fh = vnorm(v3(f.x, 0, f.z));
    int parried = try_parry();
    /* cible la plus proche (ennemi ou joueur) */
    int bestE = -1, bestP = -1; float bestD = 1e9f, rel = 0;
    for (int i = 0; i < MAXE; i++) {           /* meme test que melee() avec fist_range/FIST_CONE */
        ... reach = dist - enemy_radius ; if (reach > fist_range(rel_i)) continue;
        ... if (reach > 0.8f && dot < FIST_CONE) continue;  if (dist < bestD) { bestE = i; ... }
    }
    for (int i = 0; i < MAXP; i++) { ... np_target(i), reach = dist - 0.5f ... bestP = i ... }
    V3 knock = up ? vadd(vmul(fh, 3), v3(0, FIST_UP_KNOCK, 0)) : vadd(vmul(fh, FIST_KNOCK), v3(0, 1, 0));
    int hit = 0, tgtAir = 0;
    if (bestP >= 0) {                              /* le plus proche gagne, joueur ou ennemi */
        net_hit(bestP, fist_dmg(rel, up) * PVP_SCALE, knock, W_FIST);
        tgtAir = fabsf(NP[bestP].st.vel[1]) > 0.5f;   /* pas de drapeau "au sol" : on lit vel.y */
        hit = 1;
    } else if (bestE >= 0) {
        Enemy *e = &E[bestE];
        if (e->type == EN_RUSHER) { damage_enemy(bestE, 9999, vmul(f, 10)); parry_feedback(e->pos); parried++; }
        else {
            damage_enemy(bestE, fist_dmg(rel, up), knock);
            if (!e->alive) { P.hp = fminf(100, P.hp + 15); ammo_points(100); }   /* kill en melee (solo), hors plafond */
        }
        tgtAir = e->type == EN_DRONE; hit = 1;
    }
    if (hit) {
        P.dashCharge = fminf(2, P.dashCharge + FIST_DASH);
        int pts = FIST_PTS_MAX - P.fistPts < FIST_PTS ? FIST_PTS_MAX - P.fistPts : FIST_PTS;
        if (pts > 0) { ammo_points(pts); P.fistPts += pts; }   /* fonction a points de l'infra */
        /* kill : +100 via EV_KILL (LAN) ou damage_enemy -> !alive (solo) ; parry : +50 via parry_feedback */
        if (!up && tgtAir && !P.grounded) P.vel.y = fmaxf(P.vel.y, FIST_AIR_HANG);
        hitMarker = 0.18f; shake += up ? 0.2f : 0.08f;
        if (!parried) hitstop = fmaxf(hitstop, up ? 0.06f + sp * 0.0015f : 0.025f);
        if (up && sp > 15) impact_frame(vadd(eye, vmul(f, 1.5f)), 0.05f);
        play(up ? S_FIST_UPPER : S_FIST_HIT, 0.9f, frange(0.95f, 1.05f));
    }
    int pj = 0; float pjDist = 0;
    if (!hit && !parried) pj = fist_pj(eye, f, &pjDist);
    P.swingLanded |= hit || parried || pj;
    P.fistStep = step + 1; P.fistT = 0;
    P.meleeCd = up ? FIST_END_CD : FIST_GAP;
    P.meleeT = FIST_ANIM; P.parryWin = FIST_PARRY_WIN;
    play(S_DASH, 0.3f, 2.2f);                     /* souffle du coup */
    if (dmMode) net_fx_fist(eye, f, pj ? pjDist : sp, (step + 1) | (pj ? 0x10 : 0));
}

static int fist_pj(V3 eye, V3 f, float *dist) {
    if (!P.fistPJ || P.wallJumpCd > 0) return 0;
    V3 n; float t = ray_world(eye, f, fmaxf(FIST_PJ_RANGE, FIST_PJ_FLOOR_RANGE), &n);
    int floor = n.y > 0.7f;
    if (t >= (floor ? FIST_PJ_FLOOR_RANGE : FIST_PJ_RANGE)) return 0;
    if (floor && (P.grounded || P.coyote < 0.1f)) return 0;
    float hs0 = hlen(P.vel), vn = vdot(P.vel, n);
    if (vn < FIST_PJ_PUSH) P.vel = vadd(P.vel, vmul(n, FIST_PJ_PUSH - vn));
    float hs = hlen(P.vel), cap = fmaxf(hs0, FIST_PJ_HCAP);
    if (hs > cap) { P.vel.x *= cap / hs; P.vel.z *= cap / hs; }
    if (n.y > -0.7f) { P.grounded = 0; P.coyote = 1; }
    P.fistPJ = 0; P.wallJumpCd = FIST_PJ_CD;
    *dist = t;
    fist_pj_fx(vadd(eye, vmul(f, t)), n);         /* anneau, son S_FIST_PJ, fovKick += 5 */
    return 1;
}
```

**`player_tick`** : après `if (P.grounded) P.wallTime = 0;`, ajouter `if (P.grounded) P.fistPJ = 1;`. Au début d'un wall-run (`if (!P.wallrun) { ...; P.airJumps = 1; }`), ajouter `P.fistPJ = 1;`. Le saut, le double saut et le dash ne changent pas : les garde-fous sont tous dans `fist_pj`.

**`net_fx_fist`** : comme `net_fx_melee`, avec `e.sub = sub` (`f[6]` = vitesse, ou distance de l'impact si bit `0x10`).

**`pack_state`** : `if (P.meleeKind == MK_FIST && P.meleeT > 0) s->melee = (unsigned char)(((P.fistStep - 1) << 6) | (1 + (int)(clampf(1 - P.meleeT / FIST_ANIM, 0, 1) * 62)));`

**`apply_event`** :
- `EV_MELEE` : si `ev->sub & 0x10`, anneau au point `a + b × f[6]` ; sinon, si `ev->sub`, petite traînée cyan sur 1,2 m et `S_DASH` aigu ; sinon, effet d'épée actuel.
- `EV_HIT`, cible = moi : `if (P.meleeKind == MK_FIST && P.fistStep && vlen(knock) >= FIST_STAGGER_KNOCK) fist_combo_break(FIST_STAGGER_CD);`
- `EV_MELEE_PARRY`, attaquant = moi : `if (P.meleeKind == MK_FIST) fist_combo_break(clash ? FIST_END_CD : MELEE_PARRIED_CD);` (le recul de 11 m/s et le hitstop existants restent). Texte : « POING PARÉ PAR %s » / « CHOC ! ».

---

## 6. Réseau

- **Qui calcule** : le tireur (`fist_punch`) détecte la touche sur `NP[i].disp` et envoie **un `net_hit` par coup**, sur une seule cible. Il n'y a rien à agréger.
- **Événements** : `EV_HIT` (`sub = W_FIST`), `EV_MELEE` (`sub` = étape et punch-jump), `EV_KILL` (`sub = W_FIST`, texte « POINGS » dans `weaponNames`, tableau agrandi par l'infra). Pas de nouvel `EV_*`.
- **Mouvement du punch-jump** : simulé localement dans `player_tick` et transmis par `NetPState.vel`. L'hôte n'a rien à arbitrer.
- **`host_handle`** : `W_FIST` est ajouté aux coups parables (PLAN § 3). La tolérance dépend de l'arme de mêlée de la **victime** :

```c
static float parry_grace(int id) { return (NP[id].st.pad & 15) == MK_FIST ? FIST_PARRY_GRACE : MELEE_PARRY_GRACE; }
/* ... */
if (parable(ev->sub) && nowT - NP[v].lastParryT < parry_grace(v)) {
    int clash = (ev->sub == W_MELEE || ev->sub == W_FIST) && ev->from < MAXP
                && nowT - NP[ev->from].lastParryT < parry_grace(ev->from);
    ...  /* EV_MELEE_PARRY existant */
}
```

**Pourquoi** : `lastParryT` est mis à jour à chaque fenêtre ouverte. Un joueur qui enchaîne les coups en ouvre une toutes les 0,22 s. Avec 0,15 s de tolérance, il est considéré « en parry » pendant (0,06 + 0,15) / 0,22 = **95 %** du combo, et il serait presque imparable à l'épée. Avec 0,05 s, on tombe à (0,06 + 0,05) / 0,22 = **50 %**. L'épée qui frappe en continu est à (0,12 + 0,15) / 0,45 = 60 %. Hors réseau, la proportion est la même pour les deux armes : 0,06 / 0,22 ≈ 0,12 / 0,45 ≈ 27 %. En LAN (moins de 10 ms, états à 60/s), 0,05 s couvre 3 paquets.

- **Tolérance de latence** : la touche est validée chez le tireur sur `disp` (extrapolée au plus 0,1 s). La portée courte (1,9 m) rend l'écart d'extrapolation plus visible qu'avec l'épée. Le cône d'environ 53° et la zone de contact de 0,8 m compensent.
- `NET_VER` : passe déjà à 4 avec l'infra. La nouvelle signification de `melee` et de `EV_MELEE.sub` doit être livrée dans la même version.

---

## 7. Interactions

1. **Parry de projectiles** : `try_parry` est inchangé (rayon `(MELEE_RANGE + 1,5)` = 3,9 m, renvois, ping-pong). Si la fiche Fléau le paramètre, les poings passent les valeurs par défaut. Chaque coup en est un : 3 fenêtres de 0,06 s par combo.
2. **Pièces** : aucune interaction directe (pas de frappe de pièce). L'uppercut met la cible en vol : pièce + rail derrière.
3. **Grappin** : en traction vers un joueur (jusqu'à 46 m/s), le coup 1 part à `v_rel` ≈ 40, donc avec 4,3 m de portée et 74 de dégâts PvE (51,8 PvP), avant l'impact du grappin. Le combo s'enchaîne après `grapple_bounce`.
4. **Recharge par la mêlée** (validé le 27/10, § 1.5 du document d'idées) : **25 points par coup qui touche, 50 au plus par combo** (`FIST_PTS`, `FIST_PTS_MAX`). Les points passent par la fonction à points de l'infra (`ammo_points(pts)`) : ils vont à l'arme à distance en main, puis à l'autre si elle est pleine. Il n'y a pas de recharge passive.
   - Combo dont les coups 1 et 2 touchent : 25 + 25 ; l'uppercut ne rapporte plus rien. Un seul coup sur trois qui touche : 25.
   - **Kill en mêlée +100** (`EV_KILL` avec `sub = W_FIST`, ou mort du drone dans `fist_punch` en solo) et **parry réussi +50** (`parry_feedback`, kamikaze compris) : règles communes, hors plafond du combo.
   - **Coup paré par l'hôte** : l'infra annule les derniers points de touche (moins de 0,5 s). Le combo étant cassé, `fistPts` repart de 0 de toute façon.

| Arme (points par coup) | Points par cycle | Cycle | Cadence maximale | Kill rapide (touches + kill) |
|---|---|---|---|---|
| **Poings** (25, au plus 50 par combo) | 50 | 1,14 s | **44 pts/s** | 1 combo à `v_rel` ≥ 17,3 : 50 + 100 = **150** |
| Épée (50) | 50 | 0,45 s | 111 pts/s | 2 coups : 50 + 50 + 100 = 200 |

En pratique : 1 combo = **1 roquette** (jauge de 50), 5 combos = 1 tir de rail (250), 2 combos = 1 cartouche d'Éventail (100). Les poings rechargent volontairement moins vite que l'épée, puisqu'ils touchent plus facilement (3 essais par combo, cône de 53°). Leur part vient surtout des kills (+100) et des parries (+50).

5. **Autres mêlées** : la Lance couchée touche sans parry. Un poing qui la pare (0,06 s) suit les règles de la Lance. L'onde de l'Enclume (recul vertical de 12), la traction du Fléau (12 m/s) et le coup de bouclier (14) dépassent tous 8 m/s : ils cassent le combo.
6. **Roquettes adverses** : cassent le combo à partir de f ≈ 0,38 (§ 2.3).

---

## 8. Rendu et son

**`draw_viewmodel`** (boîtes `box_faces`, repère actuel après `glScalef(0.5)`) :
- Le bloc épée (`if (P.meleeT > 0)`) n'est dessiné que si `P.meleeKind == MK_SWORD`.
- Si `MK_FIST` et `P.meleeT > 0 || P.fistStep > 0` : l'arme à distance est abaissée de `0,12 × garde` (`garde` monte à 1 en 0,05 s et redescend en 0,15 s après la fin du combo, donc le tir reste possible).
- **Gant** = 3 boîtes : avant-bras `(-0.05,-0.05,0.0)–(0.05,0.05,0.30)` gris foncé ; poing `(-0.07,-0.07,-0.12)–(0.07,0.07,0.0)` gris acier ; **bobine émissive** (sans éclairage) `(-0.075,-0.02,-0.08)–(0.075,0.02,-0.04)`, dont la couleur va du cyan `(0.3,0.9,1)` au blanc chaud quand la vitesse monte (`heat = vlen(vel)/30`), comme le fil de l'épée.
- Positions de repos : gauche `(-0.55, -0.05, 0.10)`, droit `(-0.10, -0.12, 0.15)`. Avancement `k = 1 - meleeT / FIST_ANIM`, avec une courbe `e = sinf(k × PI)` (aller-retour).
  - Coup 1 (jab gauche) : `z -= 0.55 × e`, `x += 0.15 × e`.
  - Coup 2 (direct droit) : `z -= 0.6 × e`, `x -= 0.2 × e`.
  - Coup 3 (uppercut gauche) : `y += -0.15 + 0.45 × e`, `z -= 0.35 × e`, `glRotatef(-50 × e, 1, 0, 0)`.
  - Punch-jump : le poing frappe puis `P.recoil += 0.6` (secousse de l'arme).
- **Particules** : touche = `burst` cyan-blanc de 10 ; uppercut = colonne de 12 particules vers le haut sur la cible ; punch-jump = anneau de 16 `add_part` dans le plan de la surface (base `n × (0,1,0)` ou `n × (1,0,0)`) + 6 poussières, `shake += 0.12`, `fovKick += 5`.

**`draw_remote_players`** : si `(st.pad & 15) == MK_FIST`, deux petits gants permanents `(±0.38, h-0.75, -0.25)` de 0,14 m de côté, bobine de la couleur du joueur. Si `st.melee` : étape = `melee >> 6`, `k = (melee & 63) / 63`, le gant de l'étape (gauche, droit, gauche) avance de `0.7 × sinf(k × PI)` (uppercut : il monte de 0,5).

**`draw_hud`** : le panneau ÉPÉE devient POINGS : 3 pastilles (coups du combo, la 3e en or), barre de recharge `1 - meleeCd / FIST_END_CD`, et `melee_preview` qui affiche `fist_dmg(rel, P.fistStep == 2)` et `fist_range(rel)`. **Aide au punch-jump** : un petit anneau cyan autour du viseur quand `P.fistPJ` est disponible et que `ray_world(eye, f, FIST_PJ_RANGE)` touche (un rayon par image, bon marché).

**Sons** (`make_sounds`, avant `S_COUNT`) :
- `S_FIST_HIT` (0,12 s) : bruit passe-bas (`lp += 0.25 × (noise - lp)`), enveloppe `exp(-t × 45)`, + sinus 180→120 Hz `exp(-t × 25)`. Plus sec et plus aigu que `S_PUNCH`. La hauteur change de ±5 % à chaque coup.
- `S_FIST_UPPER` (0,3 s) : sinus montant 120→420 Hz `exp(-t × 8)` + claquement de bruit `exp(-t × 50)`.
- `S_FIST_PJ` (0,35 s) : « whoomp » : sinus 70→160 Hz `exp(-t × 9)` × 0,9 + souffle filtré montant (comme `S_DASH`, coupure de 300 à 2 500 Hz).

---

## 9. Bots

Dans `bot_think`, si `P.meleeKind == MK_FIST` :
- **Combo** : si la cible est à moins de `fist_range(rel_speed(np_vel(best))) + 0.3` et que `P.meleeCd ≤ 0`, viser (`P.yaw = aimYaw`, `P.pitch = aimPitch`) et `botPress[A_MELEE] = 1`. On répète à chaque tick tant que la cible est à portée : le tampon de 0,12 s enchaîne les 3 coups.
- **Après l'uppercut** (`fistStep == 3` et la cible monte) : tir au rail immédiat (`botHeld[A_FIRE]`), puisque la cible suit une trajectoire balistique.
- **Punch-jump** : en l'air, en chute (`vel.y < -3`), si la cible est plus haute de plus de 2 m et que les pieds sont à moins de 0,9 m du sol (`ray_world` vers le bas) : `botHeld[A_CROUCH] = 1`, `P.pitch = -1.5`, `botPress[A_MELEE] = 1`, puis `netlog("punch-jump")`.
- **Détection des coups adverses** : le test `NP[i].st.melee >= 150` (coup d'épée) devient `swing_start(i)`. Pour un adversaire `MK_FIST`, c'est `(melee & 63) < 20` au premier tick où l'on voit l'étape. Le bot tente un parry une fois sur trois (la fenêtre est plus courte).
- **Solo (drones)** : grappin sur le drone (existant), puis combo dès qu'il est à portée. `FIST_AIR_HANG` le maintient en l'air pour les coups 1-2.

---

## 10. Tests (`run_tests`)

Les scénarios utilisent le couloir x = −20. On appelle `fist_punch`, `fist_tick` et `player_tick` directement (comme `melee`).

1. **Cadence** : `botPress[A_MELEE]` à chaque tick pendant 1,5 s, solo, drone immobile à 1,5 m (`E[0]`, `vel` nulle, `enemy_tick` court-circuité en forçant `pos` à chaque tick). Instants des coups attendus : 0 ; 0,22 ; 0,44 ; 1,14 (± 1 tick + hitstop). PV du drone après 3 coups à `v_rel` = 0 : 100 − 48 = **52**.
2. **Retour au coup 1** : coup, attendre 0,5 s, coup → `fistStep == 1`, dégâts 14 (pas 20).
3. **Anti-spam** : 3 coups dans le vide → `meleeCd` ≈ 0,70 + 0,60 − 0,06 = **1,24** juste après la fin de la fenêtre du coup 3.
4. **Mini-saut** : `P.pos.y = 0.25`, en l'air, `coyote = 0.5`, `vel.y = -12`, `pitch = -PI/2` → `vel.y == 11`. Deuxième coup dans le même séjour → `vel.y` inchangée. Après l'atterrissage → de nouveau disponible.
5. **Super-saut** : au sol, saut puis coup vers le sol au tick suivant → refusé (`coyote < 0,1`), `vel.y ≈ 8,4`. Au sol : coup vers le sol → aucun effet.
6. **Mur** : boîte de test ajoutée face au joueur (`addbox` puis `nb--` à la fin), `vel = (0, 0, -5)` vers le mur → composante normale = 11 ; wall-jump refusé pendant 0,25 s (`wallJumpCd`).
7. **Plafond horizontal** : `vel` tangentielle de 25 m/s, coup au mur → `hlen == 25` (± 0,01), direction tournée. Boucle de 20 impulsions synthétiques sur des normales ±X, ±Z (en remettant `fistPJ = 1` et `wallJumpCd = 0`), en partant de 5 m/s → `hlen` max ≤ **20**.
8. **LAN simulé** : touche → `NP[1].hp` = 100 − 9,8 (coup 1 à `v_rel` = 0) ; uppercut → recul y = 11 dans l'événement ; combo à `v_rel` = 20 : PV 100 − 30,8 − 30,8 − 49 = **mort** (110,6).
9. **Parry hôte** : victime `MK_FIST`, `parryWin` actif → `host_tick`, puis `EV_HIT` (`W_MELEE`) à +0,04 s : paré ; à +0,08 s : non paré (tolérance de 0,05). Attaquant `MK_FIST` paré → `fistStep == 0`, `meleeCd ≥ 1,0`.
10. **Recul** : `EV_HIT` sur moi avec `knock = (10, 0, 0)` pendant le combo → `fistStep == 0` ; avec `knock = (4, 0, 0)` (rail) → combo intact.
11. **Points de jauge** : roquettes en main à 0 munition et jauge vide, combo de 3 touches sur un drone qui survit → la jauge a reçu **50** points (25 + 25 + 0) et les roquettes passent à **1** munition avec une jauge à 0. Un 2e combo dont seul l'uppercut touche → +25. Combo qui tue le drone au coup 3 → 50 + 100 = 150 (avec une jauge de 50 : +3 roquettes). Parry d'une orbe pendant un coup → +50, hors plafond. Contrôle « pas de recharge passive » : 10 s sans rien faire → jauge inchangée.

---

## 11. Équilibrage

| `v_rel` (m/s) | Portée | Coup 1-2 PvE / PvP | Coup 3 PvE / PvP | Combo PvP | Touches pour tuer |
|---|---|---|---|---|---|
| 0 | 1,9 m | 14 / 9,8 | 20 / 14 | 33,6 | 3 combos |
| 5 | 2,2 m | 21,5 / 15,1 | 32,5 / 22,8 | 52,9 | 2 combos |
| 10 | 2,5 m | 29 / 20,3 | 45 / 31,5 | 72,1 | combo + 1 coup |
| 15 | 2,8 m | 36,5 / 25,6 | 57,5 / 40,3 | 91,4 | combo + 1 coup |
| **17,3** | 2,9 m | 40 / 28 | 63 / 44 | **100** | **1 combo (0,44 s)** |
| 20 | 3,1 m | 44 / 30,8 | 70 / 49 | 110,6 | 1 combo |
| 30 | 3,7 m | 59 / 41,3 | 95 / **60** (plafond) | 142,6 | 1 combo |

- Un combo complet tue à pleine vie dès `v_rel` ≈ 17,3 m/s (33,6 + 3,85·v ≥ 100). L'épée tue en 2 coups (0,45 s) dès 12,7 m/s. La puissance est comparable, mais les poings ont 3 occasions d'être parés et une portée inférieure de 0,5 à 2,3 m.
- Plafond de 60 : jamais pour les coups 1-2 avant 44,5 m/s ; uppercut à partir de 26,3 m/s.
- PvE : un drone (100 PV) meurt en un combo dès `v_rel` ≈ 9,5 m/s (48 + 5,5·v).
- **À mesurer dans le journal du bot** : `hitCount[W_FIST]`, coups qui touchent par combo (moyenne), combos complets / commencés, `meleeParries` séparés par arme, punch-jumps par minute, `botMaxSpeed` avec et sans poings (il ne doit pas augmenter de plus de 10 %), points de jauge par minute à comparer avec l'épée, répartis entre touches (plafonnées à 50 par combo), kills (+100) et parries (+50), plus le temps passé à sec ; temps pour tuer (`ttkSum/ttkN`) au poing.

---

## 12. Étapes d'implémentation

| # | Étape | Fonctions | Estimation |
|---|---|---|---|
| 0 | Prérequis : infra (`meleeKind`, jauges et `ammo_points`, `W_FIST`, `W_COUNT`, `NET_VER` 4) | — | (fiche 00) |
| 1 | Constantes, champs `fist*`, `fist_range`, `fist_dmg` | en-tête, `Player`, `reset_game` | 0,5 h |
| 2 | Combo : `fist_tick`, `fist_punch`, `fist_combo_end`, `fist_combo_break`, aiguillage dans `game_tick` | Combat, `game_tick` | 1,5 h |
| 3 | Punch-jump : `fist_pj` + remises à zéro dans `player_tick` | Combat, Joueur | 1 h |
| 4 | Hôte et événements : `parry_grace`, `W_FIST` parable, choc, `EV_MELEE_PARRY` / `EV_HIT` côté poings, `net_fx_fist`, `pack_state` | Réseau | 1 h |
| 5 | Viewmodel, joueurs distants, HUD (pastilles, anneau d'aide) | Rendu, Interface | 1,5 h |
| 6 | Sons `S_FIST_*` | Audio | 0,5 h |
| 7 | Bot | `bot_think` | 1 h |
| 8 | Tests 1 à 11, puis réglage par le journal | `run_tests` | 1,5 h |

Total : environ **8,5 h** (coût S confirmé, une fois l'infra en place).

---

## 13. Questions ouvertes

> Tranché le 27/10 : recharge par la mêlée = 25 points par coup, 50 au plus par combo (§ 7.4).

1. **Punch-jump « au moins 11 » au lieu de « +11 »** (pas de cumul sur la normale), avec un **plafond horizontal de 20 m/s** : d'accord ? Le mini-saut fait alors 2,52 m, plus que le saut normal (1,47 m). Faut-il le garder à 11 ou le ramener à 8,4 pour le sol seulement ?
2. **Portée du coup vers le sol** : à 1,9 m depuis l'œil, la fenêtre est de 30 ms debout (95 ms accroupi) à −10 m/s. Faut-il passer `FIST_PJ_FLOOR_RANGE` à **2,6 m** (pieds ≤ 1,0 m, fenêtre d'environ 0,1 s) ?
3. Un coup dans le décor (punch-jump) compte-t-il comme coup « utile », ce qui évite la recharge anti-spam de 0,6 s ? (Proposé : oui.)
4. **Une seule cible par coup** (proposé) ou toutes les cibles de l'arc comme l'épée ?
5. **Tolérance de parry de l'hôte à 0,05 s** pour les fenêtres ouvertes par un poing (sinon le joueur en combo est paré à 95 %) : d'accord ? Faut-il aussi revoir celle de l'épée (0,15 s, soit 60 % de couverture en enchaînant) ?
6. **Nouvelle signification de `NetPState.melee`** pour `MK_FIST` (étape sur 2 bits, avancement sur 6 bits) et de `EV_MELEE.sub` : à valider à la relecture croisée, si d'autres fiches (Lance, Enclume) utilisent aussi ces champs.
7. **Maintenir la touche = enchaîner automatiquement** (comme le bhop), en plus du tampon de 0,12 s ?
8. **Suspension en l'air** (`FIST_AIR_HANG` 2 m/s) pour combattre les drones au poing : la garder, la limiter au solo, ou la retirer ?
9. Le **choc** entre deux poings (les deux en fenêtre) : combo cassé de 0,7 s pour les deux (proposé), ou rien ?
