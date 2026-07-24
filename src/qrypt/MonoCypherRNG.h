#include "OmniLogger.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <monocypher.h>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>

#include <bcrypt.h>
#else
#include <sys/random.h>
#endif

class MonocypherRNG
{
  private:
    std::array<uint8_t, 32> key{};
    std::array<uint8_t, 24> nonce{};
    uint64_t counter = 0;

    static bool OSEntropy(uint8_t* out, size_t len)
    {
#if defined(_WIN32)
        return BCryptGenRandom(
                   NULL, out, static_cast<ULONG>(len), BCRYPT_USE_SYSTEM_PREFERRED_RNG
               ) == 0;
#else
        return getrandom(out, len, 0) == static_cast<ssize_t>(len);
#endif
    }

  public:
    MonocypherRNG()
    {
        if (!OSEntropy(key.data(), key.size()) || !OSEntropy(nonce.data(), nonce.size())) {
            Logger::log("Failed to seed CSPRNG from OS!");
        }
    }

    ~MonocypherRNG()
    {
        crypto_wipe(key.data(), key.size());
        crypto_wipe(nonce.data(), nonce.size());
    }

    void Generate(uint8_t* dest, size_t length)
    {
        counter = crypto_chacha20_x(dest, nullptr, length, key.data(), nonce.data(), counter);
    }
};
