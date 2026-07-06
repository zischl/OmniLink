#include "ByteStream.h"

void ByteStreamReader::ReadU64Ex(uint64_t& Dest)
{
    ByteStream::ReadU64(reinterpret_cast<uint8_t*>(Data), Dest);
    Data += 8;
    CurrentLength -= 8;
}

void ByteStreamReader::ReadU32Ex(uint32_t& Dest)
{
    ByteStream::ReadU32(reinterpret_cast<uint8_t*>(Data), Dest);
    Data += 4;
    CurrentLength -= 4;
}

void ByteStreamReader::ReadU16Ex(uint16_t& Dest)
{
    ByteStream::ReadU16(reinterpret_cast<uint8_t*>(Data), Dest);
    Data += 2;
    CurrentLength -= 2;
}

void ByteStreamReader::ReadU8Ex(uint8_t& Dest)
{
    ByteStream::ReadU8(reinterpret_cast<uint8_t*>(Data), Dest);
    Data += 1;
    CurrentLength -= 1;
}

bool ByteStreamReader::SafeReadU64(uint64_t& Dest)
{
    if (CurrentLength < 8) {
        Logger::log(
            "ByteStreamReader U64 : Remaining bytestream length insufficient for decoding."
        );
        return false;
    }
    ReadU64Ex(Dest);
    return true;
}

bool ByteStreamReader::SafeReadU32(uint32_t& Dest)
{
    if (CurrentLength < 4) {
        Logger::log(
            "ByteStreamReader U32 : Remaining bytestream length insufficient for decoding."
        );
        return false;
    }
    ReadU32Ex(Dest);
    return true;
}

bool ByteStreamReader::SafeReadU16(uint16_t& Dest)
{
    if (CurrentLength < 2) {
        Logger::log(
            "ByteStreamReader U16 : Remaining bytestream length insufficient for decoding."
        );
        return false;
    }
    ReadU16Ex(Dest);
    return true;
}

void ByteStreamReader::ReadString(std::string& Dest, uint32_t Len)
{
    Dest.assign(reinterpret_cast<char*>(Data), Len);
    Data += Len;
    CurrentLength -= Len;
}

bool ByteStreamReader::SafeReadString(std::string& Dest, uint32_t Len)
{
    if (CurrentLength < Len) {
        Logger::log("ByteStreamReader : String length exceeds remaining data length");
        return false;
    }
    Dest.assign(reinterpret_cast<char*>(Data), Len);
    Data += Len;
    CurrentLength -= Len;
    return true;
}

void ByteStreamReader::ReadString(std::vector<char>& Dest, uint32_t Len)
{
    Dest.insert(Dest.end(), reinterpret_cast<char*>(Data), reinterpret_cast<char*>(Data) + Len);
    Data += Len;
    CurrentLength -= Len;
}

bool ByteStreamReader::SafeReadString(std::vector<char>& Dest, uint32_t Len)
{
    if (CurrentLength < Len) {
        Logger::log("ByteStreamReader : String length exceeds remaining data length");
        return false;
    }
    Dest.insert(Dest.end(), reinterpret_cast<char*>(Data), reinterpret_cast<char*>(Data) + Len);
    Data += Len;
    CurrentLength -= Len;
    return true;
}

bool ByteStreamReader::ReadString(char* Dest, uint32_t Len, uint32_t MaxLen)
{
    if (Len >= MaxLen || CurrentLength < Len) {
        Logger::log(
            "ByteStreamReader : String length exceeds buffer capacity or remaining data length"
        );
        uint32_t AdvanceLength = (std::min)(Len, CurrentLength);
        Data += AdvanceLength;
        CurrentLength -= AdvanceLength;
        if (MaxLen > 0)
            Dest[0] = '\0';
        return false;
    }
    std::memcpy(Dest, Data, Len);
    Dest[Len] = '\0';
    Data += Len;
    CurrentLength -= Len;
    return true;
}

bool ByteStreamReader::ReadString(char8_t* Dest, uint32_t Len, uint32_t MaxLen)
{
    if (Len >= MaxLen || CurrentLength < Len) {
        Logger::log(
            "ByteStreamReader : String length exceeds buffer capacity or remaining data length"
        );
        uint32_t AdvanceLength = (std::min)(Len, CurrentLength);
        Data += AdvanceLength;
        CurrentLength -= AdvanceLength;
        if (MaxLen > 0)
            Dest[0] = u8'\0';
        return false;
    }
    std::memcpy(Dest, Data, Len);
    Dest[Len] = u8'\0';
    Data += Len;
    CurrentLength -= Len;
    return true;
}

void ByteStreamReader::ReadBytes(uint8_t* Dest, uint32_t Len)
{
    std::memcpy(Dest, Data, Len);

    Data += Len;
    CurrentLength -= Len;
}

bool ByteStreamReader::SafeReadBytes(uint8_t* Dest, uint32_t Len)
{
    if (CurrentLength < Len)
        return false;

    std::memcpy(Dest, Data, Len);

    Data += Len;
    CurrentLength -= Len;

    return true;
}

