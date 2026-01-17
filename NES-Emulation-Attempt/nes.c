#include "nes.h"
#include "deviceRegistry.h"
#include "logger.h"
#include <stdint.h>

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

	return 0;
}

void deinitalise_nes()
{
	free_buses();
}

void nes_clock(){

}