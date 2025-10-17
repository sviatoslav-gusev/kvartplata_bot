#pragma once
#include <memory>  // for std::unique_ptr
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <functional>
#include <unordered_map>
#include <iostream>

namespace kbot {

class Getopt {
public:
    enum class Arg {
        None,
        Required,
        Optional
    };

    struct Spec {
        std::optional<char> short_name;
        std::string         long_name;
        Arg                 arg = Arg::None;
        std::string         help;
        std::function<void(std::optional<std::string_view>)> on_hit;
    };

    struct Result {
        std::vector<std::string> positionals;
    };

public:
    Getopt();                                 // constructor
    ~Getopt() noexcept;                       // destructor — no manual cleanup needed
    Getopt(const Getopt&) = delete;           // non-copyable (unique_ptr)
    Getopt& operator=(const Getopt&) = delete;
    Getopt(Getopt&&) noexcept;                // movable
    Getopt& operator=(Getopt&&) noexcept;

    void add_option(char short_name,
                    std::string long_name,
                    Arg arg,
                    std::string help = {},
                    std::function<void(std::optional<std::string_view>)> on_hit = {});
    void add_option(std::string long_name,
                    Arg arg,
                    std::string help = {},
                    std::function<void(std::optional<std::string_view>)> on_hit = {});
    void add_option(char short_name,
                    Arg arg,
                    std::string help = {},
                    std::function<void(std::optional<std::string_view>)> on_hit = {});

    Result parse(int argc, char** argv) const;
    void print_usage(std::ostream& os, std::string_view prog_name) const;

private:
    struct Impl;                              // forward declaration
    std::unique_ptr<Impl> m_impl;              // smart pointer — automatic lifetime
};

} // namespace kbot
