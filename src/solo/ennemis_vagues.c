/* VELOCITY - mode solo : ennemis (drone, kamikaze, sentinelle, essaim, bastion) et remise a zero de la partie
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   La progression des vagues (100 vagues, cartes, boutique, INFINI) est dans src/solo/vagues.c, les boss dans
   src/solo/boss.c, les comportements achetes dans src/solo/ameliorations.c. */

/* ------------------------------------------------------------------ */
/* Ennemis                                                             */
/* ------------------------------------------------------------------ */
/* PV de base par type (les boss ont les leurs, src/solo/boss.c). Les PV montent un peu en fin de partie
 * (vgHpMul, VG_HP_MAX) ; les degats, eux, ne montent jamais (VG_*_DMG). */
static const float enemyHp[EN_COUNT] = { 100, 50, 80, 25, 300, 40, 150, 220, 1, 1, VG_BOSS_PYRA_HP, VG_BOSS_TESS_HP, VG_BOSS_BOULE_HP, VG_BOSS_HEXA_HP };
static float vgHpMul = 1, vgEliteP;                       /* fixes par src/solo/vagues.c au debut de chaque vague */
static void boss_tick(int i, float dt);                   /* src/solo/boss.c */
static int vg_lure(V3 *at);                               /* src/solo/ameliorations.c : leurre MIRAGE qui attire (APPAT) */
static void vg_reset(void);                               /* src/solo/vagues.c */

static int enemy_slot(void) {
    for (int i = 0; i < MAXE; i++) if (!E[i].alive) return i;
    return -1;
}
/* point d'apparition d'un ennemi volant : en hauteur, autour du centre de la carte, loin du joueur */
static V3 enemy_air_spot(void) {
    const MapDef *md = &maps[curMap];
    V3 p = v3(0, 10, 0);
    for (int tries = 0; tries < 20; tries++) {
        float ang = frange(0, 2 * PI), rad = frange(md->enemyRMin, md->enemyRMax);
        p = v3(cosf(ang) * rad, frange(md->enemyMinY, md->enemyMaxY), sinf(ang) * rad);
        if (vlen(vsub(p, P.pos)) > 22) break;
    }
    return p;
}
/* point d'apparition au sol (bastion) : un depart de la carte, le plus loin possible du joueur parmi quelques essais */
static V3 enemy_ground_spot(void) {
    const MapDef *md = &maps[curMap];
    V3 best = md->start; float bd = -1;
    for (int k = 0; k < 6 && md->nspawns > 0; k++) {
        const float *s = md->spawns[(int)(frand() * md->nspawns) % md->nspawns];
        V3 p = v3(s[0], s[1] + 1.4f, s[2]);
        float d = vlen(vsub(p, P.pos));
        if (d > bd) { bd = d; best = p; }
    }
    return best;
}
/* nouvel ennemi de type t a la position pos (summoned : invoque par un boss) ; renvoie son index ou -1 */
static int spawn_enemy_at(int t, V3 pos, int summoned) {
    int i = enemy_slot();
    if (i < 0) return -1;
    Enemy *e = &E[i];
    memset(e, 0, sizeof *e);
    e->alive = 1; e->type = t; e->summoned = summoned;
    e->hp = e->maxHp = enemyHp[t] * (EN_BOSS(t) ? 1 : vgHpMul);
    e->elite = !EN_BOSS(t) && vagOn && frand() < vgEliteP;
    e->fireT = frange(1.5f, 3.0f);
    e->t = frange(0, 10);
    e->pos = e->home = pos;
    e->yaw = atan2f(P.pos.x - pos.x, P.pos.z - pos.z);
    sphere_pushout(&e->pos, &e->vel, enemy_radius(t));
    burst(e->pos, enemy_color(t), EN_BOSS(t) ? 160 : 40, EN_BOSS(t) ? 16.0f : 8.0f, 0.6f, 0.15f);
    return i;
}
static int spawn_enemy_type(int t) {
    return spawn_enemy_at(t, t == EN_BASTION ? enemy_ground_spot() : enemy_air_spot(), 0);
}
/* ancien solo (captures, tests) : drones et kamikazes, le kamikaze apparait a partir de la vague 2 */
static void spawn_enemy(void) {
    float rusherChance = fminf(0.55f, 0.25f + wave * 0.04f);
    spawn_enemy_type((wave > 1 && frand() < rusherChance) ? EN_RUSHER : EN_DRONE);
}

