# Boutique Vagues : arbre de modificateurs et de fusions (v2)

Document de **design** du mode VAGUES. Il n'est pas encore dans le code : `src/solo/ameliorations.c` a toujours ses chemins
de 5 étapes (voir `docs/ARMES_ET_BOUTIQUE.md`, partie II). L'arbre ci-dessous (20 graines, 112 fusions : pour l'instant aucun lien entre une compétence et une mêlée) est ce qu'on vise.

| Fichier | Rôle |
|---|---|
| `arbre_fusions_boutique_vagues_v2.html` | visionneuse (loadout / arme). ✦ = nouveau ou revu, ✓ = fusion validée, ⚙ = forme fusionnée, « chemin : … » = ce que les 4 étapes apportent au partenaire |
| `fusions_boutique_vagues_v2.xlsx` | les mêmes données en 5 feuilles (Règles, Archétypes, Pool d'étapes, Branches, Fusions). Vert = nouveau ou revu, violet = validé, ambre = pas encore relu |
| `BLOQUANTS.md` | ce qui reste à coder dans le jeu (capstones, fusions) et les lectures à valider |
| `ARCHITECTURE_ARMES.md` | refactor « armes objet » : le **sac de stats** (fait : API, opérations, conditions, phases, règles de niveau, recettes pour ajouter une étape-stat, une stat, une opération et migrer une famille), la fiche d'arme et le bus d'événements (à faire) |
| `CLASSIFICATION_ETAPES.md` | les 140 étapes des 7 graines jouables classées (stat, stat conditionnelle, événement, comportement), le catalogue des 38 stats, et pour les trois familles qui restent à migrer le bloc de données prêt, la fonction à brancher et les fichiers touchés |
| `CHANGEMENTS_v2.md` | tout ce qui a changé par rapport à la v1 |
| `outils/` | les données (`donnees/arbre_v1.json` reçu, `arbre_v2.json` produit) et les scripts qui régénèrent tout |

## Structure : les axes du propriétaire

Chaque étape d'un pool modifie **une stat** (colonne « Axe » de l'Excel, détail du HTML). Les effets spéciaux (parry, joute, mines, ondes…)
sont dans les capstones et les fusions.

- **Armes à distance** (Railgun, Roquettes, Shotgun, Élan, Scie, Ventouse, Tesla) : taille du projectile, vitesse, zone d'effet, portée,
  nombre de munitions, nombre de projectiles, dégâts, suivi. Plus l'étape de soin, en tête de la profondeur 3. Quand un axe n'a pas de
  sens pour une arme (vitesse du Railgun et du Tesla, munitions de la Scie), il est absent (`outils/axes.js`, `SANS_OBJET`).
- **Mêlée** (Épée, Enclume, Lance, Yoyo, Impulsion, Égide) : une étape par axe et par profondeur parmi cooldown, vitesse d'animation,
  longueur de combo, munition rendue, lifesteal (le lifesteal de la profondeur 3 est en tête, c'est l'étape de soin).
- **Compétences** : pas de liste donnée, elles gardent leurs pools (charges, durée, recharge, multiplicateur, portée…) avec les
  remplacements décrits plus bas.

La profondeur donne la rareté et l'ampleur : +10 % en profondeur 1, effets conditionnels en 2 et 3, versions « épiques » en 4.

Le « nombre de projectiles » dépend de l'arme : plombs du Shotgun, rails en plus, mines posées, cibles de la chaîne du Tesla. Pour la
**Scie**, les projectiles en plus sont des lames qui **orbitent autour de la scie** (Lame satellite, Couronne de lames, Halo de lames).

## Trier les branches

Un petit script montre une branche complète (4 étapes avec leur axe, capstone, fusion, forme fusionnée, branche inverse) et propose de la
valider ou de la supprimer, avec une note de 1 à 5 et un commentaire facultatifs.

```bash
trier.bat                         # double-clic, ou dans un terminal : les branches pas encore triées
trier.bat --graine RAIL,ROQ       # seulement ces graines    (--vers EPE, --type D|M|C, --tout, --statut validee|supprimee|modifiee)
trier.bat --liste                 # les décisions déjà prises
```

