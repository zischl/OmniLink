#ifndef OMNIGUI_H
#define OMNIGUI_H

#pragma once
#include "OmniEnums.h"
#include "OmniPackets.h"
#include <variant>
#include <vector>

#include "OmniAPI.h"
#include "OmniInstances.h"

#include <wrl/client.h>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"

#include <charconv>
#include <unordered_map>

class OmniLink;

struct Alert
{
    std::string Title;
    std::string Desc;
};

struct MetricItem
{
    const char* title;
    const char* value;
};

using EventTypes = std::variant<ConnectionRequest, Alert>;

struct Notification
{
    EventTypes Event;
    const char* EventName;
    bool Active = false;
    float Timeout = 15.0f;
};

static constexpr ImGuiWindowFlags DefaultFlags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

class OmniGUI
{

#define IC_SETTINGS "\xef\x80\x80"
#define IC_SERVER "\xef\x80\x81"
#define IC_KEYBOARD "\xef\x80\x82"
#define IC_LINK "\xef\x80\x83"
#define IC_WAYPOINTS "\xef\x80\x84"
#define IC_SCREEN_SHARE "\xef\x80\x85"
#define IC_APP_WINDOW "\xef\x80\x86"
#define IC_VOLUME_2 "\xef\x80\x87"
#define IC_CLIPBOARD "\xef\x80\x88"
#define IC_MOUSE "\xef\x80\x89"
#define IC_SHIELD "\xef\x80\x8a"
#define IC_WIFI "\xef\x80\x8b"
#define IC_TRASH_2 "\xef\x80\x8c"
#define IC_ZAP "\xef\x80\x8d"
#define IC_NETWORK "\xef\x80\xae"
#define IC_BELL "\xef\x80\xaf"
#define IC_INFO "\xef\x80\x90"
#define IC_DIAMOND_PLUS "\xef\x80\x91"
#define IC_AIRPLAY "\xef\x80\x92"
#define IC_X "\xEF\x80\x93"
#define IC_MINUS "\xEF\x80\x94"

    // Trust me, this was hell
    static constexpr ImU32 COL_DEV_EMPTY = IM_COL32(0x29, 0x19, 0x3F, 255);
    static constexpr ImVec4 BG_CHILD_1 = ImVec4(0.074f, 0.082f, 0.121f, 1.0f);
    static constexpr ImVec4 TEXT_MUTED = ImVec4(0.239f, 0.220f, 0.333f, 1.0f);
    static constexpr ImVec4 TEXT_ACTIVE = ImVec4(0.753f, 0.722f, 0.831f, 1.0f);
    static constexpr ImVec4 BTN_HOVER_DARK = ImVec4(0.25f, 0.25f, 0.35f, 0.5f);
    static constexpr ImVec4 BTN_HOVER_RED = ImVec4(0.70f, 0.15f, 0.15f, 0.8f);

    // ImVec4 Style Colors
    static constexpr ImVec4 STYLE_MODAL_DIM = ImVec4(0.05f, 0.05f, 0.05f, 0.65f);
    static constexpr ImVec4 STYLE_CHILD_BG = ImVec4(0.031f, 0.031f, 0.059f, 1.0f);
    static constexpr ImVec4 STYLE_SEPARATOR = ImVec4(0.12f, 0.13f, 0.16f, 1.00f);

    // Dashboard & Metrics Theme
    static constexpr ImVec4 DASH_BG = ImVec4(0.07f, 0.05f, 0.10f, 1.00f);
    static constexpr ImVec4 DASH_BORDER = ImVec4(0.18f, 0.12f, 0.25f, 1.00f);
    static constexpr ImVec4 DASH_TEXT_MUTED = ImVec4(0.45f, 0.42f, 0.55f, 1.00f);
    static constexpr ImVec4 DASH_TEXT_VALUE = ImVec4(0.70f, 0.45f, 1.00f, 1.00f);

