/* VELOCITY - mode classe : requetes au serveur de classement
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Apres src/interface/config.c (cfg_save, pendingAction, ACT_RANKED) et src/plateforme/http.c (http_request). */

/* ------------------------------------------------------------------ */
/* Mode classe : requetes HTTP au serveur de classement                */
/* (meme hote que le lobby : lobbyAddr, donc --lobby pour les tests).  */
/* Une requete a la fois, dans un fil :                                */
/*   POST /ranked/hello  {id, key, name}       -> cote, palier, rang   */
/*   POST /ranked/report {id, key, match, ...} -> pending / applied    */
/*   GET  /ranked/top?id=...                   -> top et mon rang      */
/* La meme file sert aux classements speedrun (speedrun_serveur.c).   */
/* ------------------------------------------------------------------ */
#define RK_QMAX 10
typedef struct { int kind, post, arg; char path[128]; char body[sizeof rkRepBody]; } RkReq;   /* arg : speedrun (categorie...) */
static RkReq rkQ[RK_QMAX], rkCur;
static int rkQN, rkRespOk;
static HANDLE rkTh;
static char rkResp[4096];

static int rk_http(const RkReq *q, char *out, int cap) { return http_request(lobbyAddr, q->path, q->post ? q->body : NULL, out, cap); }

static DWORD WINAPI rk_thread(LPVOID arg) {
    (void)arg;
    rkRespOk = rk_http(&rkCur, rkResp, sizeof rkResp);
    return 0;
}

static void rk_push_arg(int kind, const char *path, const char *body, int arg) {
    if (kind != RQ_REPORT && kind != RQ_SR_REPORT)        /* hello, top, ticket : une seule demande en attente suffit */
        for (int i = 0; i < rkQN; i++)
            if (rkQ[i].kind == kind && !strcmp(rkQ[i].path, path) && (rkQ[i].arg & 0xff) == (arg & 0xff)) {
                rkQ[i].arg = arg;                         /* la plus recente remplace celle qui attend (ticket : R) */
                snprintf(rkQ[i].body, sizeof rkQ[i].body, "%s", body ? body : "");
                return;
            }
    if (rkQN >= RK_QMAX) return;
    RkReq *q = &rkQ[rkQN++];
    q->kind = kind; q->post = body != NULL; q->arg = arg;
    snprintf(q->path, sizeof q->path, "%s", path);
    snprintf(q->body, sizeof q->body, "%s", body ? body : "");
}
static void rk_push(int kind, const char *path, const char *body) { rk_push_arg(kind, path, body, 0); }
static void sr_on_response(int kind, int arg, int ok);   /* speedrun : reponses (speedrun_serveur.c) */
static void dc_on_response(int kind, int ok);            /* connexion Discord : reponses (discord.c) */
static void dc_tick(void);

/* lecture minimale du JSON du serveur (cles uniques, chaines sans echappement) */
static const char *json_at(const char *s, const char *key) {
    char pat[40];
    snprintf(pat, sizeof pat, "\"%s\":", key);
    const char *p = strstr(s, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p == ' ') p++;
    return p;
}
static int json_int(const char *s, const char *key, int def) {
    const char *p = json_at(s, key);
    return p && (*p == '-' || isdigit((unsigned char)*p)) ? atoi(p) : def;
}
static int json_str(const char *s, const char *key, char *out, int n) {
    const char *p = json_at(s, key);
    int k = 0;
    if (!p || *p != '"') { out[0] = 0; return 0; }
    for (p++; *p && *p != '"' && k < n - 1; p++) out[k++] = *p;
    out[k] = 0;
    return 1;
}
static int json_obj(const char *p, char *out, int n) {   /* copie l'objet { ... } (sans objet imbrique) qui commence en p */
    const char *e = p && *p == '{' ? strchr(p, '}') : NULL;
    if (!e) { out[0] = 0; return 0; }
    int len = (int)(e - p + 1) < n - 1 ? (int)(e - p + 1) : n - 1;
    memcpy(out, p, (size_t)len);
    out[len] = 0;
    return 1;
}
static int json_ok(const char *s) { const char *p = json_at(s, "ok"); return p && !strncmp(p, "true", 4); }

static void rk_take_player(const char *o) {              /* mmr / played / wins d'une reponse : cache local */
    myMmr = rank_clamp(json_int(o, "mmr", myMmr));
    rankPlayed = json_int(o, "played", rankPlayed);
    rankWins = json_int(o, "wins", rankWins);
}

