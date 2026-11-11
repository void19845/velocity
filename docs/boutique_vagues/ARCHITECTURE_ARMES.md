# Armes « objet » : sac de stats, fiche d'arme, événements

Refactor demandé par le propriétaire (« de la prog objet sur les armes pour rendre l'implémentation des modificateurs plus simple »). Le projet est
en C pur : pas de classes, mais l'équivalent (structures de données + tables de fonctions) donne l'essentiel du bénéfice.

## État d'avancement

| Phase | Contenu | État |
|---|---|---|
| 0 | Gel : sortie de `--test` d'avant refactor (671 lignes) et lignes instables repérées | fait |
| 1 | Socle : `src/solo/boutique_stats.c`, table `shMods` générée, genres génériques `BK_DMG` / `BK_NEAR` / `BK_VOL` / `BK_PTS` migrés | fait |
| 2a | Famille **Shotgun** (modèle de référence : §4) | fait |
| 2b | **Préparation** des familles restantes : les 19 stats nommées, les opérations `to` et `min`, les conditions `direct` / `bounce` / `first` / `salvefull`, les phases d'application, `st_int`, le groupe jugé au calcul ; un test unitaire chacun ; les 56 entrées de données vérifiées | fait |
| 2c | Roquettes + Railgun, Lance + Impulsion, Pièce + All in One : trois travaux indépendants (données à coller, code à brancher, fichiers touchés : `CLASSIFICATION_ETAPES.md`, §5 et §6) | à faire |
| 3 | Fiche d'arme (B) | à faire |
| 4 | Bus d'événements (C) | à faire |

Classification des 140 étapes (stat, stat conditionnelle, événement, comportement) : **`CLASSIFICATION_ETAPES.md`**. Résultat : 82 stats (70 simples,
12 conditionnelles), 58 événements ou comportements ; 26 étapes sont déjà migrées, 56 restent (leurs données sont prêtes, §5 de ce document-là).

## 1. Constat (mesuré avant le refactor)

- Chaque famille avait ses propres accesseurs dans `src/solo/boutique_*.c` : Mêlée 33, Compétences 26, Shotgun 20, Roquettes 19, Railgun 7
  (environ 105 fonctions), plus une quarantaine de crochets `vagOn` dans les fichiers d'armes et de combat.
- Chaque famille avait son propre jeu d'identifiants d'effets (`SHE_*`, `RL_*`, `CP_*`, `BK_MEL_*`, `BK_ROQ`...), environ 180 entrées dans `outils/effets/*.js`.
- Ajouter une étape comme « Gros plomb : taille des plombs +15 % » demandait : un identifiant `SHE_GROS`, un accesseur `vgsg_size()`, un crochet dans
  `fire_shotgun`, une entrée dans `effets/shotgun.js` et une ligne de test.
- Les axes du design (dégâts, taille, vitesse, zone, portée, munitions, projectiles, suivi ; mêlée : cooldown, animation, combo, munition rendue,
  lifesteal) sont exactement des **stats** : le code ne les traitait pas comme telles.

## 2. Principe : trois couches

### A. Sac de stats (fait)

Une valeur effective par (graine, stat), calculée à partir de **lignes de données** : une étape-stat est une entrée de `effets/<famille>.js`, le
générateur l'écrit dans la table `shMods[]` de `boutique_donnees.c`, et le code de l'arme lit la stat avec `st()`, une seule fois par tir. Ajouter une
étape-stat sur une stat déjà lue = **une entrée dans `effets/*.js`, zéro ligne de C**.

**Fichiers**

| Fichier | Contenu |
|---|---|
| `src/monde/entites.c` | enums `SH_*` (graines), `STAT_*`, `SOP_*`, `COND_*` ; types `StatMod`, `StatCtx` ; déclarations avancées `st`, `st_ctx`, `st_int`, `st_int_ctx`, `sh_rebuild` (les armes, compilées avant la boutique, lisent le sac) |
| `src/solo/boutique_stats.c` | le sac : `sh_rebuild`, `st`, `st_ctx`, `st_int`, `st_int_ctx`, `st_fold` (le calcul), `st_reps` (les conditions), `st_scaled` (le niveau), `st_act` (modificateur construit à la main, pour les tests), `st_table_check`, `st_find_step` (inclus après `ameliorations.c`) |
| `src/solo/boutique_donnees.c` | GÉNÉRÉ : `shSteps` (une étape-stat a le genre `BK_STAT`, `a` = première ligne de `shMods`, `b` = nombre de lignes) et `shMods[SH_NMOD]` |
| `docs/boutique_vagues/outils/export_c.js` | le générateur ; valide les noms de stats, d'opérations et de conditions contre les enums de `entites.c`, et les paramètres (`bounce` : entier >= 1, `pierced` : >= 0, `grp` interdit avec `to`, phase 0 à 3, au plus 16 modificateurs par couple graine / stat) |

**Types**

```c
enum { SH_RAIL, SH_ROQ, SH_SHO, SH_LAN, SH_IMP, SH_PIE, SH_AIO, SH_N };
enum { STAT_DEG, STAT_TAI, STAT_VIT, STAT_ZON, STAT_POR, STAT_MUN, STAT_NBP, STAT_SUI,      /* axes des armes a distance et competences */
       STAT_CD, STAT_ANI, STAT_COM, STAT_MUR, STAT_VOL,                                      /* axes de la melee */
       STAT_CONE, STAT_PERTE, STAT_PNEAR, STAT_PFAR, STAT_COUT, STAT_PTK,                    /* stats nommees du Shotgun et des points de jauge */
       STAT_TETE, STAT_DEGC,                                 /* Railgun : zone de la tete, degats par cible traversee */
       STAT_BORD, STAT_CENTRE, STAT_DIRECT,                  /* Roquettes : degats au bord, au centre du souffle, impact direct */
       STAT_CDJ, STAT_CDE, STAT_CDV, STAT_TCH, STAT_MURJ,    /* Lance : recharges (joute, estoc, vide), charge couchee, points de joute */
       STAT_CDC, STAT_ECA, STAT_CDW, STAT_MURC,              /* Impulsion : recharges (combo, combo rate), ecart des coups, points de combo */
       STAT_RIC, STAT_DUREE,                                 /* Piece : multiplicateur de ricochet, duree de vie */
       STAT_CDT, STAT_DEGS, STAT_ANGLE,                      /* All in One : delai entre tirs, degats de la salve, demi-angle (degres) */
       STAT_COUNT };
enum { SOP_MUL, SOP_ADD, SOP_MAX, SOP_MULL, SOP_MIN, SOP_TO, SOP_COUNT };                    /* operations */
enum { COND_NONE, COND_NEAR, COND_HEAD, COND_AIR, COND_ELITE, COND_PIERCED,                  /* conditions */
       COND_DIRECT, COND_BOUNCE, COND_FIRST, COND_SALVEFULL, COND_COUNT };
#define SMF_FIXED 1                                                                          /* la valeur ne monte pas avec le niveau */
typedef struct { unsigned char seed, step, stat, op, cond, grp, flags, phase; float val, par; } StatMod;   /* une ligne de donnees */
typedef struct { float dist; int head, air, elite, pierced, bounce, direct, full; } StatCtx;               /* contexte d'un coup (0 = inconnu / faux) */
static float st(int seed, int stat, float base);                                  /* valeur effective, sans contexte */
static float st_ctx(int seed, int stat, float base, const StatCtx *c);            /* avec contexte (conditions) */
static int st_int(int seed, int stat, int base);                                  /* entier le plus proche : munitions, coups, plombs, pieces */
static int st_int_ctx(int seed, int stat, int base, const StatCtx *c);
static void sh_rebuild(void);                                                     /* recalcule le sac */
```

Le catalogue des 38 stats (sens, base et unité, fonction qui la lit, étapes concernées) est dans `CLASSIFICATION_ETAPES.md`, §2.

**Format d'une étape-stat** dans `effets/<famille>.js` (une entrée par étape, clé `GRAINE:Nom`) : un objet, ou un tableau d'objets si l'étape modifie
plusieurs stats.

| Champ | Sens |
|---|---|
| `stat` | nom de la stat sans le préfixe (`'TAI'` = `STAT_TAI`) |
| `op` | `'mul'` (`val` = facteur : 1,15 = +15 %), `'add'` (`val` = incrément), `'max'` (plancher : la valeur ne descend pas sous `val`), `'min'` (plafond : elle ne monte pas au-dessus de `val`), `'mull'` (comme `mul`, mais quand la condition se répète, les deltas s'additionnent : `1 + (val - 1) x N` au lieu de `val^N`), `'to'` (« vers une valeur » : abaisse la valeur jusqu'à `val` ; le niveau pousse AU-DELÀ de la cible, `base - (base - val) x échelle` ; sans effet si la valeur est déjà sous `val` ; plusieurs `to` : le meilleur) |
| `val` | la valeur de l'étape au niveau 1 |
| `cond` | `'near'` (+ `par` : distance en m, strictement sous), `'head'`, `'air'`, `'elite'`, `'pierced'` (+ `par` : plafond, 0 = aucun), `'direct'` (la cible de l'impact direct), `'bounce'` (+ `par` : à partir du rebond N >= 1, une application par rebond), `'first'` (le premier rebond seulement), `'salvefull'` (salve lancée chargeur plein) ; absent = toujours |
| `unit` | `'deg'` : `val` est en degrés, converti en radians dans la table (pas pour `ANGLE`, que le code lit en degrés) |
| `grp` | numéro de groupe (> 0) : parmi les modificateurs APPLICABLES (condition vraie) d'une même stat et d'un même groupe, **seul le plus fort** compte ; interdit avec `to` |
| `phase` | 0 (défaut) à 3 : ordre d'application (voir les règles) |
| `fixed` | `true` : la valeur ne monte pas avec le niveau (entier fixe, plafond absolu) ; une étape dont TOUS les modificateurs sont `fixed` n'est pas proposée en amélioration |

```js
'SHO:Gros plomb': { stat: 'TAI', op: 'mul', val: 1.15 },
'SHO:Poing de plomb': { stat: 'DEG', op: 'mul', val: 1.10, cond: 'near', par: 5 },
'SHO:Cône serré': [ { stat: 'CONE', op: 'add', val: -1, unit: 'deg' }, { stat: 'PNEAR', op: 'add', val: 5 }, { stat: 'PFAR', op: 'add', val: 5 } ],
'IMP:Combo enchaîné': { stat: 'CDC', op: 'add', val: -0.2, phase: 1 },          /* APRES le x0,9 de Cadence : 0,70 x 0,9 - 0,2 = 0,43 s */
'LAN:Relève': { stat: 'CDV', op: 'to', val: 0.25 },                              /* abaisse jusqu'à 0,25 s, le niveau pousse au-delà */
'PIE:Ricochet pur': { stat: 'RIC', op: 'mul', val: 1.25, cond: 'bounce', par: 3 },   /* x1,25 au 3e rebond, x1,25^2 au 4e... */
'ROQ:Souffle plein': [ { stat: 'BORD', op: 'add', val: 0.15 }, { stat: 'BORD', op: 'min', val: 0.95, fixed: true } ],
```

**Règles**

- **Hors VAGUES, ou graine non équipée : `st()` rend la base telle quelle** (aucune opération flottante) : PvP et LAN ne changent pas, `NET_VER` non plus.
  Les armes ne doivent donc rien faire de spécial hors VAGUES ; pour garder le chemin PvP rigoureusement identique (et rapide), le code qui lit plusieurs
  stats peut les isoler dans une fonction qui rend la base quand `!vagOn` (voir `sg_stats`).
- **Niveau** (+20 % par niveau, validé par le propriétaire) : le **delta** de chaque modificateur est multiplié par l'échelle
  `sh_scale(graine, étape) = 1 + 0,2 x (niveau - 1)`, à un seul endroit (`st_scaled`) : `mul` / `mull` : facteur = `1 + (val - 1) x échelle` (un gain monte, une
  pénalité monte aussi : x0,9 devient x0,86 au niveau 3) ; `add` / `max` / `min` : valeur = `val x échelle` ; `to` : `base - (base - val) x échelle` (le niveau
  pousse au-delà de la cible, comme `vgm_lower`). `fixed` : échelle 1.
- **Ordre d'application** : les modificateurs s'appliquent **phase par phase** (`phase` 0, puis 1, 2, 3). Dans une phase : d'abord `to` (le meilleur candidat,
  relatif à la valeur qui entre dans la phase), puis `(valeur + somme des add) x produit des mul`, puis les planchers (`max`), puis les plafonds (`min`). Sans
  phase (le cas courant), c'est `(base + add) x mul`, puis `max`, puis `min`. Un `max` passe donc après les `add` (Cône serré +5 m puis Pompe tendue, plancher
  20 m : `max(15, 20) = 20`). Une phase 1 exprime « d'abord le pourcentage, ensuite le retrait en secondes » : Impulsion, Cadence (x0,9, phase 0) puis Combo
  enchaîné (-0,2 s, phase 1) : `0,70 x 0,9 - 0,2 = 0,43 s`, alors que dans la même phase on obtiendrait `(0,70 - 0,2) x 0,9 = 0,45 s`.
