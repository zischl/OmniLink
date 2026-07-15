#ifndef OMNIGUI_H
#define OMNIGUI_H

#pragma once
#include <variant>

#include "BurstQ.h"
#include "IconLoader.h"
#include "OmniAPI.h"
#include "OmniCore.h"
#include "OmniEnums.h"
#include "OmniInstances.h"
#include "OmniPackets.h"
#include "UIEvents.h"

#ifdef _WIN32
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#endif
#include "imgui_internal.h"

#include <charconv>
#include <unordered_map>

class OmniCore;

struct UIDeviceLayout
{
    DeviceMap DeviceID;
    const char* Label;
    float DirectionalityX;
    float DirectionalityY;
    bool DiagonalState;
};

struct MetricItem
{
    const char* Title;
    const char* Value;
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

    // Welcome to HELL
    static constexpr ImU32 COL_BG_CHILD_1 = IM_COL32(19, 21, 31, 255);
    static constexpr ImU32 COL_TITLE_BG = IM_COL32(8, 9, 14, 255);
    static constexpr ImVec4 COL4_TEXT_MUTED = ImVec4(0.239f, 0.220f, 0.333f, 1.0f);
    static constexpr ImVec4 COL4_TEXT_ACTIVE = ImVec4(0.753f, 0.722f, 0.831f, 1.0f);
    static constexpr ImVec4 COL4_BTN_HOVER_DARK = ImVec4(0.250f, 0.250f, 0.350f, 0.5f);
    static constexpr ImVec4 COL4_BTN_HOVER_RED = ImVec4(0.700f, 0.150f, 0.150f, 0.8f);
    static constexpr ImVec4 COL4_TRANSPARENT = ImVec4(0.000f, 0.000f, 0.000f, 0.0f);

    // Main Style Setup Colors
    static constexpr ImVec4 COL4_STYLE_MODAL_DIM = ImVec4(0.050f, 0.050f, 0.050f, 0.65f);
    static constexpr ImVec4 COL4_STYLE_CHILD_BG = ImVec4(0.031f, 0.031f, 0.059f, 1.0f);
    static constexpr ImVec4 COL4_STYLE_SEPARATOR = ImVec4(0.120f, 0.130f, 0.160f, 1.0f);

    // Dashboard & Metrics Theme Text
    static constexpr ImVec4 COL4_DASH_TEXT_MUTED = ImVec4(0.450f, 0.420f, 0.550f, 1.0f);
    static constexpr ImVec4 COL4_DASH_TEXT_VALUE = ImVec4(0.700f, 0.450f, 1.000f, 1.0f);

    // Notification & Event Handling Theme Colors
    static constexpr ImVec4 COL4_EV_TEXT_MUTED = ImVec4(0.450f, 0.470f, 0.570f, 1.0f);
    static constexpr ImVec4 COL4_EV_ACCENT = ImVec4(0.530f, 0.440f, 0.960f, 1.0f);
    static constexpr ImVec4 COL4_EV_ACCENT_HOVER = ImVec4(0.600f, 0.520f, 0.980f, 1.0f);
    static constexpr ImVec4 COL4_EV_ACCENT_ACTIVE = ImVec4(0.450f, 0.360f, 0.880f, 1.0f);
    static constexpr ImVec4 COL4_EV_FRAME_BG = ImVec4(0.120f, 0.120f, 0.160f, 1.0f);
    static constexpr ImVec4 COL4_EV_FRAME_BG_HOVER = ImVec4(0.180f, 0.180f, 0.240f, 1.0f);
    static constexpr ImVec4 COL4_EV_BTN_DECLINE = ImVec4(0.140f, 0.140f, 0.170f, 1.0f);
    static constexpr ImVec4 COL4_EV_BTN_DEC_HOVER = ImVec4(0.180f, 0.180f, 0.220f, 1.0f);
    static constexpr ImVec4 COL4_EV_BTN_DEC_ACTIVE = ImVec4(0.100f, 0.100f, 0.130f, 1.0f);
    static constexpr ImVec4 COL4_EV_BTN_DEC_BORDER = ImVec4(0.250f, 0.250f, 0.320f, 1.0f);

    // Alert Handling Colors
    static constexpr ImVec4 COL4_ALERT_TITLE = ImVec4(1.000f, 0.820f, 0.000f, 1.0f);

    // Device Representation Layouts
    static constexpr ImU32 COL_DEV_EMPTY = IM_COL32(41, 25, 63, 255);
    static constexpr ImU32 COL_DEV_HOVER = IM_COL32(128, 0, 255, 255);
    static constexpr ImU32 COL_DEV_DEFAULT = IM_COL32(255, 255, 255, 255);
    static constexpr ImU32 COL_RING_BG = IM_COL32(37, 30, 51, 255);

