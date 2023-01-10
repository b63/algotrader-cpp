#include "crypto.h"
#include "logger.h"

#include <exception>
#include <stdexcept>


static constexpr const char HEX_MAP[17] = "0123456789abcdef";

std::string to_hex_string(const std::string& bytes)
{
    const size_t n = bytes.length();

    std::string hex;
    hex.resize(n * 2);

    for (size_t i = 0; i < n; ++i)
    {
        hex[i*2]   = HEX_MAP[0xf & static_cast<int>(bytes[i] >> 4)];
        hex[i*2+1] = HEX_MAP[0xf & static_cast<int>(bytes[i])];
    }

    return hex;
}

void hmac(const std::string& msg, const std::string& key, std::string& digest)
{
    EVP_PKEY* pkey = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, nullptr, reinterpret_cast<const unsigned char*>(key.c_str()), static_cast<int>(key.length()));

    EVP_MD_CTX* ctx = nullptr;
    //try {
        ctx = EVP_MD_CTX_new(); 
        if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed with " + std::to_string(ERR_get_error()));

        int rc = EVP_DigestSignInit(ctx, nullptr, EVP_sha256(), nullptr, pkey);
        if (rc != 1) throw std::runtime_error("EVP_DigestSignInit failed with " + std::to_string(ERR_get_error()));

        rc = EVP_DigestSignUpdate(ctx, msg.data(), msg.size());
        if (rc != 1) throw std::runtime_error("EVP_DigestSignUpdate failed with " + std::to_string(ERR_get_error()));

        size_t req = 0;
        rc = EVP_DigestSignFinal(ctx, nullptr, &req);
        if (rc != 1) throw std::runtime_error("EVP_DigestSignFinal (1) failed with " + std::to_string(ERR_get_error()));

        digest.resize(req);
        rc = EVP_DigestSignFinal(ctx, reinterpret_cast<unsigned char*>(digest.data()), &req);
        if (rc != 1) throw std::runtime_error("EVP_DigestSignFinal (2) failed with " + std::to_string(ERR_get_error()));

        digest = to_hex_string(digest);
        //log("digest size: {:d}", req);

        EVP_MD_CTX_free(ctx);
        ctx = nullptr;
    //}
    //catch(const std::runtime_error &e) {
    //    if (ctx) EVP_MD_CTX_free(ctx);
    //    log("caught runtime_error in hmac: {}", e.what());
    //    throw e;
    //}
}


