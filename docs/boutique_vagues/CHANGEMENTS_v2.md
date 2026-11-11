# Arbre de la boutique Vagues : changements de la v2

Fichier généré par `outils/rapport.js` (ne pas modifier à la main). Les fusions validées par le propriétaire ne sont pas touchées.

## Pools des armes à distance et de mêlée

Réécrits autour des axes du propriétaire : une étape = une stat. Distance : taille du projectile, vitesse, zone d’effet, portée, munitions, nombre de projectiles, dégâts, suivi (+ le soin de la profondeur 3). Mêlée : une étape par axe et par profondeur parmi cooldown, vitesse d’animation, longueur de combo, munition rendue, lifesteal. La liste complète est dans l’Excel (feuille « Pool d’étapes », colonnes Axe et Nouveauté) et dans le HTML (✦).

| Graine | Type | Nouvelles | Modifiées | Gardées | Retirées de la v1 |
|---|---|---|---|---|---|
| Railgun | distance | 8 | 2 | 10 | Cadence, Chaîne vive, Gel, Fracture, Brûlure, Surcharge, Contagion, Détonation |
| Roquettes | distance | 11 | 0 | 9 | Recul, Auto-saut, Brûlure, Vortex, Mine, Souffle large, Rocket-jump, Ressac, Bord brûlant, Contagion, Sismique |
| Shotgun | distance | 10 | 3 | 7 | Recul maîtrisé, Recul, Propulsion, Slide-tir, Brûlure, Fracture, Aérien, Gel, Contagion, Pièces explosives |
| Élan | distance | 10 | 2 | 8 | Dash-tir étendu, Cadence, Aérien, Recul cinétique, Ressac, Brûlure, Gel, Contagion, Allure pleine, Sillage |
| Scie | distance | 11 | 0 | 9 | Retour vif, Prise, Rappel rapide, Brûlure, Spectre, Gel, Éclats, Vortex, Contagion, Lame fantôme, Enchaînement |
| Ventouse | distance | 10 | 0 | 10 | Colle forte, Mine, Vortex, Gel, Mine piège, Contagion, Éclats, Fracture, Brûlure, Mine fantôme |
| Tesla | distance | 8 | 0 | 12 | Brûlure, Fourche, Spectre, Relais durable, Fracture, Contagion, Éclats, Gel |
| Épée | mêlée | 17 | 0 | 3 | Affûtage, Allonge, Fenêtre, Onde, Frappe, Miroir, Arc, Retour de lame, Fente, Désarmement, Saignement, Gel, Stase, Lame ricochet, Duelliste, Contre parfait, Éclair |
| Enclume | mêlée | 16 | 0 | 4 | Poids, Onde large, Mur haut, Impact, Recul, Ressac, Onde de parry, Faille, Vortex, Sol brûlant, Gel, Éclats, Détonation, Contagion, Pique de feu, Immobilisation |
| Lance | mêlée | 16 | 0 | 4 | Estoc, Allonge, Grappin, Frappe, Fourche, Miroir, Choc, Charge lourde, Gel, Détonation, Brûlure, Éclats, Contagion, Pointe explosive, Lance double, Cavalier |
| Yoyo | mêlée | 17 | 2 | 1 | Attrait, Chaîne, Arc large, Traction, Miroir, Arc, Pièce renvoyée, Entrave, Vortex, Gel, Brûlure, Éclats, Stase, Boulet, Lasso, Traction de groupe, Contagion |
| Impulsion | mêlée | 18 | 0 | 2 | Fracture, Jambes, Poings durs, Suspendu, Onde, Frappe, Impact, Uppercut, Double saut, Éclats, Gel, Brûlure, Vortex, Détonation, Contagion, Combo de feu, Pluie de poings, Rebond |
| Égide | mêlée | 18 | 0 | 2 | Jauge+, Régénération, Bouclier large, Vitesse, Coup de bouclier, Miroir, Spectre, Onde, Recul, Rempart, Fracture, Gel, Brûlure, Éclats, Stase, Contre-attaque, Armure réactive, Rail renvoyé |

## Étapes de compétence remplacées (26)

Gel, Brûlure, Contagion et les variantes de feu partent de tous les pools ; Pièce et Ricochet suivent le patch d’équilibrage.

