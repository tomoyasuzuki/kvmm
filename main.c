#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <linux/kvm.h>
#include <pthread.h>
#include <termios.h>
#include "util.h"
#include "type.h"
#include "vcpu.h"
#include "blk.h"
#include "io.h"
#include "lapic.h"
#include "ioapic.h"
#include "mmio.h"
#include "uart.h"
#include "vm.h"
#include "debug.h"
#include <elf.h>

// void *observe_input(void *in) {
//     int c = 0;
//     struct input *subin = (struct input*)in;

//     while(1) {
//         c = getchar();
//         set_uart_buff((char)c);
//         enq_irr(subin->vcpu, IRQ_BASE+4);
//     }
// }

// struct input {
//     char *in;
//     struct vcpu *vcpu;
// };

typedef enum {
    Run,
    Continue,
    Breakpoint,
    File,
    Unknown
} CommandType;

u32 get_target_addr(char *cm);
void vm_loop(int *vm_status);
void vm_run(int *vm_status);
char *get_path(char *cm);
void get_symbol_table(char *path);
void handle_breakpoint(char *cm);
void handle_command(char *cm, int *vm_status);
CommandType parse_command(char *cm);

extern struct vcpu *vcpu;

int main(int argc, char **argv) {
    struct vm *vm = malloc(sizeof(struct vm));
    kvm_mem *memreg = malloc(sizeof(kvm_mem));

    init_kvm(vm);
    create_vm(vm);
    create_blk();
    set_tss(vm->fd);
    set_vm_mem(vm, memreg, 0, GUEST_MEMORY_SIZE);    
    load_guest_binary(vm->mem);
    init_vcpu(vm);
    init_lapic();
    init_ioapic();
    set_regs();
    create_uart();
    create_output_file();
    init_debug_registers(vcpu->fd);

    int vm_status = 0;
    char cm[100];

    for (;;) {
        printf("(kvmm) ");
        fflush(stdout);
        fgets(cm, 20, stdin);
        handle_command(cm, &vm_status);
        vm_loop(&vm_status);
    }
   
    return 1;   
}

void handle_command(char *cm, int *vm_status) {
    CommandType type = parse_command(cm);

    switch (type) {
    case Breakpoint:
        handle_breakpoint(cm);
        break;
    case Run:
        *vm_status = 1;
        break;
    case Continue:
        *vm_status = 1;
        break;
    case File:
        get_symbol_table(get_path(cm));
        break;
    default:
        printf("Unknown commnad\n");
        break;
    }
}

void handle_breakpoint(char *cm) {
    u32 addr;
    if ((addr = get_target_addr(cm)) == 0xffffffff) {
        printf("function not found\n");
        return;
    }
    
    add_breakpoint(vcpu->fd, addr);
}

CommandType parse_command(char *cm) {
    char com[30];
    char first[2];

    // handle a character commnads.
    memcpy(first, cm, 1);
    if (*first == 'b') {
        return Breakpoint;
    } else if (*first == 'r') {
        return Run;
    } else if (*first == 'c') {
        return Continue;
    }

    for (int i = 0; i < (int)(strlen(cm)); i++) {
        if (cm[i] == ' ') {
            memcpy(com, cm, i);
            break;
        }
    }

    // handle multiple characters commnads.
    if (memcmp(com, "file", 4) == 0) {
        return File;
    } else {
        return Unknown;
    }
}

char *get_path(char *cm) {
    char *path = malloc(100);
    int path_start;
    int path_end = (int)(strlen(cm))-1;
    for (int i = 0; i < path_end; i++) {
        if (cm[i] == ' ') {
            path_start = i+1;
            break;
        }
    }

    memcpy(path, cm+path_start, path_end-path_start);
    path[path_end-path_start] = '\0';
    return path;
}

