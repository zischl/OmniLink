#include <OmniGUI.h>

using Microsoft::WRL::ComPtr;

struct D3DDevice {
		ComPtr<ID3D11Device> D3D11Device = nullptr;
		ComPtr<ID3D11DeviceContext> D3D11Context = nullptr;
	};

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
}

bool OmniGUI::IconizedButton(const char* label) {
	ImGui::PushID(label);

	ImVec2 ButtonSize = ImVec2(user_resx * 0.05, user_resx * 0.05);

	ImVec2 pos = ImGui::GetCursorScreenPos();
	bool clicked = ImGui::InvisibleButton(label, ButtonSize);
	
	ImVec2 TextSize = ImGui::CalcTextSize(label);
	ImVec2 TextPos = ImVec2(pos.x + (ButtonSize.x - TextSize.x) * 0.5f , pos.y + (ButtonSize.y - TextSize.y) * 0.5f);


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

bool OmniGUI::VerticalMenuItem(const char* label) {
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