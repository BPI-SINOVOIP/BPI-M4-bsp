#!/bin/bash

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

dram_size=$1
chip_type=0001
hwsetting=RTD1395_hwsetting_BOOT_${dram_size}DDR4_8Gb_s2133

echo "hwsetting=$hwsetting"

make clean
make rtd1395_spi_bananapi_defconfig
make Board_HWSETTING=$hwsetting
cp examples/flash_writer_vm/dvrboot.exe.bin DVRBOOT_OUT/spiloader-${dram_size}GB.bin
