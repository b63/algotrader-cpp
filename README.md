# Algo Trader

Algo Trades maintains a local order book for select instrument pairs (eg. BTC-USD) 
by subscribing to the web socket market feed provided by [Binance US](https://docs.binance.us/#websocket-information)
and [Coinbase](https://docs.cloud.coinbase.com/advanced-trade-api/docs/ws-overview).
The local order book is then used in conjunction with various trading algorithmns to execute mock trades. Requires API key/secrets to execute real trades (the default ones are not valid).
- Arbritrage trades between exchanges. Latency was kept in mind so trade orders can be submitted with minimal latency once a profitable trade is found.
- Momentum trading [WIP]

## Example 
Running `test_trader` logs the profitable arbritrage trades for instrument pair ETH-USD:
```
Maximum profit bid: $ 0.576033, (Coinbase -> Binance) 1746.080000 @ 4.60826e-01 [$804.64]-> 1747.330000 @ 2.23000e+00 [$3896.55]
Maximum profit ask: $ 1.170000, (Coinbase -> Binance) 1746.170000 @ 7.36246e+00 [$12856.10] -> 1747.340000 @ 1.00000e+00 [$1747.34]
Maximum profit ask: $ 0.314400, (Coinbase -> Binance) 1746.110000 @ 3.26696e-01 [$570.45] -> 1747.420000 @ 2.40000e-01 [$419.38]
Maximum profit ask: $ 0.314400, (Coinbase -> Binance) 1746.110000 @ 3.26696e-01 [$570.45] -> 1747.420000 @ 2.40000e-01 [$419.38]
Maximum profit bid: $ 0.034320, (Coinbase -> Binance) 1746.100000 @ 2.60000e-02 [$45.40]-> 1747.420000 @ 2.40000e-01 [$419.38]
Maximum profit ask: $ 0.314400, (Coinbase -> Binance) 1746.110000 @ 3.26696e-01 [$570.45] -> 1747.420000 @ 2.40000e-01 [$419.38]
Maximum profit ask: $ 0.314400, (Coinbase -> Binance) 1746.110000 @ 3.26696e-01 [$570.45] -> 1747.420000 @ 2.40000e-01 [$419.38]
Maximum profit bid: $ 0.964442, (Coinbase -> Binance) 1746.110000 @ 7.41879e-01 [$1295.40]-> 1747.410000 @ 3.63160e+00 [$6345.89]
Maximum profit ask: $ 0.314400, (Coinbase -> Binance) 1746.110000 @ 7.41879e-01 [$1295.40] -> 1747.420000 @ 2.40000e-01 [$419.38]
Maximum profit bid: $ 0.647477, (Coinbase -> Binance) 1746.080000 @ 4.86825e-01 [$850.04]-> 1747.410000 @ 3.63160e+00 [$6345.89]
Maximum profit ask: $ 0.321600, (Coinbase -> Binance) 1746.080000 @ 4.86825e-01 [$850.04] -> 1747.420000 @ 2.40000e-01 [$419.38]
Maximum profit bid: $ 0.647477, (Coinbase -> Binance) 1746.080000 @ 4.86825e-01 [$850.04]-> 1747.410000 @ 3.63160e+00 [$6345.89]
Maximum profit bid: $ 0.425250, (Coinbase -> Binance) 1746.060000 @ 3.15000e-01 [$550.01]-> 1747.410000 @ 3.63160e+00 [$6345.89]
...
```

The first line states based on the state of the order book at that time, simultaneously submitting a bid order `{quantity: 0.460826, price: 1746.08}` (cost to Coinbase and an ask order 
`{quantity: 0.460826, price: 1747.33}` to Binance US nets $0.57 in profit.

### TUI Application
A toy TUI application in python is avaiable under `python/monitor_book.py` to monitor the order book:
```
mkdir -p logs && python monitor_book.py --coinbase ETH-USD --binance ETHUSD
```
![screenshot](https://github.com/b63/algotrader-cpp/blob/main/screenshots/tui.png?raw=true)

# Building
Requires the following:
- libcurl
- libopenssl (with libcrypto)
- C++20 complaint toolchain (Uses libstdc++ from gcc@12.0, libc++ from llvm@16.0 does not seem to support jthread)

Locally clone the repository and initialize the submodules.
```bash
git clone https://github.com/b63/algotrader-cpp
git submodule init && git submodule update
```

Build using cmake,
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -Bbuild
cmake --build build
```
Example binaries will be under `bin/`.
