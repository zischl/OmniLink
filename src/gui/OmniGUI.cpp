#include "OmniGUI.h"
#include "AssetLogoNB.h"
#include "InterFonts.h"
#include "JetBrainsFonts.h"
#include "OmniEnums.h"
#include "OmniIcons.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <bit>
#include <string>
#include <vector>

OmniGUI::OmniGUI(OmniCore& OmniCoreInstance)
    : App(OmniCoreInstance), SelectedDevice(OmniCore::SelectedTargetDevice)
{
    AvailableInstances = App.GetAvailableInstances();
    ActiveInstances = App.GetActiveInstances();
}

void OmniGUI::SetupImGui(void* hwnd, void* D3D11Device, void* D3D11Context)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

#ifdef _WIN32
    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(static_cast<HWND>(hwnd));
    ImGui_ImplDX11_Init(
        static_cast<ID3D11Device*>(D3D11Device), static_cast<ID3D11DeviceContext*>(D3D11Context)
    );

    IconTexture.LoadEmbeddedRGBA(
        OmniLinkLogoNBData, 128, 128, static_cast<ID3D11Device*>(D3D11Device)
    );
#endif

    ImGuiStyle& style = ImGui::GetStyle();

    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.075f, 0.082f, 0.122f, 1.0f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = COL4_STYLE_MODAL_DIM;
    style.Colors[ImGuiCol_ChildBg] = COL4_STYLE_CHILD_BG;

    style.Colors[ImGuiCol_Separator] = COL4_STYLE_SEPARATOR;
    style.Colors[ImGuiCol_SeparatorHovered] = COL4_STYLE_SEPARATOR;
    style.Colors[ImGuiCol_SeparatorActive] = COL4_STYLE_SEPARATOR;

    style.SeparatorTextBorderSize = 1.0f;
    style.ItemSpacing.x = 0.0f;

    ImFontConfig FontCFG;
    FontCFG.FontDataOwnedByAtlas = false;

    InterMed12 = io.Fonts->AddFontFromMemoryTTF(Inter18Medium, Inter18MediumLen, 12.0f, &FontCFG);
    InterReg14 = io.Fonts->AddFontFromMemoryTTF(Inter18Regular, Inter18RegularLen, 14.0f, &FontCFG);
    InterReg15 = io.Fonts->AddFontFromMemoryTTF(Inter18Regular, Inter18RegularLen, 15.0f, &FontCFG);
    InterMed14 = io.Fonts->AddFontFromMemoryTTF(Inter18Medium, Inter18MediumLen, 14.0f, &FontCFG);
    InterMed15 = io.Fonts->AddFontFromMemoryTTF(Inter18Medium, Inter18MediumLen, 15.0f, &FontCFG);
    InterMed16 = io.Fonts->AddFontFromMemoryTTF(Inter18Medium, Inter18MediumLen, 16.0f, &FontCFG);
    InterBold18 = io.Fonts->AddFontFromMemoryTTF(Inter18Medium, Inter18MediumLen, 18.0f, &FontCFG);
    InterBold20 = io.Fonts->AddFontFromMemoryTTF(Inter18Medium, Inter18MediumLen, 20.0f, &FontCFG);
    JetBrainsMed15 = io.Fonts->AddFontFromMemoryTTF(
        JetBrainsMonoMedium, JetBrainsMonoMediumLen, 15.0f, &FontCFG
    );
    JetBrainsBold20 =
        io.Fonts->AddFontFromMemoryTTF(JetBrainsMonoBold, JetBrainsMonoBoldLen, 20.0f, &FontCFG);

    // Range for OmniIconsSmall: Contains only 'airplay'
    static const ImWchar OmniLargeIconRange[] = {61458, 61458, 0};

    // Range for OmniIconsSmall: Contains only 'x' and 'minus'
    static const ImWchar OmniSmallIconRange[] = {61459, 61460, 0};

    // Range for OmniIcons: Contains everything else
    static const ImWchar OmniMediumIconRange[] = {61440, 61458, 0};

    FontCFG.GlyphRanges = OmniLargeIconRange;
    OmniIconsLarge =
        io.Fonts->AddFontFromMemoryTTF(OmniIcons_ttf, OmniIcons_ttf_len, 60.0f, &FontCFG);

    FontCFG.GlyphRanges = OmniMediumIconRange;
    OmniIconsMedium =
        io.Fonts->AddFontFromMemoryTTF(OmniIcons_ttf, OmniIcons_ttf_len, 20.0f, &FontCFG);

    FontCFG.GlyphRanges = OmniSmallIconRange;
    OmniIconsSmall =
        io.Fonts->AddFontFromMemoryTTF(OmniIcons_ttf, OmniIcons_ttf_len, 14.0f, &FontCFG);
}
void OmniGUI::DeviceIconPreview(
    const ImVec2& pos, const ImU32& col, const ImVec2& text_size, const char* text
)
{
    DrawList->AddRect(
        ImVec2(pos.x - 50, pos.y - 40),
        ImVec2(pos.x + 50, pos.y + 40),
        col,
        5.0f,
        0,
        2.0f
    ); // monitor
    DrawList->AddText(ImVec2(pos.x - (text_size.x * 0.5f), pos.y), col, text);
    DrawList->AddRect(ImVec2(pos.x, pos.y + 40), ImVec2(pos.x, pos.y + 55), col, 5.0f, 0, 2.0f);
    DrawList->AddRect(
        ImVec2(pos.x - 15, pos.y + 55),
        ImVec2(pos.x + 15, pos.y + 55),
        col,
        10.0f,
        0,
        1.0f
    ); // stand
}

