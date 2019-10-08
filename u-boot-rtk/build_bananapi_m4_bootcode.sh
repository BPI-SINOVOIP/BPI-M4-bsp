#!/bin/bash

TOP=`pwd`

CROSS_COMPILE=${TOP}/../toolchains/asdk64-4.9.3-a53-EL-3.10-g2.19-a64nt-150615/bin/aarch64-linux-

if [ ! -d DVRBOOT_OUT ]; then
	mkdir -p DVRBOOT_OUT
fi

make rtd1395_bananapi_defconfig CROSS_COMPILE=$CROSS_COMPILE
make CROSS_COMPILE=$CROSS_COMPILE

# emmc bootup
cp ./examples/flash_writer_vm/install_a/hw_setting.bin ./DVRBOOT_OUT/hw_setting.bin
cp ./examples/flash_writer_vm/Bind/emmc.bind.bin ./DVRBOOT_OUT/recovery.bin
cp ./examples/flash_writer_vm/dvrboot.exe.bin ./DVRBOOT_OUT/bootloader.bin
