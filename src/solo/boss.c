/* VELOCITY - mode VAGUES : les quatre boss (vagues 25, 50, 75, 100) et leurs attaques
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).

   PYRAMIDE (25, axe PARRY) : file d'un point a l'autre tres vite, s'arrete, tire des salves d'orbes ; toutes les 3
     salves une grosse orbe chargee (a parer : elle revient l'etourdir). A 50 % elle arrete l'orbe chargee et fonce sur
     le joueur, trois charges de suite : une charge paree l'etourdit. Armure hors etourdissement (VG_ARMOR), sauf les
     orbes parees. Secrete (INFINI) : PRISME, trois pyramides fantomes chargent ensemble de trois cotes.
   TESSERACT (50, axe CONTROLE DE FOULE) : immobile, invoque des ennemis relies a lui ; invulnerable tant qu'il en reste
     VG_LINKS ; barrages d'orbes lentes (anneau, spirale, eventail). Secrete : HYPERCUBE, une sphere d'orbes se referme.
   SPHERE (75, axe MOUVEMENT AERIEN) : roule vers le joueur, bondit et frappe le sol (ondes de choc qu'on saute) ; les
     coups portes depuis le sol ne font presque rien (VG_GROUND_MULT). Secrete : METEORES, trois impacts sur tes pas.
   HEXAEDRE (100, tout) : quatre phases. PRECISION (rayons a esquiver, coeur ouvert apres chaque salve), PARRY (charges
     et grosse orbe), FOULE (invocations, bouclier, anneaux), AIR (piques au sol et ondes, tout melange). Secrete :
     DEPLIAGE, quatre rayons balayent le sol en tournant (a sauter). */

enum { BS_MOVE, BS_VOLLEY, BS_CHARGE, BS_AIM, BS_RAM, BS_RECOVER, BS_STUN, BS_JUMP, BS_LASER, BS_OPEN, BS_HOLD, BS_DROP };
static int vgBoss = -1;                                   /* index du boss en cours (barre de vie), -1 : aucun */
static int vgSecret;                                      /* INFINI : les boss ont leur attaque secrete */
static int bossBypass;                                    /* degats directs (parry) : pas d'armure */
static char vgHint[64];                                   /* conseil sous la barre du boss ("PARE SES CHARGES") */
static float vgHintT;
static void vg_hint(const char *s) { if (vgHintT < 0.5f || strcmp(s, vgHint)) { snprintf(vgHint, sizeof vgHint, "%s", s); vgHintT = 2.0f; } }

/* --- ondes de choc au sol, rayons, impacts annonces, rayons balayeurs (rendus dans src/rendu/vagues_rendu.c) --- */
typedef struct { V3 c; float r, max, speed; int hit, on; } GWave;
typedef struct { V3 a, dir; float tele, life, len; int fired, on; } Laser;
typedef struct { V3 c; float t, max; int on; } Meteor;
#define MAXGW 10
#define MAXLZ 6
#define MAXMT 6
static GWave GW[MAXGW];
static Laser LZ[MAXLZ];
static Meteor MT[MAXMT];
static struct { V3 c; float y, ang, tele, t, hitCd; int on; } SWP;   /* DEPLIAGE */
static struct { V3 c; float t; int on; } HYP;             /* HYPERCUBE : orbes annoncees, puis lancees */
static int bossJumps;                                     /* SPHERE : bonds (METEORES tous les 3 en INFINI) */

static void boss_fx_reset(void) {
    memset(GW, 0, sizeof GW); memset(LZ, 0, sizeof LZ); memset(MT, 0, sizeof MT);
    memset(&SWP, 0, sizeof SWP); memset(&HYP, 0, sizeof HYP);
    vgBoss = -1; bossJumps = 0; vgHintT = 0;
}
static void gwave_add(V3 c, float max) {
    for (int k = 0; k < MAXGW; k++) if (!GW[k].on) { GW[k] = (GWave){ c, 0.5f, max, 16, 0, 1 }; return; }
}
static void laser_add(V3 a, V3 dir, float tele) {
    for (int k = 0; k < MAXLZ; k++) if (!LZ[k].on) { LZ[k] = (Laser){ a, dir, tele, 0.25f, 120, 0, 1 }; return; }
}
static void meteor_add(V3 c, float t) {
    for (int k = 0; k < MAXMT; k++) if (!MT[k].on) { MT[k] = (Meteor){ c, t, t, 1 }; return; }
}
static float ground_below(V3 p, float maxd) {             /* hauteur du sol sous p (p.y - maxd s'il n'y en a pas) */
    return p.y - ray_world(p, v3(0, -1, 0), maxd, 0);
}
/* le joueur se protege-t-il d'un coup venant de dir (vers lui) ? Egide levee de face */
static int boss_hit_player(V3 dir, float dmg, V3 knock) {
    if (P.dead) return 0;
    if (shield_local_faces(dir)) { shield_absorb_local(dmg, dir); P.vel = vadd(P.vel, vmul(knock, SHIELD_KNOCK_MULT)); return 0; }
    damage_player(dmg);
    P.vel = vadd(P.vel, knock);
    if (knock.y > 2) P.grounded = 0;
    return 1;
}
/* sphere libre de tout decor ? */
static int sphere_free(V3 p, float r) {
    for (int i = 0; i < nb; i++) {
        V3 q = v3(clampf(p.x, boxes[i].mn.x, boxes[i].mx.x), clampf(p.y, boxes[i].mn.y, boxes[i].mx.y), clampf(p.z, boxes[i].mn.z, boxes[i].mx.z));
        if (vlen(vsub(q, p)) < r) return 0;
    }
    return 1;
}
static int boss_summoned_alive(void) {
    int n = 0;
    for (int j = 0; j < MAXE; j++) n += E[j].alive && E[j].summoned == 1;
    return n;
}
static int boss_links(int i) { (void)i; return boss_summoned_alive(); }

