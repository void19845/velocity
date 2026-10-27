# VELOCITY — Idées d'armes et de cartes

> Document de design : propositions à choisir, **rien n'est implémenté**.
> Les chiffres de départ s'appuient sur les réglages réels de `main.c` (état au 26/10/2026).
> « PvE » = contre les drones (solo), « PvP » = contre un joueur en deathmatch LAN.
> **Fiches détaillées prêtes à implémenter** : [`armes/PLAN.md`](armes/PLAN.md) (index, conventions communes, questions en attente).

---

## 1. Rappel : piliers et contraintes

### 1.1 Piliers du jeu

- **La vitesse est une ressource.** Bhop, air-strafe, slide (+4,5 m/s), dash (18 m/s minimum, 2 charges, 1,6 s chacune), wall-run, grappin (traction jusqu'à 38 m/s, 46 m/s vers un ennemi). Les armes doivent **récompenser le fait d'aller vite**, jamais pousser à rester sur place.
- **Deux axes de combat qui se complètent.**
  - *Corps à corps* (épée, touche à part) : dégâts `35 + 4 × v_rel` et portée `2,4 + 0,12 × v_rel` m, où `v_rel` est la vitesse **relative** entre toi et la cible (`rel_speed`). Un coup qui touche rend un dash.
  - *Tir* : railgun à munitions limitées (5 au spawn, 10 max, recharges de 3) avec une chaîne de touches (+15 % dégâts et +5 % cadence par touche, 8 max) ; roquettes (110 de souffle, rayon 4,5 m, recul 13) ; pièces Marksman (4 max, 1 toutes les 2 s) qui font ricocher le rail ×1,5 par pièce.
- **Le parry.** Actif pendant les 0,12 premières secondes du coup d'épée : renvoie orbes et roquettes (×1,25 puis +15 % à chaque ping-pong), pare l'épée et l'impact de grappin d'un adversaire (attaquant repoussé, épée bloquée 1 s), soigne 10 PV, rend un dash et le grappin. Un coup dans le vide coûte 0,9 s de recharge.
- **Le « juice »** : hitstop, impact frames, chiffres de dégâts, éclats. Chaque nouvelle arme doit avoir son moment fort.

### 1.2 Ce que les chiffres PvP impliquent

En PvP, les dégâts sont multipliés par `PVP_SCALE = 0,7` puis plafonnés par l'hôte à `PVP_HIT_CAP = 60` PV par touche. Pour 100 PV :

| Source actuelle | Calcul PvP | Résultat |
|---|---|---|
| Rail | 100 × 0,7 = 70 | **60** (toujours plafonné) |
| Rail via pièce | 150 × 0,7 = 105 | **60** |
| Roquette (centre / bord) | 110 × (0,3…1) × 0,7 | **60** / 23 |
| Épée | (35 + 4·v) × 0,7 | 24,5 à l'arrêt, **60 dès v_rel ≈ 12,7 m/s** |
| Impact de grappin | (50 + 3·v) × 0,7 | 35 à l'arrêt, **60 dès v ≈ 12 m/s** |

Conséquences pour concevoir :

- **Le PvP se joue en « 2 touches »** (60 + 60 > 100), et `KILL_HEAL = 30` rend un kill à 70 PV. Une nouvelle arme se pense donc en *nombre de touches pour tuer* : 2 touches (≥ 50 PvP), 3 touches (≥ 34), 4 touches (≥ 25).
- Toute touche qui dépasse ~86 PvE est **identique en PvP**. La chaîne du rail ne change donc que la cadence en PvP.
- **Les armes à touches multiples (plombs, rayons continus) contournent le plafond** si chaque plomb envoie son propre `EV_HIT`. Règle : agréger tous les dégâts d'un même tir sur un même joueur en **un seul** `net_hit`.

### 1.3 Contraintes techniques

- **Rendu OpenGL 1.1 en pipeline fixe** : boîtes texturées, quads, lignes, particules en billboards. Pas de shaders, pas de modèles. Une arme doit être lisible avec des primitives (`draw_viewmodel`) et des particules (`add_part`, `burst`, `add_beam`).
- **La carte est une liste d'AABB** (`Box boxes[256]`, `addbox`, `addpad`) avec deux types, `B_SOLID` et `B_PAD`. Toute la géométrie est compilée **une seule fois** dans une display list (`build_world_list`) : un élément qui change (mur cassable, plateforme mobile) doit être dessiné à part.
- **Mouvement simulé chez chaque joueur** (`player_tick`), puis envoyé dans `NetPState` (pos, vel, yaw, pitch, `flags` dont les bits 64 et 128 sont libres, `weapon`, `melee`, un octet `pad` libre).
- **Les touches sont validées chez le tireur** (`fire_rail`, `melee`, `explode`… appellent `net_hit`) et **arbitrées par l'hôte** (`host_handle` : plafond de 60, parry de l'épée et du grappin via `lastParryT` avec 0,15 s de tolérance, PV, kills). Le recul (`knock`) est appliqué par la victime elle-même : on peut donc aussi **tirer** une cible vers soi avec un vecteur négatif.
- Les identifiants d'arme `W_*` servent au fil des kills et aux stats du bot (`hitCount[7]` : à agrandir si on ajoute des `W_`). Tout nouveau coup de mêlée doit être ajouté à la liste des coups parables dans `host_handle`.

### 1.4 Prérequis commun : emplacements et loadout

Aujourd'hui `P.weapon` vaut 0 (rail) ou 1 (roquettes) et la mêlée est fixe. Avant toute nouvelle arme :

