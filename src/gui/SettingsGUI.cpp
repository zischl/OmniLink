#include "OmniGUI.h"

void OmniGUI::SettingsTab()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));
    ImGui::BeginChild(
        "SettingsTabChild", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding
    );

    static int CurrentFPSIdx = 1;

    // Network & Protocol Section
    NetworkSettingsSection();

    // Streaming & Framerate Section
    StreamingSettingsSection(&CurrentFPSIdx);

    // Interface & Notifications Section
    InterfaceSettingsSection();

    ImGui::EndChild();
    ImGui::PopStyleVar();
}
