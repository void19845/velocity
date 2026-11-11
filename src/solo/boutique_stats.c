/* VELOCITY - mode VAGUES : SAC DE STATS de la boutique (une valeur effective par graine et par stat)
   Morceau de main.c : inclus apres ameliorations.c, ne se compile pas seul (voir main.c).

   Chaque etape-stat de la boutique v2 est une ligne de donnees (StatMod, table shMods[] de boutique_donnees.c, generee par
   docs/boutique_vagues/outils/export_c.js depuis effets/*.js) : "cette etape, pour cette stat, fait x1,15 / +2 / plancher 20 (sous cette
   condition)". Le code d'une arme ne lit plus un accesseur par etape : il demande la valeur effective de la stat,
       taille = st(SH_SHO, STAT_TAI, 1.0f);      degats = st_ctx(SH_RAIL, STAT_DEGC, 1.0f, &contexte);      coups = st_int(SH_IMP, STAT_COM, 3);
   UNE fois par tir, dans une variable locale. Ajouter une etape-stat = une entree dans effets/<famille>.js, zero ligne de C.
   Les enums (SH_*, STAT_*, SOP_*, COND_*), StatMod et StatCtx sont dans monde/entites.c (les armes, compilees avant, lisent le sac).

   Regles :
   - Hors VAGUES, ou graine non equipee : st() rend la base telle quelle (PvP et LAN ne changent pas).
   - NIVEAU : le delta de chaque modificateur est multiplie par l'echelle sh_scale(graine, etape) = 1 + 0,2 x (niveau - 1) :
         mul, mull : facteur = 1 + (val - 1) x echelle   (un gain monte ; une penalite comme x0,9 devient x0,86 au niveau 3)
         add, max, min : val x echelle                   (le plancher, le plafond ou l'ajout eux-memes montent)
         to : cible val, echelle k : v = base - (base - val) x k   (le niveau pousse AU-DELA de la cible, comme vgm_lower)
     sauf SMF_FIXED (echelle 1 : entier fixe ou plafond absolu ; l'etape n'est alors pas proposee en amelioration, voir sh_scalable).
   - ORDRE : les modificateurs s'appliquent PHASE par PHASE (phase 0, puis 1...). Dans une phase : to (le meilleur des candidats, relatif a la
     valeur entrante), puis (valeur + somme des add) x produit des mul, puis les planchers (max), puis les plafonds (min). Sans phase (0), c'est
     le cas courant : (base + add) x mul. Une phase 1 permet "d'abord le pourcentage, ensuite le retrait en secondes" (Impulsion : Cadence x0,9
     puis Combo enchaine -0,2 s) ou "abaisser jusqu'a la cible, ensuite multiplier" (to puis mul).
   - GROUPE (grp > 0) : parmi les modificateurs APPLICABLES (condition vraie) d'une meme stat et d'un meme groupe, seul le plus fort compte (les
     paliers d'une meme idee ne s'additionnent pas : Plombs chercheurs / traqueurs ; Prime de ricochet REMPLACE Piece polie au 1er rebond mais
     pas aux suivants). L'op to n'a pas de groupe : elle garde deja le meilleur candidat.
   - CONDITIONS (StatCtx ; sans contexte, ou champ a 0, un modificateur conditionnel ne compte pas) : near (dist sous par m), head, air, elite,
     pierced (une application par cible deja traversee, par au plus), direct (cible touchee en plein), bounce (a partir du rebond par, une
     application par rebond ; rebond 1 = premier), first (le 1er rebond), salvefull (salve lancee chargeur plein).
   - L'arrondi, les garde-fous (planchers de securite, bornes) et les probabilites restent dans le code qui lit : st_int arrondit au plus proche.
   - Le sac est un cache : sh_rebuild() le recalcule (achat, montee de niveau, changement d'arme, reset), et st() compare l'etat de la
     boutique (etapes, niveaux, capstones, fusion, loadout) a celui du dernier calcul pour recalculer seul s'il a change : un test ou un
     code qui ecrit shOwn / shLvl directement reste juste. */

