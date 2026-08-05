#include "OmniGUI.h"

#define IC_NETWORK "\xef\x80\x8e"
#define IC_GAUGE "\xef\x80\x97"
#define IC_BELL "\xef\x80\x8f"

void OmniGUI::NetworkSettingsSection()
{
    ImVec2 CardPos = ImGui::GetCursorScreenPos();
    BeginGroupCard(IC_NETWORK, "Network & Protocol", 230.0f);

    ImGui::PushID("Net1");
    SettingRow(
        "UDP Streaming Port",
        "Primary port used for low-latency session streams",
        100.0f,
        [this]() { ImGui::InputInt("##UDPPort", &ConfigPort, 0, 0); }
    );
    ImGui::PopID();

    ImGui::PushID("Net2");
    SettingRow(
        "Discovery Broadcast Port",
        "Beacon port used for automatic instance discovery",
        100.0f,
        [this]() { ImGui::InputInt("##DiscPort", &ConfigDiscoveryPort, 0, 0); }
    );
    ImGui::PopID();

    ImGui::PushID("Net3");
    SettingRow(
        "Automatic Background Probe",
        "Periodically scan local network for new devices",
        44.0f,
        [this]() { ToggleSwitch("##AutoProbe", &ConfigAutoProbe); },
        false
    );
    ImGui::PopID();

    ImGui::SetCursorScreenPos(ImVec2(CardPos.x, CardPos.y + 245.0f));
    ImGui::Dummy(ImVec2(0, 0));
}

void OmniGUI::StreamingSettingsSection(int* currentFPSIdx)
{
    ImVec2 cardPos = ImGui::GetCursorScreenPos();
    BeginGroupCard(IC_GAUGE, "Streaming & Framerate", 110.0f);

    static const char* FpsOptions[] = {
        "30 FPS", "60 FPS", "75 FPS", "90 FPS", "120 FPS", "144 FPS"
    };

    ImGui::PushID("FPS1");
    SettingRow(
        "Target Framerate Cap",
        "Maximum frame rate cap for streams",
        110.0f,
        [this, currentFPSIdx]() {
            if (ImGui::Combo("##FPSCap", currentFPSIdx, FpsOptions, IM_ARRAYSIZE(FpsOptions))) {
                if (*currentFPSIdx == 0)
                    ConfigTargetFPS = 30;
                else if (*currentFPSIdx == 1)
                    ConfigTargetFPS = 60;
                else if (*currentFPSIdx == 2)
                    ConfigTargetFPS = 75;
                else if (*currentFPSIdx == 3)
                    ConfigTargetFPS = 90;
                else if (*currentFPSIdx == 4)
                    ConfigTargetFPS = 120;
                else if (*currentFPSIdx == 5)
                    ConfigTargetFPS = 144;
            }
        },
        false
    );
    ImGui::PopID();

    ImGui::SetCursorScreenPos(ImVec2(cardPos.x, cardPos.y + 125.0f));
    ImGui::Dummy(ImVec2(0, 0));
}

void OmniGUI::InterfaceSettingsSection()
{
    ImVec2 cardPos = ImGui::GetCursorScreenPos();
    BeginGroupCard(IC_BELL, "Interface & Notifications", 110.0f);

    ImGui::PushID("UI1");
    SettingRow(
        "Enable Notifications",
        "Enable or disable system wide notifications and event alerts",
        44.0f,
        [this]() { ToggleSwitch("##EnableNotifications", &ConfigToastOverlay); },
        false
    );
    ImGui::PopID();

    ImGui::SetCursorScreenPos(ImVec2(cardPos.x, cardPos.y + 125.0f));
    ImGui::Dummy(ImVec2(0, 0));
}