/* --- tick des effets des boss (ondes, rayons, impacts, balayage, hypercube), aussi apres la mort du boss --- */
static void boss_fx_tick(float dt) {
    V3 pc = pcenter();
    vgHintT -= dt;
    for (int k = 0; k < MAXGW; k++) {
        GWave *g = &GW[k];
        if (!g->on) continue;
        g->r += g->speed * dt;
        if (g->r > g->max) { g->on = 0; continue; }
        float hd = hlen(vsub(P.pos, g->c)), dy = P.pos.y - g->c.y;
        if (!g->hit && fabsf(hd - g->r) < 0.9f && dy < 1.2f && dy > -2.0f) {   /* au sol (ou presque) quand l'onde passe */
            g->hit = 1;
            V3 out = vnorm(v3(P.pos.x - g->c.x, 0, P.pos.z - g->c.z));
            boss_hit_player(out, VG_WAVE_DMG, vadd(vmul(out, 6), v3(0, 9, 0)));
            vg_hint("SAUTE PAR-DESSUS LES ONDES");
        }
    }
    for (int k = 0; k < MAXLZ; k++) {
        Laser *l = &LZ[k];
        if (!l->on) continue;
        if (!l->fired) {
            l->tele -= dt;
            if (l->tele <= 0) {
                l->fired = 1;
                l->len = ray_world(l->a, l->dir, 150, 0);
                V3 ap = vsub(pc, l->a);
                float t = clampf(vdot(ap, l->dir), 0, l->len), d = vlen(vsub(ap, vmul(l->dir, t)));
                if (d < 1.0f) boss_hit_player(l->dir, VG_LASER_DMG, vmul(l->dir, 6));
                add_light(vadd(l->a, vmul(l->dir, l->len)), v3(1, 0.9f, 0.7f), 6, 0.3f);
                play(S_RAIL, 0.6f * atten(l->a), 0.7f);
            }
        } else if ((l->life -= dt) <= 0) l->on = 0;
    }
    for (int k = 0; k < MAXMT; k++) {
        Meteor *m = &MT[k];
        if (!m->on || (m->t -= dt) > 0) continue;
        m->on = 0;
        fx_explosion(m->c, 4.5f);
        burst(m->c, v3(1, 0.4f, 0.5f), 60, 14, 0.6f, 0.2f);
        play(S_EXPLO, 0.9f * atten(m->c), 0.6f);
        shake += 0.4f;
        gwave_add(m->c, 14);
        if (hlen(vsub(P.pos, m->c)) < 4.5f && P.pos.y - m->c.y < 3) {
            V3 out = vnorm(v3(P.pos.x - m->c.x, 0.5f, P.pos.z - m->c.z));
            boss_hit_player(out, VG_WAVE_DMG, vmul(out, 14));
        }
    }
    if (SWP.on) {                                         /* DEPLIAGE : quatre rayons au ras du sol qui tournent */
        if (SWP.tele > 0) SWP.tele -= dt;
        else {
            SWP.t -= dt; SWP.ang += 0.9f * dt; SWP.hitCd -= dt;
            float hd = hlen(vsub(P.pos, SWP.c)), a = atan2f(P.pos.z - SWP.c.z, P.pos.x - SWP.c.x);
            for (int b = 0; b < 4 && SWP.hitCd <= 0 && hd < 60; b++) {
                float da = a - (SWP.ang + b * PI * 0.5f);
                while (da > PI) da -= 2 * PI;
                while (da < -PI) da += 2 * PI;
                if (fabsf(da) * hd < 0.7f && P.pos.y < SWP.y + 0.4f && P.pos.y > SWP.y - 2.2f) {
                    boss_hit_player(v3(-sinf(a), 0, cosf(a)), VG_LASER_DMG, v3(0, 6, 0));
                    SWP.hitCd = 0.6f;
                    vg_hint("SAUTE LES RAYONS");
                }
            }
            if (SWP.t <= 0) SWP.on = 0;
        }
    }
    if (HYP.on && (HYP.t -= dt) <= 0) {                   /* HYPERCUBE : les orbes partent vers le centre */
        HYP.on = 0;
        for (int k = 0; k < 26; k++) {
            float u = (k + 0.5f) / 26, th = acosf(1 - 2 * u), ph = k * 2.39996f;
            V3 d = v3(sinf(th) * cosf(ph), cosf(th), sinf(th) * sinf(ph));
            spawn_proj(PR_ORB, vadd(HYP.c, vmul(d, 15)), vmul(d, -8));
        }
        play(S_WAVE, 0.8f, 0.5f);
    }
}

