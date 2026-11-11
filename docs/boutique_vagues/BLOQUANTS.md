# Boutique Vagues : ce qui reste (a corriger ou a valider)

Etat : **140 etapes / 140**, **30 capstones / 30**, **10 fusions / 15** codees ; **30 branches / 30 terminables** ; 10 fusions atteignables
(SALVE D'ARTILLERIE, CHEVALIER, COMBO AERIEN, LANCE-ROQUETTES, ACROBATIE EXPLOSIVE, FEU D'ARTIFICE, FUSILLADE, TAPIS DE BOMBES, et FISSION et
DOUBLE DETENTE avec leur seule forme fusionnee, voir §2).
Fait aussi : niveaux d'amelioration (+20 % par niveau), formes fusionnees (3 armes hybrides), regle de soin (`vg_heal`), changement d'arme payant.
**Pour l'instant aucun lien melee x competence** (branches et fusions retirees de l'arbre ; 112 fusions au total dans l'arbre, 15 pour les 7 graines
jouables). Pour les rouvrir : vider `sansLien` dans `outils/migrer_v2.js` et regenerer (attention : `migrer_v2.js` perdrait le texte de
"Ricochet pur" et les capstones modifies a la main dans `arbre_v2.json`).
Apres un changement de texte dans `donnees/arbre_v2.json` : `node generer.js` (html + xlsx), `node rapport.js`, `node export_c.js` (C).

## 1. Fusions non codees (5) : toutes leurs branches sont terminables, il manque seulement l'effet de la fusion

| Fusion | Texte | Remarque |
|---|---|---|
| SHO-LAN CHARGE DE PLOMB | une joute reussie recharge le Shotgun et le tir qui suit est garanti a 100 % | « garanti a 100 % » a preciser (tous les plombs touchent ? aucune perte a distance ?) |
| SHO-IMP ACROBATE | le recul du Shotgun se combine au punch-jump : double impulsion, +40 % de vitesse horizontale max | a coder (mouvement) |
| RAIL-PIE BILLARD | tes ricochets s'enchainent tant qu'une cible reste sous 90 m ; le rail gagne +10 % par cible touchee par la chaine | invasif : ricochets enchaines |
| SHO-PIE PIECE MITRAILLEE | ta salve de Shotgun lance 6 pieces en eventail sans consommer de charge de piece ; une piece touchee par un plomb ricoche x1,5 sur 3 cibles | a coder (s'appuie sur le capstone SHO>PIE) |
| SHO-AIO FEU A VOLONTE | l'All in One tire toutes tes salves en 0,5 s ; chaque salve garde x1 | a coder (s'appuie sur le capstone SHO>AIO) |

## 2. Fusions partielles

- **SALVE D'ARTILLERIE** : les 3 plombs perforants a 40 % marchent ; le « +1 m de souffle de la salve de Shotgun » n'est pas fait (le Shotgun
  n'a aucun souffle a augmenter) : retirer ce bout du texte ou lui donner un souffle.
- **FISSION** (RAIL-ROQ) et **DOUBLE DETENTE** (RAIL-SHO) : achetables, mais seule la forme fusionnee est codee (genre `BK_HYB`) ; leur passif promis
  par le texte ne l'est pas (FISSION : le rail qui traverse une roquette en vol la convertit en nuke ; DOUBLE DETENTE : « reduit la recharge du rail de
  0,05 s par plomb », a harmoniser avec le capstone SHO>RAIL qui passe a la jauge).
- Cosmetique : le texte de FISSION deborde sur la ligne de prix de la carte de boutique.

## 3. Codees mais a valider (lectures et valeurs inventees)

**Validees par le proprietaire** (niveaux +20 %) : un gain monte, pas la valeur absolue (« x1,5 » devient 1,5 + 0,1 x echelle) ; la penalite de vitesse
de la Roquette lourde monte aussi ; Perce-mur : l'epaisseur de mur (1 m x echelle) ET les degats (70 % x echelle, 100 % au plus) montent tous les deux.
Les etapes qui ne montent jamais en niveau (declencheurs purs, entiers fixes : Satellite, Soute, combos de N coups, Reserve de la Piece...) ne sont pas
proposees en amelioration.

**Formes fusionnees** : cone lu comme angle total (6 et 8 deg, donc 3 et 4 deg de demi-angle comme la doc du Shotgun ; `HY_CAN_HALF`, `HY_OBU_HALF`) ;
jauge de 1500 degats, delais de tir 1,1 / 0,9 / 1,1 s, touche T, munitions des deux armes tenues pleines et non consommees pendant la forme ;
ARTILLERIE ORBITALE herite du capstone Rail orbital (x4, achete pour atteindre la fusion) et chaque souffle fait le plein degat de roquette ;
les degats propres de la forme sont multiplies par le niveau de la fusion.

