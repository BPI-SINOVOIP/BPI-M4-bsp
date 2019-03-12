PWD=$(shell pwd)
ifndef TOP
	TOP = ../..
endif
include $(TOP)/make.include

all: install

install:
	$(MAKE) -C driver/src/devicedrv/ KDIR=$(PWD)/$(LINUX_KERNEL_PATH)
	$(CROSS_COMPILE)strip $(PWD)/driver/src/devicedrv/mali/mali.ko --strip-unneeded

clean:
	$(MAKE) -C driver/src/devicedrv/ KDIR=$(PWD)/$(LINUX_KERNEL_PATH) clean