/* l'ennemi i touche le joueur au contact (kamikaze, essaim) : Egide de face = bloque, recul x0,5 */
static void enemy_contact(V3 dir, float dmg, float knock) {
    int blk = shield_local_faces(dir);
    if (blk) shield_absorb_local(dmg, dir);
    else damage_player(dmg);
    P.vel = vadd(P.vel, vmul(dir, blk ? knock * SHIELD_KNOCK_MULT : knock));
}
static void enemy_shoot(Enemy *e, V3 dir, float speed) {
    spawn_proj(PR_ORB, vadd(e->pos, vmul(dir, enemy_radius(e->type) + 0.2f)), vmul(dir, speed));
    float dist = vlen(vsub(pcenter(), e->pos));
    play(S_ESHOT, clampf(1.2f - dist / 60, 0.15f, 0.8f), frange(0.9f, 1.1f));
}
/* SENTINELLE (l'ennemi vert) : a chaque tir elle choisit (e->phase, des le debut de la visee, pour que le rayon l'annonce) soit un LASER
 * (phase 1), soit une orbe tres rapide (phase 0, VG_SNIPER_FAST_SPEED : 34 avant). Les degats restent ceux de l'orbe du type
 * (VG_ORB_DMG : aucune inflation). Le laser est un trait instantane le long du cap fige ; s'il croise le joueur, une orbe-eclair part
 * a 1,6 m de lui dans le sens du tir : tout se passe alors comme pour une orbe (parry de la melee = elle est renvoyee sur la
 * sentinelle, Egide, mort), mais l'arrivee est instantanee : on l'esquive en quittant la ligne AVANT le claquement, ou on la pare. */
static void sniper_fire(Enemy *e) {
    V3 a = e->pos, d = e->aim;
    if (e->phase != 1) { enemy_shoot(e, d, VG_SNIPER_FAST_SPEED); return; }
    float len = ray_world(a, d, 150, 0);
    V3 ap = vsub(pcenter(), a);
    float t = clampf(vdot(ap, d), 0, len), dist = vlen(vsub(ap, vmul(d, t)));
    add_beam(a, vadd(a, vmul(d, len)), v3(1, 0.4f, 0.45f), 0.3f);
    add_light(vadd(a, vmul(d, len)), v3(1, 0.5f, 0.5f), 5, 0.25f);
    play(S_RAIL, clampf(1.2f - vlen(ap) / 60, 0.15f, 0.8f), 1.4f);
    if (!P.dead && dist < VG_LASER_R && vdot(ap, d) > 0 && vdot(ap, d) < len + VG_LASER_R) {
        V3 at = vsub(pcenter(), vmul(d, 1.6f));           /* sur la ligne qui traverse le joueur : l'orbe-eclair ne le rate pas */
        if (los(a, at)) spawn_proj(PR_ORB, at, vmul(d, VG_SNIPER_FAST_SPEED));
    }
}

