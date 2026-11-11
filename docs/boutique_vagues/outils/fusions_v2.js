// Fusions et capstones retravailles dans la v2.
// Les 11 fusions validees par le proprietaire (FISSION, DUEL, CHOC LOURD, CHEVALIER, COMBO AERIEN, BOUCLIER PERCE, BILLARD,
// SURTENSION, CASCADE, FUSILLADE, EXECUTION) ne sont pas touchees. Seuls deux libelles de capstone changent a leur contact :
// RAIL>ENC (nom identique a une etape du pool) et RIC>RAIL (la competence Ricochet est reecrite : duree, rebonds sans limite).
//
// fusions : 'A-B': { n: nouveau nom, e: nouvel effet, ab: [nom, effet] capstone A->B, ba: [nom, effet] capstone B->A }
// capstones : 'A>B': [nom, effet]
// noms : 'A-B': nouveau nom (doublons)

const fusions = {
  // ---- Railgun : les 8 fusions non validees --------------------------------------------------------------------------
  'RAIL-SHO': {
    valide: true,                                       // « double détente est parfaite » (propriétaire) : ne plus y toucher
    n: 'DOUBLE DÉTENTE',
    e: 'un ennemi touché par ton rail est « ouvert » 3 s : tes plombs sur lui ne perdent plus de dégâts avec la distance et font x1,3 ; chaque plomb qui touche réduit la recharge du rail de 0,05 s (1,5 s au plus)',
    ab: ['Rail d’ouverture', 'la touche du rail ouvre la cible 3 s : plombs x1,3 sans perte à distance'],
    ba: ['Plomb d’amorçage', 'chaque plomb qui touche ajoute 5 points à la jauge de recharge du rail'],
  },
  'RAIL-SCI': {
    e: 'un rail qui traverse ta scie en vol la propulse le long de sa ligne (70 m/s) : elle traverse alors toutes les cibles, sans rebondir, et revient à toi',
  },
  'RAIL-VEN': {
    valide: true,                                       // « fusion validée » (revue du propriétaire) ; capstone et forme fusionnée à revoir
    n: 'RELAIS DE MINE',
    e: 'un rail qui passe à moins de 4 m d’une de tes mines s’y courbe, la fait exploser (x1,5) puis repart à 100 % vers l’ennemi le plus proche (30 m)',
    ab: ['Rail sur mine', 'le rail déclenche tes mines sans les armer et double leur rayon (8 m)'],
  },
  'RAIL-MAR': {
    e: 'un rail sur une cible marquée renouvelle sa marque (4 s) et traverse les ennemis suivants sans perte de dégâts ; la tuer rend 50 points de jauge',
  },
  'RAIL-MIR': {
    e: 'un rail qui traverse ton leurre repart de sa position vers l’ennemi le plus proche à 100 %, et le leurre dure 1 s de plus',
    ba: ['Leurre-relais', 'le leurre relaie tes rails au lieu de les absorber'],
  },

  // ---- Demandes du proprietaire ---------------------------------------------------------------------------------------
  // Roquettes x Piece, "comme dans Ultrakill" (la base du jeu fait deja rediriger la roquette par la piece : la fusion l'amplifie)
  'ROQ-PIE': {
    e: 'une roquette redirigée par une pièce (comme dans Ultrakill) explose à chaque pièce traversée (souffle 3 m, 60 dégâts) avant de repartir x1,5 vers l’ennemi le plus proche ; les pièces touchées ne sont pas consommées',
    ab: ['Roquette-pièce', 'la roquette redirigée garde sa vitesse et son rayon d’explosion'],
    ba: ['Pièce explosive', 'la pièce explose quand une roquette la traverse (3 m)'],
  },
  // Tesla x All in One : la base du jeu donne un eclair inevitable a l'All in One tenu avec le Tesla
  'TES-AIO': {
    e: 'l’éclair inévitable de l’All in One vide la batterie : il frappe chaque ennemi à 25 m en ligne de vue, dégâts x(batterie / 10)',
    ab: ['Tesla multiple', 'l’éclair inévitable de l’All in One frappe 3 cibles'],
    ba: ['Salve Tesla', 'l’éclair décharge toute la batterie (dégâts x batterie / 10)'],
  },

  // Shotgun x Piece : l'exemple de synergie "support" du proprietaire (un shotgun qui projette plein de pieces)
  'SHO-PIE': {
    e: 'ta salve de Shotgun lance 6 pièces en éventail sans consommer de charge de pièce ; une pièce touchée par un plomb ricoche x1,5 sur 3 cibles',
    ab: ['Shotgun à pièces', 'la salve lance 3 pièces en plus des plombs, sans charge de pièce'],
  },

  // ---- Revue du proprietaire : les 38 premieres branches (Railgun et Roquettes) ------------------------------------------
  // Railgun : les fusions pensees pour le PvP sont reecrites pour le PvE (les ennemis ne parent pas, pas de plafond PvP).
  'RAIL-EPE': {
    force: true,                                        // DUEL etait valide ; reecrit pour le PvE a la demande du proprietaire
    e: 'après un de tes parrys, ton prochain rail (3 s) fait x1,5 et rend 50 points de jauge',
    ab: ['Rail de duelliste', 'après un parry, le prochain rail part sans délai de tir'],
    ba: ['Épée au rail', 'l’orbe parrée repart en rail instantané (60 % de ses dégâts)'],
  },
  'RAIL-ENC': {
    force: true,                                        // CHOC LOURD : degats en % des PV max
    e: 'une cible projetée au mur dans les 0,5 s perd 15 % de ses PV max en plus (3 % pour un boss)',
  },
  'RAIL-YOY': {                                         // sans traction du Yoyo
    e: 'les ennemis frappés par le Yoyo restent « alignés » 1,5 s : ton rail se courbe pour les traverser tous, +25 % par cible touchée (+100 % au plus)',
    ab: ['Rail courbe', 'le rail se courbe de 15° vers les cibles frappées par le Yoyo (1,5 s)'],
    ba: ['Yoyo au rail', 'les cibles frappées restent marquées 1,5 s pour le rail'],
  },
  'RAIL-EGI': {
    force: true,                                        // BOUCLIER PERCE : le parry parfait inerte contre toi n'a de sens qu'en PvP
    e: 'ton rail ignore le bouclier de face des ennemis (Bastion) et le brise 5 s ; un parry parfait te rend 1 munition de rail',
  },
  'RAIL-CHA': {
    force: true, sansNote: true,                        // SURTENSION : version Vagues
    e: 'la touche chargée du rail ne se perd plus quand un ennemi normal te touche (x1,2 au lieu de x1,5) ; elle se perd sur un coup de boss',
    ab: ['Rail condensé', 'la charge tient pour 2 rails de suite (fenêtre de 3 s)'],
    ba: ['Charge rail', 'la touche chargée traverse tout sans perte de dégâts'],
  },
  'RAIL-AIM': {
    force: true, sansNote: true,                        // EXECUTION : pas de plafond PvP en Vagues
    e: 'pendant l’Aimant, une touche au corps compte comme une touche à la tête (x1,5, chaîne +2)',
  },
  // Roquettes
  'ROQ-TES': {                                          // la forme fusionnee ROQUETTE-ORAGE est validee
    e: 'le souffle d’une roquette décharge ta batterie en éclairs sur tous les ennemis touchés (20 de batterie = 1 éclair, 70 dégâts) ; un kill au Tesla rend 25 points de jauge de roquette',
  },
  'ROQ-EPE': {
    valide: true,                                       // validee, avec la regle ecrite : l'epee peut parer tes propres roquettes
    e: 'ton épée peut parer tes propres roquettes : une roquette parée explose x1,5 et repart 2 fois',
  },
  'ROQ-YOY': {                                          // sans traction du Yoyo
    e: 'les ennemis frappés par le Yoyo sont piégés 3 s : s’ils meurent, ils explosent comme une roquette (4 m, x1,5)',
    ba: ['Yoyo au souffle', 'chaque cible touchée subit un souffle de 3 m'],
  },
  'ROQ-EGI': {
    e: 'un parry parfait transforme tous les projectiles ennemis à 9 m en roquettes renvoyées vers leurs tireurs (x1,5)',
    ab: ['Roquette réactive', 'les projectiles que ton bouclier bloque rechargent 20 points de jauge de roquette'],
  },
  'ROQ-AIO': {
    e: 'l’All in One tire une salve de 3 roquettes en éventail par munition de jauge',
    ab: ['Roquette salve', 'l’All in One tire 8 roquettes en plus'],
  },
  'ROQ-AIM': {
    e: 'pendant l’Aimant, la roquette perce chaque cible et y déclenche ses effets (souffle compris) avant de repartir vers la suivante',
    ab: ['Roquette guidée', 'pendant l’Aimant, la roquette est téléguidée et ne coûte qu’une munition'],
  },

  // ---- Yoyo sans traction (le proprietaire retire la traction du jeu) : le Yoyo devient un balayage de zone ---------------
  // arc de 140 deg, 6 m, touche tout ce qui est dans l'arc, parry a distance, frappe les pieces, rend de la jauge
  'SHO-YOY': {
    e: 'une cible frappée par le Yoyo est « collée » 2 s : ta salve de Shotgun sur elle fait x1,6 sans perte à distance',
    ab: ['Shotgun à fouet', 'les plombs sur une cible frappée par le Yoyo font x1,3'],
    ba: ['Yoyo à plombs', 'le balayage lâche 5 plombs en cône à 6 m'],
  },
  'ELA-YOY': {
    e: 'le Yoyo frappe avec ta vitesse comme l’Élan : +5 dégâts par m/s sur chaque cible de l’arc, sans dépenser cette vitesse',
    ab: ['Élan fouetté', 'le tir plein déclenche un balayage de Yoyo gratuit (50 %)'],
    ba: ['Yoyo cinétique', 'le balayage ajoute ta vitesse aux dégâts (+3 par m/s)'],
  },
  'SCI-YOY': {
    e: 'une cible frappée par le Yoyo est marquée 2 s : ta scie la frappe en priorité, en vol et au retour (x1,5)',
    ab: ['Scie fouettée', 'un coup de Yoyo sur ta scie plantée la relance (x1,5)'],
  },
  'VEN-YOY': {
    n: 'MINE FOUETTÉE',
    e: 'un coup de Yoyo sur une de tes mines la lance dans l’arc : elle explose sur le premier ennemi touché (x1,5)',
    ab: ['Mine fouettée', 'un coup de Yoyo sur une mine la lance de 8 m'],
  },
  'TES-YOY': {
    e: 'un coup de Yoyo charge les ennemis touchés 2 s : ton faisceau Tesla les touche tous à la fois (60 %, sans limite de portée entre eux)',
  },
  'YOY-PIE': {
    e: 'une pièce frappée par le Yoyo ricoche sur 3 cibles à la suite (x1,5), en commençant par celles de l’arc',
    ba: ['Pièce rappelée', 'la pièce frappée revient vers toi après son ricochet'],
  },
  'YOY-MAR': {
    e: 'un coup de Yoyo sur une cible marquée marque aussi tous les ennemis de l’arc (4 s)',
    ba: ['Marque de fleau', 'le balayage marque toutes les cibles touchées'],
  },
  'YOY-CHA': {
    e: 'la touche chargée du Yoyo balaie à 360° et 12 m (x1,5)',
    ab: ['Yoyo chargé', 'le balayage chargé fait x2 et touche à 360°'],
  },
  'YOY-AIO': {
    e: 'l’All in One fait 5 tours de fleau d’affilée, chacun touche tout ce qui est à 6 m',
  },
  'YOY-MIR': {
    e: 'ton leurre fait un tour de Yoyo à 8 m chaque seconde (60 %)',
    ab: ['Yoyo trompeur', 'le leurre fait un balayage de Yoyo (60 %)'],
    ba: ['Leurre de fleau', 'le leurre fait un balayage de Yoyo toutes les 2 s'],
  },
  'YOY-AIM': {
    n: 'FOUET GUIDÉ',
    e: 'pendant l’Aimant, le Yoyo touche la cible verrouillée jusqu’à 14 m',
  },

  // ---- Ricochet : la competence est reecrite (duree, rebonds sans limite, portee sans limite) -------------------------
  'ROQ-RIC': {
    e: 'pendant la compétence, chaque rebond de roquette fait un souffle de 3 m (60 dégâts) et le rayon gagne 0,5 m à chaque rebond (6 m au plus)',
    ab: ['Roquette ricochante', 'pendant la compétence, la roquette garde toute sa vitesse à chaque rebond'],
    ba: ['Ricochet explosif', 'chaque rebond de roquette explose (3 m)'],
  },
  'SHO-RIC': {
    e: 'pendant la compétence, les plombs rebondissent sans limite : un plomb perdu en bout de portée revient par les murs et chaque rebond fait +10 %',
    ba: ['Ricochet en gerbe', 'chaque rebond de plomb se divise en 2 plombs (50 %)'],
  },
  'ELA-RIC': {
    e: 'pendant la compétence, l’Élan rebondit sans limite et chaque rebond te rend 20 % de la vitesse dépensée',
  },
  'SCI-RIC': {
    e: 'pendant la compétence, la scie ne se plante plus : elle rebondit sur le décor jusqu’à toucher une cible, puis revient à toi (36 m/s)',
  },
  'VEN-RIC': {
    e: 'pendant la compétence, la mine rebondit jusqu’à toucher un ennemi, puis colle et explose x1,5',
  },
  'TES-RIC': {
    e: 'pendant la compétence, l’arc rebondit sur le décor sans limite de portée (60 % par rebond) et frappe aussi les ennemis à 6 m de chaque rebond',
  },
  'EPE-RIC': {
    e: 'pendant la compétence, l’orbe parrée à l’épée rebondit sans limite : 150 dégâts à chaque ennemi touché',
    ba: ['Ricochet d’épée', 'l’orbe parrée rebondit vers l’ennemi suivant'],
  },
  'ENC-RIC': {
    e: 'pendant la compétence, l’onde rebondit sur les murs sans limite, à 70 % de sa force à chaque rebond',
    ba: ['Ricochet d’onde', 'l’onde se réfléchit sur les murs (70 %)'],
  },
  'LAN-RIC': {
    e: 'pendant la compétence, chaque joute réussie te fait rebondir vers la cible suivante : les joutes s’enchaînent sans recharge',
    ba: ['Ricochet de lance', 'la joute rebondit vers la cible suivante'],
  },
  'YOY-RIC': {
    e: 'pendant la compétence, la chaîne du Yoyo rebondit sur le décor et touche toutes les cibles sur son trajet',
  },
  'IMP-RIC': {
    e: 'pendant la compétence, le punch-jump est illimité : chaque mur touché rend la charge et garde ta vitesse',
    ba: ['Ricochet de poing', 'chaque coup sur un mur est un punch-jump'],
  },
  'EGI-RIC': {
    e: 'pendant la compétence, le rail renvoyé par un parry parfait rebondit sans limite (100 % de dégâts)',
  },
};

