# Competences interchangeables avec la piece — design (v2)

Statut : **code** (les six competences, bots, tests, HUD, LOADOUT). Branche de travail : `dev`.
Remplace la v1 (qui supprimait les pieces) : ici la **piece devient une competence parmi d'autres**.

### Ce qui est code, choix faits et ecarts au design

- Les 7 competences (`SK_COIN` .. `SK_AIM`), une seule par loadout, valeurs de §3 (charges / recharge de
  `skillDef`). Description complete dans `docs/CODE.md` (section Competences).
- **Patch d'equilibrage (7 nov.)** : PIECE passe a 2 charges / 5 s (4 / 2 s avant) ; RICOCHET est reecrit en **fenetre** (§3 bis) ;
  ALL IN ONE marche avec la Tesla (un eclair, §3 ter) ; le **triangle MIRAGE => PIECE => RICOCHET => MIRAGE** (§3 quinquies).
- **MARQUE** : ×1,25 sur **toutes** les touches du marqueur (pas de « rail qui ricoche vers la marque suivante »).
  Marque perdue a la mort de la cible. Pas encore : contre-jeu « un parry retire la marque », tesla qui propage.
- **CHARGE** : ×1,5 sur la prochaine touche, quelle que soit l'arme (les autres touches du meme tir en
  profitent pendant 0,05 s : plombs, perforation). Pas de perforation en plus ni de bonus de portee de melee.
- **RICOCHET** (v2, fenetre de 6 s) : pendant la fenetre, rail, Elan, roquettes, plombs du shotgun et scie rebondissent
  **sans limite de rebonds ni de portee**, jusqu'a une cible ou la sortie de la carte (garde-fous techniques seulement :
  24 rebonds pour le rail / l'Elan, 6 par plomb, 40 pour une roquette qui vit 15 s, 12 pour la scie). Ni la mine, ni la
  tesla. Retour sur soi a partir du 2e rebond, **une seule fois par tir** (rail perpendiculaire entre deux murs : 50 PV en
  solo, sinon il tuait) ; un tir ne touche chaque cible qu'une fois. Une seule competence par loadout : la synergie
  RICOCHET + ALL IN ONE n'existe donc pas.
- **ALL IN ONE** : rail, roquettes, Shotgun (ex-Eventail), Elan ; **Tesla : un eclair inevitable** (§3 ter) ; refus (son,
  message, charge gardee) a 1 munition, avec la Scie / Ventouse, bouclier leve ou sous decharge Tesla. Le plafond PvP s'applique par projectile :
  le one-shot passe par plusieurs touches, compte dans « salves mortelles » (journal `equilibre`), pas
  dans « one-shots » (un seul coup).
- **MIRAGE** : leurre immobile 3 s (silhouette coloree du lanceur). Absorbe un rail, un Elan ou un plomb
  adverse (sauf face a RICOCHET, qui le renvoie a son poseur) et attire les ricochets de piece adverses ; mon rail y ricoche ×1
  vers la cible la plus proche. Un tir qui part de l'interieur du leurre le
  traverse (sinon : leurre a ses pieds + rail = visee automatique). Le yoyo, la scie et le tesla l'ignorent ;
  le viseur adverse ne s'allume pas dessus. Pas encore : « un coup de melee le detruit et rend la moitie de
  la recharge ».
- **Ancrage commun** (§4) : pas fait, chaque arme garde sa regle sur les pieces (les leurres en sont exclus).
- **Bots** : competence au hasard sans `--skill`. MARQUE 10–60 m, CHARGE quand une cible approche, RICOCHET
  arme si la cible est a couvert / pres d'un mur (tir par le sol si elle reste cachee), ALL IN ONE a 4–14 m
  avec ≥ 3 munitions (donc roquettes : le rail n'en a que 2), MIRAGE sous 45 PV ou juste apres une touche.
  Un bot vise un leurre adverse une fois sur deux, 0,4 a 1 s.
- Le choix de competence ne voyage pas dans l'etat joueur (les effets visibles passent par `EV_SKILL` et
  `EV_COIN` sous-type 4).

Match bot contre bot (90 s, un match par ligne, variance forte) :

