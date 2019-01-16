#ifndef __MHL_TX_API_H__
#define __MHL_TX_API_H__
#include "si_platform.h"

void siHdmiTx_VideoSel(int vmode);
void siHdmiTx_AudioSel(int AduioMode);
uint8_t siMhlTx_VideoSet(void);
uint8_t siMhlTx_AudioSet(void);



int SiiMhlTxInitialize(uint8_t pollIntervalMs);
#define	MHL_TX_EVENT_NONE				0x00
#define	MHL_TX_EVENT_DISCONNECTION		0x01
#define	MHL_TX_EVENT_CONNECTION			0x02
#define	MHL_TX_EVENT_RCP_READY			0x03
#define	MHL_TX_EVENT_RCP_RECEIVED		0x04
#define	MHL_TX_EVENT_RCPK_RECEIVED		0x05
#define	MHL_TX_EVENT_RCPE_RECEIVED		0x06
#define	MHL_TX_EVENT_DCAP_CHG			0x07
#define	MHL_TX_EVENT_DSCR_CHG			0x08
#define	MHL_TX_EVENT_POW_BIT_CHG		0x09
#define	MHL_TX_EVENT_RGND_MHL			0x0A
typedef enum {
	MHL_TX_EVENT_STATUS_HANDLED = 0, MHL_TX_EVENT_STATUS_PASSTHROUGH
} MhlTxNotifyEventsStatus_e;
bool_t SiiMhlTxRcpSend(uint8_t rcpKeyCode);
bool_t SiiMhlTxRcpkSend(uint8_t rcpKeyCode);
bool_t SiiMhlTxRcpeSend(uint8_t rcpeErrorCode);
bool_t SiiMhlTxSetPathEn(void);
bool_t SiiMhlTxClrPathEn(void);
extern void AppMhlTxDisableInterrupts(void);
extern void AppMhlTxRestoreInterrupts(void);
extern void AppVbusControl(bool_t powerOn);
void AppNotifyMhlEnabledStatusChange(bool_t enabled);
void AppNotifyMhlDownStreamHPDStatusChange(bool_t connected);
MhlTxNotifyEventsStatus_e AppNotifyMhlEvent(uint8_t eventCode, uint8_t eventParam);
typedef enum {
	SCRATCHPAD_FAIL = -4, SCRATCHPAD_BAD_PARAM = -3, SCRATCHPAD_NOT_SUPPORTED =
	    -2, SCRATCHPAD_BUSY = -1, SCRATCHPAD_SUCCESS = 0
} ScratchPadStatus_e;
#ifdef __KERNEL__
void SiiMhlTriggerSoftInt(void);
#endif
ScratchPadStatus_e SiiMhlTxRequestWriteBurst(uint8_t startReg, uint8_t length, uint8_t *pData);
bool_t MhlTxCBusBusy(void);
void MhlTxProcessEvents(void);
uint8_t SiiTxReadConnectionStatus(void);
uint8_t SiiMhlTxSetPreferredPixelFormat(uint8_t clkMode);
uint8_t SiiTxGetPeerDevCapEntry(uint8_t index, uint8_t *pData);
ScratchPadStatus_e SiiGetScratchPadVector(uint8_t offset, uint8_t length, uint8_t *pData);
/* void SiiMhlTxHwReset(uint16_t hwResetPeriod,uint16_t hwResetDelay ); */

#if defined(__KERNEL__)
#include <linux/kernel.h>
#define	PRINT	printk
#else
#include "si_osdebug.h"
#define	PRINT	printf
#endif

#ifndef DEBUG
#define DEBUG
#endif

#if defined(__KERNEL__)
#if defined(DEBUG)
#define TX_DEBUG_PRINT_WRAPPER(...) SiiOsDebugPrint(__FILE__, __LINE__, SII_OSAL_DEBUG_TX, __VA_ARGS__)
#define TX_DEBUG_PRINT(x)  printk x	/* TX_DEBUG_PRINT_WRAPPER x */
#else
#define TX_DEBUG_PRINT(...)
#endif
#else
#ifdef ENABLE_TX_DEBUG_PRINT
#ifdef C99_VA_ARG_SUPPORT
#define TX_DEBUG_PRINT_WRAPPER(...) SiiOsDebugPrint(__FILE__, __LINE__, SII_OSAL_DEBUG_TX, __VA_ARGS__)
#define TX_DEBUG_PRINT(x)  TX_DEBUG_PRINT_WRAPPER x
#else
#define TX_DEBUG_PRINT(x)   g_debugLineNo = __LINE__; g_debugFileName = __FILE__; g_channelArg = SII_OSAL_DEBUG_TX; SiiOsDebugPrintUseGlobal x
#endif
#else
#ifdef C99_VA_ARG_SUPPORT
#define TX_DEBUG_PRINT(...)
#else
#define TX_DEBUG_PRINT(x)
#endif
#endif
#endif

/* disable EDID print to see whether plug in detect will be faster */
#define ENABLE_TX_EDID_PRINT 0
#ifdef ENABLE_TX_EDID_PRINT
#define TX_EDID_PRINT(x)	TX_DEBUG_PRINT(x)
#else
#define TX_EDID_PRINT(x)
#endif
#endif