const capstones = {
  // libelles seulement, la fusion reste inchangee
  'RIC>RAIL': ['Ricochet rail', 'pendant la compétence, chaque rebond de rail garde 100 % de ses dégâts'],   // CASCADE (validee)
  'RAIL>ENC': ['Rail de choc', 'le rail repousse la cible de 8'],                                              // CHOC LOURD (validee) : meme nom qu'une etape
  'PIE>VEN': ['Pièce collante', 'la pièce se colle au décor'],                                                  // meme nom qu'une etape de la Pièce
  // revue du proprietaire
  'ROQ>LAN': ['Roquette montée', 'lance couchée, la roquette part de la pointe à la vitesse de joute (+100 %)'],
  'ROQ>IMP': ['Roquette lente', 'la roquette ralentit de 40 % à moins de 5 m de toi (pour la frapper)'],   // echange avec l'autre cote
  'IMP>ROQ': ['Poing de roquette', 'le coup envoie une roquette (60 %)'],
  // l'ancien nom de l'arme (minuscule) restait dans deux libelles de capstone
  'MAR>SHO': ['Marque de Shotgun', 'les ennemis proches de la cible touchée sont marqués'],
  'CHA>SHO': ['Charge de Shotgun', 'la charge élargit le cône'],
};

// doublons de noms de fusion
const noms = {
  'VEN-EPE': 'DÉTONATEUR',
  'ELA-MIR': 'FANTÔME RAPIDE',
  'EPE-MIR': 'PARRY FANTÔME',
};

module.exports = { fusions, capstones, noms };
