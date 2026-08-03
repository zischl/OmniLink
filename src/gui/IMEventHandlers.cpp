#include "OmniGUI.h"

// Helper function to render common handshake header, cards, metadata grid, and passkey
void OmniGUI::HandshakeEventHeader(
    ImDrawList* DrawList,
    ImVec2 WindowPos,
    float WindowWidth,
    const char* HeaderTitle,
    const char* DeviceName,
    const char* SubText,
    const char* Key,
    int Port,
    float Timeout
)
{
    // Just a line
    DrawList->AddRectFilled(
        ImVec2(WindowPos.x + 10.0f, WindowPos.y),
        ImVec2(WindowPos.x - 10.0f + WindowWidth, WindowPos.y + 3.0f),
        OmniTheme::COL_HANDSHAKE_ACCENT,
        16.0f,
        ImDrawFlags_RoundCornersTop
    );

    float PaddingTop = 20.0f;
    ImVec2 CircleCenter =
        ImVec2(WindowPos.x + (WindowWidth * 0.5f), WindowPos.y + 24.0f + PaddingTop);
    const float Radius = 13.0f;

    float BoxSize = 48.0f;
    float BoxSpacing = 10.0f;
    float Rounding = 12.0f;
    float BoxTop = CircleCenter.y - (BoxSize * 0.5f);

    ImVec2 LeftRectBegin = ImVec2(CircleCenter.x - BoxSpacing - BoxSize, BoxTop);
    ImVec2 LeftRectEnd = ImVec2(CircleCenter.x - BoxSpacing, BoxTop + BoxSize);
    ImVec2 RightRectBegin = ImVec2(CircleCenter.x + BoxSpacing, BoxTop);
    ImVec2 RightRectEnd = ImVec2(CircleCenter.x + BoxSpacing + BoxSize, BoxTop + BoxSize);

    // Monitor Icon Containers
    DrawList->AddRectFilled(LeftRectBegin, LeftRectEnd, OmniTheme::COL_HANDSHAKE_CARD_BG, Rounding);
    DrawList->AddRect(
        LeftRectBegin, LeftRectEnd, OmniTheme::COL_HANDSHAKE_CARD_BRD, Rounding, 0, 1.2f
    );

    DrawList->AddRectFilled(
        RightRectBegin, RightRectEnd, OmniTheme::COL_HANDSHAKE_CARD_BG, Rounding
    );
    DrawList->AddRect(
        RightRectBegin, RightRectEnd, OmniTheme::COL_HANDSHAKE_CARD_BRD, Rounding, 0, 1.2f
    );

    // AirPlay Icons centered inside each box
    ImGui::PushFont(OmniIconsMedium);
    ImVec2 DevIconSize = ImGui::CalcTextSize(IC_AIRPLAY);
    ImVec2 LeftCenter =
        ImVec2((LeftRectBegin.x + LeftRectEnd.x) * 0.5f, (LeftRectBegin.y + LeftRectEnd.y) * 0.5f);
    ImVec2 RightCenter = ImVec2(
        (RightRectBegin.x + RightRectEnd.x) * 0.5f, (RightRectBegin.y + RightRectEnd.y) * 0.5f
    );

    DrawList->AddText(
        ImVec2(LeftCenter.x - (DevIconSize.x * 0.5f), LeftCenter.y - (DevIconSize.y * 0.5f)),
        OmniTheme::COL_HANDSHAKE_CARD_ICON,
        IC_AIRPLAY
    );
    DrawList->AddText(
        ImVec2(RightCenter.x - (DevIconSize.x * 0.5f), RightCenter.y - (DevIconSize.y * 0.5f)),
        OmniTheme::COL_HANDSHAKE_CARD_ICON,
        IC_AIRPLAY
    );
    ImGui::PopFont();

    // Center Connection Circle
    DrawList->AddCircleFilled(CircleCenter, Radius, OmniTheme::COL_HANDSHAKE_BADGE_BG);
    DrawList->AddCircle(CircleCenter, Radius, OmniTheme::COL_HANDSHAKE_BADGE_BRD, 0, 1.5f);

    ImGui::PushFont(OmniIconsSmall);
    ImVec2 LinkIconSize = ImGui::CalcTextSize(IC_LINK);
    DrawList->AddText(
        ImVec2(CircleCenter.x - (LinkIconSize.x * 0.5f), CircleCenter.y - (LinkIconSize.y * 0.5f)),
        OmniTheme::COL_HANDSHAKE_BADGE_ICON,
        IC_LINK
    );
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(WindowWidth, BoxSize + PaddingTop - 6.0f));

    // Header Label
    ImGui::PushFont(InterMed14);
    float HeaderWidth = ImGui::CalcTextSize(HeaderTitle).x;
    ImGui::SetCursorPosX((WindowWidth - HeaderWidth) * 0.5f);
    ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_TITLE, "%s", HeaderTitle);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    // DeviceName
    ImGui::PushFont(InterBold20);
    float DevNameWidth = ImGui::CalcTextSize(DeviceName).x;
    ImGui::SetCursorPosX((WindowWidth - DevNameWidth) * 0.5f);
    ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_HOSTNAME, "%s", DeviceName);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0.0f, 5.0f));

    // IP Address and.. Subtext
    ImGui::PushFont(InterMed14);
    float SubTextWidth = ImGui::CalcTextSize(SubText).x;
    ImGui::SetCursorPosX((WindowWidth - SubTextWidth) * 0.5f);
    ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_SUBTEXT, "%s", SubText);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0.0f, 12.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    // Metadata Grid
    float ColumnWidth = (WindowWidth - 36.0f) / 3.0f;
    float StartX = 18.0f;

    ImGui::SetCursorPosX(StartX);
    ImGui::BeginGroup();

    // PROTOCOL
    ImGui::BeginGroup();
    ImGui::PushFont(InterMed12);
    const char* ProtocolText = "PROTOCOL";
    ImGui::SetCursorPosX(StartX + (ColumnWidth - ImGui::CalcTextSize(ProtocolText).x) * 0.5f);
    ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_LABEL, "%s", ProtocolText);
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::PushFont(OmniIconsSmall);
    const char* ProtoIcons = IC_AIRPLAY " " IC_MOUSE " " IC_VOLUME_2;
    ImGui::SetCursorPosX(StartX + (ColumnWidth - ImGui::CalcTextSize(ProtoIcons).x) * 0.5f);
    ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_PROTO_ICONS, "%s", ProtoIcons);
    ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::SameLine(StartX + ColumnWidth);

    // PORT
    ImGui::BeginGroup();
    ImGui::PushFont(InterMed12);
    const char* PortText = "PORT";
    ImGui::SetCursorPosX(
        StartX + ColumnWidth + (ColumnWidth - ImGui::CalcTextSize(PortText).x) * 0.5f
    );
    ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_LABEL, "%s", PortText);
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::PushFont(InterMed15);
    char PortBuffer[16];
    snprintf(PortBuffer, sizeof(PortBuffer), "%d", Port);
    ImGui::SetCursorPosX(
        StartX + ColumnWidth + (ColumnWidth - ImGui::CalcTextSize(PortBuffer).x) * 0.5f
    );
    ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_VALUE, "%s", PortBuffer);
    ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::SameLine(StartX + (ColumnWidth * 2.0f));

    // TIMEOUT
    ImGui::BeginGroup();
    ImGui::PushFont(InterMed12);
    const char* TimeoutText = "TIMEOUT";
    ImGui::SetCursorPosX(
        StartX + (ColumnWidth * 2.0f) + (ColumnWidth - ImGui::CalcTextSize(TimeoutText).x) * 0.5f
    );
    ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_LABEL, "%s", TimeoutText);
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::PushFont(InterMed15);
    char TimeoutBuffer[16];
    int RemSec = (int)(Timeout > 0.0f ? Timeout : 0.0f);
    snprintf(TimeoutBuffer, sizeof(TimeoutBuffer), "%ds", RemSec);
    ImGui::SetCursorPosX(
        StartX + (ColumnWidth * 2.0f) + (ColumnWidth - ImGui::CalcTextSize(TimeoutBuffer).x) * 0.5f
    );
    ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_VALUE, "%s", TimeoutBuffer);
    ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::EndGroup();

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    // 6 Digit Code
    char Passkey[12]{};
    snprintf(
        Passkey,
        sizeof(Passkey),
        "%c%c%c - %c%c%c",
        Key[0] ? Key[0] : '0',
        Key[1] ? Key[1] : '0',
        Key[2] ? Key[2] : '0',
        Key[3] ? Key[3] : '0',
        Key[4] ? Key[4] : '0',
        Key[5] ? Key[5] : '0'
    );

    const char* CodeText = "VERIFICATION CODE";
    ImGui::PushFont(InterMed12);
    float CodeTextWidth = ImGui::CalcTextSize(CodeText).x;
    ImGui::SetCursorPosX((WindowWidth - CodeTextWidth) * 0.5f);
    ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_LABEL, "%s", CodeText);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    ImGui::PushFont(JetBrainsBold20);
    float CodeWidth = ImGui::CalcTextSize(Passkey).x;
    ImGui::SetCursorPosX((WindowWidth - CodeWidth) * 0.5f);
    ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_CODE, "%s", Passkey);
    ImGui::PopFont();
}

