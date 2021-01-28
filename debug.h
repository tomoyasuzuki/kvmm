#pragma once

#include <linux/kvm.h>
#include "type.h"
#include "vcpu.h"
#include "util.h"

void init_debug_registers(int fd, unsigned int address);