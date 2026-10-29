/*
 * VELOCITY - couche plateforme macOS : fenetre Cocoa + OpenGL (legacy, pipeline fixe), entrees,
 * son (AudioQueue), police bitmap (CoreText) et WebSocket du lobby (NSURLSession).
 * Compile avec -fobjc-arc ; main.c reste du C pur et ne voit que mac_platform.h.
 */
#define GL_SILENCE_DEPRECATION
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>
#import <AudioToolbox/AudioToolbox.h>
#import <CoreText/CoreText.h>
#include <OpenGL/gl.h>
#include <stdatomic.h>
#include "mac_platform.h"

/* ------------------------------------------------------------------ */
/* Clavier : touche virtuelle macOS -> scancode Windows (set 1)         */
/* Le jeu stocke ses touches en scancodes Windows (velocity.ini, binds). */
/* ------------------------------------------------------------------ */
static const struct { int vk, sc; } keyMap[] = {
    { 0, 0x1E }, { 1, 0x1F }, { 2, 0x20 }, { 3, 0x21 }, { 4, 0x23 }, { 5, 0x22 }, { 6, 0x2C }, { 7, 0x2D },
    { 8, 0x2E }, { 9, 0x2F }, { 11, 0x30 }, { 12, 0x10 }, { 13, 0x11 }, { 14, 0x12 }, { 15, 0x13 }, { 16, 0x15 },
    { 17, 0x14 }, { 18, 0x02 }, { 19, 0x03 }, { 20, 0x04 }, { 21, 0x05 }, { 22, 0x07 }, { 23, 0x06 }, { 24, 0x0D },
    { 25, 0x0A }, { 26, 0x08 }, { 27, 0x0C }, { 28, 0x09 }, { 29, 0x0B }, { 30, 0x1B }, { 31, 0x18 }, { 32, 0x16 },
    { 33, 0x1A }, { 34, 0x17 }, { 35, 0x19 }, { 36, 0x1C }, { 37, 0x26 }, { 38, 0x24 }, { 39, 0x28 }, { 40, 0x25 },
    { 41, 0x27 }, { 42, 0x2B }, { 43, 0x33 }, { 44, 0x35 }, { 45, 0x31 }, { 46, 0x32 }, { 47, 0x34 }, { 48, 0x0F },
    { 49, 0x39 }, { 50, 0x29 }, { 51, 0x0E }, { 53, 0x01 },
    { 65, 0x53 }, { 67, 0x37 }, { 69, 0x4E }, { 71, 0x45 }, { 75, 0x35 }, { 76, 0x1C }, { 78, 0x4A }, { 81, 0x0D },
    { 82, 0x52 }, { 83, 0x4F }, { 84, 0x50 }, { 85, 0x51 }, { 86, 0x4B }, { 87, 0x4C }, { 88, 0x4D }, { 89, 0x47 },
    { 91, 0x48 }, { 92, 0x49 },
    { 96, 0x3F }, { 97, 0x40 }, { 98, 0x41 }, { 99, 0x3D }, { 100, 0x42 }, { 101, 0x43 }, { 103, 0x57 }, { 109, 0x44 },
    { 111, 0x58 }, { 114, 0x52 }, { 115, 0x47 }, { 116, 0x49 }, { 117, 0x53 }, { 118, 0x3E }, { 119, 0x4F },
    { 120, 0x3C }, { 121, 0x51 }, { 122, 0x3B },
    { 123, 0x4B }, { 124, 0x4D }, { 125, 0x50 }, { 126, 0x48 },
};
#define NKEYS ((int)(sizeof keyMap / sizeof keyMap[0]))

static int sc_of_vk(int vk) {
    for (int i = 0; i < NKEYS; i++) if (keyMap[i].vk == vk) return keyMap[i].sc;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Fenetre                                                             */
/* ------------------------------------------------------------------ */
@interface VWindow : NSWindow
@end
@implementation VWindow
- (BOOL)canBecomeKeyWindow { return YES; }                /* aussi en plein ecran sans bordure */
- (BOOL)canBecomeMainWindow { return YES; }
@end

@interface VView : NSView
@end
@implementation VView
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)e { (void)e; return YES; }
- (BOOL)isOpaque { return YES; }
@end

@interface VDelegate : NSObject <NSWindowDelegate>
- (void)requestQuit:(id)sender;
@end

