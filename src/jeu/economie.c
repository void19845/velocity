/* VELOCITY - economie et boutique : pieces gagnees en partie, arme de depart parmi 4, achats a la mort
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ======================================================================= */
/* Economie et boutique (chaque joueur pour lui-meme)                       */
/* ======================================================================= */
static int credits, ecoOwned, ecoSlot[2] = { -1, -1 }, ecoNeedStarter, ecoBought;   /* ecoAmmoBoost : combat/loadout.c */
static float ecoStarterT;
static int ecoShopping;                                   /* boutique ou choix de l'arme a l'ecran (souris liberee) : pas de menu pause */
static const int ecoStarters[4] = { WP_RAIL, WP_SHOTGUN, WP_TESLA, WP_ROCKET };
static int eco_price(int w) {
    switch (w) {
    case WP_RAIL: return 500; case WP_ROCKET: return 600; case WP_SHOTGUN: return 400; case WP_KINETIC: return 650;
    case WP_SAW: return 700; case WP_MINE: return 450; case WP_TESLA: return 550; default: return 9999;
    }
}
static int eco_on(void) { return dmMode && rules.shop; }
static void eco_reset(void) {
    credits = ECO_START; ecoOwned = 0; ecoSlot[0] = ecoSlot[1] = -1; ecoAmmoBoost = 0; ecoBought = 0;
    ecoNeedStarter = eco_on(); ecoStarterT = ECO_STARTER_T;
}
static void eco_gain(int n, const char *why) {
    if (!eco_on() || n <= 0) return;
    credits += n;
    if (why) { snprintf(popText, sizeof popText, "+%d %s", n, why); popT = 1.0f; }
}
static void eco_pick_starter(int w) {                     /* arme de depart (gratuite) */
    ecoOwned |= 1 << w; ecoSlot[0] = w; ecoSlot[1] = -1; ecoNeedStarter = 0;
    netlog("boutique : arme de depart %s", ammoDef[w].name);
}
static int eco_buy(int w) {                               /* achete (ou equipe si deja a soi) ; la 2e place prend la nouvelle */
    if (w < 0 || w >= WP_COUNT || !wpReady[w]) return 0;
    if (!(ecoOwned & (1 << w))) {
        if (credits < eco_price(w)) return 0;
        credits -= eco_price(w); ecoOwned |= 1 << w; ecoBought++;
        netlog("boutique : achat %s (%d), reste %d", ammoDef[w].name, eco_price(w), credits);
        play(S_PAD, 0.6f, 1.6f);
    }
    if (ecoSlot[0] == w || ecoSlot[1] == w) return 1;
    if (ecoSlot[0] < 0) ecoSlot[0] = w; else ecoSlot[1] = w;
    return 1;
}
static int eco_buy_ammo(void) {
    if (ecoAmmoBoost || credits < ECO_AMMO_PRICE) return 0;
    credits -= ECO_AMMO_PRICE; ecoAmmoBoost = 1; play(S_PAD, 0.6f, 1.6f);
    return 1;
}
/* boutique : quelles armes au (re)spawn ; renvoie 0 hors mode boutique */
static int eco_loadout(int *l) {
    if (!eco_on()) return 0;
    if (ecoSlot[0] < 0) { l[0] = l[1] = WP_RAIL; return 1; }   /* pas encore choisie : le RAIL en attendant (le choix reste ouvert) */
    l[0] = ecoSlot[0]; l[1] = ecoSlot[1] >= 0 ? ecoSlot[1] : ecoSlot[0];
    return 1;
}
static float ammo_rule_mul(void) {                        /* jauges : regles de la partie */
    return dmMode && rules.ammo == AMMO_DOUBLE ? 2.0f : 1.0f;
}
/* bot : achete au hasard ce qu'il peut, a la mort */
static void eco_bot_shop(void) {
    if (!eco_on()) return;
    if (ecoNeedStarter) eco_pick_starter(ecoStarters[(int)(frand() * 4) & 3]);
    for (int tries = 0; tries < 6; tries++) {
        int w = (int)(frand() * WP_COUNT) % WP_COUNT;
        if ((ecoOwned & (1 << w)) || !wpReady[w] || credits < eco_price(w)) continue;
        eco_buy(w);
        break;
    }
}
