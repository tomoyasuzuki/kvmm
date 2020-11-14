.text
.globl _start
.code16
_start:
    xorw %ax, %ax
loop:
        mov     $'H',   %al
        out     %al,    $0x01
        mov     $'e',   %al
        out     %al,    $0x01
        mov     $'l',   %al
        out     %al,    $0x01
        mov     $'l',   %al
        out     %al,    $0x01
        mov     $'o',   %al
        out     %al,    $0x01
        mov     $' ',   %al
        out     %al,    $0x01
        mov     $'K',   %al
        out     %al,    $0x01
        mov     $'V',   %al
        out     %al,    $0x01
        mov     $'M',   %al
        out     %al,    $0x01
        mov     $'!',   %al
        out     %al,    $0x01
        hlt