/* BOMBE : explosion de VG_BOMBE_R m (degats fixes, Egide de face = bloque) ; elle meurt. Tuee pendant le tic, elle n'explose pas. */
static void bombe_explode(int i) {
    Enemy *e = &E[i];
    V3 c = e->pos, pc = pcenter();
    fx_explosion(c, VG_BOMBE_R);
    burst(c, v3(1, 0.3f, 0.75f), 80, 14, 0.6f, 0.25f);
    play(S_EXPLO, 0.9f * atten(c), 0.8f);
    shake += fmaxf(0, 0.5f - vlen(vsub(pc, c)) * 0.02f);
    if (!P.dead && vlen(vsub(pc, c)) < VG_BOMBE_R) enemy_contact(vnorm(vsub(pc, c)), VG_BOMBE_DMG, 12);
    kill_enemy(i, 0);
}
/* MORTIER : orbes a pulsation (lentes, detonent au contact, sur le decor ou d'elles-memes au bout de VG_MORTIER_LIFE) */
typedef struct { V3 pos, vel; float t; int on; } MortOrb;
#define MAXMB 8
static MortOrb MB[MAXMB];
static void mortar_boom(MortOrb *m) {
    m->on = 0;
    V3 pc = pcenter();
    fx_explosion(m->pos, VG_MORTIER_R);
    burst(m->pos, v3(0.75f, 0.3f, 1), 100, 16, 0.7f, 0.3f);
    play(S_EXPLO, 1.0f * atten(m->pos), 0.6f);
    shake += fmaxf(0, 0.6f - vlen(vsub(pc, m->pos)) * 0.02f);
    if (!P.dead && vlen(vsub(pc, m->pos)) < VG_MORTIER_R) enemy_contact(vnorm(vsub(pc, m->pos)), VG_MORTIER_DMG, 12);
}
static void mortar_fire(Enemy *e, V3 dir) {
    for (int k = 0; k < MAXMB; k++) if (!MB[k].on) {
        MB[k] = (MortOrb){ vadd(e->pos, vmul(dir, enemy_radius(e->type) + 0.4f)), vmul(dir, VG_MORTIER_SPEED), VG_MORTIER_LIFE, 1 };
        play(S_ESHOT, clampf(1.2f - vlen(vsub(pcenter(), e->pos)) / 60, 0.2f, 0.9f), 0.5f);
        return;
    }
}
static void mortar_tick(float dt) {
    V3 pc = pcenter();
    for (int k = 0; k < MAXMB; k++) {
        MortOrb *m = &MB[k];
        if (!m->on) continue;
        m->t -= dt;
        V3 step = vmul(m->vel, dt);
        float len = vlen(step);
        if (m->t <= 0 || (!P.dead && vlen(vsub(pc, m->pos)) < 1.4f) || (len > 1e-4f && ray_world(m->pos, vmul(step, 1.0f / len), len, 0) < len)) {
            mortar_boom(m);
            continue;
        }
        m->pos = vadd(m->pos, step);
        if (frand() < 0.6f) add_part(m->pos, vmul(rand_dir(), 0.8f), v3(0.75f, 0.3f, 1), 0.4f, 0.15f, 0);
    }
}

