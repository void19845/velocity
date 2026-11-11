# Classification des 140 étapes de la boutique v2

Premier livrable du refactor « armes objet » (voir `ARCHITECTURE_ARMES.md`) : pour chacune des 140 étapes des 7 graines jouables
(RAIL, ROQ, SHO, LAN, IMP, PIE, AIO), sa **nature** (stat, stat conditionnelle, événement, comportement) et, pour une stat, la **stat cible et
l'opération** du sac de stats (`src/solo/boutique_stats.c`). C'est la feuille de route de la migration des autres familles : chaque ligne a
été établie en lisant le code qui lit aujourd'hui l'étape (fichier et fonction cités dans « Remarque »), pas d'après le texte seul.

**État** : les 26 étapes marquées ✔ sont migrées (les quatre genres génériques `BK_DMG` / `BK_NEAR` / `BK_VOL` / `BK_PTS`, soit 15 étapes, et les
14 étapes-stat du Shotgun, dont 3 sont aussi génériques). Les autres restent dans leur famille (`BK_RAIL`, `BK_ROQ`, `BK_MEL_*`, `BK_COMP`, `BK_FX`) ;
il reste **56 étapes-stat à migrer** : Railgun 9, Roquettes 10 (plus la pénalité de vitesse de Roquette lourde), Lance 11, Impulsion 12, Pièce 8, All in One 6.

**Le socle est prêt pour ces 56 étapes** (préparation faite) : les 19 stats nommées, les opérations `to` et `min`, les conditions `direct` / `bounce` /
`first` / `salvefull`, les phases d'application, les arrondis `st_int` et le groupe jugé au moment du calcul existent déjà dans `src/monde/entites.c`,
`src/solo/boutique_stats.c` et le générateur, avec un test unitaire chacun (lignes « sac de stats » de `vagues_tests.c`). Les entrées de données
(§5) ont été vérifiées de bout en bout (générateur, table `shMods`, valeurs lues = valeurs des accesseurs actuels). **Une famille n'a donc plus qu'à
coller son bloc de données et à brancher son code sur `st()` : aucun fichier partagé à modifier (§6).**

## 1. Bilan

| Graine | stat | stat cond. | événement | comportement | stats (total) | év. + comport. | estimation du cahier des charges |
|---|---|---|---|---|---|---|---|
| RAIL | 7 | 3 | 8 | 2 | **10** | 10 | 13 / 7 |
| ROQ | 11 | 1 | 6 | 2 | **12** | 8 | 16 / 4 |
| SHO | 12 | 2 | 6 | 0 | **14** | 6 | 15 / 5 |
| LAN | 14 | 1 | 2 | 3 | **15** | 5 | 19 / 1 |
| IMP | 14 | 2 | 2 | 2 | **16** | 4 | 19 / 1 |
| PIE | 6 | 2 | 7 | 5 | **8** | 12 | 11 / 9 |
| AIO | 6 | 1 | 11 | 2 | **7** | 13 | 8 / 12 |
| **Total** | 70 | 12 | 42 | 16 | **82** | **58** | 100 / 40 |

Lecture : **82 étapes sont des stats** (70 simples, 12 conditionnelles) et **58 sont des événements (42) ou des comportements (16)**. L'estimation du
cahier des charges (100 / 40) était trop optimiste pour chaque graine, de 1 à 4 étapes (Roquettes et Lance -4, Railgun, Pièce et Impulsion -3) : beaucoup
d'étapes de profondeur 2 à 4 ont un déclencheur (éclats, kill, minuteur), même en mêlée, et les deux compétences ont 25 étapes qui ne sont pas des stats.
Les étapes de soin de profondeur 3 (Sang-froid, Cratère vital, Dividende, Dernier souffle, Armure de fer, Souffle acrobate, Peau de plomb) sont toutes des
événements : le soin passe par `vg_heal`.

Étapes **à état** (elles gardent un compteur ou un minuteur, donc un crochet d'événement : le sac ne porte que des valeurs) : Joute en chaîne,
Ruée, Tournoi, Poings infatigables, Frénésie (Lance et Impulsion), Cible unique (All in One). Rail tendu et Calibre lourd ne sont PAS à état : le
nombre de cibles déjà traversées est une propriété du tir en cours, passée en contexte (`StatCtx.pierced`).

## 2. Légende

**Types**
- **stat** : toute l'étape se résume à « cette valeur change » (facteur, ajout, plancher, plafond, cible). Aucune logique : le code de l'arme lit `st(graine, stat, base)`.
- **stat cond.** : une stat dont la valeur dépend d'une propriété de la touche ou du tir (distance, tête, en l'air, élite, cibles traversées, impact direct,
  rebond n°N, salve pleine) : le code lit `st_ctx(graine, stat, base, &contexte)`.
- **événement** : se déclenche sur un événement (tir, touche, kill, parry, joute, compétence utilisée, fin de salve) et fait quelque chose (soin, souffle,
  éclats, salve gratuite, explosion) : reste du code, pour le bus d'événements (phase C). Son paramètre numérique (« 45 dégâts », « 3 m ») reste dans le
  code et se multiplie par l'échelle du niveau comme avant.
