FLAGS  = -Wall -Wextra
CFLAGS += -nostdinc -nostdlib -fno-builtin -fno-common -c
LDFLAGS =  -s -x -T guest.ld

all: guest.h

%.h: %.bin
	xxd -i $^ > $@

%.bin: %.o
	ld $(LDFLAGS) -o $@ $+

%.o: %.s
	gcc $(CFLAGS) -o $@ $+

clean:
	rm -f *.o *.bin *.h *.map *~

.PHONY: clean