- **Conditions** : le contexte (`StatCtx`) est rempli par l'appelant qui connaît la touche ou le tir (`vg_dmg_mul` : `dist`, `air`, `elite` ; `vg_rail_mul` :
  `pierced`, `elite` ; `cp_coin_mult` : `bounce` ; `cp_dmg_mul` : `full`). Chaque condition donne un nombre d'applications du modificateur (`st_reps`) :
  `near` : `dist < par` (1 ou 0) ; `head`, `air`, `elite`, `direct`, `salvefull` : 1 ou 0 ; `pierced` : une application par cible déjà traversée (`pierced`,
  plafonné à `par`, 0 = sans plafond) ; `bounce` : une application par rebond à partir du rebond `par` (`bounce` vaut 1 au premier rebond : avec `par` 3,
  2 applications au 4e rebond) ; `first` : 1 au premier rebond seulement. Les applications se composent pour `mul` (`val^N`) et s'additionnent pour `mull` et `add`.
  **Sans contexte (`st`), un modificateur conditionnel ne compte pas** : c'est ce que veulent `hy_tree_dmg` et les lectures « une fois par tir ».
- **Groupes** : les paliers d'une même idée ne s'additionnent pas (Plombs chercheurs 3° / traqueurs 8° : le plus fort, 8°, pas 11°). Le groupe se juge **au moment
  du calcul, parmi les modificateurs applicables** : Prime de ricochet (`first`, +0,5) et Pièce polie (+0,1) sont dans le même groupe, donc Prime REMPLACE Polie
  au premier rebond, et aux rebonds suivants, où Prime ne s'applique pas, Polie revient. À égalité de force, le premier de la table. Retirer le `grp` d'une étape
  suffit pour qu'elles s'additionnent. L'opération `to` n'a pas de groupe (elle garde déjà le meilleur candidat).
