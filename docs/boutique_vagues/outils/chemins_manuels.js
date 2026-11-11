// Consignes de chemin donnees a la main, appliquees avant l'optimisation (migrer_v2.js) : l'optimiseur en tient compte pour les autres branches.
// Deux formes, par noms d'etapes :
//   'GRAINE>PARTENAIRE': ['étape 1', 'étape 2', 'étape 3', 'étape 4']       le chemin complet
//   'GRAINE>PARTENAIRE': { sans: ['étape', ...], avec: ['étape', ...] }     etapes interdites / etapes imposees (a leur profondeur)
module.exports = {
  // revue du proprietaire : « satellite, tir explosif et detonation ne sont pas pertinents » pour le Shotgun
  // (Detonation a quitte le pool ; Eclat d'impact est le meme genre d'effet que Tir explosif)
  'RAIL>SHO': { sans: ['Satellite', 'Tir explosif', 'Éclat d’impact'] },
  // revue du proprietaire : « detonation remplacee par une size et hitbox upgrade » (les 3 premieres etapes restent celles qu'il a validees)
  'RAIL>ENC': ['Calibre', 'Surtension', 'Calibre lourd', 'Rail colossal'],
  // Detonation a quitte le pool du Railgun : on ne change que la 4e etape de ces deux chemins
  'RAIL>ROQ': ['Réserve', 'Éclat d’impact', 'Doublé', 'Perce-mur'],
  'RAIL>VEN': ['Réserve', 'Éclat d’impact', 'Tir explosif', 'Rail colossal'],
};
