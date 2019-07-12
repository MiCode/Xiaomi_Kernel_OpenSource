/*

**************************************************************************
**                        STMicroelectronics		                **
**************************************************************************
**                        marco.cali@st.com				**
**************************************************************************
*                                                                        *
*			FTS API for Flashing the IC			 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

#include "ftsSoftware.h"

#define FLASH_READY				0
#define FLASH_BUSY				1
#define FLASH_UNKNOWN		   -1

#define FLASH_STATUS_BYTES		1



#define FLASH_RETRY_COUNT		1000
#define FLASH_WAIT_BEFORE_RETRY         50
#define FLASH_WAIT_TIME                 200

#ifdef FTM3_CHIP
#define PATH_FILE_FW			"st_fts.bin"
#else
#define PATH_FILE_FW			"st_fts_biel_black.ftb"
#endif

#ifndef FTM3_CHIP
#define FLASH_CHUNK			(64 * 1024)
#define DMA_CHUNK			512
#endif


typedef struct {
	u8 *data;
	u16 fw_ver;
	u16 config_id;
	u8 externalRelease[EXTERNAL_RELEASE_INFO_SIZE];
	int data_size;
#ifndef FTM3_CHIP
	u32 sec0_size;
	u32 sec1_size;
	u32 sec2_size;
	u32 sec3_size;
#endif
} Firmware;

#ifdef FTM3_CHIP
int flash_status(void);
int flash_status_ready(void);
int wait_for_flash_ready(void);
#else
int wait_for_flash_ready(u8 type);
int fts_warm_boot(void);
int flash_erase_unlock(void);
int flash_full_erase(void);
int flash_erase_page_by_page(int keep_cx);
int start_flash_dma(void);
int fillFlash(u32 address, u8 *data, int size);
#endif

int flash_unlock(void);
int fillMemory(u32 address, u8 *data, int size);
int getFirmwareVersion(u16 *fw_vers, u16 *config_id);
int getFWdata(const char *pathToFile, u8 **data, int *size, int from);
int parseBinFile(u8 *fw_data, int fw_size, Firmware *fw, int keep_cx);
int readFwFile(const char *path, Firmware *fw, int keep_cx);
int flash_burn(Firmware fw, int force_burn, int keep_cx);
int flashProcedure(const char *path, int force, int keep_cx);