static NSWindow *gWin;
static VView *gView;
static NSOpenGLContext *gCtx;
static VDelegate *gDel;
static int gQuit, gCaptured, gCursorHidden, gFull;
static NSRect gPrevFrame;
static NSWindowStyleMask gPrevStyle;
static NSInteger gPrevLevel;

static void content_size(int *w, int *h) {
    NSSize s = [gView bounds].size;
    *w = (int)s.width; *h = (int)s.height;
}

@implementation VDelegate
- (BOOL)windowShouldClose:(id)sender { (void)sender; gQuit = 1; return NO; }
- (void)requestQuit:(id)sender { (void)sender; gQuit = 1; }
- (void)windowDidResize:(NSNotification *)n {
    (void)n;
    if (!gView || !gCtx) return;                          /* pendant la creation de la fenetre */
    int w, h;
    content_size(&w, &h);
    [gCtx update];
    plat_resize(w, h);
}
- (void)windowDidResignKey:(NSNotification *)n { (void)n; plat_focus_lost(); }
@end

static void build_menu(void) {
    NSMenu *bar = [[NSMenu alloc] init];
    NSMenuItem *appItem = [[NSMenuItem alloc] init];
    [bar addItem:appItem];
    NSMenu *m = [[NSMenu alloc] init];
    NSMenuItem *quit = [[NSMenuItem alloc] initWithTitle:@"Quitter VELOCITY" action:@selector(requestQuit:) keyEquivalent:@"q"];
    [quit setTarget:gDel];
    [m addItem:quit];
    [appItem setSubmenu:m];
    [NSApp setMainMenu:bar];
}

int mac_window_create(int w, int h, const char *title, int visible) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        gDel = [[VDelegate alloc] init];
        build_menu();
        [NSApp finishLaunching];

        NSRect vis = [[NSScreen mainScreen] visibleFrame];     /* la fenetre doit tenir sur l'ecran (barre de titre ~ 30 pt) */
        if (w > vis.size.width) w = (int)vis.size.width;
        if (h > vis.size.height - 30) h = (int)vis.size.height - 30;
        NSRect r = NSMakeRect(0, 0, w, h);
        gWin = [[VWindow alloc] initWithContentRect:r
                                          styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                    NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                                            backing:NSBackingStoreBuffered
                                              defer:NO];
        [gWin setTitle:[NSString stringWithUTF8String:title]];
        [gWin setDelegate:gDel];
        [gWin setAcceptsMouseMovedEvents:YES];
        [gWin setReleasedWhenClosed:NO];
        gView = [[VView alloc] initWithFrame:r];
        [gView setWantsBestResolutionOpenGLSurface:NO];
        [gWin setContentView:gView];
        [gWin center];

        NSOpenGLPixelFormatAttribute at[] = { NSOpenGLPFADoubleBuffer, NSOpenGLPFAColorSize, 32, NSOpenGLPFADepthSize, 24,
                                              NSOpenGLPFAStencilSize, 8, 0 };
        NSOpenGLPixelFormat *pf = [[NSOpenGLPixelFormat alloc] initWithAttributes:at];
        if (!pf) return 0;
        gCtx = [[NSOpenGLContext alloc] initWithFormat:pf shareContext:nil];
        if (!gCtx) return 0;

        if (visible) {
            [gWin makeKeyAndOrderFront:nil];
            [NSApp activateIgnoringOtherApps:YES];
        }
        [gCtx setView:gView];
        [gCtx makeCurrentContext];
        GLint one = 1;
        [gCtx setValues:&one forParameter:NSOpenGLContextParameterSwapInterval];
        return 1;
    }
}

void mac_window_place(int x, int y, int cw, int ch) {
    [gWin setContentSize:NSMakeSize(cw, ch)];
    CGFloat screenH = [[NSScreen screens][0] frame].size.height;
    [gWin setFrameOrigin:NSMakePoint(x, screenH - y - [gWin frame].size.height)];
}

void mac_window_size(int *w, int *h) { content_size(w, h); }

void mac_swap(void) { [gCtx flushBuffer]; }

int mac_has_focus(void) { return [gWin isKeyWindow]; }

void mac_quit(void) { gQuit = 1; }

