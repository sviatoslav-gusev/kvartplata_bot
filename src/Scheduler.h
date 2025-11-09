
#pragma once

#include "StrongTypes.h"
#include "Logger.h"

#include <chrono>
#include <condition_variable>
#include <format>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>

namespace kbot {

class Scheduler
{
friend struct std::formatter<kbot::Scheduler>;

public:
    struct Task
    {
        Task(TaskID task_id, TimePoint time_point, UserID user_id, std::function<void()> callback)
            : task_id(task_id)
            , time_point(time_point)
            , user_id(user_id)
            , callback(callback)
        {}

        TaskID task_id;
        TimePoint time_point;
        UserID user_id;
        std::function<void()> callback;
    };

public:
    Scheduler(Logger & log);
    ~Scheduler();

    void run();

    bool has_task(TaskID task_id);
    [[nodiscard("Save TaskID")]] TaskID enqueue_user_task(TimePoint time_point, UserID user_id, std::function<void()> callback);
    TaskID enqueue_system_task(TimePoint time_point, std::function<void()> callback);
    void delete_user_task(TaskID task_id);

    void delete_all_user_tasks_for(UserID user_id);

private:
    TaskID enqueue_task(TimePoint time_point, UserID user_id, std::function<void()> callback);

    TaskID gen_task_id();

    void stop();

private:
    Logger & m_log;

    std::unordered_map<TaskID, Task> m_storage;
    std::multimap<TimePoint, TaskID> m_time_index;
    std::unordered_map<UserID, std::set<TaskID>> m_user_id_index;
    TaskID m_last_task_id {0};

    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;
    std::jthread m_worker;   // waits/sleeps/works
};

} // namespace kbot


template <>
struct std::formatter<kbot::Scheduler::Task> : std::formatter<std::string_view>
{
    constexpr auto parse(std::format_parse_context & ctx) { return ctx.begin(); }

    template <class FormatContext>
    auto format(const kbot::Scheduler::Task & t, FormatContext & ctx) const
    {
        std::string s = std::format("Task(id={}, at={:%F %T}, user={})", t.task_id, t.time_point, t.user_id);
        return std::formatter<std::string_view>::format(s, ctx);
    }
};

// Remember to use under mutex std::unique_lock lk{s.m_mutex};
template <>
struct std::formatter<kbot::Scheduler> : std::formatter<std::string_view>
{
    constexpr auto parse(std::format_parse_context & ctx) {
        return ctx.begin();
    }

    auto format(const kbot::Scheduler& s, auto& ctx) const
    {
        using namespace std::chrono;

        // текущее время
        const system_clock::time_point now = floor<minutes>(system_clock::now());

        std::string out;
        out += "Scheduler contents:\n";

        out += "m_storage:\n";
        for (const auto & [id, task] : s.m_storage)
            out += std::format("  id={} task={}\n", id, task);

        out += "m_time_index:\n";
        for (const auto & [tp, id] : s.m_time_index)
            out += std::format("  {:%F %T} -> {}\n", tp, id);

        out += "m_user_id_index:\n";
        for (const auto & [user, task_ids] : s.m_user_id_index) {
            out += std::format("  user={} -> [", user);
            bool first = true;
            for (const kbot::TaskID & id : task_ids) {
                if (!first) out += ", ";
                out += std::format("{}", id);
                first = false;
            }
            out += "]\n";
        }

        out = std::format(
            "Scheduler\n"
            "----------\n"
            "now:  {:%F %T}\n"
            "{}",
            now, out);

        return std::formatter<std::string_view>::format(out, ctx);
    }
};