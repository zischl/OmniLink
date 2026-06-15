#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include "OmniEnums.h"
#include "OmniInstances.h"
#include "OmniPackets.h"
#include "imgui.h"
#endif

#include "OmniGUI.h"
#include "OmniLink.h"
#include "fonts.h"

#include "DummyInstances.h"
#include "OmniIcons.h"

OmniGUI::OmniGUI(OmniLink& OmniLinkInstance)
    : App(OmniLinkInstance), SelectedDevice(OmniLink::SelectedTargetDevice)
{
    AvailableInstances = &DummyAvailableInstances;
    ActiveInstances = App.GetActiveInstances();
}

void OmniGUI::SetupImGui(HWND hwnd, ID3D11Device* D3D11Device, ID3D11DeviceContext* D3D11Context)
{

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using
    // Docking Branch

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(D3D11Device, D3D11Context);

    ImGuiStyle& style = ImGui::GetStyle();

    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.65f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.031f, 0.031f, 0.059f, 1.0f);

    ImFontConfig FontCFG;
    FontCFG.FontDataOwnedByAtlas = false;

    JetBrainsReg20 = io.Fonts->AddFontFromMemoryTTF(
        JetBrainsMonoRegular, JetBrainsMonoRegular_Size, 20.0f, &FontCFG);
    JetBrainsReg18 = io.Fonts->AddFontFromMemoryTTF(
        JetBrainsMonoRegular, JetBrainsMonoRegular_Size, 18.0f, &FontCFG);

    // Range for OmniIconsSmall: Contains only 'x' and 'minus'
    static const ImWchar OmniSmallIconRange[] = {61459, 61460, 0};

    // Range for OmniIcons: Contains everything else
    static const ImWchar OmniLargeIconRange[] = {61440, 61458, 0};

    FontCFG.GlyphRanges = OmniLargeIconRange;
    OmniIcons = io.Fonts->AddFontFromMemoryTTF(OmniIcons_ttf, OmniIcons_ttf_len, 20.0f, &FontCFG);

    FontCFG.GlyphRanges = OmniSmallIconRange;
    OmniIconsSmall =
        io.Fonts->AddFontFromMemoryTTF(OmniIcons_ttf, OmniIcons_ttf_len, 14.0f, &FontCFG);
}

bool OmniGUI::VerticalMenuItem(const char* Label,
                               const char* Icon,
                               bool State,
                               ImVec2& MenuItemSize)
{
    ImGui::PushID(Label);

    const ImVec2 IconSize = ImVec2(55, 55);
    const float IconBgRounding = 16.0f;

    ImVec2 Pos = ImGui::GetCursorScreenPos();
    bool Clicked = ImGui::InvisibleButton(Label, MenuItemSize);
    bool Hovered = ImGui::IsItemHovered();

    ImU32 BgColor = IM_COL32(30, 30, 40, 0);
    ImU32 IconBgColor = IM_COL32(30, 30, 40, 255);

    ImVec2 IconBoxMin = ImVec2(Pos.x + (MenuItemSize.x - IconSize.x) * 0.5f, Pos.y + 10);
    ImVec2 IconBoxMax = ImVec2(IconBoxMin.x + IconSize.x, IconBoxMin.y + IconSize.y);

    if (State) {
        ImVec2 IndicatorTop = ImVec2(Pos.x, Pos.y + (MenuItemSize.y * 0.30f));
        ImVec2 IndicatorBottom = ImVec2(Pos.x + 5, Pos.y + (MenuItemSize.y * 0.70f));

        BgColor = IM_COL32(168, 85, 247, 40);
        IconBgColor = IM_COL32(168, 85, 247, 20);

        // Active Item Strip
        DrawList->AddRectFilled(IndicatorTop, IndicatorBottom, IM_COL32(168, 85, 247, 255), 10.0f);
    } else if (Hovered) {
        BgColor = IM_COL32(255, 255, 255, 15);
    }

    ImVec2 TotalBoxMax = ImVec2(Pos.x + MenuItemSize.x, Pos.y + MenuItemSize.y);
    // Item BG
    DrawList->AddRectFilled(Pos, TotalBoxMax, BgColor, 4.0f);
    // Item Icon BG
    DrawList->AddRectFilled(IconBoxMin, IconBoxMax, IconBgColor, IconBgRounding);

    // Item Icon
    ImU32 IconTint = State ? IM_COL32(168, 85, 247, 255) : IM_COL32(140, 140, 160, 255);
    ImGui::PushFont(OmniIcons);
    ImVec2 GlyphSize = ImGui::CalcTextSize(Icon);
    ImVec2 GlyphPos = ImVec2(IconBoxMin.x + (IconSize.x - GlyphSize.x) * 0.5f,
                             IconBoxMin.y + (IconSize.y - GlyphSize.y) * 0.5f);

    DrawList->AddText(GlyphPos, IconTint, Icon);
    ImGui::PopFont();

    ImVec2 LabelSize = ImGui::CalcTextSize(Label);
    ImVec2 LabelPos = ImVec2(Pos.x + (MenuItemSize.x - LabelSize.x) * 0.5f, IconBoxMax.y + 10);
    ImU32 TextColor = State ? IM_COL32(168, 85, 247, 255) : IM_COL32(100, 105, 125, 255);

    // Item Tex
    DrawList->AddText(LabelPos, TextColor, Label);

    ImGui::PopID();
    return Clicked;
}

