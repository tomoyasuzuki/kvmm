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

extern struct vcpu *vcpu;

int main(int argc, char **argv) {
    struct vm *vm = malloc(sizeof(struct vm));
    kvm_mem *memreg = malloc(sizeof(kvm_mem));
    struct termios *tos = malloc(sizeof(struct termios));

    // if (tcgetattr(STDIN_FILENO, tos) < 0) {
    //     error("get failed\n");
    // }

    // tos->c_lflag &= ~(ECHO | ICANON);

    // if (tcsetattr(STDIN_FILENO, TCSAFLUSH, tos) < 0) {
    //     error("set failed\n");
    // }

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

    pthread_t thread;
    struct input subin;
    char *usrinput = malloc(100);
    subin.in = usrinput;
    subin.vcpu = vcpu;

    if (pthread_create(&thread, NULL, observe_input, (void*)&subin) != 0) {
        error("pthread_create");
    }

    // dr6: 0xffff0ff0
    // dr7: 0x403
    printf("(kvmm) ");
    unsigned int input_address = 0;
    scanf("%x", &input_address);
    init_debug_registers(vcpu->fd, input_address);

    for (;;) {
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
            printf("DEBUG: ");
            print_regs();
            exit(1);
            break;
        default:
            printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
            break;
        }
    }
    return 1;   
}