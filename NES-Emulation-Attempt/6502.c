#include "6502.h"
#include "logger.h"
#include <stdio.h>
#include <stdint.h>

Bus* cpu_bus = NULL;

uint16_t pc = 0x0000; // Program Counter
uint8_t sp = 0x00;   // Stack Pointer
uint8_t a = 0x00;    // Accumulator
uint8_t x = 0x00;    // X Register
uint8_t y = 0x00;    // Y Register

uint8_t status = 0x00; // Status Register

// Assisstive variables to facilitate emulation
uint8_t  fetched = 0x00;   // Represents the working input value to the ALU
uint16_t temp = 0x0000; // A convenience variable used everywhere
uint16_t addr_abs = 0x0000; // All used memory addresses end up in here
uint16_t addr_rel = 0x00;   // Represents absolute address following a branch
uint8_t  opcode = 0x00;   // Is the instruction byte
uint8_t  cycles = 0;	   // Counts how many cycles the instruction has remaining
uint32_t clock_count = 0;	   // A global accumulation of the number of clocks

typedef enum {
	CARRY = (1 << 0),
	ZERO = (1 << 1),
	INTERRUPT_DISABLE = (1 << 2),
	DECIMAL_MODE = (1 << 3),
	BREAK = (1 << 4),
	UNUSED = (1 << 5),
	OVERFLOW = (1 << 6),
	NEGATIVE = (1 << 7)
} StatusFlags;

void initialize_6502_cpu(Bus* bus) {
	cpu_bus = bus;
}


// flag manipulation functions
static void set_flag(StatusFlags flag, uint8_t value) {
	if (value)
		status |= flag;
	else
		status &= ~flag;
}

static uint8_t get_flag(StatusFlags flag) {
	return (status & flag) > 0 ? 1 : 0;
}

static uint8_t read(uint16_t addr) {
	return read_bus_at_address(cpu_bus, addr);
}

static void write(uint16_t addr, uint8_t data) {
	write_bus_at_address(cpu_bus, addr, data);
}

void reset_6502_cpu()
{
	pc = (read(0xFFFD) << 8) | read(0xFFFC);
	set_flag(INTERRUPT_DISABLE, 1);

	a = 0;
	x = 0;
	y = 0;
	sp = 0xFD;

	addr_rel = 0x0000;
	addr_abs = 0x0000;
	fetched = 0x00;
	opcode = 0x00;
	temp = 0x00;

	cycles = 8;

	log_info("resetted to address 0x%04X", pc);
}

typedef struct {
	char* name;
	uint8_t(*operate)();
	uint8_t(*address_mode)();
	uint8_t cycles;
}Instruction;

static uint8_t fetch();

static uint8_t IMM(); static uint8_t IMP();
static uint8_t ZP0(); static uint8_t ZPY(); static uint8_t ZPX();
static uint8_t IND(); static uint8_t IZY(); static uint8_t IZX();
static uint8_t ABS(); static uint8_t ABY(); static uint8_t ABX();
static uint8_t REL();

static uint8_t ADC();	static uint8_t AND();	static uint8_t ASL();	static uint8_t BCC();
static uint8_t BCS();	static uint8_t BEQ();	static uint8_t BIT();	static uint8_t BMI();
static uint8_t BNE();	static uint8_t BPL();	static uint8_t BRK();	static uint8_t BVC();
static uint8_t BVS();	static uint8_t CLC();	static uint8_t CLD();	static uint8_t CLI();
static uint8_t CLV();	static uint8_t CMP();	static uint8_t CPX();	static uint8_t CPY();
static uint8_t DEC();	static uint8_t DEX();	static uint8_t DEY();	static uint8_t EOR();
static uint8_t INC();	static uint8_t INX();	static uint8_t INY();	static uint8_t JMP();
static uint8_t JSR();	static uint8_t LDA();	static uint8_t LDX();	static uint8_t LDY();
static uint8_t LSR();	static uint8_t NOP();	static uint8_t ORA();	static uint8_t PHA();
static uint8_t PHP();	static uint8_t PLA();	static uint8_t PLP();	static uint8_t ROL();
static uint8_t ROR();	static uint8_t RTI();	static uint8_t RTS();	static uint8_t SBC();
static uint8_t SEC();	static uint8_t SED();	static uint8_t SEI();	static uint8_t STA();
static uint8_t STX();	static uint8_t STY();	static uint8_t TAX();	static uint8_t TAY();
static uint8_t TSX();	static uint8_t TXA();	static uint8_t TXS();	static uint8_t TYA();
static uint8_t XXX();

