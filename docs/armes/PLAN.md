# Plan : fiches de design détaillées des armes

> Source : [`../IDEES_ARMES_CARTES.md`](../IDEES_ARMES_CARTES.md) (armes validées, règle des munitions § 1.5, Scie § 3.3).
> Code : [`../CODE.md`](../CODE.md) (documentation de `main.c`) et `main.c`.
> Ces fiches sont des **documents de design prêts à implémenter** : rien n'est codé ici, `main.c` n'est pas modifié.

## 1. Les fiches

Chaque fiche est rédigée en parallèle. Les choix d'interface qui touchent plusieurs fiches sont **figés ci-dessous** pour éviter les conflits.

| Fichier | Contenu | Coût visé |
|---|---|---|
| `00_infra_loadout_munitions.md` | Loadout (1 mêlée + 2 distance), munitions par arme, cellules génériques, recharge par la mêlée, roquettes limitées, `NET_VER` 4 | S |
| `01_enclume.md` | Marteau-pilon : coup au sol, piqué, onde de choc | M |
| `02_lance.md` | Lance de joute : lance couchée, estoc, choc de joute | M |
| `03_fleau.md` | Fléau à chaîne : traction, pièce frappée, parry à 6 m | M |
| `04_impulsion.md` | Poings : combo de 3 coups, punch-jump | S |
| `05_egide.md` | Bouclier : blocage frontal, jauge, parry parfait, réseau | L |
| `06_eventail.md` | Fusil à pompe : plombs agrégés, recul, pièce qui éclate | S |
| `07_elan.md` | Convertisseur cinétique : dégâts = vitesse dépensée | S |
| `08_scie.md` | Scie : plantée si ratée, retour si touche, rappel au grappin sans déplacer le joueur | M |
| `09_ventouse.md` | Mines collantes : colle, télécommande, parry pour décrocher | M |
| `10_tesla.md` | Arc électrique : faisceau continu, chaîne, relais sur pièce | M |

## 2. Modèle obligatoire d'une fiche

