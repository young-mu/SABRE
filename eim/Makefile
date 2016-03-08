# KERNELRELEASE is not defined
ifeq ($(KERNELRELEASE), )
	KDIR := /home/young/sabre/kernel/lib/modules/3.0.35/build
	PWD  := $(shell pwd)
modules :
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=arm-linux- modules
	@rm -f *.o *.mod.c modules.order Module.symvers
test :
	arm-linux-gcc -static -mcpu=cortex-a9 -o eim_test eim_test.c -std=gnu99
speed : 
	arm-linux-gcc -static -mcpu=cortex-a9 -o eim_speed eim_speed.c -std=gnu99
testcpp :
	arm-linux-g++ -c libeim.cpp -o libeim.o
	arm-linux-g++ -c eim_test.cpp -o eim_testcpp.o
	arm-linux-g++ -static -mcpu=cortex-a9 -o eim_testcpp libeim.o eim_testcpp.o
	@rm -f libeim.o eim_testcpp.o
clc :
	rm -f eim_test eim_speed eim_testcpp eim.ko
.PHONY : 
	modules test speed testcpp clc
# KERNELRELEASE is defined
else
	obj-m := eim.o
endif
