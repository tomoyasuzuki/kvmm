#pragma once

#include <linux/kvm.h>
#include "type.h"
#include "vcpu.h"
#include "util.h"

void init_debug_registers(int fd);
void handle_debug(struct vcpu *vcpu);
void add_breakpoint(int fd, u64 addr);