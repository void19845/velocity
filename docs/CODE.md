# VELOCITY — documentation du code

Ce document décrit `main.c` pour qu'un humain ou une IA puisse modifier le jeu sans tout relire.
Le code est cité par **noms** de fonctions, de structures, de variables et de `#define` : cherchez-les
dans `main.c` (les numéros de ligne changent trop souvent pour être utiles).

> Les commentaires de `main.c` sont en français **sans accents**. Gardez cette convention dans le code.

---

## 1. Vue d'ensemble

### Un seul fichier, aucune dépendance

- Tout le jeu tient dans **`main.c`** (~4 900 lignes) : fenêtre Win32, rendu OpenGL 1.1 (pipeline fixe,
  `glBegin`/`glEnd`, listes d'affichage), audio `waveOut` avec sons **synthétisés** au démarrage,
  réseau UDP Winsock pour le deathmatch LAN.
- Aucune ressource externe : pas d'image, pas de son, pas de police à charger (la police vient de
  `wglUseFontBitmapsA` sur « Consolas », les textures sont générées dans `make_textures`).
- Les bibliothèques système sont liées par des `#pragma comment(lib, ...)` en tête de fichier
  (`opengl32`, `gdi32`, `user32`, `winmm`, `ws2_32`, `advapi32`).
- **Compilation** : `build.bat` trouve MSVC via `vswhere`, puis lance
  `cl /nologo /O2 /W3 /std:c11 /Fe:velocity.exe main.c /link /SUBSYSTEM:WINDOWS`.
  Il produit `velocity.exe` (et `main.obj`) à côté de `main.c`.
- Unités : mètres, secondes, m/s. L'axe Y est vertical ; `P.yaw = 0` regarde vers −Z
  (voir `view_dir`).

### Boucle principale (`WinMain`)

`WinMain` fait, dans l'ordre :

1. Copie `bindDefault` dans `binds`, puis lit la ligne de commande : `--test` (lance `run_tests` et
   quitte, **sans fenêtre**), `--shot`, et les options du mode bot (`--bot`, `--name`, `--time`,
   `--log`, `--botshot`). Le pseudo par défaut est le nom d'utilisateur Windows (`GetUserNameA`).
2. Crée la fenêtre et le contexte OpenGL, active la synchro verticale (`wglSwapIntervalEXT`),
   s'abonne à la souris brute (`RegisterRawInputDevices`).
3. Initialise : `make_font` (3 tailles : 0 = 18 px, 1 = 28 px, 2 = 56 px), `make_textures`,
   `build_level`, `build_world_list`, `make_sounds`, `cfg_load` (fichier `velocity.ini`),
   `audio_init`, `reset_game`.
4. En mode `--shot`, prépare une scène, appelle `render` une fois, `save_bmp`, puis quitte.
5. Sinon, boucle infinie :
   - pompe les messages Windows (`wndproc`) ;
   - calcule `dt` (borné à 0,1 s) et l'horloge absolue `nowT` ;
   - `net_update(dt)` à chaque image (réseau) ; `bot_think(dt)` si `botMode` ;
   - applique la souris (`mdx`, `mdy` × `sens`) à `P.yaw`/`P.pitch` **à chaque image**, pas à chaque tick ;
   - **tick fixe** : accumulateur `acc`, on appelle `game_tick(TICK)` tant que `acc >= TICK` ;
   - `render(acc / TICK, dt)` puis `SwapBuffers` ;
   - `audio_update` (remplit les tampons audio libérés) ;
   - `do_action` si un bouton de menu a posé une `pendingAction`.

### Tick fixe 120 Hz et rendu interpolé

- `TICK` vaut `1.0f / 120.0f`. **Toute la simulation** (mouvement, armes, ennemis, projectiles,
  particules, minuteries) avance dans `game_tick(dt)` avec `dt = TICK`.
- Le jeu ne tourne que si la souris est capturée (`captured`) **ou** en LAN (`dmMode`) : en solo,
  ouvrir le menu met en pause ; en LAN, la partie continue.
- `render(alpha, frameDt)` interpole **la position du joueur** entre `P.prevPos` et `P.pos` avec
  `alpha = acc / TICK`. Le reste (ennemis, particules) est dessiné à sa position du dernier tick.
- **Hitstop** : si `hitstop > 0`, `game_tick` décrémente `hitstop` et sort immédiatement — le monde
  se fige quelques centièmes de seconde sur les gros coups (les entrées attendent).

### États de l'application

| Variable | Valeurs | Rôle |
|---|---|---|
| `appState` | `APP_MENU`, `APP_JOIN`, `APP_GAME` | menu principal / recherche de parties LAN / en jeu |
| `settingsOpen` | 0 / 1 | écran PARAMÈTRES par-dessus le menu principal ou la pause |
| `captured` | 0 / 1 | souris capturée (jeu) ; sinon le menu de pause s'affiche (`draw_pause`) |
| `netMode` | `NET_OFF`, `NET_HOST`, `NET_CLIENT` | rôle réseau |
| `dmMode` | 0 / 1 | 1 = deathmatch LAN (pas d'ennemis ni de vagues, PV tenus par l'hôte) |
| `botMode` | 0, 1, 2 | 0 = humain, 1 = bot hôte, 2 = bot client |
| `shotMode` | 0 / 1 | capture d'écran automatique (`--shot`) |

Les menus sont en **mode immédiat** : `draw_main_menu`, `draw_join_menu`, `draw_pause`,
`draw_settings` dessinent et testent les clics dans la même fonction (`button`, `field`, `slider`).
Un clic ne déclenche pas l'action tout de suite : il pose `pendingAction` (`ACT_SOLO`, `ACT_HOST`,
`ACT_BROWSE`, `ACT_JOIN_FOUND`, `ACT_JOIN_IP`, `ACT_BACK`, `ACT_RESUME`, `ACT_RESTART`, `ACT_LEAVE`,
`ACT_QUIT`, `ACT_FULLSCREEN`), traitée par `do_action` après l'image.

---

## 2. Carte des sections de `main.c`

Les sections sont délimitées par des bandeaux `/* ---- */`. Dans l'ordre :

| # | Section (titre du bandeau) | Rôle | Éléments clés |
|---|---|---|---|
| 0 | *(en-tête)* | includes, `#pragma comment(lib)` | `PI`, `TICK` |
| 1 | **Maths** | vecteurs 3D et aléatoire | `V3`, `v3`, `vadd`, `vsub`, `vmul`, `vdot`, `vcross`, `vlen`, `vnorm`, `vlerp`, `vc`, `clampf`, `hlen` (vitesse horizontale), `rng`, `frand`, `frange`, `rand_dir` |
| 2 | **Reglages du mouvement** | presque tous les `#define` d'équilibrage | mouvement, grappin, rail, munitions, pièces, roquettes, épée, parry, chaîne de tir (voir §5) |
| 3 | **Audio** | mixeur `waveOut` + synthèse | `S_*`, `Sound`, `Voice`, `sounds`, `voices`, `masterVol`, `play`, `audio_fill`, `audio_update`, `noise`, `sbuf`, `mksound`, `make_sounds`, `audio_init` |
| 4 | **Monde : une liste d'AABB** | carte, collisions, rayons | `Box`, `B_SOLID`, `B_PAD`, `B_SPEED`, `boxes`, `nb`, `addbox`, `addpad`, `MAP_S`, `ms`, `Pickup`, `PU`, `npu`, `addpickup`, `build_map_arene` et les autres `build_map_*`, `addpad_to`, `addspeed`, `addanchor`, `addcell`, `MapDef`, `maps[]`, `curMap`, `build_level(m)`, `box_overlap`, `ray_box`, `ray_world`, `ray_sphere`, `los`, `sphere_pushout` |
| 5 | **Entites** | structures de jeu et état global solo | `Player` (`P`), `HOOK_NONE/FLYING/ATTACHED`, `Enemy` (`E`, `EN_DRONE`, `EN_RUSHER`), `Coin` (`CO`), `Proj` (`PR`, `PR_ROCKET`, `PR_ORB`, `PR_PARRIED`), `Part` (`PA`), `Beam` (`BM`), score/vague/combo, `hitstop`, `parryFlash`, `shotStreak`, `msg`, `popText` |
| 6 | **Multijoueur LAN : etat partage** | ce que le jeu doit connaître du réseau avant la section réseau | `MAXP`, `NET_PORT`, `FRAG_LIMIT`…, `NET_*`, `APP_*`, `W_*`, `NF_*`, `NetPState`, `NetPlayer` (`NP`), `myId`, `np_height`, `np_center`, `np_vel`, `np_aabb`, `np_target`, `rel_speed`, déclarations anticipées `net_hit`, `net_fx_*`, `netlog` |
| 7 | **Entrees** | touches physiques et couche d'actions | `SC_*`, `K_LMB`…`K_WHEEL`, `K_COUNT`, `keys`, `latch`, `A_*`, `actId`, `actLabel`, `bindDefault`, `binds`, `act`, `actP`, `botHeld`, `botPress`, réglages `fovBase`, `invertY`, `showFps`, `impactFrames`, `screenFx` |
| 8 | **Effets visuels de combat** | impact frames, chiffres de dégâts, éclats ; et aussi la lecture des entrées | `impact_frame`, `impactT`, `killMarkT`, `muzzleT`, `hurtDirT`, `DmgNum` (`DN`, `dmg_number`), `Shard` (`SH`, `shards`, `shard_tick`), `bound_held`, `input_poll`, `hookPressIn`, `hookHeldIn`, `sens`, `captured`, `shotMode`, `hwnd`, `winW`, `winH`, `set_capture` |
| 9 | **Effets** | particules et rayons | `add_part`, `burst`, `add_beam` |
| 10 | **Joueur** | physique du joueur et grappin | `pheight`, `paabb`, `pos_free`, `view_dir`, `eye_pos`, `pcenter`, `move_axis`, `wall_check`, `accelerate`, `hook_release`, `grapple_bounce`, `grapple_tick`, `player_tick` |
| 11 | **Combat** | dégâts, épée, parry, rail, pièces, roquettes | `damage_player`, `kill_enemy`, `damage_enemy`, `explode`, `explode_fx`, `atten`, `streak_mult`, `streak_rate`, `shot_result`, `parry_feedback`, `melee_range`, `melee_dmg`, `try_parry`, `melee`, `muzzle`, `throw_coin`, `coin_tick`, `ricochet`, `fire_rail`, `spawn_proj`, `fire_rocket`, `proj_tick` |
| 12 | **Ennemis et vagues** | mode solo | `spawn_enemy`, `enemy_tick`, `enemies_alive`, `wave_tick`, `reset_game` |
| 13 | **Multijoueur LAN : reseau** | protocole, hôte, client, bot, **et `game_tick`** | `NET_MAGIC`, `NET_VER`, `PK_*`, `EV_*`, structures de paquets ; utilitaires `netlog`, `set_status`, `pname`, `net_open`, `net_send`, `head` (remplit un `PkHead`), `same_addr`, `host_send_all`, `pack_state`, `find_local_ip` ; sous-parties « evenements » (`ev_seen`, `net_queue`, `net_emit`, `ev_new`, `net_hit`, `net_fx_*`, `kfeed_add`, `find_proj`, `pickup_taken`, `apply_event`), « hote : arbitrage » (`host_*`, `pick_spawn`), « client » (`net_shutdown`, `on_welcome`, `apply_snap`, `client_recv`, `net_poll`, `flush_events`, `net_update`, `net_host_start`, `net_browse_start`, `net_connect`), « bot de test » (`bot_think`), puis `pickup_tick` et `game_tick` |
| 14 | **Rendu** | 3D | `make_textures`, `quadf`, `box_faces`, `box_edges`, `build_world_list`, `make_font`, `text`, `text_c`, `rect`, `billboard`, `set_frustum`, `draw_sky`, `draw_enemy`, `draw_viewmodel` |
| 15 | **Interface : menus, tableau des scores, joueurs distants** | 2D, réglages, menus, HUD, et `render` | `ACT_*`, `settingsOpen`, `key_name`, `bind_text`, `cfg_save`, `cfg_load`, `assign_bind`, `button`, `field`, `slider`, `draw_controls`, `draw_settings`, `draw_status`, `draw_main_menu`, `draw_join_menu`, `draw_pause`, `draw_scoreboard`, `draw_killfeed`, `project`, `draw_name_tags`, `draw_remote_players`, `melee_preview`, `draw_hud`, `render` |
| 16 | **Fenetre** | Win32, outils, point d'entrée | `toggle_fullscreen`, `wndproc`, `save_bmp`, `run_tests`, `do_action`, `WinMain` |

À savoir pour s'y retrouver :

- **`game_tick` est dans la section réseau** (tout à la fin, après `bot_think` et `pickup_tick`),
  parce qu'il a besoin de `host_tick`.
- **`input_poll` est dans la section « Effets visuels de combat »**, juste après `shard_tick`.
- **`render` est à la fin de la section Interface**, car il appelle `draw_hud`.
- Le C exige qu'une fonction soit déclarée avant usage : les fonctions réseau appelées par le combat
  (`net_hit`, `net_fx_rail`, `net_fx_rocket`, `net_fx_rocket_end`, `net_fx_parry`, `net_fx_melee`,
  `net_fx_coin`, `netlog`) sont **déclarées en avance** à la fin de la section « etat partage ».
  Même chose pour `enemy_radius` et `damage_enemy` avant `grapple_tick`, et `apply_event`/`host_handle`
  avant `net_emit`.

---

## 3. Les systèmes, un par un

### 3.1 Le joueur (`Player P`)

`Player` regroupe tout l'état local : position (`pos`, `prevPos` pour l'interpolation), vitesse `vel`,
vue (`yaw`, `pitch`), états de mouvement (`grounded`, `crouch`, `sliding`, `airJumps`, `wallrun`,
`wallTouch`, `wallN`, `wallTime`, `coyote`, `dashCharge`), vie (`hp`, `dead`, `deathT`), armes
(`weapon` 0 = rail / 1 = roquettes, `fireCd`, `switchT`, `railAmmo`, `coins`), épée (`meleeCd`,
`meleeT`, `parryWin`, `swingLanded`), grappin (`hook`, `hookEnemy`, `hookPlayer`, `hookPos`,
`hookDir`, `anchor`, `ropeLen`, `hookCd`, `hookDist`, `hookBlockT`) et caméra (`eyeH`, `landDip`,
`stepOfs`, `roll`, `fovKick`, `fovCur`, `bob`, `bobT`, `swayX`, `swayY`, `recoil`).

`reset_game` remet tout à zéro (solo). En LAN, la réapparition passe par l'événement `EV_RESPAWN`.

### 3.2 Mouvement (`player_tick`)

