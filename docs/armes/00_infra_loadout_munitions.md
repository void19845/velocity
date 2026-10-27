# 00 — Infrastructure : loadout, munitions à jauge, cellules, recharge par la mêlée

> Fiche de design, **rien n'est implémenté**. Interfaces figées : [`PLAN.md`](PLAN.md) § 3 et § 4. Source : [`../IDEES_ARMES_CARTES.md`](../IDEES_ARMES_CARTES.md) § 1.4, § 1.5 (règle des jauges, décisions du 27/10), § 4.7.
> Coût visé : **S** (en pratique S+, à cause de l'écran de loadout). Toutes les autres fiches en dépendent.
> Cette version **remplace** l'ancienne règle « +1 munition par coup » (`ammo_refund`) et **toute recharge passive** (`regen`, `REGEN_DELAY`, `ammo_regen`, `P.ammoRegenT/Wait` sont supprimés).

---

## 1. Résumé validé

- **Loadout** : 1 arme de mêlée (`P.meleeKind`, `MK_*`) + 2 armes à distance (`P.loadout[2]`, deux `WP_*` **distincts**). `P.weapon` devient l'**identifiant** `WP_*` de l'arme en main. `WP_RAIL = 0` et `WP_ROCKET = 1` gardent les valeurs actuelles ; le loadout par défaut est `{WP_RAIL, WP_ROCKET}` + `MK_SWORD`.
- **Choix** : menu principal, menu pause, écran de mort (raccourcis clavier). Il prend effet **au respawn** (LAN : `EV_RESPAWN` ; solo : `reset_game`). Il est mémorisé dans `velocity.ini`.
- **Touches** : `A_WEAP1` = emplacement 1, `A_WEAP2` = emplacement 2, `A_SWAP` alterne. Aucune action nouvelle.
- **Munitions** : `P.ammo[WP_COUNT]` remplace `P.railAmmo`. **Aucune recharge passive.** On recharge en frappant au corps à corps et en ramassant des cellules.
- **Jauge** : chaque arme à distance a une jauge de points (`P.ammoGauge[w]`). Quand la jauge est pleine, l'arme gagne **+1 munition** et la jauge repart de 0 (le surplus est gardé). Au maximum de munitions, la jauge reste affichée pleine et n'avance plus.

| Arme | `WP_*` | Points par munition | Max | Spawn |
|---|---|---|---|---|
| Rail | `WP_RAIL` | 250 | 2 | 2 |
| Roquettes | `WP_ROCKET` | 50 | 8 | 4 |
| Éventail | `WP_SHOTGUN` | 100 | 6 | 3 |
| Élan | `WP_KINETIC` | 80 | 6 | 3 |
| Scie | `WP_SAW` | — (aucune jauge) | 1 | 1 |
| Ventouse | `WP_MINE` | 150 | 3 | 1 |
| Tesla | `WP_TESLA` | la batterie est la jauge : 1 point = 0,4 de batterie | 100 | 60 |

| Source de points (par coup qui touche) | Points |
|---|---|
| Épée | 50 |
| Enclume | 75 par onde ou coup qui touche |
| Lance | 100 par joute, 50 par estoc |
| Fléau | 35 |
| Poings | 25 par coup, **50 au plus par combo** (validé) |
| Égide | 50 par coup de bouclier, 100 par parry parfait |
| Toutes | **+100** par kill en mêlée, **+50** par parry réussi |
| Cellule petite / grande | 100 / 150, à **chacune** des deux armes du loadout |

- **Destination des points de mêlée** : l'arme à distance en main ; si elle est pleine (ou si c'est la Scie), le surplus va à l'autre arme du loadout.
- **Chargeur vide** : clic sec ; bascule automatique sur l'autre arme si elle peut tirer ; sinon `popText` « À SEC : VA AU CONTACT ».
- **Rocket jump** : il coûte une roquette (accepté le 27/10).
- **Réseau** : `NET_VER` 3 → 4, une seule fois pour toutes les armes. Aucun paquet ne change de taille.

## 2. Règles détaillées

### 2.1 Cycle de vie du loadout

```
wantLoadout[2], wantMelee  (globaux, velocity.ini)  --respawn-->  P.loadout[2], P.meleeKind, P.weapon, P.ammo[], P.ammoGauge[]
```

- `reset_game` fait `memset(&P, 0, ...)`. Le choix vit donc dans des globaux `wantLoadout[2]` et `wantMelee`, recopiés par `loadout_apply()` à chaque (re)spawn : dans `reset_game` (solo, `dm_local_start`, `menu_backdrop`) et dans `apply_event(EV_RESPAWN)` quand `t == myId`.
- `loadout_apply()` **valide** le choix (bornes, `wpReady`, deux armes distinctes), sinon elle revient au défaut. Elle met les munitions de spawn à **toutes** les armes et les jauges à 0, et met l'arme 1 en main.
- Entre deux respawns, modifier `want*` ne change rien en jeu ; le HUD affiche « nouveau loadout au prochain respawn ».
- **Deux emplacements identiques interdits** : choisir pour un emplacement l'arme de l'autre **échange** les deux.
- **Armes pas encore codées** : les tables `wpReady[WP_COUNT]` et `mkReady[MK_COUNT]` (au départ : rail, roquettes, épée) grisent les boutons. Chaque fiche passe sa case à 1. L'infra peut donc sortir seule.

### 2.2 Sélection et tir (`game_tick`)

- `A_WEAP1` → `P.loadout[0]`, `A_WEAP2` → `P.loadout[1]`, `A_SWAP` → l'autre emplacement. Changer d'arme coûte `SWITCH_TIME` (0,22 s, la valeur actuelle codée en dur).
- **Mort** : les mêmes touches modifient le choix (§ 2.6) au lieu de changer d'arme.
- **Tir** : si `wp_has_ammo(P.weapon)`, alors `fire_weapon()` (aiguillage par `WP_*`). Chaque `fire_*` décrémente **lui-même** `P.ammo[...]`, sans descendre sous 0, pour que `run_tests` puisse appeler `fire_rail()` / `fire_rocket()` directement. Un tir **ne touche pas** à la jauge.
- **Chargeur vide** (`dry_fire`) : son `S_HIT` grave (0,5 ; 0,45), `P.fireCd = DRYFIRE_CD` (0,35 s, sinon un clic par tick en tir maintenu). Ensuite :
  - si l'autre arme peut tirer : `weapon_select(autre)`, `popText` « RAILGUN A SEC » ;
  - sinon : `popText` « A SEC : VA AU CONTACT ! », et `dryHintT = 2` (le panneau de mêlée clignote).
