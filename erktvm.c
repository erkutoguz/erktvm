#include "erktvm.h"

#include <stdlib.h>

static u16 memory[MEMSIZE];
u16 registers[R_COUNT];

struct termios original_tio;

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

u16 check_key() {
  fd_set readfds;
  FD_ZERO(&readfds);
  // select stdin to control
  FD_SET(STDIN_FILENO, &readfds);

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  // set the wait val for stdin
  // if key pressed return 1 else 0
  return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

u16 memread(u16 addr) {
  if (addr == MR_KBSR) {
    if (check_key()) {
      // key pressed
      // set MR_KBSR addr 15th bit to 1
      // which means key pressed and ready to read char
      memory[MR_KBSR] = (1 << 15);
      memory[MR_KBDR] = getc(stdin);
    } else {
      // if key not pressed set 0 to MR_KBSR address so that we know there are
      // not any key kb activity waiting
      memory[MR_KBSR] = 0;
    }
  }

  return memory[addr];
}
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

void ldi_op(u16 instruction) {
  u16 dr = (instruction >> 9) & 0x7;
  u16 p = registers[R_PC] + sign_extend((instruction & 0x1FF), 9);
  u16 addr = memread(p);

  registers[dr] = memread(addr);
  update_flag(dr);
}

void sti_op(u16 instruction) {
  u16 sr = (instruction >> 9) & 0x7;
  u16 p = registers[R_PC] + sign_extend((instruction & 0x1FF), 9);
  u16 addr = memread(p);

  memwrite(addr, registers[sr]);
}

void ldr_op(u16 instruction) {
  u16 dr = (instruction >> 9) & 0x7;
  u16 baser = (instruction >> 6) & 0x7;
  u16 addr = registers[baser] + sign_extend((instruction & 0x3F), 6);

  registers[dr] = memread(addr);

  update_flag(dr);
}

void str_op(u16 instruction) {
  u16 sr = (instruction >> 9) & 0x7;
  u16 baser = (instruction >> 6) & 0x7;
  u16 addr = registers[baser] + sign_extend((instruction & 0x3F), 6);

  memwrite(addr, registers[sr]);
}

void lea_op(u16 instruction) {
  u16 dr = (instruction >> 9) & 0x7;
  registers[dr] = registers[R_PC] + sign_extend((instruction & 0x1FF), 9);

  update_flag(dr);
}

void trap_op(u16 instruction, int* running) {
  registers[R_R7] = registers[R_PC];
  u16 trapv = instruction & 0xFF;

  switch (trapv) {
    case TR_GETC:
      registers[R_R0] = getc(stdin);
      update_flag(R_R0);
      break;
    case TR_OUT:
      putc((char)registers[R_R0], stdout);
      break;
    case TR_PUTS: {
      u16 saddr = registers[R_R0];
      char c;
      while ((c = (char)memread(saddr++))) {
        putc(c, stdout);
      }
      fflush(stdout);
    } break;
    case TR_IN: {
      putc('>', stdout);
      fflush(stdout);
      registers[R_R0] = getc(stdin);
      putc(registers[R_R0], stdout);
      fflush(stdout);
      update_flag(R_R0);
    } break;
    case TR_PUTSP: {
      u16 saddr = registers[R_R0];
      u16 duo;
      char c1, c2;
      while (1) {
        duo = memread(saddr++);
        c1 = duo & 0xFF;
        if (c1 == 0) break;
        putc(c1, stdout);

        c2 = (duo >> 8) & 0xFF;
        if (c2 == 0) break;
        putc(c2, stdout);
      }
      fflush(stdout);
    } break;
    case TR_HALT:
      puts("exiting erktvm...");
      fflush(stdout);
      *running = 0;
      break;
    default:
      break;
  }
}

void disable_input_buffering() {
  tcgetattr(STDIN_FILENO, &original_tio);
  struct termios new_tio = original_tio;
  // ~ICANON => disable wait for enter
  // ~ECHO => do not ecoh my input when I pressed
  new_tio.c_lflag &= ~ICANON & ~ECHO;
  tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering() {
  tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

void handle_signal(int signal) {
  if (signal != SIGINT) return;

  printf("\nexiting erktvm...\n");
  restore_input_buffering();
  exit(EXIT_FAILURE);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <imagepath>\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  if (read_image(argv[1]) != 0) {
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, handle_signal);
  disable_input_buffering();

  int running = 1;
  u16 instruction;
  u16 op;

  while (running) {
    instruction = memread(registers[R_PC]++);
    op = instruction >> 12;

    switch (op) {
      case OP_BR:
        br_op(instruction);
        break;
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
        jsr_op(instruction);
        break;
      case OP_AND:
        and_op(instruction);
        break;
      case OP_LDR:
        ldr_op(instruction);
        break;
      case OP_STR:
        str_op(instruction);
        break;
      case OP_RTI:
        break;
      case OP_NOT:
        not_op(instruction);
        break;
      case OP_LDI:
        ldi_op(instruction);
        break;
      case OP_STI:
        sti_op(instruction);
        break;
      case OP_JMP:
        jmp_op(instruction);
        break;
      case OP_RES:
        break;
      case OP_LEA:
        lea_op(instruction);
        break;
      case OP_TRAP:
        trap_op(instruction, &running);
        break;
      default:
        running = 0;
        break;
    }
  }

  restore_input_buffering();
  return 0;
}
