# Fiche 02 : Lance de joute « Lance »

> Source : [`../IDEES_ARMES_CARTES.md`](../IDEES_ARMES_CARTES.md) § 2.2 (validée), § 1.2 et § 1.5. Interfaces figées : [`PLAN.md`](PLAN.md) § 3.
> Prérequis : `00_infra_loadout_munitions.md` (`P.meleeKind`, `P.loadout`, `ammo_points` (jauges, fiche 00), octet `NetPState.pad`, `W_COUNT`, `NET_VER` 4). Rien n'est redéfini ici.
> Identifiants : `MK_LANCE 2`, `W_LANCE 8`, `EV_LANCE 20`, `ST_LANCE 0x20`. Rien n'est implémenté.

## 1. Résumé validé

- **Rôle** : la joute. L'arme qui exploite le plus la **vitesse de rapprochement**, avec le grappin comme monture.
- **Lance couchée** (touche de mêlée *maintenue*, 2 s au maximum) : à chaque tick, `v_c = dot(P.vel − cible.vel, direction vers la cible)`. Si une cible est dans le cône (cos 0,92, soit 23,1°), à 3,5 m au plus, **et** que `v_c ≥ 10 m/s`, la touche part toute seule : `10 + 5 × v_c` dégâts. La lance se relève et se recharge en 1,0 s. La cible reçoit 60 % de ta vitesse et tu en gardes 50 %.
- **Estoc** (*tap*, relâché en moins de 0,15 s) : `30 + 2 × v_rel` dégâts, portée 3 m, avec la fenêtre de parry normale.
- **Lance couchée = pas de parry** pour le porteur. La victime **peut parer** la lance (couchée ou estoc). Une lance parée est indisponible 1,5 s et son porteur reçoit un recul de 11.
- **Choc de joute** (lance contre lance) : si l'un est plus rapide de plus de 2 m/s, il gagne. Sinon, les deux sont désarçonnés.
- **Chiffres** : v_c = 10 donne 60 PvE / 42 PvP ; v_c = 15 donne 85 / 59,5, donc 2 touches pour tuer.
- **Contre-jeu** : sortir du cône (pas de côté), fuir dans le même sens (`v_c` baisse), parer sur l'approche visible.
- Comme toute mêlée : une touche rend **1 dash** et appelle `melee_hit_points(meleePts[P.meleeKind])` (§ 1.5).

## 2. Règles détaillées

### Machine à états (`P.lanceState`)

| État | Entrée | Pendant | Sortie |
|---|---|---|---|
| `LS_IDLE` | défaut, mort, respawn | rien | appui `actP[A_MELEE]` avec `P.meleeCd <= 0` → `LS_THRUST` |
| `LS_THRUST` | appui | l'**estoc part tout de suite** (même tick) ; `parryWin = PARRY_WINDOW` ; `lanceT += dt` | touche relâchée avant `LANCE_TAP` (0,15 s) → `LS_IDLE` (recharge de l'estoc) ; estoc qui a touché → `LS_IDLE` ; touche encore tenue à 0,15 s sans touche → `LS_COUCH` |
| `LS_COUCH` | maintien ≥ 0,15 s | `lance_tick` teste les cibles à chaque tick ; `ST_LANCE` est émis | touche → `LS_IDLE` + `LANCE_HIT_CD` ; relâchée ou `lanceT ≥ LANCE_HOLD_MAX` (2 s) → `LS_IDLE` + `LANCE_MISS_CD` |

**Choix de conception** (voir la question 1) : l'estoc part **à l'appui**, pas au relâchement. Le parry doit répondre à l'instant de l'appui : attendre 0,15 s pour savoir si c'est un tap le rendrait inutilisable. Une lance couchée commence donc par un estoc « gratuit » de 0,15 s. Sa fenêtre de parry (0,12 s) est fermée avant que la lance se couche, ce qui respecte la règle « couchée = pas de parry ». Un estoc dans le vide suivi d'un maintien n'est **pas** puni par `MELEE_WHIFF_CD` : la lance couchée le remplace.

### Conditions exactes d'une touche couchée (une fois par tick, dans `lance_tick`)

Pour chaque cible (drone `E[i]` vivant, joueur `np_target(i)`) :

- `c` = centre (`E[i].pos` ou `np_center(i)`), `d = c − eye_pos()`, `dist = |d|`, `dir = d / dist` ;
- `reach = dist − rayon` (`enemy_radius(type)` : 0,9 pour un drone, 0,75 pour un kamikaze ; 0,5 pour un joueur, comme `melee`). Il faut `reach ≤ LANCE_REACH` (3,5 m) ;
- `dot(dir, view_dir()) ≥ LANCE_CONE` (0,92). Le cône suit la **vue**, pas la vitesse ;
- `v_c = dot(P.vel − vel_cible, dir) ≥ LANCE_VC_MIN` (10). Pour un joueur, `vel_cible = np_vel(i)`.
- **Une seule cible par joute** : parmi les candidates, on prend le plus petit `reach` (la première sur la lance). La lance se relève aussitôt. Des cibles alignées ne sont pas transpercées (question 4).
- Pas de test `los`, comme `melee` (les murs de la carte sont des AABB épaisses).

