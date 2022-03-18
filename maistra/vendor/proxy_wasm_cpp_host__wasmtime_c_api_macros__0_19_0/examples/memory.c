/*
Example of instantiating of the WebAssembly module and invoking its exported
function.

You can compile and run this example on Linux with:

   cargo build --release -p wasmtime-c-api
   cc examples/memory.c \
       -I crates/c-api/include \
       -I crates/c-api/wasm-c-api/include \
       target/release/libwasmtime.a \
       -lpthread -ldl -lm \
       -o memory
   ./memory

Note that on Windows and macOS the command will be similar, but you'll need
to tweak the `-lpthread` and such annotations.

Also note that this example was taken from
https://github.com/WebAssembly/wasm-c-api/blob/master/example/memory.c
originally
*/

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wasm.h>
#include <wasmtime.h>

static void exit_with_error(const char *message, wasmtime_error_t *error, wasm_trap_t *trap);

wasm_memory_t* get_export_memory(const wasm_extern_vec_t* exports, size_t i) {
  if (exports->size <= i || !wasm_extern_as_memory(exports->data[i])) {
    printf("> Error accessing memory export %zu!\n", i);
    exit(1);
  }
  return wasm_extern_as_memory(exports->data[i]);
}

wasm_func_t* get_export_func(const wasm_extern_vec_t* exports, size_t i) {
  if (exports->size <= i || !wasm_extern_as_func(exports->data[i])) {
    printf("> Error accessing function export %zu!\n", i);
    exit(1);
  }
  return wasm_extern_as_func(exports->data[i]);
}


void check(bool success) {
  if (!success) {
    printf("> Error, expected success\n");
    exit(1);
  }
}

void check_call(wasm_func_t* func, const wasm_val_vec_t* args_vec, int32_t expected) {
  wasm_val_t results[1];
  wasm_val_vec_t results_vec = WASM_ARRAY_VEC(results);
  wasm_trap_t *trap = NULL;
  wasmtime_error_t *error = wasmtime_func_call(func, args_vec, &results_vec, &trap);
  if (error != NULL || trap != NULL)
    exit_with_error("failed to call function", error, trap);
  if (results[0].of.i32 != expected) {
    printf("> Error on result\n");
    exit(1);
  }
}

void check_call0(wasm_func_t* func, int32_t expected) {
  wasm_val_vec_t args_vec = WASM_EMPTY_VEC;
  check_call(func, &args_vec, expected);
}

void check_call1(wasm_func_t* func, int32_t arg, int32_t expected) {
  wasm_val_t args[] = { WASM_I32_VAL(arg) };
  wasm_val_vec_t args_vec = WASM_ARRAY_VEC(args);
  check_call(func, &args_vec, expected);
}

void check_call2(wasm_func_t* func, int32_t arg1, int32_t arg2, int32_t expected) {
  wasm_val_t args[] = { WASM_I32_VAL(arg1), WASM_I32_VAL(arg2) };
  wasm_val_vec_t args_vec = WASM_ARRAY_VEC(args);
  check_call(func, &args_vec, expected);
}

void check_ok(wasm_func_t* func, const wasm_val_vec_t* args_vec) {
  wasm_trap_t *trap = NULL;
  wasm_val_vec_t results_vec = WASM_EMPTY_VEC;
  wasmtime_error_t *error = wasmtime_func_call(func, args_vec, &results_vec, &trap);
  if (error != NULL || trap != NULL)
    exit_with_error("failed to call function", error, trap);
}

void check_ok2(wasm_func_t* func, int32_t arg1, int32_t arg2) {
  wasm_val_t args[] = { WASM_I32_VAL(arg1), WASM_I32_VAL(arg2) };
  wasm_val_vec_t args_vec = WASM_ARRAY_VEC(args);
  check_ok(func, &args_vec);
}

void check_trap(wasm_func_t* func, const wasm_val_vec_t* args_vec, size_t num_results) {
  assert(num_results <= 1);
  wasm_val_t results[1];
  wasm_val_vec_t results_vec;
  results_vec.data = results;
  results_vec.size = num_results;
  wasm_trap_t *trap = NULL;
  wasmtime_error_t *error = wasmtime_func_call(func, args_vec, &results_vec, &trap);
  if (error != NULL)
    exit_with_error("failed to call function", error, NULL);
  if (trap == NULL) {
    printf("> Error on result, expected trap\n");
    exit(1);
  }
  wasm_trap_delete(trap);
}

void check_trap1(wasm_func_t* func, int32_t arg) {
  wasm_val_t args[] = { WASM_I32_VAL(arg) };
  wasm_val_vec_t args_vec = WASM_ARRAY_VEC(args);
  check_trap(func, &args_vec, 1);
}

void check_trap2(wasm_func_t* func, int32_t arg1, int32_t arg2) {
  wasm_val_t args[] = { WASM_I32_VAL(arg1), WASM_I32_VAL(arg2) };
  wasm_val_vec_t args_vec = WASM_ARRAY_VEC(args);
  check_trap(func, &args_vec, 0);
}

