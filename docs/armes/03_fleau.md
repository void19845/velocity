# Fiche 03 : Fléau à chaîne « Fléau »

> Source : [`../IDEES_ARMES_CARTES.md`](../IDEES_ARMES_CARTES.md) § 2.3 (validé), § 1.2 et § 1.5. Interfaces : [`PLAN.md`](PLAN.md) § 3.
> Loadout, `P.meleeKind`, `P.ammo[]`, `ammo_points` (jauges, fiche 00), `W_COUNT`, `NET_VER` 4 : voir `00_infra_loadout_munitions.md` (non redéfinis ici).
> Identifiants : `MK_FLAIL 3`, `W_FLAIL 9`, `EV_COIN` sous-type 2 (pièce frappée). Implémenté dans `src/armes/fleau.c`.
>
> **Mise à jour (patch d'équilibrage, 7 novembre 2026) : le Yoyo ne tire plus la cible.** Le coup garde son arc de 140°, sa portée de 6 m
> (+ 0,05 m par m/s), ses dégâts, son parry à distance (renvoi ×0,8), la pièce frappée, les points de jauge et les recharges ; mais la cible
> touchée (joueur, drone, ennemi des VAGUES) **ne bouge plus** : le recul envoyé est nul. Ont disparu avec la traction : `FLAIL_PULL`,
> `FLAIL_PULL_UP`, `FLAIL_DRONE_STOP`, le combo fléau → rail (« RAPPEL + RAIL », `FLAIL_COMBO_T`, `flailLastT`, `flailLastTgt`, `flailCombos`)
> et l'enchaînement du bot. Reste **une seule traction** : celle du *lanceur dont le fléau est paré* (`FLAIL_PARRIED_PULL`, § 5), réglable
> (0 = aucune). Les passages ci-dessous qui décrivent la traction de la cible sont marqués « avant le patch » ou barrés.
>
> **Mise à jour (10 novembre 2026) : la traction revient, sur APPUI LONG.** Le propriétaire : « on la garde, mais on la change pour que ça soit sur un
> appui long et que ça tracte aussi longtemps que le joueur maintient ». **Appui court** (touche de mêlée relâchée avant `FLAIL_HOLD_T`, 0,15 s) : le coup
> en arc décrit ci-dessous, **sans traction** (inchangé). **Appui long** : le coup part quand même à l'appui, puis la chaîne s'accroche aux cibles de l'arc
> à cet instant et les **tire vers le lanceur tant que la touche est tenue**, sans limite de durée (§ 2 « Traction », § 5, § 6 : `EV_FLAIL`, `NET_VER` 22).
> Reviennent : l'arrêt d'un ennemi à 2,5 m (`FLAIL_DRONE_STOP`), une vitesse de traction de 12 m/s (`FLAIL_PULL_SPEED`) et le combo fléau → rail (on peut
> tirer pendant la traction) ; **reste supprimé** : le bonus visuel « RAPPEL + RAIL » (`FLAIL_COMBO_T`, `flailLastT`). Les passages « avant le patch »
> décrivent l'**ancienne impulsion unique** (12 m/s + 3 vers le haut à chaque touche) : la nouvelle traction est continue et décrite dans les sections « Traction ».

---

## 1. Résumé validé

- **Rôle** : mêlée **à longue portée**, faibles dégâts. **Appui court** : un balayage qui ne déplace pas la cible. **Appui long** : *« viens ici ! »*, mêlée de contrôle qui tire les cibles de l'arc vers soi tant qu'on tient la touche. Pas une arme qui tue : elle remplit les jauges de munitions, frappe les pièces et ramène la cible dans la ligne de mire. Remplace l'épée dans le loadout (on n'a **qu'une** mêlée).
- **Coup** : portée `6 + 0,05 × v_rel` m, arc horizontal de 140° (demi-angle 70°, cos 0,34), recharge 0,6 s, dégâts `18 + 1,5 × v_rel`. Touche **toutes** les cibles dans l'arc.
- **Appui court : aucune traction** (patch d'équilibrage) : le recul envoyé dans `EV_HIT` est nul (`f[1..3] = 0`), la cible reste où elle est. Avant le patch : recul négatif de 12 m/s vers le lanceur + 3 m/s vers le haut, appliqué une fois par la victime dans `apply_event`.
- **Appui long : traction continue** (10 novembre) : touche tenue plus de 0,15 s après l'appui, la chaîne s'accroche aux cibles de l'arc (8 au plus, jamais un boss) et les tire à 12 m/s vers le lanceur (arrêt à 2,5 m d'un ennemi, 2 m d'un joueur) **tant que la touche est tenue**. Relâcher : recharge de 0,5 s. Contre-jeu : portée (8 m), mur (0,25 s), mort / étourdissement / fléau paré du lanceur, vitesse de la victime (dash, bhop, grappin : plus de 16 m/s vers l'extérieur arrache la chaîne), Égide de face (traction ÷ 2).
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
| Bloqué | fléau paré (`EV_MELEE_PARRY`) | `MELEE_PARRIED_CD` 1 s | traction du lanceur vers le pareur ; l'appui long est désarmé, plus d'accroche |
| Maintien | touche encore tenue `FLAIL_HOLD_T` (0,15 s) après l'appui, `P.flailArm` | tant que la touche est tenue | la chaîne s'accroche aux cibles de l'arc à cet instant (`flail_hook_start`), puis les tire (`flail_pull_tick`) : voir « Traction » |
| Recharge de traction | relâchement ou coupure de la traction | 0,5 s (`FLAIL_PULL_END_CD`) | rien (recharge de mêlée) |

**Conditions de touche** (pour chaque ennemi, joueur LAN et pièce à soi), avec `rel = rel_speed(vel_cible)` :

1. `reach = dist(œil, centre) − rayon_cible ≤ 6 + 0,05 × rel`.
2. **Arc horizontal** : `dot(hnorm(d), hfwd) ≥ 0,34`, où `hfwd = (sin yaw, 0, −cos yaw)`. Au contact (`reach < 0,8 m`) on touche toujours, comme l'épée.
3. **Tranche verticale** : `|asin(d.y / |d|) − P.pitch| ≤ FLAIL_VSLAB` (0,6 rad ≈ 34°). Sans elle, l'arc toucherait un drone 6 m au-dessus de la tête.
4. **Ligne de vue : la chaîne ne traverse pas le décor.** `los(œil, centre) || los(œil, centre + 0,6 m en y)` (deux rayons, l'un suffit, pour ne pas rater une cible dont seule la tête dépasse d'un muret).
**Plusieurs cibles** : chaque cible de l'arc reçoit **un** `net_hit` (ou `damage_enemy`) par coup, ce qui respecte la règle d'agrégation. Le dash (+1) et `melee_hit_points(meleePts[P.meleeKind])` ne sont donnés **qu'une fois par coup**, quel que soit le nombre de cibles. Au plus **une pièce** frappée par coup (la plus proche de l'axe de visée) : quatre pièces frappées d'un coup feraient 4 × 28 PvP sur une même cible, ce qui contourne le plafond.

