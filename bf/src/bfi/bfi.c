/*

A Brainfuck program has an implicit byte pointer, called "the pointer", which is
free to move around within an array of 30000 bytes, initially all set to zero.
The pointer itself is initialized to point to the beginning of this array.

instructions:
>   Increment the pointer.
<   Decrement the pointer.
+   Increment the byte at the pointer.
-   Decrement the byte at the pointer.
.   Output the byte at the pointer.
,   Input a byte and store it in the byte at the pointer.
[   Jump forward past the matching ] if the byte at the pointer is zero.
]   Jump backward to the matching [ unless the byte at the pointer is zero.

OR


>   becomes   ++p;
<   becomes   --p;
+   becomes   ++*p;
-   becomes   --*p;
.   becomes   putchar(*p);
,   becomes   *p = getchar();
[   becomes   while (*p) {
]   becomes   }

*/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK_SIZE 100

int8_t memory[30000];

char *load_file(FILE *file);
void eval(const char *source, size_t len);

int main(int argc, char const *argv[]) {
  if (argc < 2) {
    printf("Not enough arguments.\n\nUsage: bfc <input>\n");
    exit(0);
  }

  FILE *file = fopen(argv[1], "r");

  char *source = load_file(file);
  eval(source, strlen(source));

  free(source);
  fclose(file);
  return 0;
}

// Alocate 1k buffer and read null-terminated source into it
char *load_file(FILE *file) {
  char *source = calloc(1024, 1);
  size_t bytes_read = fread(source, 1, 1024 - 1, file);
  source[bytes_read] = 0;
  return source;
}

void print_memory(int8_t *mem, int8_t *p) {
  int8_t *x = mem;

  printf("memory: [ ");
  for (int i = 0; i < 10; i++, x++) {
    if (x == p) {
      printf("[%i]", memory[i]);
    } else {
      printf(" %i ", memory[i]);
    }
  }
  printf(" ]\n");
}

void eval(const char *source, size_t len) {
  int8_t *p = memory;

  size_t loop_positions[100];
  uint32_t nest_level = 0;

  for (size_t i = 0; i < len; i++) {
    switch (source[i]) {
    // ignore whitespace
    case ' ':
    case '\n':
    case '\t':
    case '\r':
      continue;
    case '>':
      ++p;
      break;
    case '<':
      --p;
    case '+':
      ++*p;
      break;
    case '-':
      --*p;
      break;
    case '.':
      putchar(*p);
      break;
    case ',':
      *p = getchar();
      getchar(); // eat newline
      break;
    case '[':
      loop_positions[nest_level] = i;
      nest_level++;
      if (*p == 0) {
        while (*p != ']') {
          ++i;
        }
      }
      break;
    case ']':
      if (*p != 0) {
        i = loop_positions[nest_level--]; // jump inst just after '['
      } else {
        nest_level--;
      }
      break;
    default:
      printf("Unknown instruction: '%c'", *p);
      break;
    }
    print_memory(memory, p);
  }
}