1. **Résumé validé** : rôle, règles et chiffres repris du document d'idées, avec ses corrections.
2. **Règles détaillées** : machine à états, durées, conditions exactes, cas limites (mort, respawn, changement d'arme, hitstop, fin de match).
3. **Constantes** : bloc `#define` prêt à coller, commentaires en français **sans accents** (convention de `main.c`).
4. **Données** : champs ajoutés à `Player`, `Proj`, `Coin`, `NetPState`…, préfixés par le nom de l'arme (`P.anvilT`, `P.sawState`…).
5. **Algorithme** : pseudo-code C, fonction par fonction de `main.c` (nom de la fonction existante ou nouvelle). Ce n'est pas un patch.
6. **Réseau** : qui calcule quoi (tireur ou victime), événements (identifiants alloués au § 3), arbitrage de l'hôte (`host_handle`), tolérance de latence.
7. **Interactions** : parry, pièces, grappin, autres armes, recharge de munitions par la mêlée.
8. **Rendu et son** : viewmodel (`draw_viewmodel`), joueurs distants (`draw_remote_players`), HUD (`draw_hud`), particules, impact frames, hitstop, sons synthétisés (`make_sounds`).
9. **Bots** : comment `bot_think` utilise l'arme (au moins un comportement simple).
10. **Tests** : scénarios déterministes à ajouter dans `run_tests`.
11. **Équilibrage** : tableau des valeurs PvE / PvP (× `PVP_SCALE` 0,7, plafond 60), ce qu'il faut mesurer dans le journal du bot.
12. **Étapes d'implémentation** ordonnées, avec une estimation.
13. **Questions ouvertes** pour l'utilisateur (choix qui ne sont pas encore tranchés).

## 3. Interfaces figées (communes à toutes les fiches)

### Identifiants

- **Type de mêlée** `P.meleeKind` : `MK_SWORD 0`, `MK_ANVIL 1`, `MK_LANCE 2`, `MK_FLAIL 3`, `MK_FIST 4`, `MK_SHIELD 5`.
- **Arme à distance** `P.weapon` = identifiant d'arme (plus un numéro d'emplacement) : `WP_RAIL 0`, `WP_ROCKET 1`, `WP_SHOTGUN 2`, `WP_KINETIC 3`, `WP_SAW 4`, `WP_MINE 5`, `WP_TESLA 6`, `WP_COUNT 7`. Le loadout est `P.loadout[2]` (deux `WP_*`). `NetPState.weapon` transmet le `WP_*`.
- **Source de dégâts** `W_*` (existants 0 à 6) : `W_SLAM 7`, `W_LANCE 8`, `W_FLAIL 9`, `W_FIST 10`, `W_SHIELD 11`, `W_SHOTGUN 12`, `W_KINETIC 13`, `W_SAW 14`, `W_MINE 15`, `W_TESLA 16`, `W_COUNT 17`. `hitCount` passe à `W_COUNT`.
- **Événements** (existants 1 à 14) : `EV_SHOT 15` (Éventail), `EV_SAW 16`, `EV_MINE 17`, `EV_TESLA 18`, `EV_SHIELD 19`, `EV_LANCE 20`, `EV_SLAM 21`. Fléau : `EV_COIN` sous-type 2 (pièce frappée). Poings : `EV_MELEE`. Élan : `EV_RAIL`.
- **Projectiles** (existants 0 à 2) : `PR_SAW 3`, `PR_MINE 4`.
- **Octet `NetPState.pad`** : bits 0 à 3 = `meleeKind`, bits 4 à 7 = états visibles : `ST_SLAM 0x10` (piqué en cours), `ST_LANCE 0x20` (lance couchée), `ST_SHIELD 0x40` (bouclier levé), `ST_ZAP 0x80` (arc Tesla actif). Les bits 64 et 128 de `flags` restent libres.
- **Sons** : chaque fiche ajoute ses `S_*` (préfixés par l'arme) avant `S_COUNT` et décrit leur synthèse.

### Règles communes

- **Munitions** (`00_infra`, système de jauges du 27/10, **aucune recharge passive**) : `P.ammo[WP_COUNT]` remplace `P.railAmmo`, et chaque arme a une jauge `P.ammoGauge[w]` (rail 250 points par munition, roquettes 50, etc.). Fonctions communes : `ammo_points(pts, src)` (points à l'arme en main, surplus à l'autre), `melee_hit_points(meleePts[kind])` une fois par coup qui touche, `melee_parry_points(PTS_PARRY)` (50), `PTS_MELEE_KILL` (100). Le kill est confirmé par `EV_KILL` (l'hôte décide en LAN). Les cellules donnent 100 ou 150 points aux deux armes du loadout.
- **PvP** : dégâts × `PVP_SCALE`, plafond de 60 PV par touche appliqué par l'hôte (sauf `W_SELF`). Une arme à touches multiples **agrège** ses dégâts : un seul `net_hit` par cible et par tir (ou par tranche de 0,2 s pour un rayon continu).
- **Parry côté hôte** : la liste des coups parables dans `host_handle` (aujourd'hui `W_MELEE`, `W_HOOK`) s'étend à `W_SLAM`, `W_LANCE`, `W_FLAIL`, `W_FIST`, `W_SHIELD`. Chaque fiche de mêlée précise ce qui se passe quand son coup est paré.
- **Touche validée chez le tireur**, état visible des autres via `NetPState`. L'hôte arbitre les PV, le parry et les kills.
- `NET_VER` passe à 4 **une seule fois** (infra), pour tout l'ensemble.

## 4. Relecture croisée : conventions ajoutées

Les 11 fiches ont été rédigées en parallèle. Cette section **tranche les points où elles se recoupent**. Elle l'emporte sur une fiche en cas de désaccord.

### 4.1 Bug du code actuel : **corrigé le 27/10/2026**

> Fait dans `main.c` : `host_handle` renvoie 1 quand il annule, `net_emit` et la nouvelle `host_event_in` (utilisée par `host_recv`) ne rejouent plus l'événement. Un test « recul d'un coup paré » vérifie 0 m/s si le coup est paré, 10 m/s sinon.

Un coup paré est quand même rejoué chez la victime. `host_recv` appelle `host_handle`, puis `apply_event` et `net_queue` **sans condition** : la victime d'une épée parée reçoit donc le recul et le flash de blessure, même si l'hôte n'a retiré aucun PV. Ce bug a été vérifié dans `main.c` et signalé par les fiches Lance et Enclume. Avec l'Enclume, la victime qui pare l'onde serait projetée vers le haut à 12 m/s.

**Correction** : `host_handle` renvoie 1 quand il annule un `EV_HIT` (parry, bouclier, fin de match). Dans ce cas, `host_recv` ne l'applique pas et ne le relaie pas. Pour ses propres coups, l'hôte passe par `net_emit`, qui doit aussi tenir compte de ce retour. Le refactor `host_apply_damage` de l'Égide s'appuie sur cette correction.

### 4.2 Parry de mêlée

- **Arme parée** : `EV_MELEE_PARRY.f[0]` = le `W_*` du coup paré (proposition du Fléau, retenue pour toutes les mêlées). Chaque fiche réagit à partir de ce champ, sans en ajouter d'autre.
- **Signature commune** : `try_parry(float range, float speedMult)`. L'épée appelle `try_parry(MELEE_RANGE + 1.5f, 1)`, le Fléau `try_parry(6, 0.8f)`. Les autres mêlées utilisent la valeur de l'épée, sauf mention dans leur fiche.
- **Tolérance de l'hôte** : `parry_grace(attaqueW, victimeMK)`.

  | Cas | Tolérance | Source |
  |---|---|---|
  | La victime joue les Poings (`MK_FIST`) | 0,05 s | 04, sinon un combo est imparable 95 % du temps |
  | Coup `W_SLAM` (onde de l'Enclume) | 0,20 s | 01 |
  | Décharge `W_TESLA` renvoyée | 0,25 s | 10 |
  | Tous les autres (épée, Lance, Fléau, bouclier, grappin) | 0,15 s (`MELEE_PARRY_GRACE`) | existant |

  La règle des Poings (côté victime) passe avant celle de l'arme qui attaque.

### 4.3 Champs réseau partagés

- **`NetPState.melee`** : sa signification dépend de `pad & 0x0F`. Pour l'épée, la Lance, le Fléau, l'Enclume et l'Égide, c'est le minuteur de coup actuel (0 à 255). Pour les Poings, c'est l'étape du combo sur 2 bits et l'avancement sur 6 bits (04). Le bot passe par `swing_start(i)` (04) au lieu de tester `melee >= 150`.
- **`EV_HIT`** : `f[0]` = dégâts, `f[1..3]` = recul. Les champs `f[4..6]` sont **propres à l'arme** indiquée par `sub` (par exemple la Lance : vitesse de l'attaquant et drapeau estoc ou couchée). `netId` = identifiant de l'objet source si besoin (mine de la Ventouse).
- **`EV_COIN`** : 0 = lancée, 1 = détruite (existant). La Tesla s'en sert pour la fin d'un relais. L'Éventail ajoute `f[3] = 1` pour un **éclat**. 2 = **frappée** par le Fléau. **3 = relancée** par la Scie (nouveau, car ce n'est pas la même chose qu'une pièce frappée), avec `f[6]` = vie restante.
- **`netId` des projectiles** : `(id + 1) << 24 | PR_kind << 20 | compteur & 0xFFFFF`, pour **tous** les projectiles (roquette 0, scie 3, mine 4). Cette règle généralise le « bit 23 » de la Scie et évite les collisions dans le tableau `PR` partagé.
- **`rayBox`** (08) : variable globale qui donne l'index de la dernière boîte touchée par `ray_world`. Elle servira aussi aux murs destructibles des cartes.

### 4.4 Bouclier : un seul point d'entrée

L'Égide impose que chaque arme teste le bouclier avant d'infliger des dégâts à un joueur. Convention : toutes les armes passent par `hit_player(i, dmg, knock, w, dir)`, qui appelle `shield_intercept()` puis `net_hit`. Tant que l'Égide n'est pas codée, `shield_intercept` renvoie toujours 0. Les fiches Lance, Fléau, Scie, Ventouse, Éventail et Tesla appellent `hit_player` à la place de `net_hit`.

### 4.5 Interactions arbitrées entre fiches

| Interaction | Décision proposée |
|---|---|
| Tesla contre Égide | L'arc est bloqué de face, comme un rail, et consomme la jauge à 10 par tranche de 0,2 s |
| Tesla, Éventail ou rail contre une mine (09) | Tout coup direct sur une mine la fait exploser (×1,5 pour le rail et le ricochet, ×1 pour un plomb ou l'arc) |
| Onde de l'Enclume contre Égide | Non bloquée : elle vient du sol, et c'est le contre naturel du bouclier |
| Lance couchée contre Égide | Bloquée de face, comme toute mêlée |
| Recharge de munitions des Poings | 25 points par coup, 50 au plus par combo (**validé**) |

### 4.6 Nouvel identifiant non prévu au départ

- `A_DETONATE` (09) : action « faire exploser les mines ». Elle suit la règle de `CLAUDE.md` (`A_*`, `actId`, `actLabel`, `bindDefault`). **Décision** : deux commandes séparées (`A_DETONATE_ALL`, `A_DETONATE_ONE`), voir la fiche 09.

## 5. Index

| Fiche | Lignes | Coût estimé | Points clés à retenir |
|---|---|---|---|
| [00 Infra : loadout et munitions](00_infra_loadout_munitions.md) | 526 | S (~7 h) | Écran de loadout, jauges de points (`ammo_points`), cellules de 100 et 150 ; les armes non codées sont grisées, donc l'infra peut sortir seule |
| [01 Enclume](01_enclume.md) | 372 | M | Hauteur minimale de 1,2 m pour le piqué (anti-bhop) ; un saut simple (1,47 m) n'esquive pas une onde réglée à 1,5 m |
| [02 Lance](02_lance.md) | 390 | M | L'estoc part à l'appui, la lance se couche à 0,15 s ; elle touche avant l'impact du grappin ; le choc de joute est arbitré par l'hôte |
| [03 Fléau](03_fleau.md) | 357 | M | Traction chiffrée ; pièce frappée en `W_COIN` ; signature commune de `try_parry` |
| [04 Impulsion](04_impulsion.md) | 420 | S | Punch-jump borné (au moins 11 m/s, plafond 20 m/s) ; tolérance de parry de 0,05 s |
| [05 Égide](05_egide.md) | 406 | L (~4,75 j) | `shield_intercept` ; l'hôte fait autorité sur la jauge ; tableau de réaction à chaque arme |
| [06 Éventail](06_eventail.md) | 391 | S | Motif de plombs déterministe ; recul plafonné ; éclat de pièce ; cône C (4,5° de chaque côté) : 56 / 37 / 8 PvP à 5 / 10 / 20 m |
| [07 Élan](07_elan.md) | 293 | S | `hitscan_shot` extrait de `fire_rail` ; risque du « dash-tir » double (120 PvP en 0,5 s) |
| [08 Scie](08_scie.md) | 544 | M | 6 états ; rappel au grappin sans déplacer le joueur ; 3 touches contre une cible immobile ; renvoi par parry au retour (48 m/s, aide à la visée de 7°) |
| [09 Ventouse](09_ventouse.md) | 406 | M (~14,5 h) | `A_DETONATE_ALL` et `A_DETONATE_ONE` ; table d'arbitrage `hostMine` ; mine-jump plus haut qu'un rocket jump (4,6 m contre 4,0 m) |
| [10 Tesla](10_tesla.md) | 441 | M | Voix sonores en boucle dans le mixeur ; liens d'arc dédiés (pas `add_beam`) ; 2 s pour tuer en PvP |

**Ordre d'implémentation recommandé** : 4.1 (correction du bug) → 00 Infra → 07 Élan → 01 Enclume, puis les autres selon tes choix. L'Égide (05) vient en dernier, car elle touche toutes les autres armes.

**État au 28/10/2026** (dépôt git, branche `master`) :

| Étape | État | Commit |
|---|---|---|
| 4.1 Bug du coup paré | **Fait** | (avant le dépôt) |
| 00 Infra : loadout, jauges, cellules | **Fait** | `3388cd6` |
| 07 Élan | **Fait** | `6e88bbd` |
| 01 Enclume | **Fait** | `fef969a`, fusion `b5169aa` |
| 02 Lance, 03 Fléau, 04 Poings, 06 Éventail, 08 Scie, 09 Ventouse, 10 Tesla | À faire | — |
| 05 Égide | À faire, en dernier | — |

`NET_VER` vaut 6. Pour un match de bots avec les nouvelles armes : `--loadout 3,1,1` (Élan, roquettes, Enclume).

## 6. Décisions de l'utilisateur (27/10/2026)

| # | Question | Décision |
|---|---|---|
| 1 | Le rocket jump coûte une roquette ? | **Oui.** |
| 2 | Scie : 2 ou 3 touches contre une cible immobile ? | **3 touches** (`SAW_REL_OFFSET` = 12). **Nouvelle mécanique** : parer la scie quand elle revient vers soi la renvoie plus vite, avec une légère aide à la visée (fiche 08). |
| 3 | Élan : brider le double dash-tir ? | **Non**, c'est une technique, mais sa **fenêtre est réduite** : le tir doit partir dans les 0,10 s qui suivent le dash (fiche 07). |
| — | Rail | **Moins de munitions**, car il fait beaucoup de dégâts : **2 munitions**. |
| 4 | Enclume : seuil de l'onde à 1,0 m ? | **Oui** : l'onde touche jusqu'à 1,0 m au-dessus du sol, un saut simple l'esquive (`SLAM_REACH_UP` = 1,0). |
| 5 | Éventail : 6° d'angle total ou demi-angle ? | **Option C** : 4,5° de chaque côté (9° au total). 56 / 37 / 8 PvP à 5 / 10 / 20 m (fiche 06). |
| 6 | Poings : plafond de recharge par combo ? | **Oui** : 50 points au plus par combo. |
| 7 | Ventouse : détoner tout ou la plus ancienne ? | **Deux commandes séparées** : « tout faire exploser » et « faire exploser la plus ancienne » (interprétation à confirmer, fiche 09). |
| — | Élan : deux tirs en course tuent en 0,5 s | **Pas de changement.** |
| — | Scie : lancer puis renvoi = 100,8 PvP | **Renvoi ×0,9** : le combo tombe à ~95 et ne tue plus d'un coup (fiche 08). Deux renvois réussis d'affilée peuvent tuer (105,8) : **gardé**, c'est technique. |

**Munitions : système de jauges** (remplace « +1 munition par coup » ; **aucune recharge passive**) :

- Chaque arme à distance a une jauge de points. Jauge pleine = +1 munition. Le rail a une jauge de **250 points** et **2 munitions** ; les roquettes une jauge de **50 points**, mais plus de munitions (8).
- La mêlée donne des points, montant propre à chaque arme de mêlée (épée 50 par coup qui touche). Les cellules ramassables donnent **100 ou 150 points**.
- Tableau complet : `docs/IDEES_ARMES_CARTES.md` § 1.5, et fiche 00.

Les fiches 00, 04, 06, 07, 08, 09 et 10 sont mises à jour avec ces décisions.
