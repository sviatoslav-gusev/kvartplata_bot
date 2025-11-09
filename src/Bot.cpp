#include "Bot.h"

#include "Application.h"
#include "UserConfig.h"
#include "helpers/ChronoHelpers.h"
#include "StrongTypes.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <exception>
#include <format>
#include <iostream>
#include <mutex>
#include <ranges>

kbot::Bot::Bot(Logger & log, Application & app, Scheduler & sch, std::string token)
    : m_log(log)
    , m_app(app)
    , m_sch(sch)
    , m_bot(token)
{
    load_bot_commands();
}

kbot::UserStatus kbot::Bot::get_user_status(UserID user_id)
{
    m_log.debug("{}: start for user #{}", __func__, user_id);

    ChatID chat_id;
    {
        std::scoped_lock lock(m_mutex);
        if (!m_app.cfg().has(user_id)) {
            m_log.error("{}: user #{} not exists in ConfigStorage", __func__, user_id);
            return UserStatus::NotLoaded;
        }
        chat_id = m_app.cfg().get(user_id).chat_id;
    }

    try {
        const UserStatus status = m_bot.getApi().blockedByUser(chat_id.get())
                                ? UserStatus::LoadedButBannedUs
                                : UserStatus::Loaded;
        m_log.debug("{}: user #{} has status {}", __func__, user_id, status);
        return status;
    }
    catch (const std::exception& e) {
        m_log.error("{}: blockedByUser({}) failed: {}", __func__, chat_id, e.what());
        return UserStatus::Unknown;
    }
}

void kbot::Bot::notify_payment(UserID user_id)
{
    ChatID chat_id;
    switch (get_user_status(user_id)) {
    case UserStatus::Loaded: {
        m_log.debug("{}: user #{} received notification.", __func__, user_id);
        std::scoped_lock lk(m_mutex);
        chat_id = m_app.cfg().get(user_id).chat_id;
        break;
    }
    case UserStatus::LoadedButBannedUs: {
        m_log.wow("{}: user #{} banned us! Remove his data.", __func__, user_id);
        unschedule_all_notifications_for(user_id);
        std::scoped_lock lock(m_mutex);
        m_app.cfg().remove(user_id);
        m_app.cfg().set_need_to_rewrite(true);
        return;
    }
    case UserStatus::Unknown:
    case UserStatus::NotLoaded: {
        m_log.error("{}: strange call, because user #{} not exists in ConfigStorage", __func__, user_id);
        return;
    }
    }


    m_bot.getApi().sendMessage(chat_id.get(),
                               std::format("Hi, #{}.\n"
                                           "Albanian reminder.\n"
                                           "Send \"Y\" if you paid for current month.\n",
                                           user_id));
    m_log.debug("{}: User #{} is noticed now", __func__, user_id);

    /********** SCHEDULE NEXT **********/
    TimePoint signal_tp;
    {
        std::scoped_lock lk(m_mutex);

        UserConfig & cfg = m_app.cfg().get(user_id);
        cfg.scheduled_payment_task_id.clear();

        using namespace std::chrono;
        const system_clock::time_point now = system_clock::now();
        const year_month_day now_ymd = {floor<days>(now)};
        const minutes now_hm_duration = floor<minutes>(now) - floor<days>(now);
        const year_month now_ym = year_month{now_ymd.year(), now_ymd.month()};
        const year_month & last_paid_ym = cfg.last_paid_year_month;
        const std::chrono::minutes signal_hm_duration = duration_cast<minutes>(cfg.signal_hour_minute.to_duration());

        if (last_paid_ym == now_ym) [[unlikely]] { // Schedule to next month (rare usecase: next month scheduling should be triggered by submit_payment)
            m_log.warn("{}: strange call #1. User #{} will be notified next month", __func__, user_id);
            // next_month_tp
            signal_tp = sys_days{util::make_valid_ymd(now_ym + months{1}, cfg.signal_day)} + signal_hm_duration;
        }
        else [[likely]] { // Notify today/tomorrow
            const system_clock::time_point signal_today_tp = sys_days{now_ymd} + signal_hm_duration;

            if (now_hm_duration < signal_hm_duration) [[unlikely]] { // Notify today (rare usecase)
                m_log.warn("{}: strange call #2. User #{} will be notified today later", __func__, user_id);
                // signal_today_tp
                signal_tp = signal_today_tp;
            }
            else [[likely]] { // Notify tomorrow
                m_log.debug("{}: User #{} will be noticed tomorrow at {} UTC again", __func__, user_id, cfg.signal_hour_minute);
                // signal_tomorrow_tp
                signal_tp = signal_today_tp + days{1};
            }
        }
    }

    const TaskID task_id = schedule_payment_notification(user_id, signal_tp);

    std::scoped_lock lk(m_mutex);
    m_app.cfg().get(user_id).scheduled_payment_task_id = task_id;
    m_app.cfg().set_need_to_rewrite(true);
}