bool OmniGUI::HandleEvent(HandshakeWaitEvent& Request, float Timeout)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImVec2 WindowPos = ImGui::GetWindowPos();
    float WindowWidth = ImGui::GetWindowWidth();

    const char* DeviceName =
        Request.InstanceName[0] != '\0' ? Request.InstanceName : "Target Device";
    char SubText[64];
    const char* IpStr = "192.168.1.x · Local Network";
    if (AvailableInstances) {
        auto It = AvailableInstances->find(Request.DeviceID);
        if (It != AvailableInstances->end() && It->second.IPv4_String[0] != '\0') {
            snprintf(SubText, sizeof(SubText), "%s · Local Network", It->second.IPv4_String);
            IpStr = SubText;
        }
    }

    HandshakeEventHeader(
        DrawList,
        WindowPos,
        WindowWidth,
        "CONNECTION REQUEST",
        DeviceName,
        IpStr,
        Request.VerificationCode,
        ConfigPort,
        Timeout
    );

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    // Cancel Handshake
    bool CancelState = false;
    float AvailableWidth = ImGui::GetContentRegionAvail().x;
    float ActionHeight = 42.0f;
    float ButtonWidth = AvailableWidth * 0.7f;

    ImGui::PushFont(InterMed16);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, OmniTheme::COL4_BTN_DECLINE_BG);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, OmniTheme::COL4_BTN_DECLINE_BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, OmniTheme::COL4_BTN_DECLINE_BG_ACTIVE);
    ImGui::PushStyleColor(ImGuiCol_Border, OmniTheme::COL4_BTN_DECLINE_BRD);
    ImGui::PushStyleColor(ImGuiCol_Text, OmniTheme::COL4_BTN_DECLINE_TXT);

    ImGui::SetCursorPosX((WindowWidth - ButtonWidth) * 0.5f);
    if (ImGui::Button("Cancel Handshake", ImVec2(ButtonWidth, ActionHeight))) {
        ImGui::CloseCurrentPopup();
        OmniAPI::CancelHandshake(Request.DeviceID);
        CancelState = true;
    }

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::PopStyleVar(2);

    return CancelState;
}

