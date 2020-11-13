.file   "guest.s"
.data
.bss
.text
.globl _start
_start:
        movw %ax, 0x400
        out %ax, $0x10
        hlt
        ret