bool ByteStreamEx::WriteU8Ex(uint8_t Value)
{
    if (!Data)
        return false;
    Data[CurrentLength] = Value;
    CurrentLength += 1;
    return true;
}

bool ByteStreamEx::WriteU16Ex(uint16_t Value)
{
    if (!Data)
        return false;
    ByteStream::WriteU16(Value, &Data[CurrentLength]);
    CurrentLength += 2;
    return true;
}

bool ByteStreamEx::WriteU32Ex(uint32_t Value)
{
    if (!Data)
        return false;
    ByteStream::WriteU32(Value, &Data[CurrentLength]);
    CurrentLength += 4;
    return true;
}

bool ByteStreamEx::WriteU64Ex(uint64_t Value)
{
    if (!Data)
        return false;
    ByteStream::WriteU64(Value, &Data[CurrentLength]);
    CurrentLength += 8;
    return true;
}

bool ByteStreamEx::WriteString(const std::string_view& String)
{
    if (!Data)
        return false;
    if (String.size() > UINT32_MAX)
        return false;

    uint32_t StringLength = static_cast<uint32_t>(String.size());
    std::memcpy(Data + CurrentLength, String.data(), StringLength);
    CurrentLength += StringLength;
    return true;
}

bool ByteStreamEx::SafeWriteString(const std::string_view& String, uint32_t MaxLen)
{
    if (!Data)
        return false;
    if (String.size() > UINT32_MAX)
        return false;

    uint32_t StringLength = static_cast<uint32_t>(String.size());
    if (StringLength > MaxLen) {
        Logger::log(
            "ByteStreamEx::SafeWriteString : String length exceeds maximum allowed "
            "length of the buffer for encoding."
        );
        return false;
    }

    std::memcpy(Data + CurrentLength, String.data(), StringLength);
    CurrentLength += StringLength;
    return true;
}

// bool ByteStreamWriter::WriteU16Ex(uint16_t Value, unsigned char* Dest)
//{
//     return WriteU16Ex(Value, reinterpret_cast<uint8_t*>(Dest));
// }
//
// bool ByteStreamWriter::WriteU32Ex(uint32_t Value, unsigned char* Dest)
//{
//     return WriteU32Ex(Value, reinterpret_cast<uint8_t*>(Dest));
// }
//
// bool ByteStreamWriter::WriteU64Ex(uint64_t Value, unsigned char* Dest)
//{
//     return WriteU64Ex(Value, reinterpret_cast<uint8_t*>(Dest));
// }

void ByteVecStreamEx::WriteU8Ex(uint8_t Value)
{
    ByteStream::WriteU8Ex(Value, Data);
    CurrentLength += 1;
}

void ByteVecStreamEx::WriteU16Ex(uint16_t Value)
{
    ByteStream::WriteU16Ex(Value, Data);
    CurrentLength += 2;
}

void ByteVecStreamEx::WriteU32Ex(uint32_t Value)
{
    ByteStream::WriteU32Ex(Value, Data);
    CurrentLength += 4;
}

void ByteVecStreamEx::WriteU64Ex(uint64_t Value)
{
    ByteStream::WriteU64Ex(Value, Data);
    CurrentLength += 8;
}

void ByteVecStreamEx::WriteString(const std::string_view& String)
{
    if (String.size() > UINT32_MAX) {
        Logger::log(
            "ByteVecStreamEx::WriteString : String length exceeds maximum allowed length "
            "for encoding."
        );
        return;
    }

    uint32_t StringLength = static_cast<uint32_t>(String.size());
    Data.reserve(Data.size() + StringLength);
    Data.insert(Data.end(), String.begin(), String.end());
    CurrentLength += StringLength;
}

void ByteVecStreamEx::WriteU8String(const std::u8string_view& String)
{
    if (String.size() > UINT32_MAX) {
        Logger::log(
            "ByteVecStreamEx::WriteU8String : String length exceeds maximum allowed length "
        );
        return;
    }

    uint32_t StringLength = static_cast<uint32_t>(String.size());
    Data.reserve(Data.size() + StringLength);
    const uint8_t* StartPtr = reinterpret_cast<const uint8_t*>(String.data());
    Data.insert(Data.end(), StartPtr, StartPtr + StringLength);
    CurrentLength += StringLength;
}

void ByteVecStreamEx::SafeWriteString(const std::string_view& String, const uint32_t MaxLen)
{
    if (String.size() > UINT32_MAX) {
        Logger::log(
            "ByteVecStreamEx::SafeWriteString : String length exceeds maximum allowed length "
        );
        return;
    }

    uint32_t StringLength = static_cast<uint32_t>(String.size());
    if (StringLength > MaxLen)
        return;

    Data.reserve(Data.size() + StringLength);
    Data.insert(Data.end(), String.begin(), String.end());
    CurrentLength += StringLength;
}

void ByteVecStreamEx::WriteBytes(const uint8_t* Src, uint32_t Len)
{
    const uint32_t OldSize = static_cast<uint32_t>(Data.size());

    Data.resize(OldSize + Len);

    std::memcpy(Data.data() + OldSize, Src, Len);

    CurrentLength += Len;
}