### Effets d'une touche couchée (chez l'attaquant)

- `vc = fminf(v_c, LANCE_VC_CAP)` (30) ; dégâts `LANCE_DMG + LANCE_DMG_PER_MS × vc`.
- **Transfert de vitesse** : recul envoyé à la cible `knock = 0,6 × P.vel − min(0, dot(vel_cible, dir)) × dir + (0, 3, 0)`. Le second terme **annule la composante d'approche** de la cible, qui est ainsi désarçonnée au lieu de garder son élan vers toi. Norme plafonnée à `LANCE_KNOCK_MAX` (28). La composante verticale de 3 fait passer `ev->f[2] > 2` dans `apply_event`, donc la cible décolle (`grounded = 0`).
- Puis `P.vel = 0,5 × P.vel` (« tu en gardes 50 % », sur les trois axes).
- `P.meleeCd = LANCE_HIT_CD`, +1 dash, `melee_hit_points(meleePts[P.meleeKind])`.
- Si l'on était accroché à **cette** cible (`P.hookPlayer == i + 1` ou `P.hookEnemy == i + 1`), la joute **consomme** le grappin : `P.hook = HOOK_NONE`, `hookCd = 0`, `airJumps = 1`. Il n'y a pas d'impact `W_HOOK` en plus (voir § 7).

### Cas limites

- **Mort** : `lance_tick` remet `LS_IDLE` si `P.dead`. `EV_KILL` sur soi fait de même (déjà `P.hook = HOOK_NONE` à cet endroit).
- **Respawn** (`EV_RESPAWN`, `reset_game`) : `lanceState = LS_IDLE`, `lanceT = 0`, `meleeCd = 0`.
- **Hitstop** : `game_tick` sort avant `lance_tick`, donc les minuteries de la lance gèlent. C'est voulu : l'arrêt sur image d'une joute ne consomme pas les 2 s.
- **Changement d'arme à distance** (`A_SWAP`, `A_WEAP1/2`) : aucun effet, la mêlée est indépendante. Le **tir reste possible** lance couchée (rail à 15 m puis joute = 2 touches, comme rail + épée aujourd'hui).
- **Fin de match** : l'hôte ignore les `EV_HIT` (`matchOver`). Localement, la lance continue, sans conséquence.
- **Grappin, dash, slide, wall-run** : tous autorisés lance couchée, sans pénalité (question 3).

## 3. Constantes

```c
/* Lance de joute (MK_LANCE) */
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
```

## 4. Données

```c
/* Player */
int   lanceState;        /* LS_IDLE, LS_THRUST, LS_COUCH */
float lanceT;            /* temps depuis l'appui */
float lanceVc;           /* v_c de la meilleure cible du cone (HUD, viewmodel) */
float lanceHitT;         /* temps depuis la derniere joute (anti-doublon EV_LANCE) */
int   lanceHitWho;       /* id + 1 du joueur de la derniere joute */
enum { LS_IDLE, LS_THRUST, LS_COUCH };

/* NetPlayer */
double lastLanceT;       /* derniere fois vu lance couchee (ST_LANCE), comme lastParryT */
unsigned char lanceSeen; /* etat ST_LANCE precedent : front montant -> son */

/* hote */
static double joustT[MAXP][MAXP];   /* derniere joute resolue entre a et b */
```

Champs de `EV_HIT` quand `sub == W_LANCE` (sans changer la structure) : `f[0]` dégâts, `f[1..3]` recul, **`f[4]`** vitesse de l'attaquant `|P.vel|` *avant* transfert, **`f[5]`** `v_c`, **`f[6]`** 1 = lance couchée, 0 = estoc.

