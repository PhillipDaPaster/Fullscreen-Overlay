#include "gui.h"

void tooltip(const char* text) { // move this to another file
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip(); {
			ImGui::Text(text);
		}ImGui::EndTooltip();
	}
}

void c_Gui::RenderMenu() {

	ImGui::SetNextWindowSize(WindowSize, ImGuiCond_Once);
	ImGui::Begin("example", nullptr, ImGuiWindowFlags_NoNav); {
		ImGui::Checkbox("streamproof", &globals::streamproof);
		tooltip("Doesn't work on Nvidia capture");

		ImGui::Checkbox("vsync", &globals::vsync);
		tooltip("Vertical Sync ( Sets Overlay Refresh Rate to Monitor Refresh Rate");

	}ImGui::End();
}