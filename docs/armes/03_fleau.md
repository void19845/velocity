# Fiche 03 : Fléau à chaîne « Fléau »

> Source : [`../IDEES_ARMES_CARTES.md`](../IDEES_ARMES_CARTES.md) § 2.3 (validé), § 1.2 et § 1.5. Interfaces : [`PLAN.md`](PLAN.md) § 3.
> Loadout, `P.meleeKind`, `P.ammo[]`, `ammo_points` (jauges, fiche 00), `W_COUNT`, `NET_VER` 4 : voir `00_infra_loadout_munitions.md` (non redéfinis ici).
> Identifiants : `MK_FLAIL 3`, `W_FLAIL 9`, `EV_COIN` sous-type 2 (pièce frappée). Rien n'est implémenté.

---

## 1. Résumé validé

- **Rôle** : *« viens ici ! »*. Mêlée **de contrôle**, longue portée, faibles dégâts, qui **tire la cible vers soi**. Lanceur de combos, pas une arme qui tue. Remplace l'épée dans le loadout (on n'a **qu'une** mêlée : le combo naturel est fléau → arme à distance).
- **Coup** : portée `6 + 0,05 × v_rel` m, arc horizontal de 140° (demi-angle 70°, cos 0,34), recharge 0,6 s, dégâts `18 + 1,5 × v_rel`. Touche **toutes** les cibles dans l'arc.
- **Traction** : recul négatif de 12 m/s vers le lanceur + 3 m/s vers le haut, appliqué par la victime dans `apply_event` (`EV_HIT`), comme tout recul aujourd'hui.
- **Pièce frappée** : le fléau qui touche une de **tes** pièces la frappe : elle part à 45 m/s vers la cible visible la plus proche (aide `PARRY_AIM_COS`), 40 PvE / 28 PvP. Un rail qui la touche en vol déclenche le ricochet ×1,5 normal.
- **Parry** : fenêtre 0,10 s, portée de parry des projectiles étendue à 6 m, renvoi ×0,8 en vitesse. **Fléau paré** : la chaîne s'enroule, le lanceur est tiré vers le pareur.
- **Munitions** (§ 1.5) : un coup qui touche rend +1 à l'arme à distance en main, un kill +3, un parry +1.
- Comme toute mêlée : un coup qui touche rend un dash.

## 2. Règles détaillées

**Machine à états** (une seule variable de temps, `P.flailT`, pour l'animation ; la touche est instantanée comme l'épée) :

| État | Entrée | Durée | Effet |
|---|---|---|---|
| Prêt | `P.meleeCd <= 0` | — | `actP[A_MELEE]` → `flail_swing()` |
| Lancer | appui | 0,30 s (`FLAIL_ANIM`) | touche calculée **au premier tick** ; fenêtre de parry 0,10 s ouverte ; la chaîne s'étend (0 → 0,12 s), balaie (0,12 → 0,22 s), revient (0,22 → 0,30 s) |
| Recharge | fin du lancer | 0,6 s (`FLAIL_CD`) depuis l'appui | rien |
| Raté | fin de fenêtre sans touche ni parry | `FLAIL_WHIFF_CD` 0,9 s depuis l'appui | même règle que l'épée (`game_tick`) |
| Bloqué | fléau paré (`EV_MELEE_PARRY`) | `MELEE_PARRIED_CD` 1 s | traction du lanceur vers le pareur |

**Conditions de touche** (pour chaque ennemi, joueur LAN et pièce à soi), avec `rel = rel_speed(vel_cible)` :

1. `reach = dist(œil, centre) − rayon_cible ≤ 6 + 0,05 × rel`.
2. **Arc horizontal** : `dot(hnorm(d), hfwd) ≥ 0,34`, où `hfwd = (sin yaw, 0, −cos yaw)`. Au contact (`reach < 0,8 m`) on touche toujours, comme l'épée.
3. **Tranche verticale** : `|asin(d.y / |d|) − P.pitch| ≤ FLAIL_VSLAB` (0,6 rad ≈ 34°). Sans elle, l'arc toucherait un drone 6 m au-dessus de la tête.
4. **Ligne de vue : la chaîne ne traverse pas le décor.** `los(œil, centre) || los(œil, centre + 0,6 m en y)` (deux rayons, l'un suffit, pour ne pas rater une cible dont seule la tête dépasse d'un muret). La traction, elle, ne teste aucun chemin : la victime résout ses collisions dans son propre `player_tick`.

**Plusieurs cibles** : chaque cible de l'arc reçoit **un** `net_hit` (ou `damage_enemy`) par coup, ce qui respecte la règle d'agrégation. Le dash (+1) et `melee_hit_points(meleePts[P.meleeKind])` ne sont donnés **qu'une fois par coup**, quel que soit le nombre de cibles. Au plus **une pièce** frappée par coup (la plus proche de l'axe de visée) : quatre pièces frappées d'un coup feraient 4 × 28 PvP sur une même cible, ce qui contourne le plafond.

