#include "OmniGUI.h"

void OmniGUI::RenderFeatureControlBar(
    uint32_t featureFlags,
    const ImVec2& contentSpaceSize,
    const ImVec2& contentSpaceStart
)
{
    static constexpr ImU32 COL_BG_CHILD_1 = IM_COL32(19, 21, 31, 255);
    static constexpr ImVec4 COL4_TEXT_MUTED = ImVec4(0.600f, 0.580f, 0.700f, 1.0f);

    ImGui::BeginGroup();

    const float featurePanelHeight = 92.0f;
    const float maxContentPosX = contentSpaceStart.x + contentSpaceSize.x;

    ImVec2 panelP1 = ImGui::GetCursorScreenPos();
    DrawList->AddRectFilled(
        panelP1, ImVec2(maxContentPosX, panelP1.y + featurePanelHeight), COL_BG_CHILD_1
    );

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

    // Feature Mode Text
    const char* modeText = "    M    \n    O    \n    D    \n    E    ";
    const ImVec2 modeTextSize = ImGui::CalcTextSize(modeText);
    float modeTextPadding = (featurePanelHeight - modeTextSize.y) * 0.5f;

    ImGui::BeginGroup();
    ImGui::Dummy(ImVec2(0.0f, modeTextPadding));
    ImGui::TextColored(COL4_TEXT_MUTED, "%s", modeText);
    ImGui::Dummy(ImVec2(0.0f, modeTextPadding));
    ImGui::EndGroup();

    ImGui::SameLine(0.0f, 0.0f);
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine(0.0f, 0.0f);

    ImGui::PopStyleVar();

    ImVec2 size = ImVec2(1 + ((contentSpaceSize.x - modeTextSize.x) / 5), featurePanelHeight);

    if (InterMed16)
        ImGui::PushFont(InterMed16);

    if (IconizedButton(
            "Screen Link", IC_SCREEN_SHARE, (featureFlags & FeatureFlags::fScreenLink) != 0, size
        )) {
        OmniAPI::ToggleFeature(FeatureTypes::ScreenLink, SelectedDevice);
    }
    ImGui::SameLine(0.0f, 0.0f);

    if (IconizedButton(
            "Window Link", IC_APP_WINDOW, (featureFlags & FeatureFlags::fWindowLink) != 0, size
        )) {
        OmniAPI::ToggleFeature(FeatureTypes::WindowLink, SelectedDevice);
    }
    ImGui::SameLine(0.0f, 0.0f);

    if (IconizedButton(
            "Input Link", IC_MOUSE, (featureFlags & FeatureFlags::fInputLink) != 0, size
        )) {
        OmniAPI::ToggleFeature(FeatureTypes::InputLink, SelectedDevice);
    }
    ImGui::SameLine(0.0f, 0.0f);

    if (IconizedButton(
            "Audio Link", IC_VOLUME_2, (featureFlags & FeatureFlags::fAudioLink) != 0, size
        )) {
        OmniAPI::ToggleFeature(FeatureTypes::AudioLink, SelectedDevice);
    }
    ImGui::SameLine(0.0f, 0.0f);

    if (IconizedButton(
            "Clipboard Link", IC_CLIPBOARD, (featureFlags & FeatureFlags::fClipBoardLink) != 0, size
        )) {
        // Reserved for Clipboard event handler
    }

    if (InterMed16)
        ImGui::PopFont();
    ImGui::EndGroup();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
    ImGui::PopStyleVar();
}

void OmniGUI::RenderMetricsBar(
    int deviceCount,
    size_t activeCount,
    float availableWidth
)
{
    static char availableBuf[16];
    static char activeBuf[32];

    std::to_chars(availableBuf, availableBuf + sizeof(availableBuf), deviceCount);

    size_t displayActive = activeCount > 0 ? activeCount - 1 : 0;
    auto [ptr, ec] = std::to_chars(activeBuf, activeBuf + sizeof(activeBuf), displayActive);

    *ptr = '/';
    *(ptr + 1) = '8';
    *(ptr + 2) = '\0';

    static MetricItem staticMetrics[] = {
        {"Available", availableBuf},
        {"Active", activeBuf},
        {"Latency", "7.6ms"},
        {"Bandwith", "1.2 MB/s"}
    };

    MetricDashboard("NetContainer", staticMetrics, 4, availableWidth, 55.0f);
}
