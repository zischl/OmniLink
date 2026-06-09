#ifndef OMNIPACKETS_H
#define OMNIPACKETS_H

#pragma once
#include "ByteStream.h"
#include "OmniEnums.h"
#include <cstring>
#include <string>
#include <variant>
#include <vector>

struct ArraySwapLayout
{
    uint32_t index1 = 0;
    uint32_t index2 = 1;

    static ArraySwapLayout Deserialize(ByteStreamReader& reader)
    {
        ArraySwapLayout obj;
        reader.ReadU32Ex(obj.index1);
        reader.ReadU32Ex(obj.index2);
        return obj;
    }

    static std::vector<uint8_t> Serialize(const ArraySwapLayout& obj)
    {
        ByteVecStreamEx NetWriter{sizeof(ArraySwapLayout)};
        NetWriter.WriteU32Ex(obj.index1);
        NetWriter.WriteU32Ex(obj.index2);
        return NetWriter.Data;
    }
};

struct ConnectionRequest
{
    DeviceMap DeviceID;
    char OmniReqKey[32];

    static ConnectionRequest Deserialize(ByteStreamReader& reader)
    {
        ConnectionRequest obj;

        uint8_t DevId;
        reader.ReadU8Ex(DevId);
        obj.DeviceID = DeviceMap(DevId);

        reader.ReadString(obj.OmniReqKey);

        return obj;
    }

    static std::vector<uint8_t> Serialize(const ConnectionRequest& obj)
    {
        ByteVecStreamEx NetWriter{sizeof(ConnectionRequest)};
        NetWriter.WriteU8Ex(obj.DeviceID);
        NetWriter.SafeWriteString(obj.OmniReqKey, 32);

        return NetWriter.Data;
    }
};

struct WindowCreationData
{
    uint32_t NameLen = 0;
    char WindowName[64]{};
    uint32_t Width = 1920;
    uint32_t Height = 1080;

    WindowCreationData() { SetTitle("Default Window", 15); }

    WindowCreationData(const char* Title, const size_t TitleLen, int w = 1920, int h = 1080)
        : Width(w), Height(h)
    {
        SetTitle(Title, TitleLen);
    }

    WindowCreationData(const std::string& Title, const size_t TitleLen, int w = 1920, int h = 1080)
        : Width(w), Height(h)
    {
        SetTitle(Title.c_str(), TitleLen);
    }

    WindowCreationData(std::string_view Title, int w = 1920, int h = 1080) : Width(w), Height(h)
    {
        SetTitle(Title.data(), Title.length());
    }

    void SetTitle(const char* title, const size_t TitleLen)
    {
        strncpy_s(WindowName, title, TitleLen);
        WindowName[TitleLen + 1] = '\0';
    }

    std::wstring GetTitleW() const
    {
        wchar_t WCharName[64]{};
        MultiByteToWideChar(CP_UTF8, 0, WindowName, -1, WCharName, 64);
        return std::wstring(WCharName);
    }

    static WindowCreationData Deserialize(ByteStreamReader& reader)
    {
        WindowCreationData obj;

        reader.ReadU32Ex(obj.NameLen);
        reader.ReadString(obj.WindowName, 64);
        reader.ReadU32Ex(obj.Width);
        reader.ReadU32Ex(obj.Height);

        return obj;
    }

    static std::vector<uint8_t> Serialize(const WindowCreationData& obj)
    {
        ByteVecStreamEx NetWriter{76};
        NetWriter.WriteU32Ex(obj.NameLen);
        NetWriter.WriteString(obj.WindowName);
        NetWriter.WriteU32Ex(obj.Width);
        NetWriter.WriteU32Ex(obj.Height);
        return NetWriter.Data;
    }
};

struct TestArg
{
    int x = 0;
};

using FuncArgTypes = std::variant<ArraySwapLayout, ConnectionRequest, WindowCreationData>;

