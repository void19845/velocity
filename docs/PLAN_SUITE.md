# VELOCITY — Plan d'implémentation : la suite

Sept chantiers demandés : **menus qui se chevauchent** (refaits en tout proportionnel), **saut plus haut**,
**refonte de l'onde de l'Enclume**, **kill zone sous la carte**, **nouvelles cartes**, **modificateurs qui tombent
du ciel** (personnels et de partie), un **bouton de mise à jour** depuis GitHub, et plus tard le **mode roguelike**
(non prioritaire).
Ce document fixe l'ordre, les choix techniques, les points d'ancrage dans `main.c` et la façon de vérifier
chaque étape. Rien n'est encore codé.

> Les noms cités (`build_level`, `pick_spawn`…) sont à chercher dans `main.c` ; les numéros de ligne bougent.
> Règles de `CLAUDE.md` valables partout : zéro warning, une ligne `run_tests()` par mécanique, captures
> `--shot`, match bot avant/après, `NET_VER` +1 dès qu'un paquet, un événement **ou la carte** change,
> pas d'accents dans les chaînes C, toute API Win32 nouvelle a son équivalent macOS.

---

## 0. Ordre de travail

Chaque phase se termine par un build propre, `--test`, les captures et un commit sur `dev`.

| # | Phase | Coût | Dépend de | Touche au réseau |
|---|---|---|---|---|
| **0b** | **Bouton MISE A JOUR (dernière version depuis GitHub) — primordial, en premier** | M | — | non (HTTPS vers GitHub, CI modifiée) |
| 1 | Saut plus haut | S | — | non (mais réglage partagé : même exe pour tous) |
| 1b | Refonte de l'onde de l'Enclume (taille, hauteur, portée) | M | 1 | oui : `EV_SLAM` grossit, `NET_VER` |
| 2 | Menus : tout proportionnel à la fenêtre, aucune valeur fixe | M-L | — | non |
| 3 | Infrastructure des cartes (`MapDef`) + kill zone — **fait** (`NET_VER` 13, voir `CODE.md` §3.12) | M | 2 (bouton de carte) | oui : `NET_VER` |
| 4 | Nouvelles cartes : Le Puits, Le Canyon, La Boucle, Le Dojo — **fait** (v1) | S chacune | 3 | oui : `NET_VER` |
| 5 | Modificateurs qui tombent du ciel (personnels + de partie) | L | 3 | oui : événements + `PkSnap` + `NET_VER` |
| 6 | *Plus tard* — Mode roguelike (solo), non prioritaire | L | 3, 4, 5 | non |

Pourquoi la mise à jour d'abord (décision : primordiale) : chaque phase suivante change `NET_VER`, donc deux
joueurs qui n'ont pas la même version ne se voient plus. Tant qu'il n'y a pas de bouton, chaque nouveauté oblige
tout le monde à re-télécharger l'exe à la main. Les joueurs actuels devront le faire **une dernière fois** pour
obtenir la première version qui contient le bouton ; ensuite, toutes les phases arrivent d'un clic. Plus elle sort
tôt, moins il y aura de versions à distribuer à la main.

