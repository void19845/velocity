# Fiche 10 — Arc électrique « Tesla »

> Arme à distance `WP_TESLA` (6), source de dégâts `W_TESLA` (16), événement `EV_TESLA` (18), bit d'état `ST_ZAP` (0x80).
> Source : [`../IDEES_ARMES_CARTES.md`](../IDEES_ARMES_CARTES.md) § 3.5 et § 1.5. Interfaces : [`PLAN.md`](PLAN.md) § 3.
> Loadout, `P.ammo[]`, points de mêlée et de cellules (conversion Tesla : 1 point = 0,4 de batterie), cellules génériques, clic sec et bascule automatique : **`00_infra_loadout_munitions.md`** (non redéfinis ici).
> Coût : **M** (≈ 15 h). Rien n'est implémenté.

---

## 1. Résumé validé

- **Rôle** : arme de **suivi** (tracking). Un faisceau continu tant que `A_FIRE` est tenu, qui saute de cible en cible. L'opposé du rail (précision instantanée).
- **Impulsions** : 10 par seconde (`TESLA_PULSE` 0,1 s = 12 ticks à 120 Hz). **7 PvE** par impulsion sur la cible principale.
- **Portée** : `14 + 0,3 × |P.vel|` m (20 m à 20 m/s), plafonnée à 28 m (proposition, § 13).
- **Chaîne** : 2 sauts de plus, chacun vers une cible à **≤ 6 m de la précédente**, à **60 %** (4,2 PvE), en ligne de vue.
- **Batterie (validée, § 1.5 du 27/10)** : 60 au spawn, 100 max, **−20/s en tirant** (2 par impulsion), **aucune recharge passive**. La batterie *est* la jauge de points de la Tesla : **1 point = 0,4 de batterie**. Mêlée et cellules donnent des points (00_infra) : coup d'épée 50 → **+20**, petite cellule 100 → **+40**, grande cellule 150 → **+60**, kill en mêlée +100 → **+40**, parry +50 → **+20**. Batterie pleine : les points vont à l'autre arme du loadout (règle commune).
- **Relais sur pièce** : une pièce traversée par l'arc cesse de tomber pendant 2 s et frappe toute cible à < 6 m (5 PvE par impulsion).
- **Ne compte pas** dans la chaîne de touches (`shot_result` n'est jamais appelé ; la chaîne en cours est conservée).
- **PvP** : 49 PvP/s, ≈ 2 s pour tuer en suivant parfaitement. **Un `net_hit` par cible toutes les 0,2 s** (≈ 9,8 PvP) : le plafond de 60 ne joue jamais.
- **Parry** : la cible dans l'arc pendant sa fenêtre de parry renvoie la décharge : tireur **étourdi 0,4 s** (aucun tir) et **15 PV**.
- **Réseau** : aucun événement par impulsion pour le visuel : `ST_ZAP` + `yaw`/`pitch` suffisent, chaque client recalcule l'arc.

## 2. Règles détaillées

### 2.1 Machine à états (tireur local)

| État | Condition | Effet |
|---|---|---|
| **REPOS** | `A_FIRE` relâché, ou arme ≠ Tesla | `P.teslaOn = 0`. Les relais déjà actifs continuent. Les cumuls de dégâts sont envoyés à la fin de la fenêtre. |
| **ARC** | `P.weapon == WP_TESLA`, `A_FIRE` tenu, `!P.dead`, `P.switchT <= 0`, `P.teslaStunT <= 0`, batterie > 0 | `P.teslaOn = 1` (donc `ST_ZAP`). Impulsion dès que `P.fireCd <= 0`, puis `P.fireCd = TESLA_PULSE`. |
| **VIDE** | ARC demandé, batterie à 0 | Clic sec et bascule (chemin commun de 00_infra), une seule fois par appui. |
| **ÉTOURDI** | reçu `EV_TESLA` sous-type 1 en tant que tireur | `P.teslaStunT = 0,4 s` : **aucune arme à distance** ne tire. La mêlée reste permise. Cumuls annulés. |

- La **première impulsion part au tick de l'appui** (si `fireCd <= 0`). Utiliser `P.fireCd` limite la cadence à 10/s même en tapotant le clic.
- Coût : `min(2, batterie)` par impulsion, **même si rien n'est touché** (le tir coûte, pas la touche). Avec une batterie impaire (75 après une mêlée), la dernière impulsion coûte 1.
- **Changement d'arme** : arc coupé immédiatement, cumuls envoyés (`tesla_flush`). Les relais continuent.
- **Mort** : arc coupé, cumuls envoyés (les dégâts déjà infligés comptent), **mes relais s'éteignent** (pièces détruites, `EV_COIN` sous-type 1).
- **Respawn** (`EV_RESPAWN`) : batterie = 60 (infra), `teslaStunT = 0`, cumuls et liens visuels remis à zéro.
- **Hitstop** : `game_tick` sort tôt, donc impulsions, fenêtre d'agrégation et relais sont gelés ; les arcs restent dessinés (leur durée n'est pas décrémentée).
- **Fin de match** : l'hôte ignore les `EV_HIT` (`matchOver`) ; côté client, rien de spécial.

### 2.2 Ciblage : rayon avec tolérance (pas un cône)

Un cône (même de 3°) donne ±1 m à 20 m et devient trop permissif de loin. On garde **un rayon sur la visée** et on « grossit » les cibles d'une marge fixe `TESLA_AIM_PAD = 0,4 m` (un cylindre d'aide à la visée, identique à toutes les distances) :

1. `tw = ray_world(œil, dir, portée)` : le décor arrête l'arc.
2. Cible principale = la plus proche le long du rayon avant `tw` : drones par `ray_sphere(rayon enemy_radius + 0,4)`, joueurs par `ray_box(np_aabb(i, 0,4))`. Aucune cible : l'arc finit sur le mur (ou à la portée max) et ne fait rien.
3. **Pièces** : toute pièce traversée avant la cible (`ray_sphere`, `COIN_RADIUS`) devient un relais **sans arrêter l'arc** (contrairement au rail).
4. **Chaîne** : depuis le centre de la cible précédente, la cible la plus proche à ≤ 6 m, pas encore frappée, avec `los(précédente, candidate)`. Deux fois au plus. Le plus proche gagne : c'est déterministe, ce qui aide le recalcul chez les autres clients.
5. Ligne de vue de l'œil à la cible principale : garantie par l'étape 1.

