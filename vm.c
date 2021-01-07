#include "vm.h"

void create_vm(struct vm *vm) {
    vm->fd = ioctl(vm->vm_fd, KVM_CREATE_VM, 0);
    if (vm->fd < 0) {
        error("KVM_CREATE_VM");
    }
}

void set_tss(int fd) {
    if (ioctl(fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0) {
        error("KVM_SET_TSS_ADDR");
    }
}

void load_guest_binary(void *dst) {
    int fd = open("../xv6/bootblock", O_RDONLY);
    if (fd < 0) 
        error("open fail");
    
    void *tmp = dst;
    for(;;) {
        int size = read(fd, tmp + START_ADDRESS, 
                        GUEST_BINARY_SIZE);
        if (size <= 0) 
            break;
        tmp += size;
    }
}

void memalign(void **dst, size_t size, size_t align) {
    if (posix_memalign(dst, size, align) < 0) {
        printf("memalign: faile\n");
        exit(1);
    }
}

void set_vm_mem(struct vm *vm, 
                kvm_mem *memreg,
                u64 phys_start,
                size_t size) {

    memalign(&(vm->mem), size, ALIGNMENT_SIZE);

    memreg->slot = 0;
	memreg->flags = 0;
	memreg->guest_phys_addr = phys_start;
	memreg->memory_size = (u64)size;
	memreg->userspace_addr = (unsigned long)vm->mem;
    
    if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, memreg) < 0) {
		error("KVM_SET_USER_MEMORY_REGION");
	}
}