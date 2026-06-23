#include "he3d_platform.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace HE3D {

struct Window {
    int width;
    int height;
    int presentColumns;
    int presentRows;
    std::string title;
    KeyCallback keyCallback;
    void *keyUser;
    bool closeRequested;
    bool firstPresent;
};

static void *ConsoleAlloc(unsigned long size)
{
    return std::malloc(size);
}

static void ConsoleFree(void *ptr)
{
    std::free(ptr);
}

static bool ConsoleLoadFile(const char *path, FileData *outFile)
{
    if (!path || !outFile)
    {
        return false;
    }

    outFile->handle = nullptr;
    outFile->data = nullptr;
    outFile->length = 0;

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        return false;
    }

    std::ifstream::pos_type endPos = file.tellg();
    if (endPos <= 0)
    {
        return false;
    }

    unsigned long long size = (unsigned long long)endPos;
    unsigned char *data = (unsigned char *)std::malloc((unsigned long)size);
    if (!data)
    {
        return false;
    }

    file.seekg(0, std::ios::beg);
    file.read((char *)data, (std::streamsize)size);
    if (!file)
    {
        std::free(data);
        return false;
    }

    outFile->handle = data;
    outFile->data = data;
    outFile->length = size;
    return true;
}

static void ConsoleCloseFile(FileData *file)
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

static Window *ConsoleCreateWindow(const WindowDesc *desc)
{
    if (!desc || desc->width <= 0 || desc->height <= 0)
    {
        return nullptr;
    }

    Window *window = new Window();
    if (!window)
    {
        return nullptr;
    }

    window->width = desc->width;
    window->height = desc->height;
    window->presentColumns = desc->width < 80 ? desc->width : 80;
    window->presentRows = ((desc->height + 1) / 2) < 30 ? ((desc->height + 1) / 2) : 30;
    if (window->presentColumns <= 0)
    {
        window->presentColumns = 1;
    }
    if (window->presentRows <= 0)
    {
        window->presentRows = 1;
    }
    window->title = desc->title ? desc->title : "";
    window->keyCallback = nullptr;
    window->keyUser = nullptr;
    window->closeRequested = false;
    window->firstPresent = true;

    std::cout << "\033[2J\033[H\033[?25l";
    if (!window->title.empty())
    {
        std::cout << "\033]0;" << window->title << "\007";
    }
    std::cout.flush();
    return window;
}

static void ConsoleSetWindowTitle(Window *window, const char *title)
{
    if (!window)
    {
        return;
    }

    window->title = title ? title : "";
    std::cout << "\033]0;" << window->title << "\007";
    std::cout.flush();
}

static void ConsoleDestroyWindow(Window *window)
{
    if (!window)
    {
        return;
    }

    std::cout << "\033[0m\033[?25h" << std::endl;
    delete window;
}

static void ConsoleSetKeyCallback(Window *window, KeyCallback callback, void *user)
{
    if (!window)
    {
        return;
    }

    window->keyCallback = callback;
    window->keyUser = user;
}

static void ConsolePollEvents(Window *)
{
}

static bool ConsoleShouldClose(Window *window)
{
    return window ? window->closeRequested : true;
}

static double ConsoleTimeSeconds()
{
    typedef std::chrono::steady_clock Clock;
    static const Clock::time_point start = Clock::now();
    std::chrono::duration<double> elapsed = Clock::now() - start;
    return elapsed.count();
}

static void WriteFg(const ColorA& color)
{
    std::cout << "\033[38;2;" << (int)color.r << ';' << (int)color.g << ';'
              << (int)color.b << 'm';
}

static void WriteBg(const ColorA& color)
{
    std::cout << "\033[48;2;" << (int)color.r << ';' << (int)color.g << ';'
              << (int)color.b << 'm';
}

static void ConsolePresent(Window *window, int width, int height, const ColorA *pixels)
{
    if (!window || !pixels || width <= 0 || height <= 0)
    {
        return;
    }

    if (window->firstPresent)
    {
        std::cout << "\033[2J";
        window->firstPresent = false;
    }

    const ColorA black = {0, 0, 0, 255};
    int outColumns = window->presentColumns;
    int outRows = window->presentRows;
    int sampleHeight = outRows * 2;

    std::cout << "\033[H";
    for (int y = 0; y < outRows; y++)
    {
        int srcTopY = ((y * 2) * height) / sampleHeight;
        int srcBottomY = ((y * 2 + 1) * height) / sampleHeight;
        if (srcTopY >= height)
        {
            srcTopY = height - 1;
        }
        if (srcBottomY >= height)
        {
            srcBottomY = height - 1;
        }

        const ColorA *topRow = pixels + srcTopY * width;
        const ColorA *bottomRow = srcBottomY >= 0 ? pixels + srcBottomY * width : nullptr;
        for (int x = 0; x < outColumns; x++)
        {
            int srcX = (x * width) / outColumns;
            if (srcX >= width)
            {
                srcX = width - 1;
            }

            const ColorA& top = topRow[srcX];
            const ColorA& bottom = bottomRow ? bottomRow[srcX] : black;
            WriteFg(top);
            WriteBg(bottom);
            std::cout << "\xE2\x96\x80";
        }
        std::cout << "\033[0m\n";
    }
    std::cout.flush();
}

static const Platform g_consolePlatform = {
    ConsoleAlloc,
    ConsoleFree,
    ConsoleLoadFile,
    ConsoleCloseFile,
    ConsoleCreateWindow,
    ConsoleSetWindowTitle,
    ConsoleDestroyWindow,
    ConsoleSetKeyCallback,
    ConsolePollEvents,
    ConsoleShouldClose,
    ConsoleTimeSeconds,
    ConsolePresent
};

const Platform *GetBuiltinPlatform()
{
    return &g_consolePlatform;
}

}
