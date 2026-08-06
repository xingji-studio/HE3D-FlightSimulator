#include "he3d_platform.hpp"
#include "he3d_platform_input.hpp"
#include "x3api.h"
#include <time.h>

namespace HE3D {

struct Window {
    HDLE handle;
    int32_t displayWidth;
    int32_t displayHeight;
    ColorA *scaledPixels;
    int32_t scaledWidth;
    int32_t scaledHeight;
    KeyCallback keyCallback;
    void *keyUser;
    volatile bool closeRequested;
};

static Window *g_msgWindow = nullptr;
static volatile int32_t g_keyQueueLock = 0;

struct XapiKeyEvent {
    int32_t key;
    bool pressed;
};

static XapiKeyEvent g_keyQueue[128];
static int32_t g_keyQueueHead = 0;
static int32_t g_keyQueueTail = 0;
static bool g_keyDownState[256];

enum XapiSpecialKey {
    XAPI_KEY_ESC = 128,
    XAPI_KEY_BACKSPACE = 129,
    XAPI_KEY_TAB = 130,
    XAPI_KEY_ENTER = 131,
    XAPI_KEY_SPACE = 136
};

#ifndef HE3D_XAPI_DEBUG_KEYS
#define HE3D_XAPI_DEBUG_KEYS 0
#endif

#if HE3D_XAPI_DEBUG_KEYS
static void XapiAppendUnsigned(char *dst, int32_t *pos, int32_t maxLen, uint64_t value) {
    char tmp[24];
    int32_t n = 0;
    if (value == 0) {
        tmp[n++] = '0';
    } else {
        while (value > 0 && n < (int32_t)sizeof(tmp)) {
            tmp[n++] = (char)('0' + (value % 10U));
            value /= 10U;
        }
    }
    while (n > 0 && *pos < maxLen - 1) {
        dst[(*pos)++] = tmp[--n];
    }
}

static void XapiAppendText(char *dst, int32_t *pos, int32_t maxLen, const char *text) {
    while (*text && *pos < maxLen - 1) {
        dst[(*pos)++] = *text++;
    }
}

static void XapiDebugKey(const char *stage, UINT64 type, UINT64 raw, int32_t key, bool pressed) {
    char line[128];
    int32_t pos = 0;
    XapiAppendText(line, &pos, (int32_t)sizeof(line), "HE3D key ");
    XapiAppendText(line, &pos, (int32_t)sizeof(line), stage);
    XapiAppendText(line, &pos, (int32_t)sizeof(line), " ");
    XapiAppendText(line, &pos, (int32_t)sizeof(line), pressed ? "down" : "up");
    XapiAppendText(line, &pos, (int32_t)sizeof(line), " type=");
    XapiAppendUnsigned(line, &pos, (int32_t)sizeof(line), type);
    XapiAppendText(line, &pos, (int32_t)sizeof(line), " raw=");
    XapiAppendUnsigned(line, &pos, (int32_t)sizeof(line), raw);
    XapiAppendText(line, &pos, (int32_t)sizeof(line), " key=");
    XapiAppendUnsigned(line, &pos, (int32_t)sizeof(line), key < 0 ? (uint64_t)(-key) : (uint64_t)key);
    if (key >= 32 && key <= 126 && pos < (int32_t)sizeof(line) - 4) {
        line[pos++] = ' ';
        line[pos++] = '\'';
        line[pos++] = (char)key;
        line[pos++] = '\'';
    }
    if (pos < (int32_t)sizeof(line) - 1) {
        line[pos++] = '\n';
    }
    line[pos] = 0;
    xapi_OutputSerial(line);
}
#else
static void XapiDebugKey(const char *, UINT64, UINT64, int32_t, bool) {
}
#endif

static void XapiLockKeyQueue() {
    while (__sync_lock_test_and_set(&g_keyQueueLock, 1) != 0) {
    }
}

static void XapiUnlockKeyQueue() {
    __sync_lock_release(&g_keyQueueLock);
}

static void XapiResetKeyQueue() {
    XapiLockKeyQueue();
    g_keyQueueHead = 0;
    g_keyQueueTail = 0;
    for (int32_t i = 0; i < 256; i++) {
        g_keyDownState[i] = false;
    }
    XapiUnlockKeyQueue();
}

static int32_t XapiKeyQueueCapacity() {
    return (int32_t)(sizeof(g_keyQueue) / sizeof(g_keyQueue[0]));
}

static void XapiRemoveQueuedKeyEvent(int32_t index) {
    int32_t capacity = XapiKeyQueueCapacity();
    int32_t cursor = index;
    for (;;) {
        int32_t next = (cursor + 1) % capacity;
        if (next == g_keyQueueHead) {
            break;
        }
        g_keyQueue[cursor] = g_keyQueue[next];
        cursor = next;
    }
    g_keyQueueHead = (g_keyQueueHead + capacity - 1) % capacity;
}

static bool XapiRemovePendingPressForKey(int32_t key) {
    int32_t capacity = XapiKeyQueueCapacity();
    int32_t cursor = g_keyQueueTail;
    while (cursor != g_keyQueueHead) {
        if (g_keyQueue[cursor].key == key && g_keyQueue[cursor].pressed) {
            XapiRemoveQueuedKeyEvent(cursor);
            return true;
        }
        cursor = (cursor + 1) % capacity;
    }
    return false;
}

static bool XapiDropOldestQueuedPress() {
    int32_t capacity = XapiKeyQueueCapacity();
    int32_t cursor = g_keyQueueTail;
    while (cursor != g_keyQueueHead) {
        if (g_keyQueue[cursor].pressed) {
            XapiRemoveQueuedKeyEvent(cursor);
            return true;
        }
        cursor = (cursor + 1) % capacity;
    }
    return false;
}

static void XapiQueueKeyEvent(int32_t key, bool pressed) {
    XapiLockKeyQueue();

    if (key >= 0 && key < 256) {
        if (g_keyDownState[key] == pressed) {
            XapiUnlockKeyQueue();
            return;
        }
        g_keyDownState[key] = pressed;
    }

    int32_t capacity = XapiKeyQueueCapacity();
    int32_t next = (g_keyQueueHead + 1) % capacity;
    if (next == g_keyQueueTail) {
        if (!pressed && XapiRemovePendingPressForKey(key)) {
            next = (g_keyQueueHead + 1) % capacity;
        } else if (!XapiDropOldestQueuedPress()) {
            if (pressed) {
                XapiUnlockKeyQueue();
                return;
            }
            g_keyQueueTail = (g_keyQueueTail + 1) % capacity;
        }
        next = (g_keyQueueHead + 1) % capacity;
    }
    g_keyQueue[g_keyQueueHead].key = key;
    g_keyQueue[g_keyQueueHead].pressed = pressed;
    g_keyQueueHead = next;

    XapiUnlockKeyQueue();
}

static int32_t NormalizeXapiKey(UINT64 lData) {
    int32_t key = (int32_t)(lData & 0xFF);

    switch (key) {
    case XAPI_KEY_ESC:
        return 27;
    case XAPI_KEY_BACKSPACE:
        return '\b';
    case XAPI_KEY_TAB:
        return '\t';
    case XAPI_KEY_ENTER:
        return '\n';
    case XAPI_KEY_SPACE:
        return ' ';
    default:
        break;
    }

    return PlatformNormalizeAsciiKey(key);
}

static void XapiMsgHandler(UINT64 type, UINT64 hData, UINT64 lData) {
    if (!g_msgWindow) {
        return;
    }

    (void)hData;
    if (type == MSG_KEYDOWN || type == MSG_KEYUP) {
        int32_t key = NormalizeXapiKey(lData);
        bool pressed = type == MSG_KEYDOWN;
        if (key == 27 && pressed) {
            g_msgWindow->closeRequested = true;
        }

        XapiDebugKey("msg", type, lData, key, pressed);
        XapiQueueKeyEvent(key, pressed);
    } else if (type == MSG_RESIZE) {
        UINT64 width = 0;
        UINT64 height = 0;
        xapi_GetWindowSize(g_msgWindow->handle, &width, &height);
        if (width > 0 && height > 0) {
            g_msgWindow->displayWidth = (int32_t)width;
            g_msgWindow->displayHeight = (int32_t)height;
        }
    }
}

static void *XapiAlloc(uint64_t size) {
    return xapi_AllocateMemory((UINT64)size);
}

static void XapiFree(void *ptr) {
    if (ptr) {
        xapi_FreeMemory(ptr);
    }
}

static bool XapiGetApplicationBasePath(char *buffer, uint64_t bufferSize) {
    if (buffer && bufferSize > 0) buffer[0] = '\0';
    if (!buffer || bufferSize == 0) return false;
    char executablePath[4096];
    ssize_t length = readlink("/proc/self/exe", executablePath, sizeof(executablePath) - 1);
    if (length < 1 || length >= (ssize_t)(sizeof(executablePath) - 1)) return false;
    executablePath[length] = '\0';
    ssize_t lastSlash = -1;
    for (ssize_t index = 0; index < length; ++index) {
        if (executablePath[index] == '/') lastSlash = index;
    }
    if (lastSlash < 0 || (uint64_t)(lastSlash + 2) > bufferSize) return false;
    for (ssize_t index = 0; index <= lastSlash; ++index) buffer[index] = executablePath[index];
    buffer[lastSlash + 1] = '\0';
    return true;
}

static bool XapiLoadFile(const char *path, FileData *outFile) {
    if (!path || !outFile) {
        return false;
    }

    outFile->handle = nullptr;
    outFile->data = nullptr;
    outFile->length = 0;

    XFILE *file = xapi_OpenFile((WSTR)path);
    if (!file || !file->buffer) {
        if (file) {
            xapi_CloseFile(file);
        }
        return false;
    }

    outFile->handle = file;
    outFile->data = (const uint8_t *)file->buffer;
    outFile->length = file->length;
    return true;
}

static void XapiCloseFile(FileData *file) {
    if (!file || !file->handle) {
        return;
    }

    xapi_CloseFile((XFILE *)file->handle);
    file->handle = nullptr;
    file->data = nullptr;
    file->length = 0;
}

static Window *XapiCreateWindow(const WindowDesc *desc) {
    if (!desc || desc->width <= 0 || desc->height <= 0) {
        return nullptr;
    }

    Window *window = new Window();
    if (!window) {
        return nullptr;
    }

    window->handle = 0;
    window->displayWidth = desc->width;
    window->displayHeight = desc->height;
    window->scaledPixels = nullptr;
    window->scaledWidth = 0;
    window->scaledHeight = 0;
    window->keyCallback = nullptr;
    window->keyUser = nullptr;
    window->closeRequested = false;
    XWINDOW xw;
    xw.width = (UINT32)desc->width;
    xw.height = (UINT32)desc->height;
    xw.title = (WSTR)desc->title;
    xw.sets = (UINT8)desc->flags;
    xapi_CreateWindow(&window->handle, &xw);
    if (!window->handle) {
        delete window;
        return nullptr;
    }

    g_msgWindow = window;
    XapiResetKeyQueue();
    SetMsgPrcor(window->handle, XapiMsgHandler);
    xapi_FlushTime();
    return window;
}

static void XapiSetWindowTitle(Window *window, const char *title) {
    if (!window || !title) {
        return;
    }

    xapi_SetWindowTitle(window->handle, (WSTR)title);
}

static void XapiDestroyWindow(Window *window) {
    if (!window) {
        return;
    }

    if (g_msgWindow == window) {
        g_msgWindow = nullptr;
        XapiResetKeyQueue();
    }
    xapi_CloseWindow(window->handle);
    delete[] window->scaledPixels;
    delete window;
}

static void XapiSetKeyCallback(Window *window, KeyCallback callback, void *user) {
    if (!window) {
        return;
    }

    window->keyCallback = callback;
    window->keyUser = user;
}

static void XapiPollEvents(Window *window) {
    if (!window) {
        return;
    }

    for (;;) {
        XapiLockKeyQueue();
        if (g_keyQueueTail == g_keyQueueHead) {
            XapiUnlockKeyQueue();
            break;
        }
        XapiKeyEvent event = g_keyQueue[g_keyQueueTail];
        g_keyQueueTail = (g_keyQueueTail + 1) % (int32_t)(sizeof(g_keyQueue) / sizeof(g_keyQueue[0]));
        XapiUnlockKeyQueue();

        if (window->keyCallback) {
            XapiDebugKey("poll", 0, 0, event.key, event.pressed);
            window->keyCallback(event.key, event.pressed, window->keyUser);
        }
    }
}

static bool XapiShouldClose(Window *window) {
    return window ? window->closeRequested : true;
}

static double XapiTimeSeconds() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec * 0.000000001;
}

