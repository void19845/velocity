/* VELOCITY - client : connexion, instantanes, envoi
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* --- client ----------------------------------------------------------- */
static void dm_local_start(void) {
    reset_game();
    dmMode = 1;
    P.dead = 1; P.deathT = 0; P.hp = 0;
    memset(kfeed, 0, sizeof kfeed);
    killerOfMe = -1;
}

static void stop_bot_proc(void) {
    for (int k = 0; k < MAXP; k++) {
#ifdef _WIN32
        if (botProc[k]) { TerminateProcess(botProc[k], 0); CloseHandle(botProc[k]); botProc[k] = NULL; }
#else
        if (botProc[k] > 0) { kill(botProc[k], SIGTERM); waitpid(botProc[k], NULL, 0); botProc[k] = 0; }
#endif
    }
    vsBot = 0;
}

static void net_shutdown(void) {
    rk_shutdown();                                        /* mode classe : rapport en suspens, remise a zero */
    stop_bot_proc();
    if (sock != INVALID_SOCKET) {
        PkHead l;
        head(&l, PK_LEAVE);
        for (int r = 0; r < 3; r++) {
            if (netMode == NET_HOST) host_send_all(&l, sizeof l);
            else if (connected) net_send(&l, sizeof l, &hostAddr);
        }
        closesocket(sock);
        sock = INVALID_SOCKET;
        netlog("deconnexion");
    }
    netMode = NET_OFF; connected = 0; joining = 0; dmMode = 0; matchOver = 0; myId = 0; nFound = 0;
    spectating = 0; specFollow = -1; teamSize = 0;
    memset(NP, 0, sizeof NP);
    memset(SP, 0, sizeof SP);
    online_reset();
    memset(outq, 0, sizeof outq);
}

static void menu_backdrop(void) {
    if (curMap != menuMap) load_map(menuMap);             /* le menu montre la carte choisie */
    reset_game();
    P.pos = P.prevPos = maps[curMap].menuCam;
    P.pitch = maps[curMap].menuPitch;
}

static void net_disconnect_to_menu(const char *why) {
    rk_host_gone();                                       /* mode classe : l'hote parti en plein match */
    net_shutdown();
    set_status(why);
    set_capture(0);
    appState = APP_MENU;
    menu_backdrop();
}

static void on_welcome(int id) {
    myId = id;
    spectating = id >= MAXP; specFollow = -1;
    connected = 1; joining = 0;
    lastSnapT = nowT;
    memset(NP, 0, sizeof NP);
    dm_local_start();
    if (spectating) {                                     /* vue d'ensemble de la carte */
        P.pos = P.prevPos = vadd(maps[curMap].menuCam, v3(0, maps[curMap].specUp, 0)); P.pitch = maps[curMap].menuPitch - 0.08f; P.yaw = 0;
    }
    appState = APP_GAME;
#ifdef _WIN32
    if (!botMode && GetForegroundWindow() == hwnd) set_capture(1);
#else
    if (!botMode && mac_has_focus()) set_capture(1);
#endif
    netlog("connecte a la partie, id %d", id);
}

static void apply_snap(const PkSnap *s) {
    lastSnapT = nowT;
    matchT = clk_match_from_snap(s->matchT, s->clk, s->clkSrv & 1, s->matchOver);   /* + le trajet du paquet */
    matchOver = s->matchOver;
    matchWinner = s->winner;
    fragLimit = s->fragLimit > 0 ? s->fragLimit : FRAG_LIMIT;
    teamSize = s->teamSize <= TEAM_MAX ? s->teamSize : 0;
    {                                                     /* regles de la partie et objectifs, decides par l'hote */
        int shopWas = rules.shop;
        rules.mode = s->rMode < MODE_COUNT ? s->rMode : MODE_FRAGS; rules.minutes = s->rMinutes;
        rules.ammo = s->rAmmo < AMMO_COUNT ? s->rAmmo : AMMO_NORMAL;
        rules.hook = s->rFlags & 1; rules.shop = (s->rFlags >> 1) & 1;
        if (rules.shop && !shopWas) eco_reset();          /* arrive en cours de partie : pieces de depart */
        obj = s->obj;
    }
    for (int i = 0; i < npu; i++) {                       /* l'hote dit quelles recharges sont la */
        if (s->pickMask & (1 << i)) PU[i].t = 0;
        else if (PU[i].t <= 0) PU[i].t = PICKUP_RESPAWN;
    }
    for (int i = 0; i < MAXP; i++) {
        const SnapP *sp = &s->p[i];
        int wasActive = NP[i].active, wasAlive = NP[i].alive;
        NP[i].active = sp->active; NP[i].kills = sp->kills; NP[i].deaths = sp->deaths; NP[i].hp = sp->hp;
        NP[i].team = (unsigned char)(sp->team & 1);
        if (i == myId) { P.hp = sp->hp; NP[i].alive = sp->alive; continue; }
        if (!sp->active) continue;
        NP[i].st = sp->st;
        NP[i].recvT = nowT;
        if (!wasActive || (!wasAlive && sp->alive)) NP[i].disp = fv(sp->st.pos);
        NP[i].alive = sp->alive;
    }
}