/* --- charge (pyramide, hexaedre, fantomes) : paree au bon moment = boss etourdi ; sinon le joueur est renverse --- */
static void boss_stunned(int i, float dmg) {
    Enemy *e = &E[i];
    V3 at = e->pos;
    bossBypass = 1;
    damage_enemy(i, dmg, v3(0, 0, 0));
    bossBypass = 0;
    if (!e->alive) return;
    e->st = BS_STUN; e->stT = VG_STUN_T; e->vel = vmul(vnorm(vsub(e->pos, pcenter())), 20);
    burst(at, v3(1, 1, 0.8f), 80, 14, 0.6f, 0.2f);
    shards(at, enemy_color(e->type), 16, 12);
    vg_hint("ETOURDI : FRAPPE !");
}
static void vg_boss_stun(int i, float dmg) {
    if (E[i].type == EN_PYRA || E[i].type == EN_HEXA) boss_stunned(i, dmg);
    else { bossBypass = 1; damage_enemy(i, dmg, v3(0, 0, 0)); bossBypass = 0; }
}
/* renvoie 1 si la charge est finie (paree ou encaissee) */
static int boss_ram_contact(int i) {
    Enemy *e = &E[i];
    V3 pc = pcenter(), to = vsub(e->pos, eye_pos());
    float R = enemy_radius(e->type), d = vlen(to);
    int facing = vdot(vmul(to, 1 / fmaxf(d, 0.01f)), view_dir()) > 0.3f;
    if (!P.dead && P.parryWin > 0 && facing && d < R + MELEE_RANGE + 1.0f) {   /* PARRY : la charge se brise sur l'arme */
        P.swingLanded = 1;
        parry_feedback(vadd(eye_pos(), vmul(view_dir(), 1.5f)));   /* (effets de parry de la boutique compris) */
        if (e->summoned == 2) { burst(e->pos, enemy_color(e->type), 70, 12, 0.5f, 0.2f); e->alive = 0; return 1; }   /* fantome */
        boss_stunned(i, VG_RAM_PARRY_DMG);
        return 1;
    }
    if (!P.dead && vlen(vsub(pc, e->pos)) < R + 0.8f) {
        V3 dir = vnorm(e->vel);
        boss_hit_player(dir, VG_RAM_DMG, vadd(vmul(dir, 20), v3(0, 6, 0)));
        vg_hint("PARE LA CHARGE AU DERNIER MOMENT");
        e->vel = vmul(e->vel, -0.3f);
        if (e->summoned == 2) { e->alive = 0; burst(e->pos, enemy_color(e->type), 40, 8, 0.4f, 0.15f); }
        return 1;
    }
    return 0;
}
/* sous-machine commune : AIM (vise 0,9 s, se fige 0,25 s avant) puis RAM (fonce a 42 m/s) puis RECOVER */
static void boss_ram_tick(int i, float dt) {
    Enemy *e = &E[i];
    V3 pc = pcenter(), dir = vnorm(vsub(pc, e->pos));
    if (e->st == BS_AIM) {
        e->vel = vmul(e->vel, expf(-dt * 6));
        if (e->stT > 0.25f) e->aim = dir;
        if (e->stT <= 0) {
            e->st = BS_RAM; e->stT = 1.3f; e->vel = vmul(e->aim, 42);
            play(S_DASH, 0.9f, 0.6f);
        }
    } else if (e->st == BS_RAM) {
        if (e->stT > 1.0f) e->aim = vnorm(vlerp(e->aim, dir, fminf(1, dt * 1.5f)));   /* le debut de la charge suit un peu */
        e->vel = vmul(e->aim, 42);
        if (frand() < 0.8f) add_part(e->pos, vmul(rand_dir(), 2), enemy_color(e->type), 0.4f, 0.3f, 0);
        if (boss_ram_contact(i) || e->stT <= 0) { if (e->alive) { e->st = BS_RECOVER; e->stT = 0.7f; e->combo++; } }
    }
}
static void boss_move_to(Enemy *e, V3 target, float speed, float dt) {
    V3 tv = vsub(target, e->pos);
    e->vel = vlerp(e->vel, vmul(vnorm(tv), speed * clampf(vlen(tv) / 6, 0.2f, 1)), fminf(1, dt * 4));
}
/* point de deplacement autour du joueur, a la hauteur des ennemis volants de la carte */
static V3 boss_spot_near_player(float rmin, float rmax) {
    const MapDef *md = &maps[curMap];
    V3 best = P.pos;
    for (int k = 0; k < 12; k++) {
        float a = frange(0, 2 * PI), r = frange(rmin, rmax);
        V3 p = v3(P.pos.x + cosf(a) * r, frange(md->enemyMinY, md->enemyMaxY), P.pos.z + sinf(a) * r);
        float hl = hlen(p), lim = md->enemyRMax + 10;
        if (hl > lim) { p.x *= lim / hl; p.z *= lim / hl; }
        best = p;
        if (los(p, pcenter())) break;
    }
    return best;
}

