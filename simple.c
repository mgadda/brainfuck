/*
 * This file can be used to generate basic LLVM IR
 * instructions for brainfuck.
 *
 * Compile with: `gcc -O0 -S -emit-llvm simple.c`
 *
 * Then look at `simple.ll`
 */

#include <stdint.h>
#include <stdio.h>

uint8_t memory[30000] = {0};
uint8_t *p;

void incp() {
  ++p;
}

void decp() {
  --p;
}

void incv() {
  ++*p;
}

void decv() {
  --*p;
}

void putp() {
  putchar(*p);
}

void getp() {
  *p = getchar();
}

// This function should not actually be ported
// to LLVM IR since the body of the while loop
// needs to be emitted at compile time.
// Nevertheless, it serves as a fine example of
// how to emit the right kind of logic.
void whilep() {
  while (*p) {
    // emit more code
    incp();
    getp();
    incv();
  }
}

int main() {
  printf("hello, world\n");
  return 0;
}