Instruction lookup[16][16] = {
	{{"BRK",BRK,IMM,7},{"ORA",ORA,IZX,6},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"ORA",ORA,ZP0,3},{"ASL",ASL,ZP0,5},{"???",XXX,IMP,2},{"PHP",PHP,IMP,3},{"ORA",ORA,IMM,2},{"ASL",ASL,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"ORA",ORA,ABS,4},{"ASL",ASL,ABS,6},{"???",XXX,IMP,2}},
	{{"BPL",BPL,REL,2},{"ORA",ORA,IZY,5},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"ORA",ORA,ZPX,4},{"ASL",ASL,ZPX,6},{"???",XXX,IMP,2},{"CLC",CLC,IMP,2},{"ORA",ORA,ABY,4},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"ORA",ORA,ABX,4},{"ASL",ASL,ABX,7},{"???",XXX,IMP,2}},
	{{"JSR",JSR,ABS,6},{"AND",AND,IZX,6},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"BIT",BIT,ZP0,3},{"AND",AND,ZP0,3},{"ROL",ROL,ZP0,5},{"???",XXX,IMP,2},{"PLP",PLP,IMP,4},{"AND",AND,IMM,2},{"ROL",ROL,IMP,2},{"???",XXX,IMP,2},{"BIT",BIT,ABS,4},{"AND",AND,ABS,4},{"ROL",ROL,ABS,6},{"???",XXX,IMP,2}},
	{{"BMI",BMI,REL,2},{"AND",AND,IZY,5},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"AND",AND,ZPX,4},{"ROL",ROL,ZPX,6},{"???",XXX,IMP,2},{"SEC",SEC,IMP,2},{"AND",AND,ABY,4},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"AND",AND,ABX,4},{"ROL",ROL,ABX,7},{"???",XXX,IMP,2}},
	{{"RTI",RTI,IMP,6},{"EOR",EOR,IZX,6},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"EOR",EOR,ZP0,3},{"LSR",LSR,ZP0,5},{"???",XXX,IMP,2},{"PHA",PHA,IMP,3},{"EOR",EOR,IMM,2},{"LSR",LSR,IMP,2},{"???",XXX,IMP,2},{"JMP",JMP,ABS,3},{"EOR",EOR,ABS,4},{"LSR",LSR,ABS,6},{"???",XXX,IMP,2}},
	{{"BVC",BVC,REL,2},{"EOR",EOR,IZY,5},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"EOR",EOR,ZPX,4},{"LSR",LSR,ZPX,6},{"???",XXX,IMP,2},{"CLI",CLI,IMP,2},{"EOR",EOR,ABY,4},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"EOR",EOR,ABX,4},{"LSR",LSR,ABX,7},{"???",XXX,IMP,2}},
	{{"RTS",RTS,IMP,6},{"ADC",ADC,IZX,6},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"ADC",ADC,ZP0,3},{"ROR",ROR,ZP0,5},{"???",XXX,IMP,2},{"PLA",PLA,IMP,4},{"ADC",ADC,IMM,2},{"ROR",ROR,IMP,2},{"???",XXX,IMP,2},{"JMP",JMP,IND,5},{"ADC",ADC,ABS,4},{"ROR",ROR,ABS,6},{"???",XXX,IMP,2}},
	{{"BVS",BVS,REL,2},{"ADC",ADC,IZY,5},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"ADC",ADC,ZPX,4},{"ROR",ROR,ZPX,6},{"???",XXX,IMP,2},{"SEI",SEI,IMP,2},{"ADC",ADC,ABY,4},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"ADC",ADC,ABX,4},{"ROR",ROR,ABX,7},{"???",XXX,IMP,2}},
	{{"???",XXX,IMP,2},{"STA",STA,IZX,6},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"STY",STY,ZP0,3},{"STA",STA,ZP0,3},{"STX",STX,ZP0,3},{"???",XXX,IMP,2},{"DEY",DEY,IMP,2},{"???",XXX,IMP,2},{"TXA",TXA,IMP,2},{"???",XXX,IMP,2},{"STY",STY,ABS,4},{"STA",STA,ABS,4},{"STX",STX,ABS,4},{"???",XXX,IMP,2}},
	{{"BCC",BCC,REL,2},{"STA",STA,IZY,6},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"STY",STY,ZPX,4},{"STA",STA,ZPX,4},{"STX",STX,ZPY,4},{"???",XXX,IMP,2},{"TYA",TYA,IMP,2},{"STA",STA,ABY,5},{"TXS",TXS,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"STA",STA,ABX,5},{"???",XXX,IMP,2},{"???",XXX,IMP,2}},
	{{"LDY",LDY,IMM,2},{"LDA",LDA,IZX,6},{"LDX",LDX,IMM,2},{"???",XXX,IMP,2},{"LDY",LDY,ZP0,3},{"LDA",LDA,ZP0,3},{"LDX",LDX,ZP0,3},{"???",XXX,IMP,2},{"TAY",TAY,IMP,2},{"LDA",LDA,IMM,2},{"TAX",TAX,IMP,2},{"???",XXX,IMP,2},{"LDY",LDY,ABS,4},{"LDA",LDA,ABS,4},{"LDX",LDX,ABS,4},{"???",XXX,IMP,2}},
	{{"BCS",BCS,REL,2},{"LDA",LDA,IZY,5},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"LDY",LDY,ZPX,4},{"LDA",LDA,ZPX,4},{"LDX",LDX,ZPY,4},{"???",XXX,IMP,2},{"CLV",CLV,IMP,2},{"LDA",LDA,ABY,4},{"TSX",TSX,IMP,2},{"???",XXX,IMP,2},{"LDY",LDY,ABX,4},{"LDA",LDA,ABX,4},{"LDX",LDX,ABY,4},{"???",XXX,IMP,2}},
	{{"CPY",CPY,IMM,2},{"CMP",CMP,IZX,6},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"CPY",CPY,ZP0,3},{"CMP",CMP,ZP0,3},{"DEC",DEC,ZP0,5},{"???",XXX,IMP,2},{"INY",INY,IMP,2},{"CMP",CMP,IMM,2},{"DEX",DEX,IMP,2},{"???",XXX,IMP,2},{"CPY",CPY,ABS,4},{"CMP",CMP,ABS,4},{"DEC",DEC,ABS,6},{"???",XXX,IMP,2}},
	{{"BNE",BNE,REL,2},{"CMP",CMP,IZY,5},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"CMP",CMP,ZPX,4},{"DEC",DEC,ZPX,6},{"???",XXX,IMP,2},{"CLD",CLD,IMP,2},{"CMP",CMP,ABY,4},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"CMP",CMP,ABX,4},{"DEC",DEC,ABX,7},{"???",XXX,IMP,2}},
	{{"CPX",CPX,IMM,2},{"SBC",SBC,IZX,6},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"CPX",CPX,ZP0,3},{"SBC",SBC,ZP0,3},{"INC",INC,ZP0,5},{"???",XXX,IMP,2},{"INX",INX,IMP,2},{"SBC",SBC,IMM,2},{"NOP",NOP,IMP,2},{"???",XXX,IMP,2},{"CPX",CPX,ABS,4},{"SBC",SBC,ABS,4},{"INC",INC,ABS,6},{"???",XXX,IMP,2}},
	{{"BEQ",BEQ,REL,2},{"SBC",SBC,IZY,5},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"SBC",SBC,ZPX,4},{"INC",INC,ZPX,6},{"???",XXX,IMP,2},{"SED",SED,IMP,2},{"SBC",SBC,ABY,4},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{"SBC",SBC,ABX,4},{"INC",INC,ABX,7},{"???",XXX,IMP,2}},
};