void mac_set_fullscreen(int on) {                         /* plein ecran sans bordure : instantane, comme sous Windows */
    if (on == gFull) return;
    if (on) {
        gPrevFrame = [gWin frame]; gPrevStyle = [gWin styleMask]; gPrevLevel = [gWin level];
        NSScreen *s = [gWin screen] ? [gWin screen] : [NSScreen mainScreen];
        [gWin setStyleMask:NSWindowStyleMaskBorderless];
        [gWin setLevel:NSMainMenuWindowLevel + 1];
        [gWin setFrame:[s frame] display:YES];
        [NSApp setPresentationOptions:NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar];
    } else {
        [NSApp setPresentationOptions:NSApplicationPresentationDefault];
        [gWin setStyleMask:gPrevStyle];
        [gWin setLevel:gPrevLevel];
        [gWin setFrame:gPrevFrame display:YES];
    }
    gFull = on;
    [gWin makeKeyAndOrderFront:nil];
    [gCtx update];
}

void mac_set_capture(int on) {
    if (on == gCaptured) return;
    gCaptured = on;
    if (on) {
        NSRect r = [gWin convertRectToScreen:[gView convertRect:[gView bounds] toView:nil]];
        CGFloat screenH = [[NSScreen screens][0] frame].size.height;
        CGWarpMouseCursorPosition(CGPointMake(r.origin.x + r.size.width / 2, screenH - (r.origin.y + r.size.height / 2)));
        CGAssociateMouseAndMouseCursorPosition(false);
        if (!gCursorHidden) { [NSCursor hide]; gCursorHidden = 1; }
    } else {
        CGAssociateMouseAndMouseCursorPosition(true);
        if (gCursorHidden) { [NSCursor unhide]; gCursorHidden = 0; }
    }
}

void mac_shutdown(void) {
    mac_set_capture(0);
    if (gFull) mac_set_fullscreen(0);
    [NSOpenGLContext clearCurrentContext];
}

/* ------------------------------------------------------------------ */
/* Evenements                                                          */
/* ------------------------------------------------------------------ */
static void mouse_pos(NSEvent *e) {
    NSPoint p = [e locationInWindow];
    int h = (int)[gView bounds].size.height;
    plat_mouse_move((int)p.x, h - (int)p.y);
}

static int in_content(NSEvent *e) {
    if ([e window] != gWin) return 0;
    NSRect c = [gView convertRect:[gView bounds] toView:nil];
    return NSPointInRect([e locationInWindow], c);
}

static int button_id(NSEvent *e) {                        /* 0 G, 1 D, 2 milieu, 3 pouce arriere, 4 pouce avant */
    switch ([e type]) {
    case NSEventTypeLeftMouseDown: case NSEventTypeLeftMouseUp: return 0;
    case NSEventTypeRightMouseDown: case NSEventTypeRightMouseUp: return 1;
    default: {
        NSInteger b = [e buttonNumber];
        return b == 2 ? 2 : b == 3 ? 3 : 4;
    }
    }
}

static void modifier_change(NSEvent *e) {
    static const struct { int vk; NSUInteger mask; int sc; } mods[] = {
        { 56, 0x02, 0x2A }, { 60, 0x04, 0x36 },           /* Maj gauche / droite */
        { 59, 0x01, 0x1D }, { 62, 0x2000, 0x1D },         /* Ctrl */
        { 58, 0x20, 0x38 }, { 61, 0x40, 0x38 },           /* Option = Alt */
    };
    int vk = (int)[e keyCode];
    for (int i = 0; i < 6; i++)
        if (mods[i].vk == vk) {
            plat_key(mods[i].sc, ([e modifierFlags] & mods[i].mask) != 0, 0);
            return;
        }
}