/* --- PYRAMIDE --- */
static void pyra_tick(int i, float dt) {
    Enemy *e = &E[i];
    V3 pc = pcenter();
    int phantom = e->summoned == 2;
    if (!phantom && !e->phase && e->hp <= 0.5f * e->maxHp) {
        e->phase = 1; e->st = BS_MOVE; e->stT = 0.6f; e->combo = 0;
        snprintf(msg, sizeof msg, "ELLE FONCE : PARE !"); msgT = 2.5f;
        e->aim = boss_spot_near_player(24, 30);
        play(S_WAVE, 0.8f, 0.6f);
    }
    switch (e->st) {
    case BS_MOVE:
        boss_move_to(e, e->aim, 30, dt);
        if (vlen(vsub(e->aim, e->pos)) < 2.5f || e->stT <= 0) {
            if (e->phase) { e->st = BS_AIM; e->stT = 0.9f; e->combo = 0; }
            else { e->st = BS_VOLLEY; e->stT = 0; e->fireT = 0.25f; e->frac = 5; }
        }
        break;
    case BS_VOLLEY:                                       /* salve de 5 orbes, une toutes les 0,2 s */
        e->vel = vmul(e->vel, expf(-dt * 6));
        if (e->fireT <= 0 && e->frac > 0 && !P.dead) {
            enemy_shoot(e, vnorm(vsub(pc, e->pos)), 18);
            e->frac--; e->fireT = 0.2f;
        }
        if (e->frac <= 0 && e->fireT <= 0) {
            e->combo++;
            if (!e->phase && e->combo % 3 == 0) { e->st = BS_CHARGE; e->stT = 2.5f; play(S_WAVE, 0.7f, 1.6f); vg_hint("GROSSE ORBE : PARE-LA"); }
            else { e->st = BS_MOVE; e->stT = 2.0f; e->aim = boss_spot_near_player(18, 28); }
        }
        break;
    case BS_CHARGE:                                       /* grosse attaque chargee : la pointe s'illumine */
        e->vel = vmul(e->vel, expf(-dt * 6));
        if (frand() < 0.9f) {
            V3 tip = vadd(e->pos, v3(0, enemy_radius(e->type) * 0.9f, 0)), from = vadd(tip, vmul(rand_dir(), 5));
            add_part(from, vmul(vsub(tip, from), 2.5f), v3(1, 0.8f, 0.3f), 0.4f, 0.18f, 0);
        }
        if (e->stT <= 0) {
            V3 tip = vadd(e->pos, v3(0, enemy_radius(e->type) * 0.9f, 0));
            Proj *p = spawn_proj(PR_ORB, tip, vmul(vnorm(vsub(pc, tip)), 16));
            if (p) { p->big = 1; p->life = 8; }
            play(S_EXPLO, 0.7f, 0.5f);
            e->st = BS_RECOVER; e->stT = 0.9f;
        }
        break;
    case BS_AIM: case BS_RAM:
        boss_ram_tick(i, dt);
        break;
    case BS_RECOVER:
        e->vel = vmul(e->vel, expf(-dt * 4));
        if (e->stT <= 0) {
            if (phantom) { burst(e->pos, enemy_color(e->type), 40, 8, 0.4f, 0.15f); e->alive = 0; return; }
            if (e->phase && e->combo < 3) { e->st = BS_AIM; e->stT = 0.9f; }
            else if (e->phase) {                          /* fin du triple : salve, puis on s'eloigne */
                e->st = BS_VOLLEY; e->fireT = 0.3f; e->frac = 5; e->combo = 0;
                if (vgSecret) {                           /* PRISME : trois fantomes de trois cotes */
                    snprintf(msg, sizeof msg, "PRISME"); msgT = 1.5f;
                    for (int k = 0; k < 3; k++) {
                        float a = k * 2 * PI / 3 + frange(0, 1);
                        V3 at = vadd(pc, v3(cosf(a) * 22, 4, sinf(a) * 22));
                        int f = spawn_enemy_at(EN_PYRA, at, 2);
                        if (f >= 0) { E[f].hp = E[f].maxHp = 1; E[f].st = BS_AIM; E[f].stT = 1.1f + k * 0.4f; E[f].phase = 1; }
                    }
                }
            } else { e->st = BS_MOVE; e->stT = 2.0f; e->aim = boss_spot_near_player(18, 28); }
        }
        break;
    case BS_STUN:
        e->vel = vmul(e->vel, expf(-dt * 3));
        e->vel.y -= 4 * dt;
        if (e->stT <= 0) { e->st = BS_MOVE; e->stT = 2.0f; e->aim = boss_spot_near_player(22, 30); e->combo = 0; }
        break;
    }
}

