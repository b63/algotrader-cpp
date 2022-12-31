#include "requests.h"

#include <mutex>

#include <curl/curl.h>


static std::mutex _MUTEX;
static bool _CURL_INITIALIZED = false;

void libcurl_cleanup()
{
    std::lock_guard<std::mutex> lock{_MUTEX};
    if (!_CURL_INITIALIZED)
        return;

    curl_global_cleanup();
    _CURL_INITIALIZED = false;
}

void libcurl_init()
{
    std::lock_guard<std::mutex> lock{_MUTEX};
    if (_CURL_INITIALIZED)
        return;

    curl_global_init(CURL_GLOBAL_ALL);
    _CURL_INITIALIZED = true;
}

std::string url_escape_curl(const std::string& str)
{
    char* escaped = curl_easy_escape(nullptr, str.c_str(), str.size());
    std::string escaped_str {escaped};
    if (escaped) curl_free(escaped);

    return escaped_str;
}

std::ostream& operator<<(std::ostream& ostream, const request_args_t& args)
{
   size_t n = args.m_keys.size();
   ostream << std::format("url: \"{}\", headers({:d}): ", args.m_url.str(), n);

   for (size_t i = 0; i < n; ++i)
   {
       ostream << std::format("(\"{}\", \"{}\")", args.m_keys[i], args.m_vals[i]);
       if (i+1 != n)
           ostream << ", ";
   }

   ostream << ", data: \"" << args.m_data << "\"";


    return ostream;
}
