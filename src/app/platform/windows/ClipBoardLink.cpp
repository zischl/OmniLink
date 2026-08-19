#include "ClipBoardLink.h"
#include "OmniLogger.h"
#include <chrono>
#include <thread>

ClipBoardLink::~ClipBoardLink()
{
    StopMonitoring();
}

void ClipBoardLink::StartMonitoring(HWND WindowHandle, ClipboardChangeCallback Callback)
{
    if (HookState.load()) {
        return;
    }

    WindowID = WindowHandle;
    ChangeCallback = std::move(Callback);
    LastSequenceNumber = GetClipboardSequenceNumber();
    LastText = GetClipTypeText();

    if (WindowID && AddClipboardFormatListener(WindowID)) {
        HookState.store(true);
        Logger::log("ClipBoardLink: Registered Win32 Clipboard listener");
    } else {
        Logger::log("ClipBoardLink: Failed to register Clipboard Listener");
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
        Logger::log("ClipBoardLink: Unregistered Win32 Clipboard listener");
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

    if (SequenceUpdateState) {
        std::string CurrentText = GetClipTypeText();
        bool UpdateAvailable = false;

        {
            std::lock_guard<std::mutex> Lock(TextMutex);
            if (!CurrentText.empty() && CurrentText != LastText) {
                LastText = CurrentText;
                UpdateAvailable = true;
            }
        }

        if (UpdateAvailable && ChangeCallback) {
            ChangeCallback(CurrentText);
        }
    }
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