- **Entiers** : `st_int` / `st_int_ctx` arrondissent au plus proche (`floorf(x + 0,5)`) : nombre de plombs, de coups, de munitions, de pièces. Les entiers fixes
  (`max` 4 coups, `add` +1 munition) sont `fixed`. Les garde-fous (`fmaxf(0, ...)`, plafonds de sécurité) et les probabilités restent dans le code de l'arme.
- **Cache** : le sac est recalculé par `sh_rebuild()` à l'achat, à la montée de niveau (`up_buy`), au changement d'arme (`sh_swap`) et au reset
  (`sh_reset`). En plus, `st()` compare l'état de la boutique (`shOwn`, `shLvl`, `shCap`, `shFusion`, loadout) à celui du dernier calcul et recalcule seul
  s'il a changé : un test (ou un code) qui écrit `shOwn` / `shLvl` directement reste juste, sans appeler `sh_rebuild`.
- Les formes fusionnées héritent des stats des deux arbres en lisant chacun directement (`hy_tree_dmg(graine)` = `st(graine, STAT_DEG, 1)`, `sg_stats` pour
  le Shotgun) ; une fonction `st_hybrid` n'a pas été nécessaire.

**Lecture côté arme : le modèle `SgStats`** (`src/armes/shotgun.c`). Toutes les stats d'un tir sont lues dans une structure, UNE fois, au début de
`fire_shotgun` ; le reste du tir ne lit plus que la structure. Les formes fusionnées et la pièce en plombs l'utilisent aussi.