void OmniGUI::DeviceIcon(const char* Label, const ImVec2& Pos, const OmniInstance* DeviceData)
{
    ImGui::PushID(Label);

    const ImGuiID Id = ImGui::GetID(Label);

    // Da Icon
    ImGui::PushFont(OmniIconsLarge);

    ImVec2 IconRenderSize = ImGui::CalcTextSize(IC_AIRPLAY);
    ImVec2 IconRenderPos =
        ImVec2(Pos.x - (IconRenderSize.x * 0.5f), Pos.y - (IconRenderSize.y * 0.5f));

    ImRect Bbox = ImRect(
        IconRenderPos,
        ImVec2(IconRenderPos.x + IconRenderSize.x, IconRenderPos.y + IconRenderSize.y)
    );

    ImGui::ItemAdd(Bbox, Id, NULL, ImGuiItemFlags_None);

    bool Hovered, Held;
    bool Pressed = ImGui::ButtonBehavior(Bbox, Id, &Hovered, &Held, 0);
    ImGui::RenderNavCursor(Bbox, Id);

    ImU32 Col = Hovered || Held ? COL_DEV_HOVER : COL_DEV_DEFAULT;

    DrawList->AddText(IconRenderPos, Col, IC_AIRPLAY);
    ImGui::PopFont();

    // The IP , imma go with name later
    ImVec2 TextSize = ImGui::CalcTextSize(DeviceData->InstanceName);
    float HalfWidth = TextSize.x * 0.5f;
    ImVec2 TextPos = ImVec2(Pos.x - HalfWidth, Pos.y + (IconRenderSize.y * 0.5) + 5.0f);
    DrawList->AddText(TextPos, Col, DeviceData->InstanceName);

    // Drag and Drop Handling
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(
            "DeviceInfo", &(DeviceData->DevMapIndex), sizeof(DeviceData->DevMapIndex)
        );
        DeviceIconPreview(ImGui::GetCursorScreenPos(), Col, TextSize, DeviceData->IPv4_String);
        ImGui::EndDragDropSource();
    } else if (ImGui::BeginDragDropTarget()) {
        const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("DeviceInfo");
        if (Payload != nullptr) {
            const uint8_t Data = *static_cast<uint8_t*>(Payload->Data);
            OmniAPI::SwapDeviceLayout(Data, DeviceData->DevMapIndex);
        }
        ImGui::EndDragDropTarget();
    }

    // Context Menu
    if (ImGui::BeginPopupContextItem("DeviceContextMenu")) {
        SelectedDevice = DeviceMap(DeviceData->DevMapIndex);
        if (ImGui::MenuItem("Connect Instance")) {
            ConnectionRequest Request;
            Request.DeviceID = static_cast<DeviceMap>(DeviceData->DevMapIndex);
            OmniAPI::Connect(Request);
        }
        ImGui::EndPopup();
    }

    if (Pressed) {
        SelectedDevice = DeviceMap(DeviceData->DevMapIndex);
    }

    ImGui::PopID();
}