- **comportement** : change un mécanisme (traverser un mur, minuteur sans recharge, orbite d'une pièce, deuxième salve) : reste du code.

**Colonnes** : **#** = rang de l'étape dans le pool de la graine (= bit de `shOwn`, `1u << #`, et `step` de `shMods`) ; **Axe** = axe du design du
propriétaire (`arbre_v2.json`, `axes.js`) ; « — » pour Pièce et All in One, dont les pools n'ont pas de liste d'axes ; **Niv.** = l'étape monte de niveau
(+20 % par niveau), règle de `sh_scalable` ; **Stat / op** = `STAT` + opération + valeur au niveau 1 (+ `cond`, groupe, phase, `fixed`) dans le format de
`effets/*.js` ; ✔ = migrée. Toute stat, opération ou condition citée dans une ligne non migrée **existe déjà** (catalogue ci-dessous, §4).

**Catalogue des stats** (`enum STAT_*`, `src/monde/entites.c` ; la **base** est ce que passe le code qui lit : le `#define` du jeu sans boutique, jamais un neutre
arbitraire). 38 stats : 8 + 5 axes du design, 6 stats nommées du Shotgun et des points de jauge, 19 créées pour les familles restantes (15 prévues + 4 canaux : `DEGC`, `DIRECT`, `DEGS`, `ANGLE`).

| Stat | Sens | Base, unité | Lue par | Étapes |
|---|---|---|---|---|
| `DEG` | multiplicateur de dégâts d'une touche | 1, facteur | `vg_dmg_mul` (`st_ctx`, une fois par touche) | Calibre, Charge creuse, Salve lourde, Poing de plomb, Bout portant, Plomb lourd, Roquette lourde ✔ |
| `TAI` | taille du projectile / zone de touche | 1, facteur | `sg_stats` ; `vg_rail_wide` ; `roq_setup` | Gros plomb ✔, Rail large, Rail colossal, Ogive large |
| `VIT` | vitesse | 1, facteur | `roq_setup` ; `vgm_vc` | Propulseur, Roquette lourde (x0,90), Vélocité |
| `ZON` | zone d'effet : rayon du souffle | m, base `ROCKET_RADIUS` | `roq_radius` | Rayon, Grand souffle |
| `POR` | portée | m (Shotgun : portée du tir ; Roquettes : mètres de carburant ; Pièce : portée de recherche) | `sg_stats` ; `roq_setup` ; `cp_seek_range` | Pompe tendue ✔, Combustible, Pièce lestée |
| `MUN` | munitions maximales, charges | entier (`st_int`) | `roq_max` ; `cp_max` ; `cp_allin_extra` | Soute, Réserve (Pièce), Réserve (All in One) |
| `NBP` | nombre de projectiles, tirs en plus | entier en plus | `sg_stats` ; `cp_coin_dirs` ; `cp_allin_extra` | Plombs en plus ✔, Pluie, Salve complète |
| `SUI` | suivi : courbure vers la cible | degrés (Rail, Shotgun) ; Roquettes : taux de virage par seconde | `sg_seek` ; `vg_rail_bend` ; `roq_tick_proj` | Plombs chercheurs / traqueurs ✔, Visée courbe, Tir guidé, Trajectoire courbe |
| `CD` | délai, recharge | facteur (Shotgun) ; s (Pièce, All in One : `skillDef[].recharge`) | `sg_stats` ; `cp_recharge` | Pompe vive ✔, Recharge (Pièce), Rechargement (All in One) |
| `ANI` | durée de l'animation d'un estoc | s, base 0,2 | `vgm_estoc_anim` | Lance vive, Lance-éclair |
| `COM` | longueur de combo, estocs à la suite | entier (`st_int`) | `vgm_estoc_hit` ; `vgm_fist_n` | Second estoc, Estocs en chaîne, 4e / 5e / 6e coup, Huit coups |
| `MUR` | munition rendue : points de jauge par coup | points | `vgm_estoc_pts` ; `vgm_fist_hit` | Moisson ✔, Cueillette ✔, Prise aérienne |
| `VOL` | vol de vie | fraction des dégâts (0,02 = 2 %) | `vg_lifesteal` | les 6 étapes de vol de vie ✔ |
| `CONE` | demi-angle du cône | radians | `sg_stats` | Gerbe large ✔, Cône serré ✔ |
| `PERTE` | part de la perte de dégâts à distance qui reste | 1, facteur | `sg_stats` | Chevrotine ✔ |
| `PNEAR` / `PFAR` | distance de dégâts pleins / où la perte est maximale | m | `sg_stats` | Cône serré ✔, Pompe tendue ✔ |
| `COUT` | coût de jauge d'un tir | 1, facteur | `sg_stats`, `vgsg_refund` | Double canon ✔ |
| `PTK` | points de jauge par kill | points, facteur | `ammo_points` (`src/combat/loadout.c`) | Rechargement ✔, Réserve (Railgun) |
| `TETE` | hauteur ajoutée à la zone de tête du rail | fraction (0,4 = zone 40 % plus haute), base 0 | `vg_rail_head` | Lunette, Rail colossal |
| `DEGC` | dégâts du rail PAR CIBLE touchée (canal propre, pas `DEG`) | 1, facteur ; contexte `pierced`, `elite` | `vg_rail_mul(i)` | Rail tendu, Calibre lourd, Tir d'élite |
| `BORD` | part des dégâts au bord du souffle | 0,30 ; borne 0,95 dans les données (`min`) | `roq_explode` (`roqEdge`) | Souffle plein |
| `CENTRE` | bonus de dégâts au centre du souffle | 0 | `roq_explode` (`roqCore`) | Cœur du souffle |
| `DIRECT` | part des dégâts bruts du souffle en plus sur la cible de l'impact direct (2e coup) | 0 ; contexte `direct` | `roq_explode` | Impact direct |
| `CDJ` | recharge après une joute réussie | s, base `LANCE_HIT_CD` | `vgm_joust_hit` | Recharge, Lance-éclair |
| `CDE` | recharge d'un estoc qui touche | s, base `LANCE_THRUST_CD` | `vgm_thrust_cd` | Estoc vif, Lance-éclair |
| `CDV` | recharge d'un estoc dans le vide | s (deux bases : 0,9 et 0,5 lance couchée) | `vgm_whiff_cd` | Relève |
| `TCH` | temps de charge de la lance couchée | s, base `LANCE_TAP` (0,15) | `vgm_tap` | Charge éclair |
| `MURJ` | points de jauge par joute réussie | points ; contexte `elite` | `vgm_joust_hit` | Butin de joute, Pillage d'élite |
| `CDC` | recharge de fin de combo | s, base `FIST_END_CD` (0,70) | `vgm_fist_end` | Cadence, Combo enchaîné |
| `ECA` | écart entre deux coups | s, base `FIST_GAP` (0,22) ; contexte `air` | `vgm_fist_gap` | Poings vifs, Jambes vives, Rafale |
| `CDW` | recharge après un combo sans touche | s, base `FIST_WHIFF_CD` (0,60) | `vgm_fist_whiff` | Souffle court |
| `MURC` | points de jauge par combo complet | points | `vgm_fist_full` | Butin de combo |
| `RIC` | multiplicateur de ricochet d'une pièce | base `COIN_MULT` (1,5) ; contexte `bounce` | `cp_coin_mult` | Pièce polie, Prime de ricochet, Ricochet pur |
| `DUREE` | durée de vie de la pièce lancée | s, base 4 | `cp_coin_life` | Pièce volante |
| `CDT` | délai entre deux tirs de la salve | s, base `wpCd[w]` | `cp_salve_cd` | Cadence (All in One) |
| `DEGS` | dégâts de la salve All in One (canal propre, pas `DEG`) | 1, facteur ; contexte `full` | `cp_dmg_mul` | Salve complète |
| `ANGLE` | demi-angle de la salve All in One | degrés (pas de `unit: 'deg'` : le code travaille en degrés) | `cp_half_deg` | Salve large, Salve précise |

**Stat nommée, canal ou condition ?** Une même idée peut se lire à plusieurs endroits du code : si ce sont des chemins DIFFÉRENTS (la recharge après une
joute, celle de l'estoc, celle d'un estoc dans le vide), on crée une **stat nommée par canal** (`CDJ`, `CDE`, `CDV`) : chaque chemin lit la sienne. Une
**condition** ne sert que pour une propriété de la touche ou du tir lue au MÊME endroit (distance, tête, en l'air, élite, cibles traversées). C'est
pourquoi « joute réussie », « combo complet » ou « kill » sont des canaux (`MURJ`, `MURC`) et non des conditions, et pourquoi `DEGC` (dégâts du rail
par cible) et `DEGS` (dégâts de la salve) sont des stats à part : `vg_dmg_mul` lit `DEG` à chaque touche, `DEGC` / `DEGS` sont lues par UN seul endroit.

## 3. Les 140 étapes

### RAIL — Railgun (distance)

Code d'effets actuel : src/solo/boutique_rail.c.

| # | Prof. | Étape | Axe | Type | Stat / op | Niv. | Remarque |
|---|---|---|---|---|---|---|---|
| 0 | 1 | Calibre | DEG | stat | ✔ `DEG` mul 1,10 | oui | lu par `vg_dmg_mul` |
| 1 | 1 | Réserve | MUN | stat | `PTK` mul 1,20 | oui | `vg_rail_gauge_mul` dans `ammo_points` (`src/combat/loadout.c`) : remplacer l'appel par `pts = st(SH_RAIL, STAT_PTK, pts)` (meme stat que Rechargement du Shotgun) |
| 2 | 1 | Rail large | TAI | stat | `TAI` mul 1,15, groupe 1 | oui | `vg_rail_wide` : `st(SH_RAIL, STAT_TAI, 1.0f)` ; Rail colossal REMPLACE Rail large (le plus fort l'emporte, ils ne se composent pas). Lu dans la boucle des ennemis de `hitscan_seg` : le sortir de la boucle (une lecture par segment) |
| 3 | 1 | Rail tendu | POR | stat cond. | `DEGC` mull 1,05, cond `pierced` par 5 | oui | `vg_rail_mul(i)` : `st_ctx(SH_RAIL, STAT_DEGC, 1.0f, &c)` avec `c.pierced = rlRank[i]` ; +5 % PAR cible deja traversee, plafond 5, LINEAIRE. Canal `DEGC` et non `DEG` : `vg_dmg_mul` lirait sinon Calibre (deja compte) a chaque lecture et Tir d'elite sur les souffles du rail |
| 4 | 1 | Visée courbe | SUI | stat | `SUI` add 1 (degres), groupe 1 | oui | `vg_rail_bend` : `deg = st(SH_RAIL, STAT_SUI, 0)` ; Tir guide remplace Visee courbe (le meilleur l'emporte) |
| 5 | 2 | Éclat d’impact | ZON | événement | - | oui | touche de la 1re cible traversee (`rlRank == 0`) : souffle 1,5 m a 30 % x echelle (`vg_rail_hit`) ; garder en evenement (phase C) |
| 6 | 2 | Satellite | NBP | événement | - | non | tir : lache un eclat chercheur (FX_SATELLITE, `vg_on_fire`) ; pas de niveau |
| 7 | 2 | Fourche | SUI | événement | - | oui | kill : relance le coup (FX_FOURCHE, `fx_on_kill`) ; degats x echelle |
| 8 | 2 | Surtension | DEG | comportement | - | non | une touche a la tete avance la chaine de tirs de +3 au lieu de +2 (`vg_rail_chain`) ; entier fixe, pas de niveau |
| 9 | 2 | Lunette | TAI | stat | `TETE` add 0,15, groupe 1 | oui | `vg_rail_head` : `hw = st(SH_RAIL, STAT_TETE, 0)` (fraction ajoutee a la hauteur de la zone de tete) ; Rail colossal la remplace (groupe) |
| 10 | 3 | Sang-froid | SOI | événement | - | oui | touche a la tete : `vg_heal(6 x echelle)` (`vg_rail_hit`) |
| 11 | 3 | Doublé | MUN | événement | - | non | tir : 2 kills avec le meme tir rendent 1 munition (`vg_rail_after`) ; pas de niveau |
| 12 | 3 | Calibre lourd | DEG | stat cond. | `DEGC` mul 1,15, cond `pierced` | oui | `vg_rail_mul(i)` : `(1 + 0,15 x echelle)^rang`, compose, sans plafond (pas de `par`) |
| 13 | 3 | Éclats | NBP | événement | - | oui | kill : 3 eclats chercheurs (FX_ECLATS) |
| 14 | 3 | Tir explosif | ZON | événement | - | oui | touche a la tete : souffle 3 m a 50 % x echelle (`vg_rail_hit`) |
| 15 | 4 | Rail jumeau | NBP | événement | - | oui | tir : 20 % x echelle de tirer un 2e rail (`rl_extra`, `vg_rail_after`) ; probabilite |
| 16 | 4 | Rail colossal | TAI | stat | `TAI` mul 1,40 (groupe 1) + `TETE` add 0,40 (groupe 1) | oui | une etape, deux stats : largeur du tir ET zone de tete ; remplace Rail large et Lunette (groupes) |
| 17 | 4 | Tir d’élite | DEG | stat cond. | `DEGC` mul 1,50, cond `elite` | oui | `vg_rail_mul(i)` : `c.elite = E[i].elite` ; canal `DEGC` (voir Rail tendu) |
| 18 | 4 | Perce-mur | POR | comportement | - | oui | le tir traverse un mur de moins de 1 m x echelle et repart a 70 % x echelle (100 % max) (`vg_rail_wall`) ; deux parametres numeriques mais c'est la capacite qui compte |
| 19 | 4 | Tir guidé | SUI | stat | `SUI` add 8 (degres), groupe 1 | oui | `vg_rail_bend` |

### ROQ — Roquettes (distance)

Code d'effets actuel : src/solo/boutique_roquettes.c.

| # | Prof. | Étape | Axe | Type | Stat / op | Niv. | Remarque |
|---|---|---|---|---|---|---|---|
| 0 | 1 | Charge creuse | DEG | stat | ✔ `DEG` mul 1,10 | oui | lu par `vg_dmg_mul` |
| 1 | 1 | Soute | MUN | stat | `MUN` add 1, fixed | non | `roq_max` : `return st_int(SH_ROQ, STAT_MUN, base)` apres le test du capstone MINI-NUKE (qui renvoie 1) ; entier fixe, pas de niveau |
| 2 | 1 | Propulseur | VIT | stat | `VIT` mul 1,10 | oui | `roq_setup` : `m = st(SH_ROQ, STAT_VIT, 1.0f)` puis le x2 du capstone ROQUETTE MONTEE reste en code |
| 3 | 1 | Rayon | ZON | stat | `ZON` add 0,4 (m) | oui | `roq_radius` : `r = st(SH_ROQ, STAT_ZON, ROCKET_RADIUS)` puis x0,55 (mini-roquette) ou 14 m (MINI-NUKE) en code |
| 4 | 1 | Ogive large | TAI | stat | `TAI` mul 1,20 | oui | `roq_setup` : `p->roqSize = st(SH_ROQ, STAT_TAI, 1.0f)` si > 1 ; lu par `roq_hitpad` (collision seulement, pas le visuel) |
| 5 | 2 | Combustible | POR | stat | `POR` add 20 (m) | oui | `roq_setup` : `p->life += st(SH_ROQ, STAT_POR, 0) / fmaxf(p->roqSpd, 1.0f)` (metres convertis en duree) |
| 6 | 2 | Fusée de proximité | SUI | comportement | - | oui | `roq_prox` : la roquette explose a 2 m x echelle d'un ennemi ; capacite (le rayon pourrait devenir une stat nommee) |
| 7 | 2 | Double départ | NBP | événement | - | oui | tir : 10 % x echelle de tirer 2 roquettes (`roq_fire`) ; probabilite |
| 8 | 2 | Souffle plein | ZON | stat | `BORD` add 0,15 + `BORD` min 0,95 (fixed) | oui | `roq_explode` : `roqEdge = st(SH_ROQ, STAT_BORD, 0.3f)` (base 0,30) ; le plafond 0,95 est dans les donnees (op `min`, valeur fixe) |
| 9 | 2 | Impact direct | DEG | stat cond. | `DIRECT` add 0,15, cond `direct` | oui | `roq_explode` : le 2e `damage_enemy(hitEnemy, dmg * st_ctx(SH_ROQ, STAT_DIRECT, 0, &c), ...)` avec `c.direct = 1` ; reste un DEUXIEME coup calcule sur les degats bruts du souffle (pas un `mul` de `DEG` : le resultat differerait hors du centre) |
| 10 | 3 | Cratère vital | SOI | événement | - | oui | explosion : `vg_heal(6 x echelle x min(3, touches))` (`roq_explode`) |
| 11 | 3 | Récupération | MUN | événement | - | non | kill avec 2 ennemis dans le souffle : +1 munition (`roq_explode`) ; entier fixe, pas de niveau |
| 12 | 3 | Éclats | NBP | événement | - | oui | kill : 3 eclats chercheurs (FX_ECLATS) |
| 13 | 3 | Grand souffle | ZON | stat | `ZON` add 0,8 (m) | oui | `roq_radius` |
| 14 | 3 | Cœur du souffle | DEG | stat | `CENTRE` add 0,25 | oui | `roq_explode` : `roqCore = st(SH_ROQ, STAT_CENTRE, 0)` (bonus de degats au centre du souffle) |
| 15 | 4 | Détonation en chaîne | ZON | événement | - | oui | kill : explosion 3,5 m, 45 degats (FX_ONDE) |
| 16 | 4 | Salve | NBP | comportement | - | oui | tir : une 2e roquette a 60 % x echelle (100 % max), clone du projectile (`roq_fire`) ; la roquette en plus est un comportement, pas un simple nombre |
| 17 | 4 | Trajectoire courbe | SUI | stat | `SUI` add 3 (taux de virage par seconde) | oui | `roq_tick_proj` : `turn = st(SH_ROQ, STAT_SUI, 0)`, suivi actif si > 0, fraction d'interpolation `fminf(1, turn * dt)` ; ciblage (14 m, cone `cos > 0,3`) en code. Une lecture par tick suffit (peu de roquettes, `st` est un cache) : ne pas ajouter de champ a `Proj` (fichier partage) |
| 18 | 4 | Grappe | NBP | événement | - | oui | explosion : 3 mini-roquettes a 30 % x echelle (`roq_explode`) |
| 19 | 4 | Roquette lourde | DEG | stat | ✔ `DEG` mul 1,40 + `VIT` mul 0,90 | oui | DEG deja migre ; la penalite de vitesse (-10 % x echelle, elle monte avec le niveau) devient un 2e modificateur `VIT` (supprimer `roq_has(RQ_LOURDE)` de `roq_setup`) |

### SHO — Shotgun (distance)

Code d'effets actuel : src/armes/shotgun.c, src/solo/boutique_shotgun.c.

| # | Prof. | Étape | Axe | Type | Stat / op | Niv. | Remarque |
|---|---|---|---|---|---|---|---|
| 0 | 1 | Poing de plomb | DEG | stat cond. | ✔ `DEG` mul 1,10, cond `near` 5 m | oui | lu par `vg_dmg_mul` (distance oeil - cible, strictement sous 5 m) |
| 1 | 1 | Chevrotine | POR | stat | ✔ `PERTE` mul 0,90 | oui | `sg_stats` : `fallMin = 1 - (1 - SG_FAR_MULT) x PERTE` ; la perte a distance baisse de 10 % (penalite qui monte aussi) |
| 2 | 1 | Pompe vive | CD | stat | ✔ `CD` mul 0,85 | oui | `fire_shotgun` : delai entre deux salves |
| 3 | 1 | Gros plomb | TAI | stat | ✔ `TAI` mul 1,15 | oui | `fire_shotgun` : rayon de touche des plombs ; aussi `cp_coin_pellets` |
| 4 | 1 | Rechargement | MUN | stat | ✔ `PTK` mul 1,20 | oui | `ammo_points` (`src/combat/loadout.c`) : points de jauge par kill |
| 5 | 2 | Gerbe large | ZON | stat | ✔ `CONE` add 1,5 deg | oui | `sg_stats` : demi-angle du cone (rad) |
| 6 | 2 | Rafale | NBP | événement | - | oui | tir : 2e salve gratuite a 50 % apres 0,15 s (`vgsg_after`, `vgsg_tick`) ; reste en `BK_SHO` (`SHE_RAFALE`) |
| 7 | 2 | Plombs chercheurs | SUI | stat | ✔ `SUI` add 3 (deg), groupe 1 | oui | `sg_seek` ; Plombs traqueurs remplace Plombs chercheurs (le meilleur l'emporte) |
| 8 | 2 | Bout portant | DEG | stat cond. | ✔ `DEG` mul 1,25, cond `near` 3 m | oui | lu par `vg_dmg_mul` |
| 9 | 2 | Cône serré | POR | stat | ✔ `CONE` add -1 deg + `PNEAR` add 5 + `PFAR` add 5 | oui | une etape, trois modificateurs (cone, courbe de perte) ; le cone ne descend pas sous 0,01 rad |
| 10 | 3 | Peau de plomb | SOI | événement | - | oui | tir : `vg_heal(8 x echelle)` si 6 plombs ou plus touchent (`vgsg_after`) ; reste en `BK_SHO` (`SHE_PEAU`) |
| 11 | 3 | Plombs en plus | NBP | stat | ✔ `NBP` add 2 | oui | `sg_stats` : `(int)(x + 0,5)`, au plus `SG_PMAX - SG_PELLETS` en plus ; aussi les formes fusionnees |
| 12 | 3 | Double canon | MUN | stat | ✔ `COUT` mul 0,85 | oui | `vgsg_refund` : la part du cout de jauge qui manque est rendue (garde du capstone SHO>AIO conservee) |
| 13 | 3 | Souffle de culasse | ZON | événement | - | oui | tir : onde derriere soi (3 m, 40 x echelle) quand le recul pousse (`vgsg_after`) ; reste en `BK_SHO` (`SHE_CULASSE`) |
| 14 | 3 | Éclats | SUI | événement | - | oui | kill : 3 eclats chercheurs (FX_ECLATS) |
| 15 | 4 | Coup double | NBP | événement | - | oui | kill : 2e salve gratuite a 50 % apres 0,12 s (`vgsg_after`) ; reste en `BK_SHO` (`SHE_COUP2`) |
| 16 | 4 | Plomb lourd | DEG | stat | ✔ `DEG` mul 1,30 | oui | lu par `vg_dmg_mul` ; aussi `hy_tree_dmg` (formes fusionnees) |
| 17 | 4 | Détonation | ZON | événement | - | oui | kill : explosion 3,5 m, 45 degats (FX_ONDE) |
| 18 | 4 | Pompe tendue | POR | stat | ✔ `POR` add 20 + `PNEAR` max 20 + `PFAR` add 20 | oui | portee, plancher de la distance de degats pleins (aucune perte avant 20 m x echelle) et fin de la courbe ; le `max` s'applique apres les `add` (Cone serre : 15 puis 20) |
| 19 | 4 | Plombs traqueurs | SUI | stat | ✔ `SUI` add 8 (deg), groupe 1 | oui | `sg_seek` |

### LAN — Lance (melee)

Code d'effets actuel : src/solo/boutique_melee.c, src/armes/lance.c.

| # | Prof. | Étape | Axe | Type | Stat / op | Niv. | Remarque |
|---|---|---|---|---|---|---|---|
| 0 | 1 | Recharge | CD | stat | `CDJ` add -0,2 (s) | oui | `vgm_joust_hit` : `cd = st(SH_LAN, STAT_CDJ, LANCE_HIT_CD)` = (1,0 - 0,2 x echelle) x mul de Lance-eclair ; Joute en chaine, Ruee et `fmaxf(0, cd)` restent en code |
| 1 | 1 | Vélocité | ANI | stat | `VIT` mul 1,10 | oui | `vgm_vc(vc)` : `st(SH_LAN, STAT_VIT, vc)` (vitesse de rapprochement ; axe du design : ANI) |
| 2 | 1 | Second estoc | COM | stat | `COM` max 1, fixed | non | `vgm_estoc_hit` : `n = st_int(SH_LAN, STAT_COM, 0)` (nombre d'estocs a la suite, le plus grand l'emporte) puis le `max` avec le capstone LAN>AIO reste en code ; entier fixe |
| 3 | 1 | Moisson | MUR | stat | ✔ `MUR` add 10 | oui | `vgm_estoc_pts` (canal estoc) |
| 4 | 1 | Sève de lance | VOL | stat | ✔ `VOL` add 0,02 | oui | lu par `vg_lifesteal` : les trois etapes de vol de vie de la graine s'ADDITIONNENT |
| 5 | 2 | Estoc vif | CD | stat | `CDE` mul 0,85 | oui | `vgm_thrust_cd(cd)` : `st(SH_LAN, STAT_CDE, cd)` ; se compose avec Lance-eclair (mul x mul) |
| 6 | 2 | Lance vive | ANI | stat | `ANI` mul 0,80 | oui | `vgm_estoc_anim(t)` : `st(SH_LAN, STAT_ANI, t)` ; se compose avec Lance-eclair |
| 7 | 2 | Estocs en chaîne | COM | stat | `COM` max 2, fixed | non | `vgm_estoc_hit` (voir Second estoc) |
| 8 | 2 | Butin de joute | MUR | stat | `MURJ` add 25 | oui | `vgm_joust_hit` : `pts = st_ctx(SH_LAN, STAT_MURJ, 0, &c)` avec `c.elite = E[i].elite` (canal joute : stat nommee, `MUR` reste le canal estoc) |
| 9 | 2 | Sang de cavalier | VOL | stat | ✔ `VOL` add 0,04 | oui | `vg_lifesteal` |
| 10 | 3 | Armure de fer | VOL | événement | - | oui | joute reussie : `vg_heal(10 x echelle)` (`vgm_joust_hit`) |
| 11 | 3 | Relève | CD | stat | `CDV` to 0,25 (s) | oui | `vgm_whiff_cd(cd)` : `st(SH_LAN, STAT_CDV, cd)` ; deux bases (0,9 s estoc dans le vide, 0,5 s lance couchee) : l'op `to` reproduit `vgm_lower` (base - (base - 0,25) x echelle) |
| 12 | 3 | Charge éclair | ANI | stat | `TCH` to 0,10 (s) | oui | `vgm_tap(t)` : `st(SH_LAN, STAT_TCH, t)` (base `LANCE_TAP` 0,15) ; meme op `to` que Releve |
| 13 | 3 | Joute en chaîne | COM | comportement | - | non | une joute sur deux sans recharge (`vgmChainUsed`) ; etat, pas de niveau |
| 14 | 3 | Pillage d’élite | MUR | stat cond. | `MURJ` add 40, cond `elite` | oui | `vgm_joust_hit` (meme lecture que Butin de joute) : +40 points si la cible de la joute est une elite |
| 15 | 4 | Ruée | CD | comportement | - | oui | aucune recharge pendant 2 s x echelle apres une joute reussie (`vgmRueeT`) ; minuteur a etat |
| 16 | 4 | Lance-éclair | ANI | stat | `ANI` mul 0,65 + `CDE` mul 0,65 + `CDJ` mul 0,65 | oui | estoc ET joute 35 % plus rapides : trois stats lues dans `vgm_estoc_anim`, `vgm_thrust_cd`, `vgm_joust_hit` |
| 17 | 4 | Tournoi | COM | comportement | - | oui | `vgm_joust_mul` : +20 % par joute reussie d'affilee (60 % max), compteur `vgmStreak` ; etat ; la valeur pourrait etre `DEG` mul avec une cond « serie » |
| 18 | 4 | Trésor de guerre | MUR | événement | - | oui | kill de melee : `ammo_points(100 x echelle, AP_KILL)` (`vgm_kill`) |
| 19 | 4 | Lance vorace | VOL | stat | ✔ `VOL` add 0,10 | oui | `vg_lifesteal` |

### IMP — Impulsion (melee)

Code d'effets actuel : src/solo/boutique_melee.c, src/armes/poings.c.

| # | Prof. | Étape | Axe | Type | Stat / op | Niv. | Remarque |
|---|---|---|---|---|---|---|---|
| 0 | 1 | Cadence | CD | stat | `CDC` mul 0,90 (phase 0) | oui | `vgm_fist_end(cd)` : `st(SH_IMP, STAT_CDC, cd)` (base `FIST_END_CD` 0,70) ; `vgmFreeT > 0` -> 0 reste en code |
| 1 | 1 | Poings vifs | ANI | stat | `ECA` to 0,20 (s) | oui | `vgm_fist_gap(g)` : `g = st_ctx(SH_IMP, STAT_ECA, g, &c)` avec `c.air = !P.grounded` ; Poings vifs et Rafale : `to` garde le meilleur candidat (comme `vgm_lower`) ; la Frenesie (x par empilement) reste en code apres la lecture |
| 2 | 1 | Quatrième coup | COM | stat | `COM` max 4, fixed | non | `vgm_fist_n` : `n = st_int(SH_IMP, STAT_COM, 3)` puis le `max` avec le capstone reste en code ; entier fixe |
| 3 | 1 | Cueillette | MUR | stat | ✔ `MUR` add 10 | oui | `vgm_fist_hit` : points de jauge par coup qui touche |
| 4 | 1 | Souffle de poing | VOL | stat | ✔ `VOL` add 0,02 | oui | `vg_lifesteal` |
| 5 | 2 | Souffle court | CD | stat | `CDW` mul 0,50 | oui | `vgm_fist_whiff(cd)` : `st(SH_IMP, STAT_CDW, cd)` (base `FIST_WHIFF_CD` 0,60) ; canal combo rate = stat nommee |
| 6 | 2 | Jambes vives | ANI | stat cond. | `ECA` mul 0,80, cond `air`, phase 1 | oui | `vgm_fist_gap` (meme lecture que Poings vifs) : phase 1 = APRES le `to`, comme le code d'origine |
| 7 | 2 | Cinquième coup | COM | stat | `COM` max 5, fixed | non | `vgm_fist_n` |
| 8 | 2 | Prise aérienne | MUR | stat cond. | `MUR` add 15, cond `air` | oui | `vgm_fist_hit` : `pts = st_ctx(SH_IMP, STAT_MUR, 0, &c)` avec `c.air = !P.grounded` (Cueillette, deja migree, et Prise aerienne dans la meme lecture) |
| 9 | 2 | Sang vif | VOL | stat | ✔ `VOL` add 0,04 | oui | `vg_lifesteal` |
| 10 | 3 | Souffle acrobate | VOL | événement | - | oui | uppercut qui touche : `vg_heal(8 x echelle)` (`vgm_fist_hit`) |
| 11 | 3 | Combo enchaîné | CD | stat | `CDC` add -0,2 (s), phase 1 | oui | `vgm_fist_end` : la phase 1 passe APRES le x0,9 de Cadence (0,70 x 0,9 - 0,2 = 0,43 s, comme le code d'origine) ; le plancher `fmaxf(0, ...)` reste en code |
| 12 | 3 | Rafale | ANI | stat | `ECA` to 0,16 (s) | oui | `vgm_fist_gap` (voir Poings vifs) |
| 13 | 3 | Sixième coup | COM | stat | `COM` max 6, fixed | non | `vgm_fist_n` |
| 14 | 3 | Butin de combo | MUR | stat | `MURC` add 40 | oui | `vgm_fist_full(landed)` : `pts = st(SH_IMP, STAT_MURC, 0)` si le combo complet a touche |
| 15 | 4 | Poings infatigables | CD | comportement | - | oui | aucune recharge pendant 3 s x echelle apres un kill (`vgmFreeT`) ; minuteur a etat |
| 16 | 4 | Frénésie | ANI | comportement | - | oui | chaque combo complet : coups 10 % plus vifs, 5 empilements, 6 s (`vgmFrenStacks`, `vgmFrenT`) ; etat |
| 17 | 4 | Huit coups | COM | stat | `COM` max 8, fixed | non | `vgm_fist_n` + comportement : un uppercut tous les 4 coups (`vgm_fist_up`, reste en code) |
| 18 | 4 | Pillage de poings | MUR | événement | - | oui | kill de melee : `ammo_points(100 x echelle, AP_KILL)` (`vgm_kill`) |
| 19 | 4 | Acrobate vorace | VOL | stat | ✔ `VOL` add 0,10 | oui | `vg_lifesteal` |

### PIE — Piece (competence)

Code d'effets actuel : src/solo/boutique_competences.c.

| # | Prof. | Étape | Axe | Type | Stat / op | Niv. | Remarque |
|---|---|---|---|---|---|---|---|
| 0 | 1 | Pièce lestée | — | stat | `POR` add 15 (m) | oui | `cp_seek_range` : `st(SH_PIE, STAT_POR, COIN_SEEK_RANGE)` |
| 1 | 1 | Pièce polie | — | stat | `RIC` add 0,1, groupe 1 | oui | `cp_coin_mult(depth)` : `st_ctx(SH_PIE, STAT_RIC, COIN_MULT, &c)` avec `c.bounce = depth + 1` (1 = premier rebond) |
| 2 | 1 | Recharge | — | stat | `CD` mul 0,90 | oui | `cp_recharge(SK_COIN, base)` : `st(SH_PIE, STAT_CD, base)` ; garde-fou (plancher 0,1 x base) en code |
| 3 | 1 | Réserve | — | stat | `MUN` add 1, fixed | non | `cp_max(SK_COIN, base)` : `st_int(SH_PIE, STAT_MUN, base)` ; entier fixe |
| 4 | 1 | Pièce aimantée | — | comportement | - | non | passif : tes pieces derivent vers l'ennemi proche (FX_COIN_AIMANT, `vg_fx_tick`) ; pas de niveau |
| 5 | 2 | Pièce recyclée | — | événement | - | non | ricochet qui tue : +1 charge (`cp_ric_after`) ; pas de niveau |
| 6 | 2 | Éclats | — | événement | - | oui | kill : 3 eclats chercheurs (FX_ECLATS) |
| 7 | 2 | Pièce volante | — | stat | `DUREE` add 1 (s) | oui | `cp_coin_life` : `st(SH_PIE, STAT_DUREE, 4.0f)` ; l'orbite de Piece orbitale (`cp_sc(CP_ORBITALE) x 4`) reste en code |
| 8 | 2 | Rebond de plus | — | comportement | - | non | un ricochet touche une cible de plus (`cp_ric_after`) ; pas de niveau |
| 9 | 2 | Aimant doux | — | comportement | - | oui | passif : la piece derive vers l'ennemi, vitesse 12 x echelle (`cp_tick`) ; le parametre pourrait etre `SUI` |
| 10 | 3 | Dividende | — | événement | - | oui | ricochet qui touche 2 cibles : `vg_heal(10 x echelle)` (`cp_after_ricochet`) |
| 11 | 3 | Prime de ricochet | — | stat cond. | `RIC` add 0,5, cond `first`, groupe 1 | oui | `cp_coin_mult` : au 1er rebond le groupe 1 garde Prime (+0,5 x echelle) et ecarte Polie, soit exactement `COIN_MULT + (2 - COIN_MULT) x echelle` ; aux rebonds suivants Prime ne s'applique pas et Polie revient (le groupe se juge sur les modificateurs applicables) |
| 12 | 3 | Pièce paratonnerre | — | comportement | - | oui | passif : les orbes ennemies a 6 m x echelle vont vers tes pieces et s'y eteignent (`cp_tick`) |
| 13 | 3 | Détonation | — | événement | - | oui | kill : explosion 3,5 m, 45 degats (FX_ONDE) |
| 14 | 3 | Vortex | — | événement | - | oui | kill : vortex de 1,2 s (FX_VORTEX) |
| 15 | 4 | Fourche | — | événement | - | oui | kill : relance le coup (FX_FOURCHE) |
| 16 | 4 | Pluie | — | stat | `NBP` add 1, fixed | non | `cp_coin_dirs` : `st_int(SH_PIE, STAT_NBP, 1) >= 2` ; le capstone PIE>RAIL (8 pieces) passe avant, en code ; entier fixe |
| 17 | 4 | Pièce piégée | — | événement | - | oui | ricochet : la piece explose 3 m, 45 x echelle (`cp_coin_pop`) |
| 18 | 4 | Ricochet pur | DEG | stat cond. | `RIC` mul 1,25, cond `bounce` par 3 | oui | `cp_coin_mult` : a partir du 3e rebond, `(1 + 0,25 x echelle)^(rebond - 2)` = l'ancien `depth - 1` ; `c.bounce = depth + 1` |
| 19 | 4 | Pièce orbitale | — | comportement | - | oui | la piece orbite 4 s x echelle autour de toi puis file vers l'ennemi (`cp_coin_orbit`, `cp_coin_life`) |

### AIO — All in One (competence)

Code d'effets actuel : src/solo/boutique_competences.c, src/combat/competences.c.

| # | Prof. | Étape | Axe | Type | Stat / op | Niv. | Remarque |
|---|---|---|---|---|---|---|---|
| 0 | 1 | Salve lourde | — | stat | ✔ `DEG` mul 1,10 | oui | lu par `vg_dmg_mul` (chemin All in One via `fx_paths`) |
| 1 | 1 | Rechargement | — | stat | `CD` mul 0,85 | oui | `cp_recharge(SK_ALLIN, base)` : `st(SH_AIO, STAT_CD, base)` |
| 2 | 1 | Salve large | — | stat | `ANGLE` add 0,3 (degres) | oui | `cp_half_deg(base)` : `st(SH_AIO, STAT_ANGLE, base)` en DEGRES (pas `CONE`, en radians pour le Shotgun : pas de `unit: 'deg'`) |
| 3 | 1 | Cadence | — | stat | `CDT` mul 0,70 | oui | `cp_salve_cd(cd)` : `st(SH_AIO, STAT_CDT, cd)` ; garde-fou (plancher 0,1 x cd) en code |
| 4 | 1 | Réserve | — | stat | `MUN` add 1, fixed | non | `cp_allin_extra` : `x += st_int(SH_AIO, STAT_MUN, 0)` ; entier fixe |
| 5 | 2 | Pulsion | — | événement | - | oui | competence utilisee : onde de 6 m (FX_S_ONDE) |
| 6 | 2 | Spectre | — | événement | - | oui | kill : un spectre orbite 8 s (FX_SPECTRE) |
| 7 | 2 | Salve précise | — | stat | `ANGLE` mul 0,70 | oui | `cp_half_deg` : s'applique apres Salve large (`(base + 0,3) x 0,7`, ordre add puis mul du sac) |
| 8 | 2 | Rechargement jauge | — | événement | - | oui | fin de salve : rend 25 % x echelle de la jauge (`cp_salve_end`) |
| 9 | 2 | Prime de salve | — | événement | - | oui | kill pendant la salve : -2 s x echelle de recharge (`cp_on_kill`) |
| 10 | 3 | Dernier souffle | — | événement | - | oui | debut de salve avec chargeur plein : `vg_heal(15 x echelle)` (`cp_salve_begin`) |
| 11 | 3 | Éclats | — | événement | - | oui | kill : 3 eclats chercheurs (FX_ECLATS) |
| 12 | 3 | Fracture | — | événement | - | oui | touche : 3 touches en 2 s font exploser la cible (FX_FRACTURE) |
| 13 | 3 | Fourche | — | événement | - | oui | kill : relance le coup (FX_FOURCHE) |
| 14 | 3 | Vortex | — | événement | - | oui | kill : vortex de 1,2 s (FX_VORTEX) |
| 15 | 4 | Ressac | — | événement | - | oui | kill : repousse les ennemis a 9 m x echelle (FX_RESSAC) |
| 16 | 4 | Salve complète | — | stat cond. | `DEGS` mul 1,30 + `NBP` add 1 (fixed), cond `salvefull` | oui | `cp_dmg_mul` : `st_ctx(SH_AIO, STAT_DEGS, 1.0f, &c)` avec `c.full = cpSalveFull` (canal propre : la fenetre de 2 s de la salve, differente de celle de Salve lourde / `DEG`) ; `cp_allin_extra` : `x += st_int_ctx(SH_AIO, STAT_NBP, 0, &c)` avec `c.full = P.ammo[w] >= wp_max(w)` |
| 17 | 4 | Salve double | — | comportement | - | non | une 2e salve 0,3 s apres la premiere (`cpDblN`) ; pas de niveau |
| 18 | 4 | Cible unique | — | comportement | - | oui | `cp_dmg_mul` : x1,3 sur une cible deja touchee par la salve (`cpFocus` par ennemi) ; etat par cible |
| 19 | 4 | Détonation | — | événement | - | oui | kill : explosion 3,5 m (FX_ONDE) |

## 4. Ce que sait exprimer le sac, cas par cas

Tout ce que les 56 étapes demandent est déjà dans le sac. Voici comment les cas difficiles y sont exprimés (le détail des règles est dans
`ARCHITECTURE_ARMES.md`, §2) :

| Besoin du code actuel | Mécanisme du sac | Étapes |
|---|---|---|
| Une durée qui BAISSE jusqu'à une cible et que le niveau pousse AU-DELÀ (`vgm_lower` : `base - (base - cible) x échelle`), la base variant selon le contexte | opération **`to`** (la cible ; le meilleur candidat ; sans effet si la base est déjà sous la cible) | Relève (0,9 s dans le vide, 0,5 s lance couchée), Charge éclair, Poings vifs, Rafale |
| Un pourcentage PUIS un retrait en secondes (Impulsion : 0,70 x 0,9 - 0,2 = **0,43 s**, pas 0,63 x 0,9...) | **`phase: 1`** : les phases s'appliquent dans l'ordre ; dans une phase, `to`, puis `(valeur + add) x mul`, puis `max`, puis `min` | Cadence puis Combo enchaîné ; Poings vifs / Rafale puis Jambes vives (x0,8 en l'air) |
| Une étape qui REMPLACE une autre au premier rebond seulement (Prime de ricochet remplace Pièce polie au 1er rebond, Polie revient ensuite) | condition **`first`** + **groupe** : le groupe se juge au moment du calcul, parmi les modificateurs APPLICABLES, le plus fort gagne | Prime de ricochet / Pièce polie |
| Un effet qui se répète à chaque rebond à partir du N-ième, ou à chaque cible déjà traversée | conditions **`bounce`** (`par` = rebond de départ, 1 = premier ; une application par rebond) et **`pierced`** (`par` = plafond) ; `mull` additionne les deltas (linéaire), `mul` les compose | Ricochet pur ; Rail tendu (linéaire, plafond 5) ; Calibre lourd (composé) |
| Un plafond absolu, qui ne monte pas avec le niveau | opération **`min`** + `fixed: true` | Souffle plein (0,95) |
| Un plancher absolu (nombre de coups, d'estocs) | opération `max` + `fixed: true`, lue avec `st_int` | Second estoc, Estocs en chaîne, 4e / 5e / 6e coup, Huit coups |
| Un entier arrondi au plus proche | **`st_int(graine, stat, base)`** / `st_int_ctx` (`floorf(x + 0,5)`) | Soute, Réserves, Pluie, Salve complète (tir en plus), coups de combo |
| Deux paliers d'une même idée qui ne s'additionnent pas, mais ne se remplacent pas toujours | **groupe** (`grp`) : le plus fort l'emporte, à égalité le premier de la table | Rail large / colossal (TAI), Lunette / colossal (TETE), Visée courbe / Tir guidé (SUI), Plombs chercheurs / traqueurs |
| Une valeur dont la lecture dépend d'une propriété du coup (en l'air, élite, impact direct, salve pleine) | **conditions** `air`, `elite`, `direct`, `salvefull` : le code remplit `StatCtx` | Jambes vives, Prise aérienne, Pillage d'élite, Tir d'élite, Impact direct, Salve complète |
| Un entier qui ne monte pas de niveau | `fixed: true` : l'étape n'est alors pas proposée en amélioration (`sh_scalable`) | Soute, Second estoc, Estocs en chaîne, coups de combo, Réserves, Pluie, tir en plus de Salve complète, plafond de Souffle plein |

**Ce qui reste dans le code de l'arme** (le sac ne le connaît pas) : l'arrondi quand il n'est pas au plus proche, les **garde-fous** (`fmaxf(0, ...)`,
`fmaxf(0.1f, ...)`, plafond du nombre de plombs, bornes d'angle), les **probabilités** (Double départ, Rail jumeau), les **minuteurs et compteurs**
(Ruée, Frénésie, Tournoi, Cible unique), les **capstones et fusions** (`roq_cap`, `vgm_cap`, `cp_cap`) et tout événement (soin, souffle, éclats).

**Ordre d'application** : phases dans l'ordre (0, puis 1...) ; dans une phase, `to` (le meilleur candidat), puis `(valeur + somme des add) x produit des mul`,
puis les planchers (`max`), puis les plafonds (`min`). Sans phase, c'est le cas courant `(base + add) x mul`. Les endroits du code actuel qui
demandaient un ordre particulier y rentrent exactement : Impulsion `vgm_fist_end` (phase 1 : pourcentage puis secondes), `vgm_fist_gap` (`to` puis `mul` en phase 1), Pièce
`cp_coin_mult` (groupe jugé au calcul), All in One `cp_half_deg` (`(base + 0,3) x 0,7` : add puis mul, l'ordre naturel).

**Étapes à valeur fixe** (`fixed: true`) : Soute (`MUN` +1), Second estoc / Estocs en chaîne et les coups de combo (`COM` max), Réserve (Pièce et All in One,
`MUN` +1), Pluie (`NBP` +1), le `NBP` +1 de Salve complète, et le plafond 0,95 de Souffle plein (`BORD` min).

**Plusieurs modificateurs dans une étape** : Rail colossal (`TAI` + `TETE`), Souffle plein (`BORD` add + `BORD` min), Roquette lourde (`DEG` + `VIT`), Lance-éclair
(`ANI` + `CDE` + `CDJ`), Salve complète (`DEGS` + `NBP`), Cône serré et Pompe tendue du Shotgun (déjà migrés).

**Groupes** (`grp`, « le plus fort l'emporte ») : Rail large / Rail colossal (largeur), Lunette / Rail colossal (tête), Visée courbe / Tir guidé (`SUI`),
Pièce polie / Prime de ricochet (`RIC`), et Plombs chercheurs / traqueurs du Shotgun (déjà migrés). Poings vifs / Rafale n'ont pas de groupe : l'opération `to`
garde déjà le meilleur candidat.

## 5. Migration par famille : trois travaux indépendants

### 5.0 Règles communes

1. **Ne modifier aucun fichier partagé** : `src/monde/entites.c` (enums, types, déclarations avancées), `src/solo/boutique_stats.c`, `src/solo/ameliorations.c`,
   `docs/boutique_vagues/outils/export_c.js` et `effets_c.js`, `main.c`, les docs communes (`ARCHITECTURE_ARMES.md`, `CLASSIFICATION_ETAPES.md`,
   `docs/CODE.md`, `docs/NOUVEAUTES.md` : la mise à jour se fait d'après les rapports). Si le sac ne sait vraiment pas exprimer un cas : le dire dans le rapport.
2. **Ordre de travail** : (a) remplacer, dans `effets/<famille>.js`, les entrées `['BK_...', ...]` des étapes-stat par le bloc de données de la famille (ci-dessous) ;
   (b) `cd docs/boutique_vagues/outils && node export_c.js` (régénère `boutique_donnees.c` pour tout le monde : ne jamais l'éditer à la main ; il affiche `dont N etapes-stat, M modificateurs` : 82 et 92 quand
   les trois familles auront migré) ; (c) brancher le code de l'arme sur le sac, **une lecture par tir ou par touche** dans une variable locale (modèle `sg_stats`) ;
   (d) supprimer ce qui devient inutile ; (e) compiler, `--test`, lignes de test nouvelles.
3. **Supprimer l'ancienne lecture de chaque étape migrée**, sinon l'effet est perdu ou compté deux fois : un accesseur qui cherche l'étape par son genre
   (`rl_has`, `cp_has`, `vgm_max`, `sh_sum`) répond « absente » une fois l'étape devenue `BK_STAT` ; un accesseur qui la cherche par son rang (`roq_has(RQ_X)`)
   répond encore « présente » et s'ajouterait à la stat (Roquette lourde : retirer la ligne `roq_has(RQ_LOURDE)` de `roq_setup`).
4. **Les enums `RL_*`, `RQ_*`, `CP_*`, `BK_MEL_*` restent** (le rang d'une étape = son bit de `shOwn`, utilisé par les événements et les tests) et **les signatures
   publiques ne changent pas** (`roq_max`, `roq_radius`, `vg_rail_wide`, `vgm_fist_gap`, `cp_coin_mult`... déclarées dans `entites.c`) : `lance.c`, `poings.c`,
   `pieces_rail.c`, `projectiles.c`, `competences.c`, `boutique_hybrides.c` et les anciens tests n'ont pas à bouger.
5. **Les garde-fous restent dans le code** (`fmaxf(0, ...)`, plafonds, probabilités, minuteurs). Hors VAGUES, rien ne change : `st()` rend la base telle quelle,
   sans opération flottante ; les lectures restent derrière `vagOn` quand la fonction n'est appelée qu'en VAGUES.
6. **Vérification** : zéro warning (MSVC `/W3` ; sur Mac `clang -Wall` signale aussi une fonction `static` devenue inutilisée : la supprimer), `--test` comparé à
   la référence d'avant refactor (mêmes 5 lignes instables masquées : `bots (normal, 10 m) : tirs vises a la tete`, `visee bot niveau N a 20 m : cadres`,
   `respawn : Cible en`, `sentinelle : orbes`, `boutique rail : Perce-mur ... Rail jumeau`) **sans aucune ligne retirée ni modifiée**, plus les lignes nouvelles
   de la famille ; match bot contre bot. Sur l'arbre courant (11/11/2026), deux lignes de la référence sont modifiées volontairement par d'AUTRES travaux, hors boutique : `protocole : NET_VER`
   (21 -> 22, traction du Fléau) et `vagues : boss aux vagues 25/50/75/100` (les identifiants des types de boss glissent avec les ennemis BOMBE / CARRE / MORTIER) ; elles ne
   comptent pas, et les lignes du Fléau, de la bombe, du carré et du mortier sont des lignes ajoutées par ces travaux. Compiler sous un autre nom (`/Fe:vtest_x.exe /Fo:vtest_x.obj`), jamais `build.bat` (il écrase `velocity.exe`).
   Si deux compilations se lancent en même temps, `build.bat` peut échouer sur `%TEMP%\vspath.txt` (« cl n'est pas reconnu ») : relancer.
7. **Tests** : les blocs existants de `vagues_tests.c` restent la référence d'équivalence (ils lisent les mêmes fonctions publiques) : s'ils passent sans
   changement, la migration est fidèle. Les lignes NOUVELLES vont dans le fichier réservé de la famille (une fonction `static void vg_tests_xxx(FILE *f)`,
   déjà appelée à la fin du bloc « sac de stats » de `vagues_tests.c`) : une ligne par stat lue, avec la valeur au niveau 1 et à un niveau supérieur,
   et l'identité hors VAGUES.

### 5.1 Roquettes + Railgun

**Stats** — Roquettes : `MUN` (Soute), `VIT` (Propulseur, Roquette lourde), `ZON` (Rayon, Grand souffle), `TAI` (Ogive large), `POR` (Combustible), `BORD` (Souffle plein),
`CENTRE` (Cœur du souffle), `DIRECT` (Impact direct), `SUI` (Trajectoire courbe). Railgun : `PTK` (Réserve), `TAI` (Rail large, Rail colossal), `TETE` (Lunette, Rail
colossal), `SUI` (Visée courbe, Tir guidé), `DEGC` (Rail tendu, Calibre lourd, Tir d'élite). Conditions utilisées : `direct`, `pierced`, `elite`. Groupes : `TAI`,
`TETE` et `SUI` du rail.

**Données** (à coller à la place des entrées actuelles des mêmes étapes ; les événements, comportements, capstones et fusions ne bougent pas)

**`effets/roquettes.js`**

```js
    'ROQ:Soute': { stat: 'MUN', op: 'add', val: 1, fixed: true },
    'ROQ:Propulseur': { stat: 'VIT', op: 'mul', val: 1.10 },
    'ROQ:Rayon': { stat: 'ZON', op: 'add', val: 0.4 },
    'ROQ:Ogive large': { stat: 'TAI', op: 'mul', val: 1.20 },
    'ROQ:Combustible': { stat: 'POR', op: 'add', val: 20 },
    'ROQ:Souffle plein': [
      { stat: 'BORD', op: 'add', val: 0.15 },
      { stat: 'BORD', op: 'min', val: 0.95, fixed: true },
    ],
    'ROQ:Impact direct': { stat: 'DIRECT', op: 'add', val: 0.15, cond: 'direct' },
    'ROQ:Grand souffle': { stat: 'ZON', op: 'add', val: 0.8 },
    'ROQ:Cœur du souffle': { stat: 'CENTRE', op: 'add', val: 0.25 },
    'ROQ:Trajectoire courbe': { stat: 'SUI', op: 'add', val: 3 },
    'ROQ:Roquette lourde': [
      { stat: 'DEG', op: 'mul', val: 1.40 },
      { stat: 'VIT', op: 'mul', val: 0.90 },
    ],
```

**`effets/rail.js`**

```js
    'RAIL:Réserve': { stat: 'PTK', op: 'mul', val: 1.20 },
    'RAIL:Rail large': { stat: 'TAI', op: 'mul', val: 1.15, grp: 1 },
    'RAIL:Rail tendu': { stat: 'DEGC', op: 'mull', val: 1.05, cond: 'pierced', par: 5 },
    'RAIL:Visée courbe': { stat: 'SUI', op: 'add', val: 1, grp: 1 },
    'RAIL:Lunette': { stat: 'TETE', op: 'add', val: 0.15, grp: 1 },
    'RAIL:Calibre lourd': { stat: 'DEGC', op: 'mul', val: 1.15, cond: 'pierced' },
    'RAIL:Rail colossal': [
      { stat: 'TAI', op: 'mul', val: 1.40, grp: 1 },
      { stat: 'TETE', op: 'add', val: 0.40, grp: 1 },
    ],
    'RAIL:Tir d’élite': { stat: 'DEGC', op: 'mul', val: 1.50, cond: 'elite' },
    'RAIL:Tir guidé': { stat: 'SUI', op: 'add', val: 8, grp: 1 },
```

**Code**

| Fichier | Fonction | Lecture à brancher |
|---|---|---|
| `src/solo/boutique_roquettes.c` | `roq_max(w, base)` | `return st_int(SH_ROQ, STAT_MUN, base)` après le capstone MINI-NUKE (qui renvoie 1) |
| | `roq_setup(p)` | `m = st(SH_ROQ, STAT_VIT, 1.0f)` (Propulseur et Roquette lourde se composent : x1,10 x 0,90) puis le x2 de ROQUETTE MONTEE en code ; `roqSize = st(SH_ROQ, STAT_TAI, 1.0f)` si > 1 ; `p->life += st(SH_ROQ, STAT_POR, 0) / fmaxf(p->roqSpd, 1.0f)` ; **supprimer la ligne `roq_has(RQ_LOURDE)`** |
| | `roq_radius(p)` | `r = st(SH_ROQ, STAT_ZON, ROCKET_RADIUS)` puis mini-roquette x0,55 et MINI-NUKE 14 m en code |
| | `roq_explode(...)` | `roqEdge = st(SH_ROQ, STAT_BORD, 0.3f)` (le plafond 0,95 est dans les données) ; `roqCore = st(SH_ROQ, STAT_CENTRE, 0)` ; Impact direct : `StatCtx c = { 0 }; c.direct = 1; k = st_ctx(SH_ROQ, STAT_DIRECT, 0, &c)` puis, si `k > 0`, le 2e `damage_enemy(hitEnemy, dmg * k, ...)` (un DEUXIÈME coup sur les dégâts bruts : pas un facteur de `DEG`) |
| | `roq_tick_proj(p, dt)` | `turn = st(SH_ROQ, STAT_SUI, 0)` ; suivi actif si `turn > 0`, `fminf(1, turn * dt)` ; ciblage en code |
| `src/solo/boutique_rail.c` | `vg_rail_wide(w)` | `st(SH_RAIL, STAT_TAI, 1.0f)` (Rail colossal remplace Rail large : groupe) ; lue par `hitscan_seg` dans la boucle des ennemis : sortir la lecture de la boucle |
| | `vg_rail_head(i, at, w)` | `hw = st(SH_RAIL, STAT_TETE, 0)` puis la formule de `frac` inchangée |
| | `vg_rail_bend(o, d)` | `deg = st(SH_RAIL, STAT_SUI, 0)` (Tir guidé remplace Visée courbe : groupe) |
| | `vg_rail_mul(i)` | `StatCtx c = { 0 }; c.pierced = rlRank[i]; c.elite = E[i].elite != 0; return st_ctx(SH_RAIL, STAT_DEGC, 1.0f, &c)` |
| | `vg_rail_gauge_mul()` | supprimer (la lecture passe dans `ammo_points`, ligne suivante) |
| `src/combat/loadout.c` | `ammo_points` | une ligne : remplacer `pts *= vg_rail_gauge_mul()` par `pts = st(SH_RAIL, STAT_PTK, pts)` (à côté de la ligne du Shotgun) |
| `src/combat/pieces_rail.c` | `hitscan_seg` | facultatif : lire `vg_rail_wide(w)` une fois avant la boucle des ennemis (signature inchangée) |
| `docs/boutique_vagues/outils/effets/roquettes.js`, `rail.js` | | les blocs ci-dessus ; mettre à jour le commentaire de tête |

**Tests existants à garder identiques** (blocs de `src/tests/vagues_tests.c`) : « boutique ROQUETTES : etapes, capstones et fusions », les quatre « boutique RAILGUN : ... »
(largeur, visée courbe, traversée, souffle d'impact ; tête ; Perce-mur, Rail jumeau ; All in One), « niveaux : Perce-mur monte des deux cotes », « niveaux d'amelioration »
(Rayon), « 5 etapes debloquees » (Rail tendu, Tir d'élite), « CANON SCIE », « OBUSIER », « FORMES FUSIONNEES ». **Nouvelles lignes** : `src/tests/vagues_tests_roq_rail.c`
(`vg_tests_roq_rail`). **Reste en code** : Éclat d'impact, Satellite, Fourche, Surtension, Sang-froid, Doublé, Éclats, Tir explosif, Rail jumeau, Perce-mur (Railgun) ;
Fusée de proximité, Double départ, Cratère vital, Récupération, Éclats, Détonation en chaîne, Salve, Grappe (Roquettes) ; tous les capstones et fusions.

### 5.2 Lance + Impulsion

**Stats** — Lance : `CDJ` (Recharge, Lance-éclair), `VIT` (Vélocité), `COM` (Second estoc, Estocs en chaîne), `CDE` (Estoc vif, Lance-éclair), `ANI` (Lance vive, Lance-éclair),
`MURJ` (Butin de joute, Pillage d'élite), `CDV` (Relève), `TCH` (Charge éclair). Impulsion : `CDC` (Cadence, Combo enchaîné), `ECA` (Poings vifs, Rafale, Jambes vives),
`COM` (4e, 5e, 6e coup, Huit coups), `CDW` (Souffle court), `MUR` (Prise aérienne), `MURC` (Butin de combo). Opération `to` (Relève, Charge éclair, Poings vifs, Rafale),
phase 1 (Combo enchaîné, Jambes vives), conditions `elite` et `air`.

**Données**

**`effets/melee.js`**

```js
put('LAN', 0, 0, { stat: 'CDJ', op: 'add', val: -0.2 });   // Recharge
put('LAN', 0, 1, { stat: 'VIT', op: 'mul', val: 1.10 });   // Vélocité
put('LAN', 0, 2, { stat: 'COM', op: 'max', val: 1, fixed: true });   // Second estoc
put('LAN', 1, 0, { stat: 'CDE', op: 'mul', val: 0.85 });   // Estoc vif
put('LAN', 1, 1, { stat: 'ANI', op: 'mul', val: 0.80 });   // Lance vive
put('LAN', 1, 2, { stat: 'COM', op: 'max', val: 2, fixed: true });   // Estocs en chaîne
put('LAN', 1, 3, { stat: 'MURJ', op: 'add', val: 25 });   // Butin de joute
put('LAN', 2, 1, { stat: 'CDV', op: 'to', val: 0.25 });   // Relève
put('LAN', 2, 2, { stat: 'TCH', op: 'to', val: 0.10 });   // Charge éclair
put('LAN', 2, 4, { stat: 'MURJ', op: 'add', val: 40, cond: 'elite' });   // Pillage d’élite
put('LAN', 3, 1, [
  { stat: 'ANI', op: 'mul', val: 0.65 },
  { stat: 'CDE', op: 'mul', val: 0.65 },
  { stat: 'CDJ', op: 'mul', val: 0.65 },
]);   // Lance-éclair
put('IMP', 0, 0, { stat: 'CDC', op: 'mul', val: 0.90 });   // Cadence
put('IMP', 0, 1, { stat: 'ECA', op: 'to', val: 0.20 });   // Poings vifs
put('IMP', 0, 2, { stat: 'COM', op: 'max', val: 4, fixed: true });   // Quatrième coup
put('IMP', 1, 0, { stat: 'CDW', op: 'mul', val: 0.50 });   // Souffle court
put('IMP', 1, 1, { stat: 'ECA', op: 'mul', val: 0.80, cond: 'air', phase: 1 });   // Jambes vives
put('IMP', 1, 2, { stat: 'COM', op: 'max', val: 5, fixed: true });   // Cinquième coup
put('IMP', 1, 3, { stat: 'MUR', op: 'add', val: 15, cond: 'air' });   // Prise aérienne
put('IMP', 2, 1, { stat: 'CDC', op: 'add', val: -0.2, phase: 1 });   // Combo enchaîné
put('IMP', 2, 2, { stat: 'ECA', op: 'to', val: 0.16 });   // Rafale
put('IMP', 2, 3, { stat: 'COM', op: 'max', val: 6, fixed: true });   // Sixième coup
put('IMP', 2, 4, { stat: 'MURC', op: 'add', val: 40 });   // Butin de combo
put('IMP', 3, 2, { stat: 'COM', op: 'max', val: 8, fixed: true });   // Huit coups
```

**Code** (tout est dans `src/solo/boutique_melee.c` ; `lance.c` et `poings.c` appellent les mêmes fonctions : rien à y changer)

| Fonction | Lecture à brancher |
|---|---|
| `vgm_vc(vc)` | `st(SH_LAN, STAT_VIT, vc)` |
| `vgm_estoc_hit(i)` | `n = st_int(SH_LAN, STAT_COM, 0)` puis le `max` avec le capstone LAN>AIO en code |
| `vgm_thrust_cd(cd)` | `st(SH_LAN, STAT_CDE, cd)` |
| `vgm_whiff_cd(cd)` | `st(SH_LAN, STAT_CDV, cd)` (`to` : deux bases, 0,9 s et 0,5 s) |
| `vgm_estoc_anim(t)` | `st(SH_LAN, STAT_ANI, t)` |
| `vgm_tap(t)` | `st(SH_LAN, STAT_TCH, t)` |
| `vgm_joust_hit(i)` | `pts = st_ctx(SH_LAN, STAT_MURJ, 0, &c)` avec `c.elite = E[i].elite != 0` ; `cd = st(SH_LAN, STAT_CDJ, LANCE_HIT_CD)` (Recharge puis Lance-éclair : `(1,0 - 0,2) x 0,65`) ; Joute en chaîne, Ruée, `fmaxf(0, cd)` et le soin en code |
| `vgm_fist_n()` | `n = st_int(SH_IMP, STAT_COM, 3)` puis le `max` avec le capstone IMP>AIO en code |
| `vgm_fist_gap(g)` | `c.air = !P.grounded; g = st_ctx(SH_IMP, STAT_ECA, g, &c)` ; la Frénésie (x par empilement) reste en code après la lecture |
| `vgm_fist_end(cd)` | `if (vgmFreeT > 0) return 0; return fmaxf(0, st(SH_IMP, STAT_CDC, cd))` : 0,70 x 0,9 - 0,2 = 0,43 s |
| `vgm_fist_whiff(cd)` | `st(SH_IMP, STAT_CDW, cd)` (le `vgmFreeT > 0` reste) |
| `vgm_fist_hit(up)` | `c.air = !P.grounded; pts = st_ctx(SH_IMP, STAT_MUR, 0, &c)` (Cueillette, déjà migrée, et Prise aérienne dans la même lecture) |
| `vgm_fist_full(landed)` | `pts = st(SH_IMP, STAT_MURC, 0)` si le combo complet a touché |
| `vgm_lower`, `vgm_max`, `vgm_step_b`, `vgm_step_sc` | `vgm_lower` n'a plus d'appelant (Relève, Charge éclair, Poings vifs, Rafale migrent) : le supprimer. `vgm_max`, `vgm_step_b`, `vgm_step_sc` servent encore (Tournoi, Ruée, Joute en chaîne, Poings infatigables, Frénésie) |

**Tests existants à garder identiques** : « boutique melee : Lance (estocs, joute) et Impulsion (combo) », « niveaux d'amelioration » (Vélocité), « 5 etapes debloquees »
(Relève), les blocs « capstone » qui passent par la mêlée. **Nouvelles lignes** : `src/tests/vagues_tests_melee.c` (`vg_tests_melee`). **Reste en code** : Armure de fer,
Joute en chaîne, Ruée, Tournoi, Trésor de guerre (Lance) ; Souffle acrobate, Poings infatigables, Frénésie, Pillage de poings (Impulsion) ; le comportement de Huit coups
(`vgm_fist_up` : un uppercut tous les 4 coups) ; les capstones.

### 5.3 Pièce + All in One

**Stats** — Pièce : `POR` (Pièce lestée), `RIC` (Pièce polie, Prime de ricochet, Ricochet pur), `CD` (Recharge), `MUN` (Réserve), `DUREE` (Pièce volante), `NBP` (Pluie).
All in One : `CD` (Rechargement), `ANGLE` (Salve large, Salve précise), `CDT` (Cadence), `MUN` (Réserve), `DEGS` et `NBP` (Salve complète). Conditions `first`, `bounce`, `salvefull` ;
groupe `RIC` (Polie / Prime).

**Données**

**`effets/competences.js`**

```js
    'PIE:Pièce lestée': { stat: 'POR', op: 'add', val: 15 },
    'PIE:Pièce polie': { stat: 'RIC', op: 'add', val: 0.1, grp: 1 },
    'PIE:Recharge': { stat: 'CD', op: 'mul', val: 0.90 },
    'PIE:Réserve': { stat: 'MUN', op: 'add', val: 1, fixed: true },
    'PIE:Pièce volante': { stat: 'DUREE', op: 'add', val: 1 },
    'PIE:Prime de ricochet': { stat: 'RIC', op: 'add', val: 0.5, cond: 'first', grp: 1 },
    'PIE:Pluie': { stat: 'NBP', op: 'add', val: 1, fixed: true },
    'PIE:Ricochet pur': { stat: 'RIC', op: 'mul', val: 1.25, cond: 'bounce', par: 3 },
    'AIO:Rechargement': { stat: 'CD', op: 'mul', val: 0.85 },
    'AIO:Salve large': { stat: 'ANGLE', op: 'add', val: 0.3 },
    'AIO:Cadence': { stat: 'CDT', op: 'mul', val: 0.70 },
    'AIO:Réserve': { stat: 'MUN', op: 'add', val: 1, fixed: true },
    'AIO:Salve précise': { stat: 'ANGLE', op: 'mul', val: 0.70 },
    'AIO:Salve complète': [
      { stat: 'DEGS', op: 'mul', val: 1.30, cond: 'salvefull' },
      { stat: 'NBP', op: 'add', val: 1, cond: 'salvefull', fixed: true },
    ],
```

**Code** (tout est dans `src/solo/boutique_competences.c` ; `competences.c`, `pieces_rail.c`, `projectiles.c`, `hud.c` appellent les mêmes fonctions)

| Fonction | Lecture à brancher |
|---|---|
| `cp_seek_range()` | `st(SH_PIE, STAT_POR, COIN_SEEK_RANGE)` |
| `cp_coin_mult(depth)` | `StatCtx c = { 0 }; c.bounce = depth + 1; return st_ctx(SH_PIE, STAT_RIC, COIN_MULT, &c)` (`depth` vaut 0 au premier rebond ; les appelants ne passent jamais de valeur négative) ; garder `if (!vagOn) return m` si on veut un chemin PvP sans appel |
| `cp_coin_life()` | `st(SH_PIE, STAT_DUREE, 4.0f) + cp_sc(CP_ORBITALE) * 4.0f` (l'orbite de Pièce orbitale reste en code) |
| `cp_coin_dirs(f, out)` | `st_int(SH_PIE, STAT_NBP, 1) >= 2` pour Pluie ; le capstone PIE>RAIL (8 pièces) passe avant, en code |
| `cp_max(sk, base)` | `sk == SK_COIN` : `st_int(SH_PIE, STAT_MUN, base)` |
| `cp_recharge(sk, base)` | `SK_COIN` : `st(SH_PIE, STAT_CD, base)` ; `SK_ALLIN` : `st(SH_AIO, STAT_CD, base)` ; le plancher `fmaxf(0.1f, ...)` (jamais atteint aux niveaux permis) peut rester : `fmaxf(0.1f * base, st(...))` |
| `cp_half_deg(base)` | `st(SH_AIO, STAT_ANGLE, base)` en DEGRÉS : `(base + 0,3) x 0,7` |
| `cp_allin_extra(w, n)` | `c.full = P.ammo[w] >= wp_max(w); x = st_int(SH_AIO, STAT_MUN, 0) + st_int_ctx(SH_AIO, STAT_NBP, 0, &c)` ; les capstones SALVE RAIL et SHOTGUN TOTAL après, en code |
| `cp_salve_cd(cd)` | `st(SH_AIO, STAT_CDT, cd)` ; plancher `fmaxf(0.1f * cd, ...)` en code |
| `cp_dmg_mul(i)` | `c.full = cpSalveFull; m *= st_ctx(SH_AIO, STAT_DEGS, 1.0f, &c)` ; Cible unique (état par cible) reste en code |

**Tests existants à garder identiques** : « groupe COMPETENCES : PIECE (...) », « groupe COMPETENCES : ALL IN ONE (...) », « 5 etapes debloquees » (Ricochet pur, Pièce lestée),
« niveaux d'amelioration » (Recharge de la Pièce), « capstones All in One », « capstone PIE>SHO (Piece en plombs) », « capstone AIO>SHO (Salve de Shotgun) ». **Nouvelles
lignes** : `src/tests/vagues_tests_comp.c` (`vg_tests_comp`). **Reste en code** : Pièce aimantée, Pièce recyclée, Éclats, Rebond de plus, Aimant doux, Dividende, Pièce
paratonnerre, Détonation, Vortex, Fourche, Pièce piégée, Pièce orbitale (Pièce) ; Pulsion, Spectre, Rechargement jauge, Prime de salve, Dernier souffle, Éclats, Fracture,
Fourche, Vortex, Ressac, Salve double, Cible unique, Détonation (All in One) ; les capstones.

## 6. Qui touche quoi (trois travaux en parallèle)

| Fichier | Roquettes + Railgun | Lance + Impulsion | Pièce + All in One |
|---|---|---|---|
| `effets/roquettes.js`, `effets/rail.js` | oui | | |
| `effets/melee.js` | | oui | |
| `effets/competences.js` | | | oui |
| `src/solo/boutique_roquettes.c`, `boutique_rail.c` | oui | | |
| `src/solo/boutique_melee.c` | | oui | |
| `src/solo/boutique_competences.c` | | | oui |
| `src/combat/loadout.c` (`ammo_points`, une ligne), `src/combat/pieces_rail.c` (facultatif) | oui | | |
| `src/tests/vagues_tests_roq_rail.c` | oui | | |
| `src/tests/vagues_tests_melee.c` | | oui | |
| `src/tests/vagues_tests_comp.c` | | | oui |

Autres fichiers :

- `src/solo/boutique_donnees.c` : **généré** par `node export_c.js`, jamais à la main ; chacun le relance avant de compiler (si une compilation tombe sur un fichier tronqué parce que deux générateurs ont écrit en même temps : le relancer), et une dernière fois après les trois fusions.
- `src/tests/vagues_tests.c` : corrections minimales et précises (Edit) uniquement si un ancien test dépend d'une fonction supprimée, sur les lignes de SA famille.
- Personne ne touche : `src/monde/entites.c`, `src/solo/boutique_stats.c`, `src/solo/ameliorations.c`, `export_c.js`, `effets_c.js`, `main.c`, les docs communes.

Pourquoi les trois travaux ne se gênent pas : les familles lisent des graines différentes (`SH_ROQ` / `SH_RAIL`, `SH_LAN` / `SH_IMP`, `SH_PIE` / `SH_AIO`), chaque étape-stat est une
ligne indépendante de `shMods` (aucun code ne désigne une ligne par son indice), et la table générée est correcte quelle que soit la combinaison de familles migrées
(la ligne « table generee, anomalies 0 » de `--test` le vérifie).