```c
SgStats S;
sg_stats(&S, SG_HALF_ANGLE * (P.sliding ? SG_SLIDE_SPREAD : 1), SG_RANGE);   /* les bases du tireur ; hors VAGUES : S = valeurs du PvP */
...
float fo = sg_fall(&S, travelled + tw);                 /* perte de degats a la distance parcourue */
P.fireCd = SG_CD / streak_rate() * S.cd;                /* delai entre deux salves */
```

### B. Fiche d'arme (table de fonctions) : à faire

```c
typedef struct Weapon {
    const char *name; int id, seed;
    void (*fire)(void);                       /* tir */
    void (*on_hit)(int enemy, float dmg);     /* touche (optionnel) */
    void (*on_kill)(int enemy);               /* kill (optionnel) */
} Weapon;
static const Weapon weapons[WP_COUNT];
```

Remplace les `switch (P.weapon)` dispersés (`fire_weapon`, ammo, HUD) : une nouvelle arme = une fiche à remplir. À faire après A, seulement là où il y a un
vrai gain : ne pas réécrire le code de tir qui marche.

### C. Bus d'événements pour capstones et fusions : à faire

Les effets qui ne sont pas des stats (capstones, fusions, formes fusionnées, étapes-événements : voir la classification) s'enregistrent dans une table :

