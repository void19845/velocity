/* VELOCITY - mise a jour depuis GitHub (branche release) : verification, telechargement, installation
   Morceau de main.c : inclus dans l'ordre, ne se compile pas seul (voir main.c). */

/* ------------------------------------------------------------------ */
/* Mise a jour : derniere version publiee dans la branche `release`    */
/* de GitHub (binaires construits par la CI depuis `stable`).          */
/* version.txt donne le numero de build, la taille et le SHA-256 de    */
/* chaque binaire ; le telechargement se fait dans un fil, puis on     */
/* remplace l'executable (renommage) et on relance le jeu.             */
/* ------------------------------------------------------------------ */
#ifndef VELOCITY_BUILD
#define VELOCITY_BUILD 0                                  /* compilation locale ; la CI de release passe son numero */
#endif
#ifndef UPD_REPO
#define UPD_REPO "void19845/velocity"                     /* depot GitHub dont la branche release fournit les mises a jour */
#endif
#define UPD_HOST "raw.githubusercontent.com"
#define UPD_DIR  "/" UPD_REPO "/release/"
#define UPD_PAGE "https://github.com/" UPD_REPO "/tree/release"
#ifdef _WIN32
#define UPD_BIN "velocity.exe"
#else
#define UPD_BIN "Velocity-macOS.zip"
#endif

enum { UPD_IDLE, UPD_CHECKING, UPD_UPTODATE, UPD_AVAILABLE, UPD_DOWNLOADING, UPD_READY, UPD_ERROR };
typedef struct { int build; char sha[16]; long winSize, macSize; char winHash[65], macHash[65]; } UpdInfo;
static volatile LONG updState, updGot, updTotal;          /* ecrits par le fil, lus par le menu */
static CRITICAL_SECTION updCs;
static int updCsReady, updSeen = UPD_IDLE, updCheck = 1, updAfter, updPage;   /* updCheck : reglage ; updAfter : --updated */
static int updQuiet;                                      /* verification automatique du demarrage : un echec ne dit rien */
static UpdInfo updInfo;
static char updErr[96], updFile[MAX_PATH];
static HANDLE updTh;

