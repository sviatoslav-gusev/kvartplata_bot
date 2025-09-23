#include "Bot.h"

#include "Application.h"
#include "UserConfig.h"
#include "ChronoHelpers.h"
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

void kbot::Bot::notify_payment(UserID user_id)
{
    if (!m_app.has_config(user_id)) {
        m_log.error("{}: no config", __func__);
        return;
    }

    m_bot.getApi().sendMessage(m_app.get_config(user_id).chat_id.get(),
                               std::format("Hi, #{}.\n"
                                           "Albanian reminder.\n"
                                           "Send \"Y\" if you paid for current month.\n",
                                           user_id));
    m_log.debug("{}: User #{} is noticed now", __func__, user_id);

    /********** SCHEDULE NEXT **********/
    std::scoped_lock lk(m_mutex);

    UserConfig & cfg = m_app.get_config(user_id);
    cfg.scheduled_payment_task_id.clear();

    using namespace std::chrono;
    const system_clock::time_point now = system_clock::now();
    const year_month_day now_ymd = {floor<days>(now)};
    const hh_mm_ss now_hm = std::chrono::hh_mm_ss<minutes>{floor<minutes>(now) - floor<days>(now)};
    const year_month now_ym = year_month{now_ymd.year(), now_ymd.month()};
    const year_month & last_paid_ym = cfg.last_paid_year_month;
    const std::chrono::minutes signal_hm_duration = duration_cast<minutes>(cfg.signal_hour_minute.to_duration());

    if (last_paid_ym == now_ym) [[unlikely]] { // Schedule to next month (rare usecase: next month scheduling should be triggered by submit_payment)
        m_log.warn("{}: strange call #1. User #{} will be notified next month", __func__, user_id);
        const system_clock::time_point next_month_tp =
            sys_days{util::make_valid_ymd(now_ym + months{1}, cfg.signal_day)} + signal_hm_duration;
        cfg.scheduled_payment_task_id = schedule_payment_notification(user_id, next_month_tp);
    }
    else [[likely]] { // Notify today/tomorrow
        const system_clock::time_point signal_today_tp = sys_days{now_ymd} + signal_hm_duration;

        if (now_hm.to_duration() < signal_hm_duration) [[unlikely]] { // Notify today (rare usecase)
            m_log.warn("{}: strange call #2. User #{} will be notified today later", __func__, user_id);
            cfg.scheduled_payment_task_id = schedule_payment_notification(user_id, signal_today_tp);
        }
        else [[likely]] { // Notify tomorrow
            m_log.debug("{}: User #{} will be noticed tomorrow at {} UTC again", __func__, user_id, cfg.signal_hour_minute);
            cfg.scheduled_payment_task_id = schedule_payment_notification(user_id, signal_today_tp + days{1});
        }
    }
}

void kbot::Bot::submit_payment(UserID user_id)
{
    if (!m_app.has_config(user_id)) {
        m_log.error("{}: no config", __func__);
        return;
    }

    std::scoped_lock lk(m_mutex);

    UserConfig & cfg = m_app.get_config(user_id);
    const ChatID chat_id = cfg.chat_id;

    using namespace std::chrono;
    const system_clock::time_point now = system_clock::now();
    const year_month_day now_ymd = {floor<days>(now)};
    const hh_mm_ss now_hm = std::chrono::hh_mm_ss<minutes>{floor<minutes>(now) - floor<days>(now)};
    const year_month now_ym = year_month{now_ymd.year(), now_ymd.month()};
    const year_month & last_paid_ym = cfg.last_paid_year_month;

    // A. paid this month?
    if (last_paid_ym == now_ym)
    {
        m_log.debug("{}: User #{} tried to pay for current month again", __func__, user_id);
        m_bot.getApi().sendMessage(chat_id.get(), "You already paid this month!");
        return;
    }

    // B. paid prev month, but there is too early to pay this month?
    year_month_day signal_this_month_ymd = util::make_valid_ymd(now_ym, cfg.signal_day);
    const hh_mm_ss<minutes> & signal_hm = cfg.signal_hour_minute;
    const std::chrono::minutes signal_hm_duration = duration_cast<minutes>(cfg.signal_hour_minute.to_duration());

    const bool before_payment_dhm =
        now_ymd < signal_this_month_ymd ||
         (now_ymd == signal_this_month_ymd &&
          now_hm.to_duration() < signal_hm_duration);

    // paid in prev month, but now is erly to pay
    if (last_paid_ym + months{1} == now_ym && before_payment_dhm)
    {
        m_log.debug("{}: User #{} tried to pay for this month too early.", __func__, user_id);
        m_bot.getApi().sendMessage(chat_id.get(),
                                   std::format("It is early to pay this month!\n"
                                               "I will remind you at {}, {} UTC\n",
                                                signal_this_month_ymd,
                                                signal_hm));
        return;
    }

    /********** SCHEDULE NEXT **********/
    cfg.last_paid_year_month = now_ym;

    if (!before_payment_dhm) {  // this month is too late to notice, so assume it is done and next notice will be in next month
        signal_this_month_ymd = util::make_valid_ymd(now_ym + months{1}, cfg.signal_day);
    }

    const system_clock::time_point signal_tp = sys_days{signal_this_month_ymd} + signal_hm_duration;

    unschedule_payment_notification(user_id);
    cfg.scheduled_payment_task_id = schedule_payment_notification(user_id, signal_tp);

    m_log.debug("{}: User #{} successfully paid for current month", __func__, user_id);
    m_bot.getApi().sendMessage(chat_id.get(),
                               std::format("Great!\n"
                                           "Your next notification will be called at {}, {} UTC\n",
                                           signal_this_month_ymd,
                                           signal_hm));
}