static void enemy_tick(float dt) {
    V3 pc = pcenter();
    mortar_tick(dt);
    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &E[i];
        if (!e->alive) continue;
        e->t += dt; e->flash -= dt;
        if (EN_BOSS(e->type)) { boss_tick(i, dt); continue; }   /* les boss ont leur propre IA */
        V3 tgt = pc;
        vg_lure(&tgt);                                    /* MIRAGE (APPAT) : le leurre attire les ennemis */
        V3 to = vsub(tgt, e->pos);
        float dist = vlen(to);
        V3 dir = vmul(to, 1.0f / fmaxf(dist, 0.01f));
        if (e->frozenT > 0) {                             /* GEL (boutique) : immobile, ne tire pas */
            e->frozenT -= dt;
            e->vel = vmul(e->vel, expf(-dt * 8));
            if (e->type == EN_BASTION) e->vel.y -= GRAVITY * dt;   /* le bastion gele tombe quand meme */
            e->pos = vadd(e->pos, vmul(e->vel, dt));
            sphere_pushout(&e->pos, &e->vel, enemy_radius(e->type));
            continue;
        }
        float cad = vagOn ? 0 : fminf(wave * 0.08f, 0.7f);  /* VAGUES : cadence fixe (aucune vague ne la monte) */
        if (e->type == EN_DRONE) {
            float ang = e->t * 0.35f + i * 1.7f;
            V3 target = vadd(tgt, v3(cosf(ang) * 13, 4.5f + sinf(e->t * 0.7f + i) * 2, sinf(ang) * 13));
            V3 tv = vsub(target, e->pos);
            V3 want = vmul(vnorm(tv), 8.0f * clampf(vlen(tv) / 4, 0, 1));
            e->vel = vlerp(e->vel, want, fminf(1, dt * 2));
            if (e->elite) {                               /* elite : esquive de cote quand on le vise */
                e->stT -= dt;
                V3 me = vsub(e->pos, eye_pos());
                if (e->stT <= 0 && vdot(vnorm(me), view_dir()) > 0.985f) {
                    V3 side = vnorm(vcross(view_dir(), v3(0, 1, 0)));
                    e->vel = vadd(e->vel, vmul(side, frand() < 0.5f ? 14.0f : -14.0f));
                    e->stT = 1.6f;
                }
            }
            e->fireT -= dt;
            if (e->fireT <= 0 && !P.dead) {
                if (dist < 70 && los(e->pos, tgt)) enemy_shoot(e, dir, 16);
                e->fireT = frange(1.6f, 2.6f) - cad;
            }
        } else if (e->type == EN_RUSHER) {
            float sp = vagOn ? 12.0f : 10.5f + fminf(wave * 0.3f, 3);
            V3 want = vmul(dir, sp);
            if (e->elite) {                               /* elite : zigzag, plus dur a viser */
                V3 side = vnorm(vcross(dir, v3(0, 1, 0)));
                want = vadd(want, vmul(side, sinf(e->t * 5) * 9));
            }
            e->vel = vlerp(e->vel, want, fminf(1, dt * 1.6f));
            float rr = enemy_radius(e->type) + 0.65f;
            if (!P.dead && vlen(vsub(pc, e->pos)) < rr && P.hookEnemy != i + 1) {
                enemy_contact(vnorm(vsub(pc, e->pos)), VG_RUSH_DMG, 9);
                kill_enemy(i, 0);
                continue;
            }
            if (frand() < 0.4f) add_part(e->pos, vmul(rand_dir(), 0.5f), v3(1, 0.5f, 0.1f), 0.35f, 0.12f, 0);
        } else if (e->type == EN_SNIPER) {                /* SENTINELLE : se poste loin, vise 1,4 s (rayon visible), laser ou orbe tres rapide */
            e->stT -= dt;
            if (e->st == 0) {
                float a = e->home.x + e->t * 0.05f;
                V3 perch = vadd(tgt, v3(cosf(a) * 30, 9 + 3 * sinf(e->t * 0.4f), sinf(a) * 30));
                const MapDef *md = &maps[curMap];
                perch.y = clampf(perch.y, md->enemyMinY, md->enemyMaxY + 6);
                V3 tv = vsub(perch, e->pos);
                e->vel = vlerp(e->vel, vmul(vnorm(tv), 9 * clampf(vlen(tv) / 5, 0, 1)), fminf(1, dt * 2));
                if (e->stT <= 0 && !P.dead && dist < 90 && los(e->pos, tgt)) {
                    e->st = 1; e->stT = 1.4f; e->aim = dir;
                    e->phase = frand() < VG_SNIPER_LASER_P ? 1 : 0;   /* ce tir : laser (1) ou orbe rapide (0), annonce par le rayon */
                }
                else if (e->stT <= 0) e->stT = 0.5f;
            } else {
                e->vel = vmul(e->vel, expf(-dt * 6));
                if (e->stT > (e->elite ? 0.12f : 0.35f)) e->aim = vnorm(vlerp(e->aim, dir, fminf(1, dt * 6)));   /* fige avant le tir */
                if (!los(e->pos, tgt) && e->stT > 0.35f) { e->st = 0; e->stT = 1.0f; }
                else if (e->stT <= 0) {
                    sniper_fire(e);                       /* laser parryable ou orbe tres rapide (plus l'orbe a 34 m/s) */
                    e->st = 0; e->stT = frange(2.0f, 3.2f);
                }
            }
        } else if (e->type == EN_SWARM) {                 /* ESSAIM : tourne autour, plonge, touche, repart */
            e->stT -= dt; e->fireT -= dt;
            V3 want;
            if (e->st == 1) {                             /* plongee */
                want = vmul(dir, 16);
                if (e->stT <= 0) { e->st = 0; e->stT = frange(1.8f, 3.2f); }
            } else {
                float ang = e->t * 1.3f + i * 2.1f;
                V3 target = vadd(tgt, v3(cosf(ang) * 7, 3 + sinf(e->t * 2 + i) * 1.5f, sinf(ang) * 7));
                want = vmul(vnorm(vsub(target, e->pos)), 13);
                if (e->stT <= 0) { e->st = 1; e->stT = 1.2f; }
            }
            e->vel = vlerp(e->vel, vadd(want, vmul(rand_dir(), 3)), fminf(1, dt * 3));
            if (!P.dead && e->fireT <= 0 && vlen(vsub(pc, e->pos)) < enemy_radius(e->type) + 0.7f) {
                V3 d = vnorm(vsub(pc, e->pos));
                enemy_contact(d, VG_SWARM_DMG, 4);
                e->vel = vmul(d, -14); e->fireT = 1.0f; e->st = 0; e->stT = 2;
            }
        } else if (e->type == EN_BOMBE) {                 /* BOMBE : fonce, s'arrete a ~8 m, tique VG_BOMBE_FUSE s, explose (zone de 5 m) */
            if (e->st == 0) {
                e->vel = vlerp(e->vel, vmul(dir, VG_BOMBE_SPEED), fminf(1, dt * 4));
                if (!P.dead && dist < VG_BOMBE_STOP) { e->st = 1; e->stT = VG_BOMBE_FUSE; e->fireT = 0; }
            } else {
                e->stT -= dt; e->fireT -= dt;
                e->vel = vlerp(e->vel, e->elite ? vmul(dir, 5.0f) : v3(0, 0, 0), fminf(1, dt * 6));   /* elite : continue de ramper vers toi */
                if (e->fireT <= 0) {
                    float k = clampf(e->stT / VG_BOMBE_FUSE, 0, 1);
                    play(S_MINE_BEEP, 0.7f * atten(e->pos), 1.0f + (1 - k));
                    e->fireT = 0.08f + 0.35f * k;           /* le tic accelere */
                }
                if (e->stT <= 0) { bombe_explode(i); continue; }
            }
        } else if (e->type == EN_CARRE) {                 /* CARRE : mini tesseract, flotte a 16 m, protege les autres tant qu'il vit */
            float ang = e->t * 0.25f + i * 2.3f;
            V3 target = vadd(tgt, v3(cosf(ang) * 16, 7 + sinf(e->t * 0.5f + i) * 2, sinf(ang) * 16));
            V3 tv = vsub(target, e->pos);
            e->vel = vlerp(e->vel, vmul(vnorm(tv), 6.0f * clampf(vlen(tv) / 4, 0, 1)), fminf(1, dt * 2));
        } else if (e->type == EN_MORTIER) {               /* MORTIER : se poste a ~26 m, charge 0,9 s, lance une orbe lente a grosse zone */
            e->stT -= dt;
            if (e->st == 0) {
                float a = e->home.x + e->t * 0.04f;
                V3 perch = vadd(tgt, v3(cosf(a) * 26, 11 + 2 * sinf(e->t * 0.4f), sinf(a) * 26));
                const MapDef *md = &maps[curMap];
                perch.y = clampf(perch.y, md->enemyMinY, md->enemyMaxY + 6);
                V3 tv = vsub(perch, e->pos);
                e->vel = vlerp(e->vel, vmul(vnorm(tv), 7 * clampf(vlen(tv) / 5, 0, 1)), fminf(1, dt * 2));
                if (e->stT <= 0 && !P.dead && dist < 80 && los(e->pos, tgt)) { e->st = 1; e->stT = 0.9f; e->aim = dir; }
                else if (e->stT <= 0) e->stT = 0.5f;
            } else {
                e->vel = vmul(e->vel, expf(-dt * 6));
                if (e->stT > 0.25f) e->aim = vnorm(vlerp(e->aim, dir, fminf(1, dt * 5)));
                if (e->stT <= 0) {
                    mortar_fire(e, e->aim);
                    if (e->elite) {                       /* elite : deux orbes en V */
                        V3 side = vnorm(vcross(e->aim, v3(0, 1, 0)));
                        mortar_fire(e, vnorm(vadd(e->aim, vmul(side, 0.16f))));
                    }
                    e->st = 0; e->stT = VG_MORTIER_T * (e->elite ? 0.7f : 1.0f);
                }
            }
        } else if (e->type == EN_BASTION) {               /* BASTION : marche, bouclier de face, salve de 3 orbes lentes */
            float want = atan2f(tgt.x - e->pos.x, tgt.z - e->pos.z), dy = want - e->yaw;
            while (dy > PI) dy -= 2 * PI;
            while (dy < -PI) dy += 2 * PI;
            float turn = (e->elite ? 2.4f : 1.2f) * dt;
            e->yaw += clampf(dy, -turn, turn);
            V3 fwd = v3(sinf(e->yaw), 0, cosf(e->yaw));
            float hd = hlen(vsub(tgt, e->pos)), sp = hd > 10 ? 4.0f : 0.0f;
            e->vel.x += (fwd.x * sp - e->vel.x) * fminf(1, dt * 3);
            e->vel.z += (fwd.z * sp - e->vel.z) * fminf(1, dt * 3);
            e->vel.y -= GRAVITY * dt;
            e->fireT -= dt;
            if (e->combo > 0 && e->fireT <= 0) {          /* salve en cours */
                enemy_shoot(e, vnorm(vsub(tgt, vadd(e->pos, v3(0, 0.6f, 0)))), 13);
                e->combo--; e->fireT = e->combo > 0 ? 0.25f : 4.5f;
            } else if (e->fireT <= 0 && !P.dead) {
                if (dist < 60 && los(e->pos, tgt)) e->combo = 3;
                e->fireT = e->combo ? 0 : 1.0f;
            }
        }
        for (int j = 0; j < MAXE; j++) {                   /* separation */
            if (j == i || !E[j].alive) continue;
            V3 dv = vsub(e->pos, E[j].pos);
            float d = vlen(dv), rr = enemy_radius(e->type) + enemy_radius(E[j].type);
            if (d < rr && d > 1e-4f) e->vel = vadd(e->vel, vmul(dv, (rr - d) * 4 * dt / d));
        }
        e->pos = vadd(e->pos, vmul(e->vel, dt));
        sphere_pushout(&e->pos, &e->vel, enemy_radius(e->type));
        if (e->type == EN_BASTION) {                      /* marcheur : tombe dans le vide = revient a son depart */
            if (maps[curMap].killY > -999 && e->pos.y < maps[curMap].killY) { e->pos = e->home; e->vel = v3(0, 0, 0); }
            if (e->pos.y < FALL_DEATH_Y) { e->pos = e->home; e->vel = v3(0, 0, 0); }
            continue;
        }
        float floorY = maps[curMap].enemyFloor;          /* plancher des drones (au-dessus du vide) */
        if (e->pos.y < floorY) { e->pos.y = floorY; if (e->vel.y < 0) e->vel.y = 0; }
    }
}

