/*
 * Afterlife (LucasArts, 1996; Windows 95 build) - static recompilation host.
 *
 * A 32-bit host on pcrecomp's runtime/native32. The game is one executable,
 * ALIFE.EXE, mapped at its own base 0x00400000 and lifted whole. It draws
 * with plain GDI (an 8-bit DIB section blitted to its window), so --shot and
 * --record read the source of its blits. docs/bringup.md has the rest.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native32.h"
#include "recomp_trace.h"

#define EXE_BASE 0x00400000u
#define ARG(n) MEM32(g_esp + 4 + 4 * (n))

static native32_module_t g_mods[] = { { "ALIFE.EXE", EXE_BASE } };
static char g_game_dir[MAX_PATH];
static DWORD g_watchdog_s;
static void record_close(void);

static void shim_MessageBoxA(void) {
    fprintf(stderr, "[messagebox] %s: %s\n", (char*)(uintptr_t)ARG(2), (char*)(uintptr_t)ARG(1));
    g_eax = IDOK;
    g_esp += 4 + 4 * 4;
}

static void shim_MessageBoxW(void) {
    fprintf(stderr, "[messagebox] %ls: %ls\n", (wchar_t*)(uintptr_t)ARG(2), (wchar_t*)(uintptr_t)ARG(1));
    recomp_dump_trace("messagebox");
    g_eax = IDOK;
    g_esp += 4 + 4 * 4;
}

/* The game's frames: it blits from 8-bit DIB sections to its window, the
 * map view and the panels separately. Each blit to the window is repeated
 * into a 32-bit shadow of it (GDI does the palette), which --shot and
 * --record read: on --headless's private desktop a copy of the window is
 * black. */
static HDC g_shadow;
static void shadow_blit(HDC dst, int dx, int dy, int dw, int dh, HDC src, int sx, int sy, int sw, int sh);

static void shim_BitBlt(void) {
    HDC dst = (HDC)(uintptr_t)ARG(0), src = (HDC)(uintptr_t)ARG(5);
    g_eax = BitBlt(dst, ARG(1), ARG(2), ARG(3), ARG(4), src, ARG(6), ARG(7), ARG(8));
    if (src && ARG(8) == SRCCOPY) shadow_blit(dst, ARG(1), ARG(2), ARG(3), ARG(4), src, ARG(6), ARG(7), ARG(3), ARG(4));
    g_esp += 4 + 4 * 9;
}

static void shim_StretchBlt(void) {
    HDC dst = (HDC)(uintptr_t)ARG(0), src = (HDC)(uintptr_t)ARG(5);
    g_eax = StretchBlt(dst, ARG(1), ARG(2), ARG(3), ARG(4), src, ARG(6), ARG(7), ARG(8), ARG(9), ARG(10));
    if (src && ARG(10) == SRCCOPY) shadow_blit(dst, ARG(1), ARG(2), ARG(3), ARG(4), src, ARG(6), ARG(7), ARG(8), ARG(9));
    g_esp += 4 + 4 * 11;
}

/* The cursor is the host's, not Windows': --click sets it, and the game's
 * GetCursorPos/SetCursorPos never touch the real one (on --headless's private
 * desktop SetCursorPos would move the user's pointer). Screen coordinates. */
static volatile LONG g_cur_x = 320, g_cur_y = 240;

static void shim_GetCursorPos(void) {
    POINT* p = (POINT*)(uintptr_t)ARG(0);
    p->x = g_cur_x; p->y = g_cur_y;
    g_eax = TRUE;
    g_esp += 4 + 4;
}

static void shim_SetCursorPos(void) {
    InterlockedExchange(&g_cur_x, (LONG)ARG(0));
    InterlockedExchange(&g_cur_y, (LONG)ARG(1));
    g_eax = TRUE;
    g_esp += 4 + 8;
}

/* ALIFE.INI is read from the game folder (the CD's copy says Resolution = MAX,
 * the desktop's size, and saves to C:\ALIFE). The host's own copy in work\
 * wins: 640x480 and saves under work\save. Written on first run. */
static char g_ini[MAX_PATH];

static void shim_CreateFileA(void) {
    const char* name = (const char*)(uintptr_t)ARG(0);
    const char* base = name ? strrchr(name, '\\') : NULL;
    base = base ? base + 1 : name;
    if (base && !_stricmp(base, "ALIFE.INI")) name = g_ini;
    g_eax = (uint32_t)(uintptr_t)CreateFileA(name, ARG(1), ARG(2), (LPSECURITY_ATTRIBUTES)(uintptr_t)ARG(3),
                                             ARG(4), ARG(5), (HANDLE)(uintptr_t)ARG(6));
    g_esp += 4 + 4 * 7;
}

