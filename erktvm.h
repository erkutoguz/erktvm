#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define MEMSIZE (1 << 16)

typedef uint16_t u16;
typedef uint8_t u8;

enum {
  R_R0 = 0,
  R_R1,
  R_R2,
  R_R3,
  R_R4,
  R_R5,
  R_R6,
  R_R7,
  R_PC,
  R_COND,
  R_COUNT
};

enum {
  OP_BR = 0,  // 0000
  OP_ADD,     // 0001
  OP_LD,      // 0010
  OP_ST,      // 0011
  OP_JSR,     // 0100
  OP_AND,     // 0101
  OP_LDR,     // 0110
  OP_STR,     // 0111
  OP_RTI,     // 1000
  OP_NOT,     // 1001
  OP_LDI,     // 1010
  OP_STI,     // 1011
  OP_JMP,     // 1100
  OP_RES,     // 1101
  OP_LEA,     // 1110
  OP_TRAP     // 1111
};

enum {
  FL_POS = 0,
  FL_ZERO,
  FL_NEG,
};

u16 swap_e(u16 n);

u8 read_image(const char* imagepath);

void load_image_to_mem(FILE* fp);

u16 memread(u16 addr);
void memwrite(u16 addr, u16 val);

u16 sign_extend(u16 n, int bit_count);

void update_flag(u16 r);

void add_op(u16 instruction);
void and_op(u16 instruction);
void not_op(u16 instruction);