bool OmniGUI::VerticalMenuItem(
    const char* Label, const char* Icon, bool State, ImVec2& MenuItemSize
)
{
    ImGui::PushID(Label);

    const ImVec2 IconSize = ImVec2(55, 55);
    const float IconBgRounding = 16.0f;

    ImVec2 Pos = ImGui::GetCursorScreenPos();
    bool Clicked = ImGui::InvisibleButton(Label, MenuItemSize);
    bool Hovered = ImGui::IsItemHovered();

    ImU32 BgColor = COL_MENU_BG_IDLE;
    ImU32 IconBgColor = COL_MENU_ICON_IDLE;

    ImVec2 IconBoxMin = ImVec2(Pos.x + (MenuItemSize.x - IconSize.x) * 0.5f, Pos.y + 10);
    ImVec2 IconBoxMax = ImVec2(IconBoxMin.x + IconSize.x, IconBoxMin.y + IconSize.y);

    if (State) {
        ImVec2 IndicatorTop = ImVec2(Pos.x, Pos.y + (MenuItemSize.y * 0.30f));
        ImVec2 IndicatorBottom = ImVec2(Pos.x + 5, Pos.y + (MenuItemSize.y * 0.70f));

        BgColor = COL_MENU_BG_ACTIVE;
        IconBgColor = COL_MENU_ICON_ACT;

        // Active Item Strip
        DrawList->AddRectFilled(IndicatorTop, IndicatorBottom, COL_MENU_STRIP, 10.0f);
    } else if (Hovered) {
        BgColor = COL_MENU_BG_HOVER;
    }

    ImVec2 TotalBoxMax = ImVec2(Pos.x + MenuItemSize.x, Pos.y + MenuItemSize.y);
    // Item BG
    DrawList->AddRectFilled(Pos, TotalBoxMax, BgColor, 4.0f);
    // Item Icon BG
    DrawList->AddRectFilled(IconBoxMin, IconBoxMax, IconBgColor, IconBgRounding);

    // Item Icon
    ImU32 IconTint = State ? COL_MENU_TINT_ACT : COL_MENU_TINT_IDLE;
    ImGui::PushFont(OmniIconsMedium);
    ImVec2 GlyphSize = ImGui::CalcTextSize(Icon);
    ImVec2 GlyphPos = ImVec2(
        IconBoxMin.x + (IconSize.x - GlyphSize.x) * 0.5f,
        IconBoxMin.y + (IconSize.y - GlyphSize.y) * 0.5f
    );

    DrawList->AddText(GlyphPos, IconTint, Icon);
    ImGui::PopFont();

    ImVec2 LabelSize = ImGui::CalcTextSize(Label);
    ImVec2 LabelPos = ImVec2(Pos.x + (MenuItemSize.x - LabelSize.x) * 0.5f, IconBoxMax.y + 10);
    ImU32 TextColor = State ? COL_MENU_TXT_ACT : COL_MENU_TXT_IDLE;

    // Item Tex
    DrawList->AddText(LabelPos, TextColor, Label);

    ImGui::PopID();
    return Clicked;
}

bool OmniGUI::IconizedButton(
    const char* Label, const char* Icon, bool State, const ImVec2& ButtonSize
)
{
    ImGui::PushID(Label);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    bool clicked = ImGui::InvisibleButton(Label, ButtonSize);
    bool hovered = ImGui::IsItemHovered();

    // Button BG
    if (State) {
        DrawList->AddRectFilled(
            pos, ImVec2(pos.x + ButtonSize.x, pos.y + ButtonSize.y), COL_FEAT_BG_ACTIVE
        );
    } else if (hovered) {
        DrawList->AddRectFilled(
            pos, ImVec2(pos.x + ButtonSize.x, pos.y + ButtonSize.y), COL_FEAT_BG_HOVER
        );
    }

    const ImVec2 IconSize = ImVec2(44.0f, 44.0f);
    ImVec2 IconBoxPos = ImVec2(pos.x + (ButtonSize.x - IconSize.x) * 0.5f, pos.y + 14.0f);

    ImU32 IconBgColor = State ? COL_FEAT_IC_ACTIVE : COL_FEAT_IC_HOVER;
    // Icon BG
    DrawList->AddRectFilled(
        IconBoxPos, ImVec2(IconBoxPos.x + IconSize.x, IconBoxPos.y + IconSize.y), IconBgColor, 12.0f
    );

    // Da Icon
    ImGui::PushFont(OmniIconsMedium);
    ImVec2 IconTextSize = ImGui::CalcTextSize(Icon);
    ImVec2 IconTextPos = ImVec2(
        IconBoxPos.x + (IconSize.x - IconTextSize.x) * 0.5f,
        IconBoxPos.y + (IconSize.y - IconTextSize.y) * 0.5f
    );
    ImU32 IconColor = State ? COL_FEAT_TINT_ACT : COL_FEAT_TINT_IDLE;

    DrawList->AddText(IconTextPos, IconColor, Icon);
    ImGui::PopFont();

    ImVec2 LabelSize = ImGui::CalcTextSize(Label);
    ImVec2 LabelPos =
        ImVec2(pos.x + (ButtonSize.x - LabelSize.x) * 0.5f, IconBoxPos.y + IconSize.y + 4.0f);
    ImU32 LabelColor = State ? COL_FEAT_TXT_ACT : COL_FEAT_TXT_IDLE;
    // Button Label
    DrawList->AddText(LabelPos, LabelColor, Label);

    if (State) {
        const float StripWidth = 40.0f;
        const float StripHeight = 3.0f;
        ImVec2 StripPosLeft =
            ImVec2(pos.x + (ButtonSize.x - StripWidth) * 0.5f, pos.y + ButtonSize.y - StripHeight);
        ImVec2 stripPosRight = ImVec2(StripPosLeft.x + StripWidth, pos.y + ButtonSize.y);

        DrawList->AddRectFilled(
            StripPosLeft, stripPosRight, COL_FEAT_STRIP, 1.5f, ImDrawFlags_RoundCornersTop
        );

        DrawList->AddRectFilledMultiColor(
            ImVec2(StripPosLeft.x, StripPosLeft.y - 8),
            stripPosRight,
            IM_COL32_BLACK_TRANS,
            IM_COL32_BLACK_TRANS,
            COL_FEAT_GLOW,
            COL_FEAT_GLOW
        );
    }

    ImGui::PopID();
    return clicked;
}

