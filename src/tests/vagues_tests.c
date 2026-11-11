/* VELOCITY - tests automatiques du mode VAGUES (appeles par run_tests, src/tests/tests.c)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* partie VAGUES neuve sur la carte m, sans entrees, sans hitstop */
static void vg_test_begin(int m) {
    dmMode = 0; netMode = NET_OFF; raceOn = 0;
    vagOn = 1; vgStartMap = m;
    if (curMap != m) load_map(m);
    reset_game();
    hitstop = 0; vgMenu = VGM_NONE; vgHealTok = VG_HEAL_CAP;      /* seau de soin plein au depart de chaque test */
    memset(act, 0, sizeof act); memset(actP, 0, sizeof actP);
    memset(botHeld, 0, sizeof botHeld); memset(botPress, 0, sizeof botPress);
}
static int vg_test_enemy(int t, V3 pos, float hp) {      /* ennemi pose a la main, immobile */
    int i = spawn_enemy_at(t, pos, 0);
    if (i >= 0) { E[i].hp = E[i].maxHp = hp; E[i].vel = v3(0, 0, 0); E[i].fireT = 100; E[i].elite = 0; }
    return i;
}
/* point a d m du joueur (a hauteur h) avec la ligne de vue degagee, pour les tests d'ennemis qui doivent traverser la salle */
static V3 vg_test_clear_spot(float d, float h) {
    V3 pc = pcenter(), best = vadd(pc, v3(0, h, d));
    for (int k = 0; k < 72; k++) {
        float a = k * 2 * PI / 72;
        V3 p = vadd(pc, v3(cosf(a) * d, h, sinf(a) * d));
        if (los(p, pc) && sphere_free(p, 1.0f)) return p;
    }
    return best;
}
static void vg_test_kill_all(void) {
    for (int i = 0; i < MAXE; i++) if (E[i].alive) { E[i].hp = 0; kill_enemy(i, 0); }
}

/* boutique RAILGUN : decor des tests (rail equipe, etapes en bits de shOwn[SH_RAIL], ennemis immobiles devant l'oeil) */
static void rl_t_setup(unsigned int bits) {
    vg_test_begin(0);
    P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_COIN; P.weapon = WP_RAIL;
    sh_reset();
    memset(E, 0, sizeof E); memset(CO, 0, sizeof CO);
    shOwn[SH_RAIL] = bits;
    P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0; P.vel = v3(0, 0, 0);
    P.ammo[WP_RAIL] = 2; shotStreak = 0; P.fireCd = 0; P.hp = 100; P.dead = 0; P.lanceState = LS_IDLE;
}
static V3 rl_t_at(float fwd, float side, float up) {      /* point relatif a l'oeil : devant, a droite, en haut */
    V3 d = view_dir(), r = v3(cosf(P.yaw), 0, sinf(P.yaw));
    return vadd(eye_pos(), vadd(vmul(d, fwd), vadd(vmul(r, side), v3(0, up, 0))));
}
static float rl_t_fire(int e) {                           /* un tir de rail : PV perdus par l'ennemi e */
    float h0 = E[e].hp;
    P.ammo[WP_RAIL] = 2; shotStreak = 0; P.fireCd = 0; vgSrc = WP_RAIL;
    fire_rail();
    vgSrc = -1;
    return h0 - E[e].hp;
}
static void rl_t_cap(int partner) { shCap[sh_branch(SH_RAIL, partner)] = 1; }
static int rl_t_fusion(int partner) {
    for (int k = 0; k < SH_NFU; k++) if (shFu[k].a == SH_RAIL && shFu[k].b == partner) return k;
    return -1;
}

/* formes fusionnees (boutique_hybrides.c) : decor avec les deux armes de la paire equipees et la fusion achetee */
static void hy_t_setup(int sa, int sb, int wHand) {
    rl_t_setup(0);
    P.loadout[0] = shSeedUp[sa]; P.loadout[1] = shSeedUp[sb]; P.weapon = wHand;
    shFusion = -1;
    for (int k = 0; k < SH_NFU; k++) if (shFu[k].a == sa && shFu[k].b == sb) shFusion = k;
    memset(PR, 0, sizeof PR);
}
static int hy_t_go(void) {                                /* jauge pleine + touche : 1 si la forme est active */
    hyGauge = HY_GAUGE_MAX; actP[A_FUSION] = 1; hy_tick(TICK); actP[A_FUSION] = 0;
    return hyOn;
}
static int hy_t_step(int seed, int kind, int a) {         /* index de la premiere etape de ce genre dans le pool de la graine */
    for (int k = 0; k < 20; k++) if (shSteps[seed][k].kind == kind && (a < 0 || shSteps[seed][k].a == a)) return k;
    return -1;
}
/* capstones PIE>SHO : decor (rail, Shotgun et PIECE equipes, un drone de 100000 PV a 8 m) et piece posee a la main (immobile) */
static int cap_t_coin_setup(int cap, unsigned int shoBits) {
    rl_t_setup(0); P.loadout[0] = WP_RAIL; P.loadout[1] = WP_SHOTGUN; P.weapon = WP_RAIL; P.skill = SK_COIN; P.skillCh = 2;
    shCap[sh_branch(SH_PIE, SH_SHO)] = (unsigned char)cap;
    shOwn[SH_SHO] = shoBits;
    memset(E, 0, sizeof E); memset(CO, 0, sizeof CO);
    return vg_test_enemy(EN_DRONE, rl_t_at(8, 0, 0), 100000);
}
static void cap_t_coin_at(V3 p, float life) {
    memset(CO, 0, sizeof CO);
    throw_coin_one(view_dir(), 0);
    CO[0].pos = p; CO[0].vel = v3(0, 0, 0); CO[0].life = life;
}

static void fist_test_punch(Enemy *e);                    /* src/tests/tests.c */
/* zones reservees aux agents qui migrent une famille vers le sac de stats : leurs NOUVELLES lignes de test (une fonction par fichier, appelee a la
   fin du bloc "SAC DE STATS" ci-dessous) ; les blocs existants de ce fichier ne changent pas tant que les fonctions publiques gardent leur signature */
