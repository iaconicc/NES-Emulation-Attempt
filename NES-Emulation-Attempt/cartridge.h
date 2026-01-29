#pragma once
#include "bus.h"

typedef enum {
	VERTICAL,
	HORISONTAL,
}Nt_mirroring_mode;

int insert_cartridge(const char* file);
void remove_cartridge();
Bus_device* get_cartridge_device();
Bus_device* get_ppu_cartridge_device();
Nt_mirroring_mode current_mirroring_mode();