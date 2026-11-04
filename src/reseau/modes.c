/* VELOCITY - modes de jeu (frags, chrono, 3 zones, drapeau, bouclier), economie et boutique
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* L'hote fait autorite : zones (prise en restant dedans), drapeaux    */
/* (pris, portes, laches a la mort, rendus en restant a cote sans etre */
/* touche, captures), bouclier (surcharge par les attaquants presents) */
/* et soins du SOUTIEN. L'etat (ObjState) part dans chaque PkSnap ;    */
/* les evenements EV_OBJ donnent les messages et les pieces.           */
/* Economie : chaque joueur tient ses pieces (kills, morts, objectifs) */
/* et achete dans la BOUTIQUE (ecran de mort) ; arme de depart au      */
/* choix parmi 4. Reglages : src/reglages/equilibrage.c (ZONE_*, ...). */
/* ------------------------------------------------------------------ */

/* --- carte : objectifs disponibles (une carte CHALLENGE n'accueille aucun mode : course seulement) --- */
static int mode_ok(int m, int map) {
    if (map < 0 || map >= NMAPS || maps[map].challenge) return 0;
    const MapDef *md = &maps[map];
    if (m == MODE_ZONES) return md->zones != NULL;
    if (m == MODE_DRAPEAU) return md->flags != NULL;
    if (m == MODE_BOUCLIER) return md->shield != NULL;
    return 1;
}
static V3 zone_pos(int z) { const float *p = maps[curMap].zones[z]; return v3(p[0], p[1], p[2]); }
static V3 flag_home(int t) { const float *p = maps[curMap].flags[t]; return v3(p[0], p[1], p[2]); }
static V3 shield_pos(void) { const float *p = maps[curMap].shield; return v3(p[0], p[1], p[2]); }
static float zone_r(void) { return ZONE_R * map_obj_scale(curMap); }            /* rayons a l'echelle de la carte */
static float shield_r(void) { return SHIELD_ZONE_R * map_obj_scale(curMap); }
static V3 flag_pos(int t) { return obj.fl[t].state == FLAG_HOME ? flag_home(t) : fv(obj.fl[t].pos); }
static V3 np_feet(int i) { return i == myId ? P.pos : NP[i].disp; }
static int near_point(int i, V3 c, float r) {             /* pieds a moins de r (a l'horizontale), hauteur proche */
    V3 p = np_feet(i);
    return hlen(vsub(p, c)) < r && fabsf(p.y - c.y) < 3.5f;
}

/* --- evenements d'objectif (hote -> tous) --- */
static void obj_event(int sub, int team, int actor, int idx) {
    NetEvent e = ev_new(EV_OBJ);
    e.sub = (unsigned char)sub; e.target = (unsigned char)team; e.actor = (unsigned char)(actor < 0 ? MAXP : actor);
    e.f[0] = (float)idx;
    net_emit(&e);
    apply_event(&e);
}

static float modeScoreAcc[2], flagDropT[2];
static float zoneProg[NZONES], flagRet[2];                /* hote : progression exacte (l'octet de ObjState ne sert qu'a l'envoi) */
static void mode_reset(void) {
    memset(&obj, 0, sizeof obj);
    for (int z = 0; z < NZONES; z++) obj.z[z].owner = obj.z[z].capTeam = 2;
    for (int t = 0; t < 2; t++) { obj.fl[t].state = FLAG_HOME; obj.fl[t].carrier = MAXP; }
    obj.shield = 100;
    memset(zoneProg, 0, sizeof zoneProg); memset(flagRet, 0, sizeof flagRet); memset(modeScoreAcc, 0, sizeof modeScoreAcc);
}

