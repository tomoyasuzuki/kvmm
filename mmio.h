#pragma once

#include <linux/kvm.h>
#include "vcpu.h"
#include "vm.h"
#include "lapic.h"
#include "ioapic.h"

struct mmio {
    __u64 phys_addr;
	__u8  data[8];
	__u32 len;
	__u8  is_write;
};

void emulate_mmio(struct vcpu *vcpu);