#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include "OmniPackets.h"
#include "imgui.h"
#endif

#include "OmniGUI.h"
#include "OmniLink.h"
#include "fonts.h"

OmniGUI::OmniGUI(OmniLink& OmniLinkInstance) : App(OmniLinkInstance)
{
    AvailableDevices = App.GetAvailableInstances();
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

    ImFontConfig FontCFG;
    FontCFG.FontDataOwnedByAtlas = false;

    JetBrainsReg20 = io.Fonts->AddFontFromMemoryTTF(
        JetBrainsMonoRegular, JetBrainsMonoRegular_Size, 20.0f, &FontCFG);
    JetBrainsReg18 = io.Fonts->AddFontFromMemoryTTF(
        JetBrainsMonoRegular, JetBrainsMonoRegular_Size, 18.0f, &FontCFG);
}

bool OmniGUI::VerticalMenuItem(const char* label)
{
    ImGui::PushID(label);

    ImVec2 MenuItemSize = ImVec2(180, 100);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, MenuItemSize);

    if (!ImGui::IsItemHovered()) {
        ImU32 MenuItemColor = ImGui::GetColorU32(ImVec4(0.09f, 0.09f, 0.09f, 1.0f));

        DrawList->AddRectFilled(
            pos, ImVec2(pos.x + MenuItemSize.x, pos.y + MenuItemSize.y), MenuItemColor);
    } else {
        ImU32 MenuItemColor_Hovered = ImGui::GetColorU32(ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

        DrawList->AddRectFilled(
            pos, ImVec2(pos.x + MenuItemSize.x, pos.y + MenuItemSize.y), MenuItemColor_Hovered);
    }

    ImGui::PopID();

    return clicked;
}

bool OmniGUI::IconizedButton(const char* label, ImVec2& ButtonSize)
{
    ImGui::PushID(label);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, ButtonSize);

    ImVec2 TextSize = ImGui::CalcTextSize(label);
    ImVec2 TextPos = ImVec2(pos.x + (ButtonSize.x - TextSize.x) * 0.5f,
                            pos.y + (ButtonSize.y - TextSize.y) * 0.5f);

    if (!ImGui::IsItemHovered()) {
        ImU32 ButtonColor = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImU32 TextColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        DrawList->AddRectFilled(
            pos, ImVec2(pos.x + ButtonSize.x, pos.y + ButtonSize.y), ButtonColor);
        DrawList->AddText(TextPos, TextColor, label);
    } else {
        ImU32 ButtonColor_Hovered = ImGui::GetColorU32(ImVec4(0.2f, 0.0f, 0.2f, 1.0f));
        ImU32 TextColor_Hovered = ImGui::GetColorU32(ImVec4(0.2f, 0.0f, 0.2f, 1.0f));

        DrawList->AddRectFilled(
            pos, ImVec2(pos.x + ButtonSize.x, pos.y + ButtonSize.y), ButtonColor_Hovered);
        DrawList->AddText(TextPos, TextColor_Hovered, label);
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

    DeviceIcon("C0", pos, text_size, &(*AvailableDevices)[DeviceMap::C0]);

    if ((*AvailableDevices)[DeviceMap::L1].InstanceIP) {
        DeviceIcon(
            "L1", ImVec2(pos.x - radius, pos.y), text_size, &(*AvailableDevices)[DeviceMap::L1]);
    }

    if ((*AvailableDevices)[DeviceMap::LU1].InstanceIP) {
        DeviceIcon("LU1",
                   ImVec2(pos.x - radius, pos.y - radius),
                   text_size,
                   &(*AvailableDevices)[DeviceMap::LU1]);
    }

    if ((*AvailableDevices)[DeviceMap::U1].InstanceIP) {
        DeviceIcon(
            "U1", ImVec2(pos.x, pos.y - radius), text_size, &(*AvailableDevices)[DeviceMap::U1]);
    }

    if ((*AvailableDevices)[DeviceMap::RU1].InstanceIP) {
        DeviceIcon("RU1",
                   ImVec2(pos.x + radius, pos.y - radius),
                   text_size,
                   &(*AvailableDevices)[DeviceMap::RU1]);
    }

    if ((*AvailableDevices)[DeviceMap::R1].InstanceIP) {
        DeviceIcon(
            "R1", ImVec2(pos.x + radius, pos.y), text_size, &(*AvailableDevices)[DeviceMap::R1]);
    }

    if ((*AvailableDevices)[DeviceMap::RD1].InstanceIP) {
        DeviceIcon("RD1",
                   ImVec2(pos.x + radius, pos.y + radius),
                   text_size,
                   &(*AvailableDevices)[DeviceMap::RD1]);
    }

    if ((*AvailableDevices)[DeviceMap::D1].InstanceIP) {
        DeviceIcon(
            "D1", ImVec2(pos.x, pos.y + radius), text_size, &(*AvailableDevices)[DeviceMap::D1]);
    }

    if ((*AvailableDevices)[DeviceMap::LD1].InstanceIP) {
        DeviceIcon("LD1",
                   ImVec2(pos.x - radius, pos.y + radius),
                   text_size,
                   &(*AvailableDevices)[DeviceMap::LD1]);
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
