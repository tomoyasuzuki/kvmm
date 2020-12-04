#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <linux/kvm.h>

#define CODE_START 0
#define GUEST_PATH "../xv6/xv6.img"

typedef struct kvm_userspace_memory_region kvm_mem;

struct vm {
    int vm_fd;
    int fd;
    void *mem;
};

struct vcpu {
    int fd;
    struct kvm_run *kvm_run;
    struct kvm_sregs sregs;
    struct kvm_regs regs;
};

struct blk {
    uint8_t *data;
    uint16_t data_reg;
    uint8_t sec_count_reg;
    uint8_t lba_low_reg;
    uint8_t lba_middle_reg;
    uint8_t lba_high_reg;
    uint8_t drive_head_reg;
    uint8_t status_command_reg;
    uint32_t index;
};

struct mp {             // floating pointer
  unsigned char signature[4];           // "_MP_"
  void *physaddr;               // phys addr of MP config table
  unsigned char length;                 // 1
  unsigned char specrev;                // [14]
  unsigned char checksum;               // all bytes must add up to 0
  unsigned char type;                   // MP system config type
  unsigned char imcrp;
  unsigned char reserved[3];
};

struct mpconf {         // configuration table header
  unsigned char signature[4];           // "PCMP"
  unsigned short length;                // total table length
  unsigned char version;                // [14]
  unsigned char checksum;               // all bytes must add up to 0
  unsigned char product[20];            // product id
  unsigned int *oemtable;               // OEM table pointer
  unsigned short oemlength;             // OEM table length
  unsigned short entry;                 // entry count
  unsigned int *lapicaddr;              // address of local APIC
  unsigned short xlength;               // extended table length
  unsigned char xchecksum;              // extended table checksum
  unsigned char reserved;
};

struct mpproc {         // processor table entry
  unsigned char type;                   // entry type (0)
  unsigned char apicid;                 // local APIC id
  unsigned char version;                // local APIC verison
  unsigned char flags;                  // CPU flags
    #define MPBOOT 0x02           // This proc is the bootstrap processor.
  unsigned char signature[4];           // CPU signature
  unsigned int feature;                 // feature flags from CPUID instruction
  unsigned char reserved[8];
};

struct mpioapic {       // I/O APIC table entry
  unsigned char type;                   // entry type (2)
  unsigned char apicno;                 // I/O APIC id
  unsigned char version;                // I/O APIC version
  unsigned char flags;                  // I/O APIC flags
  unsigned int *addr;                  // I/O APIC address
};

