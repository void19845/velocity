/* VELOCITY - physique du joueur : bhop, slide, dash, wall-run, grappin
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Joueur                                                              */
/* ------------------------------------------------------------------ */
static float pheight(void) { return P.crouch ? H_CROUCH : H_STAND; }
static void paabb(V3 pos, float h, V3 *mn, V3 *mx) {
    *mn = v3(pos.x - HW, pos.y, pos.z - HW);
    *mx = v3(pos.x + HW, pos.y + h, pos.z + HW);
}
static int pos_free(V3 pos, float h) {
    V3 mn, mx; paabb(pos, h, &mn, &mx);
    for (int i = 0; i < nb; i++) if (box_overlap(mn, mx, &boxes[i])) return 0;
    return 1;
}
static V3 view_dir(void) {
    float cp = cosf(P.pitch);
    return v3(sinf(P.yaw) * cp, sinf(P.pitch), -cosf(P.yaw) * cp);
}
static V3 eye_pos(void) { return v3(P.pos.x, P.pos.y + P.eyeH, P.pos.z); }
static V3 pcenter(void) { return v3(P.pos.x, P.pos.y + pheight() * 0.5f, P.pos.z); }

static void move_axis(int ax, float d, int canStep) {
    if (d == 0) return;
    float h = pheight();
    V3 mn, mx;
    *vc(&P.pos, ax) += d;
    for (int i = 0; i < nb; i++) {
        Box *b = &boxes[i];
        paabb(P.pos, h, &mn, &mx);
        if (!box_overlap(mn, mx, b)) continue;
        if (ax == 1) {
            if (d < 0) { P.pos.y = b->mx.y; if (P.vel.y < 0) P.vel.y = 0; P.grounded = 1; P.groundBox = i; }
            else { P.pos.y = b->mn.y - h - 0.001f; if (P.vel.y > 0) P.vel.y = 0; }
        } else {
            float rise = b->mx.y - P.pos.y;
            /* un jump pad se monte meme en l'air (bhop) : sinon ses 0,3 m de haut agissent comme un mur plein
             * tant qu'on n'est pas deja au sol (canStep), et a vitesse elevee on arrive dessus en l'air la
             * plupart du temps -> on se fait arreter a son bord au lieu de monter dessus et de le declencher */
            if ((canStep || b->kind == B_PAD) && rise > 0 && rise <= STEP_H) {
                V3 np = P.pos; np.y = b->mx.y + 0.001f;
                if (pos_free(np, h)) { P.stepOfs -= rise; P.pos = np; P.stairT = STAIR_RAMP_T; continue; }
            }
            if (d > 0) *vc(&P.pos, ax) = *vc(&b->mn, ax) - HW - 0.001f;
            else       *vc(&P.pos, ax) = *vc(&b->mx, ax) + HW + 0.001f;
            *vc(&P.vel, ax) = 0;
        }
    }
}

static void wall_check(void) {
    static const float dirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    P.wallTouch = 0;
    if (P.grounded) return;
    float h = pheight(), best = 2;
    V3 hv = vnorm(v3(P.vel.x, 0, P.vel.z));
    for (int d = 0; d < 4; d++) {
        V3 p = P.pos, mn, mx;
        p.x += dirs[d][0] * 0.12f; p.z += dirs[d][1] * 0.12f;
        paabb(p, h, &mn, &mx);
        mn.y += 0.3f;
        for (int i = 0; i < nb; i++)
            if (box_overlap(mn, mx, &boxes[i])) {
                V3 n = v3(-dirs[d][0], 0, -dirs[d][1]);
                float along = fabsf(vdot(n, hv));   /* on prefere le mur longe */
                if (along < best) { best = along; P.wallTouch = 1; P.wallN = n; }
                break;
            }
    }
}

static void accelerate(V3 wish, float wishspeed, float accel, float cap, float dt) {
    float ws = wishspeed > cap ? cap : wishspeed;
    float add = ws - vdot(P.vel, wish);
    if (add <= 0) return;
    float a = accel * wishspeed * dt;
    if (a > add) a = add;
    P.vel.x += wish.x * a;
    P.vel.z += wish.z * a;
}

