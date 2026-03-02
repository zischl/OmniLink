#ifndef NETENCODER_H
#define NETENCODER_H

#pragma once
#include <stdint.h>
#include <vector>
#include <string>
#include <string_view>

#include "OmniLogger.h"

class ByteStream {
private:

	static void ReadU16(const uint8_t* Source, uint16_t& Dest);

	static void ReadU32(const uint8_t* Source, uint32_t& Dest);

	static void ReadU64(const uint8_t* Source, uint64_t& Dest);


public:
	uint32_t CurrentLength;
	unsigned char* Data;

	ByteStream(uint32_t DataLen, unsigned char* DataPtr) : CurrentLength(DataLen), Data(DataPtr) {}

	void WriteU16(uint16_t Value, std::vector<uint8_t>& Dest);

	void WriteU32(uint32_t Value, std::vector<uint8_t>& Dest);

	void WriteU64(uint64_t Value, std::vector<uint8_t>& Dest);


	bool WriteU16(uint16_t Value, uint8_t* Dest);

	bool WriteU32(uint32_t Value, uint8_t* Dest);

	bool WriteU64(uint64_t Value, uint8_t* Dest);


	bool WriteU16(uint16_t Value, unsigned char* Dest);

	bool WriteU32(uint32_t Value, unsigned char* Dest);

	bool WriteU64(uint64_t Value, unsigned char* Dest);


	void WriteString(const std::string_view& String, std::vector<uint8_t>& Dest);

	bool WriteString(std::string_view& String, uint8_t* Dest, uint32_t WriteLen);

	void SafeWriteString(const std::string_view& String, std::vector<uint8_t>& Dest);

	inline void ReadU64(uint64_t& Dest) {
		ReadU64(Data, Dest);
		Data += 8;
		CurrentLength -= 8;
	}

	inline void ReadU32(uint32_t& Dest) {
		ReadU32(Data, Dest);
		Data += 4;
		CurrentLength -= 4;
	}

	inline void ReadU16(uint16_t& Dest) {
		ReadU16(Data, Dest);
		Data += 2;
		CurrentLength -= 2;
	}

	inline bool SafeReadU64(uint64_t& Dest) {
		if (CurrentLength < 8)
		{
		Logger::log("ByteStream U64 : Remaining bytestream length insufficient for decoding.");
		return false;
		};

		ReadU64(Data, Dest);
		Data += 8;
		CurrentLength -= 8;
		return true;
	}

	inline bool SafeReadU32(uint32_t& Dest) {
		if (CurrentLength < 4)
		{
			Logger::log("ByteStream U32 : Remaining bytestream length insufficient for decoding.");
			return false;
		};

		ReadU32(Data, Dest);
		Data += 4;
		CurrentLength -= 4;
		return true;
	}

	inline bool SafeReadU16(uint16_t& Dest) {
		if (CurrentLength < 2)
		{
			Logger::log("ByteStream U16 : Remaining bytestream length insufficient for decoding.");
			return false;
		};

		ReadU16(Data, Dest);
		Data += 2;
		CurrentLength -= 2;
		return true;
	}

	inline void ReadString(std::string& Dest) {
		uint32_t StringLength;
		ReadU32(StringLength);

		Dest.assign(reinterpret_cast<char*>(Data), StringLength);
		Data += StringLength;
		CurrentLength -= StringLength;
		
	}

	inline void SafeReadString(std::string& Dest) {
		uint32_t StringLength;
		if (!SafeReadU32(StringLength)) {
			return;
		}
		if (CurrentLength < StringLength)
		{
			Logger::log("ByteStream : String length exceeds maximum allowed length or remaining data length");
			return;
		};

		Dest.assign(reinterpret_cast<char*>(Data), StringLength);
		Data += StringLength;
		CurrentLength -= StringLength;

	}

	inline void ReadString(std::vector<char>& Dest) {
		uint32_t StringLength;
		ReadU32(StringLength);
		Dest.insert(Dest.end(), reinterpret_cast<char*>(Data), reinterpret_cast<char*>(Data) + StringLength);
		Data += StringLength;
		CurrentLength -= StringLength;
	}

	inline void SafeReadString(std::vector<char>& Dest) {
		uint32_t StringLength;
		if (!SafeReadU32(StringLength)) {
			return;
		}
		
		if (CurrentLength < StringLength)
		{
			Logger::log("ByteStream : String length exceeds maximum allowed length or remaining data length");
			return;
		};
		
		Dest.insert(Dest.end(), reinterpret_cast<char*>(Data), reinterpret_cast<char*>(Data) + StringLength);
		Data += StringLength;
		CurrentLength -= StringLength;
	}

	inline void ReadString(char* Dest) {
		uint32_t StringLength;
		ReadU32(StringLength);

		std::memcpy(Dest, Data, StringLength);
		Data += StringLength;
		CurrentLength -= StringLength;
	}

	inline void ReadString(char* Dest, uint32_t MaxLen) {		
		uint32_t StringLength;
		ReadU32(StringLength);

		if (StringLength > MaxLen || CurrentLength < StringLength)
		{
			Logger::log("ByteStream : String length exceeds maximum allowed length or remaining data length");
			return;
		};

		std::memcpy(Dest, Data, StringLength);
		Data += StringLength;
		CurrentLength -= StringLength;
	}

};



#endif