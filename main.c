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

int outfd = 0;

struct vm *vm;
struct blk *blk;
struct uart *uart;
int irr_count = 0;

void init_kvm(struct vm *vm) {
    vm->vm_fd = open("/dev/kvm", O_RDWR);
    if (vm->vm_fd < 0) { 
        error("open /dev/kvm");
    }
}

void create_output_file() {
    outfd = open("out.txt", O_RDWR | O_CREAT);
}

void emulate_mmio(struct vcpu *vcpu, struct vm *vm,
                  struct lapic *lapic,
                  struct ioapic *ioapic) {
    struct mmio mmio = {
        .data = vcpu->kvm_run->mmio.data,
        .is_write = vcpu->kvm_run->mmio.is_write,
        .len = vcpu->kvm_run->mmio.len,
        .phys_addr = vcpu->kvm_run->mmio.phys_addr
    };

    if (mmio.phys_addr >= LAPIC_BASE)
        emulate_lapicw(vcpu);
    if (mmio.phys_addr >= IOAPIC_BASE) {
        emulate_ioapicw(vcpu);
    }
}

void *observe_input(void *in) {
    for (;;) {
        int size = read(STDIN_FILENO, in, 1);
        uart->data_reg = *(char*)in;
    }
}

extern struct vcpu *vcpu;
extern struct lapic *lapic;
extern struct ioapic *ioapic;

int main(int argc, char **argv) {
    ioapic = malloc(sizeof(struct ioapic));
    lapic = malloc(sizeof(struct lapic));
    lapic->irr = malloc(4096);
    vm = malloc(sizeof(struct vm));
    blk = malloc(sizeof(struct blk));
    uart = malloc(sizeof(struct uart));

    kvm_mem *memreg = malloc(sizeof(kvm_mem));

    init_kvm(vm);
    create_vm(vm);
    create_blk(blk);
    set_tss(vm->fd);
    set_vm_mem(vm, memreg, 0, GUEST_MEMORY_SIZE);    
    load_guest_binary(vm->mem);
    init_vcpu(vm);
    set_regs();
    create_uart(uart);
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
            emulate_mmio(vcpu, vm, lapic, ioapic);
            break;
        case KVM_EXIT_IRQ_WINDOW_OPEN:
            if (lapic->irr->buff[0] >= 32) {
                inject_interrupt(vcpu->fd, lapic->irr->buff[0]);
                deq_irr(lapic->irr);
                vcpu->kvm_run->request_interrupt_window = 0;
            }
            break;
        default:
            printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
            break;
        }
    }
    return 1;   
}