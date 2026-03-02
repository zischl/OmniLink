#ifndef NETENCODER_H
#define NETENCODER_H

#pragma once
#include <stdint.h>
#include <vector>
#include <string>
#include <string_view>

#include "OmniLogger.h"

class ByteStream {
protected:

    static inline void ReadU16(const uint8_t* Source, uint16_t& Dest) {
        Dest = (static_cast<uint16_t>(Source[0]) << 8) |
            static_cast<uint16_t>(Source[1]);
    }

    static inline void ReadU32(const uint8_t* Source, uint32_t& Dest) {
        Dest = (static_cast<uint32_t>(Source[0]) << 24) |
            (static_cast<uint32_t>(Source[1]) << 16) |
            (static_cast<uint32_t>(Source[2]) << 8) |
            static_cast<uint32_t>(Source[3]);
    }

    static inline void ReadU64(const uint8_t* Source, uint64_t& Dest) {
        Dest = (static_cast<uint64_t>(Source[0]) << 56) |
            (static_cast<uint64_t>(Source[1]) << 48) |
            (static_cast<uint64_t>(Source[2]) << 40) |
            (static_cast<uint64_t>(Source[3]) << 32) |
            (static_cast<uint64_t>(Source[4]) << 24) |
            (static_cast<uint64_t>(Source[5]) << 16) |
            (static_cast<uint64_t>(Source[6]) << 8) |
            static_cast<uint64_t>(Source[7]);
    }


    static inline void WriteU16(uint16_t Value, uint8_t* Dest) {
        Dest[0] = static_cast<uint8_t>((Value >> 8) & 0xFF);
        Dest[1] = static_cast<uint8_t>(Value & 0xFF);
    }

    static inline void WriteU32(uint32_t Value, uint8_t* Dest) {
        Dest[0] = static_cast<uint8_t>((Value >> 24) & 0xFF);
        Dest[1] = static_cast<uint8_t>((Value >> 16) & 0xFF);
        Dest[2] = static_cast<uint8_t>((Value >> 8) & 0xFF);
        Dest[3] = static_cast<uint8_t>(Value & 0xFF);
    }

    static inline void WriteU64(uint64_t Value, uint8_t* Dest) {
        Dest[0] = static_cast<uint8_t>((Value >> 56) & 0xFF);
        Dest[1] = static_cast<uint8_t>((Value >> 48) & 0xFF);
        Dest[2] = static_cast<uint8_t>((Value >> 40) & 0xFF);
        Dest[3] = static_cast<uint8_t>((Value >> 32) & 0xFF);
        Dest[4] = static_cast<uint8_t>((Value >> 24) & 0xFF);
        Dest[5] = static_cast<uint8_t>((Value >> 16) & 0xFF);
        Dest[6] = static_cast<uint8_t>((Value >> 8) & 0xFF);
        Dest[7] = static_cast<uint8_t>(Value & 0xFF);
    }
};




class ByteStreamReader : private ByteStream {
public:
    uint32_t CurrentLength;
    unsigned char* Data;

    ByteStreamReader(uint32_t DataLen, unsigned char* DataPtr)
        : CurrentLength(DataLen), Data(DataPtr) {
    }


    void ReadU64Ex(uint64_t& Dest);

    void ReadU32Ex(uint32_t& Dest);

    void ReadU16Ex(uint16_t& Dest);


    bool SafeReadU64(uint64_t& Dest);

    bool SafeReadU32(uint32_t& Dest);

    bool SafeReadU16(uint16_t& Dest);


    void ReadString(std::string& Dest);

    void SafeReadString(std::string& Dest);

    void ReadString(std::vector<char>& Dest);

    void SafeReadString(std::vector<char>& Dest);

    void ReadString(char* Dest);

    void ReadString(char* Dest, uint32_t MaxLen);
};




class ByteStreamWriter : private ByteStream {
public:
    uint32_t CurrentLength;

    ByteStreamWriter() : CurrentLength(0) {}
    explicit ByteStreamWriter(uint32_t StartingLen) : CurrentLength(StartingLen) {}


    void WriteU16Ex(uint16_t Value, std::vector<uint8_t>& Dest);

    void WriteU32Ex(uint32_t Value, std::vector<uint8_t>& Dest);

    void WriteU64Ex(uint64_t Value, std::vector<uint8_t>& Dest);


    bool WriteU16Ex(uint16_t Value, uint8_t* Dest);

    bool WriteU32Ex(uint32_t Value, uint8_t* Dest);

    bool WriteU64Ex(uint64_t Value, uint8_t* Dest);


    /*bool WriteU16Ex(uint16_t Value, unsigned char* Dest);

    bool WriteU32Ex(uint32_t Value, unsigned char* Dest);

    bool WriteU64Ex(uint64_t Value, unsigned char* Dest);*/


    void WriteString(const std::string_view& String, std::vector<uint8_t>& Dest);

    bool WriteString(const std::string_view& String, uint8_t* Dest);

    void SafeWriteString(const std::string_view& String, std::vector<uint8_t>& Dest);

    bool SafeWriteString(const std::string_view& String, uint8_t* Dest, uint32_t MaxLen);


};



#endif