void cpu_6502_clock(){
	
	opcode = read(pc);
	pc++;

	cycles = lookup[opcode & 0xF][opcode >> 4 & 0xF].cycles;

	uint8_t additional_cycle1 = lookup[opcode >> 4 & 0xF][opcode & 0xF].address_mode();
	uint8_t additional_cycle2 = lookup[opcode >> 4 & 0xF][opcode & 0xF].operate();

	cycles += (additional_cycle1 & additional_cycle2);

	clock_count++;
}

int get_cycles()
{
	return cycles;
}

void set_cycles(int cycle)
{
	cycles = cycle;
}

static uint8_t fetch()
{
	if (!(lookup[opcode >> 4 & 0xF][opcode & 0xF].address_mode == IMP))
		fetched = read(addr_abs);
	return fetched;
}

void nmi()
{
	write(0x0100 + sp, (pc >> 8) & 0x00FF);
	sp--;
	write(0x0100 + sp, pc & 0x00FF);
	sp--;

	set_flag(BREAK, 0);
	set_flag(UNUSED, 1);
	set_flag(INTERRUPT_DISABLE, 1);
	write(0x0100 + sp, status);
	sp--;

	addr_abs = 0xFFFA;
	uint16_t lo = read(addr_abs + 0);
	uint16_t hi = read(addr_abs + 1);
	pc = (hi << 8) | lo;

	cycles = 8;
}

