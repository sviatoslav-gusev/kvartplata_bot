#pragma once

#include "Bot.h"
#include "Logger.h"
#include "UserConfig.h"
#include "Scheduler.h"
#include "StrongTypes.h"

#include "libs/json_fwd.hpp"
#include <tgbot/Bot.h>
#include <filesystem>
#include <unordered_map>
#include <string_view>
#include <mutex>

namespace kbot {

class Application {
public:
    Application();
    ~Application();

    bool has_config(UserID user_id) const;
    void set_config(UserConfig cfg);
    UserConfig & get_config(UserID user_id);

    Bot & bot() { return m_bot; }

    void save_configs();

private:
    void load_configs();

    nlohmann::json serialize() const;
    bool deserialize(const nlohmann::json & j);

private:
    Logger m_log;

    std::mutex m_file_mtx;

    uint8_t m_configs_version {1};
    std::unordered_map<UserID, UserConfig> m_configs;

    Scheduler m_sch;
    Bot m_bot;

    static constexpr std::string_view m_filename{"kvartplata_bot.cfg"};
    static constexpr std::string_view m_filename_tmp{"kvartplata_bot.cfg.tmp"};
};

} // namespace kbot