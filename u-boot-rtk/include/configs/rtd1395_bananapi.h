/*
 * Configuration settings for the Realtek 1395 bananapi board.
 *
 * Won't include this file.
 * Just type "make <board_name>_config" and will be included in source tree.
 */

#ifndef __CONFIG_RTK_RTD1395_BANANAPI_H
#define __CONFIG_RTK_RTD1395_BANANAPI_H

/*
 * Include the common settings of RTD1395 platform.
 */
#include <configs/rtd1395_common.h>

/* ========================================================== */
/* Flash type is SPI or NAND or eMMC*/
#define CONFIG_SYS_RTK_EMMC_FLASH

#if defined(CONFIG_SYS_RTK_EMMC_FLASH)
	/* Flash writer setting:
	*   The corresponding setting will be located at
	*   uboot/examples/flash_writer_u/$(CONFIG_FLASH_WRITER_SETTING).inc*/
	#define CONFIG_FLASH_WRITER_SETTING            "1395_force_bananapi"
	#define CONFIG_CHIP_ID            			   "rtd1395"
	#define CONFIG_CUSTOMER_ID            		   "bananapi"
	#define CONFIG_CHIP_TYPE            		   "0001"
	#define CONFIG_TEE_OS_DRM					   "FALSE"

	/* Factory start address and size in eMMC */
	#define CONFIG_FACTORY_START	0x220000	/* For eMMC */
	#define CONFIG_FACTORY_SIZE	    0x400000	/* For eMMC */
	#define CONFIG_FW_TABLE_SIZE    0x8000		/* For eMMC */

	#ifndef CONFIG_SYS_PANEL_PARAMETER
		#define CONFIG_SYS_PANEL_PARAMETER
	#endif

	/* MMC */
	#define CONFIG_MMC
	#ifndef CONFIG_PARTITIONS
		#define CONFIG_PARTITIONS
	#endif
	#define CONFIG_DOS_PARTITION
	#define CONFIG_GENERIC_MMC
	#define CONFIG_RTK_MMC
	#define CONFIG_CMD_MMC
	#define USE_SIMPLIFY_READ_WRITE
	#define CONFIG_SHA256

	/* ENV */
	#undef	CONFIG_ENV_IS_NOWHERE
	#ifdef CONFIG_SYS_FACTORY
		#define CONFIG_ENV_IS_IN_FACTORY
	#endif

	#ifdef CONFIG_RESCUE_FROM_USB
		#undef CONFIG_RESCUE_FROM_USB_VMLINUX
		#undef CONFIG_RESCUE_FROM_USB_DTB
		#undef CONFIG_RESCUE_FROM_USB_ROOTFS
		#define CONFIG_RESCUE_FROM_USB_VMLINUX			"emmc.uImage"
		#define CONFIG_RESCUE_FROM_USB_DTB				"rescue.emmc.dtb"
		#define CONFIG_RESCUE_FROM_USB_ROOTFS			"rescue.root.emmc.cpio.gz_pad.img"
	#endif /* CONFIG_RESCUE_FROM_USB */
#endif

/* EMMC boot and GPT format */
#define CONFIG_RTK_EMMC_TRADITIONAL_MODE

/* SPI FLASH */
#define CONFIG_RTKSPI
#define CONFIG_CMD_RTKSPI

#define CONFIG_CMD_GPT
#define CONFIG_PARTITION_UUIDS
#define CONFIG_CMD_RTKGPT
#define CONFIG_CMD_RTKFDT


/* SD FLASH */
/*
#define CONFIG_RTK_SD
*/
#define CONFIG_SYS_RTK_SD_FLASH
#ifndef CONFIG_RTK_SD_DRIVER
#define CONFIG_RTK_SD_DRIVER
#endif
/* SD */
#ifdef CONFIG_RTK_SD_DRIVER
	#define CONFIG_SD
	//#define CONFIG_SD30
	#ifndef CONFIG_PARTITIONS
		#define CONFIG_PARTITIONS
	#endif
	#define CONFIG_DOS_PARTITION
	#define CONFIG_RTK_SD
	#define CONFIG_CMD_SD
	#define USE_SIMPLIFY_READ_WRITE
	#define CONFIG_SHA256
