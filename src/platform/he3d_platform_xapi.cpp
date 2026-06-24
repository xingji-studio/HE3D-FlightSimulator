#include "he3d_platform.hpp"
#include "x3api.h"
#include <time.h>

#ifndef MSG_KEYUP
#define MSG_KEYUP   9
#endif
#ifndef MSG_KEYDOWN
#define MSG_KEYDOWN 10
#endif

namespace HE3D {

struct Window {
    HDLE handle;
    int displayWidth;
    int displayHeight;
    ColorA *scaledPixels;
    int scaledWidth;
    int scaledHeight;
    KeyCallback keyCallback;
    void *keyUser;
    bool closeRequested;
    bool needsFlushTimeEventPump;
    bool hasKeyUpMessages;
    bool keyDown[256];
    double keyLastSeen[256];
};

static Window *g_msgWindow = nullptr;

static bool ContainsText(const char *text, const char *needle)
{
    if (!text || !needle || !*needle)
    {
        return false;
    }

    for (const char *p = text; *p; ++p)
    {
        const char *a = p;
        const char *b = needle;
        while (*a && *b && *a == *b)
        {
            ++a;
            ++b;
        }
        if (!*b)
        {
            return true;
        }
    }
    return false;
}

static bool RunningUnderXswl()
{
    char version[64];
    for (unsigned int i = 0; i < sizeof(version); i++)
    {
        version[i] = 0;
    }
    xapi_GetSystemVersion(version);
    return ContainsText(version, "Emulator");
}

static double XapiNowSeconds()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec * 0.000000001;
}

static int NormalizeXapiKey(UINT64 type, UINT64 lData)
{
    int key = (int)(lData & 0xFF);

    if (type == MSG_SPCHAR)
    {
        switch (key)
        {
            case 128: return 27;   /* XKEY_ESC */
            case 130: return '\n'; /* XSWL legacy Enter */
            case '\b': return '\b';
            default: break;
        }
    }

    switch (key)
    {
        case 128: return 27;   /* XKEY_ESC */
        case 129: return '\b'; /* XKEY_BACKSPACE */
        case 130: return '\t'; /* XKEY_TAB */
        case 131: return '\n'; /* XKEY_ENTER */
        default: break;
    }

    return key;
}

static void XapiMsgHandler(UINT64 type, UINT64 hData, UINT64 lData)
{
    if (!g_msgWindow || !g_msgWindow->keyCallback)
    {
        return;
    }

    (void)hData;
    if (type == MSG_KEYDOWN || type == MSG_KEYUP)
    {
        int key = NormalizeXapiKey(type, lData);
        bool pressed = type == MSG_KEYDOWN;
        if (key == 27 && pressed)
        {
            g_msgWindow->closeRequested = true;
        }
        if (key >= 0 && key < 256)
        {
            g_msgWindow->hasKeyUpMessages = true;
            g_msgWindow->keyDown[key] = pressed;
            g_msgWindow->keyLastSeen[key] = pressed ? XapiNowSeconds() : 0.0;
        }
        g_msgWindow->keyCallback(key, pressed, g_msgWindow->keyUser);
        return;
    }

    if (type == MSG_CHAR || type == MSG_SPCHAR)
    {
        if (g_msgWindow->hasKeyUpMessages)
        {
            return;
        }

        int key = NormalizeXapiKey(type, lData);
        if (key == 27)
        {
            g_msgWindow->closeRequested = true;
        }
        if (key >= 0 && key < 256 && !g_msgWindow->keyDown[key])
        {
            g_msgWindow->keyDown[key] = true;
            g_msgWindow->keyLastSeen[key] = XapiNowSeconds();
            g_msgWindow->keyCallback(key, true, g_msgWindow->keyUser);
        }
    }
}

static void *XapiAlloc(unsigned long size)
{
    return xapi_AllocateMemory((UINT64)size);
}

static void XapiFree(void *ptr)
{
    if (ptr)
    {
        xapi_FreeMemory(ptr);
    }
}

static bool XapiLoadFile(const char *path, FileData *outFile)
{
    if (!outFile)
    {
        return false;
    }

    outFile->handle = nullptr;
    outFile->data = nullptr;
    outFile->length = 0;

    XFILE *file = xapi_OpenFile((WSTR)path);
    if (!file || !file->buffer)
    {
        if (file)
        {
            xapi_CloseFile(file);
        }
        return false;
    }

    outFile->handle = file;
    outFile->data = (const unsigned char *)file->buffer;
    outFile->length = file->length;
    return true;
}

static void XapiCloseFile(FileData *file)
{
    if (!file || !file->handle)
    {
        return;
    }

    xapi_CloseFile((XFILE *)file->handle);
    file->handle = nullptr;
    file->data = nullptr;
    file->length = 0;
}

