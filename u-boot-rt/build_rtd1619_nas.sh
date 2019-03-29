#!/bin/bash
#set -x

#rm -rf DVRBOOT_OUT

[ $# = 0 ] && target=spi || target=$1

CHIP_TYPE=0001
HWSETTING_DIR=examples/flash_writer_nv_A01/hw_setting/rtd161x/demo/$CHIP_TYPE
########### Build Thor A00 RTK #############
#BUILD_HWSETTING_LIST=`ls $HWSETTING_DIR`
#BUILD_HWSETTING_LIST=RTD161x_hwsetting_BOOT_2DDR4_8Gb_s1600 RTD161x_hwsetting_BOOT_2DDR4_8Gb_s2400 RTD161x_hwsetting_BOOT_2DDR4_8Gb_s2133 RTD161x_hwsetting_BOOT_2DDR4_8Gb_s2666 RTD161x_hwsetting_BOOT_LPDDR4_16Gb_s3200_H 
BUILD_HWSETTING_LIST=RTD161x_hwsetting_BOOT_2DDR4_8Gb_s2666

if [ $target = spi ]; then
	make mrproper; make rtd161x_spi_nas_rtk_defconfig;


	for hwsetting in $BUILD_HWSETTING_LIST
	do
		hwsetting=`echo $hwsetting | cut -d '.' -f 1`
		echo %%%%%%%% RTD1619 -- $CHIP_TYPE -- $hwsetting %%%%%%
		if [[ $hwsetting == *"NAND"* ]]; then
			echo "NAND hwsetting skip"
			continue
		fi
	
		#Build the normal version
		make Board_HWSETTING=$hwsetting CHIP_TYPE=$CHIP_TYPE
		rm -rf ./DVRBOOT_OUT/$target/hw_setting; mkdir -p ./DVRBOOT_OUT/$target/hw_setting 
		cp ./examples/flash_writer_nv_A01/hw_setting/out/${hwsetting}_final.bin ./DVRBOOT_OUT/$target/hw_setting/$CHIP_TYPE-$hwsetting-nas-$target.bin
		cp ./examples/flash_writer_nv_A01/dvrboot.exe.bin ./DVRBOOT_OUT/$target/A01-$hwsetting-nas-$target.bin
		cp ./examples/flash_writer_nv_A01/Bind/bind.bin ./DVRBOOT_OUT/$target/A01-Recovery-$hwsetting-nas-$target.bin
	done
fi



[ $# = 0 ] && target=emmc || target=$1
if [ $target = emmc ]; then
	make mrproper; make rtd161x_qa_nas_rtk_defconfig;

	for hwsetting in $BUILD_HWSETTING_LIST
	do
		hwsetting=`echo $hwsetting | cut -d '.' -f 1`
		echo %%%%%%%% RTD1619 -- $CHIP_TYPE -- $hwsetting %%%%%%
		if [[ $hwsetting == *"NAND"* ]]; then
			echo "NAND hwsetting skip"
			continue
		fi

		#Build the normal version
		make Board_HWSETTING=$hwsetting CHIP_TYPE=$CHIP_TYPE
		rm -rf ./DVRBOOT_OUT/$target/hw_setting; mkdir -p ./DVRBOOT_OUT/$target/hw_setting 
		cp ./examples/flash_writer_nv_A01/hw_setting/out/${hwsetting}_final.bin ./DVRBOOT_OUT/$target/hw_setting/$CHIP_TYPE-$hwsetting-nas-$target.bin
		cp ./examples/flash_writer_nv_A01/dvrboot.exe.bin ./DVRBOOT_OUT/$target/A01-$hwsetting-nas-$target.bin
		cp ./examples/flash_writer_nv_A01/Bind/uda_emmc.bind.bin ./DVRBOOT_OUT/$target/A01-Recovery-uda-$hwsetting-nas-$target.bin
		cp ./examples/flash_writer_nv_A01/Bind/boot_emmc.bind.bin ./DVRBOOT_OUT/$target/A01-Recovery-boot-$hwsetting-nas-$target.bin
	done
fi
