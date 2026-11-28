#ifndef ADDRESSING_H
#define ADDRESSING_H

#include <stdint.h>
#include <stdbool.h>

#ifndef ADDR_NONE
#define ADDR_NONE 0xFFFFu
#endif

typedef struct CPU6502 CPU6502;

uint16_t mode_ACC(CPU6502 *cpu, bool page_penalty);
uint16_t mode_IMP(CPU6502 *cpu, bool page_penalty);
uint16_t mode_IMM(CPU6502 *cpu, bool page_penalty);
uint16_t mode_ZP(CPU6502 *cpu, bool page_penalty);
uint16_t mode_ZPX(CPU6502 *cpu, bool page_penalty);
uint16_t mode_ZPY(CPU6502 *cpu, bool page_penalty);
uint16_t mode_ABS(CPU6502 *cpu, bool page_penalty);
uint16_t mode_ABX(CPU6502 *cpu, bool page_penalty);
uint16_t mode_ABY(CPU6502 *cpu, bool page_penalty);
uint16_t mode_ABX_W(CPU6502 *cpu, bool page_penalty);
uint16_t mode_ABY_W(CPU6502 *cpu, bool page_penalty);
uint16_t mode_REL(CPU6502 *cpu, bool page_penalty);
uint16_t mode_INX(CPU6502 *cpu, bool page_penalty);
uint16_t mode_INY(CPU6502 *cpu, bool page_penalty);
uint16_t mode_INY_W(CPU6502 *cpu, bool page_penalty);
uint16_t mode_IND(CPU6502 *cpu, bool page_penalty);

#endif /* ADDRESSING_H */