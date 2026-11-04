/* VELOCITY - paquets, evenements EV_*, apply_event
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Multijoueur LAN : reseau                                            */
/*                                                                     */
/* L'hote est aussi un joueur (id 0) et fait autorite sur PV, kills,   */
/* respawns et match. Chaque joueur simule son propre mouvement et     */
/* envoie son etat 60 fois/s ; l'hote renvoie un instantane de tous.   */
/* Les touches sont validees chez le tireur, qui envoie un evenement   */
/* HIT. Les evenements sont renvoyes 3 fois et dedoublonnes par id.    */
/* ------------------------------------------------------------------ */
#define NET_MAGIC 0x564C4F43u
#define NET_VER   20      /* 15 : regles (mode, grappin, munitions, boutique, specs) et objectifs dans PkSnap, EV_OBJ, spec dans NetPState ;
                             16 = fusion du fork + mode classe (PkJoin, PkRoster, PkAnnounce.ranked, PkWelcome pad[1]) ;
                             17 = cartes CITADELLE et OASIS retirees, objectifs sur les cartes neon ;
                             18 = horloge de match commune (PkSnap.clk, clkSrv) ;
                             19 = tirs a la tete et chaine de tirs pleine (EV_HIT.pad[0] = HF_*, f[4..6] = point sur la tete ;
                                  EV_KILL.pad[0] ; NetPState.spare[1] = chaine de tirs) ; onde de l'Enclume du plan 1b
                                  (EV_SLAM : hauteur du mur, cap et vitesse de l'ovale) ;
                             20 = specialisations retirees (PkSnap.rFlags sans le bit 2, NetPState.spare[0] libre) */
static int net_silent(double last) { return nowT - last > NET_TIMEOUT; }   /* plus de nouvelles (grace : last dans le futur) */
enum { PK_QUERY = 1, PK_ANNOUNCE, PK_JOIN, PK_WELCOME, PK_STATE, PK_SNAP, PK_ROSTER, PK_EVENT, PK_LEAVE,
       PK_PUNCH, PK_KEEP };   /* PUNCH : ouvre le NAT vers un client ; KEEP : un spectateur dit qu'il est la */
enum { EV_RAIL = 1, EV_ROCKET, EV_ROCKET_END, EV_PARRY, EV_HIT, EV_MELEE, EV_KILL, EV_RESPAWN, EV_MATCH, EV_INFO, EV_PICKUP,
       EV_PICKUP_TAKEN, EV_COIN, EV_MELEE_PARRY,
       EV_SHOT = 15,                                      /* Eventail : un tir = un evenement (graine des plombs) */
       EV_SAW = 16,                                       /* Scie */
       EV_MINE = 17,                                      /* Ventouse : sub = MINE_EV_* */
       EV_TESLA = 18,                                     /* Tesla : relais (0) ou decharge renvoyee (1) */
       EV_SHIELD = 19,                                    /* Egide : sub = SHE_* (revendication du tireur, decisions de l'hote) */
       EV_LANCE = 20,                                    /* Lance : choc de joute, emis par l'hote seulement */
       EV_SLAM = 21,                                      /* identifiants figes des armes : PLAN.md (15 a 20 reserves) */
       EV_SKILL = 22,                                     /* Competences : sub = SK_*, target = cible marquee */
       EV_OBJ = 23 };                                     /* objectifs des modes : sub = OBJ_*, target = equipe, actor = joueur, f[0] = indice */

#pragma pack(push, 1)
typedef struct { unsigned int magic; unsigned char type, from, ver, pad; } PkHead;
typedef struct {
    unsigned int eid, netId;
    unsigned char kind, from, target, sub, actor, pad[3];
    float f[7];
} NetEvent;
typedef struct {                                          /* teams : teamSize ; map : curMap ; ranked : partie classee */
    PkHead h; char name[16]; unsigned char players, maxp, teams, map, ranked, pad[3];
} PkAnnounce;
typedef struct {                                          /* mode classe : identite, cote et matchs joues (cache) */
    PkHead h; char name[16]; unsigned char spec, rplay; unsigned short mmr; unsigned char rid[16];
} PkJoin;
typedef struct { PkHead h; unsigned char id, refused, pad[2]; } PkWelcome;
typedef struct { PkHead h; NetPState st; } PkState;
typedef struct { unsigned char active, alive; short hp, kills, deaths; NetPState st; unsigned char team; } SnapP;
typedef struct { PkHead h; float matchT; unsigned char matchOver, winner; unsigned short pickMask; unsigned char fragLimit, teamSize;
                 SnapP p[MAXP];
                 unsigned char rMode, rFlags, rMinutes, rAmmo;   /* regles : mode, grappin | boutique << 1, duree, munitions */
                 ObjState obj;
                 double clk; unsigned char clkSrv, pad2[3]; } PkSnap;   /* heure de l'hote a l'envoi, calee sur le serveur (horloge.c) */
typedef struct {
    PkHead h; char names[MAXP][16];
    unsigned short mmr[MAXP]; unsigned char rplay[MAXP];  /* mode classe : cote et matchs annonces par chacun */
    unsigned char ranked, nleft, mid[8];                  /* partie classee, nombre de partis, identifiant du match */
    unsigned char rid[MAXP][16], left[MAXP][16];          /* identites de classement des joueurs et des partis */
} PkRoster;
typedef struct { PkHead h; NetEvent ev; } PkEvent;
#pragma pack(pop)

typedef struct {
    char name[16]; struct sockaddr_in addr; int players, maxp; double seen;
    int online, spec;                                     /* online : salon du lobby (addr = ip:port public) ; spec : spectateurs */
    int teams;                                            /* LAN : mode annonce (0 chacun pour soi, N = NvN) */
    char id[8];                                           /* identifiant du salon chez le lobby */
    int map;                                              /* carte annoncee (LAN), -1 = inconnue (salon en ligne) */
    int ranked, mmr;                                      /* mode classe : salon classe, cote moyenne (en ligne) */
} Found;
#define MAXFOUND 12
static Found found[MAXFOUND];
static int nFound;

typedef struct { int active; struct sockaddr_in addr; double lastHeard; } Spectator;   /* hote seulement */
static Spectator SP[MAXSPEC];

typedef struct { NetEvent ev; int left; } OutEv;
#define OUTQ 128
static OutEv outq[OUTQ];
static int outqHead;
static unsigned int evCounter, seenIds[512];
static int seenHead;

typedef struct { int killer, victim, kind, info; float t; } KillFeed;
static KillFeed kfeed[6];
static int killerOfMe = -1;

static SOCKET sock = INVALID_SOCKET;
static int wsaReady, connected, joining, pktIn, pktOut;
static struct sockaddr_in hostAddr;
static float sendAcc, rosterAcc, queryAcc, joinAcc;
static double joinStartT, lastSnapT, netStatusT;
static char netStatus[96], hostIpText[96];
static FILE *botLog;
static int joinSpec, joinOnline;                          /* la connexion en cours est un spectateur / vient du lobby */
static int lanOff;                                        /* --nolan : pas de recherche par broadcast (tests du lobby) */
static void online_reset(void);
static void lb_update(float dt);
static int stun_parse(const unsigned char *b, int n, struct sockaddr_in *out);
static void stun_got(const struct sockaddr_in *a);

static void netlog(const char *fmt, ...) {
    if (!botLog) return;
    va_list ap;
    va_start(ap, fmt);
    fprintf(botLog, "[%7.2f] ", nowT);
    vfprintf(botLog, fmt, ap);
    fputc('\n', botLog);
    fflush(botLog);
    va_end(ap);
}

static void set_status(const char *s) {
    snprintf(netStatus, sizeof netStatus, "%s", s);
    netStatusT = nowT;
    netlog("statut : %s", s);
}

static const char *pname(int i) { return (i >= 0 && i < MAXP && NP[i].name[0]) ? NP[i].name : "?"; }

static int wsa_init(void) {
#ifndef _WIN32
    wsaReady = 1;
    return 1;
#else
    if (!wsaReady) {
        WSADATA w;
        if (WSAStartup(MAKEWORD(2, 2), &w) != 0) return 0;
        wsaReady = 1;
    }
    return 1;
#endif
}

static int net_open(unsigned short port) {
    if (!wsa_init()) return 0;
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) return 0;
#ifdef _WIN32
    u_long nonBlocking = 1;
    ioctlsocket(sock, FIONBIO, &nonBlocking);
#else
    fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);
#endif
    int yes = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char *)&yes, sizeof yes);
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&a, sizeof a) != 0) { closesocket(sock); sock = INVALID_SOCKET; return 0; }
    return 1;
}

static void net_send(const void *d, int len, const struct sockaddr_in *to) {
    if (sock == INVALID_SOCKET) return;
    sendto(sock, (const char *)d, len, 0, (const struct sockaddr *)to, sizeof *to);
    pktOut++;
}

static void head(PkHead *h, int type) {
    h->magic = NET_MAGIC; h->type = (unsigned char)type; h->from = (unsigned char)myId; h->ver = NET_VER; h->pad = 0;
}

static int same_addr(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    return a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port;
}

static void host_send_all(const void *d, int len) {
    for (int i = 1; i < MAXP; i++) if (NP[i].active) net_send(d, len, &NP[i].addr);
    for (int i = 0; i < MAXSPEC; i++) if (SP[i].active) net_send(d, len, &SP[i].addr);
}

static int spec_count(void) {
    int n = 0;
    for (int i = 0; i < MAXSPEC; i++) n += SP[i].active;
    return n;
}

