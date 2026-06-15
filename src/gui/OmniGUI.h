#ifndef OMNIGUI_H
#define OMNIGUI_H

#include "OmniEnums.h"
#include "OmniPackets.h"
#include <variant>
#include <vector>
#pragma once

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
    ImFont* OmniIcons = nullptr;
    ImFont* OmniIconsSmall = nullptr;

    bool IconizedButton(const char* Label, const char* Icon, bool state, const ImVec2& ButtonSize);
    bool VerticalMenuItem(const char* label, const char* icon, bool state, ImVec2& MenuItemSize);
    void ConnectionRing(const char* label);
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
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.074f, 0.082f, 0.121f, 1.0f));
            DrawList = ImGui::GetWindowDrawList();

            ImVec2 MenuItemSize = ImVec2(110, 100);
            ImGui::BeginChild("SideMenu", ImVec2(110, 0), ImGuiChildFlags_None);
            {
                ImGui::Dummy(ImVec2(0, 50));
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

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::BeginChild("menu-item");
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
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.074f, 0.082f, 0.121f, 1.0f));

                    ImGui::BeginChild("FeaturePanel", ImVec2(0, 120), ImGuiChildFlags_None);

                    ImGui::TextColored(ImVec4(0.239f, 0.220f, 0.333f, 1.0f),
                                       "\n  M  \n  O  \n  D  \n  E  ");

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

                    ImGui::EndChild();
                    ImGui::PopStyleVar(2);
                    ImGui::PopStyleColor();

                    ImGui::BeginChild("connections", ImVec2(0, 0), ImGuiChildFlags_None);

                    DrawList = ImGui::GetWindowDrawList();

                    ConnectionRing("ConRing");

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

                    ImGui::EndChild();
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

    inline void DeviceIconPreview(const ImVec2& pos,
                                  const ImU32& col,
                                  const ImVec2& text_size = ImVec2{0, 0},
                                  const char* text = "")
    {
        DrawList->AddRect(ImVec2(pos.x - 50, pos.y - 40),
                          ImVec2(pos.x + 50, pos.y + 40),
                          col,
                          5.0f,
                          0,
                          2.0f); // monitor
        DrawList->AddText(ImVec2(pos.x - (text_size.x * 0.5f), pos.y), col, text);
        DrawList->AddRect(
            ImVec2(pos.x, pos.y + 40), ImVec2(pos.x, pos.y + 55), col, 5.0f, 0, 2.0f); // handle
        DrawList->AddRect(ImVec2(pos.x - 15, pos.y + 55),
                          ImVec2(pos.x + 15, pos.y + 55),
                          col,
                          10.0f,
                          0,
                          1.0f); // stand
    }

    inline void DeviceIcon(const char* label,
                           const ImVec2& pos,
                           const ImVec2& text_size,
                           const OmniInstance* DeviceData)
    {
        ImGui::PushID(label);

        const ImGuiID id = ImGui::GetID(label);
        ImRect bb(ImVec2(pos.x - 50, pos.y - 40), ImVec2(pos.x + 50, pos.y + 55));
        ImGui::ItemAdd(bb, id, NULL, ImGuiItemFlags_None);

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);
        ImGui::RenderNavCursor(bb, id);

        ImU32 col = hovered || held ? IM_COL32(128, 0, 255, 255) : IM_COL32(255, 255, 255, 255);

        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload(
                "DeviceInfo", &(DeviceData->DevMapIndex), sizeof(DeviceData->DevMapIndex));
            DeviceIconPreview(ImGui::GetCursorScreenPos(), col, text_size, DeviceData->IPv4_String);
            ImGui::EndDragDropSource();
        } else if (ImGui::BeginDragDropTarget()) {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DeviceInfo");
            if (payload != nullptr) {
                const uint8_t data = *static_cast<uint8_t*>(payload->Data);
                OmniAPI::SwapDeviceLayout(data, DeviceData->DevMapIndex);
            }
            // test animation : device order swap (failed -_- )
            /*else {
                    ImVec2 cpos = ImGui::GetCursorScreenPos();
                    pos.y += cpos.y < pos.x ? -(pos.y - cpos.y) : pos.y - cpos.y;
                    pos.x += cpos.x < pos.x ? pos.x - cpos.x : -(pos.x - cpos.x);

            }*/
            ImGui::EndDragDropTarget();
        }

        DrawList->AddRect(ImVec2(pos.x - 50, pos.y - 40),
                          ImVec2(pos.x + 50, pos.y + 40),
                          col,
                          5.0f,
                          0,
                          2.0f); // monitor
        DrawList->AddText(
            ImVec2(pos.x - (text_size.x * 0.5f), pos.y), col, DeviceData->IPv4_String);
        DrawList->AddRect(
            ImVec2(pos.x, pos.y + 40), ImVec2(pos.x, pos.y + 55), col, 5.0f, 0, 2.0f); // handle
        DrawList->AddRect(ImVec2(pos.x - 15, pos.y + 55),
                          ImVec2(pos.x + 15, pos.y + 55),
                          col,
                          10.0f,
                          0,
                          1.0f); // stand

        if (ImGui::BeginPopupContextItem("MyItemContextMenu")) {
            if (ImGui::MenuItem("Connect Instance")) {
                ConnectionRequest request;
                request.DeviceID = static_cast<DeviceMap>(DeviceData->DevMapIndex);
                OmniAPI::Connect(request);
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }
};

#endif
