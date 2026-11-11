/* VELOCITY - rendu specifique des armes (scie, tesla, lance, fleau...)
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- Scie : rendu du disque ------------------------------------------- */
/* disque perpendiculaire a axis : lame (fan), 12 dents decalees de spin, moyeu a la couleur du joueur */
static void draw_saw_disc(V3 c, V3 axis, float R, float spin, V3 col, const float *hub) {
    V3 a = vnorm(vcross(axis, fabsf(axis.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0))), b = vcross(axis, a);
    glColor3f(col.x, col.y, col.z);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(c.x, c.y, c.z);
    for (int k = 0; k <= 16; k++) {
        float ang = spin + k * (2 * PI / 16);
        V3 q = vadd(c, vadd(vmul(a, cosf(ang) * R), vmul(b, sinf(ang) * R)));
        glVertex3f(q.x, q.y, q.z);
    }
    glEnd();
    glColor3f(col.x * 0.75f, col.y * 0.75f, col.z * 0.8f);
    glBegin(GL_TRIANGLES);
    for (int k = 0; k < 12; k++) {                        /* dents en crochet : base sur le bord, pointe a 1,25 R */
        float a0 = spin + k * (2 * PI / 12), a1 = a0 + PI / 12;
        V3 q0 = vadd(c, vadd(vmul(a, cosf(a0) * R * 0.95f), vmul(b, sinf(a0) * R * 0.95f)));
        V3 q1 = vadd(c, vadd(vmul(a, cosf(a1) * R * 0.95f), vmul(b, sinf(a1) * R * 0.95f)));
        V3 tip = vadd(c, vadd(vmul(a, cosf(a0) * R * 1.25f), vmul(b, sinf(a0) * R * 1.25f)));
        glVertex3f(q0.x, q0.y, q0.z); glVertex3f(q1.x, q1.y, q1.z); glVertex3f(tip.x, tip.y, tip.z);
    }
    glEnd();
    glColor3f(hub[0], hub[1], hub[2]);
    for (int side = -1; side <= 1; side += 2) {           /* moyeu des deux cotes, decolle de la lame */
        V3 hc = vadd(c, vmul(axis, side * R * 0.03f));
        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(hc.x, hc.y, hc.z);
        for (int k = 0; k <= 10; k++) {
            float ang = k * (2 * PI / 10);
            V3 q = vadd(hc, vadd(vmul(a, cosf(ang) * R * 0.3f), vmul(b, sinf(ang) * R * 0.3f)));
            glVertex3f(q.x, q.y, q.z);
        }
        glEnd();
    }
}
static int saw_out_of(int id) {                           /* le joueur id a-t-il une scie dehors ? */
    for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].kind == PR_SAW && PR[i].owner == id) return 1;
    return 0;
}
/* --- fin Scie : rendu --- */

/* --- Tesla : rendu des arcs ---------------------------------------------- */
/* Les liens (TL) ne passent pas par add_beam (BM[32] serait sature en 0,1 s) : leurs extremites sont resolues a
 * chaque image, l'arc suit donc la visee et la cible entre deux impulsions. Aleatoire propre (arcRng) : le rng
 * global n'est jamais touche par le rendu. */
