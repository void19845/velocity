# Fiche 07 : Convertisseur cinétique « Élan »

> Source : [`../IDEES_ARMES_CARTES.md`](../IDEES_ARMES_CARTES.md) § 1.5 (jauges), § 3.2 et § 6 (arme recommandée en premier).
> Interfaces figées : [`PLAN.md`](PLAN.md) § 3. Prérequis : `00_infra_loadout_munitions.md` (loadout, `P.ammo[]` et `P.ammoGauge[]`, gain de points par la mêlée et les cellules, bascule à vide, `NET_VER` 4).
> Identifiants : `WP_KINETIC 3`, `W_KINETIC 13`, effets réseau via `EV_RAIL`. Coût : **S**.
> Mise à jour du 27/10 : fenêtre du dash-tir (§ 2.5), munitions par jauge sans recharge passive.

---

## 1. Résumé validé

- **Rôle** : transformer sa vitesse en balle. Le rail récompense la précision, l'Élan récompense le mouvement.
- **Hitscan**, perforant comme le rail (même trajet que `fire_rail`), non parable.
- Au tir : `v_dep = min(vitesse horizontale, 15 m/s)`, dégâts `20 + 5 × v_dep` (× `streak_mult()`), et **le tireur perd `v_dep` de vitesse horizontale**.
- Recharge **0,5 s** (divisée par `streak_rate()`, comme le rail).
- **Munitions** (§ 1.5, gérées par `00_infra`) : **3 au spawn, 6 au maximum**, **jauge de 80 points par munition**, **aucune recharge passive**. La jauge se remplit par la mêlée (épée 50 par coup qui touche, +100 par kill en mêlée, +50 par parry) et par les cellules (100 ou 150 points).
- **Dash-tir** (décision du 27/10) : le combo n'est pas bridé, mais sa **fenêtre est courte**. Un tir qui part dans les **0,10 s** (12 ticks) après un dash convertit toute la vitesse. Ensuite, et jusqu'à 0,6 s après le dash, la vitesse ajoutée par le dash ne compte pas.
- Pièces : ricochet ×1,5 par `ricochet()` existant. La chaîne de touches s'applique (dégâts et cadence).
- PvP : × `PVP_SCALE` 0,7, plafond 60 appliqué par l'hôte. 0 m/s → 20 / 14 ; 9,5 m/s → 67,5 / 47,25 ; ≥ 15 m/s → 95 / **60**.
- **Contre-jeu** : un tir puissant arrête le tireur, qui devient la cible idéale d'une épée ou d'une lance.

## 2. Règles détaillées

### 2.1 Le coût en vitesse : réduire la **norme** horizontale

Deux modèles possibles ; le modèle A est retenu.

| | A. Réduire la norme horizontale (retenu) | B. Impulsion `−visée × v_dep` |
|---|---|---|
| Calcul | `h ← h × (|h| − v_dep) / |h|` | `vel ← vel − d × v_dep` |
| Tir devant soi | perd `v_dep` | perd `v_dep` |
| Tir sur le côté | perd `v_dep` | **gagne** de la vitesse : `√(15² + 15²) ≈ 21 m/s` |
| Tir vers l'arrière | perd `v_dep` | gagne `v_dep` (c'est la variante du § 13) |
| Tir vers le haut / bas | perd `v_dep` horizontal, `vel.y` inchangé | modifie `vel.y` : saut gratuit ou piqué |
| Lisibilité | « chaque tir coûte sa vitesse », toujours | dépend de l'angle, exploitable |