/* --- TESSERACT --- */
static void tess_tick(int i, float dt) {
    Enemy *e = &E[i];
    V3 pc = pcenter();
    int phase2 = e->hp <= 0.5f * e->maxHp;
    if (phase2 && !e->phase) { e->phase = 1; snprintf(msg, sizeof msg, "LE TESSERACT S'EMBALLE"); msgT = 2.5f; }
    e->pos = e->home; e->vel = v3(0, 0, 0);               /* totalement immobile */
    e->fracT -= dt;                                       /* invocations */
    if (e->fracT <= 0) {
        e->fracT = phase2 ? 4.0f : 6.0f;
        static const int kinds[4] = { EN_DRONE, EN_RUSHER, EN_SWARM, EN_SNIPER };
        for (int k = 0; k < 4 && boss_summoned_alive() < 14; k++) {
            int t = kinds[(e->combo + k) % 4];
            V3 at = vadd(e->pos, vmul(rand_dir(), enemy_radius(e->type) + 3));
            int n = t == EN_SWARM ? 3 : 1;
            for (int s = 0; s < n; s++) spawn_enemy_at(t, vadd(at, vmul(rand_dir(), 1.2f)), 1);
            add_beam(e->pos, at, enemy_color(e->type), 0.3f);
        }
        e->combo++;
        play(S_WAVE, 0.5f, 1.3f);
    }
    if (e->frac > 0) {                                    /* spirale en cours : 2 orbes toutes les 0,08 s */
        if (e->stT <= 0) {
            e->stT = 0.08f; e->frac--; e->yaw += 0.33f;
            for (int s = 0; s < 2; s++) {
                float a = e->yaw + s * PI;
                enemy_shoot(e, vnorm(v3(cosf(a), -0.15f, sinf(a))), 10);
            }
        }
    }
    if (e->fireT <= 0 && !P.dead) {                       /* barrages */
        e->fireT = phase2 ? 3.5f : 5.0f;
        e->st = (e->st + 1) % 3;
        V3 to = vsub(pc, e->pos);
        float hd = fmaxf(hlen(to), 1), tilt = clampf(to.y / hd, -0.6f, 0.6f);
        if (e->st == 0) {                                 /* anneau penche vers le joueur */
            for (int k = 0; k < 18; k++) { float a = k * 2 * PI / 18; enemy_shoot(e, vnorm(v3(cosf(a), tilt, sinf(a))), 11); }
        } else if (e->st == 1) { e->frac = 30; e->stT = 0; }   /* spirale */
        else {                                            /* eventail de 7 vers le joueur */
            V3 d = vnorm(to), side = vnorm(vcross(d, v3(0, 1, 0)));
            for (int k = -3; k <= 3; k++) enemy_shoot(e, vnorm(vadd(d, vmul(side, k * 0.14f))), 14);
        }
    }
    if (vgSecret) {                                       /* HYPERCUBE : toutes les 20 s */
        e->aim.x -= dt;
        if (e->aim.x <= 0) {
            e->aim.x = 20;
            HYP.on = 1; HYP.t = 1.2f; HYP.c = pc;
            snprintf(msg, sizeof msg, "HYPERCUBE"); msgT = 1.5f;
        }
    }
    if (boss_links(i) >= VG_LINKS) vg_hint("BOUCLIER : TUE SES INVOCATIONS");
}