| Graine | Prof. | Rareté | Avant | Après | Effet |
|---|---|---|---|---|---|
| Pièce | 1 | Commune | Recharge | **Recharge** | délai de recharge d’une charge -10 % |
| Pièce | 1 | Commune | Réserve | **Réserve** | charges de 2 à 3 |
| Pièce | 2 | Peu commune | Gel | **Pièce recyclée** | un ricochet qui tue rend 1 charge de pièce |
| Pièce | 3 | Rare | Brûlure | **Prime de ricochet** | le 1er ricochet de chaque pièce fait x2 au lieu de x1,5 |
| Pièce | 3 | Rare | Contagion | **Pièce paratonnerre** | les orbes ennemies à 6 m dévient vers tes pièces |
| Marque | 2 | Peu commune | Brûlure | **Marque tenace** | la marque tient 3 s de plus hors de ta vue |
| Marque | 3 | Rare | Gel | **Prime de chasse** | un kill sur cible marquée rend 1 charge de marque |
| Marque | 4 | Épique | Contagion | **Meute** | chaque kill marqué : +10 % de dégâts 5 s (5 max) |
| Charge | 2 | Peu commune | Contagion | **Relance** | un tir chargé qui tue relance la fenêtre (+1 s) |
| Charge | 3 | Rare | Brûlure | **Charge cinétique** | la touche chargée gagne +1 % par m/s de vitesse |
| Charge | 3 | Rare | Gel | **Charge de secours** | perdre la charge rend 50 % de sa recharge |
| Ricochet | 1 | Commune | Rebond+ | **Durée** | +1 s de rebonds sans limite |
| Ricochet | 1 | Commune | Portée | **Cadence rebondie** | pendant la compétence : délai entre tirs -10 % |
| Ricochet | 2 | Peu commune | Gel | **Rebond vorace** | chaque kill pendant la compétence : +0,5 s |
| Ricochet | 3 | Rare | Brûlure | **Munition boomerang** | un kill par un tir rebondissant rend sa munition |
| Ricochet | 3 | Rare | Contagion | **Rebond fatal** | un projectile ayant rebondi 4 fois ou plus : x1,5 |
| Ricochet | 4 | Épique | Sillage | **Tir sans délai** | pendant la compétence : aucun délai entre tes tirs |
| All in One | 1 | Commune | Brûlure | **Salve lourde** | +10 % de dégâts sur les tirs de la salve |
| All in One | 2 | Peu commune | Gel | **Prime de salve** | un kill pendant la salve réduit la recharge de 2 s |
| All in One | 4 | Épique | Contagion | **Salve complète** | salve avec chargeur plein : +30 % et +1 tir |
| Mirage | 3 | Rare | Gel | **Leurre blindé** | le leurre absorbe 3 tirs avant de disparaître |
| Mirage | 3 | Rare | Brûlure | **Permutation** | réappuie pour échanger ta place avec le leurre |
| Mirage | 4 | Épique | Contagion | **Illusion totale** | tant que le leurre existe, les ennemis t’ignorent |
| Aimant | 1 | Commune | Gel | **Verrou lourd** | +10 % de dégâts sur la cible verrouillée |
| Aimant | 3 | Rare | Brûlure | **Œil de lynx** | la cible verrouillée est visible à travers les murs |
| Aimant | 3 | Rare | Contagion | **Rafale guidée** | chaque touche verrouillée : délai de tir -0,1 s |

## Fusions retravaillées (31)

