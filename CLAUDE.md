# VELOCITY — guide pour Claude

Movement shooter 3D en **C pur**, une seule unité de compilation (`main.c` inclut les morceaux de `src/` dans l'ordre),
sans aucune dépendance :
Win32 + OpenGL 1.1 (pipeline fixe) + waveOut (son synthétisé) + Winsock UDP (LAN).
Le propriétaire est francophone : réponses, commentaires et docs **en français**.

- Architecture, systèmes, réseau, réglages, recettes : **`docs/CODE.md`** ; **arborescence des fichiers et rôle de
  chacun : §2** (une arme = `src/armes/<arme>.c`, skins = `src/perso/`, réglages = `src/reglages/equilibrage.c`…).
  Nouvelle fonction : dans le fichier de son thème ; nouveau fichier : l'ajouter à `main.c` au bon endroit (l'ordre compte).
- Port macOS (`src/plateforme/mac/`, `build_mac.sh`, CI GitHub Actions) : **§8 de `docs/CODE.md`**. Toute nouvelle
  API Win32 dans le code doit avoir son équivalent macOS (`#ifdef _WIN32` / `mac_*`).
- Nouveautés de `dev`, une section par fonctionnalité (à compléter à chaque ajout) : **`docs/NOUVEAUTES.md`**
- Idées d'armes et de cartes à implémenter : **`docs/IDEES_ARMES_CARTES.md`**
- Parties en ligne (lobby Cloudflare gratuit, STUN, perçage de NAT, spectateurs) : **`docs/PLAN_EN_LIGNE.md`**
  et §4 de `docs/CODE.md`. Le lobby est dans **`worker/`** (`node test.js` pour ses tests, `node dev-server.js`
  pour un lobby local — Node 22.5+ pour `node:sqlite` ; le déploiement `npx wrangler deploy` est à faire par le
  propriétaire).
- **Mode classé** (PARTIE CLASSEE, cote Elo, encadré CLASSEMENT / TOP 3) : **§4 « Mode classé » de `docs/CODE.md`**.
  Le serveur fait foi : la cote est calculée et stockée par le worker (Durable Object `Ratings`,
  `worker/src/ratings.js`) ; côté jeu, `src/reseau/classe.c` (Elo, paliers, identité), `src/reseau/classe_match.c` (rapports de fin de match),
  `src/reseau/classe_serveur.c` (requêtes au serveur) et `src/plateforme/http.c` (HTTP : WinHTTP / curl). Une partie classée est toujours en chacun pour soi, aux
  règles par défaut, sans bots, hors cartes CHALLENGE (LA TOILE).
- **Mode VAGUES** (le solo : 100 vagues, boss aux vagues 25/50/75/100, sélecteur de carte toutes les 10, boutique de
  comportements, INFINI) : **§3.5 de `docs/CODE.md`**, code dans `src/solo/`. Aucune inflation : les dégâts des ennemis
  ne montent jamais, aucun achat n'augmente une stat. Temps de la vague 100 pour le speedrun : `vagues_temps_100()`.
- **Speedrun** (écran CHALLENGE, classements de temps par carte CHALLENGE, speedrun complet, vague 100) : **§4 « Speedrun »
  de `docs/CODE.md`**. Serveur : `worker/src/speedrun.js` (même DO `Ratings`, même identité) ; jeu : `src/jeu/speedrun.c`,
  `src/reseau/speedrun_serveur.c` ; point d'entrée `speedrun_begin(cat)` / `speedrun_report(cat, ms)`. Les planchers de
  temps existent en double (`sr_floor_ms` et `SR.CATS`) : nouvelle carte CHALLENGE = nouvelle catégorie dans le worker.
- Mode équipes 1v1 à 4v4 (implémenté) : **§3.11 de `docs/CODE.md`** ; éclairage, ombres, bloom : **§3.12**
- Cartes (`MapDef`, 5 cartes néon de ARENE à LE DOJO + LA TOILE en CHALLENGE, kill zone) : **§3.18** ; compétences (PIECE, MARQUE, CHARGE…) :
  `src/combat/competences.c` et `docs/COMPETENCES_MARKSMAN.md`
- Navigation des bots (graphe auto) : **§3.19** ; jauge de style : **§3.20** ; mode COURSE (bouton CHALLENGE, LA TOILE,
  fantôme) : **§3.21** ; une carte `challenge = 1` n'est jamais en partie (`map_pvp`, `mode_ok`, rotation, classé, menus)
