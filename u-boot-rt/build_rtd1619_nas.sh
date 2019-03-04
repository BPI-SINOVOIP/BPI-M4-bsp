#!/bin/bash


mkdir -p DVRBOOT_OUT/hw_setting

make mrproper; make rtd161x_spi_nas_rtk_defconfig;

########### Build Thor A00 RTK #############
CHIP_TYPE=0000
HWSETTING_DIR=examples/flash_writer_nv/hw_setting/rtd161x/demo/$CHIP_TYPE
BUILD_HWSETTING_LIST=`ls $HWSETTING_DIR`

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
	cp ./examples/flash_writer_nv/hw_setting/out/${hwsetting}_final.bin ./DVRBOOT_OUT/hw_setting/$CHIP_TYPE-$hwsetting-nas-spi.bin
	cp ./examples/flash_writer_nv/dvrboot.exe.bin ./DVRBOOT_OUT/A00-$hwsetting-nas-spi.bin
done


mkdir -p DVRBOOT_OUT/hw_setting

make mrproper; make rtd161x_qa_nas_rtk_defconfig;

########### Build Thor A00 RTK #############
CHIP_TYPE=0000
HWSETTING_DIR=examples/flash_writer_nv/hw_setting/rtd161x/demo/$CHIP_TYPE
BUILD_HWSETTING_LIST=`ls $HWSETTING_DIR`

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
	cp ./examples/flash_writer_nv/hw_setting/out/${hwsetting}_final.bin ./DVRBOOT_OUT/hw_setting/$CHIP_TYPE-$hwsetting-nas-emmc.bin
	cp ./examples/flash_writer_nv/dvrboot.exe.bin ./DVRBOOT_OUT/A00-$hwsetting-nas-emmc.bin
done