**Traction, ce que donnent les chiffres** :
- *Victime au sol* : `vy = 3` décolle (`apply_event` met `grounded = 0` car `f[2] > 2`), temps de vol `2 × 3 / 24 = 0,25 s` → 3 m parcourus, puis friction (taux 6/s) → `12 / 6 = 2` m. **≈ 5 m** au total : une cible à 6 m finit à environ 1 m du lanceur.
- *Victime en l'air* : pas de friction, `AIR_CAP` minuscule : elle garde ses 12 m/s, traverse 6 m en 0,5 s et **dépasse** le lanceur si elle ne corrige pas. C'est voulu (additionner les vitesses), mais voir Questions ouvertes.
- *Drone (solo)* : `enemy_tick` ramène `e->vel` vers sa vitesse voulue avec `vlerp(..., dt × 2)` : un recul `v0` se dissipe en un déplacement ≈ `v0 / 2`. 12 m/s → 6 m, soit un drone collé au joueur. On règle donc `v0 = min(12, 2 × (dist − 2,5))` pour qu'il s'arrête **à 2,5 m**, et en 3D (vers l'œil), puisque les drones volent.
- *Kamikaze (solo)* : l'attirer revient à se le faire exploser dessus (18 PV). Comme l'épée, le fléau qui le touche le fait **exploser au bout de la chaîne** (`damage_enemy(i, 9999, ...)`), mais **sans** `parry_feedback` (à 6 m, ce serait un soin gratuit).

**Cas limites** :
- *Mort* : `flail_swing` n'est pas appelé si `P.dead`. Une pièce déjà frappée continue son vol et peut toucher (comme une roquette tirée avant la mort).
- *Respawn* (`EV_RESPAWN`, `reset_game`) : `flailT = 0`, `flailHitN = 0`, `flailParriedT = 0`.
- *Changement d'arme* : sans objet, la mêlée ne change qu'au spawn (loadout).
- *Hitstop* : `game_tick` sort tôt ; la touche et les reculs sont déjà appliqués, l'animation reprend après.
- *Fin de match* : l'hôte ignore les dégâts (`matchOver`), mais la victime applique quand même le recul à la réception de `EV_HIT` (relayé par l'hôte avant arbitrage). Comportement identique aux armes existantes, sans gravité.
- *Double parry* : si deux victimes d'un même coup parent, l'attaquant reçoit deux `EV_MELEE_PARRY` ; il n'applique que la première traction dans une fenêtre de 0,2 s (`P.flailParriedT`).

## 3. Constantes

```c
/* Fleau a chaine : controle a longue portee, attire la cible */
#define FLAIL_RANGE         6.0f    /* portee de base */
#define FLAIL_RANGE_PER_MS  0.05f   /* +0.05 m par m/s de vitesse relative */
#define FLAIL_ARC_COS       0.34f   /* cos du demi-angle horizontal (70 deg, arc de 140) */
#define FLAIL_VSLAB         0.6f    /* ecart vertical max autour du pitch (rad) */
#define FLAIL_CD            0.6f
#define FLAIL_WHIFF_CD      0.9f    /* coup dans le vide */
#define FLAIL_DMG           18.0f
#define FLAIL_DMG_PER_MS    1.5f
#define FLAIL_PULL          12.0f   /* traction vers le lanceur (recul negatif) */
#define FLAIL_PULL_UP       3.0f
#define FLAIL_DRONE_STOP    2.5f    /* un drone attire s'arrete a cette distance */
#define FLAIL_ANIM          0.30f   /* duree de l'animation de la chaine */
#define FLAIL_PARRY_WINDOW  0.10f
#define FLAIL_PARRY_RANGE   6.0f    /* portee de parry des projectiles */
#define FLAIL_PARRY_SPEED   0.8f    /* renvoi plus lent */
#define FLAIL_PARRIED_PULL  16.0f   /* fleau pare : le lanceur est tire vers le pareur */
#define FLAIL_PARRIED_UP    4.0f
#define FLAIL_COIN_SPEED    45.0f   /* piece frappee */
#define FLAIL_COIN_DMG      40.0f   /* x PVP_SCALE = 28 en PvP */
#define FLAIL_COIN_LIFE     1.5f    /* 67 m de vol */
#define FLAIL_COIN_SEEK     60.0f   /* portee de recherche de cible */
#define FLAIL_COIN_HIT_R    0.4f    /* marge de touche de la piece frappee */
```

## 4. Données