static uint8_t IMM() {
	addr_abs = pc++;
	return 0;
}

static uint8_t IND()
{
	uint16_t ptr_lo = read(pc);
	pc++;
	uint16_t ptr_hi = read(pc);
	pc++;

	uint16_t ptr = (ptr_hi << 8) | ptr_lo;

	if (ptr_lo == 0x00FF) // Simulate page boundary hardware bug
	{
		addr_abs = (read(ptr & 0xFF00) << 8) | read(ptr + 0);
	}
	else // Behave normally
	{
		addr_abs = (read(ptr + 1) << 8) | read(ptr + 0);
	}

	return 0;
}

static uint8_t IZX() {
	uint8_t t = read(pc);
	pc++;

	uint16_t lower = read((((uint16_t)t + (uint16_t)x) & 0xff));
	uint16_t upper = read((((uint16_t)t + (uint16_t)x + 1) & 0xff));

	addr_abs = (upper << 8) | lower;

	return 0;
}

static uint8_t IZY() {
	uint8_t t = read(pc);
	pc++;

	uint16_t lower = read(((uint16_t)t & 0xff));
	uint16_t upper = read(((uint16_t)t + 1 & 0xff));

	addr_abs = (upper << 8) | lower;
	addr_abs += y;

	if ((addr_abs & 0xFF00) != (upper << 8)) return 1;

	return 0;
}

static uint8_t ZP0() {
	addr_abs = read(pc);
	pc++;
	addr_abs &= 0xFF;
	return 0;
}

static uint8_t ZPY() {
	addr_abs = (read(pc)+y);
	pc++;
	addr_abs &= 0xFF;
	return 0;
} 

static uint8_t ZPX() {
	addr_abs = (read(pc) + x);
	pc++;
	addr_abs &= 0xFF;
	return 0;
}

static uint8_t ABS()
{
	uint16_t low = read(pc);
	pc++;
	uint16_t high = read(pc);
	pc++;

	addr_abs = (high<<8) | low;
	return 0;
}

static uint8_t ABY()
{
	uint16_t low = read(pc);
	pc++;
	uint16_t high = read(pc);
	pc++;

	addr_abs = (high << 8) | low;
	addr_abs += y;

	if ((addr_abs & 0xFF00) != (high << 8)) return 1;

	return 0;
}

static uint8_t ABX()
{
	uint16_t low = read(pc);
	pc++;
	uint16_t high = read(pc);
	pc++;

	addr_abs = (high << 8) | low;
	addr_abs += x;

	if ((addr_abs & 0xFF00) != (high << 8)) return 1;

	return 0;
}

uint8_t REL()
{
	addr_rel = read(pc);
	pc++;
	if (addr_rel & 0x80)
		addr_rel |= 0xFF00;
	return 0;
}

static uint8_t IMP() {
	fetched = a;
	return 0;
}

static uint8_t ADC() {
	fetch();
	uint16_t temp = (uint16_t)a + (uint16_t)fetched + (uint16_t)get_flag(CARRY);
	set_flag(CARRY, temp > 0x00FF);
	set_flag(ZERO, (temp&0x00FF) == 0);
	set_flag(NEGATIVE,  (temp&0x80)>>7);
	set_flag(OVERFLOW, (((((uint8_t) temp & 0x00FF)^a)&(((uint8_t)temp & 0x00FF)^fetched))&0x80)>>7);

	a = temp & 0x00FF;

	return 1;
}

static uint8_t AND() {
	fetch();
	a = a&fetched;
	set_flag(ZERO, a == 0);
	set_flag(NEGATIVE, a&0x80);
	return 1;
}

static uint8_t ASL()
{
	fetch();
	temp = (uint16_t) fetched << 1;
	set_flag(ZERO, temp == 0);
	set_flag(NEGATIVE, (temp&0x80)>>7);
	set_flag(CARRY, (fetched&0x80)>>7);
	if (lookup[opcode >> 4 & 0xF][opcode & 0xF].address_mode == IMP) {
		a = temp & 0xFF;
	}
	else{
		write(addr_abs, temp&0xFF);
	}
	return 0;
}

