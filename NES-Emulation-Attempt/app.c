#include "app.h"

volatile bool running = true;

void set_quit(){
	running = false;
}

bool is_running(){
	return running;
}
