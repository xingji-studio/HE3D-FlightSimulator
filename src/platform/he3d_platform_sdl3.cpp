#include "he3d_platform.hpp"

#include <SDL3/SDL.h>
#include <cstdlib>
#include <cstdio>

namespace HE3D {

struct Window {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    int textureWidth;
    int textureHeight;
    KeyCallback keyCallback;
    void *keyUser;
    bool closeRequested;
};

static void *SdlAlloc(unsigned long size)
{
    return std::malloc(size);
}

static void SdlFree(void *ptr)
{
    std::free(ptr);
}

static bool SdlLoadFile(const char *path, FileData *outFile)
{
    if (!outFile)
    {
        return false;
    }

    outFile->handle = nullptr;
    outFile->data = nullptr;
    outFile->length = 0;

    std::FILE *file = std::fopen(path, "rb");
    if (!file)
    {
        return false;
    }

    if (std::fseek(file, 0, SEEK_END) != 0)
    {
        std::fclose(file);
        return false;
    }

    long size = std::ftell(file);
    if (size <= 0)
    {
        std::fclose(file);
        return false;
    }

    if (std::fseek(file, 0, SEEK_SET) != 0)
    {
        std::fclose(file);
        return false;
    }

    uint8_t *data = (uint8_t *)std::malloc((unsigned long)size);
    if (!data)
    {
        std::fclose(file);
        return false;
    }

    unsigned long readSize = (unsigned long)std::fread(data, 1, (unsigned long)size, file);
    std::fclose(file);
    if (readSize != (unsigned long)size)
    {
        std::free(data);
        return false;
    }

    outFile->handle = data;
    outFile->data = data;
    outFile->length = (unsigned long long)size;
    return true;
}

static void SdlCloseFile(FileData *file)
{
    if (!file)
    {
        return;
    }

    std::free(file->handle);
    file->handle = nullptr;
    file->data = nullptr;
    file->length = 0;
}

static Window *SdlCreateWindow(const WindowDesc *desc)
{
    if (!desc)
    {
        return nullptr;
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        return nullptr;
    }

    Window *window = new Window();
    if (!window)
    {
        SDL_Quit();
        return nullptr;
    }

    window->window = SDL_CreateWindow(desc->title, desc->width, desc->height, (SDL_WindowFlags)desc->flags);
    if (!window->window)
    {
        delete window;
        SDL_Quit();
        return nullptr;
    }

    window->renderer = SDL_CreateRenderer(window->window, nullptr);
    if (!window->renderer)
    {
        SDL_DestroyWindow(window->window);
        delete window;
        SDL_Quit();
        return nullptr;
    }

    window->texture = nullptr;
    window->textureWidth = 0;
    window->textureHeight = 0;
    window->keyCallback = nullptr;
    window->keyUser = nullptr;
    window->closeRequested = false;
    return window;
}

static void SdlSetWindowTitle(Window *window, const char *title)
{
    if (!window || !window->window || !title)
    {
        return;
    }

    SDL_SetWindowTitle(window->window, title);
}

static void SdlDestroyWindow(Window *window)
{
    if (!window)
    {
        return;
    }

    if (window->texture)
    {
        SDL_DestroyTexture(window->texture);
    }
    if (window->renderer)
    {
        SDL_DestroyRenderer(window->renderer);
    }
    if (window->window)
    {
        SDL_DestroyWindow(window->window);
    }
    delete window;
    SDL_Quit();
}

static void SdlSetKeyCallback(Window *window, KeyCallback callback, void *user)
{
    if (!window)
    {
        return;
    }

    window->keyCallback = callback;
    window->keyUser = user;
}

static void SdlPollEvents(Window *window)
{
    if (!window)
    {
        return;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
        {
            window->closeRequested = true;
        }
        else if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)
        {
            bool pressed = event.type == SDL_EVENT_KEY_DOWN;
            int key = (int)event.key.key;
            if (key == 27 && pressed)
            {
                window->closeRequested = true;
            }
            if (window->keyCallback)
            {
                window->keyCallback(key, pressed, window->keyUser);
            }
        }
    }
}

static bool SdlShouldClose(Window *window)
{
    return window ? window->closeRequested : true;
}

static double SdlTimeSeconds()
{
    return (double)SDL_GetTicksNS() * 0.000000001;
}

static void SdlSleepMilliseconds(unsigned long long milliseconds)
{
    SDL_Delay((Uint32)milliseconds);
}

static bool SdlEnsureTexture(Window *window, int width, int height)
{
    if (window->texture && window->textureWidth == width && window->textureHeight == height)
    {
        return true;
    }

    if (window->texture)
    {
        SDL_DestroyTexture(window->texture);
        window->texture = nullptr;
    }

    window->texture = SDL_CreateTexture(window->renderer, SDL_PIXELFORMAT_RGBA32,
                                        SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!window->texture)
    {
        window->textureWidth = 0;
        window->textureHeight = 0;
        return false;
    }

    window->textureWidth = width;
    window->textureHeight = height;
    return true;
}

static void SdlPresent(Window *window, int width, int height, const ColorA *pixels)
{
    if (!window || !pixels || width <= 0 || height <= 0)
    {
        return;
    }

    if (!SdlEnsureTexture(window, width, height))
    {
        return;
    }

    SDL_UpdateTexture(window->texture, nullptr, pixels, width * (int)sizeof(ColorA));
    SDL_RenderClear(window->renderer);
    SDL_RenderTexture(window->renderer, window->texture, nullptr, nullptr);
    SDL_RenderPresent(window->renderer);
}

static const Platform g_sdlPlatform = {
    SdlAlloc,
    SdlFree,
    SdlLoadFile,
    SdlCloseFile,
    SdlCreateWindow,
    SdlSetWindowTitle,
    SdlDestroyWindow,
    SdlSetKeyCallback,
    SdlPollEvents,
    SdlShouldClose,
    SdlTimeSeconds,
    SdlSleepMilliseconds,
    SdlPresent
};

const Platform *GetBuiltinPlatform()
{
    return &g_sdlPlatform;
}

}