static unsigned int arcRng;
static float arc_rand(void) { arcRng = arcRng * 1664525u + 1013904223u; return (float)(arcRng >> 8) / 16777216.0f * 2 - 1; }
static V3 np_hand(int i) {                                /* bout du canon d'un joueur distant (comme draw_remote_players) */
    float h = np_height(i), cy = cosf(NP[i].st.yaw), sy = sinf(NP[i].st.yaw);
    V3 off = v3(0.42f * cy + 0.1f * sy, h - 0.6f, 0.42f * sy - 0.1f * cy);
    return vadd(vadd(NP[i].disp, off), vmul(yp_dir(NP[i].st.yaw, NP[i].st.pitch), 0.7f));
}
static V3 tesla_end(int kind, int idx, V3 fb) {           /* position actuelle d'une extremite (fb : derniere connue) */
    switch (kind) {
    case TK_MUZZLE: return vadd(camEye, vadd(vmul(yp_dir(P.yaw, P.pitch), 0.6f), vadd(vmul(camR, 0.22f), vmul(camU, -0.2f))));
    case TK_HAND:   return idx < MAXP && NP[idx].active ? np_hand(idx) : fb;
    case TK_ENEMY:  return E[idx].alive ? E[idx].pos : fb;
    case TK_PLAYER: return idx == myId ? pcenter() : (idx < MAXP && NP[idx].active ? np_center(idx) : fb);
    case TK_COIN:   return CO[idx].alive ? CO[idx].pos : fb;
    }
    return fb;
}
/* eclair entre a et b : extremites fixes, deviation en sinus, forme stable 1/30 s (lisible, mais il scintille) */
static void draw_arc(V3 a, V3 b, unsigned int seed, float alpha, V3 col, int loose) {
    V3 d = vsub(b, a);
    float L = vlen(d);
    if (L < 0.05f) return;
    V3 f = vmul(d, 1 / L), s1 = vnorm(vcross(f, fabsf(f.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0))), s2 = vcross(f, s1);
    int N = (int)(L / 0.7f);
    N = N < 4 ? 4 : N > 28 ? 28 : N;
    float amp = fminf(0.12f + 0.03f * L, 0.6f) * (loose ? 2.0f : 1.0f);
    V3 p[29];
    arcRng = seed + (unsigned int)(nowT * 30);
    for (int k = 0; k <= N; k++) {
        float t = (float)k / N, e = sinf(PI * t) * amp;
        float r1 = arc_rand(), r2 = arc_rand();
        p[k] = vadd(vadd(a, vmul(d, t)), vadd(vmul(s1, r1 * e), vmul(s2, r2 * e)));
    }
    for (int pass = 0; pass < 2; pass++) {                /* halo, puis coeur blanc */
        glLineWidth(pass ? 2.0f : 6.0f);
        if (pass) glColor4f(0.9f, 0.95f, 1.0f, alpha); else glColor4f(col.x, col.y, col.z, 0.3f * alpha);
        glBegin(GL_LINE_STRIP);
        for (int k = 0; k <= N; k++) glVertex3f(p[k].x, p[k].y, p[k].z);
        glEnd();
    }
    glLineWidth(1.5f);                                    /* une branche sur trois */
    glColor4f(col.x, col.y, col.z, 0.5f * alpha);
    for (int k = 1; k < N; k += 3) {
        V3 q = p[k];
        glBegin(GL_LINE_STRIP);
        glVertex3f(q.x, q.y, q.z);
        for (int s = 0; s < 3; s++) {
            float r1 = arc_rand(), r2 = arc_rand(), r3 = arc_rand(), len = 0.3f + 0.1f * arc_rand();
            q = vadd(q, vmul(vnorm(v3(r1, r2, r3 + 0.01f)), len));
            glVertex3f(q.x, q.y, q.z);
        }
        glEnd();
    }
}
static void draw_tesla_arcs(void) {                       /* passe additive */
    for (int i = 0; i < MAXTL; i++) {
        TeslaLink *l = &TL[i];
        if (l->life <= 0) continue;
        V3 a = tesla_end(l->ak, l->ai, l->ap), b = tesla_end(l->bk, l->bi, l->bp);
        float al = clampf(l->life / fminf(l->max, 0.06f), 0, 1);
        int loose = l->bk == TK_POINT;                    /* arc dans le vide : plus pale, plus tordu */
        draw_arc(a, b, l->seed, al * (loose ? 0.5f : 1.0f), l->gold ? v3(1, 0.8f, 0.3f) : v3(0.45f, 0.65f, 1.0f), loose);
    }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texDot);
    glBegin(GL_QUADS);
    for (int i = 0; i < MAXTL; i++) {                     /* lueurs aux points d'impact */
        TeslaLink *l = &TL[i];
        if (l->life <= 0 || l->bk == TK_POINT) continue;
        float al = clampf(l->life / fminf(l->max, 0.06f), 0, 1);
        if (l->gold) glColor4f(1, 0.8f, 0.35f, 0.6f * al); else glColor4f(0.5f, 0.75f, 1, 0.45f * al);
        billboard(tesla_end(l->bk, l->bi, l->bp), 0.3f);
    }
    for (int i = 0; dmMode && i < MAXP; i++)              /* canon des joueurs distants dont l'arc est actif */
        if (np_present(i) && (NP[i].st.pad & ST_ZAP) && NP[i].st.weapon == WP_TESLA) {
            glColor4f(0.55f, 0.8f, 1, 0.7f + 0.3f * sinf(gameTime * 40 + i));
            billboard(np_hand(i), 0.22f);
        }
    glEnd();
    glDisable(GL_TEXTURE_2D);
}
/* viewmodel : corps bleu nuit, 3 bagues de bobine, 2 fourches, 5 temoins de batterie (rouges sous 20) */
static void draw_tesla_vm(void) {
    int stun = tesla_stunned(), on = P.teslaOn;
    glPushMatrix();
    if (on) glTranslatef(sinf(gameTime * 90) * 0.006f, cosf(gameTime * 77) * 0.004f, 0);   /* tremblement en tir */
    glColor3f(0.12f, 0.14f, 0.28f);
    box_faces(v3(-0.04f, -0.04f, -0.45f), v3(0.04f, 0.04f, 0.06f));
    glColor3f(0.1f, 0.1f, 0.12f);
    box_faces(v3(-0.025f, -0.16f, -0.02f), v3(0.025f, -0.04f, 0.06f));
    glColor3f(0.75f, 0.55f, 0.32f);                       /* bobine */
    for (int r = 0; r < 3; r++) {
        float z = -0.16f - r * 0.08f;
        box_faces(v3(-0.052f, -0.052f, z - 0.03f), v3(0.052f, 0.052f, z));
    }
    glColor3f(0.55f, 0.58f, 0.62f);                       /* fourches */
    box_faces(v3(-0.045f, -0.01f, -0.58f), v3(-0.025f, 0.01f, -0.45f));
    box_faces(v3(0.025f, -0.01f, -0.58f), v3(0.045f, 0.01f, -0.45f));
    glDisable(GL_LIGHTING);
    float glow = on ? 0.8f + 0.2f * sinf(gameTime * 50) : 0.25f;
    if (stun) glColor3f(1, 0.2f, 0.15f); else glColor3f(0.4f * glow, 0.75f * glow, 1.0f * glow);
    box_faces(v3(-0.047f, -0.012f, -0.59f), v3(-0.023f, 0.012f, -0.565f));
    box_faces(v3(0.023f, -0.012f, -0.59f), v3(0.047f, 0.012f, -0.565f));
    for (int i = 0; i < 5; i++) {                         /* temoins : 1 par tranche de 20 de batterie */
        float z = -0.13f + i * 0.04f;
        int lit = P.ammo[WP_TESLA] > i * 20, low = P.ammo[WP_TESLA] < 20;
        if (!lit) glColor3f(0.08f, 0.08f, 0.12f);
        else if (low) glColor3f(1, 0.25f, 0.2f);
        else glColor3f(0.35f, 0.85f, 1);
        box_faces(v3(-0.05f, 0.005f, z), v3(-0.041f, 0.025f, z + 0.025f));
    }
    /* etincelle entre les fourches : continue en tir, une toutes les 0,5 s au repos */
    if (!stun && (on || fmodf(gameTime, 0.5f) < 0.06f)) {
        arcRng = (unsigned int)(gameTime * 40) * 2654435761u;
        glLineWidth(2);
        glColor3f(0.85f, 0.95f, 1);
        glBegin(GL_LINE_STRIP);
        for (int k = 0; k <= 4; k++) glVertex3f(-0.035f + k * 0.0175f, (k % 4 ? arc_rand() * 0.012f : 0), -0.575f);
        glEnd();
    }
    if (on) {                                             /* lueur constante au bout */
        glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texDot);
        float s = 0.07f + 0.02f * sinf(gameTime * 60);
        glColor4f(0.5f, 0.8f, 1, 0.9f);
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex3f(-s, -s, -0.6f); glTexCoord2f(1, 0); glVertex3f(s, -s, -0.6f);
        glTexCoord2f(1, 1); glVertex3f(s, s, -0.6f); glTexCoord2f(0, 1); glVertex3f(-s, s, -0.6f);
        glEnd();
        glDisable(GL_TEXTURE_2D);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    glPopMatrix();
}
/* --- fin Tesla : rendu --- */
/* --- Lance : rendu --------------------------------------------------- */
/* lance le long de -z : talon en z = 0.35, pointe (non eclairee, couleur tip) en z = -len.
 * wave >= 0 : fanion qui ondule (temps) ; bar > 0 : barre de duree restante le long de la hampe */