```c
typedef struct { unsigned char trig, seedA, seedB; void (*fn)(const EvCtx *); } Hook;   /* seedB = -1 : pas de partenaire */
```

`trig` : tir, touche, kill, parry, dash, retombée, compétence utilisée. Le moteur appelle les hooks dont les graines sont équipées et la branche achetée.
Plus de `if (vgsg_cap(SH_ROQ, ...))` à l'intérieur du code des armes. Les étapes à état (Tournoi, Frénésie, Ruée, Joute en chaîne, Poings infatigables,
Cible unique) y gardent leur compteur : le sac ne porte que des valeurs.

## 3. Recettes

### 3.1 Ajouter une étape-stat (la stat existe et le code de l'arme la lit déjà)

1. Dans `docs/boutique_vagues/outils/effets/<famille>.js`, ajouter ou remplacer l'entrée `'GRAINE:Nom': { stat: 'TAI', op: 'mul', val: 1.15 }` (voir le format
   ci-dessus ; un tableau d'objets si l'étape modifie plusieurs stats).
2. `cd docs/boutique_vagues/outils && node export_c.js` : régénère `boutique_donnees.c` et refuse une stat, une opération ou une condition inconnue.
3. Compiler (zéro warning), ajouter une ligne de test (bloc « SAC DE STATS » de `src/tests/vagues_tests.c` : poser `shOwn[graine] = 1u << rang`, lire `st(...)`),
   lancer `--test`.