static Window *XapiCreateWindow(const WindowDesc *desc)
{
    if (!desc)
    {
        return nullptr;
    }

    Window *window = new Window();
    if (!window)
    {
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
    window->needsFlushTimeEventPump = RunningUnderXswl();
    window->hasKeyUpMessages = false;
    for (int i = 0; i < 256; i++)
    {
        window->keyDown[i] = false;
        window->keyLastSeen[i] = 0.0;
    }

    XWINDOW xw;
    xw.width = (UINT32)desc->width;
    xw.height = (UINT32)desc->height;
    xw.title = (WSTR)desc->title;
    xw.sets = (UINT8)desc->flags;
    xapi_CreateWindow(&window->handle, &xw);

    g_msgWindow = window;
    SetMsgPrcor(window->handle, XapiMsgHandler);
    return window;
}

static void XapiSetWindowTitle(Window *window, const char *title)
{
    if (!window || !title)
    {
        return;
    }

    xapi_SetWindowTitle(window->handle, (WSTR)title);
}

static void XapiDestroyWindow(Window *window)
{
    if (!window)
    {
        return;
    }

    if (g_msgWindow == window)
    {
        g_msgWindow = nullptr;
    }
    xapi_CloseWindow(window->handle);
    delete[] window->scaledPixels;
    delete window;
}

static void XapiSetKeyCallback(Window *window, KeyCallback callback, void *user)
{
    if (!window)
    {
        return;
    }

    window->keyCallback = callback;
    window->keyUser = user;
}

static void XapiPollEvents(Window *window)
{
    if (window && window->needsFlushTimeEventPump)
    {
        xapi_FlushTime();
    }

    if (window && window->keyCallback)
    {
        double now = XapiNowSeconds();
        const double release_timeout = window->hasKeyUpMessages ? 0.35 : 0.20;
        for (int i = 0; i < 256; i++)
        {
            if (window->keyDown[i] && (now - window->keyLastSeen[i]) > release_timeout)
            {
                window->keyDown[i] = false;
                window->keyCallback(i, false, window->keyUser);
            }
        }
    }
}

static bool XapiShouldClose(Window *window)
{
    return window ? window->closeRequested : true;
}

static double XapiTimeSeconds()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec * 0.000000001;
}

static void XapiSleepMilliseconds(unsigned long long milliseconds)
{
    xapi_Sleep((UINT64)milliseconds);
}

static bool XapiEnsureScaleBuffer(Window *window)
{
    if (window->scaledPixels && window->scaledWidth == window->displayWidth &&
        window->scaledHeight == window->displayHeight)
    {
        return true;
    }

    delete[] window->scaledPixels;
    window->scaledPixels = nullptr;
    window->scaledWidth = 0;
    window->scaledHeight = 0;

    if (window->displayWidth <= 0 || window->displayHeight <= 0)
    {
        return false;
    }

    window->scaledPixels = new ColorA[(unsigned long)window->displayWidth *
                                      (unsigned long)window->displayHeight];
    if (!window->scaledPixels)
    {
        return false;
    }

    window->scaledWidth = window->displayWidth;
    window->scaledHeight = window->displayHeight;
    return true;
}

static const ColorA *XapiScaleToWindow(Window *window, int width, int height, const ColorA *pixels)
{
    if (width == window->displayWidth && height == window->displayHeight)
    {
        return pixels;
    }

    if (!XapiEnsureScaleBuffer(window))
    {
        return nullptr;
    }

    if (window->displayWidth == width * 2 && window->displayHeight == height * 2)
    {
        for (int y = 0; y < height; y++)
        {
            const ColorA *src = pixels + (long long)y * width;
            ColorA *dst0 = window->scaledPixels + (long long)(y * 2) * window->displayWidth;
            ColorA *dst1 = dst0 + window->displayWidth;
            for (int x = 0; x < width; x++)
            {
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

    for (int y = 0; y < window->displayHeight; y++)
    {
        int srcY = (int)(((long long)y * height) / window->displayHeight);
        ColorA *dst = window->scaledPixels + (long long)y * window->displayWidth;
        const ColorA *src = pixels + (long long)srcY * width;
        for (int x = 0; x < window->displayWidth; x++)
        {
            int srcX = (int)(((long long)x * width) / window->displayWidth);
            dst[x] = src[srcX];
        }
    }

    return window->scaledPixels;
}

static void XapiPresent(Window *window, int width, int height, const ColorA *pixels)
{
    if (!window || !pixels || width <= 0 || height <= 0)
    {
        return;
    }

    const ColorA *presentPixels = XapiScaleToWindow(window, width, height, pixels);
    if (!presentPixels)
    {
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
    XapiPresent
};

const Platform *GetBuiltinPlatform()
{
    return &g_xapiPlatform;
}

}