- Cas particuliers laissés aux fiches :
  - **Scie** : `P.ammo[WP_SAW]` = 1 quand la scie est en main, 0 sinon (tenu par `08_scie`) ;
  - **Tesla** : `P.ammo[WP_TESLA]` = batterie 0 à 100, la consommation est dans `10_tesla` ;
  - **Ventouse** : la détonation marche à 0 mine ; `09_ventouse` la traite **avant** le test de munitions.

### 2.3 Jauge : conversion des points

- `gauge_add(w, pts)` ajoute des points à la jauge de `w` et convertit chaque jauge pleine en +1 munition, en gardant le surplus. Elle renvoie les points **inutilisés** : tous si l'arme n'a pas de jauge (Scie) ou est déjà au maximum, sinon ce qui dépasse quand l'arme atteint son maximum.
- **Au maximum**, la jauge stockée retombe à 0, et le HUD l'affiche **pleine** (`gauge_frac` = 1). Quand on tire depuis le maximum, la jauge repart donc de 0 : la jauge pleine n'est pas une munition en réserve. Sans cette règle, on aurait max + 1 tirs, ou une munition gratuite à chaque tir (question 2).
- **Tesla** : `ammoDef[WP_TESLA].gauge = 2.5` points par unité de batterie. 100 points donnent donc 40 de batterie, et le reste fractionnaire s'accumule dans `ammoGauge`. Le même code sert pour toutes les armes.
- La jauge est **par arme** et **locale** : elle n'est jamais transmise. Elle est remise à 0 au respawn.

### 2.4 Points de mêlée (`ammo_points`)

- **Destination** : `ammo_points(pts, src)` donne les points à l'arme en main ; ce qu'elle ne peut pas prendre va à l'autre arme ; le reste est perdu (statistique `AP_LOST`). Les deux étapes passent par `gauge_add`.
- **Une fois par coup** : `melee_hit_points(pts)` ne donne qu'une fois par coup, quel que soit le nombre de cibles touchées (drapeau `P.swingHitPts`). De même, `PTS_PARRY` n'est donné qu'une fois par coup, même si plusieurs projectiles sont renvoyés (`P.swingParryPts`). Un même coup peut toucher **et** parer : 50 + 50.
- `melee_start_swing()` remet ces deux drapeaux à 0. L'épée l'appelle en tête de `melee()`. Chaque fiche de mêlée l'appelle au début de ce qui compte comme « un coup » chez elle : une onde de l'Enclume, une joute ou un estoc, un coup du combo des Poings…
- **Kill en mêlée** : +100 par kill, sans limite par coup, **en plus** des points de la touche. Exemple : tuer à l'épée en 2 coups rapporte 50 + 50 + 100 = 200.
- **Poings** (plafond par combo, validé) : `04_impulsion` tient `P.fistComboPts`, remis à 0 au premier coup du combo, et appelle `melee_hit_points(min(PTS_FIST, PTS_FIST_COMBO_MAX - P.fistComboPts))`.
- **Égide** : coup de bouclier = 50 (valeur par défaut de `meleePts`). Un parry parfait donne 100 **à la place** des 50 d'un parry ordinaire (question 6).
- **Mort** : `ammo_points` ne fait rien si `P.dead` (un tueur mort dans un échange ne gagne rien).
- **Retour visuel** : la jauge rechargée clignote, avec « +50 » en vert pendant 0,8 s (`ammoGainT`, `ammoGainSlot`, `ammoGainPts`), et un `play(S_PAD, 0.35f, 2.4f)`. Le gain d'une munition est plus fort : `play(S_PAD, 0.5f, 3.0f)` et le compteur grossit 0,15 s.

**Qui donne quoi, et où**

| Événement | Solo | LAN |
|---|---|---|
| Coup qui touche | `melee()` : `if (hits) melee_hit_points(meleePts[P.meleeKind])` | idem (la touche est validée chez l'attaquant) |
| Kill | `melee()` : `if (!e->alive) ammo_points(PTS_MELEE_KILL, AP_KILL)` | `apply_event(EV_KILL)` : `k == myId && v != myId && is_melee_w(sub)` |
| Parry de projectile | `try_parry` : `melee_parry_points(PTS_PARRY)` | idem |
| Parry d'un coup de mêlée | — | `apply_event(EV_MELEE_PARRY)`, défenseur : `ammo_points(PTS_PARRY, AP_PARRY)` directement (décision de l'hôte, qui arrive souvent après la fin du coup) |
| Kamikaze tué à l'épée | parry 50 + touche 50, **pas** de bonus de kill (question 3) | — |

- `is_melee_w(w)` : `W_MELEE`, `W_SLAM`, `W_LANCE`, `W_FLAIL`, `W_FIST`, `W_SHIELD`. Ni `W_HOOK` ni `W_PARRY` (question 4).
- **Coup paré par l'hôte (LAN)** : l'attaquant a déjà reçu ses points. Si l'hôte déclare ensuite le coup paré (`EV_MELEE_PARRY` avec `target == myId` et `is_melee_w((int)ev->f[0])`, cf. PLAN § 4.2), il **rend** les points de cette touche. `P.undoPts[2]` mémorise ce que chaque emplacement a reçu, et `P.undoT` la date. L'annulation passe par `gauge_remove`, qui peut reprendre une munition déjà convertie. Elle n'est faite que si la touche date de moins de `REFUND_UNDO_T` (0,5 s). Les points de kill ne sont jamais annulés.

### 2.5 Cellules (deux tailles)

- `Pickup` reçoit un champ `pts` (`PICKUP_SMALL` 100 ou `PICKUP_BIG` 150) ; `addpickup(x, y, z, pts)`.
- **Ramassage** : `pickup_tick` refuse la cellule si les deux armes sont au maximum (`loadout_full()`, la Scie compte comme pleine). Sinon la cellule est prise, même si une seule arme en profite.
- `pickup_taken(i, who)`, `who == myId` : `gauge_add(P.loadout[s], PU[i].pts)` pour **chacun** des deux emplacements (pas de report d'une arme à l'autre) ; `popText` « CELLULE +150 ».
- **Réseau** : l'arbitrage ne change pas. Le client demande (`EV_PICKUP`), l'hôte tranche (`host_take_pickup` → `EV_PICKUP_TAKEN`), et **le ramasseur applique les points chez lui** avec son propre loadout. La taille est déterministe (construite par `build_level`), elle ne voyage pas. `MAXPICK` 16 et `pickMask` 16 bits ne changent pas.