Zéro ligne de C. Si l'étape doit toucher une stat que le code de l'arme ne lit pas encore, voir 3.3.

### 3.2 Migrer une famille (modèle : le Shotgun, §4)

1. Lire la section de la famille dans `CLASSIFICATION_ETAPES.md` (§5) : les lignes « stat » et « stat cond. » se migrent, les « événement » et « comportement » restent
   comme elles sont (phase C). Le bloc de données, la fonction à brancher pour chaque stat et les fichiers touchés y sont donnés ; les stats, opérations,
   conditions et phases dont les trois familles restantes ont besoin **existent déjà** (préparation faite : 3.3 et 3.4 ne servent plus que pour une idée nouvelle).
2. Dans `effets/<famille>.js`, remplacer les entrées `['BK_...', id, b]` des étapes-stat par des objets (une ligne par modificateur ; `fixed: true` pour les
   entiers fixes ; `grp` quand le plus fort remplace les autres ; `phase` pour un ordre particulier).
3. Dans le code de l'arme, lire les stats **une fois par tir** dans une structure ou des variables locales (modèle `sg_stats` / `SgStats`) à la place des
   accesseurs et des crochets. Hors VAGUES, ne rien changer : `st()` rend la base. Ne pas lire par projectile ni par plomb.
4. Supprimer l'ancienne lecture de chaque étape migrée : un accesseur qui cherche l'étape par son genre répond « absente » une fois l'étape devenue `BK_STAT`, un
   accesseur qui la cherche par son rang (`roq_has(RQ_X)`) répond encore « présente » (effet compté deux fois). Supprimer aussi ce qui devient inutile : accesseurs,
   fonctions `static` sans appelant (`clang -Wall` du port macOS les signale), cas dans `sh_scalable` (`BK_STAT` s'en charge : une étape monte de niveau sauf si
   tous ses modificateurs sont `fixed`). Les identifiants `RL_*` / `RQ_*` / `CP_*` et les signatures publiques restent.
5. Adapter les appelants extérieurs si une signature change : formes fusionnées (`boutique_hybrides.c`), tests (`vagues_tests.c`).
6. `node export_c.js`, compiler, `--test` et comparer à la référence (tolérer les 5 lignes instables), match bot, mettre à jour la classification (✔) et la doc.

### 3.3 Ajouter une stat

