/* VELOCITY - degats, kills, explosions, serie de tirs, parry
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Combat                                                              */
/* ------------------------------------------------------------------ */
/* rayon et couleur par type (EN_*) ; nom pour l'ecran de mort et la barre de boss */
static const float enemyRad[EN_COUNT] = { 0.9f, 0.75f, 0.85f, 0.5f, 1.25f, 3.2f, 4.2f, 3.5f, 3.8f };
static const float enemyCol[EN_COUNT][3] = {
    { 1.0f, 0.25f, 0.3f }, { 1.0f, 0.6f, 0.15f }, { 0.35f, 1.0f, 0.55f }, { 1.0f, 0.95f, 0.3f }, { 0.55f, 0.6f, 1.0f },
    { 1.0f, 0.62f, 0.18f }, { 0.55f, 0.45f, 1.0f }, { 1.0f, 0.3f, 0.45f }, { 0.9f, 0.95f, 1.0f },
};
static const char *enemyName[EN_COUNT] = { "DRONE", "RUSHER", "SENTINELLE", "ESSAIM", "BASTION",
                                           "PYRAMIDE", "TESSERACT", "SPHERE", "HEXAEDRE" };
static float vgBossScale = 1;                             /* boss plus petits dans une salle basse (LE DOJO) */
static float enemy_radius(int type) {
    return type >= 0 && type < EN_COUNT ? enemyRad[type] * (EN_BOSS(type) ? vgBossScale : 1) : 0.75f;
}
static V3 enemy_color(int type) {
    const float *c = enemyCol[type >= 0 && type < EN_COUNT ? type : 1];
    return v3(c[0], c[1], c[2]);
}

#define FALL_DEATH_Y -25.0f                               /* sous cette hauteur (hors de la carte), on meurt */
static int fellOut;                                       /* deja mort de chute pendant cette vie (une seule touche) */
static void damage_player(float dmg) {
    if (P.dead) return;
    if (dmMode) { net_hit(myId, dmg, v3(0, 0, 0), W_SELF); return; }  /* en LAN, l'hote tient les PV */
    P.hp -= dmg; P.hurtFlash = 0.6f; shake += 0.25f;
    play(S_HURT, 0.8f, frange(0.9f, 1.1f));
    if (P.hp <= 0) {
        P.hp = 0; P.dead = 1; P.deathT = 0;
        if (!fellOut) killcam_start(-1, 0);               /* solo : camera vers l'ennemi le plus proche */
        saw_remove_owner(myId);                           /* Scie : elle disparait, neuve au prochain depart */
        if (score > bestScore) bestScore = score;
    }
}

/* --- Egide : effets et bouclier du joueur local (solo : orbes et kamikazes ; le LAN passe par l'hote) --- */
static int shieldBlocks, shieldBreaks, shieldPerfects, shieldDenied, shieldConverted, shieldRips;   /* journal (hote) */
static int shieldBashes, shieldBashHits, shieldRams, shieldRaises;                                 /* journal (moi) */
static float shieldUpSum;                                 /* duree totale bouclier leve (moi) */
static void shield_spark_fx(V3 at, V3 dir) {              /* etincelles cyan-blanc au point d'impact */
    for (int k = 0; k < 16; k++)
        add_part(at, vadd(vmul(dir, -3), vmul(rand_dir(), 6)), k & 1 ? v3(0.5f, 0.9f, 1) : v3(1, 1, 1), 0.3f, 0.08f, 6);
}
static void shield_break_fx(V3 at) {
    shards(at, v3(0.4f, 0.75f, 0.9f), 14, 8);
    burst(at, v3(0.6f, 0.9f, 1), 30, 8, 0.4f, 0.1f);
}
static int shield_local_faces(V3 dir) {
    return P.meleeKind == MK_SHIELD && P.shieldState == SHS_UP && !P.dead && vdot(dir, view_dir()) < SHIELD_COS;
}
static void shield_local_break(void) {                    /* jauge vide : brise 3 s */
    P.shieldGauge = 0; P.shieldState = SHS_BROKEN; P.shieldBrokenT = SHIELD_BROKEN_T;
    shield_break_fx(vadd(eye_pos(), vmul(view_dir(), 0.6f)));
    play(S_SHIELD_BREAK, 0.9f, 1.0f);
    snprintf(popText, sizeof popText, "BOUCLIER BRISE"); popT = 1.2f;
}
static void shield_absorb_local(float x, V3 dir) {        /* solo : le bouclier prend le coup */
    P.shieldGauge -= x; P.shieldHitT = 0.2f;
    shield_spark_fx(vadd(eye_pos(), vmul(view_dir(), 0.6f)), dir);
    play(S_SHIELD_BLOCK, 0.8f, frange(0.95f, 1.05f));
    shake += 0.1f;
    if (P.shieldGauge <= 0) shield_local_break();
}
/* --- fin Egide --- */