    // Sidebar & Navigation Menus
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

    // Dashboard Layout Drawing
    static constexpr ImU32 COL_DASH_BG = IM_COL32(18, 13, 26, 255);
    static constexpr ImU32 COL_DASH_BORDER = IM_COL32(46, 31, 64, 255);

    // Modal Hardware Connection Layouts
    static constexpr ImU32 COL_MODAL_STRIP = IM_COL32(135, 112, 245, 255);
    static constexpr ImU32 COL_MODAL_BOX_BG = IM_COL32(40, 40, 55, 100);
    static constexpr ImU32 COL_MODAL_BOX_BRD = IM_COL32(75, 70, 105, 255);
    static constexpr ImU32 COL_MODAL_CIRC_BG = IM_COL32(23, 23, 30, 255);
    static constexpr ImU32 COL_MODAL_CIRC_BRD = IM_COL32(50, 50, 65, 255);

    // Bezier Curves
    static constexpr ImU32 COL_GLOW_OPAQUE = IM_COL32(128, 0, 255, 255);
    static constexpr ImU32 COL_GLOW_HIGH = IM_COL32(128, 0, 255, 153);
    static constexpr ImU32 COL_GLOW_MED = IM_COL32(128, 0, 255, 76);
    static constexpr ImU32 COL_GLOW_LOW = IM_COL32(128, 0, 255, 25);

  private:
    OmniCore& App;
    std::unordered_map<DeviceMap, OmniInstance>* AvailableInstances = nullptr;
    ActiveInstanceContainer* ActiveInstances = nullptr;
    DeviceMap& SelectedDevice;

    IconLoader IconTexture;
    bool ImGuiState = true;

    bool DeviceHoverState = false;
    ImVec2 SelectedDevicePos;

    ImDrawList* DrawList = nullptr;

    int ActiveMenu = 0;
    BurstQ<Notification, 4> NotificationQueue{};

    // Fonts
    ImFont* InterReg14 = nullptr;
    ImFont* InterReg15 = nullptr;
    ImFont* InterMed14 = nullptr;
    ImFont* InterMed16 = nullptr;
    ImFont* JetBrainsMed15 = nullptr;
    ImFont* JetBrainsBold20 = nullptr;
    ImFont* OmniIconsLarge = nullptr;
    ImFont* OmniIconsMedium = nullptr;
    ImFont* OmniIconsSmall = nullptr;

    void DeviceIconPreview(
        const ImVec2& pos,
        const ImU32& col,
        const ImVec2& text_size = ImVec2{0, 0},
        const char* text = ""
    );

    void DeviceIcon(const char* Label, const ImVec2& Pos, const OmniInstance* DeviceData);

    bool IconizedButton(const char* Label, const char* Icon, bool state, const ImVec2& ButtonSize);
    void DeviceAddButton(const ImVec2& CenterPos, ImU32 Color);
    bool VerticalMenuItem(const char* label, const char* icon, bool state, ImVec2& MenuItemSize);
    int ConnectionRing(const char* label, const ImVec2& WidgetSize, const float Radius);
    void MetricDashboard(
        const char* ContainerId,
        const MetricItem* Items,
        int ItemCount,
        float TotalWidth,
        float Height
    );

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

        ImVec2 position;

