#include "he3d_platform.hpp"
#include "he3d_platform_input.hpp"

#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <string>
#include <termios.h>
#include <thread>
#include <unistd.h>

namespace HE3D {

// Backend-private Window implementation; he3d_platform.hpp only exposes Window as an opaque handle.
// 后端私有的 Window 实现；he3d_platform.hpp 只把 Window 暴露为不透明句柄。
struct Window {
    int32_t width;
    int32_t height;
    int32_t presentColumns;
    int32_t presentRows;
    std::string title;
    KeyCallback keyCallback;
    void *keyUser;
    bool closeRequested;
    bool firstPresent;
    bool terminalConfigured;
    termios originalTermios;
    int originalStdinFlags;
    bool keyDown[256];
    double keyLastSeen[256];
    bool escapePending;
    bool escapeSequenceActive;
    double escapePendingTime;
};

static double ConsoleTimeSeconds();
static void RestoreConsoleInput(Window *window);
static Window *g_activeWindow = nullptr;

static void ConsoleSignalHandler(int) {
    RestoreConsoleInput(g_activeWindow);
    std::cout << "\033[0m\033[?25h" << std::flush;
    std::_Exit(130);
}

// Normalize terminal bytes to the same small key range used by SDL/XAPI backends.
// 将终端字节规整到 SDL/XAPI 后端使用的小范围键值。
static int32_t NormalizeConsoleKey(int32_t key) {
    if (key == 3) {
        return 27;
    }
    return PlatformNormalizeAsciiKey(key);
}

static bool IsConsoleEscapeLead(int32_t key) {
    return key == '[' || key == 'O';
}

static bool IsConsoleEscapeFinal(int32_t key) {
    return key >= 0x40 && key <= 0x7E;
}

static void ConsoleDispatchKey(Window *window, int32_t key, bool pressed) {
    if (!window || !window->keyCallback) {
        return;
    }

    window->keyCallback(key, pressed, window->keyUser);
}

static void ConsolePressKey(Window *window, int32_t key, double now) {
    if (!window) {
        return;
    }

    key = NormalizeConsoleKey(key);
    if (key >= 0 && key < 256) {
        window->keyLastSeen[key] = now;
        if (!window->keyDown[key]) {
            window->keyDown[key] = true;
            ConsoleDispatchKey(window, key, true);
        }
    } else {
        ConsoleDispatchKey(window, key, true);
        ConsoleDispatchKey(window, key, false);
    }
}

static void ConsoleFlushPendingEscape(Window *window, double now) {
    if (!window || !window->escapePending) {
        return;
    }

    window->escapePending = false;
    window->closeRequested = true;
    ConsolePressKey(window, 27, now);
}

// Restore terminal mode before returning to the shell.
// 回到 shell 前恢复终端模式。
static void RestoreConsoleInput(Window *window) {
    if (!window || !window->terminalConfigured) {
        return;
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &window->originalTermios);
    fcntl(STDIN_FILENO, F_SETFL, window->originalStdinFlags);
    window->terminalConfigured = false;
}

// Put stdin into nonblocking raw mode so PollEvents can read keys without stalling rendering.
// 将 stdin 切到非阻塞 raw 模式，让 PollEvents 读按键时不阻塞渲染。
static bool ConfigureConsoleInput(Window *window) {
    if (!window || !isatty(STDIN_FILENO)) {
        return false;
    }

    if (tcgetattr(STDIN_FILENO, &window->originalTermios) != 0) {
        return false;
    }

    window->originalStdinFlags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (window->originalStdinFlags < 0) {
        return false;
    }

    termios raw = window->originalTermios;
    raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    raw.c_iflag &= (tcflag_t) ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
        return false;
    }

    if (fcntl(STDIN_FILENO, F_SETFL, window->originalStdinFlags | O_NONBLOCK) != 0) {
        tcsetattr(STDIN_FILENO, TCSANOW, &window->originalTermios);
        return false;
    }

    window->terminalConfigured = true;
    return true;
}

// Platform allocation entry points use the host process heap.
// 平台内存分配入口使用宿主进程堆。

static void *ConsoleAlloc(uint64_t size) {
    return std::malloc((unsigned long)size);
}

static void ConsoleFree(void *ptr) {
    std::free(ptr);
}

static bool ConsoleLoadFile(const char *path, FileData *outFile) {
    if (!path || !outFile) {
        return false;
    }

    outFile->handle = nullptr;
    outFile->data = nullptr;
    outFile->length = 0;

    // Open at end first so tellg() gives the byte length.
    // 先从文件末尾打开，这样 tellg() 可以直接得到完整字节长度。
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }

    std::ifstream::pos_type endPos = file.tellg();
    if (endPos <= 0) {
        return false;
    }

    uint64_t size = (uint64_t)endPos;
    uint8_t *data = (uint8_t *)std::malloc((unsigned long)size);
    if (!data) {
        return false;
    }

    file.seekg(0, std::ios::beg);
    file.read((char *)data, (std::streamsize)size);
    if (!file) {
        std::free(data);
        return false;
    }

    // handle owns the allocation; data is the caller-visible byte view.
    // handle 持有需要释放的分配；data 是调用者读取文件内容的只读视图。
    outFile->handle = data;
    outFile->data = data;
    outFile->length = size;
    return true;
}

