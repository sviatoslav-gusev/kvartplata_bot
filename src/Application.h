#pragma once

#include "Bot.h"
#include "Logger.h"

#include "storages/PathsStorage.h"
#include "storages/ConfigStorage.h"
#include "Scheduler.h"
#include "StrongTypes.h"

#include <tgbot/Bot.h>

#include <string_view>
#include <mutex>

namespace kbot {

class Application {
public:
    Application(int argc, char** argv);

    Bot & bot() { return m_bot; }
    ConfigStorage & cfg() { return m_cfg; }
    const ConfigStorage & cfg() const { return m_cfg; }

private:
    PathsStorage m_paths;
    Logger m_log;
    ConfigStorage m_cfg;

    Scheduler m_sch;
    Bot m_bot;
};

} // namespace kbot