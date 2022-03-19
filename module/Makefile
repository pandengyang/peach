PWD := $(shell pwd)

obj-m += peach.o
peach-objs := peach_intel.o vmexit_handler.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
