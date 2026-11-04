/* VELOCITY - speedrun : requetes au serveur des classements de temps
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Apres src/reseau/classe_serveur.c (file de requetes rk_push_arg, lecture du JSON) et src/jeu/speedrun.c (categories). */

/* ------------------------------------------------------------------ */
/* Meme hote (lobbyAddr), meme identite et meme file que le mode       */
/* classe :                                                            */
/*   POST /speedrun/start  {id, key, name, cat}            -> ticket   */
/*   POST /speedrun/report {id, key, cat, run, ms, splits} -> record   */
/*   GET  /speedrun/top?cat=...&id=...  -> top 10, record, mon temps   */
/* Le ticket est demande au depart du run : le serveur refuse un temps */
/* plus court que le temps reel ecoule depuis (worker/src/speedrun.js) */
/* Un temps fini avant l'arrivee du ticket part des qu'il arrive.      */
/* ------------------------------------------------------------------ */
enum { SRT_NONE, SRT_WAIT, SRT_OK, SRT_FAILED };          /* etat du ticket */
static int srTicket[SR_NCAT];

static int sr_net_ok(void) { return onlineOn && hex_ok(rkId, 32); }

static void speedrun_begin(int cat) {
    if (botMode || shotMode || !sr_cat_ok(cat)) return;
    SrRun *r = &srRun[cat];
    r->run[0] = 0; r->seq = (r->seq + 1) & 0x7fff;
    r->state = SRR_NONE; r->err[0] = 0; r->rank = 0;
    srTicket[cat] = SRT_NONE;
    if (!sr_net_ok()) return;
    char body[256], key[40], nm[16];
    sr_key(cat, key, sizeof key);
    lb_safe_name(nm, sizeof nm);
    snprintf(body, sizeof body, "{\"id\":\"%s\",\"key\":\"%s\",\"name\":\"%s\",\"cat\":\"%s\"}", rkId, rkKey, nm, key);
    srTicket[cat] = SRT_WAIT;
    rk_push_arg(RQ_SR_START, "/speedrun/start", body, cat | r->seq << 8);
}

static void sr_send(int cat) {                            /* le rapport, avec le ticket (consomme) */
    SrRun *r = &srRun[cat];
    char body[sizeof rkRepBody], key[40];
    sr_key(cat, key, sizeof key);
    int k = snprintf(body, sizeof body, "{\"id\":\"%s\",\"key\":\"%s\",\"cat\":\"%s\",\"run\":\"%s\",\"ms\":%d,\"splits\":[",
                     rkId, rkKey, key, r->run, r->ms);
    for (int i = 0; i < r->n && k < (int)sizeof body - 16; i++) k += snprintf(body + k, sizeof body - k, "%s%d", i ? "," : "", r->split[i]);
    snprintf(body + k, sizeof body - k, "]}");
    rk_push_arg(RQ_SR_REPORT, "/speedrun/report", body, cat);
    r->state = SRR_SENT; r->run[0] = 0;
    srTicket[cat] = SRT_NONE;
}

static void speedrun_report_splits(int cat, int ms, const int *splitMs, int n) {
    if (botMode || shotMode || !sr_cat_ok(cat) || n < 0 || n > RACE_GATES) return;
    SrRun *r = &srRun[cat];
    r->ms = ms; r->n = n; r->rank = 0; r->err[0] = 0;
    if (n) memcpy(r->split, splitMs, sizeof r->split[0] * (size_t)n);
    if (srTicket[cat] == SRT_OK) sr_send(cat);
    else if (srTicket[cat] == SRT_WAIT) r->state = SRR_WAIT;   /* parti a l'arrivee du ticket */
    else { r->state = SRR_OFFLINE; snprintf(r->err, sizeof r->err, sr_net_ok() ? "serveur injoignable au depart" : "hors ligne"); }
}
static void speedrun_report(int cat, int ms) { speedrun_report_splits(cat, ms, NULL, 0); }

static void rq_sr_top(int cat) {
    char path[128], key[40];
    if (!sr_cat_ok(cat)) return;
    SrTop *t = &srTop[cat];
    if (!onlineOn) { t->state = 3; return; }
    if (t->state != 2) t->state = 1;                      /* deja pret : on garde l'affichage pendant la mise a jour */
    sr_key(cat, key, sizeof key);
    snprintf(path, sizeof path, "/speedrun/top?cat=%s&id=%s", key, hex_ok(rkId, 32) ? rkId : "");
    rk_push_arg(RQ_SR_TOP, path, NULL, cat);
}
static void rq_sr_top_all(void) {                         /* menu CHALLENGE : toutes les categories */
    sr_maps_init();
    for (int k = 0; k < srMapN; k++) rq_sr_top(SR_MAP0 + srMaps[k]);
    rq_sr_top(SR_COMPLET);
    rq_sr_top(SR_VAGUES100);
}

