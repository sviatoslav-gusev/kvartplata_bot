#include <csignal>
#include <chrono>
#include <thread>
#include "Application.h"

namespace {
    volatile std::sig_atomic_t g_stop = 0;

    void on_signal(int) noexcept {
#ifdef _WIN32
        // Windows resets handled value after each usage, so reassign that
        std::signal(SIGINT,  on_signal);
        std::signal(SIGTERM, on_signal);
    #ifdef SIGBREAK
        std::signal(SIGBREAK, on_signal);
    #endif
#endif
        g_stop = 1;
    }
}

int main() {
    std::signal(SIGINT,  on_signal);
    std::signal(SIGTERM, on_signal);
#ifdef SIGBREAK
    std::signal(SIGBREAK, on_signal);
#endif

    kbot::Application app;

    app.bot().run_until([&]{ return g_stop != 0; });

    return 0;
}