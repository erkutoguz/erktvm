#include "erktvm.h"

static u16 memory[MEMSIZE];
u16 registers[R_COUNT];

u16 swap_e(u16 n) { return (n >> 8) | (n << 8); }

u8 read_image(const char* imagepath) {
  FILE* fp = fopen(imagepath, "rb");
  if (!fp) return 1;

  load_image_to_mem(fp);
  fclose(fp);
  return 0;
}

void load_image_to_mem(FILE* fp) {
  u16 start;
  if (fread(&start, sizeof(u16), 1, fp) != 1) exit(EXIT_FAILURE);
  start = swap_e(start);

  registers[R_PC] = start;

  size_t max_read = MEMSIZE - start;
  u16* p = memory + start;
  size_t read = fread(p, sizeof(u16), max_read, fp);

  while (read-- > 0) {
    *p = swap_e(*p);
    ++p;
  }
}

u16 memread(u16 addr) { return memory[addr]; }
void memwrite(u16 addr, u16 val) { memory[addr] = val; }

// 00101 = 5
// 11010
// 11011 = -5
// if n is NEG
// 1111 1111 1111 1111 << 5 = 1111 1111 1110 0000
// 1111 1111 1110 0000 | 11011
// 1111 1111 1111 1011 = 16 bit representation of -5
u16 sign_extend(u16 n, int bit_count) {
  if (((n >> (bit_count - 1)) & 1) == 1) {  // if n is NEG
    n |= (0xFFFF << bit_count);             // make two's comp
  }
  return n;
}

void update_flag(u16 r) {
  if (registers[r] == 0) {
    registers[R_COND] = FL_ZERO;
  } else if (registers[r] >> 15 == 1) {
    registers[R_COND] = FL_NEG;
  } else {
    registers[R_COND] = FL_POS;
  }
}

void add_op(u16 instruction) {
  u16 dr = (instruction >> 9) & 0x7;
  u16 sr1 = (instruction >> 6) & 0x7;
  u16 mode = (instruction >> 5) & 0x1;

  if (mode) {
    // instruction & 0x1F => send only 5 bits
    u16 immt = sign_extend((instruction & 0x1F), 5);
    registers[dr] = registers[sr1] + immt;
  } else {
    u16 sr2 = instruction & 0x7;
    registers[dr] = registers[sr1] + registers[sr2];
  }

  update_flag(dr);
}

void and_op(u16 instruction) {
  u16 dr = (instruction >> 9) & 0x7;
  u16 sr1 = (instruction >> 6) & 0x7;
  u16 mode = (instruction >> 5) & 0x1;

  if (mode) {
    u16 immt = sign_extend((instruction & 0x1F), 5);
    registers[dr] = registers[sr1] & immt;
  } else {
    u16 sr2 = instruction & 0x7;
    registers[dr] = registers[sr1] & registers[sr2];
  }

  update_flag(dr);
}

void not_op(u16 instruction) {
  u16 dr = (instruction >> 9) & 0x7;
  u16 sr = (instruction >> 6) & 0x7;

  registers[dr] = ~registers[sr];
  update_flag(dr);
}

void br_op(u16 instruction) {
  u16 brflag = (instruction >> 9) & 0x7;

  if (brflag & registers[R_COND]) {
    registers[R_PC] += sign_extend((instruction & 0x1FF), 9);
  }
}

void ld_op(u16 instruction) {
  u16 dr = (instruction >> 9) & 0x7;
  u16 offset = sign_extend((instruction & 0x1FF), 9);
  registers[dr] = memread(registers[R_PC] + offset);

  update_flag(dr);
}

void st_op(u16 instruction) {
  u16 sr = (instruction >> 9) & 0x7;
  u16 addr = registers[R_PC] + sign_extend((instruction & 0x1FF), 9);
  memwrite(addr, registers[sr]);
}

void jmp_op(u16 instruction) {
  u16 baser = (instruction >> 6) & 0x7;
  registers[R_PC] = registers[baser];
}

void jsr_op(u16 instruction) {
  registers[R_R7] = registers[R_PC];
  if ((instruction >> 11) & 1) {
    registers[R_PC] += sign_extend((instruction & 0x7FF), 11);
  } else {
    u16 r = (instruction >> 6) & 0x7;
    registers[R_PC] = registers[r];
  }
}
int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <imagepath>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if (read_image(argv[1]) != 0) {
    exit(EXIT_FAILURE);
  }

  int running = 1;
  u16 instruction;
  u16 op;

  while (running) {
    instruction = memread(registers[R_PC]++);
    op = instruction >> 12;

    printf("instruction= %d\n", instruction);
    printf("OP: %d\n", op);
    switch (op) {
      case OP_ADD:
        add_op(instruction);
        break;
      case OP_LD:
        ld_op(instruction);
        break;
      case OP_ST:
        st_op(instruction);
        break;
      case OP_JSR:
        break;
      case OP_AND:
        and_op(instruction);
        break;
      case OP_LDR:
        break;
      case OP_STR:
        break;
      case OP_RTI:
        break;
      case OP_NOT:
        not_op(instruction);
        break;
      case OP_LDI:
        break;
      case OP_STI:
        break;
      case OP_JMP:
        break;
      case OP_RES:
        break;
      case OP_LEA:
        break;
      case OP_TRAP:
      default:
        running = 0;
        break;
    }
  }

  return 0;
}