/* l'hote : un pas de mode (zones, drapeaux, bouclier) */
static void mode_tick(float dt) {
    const MapDef *md = &maps[curMap];
    if (teamSize <= 0) return;
    if (rules.mode == MODE_ZONES && md->zones) {
        for (int z = 0; z < NZONES; z++) {
            ZoneSt *Z = &obj.z[z];
            int n[2] = { 0, 0 };
            for (int i = 0; i < MAXP; i++)
                if (NP[i].active && NP[i].alive && near_point(i, zone_pos(z), zone_r())) n[NP[i].team & 1]++;
            int t = n[0] && !n[1] ? 0 : n[1] && !n[0] ? 1 : -1;
            float prog = zoneProg[z];
            if (t >= 0 && Z->owner != t) {                /* une seule equipe dedans : elle prend la zone */
                if (Z->capTeam != t) { Z->capTeam = (unsigned char)t; prog = 0; }
                prog += dt / ZONE_CAP_T * (1 + 0.3f * (n[t] - 1));
                if (prog >= 1) {
                    Z->owner = (unsigned char)t; prog = 0; Z->capTeam = 2;
                    int who = -1;
                    for (int i = 0; i < MAXP && who < 0; i++) if (NP[i].active && NP[i].alive && (NP[i].team & 1) == t && near_point(i, zone_pos(z), ZONE_R)) who = i;
                    obj_event(OBJ_ZONE, t, who, z);
                }
            } else if (t < 0 && !n[0] && !n[1]) prog = fmaxf(0, prog - dt / (ZONE_CAP_T * 2));   /* personne : ca redescend */
            zoneProg[z] = clampf(prog, 0, 1);
            Z->prog = (unsigned char)(zoneProg[z] * 255);
        }
        for (int t = 0; t < 2; t++) {                     /* chaque zone tenue rapporte un point par seconde */
            int held = 0;
            for (int z = 0; z < NZONES; z++) held += obj.z[z].owner == t;
            modeScoreAcc[t] += held * dt;
            while (modeScoreAcc[t] >= 1) { modeScoreAcc[t] -= 1; obj.pts[t]++; }
            if (obj.pts[t] >= ZONE_SCORE) { host_end_match(t); return; }
        }
    }
    if (rules.mode == MODE_DRAPEAU && md->flags) {
        for (int f = 0; f < 2; f++) {                     /* drapeau de l'equipe f */
            FlagSt *F = &obj.fl[f];
            if (F->state == FLAG_CARRIED) {
                int c = F->carrier;
                if (c >= MAXP || !NP[c].active || !NP[c].alive) { F->state = FLAG_DROPPED; flagDropT[f] = 0; continue; }
                vf(F->pos, vadd(np_feet(c), v3(0, 0, 0)));
                FlagSt *mine = &obj.fl[1 - f];            /* capture : le porteur revient a son propre drapeau, reste chez lui */
                if (mine->state == FLAG_HOME && near_point(c, flag_home(1 - f), FLAG_TOUCH_R)) {
                    obj.pts[1 - f]++;
                    F->state = FLAG_HOME; F->carrier = MAXP; F->ret = 0;
                    obj_event(OBJ_FLAG_CAPTURE, 1 - f, c, f);
                    if (obj.pts[1 - f] >= FLAG_CAPS) { host_end_match(1 - f); return; }
                }
                continue;
            }
            V3 at = flag_pos(f);
            int taker = -1, guard = 0, guardHit = 0;
            for (int i = 0; i < MAXP; i++) {
                if (!NP[i].active || !NP[i].alive || !near_point(i, at, FLAG_TOUCH_R)) continue;
                if ((NP[i].team & 1) != f) { if (taker < 0) taker = i; }
                else if (F->state == FLAG_DROPPED && near_point(i, at, FLAG_RETURN_R)) {
                    guard++;
                    if (nowT - NP[i].lastHitT < FLAG_RETURN_HIT) guardHit = 1;   /* touche : le retour repart de zero */
                }
            }
            if (F->state == FLAG_DROPPED) {
                for (int i = 0; i < MAXP; i++)            /* garde a portee de retour (un peu plus large que le contact) */
                    if (!guard && NP[i].active && NP[i].alive && (NP[i].team & 1) == f && near_point(i, at, FLAG_RETURN_R)) {
                        guard++;
                        if (nowT - NP[i].lastHitT < FLAG_RETURN_HIT) guardHit = 1;
                    }
                flagDropT[f] += dt;
                float r = flagRet[f];
                if (guard && !guardHit && taker < 0) r += dt / FLAG_RETURN_T;
                else if (guardHit) r = 0;
                if (r >= 1 || flagDropT[f] > FLAG_AUTO_RETURN) {
                    int who = -1;
                    for (int i = 0; i < MAXP && who < 0; i++) if (NP[i].active && NP[i].alive && (NP[i].team & 1) == f && near_point(i, at, FLAG_RETURN_R)) who = i;
                    F->state = FLAG_HOME; F->ret = 0; flagDropT[f] = 0; flagRet[f] = 0;
                    obj_event(OBJ_FLAG_RETURN, f, r >= 1 ? who : -1, f);
                    continue;
                }
                flagRet[f] = clampf(r, 0, 1);
                F->ret = (unsigned char)(flagRet[f] * 255);
            }
            if (taker >= 0) {                             /* un adversaire le ramasse (chez lui ou la ou il est tombe) */
                F->state = FLAG_CARRIED; F->carrier = (unsigned char)taker; F->ret = 0; flagRet[f] = 0;
                obj_event(OBJ_FLAG_TAKE, f, taker, f);
            }
        }
    }
    if (rules.mode == MODE_BOUCLIER && md->shield) {      /* les ROUGES surchargent le bouclier des BLEUS */
        int att = 0, def = 0;
        for (int i = 0; i < MAXP; i++)
            if (NP[i].active && NP[i].alive && near_point(i, shield_pos(), shield_r())) { if (NP[i].team & 1) att++; else def++; }
        static float drainAcc;
        int before = obj.shield;
        if (att && !def) {
            drainAcc += dt * SHIELD_DRAIN * (1 + 0.35f * (att - 1));
            while (drainAcc >= 1 && obj.shield > 0) { drainAcc -= 1; obj.shield--; }
        }
        if (before > 50 && obj.shield <= 50) obj_event(OBJ_SHIELD, 0, -1, 50);
        if (before > 20 && obj.shield <= 20) obj_event(OBJ_SHIELD, 0, -1, 20);
        if (obj.shield <= 0) { host_end_match(1); return; }
    }
}
/* le porteur meurt : le drapeau tombe la ou il est (l'hote, depuis host_kill) */
static void mode_on_death(int v) {
    for (int f = 0; f < 2; f++)
        if (obj.fl[f].state == FLAG_CARRIED && obj.fl[f].carrier == v) {
            obj.fl[f].state = FLAG_DROPPED; obj.fl[f].carrier = MAXP; obj.fl[f].ret = 0; flagDropT[f] = 0; flagRet[f] = 0;
            vf(obj.fl[f].pos, NP[v].disp);
            if (v == myId) vf(obj.fl[f].pos, P.pos);
            obj_event(OBJ_FLAG_DROP, f, v, f);
        }
}
/* bots : l'objectif a viser selon le mode (zone a prendre ou a tenir, drapeau, bouclier) ; 0 = aucun */
static int mode_bot_goal(V3 *g) {
    const MapDef *md = &maps[curMap];
    if (!dmMode || teamSize <= 0 || myId >= MAXP || matchOver) return 0;
    int me = NP[myId].team & 1;
    if (rules.mode == MODE_ZONES && md->zones) {
        int best = -1; float bd = 1e9f;
        for (int pass = 0; pass < 2 && best < 0; pass++)  /* d'abord les zones pas a nous, sinon on tient la plus proche */
            for (int z = 0; z < NZONES; z++) {
                if (pass == 0 && obj.z[z].owner == me) continue;
                float d = vlen(vsub(zone_pos(z), P.pos)) + (z == (myId % NZONES) ? -15 : 0);   /* chacun sa preferee : on se repartit */
                if (d < bd) { bd = d; best = z; }
            }
        *g = zone_pos(best);
        return 1;
    }
    if (rules.mode == MODE_DRAPEAU && md->flags) {
        int enemy = 1 - me;
        if (obj.fl[enemy].state == FLAG_CARRIED && obj.fl[enemy].carrier == myId) {   /* je porte : a la maison */
            *g = obj.fl[me].state == FLAG_HOME ? flag_home(me) : flag_pos(me);
            return 1;
        }
        if (obj.fl[me].state != FLAG_HOME && (myId & 1)) { *g = flag_pos(me); return 1; }   /* la moitie va reprendre le notre */
        if (obj.fl[enemy].state == FLAG_CARRIED) { *g = flag_home(me); return 1; }           /* un allie le porte : on escorte */
        *g = flag_pos(enemy);
        return 1;
    }
    if (rules.mode == MODE_BOUCLIER && md->shield) { *g = shield_pos(); return 1; }
    return 0;
}

/* fin du temps : qui gagne selon le mode */
static int mode_time_winner(void) {
    if (teamSize > 0) {
        int a = team_score(0), b = team_score(1);
        if (rules.mode == MODE_ZONES || rules.mode == MODE_DRAPEAU) { a = obj.pts[0]; b = obj.pts[1]; }
        if (rules.mode == MODE_BOUCLIER) return 0;        /* le bouclier a tenu : les BLEUS gagnent */
        return a > b ? 0 : b > a ? 1 : TEAM_DRAW;
    }
    int w = 0;
    for (int i = 0; i < MAXP; i++) if (NP[i].active && NP[i].kills > NP[w].kills) w = i;
    return w;
}
static float match_time_limit(void) { return rules.minutes > 0 ? rules.minutes * 60.0f : MATCH_TIME; }

