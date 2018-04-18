/*

**************************************************************************
**                        STMicroelectronics							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*               	FTS API for Flashing the IC							 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

/*!
* \file ftsFlash.h
* \brief Contains all the definitions and structs to handle the FW update process
*/

#ifndef FTS_FLASH_H
#define FTS_FLASH_H

#include "ftsSoftware.h"


#define FLASH_READY						0
#define FLASH_BUSY						1
#define FLASH_UNKNOWN					-1

#define FLASH_STATUS_BYTES				1


#define FLASH_RETRY_COUNT				200
#define FLASH_WAIT_BEFORE_RETRY         50

#ifdef FW_H_FILE
#define PATH_FILE_FW			"NULL"
#else
#define PATH_FILE_FW			"st_fts_v521.ftb"
#endif

#define FLASH_CHUNK			64*1024
#define DMA_CHUNK			32

/**
 * Define which kind of erase page by page should be performed
 */
typedef enum {
	ERASE_ALL = 0,
	SKIP_PANEL_INIT = 1,
	SKIP_PANEL_CX_INIT = 2
} ErasePage;

/** @addtogroup fw_file
 * @{
 */

/**
* Struct which contains information and data of the FW that should be burnt into the IC
*/
typedef struct {
	u8 *data;
	u16 fw_ver;
	u16 config_id;
	u16 cx_ver;
	u8 externalRelease[EXTERNAL_RELEASE_INFO_SIZE];
	int data_size;
	u32 sec0_size;
	u32 sec1_size;
	u32 sec2_size;
	u32 sec3_size;

} Firmware;

/** @}*/

/** @addtogroup flash_command
 * @{
 */

int wait_for_flash_ready(u8 type);
int hold_m3(void);
int flash_erase_unlock(void);
int flash_full_erase(void);
int flash_erase_page_by_page(ErasePage keep_cx);
int start_flash_dma(void);
int fillFlash(u32 address, u8 *data, int size);

int flash_unlock(void);
int getFirmwareVersion(u16 *fw_vers, u16 *config_id);
int getFWdata(const char *pathToFile, u8 **data, int *size);
int parseBinFile(u8 *fw_data, int fw_size, Firmware *fw, int keep_cx);
int readFwFile(const char *path, Firmware *fw, int keep_cx);
int flash_burn(Firmware fw, int force_burn, int keep_cx);
int flashProcedure(const char *path, int force, int keep_cx);

#endif

/** @}*/
