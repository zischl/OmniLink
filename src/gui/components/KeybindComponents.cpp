#include "OmniGUI.h"

void OmniGUI::KeycapPills(
    ImFont* KeyFont,
    const std::vector<const char*>& Keys,
    float RowMinY,
    float RowMaxX,
    float RowHeight,
    bool HoverState
)
{
    static constexpr ImVec4 COL4_TEXT_ACTIVE = ImVec4(0.753f, 0.722f, 0.831f, 1.0f);

    if (KeyFont)
        ImGui::PushFont(KeyFont);

    float TotalKeysWidth = 0.0f;
    const float KeySpacing = 6.0f;
    const float KeyPaddingX = 14.0f;
    const float KeyPillHeight = 26.0f;

    thread_local static std::vector<float> KeyWidths;
    KeyWidths.clear();

    for (const char* KeyStr : Keys) {
        float TextWidth = ImGui::CalcTextSize(KeyStr).x;
        float PillWidth = (std::max)(26.0f, TextWidth + KeyPaddingX);
        KeyWidths.push_back(PillWidth);
        TotalKeysWidth += PillWidth;
    }
    if (!Keys.empty()) {
        TotalKeysWidth += static_cast<float>(Keys.size() - 1) * KeySpacing;
    }

    float KeyStartY = RowMinY + (RowHeight - KeyPillHeight) * 0.5f;
    float currentKeyX = RowMaxX - 16.0f - TotalKeysWidth;

    for (size_t idx = 0; idx < Keys.size(); ++idx) {
        float pillW = KeyWidths[idx];
        ImVec2 pillMin(currentKeyX, KeyStartY);
        ImVec2 pillMax(currentKeyX + pillW, KeyStartY + KeyPillHeight);

        ImU32 pillBg = HoverState ? IM_COL32(40, 36, 60, 255) : IM_COL32(28, 25, 42, 255);
        ImU32 pillBrd = HoverState ? IM_COL32(75, 60, 110, 255) : IM_COL32(56, 44, 80, 255);

        DrawList->AddRectFilled(pillMin, pillMax, pillBg, 5.0f);
        DrawList->AddRect(pillMin, pillMax, pillBrd, 5.0f, 0, 1.0f);

        ImVec2 textSize = ImGui::CalcTextSize(Keys[idx]);
        ImVec2 textPos(
            pillMin.x + (pillW - textSize.x) * 0.5f, pillMin.y + (KeyPillHeight - textSize.y) * 0.5f
        );

        DrawList->AddText(textPos, ImGui::ColorConvertFloat4ToU32(COL4_TEXT_ACTIVE), Keys[idx]);

        currentKeyX += pillW + KeySpacing;
    }
    if (KeyFont)
        ImGui::PopFont();
}

void OmniGUI::RenderKeybindRow(
    const KeybindItem& Item,
    size_t ItemIdx,
    size_t CategoryIdx,
    size_t TotalItems,
    ImVec2 RowMin,
    ImVec2 RowMax,
    float RowHeight
)
{
    static constexpr ImVec4 COL4_TEXT_ACTIVE = ImVec4(0.753f, 0.722f, 0.831f, 1.0f);

    float AvailableWidth = RowMax.x - RowMin.x;

    ImGui::PushID(static_cast<int>(CategoryIdx * 100 + ItemIdx));
    ImGui::SetCursorScreenPos(RowMin);
    ImGui::InvisibleButton("##KeybindRow", ImVec2(AvailableWidth, RowHeight));
    bool hoverState = ImGui::IsItemHovered();

    // Hover background highlights
    if (hoverState) {
        ImDrawFlags roundingFlags = 0;
        if (ItemIdx == 0)
            roundingFlags |= ImDrawFlags_RoundCornersTop;
        if (ItemIdx == TotalItems - 1)
            roundingFlags |= ImDrawFlags_RoundCornersBottom;

        DrawList->AddRectFilled(RowMin, RowMax, IM_COL32(255, 255, 255, 10), 10.0f, roundingFlags);
    }

    // Row separator line
    if (ItemIdx < TotalItems - 1) {
        DrawList->AddLine(
            ImVec2(RowMin.x + 16.0f, RowMax.y),
            ImVec2(RowMax.x - 16.0f, RowMax.y),
            IM_COL32(255, 255, 255, 12),
            1.0f
        );
    }

    // Keybind Name
    if (InterMed15)
        ImGui::PushFont(InterMed15);
    ImVec2 nameSize = ImGui::CalcTextSize(Item.Name);
    float nameY = RowMin.y + (RowHeight - nameSize.y) * 0.5f;
    DrawList->AddText(
        ImVec2(RowMin.x + 16.0f, nameY),
        hoverState ? IM_COL32(255, 255, 255, 255)
                   : ImGui::ColorConvertFloat4ToU32(COL4_TEXT_ACTIVE),
        Item.Name
    );
    if (InterMed15)
        ImGui::PopFont();

    // Keycap Badges
    KeycapPills(InterMed12, Item.Keys, RowMin.y, RowMax.x, RowHeight, hoverState);

    ImGui::PopID();
}

void OmniGUI::KeybindCategoryCard(
    const KeybindCategoryGroup& category, size_t catIdx, float availableWidth
)
{
    static constexpr ImU32 COL_BG_CHILD_1 = IM_COL32(19, 21, 31, 255);
    static constexpr ImU32 COL_BORDER = IM_COL32(46, 31, 64, 255);
    static constexpr ImU32 COL_FEAT_TINT_ACT = IM_COL32(157, 78, 221, 255);
    static constexpr ImVec4 COL4_TEXT_ACTIVE = ImVec4(0.753f, 0.722f, 0.831f, 1.0f);

    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    // Category Icon with Title
    if (category.Icon && category.Icon[0] != '\0') {
        if (OmniIconsMedium)
            ImGui::PushFont(OmniIconsMedium);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(COL_FEAT_TINT_ACT), "%s", category.Icon);
        if (OmniIconsMedium)
            ImGui::PopFont();
        ImGui::SameLine(0, 8.0f);
    }

    ImFont* headerFont = InterBold18 ? InterBold18 : InterMed16;
    if (headerFont)
        ImGui::PushFont(headerFont);
    ImGui::TextColored(COL4_TEXT_ACTIVE, "%s", category.Title);
    if (headerFont)
        ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    const float rowHeight = 44.0f;
    const float cardHeight = static_cast<float>(category.Items.size()) * rowHeight;

    ImVec2 cardMin = ImGui::GetCursorScreenPos();
    ImVec2 cardMax = ImVec2(cardMin.x + availableWidth, cardMin.y + cardHeight);

    // Category Container Card
    DrawList->AddRectFilled(cardMin, cardMax, COL_BG_CHILD_1, 10.0f);
    DrawList->AddRect(cardMin, cardMax, COL_BORDER, 10.0f, 0, 1.0f);

    ImGui::Dummy(ImVec2(availableWidth, cardHeight));

    for (size_t cItem = 0; cItem < category.Items.size(); ++cItem) {
        float rowY = cardMin.y + (static_cast<float>(cItem) * rowHeight);
        ImVec2 rowMin(cardMin.x, rowY);
        ImVec2 rowMax(cardMin.x + availableWidth, rowY + rowHeight);

        RenderKeybindRow(
            category.Items[cItem], cItem, catIdx, category.Items.size(), rowMin, rowMax, rowHeight
        );
    }

    ImGui::SetCursorScreenPos(ImVec2(cardMin.x, cardMin.y + cardHeight + 20.0f));
    ImGui::Dummy(ImVec2(0, 0));
}
