#include "ClipboardLink.h"
#include "OmniConfig.hpp"
#include "OmniInstances.h"
#include "OmniLogger.hpp"
#include "SessionHandler.hpp"

#include <shellapi.h>
#include <thread>

OmniNet::PoolConfig SystemClipboardLink::SetState(
    SystemLinkContext& Ctx,
    DeviceMap          DeviceID,
    FeatureActionRoute Route,
    FeatureAction      Action,
    SubStreamID        SubStreamID,
    void*              Context
)
{
    (void)SubStreamID;
    (void)Route;
    (void)Context;

    ActiveInstances = Ctx.ActiveInstances;

    uint16_t DeviceBit = (DeviceID < DeviceMap::END)
                             ? static_cast<uint16_t>(1U << static_cast<uint8_t>(DeviceID))
                             : 0;

    if (Action == FeatureAction::Activate) {
        uint16_t PrevMask =
            ActiveClipboardSubscriptions.fetch_or(DeviceBit, std::memory_order_acq_rel);
        if (PrevMask == 0 && !ClipboardLink.GetState()) {
            ClipboardLink.StartMonitoring(
                Ctx.WindowID,
                [this](const std::string& Text) { TransmitClipboard(Text); },
                [this](const ClipboardManifest& Manifest) { TransmitClipboardManifest(Manifest); }
            );
        }
    } else if (Action == FeatureAction::Deactivate) {
        uint16_t PrevMask =
            ActiveClipboardSubscriptions.fetch_and(~DeviceBit, std::memory_order_acq_rel);
        if ((PrevMask & DeviceBit) != 0 && (PrevMask & ~DeviceBit) == 0) {
            ClipboardLink.StopMonitoring();
            std::lock_guard<std::mutex> Lock(ClipboardStreamsMutex);
            for (auto& [StreamID, Stream] : ActiveClipboardStreams) {
                if (Stream) {
                    Stream->End();
                }
            }
            ActiveClipboardStreams.clear();
        }
    }

    Logger::log(
        "{:s} ClipboardSync for DeviceID {:d}",
        Action == FeatureAction::Activate ? "Enabled" : "Disabled",
        static_cast<int>(DeviceID)
    );
    return OmniNet::PoolConfig{};
}

void SystemClipboardLink::TransmitClipboard(const std::string& Text)
{
    if (Text.empty() || !ActiveInstances)
        return;

    std::vector<uint8_t> Payload(1 + Text.size());
    Payload[0] = static_cast<uint8_t>(ClipboardOp::LightGram);
    std::memcpy(Payload.data() + 1, Text.data(), Text.size());

    OmniNet::OmniHeader Header;
    Header.PacketType = OmniNet::PacketType::ProcClipboard;
    Header.Target     = 0;
    Header.Flags      = 0;

    for (auto& [DevID, Instance] : *ActiveInstances) {
        if (Instance.GetFeatureState(FeatureTypes::ClipboardLink, FeatureActionRoute::Outbound)) {
            if (Instance.InstanceSession) {
                Instance.InstanceSession->SessionSend(
                    reinterpret_cast<char*>(Payload.data()),
                    static_cast<int>(Payload.size()),
                    Header
                );
                Logger::log(
                    "Transmit completed for {:d} bytes of clipboard data to DeviceID {:d}",
                    Text.size(),
                    static_cast<int>(DevID)
                );
            }
        }
    }
}

