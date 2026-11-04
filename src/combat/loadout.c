/* VELOCITY - loadout a deux armes et munitions a jauge
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Loadout et munitions a jauge                                        */
/* ------------------------------------------------------------------ */
/* le choix vit hors de P (reset_game remet P a zero) et ne s'applique qu'au (re)spawn */
static int wantLoadout[2] = { WP_RAIL, WP_ROCKET }, wantMelee = MK_SWORD;
/* skins : apparence du joueur ; la couleur vient de l'equipe (ou du joueur en chacun pour soi) */
enum { SKN_ROBOT, SKN_POUSSIN, SKN_NINJA, SKN_CHAT, SKN_CHEVALIER, SKN_COUNT };
static const char *skinNames[SKN_COUNT] = { "ROBOT", "POUSSIN", "NINJA", "CHAT", "CHEVALIER" };
static int wantSkin = SKN_ROBOT;
/* easter egg : le skin POUSSIN se debloque apres 1000 points de degats infliges au soleil avec l'Elan */
#define SUN_UNLOCK_DMG 1000.0f
static float sunDmg;                                      /* degats cumules au soleil (velocity.ini : soleil_degats) */
static int skin_unlocked(int k) { return k != SKN_POUSSIN || sunDmg >= SUN_UNLOCK_DMG; }
static void sun_hit_check(V3 o, V3 end, int hit, float dmg);   /* defini dans rendu/eclairage.c (a besoin de sun_dir) */
static int loadoutOpen;                                   /* ecran LOADOUT ouvert */
static int skinOpen, skinPrevTeam = 0;                    /* ecran SKIN ouvert ; couleur montree dans l'apercu */
static int hostOpen, hostBots = -1;                       /* ecran HEBERGER ouvert ; bots pour completer (-1 : aucun, sinon niveau) */
static int hostFfa;                                       /* la prochaine partie hebergee est en chacun pour soi (duel du menu) */
static float ammoGainT, dryHintT, dryT;                   /* HUD : "+50" vert, panneau de melee qui clignote ; temps a sec */
static int ammoGainSlot, ammoGainPts;                     /* emplacement qui clignote (2 = les deux) */
static float apStat[AP_COUNT];                            /* journal : points par origine */
static int cellsTaken;

static int cur_slot(void) { return P.weapon == P.loadout[1]; }
static int other_wp(void) { return P.loadout[cur_slot() ^ 1]; }
static int wp_has_ammo(int w) { return P.ammo[w] > 0; }
static int is_melee_w(int w) { return w == W_MELEE || (w >= W_SLAM && w <= W_SHIELD); }
static int is_parryable_w(int w) { return is_melee_w(w) || w == W_HOOK; }
static int wp_full(int w) { return ammoDef[w].gauge <= 0 || P.ammo[w] >= ammoDef[w].max; }
static int loadout_full(void) { return wp_full(P.loadout[0]) && wp_full(P.loadout[1]); }
static float gauge_frac(int w) {                          /* HUD : 0..1, pleine au maximum */
    if (ammoDef[w].gauge <= 0) return 0;
    return P.ammo[w] >= ammoDef[w].max ? 1 : P.ammoGauge[w] / ammoDef[w].gauge;
}

/* ajoute des points a la jauge de w ; chaque jauge pleine donne 1 munition. Renvoie les points inutilises */
static float gauge_add(int w, float pts) {
    const AmmoDef *d = &ammoDef[w];
    if (d->gauge <= 0 || P.ammo[w] >= d->max) return pts;
    P.ammoGauge[w] += pts;
    while (P.ammoGauge[w] >= d->gauge) {
        P.ammoGauge[w] -= d->gauge;
        if (++P.ammo[w] >= d->max) { float rest = P.ammoGauge[w]; P.ammoGauge[w] = 0; return rest; }
    }
    return 0;
}
static void gauge_remove(int w, float pts) {              /* annulation : peut reprendre une munition */
    if (ammoDef[w].gauge <= 0 || pts <= 0) return;
    P.ammoGauge[w] -= pts;
    while (P.ammoGauge[w] < 0 && P.ammo[w] > 0) { P.ammo[w]--; P.ammoGauge[w] += ammoDef[w].gauge; }
    if (P.ammoGauge[w] < 0) P.ammoGauge[w] = 0;
}

