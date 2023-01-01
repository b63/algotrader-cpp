#ifndef _THREAD_QUEUE_H
#define _THREAD_QUEUE_H

#include <thread>
#include <queue>
#include <mutex>
#include <optional>
#include <functional>

template<typename ItemT>
class thread_queue
{
    using func_type = int(const ItemT&);
    private:
        const std::function<func_type> m_callable;
        const size_t m_max_size;
        std::unique_ptr<std::jthread> m_thread_ptr;
        std::queue<ItemT> m_queue;
        std::mutex m_mutex;
        std::stop_source m_stop_source;

        void _run(const std::stop_token& stoken)
        {
            while (!stoken.stop_requested())
            {
                ItemT item;
                if (!m_queue.pop_from_queue(item))
                    continue;

                m_callable(std::move(item));
            }

        }

    public:
        thread_queue(std::function<func_type> callable, size_t max_size)
            : m_callable {callable}, m_max_size {max_size}, m_thread_ptr {nullptr}
        {}

        bool add_to_queue(const ItemT &item)
        {
            std::lock_guard<std::mutex> guard {m_mutex};

            if (m_queue.size() >= m_max_size)
                return false;

            m_queue.push(item);
            return true;
        }

        bool pop_from_queue(ItemT &item)
        {
            std::lock_guard<std::mutex> guard {m_mutex};

            if (m_queue.size() == 0) return false;
            item = std::move(m_queue.front());
            m_queue.pop();

            return true;
        }

        bool is_queue_full()
        {
            std::lock_guard<std::mutex> guard {m_mutex};

            return m_queue.size() < m_max_size;
        }


        void run()
        {
            m_thread_ptr = std::make_unique<>(&thread_queue<ItemT>::_run, this, m_stop_source.get_token());
        }

        void join()
        {
            if (!m_thread_ptr) return;
            m_thread_ptr->join();
        }





};

#endif
