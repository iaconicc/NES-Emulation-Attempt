#include "6502.h"
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

void initialize_cpu(Bus* bus) {
	cpu_bus = bus;
}

// flag manipulation functions
static void setFlag(StatusFlags flag, uint8_t value) {
	if (value)
		status |= flag;
	else
		status &= ~flag;
}

static uint8_t getFlag(StatusFlags flag) {
	return (status & flag) > 0 ? 1 : 0;
}

static uint8_t read(uint16_t addr) {
	return read_bus_at_address(cpu_bus, addr);
}

static void write(uint16_t addr, uint8_t data) {
	write_bus_at_address(cpu_bus, addr, data);
}

typedef struct {
	char* name;
	uint8_t(*operate)();
	uint8_t(*addressMode)();
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

Instruction lookup[16][16] = {
	{"BRK",BRK,IMM,7},{"ORA",IZX,IZX,6},{"???",XXX,IMP,2},{"???",XXX,IMP,2},{ "???",XXX,IMP,8},{"ORA",ORA,ZP0,3}
};

void cpu_6502_clock(){
	
	if (cycles == 0) {
		opcode = read(pc);
		pc++;

		cycles = lookup[(opcode >> 4) & 0xF][opcode & 0xF].cycles;

		uint8_t additional_cycle1 = lookup[(opcode>>4)&0xF][opcode&0xF].addressMode();
		uint8_t additional_cycle2 = lookup[(opcode >> 4) & 0xF][opcode & 0xF].operate();

		cycles += (additional_cycle1 & additional_cycle2);
	}

	clock_count++;
	cycles--;
}

static uint8_t fetch()
{
	if (!(lookup[(opcode >> 4) & 0xF][opcode & 0xF].addressMode == IMP))
		fetched = read(addr_abs);
	return fetched;
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

	uint16_t lower = read(((uint16_t)t + (uint16_t)x & 0xff));
	uint16_t upper = read(((uint16_t)t + (uint16_t)x + 1 & 0xff));

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

	if (addr_abs & 0xFF00 != (upper << 8)) return 1;

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

	if (addr_abs & 0xFF00 != (high << 8)) return 1;

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

	if (addr_abs & 0xFF00 != (high << 8)) return 1;

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
	uint16_t temp = (uint16_t)a + (uint16_t)fetched + (uint16_t)getFlag(CARRY);
	setFlag(CARRY, temp > 0x00FF);
	setFlag(ZERO, temp&0x00FF == 0);
	setFlag(NEGATIVE,  temp&0x80);
	setFlag(OVERFLOW, ((((uint8_t) temp & 0x00FF)^a)&(((uint8_t)temp & 0x00FF)^fetched))&0x80);

	a = temp & 0x00FF;

	return 1;
}

static uint8_t AND() {
	fetch();
	a = a&fetched;
	setFlag(ZERO, a == 0);
	setFlag(NEGATIVE, a&0x80);
	return 1;
}

static uint8_t ASL()
{
	fetch();
	temp = (uint16_t) fetched << 1;
	setFlag(ZERO, fetched == 0);
	setFlag(NEGATIVE, 0x80);
	setFlag(CARRY, (fetched&0xFF00)>0);
	if (lookup[opcode>>4&0xF][opcode&0xF].addressMode == IMP) {
		a = temp & 0xFF;
	}
	else{
		write(addr_abs, temp&0xFF);
	}
	return 0;
}

static uint8_t BCC()
{

 if(getFlag(CARRY)==0)
 {
  cycles++;
  abs_addr = pc + rel_addr;
  
  if((abs_addr&0xFF00)!=(pc&0xFF00)) cycles++;
  
  pc = abs_addr;
  return 0;
 }

 return 0;
}

static uint8_t ORA()
{
	fetch();
	a |= fetched;
	setFlag(ZERO, a == 0x00);
	setFlag(NEGATIVE, a&0x80);
	return 1;
}

static uint8_t BRK() {
	pc++;
	setFlag(INTERRUPT_DISABLE, 1);
	write(0x0100 + sp, (pc >> 8) & 0x00FF);
	sp--;
	write(0x0100 + sp, pc & 0x00FF);
	sp--;

	setFlag(BREAK, 1);
	write(0x0100 + sp, status);
	sp--;
	setFlag(BREAK, 0);

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