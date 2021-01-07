#include "vcpu.h"

void init_vcpu(struct vm *vm, struct vcpu *vcpu) {
    int mmap_size;

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

void set_sregs(struct kvm_sregs *sregs) {
    sregs->cs.selector = 0;
	sregs->cs.base = 0;
}

void set_regs(struct vcpu *vcpu) {
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