static void handle_event(NSEvent *e) {
    static float scrollAcc;
    switch ([e type]) {
    case NSEventTypeKeyDown: {
        if ([e modifierFlags] & NSEventModifierFlagCommand) { [NSApp sendEvent:e]; return; }   /* Cmd+Q, Cmd+M... */
        int sc = sc_of_vk([e keyCode]);
        if (sc) plat_key(sc, 1, [e isARepeat]);
        NSString *s = [e characters];
        for (NSUInteger i = 0; i < [s length]; i++) {
            unichar c = [s characterAtIndex:i];
            if (c == 0x7F) c = 8;                         /* touche Retour arriere */
            else if (c == 3) c = 13;                      /* Entree du pave numerique */
            if (c == 8 || c == 13 || (c >= 32 && c < 127)) plat_char(c);
        }
        return;
    }
    case NSEventTypeKeyUp: {
        int sc = sc_of_vk([e keyCode]);
        if (sc) plat_key(sc, 0, 0);
        return;
    }
    case NSEventTypeFlagsChanged:
        modifier_change(e);
        return;
    case NSEventTypeLeftMouseDown: case NSEventTypeRightMouseDown: case NSEventTypeOtherMouseDown:
        if (in_content(e) || gCaptured) { mouse_pos(e); plat_button(button_id(e), 1); }
        else [NSApp sendEvent:e];                         /* barre de titre : deplacement, fermeture... */
        return;
    case NSEventTypeLeftMouseUp: case NSEventTypeRightMouseUp: case NSEventTypeOtherMouseUp:
        mouse_pos(e);
        plat_button(button_id(e), 0);
        [NSApp sendEvent:e];
        return;
    case NSEventTypeMouseMoved: case NSEventTypeLeftMouseDragged: case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
        plat_raw_motion((float)[e deltaX], (float)[e deltaY]);
        mouse_pos(e);
        return;
    case NSEventTypeScrollWheel: {
        float d = (float)[e scrollingDeltaY];
        if ([e hasPreciseScrollingDeltas]) {              /* pave tactile : un cran tous les 12 points */
            scrollAcc += d;
            if (fabsf(scrollAcc) >= 12) { scrollAcc = 0; plat_wheel(); }
        } else if (d != 0) plat_wheel();
        return;
    }
    default:
        [NSApp sendEvent:e];
    }
}

int mac_pump(void) {
    @autoreleasepool {
        for (;;) {
            NSEvent *e = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES];
            if (!e) break;
            handle_event(e);
        }
        [NSApp updateWindows];
    }
    return !gQuit;
}

/* ------------------------------------------------------------------ */
/* Noms des touches                                                    */
/* ------------------------------------------------------------------ */
void mac_key_name(int sc, char *out, int n) {
    static const struct { int sc; const char *name; } special[] = {
        { 0x01, "Echap" }, { 0x0E, "Retour" }, { 0x0F, "Tab" }, { 0x1C, "Entree" }, { 0x1D, "Ctrl" }, { 0x2A, "Maj" },
        { 0x36, "Maj droite" }, { 0x38, "Alt" }, { 0x39, "Espace" }, { 0x3A, "Verr Maj" },
        { 0x3B, "F1" }, { 0x3C, "F2" }, { 0x3D, "F3" }, { 0x3E, "F4" }, { 0x3F, "F5" }, { 0x40, "F6" }, { 0x41, "F7" },
        { 0x42, "F8" }, { 0x43, "F9" }, { 0x44, "F10" }, { 0x57, "F11" }, { 0x58, "F12" },
        { 0x48, "Fleche haut" }, { 0x50, "Fleche bas" }, { 0x4B, "Fleche gauche" }, { 0x4D, "Fleche droite" },
        { 0x47, "Debut" }, { 0x4F, "Fin" }, { 0x49, "Page haut" }, { 0x51, "Page bas" }, { 0x53, "Suppr" },
    };
    for (size_t i = 0; i < sizeof special / sizeof special[0]; i++)
        if (special[i].sc == sc) { snprintf(out, n, "%s", special[i].name); return; }
    for (int i = 0; i < NKEYS; i++) {
        if (keyMap[i].sc != sc) continue;
        TISInputSourceRef src = TISCopyCurrentKeyboardLayoutInputSource();
        CFDataRef data = src ? (CFDataRef)TISGetInputSourceProperty(src, kTISPropertyUnicodeKeyLayoutData) : NULL;
        if (data) {                                       /* caractere de la touche selon la disposition courante */
            UInt32 dead = 0;
            UniChar ch[4];
            UniCharCount len = 0;
            UCKeyTranslate((const UCKeyboardLayout *)CFDataGetBytePtr(data), (UInt16)keyMap[i].vk, kUCKeyActionDisplay, 0,
                           LMGetKbdType(), kUCKeyTranslateNoDeadKeysBit, &dead, 4, &len, ch);
            if (len == 1 && ch[0] > 32 && ch[0] < 256) {
                snprintf(out, n, "%c", ch[0] < 127 ? toupper(ch[0]) : ch[0]);
                CFRelease(src);
                return;
            }
        }
        if (src) CFRelease(src);
        break;
    }
    snprintf(out, n, "touche %d", sc);
}