| Match | TTK moyen | kills | one-shots | salves | parries | competences |
|---|---|---|---|---|---|---|
| `--skill 0` des deux cotes | 6,86 s | 7 | 0 | 0 | 15 | piece 4 |
| RICOCHET / CHARGE (hasard) | 5,73 s | 6 | 0 | 0 | 10 | ricochet 4 (3 tirs, 7 rebonds), charge 2 (2 bonifiees) |
| MARQUE / MIRAGE (hasard) | 7,37 s | 6 | 0 | 0 | 3 | marque 7 (4 bonifiees), mirage 2 |
| RICOCHET / PIECE (hasard) | 5,92 s | 7 | 0 | 0 | 7 | ricochet 3 (2 tirs, 6 rebonds), piece 4 |
| ALL IN ONE / MIRAGE (impose) | 3,40 s | 7 | 0 | 2 | 18 | all in one 3 (3, 4, 6 roquettes), mirage 3 |
| CHARGE / MARQUE (impose) | 0,82 s | 6 | 0 | 0 | 7 | charge 3 (3 bonifiees), marque 10 (7 bonifiees) |

A surveiller : ALL IN ONE aux roquettes (2 salves mortelles en 90 s) et le cumul MARQUE ×1,25 + CHARGE.

Patch d'equilibrage (7 nov.) : match bot contre bot **avant / apres**, 90 s, hote + client sur la machine, loadout identique des deux
cotes (`--loadout a,b,m,s`). **Un seul match par case (5 a 9 kills) : les moyennes de TTK ne signifient rien seules, il faut lire les compteurs.**

| Scenario | Version | Kills | TTK moyen | One-shots | Salves | Parries | A retenir |
|---|---|---|---|---|---|---|---|
| PIECE, rail + roquettes (`--skill 0`) | avant | 7 | 7,96 s | 0 | 0 | 2 | 9 pieces lancees, 3 touches de ricochet |
| | apres | 9 | 5,91 s | 0 | 2 | 3 | **5 pieces lancees** (2 charges / 5 s), 1 touche de ricochet |
| SHOTGUN + rail (`2,0,0,0`) | avant | 7 | 9,64 s | 0 | 1 | 0 | 6 touches d'Eventail |
| | apres (recul proportionnel a la vitesse, 2 matchs) | 7 et 8 | 6,45 et 7,93 s | 0 / 0 | 1 / 1 | 0 / 0 | **6 et 8 touches** de SHOTGUN (plafond de 60 par touche inchange) |
| | (1er essai, recul fixe de 5 m) | 7 | 0,41 s | 0 | 2 | 0 | 4 touches ; ce TTK de 0,41 s etait un hasard : 5 kills sur 7 etaient des combos epee + tir en moins de 0,4 s |
| RICOCHET, rail + roquettes (`0,1,0,3`) | avant | 7 | 3,24 s | 1 | 0 | 5 | 2 armements, 0 tir reflechi dans le match |
| | apres | 6 | 8,77 s | 0 | 0 | 5 | 3 fenetres, **4 tirs reflechis, 37 rebonds** (9 par tir), 0 retour sur soi |
| ELAN + rail (`3,0,0,0`) | avant | 7 | 8,21 s | 0 | 0 | 0 | 13 touches d'Elan (3 au depart, 6 au plus) |
| | apres | 8 | 1,50 s | 0 | 0 | 0 | 8 touches d'Elan (4 munitions, le tir freine tout) |
| TESLA + rail, ALL IN ONE (`6,0,0,4`) | avant | 8 | 8,13 s | 1 | 0 | 0 | 17 touches de Tesla, ALL IN ONE refuse avec la Tesla |
| | apres | 5 | 4,39 s | 0 | 1 | 0 | 10 touches de Tesla, **3 eclairs** |

Lecture : aucun match ne s'emballe ni ne cale (5 a 9 kills dans les onze). PIECE perd ~45 % de ses lancers (9 -> 5), ce qui justifie 2 charges /
5 s. RICOCHET fait ce qu'il doit (une fenetre = ~1 tir reflechi de 9 rebonds). L'Elan fait moins de touches (8 contre 13), attendu : un tir plein
exige maintenant de reprendre toute sa vitesse. Le SHOTGUN et la sentinelle ne se jugent pas sur un match de bots (plafond PvP inchange ; la
sentinelle n'existe qu'en VAGUES) : voir les lignes de `--test`.

## 1. Idee

