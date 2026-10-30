# Plan — mode par équipes (3 contre 3)

Objectif : un mode équipe (3v3) à côté du deathmatch libre actuel, sans rien casser de l'existant.
L'hôte reste autoritaire comme aujourd'hui ; on ajoute une notion d'équipe au-dessus du protocole
réseau déjà en place.

## Ce qu'on réutilise tel quel

- `NetPlayer NP[MAXP]` (`MAXP` = 8) — largement assez pour 3v3 (6 joueurs actifs + 2 places libres,
  ou des remplaçants).
- L'autorité de l'hôte : `host_handle`, `host_kill`, `host_respawn`, `host_start_match`,
  `host_end_match`, `host_tick` (voir `docs/CODE.md` §4).
- Le protocole d'événements `EV_*` et le réseau fiable (x3 + dédoublonnage) : aucun changement de
  mécanique, juste de nouveaux champs à transporter.
- Le lobby Cloudflare (`worker/`) pour la découverte/matchmaking en ligne.

## Étapes

### 1. Donnée d'équipe
- Ajouter `unsigned char team` (0 ou 1) à `NetPlayer` et au paquet `PkJoin` (le client peut demander
  une équipe, ou laisser l'hôte choisir).
- `host_on_join` assigne l'équipe la moins nombreuse par défaut (équilibrage automatique), avec
  possibilité de rejoindre un ami dans la même équipe plus tard (hors V1).
- `PkWelcome` renvoie l'équipe assignée. `PkSnap`/`NetPState` ou `PkRoster` doivent aussi porter
  l'équipe de chacun pour que les clients l'affichent (roster déjà envoyé à chaque arrivée).
- **`NET_VER++`** (nouveaux champs de paquet).

### 2. Pas de dégâts entre alliés
- Dans `host_handle`, cas `EV_HIT` : si `NP[target].team == NP[from].team` (et `target != from`),
  ignorer le coup avant le plafond `PVP_HIT_CAP` — comme le filtre « cible déjà morte / match fini »
  déjà présent.
- Vérifier un par un les chemins qui infligent des dégâts hors `EV_HIT` simple : souffle d'explosion
  (`explode_ex`, roquettes/mines), parry renvoyé (`EV_PARRY` → le projectile change de `from`, donc
  le filtre s'applique normalement à la cible suivante), bouclier/Égide (l'absorption ne doit pas
  soigner via un allié qui se tire dessus), Scie (peut-elle être ramassée/relancée par un allié sans
  dégât ?). Lister ces cas dans `docs/CODE.md` §3.4 une fois tranchés.
- Les pièces/munitions ramassées (`EV_PICKUP`) restent partagées par tous (pas de notion d'équipe
  nécessaire ici).

### 3. Score et victoire par équipe
- Garder `NP[i].kills`/`NP[i].deaths` individuels (utile pour le tableau des scores détaillé), mais
  calculer un score d'équipe = somme des kills de l'équipe (ou un compteur séparé si on veut éviter
  qu'un suicide fasse reculer l'équipe deux fois).
- `FRAG_LIMIT` devient une limite **d'équipe** : `host_kill` vérifie le total de l'équipe du tueur,
  pas son total individuel.
- `host_end_match` prend un `winner` qui est une équipe (0/1) plutôt qu'un id de joueur ; adapter
  `EV_MATCH` (`sub=1`) et le message affiché (« ÉQUIPE BLEUE GAGNE »).

### 4. Spawns côté de son équipe
- `pick_spawn` choisit aujourd'hui le point le plus loin de **tous** les joueurs actifs (bon pour du
  libre, mauvais pour des équipes : pas de camp défini, risque de spawn-trap).
- Idée simple pour la carte actuelle (symétrique, 4 côtés) : réserver les points nord à l'équipe 0 et
  sud à l'équipe 1 (ou est/ouest), en gardant la logique « le plus loin des ennemis actifs, pas des
  alliés » à l'intérieur de la zone de son équipe.
- Décision à prendre : adapter la carte actuelle (rapide, moins lisible comme carte « équipes ») ou
  construire une carte dédiée plus tard avec deux bases franches (`build_level` sait déjà charger un
  layout unique et déterministe — une 2e fonction de carte est envisageable si besoin, mais c'est un
  chantier à part).

