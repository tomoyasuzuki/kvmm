#pragma once

#include <linux/kvm.h>
#include "type.h"
#include "vcpu.h"
#include "util.h"

void init_debug_registers(int fd);
void handle_debug(struct vcpu *vcpu);
void add_breakpoint(int fd, u64 addr);
void register_symtable_num(int num);
void register_sym_en(u32 addr, char *name, int index);
u32 get_func_addr(char *name);