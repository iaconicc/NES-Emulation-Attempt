#include "app.h"
#include "nes.h"
#include "ppu.h"
#include "cartridge.h"
#include "window.h"
#include "logger.h"

volatile bool running = true;

void set_quit(){
	running = false;
}

bool initialise_app(char* file)
{
	running = true;
	log_initialise();
	if (initialise_nes() == -1) {
		log_critical("Failed to initialise NES emulator.");
		return false;
	}

	if(insert_cartridge(file) == -1) return false;
	reset_nes();

	//create windows
	if (!create_windows()) return false;

	return true;
}

void deinitialise_app()
{
	remove_cartridge();
	deinitalise_nes();
	log_deinialise();
}

static void run_frame()
{
	if (is_emulator_running())
	{
		while (!is_frame_complete())
		{
			nes_clock();
		}
	}
}

int run()
{
	run_frame();
	int ecode = 0;
	if ((ecode = updateWindows()) != 0) {
		running = false;
	}
}

bool is_running(){
	return running;
}