/* points de melee : l'arme en main d'abord, le surplus a l'autre arme du loadout */
static float ammo_rule_mul(void);                         /* src/jeu/economie.c : regles (x2) et ARTIFICIER */
static int eco_loadout(int *l);
static int ecoAmmoBoost;
static void ammo_points(float pts, int src) {
    if (P.dead || pts <= 0) return;
    pts *= ammo_rule_mul();
    int s = cur_slot(), a0 = P.ammo[P.loadout[0]], a1 = P.ammo[P.loadout[1]];
    float rest = gauge_add(P.loadout[s], pts), lost = gauge_add(P.loadout[s ^ 1], rest);
    apStat[src] += pts - lost; apStat[AP_LOST] += lost;
    if (src == AP_HIT) {                                  /* memorise pour une annulation eventuelle */
        P.undoPts[s] = pts - rest; P.undoPts[s ^ 1] = rest - lost; P.undoT = nowT;
    }
    if (pts - lost <= 0) return;
    ammoGainT = 0.8f; ammoGainPts = (int)pts; ammoGainSlot = pts - rest > 0 ? s : s ^ 1;
    if (P.ammo[P.loadout[0]] > a0 || P.ammo[P.loadout[1]] > a1) play(S_PAD, 0.5f, 3.0f);   /* munition gagnee */
    else play(S_PAD, 0.35f, 2.4f);
}
static void melee_start_swing(void) { P.swingHitPts = P.swingParryPts = 0; }
static void melee_hit_points(int pts) { if (!P.swingHitPts) { P.swingHitPts = 1; ammo_points((float)pts, AP_HIT); } }
static void melee_parry_points(int pts) { if (!P.swingParryPts) { P.swingParryPts = 1; ammo_points((float)pts, AP_PARRY); } }

static int loadout_valid(const int *l, int mk) {
    return l[0] >= 0 && l[0] < WP_COUNT && l[1] >= 0 && l[1] < WP_COUNT && l[0] != l[1] &&
           wpReady[l[0]] && wpReady[l[1]] && mk >= 0 && mk < MK_COUNT && mkReady[mk];
}
static void tesla_reset(void);                            /* Tesla : definie avec l'arme */
static void loadout_apply(void) {                         /* (re)spawn : munitions de depart, jauges vides, arme 1 en main */
    if (!loadout_valid(wantLoadout, wantMelee)) { wantLoadout[0] = WP_RAIL; wantLoadout[1] = WP_ROCKET; wantMelee = MK_SWORD; }
    P.loadout[0] = wantLoadout[0]; P.loadout[1] = wantLoadout[1]; P.meleeKind = wantMelee;
    eco_loadout(P.loadout);                               /* boutique : les armes achetees (une seule au debut) */
    if (wantSkill < 0 || wantSkill >= SK_COUNT) wantSkill = SK_COIN;
    P.skill = wantSkill; P.ricArmed = 0; P.chargeT = P.chargeHold = 0; P.aimT = 0;
    for (int w = 0; w < WP_COUNT; w++) { P.ammo[w] = ecoAmmoBoost ? ammoDef[w].max : ammoDef[w].start; P.ammoGauge[w] = 0; }   /* MUNITIONS + */
    P.weapon = P.loadout[0]; P.switchT = 0; P.undoPts[0] = P.undoPts[1] = 0;
    P.kinT = 0; P.kinDashT = 1; P.kinPreDash = 0;          /* Elan : ni fenetre de dash-tir ni malus */
    P.sgPumpT = P.sgPumpMax = 0; P.sgLastT = 0;           /* Eventail : pompe au repos */
    P.sawIdx = 0; P.sawCd = 0;                            /* Scie : neuve, en main */
    tesla_reset();                                        /* Tesla : ni arc, ni cumuls, ni etourdissement */
    P.lanceState = LS_IDLE; P.lanceT = P.lanceVc = 0;     /* Lance : levee, prete */
    P.lanceHitT = 10; P.lanceHitWho = 0; P.meleeCd = 0;
    P.flailT = P.flailParriedT = P.flailLastT = 0;      /* Fleau : chaine au repos */
    P.flailHitN = P.flailLastTgt = 0;
    P.fistStep = P.fistAnim = P.fistPts = P.fistUpTgt = 0;   /* Poings : combo au repos, punch-jump disponible */
    P.fistT = 10; P.fistBuf = P.fistGuard = P.fistUpT = 0; P.fistPJ = 1;
    P.shieldState = SHS_DOWN; P.shieldGauge = SHIELD_GAUGE_MAX;   /* Egide : bouclier baisse, jauge pleine, fenetre parfaite armee */
    P.shieldBrokenT = P.shieldRipT = P.shieldHeldT = P.shieldAnim = P.shieldHitT = 0; P.shieldDownT = 1; P.shieldRamDone = 0;
    melee_start_swing();
}
static double wpFlashAt = -10;                            /* dernier changement d'arme (temps reel) : nom affiche au centre */
static void weapon_select(int w) { if (w != P.weapon) { P.weapon = w; P.switchT = SWITCH_TIME; wpFlashAt = nowT; } }

