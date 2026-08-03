#include "OmniGUI.h"

void OmniGUI::RenderSettingsTab()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));
    ImGui::BeginChild(
        "SettingsTabChild", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding
    );

    ImGui::PushFont(InterBold20 ? InterBold20 : InterMed16);
    ImGui::TextColored(COL4_TEXT_ACTIVE, "OmniLink Project Settings");
    ImGui::PopFont();
    ImGui::PushFont(InterReg14);
    ImGui::TextColored(COL4_TEXT_MUTED, "Network ports, framerate limits, and UI preferences");
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    auto RenderToggleSwitch = [&](const char* StrId, bool* Val) {
        ImVec2 Pos = ImGui::GetCursorScreenPos();
        float Width = 44.0f;
        float Height = 24.0f;
        float Radius = Height * 0.5f;

        ImGui::InvisibleButton(StrId, ImVec2(Width, Height));
        if (ImGui::IsItemClicked()) {
            *Val = !(*Val);
        }

        bool HoverState = ImGui::IsItemHovered();

        ImU32 BgCol = *Val
                          ? (HoverState ? IM_COL32(160, 95, 255, 255) : IM_COL32(140, 80, 240, 255))
                          : (HoverState ? IM_COL32(40, 36, 60, 255) : IM_COL32(28, 25, 42, 255));
        ImU32 BorderCol =
            *Val ? IM_COL32(180, 120, 255, 255)
                 : (HoverState ? IM_COL32(75, 60, 110, 255) : IM_COL32(56, 44, 80, 255));
        ImU32 KnobCol =
            *Val ? IM_COL32(255, 255, 255, 255)
                 : (HoverState ? IM_COL32(210, 200, 230, 255) : IM_COL32(160, 150, 185, 255));

        DrawList->AddRectFilled(Pos, ImVec2(Pos.x + Width, Pos.y + Height), BgCol, Radius);
        DrawList->AddRect(Pos, ImVec2(Pos.x + Width, Pos.y + Height), BorderCol, Radius, 0, 1.2f);

        float KnobRadius = 7.5f;
        float KnobX = *Val ? (Pos.x + Width - 4.5f - KnobRadius) : (Pos.x + 4.5f + KnobRadius);
        float KnobY = Pos.y + Height * 0.5f;
        DrawList->AddCircleFilled(ImVec2(KnobX, KnobY), KnobRadius, KnobCol);
    };

    auto RenderSettingRow = [&](const char* Title,
                                const char* Subtitle,
                                float ControlWidth,
                                auto DrawControl,
                                bool ShowSeparator = true) {
        ImVec2 StartPos = ImGui::GetCursorScreenPos();
        float AvailableWidth = ImGui::GetContentRegionAvail().x;

        ImGui::SetCursorScreenPos(ImVec2(StartPos.x + 16.0f, StartPos.y + 10.0f));
        ImGui::BeginGroup();
        ImGui::PushFont(InterMed15);
        ImGui::TextColored(COL4_TEXT_ACTIVE, "%s", Title);
        ImGui::PopFont();
        if (Subtitle && Subtitle[0] != '\0') {
            ImGui::PushFont(InterReg14);
            ImGui::TextColored(COL4_TEXT_MUTED, "%s", Subtitle);
            ImGui::PopFont();
        }
        ImGui::EndGroup();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(28, 25, 42, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(40, 36, 60, 255));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(50, 44, 76, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(56, 44, 80, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(240, 235, 255, 255));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(36, 32, 54, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(60, 50, 90, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(80, 65, 120, 255));
        ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(60, 45, 95, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(90, 65, 140, 255));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(120, 85, 180, 255));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(24, 22, 38, 255));

        ImGui::SetCursorScreenPos(
            ImVec2(StartPos.x + AvailableWidth - ControlWidth - 16.0f, StartPos.y + 10.0f)
        );
        ImGui::SetNextItemWidth(ControlWidth);
        ImGui::PushFont(InterMed14);
        DrawControl();
        ImGui::PopFont();

        ImGui::PopStyleColor(12);
        ImGui::PopStyleVar(3);

        ImGui::SetCursorScreenPos(ImVec2(StartPos.x, StartPos.y + 60.0f));
        ImGui::Dummy(ImVec2(0, 0));
        if (ShowSeparator) {
            DrawList->AddLine(
                ImVec2(StartPos.x + 16.0f, StartPos.y + 60.0f),
                ImVec2(StartPos.x + AvailableWidth - 16.0f, StartPos.y + 60.0f),
                IM_COL32(255, 255, 255, 12)
            );
        }
    };

    auto BeginGroupCard = [&](const char* Icon, const char* Title, float Height) {
        ImVec2 Pos = ImGui::GetCursorScreenPos();
        float AvailableWidth = ImGui::GetContentRegionAvail().x;

        DrawList->AddRectFilled(
            Pos, ImVec2(Pos.x + AvailableWidth, Pos.y + Height), COL_BG_CHILD_1, 10.0f
        );
        DrawList->AddRect(Pos, ImVec2(Pos.x + AvailableWidth, Pos.y + Height), COL_BORDER, 10.0f);

        ImGui::SetCursorScreenPos(ImVec2(Pos.x + 16.0f, Pos.y + 14.0f));
        ImGui::PushFont(OmniIconsMedium);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(COL_FEAT_TINT_ACT), "%s", Icon);
        ImGui::PopFont();
        ImGui::SameLine(0.0f, 8.0f);
        ImGui::PushFont(InterBold18 ? InterBold18 : InterMed16);
        ImGui::TextColored(COL4_TEXT_ACTIVE, "%s", Title);
        ImGui::PopFont();

        ImGui::SetCursorScreenPos(ImVec2(Pos.x, Pos.y + 45.0f));
    };

    // Network & Protocol
    ImVec2 Card1Pos = ImGui::GetCursorScreenPos();
    BeginGroupCard(IC_NETWORK, "Network & Protocol", 230.0f);

    ImGui::PushID("Net1");
    RenderSettingRow(
        "UDP Streaming Port", "Primary port used for low-latency session streams", 100.0f, [&]() {
            ImGui::InputInt("##UDPPort", &ConfigPort, 0, 0);
        }
    );
    ImGui::PopID();

    ImGui::PushID("Net2");
    RenderSettingRow(
        "Discovery Broadcast Port",
        "Beacon port used for automatic instance discovery",
        100.0f,
        [&]() { ImGui::InputInt("##DiscPort", &ConfigDiscoveryPort, 0, 0); }
    );
    ImGui::PopID();

    ImGui::PushID("Net3");
    RenderSettingRow(
        "Automatic Background Probe",
        "Periodically scan local network for new devices",
        44.0f,
        [&]() { RenderToggleSwitch("##AutoProbe", &ConfigAutoProbe); },
        false
    );
    ImGui::PopID();

    ImGui::SetCursorScreenPos(ImVec2(Card1Pos.x, Card1Pos.y + 245.0f));
    ImGui::Dummy(ImVec2(0, 0));

    // Streaming & Framerate
    ImVec2 Card2Pos = ImGui::GetCursorScreenPos();
    BeginGroupCard(IC_GAUGE, "Streaming & Framerate", 110.0f);

    const char* FpsOptions[] = {"30 FPS", "60 FPS", "75 FPS", "90 FPS", "120 FPS", "144 FPS"};
    static int CurrentFPSIdx = 1;

    ImGui::PushID("FPS1");
    RenderSettingRow(
        "Target Framerate Cap",
        "Maximum frame rate cap for streams",
        110.0f,
        [&]() {
            if (ImGui::Combo("##FPSCap", &CurrentFPSIdx, FpsOptions, IM_ARRAYSIZE(FpsOptions))) {
                if (CurrentFPSIdx == 0)
                    ConfigTargetFPS = 30;
                else if (CurrentFPSIdx == 1)
                    ConfigTargetFPS = 60;
                else if (CurrentFPSIdx == 2)
                    ConfigTargetFPS = 75;
                else if (CurrentFPSIdx == 3)
                    ConfigTargetFPS = 90;
                else if (CurrentFPSIdx == 4)
                    ConfigTargetFPS = 120;
                else if (CurrentFPSIdx == 5)
                    ConfigTargetFPS = 144;
            }
        },
        false
    );
    ImGui::PopID();

    ImGui::SetCursorScreenPos(ImVec2(Card2Pos.x, Card2Pos.y + 125.0f));
    ImGui::Dummy(ImVec2(0, 0));

    // Interface & Notifications
    ImVec2 Card3Pos = ImGui::GetCursorScreenPos();
    BeginGroupCard(IC_BELL, "Interface & Notifications", 110.0f);

    ImGui::PushID("UI1");
    RenderSettingRow(
        "Enable Notifications",
        "Enable or disable toast notifications and event alerts",
        44.0f,
        [&]() { RenderToggleSwitch("##EnableNotifications", &ConfigToastOverlay); },
        false
    );
    ImGui::PopID();

    ImGui::SetCursorScreenPos(ImVec2(Card3Pos.x, Card3Pos.y + 125.0f));
    ImGui::Dummy(ImVec2(0, 0));

    ImGui::EndChild();
    ImGui::PopStyleVar();
}