### 2.3 Relais sur pièce

- Activation : pièce non relais, de **n'importe qui** (une pièce adverse devient **mon** relais : `teslaBy = myId`), dans la limite de **3 relais par joueur** (`TESLA_RELAY_MAX`). Au-delà, la pièce est ignorée.
- Pendant 2 s : la pièce ne tombe plus (vitesse nulle), et toutes les 0,1 s elle frappe **toute cible à < 6 m en ligne de vue** (sauf son activateur) : 5 PvE.
- **Non renouvelable** : au bout de 2 s, la pièce « grille » et disparaît. Une pièce relais ne peut donc pas devenir une tourelle permanente.
- Un **rail** sur un relais déclenche le ricochet normal (la pièce est consommée) : combo « relais puis rail » possible, et contre-jeu adverse (pièce volée).
- **Pas de cumul** : dans une fenêtre de 0,2 s, une cible reçoit `max(arc, min(relais, 2 × 5))`. L'arc (principal ou chaîne) et les relais ne s'additionnent pas, et plusieurs relais ne s'empilent pas. Invariant : **jamais plus de 70 PvE/s (49 PvP/s) sur une cible**. Les relais servent au multi-cibles et à frapper hors de vue, pas à doubler les dégâts (§ 13, Q3).

### 2.4 Agrégation (PvE et PvP, même code)