1. Ajouter `STAT_X` dans l'`enum` de `src/monde/entites.c`, **avant `STAT_COUNT`** ; pas de commentaire contenant le mot `STAT_` ni d'accolade `}` dans l'enum
   (le générateur lit l'enum pour valider les noms). Documenter l'unité et la base dans le catalogue (`CLASSIFICATION_ETAPES.md`, §2).
2. Faire lire la stat par le code de l'arme : `x = st(SH_..., STAT_X, base)`. La **base** est la valeur du jeu sans boutique (le `#define`), jamais un
   neutre arbitraire : `st()` la rend telle quelle quand rien ne la modifie.
3. Une stat par canal : si le même paramètre se lit à plusieurs endroits du code qui ne se confondent pas (recharge après une joute, d'un estoc, d'un estoc
   dans le vide), une stat par endroit (`CDJ`, `CDE`, `CDV`), pas une condition.

### 3.4 Ajouter une opération ou une condition

- **Condition** : une valeur dans `enum COND_*` (`entites.c`), le champ de contexte dans `StatCtx`, un cas dans `st_reps` (`boutique_stats.c`) qui rend le
  nombre d'applications (0 : fausse, 1 : vraie, N : répétée), la validation du paramètre `par` dans `checkMod` (`export_c.js`) et la valeur de `cond` dans
  `effets/*.js` (le générateur lit l'enum). L'appelant remplit le champ. Un test avec des `StAct` construits à la main (`st_act(op, cond, val, par, grp, phase, fixed, échelle)`).
- **Opération** : une valeur dans `enum SOP_*`, un cas dans `st_scaled` (comment le delta se multiplie par le niveau) et dans `st_fold` (comment elle
  s'applique, dans quel ordre à l'intérieur d'une phase), et un test avec des `StAct` construits à la main (voir les lignes « sac de stats » des tests :
  `st_fold` ne dépend pas des données).
- **Phase** : le champ `phase` d'une ligne (0 à 3) ordonne l'application ; il suffit d'une valeur plus grande pour qu'une étape passe APRÈS les autres.

### 3.5 Pièges rencontrés

- `near` et `far` sont des **macros vides** de `windef.h` : ne jamais s'en servir comme noms de champs ou de variables (d'où `fallA` / `fallB` / `fallMin`).
- `ST_COUNT` existe déjà (`src/jeu/style.c`, jauge de style) : les stats s'appellent `STAT_*`.
- Une variable locale nommée `st` masque la fonction `st()` : ne pas en déclarer dans une fonction qui lit le sac.
- Les enums du sac sont dans `monde/entites.c` (pas dans `boutique_stats.c`) parce que `shotgun.c` & co sont compilés avant la boutique.
- Un test qui pose `shOwn` à la main n'a pas besoin d'appeler `sh_rebuild` (cache comparé à chaque lecture) ; mais `sh_roll` / `sh_swap` / `up_buy` consomment
  le hasard global (`frand`) : le noter si un test dépend de la suite aléatoire.
- Le sac a des tailles fixes : `ST_ACT_MAX` (240 lignes actives au plus, garde de compilation `st_chk_table` : toute la table `shMods` doit y tenir) et
  `ST_PER_STAT` (16 modificateurs au plus pour un couple graine / stat, vérifié par le générateur). Les dépasser est une erreur de compilation ou de génération.
- Les étapes d'une famille s'identifient par leur **rang** dans le pool (bit de `shOwn`, champ `step` de `shMods`) : l'enum `RQ_*` / `RL_*` / `CP_*` de la
  famille reste donc utile après la migration (événements, comportements, tests), même quand plus aucune étape-stat ne s'en sert pour lire une valeur.
- `bounce` est **base 1** (1 = premier rebond) alors que `depth` des fonctions de ricochet est base 0 : `c.bounce = depth + 1`.
- Un test qui doit construire un modificateur à la main utilise `st_act(...)` (jamais un initialiseur de `StAct` : la structure peut encore évoluer).

## 4. Famille de référence : le Shotgun

| Avant | Après |
|---|---|
| 21 fonctions `vgsg_*` dans `boutique_shotgun.c` (150 lignes) | 13 (94 lignes) : les capstones et les événements (Rafale, Peau de plomb, Souffle de culasse, Coup double) |
| `SHE_CHEVRO`, `SHE_GROS`, `SHE_RECHARGE`, `SHE_GERBE`, `SHE_CHERCHE`, `SHE_SERRE`, `SHE_PLUS`, `SHE_DCANON`, `SHE_POMPE`, `SHE_TRAQUE`, `SHE_VIVE` | supprimés ; restent `SHE_RAFALE`, `SHE_PEAU`, `SHE_CULASSE`, `SHE_COUP2` et les capstones |
| accesseurs `vgsg_half`, `vgsg_extra`, `vgsg_range`, `vgsg_size`, `vgsg_falloff`, `vgsg_cd_mul`, `vgsg_points_mul`, `vgsg_seek`, `vgsg_extra_dirs` | `sg_stats` / `SgStats` (une lecture du sac par tir), `sg_fall`, `sg_seek`, `sg_extra_dirs` dans `shotgun.c` ; `ammo_points` lit `st(SH_SHO, STAT_PTK, pts)` |
| 9 `vagOn` dans `shotgun.c` | 7 : les crochets d'événements et de capstones, plus le `return` de `sg_stats` |
| 14 étapes = 14 entrées `['BK_SHO', id, b]` / `BK_DMG` / `BK_NEAR` | 14 entrées `{ stat, op, val }` dans `effets/shotgun.js` (30 lignes de `shMods` pour les 26 étapes migrées) |

Les 14 étapes-stat du Shotgun et leurs modificateurs sont dans le commentaire de tête de `effets/shotgun.js` et dans `CLASSIFICATION_ETAPES.md`. Les 6
étapes qui ne sont pas des stats restent telles quelles : Rafale, Peau de plomb, Souffle de culasse, Coup double (genre `BK_SHO`), Éclats, Détonation (`BK_FX`).

Exemple « avant / après » (Gros plomb) : avant, `SHE_GROS` dans l'enum, `vgsg_size()` dans `boutique_shotgun.c`, `gros = vgsg_size()` dans `fire_shotgun`,
`['BK_SHO', 2, 15]` dans `effets/shotgun.js`, un test. Après : `'SHO:Gros plomb': { stat: 'TAI', op: 'mul', val: 1.15 }` ; `fire_shotgun` lisait déjà `S.size`
(`st(SH_SHO, STAT_TAI, 1.0f)` dans `sg_stats`) ; un test.

## 5. Plan de migration

0. **Gel** : fait. Référence = sortie complète de `--test` d'avant refactor (671 lignes) ; lignes instables d'une exécution à l'autre : les tirs « vises a la
   tete » des bots, « visee bot niveau N a 20 m : cadres », les positions de respawn du journal des bots, « part de lasers sur 400 tirs » de la sentinelle,
   « Rail jumeau N doubles sur 80 ». Toutes les autres lignes doivent rester identiques (seules les lignes ajoutées volontairement sont nouvelles).
1. **Socle** : fait (phase 1 ci-dessus), et la classification exacte des 140 étapes.
1b. **Préparation des familles restantes** : fait. Toutes les stats, opérations (`to`, `min`), conditions (`direct`, `bounce`, `first`, `salvefull`), phases et arrondis
   (`st_int`) dont elles ont besoin sont dans `entites.c` / `boutique_stats.c` / `export_c.js`, avec un test unitaire chacun ; le bloc de données de chaque étape est
   dans `CLASSIFICATION_ETAPES.md` (§5), vérifié de bout en bout (générateur, table, valeurs lues = valeurs des accesseurs actuels).
2. **Famille par famille** : Shotgun (fait), puis trois travaux indépendants, un par groupe de fichiers (`CLASSIFICATION_ETAPES.md`, §5 et §6) : Roquettes + Railgun,
   Lance + Impulsion, Pièce + All in One. Pour chacun : recette 3.2, `--test` identique, match bot, commit lisible.
3. **Fiche d'arme** (B) là où elle simplifie, puis **bus d'événements** (C) pour les capstones, fusions et étapes-événements.
4. **Documentation** : §3.5 de `docs/CODE.md`, `NOUVEAUTES.md`, cette page et la classification à chaque famille.

## 6. Garde-fous

- Hors `vagOn` : aucun changement de comportement ni de réseau ; `NET_VER` inchangé.
- Zéro warning ; pas d'allocation dynamique ; pas de nouvelle dépendance ; tout nouveau code a son équivalent macOS (rien de spécifique Win32 : seulement
  `powf`, `fabsf`, `fmaxf`, `memcmp`).
- Les étapes à état (Tournoi, Frénésie, Ruée, Joute en chaîne, Poings infatigables, Cible unique) gardent un hook d'événement : le sac ne porte que des valeurs.
- Mesures de réussite : nombre de fonctions d'accesseur (~105 : Shotgun 21 -> 13 pour l'instant), nombre de crochets `vagOn` dans `src/armes/` et
  `src/combat/` (~40, Shotgun 9 -> 7), et « ajouter une étape-stat = une entrée dans `effets/*.js`, zéro ligne de C » (vrai pour toute stat déjà lue).
