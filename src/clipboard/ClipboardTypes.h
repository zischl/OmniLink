#pragma once

#include "ByteStream.h"

#include <cstdint>
#include <string>
#include <vector>

enum class ClipboardCategory : uint8_t { Text = 1, Image = 2, FileList = 3, RichData = 4 };

// LightGram is just messages under udp MTU
enum class ClipboardOp : uint8_t { LightGram = 0, Manifest = 1 };

struct ClipboardManifest
{
    uint32_t StreamID = 0;
    uint16_t ServerPort = 0;
    ClipboardCategory Category = ClipboardCategory::Text;
    std::string FormatMime;
    uint32_t WinFormatID = 0;
    uint64_t TotalSizeBytes = 0;
    uint32_t ItemCount = 1;
    std::string ItemName;

    static std::vector<uint8_t> Serialize(const ClipboardManifest& Manifest)
    {
        uint32_t MimeLen = static_cast<uint32_t>(Manifest.FormatMime.size());
        uint32_t NameLen = static_cast<uint32_t>(Manifest.ItemName.size());
        uint32_t TotalPayloadSize = 4 + 2 + 1 + 4 + MimeLen + 4 + 8 + 4 + 4 + NameLen;

        ByteVecStreamEx Writer{TotalPayloadSize};
        Writer.WriteU32Ex(Manifest.StreamID);
        Writer.WriteU16Ex(Manifest.ServerPort);
        Writer.WriteU8Ex(static_cast<uint8_t>(Manifest.Category));
        Writer.WriteU32Ex(MimeLen);
        if (MimeLen > 0) {
            Writer.WriteString(Manifest.FormatMime);
        }
        Writer.WriteU32Ex(Manifest.WinFormatID);
        Writer.WriteU64Ex(Manifest.TotalSizeBytes);
        Writer.WriteU32Ex(Manifest.ItemCount);
        Writer.WriteU32Ex(NameLen);
        if (NameLen > 0) {
            Writer.WriteString(Manifest.ItemName);
        }

        return Writer.Data;
    }

    static ClipboardManifest Deserialize(ByteStreamReader& Reader)
    {
        ClipboardManifest Manifest;
        Reader.ReadU32Ex(Manifest.StreamID);
        Reader.ReadU16Ex(Manifest.ServerPort);
        uint8_t CatVal = 0;
        Reader.ReadU8Ex(CatVal);
        Manifest.Category = static_cast<ClipboardCategory>(CatVal);

        uint32_t MimeLen = 0;
        Reader.ReadU32Ex(MimeLen);
        if (MimeLen > 0) {
            Reader.ReadString(Manifest.FormatMime, MimeLen);
        }

        Reader.ReadU32Ex(Manifest.WinFormatID);
        Reader.ReadU64Ex(Manifest.TotalSizeBytes);
        Reader.ReadU32Ex(Manifest.ItemCount);

        uint32_t NameLen = 0;
        Reader.ReadU32Ex(NameLen);
        if (NameLen > 0) {
            Reader.ReadString(Manifest.ItemName, NameLen);
        }

        return Manifest;
    }
};
