# Nouveautés de la branche `dev`

Tout ce qui a été ajouté par rapport à `origin/dev` (dernier commit commun : « Port macOS + musique »), dans l'ordre où
c'est arrivé. Chaque fonctionnalité a été faite sur sa propre branche (colonne « Branche »), puis tout a été réuni dans
`dev` avec les nouveautés d'`origin/dev` (bouton MISE A JOUR, musique stéréo, CI Windows et release).

Pour chaque point : ce que ça change en jeu, comment le régler, où est le code, comment le vérifier.
Le détail technique complet est dans [`CODE.md`](CODE.md) (numéros de section entre parenthèses).

| # | Fonctionnalité | Branche |
|---|---|---|
| 1 | Rendu, jump pads, railgun, bots, aimant de visée | (avant les branches) |
| 2 | Mode équipes 1v1 à 4v4, éclairage, ombres, bloom | (avant les branches) |
| 3 | Réglages graphiques modifiables sans recompiler | `graphismes-modifiables` |
| 4 | Skins, effets du monde, néons, météo, ambiances, qualité | `skins-effets-ambiances` |
| 5 | Molette : un geste = un changement d'arme | `molette-changement-arme` |
| 6 | Code rangé en dossiers (`src/`) | `arborescence-fichiers` |
| 7 | Changement d'arme visible | `retour-changement-arme` |
| 8 | Barre de vie avec traînée des dégâts | `hud-barre-de-vie` |
| 9 | Vrai modèle de lance-roquettes | `modele-lance-roquettes` |
| 10 | Second écran : carte et statistiques en direct | `second-ecran-carte` |
| 11 | Carte : seulement soi et ses équipiers | `carte-equipiers` |
| 12 | Carte en 3D inclinée (ou de dessus) | `carte-vue-3d` |
| 13 | Courbe de vitesse | `courbe-vitesse` |
| 14 | Carte de chaleur de fin de match | `carte-chaleur` |
| 15 | Fonds sous les textes du HUD | `hud-fonds` |
| 16 | Caméra de mort | `camera-de-mort` |
| 17 | Réunion avec `origin/dev` (mise à jour, stéréo, CI) | `dev` |

---

## 1. Rendu, jump pads, railgun, bots, aimant de visée

- **Décor** : textures générées plus riches (sol et murs séparés, joints biseautés, boulons, salissures, grain fin) ;
  les faces cachées des blocs ne sont plus dessinées.
- **Menu principal** : fond en dégradé (le décor se voit en bas), halo sur le titre, panneau derrière les boutons.
- **Jump pads** : vrai bug corrigé. En bhop on arrive sur le pad en l'air, et le pad (0,3 m) se comportait comme un
  mur au lieu de propulser. Deux tests de régression (au sol et en l'air à grande vitesse).
- **Railgun en main** : canon octogonal, frein de bouche, bague d'énergie, lunette (générateur de prismes réutilisable,
  `prism_z`).
- **Bots** : le niveau facile est vraiment plus imprécis et réagit plus lentement ; son bhop n'est plus parfait.
- **Aimant de visée** (joueur humain seulement) : correction très douce vers une cible proche du viseur (cône d'environ
  4°, ligne de vue vérifiée), jamais de verrouillage.

## 2. Mode équipes 1v1 à 4v4, éclairage, ombres, bloom

- **Équipes** (§3.11) : l'hôte choisit 1v1 à 4v4 dans HEBERGER ; équipes équilibrées à l'arrivée et rééquilibrées entre
  deux matchs, pas de tir allié, score et victoire par équipe, couleurs bleue / rouge, radar des alliés.
- **Éclairage** (§3.12) : lightmaps précalculées avec occlusion ambiante, ombres dynamiques des joueurs, ciel dégradé
  avec soleil et étoiles, ville au loin, bloom autour de ce qui brille.

## 3. Réglages graphiques modifiables sans recompiler

- Tout l'aspect visuel se règle dans **`graphismes/rendu.ini`** : une trentaine de clés commentées (soleil, ombres,
  occlusion, brouillard, ciel, ville, bloom, textures…). `sol.bmp`, `mur.bmp`, `grain.bmp` (facultatifs) remplacent les
  textures générées.
- **F5 en jeu** relit tout et refait l'éclairage sans relancer (« GRAPHISMES RECHARGES »).
- Le gameplay et le réseau ne lisent jamais ce dossier : deux joueurs avec des réglages différents jouent ensemble.
- Code : `src/graphismes/rendu_ini.c` (§3.13). Mode d'emploi : `graphismes/README.md`.

## 4. Skins, effets du monde, néons, météo, ambiances, qualité

- **5 skins** (ROBOT, POUSSIN, NINJA, CHAT, CHEVALIER) : écran SKIN du menu avec aperçu 3D, couleur de l'équipe,
  animation de course, de saut, de glissade et de visée. Envoyé aux autres joueurs (`NET_VER` 13). Code :
  `src/perso/skins.c`.
- **Effets du monde** : lumières dynamiques (explosions, rail, roquettes en vol), marques d'impact, ondes au sol, fumée,
  éclats à la couleur de l'équipe au kill.
- **Néons** sur les plateformes, les murs de wall-run et le centre, avec leur lueur sur le décor ; **rebond coloré** de
  la lumière.
- **Ciel et météo** : nuages, poussière, pluie, orage (éclairs et tonnerre), reflets du soleil.
- **Ambiances** (PARAMETRES > CIEL) : couchant, nuit, jour, orage (`graphismes/ambiance_*.ini`).
- **Qualité** (PARAMETRES > QUALITE) : BASSE / MOYENNE / ULTRA pour les petites machines.
- **Contours** colorés autour des joueurs et **traînées de vitesse** au-delà de 16 m/s.
- Détails : §3.14.

## 5. Molette : un geste = un changement d'arme

- **Avant** : une rotation de molette envoie des dizaines d'événements (roue libre, inertie du pavé tactile ou de la
  Magic Mouse) et chacun inversait les deux armes : l'arme clignotait pendant environ 2 s.
- **Maintenant** : une rafale d'événements ne compte qu'une fois. Il faut 0,15 s de repos entre deux gestes
  (`WHEEL_QUIET`) et 0,3 s entre deux changements (`WHEEL_COOLDOWN`) ; sur macOS la phase d'inertie est ignorée.
- Code : `wheel_accept` dans `src/plateforme/fenetre.c`. Test : « molette : rafale de 2 s=1 ».

## 6. Code rangé en dossiers

- `main.c` (16 000 lignes) est devenu une **table des matières** qui inclut, dans l'ordre, les fichiers de `src/`
  rangés par thème : `armes/` (une arme = un fichier), `perso/` (skins), `reglages/` (tous les `#define`), `rendu/`,
  `interface/`, `reseau/`, `son/`, `plateforme/` (dont `mac/`), `tests/`…
- Le code compilé est **identique** (sortie du préprocesseur comparée avant / après). Les scripts de compilation ne
  compilent toujours que `main.c`.
- Arborescence complète et rôle de chaque fichier : §2 de `CODE.md` et `README.md`.

## 7. Changement d'arme visible

- Le **nom de l'arme** apparaît sous le viseur (à sa couleur, avec une barre qui se vide) puis s'efface en 0,9 s
  (`WP_FLASH_T`).
- L'**emplacement** choisi s'allume dans la barre des armes en bas à droite.
- L'**arme remonte en pivotant** au lieu d'apparaître d'un coup.
- Code : `weapon_select` (`wpFlashAt`), `draw_hud`, `draw_viewmodel`. Capture : `--shot changearme.bmp`.

## 8. Barre de vie avec traînée des dégâts

- Après un coup, la partie perdue reste affichée **en blanc**, devient rouge et **se vide** après 0,4 s
  (`HP_GHOST_HOLD`, `HP_GHOST_RATE`) : on voit combien le coup a retiré.
- **Éclat vert** quand on se soigne, **graduations** tous les 25 PV, reflet sur le haut de la barre.
- Code : `hp_ghost_tick` dans `src/interface/hud.c`. Capture : `--shot vie.bmp`.

## 9. Vrai modèle de lance-roquettes

- Ce n'était qu'une boîte verte. Il a maintenant un tube à facettes, une bouche évasée, une tuyère, des colliers, un
  viseur lumineux, des poignées, des bandes chaudes, et l'ogive de la roquette chargée dans la bouche.
- **Une LED par roquette** sur le dessus : on voit ses munitions sans regarder le HUD.
- Code : `draw_viewmodel` dans `src/rendu/viewmodel.c`.

## 10. Second écran : carte et statistiques en direct

- Si **un deuxième écran est branché**, une fenêtre plein écran s'y ouvre toute seule. Elle ne prend jamais la souris
  ni le clavier du jeu. Le jeu recompte les écrans chaque seconde : brancher ou débrancher en cours de partie est suivi.
- **À gauche, la carte** : blocs (plus clairs quand ils sont hauts), jump pads, recharges, pièces, roquettes en vol,
  ennemis en solo ; chaque joueur est une flèche à sa couleur, avec son nom, ses PV et 2 s de trajet ; toi, avec ton
  cône de vision.
- **À droite, les stats** : mode, chrono, limite de frags, scores d'équipe, classement (frags, morts, ratio), ta vitesse
  et ta vitesse max, chaîne de tir, arme et munitions, derniers kills. En solo : vague, score, kills, ennemis restants.
