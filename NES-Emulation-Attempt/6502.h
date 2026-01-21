#pragma once
#include "bus.h"

void initialize_6502_cpu(Bus* bus);
void reset_6502_cpu();
void cpu_6502_clock();
int get_cycles();

typedef struct 
{
	uint16_t address;
	uint8_t bytes[4];
	uint8_t numberOfBytes;
	wchar_t mneumonics[128];
}Debug_instructions;

//debug functions
Debug_instructions* reset_debug_instructions();
Debug_instructions* get_debug_instructions();

typedef struct {
	uint16_t pc;
	uint8_t  a, x, y, sp, status;
} Cpu6502_Regs;

Cpu6502_Regs cpu6502_get_regs(void);