static void XapiSleepMilliseconds(uint64_t milliseconds) {
    xapi_Sleep((UINT64)milliseconds);
}

static void XapiRefreshWindowSize(Window *window) {
    if (!window || !window->handle) {
        return;
    }

    UINT64 width = 0;
    UINT64 height = 0;
    xapi_GetWindowSize(window->handle, &width, &height);
    if (width > 0 && height > 0 &&
        (window->displayWidth != (int32_t)width || window->displayHeight != (int32_t)height)) {
        window->displayWidth = (int32_t)width;
        window->displayHeight = (int32_t)height;
    }
}

static bool XapiEnsureScaleBuffer(Window *window) {
    if (window->scaledPixels && window->scaledWidth == window->displayWidth &&
        window->scaledHeight == window->displayHeight) {
        return true;
    }

    delete[] window->scaledPixels;
    window->scaledPixels = nullptr;
    window->scaledWidth = 0;
    window->scaledHeight = 0;

    if (window->displayWidth <= 0 || window->displayHeight <= 0) {
        return false;
    }

    window->scaledPixels = new ColorA[(unsigned long)window->displayWidth * (unsigned long)window->displayHeight];
    if (!window->scaledPixels) {
        return false;
    }

    window->scaledWidth = window->displayWidth;
    window->scaledHeight = window->displayHeight;
    return true;
}

