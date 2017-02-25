/*

**************************************************************************
**						STMicroelectronics						**
**************************************************************************
**						marco.cali@st.com				**
**************************************************************************
*																		*
*				FTS API for Flashing the IC			 *
*																		*
**************************************************************************
**************************************************************************

*/

#include "ftsSoftware.h"

/* Flash possible status */
#define FLASH_READY		0
#define FLASH_BUSY		1
#define FLASH_UNKNOWN		-1

#define FLASH_STATUS_BYTES	1

/* Flash timing parameters */
#define FLASH_RETRY_COUNT		1000
#define FLASH_WAIT_BEFORE_RETRY		 50	/* ms */

#define FLASH_WAIT_TIME			 200	 /* ms */

/* PATHS FW FILES */
/* #define PATH_FILE_FW			"fw.memh" */
#ifdef FTM3_CHIP
#define PATH_FILE_FW			"st_fts.bin"
#else
#define PATH_FILE_FW			"st_fts.ftb"		/* new bin file structure */
#endif

#ifndef FTM3_CHIP
#define FLASH_CHUNK			(64*1024)
#define DMA_CHUNK			32
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
int parseBinFile(const char *pathToFile, u8 **data, int *length, int dimension);
/* int parseMemhFile(const char* pathToFile, u8** data, int* length, int dimension); */
#else
int wait_for_flash_ready(u8 type);
int fts_warm_boot(void);
int parseBinFile(const char *pathToFile, Firmware *fw, int keep_cx);
int flash_erase_unlock(void);
int flash_full_erase(void);
int start_flash_dma(void);
int fillFlash(u32 address, u8 *data, int size);
#endif

int flash_unlock(void);
int fillMemory(u32 address, u8 *data, int size);
int getFirmwareVersion(u16 *fw_vers, u16 *config_id);
int readFwFile(const char *path, Firmware *fw, int keep_cx);
int flash_burn(Firmware fw, int force_burn);
int flashProcedure(const char *path, int force, int keep_cx);