#endif



/* Boot Revision */
#define CONFIG_COMPANY_ID 		"0000"
#define CONFIG_BOARD_ID         "0705"
#define CONFIG_VERSION          "0000"

/*
 * SDRAM Memory Map
 * Even though we use two CS all the memory
 * is mapped to one contiguous block
 */
#undef CONFIG_NR_DRAM_BANKS
#undef CONFIG_SYS_SDRAM_BASE
#undef CONFIG_SYS_RAM_DCU1_SIZE

#define ARM_ROMCODE_SIZE			(188*1024)
#define CONFIG_NR_DRAM_BANKS		1
#define CONFIG_SYS_SDRAM_BASE		0x0
#define CONFIG_SYS_RAM_DCU1_SIZE	0x20000000
/* #define CONFIG_FT_RESCUE */

#undef  V_NS16550_CLK
#define V_NS16550_CLK			27000000

#define COUNTER_FREQUENCY               27000000 // FIXME, need to know what impact it will cause

/* Bootcode Feature: Rescue linux read from USB */
/* #define CONFIG_RESCUE_FROM_USB */
#ifdef CONFIG_RESCUE_FROM_USB
	#define CONFIG_RESCUE_FROM_USB_VMLINUX		"/bananapi/bpi-m4/linux/uImage"
	#define CONFIG_RESCUE_FROM_USB_DTB		    "/bananapi/bpi-m4/linux/rtd-1395-bananapi-m4.dtb"
	#define CONFIG_RESCUE_FROM_USB_ROOTFS		"/bananapi/bpi-m4/linux/rescue.root.emmc.cpio.gz_pad.img"
	#define CONFIG_RESCUE_FROM_USB_AUDIO_CORE	"/bananapi/bpi-m4/linux/bluecore.audio"
#endif /* CONFIG_RESCUE_FROM_USB */

#define CONFIG_CMD_ECHO
#define CONFIG_CMD_RUN
#define CONFIG_CMD_IMPORTENV
#define CONFIG_CMD_EXPORTENV

/*The partition format and file system of ext4*/
#define CONFIG_EFI_PARTITION
#define CONFIG_CMD_GPT
#define CONFIG_PARTITION_UUIDS
#define CONFIG_FS_EXT4
#define CONFIG_CMD_EXT4

#define CONFIG_VERSION			"0000"

#undef CONFIG_SYS_TEXT_BASE
#define CONFIG_SYS_TEXT_BASE		0x00030000

#define CONFIG_HUSH_PARSER

#undef CONFIG_SYS_MAXARGS
#define CONFIG_SYS_MAXARGS		32

#undef CONFIG_BOOTDELAY
#define CONFIG_BOOTDELAY	3

#undef CONFIG_SYS_PROMPT
#define CONFIG_SYS_PROMPT       		"BPI-M4> "

#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND \
	"run boot_normal;" \
	"if test $? -ne 0; then "			\
		"run set_sdbootargs && go all;"		\
	"fi;"						\
	"if test $? -ne 0; then "			\
		"run set_emmcbootargs && bootr; "	\
	"fi;"