static void kill_enemy(int i, int byPlayer) {
    Enemy *e = &E[i];
    e->alive = 0;
    burst(e->pos, enemy_color(e->type), 60, 12, 0.9f, 0.18f);
    shards(e->pos, enemy_color(e->type), 12, 10);
    if (byPlayer) { impact_frame(e->pos, 0.07f); killMarkT = 0.45f; }
    burst(e->pos, v3(1, 1, 0.8f), 20, 6, 0.4f, 0.25f);
    if (!byPlayer) { play(S_EXPLO, 0.6f, 1.4f); return; }
    play(S_KILL, 0.6f, 1.0f + 0.06f * (combo < 8 ? combo : 8));
    kills++;
    combo++; comboT = 3.0f;
    float mult = (1 + rel_speed(e->vel) / 15.0f) * (1 + 0.25f * (combo - 1));
    static const int basePts[EN_COUNT] = { 100, 60, 120, 30, 200, 3000, 4000, 5000, 10000 };
    int pts = (int)(basePts[e->type >= 0 && e->type < EN_COUNT ? e->type : 1] * mult * style_mult());   /* le style multiplie les points */
    style_kill(-1);
    score += pts;
    P.hp = fminf(100, P.hp + (e->type == EN_DRONE || e->type == EN_SNIPER || e->type == EN_BASTION ? 15 : 8));
    snprintf(popText, sizeof popText, "+%d", pts);
    popT = 1.2f;
    if (vagOn) vg_on_kill(i);                             /* VAGUES : credits, effets de la boutique */
}

static void damage_enemy(int i, float dmg, V3 knock) {
    Enemy *e = &E[i];
    if (!e->alive) return;
    int charged = P.chargeT > 0, marked = markE[i] > 0;   /* VAGUES : contextes des chemins CHARGE et MARQUE */
    dmg = skill_out(-1, i, dmg);                          /* Competences : MARQUE, CHARGE */
    if (vagOn) dmg = vg_dmg_filter(i, dmg);               /* VAGUES : armure des boss, bouclier du bastion */
    e->hp -= dmg; e->flash = 0.12f; e->lastDmg = dmg;
    dmg_number(e->pos, dmg);
    if (!EN_BOSS(e->type)) e->vel = vadd(e->vel, knock);  /* un boss ne recule pas */
    if (vagOn && dmg > 0) vg_on_hit(i, dmg, charged, marked);
    if (e->hp <= 0 && e->alive) kill_enemy(i, 1);
}

/* byPlayer : 0 = ennemi, 1 = joueur, 2 = joueur sans recul ni degat sur soi (projectile renvoye).
 * knock : recul des joueurs (et de soi) au centre ; weapon : W_* des touches LAN ; skipEnemy / skipPlayer : cible deja
 * servie (mine collee : degats garantis) ; netId : objet source, joint aux EV_HIT (arbitrage des mines).
 * Renvoie le nombre d'ennemis touches. */