`EV_LANCE` (émis par l'hôte seulement) : `actor` vainqueur (ou premier joueur si les deux sont désarçonnés), `target` perdant, `sub` 1 = joute gagnée par `actor`, 2 = double désarçonnement, `f[0]` dégâts appliqués au perdant, `f[1..3]` recul du perdant, `f[4..6]` point de choc.

## 5. Algorithme

### `game_tick` (aiguillage)

```c
P.meleeCd -= dt; P.meleeT -= dt; P.lanceHitT += dt;
if (P.meleeKind == MK_LANCE) {
    if (!P.dead && meleePress && P.meleeCd <= 0 && P.lanceState == LS_IDLE) lance_press();
    lance_tick(dt, act[A_MELEE]);                 /* maintenu : act, pas actP */
} else if (!P.dead && meleePress && P.meleeCd <= 0) melee();
if (P.parryWin > 0) {
    if (!P.dead) try_parry();
    P.parryWin -= dt;
    if (P.parryWin <= 0 && !P.swingLanded && P.lanceState != LS_THRUST)   /* la lance decide a 0,15 s */
        P.meleeCd = fmaxf(P.meleeCd, MELEE_WHIFF_CD - PARRY_WINDOW);
}
```

`lance_tick` est appelé **après** `player_tick`, donc après `grapple_tick` (voir § 7 pour l'ordre avec l'impact du grappin).

### `lance_press` (estoc, nouvelle fonction calquée sur `melee`)

```c
static void lance_press(void) {
    V3 eye = eye_pos(), f = view_dir();
    int parried = try_parry(), hit = 0, best = -1, bestE = -1; float bd = 1e9f;
    /* une seule cible : la plus proche dans le cone de l'estoc */
    for (i in E vivants)  { reach = |E.pos-eye| - enemy_radius; if (reach <= LANCE_THRUST_RANGE && cos >= LANCE_THRUST_CONE && reach < bd) ... bestE = i; }
    for (i in np_target)  { reach = |np_center-eye| - 0.5f; meme test ... best = i; }
    if (bestE >= 0) { rel = rel_speed(E.vel); kamikaze -> damage_enemy(9999) + parry_feedback (comme melee);
                      sinon damage_enemy(bestE, LANCE_THRUST_DMG + LANCE_THRUST_PER_MS*rel, vmul(f, 8 + rel*0.5f)); hit = 1; }
    else if (best >= 0) { rel = rel_speed(np_vel(best));
        net_hit_lance(best, (LANCE_THRUST_DMG + LANCE_THRUST_PER_MS*rel) * PVP_SCALE,
                      vadd(vmul(f, 8 + rel*0.5f), v3(0,3,0)), vlen(P.vel), rel, 0); hit = 1; }
    if (dmMode) net_fx_melee(eye, f, vlen(P.vel));    /* EV_MELEE reutilise : effet visuel */
    P.lanceState = LS_THRUST; P.lanceT = 0;
    P.meleeT = 0.2f; P.parryWin = PARRY_WINDOW; P.swingLanded = hit || parried;
    if (hit) { P.dashCharge = fminf(2, P.dashCharge + 1); melee_hit_points(meleePts[P.meleeKind]); hitMarker = 0.18f;
               hitstop = fmaxf(hitstop, 0.04f); play(S_PUNCH, 0.9f, 0.8f); }
    play(S_LANCE_THRUST, 0.6f, 1.0f);
}
```

`net_hit_lance(t, dmg, knock, spd, vc, couched)` est un petit enrobage de `net_hit` qui remplit en plus `f[4..6]` (`net_hit` reste inchangé pour les autres armes).

### `lance_tick`

```c
static void lance_tick(float dt, int held) {
    if (P.dead) { P.lanceState = LS_IDLE; return; }
    if (P.lanceState == LS_IDLE) return;
    P.lanceT += dt;
    if (P.lanceState == LS_THRUST) {
        if (P.swingLanded || !held) {                    /* estoc seul */
            P.lanceState = LS_IDLE;
            P.meleeCd = P.swingLanded ? LANCE_THRUST_CD : MELEE_WHIFF_CD - P.lanceT;
            return;
        }
        if (P.lanceT < LANCE_TAP) return;
        P.lanceState = LS_COUCH; play(S_LANCE_COUCH, 0.7f, 1.0f);
    }
    /* LS_COUCH */
    if (!held || P.lanceT >= LANCE_HOLD_MAX) { P.lanceState = LS_IDLE; P.meleeCd = LANCE_MISS_CD; return; }
    V3 eye = eye_pos(), f = view_dir();
    int bi = -1, isP = 0; float br = 1e9f, bvc = 0; P.lanceVc = 0;
    for (chaque drone vivant i et chaque np_target i) {
        V3 c = ..., tv = ...; float rad = ...;
        V3 d = vsub(c, eye); float dist = vlen(d); V3 dir = vmul(d, 1/dist);
        if (vdot(dir, f) < LANCE_CONE) continue;
        float vc = vdot(vsub(P.vel, tv), dir);
        if (dist - rad < 30) P.lanceVc = fmaxf(P.lanceVc, vc);   /* apercu HUD */
        if (dist - rad > LANCE_REACH || vc < LANCE_VC_MIN) continue;
        if (dist - rad < br) { br = dist - rad; bi = i; isP = joueur; bvc = vc; }
    }
    if (bi < 0) return;
    lance_strike(bi, isP, bvc);                          /* effets du § 2 */
}
```

### `lance_strike(i, isP, vc)`

```c
V3 c = ..., tv = ..., dir = vnorm(vsub(c, eye_pos()));
float dmg = LANCE_DMG + LANCE_DMG_PER_MS * fminf(vc, LANCE_VC_CAP);
V3 knock = vsub(vmul(P.vel, LANCE_GIVE), vmul(dir, fminf(0, vdot(tv, dir))));
knock.y += 3; if (vlen(knock) > LANCE_KNOCK_MAX) knock = vmul(vnorm(knock), LANCE_KNOCK_MAX);
float spd = vlen(P.vel);
if (!isP) { damage_enemy(i, dmg, knock); if (!E[i].alive) P.hp = fminf(100, P.hp + 15); }
else { net_hit_lance(i, dmg * PVP_SCALE, knock, spd, vc, 1); P.lanceHitT = 0; P.lanceHitWho = i + 1; }
if ((isP && P.hookPlayer == i + 1) || (!isP && P.hookEnemy == i + 1)) {   /* la joute consomme le grappin */
    P.hook = HOOK_NONE; P.hookEnemy = P.hookPlayer = 0; P.hookCd = 0; P.airJumps = 1;
}
P.vel = vmul(P.vel, LANCE_KEEP);
P.lanceState = LS_IDLE; P.meleeCd = LANCE_HIT_CD;
P.dashCharge = fminf(2, P.dashCharge + 1); melee_hit_points(meleePts[P.meleeKind]);
lance_juice(c, dir, vc);                                 /* § 8 */
```

Un kamikaze touché par la lance couchée meurt normalement (62 dégâts au minimum contre 50 PV), sans `parry_feedback`. Ce n'est pas un parry.

### `pack_state`

```c
s->pad = (unsigned char)((P.meleeKind & 0x0F) | (P.lanceState == LS_COUCH ? ST_LANCE : 0) | ...autres ST_*);
s->melee = P.meleeKind == MK_LANCE ? (unsigned char)(clampf(P.meleeT / 0.2f, 0, 1) * 255) : ...;   /* estoc */
```

## 6. Réseau

**Qui calcule quoi.** L'attaquant détecte la touche (estoc ou couchée) sur `NP[i].disp` et `np_vel(i)`, applique ses propres effets (vitesse × 0,5, recharge, dash, munition, juice) et envoie `EV_HIT` (`sub = W_LANCE`, `f[4..6]` remplis). La victime applique le recul reçu (chemin existant de `apply_event`). L'hôte arbitre PV, parry, choc de joute et kills.

### `host_recv` / `host_tick` : suivi de `ST_LANCE`

```c
/* host_recv, PK_STATE */ if (NP[id].st.pad & ST_LANCE) NP[id].lastLanceT = nowT;
/* host_tick */            if (P.lanceState == LS_COUCH) NP[myId].lastLanceT = nowT;
```

### `host_handle`, branche `EV_HIT` (ordre des tests)

1. Cible absente, morte ou match fini → ignorer (existant).
2. **Parry** : `W_LANCE` rejoint la liste des coups parables (`W_MELEE`, `W_HOOK`, `W_LANCE`…). Victime en fenêtre (`lastParryT` < `MELEE_PARRY_GRACE`) → `EV_MELEE_PARRY`. Le *choc d'épées* s'applique à l'estoc (`f[6] == 0`) si l'attaquant parait aussi, jamais à la lance couchée (pas de fenêtre de parry).
3. **Choc de joute** (seulement si `f[6] == 1`) :

```c
int a = ev->from, b = ev->target;
if (nowT - joustT[a][b] < LANCE_JOUST_LOCK) return 1;            /* touche reciproque d'une joute deja jugee */
V3 fb = dir_of(NP[b].st.yaw, NP[b].st.pitch);                    /* meme formule que view_dir */
V3 ba = vnorm(vsub(np_center(a), np_center(b)));
if (nowT - NP[b].lastLanceT < LANCE_CLASH_GRACE && vdot(fb, ba) >= LANCE_CLASH_COS) {
    float sa = ev->f[4], sb = vlen(np_vel(b));                   /* "le plus rapide" : vitesse propre */
    joustT[a][b] = joustT[b][a] = nowT;
    if (sa > sb + LANCE_CLASH_DIFF) { /* a gagne : on applique la touche, puis EV_LANCE sub 1 actor a target b */ }
    else if (sb > sa + LANCE_CLASH_DIFF) {                        /* b gagne : touche de a annulee */
        float dmg = fminf((LANCE_DMG + LANCE_DMG_PER_MS * fminf(ev->f[5], LANCE_VC_CAP)) * PVP_SCALE, PVP_HIT_CAP);
        V3 kn = vmul(np_vel(b), LANCE_GIVE) (+3 en y, plafond 28);
        NP[a].hp -= dmg; emettre EV_LANCE sub 1 actor b target a f[0] dmg f[1..3] kn; apply_event;
        if (NP[a].hp <= 0) host_kill(b, a, W_LANCE);
        return 1;                                                 /* EV_HIT annule */
    } else { emettre EV_LANCE sub 2 actor a target b; apply_event; clashes++; return 1; }
}
```

4. Sinon : dégâts plafonnés à 60, kill (existant).

`v_c` est symétrique (`dot(vA − vB, dirAB)` est le même vu de A ou de B). Le vainqueur désigné par l'hôte reçoit donc les dégâts calculés avec le `v_c` du perdant, sans attendre sa propre touche.

**Prérequis découvert dans le code** : aujourd'hui, un `EV_HIT` paré est quand même rejoué. `host_recv` appelle `host_handle` puis **toujours** `apply_event` et `net_queue`, et `net_emit` met aussi en file un coup de l'hôte. La victime d'un coup d'épée paré **reçoit donc le recul** et le flash rouge. Avec les 28 m/s de la lance, ce n'est plus acceptable. `host_handle` doit renvoyer 1 quand il annule un `EV_HIT` (parry, choc de joute, verrou), et `host_recv` / `net_emit` ne doivent alors ni l'appliquer ni le relayer (question 8 : cette correction relève-t-elle de l'infra ?).

### `apply_event`

- `EV_MELEE_PARRY`, côté attaquant : `P.meleeCd = fmaxf(P.meleeCd, P.meleeKind == MK_LANCE ? LANCE_PARRIED_CD : MELEE_PARRIED_CD)` et `P.lanceState = LS_IDLE`. Le recul de 11 existe déjà. Comme l'attaquant a déjà divisé sa vitesse par deux, être paré revient en pratique à être arrêté puis renvoyé : c'est lisible.
- `EV_LANCE` (nouveau) :

```c
case EV_LANCE: {
    int w = ev->actor, l = ev->target; V3 at = fv(ev->f + 4);
    burst(at, ...); play(S_LANCE_CLASH, atten(at), 1);
    if (ev->sub == 2) {                                   /* double desarconnement */
        if (w == myId || l == myId) { V3 o = np_center(w == myId ? l : w);
            P.vel = vadd(vmul(vnorm(vsub(pcenter(), o)), LANCE_UNHORSE), v3(0, 3, 0));   /* remplace la vitesse */
            P.grounded = 0; P.meleeCd = fmaxf(P.meleeCd, LANCE_HIT_CD); P.lanceState = LS_IDLE;
            popText = "DESARCONNES !"; hitstop 0.1; impact_frame(at, 0.1f); }
    } else {
        if (l == myId) { P.vel = vadd(P.vel, fv(ev->f + 1)); P.grounded = 0;
            P.meleeCd = fmaxf(P.meleeCd, LANCE_PARRIED_CD); P.lanceState = LS_IDLE; popText = "DESARCONNE PAR %s"; }
        if (w == myId && !(P.lanceHitT < LANCE_JOUST_LOCK && P.lanceHitWho == l + 1)) {
            /* vainqueur designe par l'hote sans avoir detecte sa touche : effets de lance_strike */
            P.vel = vmul(P.vel, LANCE_KEEP); P.meleeCd = LANCE_HIT_CD; dash +1; melee_hit_points(meleePts[P.meleeKind]); juice;
        }
    }
    if (l < MAXP) NP[l].flashT = 0.12f;
}
```

**Tolérance de latence** : `disp` est extrapolé de 0,1 s au plus, et les états partent 60 fois par seconde. `LANCE_CLASH_GRACE` = 0,15 s, la même valeur que le parry. `LANCE_CLASH_COS` (0,85 au lieu de 0,92) absorbe l'écart entre le cône vu par B et le `yaw` reçu par l'hôte. `EV_HIT` et l'état du même joueur partent dans le même envoi : quand la touche de A arrive, `np_vel(b)` est encore la vitesse de B **avant** sa propre joute. C'est ce qui permet de juger sans attendre.

## 7. Interactions

- **Grappin sur un joueur (traction jusqu'à 46 m/s)**. Dans `game_tick`, `player_tick` (et donc `grapple_tick`) passe **avant** `lance_tick`. Mais l'impact du grappin se déclenche à `|pcenter − np_center| < 0,5 + 1,6 = 2,1 m`, alors que la lance part dès `|eye − c| − 0,5 ≤ 3,5`, soit à environ 3,9 m entre les centres (l'œil est 0,7 m au-dessus du centre). À 46 m/s (0,38 m par tick), la lance touche **environ 5 ticks avant** l'impact : **la lance passe en premier** si on regarde la cible. Elle consomme alors le grappin (pas de `W_HOOK` en plus). Sinon, lance (60) + impact (60) en 40 ms ferait un kill garanti à travers une seule fenêtre de parry. Si la vue n'est pas dans le cône, l'impact `W_HOOK` normal a lieu.
  Exemple : cible immobile, traction à 40 m/s, donc v_c = 40, plafonné à 30, ce qui donne 160 PvE / 60 PvP. La cible reçoit 0,6 × 40 = 24 m/s, et toi tu repars à 20 m/s.
- **Parry** : la victime pare la lance couchée par **timing** sur l'approche. Fenêtre utile : 0,12 s + 0,15 s de grâce = 0,27 s avant la touche, soit une approche de 5,4 m à v_c = 20 (le pareur doit appuyer quand le lancier est entre 8,9 et 3,5 m) et de 10,8 m à v_c = 40. L'estoc pare les projectiles (`try_parry`) comme l'épée.
- **Pièces** : la lance ne les touche pas (question ouverte du Fléau seulement).
- **Autres armes** : contre l'Égide levée (fiche 05), il faut décider si la lance couchée est bloquée (question 9). Contre le piqué de l'Enclume, rien de particulier.
- **Recharge de munitions** : touche (estoc ou couchée) → `melee_hit_points(meleePts[P.meleeKind])` chez l'attaquant ; joute gagnée par décision de l'hôte → dans `EV_LANCE` ; kill → `PTS_MELEE_KILL` via `EV_KILL` (infra) ; un parry *avec* l'estoc → `melee_parry_points` (infra).
- **Drones (solo)** : un drone tourne à environ 13 m, et un dash de 18 m/s droit dessus donne v_c ≈ 18, soit 100 dégâts, donc tué. Un **kamikaze** fonce à 10,5–13,5 m/s : une lance couchée **à l'arrêt** suffit (v_c ≥ 10,5, 62 dégâts contre 50 PV), avant ses 18 dégâts au contact (< 1,4 m). Le recul transmis (`damage_enemy` ajoute `knock` à `e->vel`) projette le drone.

## 8. Rendu et son

- **`draw_viewmodel`** (si `P.meleeKind == MK_LANCE`, à la place de la lame) :
  - *levée* (`LS_IDLE`) : hampe `box_faces` de 0,05 × 0,05 × 2,4, inclinée de 60° vers le haut à droite, rondelle (vamplate) de 0,16 de côté, pointe de 0,25 ;
  - *estoc* : même pose, translation `-0,5 × sin(π × k)` sur z (k = avancement de `meleeT` / 0,2) ;
  - *couchée* : la lance s'abaisse en 0,1 s jusqu'à l'axe de visée, légèrement à droite. La pointe (non éclairée) passe du bleu à l'orange quand `P.lanceVc ≥ LANCE_VC_MIN` (« armée »), puis au blanc à 15 et plus (plafond PvP). Fanion qui ondule (`sinf(gameTime × 20)`). Barre de 2 s qui se vide le long de la hampe.
- **`draw_remote_players`** : `meleeKind = st.pad & 0x0F`. Avec `ST_LANCE`, la lance horizontale de 2,6 m suit `st.pitch`, et sa pointe brille en orange : c'est le **signal pour parer**. Sans `ST_LANCE`, lance verticale au flanc. `st.melee > 0` : estoc (translation avant).
- **HUD (`draw_hud`)** : le panneau « EPEE » devient « LANCE ». Une `lance_preview` (variante de `melee_preview`, qui utilise déjà le cos 0,92) affiche `v_c` vers la cible visée et les dégâts (`(10 + 5·v_c) × pvp`, plafond 60 affiché), en orange au-dessus de 10. Lance couchée : un **anneau du cône** autour du viseur, de rayon `tan(23,1°) / tan(hfov / 2) × winW / 2` (0,327 × la demi-largeur à 105°, environ 314 px en 1920), qui devient plein quand une cible y est armée.
- **Juice (`lance_juice`)** : `impact_frame(c, 0.08f)` si v_c ≥ 15 ; `hitstop = fmaxf(hitstop, fminf(0.06f + 0.002f × vc, 0.14f))` ; `shake += 0.3f` ; `P.fovKick += 10` ; 40 particules le long de `dir` ; `shards(c, …, 10, 12)` ; `dmg_number` (par `net_hit`). Choc de joute : impact frame de 0,1 s centré sur le point de choc, gerbe d'étincelles blanches.
- **Sons (`make_sounds`, avant `S_COUNT`)** :
  - `S_LANCE_COUCH` (0,4 s) : glissando métallique de 300 à 900 Hz, deux sinus désaccordés de 1,5 %, enveloppe d'attaque de 0,05 s puis déclin `expf(-t*6)`. Joué localement, et chez les autres sur le **front montant** de `ST_LANCE` (`NP[i].lanceSeen`, avec `atten`) ;
  - `S_LANCE_THRUST` (0,15 s) : bruit filtré passe-bande, montée rapide (le « fsst » de l'estoc) ;
  - `S_LANCE_HIT` (0,5 s) : `S_PUNCH` grave (140 → 60 Hz) + cloche à 620 / 930 / 1480 Hz en `expf(-t*7)` + bruit court (craquement de bois) ;
  - `S_LANCE_CLASH` (0,8 s) : deux cloches désaccordées décalées de 30 ms + bruit large en `expf(-t*25)`.

## 9. Bots (`bot_think`)

Si `P.meleeKind == MK_LANCE`, avant la logique de tir :

```c
/* parer une lance couchee qui arrive (50 %) */
pour chaque np_target i avec (NP[i].st.pad & ST_LANCE) et la lance qui me vise (cos >= 0.85) :
    vc = vdot(vsub(np_vel(i), P.vel), dir_i_vers_moi); d = distance - 0.5f;
    if (vc > 8 && d - LANCE_REACH < vc * 0.12f && P.meleeCd <= 0 && decide(0.5)) { viser i; botPress[A_MELEE] = 1; return; }
    sinon, 30 % des fois : pas de cote (botHeld[A_LEFT ou A_RIGHT] = 1) quand d < 8.
/* jouter */
vc = vdot(vsub(P.vel, np_vel(best)), dir_vers_best);
if (see && bd < 20 && (vc > 6 || P.hookPlayer)) { swing = 0; P.yaw = aimYaw; P.pitch = aimPitch; botHeld[A_MELEE] = 1;
    if (bd < 12 && vc < 12 && P.dashCharge >= 1) botPress[A_DASH] = 1; }    /* dash pour armer la lance */
if (bd < LANCE_THRUST_RANGE + 0.5f && vc < LANCE_VC_MIN && P.meleeCd <= 0) botPress[A_MELEE] = 1;   /* estoc au contact */
```

Le grappin sur la cible existe déjà (`bd > 10`) ; avec la lance, le bot **tient** `A_MELEE` pendant toute la traction. Journal : `netlog("joute : v_c %.1f -> %s", ...)`, `"lance paree"`, `"choc de joute (%s)"`.

## 10. Tests (`run_tests`)

Couloir dégagé x = −20. On appelle `lance_press` et `lance_tick(TICK, held)` directement, et on déplace `P.pos += P.vel × TICK` (ou `player_tick`, pour les cas avec grappin).

1. **Joute contre un drone** : `P.vel = (0,0,−20)`, drone immobile en (−20 ; 1,6 ; 30). Appui, maintien. Attendu : touche à `reach ≤ 3,5`, drone mort (110 > 100), `|P.vel|` = 10, `meleeCd` = 1,0, dash +1.
2. **v_c insuffisante** : `P.vel` = 8. Attendu : aucune touche en 2 s, puis `LS_IDLE` et `meleeCd` = 0,5.
3. **Cône** : drone décalé de 2 m latéralement à 3 m (33,7°). Attendu : aucune touche.
4. **Tap = estoc** : `lance_press`, puis `lance_tick(TICK, 0)`, drone à 2,5 m. Attendu : PV 70, `parryWin` = 0,12, `lanceState` = `LS_IDLE`, `meleeCd` = 0,5.
5. **Kamikaze** : joueur immobile, lance couchée, kamikaze qui arrive à 10,5 m/s. Attendu : kamikaze mort, PV joueur 100.
6. **LAN, plafond** : couchée à v_c = 15 puis 20 sur `NP[1]`. Attendu : PV 40,5 puis 40 (59,5 ; plafond 60).
7. **Parry de la lance** : `NP[1].lastParryT = nowT`, `EV_HIT` `W_LANCE` `f[6]=1`. Attendu : PV 100, `meleeParries` +1 ; côté attaquant (`apply_event`) `meleeCd` = 1,5. **La victime ne reçoit pas le recul** (vérifie la correction du § 6).
8. **Choc de joute** : `NP[1].lastLanceT = nowT`, `NP[1].st.yaw` tourné vers moi. `f[4]` = 20 contre `|np_vel(1)|` = 15, puis 19, puis 25. Attendu : je gagne (PV 1 → 40) ; double désarçonnement (PV inchangés, `clashes` +1) ; il gagne (mes PV → 100 − 0,7 × (10 + 5 × v_c), plafonné). Puis un `EV_HIT` réciproque dans les 0,3 s est ignoré.
9. **Grappin + lance** : grappin sur `NP[1]` à 15 m, lance tenue, vue sur la cible. Attendu : `hitCount[W_LANCE]` = 1, `hitCount[W_HOOK]` = 0, `P.hook` = 0, `hookCd` = 0. Sans viser : `W_HOOK` = 1.
10. **Bot** : la passe « épée » existante, rejouée avec `meleeKind = MK_LANCE` (la cible fonce à 10 m/s, le bot à 10, donc v_c = 20). Attendu : au moins une touche `W_LANCE`, PV de la cible 40.

## 11. Équilibrage

| Situation | v_c ou v_rel | PvE | PvP (× 0,7, plafond 60) | Touches pour tuer (PvP) |
|---|---|---|---|---|
| Couchée, seuil | 10 | 60 | 42 | 3 |
| Couchée | 12,3 | 71,4 | **50** | 2 |
| Couchée, bhop contre cible immobile | 15 | 85 | 59,5 | 2 |
| Couchée, plafond PvP atteint | 15,14 | 85,7 | 60 | 2 |
| Couchée, deux coureurs face à face (12 + 12) | 24 | 130 | 60 | 2 |
| Couchée, grappin (46, plafonné) | 30 | 160 | 60 | 2 |
| Estoc à l'arrêt | 0 | 30 | 21 | 5 |
| Estoc | 15 | 60 | 42 | 3 |
| Estoc, plafond | 27,9 | 85,7 | 60 | 2 |

- Drone (100 PV) : tué en une joute dès v_c ≥ 18. Kamikaze (50 PV) : toujours tué.
- Comparaison : l'épée atteint 60 PvP dès v_rel 12,7, l'impact du grappin dès 12. La lance couchée l'atteint dès v_c 15,1, mais avec une **portée fixe de 3,5 m**, la touche **automatique** et le transfert de vitesse. Le prix : pas de parry, 2 s d'engagement, 1,0 s de recharge.
- **À mesurer dans le journal du bot** : `hitCount[W_LANCE]` (en séparant couchée et estoc, avec `f[6]` dans le `netlog` des dégâts), v_c moyen des joutes réussies, part des lances couchées sans touche, `meleeParries` sur `W_LANCE`, chocs de joute (`clashes` et issue), `ttkSum` / `ttkN` et `oneShots` (doit rester à 0), part des joutes servies par le grappin.

## 12. Étapes d'implémentation

1. Constantes, champs `Player` / `NetPlayer`, `S_LANCE_*` dans `make_sounds` (**S**, 0,5 h).
2. `lance_press` (estoc) + aiguillage dans `game_tick` + exception du coup dans le vide (**S**, 1 h).
3. `lance_tick` + `lance_strike` en solo, avec les tests 1 à 5 (**S-M**, 1,5 h).
4. Correction « `EV_HIT` annulé ni appliqué ni relayé » (`host_handle` renvoie 1) + `W_LANCE` parable + `LANCE_PARRIED_CD` dans `EV_MELEE_PARRY`, avec les tests 6 et 7 (**S**, 1 h).
5. `ST_LANCE` dans `pack_state`, `lastLanceT`, choc de joute dans `host_handle`, `EV_LANCE` dans `apply_event`, avec le test 8 (**M**, 2 h).
6. Interaction avec le grappin (consommation de l'accroche), test 9 (**S**, 0,5 h).
7. Viewmodel, joueurs distants, HUD (`lance_preview`, anneau du cône), juice (**M**, 2 h).
8. `bot_think` (joute, parry de la lance, pas de côté), test 10, match de bots de 60 s (**S-M**, 1,5 h).

Total : environ 10 h (coût **M**, conforme au document d'idées).

## 13. Questions ouvertes

1. **Estoc à l'appui** (choix de la fiche : parry instantané, et une lance couchée commence par un estoc) ou **au relâchement** (règle littérale, mais l'estoc et sa fenêtre de parry arrivent 0,15 s plus tard) ?
2. Recharge après une lance couchée **sans touche** : 0,5 s proposé. Faut-il plus (1,0 s), pour punir les 2 s de maintien ?
3. Lance couchée **sans pénalité de mouvement** (choix de la fiche) ou avec un malus (pas de slide, accélération aérienne × 0,7) pour marquer l'engagement ?
4. **Une seule cible par joute** (choix de la fiche). En solo, faut-il permettre de transpercer des drones alignés (perforation, comme le rail) ?
5. Grappin + lance : la joute **remplace** l'impact du grappin (choix de la fiche) ; faut-il autoriser les deux (kill quasi garanti) ?
6. Recul de la cible : **annuler son approche** puis ajouter 60 % de ta vitesse, plafonné à 28 m/s (choix de la fiche), ou simple ajout de 60 % de ta vitesse ?
7. Choc de joute : « le plus rapide » mesuré par la **vitesse propre** `|vel|` (choix de la fiche) ou par la composante vers l'adversaire ?
8. La correction « un `EV_HIT` paré n'est ni appliqué ni relayé » (le défaut existe déjà pour l'épée) doit-elle aller dans la fiche **00_infra**, pour toutes les mêlées ?
9. Égide levée face à une lance couchée : blocage normal (consomme 60 de jauge), ou la lance **perce** le bouclier si v_c ≥ 20 ?
10. Une touche finalement **parée** garde-t-elle son `melee_hit_points` (accordé localement avant la décision de l'hôte, comme l'épée aujourd'hui) ?
11. `LANCE_VC_CAP` = 30 (160 PvE au maximum) : convient-il au score solo, qui utilise déjà la vitesse relative ?
