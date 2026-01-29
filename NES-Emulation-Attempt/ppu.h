#pragma once
#include <stdbool.h>
#include "bus.h"

void initialise_ppu(Bus* bus);
void reset_ppu();
void ppu_clock();
bool ppu_nmi();
void reset_frame_complete();
void nmi_acknolodged();
bool is_frame_complete();

Bus_device* get_ppu_bus_device();
Bus_device* get_nametables_device();