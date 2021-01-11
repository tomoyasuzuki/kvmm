#include "io.h"

void emulate_io_out(struct vcpu *vcpu) {
    switch (vcpu->kvm_run->io.port) {
    case 0x1F0 ... 0x1F7:
        emulate_disk_portw(vcpu);
        break;
    case 0x3F6:
        emulate_disk_portw(vcpu);
        break;
    case 0x3F8 ... 0x3FD:
        emulate_uart_portw(vcpu, vcpu->kvm_run->io.port, vcpu->kvm_run->io.count, vcpu->kvm_run->io.size);
        break;
    default:
        break;
    }
}

void emulate_io_in(struct vcpu *vcpu) {
    switch (vcpu->kvm_run->io.port) {
    case 0x1F0 ... 0x1F7:
        emulate_disk_portr(vcpu);
        break;
    case 0x3f8 ... 0x3fd:
        emulate_uart_portr(vcpu, vcpu->kvm_run->io.port);
        break;
    case 0x60 ... 0x64:
        break;
    default:
        break;
    }
}

void emulate_io(struct vcpu *vcpu) {
    switch (vcpu->kvm_run->io.direction) {
    case KVM_EXIT_IO_OUT:
        emulate_io_out(vcpu);
        break;
    case KVM_EXIT_IO_IN:
        emulate_io_in(vcpu);
        break;
    default:
        printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
        print_regs(vcpu);
        break;
    }
}