/* --- SPHERE (boule) --- */
static void ball_tick(int i, float dt) {
    Enemy *e = &E[i];
    V3 pc = pcenter();
    float R = enemy_radius(e->type);
    int phase2 = e->hp <= 0.5f * e->maxHp;
    if (phase2 && !e->phase) { e->phase = 1; snprintf(msg, sizeof msg, "LA SPHERE ENRAGE"); msgT = 2.5f; }
    int grounded = ray_world(e->pos, v3(0, -1, 0), R + 0.4f, 0) <= R + 0.25f;
    if (e->st == BS_MOVE) {                               /* roule vers le joueur */
        V3 to = v3(pc.x - e->pos.x, 0, pc.z - e->pos.z);
        float vmax = phase2 ? 24.0f : 20.0f;
        V3 h = v3(e->vel.x, 0, e->vel.z);
        h = vadd(h, vmul(vnorm(to), 16 * dt));
        if (hlen(h) > vmax) h = vmul(h, vmax / hlen(h));
        e->vel.x = h.x; e->vel.z = h.z;
        if (e->stT <= 0 && grounded) {                    /* bond vers le joueur */
            V3 tgt = vadd(P.pos, vmul(v3(P.vel.x, 0, P.vel.z), 0.4f));
            float T = 1.5f, dh = tgt.y - (e->pos.y - R);
            float vy = clampf((dh + 0.5f * GRAVITY * T * T) / T, 14, 30);
            V3 hv = vmul(v3(tgt.x - e->pos.x, 0, tgt.z - e->pos.z), 1 / T);
            if (hlen(hv) > 26) hv = vmul(hv, 26 / hlen(hv));
            e->vel = v3(hv.x, vy, hv.z);
            e->st = BS_JUMP; e->combo = phase2 ? 2 : 1; e->stT = 0.25f;
            bossJumps++;
            play(S_JUMP, 0.9f, 0.5f);
        }
    } else if (e->st == BS_JUMP) {
        if (e->stT <= 0 && grounded && e->vel.y <= 0.5f) {   /* frappe le sol */
            V3 g = v3(e->pos.x, e->pos.y - R, e->pos.z);
            gwave_add(g, phase2 ? 34.0f : 28.0f);
            fx_explosion(g, 6);
            burst(g, enemy_color(e->type), 80, 16, 0.6f, 0.25f);
            play(S_EXPLO, 1.0f, 0.5f);
            shake += 0.6f;
            if (hlen(vsub(P.pos, g)) < R + 2.5f && P.pos.y - g.y < 2.5f)
                boss_hit_player(vnorm(v3(P.pos.x - g.x, 0.6f, P.pos.z - g.z)), VG_ROLL_DMG, v3(0, 10, 0));
            if (--e->combo > 0) {                         /* phase 2 : second rebond */
                V3 hv = vmul(vnorm(v3(pc.x - e->pos.x, 0, pc.z - e->pos.z)), 12);
                e->vel = v3(hv.x, 12, hv.z); e->stT = 0.25f;
            } else {
                e->st = BS_MOVE; e->stT = phase2 ? frange(3.5f, 5.0f) : frange(5.0f, 7.0f);
                if (vgSecret && bossJumps % 3 == 0) {     /* METEORES : trois impacts sur tes pas */
                    snprintf(msg, sizeof msg, "METEORES"); msgT = 1.5f;
                    for (int k = 0; k < 3; k++) meteor_add(v3(P.pos.x, ground_below(vadd(P.pos, v3(0, 0.5f, 0)), 30), P.pos.z), 0.9f + k * 0.7f);
                }
            }
        }
    }
    e->vel.y -= GRAVITY * dt;
    V3 before = e->vel;
    e->pos = vadd(e->pos, vmul(e->vel, dt));
    sphere_pushout(&e->pos, &e->vel, R);
    if (hlen(before) > 6 && hlen(e->vel) < 0.3f * hlen(before) && e->st == BS_MOVE) {   /* mur : rebond */
        e->vel.x = -before.x * 0.5f; e->vel.z = -before.z * 0.5f;
    }
    e->yaw += hlen(e->vel) / R * dt;                      /* angle de roulement (rendu) */
    if (e->st == BS_MOVE && !P.dead && e->fireT <= 0 && vlen(vsub(pc, e->pos)) < R + 0.7f) {   /* ecrase au contact */
        V3 d = vnorm(vsub(pc, e->pos));
        boss_hit_player(d, VG_ROLL_DMG, vadd(vmul(d, 18), v3(0, 8, 0)));
        e->fireT = 1.2f;
    }
    float ky = maps[curMap].killY > -999 ? maps[curMap].killY : FALL_DEATH_Y;
    if (e->pos.y < ky + 2) {                              /* tombee : revient du ciel sur son depart */
        e->pos = vadd(e->home, v3(0, 25, 0)); e->vel = v3(0, 0, 0);
        e->st = BS_JUMP; e->combo = 1; e->stT = 0.25f;
    }
    if (P.grounded && !P.dead) vg_hint("FRAPPE-LA DEPUIS LES AIRS");
}