static void hook_release(int jumpBoost) {
    if (P.hook == HOOK_ATTACHED) {
        P.airJumps = 1;
        P.hookCd = GRAPPLE_CD;
        if (jumpBoost) {
            P.vel.y = fmaxf(P.vel.y, 0) + GRAPPLE_JUMP;
            P.fovKick += 4;
            play(S_JUMP, 0.5f, 1.2f);
        }
        play(S_HOOK, 0.35f, 1.7f);
    } else if (P.hook == HOOK_FLYING) {
        P.hookCd = 0.2f;
    }
    P.hook = HOOK_NONE;
    P.hookEnemy = 0;
    P.hookPlayer = 0;
}

static float enemy_radius(int type);
static void damage_enemy(int i, float dmg, V3 knock);
static void slam_land(float vyImpact);                    /* Enclume : definies avec la melee */
static void slam_contact(void);
static void slam_wave_tick(float dt);
static void shield_ram(void);                             /* Egide : belier (definie avec la melee) */

/* Apres l'impact sur la cible accrochee : rebond vers le haut avec le grappin recharge,
 * pour enchainer sur la suivante. */
static void grapple_bounce(V3 hit, V3 dir) {
    for (int k = 0; k < 40; k++)
        add_part(hit, vadd(vmul(dir, 6), vmul(rand_dir(), 8)), v3(1, 0.85f, 0.4f), 0.5f, 0.15f, 6);
    shake += 0.3f;
    hitMarker = 0.18f;
    P.fovKick += 8;
    play(S_HOOKHIT, 0.9f, 0.6f);
    play(S_HIT, 0.7f, 0.8f);
    P.vel = vadd(vmul(v3(P.vel.x, 0, P.vel.z), 0.35f), v3(0, 9, 0));
    P.grounded = 0;
    P.airJumps = 1;
    P.hook = HOOK_NONE;
    P.hookEnemy = 0;
    P.hookPlayer = 0;
    P.hookCd = 0;
}

/* Grappin : le crochet vole jusqu'a un mur, puis tire le joueur vers l'ancre.
 * La corde ne se rallonge jamais : on peut se balancer autour du point d'accroche. */
/* --- Course : aimant du grappin vers les points de balancement (cubes flottants de <= 3 m, rien juste dessous) --- */
static unsigned char swingIs[MAXB];
static int swingMap = -1, swingNb = -1;
static int swing_box(int i) {
    if (swingMap != curMap || swingNb != nb) {            /* recalcule a chaque carte */
        swingMap = curMap; swingNb = nb;
        for (int a = 0; a < nb; a++) {
            Box *b = &boxes[a];
            V3 s = vsub(b->mx, b->mn);
            float m = 2 * SWING_HALF + 0.2f;
            int ok = b->kind == B_SOLID && s.x <= m && s.y <= m && s.z <= m;
            for (int j = 0; j < nb && ok; j++) {
                Box *c = &boxes[j];
                if (j != a && c->mx.x > b->mn.x && c->mn.x < b->mx.x && c->mx.z > b->mn.z && c->mn.z < b->mx.z &&
                    c->mx.y <= b->mn.y + 0.01f && c->mx.y > b->mn.y - 2) ok = 0;   /* pose sur quelque chose : pas une ancre */
            }
            swingIs[a] = (unsigned char)ok;
        }
    }
    return i >= 0 && i < nb && swingIs[i];
}
/* point de balancement le plus proche du viseur, dans le cone de l'aimant, en vue ; -1 si aucun */
static int swing_target(V3 eye, V3 dir, V3 *at) {
    int best = -1;
    float bk = RACE_MAGNET_COS;
    for (int i = 0; i < nb; i++) {
        if (!swing_box(i)) continue;
        V3 c = vmul(vadd(boxes[i].mn, boxes[i].mx), 0.5f), d = vsub(c, eye);
        float l = vlen(d);
        if (l < 3 || l > GRAPPLE_RANGE - 1) continue;
        float k = vdot(d, dir) / l;
        if (k <= bk) continue;
        if (ray_world(eye, vmul(d, 1 / l), l, 0) < l - 2 * SWING_HALF - 0.4f) continue;   /* cache par une tour */
        bk = k; best = i; *at = c;
    }
    return best;
}

