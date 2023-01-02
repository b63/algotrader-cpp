#ifndef _TRADER_H
#define _TRADER_H

#include "exchange_api.h"
#include "thread_queue.h"
#include "logger.h"

#include <mutex>

template <typename MarketFeed>
    requires is_market_feed<MarketFeed>
class ArbritrageTrader 
{


public:
    ArbritrageTrader<>(instrument_pair_t product_pair)
    {}

    bool feed_event_handler(const orderbook_t& book)
    {

    }
};

/** Trader is a any type that defines the following member function:
 *         bool feed_event_handler(const orderbook&)
 */
template <typename T>
concept is_trader = requires (T t, const orderbook_t& o){
    {t.feed_event_handler(o)} -> std::same_as<bool>;
};

template <typename Trader>
    requires is_trader<Trader>
struct GuardedFeedAdaptor
{
    GuardedFeedAdaptor<>(Trader& trader)
        : m_mutex{}, m_trader{trader}
    { }

    template <typename MarketFeed>
    void attach_to_feed(const feed_event_t& event, MarketFeed& mf) requires is_market_feed<MarketFeed>
    {
        // register this callable as the event handler
        mf.register_raw_event_handler(event, &decltype(*this)::_feed_handler, std::make_any<GuardedFeedAdaptor<Trader>*>(this));
    }

private:
    std::mutex m_mutex;
    Trader& m_trader;

    static constexpr size_t MAX_LOCK_ATTEMPTS = 4;

    static bool _feed_handler(const orderbook_t& book, std::any& this_ptr)
    {
        GuardedFeedAdaptor<Trader>* _this = std::any_cast<GuardedFeedAdaptor<Trader>*>(this_ptr);

        bool acquired = false;
        for (size_t i = 0; i < MAX_LOCK_ATTEMPTS; ++i)
        {
            // sometimes can't acquire lock even when no other thread has it 
            // so try just a couple of times
            if (!_this->m_mutex.try_lock()) continue;
            acquired = true;
            break;
        }

        if (!acquired) {
            // probably another thread has the lock, so ignore this event
            return true; 
        }

        try {
            return _this->m_trader.feed_event_handler(book);
        } catch (const std::exception& e) {
            _this->m_mutex.unlock();
            throw e;
        }
    }
};

template <typename Trader, typename ItemT>
    requires requires (Trader t, const orderbook_t& o)
        {
            is_trader<Trader>;
            {t.feed_event_to_queue_item(o)} -> std::same_as<ItemT>;
        }
struct QueueFeedAdaptor
{
    thread_queue<ItemT> queue;

    QueueFeedAdaptor<>(Trader& trader, size_t max_queue_size = 10)
        : m_trader{trader}, 
          queue {std::bind(&Trader::feed_event_handler, &m_trader, std::placeholders::_1), max_queue_size}
    { }

    template <typename MarketFeed>
    void attach_to_feed(const feed_event_t& event, MarketFeed& mf) requires is_market_feed<MarketFeed>
    {
        mf.register_event_handler(event, 
            [this](const orderbook_t& book) -> bool {
                this->queue.add_to_queue(this->m_trader.feed_event_to_queue_item(book));
                return true;
            });
    }

private:
    Trader& m_trader;
};

#endif
