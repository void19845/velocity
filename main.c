/*
 * VELOCITY - movement shooter 3D en C
 * Win32 + OpenGL 1.1 + waveOut, aucune dependance externe.
 * macOS : meme code, couche plateforme dans src/plateforme/mac/ (Cocoa, AudioQueue, NSURLSession).
 *
 * Mouvement : bhop (maintenir saut), air-strafe facon Quake, slide, dash,
 *             double saut, wall-run, wall-jump, jump pads, rocket-jump.
 *
 * Ce fichier est la table des matieres du jeu : il inclut les morceaux de src/ DANS CET ORDRE et le tout
 * forme une seule unite de compilation (build.bat et build_mac.sh ne compilent que main.c).
 * Les morceaux ne se compilent pas seuls : une fonction doit etre definie (ou declaree) dans un morceau
 * place plus haut que celui qui l'utilise. Carte detaillee : docs/CODE.md, section 2.
 */

#include "src/noyau/entete.c"                    /* includes systeme (Win32 ou couche macOS), bibliotheques, PI et TICK */
#include "src/noyau/maths.c"                     /* vecteurs 3D, bornes, aleatoire */

#include "src/reglages/equilibrage.c"            /* TOUS les #define d'equilibrage : mouvement, armes, parry, PvP, points */

#include "src/son/audio.c"                       /* mixeur waveOut / AudioQueue, sons synthetises, musique */

#include "src/monde/carte.c"                     /* carte en boites (AABB), recharges, collisions, rayons */
#include "src/monde/entites.c"                   /* structures du jeu : joueur P, ennemis, pieces, projectiles, particules */

#include "src/reseau/etat_partage.c"             /* ce que le jeu doit savoir du reseau avant le reseau : joueurs NP, equipes, declarations */

#include "src/entrees/actions.c"                 /* touches physiques, actions A_*, touches par defaut */

#include "src/effets/combat_fx.c"                /* impact frames, chiffres de degats, eclats ; lecture des entrees (input_poll) */
#include "src/effets/particules.c"               /* particules, rayons, lumieres et marques d'impact */

#include "src/joueur/mouvement.c"                /* physique du joueur : bhop, slide, dash, wall-run, grappin */
#include "src/effets/camera_mort.c"              /* camera de mort : la vue se tourne vers le tueur */

#include "src/jeu/style.c"                      /* jauge de style D a S : mouvement et beaux kills */

#include "src/combat/degats.c"                   /* degats, kills, explosions, serie de tirs, parry */
#include "src/combat/loadout.c"                  /* loadout a deux armes et munitions a jauge */

#include "src/armes/enclume.c"                   /* Enclume : coup au sol, pique, onde de choc */
#include "src/armes/lance.c"                     /* Lance de joute */
#include "src/armes/fleau.c"                     /* Fleau a chaine */
#include "src/armes/poings.c"                    /* Poings a impulsion */

#include "src/combat/melee.c"                    /* epee et aiguillage des armes de melee */

#include "src/armes/egide.c"                     /* Egide (bouclier) */

#include "src/combat/pieces_rail.c"              /* pieces, ricochets, tir instantane, railgun */

#include "src/armes/elan.c"                      /* Elan (convertisseur cinetique) */
#include "src/armes/eventail.c"                  /* Eventail (fusil a pompe) */
#include "src/armes/tesla.c"                     /* Tesla (arc electrique) */

#include "src/combat/roquettes.c"                /* projectiles generiques, lance-roquettes */

#include "src/armes/scie.c"                      /* Scie (disque-boomerang) */
#include "src/armes/ventouse.c"                  /* Ventouse (mines collantes) */

#include "src/combat/projectiles.c"              /* deplacement et impact des projectiles */

#include "src/solo/ennemis_vagues.c"             /* mode solo : ennemis (drone, kamikaze, sentinelle, essaim, bastion) */
#include "src/solo/boss.c"                       /* mode VAGUES : pyramide, tesseract, sphere, hexaedre */
#include "src/solo/ameliorations.c"              /* mode VAGUES : boutique de modificateurs (comportements, pas de stats) */

#include "src/jeu/economie.c"                   /* boutique : pieces, arme de depart, achats a la mort */
#include "src/reseau/classe.c"                   /* mode classe : etat, cote Elo, paliers, identite de classement */
#include "src/reseau/horloge.c"                  /* horloge de match commune : heure du serveur, calage des joueurs */
#include "src/reseau/protocole.c"                /* paquets, evenements EV_*, apply_event */
#include "src/reseau/hote.c"                     /* hote : arbitrage (PV, kills, respawns, parry) */
#include "src/reseau/modes.c"                    /* modes : chrono, 3 zones, drapeau, bouclier ; SOUTIEN */
#include "src/reseau/classe_match.c"             /* mode classe : suivi du match, rapport de fin, PK_JOIN */
#include "src/reseau/client.c"                   /* client : connexion, instantanes, envoi */
#include "src/reseau/en_ligne.c"                 /* parties en ligne : STUN, lobby WebSocket */

