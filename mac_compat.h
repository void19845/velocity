/*
 * VELOCITY - compatibilite macOS pour main.c : quelques types / fonctions Win32
 * reimplementes (sockets, fils, evenements, horloge, en-tetes BMP) pour que le
 * code partage ne change pas. Tout ce qui touche a la fenetre, au son, a la
 * police et au lobby est dans mac_platform.m.
 */
#ifndef MAC_COMPAT_H
#define MAC_COMPAT_H

#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "mac_platform.h"

/* --- types Win32 --- */
typedef int32_t LONG;
typedef uint32_t DWORD;
typedef void *LPVOID;
#define WINAPI
#define MAX_PATH 1024
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

/* --- sockets --- */
typedef int SOCKET;
typedef socklen_t sock_len_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define closesocket close
#define WSAGetLastError() errno
#define WSAECONNRESET ECONNRESET
#define WSAEMSGSIZE EMSGSIZE

/* --- horloge --- */
typedef struct {
  int64_t QuadPart;
} LARGE_INTEGER;
static inline void QueryPerformanceFrequency(LARGE_INTEGER *f) {
  f->QuadPart = 1000000000LL;
}
static inline void QueryPerformanceCounter(LARGE_INTEGER *c) {
  c->QuadPart = (int64_t)clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
}
static inline DWORD GetTickCount(void) {
  return (DWORD)(clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 1000000ULL);
}

/* --- verrous, evenements et fils (semantique Win32 : section critique
 * recursive, evenement auto-reset) --- */
typedef pthread_mutex_t CRITICAL_SECTION;
static inline void InitializeCriticalSection(CRITICAL_SECTION *cs) {
  pthread_mutexattr_t a;
  pthread_mutexattr_init(&a);
  pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(cs, &a);
  pthread_mutexattr_destroy(&a);
}
#define EnterCriticalSection(cs) pthread_mutex_lock(cs)
#define LeaveCriticalSection(cs) pthread_mutex_unlock(cs)

typedef struct MacHandle {
  int isThread, signaled, done, refs;
  pthread_mutex_t m;
  pthread_cond_t c;
  DWORD (*fn)(LPVOID);
  LPVOID arg;
} *HANDLE;
#define WAIT_OBJECT_0 0
#define WAIT_TIMEOUT 258
#define INFINITE 0xFFFFFFFFu

static inline HANDLE mac_handle_new(int isThread) {
  HANDLE h = (HANDLE)calloc(1, sizeof *h);
  h->isThread = isThread;
  h->refs = 1;
  pthread_mutex_init(&h->m, NULL);
  pthread_cond_init(&h->c, NULL);
  return h;
}
static inline void mac_handle_unref(HANDLE h) {
  if (__sync_sub_and_fetch(&h->refs, 1) == 0) {
    pthread_mutex_destroy(&h->m);
    pthread_cond_destroy(&h->c);
    free(h);
  }
}
static inline HANDLE CreateEventA(void *attr, int manual, int initial,
                                  const char *name) {
  (void)attr;
  (void)manual;
  (void)name;
  HANDLE h = mac_handle_new(0);
  h->signaled = initial;
  return h;
}
static inline int SetEvent(HANDLE h) {
  pthread_mutex_lock(&h->m);
  h->signaled = 1;
  pthread_cond_broadcast(&h->c);
  pthread_mutex_unlock(&h->m);
  return 1;
}
static inline DWORD WaitForSingleObject(HANDLE h, DWORD ms) {
  struct timespec ts;
  DWORD r = WAIT_OBJECT_0;
  if (ms != INFINITE) {
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (long)(ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
      ts.tv_sec++;
      ts.tv_nsec -= 1000000000L;
    }
  }
  pthread_mutex_lock(&h->m);
  while (!(h->isThread ? h->done : h->signaled)) {
    if (ms == INFINITE)
      pthread_cond_wait(&h->c, &h->m);
    else if (pthread_cond_timedwait(&h->c, &h->m, &ts) != 0) {
      r = WAIT_TIMEOUT;
      break;
    }
  }
  if (r == WAIT_OBJECT_0 && !h->isThread)
    h->signaled = 0; /* auto-reset */
  pthread_mutex_unlock(&h->m);
  return r;
}
static void *mac_thread_main(void *p) {
  HANDLE h = (HANDLE)p;
  h->fn(h->arg);
  pthread_mutex_lock(&h->m);
  h->done = 1;
  pthread_cond_broadcast(&h->c);
  pthread_mutex_unlock(&h->m);
  mac_handle_unref(h);
  return NULL;
}
static inline HANDLE CreateThread(void *attr, size_t stack, DWORD (*fn)(LPVOID),
                                  LPVOID arg, DWORD flags, DWORD *id) {
  (void)attr;
  (void)stack;
  (void)flags;
  (void)id;
  pthread_t t;
  HANDLE h = mac_handle_new(1);
  h->fn = fn;
  h->arg = arg;
  h->refs = 2; /* une reference pour l'appelant, une pour le fil */
  if (pthread_create(&t, NULL, mac_thread_main, h) != 0) {
    mac_handle_unref(h);
    mac_handle_unref(h);
    return NULL;
  }
  pthread_detach(t);
  return h;
}
#define CloseHandle(h) mac_handle_unref(h)

#define PostQuitMessage(code) mac_quit()

/* --- en-tetes BMP (save_bmp) --- */
#pragma pack(push, 2)
typedef struct {
  uint16_t bfType;
  uint32_t bfSize;
  uint16_t bfReserved1, bfReserved2;
  uint32_t bfOffBits;
} BITMAPFILEHEADER;
#pragma pack(pop)
typedef struct {
  uint32_t biSize;
  int32_t biWidth, biHeight;
  uint16_t biPlanes, biBitCount;
  uint32_t biCompression, biSizeImage;
  int32_t biXPelsPerMeter, biYPelsPerMeter;
  uint32_t biClrUsed, biClrImportant;
} BITMAPINFOHEADER;

#endif
