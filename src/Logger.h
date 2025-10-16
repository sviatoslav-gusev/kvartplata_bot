#pragma once

#include "storages/PathsStorage.h"
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <source_location>
#include <string_view>

namespace kbot {

class Logger
{
    enum class Level { Debug, Wow, Warning, Error };

    // color escape codes
    static constexpr std::string_view RESET = "\x1b[0m";
    static constexpr std::string_view RED   = "\x1b[31m";
    static constexpr std::string_view YEL   = "\x1b[33m";
    static constexpr std::string_view GRN   = "\x1b[32m";

    // level_tag<L> will call corresponding tag for log level
    template<Level L>
    static constexpr std::string_view level_tag =
        L == Level::Debug   ? "DBG" :
        L == Level::Warning ? "WRN" :
        L == Level::Error   ? "ERR" :
        L == Level::Wow     ? "WOW" :
                              "WTF";

public:
    explicit Logger(const PathsStorage & paths)
      : m_log_file(paths.log_path(), std::ios::out | std::ios::app | std::ios::binary)
    {
      if (!m_log_file) throw std::runtime_error("log: cannot open log file");

      std::ios::sync_with_stdio(false); // speedup cout via unsync printf and cout bufs
    }

    template<class... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args) const {
        log<Level::Debug>(fmt, std::forward<Args>(args)...);
    }

    template<class... Args>
    void warn(std::format_string<Args...> fmt, Args&&... args) const {
        log<Level::Warning>(fmt, std::forward<Args>(args)...);
    }

    template<class... Args>
    void wow(std::format_string<Args...> fmt, Args&&... args) const {
        log<Level::Wow>(fmt, std::forward<Args>(args)...);
    }

    template<class... Args>
    void error(std::format_string<Args...> fmt, Args&&... args) const {
        log<Level::Error>(fmt, std::forward<Args>(args)...);
    }

private:
    template<Level L, class... Args>
    void log(std::format_string<Args...> fmt, Args&&... args) const
    {
        using namespace std::chrono;
        const system_clock::time_point now = system_clock::now();
        const year_month_day now_ymd = {floor<days>(now)};
        const hh_mm_ss now_hm_ss = hh_mm_ss<seconds>{floor<seconds>(now) - floor<days>(now)};

        std::string line;
        line.reserve(128);
        line += level_tag<L>;
        line += ' ';
        std::format_to(std::back_inserter(line), "{:%Y-%m-%d} {:%H:%M:%S}: ", now_ymd, now_hm_ss);
        std::vformat_to(std::back_inserter(line), fmt.get(), std::make_format_args(args...));
        line.push_back('\n');

        std::scoped_lock lk(m_file_mtx);
        m_log_file.write(line.data(), static_cast<std::streamsize>(line.size()));
        m_log_file.flush();

        if constexpr (L == Level::Error) {
            std::cerr << RED << line << RESET;
            std::cerr.flush();
        }
        else if constexpr (L == Level::Warning) {
            std::cerr << YEL << line << RESET;
            std::cerr.flush();
        }
        else if constexpr (L == Level::Wow) {
            std::cout << GRN << line << RESET;
            std::cout.flush();
        }
        else {
            std::cout.write(line.data(), static_cast<std::streamsize>(line.size())).flush();
        }
    }

private:
    mutable std::mutex m_file_mtx;
    mutable std::ofstream m_log_file;
};

} // namespace kbot