void init_vcpu(struct vm *vm, struct vcpu *vcpu) {
    int vcpu_mmap_size;

    vcpu->fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
    if (vcpu->fd < 0) {
        perror("KVM_CREATE_VCPU");
        exit(1);
    }

    vcpu_mmap_size = ioctl(vm->vm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (vcpu_mmap_size <= 0) {
        perror("KVM_GET_CPU_MMAP_SIZE");
        exit(1);
    }

    vcpu->kvm_run = mmap(NULL, vcpu_mmap_size, PROT_READ | PROT_WRITE,
            MAP_SHARED, vcpu->fd, 0);
    if (vcpu->kvm_run == MAP_FAILED) {
        perror("mmap kvm_run");
        exit(1);
    }
}

void set_sregs(struct kvm_sregs *sregs) {
    sregs->cs.selector = 0;
	sregs->cs.base = 0;
}

void set_regs(struct vcpu *vcpu) {
    if (ioctl(vcpu->fd, KVM_GET_SREGS, &(vcpu->sregs)) < 0) {
        perror("KVM_GET_SREGS");
        exit(1);
    }

    set_sregs(&vcpu->sregs);

    if (ioctl(vcpu->fd, KVM_SET_SREGS, &(vcpu->sregs)) < 0) {
        perror("KVM_SET_SREGS");
        exit(1);
    }

    vcpu->regs.rflags = 0x0000000000000002ULL;
    vcpu->regs.rip = 0xfffe0000;

    if (ioctl(vcpu->fd, KVM_SET_REGS, &(vcpu->regs)) < 0) {
        perror("KVM_SET_REGS");
        exit(1);
    }
}

void set_irqchip(struct vm *vm) {
    if (ioctl(vm->fd, KVM_CREATE_IRQCHIP, 0) < 0) {
        perror("KVM_CREATE_IRQCHIP");
        exit(1);
    }
}

void load_bios(void *dst) {
    int biosfd = open("../seabios/out/bios.bin", O_RDONLY);

    int ret = 0;
    char *tmp = (char*)dst;
    ret = read(biosfd, tmp, 1024 * 128);
    printf("size: %d\n", ret);
}

void load_guest_binary(void *dst) {
    //int biosfd = open("../xv6/bootblock", O_RDONLY);
    int biosfd = open("../xv6/bootblock", O_RDONLY);
    
    if (biosfd < 0) {
        perror("open fail");
        exit(1);
    }

    int ret = 0;
    char *tmp = (char*)dst; 
    while(1) {
        ret = read(biosfd, tmp + 0x7c00, 4096 * 128);
        if (ret <= 0) break;

        printf("read size: %d\n", ret);
        tmp += ret;
    }
}

void set_user_memory_region(struct vm *vm, 
        struct kvm_userspace_memory_region *memreg) {
    size_t guest_memory_size = 0xFFFF0000;
    size_t alignment_size = 0x1000;

    int result = posix_memalign(&(vm->mem), 
            alignment_size, 
            guest_memory_size);

    if (result != 0) {
        printf("mmap fail: %d\n", result);
        exit(1);
    }

    memreg->slot = 1;
	memreg->flags = 0;
	memreg->guest_phys_addr = 0;
	memreg->memory_size = (__u64)guest_memory_size;
	memreg->userspace_addr = (unsigned long)vm->mem;
    
    if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, memreg) < 0) {
		perror("KVM_SET_USER_MEMORY_REGION");
        exit(1);
	}
}

void print_regs(struct vcpu *vcpu) {
    ioctl(vcpu->fd, KVM_GET_REGS, &(vcpu->regs));
    printf("rip: 0x%llx\n", vcpu->regs.rip);
}

void set_blk(struct blk *blk) {
    //blk = (struct blk*)malloc(sizeof(struct blk));
    blk->data = malloc(5120000);
    blk->data_reg = 0;
    blk->drive_head_reg = 0;
    blk->lba_high_reg = 0;
    blk->lba_middle_reg = 0;
    blk->lba_low_reg = 0;
    blk->sec_count_reg = 0;
    blk->status_command_reg = 0x40;


    int blk_img_fd = open("../xv6/xv6.img", O_RDONLY);
    if (blk_img_fd < 0) {
        perror("fail open xv6.img");
        exit(1);
    }

    int read_size = 0;
    void *tmp = (void*)blk->data;
    while(1) {
        read_size = read(blk_img_fd, tmp, 512000);
        if (read_size <= 0) break;

        printf("read blk: %d\n", read_size);

        tmp += read_size;
    }
}

void handle_io_out(struct blk *blk, int port, char value, __u16 val) {
    switch (port)
    {
    case 0x1F0:
        blk->data_reg = val;
        break;
    case 0x1F2:
        blk->sec_count_reg = value;
        break;
    case 0x1F3:
        blk->lba_low_reg = value;
        break;            
    case 0x1F4:
        blk->lba_middle_reg = value;
        break;
    case 0x1F5:
        blk->lba_high_reg = value;
    case 0x1F6:
        blk->drive_head_reg = value;
        break;
    case 0x1F7:
        if (value == 0x20) {
            uint32_t i = 0 | blk->lba_low_reg | (blk->lba_middle_reg << 8) | (blk->lba_high_reg << 16) |
                          ((blk->drive_head_reg & 0x0F) << 24);
            //printf("i: %d\n", i);
            blk->index = i * 512;
            break;
        }

        printf("value: %d\n", value);
        break;
    case 0x8a00:
        printf("error 0x8a00\n");
        exit(1);
        break;
    default:
        break;
    }
}