Pourquoi le reste dans cet ordre : le saut est isolé et change le ressenti de tout le reste (les cartes se dessinent avec la
bonne hauteur de saut). Les menus passent avant les cartes parce que les phases 3, 5 et 6 ajoutent des
écrans (choix de carte, options, choix d'amélioration) : autant poser la mise en page une seule fois.
La kill zone n'a de sens qu'avec des cartes ouvertes sur le vide, donc elle vient avec `MapDef`.
Le roguelike réutilise tout : cartes, vide, modificateurs.

---

## 0b. Bouton MISE A JOUR — en premier (primordial)

> **État (29/10/2026)** : codé sur `dev` (non commité), vérifié — build sans warning, `--test`, captures
> `menu` / `settings` / `update`, match bot, et essai réel : une copie de test a téléchargé le `velocity.exe` de la
> branche `release`, vérifié son SHA-256, s'est remplacée et relancée. Choix faits en codant : SHA-256 écrit en C pur
> (pas de BCrypt ni CommonCrypto), `curl` du système sous macOS (pas de nouvelle fonction Objective-C), vérification
> automatique silencieuse en cas d'échec. Reste : la mise en service (merge dans `stable`, accord du propriétaire)
> et l'essai sur Mac. Description du code : §3.11 de `docs/CODE.md`.

### Ce qui existe déjà
- Dépôt **public** `github.com/void19845/velocity`. Le workflow `release.yml` compile `stable` (Windows + macOS),
  lance `--test`, puis pousse les binaires dans la branche **`release`** : `velocity.exe` et `Velocity-macOS.zip`.
- Le jeu sait déjà faire de l'HTTPS : WinHTTP sous Windows (lobby), `NSURLSession` sous macOS (`mac_ws_*`).
- Il manque : un **numéro de version** dans l'exécutable et un fichier qui dit quelle est la dernière version.

### Source et version
- La CI écrit dans la branche `release` un fichier **`version.txt`** :
  ```
  build=47
  sha=1a2b3c4
  win_size=1834496
  win_sha256=9f86d0...
  mac_size=2310144
  mac_sha256=60303a...
  ```
  `build` = `github.run_number` du workflow Release (croît à chaque publication).
- Le même numéro est **compilé dans le jeu** : `build.bat` et `build_mac.sh` passent `/DVELOCITY_BUILD=%VELOCITY_BUILD%`
  (`-DVELOCITY_BUILD=…`) si la variable existe ; dans `main.c`, `#ifndef VELOCITY_BUILD` → `0` (= compilation locale,
  « version de dev »). Le numéro s'affiche en bas du menu principal (« build 47 »).
- Le jeu lit `https://raw.githubusercontent.com/void19845/velocity/release/version.txt` (pas l'API GitHub, limitée
  à 60 requêtes/heure sans compte). Le cache de raw.githubusercontent peut retarder une nouvelle version de quelques
  minutes : sans conséquence.
- Seule la branche `release` (donc `stable`) est proposée : les joueurs ne reçoivent jamais `dev`.

### Interface
- Comme cette phase passe **avant** la refonte des menus, le bouton est d'abord ajouté au menu actuel
  (`draw_main_menu`, entre PARAMETRES et QUITTER, avec le même pas que les autres boutons) ; la phase 2 le reprendra
  avec la nouvelle mise en page (racine du menu regroupé).
- Bouton **MISE A JOUR**, dont le libellé suit l'état :
  | État | Libellé / affichage |
  |---|---|
  | jamais vérifié | `VERIFIER LES MISES A JOUR` |
  | vérification | `VERIFICATION...` |
  | à jour | `A JOUR (build 47)` |
  | nouvelle version | `INSTALLER LA MISE A JOUR (build 47 -> 52)`, bouton mis en évidence |
  | téléchargement | barre de progression dans le bouton, `TELECHARGEMENT 63 %` |
  | prêt | `REDEMARRER POUR TERMINER` (ou redémarrage automatique) |
  | erreur | message clair dans `draw_status` (pas de réseau, dossier protégé, fichier corrompu) + `REESSAYER` |
- **Vérification automatique** discrète au démarrage (une seule requête, en arrière-plan) : si une version plus récente
  existe, le bouton porte une pastille « NOUVEAU ». Réglage `update_check=1` dans `velocity.ini` et bascule dans
  PARAMÈTRES (« Verifier les mises a jour au demarrage : OUI »).
- Le bouton n'existe que dans le menu principal (jamais en partie, ni dans le menu pause en LAN). Jamais rien en mode
  `--bot`, `--shot`, `--test`.
- Une build locale (`build 0`) peut aussi se mettre à jour, avec l'avertissement « tu remplaces une version de dev ».

### Téléchargement (en arrière-plan)
- Un fil dédié (modèle : `lb_reader`) fait la requête et écrit dans un fichier temporaire **à côté de l'exécutable** ;
  le jeu ne lit que l'état partagé (octets reçus, taille totale, erreur) protégé par une section critique. Le menu
  reste fluide et la musique continue.
- Contrôles avant d'installer : taille = `win_size` / `mac_size`, puis **SHA-256** = `win_sha256` / `mac_sha256`
  (Windows : `BCryptHash` de `bcrypt.lib` ; macOS : `CC_SHA256` de CommonCrypto). Un fichier tronqué ou corrompu
  n'est jamais installé.
- Délais : 5 s pour `version.txt`, 60 s sans aucun octet reçu pour le binaire → erreur.

### Installation
- **Windows** : on ne peut pas écrire sur un `.exe` en cours d'exécution, mais on peut le **renommer** :
  1. `velocity.exe.new` téléchargé et vérifié ;
  2. `MoveFileExA(velocity.exe → velocity.exe.old)`, puis `MoveFileExA(velocity.exe.new → velocity.exe)` ; en cas
     d'échec de la seconde, on remet l'ancien nom (jamais d'exe manquant) ;
  3. `CreateProcessA` du nouvel exe avec `--updated`, puis on quitte proprement (`net_shutdown`, `cfg_save`) ;
  4. au démarrage suivant, `velocity.exe.old` est supprimé, et le menu affiche « mis a jour : build 52 ».
- **macOS** : téléchargement de `Velocity-macOS.zip` dans un dossier temporaire, extraction par
  `/usr/bin/ditto -x -k`, remplacement du `.app` (ancien renommé `Velocity.app.old` puis supprimé), relance par
  `open -n`. Nouvelles fonctions `mac_http_get(url, chemin, progression)`, `mac_sha256_file`, `mac_install_update`
  dans `mac_platform.m` / `.h` (règle de `CLAUDE.md` : chaque appel Win32 a son équivalent `mac_*`). Le
  téléchargement fait par l'appli elle-même n'ajoute pas l'attribut de quarantaine.
- **`velocity.ini` n'est jamais touché** : il est à côté de l'exe (Windows) ou dans `~/Library/Application Support`
  (macOS), seul le binaire change.
- Dossier non inscriptible (ex. `Program Files`) : message « dossier protege : telecharge la nouvelle version a la
  main » et ouverture de la page de la branche `release` dans le navigateur (`ShellExecuteA` / `open`).

### Lien avec le réseau
Deux joueurs avec des `NET_VER` différents ne se voient pas. Bonus : quand REJOINDRE ne trouve rien et qu'une
mise à jour existe, afficher « une nouvelle version existe : tes amis l'ont peut-etre deja ».

### Changements de CI
- `release.yml` : `VELOCITY_BUILD: ${{ github.run_number }}` en variable d'environnement des étapes de compilation
  (Windows et macOS) ; dans `publish`, calcul de `sha256sum` et `stat -c %s` des deux binaires et écriture de
  `version.txt`, ajouté à la branche `release` avec les binaires et le `README.md`.
- `windows.yml` / `macos.yml` (builds de vérification) : rien à changer, ils restent en `build 0`.

### Tests
- `run_tests` : lecture d'un `version.txt` d'exemple (`upd_parse`), comparaison des builds (plus récent / égal /
  build 0), SHA-256 du vecteur de test « abc » (`ba7816bf…`), calcul des chemins `.new` / `.old` à partir du chemin
  de l'exe.
- Capture `--shot update` : menu principal avec le bouton dans l'état « nouvelle version » et la barre de
  progression.
- Essai réel, à faire une fois : copie de `velocity.exe` compilé en `build 1` dans un dossier temporaire, clic sur
  MISE A JOUR → téléchargement depuis `release`, redémarrage sur la nouvelle version, `velocity.ini` intact,
  `velocity.exe.old` supprimé au lancement suivant. Même essai sur Mac quand ce sera possible.

### Mise en service
1. Coder et vérifier sur `dev` (build sans warning, `--test`, capture, essai réel ci-dessus).
2. Merger `dev` → `stable` (règle de `CLAUDE.md` : vérification complète, match bot), ce qui déclenche `release.yml`
   et publie la première version numérotée avec son `version.txt`. **À faire par le propriétaire ou sur son accord
   explicite** : c'est ce qui part chez les joueurs.
3. Distribuer cette version à la main une dernière fois ; vérifier ensuite avec une seconde publication (ex. le
   saut de la phase 1) que le bouton la trouve et l'installe.

⚠ Point d'attention : la phase 0b merge `dev` dans `stable`. Au 29/10/2026, `dev` a **10 commits** d'avance sur
`stable` (port macOS, musique, Yoyo à la place du Fléau, design des compétences, CI) : ils partiraient chez les
joueurs en même temps que le bouton. Si une partie n'est pas prête, faire la mise à jour sur une branche partie de
`stable` puis la reporter dans `dev`.

---

## 1. Saut plus haut

### Constat
`JUMP_V` = 8,4 m/s, `GRAVITY` = 24 m/s² → hauteur = v² / 2g = **1,47 m** (double saut ×0,95 : 1,33 m de plus).

### Proposition
| `JUMP_V` | Hauteur | Écart |
|---|---|---|
| 9,2 | 1,76 m | +20 % |
| **9,6 (retenu)** | **1,92 m** | **+31 %** : on passe par-dessus un joueur debout (1,8 m) |
| 10,2 | 2,17 m | +48 % : les blocs de couverture (≤ 3,5 m) deviennent faciles |

On ne touche **pas** à `GRAVITY` (elle règle aussi la durée des chutes, les pièces, le piqué de l'Enclume).
À relire après le changement :
- `WALLJUMP_UP` (8,8) : passer à ~9,6 pour garder la cohérence saut/wall-jump ;
- `SLAM_REACH_UP` (1,0 m) : « un saut simple esquive l'onde » reste vrai, encore plus ;
- `SLAM_MIN_H` (1,2 m) : un saut simple au sol permet désormais de piquer — à vérifier en jeu ;
- `bot_think` : saut puis double saut au sommet (`P.vel.y < 1`), rien à changer a priori.

### Test
Nouvelle ligne `run_tests` : « saut : hauteur max x.xx m (attendu 1,92) », mesurée en appelant `player_tick`
depuis le sol jusqu'au sommet. Revérifier les lignes existantes escalier / bhop / jump pads.
Match bot avant/après : comparer vitesse moyenne et temps pour tuer.

---

## 1b. Refonte de l'Enclume : une onde qui dépend de la hauteur et de la vitesse

### Constat (`slam_resolve`, `slam_radius`, `slam_dmg`)
- Rayon = `2,5 + 0,06 × |vy|` : avec `|vy|` entre 30 et 45, l'onde fait **4,3 à 5,2 m** quelle que soit la
  hauteur : un piqué de 2 m et un piqué de 25 m se ressemblent.
- Hauteur de l'onde **fixe** : `SLAM_REACH_UP` = 1 m (un saut simple l'esquive toujours).
- La vitesse **horizontale** n'entre nulle part : l'onde est un disque centré sur le point d'impact.
- L'onde est **instantanée** : tout ce qui est dans le disque au tick d'impact est touché, on ne peut pas sauter
  « par-dessus » au bon moment.
- Dégâts : `(25 + 1,5 × |vy|) × slam_kh(H)` (déjà fonction de la hauteur via `SLAM_FULL_H`).

### Nouvelle règle : trois entrées, trois effets
| Entrée (à l'impact) | Ce qu'elle règle |
|---|---|
| **Hauteur de chute** `H` = `P.anvilStartY − P.pos.y` | **taille** (rayon) et **hauteur** du mur de l'onde, durée de l'immobilisation si l'onde rate |
| **Vitesse verticale** `|vy|` | **dégâts** et **recul vertical** des cibles |
| **Vitesse horizontale** `vh` (et sa direction) | **portée vers l'avant** : l'onde s'étire en ellipse dans le sens de la course |

Formules proposées (toutes en `#define`, section Enclume) :
```
R      = clamp(SLAM_R_MIN + SLAM_R_PER_M x H, SLAM_R_MIN, SLAM_R_MAX)        2,5 + 0,3 x H, max 9 m
Hw     = clamp(SLAM_WH_MIN + SLAM_WH_PER_M x H, SLAM_WH_MIN, SLAM_WH_MAX)    0,6 + 0,1 x H, max 2,6 m
avant  = R x min(1 + SLAM_FWD_PER_MS x vh, SLAM_FWD_MAX)                     x(1 + 0,04 x vh), max x1,8
arriere= R x max(1 - SLAM_BACK_PER_MS x vh, SLAM_BACK_MIN)                   x(1 - 0,02 x vh), min x0,6
cotes  = R
degats = (SLAM_DMG + SLAM_DMG_PER_MS x |vy|) x (0,3 + 0,7 x f)               inchange (f = 1 au centre, 0 au bord)
recul  = vertical SLAM_KNOCK_UP_MIN + 3 x Hw ; horizontal dans le sens de l'onde
rate   = immobilise SLAM_STUN + 0,02 x H (0,6 -> 1,0 s) : un gros piqué rate coute cher
```

| Exemple | H | vh | Rayon | Avant / arrière | Hauteur du mur | Esquive |
|---|---|---|---|---|---|---|
| petit piqué depuis un saut | 2 m | 0 | 3,1 m | 3,1 / 3,1 | 0,8 m | saut simple |
| depuis une plateforme | 7 m | 10 m/s | 4,6 m | 6,4 / 3,7 | 1,3 m | saut simple bien placé |
| depuis un perchoir | 13 m | 0 | 6,4 m | 6,4 / 6,4 | 1,9 m | saut simple au sommet, ou double saut |
| grosse chute en course | 20 m | 20 m/s | 8,5 m | 15,3 / 5,1 | 2,6 m | double saut, grappin, ou sortir de la zone |

### L'onde se propage (au lieu d'être instantanée)
- À l'impact, on crée une **onde active** chez le frappeur :
  `typedef struct { int alive; V3 c, dir; float r, rFwd, rBack, R, hw, dmg, t; unsigned char hit[MAXP], hitE[MAXE]; } SlamWave;`
- Elle grandit à `SLAM_WAVE_SPEED` (≈ 28 m/s : 0,3 s pour 8,5 m). À chaque tick (`slam_wave_tick`, appelé par
  `game_tick`), une cible est touchée **une seule fois** quand le front de l'ellipse la dépasse, si ses pieds sont
  sous `c.y + Hw` et au-dessus de `c.y − SLAM_REACH_DOWN`, avec ligne de vue depuis le centre (`los`, comme
  aujourd'hui).
- Conséquence voulue : **sauter au passage du front** esquive l'onde, et plus l'onde est haute, plus il faut sauter
  tôt et haut. Avec le saut à 1,92 m (phase 1), seuls les très gros piqués (H > 13 m) demandent un double saut.
- **Rebond / immobilisation** : à l'impact, le frappeur est immobilisé comme aujourd'hui ; si l'onde touche
  quelqu'un pendant sa propagation, l'immobilisation est annulée et le rebond (`SLAM_BOUNCE`, +1 dash, points de
  jauge) part à cet instant. L'onde finie sans touche → l'immobilisation (allongée selon H) continue.
- **Contact direct** (`slam_contact`, cible en l'air touchée pendant le piqué) : inchangé, la cible prend tout.
- **Parry de l'onde** (Égide, `EV_MELEE_PARRY`) : même chemin qu'aujourd'hui, la touche part simplement plus tard ;
  `SLAM_PARRY_GRACE` est à revoir (le délai de propagation s'ajoute à la latence).

### Réseau et rendu
- `EV_SLAM` porte en plus : `Hw`, la direction `dir` (2 flottants), `rFwd` et `rBack` (`f[]` a 7 places : centre 3
  + R + Hw + dir 2 = 7 ; `rFwd`/`rBack` se recalculent chez le receveur à partir de `vh` envoyé dans `sub`, ou on
  agrandit `NetEvent`). Les touches restent validées chez le frappeur (`hit_player_ex`, arbitre `host_handle`).
  `NET_VER` +1.
- Rendu (`render`, passe additive, hors liste d'affichage) : un **mur d'onde** elliptique qui s'étend — bande de
  quads de hauteur `Hw`, opaque en bas, transparente en haut, couleur orange de l'Enclume — plus la poussière et les
  éclats actuels sur le front. Chaque client dessine l'onde des autres à partir de `EV_SLAM`.
- **Repère d'impact** (`draw_slam_marker`, `slam_predict`) : afficher l'ellipse prévue et un trait de la hauteur du
  mur, pour que le joueur voie ce qu'il va produire avant de piquer. HUD : « ONDE 8,5 m / mur 2,6 m ».
- `bot_think` (réaction à `ST_SLAM` d'un adversaire) : sauter quand le front arrive, et double saut si
  `Hw > 1,9 m` ; le bot qui pique profite de l'ellipse (piquer dans le sens de sa course).

### Tests et mesures
- `run_tests` : rayon et hauteur du mur pour H = 2, 7, 13, 20 m ; portée avant/arrière à vh = 0 et 20 m/s ;
  cible à 6 m touchée vers t ≈ 0,21 s et pas avant ; cible en l'air à 1,5 m : esquive un petit piqué, touchée par un
  gros ; une cible n'est touchée qu'une fois par onde ; immobilisation annulée par une touche tardive.
- Match bot avant/après en Enclume contre Enclume et Enclume contre rail : taux de touche des ondes
  (`slamWaveHits / slamWaves`), dégâts moyens, immobilisations. Objectif : les gros piqués touchent plus souvent
  mais ratés coûtent plus cher ; le plafond PvP de 60 reste la limite.

---

## 2. Menus : tout proportionnel à la fenêtre, aucune valeur fixe

### Constat (captures 1600×900)
- **Menu principal** : le libellé « JOUEUR CONTRE BOT » est écrit 6 px sous le bas du bouton SOLO et
  chevauche son liseré (`draw_main_menu` : `y += 58` puis `y += 16`, pour un bouton de 52 px).
- Toutes les positions sont des **décalages fixes** depuis le centre (`cx - 620`, `cy - 300`…) avec des tailles
  fixes. La colonne de boutons descend jusqu'à `cy + 418` (statut compris) et l'aide des commandes s'étend de
  `cx - 130` à ~`cx + 610`. Résultat :
  - en 1366×768 ou 1280×720, le bas du menu, RETOUR / PAR DÉFAUT de PARAMÈTRES et VALIDER du LOADOUT sortent de
    l'écran ;
  - sous ~1250 px de large, la colonne de boutons et l'aide des commandes se chevauchent ;
  - la fenêtre bot (800×450) est illisible (mais elle n'est pas faite pour être lue).
- Même schéma dans `draw_pause`, `draw_join_menu`, `draw_settings`, `draw_loadout`, `draw_death_loadout`.

### Règle (décidée) : aucune valeur en pixels dans les menus
Toutes les positions, tailles, marges, interlignes **et tailles de police** se calculent à partir de la taille de
la fenêtre. Plus aucun `cx - 620`, `y += 54`, `bw = 420`, `fontH[1] * 0.3f` en dur dans les écrans.

### 1. L'unité d'interface
- `uiU` = 1 % de la hauteur utile : `uiU = fminf(winH / 100.0f, winW / 177.0f)` (le second terme évite que le menu
  déborde sur un écran très étroit). Recalculée dans `in_resize` (Windows et macOS) et au début de `draw_hud`.
- Toute dimension de menu s'écrit en `uiU` : bouton = `5,5 uiU` de haut, marge = `1 uiU`, colonne = `46 uiU` de
  large… Les seules constantes restantes sont ces **coefficients** sans unité, regroupés en `#define UI_*` en haut
  de la section Interface.

### 2. Polices recréées à la bonne taille
- Aujourd'hui `make_font(i, px)` crée 3 polices bitmap fixes (18, 28, 56 px) au démarrage.
- Désormais : tailles **proportionnelles** (`2 uiU`, `3,1 uiU`, `6,2 uiU`), recréées quand `uiU` change d'au moins
  un pixel (`glDeleteLists(fontBase[i], 224)` puis `make_font`). Sous Windows, `CreateFontA` +
  `wglUseFontBitmapsA` ; sous macOS, `mac_make_font` (déjà paramétré par `px`) : le port suit sans nouvelle API.
- Recréation **différée** à l'image suivante d'un redimensionnement (pas pendant le glissé de la bordure), pour ne
  pas reconstruire 50 fois.
- Les fenêtres du mode `--bot` (800×450) en profitent aussi.

### 3. Mesure du texte et ajustement automatique
- `text_w(f, s)` : largeur réelle d'une chaîne (Windows : `GetTextExtentPoint32A` mémorisé par caractère au moment
  de `make_font` ; macOS : largeurs renvoyées par `mac_make_font`). Aujourd'hui `fontW` n'est qu'une moyenne.
- `button`, `field`, `slider` choisissent la plus grande police qui tient dans leur largeur ; un libellé trop long
  passe en police inférieure, puis est coupé avec « ... » en dernier recours.

### 4. Mise en page en flux (plus aucun `y +=` à la main)
```c
typedef struct { float x, y, w, h, gap; int dry; } UiBox;      /* zone, curseur, espacement ; dry = mesurer sans dessiner */
static UiBox ui_area(float fx, float fy, float fw, float fh);   /* fractions de la fenetre (0..1) */
static UiBox ui_split(UiBox *b, int col, int ncol);             /* colonnes de largeur egale */
static int   ui_button(UiBox *b, float hU, const char *label);  /* hauteur en uiU ; avance le curseur */
static void  ui_label(UiBox *b, const char *t, int font);       /* un libelle occupe sa place : plus de chevauchement */
static void  ui_row(UiBox *b, int n, ...);                      /* n boutons cote a cote (FACILE / NORMAL / DIFFICILE) */
```
- Chaque écran est décrit **deux fois par la même fonction** : une passe `dry` qui mesure la hauteur totale, puis la
  passe réelle, centrée verticalement. Si le contenu dépasse, le pas vertical se resserre jusqu'à un minimum, puis
  la zone devient **défilante** à la molette (`K_WHEEL`) avec une barre fine.
- Le cas « JOUEUR CONTRE BOT » qui chevauche SOLO disparaît par construction (le libellé prend sa place dans le
  flux).

### 5. Disposition qui s'adapte au format
- **Deux colonnes** (boutons + aide des commandes, réglages + touches dans PARAMÈTRES) si le rapport largeur/hauteur
  le permet (≥ 1,45), sinon **une seule colonne** : l'aide passe sous les boutons ou est masquée, les touches de
  PARAMÈTRES passent sous les réglages (zone défilante).
- **LOADOUT** : les cartes d'armes forment une grille dont le nombre de colonnes = ce qui tient dans la largeur
  (7 en 16:9, 4 sur un écran étroit), la ligne suivante s'enroule.
- **REJOINDRE** : le nombre de parties affichées dépend de la hauteur disponible (au lieu de 7 fixe), le reste
  défile.
- Tableau des scores, écran de mort, fil des kills, HUD : même unité `uiU` (dans un second temps, après les menus).

### 6. Regrouper le menu principal (11 boutons aujourd'hui)
- `JOUER` → sous-écran : SOLO VAGUES, ROGUELIKE (phase 6), CONTRE BOT (3 niveaux) ;
- `EN LIGNE` → PARTIE RAPIDE, HÉBERGER (avec choix de carte, phase 3), REJOINDRE / REGARDER ;
- LOADOUT, PARAMÈTRES, QUITTER.
- Nouvelle variable `menuPage` (0 = racine, 1 = jouer, 2 = en ligne) ; Échap revient à la racine.

### Écrans à convertir
`draw_main_menu`, `draw_join_menu`, `draw_pause`, `draw_settings` (+ `draw_controls`), `draw_loadout`,
`draw_death_loadout`, `draw_status`, `draw_scoreboard`, puis les futurs écrans (choix de carte, choix d'amélioration
du roguelike) directement avec les nouveaux outils.

### Vérification
- Nouvelle option `--size LxH` pour forcer la taille de fenêtre en mode `--shot`. Captures `menu`, `settings`,
  `loadout`, `join`, `pause` (deux nouvelles scènes) en **1920×1080, 1600×900, 1366×768, 1280×720, 1280×1024 (5:4)
  et 800×450**. Critère : aucun chevauchement, rien hors de l'écran, texte lisible, mêmes proportions partout.
- Test `run_tests` : pour chaque taille, la passe `dry` de chaque écran tient dans la fenêtre (ou active le
  défilement), et aucun bouton n'en recouvre un autre (rectangles mémorisés pendant la passe).
- Contrôle `grep` final : plus aucun littéral de pixel dans les fonctions `draw_*` des menus.
- macOS : `in_resize` déclenche aussi la recréation des polices ; à valider par la CI (compilation) puis sur Mac.

---

## 3. Infrastructure des cartes et kill zone

> **État : fait** (branche des cartes, `NET_VER` 13). Écarts avec le plan ci-dessous : le choix de carte est dans
> le menu principal (ligne « CARTE < NOM > » + bouton ROTATION, sous le pseudo), pas dans un sous-écran EN LIGNE ;
> `MapDef` a en plus `start`, `enemyFloor`, `specUp`, `sky`, `fog` ; le lobby en ligne ne transmet pas la carte
> (le client la reçoit dans `PkWelcome`) ; pas de son grave à l'approche du vide (vignette seulement) ; les mines
> sous `killY` disparaissent simplement. Détails : `CODE.md` §3.12.

### 3.1 `MapDef`
Reprend le §4.7 de `IDEES_ARMES_CARTES.md`, mis à jour :
```c
typedef struct {
    const char *name;            /* sans accents : "ARENE", "LE PUITS" */
    void (*build)(void);         /* ajoute boites, pads, cellules */
    const float (*spawns)[3]; int nspawns;
    float killY;                 /* plan de mort ; -1000 = aucun (carte fermee) */
    float enemyMinY, enemyMaxY;  /* zone d'apparition et plancher des drones en solo */
    float enemyRMin, enemyRMax;
    V3 menuCam; float menuPitch; /* camera du menu (menu_backdrop) */
    int solo;                    /* jouable en solo vagues */
    int music;                   /* piste de music.h */
} MapDef;
static int curMap;
```
- `build_level()` devient `build_level(int map)` : remet `nb = npu = 0`, appelle `maps[map].build()`.
  **`build_map_arene()` = la carte actuelle, inchangée** (`run_tests` dépend du couloir x = −20 et des
  coordonnées des pads).
- `load_map(int m)` : `build_level(m)`, `glDeleteLists(worldList, 1)` + `build_world_list()`, remise à zéro des
  projectiles, pièces, scies, mines, cellules ; `music_set(maps[m].music, …)`.
- À brancher sur `MapDef` : `pick_spawn` (table `pts` codée en dur), `reset_game` (départ `0,0,24`),
  `menu_backdrop`, la vue spectateur (`v3(0, 12, ms(36))`), `spawn_enemy` (rayon `ms(28)`..`ms(48)`, hauteur 9-18),
  le plancher des drones (`if (e->pos.y < 1.0f)` dans `enemy_tick`), `SAW_LOST_Y`.
- **Réseau** : l'id de carte voyage dans `PkWelcome.pad[0]` (le client fait `load_map` avant `dm_local_start`),
  dans `PkAnnounce.pad[0]` (affiché dans REJOINDRE : « [LAN] Hote — LE PUITS ») et dans `EV_MATCH` (`target` ou
  `f[0]`) pour changer de carte entre deux matchs. Ajouter au lobby (`host …`) un champ carte si l'on veut
  l'afficher pour les parties en ligne (`worker/src/lobby.js` + `worker/test.js`). `NET_VER` 11 → 12.
- **Choix** : dans le sous-écran EN LIGNE, bouton « CARTE : < LE PUITS > » (flèches) avant HÉBERGER ; option
  « ROTATION » qui change de carte à chaque nouveau match. En solo, même bouton, limité aux cartes `solo`.
  Le choix est mémorisé dans `velocity.ini` (`map=`).

### 3.2 Kill zone
- Dans `game_tick`, juste après `player_tick` : si `!P.dead && !spectating && P.pos.y < maps[curMap].killY` :
  - solo : `damage_player(999)` ;
  - LAN : `net_hit(myId, 999, 0, W_VOID)`. Nouvelle arme **`W_VOID`** ajoutée **à la fin** de l'enum `W_*`
    (+ `weaponNames`, + toutes les tailles écrites en dur : `hitCount[...]`, `% N` dans `host_handle` et
    `draw_killfeed`, résumé de `WinMain`).
  - **Crédit du kill** (validé) : l'hôte mémorise pour chaque joueur le dernier attaquant et l'heure (`NP[v].lastHitBy`,
    `lastHitT`, mis à jour dans `host_handle` sur chaque `EV_HIT`). Une chute dans le vide moins de 4 s après une
    touche donne le kill à cet attaquant (fil des kills : « Rival → VIDE → Ninja »), sinon c'est un suicide.
    C'est ce qui rend le Fléau, les roquettes et la Lance intéressants près du bord.
- **Autres objets** : projectiles, pièces, mines et scies sous `killY` meurent (la Scie garde son retour après
  `SAW_LOST_T`) ; drones et kamikazes en solo ne descendent jamais sous `enemyMinY`.
- **Lisibilité** :
  - un plan rouge sombre animé (grille additive) quelques mètres sous `killY`, dessiné dans `render` hors liste
    d'affichage, et un brouillard qui rougit en dessous du sol ;
  - vignette rouge + son grave qui monte quand `P.pos.y < killY + 8` et `P.vel.y < 0` : on sent qu'on tombe,
    on a le temps d'un grappin ou d'un dash ;
  - pas de dégâts progressifs : la règle est simple, on passe sous le plan = mort.
- Les cartes fermées (Arène, Dojo) ont `killY = -1000` : aucun changement pour elles.

### Tests
- Pour chaque carte : nombre de boîtes < `MAXB`, cellules ≤ 16, chaque spawn `pos_free`, chaque spawn **au-dessus**
  de `killY`, une cellule n'est pas dans un solide.
- Kill zone : joueur placé sous `killY` → mort au tick suivant (solo) ; LAN simulé : touche de A sur B puis
  chute de B → kill crédité à A ; chute sans touche → suicide.
- Match bot sur chaque carte (`--map N` à ajouter aux options `--bot`), en surveillant le nombre de chutes :
  `bot_think` doit apprendre à **ne pas sauter dans le vide** (sonde `ray_world` vers le bas devant lui avant de
  sauter ou de dasher ; grappin d'urgence vers l'ancre la plus proche s'il tombe sous le niveau du sol).

---

## 4. Nouvelles cartes

> **État : v1 faite.** Le Puits (spirale de 8 plateformes reliées par des pads, plutôt que 6 étages espacés de
> 12 m : le saut ne monte que de 1,5 m), Le Canyon (piliers de 6 × 6 m, pads bord → îlot → îlot central),
> La Boucle (`B_SPEED`), Le Dojo (sans verre destructible). Toutes utilisent la musique « Arène ». Match bot
> sur Le Puits (90 s) : 0 chute, ~90 grappins de secours, 9 kills. Reste : verre destructible du Dojo,
> plafonner la portée de mêlée sur La Boucle si les mesures le demandent, musiques propres.

Conception déjà décrite dans `IDEES_ARMES_CARTES.md` §4 ; ordre recommandé :

| Ordre | Carte | Pourquoi maintenant | Nouveauté technique |
|---|---|---|---|
| 1 | **Le Puits** (verticalité, vide au fond, `killY` = −30) | met en valeur la kill zone et le saut plus haut | ancres de grappin (simples AABB colorées) |
| 2 | **Le Canyon** (deux falaises, gouffre de 70 m) | deuxième carte à vide, tir de loin et traversées au grappin | aucune |
| 3 | **La Boucle** (circuit de vitesse, voies à contresens) | carte à sol, jouable en solo | bande d'accélération `B_SPEED` |
| 4 | **Le Dojo** (salle à plafond bas, mêlée) | duels, jouable en solo | aucune (verre destructible plus tard) |

Pour chacune :
- une fonction `build_map_xxx()` + sa table de spawns + sa ligne dans `maps[]` ;
- couleurs propres (la palette donne l'identité : Puits bleu nuit, Canyon ocre, Boucle néon, Dojo bois) et une
  couleur de ciel / brouillard dans `MapDef` ;
- un morceau dans `music.h` (ou réutiliser « Arène » au début) ;
- `B_SPEED` (pour La Boucle) : `kind` de boîte, poussée dans `player_tick` quand `P.groundBox` en est une, rendu
  à flèches animées dans `render` ;
- mesures du match bot avant/après (temps pour tuer, chutes, portée de mêlée sur La Boucle — le doc d'idées
  prévoit qu'il faudra peut-être plafonner la portée de l'épée à ~6 m à cause des vitesses relatives de 50 m/s).

---

## 5. Modificateurs qui tombent du ciel

### Principe
Toutes les 30 à 45 s, une **capsule** apparaît à ~40 m au-dessus d'un point de la carte et **descend lentement**
(≈ 2,5 m/s, sous un petit parachute lumineux, ~15 s de descente). Une **colonne de lumière** marque son point de
chute depuis toute la carte : les joueurs ont le temps de se positionner, et la course pour l'attraper crée un
point chaud. On la prend au contact, **même en l'air** (récompense le mouvement : grappin, pads, rocket jump).
Posée au sol, elle disparaît après 20 s. Le grappin peut s'y accrocher pour la tirer vers soi.

### Liste de départ (effet personnel, 15 s, un seul actif à la fois — en prendre un autre remplace)
| Nom (écran) | Effet | Où ça se branche |
|---|---|---|
| `SURVOLTE` | dégâts ×1,4 (le plafond PvP de 60 reste) | multiplicateur appliqué par le tireur avant `net_hit` / `damage_enemy` |
| `PLUME` | gravité ×0,55 pour soi : sauts ~2× plus hauts, chute lente | `player_tick` (gravité) |
| `TURBO` | `RUN_SPEED` et accélération aérienne ×1,3, dash rechargé 3× plus vite | `player_tick`, recharge du dash |
| `VAMPIRE` | 25 % des dégâts infligés rendus en PV | **hôte** dans `host_handle` (il doit connaître le modificateur, voir réseau) ; solo direct |
| `ARSENAL` | toutes les jauges de munitions pleines + munitions max | `gauge_add` / munitions du loadout |
| `RICOCHET` | pièces illimitées, ricochets ×2 | `throw_coin`, `ricochet` |
| `BLINDE` | absorbe les 60 prochains dégâts | hôte (comme le bouclier) ; solo direct |

Effet visuel : aura colorée autour du joueur (vue des autres) + teinte du viseur et compteur dans le HUD ;
son de chute (sifflement doux) et son de prise. Option d'hébergement « MODIFICATEURS : OUI / NON ».

### Modificateurs de partie (validé : en plus des personnels)
Plus rares (une capsule **dorée**, plus grosse, toutes les 2 à 3 minutes, jamais deux à la fois) : celui qui
l'attrape déclenche un effet **pour tous les joueurs pendant 30 s**. Gros titre pour tout le monde
(« RIVAL DECLENCHE : GRAVITE BASSE »), compte à rebours en haut de l'écran, son d'annonce, et une teinte du ciel /
brouillard propre à l'effet pour qu'on sache toujours qu'il est actif.

| Nom (écran) | Effet pour tous | Où ça se branche |
|---|---|---|
| `GRAVITE BASSE` | gravité des joueurs ×0,5 (pièces, mines et roquettes inchangées) | `player_tick` |
| `HYPERVITESSE` | vitesse de course et accélération aérienne ×1,4, dash rechargé 2× plus vite | `player_tick`, dash |
| `MUNITIONS INFINIES` | aucune munition consommée | tir de chaque arme (`ammo_*`) |
| `CORPS A CORPS` | armes à distance bloquées, la mêlée rend 2× plus de PV | sélection / tir, soins |
| `PARRY ROYAL` | fenêtre de parry ×2, un parry soigne ×2 | `try_parry`, `PARRY_HEAL` côté hôte |
| `CHASSE` | le leader est visible à travers les murs ; le tuer rend 50 PV | `draw_name_tags`, `host_kill` |

Idée pour plus tard (cartes à vide) : `VIDE AFFAME`, le plan de mort monte de 6 m pendant l'effet.

Mise en œuvre :
- Une fonction par grandeur, lue par le code de jeu : `gmod_gravity()`, `gmod_speed()`, `gmod_ammo_free()`…
  (valeur neutre quand aucun effet n'est actif). Le modificateur personnel et le modificateur de partie se
  **cumulent** (ex. `PLUME` pendant `GRAVITE BASSE` : ×0,55 × 0,5), avec un plancher de gravité pour rester jouable.
- **L'hôte fait autorité** : l'effet actif et son temps restant voyagent dans `PkSnap` (2 octets en plus) — chaque
  joueur simule son propre mouvement, il applique donc l'effet dès le snapshot, et un joueur qui arrive en cours de
  partie le reçoit aussi. Un événement `EV_GMOD` (`sub` = effet, `actor` = déclencheur) sert à l'annonce et aux
  effets visuels. `NET_VER` +1.
- Option d'hébergement : « MODIFICATEURS : AUCUN / PERSONNELS / TOUS ».
- Solo vagues : les effets de partie s'appliquent au joueur seul (pas aux ennemis).
- Journal du bot : effets déclenchés, kills pendant chaque effet, temps pour tuer pendant / hors effet.
- Tests : effet actif → hauteur de saut ×2 sous `GRAVITE BASSE` ; expiration à 30 s ; cumul personnel + partie ;
  LAN simulé : le client applique l'effet reçu par snapshot ; `MUNITIONS INFINIES` ne consomme rien.

### Réseau (l'hôte fait autorité)
- `EV_MOD_SPAWN` (hôte) : `netId`, `sub` = type, `f[0..2]` point de départ, `f[3]` = `matchT` de départ.
  La chute est **déterministe** (position = fonction du temps écoulé, collision avec `ray_world` vers le bas
  calculée une fois au spawn) : aucun état à diffuser pendant la descente.
- `EV_MOD_TAKE` (client qui la touche) → l'hôte arbitre « premier arrivé » (modèle : `EV_PICKUP` /
  `host_take_pickup`) → `EV_MOD_TAKEN` (`target` = joueur, `sub` = type) à tous.
- Le modificateur actif de chaque joueur voyage dans `NetPState` (un octet `mod` + temps restant en dixièmes) :
  les autres dessinent l'aura, l'hôte applique `VAMPIRE` / `BLINDE`.
- Les arrivants en cours de partie reçoivent les capsules en vol par réémission périodique (comme `SAW_RESYNC`).
- `NET_VER` +1. Statistiques dans le journal du bot : capsules prises par type, kills sous modificateur.

### Solo
Même système dans le mode vagues (une capsule par vague) ; c'est aussi la base des « objets » du roguelike.

### Tests
Chute : position après N s, atterrissage sur une plateforme ; prise au contact ; effet appliqué puis expiré
(ex. hauteur de saut sous `PLUME`) ; LAN simulé : deux joueurs touchent la même capsule au même tick → un seul
gagnant ; `VAMPIRE` soigne côté hôte.

---

## 6. Mode roguelike (solo) — plus tard, non prioritaire

> Décision : le roguelike n'est pas prioritaire. Il reste dans le plan comme prochaine étape une fois les phases
> 1 à 5 terminées ; rien de ce qui suit ne bloque les autres phases. La section garde la conception pour ne pas
> la perdre.

### Boucle de jeu
- Une **course** = une suite d'**étages**. Chaque étage est une arène à battre ; entre deux étages, on choisit
  **1 amélioration parmi 3** (permanente pour la course). Mourir = fin de la course, retour à l'étage 1.
- Objectifs d'étage (tirés au sort) : nettoyer 3 vagues, survivre 60 s, abattre une **élite** ; tous les 5 étages,
  un **boss**.
- Les **arènes** : en v1, rotation des cartes `solo` + des cartes à vide (Puits, Canyon) avec les ennemis adaptés.
  En v2, **arènes générées** par graine (plateformes flottantes, piliers, pads, ancres au-dessus du vide) : la
  génération réutilise `addbox`/`addpad` et le `rng` déterministe ; la graine de la course est affichée pour
  rejouer la même.
- Difficulté : PV des ennemis ×(1 + 0,12 × étage), cadence de tir des drones, proportion de kamikazes, puis de
  nouveaux types (élite = drone blindé 300 PV qui ne prend que les tirs de face ; boss = gros drone qui tire en
  salves et invoque).

### Améliorations (exemples, toutes à partir de réglages existants)
| Catégorie | Améliorations |
|---|---|
| Mouvement | +1 double saut, +1 charge de dash, dash rechargé 25 % plus vite, saut +15 %, grappin rechargé 2× plus vite |
| Tir | dégâts +15 %, chaîne de tir qui ne se perd qu'après 2 ratés, +1 pièce, ricochet ×1,7 au lieu de ×1,5 |
| Mêlée | portée +20 %, chaque kill à la mêlée rend 10 PV, fenêtre de parry +50 % |
| Survie | +25 PV max, régénération hors combat, « dernier souffle » (une fois par étage, survivre à 1 PV) |
| Synergies | les ennemis tués explosent, les modificateurs durent 2× plus longtemps, la vitesse au-delà de 20 m/s donne +20 % de dégâts |

Rareté (commune / rare / légendaire) ; pas deux fois la même proposition dans un tirage.

### Code
- État de course **hors de `P`** (que `reset_game` efface) : `typedef struct { int active, floor, seed, picks[32],
  npicks; float dmgMult, hpMax, dashRate…; } Run; static Run run;` Les fonctions de jeu lisent des accesseurs
  (`run_dmg_mult()`, `run_hp_max()`, …) qui valent 1 / 100 / … hors roguelike : le mode vagues et le LAN ne
  changent pas.
- `ACT_ROGUE` (sous-écran JOUER), `appState` reste `APP_GAME` ; un écran `draw_rogue_pick` (3 cartes cliquables
  + touches 1/2/3) entre les étages, construit avec la mise en page de la phase 2.
- `rogue_tick` (appelé par `game_tick` à la place de `wave_tick` quand `run.active`) : objectif, fin d'étage,
  passage à l'écran de choix, chargement de l'étage suivant (`load_map` ou génération).
- HUD : étage, objectif, liste compacte des améliorations ; écran de fin de course (étage atteint, kills, temps,
  record). Record `rogue_best=` dans `velocity.ini` (jamais écrit en `--bot` / `--shot` / `--test`).
- Musique : `music_level` suit l'étage (niveau = étage / 2, plafonné à 8).

### Tests
Une amélioration modifie bien la valeur (ex. hauteur de saut, PV max) ; un tirage de 3 ne contient pas de
doublon ; la même graine produit le même étage (nombre de boîtes + somme de contrôle des coordonnées) ; les spawns
d'un étage généré sont `pos_free` et au-dessus de `killY` ; captures `--shot rogue` (en jeu) et `--shot roguepick`
(écran de choix).

---

## 7. Décisions

Prises :
- **Enclume** : refonte de l'onde (taille, hauteur et portée selon la hauteur de chute et la vitesse) → phase 1b.
- **Menus** : tout proportionnel à la fenêtre, aucune valeur fixe → phase 2.
- **Kill zone** : le kill revient au dernier attaquant s'il a touché moins de 4 s avant la chute → phase 3.2.
- **Modificateurs** : personnels **et** de partie → phase 5.
- **Roguelike** : non prioritaire, repoussé après la phase 5 (le choix v1 / v2 se fera à ce moment-là).
- **Hauteur du saut** : `JUMP_V` = 9,6 m/s (1,92 m) → phase 1.
- **Mise à jour** : bouton qui récupère la dernière version depuis GitHub, **primordial** → phase 0b, en premier.

Plus aucune décision ouverte : le plan est prêt à être appliqué phase par phase.