**Capstones** :
- Plomb d'amorcage (SHO>RAIL) : 5 points de jauge de rail par plomb qui touche (10 plombs = 50 points sur 250).
- Salve rail (AIO>RAIL) : 2 projectiles par munition, Railgun en main seulement ; Salve explosive (AIO>ROQ) : souffle aux degats et rayon de base du
  Shotgun (45 et 3 m, sans ses etapes) a chaque touche de la salve, pendant la fenetre de 1,5 s.
- Rail d'ouverture (RAIL>SHO) : tout ennemi touche par un rail est « ouvert » 3 s (minuteur par ennemi) ; les plombs sur lui font x1,3 sans perte a
  distance (le x1,4 de proximite du capstone SHO>LAN est conserve).
- Shotgun total (SHO>AIO) : au moins 6 salves avec le Shotgun en main ; la munition depensee ne change pas (regle de 2 munitions minimum gardee) ;
  DOUBLE CANON ne rembourse pas les salves ajoutees.
- Piece en plombs (PIE>SHO) : une piece qui finit (ricochet d'un tir, roquette qui la traverse, fin de vie) explose en 10 plombs vers les 3 ennemis
  les plus proches (ou en eventail s'il n'y en a aucun) ; une piece deja eclatee par un plomb garde son eclat habituel, sans plombs en plus.
- Salve de Shotgun (AIO>SHO) : lecture du proprietaire : cone x3 et +50 % de plombs pendant la salve avec le Shotgun en main.
- Shotgun acrobate (SHO>IMP) : le recul du Shotgun rend le double saut ; Shotgun a pieces (SHO>PIE) : la salve lance une piece par plomb, sans charge.

**Soin** : le parry de l'epee rend toujours `PARRY_HEAL` (10 PV) hors du plafond de 20 PV/s ; les kills de melee ne rendent plus leurs 15 PV en VAGUES ;
le plafond est un seau (rafale de 20 PV puis 20 PV/s) et non une fenetre glissante.

**Autres lectures** :
- Releve : plafonne aussi l'estoc dans le vide (0,9 s dans le code), pas seulement la lance couchee (0,5 s) ; Ricochet pur : « rebond » = maillon de la
  chaine de pieces ; Dernier souffle : chargeur plein au depart de la salve ; Piece orbitale : orbite 4 s a 2,5 m puis part vers l'ennemi.
- Rail orbital (RAIL>ROQ) et Mini-nuke (ROQ>RAIL) : « 1 seule munition » lu comme plafond de reserve a 1 ; Roquette salve (ROQ>AIO) : les 8 roquettes en plus
  partent en eventail horizontal.
- Cadence (All in One) : delai de tir apres la salve -30 % ; Cible unique : x1,3 sur les touches suivantes de la salve contre une cible deja touchee.
- Dividende, Piece recyclee, Rebond de plus : ricochets de tir instantane seulement (rail, Elan). Degats inventes : 45 (explosions de piece).
- Velocite (Lance) : vitesse de rapprochement x1,1 ; Lance-eclair : recharge et animation x0,65 ; Lance vive / Jambes vives : animation seulement.
- IMP>RAIL : « un uppercut recharge ton rail » lu comme munitions pleines ; IMP>ROQ : la roquette a 60 % peut blesser le joueur a bout portant.
- Ogive large : agrandit la collision de la roquette sur les ennemis, pas son visuel ; Rail jumeau : 20 % ; Perce-mur : tir a 70 % apres le mur.

## 4. Suite

- **Refactor armes « objet »** (sac de stats, fiche d'arme, bus d'evenements) : cahier des charges, API et recettes dans `ARCHITECTURE_ARMES.md`. Fait : sac de
  stats, genres generiques, famille Shotgun (la valeur d'une etape-stat est une ligne de `effets/*.js`) et preparation des familles restantes (toutes les stats, operations,
  conditions et phases dont elles ont besoin existent, les 56 lignes de donnees sont pretes). A faire : Roquettes + Railgun, Lance + Impulsion, Piece + All in One, trois
  travaux independants decrits dans `CLASSIFICATION_ETAPES.md` (§5 et §6), puis fiche d'arme et bus d'evenements. Choix a valider : les paliers d'une meme idee (Plombs chercheurs / traqueurs, Rail large / colossal,
  Visee courbe / Tir guide) restent « le plus fort l'emporte » (groupe) comme avant le refactor ; retirer le `grp` d'une etape les ferait s'additionner.
- A faire en jeu : une partie VAGUES jouee en vrai, un match bot contre bot complet apres les derniers changements.