/* --- SHA-256 (FIPS 180-4), pour verifier le fichier telecharge --- */
typedef struct { unsigned int h[8]; unsigned char buf[64]; unsigned long long len; int n; } Sha256;
static const unsigned int shaK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};
#define SHA_ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(Sha256 *s, const unsigned char *p) {
    unsigned int w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; i++) w[i] = (unsigned)p[i * 4] << 24 | (unsigned)p[i * 4 + 1] << 16 | (unsigned)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (int i = 16; i < 64; i++) {
        unsigned int s0 = SHA_ROR(w[i - 15], 7) ^ SHA_ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        unsigned int s1 = SHA_ROR(w[i - 2], 17) ^ SHA_ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (int i = 0; i < 64; i++) {
        unsigned int t1 = h + (SHA_ROR(e, 6) ^ SHA_ROR(e, 11) ^ SHA_ROR(e, 25)) + ((e & f) ^ (~e & g)) + shaK[i] + w[i];
        unsigned int t2 = (SHA_ROR(a, 2) ^ SHA_ROR(a, 13) ^ SHA_ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
static void sha_init(Sha256 *s) {
    static const unsigned int h0[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    memcpy(s->h, h0, sizeof h0); s->len = 0; s->n = 0;
}
static void sha_update(Sha256 *s, const void *data, size_t n) {
    const unsigned char *p = (const unsigned char *)data;
    s->len += n;
    while (n--) {
        s->buf[s->n++] = *p++;
        if (s->n == 64) { sha_block(s, s->buf); s->n = 0; }
    }
}
static void sha_final(Sha256 *s, char hex[65]) {           /* empreinte en hexadecimal minuscule */
    unsigned long long bits = s->len * 8;
    unsigned char pad = 0x80, z = 0, l[8];
    sha_update(s, &pad, 1);
    while (s->n != 56) sha_update(s, &z, 1);
    for (int i = 0; i < 8; i++) l[i] = (unsigned char)(bits >> (56 - 8 * i));
    sha_update(s, l, 8);
    for (int i = 0; i < 8; i++) snprintf(hex + i * 8, 9, "%08x", s->h[i]);
}
static int sha256_file(const char *path, char hex[65], long *size) {
    unsigned char buf[16384];
    size_t got;
    Sha256 s;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    sha_init(&s); *size = 0;
    while ((got = fread(buf, 1, sizeof buf, f)) > 0) { sha_update(&s, buf, got); *size += (long)got; }
    fclose(f);
    sha_final(&s, hex);
    return 1;
}

/* version.txt : lignes cle=valeur (build, sha, win_size, win_sha256, mac_size, mac_sha256). 1 si un build est lu */
static int upd_parse(const char *txt, UpdInfo *o) {
    memset(o, 0, sizeof *o);
    for (const char *p = txt; *p;) {
        char line[160], *v;
        int n = (int)strcspn(p, "\r" "\n");
        snprintf(line, sizeof line, "%.*s", n < 159 ? n : 159, p);
        p += n;
        while (*p == '\r' || *p == '\n') p++;
        if (!(v = strchr(line, '='))) continue;
        *v++ = 0;
        for (char *c = v; *c; c++) *c = (char)tolower((unsigned char)*c);
        if (!strcmp(line, "build")) o->build = atoi(v);
        else if (!strcmp(line, "sha")) snprintf(o->sha, sizeof o->sha, "%s", v);
        else if (!strcmp(line, "win_size")) o->winSize = atol(v);
        else if (!strcmp(line, "mac_size")) o->macSize = atol(v);
        else if (!strcmp(line, "win_sha256")) snprintf(o->winHash, sizeof o->winHash, "%s", v);
        else if (!strcmp(line, "mac_sha256")) snprintf(o->macHash, sizeof o->macHash, "%s", v);
    }
    return o->build > 0;
}

/* fichiers voisins de l'executable (Windows) ou du .app (macOS) : nouvelle version et ancienne */
static void upd_paths(const char *base, char *nw, char *old, int n) {
    snprintf(nw, n, "%s.new", base);
    snprintf(old, n, "%s.old", base);
}

static void upd_cs(void) {
    if (!updCsReady) { InitializeCriticalSection(&updCs); updCsReady = 1; }
}

static void upd_fail(const char *msg, int page) {         /* page : proposer la page GitHub (dossier protege) */
    upd_cs();
    EnterCriticalSection(&updCs);
    snprintf(updErr, sizeof updErr, "mise a jour : %s", msg);
    updPage = page;
    LeaveCriticalSection(&updCs);
    updState = UPD_ERROR;
}

#ifndef _WIN32
static int upd_spawn(char *const argv[], pid_t *pid) {
    extern char **environ;
    return posix_spawn(pid, argv[0], NULL, NULL, argv, environ) == 0;
}
static int upd_run(char *const argv[]) {                  /* lance et attend ; 1 si le code de sortie est 0 */
    pid_t pid;
    int st = 0;
    if (!upd_spawn(argv, &pid) || waitpid(pid, &st, 0) < 0) return 0;
    return WIFEXITED(st) && WEXITSTATUS(st) == 0;
}
static void upd_rmrf(const char *path) {
    char *argv[] = { "/bin/rm", "-rf", (char *)path, NULL };
    upd_run(argv);
}
static int upd_bundle(char *out, int n) {                 /* chemin du .app qui contient l'executable */
    char exe[MAX_PATH], real[PATH_MAX];
    uint32_t len = sizeof exe;
    if (_NSGetExecutablePath(exe, &len) != 0 || !realpath(exe, real)) return 0;
    char *c = strstr(real, ".app/Contents/MacOS/");
    if (!c) return 0;
    c[4] = 0;
    snprintf(out, n, "%s", real);
    return 1;
}
#endif

/* GET https://UPD_HOST<path>, vers le fichier outPath (avec progression updGot) ou en memoire (mem, cap octets).
 * 1 si la reponse est un HTTP 200 complet, -1 si le serveur repond autre chose (404...), 0 si pas de reponse */
static int upd_http_get(const char *path, const char *outPath, char *mem, int cap) {
#ifdef _WIN32
    wchar_t wpath[256];
    HINTERNET ses, con = NULL, req = NULL;
    FILE *out = NULL;
    int ok = 0, n = 0;
    MultiByteToWideChar(CP_ACP, 0, path, -1, wpath, 256);
    ses = WinHttpOpen(L"VELOCITY", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (ses) {
        WinHttpSetTimeouts(ses, 5000, 5000, 10000, 60000);   /* 60 s sans un octet : abandon */
        con = WinHttpConnect(ses, L"" UPD_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    }
    if (con) req = WinHttpOpenRequest(con, L"GET", wpath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, NULL)) {
        DWORD code = 0, cl = sizeof code;
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &cl,
                            WINHTTP_NO_HEADER_INDEX);
        if (code != 200) ok = -1;
        else if (!outPath || (out = fopen(outPath, "wb")) != NULL) {
            ok = 1;
            for (;;) {
                char buf[16384];
                DWORD got = 0;
                if (!WinHttpReadData(req, buf, sizeof buf, &got)) { ok = 0; break; }
                if (!got) break;
                if (out) {
                    if (fwrite(buf, 1, got, out) != got) { ok = 0; break; }
                    updGot += (LONG)got;
                } else {
                    if (n + (int)got >= cap) { ok = 0; break; }
                    memcpy(mem + n, buf, got); n += (int)got;
                }
            }
            if (out) fclose(out);
        }
    }
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    if (ses) WinHttpCloseHandle(ses);
    if (mem && cap > 0) mem[n] = 0;
    return ok;
#else                                                     /* macOS : curl, fourni avec le systeme */
    char url[320], tmp[MAX_PATH];
    const char *dst = outPath;
    pid_t pid;
    int st = 0, ok;
    snprintf(url, sizeof url, "https://" UPD_HOST "%s", path);
    if (!dst) { snprintf(tmp, sizeof tmp, "/tmp/velocity-%d.txt", (int)getpid()); dst = tmp; }
    char *argv[] = { "/usr/bin/curl", "-fsSL", "--connect-timeout", "10", "--speed-time", "60", "--speed-limit", "1",
                     "-o", (char *)dst, url, NULL };
    if (!upd_spawn(argv, &pid)) return 0;
    while (waitpid(pid, &st, WNOHANG) == 0) {             /* progression : taille du fichier en cours d'ecriture */
        struct stat sb;
        if (outPath && stat(outPath, &sb) == 0) updGot = (LONG)sb.st_size;
        usleep(100000);
    }
    ok = !WIFEXITED(st) ? 0 : WEXITSTATUS(st) == 0 ? 1 : WEXITSTATUS(st) == 22 ? -1 : 0;   /* 22 : erreur HTTP (-f) */
    if (!outPath) {
        FILE *f = fopen(tmp, "rb");
        int n = 0;
        if (f) { n = (int)fread(mem, 1, (size_t)(cap - 1), f); fclose(f); }
        else if (ok > 0) ok = 0;
        mem[n] = 0;
        remove(tmp);
    }
    return ok;
#endif
}

static DWORD WINAPI upd_check_th(LPVOID arg) {
    char txt[1024], path[96];
    UpdInfo in;
    (void)arg;
    snprintf(path, sizeof path, UPD_DIR "version.txt?t=%u", (unsigned)GetTickCount());   /* evite une copie en cache */
    int r = upd_http_get(path, NULL, txt, sizeof txt);
    if (r < 0) { upd_fail("aucune version publiee sur GitHub pour l'instant", 0); return 0; }
    if (r == 0) { upd_fail("GitHub ne repond pas (connexion ?)", 0); return 0; }
    if (!upd_parse(txt, &in)) { upd_fail("version.txt illisible", 0); return 0; }
    EnterCriticalSection(&updCs);
    updInfo = in;
    LeaveCriticalSection(&updCs);
    updState = in.build > VELOCITY_BUILD ? UPD_AVAILABLE : UPD_UPTODATE;
    return 0;
}

static DWORD WINAPI upd_download_th(LPVOID arg) {
    UpdInfo in;
    char hex[65];
    long size = 0, want;
    const char *hash;
    (void)arg;
    EnterCriticalSection(&updCs);
    in = updInfo;
    LeaveCriticalSection(&updCs);
#ifdef _WIN32
    want = in.winSize; hash = in.winHash;
#else
    want = in.macSize; hash = in.macHash;
#endif
    if (want <= 0 || strlen(hash) != 64) { upd_fail("pas de binaire pour ce systeme dans cette version", 0); return 0; }
    updGot = 0; updTotal = (LONG)want;
    FILE *t = fopen(updFile, "wb");                       /* dossier protege (Program Files...) : page de telechargement */
    if (!t) { upd_fail("impossible d'ecrire dans le dossier du jeu", 1); return 0; }
    fclose(t);
    if (upd_http_get(UPD_DIR UPD_BIN, updFile, NULL, 0) <= 0) {
        remove(updFile);
        upd_fail("telechargement interrompu", 0);
        return 0;
    }
    if (!sha256_file(updFile, hex, &size) || size != want || strcmp(hex, hash)) {
        remove(updFile);
        upd_fail("fichier corrompu (empreinte differente)", 0);
        return 0;
    }
    updState = UPD_READY;
    return 0;
}

static int upd_busy(void) {                               /* un fil de mise a jour tourne encore */
    if (!updTh) return 0;
    if (WaitForSingleObject(updTh, 0) == WAIT_TIMEOUT) return 1;
    CloseHandle(updTh);
    updTh = NULL;
    return 0;
}

static void upd_start(DWORD (WINAPI *fn)(LPVOID), int state) {
    upd_cs();
    if (upd_busy()) return;
    updState = state;
    updTh = CreateThread(NULL, 0, fn, NULL, 0, NULL);
    if (!updTh) upd_fail("impossible de lancer le telechargement", 0);
}

static void open_url(const char *url) {                  /* ouvre une page dans le navigateur du systeme */
#ifdef _WIN32
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
#else
    char *argv[] = { "/usr/bin/open", (char *)url, NULL };
    pid_t pid;
    if (upd_spawn(argv, &pid)) waitpid(pid, NULL, 0);
#endif
}
static void upd_open_page(void) { open_url(UPD_PAGE); }

/* remplace le jeu par la version telechargee et la relance (appele sur le fil principal) */
static void upd_install(void) {
    char nw[MAX_PATH], old[MAX_PATH];
#ifdef _WIN32
    char exe[MAX_PATH], dir[MAX_PATH], cmdl[MAX_PATH + 16], *slash;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    GetModuleFileNameA(NULL, exe, sizeof exe);
    upd_paths(exe, nw, old, MAX_PATH);
    DeleteFileA(old);
    if (!MoveFileExA(exe, old, MOVEFILE_REPLACE_EXISTING)) { upd_fail("impossible de remplacer velocity.exe", 1); return; }
    if (!MoveFileExA(nw, exe, MOVEFILE_REPLACE_EXISTING)) {   /* jamais d'executable manquant : on remet l'ancien */
        MoveFileExA(old, exe, MOVEFILE_REPLACE_EXISTING);
        upd_fail("impossible d'installer la nouvelle version", 1);
        return;
    }
    snprintf(dir, sizeof dir, "%s", exe);
    if ((slash = strrchr(dir, '\\')) != NULL) *slash = 0;
    snprintf(cmdl, sizeof cmdl, "\"%s\" --updated", exe);
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    cfg_save();
    if (CreateProcessA(exe, cmdl, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        PostQuitMessage(0);
    } else {
        set_status("mise a jour installee : relance VELOCITY");
        updState = UPD_UPTODATE;
    }
#else
    char app[MAX_PATH], tmp[MAX_PATH], got[MAX_PATH + 16];
    struct stat sb;
    if (!upd_bundle(app, sizeof app)) { upd_fail("lance VELOCITY depuis Velocity.app pour le mettre a jour", 1); return; }
    upd_paths(app, nw, old, MAX_PATH);
    snprintf(tmp, sizeof tmp, "%s.upd", app);
    upd_rmrf(tmp);
    mkdir(tmp, 0755);
    char *unzip[] = { "/usr/bin/ditto", "-x", "-k", updFile, tmp, NULL };
    snprintf(got, sizeof got, "%s/Velocity.app", tmp);
    if (!upd_run(unzip) || stat(got, &sb) != 0) { upd_rmrf(tmp); upd_fail("archive illisible", 0); return; }
    upd_rmrf(old);
    if (rename(app, old) != 0) { upd_rmrf(tmp); upd_fail("impossible de remplacer Velocity.app", 1); return; }
    if (rename(got, app) != 0) { rename(old, app); upd_rmrf(tmp); upd_fail("impossible d'installer la nouvelle version", 1); return; }
    upd_rmrf(tmp);
    remove(updFile);
    cfg_save();
    char *relaunch[] = { "/usr/bin/open", "-n", app, "--args", "--updated", NULL };
    pid_t pid;
    if (upd_spawn(relaunch, &pid)) mac_quit();
    else { set_status("mise a jour installee : relance VELOCITY"); updState = UPD_UPTODATE; }
#endif
}

/* bouton du menu : verifier, telecharger, installer selon l'etat */
static void upd_click(void) {
    int st = updState;
    if (st == UPD_CHECKING || st == UPD_DOWNLOADING) return;
    updQuiet = 0;
    if (st == UPD_READY) { upd_install(); return; }
    if (st == UPD_ERROR && updPage) { updPage = 0; upd_open_page(); }
    if (st != UPD_AVAILABLE) { upd_start(upd_check_th, UPD_CHECKING); return; }
#ifdef _WIN32
    char exe[MAX_PATH], old[MAX_PATH];
    GetModuleFileNameA(NULL, exe, sizeof exe);
    upd_paths(exe, updFile, old, MAX_PATH);
#else
    char app[MAX_PATH];
    if (!upd_bundle(app, sizeof app)) { upd_fail("lance VELOCITY depuis Velocity.app pour le mettre a jour", 1); return; }
    snprintf(updFile, sizeof updFile, "%s.zip", app);
#endif
    if (VELOCITY_BUILD == 0) set_status("version de dev : elle sera remplacee par la derniere version stable");
    upd_start(upd_download_th, UPD_DOWNLOADING);
}

/* chaque image du menu : messages sur les changements d'etat du fil */
static void upd_poll(void) {
    int st = updState;
    char buf[96];
    if (st == updSeen) return;
    if (st == UPD_ERROR && updQuiet) { updQuiet = 0; updState = updSeen = UPD_IDLE; return; }
    if (st != UPD_CHECKING) updQuiet = 0;
    updSeen = st;
    upd_cs();
    if (st == UPD_ERROR) {
        EnterCriticalSection(&updCs);
        snprintf(buf, sizeof buf, "%s", updErr);
        LeaveCriticalSection(&updCs);
        set_status(buf);
    } else if (st == UPD_AVAILABLE) {
        snprintf(buf, sizeof buf, "nouvelle version disponible : build %d", updInfo.build);
        set_status(buf);
    }
}

static const char *upd_label(void) {
    static char buf[64];
    char mine[16];
    if (VELOCITY_BUILD) snprintf(mine, sizeof mine, "%d", VELOCITY_BUILD); else snprintf(mine, sizeof mine, "dev");
    switch (updState) {
    case UPD_CHECKING: return "VERIFICATION...";
    case UPD_UPTODATE: snprintf(buf, sizeof buf, "A JOUR (build %s)", mine); return buf;
    case UPD_AVAILABLE: snprintf(buf, sizeof buf, "METTRE A JOUR (%s -> %d)", mine, updInfo.build); return buf;
    case UPD_DOWNLOADING:
        snprintf(buf, sizeof buf, "TELECHARGEMENT %d %%", updTotal > 0 ? (int)(100.0 * updGot / updTotal) : 0);
        return buf;
    case UPD_READY: return "REDEMARRER POUR FINIR";
    case UPD_ERROR: return updPage ? "PAGE DE TELECHARGEMENT" : "REESSAYER LA MISE A JOUR";
    default: return "MISE A JOUR";
    }
}

/* au demarrage : restes d'une mise a jour precedente */
static void upd_cleanup(void) {
    char nw[MAX_PATH], old[MAX_PATH];
#ifdef _WIN32
    char exe[MAX_PATH];
    GetModuleFileNameA(NULL, exe, sizeof exe);
    upd_paths(exe, nw, old, MAX_PATH);
    DeleteFileA(old);                                     /* l'ancien exe, libere depuis que l'ancien processus est sorti */
    DeleteFileA(nw);
#else
    char app[MAX_PATH], tmp[MAX_PATH + 8];
    if (!upd_bundle(app, sizeof app)) return;
    upd_paths(app, nw, old, MAX_PATH);
    snprintf(tmp, sizeof tmp, "%s.upd", app);
    upd_rmrf(old); upd_rmrf(tmp);
    snprintf(tmp, sizeof tmp, "%s.zip", app);
    remove(tmp);
#endif
}
