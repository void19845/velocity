# 06 — Fusil à pompe « Éventail »

> Fiche de design, **rien n'est implémenté**. Source : [`../IDEES_ARMES_CARTES.md`](../IDEES_ARMES_CARTES.md) § 1.2, § 1.5 et § 3.1. Interfaces figées : [`PLAN.md`](PLAN.md) § 3. Loadout, `P.ammo[]` et `P.ammoGauge[]` (jauges de points), points donnés par la mêlée, cellules de 100 et 150 points, et bascule à vide : fiche `00_infra_loadout_munitions.md` (référencée ici, jamais redéfinie).
> Identifiants utilisés : `WP_SHOTGUN 2`, `W_SHOTGUN 12`, `EV_SHOT 15`. Coût visé : **S**.

---

## 1. Résumé validé

- **Rôle** : le gros coup à bout portant qui manque à l'arsenal (rail = distance, roquettes = souffle). Hitscan, 10 plombs, **non parable**.
- **Chiffres validés** : 10 plombs × 8 PvE, **cône de 9° d'angle total (demi-angle 4,5°, décision du 27/10, option C)**, dégâts pleins jusqu'à 10 m puis décroissance linéaire jusqu'à ×0,3 à 30 m, recharge 0,95 s (÷ `streak_rate()`).
- **Munitions (§ 1.5, décision du 27/10)** : **3 cartouches au spawn, 6 au maximum**, **aucune recharge passive**. Une jauge de **100 points** = 1 cartouche (le surplus est gardé ; à 6 cartouches, la jauge reste pleine). Points gagnés : mêlée qui touche (épée 50, Enclume 75, Lance 100/50, Fléau 35, Poings 25 et 50 au plus par combo, Égide 50/100), +100 par kill en mêlée, +50 par parry réussi, cellule 100 (petite) ou 150 (grande). Mécanique commune de l'infra (`P.ammo[]`, `P.ammoGauge[]`), non redéfinie ici.
- **Agrégation obligatoire** : un seul `net_hit` par joueur touché et par tir (plombs **et** éclats de pièce cumulés).
- **Chaîne de tir** : le tir compte comme touche si **≥ 3 plombs** touchent (une ou plusieurs cibles), ou si un éclat de pièce touche quelqu'un.
- **Recul** : en l'air, 7 m/s à l'opposé de la visée (« shotgun jump » en tirant vers le bas), avec garde-fous (§ 2.3).
- **Slide** : dispersion divisée par 2 (demi-angle 2,25°).
- **Résultat (PvP)** : 56 à 5 m (2 touches), 37 à 10 m (3 touches), 8,4 à 20 m. En slide : 56 jusqu'à 10 m, 24 à 20 m (§ 11).
- **Pièces** : un plomb qui touche une pièce la fait **éclater** : souffle de 3 m, 45 PvE / 31,5 PvP au centre, ni dégâts ni recul sur soi.
- **Réseau** : un seul `EV_SHOT` (origine, direction, graine) ; chaque client recalcule les 10 traceurs avec un **générateur local**.

## 2. Règles détaillées

### 2.1 Machine à états (arme en main = `WP_SHOTGUN`)

| État | Condition d'entrée | Durée | Sortie |
|---|---|---|---|
| PRÊT | `P.fireCd <= 0 && P.switchT <= 0` | — | `A_FIRE` tenu ou appuyé → TIR |
| TIR | 1 tick : `fire_shotgun()` | instantané | → POMPE |
| POMPE | `P.fireCd = SG_CD / streak_rate()` | 0,95 s (0,68 s à chaîne 8) | `fireCd <= 0` → PRÊT |
| VIDE | `P.ammo[WP_SHOTGUN] == 0` au tir | — | clic sec + bascule sur l'autre arme du loadout (infra) |

- Animation de pompe : `P.sgPumpMax = P.fireCd` au tir, phase `k = 1 − P.sgPumpT / P.sgPumpMax` ; la pompe recule entre k = 0,35 et 0,85, son `S_SG_PUMP` à k = 0,35. Échelle sur la recharge réelle, donc cohérente avec la chaîne.
- **Tir automatique** : comme le rail, maintenir `A_FIRE` retire dès que PRÊT.

### 2.2 Cas limites

