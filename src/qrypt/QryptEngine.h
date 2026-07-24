#pragma once

#include "MonoCypherRNG.h"

extern "C" {
#include <monocypher.h>
}

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace X25519Impl {
inline void GenerateKeyPair(uint8_t PublicKeyDest[32], uint8_t PrivateKeyDest[32])
{
    thread_local static MonocypherRNG RNGEngine;
    RNGEngine.Generate(PrivateKeyDest, 32);

    crypto_x25519_public_key(PublicKeyDest, PrivateKeyDest);
}

inline void
DeriveSecret(uint8_t Dest[32], const uint8_t PrivateKey[32], const uint8_t PublicKey[32])
{
    crypto_x25519(Dest, PrivateKey, PublicKey);
}

inline void Blake2bHash(const uint8_t* Data, size_t Len, uint8_t Output[32])
{
    crypto_blake2b(Output, 32, Data, Len);
}

inline void Blake2bKeyedHash(
    const uint8_t* Key, size_t KeyLen, const uint8_t* Data, size_t DataLen, uint8_t Output[32]
)
{
    crypto_blake2b_keyed(Output, 32, Key, KeyLen, Data, DataLen);
}
} // namespace X25519Impl

struct ECDHSession
{
    std::array<uint8_t, 32> PrivateKey{};
    std::array<uint8_t, 32> LocalPublicKey{};
    std::array<uint8_t, 32> RemotePublicKey{};
    std::array<uint8_t, 32> SharedSecret{};
    int32_t Passkey = -1;
    bool KeyPairGenerated = false;
    bool SecretDerived = false;
    bool Authenticated = false;

    void SecureWipe()
    {
        crypto_wipe(PrivateKey.data(), PrivateKey.size());
        crypto_wipe(SharedSecret.data(), SharedSecret.size());
    }

    ~ECDHSession() { SecureWipe(); }
};

