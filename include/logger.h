#ifndef _LOGGER_H
#define _LOGGER_H

#include <format.h>

template <typename... Args>
void log(const char *fmt, Args&&... args)
{
    std::string fstr {std::format(std::runtime(fmt), std::forward<Args>(args)...)};
    std::cerr << fstr << "\n";
}


#endif