Le modèle B transforme l'arme en outil de mobilité (tirs latéraux qui accélèrent) et casse le contre-jeu. Avec A, le coût est **exactement** la vitesse convertie en dégâts, quel que soit l'angle. Comme `v_dep ≤ |h|`, le facteur est dans [0, 1] : la direction de course est conservée, seule la norme baisse (à 0 si l'on allait à 15 m/s ou moins).

### 2.2 Ordre dans le tick

`game_tick` appelle `player_tick` **avant** le tir. `v_dep` est donc la vitesse en fin de tick (après friction, collisions, dash, pad). Le freinage prend effet au tick suivant : aucune autre règle de `player_tick` n'est à ajouter. Un dash et un tir dans le **même tick** tombent dans la fenêtre (`kinDashT = 0`).

### 2.3 Cas par état de mouvement

| État | Effet du freinage | Commentaire |
|---|---|---|
| Au sol, course 9,5 m/s | `h → 0` | **on revient à 9,5 m/s en ~0,12 s** (`GROUND_ACCEL` : +0,95 m/s par tick, friction −0,15 à −0,45) : au sol, le coût jusqu'à 9,5 m/s est presque gratuit (voir § 11) |
| Bhop / air-strafe | `h → max(0, |h| − 15)` | `vel.y` intact : on finit le saut à la verticale ; `AIR_CAP` 1 m/s rend la relance lente : c'est le prix voulu |
| Slide | `h` peut passer sous `SLIDE_END` 3 m/s | le slide s'arrête au tick suivant ; le `SLIDE_BOOST` reste soumis à `SLIDE_CD` |
| Wall-run | `h` passe sous `WALLRUN_MIN` 6 m/s | le wall-run s'arrête au tick suivant (l'accélération murale n'ajoute que ~0,55 m/s par tick) ; on tombe du mur |
| Grappin accroché (décor) | `h` réduit, `vel.y` intact | la traction `GRAPPLE_PULL` 70 m/s² relance en ~0,2 s : le grappin est le meilleur « rechargeur » de vitesse. La corde n'est pas affectée |
| Grappin vers un joueur | idem | la vitesse relative de l'impact baisse d'autant : tir 60 puis impact plus faible (§ 7) |
| Jump pad (en vol) | la composante horizontale du lancer disparaît | on retombe court : choix du joueur |
| Chute ou piqué | rien (vitesse verticale non comptée) | la verticale reste le domaine de l'Enclume |

### 2.4 Cas limites

- **Hitstop** : `game_tick` sort avant le tir, aucun tir possible pendant `hitstop`. La fenêtre du dash avance dans `player_tick`, donc elle est aussi gelée pendant le hitstop : pas de fenêtre perdue à cause d'un hitstop.
- **Mort** : pas de tir (`!P.dead`). **Respawn** : 3 munitions et jauge à 0 (`00_infra`), `P.kinT = 0`, `P.kinDashT = 1` (aucune fenêtre ni malus).
- **Changement d'arme** : `switchT` 0,22 s, plus long que la fenêtre : un dash fait pendant le changement d'arme ne peut pas donner de dash-tir plein. C'est voulu (il faut tenir l'Élan).
- **Chargeur vide** : clic sec et bascule sur l'autre arme du loadout (`00_infra`). **Aucun freinage**, et la fenêtre du dash n'est pas consommée.
- **Fin de match** : le tir et le freinage ont lieu, l'hôte ignore les `EV_HIT` (`matchOver`).
- **Tir à 0 m/s** : autorisé (20 dégâts, coûte une munition, soit 80 points de jauge) ; aucun freinage.
- Le freinage s'applique **que le tir touche ou non** : c'est un coût, pas un effet de touche.

### 2.5 Fenêtre du dash-tir

`player_tick` mémorise, au moment du dash, la vitesse horizontale **d'avant** le dash (`P.kinPreDash`) et remet `P.kinDashT` à 0. La vitesse convertie au tir devient :

| Temps depuis le dash `kinDashT` | Vitesse utilisée | Nom |
|---|---|---|
| ≤ `KIN_DASH_WINDOW` (0,10 s, 12 ticks, bornes comprises) | `min(|h|, 15)` | dash-tir réussi |
| entre 0,10 et `KIN_DASH_FORGET` (0,6 s) | `min(|h|, kinPreDash, 15)` | trop tard : la vitesse du dash ne compte pas |
| ≥ 0,6 s | `min(|h|, 15)` | normal |

