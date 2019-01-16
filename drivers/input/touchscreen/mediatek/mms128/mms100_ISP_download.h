//--------------------------------------------------------
//
//
//	Melfas MMS100 Series Download base v1.7 2011.09.23
//
//
//--------------------------------------------------------


#ifndef __MELFAS_FIRMWARE_DOWNLOAD_H__
#define __MELFAS_FIRMWARE_DOWNLOAD_H__

//=====================================================================
//
//   MELFAS Firmware download pharameters
//
//=====================================================================

#define MELFAS_TRANSFER_LENGTH					(32/8)		// Fixed value
#define MELFAS_FIRMWARE_MAX_SIZE				(31*1024)   // Fixed value

#define MELFAS_ISP_DOWNLOAD                         0       // 0: ISC mode   1:ISP mode   
#define MELFAS_2CHIP_DOWNLOAD_ENABLE                0       // 0 : 1Chip Download, 1: 2Chip Download

#define MELFAS_DOWNLAOD_CORE_VERSION            0x01
#define MELFAS_DOWNLAOD_PRIVATE_VERSION         0x01
#define MELFAS_DOWNLAOD_PUBLIC_VERSION          0x01

//----------------------------------------------------
// ISC download pharameters
//----------------------------------------------------
#define ISC_MODE_SLAVE_ADDRESS                  0x48

#define MELFAS_CORE_FIRWMARE_UPDATE_ENABLE           1      // 0 : disable, 1: enable
#define MELFAS_PRIVATE_CONFIGURATION_UPDATE_ENABLE   0       // 0 : disable, 1: enable
#define MELFAS_PUBLIC_CONFIGURATION_UPDATE_ENABLE    0       // 0 : disable, 1: enable

//----------------------------------------------------
//   ISP Mode
//----------------------------------------------------
#define ISP_MODE_ERASE_FLASH					0x01
#define ISP_MODE_SERIAL_WRITE					0x02
#define ISP_MODE_SERIAL_READ					0x03
#define ISP_MODE_NEXT_CHIP_BYPASS				0x04


//----------------------------------------------------
//   ISC Mode
//----------------------------------------------------
#define ISC_READ_DOWNLOAD_POSITION              1          //0 : USE ISC_PRIVATE_CONFIG_FLASH_START 1: READ FROM RMI MAP(0x61,0x62)
#define ISC_PRIVATE_CONFIG_FLASH_START          26
#define ISC_PUBLIC_CONFIG_FLASH_START           28
#define ISC_DEFAULT_SLAVE_ADDR                  0x48
// mode
#define ISC_CORE_FIRMWARE_DL_MODE               0x01
#define ISC_PRIVATE_CONFIGURATION_DL_MODE       0x02
#define ISC_PUBLIC_CONFIGURATION_DL_MODE        0x03
#define ISC_SLAVE_DOWNLOAD_START                0x04

//----------------------------------------------------
//   Register Information
//----------------------------------------------------

#define MELFAS_FIRMWARE_VER_REG_CORE  			0xF3 //CORE F/W Version
#define MELFAS_FIRMWARE_VER_REG_PRIVATE_CUSTOM  0xF4 //PRIVATE_CUSTOM F/W Version
#define MELFAS_FIRMWARE_VER_REG_PUBLIC_CUSTOM	0xF5 //PUBLIC CUSTOM F/W version	

#define ISC_DOWNLOAD_MODE_ENTER                 0x5F
#define ISC_DOWNLOAD_MODE                       0x60
#define ISC_PRIVATE_CONFIGURATION_START_ADDR    0x61
#define ISC_PUBLIC_CONFIGURATION_START_ADDR     0x62

#define ISC_READ_SLAVE_CRC_OK                   0x63        // return value from slave


//----------------------------------------------------
//   Return values of download function
//----------------------------------------------------
#define MCSDL_RET_SUCCESS						0x00
#define MCSDL_RET_ERASE_FLASH_VERIFY_FAILED		0x01
#define MCSDL_RET_PROGRAM_VERIFY_FAILED			0x02
#define MCSDL_FIRMWARE_UPDATE_MODE_ENTER_FAILED	0x03
#define MCSDL_FIRMWARE_UPDATE_FAILED            0x04
#define MCSDL_LEAVE_FIRMWARE_UPDATE_MODE_FAILED 0x05

#define MCSDL_RET_PROGRAM_SIZE_IS_WRONG			0x10
#define MCSDL_RET_VERIFY_SIZE_IS_WRONG			0x11
#define MCSDL_RET_WRONG_BINARY					0x12

#define MCSDL_RET_READING_HEXFILE_FAILED		0x21
#define MCSDL_RET_FILE_ACCESS_FAILED			0x22
#define MCSDL_RET_MELLOC_FAILED					0x23

#define MCSDL_RET_ISC_SLAVE_CRC_CHECK_FAILED    0x30
#define MCSDL_RET_ISC_SLAVE_DOWNLOAD_TIME_OVER  0x31

#define MCSDL_RET_WRONG_MODULE_REVISION			0x40


//----------------------------------------------------
//	When you can't control VDD nor CE.
//	Set this value 1
//	Then Melfas Chip can prepare chip reset.
//----------------------------------------------------

#define MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD 	0							// If 'enable download command' is needed ( Pinmap dependent option ).

//============================================================
//
//	Port setting. ( Melfas preset this value. )
//
//============================================================

// If want to set Enable : Set to 1

#define MCSDL_USE_CE_CONTROL						0
#define MCSDL_USE_VDD_CONTROL						1
#define MCSDL_USE_RESETB_CONTROL                    1



//============================================================
//
//	Porting factors for Baseband
//
//============================================================

#include "mms100_download_porting.h"


//----------------------------------------------------
//	Functions
//----------------------------------------------------

int mms100_ISP_download_binary_data(int dl_mode);			// with binary type .c   file.
int mms100_ISP_download_binary_file(void);			// with binary type .bin file.

int mms100_ISC_download_binary_data(void);
int mms100_ISC_download_binary_file(void);

//---------------------------------
//	Delay functions
//---------------------------------
void mcsdl_delay(UINT32 nCount);



#if MELFAS_ENABLE_DELAY_TEST					// For initial porting test.
void mcsdl_delay_test(INT32 nCount);
#endif


#endif		//#ifndef __MELFAS_FIRMWARE_DOWNLOAD_H__