static void pack_state(NetPState *s) {
    vf(s->pos, P.pos); vf(s->vel, P.vel); vf(s->hookPos, P.hookPos);
    s->yaw = P.yaw; s->pitch = P.pitch;
    s->flags = (unsigned char)((P.crouch ? NF_CROUCH : 0) | (P.sliding ? NF_SLIDE : 0) | (P.wallrun ? NF_WALLRUN : 0) |
                               (P.hook != HOOK_NONE ? NF_HOOK : 0) | (P.dead ? NF_DEAD : 0) |
                               (P.parryWin > 0 ? NF_PARRY : 0));
    s->weapon = (unsigned char)P.weapon;
    s->skin = (unsigned char)wantSkin;
    s->spare[0] = 0;                                      /* libre (ancienne specialisation) */
    s->spare[1] = (unsigned char)(shotStreak < 255 ? shotStreak : 255);   /* chaine de tirs (l'hote verifie le one-shot) */
    float swingT = P.meleeKind == MK_ANVIL ? ANVIL_SWING_T : P.meleeKind == MK_LANCE ? 0.2f :   /* Lance : estoc */
                   P.meleeKind == MK_FLAIL ? FLAIL_ANIM : 0.25f;
    s->melee = (unsigned char)(clampf(P.meleeT / swingT, 0, 1) * 255);
    if (P.meleeKind == MK_FIST)                           /* Poings : coup anime (0..2) sur 2 bits, avancement 1..63 sur 6 bits */
        s->melee = P.meleeT > 0 && P.fistAnim > 0 ?
                   (unsigned char)(((P.fistAnim - 1) << 6) | (1 + (int)(clampf(1 - P.meleeT / FIST_ANIM, 0, 1) * 62))) : 0;
    s->pad =(unsigned char)(P.meleeKind & 0x0F);      /* bits 4-7 : etats d'arme (ST_*), ajoutes par chaque arme */
    if (P.anvilState == ANVIL_DIVE) s->pad |= ST_SLAM;
    if (P.teslaOn) s->pad |= ST_ZAP;                      /* Tesla : chaque client recalcule l'arc */
    if (P.lanceState == LS_COUCH) s->pad |= ST_LANCE;   /* Lance couchee : la pointe brille chez les autres */
    if (P.meleeKind == MK_SHIELD && P.shieldState == SHS_UP && !P.dead) s->pad |= ST_SHIELD;   /* Egide : bouclier leve */
}

static void find_local_ip(void) {
    char hn[256];
    struct addrinfo hints, *res, *p;
    snprintf(hostIpText, sizeof hostIpText, "?");
    if (gethostname(hn, sizeof hn) != 0) return;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    if (getaddrinfo(hn, NULL, &hints, &res) != 0) return;
    hostIpText[0] = 0;
    for (p = res; p; p = p->ai_next) {
        const char *s = inet_ntoa(((struct sockaddr_in *)p->ai_addr)->sin_addr);
        if (strncmp(s, "127.", 4) == 0 || strlen(hostIpText) + strlen(s) + 4 >= sizeof hostIpText) continue;
        if (hostIpText[0]) strcat(hostIpText, "   ");
        strcat(hostIpText, s);
    }
    freeaddrinfo(res);
    if (!hostIpText[0]) snprintf(hostIpText, sizeof hostIpText, "127.0.0.1");
}

/* --- evenements ----------------------------------------------------- */
static int ev_seen(unsigned int id) {
    for (int i = 0; i < 512; i++) if (seenIds[i] == id) return 1;
    seenIds[seenHead] = id;
    seenHead = (seenHead + 1) % 512;
    return 0;
}

static void net_queue(const NetEvent *ev) {
    outq[outqHead].ev = *ev;
    outq[outqHead].left = 3;
    outqHead = (outqHead + 1) % OUTQ;
}

static void apply_event(const NetEvent *ev);
static int host_handle(NetEvent *ev);
static const float *pcolor(int i);

/* cree un evenement local : l'hote l'arbitre tout de suite, puis il part sur le reseau
 * (sauf si l'hote l'annule : coup pare, cible morte...) */
static void net_emit(NetEvent *ev) {
    if (netMode == NET_OFF || !connected) return;
    ev->eid = (((unsigned int)myId + 1) << 24) | (++evCounter & 0xFFFFFF);
    ev->from = (unsigned char)myId;
    ev_seen(ev->eid);
    if (netMode == NET_HOST && host_handle(ev)) return;
    net_queue(ev);
}

static NetEvent ev_new(int kind) {
    NetEvent e;
    memset(&e, 0, sizeof e);
    e.kind = (unsigned char)kind;
    return e;
}

static void net_hit(int target, float dmg, V3 knock, int weapon) {
    if (target != myId) dmg = skill_out(target, -1, dmg);   /* Competences : MARQUE, CHARGE */
    if (target != myId && target < MAXP) dmg_number(np_center(target), fminf(dmg, PVP_HIT_CAP));
    flail_combo_check(target, weapon);                    /* Fleau : combo fleau -> rail (visuel) */
    NetEvent e = ev_new(EV_HIT);
    e.target = (unsigned char)target; e.sub = (unsigned char)weapon;
    e.f[0] = dmg; vf(&e.f[1], knock);
    net_emit(&e);
}
static void net_fx_rail(V3 a, V3 b, float k) {
    NetEvent e = ev_new(EV_RAIL);
    vf(e.f, a); vf(e.f + 3, b); e.f[6] = k;
    net_emit(&e);
}
static void net_fx_kinetic(V3 a, V3 b, float e) {          /* Elan : EV_RAIL sous-type 1, actor = energie x 255 */
    NetEvent ev = ev_new(EV_RAIL);
    vf(ev.f, a); vf(ev.f + 3, b); ev.f[6] = 0;
    ev.sub = 1; ev.actor = (unsigned char)(clampf(e, 0, 1) * 255);
    net_emit(&ev);
}
static Proj *find_proj(unsigned int netId);
static void net_fx_rocket(V3 pos, V3 vel, unsigned int netId) {
    NetEvent e = ev_new(EV_ROCKET);
    e.netId = netId; vf(e.f, pos); vf(e.f + 3, vel);
    { Proj *p = find_proj(netId); if (p) e.sub = (unsigned char)p->ricLeft; }   /* RICOCHET : la copie rebondit aussi */
    net_emit(&e);
}
static void net_fx_rocket_end(unsigned int netId, V3 pos) {
    NetEvent e = ev_new(EV_ROCKET_END);
    e.netId = netId; vf(e.f, pos);
    net_emit(&e);
}
static void net_fx_parry(unsigned int netId, V3 pos, V3 vel, float mult, int parries) {
    NetEvent e = ev_new(EV_PARRY);
    e.netId = netId; vf(e.f, pos); vf(e.f + 3, vel); e.f[6] = mult; e.sub = (unsigned char)parries;
    net_emit(&e);
}
static void net_fx_coin(int sub, unsigned int netId, V3 pos, V3 vel) {
    NetEvent e = ev_new(EV_COIN);
    e.sub = (unsigned char)sub; e.netId = netId; vf(e.f, pos); vf(e.f + 3, vel);
    net_emit(&e);
}
/* Competences : EV_SKILL, sub = SK_*, target = joueur marque (255 : aucun), f[0..2] / f[3..5] = trace ou centre */
static void net_fx_skill(int sub, int target, V3 a, V3 b) {
    NetEvent e = ev_new(EV_SKILL);
    e.sub = (unsigned char)sub; e.target = (unsigned char)(target < 0 ? 255 : target);
    vf(e.f, a); vf(e.f + 3, b);
    net_emit(&e);
}
static void net_fx_melee(V3 pos, V3 dir, float sp) {
    NetEvent e = ev_new(EV_MELEE);
    vf(e.f, pos); vf(e.f + 3, dir); e.f[6] = sp;
    net_emit(&e);
}
static void net_fx_fist(V3 pos, V3 dir, float x, int sub) {   /* Poings : x = vitesse, ou distance du decor si punch-jump */
    NetEvent e = ev_new(EV_MELEE);
    vf(e.f, pos); vf(e.f + 3, dir); e.f[6] = x; e.sub = (unsigned char)sub;
    net_emit(&e);
}
static void net_fx_shot(V3 eye, V3 dir, float half, unsigned int seed) {   /* Eventail : chaque client recalcule les plombs */
    NetEvent e = ev_new(EV_SHOT);
    vf(e.f, eye); vf(e.f + 3, dir); e.f[6] = half;        /* demi-angle : encode le slide */
    e.netId = seed; e.sub = SG_PELLETS;
    e.actor = (unsigned char)(shotStreak < STREAK_MAX ? shotStreak : STREAK_MAX);   /* teinte des traceurs */
    net_emit(&e);
}
/* onde de l'Enclume : purement visuelle (les touches partent du frappeur, quand le front passe chaque cible).
 * f[0..2] centre, f[3] rayon, f[4] hauteur du mur, f[5] cap de l'ovale (atan2 x, z), f[6] vitesse horizontale, actor = |vy| */
static void net_fx_slam(V3 pos, float R, float hw, V3 dir, float vh, float vy) {
    NetEvent e = ev_new(EV_SLAM);
    vf(e.f, pos); e.f[3] = R; e.f[4] = hw; e.f[5] = atan2f(dir.x, dir.z); e.f[6] = vh;
    e.actor = (unsigned char)clampf(vy, 0, 255);
    net_emit(&e);
}
/* --- Scie : evenements ---------------------------------------------- */
/* EV_SAW : sub = SAWE_*, netId de la scie, target = joueur touche (255 : aucun), actor = moyen de rattrapage ou renvois */
static void net_fx_saw(int sub, Proj *p, int target, int actor) {
    NetEvent e = ev_new(EV_SAW);
    e.sub = (unsigned char)sub; e.netId = p->netId;
    e.target = (unsigned char)(target < 0 ? 255 : target); e.actor = (unsigned char)actor;
    vf(e.f, p->pos);
    if (sub == SAWE_THROW || sub == SAWE_BOUNCE) vf(e.f + 3, p->vel);
    if (sub == SAWE_PLANT) { vf(e.f + 3, p->sawAxis); e.f[6] = (float)saw_face(p->sawN); e.actor = (unsigned char)p->sawBounces; }
    if (sub == SAWE_HIT) e.actor = (unsigned char)p->sawBounces;
    net_emit(&e);
}
/* comme net_hit, avec f[4] = nombre de renvois de la scie (journal de l'hote) */
static void net_hit_saw(int target, float dmg, V3 knock, int w, int bounces) {
    if (target != myId) dmg = skill_out(target, -1, dmg);
    if (target != myId && target < MAXP) dmg_number(np_center(target), fminf(dmg, PVP_HIT_CAP));
    NetEvent e = ev_new(EV_HIT);
    e.target = (unsigned char)target; e.sub = (unsigned char)w;
    e.f[0] = dmg; vf(&e.f[1], knock); e.f[4] = (float)bounces;
    net_emit(&e);
}
/* EV_COIN sous-type 3 : piece relancee par une scie (f[6] = vie restante) */
static void net_fx_coin_kick(const Coin *c) {
    NetEvent e = ev_new(EV_COIN);
    e.sub = 3; e.netId = c->netId; vf(e.f, c->pos); vf(e.f + 3, c->vel); e.f[6] = c->life;
    net_emit(&e);
}
/* --- fin Scie : evenements --- */
/* --- Tesla : EV_TESLA. sub 0 = relais (netId de la piece, f = position) ; sub 1 = decharge renvoyee, decidee
 * par l'hote (actor = pareur, target = tireur, f = position du pareur) --- */
