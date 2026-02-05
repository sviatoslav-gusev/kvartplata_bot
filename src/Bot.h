#pragma once
#include "Scheduler.h"
#include "StrongTypes.h"
#include "UserConfig.h"
#include "Logger.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <tgbot/tgbot.h>
#include <string_view>

namespace kbot {

class Application;

class Bot {
public:
    explicit Bot(Logger & log, Application & app, Scheduler & sch, std::string tg_token);
    void run_until(const std::function<bool()> & stop_flag);

    void initial_user_schedule(UserConfig & cfg); // on app load (schedule from stored configs) + on user start command
    bool is_user_loaded(UserID user_id);

private:
    void load_bot_commands(); // Use before run

    void try_send_message(UserID user_id, const std::string & text);

    void notify_payment(UserID user_id);  // scheduled user reminding, new reminding for tomorrow
    void submit_payment(UserID user_id);  // get payment confirmation, rescheduling to next payment period

    [[nodiscard("Save TaskID")]] TaskID schedule_payment_notification(UserID user_id, const std::chrono::system_clock::time_point & tp);
    void unschedule_payment_notification_for(UserID user_id);
    inline void unschedule_all_notifications_for(UserID user_id) { m_sch.delete_all_user_tasks_for(user_id); }
private:
    Logger & m_log;

    std::mutex m_mutex;

    Application & m_app;
    Scheduler & m_sch;
    TgBot::Bot m_bot;
};

} // namespace kbot