#include "src/bots/navigation.c"                 /* bots : graphe de la carte (sols, pads, ancres), plus court chemin */
#include "src/bots/bot.c"                        /* bots : visee, decisions */

#include "src/combat/competences.c"              /* competences : MARQUE, CHARGE, RICOCHET, ALL IN ONE, MIRAGE, AIMANT (la piece en est une) */
#include "src/jeu/course.c"                     /* mode COURSE : portes, chrono, fantome du record */
#include "src/jeu/speedrun.c"                   /* speedrun : categories de classement, planchers, speedrun complet */
#include "src/jeu/kill_zone.c"                  /* kill zone : chute dans le vide (killY de la carte), bots prudents au bord */
#include "src/solo/vagues.c"                     /* mode VAGUES : 100 vagues, selecteur de carte, challenges, INFINI */

#include "src/jeu/boucle.c"                      /* ramassage, game_tick (un pas de simulation), fx_tick */

#include "src/rendu/base.c"                      /* polices et matrices de camera */

#include "src/graphismes/rendu_ini.c"            /* lecture de graphismes/rendu.ini, ambiances, qualite */

#include "src/rendu/textures.c"                  /* textures (et .bmp perso), primitives de boites */

#include "src/perso/skins.c"                     /* skins des joueurs : modeles et animation */

#include "src/rendu/eclairage.c"                 /* lightmaps, occlusion, neons, liste du decor */

#include "src/interface/texte.c"                 /* police, texte et rectangles 2D */

#include "src/rendu/modeles.c"                   /* billboards, camera, ennemis, marteau */
#include "src/rendu/armes_rendu.c"               /* rendu specifique des armes (scie, tesla, lance, fleau...) */
#include "src/rendu/viewmodel.c"                 /* arme en main (viewmodel) */
#include "src/rendu/vagues_rendu.c"              /* mode VAGUES : ennemis, boss et effets */

#include "src/interface/mise_a_jour.c"           /* mise a jour depuis GitHub : verifier, telecharger, installer */
#include "src/plateforme/http.c"                 /* requete HTTP(S) simple : WinHTTP / curl (mode classe) */
#include "src/interface/config.c"                /* velocity.ini : reglages et touches du joueur */
#include "src/reseau/classe_serveur.c"           /* mode classe : requetes au serveur de classement (hello, top, rapport) */
#include "src/reseau/speedrun_serveur.c"         /* speedrun : ticket, temps et classements au serveur */
#include "src/reseau/discord.c"                  /* connexion Discord : un compte Discord = une identite de classement */
#include "src/interface/menus.c"                 /* boutons, menus, parametres, loadout, ecran SKIN */
#include "src/interface/scores.c"                /* tableau des scores, killfeed, pseudos */

#include "src/rendu/joueurs.c"                   /* joueurs distants, scies et mines en monde, trainees, contours */

#include "src/interface/hud.c"
#include "src/interface/modes_hud.c"               /* modes : objectifs (monde et HUD), boutique, arme de depart */
#include "src/interface/course_hud.c"              /* mode COURSE : chrono, portes en 3D, fantome, record */
#include "src/interface/vagues_hud.c"              /* mode VAGUES : HUD, barre du boss, boutique, selecteur */                   /* HUD : viseur, vie, munitions, radar */

#include "src/rendu/ciel_ombres_bloom.c"         /* ciel, ombres dynamiques, bloom */

#include "src/effets/monde_fx.c"                 /* effets du monde : neons, lumieres, marques, fumee, meteo */

#include "src/interface/carte_ecran.c"           /* second ecran : carte vue de dessus et statistiques de la partie */
#include "src/rendu/render.c"                    /* rechargement F5 et render() : l'image complete */

#include "src/plateforme/fenetre.c"              /* fenetre, entrees clavier/souris (Windows et macOS) */
#include "src/plateforme/second_ecran.c"         /* second ecran : fenetre sur l'autre moniteur (Windows et macOS) */

#include "src/tests/vagues_tests.c"              /* tests du mode VAGUES (appeles par run_tests) */
#include "src/tests/tests.c"                     /* captures --shot et tests automatiques --test */

#include "src/plateforme/demarrage.c"            /* point d'entree (WinMain / velocity_main) et boucle principale */