static uint8_t BCC()
{

	if(get_flag(CARRY)==0)
	{
		cycles++;
		addr_abs = pc + addr_rel;
  
		if((addr_abs &0xFF00)!=(pc&0xFF00)) cycles++;
  
		pc = addr_abs;
		return 0;
	}

 return 0;
}

static uint8_t BCS()
{
	if(get_flag(CARRY)==1)
	{
		cycles++;
		addr_abs = pc + addr_rel;

		if((addr_abs &0xFF00)!=(pc&0xFF00)) cycles++;
 
		pc = addr_abs;

		return 0;
	}
	return 0;
}

static uint8_t BEQ()
{
	if(get_flag(ZERO)==1)
	{
		cycles++;
		addr_abs = pc + addr_rel;

		if((addr_abs &0xFF00)!=(pc&0xFF00)) cycles++;

		pc = addr_abs;

		return 0;
	}
	return 0;
}

static uint8_t BIT()
{
	fetch();
	temp = a & fetched;
	set_flag(ZERO, temp==0);
	set_flag(NEGATIVE, (temp&0x80)>>7);
	set_flag(OVERFLOW, (temp&0x40)>>6);
	return 0;
}

static uint8_t BMI()
{
	if (get_flag(NEGATIVE) == 1)
	{
		cycles++;
		addr_abs = pc + addr_rel;

		if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++;

		pc = addr_abs;

		return 0;
	}
	return 0;
}

static uint8_t BNE()
{
	if (get_flag(ZERO) == 0)
	{
		cycles++;
		addr_abs = pc + addr_rel;

		if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++;

		pc = addr_abs;

		return 0;
	}
	return 0;
}

static uint8_t BPL()
{
	if (get_flag(NEGATIVE) == 0)
	{
		cycles++;
		addr_abs = pc + addr_rel;

		if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++;

		pc = addr_abs;

		return 0;
	}
	return 0;
}

static uint8_t BVC()
{
	if (get_flag(OVERFLOW) == 0)
	{
		cycles++;
		addr_abs = pc + addr_rel;

		if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++;

		pc = addr_abs;

		return 0;
	}
	return 0;
}

static uint8_t BVS()
{
	if (get_flag(OVERFLOW) == 1)
	{
		cycles++;
		addr_abs = pc + addr_rel;

		if ((addr_abs & 0xFF00) != (pc & 0xFF00)) cycles++;

		pc = addr_abs;

		return 0;
	}
	return 0;
}

static uint8_t CLC()
{
	set_flag(CARRY, 0);
	return 0;
}

static uint8_t CLD()
{
	set_flag(DECIMAL_MODE, 0);
	return 0;
}

static uint8_t CLI()
{
	set_flag(INTERRUPT_DISABLE, 0);
	return 0;
}

static uint8_t CLV()
{
	set_flag(OVERFLOW, 0);
	return 0;
}

static uint8_t CMP()
{
	fetch();
	set_flag(CARRY, a >= fetched);
	set_flag(ZERO, a == fetched);
	set_flag(NEGATIVE, ((a-fetched)&0x80)>>7);
	return 1;
}

static uint8_t CPX()
{
	fetch();
	set_flag(CARRY, x >= fetched);
	set_flag(ZERO, x == fetched);
	set_flag(NEGATIVE, ((x - fetched) & 0x80) >> 7);

	return 0;
}

static uint8_t CPY()
{
	fetch();
	set_flag(CARRY, y >= fetched);
	set_flag(ZERO, y == fetched);
	set_flag(NEGATIVE, ((y - fetched) & 0x80) >> 7);

	return 0;
}

static uint8_t DEC()
{
	fetch();
	temp = fetched - 1;
	set_flag(ZERO, temp==0);
	set_flag(NEGATIVE, (temp&0x80)>>7);
	if (lookup[opcode >> 4 & 0xF][opcode & 0xF].address_mode == IMP) {
		a = temp & 0xFF;
	}
	else {
		write(addr_abs, temp & 0xFF);
	}
	return 0;
}

static uint8_t DEX()
{
	x--;
	set_flag(ZERO, x == 0);
	set_flag(NEGATIVE, (x & 0x80) >> 7);
	return 0;
}

