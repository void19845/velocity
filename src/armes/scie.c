/* VELOCITY - Scie (disque-boomerang)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- Scie : disque-boomerang ------------------------------------------- */
/* Une seule lame. Lancee, elle touche (degats selon la vitesse relative, puis retour guide vers le lanceur),
 * ou se plante dans le decor : il faut aller la chercher a pied, ou la rappeler au grappin (sans bouger).
 * Quand elle revient, le lanceur peut la parer pour la renvoyer plus vite (3 renvois d'affilee au plus).
 * Le lanceur fait autorite sur sa scie ; les autres simulent une copie corrigee par EV_SAW. */
static unsigned int sawCounter;
enum { SS_THROW, SS_HIT, SS_PLANT, SS_RECALL, SS_FOOT, SS_LOST, SS_BOUNCE, SS_BHIT, SS_TRY, SS_COUNT };
static int sawStat[SS_COUNT];                             /* journal du lanceur (bots) */
static float sawRelSum;                                   /* somme des vitesses relatives des touches */
static void net_hit_saw(int target, float dmg, V3 knock, int w, int bounces);   /* reseau: definies plus loin */
static void net_fx_coin_kick(const Coin *c);

static float saw_dmg(float rel) { return SAW_DMG + fmaxf(0, rel - SAW_REL_OFFSET) * SAW_DMG_PER_MS; }
/* disque tenu a plat dans le plan de visee : son axe est le "haut" de la camera qui regarde dans la direction f */
static V3 saw_axis(V3 f) {
    V3 r = vnorm(v3(-f.z, 0, f.x));
    if (vlen(r) < 0.5f) r = v3(1, 0, 0);
    return vnorm(vcross(r, f));
}
static Proj *saw_mine(void) {
    Proj *s = P.sawIdx ? &PR[P.sawIdx - 1] : NULL;
    return s && s->alive && s->kind == PR_SAW ? s : NULL;
}
static int saw_face(V3 n) {                               /* code de face pour le reseau : axe * 2 + (signe > 0) */
    for (int a = 0; a < 3; a++) if (fabsf(*vc(&n, a)) > 0.5f) return a * 2 + (*vc(&n, a) > 0);
    return 3;
}
static V3 saw_face_n(int face) { V3 n = v3(0, 0, 0); *vc(&n, (face / 2) % 3) = (face & 1) ? 1.0f : -1.0f; return n; }
static V3 saw_col(const Proj *p) {                        /* la lame rougit avec les renvois */
    return vlerp(v3(0.78f, 0.8f, 0.86f), v3(1, 0.45f, 0.2f), clampf(p->sawBounces / (float)SAW_BOUNCE_CHAIN, 0, 1));
}

static void saw_remove_owner(int id) {
    for (int i = 0; i < MAXPR; i++)
        if (PR[i].alive && PR[i].kind == PR_SAW && PR[i].owner == id) PR[i].alive = 0;
    if (id == myId) P.sawIdx = 0;
}

static void fire_saw(void) {
    if (P.ammo[WP_SAW] < 1 || P.sawCd > 0) return;
    V3 f = view_dir();
    float s = fmaxf(SAW_SPEED + vdot(P.vel, f), SAW_MIN_SPEED);   /* seule la vitesse dans l'axe est heritee */
    Proj *p = spawn_proj(PR_SAW, vadd(eye_pos(), v3(0, SAW_SPAWN_UP, 0)), vmul(f, s));
    if (!p) return;
    p->sawState = SAW_FLY; p->sawAxis = saw_axis(f);
    if (ric_take()) p->ricLeft = RIC_SAW_CAP;             /* RICOCHET : le disque rebondit sur le decor (saw_tick) au lieu de se planter */
    p->netId = ((unsigned int)(myId + 1) << 24) | ((unsigned int)PR_SAW << 20) | (++sawCounter & 0xFFFFF);
    P.ammo[WP_SAW] = 0; P.sawIdx = (int)(p - PR) + 1; P.sawCd = SAW_CD;
    P.recoil = 0.6f; shake += 0.04f;
    sawStat[SS_THROW]++;
    play(S_SAW_THROW, 0.7f, frange(0.95f, 1.05f));
    if (dmMode) net_fx_saw(SAWE_THROW, p, -1, 0);
}

