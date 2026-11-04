/* VELOCITY - mode classe : suivi du match et rapport de fin
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* L'hote tire un identifiant par match (rkMid) et tient la liste des partis (rkLeft) ; il les diffuse dans
 * PK_ROSTER. A la fin, chacun (hote et clients) envoie au serveur le meme rapport : presents et frags, gagnant,
 * partis. Le serveur ne l'applique que si une majorite des presents concorde (worker/src/ratings.js).
 * Aucun rapport hors d'une partie classee en chacun pour soi, mode FRAGS (rk_counts). */
static void rk_push(int kind, const char *path, const char *body);   /* src/reseau/classe_serveur.c */
static void client_send_join(void) {                      /* PK_JOIN : aussi renvoye apres un match classe (nouvelle cote) */
    PkJoin j;
    memset(&j, 0, sizeof j);
    head(&j.h, PK_JOIN);
    memcpy(j.name, myName, 16);
    j.spec = (unsigned char)joinSpec;
    j.rplay = (unsigned char)(rankPlayed > 255 ? 255 : rankPlayed);
    j.mmr = (unsigned short)rank_clamp(myMmr);
    if (hex_ok(rkId, 32)) bin_of(rkId, j.rid, 16);
    net_send(&j, sizeof j, &hostAddr);
}

static int rk_slot_ok(int i) { return i >= 0 && i < MAXP && NP[i].active && rid_set(rkRid[i]); }
/* cote annoncee par une identite (PkJoin -> PkRoster) : la place d'un joueur parti la garde tant qu'elle n'est pas reprise */
static int rk_mmr_of(const unsigned char *rid) {
    for (int i = 0; i <= MAXP; i++)
        if (rid_set(rkRid[i]) && !memcmp(rkRid[i], rid, 16)) return rkMmr[i] ? rkMmr[i] : RANK_START;
    return myMmr;                                         /* inconnue : supposee egale a la mienne */
}

/* hostGone : l'hote est parti en plein match (il rejoint les partis, pas de gagnant) */
static void rk_send_report(int hostGone) {
    char body[sizeof rkRepBody], h[40];
    int opp[2 * MAXP + 1], np = 0, nl = 0, n;
    float sc[2 * MAXP + 1];
    if (!rk_counts() || !hex_ok(rkId, 32) || myId >= MAXP) return;
    bin_of(rkId, rkRid[myId], 16);
    if (!rid_set(rkRid[0])) return;
    hex_of(rkMid, 8, h);
    n = snprintf(body, sizeof body, "{\"id\":\"%s\",\"key\":\"%s\",\"match\":\"%s\",", rkId, rkKey, h);
    hex_of(rkRid[0], 16, h);
    n += snprintf(body + n, sizeof body - n, "\"host\":\"%s\",", h);
    int win = !hostGone && rk_slot_ok(matchWinner) ? matchWinner : -1;
    if (win >= 0) hex_of(rkRid[win], 16, h); else h[0] = 0;
    n += snprintf(body + n, sizeof body - n, "\"winner\":\"%s\",\"players\":[", h);
    for (int i = 0; i < MAXP; i++) {
        if (!rk_slot_ok(i) || (hostGone && i == 0)) continue;
        hex_of(rkRid[i], 16, h);
        n += snprintf(body + n, sizeof body - n, "%s{\"id\":\"%s\",\"kills\":%d}", np ? "," : "", h, NP[i].kills);
        np++;
    }
    n += snprintf(body + n, sizeof body - n, "],\"left\":[");
    unsigned char lst[MAXP + 1][16];
    int ln = 0;
    for (int k = 0; k < rkLeftN; k++) {                   /* un parti revenu dans la partie compte comme present */
        int back = 0;
        for (int i = 0; i < MAXP; i++) if (rk_slot_ok(i) && !(hostGone && i == 0) && !memcmp(rkRid[i], rkLeft[k], 16)) back = 1;
        if (!back) memcpy(lst[ln++], rkLeft[k], 16);
    }
    int dup = 0;
    for (int k = 0; k < ln; k++) if (!memcmp(lst[k], rkRid[0], 16)) dup = 1;
    if (hostGone && !dup) memcpy(lst[ln++], rkRid[0], 16);
    for (int k = 0; k < ln; k++) {
        hex_of(lst[k], 16, h);
        n += snprintf(body + n, sizeof body - n, "%s\"%s\"", nl++ ? "," : "", h);
    }
    snprintf(body + n, sizeof body - n, "]}");
    n = 0;                                                /* estimation locale (meme formule que le serveur) */
    for (int i = 0; i < MAXP; i++) {
        if (i == myId || !rk_slot_ok(i) || (hostGone && i == 0)) continue;
        opp[n] = rkMmr[i] ? rkMmr[i] : RANK_START;
        sc[n++] = rank_score(NP[myId].kills, NP[i].kills, win == myId, win == i);
    }
    for (int k = 0; k < ln; k++) { opp[n] = rk_mmr_of(lst[k]); sc[n++] = 1; }   /* partis : battus, a leur cote annoncee */
    rkEstimate = rank_delta(myMmr, rankPlayed, n, opp, sc);
    snprintf(rkRepBody, sizeof rkRepBody, "%s", body);
    rkRepState = RS_PENDING; rkRepTries = 0; rkRepRetryT = 0;
    rk_push(RQ_REPORT, "/ranked/report", body);
    netlog("classe : rapport de fin de match%s (%d present(s), %d parti(s), estimation %+d)",
           hostGone ? " (l'hote est parti)" : "", np, ln, rkEstimate);
}

