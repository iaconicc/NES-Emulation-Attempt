#include "ppu.h"
#include "cartridge.h"
#include "Graphics.h"
#include "logger.h"
#include <stdint.h>

static int scanline = 0;
static int cycles = 0;

static bool frame_complete = false;
static bool nmi = false;

#define reverse_3byte_order(word) ((word&0xFF0000) >> 16) | (word&0x00FF00)  | ((word&0x0000FF) << 16)
#define C(colour) 0xFF000000 | reverse_3byte_order(colour) & 0xFFFFFF

static uint32_t colour_palette[64] = {
C(0x626262),C(0x001C95),C(0x1904AC),C(0x42009D),C(0x61006B),C(0x6E0025),C(0x650500),C(0x491E00),C(0x223700),C(0x004900),C(0x004F00),C(0x004816),C(0x00355E),C(0x000000),C(0x000000),C(0x000000),
C(0xABABAB),C(0x0C4EDB),C(0x3D2EFF),C(0x7115F3),C(0x9B0BB9),C(0xB01262),C(0xA92704),C(0x894600),C(0x576600),C(0x237F00),C(0x008900),C(0x008332),C(0x006D90),C(0x000000),C(0x000000),C(0x000000),
C(0xFFFFFF),C(0x57A5FF),C(0x8287FF),C(0xB46DFF),C(0xDF60FF),C(0xF863C6),C(0xF8746D),C(0xDE9020),C(0xB3AE00),C(0x81C800),C(0x56D522),C(0x3DD36F),C(0x3EC1C8),C(0x4E4E4E),C(0x000000),C(0x000000),
C(0xFFFFFF),C(0xBEE0FF),C(0xCDD4FF),C(0xE0CAFF),C(0xF1C4FF),C(0xFCC4EF),C(0xFDCACE),C(0xF5D4AF),C(0xE6DF9C),C(0xD3E99A),C(0xC2EFA8),C(0xB7EFC4),C(0xB6EAE5),C(0xB8B8B8),C(0x000000),C(0x000000),
};

union
{
	struct {
		uint8_t nametablex : 1;
		uint8_t nametabley : 1;
		uint8_t vram_increment : 1;
		uint8_t pattern_sprite : 1;
		uint8_t pattern_background : 1;
		uint8_t sprite_size : 1;
		uint8_t slave_mode : 1;
		uint8_t enable_nmi : 1;
	};
	uint8_t reg;
}ctrl;

union
{
	struct {
		uint8_t greyscale : 1;
		uint8_t show_background : 1;
		uint8_t show_sprite : 1;
		uint8_t background_rendering : 1;
		uint8_t sprite_rendering : 1;
		uint8_t emphasize_red : 1;
		uint8_t emphasize_green : 1;
		uint8_t emphasize_blue : 1;
	};
	uint8_t reg;
}mask;

union
{
	struct {
		uint8_t unused : 5;
		uint8_t sprite_overflow : 1;
		uint8_t sprite_0_hit : 1;
		uint8_t vblank : 1;
	};
	uint8_t reg;
}ppu_status;

typedef union {
	struct {
		uint16_t coarse_x : 5;
		uint16_t coarse_y : 5;
		uint16_t nametablex : 1;
		uint16_t nametabley : 1;
		uint16_t fineY : 3;
		uint16_t unused : 1;
	};
	uint16_t reg;
}Vram_reg;

Vram_reg tram;
Vram_reg vram;

uint8_t fine_x;
uint8_t write_latch;

uint8_t ppu_buffer;

Bus* ppu_bus;

typedef struct {
	uint8_t nametable[1024];
}Nametable;

Nametable nametables[4];

uint8_t palette_ram[32];

uint8_t next_tile_id = 0;
uint8_t next_tile_attribute = 0;

uint8_t next_tile_chr_lsb = 0;
uint8_t next_tile_chr_msb = 0;

