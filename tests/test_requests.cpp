#include "requests.h"
#include "json.h"
#include "config.h"
#include <iostream>
#include <chrono>

int main(int argc, const char **argv)
{
    libcurl_init();

    size_t MAXCHARS = 100;
    if (argc == 2)
        MAXCHARS = std::stoll(argv[1]);


    requests_t req{};
    // add all requests that need to be done simulatniusly 
    req.add_request("https://www.google.com")
       .add_url_param("q", "New Year's Eve")
       .add_url_param("hl", "en");

    req.add_request("https://www.facebook.com")
       .add_header("Connection", "keep-alive");

    req.add_request("https://this_url_should_not_exist_.dankmemes99.com");

    req.add_request("https://www.binance.us/api/v1/depth")
       .add_header("x-mbx-apikey", "bD9QfIu4FBdXXXXXXXXXXXXXXX2lb9oUyLfC2IknlE4vcIbnFKQaeSm8f0vLW8te")
       .add_url_param("symbol", "BTC-USD")
       .add_url_param("limit", "5000");


    // won't work just for demonstration,
    // need proper authentiaction see https://docs.cloud.coinbase.com/advanced-trade-api/docs/rest-api-auth
    using namespace std::chrono;
    std::string ts {std::to_string(system_clock::now().time_since_epoch().count())};
    req.add_request("https://api.coinbase.com/api/v3/brokerage/orders/", ReqType::POST)
       .add_header("CB-ACCESS-KEY", "PUBLIC-API-KEY-GOES-HERE")
       .add_header("CB-ACCESS-TIMESTAMP", "123443555")
       .add_header("CB-ACCESS-SIGN", "HMAC hex digest of <timestamp><method><request path><body>")
       .set_data(DocumentCreator<Document>()
                  .AddString("client_order_id", "232323")
                  .AddString("product_id", "BTC-USD")
                  .AddString("product_id", "BTC-USD")
                  .AddString("end_time", ts)
                  .as_string());

    // block until all requests are complete
    std::vector<CURLcode> status;
    size_t res = req.fetch_all(status);

    if (res != 0)
    {
        std::cerr << std::format("fetch_all -> {}, [ ", res);
        for (auto& i : status)
            std::cerr << i  << " ";
        std::cerr << "]\n\n";
    }

    for (int i = 0 ; i < req.size(); ++i)
    {
        if (i > 0) std::cout << "\n";
        std::cout << "\e[1mRequest " << i << "\e[0m\n" << req.get_request_args(i) << "\n";
        std::string response {req.get_response(i)};
        std::cout << "Error Msg: \"" << req.get_error_msg(i, (CURLcode) status[i]) << "\"\n";

        std::cout << std::format("Response:({:d} bytes)\n", response.size());
        if (response.size() > MAXCHARS)
        {
            std::cout << response.substr(0, std::min(response.size(), MAXCHARS)) << "...\n";
        }
        else
        {
            std::cout << response << "\n";
        }
    }

    libcurl_cleanup();
}