static void net_fx_tesla(int sub, unsigned int netId, V3 pos, int actor, int target) {
    NetEvent e = ev_new(EV_TESLA);
    e.sub = (unsigned char)sub; e.netId = netId; vf(e.f, pos);
    e.actor = (unsigned char)actor; e.target = (unsigned char)target;
    net_emit(&e);
}
/* --- fin Tesla --- */
/* --- Ventouse : evenements --- */
/* EV_MINE : sub = MINE_EV_*, netId de la mine, f[0..2] = a, f[3..5] = b, f[6] = x, target = joueur porteur ou pareur */
static void net_fx_mine(int sub, unsigned int netId, V3 a, V3 b, int target, float x) {
    NetEvent e = ev_new(EV_MINE);
    e.sub = (unsigned char)sub; e.netId = netId; e.target = (unsigned char)target;
    vf(e.f, a); vf(e.f + 3, b); e.f[6] = x;
    net_emit(&e);
}
/* comme net_hit, avec le netId de l'objet source : l'hote ignore les touches d'une mine dont il a donne le sort a un autre */
static void net_hit_id(int target, float dmg, V3 knock, int weapon, unsigned int netId) {
    if (target != myId) dmg = skill_out(target, -1, dmg);
    if (target != myId && target < MAXP) dmg_number(np_center(target), fminf(dmg, PVP_HIT_CAP));
    NetEvent e = ev_new(EV_HIT);
    e.target = (unsigned char)target; e.sub = (unsigned char)weapon; e.netId = netId;
    e.f[0] = dmg; vf(&e.f[1], knock);
    net_emit(&e);
}
/* --- fin Ventouse : evenements --- */
/* --- Egide : point d'entree unique des degats sur un joueur ---------- */
/* le joueur i (moi compris : rendu de l'arc) bloquerait-il un coup venant dans la direction dir ? Souffle : pas par en dessous */
static int shield_faces(int i, V3 dir, int blast) {
    if (blast == SHB_NEVER || i < 0 || i >= MAXP) return 0;
    if (blast == SHB_BLAST && dir.y > 0.75f) return 0;   /* roquette dans les pieds */
    if (i == myId) return P.meleeKind == MK_SHIELD && P.shieldState == SHS_UP && vdot(dir, view_dir()) < SHIELD_COS;
    return np_shield_up(i) && NP[i].shieldBrokenT <= 0 && NP[i].shieldRipT <= 0 && vdot(dir, np_face(i)) < SHIELD_COS;
}
/* appele par toute arme juste avant l'EV_HIT : renvoie 1 si le coup est revendique comme bloque (EV_SHIELD CLAIM) ;
 * l'hote tranche (bloque, refuse, parfait). Prediction locale : etincelles et son */
static int shield_intercept(int i, V3 dir, float dmg, V3 knock, int w, int blast, unsigned int netId, int flags) {
    if (w == W_SELF || i == myId || !shield_faces(i, dir, blast)) return 0;
    NetEvent e = ev_new(EV_SHIELD);
    e.sub = blast == SHB_BLAST ? SHE_CLAIM_BLAST : SHE_CLAIM; e.target = (unsigned char)i; e.actor = (unsigned char)w;
    e.netId = netId; e.pad[0] = (unsigned char)flags;     /* chaine pleine : si l'hote refuse le blocage, elle compte */
    e.f[0] = dmg; vf(&e.f[1], dir); vf(&e.f[4], knock);
    shield_spark_fx(vsub(np_center(i), vmul(dir, 0.5f)), dir);
    play(S_SHIELD_BLOCK, 0.6f * atten(np_center(i)), frange(0.95f, 1.05f));
    if (botMode) netlog("bouclier : %s bloque (revendique), arme %d, %.0f", pname(i), w, dmg);
    net_emit(&e);
    return 1;
}
/* flags : HF_* (tirs a la tete, jauge de precision) dans pad[0] ; l'hote les valide ou les retire */
static int hit_emit(int i, float dmg, V3 knock, int w, V3 dir, int blast, unsigned int netId, float f4, float f5, float f6, int flags) {
    if (shield_intercept(i, dir, dmg, knock, w, blast, netId, flags)) return 0;
    if (i != myId) dmg = skill_out(i, -1, dmg);           /* Competences : MARQUE, CHARGE */
    if (i != myId && i < MAXP) {                          /* tete : sans plafond ; one-shot (rail a la tete, chaine pleine) */
        int one = (flags & HF_CHAIN) || ((flags & HF_HEAD) && head_oneshot(w));
        dmg_number_k(np_center(i), one || (flags & HF_HEAD) ? dmg : fminf(dmg, PVP_HIT_CAP),
                     one ? DN_ONESHOT : (flags & HF_HEAD) ? DN_HEAD : DN_NORMAL);
    }
    flail_combo_check(i, w);                              /* Fleau : combo fleau -> rail (visuel) */
    NetEvent e = ev_new(EV_HIT);
    e.target = (unsigned char)i; e.sub = (unsigned char)w; e.netId = netId; e.pad[0] = (unsigned char)flags;
    e.f[0] = dmg; vf(&e.f[1], knock); e.f[4] = f4; e.f[5] = f5; e.f[6] = f6;
    net_emit(&e);
    return 1;
}
static int hit_player_ex(int i, float dmg, V3 knock, int w, V3 dir, int blast, unsigned int netId, float f4, float f5, float f6) {
    return hit_emit(i, dmg, knock, w, dir, blast, netId, f4, f5, f6, 0);
}
static int hit_player(int i, float dmg, V3 knock, int w, V3 dir) {
    return hit_player_ex(i, dmg, knock, w, dir, SHB_DIRECT, 0, 0, 0, 0);
}
static int hit_player_fl(int i, float dmg, V3 knock, int w, V3 dir, int flags, V3 at) {
    if (!(flags & HF_HEAD)) at = v3(0, 0, 0);             /* f[4..6] : le point sur la tete, que l'hote verifie */
    return hit_emit(i, dmg, knock, w, dir, SHB_DIRECT, 0, at.x, at.y, at.z, flags);
}
/* mon grappin s'accroche au porteur : son bouclier est arrache 1 s (le grappineur fait autorite sur son accroche) */
static void shield_rip(int i) {
    NetEvent e = ev_new(EV_SHIELD);
    e.sub = SHE_RIP; e.target = (unsigned char)i;
    NP[i].shieldRipT = SHIELD_RIP_T;
    for (int k = 0; k < 10; k++) add_part(np_center(i), vmul(rand_dir(), 6), v3(1, 0.6f, 0.2f), 0.3f, 0.08f, 4);
    play(S_SHIELD_BLOCK, 0.7f * atten(np_center(i)), 0.6f);
    snprintf(popText, sizeof popText, "BOUCLIER ARRACHE"); popT = 1.0f;
    if (botMode) netlog("bouclier : celui de %s arrache au grappin", pname(i));
    net_emit(&e);
}
/* --- fin Egide : evenements --- */

enum { KF_JOIN = 1, KF_LEAVE = 2, KF_HEAD = 4, KF_ONESHOT = 8 };   /* KillFeed.info : arrivee / depart, ou drapeaux d'un kill */
static void kfeed_add(int killer, int victim, int kind, int info) {
    memmove(&kfeed[1], &kfeed[0], sizeof kfeed - sizeof kfeed[0]);
    kfeed[0].killer = killer; kfeed[0].victim = victim; kfeed[0].kind = kind; kfeed[0].info = info; kfeed[0].t = 6;
}

static Proj *find_proj(unsigned int netId) {
    for (int i = 0; i < MAXPR; i++) if (PR[i].alive && PR[i].netId == netId) return &PR[i];
    return NULL;
}