static int enemies_alive(void) {
    int n = 0;
    for (int i = 0; i < MAXE; i++) n += E[i].alive;
    return n;
}

static void reset_game(void) {
    memset(&P, 0, sizeof P);
    P.pos = P.prevPos = maps[curMap].start;
    P.hp = 100; P.dashCharge = 2; P.airJumps = 1; P.eyeH = EYE_STAND; P.fovCur = fovBase;
    style_reset();
    loadout_apply();
    anvil_reset();
    P.skillCh = (float)skillDef[P.skill].max;
    memset(CO, 0, sizeof CO);
    memset(markP, 0, sizeof markP); memset(markE, 0, sizeof markE); markedMeT = 0; lastHpSeen = 100;
    for (int i = 0; i < npu; i++) PU[i].t = PU[i].pending = 0;
    memset(E, 0, sizeof E); memset(PR, 0, sizeof PR); memset(PA, 0, sizeof PA); memset(BM, 0, sizeof BM); memset(MB, 0, sizeof MB);
    memset(TR, 0, sizeof TR);
    wave = 0; toSpawn = 0; waveDelay = 1.5f; spawnT = 0;
    score = kills = combo = 0; gameTime = 0; hitMarker = comboT = msgT = popT = shake = 0;
    shotStreak = 0; hitstop = parryFlash = streakLostT = 0;
    if (vagOn) vg_reset();                                /* VAGUES : retour a la vague 1 sur la carte de depart */
}
