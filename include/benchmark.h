#ifndef _BENCHMARK_H
#define _BENCHMARK_H

#include <array>
#include <cstring>
#include <vector>
#include <memory>
#include <thread>
#include <unordered_map>


struct timeline_entry_t
{
    static constexpr const size_t NAME_LENGTH = 15;
    int64_t time;
    char name[NAME_LENGTH+1];

    timeline_entry_t(int64_t _time, const std::string& _name)
        : time(_time), name()
    {
        std::memcpy(reinterpret_cast<void*>(&name[0]), _name.c_str(), std::min(_name.length(), NAME_LENGTH));
    }

    timeline_entry_t& operator=(timeline_entry_t&& rhs)
    {
        rhs.time = 0;
        return *this;
    }

};

template <size_t N>
struct timeline_t
{
    std::array<timeline_entry_t, N> timeline;
    timeline_t ()
        :cur_index{0}
    {}

    bool append(const timeline_entry_t& entry)
    {
        if (cur_index >= N)
            return false;
        timeline[cur_index++] = std::move(entry);
        return true;
    }

    size_t current_size() const
    { return cur_index; }

private:
    size_t cur_index;
};

typedef timeline_t<2048> fixed_timeline_t;

void mark_timepoint(timeline_entry_t entry);

#endif
