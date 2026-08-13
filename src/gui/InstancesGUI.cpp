#include "OmniGUI.h"
#include <cstdio>
#include <string>
#include <vector>

namespace {
static constexpr const char* DEFAULT_LAST_PAIRED_DATE = "xxxx-xx-xx";
}

void OmniGUI::InstancesHeader(ImVec2 HeaderStartPos, float AvailHeaderWidth)
{
    ImDrawList* DrawListPtr = ImGui::GetWindowDrawList();

    ImGui::SetCursorScreenPos(
        ImVec2(HeaderStartPos.x + AvailHeaderWidth - 290.0f, HeaderStartPos.y + 2.0f)
    );

    // Some buttons..
    if (IconButton(IC_REFRESH_CW, "Scan Network", ImVec2(138, 36), SecondaryDarkBtnCols)) {
        OmniAPI::Scan();
    }
    ImGui::SameLine(0, 10.0f);
    if (IconButton(IC_DIAMOND_PLUS, "Add Instance", ImVec2(140, 36), PrimaryPurpleBtnCols)) {
        ShowConnectModal = true;
    }

    ImGui::SetCursorScreenPos(ImVec2(HeaderStartPos.x, HeaderStartPos.y));
    ImGui::Spacing();

    // Nav bar..
    ImVec2 TabContainerPos = ImGui::GetCursorScreenPos();
    float TabItemWidth = 195.0f;
    float TabItemHeight = 34.0f;
    float TotalSwitcherWidth = (TabItemWidth * 2) + 6.0f;
    float TotalSwitcherHeight = TabItemHeight + 6.0f;

    DrawListPtr->AddRectFilled(
        TabContainerPos,
        ImVec2(TabContainerPos.x + TotalSwitcherWidth, TabContainerPos.y + TotalSwitcherHeight),
        IM_COL32(18, 16, 26, 240),
        10.0f
    );
    DrawListPtr->AddRect(
        TabContainerPos,
        ImVec2(TabContainerPos.x + TotalSwitcherWidth, TabContainerPos.y + TotalSwitcherHeight),
        IM_COL32(48, 40, 70, 255),
        10.0f,
        0,
        1.0f
    );

    ImGui::SetCursorScreenPos(ImVec2(TabContainerPos.x + 3.0f, TabContainerPos.y + 3.0f));

    RenderNavItem(IC_AIRPLAY, "Instances Profiles", 0, TabItemWidth, TabItemHeight);
    ImGui::SameLine(0.0f, 0.0f);
    RenderNavItem(IC_SHIELD, "Trusted Instances", 1, TabItemWidth, TabItemHeight);

    ImGui::SetCursorScreenPos(
        ImVec2(HeaderStartPos.x, TabContainerPos.y + TotalSwitcherHeight + 14.0f)
    );
    ImGui::Separator();
    ImGui::Spacing();
}

void OmniGUI::RenderNavItem(
    const char* Icon, const char* Label, int TabIdx, float ItemWidth, float ItemHeight
)
{
    bool NavItemState = (InstancesSubTab == TabIdx);

    ImGui::PushID(TabIdx + 3000);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, NavItemState ? 1.0f : 0.0f);

    if (NavItemState) {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(75, 45, 125, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(90, 52, 145, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(65, 38, 110, 255));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(168, 85, 247, 255));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(35, 30, 52, 180));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(45, 38, 66, 220));
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 170, 255));
    }

    ImVec2 ItemPos = ImGui::GetCursorScreenPos();
    char PaddedLabel[128];
    snprintf(PaddedLabel, sizeof(PaddedLabel), "     %s", Label);

    ImGui::PushFont(InterMed14);
    if (ImGui::Button(PaddedLabel, ImVec2(ItemWidth, ItemHeight))) {
        InstancesSubTab = TabIdx;
    }
    ImGui::PopFont();

    if (Icon && Icon[0] != '\0') {
        ImGui::PushFont(OmniIconsSmall);
        ImVec2 IcSz = ImGui::CalcTextSize(Icon);
        DrawList->AddText(
            ImVec2(ItemPos.x + 16.0f, ItemPos.y + (ItemHeight - IcSz.y) * 0.5f),
            NavItemState ? IM_COL32(210, 150, 255, 255) : IM_COL32(140, 140, 160, 255),
            Icon
        );
        ImGui::PopFont();
    }

    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);
    ImGui::PopID();
}