static int explode_ex(V3 c, float radius, float dmg, int byPlayer, float knock, int weapon, int skipEnemy, int skipPlayer,
                      unsigned int netId) {
    fx_explosion(c, radius);
    burst(c, v3(1.0f, 0.55f, 0.15f), 90, 14, 0.7f, 0.3f);
    burst(c, v3(1.0f, 0.9f, 0.5f), 30, 7, 0.35f, 0.5f);
    play(S_EXPLO, 0.9f, frange(0.9f, 1.1f));
    float dist = vlen(vsub(pcenter(), c));
    shake += fmaxf(0, 0.5f - dist * 0.02f);
    int hit = 0;
    for (int i = 0; i < MAXE; i++) {
        if (!E[i].alive || i == skipEnemy) continue;
        V3 dv = vsub(E[i].pos, c);
        float d = vlen(dv), R = radius + enemy_radius(E[i].type);
        if (d < R) { float f = 1 - d / R; damage_enemy(i, dmg * (0.3f + 0.7f * f), vmul(vnorm(dv), 12 * f)); hit++; }
    }
    for (int i = 0; byPlayer && i < MAXP; i++) {         /* LAN : le tireur calcule le souffle sur les autres */
        if (!np_target(i) || i == skipPlayer) continue;
        V3 dv = vsub(np_center(i), c);
        float d = vlen(dv), R = radius + 0.5f;
        if (d < R) {
            float f = 1 - d / R;
            hit_player_ex(i, dmg * (0.3f + 0.7f * f) * PVP_SCALE, vmul(vnorm(dv), knock * f + 3), weapon, vnorm(dv), SHB_BLAST,
                          netId, 0, 0, 0);                /* Egide : souffle de face, 50 % bloques (la moitie passe : touche) */
            hit++;
        }
    }
    if (hit && byPlayer) { hitMarker = 0.18f; play(S_HIT, 0.6f, 1.0f); }
    if (!P.dead && dist < radius && byPlayer != 2) {
        float f = 1 - dist / radius;
        V3 dir = vnorm(vsub(pcenter(), c));
        P.vel = vadd(P.vel, vmul(dir, knock * f + 3));
        if (dir.y > 0.3f) P.grounded = 0;
        if (byPlayer && dmMode && netId) net_hit_id(myId, 5 * f, v3(0, 0, 0), W_SELF, netId);   /* annule avec la mine */
        else if (byPlayer) damage_player(5 * f);
        else damage_player(30 * f);
    }
    return hit;
}
static int explode(V3 c, float radius, float dmg, int byPlayer) {
    return explode_ex(c, radius, dmg, byPlayer, ROCKET_KNOCK, byPlayer == 2 ? W_PARRY : W_ROCKET, -1, -1, 0);
}

static float atten(V3 p) { return clampf(1.2f - vlen(vsub(p, P.pos)) / 60.0f, 0.1f, 1.0f); }

/* explosion purement visuelle : roquette d'un autre joueur (c'est son tireur qui gere les degats) */
static void explode_fx(V3 c) {
    fx_explosion(c, ROCKET_RADIUS);
    burst(c, v3(1.0f, 0.55f, 0.15f), 90, 14, 0.7f, 0.3f);
    burst(c, v3(1.0f, 0.9f, 0.5f), 30, 7, 0.35f, 0.5f);
    play(S_EXPLO, 0.9f * atten(c), frange(0.9f, 1.1f));
}

static float streak_mult(void) { return 1 + STREAK_DMG * (float)(shotStreak < STREAK_MAX ? shotStreak : STREAK_MAX); }
static float streak_rate(void) { return 1 + STREAK_RATE * (float)(shotStreak < STREAK_MAX ? shotStreak : STREAK_MAX); }
static void shot_result(int hit) {
    if (hit) shotStreak++;
    else { if (shotStreak >= 2) streakLostT = 1.0f; shotStreak = 0; }
}

static void parry_feedback(V3 where) {
    impact_frame(where, 0.1f);
    hitstop = fmaxf(hitstop, 0.12f);
    parryFlash = 0.35f;
    shake += 0.2f;
    if (!dmMode) P.hp = fminf(100, P.hp + PARRY_HEAL);    /* en LAN c'est l'hote qui soigne */
    P.dashCharge = fminf(2, P.dashCharge + 1);            /* recompense : un dash et le grappin recharges */
    P.hookCd = 0;
    snprintf(popText, sizeof popText, "PARRY");
    popT = 1.0f;
    for (int k = 0; k < 30; k++) add_part(where, vmul(rand_dir(), 9), v3(1, 1, 0.85f), 0.4f, 0.12f, 0);
    play(S_PARRY, 0.9f, 1.0f);
    style_parry();
    if (vagOn) vg_on_parry(where);                        /* VAGUES : ONDE, MIROIR, STASE de la boutique */
}