bool OmniGUI::HandleEvent(HandshakeConfirmEvent& Request, float Timeout)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImVec2 WindowPos = ImGui::GetWindowPos();
    float WindowWidth = ImGui::GetWindowWidth();

    const char* DeviceName =
        Request.InstanceName[0] != '\0' ? Request.InstanceName : "Target Device";
    char SubText[64];
    const char* IpStr = "192.168.1.x · Local Network";
    if (AvailableInstances) {
        auto It = AvailableInstances->find(Request.DeviceID);
        if (It != AvailableInstances->end() && It->second.IPv4_String[0] != '\0') {
            snprintf(SubText, sizeof(SubText), "%s · Local Network", It->second.IPv4_String);
            IpStr = SubText;
        }
    }

    HandshakeEventHeader(
        DrawList,
        WindowPos,
        WindowWidth,
        "CONNECTION REQUEST",
        DeviceName,
        IpStr,
        Request.VerificationCode,
        ConfigPort,
        Timeout
    );

    ImGui::Dummy(ImVec2(0.0f, 12.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    // Custom Trust Checkbox or should i say.. check line
    float RowHeight = 20.0f;
    ImVec2 ScreenPos = ImGui::GetCursorScreenPos();
    ScreenPos.x += 22.0f;

    float BoxSquareSize = 18.0f;
    float BoxTopY = ScreenPos.y + (RowHeight - BoxSquareSize) * 0.5f;

    ImVec2 BoxMin = ImVec2(ScreenPos.x, BoxTopY);
    ImVec2 BoxMax = ImVec2(ScreenPos.x + BoxSquareSize, BoxTopY + BoxSquareSize);

    float TotalRowWidth = 260.0f;
    ImGui::SetCursorPosX(22.0f);
    if (ImGui::InvisibleButton("##TrustCheckBtn", ImVec2(TotalRowWidth, RowHeight))) {
        Request.Trusted = !Request.Trusted;
    }

    bool HoverState = ImGui::IsItemHovered();

    // The actual checkbox
    ImU32 FrameBgCol =
        HoverState ? OmniTheme::COL_HANDSHAKE_CHECK_BG_HOVER : OmniTheme::COL_HANDSHAKE_CHECK_BG;
    ImU32 BorderCol = OmniTheme::COL_HANDSHAKE_CHECK_BRD;
    DrawList->AddRectFilled(BoxMin, BoxMax, FrameBgCol, 4.0f);
    DrawList->AddRect(BoxMin, BoxMax, BorderCol, 4.0f, 0, 1.2f);

    // Draw Checkmark if Checked
    if (Request.Trusted) {
        ImU32 CheckCol = OmniTheme::COL_HANDSHAKE_CHECKMARK;
        DrawList->AddLine(
            ImVec2(BoxMin.x + 4.5f, BoxMin.y + 9.5f),
            ImVec2(BoxMin.x + 7.5f, BoxMin.y + 13.0f),
            CheckCol,
            2.0f
        );
        DrawList->AddLine(
            ImVec2(BoxMin.x + 7.5f, BoxMin.y + 13.0f),
            ImVec2(BoxMin.x + 13.5f, BoxMin.y + 5.5f),
            CheckCol,
            2.0f
        );
    }

    // Shield Icon
    ImGui::PushFont(OmniIconsSmall);
    ImVec2 ShieldSize = ImGui::CalcTextSize(IC_SHIELD);
    float ShieldX = BoxMax.x + 10.0f;
    float ShieldY = ScreenPos.y + (RowHeight - ShieldSize.y) * 0.5f;
    DrawList->AddText(ImVec2(ShieldX, ShieldY), OmniTheme::COL_HANDSHAKE_TRUST_LABEL, IC_SHIELD);
    ImGui::PopFont();

    // Trust me for eternity
    ImGui::PushFont(InterMed15);
    ImVec2 LabelSize = ImGui::CalcTextSize("Trust this device permanently");
    float LabelX = ShieldX + ShieldSize.x + 8.0f;
    float LabelY = ScreenPos.y + (RowHeight - LabelSize.y) * 0.5f;
    DrawList->AddText(
        ImVec2(LabelX, LabelY),
        OmniTheme::COL_HANDSHAKE_TRUST_LABEL,
        "Trust this device permanently"
    );
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    // Decline or.. Accept
    bool ActionState = false;
    float Spacing = 14.0f;
    float AvailableWidth = ImGui::GetContentRegionAvail().x;
    float ButtonWidth = (AvailableWidth - Spacing) * 0.5f;
    float ActionHeight = 42.0f;

    ImGui::PushFont(InterMed16);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    // Decline Button
    ImGui::PushStyleColor(ImGuiCol_Button, OmniTheme::COL4_BTN_DECLINE_BG);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, OmniTheme::COL4_BTN_DECLINE_BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, OmniTheme::COL4_BTN_DECLINE_BG_ACTIVE);
    ImGui::PushStyleColor(ImGuiCol_Border, OmniTheme::COL4_BTN_DECLINE_BRD);
    ImGui::PushStyleColor(ImGuiCol_Text, OmniTheme::COL4_BTN_DECLINE_TXT);

    if (ImGui::Button("Decline", ImVec2(ButtonWidth, ActionHeight))) {
        ImGui::CloseCurrentPopup();
        OmniAPI::RejectHandshake(Request.DeviceID);
        ActionState = true;
    }

    ImGui::PopStyleColor(5);

    ImGui::SameLine(0.0f, Spacing);

    // Accept Button
    ImGui::PushStyleColor(ImGuiCol_Button, OmniTheme::COL4_BTN_ACCEPT_BG);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, OmniTheme::COL4_BTN_ACCEPT_BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, OmniTheme::COL4_BTN_ACCEPT_BG_ACTIVE);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, OmniTheme::COL4_BTN_ACCEPT_TXT);

    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere(-1);
    }

    if (ImGui::Button("Accept", ImVec2(ButtonWidth, ActionHeight))) {
        ImGui::CloseCurrentPopup();
        OmniAPI::AcceptHandshake(Request.DeviceID, Request.Trusted);
        ActionState = true;
    }

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::PopStyleVar(2);

    return ActionState;
}