| Paire | Fusion | Effet |
|---|---|---|
| Railgun × Shotgun | **DOUBLE DÉTENTE** (avant : CANON SCIÉ) ✓ validée | un ennemi touché par ton rail est « ouvert » 3 s : tes plombs sur lui ne perdent plus de dégâts avec la distance et font x1,3 ; chaque plomb qui touche réduit la recharge du rail de 0,05 s (1,5 s au plus) |
| Railgun × Scie | **SCIE PERFORANTE** | un rail qui traverse ta scie en vol la propulse le long de sa ligne (70 m/s) : elle traverse alors toutes les cibles, sans rebondir, et revient à toi |
| Railgun × Ventouse | **RELAIS DE MINE** (avant : DÉMINAGE PROFOND) ✓ validée | un rail qui passe à moins de 4 m d’une de tes mines s’y courbe, la fait exploser (x1,5) puis repart à 100 % vers l’ennemi le plus proche (30 m) |
| Roquettes × Tesla | **ORAGE EXPLOSIF** | le souffle d’une roquette décharge ta batterie en éclairs sur tous les ennemis touchés (20 de batterie = 1 éclair, 70 dégâts) ; un kill au Tesla rend 25 points de jauge de roquette |
| Railgun × Épée | **DUEL** | après un de tes parrys, ton prochain rail (3 s) fait x1,5 et rend 50 points de jauge |
| Railgun × Enclume | **CHOC LOURD** | une cible projetée au mur dans les 0,5 s perd 15 % de ses PV max en plus (3 % pour un boss) |
| Railgun × Yoyo | **ALIGNEMENT** | les ennemis frappés par le Yoyo restent « alignés » 1,5 s : ton rail se courbe pour les traverser tous, +25 % par cible touchée (+100 % au plus) |
| Railgun × Égide | **BOUCLIER PERCÉ** | ton rail ignore le bouclier de face des ennemis (Bastion) et le brise 5 s ; un parry parfait te rend 1 munition de rail |
| Roquettes × Épée | **RICOCHET D’ACIER** ✓ validée | ton épée peut parer tes propres roquettes : une roquette parée explose x1,5 et repart 2 fois |
| Roquettes × Yoyo | **GRAPPIN EXPLOSIF** | les ennemis frappés par le Yoyo sont piégés 3 s : s’ils meurent, ils explosent comme une roquette (4 m, x1,5) |
| Roquettes × Égide | **BOUCLIER RÉACTIF** | un parry parfait transforme tous les projectiles ennemis à 9 m en roquettes renvoyées vers leurs tireurs (x1,5) |
| Shotgun × Yoyo | **PIÈGE À FLEAU** | une cible frappée par le Yoyo est « collée » 2 s : ta salve de Shotgun sur elle fait x1,6 sans perte à distance |
| Élan × Yoyo | **FRONDE** | le Yoyo frappe avec ta vitesse comme l’Élan : +5 dégâts par m/s sur chaque cible de l’arc, sans dépenser cette vitesse |
| Scie × Yoyo | **BOLAS** | une cible frappée par le Yoyo est marquée 2 s : ta scie la frappe en priorité, en vol et au retour (x1,5) |
| Ventouse × Yoyo | **MINE FOUETTÉE** (avant : HAMEÇON) | un coup de Yoyo sur une de tes mines la lance dans l’arc : elle explose sur le premier ennemi touché (x1,5) |
| Tesla × Yoyo | **FLEAU ÉLECTRIQUE** | un coup de Yoyo charge les ennemis touchés 2 s : ton faisceau Tesla les touche tous à la fois (60 %, sans limite de portée entre eux) |
| Railgun × Marque | **PRIORITÉ CIBLE** | un rail sur une cible marquée renouvelle sa marque (4 s) et traverse les ennemis suivants sans perte de dégâts ; la tuer rend 50 points de jauge |
| Railgun × Charge | **SURTENSION** | la touche chargée du rail ne se perd plus quand un ennemi normal te touche (x1,2 au lieu de x1,5) ; elle se perd sur un coup de boss |
| Railgun × Mirage | **TIR DÉPORTÉ** | un rail qui traverse ton leurre repart de sa position vers l’ennemi le plus proche à 100 %, et le leurre dure 1 s de plus |
| Railgun × Aimant | **EXÉCUTION** | pendant l’Aimant, une touche au corps compte comme une touche à la tête (x1,5, chaîne +2) |
| Roquettes × Pièce | **FEU D’ARTIFICE** | une roquette redirigée par une pièce (comme dans Ultrakill) explose à chaque pièce traversée (souffle 3 m, 60 dégâts) avant de repartir x1,5 vers l’ennemi le plus proche ; les pièces touchées ne sont pas consommées |
| Roquettes × Ricochet | **ARTILLERIE** | pendant la compétence, chaque rebond de roquette fait un souffle de 3 m (60 dégâts) et le rayon gagne 0,5 m à chaque rebond (6 m au plus) |
| Roquettes × All in One | **TAPIS DE BOMBES** | l’All in One tire une salve de 3 roquettes en éventail par munition de jauge |
| Roquettes × Aimant | **MISSILE TÉLÉGUIDÉ** | pendant l’Aimant, la roquette perce chaque cible et y déclenche ses effets (souffle compris) avant de repartir vers la suivante |
| Shotgun × Pièce | **PIÈCE MITRAILLÉE** | ta salve de Shotgun lance 6 pièces en éventail sans consommer de charge de pièce ; une pièce touchée par un plomb ricoche x1,5 sur 3 cibles |
| Shotgun × Ricochet | **SALVE DU BILLARD** | pendant la compétence, les plombs rebondissent sans limite : un plomb perdu en bout de portée revient par les murs et chaque rebond fait +10 % |
| Élan × Ricochet | **BILLE** | pendant la compétence, l’Élan rebondit sans limite et chaque rebond te rend 20 % de la vitesse dépensée |
| Scie × Ricochet | **BOOMERANG** | pendant la compétence, la scie ne se plante plus : elle rebondit sur le décor jusqu’à toucher une cible, puis revient à toi (36 m/s) |
| Ventouse × Ricochet | **BILLE MINE** | pendant la compétence, la mine rebondit jusqu’à toucher un ennemi, puis colle et explose x1,5 |
| Tesla × Ricochet | **ARC RICOCHANT** | pendant la compétence, l’arc rebondit sur le décor sans limite de portée (60 % par rebond) et frappe aussi les ennemis à 6 m de chaque rebond |
| Tesla × All in One | **SURTENSION TOTALE** | l’éclair inévitable de l’All in One vide la batterie : il frappe chaque ennemi à 25 m en ligne de vue, dégâts x(batterie / 10) |

