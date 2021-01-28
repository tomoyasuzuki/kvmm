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

union dr7 {
    u32 control;
    struct {
        u32 l0 : 1;
        u32 g0 : 1;
        u32 l1 : 1;
        u32 g1 : 1;
        u32 l2 : 1;
        u32 g2 : 1;
        u32 l3 : 1;
        u32 g3 : 1;
        u32 le : 1;
        u32 ge : 1;
        u32 reserved1 : 3;
        u32 gd : 1;
        u32 reserved2 : 2;
        u32 rw0 : 2;
        u32 len0 : 2;
        u32 rw1 : 2;
        u32 len1 : 2;
        u32 rw2 : 2;
        u32 len2 : 2;
        u32 rw3 : 2;
        u32 len3 : 2;
    } bits;
};

union dr6 {
    u32 control;
    struct {
        u32 b0 : 1;
        u32 b1: 1;
        u32 b2: 1;
        u32 b3: 1;
        u32 reserved1 : 8;
        u32 reserved1sub : 1;
        u32 bd : 1;
        u32 bs : 1;
        u32 bt : 1;
        u32 reserved2 : 16;
    } bits;
};

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


    struct kvm_guest_debug *debug = malloc(sizeof(struct kvm_guest_debug));
    debug->control = 0;
    debug->pad = 0;
    debug->control |= KVM_GUESTDBG_ENABLE;
    debug->control |= KVM_GUESTDBG_USE_HW_BP;
    union dr7 dr7;
    dr7.control = 0;
    dr7.bits.reserved1 = 1;
    dr7.bits.reserved2= 0;
    dr7. bits.l0 = 1;
    dr7.bits.len0 = 0;
    dr7.bits.g0 = 1; // first grobal breakpoints
    dr7.bits.rw0 = 0; // only execution
    union dr6 dr6;
    dr6.control = 0xFFFFFFFF;
    dr6.bits.b0 = 0;
    dr6.bits.b1 = 0;
    dr6.bits.b2 = 0;
    dr6.bits.b3 = 0;
    dr6.bits.reserved1 = 0xFF;
    dr6.bits.reserved1sub = 0;
    dr6.bits.bd = 0;
    dr6.bits.bs = 0;
    dr6.bits.bt = 0;
    u32 dr0 = 0x7c00; // break address
    debug->arch.debugreg[0] = (unsigned long long)dr0;
    debug->arch.debugreg[6] = (unsigned long long)dr6.control;
    debug->arch.debugreg[7] = (unsigned long long)dr7.control;

    if (ioctl(vcpu->fd, KVM_SET_GUEST_DEBUG, debug) < 0) {
        error("KVM_SET_GUEST_DEBUG");
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