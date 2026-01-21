#include "app.h"
#include "nes.h"
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

}

int run()
{
	int ecode = 0;
	if ((ecode = updateWindows()) != 0) {
		running = false;
		return ;
	}
	run_frame();
}

bool is_running(){
	return running;
}