- **Changement d'arme** : `P.fireCd` est partagé entre les armes (comportement actuel du rail et des roquettes). On le **garde** : il empêche le « quick-swap » rail (60) + pompe (56) en 0,22 s, qui tuerait instantanément. `P.sgPumpT` continue de décroître, l'animation reprend où elle en est.
- **Mort** : aucun tir si `P.dead`. Au respawn (`EV_RESPAWN` / `reset_game`) : 3 cartouches et jauge à 0 (infra), `P.sgPumpT = 0`, `shotStreak = 0` (déjà fait).
- **Chargeur vide** : clic sec et bascule sur l'autre arme si elle a des munitions ; sinon l'arme reste en main, vide, et seules la mêlée (coups, kills, parries) et les cellules remplissent la jauge (pas de recharge passive). Un tir à 0 ne consomme rien et ne touche pas à la chaîne.
- **Hitstop** : `game_tick` sort avant le tir ; l'appui reste dans `act[A_FIRE]` et part au premier tick libre. Le hitstop du pompe lui-même (≤ 0,05 s) n'empêche donc pas un second tir.
- **Fin de match** : le tir et les effets restent ; l'hôte ignore les `EV_HIT` (`matchOver`), rien à ajouter.
- **Grappin** : tir autorisé pendant la traction (le recul s'ajoute, la traction le plafonne).
- **Tir au contact** : `ray_box` renvoie t = 0 si l'œil est dans l'AABB agrandie de la cible : les 10 plombs touchent. `ray_sphere` gère aussi l'origine dans la sphère d'un drone.
- **Portée maximale** : `SG_RANGE` 60 m. Au-delà de 30 m le multiplicateur reste à 0,3.

### 2.3 Recul et garde-fous (shotgun jump)

Impulsion `imp = −view_dir() × SG_RECOIL` (7 m/s), appliquée **seulement si `!P.grounded`**. Deux plafonds, sur le modèle du dash (`max(DASH_SPEED, vitesse + 3)`) : le recul peut **freiner** ou **pousser jusqu'à un seuil**, jamais au-delà de la vitesse qu'on avait déjà.

- Vertical : si `imp.y > 0`, `vy` après ≤ `max(vy avant, SG_RECOIL_VMAX = 9)`.
- Horizontal : `hlen` après ≤ `max(hlen avant, SG_RECOIL_HCAP = 16)`.

| Situation | Avant | Après | Commentaire |
|---|---|---|---|
| Sommet d'un saut, visée −90° | vy = 0 | vy = 7 | +1,02 m de hauteur (7² / 48) : le « petit saut » |
| Juste après un bhop | vy = 8,4 | vy = 9 | +0,6 m/s : pas de cumul saut + recul |
| En chute | vy = −10 | vy = −3 | « freinage » d'atterrissage, utile après un piqué |
| Tir vers l'arrière, à 5 m/s | hlen 5 | hlen 12 | relance du mouvement |
| Tir vers l'arrière, après un dash à 20 m/s | hlen 20 | hlen 20 | aucun gain : le dash ne se cumule pas |
| Tir vers l'avant, à 20 m/s | hlen 20 | hlen 13 | le recul freine (choix assumé) |

Pourquoi ça suffit : la gravité retire 24 × 0,95 = 22,8 m/s entre deux tirs, contre +7 au mieux : **pas de vol plané**. Le plafond de 16 m/s horizontal rend le tir arrière en bhop inutile au-delà de la vitesse de course boostée. Enfin, chaque shotgun jump coûte une cartouche sur 6 au maximum, sans recharge passive : 100 points, soit deux coups d'épée.

## 3. Constantes

```c
/* Fusil a pompe "Eventail" : 10 plombs, plein jusqu'a 10 m, x0.3 a 30 m */
#define SG_PELLETS        10
#define SG_HALF_ANGLE     0.0785f /* demi-angle du cone en radians (4.5 deg, cone total 9 deg) */
#define SG_SLIDE_SPREAD   0.5f    /* en slide : dispersion divisee par 2 */
#define SG_PELLET_DMG     8.0f    /* degats PvE par plomb */
#define SG_NEAR           10.0f   /* degats pleins jusqu'ici */
#define SG_FAR            30.0f   /* ... puis lineaire jusqu'a SG_FAR_MULT ici */
#define SG_FAR_MULT       0.3f
#define SG_RANGE          60.0f   /* portee max d'un plomb */
#define SG_CD             0.95f   /* recharge (divisee par streak_rate) */
#define SG_STREAK_MIN     3       /* plombs touches pour compter dans la chaine */
#define SG_PELLET_KNOCK   0.6f    /* recul par plomb sur la cible */
#define SG_RECOIL         7.0f    /* recul du tireur en l'air */
#define SG_RECOIL_VMAX    9.0f    /* le recul ne pousse pas vy au-dela (sauf si deja plus) */
#define SG_RECOIL_HCAP    16.0f   /* idem pour la vitesse horizontale */
#define SG_COIN_RADIUS    0.55f   /* zone de touche d'une piece pour un plomb (rail : 0.9) */
#define SG_BURST_RADIUS   3.0f    /* piece eclatee : souffle */
#define SG_BURST_DMG      45.0f   /* au centre, PvE (x PVP_SCALE contre un joueur) */
#define SG_BURST_KNOCK    8.0f
#define SG_BURST_CHAIN    3       /* une piece eclatee fait eclater les pieces voisines (profondeur max) */
#define SG_TRACER_SPEED   260.0f  /* vitesse visuelle d'un traceur */
#define MAXTR             64      /* traceurs de plombs */
```

Munitions : **dans le tableau de l'infra**, pas de `#define` ici. Valeurs de l'Éventail : jauge **100 points** par cartouche, **6** au maximum, **3** au spawn, **aucune recharge passive**.

## 4. Données

```c
/* Player */
float sgPumpT, sgPumpMax;          /* animation de pompe : temps restant / duree totale */
int   sgLastPellets; float sgLastT;/* HUD : "7/10" apres un tir qui touche */
/* NetPlayer */
float sgFireT;                     /* recul visuel du fusil d'un joueur distant */
/* globales (section Combat) */
static unsigned int sgCounter;     /* compteur de graines */
static int sgBursts;               /* pieces eclatees au dernier tir (tests, journal) */
typedef struct { V3 a, b, col; float t, life; } Tracer;
static Tracer TR[MAXTR]; static int trNext;
/* stats du journal (hote et tireur) */
static int sgShots, sgPelletSum, sgStreakShots, sgJumps;
```

Aucun champ ajouté à `NetPState`, `Proj`, `Coin` : l'arme tenue passe déjà par `NetPState.weapon` (`WP_SHOTGUN`), le slide par `NF_SLIDE`. Aucun bit `ST_*` n'est nécessaire.

## 5. Algorithme

### 5.1 Générateur local et motif de plombs (section Maths / Combat)

Le `rng` global est consommé par `frand`, `burst`, `rand_dir`, `pick_spawn`… à des rythmes différents sur chaque machine : il est **interdit** pour les plombs. Générateur local, même LCG que `frand` mais sur un état passé en paramètre :

```c
static float sg_rand(unsigned int *s) { *s = *s * 1664525u + 1013904223u; return (*s >> 8) * (1.0f / 16777216.0f); }
static unsigned int sg_new_seed(void) {                     /* unique, deterministe, sans toucher rng */
    return (((unsigned int)myId + 1) << 24) ^ ((++sgCounter) * 2654435761u);
}
/* motif semi-fixe : 1 plomb au centre, 3 sur un anneau a 0.45, 6 sur un anneau a 0.9, tourne par la graine */
static void sg_pellet_dirs(V3 d, float half, unsigned int seed, V3 *out) {
    unsigned int s = seed;
    V3 r = vnorm(vcross(d, v3(0, 1, 0.01f))), u = vcross(r, d);   /* meme astuce que fire_rail : jamais nul */
    float rot = sg_rand(&s) * 2 * PI, th = tanf(half);
    for (int k = 0; k < SG_PELLETS; k++) {
        float rho, ang;
        if (k == 0) { rho = 0; ang = 0; }
        else if (k < 4) { rho = 0.45f; ang = rot + (k - 1) * (2 * PI / 3); }
        else { rho = 0.9f; ang = rot + PI / 6 + (k - 4) * (PI / 3); }
        rho += (sg_rand(&s) - 0.5f) * 0.2f;                    /* +-0.1 */
        ang += (sg_rand(&s) - 0.5f) * 0.35f;                   /* +-10 deg */
        if (rho > 1) rho = 1;
        out[k] = vnorm(vadd(d, vadd(vmul(r, cosf(ang) * rho * th), vmul(u, sinf(ang) * rho * th))));
    }
}
```

Le motif semi-fixe (plutôt qu'un tirage uniforme dans le disque) rend le nombre de plombs touchés **prévisible** : c'est ce qui donne un « 2 touches » fiable à bout portant. Même exécutable partout (MSVC x64, SSE2) : mêmes flottants, mêmes directions.

### 5.2 `sg_falloff(dist)`

```c
static float sg_falloff(float t) {
    if (t <= SG_NEAR) return 1;
    if (t >= SG_FAR) return SG_FAR_MULT;
    return 1 - (1 - SG_FAR_MULT) * (t - SG_NEAR) / (SG_FAR - SG_NEAR);
}
```

### 5.3 `fire_shotgun()` (nouvelle, section Combat, après `fire_rail`)

```c
typedef struct { float e[MAXE], p[MAXP]; V3 ke[MAXE], kp[MAXP]; int burstHit; } SgAcc;

static void fire_shotgun(void) {
    P.ammo[WP_SHOTGUN]--;
    V3 eye = eye_pos(), d = view_dir(), dir[SG_PELLETS], end[SG_PELLETS], nrm[SG_PELLETS];
    float half = SG_HALF_ANGLE * (P.sliding ? SG_SLIDE_SPREAD : 1), mult = streak_mult();
    unsigned int seed = sg_new_seed();
    static SgAcc A; memset(&A, 0, sizeof A);
    int pellets = 0, burstList[SG_PELLETS], nBurst = 0;
    sg_pellet_dirs(d, half, seed, dir);
    for (int k = 0; k < SG_PELLETS; k++) {
        float tw = ray_world(eye, dir[k], SG_RANGE, &nrm[k]), t;
        int ci = -1, te = -1, tp = -1;
        for (int i = 0; i < MAXCOIN; i++)                   /* toute piece, la mienne ou non */
            if (CO[i].alive && ray_sphere(eye, dir[k], CO[i].pos, SG_COIN_RADIUS, tw, &t)) { tw = t; ci = i; }
        for (int i = 0; i < MAXE; i++)                       /* le plomb s'arrete sur le premier corps */
            if (E[i].alive && ray_sphere(eye, dir[k], E[i].pos, enemy_radius(E[i].type), tw, &t)) { tw = t; te = i; ci = -1; }
        for (int i = 0; i < MAXP; i++) {
            V3 mn, mx; if (!np_target(i)) continue;
            np_aabb(i, 0.05f, &mn, &mx);
            if (ray_box(eye, dir[k], mn, mx, tw, &t, NULL) && t < tw) { tw = t; tp = i; te = ci = -1; }
        }
        end[k] = vadd(eye, vmul(dir[k], tw));
        float dmg = SG_PELLET_DMG * sg_falloff(tw) * mult;
        if (te >= 0) { A.e[te] += dmg; A.ke[te] = vadd(A.ke[te], vmul(dir[k], SG_PELLET_KNOCK)); pellets++; }
        else if (tp >= 0) { A.p[tp] += dmg; A.kp[tp] = vadd(A.kp[tp], vmul(dir[k], SG_PELLET_KNOCK)); pellets++; }
        else if (ci >= 0) burstList[nBurst++] = ci;          /* eclate apres la boucle (une fois par piece) */
    }
    sgBursts = 0;
    for (int b = 0; b < nBurst; b++) sg_coin_burst(burstList[b], 0, &A);   /* ignore une piece deja morte */
    for (int i = 0; i < MAXE; i++) if (A.e[i] > 0) damage_enemy(i, A.e[i], A.ke[i]);   /* 1 chiffre par cible */
    for (int i = 0; i < MAXP; i++) if (A.p[i] > 0)                                     /* AGREGATION : 1 net_hit */
        net_hit(i, A.p[i] * PVP_SCALE, vadd(A.kp[i], v3(0, 2, 0)), W_SHOTGUN);
    int hit = pellets >= SG_STREAK_MIN || A.burstHit;
    shot_result(hit);
    sgShots++; sgPelletSum += pellets; sgStreakShots += hit;
    if (botMode) netlog("tir pompe : %d plomb(s), %d eclat(s), %s", pellets, sgBursts, hit ? "touche" : "rate");
    if (!P.grounded) sg_recoil(d);
    sg_fx_local(dir, end, nrm, pellets);                     /* traceurs, etincelles, son, juice (§ 8) */
    if (dmMode) net_fx_shot(eye, d, half, seed);
    P.fireCd = P.sgPumpMax = P.sgPumpT = SG_CD / streak_rate();
    P.recoil = 1.6f; shake += 0.1f + 0.012f * pellets; muzzleT = 0.09f;
    P.sgLastPellets = pellets; P.sgLastT = pellets ? 0.5f : 0;
}
```

Ordre de priorité d'un plomb : le plus proche gagne entre pièce, drone et joueur (`tw` rétrécit à chaque test, les autres indices sont remis à −1).

### 5.4 `sg_coin_burst(ci, depth, A)` (nouvelle)

```c
static void sg_coin_burst(int ci, int depth, SgAcc *A) {
    Coin *c = &CO[ci];
    if (!c->alive) return;
    V3 o = c->pos; c->alive = 0; sgBursts++;
    if (dmMode) net_fx_coin(1, c->netId, o, v3(1, 0, 0));    /* f[3] = 1 : detruite EN ECLAT (Question 3) */
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive) continue;
        V3 dv = vsub(E[i].pos, o); float dd = vlen(dv), R = SG_BURST_RADIUS + enemy_radius(E[i].type);
        if (dd < R) { float f = 1 - dd / R; A->e[i] += SG_BURST_DMG * (0.3f + 0.7f * f);
                      A->ke[i] = vadd(A->ke[i], vmul(vnorm(dv), SG_BURST_KNOCK * f)); A->burstHit = 1; }
    }
    for (int i = 0; i < MAXP; i++) {
        if (!np_target(i)) continue;
        V3 dv = vsub(np_center(i), o); float dd = vlen(dv), R = SG_BURST_RADIUS + 0.5f;
        if (dd < R) { float f = 1 - dd / R; A->p[i] += SG_BURST_DMG * (0.3f + 0.7f * f);
                      A->kp[i] = vadd(A->kp[i], vmul(vnorm(dv), SG_BURST_KNOCK * f + 2)); A->burstHit = 1; }
    }
    /* soi : ni degats ni recul (comme explode byPlayer == 2) */
    sg_burst_fx(o);                                          /* § 8 */
    for (int j = 0; depth < SG_BURST_CHAIN && j < MAXCOIN; j++)   /* reaction en chaine */
        if (CO[j].alive && vlen(vsub(CO[j].pos, o)) < SG_BURST_RADIUS && los(o, CO[j].pos))
            sg_coin_burst(j, depth + 1, A);
}
```

On n'appelle **pas** `explode` : il émet un `net_hit` par cible (`W_ROCKET`) et gère le recul sur soi. L'éclat n'est pas multiplié par la chaîne (45 fixes) et la pièce adverse éclate de la même façon : son propriétaire, souvent tout près, est pris dans le souffle (contre-jeu des pièces volées).

### 5.5 `sg_recoil(d)` (nouvelle)

```c
static void sg_recoil(V3 d) {
    V3 v0 = P.vel, v = vadd(P.vel, vmul(d, -SG_RECOIL));
    if (v.y > v0.y && v.y > fmaxf(v0.y, SG_RECOIL_VMAX)) v.y = fmaxf(v0.y, SG_RECOIL_VMAX);
    float h0 = hlen(v0), h = hlen(v), lim = fmaxf(h0, SG_RECOIL_HCAP);
    if (h > lim) { v.x *= lim / h; v.z *= lim / h; }
    if (d.y < -0.7f) sgJumps++;                              /* stat : shotgun jump */
    P.vel = v;
}
```

### 5.6 `game_tick`

- Branche de tir (après la sélection de l'infra) : `if (P.weapon == WP_SHOTGUN) { if (P.ammo[WP_SHOTGUN] > 0) fire_shotgun(); else <bascule a vide de l'infra>; }`.
- Pompe : `float pv = P.sgPumpT; P.sgPumpT -= dt; if (P.weapon == WP_SHOTGUN && pv > 0.65f * P.sgPumpMax && P.sgPumpT <= 0.65f * P.sgPumpMax) play(S_SG_PUMP, 0.5f, 1);` (k = 0,35).
- `P.sgLastT -= dt;`, `for TR : TR[i].t += dt;`, `for NP : NP[i].sgFireT -= dt;`.

## 6. Réseau

| Qui | Quoi |
|---|---|
| Tireur | Calcule tous les plombs et éclats sur `NP[i].disp` (« favoriser le tireur »), **un** `net_hit(i, total × PVP_SCALE, recul, W_SHOTGUN)` par joueur, **un** `EV_SHOT`, un `EV_COIN` sous-type 1 par pièce éclatée |
| Hôte (`host_handle`) | Rien de neuf : `W_SHOTGUN` plafonné à 60, **absent** de la liste des coups parables, `hitCount[W_SHOTGUN]++` |
| Victime | Applique le recul cumulé reçu dans `EV_HIT` (chemin existant) |
| Autres | Recalculent les 10 traceurs à partir de `EV_SHOT` |

**`EV_SHOT` (15)** — émis par `net_fx_shot`, déclaré en avance avec les autres `net_fx_*` :

| Champ | Contenu |
|---|---|
| `f[0..2]` | origine = œil du tireur |
| `f[3..5]` | direction de visée `view_dir()` |
| `f[6]` | demi-angle utilisé (encode le slide : 0,0785 ou 0,0393) |
| `netId` | graine (32 bits) |
| `sub` | nombre de plombs (10 ; prévoit un réglage futur) |
| `actor` | chaîne de tir plafonnée à 8 (teinte des traceurs) |

```c
case EV_SHOT: {                                             /* apply_event */
    V3 dir[SG_PELLETS], n; sg_pellet_dirs(b, ev->f[6], ev->netId, dir);
    for (int k = 0; k < SG_PELLETS && k < ev->sub; k++) {
        float tw = ray_world(a, dir[k], SG_RANGE, &n), t; V3 mn, mx;
        for (int i = 0; i < MAXP; i++) {                    /* arret visuel sur les corps affiches */
            if (i == ev->from || !NP[i].active || !NP[i].alive) continue;
            if (i == myId) { if (P.dead) continue; paabb(P.pos, pheight(), &mn, &mx); } else np_aabb(i, 0.05f, &mn, &mx);
            if (ray_box(a, dir[k], mn, mx, tw, &t, NULL) && t < tw) tw = t;
        }
        add_tracer(vadd(a, vmul(b, 0.7f)), vadd(a, vmul(dir[k], tw)), sg_tint(ev->actor / 8.0f));
    }
    if (ev->from < MAXP) NP[ev->from].sgFireT = 0.12f;
    play(S_SG_FIRE, 0.75f * atten(a), 1.0f);  break;
}
```

- **Coût** : 1 `NetEvent` (44 octets) × 3 envois, contre 10 `EV_RAIL` (30 envois) — et `OUTQ` (128) n'est pas saturé.
- **Tolérance** : identique au rail (le tireur décide sur `disp`, extrapolé ≤ 0,1 s). Les traceurs distants peuvent s'arrêter un peu à côté du vrai point d'impact : purement visuel.
- `EV_SHOT` est ajouté par l'infra (`NET_VER` 4, une seule fois). Aucun nouveau paquet.

## 7. Interactions

- **Parry** : hitscan, **non parable** (ni `try_parry`, ni `host_handle`). L'épée paraît inutile contre lui : c'est voulu, le contre-jeu est la distance.
- **Égide (05)** : la fiche du bouclier définit le test frontal côté tireur. Pour l'Éventail, le test se fait **par plomb** avant cumul ; la touche bloquée est une seule touche agrégée (elle consomme `min(total PvP, 60)` de jauge).
- **Pièces** : plomb → éclat (§ 5.4). Rail → ricochet inchangé. Pièce frappée par le Fléau (sous-type 2) : un plomb la fait éclater en vol, comme une pièce normale. Tesla (10) : sans lien.
- **Ventouse (09)** : un plomb qui touche une mine adverse la fait-il sauter ? Hors périmètre, voir Questions.
- **Grappin** : le recul s'ajoute pendant la traction. Combo grappin sur joueur → pompe à l'arrivée = 56 + impact du grappin : kill en 0,1 s, mais les 56 ne tombent qu'à moins de ~6 m (37 à 10 m), donc à portée d'épée.
- **Mêlée** : les points de mêlée (infra) vont à l'arme en main, ou à l'autre si elle est pleine. Avec l'épée : 2 coups qui touchent = 1 cartouche ; un kill à l'épée (2 coups + bonus de kill) = 200 points = **2 cartouches = un kill au pompe à moins de ~6 m** (à 10 m, il en faut 3). Boucle visée : 3 cartouches au spawn = 1 kill de près et 1 cartouche de réserve (ou 1 kill à 10 m, sans réserve), puis il faut aller au contact.
- **Roquettes** : rocket jump puis pompe en piqué. Le recul vertical est plafonné, pas de cumul exploitable.

## 8. Rendu et son

- **Traceurs** (`add_tracer`, pool `TR`, dessinés dans `render` dans la passe additive juste après `BM`) : tête `h = a + dir × min(len, t × SG_TRACER_SPEED)`, queue 2,5 m derrière. Ligne de 4 px alpha 0,3 + cœur 1,5 px blanc. Après l'arrivée, ligne entière à alpha 0,15 qui s'éteint en 0,12 s. Couleur : cuivre `(1, 0.6, 0.25)` vers or avec la chaîne. On **n'utilise pas `add_beam`** : `MAXBM` vaut 32 et 10 rayons de 7 px par tir effaceraient les rails.
- **Impacts** (`sg_fx_local`) : 3 `add_part` par plomb sur le décor (le long de `nrm[k]`, gravité 12), `burst` rouge-orangé de 12 particules sur chaque cible touchée (une fois par cible).
- **Juice** : ≥ 8 plombs sur une même cible à < 6 m → `hitstop = fmaxf(hitstop, 0.05f)` et `impact_frame(cible, 0.05f)`. Kill au pompe → impact frame 0,07 (déjà fait par `kill_enemy` / `EV_KILL`). `hitMarker = 0.18f` si touche ; son `S_HIT` avec une hauteur de `0.9 + 0.03 × plombs`.
- **Éclat de pièce** (`sg_burst_fx`, aussi appelé par `apply_event` quand `EV_COIN` sous-type 1 porte `f[3] = 1`) : `burst` or (40 particules, vitesse 10), `shards(o, or, 6, 7)`, `play(S_EXPLO, 0.6, 1.7)`, `impact_frame(o, 0.05)`, `hitstop 0.04` chez le tireur, `popText` « ÉCLAT ! » (« ÉCLAT x2 » en chaîne).
- **Viewmodel** (`draw_viewmodel`, branche `WP_SHOTGUN`, coordonnées dans le repère déjà mis à l'échelle ×0,5) :
  - carcasse `(-0.06,-0.06,-0.30)-(0.06,0.05,0.10)` brun sombre `(0.30,0.24,0.20)` ; poignée `(-0.03,-0.17,0)-(0.03,-0.06,0.07)` ;
  - deux canons côte à côte `(±0.005…±0.05, -0.005…0.04, -0.62…-0.30)` gris acier ;
  - pompe `(-0.045,-0.06,-0.52)-(0.045,-0.015,-0.36)` décalée en z de `+0.12 × sinf(PI × clampf((k − 0.35) / 0.5, 0, 1))` ;
  - sans éclairage : bande cuivre sur le dessus × `ready`, et **6 témoins** de cartouches sur le flanc gauche (cubes de 0,018, allumés jusqu'à `P.ammo[WP_SHOTGUN]`). Le témoin suivant se remplit en hauteur selon `P.ammoGauge[WP_SHOTGUN] / 100` : on voit la mêlée recharger le fusil ;
  - flash : `tip = -0.64f`, taille ×1,5, couleur `(1, 0.75, 0.35)`. `P.recoil = 1.6` donne un kick plus fort que le rail via le `glRotatef(P.recoil * 7)` existant.
- **Joueurs distants** (`draw_remote_players`) : si `st.weapon == WP_SHOTGUN`, boîte plus courte et plus large `(-0.08,-0.07,-0.55)-(0.08,0.07,0.05)` cuivre ; recul visuel `glTranslatef(0, 0, 0.15f × NP[i].sgFireT / 0.12f)`.
- **HUD** (`draw_hud`) : nom « EVENTAIL 3/6 » (`wnBuf`, police sans accents) avec, sous le compteur, la barre de jauge commune de l'infra (0 à 100 points) ; `cdMax = P.sgPumpMax`. À 0 cartouche, le nom passe au rouge et affiche « EVENTAIL 0/6 - MELEE ! » pendant 1 s. **Cercle de dispersion** autour du viseur, 24 segments en `GL_LINE_LOOP`, rayon `tanf(half) / tanf(vfov / 2) × winH / 2` avec `vfov = 2 × atanf(tanf(hfov / 2) / aspect)`. En 1600 × 900 avec un champ de vision de 105° : rayon ≈ 0,0785 / 0,732 × 450 ≈ **48 px**, et **24 px** en slide (récompense lisible). Le rayon suit `fovCur` (il rétrécit un peu quand le champ de vision s'élargit avec la vitesse). Après un tir qui touche, « 7/10 » en petit sous le viseur pendant `P.sgLastT`.
- **Sons** (`make_sounds`, ajoutés avant `S_COUNT`) :
  - `S_SG_FIRE` (0,5 s) : bruit passe-bas dont la coupure descend de 6 000 à 300 Hz (`a = 1 − expf(−2π·fc/SR)`), × 1,8 × `expf(−t·9)` ; plus un coup grave `sinf(2π·(70 − 40t)·t) × 0,9 × expf(−t·10)` ; plus un claquement `noise() × 0,6 × expf(−t·60)`.
  - `S_SG_PUMP` (0,25 s) : deux clics à t = 0 et t = 0,12 (`noise × expf(−dt·400) × 0,5 + sinf(2π·1800·dt) × 0,3 × expf(−dt·300)`), plus un toc `sinf(2π·220·t) × 0,2 × expf(−t·30)`.

## 9. Bots (`bot_think`)

1. **Choix d'arme** (remplace la ligne « le bot client alterne rail / roquettes », sur le loadout de l'infra) : si le loadout contient `WP_SHOTGUN`, `P.ammo[WP_SHOTGUN] > 0` et `bd < 10` → Éventail (3 touches jusqu'à ~11 m avec le cône de 9°) ; sinon l'autre arme. À vide des deux, la logique « va au contact » de l'infra.
2. **Tir** : visée sur `np_center`, tir seulement si `bd < 11`, ou `bd < 13` en slide (au-delà, moins de 4 plombs et la chaîne casse). Si le bot est au sol à plus de 6,5 m/s et à moins de 13 m, il maintient `A_CROUCH` pour tirer **en slide** (56 PvP jusqu'à ~11 m).
3. **Shotgun jump** (simple) : si `seekHigh` (cellule en hauteur), en l'air, `P.vel.y < 1`, `P.fireCd <= 0` **et `P.ammo[WP_SHOTGUN] >= 3`** (il ne gaspille pas ses dernières cartouches) → `P.pitch = -1.45f`, `botHeld[A_FIRE] = 1` (une fois par séjour en l'air).
4. (Option) **Grenade de poche** : à `bd < 12`, lancer une pièce, puis tirer dessus dès qu'elle est à moins de 2,5 m de la cible.

## 10. Tests (`run_tests`, couloir x = −20)

| # | Scénario | Mesure | Attendu |
|---|---|---|---|
| 1 | `sg_pellet_dirs` deux fois avec la même graine, puis une autre graine | égalité bit à bit ; différence | 1, 1 |
| 2 | `rng` avant/après `sg_pellet_dirs` | global inchangé | 1 |
| 3 | Angle max entre `d` et les 10 plombs, debout / en slide | degrés | ≤ 4,5 / ≤ 2,25 (`rho` plafonné à 1) |
| 4 | Drone à 5 m (`(-20,1.6,35)`, tireur en `(-20,0,40)`) | plombs, PV | 10, 20 |
| 5 | Drone à 20 m (gerbe de 1,57 m de rayon, drone de 0,9 m) | plombs, PV | 4 (centre + anneau intérieur), ≈ 79 (100 − 4 × 8 × 0,65) |
| 5 b | Drone à 10 m | plombs, PV | 10, 20 (tout touche jusqu'à ~11 m) |
| 6 | LAN simulé : `NP[1]` à 3 m, `hitCount[W_SHOTGUN]` avant/après | nb d'`EV_HIT`, PV | 1, 44 |
| 6 b | Idem avec `NP[1]` à 10 m | nb d'`EV_HIT`, PV | 1, 61 à 66 (6 à 7 plombs ; 63 en moyenne) |
| 7 | Idem avec `shotStreak = 8` | PV | 40 (80 × 2,2 × 0,7 = 123 → plafond 60) |
| 8 | `shotStreak = 2`, `NP[1]` à 30 m (1 plomb attendu) | `shotStreak` | 0 (moins de 3 plombs) |
| 8 b | `shotStreak = 2`, `NP[1]` à 10 m (6 à 7 plombs) | `shotStreak` ; PV | 3 ; ≈ 100 − 37 × 1,3 = 52 |
| 9 | Recul : en l'air, vy = 0, pitch = −π/2 | vy | 7,0 |
| 10 | Recul : vy = 8,4 ; puis hlen 20 tir arrière ; puis au sol | vy ; hlen ; vel | 9,0 ; 20,0 ; inchangée |
| 11 | Pièce en `(-20,1.6,36)`, drone à 2 m d'elle, tireur à 2 m de la pièce | PV drone, `P.hp` | 100 − 28,8 ; 100 |
| 12 | Deux pièces à 2 m l'une de l'autre, un plomb sur la première | `sgBursts` | 2 |
| 13 | `apply_event` d'un `EV_SHOT` forgé (`from = 1`) | traceurs vivants dans `TR` | 10 |
| 14 | Respawn, puis 3 tirs et un 4ᵉ | `P.ammo[WP_SHOTGUN]` au spawn ; après ; arme | 3 ; 0 ; bascule (infra) |
| 15 | À 0 cartouche, attendre 10 s (`game_tick`) | `P.ammo`, `P.ammoGauge` | 0, 0 (aucune recharge passive) |
| 16 | À 0 cartouche, Éventail en main : 2 coups d'épée qui touchent un drone | cartouches, jauge | 1, 0 |
| 17 | À 5 cartouches : grande cellule (150 points) | cartouches, jauge | 6, 100 (pleine, figée) |

Calcul du test 11 : `R = 3 + 0,9 = 3,9`, `f = 1 − 2 / 3,9 = 0,487`, `45 × (0,3 + 0,7 × 0,487) = 28,8`.

## 11. Équilibrage

Cône retenu : demi-angle **4,5°** (2,25° en slide). Plombs attendus sur un joueur (AABB 0,9 × 1,9 m), motif 1 + 3 + 6, visée au centre, moyenne sur la rotation aléatoire, sans chaîne. Les touches pour tuer valent aussi les **cartouches** pour tuer (100 points de jauge chacune) :

| Distance | Rayon de la gerbe | Mult. | Plombs | PvE | **PvP** | Touches (cartouches) | Slide : plombs / PvP | Slide : touches |
|---|---|---|---|---|---|---|---|---|
| ≤ 5 m | ≤ 0,39 m | 1 | 10 | 80 | **56** | **2** (200 pts) | 10 / 56 | 2 |
| 10 m | 0,79 m | 1 | 6,6 | 53 | **37** | **3** (300 pts) | 10 / 56 | 2 |
| 15 m | 1,18 m | 0,825 | 2,9 | 19 | **13,4** | 8 | 7,9 / 36 | 3 |
| 20 m | 1,57 m | 0,65 | 2,3 | 12 | **8,4** | 12 | 6,6 / 24 | 5 |
| 30 m | 2,36 m | 0,3 | 1 | 2,4 | **1,7** | — | 2,9 / 4,9 | — |

- **Portées utiles (PvP, 100 PV)** : 2 touches jusqu'à **~6 m** (~11 m en slide), 3 touches jusqu'à ~11 m, 4 touches jusqu'à ~13 m ; au-delà de ~14 m, il faut 6 touches ou plus : arme d'appoint. C'est voulu : le cône de 9° fait de l'Éventail une arme de contact, plus tolérante à la visée qu'un cône de 6°, et le slide en double la portée efficace.
- **Chaîne** : +15 % par touche ; à chaîne ≥ 1, le bout portant est plafonné à 60. La chaîne ne change donc que la cadence en PvP (0,95 → 0,68 s à 8).
- **Éclat de pièce** : 45 PvE / 31,5 PvP au centre, 13,5 / 9,5 au bord. À portée quelconque (le plomb atteint la pièce), c'est l'outil de l'Éventail contre la distance : 4 pièces / 8 s, cumul plafonné à 60 par tir.
- **Solo** : drone (r = 0,9) touché par les 10 plombs jusqu'à ~11 m → 1 drone = 2 tirs ; à 20 m, 4 plombs seulement (≈ 21 PvE). 1 kamikaze (50 PV, r = 0,75) = 1 tir jusqu'à ~10 m.
- **Économie de munitions** (jauge de 100, 3 au spawn, 6 au maximum, sans recharge passive) :

  | Source | Points | Cartouches |
  |---|---|---|
  | Coup d'épée qui touche | 50 | 0,5 |
  | Kill à l'épée (2 coups + bonus) | 200 | 2 |
  | Parry réussi | 50 | 0,5 |
  | Petite / grande cellule | 100 / 150 | 1 / 1,5 |
  | Kill au pompe à moins de ~6 m (PvP, 2 × 56) | coût 200 | −2 |
  | Kill au pompe à 10 m (PvP, 3 × 37) | coût 300 | −3 |

  Un kill au pompe de près coûte donc exactement un kill à l'épée. Les 3 cartouches du spawn suffisent pour un kill de près (avec une de réserve, ou un shotgun jump), ou pour un seul kill à 10 m. Le rail (jauge 250 pour 60 PvP) rapporte 0,24 PvP par point. L'Éventail en rapporte 0,56 à 5 m, 0,37 à 10 m et 0,08 à 20 m : il n'est rentable **que de près**, et la règle de munitions renforce le contre-jeu de la distance.
- **À mesurer dans le journal du bot** : `hitCount[W_SHOTGUN]`, plombs moyens par tir (`sgPelletSum / sgShots`), % de tirs qui comptent pour la chaîne (`sgStreakShots / sgShots`), `ttkSum/ttkN` par rapport au rail, `oneShots` (doit rester 0), `sgJumps`, temps passé à 0 cartouche et points de jauge gagnés en mêlée par vie, et la vitesse moyenne du porteur (le recul ne doit pas la faire monter au-delà de celle du bot rail).

## 12. Étapes d'implémentation

| # | Étape | Fonctions | Estimation |
|---|---|---|---|
| 0 | Prérequis : infra (loadout, `P.ammo` et `P.ammoGauge`, points de mêlée, cellules 100/150, `W_SHOTGUN`, `EV_SHOT`, `hitCount[W_COUNT]`, `NET_VER` 4) | — | (fiche 00) |
| 1 | Constantes, `sg_rand`, `sg_new_seed`, `sg_pellet_dirs`, `sg_falloff` + tests 1 à 3 | Maths, Combat, `run_tests` | 1 h |
| 2 | `fire_shotgun` (agrégation, chaîne), `sg_recoil`, branche dans `game_tick` + tests 4 à 10, 14 à 17 | Combat, `game_tick` | 2 h |
| 3 | `sg_coin_burst` + chaîne de pièces + tests 11, 12 | Combat | 1 h |
| 4 | Pool `TR`, `add_tracer`, dessin dans `render`, `sg_fx_local`, `sg_burst_fx` | Effets, `render` | 1 h |
| 5 | `net_fx_shot`, `case EV_SHOT`, drapeau `f[3]` d'`EV_COIN` + test 13 | réseau | 1 h |
| 6 | Viewmodel, joueurs distants, HUD (cercle, « 7/10 »), sons | Rendu, Interface, Audio | 1,5 h |
| 7 | `bot_think`, match de bots, réglage | bot, journal | 1,5 h |

Total ≈ 9 h (coût **S**).

## 13. Questions ouvertes

1. ~~**Cône : angle total ou demi-angle ?**~~ **Tranchée le 27/10 : option C**, demi-angle 4,5° (cône total de 9°, `SG_HALF_ANGLE 0.0785f`). Toute la fiche en tient compte. Rappel de la comparaison (PvP sans chaîne) :

   | Option | 5 m | 10 m | 20 m | « 2 touches » jusqu'à |
   |---|---|---|---|---|
   | A : demi-angle 3° (cône de 6°) | 56 | 49 | 19 | ~9 m |
   | B : demi-angle 6° (cône de 12°) | 49 | 30 | 7 | ~4,5 m |
   | **C : demi-angle 4,5° (cône de 9°), retenue** | **56** | **37** | **8,4** | **~6 m** (~11 m en slide) |

   Raison : C garde les 2 touches de près et tolère mieux la visée que A, sans empiéter sur le rail à moyenne distance.
2. **Tir à bout portant traversant ?** Recommandation : **non** en v1 (le plomb s'arrête sur le premier corps ; le rail reste l'arme perforante). Variante : à moins de 2,5 m, un plomb traverse la première cible avec ×0,5. L'agrégation par cible le permettrait sans risque pour le plafond.
3. **Signal de l'éclat de pièce chez les autres** : `EV_COIN` sous-type 1 avec `f[3] = 1` (recommandé, pas de nouveau sous-type) ou un sous-type 3 dédié ? Les deux touchent l'interface figée de `EV_COIN` (le Fléau a le sous-type 2) : à valider à la relecture croisée.
4. **Réaction en chaîne des pièces** (profondeur 3, 45 chacune) : à garder ? Avec 4 pièces groupées : 180 PvE en solo, toujours 60 en PvP.
5. **« Coin jump »** : l'éclat ne pousse pas le tireur (recommandé). Faut-il un petit recul sur soi (6 m/s) pour une seconde technique de mouvement ?
6. **Zone de touche d'une pièce pour un plomb** : 0,55 m (proposé, pour qu'une pièce lancée devant soi n'avale pas la moitié des plombs) ou 0,9 m comme le rail ?
7. **Plafonds du recul** (9 m/s vertical, 16 m/s horizontal) : à valider au match de bots. Faut-il aussi limiter le shotgun jump à un par séjour en l'air ?
8. **Mines de la Ventouse** : un plomb détruit-il une mine adverse ? (à trancher avec la fiche 09).
9. L'éclat de pièce doit-il profiter de la chaîne de tir (`streak_mult`) ? Proposé : non (45 fixes).
