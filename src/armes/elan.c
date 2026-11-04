/* VELOCITY - Elan (convertisseur cinetique)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- Elan (convertisseur cinetique) ------------------------------------ */
/* Le tir convertit la vitesse horizontale (15 m/s au plus) en degats, et le tireur la perd.
 * Dash-tir : dans les 0,10 s apres un dash, toute la vitesse compte ; ensuite et jusqu'a 0,6 s,
 * la vitesse ajoutee par le dash ne compte plus (on prend la vitesse d'avant le dash). */
static int kin_in_window(void) { return P.kinDashT <= KIN_DASH_WINDOW + TICK * 0.5f; }   /* 12 ticks */
static float kin_vdep(void) {
    float v = fminf(hlen(P.vel), KIN_SPEED_CAP);
    if (!kin_in_window() && P.kinDashT < KIN_DASH_FORGET) v = fminf(v, P.kinPreDash);   /* trop tard */
    return v;
}
static float kin_dmg(float v) { return KIN_DMG_BASE + KIN_DMG_PER_MS * v; }
static V3 kin_col(float e) { return vlerp(v3(0.45f, 0.45f, 0.55f), v3(1.0f, 0.3f, 0.9f), clampf(e, 0, 1)); }

/* retire v a la norme horizontale, direction conservee, vel.y intact */
static void kin_brake(float v) {
    float hs = hlen(P.vel);
    if (hs < 1e-4f) return;
    float k = fmaxf(0, hs - v) / hs;
    P.vel.x *= k; P.vel.z *= k;
}

/* rayon de l'Elan (local et evenement reseau) : trait gris -> magenta, spirale et etincelles selon l'energie */
static void kin_beam_fx(V3 a, V3 b, V3 n, float e) {
    V3 col = kin_col(e);
    add_beam(a, b, col, 0.25f + 0.55f * e);
    if (e >= KIN_FULL / KIN_SPEED_CAP) add_beam(a, b, v3(1, 1, 1), 0.15f);   /* tir plein : rayon double */
    V3 bd = vsub(b, a); float bl = vlen(bd);
    if (e >= 0.15f && bl > 0.01f) {                       /* spirale facon Quake, plus large et plus dense avec l'energie */
        bd = vmul(bd, 1.0f / bl);
        V3 u = vnorm(vcross(bd, v3(0, 1, 0.01f))), w = vcross(bd, u);
        float rad = 0.06f + 0.14f * e, step = 0.6f - 0.3f * e;
        for (float s = 0; s < bl && s < 120; s += step) {
            float ang = s * 2.2f;
            V3 off = vadd(vmul(u, cosf(ang) * rad), vmul(w, sinf(ang) * rad));
            add_part(vadd(vadd(a, vmul(bd, s)), off), vmul(off, 4), col, 0.6f, 0.06f, 0);
        }
    }
    float sp = 3 + 5 * e;
    for (int i = 0; i < (int)(6 + 20 * e); i++)
        add_part(b, vadd(vmul(n, sp), vmul(rand_dir(), sp)), col, 0.45f, 0.1f, 12);
}

static void fire_kinetic(void) {
    if (P.ammo[WP_KINETIC] > 0) P.ammo[WP_KINETIC]--;
    int dashShot = kin_in_window();                       /* pour le HUD et le journal */
    float v = kin_vdep(), e = v / KIN_SPEED_CAP, mult = streak_mult();
    float dmg = kin_dmg(v) * mult, kn = KIN_KNOCK + KIN_KNOCK_PER_MS * v;
    V3 h0 = v3(P.vel.x, 0, P.vel.z), end, n, m = muzzle();
    chain_begin();                                        /* chaine de tirs pleine : ce tir peut tuer */
    int hit = hitscan_shot(dmg, kn, kn * 0.5f, W_KINETIC, &end, &n);
    kin_brake(v);                                         /* le cout, touche ou non */
    P.kinPreDash = fmaxf(0, P.kinPreDash - v);            /* la vitesse d'avant le dash est depensee */
    shot_result(hit);
    if (botMode) netlog("tir elan : v=%.1f degats=%.0f %s%s%s", v, dmg, hit ? "touche" : "rate", shotHeads ? " (tete)" : "",
                        P.kinDashT < KIN_DASH_FORGET ? (dashShot ? " (dash-tir)" : " (dash trop tot)") : "");
    chain_end(hit);
    kin_beam_fx(m, end, n, e);
    if (dmMode) net_fx_kinetic(m, end, e);
    for (int i = 0; i < (int)(6 + 18 * e); i++)           /* ton elan continue sans toi */
        add_part(m, vadd(vmul(h0, frange(0.5f, 0.9f)), vmul(rand_dir(), 2)), kin_col(e), 0.4f, 0.08f, 0);
    P.kinE = e; P.kinT = 0.3f;
    sun_hit_check(eye_pos(), end, hit, dmg);              /* easter egg : tir dans le soleil */
    P.fireCd = KIN_CD / streak_rate();
    P.recoil = 0.6f + 0.8f * e; shake += 0.04f + 0.12f * e; P.fovKick -= 8 * e;
    muzzleT = 0.07f;
    play(S_KIN, 0.45f + 0.45f * e, 0.85f + 0.3f * e);
    if (hit) {
        hitMarker = 0.18f;
        play(S_HIT, 0.6f, 1.0f + 0.05f * (shotStreak < STREAK_MAX ? shotStreak : STREAK_MAX));
        if (v >= KIN_FULL) { hitstop = fmaxf(hitstop, 0.05f); impact_frame(end, 0.05f); }
        if (dashShot) { snprintf(popText, sizeof popText, "DASH-TIR"); popT = 1.0f; }
    }
}

/* degats du prochain tir d'Elan (PvP si LAN, plafonnes), *e = energie 0..1 */
static float kin_preview(float *e) {
    float v = kin_vdep(), d = kin_dmg(v) * streak_mult();
    *e = v / KIN_SPEED_CAP;
    return dmMode ? fminf(d * PVP_SCALE, PVP_HIT_CAP) : d;
}
/* --- fin Elan ----------------------------------------------------------- */