- Hors partie : « la carte de la partie s'affichera ici ».
- Réglage : PARAMETRES > 2e ECRAN (`second_ecran=` dans `velocity.ini`).
- Technique : contexte OpenGL partagé avec le jeu (police et textures communes), 30 images par seconde, sans synchro
  verticale pour ne pas ralentir le jeu. Code : `src/interface/carte_ecran.c` (dessin) et `src/plateforme/second_ecran.c`
  (fenêtre, Windows et macOS). Captures : `--shot carte.bmp`, `carte_dm.bmp`, `carte_dm_equipes.bmp` (§3.15).

## 11. Carte : seulement soi et ses équipiers

- La carte ne sert pas de radar contre l'adversaire. Pendant la partie on n'y voit que **soi et ses équipiers**, avec
  leurs projectiles et leurs pièces ; en chacun pour soi, seulement soi.
- **Tout le monde** apparaît en spectateur et une fois le match fini.
- Code : `maps_sees`. Test : « joueurs visibles en equipes=2, chacun pour soi=1, partie finie=4 ».

## 12. Carte en 3D inclinée (ou de dessus)

- Au choix, la carte se voit **de dessus** ou **en 3D**, vue en plongée depuis le sud.
- En 3D, les **hauteurs** se lisent : chaque joueur est un pion à sa couleur avec la flèche de son regard sur la tête,
  une **ombre au sol** sous lui et un **poteau pointillé** jusqu'au sol quand il est en l'air ; les traînées suivent les
  sauts. Les murs extérieurs sont rabotés pour ne rien cacher.
- Réglage : PARAMETRES > 2e ECRAN, qui fait défiler **NON → HAUT → 3D** (`carte_vue=` dans `velocity.ini`). Angle,
  recul et champ de vision : `carte_3d` dans `graphismes/rendu.ini`.
- Code : `maps_draw_3d`. Captures : `--shot carte3d.bmp`, `carte3d_dm.bmp`.

## 13. Courbe de vitesse

- Dans le panneau des stats, la **vitesse de chaque joueur visible** au fil du temps (toi en trait épais) : les
  60 dernières secondes pendant le match, **tout le match** une fois fini. La courbe s'interrompt pendant les morts.
- Un point toutes les 0,5 s ; remise à zéro à chaque nouveau match.
- Code : `maps_speed_tick`, `maps_draw_speed`. Test : « courbe de vitesse : points en 2 s=4 ».

## 14. Carte de chaleur de fin de match

- À la fin du match, la carte montre **où l'on est mort** (taches rouges) et **d'où l'on a tiré** (taches jaunes). Les
  taches se cumulent : les zones chaudes sautent aux yeux. Elle s'affiche toujours vue de dessus.
- Chaque kill est enregistré au moment où il arrive (`maps_heat_add`, appelé sur `EV_KILL`) ; rayon d'une tache :
  `MAPS_HEAT_R`.
- Capture : `--shot carte_dm_equipes_fin.bmp`. Test : « carte de chaleur : 2 points ».

## 15. Fonds sous les textes du HUD

- Des **plaques sombres** avec un liseré clair sous les infos de partie, la mêlée, la vitesse, la chaîne de tir, le
  panneau d'arme, et sous chaque ligne de killfeed : tout reste lisible sur un ciel clair ou une explosion.
- Opacité réglable sans recompiler : `hud_fonds` dans `graphismes/rendu.ini` (0 = aucun fond).
- Code : `hud_panel` dans `src/interface/texte.c`.

## 16. Caméra de mort

- Quand on est tué, la caméra **recule, monte et pivote vers le tueur** en zoomant, pendant 1,6 s (`KILLCAM_T`) ; elle
  ne traverse pas les murs.
- À l'écran : bandes noires de cinéma, « **ELIMINE PAR** Rival », l'**arme** utilisée, les **PV qu'il lui reste**, et
  des crochets autour de lui qui se resserrent. Le bas du HUD s'efface pour laisser voir la scène ; le choix du loadout
  reste disponible. Le respawn garde son délai (2,5 s).
- En solo, la caméra se tourne vers l'ennemi vivant le plus proche qui te voyait (« TUE PAR UN DRONE »).
- Pas de caméra pour un suicide ni une chute.
- Réglage : PARAMETRES > CAM. MORT (`camera_mort=` dans `velocity.ini`).
- Code : `src/effets/camera_mort.c`, `draw_killcam_hud` (§3.16). Capture : `--shot dm_mort.bmp`.

## 17. Réunion avec `origin/dev`

Les nouveautés d'`origin/dev` sont intégrées et rangées dans l'arborescence :

- **Bouton MISE A JOUR** : vérifie, télécharge (SHA-256 vérifié) et installe la dernière version de la branche
  `release`. Il a maintenant son fichier, `src/interface/mise_a_jour.c` (§3.17). Le menu principal affiche LOADOUT,
  SKIN et PARAMETRES sur une ligne, puis MISE A JOUR ; « MAJ AUTO » est dans PARAMETRES.
- **Musique stéréo** (`src/son/music.h`), **CI Windows** (build MSVC et `--test` à chaque push) et **CI release**
  (binaires Windows et macOS construits depuis `stable`).
- Le code fusionné a été vérifié de la même façon que pour le point 6 : sortie du préprocesseur identique entre le
  fichier fusionné à plat et l'arborescence.

## 18. Fusion avec le dépôt original (`origin/stable` : compétences et cartes)

Le dépôt original a avancé sur `stable` dans l'ancien `main.c` d'un seul bloc ; tout est porté dans l'arborescence `src/` :

- **Compétences** (emplacement générique, touche COMPETENCE) : PIECE, MARQUE, CHARGE, RICOCHET, ALL IN ONE, MIRAGE ;
  choix dans LOADOUT (`skill=` dans `velocity.ini`), HUD des charges sous l'arme. Nouveau fichier
  `src/combat/competences.c` ; détails dans `docs/COMPETENCES_MARKSMAN.md`.
- **Cartes** : `MapDef` et 4 nouvelles cartes (LE PUITS, LE CANYON, LA BOUCLE, LE DOJO), bandes d'accélération,
  ancres de grappin, pads visés ; **kill zone** (`W_VOID`, kill crédité au dernier attaquant), bots prudents au bord
  du vide (`src/jeu/kill_zone.c`). Choix de la carte et ROTATION dans le menu principal **et** dans HEBERGER UNE
  PARTIE ; `--map N` pour les bots et les captures (§3.18 de `docs/CODE.md`).
