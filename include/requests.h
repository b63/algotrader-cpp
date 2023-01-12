#ifndef _REQUESTS_H
#define _REQUESTS_H

#include "fmt/core.h"
#include "logger.h"
#include "config.h"

#include <curl/curl.h>

#include <iterator>
#include <vector>
#include <array>
#include <string>
#include <sstream>
#include <functional>

void libcurl_init();
void libcurl_cleanup();
std::string url_escape_curl(const std::string& str);

enum class ReqType
{
    GET,
    POST,
    DELETE
};


struct request_args_t {
    friend struct requests_t;

    friend std::ostream& operator<<(std::ostream& ostream, const request_args_t& args);

    ReqType type;

    request_args_t(const std::string& url, ReqType req_type) : type{req_type}, m_keys{},
        m_vals{}, m_url{}, m_data{}
    {
        m_url << url;
        has_query_string = (url.find('?') != std::string::npos);
    }

    std::string url_params_to_string() const
    {
        std::string url {m_url.str()};
        size_t pos = url.find_first_of('?');
        if (pos == std::string::npos)
            return "";

        return url.substr(pos+1);
    }

    request_args_t& add_url_param(const std::string& key, const std::string& value)
    {
        if (!has_query_string) 
        {
            m_url << "?";
            has_query_string = true;
        }
        else
        {
            m_url << "&";
        }

        m_url << url_escape_curl(key) << "=" << url_escape_curl(value);
        return *this;
    }

    request_args_t& add_header(const std::string& key, const std::string& val)
    {
        m_keys.emplace_back(key);
        m_vals.emplace_back(val);
        return *this;
    }

    request_args_t& set_type(ReqType req_type)
    {
        type = req_type;
        return *this;
    }

    request_args_t& add_header(std::string&& key, std::string&& val)
    {
        m_keys.emplace_back(std::move(key));
        m_vals.emplace_back(std::move(val));

        return *this;
    }

    request_args_t& set_data(const std::string& data)
    {
        m_data = data;
        return *this;
    }

    request_args_t& set_data(std::string&& data)
    {
        m_data = std::move(data);
        return *this;
    }

    std::string url()
    {
        return m_url.str();
    }

    auto begin()
    { return iterator<std::vector<std::string>::iterator>{m_keys.begin(), m_vals.begin()}; }

    auto cbegin() const
    { return iterator<std::vector<std::string>::const_iterator>{m_keys.cbegin(), m_vals.cbegin()}; }

    auto end()
    { return iterator<std::vector<std::string>::iterator>{m_keys.end(), m_vals.end()}; }

    auto cend() const
    { return iterator<std::vector<std::string>::const_iterator>{m_keys.cend(), m_vals.cend()}; }

    template <typename ptr_t>
    struct iterator
    {
        typedef std::pair<ptr_t,ptr_t> ret_t;

        iterator(const ptr_t& key_ptr, const ptr_t& val_ptr)
            : m_key_ptr (key_ptr), m_val_ptr(val_ptr)
        {}

        bool operator!=(const iterator& other) const
        { return m_key_ptr != other.m_key_ptr || m_val_ptr != other.m_val_ptr; }

        ret_t operator*() const
        { return ret_t{m_key_ptr, m_val_ptr}; };

        const iterator& operator++()
        {
            ++m_key_ptr;
            ++m_val_ptr;
            return *this;
        }


    private:
        ptr_t m_key_ptr;
        ptr_t m_val_ptr;
    };


private:
    std::vector<std::string> m_keys;
    std::vector<std::string> m_vals;
    std::stringstream m_url;
    std::string m_data;
    bool has_query_string = false;
};


struct requests_t {
    typedef std::vector<CURLcode> statuses_t;


    struct reader_state_t
    {
        size_t pos = 0;
        const std::string *string_ptr;

        reader_state_t(std::string *source) : string_ptr{source}
        {}

        reader_state_t(const reader_state_t&) = default;
        reader_state_t(reader_state_t&&) = default;
    };

    request_args_t& add_request(const std::string& url, ReqType reqtype = ReqType::GET)
    {
        m_request_args.emplace_back(url, reqtype);
        m_responses.emplace_back();
        m_error_buf.emplace_back(CURL_ERROR_SIZE+1, '\0');

        return m_request_args.back();
    }