void kbot::Bot::submit_payment(UserID user_id)
{
    m_log.debug("{}: start. For user #{}", __func__, user_id);

    using namespace std::chrono;
    ChatID chat_id;
    year_month_day signal_ymd;
    hh_mm_ss<minutes> signal_hm;
    system_clock::time_point signal_tp;
    {
        std::scoped_lock lk(m_mutex);

        if (!m_app.cfg().has(user_id)) {
            m_log.error("{}: no config for user #{}. (Case 1)", __func__, user_id);
            return;
        }

        UserConfig & cfg = m_app.cfg().get(user_id);
        chat_id = cfg.chat_id;

        const system_clock::time_point now = system_clock::now();
        const year_month_day now_ymd = {floor<days>(now)};
        const minutes now_hm_duration = floor<minutes>(now) - floor<days>(now);
        const year_month now_ym = year_month{now_ymd.year(), now_ymd.month()};
        const year_month last_paid_ym_before = cfg.last_paid_year_month;

        // A. paid this month?
        if (last_paid_ym_before == now_ym)
        {
            m_log.debug("{}: User #{} tried to pay for current month again", __func__, user_id);
            m_bot.getApi().sendMessage(chat_id.get(), "You already paid this month!");
            return;
        }

        // B. paid prev month, but there is too early to pay this month?
        year_month_day signal_this_month_ymd = util::make_valid_ymd(now_ym, cfg.signal_day);
        signal_hm = cfg.signal_hour_minute;
        const minutes signal_hm_duration = duration_cast<minutes>(cfg.signal_hour_minute.to_duration());

        const bool before_payment_dhm =
            now_ymd < signal_this_month_ymd ||
            (now_ymd == signal_this_month_ymd &&
             now_hm_duration < signal_hm_duration);

        // paid in prev month, but now is erly to pay
        if (last_paid_ym_before + months{1} == now_ym && before_payment_dhm)
        {
            m_log.debug("{}: User #{} tried to pay for this month too early.", __func__, user_id);
            m_bot.getApi().sendMessage(cfg.chat_id.get(),
                                       std::format("It is early to pay this month!\n"
                                                   "I will remind you at {}, {} UTC\n",
                                                   signal_this_month_ymd,
                                                   signal_hm));
            return;
        }

        /********** SCHEDULE NEXT **********/
        cfg.last_paid_year_month = now_ym;

        signal_ymd = before_payment_dhm
                   ? now_ymd    // this_month_ymd
                   : util::make_valid_ymd(now_ym + months{1}, cfg.signal_day);  // next_month_ymd

        signal_tp = sys_days{signal_ymd} + signal_hm_duration;
    }

    unschedule_payment_notification_for(user_id);
    const TaskID task_id = schedule_payment_notification(user_id, signal_tp);

    {
        std::scoped_lock lk(m_mutex);

        if (!m_app.cfg().has(user_id)) {
            m_log.error("{}: no config for user #{}. (Case 2)", __func__, user_id);
            return;
        }

        m_app.cfg().get(user_id).scheduled_payment_task_id = task_id;
        m_app.cfg().set_need_to_rewrite(true);
    }

    m_log.debug("{}: User #{} successfully paid for current month. Next scheduled to {}, {}",
                __func__, user_id, signal_ymd, signal_hm);
    m_bot.getApi().sendMessage(chat_id.get(),
                               std::format("Great!\n"
                                           "Your next notification will be called at {}, {} UTC\n",
                                           signal_ymd,
                                           signal_hm));
}

kbot::TaskID kbot::Bot::schedule_payment_notification(UserID user_id, const std::chrono::system_clock::time_point & tp)
{
    return m_sch.enqueue_user_task(tp, user_id, [this, user_id]() {this->notify_payment(user_id);});
}

void kbot::Bot::unschedule_payment_notification_for(UserID user_id)
{
    TaskID task_id_to_delete;
    {
        std::scoped_lock lock(m_mutex);
        if (!m_app.cfg().has(user_id)) {
            return;
        }
        UserConfig & cfg = m_app.cfg().get(user_id);
        if (cfg.scheduled_payment_task_id.empty()) {
            return;
        }
        task_id_to_delete = cfg.scheduled_payment_task_id;
        cfg.scheduled_payment_task_id.clear();
    }

    m_sch.delete_user_task(task_id_to_delete);
}

