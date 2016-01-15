/*
http://www.muppetlabs.com/~breadbox/bf/

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

#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm/IR/Constants.h>

#define CHUNK_SIZE 100

enum Direction {
  Increment = 1,
  Decrement = -1
};

// Look for the last period in filename, replace everything after with the
// contents of ext[]. Store the new string in the already allocated result[].
// result_length specifies the size of result[]. This length will never be
// exceeded.
void replace_ext(const char filename[],
                 const char ext[],
                 char result[],
                 size_t result_length);

// Allocate 1k buffer and attempt to read null-terminated source into it.
// You are responsible for freeing this memory later.
char *load_file(const char *filename);

// Declare and initialize global memory
void setup_globals(LLVMModuleRef module);

// Declare external functions such as those defined in libc or libbfr
void setup_externals(LLVMModuleRef module);

// Emit instructions to builder which increment or decrement the value of the
// global pointer p. The direction is determined by the value of dir
void incdecp(LLVMModuleRef module, LLVMBuilderRef builder, Direction dir);

// Emit instructions to builder which increment or decrement the value pointed
// to by global pointer p. The direction is determined by the value of dir.
void incdecv(LLVMModuleRef module, LLVMBuilderRef builder, Direction dir);

// Emit instructions to builder to invoke putchar with the value pointed to
// by the global pointer p.
void putp(LLVMModuleRef module, LLVMBuilderRef builder);

// Emit instructions to builder to invoke getchar and store the result at the
// address pointed to by the global pointer p.
void getp(LLVMModuleRef module, LLVMBuilderRef builder);

// This is the meat. Iterate over each brainfuck instruction and emit
// appropriate instructions to builder.
size_t build_cfg(const char *source,
               LLVMModuleRef mod,
               LLVMBuilderRef builder,
               LLVMValueRef main);

// For debugging, emit instructions to builder which invoke printf and pass
// the pointer referenced by the global variable memory as an argument.
// The result is the contents of memory are sent to stdout up to the first
// 0 (since, of course, C strings are null terminated).
void print_memory(LLVMModuleRef mod, LLVMBuilderRef builder);

int main(int argc, const char *argv[]) {
  if (argc < 2) {
    printf("Not enough arguments.\n\nUsage: bfc <input>\n");
    exit(0);
  }

  char *source = load_file(argv[1]);

  // emit llvm blocks here
  LLVMModuleRef mod = LLVMModuleCreateWithName("bf");
  
  setup_globals(mod);
  setup_externals(mod);
  
  // int main() {
  LLVMTypeRef main_param_types[] = {
    LLVMInt32Type(), // int argc
    LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0) // char *argv[]
  };
  
  LLVMTypeRef ret_type = LLVMFunctionType(LLVMInt32Type(), main_param_types, 2, false);
  LLVMValueRef main = LLVMAddFunction(mod, "main", ret_type);

  LLVMBasicBlockRef entry = LLVMAppendBasicBlock(main, "entry");

  LLVMBuilderRef builder = LLVMCreateBuilder();
  LLVMPositionBuilderAtEnd(builder, entry);
  
  build_cfg(source, mod, builder, main);
  
  //print_memory(mod, builder);
  
  //   return 0;
  LLVMBuildRet(builder, LLVMConstInt(LLVMInt32Type(), 0, false));
  // }

  // Verify moduled
  char *error = NULL;
  LLVMVerifyModule(mod, LLVMAbortProcessAction, &error);
  LLVMDisposeMessage(error);
  
  if (argc >= 3 && strcmp(argv[2], "--emit-llvm") == 0) {
    // write output to disk
    LLVMDumpModule(mod);
  }
  
  // foo.bf -> foo.bc
  size_t max_filename_length = strlen(argv[1]) + 2;
  char output_filename[max_filename_length];
  replace_ext(argv[1], "bc", output_filename, max_filename_length);
  
  LLVMWriteBitcodeToFile(mod, output_filename);

  char *cmd;
  asprintf(&cmd, "clang ./build/libbfr.a %s", output_filename);
  system(cmd);
  free(cmd);
  
  LLVMDisposeModule(mod);
  
  free(source);
  return 0;
}

void replace_ext(const char filename[], const char ext[], char result[], size_t result_length) {
  char *mutable_filename = strdup(filename);
  char *dir = dirname(mutable_filename);
  char *base = basename(mutable_filename);
  char *dot = strrchr(base, '.');
  if (nullptr != dot) {
    dot[0] = 0;
  }
  snprintf(result, result_length, "%s/%s.%s", dir, base, ext);
  free(mutable_filename);
}

char *load_file(const char *filename) {
  FILE *file = fopen(filename, "r");
  if (NULL == file) {
    perror(filename);
    exit(-1);
  }

  char *source = (char*)calloc(1024, 1);
  size_t bytes_read = fread(source, 1, 1024 - 1, file);
  source[bytes_read] = 0;
  return source;
}

void setup_globals(LLVMModuleRef module) {

  // int8_t memory[30000];
  LLVMValueRef memory = LLVMAddGlobal(module, LLVMArrayType(LLVMInt8Type(), 30000), "memory");

  // Append zeroinitializer modifier to definition of memory global array
  llvm::Type *type = llvm::unwrap(LLVMArrayType(LLVMInt8Type(), 30000));
  LLVMValueRef zeroInit = wrap(llvm::ConstantAggregateZero::get(type));
  LLVMSetInitializer(memory, zeroInit);

  // uint8_t *p;
  LLVMValueRef p = LLVMAddGlobal(module, LLVMPointerType(LLVMInt8Type(), 0), "p");
  // p = NULL;
  //LLVMSetInitializer(p, LLVMConstNull(LLVMPointerType(LLVMInt8Type(), 0)));
  
  LLVMValueRef indices[] = {
    LLVMConstInt(LLVMInt8Type(), 0, false),
    LLVMConstInt(LLVMInt8Type(), 0, false)
  };
  
  // p = memory;
  LLVMSetInitializer(p, LLVMConstInBoundsGEP(memory, indices, 2));
}

void setup_externals(LLVMModuleRef module) {
  // int putchar(int)
  LLVMTypeRef param_types[] = { LLVMInt32Type() };
  LLVMTypeRef fnType = LLVMFunctionType(LLVMInt32Type(), param_types, 1, false);
  LLVMAddFunction(module, "putchar", fnType);

  // int getchar(void)
  fnType = LLVMFunctionType(LLVMInt32Type(), {}, 0, false);
  LLVMAddFunction(module, "getchar", fnType);
  
  // int printf(const char * restrict format, ...);
  LLVMTypeRef printf_param_types[] = { LLVMPointerType(LLVMInt8Type(), 0) };
  fnType = LLVMFunctionType(LLVMInt32Type(), printf_param_types, 1, true);
  LLVMAddFunction(module, "printf", fnType);
  
  LLVMTypeRef print_array[] = { LLVMPointerType(LLVMInt8Type(), 0), LLVMInt64Type() };
  fnType = LLVMFunctionType(LLVMVoidType(), print_array, 2, false);
  LLVMAddFunction(module, "print_array", fnType);
  
}

void incdecp(LLVMModuleRef module, LLVMBuilderRef builder, Direction dir) {
  LLVMValueRef pPtrPtrValue = LLVMGetNamedGlobal(module, "p");
  
  LLVMValueRef i1 = LLVMBuildLoad(builder, pPtrPtrValue, "");
  
  // q = p +/- 1;
  LLVMValueRef indices[] = { LLVMConstInt(LLVMInt32Type(), dir, false) };
  LLVMValueRef i2 = LLVMBuildInBoundsGEP(builder, i1, indices, 1, "");
  
  // p = q
  LLVMBuildStore(builder, i2, pPtrPtrValue);
}

void incdecv(LLVMModuleRef module, LLVMBuilderRef builder, Direction dir) {

  LLVMValueRef pPtrPtrValue = LLVMGetNamedGlobal(module, "p");
  
  LLVMValueRef pPtrValue = LLVMBuildLoad(builder, pPtrPtrValue, "");
  LLVMValueRef pValue = LLVMBuildLoad(builder, pPtrValue, "");
  
  // q = *p + 1;
  LLVMValueRef result = LLVMBuildAdd(builder, pValue, LLVMConstInt(LLVMInt8Type(), dir, false), "");
  
  // *p = q
  LLVMBuildStore(builder, result, pPtrValue);
}

void putp(LLVMModuleRef module, LLVMBuilderRef builder) {
  LLVMValueRef pPtrPtrValue = LLVMGetNamedGlobal(module, "p");
  
  LLVMValueRef pPtrValue = LLVMBuildLoad(builder, pPtrPtrValue, "");
  LLVMValueRef pValue = LLVMBuildLoad(builder, pPtrValue, "");
  
  LLVMValueRef pIntValue = LLVMBuildZExt(builder, pValue, LLVMInt32Type(), "");
  
  // Get and/or declare int putchar(int)
  LLVMValueRef putcharFn = LLVMGetNamedFunction(module, "putchar");
  
  LLVMValueRef args[] = { pIntValue };
  LLVMBuildCall(builder, putcharFn, args, 1, "");
}

void getp(LLVMModuleRef module, LLVMBuilderRef builder) {
  // Get and/or declare int getchar(void)
  LLVMValueRef getcharFn = LLVMGetNamedFunction(module, "getchar");
  
  // getchar()
  LLVMValueRef i32Result = LLVMBuildCall(builder, getcharFn, {}, 0, "");
  LLVMValueRef i8Result = LLVMBuildTrunc(builder, i32Result, LLVMInt8Type(), "");
  
  LLVMValueRef pPtrPtrValue = LLVMGetNamedGlobal(module, "p");
  LLVMValueRef pPtrValue = LLVMBuildLoad(builder, pPtrPtrValue, "");
  
  LLVMBuildStore(builder, i8Result, pPtrValue);
}

size_t whilep(const char *source,
            LLVMModuleRef mod,
            LLVMBuilderRef builder,
            LLVMValueRef main) {
  
  LLVMBasicBlockRef whileBlock = LLVMAppendBasicBlock(main, "while");
  LLVMBuildBr(builder, whileBlock);
  
  LLVMPositionBuilderAtEnd(builder, whileBlock);
  LLVMValueRef pPtrPtrValue = LLVMGetNamedGlobal(mod, "p");
  LLVMValueRef pPtrValue = LLVMBuildLoad(builder, pPtrPtrValue, "");
  LLVMValueRef pValue = LLVMBuildLoad(builder, pPtrValue, "");
  
  LLVMValueRef cmpResult = LLVMBuildICmp(builder,
                                         LLVMIntNE,
                                         pValue,
                                         LLVMConstInt(LLVMInt8Type(),
                                                      0,
                                                      false),
                                         "");
  
  LLVMBasicBlockRef thenBlock = LLVMAppendBasicBlock(main, "then");
  LLVMBasicBlockRef elseBlock = LLVMAppendBasicBlock(main, "else");
  
  LLVMBuildCondBr(builder, cmpResult, thenBlock, elseBlock);
  
  LLVMPositionBuilderAtEnd(builder, thenBlock);
  
  // emit more code... (maybe recurse here)
  // LLVMBuildStuff()
  size_t advance_by = build_cfg(source, mod, builder, main);
  // TODO: wire up return value of build_cfg to elseBlock
  
  LLVMBuildBr(builder, whileBlock);
  
  LLVMPositionBuilderAtEnd(builder, elseBlock);
  return advance_by;
}

size_t build_cfg(const char *source,
               LLVMModuleRef mod,
               LLVMBuilderRef builder,
               LLVMValueRef main) {
  size_t start_of_while = 0;
  
  for (size_t i = 0; i < strlen(source); i++) {
    switch (source[i]) {
      // ignore whitespace
    case ' ':
    case '\n':
    case '\t':
    case '\r':
      continue;
    case '>':
      incdecp(mod, builder, Increment);
      break;
    case '<':
      incdecp(mod, builder, Decrement);
        break;
    case '+':
      incdecv(mod, builder, Increment);
      break;
    case '-':
      incdecv(mod, builder, Decrement);
      break;
    case '.':
      putp(mod, builder);
      break;
    case ',':
      getp(mod, builder);
      break;
    case '[':
        start_of_while = i;
        // whilep advances our position in source
        // that amount is returned
        i += whilep(&source[i]+1, mod, builder, main);
        break;
    case ']': return i - start_of_while + 1; // +1 because we should move beyond ']'
    default:
        printf("Unknown instruction: '%c'", source[i]);
        break;
    }
    
    // Print memory
//    LLVMValueRef printArrayFn = LLVMGetNamedFunction(mod, "print_array");
//    LLVMValueRef memoryPtrPtr = LLVMGetNamedGlobal(mod, "memory");
//    LLVMValueRef indices[] = { LLVMConstInt(LLVMInt8Type(), 0, false), LLVMConstInt(LLVMInt8Type(), 0, false) };
//    LLVMValueRef gep = LLVMBuildInBoundsGEP(builder, memoryPtrPtr, indices, 2, "");
//    LLVMValueRef printArrayArgs[] = { gep, LLVMConstInt(LLVMInt64Type(), 25, false) };
//    LLVMBuildCall(builder, printArrayFn, printArrayArgs, 2, "");
  }
  return strlen(source); // we processed the entire buffer
}

void print_memory(LLVMModuleRef mod, LLVMBuilderRef builder) {
  LLVMValueRef printfFn = LLVMGetNamedFunction(mod, "printf");
  LLVMValueRef memoryPtrPtr = LLVMGetNamedGlobal(mod, "memory");
  LLVMValueRef indices[] = { LLVMConstInt(LLVMInt8Type(), 0, false), LLVMConstInt(LLVMInt8Type(), 0, false) };
  LLVMValueRef gep = LLVMBuildInBoundsGEP(builder, memoryPtrPtr, indices, 2, "");
  LLVMValueRef format_str = LLVMConstString("%02x", 4, false);
  LLVMValueRef args[] = { format_str, gep };
  LLVMBuildCall(builder, printfFn, args, 2, "");
}