static void found_add(const PkAnnounce *a, const struct sockaddr_in *from) {
    int k = -1;
    for (int i = 0; i < nFound; i++) if (same_addr(&found[i].addr, from)) k = i;
    if (k < 0) {
        if (nFound >= MAXFOUND) return;
        k = nFound++;
    }
    memcpy(found[k].name, a->name, 16);
    found[k].name[15] = 0;
    found[k].addr = *from;
    found[k].players = a->players; found[k].maxp = a->maxp; found[k].teams = a->teams;
    found[k].map = a->map < NMAPS ? a->map : -1;
    found[k].ranked = a->ranked & 1;                      /* mode classe */
    found[k].seen = nowT;
}

static void client_recv(PkHead *h, int n, struct sockaddr_in *from) {
    int fromHost = connected && same_addr(from, &hostAddr);
    switch (h->type) {
    case PK_ANNOUNCE:
        if (!connected && n >= (int)sizeof(PkAnnounce)) found_add((const PkAnnounce *)h, from);
        break;
    case PK_WELCOME:
        if (joining && n >= (int)sizeof(PkWelcome)) {
            const PkWelcome *w = (const PkWelcome *)h;
            if (w->refused == 2) { joining = 0; set_status("partie classee : identite de classement requise"); }
            else if (w->refused) { joining = 0; set_status(joinSpec ? "plus de place pour les spectateurs" : "partie pleine"); }
            else {
                if (w->pad[0] < NMAPS && w->pad[0] != curMap) load_map(w->pad[0]);   /* la carte de l'hote, avant de demarrer */
                ranked = w->pad[1] & 1;                   /* mode classe */
                on_welcome(w->id);
                if (ranked) netlog("classe : partie classee");
            }
        }
        break;
    case PK_SNAP:
        if (fromHost && n >= (int)sizeof(PkSnap)) apply_snap((const PkSnap *)h);
        break;
    case PK_ROSTER:
        if (fromHost && n >= (int)sizeof(PkRoster)) {
            const PkRoster *r = (const PkRoster *)h;
            for (int i = 0; i < MAXP; i++) { memcpy(NP[i].name, r->names[i], 16); NP[i].name[15] = 0; }
            for (int i = 0; i < MAXP; i++) {              /* mode classe */
                rkMmr[i] = r->mmr[i]; rkPlay[i] = r->rplay[i];
                memcpy(rkRid[i], r->rid[i], 16); memcpy(rkLeft[i], r->left[i], 16);
            }
            ranked = r->ranked & 1; rkLeftN = r->nleft > MAXP ? MAXP : r->nleft; memcpy(rkMid, r->mid, 8);
        }
        break;
    case PK_EVENT:
        if (fromHost && n >= (int)sizeof(PkEvent)) {
            NetEvent ev = ((PkEvent *)h)->ev;
            if (!ev_seen(ev.eid)) apply_event(&ev);
        }
        break;
    case PK_LEAVE:
        if (fromHost) net_disconnect_to_menu("l'hote a ferme la partie");
        break;
    }
}