    // Notification & Event Handling Theme
    static constexpr ImVec4 EV_TEXT_MUTED = ImVec4(0.45f, 0.47f, 0.57f, 1.0f);
    static constexpr ImVec4 EV_ACCENT = ImVec4(0.53f, 0.44f, 0.96f, 1.0f);
    static constexpr ImVec4 EV_ACCENT_HOVER = ImVec4(0.60f, 0.52f, 0.98f, 1.0f);
    static constexpr ImVec4 EV_ACCENT_ACTIVE = ImVec4(0.45f, 0.36f, 0.88f, 1.0f);
    static constexpr ImVec4 EV_FRAME_BG = ImVec4(0.12f, 0.12f, 0.16f, 1.0f);
    static constexpr ImVec4 EV_FRAME_BG_HOVER = ImVec4(0.18f, 0.18f, 0.24f, 1.0f);
    static constexpr ImVec4 EV_BTN_DECLINE = ImVec4(0.14f, 0.14f, 0.17f, 1.0f);
    static constexpr ImVec4 EV_BTN_DEC_HOVER = ImVec4(0.18f, 0.18f, 0.22f, 1.0f);
    static constexpr ImVec4 EV_BTN_DEC_ACTIVE = ImVec4(0.10f, 0.10f, 0.13f, 1.0f);
    static constexpr ImVec4 EV_BTN_DEC_BORDER = ImVec4(0.25f, 0.25f, 0.32f, 1.0f);

    // Bezier Curve / Glow Base Vectors
    static constexpr ImVec4 GLOW_OPAQUE = ImVec4(0.5f, 0.0f, 1.0f, 1.0f);
    static constexpr ImVec4 GLOW_HIGH = ImVec4(0.5f, 0.0f, 1.0f, 0.6f);
    static constexpr ImVec4 GLOW_MED = ImVec4(0.5f, 0.0f, 1.0f, 0.3f);
    static constexpr ImVec4 GLOW_LOW = ImVec4(0.5f, 0.0f, 1.0f, 0.1f);

    // ImU32 Packed Drawing Colors
    static constexpr ImU32 COL_DEV_HOVER = IM_COL32(128, 0, 255, 255);
    static constexpr ImU32 COL_DEV_DEFAULT = IM_COL32(255, 255, 255, 255);

    // Sidebar & Menus
    static constexpr ImU32 COL_MENU_BG_IDLE = IM_COL32(30, 30, 40, 0);
    static constexpr ImU32 COL_MENU_ICON_IDLE = IM_COL32(30, 30, 40, 255);
    static constexpr ImU32 COL_MENU_BG_ACTIVE = IM_COL32(168, 85, 247, 40);
    static constexpr ImU32 COL_MENU_ICON_ACT = IM_COL32(168, 85, 247, 20);
    static constexpr ImU32 COL_MENU_STRIP = IM_COL32(168, 85, 247, 255);
    static constexpr ImU32 COL_MENU_BG_HOVER = IM_COL32(255, 255, 255, 15);
    static constexpr ImU32 COL_MENU_TINT_ACT = IM_COL32(168, 85, 247, 255);
    static constexpr ImU32 COL_MENU_TINT_IDLE = IM_COL32(140, 140, 160, 255);
    static constexpr ImU32 COL_MENU_TXT_ACT = IM_COL32(168, 85, 247, 255);
    static constexpr ImU32 COL_MENU_TXT_IDLE = IM_COL32(100, 105, 125, 255);

    // Feature Grid Buttons
    static constexpr ImU32 COL_FEAT_BG_ACTIVE = IM_COL32(157, 78, 221, 30);
    static constexpr ImU32 COL_FEAT_BG_HOVER = IM_COL32(255, 255, 255, 10);
    static constexpr ImU32 COL_FEAT_IC_ACTIVE = IM_COL32(157, 78, 221, 35);
    static constexpr ImU32 COL_FEAT_IC_HOVER = IM_COL32(255, 255, 255, 10);
    static constexpr ImU32 COL_FEAT_TINT_ACT = IM_COL32(157, 78, 221, 255);
    static constexpr ImU32 COL_FEAT_TINT_IDLE = IM_COL32(160, 160, 170, 255);
    static constexpr ImU32 COL_FEAT_TXT_ACT = IM_COL32(230, 230, 255, 255);
    static constexpr ImU32 COL_FEAT_TXT_IDLE = IM_COL32(150, 150, 160, 255);
    static constexpr ImU32 COL_FEAT_STRIP = IM_COL32(157, 78, 221, 255);
    static constexpr ImU32 COL_FEAT_GLOW = IM_COL32(157, 78, 221, 45);