void OmniGUI::InstanceGroupsSection(float AvailableWidth)
{
    ImVec2 StartPos = ImGui::GetCursorScreenPos();

    SectionHeader(
        IC_WAYPOINTS, "Instance Groups", "Saved Instance Mappings for one click setup mangement"
    );
    ImVec2 CardsStartPos = ImGui::GetCursorScreenPos();

    ImGui::SetCursorScreenPos(ImVec2(StartPos.x + AvailableWidth - 175.0f, StartPos.y + 6.0f));
    if (IconButton(IC_DIAMOND_PLUS, "Save Current Group", ImVec2(175, 32), PrimaryPurpleBtnCols)) {
        char GroupName[64];
        snprintf(
            GroupName,
            sizeof(GroupName),
            "Preset Group %zu",
            OmniAPI::GetInstanceGroups().size() + 1
        );
        OmniAPI::SaveCurrentGroup(GroupName, "Nothing Special");
        Notification Notif;
        Notif.Event = Alert("Group Created", "Saved active setup as ", GroupName);
        Notif.EventName = "Group Saved";
        Notif.Layout = Notification::BOTTOM_RIGHT;
        Notif.Timeout = 3.0f;
        PushNotification(Notif);
    }

    ImGui::SetCursorScreenPos(CardsStartPos);

    const auto& InstanceGroups = OmniAPI::GetInstanceGroups();
    const float GroupCardWidth = (AvailableWidth - 16.0f) * 0.5f;
    const float GroupCardHeight = 140.0f;

    for (size_t Row = 0; Row < InstanceGroups.size(); Row += 2) {
        ImVec2 RowStartPos = ImGui::GetCursorScreenPos();

        for (size_t Col = 0; Col < 2; ++Col) {
            size_t Idx = Row + Col;
            if (Idx >= InstanceGroups.size())
                break;

            float CardX = RowStartPos.x + Col * (GroupCardWidth + 16.0f);
            float CardY = RowStartPos.y;

            GroupCardAction Act = InstanceGroupCard(
                InstanceGroups[Idx], Idx, ImVec2(CardX, CardY), GroupCardWidth, GroupCardHeight
            );

            if (Act == GroupCardAction::ConnectGroup) {
                OmniAPI::ConnectGroup(Idx);
            } else if (Act == GroupCardAction::DeleteGroup) {
                OmniAPI::RemoveInstanceGroup(Idx);
                break;
            } else if (Act == GroupCardAction::ConfigureSettings) {
                // Left empty for future configuration options
            }
        }

        ImGui::SetCursorScreenPos(ImVec2(RowStartPos.x, RowStartPos.y + GroupCardHeight + 14.0f));
        ImGui::Dummy(ImVec2(AvailableWidth, 0.0f));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void OmniGUI::ActiveInstancesSection(float AvailableWidth)
{
    SectionHeader(
        IC_SERVER, "Active Network Instances", "Currently linked and available remote target nodes"
    );

    size_t ValidCount = 0;
    if (AvailableInstances) {
        for (const auto& [ID, Inst] : *AvailableInstances) {
            if (ID != DeviceMap::C0 && Inst.InstanceIP != 0) {
                ValidCount++;
            }
        }
    }

    if (ValidCount == 0) {
        ImVec2 EmptyPos = ImGui::GetCursorScreenPos();
        DrawList->AddRectFilled(
            EmptyPos, ImVec2(EmptyPos.x + AvailableWidth, EmptyPos.y + 60.0f), COL_BG_CHILD_1, 8.0f
        );
        DrawList->AddRect(
            EmptyPos, ImVec2(EmptyPos.x + AvailableWidth, EmptyPos.y + 60.0f), COL_BORDER, 8.0f
        );
        ImGui::SetCursorScreenPos(ImVec2(EmptyPos.x + 20.0f, EmptyPos.y + 20.0f));
        ImGui::PushFont(InterMed15);
        ImGui::TextColored(
            COL4_TEXT_MUTED,
            "No active instances connected. Click 'Scan Network' or '+ Add Instance' above."
        );
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2(EmptyPos.x, EmptyPos.y + 70.0f));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    } else {
        const float CardWidth = (AvailableWidth - 16.0f) * 0.5f;
        const float CardHeight = 74.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));

        size_t CardIdx = 0;
        ImVec2 RowStartPos = ImGui::GetCursorScreenPos();

        for (const auto& [ID, Inst] : *AvailableInstances) {
            if (ID == DeviceMap::C0 || Inst.InstanceIP == 0)
                continue;

            size_t Col = CardIdx % 2;
            if (CardIdx > 0 && Col == 0) {
                RowStartPos = ImGui::GetCursorScreenPos();
            }

            float CardX = RowStartPos.x + Col * (CardWidth + 16.0f);
            float CardY = RowStartPos.y;

            ActiveCardAction Act =
                ActiveInstanceCard(ID, Inst, CardIdx, ImVec2(CardX, CardY), CardWidth, CardHeight);

            if (Act == ActiveCardAction::Disconnect) {
                OmniAPI::CancelHandshake(ID);
            } else if (Act == ActiveCardAction::Connect) {
                OmniAPI::Connect(ConnectionRequest{ID});
            } else if (Act == ActiveCardAction::ConfigureSettings) {
                SelectedDevice = ID;
                ActiveMenu = 3;
            }

            if (Col == 1) {
                ImGui::SetCursorScreenPos(
                    ImVec2(RowStartPos.x, RowStartPos.y + CardHeight + 14.0f)
                );
                ImGui::Dummy(ImVec2(AvailableWidth, 0.0f));
            }
            CardIdx++;
        }

        if (CardIdx % 2 != 0) {
            ImGui::SetCursorScreenPos(ImVec2(RowStartPos.x, RowStartPos.y + CardHeight + 14.0f));
            ImGui::Dummy(ImVec2(AvailableWidth, 0.0f));
        }

        ImGui::PopStyleVar(2);
    }
}

