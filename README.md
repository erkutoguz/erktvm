# ErktVM: A Bare-Metal LC-3 Virtual Machine

ErktVM is a 16-bit Virtual Machine (VM) based on the LC-3 (Little Computer 3) architecture. It provides a low-level environment to execute machine code by simulating a processor, memory, and peripheral interaction through C.

## Technical Specifications

* **Word Size**: 16-bit.
* **Address Space**: 65,536 addresses (128 KB of total addressable memory).
* **Registers**: 10 total (R_R0, R_R7, R_PC, R_COND).
* **Instruction Set**: 16 Opcodes (Arithmetic, Logic, Control Flow, and Memory Operations).

## Key Engineering Features

### 1. The Core Loop (Fetch-Decode-Execute)
The VM operates on a `while(running)` loop that fetches an instruction from memory, decodes the opcode using bitwise masking, and executes the corresponding hardware logic.

### 2. Non-Blocking I/O (MMIO)
Standard I/O functions block the CPU, killing real-time performance. ErktVM implements Memory-Mapped I/O using the Linux `select()` system call to poll the keyboard status (`0xFE00`) without halting the processor. This allows games like 2048 to run smoothly by checking for input only when a key is actually pressed.

### 3. Raw Terminal Discipline
To provide a true hardware experience, the VM hijacks the Linux terminal using `<termios.h>`. It disables Canonical Mode (line buffering) and Echo, ensuring that every keystroke is intercepted by the VM in its raw form.



### 4. Trap Routines
Includes specialized routines for I/O and system control:
* **TR_PUTS**: Strings are processed word-by-word from RAM.
* **TR_PUTSP**: Implements Byte Packing, storing two characters in a single 16-bit word to maximize memory efficiency.
* **TR_GETC**: Reads a single character and updates condition flags.

## Building and Running

### Prerequisites
* A C compiler (GCC/Clang)
* A Linux/Unix-based environment (for `<termios.h>` and `select()`)

### Compile
```bash
gcc -std=c99 -O3 erktvm.c -o erktvm
