#include "UserConfig.h"

#include "libs/json.hpp"
#include "helpers/ChronoHelpers.h"
#include "StrongTypes.h"
#include <optional>

kbot::UserConfig::UserConfig(UserID user_id, ChatID chat_id)
    : signal_day(cfg_default::DAY)
    , signal_hour_minute(std::chrono::hours{cfg_default::HOUR} + std::chrono::minutes{cfg_default::MINUTE})
    , last_paid_year_month([&]{
          using namespace std::chrono;

          const system_clock::time_point now = system_clock::now();
          const year_month_day now_ymd = {floor<days>(now)};
          const minutes now_hm_duration = floor<minutes>(now) - floor<days>(now);
          const year_month now_ym = year_month{now_ymd.year(), now_ymd.month()};
          const minutes signal_hm_duration = duration_cast<minutes>(signal_hour_minute.to_duration());
          const year_month_day signal_this_month_ymd = util::make_valid_ymd(now_ym, day{cfg_default::DAY});

          const bool before_payment_dhm =
              now_ymd < signal_this_month_ymd ||
              (now_ymd == signal_this_month_ymd && now_hm_duration < signal_hm_duration);

          if (before_payment_dhm) {
              const year_month_day ymd_prev = year_month_day{ now_ymd } - months{1};
              return year_month{ymd_prev.year(), ymd_prev.month()};
          }
          else {
              return year_month{now_ymd.year(), now_ymd.month()};
          }
      }())
    , user_id(user_id)
    , chat_id(chat_id)
{}

void kbot::to_json(nlohmann::json & j, const kbot::UserConfig & c)
{
    j = nlohmann::json{
        {"user_id",           c.user_id.get()},
        {"chat_id",           c.chat_id.get()},
        {"signal_day",        util::u8(c.signal_day)},
        {"signal_hour",       util::u8(c.signal_hour_minute.hours())},
        {"signal_minute",     util::u8(c.signal_hour_minute.minutes())},
        {"last_paid_year",    util::i16(c.last_paid_year_month.year())},
        {"last_paid_month",   util::u8(c.last_paid_year_month.month())}

        // No need to store. Each run TaskID are from the scratch
        // {"scheduled_payment_task_id", c.scheduled_payment_task_id.get()}
    };
}

void kbot::from_json(const nlohmann::json & j, kbot::UserConfig & c)
{
    using namespace std::chrono;

    j.at("user_id").get_to(c.user_id.get());
    j.at("chat_id").get_to(c.chat_id.get());

    c.signal_day = day{j.value("signal_day",  uint8_t{cfg_default::DAY})};

    c.signal_hour_minute = hh_mm_ss<minutes>({hours{j.value("signal_hour", uint8_t{cfg_default::HOUR})} +
                                              minutes{j.value("signal_minute", uint8_t{cfg_default::MINUTE})}});

    c.last_paid_year_month = {year{j.value("last_paid_year",  int16_t{0})},
                              month{j.value("last_paid_month", uint8_t{1})}};

    // No need to store. Each run TaskID are from the scratch
    // c.scheduled_payment_task_id.get() = j.value("scheduled_payment_task_id", uint64_t{0});
}