        if (notification.Layout == Notification::EventLayout::CENTER) {
            position = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(position, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(360.0f, 364.0f));
        } else {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImVec2 size = ImVec2(320.0f, 72.0f);
            float padding = 10.0f;
            position.x = viewport->WorkPos.x + viewport->WorkSize.x - size.x - padding;
            position.y = viewport->WorkPos.y + viewport->WorkSize.y - size.y - padding;

            ImGui::SetNextWindowPos(position, ImGuiCond_Always);
            ImGui::SetNextWindowSize(size);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 12.0f));

        bool clicked = false;

        if (ImGui::BeginPopupModal(label, NULL, DefaultFlags)) {

            notification.Timeout -= ImGui::GetIO().DeltaTime;
            if (notification.Timeout <= 0.0f) {
                end = true;
            }

            clicked = std::visit(
                [&](auto& args) { return HandleEvent(args, notification.Timeout); },
                notification.Event
            );
            if (clicked)
                end = true;

            if (end)
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar();

        return end;
    }

  public:
    OmniGUI(OmniCore& OmniCoreInstance);

    inline void PushNotification(const Notification& notification)
    {
        NotificationQueue.push(notification);
    }

    void SetupImGui(void* hwnd, void* D3D11Device, void* D3D11Context);

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
            ImGui::PushStyleColor(ImGuiCol_ChildBg, COL_BG_CHILD_1);
            DrawList = ImGui::GetWindowDrawList();

            ImVec2 MenuItemSize = ImVec2(110, 100);
            ImGui::BeginChild("SideMenu", ImVec2(110, 0), ImGuiChildFlags_None);
            {

                ImGui::PushFont(InterReg14);

                ImGui::Dummy(ImVec2(0, 4));

                ImGui::Image(
                    reinterpret_cast<ImTextureID>(IconTexture.GetTextureID()),
                    ImVec2(110.0f, 110.0f)
                );

                ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

                ImGui::Dummy(ImVec2(0, 105));

                if (VerticalMenuItem("Nexus", IC_LINK, ActiveMenu == 0, MenuItemSize))
                    ActiveMenu = 0;

                if (VerticalMenuItem("Instances", IC_SERVER, ActiveMenu == 1, MenuItemSize))
                    ActiveMenu = 1;

                if (VerticalMenuItem("Keybinds", IC_KEYBOARD, ActiveMenu == 2, MenuItemSize))
                    ActiveMenu = 2;

                if (VerticalMenuItem("Settings", IC_SETTINGS, ActiveMenu == 3, MenuItemSize))
                    ActiveMenu = 3;

                ImGui::PopFont();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::SameLine(0.0f, 0.0f);
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine(0.0f, 0.0f);

            ImGui::BeginGroup();

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

            // Content Space Begins The Journey To Defeat The Demon King Down Here.

            const ImVec2 ContentSpaceSize = ImGui::GetContentRegionAvail();
            const ImVec2 ContentSpaceStart = ImGui::GetCursorScreenPos();
            const float MaxContentPosX = ContentSpaceStart.x + ContentSpaceSize.x;

            const float VerticalSpacing = 6.0f;
            const float textHeight = ImGui::GetTextLineHeight();
            const float TitleBarHeight = textHeight + (VerticalSpacing * 2.0f);

            // Title barrrr
            ImGui::BeginGroup();
            {
                DrawList->AddRectFilled(
                    ContentSpaceStart,
                    ImVec2(MaxContentPosX, ContentSpaceStart.y + TitleBarHeight),
                    COL_TITLE_BG,
                    16.0f,
                    ImDrawFlags_RoundCornersTopRight
                );

                ImGui::PushFont(InterReg15);
                ImGui::SetCursorPosY(ContentSpaceStart.y + VerticalSpacing);
                ImGui::SetCursorPosX(ContentSpaceStart.x + 10.0f);
                // Title Bar Text
                ImGui::TextColored(COL4_TEXT_MUTED, "OmniLink   >   ");
                ImGui::SameLine(0.0f, 0.0f);

                ImGui::SetCursorPosY(ContentSpaceStart.y + VerticalSpacing);

                switch (ActiveMenu) {
                case 0:
                    ImGui::TextColored(COL4_TEXT_ACTIVE, "Nexus");
                    break;
                case 1:
                    ImGui::TextColored(COL4_TEXT_ACTIVE, "Instances");
                    break;
                case 2:
                    ImGui::TextColored(COL4_TEXT_ACTIVE, "Keybinds");
                    break;
                case 3:
                    ImGui::TextColored(COL4_TEXT_ACTIVE, "Settings");
                    break;
                }

                ImGui::PopFont();

                // Title Bar Buttons
                float TotalControlsWidth = TitleBarHeight * 2;

                // Window Dragging, Calling Virtual Funcs but shoulb pose no performance issues
                if (ImGui::IsMouseHoveringRect(
                        ContentSpaceStart,
                        ImVec2(
                            MaxContentPosX - TotalControlsWidth,
                            ContentSpaceStart.y + TitleBarHeight
                        )
                    ) &&
                    ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                    App.DragWindow();
                }

                ImGui::SameLine(ContentSpaceSize.x - TotalControlsWidth, 0.0f);

                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, COL4_TRANSPARENT);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL4_BTN_HOVER_DARK);

                ImGui::SetCursorPosY(ContentSpaceStart.y);
                ImGui::PushFont(OmniIconsSmall);
                if (ImGui::Button(IC_MINUS, ImVec2(TitleBarHeight, TitleBarHeight))) {
                    App.MinimizeWindow();
                }

                ImGui::SameLine(0.0f, 0.0f);

                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL4_BTN_HOVER_RED);
                if (ImGui::Button(IC_X, ImVec2(TitleBarHeight, TitleBarHeight))) {
                    App.HideWindow();
                }

                ImGui::PopFont();

                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar(1);
            }
            ImGui::EndGroup();

            // Content Space SubSpace Begins It's Journey To Suicide

            switch (ActiveMenu) {

            case 0:

            {
                // Feature Panel
                ImGui::BeginGroup();

                const float FeaturePanelHeight = 92.0f;

                ImVec2 PanelP1 = ImGui::GetCursorScreenPos();
                DrawList->AddRectFilled(
                    PanelP1, ImVec2(MaxContentPosX, PanelP1.y + FeaturePanelHeight), COL_BG_CHILD_1
                );

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

                // Feature Mode Text
                const char* ModeText = "    M    \n    O    \n    D    \n    E    ";
                const ImVec2 ModeTextSize = ImGui::CalcTextSize(ModeText);
                float ModeTextPadding = (FeaturePanelHeight - ModeTextSize.y) * 0.5;

                ImGui::BeginGroup();
                ImGui::Dummy(ImVec2(0.0f, ModeTextPadding));
                ImGui::TextColored(COL4_TEXT_MUTED, "%s", ModeText);
                ImGui::Dummy(ImVec2(0.0f, ModeTextPadding));
                ImGui::EndGroup();

                ImGui::SameLine(0.0f, 0.0f);
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                ImGui::SameLine(0.0f, 0.0f);

                ImGui::PopStyleVar();

                ImVec2 size =
                    ImVec2(1 + ((ContentSpaceSize.x - ModeTextSize.x) / 5), FeaturePanelHeight);

                const uint32_t FeatureSates = (*ActiveInstances)[SelectedDevice].ActiveFlags;
                ImGui::PushFont(InterMed16);

                if (IconizedButton(
                        "Screen Link",
                        IC_SCREEN_SHARE,
                        (FeatureSates & FeatureFlags::fScreenLink) != 0,
                        size
                    )) {
                    OmniAPI::ToggleFeature(FeatureTypes::ScreenLink, SelectedDevice);
                }
                ImGui::SameLine(0.0f, 0.0f);

                if (IconizedButton(
                        "Window Link",
                        IC_APP_WINDOW,
                        (FeatureSates & FeatureFlags::fWindowLink) != 0,
                        size
                    )) {
                    OmniAPI::ToggleFeature(FeatureTypes::WindowLink, SelectedDevice);
                }
                ImGui::SameLine(0.0f, 0.0f);

                if (IconizedButton(
                        "Input Link", IC_MOUSE, (FeatureSates & FeatureFlags::fInputLink) != 1, size
                    )) {
                    OmniAPI::ToggleFeature(FeatureTypes::InputLink, SelectedDevice);
                }
                ImGui::SameLine(0.0f, 0.0f);

                if (IconizedButton(
                        "Audio Link",
                        IC_VOLUME_2,
                        (FeatureSates & FeatureFlags::fAudioLink) != 0,
                        size
                    )) {
                    OmniAPI::ToggleFeature(FeatureTypes::AudioLink, SelectedDevice);
                }
                ImGui::SameLine(0.0f, 0.0f);

                if (IconizedButton(
                        "Clipboard Link",
                        IC_CLIPBOARD,
                        (FeatureSates & FeatureFlags::fClipBoardLink) != 0,
                        size
                    )) {
                    // SetEvent(EventHandler[0]);
                }

                ImGui::PopFont();
                ImGui::PopStyleVar();
                ImGui::EndGroup();
                // Feature Panel End

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
                ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
                ImGui::PopStyleVar();

                DrawList = ImGui::GetWindowDrawList();

                // Da Connection Ring
                const ImVec2 AvailableSpace = ImGui::GetContentRegionAvail();
                int DeviceCount = ConnectionRing(
                    "DiscoveryRing", ImVec2(AvailableSpace.x, AvailableSpace.y - 60), 205
                );

                /* if (ImGui::Button("Scan")) {
                    static bool scanning = false;
                    OmniAPI::Scan();
                } */

                // Metrics Dashboard
                static char availableBuf[16];
                static char activeBuf[32];

                std::to_chars(availableBuf, availableBuf + sizeof(availableBuf), DeviceCount);

                auto [ptr, ec] = std::to_chars(
                    activeBuf, activeBuf + sizeof(activeBuf), ActiveInstances->size() - 1
                );

                *ptr = '/';
                *(ptr + 1) = '8';
                *(ptr + 2) = '\0';

                static MetricItem staticMetrics[] = {
                    {"Available", availableBuf},
                    {"Active", activeBuf},
                    {"Latency", "7.6ms"},
                    {"Bandwith", "1.2 MB/s"}
                };

                MetricDashboard("NetContainer", staticMetrics, 4, AvailableSpace.x, 55.0f);

                if (auto* activeNotif = NotificationQueue.peek()) {
                    if (NotificationWindow(activeNotif->EventName, *activeNotif)) {
                        NotificationQueue.pop();
                    }
                }

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

        ImGui::EndGroup();

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
