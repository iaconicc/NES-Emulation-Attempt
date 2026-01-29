#include "cartridge.h"
#include "logger.h"
#include <stdbool.h>
#include <stdio.h>
#include <malloc.h>

int mapperId = 0;

uint8_t prg_banks = 0;
uint8_t chr_banks = 0;

uint8_t* prg_rom = NULL;
uint8_t* chr_rom = NULL;

Nt_mirroring_mode nametable_mirroring = VERTICAL;

static bool mapper_0_cpu_map(uint16_t* addr);

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
			if (!mapper_0_cpu_map(&addr)) return 0xFF;
			return prg_rom[addr];
		default:
			log_critical("the mapper %d is unimplemented cartridge read is not possible defaulting to 0xFF", mapperId);
			return 0xFF;
		}
	}
	else{
		//ppu cartridge read
		switch (mapperId)
		{
		case 0:
			//no need to remap the address as it starts from 0x0000
			addr &= 0x1FFF;
			return chr_rom[addr];
		default:
			log_critical("the mapper %d is unimplemented cartridge read is not possible defaulting to 0xFF", mapperId);
		}
	}
}

/*
	All this function will do is convert the 6502 address or ppu address into an address from to access into local
	pgr and chr memory.
	mapper 0 has no bank switching capabilities whatsoever so as there is no need to keep an internal state.
	it will be kept within this file.
*/
static bool mapper_0_cpu_map(uint16_t* addr)
{
	if (*addr < 0x8000)
	{ 
		return false; 
	}

	uint16_t mapped = *addr - 0x8000;
	if (prg_banks == 1)
	{
		mapped &= 0x3FFF;
	}else
	{ 
		mapped &= 0x7FFF;
	}

	*addr = mapped;
	return true;
}

Bus_device cartridge_device =
{
	.name = "Cartrigde",
	.start_range = 0x4020,
	.end_range = 0xFFFF,
	.read = read,
	.write = NULL,
};

Bus_device ppu_cartridge_device =
{
	.name = "Cartrigde",
	.start_range = 0x0000,
	.end_range = 0x1FFF,
	.read = read,
	.write = NULL,
};

int insert_cartridge(const char* file)
{
	FILE* nes_file = fopen(file, "rb");
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

	mapperId = header.flag7&0xF0 | (header.flag6 & 0xFF)>>4;
	nametable_mirroring = (header.flag6 & 0x01 ? VERTICAL : HORISONTAL);
	prg_rom = malloc(header.prgBanks*16384);
	if (!prg_rom) return -1;

	size_t chr_size = (header.chrBanks == 0) ? 8192 : (header.chrBanks * 8192);
	chr_rom = malloc(chr_size);

	if (!chr_rom) return -1;

	prg_banks = header.prgBanks;
	chr_banks = header.chrBanks;

	if ((header.flag6 & 0x04) != 0){
		fseek(nes_file, 512, SEEK_CUR); //skipping training data
	}

	fread(prg_rom, 16384, header.prgBanks, nes_file);

	if (header.chrBanks != 0)
	{
		fread(chr_rom, 8192, header.chrBanks, nes_file);
	}

	log_info("Loaded %s into memory with %u prg banks and %u chr banks and has the mapper id %u", file, prg_banks, chr_banks, mapperId);
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

Nt_mirroring_mode current_mirroring_mode()
{
	return nametable_mirroring;
}

Bus_device* get_cartridge_device()
{
	return &cartridge_device;
}

Bus_device* get_ppu_cartridge_device()
{
	return &ppu_cartridge_device;
}
