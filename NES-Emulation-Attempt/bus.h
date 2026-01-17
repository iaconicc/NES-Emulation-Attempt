#pragma once
#include <stdint.h>

typedef struct Bus_device Bus_device;
typedef struct Bus Bus;

/*
 gets pointer to statically allocated bus, there are only two buses
 were bus 1 corresponds to the 6502 address space and 
 bus 2 to the ppu address space
 returns null if bus number given is invalid
*/
Bus* get_bus(const uint8_t bus);

/*
 copies struct into internal registry (caller can discard original). This is for when a read or write is performed on the bus,
 the bus will pass the it to the relevant device to respond.
 returns -1 om error (OOM) returns 1 if bus is locked 0 if succesfull
*/
int register_device_on_bus(Bus* bus, const Bus_device* device);

/*
 locks the device registry after bus has initialised (all devices were registered) 
 before performing the lock it does two things
 - will also perform a safety check to make sure no bus device regions overlap
 returns -1 when safety check fails or when bus has no devices allocated or OOM otherwise 0 when succesfull
 - Sorts the devices from lowest address range to highest address range so that the device could be found easier with an binary search
*/
int lock_device_registry(Bus* bus);

/*
 bus looks for relevant device with addr then forwards the read. 
 returns the data the device responds with,
 if the device has no response or there is no device corresponding to the range then 0xFF is returned.
 device is required to implement address decoding on its own.
 bus also has to be locked.
*/
uint8_t read_bus_at_address(const Bus* bus, const uint16_t addr);

/*
 bus looks for relevant device with addr then forwards the write
 device is supposed to respond by updating its internal state with the data provided.
 device is required to implement address decoding on its own.
 bus also has to be locked.
*/
void write_bus_at_address(const Bus* bus, const uint16_t addr, const uint8_t data);

/*
 frees any allocated memory on the buses can be done before and after a registry lock,
 it will unlock the registry after
*/
void free_buses();

typedef uint8_t(*bus_read_fn)(const uint16_t addr);

typedef void (*bus_write_fn)(const uint16_t addr, const uint8_t data);

struct Bus_device {
	char* name; //for logging purposes only, can be NULL
	uint16_t start_range; //range is inclusive
	uint16_t end_range; //range is inclusive

	bus_read_fn read; //if NULL then device does not respond will read back 0xFF, attempt may be logged
	bus_write_fn write; //if NULL then device does not respond, attempt may be logged
};