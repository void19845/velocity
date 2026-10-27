# Fiche 05 — Bouclier « Égide »

> Arme de mêlée `MK_SHIELD` (5), source de dégâts `W_SHIELD` (11), événement `EV_SHIELD` (19), bit d'état `ST_SHIELD` (0x40 de `NetPState.pad`). Coût visé : **L**.
> Source : [`../IDEES_ARMES_CARTES.md`](../IDEES_ARMES_CARTES.md) § 2.5 ; interfaces figées : [`PLAN.md`](PLAN.md) § 3 ; loadout, munitions et `ammo_points` (jauges, fiche 00) : `00_infra_loadout_munitions.md` (non redéfinis ici).
> Document de design : **rien n'est implémenté**, le pseudo-code n'est pas un patch.

---

## 1. Résumé validé

- **Rôle** : le contre-choix face aux snipers (rail, pièces, Élan). Arme défensive qui **coûte de la vitesse** par choix. Elle remplace l'épée dans le loadout (1 mêlée + 2 distance).
- **Maintenir** la touche de mêlée (`A_MELEE`) = bouclier levé. Bloque **100 %** des touches frontales de rail, pièce et mêlée, et **50 %** du souffle des roquettes. « Frontal » : `dot(direction du coup, face du porteur) < −0,34`, soit un **demi-angle de 70°** autour de la visée (cône de 140° au total ; le « cône de 70° » du document d'idées désigne ce demi-angle, cohérent avec le seuil −0,34).
- **Pénalités** bouclier levé : vitesse au sol limitée à **7 m/s**, accélération aérienne **× 0,5**, **ni dash ni slide**. Aucune pénalité pendant la **traction du grappin**.
- **Jauge 120** : chaque touche bloquée consomme ses dégâts PvP (rail = 60, donc 2 rails brisent le bouclier). Régénère **30/s** bouclier baissé. Jauge à 0 = **brisé 3 s**.
- **Parry parfait** : bouclier levé au plus **0,15 s** avant la touche → le rail est **renvoyé au tireur à 60 PvP** (ricochet sans bonus), les roquettes sont renvoyées selon les règles existantes (`try_parry`), la jauge n'est **pas** consommée.
- **Tap** (relâché en moins de 0,15 s) = **coup de bouclier** `25 + 3 × v_rel`, portée 2,2 m, recul 14.
- **Bélier** : bouclier levé pendant le grappin → arriver au contact déclenche le coup de bouclier gratuitement.
- **Contre-jeu** : flanc et dos, roquette dans les pieds, ricochet de pièce par le côté, **grappin sur le porteur = bouclier arraché 1 s**.
- **Réseau** : le tireur lit `ST_SHIELD` + `yaw`/`pitch` de la cible (`NetPState`) et teste la face avant `net_hit` ; l'hôte revérifie avec `lastShieldT` (tolérance 0,15 s, schéma de `lastParryT`). Correction du document d'idées : l'état passe par **`ST_SHIELD` dans `pad`** (interface figée), pas par un `NF_SHIELD` dans `flags`.

## 2. Règles détaillées

### 2.1 Machine à états locale (`P.shieldState`)

| État | Entrée | Sortie | Effets |
|---|---|---|---|
| `SHS_DOWN` | départ, relâché, respawn | appui `A_MELEE` si `shieldBrokenT ≤ 0` et `shieldRipT ≤ 0` → `SHS_UP` | jauge +30/s ; `shieldDownT += dt` |
| `SHS_UP` | appui | relâché → `SHS_DOWN` (+ coup de bouclier si tenu < 0,15 s) ; jauge ≤ 0 → `SHS_BROKEN` ; arraché → `SHS_DOWN` ; mort → `SHS_DOWN` | pénalités, `ST_SHIELD` émis, tir à distance interdit |
| `SHS_BROKEN` | jauge à 0 (solo) ou `EV_SHIELD`/`SHE_BREAK` (LAN) | `shieldBrokenT ≤ 0` → `SHS_DOWN` avec jauge 0 | rien ne bloque ; la régénération reprend à la sortie (0 → 120 en 4 s) |

- **Levée instantanée** côté logique (l'animation de 0,08 s est purement visuelle) : sinon le parry parfait serait injouable.
- **Fenêtre parfaite** : à la levée, si `shieldDownT ≥ SHIELD_REARM` (0,3 s), on ouvre `P.parryWin = SHIELD_PARRY_WIN` (0,15 s) et `P.swingLanded = 1` (pas de pénalité de coup dans le vide). Le `SHIELD_REARM` empêche de spammer levé/baissé pour rester en fenêtre parfaite en permanence.
- **Tap / maintien** : la décision se prend **au relâchement**. Tenu < `SHIELD_TAP` et `meleeCd ≤ 0` → `shield_bash(0)`. Un tap ouvre donc aussi la fenêtre parfaite, exactement comme un coup d'épée.
- **Tir interdit bouclier levé** (`fire_*` ignoré, le clic sec n'est pas joué). Pièces autorisées. Le changement d'arme à distance reste possible.
- **Mort** : `SHS_DOWN`, minuteries à 0. **Respawn** (`EV_RESPAWN`) : jauge 120, `shieldBrokenT = shieldRipT = 0`, `shieldDownT = 1`. **Hitstop** : `game_tick` sort tôt, toutes les minuteries du bouclier gèlent. **Fin de match** : l'hôte ignore déjà les touches (`matchOver`) ; aucun traitement spécial. **Changement de mêlée** : uniquement au spawn (infra), donc pas de cas en vie.

### 2.2 Pénalités de mouvement (`player_tick`)

Actives si `shieldState == SHS_UP && P.hook != HOOK_ATTACHED` :

- **Sol** : `accelerate(wish, min(RUN_SPEED, 7), …)` et, si `hlen(vel) > 7`, freinage `SHIELD_BRAKE` = 40 m/s² jusqu'à 7 (pas de coupure nette : l'élan se perd en ≈ 0,1 s par 4 m/s).
- **Air** : `AIR_ACCEL × 0,5`. L'air-strafe gagne deux fois moins vite. Le bhop ne touche le sol qu'un tick par saut : il garde l'essentiel de l'élan (voir Questions ouvertes).
- **Dash** : `dashPress` ignoré, charge non consommée. **Slide** : ne démarre pas ; un slide en cours s'arrête à la levée.
- **Exception grappin** : `HOOK_ATTACHED` → aucune pénalité, dash permis. C'est le « bélier ».

### 2.3 Ce que bloque le bouclier

Règle générale : un coup **direct** (hitscan, mêlée, projectile) frontal est bloqué à 100 % ; un **souffle** frontal (centre de l'explosion devant le porteur) est bloqué à 50 % ; un souffle sous les pieds (direction surtout verticale) ou dans le dos n'est pas bloqué. Le recul d'un coup bloqué est multiplié par `SHIELD_KNOCK_MULT` (0,5). Une touche bloquée **ne compte pas** comme touche pour l'attaquant : pas de dash, pas d'`ammo_points` (jauges, fiche 00), et `shot_result(0)` pour les armes à chaîne.

`dir` = direction **du coup vers le porteur** ; `face` = `view_dir` du porteur reconstruite depuis `NetPState` (`yaw` et `pitch`, en 3D : regarder en l'air protège d'un piqué).

| Arme (fiche) | `dir` utilisé | Blocage | Particularité |
|---|---|---|---|
| Rail `W_RAIL` | direction du tir `d` | 100 % ; **le rail s'arrête sur le bouclier** (n'est plus perforant au-delà) | parry parfait → renvoi 60 PvP |
| Pièce / ricochet `W_COIN` | `pièce → cible` | 100 % si le ricochet arrive de face ; **un ricochet qui arrive par le côté passe** | c'est le contre-jeu « skill » des pièces ; parfait → renvoi au tireur |
| Roquette `W_ROCKET`, renvoyée `W_PARRY` | `centre → porteur` | 50 % du souffle ; une roquette qui percute le bouclier explose devant lui, donc 50 % | parfait : `try_parry` local (règles existantes, ×1,25 puis ping-pong) |
| Épée `W_MELEE` | `attaquant → porteur` | 100 % | parfait : chemin existant `EV_MELEE_PARRY` |
| Impact de grappin `W_HOOK` | `attaquant → porteur` | 100 % si le bouclier est levé **à l'impact** | l'accroche arrache le bouclier 1 s (§ 7), donc souvent l'impact passe |
| Enclume `W_SLAM` (sol / onde) | `attaquant → porteur` / — | coup au sol 100 % ; **onde de choc non bloquée** (elle passe sous le bouclier) | Enclume = contre doux du bouclier, comme la roquette au sol |
| Lance `W_LANCE` | `attaquant → porteur` | 100 % (joute et estoc) | désarçonnement à moitié : la cible reçoit 30 % de la vitesse du lancier au lieu de 60 %, le lancier garde 50 % et sa lance se relève (recharge 1,0 s) |
| Fléau `W_FLAIL` | `attaquant → porteur` | dégâts 100 % ; **traction à 50 %** | pièce frappée : projectile, `dir` = sa vitesse, 100 % |
| Poings `W_FIST` | `attaquant → porteur` | 100 % par coup | combo complet ≈ 91 PvP = 3/4 de la jauge ; le punch-jump n'est pas concerné |
| Éventail `W_SHOTGUN` | direction du tir | 100 % de l'agrégat ; un seul événement par cible et par tir | pièce qui éclate = souffle → 50 % ; parfait → renvoi de l'agrégat (≤ 60) |
| Élan `W_KINETIC` | direction du tir | 100 % | parfait → renvoi comme le rail (≤ 60) ; l'Élan a sacrifié sa vitesse pour rien |
| Scie `W_SAW` | vitesse de la scie | 100 % ; la scie bloquée **compte comme ratée** : elle tombe et se **plante au sol** sous le point d'impact (`EV_SAW` sous-type 1) | parfait : `try_parry` (projectile), règle de la fiche Scie |
| Ventouse `W_MINE` | `mine → porteur` | souffle 50 % ; une mine qui touche l'avant du bouclier **se colle au bouclier** (collée au joueur, côté face) et explose donc toujours devant : 50 % | re-lever le bouclier (fenêtre parfaite) l'arrache comme un parry (règle Ventouse) |
| Tesla `W_TESLA` | `tireur → porteur` | 100 % de chaque tranche de 0,2 s ; l'arc **ne chaîne pas** depuis un porteur qui bloque | parfait : règle Tesla existante (tireur étourdi 0,4 s, 15 dégâts) via `NF_PARRY` |
| Coup de bouclier `W_SHIELD` | `attaquant → porteur` | 100 % (bouclier contre bouclier) | parable (liste de l'hôte) |
| Dégâts sur soi `W_SELF` | — | jamais | — |
| Solo : orbe `PR_ORB` | vitesse de l'orbe | 100 % (12 sur la jauge), l'orbe disparaît | parfait : `try_parry` la renvoie (existant) |
| Solo : kamikaze (contact 18) | `kamikaze → joueur` | 100 % (18 sur la jauge), recul × 0,5, il meurt quand même | — |

## 3. Constantes

```c
/* bouclier Egide (MK_SHIELD) */
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
enum { SHS_DOWN, SHS_UP, SHS_BROKEN };
enum { SHE_CLAIM, SHE_CLAIM_BLAST, SHE_BLOCK, SHE_BREAK, SHE_PERFECT, SHE_DENIED, SHE_RIP };  /* EV_SHIELD.sub */
```

## 4. Données

```c
/* Player */
int   shieldState;                 /* SHS_* */
float shieldGauge;                 /* 0..SHIELD_GAUGE_MAX (copie locale ; l'hote fait autorite en LAN) */
float shieldHeldT, shieldDownT;    /* duree tenue / duree baisse (rearmement de la fenetre parfaite) */
float shieldBrokenT, shieldRipT;   /* brise 3 s, arrache 1 s */
int   shieldRamDone;               /* un seul belier par accroche */
/* NetPlayer */
double lastShieldT;                /* derniere fois vu avec ST_SHIELD (hote) */
float  shieldGauge, shieldBrokenT, shieldRipT;   /* hote : autorite ; clients : miroir pour le rendu */
/* statistiques hote (journal du bot) */
static int shieldBlocks, shieldBreaks, shieldPerfects, shieldDenied, shieldConverted;
```

`NetPState` : aucun champ ajouté. `pad` = `meleeKind | (shieldState == SHS_UP ? ST_SHIELD : 0)` (assemblage de l'octet décrit par l'infra). `NetEvent` : aucun champ ajouté, voir § 6.

## 5. Algorithme

### 5.1 Utilitaires (section « état partagé », après `np_aabb`)

```c
static V3 np_face(int i) {                     /* face du joueur i, comme view_dir */
    float cp = cosf(NP[i].st.pitch);
    return v3(sinf(NP[i].st.yaw) * cp, sinf(NP[i].st.pitch), -cosf(NP[i].st.yaw) * cp);
}
static int np_shield_up(int i) {
    return (NP[i].st.pad & 0x0F) == MK_SHIELD && (NP[i].st.pad & ST_SHIELD);
}
/* Appele par TOUTE arme juste avant net_hit sur un joueur. Renvoie 1 si le coup est
 * revendique comme bloque : l'appelant n'appelle alors PAS net_hit. */
static int shield_intercept(int i, V3 dir, float dmgPvp, V3 knock, int w, int blast) {
    if (w == W_SELF || !np_shield_up(i) || vdot(dir, np_face(i)) >= SHIELD_COS) return 0;
    if (blast && dir.y > 0.75f) return 0;      /* souffle sous les pieds */
    NetEvent e = ev_new(EV_SHIELD);
    e.sub = blast ? SHE_CLAIM_BLAST : SHE_CLAIM; e.target = (unsigned char)i; e.actor = (unsigned char)w;
    e.f[0] = dmgPvp; vf(&e.f[1], dir); vf(&e.f[4], knock);
    net_emit(&e);
    shield_spark_fx(np_center(i), dir);        /* effet predit chez le tireur : etincelles + S_SHIELD_BLOCK */
    return 1;
}
```

Déclaration anticipée de `shield_intercept` avec `net_hit`. Contrat pour les autres fiches : **un seul appel par cible et par tir**, avec les dégâts déjà agrégés (Éventail, Tesla).

### 5.2 Appels dans les armes existantes

- **`fire_rail`** : avant la boucle des joueurs, chercher le porteur frontal le plus proche sur le rayon (`ray_box` + `np_shield_up` + test de face) ; `tw = min(tw, tShield)` : le rail s'arrête sur le bouclier, les joueurs au-delà ne sont plus touchés et le rayon (local et `net_fx_rail`) se termine sur le bouclier. Pour chaque joueur touché : `if (!shield_intercept(i, d, dmg, vmul(d, 4), W_RAIL, 0)) { net_hit(...); hit = 1; }`. Un rail entièrement bloqué → `shot_result(0)`.
- **`ricochet`** : `dir = vnorm(at - o)` ; `shield_intercept(tp, dir, dmg * PVP_SCALE, vmul(dir, 6), W_COIN, 0)` avant `net_hit`. Pièce de côté → `dir` latéral → pas de blocage.
- **`explode`** (boucle LAN) : `dv = np_center(i) - c` ; `shield_intercept(i, vnorm(dv), dmg_f * PVP_SCALE, knock, W_ROCKET ou W_PARRY, 1)` avant `net_hit`. Solo, joueur local : `if (shield_local_faces(vnorm(pcenter() - c)))` → dégâts et recul × 0,5, la jauge prend l'autre moitié (sauf `byPlayer`, souffle de sa propre roquette : jamais bloqué, le rocket jump reste intact).
- **`melee`** (épée) : `dir = vnorm(c - eye)` ; `shield_intercept(i, dir, dmg, knock, W_MELEE, 0)`. Un coup bloqué compte pour `swingLanded` (pas de pénalité de coup dans le vide) mais ne rend ni dash ni munition.
- **`grapple_tick`** :
  - à l'accroche d'un joueur (`ptarget >= 0`) : `if (np_shield_up(ptarget)) net_fx_shield_rip(ptarget)` (§ 7) ;
  - à l'impact (`onTarget && pl >= 0`) : si **je** porte le bouclier levé → **bélier** (§ 5.4) ; sinon `shield_intercept(pl, dir, dmg, knock, W_HOOK, 0)` avant `net_hit`, et `grapple_bounce` dans tous les cas.
- **`proj_tick`** (orbes, solo) : avant `damage_player(12)`, `if (shield_local_faces(dir)) { shield_absorb_local(12); burst(…); p->alive = 0; continue; }`. **`enemy_tick`** (kamikaze) : même chose avec 18 et `dir` = direction du kamikaze vers le joueur.

`shield_local_faces(dir)` = `P.shieldState == SHS_UP && vdot(dir, view_dir()) < SHIELD_COS` (solo et joueur local). `shield_absorb_local(x)` = `P.shieldGauge -= x`, étincelles, son ; à 0 → `SHS_BROKEN`, `shieldBrokenT = 3`, éclats, `S_SHIELD_BREAK`.

### 5.3 `shield_tick(dt, pressed, held)` (nouvelle, appelée par `game_tick` à la place de `melee()` si `meleeKind == MK_SHIELD`)

```c
static void shield_tick(float dt, int press, int held) {
    P.shieldBrokenT -= dt; P.shieldRipT -= dt;
    if (P.dead) { P.shieldState = SHS_DOWN; return; }
    if (P.shieldState == SHS_BROKEN && P.shieldBrokenT <= 0) { P.shieldState = SHS_DOWN; P.shieldDownT = 0; }
    if (P.shieldState == SHS_UP && (P.shieldRipT > 0 || P.shieldBrokenT > 0)) P.shieldState = SHS_DOWN;
    if (press && P.shieldState == SHS_DOWN && P.shieldRipT <= 0) {          /* levee */
        P.shieldState = SHS_UP; P.shieldHeldT = 0; P.sliding = 0;
        if (P.shieldDownT >= SHIELD_REARM) { P.parryWin = SHIELD_PARRY_WIN; P.swingLanded = 1; }
        play(S_SHIELD_UP, 0.5f, 1.0f);
    }
    if (P.shieldState == SHS_UP) {
        P.shieldHeldT += dt;
        if (!held) {                                                        /* relache */
            P.shieldState = SHS_DOWN; P.shieldDownT = 0;
            if (P.shieldHeldT < SHIELD_TAP && P.meleeCd <= 0) shield_bash(0);
        }
        if (P.hook == HOOK_ATTACHED && !P.shieldRamDone) shield_ram_check();
    } else {
        P.shieldDownT += dt;
        if (P.shieldState == SHS_DOWN && !dmMode) P.shieldGauge = fminf(SHIELD_GAUGE_MAX, P.shieldGauge + SHIELD_REGEN * dt);
    }
    if (P.hook != HOOK_ATTACHED) P.shieldRamDone = 0;
}
```

En LAN, la régénération locale est une **prédiction** (même formule, voir § 6.3). Dans `game_tick` : la règle « coup dans le vide » après `parryWin` est sautée pour `MK_SHIELD` ; `try_parry` continue d'être appelé pendant `parryWin` (renvoi des orbes, roquettes, scies, mines en vol) ; le tir est ignoré si `shieldState == SHS_UP`.

### 5.4 Coup de bouclier et bélier

```c
static void shield_bash(int free) {            /* free = belier : ne coute pas de recharge */
    V3 eye = eye_pos(), f = view_dir(); int hits = 0;
    for (ennemis e) si (dist - rayon <= SHIELD_BASH_RANGE && (dist < 0.8 || dot >= MELEE_CONE)) {
        rel = rel_speed(e->vel);
        if (kamikaze) { damage_enemy(i, 9999, …); parry_feedback(at); }    /* comme l'epee */
        else damage_enemy(i, SHIELD_BASH_DMG + SHIELD_BASH_PER_MS * rel, vmul(f, SHIELD_BASH_KNOCK));
        hits++;
    }
    for (joueurs i, np_target) meme test de portee et de cone {
        rel = rel_speed(np_vel(i)); dmg = (SHIELD_BASH_DMG + SHIELD_BASH_PER_MS * rel) * PVP_SCALE;
        knock = vadd(vmul(f, SHIELD_BASH_KNOCK), v3(0, 3, 0));
        if (!shield_intercept(i, vnorm(c - eye), dmg, knock, W_SHIELD, 0)) { net_hit(i, dmg, knock, W_SHIELD); hits++; }
    }
    if (dmMode) net_fx_melee(eye, f, vlen(P.vel));       /* visuel existant EV_MELEE */
    P.meleeT = 0.18f;                                    /* animation de poussee */
    if (hits) { P.dashCharge = fminf(2, P.dashCharge + 1); melee_hit_points(meleePts[P.meleeKind]);
                hitstop = fmaxf(hitstop, 0.05f); play(S_SHIELD_BASH, 0.9f, 1); }
    if (!free) P.meleeCd = hits ? SHIELD_BASH_CD : SHIELD_BASH_WHIFF_CD;
}
static void shield_ram_check(void) {            /* bouclier leve pendant la traction */
    si un ennemi ou un joueur est a <= SHIELD_BASH_RANGE dans le cone MELEE_CONE :
        P.shieldRamDone = 1; shield_bash(1); impact_frame(…, 0.07f);
        si la cible est celle du grappin : grapple_bounce(hit, dir);   /* remplace l'impact W_HOOK */
}
```

Bélier sur la cible accrochée : `grapple_tick` saute son propre impact quand `P.shieldState == SHS_UP` (une seule touche, `W_SHIELD`, dégâts `max(50 + 3·v, 25 + 3·v)` = l'impact du grappin, donc au moins aussi fort). Plafonné à 60 PvP de toute façon.

### 5.5 `player_tick` (pénalités)

```c
int shPen = P.shieldState == SHS_UP && P.hook != HOOK_ATTACHED;
if (shPen) { dashPress = 0; if (P.sliding) P.sliding = 0; }           /* avant le bloc slide et le dash */
/* bloc slide : ajouter && !shPen a la condition de demarrage */
/* acceleration au sol */
accelerate(wish, P.crouch ? CROUCH_SPEED : (shPen ? SHIELD_GROUND_SPEED : RUN_SPEED), GROUND_ACCEL, 1e9f, dt);
if (shPen && P.grounded) { float h = hlen(P.vel); if (h > SHIELD_GROUND_SPEED) {
    float k = fmaxf(SHIELD_GROUND_SPEED, h - SHIELD_BRAKE * dt) / h; P.vel.x *= k; P.vel.z *= k; } }
/* air */
accelerate(wish, RUN_SPEED, AIR_ACCEL * (shPen ? SHIELD_AIR_MULT : 1), AIR_CAP, dt);
```

## 6. Réseau

### 6.1 Événement `EV_SHIELD` (19)

| `sub` | Émetteur | Champs | `host_handle` | `apply_event` |
|---|---|---|---|---|
| `SHE_CLAIM` / `SHE_CLAIM_BLAST` | tireur (`shield_intercept`) | `target` porteur, `actor` = `W_*`, `f[0]` dégâts PvP, `f[1..3]` `dir`, `f[4..6]` recul | arbitrage § 6.2 | rien (le résultat suit) |
| `SHE_BLOCK` | hôte | `actor` attaquant, `target` porteur, `f[0]` absorbé, `f[1]` jauge après, `f[2]` dégâts passés (souffle), `f[3..5]` recul × 0,5 | — | tous : étincelles sur le porteur, miroir `NP[t].shieldGauge` ; porteur : jauge, recul, flash ; attaquant : « BLOQUE » |
| `SHE_BREAK` | hôte | `target` | — | porteur : `SHS_BROKEN`, 3 s ; tous : éclats, `S_SHIELD_BREAK` |
| `SHE_PERFECT` | hôte | `actor` porteur, `target` tireur, `f[0..2]` départ, `f[3..5]` arrivée, `f[6]` dégâts renvoyés | — | rayon doré ; porteur : `parry_feedback`, `melee_parry_points(PTS_PARRY)` ; tireur : recul `dir × 4`, « RENVOYE PAR X » |
| `SHE_DENIED` | hôte | `actor` attaquant, `target` porteur, `f[0]` dégâts, `f[1..3]` recul | — | comme un `EV_HIT` : la victime prend le recul et le flash ; l'attaquant reçoit son marqueur de touche et son `ammo_points` (jauges, fiche 00) |
| `SHE_RIP` | grappineur (`grapple_tick`) | `target` porteur | `NP[t].shieldRipT = 1` | porteur : `shieldRipT = 1`, bouclier baissé ; tous : étincelles, son métallique grave |

Sept sous-types tiennent dans `sub` ; `f[7]` suffit partout ; aucun changement de `NetEvent`. `NET_VER` 4 est déjà prévu par l'infra.

### 6.2 Arbitrage par l'hôte

Refactor préalable : extraire de `host_handle` la partie « PV, plafond, ttk, kill » en `host_apply_damage(int from, int v, float dmg, int w)` (utilisée par `EV_HIT`, `SHE_DENIED`, `SHE_PERFECT`, souffle partiel).

```c
static int host_shield_ok(int v, V3 dir, float cosLim) {
    return (NP[v].st.pad & 0x0F) == MK_SHIELD && NP[v].shieldBrokenT <= 0 && NP[v].shieldRipT <= 0 &&
           nowT - NP[v].lastShieldT < SHIELD_GRACE && vdot(dir, np_face(v)) < cosLim;
}
static int host_shield_perfect(int v) { return nowT - NP[v].lastParryT < MELEE_PARRY_GRACE; }

/* EV_SHIELD / SHE_CLAIM(_BLAST), apres les gardes habituelles (cible vivante, match en cours) */
dir = fv(&ev->f[1]); blast = ev->sub == SHE_CLAIM_BLAST; w = ev->actor;
if (!host_shield_ok(v, dir, SHIELD_COS_HOST)) {              /* refus : degats complets */
    shieldDenied++; emettre SHE_DENIED; host_apply_damage(from, v, fminf(dmg, PVP_HIT_CAP), w); return; }
if (!blast && is_hitscan(w) && host_shield_perfect(v)) { host_shield_reflect(v, from, dmg); return; }
absorbed = fminf(blast ? dmg * SHIELD_BLAST_FRAC : dmg, PVP_HIT_CAP);
passed   = blast ? fminf(dmg * (1 - SHIELD_BLAST_FRAC), PVP_HIT_CAP) : 0;
NP[v].shieldGauge -= absorbed; shieldBlocks++;
emettre SHE_BLOCK (absorbed, jauge, passed, recul * SHIELD_KNOCK_MULT);
if (passed > 0) host_apply_damage(from, v, passed, w);
if (NP[v].shieldGauge <= 0) { NP[v].shieldGauge = 0; NP[v].shieldBrokenT = SHIELD_BROKEN_T; shieldBreaks++; emettre SHE_BREAK; }

/* EV_HIT : nouvelles branches, entre le parry d'epee et l'application des degats */
if (victime MK_SHIELD) {
    dir = vnorm(np_center(v) - np_center(from));             /* ligne attaquant -> victime vue par l'hote */
    if (arme parable && host_shield_perfect(v) && vdot(dir, np_face(v)) < SHIELD_COS) -> EV_MELEE_PARRY existant;
    else if (is_hitscan(w) && host_shield_perfect(v) && vdot(dir, np_face(v)) < SHIELD_COS) host_shield_reflect(v, from, dmg);
    else if (SHIELD_HOST_CONVERT && is_direct(w) && host_shield_ok(v, dir, SHIELD_COS)) { shieldConverted++; -> bloc comme ci-dessus; }
}
```

- `is_hitscan(w)` : `W_RAIL`, `W_COIN`, `W_KINETIC`, `W_SHOTGUN`. `is_direct(w)` : hitscan sauf `W_COIN` (l'hôte ne connaît pas la pièce), plus mêlées, `W_HOOK`, `W_SAW`. Les souffles ne sont **jamais** convertis par l'hôte (il ne connaît pas le centre).
- Pour une victime `MK_SHIELD`, le parry d'épée existant **exige la face** (sinon lever le bouclier parerait une épée dans le dos). Pour les autres mêlées, rien ne change.
- `host_shield_reflect(v, from, dmg)` : `shieldPerfects++`, `NP[v].hp += PARRY_HEAL` (comme tout parry), émet `SHE_PERFECT` (rayon de `np_center(v)` à `np_center(from)`), puis `host_apply_damage(v, from, fminf(dmg, SHIELD_REFLECT_PVP), W_SHIELD)` : un kill par renvoi est crédité au porteur, arme « BOUCLIER ». Jauge intacte.
- `lastShieldT` : mis à jour dans `host_recv` (`PK_STATE`) si `pad & ST_SHIELD` **et** `shieldBrokenT ≤ 0` et `shieldRipT ≤ 0` (un client ne peut pas prolonger un bouclier brisé), et dans `host_tick` pour l'hôte lui-même (`if (P.shieldState == SHS_UP) NP[myId].lastShieldT = nowT;`, à côté de `lastParryT`). La fenêtre parfaite réutilise `NF_PARRY`/`lastParryT` : aucune donnée de plus.
- Liste des coups parables : `W_SHIELD` y est (interface figée). Coup de bouclier paré : chemin `EV_MELEE_PARRY` existant, `meleeCd = MELEE_PARRIED_CD` (seul le coup est bloqué 1 s, lever reste possible).

### 6.3 Qui calcule la jauge, et comment tous la connaissent

- **Autorité : l'hôte** (`NP[v].shieldGauge`, `shieldBrokenT`). Il consomme (blocages) et régénère dans `host_tick` : pour chaque joueur actif, `brokenT -= dt`, `ripT -= dt`, et si `!(st.pad & ST_SHIELD) && brokenT <= 0` → `+30·dt`, plafond 120. `host_respawn` remet 120.
- **Porteur : prédiction** avec la même formule, **recalée** à chaque `SHE_BLOCK` (`f[1]`) et `SHE_BREAK`. L'écart entre deux recalages ne vient que du retard du bit `ST_SHIELD` (≈ 30/s × 16 ms = 0,5 point) : négligeable.
- **Autres clients** : miroir `NP[v].shieldGauge` mis à jour par `SHE_BLOCK`/`SHE_BREAK`, régénéré localement d'après le bit `ST_SHIELD` ; sert uniquement au rendu (fissures). `EV_RESPAWN` remet 120 chez tous.
- Rien dans `PkSnap` (pas de place, et inutile en LAN).

### 6.4 Divergences tireur / hôte

| Tireur voit | Hôte voit | Résultat |
|---|---|---|
| levé, de face → `CLAIM` | levé (< 0,15 s), face < −0,17 | **bloqué** |
| levé, de face → `CLAIM` | baissé depuis > 0,15 s, brisé, arraché, ou de côté | **refusé** : `SHE_DENIED`, dégâts complets |
| levé → `CLAIM` | fenêtre parfaite | **parfait** (hitscan) / bloqué (le reste) |
| baissé → `net_hit` | fenêtre parfaite + face | **parfait** : c'est le cas normal du parry parfait, que le tireur ne peut pas voir |
| baissé → `net_hit` | levé + face | **converti en bloc** si `SHIELD_HOST_CONVERT` (défaut 1, « favoriser le défenseur » comme le parry d'épée) |
| de côté → `net_hit` | de face | dégâts (l'hôte convertit au seuil strict −0,34 avec sa propre ligne attaquant → victime) |

L'émetteur ne reçoit pas son propre événement : la prédiction du tireur (étincelles, son) se fait dans `shield_intercept`, et la correction arrive par `SHE_DENIED`/`SHE_PERFECT`. Le relais des `CLAIM` aux autres clients est inutile mais inoffensif (`apply_event` les ignore). Tolérance totale effective : fenêtre parfaite 0,15 + grâce 0,15 = 0,30 s à l'hôte, comme l'épée (0,12 + 0,15).

## 7. Interactions

- **Parry** : la levée ouvre une fenêtre de parry ordinaire (`parryWin`, `NF_PARRY`) : renvois de projectiles par `try_parry` (orbes, roquettes, scies, mines), parry d'épée, de grappin et des mêlées parables côté hôte (avec face). Le rail, l'Élan, l'Éventail et le ricochet ont le renvoi propre au bouclier (§ 6.2). Tesla : règle Tesla existante.
- **Pièces** : le porteur peut lancer des pièces bouclier levé, pas tirer dedans. Une pièce adverse qui ricoche **de face** est bloquée ; le contre est de lancer la pièce sur le côté.
- **Grappin adverse** : l'accroche émet `SHE_RIP` (le grappineur fait autorité sur son accroche) ; le porteur baisse le bouclier 1 s, l'hôte le considère baissé 1 s. Un impact qui arrive après 1 s (traction longue) peut de nouveau être bloqué si le porteur relève de face.
- **Grappin du porteur** : aucune pénalité pendant la traction ; bélier (§ 5.4).
- **Munitions** (`00_infra`) : coup de bouclier qui touche → `melee_hit_points(meleePts[P.meleeKind])` ; kill → `PTS_MELEE_KILL` (par `EV_KILL`) ; parry parfait → `melee_parry_points`. Un blocage ordinaire ne rend rien (sinon le bouclier deviendrait une ferme à munitions). L'attaquant bloqué ne reçoit pas non plus de recharge.
- **Mine collée sur le bouclier** : elle reste sur la face avant (repère local du porteur) ; son explosion est donc frontale (50 %) tant qu'elle n'est pas arrachée par une nouvelle levée.

## 8. Rendu et son

- **`draw_viewmodel`** (branche `MK_SHIELD`, remplace l'épée) : bouclier baissé = plaque `(-0.62,-0.42,-0.36)…(-0.18,0.02,-0.30)` en bas à gauche, inclinée de 25°. Levé : interpolée en 0,08 s vers `(-0.42,-0.30,-0.40)…(0.12,0.18,-0.34)` ; face **semi-transparente** (alpha 0,3, `GL_BLEND`) pour garder la visée, bord opaque coloré selon la jauge (cyan à 120 → rouge à 0). Fissures (`GL_LINES` noires) sous 60. Fenêtre parfaite : bord blanc. Coup de bouclier : poussée de 0,25 sur −Z pendant `meleeT`. Brisé : rien en main, bras seul, 3 petites boîtes qui tombent au moment de la casse.
- **`draw_remote_players`** : `np_shield_up(i)` → plaque devant le torse `(-0.45,0.3,-0.52)…(0.45,h-0.25,-0.44)`, couleur du joueur × 0,6, bord selon `NP[i].shieldGauge` ; baissé (`meleeKind == MK_SHIELD` sans `ST_SHIELD`) → petite plaque sur le flanc gauche. Lisibilité : le bord brillant indique de loin qu'il faut prendre le flanc.
- **`draw_hud`** : le panneau ÉPÉE devient « EGIDE » : barre de jauge en 4 segments de 30, états « LEVE », « BRISE 2.4 s », « ARRACHE », et aperçu du coup de bouclier `(25 + 3·rel) × 0,7` via `melee_preview`. Textes « BLOQUE », « RENVOYE ! », « BOUCLIER BRISE » dans `popText`.
- **Particules** : blocage → `burst` cyan-blanc 16 étincelles au point d'impact (`np_center` + `-dir × 0,5`) ; casse → `shards(couleur, 14, 8)` + `burst` ; parfait → `add_beam` doré + `impact_frame(at, 0.08f)` ; arrachage → 10 étincelles orange. Hitstop : 0,03 s sur un blocage (porteur et attaquant), 0,12 s sur un parfait (`parry_feedback`).
- **Sons** (`make_sounds`, avant `S_COUNT`) :
  - `S_SHIELD_UP` (0,08 s) : bruit passe-bas `expf(-t*60)` + clic sinus 900 Hz `expf(-t*90)`.
  - `S_SHIELD_BLOCK` (0,5 s) : partiels 520, 780, 1170 Hz (0,3 / 0,2 / 0,12) `expf(-t*9)` + bruit `expf(-t*70)` : un « clang » plus grave que `S_PARRY`.
  - `S_SHIELD_BREAK` (0,6 s) : bruit large `expf(-t*8)` + sinus glissant de 400 à 120 Hz.
  - `S_SHIELD_BASH` (0,3 s) : `S_PUNCH` grave (110 → 60 Hz) + partiel 640 Hz `expf(-t*12)`.
  - Parfait : `S_PARRY` hauteur 0,85 + `S_RAIL` hauteur 1,3.

## 9. Bots (`bot_think`)

Si `P.meleeKind == MK_SHIELD` :

1. **Menace rail** : un adversaire visible à plus de 8 m tient `WP_RAIL`/`WP_KINETIC` et vise vers moi (`dot(np_face(i), vers moi) > 0.97`) → `botHeld[A_MELEE] = 1` en le regardant, 0,6 s au plus, puis 0,8 s baissé (régénération). Jamais si jauge < 40.
2. **Roquette** : reprendre le bloc « parry de roquette » existant : à `d < MELEE_RANGE + 1.2` → appui (fenêtre parfaite), 60 % des fois ; sinon lever plus tôt (`d < 8`) pour les 50 %.
3. **Mêlée** : épée adverse en plein élan à moins de 5 m (`st.melee ≥ 150`) → lever (une fois sur deux, comme aujourd'hui).
4. **Contact** : à moins de 2,2 m → tap (`botPress[A_MELEE]` puis relâché au tick suivant). Grappin sur un joueur → bouclier tenu pendant la traction (bélier).
5. Journal : `netlog("bouclier : bloque %.0f, jauge %.0f")`, « parfait », « brise », « refuse ».

## 10. Tests (`run_tests`)

Couloir x = −20 ; partie LAN simulée existante (`netMode = NET_HOST`, `myId = 0`, `NP[1]`).

1. **Pénalités** : au sol à 12 m/s, bouclier levé, 120 ticks → `hlen` (attendu 7,0) ; `dashCharge` après dash (attendu 2) ; `sliding` (attendu 0) ; même chose `HOOK_ATTACHED` vers un mur → vitesse > 7.
2. **Air** : air-strafe scripté 1 s avec et sans bouclier → rapport des gains (attendu ≈ 0,5).
3. **Tireur, de face** : `NP[1].st.pad = MK_SHIELD | ST_SHIELD`, `yaw` vers moi, `lastShieldT = nowT` ; `fire_rail` → PV 100, jauge 60 ; second rail → jauge 0, `shieldBrokenT` 3 ; troisième → PV 40 (attendu 100/60, 0/3, 40).
4. **Flanc** : `yaw` tourné de 90° → `fire_rail` → PV 40.
5. **Rail arrêté** : deux cibles alignées, la première bouclier de face → la seconde garde 100 PV.
6. **Parfait** : moi porteur, levée (`parryWin`), `host_tick`, `EV_HIT W_RAIL` de 1 avec `NP[1]` devant moi → `NP[1].hp` 40, `NP[0].hp` 100, jauge 120.
7. **Refus** : `EV_SHIELD CLAIM` de 1 sur moi, `lastShieldT` vieux de 1 s → `NP[0].hp` 40.
8. **Conversion** : `EV_HIT W_RAIL` de 1, mon bouclier levé de face, hors fenêtre parfaite → PV 100, jauge 60.
9. **Souffle** : `explode` à 1 m devant `NP[1]` de face, puis de dos → perte de PV de face ≈ moitié de celle de dos ; roquette sous les pieds → pas de blocage.
10. **Coup de bouclier** : cible à 1,5 m, `rel` 10 → PV 61,5 (attendu 100 − 38,5) ; dash +1.
11. **Régénération** (solo) : jauge 0, brisé → après 3 s jauge 0 et état `SHS_DOWN`, après 7 s jauge 120.
12. **Orbe** (solo) : orbe de face → `P.hp` 100, jauge 108 ; de dos → `P.hp` 88.
13. **Arrachage** : `SHE_RIP` sur moi → appui ignoré pendant 1 s, possible après.

## 11. Équilibrage

| Situation | PvE | PvP (× 0,7, plafond 60) | Remarque |
|---|---|---|---|
| Coup de bouclier, v_rel 0 | 25 | 17,5 | 6 touches pour tuer |
| v_rel 10 m/s | 55 | 38,5 | 3 touches |
| v_rel 15 m/s | 70 | 49 | 3 touches (49 + 49 = 98 < 100) |
| v_rel ≥ 20,2 m/s | ≥ 85,7 | **60** | 2 touches |
| Bélier à 46 m/s | 163 | **60** | égal à l'impact du grappin |
| Parfait sur rail / Élan max | — | **60** renvoyés | jauge intacte, +10 PV |

| Jauge 120 contre | Coups pour briser |
|---|---|
| Rail (60) | 2 |
| Épée à v_rel ≥ 12,7 (60) | 2 |
| Roquette au centre (77 → 38,5 absorbés, 38,5 subis) | 4 (et 154 PV subis : le bouclier ne sauve pas des roquettes) |
| Combo de poings à v_rel 15 (≈ 91) | 1,3 combo |
| Tesla (49/s) | 2,45 s de suivi |
| Éventail à bout portant (56) | 3 |
| Orbes de drone (12) | 10 |

Régénération complète : 4 s depuis 0 (7 s après une casse). Points à surveiller dans le journal (`hitCount[W_SHIELD]`, `shieldBlocks`, `shieldBreaks`, `shieldPerfects`, `shieldDenied`, `shieldConverted`) : taux de refus (> 5 % = tolérance mal réglée), part des kills par renvoi, vitesse moyenne d'un bot bouclier contre un bot épée (cible : ≥ 20 % plus lent), durée moyenne bouclier levé par vie.

## 12. Étapes d'implémentation

1. Constantes, champs, `shield_tick`, branche `game_tick`, pénalités de `player_tick` — 0,5 j.
2. Solo : orbes, kamikazes, explosions, régénération, viewmodel, HUD, sons — 0,75 j.
3. `np_face`, `np_shield_up`, `shield_intercept` ; appels dans `fire_rail` (arrêt du rail), `ricochet`, `explode`, `melee`, `grapple_tick` — 0,75 j.
4. `EV_SHIELD`, refactor `host_apply_damage`, arbitrage des `CLAIM`, jauge hôte et miroirs, `lastShieldT`, `SHE_BREAK` — 1 j.
5. Parry parfait (renvoi hitscan, face exigée pour le parry d'épée), arrachage, coup de bouclier, bélier — 0,75 j.
6. `draw_remote_players`, effets, `weaponNames` « BOUCLIER » — 0,25 j.
7. Bot, 13 tests, match de bots et réglages — 0,75 j.

Total ≈ **4,75 jours** (L). Prérequis : infra (loadout, `meleeKind`, `pad`, `ammo_points` (jauges, fiche 00), `NET_VER` 4). Les fiches suivantes doivent appeler `shield_intercept` avant chaque `net_hit` : Éventail, Élan, Scie, Ventouse, Tesla, Enclume, Lance, Fléau, Poings.

## 13. Questions ouvertes

1. **Débordement à la casse** : le coup qui brise la jauge est-il absorbé en entier (choix actuel) ou le surplus passe-t-il ?
2. **Conversion par l'hôte** (`SHIELD_HOST_CONVERT`) : favoriser le défenseur (défaut, comme le parry d'épée) ou le tireur (seules les revendications du tireur bloquent, sauf parry parfait) ?
3. **Tir bouclier levé** : interdit (choix actuel, pour éviter la tortue-sniper) ou autorisé avec une pénalité ?
4. **Bhop bouclier levé** : il garde presque toute sa vitesse (seul l'air-strafe est divisé par 2). Faut-il plafonner aussi la vitesse horizontale en l'air (par exemple 12 m/s) ?
5. **Onde de l'Enclume** : non bloquée (choix actuel), ou 50 % comme un souffle ?
6. **Arrachage** : 1 s fixe (document d'idées) ou « tant que le grappin est accroché, au moins 1 s » ? Avec 1 s, un grappin de plus de ≈ 35 m laisse le temps de relever avant l'impact.
7. **Touche bloquée et chaîne de tir** : la compter comme un raté (choix actuel, `shot_result(0)`) ou ni touche ni raté ?
8. **Pitch dans la face** : bouclier en 3D (choix actuel : regarder en l'air protège d'un piqué) ou horizontal seulement ?
9. **Parry parfait** : soigne-t-il aussi les 10 PV de `PARRY_HEAL` (choix actuel, cohérent avec les autres parries) ?
10. **Coordination** : Scie bloquée = plantée sous le porteur ; mine collée sur la face du bouclier ; Lance bloquée = transfert de vitesse réduit à 30 % ; Fléau bloqué = traction à 50 %. À valider avec les fiches 02, 03, 08 et 09.
