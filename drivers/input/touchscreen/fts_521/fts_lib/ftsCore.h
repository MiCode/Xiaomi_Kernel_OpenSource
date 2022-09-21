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

/*HW DATA*/
#define GPIO_NOT_DEFINED					-1									/*value assumed by reset_gpio when the reset pin of the IC is not connected*/

#define ADDR_SIZE_HW_REG					BITS_32							/*value of AddrSize for Hw register in FTI @see AddrSize*/

#define DATA_HEADER							4								/*size in byte of the header loaded with the data in the frambuffer*/
#define LOCKDOWN_CODE_RETRY  				2
/**
 * Type of CRC errors
 */
typedef enum {
	CRC_CODE = 1,																/*CRC in the code section*/
	CRC_CONFIG = 2,															/*CRC in the config section*/
	CRC_CX = 3,																/*CRC in the cx section*/
	CRC_PANEL = 4																/*CRC in the panel section*/
} CRC_Error;

/*CHIP INFO*/
/** @defgroup system_info	System Info
* System Info Data collect the most important informations about hw and fw
* @{
*/
#define SYS_INFO_SIZE						208									/*Size in bytes of System Info data*/
#define DIE_INFO_SIZE						16									/*num bytes of die info*/
#define EXTERNAL_RELEASE_INFO_SIZE			8									/*num bytes of external release in config*/
#define RELEASE_INFO_SIZE					(EXTERNAL_RELEASE_INFO_SIZE + 8)		/*num bytes of release info in sys info (first bytes are external release)*/
/** @}*/

/*RETRY MECHANISM*/
#define RETRY_MAX_REQU_DATA					2								/*Max number of attemps performed when requesting data*/
#define RETRY_SYSTEM_RESET					3									/*Max number of attemps performed to reset the IC*/

/*LOCKDOWN INFO*/
#define LOCKDOWN_LENGTH						384
#define LOCKDOWN_HEAD_LENGTH				4
#define LOCKDOWN_DATA_OFFSET				20
#define LOCKDOWN_SIGNATURE					0x5A
#define ADDR_LOCKDOWN						((u64)0x0000000000000000)
#define LOCKDOWN_WRITEREAD_CMD				0xA6

/** @addtogroup system_info
* @{
*/

/**
 * Struct which contains fundamental informations about the chip and its configuration
 */
