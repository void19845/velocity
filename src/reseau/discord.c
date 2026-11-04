/* VELOCITY - connexion Discord : un compte Discord = une seule identite de classement
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c).
   Apres src/reseau/classe_serveur.c (file de requetes rk_push, lecture du JSON) et src/interface/mise_a_jour.c (open_url). */

/* ------------------------------------------------------------------ */
/* Bouton DISCORD du menu (worker/src/discord.js) :                    */
/*   1. POST /auth/discord/start {id, key, name, state}                */
/*   2. le navigateur du systeme ouvre /auth/discord/go?s=STATE         */
/*      (page d'autorisation de Discord, puis retour au serveur)       */
/*   3. POST /auth/discord/poll {id, key, state} toutes les DC_POLL s  */
/*      -> done : identite du compte Discord (id, cle, pseudo)         */
/* Le meme compte Discord sur un autre PC donne la meme identite (avec */
/* une cle propre a ce PC) : cote, matchs et temps de speedrun suivent.*/
/* Le pseudo reste modifiable : /ranked/hello le met a jour au serveur */
/* (classe_serveur.c, rkNameSent).                                     */
/* ------------------------------------------------------------------ */
#define DC_POLL 2.0                                       /* s entre deux questions au serveur */
#define DC_WAIT 300.0                                     /* s : abandon si la page Discord n'est pas validee */
enum { DC_IDLE, DC_START, DC_WAITING };
static int dcState;
static char dcStateHex[33];                               /* state de la demande en cours (32 hex tires ici) */
static double dcPollAt, dcGiveUp;

static void dc_open_page(void) {                          /* meme hote que le lobby : https, ou http si un port est donne */
    char url[200];
    const char *colon = strchr(lobbyAddr, ':');
    int tls = !colon || atoi(colon + 1) == 443;
    snprintf(url, sizeof url, "%s://%s/auth/discord/go?s=%s", tls ? "https" : "http", lobbyAddr, dcStateHex);
    if (!botMode && !shotMode) open_url(url);
}

/* bouton DISCORD : lance la connexion (ou rouvre la page si elle est deja en cours) */
static void dc_begin(void) {
    char body[256], nm[16];
    unsigned char b[16];
    if (dcState == DC_WAITING) { dc_open_page(); return; }
    if (dcState != DC_IDLE) return;
    if (!onlineOn) { set_status("DISCORD : active PARTIES EN LIGNE dans PARAMETRES"); return; }
    if (rk_identity()) cfg_save();
    rk_random(b, 16); hex_of(b, 16, dcStateHex);
    lb_safe_name(nm, sizeof nm);
    snprintf(body, sizeof body, "{\"id\":\"%s\",\"key\":\"%s\",\"name\":\"%s\",\"state\":\"%s\"}", rkId, rkKey, nm, dcStateHex);
    dcState = DC_START;
    rk_push(RQ_DC_START, "/auth/discord/start", body);
    set_status("DISCORD : connexion au serveur ...");
}

/* reponse "done" du serveur : on passe sur l'identite du compte Discord. 1 si elle est valide */
static int dc_take(const char *s) {
    char id[40], key[40], nm[40], dn[40];
    json_str(s, "id", id, sizeof id);
    json_str(s, "key", key, sizeof key);
    json_str(s, "name", nm, sizeof nm);
    json_str(s, "discord", dn, sizeof dn);
    if (!hex_ok(id, 32) || !hex_ok(key, 32)) return 0;
    if (strcmp(id, rkId)) {                               /* autre identite : son cache de cote sera relu au serveur */
        myMmr = RANK_START; rankPlayed = rankWins = 0; rkRank = rkMeRank = 0;
    }
    snprintf(rkId, sizeof rkId, "%s", id);
    snprintf(rkKey, sizeof rkKey, "%s", key);
    dc_name_set(dn[0] ? dn : "discord");
    int okName = nm[0] && strlen(nm) < sizeof myName;     /* le pseudo enregistre pour ce compte */
    for (const char *c = nm; *c; c++) if (!(isalnum((unsigned char)*c) || *c == '_' || *c == '.' || *c == '-')) okName = 0;
    if (okName) snprintf(myName, sizeof myName, "%s", nm);
    return 1;
}

static void dc_on_response(int kind, int ok) {
    char st[16], err[48], b[96];
    json_str(rkResp, "err", err, sizeof err);
    if (kind == RQ_DC_START) {
        if (ok && json_ok(rkResp)) {
            dcState = DC_WAITING;
            dcPollAt = nowT + DC_POLL; dcGiveUp = nowT + DC_WAIT;
            dc_open_page();
            set_status("DISCORD : valide la connexion dans ton navigateur");
            return;
        }
        dcState = DC_IDLE;
        snprintf(b, sizeof b, "DISCORD indisponible : %s", err[0] ? err : "serveur injoignable");
        set_status(b);
        return;
    }
    json_str(rkResp, "state", st, sizeof st);
    if (ok && json_ok(rkResp) && !strcmp(st, "done")) {
        dcState = DC_IDLE;
        if (!dc_take(rkResp)) { set_status("DISCORD : reponse invalide du serveur"); return; }
        cfg_save();
        rkHelloAt = nowT;                                 /* cote et TOP 3 de cette identite */
        snprintf(b, sizeof b, "DISCORD : connecte (%s)", rkDiscord);
        set_status(b);
        netlog("discord : connecte au compte %s, identite %.8s...", rkDiscord, rkId);
        return;
    }
    if (ok && (!strcmp(st, "expired") || (!json_ok(rkResp) && err[0]))) {   /* demande perdue ou refusee */
        dcState = DC_IDLE;
        snprintf(b, sizeof b, "DISCORD : connexion %s", err[0] ? err : "expiree, recommence");
        set_status(b);
        return;
    }
    dcPollAt = nowT + DC_POLL;                            /* pending, ou pas de reponse : on redemande */
}

/* appele a chaque image (rk_net_poll) */
static void dc_tick(void) {
    char body[160];
    if (dcState != DC_WAITING || dcPollAt <= 0 || nowT < dcPollAt) return;
    if (nowT > dcGiveUp) { dcState = DC_IDLE; set_status("DISCORD : connexion abandonnee (delai depasse)"); return; }
    dcPollAt = 0;                                         /* relance a la reponse */
    snprintf(body, sizeof body, "{\"id\":\"%s\",\"key\":\"%s\",\"state\":\"%s\"}", rkId, rkKey, dcStateHex);
    rk_push(RQ_DC_POLL, "/auth/discord/poll", body);
}
