#ifndef OMNIPACKETS_H
#define OMNIPACKETS_H

#pragma once
#include "ByteStream.h"
#include "OmniEnums.h"

#include <algorithm>
#include <cstdint>
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

    static ConnectionRequest Deserialize(ByteStreamReader& reader)
    {
        ConnectionRequest obj;

        uint8_t DevId;
        reader.ReadU8Ex(DevId);
        obj.DeviceID = DeviceMap(DevId);

        return obj;
    }

    static std::vector<uint8_t> Serialize(const ConnectionRequest& obj)
    {
        ByteVecStreamEx NetWriter{sizeof(ConnectionRequest)};
        NetWriter.WriteU8Ex(obj.DeviceID);
        return NetWriter.Data;
    }
};

struct WindowCreationData
{
    uint32_t NameLen = 0;
    char8_t WindowName[64]{};
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

    void SetTitle(const char* Title, const size_t TitleLen)
    {
        NameLen = static_cast<uint32_t>((std::min)(TitleLen, size_t{63}));

        std::transform(Title, Title + NameLen, WindowName, [](char c) {
            return static_cast<char8_t>(c);
        });

        WindowName[NameLen] = u8'\0';
    }

    void SetTitle(std::string_view title) { SetTitle(title.data(), title.length()); }

    std::u8string GetTitleU8() const { return std::u8string(WindowName); }

    static WindowCreationData Deserialize(ByteStreamReader& reader)
    {
        WindowCreationData obj;

        reader.ReadU32Ex(obj.NameLen);
        obj.NameLen = (std::min)(obj.NameLen, uint32_t{63});
        reader.ReadString(obj.WindowName, obj.NameLen, 64);
        reader.ReadU32Ex(obj.Width);
        reader.ReadU32Ex(obj.Height);

        return obj;
    }

    static std::vector<uint8_t> Serialize(const WindowCreationData& obj)
    {
        ByteVecStreamEx NetWriter{76};
        NetWriter.WriteU32Ex(obj.NameLen);
        NetWriter.WriteBytes(reinterpret_cast<const uint8_t*>(obj.WindowName), obj.NameLen);
        NetWriter.WriteU32Ex(obj.Width);
        NetWriter.WriteU32Ex(obj.Height);
        return NetWriter.Data;
    }
};

struct HandshakeData
{
    struct MonitorRes
    {
        uint32_t Width = 1920;
        uint32_t Height = 1080;
    };

    uint32_t IP = 0;
    DeviceMap DeviceID = DeviceMap::END;
    uint32_t Token = 0;
    uint8_t Key[32]{};
    MonitorRes Resolution;

    static HandshakeData Deserialize(ByteStreamReader& reader)
    {
        HandshakeData obj;

        reader.ReadU32Ex(obj.IP);

        uint8_t DevId;
        reader.ReadU8Ex(DevId);
        obj.DeviceID = DeviceMap(DevId);

        reader.ReadU32Ex(obj.Token);

        reader.ReadBytes(obj.Key, 32);

        reader.ReadU32Ex(obj.Resolution.Width);
        reader.ReadU32Ex(obj.Resolution.Height);

        return obj;
    }

    static std::vector<uint8_t> Serialize(const HandshakeData& obj)
    {
        ByteVecStreamEx NetWriter{49};

        NetWriter.WriteU32Ex(obj.IP);
        NetWriter.WriteU8Ex(static_cast<uint8_t>(obj.DeviceID));
        NetWriter.WriteU32Ex(obj.Token);
        NetWriter.WriteBytes(obj.Key, 32);
        NetWriter.WriteU32Ex(obj.Resolution.Width);
        NetWriter.WriteU32Ex(obj.Resolution.Height);

        return NetWriter.Data;
    }
};

struct HandshakeResponse
{
    enum Action : uint8_t { ACCEPT, REJECT, CANCEL };

    DeviceMap DeviceID = DeviceMap::END;
    Action State = Action::CANCEL;
    bool Trusted = false;

    static HandshakeResponse Deserialize(ByteStreamReader& reader)
    {

        uint8_t TempID;
        uint8_t TempState;
        uint8_t TempTrustState;
        reader.ReadU8Ex(TempID);
        reader.ReadU8Ex(TempState);
        reader.ReadU8Ex(TempTrustState);

        HandshakeResponse obj{(DeviceMap)TempID, (Action)TempState, (bool)TempTrustState};

        return obj;
    }

    static std::vector<uint8_t> Serialize(const HandshakeResponse& obj)
    {
        ByteVecStreamEx NetWriter{sizeof(HandshakeResponse)};
        NetWriter.WriteU8Ex(static_cast<uint8_t>(obj.DeviceID));
        NetWriter.WriteU8Ex(static_cast<uint8_t>(obj.State));
        NetWriter.WriteU8Ex(static_cast<uint8_t>(obj.Trusted));

        return NetWriter.Data;
    }
};

using FuncArgTypes = std::variant<
    ArraySwapLayout,
    ConnectionRequest,
    WindowCreationData,
    HandshakeData,
    HandshakeResponse>;

using DataTypes = std::variant<int>;

struct OmniNetCommand
{
    CoreCommandsWArgs CommandType;
    uint32_t ArgTypeIndex = 0;
    uint32_t ArgArrayLength = 0;
    std::vector<uint8_t> Args;

    OmniNetCommand() = default;

    OmniNetCommand(
        CoreCommandsWArgs InCommandType, uint32_t InArgTypeIndex, std::vector<uint8_t> InArgs
    )
        : CommandType(InCommandType), ArgTypeIndex(InArgTypeIndex),
          ArgArrayLength(static_cast<uint32_t>(InArgs.size())), Args(std::move(InArgs))
    {
    }

    OmniNetCommand(
        CoreCommandsWArgs InCommandType,
        uint32_t InArgTypeIndex,
        uint32_t InLength,
        const uint8_t* InArgs
    )
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
        writer.WriteBytes(obj.Args.data(), payloadLen);

        return writer.Data;
    }

    static void Serialize(const OmniNetCommand& obj, std::vector<uint8_t>& out)
    {
        const uint32_t payloadLen = static_cast<uint32_t>(obj.Args.size());
        const uint32_t totalSize = 1 + 4 + 4 + payloadLen;

        out.clear();
        out.reserve(totalSize);

        ByteStreamEx writer{totalSize, out.data()};
        writer.WriteU8Ex(static_cast<uint8_t>(obj.CommandType));
        writer.WriteU32Ex(obj.ArgTypeIndex);
        writer.WriteU32Ex(payloadLen);
        writer.WriteString(
            std::string_view(reinterpret_cast<const char*>(obj.Args.data()), payloadLen)
        );
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
