#include "deviceRegistry.h"
#include "cartridge.h"
#include "ppu.h"
#include "ram.h"  

// Initialize the array with a constant size and assign the pointer later  
Bus_device** cpu_bus_devices[3];
Bus_device** ppu_bus_devices[2];

Bus_device** get_cpu_device_registry(int* count)  
{  
    cpu_bus_devices[0] = get_ram_device();
    cpu_bus_devices[1] = get_cartridge_device();
    cpu_bus_devices[2] = get_ppu_bus_device();

    if (count) {  
        *count = sizeof(cpu_bus_devices)/sizeof(Bus_device*);
    }  
    return cpu_bus_devices;  
}

Bus_device** get_ppu_device_registry(int* count)
{
    ppu_bus_devices[0] = get_ppu_cartridge_device();
    ppu_bus_devices[1] = get_nametables_device();

    if (count)
    {
        *count = sizeof(ppu_bus_devices)/sizeof(Bus_device*);
    }
    return ppu_bus_devices;
}
