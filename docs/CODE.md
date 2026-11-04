# VELOCITY — documentation du code

Ce document décrit le code du jeu (`main.c` et les morceaux de `src/`) pour qu'un humain ou une IA puisse le
modifier sans tout relire. Le code est cité par **noms** de fonctions, de structures, de variables et de `#define` :
cherchez-les avec `grep -rn nom src/` (les numéros de ligne changent trop souvent pour être utiles). Le §2 dit dans
quel fichier se trouve chaque partie.

> Les commentaires du code sont en français **sans accents**. Gardez cette convention.

---

## 1. Vue d'ensemble

### Une seule unité de compilation, aucune dépendance

- Tout le jeu forme **une seule unité de compilation** : `main.c` inclut, dans l'ordre, les morceaux de `src/`
  (voir §2). On ne compile que `main.c`. Fenêtre Win32, rendu OpenGL 1.1 (pipeline fixe,
  `glBegin`/`glEnd`, listes d'affichage), audio `waveOut` avec sons **synthétisés** au démarrage,
  réseau UDP Winsock pour le deathmatch LAN.
- Aucune ressource externe : pas d'image, pas de son, pas de police à charger (la police vient de
  `wglUseFontBitmapsA` sur « Consolas », les textures sont générées dans `make_textures` ; les `.bmp` de
  `graphismes/` ne font que les remplacer, §3.13).
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
   quitte, **sans fenêtre**), `--export-textures dossier` (écrit les textures générées en `.bmp` et quitte, sans
   fenêtre ni OpenGL), `--shot`, et les options du mode bot (`--bot`, `--name`, `--time`,
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
  ouvrir le menu met en pause ; en LAN et en ligne, **jamais de pause** :
  - le menu ECHAP laisse la partie tourner ;
  - une image de la boucle est la fonction `main_frame()` (`src/plateforme/demarrage.c`), appelée par la boucle de
    `velocity_main` **et** pendant les boucles modales du système, qui sinon bloqueraient tout (et l'hôte couperait
    les joueurs après `NET_TIMEOUT`) : fenêtre déplacée ou redimensionnée, menu de fenêtre. Windows :
    `WM_ENTERSIZEMOVE` / `WM_ENTERMENULOOP` lancent un minuteur de 8 ms, `WM_TIMER` → `modal_frame()` ; macOS : un
    `NSTimer` dans `NSEventTrackingRunLoopMode` → `plat_modal_frame()` ;
  - macOS : `beginActivityWithOptions` (`NSActivityLatencyCritical`) empêche App Nap de ralentir le jeu ;
  - le hitstop ne fige pas la simulation (ci-dessous).
- `render(alpha, frameDt)` interpole **la position du joueur** entre `P.prevPos` et `P.pos` avec
  `alpha = acc / TICK`. Le reste (ennemis, particules) est dessiné à sa position du dernier tick.
- **Hitstop** : si `hitstop > 0`, `game_tick` décrémente `hitstop` et, **en solo seulement**, sort
  immédiatement — le monde se fige quelques centièmes de seconde sur les gros coups (les entrées attendent). En
  LAN et en ligne (`dmMode`), la simulation continue : seuls l'impact frame et les effets restent.

### États de l'application

| Variable | Valeurs | Rôle |
|---|---|---|
| `appState` | `APP_MENU`, `APP_JOIN`, `APP_GAME` | menu principal / recherche de parties LAN / en jeu |
| `settingsOpen` | 0 / 1 | écran PARAMÈTRES par-dessus le menu principal ou la pause |
| `settingsPage` | 0 / 1 | dans PARAMÈTRES : 0 = réglages et commandes, 1 = écran GRAPHISMES (Échap revient d'un écran à la fois) |
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

## 2. Arborescence des fichiers

`main.c` est la **table des matières** : une liste de `#include "src/..."` dans l'ordre. Les morceaux ne se
compilent pas seuls (ils voient tout ce qui est inclus avant eux, comme si c'était un seul grand fichier) et chacun
commence par une ligne qui dit son rôle. La sortie du préprocesseur est la même qu'avec l'ancien `main.c` unique.

```
velocity/
├── main.c                     table des matieres : inclut src/ dans l'ordre (seul fichier compile)
├── build.bat / build_mac.sh   compilation Windows (MSVC) / macOS (clang, Velocity.app)
├── build_music.bat            outil d'ecoute de la musique (outils/musique.c)
├── src/
│   ├── noyau/        entete.c (includes), maths.c (V3, aleatoire)
│   ├── reglages/     equilibrage.c : TOUS les #define (mouvement, armes, parry, PvP, points)
│   ├── son/          audio.c (mixeur, sons synthetises), music.h (morceaux et sequenceur)
│   ├── monde/        carte.c (boites AABB, cartes MapDef et leurs objectifs, collisions, rayons),
│   │                 entites.c (Player P, Enemy, Proj...)
│   ├── entrees/      actions.c (touches, A_*, bindDefault)
│   ├── effets/       combat_fx.c (impact frames, input_poll), particules.c, camera_mort.c, monde_fx.c (neons, meteo)
│   ├── joueur/       mouvement.c (player_tick, grappin)
│   ├── combat/       degats.c, loadout.c, melee.c, pieces_rail.c, roquettes.c, projectiles.c, competences.c
│   ├── armes/        une arme = un fichier : enclume, lance, fleau, poings, egide, elan, eventail, tesla, scie, ventouse
│   ├── solo/         mode VAGUES : ennemis_vagues.c (ennemis), boss.c, ameliorations.c (boutique), vagues.c
│   ├── reseau/       etat_partage.c, protocole.c (paquets, EV_*), hote.c, modes.c, client.c, en_ligne.c (lobby, STUN),
│   │                 classe.c (mode classe : Elo, paliers, identite), classe_match.c (suivi du match, rapport),
│   │                 classe_serveur.c (requetes au serveur de classement), speedrun_serveur.c (classements de temps)
│   ├── bots/         navigation.c (graphe de la carte, plus court chemin), bot.c
│   ├── jeu/          style.c (jauge D a S), course.c (mode COURSE), speedrun.c (classements, speedrun complet),
│   │                 kill_zone.c (void_check, bots prudents),
│   │                 boucle.c (pickup_tick, game_tick)
│   ├── graphismes/   rendu_ini.c (lecture de graphismes/rendu.ini, ambiances, qualite)
│   ├── perso/        skins.c (modeles et animation des personnages)
│   ├── rendu/        base, textures, eclairage (lightmaps), modeles, armes_rendu, viewmodel, joueurs,
│   │                 ciel_ombres_bloom, render.c (l'image complete)
│   ├── interface/    texte.c, mise_a_jour.c, config.c (velocity.ini), menus.c, scores.c, hud.c, carte_ecran.c
│   ├── plateforme/   http.c (requetes HTTP : WinHTTP / curl), fenetre.c (Win32 + entrees communes), second_ecran.c,
│   │                 demarrage.c (WinMain, boucle, options de la ligne de commande)
│   │   └── mac/      mac_platform.m / .h, mac_compat.h (port macOS, voir §8)
│   └── tests/        tests.c (--shot, --test)
├── graphismes/       rendu.ini, ambiances, textures .bmp : modifiables sans recompiler (F5)
├── outils/           musique.c (ecoute du design musical, Windows)
├── worker/           lobby en ligne (Cloudflare Worker, Node)
└── docs/             cette doc, plans, fiches des armes (docs/armes/)
```

| Fichier | Rôle | Éléments clés |
|---|---|---|
| `noyau/entete.c` | includes, `#pragma comment(lib)` | `PI`, `TICK` |
| `noyau/maths.c` | vecteurs 3D et aléatoire | `V3`, `v3`, `vadd`, `vsub`, `vmul`, `vdot`, `vcross`, `vlen`, `vnorm`, `vlerp`, `clampf`, `hlen`, `rng`, `frand`, `frange`, `rand_dir` |
| `reglages/equilibrage.c` | presque tous les `#define` d'équilibrage (voir §5) | mouvement, grappin, armes (une sous-partie par arme), parry, PvP, points, molette |
| `son/audio.c` | mixeur + synthèse | `S_*`, `Sound`, `Voice`, `play`, `audio_fill`, `audio_update`, `make_sounds`, `audio_init` ; inclut `music.h` |
| `monde/carte.c` | cartes, objectifs des modes, collisions, rayons | `Box`, `boxes`, `addbox`, `addpad`, `addpad_to`, `addspeed`, `addanchor`, `Pickup`, `build_map_*`, `MapDef`, `maps[]`, `*Zones`, `*Flags`, `*Shield`, `*Team`, `curMap`, `map_pvp`, `map_step`, `map_obj_scale`, `build_level(m)`, `mapBox`, `ray_world`, `los`, `sphere_pushout` |
| `monde/entites.c` | structures de jeu et état solo | `Player` (`P`), `Enemy` (`E`), `Coin` (`CO`), `Proj` (`PR`), `Part`, `Beam`, `hitstop`, `msg` |
| `reseau/etat_partage.c` | ce que le jeu doit connaître du réseau avant le réseau | `MAXP`, `NetPState`, `NetPlayer` (`NP`), `myId`, équipes, déclarations anticipées `net_hit`, `net_fx_*` |
| `entrees/actions.c` | touches physiques et actions | `SC_*`, `K_*`, `keys`, `latch`, `A_*`, `actId`, `actLabel`, `bindDefault`, `binds`, `act`, `actP` |
| `effets/combat_fx.c` | impact frames, chiffres de dégâts, éclats **et lecture des entrées** | `impact_frame`, `DmgNum`, `Shard`, `input_poll`, `set_capture` |
| `effets/particules.c` | particules, rayons | `add_part`, `burst`, `add_beam`, `add_light`, `decal_at`, `fx_explosion` |
| `joueur/mouvement.c` | physique du joueur et grappin | `view_dir`, `eye_pos`, `move_axis`, `wall_check`, `accelerate`, `grapple_tick`, `player_tick` |
| `effets/camera_mort.c` | caméra de mort (§3.16) | `killcam_start`, `killcam_active`, `killcam_view` |
| `combat/degats.c` | dégâts, kills, explosions | `damage_player`, `kill_enemy`, `damage_enemy`, `explode_ex`, `streak_mult`, `parry_feedback` |
| `combat/loadout.c` | deux armes, munitions à jauge | `loadout`, `weapon_select`, `wp_has_ammo` |
| `combat/melee.c` | épée et aiguillage des mêlées | `melee` (`try_parry` est dans `loadout.c`) |
| `combat/pieces_rail.c` | pièces, ricochets, rail | `throw_coin`, `coin_tick`, `ricochet`, `hitscan_shot`, `fire_rail` |
| `combat/roquettes.c`, `combat/projectiles.c` | projectiles | `spawn_proj`, `fire_rocket`, `proj_tick` |
| `armes/*.c` | une arme par fichier (logique ; fiche de conception dans `docs/armes/`) | `slam_*`, `lance_*`, `flail_*`, `fist_*`, `shield_*`, `kin_*`, `sg_*`, `tesla_*`, `saw_*`, `mine_*` |
| `solo/ennemis_vagues.c` | mode VAGUES : ennemis (§3.5) | `spawn_enemy_at`, `spawn_enemy_type`, `enemy_tick`, `reset_game` |
| `solo/boss.c` | mode VAGUES : les quatre boss | `boss_spawn`, `boss_tick`, `boss_filter`, `boss_fx_tick`, `GW`, `LZ` |
| `solo/ameliorations.c` | mode VAGUES : boutique de comportements | `fxDef`, `upPath`, `vgLvl`, `up_buy`, `vg_on_hit`, `vg_on_kill`, `vg_fx_tick` |
| `solo/vagues.c` | mode VAGUES : progression | `wave_tick`, `vg_start`, `vg_wave_start`, `vg_choose`, `vagues_temps_100` |
| `rendu/vagues_rendu.c` | mode VAGUES : rendu | `vg_draw_enemy`, `vg_draw_world_fx` |
| `interface/vagues_hud.c` | mode VAGUES : HUD et écrans | `vg_draw_info`, `vg_draw_hud`, `vg_draw_shop`, `vg_draw_select` |
| `tests/vagues_tests.c` | mode VAGUES : tests et captures | `vagues_tests`, `vagues_shot` |
| `reseau/horloge.c` | horloge de match commune | `match_clock_start` / `match_clock_tick` (hôte, temps réel), `clk_srv_sample` (heure du worker), `clk_match_from_snap` (client) |
| `reseau/protocole.c` | paquets et événements | `NET_VER`, `PK_*`, `EV_*`, `net_emit`, `net_hit`, `net_fx_*`, `apply_event` |
| `reseau/hote.c` | autorité de l'hôte | `host_handle`, `host_kill`, `host_tick`, `pick_spawn`, `map_rotate_next`, `host_start_match` |
| `reseau/client.c` | client | `on_welcome`, `apply_snap`, `client_recv`, `net_update`, `net_connect` |
| `reseau/en_ligne.c` | lobby et STUN (voir §4) | `stun_*`, `lb_*`, `join_found`, `quick_play`, `ranked_play` |
| `reseau/classe.c` | mode classé (§4 « Mode classé ») : état, formule Elo, paliers, identité, cartes et règles imposées | `rank_delta`, `rank_tier`, `rank_div`, `rank_label`, `rank_col`, `rk_identity`, `rk_new_match`, `rk_map_ok`, `rk_map_pick`, `rk_counts` |
| `reseau/classe_match.c` | mode classé : suivi du match et rapport de fin | `rk_send_report`, `rk_tick`, `rk_host_gone`, `rk_shutdown` |
| `reseau/classe_serveur.c` | mode classé : file de requêtes au serveur de classement (fil de fond), lecture des réponses JSON | `rk_http`, `rk_thread`, `rk_push`, `rq_hello`, `rq_top`, `rk_on_hello`, `rk_on_report`, `rk_net_poll`, `rk_bot_final` |
| `reseau/speedrun_serveur.c` | speedrun (§4 « Speedrun ») : ticket, rapport de temps, classements | `speedrun_begin`, `speedrun_report`, `speedrun_report_splits`, `rq_sr_top`, `rq_sr_top_all`, `sr_parse_top`, `sr_on_response` |
| `plateforme/http.c` | requête HTTP générique, bloquante (WinHTTP ; macOS : `/usr/bin/curl`) | `http_request` |
| `bots/navigation.c` | navigation des bots (§3.19) | `nav_build`, `nav_path`, `nav_plan`, `nav_follow`, `NV`, `navBusy`, `navLanding` |
| `bots/bot.c` | bots | `aim_assist`, `bot_think` |
| `combat/competences.c` | compétences (touche `A_SKILL`) | `skill_mark`, `skill_allin`, `skill_decoy`, `skill_tick` |
| `jeu/kill_zone.c` | chute sous `killY` | `void_check`, `bot_lands`, `bot_void_guard` |
| `jeu/boucle.c` | un pas de simulation | `pickup_tick`, `spec_tick`, `fire_weapon`, `game_tick`, `fx_tick` |
| `graphismes/rendu_ini.c` | `graphismes/rendu.ini` (voir §3.13) | `gx*`, `gfxKeys`, `gfx_load_all`, `gfx_quality_apply` |
| `rendu/textures.c` | textures, primitives | `make_textures`, `tex_override`, `tex_generate` (`tex_gen_*`), `save_bmp_rgb`, `export_textures`, `quadf`, `box_faces`, `box_edges` |
| `perso/skins.c` | personnages (voir §3.14) | `SkinAnim`, `skin_robot`…, `draw_skin` |
| `rendu/eclairage.c` | lightmaps, néons, décor | `bake_lightmaps`, `build_neon`, `build_world_list` |
| `interface/texte.c` | police et 2D | `make_font`, `text`, `text_c`, `rect` |
| `rendu/modeles.c`, `rendu/armes_rendu.c`, `rendu/viewmodel.c` | modèles 3D, rendu des armes, arme en main | `draw_enemy`, `draw_tesla_arcs`, `draw_flails`, `draw_viewmodel` |
| `interface/mise_a_jour.c` | bouton MISE A JOUR (§3.17) | `upd_click`, `upd_poll`, `upd_label`, `upd_install`, `sha256_file` |
| `interface/config.c` | `velocity.ini` | `cfg_save`, `cfg_load`, `assign_bind`, `key_name` |
| `interface/menus.c` | menus et paramètres | `button`, `slider`, `draw_settings`, `draw_loadout`, `draw_skin_menu`, `draw_main_menu` (dont PARTIE CLASSEE), `draw_rank_box` (encadré CLASSEMENT / TOP 3), `draw_challenge_menu` / `draw_sr_board` (écran CHALLENGE), `draw_pause` |
| `interface/scores.c` | scores, killfeed, pseudos | `draw_scoreboard` (colonne CLASSE), `draw_rank_result`, `draw_killfeed`, `project`, `draw_name_tags` |
| `rendu/joueurs.c` | joueurs distants | `np_anim`, `draw_trails`, `draw_outlines`, `draw_remote_players` |
| `reseau/modes.c` | modes de jeu (§3.23) | `mode_tick`, `mode_reset`, `mode_on_death`, `mode_time_winner`, `mode_bot_goal`, `mode_ok`, `zone_r`, `shield_r`, `obj_event` |
| `jeu/economie.c` | boutique (§3.23) | `credits`, `eco_reset`, `eco_gain`, `eco_buy`, `eco_pick_starter`, `eco_loadout`, `ammo_rule_mul` |
| `interface/modes_hud.c` | modes : affichage (§3.23) | `draw_modes_world`, `draw_modes_hud`, `draw_shop` |
| `jeu/style.c` | jauge de style (§3.20) | `style_add`, `style_kill`, `style_parry`, `style_tick`, `style_mult` |
| `jeu/course.c` | mode COURSE (§3.21) | `race_start_map`, `race_start`, `race_tick`, `race_respawn`, `race_ghost_at` |
| `jeu/speedrun.c` | speedrun (§4 « Speedrun ») : catégories, planchers, speedrun complet | `sr_key`, `sr_label`, `sr_floor_ms`, `sr_full_start`, `sr_full_map_done`, `sr_full_tick`, `sr_full_ms` |
| `interface/hud.c` | HUD | `melee_preview`, `draw_radar`, `draw_style_hud`, `draw_hud` |
| `interface/course_hud.c` | course : affichage et record ; speedrun : résultat, panneau, bilan | `draw_race_world`, `draw_race_hud`, `race_save`, `race_load`, `sr_pb_save`, `sr_result_line`, `draw_sr_full_panel`, `draw_sr_full_end` |
| `rendu/ciel_ombres_bloom.c` | ciel, ombres, bloom (voir §3.12) | `draw_sky_dome`, `draw_dyn_shadows`, `bloom_apply` |
| `effets/monde_fx.c` | effets du monde | `draw_world_marks`, `draw_world_glow`, `draw_weather`, `storm_tick`, `draw_flare` |
| `interface/carte_ecran.c` | second écran : carte et stats (§3.15) | `draw_map_screen`, `maps_draw_map`, `maps_draw_panel` |
| `rendu/render.c` | image complète | `gfx_reload`, `render` |
| `plateforme/second_ecran.c` | fenêtre du second écran | `map_update`, `map_begin`, `map_end`, `second_screen_frame` |
| `plateforme/fenetre.c` | fenêtre et entrées | `in_key_down`, `in_wheel`, `wheel_accept`, `wndproc`, `plat_*` |
| `tests/tests.c` | outils de test (§6) | `save_bmp`, `*_test_*`, `run_tests` |
| `plateforme/demarrage.c` | point d'entrée | `music_follow_game`, `do_action`, `WinMain` / `velocity_main` |

À savoir pour s'y retrouver :

- **L'ordre des `#include` de `main.c` compte** : une fonction doit être définie, ou déclarée en avance, dans un
  morceau placé plus haut que celui qui l'appelle. Ajouter une fonction : la mettre dans le fichier de son thème ;
  si elle sert plus haut, ajouter une déclaration anticipée (comme `net_hit` dans `reseau/etat_partage.c`).
- **Nouveau fichier** : créer `src/<theme>/nom.c` avec la ligne de rôle en tête, puis l'ajouter à `main.c` au bon
  endroit. Rien d'autre à changer : les scripts de compilation ne compilent que `main.c`.
- Une arme touche plusieurs fichiers : sa logique dans `armes/`, mais ses événements dans `reseau/`, son rendu dans
  `rendu/armes_rendu.c` et son HUD dans `interface/hud.c` (chaque partie est balisée `/* --- Nom : ... --- */`).
- **`game_tick` est dans `jeu/boucle.c`**, après le réseau, parce qu'il a besoin de `host_tick`.
- **`input_poll` est dans `effets/combat_fx.c`**, juste après `shard_tick`.
- **`render` est dans `rendu/render.c`**, après le HUD et le ciel qu'il appelle.

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
  Une touche à la tête la fait avancer de `CHAIN_HEAD_STEP` (2) ; elle est bornée à `STREAK_MAX`.
- **Chaîne pleine = one-shot** (`chain_begin` / `chain_end`, `src/combat/degats.c`) : quand `shotStreak` atteint
  `STREAK_MAX` (son `S_CHAIN_FULL`, « ONE-SHOT PRET » dans le cadre CHAINE DE TIR), le prochain tir de **rail ou
  d'Élan** l'engage (`chainArmed`) : s'il touche un joueur (ou un ennemi en solo), il tue ; puis la chaîne repart de
  zéro, touche ou non. Les touches portent le drapeau `HF_CHAIN`, que l'hôte valide (voir §4, autorité de l'hôte).
- **Tirs à la tête** (`head_*`, `src/combat/degats.c` ; réglages `HEAD_*` dans `equilibrage.c`) : sphère de rayon
  `HEAD_R` centrée `HEAD_DROP` sous le haut du joueur (`np_head` : 1,54 m debout, 0,84 m accroupi, comme la tête des
  skins) ; ennemis : point faible au-dessus de `HEAD_ENEMY_FRAC` × rayon. Seuls les tirs directs de précision en ont :
  **rail** ×`HEAD_MULT_RAIL` (1,5) et **one-shot**, **Élan** ×`HEAD_MULT_KIN` (1,35) **sans plafond PvP** (90 PV au plus
  à pleine vitesse, pas de one-shot). Le ricochet de pièce vise tout seul (pas de tête), les armes de zone non plus.
  `hitscan_seg` teste la tête (`head_hit_player`) et appelle `hit_player_fl` avec le drapeau `HF_HEAD` et le point
  d'impact sur la tête (`EV_HIT.f[4..6]`). Retours : chiffre doré « TETE » (`DN_HEAD`) ou « ONE-SHOT » (`DN_ONESHOT`),
  son `S_HEADSHOT`, éclat doré, « TETE ! » chez la victime, killfeed `[RAIL TETE]` / `[RAIL ONE-SHOT]` (`KF_*`),
  caméra de mort « ONE-SHOT A LA TETE ». Bots : `botHeadP` (3 / 12 / 25 % des tirs cadrés visent la tête, `HEAD_BOT_OFS`).
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

La pièce est une **compétence** parmi sept (`enum SK_*`), choisie au LOADOUT (`wantSkill`, clé `skill=` de
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
| `SK_AIM` AIMANT | `skill_aim`, `aim_assist` | `P.aimT` = `AIM_T` (4 s) : le viseur est attiré vers la cible la plus proche du centre (cône `AIMASSIST_CONE` ~6°, `AIMASSIST_RATE`, 70 m, ligne de vue ; alliés exclus), jamais verrouillé ; 1 charge, recharge `AIM_RECHARGE` 45 s **non remplie au respawn** ; pas de 2e activation pendant l'effet ; les autres joueurs voient l'activation (`EV_SKILL`) |

- Bonus centralisé : `skill_out()` dans `damage_enemy`, `net_hit*`, `hit_player_ex` (une fois par touche).
- Réseau : `EV_SKILL` (marque, charge, all in one, aimant), `EV_COIN` sous-type 4 (leurre), `EV_ROCKET.sub` = rebonds.
- Bots (`bot_skill`, appelé en fin de `bot_think`) : MARQUE sur une cible vue à 10–60 m, CHARGE maintenue
  quand une cible approche, RICOCHET armé cible à couvert / près d'un mur (puis tir par le sol si elle reste
  cachée), ALL IN ONE à 4–14 m avec ≥ 3 munitions, MIRAGE sous 45 PV ou juste après une touche, AIMANT à l'engagement
  (cible vue à moins de 49 m ; l'aimant s'applique alors à la visée imprécise du bot, après `bot_think`). Un bot vise
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

### 3.5 Mode VAGUES (solo : `src/solo/`)

Le bouton **VAGUES** du menu (`ACT_SOLO` → `vg_start`) lance 100 vagues, puis l'INFINI. `vagOn` = partie VAGUES en
cours (remis à 0 par toute autre action du menu). Non simulé en LAN (`game_tick` n'appelle `enemy_tick` /
`wave_tick` que si `!dmMode` ; `wave_tick` tourne aussi pendant le challenge COURSE).

**Règle du propriétaire : aucune inflation.** Les dégâts des ennemis sont ceux de leur type et ne montent jamais
(`VG_ORB_DMG` 12, `VG_RUSH_DMG` 18, `VG_SWARM_DMG` 6, `VG_BIG_ORB_DMG` 30, `VG_RAM_DMG` 22, `VG_ROLL_DMG` 20,
`VG_WAVE_DMG` 18, `VG_LASER_DMG` 14 ; la cadence des drones est fixe en VAGUES) ; seuls leurs PV montent, à partir de
la vague 66, jusqu'à ×1,5 (`vg_hp_mul`, `VG_HP_FROM`, `VG_HP_MAX`, plafond aussi en INFINI). Aucun achat de la
boutique n'augmente une stat : chaque étape ajoute un comportement. Réglages : section « Mode VAGUES » de
`src/reglages/equilibrage.c`.

| Fichier | Rôle |
|---|---|
| `solo/ennemis_vagues.c` | types d'ennemis et leur IA (`enemy_tick`), apparition (`spawn_enemy_at`, `spawn_enemy_type`, `spawn_enemy` de l'ancien solo), `reset_game` (appelle `vg_reset` en VAGUES) |
| `solo/boss.c` | les quatre boss (`boss_spawn`, `boss_tick`, `boss_filter`), ondes au sol `GW`, rayons `LZ`, impacts `MT`, balayage `SWP`, hypercube `HYP` (`boss_fx_tick`) |
| `solo/ameliorations.c` | boutique : effets `fxDef`, chemins `upPath`, `vgLvl`, `up_buy`, crochets `vg_on_hit` / `vg_on_kill` / `vg_on_parry` / `vg_on_fire` / `vg_dmg_filter` / `vg_lure`, objets des effets (éclats `VS`, mines `VM`, spectres `VP`, vortex `VV`) |
| `solo/vagues.c` | progression : `wave_tick`, `vg_wave_start`, composition (`vg_count`, `vg_pick_type`), crédits, sélecteur, challenges, INFINI, temps (`vgRunT`, `vagues_temps_100`) |
| `rendu/vagues_rendu.c` | `vg_draw_enemy` (appelé par `draw_enemy`), `vg_draw_world_fx` (passe additive de `render`) |
| `interface/vagues_hud.c` | `vg_draw_info`, barre du boss, boutique, sélecteur, écran de la vague 100, écran de mort |
| `tests/vagues_tests.c` | `vagues_tests` (appelé à la fin de `run_tests`) et les captures `vagues_shot` |

**Ennemis** (`EN_*` ; rayon, couleur, nom : `enemyRad`, `enemyCol`, `enemyName` dans `degats.c` ; PV : `enemyHp`) :

| Type | PV | Comportement |
|---|---|---|
| `EN_DRONE` | 100 | tourne autour du joueur à ~13 m, orbe 16 m/s ; élite : esquive de côté quand on le vise |
| `EN_RUSHER` | 50 | kamikaze à 12 m/s, explose au contact ; élite : zigzag |
| `EN_SNIPER` SENTINELLE | 80 | se poste à ~30 m, vise 1,4 s (rayon vert visible), se fige 0,35 s avant le tir (orbe 34 m/s, parable) ; élite : suit jusqu'à 0,12 s |
| `EN_SWARM` ESSAIM | 25 | par 5, tourne autour à 7 m, plonge, touche (6 PV) et repart |
| `EN_BASTION` | 300 | marche au sol, bouclier de face (×0,15 sur les touches de face, `vg_dmg_filter`), salve de 3 orbes lentes ; tombé dans le vide = revient à son départ ; élite : tourne deux fois plus vite |

Les élites (`Enemy.elite`, couronne dorée) changent de comportement, jamais de dégâts ; leur part monte de 0 à 60 %
entre les vagues 66 et 100 (`vg_elite_p`). Champs ajoutés à `Enemy` : `maxHp`, `st` / `stT` (état de l'IA), `phase`,
`elite`, `summoned` (1 = invoqué par un boss, 2 = fantôme du PRISME), `frozenT`, `burnT`, `frac` / `fracT`, `yaw`,
`aim`, `home`, `lastDmg`, `combo`.

**Courbe de puissance** (`vg_count`, `vg_pick_type`) : vagues 1-33 faciles (3 → 14 ennemis, les types arrivent un à
un : kamikaze 2, sentinelle 8, essaim 14, bastion 22) ; 34-66 sommet du joueur (15 → 26 ennemis, surtout faibles) ;
67-100 les ennemis rattrapent (22 → 30, plus de sentinelles et de bastions, élites, PV jusqu'à ×1,5). Au plus
`VG_ALIVE_MAX` (16) vivants à la fois, un toutes les `VG_SPAWN_T`. Pause de `VG_PAUSE_T` (3 s) entre deux vagues :
**TAB** (`A_SCORES`) ouvre la boutique.

**Toutes les 10 vagues** (`vg_open_select`, écran `VGM_SELECT`) : trois choix pour le bloc suivant ; la carte est
chargée au tick suivant le clic (`vgPend` → `vg_choose`) :
1. **vagues simples** sur une carte fermée (ARENE, LA BOUCLE, LE DOJO à tour de rôle, pas la carte en cours) ;
2. **vagues difficiles** sur une carte à vide (LE PUITS, LE CANYON) : crédits ×1,5 (`VG_HARD_CREDITS`) ;
3. **challenge** : COURSE sur LA TOILE (les portes en moins de `VG_RACE_LIMIT` 75 s, sans toucher au record de la
   course) un bloc sur deux, sinon DÉFENSE (tenir la zone B de la carte simple `VG_DEF_HOLD` 40 s avant `VG_DEF_LIMIT`
   100 s, assaillants continus) ; réussi = `300 + 60 × bloc` crédits ; puis le bloc se joue sur la carte simple
   proposée.

**Boss** (`boss.c` ; barre de vie en haut de l'écran avec ses seuils de phase et un conseil `vgHint`) :

| Vague | Boss | Axe | Mécanique |
|---|---|---|---|
| 25 | PYRAMIDE (2400 PV) | parry | file d'un point à l'autre (30 m/s), s'arrête, salves de 5 orbes ; toutes les 3 salves une grosse orbe chargée 2,5 s (parée, elle revient : `VG_BIG_ORB_RETURN` et étourdie). Armure ×0,35 sauf orbes parées et étourdie (×1,5). À 50 % : plus d'orbe chargée, trois charges de suite (ligne rouge 0,9 s, se fige 0,25 s avant, 42 m/s) ; parée (`P.parryWin`, de face, à portée d'épée) = étourdie `VG_STUN_T` et `VG_RAM_PARRY_DMG` |
| 50 | TESSERACT (3600 PV) | contrôle de foule | cube 4D (16 sommets qui tournent dans XW, YW, ZW, projetés), totalement immobile au centre (point libre de tout décor) ; invoque 4 ennemis toutes les 6 s (4 s sous 50 %), **invulnérable tant qu'il en reste `VG_LINKS`** (liens visibles) ; barrages lents : anneau, spirale, éventail |
| 75 | SPHÈRE (4200 PV) | mouvement aérien | roule vers le joueur, bondit et frappe le sol : onde de choc au sol (`GW`, 16 m/s) qu'il faut sauter ; sous 50 %, deux rebonds ; **touches portées depuis le sol ×0,15** ; tombée dans le vide = retombe du ciel |
| 100 | HEXAÈDRE (8000 PV) | tout | PRÉCISION (> 75 % : trois rayons annoncés à esquiver, puis cœur ouvert 2,5 s ; armure ×0,3 sinon), PARRY (charges et grosse orbe), FOULE (invocations, bouclier, anneaux), TOUT (< 25 % : piques au sol avec ondes, rayons, charges, essaims ; ×0,2 depuis le sol) |

Un boss ne recule pas (`damage_enemy`) et ne se fige pas ; ses invocations s'éteignent à sa mort (`vg_boss_down`).
Dans une salle basse (LE DOJO), les boss rétrécissent (`vgBossScale`, appliqué par `enemy_radius` et le rendu).

**Vague 100 terminée** : `vgT100` = temps de partie, écran `VGM_WIN` (temps, score, kills, temps aux vagues 25 / 50 /
75, boutons LANCER L'INFINI, RECOMMENCER, MENU PRINCIPAL). **INFINI** (`vgInfinite`) : un boss toutes les 25 vagues
(même ordre), `vgSecret` = 1 (attaques secrètes : PRISME, trois pyramides fantômes chargent de trois côtés, seul un
parry les brise ; HYPERCUBE, une sphère de 26 orbes se referme sur le joueur ; MÉTÉORES, trois impacts annoncés sur
ses pas ; DÉPLIAGE, quatre rayons au ras du sol tournent 7 s, à sauter). Les vagues sont minutées (`VG_INF_T` 30 s, ou
`VG_INF_CLEAR_T` 2 s après avoir tout tué ; ce qui reste en file s'ajoute à la suivante), sauf les vagues de boss et
de fin de bloc (`vg_checkpoint`), qui attendent que tout soit tué. 30 → 40 ennemis, PV toujours plafonnés à ×1,5.

**Crédits** (`vgCredits`, `vg_credit_kill`) : drone 10, kamikaze 6, sentinelle 14, essaim 3, bastion 25, boss 400 /
600 / 800 / 1200 ; élite ×1,5, carte difficile ×1,5, invocation ÷3 ; `20 + vague` par vague vidée.

**Boutique** (`ameliorations.c`, écran `VGM_SHOP`) : les 4 chemins du loadout (2 armes, mêlée, compétence), 5 étapes
chacun, achetées dans l'ordre (`VG_PRICE_1..5` : 120, 240, 400, 600, 850). 20 chemins (`NUP` = `WP_COUNT + MK_COUNT +
SK_COUNT` ; indices `WP_*`, `UP_MELEE(mk)`, `UP_SKILL(sk)`), 31 effets (`fxDef` : nom, déclencheur, description). Un
effet ne vaut que pour les touches de son chemin, et seulement si l'arme est équipée (`up_has`) :
- **source** : `vgSrc` dit à quel chemin reviennent les touches en cours. Posé dans `game_tick` (arme en main autour de
  `fire_weapon`, `WP_TESLA`, `WP_MINE`, mêlée autour de `player_tick` et des coups) et par projectile dans `proj_tick`
  (roquette, scie, mine ; projectile paré = mêlée). Les compétences ont un **contexte** (`fx_paths`) : PIÈCE = touches
  d'un ricochet (`vgCoinCtx`, `pieces_rail.c`), MARQUE = cible marquée, CHARGE = touche chargée, RICOCHET = 1,5 s
  après le tir qui rebondit, ALL IN ONE = 1,5 s après la salve, AIMANT = sous l'aimant ; MIRAGE agit par le leurre
  (APPAT : les ennemis le visent, `vg_lure` ; EXPLOSIF ; REFLET ; GOUFFRE) ;
- **déclencheurs** : touche (`vg_on_hit`, depuis `damage_enemy`), kill (`vg_on_kill`, depuis `kill_enemy`), tir
  (`vg_on_fire`), parry (`vg_on_parry`, depuis `parry_feedback`), dash, retombée de haut, compétence utilisée (charge
  dépensée), passif (`vg_fx_tick`) ;
- **réaction en chaîne** limitée à une génération (`vgDepth`) ; les dégâts d'un effet comptent pour son chemin
  (`fx_damage`) ; la brûlure et les ennemis n'ont pas de chemin.

**Temps** (`vgRunT`, `vg_clock` en tête de `game_tick`, hitstop compris) : temps de partie, pauses ÉCHAP exclues ; le
temps passé dans la boutique et le sélecteur compte (`vg_menu_set`). Point d'entrée du mode speedrun :
`vagues_temps_100()` (secondes, −1 avant la fin de la vague 100), `vagues_splits_ms(out[4])` (cumulés aux vagues 25 /
50 / 75 / 100), `vg_speedrun_begin()` (départ, vague 1) et `vg_speedrun_end()` (vague 100), où brancher
`speedrun_begin` / `speedrun_report_splits`.

- Mort : écran « TOMBE A LA VAGUE n » ; **R** ou clic recommence à la vague 1 sur la carte de départ. Record : `bestScore`.
- Écrans (souris libérée, jeu figé) : `vgMenu` (`VGM_SHOP`, `VGM_SELECT`, `VGM_WIN`) ; ÉCHAP ferme la boutique ; à la
  fermeture, `vgWantResume` → `ACT_RESUME` recapture la souris.
- Tests (`vagues_tests`) : courbe (nombre, PV, élites, boss), dégâts fixes, enchaînement des vagues, sélecteur et
  crédits, boutique (ordre, prix, dégâts inchangés), effets (fourche, gel, fracture, onde de parry, propulsion),
  bastion, chaque boss, vague 100 et splits, INFINI (chrono, boss qui attend, secret), temps, challenges.
- Captures : `--shot vagues_pyramide.bmp` (`_charge`), `vagues_tesseract`, `vagues_boule`, `vagues_hexaedre`,
  `vagues_boutique` (`_small`), `vagues_selecteur`, `vagues_fin`, `vagues_infini`, `vagues` (une vague ordinaire).

### 3.6 Effets visuels

> Éclairage, ombres, ciel et bloom : voir §3.12.

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
| Changement d'arme | `wpFlashAt` (temps réel `nowT`), `P.switchT` | `weapon_select` | — (`WP_FLASH_T` 0,9 s) | `draw_hud` : nom de l'arme sous le viseur + emplacement surligné ; `draw_viewmodel` : l'arme remonte en pivotant. Capture : `--shot changearme.bmp` |
| Fonds du HUD | `gxHudPanel` (`hud_fonds` dans `rendu.ini`, 0,22 par défaut, 0 = aucun) | — | — | `hud_panel` (`interface/texte.c`) : voile sombre léger, sans liseré, sous les infos de partie, la mêlée, la vitesse, la chaîne de tir, le panneau d'arme ; un fond par ligne de killfeed |
| Barre de vie | `hpGhost`, `hpHitAt`, `hpHealAt` | — | `hp_ghost_tick` (appelé par `draw_hud`, temps réel) : la partie perdue reste `HP_GHOST_HOLD` 0,4 s puis se vide à `HP_GHOST_RATE` 80 PV/s | `draw_hud` : traînée blanche puis rouge, éclat vert au soin, graduations tous les 25 PV. Capture : `--shot vie.bmp` |

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
- **Musique** (un morceau par carte : Arène hard techno, Puits dub techno, Canyon breakbeat, Boucle synthwave,
  Dojo drum and bass, Toile trance ; `MapDef.music`) : tout le design est dans **`src/son/music.h`** (morceaux `tracks[]`
  + moteur), inclus par `son/audio.c`. Séquenceur synthétisé mixé dans `audio_fill` via `music_frame()` (stéréo), volume
  `musicVol` (`music=` dans `velocity.ini`, curseur « Musique »). Un morceau : BPM de départ, `bpmStep` (BPM
  gagnés par niveau), tonique (Hz), jusqu'à 16 **cellules** d'une mesure de 16 pas (`MusBar` : masques
  `kick/clap/hat/ohat`, notes `bass/lead` en demi-tons, `RS` = silence, `fx` = `FX_RISER` / `FX_CRASH`), et
  l'**ordre des cellules sur 32 mesures** (`song` aux niveaux 0-2 avec intro progressive, `songHi` dès le
  niveau 3 sans intro) plus une transposition `prog` par groupe de 4 mesures. Instruments : kick saturé à chute
  de hauteur, basse rumble en sidechain, hats/clap/crash/riser en bruit, lead supersaw (7 scies désaccordées, écart croissant avec le niveau), acid
  (scie dans un filtre résonant, dès le niveau 5). Chaque morceau choisit aussi sa **gamme** (`scale`, pour les notes
  voisines ; tout à 0 = do ré fa la de l'Arène), son **timbre de lead** (`MUS_SAW` supersaw, `MUS_PLUCK` carré pincé
  pour les arpèges, `MUS_BELL` cloche FM) et son **swing** (pas pairs plus longs). L'Arène est inchangée à l'échantillon près.
  Intensité : en course, 2 + la progression dans les portes ; la jauge de style ajoute +1 en A, +2 en S.
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
   `K_X2`, `K_WHEEL` (la molette n'existe qu'en « appui », jamais « tenue »). `wheel_accept` filtre la molette :
   une rafale d'événements (roue libre, inertie) ne compte qu'une fois — repos `WHEEL_QUIET` 0,15 s entre deux
   gestes, `WHEEL_COOLDOWN` 0,3 s entre deux changements ; sur macOS la phase d'inertie (`momentumPhase`) est ignorée.
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
  `wantTeams` (`teams=`), `gfxShadows` (`shadows=`), `gfxBloom` (`bloom=`),
  `impactFrames`, `screenFx`, et les `binds`.
- Fichier : `velocity.ini` **à côté de l'exécutable** (chemin `cfgPath`, calculé avec
  `GetModuleFileNameA` dans `WinMain`). Format `cle=valeur`, une par ligne, `#` pour un commentaire :
  `name`, `sens`, `fov`, `volume`, `invert_y`, `show_fps`, `fullscreen`, `impact_frames`, `screen_fx`,
  `online`, `update_check` (vérifier les mises à jour au démarrage), `map` (carte choisie, §3.18), `map_rotate`,
  `second_ecran` (**0 par défaut**, §3.15), `ranked_id` / `ranked_key` (identité du mode classé, créée au premier
  lancement, §4 « Mode classé »), `mmr` / `ranked_played` / `ranked_wins` (simple **cache d'affichage** de la cote :
  le serveur fait foi), puis `bind_<actId>=code1,code2`.
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
  touche ; Échap annule, Retour arrière/Suppr efface). PARAMÈTRES garde la sensibilité, le champ de vision, le
  volume, la musique, AXE Y, CAMERA DE MORT, PARTIES EN LIGNE, MISES A JOUR AUTO, les commandes, RETOUR / PAR
  DEFAUT et un bouton **GRAPHISMES** (liseré doré) qui ouvre `draw_graphics_settings` (`settingsPage` = 1) :
  RENDU (QUALITE, CIEL, OMBRES, BLOOM, EFFETS D'ECRAN, FLASHS D'IMPACT) et ECRAN (PLEIN ECRAN, AFFICHER LES FPS,
  SECOND ECRAN, VUE DU SECOND ECRAN : HAUT / 3D, grisée sans second écran), l'aide de F5, le dossier `graphismes`
  trouvé (`gfxDir`, ou « dossier absent ») et l'explication du réglage survolé (`gfx_button`). Libellés complets
  (« CAMERA DE MORT : OUI », pas d'abréviation). Mise en page commune `settings_lay` : compacte sous 760 px de
  haut, tout tient en 1024 × 640 (`--shot settings_small.bmp`, `graphismes_small.bmp`). Échap : GRAPHISMES →
  PARAMÈTRES → menu précédent (`in_key_down`). Test : « menus : ECHAP depuis GRAPHISMES ».
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

### 3.11 Mode équipes (1v1 à 4v4)

- **Choix** : écran HEBERGER UNE PARTIE (`draw_host_menu`, `hostOpen`) : NOMBRE DE JOUEURS (LIBRE / 1V1 / 2V2 /
  3V3 / 4V4 → `wantTeams`, `teams=` dans `velocity.ini`, `--teams N` pour un bot hôte), COMPLETER AVEC DES BOTS
  (`hostBots` : aucun ou un niveau), puis LANCER (`ACT_HOST`, ou `ACT_VSBOT` avec des bots). `net_host_start`
  copie `wantTeams` dans **`teamSize`** (0 = chacun pour soi). Une PARTIE RAPIDE et le duel « joueur contre
  bot » du menu principal (`hostFfa`) sont toujours en chacun pour soi.
- **Données** : `NP[i].team` (0 bleue, 1 rouge). L'hôte (id 0) est bleu ; `host_on_join` met chaque arrivant
  dans l'équipe la moins nombreuse et refuse au-delà de `host_max_players()` = 2 × `teamSize`.
  `host_balance_teams` (au début de chaque match) corrige un écart de 2 ou plus après un départ.
  `teamSize` et `SnapP.team` voyagent dans `PkSnap` ; `PkAnnounce.teams` annonce le mode en LAN ; en ligne,
  le nom du salon finit par `_2V2` et `max` = 2 × `teamSize`.
- **Pas de dégâts entre alliés** : `same_team(a, b)`. `np_target(i)` exclut les alliés : aucune arme, aucun
  souffle, aucune aide à la visée, aucun bot ne les vise (les roquettes et la Tesla les traversent).
  `np_present(i)` = joueur à **dessiner** (alliés compris). Filet de sécurité : `host_handle` annule tout
  `EV_HIT` entre alliés (sauf `W_SELF`).
- **Score** : `team_score(t)` = somme des frags de l'équipe. Limite `team_frag_limit(n)` =
  `TEAM_FRAG_BASE` + `TEAM_FRAG_PER` × n (2v2 : 20, 3v3 : 25, 4v4 : 30). `host_kill` termine le match quand
  l'équipe du tueur l'atteint ; à `MATCH_TIME`, la meilleure équipe gagne (`TEAM_DRAW` = égalité).
  En équipes, `matchWinner` est une **équipe** (texte : `winner_text`).
- **Spawns** : `pick_spawn` prend dans `teamPts` (bleus au nord, z < 0 ; rouges au sud, miroir), le plus loin
  des **ennemis**.
- **Affichage** : `pcolor(i)` (couleur d'équipe, éclaircie selon le rang) remplace `pcol[i]` ; scores des deux
  camps en haut au centre ; `draw_team_scoreboard` ; noms des alliés visibles à travers les murs (chevron) ;
  fil des kills teinté ; musique : `music_pvp_level` reçoit les deux scores d'équipe.
- **Contre des bots** (`ACT_VSBOT`) : en équipes, lance `host_max_players() - 1` bots (`botProc[]`) qui
  remplissent les deux camps (2v2 : toi + 1 bot contre 2 bots).

### 3.12 Éclairage, ombres, ciel, bloom

- **Lightmaps précalculées** (`bake_lightmaps`, appelé par `build_world_list`, ~0,25 s) : une texture par
  face visible de boîte (`lmaps[boite][face]`, `LM_DENS` = 3 texels/m, `face_hidden` saute les faces enfouies
  ou hors de l'arène). `lm_light` : soleil (`sun_dir`, ombre portée par `ray_world`) + ciel × occlusion
  ambiante (`AO_RAYS` rayons courts via la grille `lm_grid_build`/`ray_near`) + rebond du sol + lueur cyan des
  jump pads. Flou 1-2-1. Le monde est dessiné **sans** éclairage OpenGL, puis la lightmap est multipliée ×2
  (`GL_DST_COLOR, GL_SRC_COLOR`, 0,5 = neutre ; brouillard gris pour cette passe, blanc pour le grain).
- **Objets mobiles** : `set_dyn_light(vis)` règle `GL_LIGHT0` sur le soleil, atténué si `sun_vis(p)` = 0
  (joueurs distants, ennemis) ; l'arme en main suit `viewSun` (lissé). `GL_NORMALIZE` est activé (l'arme
  est mise à l'échelle, sinon le reflet spéculaire explose).
- **Ombres dynamiques** (`draw_dyn_shadows`, réglage `shadows=`) : silhouettes (moi, joueurs, ennemis,
  cellules) aplaties sur le dessus de la boîte qui les reçoit le long du soleil (`shadow_caster_boxes`),
  découpées par `glClipPlane`, assombries une seule fois grâce au **stencil**.
- **Ciel** : `draw_sky_dome` (dôme dégradé `sky_color`, lueur autour du soleil, étoiles, disque solaire) et
  ville lointaine `skylineList` (`build_skyline_list`, décor sans collision).
- **Bloom** (`bloom_apply`, réglage `bloom=`) : copie de l'image, réduction au quart, seuil doux (x^8 par
  multiplications), flou séparable, ajout. Fait avant l'impact frame et le HUD.
- **Valeurs par défaut adoucies** (entre-deux entre la release b1dcfd6 et le rendu du fork) : ombres lisibles
  (`occlusion_min` 0,45, `ciel_lumiere` 0,64 0,66 0,84, `rebond_sol` 0,36 0,31 0,29, `soleil_force` 1,2 : les marches
  de l'escalier central, à contre-jour, ne sont plus noires), bloom discret (`bloom` 0,5 4 2 1,6 : seuil x^16, seuls
  néons et soleil brillent), petit soleil (`soleil_taille` 6 16 50, `soleil_halo` 0,16 0,30 0,30, `reflets_soleil`
  0,25), ombres des joueurs `ombre_opacite` 0,4, fonds du HUD `hud_fonds` 0,22 sans liseré. Mêmes valeurs dans
  `rendu_ini.c` et `graphismes/rendu.ini` ; test « graphismes defauts ». Ambiances : NUIT (lune 7 14 34, ciel un peu
  plus clair 0,34 0,38 0,62), JOUR (halo 0,14 0,26, bloom 0,35).
- **Texte** : `glBitmap` (polices de `make_font`) est très lent sur certains pilotes (12 ms/image sur macOS). Au
  démarrage, `make_font_atlas` dessine les 224 glyphes une fois, les relit (`glReadPixels`, par lots si la fenêtre
  est petite) dans un atlas `GL_ALPHA` ; `text` dessine ensuite des quads texturés, au pixel près.
- **Mesurer** : `--shot bench_dm.bmp` (ou `bench_dm_equipes.bmp`…) écrit `bench_dm.bmp.txt` : ms/image avec et sans
  bloom/ombres, puis le détail par étape (`BT(k)` dans `render`). Référence (MacBook Air, 1372×891) : ~1,9 ms/image.
- **Bots cachés** (`--hidden`, ceux de « contre des bots ») : aucun rendu ni lightmap, boucle limitée à ~250 Hz.

### 3.13 Réglages graphiques modifiables (`graphismes/`)

- **`graphismes/rendu.ini`** (lu au démarrage par `gfx_load_all`, et à chaque **F5** par `gfx_reload`) : toutes les
  valeurs d'aspect, déclarées en tête de la section Rendu (`gx*`, `sunCol`, `skyCol`…) avec leur défaut, et listées
  dans **`gfxKeys`** (nom de clé → variable, nombre de valeurs). Ajouter un réglage = une variable + une ligne de
  `gfxKeys` + sa ligne commentée dans `rendu.ini`. `gfx_clamp` borne les valeurs ; `gfx_defaults` remet les défauts
  avant chaque relecture. Les tests (`--test`) n'ouvrent pas ce fichier (valeurs par défaut).
- **Textures** : `sol.bmp`, `mur.bmp`, `grain.bmp` du même dossier (`tex_override`,
  `load_bmp_square`) remplacent `texGrid`, `texWall`, `texGrime`. Sans `.bmp`, la texture est
  calculée sur le processeur par `tex_generate(TEXG_*)` (une image RGB, puis `upload_rgb_mip`) à partir de
  `sol_texture`, `mur_texture`, `grain_force` : ces réglages n'ont donc **plus d'effet** quand le
  `.bmp` existe. Le dépôt livre ces `.bmp` (exception `!graphismes/*.bmp` du `.gitignore`), produits par
  **`velocity.exe --export-textures graphismes`** (`export_textures` : `tex_generate` puis `save_bmp_rgb`, BMP 24 bits de
  bas en haut en BGR, ce que relit `load_bmp_square` : même image octet pour octet, mêmes mipmaps). L'export lit
  `rendu.ini` comme le jeu (`gfx_load_all`). Aucune de ces textures ne dépend de la carte, de l'ambiance ni de la
  qualité. Test : ligne « export des textures » de `run_tests`.
  Après un changement de `tex_gen_*`, relancer l'export pour que `graphismes/` reste fidèle au code (sinon les
  `.bmp` masquent le nouveau rendu).
- **Recherche du dossier** (`gfx_find_dir`) : macOS `~/Library/Application Support/Velocity/graphismes`, puis à côté
  de `Velocity.app`, puis `Velocity.app/Contents/Resources/graphismes` (copié par `build_mac.sh`) ; Windows : à côté de
  l'exe ; enfin `./graphismes`.
- Purement visuel : rien dans ce dossier ne touche au gameplay ni au réseau.

### 3.14 Skins, effets du monde, ambiances

- **Skins** (`SKN_ROBOT`, `SKN_POUSSIN`, `SKN_NINJA`, `SKN_CHAT`, `SKN_CHEVALIER`) : `wantSkin` (`skin=` dans
  `velocity.ini`, écran SKIN du menu principal avec aperçu 3D `draw_skin_menu`), envoyé dans `NetPState.skin`
  (`NET_VER` 13). Modèles en boîtes : `draw_skin` → `skin_robot`… (pieds à l'origine, regard vers −z, échelle selon
  la hauteur). La couleur principale vient de `pcolor` (équipe), les accents (bec, acier) sont fixes. Animation
  `SkinAnim` (`np_anim` : course, saut, glissade, visée) ; `skc` gère le mode « tout d'une couleur » (`skinFlat`,
  contours) et le flash de touche (`skinFlash`). Les bots prennent un skin au hasard. Ajouter un skin : un `SKN_*`
  avant `SKN_COUNT`, son nom dans `skinNames`, sa fonction et son `case` dans `draw_skin`.
- **Contours** (`draw_outlines`, clé `contours`) : coque inversée (modèle un peu agrandi, faces arrière seules).
  Ne pas utiliser `glPolygonMode` avec des modes différents avant/arrière : rendu logiciel sur macOS.
- **Traînées de vitesse** (`trail_tick`, `draw_trails`, clé `trainees`) : ruban à la couleur du joueur au-delà de
  16 m/s.
- **Effets d'impact** (section Effets) : `add_light` (lumières dynamiques posées sur sol et murs, `light_splat`),
  `add_decal`/`decal_at` (marques), ondes au sol (`Ring`), fumée (`Smoke`), regroupés dans `fx_explosion` (appelé par
  `explode_ex` et `explode_fx`) ; `add_beam` éclaire et marque son point d'arrivée ; les roquettes éclairent en vol.
  Kill : éclats à la couleur de l'équipe. Tout est décompté en temps réel (`fx_world_tick`).
- **Néons** (`build_neon`, `draw_world_glow`, clés `neon*`) : bandes sur le haut des plateformes (cyan), des murs de
  wall-run (magenta) et du centre (orange), repérées par la couleur des boîtes ; leur lueur est précalculée dans les
  lightmaps (`lmnP`, listes par case). **Rebond coloré** (`rebond_couleur`) : les rayons d'occlusion qui touchent une
  boîte ajoutent sa couleur.
- **Ciel et météo** : nuages (`nuages`), poussière (`poussiere`), pluie (`pluie`), orage (`orage`, `storm_tick` :
  flash et tonnerre), reflets du soleil (`draw_flare`, `reflets_soleil`).
- **Ambiances** : `gfxAmbiance` (PARAMETRES > GRAPHISMES > CIEL, `ambiance=`) charge `graphismes/ambiance_nuit|jour|orage.ini`
  par-dessus `rendu.ini`. **Qualité** : `gfxQuality` (PARAMETRES > GRAPHISMES > QUALITE, `quality=`) plafonne les coûts
  (`gfx_quality_apply`) et règle ombres/bloom. Captures : `--shot arene_nuit.bmp`, `skins.bmp`, `skins_course.bmp`,
  `skinmenu.bmp`.
- **Radar des alliés** (`draw_radar`) : mode équipes, en haut à gauche.
- **Anticrénelage** : macOS demande un format 4x MSAA (`src/plateforme/mac/mac_platform.m`), sinon le format simple. Pas encore sous
  Windows (il faudrait `wglChoosePixelFormatARB`).

### 3.15 Second écran : carte de la partie

Si un deuxième écran est branché, une fenêtre sans bordure le couvre et affiche **la carte vue de dessus en direct**
(blocs ombrés selon leur hauteur, jump pads, recharges, pièces, roquettes, ennemis en solo ; joueurs en flèches à leur
couleur avec nom, PV et traînée de 2 s ; mon cône de vision) et **les statistiques** (mode, chrono, scores d'équipe,
classement frags / morts / ratio, ma vitesse et ma vitesse max, chaîne de tir, arme, derniers kills).

- **Pas de radar contre l'adversaire** (`maps_sees`) : pendant une partie on ne voit que soi et ses équipiers (en
  chacun pour soi : soi seul), avec leurs projectiles et pièces. Tout le monde apparaît en spectateur et à la fin.
- Dessin : `src/interface/carte_ecran.c` (`draw_map_screen(w, h)`, ne dépend d'aucun état laissé par `render`).
- Fenêtre : `src/plateforme/second_ecran.c`. `second_screen_frame(dt)` est appelé après chaque image : toutes les
  `SCREEN_POLL_T` (1 s) `map_update` recompte les écrans (branché, débranché, fenêtre du jeu déplacée), puis la carte
  est redessinée à `SCREEN_MAP_FPS` (30). Elle a son propre contexte OpenGL **partagé** avec celui du jeu (police,
  textures) — Windows : `wglShareLists`, fenêtre `WS_EX_NOACTIVATE` ; macOS : `mac_map_update/begin/end`,
  `NSOpenGLContext ... shareContext:gCtx`, fenêtre qui ignore la souris. Pas de synchro verticale sur ce contexte
  (sinon deux attentes par image).
- **Deux vues** : de dessus (`maps_draw_map`) ou **en 3D inclinée** (`maps_draw_3d` : blocs ombrés par face, pions à
  la couleur des joueurs avec flèche du regard, ombre au sol et poteau pointillé quand ils sont en l'air, traînées en
  3D ; noms projetés par `maps_proj3`). Angle, recul et champ : `carte_3d` dans `graphismes/rendu.ini` (`gxMap3d`).
- **Courbe de vitesse** (`maps_speed_tick`, `maps_draw_speed`) : un point toutes les 0,5 s par joueur (`mapsSpd`,
  vitesse ×4 sur un octet), interrompue pendant les morts ; 60 dernières secondes en partie, toute la partie à la fin.
  Seulement les joueurs visibles (`maps_sees`). Remise à zéro hors partie et quand le chrono du match repart.
- **Carte de chaleur** (`maps_heat_add` appelé par `apply_event` sur `EV_KILL`, `maps_draw_heat`) : position de la
  victime (rouge) et du tireur (jaune) à chaque kill ; taches additives de rayon `MAPS_HEAT_R`. Affichée à la fin du
  match (`matchOver`), toujours en vue de dessus. Capture : `--shot carte_dm_equipes_fin.bmp`.
- Réglage : PARAMETRES > GRAPHISMES > « SECOND ECRAN : OUI / NON » et « VUE DU SECOND ECRAN : HAUT / 3D »
  (`second_ecran=`, `carte_vue=` dans `velocity.ini`).
  **Désactivé par défaut** (`secondScreen` = 0) : la fenêtre ne s'ouvre que si le joueur l'a demandée.
  Jamais en `--bot`/`--shot`. Capture de la vue 3D : `--shot carte3d.bmp`, `carte3d_dm.bmp`…
- Capture : `--shot carte.bmp` (solo), `carte_dm.bmp`, `carte_dm_equipes.bmp` dessinent cet écran dans la fenêtre du jeu.

### 3.16 Caméra de mort

`src/effets/camera_mort.c`. À la mort (`apply_event` sur `EV_KILL` en LAN, `damage_player` en solo), `killcam_start`
retient l'œil, le cap et le tueur (en solo : l'ennemi vivant le plus proche qui me voit, dans `KILLCAM_SOLO_R`). Pendant
`KILLCAM_T` (1,6 s, le respawn reste à `RESPAWN_TIME` 2,5 s), `render` prend la vue de `killcam_view` : la caméra
recule et monte (`KILLCAM_BACK`, `KILLCAM_UP`, sans traverser les murs), pivote vers le tueur en `KILLCAM_TURN` et
zoome (`KILLCAM_ZOOM`). `draw_killcam_hud` : bandes noires, « ELIMINE PAR … », arme, PV qui lui restent, crochets autour
de lui ; le bas du HUD est masqué. Le loadout ne se change plus aux touches sur l'écran de mort : seulement au menu
LOADOUT (pause), appliqué au respawn. Ni suicide ni chute. Réglage : PARAMETRES > CAMERA DE MORT
(`camera_mort=`). Capture : `--shot dm_mort.bmp`.

### 3.17 Mise à jour depuis GitHub (bouton MISE A JOUR)

Fichier `src/interface/mise_a_jour.c`, inclus juste avant `interface/config.c`.

- **Source** : la branche `release` du dépôt public **`void19845/velocity`** (`UPD_REPO` dans
  `src/interface/mise_a_jour.c`), remplie par `.github/workflows/release.yml` à chaque push sur `stable` :
  `velocity.exe`, `Velocity-Windows.zip` (l'exe + le dossier `graphismes/`, pour un premier téléchargement),
  `Velocity-macOS.zip` et **`version.txt`** (`build`, `sha`, `win_size`, `win_sha256`, `mac_size`, `mac_sha256`).
  Lu à `https://raw.githubusercontent.com/void19845/velocity/release/` (`UPD_HOST`, `UPD_DIR`). Un fork qui publie
  ses propres versions change `UPD_REPO` (ou le passe à la compilation : `/DUPD_REPO=...`).
- Le bouton ne remplace que l'exécutable (Windows) ou le `.app` (macOS) : le dossier `graphismes/` d'une
  installation existante n'est pas touché (les réglages visuels du joueur sont conservés).
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

### 3.18 Cartes (`MapDef`) et kill zone

Six cartes, décrites par la table `maps[]` (juste après les fonctions de construction, section « Monde ») : cinq
cartes de partie et une carte **CHALLENGE** (LA TOILE, mode COURSE seulement, §3.21). Objectifs des modes : §3.22.

| # | Nom | Fonction | `killY` | Solo | Idée |
|---|---|---|---|---|---|
| 0 | ARENE | `build_map_arene` | −1000 (fermée) | oui | la carte d'origine, **inchangée** (`run_tests` dépend du couloir x = −20 et des pads) |
| 1 | LE PUITS | `build_map_puits` | −30 | non | puits de 40 × 40 m, spirale de 8 plateformes reliées par des pads (y 0 → 42), sommet à 50 (grandes cellules), deux balcons au centre à y 25, 12 ancres au centre, 2 îlots de secours au fond (pads vers la spirale) |
| 2 | LE CANYON | `build_map_canyon` | −15 | non | deux falaises (y 20) et un gouffre de 70 m ; pads au bord → îlots extérieurs → îlots centraux ; plateau central à y 10 (pads vers chaque falaise) ; tour de tir de 10 m par falaise ; 6 ancres espacées de 20 m au plus |
| 3 | LA BOUCLE | `build_map_boucle` | −1000 | oui | anneau de 120 × 60 m, deux voies par ligne droite avec bandes d'accélération à contresens, séparateurs bas, bloc central coupé au milieu, passerelles et toit à 10 m, pads de coin et pads vers le toit |
| 4 | LE DOJO | `build_map_dojo` | −1000 | oui | salle de 36 × 36 m, plafond à 7 m, 16 piliers, ring surélevé de 0,5 m, **aucune cellule** |
| 5 | LA TOILE | `build_map_toile` | −10 | non | **CHALLENGE** (`challenge` = 1) : course au grappin entre gratte-ciel (§3.21), jamais en partie |

- **`MapDef`** : `name` (sans accents), `build`, `spawns`/`nspawns` (coordonnées du monde ; l'Arène garde ses
  anciens points via la macro `MS()` = `ms()` à la compilation), `start` (départ solo et position d'attente en LAN),
  `killY`, zone des drones du solo (`enemyMinY/MaxY`, `enemyRMin/RMax` autour du centre, plancher `enemyFloor`),
  caméra du menu (`menuCam`, `menuPitch` ; le spectateur est `specUp` mètres plus haut), `solo`, `music`, couleurs
  `sky` (haut du ciel) et `fog` (horizon et brouillard, utilisées par `draw_sky` et `render`), portes de course
  (`gates`, `ngates`, `startYaw`), `challenge` (1 : carte du mode COURSE seulement), `objScale`, objectifs `zones`,
  `flags`, `shield`, départs par équipe `teamSpawn` / `nTeamSpawn` (§3.22).
- **Cartes CHALLENGE** (`challenge` = 1, LA TOILE) : jamais dans la ligne CARTE du menu (`map_step` les saute), ni dans
  la ROTATION, ni en partie hébergée / rapide / classée (`map_pvp`, `rk_map_ok`, `host_setup_rules` reprend l'Arène),
  ni en solo par vagues (`solo` = 0), et `mode_ok` refuse tous les modes ; on n'y va que par le bouton CHALLENGE
  (`race_map`). `map=` de `velocity.ini` qui désigne une carte CHALLENGE (ou une carte retirée) revient à 0.
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
  (l'hôte passe à chaque nouveau match à la carte suivante **qui a le mode en cours** et n'est pas une carte
  CHALLENGE : `map_rotate_next`, appelée par `host_start_match`). SOLO utilise la carte choisie si elle est `solo`,
  sinon l'Arène (le bouton l'indique). Options `--map N` (bots, captures).
- **Réseau** (`NET_VER` 13) : l'id de carte voyage dans `PkWelcome.pad[0]` (le client fait `load_map` avant
  `on_welcome`), `PkAnnounce.map` (affiché dans REJOINDRE : « [LAN LIBRE] Hote - LE PUITS ») et `EV_MATCH`
  (`target`, sous-type 0 : début de match ; l'hôte y met la carte suivante si ROTATION). Le lobby en ligne ne
  transmet pas la carte (on la reçoit à la connexion).

- **Avec le reste de `dev`** (fusion du dépôt original, `NET_VER` 14) :
  - **Équipes** : `pick_spawn` prend les départs de son camp (`MapDef.teamSpawn`, §3.22 : bleus côté négatif de
    l'axe des équipes, rouges côté positif), le plus loin des ennemis ; sans `teamSpawn`, les départs de la carte.
  - **Annonce LAN** : `PkAnnounce` porte `teams` **et** `map` (REJOINDRE : « [LAN 2V2] Hote - LE PUITS »).
  - **Menu** : la ligne CARTE est dans le menu principal **et** dans HEBERGER UNE PARTIE (`draw_map_row`).
  - **Rendu** : les nouvelles cartes passent par les lightmaps, ombres et bloom de `dev` (`load_map` reconstruit la
    liste d'affichage dans `rendu/eclairage.c`). Ciel et brouillard : l'Arène suit `graphismes/rendu.ini` et ses
    ambiances ; les autres cartes prennent `MapDef.sky` / `fog` (`map_fog_col`, `sky_color`). La ville
    (`skylineList`) n'entoure que l'Arène.
  - **Chute** : sur une carte fermée (`killY` −1000), la chute hors de la carte de `dev` (`FALL_DEATH_Y`) reste ;
    sinon `void_check` (avec crédit du kill). Les deux mettent `fellOut` (« TOMBE DANS LE VIDE », pas de caméra de mort).
  - **Second écran et radar** : l'emprise vient de `mapBox` (boîte englobante calculée par `build_level`), plus de
    `boxes[0]` supposé être le sol (`mapFirst`).

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

### 3.19 Navigation des bots (`src/bots/navigation.c`)

- **Graphe calculé tout seul** à chaque carte (`nav_build`, au premier besoin ou quand `curMap` / `nb` change) :
  nœuds **SOL** (dessus libre d'une boîte, une grille tous les `NAV_GRID` m sur les grands sols, un coin libre si le
  centre est couvert), **PAD** (dessus d'un jump pad), **ANCRE** (petit cube flottant sans sol dessous).
- **Arêtes** : MARCHE (sol continu, passage assez large pour le corps : `nav_fat_los`), SAUT (trou ≤ `NAV_JUMP`, marche
  ≤ `NAV_JUMP_UP`), PAD (point de chute balistique), GRAPPIN (vers un dessus plus haut, au-dessus d'un grand vide ou vers
  une ancre ; visée sur le bord : `nav_hook_point`), LÂCHER (d'une ancre vers un sol plus bas). Au plus `NAV_EMAX` par
  nœud : la plus chère est remplacée. Une arête qui bloque (immobile 0,8 s ou 6 s sans arriver) est retirée.
- **Suivi** (`nav_plan` toutes les 0,4 s, jamais en plein vol ; `nav_follow`) : course au sol puis bhop avec
  air-strafe ; pad : on revient dessus tant qu'il n'a pas lancé ; grappin : point visé figé au lancer, pas d'avance
  pendant la traction, lâcher au bord puis phase « atterrissage » (`navLanding`, double saut et air-control).
- **Dans `bot_think`** : le bot suit le graphe quand sa cible est cachée (> 0,35 s), quand il va chercher une
  recharge, quand le chemin commence par autre chose qu'une marche (un vide entre eux), en plein trajet, ou en ronde
  sans cible ; il **tire en route** (sauf pendant la traction). En vue sur le même sol, il se bat comme avant.
- **Garde-fou du vide** (`bot_void_guard`) : il laisse faire la navigation pendant un grappin, un pad ou un
  atterrissage prévus ; son grappin de secours (≥ 5 m) a priorité et annule le chemin.
- Journal : « navigation : graphe N nœuds, chemins, arêtes suivies par type, arrivées ». Test : trajets départ → but
  sur chaque carte avec les vraies commandes (« navigation : … arrive partout »).

### 3.20 Jauge de style (`src/jeu/style.c`)

- Rangs **D, C, B, A, S** (`STYLE_C/B/A/S`, plafond `STYLE_MAX`). Monte avec la vitesse au-delà de `STYLE_SPEED_MIN`,
  le wall-run et le grappin à pleine vitesse ; chaque kill (`STYLE_KILL`) avec bonus `STYLE_BONUS` : AERIEN, PLEINE
  VITESSE (≥ `STYLE_FAST`), TARZAN (au grappin), SUR LE MUR (< 1 s après un wall-run), RICOCHET, AU CONTACT, DOUBLE /
  TRIPLE / CARNAGE (< `STYLE_CHAIN_T`) ; parry `STYLE_PARRY` ; porte de course. Baisse de `STYLE_DECAY` +
  `STYLE_DECAY_RANK` par rang et par seconde (×1,6 à l'arrêt), et de `STYLE_HURT_K` par PV perdu. Remise à zéro à la mort.
- **Seulement en CHALLENGE** (mode COURSE : `style_on()` = `raceOn`) : en PvP et en solo par vagues, la jauge ne monte
  pas, n'est pas dessinee et `style_mult()` vaut 1 (choix du proprietaire, pour l'instant).
- Effets : points × `style_mult()` (1 ; 1,1 ; 1,25 ; 1,5 ; 2 ; sans effet hors CHALLENGE), musique +1 en A et +2 en S, rien sur
  l'équilibre PvP. Affichage `draw_style_hud` (lettre, mot, barre, derniers bonus). Branché dans `kill_enemy`,
  `apply_event` (`EV_KILL` à moi), `parry_feedback`, `game_tick`. Capture : `--shot style.bmp`.

### 3.21 Mode COURSE (`src/jeu/course.c`, `src/interface/course_hud.c`)

- Bouton **CHALLENGE** du menu principal → écran CHALLENGE (classements, §4 « Speedrun ») : COURSE d'une carte
  (`ACT_RACE`, `pendingArg` = la carte, `race_start_map`), SPEEDRUN COMPLET (`ACT_SPEEDRUN`). Aujourd'hui une seule
  carte CHALLENGE à portes
  (`MapDef.challenge`, `gates` x y z rayon, `ngates`, `startYaw` ; **LA TOILE**, `build_map_toile` : quatre avenues
  de gratte-ciel au-dessus du vide, 13 portes jusqu'au toit d'arrivée). Pas d'ennemis, pas de vagues. LA TOILE n'est
  jouable que là (§3.18, « Cartes CHALLENGE »).
- `race_start` : décompte `RACE_COUNTDOWN` immobile, puis chrono ; une porte compte quand le segment
  `prevPos → pos` (centre du corps) traverse sa sphère, dans l'ordre. Chute sous `killY` : `race_respawn` à la
  dernière porte passée (le chrono continue, compteur de chutes). **R** ou RECOMMENCER : `race_start`.
- **Record** : temps, temps intermédiaires (écart vert / rouge à chaque porte) et **fantôme** (position et
  orientation à 30 Hz, `RACE_GMAX`) dans `velocity_course.txt` à côté de `velocity.ini` pour la première carte
  CHALLENGE, `velocity_course_<carte>.txt` pour les suivantes (jamais en `--bot`, `--shot`, `--test`). Chaque
  arrivée part aussi au classement en ligne de la carte (`speedrun_report_splits`, §4 « Speedrun »). Le fantôme est dessiné en silhouette additive bleutée (`skinFlat`).
- **Façon Spider-Man** (seulement en course, `raceOn`) : 31 **points de balancement** (`addswing`, cubes dorés de
  2 × `SWING_HALF`, contour lumineux) au milieu des avenues, 15 à 20 m au-dessus des portes. **Aimant** : au lancer,
  le grappin part vers le point de balancement en vue le plus proche du viseur dans un cône de `RACE_MAGNET_COS`
  (`swing_target`, `swing_box` : cube ≤ 3,2 m avec rien juste dessous) ; un repère tournant montre lequel sera pris.
  **Balancier** : le grappin rembobine jusqu'à `RACE_ROPE` (18 m, le bas de l'arc tombe à hauteur des portes), puis
  traction × `RACE_SWING_PULL` et la corde ne s'allonge plus ; avancer pousse de `RACE_SWING_PUMP` le long de l'arc.
  Portes de 8 m de rayon, aucun passage obligé sous une passerelle.
- Affichage : anneaux des 3 prochaines portes (la suivante respire, avec une colonne de lumière ; l'arrivée est
  dorée), flèche au bord de l'écran si la porte est hors champ, chrono, PORTE k / n, écran d'ARRIVEE.
- Captures : `--shot course.bmp`, `course_depart.bmp`, `course_fin.bmp`. Test : portes libres, lignes droites
  porte à porte dégagées, porte sautée non comptée, chute → retour à la porte, arrivée, record, fantôme ; et une
  **course jouée par un pilote simple** (regarde la porte, grappin dès que l'aimant trouve un point, lâche dans la
  remontée) : il doit arriver en moins de 120 s (41 s, 1 chute). `RACEDBG=1` trace son trajet dans le fichier de test.

### 3.22 Objectifs des modes sur les cartes néon

LA CITADELLE et L'OASIS (cartes 6 et 7, pierre et enduit au soleil, décor maillé de ~180 000 triangles, salles de
départ, carte sans grappin) ont été **retirées** : elles sortaient de l'esthétique néon. Avec elles sont partis le
décor maillé (`src/monde/decor.c`, `dc_*`, `DEC_MAXV`, `decor_bake`), les boîtes invisibles `B_HIDDEN`, les textures
pierre / enduit (`texStone`, `texPlaster`, clés `pierre`, `enduit`, `decor_occlusion`, `pierre.bmp`, `enduit.bmp`),
l'éclairage par carte (`MapDef.style`, `MapDef.light`, `MAPL_*`, `map_light_apply`), les salles de départ
(`MapDef.spawnRooms`, `SPAWN_ROOM_DPS`), `MapDef.noHook` et le morceau « Citadelle ». `MAXB` est revenu à 256 (la plus
grosse carte, LA TOILE, a 134 boîtes) et la grille des rayons courts de l'éclairage à des indices sur 8 bits
(`LMG_MAXB` 48). `NET_VER` 17.

Les modes **3 ZONES**, **DRAPEAU** et **BOUCLIER** (§3.23) se jouent maintenant sur les **cinq cartes néon** (pas sur
LA TOILE, carte CHALLENGE, §3.21). Objectifs dans `src/monde/carte.c` (`areneZones`, `puitsFlags`…), branchés dans
`MapDef` : `zones` (A côté BLEU, B au centre, C côté ROUGE), `flags` (BLEU puis ROUGE), `shield` (bouclier des
BLEUS), `teamSpawn` / `nTeamSpawn` (départs par équipe, BLEUS puis ROUGES), `objScale` (échelle des rayons
`ZONE_R` et `SHIELD_ZONE_R` sur la carte : `zone_r()`, `shield_r()` dans `modes.c`). BLEUS du côté négatif de l'axe
des équipes.

| Carte | Axe (BLEUS) | Zones A / B / C | Drapeaux | Bouclier | Départs | Échelle |
|---|---|---|---|---|---|---|
| ARENE | z < 0 (nord) | plateformes hautes nord / sud (0, 7, ∓50,4), B au sol à l'est (32, 0, 0) entre le pad et la plateforme haute est | (0, 0, ∓70), fond de chaque moitié | plateforme haute nord | 9 par camp, à l'écart des blocs | 1 |
| LE PUITS | x < 0 (ouest) | balcons (∓7, 25, 0), B au sommet (0, 50, 0) | 3e et 7e plateformes de la spirale, du côté opposé au pad (∓16, 12 / 36, ±17,8) | balcon ouest | 4 par camp, coins des plateformes | 0,6 |
| LE CANYON | x < 0 (ouest) | falaises entre les deux abris sud (∓58, 20, −20), B sur le plateau central (0, 10, 0) | (∓70, 20, 0), derrière la tour de tir | falaise ouest, entre les abris | 6 par camp | 1 |
| LA BOUCLE | x < 0 (ouest) | toit du bloc central (∓32, 10, 0), B dans le passage sous le toit de la coupe (0, 0, 0) | (∓57, 0, 0), bout de l'anneau entre le pad du toit et le mur | toit ouest | 6 par camp | 1 |
| LE DOJO | z < 0 (nord) | (0, 0, ∓9) entre le ring et le mur, B sur le ring | coins opposés (∓16, 0, ∓16), deux couloirs d'accès | (0, 0, −9) | 5 par camp | 0,8 |

- **Géométrie ajoutée** : LE CANYON a un **plateau central** de 20 × 16 m à y 10 (dessus néon cyan) sous les îlots
  centraux, avec deux pads vers le bord de chaque falaise (sans grappin, on peut enfin traverser : îlot central →
  plateau → pad) ; LE PUITS a deux **balcons** de 12 × 12 m à y 25 au centre du puits (zones A et C, bouclier), à la
  même hauteur des deux côtés ; on y monte au grappin ou en se laissant tomber du sommet.
- **Symétrie** : miroir sur l'axe des équipes (Arène, Canyon, Boucle, Dojo ; drapeaux du Dojo en demi-tour) ; la
  spirale du Puits ne peut pas être symétrique : ses paires sont symétriques par demi-tour autour de l'axe du puits et
  les départs des deux camps ont la même hauteur moyenne (19,5 m).
- **Règles vérifiées** (test « objectifs ») : chaque point au-dessus d'un sol à moins de 1,5 m (pas un pad), hors des
  boîtes, au-dessus de `killY` ; sur les cartes ouvertes, du sol à 3 m tout autour ; disque des zones et du bouclier
  entièrement sur un sol plat et libre, aucun pad dedans ; zones à plus de deux rayons l'une de l'autre, drapeaux à
  plus de 40 m ; chaque objectif du côté de son camp ; symétrie ; au moins 4 départs par camp ; chemin des bots
  (`nav_path`) de chaque camp vers chaque objectif, avec et sans grappin.
- **Navigation** : une arête SAUT vers un sol plus bas n'exigeait que la ligne droite ; quand elle traverse son propre
  sol (descendre du sommet du Puits vers un balcon), le graphe essaie maintenant « courir jusqu'au bord puis tomber »
  (`nav_edge_exit`).
- Captures : `--shot objectifs_zones.bmp --map N` (aussi `_drapeau`, `_bouclier` ; `--pos x,y,z[,inclinaison]`,
  `--yaw A`) dessine les objectifs du mode sur la carte.

### 3.23 Règles de partie, modes, boutique, spécialisations

- **Règles** (`MatchRules rules` ; `wantRules` = choix du panneau RÈGLES de HEBERGER, `regles=` dans `velocity.ini`,
  `--regles mode,grappin,munitions,boutique,specs,minutes` pour les bots) : mode, grappin oui/non (`hook_allowed()`),
  munitions normales / ×2 / infinies (`ammo_rule_mul`, `game_tick`), boutique, spécialisations,
  durée (3 à 15 min). L'hôte les copie au démarrage (`net_host_start` : mode indisponible sur la carte → FRAGS,
  mode d'équipe sans équipes → 2 contre 2) et les envoie dans chaque `PkSnap` (`rMode`, `rFlags`, `rMinutes`, `rAmmo`).
- **Modes** (`src/reseau/modes.c`, l'hôte fait autorité, état `ObjState obj` dans `PkSnap`, messages `EV_OBJ`) :
  FRAGS (comme avant) ; CHRONO (pas de limite de frags, le plus de kills au temps) ; **3 ZONES** (`MapDef.zones`, rayon
  `zone_r()` = `ZONE_R` × `objScale` :
  une seule équipe dedans prend la zone en `ZONE_CAP_T`, plus vite à plusieurs, contestée = bloquée ; chaque zone
  tenue rapporte 1 point par seconde, `ZONE_SCORE` pour gagner) ; **DRAPEAU** (`MapDef.flags` : l'adversaire le
  prend en le touchant, le porteur qui meurt le laisse tomber sur place (`mode_on_death`), un coéquipier du porteur
  peut le reprendre ; un défenseur qui reste à côté `FLAG_RETURN_T` sans être touché le rend (une touche remet le
  retour à zéro) ; capture = le porteur touche son propre drapeau resté à la base ; `FLAG_CAPS` pour gagner ;
  oublié `FLAG_AUTO_RETURN` = rendu) ; **BOUCLIER** (`MapDef.shield`, rayon `shield_r()` : les ROUGES le surchargent en restant dessus
  sans BLEU présent, les BLEUS gagnent si le temps s'écoule). L'hôte garde les progressions en décimal
  (`zoneProg`, `flagRet`) : l'octet de `ObjState` ne sert qu'à l'envoi.
- **Cartes** : les cinq cartes néon ont les trois objectifs et des départs par équipe (§3.22) ; LA TOILE (CHALLENGE)
  n'a aucun mode (`mode_ok` = 0).
- **Bots** : vont à l'objectif (`mode_bot_goal` : zone pas à eux, drapeau à voler / rendre / ramener, bouclier),
  le tiennent sur place, se battent si un ennemi est vu de près. **Dans HEBERGER, pas de bots en 3 ZONES, DRAPEAU
  et BOUCLIER** (modes entre joueurs pour l'instant) ; FRAGS et CHRONO les gardent.
- **Boutique** (`src/jeu/economie.c`, chaque joueur pour lui-même) : `ECO_START` pièces, `ECO_KILL`, `ECO_DEATH`,
  `ECO_OBJ` (objectif personnel), `ECO_TEAM_OBJ`. Début de match : choix d'une arme parmi 4 (`ecoStarters`, 10 s,
  souris libérée, immobile) ; à chaque mort, la boutique s'ouvre : acheter une arme
  (`eco_price`) l'équipe en 2e place, inverser les armes, MUNITIONS + (jauges pleines au respawn). Les armes
  achetées restent pour le match. Bots : `eco_bot_shop`.
- **Spécialisations** (LOADOUT, `spec=`, envoyée dans `NetPState.spare[0]`) : ASSAUT (+15 % de dégâts, hôte),
  REMPART (−20 % de dégâts subis, hôte ; un peu plus lent), ÉCLAIREUR (+12 % de vitesse, 3 charges de dash),
  SOUTIEN (régénère hors combat, soigne les alliés proches, hôte), ARTIFICIER (jauges +50 %). Actives en solo, et en
  multi si l'hôte les autorise.
- `NET_VER` 15 (16 depuis le mode classé, 17 depuis le retrait de LA CITADELLE et L'OASIS, §4). Captures :
  `--shot boutique.bmp`, `--shot armedepart.bmp` (Arène en 3 ZONES), `objectifs_*` (§3.22). Tests : zones, drapeau,
  bouclier (sur l'Arène), spécialisations, SOUTIEN, boutique, objectifs des cartes, cartes CHALLENGE, rotation.

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
- La géométrie n'est **pas** transmise, seulement l'**id de carte** (`PkWelcome.pad[0]`, `EV_MATCH.target`, §3.18) :
  chaque exécutable la reconstruit (`build_level(m)` est déterministe, graine `rng = 777` pour l'Arène). Tous les
  joueurs doivent donc avoir le même exécutable.
- **Aimant de visée** (`aim_assist`, `src/bots/bot.c`) : uniquement par la compétence **AIMANT** (`SK_AIM`), pour
  tout le monde, PvP et PARTIE CLASSEE compris : aucune aide passive. L'équilibrage passe par la durée courte et la
  longue recharge (`AIM_T`, `AIM_RECHARGE`), et l'activation est visible des adversaires.

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
  3. **tête et chaîne pleine** (drapeaux `HF_*` dans `EV_HIT.pad[0]`) : `head_host_ok` vérifie que le point d'impact
     déclaré (`f[4..6]`) est sur la tête de la cible telle que l'hôte la voit (`HEAD_HOST_TOL`, `HEAD_HOST_LAG` ;
     bien plus strict en hauteur) ; `chain_host_claim` vérifie la chaîne : l'hôte tient une **chaîne miroir** par
     joueur (`hostChain`, +1 par touche d'une arme de chaîne qu'il arbitre, +2 à la tête validée, ramenée à la valeur
     annoncée dans `NetPState.spare[1]` quand celle-ci baisse : un plafond de la vraie chaîne), et exige qu'elle soit
     pleine **et** vue pleine dans l'état du tireur depuis moins de `CHAIN_SEEN_T`. Un drapeau refusé est retiré de
     l'événement relayé (journal : « tete refusee », « chaine pleine refusee ») ;
  4. sinon retire `min(dégâts, PVP_HIT_CAP)` PV (`W_SELF` n'est pas plafonné). **Seules exceptions au plafond**
     (`host_apply_damage_fl`) : une touche à la tête validée (rail, Élan) n'est pas plafonnée, et un **one-shot**
     (rail à la tête, ou tir chaîne pleine validé) retire au moins tous les PV restants, spécialisations comprises ;
  5. à 0 PV : `host_kill(tueur, victime, arme)` ; `EV_KILL.pad[0]` porte `HF_HEAD` / `HF_CHAIN` / `HF_ONESHOT`.
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
`ttkSum`/`ttkN` (temps pour tuer), `oneShots`, `meleeParries`, `clashes` ; tirs à la tête par arme (`headHostHits`),
one-shots à la tête / chaîne pleine (`headOneShots`, `chainOneShots`) et refus (`headRefused`, `chainRefused`), dans la
ligne « tetes (arbitre) » du journal.

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
- **Client** (`src/reseau/en_ligne.c`, inclus après `client.c` qui définit `net_connect_ex`) :
  - fil WinHTTP (`lb_reader`) qui lit le WebSocket et fil d'écriture (`lb_writer`, `ping` après 25 s de silence ;
    en pratique la demande d'heure `time` de l'horloge de match, toutes les 24 s, en tient lieu),
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

Pas de compte : le classement est tenu par le **serveur de classement** (le Worker et sa base **D1 `leaderboard`**,
binding `LEADERBOARD`), qui calcule lui-même l'Elo à partir des rapports de fin de match. L'hôte fait autorité sur le déroulement du match
(frags, gagnant, joueurs partis), comme pour le reste du jeu.

**Où est le code** : `src/reseau/classe.c` (état, formule, paliers, identité, cartes et règles imposées) et
`src/reseau/classe_match.c` (suivi du match, rapports de fin, `rk_tick`) ;
`src/reseau/classe_serveur.c` (requêtes au serveur en fil de fond, réponses JSON), sur `http_request` de `src/plateforme/http.c` ; paquets dans `src/reseau/protocole.c`, accueil et fin de
match dans `hote.c`, réception dans `client.c`, salon classé du lobby et `ranked_play` dans `en_ligne.c` ; clés de
`velocity.ini` dans `src/interface/config.c` ; bouton et encadré TOP 3 dans `menus.c`, colonne CLASSE et résultat
dans `scores.c`, titre « PARTIE CLASSEE » dans `hud.c` ; `hello` au démarrage et options `--ranked` / `--frags`
dans `src/plateforme/demarrage.c` ; tests dans `src/tests/tests.c`.

**Identité** — au premier lancement, `rk_identity` tire `ranked_id` (128 bits) et `ranked_key` (128 bits secrets,
`RtlGenRandom` sous Windows, `arc4random_buf` sous macOS), écrits dans `velocity.ini`. Le serveur enregistre le SHA-256
de la clé au premier contact (confiance au premier usage) ; toute requête ensuite doit présenter la clé (sinon 403).
Le pseudo peut changer. Les bots (`--bot … --ranked`) utilisent une identité temporaire (jamais sauvegardée).

**Requêtes HTTP** (`rk_http` dans `src/reseau/classe_serveur.c`, sur `http_request` de `src/plateforme/http.c`, une à la fois dans un fil : WinHTTP sous Windows, `curl` du système sous macOS ; même
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

**Formule (référence unique, appliquée par `worker/src/ratings.js` ; `rank_delta` dans `src/reseau/classe.c` est
identique)** :
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
   champ `PkAnnounce.ranked`). Rejoindre une partie classée sans serveur de classement est refusé.
2. **Règles figées** : une partie classée est toujours en **chacun pour soi** (`teamSize` = 0, quel que soit
   `wantTeams`), avec les **règles par défaut** (`MatchRules` : FRAGS, grappin autorisé, munitions normales, sans
   boutique ni spécialisations, 10 min ; `wantRules` est ignoré), limite `FRAG_LIMIT`, `MATCH_TIME`, et **sans
   bots** (ni `vsBot`, ni `hostBots`) ; les spectateurs ne comptent pas. La carte doit avoir un vrai match à mort :
   `rk_map_pick` saute les cartes CHALLENGE (`rk_map_ok` = `map_pvp` : LA TOILE). Un match ne compte que si `rk_counts()` (classée,
   chacun pour soi, mode FRAGS). L'hôte refuse (`PkWelcome.refused = 2`) un joueur sans identité de classement.
   Le premier adversaire qui arrive relance le match (`host_start_match`) ; si tous les adversaires partent, l'hôte
   gagne (`host_end_match`).
3. **Réseau** : `PkJoin` porte `rid` (16 octets), `mmr` et `rplay` (cache) ; l'hôte les diffuse dans `PkRoster`
   (`mmr[]`, `rplay[]`, `rid[]`, `ranked`, `mid` = identifiant du match tiré par l'hôte à chaque match, `left[]` /
   `nleft` = joueurs partis pendant le match, listés par l'hôte). `PkWelcome.pad[1]` = partie classée
   (`pad[0]` = carte) ; `PkAnnounce.ranked` = partie classée, à côté de `teams` et `map`.
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

**Base D1** (`worker/src/ratings.js`, `speedrun.js`) : tables `players`, `matches`, `reports`, `rate`, `sr_tickets`,
`sr_best`, créées au premier usage par chaque instance du Worker (`CREATE … IF NOT EXISTS`, `ensureSchema` dans
`index.js` : aucune migration D1 à lancer). D1 n'a pas de transaction interactive et plusieurs instances du Worker
répondent en même temps (le Durable Object d'avant sérialisait tout) : chaque écriture sensible tient en une requête.
`matches.done` vaut 0 (en attente), **2** (calcul en cours : seule la requête qui fait passer 0 → 2 applique l'Elo, les
autres répondent `pending` et le jeu redemande) puis 1 (appliqué, résultat gardé) ; les cotes changent en relatif
(`mmr = MIN(MAX, MAX(MIN, mmr + Δ))`, `played + 1`) pour qu'un autre match du même joueur ne soit pas écrasé ; le
compteur de débit est un upsert ; un ticket speedrun est lu et effacé d'un coup (`DELETE … RETURNING`) et un meilleur
temps n'est remplacé que par plus rapide (`ON CONFLICT … WHERE`). En cas d'erreur D1, réponse 503 « base
indisponible » (le jeu affiche « classement indisponible »). `dev-server.js` imite l'API D1 (`memoryD1`, au-dessus de
`node:sqlite`) en rendant la main à chaque requête, ce qui permet à `node test.js` de vérifier ces cas simultanés.
Consulter la base en ligne : `npx wrangler d1 execute leaderboard --remote --command "SELECT name, mmr, played FROM
players ORDER BY mmr DESC LIMIT 10"`.

**Limites (pas de vrai anti-triche sans serveur de jeu)** : un hôte truqué peut mentir sur les frags ou déclarer
parti un client en 1 contre 1 (l'hôte fait déjà autorité sur tout le match) ; plusieurs identités jetables peuvent
s'arranger entre elles (pas de compte) ; en 1 contre 1, un hôte qui quitte n'est pas sanctionné (un client seul ne
peut pas le déclarer parti) ; la cote annoncée dans `PkJoin` (affichage, matchmaking) est déclarative.

Tests : lignes `classe …` de `run_tests()` (`src/tests/tests.c` : Elo, abandon, paliers, placement, identité, `velocity.ini` de test,
lecture du TOP, construction du rapport) ; `node test.js` (lobby classé, Elo, enregistrement, mauvaise clé,
concordance, désaccord, rapport seul, abandon, hôte parti, expiration, rapports simultanés, deux matchs simultanés
du même joueur, top, limite de débit) ;
`--shot ranked.bmp` (fin de match classée) et `--shot menu.bmp` (encadré TOP 3 avec données factices).

### Speedrun : classements des meilleurs temps

Trois familles de classements, tenus dans la **même base D1 `leaderboard`** que le mode classé (même table
`players`, même identité `ranked_id` / `ranked_key`) : le **serveur fait foi**.

| Catégorie (jeu) | Clé (serveur) | Temps intermédiaires | Plancher | Ticket valable |
|---|---|---|---|---|
| `SR_MAP0 + m` : course de la carte CHALLENGE `m` | `course:<carte>` (`course:la_toile`) | un par porte (13) | 6 975 ms | 30 min |
| `SR_COMPLET` : toutes les cartes CHALLENGE à la suite | `complet` | un par carte (1 aujourd'hui) | somme des courses | 2 h |
| `SR_VAGUES100` : mode VAGUES, vague 100 terminée | `vagues100` | 0 ou 4 (vagues 25 / 50 / 75 / 100) | 5 min (provisoire) | 8 h |

**Où est le code** : `src/jeu/speedrun.c` (catégories `sr_key` / `sr_label` / `sr_cat_ok`, planchers `sr_floor_ms`,
speedrun complet `sr_full_*`, état `srRun[]` / `srTop[]`), `src/reseau/speedrun_serveur.c` (ticket, rapport,
classement, sur la file de requêtes du mode classé : `rk_push_arg`, `RQ_SR_*`), déclarations à la fin de
`src/reseau/etat_partage.c`, record perso du speedrun complet dans `src/interface/course_hud.c`
(`velocity_speedrun.txt`), écran CHALLENGE dans `menus.c` (`draw_challenge_menu`, `draw_sr_board`) ; serveur :
`worker/src/speedrun.js`.

**Point d'entrée** (appelable de partout, déclaré dans `etat_partage.c`) :
```c
speedrun_begin(SR_VAGUES100);                            /* au depart du run : ticket horodate par le serveur */
speedrun_report(SR_VAGUES100, ms);                       /* a l'arrivee : temps de jeu en ms */
speedrun_report_splits(SR_VAGUES100, ms, splitMs, 4);    /* ou avec les temps intermediaires cumules (dernier = ms) */
```
Rien ne part en `--bot` / `--shot`, hors ligne (`online=0`) ou sans identité. Un temps fini avant l'arrivée du
ticket attend le ticket (`SRR_WAIT`) ; sans ticket (serveur injoignable au départ), il reste « garde sur cette
machine ». Un nouveau `speedrun_begin` de la même catégorie (R) remplace le ticket : la réponse d'un ancien départ
est ignorée (numéro de séquence dans `RkReq.arg`).

**Requêtes HTTP** (même hôte que le lobby, `lobbyAddr`) :

| Requête | Corps / paramètres | Réponse |
|---|---|---|
| `POST /speedrun/start` | `{id, key, name, cat}` | `{ok, run, cat}` (crée l'identité si nouvelle ; annule le ticket précédent de la catégorie) |
| `POST /speedrun/report` | `{id, key, cat, run, ms, splits:[ms cumulés]}` | `{ok, state:"record"\|"kept", best, prev, rank}` ou `{ok:false, err}` |
| `GET /speedrun/top?cat=…&id=…` | — | `{ok, cat, runners, top:[{name, ms}] (10), wr:{name, ms, splits}, me:{rank, ms, splits}}` |

**Validation (anti-triche minimale, `runError` et `report` dans `speedrun.js`)** :
- le **ticket** : le rapport doit citer le dernier ticket demandé par ce joueur pour cette catégorie ; il ne sert
  qu'une fois (même refusé), expire (`maxAgeMs`), et **le temps annoncé ne peut pas dépasser le temps réel écoulé
  depuis le ticket** (+ `TOL_MS` = 1,5 s) : un speedhack ou un temps inventé sans avoir joué aussi longtemps est
  refusé (« plus rapide que le temps reel ») ;
- catégorie connue, nombre de temps intermédiaires attendu, temps strictement croissants (troncons ≥ `SEG_MIN_MS`
  = 250 ms), le dernier égal au total ;
- **plancher** : chemin le plus court de porte en porte (du départ à la 1re porte, chaque tronçon raccourci du
  rayon de la porte d'arrivée) à `SR_VMAX` = 45 m/s de moyenne (le grappin plafonne à 38–46 m/s). Calculé par le jeu
  (`sr_floor_ms`) et **recopié** dans `SR.CATS` : la ligne « speedrun planchers » de `--test` les affiche ;
- la requête est authentifiée par la clé du joueur, comme le mode classé (le serveur ne garde que son SHA-256) : une
  signature HMAC n'ajouterait rien tant que la clé elle-même circule (en HTTPS) ;
- débit : 60 requêtes/min par identité, compteur à part du mode classé.

Limites : pas de rejeu de la course côté serveur, donc un client modifié qui joue en temps réel peut annoncer un temps
plausible (au-dessus du plancher) qu'il n'a pas fait ; le temps de jeu ne compte pas les décomptes, ce qui laisse
3 s de marge au contrôle du temps réel.

**Nouvelle carte CHALLENGE** : ajouter sa catégorie `course:<carte>` dans `SR.CATS` (plancher donné par `--test`),
ajouter son plancher à celui de `complet`, et passer `complet.splits` au nouveau nombre de cartes ; redéployer.

**Affichage** :
- bouton **CHALLENGE** du menu principal → écran CHALLENGE (`challengeOpen`) : une ligne par catégorie (COURSE de
  chaque carte, SPEEDRUN COMPLET, VAGUE 100) avec mon record, et à droite le classement de la catégorie choisie
  (`draw_sr_board`) : record du monde et son auteur, top 10 avec l'écart au premier (podium or / argent / bronze,
  ma ligne surlignée), « toi : #rang sur N - meilleur temps », mon record sur cette machine. Les classements sont
  demandés à l'ouverture (`rq_sr_top_all`) et après chaque temps accepté ; on revient sur cet écran en quittant une
  course ;
- écran d'ARRIVEE d'une course : ligne CLASSEMENT (`sr_result_line` : envoi…, « #3 sur 37 - record perso
  enregistre », « ton meilleur temps reste… », refus avec la raison, hors ligne).

**Speedrun complet** (`ACT_SPEEDRUN`, `sr_full_start`) : les cartes CHALLENGE à portes dans l'ordre de `maps[]`
(`srMaps`). Chaque carte garde son décompte (non compté), son fantôme et son record, et son temps part aussi à son
propre classement. **Chrono global** = somme des temps de course (`sr_full_ms`), temps cumulé à chaque arrivée
(`srSplit`), écart vert / rouge au record perso (`srPb`, `velocity_speedrun.txt` ; à défaut, les temps
intermédiaires de mon meilleur run au serveur). Après une arrivée, `SR_NEXT_DELAY` (2 s) puis la carte suivante
(`sr_full_tick`, appelé par `game_tick`). **R** / RECOMMENCER repart de la première carte. Panneau en haut à gauche
(`draw_sr_full_panel`), bilan final (`draw_sr_full_end`) avec le temps de chaque carte et le classement `complet`.
Avec une seule carte CHALLENGE (LA TOILE), le speedrun complet est cette course seule, mais son classement est à part.

Tests : lignes `speedrun …` de `run_tests()` (catégories, planchers, ticket puis rapport, ticket remplacé par R,
réponse record / refus, hors ligne, lecture du classement, speedrun complet sur deux cartes : enchaînement, décompte,
chrono global, record perso, R) ; `node test.js` (lignes `speedrun :` : ticket, temps réel, ticket consommé /
remplacé / d'un autre joueur / d'une autre catégorie / expiré, record gardé, rang, plancher, temps intermédiaires,
vague 100, complet, top par HTTP, limite de débit) ; captures `challenge`, `course_fin`, `speedrun`, `speedrun_fin`.
**De bout en bout** (jeu ↔ worker réel, HTTP et JSON compris) : lancer un lobby local (`cd worker && node dev-server.js 8791`)
puis `--test` avec la variable `SPEEDRUN_LOBBY=127.0.0.1:8791` : ligne « speedrun de bout en bout » (ticket, temps trop
tôt refusé, vrai temps de course de 7,6 s puis record rang 1, classement relu). Sans la variable, `--test` reste hors ligne.

### `NET_VER`

`NET_VER` est comparé à chaque paquet reçu. **Incrémentez-le à chaque changement** de structure de
paquet (`PkHead`, `NetPState`, `NetEvent`, `Pk*`, `SnapP`), de signification d'un champ, d'ajout d'un
`EV_*` ou d'un `PK_*`, et aussi de **carte** (les index de cellules et `pickMask` doivent coïncider).
Une version différente rend simplement les parties invisibles l'une à l'autre.

Historique récent : 12 équipes ; 13 skin dans `NetPState` (dev) / carte et `W_VOID` (dépôt original) ; 14 fusion des
deux ; 15 règles de partie et objectifs (`PkSnap` : `rMode`…, `obj` ; `EV_OBJ` ; spécialisation dans `NetPState`) ;
**16** fusion avec le mode classé du dépôt original (qui avait pris 14 de son côté) : `PkJoin` (`rid`, `mmr`,
`rplay`), `PkRoster` (cotes, identifiants, `ranked`, `mid`, partis), `PkAnnounce.ranked`, `PkWelcome.pad[1]` et
`refused = 2` ; **17** cartes LA CITADELLE et L'OASIS retirées (ordre de `maps[]`), objectifs sur les cartes néon
(plateau du Canyon, balcons du Puits) ; **18** horloge de match commune (`PkSnap.clk`, `clkSrv`) ; **19** tirs à la tête
et chaîne pleine (`EV_HIT.pad[0]` = `HF_*`, `f[4..6]` = point sur la tête, `EV_KILL.pad[0]`, `NetPState.spare[1]` =
chaîne de tirs) et onde de l'Enclume du plan 1b (`EV_SLAM` : centre, rayon, hauteur du mur, cap et vitesse de l'ovale,
`actor` = |vy|). La valeur fait foi dans `src/reseau/protocole.c`.

### Horloge de match commune (`src/reseau/horloge.c`)

Le chrono du match (`matchT`) suit le **temps réel**, plus les ticks : sous 10 images/s (le `dt` de la boucle est
plafonné à 0,1 s) ou pendant le `hitstop`, la simulation ralentit mais pas le chrono.
- **Hôte** : `match_clock_tick()` (dans `host_tick`) ajoute le temps réel écoulé depuis le tick précédent ; un seul
  tick par image le reçoit, un tick sauté (hitstop) est rattrapé au suivant. Plus de `CLK_LOAD` (1 s) d'un coup =
  chargement de carte, compté 1 s.
- **Heure de référence** : celle du worker du lobby. Le jeu envoie `time`, le serveur répond `time <ms>`
  (`Date.now()`) ; `clk_srv_sample` garde la mesure à l'aller-retour le plus court (décalage = heure serveur − milieu
  de l'aller-retour ; la mesure gardée « vieillit » de 2 ms à chaque nouvelle pour suivre une dérive). 6 mesures à
  0,5 s d'écart à la connexion au lobby, puis une toutes les 24 s, qui remplace le `ping` de maintien (même coût).
  Un ancien worker répond `err cmd` : pas de calage, rien ne casse.
- **Instantanés** : `PkSnap` porte `matchT`, `clk` (heure de l'hôte à l'envoi, `clk_now()`) et `clkSrv` (hôte calé
  sur le serveur). Le client calcule `matchT` de l'hôte + le trajet du paquet : si les deux sont calés sur le
  serveur, trajet = son heure serveur − `clk` ; sinon (LAN, lobby injoignable) il se cale sur l'horloge de l'hôte,
  en gardant le plus grand `clk − nowT` reçu (le trajet le plus court). Trajet borné à [0, 1 s] ; match fini :
  chrono figé à la valeur de l'hôte.
- Journal : `horloge : calee sur le serveur (aller-retour N ms)`. Test : « horloge de match ».

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
| `PVP_HIT_CAP` | 60 PV | en PvP, une touche ne retire jamais plus de 60 PV, sauf à la tête (rail, Élan) et un one-shot (rail à la tête, chaîne pleine), validés par l'hôte |
| `MELEE_PARRIED_CD` | 1 s | épée bloquée pour l'attaquant dont le coup est paré |
| `MELEE_PARRY_GRACE` | 0,15 s | tolérance de latence de l'hôte pour le parry d'épée |

### Chaîne de tir

| Nom | Valeur | Effet |
|---|---|---|
| `STREAK_MAX` | 8 | longueur maximale prise en compte |
| `STREAK_DMG` | 0,15 | +15 % de dégâts par touche successive |
| `STREAK_RATE` | 0,05 | +5 % de cadence par touche successive |
| `CHAIN_HEAD_STEP` | 2 | une touche à la tête fait avancer la chaîne de 2 |
| `CHAIN_SEEN_T` | 0,5 s | hôte : chaîne vue pleine dans l'état du tireur depuis moins de 0,5 s pour valider un one-shot |
| `HEAD_R` / `HEAD_DROP` | 0,24 / 0,26 m | sphère de tête : rayon, centre sous le haut du joueur |
| `HEAD_MULT_RAIL` | 1,5 | rail à la tête : ×1,5 et one-shot |
| `HEAD_MULT_KIN` | 1,35 | Élan à la tête : ×1,35 sans plafond PvP |
| `HEAD_ENEMY_FRAC` | 0,45 | point faible des ennemis : au-dessus de 45 % du rayon |
| `HEAD_HOST_TOL` / `HEAD_HOST_LAG` | 0,6 m / 0,15 s | hôte : tolérance du point d'impact déclaré sur la tête |
| `HEAD_BOT_OFS` | 0,62 m | bots : hauteur visée au-dessus du centre quand ils tentent la tête |

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
| `MAXB` | 256 | boîtes de la carte (`boxes`) ; indices sur 8 bits dans la grille de l'éclairage |
| `MAP_S` | 1,4 | échelle horizontale de la carte (`ms()`) ; les hauteurs ne changent pas |
| `MAXPICK` | 16 | cellules de recharge (limité aussi par les 16 bits de `pickMask`) |
| `SPEED_ACCEL` / `SPEED_MAX` | 30 m/s² / 28 m/s | bande d'accélération `B_SPEED` (La Boucle) |
| `VOID_CREDIT_T` | 4 s | chute dans le vide après une touche : kill à l'attaquant (§3.18) |
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
| `NET_VER` | 19 | version du protocole (historique : §4, « `NET_VER` ») ; 15 = règles et objectifs, 16 = mode classé, 17 = cartes néon seulement, 18 = horloge commune, 19 = tirs à la tête, chaîne pleine, onde de l'Enclume |
| `RANK_*` | voir §4 | mode classé : `RANK_START` 1000, bornes 100 à 3000, `RANK_K` 32, `RANK_K_PLACE` 64 sur `RANK_PLACEMENT` 10 matchs, paliers, `RANK_END_WAIT`, `RANK_RETRY`, `RANK_TRIES` |
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
- Équipes et éclairage : lignes `equipes …` (arrivée équilibrée, tir allié annulé, cibles, spawns, victoire
  d'équipe, snapshot, rééquilibrage) et `eclairage …` (ombre de la tour). Match en équipes :
  `--bot host --teams 2` puis 3 `--bot join` (pour ne pas publier sur le vrai lobby : `--lobby 127.0.0.1:9`).
- Scénarios actuels : escalier, jump pads est et nord, bhop + air-strafe, rocket jump, grappin vers un
  mur, lâcher en vol, grappin sur un drone, épée à 0 et 20 m/s, parry d'une orbe, chaîne de tir,
  recharges, pièces (1 et 2 ricochets), vitesse du grappin, puis un **LAN simulé** sans socket
  (`netMode = NET_HOST`, `botMode = 2`, deux `NP`) : cross-up du bot, épée du bot, coup dans le vide,
  ping-pong de roquette, plafond PvP, parry d'épée, aller-retour de `velocity.ini` (dans
  `cfg_test.ini`, supprimé ensuite), contre-jeu sur une pièce adverse.
- En fin de fichier : analyse d'une réponse STUN, adresses `ip:port`, lignes `room` / `gone` du lobby
  (`lb_line`), places de spectateurs (`MAXSPEC` acceptés, un retour ne prend pas une 2ᵉ place).
- Le lobby a ses propres tests : `cd worker && node test.js` (54 vérifications avec le serveur de classement, port 8799).
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
| `graphismes` | menu principal + PARAMÈTRES > GRAPHISMES, aide du bouton QUALITE survolé (aucun autre nom de scène ne contient « graphismes ») |
| `menu` | menu principal |
| `dm` | scène de deathmatch factice : joueurs « Rival » (grappin, épée) et « Ninja », fil des kills, HUD LAN |
| `vagues` | mode VAGUES (testé en premier) : `_pyramide` (`_charge`), `_tesseract`, `_boule`, `_hexaedre`, `_boutique`, `_selecteur`, `_fin`, `_infini`, sinon une vague ordinaire (§3.5) |
| *(autre)* | combat solo scripté : vague 3, 8 ennemis, tirs rail et roquette, grappin sur la tour, HUD des deux axes |
| `rail` (avec la scène par défaut) | la même, arme en main = railgun |
| `impact` | kill au rail à pleine vitesse, capturé pendant l'impact frame |
| `fx` | idem, 14 ticks plus tard sans l'impact frame (éclats, chiffres de dégâts, lignes de vitesse) |
| `arene` / `arene2` | vue d'ensemble de la carte (lightmaps, ombres portées, ciel) |
| `heberger` | écran HEBERGER UNE PARTIE (2v2, bots normaux) |
| `dm_equipes` | scène de deathmatch en 2v2 (HUD d'équipes, allié marqué) ; `_fin` : fin de match ; `_bas` : regard vers le sol (ombres des joueurs) |
| `sansbloom` / `sansombre` | (à combiner) coupe le bloom / les ombres dynamiques pour comparer |
| `ranked` | fin de match classée : colonne CLASSE, « +18 MMR -> OR II » (`draw_rank_result`) |
| `menu` (aussi) | l'encadré CLASSEMENT / TOP 3 est rempli de données factices |
| `challenge` | écran CHALLENGE : classement factice de la course (top 8, ma ligne, record du monde) |
| `speedrun` / `speedrun_fin` | speedrun complet en cours (panneau, chrono global) / bilan final ; `course_fin` montre aussi la ligne CLASSEMENT |

Autres noms reconnus (voir `src/plateforme/demarrage.c`, chercher `strstr(shotPath`) : `loadout` (`_charge`),
`update`, `skinmenu`, `skins` (`_course`), `small` (fenêtre 1024 × 640), `carte` / `carte3d` (second écran, avec
`_dm`, `_equipes`, `_fin`), `arene_nuit` / `_jour` / `_orage`, `dm_mort`, `dm_<arme>` (`enclume`, `lance`, `fleau`,
`poings`, `egide`, `eventail`, `tesla`), `skills`, `bench_dm` (mesure), `boutique`, `armedepart`, `objectifs_zones` /
`objectifs_drapeau` / `objectifs_bouclier` (objectifs du mode sur la carte `--map N`, §3.22), `course`
(`_depart`, `_fin`, `_aimant`), et dans la scène solo : `changearme`, `vie`, `style`, `elan`, `enclume`, `lance`,
`fleau` (`_vide`), `poings` (`_upper`), `egide` (`_bas`), `scie` (`_main`), `tesla`, `eventail`.

Options après le nom : `--map N` (carte, §3.18), `--yaw A` (orientation) et `--pos x,y,z[,inclinaison]` (point de vue,
cartes autres que l'Arène et `objectifs_*`). Sur une carte autre que l'Arène
(hors `menu`/`settings`/`graphismes`/`dm`), la scène est une vue de la carte depuis la caméra du menu ; le nom peut contenir
`spawn` (depuis le premier point de départ), `chute` (3 m au-dessus de `killY` en tombant : plan rouge et
vignette) ou `solo` (avec 8 drones). Exemple : `velocity.exe --shot puits.bmp --map 1 --yaw 0.8`.

Pas de son, `velocity.ini` n'est ni lu ni écrit.

### `--export-textures dossier` (textures générées en `.bmp`)

```bash
./velocity.exe --export-textures graphismes
```

Sans fenêtre ni OpenGL : lit `graphismes/rendu.ini` (`gfx_load_all`), calcule chaque texture générée
(`tex_generate`) et l'écrit dans le dossier (créé s'il manque) : `sol.bmp`, `mur.bmp` (256 px) et `grain.bmp`
(64 px), BMP 24 bits non compressés. Code de sortie 0 si les trois sont écrits.
Le dossier peut contenir des espaces (tout le reste de la ligne). `velocity.ini` n'est ni lu ni écrit.

### Mode bot : `--bot host|join`

```
velocity.exe --bot host|join [--name N] [--time S] [--log fichier.txt] [--botshot fichier.bmp] [--teams N] [--map N]
             [--regles m,g,mu,b,s,min] [--loadout a,b[,m[,c]]] [--level 0-2] [--ranked] [--frags N]
```

- `--bot host` → `botMode = 1` : héberge automatiquement (`ACT_HOST`). Tout autre mot → `botMode = 2` :
  cherche (`ACT_BROWSE`) puis se connecte à la première partie trouvée (`bot_think`).
- `--name` : pseudo (sinon le nom d'utilisateur Windows). `--time` : durée en secondes avant de quitter
  (20 par défaut). `--log` : journal (`netlog`). `--botshot` : capture BMP à la fin. `--map N` : carte hébergée
  (§3.18 ; le client reçoit celle de l'hôte). Sur une carte à vide, le résumé ajoute les chutes (« carte … : chutes
  dans le vide », et côté hôte « vide (arbitre) : … chutes, … creditees a l'attaquant, … suicides »).
- Fenêtres réduites à 800 × 450, l'hôte à gauche, le client à droite ; souris jamais capturée ;
  `velocity.ini` jamais lu ni écrit.
- **Visée** : un tirage par tir (`bot_roll_aim`) : cadré avec la probabilité `botHitP[niveau]` (18 / 38 / 60 %,
  moins de loin), sinon le tir **frôle** la cible (0,55 à 1,2 m sur le côté). `run_tests` garde l'ancienne visée
  sinusoïdale (`botAimTest`) pour rester déterministe.
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
`--spec` (le bot client regarde la partie au lieu d'y jouer). Mode classé : `--ranked` (partie classée, identité
temporaire) et `--frags N` (limite de frags abaissée pour les tests), voir « Match classé » ci-dessous.

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

**Speedrun** (même lobby de test, `/speedrun/…`) : `velocity.exe --lobby 127.0.0.1:8787`, bouton CHALLENGE, une
course : l'écran d'arrivée affiche « CLASSEMENT : #1 … » et le journal `speedrun : LA TOILE en … ms, record, rang 1`.
Les routes se testent aussi à la main :
`curl "http://127.0.0.1:8787/speedrun/top?cat=course:la_toile"`.

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
4. Mise en page : `draw_settings` affiche une ligne de 24 à 36 px par action (selon la hauteur de la fenêtre) et
   `draw_controls` une ligne de 21 px ; vérifiez que tout tient avec `--shot settings.bmp`,
   `--shot settings_small.bmp` (1024 × 640) et `--shot menu.bmp`.
5. Bot (facultatif) : `botPress[A_XXX] = 1` ou `botHeld[A_XXX] = 1` dans `bot_think`.
6. Test : étendez le scénario « reglages » de `run_tests` si la sauvegarde est concernée.

### 7.4 Ajouter un réglage

1. Déclarez la variable avec sa valeur par défaut près de `fovBase`, `invertY`, `impactFrames`
   (section Entrées, bloc « reglages »).
2. `cfg_save` : ajoutez un `fprintf(f, "cle=%d" "\n", var);`.
3. `cfg_load` : ajoutez un `else if (!strcmp(line, "cle")) var = ...;` en **bornant** la valeur
   (`clampf`, `!= 0`). Les clés inconnues sont ignorées, donc l'ordre n'a pas d'importance.
4. `draw_settings` (ou `draw_graphics_settings` pour un réglage d'affichage, avec son aide : `gfx_button`) :
   un bouton bascule à libellé complet (`if (button(lx, y, lw, L.bh, buf)) var = !var;`) ou un curseur
   (`var = slider(id, lx, y, lw, "Libelle", buf, var, min, max);` avec un `id` unique : 1 à 3 sont
   pris). Avancez `y` ; vérifiez que le bouton RETOUR reste à l'écran (`--shot settings.bmp`, `settings_small.bmp`,
   `graphismes_small.bmp`).
5. Utilisez la variable dans le code concerné.
6. Test : ajoutez-la au scénario « reglages » de `run_tests` (modifier, `cfg_save`, remettre la valeur
   par défaut, `cfg_load`, afficher, puis restaurer).

### 7.5 Ajouter un élément de carte

Pour une **nouvelle carte** : une fonction `build_map_xxx()`, sa table de départs, une ligne dans `maps[]` (§3.18),
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
4. Pièges : en solo, `game_tick` ne fait rien tant que `hitstop > 0` (mettez `hitstop = 0` après un coup) ; un
   parry ou un coup laisse `P.parryWin` et `P.meleeCd` actifs ; `nowT` n'avance pas tout seul (faites
   `nowT += TICK` si l'hôte compare des temps, comme dans le test de parry d'épée).
5. Restez dans le couloir dégagé (x = −20) pour ne pas dépendre des blocs aléatoires.
6. Écrivez toujours la valeur attendue entre parenthèses `(attendu ...)` pour pouvoir relire le fichier
   d'un coup d'œil, puis lancez `velocity.exe --test resultats.txt`.

---

## 8. Port macOS

Même `main.c`, compilé avec `clang` (pas de MSVC). Tout ce qui est propre à la plateforme est isolé dans
`src/plateforme/mac/` :

| Fichier | Rôle |
|---|---|
| `mac_platform.h` | API C de la couche plateforme (`mac_*`) et rappels fournis par `main.c` (`plat_*`) |
| `mac_platform.m` | Objective-C (ARC) : fenêtre Cocoa + `NSOpenGLContext` legacy, clavier/souris, `AudioQueue`, police `CoreText`, WebSocket `NSURLSession` |
| `mac_compat.h` | Shims Win32 pour le code partagé : sockets BSD (`SOCKET`, `closesocket`…), `CRITICAL_SECTION`/événements/fils en pthread, `QueryPerformanceCounter`, en-têtes BMP |

- **Build** : `./build_mac.sh` sur un Mac (Command Line Tools) → `Velocity.app` universel (arm64 + x86_64, ad hoc)
  et `Velocity-macOS.zip`. Sans Mac : le workflow `.github/workflows/macos.yml` fait la même chose sur un runner
  macOS, lance `--test` et publie le zip en artefact. Au premier lancement d'un `.app` téléchargé : clic droit →
  Ouvrir (ou `xattr -dr com.apple.quarantine Velocity.app`).
- **Code partagé** : les blocs Windows sont sous `#ifdef _WIN32`. Les entrées passent par des gestionnaires communs
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
- **Mode classé** : requêtes HTTP par `/usr/bin/curl` (`http_request` de `src/plateforme/http.c`, comme `upd_http_get`), clé
  d'identité tirée par `arc4random_buf` (Windows : `RtlGenRandom`).
- **Statut** : écrit sans pouvoir compiler ni lancer sur un Mac (développé sous Windows). À valider sur machine réelle :
  clavier/souris, son, texte, plein écran, lobby, partie contre bot.