//shift registers
uint16_t shifter_pattern_lo = 0x0000;
uint16_t shifter_pattern_hi = 0x0000;
uint16_t shifter_attrib_lo = 0x0000;
uint16_t shifter_attrib_hi = 0x0000;

void initialise_ppu(Bus* bus)
{
	ppu_bus = bus;
}

void reset_ppu()
{
	scanline = 0;
	cycles = 0;
	mask.reg = 0x00;
	ppu_status.reg = 0x00;
	ctrl.reg = 0x00;
	write_latch = 0x00;
	tram.reg = 0x0000;
	vram.reg = 0x0000;
	next_tile_id = 0;
	next_tile_attribute = 0;
	next_tile_chr_lsb = 0;
	next_tile_chr_msb = 0;
	shifter_pattern_lo = 0x0000;
	shifter_pattern_hi = 0x0000;
	shifter_attrib_lo = 0x0000;
	shifter_attrib_hi = 0x0000;

	for (int y = 0; y < 240; y++)
	{
		for (int x = 0; x < 256; x++)
		{
			set_pixel(x,y, 0xFFFFFFFF);
		}
	}
}

static void incrementScrollX()
{
	if (mask.background_rendering || mask.sprite_rendering)
	{
		if (vram.coarse_x == 31)
		{
			vram.coarse_x = 0;
			vram.nametablex = ~vram.nametablex;
		}
		else
		{
			vram.coarse_x++;
		}
	}
}

static void incrementScrollY()
{
	if (mask.background_rendering || mask.sprite_rendering)
	{
		if (vram.fineY < 7)
		{
			vram.fineY++;
		}
		else
		{
			vram.fineY = 0;

			if (vram.coarse_y == 29)
			{
				vram.coarse_y = 0;
				vram.nametabley = ~vram.nametabley;
			}
			else if (vram.coarse_y == 31)
			{
				vram.coarse_y = 0;
			}
			else
			{
				vram.coarse_y++;
			}
		}
	}
}

static void TransferAddressX()
{
	if (mask.background_rendering || mask.sprite_rendering)
	{
		vram.nametablex = tram.nametablex;
		vram.coarse_x = tram.coarse_x;
	}
}

static void TransferAddressY()
{

	if (mask.background_rendering || mask.sprite_rendering)
	{
		vram.fineY = tram.fineY;
		vram.nametabley = tram.nametabley;
		vram.coarse_y = tram.coarse_y;
	}
}

static void LoadBackgroundShifters()
{
	shifter_pattern_lo = (shifter_pattern_lo & 0xFF00) | next_tile_chr_lsb;
	shifter_pattern_hi = (shifter_pattern_hi & 0xFF00) | next_tile_chr_msb;

	shifter_attrib_lo = (shifter_attrib_lo & 0xFF00) | ((next_tile_attribute & 0b01) ? 0xFF : 0x00);
	shifter_attrib_hi = (shifter_attrib_hi & 0xFF00) | ((next_tile_attribute & 0b10) ? 0xFF : 0x00);
}

static void UpdateShifters()
{
	if (mask.background_rendering)
	{
		// Shifting background tile pattern row
		shifter_pattern_lo <<= 1;
		shifter_pattern_hi <<= 1;

		// Shifting palette attributes by 1
		shifter_attrib_lo <<= 1;
		shifter_attrib_hi <<= 1;
	}
}

static uint8_t ppu_read(uint16_t addr)
{
	return read_bus_at_address(ppu_bus, addr);
}

static void ppu_write(uint16_t addr, uint8_t data)
{
	write_bus_at_address(ppu_bus, addr, data);
}

