#pragma once

#include <chrono>
#include <cstdint>

namespace kbot::util {

constexpr uint8_t u8(std::chrono::day d) noexcept {
    return static_cast<uint8_t>(static_cast<unsigned>(d));
}

constexpr uint8_t u8(std::chrono::month m) noexcept {
    return static_cast<uint8_t>(static_cast<unsigned>(m));
}

constexpr int16_t i16(std::chrono::year y) noexcept {
    return static_cast<int16_t>(static_cast<int>(y));
}

constexpr uint8_t u8(std::chrono::hours h) noexcept {
    return static_cast<uint8_t>(h.count());
}

constexpr uint8_t u8(std::chrono::minutes m) noexcept {
    return static_cast<uint8_t>(m.count());
}

constexpr std::chrono::year_month_day make_valid_ymd(const std::chrono::year & y, const std::chrono::month & m, const std::chrono::day & day)
{
    if (static_cast<unsigned>(day) < 29) {
        return {y, m, day};
    }

    const std::chrono::day last_day_for_ym = // For months with 29-31 days
        std::chrono::year_month_day_last{y, std::chrono::month_day_last{m}}.day();

    return {y, m, std::min(day, last_day_for_ym)};
}

constexpr std::chrono::year_month_day make_valid_ymd(const std::chrono::year_month & ym, const std::chrono::day & day)
{
    return make_valid_ymd(ym.year(), ym.month(), day);
}

} // namespace kbot::util