#include "vagues_tests_roq_rail.c"                        /* Roquettes + Railgun */
#include "vagues_tests_melee.c"                           /* Lance + Impulsion */
#include "vagues_tests_comp.c"                            /* Piece + All in One */
static void vagues_tests(FILE *f) {
    float sNow = (float)nowT;
    {   /* courbe : nombre d'ennemis, PV (seuls a monter, x1,5 au plus), elites */
        fprintf(f, "vagues : ennemis aux vagues 1/33/50/66/100 = %d %d %d %d %d (attendu 3 14 21 26 30)\n",
                vg_count(1), vg_count(33), vg_count(50), vg_count(66), vg_count(100));
        fprintf(f, "vagues : PV x%.2f x%.2f x%.2f x%.2f aux vagues 1/66/100/180, elites %.2f %.2f a 50/100 (attendu 1.00 1.00 1.50 1.50, 0.00 0.60)\n",
                vg_hp_mul(1), vg_hp_mul(66), vg_hp_mul(100), vg_hp_mul(180), vg_elite_p(50), vg_elite_p(100));
        int bosses = vg_is_boss(25) + vg_is_boss(50) + vg_is_boss(75) + vg_is_boss(100) + vg_is_boss(30);
        fprintf(f, "vagues : boss aux vagues 25/50/75/100 = %d (types %d %d %d %d, 125 -> %d) (attendu 4, %d %d %d %d, %d)\n",
                bosses, vg_boss_type(25), vg_boss_type(50), vg_boss_type(75), vg_boss_type(100), vg_boss_type(125),
                EN_PYRA, EN_TESS, EN_BALL, EN_HEXA, EN_PYRA);
    }
    {   /* les degats des ennemis ne montent jamais : une orbe a la vague 1 et a la vague 95, PV d'un drone */
        float hit[2], hp[2];
        for (int k = 0; k < 2; k++) {
            vg_test_begin(0);
            vg_wave_start(k ? 95 : 1);
            memset(E, 0, sizeof E); vgQn = vgQi = 0;
            P.pos = P.prevPos = v3(-20, 0, 30);
            spawn_proj(PR_ORB, vadd(pcenter(), v3(0, 0, -3)), v3(0, 0, 16));
            for (int i = 0; i < 60; i++) proj_tick(TICK);
            hit[k] = 100 - P.hp;
            hp[k] = E[spawn_enemy_type(EN_DRONE)].maxHp;
        }
        fprintf(f, "vagues : orbe recue vague 1=%.0f vague 95=%.0f, drone vague 1=%.0f vague 95=%.0f (attendu 12 12, 100 143)\n",
                hit[0], hit[1], hp[0], hp[1]);
    }
    {   /* SENTINELLE (ennemi vert) : orbe tres rapide (110 m/s) ou laser parryable, avec les degats de l'orbe du type (aucune inflation) */
        float speed = 0, hurt[2], pv2 = 0; int orbs = 0, deadByParry = 0, parried = 0, lasers = 0;
        for (int k = 0; k < 2; k++) {                     /* k = 0 : orbe rapide, k = 1 : laser, laisse toucher */
            vg_test_begin(0);
            P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
            int s = vg_test_enemy(EN_SNIPER, v3(-20, 2.2f, 20), 80);
            E[s].st = 1; E[s].stT = 0.001f; E[s].phase = k; E[s].aim = vnorm(vsub(pcenter(), E[s].pos));
            enemy_tick(TICK);
            for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].kind == PR_ORB) { orbs++; if (!k) speed = vlen(PR[i].vel); }
            for (int i = 0; i < 120; i++) proj_tick(TICK);
            hurt[k] = 100 - P.hp;
        }
        vg_test_begin(0);                                 /* laser : parre au bon moment, l'orbe-eclair repart sur la sentinelle */
        P.pos = P.prevPos = v3(-20, 0, 40);
        int s = vg_test_enemy(EN_SNIPER, v3(-20, 2.2f, 20), 80);
        E[s].st = 1; E[s].stT = 0.001f; E[s].phase = 1; E[s].aim = vnorm(vsub(pcenter(), E[s].pos));
        {
            V3 d = vsub(E[s].pos, eye_pos());
            P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(d.y / vlen(d));
        }
        enemy_tick(TICK);
        P.parryWin = PARRY_WINDOW;
        parried = try_parry(MELEE_RANGE + 1.5f, 1);
        for (int i = 0; i < 240; i++) proj_tick(TICK);
        deadByParry = !E[s].alive; pv2 = P.hp;
        for (int i = 0; i < 400; i++) {                   /* part des lasers parmi les tirs : VG_SNIPER_LASER_P */
            vg_test_begin(0);
            P.pos = P.prevPos = v3(-20, 0, 40);
            int q = vg_test_enemy(EN_SNIPER, v3(-20, 2.2f, 20), 80);
            E[q].st = 0; E[q].stT = 0;
            enemy_tick(TICK);
            lasers += E[q].st == 1 && E[q].phase == 1;
        }
        fprintf(f, "sentinelle : orbes %d, vitesse de l'orbe rapide %.0f m/s (attendu 2, %.0f, au lieu de 34), degats recus orbe %.0f laser %.0f "
                   "(attendu %.0f %.0f : l'orbe du type, aucune inflation) ; laser pare : orbe renvoyee %d, sentinelle morte %d, PV %.0f "
                   "(attendu 1, 1, 100) ; part de lasers sur 400 tirs %.0f%% (attendu ~%.0f%%)\n",
                orbs, speed, VG_SNIPER_FAST_SPEED, hurt[0], hurt[1], VG_ORB_DMG, VG_ORB_DMG, parried, deadByParry, pv2,
                100.0f * lasers / 400, 100 * VG_SNIPER_LASER_P);
    }
    {   /* deroule : vagues qui s'enchainent, selecteur apres la 10, choix d'une carte difficile */
        vg_test_begin(0);
        int w0 = 0, wMax = 0;
        for (int i = 0; i < 120 * 40 && wave < 3; i++) {
            nowT += TICK; P.hp = 100;
            game_tick(TICK); hitstop = 0;
            if (wave == 1 && !w0) w0 = vgQn;
            if (i % 30 == 0) vg_test_kill_all();
            if (wave > wMax) wMax = wave;
        }
        fprintf(f, "vagues : vague 1 de %d ennemis, vague atteinte en jouant=%d (attendu 3, 3)\n", w0, wMax);
        wave = 10; vgState = VG_FIGHT; vgQn = vgQi = 0; vg_test_kill_all();
        wave_tick(TICK);
        int sel = vgMenu == VGM_SELECT, ch0 = vgChoiceMap[0], ch1 = vgChoiceMap[1];
        vgPend = 1; vg_menu_close();
        wave_tick(TICK);
        fprintf(f, "vagues : apres la vague 10 selecteur=%d (simple %s, difficile %s), choix difficile -> carte %s credits x%.1f (attendu 1, %s, LE CANYON)\n",
                sel, maps[ch0].name, maps[ch1].name, maps[curMap].name, vgBlockHard ? VG_HARD_CREDITS : 1.0f,
                maps[ch0].name);
        int c0 = vgCredits; kill_enemy(vg_test_enemy(EN_DRONE, v3(0, 10, 0), 10), 1);
        int hard = vgCredits - c0;
        vgBlockHard = 0; c0 = vgCredits; kill_enemy(vg_test_enemy(EN_DRONE, v3(0, 10, 0), 10), 1);
        int simple = vgCredits - c0;
        int s = vg_test_enemy(EN_DRONE, v3(0, 10, 0), 10); E[s].summoned = 1;
        c0 = vgCredits; kill_enemy(s, 1);
        fprintf(f, "vagues : credits d'un drone simple=%d difficile=%d invoque=%d (attendu 10 15 3)\n", simple, hard, vgCredits - c0);
    }
    {   /* boutique v2 : offres, prix, relance, achat, degats de Calibre (+10 %) */
        vg_test_begin(0);
        P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_COIN;
        sh_reset();
        vgCredits = 5000;
        int offers = shNOff, price0 = shOff[0].price, c0 = vgCredits;
        int a = up_buy(0), spent = c0 - vgCredits, owned = sh_count_owned();
        int r1 = sh_reroll_price(); sh_reroll(); int r2 = sh_reroll_price();
        vgCredits = 0; int poor = up_buy(0);
        float d[2];
        for (int k = 0; k < 2; k++) {
            shOwn[SH_RAIL] = k ? 1u : 0;              /* Calibre : +10 % de degats du Railgun */
            memset(E, 0, sizeof E);
            int i = vg_test_enemy(EN_DRONE, v3(0, 10, 0), 1000);
            vgSrc = WP_RAIL; damage_enemy(i, 100, v3(0, 0, 0)); vgSrc = -1;
            d[k] = 1000 - E[i].hp;
        }
        fprintf(f, "boutique vagues : offres>=1 %d, achat %d (prix %d, depense %d, etapes %d), sans assez %d, relance %d puis %d, Calibre %.0f -> %.0f (attendu 1, 1, vrai, vrai, 1, 0, 40 puis 60, 100 -> 110)\n",
                offers >= 1, a, price0 == spent, spent > 0, owned, poor, r1, r2, d[0], d[1]);
    }
    {   /* effets : FOURCHE (kill), FRACTURE (All in One), SATELLITE (tir), vol de vie de la Lance */
        vg_test_begin(0);
        P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_ALLIN;
        sh_reset();
        memset(E, 0, sizeof E);
        shOwn[SH_RAIL] = (1u << 7) | (1u << 6);          /* Fourche, Satellite */
        shOwn[SH_AIO] = 1u << 12;                        /* Fracture (profondeur 3, rang 2) */
        shOwn[SH_LAN] = 1u << 4;                         /* Seve de lance : 2 % de vol de vie */
        int v = vg_test_enemy(EN_DRONE, v3(-20, 5, 10), 50), n1 = vg_test_enemy(EN_DRONE, v3(-16, 5, 10), 500),
            n2 = vg_test_enemy(EN_DRONE, v3(-20, 5, 14), 500);
        vgSrc = WP_RAIL; damage_enemy(v, 100, v3(0, 0, 0));
        int fork = (E[n1].hp < 500) + (E[n2].hp < 500);
        float hp1 = E[n1].hp;
        vgAllinT = 1;
        damage_enemy(n1, 10, v3(0, 0, 0)); damage_enemy(n1, 10, v3(0, 0, 0)); damage_enemy(n1, 10, v3(0, 0, 0));
        int frac = fxStat[FX_FRACTURE] > 0 && E[n1].hp < hp1 - 30 - 40;
        vgAllinT = 0;
        for (int k = 0; k < MAXVS; k++) VS[k].on = 0;
        vg_on_fire(WP_RAIL);
        int sat = 0;
        for (int k = 0; k < MAXVS; k++) sat += VS[k].on;
        P.hp = 50; P.dead = 0; vgSrc = UP_MELEE(MK_LANCE);
        damage_enemy(n2, 100, v3(0, 0, 0)); vgSrc = -1;
        fprintf(f, "effets vagues : fourche sur %d voisins, fracture=%d, satellite=%d, vol de vie PV=%.0f (attendu 2, 1, 1, 52)\n",
                fork, frac, sat, P.hp);
    }
    {   /* boutique SHOTGUN : plombs en plus, rafale, pompe tendue, peau de plomb, double canon, rechargement, capstone ROQUETTES */
        vg_test_begin(0);
        P.loadout[0] = WP_SHOTGUN; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_COIN; P.weapon = WP_SHOTGUN;
        P.dead = 0; P.vel = v3(0, 0, 0);
        sh_reset();
        memset(E, 0, sizeof E);
        int np[2]; float dm[2], fo[2];
        for (int k = 0; k < 2; k++) {                    /* Plombs en plus : 10 -> 12 plombs sur une grosse cible proche */
            shOwn[SH_SHO] = k ? (1u << 11) : 0;
            memset(E, 0, sizeof E);
            V3 at = vadd(eye_pos(), vmul(view_dir(), 5));
            int i = vg_test_enemy(EN_DRONE, at, 100000);
            P.ammo[WP_SHOTGUN] = 3; sgFree = 0;
            vgSrc = WP_SHOTGUN; fire_shotgun(); vgSrc = -1;
            np[k] = P.sgLastPellets; dm[k] = 100000 - E[i].hp;
        }
        shOwn[SH_SHO] = 1u << 6;                         /* Rafale : une 2e salve a 50 % apres 0,15 s */
        memset(E, 0, sizeof E);
        {
            V3 at = vadd(eye_pos(), vmul(view_dir(), 5));
            int i = vg_test_enemy(EN_DRONE, at, 100000);
            P.ammo[WP_SHOTGUN] = 3; vgsgFreeShots = 0; memset(sgPend, 0, sizeof sgPend);
            vgSrc = WP_SHOTGUN; fire_shotgun(); vgSrc = -1;
            float d1 = 100000 - E[i].hp;
            vgsg_tick(0.2f);
            float d2 = 100000 - E[i].hp - d1;
            fprintf(f, "boutique shotgun : plombs %d -> %d, rafale %d salve gratuite (2e/1ere=%.2f), ammo=%d (attendu 10 -> 12, 1, 0.50, 2)\n",
                    np[0], np[1], vgsgFreeShots, d1 > 0 ? d2 / d1 : 0, P.ammo[WP_SHOTGUN]);
        }
        for (int k = 0; k < 2; k++) {                    /* Pompe tendue (rang 3 profondeur 4) : aucune perte a 20 m */
            shOwn[SH_SHO] = k ? (1u << 18) : 0;
            SgStats sgs; sg_stats(&sgs, SG_HALF_ANGLE, SG_RANGE);
            fo[k] = sg_fall(&sgs, 20);
        }
        shOwn[SH_SHO] = 1u << 10;                        /* Peau de plomb : +8 PV si 6 plombs touchent */
        memset(E, 0, sizeof E);
        {
            V3 at = vadd(eye_pos(), vmul(view_dir(), 4));
            vg_test_enemy(EN_DRONE, at, 100000);
            P.hp = 50; P.ammo[WP_SHOTGUN] = 3;
            vgSrc = WP_SHOTGUN; fire_shotgun(); vgSrc = -1;
            float hpPeau = P.hp;
            shOwn[SH_SHO] = 1u << 12;                    /* Double canon : -15 % de jauge */
            P.ammo[WP_SHOTGUN] = 3; P.ammoGauge[WP_SHOTGUN] = 0;
            vgSrc = WP_SHOTGUN; fire_shotgun(); vgSrc = -1;
            float gj = P.ammoGauge[WP_SHOTGUN];
            shOwn[SH_SHO] = 1u << 4;                     /* Rechargement : +20 % par kill */
            P.ammoGauge[WP_SHOTGUN] = 0; P.ammo[WP_SHOTGUN] = 0;
            ammo_points(50, AP_KILL);
            float rg = P.ammoGauge[WP_SHOTGUN] + P.ammo[WP_SHOTGUN] * 100.0f;
            fprintf(f, "boutique shotgun : pompe tendue x%.2f -> x%.2f a 20 m, peau de plomb PV=%.0f, double canon jauge=%.0f, rechargement=%.0f (attendu 0.65 -> 1.00, 58, 15, 60)\n",
                    fo[0], fo[1], hpPeau, gj, rg);
        }
        shOwn[SH_SHO] = 1u << 11;                        /* capstone ROQUETTES : le plomb explose 2 m (voisin touche) */
        for (int k = 0; k < 2; k++) {
            int b = sh_branch(SH_SHO, SH_ROQ);
            shCap[b] = (unsigned char)k;
            memset(E, 0, sizeof E);
            V3 at = vadd(eye_pos(), vmul(view_dir(), 5));
            vg_test_enemy(EN_DRONE, at, 100000);
            int nb = vg_test_enemy(EN_DRONE, vadd(at, v3(0, 1.6f, 0)), 100000);
            P.ammo[WP_SHOTGUN] = 3; P.loadout[1] = WP_ROCKET;
            vgSrc = WP_SHOTGUN; fire_shotgun(); vgSrc = -1;
            fo[k] = 100000 - E[nb].hp;
            shCap[b] = 0;
        }
        fprintf(f, "boutique shotgun : capstone ROQUETTES, degats au voisin %.0f -> %.0f (attendu 0 -> >0)\n", fo[0], fo[1]);
        {                                                /* capstone RAILGUN : les plombs qui touchent remplissent la jauge du rail */
            int br = sh_branch(SH_SHO, SH_RAIL);
            float g[2];
            for (int k = 0; k < 2; k++) {
                shCap[br] = (unsigned char)k;
                memset(E, 0, sizeof E);
                vg_test_enemy(EN_DRONE, vadd(eye_pos(), vmul(view_dir(), 5)), 100000);
                P.loadout[0] = WP_SHOTGUN; P.loadout[1] = WP_RAIL; P.ammo[WP_SHOTGUN] = 3; P.ammo[WP_RAIL] = 0; P.ammoGauge[WP_RAIL] = 0;
                vgSrc = WP_SHOTGUN; fire_shotgun(); vgSrc = -1;
                g[k] = P.ammoGauge[WP_RAIL] + P.ammo[WP_RAIL] * ammoDef[WP_RAIL].gauge;
                shCap[br] = 0;
            }
            fprintf(f, "boutique shotgun : capstone RAILGUN, jauge du rail apres une salve %.0f -> %.0f (attendu 0 -> >0)\n", g[0], g[1]);
        }
        shOwn[SH_SHO] = 0;
    }
    {   /* boutique ROQUETTES : etapes, capstones et fusions (src/solo/boutique_roquettes.c) */
        int nR, okSpd, okSalve, okRad, okHeal, okGrappe, okRecup, okCore, okNuke, okProx, okCurve, okSlow, okPel, okLance, okArt,
            okFist, okAio, okTap;
        float dmgA = 0, dmgB = 0, fC = 0;
        vg_test_begin(0);
        P.loadout[0] = WP_ROCKET; P.loadout[1] = WP_RAIL; P.meleeKind = MK_FIST; P.skill = SK_ALLIN;
        P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0;
        sh_reset();
        memset(PR, 0, sizeof PR);
        shOwn[SH_ROQ] = (1u << RQ_PROP) | (1u << RQ_LOURDE) | (1u << RQ_COMB);
        P.ammo[WP_ROCKET] = 8; P.fireCd = 0;
        fire_rocket();
        float spd = 0, life = 0;
        for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].kind == PR_ROCKET) { spd = vlen(PR[i].vel); life = PR[i].life; }
        okSpd = fabsf(spd - ROCKET_SPEED * 1.1f * 0.9f) < 0.1f && life > 6.4f;
        memset(PR, 0, sizeof PR);
        shOwn[SH_ROQ] = 1u << RQ_SALVE;
        fire_rocket();
        nR = 0; okSalve = 1;
        for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].kind == PR_ROCKET) { nR++; okSalve &= fabsf(PR[i].mult - 0.6f) < 0.001f; }
        okSalve &= nR == 2;
        memset(PR, 0, sizeof PR);
        Proj *pr = spawn_proj(PR_ROCKET, v3(0, 0, 0), v3(0, 0, 0));
        shOwn[SH_ROQ] = (1u << RQ_RAYON) | (1u << RQ_GRAND);
        okRad = fabsf(roq_radius(pr) - 5.7f) < 0.001f;
        shOwn[SH_ROQ] = 1u << RQ_SOUTE; okRad &= roq_max(WP_ROCKET, 8) == 9;
        V3 c0 = v3(-20, 6, 10);
        for (int k = 0; k < 2; k++) {
            shOwn[SH_ROQ] = k ? 1u << RQ_COEUR : 0;
            memset(E, 0, sizeof E);
            int a = vg_test_enemy(EN_DRONE, vadd(c0, v3(0.5f, 0, 0)), 1000);
            vgSrc = WP_ROCKET; pr->mult = 1; roq_explode(pr, c0, -1, 1); vgSrc = -1;
            fC = 1 - 0.5f / (ROCKET_RADIUS + enemy_radius(EN_DRONE));
            if (k) dmgB = 1000 - E[a].hp; else dmgA = 1000 - E[a].hp;
        }
        okCore = dmgA > 0 && fabsf(dmgB / dmgA - (1 + 0.25f * fC)) < 0.01f;
        shOwn[SH_ROQ] = (1u << RQ_CRATERE) | (1u << RQ_RECUP) | (1u << RQ_GRAPPE);
        memset(E, 0, sizeof E); memset(PR, 0, sizeof PR);
        pr = spawn_proj(PR_ROCKET, v3(0, 0, 0), v3(0, 0, 0));
        int e1 = vg_test_enemy(EN_DRONE, vadd(c0, v3(0.5f, 0, 0)), 20);
        vg_test_enemy(EN_DRONE, vadd(c0, v3(-1, 0, 0)), 1000);
        P.hp = 50; P.dead = 0; P.ammo[WP_ROCKET] = 2;
        vgSrc = WP_ROCKET; pr->mult = 1; roq_explode(pr, c0, -1, 1); vgSrc = -1;
        okHeal = P.hp >= 61.9f; if (!okHeal) fprintf(f, "dbg hp %.2f\n", P.hp);
        okRecup = !E[e1].alive && P.ammo[WP_ROCKET] == 3;
        nR = 0;
        for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].kind == PR_ROCKET && PR[i].roqGen == 1 && fabsf(PR[i].mult - 0.3f) < 0.001f) nR++;
        okGrappe = nR == 3;
        shOwn[SH_ROQ] = (1u << RQ_FUSEE) | (1u << RQ_OGIVE);
        memset(E, 0, sizeof E); memset(PR, 0, sizeof PR);
        int en = vg_test_enemy(EN_DRONE, v3(-20, 6, 12), 1000);
        pr = spawn_proj(PR_ROCKET, v3(-20, 6, 5), v3(0, 0, ROCKET_SPEED));
        roq_setup(pr);
        okProx = pr->roqSize > 1 && roq_hitpad(pr) > 0.25f;
        for (int i = 0; i < 40 && pr->alive; i++) proj_tick(TICK);
        okProx &= !pr->alive && E[en].hp < 1000;
        memset(E, 0, sizeof E); memset(PR, 0, sizeof PR);
        shOwn[SH_ROQ] = 1u << RQ_COURBE;
        vg_test_enemy(EN_DRONE, v3(-14, 6, 10), 1000);
        pr = spawn_proj(PR_ROCKET, v3(-20, 6, 5), v3(0, 0, ROCKET_SPEED));
        roq_setup(pr);
        roq_tick_proj(pr, TICK); roq_tick_proj(pr, TICK); roq_tick_proj(pr, TICK);
        okCurve = pr->vel.x > 1.0f && fabsf(vlen(pr->vel) - ROCKET_SPEED) < 0.1f;
        shOwn[SH_ROQ] = 0;
        shCap[sh_branch(SH_ROQ, SH_RAIL)] = 1;
        P.ammo[WP_ROCKET] = 5; roq_fx_tick(TICK);
        memset(E, 0, sizeof E); memset(PR, 0, sizeof PR);
        pr = spawn_proj(PR_ROCKET, v3(0, 0, 0), v3(0, 0, 0));
        int farE = vg_test_enemy(EN_DRONE, vadd(c0, v3(10, 0, 0)), 1000);
        vgSrc = WP_ROCKET; pr->mult = 1; roq_explode(pr, c0, -1, 1); vgSrc = -1;
        okNuke = P.ammo[WP_ROCKET] == 1 && roq_max(WP_ROCKET, 8) == 1 && roq_radius(pr) == 14.0f && E[farE].hp < 1000 - 100;
        shCap[sh_branch(SH_ROQ, SH_RAIL)] = 0;
        shCap[sh_branch(SH_ROQ, SH_IMP)] = 1;
        memset(PR, 0, sizeof PR);
        pr = spawn_proj(PR_ROCKET, vadd(pcenter(), v3(2, 0, 0)), v3(ROCKET_SPEED, 0, 0));
        roq_tick_proj(pr, TICK);
        okSlow = fabsf(vlen(pr->vel) - ROCKET_SPEED * 0.6f) < 0.1f;
        shCap[sh_branch(SH_ROQ, SH_IMP)] = 0;
        P.loadout[1] = WP_SHOTGUN;
        shCap[sh_branch(SH_ROQ, SH_SHO)] = 1;
        memset(E, 0, sizeof E); memset(PR, 0, sizeof PR);
        pr = spawn_proj(PR_ROCKET, v3(0, 0, 0), v3(0, 0, 0));
        int ps = vg_test_enemy(EN_DRONE, vadd(c0, v3(8, 0, 0)), 1000);
        roqStat[1] = 0;
        vgSrc = WP_ROCKET; pr->mult = 1; roq_explode(pr, c0, -1, 1); vgSrc = -1;
        okPel = roqStat[1] == 6 && E[ps].hp < 1000; if (!okPel) fprintf(f, "dbg pel %d %.1f\n", roqStat[1], E[ps].hp);
        shCap[sh_branch(SH_ROQ, SH_SHO)] = 0;
        shCap[sh_branch(SH_ROQ, SH_LAN)] = 1;
        memset(PR, 0, sizeof PR);
        P.lanceState = LS_COUCH; P.ammo[WP_ROCKET] = 4; P.fireCd = 0;
        P.meleeKind = MK_LANCE;
        fire_rocket();
        spd = 0;
        for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].kind == PR_ROCKET) spd = vlen(PR[i].vel);
        okTap = fabsf(spd - 2 * ROCKET_SPEED) < 0.1f;
        P.lanceState = LS_IDLE;
        shCap[sh_branch(SH_ROQ, SH_LAN)] = 0;
        int fl = -1, fp = -1, fi = -1, fa = -1;
        for (int q = 0; q < SH_NFU; q++)
            if (shFu[q].a == SH_ROQ) {
                if (shFu[q].b == SH_LAN) fl = q;
                if (shFu[q].b == SH_PIE) fp = q;
                if (shFu[q].b == SH_IMP) fi = q;
                if (shFu[q].b == SH_AIO) fa = q;
            }
        P.meleeKind = MK_LANCE; shFusion = fl;
        memset(PR, 0, sizeof PR);
        roq_joust();
        nR = 0; okLance = 0;
        for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].kind == PR_ROCKET) { nR++; okLance = fabsf(PR[i].mult - 1.5f) < 0.001f; }
        okLance &= nR == 1;
        P.skill = SK_COIN; shFusion = fp;
        memset(PR, 0, sizeof PR); memset(CO, 0, sizeof CO); memset(E, 0, sizeof E);
        CO[0].alive = 1; CO[0].owner = myId; CO[0].pos = c0;
        int ea = vg_test_enemy(EN_DRONE, vadd(c0, v3(1, 0, 0)), 1000);
        pr = spawn_proj(PR_ROCKET, vadd(c0, v3(0, 0, -1)), v3(0, 0, ROCKET_SPEED));
        rocket_coin(pr, 0);
        okArt = CO[0].alive && pr->roqCoinLast == 1 && E[ea].hp < 1000;
        P.skill = SK_ALLIN; P.meleeKind = MK_FIST; shFusion = fi;
        memset(PR, 0, sizeof PR);
        pr = spawn_proj(PR_ROCKET, vadd(eye_pos(), vmul(view_dir(), 2)), vmul(view_dir(), 5));
        P.vel = v3(0, 0, 0);
        okFist = roq_fist(eye_pos(), view_dir()) == 1 && !pr->alive && fabsf(vlen(P.vel) - 25) < 0.1f;
        shFusion = fa;
        memset(PR, 0, sizeof PR);
        P.ammo[WP_ROCKET] = 4; P.fireCd = 0; roqAllinDone = 0; allinActive = 1;
        fire_rocket();
        allinActive = 0;
        nR = 0;
        for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].kind == PR_ROCKET) nR++;
        okAio = nR == 3;
        shFusion = -1; shCap[sh_branch(SH_ROQ, SH_AIO)] = 1;
        memset(PR, 0, sizeof PR);
        P.ammo[WP_ROCKET] = 4; P.fireCd = 0; roqAllinDone = 0; allinActive = 1;
        fire_rocket();
        allinActive = 0;
        nR = 0;
        for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].kind == PR_ROCKET) nR++;
        okAio &= nR == 9;
        shCap[sh_branch(SH_ROQ, SH_AIO)] = 0;
        fprintf(f, "boutique roquettes : vitesse+portee %d, salve %d, rayon+soute %d, coeur %d, cratere %d, recup %d, grappe %d, proximite+ogive %d, courbe %d (attendu 1 x9)\n",
                okSpd, okSalve, okRad, okCore, okHeal, okRecup, okGrappe, okProx, okCurve);
        fprintf(f, "boutique roquettes : mini-nuke %d, lente %d, cluster %d, montee %d, joute %d, artifice %d, poing %d, tapis+salve %d (attendu 1 x8)\n",
                okNuke, okSlow, okPel, okTap, okLance, okArt, okFist, okAio);
        shFusion = -1; vgSrc = -1;
        memset(PR, 0, sizeof PR); memset(CO, 0, sizeof CO); memset(E, 0, sizeof E);
    }
    {   /* boutique RAILGUN : largeur, visee courbe, traversee, souffle d'impact */
        float r = enemy_radius(EN_DRONE), v[2], w[2], s[2], b[2];
        for (int k = 0; k < 2; k++) {
            rl_t_setup(k ? 1u << 2 : 0);                  /* Rail large : un ennemi a 1,1 rayon du tir */
            int e = vg_test_enemy(EN_DRONE, rl_t_at(10, r * 1.1f, 0), 1000);
            v[k] = rl_t_fire(e);
            rl_t_setup(k ? 1u << 19 : 0);                 /* Tir guide : ennemi a 5 degres de la ligne de mire */
            e = vg_test_enemy(EN_DRONE, rl_t_at(20, 20 * tanf(5 * PI / 180), 0), 1000);
            w[k] = rl_t_fire(e);
            rl_t_setup(k ? 1u << 12 : 0);                 /* Calibre lourd : la 2e cible en ligne +15 % */
            int e1 = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 1000), e2 = vg_test_enemy(EN_DRONE, rl_t_at(16, 0, 0), 1000);
            rl_t_fire(e1); s[k] = 1000 - E[e2].hp;
            rl_t_setup(k ? 1u << 5 : 0);                  /* Eclat d'impact : souffle sur le voisin de la 1re cible */
            e1 = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 1000); e2 = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 1.2f), 1000);
            rl_t_fire(e1); b[k] = 1000 - E[e2].hp;
        }
        fprintf(f, "boutique rail : Rail large %.0f -> %.0f, Tir guide %.0f -> %.0f, Calibre lourd (2e cible) %.0f -> %.0f, Eclat d'impact (voisin) %.0f -> %.0f (attendu 0 -> 100, 0 -> 100, 100 -> 115, 0 -> >0)\n",
                v[0], v[1], w[0], w[1], s[0], s[1], b[0], b[1]);
    }
    {   /* boutique RAILGUN : tete (Sang-froid, Surtension, Tir explosif, Lunette), Double, Reserve */
        float r = enemy_radius(EN_DRONE), heal, expl[2], lun[2], g[2];
        int streak[2];
        rl_t_setup(1u << 10);                             /* Sang-froid : +6 PV par touche a la tete */
        int e = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, -0.8f * r), 1000);
        P.hp = 50; rl_t_fire(e); heal = P.hp;
        for (int k = 0; k < 2; k++) {
            rl_t_setup(k ? 1u << 8 : 0);                  /* Surtension : la tete avance la chaine de 3 au lieu de 2 */
            e = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, -0.8f * r), 1000);
            P.ammo[WP_RAIL] = 2; shotStreak = 0; vgSrc = WP_RAIL; fire_rail(); vgSrc = -1;
            streak[k] = shotStreak;
            rl_t_setup(k ? 1u << 14 : 0);                 /* Tir explosif : souffle de 3 m quand la tete est touchee */
            e = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, -0.8f * r), 1000);
            int n = vg_test_enemy(EN_DRONE, vadd(E[e].pos, v3(0, 2, 0)), 1000);
            rl_t_fire(e); expl[k] = 1000 - E[n].hp;
            rl_t_setup(k ? 1u << 9 : 0);                  /* Lunette : tete 15 % plus large */
            e = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, -0.4f * r), 1000);
            lun[k] = rl_t_fire(e);
        }
        rl_t_setup(1u << 11);                             /* Double : 2 kills en un tir rendent 1 munition */
        int e1 = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 30), e2 = vg_test_enemy(EN_DRONE, rl_t_at(14, 0, 0), 30);
        P.ammo[WP_RAIL] = 2; vgSrc = WP_RAIL; fire_rail(); vgSrc = -1;
        int dbl = P.ammo[WP_RAIL], dead = !E[e1].alive + !E[e2].alive;
        for (int k = 0; k < 2; k++) {                     /* Reserve : +20 % de points de jauge sur un kill de melee */
            rl_t_setup(k ? 1u << 1 : 0);
            P.ammo[WP_RAIL] = 0; P.ammoGauge[WP_RAIL] = 0;
            ammo_points(100, AP_KILL); g[k] = P.ammoGauge[WP_RAIL];
        }
        fprintf(f, "boutique rail : Sang-froid PV=%.0f, chaine tete %d -> %d, Tir explosif voisin %.0f -> %.0f, Lunette %.0f -> %.0f, Double munitions=%d (morts %d), Reserve jauge %.0f -> %.0f (attendu 56, 2 -> 3, 0 -> >0, 100 -> 150, 2, 2, 100 -> 120)\n",
                heal, streak[0], streak[1], expl[0], expl[1], lun[0], lun[1], dbl, dead, g[0], g[1]);
    }
    {   /* boutique RAILGUN : Perce-mur (mur de 0,5 m), Rail jumeau, Rail orbital, joute, agile, Banque */
        float pm[2], tw = 0, orb, orbAmmo, joute[2];
        int twins = 0;
        for (int k = 0; k < 2; k++) {
            rl_t_setup(k ? 1u << 18 : 0);
            V3 d = view_dir(); int ax = fabsf(d.x) > 0.5f;
            int nb0 = nb;
            Box *bx = &boxes[nb++];
            memset(bx, 0, sizeof *bx);
            V3 c = rl_t_at(6, 0, 0), h = ax ? v3(0.25f, 4, 4) : v3(4, 4, 0.25f);
            bx->mn = vsub(c, h); bx->mx = vadd(c, h);
            int e = vg_test_enemy(EN_DRONE, rl_t_at(12, 0, 0), 1000);
            pm[k] = rl_t_fire(e);
            nb = nb0;
        }
        rl_t_setup(1u << 15);                             /* Rail jumeau : sur 80 tirs, au moins un double (et pas tous) */
        int e = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 1000000);
        for (int k = 0; k < 80; k++) { tw = rl_t_fire(e); twins += tw > 150; E[e].hp = 1000000; }
        rl_t_setup(0); rl_t_cap(SH_ROQ);                  /* Rail orbital : x4, souffle de 12 m, une seule munition en reserve */
        e = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 1000);
        int n = vg_test_enemy(EN_DRONE, vadd(E[e].pos, v3(0, 8, 0)), 1000);
        orb = rl_t_fire(e); vg_rail_tick(TICK); orbAmmo = (float)P.ammo[WP_RAIL];
        float nb2 = 1000 - E[n].hp;
        for (int k = 0; k < 2; k++) {                     /* Rail de joute : +40 % pendant la lance couchee */
            rl_t_setup(0); if (k) { rl_t_cap(SH_LAN); P.lanceState = LS_COUCH; }
            e = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 1000);
            joute[k] = rl_t_fire(e);
        }
        rl_t_setup(0); rl_t_cap(SH_IMP); P.meleeKind = MK_FIST;   /* Rail agile : un dash rendu */
        e = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 1000);
        P.dashCharge = 0; rl_t_fire(e); float dash = P.dashCharge;
        rl_t_setup(0); rl_t_cap(SH_PIE);                  /* Banque : le rail qui traverse une piece est gratuit */
        CO[0].alive = 1; CO[0].owner = myId; CO[0].life = 4; CO[0].decoy = 0; CO[0].pos = rl_t_at(5, 0, 0);
        e = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 1000);
        P.ammo[WP_RAIL] = 2; vgSrc = WP_RAIL; fire_rail(); vgSrc = -1;
        int bank = P.ammo[WP_RAIL];
        fprintf(f, "boutique rail : Perce-mur %.0f -> %.0f, Rail jumeau %d doubles sur 80, Rail orbital %.0f (voisin a 8 m %.0f, munitions %.0f), Rail de joute %.0f -> %.0f, Rail agile dash=%.0f, Banque munitions=%d (attendu 0 -> 70, 1..79, 400, >0, 1, 100 -> 140, 1, 2)\n",
                pm[0], pm[1], twins, orb, nb2, orbAmmo, joute[0], joute[1], dash, bank);
    }
    {   /* boutique RAILGUN : All in One (Rail multiple, FUSILLADE), CHEVALIER, COMBO AERIEN */
        float al[3];
        for (int k = 0; k < 3; k++) {
            rl_t_setup(0); P.skill = SK_ALLIN; P.skillCh = 1;
            if (k == 1) rl_t_cap(SH_AIO);
            if (k == 2) shFusion = rl_t_fusion(SH_AIO);
            int e = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 100000);
            P.ammo[WP_RAIL] = 2; shotStreak = 0; vgSrc = WP_RAIL; skill_allin(); vgSrc = -1;
            al[k] = 100000 - E[e].hp;
        }
        rl_t_setup(0); shFusion = rl_t_fusion(SH_LAN);   /* CHEVALIER : une joute recharge un rail */
        P.ammo[WP_RAIL] = 0;
        int e = vg_test_enemy(EN_DRONE, rl_t_at(3, 0, 0), 1000);
        lance_strike(e, 0, 20);
        int chev = P.ammo[WP_RAIL];
        rl_t_setup(0); shFusion = rl_t_fusion(SH_IMP); P.meleeKind = MK_FIST;   /* COMBO AERIEN : la chute est suspendue */
        e = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 1000);
        P.grounded = 0; rl_t_fire(e);
        P.vel = v3(0, -20, 0); vg_rail_tick(TICK);
        float fall = P.vel.y;
        fprintf(f, "boutique rail : All in One degats %.0f -> %.0f (Rail multiple) -> %.0f (FUSILLADE), CHEVALIER munitions=%d, COMBO AERIEN vitesse verticale=%.0f (attendu 215 -> >215 -> >215, 1, 0)\n",
                al[0], al[1], al[2], chev, fall);
        shFusion = -1; memset(shCap, 0, sizeof shCap); shOwn[SH_RAIL] = 0;
    }
    {   /* boutique melee : Lance (estocs, joute) et Impulsion (combo) */
        vg_test_begin(0);
        P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_COIN;
        vgm_reset(); memset(vgmStat, 0, sizeof vgmStat);
        P.pos = P.prevPos = v3(-20, 0, 41); P.yaw = 0; P.pitch = 0; P.vel = v3(0, 0, 0); P.meleeCd = 0; P.lanceState = LS_IDLE;
        V3 fw = view_dir();
        memset(E, 0, sizeof E);
        int a = vg_test_enemy(EN_DRONE, vadd(eye_pos(), vmul(fw, 2)), 5000);
        shOwn[SH_LAN] = (1u << 2) | (1u << 5) | (1u << 3);   /* Second estoc, Estoc vif, Moisson */
        float g0 = P.ammoGauge[WP_RAIL] + P.ammoGauge[WP_ROCKET];
        lance_press(); hitstop = 0;
        for (int k = 0; k < 40; k++) { lance_tick(TICK, 0); hitstop = 0; }
        float gp = P.ammoGauge[WP_RAIL] + P.ammoGauge[WP_ROCKET] - g0;
        fprintf(f, "boutique melee : estocs en plus %d, points d'estoc %.0f, recharge d'estoc %.3f (attendu 1, %.0f, 0.425)\n",
                vgmStat[0], gp, P.meleeCd, (float)((meleePts[MK_LANCE] + 10) % 50));
        /* joute : Recharge (-0,2 s), Butin (+25), Armure de fer (+10 PV), Tournoi (+20 %) */
        shOwn[SH_LAN] = (1u << 0) | (1u << 8) | (1u << 10) | (1u << 17);
        P.hp = 50; P.lanceState = LS_IDLE; P.meleeCd = 0; P.swingHitPts = 0; vgm_reset();
        E[a].hp = 5000; g0 = P.ammoGauge[WP_RAIL] + P.ammoGauge[WP_ROCKET];
        lance_strike(a, 0, 20); hitstop = 0;
        float d1 = 5000 - E[a].hp, gj = P.ammoGauge[WP_RAIL] + P.ammoGauge[WP_ROCKET] - g0, cdj = P.meleeCd, hpj = P.hp;
        E[a].hp = 5000; lance_strike(a, 0, 20); hitstop = 0;
        float d2 = 5000 - E[a].hp;
        fprintf(f, "boutique melee : joute recharge %.2f PV %.0f points %.0f, tournoi %.0f -> %.0f (attendu 0.80, 60, 25 (125 pts), %.0f -> %.0f)\n",
                cdj, hpj, gj, d1, d2, d1, d1 * 1.2f);
        /* Poings : 4e et 5e coup, coups vifs, cueillette, uppercut tous les 4 avec 8 coups */
        P.meleeKind = MK_FIST; shOwn[SH_LAN] = 0; vgm_reset();
        shOwn[SH_IMP] = (1u << 2) | (1u << 7) | (1u << 1) | (1u << 3);
        int n5 = vgm_fist_n();
        P.fistStep = 0; P.swingLanded = 0; P.fistPts = 0; E[a].hp = 5000; E[a].pos = vadd(eye_pos(), vmul(fw, 1.5f));
        g0 = P.ammoGauge[WP_RAIL] + P.ammoGauge[WP_ROCKET];
        fist_test_punch(&E[a]); float gap = P.meleeCd;
        shOwn[SH_IMP] = 1u << 17;                           /* Huit coups (profondeur 4, rang 3 = bit 15+2 : voir pool) */
        shOwn[SH_IMP] = 1u << (15 + 2);
        int n8 = vgm_fist_n(), up4 = vgm_fist_up(3, 8), up3 = vgm_fist_up(2, 8);
        fprintf(f, "boutique melee : poings combo %d coups, ecart %.2f, huit coups %d (uppercut au 4e %d, au 3e %d) (attendu 5, 0.20, 8, 1, 0)\n",
                n5, gap, n8, up4, up3);
        shOwn[SH_IMP] = (1u << 10) | (1u << 11) | (1u << 3) | (1u << 6);   /* Souffle acrobate, Combo enchaine, Cueillette... */
        P.hp = 50; vgHealTok = VG_HEAL_CAP; P.fistStep = 2; E[a].hp = 5000; g0 = P.ammoGauge[WP_RAIL] + P.ammoGauge[WP_ROCKET];
        fist_test_punch(&E[a]);
        fprintf(f, "boutique melee : uppercut PV %.0f, recharge de fin %.2f (attendu 58, 0.50)\n", P.hp, P.meleeCd);
        /* capstones */
        int b;
        shOwn[SH_IMP] = 0; P.fistStep = 2; E[a].hp = 5000; vgm_reset(); memset(vgmStat, 0, sizeof vgmStat);
        P.loadout[0] = WP_SHOTGUN; P.loadout[1] = WP_ROCKET;
        b = sh_branch(SH_IMP, SH_SHO); if (b >= 0) shCap[b] = 1;
        fist_test_punch(&E[a]);
        int plombs = vgmStat[1];
        P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.fistStep = 0; E[a].hp = 5000;
        b = sh_branch(SH_IMP, SH_ROQ); if (b >= 0) shCap[b] = 1;
        for (int k = 0; k < MAXPR; k++) PR[k].alive = 0;
        fist_test_punch(&E[a]);
        int roq = 0; float rm = 0;
        for (int k = 0; k < MAXPR; k++) if (PR[k].alive && PR[k].kind == PR_ROCKET) { roq++; rm = PR[k].mult; }
        P.ammo[WP_RAIL] = 0; P.fistStep = 2; E[a].hp = 5000;
        b = sh_branch(SH_IMP, SH_RAIL); if (b >= 0) shCap[b] = 1;
        fist_test_punch(&E[a]);
        int rail = P.ammo[WP_RAIL];
        fprintf(f, "boutique melee : capstones Impulsion plombs %d (>0), roquette %d x%.1f, uppercut recharge le rail %d (attendu 1, 1 x0.6, 2)\n",
                plombs > 0, roq, rm, rail);
        P.meleeKind = MK_LANCE; shOwn[SH_IMP] = 0; memset(shCap, 0, sizeof shCap); vgm_reset(); memset(vgmStat, 0, sizeof vgmStat);
        int ne = vg_test_enemy(EN_DRONE, vadd(E[a].pos, v3(0, 0, 3)), 5000);
        b = sh_branch(SH_LAN, SH_ROQ); if (b >= 0) shCap[b] = 1;
        E[a].hp = 5000; P.lanceState = LS_IDLE; E[a].pos = vadd(eye_pos(), vmul(fw, 2)); E[ne].pos = vadd(E[a].pos, v3(0, 0, 0.5f)); E[ne].hp = 5000;
        lance_strike(a, 0, 20); hitstop = 0;
        float boom = 5000 - E[ne].hp;
        shCap[b] = 0; E[ne].hp = 5000; E[a].hp = 5000; P.lanceState = LS_IDLE;
        b = sh_branch(SH_LAN, SH_RAIL); if (b >= 0) shCap[b] = 1;
        lance_strike(a, 0, 20); hitstop = 0;
        fprintf(f, "boutique melee : capstones Lance explosion voisin %.0f (>0), rail %d (attendu >0, 1)\n", boom, vgmStat[2]);
        shCap[b] = 0;
    }
    {   /* groupe COMPETENCES : PIECE (multiplicateurs, charges, recharge, rebond, recyclee, dividende, pluie, piegee) */
        vg_test_begin(0);
        P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_COIN;
        sh_reset();
        shOwn[SH_PIE] = (1u << 1) | (1u << 11) | (1u << 3) | (1u << 2);   /* polie, prime, reserve, recharge */
        float m0 = cp_coin_mult(0), m1 = cp_coin_mult(1), rc = cp_recharge(SK_COIN, 5);
        int mx = cp_max(SK_COIN, 2);
        shOwn[SH_PIE] = (1u << 16) | (1u << 8) | (1u << 5) | (1u << 10) | (1u << 17);   /* pluie, rebond, recyclee, dividende, piegee */
        V3 dirs[8];
        int nd = cp_coin_dirs(view_dir(), dirs);
        memset(E, 0, sizeof E);
        int a = vg_test_enemy(EN_DRONE, v3(-16, 5, 10), 10), b = vg_test_enemy(EN_DRONE, v3(-20, 5, 14), 1000);
        memset(CO, 0, sizeof CO);
        CO[0].alive = 1; CO[0].owner = myId; CO[0].life = 4; CO[0].pos = v3(-20, 5, 10);
        memset(ricHitE, 0, sizeof ricHitE);
        P.hp = 50; P.dead = 0; P.skillCh = 0; lastChain = 0; cpRicN = 0;
        ricochet(0, 100, 0);
        cp_after_ricochet();
        int rebond = E[b].hp < 1000, recycle = P.skillCh >= 0.99f, divid = P.hp >= 59.9f;
        float hb = E[b].hp;
        cp_coin_pop(E[b].pos);
        int pop = E[b].hp < hb;
        (void)a;
        fprintf(f, "boutique piece : x%.1f x%.1f recharge %.1f charges %d, pluie %d pieces, rebond=%d recyclee=%d dividende=%d piegee=%d (attendu x2.0 x1.6 recharge 4.5 charges 3, 2 pieces, 1 1 1 1)\n",
                m0, m1, rc, mx, nd, rebond, recycle, divid, pop);
    }
    {   /* groupe COMPETENCES : ALL IN ONE (recharge, dispersion, tirs en plus, salve complete, cible unique, prime, double) */
        vg_test_begin(0);
        P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_ALLIN;
        sh_reset();
        shOwn[SH_AIO] = (1u << 1) | (1u << 7) | (1u << 4) | (1u << 16) | (1u << 18) | (1u << 9) | (1u << 17);
        float rc = cp_recharge(SK_ALLIN, 20), hd = cp_half_deg(3);
        P.ammo[WP_RAIL] = ammoDef[WP_RAIL].max;
        int extra = cp_allin_extra(WP_RAIL, 2);
        cp_salve_begin(WP_RAIL, 3);
        memset(E, 0, sizeof E);
        int i = vg_test_enemy(EN_DRONE, v3(0, 10, 0), 1000);
        vgSrc = WP_RAIL; damage_enemy(i, 100, v3(0, 0, 0)); damage_enemy(i, 100, v3(0, 0, 0)); vgSrc = -1;
        float dealt = 1000 - E[i].hp;
        P.skillCh = 0;
        kill_enemy(vg_test_enemy(EN_DRONE, v3(0, 10, 5), 10), 1);
        float prime = P.skillCh;
        cp_salve_end(WP_RAIL, 3);
        fprintf(f, "boutique all in one : recharge %.0f s, demi-angle %.2f, tirs en plus %d, 2 tirs %.0f, prime %.2f charge, double %d (attendu 17 s, 2.10, 2, 299, 0.12, 3)\n",
                rc, hd, extra, dealt, prime, cpDblN);
        cpDblN = 0; cpSalveT = 0;
    }
    {   /* changement d'arme : prix, etapes gardees, graine inactive puis reactivee, choix rejoue au respawn */
        vg_test_begin(0);
        P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_COIN;
        sh_reset(); shOwn[SH_ROQ] = 1u << 1; vgCredits = 1000;
        int dup = sh_swap(0, WP_ROCKET);                  /* deja equipee : refuse */
        int a = sh_swap(1, WP_SHOTGUN), left = vgCredits, off = !sh_on(SH_ROQ), kept = (shOwn[SH_ROQ] >> 1) & 1;
        int sk = sh_swap(3, SK_ALLIN), mk = sh_swap(2, MK_FIST);
        vgCredits = 100; int poor = sh_swap(1, WP_ROCKET);
        vgCredits = 700; int back = sh_swap(1, WP_ROCKET);
        P.loadout[0] = WP_SAW; vg_clamp_loadout();        /* respawn : le choix de la partie revient */
        fprintf(f, "changement d'arme : doublon %d, achat %d (reste %d), ROQUETTES inactives %d etapes gardees %d, competence %d melee %d, sans assez %d, retour %d actives %d, respawn arme 1=%s (attendu 0, 1, 700, 1, 1, 1, 1, 0, 1 1, RAILGUN)\n",
                dup, a, left, off, kept, sk, mk, poor, back, sh_on(SH_ROQ), ammoDef[P.loadout[0]].name);
    }
    {   /* 5 etapes debloquees : Rail tendu, Tir d'elite, Pompe vive, Releve, Ricochet pur ; Piece lestee, orbitale, Dernier souffle */
        float tend[2], eli[2];
        for (int k = 0; k < 2; k++) {
            rl_t_setup(k ? 1u << 3 : 0);                  /* Rail tendu : la 2e cible en ligne +5 % */
            int e1 = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 1000), e2 = vg_test_enemy(EN_DRONE, rl_t_at(16, 0, 0), 1000);
            rl_t_fire(e1); tend[k] = 1000 - E[e2].hp;
            rl_t_setup(k ? 1u << 17 : 0);                 /* Tir d'elite : +50 % sur un elite */
            e1 = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 1000); E[e1].elite = 1;
            eli[k] = rl_t_fire(e1);
        }
        rl_t_setup(1u << 3);                              /* Rail tendu : plafonne a 5 cibles deja traversees */
        float mulMax;
        { int ee[7]; for (int k = 0; k < 7; k++) ee[k] = vg_test_enemy(EN_DRONE, rl_t_at(8 + 3.0f * k, 0, 0), 1000);
          rl_t_fire(ee[0]); mulMax = (1000 - E[ee[6]].hp) / (1000 - E[ee[5]].hp); }
        fprintf(f, "boutique rail debloque : Rail tendu 2e cible %.0f -> %.0f, 7e/6e cible x%.2f, Tir d'elite %.0f -> %.0f (attendu 100 -> 105, 1.00, 100 -> 150)\n",
                tend[0], tend[1], mulMax, eli[0], eli[1]);
        float cd[2];
        for (int k = 0; k < 2; k++) {                     /* Pompe vive : delai entre deux salves -15 % */
            vg_test_begin(0);
            P.loadout[0] = WP_SHOTGUN; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_COIN; P.weapon = WP_SHOTGUN;
            sh_reset(); shOwn[SH_SHO] = k ? 1u << 2 : 0;
            memset(E, 0, sizeof E);
            P.ammo[WP_SHOTGUN] = 3; sgFree = 0; shotStreak = 0; P.fireCd = 0;
            vgSrc = WP_SHOTGUN; fire_shotgun(); vgSrc = -1;
            cd[k] = P.fireCd;
        }
        fprintf(f, "boutique shotgun : Pompe vive delai %.3f -> %.3f s (attendu x0.85 : %.3f -> %.3f)\n", cd[0], cd[1], cd[0], cd[0] * 0.85f);
        float rl[2];
        for (int k = 0; k < 2; k++) {                     /* Releve : un estoc dans le vide recharge en 0,25 s */
            vg_test_begin(0);
            P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_COIN;
            sh_reset(); vgm_reset(); shOwn[SH_LAN] = k ? 1u << 11 : 0;
            memset(E, 0, sizeof E);
            P.pos = P.prevPos = v3(-20, 0, 41); P.yaw = 0; P.pitch = 0; P.vel = v3(0, 0, 0); P.meleeCd = 0; P.lanceState = LS_IDLE;
            lance_press(); hitstop = 0;
            lance_tick(TICK, 0);
            rl[k] = P.meleeCd + P.lanceT;                 /* recharge comptee depuis le debut de l'estoc */
        }
        fprintf(f, "boutique lance : Releve estoc dans le vide %.2f -> %.2f s (attendu 0.90 -> 0.25)\n", rl[0], rl[1]);
        float pur[2][4];
        for (int k = 0; k < 2; k++) {                     /* Ricochet pur : x1,5 puis, des le 3e rebond, +25 % cumules */
            vg_test_begin(0);
            P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_COIN;
            sh_reset(); shOwn[SH_PIE] = k ? 1u << 18 : 0;
            for (int d = 0; d < 4; d++) pur[k][d] = cp_coin_mult(d);
        }
        float seek = cp_seek_range();
        shOwn[SH_PIE] = 1u << 0;                          /* Piece lestee : portee +15 m */
        float seek2 = cp_seek_range();
        fprintf(f, "boutique piece : Ricochet pur rebonds 1-4 x%.2f x%.2f x%.2f x%.2f -> x%.2f x%.2f x%.3f x%.3f, Piece lestee portee %.0f -> %.0f (attendu x1.50 x1.50 x1.50 x1.50 -> x1.50 x1.50 x1.875 x2.344, 90 -> 105)\n",
                pur[0][0], pur[0][1], pur[0][2], pur[0][3], pur[1][0], pur[1][1], pur[1][2], pur[1][3], seek, seek2);
        {   /* Piece orbitale : 4 s autour du joueur (2,5 m), puis part vers l'ennemi proche */
            sh_reset(); shOwn[SH_PIE] = 1u << 19;
            memset(E, 0, sizeof E); memset(CO, 0, sizeof CO);
            P.pos = P.prevPos = v3(-20, 0, 40); P.yaw = 0; P.pitch = 0; P.vel = v3(0, 0, 0); P.skillCh = 2;
            int en = vg_test_enemy(EN_DRONE, rl_t_at(15, 0, 0), 1000);
            throw_coin_one(view_dir(), 1);
            float dMin = 1e9f, dMax = 0;
            for (int t = 0; t < 240; t++) {               /* 2 s d'orbite (TICK = 1/120 s) */
                coin_tick(TICK);
                V3 e0 = eye_pos(), c = CO[0].pos;
                float dd = sqrtf((c.x - e0.x) * (c.x - e0.x) + (c.z - e0.z) * (c.z - e0.z));
                if (dd < dMin) dMin = dd;
                if (dd > dMax) dMax = dd;
            }
            int orbiting = CO[0].alive && cpOrbT[0] > 0;
            for (int t = 0; t < 2000 && cpOrbT[0] > 0; t++) coin_tick(TICK);   /* l'orbite finit a 4 s : la piece part */
            float d0 = vlen(vsub(E[en].pos, CO[0].pos));
            for (int t = 0; t < 12; t++) coin_tick(TICK);
            float d1 = vlen(vsub(E[en].pos, CO[0].pos));
            fprintf(f, "boutique piece : Piece orbitale en orbite %d rayon %.1f-%.1f m, ensuite approche de l'ennemi %.1f -> %.1f m (attendu 1, 2.5-2.5, distance qui diminue)\n",
                    orbiting, dMin, dMax, d0, d1);
            memset(CO, 0, sizeof CO);
        }
        float hp0, hp1, hp2;
        vg_test_begin(0);                                 /* Dernier souffle : +15 PV si la salve est complete */
        P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_ALLIN;
        sh_reset(); shOwn[SH_AIO] = 1u << 10;
        P.ammo[WP_RAIL] = ammoDef[WP_RAIL].max; P.hp = 50; cp_salve_begin(WP_RAIL, 3); hp0 = P.hp;
        P.ammo[WP_RAIL] = ammoDef[WP_RAIL].max - 1; P.hp = 50; cp_salve_begin(WP_RAIL, 3); hp1 = P.hp;
        shOwn[SH_AIO] = 0;
        P.ammo[WP_RAIL] = ammoDef[WP_RAIL].max; P.hp = 50; cp_salve_begin(WP_RAIL, 3); hp2 = P.hp;
        fprintf(f, "boutique all in one : Dernier souffle salve complete PV %.0f, salve incomplete %.0f, sans l'etape %.0f (attendu 65, 50, 50)\n", hp0, hp1, hp2);
        cpSalveT = 0; sh_reset(); memset(E, 0, sizeof E);
    }
    {   /* capstones All in One : AIO>RAIL (2 rails par munition), AIO>ROQUETTES (souffle du Shotgun sur chaque touche) */
        int nr[2]; float nb[2];
        for (int k = 0; k < 2; k++) {
            rl_t_setup(0); P.skill = SK_ALLIN; P.skillCh = 1; P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.weapon = WP_RAIL;
            shCap[sh_branch(SH_AIO, SH_RAIL)] = (unsigned char)k;
            P.ammo[WP_RAIL] = 2; allinLast = 0; vgSrc = WP_RAIL; skill_allin(); vgSrc = -1;
            nr[k] = allinLast;
            shCap[sh_branch(SH_AIO, SH_RAIL)] = 0;
        }
        for (int k = 0; k < 2; k++) {
            rl_t_setup(0); P.skill = SK_ALLIN; P.skillCh = 1; P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.weapon = WP_RAIL;
            shCap[sh_branch(SH_AIO, SH_ROQ)] = (unsigned char)k;
            vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 100000);
            int vo = vg_test_enemy(EN_DRONE, rl_t_at(10, 1.6f, 0), 100000);
            P.ammo[WP_RAIL] = 2; shotStreak = 0; vgSrc = WP_RAIL; skill_allin(); vgSrc = -1;
            nb[k] = 100000 - E[vo].hp;
            shCap[sh_branch(SH_AIO, SH_ROQ)] = 0;
        }
        fprintf(f, "boutique all in one : capstone RAIL projectiles %d -> %d, capstone ROQUETTES voisin %.0f -> %.0f (attendu 2 -> 4, 0 -> >0)\n", nr[0], nr[1], nb[0], nb[1]);
    }
    {   /* capstone SHO>IMPULSION : le recul du Shotgun rend le double saut */
        int dj[2];
        for (int k = 0; k < 2; k++) {
            rl_t_setup(0); P.loadout[0] = WP_SHOTGUN; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_FIST; P.weapon = WP_SHOTGUN;
            shCap[sh_branch(SH_SHO, SH_IMP)] = (unsigned char)k;
            P.airJumps = 0; P.grounded = 0; P.vel = v3(0, 0, 12); P.ammo[WP_SHOTGUN] = 3; sgFree = 0;
            vgSrc = WP_SHOTGUN; fire_shotgun(); vgSrc = -1;
            dj[k] = P.airJumps;
            shCap[sh_branch(SH_SHO, SH_IMP)] = 0;
        }
        fprintf(f, "boutique shotgun : capstone IMPULSION, double saut apres un tir en mouvement %d -> %d (attendu 0 -> 1)\n", dj[0], dj[1]);
    }
    {   /* soin : plus de soin sur kill en VAGUES (hors VAGUES oui), vg_heal plafonne a 20 PV/s, rien en reaction en chaine */
        float kv, kn, b1, b2, b3, ch;
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        P.pos = v3(0, 0, 30);
        int d = vg_test_enemy(EN_DRONE, v3(0, 1.4f, 10), 1);
        P.hp = 50; damage_enemy(d, 50, v3(0, 0, 0)); kv = P.hp;
        vagOn = 0;
        d = vg_test_enemy(EN_DRONE, v3(0, 1.4f, 10), 1);
        P.hp = 50; damage_enemy(d, 50, v3(0, 0, 0)); kn = P.hp;
        vagOn = 1;
        gameTime = 100; vgHealTok = VG_HEAL_CAP; vgHealLast = gameTime;
        P.hp = 10; vg_heal(15); b1 = P.hp;                /* 15 passent (seau 20) */
        vg_heal(15); b2 = P.hp;                           /* reste 5 dans le seau */
        gameTime += 1.0f; vg_heal(15); b3 = P.hp;         /* 1 s plus tard : le seau est plein, 15 passent */
        P.hp = 10; vgDepth = 1; vg_heal(15); ch = P.hp; vgDepth = 0;
        fprintf(f, "soin : kill en VAGUES PV %.0f, hors VAGUES %.0f (attendu 50, 65)\n", kv, kn);
        fprintf(f, "soin : vg_heal rafale 15+15 -> %.0f puis %.0f, 1 s apres %.0f (attendu 25, 30, 45)\n", b1, b2, b3);
        fprintf(f, "soin : reaction en chaine %.0f (attendu 10)\n", ch);
        P.hp = 99; vgHealTok = VG_HEAL_CAP; vg_heal(15); b1 = P.hp; P.dead = 1; P.hp = 40; vgHealTok = VG_HEAL_CAP; vg_heal(15); b2 = P.hp; P.dead = 0;
        fprintf(f, "soin : plafond 100 PV %.0f, joueur mort %.0f (attendu 100, 40)\n", b1, b2);
        {   /* kill au corps a corps (poing) : plus de bonus de soin en VAGUES, toujours 15 + 15 PV hors VAGUES */
            float pk[2];
            for (int k = 0; k < 2; k++) {
                vagOn = !k; memset(E, 0, sizeof E);
                P.meleeKind = MK_FIST; P.pos = P.prevPos = v3(-20, 0, 41); P.yaw = 0; P.pitch = 0; P.vel = v3(0, 0, 0);
                P.fistStep = 0; P.swingLanded = 0; P.fistPts = 0;
                int a = vg_test_enemy(EN_DRONE, vadd(eye_pos(), vmul(view_dir(), 1.5f)), 1);
                P.hp = 50; fist_test_punch(&E[a]); pk[k] = P.hp;
            }
            vagOn = 1;
            fprintf(f, "soin : kill au poing en VAGUES PV %.0f, hors VAGUES %.0f (attendu 50, 80)\n", pk[0], pk[1]);
        }
        P.hp = 100;
    }
    {   /* capstone SHO>PIECES : la salve lance des pieces a la place des plombs */
        int nc[2]; float dg[2];
        for (int k = 0; k < 2; k++) {
            rl_t_setup(0); P.loadout[0] = WP_SHOTGUN; P.loadout[1] = WP_ROCKET; P.weapon = WP_SHOTGUN; P.skill = SK_COIN; P.skillCh = 2;
            shCap[sh_branch(SH_SHO, SH_PIE)] = (unsigned char)k;
            memset(CO, 0, sizeof CO);
            int e = vg_test_enemy(EN_DRONE, vadd(eye_pos(), vmul(view_dir(), 4)), 100000);
            P.ammo[WP_SHOTGUN] = 3; sgFree = 0;
            vgSrc = WP_SHOTGUN; fire_shotgun(); vgSrc = -1;
            nc[k] = 0; for (int i = 0; i < MAXCOIN; i++) nc[k] += CO[i].alive;
            dg[k] = 100000 - E[e].hp;
            shCap[sh_branch(SH_SHO, SH_PIE)] = 0;
        }
        fprintf(f, "boutique shotgun : capstone PIECES, pieces lancees %d -> %d, degats de plombs %.0f -> %.0f, charges de piece %.0f (attendu 0 -> 10, >0 -> 0, 2)\n", nc[0], nc[1], dg[0], dg[1], P.skillCh);
    }
    {   /* niveaux d'amelioration : prix et niveau max par profondeur, +20 % par niveau sur une etape de chaque famille */
        vg_test_begin(0);
        P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_COIN; P.weapon = WP_RAIL;
        sh_reset();
        static const int stp[4] = { 0, 5, 10, 15 };      /* RAIL : Calibre, Eclat d'impact, Sang-froid, Rail jumeau (profondeurs 1 a 4) */
        char pl[4][40];
        int mxl[4];
        for (int d = 0; d < 4; d++) {
            mxl[d] = sh_max_lvl(SH_RAIL, stp[d]);
            pl[d][0] = 0;
            for (int n = 1; n <= mxl[d]; n++) {
                shOwn[SH_RAIL] = 1u << stp[d]; shLvl[SH_RAIL][stp[d]] = (unsigned char)n;
                char one[12];
                snprintf(one, sizeof one, n > 1 ? "/%d" : "%d", sh_up_price(SH_RAIL, stp[d]));
                strcat(pl[d], one);
            }
        }
        fprintf(f, "niveaux : max %d %d %d %d, prix par palier %s | %s | %s | %s (attendu 5 4 3 2, 60/80/100/120/0 | 120/160/200/0 | 200/300/0 | 400/0)\n",
                mxl[0], mxl[1], mxl[2], mxl[3], pl[0], pl[1], pl[2], pl[3]);
        float dm[2], wd[2], vc[2], cr[2], rd[2], sz[2], ls;
        int nsh[2];
        memset(CO, 0, sizeof CO);
        for (int k = 0; k < 2; k++) {
            unsigned char lv = k ? 3 : 1;                  /* niveau 3 = +40 % du parametre (x1,4) */
            shOwn[SH_RAIL] = 1u | (1u << 2) | (1u << 13); shLvl[SH_RAIL][0] = lv; shLvl[SH_RAIL][2] = lv; shLvl[SH_RAIL][13] = lv;
            memset(E, 0, sizeof E); memset(VS, 0, sizeof VS);
            int i = vg_test_enemy(EN_DRONE, v3(0, 10, 0), 1000);
            vgSrc = WP_RAIL; damage_enemy(i, 100, v3(0, 0, 0)); vgSrc = -1;
            dm[k] = 1000 - E[i].hp;                        /* Calibre : +10 % de degats */
            wd[k] = vg_rail_wide(W_RAIL);                  /* Rail large : +15 % de largeur */
            int j = vg_test_enemy(EN_DRONE, v3(0, 10, 4), 5);
            vgSrc = WP_RAIL; damage_enemy(j, 9999, v3(0, 0, 0)); vgSrc = -1;
            nsh[k] = 0; for (int q = 0; q < MAXVS; q++) nsh[k] += VS[q].on;   /* Eclats : 3 eclats chercheurs (3 x 1,4 = 4) */
            shOwn[SH_RAIL] = 0;
            shOwn[SH_ROQ] = 1u << RQ_RAYON; shLvl[SH_ROQ][RQ_RAYON] = lv;
            Proj *pr = spawn_proj(PR_ROCKET, v3(0, 0, 0), v3(0, 0, 0));
            rd[k] = roq_radius(pr) - ROCKET_RADIUS;        /* Rayon : +0,4 m */
            memset(PR, 0, sizeof PR); shOwn[SH_ROQ] = 0;
            P.loadout[0] = WP_SHOTGUN;
            shOwn[SH_SHO] = 1u << 3; shLvl[SH_SHO][3] = lv;
            sz[k] = st(SH_SHO, STAT_TAI, 1.0f);            /* Gros plomb : +15 % */
            shOwn[SH_SHO] = 0; P.loadout[0] = WP_RAIL;
            shOwn[SH_LAN] = (1u << 1) | (1u << 4); shLvl[SH_LAN][1] = lv; shLvl[SH_LAN][4] = (unsigned char)(k ? 5 : 1);
            vc[k] = vgm_vc(1.0f);                          /* Velocite : +10 % */
            ls = st(SH_LAN, STAT_VOL, 0) * 100;            /* Seve de lance : 2 % de vol de vie (niveau 5 : 3,6) */
            shOwn[SH_LAN] = 0;
            shOwn[SH_PIE] = 1u << 2; shLvl[SH_PIE][2] = lv;
            cr[k] = cp_recharge(SK_COIN, 10.0f);           /* Recharge : -10 % */
            shOwn[SH_PIE] = 0;
        }
        fprintf(f, "niveaux : Calibre %.0f -> %.0f, Rail large x%.3f -> x%.3f, Eclats %d -> %d, Rayon +%.2f -> +%.2f m, Gros plomb x%.3f -> x%.3f, Velocite x%.3f -> x%.3f, Recharge %.2f -> %.2f, vol de vie niv 5 %.1f (attendu 110 -> 114, 1.150 -> 1.210, 3 -> 4, +0.40 -> +0.56, 1.150 -> 1.210, 1.100 -> 1.140, 9.00 -> 8.60, 3.6)\n",
                dm[0], dm[1], wd[0], wd[1], nsh[0], nsh[1], rd[0], rd[1], sz[0], sz[1], vc[0], vc[1], cr[0], cr[1], ls);
        /* offres d'amelioration : une etape possedee et pas au max revient avec son prix et son niveau vise */
        int found[3] = { 0, 0, 0 }, price[3] = { 0, 0, 0 }, slot = -1, nonScal = 0, tgt = 0;
        sh_reset();
        vgCredits = 5000;
        for (int c = 0; c < 3; c++) {
            memset(shLvl, 0, sizeof shLvl);
            shOwn[SH_RAIL] = c == 2 ? 0 : 1u;
            shLvl[SH_RAIL][0] = (unsigned char)(c == 0 ? 1 : 5);   /* niveau 1 : offre a 60 ; niveau max : aucune offre */
            for (int t = 0; t < 120; t++) {
                sh_roll();
                for (int k = 0; k < shNOff; k++)
                    if (shOff[k].type == SO_UP && shOff[k].seed == SH_RAIL && shOff[k].step == 0) {
                        found[c] = 1; price[c] = shOff[k].price;
                        if (c == 0 && slot < 0) { slot = k; tgt = shOff[k].br; }
                    }
                if (c == 0 && slot >= 0) break;
            }
        }
        int c0 = vgCredits, lv0 = 0, lv1 = 0, bought = 0;
        memset(shLvl, 0, sizeof shLvl); shOwn[SH_RAIL] = 1u; shLvl[SH_RAIL][0] = 1;
        for (int t = 0; t < 120 && !bought; t++) {
            sh_roll();
            for (int k = 0; k < shNOff; k++)
                if (shOff[k].type == SO_UP && shOff[k].step == 0 && shOff[k].seed == SH_RAIL) { lv0 = sh_lvl(SH_RAIL, 0); bought = up_buy(k); break; }
        }
        lv1 = sh_lvl(SH_RAIL, 0);
        shOwn[SH_RAIL] = 1u << 11; shLvl[SH_RAIL][11] = 1;       /* Double : pas de parametre numerique, jamais en amelioration */
        for (int t = 0; t < 120; t++) {
            sh_roll();
            for (int k = 0; k < shNOff; k++) nonScal += shOff[k].type == SO_UP && shOff[k].seed == SH_RAIL;
        }
        fprintf(f, "niveaux : offre NIV 2 trouvee %d a %d credits (cible niv %d), au niveau max %d, etape absente %d, achat %d niv %d -> %d depense %d, Double (sans parametre) proposee %d fois (attendu 1 a 60 (cible 2), 0, 0, 1 niv 1 -> 2 depense 60, 0)\n",
                found[0], price[0], tgt, found[1], found[2], bought, lv0, lv1, c0 - vgCredits, nonScal);
        /* niveau d'une fusion (SALVE D'ARTILLERIE, ROQ-SHO) : min des deux branches (somme des 4 etapes + capstone) / 5 */
        int fu = -1;
        for (int q = 0; q < SH_NFU; q++) if (shFu[q].a == SH_ROQ && shFu[q].b == SH_SHO) fu = q;
        int b1 = sh_branch(SH_ROQ, SH_SHO), b2 = sh_branch(SH_SHO, SH_ROQ);
        static const int FL[4][4] = { { 1, 1, 1, 1 }, { 3, 3, 2, 2 }, { 5, 4, 3, 2 }, { 5, 4, 3, 2 } };
        int fl[4]; float fs[4];
        for (int c = 0; c < 4; c++) {
            memset(shOwn, 0, sizeof shOwn); memset(shLvl, 0, sizeof shLvl); memset(shCap, 0, sizeof shCap);
            int br[2] = { b1, b2 };
            for (int q = 0; q < 2; q++)
                for (int j = 0; j < 4; j++) {
                    int st = j * 5 + shBr[br[q]].s[j];
                    shOwn[shBr[br[q]].seed] |= 1u << st;
                    shLvl[shBr[br[q]].seed][st] = (unsigned char)(c == 3 && q == 1 ? 1 : FL[c][j]);
                    shCap[br[q]] = 1;
                }
            shFusion = fu;
            fl[c] = sh_fusion_lvl(); fs[c] = sh_fusion_scale();
        }
        shFusion = -1; memset(shOwn, 0, sizeof shOwn); memset(shLvl, 0, sizeof shLvl); memset(shCap, 0, sizeof shCap);
        fprintf(f, "niveaux : fusion niv %d %d %d %d, effet x%.2f x%.2f x%.2f x%.2f (attendu 1 2 3 1, x1.00 x1.25 x1.50 x1.00 : niveaux 1, 3-3-2-2, max, une branche au minimum)\n",
                fl[0], fl[1], fl[2], fl[3], fs[0], fs[1], fs[2], fs[3]);
    }
    {   /* FORMES FUSIONNEES : jauge, activation, munitions mises de cote, fin (ARTILLERIE ORBITALE), perte d'une arme, hors VAGUES */
        hy_t_setup(SH_RAIL, SH_ROQ, WP_RAIL);
        P.ammo[WP_RAIL] = 1; P.ammo[WP_ROCKET] = 3; P.ammoGauge[WP_RAIL] = 40; P.ammoGauge[WP_ROCKET] = 25;
        int e1 = vg_test_enemy(EN_DRONE, rl_t_at(10, 0, 0), 100000), e2 = vg_test_enemy(EN_DRONE, rl_t_at(20, 0, 0), 100000);
        int e3 = vg_test_enemy(EN_DRONE, rl_t_at(14, 12, 0), 100000);
        hy_tick(TICK);
        vgSrc = WP_RAIL; damage_enemy(e3, 100, v3(0, 0, 0)); vgSrc = WP_ROCKET; damage_enemy(e3, 50, v3(0, 0, 0));
        vgSrc = WP_SHOTGUN; damage_enemy(e3, 70, v3(0, 0, 0)); vgSrc = -1;   /* le Shotgun n'est pas de la paire : rien */
        float g1 = hyGauge;
        hyGauge = HY_GAUGE_MAX - 10; vgSrc = WP_RAIL; damage_enemy(e3, 100, v3(0, 0, 0)); vgSrc = -1;
        int full = hyGauge >= HY_GAUGE_MAX && hyFull, msg = strstr(popText, "FUSION PRETE") != NULL;
        hyGauge = 400; actP[A_FUSION] = 1; hy_tick(TICK); actP[A_FUSION] = 0;
        int refused = !hyOn;
        int on = hy_t_go();
        int pegged = P.ammo[WP_RAIL] == wp_max(WP_RAIL) && P.ammo[WP_ROCKET] == wp_max(WP_ROCKET);
        float gAfter = hyGauge;
        float h1 = E[e1].hp, h2 = E[e2].hp;
        P.fireCd = 0; vgSrc = P.weapon; fire_weapon(); vgSrc = -1;
        float d1 = h1 - E[e1].hp, d2 = h2 - E[e2].hp;
        fprintf(f, "hybride : jauge %.0f apres rail 100 + roquette 50 + plombs 70 (attendu 150), pleine %d, message %d (attendu 1), refusee a 400 : %d (attendu 1)\n",
                g1, full, msg, refused);
        fprintf(f, "ARTILLERIE ORBITALE : active %d, jauge apres %.0f, munitions pleines %d (attendu 1, 0, 1)\n", on, gAfter, pegged);
        fprintf(f, "ARTILLERIE ORBITALE : degats cible 1 (10 m) %.0f, cible 2 (20 m) %.0f, souffles %d (attendu >100 chacune, 3 = 2 cibles + le bout)\n",
                d1, d2, hyStat[2]);
        float t0 = hyT;
        for (int i = 0; i < 240; i++) hy_tick(TICK);      /* 2 s */
        int still = hyOn && fabsf(t0 - hyT - 2.0f) < 0.05f;
        for (int i = 0; i < 13 * 120 + 5; i++) hy_tick(TICK);   /* le reste de la duree */
        fprintf(f, "ARTILLERIE ORBITALE : toujours active apres 2 s %d, finie apres 15 s %d, munitions rendues rail %d (attendu 1) roquette %d (attendu 3), jauges %.0f %.0f (attendu 40 25)\n",
                still, !hyOn, P.ammo[WP_RAIL], P.ammo[WP_ROCKET], P.ammoGauge[WP_RAIL], P.ammoGauge[WP_ROCKET]);
        hy_t_go();                                        /* perte d'une arme : CHANGER D'ARME remplace la roquette */
        P.loadout[1] = WP_SHOTGUN;
        hy_tick(TICK);
        int lost = !hyOn;
        int noStart = !hy_t_go();                         /* la roquette n'est plus equipee : la touche ne fait rien */
        P.loadout[1] = WP_ROCKET;
        int back = hy_t_go();
        P.dead = 1; hy_tick(TICK); P.dead = 0;
        int dead = !hyOn;
        hy_t_go();
        vagOn = 0; int off = hy_fire(); hy_tick(TICK); int offEnd = !hyOn; vagOn = 1;   /* hors VAGUES : l'hybride ne tire pas et prend fin */
        fprintf(f, "hybride : arme perdue pendant la forme -> finie %d, touche sans la 2e arme %d, a la mort -> finie %d, active a nouveau %d, hors VAGUES tir %d et fin %d (attendu 1 1 1 1 0 1)\n",
                lost, noStart, dead, back, off, offEnd);
        shFusion = -1; hyGauge = 0;
        hy_tick(TICK);
        vgSrc = WP_RAIL; hy_charge(100); vgSrc = -1;
        float gSans = hyGauge;
        int sans = hy_t_go();
        fprintf(f, "hybride : sans fusion achetee, jauge %.0f et touche %d (attendu 0 0)\n", gSans, sans);
    }
    {   /* CANON SCIE : gerbe de 8 rails fins qui traversent, heritages des deux arbres, recul du Shotgun */
        float dm[3], d2[2];
        int rays[3], ex = 0;
        for (int k = 0; k < 3; k++) {
            hy_t_setup(SH_RAIL, SH_SHO, WP_SHOTGUN);
            if (k == 1) { shOwn[SH_RAIL] = 1u << 0; shOwn[SH_SHO] = 1u << 16; }   /* Calibre (+10 %) ; Plomb lourd (+30 %) */
            if (k == 2) {
                shOwn[SH_SHO] = 1u << st_find_step(SH_SHO, STAT_NBP);   /* Plombs en plus (+2) */
                SgStats sgs; sg_stats(&sgs, SG_HALF_ANGLE, SG_RANGE);
                ex = sgs.extra;
            }
            memset(E, 0, sizeof E);
            int e1 = vg_test_enemy(EN_DRONE, rl_t_at(8, 0, 0), 100000), e2 = vg_test_enemy(EN_DRONE, rl_t_at(12, 0, 0), 100000);
            hy_t_go();
            P.vel = v3(0, 0, -12); P.grounded = 1; P.pushT = 0; P.fireCd = 0;
            hyStat[1] = 0; sgCounter = 7;                 /* meme motif de gerbe a chaque passe : les touches a la tete sont les memes */
            float h1 = E[e1].hp, h2 = E[e2].hp;
            vgSrc = P.weapon; fire_weapon(); vgSrc = -1;
            dm[k] = h1 - E[e1].hp; d2[k == 0 ? 0 : 1] = h2 - E[e2].hp; rays[k] = hyStat[1];
            if (k == 0) fprintf(f, "CANON SCIE : rails %d, recul du Shotgun %.1f m/s (pushT %.2f), delai %.2f s (attendu 8, 12.0, 0.10, 0.90 ou un peu moins : la chaine de tirs accelere)\n",
                                rays[0], sgLastPush, P.pushT, P.fireCd);
        }
        fprintf(f, "CANON SCIE : degats cible 1 = %.0f, cible 2 derriere = %.0f (traversee) (attendu 400 a 600 chacune : 8 rails a 50, tete a x1.5)\n", dm[0], d2[0]);
        fprintf(f, "CANON SCIE : avec Calibre rail +10 %% et Plomb lourd +30 %% : %.0f (x%.2f, attendu x1.43) ; plombs en plus : %d rails (attendu %d)\n",
                dm[1], dm[0] > 0 ? dm[1] / dm[0] : 0.0f, rays[2], 8 + ex);
        memset(E, 0, sizeof E);
        hy_t_setup(SH_RAIL, SH_SHO, WP_RAIL);
        int en = vg_test_enemy(EN_DRONE, rl_t_at(8, 0, 0), 100000);
        hy_t_go(); P.fireCd = 0; hyStat[1] = 0;
        vgSrc = P.weapon; fire_weapon(); vgSrc = -1;
        fprintf(f, "CANON SCIE : tire aussi avec le Railgun en main : %d rails, ennemi touche %d (attendu 8, 1)\n", hyStat[1], E[en].hp < 100000);
    }
    {   /* OBUSIER : gerbe de 6 mini-roquettes, souffle de 2,5 m, recul vers le haut */
        float dm = 0, dNear = 0, dFar = 0;
        int nR = 0;
        hy_t_setup(SH_ROQ, SH_SHO, WP_ROCKET);
        memset(E, 0, sizeof E);
        int e1 = vg_test_enemy(EN_DRONE, rl_t_at(15, 0, 0), 100000);
        int e2 = vg_test_enemy(EN_DRONE, rl_t_at(15, 8, 0), 100000);   /* loin du souffle de 2,5 m */
        hy_t_go();
        P.vel = v3(0, 0, 0); P.grounded = 1; P.fireCd = 0;
        vgSrc = P.weapon; fire_weapon(); vgSrc = -1;
        float vy = P.vel.y; int air = !P.grounded;
        for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].kind == PR_ROCKET && PR[i].roqGen == 1) nR++;
        float rad = 0;
        for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].kind == PR_ROCKET) { Proj *pp = &PR[i]; rad = roq_radius(pp); break; }
        for (int i = 0; i < 400; i++) { proj_tick(TICK); hitstop = 0; }
        dNear = 100000 - E[e1].hp; dFar = 100000 - E[e2].hp; dm = dNear;
        fprintf(f, "OBUSIER : mini-roquettes %d, souffle %.2f m, delai %.2f s, recul vy %.1f en l'air %d (attendu 6, 2.48, 1.10, >=10, 1)\n",
                nR, rad, P.fireCd, vy, air);
        fprintf(f, "OBUSIER : degats cible dans la gerbe %.0f, cible a 8 m de cote %.0f (attendu >0, 0)\n", dm, dFar);
        int zn = 0;
        shOwn[SH_ROQ] = 1u << RQ_RAYON;                   /* RQ_* = rang de l'etape dans le pool (Rayon : +0,4 m) */
        zn = fabsf(hy_zone_roq() - (ROCKET_RADIUS + 0.4f) / ROCKET_RADIUS) < 0.01f;
        shOwn[SH_ROQ] = 0;
        fprintf(f, "OBUSIER / ARTILLERIE : zone heritee du Rayon des Roquettes %d (attendu 1)\n", zn);
    }
    {   /* niveaux : Perce-mur monte des deux cotes (epaisseur 1 m x echelle ET degats 70 % x echelle) */
        float pw[2];
        for (int k = 0; k < 2; k++) {
            rl_t_setup(1u << 18); shLvl[SH_RAIL][18] = (unsigned char)(k ? 2 : 1);
            V3 d = view_dir(); int ax = fabsf(d.x) > 0.5f;
            int nb0 = nb;
            Box *bx = &boxes[nb++];
            memset(bx, 0, sizeof *bx);
            V3 c = rl_t_at(6, 0, 0), h = ax ? v3(0.55f, 4, 4) : v3(4, 4, 0.55f);   /* mur de 1,1 m */
            bx->mn = vsub(c, h); bx->mx = vadd(c, h);
            int e = vg_test_enemy(EN_DRONE, rl_t_at(12, 0, 0), 1000);
            pw[k] = rl_t_fire(e);
            nb = nb0;
        }
        fprintf(f, "boutique rail : Perce-mur niveau 1 -> 2 sur un mur de 1,1 m : degats %.0f -> %.0f (attendu 0 -> 84 : epaisseur 1,2 m et 84 %% des degats)\n", pw[0], pw[1]);
    }
    {   /* capstone RAIL>SHO (Rail d'ouverture) : la touche du rail ouvre la cible 3 s, plombs x1,3 sans perte a distance */
        float dm[3], op[4] = { 0, 0, 0, 0 }, closed = 1, mm[3] = { 0, 0, 0 };
        int side = 0;
        for (int k = 0; k < 4; k++) {                     /* 0 : sans capstone ; 1 : avec ; 2 : avec, 3,3 s apres la touche du rail ; 3 : avec, sans le Shotgun equipe */
            rl_t_setup(0); P.loadout[0] = WP_RAIL; P.loadout[1] = k == 3 ? WP_ROCKET : WP_SHOTGUN; P.weapon = WP_RAIL;
            shCap[sh_branch(SH_RAIL, SH_SHO)] = (unsigned char)(k > 0);
            memset(E, 0, sizeof E); memset(rlOpenT, 0, sizeof rlOpenT);
            int e = vg_test_enemy(EN_DRONE, rl_t_at(20.9f, 0, 0), 100000), o2 = vg_test_enemy(EN_DRONE, rl_t_at(20.9f, 6, 0), 100000);   /* surface a 20 m : perte x0,65 */
            rl_t_fire(e);                                 /* le rail traverse e (o2 n'est pas sur la ligne) */
            op[k] = rlOpenT[e];
            if (k == 3) { shCap[sh_branch(SH_RAIL, SH_SHO)] = 0; continue; }
            if (k == 1) { side = rlOpenT[o2] > 0; mm[0] = vg_rail_open_mul(e, 0.30f); mm[1] = vg_rail_open_mul(o2, 0.30f); mm[2] = vg_rail_open_mul(e, 1.4f); }
            if (k == 2) { for (int t = 0; t < 400; t++) vg_rail_tick(TICK); closed = rlOpenT[e]; }
            float h0 = E[e].hp;
            P.ammo[WP_SHOTGUN] = 3; sgFree = 0; sgCounter = 100u;   /* meme graine : memes plombs a chaque essai */
            vgSrc = WP_SHOTGUN; fire_shotgun(); vgSrc = -1;
            dm[k] = h0 - E[e].hp;
            shCap[sh_branch(SH_RAIL, SH_SHO)] = 0;
        }
        fprintf(f, "capstone RAIL>SHO : ouverture apres le rail %.1f -> %.1f s, cible hors ligne ouverte %d, 3,3 s plus tard %.1f, sans le Shotgun equipe %.1f ; plombs a 20 m %.0f -> %.0f -> %.0f (rapport %.2f) ; "
                   "multiplicateur pour une perte x0,30 / cible fermee / bonus de proximite x1,4 : %.2f %.2f %.2f "
                   "(attendu 0.0 -> 3.0 s, 0, 0.0, 0.0 ; rapport ~2.0 : x1,3 au lieu de x0,65, puis refermee (meme degats qu'au depart) ; 1.30 0.30 1.82)\n",
                op[0], op[1], side, closed, op[3], dm[0], dm[1], dm[2], dm[0] > 0 ? dm[1] / dm[0] : 0, mm[0], mm[1], mm[2]);
    }
    {   /* capstone SHO>AIO (Shotgun total) : l'All in One au Shotgun tire 6 salves, sans depenser plus de munition */
        int sh[3], last[3], amm[3], roc[3];
        float gj[3];
        for (int k = 0; k < 3; k++) {                     /* 0 : sans capstone (3 munitions) ; 1 : avec (3) ; 2 : avec (2 munitions) */
            rl_t_setup(0); P.loadout[0] = WP_SHOTGUN; P.loadout[1] = WP_ROCKET; P.weapon = WP_SHOTGUN; P.skill = SK_ALLIN; P.skillCh = 1;
            P.switchT = 0;
            shCap[sh_branch(SH_SHO, SH_AIO)] = (unsigned char)(k > 0);
            shOwn[SH_SHO] = 1u << 12;                     /* Double canon : 15 % du cout de jauge rendus par salve payee */
            memset(E, 0, sizeof E);
            P.ammo[WP_SHOTGUN] = k == 2 ? 2 : 3; P.ammo[WP_ROCKET] = 4; P.ammoGauge[WP_SHOTGUN] = 0; sgFree = 0; allinLast = 0;
            int s0 = sgShots;
            vgSrc = WP_SHOTGUN; skill_allin(); vgSrc = -1;
            sh[k] = sgShots - s0; last[k] = allinLast; amm[k] = P.ammo[WP_SHOTGUN]; roc[k] = P.ammo[WP_ROCKET]; gj[k] = P.ammoGauge[WP_SHOTGUN];
        }
        int ex[3] = { vgsg_allin_extra(2), vgsg_allin_extra(6), vgsg_allin_extra(8) };   /* le capstone reste achete : jamais moins de 6 salves, jamais de reduction */
        P.skill = SK_COIN;                                /* All in One pas equipe : le capstone ne vaut plus */
        int exOff = vgsg_allin_extra(2);
        shCap[sh_branch(SH_SHO, SH_AIO)] = 0; shOwn[SH_SHO] = 0;
        fprintf(f, "capstone SHO>AIO : salves tirees %d -> %d (2 munitions : %d), allinLast %d -> %d (%d), munition du Shotgun apres %d %d %d, roquettes %d %d %d, "
                   "jauge rendue par Double canon %.0f %.0f %.0f, salves ajoutees pour 2 / 6 / 8 : %d %d %d, sans l'All in One equipe %d "
                   "(attendu 3 -> 6 (6), 3 -> 6 (6), 0 0 0, 4 4 4, 45 45 30, 4 0 0, 0)\n",
                sh[0], sh[1], sh[2], last[0], last[1], last[2], amm[0], amm[1], amm[2], roc[0], roc[1], roc[2], gj[0], gj[1], gj[2], ex[0], ex[1], ex[2], exOff);
    }
    {   /* capstone PIE>SHO (Piece en plombs) : une piece qui finit (ricochet d'un tir, roquette, fin de vie) explose en plombs */
        float d0, d1, d2, d3, d4, d5, d6, r[2], pv[2];
        int e, fan, gone;
        e = cap_t_coin_setup(0, 0); cap_t_coin_at(rl_t_at(5, 0, 0), 4); cp_coin_pop(CO[0].pos); d0 = 100000 - E[e].hp;           /* sans capstone */
        e = cap_t_coin_setup(1, 0); cap_t_coin_at(rl_t_at(5, 0, 0), 4); cp_coin_pop(CO[0].pos); d1 = 100000 - E[e].hp;           /* avec : 10 plombs a 20 */
        e = cap_t_coin_setup(1, 1u << 16); cap_t_coin_at(rl_t_at(5, 0, 0), 4); cp_coin_pop(CO[0].pos); d2 = 100000 - E[e].hp;   /* + Plomb lourd (+30 %) */
        e = cap_t_coin_setup(1, 0); cap_t_coin_at(rl_t_at(5, 0, 0), 0.001f); coin_tick(TICK); d3 = 100000 - E[e].hp;            /* fin de vie */
        gone = !CO[0].alive;
        e = cap_t_coin_setup(1, 0); cap_t_coin_at(rl_t_at(5, 0, 0), 4); cp_coin_rocket(CO[0].pos); d4 = 100000 - E[e].hp;        /* roquette */
        e = cap_t_coin_setup(1, 0); cap_t_coin_at(rl_t_at(5, 0, 0), 4); vagOn = 0; cp_coin_pop(CO[0].pos); vagOn = 1; d5 = 100000 - E[e].hp;   /* hors VAGUES */
        e = cap_t_coin_setup(1, 0); cap_t_coin_at(rl_t_at(5, 0, 0), 4); P.loadout[1] = WP_ROCKET; cp_coin_pop(CO[0].pos); d6 = 100000 - E[e].hp;   /* sans le Shotgun equipe */
        for (int k = 0; k < 2; k++) {                     /* ricochet d'un tir de rail sur la piece : le meme ricochet, plus les plombs */
            e = cap_t_coin_setup(k, 0); cap_t_coin_at(rl_t_at(5, 0, 0), 4);
            r[k] = rl_t_fire(e);
        }
        for (int k = 0; k < 2; k++) {                     /* un PLOMB sur la piece : son eclat habituel, sans plombs en plus */
            cap_t_coin_setup(k, 0); memset(E, 0, sizeof E);
            int ne = vg_test_enemy(EN_DRONE, rl_t_at(6, 2, 0), 100000);
            cap_t_coin_at(rl_t_at(6, 0, 0), 4);
            P.ammo[WP_SHOTGUN] = 3; sgFree = 0; sgCounter = 100u;
            vgSrc = WP_SHOTGUN; fire_shotgun(); vgSrc = -1;
            pv[k] = 100000 - E[ne].hp;
        }
        cap_t_coin_setup(1, 0); memset(E, 0, sizeof E);   /* personne en vue : eventail devant soi */
        int t0 = trNext; cp_coin_pellets(rl_t_at(5, 0, 0)); fan = (trNext - t0 + MAXTR) % MAXTR;
        shCap[sh_branch(SH_PIE, SH_SHO)] = 0; memset(CO, 0, sizeof CO);
        fprintf(f, "capstone PIE>SHO : plombs de la piece sans -> avec le capstone %.0f -> %.0f, avec Plomb lourd %.0f, fin de vie %.0f (piece disparue %d), roquette %.0f, "
                   "hors VAGUES %.0f, sans le Shotgun equipe %.0f, ricochet du rail +%.0f, eventail sans cible %d traceurs (attendu 0 -> 200, 260, 200 (1), 200, 0, 0, +200, 10)\n",
                d0, d1, d2, d3, gone, d4, d5, d6, r[1] - r[0], fan);
        fprintf(f, "capstone PIE>SHO : piece eclatee par un plomb, degats au voisin %.1f -> %.1f (attendu identiques : l'eclat habituel, pas de plombs en plus)\n", pv[0], pv[1]);
    }
    {   /* capstone AIO>SHO (Salve de Shotgun) : pendant la salve au Shotgun, cone x3 et +50 % de plombs */
        int ntr[6];
        float mx[6];
        for (int k = 0; k < 6; k++) {                     /* 0 : salve sans capstone ; 1 : salve avec ; 2 : tir normal avec ; 3 : salve avec Plombs en plus (+2) ; 4 : salve avec, Railgun en main ; 5 : salve avec, Shotgun pas equipe */
            rl_t_setup(0); P.loadout[0] = k == 5 ? WP_RAIL : WP_SHOTGUN; P.loadout[1] = k == 5 ? WP_ROCKET : WP_RAIL; P.weapon = k == 4 ? WP_RAIL : WP_SHOTGUN; P.skill = SK_ALLIN;
            shCap[sh_branch(SH_AIO, SH_SHO)] = (unsigned char)(k > 0);
            if (k == 3) shOwn[SH_SHO] = 1u << 11;         /* Plombs en plus : +2 plombs */
            memset(E, 0, sizeof E);
            P.ammo[WP_SHOTGUN] = 3; sgFree = 0; sgCounter = 100u;
            allinActive = k != 2;
            int t0 = trNext;
            vgSrc = WP_SHOTGUN; fire_shotgun(); vgSrc = -1;
            allinActive = 0;
            ntr[k] = (trNext - t0 + MAXTR) % MAXTR;
            mx[k] = 0;
            V3 ey = eye_pos(), fw = view_dir();
            for (int i = 0; i < ntr[k]; i++) {            /* angle de chaque plomb avec la visee : le dernier point du traceur est sur sa trajectoire */
                float a = acosf(clampf(vdot(vnorm(vsub(TR[(t0 + i) % MAXTR].b, ey)), fw), -1, 1));
                if (a > mx[k]) mx[k] = a;
            }
            shCap[sh_branch(SH_AIO, SH_SHO)] = 0; shOwn[SH_SHO] = 0;
        }
        fprintf(f, "capstone AIO>SHO : plombs %d -> %d (tir normal %d, avec Plombs en plus %d, Railgun en main %d, Shotgun pas equipe %d), angle maximal %.1f -> %.1f deg "
                   "(rapport %.2f, tir normal %.1f, Railgun %.1f) (attendu 10 -> 15 (10, 18, 10, 10), au plus 4.5 -> 13.5 deg, rapport 3.00, au plus 4.5, au plus 4.5)\n",
                ntr[0], ntr[1], ntr[2], ntr[3], ntr[4], ntr[5], mx[0] * 180 / PI, mx[1] * 180 / PI, mx[0] > 0 ? mx[1] / mx[0] : 0, mx[2] * 180 / PI, mx[4] * 180 / PI);
    }
    {   /* SAC DE STATS (boutique_stats.c) : neutre hors VAGUES, graine non equipee = base, chaque operation, conditions, groupe, niveaux, achat, changement d'arme */
        enum { SHO_POING = 0, SHO_CHEVRO = 1, SHO_GROS = 3, SHO_GERBE = 5, SHO_CHERCHE = 7, SHO_SERRE = 9, SHO_PLUS = 11, SHO_TENDUE = 18, SHO_TRAQUEUR = 19 };
        int tableOk = st_find_step(SH_SHO, STAT_TAI) == SHO_GROS && st_find_step(SH_SHO, STAT_NBP) == SHO_PLUS &&
                      st_find_step(SH_SHO, STAT_CONE) == SHO_GERBE && st_find_step(SH_SHO, STAT_PFAR) == SHO_SERRE &&
                      st_find_step(SH_SHO, STAT_POR) == SHO_TENDUE && st_find_step(SH_SHO, STAT_SUI) == SHO_CHERCHE &&
                      hy_t_step(SH_SHO, BK_STAT, -1) == SHO_POING;   /* la table generee suit le pool de l'arbre */
        vg_test_begin(0);
        P.loadout[0] = WP_SHOTGUN; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_COIN; P.weapon = WP_SHOTGUN;
        sh_reset();
        StatCtx c3 = { 0 }, c8 = { 0 };
        c3.dist = 3; c8.dist = 8;
        {   /* hors VAGUES : st() rend la base meme avec toutes les etapes du Shotgun ; Shotgun non equipe : la base aussi ; en VAGUES equipe : la valeur */
            shOwn[SH_SHO] = (1u << 20) - 1;
            vagOn = 0;
            float n1 = st(SH_SHO, STAT_TAI, 1.0f), n2 = st(SH_SHO, STAT_NBP, 10.0f), n3 = st_ctx(SH_SHO, STAT_DEG, 100.0f, &c3), n4 = st(SH_SHO, STAT_CD, 0.95f);
            vagOn = 1;
            float v1 = st(SH_SHO, STAT_TAI, 1.0f);
            P.loadout[0] = WP_RAIL;
            float e1 = st(SH_SHO, STAT_TAI, 1.0f), e2 = st_ctx(SH_SHO, STAT_DEG, 100.0f, &c3);
            P.loadout[0] = WP_SHOTGUN;
            fprintf(f, "sac de stats : table suit le pool %d, hors VAGUES (20 etapes possedees) TAI %.2f NBP %.0f DEG a 3 m %.0f CD %.2f, en VAGUES TAI %.2f, Shotgun non equipe TAI %.2f DEG %.0f "
                       "(attendu 1, 1.00 10 100 0.95, 1.15, 1.00 100)\n", tableOk, n1, n2, n3, n4, v1, e1, e2);
            vagOn = 0;                                    /* ce que lit fire_shotgun : exactement les valeurs du PvP, meme avec les 20 etapes possedees */
            SgStats s0; sg_stats(&s0, SG_HALF_ANGLE, SG_RANGE);
            int neutral = s0.half == SG_HALF_ANGLE && s0.extra == 0 && s0.range == SG_RANGE && s0.size == 1 && s0.fallA == SG_NEAR && s0.fallB == SG_FAR &&
                          s0.fallMin == SG_FAR_MULT && s0.seek == 0 && s0.cd == 1 && s0.cost == 1 && !s0.capNear;
            float loss20 = sg_fall(&s0, 20);
            vagOn = 1;
            fprintf(f, "sac de stats : stats du Shotgun hors VAGUES (20 etapes possedees) identiques au PvP %d, perte de degats a 20 m x%.4f (attendu 1, x0.6500)\n", neutral, loss20);
        }
        {   /* une etape de chaque operation : mul (Gros plomb), add (Plombs en plus), max (Pompe tendue), une etape qui modifie trois stats (Cone serre), add puis max */
            shOwn[SH_SHO] = 1u << SHO_GROS;
            float oMul = st(SH_SHO, STAT_TAI, 1.0f);
            shOwn[SH_SHO] = 1u << SHO_PLUS;
            float oAdd = st(SH_SHO, STAT_NBP, (float)SG_PELLETS);
            shOwn[SH_SHO] = 1u << SHO_TENDUE;
            float oMax = st(SH_SHO, STAT_PNEAR, SG_NEAR), oPor = st(SH_SHO, STAT_POR, SG_RANGE), oFar = st(SH_SHO, STAT_PFAR, SG_FAR);
            shOwn[SH_SHO] = 1u << SHO_SERRE;
            float cone = st(SH_SHO, STAT_CONE, SG_HALF_ANGLE) * 180 / PI, near2 = st(SH_SHO, STAT_PNEAR, SG_NEAR), far2 = st(SH_SHO, STAT_PFAR, SG_FAR);
            shOwn[SH_SHO] = (1u << SHO_SERRE) | (1u << SHO_TENDUE);
            float oBoth = st(SH_SHO, STAT_PNEAR, SG_NEAR);
            fprintf(f, "sac de stats : mul TAI x%.2f, add NBP %.0f, max PNEAR %.0f (POR %.0f, PFAR %.0f), Cone serre (3 stats) cone %.2f deg PNEAR %.0f PFAR %.0f, add puis max %.0f "
                       "(attendu 1.15, 12, 20 (80, 50), 3.50 15 35, 20)\n", oMul, oAdd, oMax, oPor, oFar, cone, near2, far2, oBoth);
        }
        {   /* conditions et empilements, sur des modificateurs construits a la main : cibles traversees (compose, lineaire, plafond), tete, en l'air, elite, sans contexte */
            StAct pier[1] = { st_act(SOP_MUL, COND_PIERCED, 1.15f, 0, 0, 0, 0, 1.0f) }, tend[1] = { st_act(SOP_MULL, COND_PIERCED, 1.05f, 5, 0, 0, 0, 1.0f) };
            StAct hd[1] = { st_act(SOP_ADD, COND_HEAD, 6, 0, 0, 0, 0, 1.0f) }, ai[1] = { st_act(SOP_MUL, COND_AIR, 1.2f, 0, 0, 0, 0, 1.0f) },
                  el[1] = { st_act(SOP_MUL, COND_ELITE, 1.5f, 0, 0, 0, 0, 1.0f) };
            StatCtx cx = { 0 }, ch = { 0 }, cn = { 0 };
            ch.head = 1; ch.air = 1; ch.elite = 1;
            cx.pierced = 3;
            float p3 = st_fold(pier, 1, 100, &cx), t3 = st_fold(tend, 1, 100, &cx);
            cx.pierced = 7;
            float t7 = st_fold(tend, 1, 100, &cx);
            cx.pierced = 0;
            float p0 = st_fold(pier, 1, 100, &cx);
            fprintf(f, "sac de stats : cibles traversees 3 compose %.1f lineaire %.0f, 7 (plafond 5) lineaire %.0f, 0 cible %.0f ; tete %.0f sans %.0f, en l'air %.0f, elite %.0f, sans contexte %.0f "
                       "(attendu 152.1 115, 125, 100 ; 106 100, 120, 150, 100)\n", p3, t3, t7, p0, st_fold(hd, 1, 100, &ch), st_fold(hd, 1, 100, &cn),
                    st_fold(ai, 1, 100, &ch), st_fold(el, 1, 100, &ch), st_fold(hd, 1, 100, NULL));
        }
        {   /* condition de la table : Poing de plomb (x1,1 sous 5 m) en st_ctx, sans contexte, puis dans une vraie touche a 3 m et a 8 m */
            shOwn[SH_SHO] = 1u << SHO_POING;
            float k3 = st_ctx(SH_SHO, STAT_DEG, 100, &c3), k8 = st_ctx(SH_SHO, STAT_DEG, 100, &c8), k0 = st(SH_SHO, STAT_DEG, 100), hit[2];
            for (int k = 0; k < 2; k++) {
                rl_t_setup(0);
                P.loadout[0] = WP_SHOTGUN; P.loadout[1] = WP_ROCKET; P.weapon = WP_SHOTGUN;
                shOwn[SH_SHO] = 1u << SHO_POING;
                int i = vg_test_enemy(EN_DRONE, rl_t_at(k ? 8.0f : 3.0f, 0, 0), 100000);
                vgSrc = WP_SHOTGUN; damage_enemy(i, 100, v3(0, 0, 0)); vgSrc = -1;
                hit[k] = 100000 - E[i].hp;
            }
            fprintf(f, "sac de stats : Poing de plomb st_ctx a 3 m %.0f, a 8 m %.0f, sans contexte %.0f ; touche reelle de 100 a 3 m %.0f, a 8 m %.0f (attendu 110 100 100 ; 110 100)\n", k3, k8, k0, hit[0], hit[1]);
        }
        {   /* groupe : Plombs chercheurs (3 deg) et traqueurs (8 deg) ne s'additionnent pas, le plus fort compte */
            shOwn[SH_SHO] = 1u << SHO_CHERCHE;
            float g1 = st(SH_SHO, STAT_SUI, 0);
            shOwn[SH_SHO] = 1u << SHO_TRAQUEUR;
            float g2 = st(SH_SHO, STAT_SUI, 0);
            shOwn[SH_SHO] = (1u << SHO_CHERCHE) | (1u << SHO_TRAQUEUR);
            float g3 = st(SH_SHO, STAT_SUI, 0);
            shLvl[SH_SHO][SHO_CHERCHE] = 4;
            float g4 = st(SH_SHO, STAT_SUI, 0);
            shLvl[SH_SHO][SHO_CHERCHE] = 0;
            fprintf(f, "sac de stats : suivi chercheurs %.1f traqueurs %.1f les deux %.1f (pas 11), chercheurs niveau 4 + traqueurs %.1f (attendu 3.0 8.0 8.0, 8.0)\n", g1, g2, g3, g4);
        }
        {   /* niveaux : le delta est multiplie par l'echelle (gain, penalite, increment arrondi, plancher), et SMF_FIXED l'ignore */
            float lv[3], pen[2], flo[2];
            int nbp[2];
            static const unsigned char L3[3] = { 1, 3, 5 };
            shOwn[SH_SHO] = 1u << SHO_GROS;
            for (int k = 0; k < 3; k++) { shLvl[SH_SHO][SHO_GROS] = L3[k]; lv[k] = st(SH_SHO, STAT_TAI, 1.0f); }
            shOwn[SH_SHO] = 1u << SHO_CHEVRO;
            for (int k = 0; k < 2; k++) { shLvl[SH_SHO][SHO_CHEVRO] = L3[k]; pen[k] = st(SH_SHO, STAT_PERTE, 1.0f); }
            shOwn[SH_SHO] = 1u << SHO_PLUS;
            for (int k = 0; k < 2; k++) { shLvl[SH_SHO][SHO_PLUS] = L3[k]; nbp[k] = (int)(st(SH_SHO, STAT_NBP, 0) + 0.5f); }
            shOwn[SH_SHO] = 1u << SHO_TENDUE;
            for (int k = 0; k < 2; k++) { shLvl[SH_SHO][SHO_TENDUE] = (unsigned char)(k + 1); flo[k] = st(SH_SHO, STAT_PNEAR, SG_NEAR); }
            memset(shLvl, 0, sizeof shLvl);
            StatMod fx = { SH_SHO, 0, STAT_NBP, SOP_ADD, COND_NONE, 0, SMF_FIXED, 0, 1.0f, 0.0f }, pm = { SH_SHO, 0, STAT_VIT, SOP_MUL, COND_NONE, 0, 0, 0, 0.9f, 0.0f };
            float fixedV = st_scaled(&fx, 1.8f).v;
            fx.flags = 0;
            fprintf(f, "sac de stats : niveaux 1/3/5 Gros plomb x%.2f x%.2f x%.2f, penalite Chevrotine x%.2f -> x%.2f, Plombs en plus %d -> %d, plancher Pompe tendue %.0f -> %.0f m, "
                       "valeur fixe a l'echelle 1,8 : %.1f contre %.1f, penalite x0,9 a 1,8 : x%.2f (attendu 1.15 1.21 1.27, 0.90 -> 0.86, 2 -> 3, 20 -> 24, 1.0 contre 1.8, x0.82)\n",
                    lv[0], lv[1], lv[2], pen[0], pen[1], nbp[0], nbp[1], flo[0], flo[1], fixedV, st_scaled(&fx, 1.8f).v, st_scaled(&pm, 1.8f).v);
        }
        {   /* achat, montee de niveau et changement d'arme : le sac suit */
            sh_reset();
            P.loadout[0] = WP_RAIL; P.loadout[1] = WP_SHOTGUN; P.weapon = WP_RAIL;
            vgCredits = 1000;
            shOff[0] = (ShOffer){ SO_STEP, 0, SH_SHO, SHO_GROS, 100 }; shNOff = 1;
            int bought = up_buy(0);
            float b1 = st(SH_SHO, STAT_TAI, 1.0f);
            shOff[0] = (ShOffer){ SO_UP, 3, SH_SHO, SHO_GROS, 100 }; shNOff = 1;
            int up = up_buy(0);
            float b2 = st(SH_SHO, STAT_TAI, 1.0f);
            int sw1 = sh_swap(1, WP_ROCKET);
            float b3 = st(SH_SHO, STAT_TAI, 1.0f);
            int sw2 = sh_swap(1, WP_SHOTGUN);
            float b4 = st(SH_SHO, STAT_TAI, 1.0f);
            fprintf(f, "sac de stats : achat %d -> TAI x%.2f, niveau 3 %d -> x%.2f, Shotgun remplace par la roquette %d -> x%.2f, remis %d -> x%.2f, credits %d (attendu 1 -> 1.15, 1 -> 1.21, 1 -> 1.00, 1 -> 1.21, 200)\n",
                    bought, b1, up, b2, sw1, b3, sw2, b4, vgCredits);
        }
        {   /* genres generiques migres : vol de vie (s'additionne), Moisson (points par estoc, niveau 3), Cueillette (graine Impulsion seulement equipee) */
            sh_reset();
            P.loadout[0] = WP_RAIL; P.loadout[1] = WP_ROCKET; P.meleeKind = MK_LANCE; P.skill = SK_COIN;
            shOwn[SH_LAN] = (1u << 4) | (1u << 9) | (1u << 19);
            float vol = st(SH_LAN, STAT_VOL, 0) * 100;
            shOwn[SH_LAN] = 1u << 3;
            int m1 = vgm_estoc_pts();
            shLvl[SH_LAN][3] = 3;
            int m3 = vgm_estoc_pts();
            shOwn[SH_IMP] = 1u << 3;
            float cueLance = st(SH_IMP, STAT_MUR, 0);
            P.meleeKind = MK_FIST;
            float cueFist = st(SH_IMP, STAT_MUR, 0);
            fprintf(f, "sac de stats : vol de vie de la Lance 2+4+10 = %.0f %%, Moisson %d points par estoc, niveau 3 : %d, Cueillette avec la Lance %.0f, avec l'Impulsion %.0f (attendu 16 %%, 10, 14, 0, 10)\n",
                    vol, m1, m3, cueLance, cueFist);
        }
        {   /* op to (Releve, Charge eclair, Poings vifs) : vers la cible, le niveau pousse au-dela, ignoree si elle n'abaisse pas, le meilleur candidat, avant un mul de la phase suivante */
            StAct t1[1] = { st_act(SOP_TO, COND_NONE, 0.10f, 0, 0, 0, 0, 1.0f) }, t3[1] = { st_act(SOP_TO, COND_NONE, 0.10f, 0, 0, 0, 0, 1.4f) };
            StAct two[3] = { st_act(SOP_TO, COND_NONE, 0.20f, 0, 0, 0, 0, 1.0f), st_act(SOP_TO, COND_NONE, 0.16f, 0, 0, 0, 0, 1.0f),
                             st_act(SOP_MUL, COND_AIR, 0.8f, 0, 0, 1, 0, 1.0f) };
            StatCtx ca = { 0 };
            ca.air = 1;
            fprintf(f, "sac de stats : to 0.15 vers 0.10 -> %.3f, niveau 3 -> %.3f, cible au-dessus de la valeur (0.05) -> %.3f ; deux cibles 0.20 et 0.16 sur 0.22 -> %.3f, puis x0,8 en l'air -> %.3f (au sol %.3f) "
                       "(attendu 0.100, 0.080, 0.050 ; 0.160, 0.128 (0.160))\n", st_fold(t1, 1, 0.15f, NULL), st_fold(t3, 1, 0.15f, NULL), st_fold(t1, 1, 0.05f, NULL),
                    st_fold(two, 2, 0.22f, NULL), st_fold(two, 3, 0.22f, &ca), st_fold(two, 3, 0.22f, NULL));
        }
        {   /* phases et plafond : Cadence x0,9 (phase 0) PUIS Combo enchaine -0,2 s (phase 1) = 0,43 s comme le code d'origine ; dans la meme phase le add passe avant le mul : 0,45 s ; op min */
            StAct cad[2] = { st_act(SOP_MUL, COND_NONE, 0.9f, 0, 0, 0, 0, 1.0f), st_act(SOP_ADD, COND_NONE, -0.2f, 0, 0, 1, 0, 1.0f) };
            StAct same[2] = { cad[0], st_act(SOP_ADD, COND_NONE, -0.2f, 0, 0, 0, 0, 1.0f) };
            StAct l3[2] = { st_act(SOP_MUL, COND_NONE, 0.9f, 0, 0, 0, 0, 1.4f), st_act(SOP_ADD, COND_NONE, -0.2f, 0, 0, 1, 0, 1.4f) };
            StAct cap[2] = { st_act(SOP_ADD, COND_NONE, 0.15f, 0, 0, 0, 0, 1.6f), st_act(SOP_MIN, COND_NONE, 0.95f, 0, 0, 0, 1, 1.6f) };
            StAct cap2[2] = { st_act(SOP_ADD, COND_NONE, 0.8f, 0, 0, 0, 0, 1.0f), cap[1] };
            fprintf(f, "sac de stats : phases Cadence puis Combo enchaine sur 0,70 s -> %.3f (meme phase %.3f, niveau 3 %.3f) ; plafond fixe 0,95 : 0,30 + 0,15 niveau 4 -> %.2f, 0,30 + 0,8 -> %.2f "
                       "(attendu 0.430 (0.450, 0.322) ; 0.54, 0.95)\n", st_fold(cad, 2, 0.70f, NULL), st_fold(same, 2, 0.70f, NULL), st_fold(l3, 2, 0.70f, NULL),
                    st_fold(cap, 2, 0.30f, NULL), st_fold(cap2, 2, 0.30f, NULL));
        }
        {   /* groupes lus a l'usage (parmi les modificateurs applicables) : ricochet de piece, Polie +0,1 et Prime +0,5 au 1er rebond (groupe 1 : Prime REMPLACE Polie la, pas aux autres), Pur x1,25 des le 3e rebond, empile */
            StAct pol[1] = { st_act(SOP_ADD, COND_NONE, 0.1f, 0, 1, 0, 0, 1.0f) };
            StAct all[3] = { pol[0], st_act(SOP_ADD, COND_FIRST, 0.5f, 0, 1, 0, 0, 1.0f), st_act(SOP_MUL, COND_BOUNCE, 1.25f, 3, 0, 0, 0, 1.0f) };
            StatCtx cb = { 0 };
            float rp[4], ra[4];
            for (int b = 1; b <= 4; b++) { cb.bounce = b; rp[b - 1] = st_fold(pol, 1, COIN_MULT, &cb); ra[b - 1] = st_fold(all, 3, COIN_MULT, &cb); }
            cb.bounce = 2;
            fprintf(f, "sac de stats : ricochet de piece, rebonds 1 a 4 : Polie seule %.2f %.2f %.2f %.2f ; Polie + Prime + Pur %.2f %.2f %.2f %.2f ; condition first au rebond 2 : %.2f "
                       "(attendu 1.60 1.60 1.60 1.60 ; 2.00 1.60 2.00 2.50 ; 1.50)\n", rp[0], rp[1], rp[2], rp[3], ra[0], ra[1], ra[2], ra[3], st_fold(&all[1], 1, COIN_MULT, &cb));
        }
        {   /* groupe de facteurs : Rail large x1,15 et Rail colossal x1,40 (groupe 1) : le colossal remplace, il ne se compose pas ; large niveau 5 (x1,27) reste sous le colossal niveau 1 */
            StAct lc[2] = { st_act(SOP_MUL, COND_NONE, 1.15f, 0, 1, 0, 0, 1.0f), st_act(SOP_MUL, COND_NONE, 1.40f, 0, 1, 0, 0, 1.0f) };
            StAct l5[2] = { st_act(SOP_MUL, COND_NONE, 1.15f, 0, 1, 0, 0, 1.8f), lc[1] };
            fprintf(f, "sac de stats : groupe de facteurs Rail large + colossal -> x%.2f, large niveau 5 + colossal -> x%.2f, large seul -> x%.2f (attendu 1.40, 1.40, 1.15)\n",
                    st_fold(lc, 2, 1.0f, NULL), st_fold(l5, 2, 1.0f, NULL), st_fold(lc, 1, 1.0f, NULL));
        }
        {   /* degats par cible du Railgun (Calibre lourd compose, Rail tendu lineaire plafonne a 5, Tir d'elite) et conditions direct, rebond a partir du 3e (empile), salve pleine */
            StAct deg[3] = { st_act(SOP_MUL, COND_PIERCED, 1.15f, 0, 0, 0, 0, 1.0f), st_act(SOP_MULL, COND_PIERCED, 1.05f, 5, 0, 0, 0, 1.0f),
                             st_act(SOP_MUL, COND_ELITE, 1.5f, 0, 0, 0, 0, 1.0f) };
            StAct dir[1] = { st_act(SOP_ADD, COND_DIRECT, 0.15f, 0, 0, 0, 0, 1.0f) }, full[1] = { st_act(SOP_MUL, COND_SALVEFULL, 1.3f, 0, 0, 0, 0, 1.0f) };
            StAct bnc[1] = { st_act(SOP_MUL, COND_BOUNCE, 1.25f, 3, 0, 0, 0, 1.0f) };
            StatCtx cr = { 0 }, cd = { 0 }, cf = { 0 }, cn = { 0 }, cb2 = { 0 }, cb3 = { 0 }, cb5 = { 0 };
            float d0 = st_fold(deg, 3, 1.0f, &cr), d3, d7;
            cr.pierced = 3; cr.elite = 1; d3 = st_fold(deg, 3, 1.0f, &cr);
            cr.pierced = 7; cr.elite = 0; d7 = st_fold(deg, 3, 1.0f, &cr);
            cd.direct = 1; cf.full = 1; cb2.bounce = 2; cb3.bounce = 3; cb5.bounce = 5;
            fprintf(f, "sac de stats : degats par cible du rail, 1re cible %.3f, 4e elite %.3f, 8e %.3f ; direct %.2f (sans %.2f) ; salve pleine x%.2f (sans x%.2f) ; rebond a partir du 3e : 2e x%.3f, 3e x%.3f, 5e x%.3f "
                       "(attendu 1.000, 2.624, 3.325 ; 0.15 (0.00) ; x1.30 (x1.00) ; x1.000, x1.250, x1.953)\n", d0, d3, d7, st_fold(dir, 1, 0, &cd), st_fold(dir, 1, 0, &cn),
                    st_fold(full, 1, 1.0f, &cf), st_fold(full, 1, 1.0f, &cn), st_fold(bnc, 1, 1.0f, &cb2), st_fold(bnc, 1, 1.0f, &cb3), st_fold(bnc, 1, 1.0f, &cb5));
        }
        {   /* formules de familles reproduites a la main : demi-angle All in One en degres (Salve large +0,3 puis Salve precise x0,7), recharge de joute (Recharge -0,2 s puis Lance-eclair x0,65) */
            StAct ang[2] = { st_act(SOP_ADD, COND_NONE, 0.3f, 0, 0, 0, 0, 1.0f), st_act(SOP_MUL, COND_NONE, 0.7f, 0, 0, 0, 0, 1.0f) };
            StAct cdj[2] = { st_act(SOP_ADD, COND_NONE, -0.2f, 0, 0, 0, 0, 1.0f), st_act(SOP_MUL, COND_NONE, 0.65f, 0, 0, 0, 0, 1.0f) };
            fprintf(f, "sac de stats : demi-angle All in One sur 3 deg : large + precise %.2f, precise seule %.2f, large seule %.2f ; recharge de joute sur 1,0 s : Recharge + Lance-eclair %.2f "
                       "(attendu 2.31, 2.10, 3.30 ; 0.52)\n", st_fold(ang, 2, 3.0f, NULL), st_fold(&ang[1], 1, 3.0f, NULL), st_fold(&ang[0], 1, 3.0f, NULL), st_fold(cdj, 2, 1.0f, NULL));
        }
        {   /* st_int arrondit au plus proche (Plombs en plus : +2, +2,8, +3,6 aux niveaux 1, 3, 5) ; st_int_ctx de meme, avec contexte (Poing de plomb : +10 % sous 5 m) ;
               la table generee est coherente (indices, enums, une etape par ligne) */
            static const unsigned char LV[3] = { 1, 3, 5 };
            int ia[3], ib[3];
            StatCtx cp = { 0 };
            P.loadout[0] = WP_SHOTGUN; P.loadout[1] = WP_ROCKET;
            shOwn[SH_SHO] = 1u << SHO_PLUS;
            for (int k = 0; k < 3; k++) { shLvl[SH_SHO][SHO_PLUS] = LV[k]; ia[k] = st_int(SH_SHO, STAT_NBP, 0); ib[k] = st_int(SH_SHO, STAT_NBP, 10); }
            shLvl[SH_SHO][SHO_PLUS] = 0;
            shOwn[SH_SHO] = 1u << SHO_POING;
            cp.dist = 3; int pn = st_int_ctx(SH_SHO, STAT_DEG, 100, &cp);
            cp.dist = 8; int pf = st_int_ctx(SH_SHO, STAT_DEG, 100, &cp);
            fprintf(f, "sac de stats : st_int Plombs en plus niveaux 1/3/5 : %d %d %d (base 10 : %d %d %d), st_int_ctx Poing de plomb a 3 m %d, a 8 m %d ; table generee, anomalies %d "
                       "(attendu 2 3 4 (12 13 14), 110, 100 ; 0)\n", ia[0], ia[1], ia[2], ib[0], ib[1], ib[2], pn, pf, st_table_check());
        }
        memset(shOwn, 0, sizeof shOwn); memset(shLvl, 0, sizeof shLvl); vgCredits = 0;
        vg_tests_roq_rail(f); vg_tests_melee(f); vg_tests_comp(f);   /* zones reservees aux agents des familles (nouvelles lignes de test) */
    }
    {   /* bastion : bouclier de face, nu de dos */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        P.pos = v3(0, 0, 30);
        int b = vg_test_enemy(EN_BASTION, v3(0, 1.4f, 20), 1000);
        E[b].yaw = 0;                                     /* face vers +z : vers le joueur */
        damage_enemy(b, 100, v3(0, 0, 0)); float front = 1000 - E[b].hp;
        E[b].yaw = PI; damage_enemy(b, 100, v3(0, 0, 0)); float back = 1000 - front - E[b].hp;
        fprintf(f, "bastion : touche de face=%.0f de dos=%.0f (attendu 15 100)\n", front, back);
    }
    {   /* BOMBE : s'arrete a ~8 m, tique VG_BOMBE_FUSE s puis explose (5 m) ; tuee pendant le tic = pas d'explosion */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        P.pos = P.prevPos = v3(0, 0, 30); P.vel = v3(0, 0, 0); P.dead = 0; P.hp = 100;
        int b = vg_test_enemy(EN_BOMBE, vg_test_clear_spot(25, 3), 1000);   /* a 25 m : doit foncer, puis s'arreter */
        int stopped = 0; float hpFar = 100, stopDist = 0;
        for (int i = 0; i < 600 && E[b].alive; i++) {
            P.pos = P.prevPos = v3(0, 0, 30); P.hp = 100;
            enemy_tick(TICK);
            if (E[b].st == 1 && !stopped) { stopped = 1; stopDist = vlen(vsub(E[b].pos, pcenter())); }
        }
        hpFar = P.hp;
        int tickedOut = !E[b].alive;
        /* a 3 m (dans la zone) : degats VG_BOMBE_DMG ; a 12 m (hors zone) : rien */
        vg_test_begin(0); memset(E, 0, sizeof E);
        P.pos = P.prevPos = v3(0, 0, 30); P.hp = 100;
        int n1 = vg_test_enemy(EN_BOMBE, v3(0, 1.4f, 27), 1000);
        E[n1].st = 1; E[n1].stT = 0.01f;
        enemy_tick(TICK); enemy_tick(TICK);
        float hpIn = P.hp;
        vg_test_begin(0); memset(E, 0, sizeof E);
        P.pos = P.prevPos = v3(0, 0, 30); P.hp = 100;
        int n2 = vg_test_enemy(EN_BOMBE, v3(0, 1.4f, 18), 1000);
        E[n2].st = 1; E[n2].stT = 0.01f;
        enemy_tick(TICK); enemy_tick(TICK);
        float hpOut = P.hp;
        vg_test_begin(0); memset(E, 0, sizeof E);
        P.pos = P.prevPos = v3(0, 0, 30); P.hp = 100;
        int n3 = vg_test_enemy(EN_BOMBE, v3(0, 1.4f, 27), 10);
        E[n3].st = 1; E[n3].stT = 1.0f;
        damage_enemy(n3, 100, v3(0, 0, 0));
        for (int i = 0; i < 200; i++) enemy_tick(TICK);
        fprintf(f, "bombe : s'arrete a %.1f m (attendu < 8,5) et tique=%d, explose a la fin=%d, PV dans la zone %.0f hors zone %.0f tuee pendant le tic %.0f (attendu 70, 100, 100)\n",
                stopDist, stopped, tickedOut, hpIn, hpOut, P.hp);
        (void)hpFar;
    }
    {   /* CARRE : tant qu'il vit, les autres encaissent x VG_CARRE_MULT ; lui n'est pas protege */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        P.pos = P.prevPos = v3(0, 0, 30);
        int d = vg_test_enemy(EN_DRONE, v3(0, 5, 10), 1000), c = vg_test_enemy(EN_CARRE, v3(5, 8, 10), 1000);
        damage_enemy(d, 100, v3(0, 0, 0)); float withC = 1000 - E[d].hp;
        damage_enemy(c, 100, v3(0, 0, 0)); float onC = 1000 - E[c].hp;
        kill_enemy(c, 0);
        float h0 = E[d].hp; damage_enemy(d, 100, v3(0, 0, 0)); float noC = h0 - E[d].hp;
        fprintf(f, "carre : drone avec carre %.0f, carre lui-meme %.0f, drone sans carre %.0f (attendu 10, 100, 100)\n", withC, onC, noC);
    }
    {   /* MORTIER : orbe lente, explosion de 8 m (degats fixes) ; hors zone rien */
        vg_test_begin(0);
        memset(E, 0, sizeof E); memset(MB, 0, sizeof MB);
        P.pos = P.prevPos = v3(0, 0, 30); P.hp = 100; P.dead = 0; P.meleeKind = MK_FIST;
        int m = vg_test_enemy(EN_MORTIER, vg_test_clear_spot(20, 4), 1000);
        E[m].fireT = 100; E[m].st = 1; E[m].stT = 0.01f; E[m].aim = vnorm(vsub(pcenter(), E[m].pos));
        enemy_tick(TICK); enemy_tick(TICK);
        int fired = 0; for (int k = 0; k < MAXMB; k++) fired += MB[k].on;
        float sp = 0; for (int k = 0; k < MAXMB; k++) if (MB[k].on) sp = vlen(MB[k].vel);
        for (int i = 0; i < 1000; i++) { E[m].stT = 100; E[m].st = 0; enemy_tick(TICK); }   /* le mortier ne retire plus ; l'orbe arrive sur toi */
        float hpHit = P.hp;
        memset(MB, 0, sizeof MB); P.hp = 100;
        MB[0] = (MortOrb){ v3(0, 1, 15), v3(0, 0, 0), 0.01f, 1 };   /* detonation a 15 m : hors zone de 8 m */
        enemy_tick(TICK);
        fprintf(f, "mortier : orbes tirees %d a %.1f m/s, PV apres explosion %.0f, orbe a 15 m du joueur : PV %.0f (attendu 1, 7,0, 70, 100)\n",
                fired, sp, hpHit, P.hp);
    }
    {   /* PYRAMIDE : armure, phase de charges a 50 %, charge paree = etourdie, charge encaissee */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        P.pos = P.prevPos = v3(0, 0, 30); P.yaw = 0; P.pitch = 0;
        int p = boss_spawn(EN_PYRA);
        Enemy *e = &E[p];
        float h0 = e->hp; damage_enemy(p, 100, v3(0, 0, 0)); float armor = h0 - e->hp;
        h0 = e->hp; vgProjKind = PR_PARRIED; damage_enemy(p, 100, v3(0, 0, 0)); vgProjKind = -1; float parried = h0 - e->hp;
        for (int i = 0; i < 240; i++) { P.hp = 100; enemy_tick(TICK); boss_fx_tick(TICK); }
        int moved = e->st != BS_MOVE || e->combo > 0 || e->frac != 0;
        e->hp = e->maxHp * 0.49f; enemy_tick(TICK);
        int phase = e->phase;
        memset(PR, 0, sizeof PR);
        e->st = BS_RAM; e->stT = 1.0f; e->pos = vadd(eye_pos(), v3(0, 0, -(enemy_radius(EN_PYRA) + 2.5f)));
        e->aim = v3(0, 0, 1); P.parryWin = 0.1f; h0 = e->hp;
        enemy_tick(TICK);
        int stunned = e->st == BS_STUN; float rammed = h0 - e->hp;
        hitstop = 0;
        h0 = e->hp; damage_enemy(p, 100, v3(0, 0, 0)); float stunDmg = h0 - e->hp;
        e->st = BS_RAM; e->stT = 1.0f; e->pos = vadd(pcenter(), v3(0, 0, -(enemy_radius(EN_PYRA) + 0.3f)));
        e->aim = v3(0, 0, 1); P.parryWin = 0; P.hp = 100;
        enemy_tick(TICK);
        fprintf(f, "pyramide : armure %.0f, orbe paree %.0f, bouge et tire=%d, phase charges a 49%%=%d, charge paree etourdit=%d (-%.0f), etourdie %.0f, charge encaissee -%.0f PV (attendu 35, 100, 1, 1, 1 (-250), 150, 22)\n",
                armor, parried, moved, phase, stunned, rammed, stunDmg, 100 - P.hp);
        vgSecret = 1; e->st = BS_RECOVER; e->stT = 0; e->combo = 3; e->phase = 1;
        int before = enemies_alive();
        enemy_tick(TICK);
        fprintf(f, "pyramide INFINI : PRISME, fantomes apparus=%d (attendu 3)\n", enemies_alive() - before);
        vgSecret = 0;
    }
    {   /* TESSERACT : immobile, invulnerable tant qu'il a 4 invocations, invocations qui arrivent seules */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        P.pos = P.prevPos = v3(0, 0, 40);
        int t = boss_spawn(EN_TESS);
        Enemy *e = &E[t];
        V3 home = e->home;
        e->fracT = 0;
        for (int i = 0; i < 60; i++) { P.hp = 100; enemy_tick(TICK); }
        int links = boss_links(t);
        float h0 = e->hp; damage_enemy(t, 100, v3(0, 0, 0)); float shielded = h0 - e->hp;
        for (int j = 0; j < MAXE; j++) if (E[j].alive && E[j].summoned) kill_enemy(j, 0);
        h0 = e->hp; damage_enemy(t, 100, v3(0, 0, 0)); float open = h0 - e->hp;
        fprintf(f, "tesseract : invocations=%d (>= %d), degats sous bouclier=%.0f, sans invocations=%.0f, deplacement=%.2f, libre dans le decor=%d (attendu >= 4, 0, 100, 0.00, 1)\n",
                links, VG_LINKS, shielded, open, vlen(vsub(e->pos, home)), sphere_free(e->pos, enemy_radius(EN_TESS)));
    }
    {   /* SPHERE : coups depuis le sol presque nuls, onde de choc qui touche au sol et pas en l'air */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        P.pos = P.prevPos = v3(-20, 0, 30);
        int b = boss_spawn(EN_BALL);
        Enemy *e = &E[b];
        P.grounded = 1; float h0 = e->hp; damage_enemy(b, 100, v3(0, 0, 0)); float ground = h0 - e->hp;
        P.grounded = 0; h0 = e->hp; damage_enemy(b, 100, v3(0, 0, 0)); float air = h0 - e->hp;
        memset(GW, 0, sizeof GW);
        gwave_add(v3(-20, 0, 20), 30);
        P.hp = 100; P.pos = v3(-20, 0, 30);
        for (int i = 0; i < 120; i++) { P.pos = P.prevPos = v3(-20, 0, 30); P.vel = v3(0, 0, 0); boss_fx_tick(TICK); }
        float onGround = 100 - P.hp;
        memset(GW, 0, sizeof GW);
        gwave_add(v3(-20, 0, 20), 30);
        P.hp = 100;
        for (int i = 0; i < 120; i++) { P.pos = P.prevPos = v3(-20, 2.5f, 30); P.vel = v3(0, 0, 0); boss_fx_tick(TICK); }
        float jumped = 100 - P.hp;
        for (int i = 0; i < 1200; i++) { P.pos = P.prevPos = v3(-20, 30, 30); P.hp = 100; enemy_tick(TICK); boss_fx_tick(TICK); }
        fprintf(f, "sphere : touche depuis le sol=%.0f en l'air=%.0f, onde au sol=%.0f PV, en sautant=%.0f, bonds=%d (attendu 15, 100, 18, 0, >= 1)\n",
                ground, air, onGround, jumped, bossJumps);
    }
    {   /* HEXAEDRE : quatre phases, armure de la phase PRECISION et coeur ouvert */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        P.pos = P.prevPos = v3(0, 0, 40);
        int h = boss_spawn(EN_HEXA);
        Enemy *e = &E[h];
        float h0 = e->hp; damage_enemy(h, 100, v3(0, 0, 0)); float armor = h0 - e->hp;
        e->st = BS_OPEN; e->stT = 2; h0 = e->hp; damage_enemy(h, 100, v3(0, 0, 0)); float open = h0 - e->hp;
        int ph[3];
        float fr[3] = { 0.7f, 0.4f, 0.2f };
        for (int k = 0; k < 3; k++) { e->hp = e->maxHp * fr[k]; P.hp = 100; enemy_tick(TICK); ph[k] = e->phase; }
        for (int i = 0; i < 1200; i++) { P.hp = 100; enemy_tick(TICK); boss_fx_tick(TICK); }
        fprintf(f, "hexaedre : armure %.0f, coeur ouvert %.0f, phases a 70/40/20 %% = %d %d %d, vivant apres 10 s de phase TOUT=%d (attendu 30, 150, 1 2 3, 1)\n",
                armor, open, ph[0], ph[1], ph[2], e->alive);
    }
    {   /* boss vaincu : ses invocations s'eteignent, la vague se vide */
        vg_test_begin(0);
        memset(E, 0, sizeof E);
        int t = boss_spawn(EN_TESS);
        for (int k = 0; k < 3; k++) { int s = vg_test_enemy(EN_DRONE, v3(5.0f * k, 12, 0), 100); E[s].summoned = 1; }
        int c0 = vgCredits;
        E[t].hp = 1; bossBypass = 1; damage_enemy(t, 10, v3(0, 0, 0)); bossBypass = 0;
        fprintf(f, "boss vaincu : ennemis restants=%d, credits +%d, barre du boss=%d (attendu 0, 600, -1)\n", enemies_alive(), vgCredits - c0, vgBoss);
    }
    {   /* vague 100 : temps (point d'entree du speedrun), splits, puis l'INFINI */
        vg_test_begin(0);
        vgSplit[0] = 300; vgSplit[1] = 650; vgSplit[2] = 1000;
        wave = 100; vgState = VG_FIGHT; vgQn = vgQi = 0; memset(E, 0, sizeof E);
        vgRunT = 1234.5f;
        srTicket[SR_VAGUES100] = SRT_WAIT;                /* ticket du depart attendu : le rapport part a son arrivee */
        wave_tick(TICK);
        const SrRun *sr = &srRun[SR_VAGUES100];
        fprintf(f, "vague 100 -> speedrun : rapport en attente du ticket %d, %d ms en %d splits (attendu 1, 1234500 ms en 4 splits)\n",
                sr->state == SRR_WAIT, sr->ms, sr->n);
        srTicket[SR_VAGUES100] = SRT_NONE; srRun[SR_VAGUES100].state = SRR_NONE;
        int sp[4], n = vagues_splits_ms(sp);
        int win = vgState == VG_WIN && vgMenu == VGM_WIN;
        vg_clock(1); float frozenT = vagues_temps_100();
        fprintf(f, "vague 100 : ecran=%d temps=%.1f (fige %.1f) splits %d : %d %d %d %d (attendu 1, 1234.5 (1234.5), 4 : 300000 650000 1000000 1234500)\n",
                win, vagues_temps_100(), frozenT, n, sp[0], sp[1], sp[2], sp[3]);
        vg_go_infinite();
        vgPend = 0; vg_menu_close();
        wave_tick(TICK);
        for (int i = 0; i < (int)(VG_PAUSE_T * 120) + 2; i++) wave_tick(TICK);
        int w101 = wave;
        vgWaveT = 0.05f; vgQi = 0;                        /* chrono de la vague 101 ecoule, ennemis encore la */
        for (int i = 0; i < 12; i++) wave_tick(TICK);
        int w102 = wave, carried = vgQn;
        wave = 124; vgWaveT = 0.01f; vgQn = vgQi = 0; memset(E, 0, sizeof E);
        for (int i = 0; i < 12; i++) wave_tick(TICK);     /* 124 videe : 125 (boss) arrive au bout du chrono */
        int w125 = wave, boss = vgBoss >= 0;
        for (int i = 0; i < 120 * 40; i++) { P.hp = 100; enemy_tick(TICK); wave_tick(TICK); hitstop = 0; }
        fprintf(f, "INFINI : vague %d puis %d au chrono (ennemis en file %d > 0), boss %d=%d qui attend 40 s : vague %d, secret=%d (attendu 101, 102, 125=1, 125, 1)\n",
                w101, w102, carried, w125, boss, wave, vgSecret);
    }
    {   /* temps de partie : les pauses ECHAP ne comptent pas, la boutique oui */
        vg_test_begin(0);
        vgRunT = 0;
        for (int i = 0; i < 120; i++) vg_clock(TICK);
        nowT = 500; vg_menu_set(VGM_SHOP); vg_clock(1); nowT = 503; vg_menu_set(VGM_NONE);
        fprintf(f, "temps vagues : 1 s de jeu + 3 s de boutique = %.2f (attendu 4.00)\n", vgRunT);
    }
    {   /* challenges : defense de zone (reussie en restant dedans), course (arrivee) */
        vg_test_begin(0);
        vgChoiceMap[0] = 0; vgChoiceReward = 500; wave = 20;
        vg_challenge_start(CH_DEFENSE);
        int c0 = vgCredits;
        for (int i = 0; i < 120 * 45 && vgState == VG_CHALLENGE; i++) {
            P.pos = P.prevPos = vg_zone(); P.vel = v3(0, 0, 0); P.hp = 100;
            enemy_tick(TICK); proj_tick(TICK); wave_tick(TICK); hitstop = 0;
        }
        int defOk = vgState == VG_BREAK && vgChalOk, gain = vgCredits - c0;
        vg_challenge_start(CH_DEFENSE);
        for (int i = 0; i < 120 * 101 && vgState == VG_CHALLENGE; i++) {
            P.pos = P.prevPos = v3(-60, 0, -60); P.hp = 100; wave_tick(TICK);   /* hors de la zone */
        }
        int defFail = vgState == VG_BREAK && !vgChalOk;
        vg_challenge_start(CH_COURSE);
        int onRace = raceOn && curMap == race_map();
        raceCount = 0; raceDone = 1;
        wave_tick(TICK);
        fprintf(f, "challenges : defense reussie=%d (+%d), ratee hors zone=%d, course sur LA TOILE=%d, arrivee -> retour %s=%d (attendu 1 (+500), 1, 1, ARENE=1)\n",
                defOk, gain, defFail, onRace, maps[curMap].name, !raceOn && vgChalOk && curMap == 0);
    }
    vagOn = 0; vgMenu = VGM_NONE; raceOn = 0;
    if (curMap != 0) load_map(0);
    reset_game();
    nowT = sNow;
}

