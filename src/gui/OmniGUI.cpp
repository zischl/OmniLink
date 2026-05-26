#include "OmniLink.h"
#include "OmniGUI.h"
#include "fonts.h"

using Microsoft::WRL::ComPtr;


OmniGUI::OmniGUI(OmniLink& OmniLinkInstance) : App(OmniLinkInstance) {
	AvailableDevices = App.GetAvailableInstances();

}

void OmniGUI::SetupImGui(HWND hwnd, ID3D11Device* D3D11Device, ID3D11DeviceContext* D3D11Context, HANDLE* Events)
{

	EventHandler = Events;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(D3D11Device, D3D11Context);

	ImFontConfig FontCFG;
	FontCFG.FontDataOwnedByAtlas = false;

	JetBrainsReg20 = io.Fonts->AddFontFromMemoryTTF(JetBrainsMonoRegular, JetBrainsMonoRegular_Size, 20.0f, &FontCFG);
	JetBrainsReg18 = io.Fonts->AddFontFromMemoryTTF(JetBrainsMonoRegular, JetBrainsMonoRegular_Size, 18.0f, &FontCFG);

}


bool OmniGUI::VerticalMenuItem(const char* label)
{
	ImGui::PushID(label);

	ImVec2 MenuItemSize = ImVec2(180, 100);

	ImVec2 pos = ImGui::GetCursorScreenPos();
	bool clicked = ImGui::InvisibleButton(label, MenuItemSize);


	if (!ImGui::IsItemHovered()) {
		ImU32 MenuItemColor = ImGui::GetColorU32(ImVec4(0.09f, 0.09f, 0.09f, 1.0f));

		DrawList->AddRectFilled(pos, ImVec2(pos.x + MenuItemSize.x, pos.y + MenuItemSize.y), MenuItemColor);
	}
	else {
		ImU32 MenuItemColor_Hovered = ImGui::GetColorU32(ImVec4(0.13f, 0.13f, 0.13f, 1.0f));

		DrawList->AddRectFilled(pos, ImVec2(pos.x + MenuItemSize.x, pos.y + MenuItemSize.y), MenuItemColor_Hovered);
	}

	ImGui::PopID();

	return clicked;

}




bool OmniGUI::IconizedButton(const char* label, ImVec2& ButtonSize)
{
	ImGui::PushID(label);




	ImVec2 pos = ImGui::GetCursorScreenPos();
	bool clicked = ImGui::InvisibleButton(label, ButtonSize);

	ImVec2 TextSize = ImGui::CalcTextSize(label);
	ImVec2 TextPos = ImVec2(pos.x + (ButtonSize.x - TextSize.x) * 0.5f, pos.y + (ButtonSize.y - TextSize.y) * 0.5f);


	if (!ImGui::IsItemHovered()) {
		ImU32 ButtonColor = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
		ImU32 TextColor = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

		DrawList->AddRectFilled(pos, ImVec2(pos.x + ButtonSize.x, pos.y + ButtonSize.y), ButtonColor);
		DrawList->AddText(TextPos, TextColor, label);
	}
	else {
		ImU32 ButtonColor_Hovered = ImGui::GetColorU32(ImVec4(0.2f, 0.0f, 0.2f, 1.0f));
		ImU32 TextColor_Hovered = ImGui::GetColorU32(ImVec4(0.2f, 0.0f, 0.2f, 1.0f));

		DrawList->AddRectFilled(pos, ImVec2(pos.x + ButtonSize.x, pos.y + ButtonSize.y), ButtonColor_Hovered);
		DrawList->AddText(TextPos, TextColor_Hovered, label);
	}

	ImGui::PopID();

	return clicked;



}



void OmniGUI::CenterItemX(const float ItemWidth)
{
	float space = ImGui::GetContentRegionAvail().x;
	ImGui::SetCursorPosX((space - ItemWidth) * 0.5f);
}





