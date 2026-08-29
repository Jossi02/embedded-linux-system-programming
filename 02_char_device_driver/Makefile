obj-m := assign2.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

.PHONY: default clean

default: assign2-2
	$(MAKE) -C $(KDIR) M=$(PWD) modules

assign2-2: assign2-2.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -Wall -Wextra -Wpedantic -o $@ $<

clean:
	$(RM) assign2-2
	$(MAKE) -C $(KDIR) M=$(PWD) clean
