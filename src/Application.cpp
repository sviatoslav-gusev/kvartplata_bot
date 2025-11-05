#include "Application.h"
#include "storages/TokenStorage.h"

#include <exception>
#include <filesystem>

kbot::Application::Application(int argc, char** argv)
    : m_paths(argc, argv)
    , m_log(m_paths)
    , m_sch(m_log)
    , m_cfg(m_paths, m_log, m_sch)
    , m_bot(m_log, *this, m_sch, TokenStorage(m_paths, m_log).load_token())
{
    m_log.debug("{}: start", __func__);
    try {
        // SCHEDULE TASKS

        // user related tasks
        std::unordered_map<UserID, UserConfig> & configs = m_cfg.all();
        for (auto it = configs.begin(); it != configs.end();)
        {
            auto& [user_id, user_cfg] = *it;

            if (m_bot.get_user_status(user_id) == UserStatus::Loaded)
            {
                m_bot.initial_user_schedule(user_cfg);
                ++it;
                continue;
            }

            // UserStatus::Unknown,  UserStatus::NotLoaded,  UserStatus::LoadedButBannedUs
            it = configs.erase(it);
            m_cfg.set_need_to_rewrite(true);
        }

        // config synchronization task
        m_cfg.schedule_configs_synchronization();

        // RUN SCHEDULER
        m_sch.run();
    }
    catch (const std::runtime_error & e)
    {
        m_log.error("{}: error: {}", __func__, e.what());
        exit(0);
    }
    m_log.debug("{}: end", __func__);
}
