#include "Getopt.h"
#include <algorithm>
#include <stdexcept>

namespace kbot {

struct Getopt::Impl {
    std::vector<Spec> specs;
    struct ShortHash { size_t operator()(char c) const noexcept { return static_cast<unsigned char>(c); } };
    std::unordered_map<char, std::size_t, ShortHash> short_index;
    std::unordered_map<std::string, std::size_t>     long_index;

    // -------------------------------------------------------------------------
    void add_spec(Spec s) {
        if (s.short_name) {
            char c = *s.short_name;
            if (short_index.contains(c))
                throw std::runtime_error(std::string("Duplicate short option: -") + c);
        }
        if (!s.long_name.empty()) {
            if (long_index.contains(s.long_name))
                throw std::runtime_error("Duplicate long option: --" + s.long_name);
        }
        specs.push_back(std::move(s));
        const std::size_t idx = specs.size() - 1;
        if (specs[idx].short_name)
            short_index.emplace(*specs[idx].short_name, idx);
        if (!specs[idx].long_name.empty())
            long_index.emplace(specs[idx].long_name, idx);
    }

    const Spec* find_short(char c) const {
        if (auto it = short_index.find(c); it != short_index.end()) return &specs[it->second];
        return nullptr;
    }
    const Spec* find_long(std::string_view name) const {
        if (auto it = long_index.find(std::string{name}); it != long_index.end()) return &specs[it->second];
        return nullptr;
    }

    static bool is_new_option_token(std::string_view tok) {
        // A token is considered an option if it starts with '-' (and is not just "-")
        return tok.size() >= 2 && tok[0] == '-';
    }

    static void parse_error(const std::string& msg) {
        throw std::runtime_error("Getopt parse error: " + msg);
    }

    struct LongSplit {
        std::string_view name;
        std::string_view value;
        bool has_eq{};
    };
    static LongSplit split_long(std::string_view body) {
        // body: "opt" or "opt=value"
        const auto pos = body.find('=');
        if (pos == std::string_view::npos) return { body, {}, false };
        return { body.substr(0, pos), body.substr(pos + 1), true };
    }