// Cryptographic Session & Key Exchange Engine.
// X25519 ECDH key exchange, Passkey generation, Keyed HMAC Derivation primitives.
template <typename UniqueKeyType = uint64_t> class QryptEngine
{
  protected:
    std::unordered_map<UniqueKeyType, ECDHSession> ActiveSessions;

  public:
    QryptEngine() = default;
    virtual ~QryptEngine() = default;

    // Generates a X25519 keypair for a session identified by uniqueKey.
    std::vector<uint8_t> GenerateKeyPair(const UniqueKeyType& UniqueKey)
    {
        ECDHSession& session = ActiveSessions[UniqueKey];
        if (session.KeyPairGenerated) {
            return std::vector<uint8_t>(
                session.LocalPublicKey.begin(), session.LocalPublicKey.end()
            );
        }

        session.SecretDerived = false;
        session.Authenticated = false;

        X25519Impl::GenerateKeyPair(session.LocalPublicKey.data(), session.PrivateKey.data());
        session.KeyPairGenerated = true;

        return std::vector<uint8_t>(session.LocalPublicKey.begin(), session.LocalPublicKey.end());
    }

    // Derives the shared secret using the remote peer's public key.
    bool DeriveSecret(const UniqueKeyType& UniqueKey, const uint8_t* RemotePublicKey, size_t KeyLen)
    {
        if (KeyLen != 32 || ActiveSessions.find(UniqueKey) == ActiveSessions.end()) {
            return false;
        }

        ECDHSession& session = ActiveSessions[UniqueKey];
        std::copy_n(RemotePublicKey, 32, session.RemotePublicKey.begin());

        X25519Impl::DeriveSecret(
            session.SharedSecret.data(), session.PrivateKey.data(), session.RemotePublicKey.data()
        );

        session.SecretDerived = true;
        session.Authenticated = false;
        GeneratePasskey(UniqueKey);
        return true;
    }

    // Marks the session as Authenticated
    void AuthenticateSession(const UniqueKeyType& UniqueKey)
    {
        auto iter = ActiveSessions.find(UniqueKey);
        if (iter != ActiveSessions.end() && iter->second.SecretDerived) {
            iter->second.Authenticated = true;
        }
    }

    // Generates a 6 digit passkey
    // Lexico battle ensures key ordering stays the same on both local and remote.
    // Hashes both public keys and the derived secret which turns into the passkey so no MitMs
    int32_t GeneratePasskey(const UniqueKeyType& UniqueKey)
    {
        auto iter = ActiveSessions.find(UniqueKey);
        if (iter == ActiveSessions.end() || !iter->second.SecretDerived) {
            return -1;
        }

        ECDHSession& Session = iter->second;

        bool LexicoCheck = std::lexicographical_compare(
            Session.LocalPublicKey.begin(),
            Session.LocalPublicKey.end(),
            Session.RemotePublicKey.begin(),
            Session.RemotePublicKey.end()
        );

        std::array<uint8_t, 96> HashInput;
        const auto& FirstKey = LexicoCheck ? Session.LocalPublicKey : Session.RemotePublicKey;
        const auto& SecondKey = LexicoCheck ? Session.RemotePublicKey : Session.LocalPublicKey;
        std::memcpy(HashInput.data(), FirstKey.data(), 32);
        std::memcpy(HashInput.data() + 32, SecondKey.data(), 32);
        std::memcpy(HashInput.data() + 64, Session.SharedSecret.data(), 32);

        std::array<uint8_t, 32> HashOutput{};
        X25519Impl::Blake2bHash(HashInput.data(), HashInput.size(), HashOutput.data());

        uint32_t Token = (static_cast<uint32_t>(HashOutput[0]) << 24) |
                         (static_cast<uint32_t>(HashOutput[1]) << 16) |
                         (static_cast<uint32_t>(HashOutput[2]) << 8) |
                         static_cast<uint32_t>(HashOutput[3]);

        Session.Passkey = static_cast<int32_t>(Token % 1000000);
        return Session.Passkey;
    }

    int32_t GetPasskey(const UniqueKeyType& UniqueKey) const
    {
        auto iter = ActiveSessions.find(UniqueKey);
        if (iter != ActiveSessions.end()) {
            return iter->second.Passkey;
        }
        return -1;
    }

    bool SessionAuthState(const UniqueKeyType& UniqueKey) const
    {
        auto iter = ActiveSessions.find(UniqueKey);
        return iter != ActiveSessions.end() && iter->second.SecretDerived &&
               iter->second.Authenticated;
    }

    // Computes a 32-byte BLAKE2b HMAC over raw payload bytes using a provided raw key buffer.
    static void ComputeHMAC(
        const uint8_t* KeyData,
        size_t KeyLen,
        const uint8_t* PayloadData,
        size_t PayloadLen,
        uint8_t OutputHMAC[32]
    )
    {
        X25519Impl::Blake2bKeyedHash(KeyData, KeyLen, PayloadData, PayloadLen, OutputHMAC);
    }

    // Verifies a 32-byte HMAC against raw payload bytes.
    static bool VerifyHMAC(
        const uint8_t* KeyData,
        size_t KeyLen,
        const uint8_t* PayloadData,
        size_t PayloadLen,
        const uint8_t InputHMAC[32]
    )
    {
        uint8_t ExpectedHMAC[32];
        ComputeHMAC(KeyData, KeyLen, PayloadData, PayloadLen, ExpectedHMAC);
        return crypto_verify32(ExpectedHMAC, InputHMAC) == 0;
    }

    // Generates a 64 bit token derived from a 32 byte HMAC over raw payload bytes.
    static uint64_t GenerateToken64(
        const uint8_t* KeyData, size_t KeyLen, const uint8_t* PayloadData, size_t PayloadLen
    )
    {
        uint8_t HMAC_[32];
        ComputeHMAC(KeyData, KeyLen, PayloadData, PayloadLen, HMAC_);

        uint64_t Token = 0;
        std::memcpy(&Token, HMAC_, sizeof(uint64_t));
        return Token;
    }

    // Verifies a 64 bit token against raw payload bytes.
    static bool VerifyToken64(
        const uint8_t* KeyData,
        size_t KeyLen,
        const uint8_t* PayloadData,
        size_t PayloadLen,
        uint64_t ExpectedToken
    )
    {
        uint64_t Token = GenerateToken64(KeyData, KeyLen, PayloadData, PayloadLen);
        return Token != 0 && Token == ExpectedToken;
    }

    // Derives a 32 byte token from an active session's shared secret using custom salt data.
    bool DeriveSessionKey(
        const UniqueKeyType& UniqueKey, const uint8_t* SaltData, size_t SaltLen, uint8_t KeyOut[32]
    ) const
    {
        auto iter = ActiveSessions.find(UniqueKey);
        if (iter == ActiveSessions.end() || !iter->second.SecretDerived) {
            return false;
        }
        ComputeHMAC(iter->second.SharedSecret.data(), 32, SaltData, SaltLen, KeyOut);
        return true;
    }

    // Computes a 64 bit token using an active session's shared secret.
    uint64_t GenerateSessionToken64(
        const UniqueKeyType& uniqueKey, const uint8_t* payloadData, size_t payloadLen
    ) const
    {
        auto iter = ActiveSessions.find(uniqueKey);
        if (iter == ActiveSessions.end() || !iter->second.SecretDerived ||
            !iter->second.Authenticated) {
            return 0;
        }
        return GenerateToken64(iter->second.SharedSecret.data(), 32, payloadData, payloadLen);
    }

    // Verifies a 64 bit token using an active session's shared secret.
    bool VerifySessionToken64(
        const UniqueKeyType& uniqueKey,
        const uint8_t* payloadData,
        size_t payloadLen,
        uint64_t expectedToken
    ) const
    {
        uint64_t Token = GenerateSessionToken64(uniqueKey, payloadData, payloadLen);
        return Token != 0 && Token == expectedToken;
    }

    void ClearSession(const UniqueKeyType& uniqueKey)
    {
        auto iter = ActiveSessions.find(uniqueKey);
        if (iter != ActiveSessions.end()) {
            iter->second.SecureWipe();
            ActiveSessions.erase(iter);
        }
    }
};
