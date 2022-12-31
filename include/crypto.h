#ifndef _CRYPTO_H
#define _CRYPTO_H

#include <openssl/evp.h>
#include <openssl/err.h>
#include <string>


void hmac(const std::string& msg, const std::string& key, std::string& digest);


#endif

