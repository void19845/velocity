# VELOCITY — guide pour Claude

Movement shooter 3D en **C pur**, un seul fichier `main.c`, sans aucune dépendance :
Win32 + OpenGL 1.1 (pipeline fixe) + waveOut (son synthétisé) + Winsock UDP (LAN).
Le propriétaire est francophone : réponses, commentaires et docs **en français**.

- Architecture, systèmes, réseau, réglages, recettes : **`docs/CODE.md`**
- Idées d'armes et de cartes à implémenter : **`docs/IDEES_ARMES_CARTES.md`**
- Parties en ligne (lobby Cloudflare gratuit, STUN, perçage de NAT, spectateurs) : **`docs/PLAN_EN_LIGNE.md`**
  et §4 de `docs/CODE.md`. Le lobby est dans **`worker/`** (`node test.js` pour ses tests, `node dev-server.js`
  pour un lobby local ; le déploiement `npx wrangler deploy` est à faire par le propriétaire).

## Compiler

```
cmd /c ".\build.bat"          # PowerShell
cmd //c ".\\build.bat"        # bash
```
`build.bat` trouve MSVC via vswhere. **Zéro warning** : corriger tout avertissement.

## Vérifier (toujours, après chaque changement)

`velocity.exe` est une appli fenêtrée : la lancer avec `Start-Process ... -PassThru` puis
`WaitForExit`, sinon la commande rend la main tout de suite.

1. **Tests auto** : `velocity.exe --test test.txt` → chaque ligne affiche la valeur obtenue et
   « (attendu …) ». Toute nouvelle mécanique reçoit sa ligne de test dans `run_tests()`.
2. **Captures** : `velocity.exe --shot X.bmp`. Le nom choisit la scène : `rail`, `dm`, `settings`,
   `menu`, `impact`, `fx` (sinon combat solo). Convertir en PNG avec System.Drawing pour la lire.
3. **Match bot contre bot** (réseau réel sur la machine) :
   ```
   velocity.exe --bot host --name Hote --time 90 --log host.txt
   velocity.exe --bot join --name Client --time 86 --log client.txt   (2 s plus tard)
   ```
   Le journal de l'hôte finit par `equilibre : temps pour tuer…, one-shots…, parries…` et
   `touches arbitrees : …` par arme. Pour un rééquilibrage : mesurer **avant / après**.
4. Supprimer ensuite les fichiers temporaires (`test.txt`, `*.bmp`, `*.png`, logs, `*.obj`).

## Règles du code

- **Réglages** : tous les `#define` en haut de `main.c` (mouvement, armes, parry, PvP, pièces…).
  Valeurs PvP actuelles : `PVP_SCALE` 0,7, plafond `PVP_HIT_CAP` 60 PV par touche.
- **Entrées** : la logique de jeu ne lit **jamais** `keys[]` : elle lit les actions `act[A_*]`
  (tenu) et `actP[A_*]` (appui du tick), remplies par `input_poll()` via `binds`. Les bots écrivent
  dans `botHeld` / `botPress`. Nouvelle action = `A_*` + `actId` + `actLabel` + `bindDefault`.
- **Réseau** : l'hôte (id 0) fait autorité (PV, kills, respawns, parry d'épée, recharges).
  Chaque joueur simule son mouvement ; les touches sont validées chez le tireur puis arbitrées
  dans `host_handle()`. Nouvel événement = `EV_*` + `net_fx_*()` émetteur + cas dans
  `apply_event()` (+ `host_handle()` s'il faut arbitrer). **Incrémenter `NET_VER`** dès qu'un
  paquet ou un événement change.
- **Texte à l'écran** : la police ne couvre que CP1252 et le source est en UTF-8 → **pas
  d'accents dans les chaînes C** (écrire `PARAMETRES`, ou échapper `\xE9`). Commentaires du code
  en français sans accents, comme l'existant.
- Fichiers utilisateur : `velocity.ini` (réglages et touches du joueur) — ne jamais l'écraser ;
  les modes `--bot` / `--shot` / `--test` ne sauvegardent pas.

## Pièges déjà rencontrés

- Patchs Python via heredoc bash : un `\n` dans une chaîne C devient un vrai saut de ligne et
  casse la compilation. Écrire le script dans un fichier (chaînes `r"""…"""`) ou utiliser Edit.
- `bind` est une fonction Winsock : le tableau des touches s'appelle `binds`.
- Ordre des définitions : `pname()` et la section réseau sont **après** le combat — avant,
  utiliser `NP[i].name` ; les fonctions `net_fx_*` / `net_hit` y sont déclarées en avance.
- `game_tick()` sort tôt pendant le `hitstop` : les minuteurs « temps réel » (impact frames)
  se décomptent dans `render()`.
- Les bots ont un temps de réaction et une imprécision voulus ; s'ils sont « trop forts »,
  le match de comparaison n'est plus représentatif d'un humain.