static void saw_hit_fx(V3 at, float dmg, float rel) {
    burst(at, v3(1, 0.85f, 0.5f), 24, 9, 0.35f, 0.1f);
    hitMarker = 0.18f;
    hitstop = fmaxf(hitstop, 0.03f + rel * 0.001f);
    shake += 0.1f;
    if (dmg >= 85) impact_frame(at, 0.06f);                /* typiquement : un renvoi en courant */
    play(S_SAW_HIT, 0.8f, frange(0.95f, 1.05f));
    play(S_HIT, 0.6f, 1.1f);
}

static void saw_catch(Proj *p, int how) {
    static const char *hows[5] = { "retour", "a pied", "rappel", "perdue", "paree" };
    if (how == SAW_HOW_RETURN) { P.dashCharge = fminf(2, P.dashCharge + SAW_DASH); P.fovKick += 3; }
    if (how == SAW_HOW_FOOT) sawStat[SS_FOOT]++;
    P.ammo[WP_SAW] = 1; P.sawIdx = 0; p->alive = 0;       /* sawCd n'est pas touche : aucun delai ajoute */
    play(S_SAW_CATCH, 0.6f, how == SAW_HOW_RETURN ? 1.15f : 1.0f);
    if (dmMode) net_fx_saw(SAWE_CATCH, p, -1, how);
    if (botMode) netlog("scie : rattrapee (%s)", hows[how]);
}

static void saw_plant(Proj *p, V3 hit, V3 n, V3 dir, int box) {
    p->pos = vsub(hit, vmul(n, SAW_EMBED));               /* le centre s'enfonce : 0,23 m de lame depasse */
    V3 ax = vcross(dir, n);                               /* le disque entre par la tranche : son plan contient dir et n */
    if (vlen(ax) > 0.05f) p->sawAxis = vnorm(ax);         /* tir perpendiculaire a la face : on garde l'axe de vol */
    p->sawN = n; p->sawBox = box; p->vel = v3(0, 0, 0);
    p->sawState = SAW_PLANTED; p->sawT = 0; p->sawSync = SAW_RESYNC;
    for (int k = 0; k < 14; k++) add_part(hit, vadd(vmul(n, 4), vmul(rand_dir(), 3)), v3(1, 0.8f, 0.45f), 0.35f, 0.08f, 12);
    if (box >= 0) shards(hit, boxes[box].col, 3, 4);
    play(S_SAW_PLANT, 0.8f * atten(hit), frange(0.95f, 1.05f));
    sawStat[SS_PLANT]++;
    if (dmMode) net_fx_saw(SAWE_PLANT, p, -1, 0);
    if (botMode) netlog("scie : plantee a %.1f m (face %+.0f %+.0f %+.0f, apres %d renvois)",
                        vlen(vsub(hit, pcenter())), n.x, n.y, n.z, p->sawBounces);
}

/* touche du lanceur (he : drone, hp : joueur) : degats, puis retour guide */
static void saw_strike(Proj *p, V3 dir, float t, int he, int hp) {
    V3 at = vadd(p->pos, vmul(dir, t));
    float rel = vlen(vsub(p->vel, he >= 0 ? E[he].vel : np_vel(hp)));
    float dmg = saw_dmg(rel) * (p->sawBounces ? SAW_RETURN_PARRY_DMG_MULT : 1);   /* renvoi : x0,9 */
    if (he >= 0) damage_enemy(he, dmg, vmul(dir, 6));
    else if (!hit_player_ex(hp, dmg * PVP_SCALE, vadd(vmul(dir, 5), v3(0, 2, 0)), W_SAW, dir, SHB_DIRECT, 0, (float)p->sawBounces, 0, 0)) {
        V3 n;                                             /* Egide : bloquee = ratee, elle tombe et se plante sous l'impact */
        float g = ray_world(at, v3(0, -1, 0), 50, &n);
        p->pos = at; p->vel = v3(0, 0, 0);
        if (g < 50) saw_plant(p, vadd(at, v3(0, -g, 0)), n, v3(0, -1, 0), rayBox);
        else { p->sawState = SAW_LOST; p->sawT = 0; }
        if (botMode) netlog("scie : bloquee par le bouclier de %s", NP[hp].name);
        return;
    }
    if (p->sawBounces) {                                  /* renvoi qui touche = parry reussi : points a l'autre arme */
        ammo_points(PTS_PARRY, AP_PARRY);
        sawStat[SS_BHIT]++;
    }
    sawStat[SS_HIT]++; sawRelSum += rel;
    saw_hit_fx(at, dmg, rel);
    p->pos = at; p->sawState = SAW_RETURN; p->sawT = 0;   /* sawBounces garde son compte : la chaine continue */
    if (dmMode) net_fx_saw(SAWE_HIT, p, hp, 0);
    if (botMode) netlog("scie : touche %s, rel %.1f, degats %.0f, renvoi %d", he >= 0 ? "drone" : NP[hp].name, rel,
                        he >= 0 ? dmg : dmg * PVP_SCALE, p->sawBounces);
}