static native32_shim_t g_shims[] = {
    { "CreateFileA", shim_CreateFileA },
    { "GetCursorPos", shim_GetCursorPos },
    { "SetCursorPos", shim_SetCursorPos },
    { "MessageBoxA", shim_MessageBoxA },
    { "BitBlt", shim_BitBlt },
    { "StretchBlt", shim_StretchBlt },
    { "MessageBoxW", shim_MessageBoxW },
};

/* ---- diagnostics ---------------------------------------------------------- */

void recomp_not_lifted(uint32_t va) {
    fprintf(stderr, "\n[not-lifted] sub_%08X  (called from 0x%08X)\n", va, g_cur_func);
    recomp_dump_trace("not-lifted");
    native32_dump_icalls(8);
    fflush(stderr);
    TerminateProcess(GetCurrentProcess(), 2);
}

recomp_func_t recomp_lookup_manual(uint32_t va) { (void)va; return NULL; }

static LONG CALLBACK crash(EXCEPTION_POINTERS* ep) {
    static volatile LONG once;
    EXCEPTION_RECORD* r = ep->ExceptionRecord;
    HMODULE hm;
    if ((r->ExceptionCode & 0xF0000000u) != 0xC0000000u) return EXCEPTION_CONTINUE_SEARCH;
    /* A fault inside a Windows DLL is its own business first (ddraw probes
     * with __try); only the host (lifted code) and code outside any module. */
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)r->ExceptionAddress, &hm) && hm != GetModuleHandleA(NULL))
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedExchange(&once, 1)) { Sleep(5000); TerminateProcess(GetCurrentProcess(), 3); }
    fprintf(stderr, "\n=== fault 0x%08lX at %p, thread %lu ===\n", r->ExceptionCode, r->ExceptionAddress,
            GetCurrentThreadId());
    if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r->NumberParameters >= 2)
        fprintf(stderr, "  %s of 0x%08X\n", r->ExceptionInformation[0] == 0 ? "read" :
                r->ExceptionInformation[0] == 1 ? "write" : "execute", (uint32_t)r->ExceptionInformation[1]);
    fprintf(stderr, "  in lifted sub_%08X, last native call %s\n", g_cur_func, g_cur_import);
    fprintf(stderr, "  eax=%08X ecx=%08X edx=%08X ebx=%08X esp=%08X ebp=%08X esi=%08X edi=%08X\n",
            g_eax, g_ecx, g_edx, g_ebx, g_esp, g_ebp, g_esi, g_edi);
    native32_dump_icalls(12);
    recomp_trace_flush();
    TerminateProcess(GetCurrentProcess(), 3);
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI watchdog(LPVOID unused) {
    (void)unused;
    Sleep(g_watchdog_s * 1000);
    fprintf(stderr, "\n[watchdog] %lu s: in sub_%08X, last native call %s, %u indirect calls\n",
            g_watchdog_s, g_cur_func, g_cur_import, g_icall_count);
    native32_dump_icalls(8);
    recomp_trace_flush();
    fflush(stderr);
    record_close();
    TerminateProcess(GetCurrentProcess(), 4);
    return 0;
}

/* ---- --shot S FILE and --record FILE.mp4: the frames the game presents ---
 * Taken from the source of the game's blits (grab, above). */
static DWORD g_shot_s[16];
static char g_shot_file[16][MAX_PATH];
static int g_shots;
static volatile LONG g_shot_due = -1;
static char g_record[MAX_PATH];
static FILE* g_rec;

static BOOL CALLBACK find_window(HWND w, LPARAM out) {
    DWORD pid;
    GetWindowThreadProcessId(w, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(w)) { *(HWND*)out = w; return FALSE; }
    return TRUE;
}

static HWND game_window(void) {
    HWND w = NULL;
    EnumWindows(find_window, (LPARAM)&w);
    return w;
}

static void write_bmp(const char* file, int w, int h, const uint8_t* bgr, DWORD stride) {
    BITMAPINFOHEADER bh = { sizeof bh, w, h, 1, 24, BI_RGB };
    BITMAPFILEHEADER fh = { 0x4D42, sizeof fh + sizeof bh + stride * h, 0, 0, sizeof fh + sizeof bh };
    FILE* f = fopen(file, "wb");
    if (!f) return;
    fwrite(&fh, sizeof fh, 1, f);
    fwrite(&bh, sizeof bh, 1, f);
    fwrite(bgr, stride * h, 1, f);
    fclose(f);
    printf("[shot] %dx%d -> %s\n", w, h, file);
}

