#include "benchmark.h"
#include <mutex>
#include <mutex>

static std::unordered_map<std::thread::id, fixed_timeline_t> THREAD_TIMELINE_MAP;
static std::mutex THREAD_TIMELINE_MAP_MUTEX;

static thread_local fixed_timeline_t* THREAD_TIMELINE_PTR = nullptr;

fixed_timeline_t* add_thread_timeline_to_map()
{
    std::lock_guard<std::mutex> lock{THREAD_TIMELINE_MAP_MUTEX};

    return &THREAD_TIMELINE_MAP[std::this_thread::get_id()];
}

void mark_timepoint(timeline_entry_t entry)
{
    if (!THREAD_TIMELINE_PTR)
    {
        // add entry for current thread in thread-safe way
        THREAD_TIMELINE_PTR = add_thread_timeline_to_map();
    }

    THREAD_TIMELINE_PTR->append(entry);
}


void print_timeline_summary()
{
    // #TODO: calculate statistics using THREAD_TIMELINE_MAP and print them
}

