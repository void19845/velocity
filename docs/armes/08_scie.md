# Fiche 08 : Disque-boomerang « Scie »

> Source : [`../IDEES_ARMES_CARTES.md`](../IDEES_ARMES_CARTES.md) § 3.3 (version validée), § 1.2 et § 1.5. Interfaces figées : [`PLAN.md`](PLAN.md) § 3, conventions de la relecture croisée : `PLAN.md` § 4.
> Identifiants : `WP_SAW 4`, `W_SAW 14`, `EV_SAW 16`, `PR_SAW 3`. Loadout, `P.ammo[]`, jauges de points et bascule « chargeur vide » : voir `00_infra_loadout_munitions.md` (non redéfinis ici).
> Mise à jour du 27/10 : `SAW_REL_OFFSET` = 12 (3 touches contre une cible immobile) ; nouvelle mécanique **renvoi** (parer sa propre scie qui revient), dégâts du renvoi × 0,9 ; munitions en jauges de points, dont la Scie reste exclue.
> Document de design : rien n'est codé, `main.c` n'est pas modifié.

---

## 1. Résumé validé

- **Rôle** : une lame unique qu'on lance et qu'il faut **aller rechercher**. Toucher la rend tout de suite ; rater désarme de cette arme jusqu'à la récupération, donc pousse au corps à corps.
- **Lancer** : vitesse `32 m/s + composante de P.vel dans l'axe de visée`, gravité 6 m/s² (≈ 1,4 m de chute sur 22 m). **Une seule scie**, sans jauge ni recharge : la scie *est* la munition.
- **Touche un joueur (ou un drone en solo)** : dégâts `30 + 1,5 × max(0, |v_scie − v_cible| − 12)`, puis **retour automatique** vers le lanceur (36 m/s, guidé, traverse le décor, sans dégâts). Rattrapée à moins de 1,5 m : +0,5 dash.
- **Renvoi** (validé le 27/10) : pendant sa fenêtre de parry, le lanceur peut **parer sa scie qui revient** pour la relancer aussitôt, ×1,5 plus vite que le lancer, plus sa vitesse, avec une légère aide à la visée (7°) ; ses dégâts sont multipliés par 0,9 (décision du 27/10). Elle n'est alors pas rattrapée : elle repart en vol, et peut toucher… ou se planter.
- **Touche le décor** : **reste plantée** dans la face de l'AABB, sans limite de temps, sans rebond. Visible par tous, inoffensive.
- **Récupération** : à pied (< 1,5 m) ou **au grappin** : le crochet qui touche **sa** scie plantée (sphère 0,6 m) la rappelle à 45 m/s **sans déplacer le joueur** (crochet décroché aussitôt, ni traction ni balancement ; la scie traverse le décor). Option désactivée par défaut : la scie rappelée blesse le premier adversaire traversé.
- **Mort du lanceur** : la scie disparaît, scie neuve au respawn. **Sous le plan de mort** : revient au lanceur après 3 s.
- **Pièces** : la scie en vol qui frôle une pièce la relance vers le haut (+2 s de vie).
- **Parry adverse** : projectile parable (`try_parry`). Parée, elle repart ×1,25 **vers son lanceur** : s'il est touché, il prend les dégâts mais la rattrape ; sinon elle se plante.
- **Chiffres** (décision du 27/10, 3 touches contre une cible immobile) : cible immobile (rel 32) → **60 / 42 PvP** ; rel ≤ 12 → 30 / 21 ; rel ≥ 49,1 → plafond 60 PvP. Recharge de lancer 0,4 s.

## 2. Règles détaillées

### 2.1 Machine à états

L'état « en main » est porté par le joueur (`P.ammo[WP_SAW] == 1`, `P.sawIdx == 0`). Dès qu'elle est lancée, la scie est un `Proj` de `kind = PR_SAW` dont `sawState` vaut :

| État | Mouvement | Collisions | Sortie |
|---|---|---|---|
| `SAW_FLY` (en vol, y compris après un renvoi) | balistique, `vel.y -= 6·dt` | décor → `PLANTED` ; joueur/drone → dégâts puis `RETURN` ; pièce → relance | `y < killY` → `LOST` ; > 12 s → `LOST` |
| `SAW_PARRIED` (parée par un adversaire) | balistique depuis le parry, vers le lanceur | décor → `PLANTED` ; lanceur (vu par celui qui a paré) → dégâts `W_PARRY` | **renvoi** par le lanceur → `FLY` ; à < 1,5 m → rattrapée |
| `SAW_PLANTED` | immobile | aucune ; crochet du lanceur → `RECALL` | lanceur à < 1,5 m → rattrapée |
| `SAW_RETURN` (retour) | guidée 36 m/s vers `pcenter()` du lanceur | aucune (traverse tout) | **renvoi** → `FLY` ; < 1,5 m → rattrapée ; > 2,5 s → rattrapée d'office |
| `SAW_RECALL` (rappel) | guidée 45 m/s | aucune (option : 1er adversaire) | **renvoi** → `FLY` ; < 1,5 m → rattrapée ; > 2,5 s → rattrapée d'office |
| `SAW_LOST` (perdue) | invisible | aucune | après 3 s → rattrapée |

« Rattrapée » = `saw_catch(how)` : le `Proj` est libéré, `P.ammo[WP_SAW] = 1`, `P.sawIdx = 0`, compteur de renvois remis à 0.

### 2.2 Durées et conditions exactes

- **Cadence** : un lancer fixe `P.sawCd = SAW_CD` (0,4 s). Rattraper **n'ajoute aucun délai** (c'est la « recharge à 0 » du document) : au contact, la scie revient en 1 ou 2 ticks, et le plancher de 0,4 s évite de la relancer 60 fois par seconde. Tenir le clic relance dès qu'elle revient en main. Le renvoi ne passe pas par `sawCd` : il est limité par la recharge de mêlée et par les garde-fous du § 2.3.
- **Vitesse de lancer** : `s = SAW_SPEED + dot(P.vel, f)`, bornée à `SAW_MIN_SPEED` (20 m/s) pour qu'un lancer en reculant à pleine vitesse ne tombe pas aux pieds. Seule la composante dans l'axe est héritée, pas la composante latérale.
- **Origine** : `eye_pos()`, comme `fire_rocket`. Si l'œil est collé à un mur, la scie se plante tout de suite devant soi et sera ramassée à pied : c'est sans conséquence.
- **+0,5 dash** seulement au rattrapage **après une touche** (`how == SAW_HOW_RETURN`), pas après un rappel ni un ramassage.
- **Plafond de vol** : rien ne borne la durée de vol dans le document, mais un lancer à la verticale sur l'arène à ciel ouvert monte à 85 m et retombe après ≈ 10,7 s. Le garde-fou `SAW_FLY_MAX` (12 s) le passe en `LOST`.

### 2.3 Renvoi : parer sa propre scie

**Conditions**, testées dans `try_parry`, donc à chaque tick de la fenêtre `P.parryWin` (0,12 s) :

- la scie est **la mienne** (`owner == myId`), en `SAW_RETURN`, `SAW_RECALL` ou `SAW_PARRIED` ;
- elle est à portée de parry de l'arme de mêlée tenue (`try_parry(range, speedMult)`, § 4.2 du plan : 3,9 m pour l'épée, 6 m pour le Fléau), réduite de 15 % par renvoi déjà fait (`PARRY_RANGE_SHRINK ^ sawBounces`) ;
- elle est devant moi (`cos ≥ 0,3`, la règle actuelle de `try_parry`) : il faut faire face à la scie qui arrive ;
- `sawBounces < SAW_BOUNCE_CHAIN` (3).

**Fenêtre de timing** : à l'épée, le renvoi part si l'on frappe quand la scie est entre 1,5 m (rayon de rattrapage) et `3,9 + 36 × 0,12` = 8,2 m, soit **0,19 s** à 36 m/s (0,15 s à 45 m/s pour un rappel). Dans `game_tick`, `melee()` et `try_parry` passent **avant** `proj_tick` : le renvoi l'emporte sur le rattrapage du même tick.

