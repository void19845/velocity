/* VELOCITY - deplacement et impact des projectiles
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static void proj_tick(float dt) {
    for (int i = 0; i < MAXPR; i++) {
        Proj *p = &PR[i];
        if (!p->alive) continue;
        if (p->kind != PR_SAW && p->pos.y < maps[curMap].killY - 2) { p->alive = 0; continue; }   /* tombe dans le vide */
        if (vagOn) {                                      /* VAGUES : a quel chemin de la boutique reviennent ses touches */
            vgProjKind = p->kind;
            vgSrc = p->kind == PR_ROCKET ? WP_ROCKET : p->kind == PR_SAW ? WP_SAW : p->kind == PR_MINE ? WP_MINE :
                    p->kind == PR_PARRIED ? (p->mineState ? WP_MINE : WP_COUNT + P.meleeKind) : -1;
        }
        if (p->kind == PR_SAW) { saw_tick(p, dt); continue; }   /* Scie : sa propre machine a etats */
        if (p->kind == PR_MINE) { mine_tick(p, dt); continue; } /* Ventouse */
        p->life -= dt;
        V3 step = vmul(p->vel, dt);
        float len = vlen(step);
        V3 dir = vmul(step, 1.0f / len), n;
        float t = ray_world(p->pos, dir, len, &n);
        int hitEnemy = -1, hitPlayer = -1;
        if (p->kind == PR_ROCKET || p->kind == PR_PARRIED) {
            int mine = !dmMode || p->owner == myId;       /* seul le tireur calcule les degats */
            for (int j = 0; j < MAXE; j++) {
                float te;
                if (E[j].alive && ray_sphere(p->pos, dir, E[j].pos, enemy_radius(E[j].type) + 0.2f, t, &te)) { t = te; hitEnemy = j; }
            }
            for (int j = 0; dmMode && j < MAXP; j++) {    /* LAN : collision avec les joueurs */
                V3 mn, mx; float te;
                if (j == p->owner || !NP[j].active || !NP[j].alive || same_team(j, p->owner)) continue;
                if (j == myId) {
                    if (P.dead) continue;
                    paabb(P.pos, pheight(), &mn, &mx);
                    mn = vsub(mn, v3(0.2f, 0.2f, 0.2f)); mx = vadd(mx, v3(0.2f, 0.2f, 0.2f));
                } else np_aabb(j, 0.2f, &mn, &mx);
                if (ray_box(p->pos, dir, mn, mx, t, &te, NULL) && te < t) { t = te; hitPlayer = j; }
            }
            if (p->ricLeft > 0 && p->kind == PR_ROCKET && t < len && hitEnemy < 0 && hitPlayer < 0 && p->life > 0) {
                V3 at = vadd(p->pos, vmul(dir, t));       /* RICOCHET : le decor renvoie la roquette */
                p->vel = vmul(vsub(p->vel, vmul(n, 2 * vdot(p->vel, n))), RIC_ROCKET_SLOW);
                p->pos = vadd(at, vmul(n, 0.05f));
                p->ricLeft--;
                burst(at, v3(0.4f, 1, 0.7f), 14, 5, 0.3f, 0.09f);
                play(S_LAND, 0.5f * atten(at), 1.6f);
                if (mine) ricStat[1]++;
                continue;
            }
            if (t < len || p->life <= 0) {
                V3 c = vadd(vadd(p->pos, vmul(dir, t)), vmul(n, 0.15f));
                p->alive = 0;
                if (!mine) { explode_fx(c); continue; }
                if (dmMode) net_fx_rocket_end(p->netId, c);
                /* contre un joueur, l'impact direct passe par le souffle (une seule touche, plafonnee) */
                if (p->kind == PR_PARRIED && p->orb && p->big) {   /* VAGUES : grosse orbe renvoyee */
                    if (hitEnemy >= 0 && EN_BOSS(E[hitEnemy].type)) vg_boss_stun(hitEnemy, VG_BIG_ORB_RETURN);
                    else if (hitEnemy >= 0) damage_enemy(hitEnemy, VG_BIG_ORB_RETURN, vmul(dir, 10));
                    explode(c, 5.0f, 120, 2);
                } else if (p->kind == PR_PARRIED && p->orb) {
                    if (hitEnemy >= 0) damage_enemy(hitEnemy, PARRY_DMG, vmul(dir, 10));
                    explode(c, 3.0f, 80, 2);
                } else if (p->kind == PR_PARRIED && p->mineState) {   /* --- Ventouse : mine renvoyee (souffle de mine) --- */
                    if (hitEnemy >= 0) damage_enemy(hitEnemy, 40 * p->mult, vmul(dir, 10));
                    explode_ex(c, MINE_RADIUS, MINE_DMG * p->mult, 2, MINE_KNOCK, W_PARRY, -1, -1, p->netId);
                } else if (p->kind == PR_PARRIED) {
                    if (hitEnemy >= 0) damage_enemy(hitEnemy, 40 * p->mult, vmul(dir, 10));
                    explode(c, ROCKET_RADIUS, ROCKET_DMG * p->mult, 2);
                } else {
                    float mult = streak_mult();
                    int hits = hitEnemy >= 0 || hitPlayer >= 0;
                    if (hitEnemy >= 0) damage_enemy(hitEnemy, 40 * mult, vmul(dir, 6));
                    hits += explode(c, ROCKET_RADIUS, ROCKET_DMG * mult, 1);
                    /* un rocket jump qui ne touche personne ne casse pas la chaine */
                    if (hits || vlen(vsub(c, pcenter())) > ROCKET_RADIUS + 1) shot_result(hits);
                }
                p->alive = 0;
                continue;
            }
            if (p->kind == PR_PARRIED)
                add_part(p->pos, vmul(rand_dir(), 0.6f), v3(1.0f, 0.9f, 0.5f), 0.35f, 0.15f, 0);
            else if (p->life < 5.95f)                         /* pas de trainee dans l'oeil du joueur */
                add_part(p->pos, vmul(rand_dir(), 0.8f), v3(1.0f, 0.5f, 0.15f), 0.45f, 0.18f, -1);
        } else {
            V3 mn, mx; paabb(P.pos, pheight(), &mn, &mx);
            V3 q = vadd(p->pos, step);
            if (!P.dead && q.x > mn.x - 0.25f && q.x < mx.x + 0.25f && q.y > mn.y - 0.25f && q.y < mx.y + 0.25f &&
                q.z > mn.z - 0.25f && q.z < mx.z + 0.25f) {
                float od = p->big ? VG_BIG_ORB_DMG : VG_ORB_DMG;   /* degats fixes, a toutes les vagues */
                if (shield_local_faces(dir)) {            /* Egide : l'orbe s'ecrase sur le bouclier */
                    shield_absorb_local(od, dir);
                    burst(q, v3(1, 0.3f, 0.9f), 20, 5, 0.4f, 0.12f);
                    p->alive = 0;
                    continue;
                }
                damage_player(od);
                P.vel = vadd(P.vel, vmul(dir, p->big ? 12.0f : 3.0f));
                burst(q, v3(1, 0.3f, 0.9f), p->big ? 60 : 20, p->big ? 12.0f : 5.0f, 0.4f, 0.12f);
                if (p->big) fx_explosion(q, 4);
                p->alive = 0;
                continue;
            }
            if (t < len || p->life <= 0) {
                V3 at = vadd(p->pos, vmul(dir, t));
                burst(at, v3(1, 0.3f, 0.9f), p->big ? 60 : 14, p->big ? 10.0f : 4.0f, 0.4f, 0.1f);
                if (p->big) {                             /* grosse orbe : souffle a l'impact (degats fixes) */
                    fx_explosion(at, 4);
                    play(S_EXPLO, 0.8f * atten(at), 0.8f);
                    float d = vlen(vsub(pcenter(), at));
                    if (d < 4.5f && !P.dead) { damage_player(VG_BIG_ORB_DMG * 0.5f); P.vel = vadd(P.vel, vmul(vnorm(vsub(pcenter(), at)), 10)); }
                }
                p->alive = 0;
                continue;
            }
            if (frand() < 0.5f) add_part(p->pos, vmul(rand_dir(), 0.3f), v3(1, 0.3f, 0.9f), 0.3f, 0.12f, 0);
        }
        p->pos = vadd(p->pos, step);
    }
    vgProjKind = -1; vgSrc = -1;
}
