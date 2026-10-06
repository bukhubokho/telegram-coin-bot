// language: C++17, file: elf_hook.cpp, target: Android ARM64/ARM, NDK
// scan /proc/self/maps → parse ELF GOT → patch function pointer
#include "elf_hook.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/mman.h>
#include <elf.h>
#include <link.h>
#include <dlfcn.h>
#include <android/log.h>
#include <unistd.h>

#define TAG "vanta"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#ifdef __LP64__
using Elf_Ehdr = Elf64_Ehdr;
using Elf_Phdr = Elf64_Phdr;
using Elf_Dyn  = Elf64_Dyn;
using Elf_Sym  = Elf64_Sym;
using Elf_Rela = Elf64_Rela;
using Elf_Addr = Elf64_Addr;
#define ELF_R_SYM  ELF64_R_SYM
#else
using Elf_Ehdr = Elf32_Ehdr;
using Elf_Phdr = Elf32_Phdr;
using Elf_Dyn  = Elf32_Dyn;
using Elf_Sym  = Elf32_Sym;
using Elf_Rela = Elf32_Rel;
using Elf_Addr = Elf32_Addr;
#define ELF_R_SYM  ELF32_R_SYM
#endif

struct SoInfo { uintptr_t base; size_t size; };

static SoInfo find_so(const char* name) {
    FILE* f = fopen("/proc/self/maps", "r");
    if (!f) return {};
    char line[512];
    SoInfo info{};
    while (fgets(line, sizeof(line), f)) {
        if (!strstr(line, name)) continue;
        uintptr_t start, end;
        if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
            if (!info.base) info.base = start;
            info.size = end - info.base;
        }
    }
    fclose(f);
    return info;
}

static void patch_ptr(void** slot, void* new_fn, void** old_out) {
    uintptr_t page = (uintptr_t)slot & ~(uintptr_t)(getpagesize() - 1);
    mprotect((void*)page, getpagesize(), PROT_READ | PROT_WRITE);
    if (old_out) *old_out = *slot;
    *slot = new_fn;
    mprotect((void*)page, getpagesize(), PROT_READ);
    __builtin___clear_cache((char*)slot, (char*)slot + sizeof(void*));
}

void* elf_hook(const char* soname, const char* sym_name, void* new_func) {
    SoInfo so = find_so(soname);
    if (!so.base) { LOGE("so not found: %s", soname); return nullptr; }

    auto* ehdr = (Elf_Ehdr*)so.base;
    if (ehdr->e_ident[0] != 0x7f || memcmp(ehdr->e_ident + 1, "ELF", 3) != 0)
        return nullptr;

    auto* phdr = (Elf_Phdr*)(so.base + ehdr->e_phoff);
    uintptr_t dyn_off = 0;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_DYNAMIC) { dyn_off = phdr[i].p_vaddr; break; }
    }
    if (!dyn_off) return nullptr;

    auto*    dyn      = (Elf_Dyn*)(so.base + dyn_off);
    Elf_Rela* rela    = nullptr;
    Elf_Sym*  sym     = nullptr;
    char*     strtab  = nullptr;
    size_t    rela_count = 0;

    for (; dyn->d_tag != DT_NULL; dyn++) {
        switch (dyn->d_tag) {
        case DT_JMPREL:   rela      = (Elf_Rela*)(so.base + dyn->d_un.d_ptr); break;
        case DT_PLTRELSZ: rela_count = dyn->d_un.d_val / sizeof(Elf_Rela);    break;
        case DT_SYMTAB:   sym       = (Elf_Sym*)(so.base + dyn->d_un.d_ptr);  break;
        case DT_STRTAB:   strtab    = (char*)(so.base + dyn->d_un.d_ptr);     break;
        }
    }
    if (!rela || !sym || !strtab) return nullptr;

    void* original = nullptr;
    for (size_t i = 0; i < rela_count; i++) {
        uint32_t idx = ELF_R_SYM(rela[i].r_info);
        const char* name = strtab + sym[idx].st_name;
        if (strcmp(name, sym_name) != 0) continue;
        void** slot = (void**)(so.base + rela[i].r_offset);
        LOGI("hooking %s @ %p in %s", sym_name, slot, soname);
        patch_ptr(slot, new_func, &original);
        break;
    }
    return original;
}