static void grapple_tick(float dt, int press, int held) {
    P.hookCd -= dt;
    if (!hook_allowed()) press = 0;                       /* carte sans grappin ou regle de l'hote */
    if (P.dead) { P.hook = HOOK_NONE; return; }
    if (press && P.hook == HOOK_NONE && P.hookCd <= 0) {
        P.hook = HOOK_FLYING;
        P.hookPos = eye_pos();
        P.hookDir = view_dir();
        if (raceOn) {                                     /* course : aimant vers le point de balancement vise */
            V3 at;
            if (swing_target(P.hookPos, P.hookDir, &at) >= 0) P.hookDir = vnorm(vsub(at, P.hookPos));
        }
        P.hookDist = 0;
        P.hookEnemy = 0;
        P.hookPlayer = 0;
        play(S_HOOK, 0.6f, 1.0f);
    }
    if (P.hook != HOOK_NONE && !held) { hook_release(0); return; }

    if (P.hook == HOOK_FLYING) {
        float len = GRAPPLE_SPEED * dt;
        V3 n;
        float t = ray_world(P.hookPos, P.hookDir, len, &n), te;
        int target = -1, ptarget = -1;
        int sawHit = saw_hook_test(&t);                   /* Scie : ma scie plantee, avant le mur (une cible plus proche l'emporte) */
        for (int j = 0; j < MAXE; j++)                    /* un ennemi devant le mur ? */
            if (E[j].alive && ray_sphere(P.hookPos, P.hookDir, E[j].pos, enemy_radius(E[j].type) + 0.3f, t, &te)) {
                t = te; target = j;
            }
        for (int j = 0; j < MAXP; j++) {                  /* ou un autre joueur (LAN) ? */
            V3 mn, mx;
            if (!np_target(j)) continue;
            np_aabb(j, 0.3f, &mn, &mx);
            if (ray_box(P.hookPos, P.hookDir, mn, mx, t, &te, NULL) && te < t) { t = te; target = -1; ptarget = j; }
        }
        if (sawHit && target < 0 && ptarget < 0) { saw_recall_start(); return; }   /* rappel : le joueur ne bouge pas */
        if (target >= 0 || ptarget >= 0) {
            P.hook = HOOK_ATTACHED;
            if (target >= 0) { P.hookEnemy = target + 1; P.hookPos = P.anchor = E[target].pos; E[target].flash = 0.12f; }
            else {
                P.hookPlayer = ptarget + 1; P.hookPos = P.anchor = np_center(ptarget);
                if (np_shield_up(ptarget)) shield_rip(ptarget);   /* Egide : l'accroche arrache le bouclier 1 s */
            }
            P.ropeLen = vlen(vsub(P.anchor, pcenter()));
            P.hookBlockT = 0;
            P.airJumps = 1;
            P.fovKick += 6;
            play(S_HOOKHIT, 0.8f, 1.4f);
        } else if (t < len) {
            P.hookPos = P.anchor = vadd(P.hookPos, vmul(P.hookDir, t));
            P.hook = HOOK_ATTACHED;
            P.ropeLen = vlen(vsub(P.anchor, pcenter()));
            P.hookBlockT = 0;
            P.airJumps = 1;
            P.fovKick += 5;
            for (int i = 0; i < 18; i++)
                add_part(P.anchor, vadd(vmul(n, 3), vmul(rand_dir(), 4)), v3(1, 0.85f, 0.4f), 0.4f, 0.1f, 10);
            play(S_HOOKHIT, 0.7f, 1.0f);
        } else {
            P.hookPos = vadd(P.hookPos, vmul(P.hookDir, len));
            P.hookDist += len;
            if (P.hookDist > GRAPPLE_RANGE) { P.hook = HOOK_NONE; P.hookCd = 0.2f; }
        }
        return;
    }

    if (P.hook == HOOK_ATTACHED) {
        Enemy *e = P.hookEnemy ? &E[P.hookEnemy - 1] : NULL;
        int pl = P.hookPlayer - 1;
        if (e) {                                          /* l'ancre suit l'ennemi */
            if (!e->alive) { hook_release(0); return; }
            P.hookPos = P.anchor = e->pos;
        }
        if (pl >= 0) {                                    /* ou le joueur accroche */
            if (!np_target(pl)) { hook_release(0); return; }
            P.hookPos = P.anchor = np_center(pl);
        }
        int onTarget = e || pl >= 0;
        float tr = e ? enemy_radius(e->type) : (pl >= 0 ? 0.5f : 0.1f);
        V3 d = vsub(P.anchor, pcenter());
        float dist = vlen(d);
        V3 dir = vmul(d, 1.0f / fmaxf(dist, 0.01f));
        if (onTarget && dist < tr + 1.6f) {               /* impact : degats selon la vitesse relative */
            if (P.meleeKind == MK_SHIELD && P.shieldState == SHS_UP) { shield_ram(); return; }   /* Egide : belier */
            float impact = rel_speed(e ? e->vel : np_vel(pl));
            float dmg = GRAPPLE_STRIKE_DMG + impact * GRAPPLE_STRIKE_PER_MS;
            V3 hit = P.anchor;
            if (e) damage_enemy(P.hookEnemy - 1, dmg, vmul(dir, 14));
            else hit_player(pl, dmg * PVP_SCALE, vadd(vmul(dir, 14), v3(0, 4, 0)), W_HOOK, dir);
            impact_frame(hit, 0.07f);
            grapple_bounce(hit, dir);
            return;
        }
        if (!onTarget && dist < 1.6f) { hook_release(0); return; }
        /* la corde casse si un obstacle la coupe trop longtemps */
        if (!los(eye_pos(), vsub(P.anchor, vmul(dir, tr)))) P.hookBlockT += dt;
        else P.hookBlockT = 0;
        if (P.hookBlockT > 0.15f) { hook_release(0); return; }

        float pull = onTarget ? GRAPPLE_ENEMY_PULL : GRAPPLE_PULL, maxSp = onTarget ? GRAPPLE_ENEMY_MAXSPEED : GRAPPLE_MAXSPEED;
        if (raceOn && !onTarget) {                        /* course : on se balance (traction douce, elan en avancant) */
            if (dist <= RACE_ROPE) { pull *= RACE_SWING_PULL; if (P.ropeLen > RACE_ROPE) P.ropeLen = RACE_ROPE; }   /* sinon : on rembobine */
            if (act[A_FWD]) {
                V3 f = v3(sinf(P.yaw), 0, -cosf(P.yaw)), t = vsub(f, vmul(dir, vdot(f, dir)));
                if (vlen(t) > 0.1f) P.vel = vadd(P.vel, vmul(vnorm(t), RACE_SWING_PUMP * dt));
            }
        }
        if (vdot(P.vel, dir) < maxSp) P.vel = vadd(P.vel, vmul(dir, pull * dt));
        if (dist < P.ropeLen) P.ropeLen = dist;
        float out = -vdot(P.vel, dir);                    /* vitesse qui s'eloigne de l'ancre */
        if (dist >= P.ropeLen - 0.01f && out > 0) P.vel = vadd(P.vel, vmul(dir, out));
        if (dir.y > 0.2f) P.grounded = 0;
    }
}