static int json_ints(const char *s, const char *key, int *out, int cap) {   /* "key":[1,2,3] -> n */
    const char *p = json_at(s, key);
    int n = 0;
    if (!p || *p != '[') return 0;
    for (p++; *p && *p != ']' && n < cap;) {
        char *e;
        long v = strtol(p, &e, 10);
        if (e == p) break;
        out[n++] = (int)v;
        p = e;
        while (*p == ',' || *p == ' ') p++;
    }
    return n;
}
static void sr_parse_top(int cat, const char *s) {
    SrTop *t = &srTop[cat];
    char obj[512];
    const char *p = json_at(s, "top");
    t->n = 0;
    if (p && *p == '[') {
        for (p++; t->n < SR_TOPN;) {
            const char *o = strchr(p, '{'), *end = strchr(p, ']');
            if (!o || (end && end < o) || !json_obj(o, obj, sizeof obj)) break;
            json_str(obj, "name", t->name[t->n], sizeof t->name[0]);
            t->ms[t->n++] = json_int(obj, "ms", 0);
            p = o + strlen(obj);
        }
    }
    t->runners = json_int(s, "runners", t->n);
    t->wrMs = t->wrN = 0; t->wrName[0] = 0;
    if (json_obj(json_at(s, "wr"), obj, sizeof obj)) {
        json_str(obj, "name", t->wrName, sizeof t->wrName);
        t->wrMs = json_int(obj, "ms", 0);
        t->wrN = json_ints(obj, "splits", t->wrSplit, RACE_GATES);
    }
    t->meRank = t->meMs = t->meN = 0;
    if (json_obj(json_at(s, "me"), obj, sizeof obj)) {
        t->meRank = json_int(obj, "rank", 0);
        t->meMs = json_int(obj, "ms", 0);
        t->meN = json_ints(obj, "splits", t->meSplit, RACE_GATES);
    }
    t->state = 2;
    if (cat == SR_COMPLET && srPbN != srMapN && t->meN == srMapN && srMapN > 0) {   /* autre machine : le record perso du serveur */
        memcpy(srPb, t->meSplit, sizeof srPb[0] * (size_t)srMapN);
        srPbN = srMapN;
        if (srCmpN != srMapN) { memcpy(srCmp, srPb, sizeof srPb); srCmpN = srMapN; }
    }
}

static void sr_on_response(int kind, int arg, int ok) {
    int cat = arg & 0xff;
    if (!sr_cat_ok(cat)) return;
    SrRun *r = &srRun[cat];
    char err[40];
    ok = ok && json_ok(rkResp);
    json_str(rkResp, "err", err, sizeof err);
    if (kind == RQ_SR_START) {
        if ((arg >> 8) != r->seq) return;                 /* ticket d'un depart deja remplace (R) */
        if (ok && json_str(rkResp, "run", r->run, sizeof r->run) && hex_ok(r->run, 16)) {
            srTicket[cat] = SRT_OK;
            if (r->state == SRR_WAIT) sr_send(cat);
            return;
        }
        r->run[0] = 0;
        srTicket[cat] = SRT_FAILED;
        netlog("speedrun : pas de ticket pour %s (%s)", sr_label(cat), err[0] ? err : "serveur injoignable");
        if (r->state == SRR_WAIT) { r->state = SRR_OFFLINE; snprintf(r->err, sizeof r->err, "%s", err[0] ? err : "serveur injoignable"); }
    } else if (kind == RQ_SR_REPORT) {
        char st[16];
        json_str(rkResp, "state", st, sizeof st);
        if (ok) {
            r->state = !strcmp(st, "record") ? SRR_RECORD : SRR_KEPT;
            r->rank = json_int(rkResp, "rank", 0); r->best = json_int(rkResp, "best", r->ms); r->prev = json_int(rkResp, "prev", 0);
            netlog("speedrun : %s en %d ms, %s, rang %d", sr_label(cat), r->ms, st, r->rank);
            rq_sr_top(cat);
        } else {
            r->state = err[0] ? SRR_REFUSED : SRR_OFFLINE;
            snprintf(r->err, sizeof r->err, "%s", err[0] ? err : "serveur injoignable");
            netlog("speedrun : temps refuse pour %s (%s)", sr_label(cat), r->err);
        }
    } else {
        if (ok) sr_parse_top(cat, rkResp);
        else if (srTop[cat].state != 2) srTop[cat].state = 3;
    }
}
