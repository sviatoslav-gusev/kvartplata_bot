#pragma once
#include <string>
#include <string_view>
#include <algorithm>

namespace kbot::path {

// Приводит все разделители к '/', удаляет дубликаты, опционально добавляет хвостовой '/'.
inline std::string normalize(std::string_view in, bool handle_dir = false)
{
    std::string s(in.begin(), in.end());

    // 1. '\' -> '/'
    std::replace(s.begin(), s.end(), '\\', '/');

    // 2. '//' -> '/'
    std::string out;
    out.reserve(s.size());
    bool prev_slash = false;
    for (char c : s) {
        if (c == '/') {
            if (!prev_slash) out.push_back('/');
            prev_slash = true;
        } else {
            out.push_back(c);
            prev_slash = false;
        }
    }
    s.swap(out);

    // 4. Optiolally addd tail '/' is needed (except "" and "/"-root)
    if (handle_dir) {
        if (!s.empty() && s.back() != '/') {
            s.push_back('/');
        }

        if (s.size() >= 2 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':') { // Absolute Win
            if (s.size() == 2) {
                s.push_back('/'); // Windows fix: "C:" -> "C:/"
            }
        }
        else if (!s.starts_with('/') && !s.starts_with("./")) { // Add "./"-prefix for dirs without any prefix
            s.insert(0, "./");
        }
    }

    return s;
}

// Merge 2 paths
inline std::string join(std::string_view a, std::string_view b,
                        bool ensure_trailing_slash = false)
{
    std::string left  = normalize(a, /*handle_dir=*/false);
    std::string right = normalize(b, /*handle_dir=*/false);

    if (left.empty())  return normalize(right, ensure_trailing_slash);
    if (right.empty()) return normalize(left,  ensure_trailing_slash);

    if (left.back() != '/') left.push_back('/');
    std::string res = left + right;
    return ensure_trailing_slash ? normalize(res, true) : res;
}

} // namespace kbot::path