/* --- HEXAEDRE --- */
static float hexaSweepT;                                  /* DEPLIAGE : temps depuis le dernier balayage */
static void hexa_tick(int i, float dt) {
    Enemy *e = &E[i];
    V3 pc = pcenter();
    float f = e->hp / e->maxHp;
    int ph = f > 0.75f ? 0 : f > 0.5f ? 1 : f > 0.25f ? 2 : 3;
    static const char *names[4] = { "PRECISION", "PARRY", "FOULE", "TOUT" };
    if (ph != e->phase) {                                 /* nouvelle phase */
        e->phase = ph; e->combo = 0; e->frac = 0; e->stT = 1.0f; e->fireT = 2.0f; e->fracT = 1.0f;
        e->st = ph == 0 ? BS_LASER : ph == 1 ? BS_MOVE : ph == 2 ? BS_HOLD : BS_MOVE;
        e->aim = boss_spot_near_player(20, 28);
        snprintf(msg, sizeof msg, "PHASE %d : %s", ph + 1, names[ph]); msgT = 2.5f;
        play(S_WAVE, 0.9f, 0.7f);
        if (vgSecret && ph == 3) {                        /* DEPLIAGE */
            SWP.on = 1; SWP.c = e->home; SWP.y = ground_below(vadd(P.pos, v3(0, 0.5f, 0)), 40) + 0.7f;
            SWP.ang = 0; SWP.tele = 1.4f; SWP.t = 7.0f;
            snprintf(msg, sizeof msg, "DEPLIAGE"); msgT = 2;
        }
    }
    e->yaw += dt * (e->st == BS_OPEN ? 0.3f : 1.1f);
    float hover = e->home.y;
    switch (e->st) {
    case BS_LASER:                                        /* PRECISION : trois rayons annonces, puis coeur ouvert */
        boss_move_to(e, vadd(e->home, v3(cosf(e->t * 0.3f) * 14, 0, sinf(e->t * 0.3f) * 14)), 8, dt);
        if (e->stT <= 0 && e->combo < 3 && !P.dead) {
            laser_add(e->pos, vnorm(vsub(vadd(pc, vmul(P.vel, 0.25f)), e->pos)), 0.9f);
            e->combo++; e->stT = 0.7f;
            play(S_HIT, 0.6f, 0.4f);
        } else if (e->combo >= 3 && e->stT <= -0.6f) { e->st = BS_OPEN; e->stT = 2.5f; vg_hint("COEUR OUVERT : TIRE !"); }
        break;
    case BS_OPEN:
        e->vel = vmul(e->vel, expf(-dt * 4));
        if (e->stT <= 0) { e->st = ph == 3 ? BS_MOVE : BS_LASER; e->combo = 0; e->stT = 0.6f; }
        break;
    case BS_MOVE:
        if (ph == 3) {                                    /* TOUT : monte au-dessus du joueur avant de piquer */
            boss_move_to(e, v3(pc.x, fmaxf(pc.y, hover) + 8, pc.z), 22, dt);
            if (e->stT <= 0) { e->st = BS_DROP; e->vel = v3(0, -40, 0); }
        } else {
            boss_move_to(e, e->aim, 26, dt);
            if (vlen(vsub(e->aim, e->pos)) < 2.5f || e->stT <= 0) { e->st = BS_AIM; e->stT = 0.9f; e->combo = 0; }
        }
        break;
    case BS_AIM: case BS_RAM:
        boss_ram_tick(i, dt);
        break;
    case BS_RECOVER:
        e->vel = vmul(e->vel, expf(-dt * 4));
        if (e->stT <= 0) {
            if (ph == 1 && e->combo < 3) { e->st = BS_AIM; e->stT = 0.9f; }
            else if (ph == 1) {                           /* apres le triple : grosse orbe */
                e->st = BS_CHARGE; e->stT = 1.8f; e->combo = 0; vg_hint("GROSSE ORBE : PARE-LA");
            } else { e->st = BS_MOVE; e->stT = 1.6f; e->aim = boss_spot_near_player(20, 28); }
        }
        break;
    case BS_CHARGE:
        e->vel = vmul(e->vel, expf(-dt * 6));
        if (e->stT <= 0) {
            Proj *p = spawn_proj(PR_ORB, e->pos, vmul(vnorm(vsub(pc, e->pos)), 16));
            if (p) { p->big = 1; p->life = 8; p->pos = vadd(e->pos, vmul(vnorm(vsub(pc, e->pos)), enemy_radius(e->type) + 0.5f)); }
            play(S_EXPLO, 0.7f, 0.5f);
            e->st = BS_MOVE; e->stT = 1.6f; e->aim = boss_spot_near_player(20, 28);
        }
        break;
    case BS_HOLD:                                         /* FOULE : au centre, invoque et tire des anneaux */
        boss_move_to(e, e->home, 14, dt);
        e->fracT -= dt;
        if (e->fracT <= 0) {
            e->fracT = 5.0f;
            for (int k = 0; k < 5 && boss_summoned_alive() < 14; k++) {
                int t = k % 2 ? EN_RUSHER : EN_DRONE;
                spawn_enemy_at(t, vadd(e->pos, vmul(rand_dir(), enemy_radius(e->type) + 3)), 1);
            }
        }
        if (e->fireT <= 0 && !P.dead) {
            e->fireT = 4.0f;
            V3 to = vsub(pc, e->pos);
            float tilt = clampf(to.y / fmaxf(hlen(to), 1), -0.6f, 0.6f);
            for (int k = 0; k < 20; k++) { float a = k * 2 * PI / 20 + e->yaw; enemy_shoot(e, vnorm(v3(cosf(a), tilt, sinf(a))), 11); }
        }
        if (boss_links(i) >= VG_LINKS) vg_hint("BOUCLIER : TUE SES INVOCATIONS");
        break;
    case BS_DROP: {                                       /* TOUT : pique au sol, ondes, puis on remonte */
        float R = enemy_radius(e->type);
        e->vel.y = -40;
        if (ray_world(e->pos, v3(0, -1, 0), R + 0.5f, 0) <= R + 0.3f || e->pos.y < maps[curMap].enemyFloor) {
            V3 g = v3(e->pos.x, e->pos.y - R, e->pos.z);
            gwave_add(g, 30); gwave_add(g, 18);
            fx_explosion(g, 7); burst(g, enemy_color(e->type), 90, 16, 0.6f, 0.25f);
            play(S_EXPLO, 1.0f, 0.45f); shake += 0.7f;
            if (hlen(vsub(P.pos, g)) < R + 3 && P.pos.y - g.y < 3) boss_hit_player(v3(0, 1, 0), VG_ROLL_DMG, v3(0, 12, 0));
            e->vel = v3(0, 8, 0);
            e->frac++;
            /* entre deux piques : un rayon, une charge ou une invocation (tout melange) */
            int pick = e->frac % 3;
            if (pick == 0) { e->st = BS_LASER; e->combo = 1; e->stT = 0.6f; }
            else if (pick == 1) { e->st = BS_AIM; e->stT = 0.9f; e->combo = 2; }
            else {
                for (int k = 0; k < 4 && boss_summoned_alive() < 12; k++) spawn_enemy_at(EN_SWARM, vadd(e->pos, vmul(rand_dir(), 6)), 1);
                e->st = BS_MOVE; e->stT = 1.8f;
            }
        }
        break;
    }
    case BS_STUN:
        e->vel = vmul(e->vel, expf(-dt * 3));
        if (e->stT <= 0) { e->st = BS_MOVE; e->stT = 1.6f; e->aim = boss_spot_near_player(20, 28); e->combo = 0; }
        break;
    }
    if (ph == 3 && P.grounded && !P.dead) vg_hint("EN L'AIR : AU SOL TU NE LUI FAIS RIEN");
    if (vgSecret && ph == 3 && !SWP.on) {                 /* DEPLIAGE : revient toutes les 25 s */
        hexaSweepT += dt;
        if (hexaSweepT > 25) {
            hexaSweepT = 0;
            SWP.on = 1; SWP.c = e->home; SWP.y = ground_below(vadd(P.pos, v3(0, 0.5f, 0)), 40) + 0.7f;
            SWP.ang = frange(0, PI); SWP.tele = 1.4f; SWP.t = 7.0f;
        }
    }
}