void SystemClipboardLink::TransmitClipboardManifest(const ClipboardManifest& Manifest)
{
    if (!ActiveInstances)
        return;

    static std::atomic<uint32_t> GlobalStreamID{1};
    uint32_t                     StreamID = GlobalStreamID.fetch_add(1);

    auto Stream = std::make_shared<OmniTCPStream>(StreamID);
    if (!Stream->StartServer(0)) {
        Logger::log("Failed to start TCP stream server for clipboard link");
        return;
    }

    ClipboardManifest CManifest = Manifest;
    CManifest.StreamID          = StreamID;
    CManifest.ServerPort        = Stream->GetLocalPort();

    for (auto& [DevID, Instance] : *ActiveInstances) {
        Instance.RegisterTCPStream(StreamID, Stream);
    }

    std::vector<uint8_t>      LocalBuffer;
    std::vector<std::wstring> LocalFilePaths;

    int Retries = 5;
    while (!OpenClipboard(nullptr) && Retries-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (OpenClipboard(nullptr)) {
        if (CManifest.Category == ClipboardCategory::Image ||
            CManifest.Category == ClipboardCategory::Text) {
            HANDLE HData = GetClipboardData(CManifest.WinFormatID);
            if (HData) {
                size_t Size = GlobalSize(HData);
                void*  Ptr  = GlobalLock(HData);
                if (Ptr) {
                    if (Size > 0) {
                        LocalBuffer.resize(Size);
                        std::memcpy(LocalBuffer.data(), Ptr, Size);
                    }
                    GlobalUnlock(HData);
                }
            }
        } else if (CManifest.Category == ClipboardCategory::FileList) {
            HANDLE DropHandle = GetClipboardData(CF_HDROP);
            if (DropHandle) {
                HDROP Drop = static_cast<HDROP>(GlobalLock(DropHandle));
                if (Drop) {
                    UINT FileCount = DragQueryFileW(Drop, 0xFFFFFFFF, nullptr, 0);
                    for (UINT i = 0; i < FileCount; ++i) {
                        wchar_t FilePath[MAX_PATH]{};
                        if (DragQueryFileW(Drop, i, FilePath, MAX_PATH) > 0) {
                            LocalFilePaths.push_back(FilePath);
                        }
                    }
                    GlobalUnlock(DropHandle);
                }
            }
        }
        CloseClipboard();
    }

    auto StreamProgressData = std::make_shared<StreamProgress>();
    StreamProgressData->TotalBytes.store(CManifest.TotalSizeBytes, std::memory_order_relaxed);
    StreamProgressData->BytesTransferred.store(0, std::memory_order_relaxed);
    StreamProgressData->StreamState.store(true, std::memory_order_relaxed);
    StreamProgressData->Cancel.store(false, std::memory_order_relaxed);

    if (OnClipboardStreamEvent &&
        CManifest.TotalSizeBytes > LIGHTGRAM_MAX_SIZE) {
        std::string DisplayName =
            CManifest.Items.empty() ? "Clipboard Item" : CManifest.Items[0].ItemName;

        ClipboardStreamEvent Ev(
            CManifest.StreamID,
            DeviceMap::END,
            DisplayName,
            CManifest.Category == ClipboardCategory::Image
                ? "Image"
                : (CManifest.Category == ClipboardCategory::FileList ? "File" : "Text"),
            CManifest.TotalSizeBytes,
            true,
            StreamProgressData
        );
        OnClipboardStreamEvent(Ev);
    }

    std::thread([Stream,
                 Buffer    = std::move(LocalBuffer),
                 FilePaths = std::move(LocalFilePaths),
                 Progress  = StreamProgressData]() {
        if (Stream->AcceptClient(15000)) {
            if (!Buffer.empty()) {
                Stream->StreamBuffer(Buffer.data(), Buffer.size());
                if (Progress) {
                    Progress->BytesTransferred.store(Buffer.size(), std::memory_order_relaxed);
                }
            } else if (!FilePaths.empty()) {
                for (const auto& FilePath : FilePaths) {
                    if (Progress && Progress->Cancel.load(std::memory_order_relaxed)) {
                        break;
                    }
                    Stream->StreamFile(FilePath);
                }
            }
        }
        if (Progress) {
            Progress->StreamState.store(false, std::memory_order_relaxed);
        }
        Stream->End();
    }).detach();

    std::vector<uint8_t> Serialized = ClipboardManifest::Serialize(CManifest);
    std::vector<uint8_t> Payload(1 + Serialized.size());
    Payload[0] = static_cast<uint8_t>(ClipboardOp::Manifest);
    std::memcpy(Payload.data() + 1, Serialized.data(), Serialized.size());

    OmniNet::OmniHeader Header;
    Header.PacketType = OmniNet::PacketType::ProcClipboard;
    Header.Target     = 0;
    Header.Flags      = 0;

    for (auto& [DevID, Instance] : *ActiveInstances) {
        if (Instance.GetFeatureState(FeatureTypes::ClipboardLink, FeatureActionRoute::Outbound)) {
            if (Instance.InstanceSession) {
                Instance.InstanceSession->SessionSend(
                    reinterpret_cast<char*>(Payload.data()),
                    static_cast<int>(Payload.size()),
                    Header
                );
                Logger::log(
                    "Transmit completed for ClipboardManifest promises ({:s}, Size: {:d} bytes, "
                    "Port: {:d}) to "
                    "DeviceID {:d}",
                    CManifest.FormatMime.c_str(),
                    CManifest.TotalSizeBytes,
                    CManifest.ServerPort,
                    static_cast<int>(DevID)
                );
            }
        }
    }
}