    // Connection Ring Radar Layout
    static constexpr ImU32 COL_RING_BG = IM_COL32(0x11, 0x0C, 0x1A, 255);

    // Modal Hardware Connection Layouts
    static constexpr ImU32 COL_MODAL_STRIP = IM_COL32(135, 112, 245, 255);
    static constexpr ImU32 COL_MODAL_BOX_BG = IM_COL32(40, 40, 55, 100);
    static constexpr ImU32 COL_MODAL_BOX_BRD = IM_COL32(75, 70, 105, 255);
    static constexpr ImU32 COL_MODAL_CIRC_BG = IM_COL32(23, 23, 30, 255);
    static constexpr ImU32 COL_MODAL_CIRC_BRD = IM_COL32(50, 50, 65, 255);

  private:
    OmniLink& App;
    std::unordered_map<DeviceMap, OmniInstance>* AvailableInstances = nullptr;
    ActiveInstanceContainer* ActiveInstances = nullptr;
    DeviceMap& SelectedDevice;

    bool ImGuiState = true;

    bool DeviceHoverState = false;
    ImVec2 SelectedDevicePos;

    int user_resx = GetSystemMetrics(SM_CXSCREEN);
    int user_resy = GetSystemMetrics(SM_CYSCREEN);

    ImDrawList* DrawList = nullptr;

    int ActiveMenu = 0;
    std::vector<Notification> Notifications = {};

    // Fonts
    ImFont* JetBrainsReg20 = nullptr;
    ImFont* JetBrainsReg18 = nullptr;
    ImFont* OmniIconsLarge = nullptr;
    ImFont* OmniIconsMedium = nullptr;
    ImFont* OmniIconsSmall = nullptr;

    void DeviceIconPreview(const ImVec2& pos,
                           const ImU32& col,
                           const ImVec2& text_size = ImVec2{0, 0},
                           const char* text = "");

    void DeviceIcon(const char* Label, const ImVec2& Pos, const OmniInstance* DeviceData);

    bool IconizedButton(const char* Label, const char* Icon, bool state, const ImVec2& ButtonSize);
    void DeviceAddButton(const ImVec2& CenterPos, ImU32 Color);
    bool VerticalMenuItem(const char* label, const char* icon, bool state, ImVec2& MenuItemSize);
    void ConnectionRing(const char* label, const ImVec2& WidgetSize, const float Radius);
    void DrawMetricDashboard(const char* ContainerId,
                             const MetricItem* Items,
                             int ItemCount,
                             float TotalWidth,
                             float Height);

    // Notification Event Handlers
    static bool HandleEvent(ConnectionRequest& request, float timeout);
    static bool HandleEvent(Alert& request, float timeout);

    void CenterItemX(const float ItemWidth);
    void CreateCurvedLine(const char* label, int curve);