#define SHADOW_W 640
#define SHADOW_H 480
static uint8_t* g_shadow_px;     /* BGRX, top-down */
static volatile LONG g_blits;

static void shot_write(LONG due) {
    if (due < 0 || !g_shadow_px) return;
    DWORD stride = (SHADOW_W * 3 + 3) & ~3u;
    uint8_t* out = (uint8_t*)calloc(stride, SHADOW_H);
    GdiFlush();
    for (int y = 0; y < SHADOW_H; y++)
        for (int x = 0; x < SHADOW_W; x++)
            memcpy(out + (SHADOW_H - 1 - y) * stride + x * 3, g_shadow_px + (y * SHADOW_W + x) * 4, 3);
    write_bmp(g_shot_file[due], SHADOW_W, SHADOW_H, out, stride);
    free(out);
    InterlockedExchange(&g_shot_due, -1);
}

static void shadow_blit(HDC dst, int dx, int dy, int dw, int dh, HDC src, int sx, int sy, int sw, int sh) {
    if (!WindowFromDC(dst)) return;              /* off-screen work, not the window */
    InterlockedIncrement(&g_blits);
    if (!g_record[0] && !g_shots) return;
    if (!g_shadow) {
        BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), SHADOW_W, -SHADOW_H, 1, 32, BI_RGB } };
        g_shadow = CreateCompatibleDC(NULL);
        SelectObject(g_shadow, CreateDIBSection(g_shadow, &bi, DIB_RGB_COLORS, (void**)&g_shadow_px, NULL, 0));
    }
    if (dw == sw && dh == sh) BitBlt(g_shadow, dx, dy, dw, dh, src, sx, sy, SRCCOPY);
    else StretchBlt(g_shadow, dx, dy, dw, dh, src, sx, sy, sw, sh, SRCCOPY);
    if (g_shot_due >= 0 && dw * dh >= SHADOW_W * SHADOW_H / 4) shot_write(g_shot_due);
    if (g_record[0]) {
        if (!g_rec) {
            char cmd[MAX_PATH + 256];
            _snprintf(cmd, sizeof cmd - 1, "ffmpeg -hide_banner -loglevel error -y -f rawvideo -pix_fmt bgr0 -s %dx%d "
                      "-use_wallclock_as_timestamps 1 -i - -vf format=yuv420p -fps_mode vfr \"%s\"",
                      SHADOW_W, SHADOW_H, g_record);
            cmd[sizeof cmd - 1] = 0;
            g_rec = _popen(cmd, "wb");
            if (!g_rec) { fprintf(stderr, "[record] cannot start ffmpeg\n"); g_record[0] = 0; }
        }
        GdiFlush();
        if (g_rec) fwrite(g_shadow_px, (size_t)SHADOW_W * SHADOW_H * 4, 1, g_rec);
    }
}

/* ponytail: a 200 ms grace for the frame in flight, not a lock. */
static void record_close(void) {
    if (!g_rec) return;
    g_record[0] = 0;
    Sleep(200);
    _pclose(g_rec);
    g_rec = NULL;
}

static DWORD WINAPI shot_thread(LPVOID unused) {
    (void)unused;
    DWORD t0 = GetTickCount();
    for (int i = 0; i < g_shots; i++) {
        DWORD at = t0 + g_shot_s[i] * 1000, now = GetTickCount();
        if (at > now) Sleep(at - now);
        InterlockedExchange(&g_shot_due, i);
        for (int k = 0; k < 50 && g_shot_due >= 0; k++) Sleep(20);
        /* ponytail: a still screen blits nothing; the last frame is read
         * from this thread without a lock (a torn frame is fine for a shot). */
        if (g_shot_due >= 0) { printf("[shot] still screen (%ld blits so far)\n", g_blits); shot_write(g_shot_due); }
        if (InterlockedExchange(&g_shot_due, -1) >= 0) printf("[shot] no frame in 1 s\n");
    }
    return 0;
}

/* ---- --key VK@S, --click X,Y@S and --click X,Y,X2,Y2@S (a drag) ------------
 * Posted to the game window, not SendInput: under --headless the window is on
 * a desktop of its own, and SendInput would type into the user's session.
 * The game reads window messages and GetCursorPos (the host's cursor). */
static struct { int x, y, x2, y2; WORD vk; DWORD at; } g_input[64];
static int g_ninput;

static void mouse_to(HWND w, int x, int y, WPARAM buttons) {
    POINT pt = { x, y };
    ClientToScreen(w, &pt);
    InterlockedExchange(&g_cur_x, pt.x);
    InterlockedExchange(&g_cur_y, pt.y);
    PostMessageA(w, WM_MOUSEMOVE, buttons, MAKELPARAM(x, y));
}