bool OmniGUI::IconizedButton(const char* Label,
                             const char* Icon,
                             bool State,
                             const ImVec2& ButtonSize)
{
    ImGui::PushID(Label);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    bool clicked = ImGui::InvisibleButton(Label, ButtonSize);
    bool hovered = ImGui::IsItemHovered();

    // Button BG
    if (State) {
        DrawList->AddRectFilled(
            pos, ImVec2(pos.x + ButtonSize.x, pos.y + ButtonSize.y), IM_COL32(157, 78, 221, 30));
    } else if (hovered) {
        DrawList->AddRectFilled(
            pos, ImVec2(pos.x + ButtonSize.x, pos.y + ButtonSize.y), IM_COL32(255, 255, 255, 10));
    }

    const ImVec2 IconSize = ImVec2(44.0f, 44.0f);
    ImVec2 IconBoxPos = ImVec2(pos.x + (ButtonSize.x - IconSize.x) * 0.5f, pos.y + 14.0f);

    ImU32 IconBgColor = State ? IM_COL32(157, 78, 221, 35) : IM_COL32(255, 255, 255, 10);
    // Icon BG
    DrawList->AddRectFilled(IconBoxPos,
                            ImVec2(IconBoxPos.x + IconSize.x, IconBoxPos.y + IconSize.y),
                            IconBgColor,
                            12.0f);

    // Da Icon
    ImGui::PushFont(OmniIcons);
    ImVec2 IconTextSize = ImGui::CalcTextSize(Icon);
    ImVec2 IconTextPos = ImVec2(IconBoxPos.x + (IconSize.x - IconTextSize.x) * 0.5f,
                                IconBoxPos.y + (IconSize.y - IconTextSize.y) * 0.5f);
    ImU32 IconColor = State ? IM_COL32(157, 78, 221, 255) : IM_COL32(160, 160, 170, 255);

    DrawList->AddText(IconTextPos, IconColor, Icon);
    ImGui::PopFont();

    ImVec2 LabelSize = ImGui::CalcTextSize(Label);
    ImVec2 LabelPos =
        ImVec2(pos.x + (ButtonSize.x - LabelSize.x) * 0.5f, IconBoxPos.y + IconSize.y + 12.0f);
    ImU32 LabelColor = State ? IM_COL32(230, 230, 255, 255) : IM_COL32(150, 150, 160, 255);
    // Button Label
    DrawList->AddText(LabelPos, LabelColor, Label);

    if (State) {
        const float StripWidth = 40.0f;
        const float StripHeight = 3.0f;
        ImVec2 StripPosLeft =
            ImVec2(pos.x + (ButtonSize.x - StripWidth) * 0.5f, pos.y + ButtonSize.y - StripHeight);
        ImVec2 stripPosRight = ImVec2(StripPosLeft.x + StripWidth, pos.y + ButtonSize.y);

        DrawList->AddRectFilled(StripPosLeft,
                                stripPosRight,
                                IM_COL32(157, 78, 221, 255),
                                1.5f,
                                ImDrawFlags_RoundCornersTop);

        DrawList->AddRectFilledMultiColor(ImVec2(StripPosLeft.x, StripPosLeft.y - 8),
                                          stripPosRight,
                                          IM_COL32(157, 78, 221, 0),
                                          IM_COL32(157, 78, 221, 0),
                                          IM_COL32(157, 78, 221, 45),
                                          IM_COL32(157, 78, 221, 45));
    }

    ImGui::PopID();
    return clicked;
}

