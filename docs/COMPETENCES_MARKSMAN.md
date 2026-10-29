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

Les autres competences n'ont pas besoin de projectile physique propre : MARQUE et CHARGE sont des etats,
MIRAGE reutilise l'entite `Coin` (immobile, sans gravite). C'est ce qui rend l'etape 1 peu risquee.

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

- Une seule competence a la fois (propose), ou deux emplacements (piece + autre) ?
- Les competences sont-elles toutes dans le meme budget de charges, ou chacune a sa jauge ?
- Mode solo : toutes les competences disponibles des le debut, ou deblocage par vague ?
- Equilibrage PvP : les competences non-piece doivent-elles etre plafonnees par `PVP_HIT_CAP` (60 PV) ?