void OmniGUI::TrustedNodesSection(float AvailableWidth)
{
    SectionHeader(
        IC_SHIELD,
        "Trusted Node Registry",
        "Persistent paired instances authenticated via ECDH X25519 key exchanges"
    );

    const auto* Qrypt = OmniAPI::GetQryptManager();
    const auto* TokensMap = Qrypt ? &Qrypt->GetTrustedPairingTokens() : nullptr;

    if (!TokensMap || TokensMap->empty()) {
        ImVec2 EmptyPos = ImGui::GetCursorScreenPos();
        DrawList->AddRectFilled(
            EmptyPos, ImVec2(EmptyPos.x + AvailableWidth, EmptyPos.y + 60.0f), COL_BG_CHILD_1, 8.0f
        );
        DrawList->AddRect(
            EmptyPos, ImVec2(EmptyPos.x + AvailableWidth, EmptyPos.y + 60.0f), COL_BORDER, 8.0f
        );
        ImGui::SetCursorScreenPos(ImVec2(EmptyPos.x + 20.0f, EmptyPos.y + 20.0f));
        ImGui::PushFont(InterMed15);
        ImGui::TextColored(
            COL4_TEXT_MUTED,
            "No trusted nodes registered yet. Accept connections with 'Trust Permanently' to "
            "pair."
        );
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2(EmptyPos.x, EmptyPos.y + 70.0f));
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
    } else {
        const float TrustedCardWidth = (AvailableWidth - 16.0f) * 0.5f;
        const float TrustedCardHeight = 94.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 10.0f));

        size_t CardIdx = 0;
        ImVec2 RowStartPos = ImGui::GetCursorScreenPos();

        for (const auto& [DevID, Token] : *TokensMap) {
            size_t Col = CardIdx % 2;
            if (CardIdx > 0 && Col == 0) {
                RowStartPos = ImGui::GetCursorScreenPos();
            }

            float CardX = RowStartPos.x + Col * (TrustedCardWidth + 16.0f);
            float CardY = RowStartPos.y;

            const OmniInstance* Instance = nullptr;
            if (AvailableInstances && AvailableInstances->contains(DevID)) {
                Instance = &AvailableInstances->at(DevID);
            }

            TrustedCardAction Action = TrustedNodeCard(
                DevID,
                Token,
                Instance,
                CardIdx,
                ImVec2(CardX, CardY),
                TrustedCardWidth,
                TrustedCardHeight
            );

            if (Action == TrustedCardAction::Connect) {
                OmniAPI::Connect(ConnectionRequest{DevID});
            } else if (Action == TrustedCardAction::Forget) {
                OmniAPI::ForgetDevice(DevID);
                break;
            }

            if (Col == 1) {
                ImGui::SetCursorScreenPos(
                    ImVec2(RowStartPos.x, RowStartPos.y + TrustedCardHeight + 14.0f)
                );
                ImGui::Dummy(ImVec2(AvailableWidth, 0.0f));
            }
            CardIdx++;
        }

        if (CardIdx % 2 != 0) {
            ImGui::SetCursorScreenPos(
                ImVec2(RowStartPos.x, RowStartPos.y + TrustedCardHeight + 14.0f)
            );
            ImGui::Dummy(ImVec2(AvailableWidth, 0.0f));
        }

        ImGui::PopStyleVar(2);
    }
}

void OmniGUI::InstancesTab()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 20.0f));
    ImGui::BeginChild(
        "InstancesTabChild", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysUseWindowPadding
    );

    float SizeX = ImGui::GetContentRegionAvail().x;
    ImVec2 HeaderStartPos = ImGui::GetCursorScreenPos();

    InstancesHeader(HeaderStartPos, SizeX);

    if (InstancesSubTab == 0) {
        InstanceGroupsSection(SizeX);
        ActiveInstancesSection(SizeX);
    } else if (InstancesSubTab == 1) {
        TrustedNodesSection(SizeX);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}
