ifneq ($(CC), gcc)
CC := gcc
endif

ifneq ($(KERNELRELEASE),)
obj-m := hello.o
else
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
clean:
	@echo "Cleaning..."
	@rm -rf .*.cmd *.o *.mod.c *.ko .tmp_versions *.order *.symvers