/* Coup de corps a corps (clic droit / V). Degats et portee montent avec la vitesse RELATIVE
 * entre le joueur et la cible : foncer sur quelqu'un qui fonce sur toi frappe tres fort.
 * Il pare aussi : orbes ennemies et roquettes adverses sont renvoyees, les kamikazes explosent. */
static float melee_range(float rel) { return MELEE_RANGE + rel * MELEE_RANGE_PER_MS; }
static float melee_dmg(float rel) { return MELEE_DMG + rel * MELEE_DMG_PER_MS; }

/* Parry : pendant PARRY_WINDOW apres le debut du coup, les projectiles proches sont renvoyes.
 * Chaque renvoi d'une meme roquette (ping-pong) l'accelere, la rend plus forte, et reduit la zone de parry.
 * range0 : portee de base (epee MELEE_RANGE + 1,5 ; Fleau 6) ; speedMult : vitesse de renvoi (Fleau 0,8) */
static int try_parry(float range0, float speedMult) {
    V3 eye = eye_pos(), f = view_dir();
    int parried = 0;
    for (int i = 0; i < MAXPR; i++) {
        Proj *p = &PR[i];
        if (p->alive && p->kind == PR_SAW) {              /* Scie : renvoi de la mienne qui revient, ou parry d'une adverse */
            parried += saw_parry(p, eye, f, range0, speedMult);
            continue;
        }
        if (p->alive && p->kind == PR_MINE) {             /* --- Ventouse : collee sur moi = arrachee ; en vol = roquette --- */
            if (mine_tear(p, f)) { parried++; continue; }
            if (p->mineState != MS_FLY) continue;
        }                                                 /* --- fin Ventouse --- */
        int foreignRocket = dmMode && (p->kind == PR_ROCKET || p->kind == PR_PARRIED || p->kind == PR_MINE) && p->owner != myId;
        if (!p->alive || (p->kind != PR_ORB && !foreignRocket)) continue;
        V3 d = vsub(p->pos, eye);
        float dist = vlen(d), range = range0 * powf(PARRY_RANGE_SHRINK, (float)p->parries);
        if (dist > range || vdot(vmul(d, 1.0f / fmaxf(dist, 0.01f)), f) < 0.3f) continue;
        V3 aim = f; float best = PARRY_AIM_COS;
        {   /* sans cible : viser le point que le viseur touche (et non la direction de la vue, decalee par la parallaxe) */
            float hd = ray_world(eye, f, 150.0f, 0);
            if (hd > 3.0f) aim = vnorm(vsub(vadd(eye, vmul(f, fminf(hd, 150.0f))), p->pos));
        }
        for (int j = 0; j < MAXE; j++) {
            if (!E[j].alive) continue;
            V3 to = vnorm(vsub(E[j].pos, p->pos));
            float dd = vdot(to, f);
            if (dd > best) { best = dd; aim = to; }
        }
        for (int j = 0; j < MAXP; j++) {
            if (!np_target(j)) continue;
            V3 to = vnorm(vsub(np_center(j), p->pos));
            float dd = vdot(to, f);
            if (dd > best) { best = dd; aim = to; }
        }
        p->parries++;
        if (!p->orb) p->mult *= p->parries == 1 ? PARRY_ROCKET_MULT : PARRY_PINGPONG;
        p->kind = PR_PARRIED; p->life = 4; p->owner = myId;
        p->vel = vmul(aim, PARRY_SPEED * powf(PARRY_PINGPONG, (float)(p->parries - 1)) * speedMult);
        if (dmMode) net_fx_parry(p->netId, p->pos, p->vel, p->mult, p->parries);
        parried++;
        P.swingLanded = 1;
        parry_feedback(p->pos);
        melee_parry_points(P.meleeKind == MK_SHIELD ? PTS_SHIELD_PERFECT : PTS_PARRY);   /* une fois par coup ; Egide : parfait */
        if (p->parries > 1) { snprintf(popText, sizeof popText, "PARRY x%d", p->parries); popT = 1.0f; }
    }
    return parried;
}