/* --- Scie : reception ------------------------------------------------ */
/* scie d'un autre joueur : on cree ou corrige sa copie locale */
static void saw_apply(const NetEvent *ev) {
    int from = ev->from;
    if (from >= MAXP || from == myId) return;
    V3 a = fv(ev->f), b = fv(ev->f + 3);
    Proj *p = find_proj(ev->netId);
    if (ev->sub == SAWE_CATCH) {
        if (p) { p->alive = 0; play(S_SAW_CATCH, 0.4f * atten(p->pos), 1.0f); }
        return;
    }
    if (ev->sub == SAWE_PLANT && !NP[from].alive) return;   /* reemission arrivee apres sa mort */
    if (!p) {
        p = spawn_proj(PR_SAW, a, v3(0, 0, 0));
        if (!p) return;
        p->netId = ev->netId; p->sawAxis = v3(0, 1, 0);
    }
    p->kind = PR_SAW; p->owner = from;
    switch (ev->sub) {
    case SAWE_THROW: case SAWE_BOUNCE:
        p->pos = a; p->vel = b; p->sawState = SAW_FLY; p->sawT = 0; p->sawBy = -1;
        p->sawBounces = ev->sub == SAWE_BOUNCE ? ev->actor : 0;
        p->sawAxis = saw_axis(vnorm(b));
        if (ev->sub == SAWE_THROW) play(S_SAW_THROW, 0.7f * atten(a), 1.0f);
        else {
            add_beam(a, vadd(a, vmul(vnorm(b), 2)), v3(1, 0.95f, 0.8f), 0.25f);
            for (int k = 0; k < 20; k++) add_part(a, vmul(rand_dir(), 7), v3(1, 1, 1), 0.3f, 0.09f, 0);
            play(S_SAW_BOUNCE, 0.8f * atten(a), 1.0f);
        }
        break;
    case SAWE_PLANT: {
        int was = p->sawState == SAW_PLANTED && vlen(vsub(p->pos, a)) < 0.3f;
        p->pos = a; p->vel = v3(0, 0, 0); p->sawAxis = vnorm(b); p->sawN = saw_face_n((int)ev->f[6]);
        p->sawState = SAW_PLANTED; p->sawBounces = ev->actor;
        if (!was) { p->sawT = 0; play(S_SAW_PLANT, 0.6f * atten(a), 1.0f); }   /* pas a chaque reemission */
        break;
    }
    case SAWE_HIT:
        p->pos = a; p->sawState = SAW_RETURN; p->sawT = 0; p->sawBounces = ev->actor;
        burst(a, v3(1, 0.85f, 0.5f), 20, 8, 0.3f, 0.1f);
        play(S_SAW_HIT, 0.7f * atten(a), 1.0f);
        break;
    case SAWE_RECALL:
        p->pos = a; p->sawState = SAW_RECALL; p->sawT = 0;
        for (int k = 0; k < 12; k++) add_part(a, vmul(rand_dir(), 4), v3(0.4f, 0.8f, 1), 0.3f, 0.08f, 0);
        break;
    }
}

/* EV_PARRY d'une scie (adversaire qui l'a paree) : elle repart vers son lanceur */
static void saw_parry_apply(const NetEvent *ev) {
    int owner = (int)(ev->netId >> 24) - 1;
    V3 a = fv(ev->f), b = fv(ev->f + 3);
    Proj *p = find_proj(ev->netId);
    if (owner == myId) {                                  /* ma scie : accepte seulement si elle vole ou revient */
        int st = p ? p->sawState : -1;
        if (st != SAW_FLY && st != SAW_RETURN && st != SAW_PARRIED) {
            if (p && st == SAW_PLANTED) net_fx_saw(SAWE_PLANT, p, -1, 0);   /* corrige la copie de celui qui a pare */
            else if (p && st == SAW_RECALL) net_fx_saw(SAWE_RECALL, p, -1, 0);
            else if (!p) {
                Proj tmp;
                memset(&tmp, 0, sizeof tmp);
                tmp.netId = ev->netId; tmp.pos = a;
                net_fx_saw(SAWE_CATCH, &tmp, -1, SAW_HOW_PARRIED);
            }
            return;
        }
        if (botMode) netlog("scie : paree par %s", pname(ev->from));
    }
    if (!p) {
        if (owner < 0 || owner >= MAXP) return;
        p = spawn_proj(PR_SAW, a, b);
        if (!p) return;
        p->netId = ev->netId; p->owner = owner;
    }
    p->kind = PR_SAW;
    p->sawState = SAW_PARRIED; p->sawBy = ev->from; p->pos = a; p->vel = b; p->sawT = 0; p->parries = ev->sub;
    p->sawAxis = saw_axis(vnorm(b));
    for (int k = 0; k < 20; k++) add_part(a, vmul(rand_dir(), 8), v3(1, 1, 0.85f), 0.35f, 0.1f, 0);
    play(S_PARRY, 0.8f * atten(a), 1.0f);
}
/* --- fin Scie : reception --- */

/* --- Ventouse : reception ------------------------------------------- */
/* mine d'un autre joueur : copie visuelle creee ou corrigee ; ses degats sont calcules par le poseur (ou le tireur) */
static void mine_apply(const NetEvent *ev) {
    int from = ev->from;
    V3 a = fv(ev->f), b = fv(ev->f + 3);
    Proj *p = find_proj(ev->netId);
    if (ev->sub <= MINE_EV_PLAYER) {                      /* lancer, collages : seulement les copies */
        if (from == myId || from >= MAXP) return;
        if (!p) {
            p = spawn_proj(PR_MINE, a, b);
            if (!p) return;
            p->netId = ev->netId; p->mineN = v3(0, 1, 0);
        }
        p->kind = PR_MINE; p->owner = from;
    }
    switch (ev->sub) {
    case MINE_EV_THROW:
        p->pos = a; p->vel = b; p->mineAttach = 0;
        if (p->mineState == MS_NONE) { p->mineT = 0; play(S_MINE_THROW, 0.6f * atten(a), 1.0f); }
        else p->mineFell = 1;                             /* retombee d'un porteur */
        p->mineState = MS_FLY;
        break;
    case MINE_EV_WORLD:
        p->pos = a; p->vel = v3(0, 0, 0); p->mineN = b; p->mineAttach = 0;
        if (p->mineState != MS_WORLD) { p->mineState = MS_WORLD; mine_stuck_fx(p, 0.6f * atten(a)); }
        break;
    case MINE_EV_PLAYER: {
        int t = ev->target;
        if (t >= MAXP) break;
        p->mineState = MS_PLAYER; p->mineAttach = t + 1; p->mineOfs = a; p->mineArmT = MINE_ARM_PLAYER; p->mineFell = 0;
        p->mineShield = ev->f[6] > 0.5f;                  /* Egide : collee sur la face du bouclier */
        p->pos = vadd(mine_base(p), a);
        mine_stuck_fx(p, t == myId ? 1.0f : 0.6f * atten(p->pos));
        if (t == myId) {                                  /* alerte : bip, contour rouge (HUD), il faut parer */
            snprintf(popText, sizeof popText, "MINE SUR TOI ! PARE !"); popT = 1.2f;
            shake += 0.1f;
        }
        netlog("ventouse : mine de %s collee sur %s", pname(from), pname(t));
        break;
    }
    case MINE_EV_BOOM: {                                  /* la mine a saute chez le poseur (ou le tireur) */
        if (from == myId) break;
        V3 c = a;
        if (p && p->mineState == MS_PLAYER && p->mineAttach == myId + 1) c = vadd(P.pos, p->mineOfs);   /* sur mon corps */
        if (p && (p->kind == PR_MINE || p->mineState)) p->alive = 0;
        explode_fx(c);
        if (ev->f[3] > 0.5f) { burst(c, v3(1, 0.85f, 0.4f), 60, 16, 0.6f, 0.35f); play(S_EXPLO, atten(c), 0.8f); }
        break;
    }
    case MINE_EV_TORN:                                    /* arrachee par son porteur : elle repart comme une roquette renvoyee */
        if (from == myId || from >= MAXP) break;
        if (!p) {
            p = spawn_proj(PR_PARRIED, a, b);
            if (!p) break;
            p->netId = ev->netId;
        }
        p->kind = PR_PARRIED; p->mineState = MS_FLY; p->owner = from; p->pos = a; p->vel = b; p->life = 4;
        p->mult = ev->f[6]; p->parries = 1; p->orb = 0; p->mineAttach = 0;
        for (int k = 0; k < 20; k++) add_part(a, vmul(rand_dir(), 8), v3(1, 1, 0.85f), 0.35f, 0.1f, 0);
        play(S_PARRY, 0.8f * atten(a), 1.0f);
        if ((int)(ev->netId >> 24) == myId + 1) { snprintf(popText, sizeof popText, "MINE ARRACHEE PAR %s", pname(from)); popT = 1.2f; }
        netlog("ventouse : %s arrache une mine", pname(from));
        break;
    case MINE_EV_CANCEL:                                  /* hote : elle avait deja saute, l'arrachage ne compte pas */
        if (p) p->alive = 0;
        if (ev->target == myId) { snprintf(popText, sizeof popText, "TROP TARD !"); popT = 1.2f; }
        netlog("ventouse : arrachage de %s annule (mine deja explosee)", pname(ev->target));
        break;
    }
}
/* --- fin Ventouse : reception --- */