`player_tick(dt, jumpPress, dashPress)` lit `act[A_FWD]`, `act[A_BACK]`, `act[A_LEFT]`,
`act[A_RIGHT]`, `act[A_CROUCH]`, `act[A_JUMP]` et fait, dans l'ordre :

1. **Direction voulue** `wish` à partir de `yaw` et des touches.
2. Minuteries : `slideCd`, `wallJumpCd`, `coyote` (temps passé en l'air), recharge du **dash**
   (2 charges, une toutes les `DASH_RECHARGE` s).
3. **Accroupi / slide** : on reste accroupi tant qu'on ne peut pas se relever (`pos_free` avec
   `H_STAND`). Le slide démarre au sol, accroupi, au-dessus de `SLIDE_MIN` ; il donne un élan
   `SLIDE_BOOST` si `slideCd <= 0` (puis `SLIDE_CD`) et s'arrête sous `SLIDE_END`.
4. **Saut pendant le grappin** : `hook_release(1)` (lâcher avec un boost `GRAPPLE_JUMP`, sans
   consommer le double saut).
5. **Saut** :
   - au sol, ou moins de 0,1 s après avoir quitté le sol (coyote time) : `JUMP_V`. **Bhop** :
     maintenir `A_JUMP` resaute automatiquement à chaque contact avec le sol ;
   - contre un mur (`wallTouch`) : wall-jump, poussée `WALLJUMP_PUSH` selon la normale + `WALLJUMP_UP` ;
   - sinon, double saut si `airJumps > 0` (redirige 60 % de la vitesse vers `wish`).
6. **Dash** : vitesse horizontale `max(DASH_SPEED, vitesse + 3)` dans la direction voulue.
7. `grapple_tick` (voir 3.3).
8. **Friction** façon Quake (`FRICTION`, `STOPSPEED`, `SLIDE_FRICTION` en slide), aucune pendant le
   grappin.
9. **Accélération** via `accelerate(wish, wishspeed, accel, cap, dt)` — c'est l'accélération de Quake :
   on n'ajoute de la vitesse que dans la composante de `wish` qui est sous `cap`. En l'air,
   `AIR_ACCEL` avec un plafond `AIR_CAP` très bas : c'est ce qui rend l'**air-strafe** possible
   (tourner la souris + strafe fait gagner de la vitesse). Au sol `GROUND_ACCEL`, en slide une
   accélération faible, en wall-run une accélération projetée sur le mur.
10. **Gravité** `GRAVITY` (×0,12 en wall-run, ×0,5 au grappin ; chute plafonnée à 1 m/s en wall-run).
11. **Déplacement et collisions** : en sous-pas si l'on va vite (un sous-pas tous les 0,3 m),
    `move_axis` sur X, puis Z, puis Y. `move_axis` résout les collisions AABB contre `boxes` et gère
    les **marches** (montée automatique jusqu'à `STEP_H` si on était au sol).
12. **Atterrissage** : rend le double saut, poussière, `landDip`, son `S_LAND`.
13. **Jump pads** : si la boîte sous les pieds (`P.groundBox`) est `B_PAD`, la vitesse devient
    `boxes[...].launch`.
14. **Wall-run** : `wall_check` sonde les 4 directions (0,12 m) et préfère le mur le plus parallèle à
    la course. Conditions : en l'air, pas au grappin, mur touché, `A_FWD` tenu, vitesse horizontale
    > `WALLRUN_MIN`, et moins de `WALLRUN_MAX` secondes de wall-run depuis le dernier contact au sol.
15. **Caméra** : hauteur des yeux, roulis (`roll`), balancement (`bob`), décroissance de `fovKick`.

Le champ de vision s'élargit avec la vitesse dans `render` :
`fovBase + clamp((vitesse - RUN_SPEED) × 0,7, 0, 20) + P.fovKick`.

### 3.3 Grappin (`grapple_tick`, `hook_release`, `grapple_bounce`)

- Entrées : `hookPressIn` (appui ce tick) et `hookHeldIn` (tenu), copiés depuis `actP[A_HOOK]` et
  `act[A_HOOK]` par `game_tick`. **Lâcher la touche lâche le grappin.**
- `HOOK_FLYING` : le crochet avance à `GRAPPLE_SPEED` depuis l'œil. Il s'accroche au premier obstacle
  touché : décor (`ray_world`), ennemi (`ray_sphere`, rayon `enemy_radius` + 0,3) ou joueur LAN
  (`np_aabb` agrandie de 0,3). Au-delà de `GRAPPLE_RANGE`, il revient (recharge 0,2 s).
- `HOOK_ATTACHED` sur le décor : traction `GRAPPLE_PULL` tant que la vitesse vers l'ancre est sous
  `GRAPPLE_MAXSPEED`. La corde (`ropeLen`) **ne se rallonge jamais** : on se balance autour de l'ancre.
  La corde casse si un obstacle la coupe plus de 0,15 s (`hookBlockT`), ou si l'on arrive à 1,6 m.
- Accroché à une **cible** (`hookEnemy` = index + 1, ou `hookPlayer` = id + 1) : l'ancre suit la
  cible, traction plus forte (`GRAPPLE_ENEMY_PULL`, `GRAPPLE_ENEMY_MAXSPEED`). À l'arrivée,
  **impact** : `GRAPPLE_STRIKE_DMG + vitesse relative × GRAPPLE_STRIKE_PER_MS` (via `damage_enemy`, ou
  `net_hit(..., W_HOOK)` × `PVP_SCALE` en LAN), puis `grapple_bounce` : rebond vers le haut, grappin
  rechargé tout de suite, double saut rendu.
- `hook_release` : recharge `GRAPPLE_CD` après une accroche (0,2 s si le crochet était en vol).
- Un kamikaze accroché n'explose pas au contact (`P.hookEnemy != i + 1` dans `enemy_tick`).

### 3.4 Combat

#### Vitesse relative

`rel_speed(targetVel)` = norme de `P.vel - targetVel`. L'épée et l'impact du grappin utilisent la
vitesse **relative** : foncer sur quelqu'un qui fonce sur vous frappe très fort.

#### Épée et parry (`melee`, `try_parry`, `parry_feedback`)

- Déclenchée par `actP[A_MELEE]` si `P.meleeCd <= 0`.
- Pour chaque ennemi et chaque joueur LAN (`np_target`) : portée `melee_range(rel)` =
  `MELEE_RANGE + rel × MELEE_RANGE_PER_MS`, dégâts `melee_dmg(rel)` = `MELEE_DMG + rel × MELEE_DMG_PER_MS`,
  dans un cône `MELEE_CONE` (sauf au contact, < 0,8 m).
- Un **kamikaze** (`EN_RUSHER`) touché à l'épée meurt sur le coup (paré). Un kill à l'épée en solo
  rend 15 PV. Toucher au moins une cible rend **une charge de dash**.
- Le coup ouvre la **fenêtre de parry** `P.parryWin = PARRY_WINDOW`. Pendant cette fenêtre,
  `game_tick` rappelle `try_parry` à chaque tick.
- `try_parry` renvoie les orbes ennemies (`PR_ORB`) et, en LAN, les roquettes des autres
  (`PR_ROCKET`/`PR_PARRIED` dont `owner != myId`) proches et devant soi. Portée
  `(MELEE_RANGE + 1,5) × PARRY_RANGE_SHRINK^parries` ; aide à la visée vers la cible la plus proche du
  viseur (`PARRY_AIM_COS`) ; le projectile devient `PR_PARRIED`, appartient au joueur, vitesse
  `PARRY_SPEED × PARRY_PINGPONG^(parries-1)`, multiplicateur ×`PARRY_ROCKET_MULT` au premier renvoi
  puis ×`PARRY_PINGPONG` (ping-pong). En LAN il émet `net_fx_parry` (événement `EV_PARRY`).
- `parry_feedback` : impact frame, hitstop 0,12 s, flash blanc (`parryFlash`), +`PARRY_HEAL` PV en
  solo (en LAN c'est l'hôte qui soigne), +1 dash, grappin rechargé, texte « PARRY ».
- **Coup dans le vide** (ni touche ni parry, `P.swingLanded == 0`) : à la fin de la fenêtre, la
  recharge passe à `MELEE_WHIFF_CD`.
- **Parry d'épée en PvP** : décidé par l'hôte (voir §4, `EV_MELEE_PARRY`). L'attaquant paré est
  repoussé et son épée bloquée `MELEE_PARRIED_CD` ; si les deux paraient en même temps, c'est un
  **choc d'épées** (`clash`).

#### Railgun (`fire_rail`) et chaîne de tir

- Arme 0. Coûte une munition (`P.railAmmo`, départ `RAIL_AMMO_START`, max `RAIL_AMMO_MAX`). À vide, le
  tir bascule automatiquement sur les roquettes.
- Hitscan depuis l'œil jusqu'au mur (`ray_world`, 400 m). **Perforant** : touche tous les ennemis et
  joueurs avant le mur. S'arrête sur la **première pièce** traversée (de n'importe qui) et déclenche
  `ricochet`.
- Dégâts `RAIL_DMG × streak_mult()` (× `PVP_SCALE` contre un joueur).
- **Chaîne de tir** (`shotStreak`) : `shot_result(hit)` incrémente sur une touche et remet à zéro sur
  un raté. `streak_mult` = 1 + `STREAK_DMG` × chaîne, `streak_rate` = 1 + `STREAK_RATE` × chaîne
  (plafonné à `STREAK_MAX`). La recharge vaut `RAIL_CD / streak_rate()`. Le rayon vire à l'or avec la
  chaîne. « CHAINE PERDUE » s'affiche si l'on perd une chaîne d'au moins 2 (`streakLostT`).
- **Recharges** (cellules bleues, `Pickup PU`) : `pickup_tick` ; +`PICKUP_AMMO`, réapparition après
  `PICKUP_RESPAWN` s. En solo `pickup_taken` direct ; en LAN l'hôte arbitre (`EV_PICKUP` →
  `host_take_pickup` → `EV_PICKUP_TAKEN`).

#### Pièces et ricochet (`throw_coin`, `coin_tick`, `ricochet`)

- `A_COIN` lance une pièce si `P.coins >= 1` (charges jusqu'à `COIN_MAX`, une toutes les
  `COIN_RECHARGE` s). La pièce hérite de la vitesse du joueur, tombe (`COIN_GRAVITY`), vit 4 s, meurt au
  contact du décor.
- Zone de touche généreuse `COIN_RADIUS`. Un tir de rail qui touche une pièce appelle
  `ricochet(ci, dmg, depth)` :
  1. s'il y a une autre pièce en vue à moins de 40 m (jusqu'à 6 rebonds), la chaîne continue vers elle
     (récursion, chaque pièce remultiplie) ;
  2. sinon, le tir part vers la cible visible la plus proche (ennemi ou joueur) dans `COIN_SEEK_RANGE`.
  - Pièce à soi : dégâts × `COIN_MULT`. Pièce **adverse** (« volée ») : renvoie le tir sans bonus —
    c'est le contre-jeu.
  - `lastChain` mémorise la longueur de la dernière chaîne (tests, journal).
- Chaque pièce a un `netId` (id du joueur + 1 dans les 8 bits hauts, compteur dans les 24 bits bas).
  En LAN : `net_fx_coin(0, ...)` à la création, `net_fx_coin(1, ...)` à la destruction (`EV_COIN`).

#### Compétences (`skill_tick`, `skillDef`, design : `docs/COMPETENCES_MARKSMAN.md`)

La pièce est une **compétence** parmi six (`enum SK_*`), choisie au LOADOUT (`wantSkill`, clé `skill=` de
`velocity.ini`, `--skill N` ou `--loadout a,b,m,s` pour les bots ; sans eux, un bot en tire une au hasard).
Touche `A_SKILL` (ancien `A_COIN`), charges `P.skillCh` (max et recharge par `skillDef[]`).

| Compétence | Fonction | Effet |
|---|---|---|
| `SK_COIN` PIÈCE | `throw_coin` | inchangée (4 charges, 2 s) |
| `SK_MARK` MARQUE | `skill_mark` | tir sans dégâts (90 m) : cible marquée 4 s (`markP[]` / `markE[]`), losange à travers les murs, mes touches ×1,25 |
| `SK_CHARGE` CHARGE | `skill_tick` | maintenir 0,6 s → `P.chargeT` = 2 s : la prochaine touche ×1,5 (`CHARGE_GRACE` pour les autres touches du même tir), perdue si on perd des PV |
| `SK_RICOCHET` RICOCHET | `ric_take`, `hitscan_shot` | `P.ricArmed` : le prochain rail / Élan est réfléchi (`r = d − 2(d·n)n`, 3 rebonds, 120 m), la roquette rebondit (`p->ricLeft`, ×0,8) ; recharge seulement après le tir ; retour sur soi à partir du 2e rebond, une fois par tir |
| `SK_ALLIN` ALL IN ONE | `skill_allin` | toute la munition (rail, roquettes, Éventail, Élan) en éventail régulier, demi-angle `allin_half(n)` ; refus à 1 munition, Scie / Ventouse / Tesla, bouclier levé |
| `SK_DECOY` MIRAGE | `skill_decoy` | `Coin` avec `decoy = 1` : immobile 3 s, absorbe un rail / Élan / plomb adverse (`decoy_absorb`), mon rail y ricoche ×1 ; ignoré par le yoyo, la scie, le tesla et le viseur adverse ; `decoy_skip` : un tir parti de l'intérieur le traverse |

- Bonus centralisé : `skill_out()` dans `damage_enemy`, `net_hit*`, `hit_player_ex` (une fois par touche).
- Réseau : `EV_SKILL` (marque, charge, all in one), `EV_COIN` sous-type 4 (leurre), `EV_ROCKET.sub` = rebonds.
- Bots (`bot_skill`, appelé en fin de `bot_think`) : MARQUE sur une cible vue à 10–60 m, CHARGE maintenue
  quand une cible approche, RICOCHET armé cible à couvert / près d'un mur (puis tir par le sol si elle reste
  cachée), ALL IN ONE à 4–14 m avec ≥ 3 munitions, MIRAGE sous 45 PV ou juste après une touche. Un bot vise
  le leurre adverse une fois sur deux pendant 0,4 à 1 s. Journal : ligne `competences : ...`, et
  `salves mortelles` dans la ligne `equilibre` (kill depuis la pleine vie en < 0,1 s, en plusieurs touches).
- Captures : `--shot skills.bmp` (marque, leurres), `--shot loadout_small.bmp` (fenêtre 1024×640),
  `loadout_charge` (description sur deux lignes).

#### Roquettes (`fire_rocket`, `proj_tick`, `explode`)

- Arme 1. Projectile `PR_ROCKET` (`spawn_proj`) à `ROCKET_SPEED`, recharge `ROCKET_CD / streak_rate()`.
- `proj_tick` fait avancer tous les projectiles. Pour une roquette : collision avec le décor, les
  ennemis et (en LAN) les joueurs. **Seul le tireur calcule les dégâts** (`mine`) ; chez les autres,
  l'explosion est purement visuelle (`explode_fx`) et la fin est synchronisée par `EV_ROCKET_END`.
