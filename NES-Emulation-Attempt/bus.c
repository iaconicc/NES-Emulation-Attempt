#include "bus.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define INITIAL_BUS_DEVICE_REGISTRY_CAPACITY 5

typedef struct {
	int count;
	int capacity;
	Bus_device* bus_device_array_List;
}Device_registry;

struct Bus {
	char* name;
	Device_registry registry;
	bool islocked;
};

Bus bus_6502 = {
	.name = "6502",
	.islocked = false,
};

Bus bus_ppu = {
	.name = "ppu",
	.islocked = false,
};

Bus* get_bus(uint8_t bus) {
	switch (bus)
	{
	case 1:
		return &bus_6502;
	case 2:
		return &bus_ppu;
	default:
		return NULL;
	}
}

int register_device_on_bus(Bus* bus, const Bus_device* device)
{
	if (bus->islocked){
		log_warn("Attempted to register device on locked bus");
		return -1;
	}

	//allocate if array list does not yet exist
	if (!bus->registry.bus_device_array_List){
		bus->registry.bus_device_array_List = malloc(sizeof(Bus_device)* INITIAL_BUS_DEVICE_REGISTRY_CAPACITY);
		if (!bus->registry.bus_device_array_List) { 
			log_critical("Failed to allocate memory for bus device registry");
			return -1;
		}
		bus->registry.capacity = INITIAL_BUS_DEVICE_REGISTRY_CAPACITY;
		bus->registry.count = 0;
	}

	//reallocate if count is equal to capcity
	if (bus->registry.count == bus->registry.capacity){
		void* tmp = realloc(bus->registry.bus_device_array_List, sizeof(Bus_device)*
			(bus->registry.capacity*2));
		if (!tmp) {
			log_critical("Failed to reallocate memory for bus device registry");
			return -1;
		}
		bus->registry.bus_device_array_List = tmp;
		bus->registry.capacity = bus->registry.capacity * 2;
	}

	//allocate device and make copy
	memcpy(&bus->registry.bus_device_array_List[bus->registry.count], device, sizeof(Bus_device));
	bus->registry.count++;

	return 0;
}

static int cmp_device(const void* a, const void* b)
{
	const Bus_device* da = (const Bus_device*)a;
	const Bus_device* db = (const Bus_device*)b;

	if (da->start_range < db->start_range) return -1;
	if (da->start_range > db->start_range) return 1;
	if (da->end_range < db->end_range) return -1;
	if (da->end_range > db->end_range) return 1;
	return 0;
}

int lock_device_registry(Bus* bus)
{
	if (!bus->registry.bus_device_array_List){
		log_warn("Attempted to lock empty bus device registry");
		return -1;
	}

	//shrink list if not all slots are used
	if (bus->registry.count != bus->registry.capacity) {
		void* tmp = realloc(bus->registry.bus_device_array_List, sizeof(Bus_device) * bus->registry.count);
		if (!tmp){
			log_critical("Failed to reallocate memory for bus device registry");
			return -1;
		}
		bus->registry.bus_device_array_List = tmp;
		bus->registry.capacity = bus->registry.count;
	}

	//sort the list
	qsort(bus->registry.bus_device_array_List,
		bus->registry.count,
		sizeof(Bus_device),
		cmp_device
	);

	//check for overlapping regions
	for (int i = 0; i < bus->registry.count - 1; i++) {
		Bus_device* a = &bus->registry.bus_device_array_List[i];
		Bus_device* b = &bus->registry.bus_device_array_List[i + 1];
		if (a->end_range >= b->start_range) {
			log_critical("Overlapping bus device address ranges detected: 0x%04X-0x%04X and 0x%04X-0x%04X",
				a->start_range, a->end_range, b->start_range, b->end_range);
			return -1; 
		}
	}

	//log the devices and their regions
	for(int i = 0; i < bus->registry.count; i++) {
		Bus_device* device = &bus->registry.bus_device_array_List[i];
		log_info("Bus Device %s Registered: 0x%04X-0x%04X on %s bus", device->name, device->start_range, device->end_range, bus->name);
	}
		
	//lock the registry
	bus->islocked = true;

	return 0;
}

static int find_bus_device_by_address(const Bus* bus,const uint16_t addr)
{
	int lower = 0;
	int higher = bus->registry.count - 1;

	while (lower <= higher) {
		int middle = lower + (higher - lower) / 2;
		const Bus_device* device = &bus->registry.bus_device_array_List[middle];

		if (addr < device->start_range) {
			higher = middle - 1;
		}
		else if (addr > device->end_range) {
			lower = middle + 1;
		}
		else {
			return middle;
		}
	}

	return -1;
}

uint8_t read_bus_at_address(const Bus* bus, const uint16_t addr)
{
	if (!bus || !bus->islocked) {
		log_warn("attempted to read to locked or non-existant bus");
		return 0xFF;
	}
	int idx = find_bus_device_by_address(bus, addr);
	if (idx < 0) {
		log_warn("Attempted read at address 0x%04x on %s bus, but there is no device defaulted to 0xFF", addr, bus->name);
		return 0xFF;
	}

	const Bus_device* device = &bus->registry.bus_device_array_List[idx];
	if (!device->read) {
		log_warn("Attempted read at address 0x%04x on %s bus, but device has no response defaulted to 0xFF", addr, bus->name);
		return 0xFF;
	}

	return device->read(addr);

}

void write_bus_at_address(const Bus* bus, const uint16_t addr, const uint8_t data)
{
	if (!bus || !bus->islocked) 
	{
		log_warn("attempted to write to locked or non-existant bus");
		return;
	}

	int idx = find_bus_device_by_address(bus, addr);
	if (idx < 0) {
		log_warn("Attempted write at address 0x%04x on %s bus, but there is no device", addr, bus->name);
		return;
	}

	const Bus_device* device = &bus->registry.bus_device_array_List[idx];
	if (!device->write)
	{
		log_warn("Attempted write at address 0x%04x on %s bus, but device has no response", addr, bus->name);
		return;
	}

	device->write(addr, data);
}

void free_buses()
{
	bus_6502.islocked = false;
	bus_ppu.islocked = false;

	if (bus_6502.registry.bus_device_array_List)
	{
		free(bus_6502.registry.bus_device_array_List);
		bus_6502.registry.bus_device_array_List = NULL;
		bus_6502.registry.capacity = 0;
		bus_6502.registry.count = 0;
	}
	if (bus_ppu.registry.bus_device_array_List)
	{
		free(bus_ppu.registry.bus_device_array_List);
		bus_ppu.registry.bus_device_array_List = NULL;
		bus_ppu.registry.capacity = 0;
		bus_ppu.registry.count = 0;
	}

}