**Les trois états de retour** :
- `SAW_RETURN` (après une touche) : oui, c'est le cas principal, la boucle « touche → retour → renvoi → touche ».
- `SAW_RECALL` (rappel au grappin) : **oui**. Une scie plantée au loin devient une relance à longue distance : grappin, puis parry au passage. Comme la boucle « plantée → rappel → renvoi » ne rapporte rien sans cible (voir les gains), elle ne se farme pas. Elle coûte un grappin (0,5 s) et un coup de mêlée.
- `SAW_PARRIED` (renvoyée vers moi par un adversaire) : **oui**, c'est le ping-pong. Au lieu de la prendre dans la figure, on la renvoie. La latence est la même que pour les roquettes : si l'adversaire a déjà jugé la touche chez lui, on prend les dégâts **et** la scie repart (voir § 6).

**Vitesse** : `v = (SAW_SPEED × SAW_BOUNCE_MULT × speedMult + max(0, dot(P.vel, aim))) × aim`, bornée à `SAW_BOUNCE_MAX`. À l'arrêt, à l'épée : 48 m/s (lancer 32, roquette 38, `PARRY_SPEED` 36) ; en courant à 9,5 m/s : 57,5 ; plafond 70. La vitesse **n'augmente pas** d'un renvoi à l'autre : pas d'escalade façon ping-pong de roquette.

**Aide à la visée** : on prend la cible (joueur `np_target` ou drone vivant) dont le point **anticipé** est le plus proche du viseur, s'il est à moins de 7° (`SAW_BOUNCE_AIM_COS` 0,9925), à moins de 40 m et en vue (`los`). C'est plus serré que l'aide du parry des projectiles adverses (`PARRY_AIM_COS` 0,988, soit 8,9°). Le point anticipé est `c + v_cible × t + (0, ½ × 6 × t², 0)` avec `t = d / |v|` : vitesse de la cible et chute de la scie. Sans cible dans le cône, la scie part droit dans l'axe du viseur.

**Dégâts** (décision du 27/10) : formule par vitesse relative, puis **× 0,9** (`SAW_RETURN_PARRY_DMG_MULT`), pour **tous** les renvois (retour, rappel, scie parée), en PvE comme en PvP. Le multiplicateur s'applique à toute touche d'une scie dont `sawBounces > 0`.
- **Cible immobile, lanceur immobile** : 84 × 0,9 = 75,6 PvE / 52,9 PvP (contre 60 / 42 au lancer). Le combo lancer + renvoi fait 42 + 52,9 = **94,9** : la cible survit à 5,1 PV. C'est le but de la décision.
- **Lanceur en course** : le combo **tue encore**, car la vitesse reste la ressource :
  - lancer en courant (52) + renvoi à l'arrêt (52,9) = 104,9 ;
  - lancer à l'arrêt (42) + renvoi en courant (57,5 m/s → 98,3 × 0,9 = 88,4 PvE → 60 PvP au plafond) = 102 ;
  - les deux en course : 52 + 60 = 112.
- **Seuils** : après un lancer à l'arrêt, il faut au moins 58 PvP au renvoi pour tuer, soit une vitesse relative ≥ 53,4 m/s. Cela correspond à un lanceur qui avance à 5,4 m/s ou plus dans l'axe, contre une cible immobile. Le renvoi atteint le plafond de 60 PvP à partir d'une vitesse relative de 55,5 m/s.

**Ce que le renvoi rapporte et ce qu'il coûte** :
- **Pas de `parry_feedback` complet**. Parer sa propre scie se fait sans adversaire (rappel au grappin) et serait farmable. Donc ni soin, ni dash, ni grappin rechargé, ni points de parry. À la place, `saw_bounce_fx` : hitstop 0,05 s, éclat blanc, texte `RENVOI` (`RENVOI x2`, `x3`), son.
- **Si la scie renvoyée touche** : cela compte comme un parry réussi, avec les points de parry du § 1.5 (+50) via la fonction commune de l'infra. Les points vont à l'autre arme du loadout, puisque la Scie n'a pas de jauge. On garde aussi le +0,5 dash habituel au rattrapage après la touche.
- **Coût** : un coup de mêlée (`MELEE_CD` 0,45 s, pas de pénalité de coup dans le vide car `swingLanded = 1`). Surtout, **la scie n'est pas rattrapée** : si le renvoi rate, elle se plante et l'on est désarmé. On échange une récupération sûre contre une touche forte.

**Garde-fous de l'enchaînement** :
1. Au plus **3 renvois consécutifs** sans rattrapage (`sawBounces`, remis à 0 par `saw_catch`). La lame rougit à chaque renvoi ; au 4ᵉ retour, elle ne peut plus qu'être rattrapée.
2. Portée de renvoi réduite de 15 % par renvoi : 3,9 → 3,3 → 2,8 m à l'épée.
3. Vitesse constante, sans escalade ; dégâts × 0,9, identiques d'un renvoi à l'autre (pas de bonus de ping-pong). Deux renvois successifs sur une cible immobile font tout de même 52,9 + 52,9 = 105,8 PvP : la chaîne tue en 2 renvois, mais il faut les réussir tous les deux sans rattraper la scie.
4. Ni soin ni dash au renvoi lui-même.
5. Une scie plantée ou en vol ne peut pas être renvoyée : seulement une scie qui revient.

### 2.4 Cas limites

- **Mort** (`EV_KILL` vers moi, ou `damage_player` en solo) : `saw_remove_owner(myId)`, la scie disparaît dans tous les états. Chez les autres, `EV_KILL` retire aussi les `PR_SAW` dont `owner == victime`. **Respawn** (`EV_RESPAWN`) : l'infra remet `P.ammo[WP_SAW] = 1` ; on refait `saw_remove_owner(t)` par sécurité (une réémission tardive ne doit pas laisser de fantôme).
- **Changement d'arme pendant que la scie est dehors** : la scie continue sa vie (c'est un objet, pas un tir). Elle peut revenir quand une autre arme est en main : `P.ammo[WP_SAW]` repasse à 1 sans rebasculer. Le renvoi marche quelle que soit l'arme à distance en main, puisqu'il passe par la touche de mêlée.
- **Clic avec l'emplacement vide** :
  - `FLY`, `PARRIED`, `RETURN`, `RECALL`, `LOST` : rien (pas de clic sec), car la scie revient d'elle-même. Le tir est « tamponné » : clic tenu → lancer dès le retour.
  - `PLANTED` : sur un **appui** (`lmbp`, pas en tenu), clic sec, message `SCIE PLANTEE (18 m)`, puis la bascule automatique de l'infra vers l'autre arme si elle a des munitions (§ 1.5).
