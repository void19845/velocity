# Armes, competences et boutique du mode VAGUES

Deux parties : **I. Stats et fonctionnement des armes** (`src/reglages/equilibrage.c`, `src/armes/`), puis **II. Boutique du mode VAGUES** (`src/solo/ameliorations.c`).

---

# I. Armes : stats et fonctionnement

Sources : `src/reglages/equilibrage.c` (valeurs), `src/armes/*.c`, `src/combat/` (rail, roquettes, epee) et `docs/CODE.md`.
Le loadout compte 2 armes a distance + 1 arme de melee + 1 competence. Les degats ci-dessous sont les degats PvE ;
en PvP ils sont multiplies par `PVP_SCALE` (0,7) et plafonnes a `PVP_HIT_CAP` (60 PV par touche), sauf tete (rail, Elan)
et one-shot.

## Regles communes

- **Vitesse relative** (`rel`, en m/s) : la plupart des armes de melee gagnent degats et portee avec la vitesse relative
  entre toi et la cible.
- **Chaine de tirs** (rail, roquette, Eventail, Elan) : chaque touche successive donne +15 % de degats (`STREAK_DMG`) et
  +5 % de cadence (`STREAK_RATE`), jusqu'a 8 touches (`STREAK_MAX`). Un tir rate remet a zero. Chaine pleine : le
  prochain tir de rail ou d'Elan qui touche un joueur le tue (one-shot, arbitre par l'hote). Une touche a la tete avance
  la chaine de 2. Le Tesla, la Scie et la Ventouse n'y comptent pas.
- **Jauge de munitions** : aucune recharge passive. Chaque arme a une jauge de points, remplie par la melee et les
  cellules ; jauge pleine = 1 munition.
  - Points de melee par coup qui touche : EPEE 50, ENCLUME 75, LANCE 50 (estoc), YOYO 35, IMPULSION 25, EGIDE 50 (coup de bouclier).
  - Kill en melee +100, parry +50 (Egide : parry parfait +100), petite cellule +100, grande cellule +150.
- **Parry** : fenetre courte au debut d'un coup de melee ; renvoie les orbes et roquettes (+10 PV en solo, +1 dash,
  grappin recharge).

## Armes a distance

| Arme | Munitions (depart / max) | Points par munition | Recharge (tir) | Degats |
|---|---|---|---|---|
| RAILGUN | 2 / 2 | 250 | 0,85 s | 100 |
| ROQUETTES | 4 / 8 | 50 | 0,75 s | 110 (souffle 4,5 m) |
| SHOTGUN (ex-EVENTAIL) | 3 / 6 | 100 | 0,95 s | 10 plombs x 20 (x 8 avant le patch d'equilibrage) |
| ELAN | 4 / 4 | 80 | 0,5 s | 20 + 5 par m/s depense |
| SCIE | 1 / 1 | aucune jauge | 0,4 s (entre deux lancers) | 30 + 1,5 par m/s au-dela de 12 |
| VENTOUSE | 2 / 3 | 150 | 0,55 s | 90 (souffle 4 m) |
| TESLA | batterie 60 / 100 | 2,5 points par unite | 0,1 s (10 impulsions/s) | 7 par impulsion |

### RAILGUN
- Hitscan depuis l'oeil jusqu'au mur (400 m), **perforant** : touche tous les ennemis et joueurs avant le mur.
- S'arrete sur la premiere piece traversee (de n'importe qui) et declenche le **ricochet** de piece.
- Degats `100 x streak_mult` ; a la tete : x1,5 et **one-shot**, sans plafond PvP. Recharge `0,85 / streak_rate`.
- A vide, le tir bascule sur les roquettes.

### ROQUETTES
- Projectile de vitesse 38 m/s, explosion de rayon 4,5 m, 110 degats, recul 13.
- Peut etre renvoyee par un parry (x1,25 au premier renvoi, puis +15 % de vitesse et de degats a chaque re-parry).
- 1 coup d'epee = 1 roquette (50 points par munition).

