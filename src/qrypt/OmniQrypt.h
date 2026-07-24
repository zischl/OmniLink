#pragma once

#include "OmniEnums.h"
#include "QryptEngine.h"

#include <fstream>
#include <string>

// Extends the QryptEngine base class with...
// DeviceMap as unique key and pairing/action token management.
class OmniQrypt : public QryptEngine<DeviceMap>
{
  protected:
    std::unordered_map<DeviceMap, std::array<uint8_t, 32>> TrustedPairingTokens;
    std::string PairingSalt = "Omni-Liss-v1";

  public:
    OmniQrypt() = default;
    explicit OmniQrypt(std::string PairingSalt) : PairingSalt(std::move(PairingSalt)) {}

    // Generates a persistent pairing token for a device from the active ECDH session.
    std::array<uint8_t, 32> GeneratePairingToken(DeviceMap DeviceID)
    {
        std::array<uint8_t, 32> PairingToken{};
        if (!DeriveSessionKey(
                DeviceID,
                reinterpret_cast<const uint8_t*>(PairingSalt.data()),
                PairingSalt.size(),
                PairingToken.data()
            )) {
            return {};
        }
        return PairingToken;
    }

    void StorePairingToken(DeviceMap DeviceID, const std::array<uint8_t, 32>& Token)
    {
        TrustedPairingTokens[DeviceID] = Token;
        SavePairingTokensToFile();
    }

    // True if token exists for the device id.
    bool PairingTokenState(DeviceMap DeviceID) const
    {
        return TrustedPairingTokens.find(DeviceID) != TrustedPairingTokens.end();
    }

    bool SavePairingTokensToFile(const char* FilePath = "liss.dat")
    {
        std::ofstream File(FilePath, std::ios::binary);
        if (!File.is_open())
            return false;
        uint32_t Count = static_cast<uint32_t>(TrustedPairingTokens.size());
        File.write(reinterpret_cast<const char*>(&Count), sizeof(Count));
        for (const auto& [DevIdx, Token] : TrustedPairingTokens) {
            uint8_t DeviceID = static_cast<uint8_t>(DevIdx);
            File.write(reinterpret_cast<const char*>(&DeviceID), sizeof(DeviceID));
            File.write(reinterpret_cast<const char*>(Token.data()), 32);
        }
        return true;
    }

    bool LoadPairingTokensFromFile(const char* FilePath = "liss.dat")
    {
        std::ifstream File(FilePath, std::ios::binary);
        if (!File.is_open())
            return false;
        uint32_t Count = 0;
        File.read(reinterpret_cast<char*>(&Count), sizeof(Count));
        for (uint32_t i = 0; i < Count; ++i) {
            uint8_t DevIdx = 0;
            File.read(reinterpret_cast<char*>(&DevIdx), sizeof(DevIdx));

            std::array<uint8_t, 32> Token{};
            File.read(reinterpret_cast<char*>(Token.data()), 32);
            if (!File)
                return false;
            TrustedPairingTokens[static_cast<DeviceMap>(DevIdx)] = Token;
        }
        return true;
    }

    // Creates an OmniLink 9 byte payload 64 bit action token.
    uint64_t CreateActionToken(DeviceMap DeviceID, uint8_t ActionType, uint64_t Context) const
    {
        uint8_t Payload[9];
        Payload[0] = ActionType;
        std::memcpy(Payload + 1, &Context, 8);

        return GenerateSessionToken64(DeviceID, Payload, sizeof(Payload));
    }

    // Verifies that.. ^^^  action token.
    bool VerifyActionToken(
        DeviceMap DeviceID, uint8_t ActionType, uint64_t Context, uint64_t Token
    ) const
    {
        uint8_t Payload[9];
        Payload[0] = ActionType;
        std::memcpy(Payload + 1, &Context, 8);

        return VerifySessionToken64(DeviceID, Payload, sizeof(Payload), Token);
    }
};