void OmniGUI::CenterItemX(const float ItemWidth)
{
    float Space = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((Space - ItemWidth) * 0.5f);
}

void OmniGUI::ConnectionRing(const char* Label)
{
    int Radius = 205;

    ImVec2 WidgetSize = ImVec2((Radius * 2.0f) + 80.0f, (Radius * 2.0f) + 80.0f);
    ImGui::Dummy(WidgetSize);

    ImGui::PushID(Label);

    ImVec2 ItemPos = ImGui::GetItemRectMin();
    ImVec2 ItemSize = ImGui::GetItemRectSize();
    ImVec2 Pos = ImVec2(ItemPos.x + (ItemSize.x * 0.5f), ItemPos.y + (ItemSize.y * 0.5f));

    ImU32 Col = IM_COL32(128, 0, 255, 255);

    ImVec2 Space = ImGui::GetContentRegionAvail();
    ImVec2 CPos = ImGui::GetCursorScreenPos();

    // ImGui::GetWindowDrawList()->AddCircle(Pos, 205.0f, Col, 64, 3.0f);

    ImGui::PopID();

    // uh.. the math if i forget , 205 * sin(45 degrees) = 144.95
    int DiagonalAxe = static_cast<int>(Radius * 0.707106f);

    ImVec2 TextSize = ImGui::CalcTextSize("192.168.1.255");

    ImGui::PushFont(JetBrainsReg18);

    auto& Devices = *AvailableInstances;

    // Center Device
    DeviceIcon("C0", Pos, TextSize, &Devices[DeviceMap::C0]);

    // Left
    if (Devices[DeviceMap::L1].InstanceIP) {
        DeviceIcon("L1", ImVec2(Pos.x - Radius, Pos.y), TextSize, &Devices[DeviceMap::L1]);
    }

    // Left-Up
    if (Devices[DeviceMap::LU1].InstanceIP) {
        DeviceIcon("LU1",
                   ImVec2(Pos.x - DiagonalAxe, Pos.y - DiagonalAxe),
                   TextSize,
                   &Devices[DeviceMap::LU1]);
    }

    // Up
    if (Devices[DeviceMap::U1].InstanceIP) {
        DeviceIcon("U1", ImVec2(Pos.x, Pos.y - Radius), TextSize, &Devices[DeviceMap::U1]);
    }

    // Right-Up
    if (Devices[DeviceMap::RU1].InstanceIP) {
        DeviceIcon("RU1",
                   ImVec2(Pos.x + DiagonalAxe, Pos.y - DiagonalAxe),
                   TextSize,
                   &Devices[DeviceMap::RU1]);
    }

    // Right
    if (Devices[DeviceMap::R1].InstanceIP) {
        DeviceIcon("R1", ImVec2(Pos.x + Radius, Pos.y), TextSize, &Devices[DeviceMap::R1]);
    }

    // Right-Down
    if (Devices[DeviceMap::RD1].InstanceIP) {
        DeviceIcon("RD1",
                   ImVec2(Pos.x + DiagonalAxe, Pos.y + DiagonalAxe),
                   TextSize,
                   &Devices[DeviceMap::RD1]);
    }

    // Down
    if (Devices[DeviceMap::D1].InstanceIP) {
        DeviceIcon("D1", ImVec2(Pos.x, Pos.y + Radius), TextSize, &Devices[DeviceMap::D1]);
    }

    // Left-Down
    if (Devices[DeviceMap::LD1].InstanceIP) {
        DeviceIcon("LD1",
                   ImVec2(Pos.x - DiagonalAxe, Pos.y + DiagonalAxe),
                   TextSize,
                   &Devices[DeviceMap::LD1]);
    }

    ImGui::PopFont();
}

