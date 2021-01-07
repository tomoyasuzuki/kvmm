#pragma once

#include <linux/kvm.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "vm.h"
#include "util.h"

struct vcpu {
    int fd;
    struct kvm_run *kvm_run;
    struct kvm_sregs sregs;
    struct kvm_regs regs;
};

void init_vcpu(struct vm *vm, struct vcpu *vcpu);
void set_sregs(struct kvm_sregs *sregs);
void set_regs(struct vcpu *vcpu);