typedef struct { unsigned char op, cond, grp, phase; float v, par, k; } StAct;   /* modificateur actif : v a l'echelle du niveau, k = echelle (op to) */
#define ST_ACT_MAX 240                                    /* modificateurs actifs au plus (la table entiere : SH_NMOD) ; stFirst tient sur un octet */
#define ST_PER_STAT 16                                    /* modificateurs d'une meme stat pour une graine, au plus (export_c.js le verifie) */
typedef char st_chk_table[(SH_NMOD <= ST_ACT_MAX) ? 1 : -1];   /* garde de compilation : la table tient dans le sac */

static StAct stAct[ST_ACT_MAX];
static unsigned char stFirst[SH_N][STAT_COUNT], stCnt[SH_N][STAT_COUNT];   /* tranche de stAct de chaque (graine, stat) */

/* etat de la boutique au dernier calcul : s'il differe de l'etat courant, st() recalcule (recalcul paresseux) */
static struct {
    int valid, fusion, equip[4];
    unsigned int own[SH_N];
    unsigned char lvl[SH_N][20], cap[SH_NBR];
} stSnap;
static void st_snap_take(void) {
    stSnap.valid = 1; stSnap.fusion = shFusion;
    stSnap.equip[0] = P.loadout[0]; stSnap.equip[1] = P.loadout[1]; stSnap.equip[2] = P.meleeKind; stSnap.equip[3] = P.skill;
    memcpy(stSnap.own, shOwn, sizeof shOwn); memcpy(stSnap.lvl, shLvl, sizeof shLvl); memcpy(stSnap.cap, shCap, sizeof shCap);
}
static int st_snap_same(void) {
    return stSnap.valid && stSnap.fusion == shFusion &&
           stSnap.equip[0] == P.loadout[0] && stSnap.equip[1] == P.loadout[1] && stSnap.equip[2] == P.meleeKind && stSnap.equip[3] == P.skill &&
           !memcmp(stSnap.own, shOwn, sizeof shOwn) && !memcmp(stSnap.lvl, shLvl, sizeof shLvl) && !memcmp(stSnap.cap, shCap, sizeof shCap);
}

/* une ligne de donnees mise a l'echelle du niveau : le DELTA du modificateur est multiplie par l'echelle (SMF_FIXED : jamais) */
static StAct st_scaled(const StatMod *m, float scale) {
    float s = (m->flags & SMF_FIXED) ? 1.0f : scale;
    StAct a;
    a.op = m->op; a.cond = m->cond; a.grp = m->grp; a.phase = m->phase; a.par = m->par; a.k = s;
    if (m->op == SOP_MUL || m->op == SOP_MULL) a.v = 1.0f + (m->val - 1.0f) * s;
    else if (m->op == SOP_TO) a.v = m->val;               /* la cible ; l'echelle est dans k */
    else a.v = m->val * s;
    return a;
}
static float st_mag(const StAct *a) { return (a->op == SOP_MUL || a->op == SOP_MULL) ? fabsf(a->v - 1.0f) : fabsf(a->v); }   /* force d'un modificateur (groupes) */
/* modificateur actif construit a la main, exactement comme sh_rebuild le ferait pour la ligne { op, cond, val, par, grp, phase, fixed } a l'echelle
   donnee : pour les tests unitaires de st_fold (modeles : lignes "sac de stats" de src/tests/vagues_tests.c) */
static StAct st_act(int op, int cond, float val, float par, int grp, int phase, int fixed, float scale) {
    StatMod m = { 0, 0, 0, (unsigned char)op, (unsigned char)cond, (unsigned char)grp, (unsigned char)(fixed ? SMF_FIXED : 0), (unsigned char)phase, val, par };
    return st_scaled(&m, scale);
}