void OmniGUI::DrawMetricDashboard(
    const char* ContainerId, const MetricItem* Items, int ItemCount, float TotalWidth, float Height)
{
    if (ItemCount <= 0 || !Items)
        return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    if (ImGui::BeginChild(ContainerId, ImVec2(TotalWidth, Height), ImGuiChildFlags_None)) {

        float Spacing = 12.0f;
        float CardWidth = (TotalWidth - (Spacing * (ItemCount - 1))) / ItemCount;

        ImU32 ContainerBgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.07f, 0.05f, 0.10f, 1.00f));
        ImU32 ContainerBorderColor =
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.18f, 0.12f, 0.25f, 1.00f));
        ImVec4 TitleColor = ImVec4(0.45f, 0.42f, 0.55f, 1.00f);
        ImVec4 ValueColor = ImVec4(0.70f, 0.45f, 1.00f, 1.00f);

        for (int i = 0; i < ItemCount; ++i) {
            ImVec2 PMin = ImGui::GetCursorScreenPos();
            ImVec2 PMax = ImVec2(PMin.x + CardWidth, PMin.y + Height);

            DrawList->AddRectFilled(PMin, PMax, ContainerBgColor, 8.0f);
            DrawList->AddRect(PMin, PMax, ContainerBorderColor, 8.0f, 0, 1.0f);

            char ChildLabel[64];
            ImFormatString(ChildLabel, IM_ARRAYSIZE(ChildLabel), "card_inner_%d", i);

            ImGui::SetCursorScreenPos(PMin);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));

            if (ImGui::BeginChild(ChildLabel,
                                  ImVec2(CardWidth, Height),
                                  ImGuiChildFlags_None,
                                  ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar)) {
                // Title
                ImGui::PushStyleColor(ImGuiCol_Text, TitleColor);
                ImGui::TextUnformatted(Items[i].title);
                ImGui::PopStyleColor();

                ImGui::Spacing();

                // Value
                ImGui::PushStyleColor(ImGuiCol_Text, ValueColor);
                ImGui::TextUnformatted(Items[i].value);
                ImGui::PopStyleColor();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();

            if (i < ItemCount - 1) {
                ImGui::SetCursorScreenPos(ImVec2(PMin.x + CardWidth + Spacing, PMin.y));
            }
        }
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(1);
}