static uint8_t DEY()
{
	y--;
	set_flag(ZERO, y == 0);
	set_flag(NEGATIVE, (y & 0x80) >> 7);
	return 0;
}

static uint8_t EOR()
{
	fetch();
	a = fetched ^ a;
	set_flag(ZERO, a == 0);
	set_flag(NEGATIVE, (a & 0x80) >> 7);
	return 1;
}

static uint8_t INC()
{
	fetch();
	temp = fetched + 1;
	set_flag(ZERO, temp == 0);
	set_flag(NEGATIVE, (temp & 0x80) >> 7);
	if (lookup[opcode >> 4 & 0xF][opcode & 0xF].address_mode == IMP) {
		a = temp & 0xFF;
	}
	else {
		write(addr_abs, temp & 0xFF);
	}
	return 0;
}

static uint8_t INX()
{
	x++;
	set_flag(ZERO, x == 0);
	set_flag(NEGATIVE, (x & 0x80) >> 7);
	return 0;
}

static uint8_t INY()
{
	y++;
	set_flag(ZERO, y == 0);
	set_flag(NEGATIVE, (y & 0x80) >> 7);
	return 0;
}

static uint8_t JMP()
{
	pc = addr_abs;
	return 0;
}

static uint8_t JSR()
{
	write(0x100+sp, (uint8_t) ((pc & 0xFF00) >> 8) & 0xFF);
	sp--;
	write(0x100 + sp, (uint8_t)pc & 0xFF);
	sp--;

	pc = addr_abs;
	return 0;
}

static uint8_t LDA()
{
	fetch();
	a = fetched;
	set_flag(ZERO, a==0);
	set_flag(NEGATIVE, (a&0x80)>>7);
	return 1;
}

static uint8_t LDX()
{
	fetch();
	x = fetched;
	set_flag(ZERO, x == 0);
	set_flag(NEGATIVE, (x & 0x80) >> 7);
	return 1;
}

static uint8_t LDY()
{
	fetch();
	y = fetched;
	set_flag(ZERO, y == 0);
	set_flag(NEGATIVE, (y & 0x80) >> 7);
	return 1;
}

static uint8_t LSR()
{
	fetch();
	set_flag(CARRY, fetched & 0x1);
	temp = fetched >> 1;
	set_flag(ZERO, temp == 0);
	set_flag(NEGATIVE, (temp & 0x80) >> 7);
	if (lookup[opcode >> 4 & 0xF][opcode & 0xF].address_mode == IMP) {
		a = temp & 0xFF;
	}
	else {
		write(addr_abs, temp & 0xFF);
	}
	return 0;
}

static uint8_t ORA()
{
	fetch();
	a |= fetched;
	set_flag(ZERO, a == 0x00);
	set_flag(NEGATIVE, a&0x80);
	return 1;
}

static uint8_t PHA()
{
	write(0x100+sp, a);
	sp--;
	return 0;
}

static uint8_t PHP()
{
	write(0x100 + sp, status|BREAK|UNUSED);
	set_flag(BREAK, 0);
	set_flag(UNUSED, 0);
	sp--;
	return 0;
}

static uint8_t PLA()
{
	sp++;
	a = read(0x100+sp);
	set_flag(ZERO, a == 0x00);
	set_flag(NEGATIVE, (a & 0x80)>>7);
	return 0;
}

static uint8_t PLP()
{
	sp++;
	status = read(0x100 + sp);
	set_flag(UNUSED, 1);
	return 0;
}

uint8_t static ROL()
{
	fetch();
	temp = (uint16_t)(fetched << 1) | get_flag(CARRY);
	set_flag(CARRY, (fetched&0x80)>>7);
	set_flag(ZERO,  temp == 0);
	set_flag(NEGATIVE, (temp & 0x80) >> 7);
	if (lookup[opcode >> 4 & 0xF][opcode & 0xF].address_mode == IMP)
		a = temp & 0x00FF;
	else
		write(addr_abs, temp & 0x00FF);
	return 0;
}

uint8_t static ROR()
{
	fetch();
	temp = (uint16_t)(get_flag(CARRY) << 7) | (fetched>>1);
	set_flag(CARRY, fetched & 0x1);
	set_flag(ZERO, temp == 0);
	set_flag(NEGATIVE, (temp & 0x80)>>7);
	if (lookup[opcode >> 4 & 0xF][opcode & 0xF].address_mode == IMP)
		a = temp & 0x00FF;
	else
		write(addr_abs, temp & 0x00FF);
	return 0;
}

