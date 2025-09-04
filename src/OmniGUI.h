#ifndef OMNIGUI_H
#define OMNIGUI_H

#pragma once

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
						SetEvent(EventHandler[2]);
					}
					ImGui::SameLine(0.0f, 0.0f);

					if (IconizedButton("Window Link", size)) {
						SetEvent(EventHandler[1]);
					}
					ImGui::SameLine(0.0f, 0.0f);

					if (IconizedButton("Input Link", size)) {
						//SetEvent(EventHandler[0]);
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
						SetEvent(EventHandler[3]);
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

	inline void DeviceIconPreview(ImVec2& pos, ImVec2& text_size = ImVec2(0, 0), const char* text = "") {
		DrawList->AddRect(ImVec2(pos.x - 50, pos.y - 40), ImVec2(pos.x + 50, pos.y + 40), IM_COL32(255, 255, 255, 255), 5.0f, 0, 2.0f);		//monitor
		DrawList->AddText(ImVec2(pos.x - (text_size.x * 0.5f), pos.y), IM_COL32(255, 255, 255, 255), text);
		DrawList->AddRect(ImVec2(pos.x, pos.y + 40), ImVec2(pos.x, pos.y + 55), IM_COL32(255, 255, 255, 255), 5.0f, 0, 2.0f);				//handle
		DrawList->AddRect(ImVec2(pos.x - 15, pos.y + 55), ImVec2(pos.x + 15, pos.y + 55), IM_COL32(255, 255, 255, 255), 10.0f, 0, 1.0f);	//stand
	}

	inline void DeviceIcon(const char* label, ImVec2& pos, ImVec2& text_size, OmniInstance* DeviceData) {
		ImGui::PushID(label);

		const ImGuiID id = ImGui::GetID(label);
		
		ImRect bb(ImVec2(pos.x - 50, pos.y - 40), ImVec2(pos.x + 50, pos.y + 55));
		
		ImGui::ItemAdd(bb, id, NULL, ImGuiItemFlags_None);
		
		bool hovered, held;
		bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);
		ImGui::RenderNavCursor(bb, id);

		DrawList->AddRect(ImVec2(pos.x - 50, pos.y - 40), ImVec2(pos.x + 50, pos.y + 40), IM_COL32(255, 255, 255, 255), 5.0f, 0, 2.0f);		//monitor
		DrawList->AddText(ImVec2(pos.x - (text_size.x * 0.5f), pos.y), IM_COL32(255, 255, 255, 255), DeviceData->IPv4_String);
		DrawList->AddRect(ImVec2(pos.x, pos.y + 40), ImVec2(pos.x, pos.y + 55), IM_COL32(255, 255, 255, 255), 5.0f, 0, 2.0f);				//handle
		DrawList->AddRect(ImVec2(pos.x - 15, pos.y + 55), ImVec2(pos.x + 15, pos.y + 55), IM_COL32(255, 255, 255, 255), 10.0f, 0, 1.0f);	//stand

		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("DeviceInfo", DeviceData->IPv4_String, sizeof(DeviceData->IPv4_String));
			DeviceIconPreview(ImGui::GetCursorScreenPos(), text_size, DeviceData->IPv4_String);
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DeviceInfo");
			if (payload != nullptr) {
				const char* data = static_cast<char*> (payload->Data);
				std::cout << data << "\n";
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::PopID();

	}




private:
	HANDLE* EventHandler = nullptr;
	OmniLink& App;
	std::array<OmniInstance, 5>* AvailableDevices = nullptr;

	bool ImGuiState = true;

	int user_resx = GetSystemMetrics(SM_CXSCREEN);
	int user_resy = GetSystemMetrics(SM_CYSCREEN);

	ImDrawList* DrawList = nullptr;

	int ActiveMenu = 0;

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