#pragma once

namespace HE3D {

struct ColorA {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

struct FileData {
    void *handle;
    const unsigned char *data;
    unsigned long long length;
};

struct Window;

struct WindowDesc {
    int width;
    int height;
    const char *title;
    unsigned int flags;
};

typedef void (*KeyCallback)(int key, bool pressed, void *user);

struct Platform {
    void *(*alloc)(unsigned long size);
    void  (*free)(void *ptr);

    bool  (*loadFile)(const char *path, FileData *outFile);
    void  (*closeFile)(FileData *file);

    Window *(*createWindow)(const WindowDesc *desc);
    void    (*setWindowTitle)(Window *window, const char *title);
    void    (*destroyWindow)(Window *window);
    void    (*setKeyCallback)(Window *window, KeyCallback callback, void *user);
    void    (*pollEvents)(Window *window);
    bool    (*shouldClose)(Window *window);
    double  (*timeSeconds)();
    void    (*present)(Window *window, int width, int height, const ColorA *pixels);
};

const Platform *GetPlatform();
void SetPlatform(const Platform *platform);
const Platform *GetBuiltinPlatform();

void *Alloc(unsigned long size);
void  Free(void *ptr);

inline bool LoadFile(const char *path, FileData *outFile)
{
    return GetPlatform()->loadFile(path, outFile);
}

inline void CloseFile(FileData *file)
{
    GetPlatform()->closeFile(file);
}

inline Window *CreateWindow(const WindowDesc *desc)
{
    return GetPlatform()->createWindow(desc);
}

inline void SetWindowTitle(Window *window, const char *title)
{
    GetPlatform()->setWindowTitle(window, title);
}

inline void DestroyWindow(Window *window)
{
    GetPlatform()->destroyWindow(window);
}

inline void SetKeyCallback(Window *window, KeyCallback callback, void *user)
{
    GetPlatform()->setKeyCallback(window, callback, user);
}

inline void PollEvents(Window *window)
{
    GetPlatform()->pollEvents(window);
}

inline bool WindowShouldClose(Window *window)
{
    return GetPlatform()->shouldClose(window);
}

inline double TimeSeconds()
{
    return GetPlatform()->timeSeconds();
}

inline void Present(Window *window, int width, int height, const ColorA *pixels)
{
    GetPlatform()->present(window, width, height, pixels);
}

}
