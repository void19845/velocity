# Fiche 09 : Mines collantes « Ventouse »

> Source : [`../IDEES_ARMES_CARTES.md`](../IDEES_ARMES_CARTES.md) § 3.4 (validé), § 1.2 (chiffres PvP), § 1.5 (munitions).
> Interfaces : [`PLAN.md`](PLAN.md) § 3 (`WP_MINE 5`, `W_MINE 15`, `EV_MINE 17`, `PR_MINE 4`).
> Loadout, `P.ammo[]` et jauges de points, `ammo_points`, cellules, `NET_VER` 4 : voir `00_infra_loadout_munitions.md` (non redéfinis ici). Conventions croisées : PLAN.md § 4 (elles l'emportent sur cette fiche).
> Document de design : **rien n'est implémenté**. Le pseudo-code n'est pas un patch.
> Mise à jour du 27/10 : deux commandes de détonation séparées (`A_DETONATE_ALL`, `A_DETONATE_ONE`) ; munitions par jauge (150 points par mine, 3 au maximum, 1 au spawn, aucune recharge passive).

---

## 1. Résumé validé

- **Rôle** : pièges et explosions déclenchées à distance, le pendant « préparation » des roquettes ; outil de mobilité (mine-jump, booster mural).
- **Lancer** : `24 m/s × direction de visée + vitesse du joueur`, gravité 16 (comme les pièces).
- **Colle** au décor (point + normale), à un joueur (position relative, suivie) ou à un drone (solo).
- **2 mines actives au maximum.** Détonation par télécommande, avec **deux commandes séparées** (toutes les mines, ou la plus ancienne), ou automatiquement au bout de 3 s.
- **Souffle** : rayon 4 m, 90 PvE au centre (30 % au bord), **recul 15** (roquette : 13), 5 × f PV sur soi.
- **Munitions** (§ 1.5) : **jauge de 150 points par mine, 3 mines au maximum, 1 au spawn**, aucune recharge passive. La jauge se remplit par la mêlée (épée 50 par coup qui touche, +100 par kill en mêlée, +50 par parry réussi) et par les cellules (100 ou 150 points). Il faut donc 3 coups d'épée ou une grande cellule pour une mine.
- **Collée sur un joueur** : bip et contour rouge chez la victime, armement minimal 0,6 s, **60 PvP garantis** sauf parry.
- **Rail ou ricochet de pièce sur une mine** : explosion ×1,5 (135 PvE, rayon 5 m), y compris sur la mine d'un adversaire (logique « pièce volée »).
- **Parry** : en vol, renvoyée comme une roquette ; collée sur toi, arrachée et renvoyée dans ta direction de visée à 36 m/s.
- **Chiffres PvP** : centre 90 × 0,7 = 63 → **60** ; bord 27 → 18,9 ; collée 60. Deux touches pour tuer.

## 2. Règles détaillées

### 2.1 Machine à états d'une mine (`Proj.kind == PR_MINE`, champ `mineState`)

| État | Entrée | Comportement | Sortie |
|---|---|---|---|
| `MS_FLY` | lancer, ou mine qui retombe | balistique, gravité 16 ; collisions décor / drones / joueurs | collage, détonation (après 0,1 s de vol), ou `MINE_FLY_LIFE` = 3 s écoulées : explose en l'air |
| `MS_WORLD` | rayon de vol qui touche un AABB | fixe en `point + n × 0,06` ; mèche 3 s | détonation, mèche, rail ou pièce |
| `MS_ENEMY` | touche un drone ou un kamikaze (solo) | suit `E[a].pos + mineOfs` ; mèche 3 s | idem ; si le drone meurt avant : retour à `MS_FLY` avec `E[a].vel` |
| `MS_PLAYER` | touche l'AABB d'un joueur LAN | suit la position du porteur + `mineOfs` ; **armement 0,6 s**, mèche 3 s | détonation (une fois armée), parry du porteur, mort du porteur (retour à `MS_FLY`) |
| (renvoyée) | parry | `kind = PR_PARRIED`, `mineState = MS_FLY` : vol droit à 36 m/s, explose au contact comme une roquette renvoyée | explosion |

- La **mèche** de 3 s part **du collage** (une mine qui colle à 2,9 s de vol vit encore 3 s), pour que la mine reste un piège. En vol, la limite séparée de 3 s évite une mine éternelle.
- **Armement** : seule une mine collée sur un **joueur** attend 0,6 s. Une détonation demandée pendant l'armement est mémorisée (`mineDet = 1`) : la mine explose **dès qu'elle est armée**. Un rail sur une mine non armée la traverse sans la déclencher (sinon on contourne le contre-jeu). Sur le décor ou un drone, la mine est armée dès le collage (mine-jump précis).
- Une mine ne colle jamais **à son poseur** (même exclusion que `j == p->owner` dans `proj_tick`).

### 2.2 Télécommande : deux commandes séparées (décision du 27/10)

Un seul bouton (« 1er appui = lancer, 2e appui = détoner ») rendrait impossibles les **2 mines actives** validées. L'utilisateur a choisi **deux commandes séparées**, lues ici comme deux actions de détonation distinctes (une autre lecture est signalée au § 13) :

- `A_FIRE` (clic gauche) **lance** une mine si la Ventouse est en main, `P.fireCd <= 0`, `P.switchT <= 0` et `P.ammo[WP_MINE] >= 1`.
- **`A_DETONATE_ALL`** (id `detoner_tout`, libellé `DETONER TOUT`) fait exploser **toutes** les mines actives du joueur. C'est le geste réflexe : piège déclenché, mine-jump.
- **`A_DETONATE_ONE`** (id `detoner_une`, libellé `DETONER UNE`) fait exploser **la plus ancienne** mine active (plus petit `mineSeq` parmi celles sans `mineDet`). C'est le geste de précision : garder la 2e mine en piège, ou enchaîner deux boosts (mine-jump puis booster mural).
- Les deux commandes marchent **quelle que soit l'arme en main** : mine + rail dans la foulée, c'est le combo voulu. Aucune recharge sur la détonation.
- **Touches par défaut**, choisies parmi celles que `bindDefault` laisse libres (Z/W, Q/A, S, D, Espace, Ctrl, C, Maj, E, clic gauche et droit, V, F, clic milieu, G, X1, 1, 2, molette et Tab sont déjà pris ; R est réservé au redémarrage solo) :
  - `A_DETONATE_ALL` = `{ K_X2, 0x10 }` : bouton de pouce arrière de la souris, et la touche physique à gauche de l'avance (Q en QWERTY, A en AZERTY) ;
  - `A_DETONATE_ONE` = `{ 0x2D, 0 }` : touche X (même position en AZERTY et en QWERTY), sous la main gauche, à côté de C (accroupi) et de V (mêlée).
  - Le clic milieu proposé dans PLAN.md § 4.6 est déjà la 2e touche du grappin (`{ 0x21, K_MMB }`) : on ne le prend pas.
- **3e lancer** : la mine la plus ancienne **explose aussitôt** (ou est marquée `mineDet` si elle n'est pas armée), comme si l'on avait appuyé sur `A_DETONATE_ONE`, puis la nouvelle part. Aucune mine n'est perdue, et la jauge rend ce cas rare (3 mines au maximum).
- Appui sur une commande de détonation sans mine active : clic sec `S_HIT` à hauteur 0,45, rien d'autre.

### 2.3 Cas limites

- **Mort du poseur** : ses mines **restent** et explosent à la fin de leur mèche (3 s au plus, donc avant ou peu après le respawn à 2,5 s). La télécommande est inactive pendant la mort (`P.dead`). `proj_tick` tourne déjà pendant la mort : le client du mort calcule toujours les dégâts. Un kill posthume est attribué normalement (`host_kill` ne soigne le tueur que s'il est vivant).
- **Mort du porteur** : chez le poseur, à réception d'`EV_KILL` avec `target == porteur`, la mine repasse en `MS_FLY` à `disp + mineOfs`, vitesse `0,5 × np_vel(porteur)`. Elle est relancée par `EV_MINE` sous-type 0 et sa mèche continue.
- **Départ d'un joueur** (`EV_INFO` sous-type 1) : chaque client supprime les mines dont `owner` est ce joueur, et décroche celles qui sont collées sur lui.
- **Respawn** : l'infra remet 1 mine et une jauge vide. Les mines encore actives ne sont pas touchées.
- **Changement d'arme** : les mines restent actives et les deux commandes de détonation marchent toujours. Le lancer exige la Ventouse en main.
- **Chargeur vide** (0 mine) : clic sec et bascule automatique sur l'autre arme du loadout (infra). Les mines déjà posées restent détonables.
- **Hitstop** : `game_tick` sort tôt, donc les mines se figent avec le monde (mèche comprise).
- **Fin de match** : l'hôte ignore déjà les `EV_HIT` quand `matchOver` vaut 1. Sur `EV_MATCH` sous-type 0, chaque client supprime tous les `PR_MINE`.
- **Porteur accroupi** : `mineOfs.y` est borné à `np_height(i)` à chaque tick (la mine ne flotte pas au-dessus de la tête).

## 3. Constantes

```c
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
#define MINE_HIT_R         0.45f   /* rayon de touche du rail sur une mine */
#define MINE_STICK_GROW    0.15f   /* AABB des joueurs et rayon des drones agrandis pour coller */
#define MINE_SKIN          0.06f   /* decollement de la surface */
#define MINE_BEEP_SLOW     0.5f    /* intervalle du bip au collage */
#define MINE_BEEP_FAST     0.08f   /* intervalle du bip juste avant l'explosion */
enum { MS_NONE, MS_FLY, MS_WORLD, MS_ENEMY, MS_PLAYER };
enum { MINE_EV_THROW, MINE_EV_WORLD, MINE_EV_PLAYER, MINE_EV_BOOM, MINE_EV_TORN, MINE_EV_CANCEL };
```

Les munitions ne sont pas redéfinies ici : elles vivent dans la table de `00_infra` (`ammoDef[WP_MINE]` : jauge de 150 points par mine, 3 au maximum, 1 au spawn), avec `P.ammo[WP_MINE]` et la jauge `P.ammoGauge[WP_MINE]` (§ 1.5). **Aucune recharge passive.**

**Entrées** (règle de `CLAUDE.md` : `A_*` + `actId` + `actLabel` + `bindDefault`), ajoutées avant `A_COUNT`. Le fichier `velocity.ini` repère les actions par leur `actId`, donc l'insertion ne casse pas les réglages existants :

```c
/* enum A_* :   ..., A_SCORES, A_DETONATE_ALL, A_DETONATE_ONE, A_COUNT */
/* actId :      "detoner_tout", "detoner_une" */
/* actLabel :   "DETONER TOUT", "DETONER UNE"        (pas d'accents a l'ecran) */
/* bindDefault: { K_X2, 0x10 },  { 0x2D, 0 }          (pouce arriere + Q/A physique ; X) */
```

## 4. Données

```c
/* Proj : champs ajoutes (remis a zero par spawn_proj) */
int   mineState;     /* MS_* ; MS_FLY aussi pour une mine renvoyee (kind PR_PARRIED) */
int   mineAttach;    /* MS_ENEMY : index ennemi + 1 ; MS_PLAYER : id joueur + 1 */
int   mineDet;       /* detonation demandee pendant l'armement */
unsigned int mineSeq;/* ordre de lancer (la plus ancienne saute a la 3e) */
V3    mineOfs;       /* position relative au porteur (pieds) */
V3    mineN;         /* normale de la face collee (rendu) */
float mineT;         /* temps depuis le lancer (vol) ou depuis le collage */
float mineArmT;      /* armement restant (joueur seulement) */
float mineBeepT;     /* prochain bip */

/* Player */
float mineOnMeT;     /* > 0 : une mine adverse est collee sur moi (alerte HUD) */

/* hote : sort des mines (arbitrage, voir § 6) */
typedef struct { unsigned int id; int fate, who; } MineFate;   /* fate : 1 explosee, 2 arrachee */
static MineFate hostMine[32]; static int hostMineHead;
```

- `NetPState` : **aucun changement**. L'état « mine sur un joueur » circule par `EV_MINE` et n'utilise aucun bit `ST_*`.
- `NetEvent.netId` d'un `EV_HIT` (aujourd'hui toujours 0) porte le `netId` de la mine pour `W_MINE` et `W_SELF` causés par une mine. Le format du paquet ne change pas.
- Les mines reçoivent un `netId` **aussi en solo**, au format commun de PLAN.md § 4.3 : `(myId + 1) << 24 | PR_MINE << 20 | (++mineCounter & 0xFFFFF)`, pour que `mineSeq` et les tests soient uniformes.

## 5. Algorithme

### `explode` → `explode_ex` (paramétrée, réutilisée)

`explode` code en dur `ROCKET_KNOCK` et `W_ROCKET`/`W_PARRY`. On extrait le corps :

```c
static int explode_ex(V3 c, float radius, float dmg, int byPlayer, float knock, int weapon,
                      int skipEnemy, int skipPlayer, unsigned int netId);
static int explode(V3 c, float r, float d, int b) {
    return explode_ex(c, r, d, b, ROCKET_KNOCK, b == 2 ? W_PARRY : W_ROCKET, -1, -1, 0);
}
```

Le corps reste celui d'aujourd'hui : `knock` remplace `ROCKET_KNOCK`, `weapon` remplace le choix codé en dur, les cibles `skip*` sont sautées (elles ont déjà pris leurs dégâts garantis), et `net_hit` devient `net_hit_id(..., netId)`. `damage_player(5 * f)` passe aussi par une variante qui joint `netId` en LAN. `net_hit` reste `net_hit_id(t, d, k, w, 0)`. Quand l'Égide existera, les touches sur un joueur passeront par `hit_player` (PLAN.md § 4.4), qui devra transmettre ce `netId`.

### `fire_mine` (nouvelle, appelée par `game_tick` à la place de `fire_rocket` quand `P.weapon == WP_MINE`)

```c
static void fire_mine(void) {
    int n = 0;
    for (each PR[i] alive, kind PR_MINE, owner == myId, !mineDet) n++;
    if (n >= MINE_MAX_ACTIVE) mine_detonate(mine_oldest());   /* la 3e fait sauter la plus ancienne */
    V3 f = view_dir();
    Proj *p = spawn_proj(PR_MINE, vadd(eye_pos(), vmul(f, 0.5f)), vadd(vmul(f, MINE_SPEED), P.vel));
    if (!p) return;
    P.ammo[WP_MINE]--;                       /* la jauge P.ammoGauge[WP_MINE] est geree par l'infra */
    p->mineState = MS_FLY; p->life = MINE_FLY_LIFE; p->mineT = 0;
    p->netId = ((myId + 1) << 24) | (PR_MINE << 20) | (++mineCounter & 0xFFFFF); p->mineSeq = mineCounter;
    if (dmMode) net_fx_mine(MINE_EV_THROW, p->netId, p->pos, p->vel, 0, 0);
    P.fireCd = MINE_CD; P.recoil = 0.8f; muzzleT = 0.05f;
    play(S_MINE_THROW, 0.7f, frange(0.95f, 1.05f));
}
```

Portée indicative à l'arrêt : chute de 1,6 m en `sqrt(2 × 1,6 / 16)` = 0,45 s, donc environ 10,7 m à l'horizontale. À 20 m/s vers l'avant, la mine part à 44 m/s et porte à environ 20 m.

### `mine_tick(Proj *p, float dt)` (nouvelle ; `proj_tick` l'appelle en tête de boucle pour `PR_MINE`, puis `continue`)

```c
int mine = !dmMode || p->owner == myId;                  /* seul le poseur decide */
p->mineT += dt;
switch (p->mineState) {
case MS_FLY:
    p->vel.y -= MINE_GRAVITY * dt;
    step = p->vel * dt; t = ray_world(p->pos, dir, len, &n);
    if (mine) {
        /* drones (solo) : ray_sphere rayon enemy_radius + MINE_STICK_GROW -> MS_ENEMY, mineAttach = j+1,
           mineOfs = point - E[j].pos */
        /* joueurs (LAN) : ray_box sur np_aabb(j, MINE_STICK_GROW), j != owner -> MS_PLAYER,
           mineOfs = point - NP[j].disp, borne x,z a +-(HW+0.05), y a [0.1, np_height(j)],
           mineArmT = MINE_ARM_PLAYER ; net_fx_mine(MINE_EV_PLAYER, id, mineOfs, 0, j, 0) */
        /* decor : t < len -> MS_WORLD, pos = point + n*MINE_SKIN, mineN = n ;
           net_fx_mine(MINE_EV_WORLD, id, pos, n, 0, 0) */
        if (colle) { p->mineT = 0; p->vel = 0; play(S_MINE_STICK, ...); break; }
        if (p->mineT >= MINE_FLY_LIFE) { mine_explode(p, 0, myId); return; }
    } else if (t < len) { p->pos = point + n*MINE_SKIN; p->vel = 0; }   /* prevision visuelle, corrigee par l'evenement */
    p->pos = vadd(p->pos, step);
    break;
case MS_ENEMY:  p->pos = E[a].pos + mineOfs; if (!E[a].alive) { mineState = MS_FLY; vel = E[a].vel; } break;
case MS_PLAYER: p->pos = (a == myId ? P.pos : NP[a].disp) + mineOfs;   /* position affichee */
                p->mineArmT -= dt; break;
}
if (p->mineState != MS_FLY) {                              /* bip : plus rapide vers la fin de la meche */
    float k = clampf(p->mineT / MINE_FUSE, 0, 1);
    if ((p->mineBeepT -= dt) <= 0) { p->mineBeepT = lerp(MINE_BEEP_SLOW, MINE_BEEP_FAST, k); play(S_MINE_BEEP, ...); }
}
if (mine && p->mineState >= MS_WORLD) {
    int armed = p->mineState != MS_PLAYER || p->mineArmT <= 0;
    if (armed && (p->mineDet || p->mineT >= MINE_FUSE)) mine_explode(p, 0, myId);
}
/* chez les autres : si la meche est depassee de 1 s sans EV_MINE BOOM, suppression silencieuse */
```

### `mine_detonate(Proj *p)` et les commandes `A_DETONATE_ALL` / `A_DETONATE_ONE` (`game_tick`)

```c
/* game_tick, apres le tir ; actP[] est rempli par input_poll (touches) ou botPress (bots) */
if (!P.dead && (actP[A_DETONATE_ALL] || actP[A_DETONATE_ONE])) {
    Proj *old = mine_oldest();                    /* plus petit mineSeq, a moi, vivante, sans mineDet */
    if (!old) play(S_HIT, 0.5f, 0.45f);           /* rien a faire sauter : clic sec */
    else if (actP[A_DETONATE_ALL]) { for (mes PR_MINE vivantes sans mineDet) mine_detonate(p); }   /* ALL l'emporte si les deux */
    else mine_detonate(old);                      /* ONE : la plus ancienne seulement */
}

static Proj *mine_oldest(void);                   /* aussi utilisee par fire_mine pour la 3e mine */

static void mine_detonate(Proj *p) {
    if (p->mineState == MS_FLY && p->mineT < MINE_ARM_FLY) { p->mineDet = 1; return; }  /* explose a 0,1 s */
    if (p->mineState == MS_PLAYER && p->mineArmT > 0) { p->mineDet = 1; return; }
    mine_explode(p, 0, myId);
}
```

En vol, `mineDet` est aussi testé dans `MS_FLY` dès que `mineT >= MINE_ARM_FLY`. Une mine marquée `mineDet` sort du compte de `mine_oldest` : deux appuis rapides sur `A_DETONATE_ONE` visent donc bien deux mines différentes, même si la première attend son armement.

### `mine_explode(Proj *p, int boosted, int trigger)` (nouvelle ; dégâts calculés par **celui qui déclenche**)

```c
V3 c = p->pos;
float dmg = MINE_DMG * (boosted ? MINE_BOOST_MULT : 1), rad = boosted ? MINE_BOOST_RADIUS : MINE_RADIUS;
int se = -1, sp = -1, hits = 0;
p->alive = 0;
if (dmMode) net_fx_mine(MINE_EV_BOOM, p->netId, c, v3(boosted, 0, 0), 0, 0);
if (p->mineState == MS_ENEMY && E[a].alive) { damage_enemy(a, dmg, vmul(vnorm(-mineOfs), 12)); se = a; hits++; }
if (p->mineState == MS_PLAYER && np_target(a)) {        /* 60 garantis : pas de degressivite pour le porteur */
    V3 kd = vnorm(vadd(vmul(p->mineOfs, -1), v3(0, 0.6f, 0)));
    net_hit_id(a, dmg * PVP_SCALE, vmul(kd, MINE_KNOCK + 3), W_MINE, p->netId); sp = a; hits++;
    impact_frame(c, 0.06f); hitstop = fmaxf(hitstop, 0.05f);
}
hits += explode_ex(c, rad, dmg, 1, MINE_KNOCK, W_MINE, se, sp, p->netId);  /* souffle + recul et 5*f PV sur soi */
if (hits) shot_result(1);              /* une mine ne casse jamais la chaine (arme de preparation) */
if (boosted) { impact_frame(c, 0.08f); burst(c, or, 60, 16, 0.6f, 0.35f); }
```

`explode_ex` produit déjà les particules, `S_EXPLO`, le tremblement et le recul local. Le recul sur soi vaut `MINE_KNOCK × f + 3`. C'est le **mine-jump** (§ 7).

### `fire_rail` et `ricochet` (rail ou pièce sur une mine)

- `fire_rail` : après la boucle des pièces, tester toutes les mines `PR_MINE` vivantes avec `ray_sphere(eye, d, pos, MINE_HIT_R, tw, &t)`, quel que soit leur poseur. On exclut une mine `MS_PLAYER` **non armée**, et une mine collée **sur le tireur lui-même** (il doit la parer). La plus proche fixe `tw = t`, `mineHit = i`. Le rail s'arrête dessus (il ne touche ni le joueur ni le drone derrière), puis `mine_explode(&PR[mineHit], 1, myId)` et `hit |= touches`. `popText` « MINE x1,5 ! », ou « MINE VOLEE ! » si `owner != myId`.
- `ricochet`, étape 2 (cible la plus proche) : on ajoute comme candidates les mines visibles (`los`) à moins de `COIN_SEEK_RANGE` **qui ont une cible (drone ou adversaire) à moins de `MINE_BOOST_RADIUS`**. Si une telle mine existe, elle passe **avant** les cibles directes. Le rayon d'or va de la pièce à la mine, puis `mine_explode(m, 1, myId)`. Les dégâts du ricochet ne s'additionnent pas : l'explosion vaut 135 et ne cumule pas `COIN_MULT` (en PvP, le plafond rendrait le cumul inutile de toute façon).

### `try_parry`

```c
int foreignMine = dmMode && p->kind == PR_MINE && p->owner != myId &&
                  (p->mineState == MS_FLY || (p->mineState == MS_PLAYER && p->mineAttach == myId + 1));
if (!p->alive || (p->kind != PR_ORB && !foreignRocket && !foreignMine)) continue;
if (p->kind == PR_MINE && p->mineState == MS_PLAYER) {      /* collee sur moi : arrachee */
    p->kind = PR_PARRIED; p->mineState = MS_FLY; p->owner = myId; p->parries = 1;
    p->mult = PARRY_ROCKET_MULT; p->life = 4;
    p->pos = vadd(eye_pos(), vmul(f, 0.8f)); p->vel = vmul(f, PARRY_SPEED);   /* visee exacte, pas d'aide */
    net_fx_mine(MINE_EV_TORN, p->netId, p->pos, p->vel, myId, p->mult);
    P.mineOnMeT = 0; parry_feedback(p->pos); snprintf(popText, ..., "ARRACHEE !"); P.swingLanded = 1; parried++;
    continue;
}
/* mine en vol : chemin des roquettes inchange (portee, cone, aide a la visee, net_fx_parry) */
```

Dans `proj_tick`, la branche `PR_PARRIED` teste `p->mineState` pour exploser avec les paramètres de la mine : `explode_ex(c, MINE_RADIUS, MINE_DMG * p->mult, 2, MINE_KNOCK, W_PARRY, -1, -1, 0)`, plus `40 × mult` en direct sur un drone touché. `apply_event(EV_PARRY)` met `mineState = MS_FLY` si la mine était déjà une mine. Les **+50 points** du parry réussi sont donnés par l'infra dans `parry_feedback`. Ce bloc vit dans `try_parry(float range, float speedMult)` (signature commune, PLAN.md § 4.2) : l'arrachage ignore `range`, puisque la mine est sur toi.

## 6. Réseau

**Autorité** : le **poseur** simule le vol, le collage, la mèche et calcule les dégâts de ses mines (comme `mine` dans `proj_tick` pour les roquettes). Exception : un **rail ou une pièce** qui déclenche une mine (même celle d'un autre) fait calculer les dégâts au **tireur**, qui est crédité (`EV_KILL.actor`). Les autres clients n'ont qu'une copie visuelle.

`net_fx_mine(sub, netId, V3 a, V3 b, int target, float x)` émet un `EV_MINE` (`f[0..2] = a`, `f[3..5] = b`, `f[6] = x`) :

| `sub` | Émis par | Champs | `apply_event` |
|---|---|---|---|
| 0 `THROW` | poseur (`fire_mine`, mine qui retombe) | pos, vel | `find_proj` ou `spawn_proj(PR_MINE)`, `owner = from`, `MS_FLY` |
| 1 `WORLD` | poseur | pos, normale | `MS_WORLD`, pos corrigée, `mineT = 0` |
| 2 `PLAYER` | poseur | `target`, `a = mineOfs` | `MS_PLAYER`, `mineArmT = 0,6` ; si `target == myId` : `P.mineOnMeT = 3`, `S_MINE_STICK` fort |
| 3 `BOOM` | poseur, ou tireur du rail / de la pièce | pos, `b.x` = boosté | supprime la mine, `explode_fx` ; si elle était collée sur moi, l'effet est placé à `P.pos + mineOfs` (cohérent avec ce que je vois) ; plus gros si boostée |
| 4 `TORN` | porteur qui pare | pos, vel, `f[6]` mult | `kind = PR_PARRIED`, `MS_FLY`, `owner = from`, particules de parry |
| 5 `CANCEL` | **hôte** | `target` = celui qui avait arraché | supprime sans explosion ; « TROP TARD » si c'est moi |

**Arbitrage (`host_handle`) : « le premier qui décide du sort d'une mine gagne »**, avec la table `hostMine` (32 entrées en anneau) :

```c
case EV_MINE:
    if (sub == MINE_EV_BOOM) { if (!fate(id)) set_fate(id, 1, from); }
    if (sub == MINE_EV_TORN) {
        if (fate(id) == 1) { emit EV_MINE CANCEL (target = from, netId = id) ; apply_event ; return; }  /* deja explosee */
        set_fate(id, 2, from); parryCount++; NP[from].hp = fminf(100, NP[from].hp + PARRY_HEAL);
    }
case EV_HIT:   /* en tete, avant le parry d'epee */
    if (ev->netId && fate(ev->netId) && fate_who(ev->netId) != ev->from) return 1; /* decision perdue : annule, ni applique ni relaye (PLAN 4.1) */
```

- **Course détonation / arrachage** : si l'`EV_MINE BOOM` du poseur arrive avant l'arrachage, l'arrachage est annulé (`CANCEL`, pas de soin) et les 60 PV passent. Dans l'autre ordre, les `EV_HIT` du poseur portant ce `netId` sont ignorés. Le poseur a vu une explosion « fantôme », ce qui est acceptable pour une fenêtre de l'ordre de la latence (moins de 20 ms en LAN).
- **Course poseur / tireur de rail** : même règle, un seul des deux souffles compte.
- La mine renvoyée garde son `netId`. Ses `EV_HIT` (`W_PARRY`) viennent de celui qui l'a arrachée, `who` de la table, donc ils passent.
- **Latence d'une mine collée sur un joueur qui bouge** : la position n'a pas besoin d'être exacte pour le porteur, puisque ses dégâts sont garantis et que son recul se calcule à partir de `mineOfs`. Pour les **autres** joueurs, le souffle est calculé chez le poseur autour de `NP[a].disp + mineOfs` (« favoriser le tireur »). L'écart avec la position réelle vaut au plus l'extrapolation de 0,1 s de `net_update`, soit environ 2 m à 20 m/s. Le porteur, lui, voit la mine sur son propre corps (`P.pos + mineOfs`).
- **Armement chez la victime** : le compte à rebours de 0,6 s démarre à la réception de `PLAYER`, donc chez la victime l'armement paraît plus court d'une demi-latence. La tolérance du parry côté hôte (`MELEE_PARRY_GRACE` 0,15 s) n'est pas utilisée ici : seul l'ordre d'arrivée compte (§ 13).
- Chaque `EV_MINE` part 3 fois (`net_queue`) ; `find_proj` rend `THROW`/`WORLD` idempotents.

## 7. Interactions

- **Mine-jump** (mine à tes pieds, debout, `pcenter` à 0,9 m) : distance ≈ 0,84 m, f = 1 − 0,84 / 4 = 0,79, recul 15 × 0,79 + 3 = **14,9 m/s**, apogée 14,9² / 48 = **4,6 m**, coût 4 PV et une mine (150 points, soit 3 coups d'épée). **Rocket jump** actuel : point d'impact décalé de 0,15 m, f = 0,83, 13,8 m/s, apogée **4,0 m**, 4,2 PV. En sautant le même tick que la détonation : 8,4 + 14,9 = 23,3 m/s, soit **11,3 m** (roquette ≈ 10,3 m). L'avantage de la mine n'est pas la hauteur mais le **timing libre** : posée d'avance, déclenchée au sommet d'un wall-jump ou d'un double saut. Elle reste chère : une seule mine au spawn, et un rocket jump ne coûte qu'une roquette (jauge de 50, soit un coup d'épée).
- **Booster mural** : mine collée sur un mur de wall-run, 1,5 m derrière et 0,5 m sous la trajectoire, à 0,45 m du mur. Distance 1,64 m, f = 0,59, recul 11,9 m/s, dont **+10,9 m/s vers l'avant**, 3,6 vers le haut et 3,3 vers l'extérieur ; 3 PV. Le wall-run continue (la poussée suit le mur, la gravité reste à ×0,12). Aucun code spécifique : c'est `explode_ex`.
- **Pièces et rail** : voir § 5 (×1,5, « mine volée »). Une mine **n'arrête pas** une pièce et une pièce ne colle pas à une mine.
- **Grappin** : le crochet ignore les mines. Un porteur au grappin emmène sa mine.
- **Parry** : voir § 5. Un parry réussi donne +50 points à la jauge de l'arme en main (infra).
- **Mêlée** : chaque coup qui touche donne les points de l'arme de mêlée (épée 50, Enclume 75, Fléau 35…) à l'arme à distance en main, ou à l'autre si elle est pleine ; un kill en mêlée donne +100. Avec la Ventouse, il faut 3 coups d'épée pour une mine : on revient au contact entre deux pièges, c'est la boucle voulue.
- **Cellules** : une petite cellule (100) donne deux tiers de mine, une grande (150) une mine entière, à chacune des deux armes du loadout.
- **Autres armes** (PLAN.md § 4.5) : un plomb de l'Éventail ou l'arc de la Tesla qui touche une mine la fait exploser **×1** (`mine_explode(p, 0, tireur)`, dégâts calculés par le tireur, même arbitrage). Les explosions (roquettes, autres mines) ne déclenchent pas les mines voisines (§ 13).

## 8. Rendu et son

- **Mine** (`render`) : cube plein de 0,22 m (`box_faces(±0,11)`) passé dans la boucle des pièces (passe opaque), plus, dans la passe additive, un billboard clignotant de 0,25 m. Couleur cyan `(0,3 ; 1 ; 0,7)` pour mes mines, rouge `(1 ; 0,2 ; 0,2)` pour celles des autres. Fréquence liée à `mineBeepT`, de 2 Hz au collage à 12 Hz en fin de mèche. Disque « ventouse » : un quad aplati contre `mineN`. En vol : petite traînée (`add_part` une fois sur deux). Une mine renvoyée prend le billboard doré de `PR_PARRIED`.
- **Viewmodel** (`draw_viewmodel`, `WP_MINE`) : tube court et carré + disque en façade ; deux LED sur le dessus (allumées = mines actives, clignotantes = en vol). À la détonation, la crosse se relève (`P.recoil = 0,5`).
- **Joueurs distants** : le modèle d'arme générique de l'infra suffit. Les mines collées sur un joueur se dessinent toutes seules, puisqu'elles suivent `disp + mineOfs`.
- **HUD** (`draw_hud`) : « VENTOUSE 1/3 » à la place du nom de l'arme, avec sous le compteur la jauge de l'infra (`P.ammoGauge[WP_MINE]` / 150), deux pastilles de mines actives et, tant qu'une mine est active, les rappels « [X2] DETONER TOUT » et « [X] DETONER UNE » (noms des touches réellement liées, lus dans `binds`). La pastille de la plus ancienne mine est encadrée : c'est elle que vise `A_DETONATE_ONE`. **Alerte « mine sur toi »** si une `PR_MINE` a `mineAttach == myId + 1` : cadre rouge pulsant (4 `rect` de 10 px, alpha `0,4 + 0,4 × sin`) et texte centré « MINE SUR TOI ! PARE ! », avec une barre qui vide l'armement (0,6 s) puis la mèche. Le cadre disparaît dès l'arrachage ou l'explosion.
- **Juice** : explosion boostée = `impact_frame` 0,08 et `hitstop` 0,05 ; arrachage = `parry_feedback` complet et texte « ARRACHEE ! » ; mine volée = « MINE VOLEE ! ».
- **Sons** (`make_sounds`, ajoutés avant `S_COUNT`) :
  - `S_MINE_THROW` (0,14 s) : « thump », sinus `ph += (180 − 600 t) / SR` + bruit filtré (`lp += 0,1 × (noise − lp)`), enveloppe `expf(−t × 30)`.
  - `S_MINE_STICK` (0,08 s) : « plop » de ventouse, sinus de 700 à 180 Hz (`700 × expf(−t × 25)`), amplitude `0,5 × expf(−t × 40)`, plus un clic de bruit de 5 ms.
  - `S_MINE_BEEP` (0,05 s) : carré à 1 800 Hz (`sinf > 0 ? 1 : −1`) × 0,25, avec attaque et relâche linéaires de 5 ms. Joué à volume 0,9 si la mine est sur moi, sinon `0,5 × atten(pos)`.
  - Explosion : `S_EXPLO` existant (hauteur 0,8 et volume 1,0 si boostée).

## 9. Bots (`bot_think`)

1. **Mine sur soi** : si une mine adverse a `mineAttach == myId + 1`, on décide une fois par mine (comme `parryDecided`) avec 60 % de chances. Si oui, le bot attend un délai aléatoire de 0,15 à 0,45 s, puis `botPress[A_MELEE]` en visant le centre de l'adversaire le plus proche (la mine part dans sa direction de visée).
2. **Piège** : si la Ventouse est en main, que la cible est vue entre 6 et 20 m et que `P.fireCd <= 0`, le bot vise `np_center + np_vel × 0,4` (anticipation grossière) avec un pitch relevé de `0,02 × distance`, puis `A_FIRE`.
3. **Détonation** : `botPress[A_DETONATE_ALL]` dès qu'une de ses mines est collée sur un joueur et armée, ou qu'un adversaire passe à moins de 2,5 m d'une de ses mines collées (sauf si le bot lui-même est à moins de 2 m). Si un adversaire n'est près que de la **plus ancienne** et que l'autre mine est encore loin de toute cible, le bot appuie sur `A_DETONATE_ONE` pour garder la seconde en piège.
4. **Mine-jump** : dans le cross-up, phase 1, si `crossT > 0,4`, qu'aucune hauteur n'est gagnée et que `P.ammo[WP_MINE] >= 2` (il en garde une pour le combat), le bot lance une mine à `pitch = −1,5`, puis saute et appuie sur `A_DETONATE_ONE` au collage. Il journalise l'apogée (`netlog`).
5. **À sec** : avec 0 mine, le bot suit la règle de l'infra (il va au contact pour remplir la jauge). Il journalise le temps passé sans mine.
6. **Contre-jeu** : comme pour les pièces adverses, il tire au rail une fois sur deux dans une mine adverse collée à moins de 4 m de son poseur.

## 10. Tests (`run_tests`)

1. **Vol et collage** : depuis `(-20, 0, 40)`, `yaw = PI`, `pitch = −0,3`, à l'arrêt, `fire_mine`, puis 120 × `proj_tick(TICK)`. Attendu : `mineState == MS_WORLD`, `mineN = (0, 1, 0)`, `pos.y ≈ 0,06`, z entre 43 et 46 (calcul : vy0 = −7,1, chute de 1,45 m en ≈ 0,17 s, z ≈ 44,4), munitions 1 → 0 (valeur de spawn), jauge inchangée. Une variante tirée à 8 m d'un mur (point à choisir sur la carte) vérifie une normale horizontale. Note : à pitch 0,2, la mine n'atteint pas le mur nord (z = 78) depuis z = 40, puisque la portée maximale à l'arrêt est d'environ 36 m.
2. **Mèche** : on compte les ticks entre le collage et `alive == 0` sans détonation. Attendu : 360 ± 1.
3. **Mine-jump** : `pitch = −1,5`, `fire_mine`, ticks jusqu'au collage, `mine_detonate`, puis 200 ticks. Attendu : apogée > 4,3 (≈ 4,6), PV 96. Afficher à côté l'apogée du test « rocket jump » existant.
4. **Drone collé** : drone à `(-20, 1,6, 30)` avec 1 000 PV, lancer vers lui, détoner. Attendu : exactement 90 de dégâts (garantis, sans dégressivité) ; un 2e drone à 3 m reçoit `90 × (0,3 + 0,7 × (1 − 3 / 4,9))` ≈ 51,4.
5. **Rail sur mine** : mine `MS_WORLD` collée à 2 m d'un drone à 1 000 PV, `fire_rail` dans la mine. Attendu : `135 × (0,3 + 0,7 × (1 − 2 / 5,9))` ≈ 102,9 ; le drone derrière la mine n'est pas touché par le rail.
6. **Limite de 2** : `P.ammo[WP_MINE] = 3`, puis trois `fire_mine` vers le sol (`P.fireCd` remis à 0 entre chaque). Attendu : 2 mines actives, la 1re a explosé (`mineSeq` minimal absent), munitions 0.
7. **Deux commandes** : deux mines collées au sol (A puis B). `actP[A_DETONATE_ONE] = 1`, un `game_tick` : attendu A détruite, B vivante. Puis `actP[A_DETONATE_ALL] = 1` : B détruite. Enfin `A_DETONATE_ONE` sans mine : aucune mine, aucun dégât (clic sec). Variante : A collée sur `NP[1]` non armée, deux appuis sur `A_DETONATE_ONE` dans le même 0,1 s → A marquée `mineDet`, B explose tout de suite.
8. **Munitions par jauge** (avec l'infra) : 0 mine et jauge 0 ; trois touches à l'épée sur un drone (50 points chacune) → 1 mine, jauge 0 ; une grande cellule (150) → 2 mines ; à 3 mines, une cellule laisse la jauge pleine sans 4e mine. Aucune mine n'apparaît pendant 30 s d'attente (pas de recharge passive).
9. **LAN simulé (hôte sans socket)** : mine collée sur `NP[1]` (`MS_PLAYER`), `mine_detonate` à 0,3 s. Attendu : PV inchangés à 0,3 s, explosion à 0,6 s, `NP[1].hp == 40`.
10. **Arrachage** : mine `owner = 1`, `mineAttach = myId + 1`, `P.parryWin = 0,1`, `try_parry`. Attendu : `kind == PR_PARRIED`, `owner == myId`, |vel| = 36, `vdot(vnorm(vel), view_dir()) > 0,999`.
11. **Arbitrage** : `host_handle(EV_MINE TORN)` puis `host_handle(EV_HIT W_MINE, même netId, from = poseur)` → PV inchangés. Ordre inverse (`BOOM`, puis `HIT`, puis `TORN`) → PV 40 et `CANCEL` émis.

## 11. Équilibrage

| Cas | PvE | PvP (× 0,7) | Après plafond 60 | Touches pour tuer |
|---|---|---|---|---|
| Centre du souffle | 90 | 63 | **60** | 2 |
| Bord (30 %) | 27 | 18,9 | 18,9 | 6 |
| Collée sur un joueur | 90 garantis | 63 | **60** | 2 (collée + n'importe quoi) |
| Rail ou pièce, centre | 135 | 94,5 | **60** | 2 |
| Rail ou pièce, bord | 40,5 | 28,4 | 28,4 | 4 |
| Mine arrachée (×1,25), centre / bord | 112,5 / 33,8 | 78,8 / 23,6 | 60 / 23,6 | 2 |
| Sur soi | 5 × f (≈ 4 au mine-jump) | non plafonné (`W_SELF`) | — | — |
| Recul (centre) | 15 × f + 3, soit 18 m/s au maximum | idem | — | — |

**Économie des munitions** (jauge de 150, 3 au maximum, 1 au spawn, aucune recharge passive) :

| Source de points | Points | Mines obtenues |
|---|---|---|
| Coup d'épée qui touche | 50 | 1/3 |
| Kill à l'épée (2 coups + kill) | 50 + 50 + 100 = 200 | 1 mine et 1/3 |
| Parry réussi | 50 | 1/3 |
| Petite / grande cellule | 100 / 150 | 2/3 / 1 |

Une mine coûte 3 fois plus cher qu'une roquette (jauge de 50) pour un souffle à peine plus faible (90 contre 110), mais elle apporte la télécommande, le collage et les 60 garantis. Avec une seule mine au spawn, le premier combat se joue sur un piège ou un mine-jump, pas les deux. Si la Ventouse reste trop rarement chargée (plus de 50 % du temps à 0 mine dans le journal), descendre la jauge à 120 avant de toucher aux dégâts.

À mesurer dans le journal du bot (`netlog`, stats de l'hôte) : `hitCount[W_MINE]` ; temps passé à 0 mine et points reçus par source ; usage de `A_DETONATE_ALL` contre `A_DETONATE_ONE` ; répartition des explosions (télécommande / mèche / 3e mine / rail) ; taux de mines collées sur un joueur et taux d'arrachage ; délai moyen entre collage et détonation ; dégâts infligés à soi par minute ; apogées de mine-jump ; nombre d'annulations (`CANCEL`, et hits ignorés par la table). Signal d'alerte : si plus de 40 % des kills Ventouse passent par une mine collée non parée, monter l'armement à 0,8 s.

## 12. Étapes d'implémentation

| # | Étape | Fonctions | Estimation |
|---|---|---|---|
| 1 | Constantes, champs `Proj`, `spawn_proj` (remise à zéro), `explode_ex` + `net_hit_id` | `explode`, `net_hit`, `damage_player` | 1 h |
| 2 | `fire_mine`, `mine_tick` (vol, collage au décor, mèche), branche de `proj_tick` | `game_tick`, `proj_tick` | 1,5 h |
| 3 | `A_DETONATE_ALL` et `A_DETONATE_ONE` (`A_*`, `actId`, `actLabel`, `bindDefault`, ligne dans `draw_settings`), `mine_oldest`, `mine_detonate`, `mine_explode`, 3e mine | `input_poll`, `game_tick` | 1,25 h |
| 4 | Collage sur drone et sur joueur, armement, chute à la mort du porteur | `mine_tick` | 1,5 h |
| 5 | Rail et ricochet sur mine | `fire_rail`, `ricochet` | 1 h |
| 6 | Parry : mine en vol, arrachage | `try_parry`, `proj_tick` (`PR_PARRIED`) | 1 h |
| 7 | `EV_MINE` (6 sous-types), `net_fx_mine`, table `hostMine`, filtre d'`EV_HIT` | `apply_event`, `host_handle` | 2,5 h |
| 8 | Rendu, viewmodel, HUD et alerte, 3 sons | `render`, `draw_viewmodel`, `draw_hud`, `make_sounds` | 2 h |
| 9 | Bot | `bot_think` | 1,5 h |
| 10 | Tests 1 à 11 | `run_tests` | 1,75 h |

Total ≈ 15 h : **coût M**. Les étapes 1 à 3 donnent déjà une arme jouable en solo. Prérequis : la jauge de munitions de `00_infra` (`P.ammoGauge[]`, points de la mêlée et des cellules).

## 13. Questions ouvertes

1. **Lecture de « deux commandes séparées »** : la fiche retient deux actions de détonation (`A_DETONATE_ALL`, `A_DETONATE_ONE`), en plus du lancer sur `A_FIRE`. Une autre lecture est tout aussi naturelle, et peut-être plus : **une commande pour lancer** (`A_FIRE`) et **une commande pour détoner** (une seule action, qui fait tout sauter). Elle demande une touche de moins mais perd la détonation sélective. Laquelle voulais-tu ?
2. **Touches par défaut** : `A_DETONATE_ALL` sur pouce arrière (`K_X2`) et Q/A physique (`0x10`), `A_DETONATE_ONE` sur X (`0x2D`). Ces touches te conviennent-elles ? Elles remplacent le clic milieu proposé dans PLAN.md § 4.6, déjà pris par le grappin.
3. **3e mine** : la plus ancienne explose (proposé), disparaît sans dégâts, ou le lancer est refusé ?
4. **Mèche** : 3 s depuis le collage (proposé) ou depuis le lancer ? Une mèche plus longue sur le décor (6 à 8 s) rendrait les pièges plus crédibles.
5. **Mort du poseur** : les mines restent jusqu'à leur mèche (proposé) ou disparaissent (style TF2) ?
6. **Arbitrage** : la table `hostMine` et l'usage de `EV_HIT.netId` étendent l'arbitrage de l'hôte au-delà de PLAN.md § 3. À valider, ou plutôt traiter l'arrachage comme un parry côté hôte (`W_MINE` ajouté à la liste parable, tolérance `MELEE_PARRY_GRACE`) ?
7. **Réactions en chaîne** : une explosion (roquette, mine) doit-elle faire sauter les mines voisines ? Proposé : non. Plombs et arc : ×1, comme l'a tranché PLAN.md § 4.5.
8. **Bouclier Égide** : une mine qui touche un bouclier levé colle-t-elle au bouclier (et tombe quand il est baissé) ? (À accorder avec la fiche 05.)
9. **Chaîne de tir** : une mine qui touche compte pour `shotStreak` (proposé : oui, sans jamais la casser) ?
10. **Une mine au spawn** : avec la jauge, la Ventouse démarre à 1 mine et ne peut pas poser 2 pièges avant le premier contact. Est-ce voulu, ou faut-il 2 mines au spawn (le maximum restant 3) ?
