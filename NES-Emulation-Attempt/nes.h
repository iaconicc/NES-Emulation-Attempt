#pragma once
#include <stdbool.h>

int initialise_nes();
void reset_nes();
void deinitalise_nes();
void set_emulator_running(bool run);
bool is_emulator_running();
void wait_till_cpu_cycle();
void nes_clock();