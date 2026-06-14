#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include "OmniPackets.h"
#include "imgui.h"
#endif

#include "OmniGUI.h"
#include "OmniLink.h"
#include "fonts.h"

#include "OmniIcons.h"

OmniGUI::OmniGUI(OmniLink& OmniLinkInstance)
    : App(OmniLinkInstance), SelectedDevice(OmniLink::SelectedTargetDevice)
{
    AvailableInstances = App.GetAvailableInstances();
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

    OmniIcons = io.Fonts->AddFontFromMemoryTTF(OmniIcons_ttf, OmniIcons_ttf_len, 20.0f, &FontCFG);
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
    float space = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((space - ItemWidth) * 0.5f);
}

void OmniGUI::ConnectionRing(const char* label)
{
    ImVec2 space = ImGui::GetContentRegionAvail();
    ImVec2 cpos = ImGui::GetCursorScreenPos();

    ImGui::PushID(label);
    ImVec2 pos = ImVec2(cpos.x + (space.x * 0.5f), cpos.y + (space.y * 0.5f));

    ImU32 col = IM_COL32(128, 0, 255, 255);

    // DrawList->AddCircle(pos, 200, col, 20, 3.0f);

    // DrawList->AddCircle(pos, 210, col, 20, 3.0f);

    ImGui::PopID();

    int radius = 205;
    ImVec2 text_size = ImGui::CalcTextSize("192.168.1.255");

    ImGui::PushFont(JetBrainsReg18);

    auto& devices = *AvailableInstances;

    DeviceIcon("C0", pos, text_size, &devices[DeviceMap::C0]);

    if (devices[DeviceMap::L1].InstanceIP) {
        DeviceIcon("L1", ImVec2(pos.x - radius, pos.y), text_size, &devices[DeviceMap::L1]);
    }

    if (devices[DeviceMap::LU1].InstanceIP) {
        DeviceIcon(
            "LU1", ImVec2(pos.x - radius, pos.y - radius), text_size, &devices[DeviceMap::LU1]);
    }

    if (devices[DeviceMap::U1].InstanceIP) {
        DeviceIcon("U1", ImVec2(pos.x, pos.y - radius), text_size, &devices[DeviceMap::U1]);
    }

    if (devices[DeviceMap::RU1].InstanceIP) {
        DeviceIcon(
            "RU1", ImVec2(pos.x + radius, pos.y - radius), text_size, &devices[DeviceMap::RU1]);
    }

    if (devices[DeviceMap::R1].InstanceIP) {
        DeviceIcon("R1", ImVec2(pos.x + radius, pos.y), text_size, &devices[DeviceMap::R1]);
    }

    if (devices[DeviceMap::RD1].InstanceIP) {
        DeviceIcon(
            "RD1", ImVec2(pos.x + radius, pos.y + radius), text_size, &devices[DeviceMap::RD1]);
    }

    if (devices[DeviceMap::D1].InstanceIP) {
        DeviceIcon("D1", ImVec2(pos.x, pos.y + radius), text_size, &devices[DeviceMap::D1]);
    }

    if (devices[DeviceMap::LD1].InstanceIP) {
        DeviceIcon(
            "LD1", ImVec2(pos.x - radius, pos.y + radius), text_size, &devices[DeviceMap::LD1]);
    }

    // if (DeviceHoverState) {
    //	ImGui::PushID("Connect");

    //	const ImGuiID id = ImGui::GetID("Connect");
    //	ImRect bb(ImVec2(SelectedDevicePos.x - 35, SelectedDevicePos.y - 15),
    // ImVec2(SelectedDevicePos.x + 35, SelectedDevicePos.y + 15));
    //	ImGui::ItemAdd(bb, id, NULL, ImGuiItemFlags_None);

    //	bool hovered, held;
    //	bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);
    //	ImGui::RenderNavCursor(bb, id);
    //
    //	//DrawList->AddRectFilled(ImVec2(SelectedDevicePos.x - 35,
    // SelectedDevicePos.y - 15), ImVec2(SelectedDevicePos.x + 35,
    // SelectedDevicePos.y + 15), col, 5.0f, 0);
    //

    //	ImGui::PopID();
    //}

    ImGui::PopFont();
}

bool OmniGUI::HandleEvent(ConnectionRequest& request, float timeout)
{

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f); // Button & checkbox rounding
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f)); // Precise manual layouts

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