static void rk_tick(float dt) {
    if (!rk_counts() || !dmMode || !connected || spectating || myId >= MAXP) { rkPrevOver = matchOver; return; }
    if (!matchOver) {
        int opp = 0;
        if (rkPrevOver) {                                 /* fin de la pause : nouveau match */
            if (netMode == NET_HOST) rk_new_match();
            rkHadOpp = 0;
        }
        rkEndWait = -1;
        for (int i = 0; i < MAXP; i++) {
            if (i == myId) continue;
            if (NP[i].active) { opp++; rkSeen[i] = 1; continue; }
            if (!rkSeen[i]) continue;
            rkSeen[i] = 0;
            if (netMode == NET_HOST && rkHadOpp && rid_set(rkRid[i]) && rkLeftN < MAXP) {   /* l'hote liste les partis */
                memcpy(rkLeft[rkLeftN++], rkRid[i], 16);
                netlog("classe : %s quitte le match en cours (classe dernier)", pname(i));
                host_send_roster();
            }
        }
        if (opp) rkHadOpp = 1;
        else if (rkHadOpp && netMode == NET_HOST) {       /* tous les adversaires sont partis : l'hote gagne */
            netlog("classe : plus aucun adversaire, fin du match");
            host_end_match(myId);
        }
    } else if (rkHadOpp) {                                /* fin du match : rapport une fois les derniers frags recus */
        if (rkEndWait < 0) rkEndWait = RANK_END_WAIT;
        else if ((rkEndWait -= dt) <= 0) { rk_send_report(0); rkHadOpp = 0; rkEndWait = -1; }
    }
    rkPrevOver = matchOver;
}

static void rk_host_gone(void) {                          /* client : l'hote a quitte (ou ne repond plus) */
    if (!ranked || spectating || netMode != NET_CLIENT || !connected || !rkHadOpp) return;
    rk_send_report(!matchOver);
    rkHadOpp = 0;
}

static void rk_shutdown(void) {                           /* on quitte la partie (net_shutdown) */
    if (ranked && !spectating && rkHadOpp && matchOver && myId < MAXP) rk_send_report(0);   /* match fini, rapport pas encore parti */
    if (ranked && !spectating) {
        if (rkHadOpp && !matchOver) netlog("classe : partie quittee en plein match : defaite si les autres confirment");
        rkHelloAt = nowT + 4;                             /* la cote a pu changer : on la relira */
    }
    ranked = rankedWant = 0; rkHadOpp = 0; rkPrevOver = 0; rkEndWait = -1; rkLeftN = 0;
    memset(rkSeen, 0, sizeof rkSeen); memset(rkRid, 0, sizeof rkRid); memset(rkLeft, 0, sizeof rkLeft);
    memset(rkMmr, 0, sizeof rkMmr); memset(rkPlay, 0, sizeof rkPlay);
}
