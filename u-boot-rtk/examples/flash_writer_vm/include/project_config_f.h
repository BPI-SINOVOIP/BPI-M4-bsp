/************************************************************************
 *
 *  project_config.h
 *
 *  external parameters was included in this file
 * 
 *
 ************************************************************************/

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H
//********************************************************************
//Board Components
//********************************************************************
//flag                      value
//********************************************************************
#define Board_CPU_RTD1395
#define Board_Chip_Rev_1395
#define Board_HWSETTING_RTD1395_hwsetting_BOOT_2DDR4_8Gb_s2133
//********************************************************************
//Config
//********************************************************************
//flag                      value
//********************************************************************
#define Config_Encryption_FALSE
#define Config_NOR_BOOT_NAND_FALSE
#define Config_SPI_BOOT_FISRT_SPI
#define RTK_FLASH_SPI
//********************************************************************
//config password to allow input from UART when secure boot (maximum 255)
//********************************************************************
//flag                      value
//********************************************************************
//********************************************************************
//RSA key file & AES key file (under bin/image)
//********************************************************************
//flag                      value
//********************************************************************
#define Config_Secure_Key_TRUE
#define Config_VM_Enc_	 FALSE
#define Config_Secure_Improve_FALSE
#define Config_Secure_RSA_Key_File			"rsa_key_2048___pem"
#define Config_Secure_RSA_Key_FW_File			"rsa_key_2048___fw.pem"
#define Config_Secure_RSA_Key_TEE_File			"rsa_key_2048___tee.pem"
#define Config_Secure_AES_Key_File_Name			aes_128bit_key___bin
#define Config_Secure_AES_Key1_File_Name			aes_128bit_key_1___bin
#define Config_Secure_AES_Key2_File_Name			aes_128bit_key_2___bin
#define Config_Secure_AES_Key3_File_Name			aes_128bit_key_3___bin
//********************************************************************
//Config for Simulation Mode - only use BOOTCODE_UBOOT_TARGET_SIM, don't ignore RSA calculation
//********************************************************************
//flag                      value
//********************************************************************
#define Config_Uboot_Sim_Mode_FALSE
//********************************************************************
//Config for SecureBoot Loader & Secure OS
//********************************************************************
//flag                      value
//********************************************************************
#define Config_FSBL_TRUE
#define Config_FSBL_File_Name			fsbl-loader-00___00.bin.enc
#define Config_FSBL_VM_FALSE
#define Config_FSBL_VM_File_Name			fsbl-loader-vm-00___00.bin.enc
#define Config_FSBL_OS_TRUE
#define Config_FSBL_OS_File_Name			fsbl-os-00___00.bin.enc
#define Config_BL31_TRUE
#define Config_BL31_File_Name			bl31___bin.enc
#define Config_Boot1_Rescue_FALSE
//********************************************************************
//Boot parameters
//********************************************************************
//flag                      value
//********************************************************************
#define Param_MAC_hi			0x00112233
#define Param_MAC_lo			0x44550000
//********************************************************************
//Logo attribute
//********************************************************************
//flag                      value
//********************************************************************
#define Logo_Source_NULL
#define Logo_Type_NTSC
#define Logo_File_Name			"realtek_ntsc1.bmp"
//********************************************************************
//Rescue Linux attribute
//********************************************************************
//flag                      value
//********************************************************************
#define Rescue_Source_NULL
#define Rescue_File_Name			vmlinux.rescue.phoenix.bin
//********************************************************************
//user defined
//********************************************************************
//flag                      value
//********************************************************************
#define 		 CR_MEMORY_DUMP "1"
#define FPGA_BOOT_EMMC "1"

#endif //#ifndef EXTERN_PARAM_H	