### 5. Visuel : couleur par équipe
- `pcol[MAXP][3]` est aujourd'hui une palette fixe par **id** de joueur (8 couleurs distinctes) —
  utilisée pour les tags de nom, le tableau des scores et la teinte du modèle des joueurs distants.
  En mode équipe, remplacer par une couleur d'équipe (bleu/rouge) pour reconnaître l'ennemi au
  premier coup d'œil ; garder une variation mineure (teinte) pour distinguer les coéquipiers entre
  eux si besoin.
- HUD : pas de marqueur de dégât agressif sur un allié touché par erreur (puisqu'il n'y a plus de
  dégât, surtout pas de flash rouge trompeur), et les contours/outlines doivent rester cohérents
  avec la couleur d'équipe.

### 6. Tableau des scores par équipe
- `draw_scoreboard` groupe les deux équipes (deux colonnes ou deux blocs), avec le score total
  d'équipe en tête et le détail par joueur en dessous (frags/morts individuels gardent leur intérêt).

### 7. Lobby (en ligne)
- `worker/src/lobby.js` : un salon « équipes 3v3 » s'annonce avec `max = 6` (pas 8) et un champ de
  mode (`ver` suffit déjà à séparer les versions de protocole si on préfère ne pas mélanger FFA et
  équipes dans la même liste). La « partie rapide » (`quick`) doit remplir les deux équipes de façon
  équilibrée plutôt que le salon le plus rempli tout court.
- Pas besoin de toucher au perçage NAT / STUN : ça reste de la découverte, le protocole de jeu ne
  change pas de nature.

### 8. Bots (pour tester sans 6 humains)
- `bot_think` doit connaître sa propre équipe et celle des autres pour ne viser/juger que les
  ennemis. Utile à la fois pour les tests automatisés et pour compléter une équipe en attendant des
  joueurs.

### 9. Tests (`run_tests()`)
- Pas de dégâts entre deux joueurs de la même équipe (épée, rail, roquette, souffle).
- Équilibrage automatique à l'arrivée (3e joueur rejoint l'équipe la moins fournie).
- Victoire d'équipe au `FRAG_LIMIT` (somme, pas un seul joueur qui farm).
- Spawn du bon côté pour chaque équipe.

### 10. Vérification
- Build sans warning, `--test`, puis un vrai match **3v3** via plusieurs instances `--bot` (le
  protocole actuel ne supporte qu'un bot hôte + un bot client dans `docs/CODE.md` §6 — il faudra
  soit lancer plusieurs clients humains/bots manuellement, soit étendre le mode bot à plus de deux
  instances si on veut l'automatiser complètement).

## Décisions encore ouvertes (à trancher avant de coder)

1. **Carte** : adapter la carte actuelle (rapide) ou en construire une dédiée aux équipes (mieux à
   terme, plus de travail). *Recommandation : V1 sur la carte actuelle (spawns nord/sud), carte
   dédiée plus tard si le mode plaît.*
2. **Déséquilibre d'effectif** (un joueur quitte en 3v2) : rattrapage automatique (relance un bot,
   ou juste accepter le déséquilibre) ?
3. **Dégâts de zone sur un allié** (roquette/mine dont le souffle touche un coéquipier à côté de la
   cible) : à exclure comme les tirs directs — à confirmer.
4. **Rejoindre un ami dans la même équipe** : pas en V1 (équilibrage auto uniquement), à ajouter plus
   tard si demandé.

## Ordre conseillé pour avancer

Étapes 1 à 3 (équipe + pas de friendly fire + score d'équipe) suffisent pour un 3v3 jouable en local
rapidement. Spawns dédiés, HUD par équipe et lobby en ligne (étapes 4, 5/6, 7) peuvent suivre une
fois que la base est validée en jeu. C'est un chantier à plusieurs sessions, pas un patch rapide —
mieux vaut avancer étape par étape et vérifier (`--test` + partie réelle) à chaque fois plutôt que
tout faire d'un coup.