void OmniGUI::ConnectionRing(const char* label)
{
	ImVec2 space = ImGui::GetContentRegionAvail();
	ImVec2 cpos = ImGui::GetCursorScreenPos();

	ImGui::PushID(label);
	ImVec2 pos = ImVec2(cpos.x + (space.x * 0.5f), cpos.y + (space.y * 0.5f));

	ImU32 col = IM_COL32(128, 0, 255, 255);

	//DrawList->AddCircle(pos, 200, col, 20, 3.0f);

	//DrawList->AddCircle(pos, 210, col, 20, 3.0f);

	ImGui::PopID();

	int radius = 205;
	ImVec2 text_size = ImGui::CalcTextSize("192.168.1.255");

	ImGui::PushFont(JetBrainsReg18);

	DeviceIcon("C0", pos, text_size, &(*AvailableDevices)[DeviceMap::C0]);


	if ((*AvailableDevices)[DeviceMap::L1].InstanceIP) {
		DeviceIcon("L1", ImVec2(pos.x - radius, pos.y), text_size, &(*AvailableDevices)[DeviceMap::L1]);
	}

	if ((*AvailableDevices)[DeviceMap::LU1].InstanceIP) {
		DeviceIcon("LU1", ImVec2(pos.x - radius, pos.y - radius), text_size, &(*AvailableDevices)[DeviceMap::LU1]);
	}

	if ((*AvailableDevices)[DeviceMap::U1].InstanceIP) {
		DeviceIcon("U1", ImVec2(pos.x, pos.y - radius), text_size, &(*AvailableDevices)[DeviceMap::U1]);
	}

	if ((*AvailableDevices)[DeviceMap::RU1].InstanceIP) {
		DeviceIcon("RU1", ImVec2(pos.x + radius, pos.y - radius), text_size, &(*AvailableDevices)[DeviceMap::RU1]);
	}

	if ((*AvailableDevices)[DeviceMap::R1].InstanceIP) {
		DeviceIcon("R1", ImVec2(pos.x + radius, pos.y), text_size, &(*AvailableDevices)[DeviceMap::R1]);
	}

	if ((*AvailableDevices)[DeviceMap::RD1].InstanceIP) {
		DeviceIcon("RD1", ImVec2(pos.x + radius, pos.y + radius), text_size, &(*AvailableDevices)[DeviceMap::RD1]);
	}

	if ((*AvailableDevices)[DeviceMap::D1].InstanceIP) {
		DeviceIcon("D1", ImVec2(pos.x, pos.y + radius), text_size, &(*AvailableDevices)[DeviceMap::D1]);
	}

	if ((*AvailableDevices)[DeviceMap::LD1].InstanceIP) {
		DeviceIcon("LD1", ImVec2(pos.x - radius, pos.y + radius), text_size, &(*AvailableDevices)[DeviceMap::LD1]);
	}

	

	//if (DeviceHoverState) {
	//	ImGui::PushID("Connect");

	//	const ImGuiID id = ImGui::GetID("Connect");
	//	ImRect bb(ImVec2(SelectedDevicePos.x - 35, SelectedDevicePos.y - 15), ImVec2(SelectedDevicePos.x + 35, SelectedDevicePos.y + 15));
	//	ImGui::ItemAdd(bb, id, NULL, ImGuiItemFlags_None);

	//	bool hovered, held;
	//	bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);
	//	ImGui::RenderNavCursor(bb, id);
	//	
	//	//DrawList->AddRectFilled(ImVec2(SelectedDevicePos.x - 35, SelectedDevicePos.y - 15), ImVec2(SelectedDevicePos.x + 35, SelectedDevicePos.y + 15), col, 5.0f, 0);
	//	

	//	ImGui::PopID();
	//}


	ImGui::PopFont();




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

	ImU32 color = ImGui::GetColorU32(ImVec4(0.5f, 0.0f, 1.0f, 1.0f));
	float thickness = 1.0f;
	int segments = 20;

	DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness, segments);


	int glow_range = 1;

	color = ImGui::GetColorU32(ImVec4(0.5f, 0.0f, 1.0f, 0.6f));
	DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness + glow_range, segments);

	color = ImGui::GetColorU32(ImVec4(0.5f, 0.0f, 1.0f, 0.3f));
	DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness + glow_range * 2, segments);

	color = ImGui::GetColorU32(ImVec4(0.5f, 0.0f, 1.0f, 0.1f));
	DrawList->AddBezierCubic(p0, cp0, cp1, p1, color, thickness + glow_range * 2.5, segments);


	ImGui::PopID();
}