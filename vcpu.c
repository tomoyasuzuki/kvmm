#include "vcpu.h"

struct vcpu *vcpu;

void init_vcpu(struct vm *vm) {
    int mmap_size;

    vcpu = malloc(sizeof(struct vcpu));

    vcpu->fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
    
    if (vcpu->fd < 0) {
        error("KVM_CREATE_VCPU");
    }

    mmap_size = ioctl(vm->vm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);

    if (mmap_size <= 0) {
        error("KVM_GET_VCPU_MMAP_SIZE");
    }

    vcpu->kvm_run = mmap(NULL, mmap_size, 
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED, vcpu->fd, 0);

    if (vcpu->kvm_run == MAP_FAILED) {
        error("kvm_run: failed\n");
    }
}

void print_regs() {
    ioctl(vcpu->fd, KVM_GET_REGS, &(vcpu->regs));
    printf("rip: 0x%llx\n", vcpu->regs.rip);
}

void set_sregs(struct kvm_sregs *sregs) {
    sregs->cs.selector = 0;
	sregs->cs.base = 0;
}

void set_regs() {
    if (ioctl(vcpu->fd, KVM_GET_SREGS, &(vcpu->sregs)) < 0) {
        error("KVM_GET_SREGS");
    }

    set_sregs(&vcpu->sregs);

    if (ioctl(vcpu->fd, KVM_SET_SREGS, &(vcpu->sregs)) < 0) {
        error("KVM_SET_SREGS");
    }

    vcpu->regs.rflags = DEFAULT_FLAGS;
    vcpu->regs.rip = START_ADDRESS;

    if (ioctl(vcpu->fd, KVM_SET_REGS, &(vcpu->regs)) < 0) {
        error("KVM_SET_REGS");
    }
}