/* recalcule le sac : pour chaque graine equipee et chaque stat, les modificateurs des etapes achetees, a l'echelle de leur niveau */
static void sh_rebuild(void) {
    int n = 0;
    memset(stCnt, 0, sizeof stCnt); memset(stFirst, 0, sizeof stFirst);
    for (int sd = 0; sd < SH_N; sd++) {
        if (!sh_on(sd)) continue;                         /* graine non equipee : st() rend la base */
        for (int stat = 0; stat < STAT_COUNT; stat++) {
            int nt = 0;
            stFirst[sd][stat] = (unsigned char)n;
            for (int i = 0; i < SH_NMOD && nt < ST_PER_STAT; i++) {
                const StatMod *m = &shMods[i];
                if (m->seed != sd || m->stat != stat || !sh_owns(sd, m->step)) continue;
                stAct[n++] = st_scaled(m, sh_scale(sd, m->step)); nt++;
            }
            stCnt[sd][stat] = (unsigned char)nt;
        }
    }
    st_snap_take();
}

/* nombre d'applications d'un modificateur dans ce contexte : 0 = condition fausse, 1 = vraie, N = condition repetee (cibles traversees, rebonds) */
static int st_reps(const StAct *a, const StatCtx *c) {
    switch (a->cond) {
    case COND_NONE: return 1;
    case COND_NEAR: return c && c->dist < a->par;         /* distance de la cible strictement sous par (m) */
    case COND_HEAD: return c && c->head != 0;
    case COND_AIR: return c && c->air != 0;
    case COND_ELITE: return c && c->elite != 0;
    case COND_PIERCED: {                                  /* une application par cible deja traversee, par (> 0) au plus */
        int n = c ? c->pierced : 0;
        return a->par > 0 && n > (int)a->par ? (int)a->par : n;
    }
    case COND_DIRECT: return c && c->direct != 0;         /* la cible est celle de l'impact direct */
    case COND_BOUNCE: {                                   /* a partir du rebond par (>= 1) : une application par rebond (1 = premier rebond) */
        int n = c ? c->bounce : 0, from = (int)a->par;
        return from >= 1 && n >= from ? n - from + 1 : 0;
    }
    case COND_FIRST: return c && c->bounce == 1;          /* le premier rebond seulement */
    case COND_SALVEFULL: return c && c->full != 0;        /* salve lancee chargeur plein */
    }
    return 0;
}
/* applique n modificateurs actifs a une base, phase par phase (voir l'en-tete) */
static float st_fold(const StAct *a, int n, float base, const StatCtx *c) {
    int r[ST_PER_STAT], maxPh = 0;
    unsigned char lose[ST_PER_STAT];
    if (n > ST_PER_STAT) n = ST_PER_STAT;
    for (int k = 0; k < n; k++) { r[k] = st_reps(&a[k], c); lose[k] = 0; if (a[k].phase > maxPh) maxPh = a[k].phase; }
    for (int k = 0; k < n; k++) {                         /* groupe : parmi les modificateurs APPLICABLES, seul le plus fort (a egalite, le premier) */
        if (r[k] <= 0 || !a[k].grp || a[k].op == SOP_TO) continue;
        for (int j = 0; j < n; j++) {
            if (j == k || r[j] <= 0 || a[j].grp != a[k].grp || a[j].op == SOP_TO) continue;
            float mj = st_mag(&a[j]), mk = st_mag(&a[k]);
            if (mj > mk || (mj == mk && j < k)) { lose[k] = 1; break; }
        }
    }
    float v = base;
    for (int ph = 0; ph <= maxPh; ph++) {
        float add = 0, mul = 1, lo = 0, hi = 0, to = 0;
        int hasLo = 0, hasHi = 0, hasTo = 0;
        for (int k = 0; k < n; k++) {
            if (a[k].phase != ph || r[k] <= 0 || lose[k]) continue;
            switch (a[k].op) {
            case SOP_MUL: mul *= r[k] == 1 ? a[k].v : powf(a[k].v, (float)r[k]); break;     /* chaque application compose le facteur */
            case SOP_MULL: mul *= 1.0f + (a[k].v - 1.0f) * (float)r[k]; break;              /* les deltas s'additionnent : 1 + delta x N */
            case SOP_ADD: add += a[k].v * (float)r[k]; break;
            case SOP_MAX: lo = hasLo ? fmaxf(lo, a[k].v) : a[k].v; hasLo = 1; break;
            case SOP_MIN: hi = hasHi ? fminf(hi, a[k].v) : a[k].v; hasHi = 1; break;
            case SOP_TO:                                  /* vers la cible (ignoree si elle n'abaisse pas la valeur) ; le meilleur candidat */
                if (a[k].v < v) {
                    float cand = v - (v - a[k].v) * a[k].k;
                    if (!hasTo || cand < to) { to = cand; hasTo = 1; }
                }
                break;
            }
        }
        if (hasTo) v = to;
        v = (v + add) * mul;
        if (hasLo && v < lo) v = lo;
        if (hasHi && v > hi) v = hi;
    }
    return v;
}

