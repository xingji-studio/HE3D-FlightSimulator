#pragma once

namespace HE3D {

typedef __INT8_TYPE__ int8_t;
typedef __INT16_TYPE__ int16_t;
typedef __INT32_TYPE__ int32_t;
typedef __INT64_TYPE__ int64_t;
typedef __UINT8_TYPE__ uint8_t;
typedef __UINT16_TYPE__ uint16_t;
typedef __UINT32_TYPE__ uint32_t;
typedef __UINT64_TYPE__ uint64_t;

struct ColorA {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

typedef char he3d_colora_must_be_4_bytes[(sizeof(ColorA) == 4) ? 1 : -1];

struct FileData {
    void *handle;
    const uint8_t *data;
    uint64_t length;
};

struct Window;

struct WindowDesc {
    int32_t width;
    int32_t height;
    const char *title;
    uint32_t flags;
};

typedef void (*KeyCallback)(int32_t key, bool pressed, void *user);

struct Platform {
    void *(*alloc)(uint64_t size);
    void (*free)(void *ptr);

    bool (*loadFile)(const char *path, FileData *outFile);
    void (*closeFile)(FileData *file);

    Window *(*createWindow)(const WindowDesc *desc);
    void (*setWindowTitle)(Window *window, const char *title);
    void (*destroyWindow)(Window *window);
    void (*setKeyCallback)(Window *window, KeyCallback callback, void *user);
    void (*pollEvents)(Window *window);
    bool (*shouldClose)(Window *window);
    double (*timeSeconds)();
    void (*sleepMilliseconds)(uint64_t milliseconds);
    void (*present)(Window *window, int32_t width, int32_t height, const ColorA *pixels);
    bool (*getApplicationBasePath)(char *buffer, uint64_t bufferSize);
};

const Platform *GetPlatform();
void SetPlatform(const Platform *platform);
const Platform *GetBuiltinPlatform();

void *Alloc(uint64_t size);
void Free(void *ptr);
void SetFrameRateLimit(uint32_t fps);
uint32_t GetFrameRateLimit();
void SetFxaaEnabled(bool enabled);
bool IsFxaaEnabled();
void SetTaaEnabled(bool enabled);
bool IsTaaEnabled();
void SetMsaaEnabled(bool enabled);
bool IsMsaaEnabled();
void SetSsaaScale(uint32_t scale);
uint32_t GetSsaaScale();
void PaceFrame(double frameStart);

inline bool LoadFile(const char *path, FileData *outFile) {
    return GetPlatform()->loadFile(path, outFile);
}

inline bool GetApplicationBasePath(char *buffer, uint64_t bufferSize) {
    return GetPlatform()->getApplicationBasePath(buffer, bufferSize);
}

inline void CloseFile(FileData *file) {
    GetPlatform()->closeFile(file);
}

inline Window *CreateWindow(const WindowDesc *desc) {
    return GetPlatform()->createWindow(desc);
}

inline void SetWindowTitle(Window *window, const char *title) {
    GetPlatform()->setWindowTitle(window, title);
}

inline void DestroyWindow(Window *window) {
    GetPlatform()->destroyWindow(window);
}

inline void SetKeyCallback(Window *window, KeyCallback callback, void *user) {
    GetPlatform()->setKeyCallback(window, callback, user);
}

inline void PollEvents(Window *window) {
    GetPlatform()->pollEvents(window);
}

inline bool WindowShouldClose(Window *window) {
    return GetPlatform()->shouldClose(window);
}

inline double TimeSeconds() {
    return GetPlatform()->timeSeconds();
}

inline void SleepMilliseconds(uint64_t milliseconds) {
    GetPlatform()->sleepMilliseconds(milliseconds);
}

inline void Present(Window *window, int32_t width, int32_t height, const ColorA *pixels) {
    GetPlatform()->present(window, width, height, pixels);
}

} // namespace HE3D