- Impact direct : 40 × multiplicateur sur l'ennemi touché, plus `explode(c, ROCKET_RADIUS, ROCKET_DMG × mult, byPlayer)`.
- `explode` : dégâts dégressifs (30 % au bord, 100 % au centre) sur les ennemis, et sur les joueurs LAN
  (× `PVP_SCALE`). Le souffle pousse le joueur local (`ROCKET_KNOCK`) : c'est le **rocket jump** ;
  il coûte 5 × f PV si la roquette est la sienne, 30 × f si elle est ennemie. `byPlayer == 2` (projectile
  renvoyé) : ni recul ni dégât sur soi. Un rocket jump qui ne touche personne ne casse pas la chaîne.
- Projectiles renvoyés : orbe parée = `PARRY_DMG` en direct + explosion de 3 m / 80 dégâts ; roquette
  parée = 40 × `mult` en direct + explosion `ROCKET_DMG × mult`.
- Orbes ennemies (`PR_ORB`) : 12 dégâts au joueur et un petit recul.

#### Dégâts et mort

- `damage_enemy(i, dmg, knock)` : PV, flash blanc, chiffre de dégât (`dmg_number`), recul ; appelle
  `kill_enemy` à 0 PV.
- `kill_enemy(i, byPlayer)` : explosion de particules et d'éclats, impact frame et croix rouge
  (`killMarkT`) si tué par le joueur, score `(100 drone / 60 kamikaze) × (1 + vitesse relative / 15) × (1 + 0,25 × (combo − 1))`,
  soin (+15 drone / +8 kamikaze), combo (3 s pour enchaîner).
- `damage_player(dmg)` : en solo, retire des PV et gère la mort ; **en LAN, envoie
  `net_hit(myId, dmg, ..., W_SELF)`** car c'est l'hôte qui tient les PV.

#### Recharges et cadences (résumé)

| Quoi | Variable | Recharge |
|---|---|---|
| Tir rail | `P.fireCd` | `RAIL_CD / streak_rate()` |
| Tir roquette | `P.fireCd` | `ROCKET_CD / streak_rate()` |
| Changement d'arme | `P.switchT` | 0,22 s |
| Épée | `P.meleeCd` | `MELEE_CD`, `MELEE_WHIFF_CD` si raté, `MELEE_PARRIED_CD` si parée |
| Dash | `P.dashCharge` (0 à 2) | `DASH_RECHARGE` par charge ; +1 sur touche à l'épée ou parry |
| Grappin | `P.hookCd` | `GRAPPLE_CD` ; 0 après un impact sur cible ou un parry |
| Pièces | `P.coins` (0 à `COIN_MAX`) | `COIN_RECHARGE` par pièce |
| Slide boost | `P.slideCd` | `SLIDE_CD` |
| Munitions rail | `P.railAmmo` | cellules (`PICKUP_AMMO`), remises à `RAIL_AMMO_START` au respawn |

### 3.5 Ennemis et vagues (solo uniquement)

- Non simulés en LAN : `game_tick` n'appelle `enemy_tick` et `wave_tick` que si `!dmMode`.
- `EN_DRONE` (100 PV, rouge) : tourne autour du joueur à ~13 m et tire des orbes `PR_ORB` (16 m/s)
  s'il le voit à moins de 70 m ; la cadence augmente avec la vague.
- `EN_RUSHER` (50 PV, orange) : kamikaze, fonce sur le joueur (10,5 m/s + 0,3 par vague, max +3),
  18 dégâts au contact puis meurt. Apparaît à partir de la vague 2 (probabilité
  `min(0,55 ; 0,25 + 0,04 × vague)`).
- `spawn_enemy` : apparition en hauteur (9 à 18 m), entre `ms(28)` et `ms(48)` du centre, à plus de
  22 m du joueur.
- `wave_tick` : `2 + 2 × vague` ennemis (max 40), un toutes les 0,35 s ; 3 s de pause quand la vague
  est vidée, puis message « VAGUE n » et son `S_WAVE`.
- Mort en solo : `R` ou clic (après 1 s) relance (`reset_game`). Le record est `bestScore`.

### 3.6 Effets visuels

> Une autre session retouche actuellement ces effets : vérifiez les noms dans `main.c`.

| Effet | Données | Créer | Mettre à jour | Dessiner |
|---|---|---|---|---|
| Particules | `Part PA[MAXPA]` (anneau, `paNext`) | `add_part(pos, vel, col, life, size, grav)`, `burst(...)` | boucle dans `game_tick` | `render` (billboards additifs, texture `texDot`) |
| Rayons | `Beam BM[MAXBM]` | `add_beam(a, b, col, life)` | `game_tick` | `render` |
| Éclats physiques | `Shard SH[MAXSH]` | `shards(p, col, n, speed)` | `shard_tick` (gravité, rebonds sur le décor) | `render` (petites boîtes qui tournent) |
| Chiffres de dégâts | `DmgNum DN[MAXDN]` | `dmg_number(pos, val)` | `game_tick` | `draw_hud` (couleur selon la valeur ; `project` passe du monde à l'écran) |
| Impact frame | `impactT`, `impactDur`, `impactAt` | `impact_frame(at, dur)` (ignoré si `impactFrames == 0` ; garde le plus long) | décrémenté dans `render` (temps réel) | `render` : couleurs inversées (`GL_ONE_MINUS_DST_COLOR`) + traits radiaux noirs centrés sur `impactAt` |
| Hitstop | `hitstop` | `hitstop = fmaxf(hitstop, x)` | `game_tick` | — (le monde se fige) |
| Tremblement | `shake` | `shake += x` | `game_tick` (décroissance) | `render` |
| Flashs d'écran | `parryFlash`, `P.hurtFlash`, `hitMarker`, `killMarkT`, `muzzleT`, `hurtDirT`/`hurtFrom` | affectation directe | `game_tick` | `draw_hud`, `draw_viewmodel` |
| Textes | `popText`/`popT` (centre), `msg`/`msgT` (gros titre), `streakLostT` | `snprintf` + durée | `game_tick` | `draw_hud` |
| Effets d'écran | vignette, lignes de vitesse | — | — | `draw_hud`, si `screenFx` |

### 3.7 Audio synthétisé

- Format : 44,1 kHz (`SR`), **stéréo** entrelacé (gauche, droite), 16 bits. `NBUF` tampons de `BUFLEN` images
  (`BUFLEN` paires) ; `audio_fill(out, n)` écrit `n` images. Les bruitages sont mono (au centre), la musique est
  stéréo (`music_frame`) ; sous macOS `mac_audio_open` ouvre l'AudioQueue en 2 voies (non testé sur Mac).
- `audio_update` est appelé **à chaque image** : il remplit les tampons rendus par `waveOut`
  (`WHDR_DONE`) via `audio_fill`. Le mixage somme `MAXV` voix avec interpolation linéaire, puis un
  écrêtage doux `tanhf` multiplié par `masterVol`.
- `play(son, volume, hauteur)` prend une voix libre, sinon vole la plus avancée. `atten(p)` donne un
  volume selon la distance (sons des autres joueurs).
- Les sons `S_*` sont fabriqués **une fois** dans `make_sounds` (sinus, bruit filtré, enveloppes
  exponentielles) puis convertis par `mksound`. `sbuf(secondes, &n)` alloue un tampon de travail.
- Pas d'audio en mode `--shot` ni `--test` (`audio_ok == 0`, `play` ne fait rien).
- **Musique** (hard techno, un morceau par carte) : tout le design est dans **`music.h`** (morceaux `tracks[]`
  + moteur), inclus par `main.c`. Séquenceur synthétisé mixé dans `audio_fill` via `music_frame()` (stéréo), volume
  `musicVol` (`music=` dans `velocity.ini`, curseur « Musique »). Un morceau : BPM de départ, `bpmStep` (BPM
  gagnés par niveau), tonique (Hz), jusqu'à 16 **cellules** d'une mesure de 16 pas (`MusBar` : masques
  `kick/clap/hat/ohat`, notes `bass/lead` en demi-tons, `RS` = silence, `fx` = `FX_RISER` / `FX_CRASH`), et
  l'**ordre des cellules sur 32 mesures** (`song` aux niveaux 0-2 avec intro progressive, `songHi` dès le
  niveau 3 sans intro) plus une transposition `prog` par groupe de 4 mesures. Instruments : kick saturé à chute
  de hauteur, basse rumble en sidechain, hats/clap/crash/riser en bruit, lead supersaw (7 scies désaccordées, écart croissant avec le niveau), acid
  (scie dans un filtre résonant, dès le niveau 5).
  - **Stéréo** : kick, basse et acid au centre ; hats à des positions aléatoires ; clap, crash et riser larges
    (deux bruits différents) ; les 7 scies du supersaw réparties de gauche à droite ; écho ping-pong du lead
    (3 pas). `music_sample()` reste la version mono (moyenne) pour les tests.
  - **Diversité** : breaks, risers, crashs toutes les 8 mesures, filtre du lead qui respire sur 16 mesures, et
    micro-variations tirées au hasard (hats qui sautent ou fantômes, octaves, notes voisines dans la gamme
    `musScale`, notes courtes ou longues, rolls de kick, motif acid). Le hasard dépend de `musSeed`, changé à
    chaque entrée en partie : une même graine redonne exactement la même musique (tests).
  - `music_set(piste, niveau)` lance un morceau (`-1` = silence), fondu de 1,5 s.
  - **Intensité** : `music_level(n)` (0 à `MUS_LVMAX` = 8), appelé chaque image par `music_follow_game()`.
    Appliqué au début de la mesure suivante : tempo, couches (clap dès 1, lead clairsemé dès 2, lead complet
    dès 4, kicks syncopés dès 6), saturation du kick, clarté du lead.
    - **PvP** : un round = un kill. `music_pvp_level(leader, second, limite)` : 3 phases selon les frags du
      leader (tiers de `FRAG_LIMIT` : 0-4 / 5-9 / 10-14), niveaux `musPhaseLvl` = 0, 3, 6, puis le **final**
      (niveau 8) quand les deux premiers sont à limite − 1 (14-14, soit le 30e round). Pour la partie rapide à
      7 frags, l'hôte annonce `fragLimit` (`FRAG_LIMIT_QUICK` = 7) dans `PkSnap.fragLimit`.
    - **Solo** : niveau = numéro de vague − 1. La musique ne joue que pendant une partie (`APP_GAME`) : silence dans les menus, départ avec fondu à l'entrée en partie.
  - **Écouter sans le jeu** : `build_music.bat` puis `musique.exe [piste] [niveau]` (touches 0-8, +/-, P piste, R autre graine, Q) ou
    `musique.exe --wav sortie.wav piste niveau secondes`.
  - Tests : lignes « musique » de `run_tests`.

### 3.8 Entrées

Deux couches :

1. **Touches physiques** — `wndproc` remplit `keys[code]` (tenu) et `latch[code]` (appuyé depuis le
   dernier tick). Les codes 0 à 255 sont des **scancodes** (`(lp >> 16) & 0xFF` dans `wndproc`, donc indépendants
   de la disposition AZERTY/QWERTY), 256 et plus sont la souris : `K_LMB`, `K_RMB`, `K_MMB`, `K_X1`,
   `K_X2`, `K_WHEEL` (la molette n'existe qu'en « appui », jamais « tenue »).
2. **Actions** — le jeu ne lit **que** `act[A_*]` (tenu ce tick) et `actP[A_*]` (appuyé ce tick).
   `input_poll`, appelé au début de `game_tick`, construit ces tableaux à partir de `binds[a][0..1]`
   (deux touches par action, 0 = aucune) **et** de `botHeld`/`botPress`, puis vide `latch` et
   `botPress`.

Liste des actions (`A_*`, avec `actId` pour `velocity.ini`) : `A_FWD` avancer, `A_BACK` reculer,
`A_LEFT` gauche, `A_RIGHT` droite, `A_JUMP` sauter, `A_CROUCH` accroupi, `A_DASH` dash, `A_FIRE` tirer,
`A_MELEE` épée, `A_HOOK` grappin, `A_COIN` pièce, `A_WEAP1` arme1, `A_WEAP2` arme2, `A_SWAP`
changer_arme, `A_SCORES` scores. Les touches par défaut sont dans `bindDefault`.

Touches **non réaffectables**, codées en dur dans `wndproc` / `game_tick` : Échap (menu, ou annuler),
F2/F3 (sensibilité), F11 (plein écran), R (recommencer en solo, `latch[SC_R]`).

- `bound_held(a)` lit directement l'état tenu d'une action hors du tick (utilisé pour afficher le
  tableau des scores).