**Répartition proposée sur l'arène actuelle** (l'ordre des `addpickup` est conservé, car il fixe les index réseau) :

| Index | Position | Exposition | Taille |
|---|---|---|---|
| 0 | sommet de la tour (0, 13, 0) | centre, visible de partout | **grande 150** |
| 1 à 4 | plateformes hautes (±36, 7, 0), (0, 7, ±36) | moyenne | petite 100 |
| 5, 7, 9, 11 | perchoirs (±36, 13, ±36) | hauts, à découvert | **grande 150** |
| 6, 8, 10, 12 | coins au sol (±46, 0, ±46) | abrités | petite 100 |

Cela fait 5 grandes et 8 petites, soit 1 550 points par arme pour un tour complet de la carte. Rendu : les grandes cellules sont 1,4 fois plus grosses, avec des arêtes violettes (0,75 ; 0,45 ; 1) au lieu de bleues et un halo de 1,9 au lieu de 1,3.

### 2.6 Écran de mort et menus

- **Écran de mort** (solo et LAN) : `A_WEAP1` fait défiler l'emplacement 1, `A_WEAP2` l'emplacement 2, `A_MELEE` la mêlée (seulement les armes prêtes). Trois lignes `[1] < RAILGUN >   [2] < ROQUETTES >   [clic droit] < EPEE >`. La souris reste capturée, donc pas de bouton. `cfg_save` à chaque changement.
- **Écran complet** `draw_loadout` : bouton LOADOUT dans `draw_main_menu` (entre REJOINDRE et PARAMETRES) et dans `draw_pause` (sous REPRENDRE). Il s'ouvre comme PARAMETRES (`loadoutOpen`) et se ferme par VALIDER ou Échap (+ `cfg_save`).

### 2.7 Cas limites

