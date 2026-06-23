#include "he3d_platform.hpp"
#include "x3api.h"
#include <time.h>

namespace HE3D {

struct Window {
    HDLE handle;
    KeyCallback keyCallback;
    void *keyUser;
    bool closeRequested;
    bool keyDown[256];
    double keyLastSeen[256];
};

static Window *g_msgWindow = nullptr;

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
            case 128: return 27;   /* ESC */
            case 130: return '\n'; /* Enter */
            case '\b': return '\b';
            default: break;
        }
    }

    return key;
}

static void XapiMsgHandler(UINT64 type, UINT64 hData, UINT64 lData)
{
    if (!g_msgWindow || !g_msgWindow->keyCallback)
    {
        return;
    }

    if (type == MSG_CHAR || type == MSG_SPCHAR)
    {
        (void)hData;
        int key = NormalizeXapiKey(type, lData);
        if (key == 27)
        {
            g_msgWindow->closeRequested = true;
        }
        if (key >= 0 && key < 256)
        {
            g_msgWindow->keyDown[key] = true;
            g_msgWindow->keyLastSeen[key] = XapiNowSeconds();
        }
        g_msgWindow->keyCallback(key, true, g_msgWindow->keyUser);
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
    window->keyCallback = nullptr;
    window->keyUser = nullptr;
    window->closeRequested = false;
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
    xapi_FlushTime();
    if (window && window->keyCallback)
    {
        double now = XapiNowSeconds();
        const double release_timeout = 0.20;
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

static void XapiPresent(Window *window, int width, int height, const ColorA *pixels)
{
    if (!window || !pixels || width <= 0 || height <= 0)
    {
        return;
    }

    xapi_WriteBufferA(window->handle, 0, 0, (UINT32)width, (UINT32)height,
                      (XCOLORA *)pixels);
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
    XapiPresent
};

const Platform *GetBuiltinPlatform()
{
    return &g_xapiPlatform;
}

}
