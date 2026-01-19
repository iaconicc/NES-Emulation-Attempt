#include "cartridge.h"
#include "logger.h"
#include <stdio.h>
#include <malloc.h>

int mapperId = 0;
uint8_t* prg_rom = NULL;
uint8_t* chr_rom = NULL;

static void mapper_0_get_address(uint16_t* addr);

/*
	When a read is performed on the cartridge both the ppu and 6502 will be deffered to this function.
	Using the address we can differentiate which of the two has made the request as the memory ranges they use don't
	overlapp.
	The read is also responsible for mapping the program memory and character memory including switching between banks.
*/
static uint8_t read(uint16_t addr)
{
	if (addr >= 0x4020 && addr <= 0xFFFF){
		switch (mapperId)
		{
		case 0:
			mapper_0_get_address(&addr);
			if (!addr) return 0xFF;
			return prg_rom[addr];
		default:
			log_critical("the mapper %d is unimplemented cartridge read is not possible defaulting to 0xFF", mapperId);
			return 0xFF;
		}
	}
	else{
		//ppu: unimplemented
		return 0xFF;
	}
}

/*
	All this function will do is convert the 6502 address or ppu address into an address from to access into local
	pgr and chr memory.
	mapper 0 has no bank switching capabilities whatsoever so as there is no need to keep an internal state.
	it will be kept within this file.
*/
static void mapper_0_get_address(uint16_t* addr) {
	if (*addr >= 0x8000 && *addr <= 0xBFFF){
		*addr -= 0x8000;
	}else if (*addr >= 0xC000 && *addr <= 0xFFFF){
		*addr -= 0xC000;
	}
	else{
		log_warn("attempted to access program memory at 0x%04X but is not valid program memory", *addr);
		*addr = NULL;
	}
}

Bus_device cartridge_device =
{
	.name = "Cartrigde",
	.start_range = 0x4020,
	.end_range = 0xFFFF,
	.read = read,
	.write = NULL,
};

int insert_cartridge(const char* file)
{
	FILE* nes_file = fopen(file, "rb+");
	if (!nes_file) { 
		log_warn("could not open %s", file);
		return -1;
	}
	
	struct Header
	{
		char head[4];
		uint8_t prgBanks;
		uint8_t chrBanks;
		uint8_t flag6;
		uint8_t flag7;
		uint8_t flag8;
		uint8_t flag9;
		uint8_t flag10;
		uint8_t padding[5];
	}header;

	fread(&header, sizeof(header), 1, nes_file);

	mapperId = (header.flag7&0xF0)<<4 | (header.flag6 & 0xF0);
	prg_rom = malloc(header.prgBanks*16384);
	if (!prg_rom) return -1;

	chr_rom = malloc(header.chrBanks * 8192);
	if (!chr_rom) return -1;

	if ((header.flag6 & 0x2) != 0){
		fseek(nes_file, 512, SEEK_CUR); //skipping training data
	}

	fread(prg_rom, 16384, header.prgBanks, nes_file);
	fread(chr_rom, 8192, header.chrBanks, nes_file);

	fclose(nes_file);
	return 0;
}

void remove_cartridge()
{
	if (prg_rom)
	{
		free(prg_rom);
		prg_rom = NULL;
	}
	if (chr_rom)
	{
		free(chr_rom);
		chr_rom = NULL;
	}
}

Bus_device* get_cartridge_device()
{
	return &cartridge_device;
}