**Appui court : pas de traction** (depuis le patch d'équilibrage) :
- *Joueur touché* : `EV_HIT` avec un recul nul ; la victime ne décolle pas (`grounded` intact) et garde sa vitesse. (Avant le patch : `vy = 3` décollait la victime, puis friction : ≈ 5 m de traction au sol, 12 m/s conservés en l'air.)
- *Drone et ennemis des VAGUES* : `damage_enemy(i, flail_dmg(rel), (0, 0, 0))` ; l'ennemi garde sa vitesse et sa trajectoire (un boss ne reculait déjà jamais). Avant le patch : `v0 = min(12, 2 × (dist − 2,5))` vers l'œil, le drone s'arrêtait à 2,5 m.
- *Kamikaze (solo)* : le fléau qui le touche le fait **exploser au bout de la chaîne** (`damage_enemy(i, 9999, (0, 0, 0))`), mais **sans** `parry_feedback` (à 6 m, ce serait un soin gratuit).

**Traction (appui long)** — touche de mêlée tenue plus de `FLAIL_HOLD_T` = 0,15 s après l'appui (comme la Lance couchée et le tap de l'Égide). *Choix* : le coup en arc part **à l'appui** comme avant (réactivité, fenêtre de parry de 0,10 s, dégâts, pièce frappée, points, recharge de 0,6 s), puis, si la touche est toujours tenue 0,15 s plus tard, la chaîne s'accroche **aux cibles de l'arc à cet instant** (mêmes règles de portée, d'arc, de tranche verticale et de ligne de vue ; l'arc suit la vue du moment) **sans nouveaux dégâts**. Pourquoi pas « décider avant de frapper » : un appui court serait retardé de 0,15 s, et la fenêtre de parry avec lui ; c'est le schéma de la Lance (estoc à l'appui, lance couchée si tenu).
- **Qui** : les `FLAIL_HOOK_MAX` (8) cibles les plus proches de l'arc ; **jamais un boss** (`EN_BOSS`) ; un **kamikaze** dans l'arc à cet instant **explose** au bout de la chaîne (comme au coup, sans soin de parry) ; en PvP, un joueur adverse vivant (jamais un allié).
- **Durée** : tant que la touche est tenue, **sans limite**. Le lanceur ne peut pas relancer de coup pendant la traction (`P.meleeCd` tenu à `FLAIL_PULL_END_CD`, et `game_tick` n'appelle pas `flail_swing` tant que `P.flailPullN > 0`) ; il bouge et tire normalement (combo fléau → rail : le rail tue le drone ramené, testé).
- **Fin** : touche relâchée, plus de cible, ou coupure (ci-dessous) ; **recharge de mêlée de 0,5 s** (`FLAIL_PULL_END_CD`) à partir du relâchement ou de la coupure. Un appui long qui n'accroche **rien** coûte la recharge du coup dans le vide (`FLAIL_WHIFF_CD`, 0,9 s depuis l'appui, sauf si un kamikaze a explosé).
- **Ennemis** (drones et ennemis du solo / VAGUES, via `E[i]`) : à chaque tick, **avant `enemy_tick`**, leur vitesse est ramenée vers l'**œil** du lanceur : consigne `min(FLAIL_PULL_SPEED, FLAIL_PULL_GAIN × (distance − FLAIL_DRONE_STOP))` (12 m/s, qui baisse en approchant de 2,5 m), lissée par `vlerp(vel, consigne, dt × FLAIL_PULL_RESPONSE)` (temps de réponse ≈ 70 ms ; l'effet VORTEX de `ameliorations.c` ajoute lui aussi de la vitesse à `E[j].vel` chaque tick, sans consigne). L'IA du drone, 7 fois plus lente à réagir (`vlerp` à 2/s), ne gagne pas. Mesuré : pointe à 10 m/s, arrêt à 2,4 m de l'œil, tenu là tant que la touche l'est ; relâché, l'IA le reprend (2,4 → 5,1 m en 1 s).
- **Joueurs (PvP)** : la victime se tire **sur sa propre vitesse**, par un mécanisme réseau léger (§ 5, § 6) : à chaque tick (avant `player_tick`), `along` = sa vitesse vers le lanceur ; si `along < consigne` elle gagne `min(FLAIL_PULL_ACCEL × dt, consigne − along)` vers le lanceur, avec `consigne = min(12, FLAIL_PULL_GAIN × (distance − FLAIL_PULL_STOP))` (jamais plus de 12 m/s, une vitesse déjà supérieure n'est pas touchée ; arrêt à 2 m) ; près de l'arrêt (moins de 4 m), ce qui dépasse la consigne est freiné. `FLAIL_PULL_ACCEL` = 140 m/s² **bat la course au sol** (`GROUND_ACCEL × RUN_SPEED` = 114) : courir à l'opposé ne libère pas (mesuré : la victime est tenue vers 2 m).
- **Coupures** (contre-jeu : « une traction de 12 m/s n'est pas une prison ») : (1) la cible s'éloigne au-delà de `FLAIL_RANGE + FLAIL_PULL_SLACK` = 8 m ; (2) un mur coupe la ligne de vue plus de `FLAIL_PULL_LOS_T` = 0,25 s (les deux rayons de `flail_in_arc`) ; (3) le lanceur meurt, est étourdi (Tesla, Enclume) ou n'a plus le Yoyo ; **le changement d'arme à distance ne coupe pas** (c'est ce qui permet le combo fléau → rail) ; (4) le fléau est **paré** (`flail_parried` coupe tout) ; (5) **par la vitesse de la victime** : plus de `FLAIL_PULL_SNAP` = 16 m/s vers l'extérieur (dash 18, slide ou bhop rapide, grappin, rocket jump) arrache la chaîne (popup « CHAINE ARRACHEE », le lanceur est prévenu) ; (6) l'**Égide** de la victime qui fait face à la chaîne divise la traction par deux (l'hôte écrit `f[0]` = `SHIELD_KNOCK_MULT`) ; (7) la victime ou le lanceur peut **mourir**, la mort libère.

**Cas limites** :
- *Mort* : `flail_swing` n'est pas appelé si `P.dead`. Une pièce déjà frappée continue son vol et peut toucher (comme une roquette tirée avant la mort).
- *Respawn* (`EV_RESPAWN`, `reset_game`) : `flailT = 0`, `flailHitN = 0`, `flailParriedT = 0` ; la traction (`flailArm`, `flailPullN`, `flailBy`) est déjà coupée par la mort, et `reset_game` remet `P` à zéro.
- *Changement d'arme* : sans objet pour la mêlée (elle ne change qu'au spawn) ; changer d'arme **à distance** ne coupe pas la traction.
- *Hitstop* : `game_tick` sort tôt (solo seulement : jamais de pause en LAN) ; la touche et les reculs sont déjà appliqués, l'animation reprend après. La traction fait de même : en solo, quelques ticks de pause après un coup qui touche.
- *Fin de match* : l'hôte ignore les dégâts (`matchOver`).
- *Double parry* : si deux victimes d'un même coup parent, l'attaquant reçoit deux `EV_MELEE_PARRY` ; il n'applique que la première traction dans une fenêtre de 0,2 s (`P.flailParriedT`).

## 3. Constantes

```c
/* Fleau a chaine (Yoyo) : arc a longue portee ; appui court SANS traction (patch d'equilibrage), appui long = traction continue */
#define FLAIL_RANGE         6.0f    /* portee de base */
#define FLAIL_RANGE_PER_MS  0.05f   /* +0.05 m par m/s de vitesse relative */
#define FLAIL_ARC_COS       0.34f   /* cos du demi-angle horizontal (70 deg, arc de 140) */
#define FLAIL_VSLAB         0.6f    /* ecart vertical max autour du pitch (rad) */
#define FLAIL_CD            0.6f
#define FLAIL_WHIFF_CD      0.9f    /* coup dans le vide */
#define FLAIL_DMG           18.0f
#define FLAIL_DMG_PER_MS    1.5f
/* (supprimes par le patch du 7 novembre : FLAIL_PULL 12, FLAIL_PULL_UP 3, FLAIL_COMBO_T 1.0 ; FLAIL_DRONE_STOP est de retour, plus bas) */
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
/* traction (appui long), 10 novembre : le coup part a l'appui ; si la touche est encore tenue FLAIL_HOLD_T apres, la chaine s'accroche aux
 * cibles de l'arc a cet instant (sans nouveaux degats) et les tire tant que la touche est tenue */
#define FLAIL_HOLD_T        0.15f   /* tenue plus longtemps apres l'appui : traction (comme la Lance couchee et le tap de l'Egide) */
#define FLAIL_HOOK_MAX      8       /* cibles accrochees au plus, les plus proches d'abord */
#define FLAIL_PULL_SPEED    12.0f   /* vitesse de traction vers le lanceur (m/s) : plus vite que la course (RUN_SPEED 9,5) */
#define FLAIL_PULL_RESPONSE 14.0f   /* ennemis : leur vitesse rejoint la consigne en ~70 ms (1/s) */
#define FLAIL_PULL_ACCEL    140.0f  /* joueurs : acceleration de la traction (m/s2) ; plus que la course au sol (GROUND_ACCEL x RUN_SPEED = 114) */
#define FLAIL_DRONE_STOP    2.5f    /* ennemis : un ennemi tire s'arrete a cette distance de l'oeil */
#define FLAIL_PULL_STOP     2.0f    /* joueurs : une victime tiree s'arrete a cette distance du lanceur */
#define FLAIL_PULL_GAIN     6.0f    /* on ralentit en arrivant : vitesse de consigne <= GAIN x (distance - arret) */
#define FLAIL_PULL_SLACK    2.0f    /* coupee si la cible s'eloigne au-dela de FLAIL_RANGE + SLACK (m) */
#define FLAIL_PULL_LOS_T    0.25f   /* un mur qui coupe la ligne de vue plus longtemps que cela coupe la traction (s) */
#define FLAIL_PULL_SNAP     16.0f   /* PvP : une victime qui s'eloigne plus vite que cela (dash 18, bhop rapide, grappin) arrache la chaine */
#define FLAIL_PULL_REFRESH  0.10f   /* PvP : EV_FLAIL renvoye toutes les 0,1 s par le lanceur tant que la touche est tenue */
#define FLAIL_PULL_LEASE    0.30f   /* PvP : bail cote victime : sans nouvelle depuis 0,3 s, la traction s'arrete */
#define FLAIL_PULL_END_CD   0.5f    /* recharge de melee apres la traction (a partir du relachement ou de la coupure) */
#define FLAIL_HOST_TOL      3.0f    /* PvP : tolerance de latence de l'hote sur la portee de l'accroche (m) */
#define FLAIL_HOST_PARRY_T  0.6f    /* PvP : l'hote refuse l'accroche d'un fleau pare dans ce delai (la parade peut arriver apres l'appui long chez un client lent) (s) */
enum { FLE_START, FLE_PULL, FLE_END, FLE_BREAK };   /* EV_FLAIL.sub : accroche, rafraichissement, fin (lanceur), chaine rompue (victime) */
```

## 4. Données

```c
/* Player */
float flailT;          /* animation restante de la chaine (0..FLAIL_ANIM) */
V3    flailHitPos;     /* point ou la chaine s'est accrochee (premiere cible touchee) */
int   flailHitN;       /* cibles touchees par le dernier coup (0 = chaine libre) */
float flailParriedT;   /* garde anti double traction du LANCEUR apres un parry */
/* (supprimes par le patch : flailLastTgt, flailLastT, le combo fleau -> rail) */
/* traction (appui long), 10 novembre : tout est dans Player (remis a zero par reset_game) */
float flailArm;                    /* temps restant avant l'accroche (0 = aucun appui a suivre) ; armee par flail_swing, avant les touches */
int   flailPullN, flailHk[8];      /* cibles accrochees ; indice fleau : ennemi i, ou MAXE + joueur */
float flailHkLos[8], flailHkSend[8];   /* temps sans ligne de vue ; delai avant le prochain EV_FLAIL (joueurs) */
int   flailBy;                     /* victime LAN : qui me tire (id + 1, 0 = personne) */
float flailLeaseT, flailScale, flailLosT;   /* bail restant, intensite (Egide : 1/2), temps sans ligne de vue */
/* Coin */
int   flailStruck;     /* 1 = frappee au fleau : vol droit, sans gravite */
/* fleau.c, statiques : flailLinkT[MAXP] / flailLinkBy[MAXP] = chaines vues chez tous (victime -> bail restant, lanceur + 1) pour le dessin ;
 * flailPulls, flailPullTgt, flailPullPl, flailSnaps, flailPullTime = journal du bot ; hote.c : flailHostStarts, flailHostPulls, flailHostDenied,
 * flailHostBreaks, flailHostHalved */
```

- `NetPState` : rien de nouveau. `pad & 0x0F = MK_FLAIL` (infra), `melee` = avancement du coup (déjà transmis). **Aucun bit `ST_*`** : le fléau n'a pas d'état maintenu ; l'appui long ne passe **pas** par l'état à 60 Hz mais par `EV_FLAIL` (§ 6, choix expliqué). À l'accroche, `P.meleeT` et `P.flailT` sont remis à 0 : le balayage s'arrête (chez les autres aussi) et la chaîne tendue prend le relais.
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
    P.flailArm = FLAIL_HOLD_T;       /* appui long : armee AVANT les touches (chez l'hote un parry revient tout de suite et la desarme) */
    int hits = 0, parried = try_parry(FLAIL_PARRY_RANGE, FLAIL_PARRY_SPEED);
    P.flailHitN = 0;
    for (i in E vivants) {
        float rel = rel_speed(E[i].vel);
        if (!flail_in_arc(eye, E[i].pos, enemy_radius(E[i].type), rel)) continue;
        if (E[i].type == EN_RUSHER) damage_enemy(i, 9999, v3(0, 0, 0));       /* explose au bout */
        else damage_enemy(i, flail_dmg(rel), v3(0, 0, 0));                    /* aucune traction : recul nul */
        flail_mark_hit(E[i].pos); hits++;
    }
    for (i in joueurs, np_target(i)) {
        float rel = rel_speed(np_vel(i)); V3 c = np_center(i);
        if (!flail_in_arc(eye, c, 0.5f, rel)) continue;
        net_hit(i, flail_dmg(rel) * PVP_SCALE, v3(0, 0, 0), W_FLAIL);        /* recul nul : la cible n'est pas tiree */
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
P.flailT -= dt; P.flailParriedT -= dt;
```

**`apply_event`** :
- `EV_HIT`, `sub == W_FLAIL` : le recul est nul, la victime n'a rien à appliquer. **Pour tous** : `add_beam(main de l'attaquant, centre de la cible, acier, 0,25 s)` : la chaîne tendue sur la victime, sans la tirer (l'attaquant est `ev->from`, `pcenter()` si c'est moi). Le « zip de câble » (`S_HOOK` à hauteur 0,7 chez la victime) a disparu avec la traction.
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

Chiffres : 16 m/s + 4 vers le haut → 0,33 s de vol (5,3 m) puis ≈ 2,7 m de glissade : le lanceur, parti de 6 m, **atterrit sur le pareur**, droit dans son épée (s'il en a une). En plus (traction sur appui long) : `flail_parried` appelle d'abord `flail_pull_end("fleau pare")`, qui coupe toute traction en cours et désarme l'appui long.

**Traction (appui long)** (`fleau.c`, section « Traction » ; `game_tick` dans `boucle.c`) :

```c
/* game_tick, melee : appui = coup, maintien = traction ; flail_swing n'est pas rappele tant que P.flailPullN > 0 */
else if (P.meleeKind == MK_FLAIL) {
    if (!P.dead && meleePress && P.meleeCd <= 0 && P.flailPullN == 0) flail_swing();
    flail_tick(dt, act[A_MELEE]);                         /* maintenu : act, pas actP */
}
flail_victim_tick(dt);                                    /* avant player_tick : un adversaire me tire (EV_FLAIL) */
flail_fx_tick(dt);                                        /* dans fx_tick : baux des chaines vues (dessin) */

static void flail_tick(float dt, int held) {
    if (P.dead || P.meleeKind != MK_FLAIL || etourdi) { flail_pull_end(...); return; }
    if (P.flailArm > 0) {
        if (!held) P.flailArm = 0;                        /* relachee avant FLAIL_HOLD_T : appui court, rien de plus */
        else if ((P.flailArm -= dt) <= 0) { P.flailArm = 0; flail_hook_start(); }
    }
    if (P.flailPullN > 0) flail_pull_tick(dt, held);
}
```

- **`flail_hook_start()`** : pour chaque cible `k` (ennemi `k`, ou joueur `k − MAXE` via `flail_target`) : pas un boss ; `flail_in_arc(eye, c, rayon, rel, 1)` (vue du moment) ; un kamikaze explose (`damage_enemy(.., 9999, 0)`) ; les autres sont triées par distance, les `FLAIL_HOOK_MAX` plus proches sont accrochées (`P.flailHk[]`), un `EV_FLAIL` `FLE_START` part pour chaque joueur. Rien d'accroché : `P.meleeCd = max(P.meleeCd, FLAIL_WHIFF_CD − FLAIL_HOLD_T)`. Sinon : `P.flailT = P.meleeT = 0` (le balayage s'arrête), `S_FLAIL_HIT` discret, `burst` à chaque cible.
- **`flail_pull_tick(dt, held)`** : touche relâchée → `flail_pull_end("relache")`. Pour chaque cible : vivante (`flail_target`), à moins de `FLAIL_RANGE + FLAIL_PULL_SLACK`, ligne de vue (deux rayons, comme le coup) sinon compteur `P.flailHkLos[h]` jusqu'à `FLAIL_PULL_LOS_T` ; sinon `flail_hook_drop` (le dernier prend sa place, `FLE_END` envoyé au joueur lâché). **Ennemi** : `e->vel = vlerp(e->vel, dir × consigne, min(1, dt × FLAIL_PULL_RESPONSE))` vers l'œil. **Joueur** : `FLE_PULL` toutes les `FLAIL_PULL_REFRESH` s ; la même règle de vitesse (plus de `FLAIL_PULL_SNAP` vers l'extérieur) lâche la cible chez le lanceur aussi. Plus de cible : `flail_pull_finish` (recharge 0,5 s, son `S_HOOK` grave, journal) ; sinon `P.meleeCd = max(P.meleeCd, FLAIL_PULL_END_CD)` à chaque tick.
- **`flail_victim_tick(dt)`** (LAN, victime) : décompte du bail `P.flailLeaseT` ; fin si mort, hors partie, fin de match, bail fini, lanceur parti / mort / allié ; puis coupures (distance > 8 m, mur > 0,25 s : `flail_victim_cut(0)` ; vitesse vers l'extérieur > 16 m/s : `flail_victim_cut(1)`, popup « CHAINE ARRACHEE », `S_HOOK`) ; sinon la règle de vitesse du § 2 (`along`, consigne, accélération `FLAIL_PULL_ACCEL × intensité`, freinage près de l'arrêt). Une coupure envoie `FLE_BREAK` au lanceur (il retire sa cible, sa chaîne se rétracte chez tous).
- **`flail_net_event(from, target, sub, scale)`** (appelé par `apply_event`) : `FLE_START` accroche la victime (`P.flailBy`, bail, intensité, `S_HOOK` « zip de câble » et popup « CHAINE DE X ») ; `FLE_PULL` **ne fait que prolonger** un bail du même lanceur (un rafraîchissement en retard après la fin ne raccroche pas) ; `FLE_END` (lanceur) et `FLE_BREAK` (victime) coupent. Tout le monde garde la chaîne dans `flailLinkT/By[victime]` pour la dessiner.

## 6. Réseau

| Quoi | Qui calcule | Événement |
|---|---|---|
| Touche du fléau (dégâts, recul nul) | lanceur, sur `NP[i].disp` | `EV_HIT`, `sub = W_FLAIL`, `f[0]` dégâts PvP, `f[1..3]` = 0 (avant le patch : recul négatif de traction) |
| Traction du lanceur paré | lanceur, à la réception de `EV_MELEE_PARRY` (`flail_parried`) | — |
| Animation de la chaîne | chacun, depuis `NetPState.melee` + `yaw`/`pitch` + `pad & 0x0F` | `EV_MELEE` (`net_fx_melee`) pour le son et l'arc |
| Pièce frappée (départ) | lanceur | `EV_COIN` `sub = 2`, `netId`, `f[0..2]` pos, `f[3..5]` vitesse |
| Pièce frappée (touche) | lanceur | `EV_HIT` `W_COIN`, puis `EV_COIN` `sub = 1` |
| Pièce frappée (rail) | tireur du rail (`ricochet`) | `EV_COIN` `sub = 1` (existant) |
| Parry du fléau | **hôte** (`host_handle`) | `EV_MELEE_PARRY` |
| Traction : accroche, maintien, fin (appui long) | lanceur, ~10 Hz tant que la touche est tenue ; l'hôte arbitre (`flail_host`) | `EV_FLAIL` (24), `sub` = `FLE_START` / `FLE_PULL` / `FLE_END`, `target` = victime, `f[0]` = intensité (1, ou 0,5 face à une Égide : écrite par l'hôte) |
| Traction : chaîne rompue par la victime (trop loin, mur, arrachée) | victime | `EV_FLAIL` `sub = FLE_BREAK`, `target` = **lanceur** |
| Traction : la vitesse de la victime | **la victime**, chacun simule le sien | — (`flail_victim_tick`, sur son bail) |

**Traction en PvP (appui long), `NET_VER` 22** : la victime est **tirée sur sa propre vitesse** (chaque joueur simule son mouvement), jamais par une position imposée. Le lanceur envoie `FLE_START` à l'accroche (0,15 s après l'appui), puis un `FLE_PULL` toutes les `FLAIL_PULL_REFRESH` = 0,1 s tant qu'il tient la touche, puis `FLE_END` au relâchement ; la victime enregistre « accroché par X jusqu'à t + `FLAIL_PULL_LEASE` (0,3 s) » à chaque événement et s'applique la règle de vitesse de § 2 à chaque tick tant que le bail court (trois rafraîchissements perdus d'affilée la libèrent ; chaque événement part en trois exemplaires, `net_queue`). *Pourquoi un événement plutôt qu'un champ de `NetPState`* (un octet `spare[0]` en masque de victimes aurait coûté zéro paquet) : l'événement passe par `host_handle`, point d'arbitrage unique du jeu (Égide, équipes, vivants), se teste en le fabriquant (`apply_event`), garde `NetPState` figé, et ne coûte que ~1,5 Ko/s par victime (52 octets × 3 × 10 Hz).
- **Arbitrage de l'hôte** (`flail_host`, `hote.c`) : `FLE_START` / `FLE_PULL` annulés (ni appliqués, ni relayés) si lanceur ou victime absents / morts, fin de match, **alliés** (équipes), lanceur sans le Yoyo (`NetPState.pad & 0x0F != MK_FLAIL`), victime à plus de `FLAIL_RANGE + FLAIL_PULL_SLACK + FLAIL_HOST_TOL` = 11 m, ou **fléau du lanceur paré depuis moins de `FLAIL_HOST_PARRY_T`** = 0,6 s (`flailHostParriedAt[]`, posé par `host_melee_parry` : chez un client lent la parade arrive après l'accroche, l'hôte ne doit pas tirer une victime qui a paré). Sinon l'hôte écrit l'**intensité** dans `f[0]` avant de relayer : `SHIELD_KNOCK_MULT` (0,5) si l'Égide de la victime est levée, valide et fait face à la chaîne (`host_shield_ok`, tolérance `SHIELD_COS_HOST`), 1 sinon ; il la ré-évalue à **chaque** rafraîchissement (une Égide levée en cours de traction la divise aussitôt par deux). `FLE_END` et `FLE_BREAK` ne font que retirer une traction : toujours relayés. Ni dégâts ni points : la traction ne change aucune PV, l'arbitrage des dégâts reste celui de l'`EV_HIT` du coup.
- **Réception** (`apply_event`) : chacun garde la chaîne (`flailLinkT/By`) pour la dessiner (lanceur → victime, spectateurs compris) ; seule la victime applique la vitesse. Un `FLE_PULL` d'un lanceur qui n'a pas de bail sur moi est ignoré.
- **Confiance** : comme le reste du jeu, l'hôte ne vérifie pas que le lanceur tient réellement la touche (un client modifié pourrait tirer un adversaire à moins de 11 m sans coup préalable : documenté, comme les touches validées chez le tireur) ; la portée, l'équipe, l'arme et la vie le sont.

**Arbitrage de l'hôte** : `W_FLAIL` entre dans la liste des coups parables (PLAN § 3). Le test reste purement temporel (`nowT − NP[v].lastParryT < MELEE_PARRY_GRACE`) : la portée de 6 m est assurée par le lanceur. Le **choc** (`clash`) reste réservé à `W_MELEE` : deux fléaux lancés l'un vers l'autre dans la même fenêtre sont **tous deux parés**, et les deux lanceurs sont tirés l'un vers l'autre (« chaînes croisées »), sans code de plus. Pour que l'attaquant sache *quelle* arme a été parée, l'hôte écrit l'arme dans `EV_MELEE_PARRY.f[0]` (`(float)ev->sub`) ; `sub` garde son sens (choc) et le code existant ne change pas. Statistiques : `meleeParries` compte aussi les fléaux parés, `hitCount[W_FLAIL]`.

**Latence** : à 6 m, l'écart dû au lissage de `disp` (≤ 0,1 s d'extrapolation) est négligeable devant la portée. La pièce frappée est vue chez les autres avec un retard de ~50 ms (≈ 2 m à 45 m/s) ; on n'extrapole pas (pas d'estimation de latence dans `NetEvent`). Course possible entre la touche de la pièce (lanceur) et un rail adverse dessus : les deux effets peuvent avoir lieu, comme aujourd'hui pour les pièces (l'hôte n'arbitre pas les pièces).

