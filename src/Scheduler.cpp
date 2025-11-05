#include "Scheduler.h"
#include <format>
#include <iostream>

kbot::Scheduler::Scheduler(Logger & log)
    : m_log(log)
{}

kbot::Scheduler::~Scheduler()
{
    stop();     // m_worker makes join via RAII
}

kbot::TaskID kbot::Scheduler::gen_task_id()
{
    return ++m_last_task_id; // 1..
}

bool kbot::Scheduler::has_task(TaskID task_id)
{
    std::lock_guard lk(m_mutex);

    return m_storage.contains(task_id);
}

kbot::TaskID kbot::Scheduler::enqueue_system_task(TimePoint time_point, std::function<void()> callback)
{
    TaskID task_id;
    {
        std::lock_guard lk(m_mutex);
        task_id = gen_task_id();
        m_storage.emplace(task_id, Task(task_id, time_point, UserID{}, callback));
        m_time_index.emplace(time_point, task_id);

        // TODO: maybe remove later
        m_log.debug("{} after: {}", __func__, *this);
    }
    m_cv.notify_one();
    return task_id;
}

kbot::TaskID kbot::Scheduler::enqueue_user_task(TimePoint time_point, UserID user_id, std::function<void()> callback)
{
    TaskID task_id;
    {
        std::lock_guard lk(m_mutex);
        task_id = gen_task_id();
        m_storage.emplace(task_id, Task(task_id, time_point, user_id, callback));
        m_user_id_index[user_id].insert(task_id);
        m_time_index.emplace(time_point, task_id);

        // TODO: maybe remove later
        m_log.debug("{} after: {}", __func__, *this);
    }
    m_cv.notify_one();
    return task_id;
}

void kbot::Scheduler::delete_user_task(TaskID task_id)
{
    if (task_id.empty()) {
        m_log.warn("{}: tried to remove task #{}, but it does not exists", __func__, task_id);
        return;
    }

    std::lock_guard lk(m_mutex);

    // m_storage
    const std::unordered_map<TaskID, Task>::node_type popped_task = m_storage.extract(task_id);
    if (popped_task.empty()) {
        m_log.warn("{}: task #{} not exists", __func__, task_id);
        return;
    }
    const Task & task = popped_task.mapped();

    // m_time_index
    auto [it_first, it_last] = m_time_index.equal_range(task.time_point);
    for (auto it = it_first; it != it_last;) {
        if (task_id == it->second) {
            it = m_time_index.erase(it);
        }
        else {
            ++it;
        }
    }

    // m_user_id_index
    m_user_id_index[task.user_id].erase(task.task_id);
}

void kbot::Scheduler::delete_all_user_tasks_for(UserID user_id)
{
    std::lock_guard lk(m_mutex);

    if (!m_user_id_index.contains(user_id))
    {
        m_log.debug("{}: user #{} has no tasks already", __func__, user_id);
        return;
    }

    // Clean at m_user_id_index
    const std::set<TaskID> task_ids_to_delete = std::move(m_user_id_index.extract(user_id).mapped());

    for (const TaskID task_id : task_ids_to_delete) {
        // Clean at m_storage
        const Task task = std::move(m_storage.extract(task_id).mapped());

        // Clean at m_time_index
        auto [it_first, it_last] = m_time_index.equal_range(task.time_point);
        for (auto it = it_first; it != it_last;) {
            if (task_id == it->second) {
                it = m_time_index.erase(it);
            }
            else {
                ++it;
            }
        }
        // RAII for m_storage
    }
    // RAII for m_user_id_index
}

void kbot::Scheduler::stop()
{
    {
        std::lock_guard lk(m_mutex);
        m_stop = true;
    }
    m_cv.notify_one();
}

void kbot::Scheduler::run()
{
    m_worker = std::jthread{[this]
    {
        m_log.debug("run: start worker");

        while (!m_stop) {
            std::unique_lock lk(m_mutex);

            if (m_time_index.empty()) {
                m_cv.wait(lk, [&]{ return m_stop || !m_time_index.empty(); });
                continue;
            }

            const TimePoint closest_action_tp = m_time_index.begin()->first;

            m_log.debug("run: worker loop checkpoint 1. Closest tp: {:%F %T}", closest_action_tp);

            if (m_cv.wait_until(lk, closest_action_tp, [&]{ return m_stop; })) {
                break;
            }

            m_log.debug("run: worker loop checkpoint 2");

            if (m_time_index.empty()) {
                continue;
            }

            m_log.debug("run: worker loop checkpoint 3");

            std::multimap<TimePoint, TaskID>::iterator closest_action_it = m_time_index.begin();

            if (std::chrono::system_clock::now() >= /*TimePoint*/ closest_action_it->first) {
                // Extract task and erase indices
                const Task task = std::move(m_storage.extract(/*TaskID*/ closest_action_it->second).mapped());
                m_time_index.erase(closest_action_it);

                if (!task.user_id.empty() && m_user_id_index.contains(task.user_id)) {
                    m_user_id_index[task.user_id].erase(task.task_id);
                }

                // TODO: Maybe remove later
                m_log.debug("{} executed: ", __func__, task);
                m_log.debug("{} scheduler after execution: ", __func__, *this);

                // Call callback
                lk.unlock();
                task.callback();

                // RAII for task
            }
        }

        m_log.debug("{}: end", __func__);
    }};
}
