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

int outfd = 0;

void init_kvm(struct vm *vm) {
    vm->vm_fd = open("/dev/kvm", O_RDWR);
    if (vm->vm_fd < 0) { 
        error("open /dev/kvm");
    }
}

void create_output_file() {
    outfd = open("out.txt", O_RDWR | O_CREAT);
}

struct input {
    char *in;
    struct vcpu *vcpu;
};

void *observe_input(void *in) {
    int c = 0;
    struct input *subin = (struct input*)in;

    while(1) {
        c = getchar();
        set_uart_buff((char)c);
        enq_irr(subin->vcpu, IRQ_BASE+4);
    }
}

u32 get_target_addr(char *cm);
void vm_loop(int *vm_status);
void vm_run(int *vm_status);
char *get_path(char *cm);
void get_symbol_table(char *path);

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

    // pthread_t thread;
    // struct input subin;
    // char *usrinput = malloc(100);
    // subin.in = usrinput;
    // subin.vcpu = vcpu;

    // if (pthread_create(&thread, NULL, handle_command, (void*)usrinput) != 0) {
    //     error("pthread_create");
    // }
    int vm_status = 0;
    char cm[100];

    for (;;) {
        printf("(kvmm) ");
        fflush(stdout);
        read(STDIN_FILENO, cm, 20);
        char first[2];
        memcpy(first, cm, 1);
        if (*first == 'b') {
            u32 addr = get_target_addr(cm);
            add_breakpoint(vcpu->fd, (u64)addr);
        } else if (*first == 'r') {
            vm_status = 1;
        } else if (*first == 'c') {
            vm_status = 1;
        } else if (*first == 'f') {
            get_symbol_table(get_path(cm));
        }

        vm_loop(&vm_status);
    }
   
    return 1;   
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

    memcpy(path, cm+path_start, path_end-path_start-1);
    path[path_end-path_start-1] = '\0';
    return path;
}

void get_symbol_table(char *path) {
    FILE *file = NULL;
    size_t size = 0;
    void *buff = malloc(200000);
    char *sname = malloc(1000);
    Elf32_Ehdr *ehdr;
    Elf32_Shdr *shdr = malloc(200000);
    Elf32_Shdr *sym = malloc(200000);
    Elf32_Sym *sym_en = malloc(200000);
    Elf32_Shdr *strtab = malloc(200000);
    Elf32_Shdr *shstr = malloc(200000);

    if ((file = fopen(path, "rb")) == NULL) {
        error("OPEN ERROR");
    }

     fseek(file, 0, SEEK_END);
     size = (size_t)ftell(file);
     fseek(file, 0, SEEK_SET);
     fread(buff, sizeof(char), size, file);

    ehdr = (Elf32_Ehdr*)buff;

    shstr = (Elf32_Shdr*)(buff + ehdr->e_shoff + ehdr->e_shentsize * ehdr->e_shstrndx);

    for (int i = 0;i  < ehdr->e_shnum; i++) {
        shdr = (Elf32_Shdr*)(buff + ehdr->e_shoff + ehdr->e_shentsize * i);
        sname = (char*)(buff + shstr->sh_offset + shdr->sh_name);
        if (!strcmp(sname, ".strtab"))
            strtab = shdr;
    }

    if (ehdr->e_ident[0] != 0x7f | ehdr->e_ident[EI_CLASS] != ELFCLASS32) {
        printf("Magic number or class is wrong\n");
        exit(0);
    }

    for (int i = 0; i < ehdr->e_shnum; i++) {
        shdr = (Elf32_Shdr*)(buff + ehdr->e_shoff + ehdr->e_shentsize * i);
        if (shdr->sh_type != SHT_SYMTAB)
            continue;

        sym = shdr;
        for (int j = 0; j < sym->sh_size / sym->sh_entsize; j++) {
            sym_en = (Elf32_Sym*)(buff + sym->sh_offset + sym->sh_entsize * j);
            if (!sym_en->st_name || sym->sh_type != STT_FUNC)
                continue;
            
            printf("%s\n", (char*)(buff + strtab->sh_offset + sym_en->st_name));
        }
        break;
    }

    
}

u32 get_target_addr(char *cm) {
    char target[7];
    u32 addr;
    for (int i = 0; i < (int)(strlen(cm)); i++) {
        if (cm[i] == ' ') {    
            strncpy(target, cm+i+1, 6);
            addr = (u32)strtol(target, NULL, 0);
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