## Fusions renommées (doublons de nom)

- Ventouse × Épée : COUP DE GRÂCE → **DÉTONATEUR**
- Élan × Mirage : DOUBLE → **FANTÔME RAPIDE**

## Formes fusionnées (21 paires à distance, proposition à valider)

Forme fusionnée (proposition à valider) : acheter la fusion donne une jauge de fusion, remplie par les dégâts des deux armes. Pleine, une touche active l’arme hybride pendant 15 s : elle remplace les deux armes à distance (leurs munitions sont mises de côté puis rendues), hérite des modificateurs des deux arbres stat par stat (dégâts, projectiles, zone, portée…), puis les deux armes reviennent. La mêlée et la compétence restent disponibles.

| Paire | Fusion (synergie passive) | Forme fusionnée | Prend à chaque parent | Effet |
|---|---|---|---|---|
| Railgun × Roquettes | FISSION ✓ | **ARTILLERIE ORBITALE** | tir du Railgun × explosion des Roquettes | un trait instantané qui traverse tout, explose (8 m) au bout de sa course et à chaque cible traversée (3 m) ; dégâts du rail, souffle de la roquette |
| Railgun × Shotgun | DOUBLE DÉTENTE ✓ | **CANON SCIÉ** | motif du Shotgun × projectile du Railgun | 8 rails fins en gerbe (cône de 6°), chacun traverse les ennemis à 50 % des dégâts du rail ; portée 120 m ; le recul du Shotgun te propulse |
| Railgun × Élan | VITESSE MORTELLE | **LUMIÈRE VIVE** | dégâts de l’Élan × tir du Railgun | un fusil unique à tir instantané et perforant : +8 dégâts par m/s de ta vitesse, qui n’est jamais dépensée ; 4 munitions, un tir toutes les 0,5 s |
| Railgun × Scie | SCIE PERFORANTE | **SCIE ÉCLAIR** | lancer de la Scie × vitesse du Railgun | une scie qui part à la vitesse d’un rail : elle découpe tout sur sa ligne puis revient par la même ligne (second passage à 50 %), sans jamais se planter |
| Railgun × Ventouse | RELAIS DE MINE ✓ | **FUSIL MINEUR** | tir du Railgun × mines de la Ventouse | un seul tir instantané qui traverse tout et laisse une mine (x1,5) sur chaque ennemi touché, 3 au plus ; les mines explosent ensemble au second appui |
| Railgun × Tesla | COURANT PERMANENT | **FAISCEAU ORBITAL** | faisceau du Tesla × perforation du Railgun | un faisceau continu de 28 m qui traverse les ennemis, +10 % de dégâts par seconde de contact ; consomme la batterie et la jauge du rail |
| Roquettes × Shotgun | SALVE D’ARTILLERIE | **OBUSIER** | motif du Shotgun × projectile des Roquettes | 6 mini-roquettes en gerbe (cône de 8°), souffle de 2,5 m chacune ; le recul t’envoie en l’air |
| Roquettes × Élan | BOULET DE CANON | **CANON CINÉTIQUE** | vitesse de l’Élan × roquette | une roquette dont la vitesse et le souffle montent avec les tiens (+0,2 m de rayon par m/s) et qui te rend la moitié de la vitesse dépensée |
| Roquettes × Scie | ESCADRILLE | **SCIES-FUSÉES** | retour de la Scie × explosion des Roquettes | une scie propulsée par roquette, escortée de 3 lames en orbite : chaque touche explose (2 m), puis elle revient à toi |
| Roquettes × Ventouse | ARTILLERIE LOURDE | **MORTIER À MÈCHE** | projectile des Roquettes × collage de la Ventouse | la roquette colle où elle touche et attend : jusqu’à 3 roquettes collées explosent ensemble au second appui (rayon x1,5) |
| Roquettes × Tesla | ORAGE EXPLOSIF | **ROQUETTE-ORAGE** | roquette × arcs du Tesla | en vol, la roquette tire un arc sur 3 cibles toutes les 0,3 s ; à l’impact le souffle enchaîne un éclair sur chaque ennemi touché |
| Shotgun × Élan | FUSIL SOLAIRE | **SOLEIL** | motif du Shotgun × conversion de l’Élan | la salve convertit ta vitesse (+10 % de dégâts par m/s) et le recul ne t’en retire rien : tu gardes toute ta vitesse |
| Shotgun × Scie | HACHOIR | **SCIE À POMPE** | motif du Shotgun × lames de la Scie | la salve lance 5 mini-lames en éventail ; chacune revient vers toi en touchant encore (60 %) |
| Shotgun × Ventouse | PIÈGE À PLOMB | **GERBE DE MINES** | motif du Shotgun × mines de la Ventouse | la salve colle 6 mini-mines (30 %) en éventail ; elles explosent ensemble au second appui ou à 2 m d’un ennemi |
| Shotgun × Tesla | DÉCHARGE | **FUSIL À ARCS** | motif du Shotgun × arcs du Tesla | un cône de 8 arcs, chacun chaîne vers 2 cibles (40 %) ; la salve coûte 20 de batterie au lieu de la jauge |
| Élan × Scie | VITESSE DE LAME | **SCIE CINÉTIQUE** | vitesse de l’Élan × lame de la Scie | une scie lancée avec ta vitesse convertie (+6 dégâts par m/s) ; son retour te rend toute la vitesse dépensée |
| Élan × Ventouse | CATAPULTE | **MINE-BÉLIER** | vitesse de l’Élan × mine de la Ventouse | une mine lancée à ta vitesse (x2) qui te propulse de 12 à son explosion et fait +5 dégâts par m/s |
| Élan × Tesla | COURT-CIRCUIT | **ARC CINÉTIQUE** | vitesse de l’Élan × faisceau du Tesla | un faisceau dont la portée et les dégâts montent avec ta vitesse (+1 m et +5 % par m/s) ; courir recharge la batterie de 2 par seconde |
| Scie × Ventouse | MINE À LAME | **SCIE-MINE** | lame de la Scie × mines de la Ventouse | la lame plantée devient une mine : elle explose (4 m) au rappel ; un seul lancer pose et rappelle |
| Scie × Tesla | LAME ÉLECTRIQUE | **SCIE CONDUCTRICE** | lame de la Scie × arcs du Tesla | en vol la lame émet un arc continu vers l’ennemi à 8 m ; plantée, elle devient un relais de 10 m |
| Ventouse × Tesla | BOBINE | **CHAMP ÉLECTRIQUE** | mines de la Ventouse × arcs du Tesla | tes mines forment un réseau : arcs de 8 m entre elles (40 %), l’arc qui traverse une mine la fait exploser |

