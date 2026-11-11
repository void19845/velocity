/* VELOCITY - epee et aiguillage des armes de melee
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static void melee(void) {
    if (P.meleeKind == MK_ANVIL) { anvil_press(); return; }
    if (P.meleeKind == MK_FLAIL) { flail_swing(); return; }
    if (P.meleeKind == MK_FIST) { fist_punch(); return; }   /* Poings : un coup (le combo vit dans fist_tick) */
    float sp = vlen(P.vel);
    V3 eye = eye_pos(), f = view_dir();
    V3 r = v3(cosf(P.yaw), 0, sinf(P.yaw)), u = vcross(r, f);
    int hits = 0, parried = 0;

    melee_start_swing();
    parried = try_parry(MELEE_RANGE + 1.5f, 1);

    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &E[i];
        if (!e->alive) continue;
        float rel = rel_speed(e->vel);
        V3 d = vsub(e->pos, eye);
        float dist = vlen(d), reach = dist - enemy_radius(e->type);
        if (reach > melee_range(rel)) continue;
        if (reach > 0.8f && vdot(vmul(d, 1.0f / dist), f) < MELEE_CONE) continue;
        if (e->type == EN_RUSHER) {                       /* kamikaze pare : explose dans sa face */
            V3 at = e->pos;
            damage_enemy(i, 9999, vmul(f, 10));
            parry_feedback(at);
            melee_parry_points(PTS_PARRY);                /* compte comme un parry, sans bonus de kill */
            parried++;
        } else {
            damage_enemy(i, melee_dmg(rel), vmul(f, 8 + rel * 0.5f));
            if (!e->alive) {
                if (!vagOn) P.hp = fminf(100, P.hp + 15);   /* bonus de soin des kills au corps a corps (supprime en VAGUES) */
                ammo_points(PTS_MELEE_KILL, AP_KILL);
            }
        }
        hits++;
    }

    for (int i = 0; i < MAXP; i++) {                     /* LAN : les autres joueurs */
        if (!np_target(i)) continue;
        float rel = rel_speed(np_vel(i));
        V3 c = np_center(i), d = vsub(c, eye);
        float dist = vlen(d), reach = dist - 0.5f;
        if (reach > melee_range(rel)) continue;
        if (reach > 0.8f && vdot(vmul(d, 1.0f / dist), f) < MELEE_CONE) continue;
        if (!hit_player(i, melee_dmg(rel) * PVP_SCALE, vadd(vmul(f, 8 + rel * 0.5f), v3(0, 3, 0)), W_MELEE, vnorm(d))) {
            parried++;                                    /* Egide : bloque = ni dash ni points, mais pas un coup dans le vide */
            continue;
        }
        burst(c, v3(1, 0.8f, 0.5f), 20, 8, 0.3f, 0.12f);
        hits++;
    }

    float range = melee_range(sp);
    for (float a = -0.9f; a <= 0.9f; a += 0.1f) {        /* arc de la frappe */
        V3 p = vadd(eye, vadd(vmul(f, range * 0.55f + 0.6f), vadd(vmul(r, sinf(a) * range * 0.45f), vmul(u, -a * 0.3f))));
        add_part(p, vmul(f, 3), sp > 15 ? v3(1, 0.7f, 0.3f) : v3(0.9f, 0.9f, 1), 0.15f, 0.07f + sp * 0.002f, 0);
    }
    if (dmMode) net_fx_melee(eye, f, sp);
    P.meleeCd = MELEE_CD;
    P.meleeT = 0.25f;
    P.parryWin = PARRY_WINDOW;                            /* la fenetre de parry reste ouverte un court instant */
    P.swingLanded = hits || parried;
    play(S_DASH, 0.4f, 1.8f);
    if (hits) {
        melee_hit_points(meleePts[P.meleeKind]);          /* la melee remplit les jauges des armes a distance */
        P.dashCharge = fminf(2, P.dashCharge + 1);        /* toucher au corps a corps rend un dash */
        if (sp > 18) impact_frame(vadd(eye, vmul(f, 2)), 0.05f);
        hitMarker = 0.18f;
        shake += 0.15f + sp * 0.006f;
        if (!parried) hitstop = fmaxf(hitstop, 0.04f + sp * 0.0015f);
        play(S_PUNCH, 0.9f, frange(0.9f, 1.1f));
    }
}
