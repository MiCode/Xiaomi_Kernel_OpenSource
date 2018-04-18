/*

**************************************************************************
**                        STMicroelectronics							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*							FTS Core definitions						 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

/*!
* \file ftsCore.h
* \brief Contains all the definitions and structs of Core functionalities
*/

#ifndef FTS_CORE_H
#define FTS_CORE_H

#include "ftsHardware.h"
#include "ftsSoftware.h"
#include "../fts.h"


#define GPIO_NOT_DEFINED					-1

#define ADDR_SIZE_HW_REG					BITS_32

#define DATA_HEADER							4
#define LOCKDOWN_CODE_RETRY     2

/**
 * Type of CRC errors
 */
typedef enum {
	CRC_CODE = 1,
	CRC_CONFIG = 2,
	CRC_CX = 3,
	CRC_PANEL = 4
} CRC_Error;


/** @defgroup system_info	System Info
* System Info Data collect the most important informations about hw and fw
* @{
*/
#define SYS_INFO_SIZE						208
#define DIE_INFO_SIZE						16
#define EXTERNAL_RELEASE_INFO_SIZE			8
#define RELEASE_INFO_SIZE					EXTERNAL_RELEASE_INFO_SIZE+8
/** @}*/


#define RETRY_MAX_REQU_DATA					2
#define RETRY_SYSTEM_RESET					3


#define LOCKDOWN_LENGTH						384
#define LOCKDOWN_HEAD_LENGTH				4
#define LOCKDOWN_DATA_OFFSET				20
#define LOCKDOWN_SIGNATURE					0x5A
#define ADDR_LOCKDOWN						(u64)0x0000000000000000
#define LOCKDOWN_WRITEREAD_CMD				0xA6

/** @addtogroup system_info
* @{
*/

/**
 * Struct which contains fundamental informations about the chip and its configuration
 */
typedef struct {
	u16 u16_apiVer_rev;
	u8 u8_apiVer_minor;
	u8 u8_apiVer_major;
	u16 u16_chip0Ver;
	u16 u16_chip0Id;
	u16 u16_chip1Ver;
	u16 u16_chip1Id;
	u16 u16_fwVer;
	u16 u16_svn_ver;

	u16 u16_cfgVer;
	u16 u16_cfgId;
	u16 u16_cxVer;
	u16 u16_cxId;
	u8 u8_fwCfgAfeVer;
	u8 u8_fwCxAfeVer;
	u8 u8_panelCfgAfeVer;
	u8 u8_protocol;
	u8 u8_dieInfo[DIE_INFO_SIZE];
	u8 u8_releaseInfo[EXTERNAL_RELEASE_INFO_SIZE];
	u8 u8_fwcrc[4];
	u8 u8_fwcfg[4];






	u16 u16_scrResX;
	u16 u16_scrResY;
	u8 u8_scrTxLen;
	u8 u8_scrRxLen;
	u8 u8_keyLen;
	u8 u8_forceLen;

	u16 u16_dbgInfoAddr;

	u16 u16_msTchRawAddr;
	u16 u16_msTchFilterAddr;
	u16 u16_msTchStrenAddr;
	u16 u16_msTchBaselineAddr;

	u16 u16_ssTchTxRawAddr;
	u16 u16_ssTchTxFilterAddr;
	u16 u16_ssTchTxStrenAddr;
	u16 u16_ssTchTxBaselineAddr;

	u16 u16_ssTchRxRawAddr;
	u16 u16_ssTchRxFilterAddr;
	u16 u16_ssTchRxStrenAddr;
	u16 u16_ssTchRxBaselineAddr;

	u16 u16_keyRawAddr;
	u16 u16_keyFilterAddr;
	u16 u16_keyStrenAddr;
	u16 u16_keyBaselineAddr;

	u16 u16_frcRawAddr;
	u16 u16_frcFilterAddr;
	u16 u16_frcStrenAddr;
	u16 u16_frcBaselineAddr;

	u16 u16_ssHvrTxRawAddr;
	u16 u16_ssHvrTxFilterAddr;
	u16 u16_ssHvrTxStrenAddr;
	u16 u16_ssHvrTxBaselineAddr;

	u16 u16_ssHvrRxRawAddr;
	u16 u16_ssHvrRxFilterAddr;
	u16 u16_ssHvrRxStrenAddr;
	u16 u16_ssHvrRxBaselineAddr;

	u16 u16_ssPrxTxRawAddr;
	u16 u16_ssPrxTxFilterAddr;
	u16 u16_ssPrxTxStrenAddr;
	u16 u16_ssPrxTxBaselineAddr;

	u16 u16_ssPrxRxRawAddr;
	u16 u16_ssPrxRxFilterAddr;
	u16 u16_ssPrxRxStrenAddr;
	u16 u16_ssPrxRxBaselineAddr;
} SysInfo;

/** @}*/

int initCore(struct fts_ts_info *info);
void setResetGpio(int gpio);
int fts_system_reset(void);
int isSystemResettedUp(void);
int isSystemResettedDown(void);
void setSystemResetedUp(int val);
void setSystemResetedDown(int val);
int pollForEvent(int *event_to_search, int event_bytes, u8 *readData,
		 int time_to_wait);
int checkEcho(u8 *cmd, int size);
int setScanMode(u8 mode, u8 settings);
int setFeatures(u8 feat, u8 *settings, int size);
int defaultSysInfo(int i2cError);
int writeSysCmd(u8 sys_cmd, u8 *sett, int size);
int readSysInfo(int request);
int readConfig(u16 offset, u8 *outBuf, int len);
int fts_disableInterrupt(void);
int fts_disableInterruptNoSync(void);
int fts_enableInterrupt(void);
int fts_crc_check(void);
int requestSyncFrame(u8 type);
int fts_get_lockdown_info(u8 *lockData);
int writeLockDownInfo(u8 *data, int size, u8 lock_id);
int readLockDownInfo(u8 *lockData, u8 lock_id, int size);

#endif /* FTS_CORE_H */