    bool NotificationWindow(const char* label, Notification& notification)
    {
        bool end = false;

        if (notification.Active) {
            ImGui::OpenPopup(label);
            notification.Active = false;
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(360.0f, 364.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));

        bool clicked = false;

        if (ImGui::BeginPopupModal(label, NULL, DefaultFlags)) {

            notification.Timeout -= ImGui::GetIO().DeltaTime;
            if (notification.Timeout <= 0.0f) {
                end = true;
            }

            clicked =
                std::visit([&](auto& args) { return HandleEvent(args, notification.Timeout); },
                           notification.Event);
            if (clicked)
                end = true;

            if (end)
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();

        if (end) {
            Notifications.erase(Notifications.begin());
        }

        return clicked ? true : false;
    }

  public:
    OmniGUI(OmniLink& OmniLinkInstance);

    void SetupImGui(HWND hwnd, ID3D11Device* D3D11Device, ID3D11DeviceContext* D3D11Context);

    inline void FrameBegin()
    {
        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawList = ImGui::GetWindowDrawList();

        ImVec2 WindowSize = ImVec2(1280, 810);
        ImGui::SetNextWindowSize(WindowSize);
        ImGui::SetNextWindowPos(ImVec2(0, 0));

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 15.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        if (ImGui::Begin("OmniLink", &ImGuiState, ImGuiWindowFlags_NoTitleBar)) {

            ImGui::PopStyleVar();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_CHILD_1);
            DrawList = ImGui::GetWindowDrawList();

            ImVec2 MenuItemSize = ImVec2(110, 100);
            ImGui::BeginChild("SideMenu", ImVec2(110, 0), ImGuiChildFlags_None);
            {
                ImGui::Dummy(ImVec2(0, 50));

                ImGui::SameLine(0.0f, 0.0f);
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
                ImGui::SameLine(0.0f, 0.0f);

                ImGui::Dummy(ImVec2(0, 155));

                if (VerticalMenuItem("Nexus", IC_LINK, ActiveMenu == 0, MenuItemSize))
                    ActiveMenu = 0;

                if (VerticalMenuItem("Instances", IC_SERVER, ActiveMenu == 1, MenuItemSize))
                    ActiveMenu = 1;

                if (VerticalMenuItem("Keybinds", IC_KEYBOARD, ActiveMenu == 2, MenuItemSize))
                    ActiveMenu = 2;

                if (VerticalMenuItem("Settings", IC_SETTINGS, ActiveMenu == 3, MenuItemSize))
                    ActiveMenu = 3;
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::SameLine(0.0f, 0.0f);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine(0.0f, 0.0f);

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::BeginChild("menu-item", ImVec2(0.0f, 0.0f));
            {
                // Title barrrr
                const float VerticalSpacing = 6.0f;
                const float textHeight = ImGui::GetTextLineHeight();
                const float buttonSize = textHeight + (VerticalSpacing * 2.0f);

                const float startY = ImGui::GetCursorPosY();

                ImGui::SetCursorPosY(startY + VerticalSpacing);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f);
                ImGui::TextColored(ImVec4(0.239f, 0.220f, 0.333f, 1.0f), "OmniLink > ");
                ImGui::SameLine(0.0f, 0.0f);

                switch (ActiveMenu) {
                case 0:
                    ImGui::TextColored(ImVec4(0.753f, 0.722f, 0.831f, 1.0f), "Nexus");
                    break;
                case 1:
                    ImGui::TextColored(ImVec4(0.753f, 0.722f, 0.831f, 1.0f), "Instances");
                    break;
                case 2:
                    ImGui::TextColored(ImVec4(0.753f, 0.722f, 0.831f, 1.0f), "Keybinds");
                    break;
                case 3:
                    ImGui::TextColored(ImVec4(0.753f, 0.722f, 0.831f, 1.0f), "Settings");
                    break;
                }

                // Title Bar Buttons
                float totalControlsWidth = buttonSize * 2;
                float availableX = ImGui::GetContentRegionAvail().x;

                ImGui::SameLine(availableX - totalControlsWidth, 0.0f);

                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.35f, 0.5f));

                ImGui::SetCursorPosY(startY);

                ImGui::PushFont(OmniIconsSmall);

                if (ImGui::Button(IC_MINUS, ImVec2(buttonSize, buttonSize))) {
                }

                ImGui::SameLine(0.0f, 0.0f);
                ImGui::SetCursorPosY(startY);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.15f, 0.15f, 0.8f));
                if (ImGui::Button(IC_X, ImVec2(buttonSize, buttonSize))) {
                    ImGuiState = false;
                }

                ImGui::PopFont();

                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar();

                ImGui::SetCursorPosY(startY + buttonSize);

                switch (ActiveMenu) {

                case 0:

                {

                    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, BG_CHILD_1);

                    ImGui::BeginChild("FeaturePanel", ImVec2(0, 120));

                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
                    ImGui::SameLine(0.0f, 0.0f);

                    ImGui::TextColored(ImVec4(0.239f, 0.220f, 0.333f, 1.0f),
                                       "\n  M  \n  O  \n  D  \n  E  \n");

                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine(0.0f, 0.0f);

                    ImGui::SameLine(0.0f, 0.0f);

                    const ImVec2 AvailabelSpace = ImGui::GetContentRegionAvail();
                    ImVec2 size = ImVec2(AvailabelSpace.x / 5, 120);

                    const uint32_t FeatureSates = (*ActiveInstances)[SelectedDevice].ActiveFlags;

                    if (IconizedButton("Screen Link",
                                       IC_SCREEN_SHARE,
                                       (FeatureSates & FeatureFlags::fScreenLink) != 0,
                                       size)) {
                        OmniAPI::ToggleFeature(FeatureTypes::ScreenLink, DeviceMap::C0);
                    }
                    ImGui::SameLine(0.0f, 0.0f);

                    if (IconizedButton("Window Link",
                                       IC_APP_WINDOW,
                                       (FeatureSates & FeatureFlags::fWindowLink) != 0,
                                       size)) {
                        OmniAPI::ToggleFeature(FeatureTypes::WindowLink, DeviceMap::C0);
                    }
                    ImGui::SameLine(0.0f, 0.0f);

                    if (IconizedButton("Input Link",
                                       IC_MOUSE,
                                       (FeatureSates & FeatureFlags::fInputLink) != 0,
                                       size)) {
                        OmniAPI::ToggleFeature(FeatureTypes::InputLink, DeviceMap::C0);
                    }
                    ImGui::SameLine(0.0f, 0.0f);

                    if (IconizedButton("Audio Link",
                                       IC_VOLUME_2,
                                       (FeatureSates & FeatureFlags::fAudioLink) != 0,
                                       size)) {
                        OmniAPI::ToggleFeature(FeatureTypes::AudioLink, DeviceMap::C0);
                    }
                    ImGui::SameLine(0.0f, 0.0f);

                    if (IconizedButton("Clipboard Link",
                                       IC_CLIPBOARD,
                                       (FeatureSates & FeatureFlags::fClipBoardLink) != 0,
                                       size)) {
                        // SetEvent(EventHandler[0]);
                    }

                    ImGui::SameLine(0.0f, 0.0f);
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
                    ImGui::SameLine(0.0f, 0.0f);

                    ImGui::EndChild();
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor();

                    DrawList = ImGui::GetWindowDrawList();

                    ConnectionRing(
                        "ConRing", ImVec2(ImGui::GetContentRegionAvail().x, 510.0f), 205);

                    if (ImGui::Button("Scan")) {
                        OmniAPI::Scan();
                    }

                    static char availableBuf[16];
                    static char activeBuf[32];

                    std::to_chars(availableBuf,
                                  availableBuf + sizeof(availableBuf),
                                  AvailableInstances->size());

                    auto [ptr, ec] = std::to_chars(
                        activeBuf, activeBuf + sizeof(activeBuf), ActiveInstances->size());

                    *ptr = '/';
                    *(ptr + 1) = '8';
                    *(ptr + 2) = '\0';

                    static MetricItem staticMetrics[] = {{"Available", availableBuf},
                                                         {"Active", activeBuf},
                                                         {"Latency", "7.6ms"},
                                                         {"Bandwith", "1.2 MB/s"}};

                    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetContentRegionAvail().y -
                                         70.0f);
                    DrawMetricDashboard("NetContainer", staticMetrics, 4, AvailabelSpace.x, 70.0f);

                    if (!Notifications.empty()) {
                        for (Notification& notification : Notifications) {
                            NotificationWindow(notification.EventName, notification);
                        }
                    }

                    /*CreateCurvedLine("ln4", 20); ImGui::SameLine(40.0f, -1.0f);

                    CreateCurvedLine("ln3", 25); ImGui::SameLine(70.0f, -1.0f);

                    CreateCurvedLine("ln2", 30); ImGui::SameLine(100.0f, -1.0f);

                    CreateCurvedLine("ln1", 40); ImGui::SameLine(100.0f, -1.0f);*/

                    /*CreateCurvedLine("ln1", 40); ImGui::SameLine(40.0f, -1.0f);

                    CreateCurvedLine("ln2", 30); ImGui::SameLine(70.0f, -1.0f);

                    CreateCurvedLine("ln3", 25); ImGui::SameLine(100.0f, -1.0f);

                    CreateCurvedLine("ln4", 20);*/

                }

                break;

                case 1:

                    break;

                case 2:
                    break;

                case 3:
                    break;
                }
            }
            ImGui::EndChild();
        }

        ImGui::End();
    }

    inline void Render()
    {
        // Rendering
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
};

#endif
