#include "deviceRegistry.h"
#include "cartridge.h"
#include "ram.h"  

// Initialize the array with a constant size and assign the pointer later  
Bus_device** cpu_bus_devices[2];

Bus_device** get_cpu_device_registry(int* count)  
{  
    cpu_bus_devices[0] = get_ram_device();
    cpu_bus_devices[1] = get_cartridge_device();

    if (count) {  
        *count = sizeof(cpu_bus_devices)/sizeof(Bus_device*);
    }  
    return cpu_bus_devices;  
}