### SHOTGUN (ex-EVENTAIL, fusil a pompe)
- 10 plombs en motif deterministe : 1 au centre, 3 sur un anneau a 0,45, 6 sur un anneau a 0,9 ; cone de 9 degres
  (4,5 degres de demi-angle), divise par 2 en slide.
- 20 degats par plomb (8 avant le patch) : degats pleins jusqu'a 10 m, puis decroissance lineaire jusqu'a x0,3 a 30 m ; portee 60 m.
  Le plafond PvP de 60 PV par touche et le x0,7 sont inchanges : pas de one-shot, le kill en 2 tirs passe de ~4 m a ~14 m.
- Recul du tireur proportionnel a sa vitesse juste avant le tir : 1 m/s de poussee par m/s (`SG_KNOCK_RATIO`), plafond 60 m/s (`SG_KNOCK_MAX`),
  pendant 0,10 s, dans le sens inverse du tir, au sol comme en l'air ; a l'arret il n'y a aucun recul (`SG_KNOCK_MIN`, 0 par defaut, pour un
  minimum). En PvP la cible recoit la meme poussee dans le sens du tir, multipliee par la part de plombs qui la touchent ; les ennemis du mode
  Vagues gardent une vitesse fixe (`SG_PELLET_KNOCK`). Le saut au recul marche toujours (residu de 7 m/s en l'air).
- Un plomb qui touche une piece la fait eclater (souffle 3 m, 45 degats, recul 8, eclate les pieces voisines jusqu'a 3 de profondeur).
- Les degats sont agreges par cible : un seul `net_hit` par joueur et par tir. 3 plombs touches minimum comptent dans la chaine.

### ELAN (convertisseur cinetique)
- Le tir convertit la vitesse horizontale (15 m/s au plus) en degats : `20 + 5 x v`, soit 20 a l'arret et 95 a 15 m/s ; le tir retire toute la vitesse horizontale du tireur (`KIN_BRAKE_ALL`, la vitesse verticale reste).
- Recul de la cible : 3 a l'arret, +0,4 par m/s depense. Tir « plein » a partir de 14,5 m/s (hitstop, son, rayon max).
- **Dash-tir** : dans les 0,10 s apres un dash, toute la vitesse compte ; ensuite, jusqu'a 0,6 s, la vitesse ajoutee par le dash est ignoree.
- Tir a la tete : x1,35 sans plafond PvP (pas de one-shot, 90 PV au plus).

### SCIE (disque-boomerang)
- Une seule lame, pas de munitions : la scie est sa propre munition. Vitesse de lancer 32 m/s (plancher 20) plus ta vitesse, gravite legere.
- Degats `30 + 1,5 x max(0, rel - 12)` : environ 60 PvE / 42 PvP sur une cible immobile. Apres une touche, retour guide (36 m/s).
- Plantee dans le decor : il faut aller la chercher a pied (rayon 1,5 m) ou la **rappeler au grappin** (45 m/s, sans bouger).
- Au retour, on peut la **parer** pour la renvoyer plus vite (x1,5 de la vitesse de lancer, max 70 m/s, 3 renvois d'affilee, aide a la visee 7 degres, degats x0,9).
- Frole une piece : la piece est lancee en l'air (11 m/s verticale, +2 s de vie). Perdue sous la carte : revient apres 3 s.

### VENTOUSE (mines collantes)
- Lancee a 24 m/s + ta vitesse, gravite 16 ; colle au decor, a un drone ou a un joueur. **2 mines actives au plus** (la 3e fait sauter la plus ancienne).
- Detonation a distance (toutes, ou la plus ancienne) ou meche de 3 s depuis le collage. Une mine qui ne colle a rien explose en l'air apres 3 s.
- Sur un joueur : armement de 0,6 s (le temps de l'arracher par un parry), puis 60 PvP garantis.
- Explosion : rayon 4 m, 90 degats au centre (30 % au bord), recul 15. Un rail ou un ricochet la fait sauter x1,5 (rayon 5 m), un plomb ou l'arc x1, meme celle d'un adversaire.
- **Piege** : une mine posee sur le decor explose seule si un adversaire passe a moins de 2 m, 0,4 s apres le collage.

### TESLA (arc electrique)
- Faisceau continu tant que le tir est tenu : 10 impulsions par seconde, 2 de batterie chacune (20 par seconde), aucune recharge passive.
- Portee 14 m + 0,3 par m/s de vitesse, 28 m au plus. Les cibles grossissent de 0,4 m (aide a la visee en cylindre).
- Chaine vers 2 cibles de plus a 6 m de distance maximale, a 60 % des degats.
- **Relais sur piece** : une piece traversee devient un relais (2 s, rayon 6 m, 5 degats ; 3 relais au plus par joueur). Ne s'ajoute pas a l'arc.
- Degats cumules par cible et envoyes toutes les 0,2 s. Decharge parree : le tireur est etourdi 0,4 s (plus de tir a distance) et prend 15 en PvP direct.
- Ne compte pas dans la chaine de touches.

## Melee

| Arme | Degats | Portee | Recharge | Specifique |
|---|---|---|---|---|
| EPEE | 35 + 4 par m/s | 2,4 m + 0,12 par m/s | 0,45 s (0,9 s dans le vide) | cone 0,55 (cosinus) |
| ENCLUME | 45 + 3 par m/s au sol | 2,6 m + 0,10 par m/s | 0,8 s (1,2 s dans le vide) | pique et onde de choc en l'air |
| LANCE | estoc 30 + 2 par m/s ; joute 10 + 5 x v_c | estoc 3 m ; lance couchee 3,5 m | estoc 0,5 s ; joute reussie 1 s | joute automatique |
| YOYO (fleau) | 18 + 1,5 par m/s | 6 m + 0,05 par m/s | 0,6 s (0,9 s dans le vide) ; 0,5 s apres une traction | arc de 140 degres ; appui long : tire les cibles tant que la touche est tenue |
| IMPULSION (poings) | 14 + 1,5 par m/s ; uppercut 20 + 2,5 | 1,9 m + 0,06 par m/s | 0,22 s entre coups, 0,7 s apres le 3e | combo de 3, punch-jump |
| EGIDE (bouclier) | coup de bouclier 25 + 3 par m/s | 2,2 m | 0,5 s (0,8 s dans le vide) | bloque de face, jauge de 120 |

### EPEE
- Coup rapide a cone frontal ; degats et portee croissent avec la vitesse relative.
- **Parry** : actif seulement au tout debut du coup (0,12 s). Renvoie orbes et roquettes (vitesse 36 m/s, 150 degats en solo, aide a la visee ~9 degres).
- En PvP, l'hote arbitre le parry : l'attaquant pare est repousse et son epee bloquee 1 s ; deux parries simultanes donnent un choc d'epees.

### ENCLUME (marteau-pilon)
- **Au sol** (ou en l'air trop pres du sol) : coup lourd frontal, recul 14 vers l'avant et 6 vers le haut. Fenetre de parry 0,10 s.
- **En l'air** (au moins 1,2 m de hauteur) : **pique** a 30 m/s minimum (plafond 45), puis **onde de choc** a l'atterrissage.
  - Rayon `2,5 + 0,3 x H` (2,5 a 9 m) ; hauteur du mur `0,6 + 0,1 x H` (0,6 a 2,6 m) ; H = hauteur de chute. Sauter au passage du front l'esquive.
  - Etiree en ovale vers l'avant avec la vitesse horizontale (x1,8 au plus), raccourcie vers l'arriere (x0,6 au moins).
  - Degats `(25 + 1,5 x vitesse de chute) x kh` ; `kh` va de 0,5 a 1 (100 % des degats a partir de 4 m de chute).
  - Le front se propage a 28 m/s ; chaque cible est touchee une fois, si ses pieds sont sous le mur.
  - Onde qui touche : rebond (9) et dash. Onde qui rate : immobilisation de 0,6 s + 0,02 s par metre de chute (1 s au plus).
  - Recharge 0,8 s apres l'atterrissage. Une poussee vers le haut (vy < -15) casse le pique.

### LANCE (joute)
- **Estoc** : a l'appui (fenetre de parry normale). 30 + 2 par m/s, portee 3 m, cone plus etroit que l'epee.
- **Lance couchee** : touche tenue plus de 0,15 s, jusqu'a 2 s, sans parry. A chaque tick, la premiere cible du cone (~23 degres, 3,5 m) qui se rapproche a au moins 10 m/s est touchee toute seule : `10 + 5 x v_c`, v_c plafonne a 30 (160 PvE).
- La cible recoit 60 % de ta vitesse, tu en gardes la moitie ; recul max 28.
- Choc de joute : l'ecart de vitesse de 2 m/s decide ; les deux joueurs desarconnes sont repousses de 11.
- Recharges : joute reussie 1 s, lance couchee sans touche 0,5 s, lance paree ou joute perdue 1,5 s. Le grappin sert de monture : la joute part avant l'impact du crochet et le consomme.

### YOYO (fleau a chaine)
- Coup en arc horizontal de 140 degres, 6 m (+0,05 par m/s), sur toutes les cibles de l'arc ; ne traverse pas le decor.
- Faibles degats. **Appui court : le balayage, sans traction** (la cible touchee ne bouge pas ; retrait decide par le proprietaire le 2026-11-07).
- **Appui long : la traction est de retour** (le proprietaire, le 2026-11-10 : « on la garde, mais sur un appui long, et ca tracte aussi longtemps que le
  joueur maintient »). Touche de melee tenue plus de 0,15 s : le coup en arc part quand meme a l'appui (reactivite, fenetre de parry), puis la chaine
  s'accroche aux cibles de l'arc **a cet instant** (jusqu'a 8, les plus proches, memes regles de portee, d'arc et de ligne de vue ; aucun nouveau degat)
  et les **tire vers le lanceur tant que la touche est tenue**, sans limite de duree : 12 m/s, un ennemi s'arrete a 2,5 m de l'oeil, un joueur a 2 m.
  Les boss ne sont jamais tires ; un kamikaze accroche explose ; relacher coupe la chaine (recharge de melee 0,5 s ; l'appui long dans le vide coute la
  recharge du coup dans le vide, 0,9 s). Pendant la traction, le lanceur bouge et tire (combo fleau -> rail), mais ne peut pas relancer de coup.
- Contre-jeu (la traction n'est pas une prison) : la chaine se coupe si la cible s'eloigne a plus de 8 m, si un mur coupe la ligne de vue plus de 0,25 s,
  si le lanceur meurt, est etourdi ou voit son fleau pare. En PvP la victime est tiree sur **sa propre vitesse** : courir a l'oppose ne suffit pas (la
  traction bat la course), mais un dash, un slide ou un bhop qui l'eloigne a plus de 16 m/s **arrache la chaine** ; une Egide de face divise la traction par deux.
- Une de tes pieces dans l'arc est frappee vers la cible visible la plus proche (45 m/s, 40 degats, 28 en PvP, portee de recherche 60 m).
- Parry a 6 m, renvoi plus lent (x0,8). Fleau pare : la chaine s'enroule et le lanceur est tire vers le pareur (16).

### IMPULSION (poings)
- Combo de 3 coups espaces de 0,22 s ; le 3e est un **uppercut** (recul vertical 11). Une seule cible par coup, la plus proche du cone (~53 degres).
- Un coup sans cible qui frappe le decor a moins de 1,9 m donne un **punch-jump** : la composante normale de la vitesse passe a au moins 11 m/s, sans pousser la vitesse horizontale au-dela de max(avant, 20 m/s). Une charge par sejour en l'air (rendue au sol et au debut d'un wall-run), partagee avec le wall-jump.
- Touche en l'air sur une cible en l'air : tu restes suspendu 2 s. Chaque coup qui touche rend un dash (0,35).
- Fenetre de parry 0,06 s par coup ; combo sans touche ni parry : +0,6 s de recharge ; poing pare : combo casse, 1 s. Jauge : 25 points par touche, 50 au plus par combo.

### EGIDE (bouclier)
- **Maintenir** : bouclier leve. Bloque de face (demi-angle 70 degres, 80 cote hote). Jauge de **120** degats PvP absorbables, regeneree de 30 par seconde bouclier baisse ; brise : indisponible 3 s.
- Souffle : la moitie est bloquee de face. Recul d'un coup bloque : x0,5.
- Vitesse au sol 7 m/s bouclier leve, freinage 40, acceleration aerienne x0,5.
- **Tap** (moins de 0,15 s) : coup de bouclier, 25 + 3 par m/s, portee 2,2 m, recul 14.
- **Parry parfait** : la levee ouvre une fenetre de 0,15 s si le bouclier etait baisse depuis au moins 0,3 s ; elle renvoie le rail (60 en PvP) et donne +100 points.
- Leve pendant la traction du grappin : arriver au contact donne un coup de bouclier gratuit (belier, degats d'impact du grappin). Un grappin adverse peut arracher le bouclier 1 s.

## Competences

| Competence | Charges | Recharge par charge | Effet |
|---|---|---|---|
| PIECE | 2 | 5 s | Lance une piece : un rail ou une roquette qui la touche est redirige vers la cible la plus proche (x1,5 par ricochet, portee de recherche 90 m). |
| MARQUE | 2 | 6 s | Marque une cible 4 s, visible a travers les murs (90 m) : tes touches sur elle x1,25. |
| CHARGE | 1 | 8 s | Maintenir 0,6 s : la prochaine touche dans les 2 s fait x1,5 (perdue si tu perds des PV). |
| RICOCHET | 1 | 20 s (apres la fenetre) | Fenetre de 6 s : tout ce que tu tires (rail, elan, roquettes, plombs, scie) rebondit sans limite, jusqu'a une cible ou la sortie de la carte (plafonds techniques). |
| ALL IN ONE | 1 | 20 s | Tire toute la munition de l'arme en main en eventail regulier (demi-angle 0,8 degre + 0,7 par munition, 6 au plus). |
| MIRAGE | 2 | 7 s | Leurre immobile 3 s : absorbe un tir adverse, ton rail y ricoche sans bonus. |
| AIMANT | 1 | 45 s (non remplie au respawn) | 4 s : le viseur est attire vers la cible proche du centre (cone ~6 degres, 70 m, ligne de vue). |

Regles croisees ajoutees par le patch d'equilibrage (NET_VER 21) :
- **Triangle MIRAGE ⇒ PIECE ⇒ RICOCHET ⇒ MIRAGE**, un pierre-feuille-ciseaux a sens unique (une constante par fleche) :
  le ricochet d'une piece vise le leurre adverse avant le joueur ; avec RICOCHET equipe, un tir sur un leurre adverse repart vers son poseur ;
  un projectile en rebond (fenetre RICOCHET ouverte) qui touche une piece adverse repart vers son tireur (`COIN_VS_RIC`, lecture par defaut, a confirmer).
- **TESLA × ALL IN ONE** : un eclair instantane sur la cible la plus proche du viseur, qui vide toute la batterie (20 au moins) ; 130 degats a pleine batterie.
- **ROQUETTES × PIECE** : la roquette qui touche une piece est redirigee vers l'ennemi le plus proche et le suit (x1,5 par piece).
- **SENTINELLE** (ennemi vert) : un tir sur deux est un laser parable (trait instantane), sinon une orbe a 110 m/s (34 avant) ; degats inchanges (12).
- **SCIE** : la sphere que le crochet doit toucher pour rappeler la scie plantee passe de 0,6 a 1,2 m (`SAW_HOOK_R`).

---

# II. Boutique du mode VAGUES : chemins et effets

> Cette partie decrit l'ANCIENNE boutique (chemins de 5 etapes), remplacee par la boutique v2 (voir `docs/CODE.md` §3.5 et `docs/boutique_vagues/`).
> Ancien texte : L'arbre de modificateurs et de fusions qu'on vise (20 graines, 154 fusions,
> une etape = une stat) est dans `docs/boutique_vagues/` (voir son `LISEZMOI.md`).

Source : `src/solo/ameliorations.c`. Chaque chemin (arme a distance, melee, competence) a 5 etapes achetees dans
l'ordre, aux prix 120, 240, 400, 600, 850 (`VG_PRICE_1..5`). Aucun achat n'augmente une stat : chaque etape ajoute un
comportement, qui ne vaut que pour les touches de son chemin, et seulement si l'arme ou la competence est equipee.

## Chemins

### Armes a distance
| Arme | Etape 1 | Etape 2 | Etape 3 | Etape 4 | Etape 5 |
|---|---|---|---|---|---|
| RAILGUN | FOURCHE | GEL | SATELLITE | FRACTURE | SURCHARGE |
| ROQUETTES | BRULURE | VORTEX | MINE | ECLATS | CONTAGION |
| EVENTAIL | RECUL | PROPULSION | ECLATS | BRULURE | DETONATION |
| ELAN | AERIEN | IMPACT | FRAPPE | FOURCHE | ARC |
| SCIE | BRULURE | SPECTRE | GEL | ECLATS | VORTEX |
| VENTOUSE | MINE | VORTEX | GEL | CONTAGION | ECLATS |
| TESLA | BRULURE | FOURCHE | SPECTRE | FRACTURE | CONTAGION |

### Melee
| Arme | Etape 1 | Etape 2 | Etape 3 | Etape 4 | Etape 5 |
|---|---|---|---|---|---|
| EPEE | ONDE (parry) | FRAPPE | MIROIR (parry) | ARC | STASE (parry) |
| ENCLUME | IMPACT | RECUL | RESSAC | ONDE (parry) | VORTEX |
| LANCE | FRAPPE | FOURCHE | MIROIR (parry) | GEL | DETONATION |
| YOYO | ATTRAIT | MIROIR (parry) | ARC | VORTEX | STASE (parry) |
| IMPULSION | FRACTURE | ONDE (parry) | FRAPPE | IMPACT | ECLATS |
| EGIDE | MIROIR (parry) | SPECTRE | ONDE (parry) | RECUL | STASE (parry) |

### Competences
| Competence | Etape 1 | Etape 2 | Etape 3 | Etape 4 | Etape 5 |
|---|---|---|---|---|---|
| PIECE | GEL | ECLATS | AIMANTEE | DETONATION | CONTAGION |
| MARQUE | PROPAGATION | BRULURE | ARRET | FOURCHE | VORTEX |
| CHARGE | RECUL | DETONATION | PULSION | ECLATS | CONTAGION |
| RICOCHET | ARC | MINE | GEL | FOURCHE | VORTEX |
| ALL IN ONE | BRULURE | PULSION | SPECTRE | RESSAC | CONTAGION |
| MIRAGE | APPAT | EXPLOSIF | REFLET | ARRET | GOUFFRE |
| AIMANT | GEL | ECLATS | ARC | SATELLITE | FOURCHE |

## Regles generales
- **Chemin concerne** : un effet ne se declenche que pour les touches de son chemin (`vgSrc`).
- **Contexte des competences** :
  - PIECE : touches d'un ricochet de piece ;
  - MARQUE : touche sur une cible marquee ;
  - CHARGE : touche chargee ;
  - RICOCHET : 1,5 s apres le tir qui rebondit ;
  - ALL IN ONE : 1,5 s apres la salve ;
  - AIMANT : pendant l'effet de l'aimant.
- **Une seule generation de reaction en chaine** (`vgDepth`) : un effet qui fait des degats ne redeclenche pas d'autres
  effets. Ses degats comptent pour le chemin qui l'a produit (`fx_damage`).
- **Boss epargnes** : ils ne se figent pas, ne se repoussent pas et ne sont pas aspires.

## Effets

### Declenches par une touche
| Effet | Fonctionnement |
|---|---|
| GEL | L'ennemi est fige 0,8 s (ni deplacement ni tir). |
| BRULURE | Il brule 3 s : 4 PV deux fois par seconde (8 PV/s). |
| RECUL | Impulsion de 14 depuis toi vers lui. |
| ATTRAIT | Impulsion de 10 vers toi, plus un petit saut. |
| ARC | Eclair vers l'ennemi le plus proche (9 m, ligne de vue) pour 40 % des degats de la touche. |
| FRACTURE | Compteur par ennemi : 3 touches en moins de 2 s declenchent une explosion de 3 m, 50 degats. |
| AERIEN | Seulement en l'air : 30 % des degats aux ennemis a moins de 2,5 m de la cible. |

### Declenches par un kill
| Effet | Fonctionnement |
|---|---|
| FOURCHE | Le coup repart vers les 2 ennemis les plus proches (25 m), au moins 30 degats ou la moitie du dernier coup. |
| DETONATION | Explosion de 3,5 m, 45 degats, sur la victime (`FX_ONDE` dans le code). |
| ECLATS | 3 eclats chercheurs : ils suivent l'ennemi le plus proche (40 m), vivent 2,5 s, 22 degats. Maximum 32 actifs. |
| MINE | Mine posee sur place : armee en 0,4 s, dure 10 s, explose a moins de 3,2 m d'un ennemi (rayon 4 m, 60 degats). Maximum 12. |
| SPECTRE | Un spectre orbite autour de toi 8 s et detruit la premiere orbe ennemie touchee. Maximum 6. |
| RESSAC | Repousse les ennemis a moins de 9 m de la victime (impulsion 18). |
| VORTEX | Un vortex de 1,2 s aspire les ennemis a moins de 12 m vers le point du kill. Maximum 8. |
| CONTAGION | Transmet brulure, gel et marque de la victime aux ennemis a moins de 9 m. |
| SURCHARGE | Remet le delai de tir a zero. Railgun seulement. |
| PROPAGATION | Si la cible morte etait marquee, marque les 2 ennemis les plus proches (30 m). |

### Declenches par un tir, un dash ou une retombee
| Effet | Declencheur | Fonctionnement |
|---|---|---|
| SATELLITE | Tir | Lache un eclat chercheur droit devant, vitesse 20. |
| PROPULSION | Tir | Te pousse de 7 vers l'arriere ; viser vers le bas te decolle du sol (saut au recul). |
| FRAPPE | Dash | Pendant 0,35 s, chaque ennemi traverse (a moins de 1,4 m) prend 45 degats et un recul, une fois par dash. |
| IMPACT | Retombee | Apres une chute a plus de 13 de vitesse : onde de 6 m, 50 degats, recul 10. |

### Declenches par un parry (melee equipee)
| Effet | Fonctionnement |
|---|---|
| ONDE (`FX_P_ONDE`) | Onde de 7 m : 40 degats, recul 16. |
| STASE (`FX_P_STASE`) | Fige tous les ennemis a moins de 15 m pendant 1,2 s. |
| MIROIR (`FX_P_MULTI`) | Les autres orbes ennemies a moins de 9 m repartent aussi vers l'ennemi le plus proche. |

### Declenches par l'usage d'une competence (charge depensee)
| Effet | Fonctionnement |
|---|---|
| PULSION (`FX_S_ONDE`) | Onde de 6 m : 25 degats, recul 14. |
| ARRET (`FX_S_STASE`) | Fige 1,2 s les ennemis dans un cone d'environ 37 degres devant toi, jusqu'a 30 m. |

### Passifs
| Effet | Fonctionnement |
|---|---|
| AIMANTEE | Tes pieces derivent vers l'ennemi le plus proche (14 m, vitesse 9) ; pas le leurre. |
| APPAT | Les ennemis visent et chargent ton leurre. |
| EXPLOSIF | Quand le leurre disparait, il explose (4,5 m, 70 degats). |
| REFLET | Les orbes qui touchent le leurre (a moins de 1,4 m) repartent vers les ennemis. |
| GOUFFRE | Le leurre aspire les ennemis a moins de 10 m (hors boss). |