uint8_t static RTI()
{
	sp++;
	status = read(0x100+sp);
	sp++;

	set_flag(BREAK, 0);
	set_flag(UNUSED, 0);


	uint8_t low = read(0x100 + sp);
	sp++;
	uint8_t high = read(0x100 + sp);
	pc = (high<<8) | low;
	return 0;
}

uint8_t static RTS()
{
	sp++;
	uint8_t low = read(0x100 + sp);
	sp++;
	uint8_t high = read(0x100 + sp);
	pc = (high << 8) | low;
	pc++;
	return 0;
}

uint8_t static SBC()
{
	fetch();

	uint16_t value = ((uint16_t)fetched) ^ 0x00FF;

	temp = (uint16_t)a + value + (uint16_t) get_flag(CARRY);
	set_flag(CARRY, temp & 0xFF00);
	set_flag(ZERO, (temp & 0xFF) == 0);
	set_flag(OVERFLOW, ((temp^a)&(temp^~fetched)&0x80)>>7);
	set_flag(NEGATIVE, (temp & 0x80)>>7);
	a = temp & 0x00FF;
	return 1;
}

uint8_t static SEC()
{
	set_flag(CARRY, 1);
	return 0;
}

uint8_t static SED()
{
	set_flag(DECIMAL_MODE, 1);
	return 0;
}

uint8_t static SEI()
{
	set_flag(INTERRUPT_DISABLE, 1);
	return 0;
}

uint8_t static STA()
{
	write(addr_abs,a);
	return 0;
}

uint8_t static STX()
{
	write(addr_abs, x);
	return 0;
}

uint8_t static STY()
{
	write(addr_abs, y);
	return 0;
}

uint8_t static TAX()
{
	x = a;
	set_flag(ZERO, x == 0);
	set_flag(NEGATIVE, (x&0x80)>>7);
	return 0;
}

uint8_t static TAY()
{
	y = a;
	set_flag(ZERO, y == 0);
	set_flag(NEGATIVE, (y & 0x80) >> 7);
	return 0;
}

uint8_t static TSX()
{
	x = sp;
	set_flag(ZERO, x == 0);
	set_flag(NEGATIVE, (x & 0x80) >> 7);
	return 0;
}

uint8_t static TXA()
{
	a = x;
	set_flag(ZERO, a == 0);
	set_flag(NEGATIVE, (a & 0x80) >> 7);
	return 0;
}

uint8_t static TXS()
{
	sp = x;
	set_flag(ZERO, sp == 0);
	set_flag(NEGATIVE, (sp & 0x80) >> 7);
	return 0;
}

uint8_t static TYA()
{
	a = y;
	set_flag(ZERO, a == 0);
	set_flag(NEGATIVE, (a & 0x80) >> 7);
	return 0;
}

static uint8_t BRK() {
	pc++;
	set_flag(INTERRUPT_DISABLE, 1);
	write(0x0100 + sp, (pc >> 8) & 0x00FF);
	sp--;
	write(0x0100 + sp, pc & 0x00FF);
	sp--;

	set_flag(BREAK, 1);
	write(0x0100 + sp, status);
	sp--;
	set_flag(BREAK, 0);

	pc = (uint16_t)read(0xFFFE) | ((uint16_t)read(0xFFFF) << 8);
	return 0;
}

static uint8_t XXX()
{
	return 0;
}

static uint8_t NOP()
{
	return 0;
}

static Debug_instructions g_dbg[24];