/* la scie en vol frole une piece : elle la relance vers le haut (une fois par piece et par vol) */
static void saw_coins(Proj *p, V3 dir, float t) {
    for (int i = 0; i < MAXCOIN && i < 32; i++) {
        Coin *c = &CO[i];
        float tc;
        if (!c->alive || c->decoy || ((p->sawCoins >> i) & 1u)) continue;   /* MIRAGE : le leurre ne se relance pas */
        if (!ray_sphere(p->pos, dir, c->pos, SAW_COIN_R, t, &tc)) continue;
        c->vel = v3(c->vel.x * 0.3f, SAW_COIN_KICK, c->vel.z * 0.3f);
        c->life += SAW_COIN_LIFE;
        p->sawCoins |= 1u << i;
        for (int k = 0; k < 10; k++) add_part(c->pos, vmul(rand_dir(), 5), v3(1, 0.85f, 0.3f), 0.3f, 0.08f, 0);
        play(S_HIT, 0.5f, 2.6f);
        if (dmMode) net_fx_coin_kick(c);
    }
}

/* option SAW_RECALL_HURTS : la scie rappelee blesse le premier adversaire traverse */
static void saw_recall_hurt(Proj *p, V3 dir, float len) {
    float te;
    for (int j = 0; j < MAXE; j++)
        if (E[j].alive && ray_sphere(p->pos, dir, E[j].pos, enemy_radius(E[j].type) + 0.3f, len, &te)) {
            damage_enemy(j, saw_dmg(vlen(vsub(p->vel, E[j].vel))), vmul(dir, 6)); p->sawHurt = 1; return;
        }
    for (int j = 0; j < MAXP; j++) {
        V3 mn, mx;
        if (!np_target(j)) continue;
        np_aabb(j, SAW_HIT_GROW, &mn, &mx);
        if (ray_box(p->pos, dir, mn, mx, len, &te, NULL)) {
            hit_player_ex(j, saw_dmg(vlen(vsub(p->vel, np_vel(j)))) * PVP_SCALE, vmul(dir, 5), W_SAW, dir, SHB_DIRECT, 0, 0, 0, 0);
            p->sawHurt = 1; return;
        }
    }
}

static void saw_bounce_fx(V3 pos, V3 dir, int n) {
    hitstop = fmaxf(hitstop, 0.05f);
    P.fovKick += 4;
    if (n > 1) snprintf(popText, sizeof popText, "RENVOI x%d", n); else snprintf(popText, sizeof popText, "RENVOI");
    popT = 1.0f;
    add_beam(pos, vadd(pos, vmul(dir, 2)), v3(1, 0.95f, 0.8f), 0.25f);
    for (int k = 0; k < 20; k++) add_part(pos, vmul(rand_dir(), 7), v3(1, 1, 1), 0.3f, 0.09f, 0);
    play(S_SAW_BOUNCE, 0.8f, 1.0f);
    play(S_PARRY, 0.5f, 1.3f + 0.15f * n);
}

