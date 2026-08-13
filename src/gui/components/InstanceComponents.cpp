#include "OmniGUI.h"
#include <cstdio>
#include <ctime>

namespace {
static const char* GetDeviceIcon(DeviceType Type)
{
    switch (Type) {
    case DeviceType::Laptop:
        return IC_LAPTOP;
    case DeviceType::Desktop:
        return IC_AIRPLAY;
    case DeviceType::Mobile:
        return "";
    case DeviceType::Unknown:
    default:
        return IC_AIRPLAY;
    }
}
} // namespace

GroupCardAction OmniGUI::InstanceGroupCard(
    const OmniInstanceGroup& Group,
    size_t CardIdx,
    ImVec2 CardPos,
    float CardWidth,
    float CardHeight
)
{
    GroupCardAction Action = GroupCardAction::None;

    ImGui::PushID("GroupCard");
    ImGui::PushID(static_cast<int>(CardIdx));

    ImU32 BorderCol = Group.State ? IM_COL32(168, 85, 247, 255) : COL_BORDER;

    // Card Frame
    DrawList->AddRectFilled(
        CardPos, ImVec2(CardPos.x + CardWidth, CardPos.y + CardHeight), COL_BG_CHILD_1, 12.0f
    );
    DrawList->AddRect(
        CardPos,
        ImVec2(CardPos.x + CardWidth, CardPos.y + CardHeight),
        BorderCol,
        12.0f,
        0,
        Group.State ? 1.5f : 1.0f
    );

    // Icon Box
    const char* GroupIcon = IC_WAYPOINTS;

    ImVec2 IconBoxMin = ImVec2(CardPos.x + 16.0f, CardPos.y + 16.0f);
    ImVec2 IconBoxMax = ImVec2(IconBoxMin.x + 40.0f, IconBoxMin.y + 40.0f);
    ImU32 IconBg = Group.State ? COL_FEAT_IC_ACTIVE : COL_BG_CHILD_2;
    DrawList->AddRectFilled(IconBoxMin, IconBoxMax, IconBg, 8.0f);

    if (OmniIconsMedium)
        ImGui::PushFont(OmniIconsMedium);
    ImVec2 IconSize = ImGui::CalcTextSize(GroupIcon);
    DrawList->AddText(
        ImVec2(
            IconBoxMin.x + (40.0f - IconSize.x) * 0.5f, IconBoxMin.y + (40.0f - IconSize.y) * 0.5f
        ),
        Group.State ? COL_FEAT_TINT_ACT : COL_MENU_TXT_IDLE,
        GroupIcon
    );
    if (OmniIconsMedium)
        ImGui::PopFont();

    // Group Name
    ImVec2 TextPos = ImVec2(IconBoxMax.x + 14.0f, CardPos.y + 16.0f);
    if (InterMed16)
        ImGui::PushFont(InterMed16);
    DrawList->AddText(TextPos, ImGui::GetColorU32(COL4_TEXT_ACTIVE), Group.GroupName);
    if (InterMed16)
        ImGui::PopFont();

    DrawList->AddText(
        ImVec2(TextPos.x, TextPos.y + 22.0f), ImGui::GetColorU32(COL4_TEXT_MUTED), Group.Subtitle
    );

    // Status
    const char* StatusText = Group.State ? "ACTIVE" : "IDLE";
    ImU32 StatusDotCol = Group.State ? IM_COL32(168, 85, 247, 255) : IM_COL32(100, 105, 125, 255);
    ImVec2 BadgeSize = ImGui::CalcTextSize(StatusText);
    ImVec2 BadgePos = ImVec2(CardPos.x + CardWidth - BadgeSize.x - 16.0f, CardPos.y + 18.0f);

    DrawList->AddCircleFilled(
        ImVec2(BadgePos.x - 8.0f, BadgePos.y + BadgeSize.y * 0.5f), 3.5f, StatusDotCol
    );
    DrawList->AddText(BadgePos, StatusDotCol, StatusText);

    // Devices
    ImVec2 MetaPos = ImVec2(CardPos.x + 16.0f, CardPos.y + CardHeight - 44.0f);
    DrawList->AddText(MetaPos, ImGui::GetColorU32(COL4_TEXT_MUTED), "Devices");

    std::string DevCountStr = std::to_string(Group.DeviceCount);
    DrawList->AddText(
        ImVec2(MetaPos.x, MetaPos.y + 16.0f),
        ImGui::GetColorU32(COL4_TEXT_ACTIVE),
        DevCountStr.c_str()
    );

    char dateStr[32] = "N/A";
    if (Group.DateCreated != 0) {
        std::time_t t = static_cast<std::time_t>(Group.DateCreated);
        std::tm tm_buf{};
#if defined(_WIN32)
        localtime_s(&tm_buf, &t);
#else
        localtime_r(&t, &tm_buf);
#endif
        std::strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", &tm_buf);
    }

    DrawList->AddText(
        ImVec2(MetaPos.x + 80.0f, MetaPos.y), ImGui::GetColorU32(COL4_TEXT_MUTED), "Created"
    );
    DrawList->AddText(
        ImVec2(MetaPos.x + 80.0f, MetaPos.y + 16.0f), ImGui::GetColorU32(COL4_TEXT_ACTIVE), dateStr
    );

    ImGui::SetCursorScreenPos(
        ImVec2(CardPos.x + CardWidth - 188.0f, CardPos.y + CardHeight - 46.0f)
    );

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    // Connect Button
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(140, 80, 240, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(160, 95, 255, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(120, 65, 220, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(180, 110, 255, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));

    if (InterReg14)
        ImGui::PushFont(InterReg14);
    if (ImGui::Button("Connect", ImVec2(90, 32))) {
        Action = GroupCardAction::ConnectGroup;
    }
    if (InterReg14)
        ImGui::PopFont();
    ImGui::PopStyleColor(5);

    ImGui::SameLine(0.0f, 8.0f);

    if (OmniIconsSmall)
        ImGui::PushFont(OmniIconsSmall);

    // Settings Button... doesn't do any shit yet
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(24, 26, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(42, 38, 64, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(56, 48, 86, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(56, 44, 80, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(210, 180, 255, 255));

    if (ImGui::Button(IC_SETTINGS, ImVec2(34, 32))) {
        Action = GroupCardAction::ConfigureSettings;
    }
    ImGui::PopStyleColor(5);

    ImGui::SameLine(0.0f, 8.0f);

    // Delete Button
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(32, 20, 26, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(180, 30, 45, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(220, 38, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(90, 35, 45, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(240, 110, 120, 255));

    if (ImGui::Button(IC_TRASH_2, ImVec2(34, 32))) {
        Action = GroupCardAction::DeleteGroup;
    }
    ImGui::PopStyleColor(5);
    if (OmniIconsSmall)
        ImGui::PopFont();
    ImGui::PopStyleVar(2);

    ImGui::SetCursorScreenPos(ImVec2(CardPos.x + CardWidth, CardPos.y + CardHeight));
    ImGui::Dummy(ImVec2(0.0f, 0.0f));

    ImGui::PopID();
    ImGui::PopID();

    return Action;
}

ActiveCardAction OmniGUI::ActiveInstanceCard(
    DeviceMap DevId,
    const OmniInstance& Instance,
    size_t CardIdx,
    ImVec2 CardPos,
    float CardWidth,
    float CardHeight
)
{
    ActiveCardAction Action = ActiveCardAction::None;

    ImGui::PushID("ActiveInstanceCard");
    ImGui::PushID(static_cast<int>(CardIdx));

    bool IsActive = (Instance.LinkState == NetLinkState::LINKED || Instance.InstanceIP != 0);
    const char* Hostname = Instance.InstanceName[0] ? Instance.InstanceName : "Node";
    const char* IPAddress = Instance.IPv4_String[0] ? Instance.IPv4_String : "192.168.1.120";
    const char* Subtitle = "Windows Workstation";
    int SlotComboIdx = (static_cast<uint8_t>(DevId) < 9) ? static_cast<int>(DevId) : 1;

    ImGui::SetCursorScreenPos(CardPos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, COL_BG_CHILD_1);
    ImGui::PushStyleColor(ImGuiCol_Border, IsActive ? IM_COL32(168, 85, 247, 255) : COL_BORDER);

    ImGui::BeginChild(
        "ActiveCard",
        ImVec2(CardWidth, CardHeight),
        true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    const char* ActiveDevIcon = GetDeviceIcon(Instance.Type);

    ImVec2 IconBoxMin = ImVec2(CardPos.x + 14.0f, CardPos.y + (CardHeight - 40.0f) * 0.5f);
    ImVec2 IconBoxMax = ImVec2(IconBoxMin.x + 40.0f, IconBoxMin.y + 40.0f);
    ImU32 IconBg = IsActive ? COL_FEAT_IC_ACTIVE : COL_BG_CHILD_2;
    DrawList->AddRectFilled(IconBoxMin, IconBoxMax, IconBg, 8.0f);

    if (OmniIconsMedium)
        ImGui::PushFont(OmniIconsMedium);
    ImVec2 IcSize = ImGui::CalcTextSize(ActiveDevIcon);
    DrawList->AddText(
        ImVec2(IconBoxMin.x + (40.0f - IcSize.x) * 0.5f, IconBoxMin.y + (40.0f - IcSize.y) * 0.5f),
        IsActive ? COL_FEAT_TINT_ACT : COL_MENU_TXT_IDLE,
        ActiveDevIcon
    );
    if (OmniIconsMedium)
        ImGui::PopFont();

    // Text Info
    ImGui::SetCursorScreenPos(ImVec2(CardPos.x + 64.0f, CardPos.y + (CardHeight - 38.0f) * 0.5f));
    ImGui::BeginGroup();
    if (InterMed16)
        ImGui::PushFont(InterMed16);
    ImGui::TextColored(COL4_TEXT_ACTIVE, "%s", Hostname);
    if (InterMed16)
        ImGui::PopFont();

    if (InterReg14)
        ImGui::PushFont(InterReg14);
    ImGui::TextColored(COL4_TEXT_MUTED, "%s  |  %s", Subtitle, IPAddress);
    if (InterReg14)
        ImGui::PopFont();
    ImGui::EndGroup();

    // Mini 3x3 Grid Widget
    float RightClusterWidth = 176.0f;
    float RightStartX = CardPos.x + CardWidth - RightClusterWidth - 16.0f;
    float CenterY = CardPos.y + (CardHeight - 32.0f) * 0.5f;

    ImVec2 GridWidgetPos = ImVec2(RightStartX, CardPos.y + (CardHeight - 25.0f) * 0.5f);
    Mini3x3GridWidget(GridWidgetPos, SlotComboIdx);

    ImGui::SetCursorScreenPos(ImVec2(RightStartX + 39.0f, CenterY));

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    if (IsActive) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 20, 28, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(70, 28, 38, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(90, 32, 45, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(220, 38, 38, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 120, 120, 255));

        if (InterReg14)
            ImGui::PushFont(InterReg14);
        if (ImGui::Button("Disconnect", ImVec2(95, 32))) {
            Action = ActiveCardAction::Disconnect;
        }
        if (InterReg14)
            ImGui::PopFont();
        ImGui::PopStyleColor(5);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(140, 80, 240, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(160, 95, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(120, 65, 220, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(180, 110, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));

        if (InterReg14)
            ImGui::PushFont(InterReg14);
        if (ImGui::Button("Connect", ImVec2(95, 32))) {
            Action = ActiveCardAction::Connect;
        }
        if (InterReg14)
            ImGui::PopFont();
        ImGui::PopStyleColor(5);
    }

    ImGui::SameLine(0.0f, 8.0f);

    // Settings Button
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(24, 26, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(42, 38, 64, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(56, 48, 86, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(56, 44, 80, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(210, 180, 255, 255));

    if (OmniIconsSmall)
        ImGui::PushFont(OmniIconsSmall);
    if (ImGui::Button(IC_SETTINGS, ImVec2(34, 32))) {
        Action = ActiveCardAction::ConfigureSettings;
    }
    if (OmniIconsSmall)
        ImGui::PopFont();
    ImGui::PopStyleColor(5);

    ImGui::PopStyleVar(2);

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopID();
    ImGui::PopID();

    return Action;
}

TrustedCardAction OmniGUI::TrustedNodeCard(
    DeviceMap DevId,
    const std::array<uint8_t, 32>& Token,
    const OmniInstance* Instance,
    size_t CardIdx,
    ImVec2 CardPos,
    float CardWidth,
    float CardHeight
)
{
    TrustedCardAction Action = TrustedCardAction::None;

    ImGui::PushID("trusted_card");
    ImGui::PushID(static_cast<int>(CardIdx));

    bool IsOnline = Instance && (Instance->LinkState >= NetLinkState::LINKING_WAIT ||
                                 Instance->InstanceIP != 0);
    const char* NodeName =
        (Instance && Instance->InstanceName[0]) ? Instance->InstanceName : "Trusted Node";
    const char* NodeIP =
        (Instance && Instance->IPv4_String[0]) ? Instance->IPv4_String : "192.168.1.100";
    DeviceType DevType = Instance ? Instance->Type : DeviceType::Unknown;

    char Fingerprint[32];
    snprintf(
        Fingerprint,
        sizeof(Fingerprint),
        "%02x:%02x:%02x:%02x:%02x:%02x",
        Token[0],
        Token[1],
        Token[2],
        Token[3],
        Token[4],
        Token[5]
    );

    ImGui::SetCursorScreenPos(CardPos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, COL_BG_CHILD_1);
    ImGui::PushStyleColor(ImGuiCol_Border, IsOnline ? IM_COL32(168, 85, 247, 255) : COL_BORDER);

    ImGui::BeginChild(
        "TrustedNode",
        ImVec2(CardWidth, CardHeight),
        true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImVec2 CardInnerPos = ImGui::GetCursorScreenPos();
    float AvailableWidth = ImGui::GetContentRegionAvail().x;

    const char* DevIcon = GetDeviceIcon(DevType);

    ImVec2 IconBoxMin = ImVec2(CardInnerPos.x, CardInnerPos.y);
    ImVec2 IconBoxMax = ImVec2(IconBoxMin.x + 40.0f, IconBoxMin.y + 40.0f);
    ImU32 IconBg = IsOnline ? COL_FEAT_IC_ACTIVE : COL_BG_CHILD_2;
    DrawList->AddRectFilled(IconBoxMin, IconBoxMax, IconBg, 8.0f);

    if (OmniIconsMedium)
        ImGui::PushFont(OmniIconsMedium);
    ImVec2 IcSize = ImGui::CalcTextSize(DevIcon);
    DrawList->AddText(
        ImVec2(IconBoxMin.x + (40.0f - IcSize.x) * 0.5f, IconBoxMin.y + (40.0f - IcSize.y) * 0.5f),
        IsOnline ? COL_FEAT_TINT_ACT : COL_MENU_TXT_IDLE,
        DevIcon
    );
    if (OmniIconsMedium)
        ImGui::PopFont();

    // Node Name
    ImVec2 TitlePos = ImVec2(IconBoxMax.x + 14.0f, CardInnerPos.y);
    ImGui::SetCursorScreenPos(TitlePos);
    if (InterMed16)
        ImGui::PushFont(InterMed16);
    ImGui::TextColored(COL4_TEXT_ACTIVE, "%s", NodeName);
    if (InterMed16)
        ImGui::PopFont();

    // Status Badge
    const char* StatusText = IsOnline ? "ONLINE" : "OFFLINE";
    ImU32 StatusDotCol = IsOnline ? IM_COL32(46, 213, 115, 255) : IM_COL32(140, 140, 160, 255);
    ImVec2 BadgeSize = ImGui::CalcTextSize(StatusText);
    ImVec2 BadgePos =
        ImVec2(CardInnerPos.x + AvailableWidth - BadgeSize.x - 16.0f, CardInnerPos.y + 2.0f);

    DrawList->AddCircleFilled(
        ImVec2(BadgePos.x - 8.0f, BadgePos.y + BadgeSize.y * 0.5f), 3.5f, StatusDotCol
    );
    DrawList->AddText(BadgePos, StatusDotCol, StatusText);

    // Subtitle & Last Seen
    if (InterReg14)
        ImGui::PushFont(InterReg14);
    ImGui::SetCursorScreenPos(ImVec2(TitlePos.x, TitlePos.y + 20.0f));
    ImGui::TextColored(COL4_TEXT_MUTED, "%s  |  X25519: %s", NodeIP, Fingerprint);

    ImGui::SetCursorScreenPos(ImVec2(CardInnerPos.x, CardInnerPos.y + 50.0f));
    ImGui::TextColored(COL4_TEXT_MUTED, "Last paired: 2026-08-04");
    if (InterReg14)
        ImGui::PopFont();

    float ButtonWidth = IsOnline ? 183.0f : 80.0f;
    ImGui::SetCursorScreenPos(
        ImVec2(CardInnerPos.x + AvailableWidth - ButtonWidth, CardInnerPos.y + 44.0f)
    );

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    if (InterReg14)
        ImGui::PushFont(InterReg14);

    if (IsOnline) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(140, 80, 240, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(160, 95, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(120, 65, 220, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(180, 110, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));

        if (ImGui::Button("Connect", ImVec2(95, 30))) {
            Action = TrustedCardAction::Connect;
        }
        ImGui::PopStyleColor(5);
        ImGui::SameLine(0.0f, 8.0f);
    }

    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(32, 20, 26, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(180, 30, 45, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(220, 38, 38, 255));
    ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(90, 35, 45, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(240, 110, 120, 255));

    if (ImGui::Button("Forget", ImVec2(80, 30))) {
        Action = TrustedCardAction::Forget;
    }
    ImGui::PopStyleColor(5);

    if (InterReg14)
        ImGui::PopFont();
    ImGui::PopStyleVar(2);

    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopID();
    ImGui::PopID();

    return Action;
}