- **Hitstop** : `game_tick` sort avant `proj_tick` : la scie se fige avec le monde. Pas de cas particulier.
- **Fin de match** : l'hôte annule les `EV_HIT` (`matchOver`). Au match suivant, `host_start_match` → `host_respawn` → nettoyage par `EV_RESPAWN`.
- **Départ d'un joueur** (`EV_INFO` sub 1) : retirer ses `PR_SAW`.
- **Scie plantée dans un pad** (`B_PAD`) : plantée comme ailleurs. Celui qui vient la chercher est lancé par le pad, mais le ramassage à 1,5 m se fait au bord du pad, en général avant. Rien à coder.
- **Scie plantée dans une boîte destructible** (`B_GLASS`, § 4.1 du document d'idées, pas encore codée) : on mémorise l'index de la boîte (`p->sawBox`, via `rayBox`). Quand le lanceur reçoit `EV_BOX_BREAK(i)` avec `i == sawBox`, la scie repasse en `SAW_FLY` avec `vel = 0` (elle tombe, puis se replante plus bas) et il émet `EV_SAW` sub 0 avec la nouvelle position.

## 3. Constantes

```c
/* Scie : disque-boomerang (une seule, ni jauge ni recharge) */
#define SAW_SPEED        32.0f   /* vitesse de lancer, plus la vitesse du joueur dans l'axe */
#define SAW_MIN_SPEED    20.0f   /* plancher si on lance en reculant */
#define SAW_GRAVITY      6.0f    /* gravite legere : ~1,4 m de chute sur 22 m */
#define SAW_DMG          30.0f   /* degats de base... */
#define SAW_DMG_PER_MS   1.5f    /* ...plus 1,5 par m/s de vitesse relative au-dela de l'offset */
#define SAW_REL_OFFSET   12.0f   /* cible immobile (rel 32) : 60 PvE / 42 PvP, 3 touches (choix du 27/10) */
#define SAW_CD           0.4f    /* delai minimal entre deux lancers */
#define SAW_RETURN_SPEED 36.0f   /* retour guide apres une touche */
#define SAW_RECALL_SPEED 45.0f   /* rappel au grappin */
#define SAW_HOME_MAX     2.5f    /* retour ou rappel plus long : rattrapee d'office */
#define SAW_CATCH_R      1.5f    /* distance de ramassage / rattrapage */
#define SAW_HOOK_R       0.6f    /* sphere que le crochet doit toucher */
#define SAW_RADIUS       0.35f   /* rayon du disque (rendu, zone de touche) */
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
#define SAW_LOST_Y       -20.0f  /* plan de mort par defaut si la carte n'en a pas */
#define SAW_FLY_MAX      12.0f   /* garde-fou : vol trop long = perdue */
#define SAW_RESYNC       2.0f    /* reemission periodique de l'etat plante (joueurs arrives en cours) */
#define SAW_RECALL_HURTS 0       /* option : la scie rappelee blesse le premier adversaire traverse */
```

## 4. Données

```c
/* Player */
int   sawIdx;            /* 0 = en main (ou pas de scie), sinon index PR + 1 */
float sawCd;             /* recharge de lancer */
/* P.ammo[WP_SAW] (infra) : 1 = en main, 0 = dehors. Depart 1, max 1, aucune jauge. */

/* Proj (champs utilises seulement si kind == PR_SAW) */
int   sawState;          /* SAW_FLY, SAW_PARRIED, SAW_PLANTED, SAW_RETURN, SAW_RECALL, SAW_LOST */
float sawT;              /* temps passe dans l'etat courant */
float sawSpin;           /* angle de rotation du disque (rendu) */
V3    sawN, sawAxis;     /* normale de la face (plantee), axe du disque */
int   sawBy;             /* id de celui qui l'a paree, -1 sinon */
int   sawBox;            /* boite ou elle est plantee (-1) */
int   sawHurt;           /* option rappel : deja blesse quelqu'un */
int   sawBounces;        /* renvois consecutifs depuis le dernier rattrapage (0..SAW_BOUNCE_CHAIN) */
unsigned int sawCoins;   /* pieces deja relancees (bit = index CO, 24 bits) */
float sawSync;           /* minuterie de la reemission SAW_RESYNC */

enum { SAW_FLY, SAW_PARRIED, SAW_PLANTED, SAW_RETURN, SAW_RECALL, SAW_LOST };
enum { SAWE_THROW, SAWE_PLANT, SAWE_HIT, SAWE_CATCH, SAWE_RECALL, SAWE_BOUNCE };     /* EV_SAW.sub */
enum { SAW_HOW_RETURN, SAW_HOW_FOOT, SAW_HOW_RECALL, SAW_HOW_LOST, SAW_HOW_PARRIED }; /* EV_SAW.actor */
static unsigned int sawCounter;
```

- **`netId` de la scie** : règle commune du plan § 4.3, `((myId + 1) << 24) | (PR_SAW << 20) | (++sawCounter & 0xFFFFF)`. Elle évite la collision avec les roquettes dans le tableau `PR` et `find_proj`.
- **`NetPState`** : rien à ajouter (l'octet `pad` est déjà entièrement alloué). Les autres déduisent « scie en main » de l'absence de `PR_SAW` dont `owner == i`.

## 5. Algorithme

### `saw_dmg(rel)`, `saw_axis(f)` (nouvelles)

```c
static float saw_dmg(float rel) { return SAW_DMG + fmaxf(0, rel - SAW_REL_OFFSET) * SAW_DMG_PER_MS; }
/* disque tenu a plat dans le plan de visee : son axe est le "haut" de la camera */
static V3 saw_axis(V3 f) { V3 r = v3(cosf(P.yaw), 0, sinf(P.yaw)); return vnorm(vcross(r, f)); }
```

### `fire_saw()` (nouvelle, appelée par la répartition du tir de l'infra pour `WP_SAW`)

```c
static void fire_saw(void) {
    if (P.ammo[WP_SAW] < 1 || P.sawCd > 0) return;
    V3 f = view_dir();
    float s = fmaxf(SAW_SPEED + vdot(P.vel, f), SAW_MIN_SPEED);
    Proj *p = spawn_proj(PR_SAW, eye_pos(), vmul(f, s));
    if (!p) return;
    p->sawState = SAW_FLY; p->sawT = 0; p->sawBy = -1; p->sawBox = -1; p->sawCoins = 0; p->sawHurt = 0; p->sawBounces = 0;
    p->sawAxis = saw_axis(f);
    p->netId = ((unsigned)(myId + 1) << 24) | ((unsigned)PR_SAW << 20) | (++sawCounter & 0xFFFFF);
    P.ammo[WP_SAW] = 0; P.sawIdx = (int)(p - PR) + 1; P.sawCd = SAW_CD;
    P.recoil = 0.6f; shake += 0.04f;
    play(S_SAW_THROW, 0.7f, frange(0.95f, 1.05f));
    if (dmMode) net_fx_saw(SAWE_THROW, p, -1, 0);
}
```

Dans `game_tick` : `P.sawCd -= dt;` à côté de `P.fireCd -= dt`. Clic avec l'emplacement vide : si `P.sawIdx` et `PR[P.sawIdx-1].sawState == SAW_PLANTED` et `lmbp` → clic sec (`S_HIT` 0,5 / 0,45 comme le rail), `popText` « SCIE PLANTEE (%d m) », puis bascule de l'infra. Sinon, rien.

### `proj_tick` (modifiée)

Au début de la boucle, avant le calcul du pas commun : `if (p->kind == PR_SAW) { saw_tick(p, dt); continue; }`. La scie n'utilise pas `p->life`.

### `saw_tick(Proj *p, float dt)` (nouvelle)

```c
static void saw_tick(Proj *p, float dt) {
    int mine = !dmMode || p->owner == myId;
    p->sawT += dt;
    p->sawSpin += dt * (p->sawState == SAW_PLANTED ? 0 : 30);
    V3 home = mine ? pcenter() : np_center(p->owner);
    switch (p->sawState) {
    case SAW_FLY: case SAW_PARRIED: {
        p->vel.y -= SAW_GRAVITY * dt;
        V3 step = vmul(p->vel, dt); float len = vlen(step); V3 dir = vmul(step, 1 / len), n;
        float t = ray_world(p->pos, dir, len, &n); int box = rayBox;
        int he = -1, hp = -1;
        if (mine && p->sawState == SAW_FLY) {                      /* seul le lanceur touche */
            for (j < MAXE) if (E[j].alive && ray_sphere(p->pos, dir, E[j].pos, enemy_radius(E[j].type) + 0.3f, t, &te)) { t = te; he = j; }
            for (j < MAXP) if (np_target(j)) { np_aabb(j, SAW_HIT_GROW, &mn, &mx); if (ray_box(...) && te < t) { t = te; hp = j; he = -1; } }
            saw_coins(p, dir, t);                                  /* frolement de pieces */
        }
        if (p->sawState == SAW_PARRIED && p->sawBy == myId && np_target(p->owner)) {   /* celui qui a pare juge la touche */
            np_aabb(p->owner, SAW_HIT_GROW, &mn, &mx);
            if (ray_box(p->pos, dir, mn, mx, t, &te, NULL) && te < t) {
                V3 at = vadd(p->pos, vmul(dir, te));
                hit_player(p->owner, saw_dmg(vlen(vsub(p->vel, np_vel(p->owner)))) * PVP_SCALE,
                           vadd(vmul(dir, 5), v3(0, 2, 0)), W_PARRY, dir);
                saw_hit_fx(at, 0); p->alive = 0; return;           /* le lanceur la rattrape chez lui (voir § 6) */
            }
        }
        if (he >= 0 || hp >= 0) { saw_strike(p, dir, t, he, hp); return; }
        if (t < len) {                                             /* decor : elle se plante */
            if (mine) saw_plant(p, vadd(p->pos, vmul(dir, t)), n, dir, box);
            else { p->pos = vsub(vadd(p->pos, vmul(dir, t)), vmul(n, SAW_EMBED)); p->sawState = SAW_PLANTED; p->vel = v3(0,0,0); }
            return;                                                /* chez les autres : prediction, corrigee par SAWE_PLANT */
        }
        p->pos = vadd(p->pos, step);
        if (mine && p->sawState == SAW_PARRIED && vlen(vsub(p->pos, home)) < SAW_CATCH_R) { saw_catch(p, SAW_HOW_PARRIED); return; }
        if (mine && (p->pos.y < map_kill_y() || p->sawT > SAW_FLY_MAX)) { p->sawState = SAW_LOST; p->sawT = 0; }
        if (frand() < 0.6f) add_part(p->pos, vmul(rand_dir(), 0.5f), saw_trail_col(p), 0.25f, 0.08f, 0);  /* etincelles */
        break;
    }
    case SAW_PLANTED:
        if (!mine) break;
        if (!P.dead && vlen(vsub(pcenter(), p->pos)) < SAW_CATCH_R) { saw_catch(p, SAW_HOW_FOOT); return; }
        if (dmMode && (p->sawSync -= dt) <= 0) { p->sawSync = SAW_RESYNC; net_fx_saw(SAWE_PLANT, p, -1, 0); }
        break;
    case SAW_RETURN: case SAW_RECALL: {
        float sp = p->sawState == SAW_RETURN ? SAW_RETURN_SPEED : SAW_RECALL_SPEED;
        V3 d = vsub(home, p->pos); float l = vlen(d);
        p->vel = vmul(d, sp / fmaxf(l, 0.01f));                     /* guidage parfait, a travers le decor */
        if (SAW_RECALL_HURTS && mine && p->sawState == SAW_RECALL && !p->sawHurt) saw_recall_hurt(p, dt);
        if (l < SAW_CATCH_R || l < sp * dt || p->sawT > SAW_HOME_MAX) {
            if (mine) saw_catch(p, p->sawState == SAW_RETURN ? SAW_HOW_RETURN : SAW_HOW_RECALL);
            else p->alive = 0;                                     /* SAWE_CATCH confirmera */
            return;
        }
        p->pos = vadd(p->pos, vmul(p->vel, dt));
        add_part(p->pos, v3(0, 0, 0), v3(0.4f, 0.8f, 1.0f), 0.2f, 0.1f, 0);   /* trainee "magnetique" */
        break;
    }
    case SAW_LOST:
        if (mine && p->sawT > SAW_LOST_T) saw_catch(p, SAW_HOW_LOST);
        break;
    }
}
```

`map_kill_y()` renvoie le `killY` de la carte (`MapDef`, § 4.7 du document d'idées) ou `SAW_LOST_Y` tant que les cartes n'existent pas. Chez les autres joueurs, une scie qui arrive au lanceur est retirée localement : si celui-ci la renvoie, `SAWE_BOUNCE` la recrée (`find_proj` échoue → `spawn_proj`).

### `saw_plant` : point et normale avec `ray_world`

`ray_world` renvoie le `t` du premier `ray_box` et sa normale `bn`, alignée sur un axe (`axis`, `sign` dans `ray_box`). Si l'origine est déjà dans une boîte, `ray_box` renvoie `t = 0` et `n = −d`, ce qui reste utilisable. L'index de la boîte vient de `rayBox` (plan § 4.3), écrit par `ray_world` (−1 si rien). Aucun appel existant n'est touché.

```c
static void saw_plant(Proj *p, V3 hit, V3 n, V3 dir, int box) {
    p->pos = vsub(hit, vmul(n, SAW_EMBED));          /* le centre s'enfonce : 0,23 m de lame depasse */
    V3 ax = vcross(dir, n);                          /* le disque entre "par la tranche" : son plan contient dir et n */
    p->sawAxis = vlen(ax) > 0.05f ? vnorm(ax) : p->sawAxis;  /* tir perpendiculaire a la face : on garde l'axe de vol */
    p->sawN = n; p->sawBox = box; p->vel = v3(0, 0, 0);
    p->sawState = SAW_PLANTED; p->sawT = 0; p->sawSync = SAW_RESYNC;
    for (k < 14) add_part(hit, vadd(vmul(n, 4), vmul(rand_dir(), 3)), v3(1, 0.8f, 0.45f), 0.35f, 0.08f, 12);
    shards(hit, boxes[box].col, 3, 4);
    play(S_SAW_PLANT, 0.8f * atten(hit), frange(0.95f, 1.05f));
    if (dmMode) net_fx_saw(SAWE_PLANT, p, -1, 0);
    if (botMode) netlog("scie : plantee a %.1f m (face %+.0f %+.0f %+.0f, apres %d renvois)",
                        vlen(vsub(hit, pcenter())), n.x, n.y, n.z, p->sawBounces);
}
```

Code de face envoyé sur le réseau : `face = axe * 2 + (signe > 0)`, avec axe 0, 1 ou 2.

### `saw_strike` (touche) et `saw_catch` (nouvelles)

```c
static void saw_strike(Proj *p, V3 dir, float t, int he, int hp) {
    V3 at = vadd(p->pos, vmul(dir, t));
    float rel = vlen(vsub(p->vel, he >= 0 ? E[he].vel : np_vel(hp)));
    float dmg = saw_dmg(rel) * (p->sawBounces ? SAW_RETURN_PARRY_DMG_MULT : 1);   /* renvoi : x0,9 */
    if (he >= 0) damage_enemy(he, dmg, vmul(dir, 6));
    else hit_player(hp, dmg * PVP_SCALE, vadd(vmul(dir, 5), v3(0, 2, 0)), W_SAW, dir);  /* EV_HIT f[4] = sawBounces */
    if (p->sawBounces) melee_parry_points(PTS_PARRY);    /* renvoi qui touche = parry reussi : points a l'autre arme */
    saw_hit_fx(at, dmg);                              /* etincelles, hitMarker, hitstop 0,03 + rel*0,001 ; impact_frame si dmg >= 85 */
    p->pos = at; p->sawState = SAW_RETURN; p->sawT = 0;   /* sawBounces garde son compte : la chaine continue */
    if (dmMode) net_fx_saw(SAWE_HIT, p, hp, 0);
    if (botMode) netlog("scie : touche %s, rel %.1f, degats %.0f, renvoi %d", he >= 0 ? "drone" : pname(hp), rel, dmg, p->sawBounces);
}
static void saw_catch(Proj *p, int how) {
    if (how == SAW_HOW_RETURN) P.dashCharge = fminf(2, P.dashCharge + SAW_DASH);
    P.ammo[WP_SAW] = 1; P.sawIdx = 0; p->alive = 0;  /* sawCd n'est pas touche : pas de delai ajoute */
    play(S_SAW_CATCH, 0.6f, how == SAW_HOW_RETURN ? 1.15f : 1.0f);
    if (dmMode) net_fx_saw(SAWE_CATCH, p, -1, how);
}
```

`saw_coins(p, dir, t)` : pour chaque `CO[i]` vivante, `!(p->sawCoins >> i & 1)` et `ray_sphere(p->pos, dir, CO[i].pos, SAW_COIN_R, t, &tc)` → `CO[i].vel = v3(CO[i].vel.x * 0.3f, SAW_COIN_KICK, CO[i].vel.z * 0.3f)`, `CO[i].life += SAW_COIN_LIFE`, bit marqué, son `S_HIT` aigu. En LAN, on émet l'événement **`EV_COIN` sous-type 3 « relancée »** (plan § 4.3) : `netId`, `f[0..2]` position, `f[3..5]` vitesse, `f[6]` vie restante. La scie ne dévie pas.

### `try_parry(range, speedMult)` (modifiée) : parry adverse et renvoi

La signature commune vient du plan § 4.2. Les deux branches `PR_SAW` passent **avant** le code des roquettes.

```c
if (p->kind == PR_SAW) {
    int st = p->sawState;
    if (p->owner == myId) {                                      /* RENVOI : ma scie qui revient */
        if ((st != SAW_RETURN && st != SAW_RECALL && st != SAW_PARRIED) || p->sawBounces >= SAW_BOUNCE_CHAIN) continue;
        float r = range * powf(PARRY_RANGE_SHRINK, (float)p->sawBounces);
        if (dist > r || vdot(vmul(d, 1.0f / fmaxf(dist, 0.01f)), f) < 0.3f) continue;
        saw_bounce(p, speedMult);
        parried++; P.swingLanded = 1;                            /* pas de parry_feedback : pas de soin, dash, grappin */
        continue;
    }
    if (!dmMode || (st != SAW_FLY && st != SAW_PARRIED)) continue;   /* scie adverse en vol : parry normal */
    V3 to = vnorm(vsub(np_center(p->owner), p->pos));            /* toujours vers son lanceur, sans aide a la visee */
    p->vel = vmul(to, fmaxf(PARRY_SPEED, vlen(p->vel) * SAW_PARRY_MULT) * speedMult);
    p->sawState = SAW_PARRIED; p->sawBy = myId; p->sawT = 0; p->parries++;
    net_fx_parry(p->netId, p->pos, p->vel, 1, p->parries);
    parried++; P.swingLanded = 1; parry_feedback(p->pos);        /* parry adverse : recompense complete (+50 points, soin, dash) */
    continue;
}
```

### `saw_bounce(Proj *p, float speedMult)` (nouvelle)

```c
static void saw_bounce(Proj *p, float speedMult) {
    V3 eye = eye_pos(), f = view_dir(), aim = f;
    float base = SAW_SPEED * SAW_BOUNCE_MULT * speedMult;
    float sp = fminf(base + fmaxf(0, vdot(P.vel, f)), SAW_BOUNCE_MAX), best = SAW_BOUNCE_AIM_COS;
    /* aide a la visee : cible dont le point anticipe est a moins de 7 degres du viseur */
    for (chaque cible c (E[j] vivant, ou np_center(j) si np_target(j)), de vitesse vc) {
        float dd = vlen(vsub(c, p->pos)); if (dd > SAW_BOUNCE_AIM_RANGE || !los(p->pos, c)) continue;
        float tt = dd / sp;
        V3 lead = vadd(vadd(c, vmul(vc, tt)), v3(0, 0.5f * SAW_GRAVITY * tt * tt, 0));
        V3 to = vnorm(vsub(lead, p->pos));
        float k = vdot(vnorm(vsub(lead, eye)), f);               /* cone mesure depuis l'oeil, comme le viseur */
        if (k > best) { best = k; aim = to; }
    }
    p->vel = vmul(aim, sp);
    p->sawState = SAW_FLY; p->sawT = 0; p->sawBy = -1; p->sawCoins = 0; p->sawBounces++;
    saw_bounce_fx(p->pos, p->sawBounces);                        /* hitstop 0,05, eclat blanc, "RENVOI xN", S_SAW_BOUNCE */
    if (dmMode) net_fx_saw(SAWE_BOUNCE, p, -1, p->sawBounces);
    if (botMode) netlog("scie : renvoi %d a %.0f m/s, aide %s", p->sawBounces, sp, best > SAW_BOUNCE_AIM_COS ? "oui" : "non");
}
```

### `grapple_tick` (modifiée) : rappel de la scie plantée

Dans `HOOK_FLYING`, juste après `t = ray_world(...)` et **avant** les boucles ennemis et joueurs :

```c
int sawHit = 0;
if (P.sawIdx) {
    Proj *s = &PR[P.sawIdx - 1]; float ts;            /* seulement MA scie, seulement plantee */
    if (s->sawState == SAW_PLANTED && ray_sphere(P.hookPos, P.hookDir, s->pos, SAW_HOOK_R, t + 0.05f, &ts) &&
        los(P.hookPos, vadd(s->pos, vmul(s->sawN, 0.25f)))) { t = ts; sawHit = 1; }   /* los : pas a travers un mur fin */
}
/* ... boucles ennemis et joueurs existantes : une cible plus proche annule sawHit (target/ptarget >= 0) ... */
if (sawHit && target < 0 && ptarget < 0) {
    Proj *s = &PR[P.sawIdx - 1];
    s->sawState = SAW_RECALL; s->sawT = 0;
    P.hook = HOOK_NONE; P.hookCd = GRAPPLE_CD;        /* decroche : ni traction, ni ancre, P.vel intact */
    for (k < 12) add_part(s->pos, vmul(rand_dir(), 4), v3(0.4f, 0.8f, 1), 0.3f, 0.08f, 0);
    play(S_HOOKHIT, 0.7f, 1.6f); play(S_SAW_CATCH, 0.4f, 0.7f);
    if (dmMode) net_fx_saw(SAWE_RECALL, s, -1, 0);
    return;
}
```

Les scies des autres sont ignorées : le crochet s'accroche au mur derrière elles. Comme le crochet avance de 1 m par tick (120 m/s), le test sur le segment suffit.

### Réception, mort, respawn

`saw_remove_owner(int id)` : `PR[i].alive = 0` pour tout `PR_SAW` dont `owner == id` ; si `id == myId`, `P.sawIdx = 0`.
- **Appelée dans `apply_event`** : `EV_KILL` (victime), `EV_RESPAWN` (cible), `EV_INFO` sub 1.
- **En solo** : dans le chemin de mort de `damage_player` (et `reset_game` vide déjà `PR` et `P`).

## 6. Réseau

**Autorité** : le lanceur fait autorité sur le cycle de vie de sa scie (vol, plantage, touche, retour, renvoi, rattrapage, perte). Le renvoi est un parry de **sa propre** scie, fait chez lui : il ne passe pas par l'hôte et ne crée pas d'`EV_PARRY`, donc pas de soin de l'hôte. Les dégâts suivent la règle « touche validée chez le tireur » : `hit_player(..., W_SAW)` chez le lanceur, avec `EV_HIT.f[4]` = nombre de renvois (champ propre à l'arme, plan § 4.3), et `hit_player(..., W_PARRY)` chez celui qui a paré. L'hôte n'arbitre que les PV (`PVP_HIT_CAP`) et les kills. Pas de cas `EV_SAW` dans `host_handle`, sauf des compteurs de journal (§ 11).

`net_fx_saw(int sub, Proj *p, int target, int actor)` construit un `EV_SAW` :

| `sub` | Quand | Champs | Effet chez les autres (`apply_event`) |
|---|---|---|---|
| 0 `SAWE_THROW` | `fire_saw` (et chute après `EV_BOX_BREAK`) | `netId`, `f[0..2]` pos, `f[3..5]` vel | `spawn_proj(PR_SAW)`, `owner = from`, état `FLY`, axe recalculé depuis vel ; son `S_SAW_THROW` × `atten` |
| 1 `SAWE_PLANT` | `saw_plant`, puis toutes les `SAW_RESYNC` s | `netId`, `f[0..2]` centre, `f[3..5]` axe, `f[6]` code de face | `find_proj` ou création, état `PLANTED`, position imposée. **Ignoré si `!NP[from].alive`** (réémission arrivée après la mort) |
| 2 `SAWE_HIT` | `saw_strike` | `netId`, `target` (255 = aucun), `f[0..2]` point | état `RETURN` depuis ce point ; étincelles |
| 3 `SAWE_CATCH` | `saw_catch` | `netId`, `actor` = moyen | retire la scie ; petit son |
| 4 `SAWE_RECALL` | `grapple_tick` | `netId`, `f[0..2]` pos | état `RECALL` |
| 5 `SAWE_BOUNCE` (nouveau) | `saw_bounce` | `netId`, `f[0..2]` pos, `f[3..5]` nouvelle vitesse, `actor` = `sawBounces` | `find_proj` ou création, état `FLY`, `sawBounces = actor` (couleur de la lame), éclat blanc et `S_SAW_BOUNCE` × `atten` |

- **Simulation chez les autres** : `saw_tick` avec `mine == 0`. Le vol est simulé à l'identique (même départ, même gravité, même carte), donc le plantage prédit tombe presque toujours au bon endroit ; `SAWE_PLANT` le corrige. Les touches sur joueurs ne sont **pas** prédites : la scie traverse la cible visuellement pendant ≤ 100 ms, jusqu'à `SAWE_HIT`. Retour et rappel visent `np_center(owner)`.
- **Renvoi et latence** : le lanceur voit sa scie revenir sans délai (il la simule lui-même), donc la fenêtre de 0,19 s ne dépend pas du ping. Chez les autres, la scie a pu être retirée localement en arrivant au lanceur (`np_center` est en retard) : `SAWE_BOUNCE` la recrée au bon endroit. La cible voit donc le renvoi partir avec ~1 RTT/2 de retard, comme une roquette.
- **Parry adverse** : `EV_PARRY` (via `net_fx_parry`) garde son chemin. L'hôte soigne déjà celui qui pare (`host_handle`). Dans `apply_event` `EV_PARRY`, **avant** le code des roquettes : si `find_proj(netId)` est un `PR_SAW`, on ne change ni `kind` ni `owner`, et on fixe `sawState = SAW_PARRIED`, `sawBy = from`, `pos`, `vel`.
  - **Chez le lanceur** : accepté seulement si sa scie est en `FLY` ou en `RETURN`. Sinon (déjà plantée ou rattrapée chez lui), il réémet aussitôt `SAWE_PLANT` ou `SAWE_CATCH` pour corriger la copie de celui qui a paré.
  - **Touche par la scie parée** : celui qui a paré la juge. Le lanceur, qui reçoit un `EV_HIT` `W_PARRY` venant de `sawBy` alors que sa scie est `PARRIED`, la **rattrape d'office** (`saw_catch(SAW_HOW_PARRIED)`). Cela tient la règle « il prend les dégâts mais la rattrape » même si les deux écrans divergent.
  - **Exception au rattrapage d'office** : si le lanceur l'a déjà renvoyée (`FLY` avec `sawBounces > 0`), il prend les dégâts et la scie continue. Celui qui a paré reçoit `SAWE_BOUNCE` et recrée la scie qu'il avait cachée.
- **Arrivée en cours de match** : les événements passés ne sont pas rejoués. La réémission `SAWE_PLANT` toutes les 2 s rend chaque scie plantée visible au nouveau venu en ≤ 2 s. Coût : ≤ 8 joueurs × 0,5 évt/s × 3 copies, négligeable devant `OUTQ` 128 et `seenIds` 512. Les scies en vol ou en retour, de vie courte, ne sont pas resynchronisées.
- **Latence des touches** : la touche est validée sur `NP[j].disp`, comme la roquette. Si le parry et la touche se croisent (fenêtre de 0,12 s), la victime peut à la fois parer et prendre les dégâts : c'est le même compromis qu'aujourd'hui pour les roquettes. Voir la question 4.

## 7. Interactions

- **Parry** : voir § 2.3, § 5 et § 6. Une scie adverse en `FLY` ou `PARRIED` se pare normalement (récompense complète de `parry_feedback`). Ma propre scie qui revient se **renvoie** (récompense réduite). `PLANTED` et `LOST` ne sont jamais parables.
- **Armes de mêlée et renvoi** : le renvoi utilise `try_parry(range, speedMult)` de la mêlée tenue. Avec l'épée : 3,9 m, ×1. Avec le Fléau : 6 m, ×0,8, donc un renvoi à 38,4 m/s, plus facile mais plus lent. Les autres mêlées prennent la valeur de l'épée sauf mention dans leur fiche. Le coup de mêlée qui renvoie peut aussi toucher un adversaire proche, avec les points de mêlée normaux.
- **Pièces** : seule la scie en `FLY` relance les pièces, renvoi compris (une fois par pièce et par vol ; `sawCoins` est remis à 0 au renvoi). La pièce garde son propriétaire : un rail adverse peut la « voler » comme d'habitude.
- **Grappin** : rappel de **sa** scie plantée seulement, priorité à une cible plus proche sur le trajet. Crochet libéré tout de suite, `GRAPPLE_CD` 0,5 s, portée 55 m inchangée. Le grappin sur un joueur ou un ennemi n'interagit pas avec une scie en vol.
- **Égide** : toutes les touches passent par `hit_player` (plan § 4.4), donc le bouclier peut bloquer la scie. La réaction (bloquée de face, la scie se plante-t-elle ou revient-elle ?) est décrite dans le tableau de la fiche 05.
- **Autres armes** : la scie ne détruit ni roquettes, ni mines, ni pièces. Une explosion ne déplace pas une scie plantée. Les scies plantées de chacun sont lisibles (moyeu à la couleur du joueur, `pcol`), ce qui est le contre-jeu voulu (« camper la scie »).
- **Jauges de points** (§ 1.5) : la Scie **n'a ni jauge ni recharge**. Elle compte comme « pleine » : les points des coups de mêlée, des kills en mêlée, des parries et des cellules vont à l'autre arme du loadout. Une touche de la Scie ne donne aucun point (c'est une arme à distance), **sauf** une touche après renvoi, qui compte comme un parry réussi (+50).

## 8. Rendu et son

- **Disque en monde** (`render`, passe opaque à côté des pièces, `GL_LIGHTING` coupé comme pour elles). La base `(a, b)` est perpendiculaire à `sawAxis` (`a = vnorm(vcross(axis, |axis.y| < 0.9 ? Y : X))`, `b = vcross(axis, a)`), et les sommets sont calculés en C (pas de `glRotatef`) :
  - lame : `GL_TRIANGLE_FAN` de 16 segments, rayon `SAW_RADIUS`, gris acier `(0.78, 0.8, 0.86)`, qui **rougit avec les renvois** (`lerp` vers `(1, 0.45, 0.2)` de `sawBounces / 3`) ;
  - 12 dents : `GL_TRIANGLES`, base sur le bord et pointe à `1.25 × R`, décalées de `sawSpin` ;
  - moyeu : fan de rayon 0,1, couleur `pcol[owner]`.

  Comme le culling n'est pas activé, une seule face suffit. Une scie plantée **vibre** au plantage : décalage angulaire `0.15 × sin(sawT × 60) × exp(−sawT × 8)` autour de `sawN`.
- **Passe additive** : halo `billboard(pos, 0.6)` orange en vol, blanc doré après un renvoi, cyan en retour ou rappel, doré pulsé (`0.5 + 0.3 sin`) quand elle est plantée et **à moi**, pour la retrouver de loin. Pas de halo pour les scies plantées adverses. Au renvoi : `add_beam` court (2 m) dans la nouvelle direction, et 20 particules blanches.
- **Viewmodel** (`draw_viewmodel`) : `WP_SAW` en main → disque tenu à plat dans la main droite (même tracé, rayon 0,25, rotation lente `gameTime × 3`), lancer animé par `P.recoil`. Scie dehors → main vide (petite boîte « gant ») ; en retour, lueur cyan sur la paume, qui annonce le moment du renvoi. Le renvoi joue l'animation de mêlée habituelle.
- **Joueurs distants** (`draw_remote_players`) : si `st.weapon == WP_SAW` et aucun `PR_SAW` avec `owner == i`, petit disque dans la main ; sinon rien.
- **HUD** (`draw_hud`) :
  - panneau d'arme : `SCIE  EN MAIN`, `SCIE  EN VOL`, `SCIE  RETOUR`, `SCIE  PLANTEE 18 m`, `SCIE  PERDUE 2.1 s`, plus la jauge de `sawCd`, et 3 pastilles de renvois (pleines = encore disponibles) ;
  - **indicateur de renvoi** : quand ma scie revient et que la mêlée est prête, un petit cercle autour du viseur se resserre quand la scie entre dans la fenêtre (≤ 8,2 m à l'épée). Il devient blanc quand elle est à portée de parry ;
  - **indicateur de direction** de la scie plantée (ou perdue) : si `project(pos, &sx, &sy)` réussit et que le point est dans l'écran, losange doré et distance en mètres, même à travers les murs. Sinon, flèche dorée sur un cercle de 150 px, comme la flèche de dégâts (`ang = atan2f(d.x, −d.z) − P.yaw`) ;
  - viseur de grappin : si le crochet est prêt et que `ray_sphere(eye, vd, saw.pos, SAW_HOOK_R, 55)` passe (et `los`), le texte du grappin devient `GRAPPIN -> RAPPEL SCIE`, en doré.
- **Juice** : touche → `burst` d'étincelles, `hitMarker`, `hitstop = 0.03 + rel × 0.001`. Si les dégâts PvE atteignent 85 (plafond PvP), `impact_frame(at, 0.06)` : c'est le cas typique d'un renvoi en courant. Renvoi → `hitstop` 0,05, `P.fovKick += 4`, texte `RENVOI` / `RENVOI x2` / `RENVOI x3`. Rattrapage après touche → `P.fovKick += 3`.
- **Sons** (`make_sounds`, avant `S_COUNT`) :
  - `S_SAW_THROW` (0,35 s) : bruit filtré passe-bande qui monte de 800 à 2 500 Hz, plus un sinus de 1 800 Hz vibré à 40 Hz, enveloppe `exp(−t × 7)`. Rejoué toutes les 0,18 s en vol, à 0,3 × `atten`, en guise de vrombissement ;
  - `S_SAW_PLANT` (0,5 s) : choc grave (sinus de 90 Hz, `exp(−t × 30)`) et « twang » métallique (2 400 Hz modulé en fréquence à ±60 Hz par un LFO de 30 Hz, `exp(−t × 6)`) ;
  - `S_SAW_HIT` (0,25 s) : dent-de-scie de 300 Hz plus bruit, `exp(−t × 12)` : grincement de lame ;
  - `S_SAW_CATCH` (0,15 s) : clic (bruit sur 5 ms) puis carillon de 1 200 à 2 000 Hz ;
  - `S_SAW_BOUNCE` (0,3 s) : « ting » métallique (sinus de 3 200 Hz plus harmonique de 4 800 Hz, `exp(−t × 10)`), sur une attaque de bruit de 10 ms ; joué avec `S_PARRY` à la hauteur 1,3 + 0,15 × renvoi, qui monte comme les ricochets.

## 9. Bots

Dans `bot_think`, si `WP_SAW` fait partie du loadout :

1. **Scie en main**, cible visible à moins de 30 m, `P.sawCd <= 0` : choisir `WP_SAW` et tirer, en visant avec anticipation : `tt = d / s`, point visé `c + vel_cible × tt + (0, 0.5 × 6 × tt², 0)`, avec l'erreur de visée habituelle (`aimErr`).
2. **Renvoi** : ma scie en `RETURN`, `RECALL` ou `PARRIED`, à moins de 6 m, dans un cône de 70° devant la vue, cible visible à moins de 40 m, `P.meleeCd <= 0`, `sawBounces < 3` :
   - une fois sur deux (tirage au sort par retour, comme `parryGo`), viser la cible (`aimYaw`, `aimPitch` avec `aimErr`) et `botPress[A_MELEE] = 1` ;
   - log « scie : renvoi tente (n) ».

   La cible est en général dans la direction d'où revient la scie : la viser revient à faire face à la scie.
3. **Scie plantée** (et pas d'adversaire à moins de 8 m) :
   - à moins de `GRAPPLE_RANGE`, en `los`, avec `hookCd <= 0` : viser la scie et `botPress[A_HOOK] = 1` (rappel) ;
   - sinon, la traiter comme une recharge à rejoindre (même code que `seek` : `aimYaw` vers la scie).
4. **Scie dehors** : `P.weapon` passe sur l'autre arme du loadout.
5. **Parry d'une scie adverse** : le bloc « roquette adverse » accepte aussi `PR_SAW` en `FLY` dont `owner != myId`, avec la même probabilité de 0,6.

## 10. Tests (`run_tests`)

Dans le couloir dégagé `x = −20`, regard vers −z (`yaw = 0`), comme le test de parry existant.

1. **Formule** : `saw_dmg(0)`, `saw_dmg(32)`, `saw_dmg(41.5)`, `saw_dmg(51)`, puis × 0,7 avec le plafond. Attendu : `30/21`, `60/42`, `74.3/52`, `88.5/60`.
   - Renvoi : `saw_dmg(48) × SAW_RETURN_PARRY_DMG_MULT` et `saw_dmg(57.5) × 0,9`, puis × 0,7 avec plafond. Attendu : `75.6/52.9` et `88.4/60`.
   - Combo, via `host_handle` (deux `EV_HIT` `W_SAW` sur `NP[1]` à 100 PV). Lancer immobile 42 + renvoi immobile 52,9 → **5,1 PV, vivant**. Lancer immobile 42 + renvoi en course 60 → mort.
2. **Lancer et plantage** : `P` en (−20, 0, 40), immobile, `fire_saw()`, 600 × `proj_tick`. Attendu : état `PLANTED`, `|sawN.z| == 1`, chute ≈ `3 × (d/32)²` (afficher aussi `d`), `P.ammo[WP_SAW] == 0`.
3. **Touche sur un drone immobile** : drone en (−20, 1.7, 30), 100 PV. Après le lancer et ≤ 120 ticks : PV ≈ 40 (rel ≈ 32,05, soit 60,1 dégâts), état `RETURN`, puis rattrapée. Attendu : `P.ammo[WP_SAW] == 1`, `P.sawIdx == 0`, `dashCharge` +0,5.
4. **Lancer en courant** : `P.vel = (0, 0, −10)`, même drone. Attendu : vitesse de lancer 42, dégâts 75.
5. **Ramassage à pied** : scie plantée au sol en (−20, −0.12, 35), joueur placé à 1,4 m → `saw_tick` → rattrapée. À 1,6 m → toujours plantée.
6. **Rappel au grappin** : scie plantée dans le mur du test 2, regard sur elle, `hookPressIn = hookHeldIn = 1`, ticks de `player_tick`. Attendu : `P.hook == HOOK_NONE` dès le contact, `|P.vel| == 0` et `P.pos` inchangée, état `RECALL`, rattrapée après ≈ `d/45` s, `P.hookCd ≈ 0.5`.
7. **Scie d'un autre ignorée** : la même scie avec `owner = 1` → `P.hook == HOOK_ATTACHED` sur le mur.
8. **Parry adverse (LAN simulé)** : `dmMode = 1`, `NP[1]` lanceur en (−20, 0, 24), scie `owner = 1` en `FLY` à 2 m devant moi vers moi, à 32 m/s, puis `melee()`. Attendu : état `PARRIED`, `sawBy == myId`, vitesse 40 (32 × 1,25), direction vers `np_center(1)`.
9. **Renvoi avec aide à la visée** : `P` immobile en (−20, 0, 40), drone immobile en (−20, 1.7, 22), ma scie en `RETURN` en (−20, 1.6, 37) vers moi, `P.pitch` vers le drone, PV et dash notés, puis `melee()`.
   - Attendu juste après : état `FLY`, `|vel| = 48`, `sawBounces == 1`, PV et `dashCharge` inchangés (pas de soin ni de dash).
   - Après ≤ 60 ticks : drone à 24,4 PV (84 × 0,9 = 75,6 dégâts), état `RETURN`.
10. **Cône de l'aide** : même scène, drone décalé à 5° du viseur → direction à moins de 0,3° du drone. Drone à 10° → direction = `view_dir()` (pas d'aide).
11. **Garde-fous** :
    - `sawBounces = 3` → `melee()` ne renvoie pas (l'état reste `RETURN`, puis la scie est rattrapée et `sawBounces == 0`) ;
    - scie `PLANTED` à 2 m → `melee()` la laisse plantée ;
    - scie en `RECALL` → renvoi accepté.
12. **Pièce relancée** : pièce sur la trajectoire à 0,5 m de l'axe. Attendu : `vel.y == 11`, vie + 2, scie toujours en `FLY`.
13. **Perdue** : scie en `FLY` avec `pos.y = −25` → `LOST`, puis rattrapée après 3 s (360 ticks).
14. **Mort** : `EV_KILL` local sur moi → aucun `PR_SAW` vivant, `P.sawIdx == 0`.
15. **netId** : un `fire_rocket` puis un `fire_saw` en `dmMode`. Attendu : `netId` différents, `(netId >> 20 & 0xF) == PR_SAW` pour la scie.

## 11. Équilibrage

Formule retenue : `30 + 1,5 × max(0, rel − 12)`, × 0,7 en PvP, plafond 60.

| Situation | Vitesse relative | PvE | PvP | Touches pour tuer |
|---|---|---|---|---|
| Cible qui fuit à ≥ 20 m/s dans l'axe | ≤ 12 | 30 | 21 | 5 |
| Cible qui s'écarte à 12 m/s | 20 | 42 | 29,4 | 4 |
| Seuil des 3 touches | 24,4 | 48,6 | 34 | 3 |
| **Lanceur et cible immobiles** | **32** | **60** | **42** | **3** |
| Lanceur à 9,5 m/s, cible immobile | 41,5 | 74,3 | 52 | 2 |
| Seuil des 2 touches | 39,6 | 71,4 | 50 | 2 |
| Les deux à 9,5 m/s, face à face | 51 | 88,5 | **60** | 2 |
| **Renvoi à l'arrêt** (48 m/s), cible immobile, × 0,9 | 48 | 75,6 | 52,9 | 2 |
| Renvoi : seuil du combo mortel après un lancer à l'arrêt | 53,4 | 82,9 | 58 | — |
| Renvoi en courant à 9,5 m/s, × 0,9 | 57,5 | 88,4 | **60** | 2 |
| Renvoi : seuil du plafond | ≥ 55,5 | ≥ 85,7 | 60 | 2 |
| Plafond atteint | ≥ 49,1 | ≥ 85,7 | 60 | 2 |

**Ce qu'il faut retenir** :
- Au lancer simple, la Scie est une arme à **3 touches** contre une cible immobile, et à 2 touches si le lanceur fonce (le pilier « la vitesse est une ressource »).
- **Combo lancer + renvoi**, avec le × 0,9 décidé le 27/10 (sur une cible immobile à pleine vie) :

  | Lancer | Renvoi | Total PvP | Résultat |
  |---|---|---|---|
  | à l'arrêt (42) | à l'arrêt (52,9) | **94,9** | survit à 5,1 PV |
  | à l'arrêt (42) | lanceur à ≥ 5,4 m/s (≥ 58) | ≥ 100 | tué |
  | à l'arrêt (42) | en course à 9,5 m/s (60) | 102 | tué |
  | en course à 9,5 m/s (52) | à l'arrêt (52,9) | 104,9 | tué |
  | en course (52) | en course (60) | 112 | tué |

  Le combo statique ne tue plus ; le combo **en mouvement** tue toujours. Cela respecte le pilier « la vitesse est une ressource ». Une cible qui fuit (rel − 4 m/s par touche, soit −4,2 PvP) survit même à un combo en course partielle.
- **Renvois successifs** (dégâts constants, pas d'escalade) : lancer + 2 renvois à l'arrêt = 42 + 52,9 + 52,9 = 147,8 ; 2 renvois seuls (chaîne lancée par un rappel au grappin) = 105,8 → tué. La chaîne complète (lancer + 3 renvois) peut infliger jusqu'à 42 + 3 × 52,9 = 200,7, mais chaque renvoi raté plante la scie.
- **Cadence de la boucle de renvoi** : à 15 m, renvoi `15/48` + retour `15/36` = 0,73 s par cycle (la recharge de mêlée de 0,45 s ne bride pas). Kill par la chaîne statique (lancer + 2 renvois) : `0,47 + 0,42 + 0,73 + 0,31` ≈ **1,9 s**, contre 0,85 s pour 2 tirs de rail. Avec 3 renvois au plus, une chaîne complète dure ≈ 2,2 s avant un rattrapage obligatoire.
- **Au contact** : 2,5 lancers/s × 21 PvP (rel ≤ 12) ≈ 52 PV/s, en dessous de l'épée (rel 10 : 52 PvP / 0,45 s ≈ 116 PV/s). La Scie n'est pas une arme de bout portant.
- **Aller-retour** sans renvoi : à 5 m 0,3 s (plancher 0,4 s), à 15 m 0,89 s, à 30 m 1,77 s.

**Journal du bot** (`netlog`, et compteurs de l'hôte dans `host_handle` sur `EV_SAW` et `EV_HIT` `W_SAW`) :
- lancers, touches (`sub` 2), plantages, rappels au grappin, ramassages à pied, pertes ;
- **renvois tentés et réussis**, **taux de touche des renvois**, longueur moyenne des chaînes, kills « lancer + renvoi » en moins de 1,5 s ;
- vitesse relative moyenne des touches, temps moyen « désarmé » (de `SAWE_PLANT` à `SAWE_CATCH`), `hitCount[W_SAW]`, TTK avec la Scie seule.

## 12. Étapes d'implémentation

Prérequis : infra (`P.ammo[]`, jauges, loadout, `NET_VER` 4), `hit_player` (plan § 4.4), `try_parry(range, speedMult)` (plan § 4.2).

1. Constantes, champs `Player` et `Proj`, `PR_SAW`, enums, `rayBox` dans `ray_world`, `saw_dmg`, `saw_axis`. **0,5 h**
2. `fire_saw`, branchement du tir et du clic à vide dans `game_tick`, `P.sawCd`. **0,5 h**
3. `saw_tick` en solo : vol, plantage, touche sur drone, retour, ramassage, perte, pièces. **2 h**
4. `grapple_tick` : rappel ; `saw_catch` ; mort et `reset_game`. **1 h**
5. Renvoi : branche de `try_parry`, `saw_bounce` (aide à la visée, garde-fous), `saw_bounce_fx`, points sur touche. **1,5 h**
6. Rendu du disque (couleur selon les renvois), viewmodel, HUD (panneau, pastilles, indicateur de renvoi, direction, viseur de rappel), sons. **2,5 h**
7. Réseau : `net_fx_saw` (6 sous-types), `apply_event` `EV_SAW`, branche `PR_SAW` de `EV_PARRY`, `EV_COIN` sub 3, réémission périodique, nettoyages `EV_KILL` / `EV_RESPAWN` / `EV_INFO`, compteurs de l'hôte. **2,5 h**
8. Bots et tests 1 à 15 ; match bot contre bot, lecture du journal. **2 h**

Total ≈ **12,5 h (M)**.

## 13. Questions ouvertes

1. **Recharge** : « recharge à 0 au rattrapage » et « 0,4 s » sont lues ici comme un plancher de 0,4 s **depuis le lancer**, sans délai ajouté au rattrapage. Ou bien 0,4 s **après chaque** ramassage ?
2. ~~Puissance du renvoi~~ : **tranché le 27/10**, `SAW_RETURN_PARRY_DMG_MULT` = 0,9. Combo statique 94,9 PvP (la cible survit), combo en course ≥ 102 (elle meurt). 2 renvois réussis d'affilée tuent toujours une cible immobile (105,8) : **gardé** (décision du 27/10, « c'est technique »). Pas de ×0,75 sur les renvois suivants.
3. **Renvoi depuis un rappel au grappin** : retenu ici (relance à longue portée, non farmable). On le garde, ou le renvoi est réservé au retour après touche et à la scie parée ?
4. **Parry côté hôte** : faut-il donner une tolérance à `W_SAW` dans `parry_grace` (plan § 4.2), pour annuler une touche croisée avec un parry ? Non retenu par défaut.
5. **×1,25 au parry adverse** : appliqué à la **vitesse** (dégâts déduits de la vitesse relative). Faut-il plutôt un multiplicateur de dégâts comme pour les roquettes ?
6. **Clic avec la scie plantée** : clic sec puis bascule automatique (§ 1.5), ou rester sur l'emplacement vide ?
7. **Pièces** : la scie en retour ou en rappel relance-t-elle aussi les pièces (ici : vol et renvoi seulement) ?
8. **Vitesse minimale de lancer** (`SAW_MIN_SPEED` 20) : acceptable, ou vitesse héritée sans plancher comme le dit le texte ?
9. **Scie plantée dans une boîte détruite** (`B_GLASS`) : elle retombe et se replante (proposé ici), ou elle revient au lanceur ?
