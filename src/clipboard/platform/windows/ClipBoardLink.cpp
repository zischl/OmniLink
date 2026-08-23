#include "ClipBoardLink.h"
#include "OmniConfig.h"
#include "OmniLogger.h"

#include <chrono>
#include <shellapi.h>
#include <thread>

ClipBoardLink::~ClipBoardLink()
{
    StopMonitoring();
}

void ClipBoardLink::StartMonitoring(
    HWND WindowHandle, ClipboardChangeCallback LightGramCB, ClipboardManifestCallback ManifestCB
)
{
    if (HookState.load()) {
        return;
    }

    WindowID = WindowHandle;
    ChangeCallback = std::move(LightGramCB);
    ManifestCallback = std::move(ManifestCB);
    LastSequenceNumber = GetClipboardSequenceNumber();
    LastText = GetClipTypeText();

    if (WindowID && AddClipboardFormatListener(WindowID)) {
        HookState.store(true);
        Logger::log("Registered Clipboard listener");
    } else {
        Logger::log("Failed to register Clipboard Listener");
    }
}

void ClipBoardLink::StopMonitoring()
{
    if (!HookState.exchange(false)) {
        return;
    }

    if (WindowID) {
        RemoveClipboardFormatListener(WindowID);
        WindowID = nullptr;
        Logger::log("Unregistered Clipboard listener");
    }
}