static void lance_model(float len, V3 tip, float wave, float bar) {
    glColor3f(0.45f, 0.32f, 0.2f);                        /* hampe en bois */
    box_faces(v3(-0.025f, -0.025f, -len + 0.25f), v3(0.025f, 0.025f, 0.35f));
    glColor3f(0.55f, 0.57f, 0.62f);                       /* rondelle (vamplate) */
    box_faces(v3(-0.08f, -0.08f, -0.07f), v3(0.08f, 0.08f, 0.0f));
    glColor3f(0.3f, 0.3f, 0.34f);                         /* collet de la pointe */
    box_faces(v3(-0.04f, -0.04f, -len + 0.3f), v3(0.04f, 0.04f, -len + 0.22f));
    glDisable(GL_LIGHTING);
    glColor3f(tip.x, tip.y, tip.z);
    glBegin(GL_TRIANGLES);                                /* pointe : pyramide */
    {
        float z0 = -len + 0.25f, s = 0.045f;
        V3 q[4] = { v3(-s, -s, z0), v3(s, -s, z0), v3(s, s, z0), v3(-s, s, z0) };
        for (int k = 0; k < 4; k++) {
            V3 a = q[k], b = q[(k + 1) % 4];
            glVertex3f(a.x, a.y, a.z); glVertex3f(b.x, b.y, b.z); glVertex3f(0, 0, -len);
        }
    }
    glEnd();
    if (wave >= 0) {                                      /* fanion rouge, l'extremite libre ondule */
        float z0 = -len + 0.36f, z1 = -len + 0.75f, w = sinf(wave * 20) * 0.04f;
        glColor3f(0.85f, 0.15f, 0.15f);
        glBegin(GL_TRIANGLES);
        glVertex3f(0, 0.03f, z0); glVertex3f(w, 0.2f, z1); glVertex3f(0, 0.03f, z1);
        glVertex3f(0, 0.03f, z0); glVertex3f(0, 0.03f, z1); glVertex3f(w, 0.2f, z1);
        glEnd();
    }
    if (bar > 0) {                                        /* barre des 2 s de lance couchee */
        glColor3f(1, 0.6f, 0.2f);
        box_faces(v3(-0.012f, 0.026f, -0.08f - (len - 0.6f) * bar), v3(0.012f, 0.034f, -0.08f));
    }
    glEnable(GL_LIGHTING);
}
static V3 lance_tip_col(float vc) {                       /* bleu, orange quand la lance est armee, blanc au plafond PvP */
    return vc >= 15 ? v3(1, 1, 1) : vc >= LANCE_VC_MIN ? v3(1, 0.55f, 0.15f) : v3(0.3f, 0.6f, 1);
}
/* viewmodel : levee a gauche au repos, translation avant pour l'estoc, abaissee sur l'axe de visee en 0,1 s */
static void draw_lance_vm(void) {
    float cf = P.lanceState == LS_COUCH ? clampf((P.lanceT - LANCE_TAP) / 0.1f, 0, 1) : 0;
    float k = P.meleeT > 0 ? clampf(1 - P.meleeT / 0.2f, 0, 1) : 0;
    float thrust = (P.lanceState != LS_COUCH && P.meleeT > 0) ? -0.5f * sinf(PI * k) : 0;
    V3 tip = P.lanceState == LS_COUCH ? lance_tip_col(P.lanceVc) : v3(0.75f, 0.78f, 0.85f);
    glPushMatrix();
    glTranslatef(-1.25f + 0.95f * cf, -0.3f + 0.24f * cf, 0.2f + 0.1f * cf + thrust);
    glRotatef(60 * (1 - cf) + 2 * cf, 1, 0, 0);
    glRotatef(20 * (1 - cf), 0, 0, 1);                    /* levee : la pointe penche vers la gauche, hors du centre */
    lance_model(2.4f, tip, cf > 0 ? gameTime : -1, cf > 0 ? 1 - P.lanceT / LANCE_HOLD_MAX : 0);
    glPopMatrix();
}
/* --- fin Lance : rendu --- */