void get_symbol_table(char *path) {
    FILE *file = NULL;
    size_t size = 0;
    void *buff;
    char *sname;
    Elf32_Ehdr *ehdr;
    Elf32_Shdr *shdr, *shstr, *strtab, *sym;
    Elf32_Sym *sym_en;

    if ((file = fopen(path, "rb")) == NULL) {
        error("OPEN ERROR");
    }

    fseek(file, 0, SEEK_END);
    size = (size_t)(ftell(file));
    fseek(file, 0, SEEK_SET);

    buff = malloc(size);
    fread(buff, 1, size, file);

    ehdr = (Elf32_Ehdr*)buff;
    shstr = (Elf32_Shdr*)(buff + ehdr->e_shoff + ehdr->e_shentsize * ehdr->e_shstrndx);

    for (int i = 0;i  < ehdr->e_shnum; i++) {
        shdr = (Elf32_Shdr*)(buff + ehdr->e_shoff + ehdr->e_shentsize * i);
        sname = (char*)(buff + shstr->sh_offset + shdr->sh_name);
        if (!strcmp(sname, ".strtab"))
            strtab = shdr;
    }

    if (ehdr->e_ident[0] != 0x7f) {
        printf("Error: ELF magic number is wrong\n");
        return;
    }

    if ( ehdr->e_ident[EI_CLASS] != ELFCLASS32) {
        printf("Error: kvmm can only accept 32bit Elf file\n");
        return;
    }

    for (int i = 0; i < ehdr->e_shnum; i++) {
        shdr = (Elf32_Shdr*)(buff + ehdr->e_shoff + ehdr->e_shentsize * i);
        if (shdr->sh_type != SHT_SYMTAB)
            continue;

        sym = shdr;
        int sym_func_index = 0;
        for (int j = 0; j < sym->sh_size / sym->sh_entsize; j++) {
            sym_en = (Elf32_Sym*)(buff + sym->sh_offset + sym->sh_entsize * j);
            if (!sym_en->st_name || sym->sh_type != STT_FUNC)
                continue;

            if (sym_func_index == 0)
                register_symtable_num(sym->sh_size / sym->sh_entsize);
        
            register_sym_en(sym_en->st_value, 
                            (char*)(buff + strtab->sh_offset + sym_en->st_name), sym_func_index);
            sym_func_index++;
        }
        break;
    }

    free(buff);
}

u32 get_target_addr(char *cm) {
    char target_addr[10];
    char target_name[100];
    u32 addr;
    for (int i = 0; i < (int)(strlen(cm)); i++) {
        if (cm[i] == ' ') {    
            char prefix[3];
            strncpy(prefix, cm+i+1, 2);
            if (memcmp(prefix, "0x", 2) == 0) {
                //expect address is in 0x0~0xffffffff
                strncpy(target_addr, cm+i+1, 10); 
                addr = (u32)strtol(target_addr, NULL, 0);
                break;
            }
            
            int name_length = strlen(cm+i+1);
            printf("length %d\n", name_length);
            strncpy(target_name, cm+i+1, name_length);
            // replace '\n' with '\0'
            target_name[name_length-1] = '\0';
            // printf("name: ");
            // for (int n = 0; n < name_length; n++) {
            //     printf("%d ", (int)target_name[n]);
            // }
            // printf("\n");
            if ((addr = get_func_addr(target_name)) == 0xffffffff)
                return 0xffffffff;
            break;
        }
    }

    return addr;
}

void vm_run(int *vm_status) {
    if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
        print_regs(vcpu);
        error("KVM_RUN");
    } 

    struct kvm_run *run = vcpu->kvm_run;

    switch (run->exit_reason) {
    case KVM_EXIT_IO:
        emulate_io(vcpu);
        break;
    case KVM_EXIT_MMIO:
        emulate_mmio(vcpu);
        break;
    case KVM_EXIT_IRQ_WINDOW_OPEN:
        emulate_interrupt(vcpu);
        break;
    case KVM_EXIT_DEBUG:
        handle_debug(vcpu);
        *vm_status = 0;
        break;
    default:
        printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
        break;

    }
}

void vm_loop(int *vm_status) {
    if (*vm_status == 0)
        return;
    
    for(;;) {
        if (*vm_status == 0)
            break;
        vm_run(vm_status);
    }
}