void ppu_clock()
{
	//visible scanlines
	if (scanline >= -1 && scanline < 240)
	{
		//skip odd pixel pixel
		if (scanline == 0 && cycles == 0)
		{
			cycles = 1;
		}

		// clear vblank
		if (scanline == -1 && cycles == 1)
		{
			ppu_status.vblank = 0;
		}

		if ((cycles >= 2 && cycles < 258) || (cycles >= 321 && cycles < 338))
		{
			UpdateShifters();

			switch ((cycles - 1) % 8)
			{
			case 0:
			{
				LoadBackgroundShifters();

				//read next tile id
				next_tile_id = ppu_read(0x2000 | (vram.reg & 0x0FFF));
				break;
			}
			case 2:
			{
				//read attribute byte
				next_tile_attribute = ppu_read(0x23C0 | (vram.nametablex) << 10 | (vram.nametabley) << 11 |
					vram.coarse_x >> 2 | (vram.coarse_y >> 2) << 3);
				if (vram.coarse_y & 0x02) next_tile_attribute >>= 4;
				if (vram.coarse_x & 0x02) next_tile_attribute >>= 2;
				next_tile_attribute &= 0x03;
				break;
			}
			case 4:
			{
				next_tile_chr_lsb = ppu_read((ctrl.pattern_background << 12) 
					+((uint16_t)next_tile_id << 4) 
					+(vram.fineY) + 0);
				break;
			}
			case 6:
				next_tile_chr_lsb = ppu_read(ctrl.pattern_background << 12 +
					((uint16_t)next_tile_id << 4) +
					(vram.fineY) + 8);
				break;
			case 7:
				incrementScrollX();
				break;
			}

			if (cycles == 256)
			{
				incrementScrollY();
			}

			if (cycles == 257)
			{
				LoadBackgroundShifters();
				TransferAddressX();
			}

			if (cycles == 338 || cycles == 340)
			{
				next_tile_id = ppu_read(0x2000 | (vram.reg & 0x0FFF));
			}

			if (scanline == -1 && cycles >= 280 && cycles < 305)
			{
				TransferAddressY();
			}
		}
	}

	// post-rendering scanlines
	if (scanline >= 241 && scanline < 261)
	{
		if (scanline == 241 && cycles == 1)
		{
			ppu_status.vblank = 1;

			if (ctrl.enable_nmi) nmi = true;
		}
	}

	uint8_t bg_pixel = 0x00;
	uint8_t bg_palette = 0x00;
	uint8_t colour = 0x00;

	//composition
	if (mask.background_rendering)
	{
		uint16_t bit_mask = 0x8000 >> fine_x;

		uint8_t p0_pixel = (shifter_pattern_lo & bit_mask) > 0;
		uint8_t p1_pixel = (shifter_pattern_hi & bit_mask) > 0;

		bg_pixel = (p1_pixel << 1) | p0_pixel;

		uint8_t bg_pal0 = (shifter_attrib_lo & bit_mask) > 0;
		uint8_t bg_pal1 = (shifter_attrib_hi & bit_mask) > 0;

		bg_palette = (bg_pal1 << 1) | bg_pal0;

		colour = palette_ram[((bg_palette << 2) + bg_pixel)&0x3F];
	}

	set_pixel(cycles - 1, scanline, colour_palette[colour]);

	//increment the cycle
	cycles++;
	if (cycles >= 341)
	{
		cycles = 0;
		scanline++;
		if (scanline >= 261)
		{
			scanline = -1;
			frame_complete = true;
		}
	}
}

static uint8_t cpu_read_ppu(uint16_t addr)
{
	uint8_t data = 0x00;

	switch ((addr-0x2000)%8)
	{
	case 0: // Control
		break;
	case 1: // Mask
		break;
	case 2: // Status
	{
		data = ppu_status.reg;
		write_latch = 0;
		ppu_status.vblank = 0;
		break;
	}
	case 3: // OAM Address
		break;
	case 4: // OAM Data
		break;
	case 5: // Scroll
		break;
	case 6: // PPU Address
		break;
	case 7: // PPU Data
	{
		data = ppu_buffer;
		ppu_buffer = ppu_read(vram.reg);
		if (vram.reg >= 0x3F00) data = ppu_buffer;
		vram.reg += (ctrl.vram_increment?32:1);
		break;
	}
	}

	return data;
}

