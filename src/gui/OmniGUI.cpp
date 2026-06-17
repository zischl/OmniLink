#include "OmniGUI.h"
#include "InterFonts.h"
#include "JetBrainsFonts.h"
#include "OmniLink.h"

#include "DummyInstances.h"
#include "OmniIcons.h"

#include "imgui.h"
#include "imgui_internal.h"

OmniGUI::OmniGUI(OmniLink& OmniLinkInstance)
    : App(OmniLinkInstance), SelectedDevice(OmniLink::SelectedTargetDevice)
{
    AvailableInstances = &DummyAvailableInstances;
    ActiveInstances = App.GetActiveInstances();
}

void OmniGUI::SetupImGui(HWND hwnd, ID3D11Device* D3D11Device, ID3D11DeviceContext* D3D11Context)
{

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(D3D11Device, D3D11Context);

    ImGuiStyle& style = ImGui::GetStyle();

    style.Colors[ImGuiCol_ModalWindowDimBg] = STYLE_MODAL_DIM;
    style.Colors[ImGuiCol_ChildBg] = STYLE_CHILD_BG;

    style.Colors[ImGuiCol_Separator] = STYLE_SEPARATOR;
    style.Colors[ImGuiCol_SeparatorHovered] = STYLE_SEPARATOR;
    style.Colors[ImGuiCol_SeparatorActive] = STYLE_SEPARATOR;

    style.SeparatorTextBorderSize = 1.0f;
    style.ItemSpacing.x = 0.0f;

    ImFontConfig FontCFG;
    FontCFG.FontDataOwnedByAtlas = false;

    InterReg14 = io.Fonts->AddFontFromMemoryTTF(Inter18Regular, Inter18RegularLen, 14.0f, &FontCFG);
    InterMed14 = io.Fonts->AddFontFromMemoryTTF(Inter18Medium, Inter18MediumLen, 14.0f, &FontCFG);
    InterMed16 = io.Fonts->AddFontFromMemoryTTF(Inter18Medium, Inter18MediumLen, 16.0f, &FontCFG);
    JetBrainsMed15 = io.Fonts->AddFontFromMemoryTTF(
        JetBrainsMonoMedium, JetBrainsMonoMediumLen, 15.0f, &FontCFG);
    JetBrainsBold16 =
        io.Fonts->AddFontFromMemoryTTF(JetBrainsMonoBold, JetBrainsMonoBoldLen, 16.0f, &FontCFG);

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
void OmniGUI::DeviceIconPreview(const ImVec2& pos,
                                const ImU32& col,
                                const ImVec2& text_size,
                                const char* text)
{
    DrawList->AddRect(ImVec2(pos.x - 50, pos.y - 40),
                      ImVec2(pos.x + 50, pos.y + 40),
                      col,
                      5.0f,
                      0,
                      2.0f); // monitor
    DrawList->AddText(ImVec2(pos.x - (text_size.x * 0.5f), pos.y), col, text);
    DrawList->AddRect(ImVec2(pos.x, pos.y + 40), ImVec2(pos.x, pos.y + 55), col, 5.0f, 0, 2.0f);
    DrawList->AddRect(ImVec2(pos.x - 15, pos.y + 55),
                      ImVec2(pos.x + 15, pos.y + 55),
                      col,
                      10.0f,
                      0,
                      1.0f); // stand
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

    ImRect Bbox =
        ImRect(IconRenderPos,
               ImVec2(IconRenderPos.x + IconRenderSize.x, IconRenderPos.y + IconRenderSize.y));

    ImGui::ItemAdd(Bbox, Id, NULL, ImGuiItemFlags_None);

    bool Hovered, Held;
    bool Pressed = ImGui::ButtonBehavior(Bbox, Id, &Hovered, &Held, 0);
    ImGui::RenderNavCursor(Bbox, Id);

    ImU32 Col = Hovered || Held ? COL_DEV_HOVER : COL_DEV_DEFAULT;

    DrawList->AddText(IconRenderPos, Col, IC_AIRPLAY);
    ImGui::PopFont();

    // The IP , imma go with name later
    ImVec2 TextSize = ImGui::CalcTextSize(DeviceData->IPv4_String);
    float HalfWidth = TextSize.x * 0.5f;
    ImVec2 TextPos = ImVec2(Pos.x - HalfWidth, Pos.y + (IconRenderSize.y * 0.5) + 5.0f);
    DrawList->AddText(TextPos, Col, DeviceData->IPv4_String);

    // Drag and Drop Handling
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(
            "DeviceInfo", &(DeviceData->DevMapIndex), sizeof(DeviceData->DevMapIndex));
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
        if (ImGui::MenuItem("Connect Instance")) {
            ConnectionRequest Request;
            Request.DeviceID = static_cast<DeviceMap>(DeviceData->DevMapIndex);
            OmniAPI::Connect(Request);
        }
        ImGui::EndPopup();
    }

    ImGui::PopID();
}