bool OmniGUI::DeviceAddButton(const char* Label, const ImVec2& CenterPos, ImU32 Color)
{
    ImGui::PushID(Label);

    const ImGuiID Id = ImGui::GetID(Label);

    ImGui::PushFont(OmniIconsLarge);
    ImVec2 TextSize = ImGui::CalcTextSize(IC_DIAMOND_PLUS);
    ImVec2 RenderPos = ImVec2(CenterPos.x - (TextSize.x * 0.5f), CenterPos.y - (TextSize.y * 0.5f));

    const float Padding = 8.0f;
    ImRect Bbox = ImRect(
        ImVec2(RenderPos.x - Padding, RenderPos.y - Padding),
        ImVec2(RenderPos.x + TextSize.x + Padding, RenderPos.y + TextSize.y + Padding)
    );

    ImGui::ItemAdd(Bbox, Id, NULL, ImGuiItemFlags_None);

    bool Hovered, Held;
    bool Pressed = ImGui::ButtonBehavior(Bbox, Id, &Hovered, &Held, 0);
    ImGui::RenderNavCursor(Bbox, Id);

    ImU32 BgColor = Hovered || Held ? COL_MENU_BG_HOVER : ImGui::GetColorU32(ImGuiCol_WindowBg);
    ImU32 RenderColor = Hovered || Held ? COL_DEV_HOVER : Color;

    DrawList->AddRectFilled(Bbox.Min, Bbox.Max, BgColor, 6.0f);
    if (Hovered) {
        DrawList->AddRect(Bbox.Min, Bbox.Max, COL_MENU_STRIP, 6.0f, 0, 1.5f);
    }

    DrawList->AddText(RenderPos, RenderColor, IC_DIAMOND_PLUS);

    ImGui::PopFont();
    ImGui::PopID();

    return Pressed;
}

void OmniGUI::CenterItemX(const float ItemWidth)
{
    float Space = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((Space - ItemWidth) * 0.5f);
}

