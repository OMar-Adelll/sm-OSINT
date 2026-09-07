#include "storage_crypto.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace
{
constexpr std::size_t KeyBytes = 32;
constexpr std::size_t NonceBytes = 12;
constexpr std::size_t TagBytes = 16;

unsigned char hexNibble(char character)
{
    if (character >= '0' && character <= '9') return static_cast<unsigned char>(character - '0');
    if (character >= 'a' && character <= 'f') return static_cast<unsigned char>(character - 'a' + 10);
    if (character >= 'A' && character <= 'F') return static_cast<unsigned char>(character - 'A' + 10);
    throw std::invalid_argument("encryption key must be hexadecimal");
}

std::string decodeKey(const std::string &hex)
{
    if (hex.size() != KeyBytes * 2)
        throw std::invalid_argument("SM_OSINT_ENCRYPTION_KEY must be exactly 64 hexadecimal characters");

    std::string decoded(KeyBytes, '\0');
    for (std::size_t i = 0; i < KeyBytes; ++i)
        decoded[i] = static_cast<char>((hexNibble(hex[i * 2]) << 4) | hexNibble(hex[i * 2 + 1]));
    return decoded;
}

std::string base64Encode(const unsigned char *data, std::size_t length)
{
    std::string encoded(4 * ((length + 2) / 3), '\0');
    const int written = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(&encoded[0]), data, static_cast<int>(length));
    encoded.resize(static_cast<std::size_t>(written));
    return encoded;
}

std::string base64Decode(const std::string &encoded)
{
    if (encoded.empty() || encoded.size() % 4 != 0)
        throw std::runtime_error("invalid encrypted database field");

    std::string decoded((encoded.size() / 4) * 3, '\0');
    const int written = EVP_DecodeBlock(reinterpret_cast<unsigned char *>(&decoded[0]),
                                        reinterpret_cast<const unsigned char *>(encoded.data()),
                                        static_cast<int>(encoded.size()));
    if (written < 0)
        throw std::runtime_error("invalid encrypted database field");

    std::size_t padding = 0;
    if (!encoded.empty() && encoded.back() == '=') ++padding;
    if (encoded.size() > 1 && encoded[encoded.size() - 2] == '=') ++padding;
    decoded.resize(static_cast<std::size_t>(written) - padding);
    return decoded;
}

std::array<std::string, 3> splitEnvelope(const std::string &envelope)
{
    if (envelope.rfind("v1.", 0) != 0)
        throw std::runtime_error("unsupported encrypted database field version");

    const std::size_t second = envelope.find('.', 3);
    const std::size_t third = second == std::string::npos ? std::string::npos : envelope.find('.', second + 1);
    if (second == std::string::npos || third == std::string::npos || envelope.find('.', third + 1) != std::string::npos)
        throw std::runtime_error("invalid encrypted database field");
    return {envelope.substr(3, second - 3), envelope.substr(second + 1, third - second - 1), envelope.substr(third + 1)};
}
}

StorageCrypto::StorageCrypto(const std::string &hexKey) : key(decodeKey(hexKey)) {}

std::string StorageCrypto::encrypt(const std::string &plaintext) const
{
    std::array<unsigned char, NonceBytes> nonce{};
    std::array<unsigned char, TagBytes> tag{};
    if (RAND_bytes(nonce.data(), static_cast<int>(nonce.size())) != 1)
        throw std::runtime_error("could not generate encryption nonce");

    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    if (context == nullptr) throw std::runtime_error("could not create encryption context");

    std::vector<unsigned char> encrypted(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
    int written = 0;
    int finalWritten = 0;
    const bool ok = EVP_EncryptInit_ex(context, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
                    EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, NonceBytes, nullptr) == 1 &&
                    EVP_EncryptInit_ex(context, nullptr, nullptr,
                                       reinterpret_cast<const unsigned char *>(key.data()), nonce.data()) == 1 &&
                    EVP_EncryptUpdate(context, encrypted.data(), &written,
                                      reinterpret_cast<const unsigned char *>(plaintext.data()), static_cast<int>(plaintext.size())) == 1 &&
                    EVP_EncryptFinal_ex(context, encrypted.data() + written, &finalWritten) == 1 &&
                    EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_GET_TAG, TagBytes, tag.data()) == 1;
    EVP_CIPHER_CTX_free(context);
    if (!ok) throw std::runtime_error("could not encrypt database field");

    encrypted.resize(static_cast<std::size_t>(written + finalWritten));
    return "v1." + base64Encode(nonce.data(), nonce.size()) + "." +
           base64Encode(encrypted.data(), encrypted.size()) + "." + base64Encode(tag.data(), tag.size());
}

std::string StorageCrypto::decrypt(const std::string &envelope) const
{
    const auto parts = splitEnvelope(envelope);
    const std::string nonce = base64Decode(parts[0]);
    const std::string ciphertext = base64Decode(parts[1]);
    const std::string tag = base64Decode(parts[2]);
    if (nonce.size() != NonceBytes || tag.size() != TagBytes)
        throw std::runtime_error("invalid encrypted database field");

    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    if (context == nullptr) throw std::runtime_error("could not create decryption context");
    std::vector<unsigned char> plaintext(ciphertext.size() + EVP_MAX_BLOCK_LENGTH);
    int written = 0;
    int finalWritten = 0;
    const bool initialized = EVP_DecryptInit_ex(context, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
                             EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_IVLEN, NonceBytes, nullptr) == 1 &&
                             EVP_DecryptInit_ex(context, nullptr, nullptr,
                                                reinterpret_cast<const unsigned char *>(key.data()),
                                                reinterpret_cast<const unsigned char *>(nonce.data())) == 1 &&
                             EVP_DecryptUpdate(context, plaintext.data(), &written,
                                               reinterpret_cast<const unsigned char *>(ciphertext.data()),
                                               static_cast<int>(ciphertext.size())) == 1 &&
                             EVP_CIPHER_CTX_ctrl(context, EVP_CTRL_GCM_SET_TAG, TagBytes,
                                                 const_cast<char *>(tag.data())) == 1;
    const int finalized = initialized ? EVP_DecryptFinal_ex(context, plaintext.data() + written, &finalWritten) : 0;
    EVP_CIPHER_CTX_free(context);
    if (finalized != 1) throw std::runtime_error("encrypted database field failed authentication");
    return std::string(reinterpret_cast<char *>(plaintext.data()), static_cast<std::size_t>(written + finalWritten));
}

std::string StorageCrypto::lookupDigest(const std::string &value) const
{
    unsigned int length = 0;
    unsigned char digest[EVP_MAX_MD_SIZE];
    if (HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()),
             reinterpret_cast<const unsigned char *>(value.data()), value.size(), digest, &length) == nullptr)
        throw std::runtime_error("could not create username lookup digest");

    std::ostringstream output;
    for (unsigned int i = 0; i < length; ++i)
        output << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(digest[i]);
    return output.str();
}
