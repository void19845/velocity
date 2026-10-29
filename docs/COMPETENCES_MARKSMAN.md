# Competences interchangeables avec la piece — design (v2)

Statut : **proposition, rien n'est code**. Branche de travail : `dev`.
Remplace la v1 (qui supprimait les pieces) : ici la **piece devient une competence parmi d'autres**.

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
| **PIECE** (`SK_COIN`) | appui | lance une piece ; le rail qui la touche ricoche (x1,5), yoyo et scie interagissent | 4 / 2 s (actuel) |
| **MARQUE** (`SK_MARK`) | appui | tir sans degats : marque 4 s une cible, visible a travers les murs ; le rail qui touche une cible marquee ricoche vers la marque suivante (x1,25) | 2 / 6 s |
| **CHARGE** (`SK_CHARGE`) | maintenir 0,6 s | ouvre 2 s pendant lesquelles le prochain rail fait x1,5 et perce une cible ; perdue si on est touche | 1 / 8 s |
| **RICOCHET** (`SK_RICOCHET`) | appui | arme le prochain projectile tire (rail ou roquette) : il rebondit sur les surfaces au lieu de s'arreter. Voir §3 bis | 1 / 15 s |
| **ALL IN ONE** (`SK_ALLIN`) | appui | tire **toute** la munition de l'arme en main d'un coup, en eventail ; plus il y en a, plus l'eventail est large. Voir §3 ter | 1 / 20 s (a regler) |
| **MIRAGE** (`SK_DECOY`) | appui | leurre immobile 3 s ; un rail qui le traverse ricoche vers la cible la plus proche (piece qui ne tombe pas) | 2 / 7 s |

### 3 bis. RICOCHET (choix du proprietaire : « le prochain projectile rebondit sur les surfaces », 15 s)

- **Armement** : appui sur la touche, `P.ricArmed = 1` (indicateur au HUD et sur le viewmodel). La competence
  est consommee au premier projectile tire ; si on change d'arme, elle reste armee. Recharge de 15 s qui
  demarre **au tir** (pas a l'armement), pour ne pas bloquer le joueur qui arme sans tirer.
- **Rail** : au lieu de s'arreter au mur, le tir est reflechi (`r = d - 2(d.n)n`), jusqu'a `RIC_BOUNCES` = 3
  rebonds, portee totale plafonnee a `RIC_MAX_LEN` = 120 m. Touche chaque cible traversee, sans bonus
  de degats par rebond au depart (le gain est la couverture, pas la puissance), perce toujours selon la
  regle du rail. Un joueur peut se toucher lui-meme par un rebond : degats `W_SELF` a partir du 2e rebond,
  c'est le risque de la competence.
- **Roquette** : rebondit (vitesse x0,8 par rebond, `RIC_BOUNCES` rebonds) et n'explose qu'au contact d'une
  cible ou au 4e impact.
- **Scie** : rebondit deja ; l'armer n'ajoute que +1 renvoi dans `SAW_BOUNCE_CHAIN`.
- **Interactions** : un rail qui ricoche touche aussi une piece / marque / mirage (ancrage commun, §4) apres
  rebond, ce qui permet les tirs « sans ligne de vue » ; le yoyo et le tesla ne sont pas concernes.
- **Reseau** : le tir est valide chez le tireur, comme aujourd'hui ; `EV_SKILL` (`sub = SK_RICOCHET`) porte
  le fait que le tir est reflechi, et la trace (`Beam`) est dessinee avec ses segments. Le plafond PvP par
  touche (`PVP_HIT_CAP`) reste inchange. **Incrementer `NET_VER`.**
- **Tests a ajouter** : reflexion sur un mur (angle d'incidence = angle de sortie, a 0,5 deg pres), 3 rebonds
  maximum, recharge 15 s apres le tir, projectile non arme = comportement actuel, auto-degats au 2e rebond.

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

Les autres competences n'ont pas besoin de projectile physique propre : MARQUE et CHARGE sont des etats,
MIRAGE reutilise l'entite `Coin` (immobile, sans gravite). C'est ce qui rend l'etape 1 peu risquee.

## 3 quater. Regle de synergie : chaque competence marche avec (presque) toutes les armes

Armes de tir : rail, roquettes, shotgun, kinetic, scie, mines, tesla. Melee : epee, enclume, lance, yoyo,
poings, egide. Une competence **ne doit pas etre pensee pour une seule arme** : elle agit sur « le prochain
tir » ou sur « le prochain coup », avec une regle par famille. Une case vide = pas de synergie voulue
(une competence peut ignorer au plus 2 armes, a justifier).

| Competence | Armes de tir | Melee | Ignore |
|---|---|---|---|
| **PIECE** | rail ricoche (x1,5) ; roquette, shotgun, kinetic, tesla la touchent et partent vers une cible | yoyo frappe la piece ; epee et lance la renvoient (parry de projectile) | poings, egide |
| **MARQUE** | tout tir qui touche une cible marquee : +25 % (rail, shotgun, kinetic) ou souffle +20 % (roquette, mine) ; tesla propage la marque | tout coup de melee sur une marquee : +25 % ; yoyo la tire ; egide : le coup de bouclier la marque | scie (traverse, voir §4) |
| **CHARGE** | le prochain tir : x1,5 et perce ; scie : traverse sans s'arreter ; mine : rayon x1,3 | le prochain coup : x1,5, portee +20 % | — |
| **RICOCHET** | rail, shotgun, kinetic : reflechis ; roquette : rebondit ; mine : roule et rebondit ; scie : +1 renvoi ; tesla : l'arc rebondit | — (rien a faire rebondir) | les 6 armes de melee |
| **ALL IN ONE** | vide la munition de l'arme de tir en main en eventail (rail, roquette, shotgun, kinetic ; mines posees en ligne ; tesla : arcs en eventail) | — | scie (1 seule), melee |
| **MIRAGE** | tout tir qui traverse le leurre est redirige vers la cible la plus proche | tout coup de melee qui touche le leurre le detruit et rend la moitie de la recharge | — |

Verification : les competences de la ligne « Ignore » doivent rester a 2 armes ou moins ; RICOCHET et
ALL IN ONE sont volontairement des competences de tir (elles n'ont pas de sens au corps a corps) : si on
choisit un loadout de melee seul, on propose a la place MARQUE, CHARGE ou MIRAGE.

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
