#pragma once
#include <cstddef>

void* elf_hook(const char* soname, const char* sym_name, void* new_func);