#undef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS                   \
   "bpiver=1\0" \
   "bpi=bananapi\0" \
   "board=bpi-m4\0" \
   "chip=RTD1395\0" \
   "service=linux\0" \
   "scriptaddr=0x1500000\0" \
   "device=sd\0" \
   "partition=0:1\0" \
   "kernel=uImage\0" \
   "root=/dev/mmcblk0p2\0" \
   "debug=7\0" \
   "bootenv=uEnv.txt\0" \
   "checksd=fatinfo ${device} 0:1\0" \
   "loadbootenv=fatload ${device} ${partition} ${scriptaddr} ${bpi}/${board}/${service}/${bootenv} || fatload ${device} ${partition} ${scriptaddr} ${bootenv}\0" \
   "boot_normal=if run checksd; then echo Boot from ${device} ; setenv partition 0:1; else echo Boot from USB ; usb start ; setenv device usb ; setenv partition 0:1 ; fi; if run loadbootenv; then echo Loaded environment from ${bootenv}; env import -t ${scriptaddr} ${filesize}; fi; run uenvcmd; fatload mmc 0:1 ${loadaddr} ${bpi}/${board}/${service}/${kernel}; bootr\0" \
   "boot_user=echo Boot from USB ; usb start ; setenv device usb ; setenv partition 0:1 ; fi; if run loadbootenv; then echo Loaded environment from ${bootenv}; env import -t ${scriptaddr} ${filesize}; fi; run usercmd; fatload mmc 0:1 ${loadaddr} ${bpi}/${board}/${service}/${kernel}; bootr\0" \
   "bootmenu_delay=30\0" \
   "initrd_high=0xffffffffffffffff\0"				\
   "console_args=earlycon=uart8250,mmio32,0x98007800 fbcon=map:0 console=ttyS0,115200 loglevel=7 cma=32m@576m\0" \
   "sdroot_args=board=bpi-m4 rootwait root=/dev/mmcblk0p2 rw\0" \
   "set_sdbootargs=setenv bootargs ${console_args} ${sdroot_args}\0" \
   "emmcroot_args=root=/dev/mmcblk0p2 rootwait\0" \
   "set_emmcbootargs=setenv bootargs ${console_args} ${emmcroot_args}\0" \
   "bootargs=earlycon=uart8250,mmio32,0x98007800 board=bpi-m4 console=tty1 console=ttyS0,115200 loglevel=7 cma=32m@576m rootwait root=/dev/mmcblk0p2 rw \0" \
   "RTK_ARM64=y\0"                  \
   "kernel_loadaddr=0x03000000\0"                  \
   "fdt_loadaddr=0x02100000\0"                  \
   "fdt_high=0xffffffffffffffff\0"                  \
   "rootfs_loadaddr=0x02200000\0"                   \
   "initrd_high=0xffffffffffffffff\0"				\
   "rescue_rootfs_loadaddr=0x02200000\0"                   \
   "audio_loadaddr=0x0f900000\0"                 \
   "dtbo_loadaddr=0x26400000\0"                 \
   "blue_logo_loadaddr="STR(CONFIG_BLUE_LOGO_LOADADDR)"\0"      \
   "mtd_part=mtdparts=rtk_nand:\0"                  \
   "eth_drv_para=phy,bypass,noacp\0"                  \
   
/* define 1395 USB GPIO control */
#undef USB_PORT0_GPIO_TYPE
#undef USB_PORT0_GPIO_NUM
#undef USB_PORT0_TYPE_C_SWITCH_GPIO_TYPE
#undef USB_PORT0_TYPE_C_SWITCH_GPIO_NUM
#undef USB_PORT1_GPIO_TYPE
#undef USB_PORT1_GPIO_NUM
#undef USB_PORT2_GPIO_TYPE
#undef USB_PORT2_GPIO_NUM
/* Port 0, DRD, TYPE_C */
#define USB_PORT0_GPIO_TYPE "NONE"
#define USB_PORT0_GPIO_NUM 0
#define USB_PORT0_TYPE_C_SWITCH_GPIO_TYPE "NONE"
#define USB_PORT0_TYPE_C_SWITCH_GPIO_NUM 0
/* Port 1, xhci u2 host */
#define USB_PORT1_GPIO_TYPE "NONE"
#define USB_PORT1_GPIO_NUM  0
/* Port 2, xhci u2 host */
#define USB_PORT2_GPIO_TYPE "NONE"
#define USB_PORT2_GPIO_NUM  0

/* Boot and Install gpio */
#define CONFIG_REALTEK_GPIO
/* #define CONFIG_INSTALL_GPIO_NUM    		52 */
#define CONFIG_BOOT_GPIO_NUM    		49

/* #define CONFIG_ACPU_LOGBUF_ENABLE */
#ifdef CONFIG_ACPU_LOGBUF_ENABLE
#define CONFIG_ACPU_LOGBUF_ADDR                0x0FE00000
#define CONFIG_ACPU_LOGBUF_SIZE                0x00002000
#endif

#endif /* __CONFIG_RTK_RTD1395_BANANAPI_H */
