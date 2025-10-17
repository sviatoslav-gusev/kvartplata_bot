#include "PathsStorage.h"
#include "../helpers/Getopt.h"
#include "../helpers/PathHelpers.h"
#include <filesystem>
#include <string_view>
#include <iostream>

kbot::PathsStorage::PathsStorage(int argc, char** argv)
{
    std::cout << "PathsStorage ctor: start\n";
    read_paths_from_cli_env(argc, argv);
    std::cout << "PathsStorage ctor: end\n";
}

void kbot::PathsStorage::dir_existence(const std::string & name, const std::string_view & dir) const
{
    const std::filesystem::path path{dir};
    if (!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
    {
        std::cerr << "Wrong " << name << " directory: " << path << std::endl;
        std::cerr.flush();
        exit(0);
    }
}

void kbot::PathsStorage::read_paths_from_cli_env(int argc, char** argv)
{
    cfg_dir = "./";
    key_dir = "./";
    log_dir = "./";
    m_enable_logs_to_file = false;
    bool show_help = false;

    Getopt opt;

    opt.add_option('h', "help", Getopt::Arg::None,
                   "Show this help and exit",
                   [&](std::optional<std::string_view>) {
                       show_help = true;
                   });

    opt.add_option('f', "enable_logs_to_file", Getopt::Arg::None,
                   "Enable writing logs to file",
                   [&](std::optional<std::string_view>) {
                       m_enable_logs_to_file = true;
                   });

    opt.add_option('c', "cfg_dir", Getopt::Arg::Required,
                   "Config directory",
                   [&](std::optional<std::string_view> v) {
                       cfg_dir = v ? std::string(*v) : cfg_dir;
                   });

    opt.add_option('k', "key_dir", Getopt::Arg::Required,
                   "Key directory",
                   [&](std::optional<std::string_view> v) {
                       key_dir = v ? std::string(*v) : key_dir;
                   });

    opt.add_option('l', "log_dir", Getopt::Arg::Required,
                   "Log directory",
                   [&](std::optional<std::string_view> v) {
                       log_dir = v ? std::string(*v) : log_dir;
                   });

    const Getopt::Result positionals = opt.parse(argc, argv);

    if (!positionals.positionals.empty()) {
        std::cerr << "You do not need to set positional arguments. Read Help:\n";
        std::cerr.flush();
        exit(0);
    }

    if (show_help) {
        opt.print_usage(std::cout, argv[0]);
        exit(0);
    }

    cfg_dir = path::normalize(cfg_dir, true);
    key_dir = path::normalize(key_dir, true);
    log_dir = path::normalize(log_dir, true);

    dir_existence("cfg_dir", cfg_dir);
    dir_existence("key_dir", key_dir);
    dir_existence("log_dir", log_dir);

    m_cfg_path       = path::join(cfg_dir, cfg_filename);
    m_cfg_tmp_path   = path::join(cfg_dir, cfg_tmp_filename);
    m_token_enc_path = path::join(cfg_dir, token_enc_filename);
    m_token_key_path = path::join(key_dir, token_key_filename);
    m_log_path       = path::join(log_dir, log_filename);

    std::cout << "Accepted args:\n "
              << m_cfg_path << "\n "
              << m_cfg_tmp_path  << "\n "
              << m_token_enc_path  << "\n "
              << m_token_key_path  << "\n "
              << m_log_path  << "\n";
    // Позиционные аргументы: res.positionals
}