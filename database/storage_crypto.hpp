#pragma once

#include <string>

// Encrypts database fields using AES-256-GCM. The supplied key must be a
// 32-byte key encoded as 64 hexadecimal characters; it is never stored here.
class StorageCrypto
{
public:
    explicit StorageCrypto(const std::string &hexKey);

    std::string encrypt(const std::string &plaintext) const;
    std::string decrypt(const std::string &ciphertext) const;

    // A keyed digest supports equality/unique lookup without exposing the
    // normalized username to an offline database dump.
    std::string lookupDigest(const std::string &value) const;

private:
    std::string key;
};