static void ConsoleCloseFile(FileData *file) {
    if (!file) {
        return;
    }

    std::free(file->handle);
    file->handle = nullptr;
    file->data = nullptr;
    file->length = 0;
}

static Window *ConsoleCreateWindow(const WindowDesc *desc) {
    if (!desc || desc->width <= 0 || desc->height <= 0) {
        return nullptr;
    }

    Window *window = new Window();
    if (!window) {
        return nullptr;
    }

    window->width = desc->width;
    window->height = desc->height;
    // Terminals are character grids, so cap output size to keep the demo readable and avoid flooding scrollback.
    // 终端是字符网格，所以限制输出尺寸，避免示例撑爆终端或疯狂刷滚动缓冲区。
    window->presentColumns = desc->width < 80 ? desc->width : 80;
    window->presentRows = (window->presentColumns * desc->height + desc->width - 1) / desc->width;
    window->presentRows = (window->presentRows + 1) / 2;
    if (window->presentRows > 30) {
        window->presentRows = 30;
        window->presentColumns = (window->presentRows * 2 * desc->width) / desc->height;
        if (window->presentColumns > 80) {
            window->presentColumns = 80;
        }
    }
    if (window->presentColumns <= 0) {
        window->presentColumns = 1;
    }
    if (window->presentRows <= 0) {
        window->presentRows = 1;
    }
    window->title = desc->title ? desc->title : "";
    window->keyCallback = nullptr;
    window->keyUser = nullptr;
    window->closeRequested = false;
    window->firstPresent = true;
    window->terminalConfigured = false;
    window->originalStdinFlags = -1;
    for (int32_t i = 0; i < 256; i++) {
        window->keyDown[i] = false;
        window->keyLastSeen[i] = 0.0;
    }
    window->escapePending = false;
    window->escapeSequenceActive = false;
    window->escapePendingTime = 0.0;
    ConfigureConsoleInput(window);
    g_activeWindow = window;
    signal(SIGINT, ConsoleSignalHandler);
    signal(SIGTERM, ConsoleSignalHandler);

    // Clear the terminal, move the cursor home, and hide the cursor during presentation.
    // 清空终端、把光标移到左上角，并在显示帧时隐藏光标。
    std::cout << "\033[2J\033[H\033[?25l";
    if (!window->title.empty()) {
        // OSC 0 changes the terminal window title when supported.
        // OSC 0 在终端支持时修改终端窗口标题。
        std::cout << "\033]0;" << window->title << "\007";
    }
    std::cout.flush();
    return window;
}

static void ConsoleSetWindowTitle(Window *window, const char *title) {
    if (!window) {
        return;
    }

    window->title = title ? title : "";
    std::cout << "\033]0;" << window->title << "\007";
    std::cout.flush();
}

static void ConsoleDestroyWindow(Window *window) {
    if (!window) {
        return;
    }

    // Reset colors and restore the terminal before returning to the shell.
    // 回到 shell 前重置颜色并恢复终端。
    std::cout << "\033[0m\033[?25h" << std::endl;
    RestoreConsoleInput(window);
    if (g_activeWindow == window) {
        g_activeWindow = nullptr;
    }
    delete window;
}

static void ConsoleSetKeyCallback(Window *window, KeyCallback callback, void *user) {
    if (!window) {
        return;
    }

    window->keyCallback = callback;
    window->keyUser = user;
}

// Poll raw terminal bytes and synthesize key-up because normal terminals do not report release events.
// 轮询 raw 终端字节，并合成 key-up，因为普通终端不会报告松开事件。
static void ConsolePollEvents(Window *window) {
    if (!window) {
        return;
    }
    if (!window->terminalConfigured) {
        return;
    }

    const double now = ConsoleTimeSeconds();
    unsigned char buffer[64];

    for (;;) {
        ssize_t count = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (count > 0) {
            for (ssize_t i = 0; i < count; i++) {
                int32_t rawKey = (int32_t)buffer[i];
                if (window->escapeSequenceActive) {
                    if (IsConsoleEscapeFinal(rawKey)) {
                        window->escapeSequenceActive = false;
                    }
                    continue;
                }

                if (window->escapePending) {
                    if (IsConsoleEscapeLead(rawKey)) {
                        window->escapePending = false;
                        window->escapeSequenceActive = true;
                        continue;
                    }

                    ConsoleFlushPendingEscape(window, now);
                }

                if (rawKey == 27) {
                    window->escapePending = true;
                    window->escapePendingTime = now;
                    continue;
                }

                ConsolePressKey(window, rawKey, now);
            }
            continue;
        }

        if (count == 0 || errno == EAGAIN || errno == EWOULDBLOCK) {
            break;
        }

        break;
    }

    if (window->escapePending && now - window->escapePendingTime > 0.03) {
        ConsoleFlushPendingEscape(window, now);
    }

    const double releaseDelay = 0.35;
    for (int32_t key = 0; key < 256; key++) {
        if (window->keyDown[key] && now - window->keyLastSeen[key] > releaseDelay) {
            window->keyDown[key] = false;
            if (window->keyCallback) {
                window->keyCallback(key, false, window->keyUser);
            }
        }
    }
}

