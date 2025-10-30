
#pragma once

#include "StrongTypes.h"
#include "Logger.h"

#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>

namespace kbot {

class Scheduler
{
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

    bool has_task(TaskID task_id);
    [[nodiscard("Save TaskID")]] TaskID enqueue_task(TimePoint time_point, UserID user_id, std::function<void()> callback);
    void delete_task(TaskID task_id);

    void delete_all_tasks_for(UserID user_id);

private:
    TaskID gen_task_id();

    void run();
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