- `set_capture(on)` capture/relâche la souris (curseur caché et confiné). Perdre le focus relâche.
- La souris de visée vient de `WM_INPUT` (souris brute) dans `mdx`/`mdy`.
- **Bots** : écrivent `botHeld[A_*]` (tenu) et `botPress[A_*]` (appui d'un tick). Ils modifient aussi
  directement `P.yaw`, `P.pitch` et parfois `P.weapon`.

### 3.9 Réglages et `velocity.ini`

- Variables : `myName`, `sens`, `fovBase`, `masterVol`, `invertY`, `showFps`, `fullscreen`,
  `impactFrames`, `screenFx`, et les `binds`.
- Fichier : `velocity.ini` **à côté de l'exécutable** (chemin `cfgPath`, calculé avec
  `GetModuleFileNameA` dans `WinMain`). Format `cle=valeur`, une par ligne, `#` pour un commentaire :
  `name`, `sens`, `fov`, `volume`, `invert_y`, `show_fps`, `fullscreen`, `impact_frames`, `screen_fx`,
  `online`, `update_check` (vérifier les mises à jour au démarrage), `map` (carte choisie, §3.12), `map_rotate`,
  `ranked_id` / `ranked_key` (identité du mode classé, créée au premier lancement), `mmr` / `ranked_played` /
  `ranked_wins` (simple **cache d'affichage** de la cote : le serveur fait foi), puis `bind_<actId>=code1,code2`.
- `cfg_load` lit au démarrage (pas en mode bot ni `--shot`), borne les valeurs (`clampf`) et ignore
  les clés inconnues. Il renvoie 1 si le plein écran est demandé.
- `cfg_save` réécrit tout le fichier ; il ne fait rien si `botMode`, `shotMode` ou `cfgPath` vide.
  Appelé : après chaque réaffectation (`assign_bind`), en quittant PARAMÈTRES (bouton RETOUR ou
  Échap), sur PAR DÉFAUT, au lancement d'une partie (`do_action` pour `ACT_SOLO`/`ACT_HOST`/`ACT_BROWSE`)
  et au basculement plein écran. Les curseurs changent la valeur en direct mais ne sont sauvegardés
  qu'en quittant l'écran.

### 3.10 Interface et menus

- Primitives 2D : `rect`, `text` (avec ombre), `text_c` (centré), polices 0/1/2.
- Widgets immédiats : `button` (renvoie 1 au clic), `field` (champ texte, focus `uiFocus` 1 = pseudo,
  2 = IP ; la saisie passe par `WM_CHAR`), `slider(id, ...)` (glisser ; `uiDrag` mémorise l'`id` en cours).
- Écrans : `draw_main_menu` (pseudo, SOLO, PARTIE RAPIDE / PARTIE CLASSEE, HÉBERGER, REJOINDRE, encadré CLASSEMENT + TOP 3, PARAMÈTRES, QUITTER + aide
  `draw_controls`), `draw_join_menu` (parties trouvées + IP), `draw_pause` (en jeu, souris libre),
  `draw_settings` (réglages à gauche, commandes à droite ; `waitAct`/`waitSlot` = case en attente d'une
  touche ; Échap annule, Retour arrière/Suppr efface).
- `draw_hud` : si `appState != APP_GAME`, dessine seulement le menu correspondant. En jeu : noms au-dessus
  des joueurs (`draw_name_tags`), vignette, chiffres de dégâts, flèche de direction des dégâts, viseur,
  marqueurs, vitesse en m/s, charges de dash, recharge et cible du grappin, panneau ÉPÉE
  (`melee_preview` : dégâts et portée contre la cible visée), panneau CHAÎNE DE TIR, PV, arme et
  munitions, pièces, infos de partie (vague/score en solo ; frags, rang, temps et fil des kills
  `draw_killfeed` en LAN), écrans de mort et de fin de match (`draw_scoreboard`), puis pause ou
  paramètres si la souris n'est pas capturée. Sous 1 300 px de large, les panneaux passent en
  disposition compacte.
- `render` (3D) : ciel (`draw_sky`), caméra, brouillard, étoiles et soleil, la carte (liste
  d'affichage `worldList`), ombres des ennemis, ennemis (`draw_enemy`), joueurs distants
  (`draw_remote_players`), éclats, pièces, cellules, câble du grappin, puis tout ce qui brille en
  additif (rayons, particules, projectiles, reflets, jump pads), l'arme en main (`draw_viewmodel`),
  l'impact frame et enfin `draw_hud`.

### 3.11 Mise à jour depuis GitHub (bouton MISE A JOUR)

Section « Mise a jour » de `main.c`, juste avant « Interface ».

- **Source** : la branche `release` du dépôt public `void19845/velocity`, remplie par `.github/workflows/release.yml`
  à chaque push sur `stable` : `velocity.exe`, `Velocity-macOS.zip` et **`version.txt`** (`build`, `sha`,
  `win_size`, `win_sha256`, `mac_size`, `mac_sha256`). Lu à `https://raw.githubusercontent.com/void19845/velocity/release/`
  (`UPD_HOST`, `UPD_DIR`).
- **Numéro de build** : `VELOCITY_BUILD`, passé par `build.bat` / `build_mac.sh` si la variable d'environnement
  existe (la CI y met `github.run_number`, le même numéro que `version.txt`). Absent = `0` = « dev ».
  Une version est proposée si `build` de `version.txt` > `VELOCITY_BUILD`.
- **États** `updState` : `UPD_IDLE`, `UPD_CHECKING`, `UPD_UPTODATE`, `UPD_AVAILABLE`, `UPD_DOWNLOADING`, `UPD_READY`,
  `UPD_ERROR`. Le libellé du bouton vient de `upd_label`, le clic de `upd_click`, les messages de `upd_poll`
  (appelé par `draw_main_menu`). Le bouton n'existe que dans le menu principal.
- **Fils** : `upd_check_th` (lit `version.txt`, `upd_parse`) et `upd_download_th` (télécharge dans
  `velocity.exe.new`, progression `updGot` / `updTotal`, puis contrôle taille + **SHA-256** maison `sha256_file`).
  HTTP : `upd_http_get` (WinHTTP ; macOS : `/usr/bin/curl`). Données partagées sous `updCs`.
- **Installation** (`upd_install`, fil principal) : Windows renomme l'exe en cours en `velocity.exe.old`, met le
  nouveau à sa place (l'ancien est remis si ça échoue), relance avec `--updated` et quitte ; `upd_cleanup` supprime
  `.old` au démarrage suivant. macOS : `Velocity.app.zip` à côté du `.app`, extraction `ditto`, `.app` remplacé
  (ancien en `.old`), relance par `open -n … --args --updated`. `velocity.ini` n'est jamais touché.
- **Démarrage** : `upd_cleanup`, message « mise a jour installee » si `--updated`, puis vérification automatique
  silencieuse si `update_check=1` (`updQuiet` : un échec n'affiche rien). Rien en `--bot`, `--shot`, `--test`.
- **Erreurs** : réseau, « aucune version publiee » (HTTP ≠ 200), fichier corrompu, dossier non inscriptible
  (le bouton ouvre alors la page de la branche `release`, `UPD_PAGE`).
- **Tests** : lignes « mise a jour » et « sha256 » de `run_tests` ; capture `--shot update` (bouton en téléchargement).
- **Antivirus** : Windows Defender signale parfois `velocity.exe` (et les builds de test) comme
  `Trojan:Win32/Bearfoos.A!ml`, un faux positif heuristique déjà vu avant le bouton de mise à jour. Pistes :
  signaler le faux positif à Microsoft, ajouter une ressource de version (`VERSIONINFO`), signer le binaire.

### 3.12 Cartes (`MapDef`) et kill zone

Cinq cartes, décrites par la table `maps[]` (juste après les fonctions de construction, section « Monde ») :

| # | Nom | Fonction | `killY` | Solo | Idée |
|---|---|---|---|---|---|
| 0 | ARENE | `build_map_arene` | −1000 (fermée) | oui | la carte d'origine, **inchangée** (`run_tests` dépend du couloir x = −20 et des pads) |
| 1 | LE PUITS | `build_map_puits` | −30 | non | puits de 40 × 40 m, spirale de 8 plateformes reliées par des pads (y 0 → 42), sommet à 50 (grandes cellules), 12 ancres au centre, 2 îlots de secours au fond (pads vers la spirale) |
| 2 | LE CANYON | `build_map_canyon` | −15 | non | deux falaises (y 20) et un gouffre de 70 m ; pads au bord → îlots extérieurs → îlots centraux ; tour de tir de 10 m par falaise ; 6 ancres espacées de 20 m au plus |
| 3 | LA BOUCLE | `build_map_boucle` | −1000 | oui | anneau de 120 × 60 m, deux voies par ligne droite avec bandes d'accélération à contresens, séparateurs bas, bloc central coupé au milieu, passerelles et toit à 10 m, pads de coin et pads vers le toit |
| 4 | LE DOJO | `build_map_dojo` | −1000 | oui | salle de 36 × 36 m, plafond à 7 m, 16 piliers, ring surélevé de 0,5 m, **aucune cellule** |

- **`MapDef`** : `name` (sans accents), `build`, `spawns`/`nspawns` (coordonnées du monde ; l'Arène garde ses
  anciens points via la macro `MS()` = `ms()` à la compilation), `start` (départ solo et position d'attente en LAN),
  `killY`, zone des drones du solo (`enemyMinY/MaxY`, `enemyRMin/RMax` autour du centre, plancher `enemyFloor`),
  caméra du menu (`menuCam`, `menuPitch` ; le spectateur est `specUp` mètres plus haut), `solo`, `music`, couleurs
  `sky` (haut du ciel) et `fog` (horizon et brouillard, utilisées par `draw_sky` et `render`).
- **Chargement** : `build_level(m)` remet `nb = npu = 0` et appelle `maps[m].build()`. `load_map(m)` fait
  `build_level`, reconstruit la liste d'affichage (`glDeleteLists` + `build_world_list`, seulement si `glReady` :
  `--test` n'a pas de contexte OpenGL), vide projectiles, pièces, drones, particules, rayons, rend les scies, remet
  les cellules. `curMap` = carte chargée ; `menuMap` / `mapRotate` = choix du menu (`map=`, `map_rotate=` dans
  `velocity.ini`).
- **Branché sur `MapDef`** : `pick_spawn`, `reset_game`, `menu_backdrop` (charge `menuMap` : le fond du menu montre
  la carte choisie), la vue du spectateur (`on_welcome`), `spawn_enemy`, le plancher des drones dans `enemy_tick`,
  la scie perdue (`map_lost_y()` = `killY`, ou `SAW_LOST_Y` sur une carte fermée), ciel et brouillard.
- **Outils de carte** (coordonnées du monde, sans `MAP_S`) : `addpad_at(x, y, z, v)` (pad posé à la hauteur y),
  `addpad_to(x, y, z, cible, vy)` (calcule la vitesse avec `pad_aim` pour retomber sur `cible`), `addspeed`
  (bande d'accélération), `addanchor` (ancre de grappin : cube de 1,2 m jaune), `addcell` (cellule).
- **`B_SPEED`** (bande d'accélération) : boîte plate de 0,12 m ; `launch` = sens × `SPEED_MAX`. Dans `player_tick`,
  au sol dessus : aucune friction, et `+SPEED_ACCEL` m/s² dans son sens jusqu'à `SPEED_MAX` (28 m/s). Rendu :
  arêtes roses dans la liste d'affichage, chevrons qui défilent dans `draw_map_fx` (hors liste).
- **Choix** : menu principal, ligne « CARTE : < NOM > » sous le pseudo (flèches cliquables) + bouton ROTATION
  (l'hôte passe à la carte suivante à chaque nouveau match). SOLO utilise la carte choisie si elle est `solo`,
  sinon l'Arène (le bouton l'indique). Options `--map N` (bots, captures).
- **Réseau** (`NET_VER` 13) : l'id de carte voyage dans `PkWelcome.pad[0]` (le client fait `load_map` avant
  `on_welcome`), `PkAnnounce.pad[0]` (affiché dans REJOINDRE : « [LAN] Hote - LE PUITS ») et `EV_MATCH`
  (`target`, sous-type 0 : début de match ; l'hôte y met la carte suivante si ROTATION). Le lobby en ligne ne
  transmet pas la carte (on la reçoit à la connexion).

**Kill zone** (`void_check`, appelée dans `game_tick` juste après `player_tick`) :
- sous `killY` : en solo `damage_player(999)` ; en LAN `net_hit(myId, 999, …, W_VOID)` (renvoyé toutes les 0,5 s
  tant que l'hôte n'a pas tranché). Un corps mort s'arrête 6 m sous le plan.
- **`W_VOID`** (dernier des `W_*`, « VIDE » dans le fil) : pas de plafond `PVP_HIT_CAP`, pas d'Égide, pas compté
  dans le temps pour tuer. L'hôte mémorise pour chaque joueur le dernier attaquant et l'heure
  (`NP[v].lastHitBy`, `lastHitT`, mis à jour dans `host_apply_damage`, remis à zéro par `host_respawn`) : une chute
  moins de `VOID_CREDIT_T` (4 s) après une touche donne le kill à l'attaquant (« Rival [VIDE] Ninja »), sinon
  c'est un suicide (« Ninja est tombe dans le vide », −1 frag). Compteurs `voidCredited`, `voidSuicides` (journal).
- projectiles et pièces sous `killY − 2` disparaissent ; la scie est « perdue » et revient après `SAW_LOST_T`.
- lisibilité : plan rouge sombre et grille additive qui respire 3 m sous `killY` (`draw_map_fx`, hors liste
  d'affichage, sans brouillard) ; vignette rouge dans `draw_hud` quand on tombe à moins de `VOID_WARN_H` (8 m) du plan.
- **Bots** : `bot_void_guard` (appelée dans la boucle principale juste après `bot_think`) annule un saut ou un dash
  dont la trajectoire balistique (`bot_lands`) ne retombe sur aucun sol avant `killY`, freine au bord (dos au vide),
  et en chute sans issue tente le double saut puis un **grappin de secours** vers le dessus de boîte le plus proche.

---

## 4. Réseau LAN

### Principes

- **Hôte autoritaire léger.** L'hôte est aussi un joueur (id 0). Les clients ont les ids 1 à
  `MAXP - 1`. L'hôte fait autorité sur : **PV, kills, morts, réapparitions, recharges ramassées et
  déroulement du match**.
- **Chaque joueur simule son propre mouvement** et envoie son état ; pas de prédiction ni de
  correction côté hôte.
- **Les touches sont validées chez le tireur** (« favoriser le tireur ») : le tireur détecte la
  touche avec les positions affichées (`NP[i].disp`) et envoie un événement `EV_HIT`. L'hôte applique
  les dégâts (avec plafond) et décide des kills.
- **Le recul est appliqué par la victime** (à la réception de `EV_HIT`).
- UDP non bloquant, port `NET_PORT` (27960). Chaque paquet commence par `PkHead`
  (`magic` = `NET_MAGIC`, `type`, `from`, `ver` = `NET_VER`) ; `net_poll` jette tout paquet dont le
  magic ou la version diffère.
- La géométrie n'est **pas** transmise, seulement l'**id de carte** (`PkWelcome.pad[0]`, `EV_MATCH.target`, §3.12) :
  chaque exécutable la reconstruit (`build_level(m)` est déterministe, graine `rng = 777` pour l'Arène). Tous les
  joueurs doivent donc avoir le même exécutable.

### Paquets `PK_*`

| Paquet | Structure | Émetteur → destinataire | Fréquence | Contenu / effet |
|---|---|---|---|---|
| `PK_QUERY` | `PkHead` | client → diffusion `255.255.255.255:27960` **et** `127.0.0.1:27960` | 1/s tant que non connecté | « qui héberge ? » |
| `PK_ANNOUNCE` | `PkAnnounce` | hôte → client | en réponse | nom de l'hôte, nombre de joueurs, `MAXP` ; `found_add` côté client (oublié après 3,5 s) |
| `PK_JOIN` | `PkJoin` | client → hôte | toutes les 0,4 s pendant 5 s max (0,2 s pendant 10 s si la partie vient du lobby) | pseudo, drapeau `spec` (spectateur) ; `host_on_join` (même adresse = même id) |
| `PK_WELCOME` | `PkWelcome` | hôte → client | en réponse | id attribué (`MAXP` = spectateur), ou `refused = 1` si la partie (ou les places de spectateur) est pleine ; `on_welcome` côté client |
| `PK_PUNCH` | `PkHead` | hôte → client annoncé par le lobby | toutes les 0,1 s pendant 4 s | ouvre le NAT de l'hôte vers le client ; ignoré à l'arrivée |
| `PK_KEEP` | `PkHead` | spectateur → hôte | 2/s | « je suis là » (sinon l'hôte retire le spectateur après 5 s) |
| `PK_STATE` | `PkState` (`NetPState`) | client → hôte | 60/s | position, vitesse, `hookPos`, yaw/pitch, drapeaux `NF_*`, arme, avancement du coup d'épée |
| `PK_SNAP` | `PkSnap` | hôte → tous les clients | 60/s | `matchT`, `matchOver`, gagnant, `pickMask` (bit i = cellule i disponible), `fragLimit`, et pour chaque joueur : actif, vivant, PV, kills, morts, `NetPState` ; `apply_snap` côté client |
| `PK_ROSTER` | `PkRoster` | hôte → clients | toutes les 0,5 s et à chaque arrivée | les pseudos |
| `PK_EVENT` | `PkEvent` (`NetEvent`) | dans les deux sens (le client n'envoie qu'à l'hôte) | joint à chaque envoi d'état | un événement (voir plus bas) |
| `PK_LEAVE` | `PkHead` | dans les deux sens | 3 fois à la fermeture (`net_shutdown`) | départ ; `host_drop` côté hôte, retour au menu côté client |

Drapeaux d'état `NF_*` dans `NetPState.flags` : `NF_CROUCH`, `NF_SLIDE`, `NF_WALLRUN`, `NF_HOOK`,
`NF_DEAD`, `NF_PARRY` (en fenêtre de parry ; sert à l'hôte pour le parry d'épée). `pack_state` remplit
`NetPState` depuis `P`.

**Délais** : l'hôte retire un client muet depuis plus de 5 s (`host_drop`) ; le client revient au menu
s'il ne reçoit plus de `PK_SNAP` pendant 5 s (`net_disconnect_to_menu`).

**Lissage** : à la fin de `net_update`, la position affichée `NP[i].disp` des autres joueurs suit
`st.pos` extrapolée par `st.vel` (au plus 0,1 s), avec un lissage rapide, et saute directement si
l'écart dépasse 4 m. Les touches sont calculées sur `disp` (via `np_center`, `np_aabb`).

### Événements `EV_*`

Un `NetEvent` contient : `eid` (identifiant unique), `netId` (objet concerné : roquette, pièce),
`kind` (`EV_*`), `from` (émetteur), `target`, `sub`, `actor`, et `f[7]` (flottants libres).

| Événement | Émis par (fonction) | Champs | Arbitrage hôte (`host_handle`) | Effet à la réception (`apply_event`) |
|---|---|---|---|---|
| `EV_RAIL` | tireur : `fire_rail`, `ricochet` via `net_fx_rail` | `f[0..2]` départ, `f[3..5]` arrivée, `f[6]` teinte or | — | rayon, étincelles, son |
| `EV_ROCKET` | tireur : `fire_rocket` via `net_fx_rocket` | `netId`, `f[0..2]` pos, `f[3..5]` vitesse | — | crée la roquette (`owner = from`), purement visuelle chez les autres |
| `EV_ROCKET_END` | tireur : `proj_tick` via `net_fx_rocket_end` | `netId`, `f[0..2]` point d'explosion | — | supprime la roquette, `explode_fx` |
| `EV_PARRY` | celui qui pare : `try_parry` via `net_fx_parry` | `netId`, pos, vitesse, `f[6]` mult, `sub` nombre de renvois | soigne l'émetteur de `PARRY_HEAL` | la roquette devient `PR_PARRIED`, appartient à l'émetteur |
| `EV_HIT` | tireur : `net_hit` (rail, roquette, épée, grappin, ricochet) ; victime elle-même pour `W_SELF` (`damage_player`) | `target`, `sub` = arme `W_*`, `f[0]` dégâts, `f[1..3]` recul | **voir ci-dessous** | victime : recul, flash rouge, flèche de direction ; autres : flash blanc sur la cible |
| `EV_MELEE` | `melee` via `net_fx_melee` | `f[0..2]` œil, `f[3..5]` direction, `f[6]` vitesse | — | arc de particules, son |
| `EV_KILL` | **hôte** : `host_kill` | `actor` tueur, `target` victime, `sub` arme | — | fil des kills, explosion, mort locale (`P.dead`, `killerOfMe`), combo du tueur |
| `EV_RESPAWN` | **hôte** : `host_respawn` (point choisi par `pick_spawn`) | `target`, `f[0..2]` position | — | réapparition (PV, munitions, pièces, dash remis à neuf) |
| `EV_MATCH` | **hôte** : `host_start_match` (`sub = 0`), `host_end_match` (`sub = 1`, `target` = gagnant) | `sub`, `target` | — | « COMBAT ! » ou « X GAGNE » |
| `EV_INFO` | **hôte** : `host_on_join` (`sub = 0`), `host_drop` (`sub = 1`) | `target` | — | ligne « a rejoint / a quitté » dans le fil |
| `EV_PICKUP` | client : `pickup_tick` (demande) | `sub` = index de la cellule | `host_take_pickup` (premier arrivé, premier servi) | — (aucun cas dans `apply_event`) |
| `EV_PICKUP_TAKEN` | **hôte** : `host_take_pickup` | `sub` cellule, `target` joueur | — | `pickup_taken` (munitions si c'est moi, sinon effet) |
| `EV_COIN` | lanceur : `throw_coin` (`sub = 0`), tireur : `ricochet` (`sub = 1`) via `net_fx_coin` | `netId`, pos, vitesse | — | crée la pièce d'un autre (`owner = from`) ou la détruit |
| `EV_MELEE_PARRY` | **hôte** : `host_handle` | `actor` défenseur, `target` attaquant, `sub` = choc d'épées | soigne le défenseur | défenseur : `parry_feedback` ; attaquant : repoussé, épée bloquée, hitstop |

Les armes `W_*` (champ `sub` de `EV_HIT` et `EV_KILL`) : `W_RAIL`, `W_ROCKET`, `W_MELEE`, `W_HOOK`,
`W_PARRY` (projectile renvoyé), `W_SELF` (dégâts sur soi, non plafonnés), `W_COIN` (ricochet). Les
noms affichés dans le fil des kills sont dans `weaponNames`, dans le même ordre.

### Fiabilité : renvoi ×3 et dédoublonnage

- `net_emit(ev)` (point d'entrée de **tout** événement local) : ne fait rien hors réseau ; attribue
  `eid = (myId + 1) << 24 | compteur` et `from = myId` ; marque l'`eid` comme déjà vu (`ev_seen`) ; si
  l'on est l'hôte, appelle tout de suite `host_handle` ; puis `net_queue`.
- `net_queue` place l'événement dans la file circulaire `outq` (`OUTQ` places) avec `left = 3`.
- `flush_events`, appelé à chaque envoi d'état (60/s), envoie chaque événement dont `left > 0` et
  décrémente : **chaque événement part 3 fois** sur 3 envois consécutifs.
- `ev_seen(eid)` garde les 512 derniers `eid` (`seenIds`) : une copie déjà vue est ignorée.
- **L'émetteur n'applique jamais son propre événement via `apply_event`** (son `eid` est déjà marqué
  vu). Il doit donc produire lui-même l'effet local (ex. : `fire_rail` dessine son rayon). Exception :
  les décisions de l'hôte (`host_kill`, `host_respawn`, `host_start_match`, `host_end_match`,
  `host_drop`, `host_take_pickup`, `EV_MELEE_PARRY`, `EV_INFO` d'arrivée) appellent `net_emit` **puis**
  `apply_event` explicitement.
- **Relais par l'hôte** : `host_recv` reçoit un `PK_EVENT` d'un client, vérifie que `ev.from` est bien
  l'id lié à l'adresse d'envoi (« un client ne parle qu'en son nom ») et que l'`eid` est nouveau, puis
  `host_handle`, `apply_event` et `net_queue` (relais à tous les clients, l'émetteur l'ignorera).
- Le client n'accepte des `PK_EVENT` que de l'hôte (`client_recv`).

### Autorité de l'hôte (`host_handle`, `host_kill`, `host_tick`)

`host_handle` reçoit chaque événement (local ou relayé) :

- `EV_PICKUP` → `host_take_pickup`.
- `EV_HIT` :
  1. ignore si la cible n'existe pas, est morte, ou si le match est fini ;
  2. **parry d'épée** : si l'arme est `W_MELEE` ou `W_HOOK` et que la victime était en fenêtre de parry
     il y a moins de `MELEE_PARRY_GRACE` s (`NP[v].lastParryT`, mis à jour par `host_tick` pour l'hôte
     et par `NF_PARRY` dans `PK_STATE` pour les clients), le coup est annulé et l'hôte émet
     `EV_MELEE_PARRY` (choc d'épées si l'attaquant paraît aussi) ;
  3. sinon retire `min(dégâts, PVP_HIT_CAP)` PV (`W_SELF` n'est pas plafonné) ;
  4. à 0 PV : `host_kill(tueur, victime, arme)`.
- `EV_PARRY` → soigne l'auteur du parry de `PARRY_HEAL`.

`host_kill` : marque la victime morte (réapparition dans `RESPAWN_TIME`), +1 kill et +`KILL_HEAL` PV
au tueur ; un **suicide** retire un kill ; émet `EV_KILL` ; termine le match au `FRAG_LIMIT`.

`host_tick` (dans `game_tick`, hôte seulement) : recopie l'état de l'hôte dans `NP[0]`, impose
`P.hp = NP[myId].hp`, avance `matchT`, termine le match à `MATCH_TIME` (le plus de frags gagne), fait
réapparaître les morts, et relance un match 8 s après la fin (`matchEndT`).

Où s'applique quoi :

- `PVP_SCALE` est appliqué **par le tireur** au moment d'appeler `net_hit`.
- `PVP_HIT_CAP` est appliqué **par l'hôte** dans `host_handle` (le chiffre de dégât affiché par
  `net_hit` est aussi plafonné, pour être juste).
- Les clients reçoivent leurs PV par `PK_SNAP` (`apply_snap` écrase `P.hp`).

Statistiques d'équilibrage tenues par l'hôte pour le journal du bot : `hitCount`, `parryCount`,
`ttkSum`/`ttkN` (temps pour tuer), `oneShots`, `meleeParries`, `clashes`.

### Découverte et connexion

1. « REJOINDRE » → `net_browse_start` : socket sur un port libre (`net_open(0)`), `netMode = NET_CLIENT`,
   `appState = APP_JOIN`.
2. `net_update` diffuse `PK_QUERY` chaque seconde (diffusion **et** boucle locale : deux instances sur la
   même machine se trouvent). Les réponses `PK_ANNOUNCE` remplissent `found`.
3. Cliquer une partie (ou taper une IP) → `net_connect` → `PK_JOIN` répétés. `PK_WELCOME` →
   `on_welcome` → `dm_local_start` et `APP_GAME`.
4. « HÉBERGER » → `net_host_start` : écoute sur `NET_PORT` (échoue si le port est pris), `NP[0]` actif,
   `find_local_ip` (affiché dans le menu de pause), réapparition immédiate de l'hôte.

### En ligne : lobby, STUN, perçage de NAT

Plan et protocole complet : [`PLAN_EN_LIGNE.md`](PLAN_EN_LIGNE.md). Le jeu de combat ne change pas : seules la
**découverte** et la **mise en relation** passent par Internet, le trafic de jeu reste de l'UDP direct.

- **Lobby** (`worker/`, Cloudflare Worker + Durable Object, gratuit) : lignes de texte sur un WebSocket.
  `worker/src/lobby.js` = logique pure (testée par `worker/test.js`), `index.js` = Worker/DO hibernable,
  `dev-server.js` = même logique en local (`node dev-server.js 8787`).
- **Client** (`main.c`, section « Parties en ligne », après `net_connect_ex`) :
  - fil WinHTTP (`lb_reader`) qui lit le WebSocket et fil d'écriture (`lb_writer`, `ping` toutes les 25 s),
    deux files protégées par `lbCs` ; le jeu ne touche aux sockets du lobby que via `lb_send` / `lb_update`.
  - `lb_update(dt)` (appelé par `net_update`) : (re)connexion avec attente de 4 s, lecture des lignes
    (`lb_line`), requêtes STUN, publication du salon de l'hôte (`host …`, au changement et toutes les 20 s),
    envoi des demandes en attente (`join` / `quick`), émission des `PK_PUNCH`.
  - **STUN** (`stun_send` / `stun_parse` / `stun_got`) : requête depuis le socket du jeu vers
    `stun.l.google.com:19302` ; la réponse est reconnue dans `net_poll` avant le contrôle du magic.
  - `join_found(i, spec)` : LAN → `net_connect_ex` direct ; en ligne → demande `join` au lobby, qui répond
    `go <ep>` ; `quick_play` → `quick`, et `none` → `lbWantHost` (le jeu héberge lui-même).
  - Si hôte et client ont la même IP publique, le lobby renvoie l'adresse **LAN** de l'hôte (pas de perçage).
- **Réglages** : `online=0/1` et `lobby=hote[:port]` dans `velocity.ini` (un port ≠ 443 = `ws://`, sinon `wss://`).
  Le défaut est `LOBBY_DEFAULT` (à remplacer après `wrangler deploy`) ; `lobby=` n'est écrit que s'il diffère.
- **Limites** : NAT symétrique / CGNAT → pas de perçage (message « connexion impossible » après 10 s, la saisie
  d'IP reste possible). Un relais UDP pourrait être ajouté sans changer le protocole de jeu.

### Spectateurs

- Un spectateur envoie `PK_JOIN` avec `spec = 1`. `host_on_join` lui donne une place du tableau `SP[MAXSPEC]`
  (pas de slot `NP`, donc il ne compte pas dans les 8 joueurs) et répond `PK_WELCOME` avec `id = MAXP`.
  `host_send_all` envoie aussi aux spectateurs : snapshots (60/s ≈ 28 Ko/s chacun), événements, roster.
- Côté client : `on_welcome` met `spectating = 1` et `myId = MAXP` (d'où `NP[MAXP + 1]` : la place `MAXP` reste
  vide). `game_tick` appelle `spec_tick` au lieu du tick joueur : caméra libre (déplacement, saut/accroupi =
  monter/descendre, dash = rapide), **tir = suivre le joueur suivant** (vue à la troisième personne,
  `specFollow`), puis retour à la caméra libre. `P.dead` reste à 1 : aucune arme, aucun dégât subi.
  `draw_spec_hud` remplace le HUD (classement, minuteur, fil des kills) et `draw_viewmodel` est sauté.
- Le spectateur n'envoie que `PK_KEEP` ; l'hôte le retire après 5 s sans nouvelle.

### Mode classé (PARTIE CLASSEE, cote Elo tenue par le serveur)

Pas de compte : le classement est tenu par le **serveur de classement** (Durable Object `Ratings` du worker, SQLite),
qui calcule lui-même l'Elo à partir des rapports de fin de match. L'hôte fait autorité sur le déroulement du match
(frags, gagnant, joueurs partis), comme pour le reste du jeu.

**Identité** — au premier lancement, `rk_identity` tire `ranked_id` (128 bits) et `ranked_key` (128 bits secrets,
`RtlGenRandom` sous Windows, `arc4random_buf` sous macOS), écrits dans `velocity.ini`. Le serveur enregistre le SHA-256
de la clé au premier contact (confiance au premier usage) ; toute requête ensuite doit présenter la clé (sinon 403).
Le pseudo peut changer. Les bots (`--bot … --ranked`) utilisent une identité temporaire (jamais sauvegardée).

**Requêtes HTTP** (`rk_http`, une à la fois dans un fil : WinHTTP sous Windows, `curl` du système sous macOS ; même
hôte que le lobby, `lobbyAddr`, donc `--lobby 127.0.0.1:8787` vise le `dev-server.js`) :

| Requête | Corps / paramètres | Réponse |
|---|---|---|
| `POST /ranked/hello` | `{id, key, name}` | `{ok, mmr, played, wins, tier, rank}` (enregistre l'identité si nouvelle) |
| `POST /ranked/report` | `{id, key, match, host, winner, players:[{id, kills}], left:[id]}` | `{ok, state:"pending"}` ou `{ok, state:"applied", delta, mmr, played, wins, tier}` ; `expired`, `ignored` |
| `GET /ranked/top?id=…` | — | `{ok, top:[{name, mmr, tier, played}] (10 max, placement fini), me:{rank, mmr, …}}` |

Au démarrage (hors `--shot`/`--test`, et hors bots sans `--ranked`) : `hello` puis `top`. Le menu principal affiche
l'encadré **CLASSEMENT** (`draw_rank_box`) : mon palier et ma cote, `#rang`, puis le **TOP 3** (podium or / argent /
bronze) et une 4e ligne « toi : #rang, cote » si je n'y suis pas ; « classement indisponible » sans réseau. Sous
~1540 px de large, l'encadré devient une ligne sous le titre. Il est rafraîchi après chaque match classé.

**Formule (référence unique, appliquée par `worker/src/ratings.js` ; `rank_delta` dans `main.c` est identique)** :
- cote de départ `RANK_START` = 1000, bornée à [`RANK_MIN`, `RANK_MAX`] = [100, 3000] ;
- Elo multijoueur **par paires** : pour le joueur `a` contre chacun des `n` autres participants `b`,
  `E = 1 / (1 + 10^((Rb − Ra) / 400))`, `S` = 1 / 0,5 / 0 selon le classement final ;
  `delta = K × Σ(S − E) / n`, arrondi au plus proche (0,5 s'éloigne de zéro, comme `lroundf`). En 1 contre 1 à
  1000 contre 1000, le gagnant prend `K / 2` ;
- `K` = `RANK_K_PLACE` = 64 pendant les `RANK_PLACEMENT` = 10 premiers matchs (affichés « PLACEMENT x/10 »),
  puis `RANK_K` = 32 ;
- classement final : le gagnant désigné par l'hôte d'abord, puis les frags (égalité = 0,5) ; tout présent bat tout
  joueur parti ; entre partis, le dernier parti passe devant le premier. **Quitter un match en cours = défaite**
  contre tous ;
- paliers (`RANK_ARGENT` 900, `RANK_OR` 1050, `RANK_PLATINE` 1200, `RANK_DIAMANT` 1350, `RANK_MAITRE` 1500) :
  BRONZE, ARGENT, OR, PLATINE, DIAMANT découpés en divisions III (bas) / II / I (haut) de 50 points ; MAITRE sans
  division. BRONZE commence à 750 pour le calcul des divisions.

**Déroulement** :
1. **PARTIE CLASSEE** (`ACT_RANKED` → `ranked_play`) : il faut `online=1` et un `hello` réussi (sinon message clair :
   « PARTIE CLASSEE indisponible … »). Partie rapide du lobby avec `quick <ver> <ep> 1 <cote>` : salon **classé**
   de cote la plus proche ; `none` → on héberge une partie classée (`hostRanked`). La partie rapide normale évite les
   salons classés. Le salon classé publie `host … <nom> 1 <cote moyenne>` ; REJOINDRE affiche « [CLASSEE] » (LAN :
   `PkAnnounce.pad[1]`). Rejoindre une partie classée sans serveur de classement est refusé.
2. **Règles figées** : match à mort normal, limite `FRAG_LIMIT`, `MATCH_TIME` ; jamais contre un bot (`vsBot`) ;
   les spectateurs ne comptent pas. L'hôte refuse (`PkWelcome.refused = 2`) un joueur sans identité de classement.
   Le premier adversaire qui arrive relance le match (`host_start_match`) ; si tous les adversaires partent, l'hôte
   gagne (`host_end_match`).
3. **Réseau** : `PkJoin` porte `rid` (16 octets), `mmr` et `rplay` (cache) ; l'hôte les diffuse dans `PkRoster`
   (`mmr[]`, `rplay[]`, `rid[]`, `ranked`, `mid` = identifiant du match tiré par l'hôte à chaque match, `left[]` /
   `nleft` = joueurs partis pendant le match, listés par l'hôte). `PkWelcome.pad[1]` = partie classée.
   `pad[0]` de `PkAnnounce`/`PkWelcome` reste réservé à la carte.
4. **Fin de match** (`rk_tick`, appelé par `net_update`) : `RANK_END_WAIT` (0,5 s) après `matchOver`, l'hôte **et**
   chaque client envoient le même rapport (`rk_send_report`) ; il est renvoyé toutes les `RANK_RETRY` s tant que le
   serveur répond `pending` (`RANK_TRIES` fois). Si l'hôte part en plein match (`PK_LEAVE` ou silence de 5 s), les
   clients rapportent l'hôte comme parti et sans gagnant (`rk_host_gone`). Après `applied`, le client renvoie un
   `PK_JOIN` (même adresse = même id) pour que l'hôte diffuse sa nouvelle cote.
5. **Affichage** : palier + cote dans le tableau des scores (colonne CLASSE), à côté des noms en jeu, titre
   « PARTIE CLASSEE » dans le HUD, et sous le tableau de fin de match « +18 MMR -> OR II (1093) » (`draw_rank_result` ;
   estimation locale en attendant le serveur).

**Concordance côté serveur** (`accepted` dans `ratings.js`) : les rapports d'un même `match` sont groupés par forme
canonique (hôte, gagnant, présents triés avec frags, partis). Un groupe est appliqué si une **majorité stricte des
présents** l'a envoyé (1 contre 1 : les deux) ; s'il déclare des partis, l'hôte doit être dans le groupe, ou bien
l'hôte est lui-même parti et au moins 2 joueurs confirment. Un match non confirmé en 10 min (`EXPIRE_MS`) est perdu ;
un match appliqué ne l'est qu'une fois (anti-rejeu, gardé 7 jours). Limite de débit : 30 requêtes/min par identité.

**Limites (pas de vrai anti-triche sans serveur de jeu)** : un hôte truqué peut mentir sur les frags ou déclarer
parti un client en 1 contre 1 (l'hôte fait déjà autorité sur tout le match) ; plusieurs identités jetables peuvent
s'arranger entre elles (pas de compte) ; en 1 contre 1, un hôte qui quitte n'est pas sanctionné (un client seul ne
peut pas le déclarer parti) ; la cote annoncée dans `PkJoin` (affichage, matchmaking) est déclarative.

Tests : lignes `classe …` de `run_tests()` (Elo, abandon, paliers, placement, identité, `velocity.ini` de test,
lecture du TOP, construction du rapport) ; `node test.js` (lobby classé, Elo, enregistrement, mauvaise clé,
concordance, désaccord, rapport seul, abandon, hôte parti, expiration, top, limite de débit) ;
`--shot ranked.bmp` (fin de match classée) et `--shot menu.bmp` (encadré TOP 3 avec données factices).

### `NET_VER`

`NET_VER` est comparé à chaque paquet reçu. **Incrémentez-le à chaque changement** de structure de
paquet (`PkHead`, `NetPState`, `NetEvent`, `Pk*`, `SnapP`), de signification d'un champ, d'ajout d'un
`EV_*` ou d'un `PK_*`, et aussi de **carte** (les index de cellules et `pickMask` doivent coïncider).
Une version différente rend simplement les parties invisibles l'une à l'autre.

---

## 5. Tous les `#define` de réglage

### Moteur

| Nom | Valeur | Effet |
|---|---|---|
| `PI` | 3,14159265 | π |
| `TICK` | 1/120 s | pas de simulation fixe |

### Mouvement

| Nom | Valeur | Effet |
|---|---|---|
| `HW` | 0,4 m | demi-largeur de la boîte du joueur |
| `H_STAND` | 1,8 m | hauteur debout |
| `H_CROUCH` | 1,1 m | hauteur accroupi |
| `EYE_STAND` | 1,6 m | hauteur des yeux debout |
| `EYE_CROUCH` | 0,95 m | hauteur des yeux accroupi |
| `STEP_H` | 0,55 m | hauteur de marche franchie automatiquement |
| `GRAVITY` | 24 m/s² | gravité |
| `JUMP_V` | 8,4 m/s | vitesse verticale du saut (double saut : ×0,95) |
| `RUN_SPEED` | 9,5 m/s | vitesse de course cible ; seuil des effets de vitesse |
| `CROUCH_SPEED` | 4,5 m/s | vitesse accroupi |
| `GROUND_ACCEL` | 12 | accélération au sol |
| `FRICTION` | 6 | friction au sol |
| `STOPSPEED` | 3 m/s | sous cette vitesse, la friction agit comme si l'on allait à 3 m/s (arrêt net) |
| `AIR_ACCEL` | 14 | accélération en l'air |
| `AIR_CAP` | 1 m/s | plafond de la composante voulue en l'air (rend l'air-strafe possible) |
| `SLIDE_MIN` | 6,5 m/s | vitesse minimale pour démarrer un slide |
| `SLIDE_END` | 3 m/s | le slide s'arrête sous cette vitesse |
| `SLIDE_BOOST` | 4,5 m/s | élan donné au début du slide |
| `SLIDE_FRICTION` | 0,7 | friction en slide |
| `SLIDE_CD` | 1 s | délai entre deux élans de slide |
| `DASH_SPEED` | 18 m/s | vitesse horizontale minimale du dash |
| `DASH_RECHARGE` | 1,6 s | temps de recharge d'une charge de dash (2 charges) |
| `WALLJUMP_PUSH` | 8 m/s | poussée horizontale du wall-jump |
| `WALLJUMP_UP` | 8,8 m/s | vitesse verticale du wall-jump |
| `WALLRUN_MIN` | 6 m/s | vitesse minimale pour courir sur un mur |
| `WALLRUN_MAX` | 1,8 s | durée maximale de wall-run avant de retoucher le sol |
| `BASE_HFOV` | 105° | champ de vision horizontal par défaut (`fovBase`) |

### Grappin

| Nom | Valeur | Effet |
|---|---|---|
| `GRAPPLE_RANGE` | 55 m | portée maximale du crochet |
| `GRAPPLE_SPEED` | 120 m/s | vitesse de vol du crochet |
| `GRAPPLE_PULL` | 70 m/s² | traction vers une ancre de décor |
| `GRAPPLE_MAXSPEED` | 38 m/s | plus de traction au-delà de cette vitesse vers l'ancre |
| `GRAPPLE_CD` | 0,5 s | recharge après avoir lâché |
| `GRAPPLE_JUMP` | 5 m/s | boost vertical quand on lâche en sautant |
| `GRAPPLE_ENEMY_PULL` | 95 m/s² | traction vers un ennemi ou un joueur |
| `GRAPPLE_ENEMY_MAXSPEED` | 46 m/s | vitesse maximale de traction vers une cible |
| `GRAPPLE_STRIKE_DMG` | 50 | dégâts de base de l'impact sur la cible |
| `GRAPPLE_STRIKE_PER_MS` | 3 | dégâts en plus par m/s de vitesse relative à l'impact |

### Railgun, munitions, pièces, roquettes

| Nom | Valeur | Effet |
|---|---|---|
| `RAIL_CD` | 0,85 s | recharge du railgun (divisée par `streak_rate`) |
| `RAIL_AMMO_START` | 5 | munitions au (re)spawn |
| `RAIL_AMMO_MAX` | 10 | munitions maximales |
| `PICKUP_AMMO` | 3 | munitions par cellule |
| `PICKUP_RESPAWN` | 12 s | réapparition d'une cellule |
| `COIN_MAX` | 4 | charges de pièces |
| `COIN_RECHARGE` | 2 s | recharge d'une pièce |
| `COIN_RADIUS` | 0,9 m | rayon de touche d'une pièce |
| `COIN_MULT` | 1,5 | multiplicateur de dégâts par ricochet sur sa propre pièce |
| `COIN_GRAVITY` | 16 m/s² | gravité des pièces |
| `COIN_SEEK_RANGE` | 90 m | portée de recherche de cible du ricochet |
| `RAIL_DMG` | 100 | dégâts du rail |
| `ROCKET_CD` | 0,75 s | recharge des roquettes (divisée par `streak_rate`) |
| `ROCKET_SPEED` | 38 m/s | vitesse des roquettes |
| `ROCKET_DMG` | 110 | dégâts de l'explosion au centre |
| `ROCKET_RADIUS` | 4,5 m | rayon de l'explosion |
| `ROCKET_KNOCK` | 13 m/s | recul de l'explosion (rocket jump) |

### Épée et parry

| Nom | Valeur | Effet |
|---|---|---|
| `MELEE_CD` | 0,45 s | recharge de l'épée |
| `MELEE_RANGE` | 2,4 m | portée de base |
| `MELEE_RANGE_PER_MS` | 0,12 m | portée en plus par m/s de vitesse relative |
| `MELEE_DMG` | 35 | dégâts de base |
| `MELEE_DMG_PER_MS` | 4 | dégâts en plus par m/s de vitesse relative |
| `MELEE_CONE` | 0,55 | cosinus du demi-angle du coup |
| `PARRY_SPEED` | 36 m/s | vitesse d'un projectile renvoyé |
| `PARRY_DMG` | 150 | dégâts directs d'une orbe renvoyée (solo) |
| `PARRY_HEAL` | 10 PV | soin d'un parry réussi |
| `PARRY_WINDOW` | 0,12 s | durée de la fenêtre de parry au début du coup |
| `MELEE_WHIFF_CD` | 0,9 s | recharge d'un coup qui ne touche ni ne pare rien |
| `PARRY_ROCKET_MULT` | 1,25 | multiplicateur de dégâts d'une roquette au premier renvoi |
| `PARRY_PINGPONG` | 1,15 | chaque nouveau renvoi : +15 % de vitesse et de dégâts |
| `PARRY_RANGE_SHRINK` | 0,85 | chaque renvoi réduit la zone de parry de 15 % |
| `PARRY_AIM_COS` | 0,988 | aide à la visée du renvoi (~9°) |
| `PVP_HIT_CAP` | 60 PV | en PvP, une touche ne retire jamais plus de 60 PV |
| `MELEE_PARRIED_CD` | 1 s | épée bloquée pour l'attaquant dont le coup est paré |
| `MELEE_PARRY_GRACE` | 0,15 s | tolérance de latence de l'hôte pour le parry d'épée |

### Chaîne de tir

| Nom | Valeur | Effet |
|---|---|---|
| `STREAK_MAX` | 8 | longueur maximale prise en compte |
| `STREAK_DMG` | 0,15 | +15 % de dégâts par touche successive |
| `STREAK_RATE` | 0,05 | +5 % de cadence par touche successive |

### Audio

| Nom | Valeur | Effet |
|---|---|---|
| `SR` | 44100 | fréquence d'échantillonnage |
| `NBUF` | 4 | nombre de tampons `waveOut` |
| `BUFLEN` | 512 | échantillons par tampon (~11,6 ms) |
| `MAXV` | 32 | voix simultanées |

### Carte et capacités des tableaux

| Nom | Valeur | Effet |
|---|---|---|
| `MAXB` | 256 | boîtes de la carte (`boxes`) |
| `MAP_S` | 1,4 | échelle horizontale de la carte (`ms()`) ; les hauteurs ne changent pas |
| `MAXPICK` | 16 | cellules de recharge (limité aussi par les 16 bits de `pickMask`) |
| `SPEED_ACCEL` / `SPEED_MAX` | 30 m/s² / 28 m/s | bande d'accélération `B_SPEED` (La Boucle) |
| `VOID_CREDIT_T` | 4 s | chute dans le vide après une touche : kill à l'attaquant (§3.12) |
| `VOID_WARN_H` | 8 m | hauteur au-dessus de `killY` où la vignette rouge apparaît |
| `MAXE` | 64 | ennemis |
| `MAXCOIN` | 24 | pièces |
| `MAXPR` | 128 | projectiles |
| `MAXPA` | 4096 | particules |
| `MAXBM` | 32 | rayons |
| `MAXDN` | 32 | chiffres de dégâts |
| `MAXSH` | 256 | éclats |

### Multijoueur

| Nom | Valeur | Effet |
|---|---|---|
| `MAXP` | 8 | joueurs maximum |
| `NET_PORT` | 27960 | port UDP de l'hôte |
| `FRAG_LIMIT` | 15 | frags pour gagner (partie hébergée à la main) ; `FRAG_LIMIT_QUICK` = 7 pour une partie rapide (l'hôte l'envoie dans `PkSnap.fragLimit`) |
| `MATCH_TIME` | 600 s | durée maximale d'un match |
| `RESPAWN_TIME` | 2,5 s | délai de réapparition |
| `PVP_SCALE` | 0,7 | dégâts contre les joueurs = 70 % des dégâts contre les monstres |
| `KILL_HEAL` | 30 PV | soin du tueur |
| `NET_MAGIC` | `0x564C4F43` | signature des paquets (« VLOC ») |
| `NET_VER` | 13 | version du protocole (voir §4) ; 13 = carte dans WELCOME / ANNOUNCE / EV_MATCH, `W_VOID` |
| `MAXSPEC` | 4 | spectateurs en direct sur l'hôte |
| `MAXFOUND` | 12 | parties mémorisées (LAN + en ligne ; 7 affichées) |
| `LOBBY_DEFAULT` | `velocity-lobby.velocity-void.workers.dev` | adresse du lobby déployé (`lobby=` dans `velocity.ini` la remplace) |
| `STUN_HOST` / `STUN_PORT` | `stun.l.google.com` / 19302 | serveur STUN public |
| `OUTQ` | 128 | taille de la file d'événements sortants |

Valeurs codées en dur utiles (pas des `#define`) : PV max 100 ; orbe 12 dégâts à 16 m/s ; kamikaze
18 dégâts ; souffle des roquettes ennemies 30 × f ; changement d'arme 0,22 s ; envoi réseau 60/s ;
délai de déconnexion 5 s ; fin de match → nouveau match après 8 s.

---

## 6. Outils de test

Tous les fichiers passés en argument sont relatifs au **répertoire courant**. Les arguments sont
découpés sur les espaces : utilisez des noms **sans espace ni guillemets** (le dossier du projet
contient un espace, donc lancez depuis le dossier et donnez des noms relatifs).

Depuis Git Bash, la commande attend la fin du programme. Depuis `cmd.exe`, un programme fenêtré rend la
main tout de suite : utilisez `start /wait`.

### `--test fichier.txt` (`run_tests`)

```bash
./build.bat && ./velocity.exe --test resultats.txt && cat resultats.txt
```

- Doit être le **premier** argument. S'exécute **sans fenêtre** ni son, en une fraction de seconde.
- `run_tests` construit la carte et joue des scénarios en appelant directement les fonctions du jeu
  (`player_tick`, `melee`, `fire_rail`, `fire_rocket`, `proj_tick`, `throw_coin`, `coin_tick`,
  `pickup_tick`, `host_handle`, `bot_think` + `game_tick`, `cfg_save`/`cfg_load`…).
- Chaque ligne écrit les valeurs mesurées puis `(attendu ...)` : **la comparaison est à faire à la
  main** (rien ne signale automatiquement un échec).
- Scénarios actuels : escalier, jump pads est et nord, bhop + air-strafe, rocket jump, grappin vers un
  mur, lâcher en vol, grappin sur un drone, épée à 0 et 20 m/s, parry d'une orbe, chaîne de tir,
  recharges, pièces (1 et 2 ricochets), vitesse du grappin, puis un **LAN simulé** sans socket
  (`netMode = NET_HOST`, `botMode = 2`, deux `NP`) : cross-up du bot, épée du bot, coup dans le vide,
  ping-pong de roquette, plafond PvP, parry d'épée, aller-retour de `velocity.ini` (dans
  `cfg_test.ini`, supprimé ensuite), contre-jeu sur une pièce adverse.
- En fin de fichier : analyse d'une réponse STUN, adresses `ip:port`, lignes `room` / `gone` du lobby
  (`lb_line`), places de spectateurs (`MAXSPEC` acceptés, un retour ne prend pas une 2ᵉ place).
- Le lobby a ses propres tests : `cd worker && node test.js` (24 vérifications, port 8799).
- Pendant la partie LAN simulée, `botLog` pointe sur le fichier de résultats : les lignes `netlog`
  (`[   0.96] degats : ...`) s'y mêlent.
- Les scénarios utilisent un **couloir dégagé** autour de x = −20, z entre 12 et 70 : `build_level`
  y interdit les blocs de couverture aléatoires.

### `--shot fichier.bmp` (capture automatique)

```bash
./velocity.exe --shot capture.bmp
```

Ouvre la fenêtre sans l'activer, prépare une scène, la dessine une fois (`render`), enregistre un BMP
24 bits de la taille de la fenêtre (`save_bmp`), et quitte. Le **nom du fichier** choisit la scène
(recherche de sous-chaîne, testée dans cet ordre) :

| Le nom contient | Scène |
|---|---|
| `settings` | menu principal + écran PARAMÈTRES, avec une case en attente de touche (pièce, 2ᵉ touche) |
| `menu` | menu principal |
| `dm` | scène de deathmatch factice : joueurs « Rival » (grappin, épée) et « Ninja », fil des kills, HUD LAN |
| *(autre)* | combat solo scripté : vague 3, 8 ennemis, tirs rail et roquette, grappin sur la tour, HUD des deux axes |
| `rail` (avec la scène par défaut) | la même, arme en main = railgun |
| `impact` | kill au rail à pleine vitesse, capturé pendant l'impact frame |
| `fx` | idem, 14 ticks plus tard sans l'impact frame (éclats, chiffres de dégâts, lignes de vitesse) |

Options après le nom : `--map N` (carte, §3.12) et `--yaw A` (orientation). Sur une carte autre que l'Arène
(hors `menu`/`settings`/`dm`), la scène est une vue de la carte depuis la caméra du menu ; le nom peut contenir
`spawn` (depuis le premier point de départ), `chute` (3 m au-dessus de `killY` en tombant : plan rouge et
vignette) ou `solo` (avec 8 drones). Exemple : `velocity.exe --shot puits.bmp --map 1 --yaw 0.8`.

Pas de son, `velocity.ini` n'est ni lu ni écrit.

### Mode bot : `--bot host|join`

```
velocity.exe --bot host|join [--name N] [--time S] [--log fichier.txt] [--botshot fichier.bmp] [--map N]
```

- `--bot host` → `botMode = 1` : héberge automatiquement (`ACT_HOST`). Tout autre mot → `botMode = 2` :
  cherche (`ACT_BROWSE`) puis se connecte à la première partie trouvée (`bot_think`).
- `--name` : pseudo (sinon le nom d'utilisateur Windows). `--time` : durée en secondes avant de quitter
  (20 par défaut). `--log` : journal (`netlog`). `--botshot` : capture BMP à la fin. `--map N` : carte hébergée
  (§3.12 ; le client reçoit celle de l'hôte). Sur une carte à vide, le résumé ajoute les chutes (« carte … : chutes
  dans le vide », et côté hôte « vide (arbitre) : … chutes, … creditees a l'attaquant, … suicides »).
- Fenêtres réduites à 800 × 450, l'hôte à gauche, le client à droite ; souris jamais capturée ;
  `velocity.ini` jamais lu ni écrit.
- `bot_think` joue « comme un humain » : bhop permanent, air-strafe en balançant la vue, cross-up
  (saut, double saut, dash par-dessus, tir dans le dos), dash et grappin pour réduire la distance,
  recherche de cellules quand le rail est vide, pièces + tir dedans à pleine vitesse, contre-tir sur les
  pièces adverses (une sur deux), parry des roquettes (60 %) et des épées (50 %), épée au contact,
  sortie des coins où il est coincé. Il a un temps de réaction et une imprécision qui grandit avec la
  distance.
- Le journal contient l'état du bot chaque seconde, chaque dégât arbitré, kill, parry, ricochet, puis un
  **résumé** : frags/morts de chacun, paquets reçus/envoyés, vitesse moyenne et maximale, et côté hôte
  les statistiques d'équilibrage (temps pour tuer moyen, one-shots, parries, épées parées, chocs,
  touches par arme).

Options réseau en ligne : `--lobby hote[:port]` (adresse du lobby), `--lanep` (adresses locales à la place de
STUN : tout sur une machine), `--nolan` (pas de broadcast, pour que seule la voie du lobby soit testée),
`--spec` (le bot client regarde la partie au lieu d'y jouer).

### Match bot via le lobby local

```bash
cd worker && node dev-server.js 8787 &        # lobby de test
./velocity.exe --bot host --name Hote --time 40 --log host.txt --lobby 127.0.0.1:8787 --lanep --nolan &
sleep 3; ./velocity.exe --bot join --name Client --time 35 --log client.txt --lobby 127.0.0.1:8787 --lanep --nolan &
sleep 3; ./velocity.exe --bot join --spec --name Spec --time 30 --log spec.txt --lobby 127.0.0.1:8787 --lanep --nolan; wait
```

Les journaux montrent `lobby >> host …`, `lobby << room …`, `lobby >> join …`, `lobby << go …`, `punch`, puis
`connecte a la partie` ; le spectateur reçoit `id 8` (= `MAXP`) et le journal de l'hôte note `spectateur 0 arrive`.
Arrêtez ensuite le lobby de test (pas de `taskkill` global sur `node`).

**Match classé** (même lobby de test, qui sert aussi `/ranked/…` sur une base en mémoire) : ajouter `--ranked` aux
deux bots (identité temporaire, rien n'est sauvegardé) ; `--frags N` (bots seulement) abaisse la limite de frags
pour voir plusieurs matchs complets en 90 s :

```bash
F="--ranked --lobby 127.0.0.1:8787 --lanep --nolan --frags 2"
./velocity.exe --bot host --name Hote --time 90 --log host.txt $F &
sleep 2; ./velocity.exe --bot join --name Client --time 86 --log client.txt $F; wait
```

Les journaux montrent `classe : rapport de fin de match …`, `classe (serveur) : cote 1000 -> 1032 (+32) …` à chaque
match, l'abandon du client à 86 s (`Client quitte le match en cours`, l'hôte gagne), puis
`classe : cote finale (serveur) …, variation …` : les variations de l'hôte et du client doivent s'annuler.

### Match bot contre bot sur une seule machine

L'hôte écoute sur le port 27960, le client utilise un port libre et interroge aussi `127.0.0.1` : deux
instances sur le même PC se trouvent toutes seules. Donnez à l'hôte un peu plus de temps que le client
(sinon le client voit « l'hôte a fermé la partie »).

Git Bash, depuis le dossier du projet :

```bash
./velocity.exe --bot host --name Hote --time 65 --log bot_hote.txt & sleep 1; ./velocity.exe --bot join --name Client --time 60 --log bot_client.txt; wait
```

`cmd.exe` :

```
start "" velocity.exe --bot host --name Hote --time 65 --log bot_hote.txt
start "" velocity.exe --bot join --name Client --time 60 --log bot_client.txt
```

Puis lisez la fin de `bot_hote.txt` (résumé et statistiques d'équilibrage) et `bot_client.txt`.
Le résumé de chaque journal ne liste que les joueurs encore connectés à la fin : comme le client part
avant, le résumé de l'hôte ne montre que lui ; les frags du client sont dans `bot_client.txt`.
Le pare-feu Windows peut demander d'autoriser `velocity.exe` la première fois.

---

## 7. Recettes pas à pas

Après chaque recette : `build.bat` sans avertissement nouveau, `--test` (toutes les lignes conformes),
`--shot` pour voir le résultat, et un match de bots si le réseau est touché.

### 7.1 Ajouter une arme

Exemple : une troisième arme principale (`P.weapon == 2`).

1. **Réglages** — ajoutez ses `#define` (dégâts, recharge, vitesse…) dans la section « Reglages du
   mouvement », à côté de `RAIL_*`/`ROCKET_*`, avec un commentaire court sans accents.
2. **Tir** — écrivez `fire_xxx()` dans la section Combat :
   - hitscan : partez de `fire_rail` (`ray_world`, `ray_sphere` sur `E`, `ray_box` sur `np_aabb` des
     joueurs `np_target`) ;
   - projectile : partez de `fire_rocket` et `spawn_proj`, ajoutez un type `PR_*` à l'enum
     (`PR_ROCKET`, `PR_ORB`, `PR_PARRIED`), sa branche dans `proj_tick`, et son dessin dans `render`
     (boucle des billboards sur `PR`).
   - Réglez `P.fireCd`, `P.recoil`, `shake`, `muzzleT`, un `play(S_...)`, et appelez `shot_result` si
     l'arme doit compter dans la chaîne de tir.
3. **Dégâts** — ennemis : `damage_enemy` ou `explode`. Joueurs LAN : `net_hit(i, dmg * PVP_SCALE, recul, W_XXX)`.
   Ajoutez `W_XXX` **à la fin** de l'enum `W_*`, son nom à la fin de `weaponNames`, et passez de 7 à 8
   partout où la taille est écrite en dur : `hitCount[7]`, `hitCount[ev->sub % 7]` dans `host_handle`,
   `weaponNames[f->kind % 7]` dans `draw_killfeed`, et la ligne « touches arbitrees » du résumé de
   `WinMain`.
4. **Sélection** — dans `game_tick` : la sélection lit `actP[A_WEAP1]`, `actP[A_WEAP2]`, `actP[A_SWAP]`
   et fait `nw = 1 - P.weapon` pour changer d'arme. Avec trois armes, remplacez par un cycle
   (`(P.weapon + 1) % 3`) et ajoutez une action `A_WEAP3` (recette 7.3). Ajoutez la branche de tir à
   côté de `fire_rocket`/`fire_rail`.
5. **Ce que voient les autres** — rail : réutilisez `net_fx_rail` pour le rayon ; sinon un nouvel
   événement (recette 7.2). L'arme tenue est déjà transmise (`NetPState.weapon`) : complétez la couleur
   dans `draw_remote_players`.
6. **Affichage** — modèle en main dans `draw_viewmodel` (branche sur `P.weapon`, flash du canon), nom,
   munitions et recharge dans `draw_hud` (`wnBuf`, `cdMax`), astuce dans `draw_controls` si utile.
7. **Son** — ajoutez `S_XXX` avant `S_COUNT` et sa synthèse dans `make_sounds`
   (`b = sbuf(durée, &n); ... sounds[S_XXX] = mksound(b, n);`).
8. **Bot** — `bot_think` choisit l'arme (`P.weapon = ...`, ligne « le bot client alterne rail /
   roquettes ») : apprenez-lui quand utiliser la nouvelle.
9. **Test** — un scénario dans `run_tests` (recette 7.6). **`NET_VER`** + 1 si un paquet ou un événement
   change.

### 7.2 Ajouter un événement réseau

1. Ajoutez `EV_XXX` **à la fin** de l'enum `EV_*` (après `EV_MELEE_PARRY`) pour ne pas décaler les
   valeurs existantes.
2. Choisissez l'usage des champs de `NetEvent` : `netId` (objet suivi), `target`, `sub`, `actor`
   (octets), `f[0..6]` (flottants ; `vf`/`fv` pour copier un `V3`). S'il faut plus de place, agrandissez
   `NetEvent` (tous les paquets d'événement grossissent).
3. Écrivez un émetteur dans la sous-partie « evenements » de la section réseau, sur le modèle de
   `net_fx_melee` :
   ```c
   static void net_fx_xxx(V3 pos, int k) {
       NetEvent e = ev_new(EV_XXX);
       vf(e.f, pos); e.sub = (unsigned char)k;
       net_emit(&e);
   }
   ```
   S'il est appelé depuis une section placée **avant** le réseau (Joueur, Combat), ajoutez sa
   déclaration anticipée à côté de celles de `net_fx_rail` etc. (fin de la section « etat partage »).
4. Appelez-le là où l'action se produit, **seulement en LAN** (`if (dmMode) net_fx_xxx(...)` ; de toute
   façon `net_emit` ne fait rien hors réseau), et produisez l'effet local vous-même : l'émetteur ne
   reçoit pas son propre événement.
5. Ajoutez un `case EV_XXX:` dans `apply_event` pour l'effet chez les autres. Utilisez `ev->from` pour
   savoir qui l'envoie, `atten(pos)` pour le volume des sons.
6. Si l'hôte doit **arbitrer** (PV, objets partagés, décisions), ajoutez une branche dans `host_handle`.
   Une décision prise par l'hôte se diffuse par `net_emit(&e); apply_event(&e);` (modèle :
   `host_take_pickup`).
7. Incrémentez **`NET_VER`**.
8. Vérifiez par un match de bots (ajoutez un `netlog` dans `apply_event` pour voir l'événement arriver)
   et, si possible, dans la partie « LAN simulé » de `run_tests`.

### 7.3 Ajouter une action réaffectable

1. Ajoutez `A_XXX` dans l'enum `A_*`, juste **avant `A_COUNT`**.
2. Ajoutez, **à la même position**, une entrée dans les trois tableaux indexés par l'action :
   - `actId` : identifiant pour `velocity.ini` (minuscules, sans espace ni accent, unique) ;
   - `actLabel` : texte affiché dans PARAMÈTRES et l'aide (sans accents : la police bitmap est en ANSI) ;
   - `bindDefault` : `{ touche1, touche2 }` en scancodes (ex. `0x13` = R) ou codes souris `K_*`, `0` pour
     aucune. Vérifiez qu'aucune autre action n'utilise déjà ces touches par défaut.
   Les fichiers `velocity.ini` existants restent valides (lus par nom via `actId`) ; la nouvelle action
   prend sa valeur par défaut.
3. Lisez-la dans `game_tick` (ou une fonction appelée par lui) : `actP[A_XXX]` pour un appui,
   `act[A_XXX]` pour un maintien. Ne lisez jamais `keys[]` directement pour une commande de jeu.
4. Mise en page : `draw_settings` affiche une ligne de 36 px par action et `draw_controls` une ligne de
   21 px ; vérifiez que tout tient avec `--shot settings.bmp` et `--shot menu.bmp`.
5. Bot (facultatif) : `botPress[A_XXX] = 1` ou `botHeld[A_XXX] = 1` dans `bot_think`.
6. Test : étendez le scénario « reglages » de `run_tests` si la sauvegarde est concernée.

### 7.4 Ajouter un réglage

1. Déclarez la variable avec sa valeur par défaut près de `fovBase`, `invertY`, `impactFrames`
   (section Entrées, bloc « reglages »).
2. `cfg_save` : ajoutez un `fprintf(f, "cle=%d" "\n", var);`.
3. `cfg_load` : ajoutez un `else if (!strcmp(line, "cle")) var = ...;` en **bornant** la valeur
   (`clampf`, `!= 0`). Les clés inconnues sont ignorées, donc l'ordre n'a pas d'importance.
4. `draw_settings` : un bouton bascule (`if (button(lx, y, lw, 46, buf)) var = !var;`) ou un curseur
   (`var = slider(id, lx, y, lw, "Libelle", buf, var, min, max);` avec un `id` unique : 1 à 3 sont
   pris). Avancez `y` ; vérifiez que le bouton RETOUR reste à l'écran (`--shot settings.bmp`).
5. Utilisez la variable dans le code concerné.
6. Test : ajoutez-la au scénario « reglages » de `run_tests` (modifier, `cfg_save`, remettre la valeur
   par défaut, `cfg_load`, afficher, puis restaurer).

### 7.5 Ajouter un élément de carte

Pour une **nouvelle carte** : une fonction `build_map_xxx()`, sa table de départs, une ligne dans `maps[]` (§3.12),
avec les outils `addpad_to`, `addspeed`, `addanchor`, `addcell` (coordonnées du monde), puis `--test` : la ligne
« carte … » vérifie les limites, que chaque départ est libre, au sol et au-dessus du vide, qu'aucune cellule n'est
dans un solide et que chaque pad retombe sur un sol. Pour **modifier l'Arène** :

1. Dans `build_map_arene`, ajoutez :
   - un bloc : `addbox(x0, y0, z0, x1, y1, z1, r, g, b)` — passez les coordonnées **horizontales** par
     `ms()` pour suivre l'échelle `MAP_S`, pas les hauteurs ;
   - un jump pad : `addpad(x, z, vitesse_de_lancement)` (coordonnées déjà mises à l'échelle) ;
   - une cellule de recharge : `addpickup(x, y, z)` — coordonnées **non** mises à l'échelle
     (`addpickup` applique `ms()` lui-même), `y` = hauteur du sol sous la cellule.
2. Respectez les limites : `MAXB` boîtes, `MAXPICK` cellules (16 au plus à cause de `pickMask`).
   L'ordre des `addpickup` fixe les index réseau des cellules.
3. Blocs de couverture aléatoires (fin de `build_map_arene`) : ajoutez une exclusion (`near2(...)` ou un
   test de zone) pour qu'aucun bloc n'apparaisse dans votre élément, et **gardez dégagé le couloir de
   test** (x ≈ −20, z de 12 à 70). Ne changez pas la graine `rng = 777` sans raison : la carte doit être
   identique chez tous les joueurs.
4. Rendu : la liste d'affichage `worldList` est compilée **une fois** par `build_world_list` après
   `build_level` dans `WinMain` ; un nouveau bloc y apparaît automatiquement. Si vous modifiez la
   carte en cours de partie, il faut reconstruire cette liste.
5. Nouveau **type** de boîte : ajoutez une valeur à `B_SOLID`/`B_PAD`/`B_SPEED`, réglez `boxes[nb - 1].kind`
   après `addbox` (modèle : `addpad`), codez son comportement dans `player_tick` (`P.groundBox` donne
   la boîte sous les pieds) et son aspect dans `build_world_list` ou `render`.
6. Pensez aux points d'apparition LAN (`areneSpawns`), au départ solo, à la caméra du menu et aux zones
   d'apparition des ennemis : ce sont des champs de `maps[0]`.
7. Incrémentez `NET_VER` (une carte différente fausserait les cellules et les collisions entre
   versions). Vérifiez avec `--shot` et `--test` (escalier, jump pads et grappin dépendent de la
   géométrie).

### 7.6 Ajouter un test

1. Dans `run_tests`, ajoutez un bloc avant `botLog = NULL;` (fin de fonction). Si votre test est solo,
   placez-le **avant** le commentaire « LAN simule », ou remettez vous-même `dmMode = 0`,
   `netMode = NET_OFF`, `botMode = 0` (ces variables ne sont pas remises à zéro par `reset_game`).
2. Squelette :
   ```c
   /* description courte sans accents */
   reset_game();
   P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
   E[0].alive = 1; E[0].type = EN_DRONE; E[0].hp = 100; E[0].pos = v3(-20, 1.6f, 30);
   for (int i = 0; i < 120; i++) player_tick(TICK, 0, 0);
   fprintf(f, "mon test : valeur=%.1f (attendu ...)\n", mesure);
   ```
3. Pour piloter les entrées :
   - avec `player_tick` directement : écrivez `act[A_*]` (et remettez à 0 avec `memset(act, 0, sizeof act)`) ;
     grappin : `hookPressIn`/`hookHeldIn` ;
   - avec `game_tick` : écrivez `botHeld[A_*]`/`botPress[A_*]` (car `input_poll` réécrit `act`).
4. Pièges : `game_tick` ne fait rien tant que `hitstop > 0` (mettez `hitstop = 0` après un coup) ; un
   parry ou un coup laisse `P.parryWin` et `P.meleeCd` actifs ; `nowT` n'avance pas tout seul (faites
   `nowT += TICK` si l'hôte compare des temps, comme dans le test de parry d'épée).
5. Restez dans le couloir dégagé (x = −20) pour ne pas dépendre des blocs aléatoires.
6. Écrivez toujours la valeur attendue entre parenthèses `(attendu ...)` pour pouvoir relire le fichier
   d'un coup d'œil, puis lancez `velocity.exe --test resultats.txt`.

---

## 7. Port macOS

Même `main.c`, compilé avec `clang` (pas de MSVC). Tout ce qui est propre à la plateforme est isolé :

| Fichier | Rôle |
|---|---|
| `mac_platform.h` | API C de la couche plateforme (`mac_*`) et rappels fournis par `main.c` (`plat_*`) |
| `mac_platform.m` | Objective-C (ARC) : fenêtre Cocoa + `NSOpenGLContext` legacy, clavier/souris, `AudioQueue`, police `CoreText`, WebSocket `NSURLSession` |
| `mac_compat.h` | Shims Win32 pour le code partagé : sockets BSD (`SOCKET`, `closesocket`…), `CRITICAL_SECTION`/événements/fils en pthread, `QueryPerformanceCounter`, en-têtes BMP |

- **Build** : `./build_mac.sh` sur un Mac (Command Line Tools) → `Velocity.app` universel (arm64 + x86_64, ad hoc)
  et `Velocity-macOS.zip`. Sans Mac : le workflow `.github/workflows/macos.yml` fait la même chose sur un runner
  macOS, lance `--test` et publie le zip en artefact. Au premier lancement d'un `.app` téléchargé : clic droit →
  Ouvrir (ou `xattr -dr com.apple.quarantine Velocity.app`).
- **`main.c`** : les blocs Windows sont sous `#ifdef _WIN32`. Les entrées passent par des gestionnaires communs
  (`in_key_down`, `in_char`, `in_button_down`, `in_wheel`, `in_resize`…) appelés par `wndproc` (Windows) ou par
  les `plat_*` (macOS). `WinMain` devient `velocity_main(cmd)` ; `main(argc, argv)` recolle les arguments.
  Le fil principal reste le seul à lire/écrire l'état du jeu, y compris l'audio : `AudioQueue` ne fait que
  signaler « tampon libre », `audio_update()` le remplit.
- **Touches** : le jeu stocke des **scancodes Windows** (set 1) dans `velocity.ini`. `mac_platform.m` convertit les
  touches virtuelles macOS (`keyMap`) ; les noms affichés viennent de la disposition clavier courante (`UCKeyTranslate`).
- **Souris** : `CGAssociateMouseAndMouseCursorPosition(false)` + `[NSCursor hide]`, deltas bruts via `deltaX/deltaY`.
- **Fenêtre** : surface OpenGL non Retina (1 point = 1 pixel, comme `winW`/`winH`) ; plein écran = fenêtre sans bordure
  (F11 comme sous Windows) ; `Cmd+Q` quitte.
- **Réglages** : `~/Library/Application Support/Velocity/velocity.ini` (le `.app` peut être en lecture seule).
- **Lobby en ligne** : `mac_ws_open/send/recv/close/free` (bloquants, appelés depuis les fils `lb_reader`/`lb_writer`).
  `mac_ws_close` coupe la connexion ; `mac_ws_free` n'est appelé qu'après l'arrêt des fils (`lb_stop`).
- **Bot adverse** (`ACT_VSBOT`) : `posix_spawn` du même exécutable (`_NSGetExecutablePath`).
- **Statut** : écrit sans pouvoir compiler ni lancer sur un Mac (développé sous Windows). À valider sur machine réelle :
  clavier/souris, son, texte, plein écran, lobby, partie contre bot.
