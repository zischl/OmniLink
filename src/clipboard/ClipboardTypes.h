#pragma once

#include "ByteStream.h"
#include "OmniEnums.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

enum class ClipboardCategory : uint8_t { Text = 1, Image = 2, FileList = 3, RichData = 4 };

// LightGram is just messages under udp MTU
enum class ClipboardOp : uint8_t { LightGram = 0, Manifest = 1 };

enum ClipboardItemFlags : uint32_t {
    ItemFlag_None      = 0,
    ItemFlag_Directory = 1 << 0,
    ItemFlag_Virtual   = 1 << 1
};

struct StreamProgress
{
    std::atomic<uint64_t> BytesTransferred{0};
    std::atomic<uint64_t> TotalBytes{0};
    std::atomic<bool>     StreamState{false};
    std::atomic<bool>     Cancel{false};
};

struct ClipboardStreamEvent
{
    uint32_t                        StreamID         = 0;
    DeviceMap                       DeviceID         = DeviceMap::END;
    char                            ItemName[64]     = {};
    char                            CategoryName[16] = {};
    uint64_t                        TotalBytes       = 0;
    bool                            OutboundMode     = false;
    std::shared_ptr<StreamProgress> Progress         = nullptr;

    ClipboardStreamEvent() = default;

    ClipboardStreamEvent(
        uint32_t                        StreamID_,
        DeviceMap                       DeviceID_,
        std::string_view                ItemName_,
        std::string_view                Category_,
        uint64_t                        TotalBytes_,
        bool                            OutboundMode_,
        std::shared_ptr<StreamProgress> Progress_ = nullptr
    )
        : StreamID(StreamID_), DeviceID(DeviceID_), TotalBytes(TotalBytes_),
          OutboundMode(OutboundMode_), Progress(std::move(Progress_))
    {
        ItemName_.copy(ItemName, sizeof(ItemName) - 1);
        Category_.copy(CategoryName, sizeof(CategoryName) - 1);
    }
};

struct ClipboardFeatureContext
{
    std::function<void(const ClipboardStreamEvent&)> OnStreamEvent = nullptr;
};

struct ClipboardItemEntry
{
    std::string ItemName;
    std::string FormatMime;
    uint32_t    FormatID  = 0;
    uint64_t    SizeBytes = 0;
    uint32_t    Flags     = ItemFlag_None;
};

struct ClipboardManifest
{
    uint32_t                        StreamID   = 0;
    uint16_t                        ServerPort = 0;
    ClipboardCategory               Category   = ClipboardCategory::Text;
    std::string                     FormatMime;
    uint32_t                        WinFormatID    = 0;
    uint64_t                        TotalSizeBytes = 0;
    std::vector<ClipboardItemEntry> Items;

    static std::vector<uint8_t> Serialize(const ClipboardManifest& Manifest)
    {
        uint32_t MimeLen   = static_cast<uint32_t>(Manifest.FormatMime.size());
        uint32_t ItemCount = static_cast<uint32_t>(Manifest.Items.size());

        uint32_t TotalPayloadSize = 4 + 2 + 1 + 4 + MimeLen + 4 + 8 + 4;
        for (const auto& Item : Manifest.Items) {
            TotalPayloadSize += 4 + static_cast<uint32_t>(Item.ItemName.size()) + 4 +
                                static_cast<uint32_t>(Item.FormatMime.size()) + 4 + 8 + 4;
        }

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
        Writer.WriteU32Ex(ItemCount);

        for (const auto& Item : Manifest.Items) {
            uint32_t NameLen = static_cast<uint32_t>(Item.ItemName.size());
            Writer.WriteU32Ex(NameLen);
            if (NameLen > 0) {
                Writer.WriteString(Item.ItemName);
            }

            uint32_t ItemMimeLen = static_cast<uint32_t>(Item.FormatMime.size());
            Writer.WriteU32Ex(ItemMimeLen);
            if (ItemMimeLen > 0) {
                Writer.WriteString(Item.FormatMime);
            }

            Writer.WriteU32Ex(Item.FormatID);
            Writer.WriteU64Ex(Item.SizeBytes);
            Writer.WriteU32Ex(Item.Flags);
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

        uint32_t ItemCount = 0;
        Reader.ReadU32Ex(ItemCount);
        Manifest.Items.reserve(ItemCount);

        for (uint32_t i = 0; i < ItemCount; ++i) {
            ClipboardItemEntry Item;
            uint32_t           NameLen = 0;
            Reader.ReadU32Ex(NameLen);
            if (NameLen > 0) {
                Reader.ReadString(Item.ItemName, NameLen);
            }

            uint32_t ItemMimeLen = 0;
            Reader.ReadU32Ex(ItemMimeLen);
            if (ItemMimeLen > 0) {
                Reader.ReadString(Item.FormatMime, ItemMimeLen);
            }

            Reader.ReadU32Ex(Item.FormatID);
            Reader.ReadU64Ex(Item.SizeBytes);
            Reader.ReadU32Ex(Item.Flags);

            Manifest.Items.push_back(std::move(Item));
        }

        return Manifest;
    }
};