using DataTypes = std::variant<int>;

struct OmniNetCommand
{
    CoreCommandsWArgs CommandType;
    uint32_t ArgTypeIndex = 0;
    uint32_t ArgArrayLength = 0;
    std::vector<uint8_t> Args;

    OmniNetCommand() = default;

    OmniNetCommand(CoreCommandsWArgs InCommandType,
                   uint32_t InArgTypeIndex,
                   std::vector<uint8_t> InArgs)
        : CommandType(InCommandType), ArgTypeIndex(InArgTypeIndex),
          ArgArrayLength(static_cast<uint32_t>(InArgs.size())), Args(std::move(InArgs))
    {
    }

    OmniNetCommand(CoreCommandsWArgs InCommandType,
                   uint32_t InArgTypeIndex,
                   const uint8_t* InArgs,
                   uint32_t InLength)
        : CommandType(InCommandType), ArgTypeIndex(InArgTypeIndex), ArgArrayLength(InLength),
          Args(InArgs, InArgs + InLength)
    {
    }

    static std::vector<uint8_t> Serialize(const OmniNetCommand& obj)
    {
        const uint32_t payloadLen = static_cast<uint32_t>(obj.Args.size());

        ByteVecStreamEx writer{static_cast<uint32_t>(1 + 4 + 4 + payloadLen)};

        writer.WriteU8Ex(static_cast<uint8_t>(obj.CommandType));
        writer.WriteU32Ex(obj.ArgTypeIndex);
        writer.WriteU32Ex(payloadLen);

        writer.Data.insert(writer.Data.end(), obj.Args.begin(), obj.Args.end());
        writer.CurrentLength += payloadLen;

        return writer.Data;
    }

    static void Serialize(const OmniNetCommand& obj, std::vector<uint8_t>& out)
    {
        const uint32_t payloadLen = static_cast<uint32_t>(obj.Args.size());
        const uint32_t totalSize = 1 + 4 + 4 + payloadLen;

        out.clear();
        out.reserve(totalSize);

        ByteVecStreamEx writer{totalSize};

        writer.WriteU8Ex(static_cast<uint8_t>(obj.CommandType));
        writer.WriteU32Ex(obj.ArgTypeIndex);
        writer.WriteU32Ex(payloadLen);

        writer.Data.insert(writer.Data.end(), obj.Args.begin(), obj.Args.end());
        writer.CurrentLength += payloadLen;

        out = std::move(writer.Data);
    }

    static OmniNetCommand Deserialize(ByteStreamReader& Reader)
    {
        OmniNetCommand Cmd{};

        uint8_t RawCommandType = 0;
        Reader.ReadU8Ex(RawCommandType);
        Cmd.CommandType = static_cast<CoreCommandsWArgs>(RawCommandType);

        Reader.ReadU32Ex(Cmd.ArgTypeIndex);
        Reader.ReadU32Ex(Cmd.ArgArrayLength);

        Cmd.Args.resize(Cmd.ArgArrayLength);

        Reader.ReadBytes(Cmd.Args.data(), Cmd.ArgArrayLength);

        return Cmd;
    }
};

struct OmniCommand
{
    CoreCommandsWArgs CommandType = CoreCommandsWArgs::SwapLayout;
    uint32_t ArgTypeIndex = 0;
    FuncArgTypes Args = ArraySwapLayout{0, 0};

    OmniCommand() = default;

    OmniCommand(CoreCommandsWArgs InCommandType, uint32_t InArgTypeIndex, FuncArgTypes InArgs)
        : CommandType(InCommandType), ArgTypeIndex(InArgTypeIndex), Args(std::move(InArgs))
    {
    }

    explicit OmniCommand(const OmniNetCommand& NetCmd)
        : CommandType(NetCmd.CommandType), ArgTypeIndex(NetCmd.ArgTypeIndex)
    {
    }
};

#endif // OMNIPACKETS_H
