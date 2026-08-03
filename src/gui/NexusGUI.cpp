#include "OmniGUI.h"
#include <charconv>

void OmniGUI::RenderNexusTab()
{
    // Feature Panel
    ImGui::BeginGroup();

    const float FeaturePanelHeight = 92.0f;

    const ImVec2 ContentSpaceSize = ImGui::GetContentRegionAvail();
    const ImVec2 ContentSpaceStart = ImGui::GetCursorScreenPos();
    const float MaxContentPosX = ContentSpaceStart.x + ContentSpaceSize.x;

    ImVec2 PanelP1 = ImGui::GetCursorScreenPos();
    DrawList->AddRectFilled(
        PanelP1, ImVec2(MaxContentPosX, PanelP1.y + FeaturePanelHeight), COL_BG_CHILD_1
    );

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

    // Feature Mode Text
    const char* ModeText = "    M    \n    O    \n    D    \n    E    ";
    const ImVec2 ModeTextSize = ImGui::CalcTextSize(ModeText);
    float ModeTextPadding = (FeaturePanelHeight - ModeTextSize.y) * 0.5f;

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

    const uint32_t FeatureSates =
        (ActiveInstances && ActiveInstances->contains(SelectedDevice))
            ? ActiveInstances->at(SelectedDevice).ActiveFlags
            : 0;
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
            "Input Link", IC_MOUSE, (FeatureSates & FeatureFlags::fInputLink) != 0, size
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
}