/* renvoi : je pare ma scie qui revient ; elle repart vers le viseur (aide a la visee de 7 degres) */
static void saw_bounce(Proj *p, float speedMult) {
    V3 eye = eye_pos(), f = view_dir(), aim = f;
    float base = SAW_SPEED * SAW_BOUNCE_MULT * speedMult;
    float sp = fminf(base + fmaxf(0, vdot(P.vel, f)), SAW_BOUNCE_MAX), best = SAW_BOUNCE_AIM_COS;
    for (int j = 0; j < MAXE + MAXP; j++) {               /* drones puis joueurs : point anticipe le plus pres du viseur */
        V3 c, vc2;
        if (j < MAXE) { if (!E[j].alive) continue; c = E[j].pos; vc2 = E[j].vel; }
        else { if (!np_target(j - MAXE)) continue; c = np_center(j - MAXE); vc2 = np_vel(j - MAXE); }
        float dd = vlen(vsub(c, p->pos));
        if (dd > SAW_BOUNCE_AIM_RANGE || !los(p->pos, c)) continue;
        float tt = dd / sp;
        V3 lead = vadd(vadd(c, vmul(vc2, tt)), v3(0, 0.5f * SAW_GRAVITY * tt * tt, 0));
        float k = vdot(vnorm(vsub(lead, eye)), f);        /* cone mesure depuis l'oeil, comme le viseur */
        if (k > best) { best = k; aim = vnorm(vsub(lead, p->pos)); }
    }
    p->vel = vmul(aim, sp);
    p->sawState = SAW_FLY; p->sawT = 0; p->sawBy = -1; p->sawCoins = 0; p->sawBounces++;
    p->sawAxis = saw_axis(aim);
    sawStat[SS_BOUNCE]++;
    saw_bounce_fx(p->pos, aim, p->sawBounces);
    if (dmMode) net_fx_saw(SAWE_BOUNCE, p, -1, p->sawBounces);
    if (botMode) netlog("scie : renvoi %d a %.0f m/s, aide %s", p->sawBounces, sp, best > SAW_BOUNCE_AIM_COS ? "oui" : "non");
}

/* appele par try_parry pour chaque scie : renvoi de la mienne, ou parry d'une scie adverse en vol. Renvoie 1 si paree */
static int saw_parry(Proj *p, V3 eye, V3 f, float range, float speedMult) {
    int st = p->sawState;
    V3 d = vsub(p->pos, eye);
    float dist = vlen(d);
    int front = vdot(vmul(d, 1.0f / fmaxf(dist, 0.01f)), f) >= 0.3f;   /* il faut faire face a la scie */
    if (!dmMode || p->owner == myId) {                    /* RENVOI : seulement une scie qui revient */
        if ((st != SAW_RETURN && st != SAW_RECALL && st != SAW_PARRIED) || p->sawBounces >= SAW_BOUNCE_CHAIN) return 0;
        if (dist > range * powf(PARRY_RANGE_SHRINK, (float)p->sawBounces) || !front) return 0;
        saw_bounce(p, speedMult);
        P.swingLanded = 1;                                /* ni soin, ni dash, ni grappin : ce serait farmable */
        return 1;
    }
    if (st != SAW_FLY && st != SAW_PARRIED) return 0;     /* scie adverse en vol : parry normal */
    if (st == SAW_PARRIED && p->sawBy == myId) return 0;  /* deja renvoyee par moi */
    if (dist > range * powf(PARRY_RANGE_SHRINK, (float)p->parries) || !front) return 0;
    V3 to = vnorm(vsub(np_center(p->owner), p->pos));     /* toujours vers son lanceur, sans aide a la visee */
    p->vel = vmul(to, fmaxf(PARRY_SPEED, vlen(p->vel) * SAW_PARRY_MULT) * speedMult);
    p->sawState = SAW_PARRIED; p->sawBy = myId; p->sawT = 0; p->parries++;
    p->sawAxis = saw_axis(to);
    net_fx_parry(p->netId, p->pos, p->vel, 1, p->parries);
    P.swingLanded = 1;
    parry_feedback(p->pos);                               /* parry adverse : recompense complete */
    melee_parry_points(PTS_PARRY);
    if (botMode) netlog("scie de %s paree", NP[p->owner].name);
    return 1;
}