kbot::TaskID kbot::Bot::schedule_payment_notification(UserID user_id, const std::chrono::system_clock::time_point & tp)
{
    return m_sch.enqueue_task(tp, user_id, [this, user_id]() {this->notify_payment(user_id);});
}

void kbot::Bot::unschedule_payment_notification(UserID user_id)
{
//    std::scoped_lock lk(m_mutex);

    if (!m_app.has_config(user_id)) {
        return;
    }

    UserConfig & cfg = m_app.get_config(user_id);

    if (cfg.scheduled_payment_task_id.empty()) {
        return;
    }
    m_sch.delete_task(cfg.scheduled_payment_task_id);
    cfg.scheduled_payment_task_id.clear();
}

void kbot::Bot::load_bot_commands()
{
    m_bot.getEvents().onCommand("start", [this](TgBot::Message::Ptr msg) {
        const UserID user_id{msg->from->id};
        const ChatID chat_id{msg->chat->id};

        if(m_app.has_config(user_id)) // User duplicated start
        {
            m_log.wow("onCommand(\"start\"): /start for existing user #{}", user_id);
            m_bot.getApi().sendMessage(chat_id.get(),
                                       std::format("Hi, #{}!\n"
                                                   "Your account is alreary running.\n",
                                                   user_id));
            return;
        }

        UserConfig new_user_config(user_id, chat_id);
        m_app.set_config(new_user_config);
        initial_user_schedule(new_user_config);

        m_log.wow("onCommand(\"start\"): /start for new user #{}", user_id);
        m_bot.getApi().sendMessage(chat_id.get(),
                                   std::format("Welcome, #{}!\n"
                                               "Your closest notification will be at closest {} day in {}\n",
                                               user_id,
                                               new_user_config.signal_day,
                                               new_user_config.signal_hour_minute));
    });
    m_bot.getEvents().onAnyMessage([this](TgBot::Message::Ptr msg) {
        if (StringTools::startsWith(msg->text, "/start")) {
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
    m_log.debug("{}: start. For user #{}", __func__, cfg.user_id);

    std::scoped_lock lk(m_mutex);

    using namespace std::chrono;
    const system_clock::time_point now = system_clock::now();
    const year_month_day now_ymd = {floor<days>(now)};
    const hh_mm_ss now_hm = std::chrono::hh_mm_ss<minutes>{floor<minutes>(now) - floor<days>(now)};
    const year_month now_ym = year_month{now_ymd.year(), now_ymd.month()};
    const year_month & last_paid_ym = cfg.last_paid_year_month;

    const std::chrono::minutes signal_hm_duration = duration_cast<minutes>(cfg.signal_hour_minute.to_duration());
    const std::chrono::minutes now_hm_duration = duration_cast<minutes>(now_hm.to_duration());

    // A. paid this month? Schedule next month
    if (last_paid_ym == now_ym)
    {
        const system_clock::time_point signal_next_month_tp =
            sys_days{util::make_valid_ymd(now_ym + months{1}, cfg.signal_day)} + signal_hm_duration;
        cfg.scheduled_payment_task_id = schedule_payment_notification(cfg.user_id, signal_next_month_tp);
        return;
    }

    const year_month_day signal_this_month_ymd = util::make_valid_ymd(now_ym, cfg.signal_day);

    const bool before_payment_dhm =
        now_ymd < signal_this_month_ymd ||
         (now_ymd == signal_this_month_ymd &&
          now_hm_duration < signal_hm_duration);

    // B. paid prev month, and there payday of this month had not came yet
    if (last_paid_ym + months{1} == now_ym && before_payment_dhm)
    {
        const system_clock::time_point signal_this_month_tp =
            sys_days{signal_this_month_ymd} + signal_hm_duration;
        cfg.scheduled_payment_task_id = schedule_payment_notification(cfg.user_id, signal_this_month_tp);
        return;
    }

    // C. time to pay. Remind today or tomorrow
    const system_clock::time_point signal_today_tp = sys_days{now_ymd} + signal_hm_duration;

    if (now_hm_duration < signal_hm_duration) { // Notify today
        cfg.scheduled_payment_task_id = schedule_payment_notification(cfg.user_id, signal_today_tp);
    }
    else { // Notify tomorrow
        cfg.scheduled_payment_task_id = schedule_payment_notification(cfg.user_id, signal_today_tp + days{1});
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