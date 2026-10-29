# Competences Marksman — design (remplacement des pieces)

Statut : **proposition, rien n'est code**. Branche de travail : `dev`.

## 1. Pourquoi remplacer les pieces

La piece est aujourd'hui le centre du jeu d'adresse : on la lance, le rail la touche et ricoche
(x`COIN_MULT` par rebond), le yoyo la frappe (`FLAIL_COIN_*`), la scie la frole (`SAW_COIN_*`), le tesla
la charge. Probleme : c'est **un objet physique a viser**, donc lourd a synchroniser (`EV_COIN`, `netId`,
vol, gravite, vol de piece adverse) et il impose un seul style : lancer, puis tirer dedans.

Un jeu de **competences** garde l'esprit (adresse, enchainement, risque) mais rend l'effet
**lisible, previsible et peu couteux en reseau**.

## 2. Principes

1. Meme plafond de skill : un bon joueur doit pouvoir faire plus de degats qu'avec les pieces.
2. Le rail reste l'arme-reine ; les competences l'amplifient, elles ne le remplacent pas.
3. Pas de nouvel objet physique : tout est **etat sur le joueur ou la cible** (marque, charge, fenetre).
4. Chaque competence a un contre-jeu (visible par la cible, ou limitee dans le temps).
5. Les interactions des autres armes (yoyo, scie, tesla, egide) sont conservees en changeant seulement
   leur declencheur : *piece* -> *cible marquee* ou *tir amplifie*.

## 3. Le kit Marksman (3 competences)

Touches : nouvelles actions `A_SKILL1`, `A_SKILL2`, `A_SKILL3` (a la place de `A_COIN` pour la premiere).
Recharges independantes, jauge visible au HUD comme les charges de pieces.

| Competence | Effet | Valeurs de depart | Remplace |
|---|---|---|---|
| **MARQUE** (`SK_MARK`) | Tir instantane sans degats : pose une marque 4 s sur la premiere cible touchee (ennemi ou joueur). Une cible marquee est visible a travers les murs. | portee 90 m, recharge 6 s, marque 4 s | la piece comme point d'appel |
| **CHARGE** (`SK_CHARGE`) | Le prochain tir de rail est amplifie : maintenir la touche 0,6 s charge une fenetre de 2 s ; un rail tire dans la fenetre gagne x1,5 et perce une cible. Le mouvement n'est pas ralenti, mais la charge se perd a l'impact d'un coup recu. | recharge 8 s | `COIN_MULT` sur ricochet |
| **RICOCHET** (`SK_BOUNCE`) | Le prochain tir de rail se redirige automatiquement vers la cible marquee la plus proche (jusqu'a 3 rebonds de cible a cible, x1,25 chacun, comme `COIN_SEEK_RANGE`). Sans marque : se comporte comme un tir normal. | recharge 10 s | ricochet de piece |

Combo de reference : MARQUE -> CHARGE -> RICOCHET -> rail = x1,5 x 1,25^n. Plafond a verifier contre
`PVP_HIT_CAP` (60 PV par touche) : en PvP le plafond reste la regle, le gain vient de la fiabilite du tir
(pas de piece a viser) plutot que du degat brut.

## 4. Nouvelles interactions entre armes

| Arme | Avant (piece) | Apres (marque / charge) |
|---|---|---|
| Yoyo | frappe une piece : tir de 40 degats vers une cible | touche une cible **marquee** : la tire vers soi et la verrouille pour le rail (fenetre combo `FLAIL_COMBO_T` reelle, plus seulement visuelle) |
| Scie | frole une piece : la relance en l'air | traverse ou plante dans une cible marquee : degats x1,25 et la marque se prolonge de 2 s |
| Tesla | charge la piece | un arc touchant une cible marquee la **propage** a une cible voisine |
| Egide | n'avait pas d'interaction | un tir marque sur un porteur d'egide ignore 30 % de l'absorption (`SHIELD_*`) |
| Parry | renvoyait aussi les pieces | un parry parfait **retire la marque** de l'attaquant (contre-jeu) |

## 5. Reseau et code (esquisse)

- Etat joueur : `P.skillCd[3]`, `P.chargeT`, et sur chaque cible `markT` / `markBy`.
- Hote autoritaire (comme le bouclier) : la pose de marque est un evenement `EV_MARK` arbitre dans
  `host_handle()` ; l'etat de marque est resynchronise dans l'etat joueur (pas de nouvel objet a suivre,
  donc suppression de `EV_COIN`, `netId` des pieces et vol de piece).
- **Incrementer `NET_VER`** : paquet et evenements changent.
- Suppression : `Coin`, `CO`, `throw_coin`, `coin_tick`, `ricochet` (remplace par `ricochet_marked`),
  reglages `COIN_*` et `FLAIL_COIN_*`, `SAW_COIN_*`, texte « PIECE VOLEE ». Garder `W_COIN` le temps d'une
  etape pour les journaux, puis le renommer.
- Entrees : `A_SKILL1..3` + `actId` + `actLabel` + `bindDefault` ; bots dans `botHeld` / `botPress`.
- Aucun accent dans les chaines C (police CP1252).

## 6. Plan de migration (par etapes, chacune compilable et testee)

1. **Etat de marque** + competence MARQUE seule (HUD, effet visuel, reseau). Les pieces restent.
2. **CHARGE** et **RICOCHET** ; ajouter les lignes de test (`marque : portee, duree`, `charge : x1,5`,
   `ricochet : 3 rebonds`, contre-jeu parry).
3. Reconnecter yoyo, scie, tesla, egide aux marques (section 4).
4. Retirer les pieces et leur code ; bots : apprendre la sequence MARQUE -> CHARGE -> rail.
5. Match bot contre bot **avant / apres** : temps pour tuer, one-shots, touches arbitrees. Verifier que le
   temps pour tuer ne baisse pas de plus de 10 %.
6. Fusion `dev` -> `stable` seulement apres cette comparaison.

## 7. Questions ouvertes (a trancher par le proprietaire)

- **Nombre de competences :** 3 comme ci-dessus, ou une seule (MARQUE) pour commencer plus simple ?
- **Maintien ou appui :** CHARGE doit-elle se maintenir (plus d'adresse) ou s'activer d'un appui (plus fluide) ?
- **Marque visible par la cible ?** Plus juste (contre-jeu), mais moins brutal pour le tireur.
- **Mode solo :** les vagues d'ennemis gardent-elles un role pour la piece (score) ou tout passe aux marques ?
- **Autres kits :** ce fichier ne couvre que Marksman ; prevoir des kits pour d'autres styles (melee, mobilite) ?
