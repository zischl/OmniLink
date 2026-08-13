#ifndef OMNIGUI_H
#define OMNIGUI_H

#pragma once
#include <variant>

#include "BurstQ.h"
#include "GUIEnums.h"
#include "GUITypes.h"
#include "IconLoader.h"
#include "OmniAPI.h"
#include "OmniColors.h"
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
#define IC_NETWORK "\xef\x80\x8e"
#define IC_BELL "\xef\x80\x8f"
#define IC_INFO "\xef\x80\x90"
#define IC_DIAMOND_PLUS "\xef\x80\x91"
#define IC_AIRPLAY "\xef\x80\x92"
#define IC_X "\xef\x80\x93"
#define IC_MINUS "\xef\x80\x94"
#define IC_SLIDERS_HORIZONTAL "\xef\x80\x95"
#define IC_SLIDERS IC_SLIDERS_HORIZONTAL
#define IC_REFRESH_CW "\xef\x80\x96"
#define IC_GAUGE "\xef\x80\x97"
#define IC_CIRCLE_CHECK "\xef\x80\x98"
#define IC_CIRCLE_X "\xef\x80\x99"
#define IC_CIRCLE_ALERT "\xef\x80\x9a"
#define IC_LAPTOP "\xef\x80\x9b"
#define IC_AIRPLAY_THICK "\xef\x80\x9c"

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
    ImFont* InterMed12 = nullptr;
    ImFont* InterReg14 = nullptr;
    ImFont* InterReg15 = nullptr;
    ImFont* InterMed14 = nullptr;
    ImFont* InterMed15 = nullptr;
    ImFont* InterMed16 = nullptr;
    ImFont* InterBold18 = nullptr;
    ImFont* InterBold20 = nullptr;
    ImFont* JetBrainsMed15 = nullptr;
    ImFont* JetBrainsBold20 = nullptr;
    ImFont* OmniIconsLarge = nullptr;
    ImFont* OmniIconsMedium = nullptr;
    ImFont* OmniIconsSmall = nullptr;

    // Setting Row State
    ImVec2 SettingRowStartPos{0.0f, 0.0f};
    float SettingRowAvailWidth = 0.0f;
    bool SettingRowShowSeparator = true;

    // UI Widgets
    bool
    IconButton(const char* Icon, const char* Label, const ImVec2& Size, const ButtonColors& Colors);

    void SectionHeader(const char* Icon, const char* Title, const char* Subtitle = nullptr);

    void Mini3x3GridWidget(ImVec2 GridTopLeft, int SlotIdx);

    void ToggleSwitch(const char* StrId, bool* Val);

    void BeginGroupCard(const char* Icon, const char* Title, float Height);

    void BeginSettingRow(
        const char* title, const char* subtitle, float controlWidth, bool showSeparator = true
    );

    void EndSettingRow();

    void DeviceIconPreview(
        const ImVec2& Pos,
        const ImU32& Col,
        const ImVec2& TextSize = ImVec2{0, 0},
        const char* Text = ""
    );
    void DeviceIcon(const char* Label, const ImVec2& Pos, const OmniInstance* DeviceData);
    bool DeviceAddButton(const char* Label, const ImVec2& CenterPos, ImU32 Color);
    int ConnectionRing(const char* label, const ImVec2& WidgetSize, const float Radius);

    bool IconizedButton(const char* Label, const char* Icon, bool state, const ImVec2& ButtonSize);
    bool VerticalMenuItem(const char* label, const char* icon, bool state, ImVec2& MenuItemSize);

    void MetricDashboard(
        const char* ContainerId,
        const MetricItem* Items,
        int ItemCount,
        float TotalWidth,
        float Height
    );

    void RenderConnectModal();
    void RenderFeatureControlBar(
        uint32_t FeatureFlags, const ImVec2& ContentSpaceSize, const ImVec2& ContentSpaceStart
    );
    void RenderMetricsBar(int DeviceCount, size_t ActiveCount, float AvailableWidth);

    void InstancesHeader(ImVec2 StartPos, float HeaderWidth);
    void RenderNavItem(
        const char* Icon, const char* Label, int TabIdx, float ItemWidth, float ItemHeight
    );
    void InstanceGroupsSection(float AvailableWidth);
    void ActiveInstancesSection(float AvailableWidth);
    void TrustedNodesSection(float AvailableWidth);

    GroupCardAction InstanceGroupCard(
        const OmniInstanceGroup& Group,
        size_t CardIdx,
        ImVec2 CardPos,
        float CardWidth,
        float CardHeight
    );

    ActiveCardAction ActiveInstanceCard(
        DeviceMap DevId,
        const OmniInstance& Instance,
        size_t CardIdx,
        ImVec2 CardPos,
        float CardWidth,
        float CardHeight
    );

    TrustedCardAction TrustedNodeCard(
        DeviceMap DevId,
        const std::array<uint8_t, 32>& Token,
        const OmniInstance* Instance,
        size_t CardIdx,
        ImVec2 CardPos,
        float CardWidth,
        float CardHeight
    );

    void KeycapPills(
        ImFont* KeyFont,
        const std::vector<const char*>& Keys,
        float RowMinY,
        float RowMaxX,
        float RowHeight,
        bool HoverState
    );
    void RenderKeybindRow(
        const KeybindItem& Item,
        size_t ItemIdx,
        size_t CategoryIndex,
        size_t TotalItems,
        ImVec2 RowMin,
        ImVec2 RowMax,
        float RowHeight
    );
    void
    KeybindCategoryCard(const KeybindCategoryGroup& Category, size_t catIdx, float AvailableWidth);

    void NetworkSettingsSection();
    void StreamingSettingsSection(int* currentFPSIdx);
    void InterfaceSettingsSection();

    // Da tabs..
    void NexusTab();
    void InstancesTab();
    void KeybindsTab();
    void SettingsTab();

    // Modal & Config State
    bool ShowConnectModal = false;
    DeviceMap TargetSlotForAdd = DeviceMap::C0;
    char ManualIPBuffer[32] = {};

    int ConfigPort = 62485;
    int ConfigDiscoveryPort = 58426;
    bool ConfigAutoProbe = true;
    int ConfigTargetFPS = 60;
    bool ConfigLockCursor = true;
    bool ConfigClipboardSync = true;
    int ConfigEdgeSensitivity = 5;
    bool ConfigToastOverlay = true;

    // Instance Management Tab State
    int InstancesSubTab = 0;
    char NewGroupNameBuffer[64] = "";
    int SelectedGroupIndex = 0;

    void HandshakeEventHeader(
        ImDrawList* DrawList,
        ImVec2 WindowPos,
        float WindowWidth,
        const char* HeaderTitle,
        const char* DeviceName,
        const char* SubText,
        const char* Key,
        int Port,
        float Timeout
    );

    // Notification Event Handlers
    bool HandleEvent(HandshakeWaitEvent& Request, float Timeout);
    bool HandleEvent(HandshakeConfirmEvent& Request, float Timeout);
    bool HandleEvent(Alert& Request, float Timeout);

    void CenterItemX(const float ItemWidth);
    void CreateCurvedLine(const char* label, int curve);

    // Just.. make sure to keep a HandleEvent func ready.
    // HandleEvent must return true to be exterminated.
    // BeginPopupModal returns true as long as if not clicked or timedout.
    bool NotificationWindow(const char* label, Notification& notification)
    {
        bool Exterminate = false;

        if (notification.Layout == Notification::EventLayout::CENTER) {
            if (notification.Active) {
                ImGui::OpenPopup(label);
                notification.Active = false;
            }

            ImVec2 WindowPos = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(WindowPos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(390.0f, 0.0f));

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 18.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, COL_BG_CHILD_1);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.14f, 0.14f, 0.20f, 1.0f));

            bool Clicked = false;
            ImGuiWindowFlags modalFlags = DefaultFlags | ImGuiWindowFlags_AlwaysAutoResize;

            if (ImGui::BeginPopupModal(label, NULL, modalFlags)) {

                notification.Timeout -= ImGui::GetIO().DeltaTime;
                if (notification.Timeout <= 0.0f) {
                    Exterminate = true;
                }

                Clicked = std::visit(
                    [&](auto& args) { return HandleEvent(args, notification.Timeout); },
                    notification.Event
                );
                if (Clicked)
                    Exterminate = true;

                if (Exterminate)
                    ImGui::CloseCurrentPopup();

                ImGui::EndPopup();
            } else {
                if (!notification.Active) {
                    Exterminate = true;
                }
            }

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(3);
        } else {
            const ImGuiViewport* Viewport = ImGui::GetMainViewport();
            ImVec2 Size = ImVec2(340.0f, 72.0f);
            float Padding = 14.0f;
            ImVec2 WindowPos;
            WindowPos.x = Viewport->WorkPos.x + Viewport->WorkSize.x - Size.x - Padding;
            WindowPos.y = Viewport->WorkPos.y + Viewport->WorkSize.y - Size.y - Padding;

            ImGui::SetNextWindowPos(WindowPos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(Size);

            ImGuiWindowFlags WindowFlags =
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, COL_BG_CHILD_1);
            ImGui::PushStyleColor(ImGuiCol_Border, COL_MENU_STRIP);

            bool Clicked = false;

            if (ImGui::Begin(label, NULL, WindowFlags)) {

                notification.Timeout -= ImGui::GetIO().DeltaTime;
                if (notification.Timeout <= 0.0f) {
                    Exterminate = true;
                }

                Clicked = std::visit(
                    [&](auto& args) { return HandleEvent(args, notification.Timeout); },
                    notification.Event
                );
                if (Clicked)
                    Exterminate = true;

                ImGui::End();
            } else {
                notification.Timeout -= ImGui::GetIO().DeltaTime;
                if (notification.Timeout <= 0.0f) {
                    Exterminate = true;
                }
            }

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(3);
        }

        return Exterminate;
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
        style.WindowRounding = 8.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        if (ImGui::Begin("OmniLink", &ImGuiState, ImGuiWindowFlags_NoTitleBar)) {

            ImGui::PopStyleVar();
            DrawList = ImGui::GetWindowDrawList();

            ImVec2 SideMenuPos = ImGui::GetCursorScreenPos();
            float SideMenuHeight = ImGui::GetContentRegionAvail().y;
            DrawList->AddRectFilled(
                SideMenuPos,
                ImVec2(SideMenuPos.x + 110.0f, SideMenuPos.y + SideMenuHeight),
                COL_BG_CHILD_1,
                10.0f,
                ImDrawFlags_RoundCornersLeft
            );

            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32_BLACK_TRANS);

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
                NexusTab();
                break;

            case 1:
                InstancesTab();
                break;

            case 2:
                KeybindsTab();
                break;

            case 3:
                SettingsTab();
                break;
            }

            ImGui::PopStyleVar();

            if (auto* activeNotif = NotificationQueue.peek()) {
                if (activeNotif->Cancelled &&
                    activeNotif->Cancelled->load(std::memory_order_relaxed)) {
                    NotificationQueue.pop();
                } else if (NotificationWindow(activeNotif->EventName, *activeNotif)) {
                    NotificationQueue.pop();
                }
            }

            RenderConnectModal();
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