    void handle_arg_policy(const Spec& spec,
                           bool has_eq,
                           std::string_view eq_value,
                           int argc, char** argv, int& i) const
    {
        switch (spec.arg) {
            case Arg::None: {
                if (has_eq)
                    parse_error("--" + spec.long_name + " does not take a value");
                if (spec.on_hit) spec.on_hit(std::nullopt);
            } break;

            case Arg::Required: {
                std::string_view value;
                if (has_eq) {
                    value = eq_value;
                } else {
                    if (i + 1 >= argc)
                        parse_error("--" + spec.long_name + " requires a value");
                    value = std::string_view(argv[i + 1] ? argv[i + 1] : "");
                    ++i;
                }
                if (spec.on_hit) spec.on_hit(value);
            } break;

            case Arg::Optional: {
                std::string_view value;
                if (has_eq) {
                    value = eq_value;
                } else {
                    if (i + 1 < argc) {
                        std::string_view peek{argv[i + 1] ? argv[i + 1] : ""};
                        if (!is_new_option_token(peek)) {
                            value = peek;
                            ++i;
                        }
                    }
                }
                if (spec.on_hit)
                    spec.on_hit(value.empty() ? std::nullopt : std::optional<std::string_view>(value));
            } break;
        }
    }
};

// -----------------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------------
Getopt::Getopt() : m_impl(std::make_unique<Impl>()) {}
Getopt::~Getopt() noexcept = default;
Getopt::Getopt(Getopt&&) noexcept = default;
Getopt& Getopt::operator=(Getopt&&) noexcept = default;

void Getopt::add_option(char short_name,
                        std::string long_name,
                        Arg arg,
                        std::string help,
                        std::function<void(std::optional<std::string_view>)> on_hit)
{
    Spec s;
    s.short_name = short_name;
    s.long_name  = std::move(long_name);
    s.arg        = arg;
    s.help       = std::move(help);
    s.on_hit     = std::move(on_hit);
    m_impl->add_spec(std::move(s));
}

void Getopt::add_option(std::string long_name,
                        Arg arg,
                        std::string help,
                        std::function<void(std::optional<std::string_view>)> on_hit)
{
    Spec s;
    s.long_name  = std::move(long_name);
    s.arg        = arg;
    s.help       = std::move(help);
    s.on_hit     = std::move(on_hit);
    m_impl->add_spec(std::move(s));
}

void Getopt::add_option(char short_name,
                        Arg arg,
                        std::string help,
                        std::function<void(std::optional<std::string_view>)> on_hit)
{
    Spec s;
    s.short_name = short_name;
    s.arg        = arg;
    s.help       = std::move(help);
    s.on_hit     = std::move(on_hit);
    m_impl->add_spec(std::move(s));
}

// -----------------------------------------------------------------------------
// Main parse routine
// -----------------------------------------------------------------------------
Getopt::Result Getopt::parse(int argc, char** argv) const {
    Result res;
    bool stop_options = false;
    int i = 1;

    while (i < argc) {
        std::string_view tok{argv[i] ? argv[i] : ""};

        if (!stop_options && tok == "--") {
            stop_options = true;
            ++i;
            continue;
        }

        // ---- Long option: --name or --name=value
        if (!stop_options && tok.size() >= 2 && tok.starts_with("--")) {
            const auto [name, val, has_eq] = Impl::split_long(tok.substr(2));
            const Spec* spec = m_impl->find_long(name);
            if (!spec)
                Impl::parse_error("Unknown long option '--" + std::string(name) + "'");
            m_impl->handle_arg_policy(*spec, has_eq, val, argc, argv, i);
            ++i;
            continue;
        }

        // ---- Short options cluster: -xzvf, -oVALUE, -o VALUE
        if (!stop_options && tok.size() >= 2 && tok[0] == '-') {
            std::size_t pos = 1;
            while (pos < tok.size()) {
                char c = tok[pos];
                const Spec* spec = m_impl->find_short(c);
                if (!spec)
                    Impl::parse_error(std::string("Unknown short option '-") + c + "'");
                if (spec->arg == Arg::None) {
                    if (spec->on_hit) spec->on_hit(std::nullopt);
                    ++pos;
                    continue;
                }

                // Option with an argument
                std::string_view value;
                bool inline_val = false;

                // -oVALUE form
                if (pos + 1 < tok.size()) {
                    value = tok.substr(pos + 1);
                    inline_val = true;
                } else {
                    // -o VALUE form
                    if (spec->arg == Arg::Required) {
                        if (i + 1 >= argc)
                            Impl::parse_error(std::string("Option '-") + c + "' requires an argument");
                        value = std::string_view(argv[i + 1] ? argv[i + 1] : "");
                        ++i;
                    } else {
                        // Optional argument
                        if (i + 1 < argc) {
                            std::string_view peek{argv[i + 1] ? argv[i + 1] : ""};
                            if (!Impl::is_new_option_token(peek)) {
                                value = peek;
                                ++i;
                            }
                        }
                    }
                }

                if (spec->on_hit)
                    spec->on_hit(value.empty() ? std::nullopt : std::optional<std::string_view>(value));

                if (inline_val)
                    pos = tok.size();
                else
                    ++pos;
            }
            ++i;
            continue;
        }

        // ---- Positional argument
        res.positionals.emplace_back(tok);
        ++i;
    }

    return res;
}

// -----------------------------------------------------------------------------
// Usage printing
// -----------------------------------------------------------------------------
void Getopt::print_usage(std::ostream& os, std::string_view prog_name) const {
    os << "Usage: " << prog_name << " [options] [args...]\n\n";
    os << "Options:\n";
    for (auto const& s : m_impl->specs) {
        os << "  ";
        bool first = true;
        if (s.short_name) {
            os << "-" << *s.short_name;
            first = false;
        }
        if (!s.long_name.empty()) {
            if (!first) os << ", ";
            os << "--" << s.long_name;
        }
        switch (s.arg) {
            case Arg::None: break;
            case Arg::Required: os << " <value>"; break;
            case Arg::Optional: os << " [value]"; break;
        }
        if (!s.help.empty()) {
            os << "\n      " << s.help;
        }
        os << "\n";
    }
}

} // namespace kbot