static void boss_tick(int i, float dt) {
    Enemy *e = &E[i];
    e->stT -= dt; e->fireT -= dt;
    switch (e->type) {
    case EN_PYRA: pyra_tick(i, dt); break;
    case EN_TESS: tess_tick(i, dt); return;               /* immobile : pas de deplacement */
    case EN_BALL: ball_tick(i, dt); return;               /* sa propre physique (gravite, roulement) */
    case EN_HEXA: hexa_tick(i, dt); break;
    }
    if (!e->alive) return;
    e->pos = vadd(e->pos, vmul(e->vel, dt));
    sphere_pushout(&e->pos, &e->vel, enemy_radius(e->type));
    const MapDef *md = &maps[curMap];
    float minY = md->enemyFloor + enemy_radius(e->type) * 0.5f;
    if (e->pos.y < minY) { e->pos.y = minY; if (e->vel.y < 0) e->vel.y = 0; }
}

/* degats recus par un boss : armure, bouclier, coque (comportement, pas de PV en plus) */
static float boss_filter(int i, float dmg) {
    Enemy *e = &E[i];
    if (bossBypass) return dmg;
    if (e->summoned == 2) return 0;                       /* fantome du PRISME : seul un parry le brise */
    int parried = vgProjKind == PR_PARRIED;
    switch (e->type) {
    case EN_PYRA:
        if (e->st == BS_STUN) return dmg * VG_STUN_MULT;
        if (parried) return dmg;
        vg_hint("ARMURE : PARE SES TIRS ET SES CHARGES");
        return dmg * VG_ARMOR;
    case EN_TESS:
        if (boss_links(i) >= VG_LINKS) { vg_hint("BOUCLIER : TUE SES INVOCATIONS"); return 0; }
        return dmg;
    case EN_BALL:
        return P.grounded ? dmg * VG_GROUND_MULT : dmg;
    case EN_HEXA:
        if (e->phase == 0) return e->st == BS_OPEN ? dmg * VG_STUN_MULT : dmg * 0.3f;
        if (e->phase == 1) return e->st == BS_STUN ? dmg * VG_STUN_MULT : parried ? dmg : dmg * VG_ARMOR;
        if (e->phase == 2) return boss_links(i) >= VG_LINKS ? 0 : dmg;
        return P.grounded ? dmg * 0.2f : dmg;
    }
    return dmg;
}

/* apparition du boss de la vague : sous-titre, position libre (le tesseract au centre, la sphere au sol) */
static const char *bossTitle[4] = { "LA PYRAMIDE", "LE TESSERACT", "LA SPHERE", "L'HEXAEDRE" };
static int boss_spawn(int type) {
    const MapDef *md = &maps[curMap];
    float R = enemy_radius(type);
    V3 at;
    if (type == EN_TESS || type == EN_HEXA) {             /* au centre, assez haut pour ne toucher aucun decor */
        at = v3(0, (md->enemyMinY + md->enemyMaxY) * 0.5f, 0);
        for (int k = 0; k < 40 && !sphere_free(at, R + 0.5f); k++) at.y += 1;
    } else if (type == EN_BALL) at = vadd(md->start, v3(0, R + 12, 0));
    else at = enemy_air_spot();
    int i = spawn_enemy_at(type, at, 0);
    if (i < 0) return -1;
    Enemy *e = &E[i];
    e->home = type == EN_BALL ? md->start : at;
    e->st = type == EN_BALL ? BS_JUMP : type == EN_HEXA ? BS_LASER : BS_MOVE;
    e->combo = type == EN_BALL ? 1 : 0;
    e->stT = 1.0f; e->fireT = 2.5f; e->fracT = 1.5f; e->phase = 0;
    e->aim = boss_spot_near_player(18, 28);
    if (type == EN_TESS) { e->aim.x = 8; e->st = 0; }
    if (type == EN_HEXA) hexaSweepT = 0;
    vgBoss = i;
    snprintf(msg, sizeof msg, "%s", bossTitle[type - EN_PYRA]); msgT = 3.0f;
    play(S_WAVE, 1.0f, 0.5f);
    shake += 0.5f;
    return i;
}