/* --- captures --shot vagues_* (appelees par velocity_main) : un boss, la boutique, le selecteur, la fin --- */
static void save_bmp(const char *path);
static void vg_shot_look(V3 at) {                         /* le joueur regarde le point at */
    V3 d = vsub(at, eye_pos());
    P.yaw = atan2f(d.x, -d.z); P.pitch = asinf(clampf(d.y / fmaxf(vlen(d), 0.01f), -1, 1));
}
static int vagues_shot(const char *path) {
    if (!strstr(path, "vagues")) return 0;
    appState = APP_GAME;
    vg_test_begin(0);
    P.pos = P.prevPos = v3(0, 0, 34); P.grounded = 1;
    score = 48250; kills = 212; vgCredits = 1460; vgRunT = 1311.4f; wave = 24; vgState = VG_FIGHT;
    if (strstr(path, "pyramide")) {                       /* vague 25 : grosse orbe en charge ("charge" : phase 2, charge annoncee) */
        wave = 25;
        int b = boss_spawn(EN_PYRA);
        Enemy *e = &E[b];
        e->pos = v3(0, 9, 8); e->vel = v3(0, 0, 0);
        vg_shot_look(vadd(e->pos, v3(0, -1, 0)));
        if (strstr(path, "charge")) { e->phase = 1; e->hp = e->maxHp * 0.42f; e->st = BS_AIM; e->stT = 0.35f; e->aim = vnorm(vsub(pcenter(), e->pos)); }
        else {
            e->hp = e->maxHp * 0.81f; e->st = BS_CHARGE; e->stT = 0.6f;
            for (int k = 0; k < 4; k++) spawn_proj(PR_ORB, vadd(e->pos, v3(-3 + 2.0f * k, -1, 6 + 3.0f * k)), v3(0, -1, 18));
            for (int i = 0; i < 20; i++) { add_part(vadd(e->pos, vmul(rand_dir(), 4)), v3(0, 0, 0), v3(1, 0.8f, 0.3f), 0.4f, 0.18f, 0); }
        }
        snprintf(vgHint, sizeof vgHint, strstr(path, "charge") ? "PARE LA CHARGE AU DERNIER MOMENT" : "GROSSE ORBE : PARE-LA"); vgHintT = 2;
    } else if (strstr(path, "tesseract")) {               /* vague 50 : invocations reliees (bouclier) et anneau d'orbes */
        wave = 50;
        int b = boss_spawn(EN_TESS);
        Enemy *e = &E[b];
        e->fracT = 0; e->fireT = 100;
        for (int i = 0; i < 40; i++) enemy_tick(TICK);
        e->fireT = 0; e->st = 2;                          /* le prochain barrage est l'anneau */
        enemy_tick(TICK);
        for (int i = 0; i < 50; i++) proj_tick(TICK);
        e->hp = e->maxHp * 0.66f;
        P.pos = P.prevPos = v3(-10, 0, 22);
        vg_shot_look(e->pos);
        snprintf(vgHint, sizeof vgHint, "BOUCLIER : TUE SES INVOCATIONS"); vgHintT = 2;
    } else if (strstr(path, "boule")) {                   /* vague 75 : la sphere vient de frapper le sol, on saute l'onde */
        wave = 75;
        int b = boss_spawn(EN_BALL);
        Enemy *e = &E[b];
        float R = enemy_radius(EN_BALL);
        e->pos = v3(-15, R, 14); e->vel = v3(6, 0, 4); e->yaw = 1.3f; e->st = BS_MOVE; e->stT = 4;
        gwave_add(v3(-15, 0, 14), 30); GW[0].r = 13;
        gwave_add(v3(-15, 0, 14), 30); GW[1].r = 6;
        P.pos = P.prevPos = v3(2, 3.2f, 30); P.grounded = 0; P.vel = v3(0, 4, -9);
        e->hp = e->maxHp * 0.57f;
        vg_shot_look(vadd(e->pos, v3(0, -1.5f, 0)));
        snprintf(vgHint, sizeof vgHint, "SAUTE PAR-DESSUS LES ONDES"); vgHintT = 2;
    } else if (strstr(path, "hexaedre")) {                /* vague 100 : rayons annonces, un rayon parti, coeur ouvert */
        wave = 100;
        int b = boss_spawn(EN_HEXA);
        Enemy *e = &E[b];
        e->pos = v3(0, 11, 6); e->vel = v3(0, 0, 0); e->st = BS_OPEN; e->stT = 2; e->hp = e->maxHp * 0.79f; e->yaw = 0.6f;
        laser_add(e->pos, vnorm(vsub(vadd(pcenter(), v3(2.5f, 0, 0)), e->pos)), 0.5f);
        laser_add(e->pos, vnorm(vsub(vadd(pcenter(), v3(-4, 0, 1)), e->pos)), 0);
        boss_fx_tick(TICK); LZ[1].life = 0.2f;
        vg_shot_look(e->pos);
        snprintf(vgHint, sizeof vgHint, "COEUR OUVERT : TIRE !"); vgHintT = 2;
    } else if (strstr(path, "hybride")) {                 /* forme fusionnee active (vagues_hybride_canon / _obusier, sinon ARTILLERIE ORBITALE) */
        int canon = strstr(path, "canon") != NULL, obus = strstr(path, "obusier") != NULL;
        wave = 40;
        sh_reset();
        if (canon) hy_t_setup(SH_RAIL, SH_SHO, WP_SHOTGUN);
        else if (obus) hy_t_setup(SH_ROQ, SH_SHO, WP_ROCKET);
        else hy_t_setup(SH_RAIL, SH_ROQ, WP_RAIL);
        vgCredits = 1460; vgState = VG_FIGHT; vgMenu = VGM_NONE; vgHpMul = vg_hp_mul(wave);
        P.pos = P.prevPos = v3(0, 0, 34); P.grounded = 1; P.yaw = 0; P.pitch = 0.05f; P.hp = 100; P.dead = 0;
        memset(E, 0, sizeof E);
        static const float at[5][3] = { { 0, 3, 12 }, { -6, 4, 6 }, { 6, 2.5f, 10 }, { 2, 6, 0 }, { -10, 5, 18 } };
        for (int k = 0; k < 5; k++) { int i = spawn_enemy_at(k == 2 ? EN_BASTION : EN_DRONE, v3(at[k][0], at[k][1], at[k][2]), 0); if (i >= 0) { E[i].fireT = 100; E[i].vel = v3(0, 0, 0); } }
        vgQn = 6; vgQi = 1;
        hy_t_go(); hyT = 11.3f;
        vg_shot_look(v3(0, 3, 10));
        P.fireCd = 0; vgSrc = P.weapon; fire_weapon(); vgSrc = -1;
        if (obus) for (int i = 0; i < 14; i++) proj_tick(TICK);
    } else if (strstr(path, "boutique")) {                /* boutique : quelques etapes achetees */
        vgState = VG_BREAK; wave = 30;
        sh_reset(); vgCredits = 640;
        shOwn[SH_RAIL] = 0x1u; shOwn[SH_ROQ] = 0x1u;
        shLvl[SH_RAIL][0] = 2;                            /* Calibre niveau 2 : la premiere offre est son niveau 3 */
        sh_roll();
        if (shNOff < 1) shNOff = 1;
        shOff[0] = (ShOffer){ SO_UP, 3, SH_RAIL, 0, sh_up_price(SH_RAIL, 0) };
        if (strstr(path, "forme")) {                      /* vagues_boutique_forme : offre FUSION + FORME ; _forme_achetee : fusion achetee (resume) */
            int fu = rl_t_fusion(SH_ROQ);
            shOff[0] = (ShOffer){ SO_FUSION, fu, SH_RAIL, 0, SH_FUSION_PRICE };
            if (strstr(path, "achetee")) shFusion = fu;
        }
        vgMenu = VGM_SHOP; vgShopHover = 0; shSwapOpen = strstr(path, "arme") != NULL;   /* vagues_boutique_arme : panneau CHANGER D ARME */
    } else if (strstr(path, "selecteur")) {               /* apres la vague 20 : simple, difficile, challenge */
        wave = 20; vgCredits = 2210;
        vg_make_choices();
        vgState = VG_SELECT; vgMenu = VGM_SELECT;
    } else if (strstr(path, "fin") && !strstr(path, "infini")) {   /* vague 100 terminee */
        wave = 100; vgT100 = vgRunT = 2843.27f; vgSplit[0] = 611.2f; vgSplit[1] = 1290.8f; vgSplit[2] = 2034.5f; vgSplit[3] = vgT100;
        score = 412830; kills = 2316; vgKillsBoss = 4; bestScore = score;
        vgState = VG_WIN; vgMenu = VGM_WIN;
    } else {                                              /* "vagues" / "vagues_infini" : une vague ordinaire en cours */
        if (strstr(path, "infini")) { vgInfinite = 1; vgSecret = 1; wave = 113; vgWaveT = 11.5f; }
        vgHpMul = vg_hp_mul(wave); vgEliteP = 0.4f;
        static const int ty[8] = { EN_DRONE, EN_SNIPER, EN_BASTION, EN_SWARM, EN_SWARM, EN_SWARM, EN_RUSHER, EN_DRONE };
        static const float at[8][3] = { { -8, 9, 6 }, { 10, 13, 0 }, { -13, 1.4f, 24 }, { -3, 4, 18 }, { -2, 4.5f, 19 },
                                        { -4, 3.8f, 19 }, { 6, 2, 20 }, { 12, 7, 10 } };
        for (int k = 0; k < 8; k++) { int i = spawn_enemy_at(ty[k], v3(at[k][0], at[k][1], at[k][2]), 0); if (i >= 0) E[i].fireT = 100; }
        E[1].st = 1; E[1].stT = 0.4f; E[1].aim = vnorm(vsub(pcenter(), E[1].pos));
        if (strstr(path, "laser")) { E[1].phase = 1; E[1].stT = 0.25f; }   /* SENTINELLE : laser fige (rayon blanc, le claquement est proche) */
        E[2].yaw = atan2f(P.pos.x - E[2].pos.x, P.pos.z - E[2].pos.z);
        vgQn = 12; vgQi = 4;
        vg_shot_look(v3(0, 5, 10));
    }
    msgT = 0;                                             /* le grand titre du boss cacherait la scene */
    gameTime = 3;
    render(1, TICK);
    save_bmp(path);
    return 1;
}