int main(int argc, const char* argv[]) {
  // Initialize.
  printf("Initializing...\n");
  wasm_engine_t* engine = wasm_engine_new();
  wasm_store_t* store = wasm_store_new(engine);

  // Load our input file to parse it next
  FILE* file = fopen("examples/memory.wat", "r");
  if (!file) {
    printf("> Error loading file!\n");
    return 1;
  }
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0L, SEEK_SET);
  wasm_byte_vec_t wat;
  wasm_byte_vec_new_uninitialized(&wat, file_size);
  if (fread(wat.data, file_size, 1, file) != 1) {
    printf("> Error loading module!\n");
    return 1;
  }
  fclose(file);

  // Parse the wat into the binary wasm format
  wasm_byte_vec_t binary;
  wasmtime_error_t *error = wasmtime_wat2wasm(&wat, &binary);
  if (error != NULL)
    exit_with_error("failed to parse wat", error, NULL);
  wasm_byte_vec_delete(&wat);

  // Compile.
  printf("Compiling module...\n");
  wasm_module_t* module = NULL;
  error = wasmtime_module_new(engine, &binary, &module);
  if (error)
    exit_with_error("failed to compile module", error, NULL);
  wasm_byte_vec_delete(&binary);

  // Instantiate.
  printf("Instantiating module...\n");
  wasm_instance_t* instance = NULL;
  wasm_trap_t *trap = NULL;
  wasm_extern_vec_t imports = WASM_EMPTY_VEC;
  error = wasmtime_instance_new(store, module, &imports, &instance, &trap);
  if (!instance)
    exit_with_error("failed to instantiate", error, trap);

  // Extract export.
  printf("Extracting exports...\n");
  wasm_extern_vec_t exports;
  wasm_instance_exports(instance, &exports);
  size_t i = 0;
  wasm_memory_t* memory = get_export_memory(&exports, i++);
  wasm_func_t* size_func = get_export_func(&exports, i++);
  wasm_func_t* load_func = get_export_func(&exports, i++);
  wasm_func_t* store_func = get_export_func(&exports, i++);

  wasm_module_delete(module);

  // Try cloning.
  wasm_memory_t* copy = wasm_memory_copy(memory);
  wasm_memory_delete(copy);

  // Check initial memory.
  printf("Checking memory...\n");
  check(wasm_memory_size(memory) == 2);
  check(wasm_memory_data_size(memory) == 0x20000);
  check(wasm_memory_data(memory)[0] == 0);
  check(wasm_memory_data(memory)[0x1000] == 1);
  check(wasm_memory_data(memory)[0x1003] == 4);

  check_call0(size_func, 2);
  check_call1(load_func, 0, 0);
  check_call1(load_func, 0x1000, 1);
  check_call1(load_func, 0x1003, 4);
  check_call1(load_func, 0x1ffff, 0);
  check_trap1(load_func, 0x20000);

  // Mutate memory.
  printf("Mutating memory...\n");
  wasm_memory_data(memory)[0x1003] = 5;
  check_ok2(store_func, 0x1002, 6);
  check_trap2(store_func, 0x20000, 0);

  check(wasm_memory_data(memory)[0x1002] == 6);
  check(wasm_memory_data(memory)[0x1003] == 5);
  check_call1(load_func, 0x1002, 6);
  check_call1(load_func, 0x1003, 5);

  // Grow memory.
  printf("Growing memory...\n");
  check(wasm_memory_grow(memory, 1));
  check(wasm_memory_size(memory) == 3);
  check(wasm_memory_data_size(memory) == 0x30000);

  check_call1(load_func, 0x20000, 0);
  check_ok2(store_func, 0x20000, 0);
  check_trap1(load_func, 0x30000);
  check_trap2(store_func, 0x30000, 0);

  check(! wasm_memory_grow(memory, 1));
  check(wasm_memory_grow(memory, 0));

  wasm_extern_vec_delete(&exports);
  wasm_instance_delete(instance);

  // Create stand-alone memory.
  printf("Creating stand-alone memory...\n");
  wasm_limits_t limits = {5, 5};
  wasm_memorytype_t* memorytype = wasm_memorytype_new(&limits);
  wasm_memory_t* memory2 = wasm_memory_new(store, memorytype);
  check(wasm_memory_size(memory2) == 5);
  check(! wasm_memory_grow(memory2, 1));
  check(wasm_memory_grow(memory2, 0));

  wasm_memorytype_delete(memorytype);
  wasm_memory_delete(memory2);

  // Shut down.
  printf("Shutting down...\n");
  wasm_store_delete(store);
  wasm_engine_delete(engine);

  // All done.
  printf("Done.\n");
  return 0;
}

static void exit_with_error(const char *message, wasmtime_error_t *error, wasm_trap_t *trap) {
  fprintf(stderr, "error: %s\n", message);
  wasm_byte_vec_t error_message;
  if (error != NULL) {
    wasmtime_error_message(error, &error_message);
    wasmtime_error_delete(error);
  } else {
    wasm_trap_message(trap, &error_message);
    wasm_trap_delete(trap);
  }
  fprintf(stderr, "%.*s\n", (int) error_message.size, error_message.data);
  wasm_byte_vec_delete(&error_message);
  exit(1);
}
