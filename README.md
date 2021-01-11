# kvmm

kvmm is a type2 hypervisor that uses the Linux Kernel Virtual Machine(KVM).  
The goal of kvmm is to run xv6.  

## features
- [x] execution of basic instructions
- [x] LAPIC/IOAPIC emulation (partially)
- [x] disk emulation (partially)
- [x] uart emulation (partially)
- [x] ide interrupts (partially)
- [x] uart interrupts
- [ ] keyboard interrupts
- [ ] timer
- [ ] multiproccessor support 

## references
- [dpw/kvm-hello-world](https://github.com/dpw/kvm-hello-world)
- [kvmtool/kvmtool](https://github.com/kvmtool/kvmtool)
- [ykskb/dax86](https://github.com/ykskb/dax86)