À l'invite, une seule ligne : `v 5 très bon chemin` (valide, note 5, commentaire), `s 2 trop faible` (supprime), `v` ou `s` seuls, `p` ou
Entrée (passer), `b` (retour), `c texte` (commentaire seul), `n 4` (note seule), `q` (quitter), `?` (aide). Chaque décision est écrite tout
de suite dans `outils/donnees/revue.json`.

**Comment ta revue est lue** (tes 38 premières branches) : une branche validée sans commentaire est figée. Un commentaire, qu'elle soit
validée ou « supprimée », est une consigne : je l'applique, j'écris ce qui a été fait dans le champ `reponse` de `revue.json`, et la
branche revient dans la file avec le statut « retravaillée, à revalider » (ta décision précédente est gardée, tes notes et commentaires aussi).
Le champ `chemin: "ok"` dit que tu n'as pas contesté les étapes : le chemin est gardé même quand la fusion ou le capstone sont à refaire.
Un nouveau `v` ou `s` efface la réponse.

Pour retrouver la revue dans le HTML (✓ / ✗, étoiles, commentaire) et l'Excel (colonnes Revue, Note, Commentaire de la feuille Branches) :
`node migrer_v2.js && node generer.js` dans `outils/`. À la régénération, une branche **validée est figée** (son chemin ne change plus) et un
chemin **supprimé n'est pas reproposé** : l'optimiseur en choisit un autre, la branche revient dans la file « modifiée depuis le tri » avec
ton ancien commentaire.

## Ce que la v2 change

1. **Pools** : 260 étapes d'armes à distance et de mêlée réécrites autour des axes (167 nouvelles, 9 modifiées, 84 gardées de la v1).
   Plus de Gel, Brûlure, Contagion, Fracture (Railgun) ni de variantes de feu, nulle part.
   **Règle** : une étape de pool sert dans tous les loadouts, elle ne dépend jamais d'une autre graine (« Pièces explosives » dans le pool
   du Shotgun ne servait à rien sans la Pièce). `outils/migrer_v2.js` refuse une étape qui nomme une autre graine.
2. **Chemins** : la v1 donnait aux 4 étapes d'une branche une rotation fixe, donc 297 paires de branches d'un même loadout avaient un chemin
   *identique*. En v2, chaque étape est choisie selon ce que le partenaire attend des stats de la graine (profils dans `outils/axes.js`, par
   exemple la munition rendue de la mêlée nourrit les armes à distance) : aucun chemin identique, au plus une étape commune entre deux
   branches qui cohabitent, aucune étape inutilisée.