```c
/* Player */
float flailT;          /* animation restante de la chaine (0..FLAIL_ANIM) */
V3    flailHitPos;     /* point ou la chaine s'est accrochee (premiere cible touchee) */
int   flailHitN;       /* cibles touchees par le dernier coup (0 = chaine libre) */
float flailParriedT;   /* garde anti double traction apres un parry */
int   flailLastTgt;    /* derniere cible LAN touchee (id + 1), pour le combo fleau -> rail */
float flailLastT;      /* temps restant de la fenetre de combo (1,0 s) */
/* Coin */
int   flailStruck;     /* 1 = frappee au fleau : vol droit, sans gravite */
```

- `NetPState` : rien de nouveau. `pad & 0x0F = MK_FLAIL` (infra), `melee` = avancement du coup (déjà transmis). **Aucun bit `ST_*`** : le fléau n'a pas d'état maintenu.
- `pack_state` : `s->melee` doit être normalisé par la durée d'animation du type de mêlée (`FLAIL_ANIM` = 0,30 s au lieu de 0,25 s) : `clampf(P.meleeT / melee_anim(), 0, 1) * 255`.

## 5. Algorithme

**`melee()`** : aiguillage (posé par l'infra) `if (P.meleeKind == MK_FLAIL) { flail_swing(); return; }`.

**`flail_swing()`** (nouvelle) :

```c
static float flail_range(float rel) { return FLAIL_RANGE + rel * FLAIL_RANGE_PER_MS; }
static float flail_dmg(float rel)   { return FLAIL_DMG + rel * FLAIL_DMG_PER_MS; }
static int flail_in_arc(V3 eye, V3 c, float radius, float rel) {
    V3 d = vsub(c, eye); float dist = vlen(d), reach = dist - radius;
    if (reach > flail_range(rel)) return 0;
    if (reach < 0.8f) return 1;                                   /* contact */
    V3 hf = v3(sinf(P.yaw), 0, -cosf(P.yaw)), hd = vnorm(v3(d.x, 0, d.z));
    if (vdot(hd, hf) < FLAIL_ARC_COS) return 0;
    if (fabsf(asinf(d.y / dist) - P.pitch) > FLAIL_VSLAB) return 0;
    return los(eye, c) || los(eye, vadd(c, v3(0, 0.6f, 0)));   /* la chaine ne traverse pas le decor */
}

static void flail_swing(void) {
    V3 eye = eye_pos(); float sp = vlen(P.vel);
    int hits = 0, parried = try_parry(FLAIL_PARRY_RANGE, FLAIL_PARRY_SPEED);
    P.flailHitN = 0;
    for (i in E vivants) {
        float rel = rel_speed(E[i].vel);
        if (!flail_in_arc(eye, E[i].pos, enemy_radius(E[i].type), rel)) continue;
        V3 to = vsub(eye, E[i].pos); float dist = vlen(to);
        if (E[i].type == EN_RUSHER) damage_enemy(i, 9999, vmul(to, -10 / dist));  /* explose au bout */
        else {
            float v0 = fminf(FLAIL_PULL, 2 * fmaxf(0, dist - FLAIL_DRONE_STOP));
            damage_enemy(i, flail_dmg(rel), vmul(to, v0 / dist));             /* 3D, vers l'oeil */
        }
        flail_mark_hit(E[i].pos); hits++;
    }
    for (i in joueurs, np_target(i)) {
        float rel = rel_speed(np_vel(i)); V3 c = np_center(i);
        if (!flail_in_arc(eye, c, 0.5f, rel)) continue;
        V3 h = vnorm(v3(eye.x - c.x, 0, eye.z - c.z));
        net_hit(i, flail_dmg(rel) * PVP_SCALE, vadd(vmul(h, FLAIL_PULL), v3(0, FLAIL_PULL_UP, 0)), W_FLAIL);
        P.flailLastTgt = i + 1; P.flailLastT = 1.0f;
        flail_mark_hit(c); hits++;
    }
    flail_strike_coin(eye, sp);                       /* au plus une piece, ne compte pas comme touche */
    if (dmMode) net_fx_melee(eye, view_dir(), sp);
    P.meleeCd = FLAIL_CD; P.meleeT = P.flailT = FLAIL_ANIM;
    P.parryWin = FLAIL_PARRY_WINDOW;
    P.swingLanded = hits || parried;
    play(S_FLAIL_SWING, 0.6f, frange(0.95f, 1.05f));
    if (hits) {
        P.dashCharge = fminf(2, P.dashCharge + 1);
        melee_hit_points(meleePts[P.meleeKind]);                      /* une fois par coup (infra) */
        hitMarker = 0.18f; shake += 0.1f;
        hitstop = fmaxf(hitstop, 0.03f);              /* court : la chaine tire, elle ne cogne pas */
        play(S_FLAIL_HIT, 0.8f, frange(0.9f, 1.1f));
    }
}
/* flail_mark_hit(p) : si P.flailHitN++ == 0, P.flailHitPos = p ; burst(p, acier, 14, 6, 0.3, 0.1). */
```

**`flail_strike_coin(eye, sp)`** (nouvelle) :

```c
int ci = -1; float bestAx = -1;
for (i < MAXCOIN) {
    Coin *c = &CO[i];
    if (!c->alive || c->owner != myId || c->flailStruck) continue;   /* seulement mes pieces */
    if (!flail_in_arc(eye, c->pos, COIN_RADIUS, rel_speed(c->vel))) continue;
    float ax = vdot(vnorm(vsub(c->pos, eye)), view_dir());
    if (ax > bestAx) { bestAx = ax; ci = i; }
}
if (ci < 0) return;
Coin *c = &CO[ci]; V3 f = view_dir(), aim = f; float best = PARRY_AIM_COS;
/* 1. aide a la visee : la cible la mieux alignee avec le viseur (comme try_parry) */
for (ennemis vivants et np_target) { V3 to = vnorm(cible - c->pos);
    if (vdot(to, f) > best && los(c->pos, cible)) { best = vdot(to, f); aim = to; } }
/* 2. sinon la cible visible la plus proche, devant soi (dot > FLAIL_ARC_COS) */
if (best == PARRY_AIM_COS) { float bd = FLAIL_COIN_SEEK;
    for (...) if (d < bd && vdot(to, f) > FLAIL_ARC_COS && los(c->pos, cible)) { bd = d; aim = to; } }
c->flailStruck = 1; c->vel = vmul(aim, FLAIL_COIN_SPEED); c->life = FLAIL_COIN_LIFE;
if (dmMode) net_fx_coin(2, c->netId, c->pos, c->vel);
impact_frame(c->pos, 0.04f); play(S_FLAIL_COIN, 0.8f, 1.0f);
snprintf(popText, sizeof popText, "PIECE FRAPPEE"); popT = 0.8f;
```

**`coin_tick(dt)`** : branche « frappée » avant la gravité.

```c
if (c->flailStruck) {
    V3 step = vmul(c->vel, dt); float len = vlen(step); V3 dir = vmul(step, 1 / len);
    if (ray_world(c->pos, dir, len, 0) < len) { c->life = 0; ... }        /* local partout, deterministe */
    else if (c->owner == myId && flail_coin_hit(c, dir, len)) { c->alive = 0;
        if (dmMode) net_fx_coin(1, c->netId, c->pos, v3(0,0,0)); continue; }
    else c->pos = vadd(c->pos, step);
    add_part(c->pos, v3(0,0,0), v3(1, 0.85f, 0.3f), 0.2f, 0.09f, 0);       /* trainee doree */
    /* fin de vie : comme aujourd'hui */
}
```

`flail_coin_hit` (lanceur seulement) : `ray_sphere(c->pos, dir, E.pos, enemy_radius + FLAIL_COIN_HIT_R, len)` → `damage_enemy(i, FLAIL_COIN_DMG, vmul(dir, 8))` ; joueurs : `ray_box` sur `np_aabb(i, FLAIL_COIN_HIT_R)` → `net_hit(i, FLAIL_COIN_DMG * PVP_SCALE, vmul(dir, 6), W_COIN)`. On choisit **`W_COIN`**, pas `W_FLAIL` : sinon l'hôte appliquerait la règle de parry de mêlée à une pièce venue de 30 m. Puis `burst`, `hitMarker`, `impact_frame(at, 0.05f)`.

**`fire_rail` / `ricochet`** : aucun changement. `fire_rail` teste déjà toutes les pièces vivantes ; une pièce frappée touchée par le rail est consommée par `ricochet` (×1,5 si c'est la mienne, renvoi sans bonus si c'est celle d'un autre). Ses 40 dégâts propres sont **remplacés** par le ricochet (150 PvE / 60 PvP).

**`try_parry(float range, float speedMult)`** : la portée de base `MELEE_RANGE + 1,5` devient le paramètre `range` (× `PARRY_RANGE_SHRINK^parries` inchangé), et la vitesse de renvoi `PARRY_SPEED × PARRY_PINGPONG^(parries−1) × speedMult`. Le multiplicateur de dégâts ne change pas. Épée : `try_parry(MELEE_RANGE + 1.5f, 1)` ; fléau : `try_parry(6, 0,8)` → 6 m puis 5,1 m, 4,3 m ; renvoi à 28,8 m/s puis 33,1 m/s. D'autres fiches de mêlée modifient la même signature : à harmoniser en relecture croisée.

**`game_tick`** : l'appel `try_parry()` de la fenêtre et la recharge de raté passent par le type de mêlée :

```c
if (P.parryWin > 0) {
    if (!P.dead) melee_kind_parry();                  /* try_parry avec les parametres du type */
    P.parryWin -= dt;
    if (P.parryWin <= 0 && !P.swingLanded) P.meleeCd = fmaxf(P.meleeCd, melee_whiff_cd() - melee_parry_window());
}
P.flailT -= dt; P.flailLastT -= dt; P.flailParriedT -= dt;
```

**`apply_event`** :
- `EV_HIT`, `sub == W_FLAIL` : la victime applique le recul (code existant, rien à changer). En plus, **pour tous** : `add_beam(main de l'attaquant, centre de la cible, acier, 0,25 s)` : la chaîne tendue (l'attaquant est `ev->from`, `pcenter()` si c'est moi).
- `EV_COIN`, `sub == 2` : chercher la pièce par `netId` (sinon la créer avec `owner = from`), puis `flailStruck = 1`, `pos = a`, `vel = b`, `life = FLAIL_COIN_LIFE`. Chez les autres, elle est purement visuelle (seul le lanceur teste les cibles), mais reste touchable par leur rail.
- `EV_MELEE_PARRY` avec arme parée `W_FLAIL` (voir § 6), côté attaquant :

```c
if (att == myId && weapon == W_FLAIL) {
    if (P.flailParriedT <= 0) {                       /* une seule traction par coup */
        V3 h = vnorm(v3(pd.x - pa.x, 0, pd.z - pa.z));
        P.vel = vadd(P.vel, vadd(vmul(h, FLAIL_PARRIED_PULL), v3(0, FLAIL_PARRIED_UP, 0)));
        P.grounded = 0; P.flailParriedT = 0.2f;
    }
    P.meleeCd = fmaxf(P.meleeCd, MELEE_PARRIED_CD); hitstop = fmaxf(hitstop, 0.1f); shake += 0.3f;
    snprintf(popText, ..., "CHAINE RETOURNEE PAR %s", pname(def));
}
```

Chiffres : 16 m/s + 4 vers le haut → 0,33 s de vol (5,3 m) puis ≈ 2,7 m de glissade : le lanceur, parti de 6 m, **atterrit sur le pareur**, droit dans son épée (s'il en a une).

## 6. Réseau

| Quoi | Qui calcule | Événement |
|---|---|---|
| Touche du fléau (dégâts + vecteur de traction) | lanceur, sur `NP[i].disp` | `EV_HIT`, `sub = W_FLAIL`, `f[0]` dégâts PvP, `f[1..3]` recul négatif |
| Traction | victime, à la réception (`apply_event`) | — |
| Animation de la chaîne | chacun, depuis `NetPState.melee` + `yaw`/`pitch` + `pad & 0x0F` | `EV_MELEE` (`net_fx_melee`) pour le son et l'arc |
| Pièce frappée (départ) | lanceur | `EV_COIN` `sub = 2`, `netId`, `f[0..2]` pos, `f[3..5]` vitesse |
| Pièce frappée (touche) | lanceur | `EV_HIT` `W_COIN`, puis `EV_COIN` `sub = 1` |
| Pièce frappée (rail) | tireur du rail (`ricochet`) | `EV_COIN` `sub = 1` (existant) |
| Parry du fléau | **hôte** (`host_handle`) | `EV_MELEE_PARRY` |

**Arbitrage de l'hôte** : `W_FLAIL` entre dans la liste des coups parables (PLAN § 3). Le test reste purement temporel (`nowT − NP[v].lastParryT < MELEE_PARRY_GRACE`) : la portée de 6 m est assurée par le lanceur. Le **choc** (`clash`) reste réservé à `W_MELEE` : deux fléaux lancés l'un vers l'autre dans la même fenêtre sont **tous deux parés**, et les deux lanceurs sont tirés l'un vers l'autre (« chaînes croisées »), sans code de plus. Pour que l'attaquant sache *quelle* arme a été parée, l'hôte écrit l'arme dans `EV_MELEE_PARRY.f[0]` (`(float)ev->sub`) ; `sub` garde son sens (choc) et le code existant ne change pas. Statistiques : `meleeParries` compte aussi les fléaux parés, `hitCount[W_FLAIL]`.

**Latence** : à 6 m, l'écart dû au lissage de `disp` (≤ 0,1 s d'extrapolation) est négligeable devant la portée. Le recul est un **vecteur** calculé par le lanceur : si la victime a bougé de 1 à 2 m entre-temps, elle est tirée dans la direction d'alors (erreur angulaire < 15°). La pièce frappée est vue chez les autres avec un retard de ~50 ms (≈ 2 m à 45 m/s) ; on n'extrapole pas (pas d'estimation de latence dans `NetEvent`). Course possible entre la touche de la pièce (lanceur) et un rail adverse dessus : les deux effets peuvent avoir lieu, comme aujourd'hui pour les pièces (l'hôte n'arbitre pas les pièces).

## 7. Interactions

- **Parry (le porteur du fléau pare)** : orbes et roquettes à 6 m (au lieu de 3,9 m), renvoi à 28,8 m/s, `parry_feedback` complet, des points de jauge (`melee_parry_points`, infra). Un kamikaze n'est pas « paré » par le fléau (pas de soin), il explose simplement.
- **Pièces** : enchaînement « pièce → fléau » : la pièce lancée (`P.vel + 9 f + 6 y`) est à ~3 m devant et 1 m plus haut après 0,3 s, avec `rel ≈ 9` → portée 6,45 m : bien dans l'arc. Les pièces **adverses** sont ignorées par le fléau (voir Questions).
- **Rail** : *fléau → rail* : la cible tirée suit une parabole connue (vy 3 puis gravité) ; *pièce frappée → rail dans la pièce* = ricochet ×1,5 au moment choisi. Bonus visuel seulement : un `EV_HIT W_RAIL` sur `flailLastTgt` pendant `flailLastT` affiche « RAPPEL + RAIL » et un `impact_frame` de 0,06 s (aucun bonus de dégâts).
- **Roquettes** : tirer la cible puis viser ses pieds pendant les 0,25 s de vol.
- **Grappin** : grappin sur un joueur + fléau à l'arrivée = deux tractions opposées qui s'additionnent (vitesses relatives fortes → dégâts 18 + 1,5 × 30 = 63 PvE / 44 PvP).
- **Vide** (cartes Puits, Canyon, plan de mort à venir) : en l'air au-dessus du vide, on décroche la cible de son rebord. Il faudra que l'hôte attribue la chute au dernier attaquant (`lastHitBy` < 3 s) : à prévoir avec l'infrastructure de cartes, pas ici.
- **Munitions** : +1 par coup qui touche (pas par cible), rien pour la pièce frappée ni pour sa touche (c'est une pièce, pas la mêlée).

## 8. Rendu et son

**Chaîne (monde, `render`, après le câble du grappin)**. Elle fait 6 à 8 m : elle se dessine dans la passe du monde (profondeur réelle), pas dans le viewmodel.

```c
static void draw_chain(V3 a, V3 b, float sag, float k) {        /* 16 maillons, deux teintes alternees */
    glLineWidth(3); glBegin(GL_LINES);
    for (int s = 0; s < 16; s++) {
        float u0 = s / 16.0f, u1 = (s + 1) / 16.0f;
        V3 p0 = vadd(vlerp(a, b, u0), v3(0, -sag * 4 * u0 * (1 - u0), 0));    /* chainette approchee */
        V3 p1 = vadd(vlerp(a, b, u1), v3(0, -sag * 4 * u1 * (1 - u1), 0));
        if (s & 1) glColor3f(0.55f, 0.57f, 0.62f); else glColor3f(0.30f, 0.31f, 0.35f);
        glVertex3f(p0...); glVertex3f(p1...);
    }
    glEnd();
}
```

Position de la tête, avec `k = 1 − flailT / FLAIL_ANIM` : extension `ext = k < 0.4 ? k / 0.4 : k < 0.73 ? 1 : 1 − (k − 0.73) / 0.27` ; angle horizontal `ang = 70° → −70°` sur `k ∈ [0,1 ; 0,7]` ; `tête = main + rot_y(hfwd, ang) × flail_range(vitesse) × ext` (avec le pitch). Si `flailHitN > 0`, la tête va droit sur `flailHitPos` et revient avec elle (chaîne tendue, `sag = 0`). Sinon `sag = 0,4 × (1 − ext)`. Tête : petite boîte 0,12 m + 3 pointes (`box_faces`), puis halo `billboard` additif ; dans `game_tick`, tant que `flailT > 0`, une particule par tick sur la tête (`add_part`, orange au-delà de 15 m/s comme l'épée).

**Viewmodel (`draw_viewmodel`)** : si `MK_FLAIL`, manche (`box_faces` 0,03 × 0,03 × 0,25) incliné dans la main gauche ; au repos, tête suspendue sous le manche, balancée par `P.swayX`/`P.bob` ; pendant `flailT > 0`, la tête est masquée (elle est dans le monde).

**Joueurs distants (`draw_remote_players`)** : si `(st.pad & 0x0F) == MK_FLAIL` et `st.melee > 0`, même `draw_chain` depuis l'épaule (`disp + (0,42 ; h − 0,6 ; −0,1)` tourné par `yaw`), avec `k = 1 − melee / 255`. La chaîne tendue vers une victime vient de l'`add_beam` de `EV_HIT W_FLAIL`. `EV_MELEE` d'un porteur de fléau : arc de particules à 6 m au lieu de 1,5 m.

**HUD (`draw_hud`)** : le panneau « EPEE » devient « FLEAU (vs cible) », `%d dmg %.1fm` avec `flail_dmg`/`flail_range`, barre de recharge sur `FLAIL_CD`. `melee_preview` garde son cône de visée (0,92). Petits crochets autour du viseur quand une cible est dans l'arc et à portée (même test que `flail_in_arc`, sans `los` pour le coût).

**Sons (`make_sounds`, avant `S_COUNT`)** :
- `S_FLAIL_SWING` (0,35 s) : souffle (bruit filtré passe-bas dont la coupure monte de 300 à 1800 Hz, enveloppe 0,02 s puis `exp(−t × 7)`) + **cliquetis** : toutes les 1/45 s, une impulsion de bruit `exp(−τ × 900)` d'amplitude aléatoire 0,3 à 0,6, enveloppe globale `exp(−t × 6)`.
- `S_FLAIL_HIT` (0,4 s) : choc métallique inharmonique (`sin` 820 + 1230 + 1960 Hz, `exp(−t × 10)`) + coup sourd 70 Hz `exp(−t × 18)` + bruit `exp(−t × 60)`. Joué à 0,6 de hauteur pour « CHAINE RETOURNEE ».
- `S_FLAIL_COIN` (0,3 s) : « ting » 2600 + 3900 Hz `exp(−t × 14)` + souffle descendant court.
- La traction de la victime réutilise `S_HOOK` à hauteur 0,7 (zip de câble), joué dans `apply_event` quand `t == myId`.

## 9. Bots

Dans `bot_think`, si `P.meleeKind == MK_FLAIL` :
- **Attaque** : à la place du test `bd < melee_range(...) + 0,5`, frapper si `see && bd > 2,5 && bd < flail_range(rel) + 0,3` et `P.meleeCd <= 0` (`P.yaw = aimYaw`, `botPress[A_MELEE] = 1`).
- **Suite** : si `P.flailLastT > 0.7f` (moins de 0,3 s après une touche), tirer sur la cible (`botHeld[A_FIRE]`, visée sans erreur supplémentaire) : combo fléau → rail.
- **Pièce frappée** : variante du `coinPlan` : à moins de 25 m, lancer une pièce puis frapper au fléau 0,3 s plus tard au lieu de tirer dedans.
- **Défense** : la détection « adversaire qui lance sa mêlée » passe de 5 m à 7 m quand `(NP[i].st.pad & 0x0F) == MK_FLAIL`.
- **Journal** (`netlog`) : `fleau : %d cible(s), v_rel %.1f, traction vers %s`, `piece frappee -> %s`, `combo fleau -> rail`.

## 10. Tests (`run_tests`)

Dans le couloir dégagé (x = −20, joueur en `(−20, 0, 40)`, `yaw = 0` regarde vers −z), `P.meleeKind = MK_FLAIL` :

1. **Dégâts et traction d'un drone** : drone à `(−20, 1.6, 34.5)`, `flail_swing()` → PV 82, `vel.z > 0` (vers le joueur) ; sur 60 `enemy_tick(TICK)`, distance **minimale** à l'œil < 3,5 m (attendu ≈ 2,5 ; on mesure le minimum car l'orbite du drone le ramène ensuite à 13 m).
2. **À 20 m/s** : `P.vel = (0, 0, −20)`, drone à 7,6 m (portée 7 m + rayon 0,9) → PV 52, dash +1.
3. **Arc** : drones à 0°, ±60° et 80° à 5 m → 3 touchés, celui à 80° intact.
4. **Ligne de vue** : boîte ajoutée temporairement entre le joueur et un drone à 5 m (`addbox`, puis `nb--`) → intact ; `P.swingLanded == 0`, puis recharge de raté ≈ 0,8 s après 0,1 s.
5. **Kamikaze** : à 5 m → mort, PV du joueur inchangés (pas de soin de parry).
6. **Pièce frappée** : pièce à moi en `(−20, 2.5, 37)`, drone à `(−20, 2, 15)` → `flailStruck = 1`, vitesse 45 ; 90 `coin_tick` (22 m en ~0,46 s) → drone à 60 PV, pièce morte.
7. **Pièce frappée + rail** : même départ, puis `fire_rail()` visant la pièce après 3 ticks → `lastChain = 1`, drone mort (150).
8. **Parry à 6 m** : orbe à 5,5 m devant → renvoyée (`PR_PARRIED`), vitesse 28,8 ; avec l'épée, même position → pas renvoyée.
9. **LAN simulé, touche** (`netMode = NET_HOST`, `NP[1]` à `(−20, 0, 35)`) : PV de `NP[1]` = 87,4 (18 × 0,7) ; dernier événement de `outq` : `f[1..3] ≈ (0, 3, 12)`.
10. **LAN simulé, fléau paré** : `NP[1].lastParryT = nowT`, `EV_HIT W_FLAIL` de 0 vers 1 → `NP[1].hp = 100`, `P.vel.z ≈ −16` (vers `NP[1]`), `P.meleeCd ≥ 1`.

## 11. Équilibrage

| v_rel (m/s) | Portée | PvE | PvP (× 0,7) | Touches pour tuer (PvP, 100 PV) |
|---|---|---|---|---|
| 0 | 6,0 m | 18 | 12,6 | 8 |
| 10 | 6,5 m | 33 | 23,1 | 5 |
| 20 | 7,0 m | 48 | 33,6 | 3 |
| 30 | 7,5 m | 63 | 44,1 | 3 |
| 40 | 8,0 m | 78 | 54,6 | **2** (voir Questions) |
| Pièce frappée | — | 40 | 28 | 4 |

Plafond de 60 atteint seulement à `v_rel ≈ 45 m/s`. DPS PvP à v_rel 20 : 33,6 / 0,6 s = 56/s (épée plafonnée : 60 / 0,45 s = 133/s). Combo type : fléau (23) + rail (60) + fléau (23) = 106 → kill en ~1,5 s.

À mesurer dans le journal du bot : touches de fléau par minute, `v_rel` moyen à la touche, nombre de cibles par coup, distance moyenne de traction (position de la victime 0,5 s après), taux de combos fléau → rail (< 1 s), fléaux parés (`meleeParries` filtré par arme), `ttkSum/ttkN` avec et sans fléau dans le loadout.

## 12. Étapes d'implémentation

Prérequis : infra (loadout, `meleeKind`, `ammo_points` (jauges, fiche 00), `W_COUNT`, `NET_VER` 4).

1. Constantes, champs `Player`/`Coin`, remises à zéro, aiguillage dans `melee()`, `pack_state` normalisé (0,5 h).
2. `flail_swing` solo (arc, tranche, ligne de vue, traction des drones, kamikaze) + tests 1 à 5 (1,5 h).
3. `try_parry(range, speedMult)`, fenêtre et raté par type de mêlée dans `game_tick` + test 8 (0,5 h).
4. PvP : `net_hit W_FLAIL`, `W_FLAIL` parable, `f[0]` de `EV_MELEE_PARRY`, traction de l'attaquant paré + tests 9 et 10 (1 h).
5. Pièce frappée : `flail_strike_coin`, branche de `coin_tick`, `EV_COIN` sous-type 2 + tests 6 et 7 (2 h).
6. Rendu : `draw_chain`, viewmodel, joueurs distants, beam de `EV_HIT`, HUD, trois sons (2 h).
7. Bot et journal, puis matchs bot contre bot pour régler les chiffres (2 h).

Total ≈ 9,5 h : coût **M** (≈ 5 h sans la pièce frappée ni le bot).

## 13. Questions ouvertes

1. **Plafond de vitesse relative** : au-delà de `v_rel ≈ 37 m/s` (grappin sur joueur, voies à contresens), le fléau passe à 2 touches et sort de son rôle. Plafonner `v_rel` à 25 m/s (max 55,5 PvE / 38,9 PvP, 3 touches) ?
2. **Traction en PvP** : 12 m/s fixes font dépasser le lanceur à une victime en l'air. Garder (vitesses additionnées, fun) ou passer à `min(12, 2,4 × (dist − 1,5))` comme pour les drones ?
3. **Grappin de la victime** : le fléau doit-il casser le grappin de la cible (`hook_release`) ? Sinon la corde, qui ne se rallonge jamais, annule une partie de la traction.
4. **Kamikaze** : explosion au bout de la chaîne sans soin (proposé), ou simple dégât + recul *positif* ?
5. **Recharge de raté** 0,9 s (comme l'épée en absolu, ×1,5 au lieu de ×2 en relatif) : à valider.
6. **Pièce frappée parable ?** Proposé : non en v1 ; le contre-jeu est l'esquive (trajectoire droite et visible) ou le rail qui la vole. Sinon il faudrait un événement « pièce renvoyée ».
7. **Arme parée dans `EV_MELEE_PARRY`** : `f[0]` proposé ; à harmoniser avec les autres fiches de mêlée (ou figer dans PLAN § 3).
8. **Signature de `try_parry`** : `(range, speedMult)` proposée ; les fiches Enclume, Lance, Impulsion et Égide la touchent aussi.
9. **Lecture de « cible visible la plus proche avec aide `PARRY_AIM_COS` »** : proposé = d'abord la cible alignée avec le viseur (9°), sinon la plus proche devant soi. Ou seulement la plus proche, comme `ricochet` ?
10. **Pièces adverses** : le fléau peut-il frapper une pièce adverse (vol, comme le rail) ? Proposé : non.
11. **Délai de touche** : instantané comme l'épée (proposé), ou 0,08 s pour que la chaîne soit lisible et le parry plus juste à 6 m ?
12. **Tranche verticale** `FLAIL_VSLAB` = 34° : assez pour attraper un drone en vol rasant sans viser ?