int OmniGUI::ConnectionRing(const char* Label, const ImVec2& WidgetSize, const float Radius)
{
    ImGui::Dummy(WidgetSize);

    ImGui::PushID(Label);

    ImVec2 ItemPos = ImGui::GetItemRectMin();
    ImVec2 ItemSize = ImGui::GetItemRectSize();
    ImVec2 Pos = ImVec2(ItemPos.x + (ItemSize.x * 0.5f), ItemPos.y + (ItemSize.y * 0.5f));

    ImVec2 Space = ImGui::GetContentRegionAvail();
    ImVec2 CPos = ImGui::GetCursorScreenPos();

    ImGui::GetWindowDrawList()->AddCircle(Pos, 205.0f, COL_RING_BG, 64, 3.0f);

    ImGui::PopID();

    ImGui::PushFont(JetBrainsMed15);

    // uh.. the math if i forget , 205 * sin(45 degrees) = 144.95
    const int DiagonalAxe = static_cast<int>(Radius * 0.707106f);

    auto& Devices = *AvailableInstances;

    // Center Device
    DeviceIcon("C0", Pos, &Devices[DeviceMap::C0]);

    static const UIDeviceLayout LAYOUTS[9] = {
        {DeviceMap::C0, "C0", 0.0f, 0.0f, false},    // Center
        {DeviceMap::L1, "L1", -1.0f, 0.0f, false},   // Left
        {DeviceMap::U1, "U1", 0.0f, -1.0f, false},   // Up
        {DeviceMap::R1, "R1", 1.0f, 0.0f, false},    // Right
        {DeviceMap::D1, "D1", 0.0f, 1.0f, false},    // Down
        {DeviceMap::LU1, "LU1", -1.0f, -1.0f, true}, // Left-Up
        {DeviceMap::RU1, "RU1", 1.0f, -1.0f, true},  // Right-Up
        {DeviceMap::RD1, "RD1", 1.0f, 1.0f, true},   // Right-Down
        {DeviceMap::LD1, "LD1", -1.0f, 1.0f, true}   // Left-Down
    };

    unsigned int active_mask = 0;

    float offset;

    for (int i = 1; i < 9; ++i) {
        const auto& layout = LAYOUTS[i];
        auto& dev = Devices[layout.DeviceID];

        offset = layout.DiagonalState ? DiagonalAxe : Radius;
        ImVec2 target_pos(
            Pos.x + (layout.DirectionalityX * offset), Pos.y + (layout.DirectionalityY * offset)
        );

        if (dev.InstanceIP) {
            DeviceIcon(layout.Label, target_pos, &dev);
            active_mask |= (1 << i);
        } else {
            if (DeviceAddButton(layout.Label, target_pos, COL_DEV_EMPTY)) {
                TargetSlotForAdd = layout.DeviceID;
                ShowConnectModal = true;
            }
        }
    }

    if (SelectedDevice != DeviceMap::C0 && SelectedDevice < DeviceMap::END) {
        const uint8_t layout_index = static_cast<uint8_t>(SelectedDevice);
        const auto& layout = LAYOUTS[layout_index];

        float offset = layout.DiagonalState ? DiagonalAxe : Radius;
        ImVec2 target_pos(
            Pos.x + (layout.DirectionalityX * offset),
            Pos.y + 12.0f + (layout.DirectionalityY * offset)
        );

        constexpr float total_offset = 32.0f + 12.0f;
        constexpr float arrow_size = 8.0f;
        constexpr float thickness = 2.0f;

        ImDrawList* DrawList = ImGui::GetWindowDrawList();

        ImVec2 tl_tip(target_pos.x - total_offset, target_pos.y - total_offset);
        ImVec2 tl_pts[3] = {
            ImVec2(tl_tip.x - arrow_size, tl_tip.y), tl_tip, ImVec2(tl_tip.x, tl_tip.y - arrow_size)
        };

        ImVec2 tr_tip(target_pos.x + total_offset, target_pos.y - total_offset);
        ImVec2 tr_pts[3] = {
            ImVec2(tr_tip.x + arrow_size, tr_tip.y), tr_tip, ImVec2(tr_tip.x, tr_tip.y - arrow_size)
        };

        ImVec2 bl_tip(target_pos.x - total_offset, target_pos.y + total_offset);
        ImVec2 bl_pts[3] = {
            ImVec2(bl_tip.x - arrow_size, bl_tip.y), bl_tip, ImVec2(bl_tip.x, bl_tip.y + arrow_size)
        };

        ImVec2 br_tip(target_pos.x + total_offset, target_pos.y + total_offset);
        ImVec2 br_pts[3] = {
            ImVec2(br_tip.x + arrow_size, br_tip.y), br_tip, ImVec2(br_tip.x, br_tip.y + arrow_size)
        };

        DrawList->AddPolyline(tl_pts, 3, COL_MENU_STRIP, 0, thickness);
        DrawList->AddPolyline(tr_pts, 3, COL_MENU_STRIP, 0, thickness);
        DrawList->AddPolyline(bl_pts, 3, COL_MENU_STRIP, 0, thickness);
        DrawList->AddPolyline(br_pts, 3, COL_MENU_STRIP, 0, thickness);
    }

    ImGui::PopFont();

    return std::popcount(active_mask);
}

