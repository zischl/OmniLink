#ifndef OMNIGUI_H
#define OMNIGUI_H

#pragma once

#include "OmniAPI.h"
#include "OmniTypes.h"

#include <Windows.h>
#include <wrl/client.h>

#include <array>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"

class OmniLink;

class OmniGUI {
public:
	OmniGUI(OmniLink& OmniLinkInstance);

	void SetupImGui(HWND hwnd, ID3D11Device* D3D11Device, ID3D11DeviceContext* D3D11Context, HANDLE* Events);

	inline void FrameBegin() {
		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(1280, 810));
		ImGui::SetNextWindowPos(ImVec2(0, 0));

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 15.0f;

		if (ImGui::Begin("OmniLink", &ImGuiState, ImGuiWindowFlags_NoTitleBar)) {


			ImGui::BeginChild("SideMenu", ImVec2(200, 0), true);
			{

				DrawList = ImGui::GetWindowDrawList();

				if (VerticalMenuItem("OmniLinks")) ActiveMenu = 0;
				if (VerticalMenuItem("ActiveLinks")) ActiveMenu = 1;
				if (VerticalMenuItem("Keybinds")) ActiveMenu = 2;
				if (VerticalMenuItem("Settings")) ActiveMenu = 3;


			}ImGui::EndChild();


			ImGui::SameLine();


			ImGui::BeginChild("menu-item");
			{

				switch (ActiveMenu) {

				case 0:

				{
					DrawList = ImGui::GetWindowDrawList();

					ImGui::BeginChild("FeaturePanel", ImVec2(0, 150), true);

					ImVec2 size = ImVec2(165, 135);

					if (IconizedButton("Screen Link", size)) {
						OmniAPI::ToggleFeature(FeatureTypes::ScreenLink, DeviceMap::C0);
					}
					ImGui::SameLine(0.0f, 0.0f);

					if (IconizedButton("Window Link", size)) {
						OmniAPI::ToggleFeature(FeatureTypes::WindowLink, DeviceMap::C0);
					}
					ImGui::SameLine(0.0f, 0.0f);

					if (IconizedButton("Input Link", size)) {
						OmniAPI::ToggleFeature(FeatureTypes::InputLink, DeviceMap::C0);
					}
					ImGui::SameLine(0.0f, 0.0f);

					if (IconizedButton("Audio Link", size)) {
						//SetEvent(EventHandler[0]);
					}
					ImGui::SameLine(0.0f, 0.0f);

					if (IconizedButton("Clipboard Link", size)) {
						//SetEvent(EventHandler[0]);
					}

					ImGui::EndChild();

					ImGui::BeginChild("connections", ImVec2(0, 0), true);

					DrawList = ImGui::GetWindowDrawList();

					ConnectionRing("ConRing");



					if (ImGui::Button("Scan")) {
						OmniAPI::Scan(); 
					}



					/*CreateCurvedLine("ln4", 20); ImGui::SameLine(40.0f, -1.0f);

					CreateCurvedLine("ln3", 25); ImGui::SameLine(70.0f, -1.0f);

					CreateCurvedLine("ln2", 30); ImGui::SameLine(100.0f, -1.0f);

					CreateCurvedLine("ln1", 40); ImGui::SameLine(100.0f, -1.0f);*/





					/*CreateCurvedLine("ln1", 40); ImGui::SameLine(40.0f, -1.0f);

					CreateCurvedLine("ln2", 30); ImGui::SameLine(70.0f, -1.0f);

					CreateCurvedLine("ln3", 25); ImGui::SameLine(100.0f, -1.0f);

					CreateCurvedLine("ln4", 20);*/

					ImGui::EndChild();
				}

				break;

				case 1:

					break;

				case 2:
					break;

				case 3:
					break;

				}

			}ImGui::EndChild();


		}