static void rk_parse_top(const char *s) {
    char obj[256];
    const char *p = json_at(s, "top");
    rkTopN = 0;
    if (p && *p == '[') {
        for (p++; rkTopN < 3;) {
            const char *o = strchr(p, '{'), *end = strchr(p, ']');
            if (!o || (end && end < o) || !json_obj(o, obj, sizeof obj)) break;
            RkTop *t = &rkTop[rkTopN++];
            json_str(obj, "name", t->name, sizeof t->name);
            json_str(obj, "tier", t->tier, sizeof t->tier);
            t->mmr = json_int(obj, "mmr", 0); t->played = json_int(obj, "played", 0);
            p = o + strlen(obj);
        }
    }
    rkMeRank = 0;
    if (json_obj(json_at(s, "me"), obj, sizeof obj)) { rkMeRank = json_int(obj, "rank", 0); rk_take_player(obj); }
}

static char rkNameSent[16];                              /* pseudo du dernier hello : un changement de pseudo le renvoie */
static void rq_hello(void) {
    char body[200], nm[16];
    if (!onlineOn || !hex_ok(rkId, 32)) return;
    lb_safe_name(nm, sizeof nm);
    snprintf(rkNameSent, sizeof rkNameSent, "%s", nm);
    snprintf(body, sizeof body, "{\"id\":\"%s\",\"key\":\"%s\",\"name\":\"%s\"}", rkId, rkKey, nm);
    rkHelloBusy = 1;
    rk_push(RQ_HELLO, "/ranked/hello", body);
}
static int rkMenuAuto;                                    /* menu : cote et TOP 3 relus tout seuls (jeu normal, pas bots ni tests) */
static double rkTopAskT;                                  /* derniere demande du TOP */
static void rq_top(void) {
    char path[96];
    rkTopAskT = nowT;
    if (!onlineOn) { rkTopState = 2; return; }
    snprintf(path, sizeof path, "/ranked/top?id=%s", hex_ok(rkId, 32) ? rkId : "");
    rk_push(RQ_TOP, path, NULL);
}

static void rk_on_hello(int ok) {
    char err[48];
    rkHelloBusy = 0;
    if (ok && json_ok(rkResp)) {
        rkOnline = 1;
        rk_take_player(rkResp);
        rkRank = json_int(rkResp, "rank", 0);
        if (botMmr0 < 0) botMmr0 = myMmr;
        cfg_save();
        netlog("classement : identite reconnue, cote %d, %d match(s) classe(s), %d victoire(s)", myMmr, rankPlayed, rankWins);
        if (rkAfterHello) { rkAfterHello = 0; pendingAction = ACT_RANKED; }
        return;
    }
    rkOnline = 0;
    json_str(rkResp, "err", err, sizeof err);
    netlog("classement : serveur %s", err[0] ? err : "injoignable");
    if (rkAfterHello) {
        char b[96];
        rkAfterHello = 0;
        if (err[0]) snprintf(b, sizeof b, "PARTIE CLASSEE indisponible : %s", err);
        else snprintf(b, sizeof b, "PARTIE CLASSEE indisponible : serveur de classement injoignable");
        set_status(b);
    }
}

static void rk_on_report(int ok) {
    char st[16], tier[24];
    json_str(rkResp, "state", st, sizeof st);
    if (ok && !strcmp(st, "applied")) {
        int before = myMmr;
        rk_take_player(rkResp);
        rkLastDelta = json_int(rkResp, "delta", myMmr - before);
        rkLastMmr = myMmr; rkLastPlayed = rankPlayed;
        json_str(rkResp, "tier", tier, sizeof tier);
        rkRepState = RS_DONE;
        cfg_save();
        netlog("classe (serveur) : cote %d -> %d (%+d), %s, %d match(s) classe(s), %d victoire(s)",
               myMmr - rkLastDelta, myMmr, rkLastDelta, tier, rankPlayed, rankWins);
        if (connected && myId < MAXP) { rkMmr[myId] = myMmr; rkPlay[myId] = rankPlayed; }
        if (netMode == NET_CLIENT && connected && !spectating)   /* l'hote apprend ma nouvelle cote (PK_ROSTER) */
            for (int r = 0; r < 3; r++) client_send_join();
        rq_top();
        return;
    }
    if (!ok || !strcmp(st, "pending")) {                  /* en attente des rapports des autres (ou pas de reponse) */
        if (++rkRepTries < RANK_TRIES) { rkRepRetryT = nowT + RANK_RETRY; return; }
        netlog("classe : resultat non confirme par les autres joueurs");
    } else {
        char err[48];
        json_str(rkResp, "err", err, sizeof err);
        netlog("classe : rapport refuse (%s)", st[0] ? st : err);
    }
    rkRepState = RS_FAILED;
}