- **Loadout choisi au spawn** (menu ou écran de mort) : **1 arme de mêlée + 2 armes à distance**. On garde ainsi deux boutons de tir (`A_WEAP1`, `A_WEAP2`, `A_SWAP`) et une touche de mêlée, ce qui reste lisible et facile à équilibrer.
- `P.meleeKind`, transmis dans l'octet `pad` de `NetPState` pour que les autres voient la bonne arme. On découpe cet octet en 4 bits pour le type de mêlée et 4 bits d'« état d'arme » (`NF_SLAM`, `NF_ZAP`…), puisque seuls deux bits de `flags` restent libres.
- Coût : **S**. Fonctions : `game_tick` (sélection), `draw_hud` (nom d'arme, temps de recharge), `draw_viewmodel`, `draw_remote_players`, `cfg_save`/`cfg_load` (mémoriser le loadout), enum `W_*`.

### 1.5 Règle validée : munitions limitées, rechargées par des jauges

But : **forcer le corps à corps**. Aucune arme à distance n'a de munitions illimitées, et **il n'y a pas de recharge passive** (décision du 27/10). On recharge en frappant au corps à corps et en ramassant des cellules.

- **Une jauge par arme.** Chaque arme à distance a une jauge de points. Quand la jauge est pleine, l'arme gagne **1 munition** et la jauge repart de zéro (le surplus est gardé). Arrivée au maximum de munitions, la jauge reste pleine et n'avance plus.
- **La taille de la jauge dépend de la puissance de l'arme.** Le rail tape très fort : sa jauge est grande (250) et il n'a que 2 munitions. Les roquettes tapent moins fort : petite jauge (50), mais beaucoup plus de munitions.
- **Les armes de mêlée donnent des points.** Chaque coup de mêlée qui touche donne un **montant propre à l'arme de mêlée**. Les points vont à l'arme à distance en main ; si elle est pleine, ils vont à l'autre arme du loadout. Un kill en mêlée donne un bonus, un parry réussi aussi.
- **Les cellules ramassables donnent 100 ou 150 points** : petite cellule 100, grande cellule 150, placée dans les endroits exposés. Une cellule remplit la jauge **des deux** armes du loadout, chacune de ce montant.
- **Chargeur vide** : un clic sec, et on bascule automatiquement sur l'autre arme du loadout si elle a encore des munitions.
- **Rocket jump** : il coûte une roquette (accepté le 27/10). Avec une jauge de 50, un seul coup d'épée qui touche rend une roquette.

**Armes à distance**

| Arme | Taille de la jauge (points par munition) | Munitions max | Au spawn | Logique |
|---|---|---|---|---|
| Rail | **250** | **2** | 2 | Très haut dégâts : 2 munitions, très lent à recharger |
| Roquettes | **50** | 8 | 4 | Dégâts moyens, beaucoup de munitions, rocket jump abordable |
| Éventail | 100 | 6 | 3 | Gros coup de près |
| Élan | 80 | 6 | 3 | Coûte déjà de la vitesse : jauge assez petite |
| Ventouse | 150 | 3 | 1 | Pièges puissants : peu nombreux |
| Tesla | La batterie est la jauge : 1 point = 0,4 de batterie (100 points = 40) | batterie 100 | 60 | Tir continu |
| Scie | — | 1 | 1 | Aucune jauge : mécanique propre (récupération, renvoi par parry) |

**Points donnés par les armes de mêlée** (par coup qui touche)

| Arme de mêlée | Points | Pourquoi |
|---|---|---|
| Épée | 50 | Référence : 5 coups = 1 tir de rail, 1 coup = 1 roquette |
| Enclume | 75 par onde ou coup qui touche | Lente et risquée |
| Lance | 100 par joute, 50 par estoc | Joute difficile à placer |
| Fléau | 35 | Longue portée, 4 touches pour tuer |
| Poings | 25 par coup, **50 au plus par combo** (validé le 27/10) | Très rapides |
| Égide | 50 par coup de bouclier, 100 par parry parfait | Arme défensive |
| Tous | **+100** par kill en mêlée, **+50** par parry réussi | Récompense le risque |

> Valeurs de départ, à régler avec le journal des matchs de bots. Exemple : tuer à l'épée (2 coups + kill) rapporte 50 + 50 + 100 = 200 points, presque un tir de rail ou 4 roquettes.

- Coût : **S**. Fonctions : `Player` (`ammo[]` et `ammoGauge[]`), `game_tick`, `pickup_taken` (deux tailles de cellules), `melee`/`try_parry` (points), `draw_hud` (jauge sous le compteur), `bot_think` (le bot va au contact quand il est à sec).

---

## 2. Armes de corps à corps

Toutes gardent la touche de mêlée actuelle et la règle « un coup qui touche rend un dash », sauf mention contraire.

### 2.1 Marteau-pilon « Enclume »

- **Fantasme / rôle** : tomber du ciel comme une météorite. La mêlée verticale : là où l'épée récompense la vitesse horizontale, l'Enclume récompense la **hauteur**.
- **Mécanique**
  - *Au sol* : coup lourd frontal. Recharge 0,8 s, portée `2,6 + 0,10 × v_rel` m, cône plus large (cos 0,45), recul 14 + 6 vers le haut.
  - *En l'air* : appui = **piqué**. `vel.y = min(vel.y, −30)`, la vitesse horizontale est conservée (on vise son point de chute), plus de contrôle aérien. À l'atterrissage : **onde de choc** de rayon `2,5 + 0,06 × |vy|` m, dégâts `25 + 1,5 × |vy|` avec la même atténuation que `explode` (0,3 à 1). L'onde n'atteint que les cibles à moins de 1,0 m au-dessus du sol : **sauter l'esquive**. `|vy|` est plafonné à 45.
  - Un piqué qui touche fait **rebondir** (vy = 9) et rend un dash. Un piqué dans le vide immobilise 0,6 s (atterrissage lourd).
- **Chiffres de départ**

  | Situation | Dégâts PvE | Rayon | PvP |
  |---|---|---|---|
  | Coup au sol, v_rel = 0 / 10 m/s | 45 / 75 | portée 2,6 / 3,6 m | 31,5 / 52,5 |
  | Piqué depuis le sommet d'un jump pad (arrivée à vy ≈ 36, corrigé par la fiche 01) | 79 | 4,7 m | 55 (centre) |
  | Piqué depuis un perchoir de 13 m (vy ≈ 39) | 83,5 | 4,8 m | 58,5 (centre) |

- **Synergie mouvement** : grappin vers un perchoir puis piqué ; jump pad (vy 20, sommet à environ 8 m) puis piqué ; rocket jump puis piqué. L'onde projette les cibles en l'air (recul vertical 12) : enchaînement naturel vers un **rail sur cible en vol**.
- **Contre-jeu** : le piqué est très visible (traînée et sifflement grâce à un drapeau réseau `NF_SLAM`). Sauter au moment de l'impact suffit à l'éviter. Le marteau au sol est lent.
- **Parry** : le coup au sol a une fenêtre de **0,10 s** (plus courte que l'épée). Le piqué ne pare rien, mais une victime dans sa fenêtre de parry au moment de l'impact **pare l'onde**. Le chemin `EV_MELEE_PARRY` existe déjà : l'attaquant est renvoyé en l'air, marteau bloqué 1 s.
- **Coût : M.** Fonctions : `melee` (selon `meleeKind`), `player_tick` (atterrissage : la détection `prevVy < -10` existe déjà), nouvelle `slam_land` (boucle de dégâts calquée sur `explode`), `host_handle` (ajouter `W_SLAM` aux coups parables), `draw_viewmodel`, `draw_remote_players`.

### 2.2 Lance de joute « Lance »

- **Fantasme / rôle** : la joute de chevaliers, version 40 m/s. La **vitesse relative de rapprochement** poussée au maximum, et le grappin comme monture.
- **Mécanique**
  - *Maintenir* la touche = **lance couchée** (2 s maximum). À chaque tick, on calcule la vitesse de rapprochement `v_c = dot(P.vel − cible.vel, direction vers la cible)`. Seule la composante d'approche compte. Si une cible entre dans un cône étroit (cos 0,92, environ 23°) à moins de 3,5 m **et** que `v_c ≥ 10 m/s` : touche automatique `10 + 5 × v_c`, la lance se relève, recharge 1,0 s. La cible reçoit 60 % de ta vitesse, tu en gardes 50 % (désarçonnement).
  - *Tap* (relâché en moins de 0,15 s) = estoc classique : `30 + 2 × v_rel`, portée 3 m, **avec** la fenêtre de parry habituelle.
  - Lance couchée : pas de parry.
- **Chiffres de départ** : v_c = 10 → 60 PvE / 42 PvP. v_c = 15 → 85 / **59,5** (2 touches). Lance face à lance (« choc de joute ») : le plus rapide de plus de 2 m/s gagne, sinon les deux sont désarçonnés.
- **Synergie mouvement** : grappin sur un joueur (traction jusqu'à 46 m/s), lance couchée = joute presque garantie. Dash, jump pads et carte « La Boucle » (voies à contresens).
- **Contre-jeu** : **un pas de côté** fait chuter `v_c` sous 10. Fuir dans le même sens annule la joute. Le cône est étroit.
- **Parry** : la lance touche automatiquement, donc la victime peut **timer son parry sur l'approche visible**. Lance parée : lance indisponible 1,5 s et recul de 11 (chemin existant).
- **Coût : M.** Fonctions : `game_tick` (lire `act[A_MELEE]` maintenu), nouvelle `lance_tick`, `melee` (estoc), `host_handle` (`W_LANCE` parable), drapeau `NF_LANCE` (bit 64), `draw_viewmodel`, `draw_remote_players`.

### 2.3 Fléau à chaîne « Fléau »

- **Fantasme / rôle** : *« viens ici ! »*. Mêlée **de contrôle** à longue portée, faible dégât, qui déplace la cible.
- **Mécanique** : portée `6 + 0,05 × v_rel` m, arc horizontal de 140° (cos 0,34), recharge 0,6 s. Dégâts `18 + 1,5 × v_rel`. La cible est **tirée vers toi** : recul négatif de 12 m/s + 3 vers le haut. `net_hit` le permet déjà, puisque la victime applique le vecteur reçu.
  - **Pièces** : le fléau qui touche une de tes pièces la **frappe**. Elle part à 45 m/s vers la cible visible la plus proche (aide à la visée `PARRY_AIM_COS`) et fait 40 PvE / 28 PvP. Si un rail la touche en vol, le ricochet ×1,5 normal s'applique.
- **Chiffres de départ** : v_rel = 0 → 18 PvE / 12,6 PvP ; v_rel = 20 → 48 / 33,6. Il faut environ 4 touches pour tuer : c'est un **lanceur de combos**, pas une arme qui tue.
- **Synergie mouvement** : tirer quelqu'un vers soi pendant qu'on fonce **additionne les vitesses**. Combos fléau → rail (la cible suit une trajectoire balistique prévisible) ou fléau → roquette. Au-dessus du vide (cartes Puits et Canyon), on peut faire tomber quelqu'un.
- **Contre-jeu** : faibles dégâts, et ramener l'adversaire vers soi le met aussi à **sa** portée de mêlée.
- **Parry** : fenêtre de 0,10 s, mais **portée de parry étendue à 6 m** avec un renvoi plus lent (×0,8). Fléau paré : la chaîne s'enroule et c'est le lanceur qui est tiré vers le pareur, droit dans son épée.
- **Coût : M** (S sans les pièces). Fonctions : `melee`, `try_parry` (portée et vitesse en paramètres), `coin_tick` (état « frappée »), `net_fx_coin` (sous-type 2), `host_handle` (`W_FLAIL` parable).

### 2.4 Poings à impulsion « Impulsion »

- **Fantasme / rôle** : boxeur cinétique. Cadence élevée, petits coups, et les poings servent **aussi à se déplacer**.
- **Mécanique** : combo de 3 coups, 0,22 s entre deux coups, 0,7 s après le troisième. Portée `1,9 + 0,06 × v_rel` m.
  - Coups 1 et 2 : `14 + 1,5 × v_rel`. Coup 3 : `20 + 2,5 × v_rel` + uppercut (recul vertical 11).
  - Chaque coup qui touche rend **0,35 dash** (environ 1 dash par combo, comme l'épée).
  - **Coup dans le décor** : si le poing frappe un AABB à moins de 1,9 m (`ray_world`), tu es propulsé de **+11 m/s le long de la normale** de la surface. Frapper le sol en l'air = mini-saut, une fois par séjour en l'air (remis à zéro au sol ou en wall-run).
- **Chiffres de départ** : à v_rel = 15, coups 1-2 = 36,5 PvE / 25,5 PvP, coup 3 = 57,5 / 40. Combo complet ≈ 91 PvP en 0,44 s. Le plafond de 60 ne joue jamais.
- **Synergie mouvement** : « punch-jump » pour enchaîner les murs ou redresser une trajectoire, uppercut puis rail.
- **Contre-jeu** : portée courte. Le recul d'une roquette casse le combo.
- **Parry** : fenêtre de **0,06 s seulement** par coup. Si aucun coup du combo ne touche ni ne pare, recharge de 0,6 s (anti-spam). Poing paré : combo cassé, 1,0 s (`MELEE_PARRIED_CD`).
- **Coût : S.** Fonctions : `melee` (compteur de combo dans `Player`), `game_tick` (recharge du combo), `ray_world` (impulsion), `host_handle` (`W_FIST` parable), `draw_viewmodel`.

### 2.5 Bouclier « Égide »

- **Fantasme / rôle** : le **contre-choix** face aux snipers du rail et des pièces. Arme défensive qui coûte volontairement de la vitesse.
- **Mécanique**
  - *Maintenir* = bouclier levé. Bloque **100 %** des touches frontales (cône de 70°) : rail, pièce, mêlée. Bloque **50 %** du souffle des roquettes.
  - Bouclier levé : vitesse au sol limitée à 7 m/s, accélération aérienne ×0,5, ni dash ni slide. Exception : aucune pénalité pendant la traction du grappin.
  - Jauge de 120 points : chaque touche bloquée consomme ses dégâts PvP (rail = 60). Elle se régénère de 30/s quand le bouclier est baissé. Jauge vide = bouclier brisé 3 s.
  - **Parry parfait** : lever le bouclier au plus 0,15 s avant une touche **renvoie** le rail vers le tireur (60 PvP, comme un ricochet sans bonus) ou la roquette (règles existantes), sans consommer la jauge.
  - *Tap* = coup de bouclier : `25 + 3 × v_rel`, portée 2,2 m, recul 14.
- **Synergie mouvement** : faible par choix. Bouclier levé pendant le grappin = **bélier** (arriver au contact déclenche le coup de bouclier gratuitement).
- **Contre-jeu** : prendre le flanc ou le dos (la vitesse latérale bat le bouclier), roquettes au sol, grappin sur le porteur (bouclier arraché 1 s).
- **Parry** : voir le parry parfait. Le bouclier remplace l'épée, donc on garde les renvois de projectiles.
- **Réseau** : la touche est décidée chez le tireur, qui doit donc connaître l'état du bouclier : drapeau `NF_SHIELD` (bit 128) + `yaw` déjà transmis. `fire_rail` teste `dot(direction, face de la cible) < −0,34` avant `net_hit`. L'hôte revérifie avec un `lastShieldT` (même schéma que `lastParryT`, tolérance 0,15 s).
- **Coût : L.** Fonctions : `fire_rail`, `ricochet`, `explode`, `melee`, `player_tick` (pénalités), `host_handle`, `pack_state`, rendu du bouclier sur les joueurs distants.

---

## 3. Armes à distance

### 3.1 Fusil à pompe « Éventail »

- **Fantasme / rôle** : le gros coup à bout portant qui manque à l'arsenal (le rail sert à distance, les roquettes au souffle).
- **Mécanique** : 10 plombs en cône de 9° au total, soit 4,5° de chaque côté (option C, décidée le 27/10), 8 PvE par plomb. Dégâts pleins jusqu'à 10 m, puis décroissance linéaire jusqu'à ×0,3 à 30 m. Recharge 0,95 s. **Jauge de 100 points par cartouche, 6 au maximum, 3 au spawn** (§ 1.5). Un tir compte comme touche pour la chaîne si au moins 3 plombs touchent.
  - **Agrégation** obligatoire : un seul `net_hit` par joueur touché et par tir.
  - **Recul** : en l'air, tirer pousse de 7 m/s à l'opposé de la visée. Tirer vers le bas donne un petit saut (« shotgun jump »).
  - **En slide** : dispersion divisée par 2 (récompense du slide).
  - **Pièces** : des plombs qui touchent une pièce la font **éclater** : souffle de 3 m, 45 PvE / 31,5 PvP, sans dégâts pour soi. Une grenade de poche.
- **Chiffres de départ** : à bout portant, 80 PvE / **56 PvP** (2 touches). À 10 m, 37 PvP. À 20 m, seulement 8 PvP, car une partie des plombs manque le joueur. Valeurs calculées par la fiche 06 pour le cône retenu (option C).
- **Contre-jeu** : la distance. Le fusil ne profite pas de la vitesse relative, ce qui le rend fiable mais moins spectaculaire.
- **Parry** : hitscan, donc non parable.
- **Réseau** : un seul événement `EV_SHOT` (origine, direction, graine aléatoire). Chaque client recalcule les 10 traceurs visuels, au lieu d'envoyer 10 `EV_RAIL`.
- **Coût : S.** Fonctions : nouvelle `fire_shotgun` (copie de `fire_rail` avec boucle de rayons), `game_tick`, `apply_event`, `draw_viewmodel`, `draw_hud`.

### 3.2 Convertisseur cinétique « Élan »

- **Fantasme / rôle** : **transformer sa vitesse en balle**. Le pilier « vitesse » appliqué au tir : le rail récompense la précision, l'Élan récompense le mouvement.
- **Mécanique** : hitscan. Au tir, `v_dep = min(vitesse horizontale, 15 m/s)` et dégâts = `20 + 5 × v_dep`. **Tu perds `v_dep` de vitesse horizontale**, comme un recul réel. Recharge 0,5 s. **Jauge de 80 points par charge, 6 au maximum, 3 au spawn** (§ 1.5) : l'arme coûte donc à la fois de la vitesse et des munitions. Les pièces fonctionnent comme avec le rail (ricochet ×1,5, `ricochet()` réutilisé), et la chaîne de touches s'applique.
- **Chiffres de départ**

  | Vitesse au tir | Dégâts PvE | PvP | Touches pour tuer |
  |---|---|---|---|
  | 0 m/s (à l'arrêt) | 20 | 14 | 8 |
  | 9,5 m/s (course) | 67,5 | 47 | 3 |
  | ≥ 15 m/s (bhop, dash, slide) | 95 | **60** | 2, mais tu t'arrêtes net |

- **Dash-tir** (**décision du 27/10**) : le double dash-tir reste permis, car c'est une technique. Mais sa **fenêtre est réduite** : le tir doit partir dans les 0,10 s qui suivent le dash pour profiter de la vitesse du dash. Raté, le tir est calculé avec la vitesse d'avant le dash. Détails dans la fiche 07.
- **Synergie mouvement** : dash (18 m/s) puis tir = dégâts maximaux. Relancer la vitesse (slide, pad, grappin) entre deux tirs devient le rythme de l'arme. *Variante à tester* : tirer **vers l'arrière** pousse vers l'avant (le recul s'ajoute à la vitesse au lieu de l'enlever), plafonné à 30 m/s. On peut alors fuir en tirant sur son poursuivant.
- **Contre-jeu** : après un tir puissant, le tireur est **lent**, donc la cible idéale pour une épée ou une lance. À l'arrêt, l'arme ne vaut presque rien.
- **Parry** : comme le rail (hitscan non parable).
- **Coût : S.** Fonctions : `fire_rail` paramétrée (dégâts et coût en vitesse) ou petite `fire_kinetic`, `game_tick`, `draw_hud` (aperçu des dégâts selon la vitesse, comme `melee_preview`).

### 3.3 Disque-boomerang « Scie »

- **Fantasme / rôle** : une lame unique qu'on lance et qu'il faut **aller rechercher**. Toucher, c'est la récupérer tout de suite. Rater, c'est être désarmé jusqu'à ce qu'on aille la chercher, donc forcé au corps à corps.
- **Mécanique** (version validée)
  - **Lancer** : projectile à `32 m/s + ta vitesse dans l'axe de visée` (il hérite de ton élan), gravité légère de 6 m/s² (environ 1,4 m de chute sur 22 m). **Une seule scie**, sans munitions : la scie *est* la munition.
  - **Elle touche un joueur** : dégâts `30 + 1,5 × max(0, vitesse relative scie / cible − 12)` (décision du 27/10 : 3 touches contre une cible immobile), puis elle **revient automatiquement vers le lanceur** (36 m/s, guidée, à travers le décor : c'est un rappel magnétique). Le retour ne fait pas de dégâts. Le lanceur la **rattrape** automatiquement à moins de 1,5 m : recharge de lancer remise à 0 et +0,5 dash.
  - **Elle touche le décor** (sol, mur ou plafond) : elle **reste plantée** dans la face de l'AABB, à l'endroit de l'impact, sans limite de temps. Elle ne rebondit plus.
  - **Récupérer une scie plantée**
    - *À pied* : passer à moins de 1,5 m.
    - *Au grappin* (**validé**) : le crochet qui touche **ta** scie plantée (sphère de 0,6 m) la **ramène vers toi à 45 m/s sans te déplacer**. Le grappin se décroche aussitôt, sans traction ni balancement. La scie traverse le décor au retour (rappel magnétique) et arrive dans ta main : recharge de lancer à 0. C'est le seul moyen de récupérer une scie plantée au plafond ou hors d'atteinte. Le crochet garde sa portée de 55 m et sa recharge de 0,5 s.
    - *Option à tester* : la scie rappelée blesse le premier adversaire qu'elle traverse (mêmes dégâts, une seule cible). Désactivée par défaut.
  - **Mort du lanceur** : la scie plantée disparaît, et il en a une neuve au respawn.
  - Une scie plantée est **visible par tous** et ne blesse personne. Les adversaires savent donc que tu es désarmé et où tu vas aller.
  - **Pièces** : la scie en vol qui frôle une pièce la **relance** vers le haut (+2 s de vie), ce qui prépare un rail.
- **Chiffres de départ**

  | Vitesse relative scie / cible | Dégâts PvE | PvP | Touches pour tuer |
  |---|---|---|---|
  | ≤ 12 m/s (cible qui fuit presque à la vitesse de la scie) | 30 | 21 | 5 |
  | 20 m/s (cible qui s'écarte à 12 m/s) | 42 | 29,4 | 4 |
  | 32 m/s (cible immobile, la scie part à 32 m/s au moins) | 60 | **42** | **3** |
  | ≥ 49 m/s (cible qui fonce vers la scie, ou scie renvoyée par parry) | ≥ 86 | **60** | 2 |

  Recharge de lancer 0,4 s après une récupération.
- **Synergie mouvement** : lancer en pleine vitesse = scie plus rapide, donc plus de vitesse relative. Rater, puis récupérer la scie au grappin ou en wall-run, devient un mini-parcours. Toucher la rend instantanément, ce qui récompense la précision par la cadence.
- **Contre-jeu** : la trajectoire est lente (32 m/s, contre 38 pour les roquettes) et prévisible. Après un raté, le lanceur est désarmé de cette arme : on peut **attendre près de sa scie plantée**, puisqu'on sait qu'il va y revenir.
- **Renvoi par parry au retour** (**validé le 27/10**) : quand la scie revient vers toi après une touche, tu peux la **parer** (touche de mêlée, dans ta fenêtre de parry). Au lieu de la rattraper, tu la **renvoies plus vite**, avec une **légère aide à la visée**. La vitesse plus élevée augmente la vitesse relative, donc les dégâts. Détails chiffrés dans la fiche 08.
- **Parry** : c'est un projectile, donc `try_parry` l'accepte. Une scie parée repart ×1,25 **vers son lanceur**. Si elle le touche, il prend les dégâts mais la rattrape quand même. Si elle le rate, elle se plante où elle tombe et il doit aller la chercher.
- **Réseau** : le lanceur fait autorité sur sa scie. Un événement `EV_SAW` à sous-types : 0 lancée (position, vitesse, netId), 1 plantée (position, normale), 2 touche un joueur et revient, 3 récupérée. Les dégâts passent par `net_hit(..., W_SAW)`. Le parry de la victime suit le même chemin que pour une roquette (`net_fx_parry`).
- **Coût : M.** Fonctions : `spawn_proj` (`PR_SAW`, états vol / plantée / retour), `proj_tick`, nouvelle `saw_tick` (ramassage à 1,5 m, retour guidé), `try_parry`, `grapple_tick` (le crochet teste ta scie plantée et la rappelle), `apply_event` (`EV_SAW`), rendu du disque (en vol, et planté dans le décor), `draw_hud` (indicateur « scie plantée » avec sa direction).

### 3.4 Mines collantes « Ventouse »

- **Fantasme / rôle** : pièges et explosions déclenchées à distance. Le pendant « préparation » des roquettes.
- **Mécanique** : lancer à `24 m/s + ta vitesse`, gravité 16 (comme les pièces). La mine colle au décor ou à un joueur. 2 mines actives au maximum. Détonation au 2e appui (télécommande) ou au bout de 3 s. Souffle de 4 m, 90 PvE au centre, **recul 15** (plus que la roquette), 5 PV de dégâts sur soi (comme les roquettes). **Jauge de 150 points par mine, 3 au maximum, 1 au spawn** (§ 1.5). Détonation : deux commandes séparées (voir la fiche 09).
  - **Collée sur un joueur** : bip et contour rouge chez la victime. Armement minimal de 0,6 s avant de pouvoir exploser.
  - **Rail ou pièce sur une mine** : explosion ×1,5 (135 PvE, rayon 5 m). C'est une « pièce explosive », et le rail d'un adversaire peut faire sauter ta mine **à côté de toi** (même logique que la pièce volée).
- **Chiffres de départ** : centre 90 PvE → 63 → **60 PvP**. Bord 27 → 19 PvP. Mine collée = 60 garantis, si la victime ne pare pas.
- **Synergie mouvement** : « mine-jump » au timing précis (meilleur contrôle qu'une roquette). Une mine posée sur un mur de wall-run sert de **booster** à déclencher en passant.
- **Contre-jeu** : bip lisible, rail pour faire sauter la mine près de son poseur, parry.
- **Parry** : en vol, renvoyée comme une roquette. **Collée sur toi** : un parry pendant ta fenêtre l'arrache et la renvoie dans ta direction de visée à 36 m/s.
- **Coût : M.** Fonctions : `spawn_proj` (`PR_MINE`, état collé à un AABB ou à un joueur), `proj_tick`, `explode` (réutilisée), `try_parry`, `fire_rail` (rayon contre les mines), nouvel événement pour coller ou décrocher une mine, `draw_hud` (alerte).

### 3.5 Arc électrique « Tesla »

- **Fantasme / rôle** : rayon continu qui saute de cible en cible. Une arme de **suivi** (tracking), à l'opposé du rail qui demande de la précision instantanée.
- **Mécanique** : faisceau tant que le clic est maintenu, 10 ticks/s. Portée `14 + 0,3 × ta vitesse` m (20 m à 20 m/s). 7 PvE par tick. **Chaîne** : l'arc saute vers 2 cibles de plus, chacune à 6 m au plus de la précédente, à 60 % des dégâts.
  - Batterie : **60 au spawn, 100 au maximum**, −20/s en tirant (5 s de tir continu à plein), **pas de recharge passive**. La batterie sert de jauge : chaque point reçu (mêlée, cellule) donne 0,4 de batterie, donc une petite cellule (100) rend 40 et un coup d'épée (50) rend 20.
  - **Pièces** : une pièce touchée devient un **relais** pendant 2 s. Elle cesse de tomber et frappe toute cible à moins de 6 m (5 PvE par tick).
  - Ne compte pas dans la chaîne de touches (arme continue).
- **Chiffres de départ** : 70 PvE/s, 49 PvP/s, donc environ 2 s pour tuer en suivant parfaitement. Réseau : un `net_hit` toutes les 0,2 s par cible (≈ 10 PvP), le plafond ne joue jamais.
- **Synergie mouvement** : la portée grandit avec la vitesse. Pièces lancées en courant = relais semés sur la trajectoire.
- **Contre-jeu** : portée courte, ligne de vue, et un adversaire très mobile casse le suivi.
- **Parry** : être dans l'arc **pendant sa fenêtre de parry** renvoie la décharge. Le tireur est étourdi 0,4 s (ne peut plus tirer) et prend 15.
- **Réseau** : aucun événement par tick pour le visuel. Un drapeau `NF_ZAP` (dans l'octet d'état d'arme) suffit, avec `yaw`/`pitch` déjà transmis : chaque client dessine l'arc.
- **Coût : M.** Fonctions : nouvelle `tesla_tick` appelée dans `game_tick` (tir maintenu), `coin_tick` (relais), `try_parry`/`host_handle` (renvoi), rendu de l'arc (lignes brisées avec `add_beam`).

---

## 4. Cartes et nouveaux éléments de décor

### 4.1 Nouveaux éléments compatibles AABB

| Élément | Principe | Coût |
|---|---|---|
| **Bande d'accélération** `B_SPEED` | Boîte plate au sol. Tant qu'on est dessus, +30 m/s² dans une direction fixe (champ `launch` réutilisé), jusqu'à 28 m/s, sans friction. | S |
| **Pad additif** `B_BOOST` | Comme `B_PAD`, mais `vel += launch` au lieu de `vel = launch` : il **conserve** l'élan. | S |
| **Volume déclencheur** (anneau de vitesse, zone de recharge) | Boîte non solide. Les fonctions de collision (`move_axis`, `pos_free`, `ray_world`, `los`) ignorent les `kind ≥ B_TRIGGER`. +6 m/s dans la direction courante, une fois toutes les 2 s. | S-M |
| **Plan de mort** (vide) | `if (P.pos.y < killY) damage_player(999)` (passe par `W_SELF`). Les cartes à vide en ont besoin. En solo, il faut adapter les drones (`enemy_tick` force y ≥ 1). | S |
| **Ancres de grappin** | Petits cubes flottants d'une couleur dédiée : de simples AABB. | S |
| **Mur destructible** `B_GLASS` | 60 PV. Cassé par roquette, piqué, ou corps lancé à plus de 20 m/s. L'hôte fait autorité (`EV_BOX_BREAK` avec l'index de la boîte), réapparition 20 s. Rendu **hors** display list. | M |
| **Vraie rampe** `B_RAMP` | AABB avec un axe de pente : hauteur du sol interpolée dans `move_axis`/`pos_free`, rendu en quad incliné. Glisser en descente ajoute `sin θ × 24 m/s²`. Sans cela, une « rampe » n'est qu'un escalier de marches de 0,3 m (`STEP_H` = 0,55), sans gain de vitesse. | L |
| **Plateforme mobile** | Position = fonction de `matchT` (déjà synchronisé par snapshot), donc déterministe chez tous. Collision avec un objet qui bouge et entraînement du joueur. | L |

### 4.2 « La Boucle » : circuit de vitesse (inspiration Redliner)

- **Plan** : anneau rectangulaire de 120 × 60 m autour d'un bloc central de 90 × 30 m et 10 m de haut, soit un couloir de 15 m de large. Chaque ligne droite est coupée en **deux voies** par des séparateurs bas (1 m, qui servent de couverture) et chaque voie a des bandes d'accélération **dans des sens opposés**. Les joueurs se croisent donc de face. Murs de wall-run sur l'extérieur des virages, pads directionnels pour couper les coins. Deux **passerelles** à 10 m de haut traversent le bloc central (raccourci et point de chute pour l'Enclume). Le toit du bloc est accessible au grappin : un poste de tir exposé.
- **Ce qu'elle encourage** : vitesse maximale et **joutes frontales**. Deux joueurs à 25 m/s en sens inverse = 50 m/s de vitesse relative : épée plafonnée à 60, portée `2,4 + 0,12 × 50` = 8,4 m. ⚠ Il faudra sans doute **plafonner la portée de mêlée à environ 6 m**. Les lignes droites de 120 m favorisent le rail et l'Élan.
- **Budget** : environ 70 boîtes sur 256. 8 points de spawn le long de l'anneau, 8 recharges (milieu des lignes droites, passerelles, toit).
- **Coût : S** pour la carte, plus **S** pour `B_SPEED`.

### 4.3 « Le Puits » : verticalité

- **Plan** : puits carré de 40 × 40 m et 70 m de haut, **vide au fond** (plan de mort à y = −30). Six étages de plateformes de 8 × 8 m en spirale, espacés de 12 m. Les quatre parois sont des murs de wall-run sur toute la hauteur. 12 ancres de grappin suspendues au centre. Pads au fond (vy 30, soit environ 19 m de montée). Les recharges sont en haut : il faut prendre de la hauteur, donc un risque.
- **Ce qu'elle encourage** : verticalité, Enclume (les piqués à 45 m/s sont fréquents), rocket et mine jumps, grappin. Combats aériens avec des pièces lancées en chute. Le Fléau peut faire tomber un adversaire dans le vide.
- **Budget** : environ 55 boîtes. Spawns sur les plateformes, en vérifiant `pos_free`.
- **Coût : S-M** (le plan de mort est partagé avec le Canyon). Carte **LAN seulement** au début, à cause des drones du mode solo.

### 4.4 « Le Dojo » : duels au corps à corps

- **Plan** : salle de 36 × 36 m avec un **plafond à 7 m**. Ce toit limite les rocket jumps, mais le grappin peut s'y accrocher pour se balancer. 16 piliers de 2 × 2 m en grille bloquent les lignes de rail. Au centre, un ring de 12 × 12 m surélevé de 0,5 m, entouré de murs de verre destructibles de 3 m (version 1 possible sans verre). **Aucune cellule de munitions** : on n'a que les munitions du spawn, plus celles que rend la mêlée (§ 1.5). Chaque tir compte et la mêlée domine.
- **Ce qu'elle encourage** : parry, lecture de l'adversaire, épée, Lance, Poings, Fléau. Idéale en 1v1 ou 2v2, avec par exemple une limite de 10 frags.
- **Budget** : environ 50 boîtes.
- **Coût : S** (M avec `B_GLASS`).

### 4.5 « Le Canyon » : tir de loin et traversées

- **Plan** : deux falaises (plateaux de 40 × 60 m, à 20 m de haut) séparées par un gouffre de 70 m (plan de mort). Au milieu, 6 îlots-piliers de 4 × 4 m et des ancres de grappin espacées d'au plus 25 m : la portée du grappin étant de 55 m, on traverse toujours en 3 accroches. Chaque falaise a des bâtiments de couverture et une tour de tir à 10 m. Des pads au bord des falaises lancent vers l'îlot central.
- **Recharges** : 6 sur les îlots, 2 par falaise. Pour recharger ses armes, il faut **traverser**, ou aller chercher les munitions au corps à corps. Une Scie ratée tombe souvent dans le gouffre : prévoir qu'une scie qui passe sous le plan de mort revienne au lanceur après 3 s.
- **Ce qu'elle encourage** : rail et pièces (les ricochets contournent les couvertures), Élan lancé à pleine vitesse en sortie de pad, duels au grappin au-dessus du vide.
- **Budget** : environ 70 boîtes.
- **Coût : S-M.**

### 4.6 (Option) « Le Skatepark » : rampes et glisse

- Bols, quarter-pipes et longues pentes pour du **slide-hop** façon Titanfall : descendre en slide fait gagner de la vitesse, remonter une rampe transforme la vitesse en hauteur.
- **Coût : L**, car il dépend de `B_RAMP`. À garder pour plus tard.

### 4.7 Comment ajouter des cartes dans le code

1. **Découper** `build_level()` en `build_level(int map)` qui appelle `build_map_arene()` (la carte actuelle, **inchangée** : `run_tests` dépend de son couloir dégagé en x = −20 et de ses coordonnées), `build_map_boucle()`, etc.
2. **Décrire chaque carte** : `typedef struct { const char *name; void (*build)(void); const float (*spawns)[3]; int nspawns; float killY; V3 sky; } MapDef;`. `pick_spawn` lit `spawns` au lieu de sa table codée en dur.
3. **Respecter les limites** : 256 boîtes (`MAXB`), **16 recharges** au maximum (`MAXPICK` et `pickMask` sur 16 bits dans `PkSnap`).
4. **Choix par l'hôte en LAN** : un bouton « Carte : < nom > » dans le menu d'hébergement (`draw_main_menu`). L'identifiant de carte voyage dans `PkWelcome.pad[0]` (sans changer la taille du paquet) et dans `EV_MATCH` (champ `sub`) pour changer de carte entre deux matchs. `PkAnnounce.pad` peut l'annoncer dans la liste des parties. À la réception : `build_level(map)`, `glDeleteLists(worldList, 1)`, `build_world_list()`, et remise à zéro des projectiles, pièces et recharges. Passer `NET_VER` de 3 à 4.
5. **Solo** : même menu, mais seules les cartes « avec sol » (Arène, Boucle, Dojo) au début.
6. **Vérifier** `bot_think`, qui contient peut-être des hypothèses sur la géométrie de l'arène. Ajouter dans `run_tests`, pour chaque carte : aucun spawn ni recharge dans un solide (`pos_free`), nombre de boîtes inférieur à `MAXB`.

---

## 5. Récapitulatif

### 5.1 Armes

| Arme | Type | Rôle | Dégâts PvP clés | Touches pour tuer | Munitions (jauge par munition, max) | Lien avec la vitesse | Parry | Coût |
|---|---|---|---|---|---|---|---|---|
| Enclume (marteau-pilon) | Mêlée | Piqué vertical, onde de choc | 49-60 (piqué), 31-52 (sol) | 2 | — | Vitesse **verticale** | Fenêtre 0,10 s au sol ; l'onde est parable | M |
| Lance | Mêlée | Joute frontale | 42-60 | 2 | — | Vitesse de **rapprochement** | Pas de parry quand couchée ; parable par la cible | M |
| Fléau | Mêlée | Contrôle, attire la cible | 12-34 | 4 | — | Vitesse relative + traction | Parry à 6 m | M |
| Impulsion (poings) | Mêlée | Combo rapide + déplacement | 25 / 25 / 40 | 3 (combo) | — | Punch-jump | Fenêtre 0,06 s | S |
| Égide (bouclier) | Mêlée | Défense anti-tir | 17-60 (coup de bouclier) | — | Jauge 120 (régénère) | Pénalise (choix voulu) | Parry parfait renvoie le rail | L |
| Éventail (pompe) | Distance | Gros coup à bout portant | 56 → 8 | 2-3 | Jauge 100, max 6 | Slide, recul | Non | S |
| Élan (convertisseur) | Distance | Vitesse → dégâts | 14 → 60 | 2 à 8 | Jauge 80, max 6 | **Consomme** ta vitesse | Non | S |
| Scie (lame à récupérer) | Distance | Touche = retour ; raté = plantée | 21-60 | 2-3 (5 au pire) | 1 scie à récupérer (à pied, au grappin) | Hérite de ton élan | Parable, renvoyée au lanceur | M |
| Ventouse (mines) | Distance | Pièges, mine-jump | 19-60 | 2 | Jauge 150, max 3 | Mine-jump, booster mural | Parable, décrochable | M |
| Tesla (arc) | Distance | Suivi, chaînes | 49/s | ~2 s | Batterie 100 (1 point = 0,4) | Portée selon vitesse | Renvoi + étourdissement | M |

Rappel § 1.5 : pas de recharge passive. Chaque arme a une jauge de points (rail 250 pour 2 munitions, roquettes 50 pour 8) que remplissent la mêlée (montant propre à chaque arme de mêlée) et les cellules ramassables (100 ou 150).

### 5.2 Cartes

| Carte | Encourage | Nouveaux éléments | Coût |
|---|---|---|---|
| La Boucle | Vitesse pure, joutes frontales, lignes de rail | `B_SPEED` | S + S |
| Le Puits | Verticalité, piqués, grappin | Plan de mort, ancres | S-M |
| Le Dojo | Duels au corps à corps, parry | (`B_GLASS`) | S (M) |
| Le Canyon | Tir de loin, ricochets, traversées | Plan de mort, ancres, pads | S-M |
| Le Skatepark | Slide-hop | `B_RAMP` | L |

---

## 6. Recommandation : par quoi commencer

### Arme 1 : **Élan, le convertisseur cinétique** (coût S)

- C'est l'arme qui exprime le mieux le pilier du jeu **sur l'axe du tir**. Aujourd'hui, la mêlée récompense la vitesse (`rel_speed`) mais le tir ne récompense que la précision (chaîne de touches). L'Élan comble ce manque.
- Elle crée une **vraie tension** : un tir à 60 PvP t'arrête net, donc tu deviens une cible pour l'épée. Cela rend le duel tir contre mêlée plus intéressant sans rien changer aux armes existantes.
- Elle coûte presque rien : c'est `fire_rail` avec deux paramètres. Pièces, chaîne et réseau (`net_hit` W_RAIL ou nouveau `W_KINETIC`) sont réutilisés tels quels. C'est aussi le bon prétexte pour construire le **loadout** (§ 1.4) et le **compteur de munitions par arme** (§ 1.5), dont toutes les autres armes ont besoin. Quand le chargeur de l'Élan est vide, on retourne au contact pour le remplir : c'est exactement la boucle voulue.

### Arme 2 : **Enclume, le marteau-pilon** (coût M)

- Elle donne à la mêlée une **dimension verticale** que l'épée n'a pas. La hauteur gagnée (pads, perchoirs, grappin, rocket jumps) devient une ressource offensive.
- Le code est déjà presque là : la détection d'atterrissage existe (`prevVy < -10`), la boucle de dégâts se copie sur `explode`, et le parry réseau passe par le chemin existant `EV_MELEE_PARRY` (il suffit d'ajouter `W_SLAM` dans `host_handle`).
- Fort potentiel de « juice » avec les impact frames, le hitstop et les éclats existants. Le combo piqué → cible projetée → rail est très lisible et très satisfaisant.
- *Pourquoi pas la Lance ?* Elle est excellente mais recoupe l'épée et l'impact du grappin (tous deux fondés sur la vitesse relative). *Pourquoi pas le fusil à pompe ?* Il comble un trou mais ignore le pilier vitesse. C'est le meilleur troisième choix.

### Carte : **La Boucle** (coût S + S)

- **Aucune nouvelle technique de collision** : uniquement des AABB, plus la bande d'accélération (S). Elle permet aussi de mettre en place le **choix de carte en LAN** (`MapDef`, `NET_VER` 4, reconstruction de la display list), qui servira à toutes les cartes suivantes.
- Elle met en valeur les deux armes recommandées : les **lignes droites** pour l'Élan à pleine vitesse, les **passerelles** à 10 m pour les piqués de l'Enclume.
- Les voies à contresens produisent des vitesses relatives inédites (50 m/s), et l'épée existante devient spectaculaire sans rien coder. Le journal du bot (`ttkSum`, `oneShots`, `meleeParries`) permet de mesurer l'effet et de régler le plafond de portée de mêlée.
- Ensuite, **Le Puits** (il ajoute le plan de mort) est la suite logique pour mettre en vedette l'Enclume.

### Ordre de travail proposé

1. Loadout et emplacements d'arme (S).
2. Élan (S), testé sur l'arène actuelle.
3. Infrastructure de cartes, `B_SPEED` et La Boucle (S + S + S).
4. Enclume (M).
5. Matchs de bots sur La Boucle pour comparer le temps pour tuer et régler les chiffres.