		ImGui::End();

	}

	inline void Render() {
		// Rendering
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


	}

	inline void DeviceIconPreview(const ImVec2& pos, const ImU32& col , const ImVec2& text_size = ImVec2{ 0, 0 }, const char* text = "") {
		DrawList->AddRect(ImVec2(pos.x - 50, pos.y - 40), ImVec2(pos.x + 50, pos.y + 40), col, 5.0f, 0, 2.0f);		//monitor
		DrawList->AddText(ImVec2(pos.x - (text_size.x * 0.5f), pos.y), col, text);
		DrawList->AddRect(ImVec2(pos.x, pos.y + 40), ImVec2(pos.x, pos.y + 55), col, 5.0f, 0, 2.0f);				//handle
		DrawList->AddRect(ImVec2(pos.x - 15, pos.y + 55), ImVec2(pos.x + 15, pos.y + 55), col, 10.0f, 0, 1.0f);	//stand
	}

	inline void DeviceIcon(const char* label, const ImVec2& pos, const ImVec2& text_size, const OmniInstance* DeviceData) {
		ImGui::PushID(label);

		const ImGuiID id = ImGui::GetID(label);
		ImRect bb(ImVec2(pos.x - 50, pos.y - 40), ImVec2(pos.x + 50, pos.y + 55));
		ImGui::ItemAdd(bb, id, NULL, ImGuiItemFlags_None);
		
		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);
		ImGui::RenderNavCursor(bb, id);

		ImU32 col = hovered || held ? IM_COL32(128, 0, 255, 255) : IM_COL32(255, 255, 255, 255);


		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("DeviceInfo", &(DeviceData->DevMapIndex), sizeof(DeviceData->DevMapIndex));
			DeviceIconPreview(ImGui::GetCursorScreenPos(), col, text_size, DeviceData->IPv4_String);
			ImGui::EndDragDropSource();
		}
		else if (ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DeviceInfo");
			if (payload != nullptr) {
				const uint8_t data = *static_cast<uint8_t*> (payload->Data);
				OmniAPI::SwapDeviceLayout(data, DeviceData->DevMapIndex);
			}
			//test animation : device order swap (failed -_- )
			/*else { 
				ImVec2 cpos = ImGui::GetCursorScreenPos();
				pos.y += cpos.y < pos.x ? -(pos.y - cpos.y) : pos.y - cpos.y;
				pos.x += cpos.x < pos.x ? pos.x - cpos.x : -(pos.x - cpos.x);
				
			}*/
			ImGui::EndDragDropTarget();
		}


		DrawList->AddRect(ImVec2(pos.x - 50, pos.y - 40), ImVec2(pos.x + 50, pos.y + 40), col, 5.0f, 0, 2.0f);		//monitor
		DrawList->AddText(ImVec2(pos.x - (text_size.x * 0.5f), pos.y), col, DeviceData->IPv4_String);
		DrawList->AddRect(ImVec2(pos.x, pos.y + 40), ImVec2(pos.x, pos.y + 55), col, 5.0f, 0, 2.0f);				//handle
		DrawList->AddRect(ImVec2(pos.x - 15, pos.y + 55), ImVec2(pos.x + 15, pos.y + 55), col, 10.0f, 0, 1.0f);	//stand


		if (ImGui::BeginPopupContextItem("MyItemContextMenu")) {
			if (ImGui::MenuItem("Connect Instance")) {
				OmniAPI::Connect(static_cast<DeviceMap>(DeviceData->DevMapIndex));
			}
		
			ImGui::EndPopup();
		}

	


		ImGui::PopID();

	}




private:
	HANDLE* EventHandler = nullptr;
	OmniLink& App;
	std::unordered_map<DeviceMap, OmniInstance>* AvailableDevices = nullptr;

	bool ImGuiState = true;
	bool DeviceHoverState = false;
	ImVec2 SelectedDevicePos;

	int user_resx = GetSystemMetrics(SM_CXSCREEN);
	int user_resy = GetSystemMetrics(SM_CYSCREEN);

	ImDrawList* DrawList = nullptr;

	int ActiveMenu = 0;
	//bool PopUp1 = false;

	bool IconizedButton(const char* label, ImVec2& ButtonSize);
	bool VerticalMenuItem(const char* label);
	void ConnectionRing(const char* label);


	void CenterItemX(const float ItemWidth);
	void CreateCurvedLine(const char* label, int curve);


	//Fonts
	ImFont* JetBrainsReg20 = nullptr;
	ImFont* JetBrainsReg18 = nullptr;


};

#endif