    request_args_t& get_request_args(size_t index)
    {
        return m_request_args.at(index);
    }

    const request_args_t& get_request_args(size_t index) const
    {
        return m_request_args.at(index);
    }

    std::string get_response(size_t index)
    {
        return m_responses.at(index).str();
    }

    std::string get_error_msg(size_t index)
    {
        return std::string{static_cast<const char*>(m_error_buf[index].data())};
    }

    std::string get_error_msg(size_t index, CURLcode error_code)
    {
        std::string buf_str {static_cast<const char*>((m_error_buf[index]).data())};

        if (buf_str.size() > 0)
            return buf_str;

        // get error message using error code if no error message in the error message buffer
        return std::string{str_curl_error_code(error_code)};
    }


    void clear_responses()
    {
        for (auto& ss : m_responses)
            ss.clear();
        m_readers.clear();
    }


    size_t fetch_all(std::vector<CURLcode>& status)
    {
        const size_t n = m_request_args.size();
        std::vector<std::pair<CURL*, curl_slist*>>   handles;
        std::unordered_map<CURL*, size_t>            handles_index;

        for (int i = 0; i < n; ++i)
        {
            CURL* handle = curl_easy_init();
            struct curl_slist* slist  = nullptr;

            try {
                init_easy_handle(handle, slist, i);
                handles.push_back(std::pair<CURL*, curl_slist*>{handle, slist});
                handles_index.insert({handle, static_cast<size_t>(i)}); // save index
            } catch (const std::runtime_error& e) {
                curl_free(handles); // free previous handles

                // free current handle that raised exception
                if (slist) curl_slist_free_all(slist);
                curl_easy_cleanup(handle);

                throw e;
            }
        }

        CURLM* multi_handle = curl_multi_init();
        for (const auto& [handle, slist] : handles)
            curl_multi_add_handle(multi_handle, handle);

        try {
            poll(multi_handle);
        }
        catch (const std::runtime_error& e)
        {
            curl_free(handles); // free easy handles
            curl_multi_cleanup(multi_handle);
            throw e;
        }

        status.resize(n);
        struct CURLMsg* m;
        size_t success = 0;
        do {
            int msgq;
            m = curl_multi_info_read(multi_handle, &msgq);
            if (m && (m->msg == CURLMSG_DONE))
            {
                size_t index = handles_index[m->easy_handle];
                CURLcode res = m->data.result;
                status[index] = res;

                if (res == CURLE_OK)
                    ++success;
            }
        } while (m);

        curl_free(handles); // free easy handles
        curl_multi_cleanup(multi_handle);

        // success <= n
        // return number of requests that failed, should be zero if all requests were successful
        return n - success;
    }

    size_t size() const 
    {
        return m_request_args.size();
    }


private:
    void poll(CURLM*& multi_handle)
    {
        int running_handles;
        do {
            CURLMcode mc = curl_multi_perform(multi_handle, &running_handles);
            if (mc)
                throw std::runtime_error(std::format("curl_multi_perform failed with {:d}", (int) mc));

            if (running_handles)
                mc = curl_multi_poll(multi_handle, nullptr, 0, 100, nullptr);
        } while (running_handles);
    }

    void curl_free(const std::vector<std::pair<CURL*, curl_slist*>>& ptr_vec)
    {
        for (const auto& pair : ptr_vec) 
        {
            CURL* handle = pair.first;
            curl_slist* slist  = pair.second;

            if (slist)  curl_slist_free_all(slist);
            if (handle) curl_easy_cleanup(handle);
        }
    }


    void init_easy_handle(CURL*& handle, curl_slist*& slist, size_t i)
    {
        curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &m_responses[i]);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &requests_t::write_callback);
        curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, m_error_buf[i].data());
        curl_easy_setopt(handle, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 5L);
#ifdef VERBOSE_CURL_REQUESTS
        curl_easy_setopt(handle, CURLOPT_DEBUGFUNCTION, &requests_t::debug_callback);
        curl_easy_setopt(handle, CURLOPT_DEBUGDATA, this);
        curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
