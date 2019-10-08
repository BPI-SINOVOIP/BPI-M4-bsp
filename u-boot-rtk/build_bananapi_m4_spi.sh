#!/bin/bash

TOP=`pwd`

CROSS_COMPILE=${TOP}/../toolchains/asdk64-4.9.3-a53-EL-3.10-g2.19-a64nt-150615/bin/aarch64-linux-

usage() {
	cat <<-EOT >&2
	Usage: $0 <dram size>
	
	e.g "$0 1" or "$0 2"
	EOT
}

if [ $# -lt 1 ]; then
        usage
        exit 1
fi

if [ ! -d DVRBOOT_OUT ]; then
	mkdir -p DVRBOOT_OUT
fi

dram_size=$1
chip_type=0001
hwsetting=RTD1395_hwsetting_BOOT_${dram_size}DDR4_8Gb_s2133

echo "hwsetting=$hwsetting"

make distclean
make rtd1395_spi_bananapi_defconfig CROSS_COMPILE=$CROSS_COMPILE
make Board_HWSETTING=$hwsetting CROSS_COMPILE=$CROSS_COMPILE
cp examples/flash_writer_vm/dvrboot.exe.bin DVRBOOT_OUT/spiloader-${dram_size}GB.bin