3. **Fusions** : 9 validées (FISSION, DOUBLE DÉTENTE, RELAIS DE MINE, CHEVALIER, COMBO AÉRIEN, RICOCHET D'ACIER, BILLARD, CASCADE, FUSILLADE),
   40 retravaillées à valider, 105 au texte de la v1. Cinq anciennes validées (DUEL, CHOC LOURD, BOUCLIER PERCÉ, SURTENSION, EXÉCUTION) ont été
   réécrites à ta demande (pensées pour le PvP, ou dégâts fixes au lieu d'un pourcentage de PV max) et sont à revalider.
   Retravaillées : Railgun avec Scie, Ventouse, Marque, Mirage ; Roquettes × Pièce « comme Ultrakill » ; Tesla × All in One (éclair inévitable) ;
   les 12 fusions Ricochet, adaptées à la nouvelle compétence ; Shotgun × Pièce, l'exemple de synergie « support » (un shotgun qui projette
   des pièces). 3 doublons de nom corrigés.
4. **Formes fusionnées** (idée du propriétaire, proposition) : la fusion finale de deux armes à distance débloque temporairement une arme
   hybride qui remplace les deux, par exemple un shotgun qui tire des rails (CANON SCIÉ). 21 paires, une arme chacune. La synergie passive
   de la fusion reste active en permanence. Cadre proposé : jauge de fusion remplie par les dégâts, touche d'activation, 15 s, munitions mises
   de côté puis rendues, les modificateurs des deux arbres s'appliquent à l'hybride stat par stat (texte complet dans l'Excel, feuille Règles).
5. **Éventail devient Shotgun** partout, comme dans le patch d'équilibrage ; Pièce (2 charges) et Ricochet (durée) suivent le patch.

## À valider

- **Formes fusionnées** : codées pour ARTILLERIE ORBITALE, CANON SCIÉ et OBUSIER (`src/solo/boutique_hybrides.c`, touche T, jauge de 1500 dégâts,
  15 s ; réglages et lectures à valider dans `BLOQUANTS.md`). Le cadre (touche, 15 s, jauge) et chacune des 21 armes sont des propositions. Les deux fusions validées FISSION et
  DOUBLE DÉTENTE gardent leur effet passif ; leur forme fusionnée (ARTILLERIE ORBITALE, CANON SCIÉ) s'y ajoute, à confirmer.
- **Axes des compétences** : pas de liste donnée. Si tu en as une, les pools des 7 compétences passent au même format.
- **Règle `CLAUDE.md`** (« aucun achat n'augmente une stat ») : elle date de la boutique de comportements. L'arbre de modificateurs la contredit
  volontairement, à mettre à jour quand il passe dans le code.
- **Variantes de feu** : remplacées avec Brûlure (« brûlure tout court »).
- **Valeurs du patch** : recharge de la Pièce, durée du Ricochet, recul du Shotgun ne sont pas fixées ; les textes sont en relatif et se
  recaleront sur les chiffres du patch une fois fini.
- **Yoyo sans traction** (puis **traction rétablie sur appui long**, 2026-11-10) : décidé (« La retirer »), retiré du code le 7 novembre ; le
  propriétaire est revenu dessus (« on la garde, mais sur un appui long, et ça tracte aussi longtemps que le joueur maintient ») : **le code a de
  nouveau une traction**, déclenchée par un appui long (touche de mêlée tenue 0,15 s), qui tire les cibles de l'arc tant que la touche est tenue
  (`fleau.c`, `docs/armes/03_fleau.md`, `docs/NOUVEAUTES.md` § 38). L'appui court reste le balayage sans traction. **Les pools du Yoyo de l'arbre ne
  sont pas modifiés** : « Balayage de zone » décrit l'appui court ; la traction de l'appui long n'y est pas encore une étape ni un axe (à décider :
  base du Yoyo, ou branche). Le paragraphe suivant date du retrait. Dans l'arbre, le Yoyo devient « Balayage de zone » (arc de 140°, 6 m,
  touche tout ce qui est dans l'arc, parry à distance, frappe les pièces) : son pool ne parle plus de cibles tirées et ses 11 fusions et ses
  capstones qui reposaient sur la traction sont réécrits (MINE FOUETTÉE remplace HAMEÇON, FOUET GUIDÉ remplace HARPON GUIDÉ). À relire en triant
  les branches du Yoyo. Le capstone Roquettes → Yoyo « la roquette attire à 6 m » est celui de la roquette, tu l'avais validé.
- **Fusions non relues** : 105 gardent le texte de la v1 (statut « À valider »). Les familles Charge, All in One, Aimant, Marque, Mirage et
  Pièce sont les plus formulaïques (« la touche chargée fait ×2 », « pendant l'Aimant, X suit la cible »…).

## Régénérer

```bash
cd docs/boutique_vagues/outils
node migrer_v2.js      # v1 + pools_v3 + etapes_v2 + chemins + fusions_v2 + hybrides_v3 -> donnees/arbre_v2.json (et contrôles)
node generer.js        # -> le .html et le .xlsx
node rapport.js        # -> CHANGEMENTS_v2.md
```

Pour le **jeu** : `node export_c.js` écrit `src/solo/boutique_donnees.c` depuis `arbre_v2.json` et `effets/*.js`. Une étape qui modifie une stat s'y écrit
`'GRAINE:Nom': { stat: 'TAI', op: 'mul', val: 1.15 }` (et `cond`, `grp`, `phase`, `unit`, `fixed` au besoin), sans ligne de C quand le code de l'arme lit déjà la stat :
voir `ARCHITECTURE_ARMES.md` (recette 3.1) et le commentaire de tête de `effets/shotgun.js`.

Les pools d'armes : `outils/pools_v3.js`. Les étapes de compétence : `outils/etapes_v2.js`. Une fusion ou un capstone : `outils/fusions_v2.js`.
Une forme fusionnée : `outils/hybrides_v3.js`. Les axes et les profils de partenaire : `outils/axes.js`. Un chemin à la main :
`outils/chemins_manuels.js`. Aucune dépendance (Node 22+).