Debug_instructions* reset_debug_instructions()
{
	uint16_t cursor_pc = pc; // start from current CPU PC

	for (int i = 0; i < 24; i++)
	{
		Debug_instructions* di = &g_dbg[i];

		uint8_t op = read(cursor_pc);

		di->address = cursor_pc;
		di->bytes[0] = op;
		di->bytes[1] = 0;
		di->bytes[2] = 0;
		di->numberOfBytes = 1;

		Instruction inst = lookup[(op >> 4) & 0xF][op & 0xF];

		if (inst.address_mode == IMP)
		{
			swprintf(di->mneumonics, 128, L"%hs", inst.name);
			di->numberOfBytes = 1;
		}
		else if (inst.address_mode == IMM)
		{
			uint8_t imm = read((uint16_t)(cursor_pc + 1));
			di->bytes[1] = imm;
			di->numberOfBytes = 2;

			// Example formatting: "LDA #$10"
			swprintf(di->mneumonics, 128, L"%hs #$%02X", inst.name, imm);
		}
		else if (inst.address_mode == ZP0)
		{
			uint8_t zp = read((uint16_t)(cursor_pc + 1));
			di->bytes[1] = zp;
			di->numberOfBytes = 2;

			swprintf(di->mneumonics, 128, L"%hs $%02X", inst.name, zp);
		}
		else if (inst.address_mode == ABS)
		{
			uint8_t lo = read((uint16_t)(cursor_pc + 1));
			uint8_t hi = read((uint16_t)(cursor_pc + 2));
			uint16_t addr = (uint16_t)(lo | (hi << 8));

			di->bytes[1] = lo;
			di->bytes[2] = hi;
			di->numberOfBytes = 3;

			swprintf(di->mneumonics, 128, L"%hs $%04X", inst.name, addr);
		}else if (inst.address_mode == REL){
			int8_t rel = (int8_t) read((uint16_t)(cursor_pc + 1));

			di->bytes[1] = rel;
			di->numberOfBytes = 2;

			swprintf(di->mneumonics, 128, L"%hs %d", inst.name, rel);
		} else if (inst.address_mode == ABY) {
			uint8_t lo = read((uint16_t)(cursor_pc + 1));
			uint8_t hi = read((uint16_t)(cursor_pc + 2));
			uint16_t addr = (uint16_t)(lo | (hi << 8));

			di->bytes[1] = lo;
			di->bytes[2] = hi;
			di->numberOfBytes = 3;

			swprintf(di->mneumonics, 128, L"%hs $%04X,Y", inst.name, addr);
		}else if (inst.address_mode == ABX) {
			uint8_t lo = read((uint16_t)(cursor_pc + 1));
			uint8_t hi = read((uint16_t)(cursor_pc + 2));
			uint16_t addr = (uint16_t)(lo | (hi << 8));

			di->bytes[1] = lo;
			di->bytes[2] = hi;
			di->numberOfBytes = 3;

			swprintf(di->mneumonics, 128, L"%hs $%04X,X", inst.name, addr);
		}else if (inst.address_mode == ZPX) {
			uint8_t zp = read((uint16_t)(cursor_pc + 1));
			
			di->bytes[1] = zp;
			di->numberOfBytes = 2;

			swprintf(di->mneumonics, 128, L"%hs $%02X,X", inst.name, zp);
		}else if (inst.address_mode == ZPY) {
			uint8_t zp = read((uint16_t)(cursor_pc + 1));

			di->bytes[1] = zp;
			di->numberOfBytes = 2;

			swprintf(di->mneumonics, 128, L"%hs $%02X,Y", inst.name, zp);
		}else if (inst.address_mode == IZX) {
			uint8_t zp = read((uint16_t)(cursor_pc + 1));

			di->bytes[1] = zp;
			di->numberOfBytes = 2;

			swprintf(di->mneumonics, 128, L"%hs ($%02X,X)", inst.name, zp);
		}else if (inst.address_mode == IZY) {
			uint8_t zp = read((uint16_t)(cursor_pc + 1));

			di->bytes[1] = zp;
			di->numberOfBytes = 2;

			swprintf(di->mneumonics, 128, L"%hs ($%02X),Y", inst.name, zp);
		}else if (inst.address_mode == IND) {
			uint8_t lo = read((uint16_t)(cursor_pc + 1));
			uint8_t hi = read((uint16_t)(cursor_pc + 2));
			uint16_t addr = (uint16_t)(lo | (hi << 8));

			di->bytes[1] = lo;
			di->bytes[2] = hi;
			di->numberOfBytes = 3;

			swprintf(di->mneumonics, 128, L"%hs ($%04X)", inst.name, addr);
		}
		else
		{
			// Fallback: at least show mnemonic
			swprintf(di->mneumonics, 128, L"%hs", inst.name);
			di->numberOfBytes = 1;
		}

		cursor_pc = (uint16_t)(cursor_pc + di->numberOfBytes);
	}


	return g_dbg;
}

Debug_instructions* get_debug_instructions()
{
	return g_dbg;
}

Cpu6502_Regs cpu6502_get_regs(void)
{
	Cpu6502_Regs r;
	r.pc = pc;
	r.a = a; r.x = x; r.y = y; r.sp = sp; r.status = status;
	return r;
}