void OmniGUI::MetricDashboard(
    const char* ContainerId, const MetricItem* Items, int ItemCount, float TotalWidth, float Height
)
{
    if (ItemCount <= 0 || !Items)
        return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, COL4_TRANSPARENT);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    if (ImGui::BeginChild(
            ContainerId,
            ImVec2(TotalWidth, Height),
            ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
        )) {

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
        ImGui::PopStyleVar();

        ImVec2 BarMin = ImGui::GetCursorScreenPos();
        ImVec2 BarMax = ImVec2(BarMin.x + TotalWidth, BarMin.y + Height);

        ImU32 ContainerBgColor = COL_DASH_BG;
        ImU32 ContainerBorderColor = COL_DASH_BORDER;

        // Main Container
        DrawList->AddRectFilled(
            BarMin, BarMax, ContainerBgColor, 12.0f, ImDrawFlags_RoundCornersBottomRight
        );

        float SegmentWidth = TotalWidth / ItemCount;

        // Padding metrics inside each slot
        float InnerPaddingX = 14.0f;
        float InnerPaddingY = 6.0f;
        float AccentPillWidth = 3.0f;
        float AccentGapping = 10.0f;

        ImGui::PushFont(InterMed14);

        for (int i = 0; i < ItemCount; ++i) {
            ImVec2 SegMin = ImVec2(BarMin.x + (i * SegmentWidth), BarMin.y);
            ImVec2 SegMax = ImVec2(SegMin.x + SegmentWidth, BarMin.y + Height);

            // Vertical Segment Separators
            if (i < ItemCount - 1) {
                DrawList->AddLine(
                    ImVec2(SegMax.x, SegMin.y),
                    ImVec2(SegMax.x, SegMax.y),
                    ContainerBorderColor,
                    1.0f
                );
            }

            // Vertical Accent Pill
            ImVec2 PillMin = ImVec2(SegMin.x + InnerPaddingX, SegMin.y + InnerPaddingY + 2.0f);
            ImVec2 PillMax = ImVec2(PillMin.x + AccentPillWidth, SegMax.y - InnerPaddingY - 2.0f);
            ImU32 AccentCol = COL_DASH_BORDER;
            DrawList->AddRectFilled(PillMin, PillMax, AccentCol, 1.5f);

            float TextOriginX = PillMax.x + AccentGapping;
            ImVec2 TitlePos = ImVec2(TextOriginX, SegMin.y + InnerPaddingY);
            ImGui::SetCursorScreenPos(TitlePos);

            // Da Title
            ImGui::PushStyleColor(ImGuiCol_Text, COL4_DASH_TEXT_MUTED);
            ImGui::TextUnformatted(Items[i].Title);
            ImGui::PopStyleColor();

            float LineHeight = ImGui::GetTextLineHeight();
            ImVec2 ValuePos = ImVec2(TextOriginX, TitlePos.y + LineHeight - 1.0f);
            ImGui::SetCursorScreenPos(ValuePos);

            // Value
            ImGui::PushFont(JetBrainsBold20);
            ImGui::PushStyleColor(ImGuiCol_Text, COL4_DASH_TEXT_VALUE);
            ImGui::TextUnformatted(Items[i].Value);
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }

        ImGui::PopFont();
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(1);
}

void OmniGUI::CreateCurvedLine(const char* label, int curve)
{
    ImGui::PushID(label);
    ImVec2 pos = ImGui::GetCursorScreenPos();
    int length = 150;

    ImVec2 p0 = ImVec2(pos.x, pos.y);
    ImVec2 p1 = ImVec2(pos.x, pos.y + length);
    ImVec2 cp0 = ImVec2(pos.x - curve, pos.y + curve);
    ImVec2 cp1 = ImVec2(pos.x - curve, pos.y + length - curve);

    ImU32 color = COL_GLOW_OPAQUE;
    float thickness = 1.0f;
    int segments = 20;

    DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness, segments);

    int glow_range = 1;

    color = COL_GLOW_HIGH;
    DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness + glow_range, segments);

    color = COL_GLOW_MED;
    DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness + glow_range * 2, segments);

    color = COL_GLOW_LOW;
    DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness + glow_range * 2.5, segments);

    ImGui::PopID();
}

