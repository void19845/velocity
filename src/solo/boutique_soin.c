/* boutique VAGUES : aide commune de soin. Tous les soins de la boutique (etapes de soin, vol de vie) passent par
 * vg_heal() : seau a jetons de VG_HEAL_RATE PV par seconde (capacite VG_HEAL_CAP), rien pendant une reaction en chaine
 * (degats d'un effet, vgDepth > 0), rien hors VAGUES, rien sur un joueur mort, jamais au-dela de 100 PV.
 * Le soin sur kill n'existe plus en VAGUES (voir kill_enemy et melee_hit). */
#define VG_HEAL_RATE 20.0f                                /* PV de soin par seconde, tous effets confondus */
#define VG_HEAL_CAP  20.0f                                /* rafale maximale d'un coup (seau plein) */
static float vgHealTok = VG_HEAL_CAP, vgHealLast;

static float vg_heal(float pv) {                          /* rend les PV effectivement soignes */
    if (!vagOn || vgDepth > 0 || P.dead || pv <= 0) return 0;
    float dt = gameTime - vgHealLast;                     /* gameTime : temps de jeu (jamais en pause) */
    if (dt < 0) vgHealTok = VG_HEAL_CAP;                  /* nouvelle partie : horloge repartie de zero */
    else vgHealTok = fminf(VG_HEAL_CAP, vgHealTok + dt * VG_HEAL_RATE);
    vgHealLast = gameTime;
    float room = 100 - P.hp;
    float g = fminf(pv, fminf(vgHealTok, room > 0 ? room : 0));
    if (g <= 0) return 0;
    vgHealTok -= g; P.hp += g;
    return g;
}