## Capstones réécrits (36)

| Branche | Capstone | Effet |
|---|---|---|
| Railgun → Shotgun | **Rail d’ouverture** | la touche du rail ouvre la cible 3 s : plombs x1,3 sans perte à distance |
| Railgun → Ventouse | **Rail sur mine** | le rail déclenche tes mines sans les armer et double leur rayon (8 m) |
| Railgun → Épée | **Rail de duelliste** | après un parry, le prochain rail part sans délai de tir |
| Railgun → Enclume | **Rail de choc** | le rail repousse la cible de 8 |
| Railgun → Yoyo | **Rail courbe** | le rail se courbe de 15° vers les cibles frappées par le Yoyo (1,5 s) |
| Railgun → Charge | **Rail condensé** | la charge tient pour 2 rails de suite (fenêtre de 3 s) |
| Roquettes → Lance | **Roquette montée** | lance couchée, la roquette part de la pointe à la vitesse de joute (+100 %) |
| Roquettes → Impulsion | **Roquette lente** | la roquette ralentit de 40 % à moins de 5 m de toi (pour la frapper) |
| Roquettes → Égide | **Roquette réactive** | les projectiles que ton bouclier bloque rechargent 20 points de jauge de roquette |
| Roquettes → Pièce | **Roquette-pièce** | la roquette redirigée garde sa vitesse et son rayon d’explosion |
| Roquettes → Ricochet | **Roquette ricochante** | pendant la compétence, la roquette garde toute sa vitesse à chaque rebond |
| Roquettes → All in One | **Roquette salve** | l’All in One tire 8 roquettes en plus |
| Roquettes → Aimant | **Roquette guidée** | pendant l’Aimant, la roquette est téléguidée et ne coûte qu’une munition |
| Shotgun → Railgun | **Plomb d’amorçage** | chaque plomb qui touche ajoute 5 points à la jauge de recharge du rail |
| Shotgun → Yoyo | **Shotgun à fouet** | les plombs sur une cible frappée par le Yoyo font x1,3 |
| Shotgun → Pièce | **Shotgun à pièces** | remplace tous les plombs par des pièces (sans charge de pièce) |
| Élan → Yoyo | **Élan fouetté** | le tir plein déclenche un balayage de Yoyo gratuit (50 %) |
| Scie → Yoyo | **Scie fouettée** | un coup de Yoyo sur ta scie plantée la relance (x1,5) |
| Ventouse → Yoyo | **Mine fouettée** | un coup de Yoyo sur une mine la lance de 8 m |
| Tesla → All in One | **Tesla multiple** | l’éclair inévitable de l’All in One frappe 3 cibles |
| Épée → Railgun | **Épée au rail** | l’orbe parrée repart en rail instantané (60 % de ses dégâts) |
| Yoyo → Railgun | **Yoyo au rail** | les cibles frappées restent marquées 1,5 s pour le rail |
| Yoyo → Roquettes | **Yoyo au souffle** | chaque cible touchée subit un souffle de 3 m |
| Yoyo → Shotgun | **Yoyo à plombs** | le balayage lâche 5 plombs en cône à 6 m |
| Yoyo → Élan | **Yoyo cinétique** | le balayage ajoute ta vitesse aux dégâts (+3 par m/s) |
| Impulsion → Roquettes | **Poing de roquette** | le coup envoie une roquette (60 %) |
| Pièce → Roquettes | **Pièce explosive** | la pièce explose quand une roquette la traverse (3 m) |
| Pièce → Ventouse | **Pièce collante** | la pièce se colle au décor |
| Marque → Shotgun | **Marque de Shotgun** | les ennemis proches de la cible touchée sont marqués |
| Charge → Railgun | **Charge rail** | la touche chargée traverse tout sans perte de dégâts |
| Charge → Shotgun | **Charge de Shotgun** | la charge élargit le cône |
| Ricochet → Railgun | **Ricochet rail** | pendant la compétence, chaque rebond de rail garde 100 % de ses dégâts |
| Ricochet → Roquettes | **Ricochet explosif** | chaque rebond de roquette explose (3 m) |
| Ricochet → Shotgun | **Ricochet en gerbe** | chaque rebond de plomb se divise en 2 plombs (50 %) |
| All in One → Tesla | **Salve Tesla** | l’éclair décharge toute la batterie (dégâts x batterie / 10) |
| Mirage → Railgun | **Leurre-relais** | le leurre relaie tes rails au lieu de les absorber |