/* ------------------------------------------------------------------ */
/* Police bitmap : 224 listes d'affichage glBitmap, comme wglUseFontBitmaps */
/* ------------------------------------------------------------------ */
int mac_make_font(unsigned base, int px, int *cw, int *ch) {
    @autoreleasepool {
        CTFontRef font = CTFontCreateWithName(CFSTR("Menlo-Bold"), px * 0.92, NULL);
        if (!font) font = CTFontCreateWithName(CFSTR("Courier-Bold"), px * 0.92, NULL);
        if (!font) return 0;
        UniChar m = 'M';
        CGGlyph gm = 0;
        CGSize adv = CGSizeMake(px * 0.55, 0);
        if (CTFontGetGlyphsForCharacters(font, &m, &gm, 1))
            CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal, &gm, &adv, 1);
        int D = (int)ceil(CTFontGetDescent(font)), W = (int)ceil(adv.width), H = (int)ceil(CTFontGetAscent(font)) + D;
        int bpr = (W + 7) / 8;
        CGColorSpaceRef gray = CGColorSpaceCreateDeviceGray();
        unsigned char *pix = (unsigned char *)malloc((size_t)W * H);
        unsigned char *bits = (unsigned char *)malloc((size_t)bpr * H);
        GLint oldAlign;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldAlign);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        for (int c = 32; c < 256; c++) {
            memset(pix, 0, (size_t)W * H);
            memset(bits, 0, (size_t)bpr * H);
            unsigned char byte = (unsigned char)c;
            NSString *str = [[NSString alloc] initWithBytes:&byte length:1 encoding:NSWindowsCP1252StringEncoding];
            UniChar uc = ([str length] > 0) ? [str characterAtIndex:0] : '?';
            CGGlyph g = 0;
            if (c != 32 && CTFontGetGlyphsForCharacters(font, &uc, &g, 1)) {
                CGContextRef cg = CGBitmapContextCreate(pix, W, H, 8, W, gray, (CGBitmapInfo)kCGImageAlphaNone);
                CGContextSetAllowsFontSmoothing(cg, false);
                CGContextSetGrayFillColor(cg, 1, 1);
                CGPoint pos = CGPointMake(0, D);
                CTFontDrawGlyphs(font, &g, &pos, 1, cg);
                CGContextRelease(cg);
                for (int r = 0; r < H; r++)               /* glBitmap : premiere ligne = bas ; la memoire CG commence par le haut */
                    for (int x = 0; x < W; x++)
                        if (pix[(size_t)(H - 1 - r) * W + x] >= 100) bits[(size_t)r * bpr + x / 8] |= (unsigned char)(0x80 >> (x & 7));
            }
            glNewList(base + (unsigned)(c - 32), GL_COMPILE);
            glBitmap(W, H, 0, (GLfloat)D, (GLfloat)W, 0, bits);
            glEndList();
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, oldAlign);
        free(pix); free(bits);
        CGColorSpaceRelease(gray);
        CFRelease(font);
        *cw = W; *ch = H;
        return 1;
    }
}

/* ------------------------------------------------------------------ */
/* Audio : AudioQueue, tampons rendus au fil principal                  */
/* ------------------------------------------------------------------ */
#define MAX_AUDIO_BUF 8
static AudioQueueRef gQ;
static AudioQueueBufferRef gBuf[MAX_AUDIO_BUF];
static atomic_int gDone[MAX_AUDIO_BUF];
static int gBufBytes;

static void audio_cb(void *user, AudioQueueRef q, AudioQueueBufferRef b) {
    (void)user; (void)q;
    atomic_store(&gDone[(intptr_t)b->mUserData], 1);
}

int mac_audio_open(int rate, int nbuf, int buflen) {
    if (nbuf > MAX_AUDIO_BUF) return 0;
    AudioStreamBasicDescription f = { 0 };
    f.mSampleRate = rate; f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    f.mBitsPerChannel = 16; f.mChannelsPerFrame = 1; f.mBytesPerFrame = 2; f.mFramesPerPacket = 1; f.mBytesPerPacket = 2;
    if (AudioQueueNewOutput(&f, audio_cb, NULL, NULL, NULL, 0, &gQ) != noErr) return 0;
    gBufBytes = buflen * 2;
    for (int i = 0; i < nbuf; i++) {
        if (AudioQueueAllocateBuffer(gQ, (UInt32)gBufBytes, &gBuf[i]) != noErr) return 0;
        gBuf[i]->mUserData = (void *)(intptr_t)i;
        atomic_store(&gDone[i], 0);
    }
    return AudioQueueStart(gQ, NULL) == noErr;
}

int mac_audio_done(int i) { return atomic_load(&gDone[i]); }

