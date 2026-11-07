/* VELOCITY - projectiles generiques, lance-roquettes
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

static Proj *spawn_proj(int kind, V3 pos, V3 vel) {
    for (int i = 0; i < MAXPR; i++)
        if (!PR[i].alive) {
            Proj *p = &PR[i];
            memset(p, 0, sizeof *p);                      /* champs de la Scie a zero */
            p->sawBy = p->sawBox = -1;
            p->alive = 1; p->kind = kind; p->pos = pos; p->vel = vel; p->life = 6; p->owner = myId; p->netId = 0;
            p->mult = 1; p->parries = 0; p->orb = kind == PR_ORB;
            return p;
        }
    return NULL;
}

static unsigned int rocketCounter;

static void fire_rocket(void) {
    if (P.ammo[WP_ROCKET] > 0) P.ammo[WP_ROCKET]--;      /* limitees : un rocket jump coute une roquette */
    Proj *p = spawn_proj(PR_ROCKET, eye_pos(), vmul(view_dir(), ROCKET_SPEED));
    if (p && ric_take()) { p->ricLeft = RIC_ROCKET_CAP; p->life = RIC_ROCKET_LIFE; }   /* RICOCHET : rebondit jusqu'a une cible ou la sortie de la carte */
    if (p && dmMode) {
        p->netId = ((unsigned int)(myId + 1) << 24) | (++rocketCounter & 0xFFFFFF);
        net_fx_rocket(p->pos, p->vel, p->netId);
    }
    P.fireCd = ROCKET_CD / streak_rate(); P.recoil = 1.3f; shake += 0.08f;
    muzzleT = 0.08f;
    play(S_ROCKET, 0.8f, frange(0.95f, 1.05f));
}
