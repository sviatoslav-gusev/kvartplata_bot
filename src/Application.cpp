#include "Application.h"
#include "TokenStorage.h"
#include "libs/json.hpp"

#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <fstream>

kbot::Application::Application()
    : m_sch(m_log)
    , m_bot(m_log, *this, m_sch, TokenStorage(m_log).load_token())
{
    m_log.debug("{}: start", __func__);
    try {
        load_configs();
        for (auto & [user_id, user_cfg] : m_configs)
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

kbot::Application::~Application()
{
    m_log.debug("{}: start", __func__);
    try {
        save_configs();
    }
    catch (const std::runtime_error & e) // should be noexcept, but we are saving config, so..
    {
        m_log.error("{}: error: {}", __func__, e.what());
    }
    m_log.debug("{}: end", __func__);
}

void kbot::Application::load_configs()
{
    std::scoped_lock lk(m_file_mtx);
    m_log.debug("{}: start", __func__);

    nlohmann::json j;
    if (std::filesystem::exists(m_filename_tmp))
    {
        std::ifstream ifs(m_filename_tmp.data(), std::ios::binary);
        if (!ifs) throw std::runtime_error("load: cannot open tmp file\n");
        ifs >> j;
        ifs.close();
        if (deserialize(j)) {
            m_log.debug("{}: end. Tmp json was load", __func__);
            std::filesystem::rename(m_filename_tmp, m_filename);
            return;
        }
        else {
            j.clear();
            std::filesystem::remove(m_filename_tmp);
        }
    }

    if (std::filesystem::exists(m_filename))
    {
        std::ifstream ifs(m_filename.data(), std::ios::binary);
        if (!ifs) throw std::runtime_error("load: cannot open file\n");
        ifs >> j;
        ifs.close();
        if (deserialize(j)) {
            m_log.debug("{}: end. Normal json was load", __func__);
            return;
        }
        else {
            j.clear();
            std::filesystem::remove(m_filename);
        }
    }

    m_log.warn("{}: end. None of json was load", __func__);
}

void kbot::Application::save_configs()
{
    std::scoped_lock lk(m_file_mtx);
    m_log.debug("{}: start", __func__);

    const nlohmann::json j = serialize();

    {   // RAII for writing file
        std::ofstream ofs(m_filename_tmp.data(), std::ios::binary | std::ios::trunc);
        if (!ofs) {
            m_log.error("{}: end. Cannot open tmp file", __func__);
            return;
        }
        ofs << j;
        ofs.flush();       // flush buffer
        if (!ofs) {
            m_log.error("{}: end. Write failed", __func__);
            return;
        }
    }   // ofs is closed, file is saved

    std::error_code ec;
    std::filesystem::rename(m_filename_tmp, m_filename, ec); // atomic replacement
    if (ec) {
        m_log.error("{}: end. Rename failed: {}", __func__, ec.message());
        return;
    }

    m_log.debug("{}: end", __func__);
}

nlohmann::json kbot::Application::serialize() const
{
    m_log.debug("{}: start", __func__);
    nlohmann::json j;

    j["version"] = m_configs_version;
    j["items"] = nlohmann::json::array();
    for (const auto & [user_id, cfg] : m_configs)
    {
        j["items"].emplace_back(cfg); // works via .to_json
    }

    m_log.debug("{}: end", __func__);
    return j;
}

bool kbot::Application::deserialize(const nlohmann::json & j)
{
    m_log.debug("{}: start", __func__);
    if (j.empty())
    {
        m_log.warn("{}: end. JSON is empty, creating cache from the scratch", __func__);
        return true;
    }

    if (!j.is_object() || !j.contains("version") || !j.contains("items"))
    {
        m_log.error("{}: end. Problem at reading: is_object={}, contains_version={}, contains_items={}",
                    __func__, j.is_object(), j.contains("version"), j.contains("items"));
        return false;
    }

    m_configs_version = j.at("version").get<uint8_t>();

    for (const nlohmann::json & item : j["items"]) {
        UserConfig c = item.get<UserConfig>();
        m_configs.emplace(c.user_id, c);
    }

    m_log.debug("{}: end. Version {}, total configs {}", __func__, m_configs_version, m_configs.size());
    return true;
}

bool kbot::Application::has_config(UserID user_id) const
{
    return m_configs.contains(user_id);
}
void kbot::Application::set_config(UserConfig cfg) {
    m_configs.emplace(cfg.user_id, cfg);
}
kbot::UserConfig & kbot::Application::get_config(UserID user_id) {
    return m_configs.find(user_id)->second;
}