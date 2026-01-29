#include "nes.h"
#include "deviceRegistry.h"
#include "6502.h"
#include "ppu.h"
#include "logger.h"
#include <stdint.h>
#include <time.h>

uint64_t SystemCounter = 0;

int initialise_nes()
{
	//create a registry of bus devices for the cpu bus
	Bus* cpu_bus = get_bus(1);
	int count = 0;
	Bus_device** cpu_device_list = get_cpu_device_registry(&count);
	for (int i = 0; i < count; i++){
		if (register_device_on_bus(cpu_bus, cpu_device_list[i]) == -1) return -1;
		log_debug("Registered device %s on CPU bus", cpu_device_list[i]->name);
	}

	if(lock_device_registry(cpu_bus) == -1) return -1;
	initialize_6502_cpu(cpu_bus);

	//create registry of bus devices for the ppu bus
	Bus* ppu_bus = get_bus(2);
	count = 0;
	Bus_device** ppu_device_list = get_ppu_device_registry(&count);
	for (int i = 0; i < count; i++) {
		if (register_device_on_bus(ppu_bus, ppu_device_list[i]) == -1) return -1;
		log_debug("Registered device %s on PPU bus", ppu_device_list[i]->name);
	}

	if (lock_device_registry(ppu_bus) == -1) return -1;
	initialise_ppu(ppu_bus);

	return 0;
}

void reset_nes()
{
	reset_ppu();
	reset_6502_cpu();
	while (get_cycles() != 0) nes_clock();
	SystemCounter = 0;
}

void deinitalise_nes()
{
	free_buses();
}

bool emulator_running = false;
void set_emulator_running(bool run)
{
	emulator_running = run;
}

bool is_emulator_running()
{
	return emulator_running;
}

void wait_till_cpu_cycle()
{
	while (SystemCounter % 3 != 0)
	{
		nes_clock();
	}
};

void nes_clock(){

	ppu_clock();
	if (SystemCounter % 3 == 0)
	{
		int cycles = get_cycles();
		if ( cycles == 0)
		{
			cpu_6502_clock();
		}
		cycles--;
		set_cycles(cycles);
	}

	if (ppu_nmi())
	{
		nmi();
		nmi_acknolodged();
	}
	
	SystemCounter++;
}