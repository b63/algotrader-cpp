#ifndef _MARKET_SOCKET_H
#define _MARKET_SOCKET_H

#include "json.h"
#include "logger.h"

#define ASIO_STANDALONE
#include <asio.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#include <string_view>


using client          = websocketpp::client<websocketpp::config::asio_tls_client>;
using ssl_context_ptr = std::shared_ptr<asio::ssl::context>;


struct market_feed_socket
{
    market_feed_socket(const std::string& uri, std::function<bool(const Document&)> on_message)
        : uri {uri}, m_client{}, m_con_ptr {nullptr}, m_on_message_hdlr {on_message}, 
        m_opening_msgs{}, m_headers {},
        m_io_context {}
    {
        using namespace std::placeholders;

        m_client.set_access_channels(websocketpp::log::alevel::all);
        m_client.clear_access_channels(websocketpp::log::alevel::frame_payload);
        m_client.set_error_channels(websocketpp::log::elevel::all);

        m_client.init_asio(&m_io_context);

        m_client.set_message_handler(std::bind(&market_feed_socket::on_message, this, _1, _2));
        m_client.set_open_handler(std::bind(&market_feed_socket::on_open, this, _1));
        m_client.set_tls_init_handler(std::bind(&market_feed_socket::mock_tls_init_handler, this, _1));
    }

    void add_opening_message_json(const Document& json)
    {
        m_opening_msgs.push_back(to_string<Document>(json));
    }

    std::error_code send_json(const Document& json)
    {
        if(!m_con_ptr)
            throw std::logic_error("tried to send payload when connection is null");

        std::string msg {to_string<Document>(json)};
        log("sending: {}\n", msg);
        return m_con_ptr->send(msg);
    }

    void add_header(const std::string& key, const std::string& value)
    {
        m_headers.push_back(std::pair<std::string, std::string>{key, value});
    }

    void close()
    {
        // close connection from thread running io_context event loop
        asio::post(m_io_context, [&]() {
                if(!m_con_ptr) return;
                log("closing socket ...");
                m_con_ptr->close(websocketpp::close::status::normal, "OK");
            });
    }

    bool connect(std::error_code &ec)
    {
        m_con_ptr = m_client.get_connection(uri, ec);
        if (ec) {
            return false;
        }

        for (const auto& [key, value] : m_headers)
        {
            m_con_ptr->append_header(key, value);
        }

        m_client.connect(m_con_ptr);
        m_client.run();

        m_con_ptr = nullptr;

        return  true;
    }


    ssl_context_ptr mock_tls_init_handler(websocketpp::connection_hdl hdl)
    {
        ssl_context_ptr ctx_ptr = std::make_shared<asio::ssl::context>(asio::ssl::context::tls);
        /* TODO: more TLS configuration here */
        return ctx_ptr;
    }

    std::error_code send(const std::string& msg)
    {
        if(!m_con_ptr)
            throw std::logic_error("tried to send payload when connection is null");

        log("sending message: {}", msg);
        return m_con_ptr->send(msg);
    }


    const std::string uri;

private:
    client m_client;
    client::connection_ptr m_con_ptr;
    std::function<bool(const Document&)> m_on_message_hdlr;
    std::vector<std::string> m_opening_msgs;
    std::vector<std::pair<std::string, std::string>> m_headers;
    asio::io_context m_io_context;

    void on_message(websocketpp::connection_hdl hdl, client::message_ptr msg)
    {
        std::string& payload {msg->get_raw_payload()};
        if (!payload.ends_with('\0'))
            payload.push_back('\0');

        Document json;
        json.ParseInsitu(payload.data());

        if (!m_on_message_hdlr(json))
        {
            m_client.get_con_from_hdl(hdl)->close(websocketpp::close::status::normal, "OK");
        }
    }

    void on_open(websocketpp::connection_hdl hdl)
    {
        if (m_opening_msgs.size() == 0)
            return;
        for (const auto& msg : m_opening_msgs)
        {
            log("sending opening message: {}", msg);
            std::error_code ec = m_client.get_con_from_hdl(hdl)->send(msg);
            if (ec) {
                throw std::runtime_error("failed to send opening message: " + ec.message());
            }
        }
    }
};








#endif

