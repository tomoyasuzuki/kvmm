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

struct vm *vm;
int irr_count = 0;
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

void *observe_input(void *in) {
    for (;;) {
        int size = read(STDIN_FILENO, in, 1);
        // uart->data_reg = *(char*)in;
    }
}

extern struct vcpu *vcpu;

int main(int argc, char **argv) {
    vm = malloc(sizeof(struct vm));

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

    pthread_t thread;
    char input[100];

    if (pthread_create(&thread, NULL, observe_input, input) != 0) {
        error("pthread_create");
    }

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
        default:
            printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
            break;
        }
    }
    return 1;   
}