## 7. Interactions

- **Parry (le porteur du fléau pare)** : orbes et roquettes à 6 m (au lieu de 3,9 m), renvoi à 28,8 m/s, `parry_feedback` complet, des points de jauge (`melee_parry_points`, infra). Un kamikaze n'est pas « paré » par le fléau (pas de soin), il explose simplement.
- **Pièces** : enchaînement « pièce → fléau » : la pièce lancée (`P.vel + 9 f + 6 y`) est à ~3 m devant et 1 m plus haut après 0,3 s, avec `rel ≈ 9` → portée 6,45 m : bien dans l'arc. Les pièces **adverses** sont ignorées par le fléau (voir Questions).
- **Rail** : *pièce frappée → rail dans la pièce* = ricochet ×1,5 au moment choisi. **Fléau → rail** : avec l'appui long, le combo **redevient possible** (la cible est tenue à 2–2,5 m, dans la ligne de mire, pendant que la touche est tenue ; testé : le rail tue le drone ramené) ; le bonus visuel « RAPPEL + RAIL » (`impact_frame` de 0,06 s) reste **supprimé** (aucun bonus de dégâts, il n'existait que pour fêter la cible tirée). Changer d'arme à distance ne coupe pas la traction.
- **Roquettes** : la cible ramenée peut de nouveau être visée aux pieds (appui long, puis roquette sous la cible tenue).
- **Grappin** : grappin sur un joueur + fléau à l'arrivée : la vitesse relative élevée allonge la portée et augmente les dégâts (18 + 1,5 × 30 = 63 PvE / 44 PvP). Une victime qui se grappine loin du lanceur à plus de 16 m/s arrache la chaîne ; sa traction (95 m/s²) et celle du fléau (140 m/s²) s'additionnent tant qu'elle est sous les 16 m/s.
- **Vide** (cartes Puits, Canyon, plan de mort à venir) : l'appui court ne déplace pas la cible. L'appui long la tire de nouveau vers le lanceur, **sur sa vitesse et avec les collisions normales** : un lanceur au bord peut ramener une victime au-dessus du vide (elle est libre dès qu'elle dépasse 16 m/s vers l'extérieur, ou à 8 m).
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

