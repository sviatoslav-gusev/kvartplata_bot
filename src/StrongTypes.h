#pragma once

#include <compare>
#include <cstdint>
#include <chrono>
#include <functional>
#include <ostream>
#include <format>
#include <type_traits>
#include <utility>

namespace kbot {

template<class T, class Tag>
    requires std::is_constructible_v<T, int> // default ctor
          && std::is_assignable_v<T&, int>   // clear()
          && std::is_class_v<Tag>

class Strong {

public:
    constexpr Strong()                      : m_val(0) {}
    constexpr explicit Strong(const T& val) : m_val(val) {}
    constexpr explicit Strong(T&& val)      : m_val(std::move(val)) {}

    constexpr       T& get()       noexcept { return m_val; }
    constexpr const T& get() const noexcept { return m_val; }

    constexpr auto operator<=>(const Strong&) const = default;
    constexpr bool operator==(const Strong&) const  = default;

    constexpr bool empty() const noexcept { return m_val == 0; }
    constexpr void clear() noexcept { m_val = 0; }

private:
    T m_val{};
};

struct UserIDTag {};
struct ChatIDTag {};
struct TaskIDTag {};

using UserID = Strong<int64_t, UserIDTag>;
using ChatID = Strong<int64_t, ChatIDTag>;
using TaskID = Strong<int64_t, TaskIDTag>;
using TimePoint = std::chrono::system_clock::time_point; // not strong, but :)

// for Scheduler::gen_task_id()
constexpr TaskID & operator++(TaskID & x) noexcept {
    ++x.get();
    return x;
}

} // namespace kbot


template<class T, class Tag>
struct std::hash<kbot::Strong<T, Tag>> {
    size_t operator()(const kbot::Strong<T, Tag>& x) const noexcept {
        return std::hash<T>{}(x.get());
    }
};

template<class T, class Tag>
std::ostream& operator<<(std::ostream& os, const kbot::Strong<T, Tag>& x) {
    return os << x.get();
}

template<class T, class Tag, class CharT>
struct std::formatter<kbot::Strong<T, Tag>, CharT> : std::formatter<T, CharT> {
    auto format(const kbot::Strong<T, Tag>& x, auto& ctx) const {
        return std::formatter<T, CharT>::format(x.get(), ctx);
    }
};