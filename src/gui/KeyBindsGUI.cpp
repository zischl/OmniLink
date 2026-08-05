#include "OmniGUI.h"

void OmniGUI::KeybindsTab()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));
    ImGui::BeginChild(
        "KeybindsTabChild", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding
    );

    static const std::vector<KeybindCategoryGroup> Categories = {
        {IC_LINK,
         "Link Controls",
         {{"Toggle Screen Link", {"CTRL", "ALT", "S"}},
          {"Toggle Input Link", {"CTRL", "ALT", "I"}},
          {"Toggle Window Link", {"CTRL", "ALT", "W"}},
          {"Toggle Audio Link", {"CTRL", "ALT", "A"}},
          {"Toggle Clipboard Link", {"CTRL", "ALT", "C"}}}},
        {IC_SLIDERS,
         "Basic Controls",
         {{"Show/Hide OmniLink", {"CTRL", "SHIFT", "O"}},
          {"Toggle Seamless Cursor", {"CTRL", "ALT", "M"}},
          {"Switch Cursor to Device (1–9)", {"CTRL", "ALT", "1...9"}}}}
    };

    const float AvailableWidth = ImGui::GetContentRegionAvail().x;

    for (size_t catIdx = 0; catIdx < Categories.size(); ++catIdx) {
        KeybindCategoryCard(Categories[catIdx], catIdx, AvailableWidth);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}
