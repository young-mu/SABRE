# KERNELRELEASE is not defined
ifeq ($(KERNELRELEASE), )
	KDIR := /home/young/sabre/kernel/lib/modules/3.0.35/build
	PWD := $(shell pwd)
   	COMPILER := arm-linux-
modules :
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=$(COMPILER) modules
	@rm -f *.o *.mod.c modules.order Module.symvers
test :
	arm-linux-gcc -static -mcpu=cortex-a9 -o fpp_test fpp_test.c -std=gnu99
clc :
	rm -f fpp.ko fpp_test 

# KERNELRELEASE is defined
else
	obj-m := fpp.o
endif