bool OmniGUI::HandleEvent(ConnectionRequest& request, float timeout)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    ImVec4 textMuted = ImVec4(0.45f, 0.47f, 0.57f, 1.0f);
    ImVec4 purpleAccent = ImVec4(0.53f, 0.44f, 0.96f, 1.0f);
    ImVec4 purpleHover = ImVec4(0.60f, 0.52f, 0.98f, 1.0f);
    ImVec4 purpleActive = ImVec4(0.45f, 0.36f, 0.88f, 1.0f);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    float windowWidth = ImGui::GetWindowWidth();

    drawList->AddRectFilled(ImVec2(windowPos.x + 10.0f, windowPos.y),
                            ImVec2(windowPos.x - 10.0f + windowWidth, windowPos.y + 3.0f),
                            IM_COL32(135, 112, 245, 255),
                            16.0f,
                            ImDrawFlags_RoundCornersTop);

    float paddingTop = 21.0f;
    ImVec2 circleCenter =
        ImVec2(windowPos.x + (windowWidth / 2.0f), windowPos.y + 25.0f + paddingTop);
    const float radius = 14.0f;

    float boxSize = 48.0f;
    float boxSpacing = 18.0f;
    float rounding = 12.0f;
    float boxTop = circleCenter.y - (boxSize / 2.0f);
    float botBottom = boxTop + boxSize;

    ImVec2 leftRectBegin = ImVec2(circleCenter.x - boxSpacing - boxSize, boxTop);
    ImVec2 leftRectEnd = ImVec2(circleCenter.x - boxSpacing, botBottom);

    ImVec2 rightRectBegin = ImVec2(circleCenter.x + boxSpacing, boxTop);
    ImVec2 rightRectEnd = ImVec2(circleCenter.x + boxSpacing + boxSize, botBottom);

    drawList->AddRectFilled(leftRectBegin, leftRectEnd, IM_COL32(40, 40, 55, 100), rounding);
    drawList->AddRect(leftRectBegin, leftRectEnd, IM_COL32(75, 70, 105, 255), rounding, 0, 1.5f);

    drawList->AddRectFilled(rightRectBegin, rightRectEnd, IM_COL32(40, 40, 55, 100), rounding);
    drawList->AddRect(rightRectBegin, rightRectEnd, IM_COL32(75, 70, 105, 255), rounding, 0, 1.5f);

    drawList->AddCircleFilled(circleCenter, radius, IM_COL32(23, 23, 30, 255));
    drawList->AddCircle(circleCenter, radius, IM_COL32(50, 50, 65, 255), 0, 1.5f);

    const char* arrowText = "⇄";
    ImGui::SetWindowFontScale(11.0f / 18.0f);
    ImVec2 arrowSize = ImGui::CalcTextSize(arrowText);
    drawList->AddText(
        ImVec2(circleCenter.x - (arrowSize.x / 2.0f), circleCenter.y - (arrowSize.y / 2.0f)),
        IM_COL32_WHITE,
        arrowText);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Dummy(ImVec2(windowWidth, boxSize + paddingTop + 14.0f));

    const char* txtConnReq = "CONNECTION REQUEST";
    ImGui::SetWindowFontScale(11.0f / 18.0f);

    float txtConnReqWidth = ImGui::CalcTextSize(txtConnReq).x;
    ImGui::SetCursorPosX((windowWidth - txtConnReqWidth) / 2.0f);

    ImGui::TextColored(textMuted, "%s", txtConnReq);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    const char* deviceName = "DESKTOP-7K3MX2";
    ImGui::SetWindowFontScale(17.0f / 18.0f);

    float deviceNameWidth = ImGui::CalcTextSize(deviceName).x;
    ImGui::SetCursorPosX((windowWidth - deviceNameWidth) / 2.0f);

    ImGui::TextUnformatted(deviceName);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    const char* ipSub = "192.168.1.42  Local Network";
    ImGui::SetWindowFontScale(12.0f / 18.0f);

    float ipSubWidth = ImGui::CalcTextSize(ipSub).x;
    ImGui::SetCursorPosX((windowWidth - ipSubWidth) / 2.0f);

    ImGui::TextColored(textMuted, "%s", ipSub);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Dummy(ImVec2(0.0f, 14.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    if (ImGui::BeginTable("meta_info_row", 3, ImGuiTableFlags_NoBordersInBody)) {
        ImGui::TableSetupColumn("C1", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("C2", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("C3", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::SetWindowFontScale(10.0f / 18.0f);

        ImGui::TableSetColumnIndex(0);
        ImGui::SetCursorPosX(
            ImGui::GetCursorPosX() +
            (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("PROTOCOL").x) * 0.5f);
        ImGui::TextColored(textMuted, "PROTOCOL");

        ImGui::TableSetColumnIndex(1);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("PORT").x) *
                                 0.5f);
        ImGui::TextColored(textMuted, "PORT");

        ImGui::TableSetColumnIndex(2);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("LATENCY").x) *
                                 0.5f);
        ImGui::TextColored(textMuted, "LATENCY");

        ImGui::TableNextRow();
        ImGui::SetWindowFontScale(13.0f / 18.0f);

        ImGui::TableSetColumnIndex(0);
        ImGui::SetCursorPosX(
            ImGui::GetCursorPosX() +
            (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("[V] [M] [A]").x) * 0.5f);
        ImGui::TextColored(purpleAccent, "[V] [M] [A]");

        ImGui::TableSetColumnIndex(1);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("7474").x) *
                                 0.5f);
        ImGui::TextUnformatted("7474");

        ImGui::TableSetColumnIndex(2);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("4 ms").x) *
                                 0.5f);
        ImGui::TextUnformatted("4 ms");

        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0.0f, 14.0f));
    ImGui::Separator();

    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.16f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, purpleAccent);

    ImGui::SetWindowFontScale(12.5f / 18.0f);
    ImGui::Checkbox("🛡 Trust this device permanently", &request.Verified);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::PopStyleColor(3);
    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    bool actionState = false;
    float Spacing = 24.0f;
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float individualButtonWidth = (availableWidth - Spacing) / 2.0f;
    float actionHeight = 40.0f;

    ImGui::SetWindowFontScale(13.0f / 18.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.14f, 0.17f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.10f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.25f, 0.25f, 0.32f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    if (ImGui::Button("Decline", ImVec2(individualButtonWidth, actionHeight))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    ImGui::SameLine(0.0f, Spacing);

    ImGui::PushStyleColor(ImGuiCol_Button, purpleAccent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, purpleHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, purpleActive);

    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere(-1);
    }

    if (ImGui::Button("Accept", ImVec2(individualButtonWidth, actionHeight))) {
        ImGui::CloseCurrentPopup();
        actionState = true;
    }
    ImGui::PopStyleColor(3);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Dummy(ImVec2(0.0f, 24.0f));
    ImGui::PopStyleVar(2);

    return actionState;
}

