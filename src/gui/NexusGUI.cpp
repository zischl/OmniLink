#include "OmniGUI.h"

void OmniGUI::NexusTab()
{
    const ImVec2 ContentSpaceSize = ImGui::GetContentRegionAvail();
    const ImVec2 ContentSpaceStart = ImGui::GetCursorScreenPos();
    DrawList = ImGui::GetWindowDrawList();

    const uint32_t FeatureFlags = (ActiveInstances && ActiveInstances->contains(SelectedDevice))
                                      ? ActiveInstances->at(SelectedDevice).ActiveFlags
                                      : 0;

    // Feature Link Control Bar
    RenderFeatureControlBar(FeatureFlags, ContentSpaceSize, ContentSpaceStart);

    // Spatial Device Connection Ring
    const ImVec2 AvailableSpace = ImGui::GetContentRegionAvail();
    int DeviceCount =
        ConnectionRing("DiscoveryRing", ImVec2(AvailableSpace.x, AvailableSpace.y - 60), 205);

    // Metrics Dashboard Status Bar
    size_t ActiveCount = ActiveInstances ? ActiveInstances->size() : 1;
    RenderMetricsBar(DeviceCount, ActiveCount, AvailableSpace.x);
}
