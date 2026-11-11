// Pools des armes a distance et de melee, ecrits autour des axes du proprietaire (voir axes.js).
// Une etape = une stat a modifier. Les effets speciaux (parry, joute, mines, etc.) sont dans les capstones et les fusions.
// Format : GRAINE: [ profondeur 1, 2, 3, 4 ] ; chaque profondeur = 5 etapes [nom (20 car. max), effet (niveau 1), axe].
//  - Distance : TAI VIT ZON POR MUN NBP DEG SUI (+ SOI, l'etape de soin, toujours en tete de la profondeur 3).
//  - Melee    : une etape par axe et par profondeur (CD ANI COM MUR VOL) ; le lifesteal de la profondeur 3 est en tete.
// Une etape ne parle jamais d'une autre graine (elle sert dans tous les loadouts).
// Le "nombre de projectiles" est propre a chaque arme : plombs, rails, mines, cibles de la chaine du Tesla ; pour la Scie
// (consigne du proprietaire), les projectiles en plus sont des lames qui ORBITENT autour de la scie.

module.exports = {
  // ======================================================== ARMES A DISTANCE ========================================
  RAIL: [
    [['Calibre', '+10 % de dégâts', 'DEG'], ['Réserve', '+20 % de points de jauge par kill', 'MUN'], ['Rail large', 'le tir est 15 % plus large', 'TAI'],
     ['Rail tendu', '+5 % de dégâts par cible déjà traversée (5 max)', 'POR'], ['Visée courbe', 'le tir se courbe de 1° vers l’ennemi proche', 'SUI']],
    [['Éclat d’impact', 'la 1re cible touchée subit un souffle de 1,5 m (30 %)', 'ZON'], ['Satellite', 'chaque tir lâche un éclat chercheur', 'NBP'],
     ['Fourche', 'le kill repart vers 2 ennemis (25 m)', 'SUI'], ['Surtension', 'touche à la tête : chaîne +3 au lieu de +2', 'DEG'],
     ['Lunette', 'la tête est 15 % plus large', 'TAI']],
    [['Sang-froid', 'SOIN : +6 PV par touche à la tête', 'SOI'], ['Doublé', 'tuer 2 ennemis avec un même tir rend 1 munition', 'MUN'],
     ['Calibre lourd', 'le tir traversant : +15 % sur la cible suivante', 'DEG'], ['Éclats', 'le kill libère 3 éclats chercheurs', 'NBP'],
     ['Tir explosif', 'la touche à la tête fait un souffle de 3 m (50 %)', 'ZON']],
    [['Rail jumeau', '20 % de chance de tirer deux rails', 'NBP'], ['Rail colossal', 'le tir est 40 % plus large, tête comprise', 'TAI'],
     ['Tir d’élite', '+50 % de dégâts du rail sur les élites', 'DEG'], ['Perce-mur', 'le tir traverse les murs de moins de 1 m (70 %)', 'POR'],
     ['Tir guidé', 'le tir se courbe de 8° vers l’ennemi proche', 'SUI']],
  ],

  ROQ: [
    [['Charge creuse', '+10 % de dégâts', 'DEG'], ['Soute', 'munition maximale +1', 'MUN'], ['Propulseur', 'vitesse du projectile +10 %', 'VIT'],
     ['Rayon', 'rayon d’explosion +0,4 m', 'ZON'], ['Ogive large', 'roquette 20 % plus grosse : touche directe facilitée', 'TAI']],
    [['Combustible', 'la roquette vole 20 m de plus avant de s’éteindre', 'POR'], ['Fusée de proximité', 'la roquette explose à 2 m d’un ennemi', 'SUI'],
     ['Double départ', '10 % de chance de tirer 2 roquettes', 'NBP'], ['Souffle plein', 'dégâts au bord du souffle x0,3 -> x0,45', 'ZON'],
     ['Impact direct', '+15 % sur la cible touchée en plein', 'DEG']],
    [['Cratère vital', 'SOIN : +6 PV par ennemi dans le souffle (3 max)', 'SOI'], ['Récupération', 'un kill avec 2 ennemis dans le souffle rend 1 munition', 'MUN'],
     ['Éclats', 'le kill libère 3 éclats chercheurs', 'NBP'], ['Grand souffle', 'rayon d’explosion +0,8 m', 'ZON'],
     ['Cœur du souffle', 'dégâts au centre du souffle +25 %', 'DEG']],
    [['Détonation en chaîne', 'le kill explose 3,5 m, 45 dégâts', 'ZON'], ['Salve', 'tire 2 roquettes à 60 %', 'NBP'],
     ['Trajectoire courbe', 'la roquette dévie vers l’ennemi proche', 'SUI'], ['Grappe', 'se divise en 3 mini-roquettes (30 %)', 'NBP'],
     ['Roquette lourde', '+40 % de dégâts, vitesse -10 %', 'DEG']],
  ],

  SHO: [
    [['Poing de plomb', 'x1,1 sous 5 m', 'DEG'], ['Chevrotine', 'perte de dégâts à distance -10 %', 'POR'], ['Pompe vive', 'délai entre deux salves -15 %', 'CD'],
     ['Gros plomb', 'taille des plombs +15 %', 'TAI'], ['Rechargement', '+20 % de points de jauge par kill', 'MUN']],
    [['Gerbe large', 'cône +1,5° : la salve touche plus d’ennemis', 'ZON'], ['Rafale', 'deux salves à 0,15 s d’écart, 50 %', 'NBP'],
     ['Plombs chercheurs', 'les plombs se courbent de 3° vers la cible', 'SUI'], ['Bout portant', 'sous 3 m : x1,25', 'DEG'],
     ['Cône serré', 'cône -1°, portée utile +5 m', 'POR']],
    [['Peau de plomb', 'SOIN : +8 PV si 6 plombs ou plus touchent', 'SOI'], ['Plombs en plus', '+2 plombs par salve', 'NBP'],
     ['Double canon', 'coût de jauge -15 %', 'MUN'], ['Souffle de culasse', 'le recul projette une onde derrière toi (3 m, 40)', 'ZON'],
     ['Éclats', 'le kill libère 3 éclats chercheurs', 'SUI']],
    [['Coup double', 'un kill lâche une 2e salve gratuite à 50 %', 'NBP'], ['Plomb lourd', 'dégâts des plombs +30 %', 'DEG'],
     ['Détonation', 'le kill explose 3,5 m, 45 dégâts', 'ZON'], ['Pompe tendue', 'portée +20 m, aucune perte de dégâts avant 20 m', 'POR'],
     ['Plombs traqueurs', 'les plombs se courbent de 8° vers la cible', 'SUI']],
  ],

  ELA: [
    [['Convertisseur', '+10 % de vitesse convertie en dégâts', 'DEG'], ['Plafond relevé', 'vitesse convertie max +1 m/s', 'VIT'],
     ['Tir large', 'le tir est 15 % plus large', 'TAI'], ['Tir tendu', 'portée du tir +15 %', 'POR'], ['Soute', 'munition maximale +1', 'MUN']],
    [['Visée mobile', 'aide à la visée +30 % en course', 'SUI'], ['Impact', 'retombée rapide : onde de 6 m, 50 dégâts', 'ZON'],
     ['Rétention', 'garde 10 % de la vitesse dépensée', 'VIT'], ['Écho léger', '10 % de chance de répéter le tir à 50 %', 'NBP'],
     ['Frappe', 'dash : 45 dégâts aux ennemis traversés', 'DEG']],
    [['Second souffle', 'SOIN : +5 PV par touche en dash-tir', 'SOI'], ['Fourche', 'le kill repart vers 2 ennemis (25 m)', 'SUI'],
     ['Vitesse recyclée', 'un kill rend 100 % de la vitesse dépensée', 'VIT'], ['Onde de retombée', 'l’onde de retombée gagne 2 m de rayon', 'ZON'],
     ['Munition de vitesse', 'un tir plein qui tue rend 1 munition', 'MUN']],
    [['Écho cinétique', 'un tir plein est répété 0,4 s plus tard (50 %)', 'NBP'], ['Tir plein', 'à 14,5 m/s ou plus : x1,5', 'DEG'],
     ['Vitesse de croisière', 'après un tir plein, la vitesse reste à 10 m/s ou plus 3 s', 'VIT'], ['Arc', 'éclair vers l’ennemi proche (40 %)', 'SUI'],
     ['Choc cinétique', 'un tir plein explose 3 m (60 dégâts)', 'ZON']],
  ],

  SCI: [
    [['Lame affûtée', '+10 % de dégâts', 'DEG'], ['Aérodynamique', 'vitesse de lancer +10 %', 'VIT'], ['Grande lame', 'la lame est 15 % plus grande', 'TAI'],
     ['Lancer long', 'portée avant retour +15 %', 'POR'], ['Retour guidé', 'le retour se courbe de 5° vers l’ennemi proche', 'SUI']],
    [['Renvoi renforcé', 'dégâts de renvoi x0,9 -> x1,1', 'DEG'], ['Lame plantée', 'la lame plantée blesse les ennemis à 2 m (15/s)', 'ZON'],
     ['Ricochet de lame', 'rebondit une fois sur le décor', 'POR'], ['Lame chercheuse', 'se courbe de 5° vers l’ennemi proche en vol', 'SUI'],
     ['Lame satellite', '1 lame orbite autour de la scie (30 % des dégâts)', 'NBP']],
    [['Lame vitale', 'SOIN : +8 PV par touche au retour', 'SOI'], ['Rappel tranchant', 'le rappel blesse tout sur son trajet (30)', 'DEG'],
     ['Couronne de lames', '+1 lame en orbite autour de la scie (40 % des dégâts)', 'NBP'], ['Rappel explosif', 'le rappel explose 3 m, 45 dégâts', 'ZON'],
     ['Fourche', 'le kill repart vers 2 ennemis (25 m)', 'SUI']],
    [['Halo de lames', '+2 lames en orbite autour de la scie (50 % des dégâts)', 'NBP'], ['Perforation', 'traverse la première cible', 'POR'],
     ['Détonation', 'le kill explose 3,5 m, 45 dégâts', 'ZON'], ['Chaîne de lame', 'chaque touche consécutive : +15 % (6 max)', 'DEG'],
     ['Lame traqueuse', 'suit l’ennemi le plus proche en vol (12°)', 'SUI']],
  ],

  VEN: [
    [['Charge dense', '+10 % de dégâts', 'DEG'], ['Poche', 'mines actives 2 -> 3', 'MUN'], ['Pose rapide', 'vitesse de lancer +20 %', 'VIT'],
     ['Mine large', 'souffle +0,4 m', 'ZON'], ['Détecteur', 'déclenchement piège 2 -> 2,6 m', 'POR']],
    [['Grosse mine', 'la mine est 30 % plus grande', 'TAI'], ['Grosse charge', 'rayon d’explosion de 4 à 5 m', 'ZON'],
     ['Mine recyclée', 'une mine qui expire te rend 50 points de jauge', 'MUN'], ['Mine glissante', 'la mine dérive de 1 m/s vers l’ennemi (3 m)', 'SUI'],
     ['Détonateur', '+20 % sur les mines collées à un ennemi', 'DEG']],
    [['Récupération', 'SOIN : +8 PV quand une de tes mines touche', 'SOI'], ['Pose jumelle', 'chaque pose lance 2 mines (la 2e à 60 %)', 'NBP'],
     ['Chaîne de mines', 'une mine sautée fait sauter les voisines (2 m)', 'ZON'], ['Poche profonde', 'mines actives 3 -> 4', 'MUN'],
     ['Minuterie', 'déclenchement groupé : x1,3', 'DEG']],
    [['Mine mère', 'se divise en 3 mini-mines', 'NBP'], ['Mine aimantée', 'dérive vers l’ennemi (6 m)', 'SUI'], ['Charge massive', 'rayon d’explosion de 5 à 6,5 m', 'ZON'],
     ['Mine lourde', '+40 % de dégâts', 'DEG'], ['Détecteur long', 'déclenchement piège 2,6 -> 3,5 m', 'POR']],
  ],

  TES: [
    [['Batterie', 'capacité +10', 'MUN'], ['Rendement', 'consommation -10 %', 'MUN'], ['Portée', '+3 m', 'POR'], ['Impulsion', '+10 % de dégâts', 'DEG'],
     ['Calibrage', 'aide à la visée +0,2 m', 'TAI']],
    [['Étincelle', 'la 1re impulsion de chaque faisceau fait x3', 'DEG'], ['Arc étendu', 'la chaîne atteint 3 cibles', 'NBP'],
     ['Arc accroché', 'le faisceau reste accroché 0,4 s à la cible perdue', 'SUI'], ['Mini-décharge', 'relâcher le faisceau fait un souffle de 2 m', 'ZON'],
     ['Faisceau long', 'portée max de 28 à 31 m', 'POR']],
    [['Courant vital', 'SOIN : +2 PV par seconde de contact', 'SOI'], ['Rétrocourant', 'un kill rend 8 de batterie', 'MUN'],
     ['Chaîne large', 'la chaîne saute à 8 m et touche 1 cible de plus', 'NBP'], ['Décharge', 'relâché : explosion 3 m selon la durée', 'ZON'],
     ['Conductivité', 'la cible touchée subit +15 % de toutes tes armes (2 s)', 'DEG']],
    [['Saturation', 'après 2 s de contact : +30 %', 'DEG'], ['Réseau', 'la batterie regagne 1 point par seconde', 'MUN'],
     ['Orage', 'éclair sur un ennemi proche toutes les 0,5 s', 'NBP'], ['Détonation', 'le kill explose 3,5 m, 45 dégâts', 'ZON'],
     ['Faisceau dirigé', 'le faisceau se courbe de 10° vers l’ennemi proche', 'SUI']],
  ],

  // ======================================================== MELEE (5 axes x 4 profondeurs) ============================
  EPE: [
    [['Vif', 'recharge -10 %', 'CD'], ['Poignet souple', 'animation du coup 10 % plus rapide', 'ANI'], ['Enchaînement', 'un 2e coup à 60 % dans la foulée', 'COM'],
     ['Réserve', '+10 points de jauge par coup', 'MUR'], ['Goutte d’acier', 'vol de vie : 2 % des dégâts', 'VOL']],
    [['Garde basse', 'recharge dans le vide -25 %', 'CD'], ['Fente vive', 'coup en dash 20 % plus rapide', 'ANI'], ['Double frappe', 'le 2e coup du combo fait 100 %', 'COM'],
     ['Siphon', '+15 points de jauge si le coup touche 2 ennemis', 'MUR'], ['Fil de vie', 'vol de vie : 4 % des dégâts', 'VOL']],
    [['Riposte vitale', 'SOIN : +12 PV par parry réussi', 'VOL'], ['Riposte vive', 'après un parry, aucune recharge pendant 1 s', 'CD'],
     ['Lame d’éclair', 'coups 25 % plus rapides 2 s après un parry', 'ANI'], ['Triple lame', 'combo de 3 coups', 'COM'],
     ['Prise de jauge', '+25 points de jauge par parry réussi', 'MUR']],
    [['Lame infatigable', 'aucune recharge pendant 3 s après un kill', 'CD'], ['Vitesse de duelliste', 'coups 40 % plus rapides si un seul ennemi est à 10 m', 'ANI'],
     ['Valse d’acier', 'combo de 4 coups, le dernier fait x1,5', 'COM'], ['Trésor de lame', 'un kill rend 100 points de jauge de plus', 'MUR'],
     ['Soif de duel', 'vol de vie : 10 % (20 % contre un seul ennemi)', 'VOL']],
  ],

  ENC: [
    [['Atterrissage', 'recharge de 0,8 à 0,65 s', 'CD'], ['Pique rapide', 'vitesse de pique +10 %', 'ANI'], ['Second coup', 'un 2e coup au sol à 60 % dans la foulée', 'COM'],
     ['Récolte', '+10 points de jauge par coup', 'MUR'], ['Poussière de vie', 'vol de vie : 2 % des dégâts', 'VOL']],
    [['Appui ferme', 'recharge dans le vide -25 %', 'CD'], ['Chute vive', 'coup au sol 20 % plus rapide', 'ANI'], ['Séisme', 'l’onde se répète à 60 %', 'COM'],
     ['Écrasement', '+15 points de jauge par ennemi touché par l’onde', 'MUR'], ['Cœur de pierre', 'vol de vie : 4 % des dégâts', 'VOL']],
    [['Carapace', 'SOIN : +10 PV si l’onde touche', 'VOL'], ['Onde sur onde', 'une onde qui touche réduit la recharge de 0,3 s', 'CD'],
     ['Poids plume', 'le pique prend de la vitesse 25 % plus vite', 'ANI'], ['Onde triple', 'l’onde se répète 2 fois (60 % puis 40 %)', 'COM'],
     ['Butin d’onde', '+25 points de jauge si l’onde touche 3 ennemis', 'MUR']],
    [['Marteau infatigable', 'aucune recharge pendant 3 s après un kill', 'CD'], ['Foudre de fer', 'vitesse de pique +50 % au-delà de 30 m/s', 'ANI'],
     ['Cascade de séismes', 'l’onde se répète 3 fois', 'COM'], ['Pillage', 'un kill rend 100 points de jauge de plus', 'MUR'],
     ['Titan', 'vol de vie : 10 % des dégâts', 'VOL']],
  ],

  LAN: [
    [['Recharge', 'joute réussie : -0,2 s', 'CD'], ['Vélocité', 'vitesse de joute +10 %', 'ANI'], ['Second estoc', 'un 2e estoc à 60 % dans la foulée', 'COM'],
     ['Moisson', '+10 points de jauge par estoc', 'MUR'], ['Sève de lance', 'vol de vie : 2 % des dégâts', 'VOL']],
    [['Estoc vif', 'recharge de l’estoc -15 %', 'CD'], ['Lance vive', 'animation de l’estoc 20 % plus rapide', 'ANI'], ['Estocs en chaîne', 'combo de 3 estocs', 'COM'],
     ['Butin de joute', '+25 points de jauge par joute réussie', 'MUR'], ['Sang de cavalier', 'vol de vie : 4 % des dégâts', 'VOL']],
    [['Armure de fer', 'SOIN : +10 PV par joute réussie', 'VOL'], ['Relève', 'un estoc dans le vide : recharge 0,5 -> 0,25 s', 'CD'],
     ['Charge éclair', 'la lance couchée est prête en 0,10 s au lieu de 0,15 s', 'ANI'], ['Joute en chaîne', 'une seconde joute sans recharge', 'COM'],
     ['Pillage d’élite', '+40 points de jauge si la joute touche une élite', 'MUR']],
    [['Ruée', 'aucune recharge pendant 2 s après une joute réussie', 'CD'], ['Lance-éclair', 'estoc et joute 35 % plus rapides', 'ANI'],
     ['Tournoi', 'chaque joute réussie d’affilée : +20 % (60 % max)', 'COM'], ['Trésor de guerre', 'un kill rend 100 points de jauge de plus', 'MUR'],
     ['Lance vorace', 'vol de vie : 10 % des dégâts', 'VOL']],
  ],

  YOY: [
    [['Recharge', 'recharge -10 %', 'CD'], ['Poignet de chaîne', 'animation du coup 10 % plus rapide', 'ANI'], ['Retour de chaîne', 'un 2e balayage à 40 %', 'COM'],
     ['Glanage', '+10 points de jauge par cible touchée', 'MUR'], ['Chaîne de sève', 'vol de vie : 2 % des dégâts', 'VOL']],
    [['Chaîne souple', 'recharge dans le vide -25 %', 'CD'], ['Balayage vif', 'l’arc se balaie 20 % plus vite', 'ANI'], ['Tourbillon', 'un second tour à 50 %', 'COM'],
     ['Prise de jauge', '+15 points de jauge si le balayage touche 2 cibles ou plus', 'MUR'], ['Sang de chaîne', 'vol de vie : 4 % des dégâts', 'VOL']],
    [['Chaîne de soin', 'SOIN : +8 PV par cible touchée (3 max)', 'VOL'], ['Retour de fleau', 'toucher 2 cibles ou plus réduit la recharge de 0,3 s', 'CD'],
     ['Fouet rapide', 'coup 25 % plus rapide si 3 cibles sont dans l’arc', 'ANI'], ['Double tour', 'un second tour à 80 %', 'COM'],
     ['Butin de balayage', 'toucher 3 cibles ou plus rend 50 points de jauge', 'MUR']],
    [['Fleau infatigable', 'aucune recharge pendant 3 s après un kill', 'CD'], ['Tempête de chaîne', 'coups 40 % plus rapides', 'ANI'],
     ['Tornade', '3 tours successifs (100 %, 70 %, 50 %)', 'COM'], ['Butin de chaîne', 'un kill rend 100 points de jauge de plus', 'MUR'],
     ['Chaîne vorace', 'vol de vie : 10 % des dégâts', 'VOL']],
  ],

  IMP: [
    [['Cadence', 'combo -10 % de recharge', 'CD'], ['Poings vifs', 'coups espacés de 0,20 s au lieu de 0,22 s', 'ANI'], ['Quatrième coup', 'combo de 4 coups', 'COM'],
     ['Cueillette', '+10 points de jauge par coup', 'MUR'], ['Souffle de poing', 'vol de vie : 2 % des dégâts', 'VOL']],
    [['Souffle court', 'recharge après un combo sans touche -50 %', 'CD'], ['Jambes vives', 'animation 20 % plus rapide en l’air', 'ANI'],
     ['Cinquième coup', 'combo de 5 coups', 'COM'], ['Prise aérienne', '+15 points de jauge par coup porté en l’air', 'MUR'],
     ['Sang vif', 'vol de vie : 4 % des dégâts', 'VOL']],
    [['Souffle acrobate', 'SOIN : +8 PV par uppercut touché', 'VOL'], ['Combo enchaîné', 'un combo complet réduit la recharge suivante de 0,2 s', 'CD'],
     ['Rafale', 'coups espacés de 0,16 s', 'ANI'], ['Sixième coup', 'combo de 6 coups', 'COM'], ['Butin de combo', '+40 points de jauge par combo complet', 'MUR']],
    [['Poings infatigables', 'aucune recharge pendant 3 s après un kill', 'CD'], ['Frénésie', 'chaque combo complet : coups 10 % plus vifs (5 max, 6 s)', 'ANI'],
     ['Huit coups', 'combo de 8 coups, un uppercut tous les 4', 'COM'], ['Pillage de poings', 'un kill rend 100 points de jauge de plus', 'MUR'],
     ['Acrobate vorace', 'vol de vie : 10 % des dégâts', 'VOL']],
  ],

  EGI: [
    [['Remise en garde', 'bouclier brisé : indisponible 2,5 s au lieu de 3', 'CD'], ['Garde vive', 'levée du bouclier 10 % plus rapide', 'ANI'],
     ['Coup jumeau', 'un 2e coup de bouclier à 60 % dans la foulée', 'COM'], ['Récolte', '+10 points de jauge par coup de bouclier', 'MUR'],
     ['Éclat de garde', 'vol de vie : 2 % des dégâts', 'VOL']],
    [['Coup vif', 'recharge du coup de bouclier -15 %', 'CD'], ['Poing de garde', 'coup de bouclier 20 % plus rapide', 'ANI'], ['Triple coup', 'combo de 3 coups de bouclier', 'COM'],
     ['Encaisseur', '+15 points de jauge par coup bloqué', 'MUR'], ['Sang de rempart', 'vol de vie : 4 % des dégâts', 'VOL']],
    [['Pansement', 'SOIN : +15 PV par parry', 'VOL'], ['Rebond de garde', 'un coup bloqué réduit la recharge de 0,2 s', 'CD'],
     ['Parry étendu', 'fenêtre parfaite de 0,15 à 0,2 s', 'ANI'], ['Riposte en chaîne', 'chaque coup bloqué prépare un coup de bouclier x1,5', 'COM'],
     ['Garde fertile', '+30 points de jauge par parry parfait', 'MUR']],
    [['Égide infatigable', 'aucune recharge pendant 3 s après un parry parfait', 'CD'], ['Réflexe d’acier', 'levée instantanée après un coup bloqué', 'ANI'],
     ['Muraille', 'combo de 4 coups de bouclier, le dernier repousse de 20', 'COM'], ['Butin de garde', 'un kill rend 100 points de jauge de plus', 'MUR'],
     ['Rempart vorace', 'vol de vie : 10 % des dégâts', 'VOL']],
  ],
};
