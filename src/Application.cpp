#include "Application.h"
#include "storages/TokenStorage.h"

#include <exception>
#include <filesystem>

kbot::Application::Application(int argc, char** argv)
    : m_paths(argc, argv)
    , m_log(m_paths)
    , m_cfg(m_paths, m_log)
    , m_sch(m_log)
    , m_bot(m_log, *this, m_sch, TokenStorage(m_paths, m_log).load_token())
{
    m_log.debug("{}: start", __func__);
    try {
        for (auto & [user_id, user_cfg] : m_cfg.all())
        {
            m_bot.initial_user_schedule(user_cfg);
        }
    }
    catch (const std::runtime_error & e)
    {
        m_log.error("{}: error: {}", __func__, e.what());
        exit(0);
    }
    m_log.debug("{}: end", __func__);
}