Aujourd'hui le joueur a un emplacement fixe : la **piece** (`A_COIN`, charges `COIN_MAX`, recharge `COIN_RECHARGE`).
On le transforme en **emplacement de competence** : meme touche, meme jauge de charges au HUD, mais le
contenu est choisi dans l'ecran LOADOUT, comme l'arme de melee (`wantMelee` / `MK_*`).

```
loadout = 2 armes de tir (WP_*)  +  1 arme de melee (MK_*)  +  1 competence (SK_*)
                                                                 ^ nouveau, SK_COIN par defaut
```

Rien n'est perdu : **SK_COIN est exactement la piece actuelle** (ricochet, frappe de yoyo, scie, tesla,
contre-jeu). Le jeu actuel reste jouable a l'identique tant qu'on ne choisit pas autre chose.

## 2. Mecanique commune

- Une competence = `SK_*`, avec : `charges max`, `recharge`, `utiliser()`. Tout passe par la touche `A_COIN`
  (renommee `A_SKILL` dans le code ; l'ancien nom reste dans `velocity.ini` pour ne pas casser les touches).
- Etat joueur : `P.skill` (le `SK_*` choisi), `P.coins` devient `P.skillCharges`. Le reste (`COIN_MAX`,
  `COIN_RECHARGE`) devient une table par competence : `skillDef[SK_COUNT] = { nom, max, recharge }`.
- Choix au LOADOUT, applique au prochain respawn (`loadout_apply`), sauvegarde dans `velocity.ini`
  (cle `skill=`), comme `wantMelee`. Validation dans `loadout_valid`.
- HUD : les memes pastilles de charges, avec le nom de la competence.
- Reseau : `EV_COIN` generalise en `EV_SKILL` (`sub` = type de competence). Le choix de competence voyage
  dans l'etat joueur (comme `meleeKind`) pour que les autres voient le bon effet. **Incrementer `NET_VER`.**
- Bots : choisissent une competence au hasard parmi celles jouables, et savent utiliser SK_COIN et SK_MARK.

## 3. Catalogue de depart (a affiner)

| Competence | Utilisation | Effet | Charges / recharge |
|---|---|---|---|
| **PIECE** (`SK_COIN`) | appui | lance une piece ; le rail, l'Elan ou la roquette qui la touche ricochent (x1,5), yoyo et scie interagissent | **2 / 5 s** (4 / 2 s avant le patch d'equilibrage) |
| **MARQUE** (`SK_MARK`) | appui | tir sans degats : marque 4 s une cible, visible a travers les murs ; le rail qui touche une cible marquee ricoche vers la marque suivante (x1,25) | 2 / 6 s |
| **CHARGE** (`SK_CHARGE`) | maintenir 0,6 s | ouvre 2 s pendant lesquelles le prochain rail fait x1,5 et perce une cible ; perdue si on est touche | 1 / 8 s |
| **RICOCHET** (`SK_RICOCHET`) | appui | ouvre une **fenetre de 6 s** pendant laquelle tout ce que tu tires rebondit sans fin sur les surfaces. Voir §3 bis | 1 / 20 s (la recharge repart a la fermeture ; 15 s et « prochain projectile » avant) |
| **ALL IN ONE** (`SK_ALLIN`) | appui | tire **toute** la munition de l'arme en main d'un coup, en eventail ; plus il y en a, plus l'eventail est large. **Tesla : un eclair inevitable**. Voir §3 ter | 1 / 20 s (a regler) |
| **MIRAGE** (`SK_DECOY`) | appui | leurre immobile 3 s ; un rail qui le traverse ricoche vers la cible la plus proche (piece qui ne tombe pas) ; attire les ricochets de piece adverses | 2 / 7 s |

### 3 bis. RICOCHET v2 (patch d'equilibrage : « une duree pendant laquelle tous les projectiles rebondissent a l'infini »)

Remplace la v1 (« arme le prochain projectile, 3 rebonds, 120 m, 15 s » : `P.ricArmed` consomme au premier tir).

- **Fenetre** : appui = `P.ricT = RIC_DURATION` (6 s, `P.ricArmed` en est le miroir pour le HUD, les bots et la boutique de VAGUES). Le HUD
  affiche `RICOCHET 4.2 s`. Pas de 2e ouverture pendant la fenetre. Charge : 1 ; recharge **20 s qui repart a la fermeture**
  (`skillDef`, a mesurer : cycle de 26 s, 23 % du temps ouvert). Mort et respawn ferment la fenetre.