bool OmniGUI::VerticalMenuItem(const char* Label,
                               const char* Icon,
                               bool State,
                               ImVec2& MenuItemSize)
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
    ImVec2 GlyphPos = ImVec2(IconBoxMin.x + (IconSize.x - GlyphSize.x) * 0.5f,
                             IconBoxMin.y + (IconSize.y - GlyphSize.y) * 0.5f);

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

bool OmniGUI::IconizedButton(const char* Label,
                             const char* Icon,
                             bool State,
                             const ImVec2& ButtonSize)
{
    ImGui::PushID(Label);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* DrawList = ImGui::GetWindowDrawList();

    bool clicked = ImGui::InvisibleButton(Label, ButtonSize);
    bool hovered = ImGui::IsItemHovered();

    // Button BG
    if (State) {
        DrawList->AddRectFilled(
            pos, ImVec2(pos.x + ButtonSize.x, pos.y + ButtonSize.y), COL_FEAT_BG_ACTIVE);
    } else if (hovered) {
        DrawList->AddRectFilled(
            pos, ImVec2(pos.x + ButtonSize.x, pos.y + ButtonSize.y), COL_FEAT_BG_HOVER);
    }

    const ImVec2 IconSize = ImVec2(44.0f, 44.0f);
    ImVec2 IconBoxPos = ImVec2(pos.x + (ButtonSize.x - IconSize.x) * 0.5f, pos.y + 14.0f);

    ImU32 IconBgColor = State ? COL_FEAT_IC_ACTIVE : COL_FEAT_IC_HOVER;
    // Icon BG
    DrawList->AddRectFilled(IconBoxPos,
                            ImVec2(IconBoxPos.x + IconSize.x, IconBoxPos.y + IconSize.y),
                            IconBgColor,
                            12.0f);

    // Da Icon
    ImGui::PushFont(OmniIconsMedium);
    ImVec2 IconTextSize = ImGui::CalcTextSize(Icon);
    ImVec2 IconTextPos = ImVec2(IconBoxPos.x + (IconSize.x - IconTextSize.x) * 0.5f,
                                IconBoxPos.y + (IconSize.y - IconTextSize.y) * 0.5f);
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
            StripPosLeft, stripPosRight, COL_FEAT_STRIP, 1.5f, ImDrawFlags_RoundCornersTop);

        DrawList->AddRectFilledMultiColor(ImVec2(StripPosLeft.x, StripPosLeft.y - 8),
                                          stripPosRight,
                                          IM_COL32_BLACK_TRANS,
                                          IM_COL32_BLACK_TRANS,
                                          COL_FEAT_GLOW,
                                          COL_FEAT_GLOW);
    }

    ImGui::PopID();
    return clicked;
}
void OmniGUI::DeviceAddButton(const ImVec2& CenterPos, ImU32 Color)
{
    ImVec2 TextSize = ImGui::CalcTextSize(IC_DIAMOND_PLUS); // egfeg
    ImVec2 RenderPos = ImVec2(CenterPos.x - (TextSize.x * 0.5f), CenterPos.y - (TextSize.y * 0.5f));

    // Yes.. It's the Icon again
    ImGui::PushFont(OmniIconsLarge);
    DrawList->AddText(RenderPos, Color, IC_DIAMOND_PLUS);
    ImGui::PopFont();
}

void OmniGUI::CenterItemX(const float ItemWidth)
{
    float Space = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((Space - ItemWidth) * 0.5f);
}