- Les dégâts de chaque impulsion sont **cumulés par cible** (`teslaArcE/P[]`, `teslaRelE/P[]`) et envoyés toutes les `TESLA_AGG = 0,2 s` (24 ticks) : `damage_enemy` en PvE (donc un chiffre de dégâts toutes les 0,2 s au lieu de 10/s, ce qui préserve l'anneau `DN[32]`), `net_hit(..., W_TESLA)` en PvP.
- **Envoi anticipé** d'une cible : si le cumul tue d'après les PV connus (`E[i].hp`, ou `NP[i].hp` reçu par `PK_SNAP`), ou si la cible affiche `NF_PARRY`. Cela supprime le retard du kill (au plus 0,2 s) et fait arriver vite à l'hôte un coup qui sera paré.
- Aussi à l'arrêt de l'arc, au changement d'arme et à la mort.

## 3. Constantes

```c
/* Arc electrique "Tesla" : faisceau continu, chaine, relais sur piece */
#define TESLA_PULSE        0.1f    /* 10 impulsions par seconde */
#define TESLA_COST         2       /* batterie par impulsion : 20 par seconde */
#define TESLA_BATT_START   60      /* batterie au spawn (table des munitions de l'infra) */
#define TESLA_BATT_MAX     100
#define TESLA_PTS_NUM      2       /* points -> batterie : x 2 / 5 = 0,4 (entier : 25 pts -> 10, 35 -> 14) */
#define TESLA_PTS_DEN      5       /* aucune recharge passive : seulement melee et cellules */
#define TESLA_RANGE        14.0f   /* portee de base */
#define TESLA_RANGE_PER_MS 0.3f    /* portee en plus par m/s de vitesse */
#define TESLA_RANGE_MAX    28.0f
#define TESLA_AIM_PAD      0.4f    /* aide a la visee : les cibles grossissent de 0,4 m */
#define TESLA_DMG          7.0f    /* par impulsion, cible principale */
#define TESLA_CHAIN_N      2       /* sauts en plus */
#define TESLA_CHAIN_R      6.0f
#define TESLA_CHAIN_MULT   0.6f
#define TESLA_RELAY_T      2.0f    /* duree du relais, puis la piece grille */
#define TESLA_RELAY_R      6.0f
#define TESLA_RELAY_DMG    5.0f
#define TESLA_RELAY_MAX    3       /* relais actifs par joueur */
#define TESLA_AGG          0.2f    /* un net_hit par cible toutes les 0,2 s */
#define TESLA_KNOCK_PVE    0.3f    /* petit recul par impulsion sur les drones (aucun en PvP) */
#define TESLA_STUN         0.4f    /* tireur dont la decharge est paree : plus de tir */
#define TESLA_REFLECT_DMG  15.0f   /* PvP direct, ni multiplie ni plafonne */
#define TESLA_PARRY_GRACE  0.25f   /* tolerance de l'hote : fenetre d'agregation + latence */
```

## 4. Données

```c
/* Player */
int   teslaOn;          /* arc actif ce tick (-> ST_ZAP, son en boucle) */
float teslaStunT;       /* etourdi : aucun tir */
float teslaAggT;        /* temps restant avant l'envoi des cumuls */
int   teslaDry;         /* clic sec deja joue pour cet appui */
/* batterie : P.ammo[WP_TESLA] (infra) */

/* Coin */
float teslaT;           /* > 0 : relais actif (temps restant) */
float teslaPulse;       /* prochaine impulsion du relais */
int   teslaBy;          /* joueur qui a active le relais (myId en solo) */

/* NetPlayer */
double teslaReflT;      /* hote : derniere decharge renvoyee vers ce tireur */
double teslaHitMeT;     /* client : dernier EV_HIT W_TESLA de ce tireur sur moi (verrou visuel) */
float  teslaVisT;       /* client : prochain recalcul de son arc */

/* Etat global (section Combat) */
static float teslaArcE[MAXE], teslaRelE[MAXE], teslaArcP[MAXP], teslaRelP[MAXP];
typedef struct { int ak, ai, bk, bi; V3 bp; float life; unsigned seed; } TeslaLink;  /* extremites resolues au rendu */
enum { TK_NONE, TK_MUZZLE, TK_HAND, TK_ENEMY, TK_PLAYER, TK_COIN, TK_POINT };
#define MAXTL 48
static TeslaLink TL[MAXTL];
static int teslaVoice[MAXP];  /* voix en boucle par tireur (index + 1, 0 = aucune) */
static int teslaReflects;     /* stats hote */
```

`NetPState` ne change pas : `pad |= ST_ZAP` quand `P.teslaOn`, et `weapon = WP_TESLA`.

## 5. Algorithme

### 5.1 `tesla_range`, `tesla_trace` (nouvelles, section Combat)

```c
static float tesla_range(V3 vel) { return fminf(TESLA_RANGE + TESLA_RANGE_PER_MS * vlen(vel), TESLA_RANGE_MAX); }

typedef struct { int kind, idx; V3 at; } TeslaTgt;   /* TK_ENEMY, TK_PLAYER (id, myId = moi), TK_POINT */
static V3 tgt_center(int kind, int idx) {
    return kind == TK_ENEMY ? E[idx].pos : (idx == myId ? pcenter() : np_center(idx));
}
/* shooter : id du tireur (exclu), -1 en solo. Retourne le nombre de cibles (0 = out[0] est le point d'arret). */
static int tesla_trace(V3 eye, V3 d, float range, int shooter, TeslaTgt out[3], int coins[], int *nc) {
    float tw = ray_world(eye, d, range, 0), t;
    int n = 0; out[0].kind = TK_POINT; out[0].at = vadd(eye, vmul(d, tw));
    for (int i = 0; i < MAXE; i++)                                   /* solo : drones et kamikazes */
        if (E[i].alive && ray_sphere(eye, d, E[i].pos, enemy_radius(E[i].type) + TESLA_AIM_PAD, tw, &t)) {
            tw = t; out[0] = (TeslaTgt){ TK_ENEMY, i, E[i].pos }; n = 1; }
    for (int i = 0; dmMode && i < MAXP; i++) {                       /* LAN : moi compris si c'est un autre qui tire */
        if (i == shooter || !NP[i].active || !NP[i].alive) continue;
        V3 mn, mx;
        if (i == myId) { paabb(P.pos, pheight(), &mn, &mx); mn = vsub(mn, v3(TESLA_AIM_PAD, TESLA_AIM_PAD, TESLA_AIM_PAD)); mx = vadd(mx, v3(TESLA_AIM_PAD, TESLA_AIM_PAD, TESLA_AIM_PAD)); }
        else np_aabb(i, TESLA_AIM_PAD, &mn, &mx);
        if (ray_box(eye, d, mn, mx, tw, &t, NULL)) { tw = t; out[0] = (TeslaTgt){ TK_PLAYER, i, tgt_center(TK_PLAYER, i) }; n = 1; }
    }
    if (nc) { *nc = 0;                                               /* pieces traversees : relais, l'arc continue */
        for (int i = 0; i < MAXCOIN; i++)
            if (CO[i].alive && CO[i].teslaT <= 0 && ray_sphere(eye, d, CO[i].pos, COIN_RADIUS, tw, &t)) coins[(*nc)++] = i; }
    for (int k = 0; n && k < TESLA_CHAIN_N; k++) {                   /* chaine : la plus proche a 6 m, en vue */
        V3 from = out[n - 1].at; float best = TESLA_CHAIN_R; TeslaTgt nx = { TK_NONE };
        /* boucle sur E[] puis sur les joueurs (meme filtre que ci-dessus), en excluant out[0..n-1] :
           d = vlen(vsub(c, from)); if (d < best && los(from, c)) { best = d; nx = ... } */
        if (nx.kind == TK_NONE) break;
        out[n++] = nx;
    }
    return n;
}
```

### 5.2 `tesla_pulse`, `tesla_add`, `tesla_flush`

```c
static void tesla_add(int kind, int idx, float dmg, int relay) {
    float *a = kind == TK_ENEMY ? (relay ? &teslaRelE[idx] : &teslaArcE[idx]) : (relay ? &teslaRelP[idx] : &teslaArcP[idx]);
    *a += dmg;
    float arc = kind == TK_ENEMY ? teslaArcE[idx] : teslaArcP[idx], rel = kind == TK_ENEMY ? teslaRelE[idx] : teslaRelP[idx];
    float tot = fmaxf(arc, fminf(rel, 2 * TESLA_RELAY_DMG));
    int early = kind == TK_ENEMY ? tot >= E[idx].hp
                                 : (tot * PVP_SCALE >= NP[idx].hp || (NP[idx].st.flags & NF_PARRY));
    if (early) tesla_flush_one(kind, idx);
}
static void tesla_flush_one(int kind, int idx) {   /* envoie et remet a zero le cumul d'une cible */
    /* tot calcule comme ci-dessus ; si tot > 0 :
       PvE : damage_enemy(idx, tot, vmul(dir_vers_cible, TESLA_KNOCK_PVE * impulsions));
       PvP : if (np_target(idx)) net_hit(idx, tot * PVP_SCALE, v3(0, 0, 0), W_TESLA); */
}
static void tesla_flush(void) { /* tesla_flush_one sur toutes les cibles ; P.teslaAggT = TESLA_AGG */ }

static void tesla_pulse(void) {
    V3 eye = eye_pos(), d = view_dir();
    TeslaTgt t[3]; int coins[MAXCOIN], nc;
    int n = tesla_trace(eye, d, tesla_range(P.vel), dmMode ? myId : -1, t, coins, &nc);
    P.ammo[WP_TESLA] -= P.ammo[WP_TESLA] < TESLA_COST ? P.ammo[WP_TESLA] : TESLA_COST;
    P.fireCd = TESLA_PULSE;
    for (int k = 0; k < nc; k++) tesla_relay_start(coins[k]);
    tesla_link(TK_MUZZLE, 0, n ? t[0].kind : TK_POINT, t[0].idx, t[0].at);      /* liens visuels, vie 0,12 s */
    for (int k = 0; k < n; k++) {
        tesla_add(t[k].kind, t[k].idx, k ? TESLA_DMG * TESLA_CHAIN_MULT : TESLA_DMG, 0);
        if (k) tesla_link(t[k - 1].kind, t[k - 1].idx, t[k].kind, t[k].idx, t[k].at);
        for (int s = 0; s < 3; s++) add_part(t[k].at, vmul(rand_dir(), 5), v3(0.6f, 0.8f, 1), 0.2f, 0.07f, 0);
    }
    if (n) { hitMarker = fmaxf(hitMarker, 0.06f); play(S_TESLA_ZAP, 0.3f, frange(0.9f, 1.25f)); }
    if (n == 3 && !teslaChain3Shown) { popText "CHAINE x3"; impact_frame(t[2].at, 0.04f); teslaChain3Shown = 1; } /* une fois par appui */
    P.recoil = fmaxf(P.recoil, 0.15f); shake += 0.01f; muzzleT = 0.03f;
    if (botMode) netlog("tesla : %d cible(s), batterie %d", n, P.ammo[WP_TESLA]);
}
```

### 5.3 `tesla_relay_start`, `coin_tick` (relais)

```c
static void tesla_relay_start(int ci) {
    int mine = 0;
    for (int i = 0; i < MAXCOIN; i++) mine += CO[i].alive && CO[i].teslaT > 0 && CO[i].teslaBy == myId;
    if (mine >= TESLA_RELAY_MAX) return;
    Coin *c = &CO[ci];
    c->teslaT = TESLA_RELAY_T; c->teslaPulse = 0; c->teslaBy = dmMode ? myId : 0; c->vel = v3(0, 0, 0);
    burst(c->pos, v3(0.5f, 0.75f, 1), 20, 6, 0.3f, 0.1f); play(S_TESLA_ZAP, 0.5f, 0.7f);
    if (dmMode) net_fx_tesla(0, c->netId, c->pos, 0, 0);
}

/* coin_tick : en tete de boucle, avant la gravite */
if (c->teslaT > 0) {
    c->teslaT -= dt; c->spin += dt * 3;
    if ((c->teslaPulse -= dt) <= 0) {
        c->teslaPulse += TESLA_PULSE;
        tesla_relay_zap(i, c->teslaBy == myId);   /* degats si c'est mon relais, sinon visuel seul */
    }
    if (c->teslaT <= 0) {                         /* la piece grille */
        c->alive = 0; burst(c->pos, v3(0.6f, 0.8f, 1), 16, 5, 0.3f, 0.08f);
        if (dmMode && c->teslaBy == myId) net_fx_coin(1, c->netId, c->pos, v3(0, 0, 0));
    }
    continue;
}
/* tesla_relay_zap(ci, mine) : pour chaque cible (drones, joueurs y compris moi si ce n'est pas mon relais,
   sauf teslaBy) a < TESLA_RELAY_R avec los(coin, cible) : tesla_link(TK_COIN, ci, ...) ; si mine : tesla_add(..., TESLA_RELAY_DMG, 1). */
```

Mort du joueur : `tesla_tick` éteint mes relais (`teslaT = 0.001f`, ce qui laisse `coin_tick` les détruire proprement).

### 5.4 `tesla_tick` et `game_tick`

```c
static void tesla_tick(float dt, int held) {
    int was = P.teslaOn;
    P.teslaOn = 0;
    P.teslaStunT -= dt;
    if (!held) { P.teslaDry = 0; teslaChain3Shown = 0; }
    else if (P.teslaStunT <= 0) {
        if (P.ammo[WP_TESLA] <= 0) { if (!P.teslaDry) { P.teslaDry = 1; /* clic sec + bascule : 00_infra */ } }
        else { P.teslaOn = 1; if (P.fireCd <= 0) tesla_pulse(); }
    }
    if (P.dead) /* eteindre mes relais */;
    if ((P.teslaAggT -= dt) <= 0 || (was && !P.teslaOn)) tesla_flush();
    tesla_remote_tick(dt);   /* arcs des autres (visuel) */
    tesla_audio();           /* boucles sonores */
    for (int i = 0; i < MAXTL; i++) TL[i].life -= dt;
}

/* game_tick, bloc de tir */
int fireHeld = act[A_FIRE] || lmbp;
if (!P.dead && P.weapon != WP_TESLA && P.teslaStunT <= 0 && fireHeld && P.fireCd <= 0 && P.switchT <= 0) { /* tirs existants */ }
tesla_tick(dt, !P.dead && P.weapon == WP_TESLA && fireHeld && P.switchT <= 0);
```

`pack_state` : `s->pad |= P.teslaOn ? ST_ZAP : 0;` (après le `meleeKind` de l'infra).

## 6. Réseau

| Quoi | Qui calcule | Comment |
|---|---|---|
| Dégâts de l'arc, de la chaîne, des relais | **tireur** (positions affichées `NP[i].disp`) | cumul, `net_hit(i, total × 0,7, 0, W_TESLA)` toutes les 0,2 s par cible |
| PV, kills, plafond | hôte (`host_handle`) | inchangé ; le plafond ne joue jamais (≤ 9,8 par `net_hit`) |
| Parry (renvoi) | **hôte** | voir ci-dessous |
| Visuel de l'arc d'un autre | chaque client | `ST_ZAP` + `yaw`/`pitch`/`vel` → `tesla_trace` local |
| Relais | activateur | `EV_TESLA` sous-type 0 ; fin par minuterie + `EV_COIN` sous-type 1 |

**`EV_TESLA`** (18), émetteur `net_fx_tesla(int sub, unsigned netId, V3 pos, int actor, int target)` (déclaration anticipée en fin de section « etat partage ») :

- **sous-type 0, relais** (émis par l'activateur) : `netId` de la pièce, `f[0..2]` position. `apply_event` : retrouve la pièce par `netId`, `pos = f`, `vel = 0`, `teslaT = TESLA_RELAY_T`, `teslaBy = from`. Conflit (deux activations simultanées) : **le plus petit id gagne** (`if (c->teslaT > 0 && c->teslaBy < from) break;`), sans arbitrage de l'hôte ; un double comptage pendant au plus une impulsion est acceptable.
- **sous-type 1, décharge renvoyée** (décidée par l'hôte, `net_emit` puis `apply_event`) : `actor` = pareur, `target` = tireur, `f[0..2]` position du pareur.
  - pareur (`actor == myId`) : `parry_feedback(f)`, +50 points du parry (00_infra, soit +20 de batterie si la Tesla est en main), texte « DECHARGE RENVOYEE ».
  - tireur (`target == myId`) : `teslaStunT = TESLA_STUN`, `teslaOn = 0`, cumuls **annulés** (pas envoyés), `hitstop = max(hitstop, 0.08)`, `shake += 0.3`, `impact_frame(œil, 0.05)`, texte « SURCHARGE ! », `play(S_TESLA_ZAP, 0.9, 0.55)`.
  - tous : lien visuel doré pareur → tireur (0,25 s), étincelles.

**Parry dans `host_handle`** (branche `EV_HIT`, après le test de la cible morte, avant le plafond) :

```c
if (ev->sub == W_TESLA) {
    int s = ev->from;
    if (s < MAXP && nowT - NP[s].teslaReflT < TESLA_STUN) return;      /* coups deja en vol apres un renvoi : jetes */
    if (nowT - NP[v].lastParryT < TESLA_PARRY_GRACE) {
        NP[s].teslaReflT = nowT; teslaReflects++;
        NP[v].hp = fminf(100, NP[v].hp + PARRY_HEAL);
        NetEvent e = ev_new(EV_TESLA);
        e.sub = 1; e.actor = (unsigned char)v; e.target = (unsigned char)s;
        vf(e.f, v == myId ? pcenter() : np_center(v));
        net_emit(&e); apply_event(&e);
        if (NP[s].alive && (NP[s].hp -= TESLA_REFLECT_DMG) <= 0) host_kill(v, s, W_TESLA);
        return;
    }
}
```

- **Pourquoi l'hôte et pas la victime** : c'est la règle commune (PLAN § 3), et le coup existe déjà sous forme d'`EV_HIT`. Une détection chez la victime l'obligerait à recalculer l'arc adverse (visuel, donc divergent) pour décider d'un effet de jeu. `lastParryT` vient de `NF_PARRY` (clients) et de `host_tick` (hôte).
- **Tolérance 0,25 s** au lieu de 0,15 : un coup peut rester jusqu'à 0,2 s dans le cumul. L'envoi anticipé sur `NF_PARRY` réduit ce délai, mais `NF_PARRY` arrive lui-même avec la latence.
- Tous les `W_TESLA` sont parables : arc, chaîne **et relais** (« la décharge remonte la pièce », § 13 Q5).

**Visuel des autres et divergence** (`tesla_remote_tick`) : pour chaque `i` avec `NP[i].st.pad & ST_ZAP` et `weapon == WP_TESLA`, toutes les 0,1 s (`teslaVisT`) : œil = `disp + (0, 1,6 ou 0,95, 0)`, direction = `yaw`/`pitch` comme `view_dir`, portée = `tesla_range(np_vel(i))`, `tesla_trace(..., shooter = i, coins = NULL)`, puis des liens `TK_HAND(i)` → cibles.

- **Divergence acceptée** : purement visuelle. Le tireur valide sur ses positions affichées, l'observateur dessine avec les siennes. L'écart vaut au plus environ 0,1 s de mouvement plus le lissage, soit une cible « limite » au bord du cylindre de 0,4 m. Au pire, l'arc est dessiné sur un voisin de chaîne ou dans le vide pendant 0,1 s.
- **Verrou victime** : à la réception d'un `EV_HIT` `W_TESLA` de `i` sur moi, `NP[i].teslaHitMeT = nowT`. Pendant 0,3 s, si je suis à portée de `i`, l'arc dessiné finit **sur moi**. La personne qui perd des PV voit toujours l'arc qui la touche.
- **Débit** : aucun paquet de plus pour l'arc. `EV_HIT` : 5/s par cible touchée, × 3 renvois, soit au plus 45 petits événements/s avec la chaîne à 3 cibles (`OUTQ` 128, chaque événement y reste 3 envois). `NET_VER` 4 est déjà pris en charge par l'infra.

## 7. Interactions

- **Parry** : voir § 6. Côté pareur, aucune différence avec un parry d'épée (même fenêtre `PARRY_WINDOW`, `MELEE_WHIFF_CD` si raté). Un parry « à l'aveugle » contre un arc de 10/s est facile à tenter : le coût est la fenêtre courte et la recharge d'un coup dans le vide.
- **Pièces** : relais (§ 2.3). Le rail consomme un relais par ricochet. L'Éventail fait éclater une pièce relais (06). Le Fléau qui frappe une pièce relais la relance (03) et le relais s'arrête.
- **Grappin** : l'arc reste utilisable pendant la traction. Accroché à un joueur à 46 m/s, la portée vaut 27,8 m : la Tesla « verrouille » la cible pendant l'approche. Le grappin ne déclenche pas de relais.
- **Mêlée** : les points de chaque arme de mêlée (§ 1.5) deviennent de la batterie à 0,4 : épée 50 → +20, Enclume 75 → +30, joute 100 → +40, estoc 50 → +20, Fléau 35 → +14, Poings 25 par coup (50 au plus par combo) → +10 (+20 par combo), coup de bouclier 50 → +20, parry parfait de l'Égide 100 → +40 ; kill en mêlée +100 → +40, parry +50 → +20. Plafond 100, puis le surplus va à l'autre arme. Conversion dans le code commun de 00_infra : `P.ammo[WP_TESLA] = min(TESLA_BATT_MAX, P.ammo[WP_TESLA] + pts * TESLA_PTS_NUM / TESLA_PTS_DEN)` (tous les montants sont des multiples de 2,5 points, donc la batterie reste entière). C'est la boucle voulue : batterie vide → contact → arc.
- **Chaîne de touches** : ni augmentée ni remise à zéro.
- **Solo, orbes** : l'arc **grille les orbes** (`PR_ORB`) qu'il traverse (`ray_sphere` 0,5 m, sans s'arrêter, `burst` violet). C'est une utilité défensive contre les drones (§ 13 Q7). Pas d'effet sur les roquettes adverses en LAN (il faudrait un événement).
- **Autres armes** (à confirmer avec les fiches concernées) : Égide (05) : le bouclier levé de face **arrête l'arc** et consomme sa jauge de la valeur PvP cumulée ; la Tesla teste `ST_SHIELD` + `yaw` dans `tesla_trace`. Ventouse (09) : l'arc ne fait pas sauter les mines. Lance couchée, piqué de l'Enclume : aucune interaction.

## 8. Rendu et son

**Arcs (`draw_tesla_arcs`, appelée dans `render` juste après la boucle des `BM`, en mode additif)** : pas de `add_beam` pour l'arc lui-même, car un arc à 3 maillons × 6 segments, plus les relais, saturerait l'anneau `BM[32]` en un dixième de seconde et effacerait les rayons de rail. On dessine des `TeslaLink`, dont les extrémités sont **résolues à chaque image** (`muzzle()` local, main du joueur distant, `E[i].pos`, `np_center`, `pcenter`, `CO[i].pos`). L'arc suit donc la visée et la cible entre deux impulsions.

```c
static unsigned arcRng;                    /* aleatoire du rendu : ne touche pas rng (determinisme des tests) */
static float arc_rand(void) { arcRng = arcRng * 1664525u + 1013904223u; return (float)(arcRng >> 8) / 16777216.0f * 2 - 1; }
static void draw_arc(V3 a, V3 b, unsigned seed, float alpha) {
    V3 d = vsub(b, a); float L = vlen(d); if (L < 0.05f) return;
    V3 f = vmul(d, 1 / L), s1 = vnorm(vcross(f, fabsf(f.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0))), s2 = vcross(f, s1);
    int N = L / 0.7f < 4 ? 4 : (L / 0.7f > 28 ? 28 : (int)(L / 0.7f));
    float amp = fminf(0.12f + 0.03f * L, 0.6f);
    arcRng = seed + (unsigned)(nowT * 30);   /* forme stable 1/30 s : lisible, et scintille quand meme */
    V3 p[29];
    for (int k = 0; k <= N; k++) { float t = (float)k / N, e = sinf(PI * t) * amp;   /* extremites fixes */
        p[k] = vadd(vadd(a, vmul(d, t)), vadd(vmul(s1, arc_rand() * e), vmul(s2, arc_rand() * e))); }
    glLineWidth(6); glColor4f(0.45f, 0.65f, 1.0f, 0.3f * alpha);  /* halo */  /* GL_LINE_STRIP sur p[0..N] */
    glLineWidth(2); glColor4f(0.9f, 0.95f, 1.0f, alpha);         /* coeur */
    /* une branche sur trois : 3 segments de 0,2 a 0,4 m depuis p[k] au hasard, alpha 0,5 */
}
```

Lueur `billboard(texDot)` de 0,6 m aux points d'impact et au canon. Arc dans le vide : `alpha` 0,5, `amp` doublée. Les relais tournent sur eux-mêmes (`spin` × 3) et ont une lueur bleue au lieu du reflet doré.

- **Viewmodel** (`draw_viewmodel`, branche `WP_TESLA`) : corps (boîte 0,08 × 0,08 × 0,45, bleu nuit), 3 bagues de bobine (boîtes fines claires), **deux fourches** au bout (±0,035 m). Au repos, une petite étincelle entre les fourches toutes les 0,5 s ; en tir, lueur constante et tremblement (`swayX` ± 0,003). **5 témoins** sur le flanc (1 par tranche de 20 de batterie), rouges sous 20. Étourdi : fourches rouges, fumée (`add_part` gris).
- **Joueurs distants** (`draw_remote_players`) : canon couleur (0,5 ; 0,8 ; 1,0) avec deux fourches ; avec `ST_ZAP`, lueur au bout (passe additive). La « main » vaut `disp + rot_yaw(0,42 ; h − 0,6 ; −0,1) + dir × 0,7` (helper `np_hand(i)`, réutilisé par `TK_HAND`).
- **HUD** (`draw_hud`, bloc « arme ») : `TESLA 58/100`. La barre de recharge devient une **barre de batterie** (200 px, cyan, graduée tous les 20 = 1 s de tir), qui clignote en rouge sous 20. « BATTERIE VIDE » à 0, barre rouge et « SURCHARGE » pendant l'étourdissement. Petit texte `PORTEE 19 M`, qui se met à jour avec la vitesse. **Viseur** : anneau cyan si `tesla_trace` (aperçu sans dégâts, sur le modèle du test des pièces) trouve une cible à portée, gris sinon. Les icônes de pièces passent au bleu pour les relais actifs.
- **Impact frames, hitstop** : aucun par impulsion (10/s figerait le jeu). Kill normal (`kill_enemy`, `EV_KILL`). « CHAINE x3 » : impact frame de 0,04 une fois par appui. Renvoi : hitstop et impact frame (§ 6).
- **Son continu** : `play` ne joue que des sons ponctuels. On ajoute des **voix en boucle** au mixeur. `audio_fill` tourne dans le thread principal (`audio_update` à chaque image, pas de callback), donc aucun verrou n'est nécessaire.

```c
typedef struct { int snd, active, loop; float pos, step, vol, fade; } Voice;   /* + loop, fade */
static int play_loop(int s, float vol, float pitch);   /* comme play, loop = 1 ; renvoie index + 1 */
static void loop_set(int v, float vol, float pitch);   /* volume (atten) et hauteur, a chaque tick */
static void loop_stop(int v);                          /* loop = 0, fade = vol / (0.03 * SR) : pas de clic */
/* audio_fill : if (p >= len - 1) { if (vo->loop) vo->pos -= len - 1; else { vo->active = 0; continue; } }
                if (vo->fade > 0 && (vo->vol -= vo->fade) <= 0) { vo->active = 0; continue; }
   play : ne vole jamais une voix loop (on saute les voix loop dans la recherche). */
```

`tesla_audio()` : `teslaVoice[myId]` démarre quand `P.teslaOn` passe à 1 et s'arrête quand il retombe à 0. Même chose pour chaque joueur distant avec `ST_ZAP` (`loop_set(v, 0.5 × atten(disp), 1)`). La hauteur monte légèrement quand l'arc touche (`1,0` → `1,08`).

Sons ajoutés avant `S_COUNT`, synthèse dans `make_sounds` :

- `S_TESLA_LOOP` (0,5 s, **bouclable**) : bourdonnement à 120 Hz (dents de scie : somme des harmoniques 120, 240 et 360 Hz, soit des nombres entiers de cycles en 0,5 s, donc sans clic au raccord) × 0,25, plus un crépitement : bruit passe-haut haché par des fenêtres aléatoires de 2 à 6 ms (probabilité 15 % par tranche de 5 ms) × 0,35. Aucune enveloppe.
- `S_TESLA_ZAP` (0,08 s) : bruit passe-haut + sinus à 1 800 Hz, enveloppe `exp(−t × 60)`. Joué à chaque impulsion qui touche (vol 0,3, hauteur 0,9 à 1,25), à l'activation d'un relais (hauteur 0,7) et à la surcharge (hauteur 0,55, vol 0,9).

## 9. Bots (`bot_think`)

- **Tir** : si `P.weapon == WP_TESLA`, que la batterie est > 0, `see`, `seeT > 0,3` et `bd < tesla_range(P.vel) − 1` : `botHeld[A_FIRE] = 1` **en continu**, visée `aimYaw + aimErrYaw` (l'imprécision sinusoïdale existante simule un suivi imparfait : c'est exactement ce que l'arme doit tester). Hors portée : on ferme la distance (dash et grappin existants). Le bot ne relâche pas le clic entre deux impulsions.
- **Relais** : si `see`, `bd < 12` et `P.coins >= 1` : pièce lancée vers la cible, puis l'arc la traverse naturellement (2,5 s entre deux pièces, comme `nextCoin`).
- **Batterie** : `seek` d'une cellule si la batterie est < 10 et que personne n'est à moins de 25 m (même test que `railAmmo < 2`). Sinon, au contact, l'épée existante recharge (+20 par coup, +40 par kill).
- **Parry de l'arc** : si un adversaire a `ST_ZAP`, qu'il est à portée et que `dot(sa visée, direction vers moi) > 0,98` : une fois toutes les 1,5 s, 35 % de chances d'appuyer sur `A_MELEE` après 0,25 s de réaction (`netlog("parry tesla tente ...")`).
- **Choix d'arme** : la Tesla de 8 à 20 m, l'autre arme du loadout au-delà ou à batterie vide (à étendre dans la ligne « le bot client alterne », selon le loadout de l'infra).

## 10. Tests (`run_tests`)

Couloir x = −20 ; `tesla_tick(TICK, held)` appelé directement (sans `enemy_tick`, pour que les drones ne bougent pas) ; `hitstop = 0` entre les blocs.

| # | Scénario | Attendu |
|---|---|---|
| 1 | `tesla_range` à 0, 20 et 60 m/s | 14, 20, 28 |
| 2 | Batterie 60, tir tenu 120 ticks dans le vide | 40, et 10 impulsions (compteur) |
| 3 | Tir tenu 4 s avec 60 de batterie, puis 5 s sans tirer | 0 → bascule d'arme (`P.weapon != WP_TESLA`) ; **toujours 0** après 5 s (aucune recharge passive) |
| 4 | Drone (100 PV) à 10 m, immobile, 120 ticks | PV 30 (10 impulsions × 7), 5 chiffres de dégâts au plus |
| 5 | Même drone, tir jusqu'au kill | tué à la 15e impulsion, 1,4 s (± 1 tick), sans attendre la fenêtre (envoi anticipé) |
| 6 | Drone à 16 m : `P.vel = 0` puis `P.vel = (0, 0, −10)` (portée 17) | 0 dégât puis 70 |
| 7 | Chaîne : drones à 10, 15, 20 et 27 m dans l'axe, 120 ticks | 70 / 42 / 42 / 0 |
| 8 | Chaîne bloquée : boîte temporaire (`boxes[nb++]`, puis `nb--`) entre les drones 2 et 3 | 70 / 42 / 0 |
| 9 | Relais : pièce à 8 m dans l'axe, drone à 4 m de la pièce hors de l'axe, 1 s | `teslaT > 0`, `pos.y` inchangée, drone −50 ; après 2,1 s, pièce morte |
| 10 | Pas de cumul : drone dans l'arc **et** à 3 m d'un relais, 1 s | −70 (et pas −120) |
| 11 | Plafond de relais : 4 pièces traversées | 3 relais actifs |
| 12 | Chaîne de touches : `shotStreak = 3`, 1 s de Tesla sur un drone | 3 |
| 13 | LAN simulé : `NP[1]` à 10 m, 120 ticks | `hitCount[W_TESLA]` = 5, `NP[1].hp` = 51 |
| 14 | Parry : `NP[1].lastParryT = nowT`, `EV_HIT` `W_TESLA` de 0 vers 1 | `NP[1].hp` 100, `NP[0].hp` 85, `P.teslaStunT` 0,4, aucun tir pendant 48 ticks ; 2e `EV_HIT` dans les 0,4 s ignoré (`NP[0].hp` toujours 85) |
| 15 | Points en main Tesla : batterie 0 + coup d'épée (50) ; + kill (100) ; + parry (50) ; + grande cellule (150) ; puis batterie 90 + petite cellule (100) | 20 ; 60 ; 80 ; 100 (plafond) ; 100 (surplus vers l'autre arme) |
| 16 | Recalcul : `NP[1]` simule mon tir (`st` copiée de `pack_state`) ; `tesla_trace(shooter = 1)` contre ma propre trace | même cible |
| 17 | Déterminisme : `rng` identique avant et après `draw_arc` (appel hors contexte GL exclu : on teste `arc_rand` seul) | `rng` inchangé |

## 11. Équilibrage

| Source | PvE / impulsion | PvE/s | PvP / impulsion | PvP/s | Par `net_hit` (0,2 s) |
|---|---|---|---|---|---|
| Arc principal | 7 | 70 | 4,9 | 49 | 9,8 |
| Chaîne (×0,6) | 4,2 | 42 | 2,94 | 29,4 | 5,9 |
| Relais (au plus un par cible) | 5 | 50 | 3,5 | 35 | 7 ; 70 PvP max sur 2 s |
| Renvoi (parry) | — | — | 15 direct + 0,4 s sans tir | — | — |

- **Temps pour tuer (PvP, suivi parfait)** : 100 PV → 21 impulsions = **2,0 s** ; 70 PV (après `KILL_HEAL`) → 15 impulsions = 1,4 s. Rail : 2 touches en 0,85 s. La Tesla est plus lente mais pardonne beaucoup plus.
- **Batterie** : un kill PvP coûte 42 (21 impulsions), soit **105 points**. Au spawn (60) : 1 kill et un peu plus ; plein (100) : 2 kills ; petite cellule (+40) ≈ 1 kill, grande cellule (+60) ≈ 1,4 kill. Aucune recharge passive : sans contact ni cellule, la batterie ne remonte jamais.
- **Recharger un kill par la mêlée** (42 de batterie) :

  | Mêlée | Batterie par touche | Touches pour 42 |
  |---|---|---|
  | Épée (50) | +20 | **3** (ou 2 coups + 1 parry) |
  | Enclume (75) | +30 | 2 |
  | Lance : joute (100) / estoc (50) | +40 / +20 | 2 (joute + n'importe quoi) / 3 |
  | Fléau (35) | +14 | 3 (exactement 42) |
  | Poings (25 par coup, 50 au plus par combo) | +10, soit +20 par combo | 3 combos |
  | Égide : coup (50) / parry parfait (100) | +20 / +40 | 3 / 2 |
  | Kill en mêlée | +40 en plus des touches | Kill à l'épée (2 coups + kill = 200 points) : **+80**, soit environ 2 kills Tesla |

  Comparaison : le même kill à l'épée rapporte 200 points, donc 0,8 tir de rail (jauge de 250) ou 4 roquettes. La Tesla recharge un peu plus vite que le rail : c'est cohérent avec ses dégâts par seconde plus faibles. À surveiller au journal : si le bot reste trop souvent à sec, la Tesla est trop chère. **PvE** : drone 30, kamikaze 16 (8 impulsions). La chaîne rend la Tesla excellente contre les essaims : c'est voulu, la batterie la bride.
- Le plafond de 60 ne joue jamais, et l'invariant « ≤ 49 PvP/s par cible » tient avec les relais.
- **À mesurer dans le journal du bot** (match avant / après) : `touches arbitrees` pour `W_TESLA` et dégât moyen par `net_hit` (attendu ≈ 9,8 en suivi, moins en cas de décrochage) ; **taux d'accroche** = impulsions qui touchent / impulsions tirées (cible : 50 à 65 % pour le bot) ; batterie dépensée par kill ; temps pour tuer moyen des kills `W_TESLA` (≈ 2 à 3 s visés) ; `teslaReflects` ; part de la batterie rechargée par la mêlée et par les cellules.

## 12. Étapes d'implémentation

Après 00_infra (`P.ammo`, `WP_*`, `W_*`, `EV_*`, octet `pad`).

1. Constantes, champs, `W_TESLA` dans `weaponNames` (« TESLA ») — 0,5 h.
2. `tesla_range`, `tesla_trace`, `tesla_pulse`, agrégation PvE, `tesla_tick` dans `game_tick` ; tests 1 à 8 et 12 — 3 h.
3. Relais (`tesla_relay_start`, branche de `coin_tick`, `tesla_relay_zap`) ; tests 9 à 11 — 2 h.
4. PvP : agrégation `net_hit`, envoi anticipé, `ST_ZAP` dans `pack_state`, `EV_TESLA` (`net_fx_tesla`, `apply_event`) ; test 13 — 2 h.
5. Parry dans `host_handle`, étourdissement ; test 14 — 1,5 h.
6. Rendu : `TeslaLink`, `draw_arc`, viewmodel, joueurs distants, `tesla_remote_tick` + verrou ; `--shot` (scène `tesla` à ajouter) — 3 h.
7. Mixeur en boucle (`play_loop`, `loop_set`, `loop_stop`), `S_TESLA_LOOP`, `S_TESLA_ZAP` — 1,5 h.
8. HUD (batterie, portée, viseur) — 1 h.
9. Bots, puis match bot contre bot et relevés du § 11 — 1,5 h.

## 13. Questions ouvertes

1. **Portée** : vitesse totale (`vlen`, proposé) ou horizontale (`hlen`, comme l'Élan) ? Plafond à 28 m (grappin à 46 m/s), ou aucun ?
2. **Aide à la visée** : un cylindre de 0,4 m (proposé), le même en PvE et en PvP ? Ou 0,6 m contre les drones ?
3. **Relais et arc** : pas de cumul (proposé, invariant de 49 PvP/s), ou cumul (12 PvE/impulsion, environ 84 PvP/s, TTK 1,2 s) ?
4. **Pièce adverse** : l'arc peut-il la transformer en relais pour soi (proposé, contre-jeu comme la « pièce volée »), ou seulement ses propres pièces ?
5. **Relais parables** (proposé : oui, tout `W_TESLA` l'est) ? Un renvoi qui étourdit le tireur à 25 m de son relais peut surprendre.
6. **Relais** : plafond de 3 par joueur et non renouvelable (la pièce grille après 2 s) : d'accord ?
7. **Solo** : l'arc grille-t-il les orbes des drones (proposé) ? Et les drones devraient-ils avoir une parade spécifique (un drone « isolant ») ?
8. **Égide / Ventouse** : le bouclier arrête-t-il l'arc ? L'arc fait-il sauter les mines (proposé : non) ? À trancher avec les fiches 05 et 09.
9. **Tolérance du parry** : 0,25 s (proposé, à cause de la fenêtre d'agrégation) ou 0,15 s comme l'épée ?
10. **Étourdissement** : bloque-t-il seulement le tir (proposé) ou aussi la mêlée et le grappin ?
11. **Recul** : aucun en PvP (proposé), ou un léger ralentissement de la cible (effet « électrocuté ») ?
