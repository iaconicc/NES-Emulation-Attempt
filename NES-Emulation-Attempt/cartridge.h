#pragma once
#include "bus.h"

int insert_cartridge(const char* file);
void remove_cartridge();
Bus_device* get_cartridge_device();