void alloc_bios_memory(struct vm *vm) {
    uint64_t bios_mem_size = (1024 * 128);
    uint64_t bios_ram1_addr = 0x00000000;
    uint64_t bios_ram1_size = (1024 * 640);
    uint64_t bios_ram2_addr = 0x000C0000;
    uint64_t bios_ram2_size = (1024 * 128);

    /*  allocate bios rom */
    int result = posix_memalign(&(vm->mem), 
            0x1000, 
            bios_mem_size);
    if (result < 0) {
        perror("POSIX_MEMALIGN");
        exit(1);
    }

    /*  set bios rom */
    kvm_mem *mem = (kvm_mem*)malloc(sizeof(kvm_mem));
    mem->slot = 1;
	mem->flags = 0;
	mem->guest_phys_addr = 0xfffe0000;
	mem->memory_size = (1024 * 128);
	mem->userspace_addr = (unsigned long)vm->mem;
    if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, mem)) {
        perror("SET_BIOS_MEMORY");
        exit(1);
    }

    // /* set bios ram */
    void *ram1;
    int ram1_result = posix_memalign(&(ram1),
                        0x1000,
                        bios_ram1_size);
    if (result < 0) {
        perror("ALLOC_RAM1_MEMORY");
        exit(1);
    }
    
    kvm_mem ram1mem;
    ram1mem.slot = 0;
    ram1mem.flags = 0;
    ram1mem.memory_size = bios_ram1_size;
    ram1mem.guest_phys_addr = bios_ram1_addr;
    ram1mem.userspace_addr = (uint64_t)ram1;

    if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &ram1mem)) {
        perror("SET_RAM1_MEM");
        exit(1);
    }

    void *ram2;
    int ram2_result = posix_memalign(&(ram2),
                        0x1000,
                        bios_ram2_size);
    if (result < 0) {
        perror("ALLOC_RAM1_MEMORY");
        exit(1);
    }
    
    kvm_mem ram2mem;
    ram2mem.slot = 0;
    ram2mem.flags = 0;
    ram2mem.memory_size = bios_ram2_size;
    ram2mem.guest_phys_addr = bios_ram2_addr;
    ram2mem.userspace_addr = (uint64_t)ram2;

    if (ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, ram2mem)) {
        perror("SET_RAM2_MEM");
        exit(1);
    }
}

