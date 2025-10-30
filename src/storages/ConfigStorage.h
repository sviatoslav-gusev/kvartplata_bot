#pragma once

#include "PathsStorage.h"
#include "../Logger.h"
#include "../StrongTypes.h"
#include "../UserConfig.h"

#include "../libs/json_fwd.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

namespace kbot {

class ConfigStorage
{
  public:
    ConfigStorage(const PathsStorage & paths, Logger & log);
    ~ConfigStorage();

    inline std::unordered_map<UserID, UserConfig> & all() { return m_configs; }
    inline const std::unordered_map<UserID, UserConfig> & all() const { return m_configs; }

    inline bool has(UserID user_id) const { return m_configs.contains(user_id); }
    inline void remove(UserID user_id) { m_configs.erase(user_id); }
    inline void set(UserConfig cfg) { m_configs.emplace(cfg.user_id, cfg); }
    UserConfig & get(UserID user_id);
    const UserConfig & get(UserID user_id) const;

    bool get_need_to_rewrite() const { return m_modified_but_not_written_to_file_yet; }
    void set_need_to_rewrite(bool status) { m_modified_but_not_written_to_file_yet = status; }

private:
    void load_configs();
    void save_configs();

    bool deserialize(const nlohmann::json & j);
    nlohmann::json serialize() const;

private:
    const PathsStorage & m_paths;
    Logger & m_log;

    std::mutex m_file_mtx;

    uint8_t m_configs_version {1};
    std::unordered_map<UserID, UserConfig> m_configs;
    bool m_modified_but_not_written_to_file_yet {false};
};

} // namespace kbot