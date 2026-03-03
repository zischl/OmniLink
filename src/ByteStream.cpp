#include "ByteStream.h"
	



void ByteStreamReader::ReadU64Ex(uint64_t& Dest) {
    ByteStream::ReadU64(reinterpret_cast<uint8_t*>(Data), Dest);
    Data += 8;
    CurrentLength -= 8;
}

void ByteStreamReader::ReadU32Ex(uint32_t& Dest) {
    ByteStream::ReadU32(reinterpret_cast<uint8_t*>(Data), Dest);
    Data += 4;
    CurrentLength -= 4;
}

void ByteStreamReader::ReadU16Ex(uint16_t& Dest) {
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


bool ByteStreamReader::SafeReadU64(uint64_t& Dest) {
    if (CurrentLength < 8) {
        Logger::log("ByteStreamReader U64 : Remaining bytestream length insufficient for decoding.");
        return false;
    }
    ReadU64Ex(Dest);
    return true;
}

bool ByteStreamReader::SafeReadU32(uint32_t& Dest) {
    if (CurrentLength < 4) {
        Logger::log("ByteStreamReader U32 : Remaining bytestream length insufficient for decoding.");
        return false;
    }
    ReadU32Ex(Dest);
    return true;
}

bool ByteStreamReader::SafeReadU16(uint16_t& Dest) {
    if (CurrentLength < 2) {
        Logger::log("ByteStreamReader U16 : Remaining bytestream length insufficient for decoding.");
        return false;
    }
    ReadU16Ex(Dest);
    return true;
}

void ByteStreamReader::ReadString(std::string& Dest) {
    uint32_t StringLength;
    ReadU32Ex(StringLength);

    Dest.assign(reinterpret_cast<char*>(Data), StringLength);
    Data += StringLength;
    CurrentLength -= StringLength;
}

void ByteStreamReader::SafeReadString(std::string& Dest) {
    uint32_t StringLength;
    if (!SafeReadU32(StringLength)) return;

    if (CurrentLength < StringLength) {
        Logger::log("ByteStreamReader : String length exceeds maximum allowed length or remaining data length");
        return;
    }

    Dest.assign(reinterpret_cast<char*>(Data), StringLength);
    Data += StringLength;
    CurrentLength -= StringLength;
}

void ByteStreamReader::ReadString(std::vector<char>& Dest) {
    uint32_t StringLength;
    ReadU32Ex(StringLength);

    Dest.insert(Dest.end(),
        reinterpret_cast<char*>(Data),
        reinterpret_cast<char*>(Data) + StringLength);

    Data += StringLength;
    CurrentLength -= StringLength;
}

void ByteStreamReader::SafeReadString(std::vector<char>& Dest) {
    uint32_t StringLength;
    if (!SafeReadU32(StringLength)) return;

    if (CurrentLength < StringLength) {
        Logger::log("ByteStreamReader : String length exceeds maximum allowed length or remaining data length");
        return;
    }

    Dest.insert(Dest.end(),
        reinterpret_cast<char*>(Data),
        reinterpret_cast<char*>(Data) + StringLength);

    Data += StringLength;
    CurrentLength -= StringLength;
}

void ByteStreamReader::ReadString(char* Dest) {
    uint32_t StringLength;
    ReadU32Ex(StringLength);

    std::memcpy(Dest, Data, StringLength);
    Data += StringLength;
    CurrentLength -= StringLength;
}

void ByteStreamReader::ReadString(char* Dest, uint32_t MaxLen) {
    uint32_t StringLength;
    ReadU32Ex(StringLength);

    if (StringLength > MaxLen || CurrentLength < StringLength) {
        Logger::log("ByteStreamReader : String length exceeds maximum allowed length or remaining data length");
        return;
    }

    std::memcpy(Dest, Data, StringLength);
    Data += StringLength;
    CurrentLength -= StringLength;
}




void ByteStreamWriter::WriteU8Ex(uint8_t Value, std::vector<uint8_t>& Dest)
{
    Dest.push_back(Value);
    CurrentLength += 1;
}


void ByteStreamWriter::WriteU16Ex(uint16_t Value, std::vector<uint8_t>& Dest) 
{
    Dest.push_back(static_cast<uint8_t>((Value >> 8) & 0xFF));
    Dest.push_back(static_cast<uint8_t>(Value & 0xFF));
    CurrentLength += 2;
}

void ByteStreamWriter::WriteU32Ex(uint32_t Value, std::vector<uint8_t>& Dest) 
{
    Dest.push_back(static_cast<uint8_t>((Value >> 24) & 0xFF));
    Dest.push_back(static_cast<uint8_t>((Value >> 16) & 0xFF));
    Dest.push_back(static_cast<uint8_t>((Value >> 8) & 0xFF));
    Dest.push_back(static_cast<uint8_t>(Value & 0xFF));
    CurrentLength += 4;
}

void ByteStreamWriter::WriteU64Ex(uint64_t Value, std::vector<uint8_t>& Dest) {
    Dest.push_back(static_cast<uint8_t>((Value >> 56) & 0xFF));
    Dest.push_back(static_cast<uint8_t>((Value >> 48) & 0xFF));
    Dest.push_back(static_cast<uint8_t>((Value >> 40) & 0xFF));
    Dest.push_back(static_cast<uint8_t>((Value >> 32) & 0xFF));
    Dest.push_back(static_cast<uint8_t>((Value >> 24) & 0xFF));
    Dest.push_back(static_cast<uint8_t>((Value >> 16) & 0xFF));
    Dest.push_back(static_cast<uint8_t>((Value >> 8) & 0xFF));
    Dest.push_back(static_cast<uint8_t>(Value & 0xFF));
    CurrentLength += 8;
}



bool ByteStreamWriter::WriteU8Ex(uint8_t Value, uint8_t* Dest)
{
    if (!Dest) return false;
    Dest[CurrentLength] = Value;
    CurrentLength += 1;
    return true;
}

bool ByteStreamWriter::WriteU16Ex(uint16_t Value, uint8_t* Dest)
{
    if (!Dest) return false;
    ByteStream::WriteU16(Value, Dest);
    CurrentLength += 2;
    return true;
}

bool ByteStreamWriter::WriteU32Ex(uint32_t Value, uint8_t* Dest)
{
    if (!Dest) return false;
    ByteStream::WriteU32(Value, Dest);
    CurrentLength += 4;
    return true;
}

bool ByteStreamWriter::WriteU64Ex(uint64_t Value, uint8_t* Dest)
{
    if (!Dest) return false;
    ByteStream::WriteU64(Value, Dest);
    CurrentLength += 8;
    return true;
}

//bool ByteStreamWriter::WriteU16Ex(uint16_t Value, unsigned char* Dest)
//{
//    return WriteU16Ex(Value, reinterpret_cast<uint8_t*>(Dest));
//}
//
//bool ByteStreamWriter::WriteU32Ex(uint32_t Value, unsigned char* Dest)
//{
//    return WriteU32Ex(Value, reinterpret_cast<uint8_t*>(Dest));
//}
//
//bool ByteStreamWriter::WriteU64Ex(uint64_t Value, unsigned char* Dest)
//{
//    return WriteU64Ex(Value, reinterpret_cast<uint8_t*>(Dest));
//}

void ByteStreamWriter::WriteString(const std::string_view& String, std::vector<uint8_t>& Dest) 
{

    if (String.size() > UINT32_MAX) {
        Logger::log("ByteStreamWriter::WriteString : String length exceeds maximum allowed length for encoding.");
        return;
    }

    uint32_t StringLength = static_cast<uint32_t>(String.size());
    WriteU32Ex(StringLength, Dest);

    Dest.reserve(Dest.size() + StringLength);
    Dest.insert(Dest.end(), String.begin(), String.end());

    CurrentLength += StringLength;
}

bool ByteStreamWriter::WriteString(const std::string_view& String, uint8_t* Dest)
{
    if (!Dest) return false;
    if (String.size() > UINT32_MAX) return false;

    uint32_t StringLength = static_cast<uint32_t>(String.size());
    WriteU32(StringLength, Dest);

    ByteStream::WriteU32(StringLength, Dest);
    std::memcpy(Dest + 4, String.data(), StringLength);
    CurrentLength += StringLength;
    return true;
}

void ByteStreamWriter::SafeWriteString(const std::string_view& String, std::vector<uint8_t>& Dest) 
{
    WriteString(String, Dest);
}


bool ByteStreamWriter::SafeWriteString(const std::string_view& String, uint8_t* Dest, uint32_t MaxLen)
{
    if (!Dest) return false;
    if (String.size() > UINT32_MAX) return false;

    uint32_t StringLength = static_cast<uint32_t>(String.size());
    WriteU32(StringLength, Dest);

    if (StringLength > MaxLen) {
        Logger::log("ByteStreamWriter::SafeWriteString : String length exceeds maximum allowed length of the buffer for encoding.");
    }

    ByteStream::WriteU32(StringLength, Dest);
    std::memcpy(Dest + 4, String.data(), StringLength);
    CurrentLength += StringLength;
    return true;
}