static void net_poll(void) {
    char buf[2048];
    for (;;) {
        if (sock == INVALID_SOCKET) return;
        struct sockaddr_in from;
        sock_len_t fl = sizeof from;
        int n = recvfrom(sock, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
        if (n == SOCKET_ERROR) {
            int err = WSAGetLastError();
            if (err == WSAECONNRESET || err == WSAEMSGSIZE) continue;  /* ICMP "port injoignable" : on ignore */
            return;
        }
        struct sockaddr_in mapped;
        if (n >= 20 && stun_parse((const unsigned char *)buf, n, &mapped)) { stun_got(&mapped); continue; }
        if (n < (int)sizeof(PkHead)) continue;
        PkHead *h = (PkHead *)buf;
        if (h->magic != NET_MAGIC || h->ver != NET_VER) continue;
        pktIn++;
        if (netMode == NET_HOST) host_recv(h, n, &from);
        else if (netMode == NET_CLIENT) client_recv(h, n, &from);
    }
}

static void flush_events(void) {
    for (int i = 0; i < OUTQ; i++) {
        if (outq[i].left <= 0) continue;
        PkEvent pe;
        head(&pe.h, PK_EVENT);
        pe.ev = outq[i].ev;
        if (netMode == NET_HOST) host_send_all(&pe, sizeof pe);
        else net_send(&pe, sizeof pe, &hostAddr);
        outq[i].left--;
    }
}

static void net_update(float dt) {
    if (sock == INVALID_SOCKET) return;
    net_poll();
    if (sock == INVALID_SOCKET) return;
    lb_update(dt);
    sendAcc += dt; rosterAcc += dt; queryAcc += dt; joinAcc += dt;
    if (netMode == NET_HOST) {
        for (int i = 1; i < MAXP; i++)
            if (NP[i].active && net_silent(NP[i].lastHeard)) host_drop(i);
        for (int i = 0; i < MAXSPEC; i++)
            if (SP[i].active && net_silent(SP[i].lastHeard)) { SP[i].active = 0; netlog("spectateur %d perdu", i); }
        if (sendAcc >= 1.0f / 60) {
            sendAcc = fminf(sendAcc - 1.0f / 60, 0.05f);
            PkSnap s;
            memset(&s, 0, sizeof s);
            head(&s.h, PK_SNAP);
            s.matchT = matchT; s.matchOver = (unsigned char)matchOver; s.winner = (unsigned char)matchWinner;
            s.clk = clk_now(); s.clkSrv = (unsigned char)clkSrvOk;
            s.fragLimit = (unsigned char)fragLimit;
            for (int i = 0; i < npu; i++) if (PU[i].t <= 0) s.pickMask |= (unsigned short)(1 << i);
            for (int i = 0; i < MAXP; i++) {
                s.p[i].active = (unsigned char)NP[i].active; s.p[i].alive = (unsigned char)NP[i].alive;
                s.p[i].hp = (short)ceilf(NP[i].hp); s.p[i].kills = (short)NP[i].kills; s.p[i].deaths = (short)NP[i].deaths;
                s.p[i].st = NP[i].st; s.p[i].team = NP[i].team;
            }
            s.teamSize = (unsigned char)teamSize;
            s.rMode = (unsigned char)rules.mode; s.rMinutes = (unsigned char)rules.minutes; s.rAmmo = (unsigned char)rules.ammo;
            s.rFlags = (unsigned char)((rules.hook ? 1 : 0) | (rules.shop ? 2 : 0));
            s.obj = obj;
            host_send_all(&s, sizeof s);
            flush_events();
        }
        if (rosterAcc >= 0.5f) { rosterAcc = 0; host_send_roster(); }
    } else if (netMode == NET_CLIENT) {
        if (!connected) {
            if (queryAcc >= 1.0f) {                       /* qui heberge sur le reseau local ? */
                queryAcc = 0;
                if (!lanOff) {
                    PkHead q;
                    head(&q, PK_QUERY);
                    struct sockaddr_in b;
                    memset(&b, 0, sizeof b);
                    b.sin_family = AF_INET; b.sin_port = htons(NET_PORT);
                    b.sin_addr.s_addr = htonl(INADDR_BROADCAST);
                    net_send(&q, sizeof q, &b);
                    b.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                    net_send(&q, sizeof q, &b);
                }
                for (int i = 0; i < nFound; i++)          /* les salons du lobby partent avec "gone", pas avec le temps */
                    if (!found[i].online && nowT - found[i].seen > 3.5) { found[i] = found[--nFound]; i--; }
            }
            if (joining && joinAcc >= (joinOnline ? 0.2f : 0.4f)) {   /* en ligne : les paquets repetes percent le NAT */
                joinAcc = 0;
                client_send_join();                       /* nom, spectateur et identite / cote du mode classe */
                if (nowT - joinStartT > (joinOnline ? 10 : 5)) {
                    joining = 0;
                    set_status(joinOnline ? "connexion impossible (NAT strict ?) - essaie la saisie d'IP" : "pas de reponse de l'hote");
                }
            }
        } else if (spectating) {
            if (sendAcc >= 0.5f) {                        /* le spectateur ne joue pas : il dit seulement qu'il est la */
                sendAcc = 0;
                PkHead k;
                head(&k, PK_KEEP);
                net_send(&k, sizeof k, &hostAddr);
            }
            if (net_silent(lastSnapT)) { net_disconnect_to_menu("connexion perdue avec l'hote"); return; }
        } else {
            if (sendAcc >= 1.0f / 60) {
                sendAcc = fminf(sendAcc - 1.0f / 60, 0.05f);
                PkState s;
                head(&s.h, PK_STATE);
                pack_state(&s.st);
                net_send(&s, sizeof s, &hostAddr);
                flush_events();
            }
            if (net_silent(lastSnapT)) { net_disconnect_to_menu("connexion perdue avec l'hote"); return; }
        }
    }
    rk_tick(dt);                                          /* mode classe */
    for (int i = 0; i < MAXP; i++) {                     /* lissage des autres joueurs */
        if (i == myId || !NP[i].active) continue;
        float ahead = (float)fmin(nowT - NP[i].recvT, 0.1);
        V3 target = vadd(fv(NP[i].st.pos), vmul(fv(NP[i].st.vel), ahead));
        if (vlen(vsub(target, NP[i].disp)) > 4) NP[i].disp = target;
        else NP[i].disp = vlerp(NP[i].disp, target, fminf(1, dt * 25));
    }
}

/* regles de la partie hebergee : celles de HEBERGER (wantTeams, wantRules), ou les regles par defaut pour une partie
 * rapide, le duel du menu et une partie classee (chacun pour soi, FRAGS, grappin selon la carte, munitions normales,
 * sans boutique, duree par defaut). Fixe teamSize, rules et fragLimit ; renvoie la carte a charger
 * (classee : une carte qui a un deathmatch FRAGS, pas une carte de course) */
static int host_setup_rules(void) {
    int dflt = hostQuick || hostFfa || ranked, m = ranked ? rk_map_pick(menuMap) : map_pvp(menuMap) ? menuMap : 0;   /* jamais une carte CHALLENGE */
    teamSize = dflt ? 0 : wantTeams;                      /* partie rapide, duel du menu, classee : chacun pour soi */
    rules = dflt ? (MatchRules){ MODE_FRAGS, 1, AMMO_NORMAL, 0, 10 } : wantRules;   /* regles de HEBERGER */
    if (!mode_ok(rules.mode, m)) rules.mode = MODE_FRAGS;        /* objectifs absents de cette carte */
    if (mode_team(rules.mode) && teamSize <= 0) teamSize = 2;    /* modes d'equipe : 2 contre 2 au moins */
    hostFfa = 0;
    fragLimit = hostQuick ? FRAG_LIMIT_QUICK : team_frag_limit(teamSize); hostQuick = 0;
    if (rules.mode != MODE_FRAGS) fragLimit = 255;        /* chrono et objectifs : pas de limite de frags */
    if (ranked) fragLimit = rkFragsTest > 0 ? rkFragsTest : FRAG_LIMIT;   /* classe : limite par defaut (--frags : tests) */
    return m;
}

static int net_host_start(void) {
    net_shutdown();
    if (!net_open(NET_PORT)) { set_status("impossible d'heberger : le port UDP 27960 est deja utilise"); return 0; }
    netMode = NET_HOST; myId = 0; connected = 1;
    memset(NP, 0, sizeof NP);
    NP[0].active = 1;
    memcpy(NP[0].name, myName, 16);
    find_local_ip();
    ranked = hostRanked && rkOnline; hostRanked = 0;      /* mode classe : regles figees (host_setup_rules) */
    int m = host_setup_rules();
    if (curMap != m) load_map(m);                         /* carte choisie dans le menu (ou --map) */
    dm_local_start();
    matchOver = 0; match_clock_start();
    if (ranked) rk_new_match();
    rkMmr[0] = myMmr; rkPlay[0] = rankPlayed;
    if (hex_ok(rkId, 32)) bin_of(rkId, rkRid[0], 16);
    mode_reset(); eco_reset();
    NP[0].team = 0;
    host_respawn(0);
    netlog("partie hebergee, ip %s, %d frags, equipes %d, mode %s, grappin %d, munitions %s, boutique %d, %d min%s",
           hostIpText, fragLimit, teamSize, modeNames[rules.mode], rules.hook, ammoRuleNames[rules.ammo], rules.shop, rules.minutes,
           ranked ? ", CLASSEE" : "");
    return 1;
}

static int net_browse_start(void) {
    net_shutdown();
    if (!net_open(0)) { set_status("reseau indisponible"); return 0; }
    netMode = NET_CLIENT;
    queryAcc = 1.0f;
    netlog("recherche de parties");
    return 1;
}

static void net_connect_ex(struct sockaddr_in addr, int spec, int online) {
    if (netMode != NET_CLIENT && !net_browse_start()) return;
    hostAddr = addr;
    joining = 1; joinStartT = nowT; joinAcc = 1.0f;
    joinSpec = spec; joinOnline = online;
    char buf[96];
    snprintf(buf, sizeof buf, "connexion a %s ...", inet_ntoa(addr.sin_addr));
    set_status(buf);
}

static void net_connect(struct sockaddr_in addr) { net_connect_ex(addr, 0, 0); }
