#pragma once

#include "StrongTypes.h"
#include "libs/json_fwd.hpp"
#include <chrono>
#include <cstdint>
#include <optional>
#include <format>

namespace kbot {

namespace cfg_default
{
    constexpr uint8_t DAY = 20;
    constexpr uint8_t HOUR = 18;
    constexpr uint8_t MINUTE = 0;
};


struct UserConfig
{
    UserConfig() = default;
    explicit UserConfig(UserID user_id, ChatID chat_id);

    // All below is UTC
    std::chrono::day signal_day {cfg_default::DAY};
    std::chrono::hh_mm_ss<std::chrono::minutes> signal_hour_minute {std::chrono::hours{cfg_default::HOUR} +
                                                                    std::chrono::minutes{cfg_default::MINUTE}};

    std::chrono::year_month last_paid_year_month;

    TaskID scheduled_payment_task_id {0};

    UserID user_id {0};
    ChatID chat_id {0};
};

void to_json(nlohmann::json& j, const kbot::UserConfig& c);
void from_json(const nlohmann::json& j, kbot::UserConfig& c);

} // namespace kbot

template <>
struct std::formatter<kbot::UserConfig> : std::formatter<std::string> {
    auto format(const kbot::UserConfig& cfg, auto& ctx) const {
        return std::formatter<std::string>::format(
            std::format("UserConfig:\n"
                        " user_id={}, chat_id={}\n"
                        " signal_day={}, signal_hour_minute={}\n"
                        " last_paid_year_month={}\n"
                        " scheduled_payment_task_id={}\n",
                        cfg.user_id, cfg.chat_id,
                        cfg.signal_day, cfg.signal_hour_minute,
                        cfg.last_paid_year_month,
                        cfg.scheduled_payment_task_id),
            ctx);
    }
};