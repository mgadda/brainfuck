//
//  runtime.c
//  bf
//
//  Created by Matthew Gadda on 1/14/16.
//  Copyright © 2016 Matt Gadda. All rights reserved.
//

#include "runtime.h"

void print_array(int8_t memory[], size_t length) {
  printf("\n[ ");
  for (size_t i = 0; i < length; i++) {
    printf("%02d ", memory[i]);
  }
  printf("]\n");
}