/* --- Ventouse : lanceur (tube carre, disque en facade, mine chargee) ; deux LED = mines actives, clignotantes en vol --- */
static void draw_mine_vm(float ready) {
    int n = 0, fly = 0;
    for (int i = 0; i < MAXPR; i++)
        if (mine_own(&PR[i])) { n++; fly |= PR[i].mineState == MS_FLY; }
    glColor3f(0.28f, 0.3f, 0.34f);
    box_faces(v3(-0.06f, -0.06f, -0.34f), v3(0.06f, 0.06f, 0.08f));
    glColor3f(0.2f, 0.2f, 0.22f);
    box_faces(v3(-0.03f, -0.17f, -0.02f), v3(0.03f, -0.06f, 0.06f));
    glColor3f(0.13f, 0.13f, 0.15f);
    box_faces(v3(-0.085f, -0.085f, -0.38f), v3(0.085f, 0.085f, -0.34f));
    glDisable(GL_LIGHTING);
    if (P.ammo[WP_MINE] > 0) {
        glColor3f(1.0f * ready, 0.35f * ready, 0.35f * ready);
        box_faces(v3(-0.04f, -0.04f, -0.395f), v3(0.04f, 0.04f, -0.38f));
    }
    for (int s = 0; s < MINE_MAX_ACTIVE; s++) {
        float b = s < n ? ((fly && fmodf(gameTime, 0.2f) < 0.1f) ? 0.3f : 1.0f) : 0.12f;
        glColor3f(0.3f * b, 1.0f * b, 0.7f * b);
        box_faces(v3(-0.045f + s * 0.05f, 0.06f, -0.2f), v3(-0.015f + s * 0.05f, 0.075f, -0.16f));
    }
}
/* --- fin Ventouse --- */
/* --- Fleau : rendu ---------------------------------------------------- */
/* tete a pointes centree en c (demi-taille s) : boite + 3 pointes le long des axes */
static void flail_head_model(V3 c, float s) {            /* yoyo : deux disques rouges autour d'un axe d'acier */
    glColor3f(0.85f, 0.18f, 0.15f);
    float r = s * 1.5f, e = s * 0.35f;
    box_faces(vsub(c, v3(r, r, e * 2.2f)), vadd(c, v3(r, r, -e * 0.4f)));
    box_faces(vsub(c, v3(r, r, -e * 0.4f)), vadd(c, v3(r, r, e * 2.2f)));
    glColor3f(0.55f, 0.57f, 0.62f);
    box_faces(vsub(c, v3(s * 0.35f, s * 0.35f, e * 2.4f)), vadd(c, v3(s * 0.35f, s * 0.35f, e * 2.4f)));
}
/* ficelle de a a b : un seul trait clair, retombant en parabole de fleche sag */
static void draw_chain_w(V3 a, V3 b, float sag, float width) {
    glLineWidth(width);
    glBegin(GL_LINES);
    glColor3f(0.92f, 0.9f, 0.82f);
    for (int s = 0; s < 16; s++) {
        float u0 = s / 16.0f, u1 = (s + 1) / 16.0f;
        V3 p0 = vadd(vlerp(a, b, u0), v3(0, -sag * 4 * u0 * (1 - u0), 0));
        V3 p1 = vadd(vlerp(a, b, u1), v3(0, -sag * 4 * u1 * (1 - u1), 0));
        glVertex3f(p0.x, p0.y, p0.z); glVertex3f(p1.x, p1.y, p1.z);
    }
    glEnd();
}
static void draw_chain(V3 a, V3 b, float sag) { draw_chain_w(a, b, sag, 2); }
/* main gauche du joueur distant i (comme draw_remote_players) */
static V3 np_flail_hand(int i) {
    NetPlayer *n = &NP[i];
    float cy = cosf(n->st.yaw), sy = sinf(n->st.yaw), ox = -0.4f, oz = -0.1f;
    return vadd(n->disp, v3(ox * cy - oz * sy, np_height(i) - 0.6f, ox * sy + oz * cy));
}
/* chaine du joueur distant i pendant son coup, sinon 0 */
static int np_flail_chain(int i, V3 *hand, V3 *head, float *sag) {
    NetPlayer *n = &NP[i];
    if (!np_present(i) || (n->st.pad & 0x0F) != MK_FLAIL || !n->st.melee) return 0;
    *hand = np_flail_hand(i);
    *head = flail_head(*hand, n->st.yaw, n->st.pitch, 1 - n->st.melee / 255.0f, flail_range(vlen(np_vel(i))), 0, *hand, sag);
    return 1;
}
/* chaines dans le monde (6 a 8 m : profondeur reelle, pas dans le viewmodel), puis halos additifs des tetes. Traction (appui long) :
 * une chaine tendue de la main jusqu'a chaque cible accrochee (les miennes, puis celles des autres joueurs : baux EV_FLAIL) */
