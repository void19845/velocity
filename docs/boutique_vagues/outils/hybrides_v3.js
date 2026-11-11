// Formes fusionnees : la fusion finale de deux armes a distance debloque temporairement une arme hybride qui remplace les deux.
// Idee du proprietaire : "un shotgun avec des projectiles de railgun", "un shotgun qui projette plein de pieces".
// Une arme = un lot de stats (taille, vitesse, zone, portee, munitions, nombre de projectiles, degats, suivi) : l'hybride recombine
// celles des deux parents. Format : 'A-B': { n: nom de l'arme hybride, b: ce qu'elle prend a chaque parent, e: effet }.
// Cadre commun (a valider par le proprietaire) : voir CADRE ci-dessous. La synergie passive de la fusion reste active en permanence.

const CADRE = 'Forme fusionnée (proposition à valider) : acheter la fusion donne une jauge de fusion, remplie par les dégâts des deux armes. ' +
  'Pleine, une touche active l’arme hybride pendant 15 s : elle remplace les deux armes à distance (leurs munitions sont mises de côté puis rendues), ' +
  'hérite des modificateurs des deux arbres stat par stat (dégâts, projectiles, zone, portée…), puis les deux armes reviennent. ' +
  'La mêlée et la compétence restent disponibles.';

const hybrides = {
  'RAIL-ROQ': { n: 'ARTILLERIE ORBITALE', b: 'tir du Railgun × explosion des Roquettes',
    e: 'un trait instantané qui traverse tout, explose (8 m) au bout de sa course et à chaque cible traversée (3 m) ; dégâts du rail, souffle de la roquette' },
  'RAIL-SHO': { n: 'CANON SCIÉ', b: 'motif du Shotgun × projectile du Railgun',
    e: '8 rails fins en gerbe (cône de 6°), chacun traverse les ennemis à 50 % des dégâts du rail ; portée 120 m ; le recul du Shotgun te propulse' },
  'RAIL-ELA': { n: 'LUMIÈRE VIVE', b: 'dégâts de l’Élan × tir du Railgun',
    e: 'un fusil unique à tir instantané et perforant : +8 dégâts par m/s de ta vitesse, qui n’est jamais dépensée ; 4 munitions, un tir toutes les 0,5 s' },
  'RAIL-SCI': { n: 'SCIE ÉCLAIR', b: 'lancer de la Scie × vitesse du Railgun',
    e: 'une scie qui part à la vitesse d’un rail : elle découpe tout sur sa ligne puis revient par la même ligne (second passage à 50 %), sans jamais se planter' },
  'RAIL-VEN': { n: 'FUSIL MINEUR', b: 'tir du Railgun × mines de la Ventouse',
    e: 'un seul tir instantané qui traverse tout et laisse une mine (x1,5) sur chaque ennemi touché, 3 au plus ; les mines explosent ensemble au second appui' },
  'RAIL-TES': { n: 'FAISCEAU ORBITAL', b: 'faisceau du Tesla × perforation du Railgun',
    e: 'un faisceau continu de 28 m qui traverse les ennemis, +10 % de dégâts par seconde de contact ; consomme la batterie et la jauge du rail' },
  'ROQ-SHO': { n: 'OBUSIER', b: 'motif du Shotgun × projectile des Roquettes',
    e: '6 mini-roquettes en gerbe (cône de 8°), souffle de 2,5 m chacune ; le recul t’envoie en l’air' },
  'ROQ-ELA': { n: 'CANON CINÉTIQUE', b: 'vitesse de l’Élan × roquette',
    e: 'une roquette dont la vitesse et le souffle montent avec les tiens (+0,2 m de rayon par m/s) et qui te rend la moitié de la vitesse dépensée' },
  'ROQ-SCI': { n: 'SCIES-FUSÉES', b: 'retour de la Scie × explosion des Roquettes',
    e: 'une scie propulsée par roquette, escortée de 3 lames en orbite : chaque touche explose (2 m), puis elle revient à toi' },
  'ROQ-VEN': { n: 'MORTIER À MÈCHE', b: 'projectile des Roquettes × collage de la Ventouse',
    e: 'la roquette colle où elle touche et attend : jusqu’à 3 roquettes collées explosent ensemble au second appui (rayon x1,5)' },
  'ROQ-TES': { ok: true, n: 'ROQUETTE-ORAGE', b: 'roquette × arcs du Tesla',     // validee par le proprietaire
    e: 'en vol, la roquette tire un arc sur 3 cibles toutes les 0,3 s ; à l’impact le souffle enchaîne un éclair sur chaque ennemi touché' },
  'SHO-ELA': { n: 'SOLEIL', b: 'motif du Shotgun × conversion de l’Élan',
    e: 'la salve convertit ta vitesse (+10 % de dégâts par m/s) et le recul ne t’en retire rien : tu gardes toute ta vitesse' },
  'SHO-SCI': { n: 'SCIE À POMPE', b: 'motif du Shotgun × lames de la Scie',
    e: 'la salve lance 5 mini-lames en éventail ; chacune revient vers toi en touchant encore (60 %)' },
  'SHO-VEN': { n: 'GERBE DE MINES', b: 'motif du Shotgun × mines de la Ventouse',
    e: 'la salve colle 6 mini-mines (30 %) en éventail ; elles explosent ensemble au second appui ou à 2 m d’un ennemi' },
  'SHO-TES': { n: 'FUSIL À ARCS', b: 'motif du Shotgun × arcs du Tesla',
    e: 'un cône de 8 arcs, chacun chaîne vers 2 cibles (40 %) ; la salve coûte 20 de batterie au lieu de la jauge' },
  'ELA-SCI': { n: 'SCIE CINÉTIQUE', b: 'vitesse de l’Élan × lame de la Scie',
    e: 'une scie lancée avec ta vitesse convertie (+6 dégâts par m/s) ; son retour te rend toute la vitesse dépensée' },
  'ELA-VEN': { n: 'MINE-BÉLIER', b: 'vitesse de l’Élan × mine de la Ventouse',
    e: 'une mine lancée à ta vitesse (x2) qui te propulse de 12 à son explosion et fait +5 dégâts par m/s' },
  'ELA-TES': { n: 'ARC CINÉTIQUE', b: 'vitesse de l’Élan × faisceau du Tesla',
    e: 'un faisceau dont la portée et les dégâts montent avec ta vitesse (+1 m et +5 % par m/s) ; courir recharge la batterie de 2 par seconde' },
  'SCI-VEN': { n: 'SCIE-MINE', b: 'lame de la Scie × mines de la Ventouse',
    e: 'la lame plantée devient une mine : elle explose (4 m) au rappel ; un seul lancer pose et rappelle' },
  'SCI-TES': { n: 'SCIE CONDUCTRICE', b: 'lame de la Scie × arcs du Tesla',
    e: 'en vol la lame émet un arc continu vers l’ennemi à 8 m ; plantée, elle devient un relais de 10 m' },
  'VEN-TES': { n: 'CHAMP ÉLECTRIQUE', b: 'mines de la Ventouse × arcs du Tesla',
    e: 'tes mines forment un réseau : arcs de 8 m entre elles (40 %), l’arc qui traverse une mine la fait exploser' },
};

module.exports = { CADRE, hybrides };
