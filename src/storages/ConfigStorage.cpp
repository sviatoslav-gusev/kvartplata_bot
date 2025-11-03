#include "ConfigStorage.h"
#include "../libs/json.hpp"

#include <exception>
#include <stdexcept>

kbot::ConfigStorage::ConfigStorage(const PathsStorage & paths, Logger & log, Scheduler & sch)
    : m_paths(paths)
    , m_log(log)
    , m_sch(sch)
{
    m_log.debug("{}: start", __func__);
    try {
        load_configs();
    }
    catch (const std::runtime_error & e)
    {
        m_log.error("{}: error: {}", __func__, e.what());
        exit(0);
    }
    m_log.debug("{}: end", __func__);
}

kbot::ConfigStorage::~ConfigStorage()
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

void kbot::ConfigStorage::load_configs()
{
    std::scoped_lock lk(m_file_mtx);
    m_log.debug("{}: start", __func__);

    nlohmann::json j;
    if (std::filesystem::exists(m_paths.cfg_tmp_path()))
    {
        std::ifstream ifs(m_paths.cfg_tmp_path(), std::ios::binary);
        if (!ifs) throw std::runtime_error("load: cannot open tmp file\n");
        ifs >> j;
        ifs.close();
        if (deserialize(j)) {
            m_log.debug("{}: end. Tmp json was load", __func__);
            std::filesystem::rename(m_paths.cfg_tmp_path(), m_paths.cfg_path());
            return;
        }
        else {
            j.clear();
            std::filesystem::remove(m_paths.cfg_tmp_path());
        }
    }

    if (std::filesystem::exists(m_paths.cfg_path()))
    {
        std::ifstream ifs(m_paths.cfg_path(), std::ios::binary);
        if (!ifs) throw std::runtime_error("load: cannot open file\n");
        ifs >> j;
        ifs.close();
        if (deserialize(j)) {
            m_log.debug("{}: end. Normal json was load", __func__);
            return;
        }
        else {
            j.clear();
            std::filesystem::remove(m_paths.cfg_path());
        }
    }

    m_log.warn("{}: end. None of json was load", __func__);
}

void kbot::ConfigStorage::save_configs()
{
    std::scoped_lock lk(m_file_mtx);
    m_log.debug("{}: start", __func__);

    const nlohmann::json j = serialize();

    {   // RAII for writing file
        std::ofstream ofs(m_paths.cfg_tmp_path(), std::ios::binary | std::ios::trunc);
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
    std::filesystem::rename(m_paths.cfg_tmp_path(), m_paths.cfg_path(), ec); // atomic replacement
    if (ec) {
        m_log.error("{}: end. Rename failed: {}", __func__, ec.message());
        return;
    }

    m_log.debug("{}: end", __func__);
}

void kbot::ConfigStorage::schedule_configs_synchronization()
{
    m_log.debug("{}: start", __func__);

    using namespace std::chrono;
    const system_clock::time_point now = system_clock::now();
    const sys_days now_ymd_duration = floor<days>(now);
    const minutes now_hm_duration = floor<minutes>(now) - floor<days>(now);

    TimePoint tp;
    if (now_hm_duration > m_sync_hm.to_duration()) {
        tp = now_ymd_duration + days{1} + m_sync_hm.to_duration();
    }
    else {
        tp = now_ymd_duration + m_sync_hm.to_duration();
    }

    m_sch.enqueue_system_task(tp, [this](){
        if (this->get_need_to_rewrite()) {
            this->save_configs();
            this->set_need_to_rewrite(false);
        }
        this->schedule_configs_synchronization();
    });

    m_log.debug("{}: end. Daily configs saving is scheduled to closest {}", __func__, m_sync_hm);
}

nlohmann::json kbot::ConfigStorage::serialize() const
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

bool kbot::ConfigStorage::deserialize(const nlohmann::json & j)
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

kbot::UserConfig & kbot::ConfigStorage::get(UserID user_id)
{
    try {
        return m_configs.find(user_id)->second;
    }
    catch (const std::runtime_error & e)
    {
        m_log.error("{}: error: {}", __func__, e.what());
        exit(0);
    }
}

const kbot::UserConfig & kbot::ConfigStorage::get(UserID user_id) const
{
    try {
        return m_configs.find(user_id)->second;
    }
    catch (const std::runtime_error & e)
    {
        m_log.error("{}: error: {}", __func__, e.what());
        exit(0);
    }
}

bool kbot::ConfigStorage::get_need_to_rewrite() const
{
    m_log.debug("{}: {}", __func__, m_modified_but_not_written_to_file_yet);
    return m_modified_but_not_written_to_file_yet;
}

void kbot::ConfigStorage::set_need_to_rewrite(bool status)
{
    m_log.debug("{}: {}", __func__, status);
    m_modified_but_not_written_to_file_yet = status;
}