- **Rail et Elan** (`hitscan_shot`) : a chaque mur, reflexion `r = d - 2 (d.n) n`. **Aucun plafond de portee** : un segment de
  `RIC_SEG_LEN` = 400 m sans mur veut dire « sorti de la carte » (kill zone, carte ouverte) et le tir s'arrete. Garde-fou technique
  `RIC_CAP_BOUNCES` = 24 (une carte fermee ne laisse jamais sortir un tir ; cout CPU et traits a l'ecran). Le tir s'arrete aussi sur une
  piece, une mine ou un bouclier. **Une touche par cible et par tir** (`RIC_HITS_MAX`) : sans cela un tir qui recroise sa cible
  la toucherait a chaque aller-retour (2 x 60 = un one-shot imprevu). Retour sur soi a partir du 2e rebond, une seule fois par tir (`W_SELF` :
  moitie des degats en solo, x `PVP_SCALE` plafonne en LAN).
- **Roquette** : `ricLeft` = `RIC_ROCKET_CAP` (40), **aucune perte de vitesse** (`RIC_ROCKET_SLOW` 1, 0,8 en v1), vie `RIC_ROCKET_LIFE`
  15 s (6 s sinon) : elle rebondit jusqu'a une cible, a sa fin de vie ou jusqu'a sortir de la carte.
- **Plombs du SHOTGUN** : chaque plomb rebondit, `RIC_PELLET_CAP` = 6 rebonds, `SG_RANGE` par segment, degats selon la distance totale parcourue
  (donc faibles loin). **Scie** : elle rebondit sur le decor au lieu de se planter (`RIC_SAW_CAP` = 12, `SAW_FLY_MAX` l'arrete aussi).
  **Ni la mine, ni la Tesla** (la mine colle, l'arc n'est pas un projectile).
- **Interactions** : une piece, un mirage ou une marque touches apres un rebond jouent comme avant. RICOCHET est la competence du triangle
  qui contre MIRAGE (§3 quinquies).
- **Reseau** : le tir est valide chez le tireur. **Tous les rebonds d'un tir tiennent dans un seul evenement** (`EV_RAIL` sous-type 2 :
  depart et direction du 1er segment reflechi, nombre de segments) : chaque client recalcule la trace sur le decor, jamais un evenement par
  segment (24 par tir saturaient la file des 128 evenements). `EV_ROCKET.sub` = rebonds restants, `EV_SHOT.pad[0]` = rebonds des plombs,
  `EV_SKILL` `SK_RICOCHET` = ouverture. `PVP_HIT_CAP` inchange. **`NET_VER` 21.**
- **Tests** (`--test`, lignes « ricochet ») : fenetre ouverte / encore ouverte a 5 s / fermee a 6,1 s, incidence = sortie (< 0,5 deg), tir sorti du
  labo (6 segments, 5 rebonds), plafond de 24 rebonds entre deux murs paralleles, une touche par cible, roquette (vitesse constante, vie 15 s),
  plombs derriere le rebond, scie, trace recalculee par les autres, recharge de 20 s apres la fermeture.

### 3 ter. ALL IN ONE (choix du proprietaire : « toutes les munitions d'un coup, spread croissant »)

- **Effet** : appui = l'arme en main (rail ou roquettes) vide sa munition `n` (`P.ammo[...]`) en `n`
  projectiles tires au meme tick, repartis dans un cone autour de la visee. Au moins 2 munitions requises
  (avec 1, ce serait un tir normal sans le cout de la recharge : refuse avec un son d'erreur).
- **Spread** : demi-angle du cone `a = ALLIN_SPREAD_BASE + ALLIN_SPREAD_PER_AMMO * (n - 1)`, valeurs de depart
  `ALLIN_SPREAD_BASE` = 0,8 deg et `ALLIN_SPREAD_PER_AMMO` = 0,7 deg (n = 2 : ~1,5 deg ; n = 6 : ~4,3 deg ;
  plafond `ALLIN_SPREAD_MAX` = 6 deg). Les projectiles sont repartis en eventail regulier (pas aleatoire),
  avec un decalage aleatoire commun d'au plus la moitie de l'ecart, pour que le tir reste previsible
  et lisible.
- **Equilibrage (decision du proprietaire)** : **le tir peut one-shot en PvP**. Chaque projectile garde ses
  degats et le plafond `PVP_HIT_CAP` s'applique par projectile, pas a la somme. Le contrepoids est le
  drawback : toute la munition de l'arme disparait, la recharge de la competence est longue (20 s), l'eventail
  s'ouvre avec le nombre de projectiles (donc le one-shot n'arrive qu'a courte portee ou avec peu de munition),
  et l'arme est vide ensuite. A mesurer au match bot : « one-shots » du journal d'equilibre.
- **Interactions** : fonctionne avec RICOCHET (chaque projectile rebondit : un seul armement pour tout
  l'eventail, ce qui donne le tir « disco » en espace clos, a surveiller pour l'auto-degats) et avec les
  ancrages (chaque projectile peut en toucher un). Pas d'effet sur le yoyo, la scie ni l'epee.
- **Reseau** : tir valide chez le tireur, un seul `EV_SKILL` (`sub = SK_ALLIN`, nombre de projectiles et
  graine du decalage) pour que les autres joueurs dessinent le meme eventail. **Incrementer `NET_VER`.**
- **Tests a ajouter** : n projectiles pour n munitions, munition a zero apres le tir, demi-angle conforme
  a la formule (n = 2, 4, 6), refus a 1 munition, plafond PvP applique par projectile (un tir a courte portee peut one-shot), recharge.

- **Tesla : un eclair inevitable** (patch d'equilibrage, `tesla_bolt`) : la Tesla est un faisceau, pas un projectile, donc pas de
  « toute la munition en eventail » ; ALL IN ONE lance a la place **un eclair instantane** (aucun delai de vol, rien a esquiver) sur la
  cible la plus proche du viseur dans un cone de ~10 deg (`TESLA_BOLT_CONE`), en vue, a 45 m. Il saute ensuite comme l'arc
  (2 cibles de plus a 6 m, x0,6). **Il vide toute la batterie** (`TESLA_BOLT_MIN` = 20 au moins, sinon refus sans depense) et fait
  `TESLA_BOLT_DMG` = 130 x batterie / 100 en PvE (65 a demi-batterie) ; en PvP x `PVP_SCALE` puis le plafond de 60 de l'hote (donc ~71 % de
  batterie suffisent), arme `W_TESLA` : la cible peut toujours le parer comme une decharge. Hypotheses a confirmer : cout = toute la batterie,
  degats proportionnels, pas de verrou au-dela du cone. Reseau : `EV_SKILL` `SK_ALLIN` avec `actor` = 1 (`net_fx_bolt`).

Les autres competences n'ont pas besoin de projectile physique propre : MARQUE et CHARGE sont des etats,
MIRAGE reutilise l'entite `Coin` (immobile, sans gravite). C'est ce qui rend l'etape 1 peu risquee.

## 3 quater. Regle de synergie : chaque competence marche avec (presque) toutes les armes

Armes de tir : rail, roquettes, shotgun, kinetic, scie, mines, tesla. Melee : epee, enclume, lance, yoyo,
poings, egide. Une competence **ne doit pas etre pensee pour une seule arme** : elle agit sur « le prochain
tir » ou sur « le prochain coup », avec une regle par famille. Une case vide = pas de synergie voulue
(une competence peut ignorer au plus 2 armes, a justifier).

| Competence | Armes de tir | Melee | Ignore |
|---|---|---|---|
| **PIECE** | rail et kinetic ricochent (x1,5) ; **roquette : redirigee vers l'ennemi le plus proche (x1,5, patch)** ; shotgun, tesla la touchent | yoyo frappe la piece ; epee et lance la renvoient (parry de projectile) | poings, egide |
| **MARQUE** | tout tir qui touche une cible marquee : +25 % (rail, shotgun, kinetic) ou souffle +20 % (roquette, mine) ; tesla propage la marque | tout coup de melee sur une marquee : +25 % ; yoyo : rien de plus (il ne tire plus la cible, patch d'equilibrage) ; egide : le coup de bouclier la marque | scie (traverse, voir §4) |
| **CHARGE** | le prochain tir : x1,5 et perce ; scie : traverse sans s'arreter ; mine : rayon x1,3 | le prochain coup : x1,5, portee +20 % | — |
| **RICOCHET** | fenetre de 6 s : rail, kinetic, plombs du shotgun, roquette et scie rebondissent a l'infini (garde-fous techniques) ; mine et tesla : non (la mine colle, l'arc n'est pas un projectile) | — (rien a faire rebondir) | les 6 armes de melee |
| **ALL IN ONE** | vide la munition de l'arme de tir en main en eventail (rail, roquette, shotgun, kinetic ; mines posees en ligne ; **tesla : un eclair inevitable**) | — | scie (1 seule), melee |
| **MIRAGE** | tout tir qui traverse le leurre est redirige vers la cible la plus proche | tout coup de melee qui touche le leurre le detruit et rend la moitie de la recharge | — |

Verification : les competences de la ligne « Ignore » doivent rester a 2 armes ou moins ; RICOCHET et
ALL IN ONE sont volontairement des competences de tir (elles n'ont pas de sens au corps a corps) : si on
choisit un loadout de melee seul, on propose a la place MARQUE, CHARGE ou MIRAGE.

## 3 quinquies. Le triangle MIRAGE => PIECE => RICOCHET => MIRAGE (pierre-feuille-ciseaux)

Choix du proprietaire : « c'est plus comme un pierre feuille ciseaux ». Ce n'est **pas** une synergie de loadout (une seule competence par
loadout) : chaque competence en **contre** une autre, a sens unique. Chaque fleche se coupe par une constante de `equilibrage.c`.

| Fleche | Regle | Constante |
|---|---|---|
| MIRAGE => PIECE | le ricochet d'une piece (rail, Elan, roquette) vise le **leurre adverse** en vue (90 m) **avant** le joueur : le leurre l'absorbe, le joueur n'est pas touche (« les pieces suivent le mirage et non le joueur ») | `DECOY_COIN_LURE` |
| RICOCHET => MIRAGE | si j'ai RICOCHET, mon tir (rail, Elan, plombs) sur le leurre adverse repart vers le **joueur qui l'a pose** (en vue du leurre, 90 m, memes degats, plafond PvP) ; sans ligne de vue, le leurre absorbe | `DECOY_RIC_ALWAYS` (1 : des qu'il est equipe) |
| PIECE => RICOCHET | **lecture par defaut, le proprietaire n'a « rien de particulier »** : un projectile en rebond (fenetre RICOCHET ouverte) qui touche une piece **adverse** repart vers **son tireur** au lieu de la voler (rail / Elan : touche comme un rebond qui revient ; roquette : revient et explose sur lui, 30 x f) | `COIN_VS_RIC` |

Le yoyo, la scie et la Tesla continuent d'ignorer le leurre. Les roquettes ignorent le leurre aussi (elles le traversent), sauf quand une
piece les y envoie : elles s'y ecrasent.

## 4. Interactions avec les autres armes

Le yoyo, la scie, le tesla et l'egide ne reagissent plus a « une piece » mais a un **point d'ancrage**
(`anchor`) : piece, mirage ou cible marquee. Une seule fonction de recherche d'ancrage, appelee par
`flail_strike_coin`, la scie et le tesla. Ainsi une nouvelle competence qui cree un ancrage fonctionne
d'office avec toutes les armes.

Contre-jeu : un parry parfait retire la marque de l'attaquant ; une piece ou un mirage adverse se
« vole » comme aujourd'hui (message `PIECE VOLEE !`).

## 5. Plan par etapes (chacune compilable, testee)

1. **Emplacement generique** : `SK_COIN` seul, tables `skillDef`, choix dans le LOADOUT, `velocity.ini`,
   `EV_SKILL`. Comportement identique a aujourd'hui (les tests existants doivent rester verts).
2. **Ancrage commun** (`anchor`) pour yoyo / scie / tesla, sans changer le comportement.
3. **SK_MARK**, avec lignes de test (`marque : duree, portee`, `marque + rail : x1,25`, contre-jeu parry).
4. **SK_CHARGE**, puis **SK_DECOY**.
5. Bots, HUD, captures (`--shot` du loadout), match bot contre bot **avant / apres**.
6. Fusion `dev` -> `stable` apres verification complete.

## 6. Questions ouvertes

Tranche par le proprietaire : **une seule competence par loadout** ; ALL IN ONE peut one-shot (drawback
fort) ; toute competence doit synergiser avec (presque) toutes les armes (§3 quater).

- Chaque competence a sa propre jauge de charges (par `skillDef`) : OK ?
- Mode solo : toutes les competences disponibles des le debut, ou deblocage par vague ?
- Le tableau de synergies (§3 quater) : les valeurs (+25 %, x1,3, ...) sont des points de depart a mesurer.
