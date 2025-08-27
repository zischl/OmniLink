#ifndef OMNIGUI_H
#define OMNIGUI_H

#pragma once
#include <Windows.h>
#include <wrl/client.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

class OmniGUI {
public:
	void SetupImGui(HWND hwnd, ID3D11Device* D3D11Device, ID3D11DeviceContext* D3D11Context, HANDLE* Events);
	
	inline void FrameBegin() {
		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowSize(ImVec2(720 , 720));
		//ImGui::SetNextWindowPos(ImVec2(0, 0));

		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 15.0f;

		if (ImGui::Begin("OmniLink", &ImGuiState, 0)) {
			DrawList = ImGui::GetWindowDrawList();

			ImGui::BeginChild("SideMenu", ImVec2(200, 0), true);

			VerticalMenuItem("OmniLinks");
			VerticalMenuItem("Settings");




			ImGui::EndChild();

			ImGui::SameLine();

			ImGui::BeginChild("FeaturePanel", ImVec2(0, 150), true);

			if (IconizedButton("Screen Link")) {
				SetEvent(EventHandler[2]);
			}
			ImGui::SameLine();

			if (IconizedButton("Window Link")) {
				SetEvent(EventHandler[1]);
			}
			ImGui::SameLine();

			if (IconizedButton("Input Link")) {
				//SetEvent(EventHandler[0]);
			}
			ImGui::SameLine();

			if (IconizedButton("Audio Link")) {
				//SetEvent(EventHandler[0]);
			}


			ImGui::EndChild();

			

			

		}

		ImGui::End();

	}

	inline void Render() {
		// Rendering
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		
	}



private:
	HANDLE* EventHandler = nullptr;

	bool ImGuiState = true;

	int user_resx = GetSystemMetrics(SM_CXSCREEN);
	int user_resy = GetSystemMetrics(SM_CYSCREEN);

	ImDrawList* DrawList = nullptr;

	bool IconizedButton(const char* label);
	bool OmniGUI::VerticalMenuItem(const char* label);

};

#endif