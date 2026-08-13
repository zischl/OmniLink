#include "OmniGUI.h"

bool OmniGUI::IconButton(
    const char* Icon, const char* Label, const ImVec2& Size, const ButtonColors& Colors
)
{
    ImFont* LabelFont = InterMed15 ? InterMed15 : InterMed14;
    ImFont* IconFont = OmniIconsSmall;
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    ImVec2 CursorPos = ImGui::GetCursorScreenPos();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, Colors.BorderSize);
    ImGui::PushStyleColor(ImGuiCol_Button, Colors.Normal);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors.Hovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, Colors.Active);
    ImGui::PushStyleColor(ImGuiCol_Border, Colors.Border);
    ImGui::PushStyleColor(ImGuiCol_Text, Colors.Text);

    char ButtonLabel[128];
    snprintf(ButtonLabel, sizeof(ButtonLabel), "     %s", Label);

    if (LabelFont)
        ImGui::PushFont(LabelFont);
    bool Clicked = ImGui::Button(ButtonLabel, Size);
    if (LabelFont)
        ImGui::PopFont();

    if (Icon && Icon[0] != '\0' && IconFont) {
        ImGui::PushFont(IconFont);
        ImVec2 IconSize = ImGui::CalcTextSize(Icon);
        DrawList->AddText(
            ImVec2(CursorPos.x + 14.0f, CursorPos.y + (Size.y - IconSize.y) * 0.5f),
            Colors.Text,
            Icon
        );
        ImGui::PopFont();
    }

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
    return Clicked;
}

void OmniGUI::SectionHeader(const char* Icon, const char* Title, const char* Subtitle)
{
    static constexpr ImVec4 COL4_TEXT_MUTED = ImVec4(0.600f, 0.580f, 0.700f, 1.0f);
    static constexpr ImVec4 COL4_TEXT_ACTIVE = ImVec4(0.753f, 0.722f, 0.831f, 1.0f);

    ImFont* IconFont = OmniIconsMedium;
    ImFont* TitleFont = InterBold18 ? InterBold18 : InterMed16;
    ImFont* SubtitleFont = InterReg14;

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    if (Icon && Icon[0] != '\0' && IconFont) {
        ImGui::PushFont(IconFont);
        ImGui::TextColored(ImVec4(0.530f, 0.440f, 0.960f, 1.0f), "%s", Icon);
        ImGui::PopFont();
        ImGui::SameLine(0.0f, 10.0f);
    }
    if (TitleFont)
        ImGui::PushFont(TitleFont);
    ImGui::TextColored(COL4_TEXT_ACTIVE, "%s", Title);
    if (TitleFont)
        ImGui::PopFont();

    if (Subtitle && Subtitle[0] != '\0') {
        ImGui::Dummy(ImVec2(0.0f, 3.0f));
        if (SubtitleFont)
            ImGui::PushFont(SubtitleFont);
        ImGui::TextColored(COL4_TEXT_MUTED, "%s", Subtitle);
        if (SubtitleFont)
            ImGui::PopFont();
    }
    ImGui::Dummy(ImVec2(0.0f, 12.0f));
}

void OmniGUI::Mini3x3GridWidget(ImVec2 GridTopLeft, int SlotIdx)
{
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    struct GridSlotPos
    {
        int Row;
        int Col;
    };
    static const GridSlotPos SlotGridMap[9] = {
        {1, 1}, // C0 Center
        {1, 0}, // L1 Left
        {0, 1}, // U1 Up
        {1, 2}, // R1 Right
        {2, 1}, // D1 Down
        {0, 0}, // LU1 Top-Left
        {0, 2}, // RU1 Top-Right
        {2, 2}, // RD1 Bot-Right
        {2, 0}  // LD1 Bot-Left
    };

    float CellSize = 7.0f;
    float CellGap = 2.0f;
    int TargetRow = (SlotIdx >= 0 && SlotIdx < 9) ? SlotGridMap[SlotIdx].Row : 1;
    int TargetCol = (SlotIdx >= 0 && SlotIdx < 9) ? SlotGridMap[SlotIdx].Col : 1;

    for (int R = 0; R < 3; ++R) {
        for (int C = 0; C < 3; ++C) {
            ImVec2 CMin = ImVec2(
                GridTopLeft.x + C * (CellSize + CellGap), GridTopLeft.y + R * (CellSize + CellGap)
            );
            ImVec2 CMax = ImVec2(CMin.x + CellSize, CMin.y + CellSize);

            if (R == TargetRow && C == TargetCol) {
                DrawList->AddRectFilled(CMin, CMax, IM_COL32(168, 85, 247, 255), 2.0f);
                DrawList->AddRect(CMin, CMax, IM_COL32(235, 200, 255, 255), 2.0f, 0, 1.0f);
            } else {
                DrawList->AddRectFilled(CMin, CMax, IM_COL32(32, 28, 48, 220), 2.0f);
                DrawList->AddRect(CMin, CMax, IM_COL32(56, 46, 75, 255), 2.0f, 0, 0.8f);
            }
        }
    }
}

