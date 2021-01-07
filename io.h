#pragma once
#include <linux/kvm.h>

struct io {
    __u8 direction;
    __u8 size;
    __u16 port;
    __u32 count;
    __u64 data_offset; 
};