/* --- Egide : reception des decisions de l'hote (les CLAIM ne font rien ici : le resultat suit) --- */
static V3 any_center(int i) { return i == myId ? pcenter() : np_center(i); }
static void shield_apply(const NetEvent *ev) {
    int t = ev->target, a = ev->actor;
    if (t >= MAXP) return;
    V3 ct = any_center(t);
    switch (ev->sub) {
    case SHE_BLOCK: {                                     /* actor = attaquant, f[0] absorbe, f[1] jauge, f[2] passe, f[3..5] recul */
        V3 dir = a < MAXP ? vnorm(vsub(ct, any_center(a))) : v3(0, 0, -1);
        NP[t].shieldGauge = ev->f[1];
        shield_spark_fx(vsub(ct, vmul(dir, 0.5f)), dir);
        if (t == myId) {
            P.shieldGauge = ev->f[1]; P.shieldHitT = 0.2f;
            P.vel = vadd(P.vel, fv(ev->f + 3));
            if (ev->f[4] > 2) P.grounded = 0;
            hitstop = fmaxf(hitstop, 0.03f); shake += 0.12f;
            play(S_SHIELD_BLOCK, 0.9f, frange(0.95f, 1.05f));
            if (ev->f[2] > 0) { P.hurtFlash = 0.4f; play(S_HURT, 0.6f, 1.0f); }   /* souffle : la moitie passe */
            if (a < MAXP) { hurtFrom = np_center(a); hurtDirT = 0.8f; }
        } else {
            NP[t].flashT = 0.06f;
            if (a != myId) play(S_SHIELD_BLOCK, 0.6f * atten(ct), 1.0f);
        }
        if (a == myId) {                                  /* moi, l'attaquant : la prediction a deja joue le son */
            hitstop = fmaxf(hitstop, 0.03f);
            snprintf(popText, sizeof popText, "BLOQUE"); popT = 0.8f;
        }
        netlog("bouclier : %s bloque %s, %.0f absorbe, jauge %.0f%s", pname(t), pname(a), ev->f[0], ev->f[1],
               ev->f[2] > 0 ? " (souffle : la moitie passe)" : "");
        break;
    }
    case SHE_BREAK:                                       /* actor = celui qui l'a brise */
        NP[t].shieldGauge = 0; NP[t].shieldBrokenT = SHIELD_BROKEN_T;
        shield_break_fx(t == myId ? vadd(eye_pos(), vmul(view_dir(), 0.6f)) : ct);
        play(S_SHIELD_BREAK, t == myId ? 1.0f : 0.8f * atten(ct), 1.0f);
        if (t == myId) {
            P.shieldGauge = 0; P.shieldState = SHS_BROKEN; P.shieldBrokenT = SHIELD_BROKEN_T;
            snprintf(popText, sizeof popText, "BOUCLIER BRISE"); popT = 1.2f;
        } else if (a == myId) { snprintf(popText, sizeof popText, "BOUCLIER BRISE !"); popT = 1.2f; }
        netlog("bouclier : celui de %s brise", pname(t));
        break;
    case SHE_PERFECT: {                                   /* actor = porteur, target = tireur, f[0..2] -> f[3..5], f[6] renvoye */
        V3 pa = fv(ev->f), pb = fv(ev->f + 3), dir = vnorm(vsub(pb, pa));
        if (a >= MAXP) break;
        add_beam(pa, pb, v3(1, 0.85f, 0.3f), 0.7f);
        add_beam(pa, pb, v3(1, 1, 1), 0.2f);
        if (a == myId) {
            parry_feedback(pa);
            melee_parry_points(PTS_SHIELD_PERFECT);       /* parfait : 100 points au lieu de 50 */
            snprintf(popText, sizeof popText, "RENVOYE !"); popT = 1.2f;
            play(S_RAIL, 0.7f, 1.3f);
        } else play(S_PARRY, 0.7f * atten(pa), 0.85f);
        if (t == myId) {
            P.vel = vadd(P.vel, vmul(dir, 4));
            P.hurtFlash = 0.6f; shake += 0.3f; hurtFrom = pa; hurtDirT = 1.2f;
            play(S_HURT, 0.8f, 1.0f);
            snprintf(popText, sizeof popText, "RENVOYE PAR %s", pname(a)); popT = 1.2f;
        } else NP[t].flashT = 0.12f;
        impact_frame(a == myId || t == myId ? pa : pb, 0.08f);
        netlog("bouclier : parry parfait de %s, %.0f renvoyes a %s", pname(a), ev->f[6], pname(t));
        break;
    }
    case SHE_DENIED: {                                    /* comme un EV_HIT : actor = attaquant, f[0] degats, f[1..3] recul, f[4] arme */
        int w = (int)ev->f[4];
        if (t == myId) {
            P.vel = vadd(P.vel, fv(ev->f + 1));
            if (ev->f[2] > 2) P.grounded = 0;
            P.hurtFlash = 0.6f; shake += 0.25f;
            play(S_HURT, 0.8f, frange(0.9f, 1.1f));
            if (a < MAXP) { hurtFrom = np_center(a); hurtDirT = 1.2f; }
        } else {
            burst(ct, v3(1, 0.35f, 0.3f), 14, 6, 0.3f, 0.1f);
            NP[t].flashT = 0.12f;
        }
        if (a == myId) {                                  /* la revendication etait fausse : c'etait une touche */
            hitMarker = 0.18f; play(S_HIT, 0.6f, 1.0f);
            if (is_melee_w(w)) { melee_hit_points(meleePts[P.meleeKind]); P.dashCharge = fminf(2, P.dashCharge + 1); }
        }
        netlog("bouclier : blocage de %s refuse (attaquant %s, arme %d)", pname(t), pname(a), w);
        break;
    }
    case SHE_RIP:                                         /* emis par le grappineur */
        NP[t].shieldRipT = SHIELD_RIP_T;
        for (int k = 0; k < 10; k++) add_part(ct, vmul(rand_dir(), 6), v3(1, 0.6f, 0.2f), 0.3f, 0.08f, 4);
        play(S_SHIELD_BLOCK, (t == myId ? 0.9f : 0.6f * atten(ct)), 0.6f);
        if (t == myId) {
            P.shieldRipT = SHIELD_RIP_T;
            if (P.shieldState == SHS_UP) { P.shieldState = SHS_DOWN; P.shieldDownT = 0; }
            snprintf(popText, sizeof popText, "BOUCLIER ARRACHE"); popT = 1.0f;
        }
        netlog("bouclier : celui de %s arrache par %s", pname(t), pname(ev->from));
        break;
    }
}
/* --- fin Egide : reception --- */

static void pickup_taken(int i, int who) {
    PU[i].t = PICKUP_RESPAWN;
    netlog("recharge : %s prend la cellule %d", pname(who), i);
    burst(PU[i].pos, v3(0.4f, 0.9f, 1.0f), 25, 5, 0.5f, 0.12f);
    if (who == myId) {                                    /* chaque arme du loadout recoit les points de la cellule */
        float pts = (float)PU[i].pts;
        pts *= ammo_rule_mul();                           /* regles x2, ARTIFICIER */
        for (int s = 0; s < 2; s++) { float lost = gauge_add(P.loadout[s], pts); apStat[AP_CELL] += pts - lost; apStat[AP_LOST] += lost; }
        snprintf(popText, sizeof popText, "CELLULE +%d", PU[i].pts);
        popT = 1.0f;
        ammoGainT = 0.8f; ammoGainPts = PU[i].pts; ammoGainSlot = 2;
        cellsTaken++;
        play(S_PAD, 0.5f, 1.8f);
    } else {
        play(S_PAD, 0.3f * atten(PU[i].pos), 1.8f);
    }
}

