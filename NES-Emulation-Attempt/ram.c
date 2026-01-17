#include "ram.h"

uint8_t ram[2048];

static uint8_t read(uint16_t addr)
{
	addr &= 0x07FF;
	return ram[addr];
}

static uint8_t write(uint16_t addr, uint8_t data){
	addr &= 0x07FF;
	ram[addr] = data;
}

Bus_device ram_device = {
	.name = "RAM",
	.start_range = 0x0000,
	.end_range = 0x1FFF,
	.read = read,
    .write = write,
};

Bus_device* get_ram_device()
{
	return &ram_device;
}