static const ColorA *XapiScaleToWindow(Window *window, int32_t width, int32_t height, const ColorA *pixels) {
    if (width == window->displayWidth && height == window->displayHeight) {
        return pixels;
    }

    if (!XapiEnsureScaleBuffer(window)) {
        return nullptr;
    }

    if (window->displayWidth == width * 2 && window->displayHeight == height * 2) {
        for (int y = 0; y < height; y++) {
            const ColorA *src = pixels + (long long)y * width;
            ColorA *dst0 = window->scaledPixels + (long long)(y * 2) * window->displayWidth;
            ColorA *dst1 = dst0 + window->displayWidth;
            for (int x = 0; x < width; x++) {
                ColorA c = src[x];
                int dx = x * 2;
                dst0[dx] = c;
                dst0[dx + 1] = c;
                dst1[dx] = c;
                dst1[dx + 1] = c;
            }
        }
        return window->scaledPixels;
    }

    for (int y = 0; y < window->displayHeight; y++) {
        int srcY = (int)(((long long)y * height) / window->displayHeight);
        ColorA *dst = window->scaledPixels + (long long)y * window->displayWidth;
        const ColorA *src = pixels + (long long)srcY * width;
        for (int x = 0; x < window->displayWidth; x++) {
            int srcX = (int)(((long long)x * width) / window->displayWidth);
            dst[x] = src[srcX];
        }
    }

    return window->scaledPixels;
}

static void XapiPresent(Window *window, int32_t width, int32_t height, const ColorA *pixels) {
    if (!window || !pixels || width <= 0 || height <= 0) {
        return;
    }

    XapiRefreshWindowSize(window);
    const ColorA *presentPixels = XapiScaleToWindow(window, width, height, pixels);
    if (!presentPixels) {
        return;
    }

    xapi_WriteBufferA(window->handle, 0, 0, (UINT32)window->displayWidth,
                      (UINT32)window->displayHeight, (XCOLORA *)presentPixels);
    xapi_RefreshWindow(window->handle);
}

static const Platform g_xapiPlatform = {
    XapiAlloc,
    XapiFree,
    XapiLoadFile,
    XapiCloseFile,
    XapiCreateWindow,
    XapiSetWindowTitle,
    XapiDestroyWindow,
    XapiSetKeyCallback,
    XapiPollEvents,
    XapiShouldClose,
    XapiTimeSeconds,
    XapiSleepMilliseconds,
    XapiPresent,
    XapiGetApplicationBasePath};

const Platform *GetBuiltinPlatform() {
    return &g_xapiPlatform;
}

} // namespace HE3D
