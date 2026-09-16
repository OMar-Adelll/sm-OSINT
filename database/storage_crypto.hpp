#pragma once

#include <string>

class StorageCrypto
{
public:
    explicit StorageCrypto(const std::string &hexKey);

    std::string encrypt(const std::string &plaintext) const;
    std::string decrypt(const std::string &ciphertext) const;
    std::string lookupDigest(const std::string &value) const;

private:
    std::string key;
};