| Cas | Comportement |
|---|---|
| Mort | `P.ammo` et `P.ammoGauge` gelés ; aucun point ; les touches d'arme modifient le choix. |
| Respawn / début de match | `loadout_apply()` : munitions de spawn, jauges à 0, arme 1 en main (`host_start_match` refait apparaître tout le monde). |
| Changement d'arme pendant un gain | Les points vont à l'arme en main **au moment** du gain (kill reçu par `EV_KILL` après un changement d'arme compris). |
| Hitstop | `game_tick` sort tôt ; les points venus d'`apply_event` s'appliquent quand même. |
| Cellule ramassée par un client qui tire entre-temps | Sans effet : la recharge est appliquée à la réception de `EV_PICKUP_TAKEN`. |
| Valeur réseau inconnue | `st.weapon >= WP_COUNT` → dessiné comme rail ; `(pad & 15) >= MK_COUNT` → épée. |

## 3. Constantes

```c
/* --- loadout et munitions a jauge (infra) --- */
enum { MK_SWORD, MK_ANVIL, MK_LANCE, MK_FLAIL, MK_FIST, MK_SHIELD, MK_COUNT };
enum { WP_RAIL, WP_ROCKET, WP_SHOTGUN, WP_KINETIC, WP_SAW, WP_MINE, WP_TESLA, WP_COUNT };
enum { W_RAIL, W_ROCKET, W_MELEE, W_HOOK, W_PARRY, W_SELF, W_COIN, W_SLAM, W_LANCE, W_FLAIL, W_FIST,
       W_SHIELD, W_SHOTGUN, W_KINETIC, W_SAW, W_MINE, W_TESLA, W_COUNT };
enum { AP_HIT, AP_KILL, AP_PARRY, AP_CELL, AP_LOST, AP_COUNT };   /* origine des points (journal) */

#define PTS_MELEE_KILL     100     /* kill en melee, toutes armes */
#define PTS_PARRY          50      /* parry reussi, une fois par coup */
#define PTS_LANCE_JOUST    100     /* joute de la Lance (l'estoc = meleePts) */
#define PTS_FIST_COMBO_MAX 50      /* Poings : au plus 50 points par combo */
#define PTS_SHIELD_PERFECT 100     /* Egide : parry parfait (remplace PTS_PARRY) */
#define PICKUP_SMALL       100     /* petite cellule : points a chaque arme du loadout */
#define PICKUP_BIG         150     /* grande cellule (endroits exposes) */
#define REFUND_UNDO_T      0.5f    /* coup declare pare par l'hote : on rend ses points s'il a moins de 0,5 s */
#define SWITCH_TIME        0.22f   /* changement d'arme (valeur deja codee en dur) */
#define DRYFIRE_CD         0.35f   /* delai entre deux clics a sec */
#define ST_MASK            0xF0    /* NetPState.pad : bits 4-7 = etats d'arme, bits 0-3 = meleeKind */

/* points de melee par coup qui touche (Lance : estoc ; Egide : coup de bouclier) */
static const int meleePts[MK_COUNT] = { 50, 75, 50, 35, 25, 50 };

/* gauge : points pour gagner 1 munition (0 = pas de jauge). Tesla : points par unite de batterie */
typedef struct { const char *name; int start, max; float gauge; float r, g, b; } AmmoDef;
static const AmmoDef ammoDef[WP_COUNT] = {
    { "RAILGUN",   2,  2,   250, 0.40f, 0.85f, 1.00f },   /* tres haut degats : 2 tirs, tres lent */
    { "ROQUETTES", 4,  8,   50,  1.00f, 0.60f, 0.20f },   /* 1 coup d'epee = 1 roquette */
    { "EVENTAIL",  3,  6,   100, 1.00f, 0.85f, 0.35f },
    { "ELAN",      3,  6,   80,  0.55f, 1.00f, 0.55f },
    { "SCIE",      1,  1,   0,   0.90f, 0.90f, 0.95f },   /* objet unique : aucune jauge */
    { "VENTOUSE",  1,  3,   150, 1.00f, 0.35f, 0.35f },
    { "TESLA",     60, 100, 2.5f, 0.60f, 0.70f, 1.00f },  /* 1 point = 0,4 de batterie */
};
static const char *meleeNames[MK_COUNT] = { "EPEE", "ENCLUME", "LANCE", "FLEAU", "IMPULSION", "EGIDE" };
static unsigned char wpReady[WP_COUNT] = { 1, 1, 0, 0, 0, 0, 0 };   /* chaque fiche passe sa case a 1 */
static unsigned char mkReady[MK_COUNT] = { 1, 0, 0, 0, 0, 0 };
```

`RAIL_AMMO_START`, `RAIL_AMMO_MAX` et `PICKUP_AMMO` disparaissent. `REFUND_HIT/KILL/PARRY`, `REFUND_KILL_PTS`, `TESLA_PER_PT` (ancienne version) et `regen`/`REGEN_DELAY` n'existent plus.

## 4. Données

```c
/* Player : remplace "int railAmmo;" ; "weapon" change de sens (WP_* et non plus 0/1) */
int weapon;                    /* WP_* en main */
int loadout[2];                /* deux WP_* distincts */
int meleeKind;                 /* MK_* */
int ammo[WP_COUNT];            /* munitions (batterie Tesla 0..100) */
float ammoGauge[WP_COUNT];     /* points vers la prochaine munition (0..gauge), 0 au maximum */
int swingHitPts, swingParryPts;/* ce coup a deja donne ses points de touche / de parry */
float undoPts[2];              /* points de la derniere touche, par emplacement (annulables) */
double undoT;

/* Pickup : un champ */
typedef struct { V3 pos; float t, pending; int pts; } Pickup;   /* pts : PICKUP_SMALL ou PICKUP_BIG */

/* globaux (survivent a reset_game) */
static int wantLoadout[2] = { WP_RAIL, WP_ROCKET }, wantMelee = MK_SWORD;
static int loadoutOpen;                            /* ecran LOADOUT ouvert (comme settingsOpen) */
static float ammoGainT, dryHintT;                  /* HUD : "+50" vert, panneau de melee qui clignote */
static int ammoGainSlot, ammoGainPts;
static float apStat[AP_COUNT], dryT;               /* journal : points par origine, temps a sec */
static int cellsTaken, hitCount[W_COUNT];          /* hitCount etait [7] */
```

- `NetPState` : taille inchangée ; `weapon` = `WP_*` ; `pad` = `meleeKind | ST_*`.
- `NetEvent`, `PkWelcome`, `PkSnap` : inchangés (`PkWelcome.pad[0]` reste réservé au choix de carte).
- `weaponNames[W_COUNT]` : `"RAIL", "ROQUETTE", "EPEE", "GRAPPIN", "PARRY", "SOLO", "RICOCHET", "ENCLUME", "LANCE", "FLEAU", "POINGS", "EGIDE", "EVENTAIL", "ELAN", "SCIE", "VENTOUSE", "TESLA"`.

## 5. Algorithme

```c
/* --- Combat : fonctions communes --- */
static int cur_slot(void) { return P.weapon == P.loadout[1]; }
static int other_wp(void) { return P.loadout[cur_slot() ^ 1]; }
static int wp_has_ammo(int w) { return P.ammo[w] > 0; }
static int is_melee_w(int w) { return w == W_MELEE || (w >= W_SLAM && w <= W_SHIELD); }
static int is_parryable_w(int w) { return is_melee_w(w) || w == W_HOOK; }
static int wp_full(int w) { return ammoDef[w].gauge <= 0 || P.ammo[w] >= ammoDef[w].max; }
static int loadout_full(void) { return wp_full(P.loadout[0]) && wp_full(P.loadout[1]); }
static float gauge_frac(int w) {                    /* HUD : 0..1, pleine au maximum */
    if (ammoDef[w].gauge <= 0) return 0;
    return P.ammo[w] >= ammoDef[w].max ? 1 : P.ammoGauge[w] / ammoDef[w].gauge;
}

static float gauge_add(int w, float pts) {          /* renvoie les points inutilises */
    const AmmoDef *d = &ammoDef[w];
    if (d->gauge <= 0 || P.ammo[w] >= d->max) return pts;
    P.ammoGauge[w] += pts;
    while (P.ammoGauge[w] >= d->gauge) {
        P.ammoGauge[w] -= d->gauge;
        if (++P.ammo[w] >= d->max) { float rest = P.ammoGauge[w]; P.ammoGauge[w] = 0; return rest; }
    }
    return 0;
}
static void gauge_remove(int w, float pts) {        /* annulation : peut reprendre une munition */
    if (ammoDef[w].gauge <= 0 || pts <= 0) return;
    P.ammoGauge[w] -= pts;
    while (P.ammoGauge[w] < 0 && P.ammo[w] > 0) { P.ammo[w]--; P.ammoGauge[w] += ammoDef[w].gauge; }
    if (P.ammoGauge[w] < 0) P.ammoGauge[w] = 0;
}

/* points de melee : arme en main d'abord, le surplus a l'autre arme */
static void ammo_points(float pts, int src) {
    if (P.dead || pts <= 0) return;
    int s = cur_slot(), a0 = P.ammo[P.loadout[0]], a1 = P.ammo[P.loadout[1]];
    float rest = gauge_add(P.loadout[s], pts), lost = gauge_add(P.loadout[s ^ 1], rest);
    apStat[src] += pts - lost; apStat[AP_LOST] += lost;
    if (src == AP_HIT) {                            /* memorise pour une annulation eventuelle */
        P.undoPts[s] = pts - rest; P.undoPts[s ^ 1] = rest - lost; P.undoT = nowT;
    }
    if (pts - lost <= 0) return;
    ammoGainT = 0.8f; ammoGainPts = (int)pts; ammoGainSlot = pts - rest > 0 ? s : s ^ 1;
    if (P.ammo[P.loadout[0]] > a0 || P.ammo[P.loadout[1]] > a1) play(S_PAD, 0.5f, 3.0f);   /* munition gagnee */
    else play(S_PAD, 0.35f, 2.4f);
}
static void melee_start_swing(void) { P.swingHitPts = P.swingParryPts = 0; }
static void melee_hit_points(int pts) { if (!P.swingHitPts) { P.swingHitPts = 1; ammo_points((float)pts, AP_HIT); } }
static void melee_parry_points(int pts) { if (!P.swingParryPts) { P.swingParryPts = 1; ammo_points((float)pts, AP_PARRY); } }

static int loadout_valid(const int *l, int mk) {
    return l[0] >= 0 && l[0] < WP_COUNT && l[1] >= 0 && l[1] < WP_COUNT && l[0] != l[1] &&
           wpReady[l[0]] && wpReady[l[1]] && mk >= 0 && mk < MK_COUNT && mkReady[mk];
}
static void loadout_apply(void) {
    if (!loadout_valid(wantLoadout, wantMelee)) { wantLoadout[0] = WP_RAIL; wantLoadout[1] = WP_ROCKET; wantMelee = MK_SWORD; }
    P.loadout[0] = wantLoadout[0]; P.loadout[1] = wantLoadout[1]; P.meleeKind = wantMelee;
    for (int w = 0; w < WP_COUNT; w++) { P.ammo[w] = ammoDef[w].start; P.ammoGauge[w] = 0; }
    P.weapon = P.loadout[0]; P.switchT = 0; P.undoPts[0] = P.undoPts[1] = 0;
    melee_start_swing();
}
static void weapon_select(int w) { if (w != P.weapon) { P.weapon = w; P.switchT = SWITCH_TIME; } }

static void fire_weapon(void) {
    switch (P.weapon) {
    case WP_RAIL:   fire_rail(); break;            /* "P.railAmmo--" -> "if (P.ammo[WP_RAIL] > 0) P.ammo[WP_RAIL]--" */
    case WP_ROCKET: fire_rocket(); break;          /* ajout en tete : idem pour WP_ROCKET */
    /* WP_SHOTGUN, WP_KINETIC, WP_SAW, WP_MINE : fiches 06 a 09 ; WP_TESLA : tesla_tick (tir maintenu) */
    }
}
static void dry_fire(void) {
    play(S_HIT, 0.5f, 0.45f);
    P.fireCd = DRYFIRE_CD;
    int o = other_wp();
    if (wp_has_ammo(o)) { snprintf(popText, sizeof popText, "%s A SEC", ammoDef[P.weapon].name); weapon_select(o); }
    else { snprintf(popText, sizeof popText, "A SEC : VA AU CONTACT !"); dryHintT = 2; }
    popT = 1.0f;
}
static void loadout_cycle(int slot) {               /* ecran de mort : arme prete suivante, echange si doublon */
    int w = wantLoadout[slot];
    do w = (w + 1) % WP_COUNT; while (!wpReady[w]);
    if (w == wantLoadout[slot ^ 1]) wantLoadout[slot ^ 1] = wantLoadout[slot];
    wantLoadout[slot] = w;
    cfg_save();
}
```

**`game_tick`** (remplace le bloc `nw = 1 - P.weapon` et le bloc de tir) :

```c
if (P.dead) {
    if (w1) loadout_cycle(0);
    if (w2) loadout_cycle(1);
    if (meleePress) { do wantMelee = (wantMelee + 1) % MK_COUNT; while (!mkReady[wantMelee]); cfg_save(); }
} else {
    int nw = P.weapon;
    if (w1) nw = P.loadout[0];
    if (w2) nw = P.loadout[1];
    if (swap) nw = other_wp();
    weapon_select(nw);
}
...
if (!P.dead && (act[A_FIRE] || lmbp) && P.fireCd <= 0 && P.switchT <= 0) {
    if (wp_has_ammo(P.weapon)) fire_weapon(); else dry_fire();
}
if (!P.dead && !wp_has_ammo(P.loadout[0]) && !wp_has_ammo(P.loadout[1])) dryT += dt;
ammoGainT -= dt; dryHintT -= dt;
```

**`melee()`** :
- `melee_start_swing();` en tête ;
- branche kamikaze : `melee_parry_points(PTS_PARRY);` (pas de bonus de kill) ;
- après `damage_enemy(i, melee_dmg(rel), ...)` : `if (!e->alive) ammo_points(PTS_MELEE_KILL, AP_KILL);` ;
- en fin de fonction : `if (hits) melee_hit_points(meleePts[P.meleeKind]);`.

**`try_parry(range, speedMult)`** (signature commune, PLAN § 4.2) : après chaque renvoi, `melee_parry_points(PTS_PARRY);`. `parry_feedback` ne donne **pas** de points, car il est appelé par plusieurs chemins.

**`apply_event`** :

```c
case EV_KILL:           /* apres le bloc "k == myId && v != myId" existant */
    if (k == myId && v != myId && is_melee_w(ev->sub)) ammo_points(PTS_MELEE_KILL, AP_KILL);
case EV_MELEE_PARRY:    /* def == myId : apres parry_feedback */
    ammo_points(PTS_PARRY, AP_PARRY);       /* l'Egide passe PTS_SHIELD_PERFECT pour un parry parfait (05) */
                        /* att == myId : le coup ne compte plus */
    if (is_melee_w((int)ev->f[0]) && nowT - P.undoT < REFUND_UNDO_T) {
        gauge_remove(P.loadout[0], P.undoPts[0]); gauge_remove(P.loadout[1], P.undoPts[1]);
        P.undoPts[0] = P.undoPts[1] = 0;
    }
case EV_RESPAWN:        /* remplace "P.railAmmo = RAIL_AMMO_START;" */
    loadout_apply();
```

**`reset_game`** : `loadout_apply();` après le `memset` (remplace `P.railAmmo = RAIL_AMMO_START`).

**`addpickup` / `build_level`** : `addpickup(x, y, z, pts)`. Appels : `(0, 13, 0, PICKUP_BIG)`, les plateformes hautes en `PICKUP_SMALL`, et dans la boucle `addpickup(sx * 36.0f, 13, sz * 36.0f, PICKUP_BIG)` / `addpickup(sx * 46.0f, 0, sz * 46.0f, PICKUP_SMALL)`.

**`pickup_tick`** : la condition `P.railAmmo >= RAIL_AMMO_MAX` devient `loadout_full()`.

**`pickup_taken`** (branche `who == myId`) :

```c
float pts = (float)PU[i].pts;
for (int s = 0; s < 2; s++) { float lost = gauge_add(P.loadout[s], pts); apStat[AP_CELL] += pts - lost; apStat[AP_LOST] += lost; }
snprintf(popText, sizeof popText, "CELLULE +%d", PU[i].pts);
ammoGainT = 0.8f; ammoGainPts = PU[i].pts; ammoGainSlot = 2;   /* 2 = les deux emplacements clignotent */
cellsTaken++;
```

**`pack_state`** : `s->weapon = (unsigned char)P.weapon; s->pad = (unsigned char)((P.meleeKind & 0x0F) | st_bits());`, où `st_bits()` renvoie 0 pour l'instant et sera complétée par les fiches (`ST_SLAM`, `ST_LANCE`, `ST_SHIELD`, `ST_ZAP`).

**`host_handle`** (qui renvoie maintenant 1 quand il annule, PLAN § 4.1) :
- `if (ev->sub >= W_COUNT) return 1;`
- `hitCount[ev->sub]++;`
- le test de parry devient `is_parryable_w(ev->sub) && nowT - NP[v].lastParryT < parry_grace(ev->sub, ...)`, avec `EV_MELEE_PARRY.f[0] = ev->sub` ;
- `clash = is_melee_w(ev->sub) && ...`.

**Autres migrations** `railAmmo` → `ammo[]` : `bot_think` (§ 9), `run_tests` (§ 10), la scène `--shot` (`P.weapon = WP_RAIL; P.ammo[WP_RAIL] = 2`), `draw_hud` (§ 8), et le résumé de `WinMain` (boucle sur `W_COUNT` avec `weaponNames`).

**`cfg_save` / `cfg_load`** : `loadout=%d,%d` et `melee=%d`. Après lecture, si `!loadout_valid(...)`, on revient aux valeurs par défaut.

## 6. Réseau

| Élément | v3 | v4 |
|---|---|---|
| `NET_VER` | 3 | **4** (une seule fois pour toutes les fiches) |
| `NetPState.weapon` | emplacement 0/1 | `WP_*` (0 et 1 inchangés) |
| `NetPState.pad` | 0 | bits 0-3 `meleeKind`, bits 4-7 `ST_*` |
| `EV_HIT.sub`, `EV_KILL.sub` | `W_*` 0 à 6 | `W_*` 0 à 16 |
| `EV_MELEE_PARRY.f[0]` | — | `W_*` du coup paré (PLAN § 4.2), lu par l'attaquant pour annuler ses points |
| Cellules | une taille | `pts` par cellule, déterministe (même `build_level`), non transmis |
| Taille des paquets | — | inchangée |

- **Qui calcule quoi** : les munitions, les jauges, la sélection et les points sont **locaux** (jamais transmis, aucun arbitrage nécessaire). Les kills de mêlée sont **décidés par l'hôte** et reconnus par le tueur dans `EV_KILL`. Les cellules sont arbitrées par l'hôte (premier arrivé), et les points appliqués par le ramasseur.
- **Ce que voient les autres** : l'arme en main et la mêlée. Les jauges et munitions des autres ne sont pas visibles (question 8).
- **Compatibilité** : v3 et v4 ne se voient pas (`net_poll` jette le paquet). Un `velocity.ini` sans `loadout=` donne le défaut.
- **Coordination** : IDEES § 4.7 (choix de carte) demande aussi `NET_VER` 4 : il faut les sortir ensemble, sinon la carte passera à 5 (question 9).
- **Latence** : seule l'annulation des points d'un coup paré dépend du réseau ; `REFUND_UNDO_T` = 0,5 s couvre la tolérance de parry la plus longue (0,25 s) plus un aller-retour.

## 7. Interactions

- **Autres fiches de mêlée** (01 à 05) : `ammo_refund(REFUND_HIT)` est remplacé par `melee_start_swing()` au début d'un coup, puis `melee_hit_points(pts)` quand il touche. Valeurs : Enclume `meleePts[MK_ANVIL]` (75) par onde ou coup qui touche ; Lance `PTS_LANCE_JOUST` (100) pour la joute, `meleePts[MK_LANCE]` (50) pour l'estoc ; Fléau 35 ; Poings 25 avec le plafond `PTS_FIST_COMBO_MAX` (remplace `FIST_REFUND_MAX` de PLAN § 4.5) ; Égide 50, et `PTS_SHIELD_PERFECT` pour un parry parfait. Les parries passent par `melee_parry_points(PTS_PARRY)`.
- **Autres fiches à distance** (06 à 10) : décrémentent `P.ammo[WP_x]` dans leur `fire_*` et se branchent dans `fire_weapon`. Elles ne touchent jamais à la jauge. Tesla : la batterie est `P.ammo[WP_TESLA]`.
- **Pièces** : non concernées (`P.coins` à part). Un ricochet coûte 1 rail, soit 250 points.
- **Grappin** : l'impact `W_HOOK` est parable mais ne donne pas de points (question 4).
- **Rocket jump** : coûte 1 roquette = 50 points = un coup d'épée qui touche.
- **Chaîne de tir** : inchangée ; un clic à sec n'appelle pas `shot_result`.

## 8. Rendu et son

**HUD (`draw_hud`, coin bas droit)**

- Compteur (police 1, `winH - 60`) : `RAILGUN 1/2` dans la couleur `ammoDef[w]`, en rouge à 0, grossi 0,15 s quand une munition est gagnée. Tesla : `TESLA 60%`. Scie : texte fourni par `08_scie`.
- Barre de recharge du tir (`winH - 40`, 200 × 8, existante) : `cdMax` lu dans une table `wpCd[WP_COUNT]` remplie par les fiches.
- **Barre de jauge** (nouvelle, `winH - 28`, 200 × 5) : fond sombre, remplissage `gauge_frac(P.weapon)` dans la couleur de l'arme. Au maximum, elle est pleine et plus pâle (alpha 0,45) ; en gain (`ammoGainT > 0`), elle est blanche et « +50 » vert apparaît à droite. Tesla : la barre affiche la batterie (`ammo / 100`). Scie : pas de barre.
- Ligne des emplacements (police 0, `winH - 10`, remplace l'astuce « [1] rail [2] roquettes ») : `[1] RAILGUN 1   [2] ROQUETTES 5   [molette]`, chaque texte suivi d'une mini-jauge 40 × 3. L'emplacement actif est en blanc, l'autre en gris, une arme à sec en rouge.
- Panneau de mêlée : titre `meleeNames[P.meleeKind]` et, sous les dégâts, `+50 PTS / coup`. Si `dryHintT > 0`, le titre clignote en orange : « A SEC : MELEE = MUNITIONS ».
- Si `want* != P.*` : petite ligne grise « nouveau loadout au prochain respawn ».
- Écran de mort : les trois lignes de choix (§ 2.6).

**Écran `draw_loadout(cx, cy)`** (style de `draw_settings`) :

```c
rect(0, 0, winW, winH, 0.02f, 0.02f, 0.06f, 0.72f);
text_c(2, cx, cy - 300, "LOADOUT", 0.5f, 0.9f, 1.0f, 1);
float bw = winW < 1300 ? 110 : 150, step = bw + 8, x0 = cx - step * 3.5f;
ligne "MELEE"  (y = cy - 200) : sel_button(x0 + mk * step, y, bw, 46, meleeNames[mk], mk == wantMelee, mkReady[mk]);
ligne "ARME 1" (y = cy - 90)  : sel_button(..., ammoDef[w].name, w == wantLoadout[0], wpReady[w]);  /* echange si doublon */
ligne "ARME 2" (y = cy + 20)  : idem pour l'emplacement 2;
info arme  (y = cy + 110) : "RAILGUN : 2 au spawn, 2 max, 250 pts par tir (5 coups d'epee)";
info melee (y = cy + 136) : "EPEE : 50 pts par coup, +100 par kill, +50 par parry";
si appState == APP_GAME : "pris en compte au prochain respawn";
if (button(cx - 105, cy + 180, 210, 50, "VALIDER")) { loadoutOpen = 0; cfg_save(); }
```

`sel_button` = `button` précédé d'un cadre de 3 px dans la couleur de l'arme si elle est choisie. Une arme non prête est grise, libellée « BIENTOT », et ne réagit pas au clic. Branchements : `draw_hud` (`loadoutOpen` à côté de `settingsOpen`, pour `APP_MENU` et en pause) et Échap dans `wndproc`. Nouvelle scène `--shot loadout.bmp`.

**Monde** : `draw_viewmodel` et `draw_remote_players` prennent la couleur du canon dans `ammoDef[...]`. L'épée n'est dessinée que si la mêlée est `MK_SWORD` (`(st.pad & 15)` pour les autres joueurs) ; les autres fiches ajoutent leur branche. Cellules : § 2.5 (grandes = ×1,4, arêtes violettes, halo 1,9).

**Son** : aucun `S_*` nouveau. Clic à sec = `S_HIT` grave ; points = `S_PAD` aigu et bref ; munition gagnée = `S_PAD` plus fort (hauteur 3,0).

## 9. Bots

- Loadout : défaut. L'argument proposé `--loadout a,b,m` (lu dans `WinMain`) remplit `wantLoadout`/`wantMelee` pour les matchs de bots avec les nouvelles armes.
- `bot_pick_weapon()` remplace `P.weapon = (P.railAmmo <= 0 || ...) ? 1 : 0;` :

```c
int a = P.loadout[0], b = P.loadout[1];
int pref = (bd < 20 && ((int)(botT / 5 + botMode)) % 2) ? b : a;     /* meme alternance qu'aujourd'hui */
if (!wp_has_ammo(pref)) pref = pref == a ? b : a;
P.weapon = pref;
int dry = !wp_has_ammo(a) && !wp_has_ammo(b);
```

- **À sec** (`dry`) : le bot ne tire plus (il saute les blocs « contre-jeu », « pièce + tir » et « recale la visée ») et **va au contact** : dash vers la cible dès `bd > 4` (au lieu de 14), grappin sur la cible dès `bd > 6` (au lieu de 10). L'épée au contact existe déjà. C'est la boucle voulue : la mêlée remplit les jauges.
- **Cellules** : `P.railAmmo < 2` devient `!wp_has_ammo(a) || !wp_has_ammo(b)`, toujours à condition que personne ne soit à moins de 25 m. Le bot préfère les grandes cellules à distance égale : on compare `d - (PU[i].pts == PICKUP_BIG ? 8 : 0)`.
- Le contre-jeu sur les pièces adverses (1 rail = 250 points) n'est tenté que si `P.ammo[WP_RAIL] == 2`, pour ne pas gaspiller le dernier rail.

## 10. Tests (`run_tests`)

Avant « LAN simulé » sauf mention contraire, dans le couloir x = −20.

1. **Spawn** : `reset_game()` → `ammo[RAIL]=2`, `ammo[ROCKET]=4`, jauges 0, `weapon=WP_RAIL`, `meleeKind=MK_SWORD` (attendu 2, 4, 0, 0, 0).
2. **Conversion** : roquettes en main, `ammo[ROCKET]=0` ; `ammo_points(50)` → 1 et jauge 0 ; `ammo_points(70)` → 2 et jauge 20 (attendu 1/0 puis 2/20).
3. **Surplus vers l'autre arme** : rail en main au maximum (2) ; `ammo_points(100)` → rail 2, roquettes +2 (attendu 2, 6).
4. **Maximum** : roquettes 7, jauge 30, rail plein ; `ammo_points(100)` → roquettes 8, jauge stockée 0, `gauge_frac` 1, `apStat[AP_LOST]` +80 ; un tir → 7 et jauge 0 (attendu 8, 0, 1, 80, 7, 0).
5. **Épée en solo** : drone à 2 m, 100 PV, roquettes en main à 0 → `melee()` → 1 roquette. Drone à 10 PV → 50 + 100 → 3 roquettes. Un kamikaze → 50 + 50 → 2 roquettes (attendu 1, 3, 2).
6. **Un parry par coup** : deux orbes dans la même fenêtre → +50 une seule fois (attendu 50).
7. **Kill en LAN** : `netMode=NET_HOST`, `NP[1]` à 40 PV ; `EV_HIT` de 0 vers 1 en `W_MELEE` pour 60 → `host_handle` → `EV_KILL` → +100 ; même chose en `W_RAIL` → +0 (attendu 100, 0).
8. **Annulation** : roquettes 0, touche d'épée (+50 → 1 roquette), puis `EV_MELEE_PARRY` avec `target=myId`, `f[0]=W_MELEE` dans les 0,5 s → 0 roquette, jauge 0 ; même chose avec `f[0]=W_HOOK` → pas d'annulation (attendu 0/0 puis 1).
9. **Cellules** (remplace le test « recharges ») : cellule 6 (coin, petite) avec rail 0/jauge 0 et roquettes 2 → rail jauge 100, roquettes 4 ; `PU[5].pts == 150`, `PU[0].pts == 150` ; loadout plein → cellule non prise ; réapparition après 12 s (attendu 100, 4, 1, 1, 0, 1).
10. **Tesla** : loadout forcé `{WP_TESLA, WP_RAIL}`, batterie 50 ; `ammo_points(100)` → 90 (attendu 90).
11. **Rocket jump** : `fire_rocket()` vers le sol → `ammo[ROCKET]` 4 → 3 (attendu 3).
12. **Clic à sec** : roquettes 0, rail 0, `botHeld[A_FIRE]` pendant 120 ticks → pas de tir, « A SEC : VA AU CONTACT ! » ; rail 1 → bascule sur le rail (attendu 0, 1).
13. **Respawn**, **`velocity.ini`** (`loadout=1,0`, puis `3,3` invalide → `0,1`) et **`pack_state`** (`st.weapon`, `st.pad & 15`), comme dans la version précédente.
14. Tests existants : la chaîne de tir fait 4 tirs avec un rail qui n'en a que 2. Le test met `P.ammo[WP_RAIL] = 9` avant (`fire_rail` ne descend pas sous 0, mais le test lit les dégâts, pas les munitions). À migrer aussi : le contre-jeu et la scène `--shot`. Le cross-up du bot est à revérifier (2 rails, le bot sera vite à sec).

## 11. Équilibrage

L'infra ne change aucun dégât (`PVP_SCALE` 0,7 et plafond de 60 identiques) : elle règle l'**économie** des tirs.

| Arme | Points / munition | Coups d'épée pour 1 tir | Kill à l'épée (200 pts) | Petite cellule (100) | Grande cellule (150) |
|---|---|---|---|---|---|
| Rail | 250 | 5 | 0,8 rail | 0,4 | 0,6 |
| Roquettes | 50 | 1 | 4 roquettes | 2 | 3 |
| Éventail | 100 | 2 | 2 | 1 | 1,5 |
| Élan | 80 | 1,6 | 2,5 | 1,25 | 1,9 |
| Ventouse | 150 | 3 | 1,3 | 0,67 | 1 |
| Tesla | 2,5 / unité | 50 pts = 20 batterie = 1 s d'arc | 80 = 4 s | 40 = 2 s | 60 = 3 s |

| Mêlée | Points par touche | Kill « typique » | Total |
|---|---|---|---|
| Épée | 50 | 2 touches + kill | 200 |
| Enclume | 75 | 2 touches + kill | 250 |
| Lance | 100 (joute) / 50 (estoc) | joute + estoc + kill | 250 |
| Fléau | 35 | 4 touches + kill | 240 |
| Poings | 25, 50 max par combo | 1 combo de 3 + kill | 150 |
| Égide | 50 | variable | 50 à 200 |

Au spawn : 2 rails + 4 roquettes = **6 tirs** (contre 5 + ∞ aujourd'hui). Le rail devient rare : 1 tir pour 5 touches d'épée.

À ajouter au résumé du journal du bot (hôte et client) :
- `points par origine` : `apStat[AP_HIT/KILL/PARRY/CELL]` et `AP_LOST` (points perdus quand tout est plein) ;
- `tirs par vie` par `WP_*`, `cellules prises par vie` (petites / grandes) ;
- `temps à sec` = `dryT / temps en vie`, cible de 10 à 20 % ;
- `rocket jumps par vie` (roquette dont le souffle pousse le tireur sans toucher personne) ;
- `touches arbitrées` par `W_*` (17 colonnes).

Leviers : si le temps à sec est trop haut, kill à 150, petite cellule à 120 ou rail à 200 points ; s'il est trop bas, roquettes à 6 au maximum. Comparer avant / après sur un match de bots de 90 s.

## 12. Étapes d'implémentation

| # | Étape | Fonctions | Estimation |
|---|---|---|---|
| 1 | Enums `MK_*`/`WP_*`/`W_*` (17), `ammoDef`, `meleePts`, `weaponNames`, `hitCount[W_COUNT]`, résumé de `WinMain`, `draw_killfeed` | en-tête, `host_handle` | 30 min |
| 2 | `Player` (`loadout`, `meleeKind`, `ammo`, `ammoGauge`), `loadout_apply`, migration de `railAmmo` | `reset_game`, `apply_event`, `fire_rail`, `fire_rocket`, `--shot` | 45 min |
| 3 | Sélection par `WP_*`, `fire_weapon`, `dry_fire`, roquettes limitées | `game_tick` | 30 min |
| 4 | `gauge_add`/`gauge_remove`/`ammo_points`, points de mêlée, kill et parry, annulation | `melee`, `try_parry`, `apply_event` | 1 h |
| 5 | Cellules à deux tailles : `Pickup.pts`, `addpickup`, répartition, rendu | `build_level`, `pickup_tick`, `pickup_taken`, `render` | 30 min |
| 6 | Réseau : `pad`, `NET_VER 4`, parry étendu, rendu distant | `pack_state`, `host_handle`, `draw_remote_players` | 30 min |
| 7 | HUD (compteur, jauge, emplacements), `draw_loadout`, menus, écran de mort, `cfg_*`, Échap | `draw_hud`, `draw_main_menu`, `draw_pause`, `wndproc`, `cfg_save`/`cfg_load` | 2 h |
| 8 | Bot (`bot_pick_weapon`, à sec → contact, grandes cellules, `--loadout`), tests 1 à 14, `--shot loadout.bmp`, match de bots avant / après | `bot_think`, `run_tests`, `WinMain` | 1 h 30 |

Total ≈ 7 h 30. Après chaque étape : `build.bat` sans avertissement, `--test`, puis un match de bots (étapes 6 et 8).

## 13. Questions ouvertes

1. ~~PLAN.md § 3 et § 4.5 parlent encore de `ammo_refund`~~ : **réglé** (PLAN.md et fiches 01 à 05 et 08 passés à `melee_hit_points` / `melee_parry_points` / `ammo_points`).
2. **Jauge au maximum** : elle est affichée pleine mais vaut 0 en interne (un tir depuis le maximum repart de 0, proposé). L'autre option est qu'une jauge pleine compte comme une munition en réserve : on aurait alors max + 1 tirs.
3. **Kamikaze tué à l'épée (solo)** : 50 (parry) + 50 (touche), sans bonus de kill (proposé), ou +100 en plus ? Les kamikazes sont nombreux.
4. **Impact de grappin (`W_HOOK`)** et **projectile renvoyé (`W_PARRY`)** : donnent-ils des points de touche et de kill ? Proposé : non (le parry a déjà donné ses 50).
5. **Coup qui touche plusieurs cibles** : les points une seule fois par coup (proposé) ou par cible ?
6. **Égide** : le parry parfait vaut-il 100 **à la place** des 50 du parry (proposé) ou 100 + 50 ?
7. **Cellules** : on garde la répartition proposée (5 grandes : tour et perchoirs ; 8 petites) ? Et une cellule est-elle prise quand une seule arme en profite (proposé) ?
8. **Lisibilité en PvP** : faut-il montrer aux autres qu'un joueur est à sec (par exemple un bit de `flags`, 64 ou 128 sont libres), pour qu'on sache quand foncer sur lui ?
9. **`NET_VER` 4** : sortir le choix de carte (IDEES § 4.7) dans la même version ? Sinon la carte demandera la version 5.
10. **Identifiants non prévus par PLAN.md**, ajoutés sans rien décaler : `MK_COUNT`, `ST_MASK`, `AP_*`, `PTS_*`, `PICKUP_SMALL`/`PICKUP_BIG`, `REFUND_UNDO_T`, `SWITCH_TIME`, `DRYFIRE_CD`, les tables `ammoDef`, `meleePts`, `wpReady` et `mkReady`, et les fonctions `ammo_points`, `melee_hit_points`, `melee_parry_points`, `melee_start_swing`. À valider pour que les autres fiches s'en servent.
11. **Solo** : appliquer le loadout dès la sortie du menu pause, ou seulement à R / à la mort (proposé) ?
12. **Écran de mort** : `A_MELEE` fait défiler la mêlée. Préfères-tu une autre touche (par exemple `A_COIN`) pour éviter un changement par réflexe ?