static void draw_flails(V3 eye) {
    V3 heads[MAXP + 1 + FLAIL_HOOK_MAX + MAXP], hand, head;
    int nh = 0;
    float sag;
    if (P.meleeKind == MK_FLAIL && !P.dead && !spectating) {
        if (P.flailPullN > 0) {
            hand = flail_hand(eye);
            for (int h = 0; h < P.flailPullN; h++) {
                V3 c;
                if (!flail_target(P.flailHk[h], &c)) continue;
                draw_chain_w(hand, c, 0, 3.5f);           /* chaine tendue : un trait plus epais que le balayage */
                heads[nh++] = c;
            }
        } else if (P.flailT > 0) {
            head = flail_my_head(eye, &sag);
            draw_chain(flail_hand(eye), head, sag);
            heads[nh++] = head;
        }
    }
    for (int i = 0; dmMode && i < MAXP; i++)
        if (np_flail_chain(i, &hand, &head, &sag)) { draw_chain(hand, head, sag); heads[nh++] = head; }
    for (int v = 0; dmMode && v < MAXP; v++) {            /* tractions des autres : du lanceur a sa victime */
        int by = flailLinkBy[v] - 1;
        if (flailLinkT[v] <= 0 || by < 0 || by >= MAXP || by == myId || !np_present(by) || (v != myId && !np_present(v))) continue;
        head = v == myId ? pcenter() : np_center(v);
        draw_chain_w(np_flail_hand(by), head, 0, 3.5f);
        heads[nh++] = head;
    }
    if (!nh) return;
    glEnable(GL_LIGHTING);
    for (int k = 0; k < nh; k++) flail_head_model(heads[k], 0.06f);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE); glDepthMask(GL_FALSE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texDot);
    glBegin(GL_QUADS);
    glColor4f(0.8f, 0.85f, 1, 0.45f);
    for (int k = 0; k < nh; k++) billboard(heads[k], 0.25f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE); glDisable(GL_BLEND);
}
/* viewmodel : manche incline dans la main gauche ; au repos, tete suspendue et balancee par le mouvement.
 * Pendant le coup, la tete est dans le monde (draw_flails) : seul le manche reste, releve */
