import json, hmac, hashlib, time, base64, requests

SECRET_KEY = "wPIb3XXXXXXXXXXXXXXtGGDFIRXsRCSm"
API_KEY    = "F7lqbXXXXXX9DBmf"
URL = "https://api.coinbase.com/api/v3/brokerage/accounts/"


def coinbase_rest_api(url, query_params=None, body=None, method="GET"):
    timestamp = str(int(time.time())) 
    request_path = '/' + '/'.join(url.split("/")[3:])


    if body:
        message = json.dumps(body)
    else:
        message = ""

    signature_plain = timestamp + method + request_path + message
    print(f"plain signature: {signature_plain}")
    signature = hmac.new(SECRET_KEY.encode('utf-8'), signature_plain.encode('utf-8'), digestmod=hashlib.sha256).hexdigest()
    headers = {
            "CB-ACCESS-KEY": API_KEY,
            "CB-ACCESS-TIMESTAMP": timestamp,
            "CB-ACCESS-SIGN": signature 
        }

    response = requests.request(method, url, params=query_params, data=message, headers=headers)
    return response


if __name__ == "__main__":
    res = coinbase_rest_api("https://api.coinbase.com/api/v3/brokerage/accounts")
    print(f"headers: {res.headers}")
    print(f"status_code: {res.status_code}")
    print(f"raw: {res.text}")
    try:
        print(f"<< {json.dumps(res.json(), indent=4)}")
    except:
        pass