int main(int argc, char **argv) {
    struct vm *vm = (struct vm*)malloc(sizeof(struct vm));
    struct vcpu *vcpu = (struct vcpu*)malloc(sizeof(struct vcpu));
    //struct kvm_userspace_memory_region *memreg = (struct kvm_userspace_memory_region*)malloc(sizeof(struct kvm_userspace_memory_region));
    struct blk *blk = (struct blk*)malloc(sizeof(struct blk));

    vm->vm_fd = open("/dev/kvm", O_RDWR);
    if (vm->vm_fd < 0) { 
        perror("open /dev/kvm/");
        exit(1);
    }

    vm->fd = ioctl(vm->vm_fd, KVM_CREATE_VM, 0);
    if (vm->fd < 0) {
        perror("KVM_CREATE_VM");
        exit(1);
    }

    set_irqchip(vm);

    if (ioctl(vm->fd, KVM_CREATE_PIT) < 0) {
        perror("KVM_CREATE_PIT");
        exit(1);
    }

    // if (ioctl(vm->fd, KVM_SET_TSS_ADDR, 0xfffbd000) < 0) {
    //     perror("KVM_SET_TSS_ADDR");
    //     exit(1);
    // }

    //set_user_memory_region(vm, memreg);
    alloc_bios_memory(vm);    

    //load_guest_binary(vm->mem);
    load_bios(vm->mem);

    init_vcpu(vm, vcpu);
    set_regs(vcpu);

    //set_blk(blk);

    blk->data = malloc(5120000);
    blk->data_reg = 0;
    blk->drive_head_reg = 0;
    blk->lba_high_reg = 0;
    blk->lba_middle_reg = 0;
    blk->lba_low_reg = 0;
    blk->sec_count_reg = 0;
    blk->status_command_reg = 0x40;

    int blk_img_fd = open("../xv6/xv6.img", O_RDONLY);
    if (blk_img_fd < 0) {
        perror("fail open xv6.img");
        exit(1);
    }

    int read_size = 0;
    void *tmp = (void*)blk->data;
    while(1) {
        read_size = read(blk_img_fd, tmp, 512000);
        if (read_size <= 0) break;
        tmp += read_size;
    }

     int i_in = 0;

    for (;;) {
        if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
            perror("KVM_RUN");
            exit(1);
        }

        if (vcpu->kvm_run->exit_reason == KVM_EXIT_IO) {
            int port = vcpu->kvm_run->io.port;
            uint32_t d = 0;

            if (vcpu->kvm_run->io.direction == KVM_EXIT_IO_OUT) {
                for (int i = 0; i < vcpu->kvm_run->io.count; i++) {
                    
                    printf("out: %u\n", port);
                    print_regs(vcpu);

                    char value = *(unsigned char *)((unsigned char *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset);
                    __u16 val = *(__u16 *)((__u16 *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset);
                    ioctl(vcpu->fd, KVM_GET_REGS, &(vcpu->regs));

                    if (port == 0x402) {
                        for (int i_out = 0; i_out < vcpu->kvm_run->io.count; i_out++) {
                            putchar(*(char *)((unsigned char *)vcpu->kvm_run->io.data_offset));
                            vcpu->kvm_run->io.data_offset += vcpu->kvm_run->io.size;
                        }
                        break;
                    }
                    //print_regs(vcpu);
                    handle_io_out(blk, port, value, val);
		        }
            } else {
                printf("in: %u\n", port);
                switch (port)
                {
                case 0x1F7:
                    *(unsigned char *)((unsigned char *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset) = blk->status_command_reg; 
                    break;
                case 0x1F0:
                    for (int i = 0; i < vcpu->kvm_run->io.count; ++i) {
                        i_in++;
                        for (int j = 0; j < 4; j++) {
                            //printf("blk index: %d\n", blk->index);
                            d |= blk->data[blk->index] << (8 * j);
                            blk->index += 1;
                        }
                        // 7f 0001111111
                        // 45 000000000001000101
                        //break;
                        // 46 4c 45 7f = 01000110010011000100010101111111 

                        *(uint32_t *)((unsigned char *)vcpu->kvm_run + vcpu->kvm_run->io.data_offset) = d;
                        vcpu->kvm_run->io.data_offset += vcpu->kvm_run->io.size;
                        d = 0;
                    }
                    
                    break;
                default:
                    break;
                }
            }
        } else if (vcpu->kvm_run->exit_reason == KVM_EXIT_HLT) {
            print_regs(vcpu);
            printf("HLT\n");
            exit(1);
        } else if (vcpu->kvm_run->exit_reason == KVM_EXIT_MMIO) {
            print_regs(vcpu);
            printf("mmio phys: 0x%llx\n", vcpu->kvm_run->mmio.phys_addr);
            if (vcpu->kvm_run->mmio.is_write) {
                printf("is write\n");
            }
            exit(1);
        } else {
            print_regs(vcpu);
            printf("exit reason: %d\n", vcpu->kvm_run->exit_reason);
            exit(1);
        }
    }

    return 1;   
}