void OmniGUI::ConnectionRing(const char* Label, const ImVec2& WidgetSize, const float Radius)
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
    int DiagonalAxe = static_cast<int>(Radius * 0.707106f);

    auto& Devices = *AvailableInstances;

    // Center Device
    DeviceIcon("C0", Pos, &Devices[DeviceMap::C0]);

    // Left
    if (Devices[DeviceMap::L1].InstanceIP) {
        DeviceIcon("L1", ImVec2(Pos.x - Radius, Pos.y), &Devices[DeviceMap::L1]);
    } else {
        DeviceAddButton(ImVec2(Pos.x - Radius, Pos.y), COL_DEV_EMPTY);
    }

    // Left-Up
    if (Devices[DeviceMap::LU1].InstanceIP) {
        DeviceIcon(
            "LU1", ImVec2(Pos.x - DiagonalAxe, Pos.y - DiagonalAxe), &Devices[DeviceMap::LU1]);
    } else {
        DeviceAddButton(ImVec2(Pos.x - DiagonalAxe, Pos.y - DiagonalAxe), COL_DEV_EMPTY);
    }

    // Up
    if (Devices[DeviceMap::U1].InstanceIP) {
        DeviceIcon("U1", ImVec2(Pos.x, Pos.y - Radius), &Devices[DeviceMap::U1]);
    } else {
        DeviceAddButton(ImVec2(Pos.x, Pos.y - Radius), COL_DEV_EMPTY);
    }

    // Right-Up
    if (Devices[DeviceMap::RU1].InstanceIP) {
        DeviceIcon(
            "RU1", ImVec2(Pos.x + DiagonalAxe, Pos.y - DiagonalAxe), &Devices[DeviceMap::RU1]);
    } else {
        DeviceAddButton(ImVec2(Pos.x + DiagonalAxe, Pos.y - DiagonalAxe), COL_DEV_EMPTY);
    }

    // Right
    if (Devices[DeviceMap::R1].InstanceIP) {
        DeviceIcon("R1", ImVec2(Pos.x + Radius, Pos.y), &Devices[DeviceMap::R1]);
    } else {
        DeviceAddButton(ImVec2(Pos.x + Radius, Pos.y), COL_DEV_EMPTY);
    }

    // Right-Down
    if (Devices[DeviceMap::RD1].InstanceIP) {
        DeviceIcon(
            "RD1", ImVec2(Pos.x + DiagonalAxe, Pos.y + DiagonalAxe), &Devices[DeviceMap::RD1]);
    } else {
        DeviceAddButton(ImVec2(Pos.x + DiagonalAxe, Pos.y + DiagonalAxe), COL_DEV_EMPTY);
    }

    // Down
    if (Devices[DeviceMap::D1].InstanceIP) {
        DeviceIcon("D1", ImVec2(Pos.x, Pos.y + Radius), &Devices[DeviceMap::D1]);
    } else {
        DeviceAddButton(ImVec2(Pos.x, Pos.y + Radius), COL_DEV_EMPTY);
    }

    // Left-Down
    if (Devices[DeviceMap::LD1].InstanceIP) {
        DeviceIcon(
            "LD1", ImVec2(Pos.x - DiagonalAxe, Pos.y + DiagonalAxe), &Devices[DeviceMap::LD1]);
    } else {
        DeviceAddButton(ImVec2(Pos.x - DiagonalAxe, Pos.y + DiagonalAxe), COL_DEV_EMPTY);
    }

    ImGui::PopFont();
}

void OmniGUI::MetricDashboard(
    const char* ContainerId, const MetricItem* Items, int ItemCount, float TotalWidth, float Height)
{
    if (ItemCount <= 0 || !Items)
        return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    if (ImGui::BeginChild(ContainerId,
                          ImVec2(TotalWidth, Height),
                          ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
        ImGui::PopStyleVar();

        ImVec2 BarMin = ImGui::GetCursorScreenPos();
        ImVec2 BarMax = ImVec2(BarMin.x + TotalWidth, BarMin.y + Height);

        ImU32 ContainerBgColor = ImGui::ColorConvertFloat4ToU32(DASH_BG);
        ImU32 ContainerBorderColor = ImGui::ColorConvertFloat4ToU32(DASH_BORDER);

        // Main Container
        DrawList->AddRectFilled(BarMin, BarMax, ContainerBgColor, 0.0f);

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
                DrawList->AddLine(ImVec2(SegMax.x, SegMin.y),
                                  ImVec2(SegMax.x, SegMax.y),
                                  ContainerBorderColor,
                                  1.0f);
            }

            // Vertical Accent Pill
            ImVec2 PillMin = ImVec2(SegMin.x + InnerPaddingX, SegMin.y + InnerPaddingY + 2.0f);
            ImVec2 PillMax = ImVec2(PillMin.x + AccentPillWidth, SegMax.y - InnerPaddingY - 2.0f);
            ImU32 AccentCol = ImGui::ColorConvertFloat4ToU32(DASH_BORDER);
            DrawList->AddRectFilled(PillMin, PillMax, AccentCol, 1.5f);

            float TextOriginX = PillMax.x + AccentGapping;
            ImVec2 TitlePos = ImVec2(TextOriginX, SegMin.y + InnerPaddingY);
            ImGui::SetCursorScreenPos(TitlePos);

            // Da Title
            ImGui::PushStyleColor(ImGuiCol_Text, DASH_TEXT_MUTED);
            ImGui::TextUnformatted(Items[i].title);
            ImGui::PopStyleColor();

            float LineHeight = ImGui::GetTextLineHeight();
            ImVec2 ValuePos = ImVec2(TextOriginX, TitlePos.y + LineHeight - 1.0f);
            ImGui::SetCursorScreenPos(ValuePos);

            // Value
            ImGui::PushFont(JetBrainsBold16);
            ImGui::PushStyleColor(ImGuiCol_Text, DASH_TEXT_VALUE);
            ImGui::TextUnformatted(Items[i].value);
            ImGui::PopStyleColor();
            ImGui::PopFont();
        }

        ImGui::PopFont();
    }
    ImGui::EndChild();

    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(1);
}