void OmniGUI::RenderConnectModal()
{
    if (!ShowConnectModal)
        return;

    ImGui::OpenPopup("Connect Device Prompt");

    ImVec2 Center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(Center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 22.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
    ImGui::PushStyleColor(ImGuiCol_PopupBg, OmniTheme::COL_HANDSHAKE_BADGE_BG);
    ImGui::PushStyleColor(ImGuiCol_Border, OmniTheme::COL_HANDSHAKE_CARD_BRD);
    ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, ImVec4(0.04f, 0.04f, 0.08f, 0.75f));

    ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::BeginPopupModal("Connect Device Prompt", &ShowConnectModal, WindowFlags)) {

        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        ImVec2 WinPos = ImGui::GetWindowPos();
        float WinWidth = ImGui::GetWindowWidth();

        // Accent top line
        DrawList->AddRectFilled(
            ImVec2(WinPos.x + 12.0f, WinPos.y),
            ImVec2(WinPos.x + WinWidth - 12.0f, WinPos.y + 3.0f),
            OmniTheme::COL_HANDSHAKE_ACCENT,
            16.0f,
            ImDrawFlags_RoundCornersTop
        );

        // Icon + Title + Subtitle
        ImGui::PushFont(OmniIconsMedium);
        ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_TITLE, "%s", IC_NETWORK);
        ImGui::PopFont();

        ImGui::SameLine(0.0f, 10.0f);

        ImGui::BeginGroup();
        ImGui::PushFont(InterBold20);
        ImGui::TextColored(COL4_TEXT_ACTIVE, "Connect Device Instance");
        ImGui::PopFont();

        ImGui::PushFont(InterMed12);
        ImGui::TextColored(
            COL4_TEXT_MUTED, "Enter an IPv4 address or select a discovered network node."
        );
        ImGui::PopFont();
        ImGui::EndGroup();

        // Close Button
        ImGui::SameLine(WinWidth - 52.0f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 2.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, COL4_TRANSPARENT);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COL4_BTN_HOVER_DARK);
        ImGui::PushFont(OmniIconsSmall);
        if (ImGui::Button(IC_X, ImVec2(24.0f, 24.0f))) {
            ShowConnectModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopFont();
        ImGui::PopStyleColor(2);

        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 10.0f));

        // Manual IPv4 Address
        ImGui::PushFont(InterMed12);
        ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_LABEL, "MANUAL IPv4 ADDRESS");
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 8.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, OmniTheme::COL4_BTN_DECLINE_BG);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, OmniTheme::COL4_BTN_DECLINE_BG_HOVER);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, OmniTheme::COL4_BTN_DECLINE_BG_ACTIVE);
        ImGui::PushStyleColor(ImGuiCol_Border, OmniTheme::COL4_BTN_DECLINE_BRD);
        ImGui::PushStyleColor(ImGuiCol_Text, COL4_TEXT_ACTIVE);

        ImGui::SetNextItemWidth(-1.0f);
        ImGui::PushFont(InterMed14);
        ImGui::InputText("##IPInput", ManualIPBuffer, sizeof(ManualIPBuffer));
        ImGui::PopFont();

        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(3);

        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        // Discovered Network Devices
        ImGui::PushFont(InterMed12);
        ImGui::TextColored(OmniTheme::COL4_HANDSHAKE_LABEL, "DISCOVERED NETWORK DEVICES");
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0.0f, 4.0f));

        int DiscoveredCount = 0;
        if (AvailableInstances && !AvailableInstances->empty()) {
            for (const auto& [id, instance] : *AvailableInstances) {
                if (id == DeviceMap::C0)
                    continue;

                if (instance.InstanceIP != 0 && instance.IPv4_String[0] != '\0') {
                    DiscoveredCount++;

                    ImGui::PushID(static_cast<int>(id));
                    bool Selected = (strcmp(ManualIPBuffer, instance.IPv4_String) == 0);

                    ImVec2 CardMin = ImGui::GetCursorScreenPos();
                    float CardWidth = ImGui::GetContentRegionAvail().x;
                    float CardHeight = 44.0f;
                    ImVec2 CardMax = ImVec2(CardMin.x + CardWidth, CardMin.y + CardHeight);

                    ImGui::InvisibleButton("##DevCard", ImVec2(CardWidth, CardHeight));
                    bool Hovered = ImGui::IsItemHovered();
                    if (ImGui::IsItemClicked()) {
                        strncpy(ManualIPBuffer, instance.IPv4_String, sizeof(ManualIPBuffer));
                    }

                    ImU32 CardBg = Selected  ? OmniTheme::COL_HANDSHAKE_CARD_BG
                                   : Hovered ? OmniTheme::COL_HANDSHAKE_CHECK_BG_HOVER
                                             : OmniTheme::COL_HANDSHAKE_CHECK_BG;
                    ImU32 CardBrd = Selected || Hovered ? OmniTheme::COL_HANDSHAKE_CARD_BRD
                                                        : OmniTheme::COL_HANDSHAKE_CHECK_BRD;

                    DrawList->AddRectFilled(CardMin, CardMax, CardBg, 10.0f);
                    DrawList->AddRect(CardMin, CardMax, CardBrd, 10.0f, 0, 1.2f);

                    // Device Icon
                    ImGui::PushFont(OmniIconsSmall);
                    ImVec2 IconSize = ImGui::CalcTextSize(IC_AIRPLAY);
                    DrawList->AddText(
                        ImVec2(CardMin.x + 14.0f, CardMin.y + (CardHeight - IconSize.y) * 0.5f),
                        OmniTheme::COL_HANDSHAKE_CARD_ICON,
                        IC_AIRPLAY
                    );
                    ImGui::PopFont();

                    // Device Name
                    const char* DevName =
                        instance.InstanceName[0] ? instance.InstanceName : "Device Node";
                    ImGui::PushFont(InterMed14);
                    DrawList->AddText(
                        ImVec2(CardMin.x + 38.0f, CardMin.y + 6.0f),
                        ImGui::GetColorU32(COL4_TEXT_ACTIVE),
                        DevName
                    );
                    ImGui::PopFont();

                    // Device IP
                    ImGui::PushFont(InterMed12);
                    DrawList->AddText(
                        ImVec2(CardMin.x + 38.0f, CardMin.y + 24.0f),
                        ImGui::GetColorU32(OmniTheme::COL4_HANDSHAKE_TITLE),
                        instance.IPv4_String
                    );
                    ImGui::PopFont();

                    ImGui::PopID();
                    ImGui::Dummy(ImVec2(0.0f, 4.0f));
                }
            }
        }

        if (DiscoveredCount == 0) {
            ImVec2 BoxMin = ImGui::GetCursorScreenPos();
            float BoxWidth = ImGui::GetContentRegionAvail().x;
            float BoxHeight = 52.0f;
            ImVec2 BoxMax = ImVec2(BoxMin.x + BoxWidth, BoxMin.y + BoxHeight);

            DrawList->AddRectFilled(BoxMin, BoxMax, OmniTheme::COL_HANDSHAKE_CHECK_BG, 10.0f);
            DrawList->AddRect(BoxMin, BoxMax, OmniTheme::COL_HANDSHAKE_CHECK_BRD, 10.0f, 0, 1.0f);

            ImGui::PushFont(OmniIconsSmall);
            ImVec2 IcSize = ImGui::CalcTextSize(IC_WIFI);
            DrawList->AddText(
                ImVec2(BoxMin.x + 16.0f, BoxMin.y + (BoxHeight - IcSize.y) * 0.5f),
                OmniTheme::COL_HANDSHAKE_TRUST_LABEL,
                IC_WIFI
            );
            ImGui::PopFont();

            ImGui::PushFont(InterMed12);
            DrawList->AddText(
                ImVec2(BoxMin.x + 38.0f, BoxMin.y + (BoxHeight - 14.0f) * 0.5f),
                ImGui::GetColorU32(COL4_TEXT_MUTED),
                "No active broadcast instances found. Click Scan Network."
            );
            ImGui::PopFont();

            ImGui::Dummy(ImVec2(BoxWidth, BoxHeight));
        }

        ImGui::Dummy(ImVec2(0.0f, 14.0f));
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 14.0f));

        // Action Buttons
        float ContentWidth = ImGui::GetContentRegionAvail().x;
        float BtnHeight = 38.0f;

        ImGui::PushFont(InterMed14);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

        // Scan Network
        ImGui::PushStyleColor(ImGuiCol_Button, OmniTheme::COL4_BTN_DECLINE_BG);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, OmniTheme::COL4_BTN_DECLINE_BG_HOVER);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, OmniTheme::COL4_BTN_DECLINE_BG_ACTIVE);
        ImGui::PushStyleColor(ImGuiCol_Border, OmniTheme::COL4_BTN_DECLINE_BRD);
        ImGui::PushStyleColor(ImGuiCol_Text, OmniTheme::COL4_BTN_DECLINE_TXT);

        if (ImGui::Button("Scan Network", ImVec2(125.0f, BtnHeight))) {
            OmniAPI::Scan();
        }

        ImGui::PopStyleColor(5);

        // Cancel and Connect
        float RightButtonsWidth = 90.0f + 10.0f + 105.0f;
        ImGui::SameLine(ContentWidth - RightButtonsWidth);

        // Cancel
        ImGui::PushStyleColor(ImGuiCol_Button, OmniTheme::COL4_BTN_DECLINE_BG);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, OmniTheme::COL4_BTN_DECLINE_BG_HOVER);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, OmniTheme::COL4_BTN_DECLINE_BG_ACTIVE);
        ImGui::PushStyleColor(ImGuiCol_Border, OmniTheme::COL4_BTN_DECLINE_BRD);
        ImGui::PushStyleColor(ImGuiCol_Text, OmniTheme::COL4_BTN_DECLINE_TXT);

        if (ImGui::Button("Cancel", ImVec2(90.0f, BtnHeight))) {
            ShowConnectModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopStyleColor(5);

        ImGui::SameLine(0.0f, 10.0f);

        // Connect
        ImGui::PushStyleColor(ImGuiCol_Button, OmniTheme::COL4_BTN_ACCEPT_BG);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, OmniTheme::COL4_BTN_ACCEPT_BG_HOVER);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, OmniTheme::COL4_BTN_ACCEPT_BG_ACTIVE);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, OmniTheme::COL4_BTN_ACCEPT_TXT);

        if (ImGui::Button("Connect", ImVec2(105.0f, BtnHeight))) {
            if (ManualIPBuffer[0] != '\0') {
                ConnectionRequest Req;
                Req.DeviceID = TargetSlotForAdd;
                OmniAPI::Connect(Req);
            }
            ShowConnectModal = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopStyleColor(5);

        ImGui::PopStyleVar(2);
        ImGui::PopFont();

        ImGui::EndPopup();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
}