static void cpu_write_ppu(uint16_t addr, uint8_t data)
{
	switch ((addr - 0x2000) % 8)
	{
	case 0: // Control
	{
		ctrl.reg = data;
		tram.nametablex = ctrl.nametablex;
		tram.nametabley = ctrl.nametabley;
		break;
	}
	case 1: // Mask
		mask.reg = data;
		break;
	case 2: // Status
		break;
	case 3: // OAM Address
		break;
	case 4: // OAM Data
		break;
	case 5: // Scroll
	{
		if (write_latch == 0)
		{
			fine_x = data & 0x7;
			tram.coarse_x = data >> 3;
			write_latch = 1;
		}
		else
		{
			tram.fineY = data & 0x7;
			tram.coarse_y = data >> 3;
			write_latch = 0;
		}
		break;
	}
	case 6: // PPU Address
		if (write_latch == 0)
		{
			tram.reg = (data&0x3F) << 8| (tram.reg & 0xFF);
			write_latch = 1;
		}
		else
		{
			tram.reg = data | (tram.reg & 0xFF00);
			vram.reg = tram.reg;
			write_latch = 0;
		}
		break;
	case 7: // PPU Data
		ppu_write(vram.reg, data);
		vram.reg += (ctrl.vram_increment ? 32 : 1);
		break;
	}
}

Bus_device ppu_device = {
	.name = "PPU",
	.read = cpu_read_ppu,
	.write = cpu_write_ppu,
	.start_range = 0x2000,
	.end_range = 0x3FFF,
};


static uint8_t nametable_read(uint16_t addr)
{
	int nametable = (addr&0xC00)>>10;
	return nametables[nametable].nametable[addr&0x3FF];
}

static void nametable_write(uint16_t addr, uint8_t data)
{
	Nt_mirroring_mode mirror_mode = current_mirroring_mode();

	int nametable = (addr & 0xC00) >> 10;
	int offset = addr & 0x3FF;

	if (mirror_mode == VERTICAL)
	{
		if (nametable == 0 || nametable == 2)
		{
			nametables[0].nametable[offset] = data;
			nametables[2].nametable[offset] = data;
		}
		else{
			nametables[1].nametable[offset] = data;
			nametables[3].nametable[offset] = data;
		}
	}
	else if (mirror_mode == HORISONTAL)
	{
		if (nametable == 0 || nametable == 1)
		{
			nametables[0].nametable[offset] = data;
			nametables[1].nametable[offset] = data;
		}
		else {
			nametables[2].nametable[offset] = data;
			nametables[3].nametable[offset] = data;
		}
	}
}

Bus_device nametable_device = {
	.name = "NAMETABLE",
	.read = nametable_read,
	.write = nametable_write,
	.start_range = 0x2000,
	.end_range = 0x2FFF,
};

static uint8_t palette_read(uint16_t addr)
{
	return palette_ram[(addr - 0x3F00) & 0x20];
}

static void palette_write(uint16_t addr, uint8_t data)
{
	palette_ram[(addr - 0x3F00) & 0x3F] = data;
}

Bus_device palette_ram_device = {
	.name = "PALETTE_RAM",
	.read = palette_read,
	.write = palette_write,
	.start_range = 0x3F00,
	.end_range = 0x3FFF,
};

Bus_device* get_ppu_bus_device() { return &ppu_device; }
Bus_device* get_nametables_device() { return &nametable_device; }
Bus_device* get_palette_ram_device() { return &palette_ram_device; }
uint8_t* get_nametable_buffer(int nametable) { return &nametables[nametable]; }
bool is_frame_complete(){return frame_complete;	}
void reset_frame_complete() { frame_complete = false; }
bool ppu_nmi() { return nmi; }
void nmi_acknolodged() { nmi = false; }

Ppu_Regs ppu_get_regs()
{
	Ppu_Regs r;
	r.vram = vram.reg;
	r.tram = tram.reg;
	r.ctrl = ctrl.reg; r.mask = mask.reg; r.status = ppu_status.reg;
	return r;
}
