import requests

headers = {"x-mbx-apikey": "bD9QfIu4XXXXXXXXXXXXXX6KMX2lb9oUyLfC2IknlE4vcIbnFKQaeSm8f0vLW8te"}
res = None


def f(ticker="BNBBTC", limit=0):
    if limit > 0:
        url = f"https://www.binance.us/api/v1/depth?symbol={ticker}&limit={limit}"
    else:
        url = f"https://www.binance.us/api/v1/depth?symbol={ticker}"
    c_res = requests.get(url, headers=headers)
    global res
    res = c_res
    resj = c_res.json()
    if "bids" in resj and "asks" in resj:
        bids, asks = resj["bids"], resj["asks"]
        update_id = resj["lastUpdateId"]
        print(f"{update_id} {len(bids)} bids, {len(asks)} asks")
    else:
        print(resj)