static float st_ctx(int seed, int stat, float base, const StatCtx *c) {
    if (!vagOn || seed < 0 || seed >= SH_N || stat < 0 || stat >= STAT_COUNT) return base;
    if (!st_snap_same()) sh_rebuild();                    /* l'etat de la boutique a change depuis le dernier calcul */
    int n = stCnt[seed][stat];
    return n ? st_fold(&stAct[stFirst[seed][stat]], n, base, c) : base;
}
static float st(int seed, int stat, float base) { return st_ctx(seed, stat, base, NULL); }
static int st_int_ctx(int seed, int stat, int base, const StatCtx *c) { return (int)floorf(st_ctx(seed, stat, (float)base, c) + 0.5f); }   /* entier le plus proche */
static int st_int(int seed, int stat, int base) { return st_int_ctx(seed, stat, base, NULL); }

/* rang de la premiere etape de la graine qui modifie cette stat (-1 : aucune) : pour les tests */
static int st_find_step(int seed, int stat) {
    for (int i = 0; i < SH_NMOD; i++) if (shMods[i].seed == seed && shMods[i].stat == stat) return shMods[i].step;
    return -1;
}
/* coherence de la table generee (test --test, ligne "table") : rend le nombre d'anomalies, 0 si tout va bien. Verifie les indices, les enums, le nombre
   de modificateurs par (graine, stat), et que chaque ligne appartient a exactement une etape BK_STAT qui la designe (a = premiere ligne, b = nombre) */
static int st_table_check(void) {
    int bad = 0, per[SH_N][STAT_COUNT];
    unsigned char owner[SH_NMOD];
    memset(per, 0, sizeof per); memset(owner, 0, sizeof owner);
    for (int i = 0; i < SH_NMOD; i++) {
        const StatMod *m = &shMods[i];
        if (m->seed >= SH_N || m->step >= 20 || m->stat >= STAT_COUNT || m->op >= SOP_COUNT || m->cond >= COND_COUNT || m->phase > 3) { bad++; continue; }
        if (++per[m->seed][m->stat] > ST_PER_STAT) bad++;
        if (shSteps[m->seed][m->step].kind != BK_STAT) bad++;
    }
    for (int s = 0; s < SH_N; s++)
        for (int k = 0; k < 20; k++) {
            const ShStep *t = &shSteps[s][k];
            if (t->kind != BK_STAT) continue;
            if (t->b < 1 || t->a < 0 || t->a + t->b > SH_NMOD) { bad++; continue; }
            for (int i = t->a; i < t->a + t->b; i++) {
                if (shMods[i].seed != s || shMods[i].step != k) bad++;
                owner[i]++;
            }
        }
    for (int i = 0; i < SH_NMOD; i++) if (owner[i] != 1) bad++;
    return bad;
}
