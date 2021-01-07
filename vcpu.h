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

void init_vcpu(struct vm *vm);
void print_regs();
void set_regs();