Règles complémentaires :
- **Freinage** : on retire la vitesse **convertie**. Un tir trop tard ne coûte donc que `kinPreDash` : on garde la vitesse du dash, mais on n'en tire rien. On peut alors attendre la fin du malus (0,6 s) pour un tir plein, ce qui est la voie lente.
- **Un tir consomme la vitesse d'avant le dash** : `kinPreDash -= v_dep` (plancher 0). Deux tirs trop tard ne comptent pas deux fois la même vitesse.
- **Deuxième dash pendant le malus** (`kinDashT < 0,6`) : `kinPreDash = min(kinPreDash, |h|)`, il ne peut pas monter (sinon dash + dash donnerait un tir plein hors fenêtre, la vitesse du 1er dash devenant « vitesse d'avant » le 2e) ; `kinDashT` repart à 0, donc une nouvelle fenêtre de 0,10 s s'ouvre.
- La vitesse gagnée **après** le dash pendant le malus (grappin, pad) est aussi ignorée par le `min`. C'est un effet de bord accepté pour 0,5 s (question 4).

## 3. Constantes

À coller à côté des `RAIL_*`. Les munitions (jauge 80, max 6, spawn 3) sont dans le tableau par `WP_*` de `00_infra`.

```c
#define KIN_CD            0.5f    /* recharge de l'Elan (divisee par streak_rate) */
#define KIN_DMG_BASE      20.0f   /* degats a l'arret */
#define KIN_DMG_PER_MS    5.0f    /* degats en plus par m/s de vitesse depensee */
#define KIN_SPEED_CAP     15.0f   /* vitesse horizontale maximale convertie par tir */
#define KIN_KNOCK         3.0f    /* recul de la cible a l'arret (PvE ; moitie en PvP) */
#define KIN_KNOCK_PER_MS  0.4f    /* recul en plus par m/s depensee : 3 -> 9 m/s */
#define KIN_FULL          14.5f   /* au-dela : tir "plein" (hitstop, son et rayon max) */
#define KIN_DASH_WINDOW   0.10f   /* dash-tir : le tir doit partir dans les 12 ticks apres le dash */
#define KIN_DASH_FORGET   0.6f    /* jusque-la, hors fenetre, la vitesse du dash ne compte pas */
#define KIN_BACK_BOOST    0       /* variante tir arriere (question ouverte), 0 = desactivee */
#define KIN_BACK_MAX      30.0f   /* plafond de vitesse de la variante */
```

## 4. Données

- `Player` : `float kinT;` (anime la décharge du viewmodel, 0,3 s), `float kinE;` (énergie du dernier tir, 0 à 1), `float kinDashT;` (temps depuis le dernier dash, 1 au spawn), `float kinPreDash;` (vitesse horizontale d'avant le dash, encore convertible).
- Munitions : `P.ammo[WP_KINETIC]` et `P.ammoGauge[WP_KINETIC]` (`00_infra`).
- Aucun champ dans `Proj`, `Coin` ni `NetPState` : l'arme tenue passe déjà par `NetPState.weapon`, et le freinage se voit dans `vel`. Aucun bit `ST_*`. La fenêtre du dash est purement locale.
- `EV_RAIL` : champs libres réutilisés, `sub = 1` (rayon d'Élan, 0 = rail) et `actor = énergie × 255`. `f[0..6]` gardent leur sens.
- `weaponNames[W_KINETIC] = "ELAN"`, `S_KIN` avant `S_COUNT`.

## 5. Algorithme

### 5.1 Factoriser le trajet du rail (refactor sans changement de comportement)

Extraire de `fire_rail` le cœur du hitscan pour ne pas dupliquer 40 lignes :

```c
/* trajet hitscan perforant : pieces, ennemis, joueurs LAN ; renvoie 1 si touche */
static int hitscan_shot(float dmg, float knockE, float knockP, int w, V3 *endOut, V3 *nOut) {
    V3 eye = eye_pos(), d = view_dir(), n;
    float tw = ray_world(eye, d, 400, &n);
    int hit = 0, coinHit = -1;
    /* ... boucle CO[] (arret sur la piece), boucle E[] : damage_enemy(i, dmg, vmul(d, knockE)) ... */
    /* ... boucle NP : net_hit(i, dmg * PVP_SCALE, vmul(d, knockP), w) : un seul net_hit par joueur ... */
    if (coinHit >= 0) hit |= ricochet(coinHit, dmg, 0);
    *endOut = vadd(eye, vmul(d, tw)); *nOut = n;
    return hit;
}
```

`fire_rail` devient `hit = hitscan_shot(RAIL_DMG * mult, 8, 4, W_RAIL, &end, &n);` suivi de son rendu inchangé. Les lignes « chaine » et « piece » de `run_tests` doivent donner **exactement** les mêmes valeurs avant et après.

### 5.2 `player_tick` : mémoriser le dash

```c
P.kinDashT += dt;                                  /* en tete, avec slideCd et wallJumpCd */
...
/* dash */
if (alive && dashPress && P.dashCharge >= 1) {
    float hv = hlen(P.vel);                        /* vitesse d'avant le dash */
    P.kinPreDash = P.kinDashT >= KIN_DASH_FORGET ? hv : fminf(P.kinPreDash, hv);   /* malus en cours : ne monte pas */
    P.kinDashT = 0;
    P.dashCharge -= 1;
    /* ... code du dash inchange ... */
}
```

### 5.3 Fonctions de l'Élan (section Combat, après `fire_rail`)

```c
static int kin_in_window(void) { return P.kinDashT <= KIN_DASH_WINDOW + TICK * 0.5f; }   /* 12 ticks */
static float kin_vdep(void) {
    float v = fminf(hlen(P.vel), KIN_SPEED_CAP);
    if (!kin_in_window() && P.kinDashT < KIN_DASH_FORGET) v = fminf(v, P.kinPreDash);   /* trop tard */
    return v;
}
static float kin_dmg(float v) { return KIN_DMG_BASE + KIN_DMG_PER_MS * v; }

/* retire v a la norme horizontale, direction conservee, vel.y intact */
static void kin_brake(float v) {
    float hs = hlen(P.vel);
    if (hs < 1e-4f) return;
    float k = fmaxf(0, hs - v) / hs;
    P.vel.x *= k; P.vel.z *= k;
}

static void fire_kinetic(void) {
    P.ammo[WP_KINETIC]--;
    int dashShot = kin_in_window() && P.kinDashT < KIN_DASH_FORGET;   /* pour le HUD et le journal */
    float v = kin_vdep(), e = v / KIN_SPEED_CAP, mult = streak_mult();
    float dmg = kin_dmg(v) * mult, kn = KIN_KNOCK + KIN_KNOCK_PER_MS * v;
    V3 h0 = v3(P.vel.x, 0, P.vel.z), end, n, m = muzzle();
    int hit = hitscan_shot(dmg, kn, kn * 0.5f, W_KINETIC, &end, &n);
    kin_brake(v);                                  /* le cout, touche ou non */
    P.kinPreDash = fmaxf(0, P.kinPreDash - v);     /* la vitesse d'avant le dash est depensee */
    shot_result(hit);
    if (botMode) netlog("tir elan : v=%.1f degats=%.0f %s%s", v, dmg, hit ? "touche" : "rate",
                        P.kinDashT < KIN_DASH_FORGET ? (dashShot ? " (dash-tir)" : " (dash trop tot)") : "");
    kin_beam_fx(m, end, n, e);                     /* rayon + spirale + etincelles (§ 8) */
    if (dmMode) net_fx_kinetic(m, end, e);
    for (int i = 0; i < (int)(6 + 18 * e); i++)    /* ton elan continue sans toi */
        add_part(m, vadd(vmul(h0, frange(0.5f, 0.9f)), vmul(rand_dir(), 2)), kin_col(e), 0.4f, 0.08f, 0);
    P.kinE = e; P.kinT = 0.3f;
    P.fireCd = KIN_CD / streak_rate();
    P.recoil = 0.6f + 0.8f * e; shake += 0.04f + 0.12f * e; P.fovKick -= 8 * e;
    muzzleT = 0.07f;
    play(S_KIN, 0.45f + 0.45f * e, 0.85f + 0.3f * e);
    if (hit) {
        hitMarker = 0.18f;
        play(S_HIT, 0.6f, 1.0f + 0.05f * (shotStreak < STREAK_MAX ? shotStreak : STREAK_MAX));
        if (v >= KIN_FULL) { hitstop = fmaxf(hitstop, 0.05f); impact_frame(end, 0.05f); }
    }
}
```

`kin_col(e)` : `vlerp(v3(0.45f, 0.45f, 0.55f), v3(1.0f, 0.3f, 0.9f), e)` (gris terne → magenta vif, distinct du bleu/or du rail et de l'orange des roquettes).

### 5.4 `game_tick`

Dans la branche de tir générique de `00_infra` (munitions > 0) : `case WP_KINETIC: fire_kinetic(); break;`. Rien d'autre : `P.fireCd`, `switchT`, hitstop, mort et jauge sont déjà gérés.

### 5.5 Aperçu pour le HUD (modèle `melee_preview`)

```c
/* degats du prochain tir d'Elan (PvP si LAN, plafonnes), *e = energie 0..1 */
static float kin_preview(float *e) {
    float v = kin_vdep(), d = kin_dmg(v) * streak_mult();
    *e = v / KIN_SPEED_CAP;
    return dmMode ? fminf(d * PVP_SCALE, PVP_HIT_CAP) : d;
}
```

`kin_vdep()` tient compte de la fenêtre : l'aperçu montre donc la chute des dégâts dès que la fenêtre est passée.

## 6. Réseau

- **Qui calcule quoi** : tout chez le tireur, comme le rail. `v_dep` et la fenêtre du dash viennent de l'état local (`P.vel`, `P.kinDashT`), simulé par le joueur lui-même (`player_tick`). La touche part en `net_hit(i, dmg × PVP_SCALE, recul, W_KINETIC)`, un seul par joueur et par tir (perforant : une boucle visite chaque joueur une fois). Le ricochet garde `W_COIN`.
- **Hôte** : rien de nouveau. `host_handle` plafonne à 60, compte `hitCount[W_KINETIC]`, `host_kill` affiche « ELAN ». `W_KINETIC` n'entre **pas** dans la liste des coups parables.
- **Faut-il valider la vitesse ?** Non, par cohérence : l'hôte ne valide déjà ni la visée du rail ni la vitesse relative de l'épée. Le gain d'une triche serait faible : au-delà de 86 PvE, tout vaut 60, et un tricheur pourrait de toute façon envoyer le rail à 60. Un contrôle fiable est impossible : le `NetPState` de l'hôte peut déjà montrer la vitesse **après** freinage (état à 60/s, événement sur un autre paquet), et la fenêtre de 12 ticks est plus courte qu'un aller-retour réseau. On n'ajoute rien.
- **Effet visuel** : nouvel émetteur, sans changer `net_fx_rail` ni ses appels existants :

```c
static void net_fx_kinetic(V3 a, V3 b, float e) {   /* declaration anticipee a cote de net_fx_rail */
    NetEvent ev = ev_new(EV_RAIL);
    vf(ev.f, a); vf(ev.f + 3, b); ev.f[6] = 0;
    ev.sub = 1; ev.actor = (unsigned char)(clampf(e, 0, 1) * 255);
    net_emit(&ev);
}
```

  Dans `apply_event`, `case EV_RAIL` : `if (ev->sub == 1) { kin_beam_fx(a, b, v3(0,1,0), ev->actor / 255.0f); break; }` avant le code du rail. Le son est atténué par `atten(a)`.
- **Ce que voient les autres** : l'arme tenue (`NetPState.weapon == WP_KINETIC`) et le freinage, visible via `vel` interpolé : le tireur s'arrête net, c'est le signal du contre-jeu. Aucun événement de plus.
- **Latence** : identique au rail (touche validée chez le tireur sur la position affichée `NP[i].disp`). La fenêtre du dash ne dépend que des ticks locaux : elle est la même en solo et en LAN.
- `NET_VER` : déjà passé à 4 par l'infra ; l'usage de `sub`/`actor` dans `EV_RAIL` fait partie de cet ensemble.

## 7. Interactions

- **Chaîne de touches** : `shot_result(hit)` ; un tir perforant qui touche 2 cibles compte +1. La chaîne change beaucoup l'arme : avec 1 touche (×1,15), 2 touches pour tuer dès 8,4 m/s ; à la chaîne maximale (×2,2), un tir à l'arrêt fait 44 PvE / 30,8 PvP.
- **Pièces** : `ricochet(coinHit, dmg, 0)` reçoit les dégâts déjà convertis. Le seuil de 60 PvP via sa propre pièce est atteint dès **7,4 m/s** (`85,71 / 1,5 = 57,1` PvE). Pièce adverse : renvoi sans bonus (contre-jeu existant). Le freinage s'applique comme pour un tir normal. Combo à étudier : lancer la pièce **pendant** le dash (elle hérite de la vitesse), puis tirer dedans dans la fenêtre.
- **Recharge par la jauge** (`00_infra`) : boucle visée « tirer → lent → mêlée → recharger ». Avec une jauge de 80 :
  - un coup d'épée qui touche (50) = 0,625 munition ; **2 coups = 1 munition + 20 points** ;
  - un parry (+50) suivi d'un coup qui touche (+50) = 1 munition + 20 ;
  - un kill à l'épée (2 coups + kill = 200 points) = **2 munitions + 40** ;
  - petite cellule (100) = 1 munition + 20 ; grande (150) = 1 munition + 70, pour **chacune** des deux armes du loadout.
  Il n'y a pas de recharge passive : un Élan vide oblige à aller au contact ou à une cellule. Le tireur arrêté se fait charger ; s'il **pare** (+50 points, +1 dash, grappin rechargé), il enchaîne un **dash-tir** à 95 dans la fenêtre. Le parry rend ainsi à la fois une partie de la munition et la vitesse que l'Élan consomme.
- **Dash** : 2 charges = 2 dash-tirs pleins en 0,5 s (120 PvP = kill), **à condition de réussir les deux fenêtres** (§ 2.5 et § 11).
- **Grappin** : traction vers une cible à 46 m/s → tir 60 → impact `50 + 3 × v_rel` (vitesse relative réduite de 15) : kill quasi instantané, comme rail + grappin aujourd'hui. Pendant le malus d'un dash, la vitesse du grappin est ignorée (§ 2.5).
- **Slide** : dégâts pleins en sortie de slide ; le tir arrête le slide.
- **Égide** (fiche 05) : si le parry parfait du bouclier renvoie le rail, il doit traiter `W_KINETIC` pareil (trajet `hitscan_shot` commun). À trancher dans la fiche 05.
- **Parry** : hitscan, non parable par l'épée.

## 8. Rendu et son

- **Rayon** `kin_beam_fx(a, b, n, e)` (local et `apply_event`) :
  - `add_beam(a, b, kin_col(e), 0.25f + 0.55f * e)` ; si `e ≥ KIN_FULL / 15`, un second `add_beam` blanc de 0,15 s (la largeur fixe de 7 px de `render` paraît alors doublée, sans toucher à `Beam`).
  - Spirale façon Quake (code de `fire_rail`) : rayon `0.06 + 0.14 × e`, un point tous les `0.6 − 0.3 × e` m ; aucun à `e < 0,15` (tir mou = trait gris seul).
  - Étincelles à l'impact : `6 + 20 × e`, vitesse `3 + 5 × e`, couleur `kin_col(e)`.
- **Viewmodel** (`draw_viewmodel`, branche `WP_KINETIC`) : corps court et trapu (`box_faces` -0,06…0,06), canon -0,55, et trois anneaux (volant d'inertie) autour du canon dont la couleur suit **en direct** `kin_vdep() / 15` : un compteur de vitesse dans la main. **Pendant la fenêtre du dash, les anneaux passent au blanc**. Ils s'éteignent pendant `P.kinT` (décharge), avec un recul vers l'arrière `P.kinT × 0,1`. Flash du canon de couleur `kin_col(P.kinE)`, `tip = -0.57f`. `ready` (fireCd) atténue les anneaux comme les autres armes.
- **Joueurs distants** (`draw_remote_players`) : couleur d'arme `glColor3f(0.95f, 0.3f, 0.85f)` pour `WP_KINETIC`.
- **HUD** (`draw_hud`) :
  - bloc « arme » : `ELAN %d/6` avec la jauge de 80 points sous le compteur (`00_infra`) ; au-dessus, gros chiffre `kin_preview()` (« 60 dmg » en LAN, « 95 dmg » en solo), couleur `kin_col(e)` ; barre 200 px remplie à `e`, avec un repère au seuil « 2 touches » (50 PvP, 10,3 m/s sans chaîne) en LAN ; mot `PLEIN` quand `e ≥ 1`. `cdMax = KIN_CD`.
  - **signal de la fenêtre du dash** : tant que `kin_in_window()` (et l'Élan en main), un anneau magenta autour du viseur se resserre de 28 à 10 px en 12 ticks (`r = 10 + 18 × (1 − kinDashT / KIN_DASH_WINDOW)`), et le chiffre d'aperçu clignote en blanc. Pendant le malus (0,10 à 0,6 s), le chiffre passe au gris et une fine barre grise sous le viseur se vide jusqu'à 0,6 s : on voit quand le tir plein revient.
  - le panneau « CHAINE DE TIR » reste inchangé.
- **Juice** : `fovKick` négatif (−8 × e, sensation de freinage), hitstop 0,05 s et `impact_frame` sur un tir plein qui touche, chiffre de dégât rouge (≥ 60) existant. Un dash-tir réussi qui touche affiche `popText` « DASH-TIR ».
- **Son** `S_KIN` (`make_sounds`, 0,45 s) : « whump » = sinus balayé de 160 à 45 Hz (`ph += (45 + 115 * expf(-t * 12)) / SR`) × `expf(-t * 7)`, plus bruit filtré passe-bas (`lp += 0.1f * (noise() - lp)`) × `expf(-t * 18)`, plus un tintement métallique de 1 800 Hz × `expf(-t * 30)` à 0,15. Volume et hauteur modulés par `e` à l'appel (`play(S_KIN, 0.45 + 0.45e, 0.85 + 0.3e)`).

## 9. Bots

Dans `bot_think`, quand l'arme choisie est `WP_KINETIC` (le choix d'arme lui-même suit la logique de loadout de `00_infra`). Le bot réussit la fenêtre **une fois sur deux** : sinon il tire 0,15 à 0,3 s après le dash, comme un humain en retard.

```c
static float kinLateT = -1;                         /* tir en retard programme (fenetre ratee) */
if (P.weapon == WP_KINETIC && see && seeT > 0.3f && P.switchT <= 0 && bd < 55) {
    float v = kin_vdep(), need = bd > 20 ? 14.0f : 10.5f;   /* de loin : tir plein ; de pres : 2 touches */
    if (kinLateT >= 0 && botT >= kinLateT && P.fireCd <= 0) {
        P.yaw = aimYaw + aimErrYaw; P.pitch = aimPitch + aimErrPitch; botHeld[A_FIRE] = 1; kinLateT = -1;
    } else if (P.fireCd <= 0 && v >= need) {
        P.yaw = aimYaw + aimErrYaw; P.pitch = aimPitch + aimErrPitch; botHeld[A_FIRE] = 1;
    } else if (P.fireCd <= 0 && P.dashCharge >= 1 && botT > botNextDash && kinLateT < 0) {
        P.yaw = aimYaw; botPress[A_DASH] = 1; botNextDash = botT + 1.2f;
        if (frand() < 0.5f) botHeld[A_FIRE] = 1;    /* dash-tir dans le meme tick : fenetre reussie */
        else kinLateT = botT + frange(0.15f, 0.3f); /* fenetre ratee : tir en retard */
    }                                               /* sinon : bhop / grappin pour reprendre de la vitesse */
}
```

- Après un tir, si `hs < 5` et la cible à moins de 8 m, la règle d'épée existante s'applique (la mêlée remplit la jauge).
- Le blocage « coincé » (`hs < 3` pendant 0,4 s) ne doit pas se déclencher juste après un tir : ignorer `stuckT` tant que `P.kinT > 0`.
- **À sec** : sans recharge passive, le bot qui n'a plus de munitions sur ses deux armes va au contact (règle de `00_infra`) ; la recherche de cellule remplace `P.railAmmo < 2` par le test générique de `00_infra` sur l'arme en main.

## 10. Tests (`run_tests`, couloir x = −20)

Tests 1 à 9 : `fire_kinetic()` appelé directement (pas de friction), `shotStreak = 0` avant chaque tir, `P.kinDashT = 1`, drone `E[0]` à `(-20, 1,6, 30)` avec 1 000 PV, joueur en `(-20, 0, 40)`, `yaw = 0`, `P.ammo[WP_KINETIC] = 6`.

| # | Scénario | Mesure | Attendu |
|---|---|---|---|
| 1 | `P.vel = (0, 0, −v)` pour v = 0 ; 9,5 ; 15 ; 25 | dégâts, `hlen(P.vel)` après | 20 / 67,5 / 95 / 95 ; 0 / 0 / 0 / 10 |
| 2 | `P.vel = (12, 6, 5)` (sur le côté, en montée) | dégâts, `hlen` après, `vel.y` | 85 ; 0 ; 6 (vertical intact) |
| 3 | `pitch = 1,2` (vise en l'air, rate), `P.vel = (0, 0, −15)` | `hlen` après, `shotStreak` | 0 ; 0 (freinage même raté) |
| 4 | chaîne : 3 tirs à 15 m/s (vitesse remise avant chaque tir) | dégâts | 95 / 109,25 / 123,5 |
| 5 | pièce (copie du test « piece », `P.vel = (0, 0, −20)` au tir) | dégâts drone, `hlen` après | 142,5 ; 5 |
| 6 | slide : au sol, `act[A_CROUCH] = 1`, `P.vel = (0, 0, −12)`, 1 `player_tick`, tir, 1 `player_tick` | dégâts, `P.sliding` après | 95 ; 0 |
| 7 | munitions : `P.ammo[WP_KINETIC] = 0`, tir via `game_tick` | munitions, arme | 0 ; bascule (`00_infra`) |
| 8 | LAN simulé (après le « plafond PvP »), `NP[1]` à 10 m devant, `P.vel` = 0 / 9,5 / 15, PV remis à 100 | `NP[1].hp` | 86 / 52,75 / 40 |
| 9 | LAN simulé, `botHeld[A_FIRE]` tenu 0,99 s vers un mur, 6 munitions | munitions restantes, jauge | 4 ; 0 (aucune recharge passive) |

Tests 10 à 14, **fenêtre du dash** : on mesure `kin_vdep()` (pas de géométrie à toucher). Joueur en l'air en `(-20, 8, 40)` (chute de ~0,9 s, pas de friction), `memset(act, 0, sizeof act)`, `P.dashCharge = 2`, `P.kinDashT = 1`. Le dash est `player_tick(TICK, 0, 1)`, les ticks suivants `player_tick(TICK, 0, 0)`.

| # | Scénario | Mesure | Attendu |
|---|---|---|---|
| 10 | à l'arrêt, dash, mesure dans le même tick | `kin_vdep()` | 15 (dash-tir) |
| 11 | à l'arrêt, dash, puis 12 ticks / 13 ticks | `kin_vdep()` | 15 / **0** (fenêtre de 12 ticks comprise) |
| 12 | `P.vel = (0, 0, −9,5)`, dash, puis 24 ticks | `kin_vdep()`, `hlen` | 9,5 ; 18 |
| 13 | à l'arrêt, dash, puis 73 ticks (0,608 s) | `kin_vdep()` | 15 (malus terminé) |
| 14 | à l'arrêt, dash, 30 ticks, deuxième dash, puis 24 ticks | `kin_vdep()` | 0 (`kinPreDash` gardé) |
| 15 | jauge (si `00_infra` n'a pas ce test) : Élan en main, `P.ammo = 3`, jauge 0, deux coups d'épée qui touchent (50 + 50) | munitions, jauge | 4 ; 20 |

Format : `fprintf(f, "elan a %.1f m/s : degats %.1f, vitesse apres %.1f (attendu ...)\n", ...)` et `fprintf(f, "dash-tir : vdep %.1f apres %d ticks (attendu ...)\n", ...)`.

## 11. Équilibrage

Dégâts = `(20 + 5·v) × chaîne` ; PvP = × 0,7, plafond 60.

| v_dep (m/s) | PvE | PvP | Touches (100 PV) | PvP avec chaîne ×1,15 | PvP via sa pièce |
|---|---|---|---|---|---|
| 0 | 20 | 14 | 8 | 16,1 | 21 |
| 5 | 45 | 31,5 | 4 | 36,2 | 47,25 |
| 9,5 (course) | 67,5 | 47,25 | 3 | 54,3 (2) | 60 |
| 10,3 | 71,4 | 50 | **2** | 57,5 | 60 |
| 13,1 | 85,7 | 60 | 2 | 60 | 60 |
| ≥ 15 | 95 | 60 | 2 | 60 | 60 |

- Seuils : 2 touches à 10,3 m/s sans chaîne (8,4 avec 1 touche, 7,0 avec 2) ; plafond 60 dès 13,1 m/s. La plage utile est donc 10 à 15 m/s : un bhop correct suffit.

**Fenêtre du dash-tir : temps pour tuer** (cible à 100 PV, PvP, chaîne ×1,15 après la 1re touche, 2e tir au plus tôt à 0,5 s)

| Séquence | Tir 1 | Tir 2 | Total | Kill | Temps |
|---|---|---|---|---|---|
| Double dash-tir, deux fenêtres réussies | 60 | 60 | 120 | oui | **0,5 s** |
| En l'air, 2e fenêtre ratée (les 3 m/s restants viennent du 1er dash : `kinPreDash` = 0) | 60 | `20 × 1,15 × 0,7` = 16,1 | 76,1 | non | 3e tir plein à la fin du malus du 2e dash : **~1,0 s** |
| En l'air depuis l'arrêt, 1re fenêtre ratée (tir à 0,2 s) | 14 | 60 (sans 2e dash : malus fini à 0,6 s, vitesse du dash gardée) | 74 | non | 3e tir à 3 m/s, chaîne ×1,3 : 31,9 → kill à **~1,15 s** |
| Dash-tir réussi, puis tir au sol en course (9,5 m/s, sans dash) | 60 | 54,3 | 114,3 | oui | 0,5 s |
| **Au sol, deux tirs en course, sans dash** | 47,25 | 54,3 | 101,6 | **oui** | **0,5 s** |
| Rail (référence) | 60 | 60 | 120 | oui | 0,81 s |

- Réussir la fenêtre **en l'air** fait passer le temps pour tuer de ~1,0 s à 0,5 s : le timing est récompensé sans que le combo soit bridé.
- **Constat important** : au sol, on revient à 9,5 m/s en ~0,12 s après un tir (§ 2.3). Deux tirs en course suffisent donc à tuer en 0,5 s (47,25 + 54,3 grâce à la chaîne), sans dash ni fenêtre. Le coût en vitesse ne mord vraiment qu'au-dessus de 9,5 m/s et en l'air. Voir la question 5.
- **Munitions** : 3 au spawn = un kill (2 touches) et un raté. Remplir l'arme (3 → 6 = 240 points) demande environ 5 coups d'épée, ou un kill à l'épée et un coup de plus, ou deux grandes cellules. Comparaison : rail 2 munitions max et 250 points par munition ; l'Élan tire 3 fois plus souvent pour 80 points, mais coûte de la vitesse.
- **À mesurer** (match de bots avant / après, `--log`) : `hitCount[W_KINETIC]` ; `v_dep` moyen par tir et part de tirs ≥ 14,5 ; part des dash-tirs réussis (lignes « (dash-tir) » contre « (dash trop tot) », ~50 % attendu pour le bot) ; `ttkSum/ttkN` ; `oneShots` (doit rester 0 pour l'Élan) ; morts du tireur dans les 1,5 s suivant un tir plein (le contre-jeu fonctionne-t-il ?) ; temps passé à sec sur l'Élan ; vitesse moyenne du bot (`botSpeedSum`) comparée au rail.

## 12. Étapes d'implémentation

| # | Étape | Fonctions | Estimation |
|---|---|---|---|
| 0 | Prérequis `00_infra` : `WP_*`, `W_COUNT`, `P.ammo[]`, `P.ammoGauge[]`, gain de points, bascule à vide | — | (fiche 00) |
| 1 | Extraire `hitscan_shot` de `fire_rail` ; vérifier que les tests « chaine » et « piece » sont identiques | `fire_rail` | 30 min |
| 2 | Constantes, `kin_vdep`, `kin_dmg`, `kin_brake`, `fire_kinetic`, branche `WP_KINETIC` | Combat, `game_tick` | 1 h |
| 3 | Fenêtre du dash : `kinDashT`, `kinPreDash` dans `player_tick`, `kin_in_window` | `player_tick`, `reset_game`, respawn | 30 min |
| 4 | Tests 1 à 7 et 10 à 15 | `run_tests` | 1 h |
| 5 | Réseau : `net_fx_kinetic`, `EV_RAIL` sub 1, `"ELAN"`, tests 8 et 9 | `apply_event`, `weaponNames` | 30 min |
| 6 | Rendu : `kin_beam_fx`, `kin_col`, viewmodel, couleur distante, HUD `kin_preview` et signal de fenêtre | `draw_viewmodel`, `draw_remote_players`, `draw_hud` | 1 h 45 |
| 7 | Son `S_KIN` | `make_sounds` | 20 min |
| 8 | Bot (seuil de vitesse, dash-tir une fois sur deux) puis match de bots et mesures | `bot_think` | 1 h 30 |

Total : environ 7 h hors infra (coût **S**). Après chaque étape : `build.bat` sans avertissement, `--test`, `--shot rail.bmp`.

## 13. Questions ouvertes

1. **Variante « tirer vers l'arrière pousse vers l'avant »** (plafond 30 m/s). Définition : si `dot(visée horizontale, h / |h|) < −0,5`, ajouter `v_dep` dans le sens de `h` au lieu de freiner, sans dépasser `KIN_BACK_MAX`. Risques :
   - elle **supprime le contre-jeu** dans la fuite : le poursuivi inflige 60 et accélère ;
   - **boucle auto-entretenue** : à ≥ 15 m/s, chaque tir arrière donne +15, donc 30 m/s dès le premier tir, puis jusqu'à 6 tirs à 60 sans jamais ralentir ;
   - **mobilité gratuite** : un tir arrière vers le sol vaut un rocket jump horizontal sans perte de PV ; un dash-tir arrière dans la fenêtre donne 30 m/s immédiatement ;
   - seuil d'angle flou (tir latéral à 120°), et les bots l'exploiteraient parfaitement.
   Atténuations possibles : gain = `v_dep × 0,5`, seulement en l'air, dégâts ×0,5 pour un tir arrière, ou 2 munitions par tir. **Recommandation** : hors v1, derrière `KIN_BACK_BOOST` (0), à tester en match de bots. On la garde ?
2. **Vitesse comptée** : horizontale seulement (retenu) ou 3D ? En 3D, un piqué donnerait des tirs pleins, ce qui empiète sur l'Enclume.
3. **Wall-run** : un tir plein fait tomber du mur. Faut-il épargner le wall-run (garder `WALLRUN_MIN`) pour récompenser ce style ?
4. **Fenêtre du dash** : 0,10 s (12 ticks) et un malus de 0,6 s conviennent-ils ? Hors fenêtre, on prend `min(vitesse, vitesse d'avant le dash)`, ce qui ignore aussi la vitesse gagnée au grappin pendant le malus. L'autre choix est de **soustraire le gain du dash** (`vitesse − (vitesse après dash − vitesse avant)`), plus juste avec le grappin mais moins lisible.
5. ~~**Course au sol trop rentable ?**~~ **Tranché le 27/10 : pas de changement**, l'utilisateur garde ces valeurs. Constat d'origine : Deux tirs à 9,5 m/s tuent en 0,5 s (chaîne ×1,15), et la vitesse au sol revient en 0,12 s. Pistes : ne pas appliquer `streak_mult` aux dégâts de l'Élan (cadence seulement), un malus de friction de 0,3 s après un tir au sol, ou `KIN_DMG_BASE` 15.
6. **Ricochet** : la touche via pièce reste `W_COIN` (fil des kills « PIECE ») ; faut-il passer l'arme d'origine à `ricochet()` pour créditer `W_KINETIC` dans les stats ?
7. **Recul de la cible** : transmettre la vitesse à la cible (3 à 9 m/s PvE, moitié en PvP) est-il souhaité, ou garde-t-on le recul fixe du rail ?
8. **Tir à l'arrêt** : 20 dégâts pour une munition (80 points de jauge) est-il acceptable, ou faut-il bloquer le tir sous 2 m/s (clic sec « TROP LENT ») pour ne pas gaspiller ?
