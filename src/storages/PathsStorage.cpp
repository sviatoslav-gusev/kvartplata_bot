#include "PathsStorage.h"

#ifndef _WIN32
#include <getopt.h> // GNU getopt_long
#endif

#include <iostream>

kbot::PathsStorage::PathsStorage(int argc, char** argv)
{
    read_paths_from_cli_env(argc, argv);
}

void kbot::PathsStorage::try_getenv(const char* env_name, std::string & destination)
{
    if (const char* v = std::getenv(env_name))
    {
        destination = v;
    }
}

void kbot::PathsStorage::read_paths_from_cli_env(int argc, char** argv)
{
    // Try ENV (override defaults if exists)
    try_getenv("KBOT_KEY_DIR", key_dir);
    try_getenv("KBOT_CFG_DIR", cfg_dir);
    try_getenv("KBOT_LOG_DIR", log_dir);

#ifndef _WIN32
    // Try CLI (override ENV if exists)
    option longopts[] = {
        {"key_dir", required_argument, nullptr, 'k'},
        {"cfg_dir", required_argument, nullptr, 'c'},
        {"log_dir", required_argument, nullptr, 'l'},
        {nullptr, 0, nullptr, 0}
    };
    int opt, idx;
    while ((opt = getopt_long(argc, argv, "", longopts, &idx)) != -1) {
        switch (opt) {
            case 'k': key_dir = optarg; break;
            case 'c': cfg_dir = optarg; break;
            case 'l': log_dir = optarg; break;
            default:
                std::cerr << "Usage: " << argv[0]
                          << " [--key_dir PATH] [--cfg_dir PATH] [--log_dir DIR]\n";
                std::exit(2);
        }
    }
#endif

    m_cfg_path = cfg_dir + cfg_filename.data();
    m_cfg_tmp_path = cfg_dir + cfg_tmp_filename.data();
    m_token_enc_path = cfg_dir + token_enc_filename.data();
    m_token_key_path = key_dir + token_key_filename.data();
    m_log_path = log_dir + log_filename.data();
}