**Traction (appui long)** : `draw_flails(eye)` dessine, tant que la traction dure, **une chaîne tendue (`draw_chain(main, cible, 0)`, sans mou) de la main gauche jusqu'à chaque cible accrochée**, avec une tête (`flail_head_model`) et un halo additif à l'extrémité, à la place du balayage (`P.flailPullN > 0`). Les autres joueurs voient la même chose depuis les baux `EV_FLAIL` (`flailLinkT/By[victime]`, du lanceur à la victime, y compris quand la victime, c'est moi) ; leur tête n'est plus pendue à leur main pendant la traction (`flail_pulling(i)`, `joueurs.c`). Viewmodel (`draw_flail_vm`) : manche levé (`lift = 0,85`), sans tête pendue (elle est au bout des chaînes). Sons : à l'accroche, `S_FLAIL_HIT` discret (volume 0,4, hauteur 1,3) chez le lanceur ; chez la victime le **zip de câble** de l'ancienne traction (`S_HOOK`, hauteur 0,7) ; à la fin, `S_HOOK` aigu et faible (hauteur 1,8) chez le lanceur ; chaîne arrachée : `S_HOOK` (1,6) et popup « CHAINE ARRACHEE » chez la victime, popup « CHAINE DE X » à l'accroche.

**HUD (`draw_hud`)** : le panneau « EPEE » devient « FLEAU (vs cible) », `%d dmg %.1fm` avec `flail_dmg`/`flail_range`, barre de recharge sur `FLAIL_CD`. `melee_preview` garde son cône de visée (0,92). Petits crochets autour du viseur quand une cible est dans l'arc et à portée (même test que `flail_in_arc`, sans `los` pour le coût).

**Sons (`make_sounds`, avant `S_COUNT`)** :
- `S_FLAIL_SWING` (0,35 s) : souffle (bruit filtré passe-bas dont la coupure monte de 300 à 1800 Hz, enveloppe 0,02 s puis `exp(−t × 7)`) + **cliquetis** : toutes les 1/45 s, une impulsion de bruit `exp(−τ × 900)` d'amplitude aléatoire 0,3 à 0,6, enveloppe globale `exp(−t × 6)`.
- `S_FLAIL_HIT` (0,4 s) : choc métallique inharmonique (`sin` 820 + 1230 + 1960 Hz, `exp(−t × 10)`) + coup sourd 70 Hz `exp(−t × 18)` + bruit `exp(−t × 60)`. Joué à 0,6 de hauteur pour « CHAINE RETOURNEE ».
- `S_FLAIL_COIN` (0,3 s) : « ting » 2600 + 3900 Hz `exp(−t × 14)` + souffle descendant court.
- Le « zip de câble » de la victime (`S_HOOK` à hauteur 0,7) avait disparu avec la traction du 7 novembre ; il est **de retour** à l'accroche de l'appui long (`flail_net_event`).

## 9. Bots

Dans `bot_think`, si `P.meleeKind == MK_FLAIL` :
- **Attaque** : à la place du test `bd < melee_range(...) + 0,5`, frapper si `see && bd > 2,5 && bd < flail_range(rel) + 0,3` et `P.meleeCd <= 0` (`P.yaw = aimYaw`, `botPress[A_MELEE] = 1`).
- **Suite** : *supprimée avec la traction* (avant le patch : moins de 0,3 s après une touche, le bot tirait sur la cible tirée vers lui, combo fléau → rail). Pas rétablie : le bot tire de toute façon sur sa cible avec son comportement habituel.
- **Appui long (10 novembre)** : une fois sur deux, quand la cible est à plus de 3,5 m (`FLAIL_PULL_STOP + 1,5`), le bot **tient la touche** (`botHeld[A_MELEE]`, `fcHold` = 0,9 s au plus) : la chaîne tire la cible pendant ce temps, il relâche si rien n'est accroché ou quand la traction finit. Il ne relance pas de coup pendant l'appui long (`P.flailPullN == 0`, `fcHold <= 0`). Choix **simple et sans risque** : le match bot exerce ainsi toute la voie réseau (START, PULL, END, vitesse de la victime, arrachée par le dash du bot adverse).
- **Pièce frappée** : variante du `coinPlan` : à moins de 25 m, lancer une pièce puis frapper au fléau 0,3 s plus tard au lieu de tirer dedans.
- **Défense** : la détection « adversaire qui lance sa mêlée » passe de 5 m à 7 m quand `(NP[i].st.pad & 0x0F) == MK_FLAIL`.
- **Journal** (`netlog`) : `fleau : %d cible(s), v_rel %.1f, premiere : %s`, `piece frappee -> %s`, `fleau : traction, %d cible(s) accrochee(s)`, `fleau : traction finie (%s), %.1f s au total`, `fleau : %s m'accroche` (victime) ; fin de match : `fleau traction (moi)` (tractions, secondes tenues, cibles accrochées dont joueurs, chaînes arrachées en victime) et `fleau traction (arbitre)` (accroches, dont à moitié par l'Égide, rafraîchissements, refus, chaînes rompues par la victime). Plus de `combo fleau -> rail` (supprimé depuis le patch).

## 10. Tests (`run_tests`)

Dans le couloir dégagé (x = −20, joueur en `(−20, 0, 40)`, `yaw = 0` regarde vers −z), `P.meleeKind = MK_FLAIL` :

1. **Dégâts d'un drone, sans traction** : drone à `(−20, 1.6, 34.5)`, `flail_swing()` → PV 82, vitesse du drone 0 et position inchangée juste après le coup ; après 60 `enemy_tick(TICK)`, distance à l'œil > 4 m (mesuré 5,05 m). Avant le patch : `vel.z = 6` vers le joueur et distance **minimale** < 3,5 m (≈ 2,5).
2. **À 20 m/s** : `P.vel = (0, 0, −20)`, drone à 7,6 m (portée 7 m + rayon 0,9) → PV 52, dash +1.
3. **Arc** : drones à 0°, ±60° et 80° à 5 m → 3 touchés, celui à 80° intact.
4. **Ligne de vue** : boîte ajoutée temporairement entre le joueur et un drone à 5 m (`addbox`, puis `nb--`) → intact ; `P.swingLanded == 0`, puis recharge de raté ≈ 0,8 s après 0,1 s.
5. **Kamikaze** : à 5 m → mort, PV du joueur inchangés (pas de soin de parry).
6. **Pièce frappée** : pièce à moi en `(−20, 2.5, 37)`, drone à `(−20, 2, 15)` → `flailStruck = 1`, vitesse 45 ; 90 `coin_tick` (22 m en ~0,46 s) → drone à 60 PV, pièce morte.
7. **Pièce frappée + rail** : même départ, puis `fire_rail()` visant la pièce après 3 ticks → `lastChain = 1`, drone mort (150).
8. **Parry à 6 m** : orbe à 5,5 m devant → renvoyée (`PR_PARRIED`), vitesse 28,8 ; avec l'épée, même position → pas renvoyée.
9. **LAN simulé, touche** (`netMode = NET_HOST`, `NP[1]` à `(−20, 0, 35)`) : PV de `NP[1]` = 87,4 (18 × 0,7) ; dernier événement de `outq` : `f[1..3] = (0, 0, 0)` (avant le patch : `(0, 3, 12)`).
10. **LAN simulé, fléau paré** : `NP[1].lastParryT = nowT`, `EV_HIT W_FLAIL` de 0 vers 1 → `NP[1].hp = 100`, `P.vel.z ≈ −16` (le *lanceur* est tiré vers `NP[1]`), `P.meleeCd ≥ 1` ; deux pareurs : une seule traction (|v| 16,5).
11. **Bot au fléau** (armes à distance vides) : cible à 5 m → 1 touche en moins de 0,35 s, PV de la cible 86 à 87,4 selon `v_rel`, recul envoyé `(0, 0, 0)`. (Avant le patch : le coup puis le rail dans les 0,3 s, « combo fléau → rail ».)

**Traction sur appui long** (lignes « fleau traction … » et « fleau appui court », `run_tests`, `game_tick` complet avec `botPress` / `botHeld[A_MELEE]` pour l'appui et le maintien ; les lignes 1 à 11 ci-dessus restent inchangées et valides : l'appui court n'a pas bougé) :

12. **Drone à 5,5 m, touche tenue 1,5 s** : PV 82 (le coup part à l'appui), accroche à 0,15 s, distance 2,59 m à 0,6 s, mini et finale 2,37 m (arrêt vers 2,5 m, tenu), vitesse max vers l'œil 10 m/s ; relâché : plus accroché, recharge 0,50 s, et son IA le reprend (distance 2,37 → 5,10 m une seconde plus tard).
13. **Appui court** : touche tenue 1 tick, ou 0,10 s (< 0,15 s) → 0 traction, distance mini 4,93 m (le drone n'est jamais tiré), PV 82 (le coup est parti).
14. **Boss** : boss et drone dans l'arc → 1 cible accrochée (le drone), vitesse du boss 0 ; un kamikaze dans l'arc à l'accroche explose.
15. **Coupures** : distance (drone téléporté à 9,5 m) lâché au tick suivant ; **mur** ajouté entre nous : lâché au bout de 0,25 s ; lanceur mort : lâché au tick suivant ; recharge de 0,50 s à chaque fois.
16. **Fléau → rail** : appuis répétés à chaque tick touche tenue → un seul coup de fléau ; le drone ramené à 2,48 m est tué par un rail pendant la traction, qui finit (plus de cible).
17. **LAN, je tire** (hôte, cible à 6 m) : 1 `FLE_START` (intensité 1,0), 3 `FLE_PULL` en 0,5 s, PV de la cible 87,4 (un seul coup, aucun dégât de traction), au relâchement 1 `FLE_END`.
18. **LAN, victime** (`apply_event` d'un `FLE_START` de `NP[1]` à 6 m, rafraîchi toutes les 0,1 s, `flail_victim_tick` + `player_tick`) : vitesse max 11,4 m/s (≤ 12), distance mini = finale 2,04 m (arrêt vers 2 m), accrochée ; sans rafraîchissement : encore accrochée à 0,23 s, libre à 0,33 s (bail de 0,3 s).
19. **Contre-jeu** : victime qui court à l'opposé (touche arrière tenue, départ à 4,5 m) : tenue, vitesse −0,3 m/s (jamais vers l'extérieur), distance 2,21 m (≈ l'arrêt) ; **dash à 18 m/s vers l'extérieur** : chaîne arrachée, `FLE_BREAK` émis vers le lanceur, compteur +1 ; **Égide** (intensité 0,5 dans le START et les PULL) : vitesse max 5,7 m/s (≤ 6,2).
20. **Arbitrage de l'hôte** : intensité 1,0 sans Égide et 0,5 face à une Égide levée (écrite dans le `FLE_START` relayé) ; lanceur sans le Yoyo, victime à 20 m ou alliée : refusés (rien dans la file), un `FLE_START` en règle : relayé.
21. **Parry** : `NP[1].lastParryT = nowT` puis appui long → 0 traction, 0 `FLE_START`, recharge ≈ 0,67 s ; un `FLE_START` en retard (client lent) est **refusé par l'hôte** (parade vieille de 0,34 s), puis accepté 0,5 s plus tard ; `flail_parried` en pleine traction → 0 cible, 1 `FLE_END`.
22. **Observateur** : `FLE_START` de `NP[2]` sur `NP[1]` → chaîne gardée (bail 0,30 s, 0 après 0,35 s) ; un `FLE_PULL` de `NP[1]` sur moi sans accroche préalable ne m'accroche pas.

## 11. Équilibrage

| v_rel (m/s) | Portée | PvE | PvP (× 0,7) | Touches pour tuer (PvP, 100 PV) |
|---|---|---|---|---|
| 0 | 6,0 m | 18 | 12,6 | 8 |
| 10 | 6,5 m | 33 | 23,1 | 5 |
| 20 | 7,0 m | 48 | 33,6 | 3 |
| 30 | 7,5 m | 63 | 44,1 | 3 |
| 40 | 8,0 m | 78 | 54,6 | **2** (voir Questions) |
| Pièce frappée | — | 40 | 28 | 4 |

Plafond de 60 atteint seulement à `v_rel ≈ 45 m/s`. DPS PvP à v_rel 20 : 33,6 / 0,6 s = 56/s (épée plafonnée : 60 / 0,45 s = 133/s). Enchaînement type : fléau (23) + rail (60) + fléau (23) = 106 → kill en ~1,5 s (sans traction, la cible doit être encore à portée du fléau entre les deux coups).

Sans traction (appui court), le fléau perd son rôle de « lanceur de combos » : il reste une mêlée de zone à 6 m (arc de 140°), avec pièce frappée et parry à distance. Le patch d'équilibrage ne compense pas cette perte (aucune valeur de dégâts ou de portée modifiée) ; à surveiller dans les matchs bot. **Avec l'appui long (10 novembre), il retrouve ce rôle** : un appui long = le coup (dégâts du tableau ci-dessus) puis une traction sans dégât ; le rail, la roquette ou le Shotgun tirent ensuite la cible tenue à 2–2,5 m, dans la ligne de mire. Aucune valeur de dégâts ni de portée n'est modifiée : le prix du combo est la recharge de 0,5 s après la traction, le temps où la touche est tenue (pas de nouveau coup) et le contre-jeu de la victime (vitesse, mur, Égide, parry).

**Mesure Yoyo** (patch « plus de traction ») : deux matchs bot contre bot de 90 s par version, réseau réel sur la machine, Yoyo des deux côtés
(`--loadout 0,1,3,0`, rail + roquette + Yoyo, compétence PIÈCE ; « avant » = le commit précédent, « après » = sans traction).

| Version | Kills (match 1 / 2) | TTK moyen | Coups de Yoyo | Coups qui touchent | Touches arbitrées | Pièces frappées (qui touchent) | Combos fléau → rail |
|---|---|---|---|---|---|---|---|
| avant (traction) | 7 / 7 | 9,94 / 4,48 s | 13 / 14 | 4 / 6 | 4 / 6 | 5 (2) / 4 (1) | 0 / 2 |
| après (sans traction) | 8 / 6 | 4,51 / 1,98 s | 17 / 16 | 3 / 6 | 3 / 6 | 8 (3) / 6 (1) | — (supprimé) |

Deux matchs par version : **les écarts de TTK (2 à 10 s) sont du bruit**, rien ne s'emballe ni ne cale. À retenir : les bots frappent aussi souvent et
touchent autant (3 à 6 coups qui touchent contre 4 à 6), les 6 à 8 kills par match sont les mêmes, et l'ancien combo fléau → rail n'avait servi que
2 fois en 2 matchs. La perte réelle (le Yoyo ne ramène plus la cible dans la ligne de mire) se jouera surtout en partie humaine.

**Mesure Yoyo (traction sur appui long, 11 novembre)** : matchs bot contre bot en **réseau réel** sur la machine (hôte + client, `--loadout 0,1,3,0`,
Yoyo des deux côtés ; le bot tient la touche une fois sur deux à plus de 3,5 m, 0,9 s au plus). LE DOJO (`--map 4`, petite carte) provoque des
combats proches ; la variante de vérification (copie temporaire du code, hors dépôt) relève `FLAIL_PULL_SNAP` à 60 m/s et fait tenir la touche à chaque
coup à plus de 3,5 m (1,5 s au plus) pour exercer une traction soutenue.

| Match | Kills | TTK moyen | Coups de Yoyo (hôte / client) | Qui touchent | Fléaux parés | Tractions (durée tenue) | `EV_FLAIL` vus par l'hôte | Chaînes arrachées par la victime |
|---|---|---|---|---|---|---|---|---|
| ARENE 60 s, code final | 4 | 9,13 s | 6 / 6 | 2 / 1 | 1 | 0 | 0 | 0 |
| DOJO 90 s, 1re passe | 19 | 3,22 s | 23 / 24 | 10 / 11 | 5 | 2 (0,1 s) | 2 START, 2 BREAK | 2 |
| DOJO 90 s, code final | 16 | 2,46 s | 20 / 22 | 11 / 10 | 9 | 3 hôte + 1 client (0,7 s) | 4 START, 4 PULL, 1 BREAK | 1 |
| DOJO 90 s, variante de vérification | 16 | 3,41 s | 20 / 19 | 10 / 8 | 7 | 2 (1,3 s : 1 s à 2,1 m puis kill) | 2 START, 12 PULL | 0 |

À retenir : **aucun plantage, aucune désynchronisation visible** (les deux processus finissent sur le code 0, le client voit « Hote m'accroche » pour chaque
START arbitré, l'hôte compte 0 refus) ; la carte du DOJO donne 16 à 19 kills en 90 s avec un temps pour tuer de 2,5 à 3,4 s (pas de comparaison directe avec
les 6 à 8 kills de la grande carte mesurés plus haut) : la traction ne fait ni caler ni emballer le match. **Les bots à plus de 16 m/s vers l'extérieur
arrachent presque toujours la chaîne** (cf. question 15 ci-dessous) : dans la variante de vérification, la traction soutenue ramène la victime à 2,1 m (lue
dans le journal du client : « cible 2,1 m »), le rail la tue, 12 rafraîchissements `FLE_PULL` traversent l'hôte, et la mort du lanceur coupe proprement la
traction (`FLE_END`, « traction finie (mort) »). Peu de tractions par match (le bot ne tient la touche que de temps en temps) : la mesure est un contrôle de
non-régression du réseau et de la physique, pas un réglage de l'équilibrage, qui se jouera en partie humaine.

À mesurer dans le journal du bot : touches de fléau par minute, `v_rel` moyen à la touche, nombre de cibles par coup, fléaux parés (`meleeParries` filtré par arme), `ttkSum/ttkN` avec et sans fléau dans le loadout ; pour la traction : lignes `fleau traction (moi)` et `fleau traction (arbitre)`.

## 12. Étapes d'implémentation

Prérequis : infra (loadout, `meleeKind`, `ammo_points` (jauges, fiche 00), `W_COUNT`, `NET_VER` 4).

1. Constantes, champs `Player`/`Coin`, remises à zéro, aiguillage dans `melee()`, `pack_state` normalisé (0,5 h).
2. `flail_swing` solo (arc, tranche, ligne de vue, kamikaze ; la traction des drones a été retirée par le patch) + tests 1 à 5 (1,5 h).
3. `try_parry(range, speedMult)`, fenêtre et raté par type de mêlée dans `game_tick` + test 8 (0,5 h).
4. PvP : `net_hit W_FLAIL`, `W_FLAIL` parable, `f[0]` de `EV_MELEE_PARRY`, traction de l'attaquant paré + tests 9 et 10 (1 h).
5. Pièce frappée : `flail_strike_coin`, branche de `coin_tick`, `EV_COIN` sous-type 2 + tests 6 et 7 (2 h).
6. Rendu : `draw_chain`, viewmodel, joueurs distants, beam de `EV_HIT`, HUD, trois sons (2 h).
7. Bot et journal, puis matchs bot contre bot pour régler les chiffres (2 h).

Total ≈ 9,5 h : coût **M** (≈ 5 h sans la pièce frappée ni le bot).

8. **Traction sur appui long** (10 novembre, ≈ 6 h) : constantes `FLAIL_HOLD_T`… et `FLE_*` (`equilibrage.c`), champs `Player` (`flailArm`, `flailPullN`, `flailHk[]`, `flailBy`…), `flail_tick` / `flail_hook_start` / `flail_pull_tick` / `flail_victim_tick` / `flail_net_event` (`fleau.c`), aiguillage dans `game_tick` (`boucle.c`), `EV_FLAIL` + `net_fx_flail` + `apply_event` + `NET_VER` 22 (`protocole.c`), `flail_host` (`hote.c`), chaînes tendues (`armes_rendu.c`, `joueurs.c`), appui long du bot, journal, tests 12 à 22, scènes `--shot fleau_traction` / `dm_fleau_traction`.

## 13. Questions ouvertes

1. **Plafond de vitesse relative** : au-delà de `v_rel ≈ 37 m/s` (grappin sur joueur, voies à contresens), le fléau passe à 2 touches et sort de son rôle. Plafonner `v_rel` à 25 m/s (max 55,5 PvE / 38,9 PvP, 3 touches) ?
2. **Traction en PvP** : ~~sans objet~~ (retirée le 7 novembre) → **rouverte le 10 novembre** : revenue sur appui long, voir la liste de choix à valider en fin de section.
3. **Grappin de la victime** : une victime qui se grappine loin du lanceur à plus de 16 m/s arrache la chaîne (`FLAIL_PULL_SNAP`) ; en dessous, la traction du grappin (70 m/s² vers un mur, 95 vers une cible) et celle du fléau (140 m/s²) s'additionnent. Acceptable ?
4. **Kamikaze** : explosion au bout de la chaîne sans soin (retenu, recul nul), ou simple dégât + recul *positif* ?
5. **Recharge de raté** 0,9 s (comme l'épée en absolu, ×1,5 au lieu de ×2 en relatif) : à valider.
6. **Pièce frappée parable ?** Proposé : non en v1 ; le contre-jeu est l'esquive (trajectoire droite et visible) ou le rail qui la vole. Sinon il faudrait un événement « pièce renvoyée ».
7. **Arme parée dans `EV_MELEE_PARRY`** : `f[0]` proposé ; à harmoniser avec les autres fiches de mêlée (ou figer dans PLAN § 3).
8. **Signature de `try_parry`** : `(range, speedMult)` proposée ; les fiches Enclume, Lance, Impulsion et Égide la touchent aussi.
9. **Lecture de « cible visible la plus proche avec aide `PARRY_AIM_COS` »** : proposé = d'abord la cible alignée avec le viseur (9°), sinon la plus proche devant soi. Ou seulement la plus proche, comme `ricochet` ?
10. **Pièces adverses** : le fléau peut-il frapper une pièce adverse (vol, comme le rail) ? Proposé : non.
11. **Délai de touche** : instantané comme l'épée (proposé), ou 0,08 s pour que la chaîne soit lisible et le parry plus juste à 6 m ?
12. **Tranche verticale** `FLAIL_VSLAB` = 34° : assez pour attraper un drone en vol rasant sans viser ?

**Choix à valider pour la traction sur appui long (10 novembre)** — la décision de fond (appui long, tracte tant que la touche est tenue) est celle du propriétaire ; le reste a été choisi et est réglable dans `equilibrage.c` :
13. **Le coup part à l'appui, l'accroche vient 0,15 s plus tard** (recommandation retenue) plutôt que « décider avant de frapper » : un appui court reste réactif (parry de 0,10 s compris). Conséquence : un appui long fait **d'abord** les dégâts du coup (18 + 1,5 × v_rel, × 0,7 en PvP), puis la traction sans dégâts ; l'arc de l'accroche est recalculé avec la vue de ce moment (on peut « finir » de viser pendant les 0,15 s).
14. **12 m/s, arrêt à 2,5 m (ennemis) et 2 m (joueurs), 140 m/s² pour les joueurs** (`FLAIL_PULL_SPEED`, `FLAIL_DRONE_STOP`, `FLAIL_PULL_STOP`, `FLAIL_PULL_ACCEL`) : la traction bat la course au sol (114 m/s²), pas un dash. Trop fort / trop faible ?
15. **Chaîne arrachée au-delà de 16 m/s vers l'extérieur** (`FLAIL_PULL_SNAP`) : c'est ce qui rend « le dash, le slide rapide et le bhop » efficaces contre la traction (un dash fait 18) ; sans lui, 140 m/s² l'annulerait en 0,13 s. Seuil à valider. **Mesuré en match bot** (LE DOJO, les bots bhoppent à 20–30 m/s) : les deux accroches du match ont été arrachées par la victime dans les 0,1 s (vitesse vers l'extérieur > 16 m/s) ; avec le seuil relevé à 60 m/s (variante de vérification), les tractions durent (1 s, victime ramenée à 2,1 m, puis rail) : en pratique la traction n'accroche pas une cible qui fuit vite, elle tient celles qui se battent ou courent. Si c'est trop sévère en partie humaine, monter `FLAIL_PULL_SNAP` (20 : le dash seul n'arrache plus, il faut un bhop ou un slide).
16. **Recharge de 0,5 s après la traction** (`FLAIL_PULL_END_CD`), **0,9 s si rien n'est accroché** (`FLAIL_WHIFF_CD`, appui long dans le vide).
17. **8 cibles au plus** (`FLAIL_HOOK_MAX`), les plus proches ; aucune limite de durée (demandé) ; coupure à 8 m (`FLAIL_RANGE + 2`) et 0,25 s sans ligne de vue.
18. **Égide de face : traction ÷ 2** (et non bloquée) : c'était la règle d'avant (« recul ÷ 2 par l'hôte »). Changer d'arme à distance **ne coupe pas** la traction (sinon pas de combo fléau → rail).
19. **`EV_FLAIL` à 10 Hz plutôt qu'un champ de `NetPState`** : voir § 6. Si le coût de 1,5 Ko/s par victime gêne, le masque de victimes dans `spare[0]` est la variante à zéro paquet.
20. **VAGUES** : la boutique v2 n'autorise que la Lance et l'Impulsion comme mêlée (`vg_mk_ok`) : le Yoyo n'y est pas équipable, la traction des ennemis (codée, testée hors VAGUES) n'y est donc pas jouable aujourd'hui. À rouvrir si le Yoyo revient dans le loadout VAGUES (ses pools de l'arbre ne sont pas modifiés).