bool OmniGUI::HandleEvent(Alert& request, float timeout)
{
    return true;
}

void OmniGUI::CreateCurvedLine(const char* label, int curve)
{
    ImGui::PushID(label);

    ImVec2 pos = ImGui::GetCursorScreenPos();

    int length = 150;

    ImVec2 p0 = ImVec2(pos.x, pos.y);
    ImVec2 p1 = ImVec2(pos.x, pos.y + length);
    ImVec2 cp0 = ImVec2(pos.x - curve, pos.y + curve);
    ImVec2 cp1 = ImVec2(pos.x - curve, pos.y + length - curve);

    ImU32 color = ImGui::GetColorU32(ImVec4(0.5f, 0.0f, 1.0f, 1.0f));
    float thickness = 1.0f;
    int segments = 20;

    DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness, segments);

    int glow_range = 1;

    color = ImGui::GetColorU32(ImVec4(0.5f, 0.0f, 1.0f, 0.6f));
    DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness + glow_range, segments);

    color = ImGui::GetColorU32(ImVec4(0.5f, 0.0f, 1.0f, 0.3f));
    DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness + glow_range * 2, segments);

    color = ImGui::GetColorU32(ImVec4(0.5f, 0.0f, 1.0f, 0.1f));
    DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness + glow_range * 2.5, segments);

    ImGui::PopID();
}