typedef struct {
	u16 u16_apiVer_rev;														/*API revision version*/
	u8 u8_apiVer_minor;														/*API minor version*/
	u8 u8_apiVer_major;														/*API major version*/
	u16 u16_chip0Ver;															/*Dev0 version*/
	u16 u16_chip0Id;															/*Dev0 ID*/
	u16 u16_chip1Ver;															/*Dev1 version*/
	u16 u16_chip1Id;															/*Dev1 ID*/
	u16 u16_fwVer;																/*Fw version*/
	u16 u16_svnRev;															/*SVN Revision*/
	u16 u16_cfgVer;															/*Config Version*/
	u16 u16_cfgProgectId;														/*Config Project ID*/
	u16 u16_cxVer;																/*Cx Version*/
	u16 u16_cxProjectId;														/*Cx Project ID*/
	u8 u8_cfgAfeVer;															/*AFE version in Config*/
	u8 u8_cxAfeVer;															/*AFE version in CX*/
	u8 u8_panelCfgAfeVer;														/*AFE version in PanelMem*/
	u8 u8_protocol;															/*Touch Report Protocol*/
	u8 u8_dieInfo[DIE_INFO_SIZE];												/*Die information*/
	u8 u8_releaseInfo[RELEASE_INFO_SIZE];										/*Release information*/
	u32 u32_fwCrc;																/*Crc of FW*/
	u32 u32_cfgCrc;															/*Crc of config*/

	u16 u16_scrResX;															/*X resolution on main screen*/
	u16 u16_scrResY;															/*Y resolution on main screen*/
	u8 u8_scrTxLen;															/*Tx length*/
	u8 u8_scrRxLen;															/*Rx length*/
	u8 u8_keyLen;																/*Key Len*/
	u8 u8_forceLen;															/*Force Len*/

	u16 u16_dbgInfoAddr;														/*Offset of debug Info structure*/

	u16 u16_msTchRawAddr;														/*Offset of MS touch raw frame*/
    u16 u16_msTchFilterAddr;													/*Offset of MS touch filter frame*/
    u16 u16_msTchStrenAddr;													/*Offset of MS touch strength frame*/
    u16 u16_msTchBaselineAddr;													/*Offset of MS touch baseline frame*/

    u16 u16_ssTchTxRawAddr;													/*Offset of SS touch force raw frame*/
    u16 u16_ssTchTxFilterAddr;													/*Offset of SS touch force filter frame*/
    u16 u16_ssTchTxStrenAddr;													/*Offset of SS touch force strength frame*/
    u16 u16_ssTchTxBaselineAddr;												/*Offset of SS touch force baseline frame*/

    u16 u16_ssTchRxRawAddr;													/*Offset of SS touch sense raw frame*/
    u16 u16_ssTchRxFilterAddr;													/*Offset of SS touch sense filter frame*/
    u16 u16_ssTchRxStrenAddr;													/*Offset of SS touch sense strength frame*/
    u16 u16_ssTchRxBaselineAddr;												/*Offset of SS touch sense baseline frame*/

    u16 u16_keyRawAddr;														/*Offset of key raw frame*/
    u16 u16_keyFilterAddr;														/*Offset of key filter frame*/
    u16 u16_keyStrenAddr;														/*Offset of key strength frame*/
    u16 u16_keyBaselineAddr;													/*Offset of key baseline frame*/

    u16 u16_frcRawAddr;														/*Offset of force touch raw frame*/
    u16 u16_frcFilterAddr;														/*Offset of force touch filter frame*/
    u16 u16_frcStrenAddr;														/*Offset of force touch strength frame*/
    u16 u16_frcBaselineAddr;													/*Offset of force touch baseline frame*/

    u16 u16_ssHvrTxRawAddr;													/*Offset of SS hover Force raw frame*/
    u16 u16_ssHvrTxFilterAddr;													/*Offset of SS hover Force filter frame*/
    u16 u16_ssHvrTxStrenAddr;													/*Offset of SS hover Force strength frame*/
    u16 u16_ssHvrTxBaselineAddr;												/*Offset of SS hover Force baseline frame*/

    u16 u16_ssHvrRxRawAddr;													/*Offset of SS hover Sense raw frame*/
    u16 u16_ssHvrRxFilterAddr;													/*Offset of SS hover Sense filter frame*/
    u16 u16_ssHvrRxStrenAddr;													/*Offset of SS hover Sense strength frame*/
    u16 u16_ssHvrRxBaselineAddr;												/*Offset of SS hover Sense baseline frame*/

    u16 u16_ssPrxTxRawAddr;													/*Offset of SS proximity force raw frame*/
    u16 u16_ssPrxTxFilterAddr;													/*Offset of SS proximity force filter frame*/
    u16 u16_ssPrxTxStrenAddr;													/*Offset of SS proximity force strength frame*/
    u16 u16_ssPrxTxBaselineAddr;												/*Offset of SS proximity force baseline frame*/

    u16 u16_ssPrxRxRawAddr;													/*Offset of SS proximity sense raw frame*/
    u16 u16_ssPrxRxFilterAddr;													/*Offset of SS proximity sense filter frame*/
    u16 u16_ssPrxRxStrenAddr;													/*Offset of SS proximity sense strength frame*/
    u16 u16_ssPrxRxBaselineAddr;												/*Offset of SS proximity sense baseline frame*/
} SysInfo;

/** @}*/

int initCore(struct fts_ts_info *info);
void setResetGpio(int gpio);
int fts_system_reset(void);
int isSystemResettedUp(void);
int isSystemResettedDown(void);
void setSystemResetedUp(int val);
void setSystemResetedDown(int val);
int pollForEvent(int *event_to_search, int event_bytes, u8 *readData, int time_to_wait);
int checkEcho(u8 *cmd, int size);
int setScanMode(u8 mode, u8 settings);
int setFeatures(u8 feat, u8 *settings, int size);
int defaultSysInfo(int i2cError);
int writeSysCmd(u8 sys_cmd, u8 *sett, int size);
int readSysInfo(int request);
int readConfig(u16 offset, u8 *outBuf, int len);
int fts_disableInterrupt(void);
int fts_disableInterruptNoSync(void);
int fts_resetDisableIrqCount(void);
int fts_enableInterrupt(void);
int fts_crc_check(void);
int requestSyncFrame(u8 type);
int fts_get_lockdown_info(u8 *lockData, struct fts_ts_info *info);
int writeLockDownInfo(u8 *data, int size, u8 lock_id);
int readLockDownInfo(u8 *lockData, u8 lock_id, int size);

#endif /* FTS_CORE_H */
