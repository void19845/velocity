# Plan — parties en ligne gratuites (salons, matchmaking, spectateurs)

Objectif : jouer en ligne **sans serveur de jeu payant**, avec un matchmaking quasi instantané et la
possibilité de regarder une partie. L'hôte reste un joueur (id 0) et fait autorité, comme en LAN.

## Architecture

```
  joueur A (hôte)            Cloudflare (gratuit)             joueur B (client / spectateur)
  ───────────────          ──────────────────────          ─────────────────────────────
  STUN → ip:port public ──►  Durable Object « Lobby »  ◄──  STUN → ip:port public
  WebSocket (texte)  ◄────►  liste des salons, quick play ◄► WebSocket (texte)
          │                  signale « punch » à l'hôte              │
          └────────────── UDP direct (hole punching) ────────────────┘
                     même protocole que le LAN : PK_JOIN, PK_SNAP, PK_EVENT…
```

- **Lobby** : un Worker + un Durable Object (`worker/`). Il ne voit jamais le trafic de jeu, seulement
  des lignes de texte (salon, ip:port). Une connexion WebSocket par joueur au menu : la liste se met à
  jour en direct et l'hôte est prévenu **immédiatement** qu'un client arrive (pas de polling).
- **Découverte de l'adresse publique** : requête STUN (RFC 5389) envoyée **depuis le socket du jeu**
  (même port → même mapping NAT) à `stun.l.google.com:19302`.
- **Connexion** : le client reçoit `go <ip:port de l'hôte>` et envoie ses `PK_JOIN` ; l'hôte reçoit
  `punch <ip:port du client>` et lui envoie des `PK_PUNCH`. Les deux NAT s'ouvrent. Si les deux joueurs
  sont derrière la même box (même IP publique), le lobby renvoie l'adresse **LAN** de l'hôte.
- **Spectateur** : un client qui rejoint avec le drapeau `spectator` dans `PK_JOIN`. Il n'a pas de slot
  `NP` : il reçoit snapshots, roster et événements, n'envoie rien. Caméra libre / suivi d'un joueur.
- **Limites** : NAT symétrique / CGNAT → échec du perçage (message clair + saisie d'IP manuelle). Un
  relais UDP (VM Oracle gratuite) pourra être ajouté plus tard sans changer le protocole de jeu.

## Protocole du lobby (lignes de texte, champs séparés par des espaces, `_` à la place des espaces)

| Sens | Message | Rôle |
|---|---|---|
| C→S | `list <ver>` | s'abonner à la liste des salons de cette version de protocole |
| C→S | `host <ver> <joueurs> <max> <spec> <ep_public> <ep_lan> <nom>` | publier / mettre à jour mon salon |
| C→S | `join <id> <mon_ep> <spec 0/1>` | rejoindre ou regarder un salon |
| C→S | `quick <ver> <mon_ep>` | partie rapide : le lobby choisit le salon le plus rempli et non plein |
| C→S | `ping` | maintien de connexion (toutes les 25 s) |
| S→C | `hello <id>` | à la connexion |
| S→C | `room <id> <joueurs> <max> <spec> <ep> <nom>` / `gone <id>` / `end` | liste et mises à jour |
| S→C | `go <ep> <spec>` | adresse à rejoindre |
| S→C | `punch <ep>` | (hôte) un client arrive, ouvre ton NAT vers lui |
| S→C | `none` / `err <texte>` / `pong` | réponses |

`ep` = `a.b.c.d:port`. Un salon disparaît quand l'hôte se déconnecte.

## État d'avancement

Étapes 1 à 5 **faites**. Lobby déployé : `velocity-lobby.velocity-void.workers.dev` (`wss://` vérifié :
WebSocket ouvert en ~75 ms). Vérifié sur une machine avec le vrai lobby et le vrai STUN de Google : l'hôte
publie son adresse publique, un client et un spectateur le trouvent, `join` → `go` (adresse LAN, même box) →
connexion. **Non vérifié** : le perçage de NAT entre deux vrais réseaux différents (seule la branche
« même box » a été exercée).

## Étapes

1. **Lobby** — `worker/src/lobby.js` (logique pure, testée), `worker/src/index.js` (Worker + Durable
   Object en WebSocket hibernable), `worker/dev-server.js` (même logique en local pour les tests),
   `worker/test.js`.
2. **Client en ligne** (`main.c`) — fil WinHTTP WebSocket (`lb_*`), STUN (`stun_*`), `PK_PUNCH`,
   salons en ligne dans la liste « REJOINDRE », bouton « PARTIE RAPIDE », publication automatique de
   l'hôte, réglages `online` / `lobby` dans `velocity.ini`. `NET_VER` → 4.
3. **Spectateur** — drapeau dans `PK_JOIN`, slots `SP[]` côté hôte, `myId = MAXP` côté client,
   caméra libre / suivi, HUD « SPECTATEUR ».
4. **Vérifications** — `--test` (analyse STUN, protocole du lobby, spectateur), match de bots passant
   par le lobby local (`--lobby 127.0.0.1:8787 --lanep`), captures.
5. **Déploiement** (à faire par le propriétaire, compte Cloudflare) — `cd worker && npx wrangler deploy`,
   puis mettre l'adresse `xxx.workers.dev` dans `velocity.ini` (`lobby=`) ou dans `LOBBY_DEFAULT`.

## Coûts réseau (mesurés sur les structures)

`PkSnap` = 464 octets × 60/s ≈ 28 Ko/s ≈ 220 kbit/s **par client ou spectateur** côté hôte. 8 joueurs
+ 4 spectateurs ≈ 2,7 Mbit/s d'upload : limite à `MAXSPEC` = 4 spectateurs en direct.

## Hors périmètre (pour plus tard)

Relais UDP (Oracle), spectateurs en nombre avec retard, classement, authentification.
