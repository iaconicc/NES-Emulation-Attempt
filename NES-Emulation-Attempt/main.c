#include <windows.h>
#include <stdio.h>
#include "logger.h"
#include "nes.h"
#include "cartridge.h"
#include "app.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd){
	log_initialise();
	if (initialise_nes() == -1) {
		log_critical("Failed to initialise NES emulator.");
		return -1;
	}

	insert_cartridge("nestest.nes");
	reset_nes();
	while (is_running()){
		nes_clock();
	}

	remove_cartridge();
	deinitalise_nes();
	log_deinialise();
	return 0;
}