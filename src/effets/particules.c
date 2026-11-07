/* VELOCITY - particules, rayons, lumieres et marques d'impact
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Effets                                                              */
/* ------------------------------------------------------------------ */
static void add_part(V3 p, V3 v, V3 col, float life, float size, float grav) {
    Part *q = &PA[paNext]; paNext = (paNext + 1) % MAXPA;
    q->pos = p; q->vel = v; q->col = col; q->life = q->max = life; q->size = size; q->grav = grav;
}

static void burst(V3 p, V3 col, int n, float speed, float life, float size) {
    for (int i = 0; i < n; i++)
        add_part(p, vmul(rand_dir(), speed * frange(0.2f, 1.0f)), col, life * frange(0.5f, 1.0f), size * frange(0.6f, 1.2f), 6);
}

static void add_light(V3 p, V3 col, float r, float life);
static void decal_at(V3 p, V3 col, float sz, float life, float reach);
static void add_beam(V3 a, V3 b, V3 col, float life) {
    Beam *m = &BM[bmNext]; bmNext = (bmNext + 1) % MAXBM;
    m->a = a; m->b = b; m->col = col; m->life = m->max = life;
    add_light(b, col, 5, 0.3f);                           /* le point d'impact s'eclaire et garde une marque */
    decal_at(b, vmul(col, 0.25f), 0.35f, 6, 0.4f);
}

/* SHOTGUN : traceur de plomb, la tete avance a SG_TRACER_SPEED puis la ligne s'eteint en 0,12 s */
static void add_tracer(V3 a, V3 b, V3 col) {
    Tracer *r = &TR[trNext]; trNext = (trNext + 1) % MAXTR;
    r->a = a; r->b = b; r->col = col; r->t = 0;
    r->life = vlen(vsub(b, a)) / SG_TRACER_SPEED + 0.12f;
}

/* --- effets de rendu (purement visuels, decomptes en temps reel dans render) ---
 * lumieres dynamiques (explosions, rail, roquettes) projetees sur le decor proche, marques d'impact,
 * ondes de choc au sol, fumee (melangee, pas additive) */
typedef struct { V3 p, col; float r, life, max; } DynLight;
typedef struct { V3 p, n, col; float s, life, max, rot; } Decal;
typedef struct { V3 p, n, col; float r, life, max; } Ring;
typedef struct { V3 p, v; float life, max, size, shade; } Smoke;
#define MAXDL 32
#define MAXDC 64
#define MAXRG 16
#define MAXSM 160
static DynLight DL[MAXDL];
static Decal DC[MAXDC];
static Ring RG[MAXRG];
static Smoke SMK[MAXSM];
static int dlNext, dcNext, rgNext, smNext;
static void add_light(V3 p, V3 col, float r, float life) {
    DynLight *l = &DL[dlNext]; dlNext = (dlNext + 1) % MAXDL;
    l->p = p; l->col = col; l->r = r; l->life = l->max = life;
}
static void add_decal(V3 p, V3 n, V3 col, float sz, float life) {
    Decal *d = &DC[dcNext]; dcNext = (dcNext + 1) % MAXDC;
    d->p = vadd(p, vmul(n, 0.02f)); d->n = n; d->col = col; d->s = sz; d->life = d->max = life; d->rot = frange(0, 2 * PI);
}
/* marque sur la surface la plus proche de p (a moins de reach m), si elle existe */
static void decal_at(V3 p, V3 col, float sz, float life, float reach) {
    static const float dirs[6][3] = { { 0, -1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }, { 0, 0, 1 }, { 0, 0, -1 }, { 0, 1, 0 } };
    float best = reach; V3 bn = v3(0, 1, 0), bp = p;
    for (int k = 0; k < 6; k++) {
        V3 d = v3(dirs[k][0], dirs[k][1], dirs[k][2]), n;
        float t = ray_world(p, d, best, &n);
        if (t < best) { best = t; bn = n; bp = vadd(p, vmul(d, t)); }
    }
    if (best < reach) add_decal(bp, bn, col, sz, life);
}
static void add_smoke(V3 p, V3 v, float life, float size, float shade) {
    Smoke *m = &SMK[smNext]; smNext = (smNext + 1) % MAXSM;
    m->p = p; m->v = v; m->life = m->max = life; m->size = size; m->shade = shade;
}
/* tout ce qu'une explosion ajoute en plus des particules : flash de lumiere, onde au sol, brulure, fumee */
static void fx_explosion(V3 c, float radius) {
    add_light(c, v3(1.0f, 0.6f, 0.25f), radius * 2.6f, 0.45f);
    float t = ray_world(c, v3(0, -1, 0), radius * 1.5f, 0);
    if (t < radius * 1.5f) {
        Ring *g = &RG[rgNext]; rgNext = (rgNext + 1) % MAXRG;
        g->p = vadd(c, v3(0, -t + 0.04f, 0)); g->n = v3(0, 1, 0); g->col = v3(1, 0.7f, 0.35f);
        g->r = radius * 1.4f; g->life = g->max = 0.45f;
    }
    decal_at(c, v3(0.05f, 0.04f, 0.04f), radius * 0.55f, 9, 1.2f);
    for (int i = 0; i < 9; i++)
        add_smoke(vadd(c, vmul(rand_dir(), 0.6f)), vadd(vmul(rand_dir(), 1.5f), v3(0, 1.4f, 0)), frange(1.6f, 2.8f),
                  frange(0.9f, 1.6f), frange(0.15f, 0.3f));
}
