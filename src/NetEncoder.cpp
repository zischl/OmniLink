#include "NetEncoder.h"
	

void ByteStream::WriteU16(uint16_t Value, std::vector<uint8_t>& Dest)
{
	Dest.push_back(static_cast<uint8_t>((Value >> 8) & 0xFF));
	Dest.push_back(static_cast<uint8_t>(Value & 0xFF));

	CurrentLength += 2;

}

void ByteStream::WriteU32(uint32_t Value, std::vector<uint8_t>& Dest)
{
	Dest.push_back(static_cast<uint8_t>((Value >> 24) & 0xFF));
	Dest.push_back(static_cast<uint8_t>((Value >> 16) & 0xFF));
	Dest.push_back(static_cast<uint8_t>((Value >> 8) & 0xFF));
	Dest.push_back(static_cast<uint8_t>(Value & 0xFF));

	CurrentLength += 4;	
}


void ByteStream::WriteU64(uint64_t Value, std::vector<uint8_t>& Dest)
{
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


bool ByteStream::WriteU16(uint16_t Value, uint8_t* Dest)
{
	Dest[0] = static_cast<uint8_t>((Value >> 8) & 0xFF);
	Dest[1] = static_cast<uint8_t>(Value & 0xFF);

	CurrentLength += 2;

	return true;
}

bool ByteStream::WriteU32(uint32_t Value, uint8_t* Dest)
{

	Dest[0] = static_cast<uint8_t>((Value >> 24) & 0xFF);
	Dest[1] = static_cast<uint8_t>((Value >> 16) & 0xFF);
	Dest[2] = static_cast<uint8_t>((Value >> 8) & 0xFF);
	Dest[3] = static_cast<uint8_t>(Value & 0xFF);

	CurrentLength += 4;

	return true;
}

bool ByteStream::WriteU64(uint64_t Value, uint8_t* Dest)
{
	
	Dest[0] = static_cast<uint8_t>((Value >> 56) & 0xFF);
	Dest[1] = static_cast<uint8_t>((Value >> 48) & 0xFF);
	Dest[2] = static_cast<uint8_t>((Value >> 40) & 0xFF);
	Dest[3] = static_cast<uint8_t>((Value >> 32) & 0xFF);
	Dest[4] = static_cast<uint8_t>((Value >> 24) & 0xFF);
	Dest[5] = static_cast<uint8_t>((Value >> 16) & 0xFF);
	Dest[6] = static_cast<uint8_t>((Value >> 8) & 0xFF);
	Dest[7] = static_cast<uint8_t>(Value & 0xFF);


	CurrentLength += 8;

	return true;
}


bool ByteStream::WriteU16(uint16_t Value, unsigned char* Dest)
{
	return WriteU16(Value, reinterpret_cast<uint8_t*>(Dest));
}

bool ByteStream::WriteU32(uint32_t Value, unsigned char* Dest)
{
	return WriteU32(Value, reinterpret_cast<uint8_t*>(Dest));
}

bool ByteStream::WriteU64(uint64_t Value, unsigned char* Dest)
{
	return WriteU64(Value, reinterpret_cast<uint8_t*>(Dest));
}



void ByteStream::ReadU16(const uint8_t* Source, uint16_t& Dest)
{
	Dest = (static_cast<uint16_t>(Source[0]) << 8) | static_cast<uint16_t>(Source[1]);

}

void ByteStream::ReadU32(const uint8_t* Source, uint32_t& Dest)
{
	Dest = (static_cast<uint32_t>(Source[0]) << 24) |
		(static_cast<uint32_t>(Source[1]) << 16) |
		(static_cast<uint32_t>(Source[2]) << 8) |
		static_cast<uint32_t>(Source[3]);

}

void ByteStream::ReadU64(const uint8_t* Source, uint64_t& Dest)
{
	Dest = (static_cast<uint64_t>(Source[0]) << 56) | (static_cast<uint64_t>(Source[1]) << 48) |
		(static_cast<uint64_t>(Source[2]) << 40) | (static_cast<uint64_t>(Source[3]) << 32) |
		(static_cast<uint64_t>(Source[4]) << 24) | (static_cast<uint64_t>(Source[5]) << 16) |
		(static_cast<uint64_t>(Source[6]) << 8) | static_cast<uint64_t>(Source[7]);
}

void ByteStream::WriteString(const std::string_view& String, std::vector<uint8_t>& Dest)
{
	uint32_t StringLength = String.size();
	WriteU32(StringLength, Dest);

	Dest.reserve(Dest.size() + StringLength + 4);
	Dest.insert(Dest.end(), String.begin(), String.end());
} 


void ByteStream::WriteString(const std::string_view& String, std::vector<uint8_t>& Dest)
{
	uint32_t StringLength = String.size();
	WriteU32(StringLength, Dest);

	Dest.reserve(Dest.size() + StringLength + 4);
	Dest.insert(Dest.end(), String.begin(), String.end());
}

void ByteStream::SafeWriteString(const std::string_view& String, std::vector<uint8_t>& Dest)
{
	if (String.size() > UINT32_MAX) {
		Logger::log("NetEncoder::SafeWriteString : String length exceeds maximum allowed length for encoding.");
		return;
	}
	uint32_t StringLength = String.size();
	
	WriteU32(StringLength, Dest);

	Dest.reserve(Dest.size() + StringLength + 4);
	Dest.insert(Dest.end(), String.begin(), String.end());
}

bool ByteStream::WriteString(std::string_view& String, uint8_t* Dest, uint32_t WriteLen)
{
	if (!Dest) return false;
	if (String.size() > UINT32_MAX) return false;

	WriteU32(WriteLen, Dest);
	Dest += 4;


	return true;
}