static float spec_speed(void) {                          /* ECLAIREUR plus rapide, REMPART plus lent */
    int s = my_spec();
    return s == SPEC_ECLAIREUR ? SPEC_ECLAIR_SPD : s == SPEC_REMPART ? SPEC_REMPART_SPD : 1.0f;
}
static void player_tick(float dt, int jumpPress, int dashPress) {
    P.prevPos = P.pos;
    int alive = !P.dead;
    float fw = alive ? (float)(act[A_FWD] - act[A_BACK]) : 0;
    float sd = alive ? (float)(act[A_RIGHT] - act[A_LEFT]) : 0;
    V3 f = v3(sinf(P.yaw), 0, -cosf(P.yaw)), r = v3(cosf(P.yaw), 0, sinf(P.yaw));
    V3 wish = vnorm(vadd(vmul(f, fw), vmul(r, sd)));
    int hasWish = fw != 0 || sd != 0;

    /* Enclume : pendant le pique et l'immobilisation, plus de controle (ni saut ni grappin) */
    if (P.dead && P.anvilState != ANVIL_IDLE) { P.anvilState = ANVIL_IDLE; P.anvilStunT = 0; }
    int diving = P.anvilState == ANVIL_DIVE, stun = P.anvilState == ANVIL_STUN;
    V3 wishIn = wish;
    int hasWishIn = hasWish;
    if (diving || stun) { fw = sd = 0; wish = v3(0, 0, 0); hasWish = 0; jumpPress = 0; }
    if (stun) dashPress = 0;
    if (diving && dashPress && P.dashCharge >= 1) {       /* le dash annule le pique : l'echappatoire payante */
        P.anvilState = ANVIL_IDLE; P.meleeCd = SLAM_CD; diving = 0;
        wish = wishIn; hasWish = hasWishIn;
    }
    /* Egide : bouclier leve = ni dash ni slide, 7 m/s au sol, air-strafe divise par 2 ; aucune penalite au grappin */
    int shPen = P.shieldState == SHS_UP && P.hook != HOOK_ATTACHED && alive;
    if (shPen) { dashPress = 0; P.sliding = 0; }

    P.slideCd -= dt; P.wallJumpCd -= dt;
    P.kinDashT = fminf(P.kinDashT + dt, 10);              /* Elan : temps depuis le dernier dash */
    P.coyote = P.grounded ? 0 : P.coyote + dt;
    {                                                     /* ECLAIREUR : une charge de dash de plus */
        float dmax = my_spec() == SPEC_ECLAIREUR ? 3.0f : 2.0f;
        if (P.dashCharge < dmax) P.dashCharge = fminf(dmax, P.dashCharge + dt / DASH_RECHARGE);
    }

    /* accroupi / slide */
    int wantC = alive && act[A_CROUCH];
    if (wantC) P.crouch = 1;
    else if (P.crouch && pos_free(P.pos, H_STAND)) P.crouch = 0;
    float hs = hlen(P.vel);
    if (P.sliding && (!P.crouch || !P.grounded || hs < SLIDE_END)) P.sliding = 0;
    if (P.grounded && P.crouch && !P.sliding && hs > SLIDE_MIN && !shPen) {
        P.sliding = 1;
        if (P.slideCd <= 0) {
            V3 hd = vnorm(v3(P.vel.x, 0, P.vel.z));
            P.vel = vadd(P.vel, vmul(hd, SLIDE_BOOST));
            P.slideCd = SLIDE_CD;
            P.fovKick += 4;
        }
        play(S_SLIDE, 0.7f, 1.0f);
    }

    /* sauter pendant le grappin = lacher avec un boost, sans consommer le double saut */
    if (alive && jumpPress && P.hook == HOOK_ATTACHED) { hook_release(1); jumpPress = 0; }

    /* saut : sol (bhop auto en maintenant), wall-jump, double saut */
    if (alive && !diving && !stun && (jumpPress || (act[A_JUMP] && P.grounded))) {
        if (P.grounded || P.coyote < 0.1f) {
            P.vel.y = JUMP_V; P.grounded = 0; P.coyote = 1; P.sliding = 0;
            play(S_JUMP, 0.5f, frange(0.95f, 1.05f));
        } else if (jumpPress && P.wallTouch && P.wallJumpCd <= 0) {
            P.vel = vadd(P.vel, vmul(P.wallN, WALLJUMP_PUSH));
            P.vel.y = WALLJUMP_UP;
            P.wallrun = 0; P.wallJumpCd = 0.25f; P.airJumps = 1;
            play(S_WALLJUMP, 0.6f, 1.0f);
        } else if (jumpPress && P.airJumps > 0) {
            P.airJumps--;
            P.vel.y = JUMP_V * 0.95f;
            if (hasWish) {
                float h = hlen(P.vel);
                V3 hv = vlerp(v3(P.vel.x, 0, P.vel.z), vmul(wish, h), 0.6f);
                P.vel.x = hv.x; P.vel.z = hv.z;
            }
            for (int i = 0; i < 12; i++)
                add_part(vadd(P.pos, v3(frange(-.4f, .4f), 0, frange(-.4f, .4f))), v3(frange(-2, 2), -3, frange(-2, 2)),
                         v3(0.5f, 0.8f, 1.0f), 0.4f, 0.12f, 0);
            play(S_JUMP, 0.5f, 1.35f);
        }
    }

    /* dash */
    if (alive && dashPress && P.dashCharge >= 1) {
        {   /* Elan : vitesse d'avant le dash ; pendant le malus d'un dash precedent, elle ne peut pas monter */
            float hv = hlen(P.vel);
            P.kinPreDash = P.kinDashT >= KIN_DASH_FORGET ? hv : fminf(P.kinPreDash, hv);
            P.kinDashT = 0;
        }
        P.dashCharge -= 1;
        V3 d = hasWish ? wish : f;
        float sp = fmaxf(hlen(P.vel) + 3, DASH_SPEED);
        P.vel.x = d.x * sp; P.vel.z = d.z * sp;
        {   /* poussee verticale selon l'inclinaison de la vue : viser en haut = monter, en bas = plonger */
            float vy = sinf(P.pitch) * sp;
            if (P.grounded) { if (vy > 0) P.vel.y = vy; }
            else P.vel.y = (vy > 2) ? vy : (vy < 0 ? vy : 2);
        }
        P.fovKick += 9;
        play(S_DASH, 0.7f, 1.0f);
    }

    grapple_tick(dt, alive && hookPressIn && !diving && !stun, alive && hookHeldIn);

    /* friction (aucune quand le grappin tire) */
    if (P.grounded && P.hook != HOOK_ATTACHED && boxes[P.groundBox].kind != B_SPEED) {   /* bande : aucune friction */
        float sp = hlen(P.vel);
        if (sp > 1e-4f) {
            float fr = P.sliding ? SLIDE_FRICTION : (hasWish ? FRICTION * FRICTION_MOVE : FRICTION);
            float ctrl = sp < STOPSPEED ? STOPSPEED : sp;
            float ns = sp - ctrl * fr * dt;
            if (ns < 0) ns = 0;
            ns /= sp;
            P.vel.x *= ns; P.vel.z *= ns;
        }
    }

    /* acceleration */
    if (P.wallrun) {
        V3 wp = vnorm(vsub(wish, vmul(P.wallN, vdot(wish, P.wallN))));
        accelerate(wp, RUN_SPEED * spec_speed() * 1.15f, 6, 1e9f, dt);
        float away = vdot(P.vel, P.wallN);
        if (away > 0) P.vel = vsub(P.vel, vmul(P.wallN, away));
        P.vel = vsub(P.vel, vmul(P.wallN, 0.5f));                   /* colle au mur */
    } else if (P.grounded && !P.sliding) {
        accelerate(wish, (P.crouch ? CROUCH_SPEED : shPen ? SHIELD_GROUND_SPEED : RUN_SPEED) * spec_speed(), GROUND_ACCEL, 1e9f, dt);
        if (shPen) {                                      /* Egide : freinage progressif jusqu'a 7 m/s */
            float h = hlen(P.vel);
            if (h > SHIELD_GROUND_SPEED) { float k = fmaxf(SHIELD_GROUND_SPEED, h - SHIELD_BRAKE * dt) / h; P.vel.x *= k; P.vel.z *= k; }
        }
    } else if (P.sliding) {
        accelerate(wish, RUN_SPEED * spec_speed(), 3, 0.5f, dt);
    } else {
        accelerate(wish, RUN_SPEED * spec_speed(), AIR_ACCEL * (shPen ? SHIELD_AIR_MULT : 1), AIR_CAP, dt);
    }

    /* gravite */
    P.vel.y -= (P.wallrun ? GRAVITY * 0.12f : P.hook == HOOK_ATTACHED ? GRAVITY * 0.5f : GRAVITY) * dt;
    if (P.wallrun && P.vel.y < -1.0f) P.vel.y = -1.0f;

    /* Enclume : chute plafonnee ; une poussee vers le haut (roquette, recul) casse le pique */
    if (diving) {
        if (P.vel.y < -SLAM_VY_MAX) P.vel.y = -SLAM_VY_MAX;
        if (P.vel.y > SLAM_BREAK_VY) { P.anvilState = ANVIL_IDLE; P.meleeCd = fmaxf(P.meleeCd, SLAM_CD); diving = 0; }
        else for (int k = 0; k < 2; k++)                  /* trainee du meteore (au-dessus de la tete : hors de la vue) */
            add_part(vadd(P.pos, v3(frange(-.3f, .3f), frange(1.9f, 2.4f), frange(-.3f, .3f))), v3(frange(-1, 1), 4, frange(-1, 1)),
                     v3(1, 0.6f, 0.25f), 0.35f, 0.14f, 0);
    }
    if (stun) {                                           /* immobilise : seul un recul vers le haut libere */
        if (P.vel.y > 2) { P.anvilState = ANVIL_IDLE; P.anvilStunT = 0; stun = 0; }
        else {
            P.vel.x = P.vel.z = 0;
            if ((P.anvilStunT -= dt) <= 0) { P.anvilState = ANVIL_IDLE; P.anvilStunT = 0; }
        }
    }

    /* deplacement + collisions, sous-pas si on va vite */
    int wasG = P.grounded;
    float prevVy = P.vel.y;
    if (P.stairT > 0) P.stairT -= dt;
    float stairSp = hlen(P.vel);
    int steps = (int)(vlen(P.vel) * dt / 0.3f) + 1;
    P.grounded = 0;
    int onPad = 0;                                   /* verifie a chaque sous-pas (pas seulement a la fin du tick) */
    for (int s = 0; s < steps && !onPad; s++) {
        move_axis(0, P.vel.x * dt / steps, wasG);
        move_axis(2, P.vel.z * dt / steps, wasG);
        move_axis(1, P.vel.y * dt / steps, 0);
        if (P.grounded && boxes[P.groundBox].kind == B_PAD) onPad = 1;
    }
    if (wasG && !P.grounded && P.stairT > 0 && alive && P.hook != HOOK_ATTACHED) {   /* haut de l'escalier : decollage de rampe */
        float ramp = stairSp * STAIR_SLOPE;
        if (P.vel.y < ramp) P.vel.y = ramp;
        P.stairT = 0;
    }

    /* Enclume : contact direct en l'air, puis onde de choc a l'atterrissage (avant le jump pad) */
    int slammed = 0;
    if (diving && alive) slam_contact();
    if (P.grounded && !wasG && P.anvilState == ANVIL_DIVE && alive) { slam_land(-prevVy); slammed = 1; }
    slam_wave_tick(dt);                                   /* Enclume : le front de mon onde avance */

    if (P.grounded && !wasG && !slammed) {
        P.airJumps = 1; P.wallTime = 0; P.wallrun = 0;
        if (prevVy < -10)
            for (int k = 0; k < 18; k++) {                /* anneau de poussiere */
                float ang = k * (2 * PI / 18);
                add_part(vadd(P.pos, v3(0, 0.1f, 0)), v3(cosf(ang) * 6, 0.5f, sinf(ang) * 6), v3(0.75f, 0.75f, 0.8f),
                         0.45f, 0.2f, 0);
            }
        if (prevVy < -6) {
            P.landDip = fminf(0.3f, -prevVy * 0.012f);
            play(S_LAND, clampf(-prevVy / 25, 0.2f, 1), 1.0f);
        }
    }

    if (onPad && alive) {
        V3 L = boxes[P.groundBox].launch;
        P.vel = L; P.grounded = 0; P.airJumps = 1; P.coyote = 1;
        P.anvilState = ANVIL_IDLE; P.anvilStunT = 0;      /* le pad relance apres l'onde : pas d'immobilisation */
        P.fovKick += 6;
        burst(P.pos, v3(0.3f, 0.9f, 1.0f), 30, 6, 0.6f, 0.15f);
        play(S_PAD, 0.8f, 1.0f);
    }
    if (P.grounded && boxes[P.groundBox].kind == B_SPEED && alive) {   /* bande d'acceleration : poussee dans son sens */
        V3 L = boxes[P.groundBox].launch, d = vnorm(L);
        float along = vdot(P.vel, d), cap = vlen(L);
        if (along < cap) P.vel = vadd(P.vel, vmul(d, fminf(SPEED_ACCEL * dt, cap - along)));
    }

    wall_check();
    if (alive && P.anvilState != ANVIL_DIVE && !P.grounded && P.hook != HOOK_ATTACHED && P.wallTouch && act[A_FWD] &&
        hlen(P.vel) > WALLRUN_MIN && P.wallTime < WALLRUN_MAX) {
        if (!P.wallrun) { P.wallrun = 1; if (P.vel.y < 1) P.vel.y = 1; P.airJumps = 1; P.fistPJ = 1; }   /* Poings : charge rendue */
        P.wallTime += dt;
        if (frand() < 0.3f)
            add_part(vsub(vadd(P.pos, v3(0, 0.5f, 0)), vmul(P.wallN, HW)), v3(frange(-1, 1), frange(0, 1), frange(-1, 1)),
                     v3(1, 0.6f, 0.9f), 0.4f, 0.08f, 4);
    } else P.wallrun = 0;
    if (P.grounded) P.wallTime = 0;
    if (P.grounded) { P.fistPJ = 1; P.fistAirT = 0; } else P.fistAirT += dt;   /* Poings : 1 punch-jump par sejour en l'air */

    /* camera */
    float targetEye = P.dead ? 0.3f : (P.crouch ? EYE_CROUCH : EYE_STAND);
    P.eyeH += (targetEye - P.eyeH) * fminf(1, dt * 14);
    P.stepOfs *= expf(-dt * 14);
    P.landDip *= expf(-dt * 8);
    P.fovKick *= expf(-dt * 4);
    float targetRoll = P.dead ? 25.0f : (P.wallrun ? vdot(P.wallN, r) * 14.0f : sd * 1.8f);
    if (P.sliding) targetRoll += 3;
    P.roll += (targetRoll - P.roll) * fminf(1, dt * 8);
    hs = hlen(P.vel);
    if (P.grounded && !P.sliding && hs > 1) P.bobT += hs * dt * 1.15f;
    P.bob += ((P.grounded && !P.sliding) ? sinf(P.bobT) * 0.045f * fminf(hs / RUN_SPEED, 1) - P.bob : -P.bob) * fminf(1, dt * 12);

    if (P.sliding && frand() < 0.5f)
        add_part(vadd(P.pos, v3(frange(-.3f, .3f), 0.05f, frange(-.3f, .3f))), v3(frange(-1, 1), frange(1, 3), frange(-1, 1)),
                 v3(1.0f, 0.75f, 0.4f), 0.35f, 0.07f, 10);
}