static DWORD WINAPI input_thread(LPVOID unused) {
    (void)unused;
    DWORD t0 = GetTickCount();
    for (int i = 0; i < g_ninput; i++) {
        DWORD at = t0 + g_input[i].at, now = GetTickCount();
        if (at > now) Sleep(at - now);
        HWND w = game_window();
        if (!w) { printf("[input] no window\n"); continue; }
        if (g_input[i].vk) {
            UINT sc = MapVirtualKeyA(g_input[i].vk, 0);
            LPARAM lp = 1 | ((LPARAM)sc << 16);
            PostMessageA(w, WM_KEYDOWN, g_input[i].vk, lp);
            Sleep(150);
            PostMessageA(w, WM_KEYUP, g_input[i].vk, lp | 0xC0000000);
            printf("[key] 0x%02X at %lu ms\n", g_input[i].vk, GetTickCount() - t0);
            continue;
        }
        int x = g_input[i].x, y = g_input[i].y, x2 = g_input[i].x2, y2 = g_input[i].y2;
        mouse_to(w, x, y, 0);
        Sleep(250);
        PostMessageA(w, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
        Sleep(200);
        for (int k = 1; k <= 10 && (x2 != x || y2 != y); k++) {   /* a drag, in ten steps */
            mouse_to(w, x + (x2 - x) * k / 10, y + (y2 - y) * k / 10, MK_LBUTTON);
            Sleep(60);
        }
        PostMessageA(w, WM_LBUTTONUP, 0, MAKELPARAM(x2, y2));
        printf("[click] %d,%d", x, y);
        if (x2 != x || y2 != y) printf(" to %d,%d", x2, y2);
        printf(" at %lu ms\n", GetTickCount() - t0);
    }
    return 0;
}

static int add_input(const char* spec, int click) {
    const char* at = strchr(spec, '@');
    if (!at || g_ninput >= 64) return 0;
    if (click) {
        int n = sscanf(spec, "%d,%d,%d,%d", &g_input[g_ninput].x, &g_input[g_ninput].y,
                       &g_input[g_ninput].x2, &g_input[g_ninput].y2);
        if (n == 2) { g_input[g_ninput].x2 = g_input[g_ninput].x; g_input[g_ninput].y2 = g_input[g_ninput].y; }
        else if (n != 4) return 0;
    } else {
        g_input[g_ninput].vk = (WORD)strtoul(spec, NULL, 0);
    }
    g_input[g_ninput++].at = (DWORD)(atof(at + 1) * 1000);
    return 1;
}

static int host_main(int argc, char** argv) {
    const char* game = "game";
    int run = 0;
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    /* Silent test runs: --mute, and --headless unless PCRECOMP_MUTE says
     * otherwise (native32_init mutes the audio session; PCRECOMP_MUTE=0
     * restores it). Set before the relaunches, which inherit it. */
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--mute") ||
            (!strcmp(argv[i], "--headless") && !GetEnvironmentVariableA("PCRECOMP_MUTE", NULL, 0)))
            SetEnvironmentVariableA("PCRECOMP_MUTE", "1");
    for (int i = 1; i < argc; i++) {
        int rc;
        if (!strcmp(argv[i], "--headless") && (rc = native32_headless()) >= 0) return rc;
    }
    for (int i = 1; i < argc; i++) {
        int n = recomp_trace_arg(argc, argv, i);
        if (n) { i += n - 1; continue; }
        if (!strcmp(argv[i], "--run")) run = 1;
        else if (!strcmp(argv[i], "--headless") ||!strcmp(argv[i], "--mute")) ;
        else if (!strcmp(argv[i], "--game") && i + 1 < argc) game = argv[++i];
        else if (!strcmp(argv[i], "--watchdog") && i + 1 < argc) g_watchdog_s = strtoul(argv[++i], NULL, 0);
        else if (!strcmp(argv[i], "--shot") && i + 2 < argc && g_shots < 16) {
            g_shot_s[g_shots] = strtoul(argv[++i], NULL, 0);
            GetFullPathNameA(argv[++i], MAX_PATH, g_shot_file[g_shots++], NULL);
        }
        else if (!strcmp(argv[i], "--key") && i + 1 < argc) { if (!add_input(argv[++i], 0)) return 1; }
        else if (!strcmp(argv[i], "--click") && i + 1 < argc) { if (!add_input(argv[++i], 1)) return 1; }
        else if (!strcmp(argv[i], "--record") && i + 1 < argc) GetFullPathNameA(argv[++i], MAX_PATH, g_record, NULL);
        else if (!strcmp(argv[i], "--native-trace")) native32_trace_native = 1;
        else if (!strcmp(argv[i], "--callbacks")) native32_trace_callbacks = 1;
        else {
            printf("usage: afterlife [--run] [--headless] [--game DIR] [--watchdog S]\n"
                   "                    [--shot S FILE.bmp] [--record FILE.mp4] [--key VK@S] [--click X,Y@S]\n"
                   "                    [--native-trace] [--callbacks]\n");
            recomp_trace_help();
            return argv[i][1] == 'h' || argv[i][2] == 'h' ? 0 : 1;
        }
    }
    GetFullPathNameA(game, MAX_PATH, g_game_dir, NULL);
    {
        char exe[MAX_PATH];
        _snprintf(exe, sizeof exe - 1, "%s\\ALIFE.EXE", g_game_dir);
        exe[MAX_PATH - 1] = 0;
        native32_hold_image(exe, EXE_BASE);
    }

    native32_init();
    native32_allow_heap_code();
    {
        char save[MAX_PATH];
        GetFullPathNameA("work/save", MAX_PATH, save, NULL);
        GetFullPathNameA("work/ALIFE.INI", MAX_PATH, g_ini, NULL);
        CreateDirectoryA("work", NULL);
        CreateDirectoryA(save, NULL);
        if (GetFileAttributesA(g_ini) == INVALID_FILE_ATTRIBUTES) {
            FILE* f = fopen(g_ini, "w");
            if (f) {
                fprintf(f, "Resolution     = 640x480\nResource file  = ALIFE.000\nCD Path        = ALIFE.000\n"
                           "Save games     = %s\nWindow name    = Afterlife\nMixer channels = 8\n"
                           "Color depth    = 8\nHeap size      = 7000\nMusic stream   = 200000, 150000, 32000\n"
                           "Voice stream   = 65000, 48000, 22000\n", save);
                fclose(f);
            }
        }
    }
    AddVectoredExceptionHandler(0, crash);
    printf("Afterlife recomp host\n  lifted functions in dispatch: %u\n", recomp_dispatch_count);
    if (native32_modules_map(g_game_dir, g_game_dir, g_mods, 1) != 1) {
        fprintf(stderr, "cannot map %s\\ALIFE.EXE at 0x%08X\n", g_game_dir, EXE_BASE);
        return 1;
    }
    int missing = native32_modules_bind(g_shims, (int)(sizeof g_shims / sizeof g_shims[0]));
    if (missing) fprintf(stderr, "warning: %d imports unresolved\n", missing);
    IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)(uintptr_t)(EXE_BASE + MEM32(EXE_BASE + 0x3C));
    uint32_t entry = EXE_BASE + nt->OptionalHeader.AddressOfEntryPoint;
    if (!run) {
        printf("\n(dry run: mapped and bound; --run enters 0x%08X)\n", entry);
        return missing ? 1 : 0;
    }
    if (!SetCurrentDirectoryA(g_game_dir)) { fprintf(stderr, "cannot enter %s\n", g_game_dir); return 1; }
    if (g_watchdog_s) CloseHandle(CreateThread(NULL, 0, watchdog, NULL, 0, NULL));
    if (g_shots) CloseHandle(CreateThread(NULL, 0, shot_thread, NULL, 0, NULL));
    if (g_ninput) CloseHandle(CreateThread(NULL, 0, input_thread, NULL, 0, NULL));
    if (!native32_attach_imports(EXE_BASE)) return 1;
    printf("  entering 0x%08X\n\n", entry);
    native32_call_guest(entry, 0, NULL);
    record_close();
    printf("\nentry returned eax=%08X\n", g_eax);
    return (int)g_eax;
}

/* Lifted code runs on the host's C stack and nests deeply, so it gets 16 MB on
 * a thread of its own; the first thread is linked with 256 KB (CMakeLists.txt)
 * so its stack fits below ALIFE.EXE (native32_hold_image). */
static int g_argc;
static char** g_argv;
static DWORD WINAPI host_thread(LPVOID unused) { (void)unused; return (DWORD)host_main(g_argc, g_argv); }

int main(int argc, char** argv) {
    DWORD code = 1;
    g_argc = argc;
    g_argv = argv;
    HANDLE t = CreateThread(NULL, 16u << 20, host_thread, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    if (!t) return 1;
    WaitForSingleObject(t, INFINITE);
    GetExitCodeThread(t, &code);
    return (int)code;
}