- **Raccords avec `dev`** : départs par camp sur toutes les cartes en mode équipes, annonce LAN avec le mode **et**
  la carte, lightmaps/ombres/bloom sur les nouvelles cartes (ciel et brouillard propres à chaque carte, `rendu.ini`
  et ses ambiances pour l'Arène), carte du second écran et radar calés sur l'emprise réelle de la carte, chute hors
  de la carte conservée sur les cartes fermées. `NET_VER` passe à **14** (les deux côtés avaient pris 13).
- Vérifié : compilation macOS sans avertissement, toutes les lignes `--test` de `dev` et de l'original présentes et
  conformes (+ une ligne : départs par camp sur LE CANYON), captures, matchs bot contre bot sur LE CANYON et LE DOJO
  (le client sans `--map` charge bien la carte de l'hôte). Pas encore vérifié sous Windows.

## 19. Bots qui savent se déplacer, jauge de style, course au grappin, nouvelles musiques

- **Navigation des bots** : chaque carte a son graphe calculé tout seul (sols, pads, ancres, grappin). Les bots
  passent les vides du Canyon (pad → îlot → grappin vers l'autre falaise), montent la spirale du Puits, contournent
  les piliers du Dojo, et tirent en route. Simulation de test : les 6 trajets d'essai arrivent, sans chute
  (avant : Canyon et Puits impossibles). Match de bots sur Le Canyon : 1 chute au lieu de 3 à 6.
- **Jauge de style D → S** (à droite du viseur) : la vitesse, le wall-run, le grappin et les beaux kills (en l'air,
  à pleine vitesse, au grappin, ricochet, enchaînés, parry) la remplissent ; les coups reçus la vident. Points du solo
  × 2 en S, musique plus intense en A et S.
- **Mode COURSE** (bouton à côté de SOLO) : course au grappin façon Spider-Man sur la nouvelle carte **LA TOILE**,
  quatre avenues de gratte-ciel au-dessus du vide, 13 portes, chrono, écart au record à chaque porte, **fantôme** de
  ton meilleur temps, chute = retour à la dernière porte, R pour recommencer. Record dans `velocity_course.txt`.
  Rendue plus facile : 31 gros **points de balancement** dorés et lumineux, un **aimant** qui envoie le grappin
  vers celui que tu vises à peu près (repère tournant dessus), une corde ramenée à 18 m pour un vrai balancier à
  hauteur des portes, de l'élan en avançant pendant le balancement, des portes plus grandes et plus de passage
  obligé sous une passerelle. Un pilote de test très simple finit le parcours en 41 s.
- **5 nouvelles musiques**, une par carte : Le Puits (dub techno sombre, cloches FM), Le Canyon (breakbeat du désert
  avec swing), La Boucle (synthwave, arpèges), Le Dojo (drum and bass, gamme japonaise), La Toile (trance qui monte
  avec les portes). Le moteur gère maintenant une gamme, un timbre de lead et un swing par morceau ; l'Arène est
  identique à l'échantillon près.
- Docs : §3.19 à §3.21 de `docs/CODE.md`. Tests : navigation, style, course, musiques.

## 20. LA CITADELLE et le décor maillé (retirés au point 23)

- **Nouvelle carte LA CITADELLE**, dans l'esprit des shooters tactiques à l'architecture soignée (carte originale) :
  ville fortifiée en plein soleil, pierre, enduit, tuiles et coupoles turquoise. Deux spawns, trois couloirs, site A
  (cour de la fontaine sous une arcade), site B (marché couvert à colonnes), place du clocher au centre, remparts à
  chemin de ronde, terrasses accessibles par pads, paysage hors les murs (collines, mer, montagnes).
- **Beaucoup plus de polygones** : ~180 000 triangles de décor (avant : quelques centaines de boîtes). Nouveau
  système de **décor maillé** (colonnes, arches, coupoles, toits de tuiles en rangées, fenêtres à volets, auvents,
  arbres, rambardes, lanternes, amphores, fontaine...) éclairé par sommet avec la même lumière que les lightmaps
  (soleil, ombres, occlusion), et boîtes de collision invisibles derrière.
- **Éclairage et textures par carte** : plein soleil sur la Citadelle (le reste garde `rendu.ini`), textures de
  pierre et d'enduit, nouveau morceau « Citadelle » (groove à mi-tempo, sol dorien, lead façon guitare).
- Moteur : jusqu'à 768 boîtes par carte, lightmaps libérées au changement de carte, `--pos` pour les captures.
- Vérifié : tous les tests (aucune régression), match de bots sur la carte (4 kills, navigation, 0 chute).

## 21. Règles de partie, modes d'équipe, boutique, spécialisations, L'OASIS (carte retirée au point 23)

- **Règles choisies par l'hôte** (HEBERGER > RÈGLES) : mode, grappin oui/non, munitions normales / ×2 / infinies,
  boutique, spécialisations, durée.
- **Modes** : CHRONO (le plus de kills dans le temps), **3 ZONES** (rester dedans pour la prendre, l'équipe qui en
  tient le plus marque), **DRAPEAU** (le porteur qui meurt le laisse sur place, un coéquipier peut le reprendre, un
  défenseur qui reste à côté sans être touché le rend), **BOUCLIER** (les ROUGES le surchargent, les BLEUS tiennent).
  Pas de bots dans ces trois modes pour l'instant.
- **Boutique** : une arme de départ au choix parmi 4, des pièces à chaque kill, mort et objectif, et à chaque mort on
  achète d'autres armes (gardées pour le match) ou des munitions.
- **Spécialisations** : ASSAUT, REMPART, ÉCLAIREUR, SOUTIEN, ARTIFICIER (LOADOUT).
- **Nouvelle carte L'OASIS** : tactique, sans grappin, une salle d'apparition fermée par équipe (l'adversaire y
  brûle), trois couloirs (ruines, temple, bazar), deux tours de guet.
- Corrigé au passage : le sol d'une carte pouvait disparaître quand ses coins et son centre étaient sous d'autres
  boîtes (test de face enfouie sur une grille 5 × 5).

## 22. Fusion avec le dépôt original (mode classé)

Le dépôt original (`void19845/velocity`, branche `dev`) a ajouté le **mode classé** dans l'ancien `main.c` d'un seul
bloc. Cette branche part de notre `dev` et y fusionne le sien : on garde l'arborescence `src/` et on y reporte son
code.

- **PARTIE CLASSEE** (bouton à côté de PARTIE RAPIDE) : cote Elo **calculée et stockée par le serveur** (Durable
  Object `Ratings` du worker Cloudflare, `worker/src/ratings.js`), sans compte (identité anonyme `ranked_id` /
  `ranked_key` dans `velocity.ini`). Matchmaking par cote via le lobby, 10 matchs de placement, paliers BRONZE à
  MAITRE, encadré **CLASSEMENT / TOP 3** dans le menu principal, colonne CLASSE dans le tableau des scores et
  « +18 MMR -> OR II » en fin de match. Quitter un match classé en cours = défaite.
- **Où est le code** : nouveaux fichiers `src/reseau/classe.c` (formule Elo, paliers, identité),
  `src/reseau/classe_match.c` (suivi du match, rapport de fin), `src/reseau/classe_serveur.c` (requêtes au serveur de classement) et `src/plateforme/http.c` (requêtes HTTP : WinHTTP sous Windows, `curl` sous macOS) ; le reste dans les
  fichiers existants (`protocole.c`, `hote.c`, `client.c`, `en_ligne.c`, `config.c`, `menus.c`, `scores.c`, `hud.c`,
  `demarrage.c`, `tests.c`). Détails : §4 « Mode classé » de `docs/CODE.md`.
- **Une partie classée force chacun pour soi**, les règles par défaut (FRAGS, grappin, munitions normales, ni
  boutique ni spécialisations), une carte sans portes de course et **aucun bot**, quels que soient les choix de HEBERGER : les nouveaux modes et la
  boutique de cette branche ne faussent pas la cote.
- **`NET_VER` passe à 16** : les deux côtés avaient pris des numéros différents (14 pour le mode classé chez
  l'original, 15 pour les règles et objectifs ici). `PkAnnounce` gagne un champ **`ranked`** explicite (l'original
  utilisait `pad[1]`, que notre `PkAnnounce` n'a plus depuis le champ `map`).
- **Corrections au passage** :
  - le bouton **MISE A JOUR** revient sur le dépôt `void19845/velocity` (`UPD_REPO`) au lieu du fork ;
  - l'**aimant de visée** n'est plus une aide passive : c'est la compétence **AIMANT** (LOADOUT), 4 s d'aimant puis
    45 s de recharge non remplie au respawn, utilisable partout (solo, PvP, classé), activation visible des adversaires ;
  - le **second écran** est désactivé par défaut (`second_ecran=0`) : à activer dans PARAMETRES > 2e ECRAN.
- **Textures exportées** : `velocity.exe --export-textures <dossier>` écrit les textures générées par le code en
  `.bmp` (`sol`, `mur`, `grain`, `pierre`, `enduit`). Le dossier `graphismes/` du dépôt les contient (prêtes à
  retoucher), et la release publie un **`Velocity-Windows.zip`** (exe + `graphismes/`) à côté de `velocity.exe`.
- Tests : lignes `classe …` de `--test`, `node test.js` (lobby classé, Elo, concordance des rapports, abandon, TOP),
  captures `--shot ranked.bmp` et `--shot menu.bmp`, match de bots classé sur le lobby local
  (`--ranked --frags 2`, §6 de `docs/CODE.md`).

## 23. Retour aux cartes néon : objectifs partout, LA TOILE en CHALLENGE

- **LA CITADELLE et L'OASIS sont retirées** (elles sortaient de l'esthétique néon), avec tout ce qui n'existait que
  pour elles : décor maillé (`src/monde/decor.c`), `citadelle.c`, `oasis.c`, boîtes invisibles `B_HIDDEN`, textures
  pierre / enduit (et leurs `.bmp`, clés `pierre`, `enduit`, `decor_occlusion` de `rendu.ini`), éclairage propre à
  une carte, salles de départ, carte sans grappin, morceau « Citadelle ». `MAXB` revient à 256 ; environ 36 Mo de
  mémoire statique en moins (le décor maillé réservait 750 000 sommets).
- **3 ZONES, DRAPEAU et BOUCLIER sur les cinq cartes néon** (ARENE, LE PUITS, LE CANYON, LA BOUCLE, LE DOJO) : une
  zone centrale et une zone près de chaque camp, un drapeau au fond de chaque camp (à plus de 40 m l'un de l'autre),
  le bouclier des BLEUS sur une position haute ou couverte attaquable de plusieurs côtés, et des départs par équipe
  près de leur drapeau. Coordonnées et raisons : §3.22 de `docs/CODE.md`.
- **Un peu de géométrie** pour que ça tienne : un plateau central néon dans le gouffre du CANYON (zone B, deux pads
  vers les falaises : sans grappin, on peut enfin traverser) et deux balcons à mi-hauteur au centre du PUITS (zones
  A et C, bouclier). Sur les petites cartes, les zones sont plus petites (PUITS × 0,6, DOJO × 0,8 : `MapDef.objScale`).
- **LA TOILE devient une carte CHALLENGE** : plus dans la ligne CARTE, ni en partie (hébergée, rapide, classée), ni
  dans la ROTATION, ni en solo ; on y va par le bouton **CHALLENGE** (anciennement COURSE) du menu principal.
- **ROTATION corrigée** : elle passe à la carte suivante qui accueille le mode en cours (avant, une partie 3 ZONES
  pouvait tourner vers une carte sans zones, ou vers LA TOILE).
- **Bots** : le graphe de navigation sait maintenant « courir jusqu'au bord puis tomber » vers un sol plus bas (le
  sommet du Puits vers un balcon) ; un test vérifie un chemin de chaque camp vers chaque objectif.
- `NET_VER` 17. Captures : `--shot objectifs_zones.bmp --map N` (aussi `_drapeau`, `_bouclier`). Tests : « carte
  CHALLENGE », « rotation », « objectifs » (sol, boîtes, vide, bords, disques, pads, distances, côtés, symétrie,
  chemins avec et sans grappin).

---

## 24. Jauge de style réservée au CHALLENGE

- La jauge **D → S** (et son multiplicateur de points) n'existe plus qu'en **CHALLENGE** (mode COURSE, LA TOILE) :
  en PvP et en solo par vagues elle ne monte pas, n'est pas affichée et les points restent ×1 (`style_on()` dans
  `src/jeu/style.c`). Capture `--shot style` : jauge forcée (`styleShow`). Test : « style hors CHALLENGE ».

## 25. Entre-deux de rendu : HUD allégé, ombres et halo adoucis, écran GRAPHISMES

- **HUD** : on garde la barre de vie (graduations, traînée des dégâts) et les fonds sous les textes, mais en voile
  léger (`hud_fonds` 0,45 → 0,22) et **sans le liseré clair** du haut (`hud_panel`, `src/interface/texte.c`) ; le fil
  des kills suit la même opacité.
- **Lumière** : ombres moins noires (`occlusion_min` 0,15 → 0,45, `ciel_lumiere` 0,52 0,54 0,74 → 0,64 0,66 0,84,
  `rebond_sol` 0,16 0,13 0,12 → 0,36 0,31 0,29, `soleil_force` 1,3 → 1,2, `ombre_opacite` 0,5 → 0,4) : l'escalier
  central de l'Arène reste lisible à contre-jour. Bloom discret (`bloom` 0,7 3 → 0,5 4), soleil plus petit
  (`soleil_taille` 8 30 120 → 6 16 50, `soleil_halo` 0,30 0,55 0,42 → 0,16 0,30 0,30, `reflets_soleil` 0,5 → 0,25).
  Défauts du code (`src/graphismes/rendu_ini.c`) = `graphismes/rendu.ini` ; ambiances NUIT et JOUR ajustées.
- **Écran GRAPHISMES** (PARAMETRES > GRAPHISMES) : QUALITE, CIEL, OMBRES, BLOOM, EFFETS D'ECRAN, FLASHS D'IMPACT,
  PLEIN ECRAN, AFFICHER LES FPS, SECOND ECRAN et sa VUE (HAUT / 3D), rappel de F5, dossier `graphismes` trouvé, aide
  du réglage survolé. PARAMETRES garde les curseurs, AXE Y, CAMERA DE MORT, PARTIES EN LIGNE, MISES A JOUR AUTO et
  les commandes. Libellés complets, mise en page qui tient en 1024 × 640 ; Échap revient d'un écran à la fois.
- **Mode DRAPEAU** : « DRAPEAU BLEU » (et « LES BLEUS MARQUENT », « ZONE A PRISE PAR LES BLEUS » dans les messages),
  états des drapeaux sur une ligne sous le score au lieu de le chevaucher (`src/interface/modes_hud.c`).
- Captures : `--shot graphismes.bmp`, `settings_small.bmp`, `graphismes_small.bmp`. Tests : « graphismes defauts »,
  « menus : ECHAP depuis GRAPHISMES ».

## 26. Chrono de match en temps réel, calé sur l'heure du serveur

- **Avant** : le chrono avançait avec les ticks de l'hôte ; sous 10 images/s (`dt` plafonné à 0,1 s) ou pendant le
  hitstop, il prenait du retard sur l'horloge, et le client recevait le chrono avec le retard du paquet.
- **Maintenant** (`src/reseau/horloge.c`) : l'hôte compte le temps réel écoulé ; le worker du lobby donne son heure
  (`time` → `time <ms>`), l'hôte et le client s'y calent (aller-retour le plus court), et chaque instantané porte
  l'heure de l'hôte à l'envoi : le client ajoute le trajet du paquet. Sans lobby (LAN), le client se cale sur
  l'horloge de l'hôte. Seul le chrono change : la simulation, elle, ralentit toujours sous 10 images/s.
- `NET_VER` **18** (`PkSnap.clk`, `clkSrv`). Le worker doit être redéployé (`npx wrangler deploy`) pour répondre à
  `time` ; d'ici là le jeu se cale sur l'hôte, sans erreur.
- Tests : « horloge de match » (`--test`), « heure du serveur » (`node test.js`) ; match de bots par un lobby local :
  les deux se calent (17–20 ms d'aller-retour) et se rejoignent par le lobby.

## 27. Plus de changement de loadout sur l'écran de mort

- Les touches d'arme, d'épée et de compétence ne font plus défiler le loadout pendant la mort (ni le sélecteur
  « LOADOUT DU PROCHAIN RESPAWN ») : le loadout se choisit au menu **LOADOUT** (pause), appliqué au respawn suivant.
  La boutique (règle BOUTIQUE) s'ouvre toujours à la mort. Test : « ecran de mort ».

## 28. Jamais de pause en ligne

- **Hitstop** : en LAN et en ligne, il ne fige plus ta simulation (les autres joueurs, eux, continuaient) ; l'impact
  frame et les effets restent. En solo, rien ne change.
- **Fenêtre déplacée ou redimensionnée, menu de fenêtre** : la boucle de Windows (et le suivi d'événements de
  macOS) bloquait le jeu, et l'hôte coupait les joueurs au bout de 5 s. La boucle est maintenant `main_frame()`,
  appelée aussi par un minuteur pendant ces boucles (`WM_TIMER` sous Windows, `NSTimer` sous macOS).
- **macOS** : plus d'App Nap (`beginActivityWithOptions`), le jeu ne ralentit pas fenêtre cachée.
- Le menu ECHAP laissait déjà la partie tourner. Test : « hitstop ».

## 29. Mode VAGUES : 100 vagues, boss, boutique de comportements, INFINI

Le solo devient le mode **VAGUES** (bouton VAGUES du menu, à la place de « SOLO - VAGUES »). Détail : §3.5 de
[`CODE.md`](CODE.md).

- **100 vagues** avec une courbe de puissance voulue : 1-33 faciles (les ennemis arrivent un type à la fois), 34-66 le
  sommet du joueur (beaucoup d'ennemis faibles que ses achats balaient), 67-100 les ennemis rattrapent (sentinelles,
  bastions, élites qui changent de comportement, PV jusqu'à ×1,5).
- **Aucune inflation** : les dégâts des ennemis ne montent jamais (ceux de leur type, `VG_*_DMG`), seuls leurs PV
  montent, et peu ; aucun achat n'augmente une stat.
- **Nouveaux ennemis** : SENTINELLE (vise 1,4 s avec un rayon visible, tir rapide parable), ESSAIM (par 5, plonge et
  repart), BASTION (marche au sol, bouclier de face : à contourner ou à frapper d'en haut).
- **Toutes les 10 vagues, un choix** : vagues simples (carte fermée), vagues difficiles (LE PUITS / LE CANYON, crédits
  ×1,5) ou un challenge (COURSE sur LA TOILE en moins de 75 s, ou DÉFENSE : tenir une zone 40 s), qui rapporte des
  crédits.
- **Boss** avec barre de vie : PYRAMIDE (25, le parry : grosse orbe chargée à renvoyer, puis à 50 % des charges à
  parer), TESSERACT (50, cube 4D animé, immobile, invulnérable tant que ses invocations vivent : contrôle de foule),
  SPHÈRE (75, roule et frappe le sol : ondes à sauter, ne craint presque rien depuis le sol), HEXAÈDRE (100, quatre
  phases : précision, parry, foule, tout à la fois).
- **Vague 100 terminée** : temps et score, puis **INFINI** : les vagues arrivent au bout du chrono (30 s), les boss
  attendent qu'on ait tout tué, et chacun a une attaque secrète (PRISME, HYPERCUBE, MÉTÉORES, DÉPLIAGE).
- **Crédits** à chaque kill, dépensés dans la **boutique de modificateurs** (TAB entre deux vagues, ou depuis le
  sélecteur) : chaque arme, mêlée et compétence a un chemin de 5 étapes **comportementales** (FOURCHE, GEL, FRACTURE,
  VORTEX, MINE, ÉCLATS, CONTAGION, ONDE de parry, MIROIR, FRAPPE au dash, APPÂT du leurre…).
- **Temps de la partie** exposé pour le mode speedrun : `vagues_temps_100()`, `vagues_splits_ms()` (vagues 25 / 50 /
  75 / 100), et `vg_speedrun_begin` / `vg_speedrun_end` où brancher l'envoi du temps.
- Code : `src/solo/` (`ennemis_vagues.c`, `boss.c`, `ameliorations.c`, `vagues.c`), `src/rendu/vagues_rendu.c`,
  `src/interface/vagues_hud.c` ; réglages : section « Mode VAGUES » de `src/reglages/equilibrage.c`.
- Vérifier : `--test` (lignes `vagues`, `boutique vagues`, `effets vagues`, `bastion`, `pyramide`, `tesseract`,
  `sphere`, `hexaedre`, `boss vaincu`, `vague 100`, `INFINI`, `temps vagues`, `challenges`) ; captures
  `--shot vagues_pyramide.bmp` (`_charge`), `vagues_tesseract`, `vagues_boule`, `vagues_hexaedre`, `vagues_boutique`
  (`_small`), `vagues_selecteur`, `vagues_fin`, `vagues_infini`.

## 30. Speedrun : classements en ligne, speedrun complet

- **Écran CHALLENGE** (bouton du menu principal) : une ligne par catégorie (COURSE de chaque carte CHALLENGE,
  SPEEDRUN COMPLET, VAGUE 100) avec ton record, et le **classement en ligne** de la catégorie choisie : record du
  monde, top 10 (écart au premier), ton rang et ton meilleur temps. LANCER LA COURSE / LANCER LE SPEEDRUN.
- **Fin de course** : ligne CLASSEMENT (« #3 sur 37 - record perso enregistre », « ton meilleur temps reste… »,
  refus et raison, ou « hors ligne, temps garde sur cette machine »). Le fantôme du record reste local.
- **Speedrun complet** : toutes les cartes CHALLENGE à la suite, chrono global (somme des courses, décomptes
  exclus), temps à chaque arrivée comparé au record perso (vert / rouge), 2 s puis la carte suivante, bilan final ;
  R repart de la première carte. Record perso dans `velocity_speedrun.txt`. Avec la seule LA TOILE aujourd'hui,
  c'est une course seule, mais un classement à part, prêt pour les prochaines cartes.
- **Vague 100 la plus rapide** : catégorie prête côté jeu et serveur ; le mode VAGUES n'a qu'à appeler
  `speedrun_begin(SR_VAGUES100)` au départ et `speedrun_report(SR_VAGUES100, ms)` (ou `_splits` avec les vagues de
  boss) à la fin de la vague 100.
- **Serveur** (`worker/src/speedrun.js`, même Durable Object et même identité que le mode classé) : `/speedrun/start`
  (ticket), `/speedrun/report`, `/speedrun/top`. Anti-triche minimal : ticket à usage unique, temps ≤ temps réel
  écoulé depuis le ticket, plancher par catégorie (portes à 45 m/s), temps intermédiaires cohérents. **À déployer**
  par le propriétaire (`npx wrangler deploy`) ; d'ici là l'écran affiche « classement indisponible » et les temps
  restent locaux, sans erreur.
- Code : `src/jeu/speedrun.c`, `src/reseau/speedrun_serveur.c`, `menus.c` (`draw_challenge_menu`), `course_hud.c` ;
  détails au §4 « Speedrun » de `CODE.md`. Tests : lignes `speedrun …` (`--test`) et `speedrun :` (`node test.js`) ;
  captures `challenge`, `course_fin`, `speedrun`, `speedrun_fin` ; de bout en bout contre un lobby local avec
  `SPEEDRUN_LOBBY=127.0.0.1:8791` (voir `CODE.md`).

## 31. Tirs à la tête, chaîne de tirs pleine, Ventouse piège, onde de l'Enclume

Branche `claude/equilibrage-tete-1385aa`. `NET_VER` **19**.

- **Tirs à la tête** (§3.4) : une sphère de tête en haut de chaque joueur (1,54 m debout, 0,84 m accroupi, calée sur
  la tête des skins) et un point faible au sommet des drones et rushers. Seuls les tirs directs de précision en ont :
  - **rail** ×1,5, et c'est un **one-shot** ;
  - **Élan** ×1,35, **sans plafond PvP** (jusqu'à 90 PV à pleine vitesse, pas de one-shot) ;
  - le ricochet de pièce (il vise tout seul) et les armes de zone n'ont pas de tête.
  Le tireur envoie le point d'impact ; **l'hôte vérifie** qu'il est sur la tête de la cible telle qu'il la voit et
  retire le drapeau sinon. Le plafond `PVP_HIT_CAP` ne saute que pour ces touches validées (`host_apply_damage_fl`).
- **Chaîne de tirs pleine = one-shot** : pas de nouvelle jauge, c'est la CHAINE DE TIR existante. Une touche à la tête
  la fait avancer de 2 ; pleine (8), « ONE-SHOT PRET » s'affiche dans son cadre (son de verrou) et le prochain tir de
  rail ou d'Élan qui touche un joueur le tue ; la chaîne repart ensuite de zéro, touche ou non. Même règle pour tous ;
  l'hôte tient une chaîne miroir par joueur (+ la valeur annoncée dans `NetPState.spare[1]`) et refuse un one-shot qu'il
  n'a pas vu venir. Contre les ennemis du solo et des VAGUES : même règle, la touche les tue, **sauf les boss** des
  VAGUES (touche normale : pas de boss tué d'un coup) ; l'armure des boss et le bouclier du bastion s'appliquent après.
- **Retours** : chiffres « TETE » (doré) et « ONE-SHOT » (or et rouge), son `S_HEADSHOT`, éclat doré sur la tête,
  « TETE ! » chez la victime, killfeed `[RAIL TETE]` / `[RAIL ONE-SHOT]`, caméra de mort « ONE-SHOT A LA TETE ».
- **Ventouse** (option A, « piège ») : collage plus large (0,45 m au lieu de 0,15), une mine posée sur le décor explose
  seule quand un adversaire passe à moins de 2 m (armée 0,4 s après le collage), **2 mines au départ**. Dégâts et rayon
  inchangés.
- **Enclume** (plan 1b de `PLAN_SUITE.md`) : l'onde dépend de la **hauteur de chute** (rayon 2,5 + 0,3 × H jusqu'à 9 m,
  mur de 0,6 + 0,1 × H jusqu'à 2,6 m) et de la **vitesse horizontale** (ovale étiré vers l'avant jusqu'à ×1,8,
  raccourci derrière jusqu'à ×0,6). Elle **se propage** à 28 m/s : chaque cible est jugée une fois, au passage du
  front, et ses pieds au-dessus du mur l'esquivent. Un piqué raté immobilise 0,6 s + 0,02 s par mètre de chute ; la
  première touche annule l'immobilisation (rebond, dash, vitesse rendue). Mur d'onde orange dessiné chez tous
  (`EV_SLAM` : rayon, mur, cap et vitesse de l'ovale), repère d'impact en ovale avec la hauteur du mur, HUD
  « PIQUE … mur … m ». Les bots sautent au passage du front.
- Réglages : `HEAD_*`, `CHAIN_*`, `MINE_STICK_GROW`, `MINE_PROX_*`, `SLAM_R_*`, `SLAM_WH_*`, `SLAM_FWD_*`, `SLAM_BACK_*`,
  `SLAM_WAVE_SPEED`, `SLAM_STUN_*` dans `src/reglages/equilibrage.c`.
- Tests (`--test`) : « tete », « chaine de tirs », « chaine pleine », « elan a la tete », « hote : tete declaree… »,
  « drone : rail au point faible », « bots : tirs vises a la tete », « ventouse piege », « enclume onde / ovale /
  propagation / mur ». Captures : `--shot tete.bmp` (chiffres TETE / ONE-SHOT, chaîne pleine), `dm_enclume` (mur d'onde).
- **Match bot avant / après** (6 matchs de 2 min de chaque côté, mêmes loadouts ; temps pour tuer moyen et kills) :

  | Loadouts (hôte / client) | Avant | Après |
  |---|---|---|
  | défaut (rail, roquettes, épée) | 8,4 s, 9 kills | 5,4 s, 10 kills |
  | rail + Élan, épée / Enclume | 5,1 s, 9 kills | 7,3 s, 11 kills |
  | Ventouse + Élan + Enclume des deux côtés | 2,6 s, 7 kills | 5,4 s, 8 kills |
  | Ventouse, roquettes, Enclume / rail, roquettes, épée | 2,2 s, 7 kills | 2,4 s, 7 kills |
  | Éventail + Scie, Lance / Yoyo | 15,4 s, 8 kills | 4,9 s, 1 kill (peu de combats) |
  | Tesla + rail, Poings / rail + Tesla, Égide | 12,9 s, 8 kills | 4,4 s, 9 kills |

  Ce que les bots en tirent : 3 têtes à l'Élan (validées par l'hôte, aucun refus), pas de tête au rail sur cette série
  (1 à 5 par match sur la série précédente), **jamais la chaîne pleine** (8 touches de suite : c'est une récompense de
  joueur précis, les bots touchent ~38 % de leurs tirs). **Ventouse : toujours 0 touche** : les bots lancent leurs
  mines à 10–20 m d'une cible qui bouge, elles collent au décor et le piège ne s'est jamais déclenché (les bots ne
  posent pas de pièges sur les passages). **Enclume** : l'onde touche ~1 fois sur 8 avant comme après, car les bots
  piquent de 2–3 m (petites ondes : 3,1 m de rayon) ; le plan récompense les gros piqués, qu'ils ne cherchent pas.
  Le temps pour tuer varie surtout avec le hasard des rencontres (un match de 2 min fait 1 à 11 kills) : à
  vérifier en partie humaine, surtout la Ventouse et l'Enclume.

## 32. Classements dans une base D1

- Le mode CLASSE (cotes, rapports de match, TOP) et les classements SPEEDRUN sont maintenant stockés dans la base
  **Cloudflare D1 `leaderboard`** (binding `LEADERBOARD`, `worker/wrangler.toml`) au lieu du Durable Object `Ratings`,
  supprimé par la migration `v3`. Le serveur calcule toujours l'Elo et vérifie les temps : mêmes routes, mêmes
  réponses, **rien ne change dans le jeu** (pas de `NET_VER`).
- Les écritures tiennent chacune en une requête pour rester justes quand plusieurs instances du Worker répondent en
  même temps : un match n'est appliqué qu'une fois (`matches.done` = 2 pendant le calcul), cotes modifiées en
  relatif, ticket speedrun consommé par `DELETE … RETURNING` (détails au §4 « Mode classé » de `CODE.md`).
- **À déployer** par le propriétaire : `cd worker && npx wrangler deploy` (les tables se créent à la première
  requête). Vérifier : `curl https://<lobby>/ranked/top` → `{"ok":true,"top":[],"me":null}`.
- Code : `worker/src/ratings.js`, `speedrun.js`, `index.js` ; `dev-server.js` imite D1 (`memoryD1`). Tests :
  `node test.js` (nouveaux : rapports simultanés, deux matchs simultanés du même joueur, ticket rejoué en même temps).

## 33. Spécialisations retirées

- Les cinq spécialisations (ASSAUT, REMPART, ÉCLAIREUR, SOUTIEN, ARTIFICIER) n'existent plus : plus de rangée
  SPECIALISATION dans LOADOUT, plus de bouton SPECIALISATIONS dans les règles de HEBERGER (BOUTIQUE prend toute la
  ligne), plus de bonus de dégâts, de vitesse, de dash, de soins ni de jauges, et les bots n'en tirent plus au hasard.
- `velocity.ini` : `regles=` a 5 champs (`mode,grappin,munitions,boutique,minutes`, comme `--regles`) ; un ancien
  `regles=` à 6 champs se relit (le 5e est ignoré) et une ancienne ligne `spec=` est ignorée sans erreur.
- Réseau : **`NET_VER` 20** (`PkSnap.rFlags` sans le bit 2, `NetPState.spare[0]` libre) : les parties en 19 et en 20
  ne se voient pas.
- Tests : les lignes « specialisations » et « SOUTIEN » sont retirées ; nouvelle ligne « reglages : ancien regles=
  (6 champs) ».

## 34. Pseudo verrouillé et connexion Discord

- Le champ **PSEUDO** du menu ne s'ouvre plus d'un clic : il faut un **double-clic** (astuce « double-clic pour
  modifier » au survol). Quand la saisie se termine avec un nouveau pseudo, le jeu le renvoie tout de suite au serveur
  (`/ranked/hello`) : le classement et les speedruns affichent le nouveau nom.
- Bouton **DISCORD** à droite du pseudo : ouvre la connexion Discord dans le navigateur, puis le jeu attend la
  validation. Un compte Discord = **une seule identité de classement** : sur un autre PC, la même connexion redonne
  la même cote, les mêmes matchs, les mêmes temps et le même pseudo (chaque PC reçoit sa propre clé). La première
  connexion rattache l'identité actuelle au compte. Le compte lié s'affiche en vert au-dessus du bouton.
- `velocity.ini` : nouvelle clé `discord=` (affichage). Réseau de jeu inchangé (pas de `NET_VER`).
- **À faire par le propriétaire** : créer l'application Discord, déclarer la redirection
  `https://<worker>/auth/discord/callback`, mettre `DISCORD_CLIENT_ID` dans `worker/wrangler.toml`,
  `npx wrangler secret put DISCORD_CLIENT_SECRET`, puis `npx wrangler deploy`. Sans ça, le bouton répond
  « DISCORD indisponible : discord non configure ».
- Code : `src/reseau/discord.c`, `field_click` dans `src/interface/menus.c`, `rkNameSent` dans
  `src/reseau/classe_serveur.c`, `worker/src/discord.js` (+ `player_keys` dans `ratings.js`). Tests : lignes
  « discord » et « menus : PSEUDO » de `--test`, 19 lignes « discord » de `node test.js` ; de bout en bout avec le faux
  Discord du `dev-server.js` : `DISCORD_LOBBY=127.0.0.1:8787` (§4 « Mode classé » de `CODE.md`).

## 35. Classement relu au lancement, estimation de fin de match, bots muets

- Le classement (cote, rang, TOP 3) est demandé au lancement **et redemandé tout seul** dans le menu : 15 s après un
  échec (serveur Cloudflare qui se réveille, réseau pas encore prêt), puis toutes les 60 s (`RANK_MENU_RETRY`,
  `RANK_MENU_REFRESH`). Avant, un échec au lancement laissait « classement indisponible » et l'ancienne cote du
  cache jusqu'à la prochaine partie classée.
- Estimation de fin de match classée : un joueur parti compte à la **cote qu'il avait annoncée** (et plus à la
  mienne) : l'estimation affichée en attendant le serveur est la même que la variation du serveur.
- `--bot` : son et musique coupés.
- Tests : ligne « classe rapport : estimation avec la cote du parti » ; vérifié en vrai : lancement sans lobby, lobby
  démarré 6 s plus tard, cote reçue à 15 s ; match classé de bots (5 matchs appliqués, ±60 des deux côtés).

## 36. Patch d'équilibrage : triangle de compétences, SHOTGUN, RICOCHET en fenêtre, Tesla, sentinelle

Demandé par le propriétaire (notes brutes). Chaque valeur est une constante de `src/reglages/equilibrage.c` ; le détail
technique est dans `docs/CODE.md` (§3.4, §3.5, §5, `NET_VER` 21) et `docs/COMPETENCES_MARKSMAN.md`.

- **Triangle MIRAGE ⇒ PIÈCE ⇒ RICOCHET ⇒ MIRAGE**, un pierre-feuille-ciseaux à sens unique (pas une synergie de loadout) :
  - *MIRAGE ⇒ PIÈCE* : le ricochet d'une pièce (rail, Élan, roquette) vise le **leurre adverse** en vue avant le joueur, qui n'est
    pas touché (`DECOY_COIN_LURE`).
  - *RICOCHET ⇒ MIRAGE* : si j'ai RICOCHET, mon tir sur le leurre adverse ne s'y arrête plus, il repart vers le **joueur qui l'a posé**
    (`DECOY_RIC_ALWAYS`).
  - *PIÈCE ⇒ RICOCHET* : le propriétaire n'a rien précisé ; **lecture par défaut** : un projectile en rebond (fenêtre RICOCHET ouverte)
    qui touche une pièce adverse repart vers **son tireur** (`COIN_VS_RIC`). À confirmer ou à changer.
- **ÉVENTAIL devient SHOTGUN** (menus, HUD, LOADOUT, killfeed, tests, docs ; `src/armes/shotgun.c`, `--shot dm_shotgun`, l'ancien
  nom marche encore) : **20 dégâts par plomb** (8 avant, 200 à bout portant en solo). En PvP `PVP_SCALE` 0,7 et `PVP_HIT_CAP` 60 restent :
  le plafond s'applique toujours, la hausse étend surtout la portée du kill en 2 tirs (de ~4 m à ~14 m). **Recul** : une poussée forcée
  **proportionnelle à la vitesse actuelle du tireur** (1 m/s de poussée par m/s, **sans plafond**) pendant 0,10 s, vers l'arrière, au sol comme
  en l'air (50 m/s → 5 m, 30 m/s → 3 m, immobile → aucun recul), et la même sur la cible vers l'avant (× part de plombs touchés) ; le tireur
  garde sa vitesse, et le saut au recul (tir vers le bas) marche toujours grâce au résidu de 7 m/s en l'air, même depuis l'arrêt.
- **TESLA × ALL IN ONE** : un **éclair inévitable** (instantané) sur la cible la plus proche du viseur (cône de ~10°, en vue, 45 m), qui vide
  toute la batterie (20 au moins) : 130 dégâts à pleine batterie (proportionnels), chaîne comme l'arc, PvP × 0,7 plafonné à 60.
- **ROQUETTES × PIÈCE** (comme Ultrakill) : une roquette qui traverse une pièce part vers l'ennemi le plus proche (90 m) et le suit
  jusqu'à l'explosion, × 1,5 par pièce.
- **ÉLAN** : 4 munitions au départ et au plus (3 et 6 avant) ; **le tir enlève toute la vitesse horizontale** du tireur (avant : seulement
  les 15 m/s convertis ; `KIN_BRAKE_ALL`). La vitesse verticale n'est pas touchée (`KIN_BRAKE_VERT` 1 la ferait aussi disparaître).
- **PIÈCE** : 2 charges (4 avant), 5 s par charge (2 s avant).
- **SCIE** : la sphère que le crochet doit toucher pour rappeler la scie plantée passe à 1,2 m (0,6 avant ; aussi pour le viseur
  « GRAPPIN -> RAPPEL SCIE »). Effet de bord : un crochet visant un ennemi ou un mur derrière la scie plantée, dans son axe, la rappelle d'abord.
- **SENTINELLE** (l'ennemi vert) : au lieu de son orbe à 34 m/s, elle tire un **laser** (instantané, annoncé par le rayon qui blanchit,
  parable) ou une **orbe à 110 m/s**, une fois sur deux ; mêmes dégâts qu'avant (12, aucune inflation).
- **RICOCHET réécrit** : une fenêtre de **6 s** pendant laquelle TOUT ce que tu tires (rail, Élan, roquettes, plombs, scie) rebondit
  **sans limite de rebonds ni de portée**, jusqu'à une cible ou la sortie de la carte (garde-fous techniques : 24 rebonds pour le rail,
  6 par plomb, 40 pour une roquette qui vit 15 s). Recharge 20 s, qui ne repart qu'à la fermeture. Un tir ne touche chaque cible qu'une
  fois ; il peut te revenir dessus une fois.
- Réseau : `NET_VER` **21** ; tous les rebonds d'un tir RICOCHET voyagent dans **un seul** événement (`EV_RAIL` sous-type 2).
- Mesure (match bot contre bot de 90 s, un par scénario, avant / après, tableau dans `docs/COMPETENCES_MARKSMAN.md`) : de 5 à 9 kills dans les
  onze matchs, rien ne s'emballe ; PIÈCE lance 9 → 5 pièces ; un tir RICOCHET fait ~9 rebonds ; l'Élan fait 13 → 8 touches ; 3 éclairs de Tesla ; le SHOTGUN touche autant qu'avant (6 et 8 touches contre 6, recul proportionnel).
  Un seul match par case : les moyennes de TTK ne sont pas significatives.
- Tests (`--test`) : lignes « piece », « elan jauge », « elan munitions », « shotgun poussee du tireur », « shotgun PvP », « ricochet … »
  (fenêtre, plafond, une touche par cible, roquette, plombs, scie, réseau), « mirage => piece », « ricochet => mirage »,
  « piece => ricochet », « tesla all in one », « roquette x piece », « scie hitbox du crochet », « sentinelle ».

## 37. Le Yoyo ne tire plus la cible

Demandé par le propriétaire (« retire la traction du Yoyo »). Détail technique : `docs/CODE.md` (« Yoyo », dans le patch d'équilibrage) et
la fiche `docs/armes/03_fleau.md`.

- **Plus de traction** : une touche du Yoyo (fléau, `MK_FLAIL`) inflige ses dégâts **sans déplacer la cible**, ni en PvP (le recul envoyé dans
  `EV_HIT` est nul) ni sur les drones, kamikazes et autres ennemis du mode VAGUES (`damage_enemy(..., (0, 0, 0))` ; les boss ne reculaient
  déjà pas). Avant : 12 m/s vers le lanceur + 3 vers le haut en PvP, drone ramené et arrêté à 2,5 m.
- **Gardé tel quel** : arc de 140°, portée 6 m (+ 0,05 m par m/s), dégâts 18 + 1,5 × v_rel (PvP × 0,7 plafonné à 60), recharges (0,6 s ;
  0,9 s dans le vide), parry à distance (6 m, renvoi ×0,8), pièce frappée (45 m/s, 40 PvE / 28 PvP), points de jauge, dash rendu.
- **Supprimé avec la traction** : `FLAIL_PULL` (12), `FLAIL_PULL_UP` (3), `FLAIL_DRONE_STOP` (2,5), `FLAIL_COMBO_T` (1,0) ; le combo fléau → rail
  (popup « RAPPEL + RAIL », `flail_combo_check`, `P.flailLastT` / `P.flailLastTgt`, ligne de journal `combos fleau -> rail`) et l'enchaînement
  du bot ; le « zip de câble » (`S_HOOK`) que la victime entendait.
- **Gardé volontairement (à confirmer)** : un fléau *paré* tire encore le **lanceur** vers le pareur (`FLAIL_PARRIED_PULL` 16 +
  `FLAIL_PARRIED_UP` 4, « CHAINE RETOURNEE ») : c'est la pénalité du parry, pas la traction de la cible. `FLAIL_PARRIED_PULL` à 0 la retire aussi.
- Boutique VAGUES : l'effet ATTRAIT (« une touche tire l'ennemi vers toi », premier du chemin YOYO de `src/solo/ameliorations.c`) est un achat
  indépendant du Yoyo de base ; il n'est pas touché ici (l'arbre de la boutique est réécrit à part).
- Réseau : `NET_VER` inchangé (**21**) ; ni paquet ni événement ne changent, seule la valeur du recul de `EV_HIT` (arme `W_FLAIL`).
- **Partiellement annulé par le § 38** (10 novembre) : la traction **revient sur appui long** ; l'appui court reste sans traction comme décrit ici.
- Le Yoyo perd son rôle de lanceur de combos : il reste une mêlée de zone à 6 m. Aucune valeur de dégâts ni de portée n'est modifiée pour compenser.
- Tests (`--test`) réécrits : « fleau drone a 5.5 m » (vitesse 0, position inchangée), « fleau PvP » (recul `(0, 0, 0)`), « bot fleau » (le bot
  frappe, aucun recul, plus de combo) ; « fleau pare » (le lanceur est tiré vers le pareur) inchangé.
- Mesure (2 matchs bot contre bot de 90 s par version, `--loadout 0,1,3,0`, tableau dans `docs/armes/03_fleau.md` § 11) : 7 et 7 kills avant, 8 et 6
  après ; coups de Yoyo qui touchent 4 et 6 avant, 3 et 6 après ; l'ancien combo fléau → rail n'avait servi que 2 fois en 2 matchs. Les TTK (2 à 10 s)
  sont du bruit à ce nombre de matchs : rien ne s'emballe ni ne cale, la perte se jouera surtout en partie humaine.

## 38. Yoyo : traction sur appui long

Demandé par le propriétaire, qui revient sur le § 37 : « on la garde, mais on la change pour que ça soit sur un **appui long** et que ça tracte **aussi
longtemps que le joueur maintient** ». Détail technique : `docs/CODE.md` (« Yoyo : traction sur appui long ») et la fiche `docs/armes/03_fleau.md`.

- **Appui court** (touche de mêlée relâchée avant 0,15 s, `FLAIL_HOLD_T`) : **inchangé**, le coup en arc de 140° (dégâts, parry à 6 m, pièce frappée,
  recharges) **sans traction**, comme au § 37.
- **Appui long** (touche tenue plus de 0,15 s) : le coup part **à l'appui** comme avant (réactivité, fenêtre de parry), puis, 0,15 s plus tard, la
  chaîne s'accroche aux **cibles de l'arc à cet instant** (mêmes règles de portée, d'arc et de ligne de vue ; 8 au plus ; aucun nouveau dégât) et les
  **tire vers le lanceur tant que la touche est tenue**, sans limite de durée. Relâcher coupe la chaîne : recharge de mêlée de **0,5 s** (0,9 s si
  l'appui long n'a rien accroché). Le lanceur peut bouger et tirer pendant la traction : le combo fléau → rail est de retour (sans le bonus visuel
  « RAPPEL + RAIL », qui reste supprimé) ; il ne peut pas relancer de coup tant qu'il tient la touche.
- **Ennemis** (drones, kamikazes… du solo / VAGUES) : leur vitesse est ramenée vers le lanceur à 12 m/s (lissée, comme l'effet VORTEX mais avec une
  consigne), ils s'arrêtent à 2,5 m (`FLAIL_DRONE_STOP`, de retour) ; **jamais un boss** ; un kamikaze accroché explose comme avant. *Attention* : la
  boutique v2 limite la mêlée de VAGUES à la Lance et à l'Impulsion (`vg_mk_ok`, menu LOADOUT grisé) : **le Yoyo n'est pas équipable en VAGUES pour
  l'instant**, la traction des ennemis n'y est donc pas jouable tant que ce choix tient (elle est codée et testée hors VAGUES, `--test` et `--shot`).
- **Joueurs (PvP / LAN)** : la victime est tirée **sur sa propre vitesse** (12 m/s au plus, arrêt vers 2 m, le décor l'arrête comme d'habitude) par un
  nouvel événement léger **`EV_FLAIL`** : le lanceur l'envoie à l'accroche puis toutes les 0,1 s tant qu'il tient la touche, la victime enregistre
  « accrochée par X pour 0,3 s » et s'applique la traction à chaque tick ; **l'hôte arbitre** (lanceur et victime vivants et adverses, lanceur au Yoyo,
  fléau non paré, portée) et divise la traction **par deux** si l'Égide de la victime fait face à la chaîne.
- **Contre-jeu** (« une traction de 12 m/s n'est pas une prison ») : la traction se coupe si la cible s'éloigne à plus de **8 m**, si un **mur** coupe
  la ligne de vue plus de 0,25 s, si le lanceur **meurt**, est **étourdi** (Tesla, Enclume) ou voit son fléau **paré** (la chaîne s'enroule, le lanceur
  est tiré vers le pareur comme avant) ; et la victime s'en dégage **par sa vitesse** : courir à l'opposé ne suffit pas (la traction bat la course),
  mais un **dash, un slide ou un bhop rapide** (plus de 16 m/s vers l'extérieur), un grappin ou un rocket jump **arrache la chaîne** (« CHAINE
  ARRACHEE »).
- **Retour visuel et sonore** : une chaîne tendue de la main jusqu'à chaque cible accrochée, tant que la traction dure (chez les autres joueurs aussi,
  spectateurs compris) ; son discret à l'accroche et à la coupure ; chez la victime, le **zip de câble** d'avant est de retour, avec le popup
  « CHAINE DE X ». Rien de nouveau dans le HUD.
- **Bots** : une fois sur deux, à plus de 3,5 m, le bot tient la touche jusqu'à 0,9 s (le match bot exerce ainsi toute la voie réseau) ; journal :
  tractions, secondes tenues, cibles accrochées, chaînes arrachées, et côté hôte accroches / refus / Égide.
- Réseau : **`NET_VER` 22** (nouvel événement `EV_FLAIL`, sous-types `FLE_START`, `FLE_PULL`, `FLE_END` du lanceur vers la victime, `FLE_BREAK` de la
  victime vers le lanceur ; `f[0]` = intensité écrite par l'hôte). Aucun changement de paquet. `CLAUDE.md` annonce encore `NET_VER` 20 : à mettre à jour.
- Constantes (`src/reglages/equilibrage.c`, bloc Fléau) : `FLAIL_HOLD_T` 0,15, `FLAIL_HOOK_MAX` 8, `FLAIL_PULL_SPEED` 12, `FLAIL_PULL_RESPONSE` 14,
  `FLAIL_PULL_ACCEL` 140, `FLAIL_DRONE_STOP` 2,5, `FLAIL_PULL_STOP` 2, `FLAIL_PULL_GAIN` 6, `FLAIL_PULL_SLACK` 2, `FLAIL_PULL_LOS_T` 0,25,
  `FLAIL_PULL_SNAP` 16, `FLAIL_PULL_REFRESH` 0,1, `FLAIL_PULL_LEASE` 0,3, `FLAIL_PULL_END_CD` 0,5, `FLAIL_HOST_TOL` 3, `FLAIL_HOST_PARRY_T` 0,6.
- **Choix à valider** (documentés dans la fiche, § 13) : le coup part à l'appui et l'accroche suit 0,15 s plus tard ; 12 m/s, 140 m/s², arrêt à
  2,5 / 2 m ; l'arrachage de la chaîne au-delà de 16 m/s vers l'extérieur (c'est lui qui rend le dash utile) ; recharge de 0,5 s ; 8 cibles au plus ;
  Égide ÷ 2 (et non bloquée) ; changer d'arme à distance ne coupe pas la traction ; événement à 10 Hz plutôt qu'un champ de l'état à 60 Hz.
- Tests (`--test`) : 11 lignes « fleau traction … » / « fleau appui court » (drone tenu à 2,4 m puis relâché, appui court sans traction, boss, coupures
  distance / mur / mort, fléau → rail pendant la traction, événements du lanceur, victime LAN, contre-jeu course / dash / Égide, arbitrage de l'hôte,
  parry, observateur) ; **les lignes « fleau » existantes sont inchangées**. Captures : `--shot fleau_traction` (deux drones accrochés),
  `--shot dm_fleau_traction` (la chaîne d'un adversaire sur un autre joueur).
- Mesure (matchs bot contre bot en réseau réel, `--loadout 0,1,3,0`, tableau dans `docs/armes/03_fleau.md` § 11) : ARENE 60 s (4 kills, aucune traction), LE DOJO
  90 s (16 et 19 kills, 3 à 4 tractions, 1 à 2 chaînes arrachées par le bot victime : ils fuient à plus de 16 m/s), et une variante de vérification (seuil
  d'arrachage relevé, le bot tient toujours la touche) qui montre une traction soutenue de bout en bout (START, 12 PULL relayés par l'hôte, victime ramenée à
  2,1 m puis tuée au rail, END à la mort du lanceur). Aucun plantage ni désynchronisation visible. **Pas encore joué en vrai** (partie humaine) : le
  réglage fin (12 m/s, 140 m/s², seuil d'arrachage de 16 m/s, recharge de 0,5 s) se jugera en jouant.

## Vérifications faites

- Compilation macOS sans avertissement ; tous les tests `--test` conformes, y compris ceux d'`origin/dev` (stéréo,
  mise à jour, SHA-256) et une ligne de test par nouvelle mécanique.
- Captures de chaque nouveauté et match bot contre bot en réseau réel.
- Fenêtre du second écran essayée en vrai sur un Mac (contexte partagé, police, partie solo en direct).
- **Pas encore vérifié sous Windows** : la CI Windows d'`origin/dev` compilera et testera `dev` à chaque push ; reste
  à essayer le second écran avec deux moniteurs.

## Boutique v2 du mode VAGUES : un seul loadout et arbre de graines

- VAGUES n'accepte plus qu'un loadout : Railgun / Roquettes / Shotgun, Lance / Impulsion, Pièce / All in One (le menu LOADOUT grise le
  reste en partie ; les choix du PvP ne sont pas modifiés).
- Nouvelle boutique : 3 offres tirées par rareté, étapes qui modifient une stat, branches vers les partenaires du loadout, capstones,
  fusions, relance payante, changement d'arme payant (300 crédits). Arbre et règles : `docs/boutique_vagues/` (pour l'instant aucun
  lien mêlée x compétence). Couverture actuelle : 140 étapes / 140, 30 capstones / 30, 10 fusions / 15 (voir §3.5 de `docs/CODE.md`).
- Vérifié : build sans warning, `--test`, capture `vagues_boutique`. Pas encore joué en vrai ni en match bot.

## Mode VAGUES : niveaux d'amélioration de la boutique

- Une étape achetée peut monter de niveau (offre « NIV n+1 » dans la boutique, avec le niveau courant) : +20 % d'effet par niveau sur
  ses paramètres numériques. Profondeur 1 : 60 / 80 / 100 / 120 crédits (niveau 5 au maximum), profondeur 2 : 120 / 160 / 200 (4),
  profondeur 3 : 200 / 300 (3), profondeur 4 : 400 (2), capstone : niveau fixe.
- Les étapes sans paramètre numérique réel (Satellite, Double, Surtension, Soute, Récupération, nombre d'estocs ou de coups de combo,
  Réserve, Pièce recyclée, Rebond de plus, Pluie, Salve double…) ne montent pas et ne sont jamais proposées en amélioration.
- Une fusion a un niveau (somme des niveaux des étapes et du capstone de ses deux branches, divisée par 5) : 100 / 125 / 150 % d'effet.
  Le résumé de la boutique montre la somme des niveaux de chaque graine et le niveau de la fusion.
- Vérifié : build sans warning, `--test` (blocs « niveaux »), capture `vagues_boutique`. Pas encore joué en vrai.

## Mode VAGUES : règle de soin

- Plus de soin sur kill en VAGUES (ni les 8 / 15 PV des ennemis, ni le bonus de mêlée). Hors VAGUES rien ne change.
- Les étapes de soin de la boutique et le vol de vie passent par `vg_heal` : 20 PV de soin par seconde au maximum, aucun soin issu
  d'une réaction en chaîne, jamais au-delà de 100 PV. Le parry rend toujours ses PV.
- Vérifié : tests `--test` (bloc « soin »). Pas encore joué en vrai.

## Mode VAGUES : formes fusionnées (armes hybrides)

- Acheter la fusion d'une paire d'armes à distance donne une **jauge de fusion**, remplie par les dégâts des deux armes. Pleine, la touche
  **T** (réaffectable : PARAMETRES > COMMANDES, « Fusion (arme hybride) ») active l'arme hybride pendant 15 s : elle remplace les deux armes (munitions mises
  de côté puis rendues), hérite des modificateurs des deux arbres, puis les deux armes reviennent. Mêlée et compétence restent disponibles.
- Trois formes : ARTILLERIE ORBITALE (rail + souffles de 3 m par cible et 8 m au bout), CANON SCIE (8 rails fins à 50 % en gerbe de 6° qui
  traversent, recul du Shotgun), OBUSIER (6 mini-roquettes de 2,5 m en gerbe de 8°, recul vers le haut). Jauge, message « FUSION PRETE », nom
  et temps restant dans le HUD ; la boutique annonce « FUSION + FORME » et rappelle la touche. Le niveau de la fusion (100 / 125 / 150 %)
  multiplie les dégâts propres de la forme.
- FISSION et DOUBLE DETENTE (pas d'effet passif codé) deviennent achetables : leur seul effet est la forme fusionnée. DOUBLE DETENTE reste
  inatteignable tant que le capstone RAIL>SHO n'est pas codé.
- Aucun effet hors VAGUES (PvP / LAN inchangés, pas de `NET_VER`). Détail : §3.5 de `docs/CODE.md`.
- Vérifié : build sans warning, `--test` (blocs « hybride », « ARTILLERIE ORBITALE », « CANON SCIE », « OBUSIER »), captures `vagues_hybride*`.
  Pas encore joué en vrai.

## Mode VAGUES : sac de stats (refactor des armes « objet »)

- Rien ne change pour le joueur ni hors VAGUES (PvP / LAN identiques, pas de `NET_VER`) : c'est un refactor de la boutique v2 pour rendre l'ajout
  d'un modificateur d'arme simple. Cahier des charges, API et recettes : `docs/boutique_vagues/ARCHITECTURE_ARMES.md`.
- Les étapes qui modifient une **stat** (dégâts, taille, portée, cône, perte à distance, plombs en plus, délai, coût de jauge, vol de vie, munition
  rendue...) ne sont plus du code : ce sont des lignes de données (`{ stat: 'TAI', op: 'mul', val: 1.15 }` dans `docs/boutique_vagues/outils/effets/*.js`,
  écrites dans `boutique_donnees.c` par `node export_c.js`) lues par le **sac de stats** (`src/solo/boutique_stats.c`, `st()` / `st_ctx()`). Le niveau
  (+20 %) multiplie le delta de chaque modificateur, gain ou pénalité ; des conditions (distance, tête, en l'air, élite, cibles traversées, impact direct,
  rebond n°N, salve pleine), des groupes (le plus fort l'emporte), des phases d'application (un retrait en secondes APRÈS un pourcentage), les opérations
  « vers une valeur » (`to`) et « plafond » (`min`) et l'arrondi entier (`st_int`) existent. Hors VAGUES ou graine non équipée, `st()` rend la base.
- Migrés : les genres génériques `BK_DMG` / `BK_NEAR` / `BK_VOL` / `BK_PTS` (15 étapes) et toute la famille **Shotgun** (14 étapes-stat ; Rafale, Peau
  de plomb, Souffle de culasse, Coup double, Éclats et Détonation restent des événements). `fire_shotgun` lit ses stats une fois par tir (`sg_stats`).
  Les formes fusionnées et la pièce en plombs lisent le même sac.
- Classification exacte des 140 étapes (82 stats, 58 événements ou comportements) : `docs/boutique_vagues/CLASSIFICATION_ETAPES.md`, feuille de route
  de la migration des autres familles (Roquettes, Railgun, Mêlée, Compétences).
- **Préparation des familles restantes** (Roquettes + Railgun, Lance + Impulsion, Pièce + All in One) : le socle a été complété pour les 56 étapes-stat
  qui restent (19 stats nommées, opérations `to` / `min`, conditions `direct` / `bounce` / `first` / `salvefull`, phases, `st_int`) et chaque étape a sa ligne de
  données prête, vérifiée de bout en bout ; la classification donne, par famille, le bloc à coller, la fonction à brancher et les fichiers touchés. Les trois
  migrations peuvent se faire en parallèle sans toucher un fichier partagé.
- Vérifié : build sans warning ; `--test` identique à la référence d'avant refactor (671 lignes, hors 5 lignes instables d'une exécution à l'autre) plus
  16 lignes « sac de stats » (une par opération et par condition) ; deux matchs bot contre bot en réseau réel (host / client, 60 s : loadout par défaut, puis
  Shotgun + Railgun avec des touches de Shotgun arbitrées par l'hôte) sans plantage. Pas encore joué en vrai.

## VAGUES : bombe, carré et mortier

Trois nouveaux ennemis (détail dans `docs/CODE.md` §3.5) : la **BOMBE** (kamikaze rapide qui s'arrête, tique 2 s puis explose sur 5 m), le **CARRÉ** (mini tesseract : les autres ennemis encaissent ×0,1 tant qu'il vit) et le **MORTIER** (orbe lente à pulsation, explosion de 8 m). Ils arrivent aux vagues 12, 18 et 28.