void kbot::Bot::load_bot_commands()
{
    m_bot.getEvents().onCommand("start", [this](TgBot::Message::Ptr msg) {
        const UserID user_id{msg->from->id};
        const ChatID chat_id{msg->chat->id};

        using namespace std::chrono;
        bool user_is_duplicated = false;
        day signal_day;
        hh_mm_ss<minutes> signal_hour_minute;
        {
            std::scoped_lock lock(m_mutex);
            user_is_duplicated = m_app.cfg().has(user_id);

            if (!user_is_duplicated)
            {
                UserConfig new_user_config(user_id, chat_id);
                signal_day = new_user_config.signal_day;
                signal_hour_minute = new_user_config.signal_hour_minute;

                m_app.cfg().set(new_user_config);
            }
        }

        if (user_is_duplicated)
        {
            m_log.wow("onCommand(\"start\"): /start for existing user #{}", user_id);
            m_bot.getApi().sendMessage(chat_id.get(),
                                       std::format("Hi, #{}!\n"
                                                   "Your account is alreary running.\n",
                                                   user_id));
        }
        else
        {
            initial_user_schedule(m_app.cfg().get(user_id));
            m_log.wow("onCommand(\"start\"): /start for new user #{}", user_id);
            m_bot.getApi().sendMessage(chat_id.get(),
                                       std::format("Welcome, #{}!\n"
                                                   "Your closest notification will be at closest {} day in {}\n",
                                                   user_id,
                                                   signal_day,
                                                   signal_hour_minute));
        }
    });

    m_bot.getEvents().onCommand("stop", [this](TgBot::Message::Ptr msg) {
        const UserID user_id{msg->from->id};
        m_log.wow("{}: user #{} stopped us", __func__, user_id);
        unschedule_all_notifications_for(user_id);
        std::scoped_lock lock(m_mutex);
        m_app.cfg().remove(user_id);
        m_app.cfg().set_need_to_rewrite(true);
        return;
    });

    m_bot.getEvents().onAnyMessage([this](TgBot::Message::Ptr msg) {
        if (StringTools::startsWith(msg->text, "/start") || StringTools::startsWith(msg->text, "/stop")) {
            return;
        }
        m_log.wow("onAnyMessage(): user #{} wrote: {}", msg->from->id, msg->text);

        if (std::ranges::any_of(std::array{"Y", "y", "Д", "д"}, [&](auto v){ return msg->text == v; })) {
            submit_payment(UserID{msg->from->id});
            return;
        }

        m_bot.getApi().sendMessage(msg->chat->id, "Unknown message: " + msg->text);
    });
}

void kbot::Bot::initial_user_schedule(UserConfig & cfg)
{
    const UserID user_id = cfg.user_id;
    m_log.debug("{}: start. For user #{}", __func__, user_id);

    TimePoint signal_tp;
    {
        std::scoped_lock lk(m_mutex);

        if (const TaskID task_id = cfg.scheduled_payment_task_id; !task_id.empty()) {
            m_log.warn("{}: User #{} already has scheduled task #{}", __func__, user_id, task_id);
            return;
        }

        using namespace std::chrono;
        const system_clock::time_point now = system_clock::now();
        const year_month_day now_ymd = {floor<days>(now)};
        const minutes now_hm_duration = floor<minutes>(now) - floor<days>(now);
        const year_month now_ym = year_month{now_ymd.year(), now_ymd.month()};
        const year_month & last_paid_ym = cfg.last_paid_year_month;

        const minutes signal_hm_duration = duration_cast<minutes>(cfg.signal_hour_minute.to_duration());

        // A. paid this month? Schedule next month
        if (last_paid_ym == now_ym)
        {
            // signal_next_month_tp
            signal_tp = sys_days{util::make_valid_ymd(now_ym + months{1}, cfg.signal_day)} + signal_hm_duration;
        }
        else
        {
            const year_month_day signal_this_month_ymd = util::make_valid_ymd(now_ym, cfg.signal_day);

            const bool before_payment_dhm =
                now_ymd < signal_this_month_ymd ||
                (now_ymd == signal_this_month_ymd && now_hm_duration < signal_hm_duration);

            // B. paid prev month, and there payday of this month had not came yet
            if (last_paid_ym + months{1} == now_ym && before_payment_dhm)
            {
                // signal_this_month_tp
                signal_tp = sys_days{signal_this_month_ymd} + signal_hm_duration;
            }
            else
            {
                // C. time to pay. Remind today or tomorrow
                const system_clock::time_point signal_today_tp = sys_days{now_ymd} + signal_hm_duration;

                if (now_hm_duration < signal_hm_duration) {
                    // signal_today
                    signal_tp = signal_today_tp;
                }
                else {
                    // signal_tomorrow
                    signal_tp = signal_today_tp + days{1};
                }
            }
        }

        cfg.scheduled_payment_task_id = schedule_payment_notification(cfg.user_id, signal_tp);
    }

    m_log.debug("{}: end", __func__);
}

void kbot::Bot::run_until(const std::function<bool()> & stop_flag)
{
    try {
        m_bot.getApi().deleteWebhook();

        TgBot::TgLongPoll longPoll(m_bot, 1);

        while (!stop_flag()) {
            m_log.debug("long poll started");
            try {
//                m_bot.getApi().sendMessage(message->chat->id, "Your message is: " + message->text);
                longPoll.start();   // will return by timeouts of at answer
            } catch (const std::exception& e) {
                // not to throw to avoid crashes at temporary network problems
                m_log.warn("{}: long poll error: {}", __func__, e.what());
            }
        }
    }
    catch (std::exception & e) {
        m_log.error("{}: error: {}", __func__, e.what());
    }
}