static bool ConsoleShouldClose(Window *window) {
    return window ? window->closeRequested : true;
}

static double ConsoleTimeSeconds() {
    // steady_clock is monotonic, so frame timing is not affected by wall-clock changes.
    // steady_clock 是单调时钟，帧计时不会被系统时间变化影响。
    typedef std::chrono::steady_clock Clock;
    static const Clock::time_point start = Clock::now();
    std::chrono::duration<double> elapsed = Clock::now() - start;
    return elapsed.count();
}

static void ConsoleSleepMilliseconds(uint64_t milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

static void WriteFg(std::ostringstream &out, const ColorA &color) {
    // ANSI 24-bit foreground color: ESC[38;2;<r>;<g>;<b>m
    // ANSI 24 位前景色：ESC[38;2;<r>;<g>;<b>m
    out << "\033[38;2;" << (int)color.r << ';' << (int)color.g << ';' << (int)color.b << 'm';
}

static void WriteBg(std::ostringstream &out, const ColorA &color) {
    // ANSI 24-bit background color: ESC[48;2;<r>;<g>;<b>m
    // ANSI 24 位背景色：ESC[48;2;<r>;<g>;<b>m
    out << "\033[48;2;" << (int)color.r << ';' << (int)color.g << ';' << (int)color.b << 'm';
}

static bool SameColor(const ColorA &a, const ColorA &b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static void ConsolePresent(Window *window, int32_t width, int32_t height, const ColorA *pixels) {
    if (!window || !pixels || width <= 0 || height <= 0) {
        return;
    }

    if (window->firstPresent) {
        std::cout << "\033[2J";
        window->firstPresent = false;
    }

    // Each terminal cell displays two pixels with foreground/background colors and U+2580.
    // 每个终端字符格显示两个像素：前景色是上方像素，背景色是下方像素，U+2580 画上半格。
    const ColorA black = {0, 0, 0, 255};
    int outColumns = window->presentColumns;
    int outRows = window->presentRows;
    int sampleHeight = outRows * 2;

    // Move back to the top-left and overwrite the previous frame in place.
    // 回到左上角，直接覆盖上一帧内容。
    std::ostringstream frame;
    frame << "\033[H";
    ColorA currentFg = {0, 0, 0, 0};
    ColorA currentBg = {0, 0, 0, 0};
    bool hasFg = false;
    bool hasBg = false;
    for (int y = 0; y < outRows; y++) {
        // Nearest-neighbor scaling from renderer pixels to terminal cells.
        // 从渲染器像素到终端字符格使用最近邻缩放。
        int srcTopY = ((y * 2) * height) / sampleHeight;
        int srcBottomY = ((y * 2 + 1) * height) / sampleHeight;
        if (srcTopY >= height) {
            srcTopY = height - 1;
        }
        if (srcBottomY >= height) {
            srcBottomY = height - 1;
        }

        const ColorA *topRow = pixels + srcTopY * width;
        const ColorA *bottomRow = srcBottomY >= 0 ? pixels + srcBottomY * width : nullptr;
        for (int x = 0; x < outColumns; x++) {
            int srcX = (x * width) / outColumns;
            if (srcX >= width) {
                srcX = width - 1;
            }

            const ColorA &top = topRow[srcX];
            const ColorA &bottom = bottomRow ? bottomRow[srcX] : black;
            if (!hasFg || !SameColor(currentFg, top)) {
                WriteFg(frame, top);
                currentFg = top;
                hasFg = true;
            }
            if (!hasBg || !SameColor(currentBg, bottom)) {
                WriteBg(frame, bottom);
                currentBg = bottom;
                hasBg = true;
            }
            // UTF-8 encoding of U+2580.
            // U+2580 的 UTF-8 编码。
            frame << "\xE2\x96\x80";
        }
        frame << "\033[0m\n";
        hasFg = false;
        hasBg = false;
    }
    std::cout << frame.str();
    std::cout.flush();
}

// The Platform table is the backend boundary; HE3D wrappers dispatch to these function pointers.
// Platform 表就是后端边界；HE3D 包装函数最终会分发到这些函数指针。
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
    ConsoleSleepMilliseconds,
    ConsolePresent};

const Platform *GetBuiltinPlatform() {
    // Returning this table makes the console backend the default Platform for this target.
    // 返回这个表后，控制台后端就是当前目标的默认 Platform。
    return &g_consolePlatform;
}

} // namespace HE3D