/* effets d'un evenement venu d'ailleurs (ou d'une decision de l'hote) */
static void apply_event(const NetEvent *ev) {
    V3 a = fv(ev->f), b = fv(ev->f + 3);
    switch (ev->kind) {
    case EV_RAIL: {
        if (ev->sub == 1) {                               /* rayon d'Elan */
            float e = ev->actor / 255.0f;
            kin_beam_fx(a, b, v3(0, 1, 0), e);
            play(S_KIN, (0.45f + 0.45f * e) * atten(a), 0.85f + 0.3f * e);
            break;
        }
        float k = ev->f[6];
        add_beam(a, b, v3(0.3f + 0.7f * k, 0.85f - 0.05f * k, 1.0f - 0.7f * k), 0.6f);
        for (int i = 0; i < 12; i++) add_part(b, vmul(rand_dir(), 4), v3(0.6f, 0.9f, 1.0f), 0.4f, 0.1f, 10);
        play(S_RAIL, 0.6f * atten(a), frange(0.97f, 1.03f));
        break;
    }
    case EV_ROCKET: {
        Proj *p = spawn_proj(PR_ROCKET, a, b);
        if (p) { p->owner = ev->from; p->netId = ev->netId; p->ricLeft = ev->sub; }
        play(S_ROCKET, 0.7f * atten(a), 1.0f);
        break;
    }
    case EV_ROCKET_END: {
        Proj *p = find_proj(ev->netId);
        if (p) { p->alive = 0; explode_fx(a); }
        break;
    }
    case EV_PARRY: {
        Proj *p = find_proj(ev->netId);
        if ((p && p->kind == PR_SAW) || (!p && ((ev->netId >> 20) & 0xF) == PR_SAW)) { saw_parry_apply(ev); break; }   /* Scie */
        if (!p) p = spawn_proj(PR_PARRIED, a, b);
        if (p) {
            p->kind = PR_PARRIED; p->owner = ev->from; p->netId = ev->netId; p->pos = a; p->vel = b; p->life = 4;
            p->mult = ev->f[6]; p->parries = ev->sub; p->orb = 0;
            if (p->mineState || ((ev->netId >> 20) & 0xF) == PR_MINE) { p->mineState = MS_FLY; p->mineAttach = 0; }   /* Ventouse */
        }
        for (int k = 0; k < 20; k++) add_part(a, vmul(rand_dir(), 8), v3(1, 1, 0.85f), 0.35f, 0.1f, 0);
        play(S_PARRY, 0.8f * atten(a), 1.0f);
        break;
    }
    case EV_HIT: {
        int t = ev->target;
        if (t == myId) {
            if (ev->from != myId) {                       /* recul applique par la victime */
                P.vel = vadd(P.vel, fv(ev->f + 1));
                if (ev->f[2] > 2) P.grounded = 0;
                if (P.meleeKind == MK_FIST && P.fistStep && vlen(fv(ev->f + 1)) >= FIST_STAGGER_KNOCK)
                    fist_combo_break(FIST_STAGGER_CD);    /* Poings : un gros recul casse le combo */
            }
            P.hurtFlash = 0.6f; shake += 0.25f;
            play(S_HURT, 0.8f, frange(0.9f, 1.1f));
            if (ev->from != myId && ev->from < MAXP) { hurtFrom = np_center(ev->from); hurtDirT = 1.2f; }
            if ((ev->pad[0] & HF_HEAD) && ev->from != myId) {   /* touche a la tete (validee par l'hote) */
                play(S_HEADSHOT, 0.5f, 0.85f); shake += 0.2f;
                snprintf(popText, sizeof popText, "TETE !"); popT = 0.8f;
            }
            if (ev->sub == W_TESLA && ev->from < MAXP) NP[ev->from].teslaHitMeT = nowT;   /* Tesla : verrou visuel */
            {                                             /* Scie : ma scie paree m'a touche, je la rattrape d'office */
                Proj *s = saw_mine();
                if (s && ev->sub == W_PARRY && s->sawState == SAW_PARRIED && s->sawBy == ev->from) saw_catch(s, SAW_HOW_PARRIED);
            }
        } else if (t < MAXP && NP[t].active) {
            burst(np_center(t), v3(1, 0.35f, 0.3f), 14, 6, 0.3f, 0.1f);
            if (ev->pad[0] & HF_HEAD) burst(np_head(t), v3(1, 0.85f, 0.3f), 16, 7, 0.35f, 0.08f);   /* tete : eclat dore */
            NP[t].flashT = 0.12f;
        }
        if (ev->sub == W_FLAIL && ev->from < MAXP && t < MAXP) {   /* --- Fleau : la chaine tendue, la victime est tiree --- */
            V3 ha = ev->from == myId ? pcenter() : np_center(ev->from), ct = t == myId ? pcenter() : np_center(t);
            add_beam(vadd(ha, v3(0, 0.3f, 0)), ct, v3(0.55f, 0.57f, 0.62f), 0.25f);
            if (t == myId && ev->from != myId) play(S_HOOK, 0.6f, 0.7f);   /* zip de cable */
        }                                                 /* --- fin Fleau --- */
        break;
    }
    case EV_MELEE:
        if (ev->sub) {                                    /* --- Poings : sub = coup (1..3) | 0x10 punch-jump --- */
            if (ev->sub & 0x10) fist_pj_fx(vadd(a, vmul(b, ev->f[6])), vmul(b, -1), 0);   /* normale : face au coup */
            else {
                for (int k = 0; k < 6; k++)
                    add_part(vadd(a, vmul(b, 0.5f + k * 0.14f)), vmul(b, 3), v3(0.55f, 0.9f, 1), 0.12f, 0.06f, 0);
                play((ev->sub & 3) == 3 ? S_FIST_UPPER : S_DASH, 0.35f * atten(a), (ev->sub & 3) == 3 ? 1.0f : 2.2f);
            }
            break;
        }                                                 /* --- fin Poings --- */
        if (ev->from < MAXP && (NP[ev->from].st.pad & 15) == MK_ANVIL) {   /* coup au sol de l'Enclume : plus lourd */
            for (int k = 0; k < 12; k++)
                add_part(vadd(a, vmul(b, 1.5f + k * 0.15f)), v3(b.x * 2, -4, b.z * 2), v3(1, 0.75f, 0.45f), 0.18f, 0.1f, 0);
            play(S_DASH, 0.45f * atten(a), 0.8f);
            break;
        }
        if (ev->from < MAXP && (NP[ev->from].st.pad & 15) == MK_FLAIL) {   /* --- Fleau : arc de particules a 6 m --- */
            float yw = atan2f(b.x, -b.z), pt = asinf(clampf(b.y, -1, 1)), rg = flail_range(ev->f[6]) * 0.85f;
            for (float an = -1.2f; an <= 1.21f; an += 0.15f) {
                V3 d = flail_dir(yw + an, pt);
                add_part(vadd(a, vmul(d, rg)), vmul(d, 2), ev->f[6] > 15 ? v3(1, 0.7f, 0.3f) : v3(0.75f, 0.78f, 0.85f), 0.2f, 0.07f, 0);
            }
            play(S_FLAIL_SWING, 0.6f * atten(a), 1.0f);
            break;
        }                                                 /* --- fin Fleau --- */
        if (ev->from < MAXP && (NP[ev->from].st.pad & 15) == MK_SHIELD) {   /* Egide : coup de bouclier, mur de particules */
            for (int k = 0; k < 14; k++)
                add_part(vadd(a, vadd(vmul(b, 0.9f + 0.05f * k), vmul(rand_dir(), 0.35f))), vmul(b, 5), v3(0.6f, 0.85f, 1), 0.15f, 0.07f, 0);
            play(S_SHIELD_BASH, 0.5f * atten(a), 1.3f);
            break;
        }
        if (ev->from < MAXP && (NP[ev->from].st.pad & 15) == MK_LANCE) {   /* estoc de la Lance : trait droit */
            for (int k = 0; k < 14; k++) add_part(vadd(a, vmul(b, 0.8f + k * 0.18f)), vmul(b, 4), v3(0.9f, 0.9f, 1), 0.12f, 0.06f, 0);
            play(S_LANCE_THRUST, 0.6f * atten(a), 1.0f);
            break;
        }
        for (int k = 0; k < 10; k++) add_part(vadd(a, vmul(b, 1.5f + k * 0.15f)), vmul(b, 3), v3(0.9f, 0.9f, 1), 0.15f, 0.08f, 0);
        play(S_DASH, 0.4f * atten(a), 1.8f);
        break;
    case EV_SHOT: {                                       /* Eventail d'un autre joueur : memes plombs, traceurs seulement */
        V3 dir[SG_PELLETS], n;
        sg_pellet_dirs(b, ev->f[6], ev->netId, dir);
        for (int k = 0; k < SG_PELLETS && k < ev->sub; k++) {
            float tw = ray_world(a, dir[k], SG_RANGE, &n), t;
            V3 mn, mx;
            for (int i = 0; i < MAXP; i++) {              /* arret visuel sur les corps affiches */
                if (i == ev->from || !NP[i].active || !NP[i].alive || same_team(i, ev->from)) continue;
                if (i == myId) { if (P.dead) continue; paabb(P.pos, pheight(), &mn, &mx); }
                else np_aabb(i, 0.05f, &mn, &mx);
                if (ray_box(a, dir[k], mn, mx, tw, &t, NULL) && t < tw) tw = t;
            }
            add_tracer(vadd(a, vmul(b, 0.7f)), vadd(a, vmul(dir[k], tw)), sg_tint(ev->actor / (float)STREAK_MAX));
        }
        if (ev->from < MAXP) NP[ev->from].sgFireT = 0.12f;
        play(S_SG_FIRE, 0.75f * atten(a), 1.0f);
        break;
    }
    case EV_SLAM: {                                       /* onde de choc d'un autre joueur : effets seulement */
        float R = ev->f[3], vy = ev->actor;
        slam_vis_add(a, v3(sinf(ev->f[5]), 0, cosf(ev->f[5])), R, ev->f[6], ev->f[4]);   /* mur qui s'etend */
        burst(a, v3(0.75f, 0.72f, 0.7f), 30, 6, 0.6f, 0.2f);
        shards(a, v3(0.55f, 0.52f, 0.5f), 6 + (int)(vy / 5), 9);
        play(S_ANVIL_SLAM, clampf(vy / 40, 0.5f, 1) * atten(a), 1.1f - vy * 0.005f);
        if (!spectating && vlen(vsub(P.pos, a)) < R + 3) shake += 0.15f;
        break;
    }
    case EV_KILL: {
        int k = ev->actor, v = ev->target, kf = ev->pad[0];   /* kf : HF_HEAD, HF_CHAIN, HF_ONESHOT (decides par l'hote) */
        if (v >= MAXP) break;
        kfeed_add(k, v, ev->sub, ((kf & HF_HEAD) ? KF_HEAD : 0) | ((kf & HF_ONESHOT) ? KF_ONESHOT : 0));
        maps_heat_add(k, v);                              /* avant que la victime ne disparaisse */
        V3 at = v == myId ? pcenter() : np_center(v);
        const float *vc0 = pcolor(v);                     /* le corps eclate dans la couleur de son equipe */
        burst(at, v3(vc0[0], vc0[1], vc0[2]), 70, 12, 0.9f, 0.18f);
        burst(at, v3(1, 0.4f, 0.3f), 30, 9, 0.7f, 0.14f);
        shards(at, v3(vc0[0] * 0.8f, vc0[1] * 0.8f, vc0[2] * 0.8f), 22, 11);
        add_light(at, v3(vc0[0], vc0[1], vc0[2]), 7, 0.5f);
        if (k == myId && v != myId) { impact_frame(at, 0.08f); killMarkT = 0.45f; }
        play(S_EXPLO, 0.7f * atten(at), 1.3f);
        NP[v].alive = 0;
        saw_remove_owner(v);                              /* Scie : disparait avec son lanceur */
        if (v == myId) {
            P.dead = 1; P.deathT = 0; P.hook = HOOK_NONE; P.hookEnemy = P.hookPlayer = 0;
            P.lanceState = LS_IDLE;                       /* Lance : relevee */
            killerOfMe = k;
            if (!fellOut) killcam_start(k, ev->sub);          /* camera de mort */
            eco_gain(ECO_DEATH, NULL);                    /* boutique : de quoi se racheter quelque chose */
            if (botMode) eco_bot_shop();
        }
        if (k == myId && v != myId) {
            if (is_melee_w(ev->sub)) ammo_points(PTS_MELEE_KILL, AP_KILL);   /* kill en melee : +100 points */
            play(S_KILL, 0.7f, 1.0f);
            combo++; comboT = 3; hitMarker = 0.25f;
            style_kill(ev->sub);
            eco_gain(ECO_KILL, "KILL");
            snprintf(popText, sizeof popText, "%s%s", (kf & HF_ONESHOT) ? "ONE-SHOT ! " : (kf & HF_HEAD) ? "TETE ! " : "ELIMINE ", pname(v));
            popT = 1.5f;
            if (kf & (HF_HEAD | HF_ONESHOT)) { play(S_HEADSHOT, 0.8f, 0.75f); impact_frame(at, 0.1f); }
        }
        if (v == myId) kcOneShot = kf;                    /* camera de mort : "ONE-SHOT" / "TETE" */
        netlog("kill : %s -> %s (arme %d)%s%s", pname(k), pname(v), ev->sub, (kf & HF_HEAD) ? " a la tete" : "",
               (kf & HF_ONESHOT) ? ((kf & HF_CHAIN) ? ", one-shot (chaine pleine)" : ", one-shot") : "");
        break;
    }
    case EV_RESPAWN: {
        int t = ev->target;
        if (t >= MAXP) break;
        NP[t].alive = 1; NP[t].disp = a; vf(NP[t].st.pos, a);
        NP[t].shieldGauge = SHIELD_GAUGE_MAX; NP[t].shieldBrokenT = NP[t].shieldRipT = 0;   /* Egide : bouclier neuf */
        saw_remove_owner(t);                              /* Scie : pas de fantome d'une vie precedente */
        if (t == myId) {
            P.pos = P.prevPos = a; P.vel = v3(0, 0, 0);
            P.dead = 0; P.hp = 100; P.hook = HOOK_NONE; P.hookEnemy = P.hookPlayer = 0;
            P.dashCharge = 2; P.airJumps = 1; P.crouch = P.sliding = P.wallrun = 0;
            loadout_apply();
            anvil_reset();
            if (P.skill != SK_AIM) P.skillCh = (float)skillDef[P.skill].max;   /* AIMANT : la recharge continue */
            P.ricArmed = 0; P.chargeT = P.chargeHold = 0; P.aimT = 0;
            P.yaw = atan2f(-a.x, a.z); P.pitch = 0; P.eyeH = EYE_STAND; P.roll = 0;
            shotStreak = 0;
        }
        burst(vadd(a, v3(0, 1, 0)), v3(0.4f, 0.9f, 1), 40, 6, 0.6f, 0.12f);
        netlog("respawn : %s en (%.0f, %.0f, %.0f)", pname(t), a.x, a.y, a.z);
        break;
    }
    case EV_MATCH:
        if (ev->sub == 0) {
            if (ev->target < NMAPS && ev->target != curMap) {   /* changement de carte entre deux matchs */
                if (netMode == NET_CLIENT) lastSnapT = fmax(lastSnapT, nowT + NET_LOAD_GRACE);   /* l'hote charge aussi */
                load_map(ev->target);
                if (!spectating) { P.pos = P.prevPos = maps[curMap].start; P.vel = v3(0, 0, 0); }
                else { P.pos = P.prevPos = vadd(maps[curMap].menuCam, v3(0, maps[curMap].specUp, 0)); }
                netlog("carte : %s", maps[curMap].name);
            }
            matchOver = 0;
            eco_reset();                                  /* boutique : pieces de depart, arme de depart a choisir */
            if (botMode && ecoNeedStarter) eco_bot_shop();
            for (int i = 0; i < MAXPR; i++) if (PR[i].kind == PR_MINE) PR[i].alive = 0;   /* Ventouse : nouveau match */
            memset(kfeed, 0, sizeof kfeed);
            snprintf(msg, sizeof msg, "COMBAT !");
            msgT = 2;
            play(S_WAVE, 0.7f, 1.0f);
            netlog("debut du match");
        } else {
            matchOver = 1; matchWinner = ev->target; matchEndT = 8;
            if (teamSize > 0 && ev->target >= TEAM_DRAW) snprintf(msg, sizeof msg, "EGALITE");
            else if (teamSize > 0) snprintf(msg, sizeof msg, "EQUIPE %s GAGNE", teamNames[ev->target & 1]);
            else snprintf(msg, sizeof msg, "%s GAGNE", pname(ev->target));
            msgT = 4;
            play(S_WAVE, 0.8f, 0.8f);
            netlog("fin du match, gagnant : %s", msg);
        }
        break;
    case EV_OBJ: {                                        /* objectifs : message, son, pieces */
        int t = ev->target & 1, a = ev->actor, idx = (int)ev->f[0];
        static const char *teamM[2] = { "BLEU", "ROUGE" };   /* au masculin : DRAPEAU BLEU, LES BLEUS (teamNames : EQUIPE BLEUE) */
        const char *tn = teamM[t];
        static const char *zl[NZONES] = { "A", "B", "C" };
        int mine = myId < MAXP && (NP[myId].team & 1) == t;
        switch (ev->sub) {
        case OBJ_ZONE: snprintf(msg, sizeof msg, "ZONE %s PRISE PAR LES %sS", zl[idx % NZONES], tn); break;
        case OBJ_FLAG_TAKE: snprintf(msg, sizeof msg, "DRAPEAU %s PRIS PAR %s", tn, a < MAXP ? pname(a) : "?"); break;
        case OBJ_FLAG_DROP: snprintf(msg, sizeof msg, "DRAPEAU %s TOMBE", tn); break;
        case OBJ_FLAG_RETURN: snprintf(msg, sizeof msg, "DRAPEAU %s RENDU", tn); break;
        case OBJ_FLAG_CAPTURE: snprintf(msg, sizeof msg, "LES %sS MARQUENT !", tn); break;
        case OBJ_SHIELD: snprintf(msg, sizeof msg, "BOUCLIER A %d %%", idx); break;
        }
        msgT = 2.5f;
        play(ev->sub == OBJ_FLAG_CAPTURE || ev->sub == OBJ_ZONE ? S_WAVE : S_PAD, 0.7f, mine ? 1.2f : 0.8f);
        if (a == myId) eco_gain(ev->sub == OBJ_FLAG_DROP ? 0 : ECO_OBJ, "OBJECTIF");
        else if (mine && ev->sub != OBJ_FLAG_DROP && ev->sub != OBJ_SHIELD && ev->sub != OBJ_FLAG_TAKE) eco_gain(ECO_TEAM_OBJ, NULL);
        netlog("objectif : %s", msg);
        break;
    }
    case EV_SKILL:                                        /* Competences d'un autre joueur */
        if (ev->sub == SK_MARK) {
            add_beam(a, b, v3(0.85f, 0.35f, 1), 0.3f);
            if (ev->target == myId) {
                markedMeT = MARK_T;
                snprintf(popText, sizeof popText, "TU ES MARQUE"); popT = 1.2f;
                play(S_HIT, 0.6f, 0.6f);
            }
        } else if (ev->sub == SK_CHARGE) burst(a, v3(1, 0.55f, 0.2f), 30, 5, 0.4f, 0.1f);
        else if (ev->sub == SK_ALLIN) { play(S_EXPLO, 0.5f * atten(a), 1.5f); }
        else if (ev->sub == SK_AIM) { burst(a, v3(1, 0.95f, 0.45f), 24, 4, 0.4f, 0.1f); play(S_PAD, 0.4f * atten(a), 1.8f); }
        break;
    case EV_COIN:                                         /* piece d'un autre joueur : visible, mais pas a nous */
        if (ev->sub == 4) {                               /* MIRAGE : leurre immobile (f[3] = orientation) */
            for (int i = 0; i < MAXCOIN; i++)
                if (!CO[i].alive) {
                    memset(&CO[i], 0, sizeof CO[i]);
                    CO[i].alive = 1; CO[i].decoy = 1; CO[i].owner = ev->from; CO[i].netId = ev->netId;
                    CO[i].pos = a; CO[i].life = DECOY_T; CO[i].spin = ev->f[3];
                    break;
                }
        } else if (ev->sub == 0) {
            for (int i = 0; i < MAXCOIN; i++)
                if (!CO[i].alive) {
                    CO[i].alive = 1; CO[i].owner = ev->from; CO[i].netId = ev->netId;
                    CO[i].pos = a; CO[i].vel = b; CO[i].life = 4; CO[i].spin = 0; CO[i].teslaT = 0; CO[i].flailStruck = 0; CO[i].decoy = 0;
                    break;
                }
        } else if (ev->sub == 1 && ev->f[3] == 1) {       /* Eventail : piece eclatee par un plomb */
            for (int i = 0; i < MAXCOIN; i++)
                if (CO[i].alive && CO[i].netId == ev->netId) CO[i].alive = 0;
            sg_burst_fx(a, 0);
        } else if (ev->sub == 3) {                        /* relancee par une scie */
            for (int i = 0; i < MAXCOIN; i++)
                if (CO[i].alive && !CO[i].decoy && CO[i].netId == ev->netId) { CO[i].pos = a; CO[i].vel = b; CO[i].life = ev->f[6]; }
        } else if (ev->sub == 2) {                        /* --- Fleau : frappee (visuelle ici, mais le rail peut la voler) --- */
            int ci = -1;
            for (int i = 0; i < MAXCOIN && ci < 0; i++) if (CO[i].alive && CO[i].netId == ev->netId) ci = i;
            for (int i = 0; i < MAXCOIN && ci < 0; i++)
                if (!CO[i].alive) { ci = i; CO[i].alive = 1; CO[i].owner = ev->from; CO[i].netId = ev->netId; CO[i].spin = 0; CO[i].teslaT = 0; }
            if (ci >= 0 && !CO[ci].decoy) {
                CO[ci].flailStruck = 1; CO[ci].pos = a; CO[ci].vel = b; CO[ci].life = FLAIL_COIN_LIFE;
                play(S_FLAIL_COIN, 0.7f * atten(a), 1.0f);
            }                                             /* --- fin Fleau --- */
        } else {
            for (int i = 0; i < MAXCOIN; i++)
                if (CO[i].alive && CO[i].netId == ev->netId) {
                    CO[i].alive = 0;
                    for (int k = 0; k < 16; k++) add_part(CO[i].pos, vmul(rand_dir(), 6), v3(1, 0.85f, 0.3f), 0.3f, 0.1f, 0);
                    play(S_PARRY, 0.45f * atten(CO[i].pos), 1.5f);
                }
        }
        break;
    case EV_MELEE_PARRY: {                                /* epee paree (ou choc de deux epees) */
        int def = ev->actor, att = ev->target, clash = ev->sub;
        int wave = (int)ev->f[0] == W_SLAM && att == myId && P.anvilLandT < 0.5f;   /* c'est mon onde qui a ete paree */
        if (def >= MAXP || att >= MAXP) break;
        V3 pd = def == myId ? pcenter() : np_center(def), pa = att == myId ? pcenter() : np_center(att);
        V3 mid = vlerp(pd, pa, 0.5f);
        for (int k = 0; k < 30; k++) add_part(mid, vmul(rand_dir(), 10), v3(1, 0.95f, 0.7f), 0.35f, 0.1f, 0);
        play(S_PUNCH, 0.8f * atten(mid), 1.6f);
        int fistW = (int)ev->f[0] == W_FIST;              /* Poings : coup pare, ou choc avec un poing */
        if (def == myId) {
            parry_feedback(mid);
            ammo_points(P.meleeKind == MK_SHIELD ? (float)PTS_SHIELD_PERFECT : (float)PTS_PARRY, AP_PARRY);   /* decision de l'hote ; Egide : parfait */
            snprintf(popText, sizeof popText, clash ? (fistW || P.meleeKind == MK_FIST ? "CHOC !" : "CHOC D'EPEES !") : "PARRY !");
            if (clash) P.vel = vadd(P.vel, vadd(vmul(vnorm(vsub(pd, pa)), 8), v3(0, 3, 0)));
            if (P.meleeKind == MK_FIST) {                 /* Poings : un parry rend le combo utile ; un choc le casse */
                if (clash) fist_combo_break(FIST_END_CD);
                else if (P.fistStep) P.swingLanded = 1;
            }
        }
        if (att == myId) {                                /* l'attaquant est repousse, epee bloquee */
            if (is_melee_w((int)ev->f[0]) && nowT - P.undoT < REFUND_UNDO_T) {   /* le coup ne compte plus : on rend ses points */
                gauge_remove(P.loadout[0], P.undoPts[0]); gauge_remove(P.loadout[1], P.undoPts[1]);
                P.undoPts[0] = P.undoPts[1] = 0;
            }
            if (wave) {                                   /* Enclume : onde paree, renvoye en l'air, marteau bloque */
                P.anvilState = ANVIL_IDLE; P.anvilStunT = 0;
                P.vel = vadd(v3(P.vel.x, 0, P.vel.z), vadd(vmul(vnorm(vsub(pa, pd)), 6), v3(0, SLAM_PARRIED_UP, 0)));
                P.meleeCd = fmaxf(P.meleeCd, MELEE_PARRIED_CD);
            } else if ((int)ev->f[0] == W_FLAIL) {        /* Fleau : la chaine s'enroule, tire vers le pareur */
                flail_parried(pd, pa);
            } else {
                int lance = (int)ev->f[0] == W_LANCE;     /* Lance paree : indisponible 1,5 s, relevee */
                P.vel = vadd(P.vel, vadd(vmul(vnorm(vsub(pa, pd)), clash ? 8.0f : 11.0f), v3(0, 3, 0)));
                P.meleeCd = fmaxf(P.meleeCd, clash ? MELEE_CD : lance ? LANCE_PARRIED_CD : MELEE_PARRIED_CD);
                if (lance) { P.lanceState = LS_IDLE; P.lanceVc = 0; }
                if (fistW) fist_combo_break(clash ? FIST_END_CD : MELEE_PARRIED_CD);   /* Poings : combo casse */
            }
            P.grounded = 0;
            hitstop = fmaxf(hitstop, 0.1f);
            shake += 0.3f;
            snprintf(popText, sizeof popText, clash ? (fistW ? "CHOC !" : "CHOC D'EPEES !") : wave ? "ONDE PAREE PAR %s" :
                     (int)ev->f[0] == W_LANCE ? "LANCE PAREE PAR %s" : fistW ? "POING PARE PAR %s" :
                     (int)ev->f[0] == W_FLAIL ? "CHAINE RETOURNEE PAR %s" : "PARE PAR %s", pname(def));
            popT = 1.2f;
        }
        netlog("%s : %s -> %s", clash ? (fistW ? "choc de poings" : "choc d'epees") : (int)ev->f[0] == W_SLAM ? "enclume paree" :
               (int)ev->f[0] == W_LANCE ? "lance paree" : (int)ev->f[0] == W_FLAIL ? "fleau pare" : fistW ? "poing pare" : "epee paree",
               pname(att), pname(def));
        break;
    }
    case EV_LANCE: {                                      /* --- Lance : choc de joute arbitre par l'hote --- */
        int w = ev->actor, l = ev->target;                /* sub 1 : w gagne contre l ; sub 2 : les deux desarconnes */
        V3 at = fv(ev->f + 4);
        if (w >= MAXP || l >= MAXP) break;
        for (int k = 0; k < 40; k++) add_part(at, vmul(rand_dir(), 12), v3(1, 1, 0.95f), 0.35f, 0.1f, 0);
        play(S_LANCE_CLASH, atten(at), 1.0f);
        if (ev->sub == 2) {
            if (w == myId || l == myId) {                 /* double desarconnement : chacun repart en arriere */
                V3 o = np_center(w == myId ? l : w);
                P.vel = vadd(vmul(vnorm(vsub(pcenter(), o)), LANCE_UNHORSE), v3(0, 3, 0));
                P.grounded = 0; P.meleeCd = fmaxf(P.meleeCd, LANCE_HIT_CD); P.lanceState = LS_IDLE; P.lanceVc = 0;
                lance_refund();
                snprintf(popText, sizeof popText, "DESARCONNES !"); popT = 1.2f;
                hitstop = fmaxf(hitstop, 0.1f); shake += 0.3f;
                impact_frame(at, 0.1f);
            }
            NP[w].flashT = 0.12f;
        } else {
            if (l == myId) {                              /* joute perdue : recul du vainqueur, lance indisponible 1,5 s */
                P.vel = vadd(P.vel, fv(ev->f + 1));
                if (ev->f[2] > 2) P.grounded = 0;
                P.meleeCd = fmaxf(P.meleeCd, LANCE_PARRIED_CD); P.lanceState = LS_IDLE; P.lanceVc = 0;
                lance_refund();
                if (ev->f[0] > 0) {                       /* degats portes par cet evenement (sinon par l'EV_HIT du vainqueur) */
                    P.hurtFlash = 0.6f; shake += 0.25f;
                    play(S_HURT, 0.8f, 1.0f);
                    hurtFrom = np_center(w); hurtDirT = 1.2f;
                }
                snprintf(popText, sizeof popText, "DESARCONNE PAR %s", pname(w)); popT = 1.2f;
                impact_frame(at, 0.1f);
            }
            if (w == myId && !(P.lanceHitT < LANCE_JOUST_LOCK && P.lanceHitWho == l + 1)) {
                /* vainqueur designe par l'hote sans avoir detecte sa propre touche : effets de lance_strike */
                lance_win();
                lance_juice(at, vnorm(vsub(at, eye_pos())), vlen(P.vel) * 2);
                P.lanceHitT = 0; P.lanceHitWho = l + 1;
            }
            if (w == myId) { snprintf(popText, sizeof popText, "JOUTE GAGNEE"); popT = 1.0f; }
        }
        NP[l].flashT = 0.12f;
        netlog("choc de joute (%s) : %s / %s", ev->sub == 2 ? "double desarconnement" : "gagne", pname(w), pname(l));
        break;
    }                                                     /* --- fin Lance --- */
    case EV_PICKUP_TAKEN:
        if (ev->sub < npu) pickup_taken(ev->sub, ev->target);
        break;
    case EV_SAW:
        saw_apply(ev);
        break;
    case EV_MINE:                                         /* Ventouse */
        mine_apply(ev);
        break;
    case EV_SHIELD:                                       /* Egide */
        shield_apply(ev);
        break;
    case EV_TESLA:                                        /* --- Tesla --- */
        if (ev->sub == 0) {                               /* relais active par un autre : sa copie flotte et crepite */
            if (ev->from == myId || ev->from >= MAXP) break;
            for (int i = 0; i < MAXCOIN; i++) {
                Coin *c = &CO[i];
                if (!c->alive || c->netId != ev->netId) continue;
                if (c->decoy || (c->teslaT > 0 && c->teslaBy < ev->from)) break;   /* MIRAGE : jamais relais */   /* activations simultanees : le plus petit id gagne */
                c->pos = a; c->vel = v3(0, 0, 0);
                c->teslaT = TESLA_RELAY_T; c->teslaPulse = 0; c->teslaBy = ev->from;
                burst(a, v3(0.5f, 0.75f, 1), 20, 6, 0.3f, 0.1f);
                play(S_TESLA_ZAP, 0.5f * atten(a), 0.7f);
                break;
            }
        } else {                                          /* decharge renvoyee par un parry (decision de l'hote) */
            int def = ev->actor, sh = ev->target;
            if (def >= MAXP || sh >= MAXP) break;
            V3 ps = sh == myId ? pcenter() : np_center(sh);
            tesla_link(TK_PLAYER, def, TK_PLAYER, sh, a, ps, 0.25f, 1);
            for (int k = 0; k < 24; k++) add_part(vlerp(a, ps, frand()), vmul(rand_dir(), 6), v3(1, 0.85f, 0.4f), 0.35f, 0.09f, 0);
            if (def == myId) {
                parry_feedback(a);
                ammo_points(PTS_PARRY, AP_PARRY);
                snprintf(popText, sizeof popText, "DECHARGE RENVOYEE"); popT = 1.2f;
            }
            if (sh == myId) {                             /* moi, le tireur : surcharge, plus de tir pendant 0,4 s */
                P.teslaStunT = TESLA_STUN + 0.5f * TICK; P.teslaOn = 0;
                tesla_cancel();
                hitstop = fmaxf(hitstop, 0.08f); shake += 0.3f;
                impact_frame(eye_pos(), 0.05f);
                snprintf(popText, sizeof popText, "SURCHARGE !"); popT = 1.2f;
                play(S_TESLA_ZAP, 0.9f, 0.55f);
                teslaStuns++;
            } else play(S_TESLA_ZAP, 0.7f * atten(ps), 0.55f);
            netlog("tesla : decharge renvoyee par %s vers %s", pname(def), pname(sh));
        }
        break;                                            /* --- fin Tesla --- */
    case EV_INFO:
        if (ev->sub && ev->target < MAXP) saw_remove_owner(ev->target);   /* depart : ses scies disparaissent */
        if (ev->sub && ev->target < MAXP) mine_remove_owner(ev->target);  /* Ventouse : ses mines aussi (celles sur lui retombent) */
        kfeed_add(-1, ev->target, 0, ev->sub ? 2 : 1);
        netlog("%s %s", pname(ev->target), ev->sub ? "a quitte la partie" : "a rejoint la partie");
        break;
    }
}
