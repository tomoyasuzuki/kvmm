#pragma once

#include <linux/kvm.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "util.h"
#include "type.h"

#define GUEST_PATH "../xv6/xv6.img"
#define START_ADDRESS 0x7c00
#define GUEST_MEMORY_SIZE 0x80000000
#define ALIGNMENT_SIZE 0x1000
#define DEFAULT_FLAGS 0x0000000000000002ULL
#define GUEST_BINARY_SIZE (4096 * 128)
#define FS_IMAGE_SIZE (500 * 1024)

typedef struct kvm_userspace_memory_region kvm_mem;

struct vm {
    int vm_fd;
    int fd;
    void *mem;
};

void create_vm(struct vm *vm);
void set_tss(int fd);
void load_guest_binary(void *dst);
void set_vm_mem(struct vm *vm, kvm_mem *memreg, u64 phys_start, size_t size);