static void draw_flail_vm(void) {
    float k = P.flailT > 0 ? clampf(1 - P.flailT / FLAIL_ANIM, 0, 1) : 0, lift = P.flailT > 0 ? sinf(PI * k) : 0;
    int pulling = P.flailPullN > 0;                       /* traction : manche leve, la tete est au bout des chaines (draw_flails) */
    if (pulling) lift = 0.85f;
    glPushMatrix();
    glTranslatef(-0.9f, -0.22f, -0.3f);
    glRotatef(-25 - 50 * lift, 1, 0, 0);                  /* incline vers l'avant, releve pendant le lancer */
    glRotatef(12 + 25 * lift, 0, 0, 1);
    glColor3f(0.92f, 0.9f, 0.82f);                        /* ficelle nouee au doigt */
    box_faces(v3(-0.008f, 0, -0.008f), v3(0.008f, 0.38f, 0.008f));
    glColor3f(0.4f, 0.41f, 0.45f);                        /* bague */
    box_faces(v3(-0.03f, 0.34f, -0.03f), v3(0.03f, 0.4f, 0.03f));
    if (P.flailT <= 0 && !pulling) {
        float sx = clampf(-P.swayX * 5, -0.15f, 0.15f) + 0.02f * sinf(gameTime * 2.3f);
        float sz = clampf(P.bob * 3, -0.1f, 0.1f) + 0.02f * cosf(gameTime * 1.7f);
        V3 top = v3(0, 0.4f, 0), hd = v3(-0.12f + sx, 0.12f, sz);
        for (int s = 1; s < 6; s++) {                     /* maillons */
            V3 q = vlerp(top, hd, s / 6.0f);
            if (s & 1) glColor3f(0.4f, 0.41f, 0.45f); else glColor3f(0.22f, 0.23f, 0.26f);
            box_faces(vsub(q, v3(0.01f, 0.02f, 0.01f)), vadd(q, v3(0.01f, 0.02f, 0.01f)));
        }
        flail_head_model(hd, 0.045f);
    }
    glPopMatrix();
}
/* --- fin Fleau : rendu --- */

