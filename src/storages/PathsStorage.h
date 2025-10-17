#pragma once
#include <string>
#include <string_view>

namespace kbot {

class PathsStorage
{
public:
    PathsStorage(int argc, char** argv);
    void read_paths_from_cli_env(int argc, char** argv);

public:
     const std::string & cfg_path() const { return m_cfg_path; }
     const std::string & cfg_tmp_path() const { return m_cfg_tmp_path; }
     const std::string & token_enc_path() const { return m_token_enc_path; }
     const std::string & token_key_path() const { return m_token_key_path; }
     const std::string & log_path() const { return m_log_path; }
     bool enable_logs_to_file() const { return m_enable_logs_to_file; }

private:
    void dir_existence(const std::string & name, const std::string_view & dir) const;

    static constexpr std::string_view cfg_filename{"kvartplata_bot.cfg"};
    static constexpr std::string_view cfg_tmp_filename{"kvartplata_bot.cfg.tmp"};
    static constexpr std::string_view token_enc_filename{"kvartplata_bot.enc"};
    static constexpr std::string_view token_key_filename{"kvartplata_bot.key"};
    static constexpr std::string_view log_filename{"kvartplata_bot.log"};

    std::string cfg_dir{"./"}; // = "/var/lib/kvartplata-bot/"; // cfg, token_enc
    std::string key_dir{"./"};; // = "/etc/kvartplata-bot/";     // token_key
    std::string log_dir{"./"};; // = "/var/log/kvartplata-bot/"; // log

    std::string m_cfg_path;
    std::string m_cfg_tmp_path;
    std::string m_token_enc_path;
    std::string m_token_key_path;
    std::string m_log_path;
    bool m_enable_logs_to_file = false;
};

} // namespace kbot