bool OmniGUI::HandleEvent(ConnectionRequest& request, float timeout)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 windowPos = ImGui::GetWindowPos();
    float windowWidth = ImGui::GetWindowWidth();

    drawList->AddRectFilled(ImVec2(windowPos.x + 10.0f, windowPos.y),
                            ImVec2(windowPos.x - 10.0f + windowWidth, windowPos.y + 3.0f),
                            COL_MODAL_STRIP,
                            16.0f,
                            ImDrawFlags_RoundCornersTop);

    float paddingTop = 21.0f;
    ImVec2 circleCenter =
        ImVec2(windowPos.x + (windowWidth / 2.0f), windowPos.y + 25.0f + paddingTop);
    const float radius = 14.0f;

    float boxSize = 48.0f;
    float boxSpacing = 18.0f;
    float rounding = 12.0f;
    float boxTop = circleCenter.y - (boxSize / 2.0f);
    float botBottom = boxTop + boxSize;

    ImVec2 leftRectBegin = ImVec2(circleCenter.x - boxSpacing - boxSize, boxTop);
    ImVec2 leftRectEnd = ImVec2(circleCenter.x - boxSpacing, botBottom);
    ImVec2 rightRectBegin = ImVec2(circleCenter.x + boxSpacing, boxTop);
    ImVec2 rightRectEnd = ImVec2(circleCenter.x + boxSpacing + boxSize, botBottom);

    drawList->AddRectFilled(leftRectBegin, leftRectEnd, COL_MODAL_BOX_BG, rounding);
    drawList->AddRect(leftRectBegin, leftRectEnd, COL_MODAL_BOX_BRD, rounding, 0, 1.5f);

    drawList->AddRectFilled(rightRectBegin, rightRectEnd, COL_MODAL_BOX_BG, rounding);
    drawList->AddRect(rightRectBegin, rightRectEnd, COL_MODAL_BOX_BRD, rounding, 0, 1.5f);

    drawList->AddCircleFilled(circleCenter, radius, COL_MODAL_CIRC_BG);
    drawList->AddCircle(circleCenter, radius, COL_MODAL_CIRC_BRD, 0, 1.5f);

    const char* arrowText = "⇄";
    ImGui::SetWindowFontScale(11.0f / 18.0f);
    ImVec2 arrowSize = ImGui::CalcTextSize(arrowText);
    drawList->AddText(
        ImVec2(circleCenter.x - (arrowSize.x / 2.0f), circleCenter.y - (arrowSize.y / 2.0f)),
        IM_COL32_WHITE,
        arrowText);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Dummy(ImVec2(windowWidth, boxSize + paddingTop + 14.0f));

    const char* txtConnReq = "CONNECTION REQUEST";
    ImGui::SetWindowFontScale(11.0f / 18.0f);
    float txtConnReqWidth = ImGui::CalcTextSize(txtConnReq).x;
    ImGui::SetCursorPosX((windowWidth - txtConnReqWidth) / 2.0f);
    ImGui::TextColored(EV_TEXT_MUTED, "%s", txtConnReq);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    const char* deviceName = "DESKTOP-7K3MX2";
    ImGui::SetWindowFontScale(17.0f / 18.0f);
    float deviceNameWidth = ImGui::CalcTextSize(deviceName).x;
    ImGui::SetCursorPosX((windowWidth - deviceNameWidth) / 2.0f);
    ImGui::TextUnformatted(deviceName);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    const char* ipSub = "192.168.1.42  Local Network";
    ImGui::SetWindowFontScale(12.0f / 18.0f);
    float ipSubWidth = ImGui::CalcTextSize(ipSub).x;
    ImGui::SetCursorPosX((windowWidth - ipSubWidth) / 2.0f);
    ImGui::TextColored(EV_TEXT_MUTED, "%s", ipSub);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Dummy(ImVec2(0.0f, 14.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 14.0f));

    if (ImGui::BeginTable("meta_info_row", 3, ImGuiTableFlags_NoBordersInBody)) {
        ImGui::TableSetupColumn("C1", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("C2", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("C3", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::SetWindowFontScale(10.0f / 18.0f);

        ImGui::TableSetColumnIndex(0);
        ImGui::SetCursorPosX(
            ImGui::GetCursorPosX() +
            (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("PROTOCOL").x) * 0.5f);
        ImGui::TextColored(EV_TEXT_MUTED, "PROTOCOL");

        ImGui::TableSetColumnIndex(1);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("PORT").x) *
                                 0.5f);
        ImGui::TextColored(EV_TEXT_MUTED, "PORT");

        ImGui::TableSetColumnIndex(2);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("LATENCY").x) *
                                 0.5f);
        ImGui::TextColored(EV_TEXT_MUTED, "LATENCY");

        ImGui::TableNextRow();
        ImGui::SetWindowFontScale(13.0f / 18.0f);

        ImGui::TableSetColumnIndex(0);
        ImGui::SetCursorPosX(
            ImGui::GetCursorPosX() +
            (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("[V] [M] [A]").x) * 0.5f);
        ImGui::TextColored(EV_ACCENT, "[V] [M] [A]");

        ImGui::TableSetColumnIndex(1);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("7474").x) *
                                 0.5f);
        ImGui::TextUnformatted("7474");

        ImGui::TableSetColumnIndex(2);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("4 ms").x) *
                                 0.5f);
        ImGui::TextUnformatted("4 ms");

        ImGui::SetWindowFontScale(1.0f);
        ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0.0f, 14.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    ImGui::PushStyleColor(ImGuiCol_FrameBg, EV_FRAME_BG);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, EV_FRAME_BG_HOVER);
    ImGui::PushStyleColor(ImGuiCol_CheckMark, EV_ACCENT);

    ImGui::SetWindowFontScale(12.5f / 18.0f);
    ImGui::Checkbox("🛡 Trust this device permanently", &request.Verified);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::PopStyleColor(3);
    ImGui::Dummy(ImVec2(0.0f, 16.0f));

    bool actionState = false;
    float Spacing = 24.0f;
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float individualButtonWidth = (availableWidth - Spacing) / 2.0f;
    float actionHeight = 40.0f;

    ImGui::SetWindowFontScale(13.0f / 18.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, EV_BTN_DECLINE);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EV_BTN_DEC_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EV_BTN_DEC_ACTIVE);
    ImGui::PushStyleColor(ImGuiCol_Border, EV_BTN_DEC_BORDER);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);

    if (ImGui::Button("Decline", ImVec2(individualButtonWidth, actionHeight))) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    ImGui::SameLine(0.0f, Spacing);

    ImGui::PushStyleColor(ImGuiCol_Button, EV_ACCENT);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EV_ACCENT_HOVER);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, EV_ACCENT_ACTIVE);

    if (ImGui::IsWindowAppearing()) {
        ImGui::SetKeyboardFocusHere(-1);
    }

    if (ImGui::Button("Accept", ImVec2(individualButtonWidth, actionHeight))) {
        ImGui::CloseCurrentPopup();
        actionState = true;
    }
    ImGui::PopStyleColor(3);
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Dummy(ImVec2(0.0f, 24.0f));
    ImGui::PopStyleVar(2);

    return actionState;
}

bool OmniGUI::HandleEvent(Alert& request, float timeout)
{
    return true;
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

    ImU32 color = ImGui::GetColorU32(GLOW_OPAQUE);
    float thickness = 1.0f;
    int segments = 20;

    DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness, segments);

    int glow_range = 1;

    color = ImGui::GetColorU32(GLOW_HIGH);
    DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness + glow_range, segments);

    color = ImGui::GetColorU32(GLOW_MED);
    DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness + glow_range * 2, segments);

    color = ImGui::GetColorU32(GLOW_LOW);
    DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness + glow_range * 2.5, segments);

    ImGui::PopID();
}