/* grappin en vol : le crochet touche-t-il ma scie plantee avant *t ? (pas a travers un mur fin) */
static int saw_hook_test(float *t) {
    Proj *s = saw_mine();
    float ts;
    if (!s || s->sawState != SAW_PLANTED) return 0;
    if (!ray_sphere(P.hookPos, P.hookDir, s->pos, SAW_HOOK_R, *t + 0.05f, &ts)) return 0;
    if (!los(P.hookPos, vadd(s->pos, vmul(s->sawN, 0.25f)))) return 0;
    *t = ts;
    return 1;
}
/* rappel : la scie revient a 45 m/s ; le crochet est decroche aussitot, ni traction ni balancement */
static void saw_recall_start(void) {
    Proj *s = saw_mine();
    if (!s) return;
    s->sawState = SAW_RECALL; s->sawT = 0;
    P.hook = HOOK_NONE; P.hookEnemy = P.hookPlayer = 0; P.hookCd = GRAPPLE_CD;
    for (int k = 0; k < 12; k++) add_part(s->pos, vmul(rand_dir(), 4), v3(0.4f, 0.8f, 1), 0.3f, 0.08f, 0);
    play(S_HOOKHIT, 0.7f, 1.6f); play(S_SAW_CATCH, 0.4f, 0.7f);
    sawStat[SS_RECALL]++;
    if (dmMode) net_fx_saw(SAWE_RECALL, s, -1, 0);
    if (botMode) netlog("scie : rappel au grappin (%.1f m)", vlen(vsub(s->pos, pcenter())));
}

