#include "overlay/overlay.h"
#include <iostream>
#include <thread>

int main() {
        std::thread([&]() { overlay.Render(Mode::Banding, L"Counter-Strike 2", L"SDL_app"); }).detach(); // dont print before this it causes double prints

        // temp to not close the cheat after threads complete
	while (true) {
		Sleep(5);
	}

	return 0;
}