/* --- Poings : rendu --- */
/* un gant : avant-bras, poing d'acier, bobine emissive (couleur coil, sans eclairage) */
static void fist_glove_model(V3 coil) {
    glColor3f(0.2f, 0.21f, 0.24f);                        /* avant-bras */
    box_faces(v3(-0.05f, -0.05f, 0.0f), v3(0.05f, 0.05f, 0.30f));
    glColor3f(0.55f, 0.58f, 0.63f);                       /* poing */
    box_faces(v3(-0.07f, -0.07f, -0.12f), v3(0.07f, 0.07f, 0.0f));
    glDisable(GL_LIGHTING);
    glColor3f(coil.x, coil.y, coil.z);                    /* bobine */
    box_faces(v3(-0.075f, -0.02f, -0.08f), v3(0.075f, 0.02f, -0.04f));
    glEnable(GL_LIGHTING);
}
/* viewmodel : gant gauche (coups 1 et 3) et gant droit (coup 2) ; la bobine va du cyan au blanc chaud avec la vitesse */
static void draw_fist_vm(void) {
    float k = P.meleeT > 0 ? clampf(1 - P.meleeT / FIST_ANIM, 0, 1) : 0, e = P.meleeT > 0 ? sinf(k * PI) : 0;
    float heat = clampf(vlen(P.vel) / 30, 0, 1);
    int st = P.meleeT > 0 ? P.fistAnim : 0;
    V3 coil = v3(0.3f + 0.7f * heat, 0.9f, 1.0f - 0.35f * heat);
    for (int g = 0; g < 2; g++) {                         /* 0 = gauche, 1 = droit */
        /* au repos : bas de l'ecran, de part et d'autre de l'arme ; le coup les amene vers le centre de la vue */
        float x = g ? 0.32f : -0.95f, y = g ? -0.36f : -0.30f, z = g ? 0.0f : -0.05f, rot = 0;
        if (st == 1 && g == 0) { z -= 0.55f * e; x += 0.35f * e; y += 0.15f * e; }          /* jab gauche */
        if (st == 2 && g == 1) { z -= 0.6f * e; x -= 0.3f * e; y += 0.2f * e; }             /* direct droit */
        if (st == 3 && g == 0) { y += 0.4f * e; z -= 0.35f * e; x += 0.3f * e; rot = -50 * e; }   /* uppercut gauche */
        glPushMatrix();
        glTranslatef(x, y, z);
        if (rot != 0) glRotatef(rot, 1, 0, 0);
        fist_glove_model(coil);
        glPopMatrix();
    }
}
/* --- fin Poings : rendu --- */
/* --- Egide : rendu ------------------------------------------------------ */
static V3 shield_col(float gauge) {                       /* bord : cyan a 120 -> rouge a 0 */
    return vlerp(v3(1, 0.2f, 0.15f), v3(0.3f, 0.9f, 1), clampf(gauge / SHIELD_GAUGE_MAX, 0, 1));
}
/* plaque centree sur l'origine, face vers -z : face (alpha < 1 = semi-transparente), bord colore, fissures sous 60 */
static void shield_plate(float w, float h, float th, V3 face, float alpha, V3 edge, float gauge) {
    float x = w * 0.5f, y = h * 0.5f, b = fminf(w, h) * 0.07f;
    glDisable(GL_LIGHTING);
    glColor3f(edge.x, edge.y, edge.z);                    /* bord opaque */
    box_faces(v3(-x, y - b, -th * 1.4f), v3(x, y, th * 0.4f));
    box_faces(v3(-x, -y, -th * 1.4f), v3(x, -y + b, th * 0.4f));
    box_faces(v3(-x, -y, -th * 1.4f), v3(-x + b, y, th * 0.4f));
    box_faces(v3(x - b, -y, -th * 1.4f), v3(x, y, th * 0.4f));
    if (gauge < SHIELD_GAUGE_MAX * 0.5f) {                /* fissures : plus nombreuses quand la jauge baisse */
        int n = gauge < SHIELD_GAUGE_MAX * 0.25f ? 6 : 3;
        glLineWidth(2);
        glColor3f(0.05f, 0.05f, 0.08f);
        glBegin(GL_LINES);
        for (int k = 0; k < n; k++) {
            float a0 = k * 2.4f + 0.3f, cx = 0.15f * w * cosf(a0 * 1.7f), cy = 0.1f * h * sinf(a0);
            glVertex3f(cx, cy, -th * 1.05f);
            glVertex3f(cx + cosf(a0) * x * 0.8f, cy + sinf(a0) * y * 0.8f, -th * 1.05f);
        }
        glEnd();
    }
    glEnable(GL_LIGHTING);
    if (alpha < 1) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE); }
    glColor4f(face.x, face.y, face.z, alpha);
    box_faces(v3(-x + b, -y + b, -th), v3(x - b, y - b, 0));
    if (alpha < 1) { glDepthMask(GL_TRUE); glDisable(GL_BLEND); }
}
/* viewmodel : baisse en bas a gauche (incline), leve devant la vue (face transparente pour garder la visee) */
static void draw_shield_vm(void) {
    if (P.shieldState == SHS_BROKEN) {                    /* brise : le bras seul */
        glColor3f(0.2f, 0.18f, 0.17f);
        box_faces(v3(-0.5f, -0.4f, -0.3f), v3(-0.4f, -0.3f, -0.1f));
        return;
    }
    float k = P.shieldAnim * P.shieldAnim * (3 - 2 * P.shieldAnim);
    /* reperes du viewmodel (decale de +0,2 / -0,19 et reduit de moitie) : leve, la plaque est centree sur le viseur */
    V3 mn = vlerp(v3(-0.95f, -0.45f, -0.36f), v3(-0.78f, 0.0f, -0.40f), k);
    V3 mx = vlerp(v3(-0.55f, 0.0f, -0.30f), v3(-0.02f, 0.72f, -0.34f), k);
    V3 c = vlerp(mn, mx, 0.5f), sz = vsub(mx, mn);
    float push = P.meleeT > 0 ? 0.25f * sinf(PI * clampf(1 - P.meleeT / SHIELD_BASH_ANIM, 0, 1)) : 0;
    V3 edge = P.parryWin > 0 ? v3(1, 1, 1) : shield_col(P.shieldGauge);
    if (P.shieldHitT > 0) edge = vlerp(edge, v3(1, 1, 1), clampf(P.shieldHitT / 0.2f, 0, 1));
    glPushMatrix();
    glTranslatef(c.x, c.y, c.z - push);
    glRotatef(25 * (1 - k), 0, 1, 0);
    glColor3f(0.2f, 0.18f, 0.17f);                        /* poignee (en bas : ne cache pas le viseur une fois leve) */
    box_faces(v3(-0.03f, -sz.y * 0.5f + 0.02f, 0.0f), v3(0.03f, -sz.y * 0.5f + 0.1f, 0.12f));
    shield_plate(sz.x, sz.y, 0.03f, v3(0.55f, 0.6f, 0.68f), k > 0.5f ? 0.3f : 1.0f, edge, P.shieldGauge);
    glPopMatrix();
}
/* --- fin Egide : rendu --- */