bool OmniGUI::HandleEvent(Alert& Request, float Timeout)
{
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImVec2 WinPos = ImGui::GetWindowPos();
    ImVec2 WinSize = ImGui::GetWindowSize();

    // Icon Box
    ImVec2 IconBoxMin = ImVec2(WinPos.x + 10.0f, WinPos.y + (WinSize.y - 36.0f) * 0.5f);
    ImVec2 IconBoxMax = ImVec2(IconBoxMin.x + 36.0f, IconBoxMin.y + 36.0f);
    DrawList->AddRectFilled(IconBoxMin, IconBoxMax, COL_FEAT_IC_ACTIVE, 8.0f);

    ImGui::PushFont(OmniIconsMedium);
    ImVec2 IcSize = ImGui::CalcTextSize(IC_ZAP);
    DrawList->AddText(
        ImVec2(IconBoxMin.x + (36.0f - IcSize.x) * 0.5f, IconBoxMin.y + (36.0f - IcSize.y) * 0.5f),
        COL_FEAT_TINT_ACT,
        IC_ZAP
    );
    ImGui::PopFont();

    // Da Text
    float TextStartX = IconBoxMax.x + 12.0f;
    float ButtonWidth = 40.0f;
    float TextWidth = WinSize.x - (TextStartX - WinPos.x) - ButtonWidth - 10.0f;

    ImGui::SetCursorScreenPos(ImVec2(TextStartX, WinPos.y + 12.0f));
    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(TextStartX + TextWidth);

    ImGui::PushFont(InterMed14);
    ImGui::TextColored(COL4_TEXT_ACTIVE, "%s", Request.Title);
    ImGui::PopFont();

    ImGui::PushFont(InterReg14);
    ImGui::TextColored(COL4_TEXT_MUTED, "%s", Request.Desc);
    ImGui::PopFont();

    ImGui::PopTextWrapPos();
    ImGui::EndGroup();

    // Dismiss Button
    ImVec2 ButtonMin = ImVec2(WinPos.x + WinSize.x - ButtonWidth, WinPos.y);
    ImVec2 ButtonMax = ImVec2(WinPos.x + WinSize.x, WinPos.y + WinSize.y);

    ImVec2 SavedCursorPos = ImGui::GetCursorPos();
    ImGui::SetCursorScreenPos(ButtonMin);
    bool Dismissed = ImGui::InvisibleButton("##AlertDismissBtn", ImVec2(ButtonWidth, WinSize.y));
    ImGui::SetCursorPos(SavedCursorPos);

    bool Hovered = ImGui::IsItemHovered();
    bool Active = ImGui::IsItemActive();

    if (Hovered || Active) {
        ImU32 HoverCol = ImGui::GetColorU32(COL4_BTN_HOVER_DARK);
        DrawList->AddRectFilled(
            ButtonMin, ButtonMax, HoverCol, 10.0f, ImDrawFlags_RoundCornersRight
        );
    }

    ImGui::PushFont(OmniIconsSmall);
    ImVec2 XSize = ImGui::CalcTextSize(IC_X);
    ImVec2 XPos = ImVec2(
        ButtonMin.x + (ButtonWidth - XSize.x) * 0.5f, ButtonMin.y + (WinSize.y - XSize.y) * 0.5f
    );
    DrawList->AddText(
        XPos,
        Hovered ? ImGui::GetColorU32(COL4_TEXT_ACTIVE) : ImGui::GetColorU32(COL4_TEXT_MUTED),
        IC_X
    );
    ImGui::PopFont();

    return Dismissed;
}