void OmniGUI::ToggleSwitch(const char* StrId, bool* Val)
{
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    ImVec2 CursorPos = ImGui::GetCursorScreenPos();
    float Width = 44.0f;
    float Height = 24.0f;
    float Radius = Height * 0.5f;

    ImGui::InvisibleButton(StrId, ImVec2(Width, Height));
    if (ImGui::IsItemClicked()) {
        *Val = !(*Val);
    }

    bool HoverState = ImGui::IsItemHovered();

    ImU32 BgCol = *Val ? (HoverState ? IM_COL32(160, 95, 255, 255) : IM_COL32(140, 80, 240, 255))
                       : (HoverState ? IM_COL32(40, 36, 60, 255) : IM_COL32(28, 25, 42, 255));
    ImU32 BorderCol = *Val ? IM_COL32(180, 120, 255, 255)
                           : (HoverState ? IM_COL32(75, 60, 110, 255) : IM_COL32(56, 44, 80, 255));
    ImU32 KnobCol =
        *Val ? IM_COL32(255, 255, 255, 255)
             : (HoverState ? IM_COL32(210, 200, 230, 255) : IM_COL32(160, 150, 185, 255));

    DrawList->AddRectFilled(
        CursorPos, ImVec2(CursorPos.x + Width, CursorPos.y + Height), BgCol, Radius
    );
    DrawList->AddRect(
        CursorPos, ImVec2(CursorPos.x + Width, CursorPos.y + Height), BorderCol, Radius, 0, 1.2f
    );

    float KnobRadius = 7.5f;
    float KnobX =
        *Val ? (CursorPos.x + Width - 4.5f - KnobRadius) : (CursorPos.x + 4.5f + KnobRadius);
    float KnobY = CursorPos.y + Height * 0.5f;
    DrawList->AddCircleFilled(ImVec2(KnobX, KnobY), KnobRadius, KnobCol);
}

void OmniGUI::BeginGroupCard(const char* Icon, const char* Title, float Height)
{
    static constexpr ImU32 COL_BG_CHILD_1 = IM_COL32(19, 21, 31, 255);
    static constexpr ImU32 COL_BORDER = IM_COL32(46, 31, 64, 255);
    static constexpr ImU32 COL_FEAT_TINT_ACT = IM_COL32(157, 78, 221, 255);
    static constexpr ImVec4 COL4_TEXT_ACTIVE = ImVec4(0.753f, 0.722f, 0.831f, 1.0f);

    ImFont* IconFont = OmniIconsMedium;
    ImFont* TitleFont = InterBold18 ? InterBold18 : InterMed16;
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    ImVec2 Pos = ImGui::GetCursorScreenPos();
    float AvailableWidth = ImGui::GetContentRegionAvail().x;

    DrawList->AddRectFilled(
        Pos, ImVec2(Pos.x + AvailableWidth, Pos.y + Height), COL_BG_CHILD_1, 10.0f
    );
    DrawList->AddRect(Pos, ImVec2(Pos.x + AvailableWidth, Pos.y + Height), COL_BORDER, 10.0f);

    ImGui::SetCursorScreenPos(ImVec2(Pos.x + 16.0f, Pos.y + 14.0f));
    if (IconFont)
        ImGui::PushFont(IconFont);
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(COL_FEAT_TINT_ACT), "%s", Icon);
    if (IconFont)
        ImGui::PopFont();
    ImGui::SameLine(0.0f, 8.0f);
    if (TitleFont)
        ImGui::PushFont(TitleFont);
    ImGui::TextColored(COL4_TEXT_ACTIVE, "%s", Title);
    if (TitleFont)
        ImGui::PopFont();

    ImGui::SetCursorScreenPos(ImVec2(Pos.x, Pos.y + 45.0f));
}

void OmniGUI::BeginSettingRow(
    const char* Title, const char* Subtitle, float ControllerWidth, bool Separator
)
{
    static constexpr ImVec4 COL4_TEXT_MUTED = ImVec4(0.600f, 0.580f, 0.700f, 1.0f);
    static constexpr ImVec4 COL4_TEXT_ACTIVE = ImVec4(0.753f, 0.722f, 0.831f, 1.0f);

    SettingRowStartPos = ImGui::GetCursorScreenPos();
    SettingRowAvailWidth = ImGui::GetContentRegionAvail().x;
    SettingRowShowSeparator = Separator;

    ImGui::SetCursorScreenPos(ImVec2(SettingRowStartPos.x + 16.0f, SettingRowStartPos.y + 10.0f));
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

    ImGui::SetCursorScreenPos(ImVec2(
        SettingRowStartPos.x + SettingRowAvailWidth - ControllerWidth - 16.0f,
        SettingRowStartPos.y + 10.0f
    ));
    ImGui::SetNextItemWidth(ControllerWidth);
    ImGui::PushFont(InterMed14);
}

void OmniGUI::EndSettingRow()
{
    ImGui::PopFont();

    ImGui::PopStyleColor(12);
    ImGui::PopStyleVar(3);

    ImGui::SetCursorScreenPos(ImVec2(SettingRowStartPos.x, SettingRowStartPos.y + 60.0f));
    ImGui::Dummy(ImVec2(0, 0));
    if (SettingRowShowSeparator) {
        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        DrawList->AddLine(
            ImVec2(SettingRowStartPos.x + 16.0f, SettingRowStartPos.y + 60.0f),
            ImVec2(
                SettingRowStartPos.x + SettingRowAvailWidth - 16.0f, SettingRowStartPos.y + 60.0f
            ),
            IM_COL32(255, 255, 255, 12)
        );
    }
}
