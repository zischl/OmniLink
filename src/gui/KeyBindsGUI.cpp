#include "OmniGUI.h"
#include <algorithm>
#include <vector>

void OmniGUI::RenderKeybindsTab()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));
    ImGui::BeginChild(
        "KeybindsTabChild", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding
    );

    struct KeybindItem
    {
        const char* Name;
        std::vector<const char*> Keys;
    };

    struct CategoryGroup
    {
        const char* Icon;
        const char* Title;
        std::vector<KeybindItem> Items;
    };

    static const std::vector<CategoryGroup> Categories = {
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

    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    for (size_t CatogIndex = 0; CatogIndex < Categories.size(); ++CatogIndex) {
        const auto& Catog = Categories[CatogIndex];

        // Category Icon with Title
        if (Catog.Icon && Catog.Icon[0] != '\0') {
            ImGui::PushFont(OmniIconsMedium ? OmniIconsMedium : InterMed16);
            ImGui::TextColored(ImVec4(0.753f, 0.518f, 0.988f, 1.0f), "%s", Catog.Icon);
            ImGui::PopFont();
            ImGui::SameLine(0, 8.0f);
        }
        ImGui::PushFont(InterBold18 ? InterBold18 : InterMed16);
        ImGui::TextColored(ImVec4(0.753f, 0.518f, 0.988f, 1.0f), "%s", Catog.Title);
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        const float AvailableWidth = ImGui::GetContentRegionAvail().x;
        const float RowHeight = 44.0f;
        const float CardHeight = static_cast<float>(Catog.Items.size()) * RowHeight;

        ImVec2 CardMin = ImGui::GetCursorScreenPos();
        ImVec2 CardMax = ImVec2(CardMin.x + AvailableWidth, CardMin.y + CardHeight);

        // Category Container Card
        DrawList->AddRectFilled(CardMin, CardMax, COL_BG_CHILD_1, 10.0f);
        DrawList->AddRect(CardMin, CardMax, COL_BORDER, 10.0f, 0, 1.0f);

        ImGui::Dummy(ImVec2(AvailableWidth, CardHeight));

        for (size_t CItem = 0; CItem < Catog.Items.size(); ++CItem) {
            const auto& KeyBindings = Catog.Items[CItem];

            float RowY = CardMin.y + (static_cast<float>(CItem) * RowHeight);
            ImVec2 RowMin(CardMin.x, RowY);
            ImVec2 RowMax(CardMin.x + AvailableWidth, RowY + RowHeight);

            ImGui::PushID(static_cast<int>(CatogIndex * 100 + CItem));
            ImGui::SetCursorScreenPos(RowMin);
            ImGui::InvisibleButton("##KeybindRow", ImVec2(AvailableWidth, RowHeight));
            bool HoverState = ImGui::IsItemHovered();

            // Hover background highlights
            if (HoverState) {
                ImDrawFlags RoundingFlags = 0;
                if (CItem == 0)
                    RoundingFlags |= ImDrawFlags_RoundCornersTop;
                if (CItem == Catog.Items.size() - 1)
                    RoundingFlags |= ImDrawFlags_RoundCornersBottom;

                DrawList->AddRectFilled(
                    RowMin, RowMax, IM_COL32(255, 255, 255, 10), 10.0f, RoundingFlags
                );
            }

            // Row separator line
            if (CItem < Catog.Items.size() - 1) {
                DrawList->AddLine(
                    ImVec2(RowMin.x + 16.0f, RowMax.y),
                    ImVec2(RowMax.x - 16.0f, RowMax.y),
                    IM_COL32(255, 255, 255, 12),
                    1.0f
                );
            }

            // Keybind Name
            ImGui::PushFont(InterMed15);
            ImVec2 NameSize = ImGui::CalcTextSize(KeyBindings.Name);
            float NameY = RowMin.y + (RowHeight - NameSize.y) * 0.5f;
            DrawList->AddText(
                ImVec2(RowMin.x + 20.0f, NameY),
                HoverState ? IM_COL32(255, 255, 255, 255) : IM_COL32(240, 240, 245, 255),
                KeyBindings.Name
            );
            ImGui::PopFont();

            // Keycap Badges
            ImGui::PushFont(InterMed12);
            float TotalKeysWidth = 0.0f;
            const float KeySpacing = 6.0f;
            const float KeyPaddingX = 14.0f;
            const float KeyPillHeight = 26.0f;

            thread_local static std::vector<float> KeyWidths;
            KeyWidths.clear();

            for (const char* KeyStr : KeyBindings.Keys) {
                float TextW = ImGui::CalcTextSize(KeyStr).x;
                float PillW = (std::max)(26.0f, TextW + KeyPaddingX);
                KeyWidths.push_back(PillW);
                TotalKeysWidth += PillW;
            }
            if (!KeyBindings.Keys.empty()) {
                TotalKeysWidth += static_cast<float>(KeyBindings.Keys.size() - 1) * KeySpacing;
            }

            float KeyStartY = RowMin.y + (RowHeight - KeyPillHeight) * 0.5f;
            float CurrentKeyX = RowMax.x - 20.0f - TotalKeysWidth;

            for (size_t Idx = 0; Idx < KeyBindings.Keys.size(); ++Idx) {
                float PillW = KeyWidths[Idx];
                ImVec2 PillMin(CurrentKeyX, KeyStartY);
                ImVec2 PillMax(CurrentKeyX + PillW, KeyStartY + KeyPillHeight);

                ImU32 PillBg = HoverState ? IM_COL32(40, 36, 60, 255) : IM_COL32(28, 25, 42, 255);
                ImU32 PillBrd = HoverState ? IM_COL32(85, 70, 120, 255) : IM_COL32(56, 44, 80, 255);

                DrawList->AddRectFilled(PillMin, PillMax, PillBg, 5.0f);
                DrawList->AddRect(PillMin, PillMax, PillBrd, 5.0f, 0, 1.0f);

                ImVec2 TextSize = ImGui::CalcTextSize(KeyBindings.Keys[Idx]);
                ImVec2 TextPos(
                    PillMin.x + (PillW - TextSize.x) * 0.5f,
                    PillMin.y + (KeyPillHeight - TextSize.y) * 0.5f
                );

                DrawList->AddText(TextPos, IM_COL32(220, 212, 240, 255), KeyBindings.Keys[Idx]);

                CurrentKeyX += PillW + KeySpacing;
            }
            ImGui::PopFont();

            ImGui::PopID();
        }

        ImGui::SetCursorScreenPos(ImVec2(CardMin.x, CardMin.y + CardHeight + 20.0f));
        ImGui::Dummy(ImVec2(0, 0));
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}