void mac_audio_write(int i, const short *samples) {
    memcpy(gBuf[i]->mAudioData, samples, (size_t)gBufBytes);
    gBuf[i]->mAudioDataByteSize = (UInt32)gBufBytes;
    atomic_store(&gDone[i], 0);
    AudioQueueEnqueueBuffer(gQ, gBuf[i], 0, NULL);
}

void mac_audio_close(void) {
    if (gQ) { AudioQueueStop(gQ, true); AudioQueueDispose(gQ, true); gQ = NULL; }
}

/* ------------------------------------------------------------------ */
/* WebSocket du lobby                                                  */
/* ------------------------------------------------------------------ */
@interface WsImpl : NSObject <NSURLSessionWebSocketDelegate>
@property(strong) NSURLSession *session;
@property(strong) NSURLSessionWebSocketTask *task;
@property(strong) dispatch_semaphore_t openSem;
@property(atomic) int opened;
@end

@implementation WsImpl
- (void)URLSession:(NSURLSession *)s webSocketTask:(NSURLSessionWebSocketTask *)t didOpenWithProtocol:(NSString *)p {
    (void)s; (void)t; (void)p;
    self.opened = 1;
    dispatch_semaphore_signal(self.openSem);
}
- (void)URLSession:(NSURLSession *)s task:(NSURLSessionTask *)t didCompleteWithError:(NSError *)e {
    (void)s; (void)t; (void)e;
    dispatch_semaphore_signal(self.openSem);              /* echec avant l'ouverture */
}
@end

MacWs *mac_ws_open(const char *host, int port, int secure) {
    @autoreleasepool {
        WsImpl *w = [[WsImpl alloc] init];
        w.openSem = dispatch_semaphore_create(0);
        NSURL *url = [NSURL URLWithString:[NSString stringWithFormat:@"%s://%s:%d/", secure ? "wss" : "ws", host, port]];
        if (!url) return NULL;
        NSURLSessionConfiguration *cfg = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        cfg.timeoutIntervalForRequest = 8;
        w.session = [NSURLSession sessionWithConfiguration:cfg delegate:w delegateQueue:nil];
        w.task = [w.session webSocketTaskWithURL:url];
        [w.task resume];
        dispatch_semaphore_wait(w.openSem, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
        if (!w.opened) {
            [w.task cancel];
            [w.session invalidateAndCancel];
            return NULL;
        }
        return (MacWs *)CFBridgingRetain(w);
    }
}

int mac_ws_send(MacWs *h, const char *s, int n) {
    @autoreleasepool {
        WsImpl *w = (__bridge WsImpl *)h;
        NSString *str = [[NSString alloc] initWithBytes:s length:(NSUInteger)n encoding:NSUTF8StringEncoding];
        if (!str) return -1;
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        __block int fail = 0;
        [w.task sendMessage:[[NSURLSessionWebSocketMessage alloc] initWithString:str]
          completionHandler:^(NSError *e) { fail = e != nil; dispatch_semaphore_signal(sem); }];
        if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC)) != 0) return -1;
        return fail ? -1 : 0;
    }
}

int mac_ws_recv(MacWs *h, char *buf, int cap) {
    @autoreleasepool {
        WsImpl *w = (__bridge WsImpl *)h;
        dispatch_semaphore_t sem = dispatch_semaphore_create(0);
        __block NSString *got = nil;
        [w.task receiveMessageWithCompletionHandler:^(NSURLSessionWebSocketMessage *m, NSError *e) {
            if (!e && m) {
                if ([m type] == NSURLSessionWebSocketMessageTypeString) got = [m string];
                else got = [[NSString alloc] initWithData:[m data] encoding:NSUTF8StringEncoding];
                if (!got) got = @"";
            }
            dispatch_semaphore_signal(sem);
        }];
        /* le serveur repond a nos "ping" : 60 s sans rien = connexion morte */
        if (dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, 60 * NSEC_PER_SEC)) != 0) return -1;
        if (!got) return -1;
        const char *u = [got UTF8String];
        int len = (int)strlen(u);
        if (len >= cap) return 0;                         /* ligne beaucoup trop longue : on jette */
        memcpy(buf, u, (size_t)len + 1);
        return len;
    }
}

void mac_ws_close(MacWs *h) {
    if (!h) return;
    WsImpl *w = (__bridge WsImpl *)h;
    [w.task cancelWithCloseCode:NSURLSessionWebSocketCloseCodeNormalClosure reason:nil];
    [w.session invalidateAndCancel];
}

void mac_ws_free(MacWs *h) {
    if (h) CFBridgingRelease(h);
}