static void saw_tick(Proj *p, float dt) {
    int mine = !dmMode || p->owner == myId;
    if (!mine && (p->owner < 0 || p->owner >= MAXP || !NP[p->owner].active)) { p->alive = 0; return; }
    p->sawT += dt;
    if (p->sawState != SAW_PLANTED && p->sawState != SAW_LOST) {
        p->sawSpin += dt * 30;
        if ((p->sawSync -= dt) <= 0) { p->sawSync = SAW_WHIRR; play(S_SAW_THROW, 0.3f * atten(p->pos), 1.3f); }   /* vrombissement */
    }
    V3 home = mine ? pcenter() : np_center(p->owner);
    switch (p->sawState) {
    case SAW_FLY: case SAW_PARRIED: {
        p->vel.y -= SAW_GRAVITY * dt;
        V3 step = vmul(p->vel, dt), dir = v3(0, -1, 0), n;
        float len = vlen(step);
        if (len > 1e-6f) dir = vmul(step, 1.0f / len); else len = 1e-6f;
        float t = ray_world(p->pos, dir, len, &n), te;
        int box = rayBox, he = -1, hp = -1;
        if (mine && p->sawState == SAW_FLY) {             /* seul le lanceur juge les touches */
            for (int j = 0; j < MAXE; j++)
                if (E[j].alive && ray_sphere(p->pos, dir, E[j].pos, enemy_radius(E[j].type) + 0.3f, t, &te)) { t = te; he = j; }
            for (int j = 0; j < MAXP; j++) {
                V3 mn, mx;
                if (!np_target(j)) continue;
                np_aabb(j, SAW_HIT_GROW, &mn, &mx);
                if (ray_box(p->pos, dir, mn, mx, t, &te, NULL) && te < t) { t = te; hp = j; he = -1; }
            }
            saw_coins(p, dir, t);
        }
        if (p->sawState == SAW_PARRIED && dmMode && p->sawBy == myId && np_target(p->owner)) {   /* celui qui a pare juge la touche */
            V3 mn, mx;
            np_aabb(p->owner, SAW_HIT_GROW, &mn, &mx);
            if (ray_box(p->pos, dir, mn, mx, t, &te, NULL) && te < t) {
                V3 at = vadd(p->pos, vmul(dir, te));
                float rel = vlen(vsub(p->vel, np_vel(p->owner)));
                hit_player_ex(p->owner, saw_dmg(rel) * PVP_SCALE, vadd(vmul(dir, 5), v3(0, 2, 0)), W_PARRY, dir, SHB_DIRECT, 0, 0, 0, 0);
                saw_hit_fx(at, 0, rel);
                p->alive = 0;                             /* le lanceur la rattrape chez lui */
                return;
            }
        }
        if (he >= 0 || hp >= 0) { saw_strike(p, dir, t, he, hp); return; }
        if (t < len && mine && p->sawState == SAW_FLY && p->ricLeft > 0) {   /* RICOCHET : le disque rebondit au lieu de se planter */
            V3 hit = vadd(p->pos, vmul(dir, t));
            p->vel = vsub(p->vel, vmul(n, 2 * vdot(p->vel, n)));
            p->pos = vadd(hit, vmul(n, 0.05f));
            p->ricLeft--; ricStat[1]++;
            burst(hit, v3(1, 0.85f, 0.5f), 14, 6, 0.3f, 0.09f);
            play(S_SAW_BOUNCE, 0.5f * atten(hit), 1.2f);
            p->sawAxis = saw_axis(vnorm(p->vel));
            if (dmMode) net_fx_saw(SAWE_THROW, p, -1, 0);   /* les autres recalent la copie (position et vitesse) */
            return;
        }
        if (t < len) {                                    /* decor : elle se plante */
            V3 hit = vadd(p->pos, vmul(dir, t));
            if (mine) saw_plant(p, hit, n, dir, box);
            else {                                        /* prediction, corrigee par SAWE_PLANT */
                V3 ax = vcross(dir, n);
                if (vlen(ax) > 0.05f) p->sawAxis = vnorm(ax);
                p->pos = vsub(hit, vmul(n, SAW_EMBED)); p->sawN = n; p->vel = v3(0, 0, 0);
                p->sawState = SAW_PLANTED; p->sawT = 0;
                play(S_SAW_PLANT, 0.6f * atten(hit), 1.0f);
            }
            return;
        }
        p->pos = vadd(p->pos, step);
        if (mine && p->sawState == SAW_PARRIED && vlen(vsub(p->pos, home)) < SAW_CATCH_R) { saw_catch(p, SAW_HOW_PARRIED); return; }
        if (p->pos.y < map_lost_y() || p->sawT > SAW_FLY_MAX) {
            p->sawState = SAW_LOST; p->sawT = 0;
            if (mine) { sawStat[SS_LOST]++; if (botMode) netlog("scie : perdue"); }
            return;
        }
        if (frand() < 0.6f) add_part(p->pos, vmul(rand_dir(), 0.5f), p->sawBounces ? v3(1, 0.9f, 0.6f) : v3(1, 0.6f, 0.25f),
                                     0.25f, 0.08f, 0);   /* etincelles */
        break;
    }
    case SAW_PLANTED:
        if (!mine) break;
        if (!P.dead && vlen(vsub(pcenter(), p->pos)) < SAW_CATCH_R) { saw_catch(p, SAW_HOW_FOOT); return; }
        if (dmMode && (p->sawSync -= dt) <= 0) { p->sawSync = SAW_RESYNC; net_fx_saw(SAWE_PLANT, p, -1, 0); }
        break;
    case SAW_RETURN: case SAW_RECALL: {
        float sp = p->sawState == SAW_RETURN ? SAW_RETURN_SPEED : SAW_RECALL_SPEED;
        V3 d = vsub(home, p->pos);
        float l = vlen(d);
        p->vel = vmul(d, sp / fmaxf(l, 0.01f));           /* guidage parfait, a travers le decor */
        if (SAW_RECALL_HURTS && mine && p->sawState == SAW_RECALL && !p->sawHurt && l > 0.01f)
            saw_recall_hurt(p, vmul(d, 1.0f / l), sp * dt);
        if (l < SAW_CATCH_R || l < sp * dt || p->sawT > SAW_HOME_MAX) {
            if (mine) saw_catch(p, p->sawState == SAW_RETURN ? SAW_HOW_RETURN : SAW_HOW_RECALL);
            else p->alive = 0;                            /* SAWE_CATCH confirmera (SAWE_BOUNCE la recree) */
            return;
        }
        p->pos = vadd(p->pos, vmul(p->vel, dt));
        add_part(p->pos, v3(0, 0, 0), v3(0.4f, 0.8f, 1.0f), 0.2f, 0.1f, 0);   /* trainee "magnetique" */
        break;
    }
    case SAW_LOST:
        if (mine && p->sawT > SAW_LOST_T) saw_catch(p, SAW_HOW_LOST);
        else if (!mine && p->sawT > SAW_LOST_T + 2) p->alive = 0;
        break;
    }
}
/* --- fin Scie ----------------------------------------------------------- */
