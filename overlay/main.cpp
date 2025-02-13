#include "overlay/overlay.h"
#include <iostream>
#include <thread>

int main() {

	std::thread(&c_Overlay::Render, &overlay, Mode::Banding, L"Counter-Strike 2", L"SDL_app").detach(); // dont print before this it causes double prints

	while (true) {
		Sleep(5);
	}

	return 0;
}