/* VELOCITY - camera de mort : apres un kill, la camera se tourne vers le tueur pendant KILLCAM_T
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* La camera part de mes yeux au moment de la mort, monte un peu, recule et pivote vers le tueur en zoomant. En LAN le
   tueur est connu (EV_KILL) ; en solo on prend l'ennemi vivant le plus proche qui me voit. Ni suicide ni chute.
   Purement visuel : la simulation ne change pas, le respawn garde son delai (RESPAWN_TIME). */

static int killcamOn = 1;                                 /* reglage (camera_mort= dans velocity.ini) */
static int kcOn, kcKiller, kcWeapon, kcEnemy;             /* en cours ; tueur (LAN) ; arme W_* ; ennemi (solo) */
static int kcOneShot;                                     /* drapeaux HF_* du kill (tete, one-shot) : texte de la camera de mort */
static V3 kcFrom, kcLast;                                 /* oeil a la mort ; derniere position connue du tueur */
static float kcYaw0, kcPitch0;

static int killcam_target(V3 *t) {
    if (dmMode) {
        if (kcKiller < 0 || kcKiller >= MAXP || !NP[kcKiller].active) return 0;
        *t = np_center(kcKiller);
    } else {
        if (kcEnemy < 0 || !E[kcEnemy].alive) return 0;
        *t = E[kcEnemy].pos;
    }
    return 1;
}

static void killcam_start(int killer, int weapon) {
    kcOn = 0;
    if (!killcamOn || spectating) return;
    kcFrom = eye_pos(); kcYaw0 = P.yaw; kcPitch0 = P.pitch;
    kcKiller = killer; kcWeapon = weapon; kcEnemy = -1;
    if (dmMode) {
        if (killer < 0 || killer >= MAXP || killer == myId) return;   /* suicide ou chute : pas de camera */
    } else {
        float best = KILLCAM_SOLO_R;
        for (int i = 0; i < MAXE; i++) {                  /* solo : le coupable probable */
            if (!E[i].alive) continue;
            float d = vlen(vsub(E[i].pos, kcFrom));
            if (d < best && los(kcFrom, E[i].pos)) { best = d; kcEnemy = i; }
        }
        if (kcEnemy < 0) return;
    }
    kcOn = killcam_target(&kcLast);
}

static int killcam_active(void) {
    return kcOn && P.dead && P.deathT < KILLCAM_T && !spectating && !matchOver;
}

/* vue de la camera de mort : oeil, cap, inclinaison et facteur de zoom du champ de vision */
static void killcam_view(V3 *eye, float *yaw, float *pitch, float *zoom) {
    V3 tg;
    if (killcam_target(&tg)) kcLast = tg; else tg = kcLast;   /* tueur parti : on garde sa derniere position */
    float k = clampf(P.deathT / KILLCAM_TURN, 0, 1);
    k = k * k * (3 - 2 * k);                              /* demarrage et arrivee en douceur */
    V3 d = vsub(tg, kcFrom), dh = vnorm(v3(d.x, 0, d.z));
    V3 back = v3(-dh.x * KILLCAM_BACK, KILLCAM_UP, -dh.z * KILLCAM_BACK);
    float room = ray_world(kcFrom, vnorm(back), vlen(back) + 0.5f, 0) - 0.5f;   /* pas a travers les murs */
    *eye = vadd(kcFrom, vmul(vnorm(back), fmaxf(0, fminf(room, vlen(back))) * k));
    V3 f = vnorm(vsub(tg, *eye));
    float ty = atan2f(f.x, -f.z), tp = asinf(clampf(f.y, -1, 1));
    float dy = ty - kcYaw0;
    while (dy > PI) dy -= 2 * PI;
    while (dy < -PI) dy += 2 * PI;                        /* le plus court chemin */
    *yaw = kcYaw0 + dy * k;
    *pitch = kcPitch0 + (tp - kcPitch0) * k;
    *zoom = 1 - KILLCAM_ZOOM * k;
}
