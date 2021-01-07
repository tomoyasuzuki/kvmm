#pragma once
#include <linux/kvm.h>

struct vcpu {
    int fd;
    struct kvm_run *kvm_run;
    struct kvm_sregs sregs;
    struct kvm_regs regs;
};