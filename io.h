#pragma once

#include <linux/kvm.h>
#include "type.h"
#include "vcpu.h"
#include "uart.h"
#include "blk.h"

void emulate_io(struct vcpu *vcpu);