#!/bin/bash
mkdir -p DVRBOOT_OUT/hw_setting

make rtd1395_bananapi_defconfig;
make

cp ./examples/flash_writer_vm/install_a/hw_setting.bin ./DVRBOOT_OUT/hw_setting/
cp ./examples/flash_writer_vm/Bind/emmc.bind.bin ./DVRBOOT_OUT/hw_setting/
cp ./examples/flash_writer_vm/dvrboot.exe.bin ./DVRBOOT_OUT/
