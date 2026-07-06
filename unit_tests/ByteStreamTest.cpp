#pragma once
#include "ByteStream.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

void ByteStreamTest()
{
    std::cout << "[RUN] ByteStreamTest\n";

    // Test 1 award goes to... ByteVecStreamEx + ByteStreamReader
    {
        ByteVecStreamEx writer(100);
        writer.WriteU8Ex(0xAA);
        writer.WriteU16Ex(0xBEEF);
        writer.WriteU32Ex(0xDEADBEEF);
        writer.WriteU64Ex(0x0123456789ABCDEFULL);
        writer.WriteString("Hello ByteStream");

        ByteStreamReader reader(writer.CurrentLength, writer.Data.data());

        uint8_t u8Val = 0;
        uint16_t u16Val = 0;
        uint32_t u32Val = 0;
        uint64_t u64Val = 0;
        std::string strVal;

        reader.ReadU8Ex(u8Val);
        reader.ReadU16Ex(u16Val);
        reader.ReadU32Ex(u32Val);
        reader.ReadU64Ex(u64Val);
        reader.ReadString(strVal, 16);

        assert(u8Val == 0xAA);
        assert(u16Val == 0xBEEF);
        assert(u32Val == 0xDEADBEEF);
        assert(u64Val == 0x0123456789ABCDEFULL);
        assert(strVal == "Hello ByteStream");
    }

    // Test 2... ByteStreamEx & Safe Reads altho I normally don't do safe :]
    {
        uint8_t buffer[64] = {};
        ByteStreamEx writer(buffer);

        bool w1 = writer.WriteU8Ex(0x01);
        bool w2 = writer.WriteU16Ex(0x0203);
        bool w3 = writer.WriteU32Ex(0x04050607);
        bool w4 = writer.WriteU64Ex(0x08090A0B0C0D0E0FULL);
        bool w5 = writer.WriteString("Safe");

        assert(w1 && w2 && w3 && w4 && w5);

        ByteStreamReader reader(writer.CurrentLength, buffer);
        uint8_t r8 = 0;
        uint16_t r16 = 0;
        uint32_t r32 = 0;
        uint64_t r64 = 0;
        std::string rstr;

        reader.ReadU8Ex(r8);
        assert(r8 == 0x01);

        bool s16 = reader.SafeReadU16(r16);
        assert(s16 && r16 == 0x0203);

        bool s32 = reader.SafeReadU32(r32);
        assert(s32 && r32 == 0x04050607);

        bool s64 = reader.SafeReadU64(r64);
        assert(s64 && r64 == 0x08090A0B0C0D0E0FULL);

        reader.SafeReadString(rstr, 4);
        assert(rstr == "Safe");

        // Should fail as remaining length is.. prolly 0
        uint16_t out16 = 0;
        bool sOutOfBounds = reader.SafeReadU16(out16);
        assert(!sOutOfBounds);
    }

    std::cout << "[PASS] ByteStreamTest\n";
}