- Objectifs des modes sur les cartes néon (zones, drapeaux, bouclier, départs par équipe, `objScale`) : **§3.22**
  (LA CITADELLE et L'OASIS, décor maillé, textures pierre / enduit ont été retirés : esthétique néon seulement)
- Règles de partie, modes (CHRONO, 3 ZONES, DRAPEAU, BOUCLIER), boutique, spécialisations : **§3.23** ;
  l'hôte arbitre les objectifs (`mode_tick`), les clients reçoivent `rules` et `obj` dans `PkSnap`
- Réglages graphiques modifiables sans recompiler : **`graphismes/rendu.ini`** (+ textures `.bmp`, F5 en jeu) ;
  code : §3.13 de `docs/CODE.md`. Toute nouvelle valeur d'aspect doit y être exposée (`gfxKeys`).
  `velocity.exe --export-textures <dossier>` écrit les textures générées par le code en `.bmp` (`sol`, `mur`,
  `grain`) : c'est ce que contient `graphismes/`.

## Branches

- **`stable`** : version distribuable, toujours compilable, tests verts. On n'y merge que depuis `dev`,
  apres verification complete (build sans warning, `--test`, match bot).
  **Merger dans `stable` = publier chez tous les joueurs** (voir « Mise à jour » plus bas) : seulement sur accord
  explicite du propriétaire.
- **`dev`** : nouvelles fonctionnalites et experiences. Travailler ici par defaut.
- **`release`** : construite par la CI depuis `stable` (`velocity.exe`, `Velocity-Windows.zip` = exe + `graphismes/`,
  `Velocity-macOS.zip`, `version.txt`) ; ne jamais y pousser à la main.
- `master` : ancien nom, conserve a l'identique du point de depart de `stable`.

## Compiler

```
cmd /c ".\build.bat"          # PowerShell
cmd //c ".\\build.bat"        # bash
```
`build.bat` trouve MSVC via vswhere. **Zéro warning** : corriger tout avertissement.
Sur Mac : `./build_mac.sh`, puis `Velocity.app/Contents/MacOS/velocity --test test.txt` (même options que l'exe).

## Vérifier (toujours, après chaque changement)

`velocity.exe` est une appli fenêtrée : la lancer avec `Start-Process ... -PassThru` puis
`WaitForExit`, sinon la commande rend la main tout de suite.

1. **Tests auto** : `velocity.exe --test test.txt` → chaque ligne affiche la valeur obtenue et
   « (attendu …) ». Toute nouvelle mécanique reçoit sa ligne de test dans `run_tests()` (`src/tests/tests.c`).
2. **Captures** : `velocity.exe --shot X.bmp`. Le nom choisit la scène (sous-chaîne, liste complète dans
   `src/plateforme/demarrage.c`, chercher `strstr(shotPath`, et §6 de `docs/CODE.md`) :
   - menus : `menu` (avec l'encadré TOP 3 factice), `settings`, `graphismes` (PARAMETRES > GRAPHISMES), `loadout`,
     `heberger`, `update`, `skinmenu`, `challenge` (classement speedrun) ; mise en page en 1024 × 640 : `settings_small`, `graphismes_small` ;
   - LAN : `dm`, `dm_equipes` (`_fin`, `_bas`), `dm_mort`, `dm_<arme>` (`enclume`, `lance`, `fleau`, `poings`, `egide`,
     `eventail`, `tesla`), `skills`, `ranked` (fin de match classée), `boutique`, `armedepart` ;
   - VAGUES : `vagues_pyramide` (`_charge`), `vagues_tesseract`, `vagues_boule`, `vagues_hexaedre`, `vagues_boutique`,
     `vagues_selecteur`, `vagues_fin`, `vagues_infini` ;
   - solo (scène par défaut) : `rail`, `impact`, `fx`, `changearme`, `vie`, `style`, et une arme (`elan`, `scie`…) ;
   - carte : `arene` (`arene_nuit`…), `course` (`_depart`, `_fin`, `_aimant`), `speedrun` (`_fin`), `carte` / `carte3d` (second écran),
     `skins` ; autres cartes avec `--map N` (`spawn`, `chute`, `solo`, `--pos x,y,z`, `--yaw A`) ; objectifs d'un mode
     sur une carte : `objectifs_zones`, `objectifs_drapeau`, `objectifs_bouclier` (avec `--map N`, `--pos`, `--yaw`) ;
   - à combiner : `sansbloom`, `sansombre`, `small` (fenêtre 1024 × 640) ; `bench_dm` mesure le temps de rendu.
   Convertir en PNG avec System.Drawing pour la lire.
3. **Match bot contre bot** (réseau réel sur la machine) :
   ```
   velocity.exe --bot host --name Hote --time 90 --log host.txt
   velocity.exe --bot join --name Client --time 86 --log client.txt   (2 s plus tard)
   ```
   Le journal de l'hôte finit par `equilibre : temps pour tuer…, one-shots…, parries…` et
   `touches arbitrees : …` par arme. Pour un rééquilibrage : mesurer **avant / après**.
   Mode classé : lobby local (`cd worker && node dev-server.js 8787`) et `--ranked --lobby 127.0.0.1:8787 --lanep
   --nolan --frags 2` sur les deux bots (§6 de `docs/CODE.md`, « Match classé »).
4. Supprimer ensuite les fichiers temporaires (`test.txt`, `*.bmp`, `*.png`, logs, `*.obj`).

## Règles du code

- **Réglages** : les `#define` d'équilibrage dans `src/reglages/equilibrage.c` (mouvement, armes, parry, PvP, pièces…) ;
  ceux du match et du réseau dans `src/reseau/etat_partage.c` / `protocole.c`.
  Valeurs PvP actuelles : `PVP_SCALE` 0,7 (`etat_partage.c`), plafond `PVP_HIT_CAP` 60 PV par touche
  (`equilibrage.c`), `KILL_HEAL` 30 PV, `FRAG_LIMIT` 15 (7 en partie rapide), `MATCH_TIME` 600 s.
- **Entrées** : la logique de jeu ne lit **jamais** `keys[]` : elle lit les actions `act[A_*]`
  (tenu) et `actP[A_*]` (appui du tick), remplies par `input_poll()` via `binds`. Les bots écrivent
  dans `botHeld` / `botPress`. Nouvelle action = `A_*` + `actId` + `actLabel` + `bindDefault`.
- **Réseau** : l'hôte (id 0) fait autorité (PV, kills, respawns, parry d'épée, recharges).
  Chaque joueur simule son mouvement ; les touches sont validées chez le tireur puis arbitrées
  dans `host_handle()`. Nouvel événement = `EV_*` + `net_fx_*()` émetteur + cas dans
  `apply_event()` (+ `host_handle()` s'il faut arbitrer). **Incrémenter `NET_VER`** dès qu'un
  paquet ou un événement change (actuellement **18**, `src/reseau/protocole.c`).
  Chrono du match : temps réel, calé sur l'heure du worker (`time`), voir « Horloge de match commune » au §4 de
  `docs/CODE.md` (`src/reseau/horloge.c`).
- **Équité en réseau** : pas d'aide à la visée passive. L'aimant de visée (`aim_assist`) n'existe que comme
  compétence AIMANT (`SK_AIM`, `AIM_T` / `AIM_RECHARGE`), la même pour tous.
- **Texte à l'écran** : la police ne couvre que CP1252 et le source est en UTF-8 → **pas
  d'accents dans les chaînes C** (écrire `PARAMETRES`, ou échapper `\xE9`). Commentaires du code
  en français sans accents, comme l'existant.
- Fichiers utilisateur : `velocity.ini` (réglages et touches du joueur, identité du mode classé
  `ranked_id` / `ranked_key`) — ne jamais l'écraser ; les modes `--bot` / `--shot` / `--test` ne sauvegardent pas.
  Le second écran est désactivé par défaut (`second_ecran=0`).
- **Mise à jour** (bouton du menu, `src/interface/mise_a_jour.c`, §3.17 de `docs/CODE.md`) : les joueurs reçoivent la
  branche `release` du dépôt **`void19845/velocity`** (`UPD_REPO`), construite par la CI depuis `stable` avec
  `version.txt` et le numéro `VELOCITY_BUILD`. Merger dans `stable` = publier chez tous les joueurs : seulement sur
  accord explicite du propriétaire.

## Pièges déjà rencontrés

- Patchs Python via heredoc bash : un `\n` dans une chaîne C devient un vrai saut de ligne et
  casse la compilation, et un `\\` devient `\`. Écrire le script dans un fichier (chaînes `r"""…"""`) ou utiliser Edit.
- `velocity.exe` verrouillé (le jeu tourne chez le propriétaire) : compiler sous un autre nom pour les tests
  (`/Fe:vtest.exe /Fo:vtest.obj`) plutôt que fermer ses fenêtres.
- Windows Defender met parfois en quarantaine les exe compilés (`Trojan:Win32/Bearfoos.A!ml`, faux positif) :
  recompiler et relancer ; ne jamais toucher aux réglages de sécurité.
- `bind` est une fonction Winsock : le tableau des touches s'appelle `binds`.
- Ordre des définitions : `pname()` et la section réseau sont **après** le combat — avant,
  utiliser `NP[i].name` ; les fonctions `net_fx_*` / `net_hit` y sont déclarées en avance (`src/reseau/etat_partage.c`).
- `game_tick()` sort tôt pendant le `hitstop` (en solo seulement : jamais de pause en LAN / en ligne) : les
  minuteurs « temps réel » (impact frames) se décomptent dans `render()`. La boucle principale est `main_frame()`,
  aussi appelée pendant les boucles modales (fenêtre déplacée) : rien ne doit y supposer un appel unique.
- Les bots ont un temps de réaction et une imprécision voulus ; s'ils sont « trop forts »,
  le match de comparaison n'est plus représentatif d'un humain.
- Les `.bmp` de `graphismes/` remplacent les textures générées : après une modification de `make_textures`,
  relancer `--export-textures graphismes`, sinon les anciens `.bmp` masquent le changement.
- Mode classé : la formule Elo existe en double (`rank_delta` dans `src/reseau/classe.c`, `worker/src/ratings.js`) ;
  toute modification se fait des deux côtés, tests `--test` et `node test.js` à l'appui.