void ClipBoardLink::OnClipboardUpdate()
{
    if (!HookState.load()) {
        return;
    }

    DWORD CurrentSeqItem = GetClipboardSequenceNumber();
    bool SequenceUpdateState = false;

    {
        std::lock_guard<std::mutex> Lock(TextMutex);
        if (CurrentSeqItem != LastSequenceNumber) {
            LastSequenceNumber = CurrentSeqItem;
            SequenceUpdateState = true;
        }
    }

    if (!SequenceUpdateState) {
        return;
    }

    int Retries = 5;
    while (!OpenClipboard(nullptr) && Retries-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!OpenClipboard(nullptr)) {
        return;
    }

    bool DIBState = IsClipboardFormatAvailable(CF_DIBV5) || IsClipboardFormatAvailable(CF_DIB);
    bool HDropState = IsClipboardFormatAvailable(CF_HDROP);
    bool TextState = IsClipboardFormatAvailable(CF_UNICODETEXT);

    // Handling files/folders
    if (HDropState && ManifestCallback) {
        HANDLE DropHandle = GetClipboardData(CF_HDROP);
        if (DropHandle) {
            HDROP Drop = static_cast<HDROP>(GlobalLock(DropHandle));
            if (Drop) {
                UINT FileCount = DragQueryFileW(Drop, 0xFFFFFFFF, nullptr, 0);
                ClipboardManifest Manifest;
                Manifest.Category = ClipboardCategory::FileList;
                Manifest.FormatMime = "CF_HDROP";
                Manifest.WinFormatID = CF_HDROP;
                Manifest.TotalSizeBytes = 0;

                for (UINT i = 0; i < FileCount; ++i) {
                    wchar_t FilePath[MAX_PATH]{};
                    if (DragQueryFileW(Drop, i, FilePath, MAX_PATH) > 0) {
                        WIN32_FILE_ATTRIBUTE_DATA FileData{};
                        bool IsDirectory = false;
                        uint64_t FileSize = 0;

                        if (GetFileAttributesExW(FilePath, GetFileExInfoStandard, &FileData)) {
                            IsDirectory = (FileData.dwFileAttributes != INVALID_FILE_ATTRIBUTES) &&
                                          (FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
                            if (!IsDirectory) {
                                FileSize = (static_cast<uint64_t>(FileData.nFileSizeHigh) << 32) |
                                           FileData.nFileSizeLow;
                            }
                        }

                        const wchar_t* BaseName = wcsrchr(FilePath, L'\\');
                        if (!BaseName) {
                            BaseName = wcsrchr(FilePath, L'/');
                        }
                        BaseName = BaseName ? (BaseName + 1) : FilePath;

                        int Utf8Len = WideCharToMultiByte(
                            CP_UTF8, 0, BaseName, -1, nullptr, 0, nullptr, nullptr
                        );
                        std::string NameStr;
                        if (Utf8Len > 1) {
                            NameStr.resize(Utf8Len - 1);
                            WideCharToMultiByte(
                                CP_UTF8, 0, BaseName, -1, NameStr.data(), Utf8Len, nullptr, nullptr
                            );
                        }

                        ClipboardItemEntry Entry;
                        Entry.ItemName = std::move(NameStr);
                        Entry.FormatMime = "CF_HDROP";
                        Entry.FormatID = CF_HDROP;
                        Entry.SizeBytes = FileSize;
                        Entry.Flags = IsDirectory ? ItemFlag_Directory : ItemFlag_None;

                        Manifest.TotalSizeBytes += FileSize;
                        Manifest.Items.push_back(std::move(Entry));
                    }
                }

                GlobalUnlock(DropHandle);
                CloseClipboard();

                ManifestCallback(Manifest);
                return;
            }
        }
    }

    if (DIBState && ManifestCallback) {
        UINT Format = IsClipboardFormatAvailable(CF_DIBV5) ? CF_DIBV5 : CF_DIB;
        HANDLE DataHandle = GetClipboardData(Format);
        if (DataHandle) {
            size_t DataSize = GlobalSize(DataHandle);
            CloseClipboard();

            ClipboardManifest Manifest;
            Manifest.Category = ClipboardCategory::Image;
            Manifest.FormatMime = (Format == CF_DIBV5) ? "CF_DIBV5" : "CF_DIB";
            Manifest.WinFormatID = Format;
            Manifest.TotalSizeBytes = DataSize;

            ClipboardItemEntry Entry;
            Entry.ItemName = "clipboard_image.bmp";
            Entry.FormatMime = Manifest.FormatMime;
            Entry.FormatID = Format;
            Entry.SizeBytes = DataSize;
            Entry.Flags = ItemFlag_None;
            Manifest.Items.push_back(std::move(Entry));

            ManifestCallback(Manifest);
            return;
        }
    }

    if (TextState) {
        CloseClipboard();
        std::string CurrentText = GetClipTypeText();
        bool UpdateAvailable = false;

        {
            std::lock_guard<std::mutex> Lock(TextMutex);
            if (!CurrentText.empty() && CurrentText != LastText) {
                LastText = CurrentText;
                UpdateAvailable = true;
            }
        }

        if (UpdateAvailable) {
            if (CurrentText.size() <= LIGHTGRAM_MAX_SIZE && ChangeCallback) {
                ChangeCallback(CurrentText);
            } else if (CurrentText.size() > LIGHTGRAM_MAX_SIZE && ManifestCallback) {
                ClipboardManifest Manifest;
                Manifest.Category = ClipboardCategory::Text;
                Manifest.FormatMime = "text/plain";
                Manifest.WinFormatID = CF_UNICODETEXT;
                Manifest.TotalSizeBytes = CurrentText.size();

                ClipboardItemEntry Entry;
                Entry.ItemName = CurrentText.substr(0, (std::min)(CurrentText.size(), size_t(32)));
                Entry.FormatMime = "text/plain";
                Entry.FormatID = CF_UNICODETEXT;
                Entry.SizeBytes = CurrentText.size();
                Entry.Flags = ItemFlag_None;
                Manifest.Items.push_back(std::move(Entry));

                ManifestCallback(Manifest);
            }
        }
        return;
    }

    CloseClipboard();
}

void ClipBoardLink::SetPasteRequestCallback(PasteRequestCallback Callback)
{
    std::lock_guard<std::mutex> Lock(ManifestMutex);
    PRequestCallback = std::move(Callback);
}

void ClipBoardLink::AddClipItemPromise(const ClipboardManifest& Manifest)
{
    if (!WindowID) {
        return;
    }

    {
        std::lock_guard<std::mutex> Lock(ManifestMutex);
        PendingManifest = Manifest;
    }

    int Retries = 5;
    while (!OpenClipboard(WindowID) && Retries-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!OpenClipboard(WindowID)) {
        return;
    }

    EmptyClipboard();

    if (Manifest.Category == ClipboardCategory::Image) {
        SetClipboardData(CF_DIBV5, nullptr);
        SetClipboardData(CF_DIB, nullptr);
    } else if (Manifest.Category == ClipboardCategory::FileList) {
        SetClipboardData(CF_HDROP, nullptr);
    } else if (Manifest.Category == ClipboardCategory::Text) {
        SetClipboardData(CF_UNICODETEXT, nullptr);
    }

    CloseClipboard();

    {
        std::lock_guard<std::mutex> Lock(TextMutex);
        LastSequenceNumber = GetClipboardSequenceNumber();
    }

    Logger::log(
        "Clipboard item available | {:s} (Size: {:d} bytes)",
        Manifest.FormatMime.c_str(),
        Manifest.TotalSizeBytes
    );
}

void ClipBoardLink::OnPasteRequest(UINT uFormat)
{
    ClipboardManifest Manifest;
    PasteRequestCallback Callback;

    {
        std::lock_guard<std::mutex> Lock(ManifestMutex);
        Manifest = PendingManifest;
        Callback = PRequestCallback;
    }

    if (!Callback || Manifest.TotalSizeBytes == 0) {
        Logger::log("Clipboard paste request failed due to no pull callback or empty manifest");
        return;
    }

    Logger::log("Fetching remote clipboard data for format {:d}...", uFormat);
    std::vector<uint8_t> Data = Callback(Manifest, uFormat);

    if (Data.empty()) {
        Logger::log("Failed to retrieve remote clipboard stream for format {:d}", uFormat);
        return;
    }

    HGLOBAL HMem = GlobalAlloc(GMEM_MOVEABLE, Data.size());
    if (!HMem) {
        Logger::log("Failed to allocate HGLOBAL for clipboard render");
        return;
    }

    void* Ptr = GlobalLock(HMem);
    if (!Ptr) {
        GlobalFree(HMem);
        return;
    }

    std::memcpy(Ptr, Data.data(), Data.size());
    GlobalUnlock(HMem);

    if (SetClipboardData(uFormat, HMem) == nullptr) {
        GlobalFree(HMem);
        Logger::log("SetClipboardData failed during WM_RENDERFORMAT");
    } else {
        Logger::log("WM_RENDERFORMAT completed successfully for format {:d}", uFormat);
    }
}

void ClipBoardLink::OnRequestInvalidation()
{
    std::lock_guard<std::mutex> Lock(ManifestMutex);
    PendingManifest = ClipboardManifest{};
    Logger::log("Clipboard request invalidation event processed");
}

std::string ClipBoardLink::GetClipTypeText()
{
    int Retries = 5;
    while (!OpenClipboard(nullptr) && Retries-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!OpenClipboard(nullptr)) {
        return "";
    }

    HANDLE DataHandle = GetClipboardData(CF_UNICODETEXT);
    if (!DataHandle) {
        CloseClipboard();
        return "";
    }

    wchar_t* PWText = static_cast<wchar_t*>(GlobalLock(DataHandle));
    if (!PWText) {
        CloseClipboard();
        return "";
    }

    int Utf8Len = WideCharToMultiByte(CP_UTF8, 0, PWText, -1, nullptr, 0, nullptr, nullptr);
    std::string Result;
    if (Utf8Len > 1) {
        Result.resize(Utf8Len - 1);
        WideCharToMultiByte(CP_UTF8, 0, PWText, -1, Result.data(), Utf8Len, nullptr, nullptr);
    }

    GlobalUnlock(DataHandle);
    CloseClipboard();

    return Result;
}

bool ClipBoardLink::SetClipTypeText(const std::string& Text)
{
    if (Text.empty()) {
        return false;
    }

    int WLen = MultiByteToWideChar(CP_UTF8, 0, Text.c_str(), -1, nullptr, 0);
    if (WLen <= 0) {
        return false;
    }

    HGLOBAL HMem = GlobalAlloc(GMEM_MOVEABLE, WLen * sizeof(wchar_t));
    if (!HMem) {
        return false;
    }

    wchar_t* PWText = static_cast<wchar_t*>(GlobalLock(HMem));
    if (!PWText) {
        GlobalFree(HMem);
        return false;
    }

    MultiByteToWideChar(CP_UTF8, 0, Text.c_str(), -1, PWText, WLen);
    GlobalUnlock(HMem);

    int Retries = 5;
    while (!OpenClipboard(nullptr) && Retries-- > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!OpenClipboard(nullptr)) {
        GlobalFree(HMem);
        return false;
    }

    EmptyClipboard();
    if (SetClipboardData(CF_UNICODETEXT, HMem) == nullptr) {
        GlobalFree(HMem);
        CloseClipboard();
        return false;
    }

    CloseClipboard();

    {
        std::lock_guard<std::mutex> Lock(TextMutex);
        LastText = Text;
        LastSequenceNumber = GetClipboardSequenceNumber();
    }

    return true;
}
