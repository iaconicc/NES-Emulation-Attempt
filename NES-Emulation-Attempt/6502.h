#pragma once
#include "bus.h"

void initialize_6502_cpu(Bus* bus);
void reset_6502_cpu();
void cpu_6502_clock();