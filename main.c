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

// void *handle_command(void *input) {
//     for(;;) {
//         char *b = 'b';
//         char *ini_c;
//         memcpy(ini_c, input, 1);
//         if (*b == *ini_c) {
//             printf("initial value is b\n");
//             exit(1);
//         }
//     }
// }
u32 get_target_addr(char *cm);
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
            printf("target: 0x%x\n", addr);
        } else if (*first == 'r') {
            printf("setup r\n");
        }
        if (vm_status) {
            vm_run();
        }
    }
   
    return 1;   
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

void handle_command(char *cm) {
    char b = 'b';
    char r = 'r';
    char *first = malloc(1);
    memcpy(first, cm, 1);
    if (*first == b) {
        printf("setup b\n");
    } else if (*first == r) {
        printf("setup r\n");
    }
}

void vm_run() {
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
        break;
    default:
        printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
        break;
    }
}