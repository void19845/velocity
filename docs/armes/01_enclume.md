# Fiche 01 : Marteau-pilon « Enclume »

> Source : [`../IDEES_ARMES_CARTES.md`](../IDEES_ARMES_CARTES.md) § 2.1 (validé), § 1.2 (chiffres PvP), § 1.5 (munitions).
> Interfaces figées : [`PLAN.md`](PLAN.md) § 3. Loadout, `P.meleeKind`, `ammo_points` (jauges, fiche 00), `NET_VER` 4 : fiche `00_infra_loadout_munitions.md` (non redéfinis ici).
> Identifiants : `MK_ANVIL` (1), `W_SLAM` (7), `EV_SLAM` (21), `ST_SLAM` (0x10 dans `NetPState.pad`). **Rien n'est implémenté.**

---

## 1. Résumé validé

- **Rôle** : la mêlée verticale. L'épée récompense la vitesse horizontale relative ; l'Enclume récompense la **hauteur**.
- **Coup au sol** (ou en l'air trop près du sol) : coup lourd frontal, recharge 0,8 s, portée `2,6 + 0,10 × v_rel` m, cône cos 0,45, dégâts `45 + 3 × v_rel` (45 / 75 à 0 / 10 m/s, chiffres du tableau § 2.1), recul 14 vers l'avant + 6 vers le haut. Fenêtre de parry **0,10 s**.
- **Piqué** (appui en l'air) : `vel.y = min(vel.y, −30)`, vitesse horizontale conservée, plus de contrôle aérien, `|vy|` plafonné à 45.
- **Onde de choc** à l'atterrissage : rayon `2,5 + 0,06 × |vy|`, dégâts `25 + 1,5 × |vy|`, atténuation `0,3 + 0,7 × f` comme `explode`, uniquement les cibles dont les pieds sont à moins de **1,0 m** (décision du 27/10) au-dessus du sol d'impact. Recul vertical 12.
- **Touche** : rebond `vy = 9` + 1 dash. **Raté** : immobilisé 0,6 s.
- **Parry** : le piqué ne pare rien ; une victime en fenêtre de parry à l'impact **pare l'onde** (chemin `EV_MELEE_PARRY`) : l'attaquant est renvoyé en l'air, marteau bloqué 1 s.
- **Munitions** (§ 1.5) : une touche rend `melee_hit_points(meleePts[P.meleeKind])`, le kill et le parry passent par `00_infra`.

**Corrections et précisions issues de l'étude du code** (à valider, voir § 13) :

1. *Chiffre du jump pad* : le tableau § 2.1 prend `|vy| = 30` pour un piqué au sommet d'un pad. En réalité le pad lance à `vy = 20`, sommet à `20² / (2 × 24) = 8,3 m` ; un piqué à 30 m/s depuis 8,3 m arrive à `√(30² + 2 × 24 × 8,3) = 36,1 m/s` → **79 PvE / 55,4 PvP**, rayon 4,67 m.
2. *Le bhop rend le piqué omniprésent* : `bot_think` et les joueurs bhoppent, donc sont presque toujours « en l'air ». Sans garde-fou, le coup au sol deviendrait inutilisable et un simple saut donnerait un piqué à 49 PvP. D'où une **hauteur minimale** `SLAM_MIN_H = 1,2 m` (sinon c'est le coup au sol) et un **facteur de hauteur** `kh` (dégâts réduits sous 4 m de chute).
3. *« Sauter l'esquive » ne marchait pas avec 1,5 m* (**réglé le 27/10 : seuil à 1,0 m**) : un saut simple culmine à `8,4² / 48 = 1,47 m` < 1,5 m. Avec le seuil validé, seul un double saut (≈ 2,8 m) esquive. Recommandation : 1,0 m (le saut simple passe au-dessus pendant 0,40 s, de t = 0,15 à 0,55 s). Gardé à 1,5 dans les constantes en attendant la décision.

## 2. Règles détaillées

### Machine à états (`P.anvilState`)

| État | Entrée | Pendant | Sortie |
|---|---|---|---|
| `ANVIL_IDLE` 0 | défaut | coup au sol possible si `meleeCd <= 0` | appui mêlée en l'air à ≥ `SLAM_MIN_H` → `DIVE` |
| `ANVIL_DIVE` 1 | `slam_start` | `ST_SLAM` émis ; pas de contrôle aérien, sauts et grappin ignorés, wall-run interdit, `vy ≤ −30`, `|vy| ≤ 45` | sol → `slam_land` ; contact direct → `slam_land` ; dash ou poussée vers le haut → annulé (`IDLE`) ; mort → `IDLE` |
| `ANVIL_STUN` 2 | piqué raté | 0,6 s : vitesse horizontale nulle, sauts, dash, grappin et mêlée ignorés ; tir permis | fin du minuteur, jump pad, recul reçu vers le haut, mort |

### Appui sur la mêlée (`anvil_press`, appelé par `melee` quand `P.meleeKind == MK_ANVIL`)

- `P.grounded`, ou hauteur sous les pieds < `SLAM_MIN_H` (rayon vers le bas `ray_world`) → **coup au sol** (`anvil_strike`).
- Sinon → **piqué** (`slam_start`). Le piqué n'ouvre **pas** de fenêtre de parry (`parryWin` reste 0) et ne déclenche donc jamais la règle du coup dans le vide de `game_tick`.
- `anvilState != IDLE` → appui ignoré.

### Calcul de l'onde (`slam_land`)

- `vy = min(|vy à l'impact|, 45)` ; `prevVy` de `player_tick` (vitesse après gravité, avant le déplacement) sert de vitesse d'impact.
- `H = anvilStartY − P.pos.y` (hauteur réellement chutée) ; `kh = clamp(H / SLAM_FULL_H, 0,5, 1)` (`SLAM_FULL_H = 0` désactive).
- `R = 2,5 + 0,06 × vy` ; `D = (25 + 1,5 × vy) × kh` ; pour chaque cible : `d` = distance **horizontale** au point d'impact, `Rc = R + rayon cible` (0,5 joueur, `enemy_radius` ennemi), `f = 1 − d / Rc`, dégâts `D × (0,3 + 0,7 f)`.
- Filtre vertical : pieds de la cible entre `sol − SLAM_REACH_DOWN` (1 m, pour ne pas toucher l'étage du dessous) et `sol + SLAM_REACH_UP` (1,0 m).
- Ligne de vue `los(impact + 0,5 m, centre cible)` : contrairement à `explode`, l'onde ne traverse pas les murs (elle rampe au sol).
- Recul : horizontal `SLAM_KNOCK_OUT × f` vers l'extérieur + vertical `SLAM_KNOCK_UP` (12) fixe. Un joueur projeté monte à `12² / 48 = 3 m` et reste ~1 s en l'air : fenêtre du rail.
- **Pas de dégâts sur soi** ni de recul sur soi. Le coût est l'immobilisation en cas de raté.

### Cas limites

- **Piqué très près du sol** : sous 1,2 m, c'est le coup au sol (en l'air). Entre 1,2 et 4 m, `kh` réduit les dégâts : saut simple (1,47 m) → 36 PvE / 25 PvP ; double saut (2,8 m) → 51 / 36.
- **Contact direct** : les joueurs ne se bloquent pas entre eux (`move_axis` ne teste que `boxes`). Pendant `DIVE`, si la boîte du joueur chevauche `np_aabb(i, 0)` (ou la sphère d'un ennemi) **et** que la cible est hors d'atteinte de l'onde (pieds > `SLAM_REACH_UP` au-dessus du sol sous elle), on résout tout de suite : cette cible seule prend `D` plein (`f = 1`), rebond. Une cible au sol est laissée à l'onde (elle arrive 1 ou 2 ticks plus tard).
- **Jump pad** : `slam_land` s'exécute (onde normale), puis le pad relance (`vel = launch`) et efface l'immobilisation. Une boucle pad → piqué → pad dure ≈ 0,83 + 0,25 = 1,1 s > `SLAM_CD` 0,8 s (voir § 13).
- **Wall-run** : un piqué peut partir d'un wall-run (`wallrun = 0`) ; pendant `DIVE`, la condition de wall-run de `player_tick` est fausse (sinon `vy` serait bridé à −1).
- **Grappin** : `slam_start` lâche le grappin (`hook_release(0)`) ; tir du grappin ignoré en `DIVE` et en `STUN`. Combo voulu : grappin vers un perchoir, lâcher, piqué.
- **Dash pendant le piqué** : autorisé, il **annule** le piqué (le code du dash met déjà `vy ≥ 2` en l'air), coûte la charge, `meleeCd = SLAM_CD`. C'est l'échappatoire payante.
- **Poussée externe** (roquette, recul d'`EV_HIT`, renvoi de parry) : si `vel.y > SLAM_BREAK_VY` (−15) pendant `DIVE`, le piqué est cassé. Contre-jeu : une roquette sous le météore l'arrête.
- **Hitstop** : `game_tick` sort tôt, l'état est gelé ; le hitstop posé par `slam_land` fige le rebond déjà appliqué.
- **Mort / respawn** : `P.dead` → `IDLE` dans `player_tick` ; `EV_RESPAWN` et `reset_game` remettent `anvilState`, `anvilStunT` à 0.
- **Fin de match** : l'onde reste visuelle ; `host_handle` ignore les `EV_HIT` (`matchOver`).
- **Rebond et jonglage** : vol du rebond `2 × 9 / 24 = 0,75 s` < `SLAM_CD` 0,8 s, et la cible projetée est hors de l'onde : pas de piqué en boucle sur une cible en l'air (sauf contact direct).

## 3. Constantes

```c
/* Enclume (marteau-pilon) : coup au sol */
#define ANVIL_CD           0.8f    /* recharge du coup au sol */
#define ANVIL_WHIFF_CD     1.2f    /* coup au sol dans le vide (propose) */
#define ANVIL_RANGE        2.6f    /* portee de base */
#define ANVIL_RANGE_PER_MS 0.10f   /* portee en plus par m/s relatif */
#define ANVIL_DMG          45.0f   /* degats de base */
#define ANVIL_DMG_PER_MS   3.0f    /* degats en plus par m/s relatif */
#define ANVIL_CONE         0.45f   /* cosinus du demi-angle (plus large que l'epee) */
#define ANVIL_KNOCK        14.0f   /* recul vers l'avant */
#define ANVIL_KNOCK_UP     6.0f    /* recul vers le haut */
#define ANVIL_PARRY_WINDOW 0.10f   /* fenetre de parry, plus courte que l'epee */
/* Enclume : pique et onde de choc */
#define SLAM_VY            30.0f   /* vitesse de chute minimale du pique */
#define SLAM_VY_MAX        45.0f   /* plafond de |vy| */
#define SLAM_MIN_H         1.2f    /* hauteur minimale sous les pieds pour piquer */
#define SLAM_FULL_H        4.0f    /* hauteur de chute pour 100% des degats (0 = desactive) */
#define SLAM_RADIUS        2.5f    /* rayon de base de l'onde */
#define SLAM_RADIUS_PER_MS 0.06f   /* rayon en plus par m/s de chute */
#define SLAM_DMG           25.0f   /* degats de base de l'onde */
#define SLAM_DMG_PER_MS    1.5f    /* degats en plus par m/s de chute */
#define SLAM_REACH_UP      1.0f    /* l'onde ne touche que les pieds a moins de 1 m du sol : un saut simple l'esquive */
#define SLAM_REACH_DOWN    1.0f    /* ni ce qui est plus bas que le sol d'impact */
#define SLAM_KNOCK_UP      12.0f   /* recul vertical des cibles */
#define SLAM_KNOCK_OUT     8.0f    /* recul horizontal au centre */
#define SLAM_BOUNCE        9.0f    /* rebond si l'onde touche */
#define SLAM_STUN          0.6f    /* immobilisation si l'onde rate */
#define SLAM_CD            0.8f    /* recharge apres l'atterrissage */
#define SLAM_BREAK_VY      -15.0f  /* poussee vers le haut qui casse le pique */
#define SLAM_PARRY_GRACE   0.2f    /* tolerance de l'hote pour parer l'onde */
#define SLAM_PARRIED_UP    12.0f   /* attaquant pare : renvoye en l'air */
enum { ANVIL_IDLE, ANVIL_DIVE, ANVIL_STUN };
```

## 4. Données

- `Player` : `int anvilState;` `float anvilStartY;` (y au début du piqué) `float anvilStunT;` `float anvilLandT;` (temps depuis le dernier atterrissage de piqué, incrémenté dans `game_tick`, sert à reconnaître un parry de l'onde) `float anvilLastVy;` (tests, journal).
- `NetPState.pad` : bits 0-3 = `meleeKind` (infra), bit `ST_SLAM` = `anvilState == ANVIL_DIVE`. Rien d'autre.
- `NetPlayer` : `unsigned char anvilPrevPad;` (front montant de `ST_SLAM` pour le sifflement et la traînée chez les autres).
- Statistiques hôte : `slamHits`, `slamParried`, `slamRailCombos`, `double slamLastHitT[MAXP]`, `int slamLastBy[MAXP]` (voir § 11).

## 5. Algorithme

### `melee` (aiguillage)

```c
static void melee(void) {
    if (P.meleeKind == MK_ANVIL) { anvil_press(); return; }
    ... /* epee inchangee */
}
static float anvil_ground_dist(V3 feet) {      /* distance au sol sous un point, 50 si rien */
    return ray_world(vadd(feet, v3(0, 0.05f, 0)), v3(0, -1, 0), 50, 0) - 0.05f;
}
static void anvil_press(void) {
    if (P.anvilState != ANVIL_IDLE) return;
    if (P.grounded || anvil_ground_dist(P.pos) < SLAM_MIN_H) anvil_strike();
    else slam_start();
}
```

### `anvil_strike` (copie de `melee` avec les constantes `ANVIL_*`)

```c
parried = try_parry();                          /* orbes et roquettes, comme l'epee */
for ennemis : rel, reach = dist - enemy_radius ; if (reach > ANVIL_RANGE + rel*ANVIL_RANGE_PER_MS) continue;
    if (reach > 0.8f && cone < ANVIL_CONE) continue;
    rusher -> 9999 + parry_feedback (comme l'epee) ; drone -> damage_enemy(i, ANVIL_DMG + rel*ANVIL_DMG_PER_MS,
                                                     vadd(vmul(f, ANVIL_KNOCK), v3(0, ANVIL_KNOCK_UP, 0)));
for joueurs np_target : idem, net_hit(i, dmg * PVP_SCALE, meme recul, W_SLAM);
if (dmMode) net_fx_melee(eye, f, sp);           /* EV_MELEE : le recepteur lit meleeKind dans pad */
P.meleeCd = ANVIL_CD; P.meleeT = 0.3f; P.parryWin = ANVIL_PARRY_WINDOW; P.swingLanded = hits || parried;
if (hits) { dash +1 ; melee_hit_points(meleePts[P.meleeKind]) (une fois par coup) ; hitstop = max(hitstop, 0.06f + sp*0.002f);
            shake += 0.25f ; play(S_ANVIL_CLANG, 0.9f, frange(0.9f, 1.05f)); }
play(S_DASH, 0.45f, 0.8f);                      /* souffle plus grave que l'epee */
```

### `game_tick` (coup dans le vide, généralisé)

```c
float win = P.meleeKind == MK_ANVIL ? ANVIL_PARRY_WINDOW : PARRY_WINDOW;
float wcd = P.meleeKind == MK_ANVIL ? ANVIL_WHIFF_CD : MELEE_WHIFF_CD;
if (P.parryWin <= 0 && !P.swingLanded) P.meleeCd = fmaxf(P.meleeCd, wcd - win);
...
if (P.anvilLandT < 10) P.anvilLandT += dt;
anvil_remote_tick();                            /* sifflement + trainee des pique adverses */
```
(Si `00_infra` introduit une table par `meleeKind`, y ranger ces deux valeurs.)

### `slam_start`

```c
if (P.hook != HOOK_NONE) hook_release(0);
P.wallrun = P.sliding = 0;
P.anvilState = ANVIL_DIVE; P.anvilStartY = P.pos.y;
P.vel.y = fminf(P.vel.y, -SLAM_VY);
P.fovKick += 6; shake += 0.1f;
play(S_ANVIL_DIVE, 0.8f, 1.0f);
```

### `player_tick` (modifications)

```c
int diving = P.anvilState == ANVIL_DIVE, stun = P.anvilState == ANVIL_STUN;
if (P.dead) { P.anvilState = ANVIL_IDLE; diving = stun = 0; }
if (diving || stun) { fw = sd = 0; hasWish = 0; wish = v3(0,0,0); jumpPress = 0; }   /* plus de controle */
/* bhop : la condition de saut lit act[A_JUMP] -> ajouter "&& !diving && !stun" */
if (stun) dashPress = 0;
if (diving && dashPress && P.dashCharge >= 1) { P.anvilState = ANVIL_IDLE; P.meleeCd = SLAM_CD; diving = 0; }
grapple_tick(dt, alive && hookPressIn && !diving && !stun, alive && hookHeldIn);
... friction, acceleration (wish nul), gravite ...
if (diving) {
    if (P.vel.y < -SLAM_VY_MAX) P.vel.y = -SLAM_VY_MAX;
    if (P.vel.y > SLAM_BREAK_VY) { P.anvilState = ANVIL_IDLE; P.meleeCd = SLAM_CD; diving = 0; }   /* pique casse */
}
... deplacement ...
if (diving) slam_contact();                     /* contact direct (peut appeler slam_land) */
int onPad = P.grounded && boxes[P.groundBox].kind == B_PAD;
if (P.grounded && !wasG && P.anvilState == ANVIL_DIVE) slam_land(-prevVy);
if (stun) { P.vel.x = P.vel.z = 0; if ((P.anvilStunT -= dt) <= 0) P.anvilState = ANVIL_IDLE; }
if (onPad && alive) { ...pad existant... ; P.anvilState = ANVIL_IDLE; P.anvilStunT = 0; }
wall_check();
if (alive && P.anvilState != ANVIL_DIVE && !P.grounded && ...) /* wall-run */
```

### `slam_land(float vyImpact)` (nouvelle, calquée sur `explode`)

```c
static void slam_land(float vyImpact) { slam_resolve(vyImpact, -1, -1); }
/* directP / directE : cible du contact direct (-1 = onde au sol) */
static void slam_resolve(float vyImpact, int directP, int directE) {
    float vy = fminf(vyImpact, SLAM_VY_MAX), H = P.anvilStartY - P.pos.y;
    float kh = SLAM_FULL_H > 0 ? clampf(H / SLAM_FULL_H, 0.5f, 1) : 1;
    float R = SLAM_RADIUS + SLAM_RADIUS_PER_MS * vy, D = (SLAM_DMG + SLAM_DMG_PER_MS * vy) * kh;
    V3 c = P.pos, eyeC = vadd(c, v3(0, 0.5f, 0));
    int hits = 0;
    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &E[i]; float er = enemy_radius(e->type);
        if (!e->alive || (directE >= 0 && i != directE)) continue;
        float f = 1;
        if (directE < 0) {
            float bot = e->pos.y - er - c.y;
            if (bot > SLAM_REACH_UP || e->pos.y + er < c.y - SLAM_REACH_DOWN) continue;
            V3 dh = v3(e->pos.x - c.x, 0, e->pos.z - c.z);
            float d = hlen(dh), Rc = R + er;
            if (d >= Rc || !los(eyeC, e->pos)) continue;
            f = 1 - d / Rc;
        }
        V3 out = vnorm(v3(e->pos.x - c.x, 0.001f, e->pos.z - c.z));
        damage_enemy(i, D * (0.3f + 0.7f * f), v3(out.x * SLAM_KNOCK_OUT * f, SLAM_KNOCK_UP, out.z * SLAM_KNOCK_OUT * f));
        hits++;
    }
    for (int i = 0; i < MAXP; i++) {             /* LAN : meme boucle sur np_target, np_center, disp.y */
        ... net_hit(i, D * (0.3f + 0.7f * f) * PVP_SCALE, recul, W_SLAM); hits++;
    }
    /* effets locaux */
    for (int k = 0; k < 36; k++) { float a = k * (2*PI/36);
        add_part(vadd(c, v3(0, 0.15f, 0)), v3(cosf(a) * R * 2.2f, 0.6f, sinf(a) * R * 2.2f), v3(1, 0.7f, 0.35f), 0.4f, 0.22f, 0); }
    burst(c, v3(0.75f, 0.72f, 0.7f), 40, 6, 0.6f, 0.2f);
    shards(c, v3(0.55f, 0.52f, 0.5f), 8 + (int)(vy / 4), 9);
    shake += 0.3f + vy * 0.01f; P.landDip = 0.3f;
    play(S_ANVIL_SLAM, clampf(vy / 40, 0.5f, 1), 1.1f - vy * 0.005f);
    if (dmMode) net_fx_slam(c, R, vy, hits);
    P.anvilLastVy = vy; P.anvilLandT = 0; P.meleeCd = SLAM_CD;
    if (hits) {
        P.vel.y = SLAM_BOUNCE; P.grounded = 0; P.airJumps = 1;    /* vitesse horizontale conservee */
        P.dashCharge = fminf(2, P.dashCharge + 1);
        melee_hit_points(meleePts[P.meleeKind]);                                  /* une fois par onde */
        hitMarker = 0.18f; play(S_HIT, 0.7f, 0.8f);
        hitstop = fmaxf(hitstop, fminf(0.12f, 0.05f + vy * 0.0015f));
        if (vy > 35) impact_frame(c, 0.07f);
        P.anvilState = ANVIL_IDLE;
    } else { P.anvilState = ANVIL_STUN; P.anvilStunT = SLAM_STUN; P.vel.x = P.vel.z = 0; }
}
```

### `slam_contact`

```c
for (i in np_target) if (aabb_overlap(paabb, np_aabb(i, 0)) &&
        anvil_ground_dist(NP[i].disp) > SLAM_REACH_UP) { slam_resolve(-P.vel.y, i, -1); return; }
for (j in E vivants) if (sphere_aabb(E[j].pos, enemy_radius, paabb) &&
        anvil_ground_dist(v3(E[j].pos.x, E[j].pos.y - er, E[j].pos.z)) > SLAM_REACH_UP) { slam_resolve(-P.vel.y, -1, j); return; }
```

Note : en contact direct, `slam_resolve` ne fait pas d'onde (seule la cible directe passe la boucle) et le rebond se fait en l'air.

## 6. Réseau

- **Qui calcule** : le frappeur (comme `explode`) : touches sur `NP[i].disp`, un `net_hit(i, …, W_SLAM)` **par cible** et par onde, donc agrégé par construction. L'hôte arbitre PV, plafond 60, parry, kill.
- **État visible** : `pack_state` : `s->pad = (P.meleeKind & 15) | (P.anvilState == ANVIL_DIVE ? ST_SLAM : 0) | …` (les autres bits sont posés par leurs fiches).
- **`EV_SLAM` (21)**, émis par `net_fx_slam` (déclaration anticipée avec `net_fx_melee`) à chaque atterrissage de piqué : `f[0..2]` point d'impact, `f[3]` rayon, `f[4]` `|vy|`, `sub` nombre de touches. `apply_event` : anneau de particules, éclats, `S_ANVIL_SLAM` × `atten`, `shake += 0,15` si à moins de `R + 3` m. Pas d'arbitrage dans `host_handle`.
- **`EV_MELEE`** du coup au sol : inchangé ; `apply_event` lit `NP[ev->from].st.pad & 15` pour choisir couleur et son (`S_ANVIL_CLANG` si `MK_ANVIL`).
- **`host_handle`** : `W_SLAM` rejoint `W_MELEE`, `W_HOOK` dans la liste des coups parables, avec une tolérance propre :
  ```c
  int parable = ev->sub == W_MELEE || ev->sub == W_HOOK || ev->sub == W_SLAM /* || autres fiches */;
  double grace = ev->sub == W_SLAM ? SLAM_PARRY_GRACE : MELEE_PARRY_GRACE;
  if (parable && nowT - NP[v].lastParryT < grace) { ... EV_MELEE_PARRY, clash = 0 si W_SLAM ... }
  ```
  `clash` reste réservé à l'épée (le piqué n'a pas de fenêtre de parry ; un coup au sol contre un coup au sol peut être un choc, comme l'épée).
- **Réaction de l'attaquant** (`apply_event`, `EV_MELEE_PARRY`, `att == myId`) :
  ```c
  if (P.meleeKind == MK_ANVIL && P.anvilLandT < 0.5f) {          /* c'est l'onde qui a ete paree */
      P.anvilState = ANVIL_IDLE; P.anvilStunT = 0;
      P.vel = vadd(v3(P.vel.x, 0, P.vel.z), vadd(vmul(vnorm(vsub(pa, pd)), 6), v3(0, SLAM_PARRIED_UP, 0)));
      P.grounded = 0; P.meleeCd = fmaxf(P.meleeCd, MELEE_PARRIED_CD); hitstop = fmaxf(hitstop, 0.1f);
  } else ... /* reaction existante (coup au sol pare = comme l'epee) */
  ```
  Plusieurs victimes qui parent : plusieurs `EV_MELEE_PARRY`, réaction idempotente sauf la poussée horizontale (acceptable).
- **Latence** : la victime voit le météore arriver (`ST_SLAM`, traînée) et peut anticiper ; `SLAM_PARRY_GRACE` 0,2 s couvre une fenêtre de 0,10-0,12 s ouverte un peu tôt. Le rebond est décidé chez le frappeur avant l'arbitrage ; si l'onde est parée, le renvoi en l'air écrase le rebond.
- **Défaut existant à corriger** : un `EV_HIT` paré est quand même relayé par `host_recv` (`host_handle` puis `apply_event` puis `net_queue`) et la victime applique le recul. Avec 12 m/s vers le haut, une victime qui pare serait projetée. Proposition : dans `apply_event`/`EV_HIT`, la victime ignore le recul d'une arme parable si sa propre fenêtre de parry était ouverte il y a moins de la tolérance (voir § 13).
- `NET_VER` : déjà passé à 4 par l'infra.

## 7. Interactions

| Avec | Effet |
|---|---|
| Parry (victime) | Toute fenêtre de parry (épée, coup au sol de l'Enclume, autres fiches) pare l'onde : `parry_feedback` (soin 10, dash, grappin, `melee_parry_points` via infra) ; attaquant renvoyé en l'air, bloqué 1 s. |
| Parry (frappeur) | Le coup au sol pare orbes et roquettes (`try_parry`, fenêtre 0,10 s). Le piqué ne pare rien. |
| Pièces | Aucune interaction directe (l'onde ne touche pas les pièces). Combo possible : rebond, pièce, rail. |
| Grappin | Lâché au début du piqué ; interdit pendant `DIVE` et `STUN`. Grappin vers un perchoir puis piqué : synergie principale. |
| Jump pad | Onde puis relance ; pas d'immobilisation. |
| Wall-run | Départ de piqué possible depuis un mur ; pas de wall-run pendant le piqué. |
| Dash | Annule le piqué (coûte la charge) ; interdit pendant l'immobilisation. |
| Roquettes | Rocket jump puis piqué ; une explosion qui pousse vers le haut casse le piqué. |
| Rail | Onde → cible à 3 m en l'air pendant ~1 s, trajectoire balistique → rail. Total PvP 55 + 60 = kill. La touche rend des points de jauge : le rail est toujours chargé pour l'enchaînement. |
| Drones (solo) | Ils volent à ~4,5 m au-dessus du joueur : l'onde les rate souvent ; le contact direct les touche. Recul vertical 12 (amorti par `enemy_tick` en ~0,5 s). Kamikazes au sol : touchés par l'onde (50 PV). |
| Munitions | `melee_hit_points(meleePts[P.meleeKind])` une fois par coup au sol qui touche et une fois par onde qui touche ; kill +3 et parry +1 par `00_infra`. |

## 8. Rendu et son

- **`draw_viewmodel`** (branche `P.meleeKind == MK_ANVIL`, à la place de l'épée) : manche `box_faces` 0,05 × 0,05 × 0,9 (gris foncé), tête 0,32 × 0,2 × 0,2 (acier), bande `GL_LIGHTING` désactivée qui chauffe de bleu à orange avec `|vy|`.
  - Repos : tenu bas à gauche. Coup au sol (`meleeT` sur 0,3 s) : rotation autour de X de −110° à +25°, de haut en bas.
  - `DIVE` : marteau levé au-dessus de la tête (−150°), tremblement `sinf(t × 60) × 0,01`, tête incandescente.
  - `STUN` : tête posée en bas de l'écran, abaissement comme `switchT`.
- **Repère d'impact** (`render`, passe additive) : en `DIVE`, ou en l'air au-dessus de `SLAM_MIN_H` avec l'Enclume, la trajectoire est déterministe (pas de contrôle aérien) : `t` tel que `h = v0·t + 12 t²` (avec `v0 = max(−vel.y, 30)`), point `pos + vel_h × t` ; cercle de 32 segments de rayon `R` prévu au sol (`ray_world` vers le bas au point prévu), orange si une cible est dedans.
- **`draw_hud`** : panneau « ENCLUME » à la place de « ÉPÉE » : au sol, `melee_preview` avec les formules `ANVIL_*` ; en l'air, dégâts et rayon prévus du piqué (même calcul que le repère), en gris sous `SLAM_MIN_H`. Barre d'immobilisation pendant `STUN`.
- **`draw_remote_players`** : si `pad & 15 == MK_ANVIL`, marteau au lieu de l'épée (même animation `st.melee`) ; si `ST_SLAM`, marteau levé et corps incliné de 15° vers l'avant.
- **`anvil_remote_tick`** (dans `game_tick`) : pour chaque `np_target` avec `ST_SLAM`, 2 particules orange par tick (traînée) ; au front montant (`anvilPrevPad`), `play(S_ANVIL_DIVE, 0.8 × atten)`.
- **Juice** : hitstop 0,05 + 0,0015 × vy (max 0,12), impact frame 0,07 s au-dessus de 35 m/s, `shards` gris, `landDip` 0,3, `fovKick` +6 au départ.
- **Sons** (`make_sounds`, avant `S_COUNT`) :
  - `S_ANVIL_DIVE` (1,0 s) : sifflement descendant, sinus 1 800 → 500 Hz (`ph += (500 + 1300 × expf(−t × 2,5)) / SR`) + bruit passe-bas (`lp += 0,15 × (noise() − lp)`), enveloppe croissante `t / 1,0` puis coupure. Pas de voix interrompue (`play` ne sait pas arrêter un son) : la durée couvre un piqué typique (0,25 à 0,8 s).
  - `S_ANVIL_SLAM` (1,0 s) : sinus 55 → 30 Hz (`× 0,9 × expf(−t × 4)`) + bruit filtré à coupure descendante (modèle `S_EXPLO`) + deux partiels métalliques 220 et 347 Hz (`× 0,15 × expf(−t × 6)`).
  - `S_ANVIL_CLANG` (0,4 s) : partiels inharmoniques 520, 1 310, 2 150 Hz décroissant à `expf(−t × 12/18/25)` + clic de bruit 5 ms.

## 9. Bots (`bot_think`)

- **Coup au sol** : la règle existante (« épée au contact ») avec `ANVIL_RANGE + rel × ANVIL_RANGE_PER_MS`.
- **Piqué opportuniste** : pendant la phase 1 du cross-up (le bot prend déjà de la hauteur au-dessus de la cible), si `anvil_ground_dist(P.pos) ≥ 2,5`, que la cible est au sol (`|st.vel[1]| < 1`) et que le point d'impact prévu (même calcul que le repère) tombe à moins de `0,5 × R` de `disp + vel_h × t` : `botPress[A_MELEE] = 1` au lieu du dash, `netlog("pique sur %s, %.1f m", …)`.
- **Après un rebond qui touche** : passer au rail (`P.weapon` = rail du loadout) et tirer sur la cible en l'air (la visée existante suffit).
- **Défense** : un adversaire avec `ST_SLAM` dont la position horizontale est à moins de 6 m : `t = (disp.y − P.pos.y) / max(−st.vel[1], 30)`. Si `t < 0,12` : 50 % parry (`botPress[A_MELEE]`), sinon, si `t < 0,35` : double saut (`botPress[A_JUMP]` puis un deuxième au sommet) ou sortie du rayon par un dash.
- Le bot ne pique jamais sous `SLAM_MIN_H` (garanti par `anvil_press`).

## 10. Tests (`run_tests`, couloir x = −20)

Tous en solo sauf 9-10 (partie « LAN simulé »). `P.meleeKind = MK_ANVIL` après `reset_game`.

1. **Piqué depuis 13 m** : `P.pos = (−20, 13, 40)`, `vel = 0`, `grounded = 0` ; drone à `(−20, 1.6, 38)` (`d` = 2 m) ; `anvil_press()` puis `player_tick` jusqu'à `anvilState != DIVE`. Attendu `anvilLastVy ≈ 39,0` (± 0,3), PV drone ≈ 100 − 63,2 = 36,8 (R = 4,84, `f = 0,652`), `vel.y = 9`, dash +1.
2. **Raté** : même chute sans cible ; attendu `STUN`, vitesse horizontale 0 après l'impact, position inchangée après 0,3 s avec `act[A_FWD] = 1`, `IDLE` après 0,7 s.
3. **Trop près du sol** : `P.pos.y = 0,8`, en l'air → `anvil_press` ; attendu coup au sol (`meleeT > 0`, `anvilState == IDLE`).
4. **Piqué bas** : départ à 2 m, drone à `(−18.6, 1.0, 40)` (`d` = 1,4 m, hors contact direct) ; attendu `|vy| ≈ 31,6`, `kh = 0,5`, dégâts ≈ 29,5 (58,9 sans `kh`).
5. **Plafond** : départ à 40 m ; attendu `anvilLastVy = 45`.
6. **Coup au sol** : drone à `(−20, 1.6, 38)` à l'arrêt puis `P.vel = (0, 0, −10)` ; attendu 55 puis 25 PV restants, `meleeCd = 0,8`.
7. **Dash annule** : en `DIVE`, `player_tick(TICK, 0, 1)` ; attendu `IDLE`, `vel.y ≥ 2`, une charge de moins.
8. **Jump pad** : départ en `(ms(16), 5, 0)` au-dessus du pad est ; attendu onde déclenchée (`anvilLandT == 0` juste après), `vel == launch`, `IDLE`.
9. **LAN, onde** : `NP[1].disp = (−20, 0, 38)`, chute de 13 m ; attendu `NP[1].hp ≈ 100 − 43,2 = 56,8` (`f = 0,626`, × 0,7) ; puis `NP[1].disp.y = 1,6` (en l'air) : attendu PV inchangés.
10. **LAN, onde parée** : `NP[1].lastParryT = nowT` puis atterrissage ; attendu `NP[1].hp = 100` (110 plafonné), `P.vel.y ≥ 12`, `P.meleeCd ≥ 1`, `meleeParries` + 1.

## 11. Équilibrage

| Situation | `|vy|` impact | `kh` | Rayon | PvE centre | PvP centre | PvP bord |
|---|---|---|---|---|---|---|
| Coup au sol, v_rel 0 / 10 / 13,6 m/s | — | — | portée 2,6 / 3,6 / 4,0 | 45 / 75 / 86 | 31,5 / 52,5 / **60** | — |
| Saut simple (1,47 m) | 31,2 | 0,5 | 4,37 | 35,9 | 25,1 | 7,5 |
| Double saut (2,8 m) | 32,2 | 0,7 | 4,43 | 51,3 | 35,9 | 10,8 |
| Chute de 4 m | 33,0 | 1 | 4,48 | 74,6 | 52,2 | 15,7 |
| Sommet de jump pad (8,3 m) | 36,1 | 1 | 4,67 | 79,1 | 55,4 | 16,6 |
| Perchoir de 13 m | 39,0 | 1 | 4,84 | 83,6 | 58,5 | 17,6 |
| Chute ≥ 15,4 m | ≥ 40,5 | 1 | 4,93 | ≥ 85,7 | **60** | 18 |
| Chute ≥ 23,4 m (plafond 45) | 45 | 1 | 5,2 | 92,5 | 60 (64,8) | 19,4 |

- Depuis un pad, il faut `f ≥ 0,86` (moins de 0,7 m du centre) pour une touche à 50 PvP (2 touches pour tuer) ; à moins de 2,8 m on reste à ≥ 34 (3 touches). L'onde est donc « 2 touches » au centre, « 3 » sur la moitié du rayon : cohérent avec le tableau § 5.1 (49-60).
- **Journal du bot** (hôte) : `hitCount[W_SLAM]`, `slamHits` (ondes qui touchent / piqués), `slamParried`, `|vy|` moyen, dégâts moyens par onde, `slamRailCombos` (touche `W_RAIL` du même tireur sur la même victime moins de 1,5 s après une onde : `slamLastHitT`, `slamLastBy`), temps pour tuer quand le tueur a l'Enclume, immobilisations subies.
- À surveiller : boucle pad + piqué (fréquence d'onde autour d'un pad), part des kills Enclume, rapport ondes touchées / ratées (cible 40-60 %).

## 12. Étapes d'implémentation

1. Constantes, champs `Player`/`NetPlayer`, sons `S_ANVIL_*` (S, ~60 lignes).
2. `anvil_press`, `anvil_strike`, aiguillage dans `melee`, coup dans le vide généralisé ; tests 3 et 6 (S, ~60 lignes).
3. `slam_start`, modifications de `player_tick`, `slam_resolve`/`slam_land`, `slam_contact` ; tests 1, 2, 4, 5, 7, 8 (M, ~150 lignes).
4. Viewmodel, repère d'impact, panneau HUD (S, ~90 lignes).
5. Réseau : `ST_SLAM` dans `pack_state`, `net_fx_slam` + `EV_SLAM`, `W_SLAM` parable dans `host_handle`, réaction de l'attaquant, `anvil_remote_tick`, `draw_remote_players` ; tests 9 et 10 (M, ~110 lignes).
6. Bot : piqué opportuniste, défense, enchaînement rail (S, ~50 lignes).
7. Matchs de bots (arène actuelle puis La Boucle), réglage de `SLAM_FULL_H`, `SLAM_REACH_UP`, `SLAM_PARRY_GRACE` (S).

Total : **M** (≈ 520 lignes, 1 à 2 jours).

## 13. Questions ouvertes

1. ~~**Seuil de l'onde**~~ : **tranché le 27/10 : 1,0 m**. Un saut simple esquive l'onde pendant 0,40 s.
2. **Piqué près du sol** : valider `SLAM_MIN_H` = 1,2 m et le facteur `kh` (50 % sous 2 m, 100 % dès 4 m), sinon un saut + piqué fait 49 PvP en zone.
3. **Contact direct** sur une cible en l'air (dégâts pleins) : le garder ? Il punit le saut d'esquive juste sous le météore.
4. **Jump pad** : onde puis relance (proposé), ou le pad « absorbe » le piqué (ni onde ni immobilisation) pour éviter le camping des pads ?
5. **Dash pendant le piqué** : annulation autorisée (proposé) ou piqué irrévocable ?
6. **`ANVIL_WHIFF_CD`** 1,2 s pour un coup au sol dans le vide (non chiffré dans le document d'idées).
7. **Convention commune** : le recul d'un coup paré est encore appliqué chez la victime (relais de l'`EV_HIT` par `host_recv`). La correction côté victime (ignorer le recul si sa fenêtre de parry était ouverte) concerne toutes les fiches de mêlée : à trancher à la relecture croisée. De même, faut-il ajouter l'arme parée dans `EV_MELEE_PARRY` (`f[0] = W_*`) ? Ici on s'en passe (`P.anvilLandT`), mais d'autres fiches pourraient en avoir besoin.
8. **Tolérance du parry de l'onde** : `SLAM_PARRY_GRACE` 0,2 s suffit-il, ou l'hôte doit-il retenir les `W_SLAM` 0,1 s pour attendre un parry tardif ?