/* appele a chaque image : resultat de la requete finie, renvois, requete suivante */
static void rk_net_poll(void) {
    if (rkTh && WaitForSingleObject(rkTh, 0) != WAIT_TIMEOUT) {
        CloseHandle(rkTh);
        rkTh = NULL;
        if (rkCur.kind == RQ_HELLO) rk_on_hello(rkRespOk);
        else if (rkCur.kind == RQ_REPORT) rk_on_report(rkRespOk);
        else if (rkCur.kind >= RQ_DC_START) dc_on_response(rkCur.kind, rkRespOk);
        else if (rkCur.kind >= RQ_SR_START) sr_on_response(rkCur.kind, rkCur.arg, rkRespOk);
        else if (rkRespOk && json_ok(rkResp)) { rk_parse_top(rkResp); rkTopState = 1; }
        else rkTopState = 2;
    }
    if (rkRepState == RS_PENDING && rkRepRetryT > 0 && nowT >= rkRepRetryT) {
        rkRepRetryT = 0;
        rk_push(RQ_REPORT, "/ranked/report", rkRepBody);
    }
    if (rkHelloAt > 0 && nowT >= rkHelloAt) { rkHelloAt = 0; rq_hello(); rq_top(); }
    if (rkMenuAuto && onlineOn && appState == APP_MENU && rkHelloAt <= 0 && !rkHelloBusy && !rkTh && !rkQN &&
        nowT - rkTopAskT > (!rkOnline || rkTopState == 2 ? RANK_MENU_RETRY : RANK_MENU_REFRESH)) {
        if (!rkOnline) rq_hello();                        /* echec au lancement : on retente, puis on rafraichit */
        rq_top();
    }
    if (rkNameSent[0] && uiFocus != 1 && !rkHelloBusy) {  /* pseudo change (saisie terminee) : le serveur le note pour cette identite */
        char nm[16];
        lb_safe_name(nm, sizeof nm);
        if (strcmp(nm, rkNameSent)) rq_hello();
    }
    dc_tick();
    if (!rkTh && rkQN > 0) {
        rkCur = rkQ[0];
        memmove(rkQ, rkQ + 1, sizeof rkQ[0] * (size_t)(rkQN - 1));
        rkQN--;
        rkTh = CreateThread(NULL, 0, rk_thread, NULL, 0, NULL);
        if (!rkTh) {                                      /* pas de fil : comme une absence de reponse */
            rkResp[0] = 0;
            if (rkCur.kind == RQ_HELLO) rk_on_hello(0);
            else if (rkCur.kind == RQ_REPORT) rk_on_report(0);
            else if (rkCur.kind >= RQ_DC_START) dc_on_response(rkCur.kind, 0);
            else if (rkCur.kind >= RQ_SR_START) sr_on_response(rkCur.kind, rkCur.arg, 0);
            else rkTopState = 2;
        }
    }
}

/* fin d'un bot --ranked : laisse partir les derniers rapports, relit la cote au serveur et l'ecrit au journal */
static void rk_bot_final(void) {
    for (int k = 0; k < 120 && (k < 30 || rkTh || rkQN || rkRepState == RS_PENDING); k++) {   /* 3 s (rapport de l'hote apres un abandon), 12 s au plus */
#ifdef _WIN32
        Sleep(100);
#else
        usleep(100000);
#endif
        nowT += 0.1;
        rk_net_poll();
    }
    RkReq q;
    memset(&q, 0, sizeof q);
    q.kind = RQ_HELLO; q.post = 1;
    snprintf(q.path, sizeof q.path, "/ranked/hello");
    snprintf(q.body, sizeof q.body, "{\"id\":\"%s\",\"key\":\"%s\"}", rkId, rkKey);
    if (rk_http(&q, rkResp, sizeof rkResp) && json_ok(rkResp)) {
        char tl[32];
        rk_take_player(rkResp);
        rank_label(myMmr, rankPlayed, tl, sizeof tl);
        netlog("classe : cote finale (serveur) %d, %s, %d match(s) classe(s), %d victoire(s) ; debut %d, variation %+d",
               myMmr, tl, rankPlayed, rankWins, botMmr0, botMmr0 >= 0 ? myMmr - botMmr0 : 0);
    } else netlog("classe : serveur de classement injoignable a la fin");
}