#endif

        if(m_request_args[i].type == ReqType::POST)
        {
            curl_easy_setopt(handle, CURLOPT_POST, 1);

            std::string full_url {m_request_args[i].url()};
            size_t pos = full_url.find_first_of('?');

            curl_easy_setopt(handle, CURLOPT_URL, full_url.substr(0, pos).c_str());
            if (pos != full_url.npos)
                curl_easy_setopt(handle, CURLOPT_POSTFIELDS, full_url.substr(pos+1).c_str());

        }
        else if(m_request_args[i].type == ReqType::GET)
        {
            curl_easy_setopt(handle, CURLOPT_HTTPGET, 1);
            curl_easy_setopt(handle, CURLOPT_URL, m_request_args[i].url().c_str());
        }
        else if(m_request_args[i].type == ReqType::DELETE)
        {
            curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "DELETE");
            curl_easy_setopt(handle, CURLOPT_URL, m_request_args[i].url().c_str());
        }
        else
        {
            curl_easy_setopt(handle, CURLOPT_URL, m_request_args[i].url().c_str());
        }

        if (m_request_args[i].m_data.size() > 0)
        {
            m_readers.emplace(i, &m_request_args[i].m_data);

            curl_easy_setopt(handle, CURLOPT_READDATA, &m_readers.at(i));
            curl_easy_setopt(handle, CURLOPT_READFUNCTION, &requests_t::read_callback);
        }

        struct curl_slist* temp  = nullptr;
        for (const auto& [key_it, value_it] : m_request_args[i])
        {
            std::string header_str {*key_it + ": " + *value_it};
            temp = curl_slist_append(slist, header_str.c_str());

            if (!temp)
            {
                throw std::runtime_error("curl_slist_append returned NULL, failed to add header: " + header_str);
            }

            slist = temp;
        }

        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, slist);

    }


    static const char* str_curl_error_code(CURLcode ec)
    { return curl_easy_strerror(ec); }

    static size_t read_callback(char* buffer, size_t size, size_t nitems, void *reader_state_ptr)
    {
        reader_state_t& reader = *reinterpret_cast<reader_state_t*>(reader_state_ptr);
        const size_t remaining = reader.string_ptr->size() - std::min(reader.string_ptr->size(), reader.pos);
        const size_t to_read = std::min(size*nitems, remaining);
        if (to_read == 0)
        {
            reader.pos = reader.string_ptr->size();
            return 0;
        }

        const char* start = reader.string_ptr->data() + reader.pos;
        std::memcpy(buffer, start, to_read);
        reader.pos += to_read;
        return to_read;
    }

    static size_t write_callback(void* contents, size_t size, size_t bytes, void *sstream_ptr)
    {
        std::stringstream& ss = *reinterpret_cast<std::stringstream*>(sstream_ptr);
        ss.write(reinterpret_cast<const char*>(contents), size*bytes);

        return size*bytes;
    }

    static int debug_callback(CURL* handle, curl_infotype type, char* data, size_t size, void* this_ptr)
    {
        //requests_t& _this = *reinterpret_cast<requests_t*>(this_ptr);
        std::string prefix;
        bool ssl_data = false;
        switch(type)
        {
            case CURLINFO_TEXT:
                log_noln("== Info: {}", std::string(data, size));
                // explicit fallthrough
            default:
                return 0;

            case CURLINFO_HEADER_OUT:
                prefix = "=> Send header";
                break;
            case CURLINFO_DATA_OUT:
                prefix = "=> Send data";
                break;
            case CURLINFO_SSL_DATA_OUT:
                ssl_data = true;
                prefix = "=> Send SSL data";
                break;
            case CURLINFO_HEADER_IN:
                prefix = "<= Recv header";
                break;
            case CURLINFO_SSL_DATA_IN:
                ssl_data = true;
                prefix = "<= Recv SSL data";
                break;
        }

        if (!ssl_data)
        {
            log("{}\n{}", prefix, std::string(data, size));
            return 0;
        }

        log("{}: {:d} bytes", prefix, size);
        return 0;
    }

    std::vector<request_args_t>                m_request_args;
    std::vector<std::stringstream>             m_responses;
    std::vector<std::string>                   m_error_buf;
    std::unordered_map<size_t, reader_state_t> m_readers;
};



#endif