## Revue des branches

19 validée(s), 0 supprimée(s), 19 modifiée(s) depuis leur tri, 186 à trier.

| Branche | Revue | Note | Commentaire |
|---|---|---|---|
| Railgun → Roquettes | ~ modifiée depuis le tri |  |  |
| Railgun → Shotgun | ~ modifiée depuis le tri |  | satelite non pertinent tir explosif et detonation non plus |
| Railgun → Élan | ~ modifiée depuis le tri |  | la forme fusionée semble indiquer deux armes mais sinon valide |
| Railgun → Scie | ✓ validée |  |  |
| Railgun → Ventouse | ~ modifiée depuis le tri |  | capstone a revoir fusion validé forme fusioné a revoir |
| Railgun → Tesla | ✓ validée |  |  |
| Railgun → Épée | ~ modifiée depuis le tri |  | a rework pour pve les enemy peuvent pas parry |
| Railgun → Enclume | ~ modifiée depuis le tri |  | les degat suplementaire devrais etre en %de vie max et detonation devrait etre remplacer dans se cas par une size et hitbox upgrade |
| Railgun → Lance | ✓ validée |  |  |
| Railgun → Yoyo | ~ modifiée depuis le tri |  | le yoyo a pas de traction |
| Railgun → Impulsion | ✓ validée |  | car encourage les combo soit l'archetype d'impulsion |
| Railgun → Égide | ~ modifiée depuis le tri |  | fusion a rework pour pve |
| Railgun → Pièce | ✓ validée |  |  |
| Railgun → Marque | ✓ validée |  |  |
| Railgun → Charge | ~ modifiée depuis le tri |  | etape valide mais fusiont et capstone a rework |
| Railgun → Ricochet | ✓ validée |  |  |
| Railgun → All in One | ✓ validée |  |  |
| Railgun → Mirage | ✓ validée |  |  |
| Railgun → Aimant | ~ modifiée depuis le tri |  | etape validé mais  rework fusion pour pve |
| Roquettes → Railgun | ✓ validée |  |  |
| Roquettes → Shotgun | ✓ validée |  |  |
| Roquettes → Élan | ✓ validée |  |  |
| Roquettes → Scie | ✓ validée |  | mais a play test |
| Roquettes → Ventouse | ✓ validée |  |  |
| Roquettes → Tesla | ~ modifiée depuis le tri |  | fusion non valide forme fusioné valide |
| Roquettes → Épée | ~ modifiée depuis le tri |  | mais changement au fonctionement naturelle de l'épée qui autorise la parry de ses propre projectile |
| Roquettes → Enclume | ✓ validée |  |  |
| Roquettes → Lance | ~ modifiée depuis le tri |  | mais capstone a rework |
| Roquettes → Yoyo | ~ modifiée depuis le tri |  | capstone valide mais fusion a rework car pas de traction du yoyo |
| Roquettes → Impulsion | ~ modifiée depuis le tri |  | mais la capstone devrait ètre celle de l'autre coté et danse se cas la capstone reduit la vitesse de la roquete |
| Roquettes → Égide | ~ modifiée depuis le tri |  | capstone a rework et fusion a reword "transforme tous le projectiles proche en roquete" |
| Roquettes → Pièce | ✓ validée |  |  |
| Roquettes → Marque | ✓ validée |  |  |
| Roquettes → Charge | ✓ validée |  |  |
| Roquettes → Ricochet | ~ modifiée depuis le tri |  | mais capstone a reworks avec le rework récent de ricochet |
| Roquettes → All in One | ~ modifiée depuis le tri |  | capstone valité mais reword tire 8 roquettes suplémentaires et la fusion tire une salve par munition |
| Roquettes → Mirage | ✓ validée |  |  |
| Roquettes → Aimant | ~ modifiée depuis le tri |  | captone okay mais réduit a une munition et la fusion chaque cible touché déclenche les effet de la roquete et la roquete perce la cible |

## Chemins

```
paires de branches qui cohabitent : 1113 ; étapes communes 0/1/2/3/4 = 645 / 468 / 0 / 0 / 0
étapes inutilisées : 57 ; utilisation maximale d’une étape : 5 branches
```

En v1 : 297 paires de branches cohabitant dans un loadout avaient un chemin identique (les 4 étapes), les 1404 autres n’avaient rien en commun. En v2 : aucun chemin identique, au plus une étape commune.
