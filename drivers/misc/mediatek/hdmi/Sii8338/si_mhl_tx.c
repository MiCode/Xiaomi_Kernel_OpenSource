#include "si_common.h"
#include "si_mhl_defs.h"
#include "si_mhl_tx_api.h"
#include "si_mhl_tx.h"
#include "si_mhl_tx_base_drv_api.h"
#include "si_drv_mhl_tx.h"
#include "si_cra_cfg.h"
#include "si_8338_regs.h"
#include "si_cra.h"
#include "smartbook.h"
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include "si_drv_mdt_tx.h"
#include "hdmi_drv.h"

#define NUM_CBUS_EVENT_QUEUE_EVENTS 5
typedef struct _CBusQueue_t {
	uint8_t head;
	uint8_t tail;
	cbus_req_t queue[NUM_CBUS_EVENT_QUEUE_EVENTS];
} CBusQueue_t, *PCBusQueue_t;
#define QUEUE_SIZE(x) (sizeof(x.queue)/sizeof(x.queue[0]))
#define MAX_QUEUE_DEPTH(x) (QUEUE_SIZE(x) - 1)
#define QUEUE_DEPTH(x) ((x.head <= x.tail)?(x.tail-x.head):(QUEUE_SIZE(x)-x.head+x.tail))
#define QUEUE_FULL(x) (QUEUE_DEPTH(x) >= MAX_QUEUE_DEPTH(x))
#define ADVANCE_QUEUE_HEAD(x) { x.head = (x.head < MAX_QUEUE_DEPTH(x))?(x.head+1):0; }
#define ADVANCE_QUEUE_TAIL(x) { x.tail = (x.tail < MAX_QUEUE_DEPTH(x))?(x.tail+1):0; }
#define RETREAT_QUEUE_HEAD(x) { x.head = (x.head > 0)?(x.head-1):MAX_QUEUE_DEPTH(x); }
CBusQueue_t CBusQueue;

extern uint8_t VIDEO_CAPABILITY_D_BLOCK_found;
extern bool_t mscAbortFlag;

struct hrtimer hr_timer_AbortTimer_CHK;

enum hrtimer_restart timer_AbortTimer_CHK_callback(struct hrtimer *timer)
{
	mscAbortFlag = false;
	TX_DEBUG_PRINT(("Drv: timer_AbortTimer_CHK_Expired now!!!!!!\n"));
	return HRTIMER_NORESTART;
}


cbus_req_t *GetNextCBusTransactionImpl(void)
{
	if (0 == QUEUE_DEPTH(CBusQueue)) {
		return NULL;
	} else {
		cbus_req_t *retVal;
		retVal = &CBusQueue.queue[CBusQueue.head];
		ADVANCE_QUEUE_HEAD(CBusQueue)
		    return retVal;
	}
}

#ifdef ENABLE_TX_DEBUG_PRINT
cbus_req_t *GetNextCBusTransactionWrapper(char *pszFunction)
{
	TX_DEBUG_PRINT(("MhlTx:GetNextCBusTransaction: %s depth: %d  head: %d  tail: %d\n",
			pszFunction,
			(int)QUEUE_DEPTH(CBusQueue), (int)CBusQueue.head, (int)CBusQueue.tail));
	return GetNextCBusTransactionImpl();
}

#define GetNextCBusTransaction(func) GetNextCBusTransactionWrapper(#func)
#else
#define GetNextCBusTransaction(func) GetNextCBusTransactionImpl()
#endif
bool_t PutNextCBusTransactionImpl(cbus_req_t *pReq)
{
	if (QUEUE_FULL(CBusQueue)) {
		return false;
	}
	CBusQueue.queue[CBusQueue.tail] = *pReq;
	ADVANCE_QUEUE_TAIL(CBusQueue)
	    return true;
}

#ifdef ENABLE_TX_DEBUG_PRINT
bool_t PutNextCBusTransactionWrapper(cbus_req_t *pReq)
{
	bool_t retVal;
	TX_DEBUG_PRINT(("MhlTx: PutNextCBusTransaction %02X %02X %02X depth:%d head: %d tail:%d\n",
			(int)pReq->command,
			(int)((MHL_MSC_MSG ==
			       pReq->command) ? pReq->payload_u.msgData[0] : pReq->offsetData)
			,
			(int)((MHL_MSC_MSG ==
			       pReq->command) ? pReq->payload_u.msgData[1] : pReq->payload_u.
			      msgData[0])
			, (int)QUEUE_DEPTH(CBusQueue)
			, (int)CBusQueue.head, (int)CBusQueue.tail));
	retVal = PutNextCBusTransactionImpl(pReq);
	if (!retVal) {
		TX_DEBUG_PRINT(("MhlTx: PutNextCBusTransaction queue full, when adding event %02x\n", (int)pReq->command));
	}
	return retVal;
}

#define PutNextCBusTransaction(req) PutNextCBusTransactionWrapper(req)
#else
#define PutNextCBusTransaction(req) PutNextCBusTransactionImpl(req)
#endif
bool_t PutPriorityCBusTransactionImpl(cbus_req_t *pReq)
{
	if (QUEUE_FULL(CBusQueue)) {
		TX_DEBUG_PRINT(("MhlTx  Queque is full\n"));
		return false;
	}
	RETREAT_QUEUE_HEAD(CBusQueue)
	    CBusQueue.queue[CBusQueue.head] = *pReq;
	return true;
}

#ifdef ENABLE_TX_DEBUG_PRINT
bool_t PutPriorityCBusTransactionWrapper(cbus_req_t *pReq)
{
	bool_t retVal;
	TX_DEBUG_PRINT(("MhlTx: PutPriorityCBusTransaction %02X %02X %02X depth:%d head: %d tail:%d\n", (int)pReq->command, (int)((MHL_MSC_MSG == pReq->command) ? pReq->payload_u.msgData[0] : pReq->offsetData)
			,
			(int)((MHL_MSC_MSG ==
			       pReq->command) ? pReq->payload_u.msgData[1] : pReq->payload_u.
			      msgData[0])
			, (int)QUEUE_DEPTH(CBusQueue)
			, (int)CBusQueue.head, (int)CBusQueue.tail));
	retVal = PutPriorityCBusTransactionImpl(pReq);
	if (!retVal) {
		TX_DEBUG_PRINT(("MhlTx: PutPriorityCBusTransaction queue full, when adding event 0x%02X\n", (int)pReq->command));
	}
	return retVal;
}

#define PutPriorityCBusTransaction(pReq) PutPriorityCBusTransactionWrapper(pReq)
#else
#define PutPriorityCBusTransaction(pReq) PutPriorityCBusTransactionImpl(pReq)
#endif
#define IncrementCBusReferenceCount(func) {mhlTxConfig.cbusReferenceCount++; TX_DEBUG_PRINT(("MhlTx:%s cbusReferenceCount:%d\n", #func, (int)mhlTxConfig.cbusReferenceCount)); }
#define DecrementCBusReferenceCount(func) {mhlTxConfig.cbusReferenceCount--; TX_DEBUG_PRINT(("MhlTx:%s cbusReferenceCount:%d\n", #func, (int)mhlTxConfig.cbusReferenceCount)); }
#define SetMiscFlag(func, x) { mhlTxConfig.miscFlags |=  (x); TX_DEBUG_PRINT(("MhlTx:%s set %s\n", #func, #x)); }
#define ClrMiscFlag(func, x) { mhlTxConfig.miscFlags &= ~(x); TX_DEBUG_PRINT(("MhlTx:%s clr %s\n", #func, #x)); }
static mhlTx_config_t mhlTxConfig = { 0 };

static bool_t SiiMhlTxSetDCapRdy(void);
static bool_t SiiMhlTxRapkSend(uint8_t rapkErrCode);
static void MhlTxResetStates(void);
static bool_t MhlTxSendMscMsg(uint8_t command, uint8_t cmdData);
static void SiiMhlTxRefreshPeerDevCapEntries(void);
static void MhlTxDriveStates(void);
extern PLACE_IN_CODE_SEG uint8_t rcpSupportTable[];
extern HDMI_CABLE_TYPE MHL_Connect_type;
extern void hdmi_state_callback(HDMI_STATE state);
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
extern void smartbook_state_callback(void);
#endif

bool_t MhlTxCBusBusy(void)
{
	return ((QUEUE_DEPTH(CBusQueue) > 0) || SiiMhlTxDrvCBusBusy()
		|| mhlTxConfig.cbusReferenceCount) ? true : false;
}

static uint8_t QualifyPathEnable(void)
{
	uint8_t retVal = 0;
	if (MHL_STATUS_PATH_ENABLED & mhlTxConfig.status_1) {
		TX_DEBUG_PRINT(("\t\t\tMHL_STATUS_PATH_ENABLED\n"));
		retVal = 1;
	} else {
		if (0x10 == mhlTxConfig.aucDevCapCache[DEVCAP_OFFSET_MHL_VERSION]) {
			if (0x44 == mhlTxConfig.aucDevCapCache[DEVCAP_OFFSET_INT_STAT_SIZE]) {
				retVal = 1;
				TX_DEBUG_PRINT(("\t\t\tLegacy Support for MHL_STATUS_PATH_ENABLED\n"));
			}
		}
	}
	return retVal;
}

void SiiMhlTxTmdsEnable(void)
{
	TX_DEBUG_PRINT(("MhlTx:SiiMhlTxTmdsEnable\n"));
	if (MHL_RSEN & mhlTxConfig.mhlHpdRSENflags) {
		TX_DEBUG_PRINT(("\tMHL_RSEN\n"));
		if (MHL_HPD & mhlTxConfig.mhlHpdRSENflags) {
			TX_DEBUG_PRINT(("\t\tMHL_HPD\n"));
			if (QualifyPathEnable()) {
				if (RAP_CONTENT_ON & mhlTxConfig.rapFlags) {
					TX_DEBUG_PRINT(("\t\t\t\tRAP CONTENT_ON\n"));
					SiiMhlTxDrvTmdsControl(true);
				}
			}
		}
	}
}

static bool_t SiiMhlTxSetInt(uint8_t regToWrite, uint8_t mask, uint8_t priorityLevel)
{
	cbus_req_t req;
	bool_t retVal;
	req.retryCount = 2;
	req.command = MHL_SET_INT;
	req.offsetData = regToWrite;
	req.payload_u.msgData[0] = mask;
	if (0 == priorityLevel) {
		retVal = PutPriorityCBusTransaction(&req);
	} else {
		retVal = PutNextCBusTransaction(&req);
	}
	return retVal;
}

/* Timon: phase out this function since pdatabytes is not used anymore */
/* Why? cause pdatabytes is useless */
static bool_t SiiMhlTxDoWriteBurst(uint8_t startReg, uint8_t *pData, uint8_t length)
{
	int i;
	if (FLAGS_WRITE_BURST_PENDING & mhlTxConfig.miscFlags) {
		cbus_req_t req;
		bool_t retVal;
		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxDoWriteBurst startReg:%d length:%d\n", (int)startReg,
				(int)length));
		req.retryCount = 1;
		req.command = MHL_WRITE_BURST;
		req.length = length;
		req.offsetData = startReg;
		/* req.payload_u.pdatabytes  = pData; */
		for (i = 0; i < length; i++)
			req.payload_u.msgData[i] = pData[i];
		retVal = PutPriorityCBusTransaction(&req);
		ClrMiscFlag(SiiMhlTxDoWriteBurst, FLAGS_WRITE_BURST_PENDING)
		    return retVal;
	}
	return false;
}

ScratchPadStatus_e SiiMhlTxRequestWriteBurst(uint8_t startReg, uint8_t length, uint8_t *pData)
{
	ScratchPadStatus_e retVal = SCRATCHPAD_BUSY;
	if (!(MHL_FEATURE_SP_SUPPORT & mhlTxConfig.mscFeatureFlag)) {
		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxRequestWriteBurst failed SCRATCHPAD_NOT_SUPPORTED\n"));
		retVal = SCRATCHPAD_NOT_SUPPORTED;
	} else {
		if ((FLAGS_SCRATCHPAD_BUSY & mhlTxConfig.miscFlags)
		    || MhlTxCBusBusy()
		    ) {
			TX_DEBUG_PRINT(("MhlTx:SiiMhlTxRequestWriteBurst failed FLAGS_SCRATCHPAD_BUSY\n"));
		} else {
			bool_t temp;
			uint8_t i, reg;
			TX_DEBUG_PRINT(("MhlTx:SiiMhlTxRequestWriteBurst, request sent\n"));
			for (i = 0, reg = startReg; (i < length) && (reg < SCRATCHPAD_SIZE);
			     ++i, ++reg) {
				mhlTxConfig.localScratchPad[reg] = pData[i];
			}
			temp = SiiMhlTxSetInt(MHL_RCHANGE_INT, MHL_INT_REQ_WRT, 1);
			retVal = temp ? SCRATCHPAD_SUCCESS : SCRATCHPAD_FAIL;
		}
	}
	return retVal;
}

int SiiMhlTxInitialize(uint8_t pollIntervalMs)
{
	bool_t ret = false;
	TX_DEBUG_PRINT(("MhlTx:SiiMhlTxInitialize\n"));
	CBusQueue.head = 0;
	CBusQueue.tail = 0;
	mhlTxConfig.pollIntervalMs = pollIntervalMs;
	TX_DEBUG_PRINT(("MhlTx: HPD: %d RSEN: %d\n",
			(int)((mhlTxConfig.mhlHpdRSENflags & MHL_HPD) ? 1 : 0)
			, (int)((mhlTxConfig.mhlHpdRSENflags & MHL_RSEN) ? 1 : 0)
		       ));
	MhlTxResetStates();
	TX_DEBUG_PRINT(("MhlTx: HPD: %d RSEN: %d\n",
			(mhlTxConfig.mhlHpdRSENflags & MHL_HPD) ? 1 : 0,
			(mhlTxConfig.mhlHpdRSENflags & MHL_RSEN) ? 1 : 0));
	hrtimer_init(&hr_timer_AbortTimer_CHK, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	hr_timer_AbortTimer_CHK.function = &timer_AbortTimer_CHK_callback;
	ret = SiiMhlTxChipInitialize();
	/* SiiInitExtVideo(); */
	if (ret) {
		return 0;	/* success */
	} else {
		return -1;	/* fail */
	}
}

void MhlTxProcessEvents(void)
{
	MhlTxDriveStates();
	if (mhlTxConfig.mhlConnectionEvent) {
		TX_DEBUG_PRINT(("MhlTx:MhlTxProcessEvents mhlConnectionEvent\n"));
		mhlTxConfig.mhlConnectionEvent = false;
		AppNotifyMhlEvent(mhlTxConfig.mhlConnected, mhlTxConfig.mscFeatureFlag);
		if (MHL_TX_EVENT_DISCONNECTION == mhlTxConfig.mhlConnected) {
			MhlTxResetStates();
		} else if (MHL_TX_EVENT_CONNECTION == mhlTxConfig.mhlConnected) {
			SiiMhlTxSetDCapRdy();
		}
	} else if (mhlTxConfig.mscMsgArrived) {
		TX_DEBUG_PRINT(("MhlTx:MhlTxProcessEvents MSC MSG <%02X, %02X>\n",
				(int)(mhlTxConfig.mscMsgSubCommand)
				, (int)(mhlTxConfig.mscMsgData)
			       ));
		mhlTxConfig.mscMsgArrived = false;
		switch (mhlTxConfig.mscMsgSubCommand) {
		case MHL_MSC_MSG_RAP:
			if (MHL_RAP_CONTENT_ON == mhlTxConfig.mscMsgData) {
				mhlTxConfig.rapFlags |= RAP_CONTENT_ON;
				TX_DEBUG_PRINT(("RAP CONTENT_ON\n"));
				SiiMhlTxTmdsEnable();
				SiiMhlTxRapkSend(MHL_MSC_MSG_RAP_NO_ERROR);
			} else if (MHL_RAP_CONTENT_OFF == mhlTxConfig.mscMsgData) {
				mhlTxConfig.rapFlags &= ~RAP_CONTENT_ON;
				TX_DEBUG_PRINT(("RAP CONTENT_OFF\n"));
				SiiMhlTxDrvTmdsControl(false);
				SiiMhlTxRapkSend(MHL_MSC_MSG_RAP_NO_ERROR);
			} else if (MHL_RAP_CMD_POLL == mhlTxConfig.mscMsgData) {
				SiiMhlTxRapkSend(MHL_MSC_MSG_RAP_UNSUPPORTED_ACT_CODE);
			} else {
				SiiMhlTxRapkSend(MHL_MSC_MSG_RAP_UNRECOGNIZED_ACT_CODE);
			}
			break;
		case MHL_MSC_MSG_RCP:
			if (MHL_LOGICAL_DEVICE_MAP & rcpSupportTable[mhlTxConfig.mscMsgData & 0x7F]) {
				AppNotifyMhlEvent(MHL_TX_EVENT_RCP_RECEIVED,
						  mhlTxConfig.mscMsgData);
			} else {
				mhlTxConfig.mscSaveRcpKeyCode = mhlTxConfig.mscMsgData;
				SiiMhlTxRcpeSend(RCPE_INEEFECTIVE_KEY_CODE);
			}
			break;
		case MHL_MSC_MSG_RCPK:
			AppNotifyMhlEvent(MHL_TX_EVENT_RCPK_RECEIVED, mhlTxConfig.mscMsgData);
			DecrementCBusReferenceCount(MhlTxProcessEvents)
			    mhlTxConfig.mscLastCommand = 0;
			mhlTxConfig.mscMsgLastCommand = 0;
			TX_DEBUG_PRINT(("MhlTx:MhlTxProcessEvents RCPK\n"));
			break;
		case MHL_MSC_MSG_RCPE:
			AppNotifyMhlEvent(MHL_TX_EVENT_RCPE_RECEIVED, mhlTxConfig.mscMsgData);
			break;
		case MHL_MSC_MSG_RAPK:
			DecrementCBusReferenceCount(MhlTxProcessEvents)
			    mhlTxConfig.mscLastCommand = 0;
			mhlTxConfig.mscMsgLastCommand = 0;
			TX_DEBUG_PRINT(("MhlTx:MhlTxProcessEventsRAPK\n"));
			break;
		default:
			break;
		}
	}
}

extern bool_t mscAbortFlag;
static void MhlTxDriveStates(void)
{
#ifndef __KERNEL__
	if (mscAbortFlag) {
		if (HalTimerExpired(TIMER_ABORT)) {
			mscAbortFlag = false;
			TX_DEBUG_PRINT(("Timer Tabort_next timeout!\n"));
		} else {
			return;
		}
	}
#else
	if (mscAbortFlag) {
		TX_DEBUG_PRINT(("Abort Timer is working!\n"));
		return;
	} else {
		/* TX_DEBUG_PRINT(("normal work!\n")); */
	}
#endif
	if (QUEUE_DEPTH(CBusQueue) > 0) {
		/* HalTimerWait(100); */
		if (!SiiMhlTxDrvCBusBusy()) {
			int reQueueRequest = 0;
			cbus_req_t *pReq = GetNextCBusTransaction(MhlTxDriveStates);
			if (pReq == NULL) {
				printk("mhl_drv: GetNextCBusTransaction returns null\n");
				return;
			}
			if (MHL_SET_INT == pReq->command) {
				if (MHL_RCHANGE_INT == pReq->offsetData) {
					if (FLAGS_SCRATCHPAD_BUSY & mhlTxConfig.miscFlags) {
						if (MHL_INT_REQ_WRT == pReq->payload_u.msgData[0]) {
							reQueueRequest = 1;
						} else if (MHL_INT_GRT_WRT ==
							   pReq->payload_u.msgData[0]) {
							reQueueRequest = 0;
						}
					} else {
						if (MHL_INT_REQ_WRT == pReq->payload_u.msgData[0]) {
							IncrementCBusReferenceCount
							    (MhlTxDriveStates)
							    SetMiscFlag(MhlTxDriveStates,
									FLAGS_SCRATCHPAD_BUSY)
							    SetMiscFlag(MhlTxDriveStates,
									FLAGS_WRITE_BURST_PENDING)
						} else if (MHL_INT_GRT_WRT ==
							   pReq->payload_u.msgData[0]) {
							SetMiscFlag(MhlTxDriveStates,
								    FLAGS_SCRATCHPAD_BUSY)
						}
					}
				}
			}
			if (reQueueRequest) {
				if (pReq->retryCount-- > 0) {
					PutNextCBusTransaction(pReq);
				}
			} else {
				if (MHL_MSC_MSG == pReq->command) {
					mhlTxConfig.mscMsgLastCommand = pReq->payload_u.msgData[0];
					mhlTxConfig.mscMsgLastData = pReq->payload_u.msgData[1];
				} else {
					mhlTxConfig.mscLastOffset = pReq->offsetData;
					mhlTxConfig.mscLastData = pReq->payload_u.msgData[0];
				}
				mhlTxConfig.mscLastCommand = pReq->command;
				IncrementCBusReferenceCount(MhlTxDriveStates)
				    SiiMhlTxDrvSendCbusCommand(pReq);
			}
		} else {
			/* TX_DEBUG_PRINT(("SiiMhlTxDrvCBusBusy\n")); */
		}
	}
}

static void ExamineLocalAndPeerVidLinkMode(void)
{
	mhlTxConfig.linkMode &= ~MHL_STATUS_CLK_MODE_MASK;
	mhlTxConfig.linkMode |= MHL_STATUS_CLK_MODE_NORMAL;
	if (MHL_DEV_VID_LINK_SUPP_PPIXEL & mhlTxConfig.aucDevCapCache[DEVCAP_OFFSET_VID_LINK_MODE]) {
		if (MHL_DEV_VID_LINK_SUPP_PPIXEL & DEVCAP_VAL_VID_LINK_MODE) {
			mhlTxConfig.linkMode &= ~MHL_STATUS_CLK_MODE_MASK;
			mhlTxConfig.linkMode |= mhlTxConfig.preferredClkMode;
		}
	}
	SiMhlTxDrvSetClkMode(mhlTxConfig.linkMode & MHL_STATUS_CLK_MODE_MASK);
}

#define FLAG_OR_NOT(x) (FLAGS_HAVE_##x & mhlTxConfig.miscFlags)?#x:""
#define SENT_OR_NOT(x) (FLAGS_SENT_##x & mhlTxConfig.miscFlags)?#x:""
void SiiMhlTxMscCommandDone(uint8_t data1)
{
	TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone. data1 = %02X\n", (int)data1));
	DecrementCBusReferenceCount(SiiMhlTxMscCommandDone)
	    if (MHL_READ_DEVCAP == mhlTxConfig.mscLastCommand) {
		if (mhlTxConfig.mscLastOffset < sizeof(mhlTxConfig.aucDevCapCache)) {
			mhlTxConfig.aucDevCapCache[mhlTxConfig.mscLastOffset] = data1;
			TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone peer DEV_CAP[0x%02x]:0x%02x index:0x%02x\n", (int)mhlTxConfig.mscLastOffset, (int)data1, (int)mhlTxConfig.ucDevCapCacheIndex));
			if (MHL_DEV_CATEGORY_OFFSET == mhlTxConfig.mscLastOffset) {
				uint8_t param;
				mhlTxConfig.miscFlags |= FLAGS_HAVE_DEV_CATEGORY;
				param = data1 & MHL_DEV_CATEGORY_POW_BIT;
				TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone FLAGS_HAVE_DEV_CATEGORY\n"));
				if (MHL_TX_EVENT_STATUS_PASSTHROUGH ==
				    AppNotifyMhlEvent(MHL_TX_EVENT_POW_BIT_CHG, param)) {
					SiiMhlTxDrvPowBitChange((bool_t) param);
				}
			} else if (MHL_DEV_FEATURE_FLAG_OFFSET == mhlTxConfig.mscLastOffset) {
				mhlTxConfig.miscFlags |= FLAGS_HAVE_DEV_FEATURE_FLAGS;
				TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone FLAGS_HAVE_DEV_FEATURE_FLAGS\n"));
				mhlTxConfig.mscFeatureFlag = data1;
				TX_DEBUG_PRINT(("MhlTx: Peer's Feature Flag = %02X\n\n",
						(int)data1));
			} else if (DEVCAP_OFFSET_VID_LINK_MODE == mhlTxConfig.mscLastOffset) {
				ExamineLocalAndPeerVidLinkMode();
			} else if (DEVCAP_OFFSET_RESERVED == mhlTxConfig.mscLastOffset) {
				MHL_Connect_type = MHL_CABLE;
				if (mhlTxConfig.aucDevCapCache[DEVCAP_OFFSET_RESERVED] == 0xB9) {
					MHL_Connect_type = MHL_SMB_CABLE;
				}
				TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone connect type %d %x\n",
						MHL_Connect_type,
						mhlTxConfig.
						aucDevCapCache[DEVCAP_OFFSET_RESERVED]));
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
				smartbook_state_callback();
				if (MHL_Connect_type == MHL_SMB_CABLE) {
					SiiHidSuspend(1);	/* call register input device when smartbook plug-in */
				}
#endif

			}

			if (++mhlTxConfig.ucDevCapCacheIndex < sizeof(mhlTxConfig.aucDevCapCache)) {
				SiiMhlTxReadDevcap(mhlTxConfig.ucDevCapCacheIndex);
			} else {
				AppNotifyMhlEvent(MHL_TX_EVENT_DCAP_CHG, 0);
				mhlTxConfig.mscLastCommand = 0;
				mhlTxConfig.mscLastOffset = 0;
			}
		}
	} else if (MHL_WRITE_STAT == mhlTxConfig.mscLastCommand) {
		TX_DEBUG_PRINT(("MhlTx: WRITE_STAT miscFlags: %02X\n\n",
				(int)mhlTxConfig.miscFlags));
		if (MHL_STATUS_REG_CONNECTED_RDY == mhlTxConfig.mscLastOffset) {
			if (MHL_STATUS_DCAP_RDY & mhlTxConfig.mscLastData) {
				mhlTxConfig.miscFlags |= FLAGS_SENT_DCAP_RDY;
				TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone FLAGS_SENT_DCAP_RDY\n"));
				SiiMhlTxSetInt(MHL_RCHANGE_INT, MHL_INT_DCAP_CHG, 0);
			}
		} else if (MHL_STATUS_REG_LINK_MODE == mhlTxConfig.mscLastOffset) {
			if (MHL_STATUS_PATH_ENABLED & mhlTxConfig.mscLastData) {
				mhlTxConfig.miscFlags |= FLAGS_SENT_PATH_EN;
				TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone FLAGS_SENT_PATH_EN\n"));
			}
		}
		mhlTxConfig.mscLastCommand = 0;
		mhlTxConfig.mscLastOffset = 0;
	} else if (MHL_MSC_MSG == mhlTxConfig.mscLastCommand) {
		if (MHL_MSC_MSG_RCPE == mhlTxConfig.mscMsgLastCommand) {
			if (SiiMhlTxRcpkSend(mhlTxConfig.mscSaveRcpKeyCode)) {
			}
		} else {
			TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone default\n"
					"\tmscLastCommand: 0x%02X\n"
					"\tmscMsgLastCommand: 0x%02X mscMsgLastData: 0x%02X\n"
					"\tcbusReferenceCount: %d\n",
					(int)mhlTxConfig.mscLastCommand,
					(int)mhlTxConfig.mscMsgLastCommand,
					(int)mhlTxConfig.mscMsgLastData,
					(int)mhlTxConfig.cbusReferenceCount));
		}
		mhlTxConfig.mscLastCommand = 0;
	} else if (MHL_WRITE_BURST == mhlTxConfig.mscLastCommand) {
		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone MHL_WRITE_BURST\n"));
		mhlTxConfig.mscLastCommand = 0;
		mhlTxConfig.mscLastOffset = 0;
		mhlTxConfig.mscLastData = 0;
		/* SiiMhlTxSetInt( MHL_RCHANGE_INT,MHL_INT_DSCR_CHG,0 ); */
	} else if (MHL_SET_INT == mhlTxConfig.mscLastCommand) {
		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone MHL_SET_INT\n"));
		if (MHL_RCHANGE_INT == mhlTxConfig.mscLastOffset) {
			TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone MHL_RCHANGE_INT\n"));
			if (MHL_INT_DSCR_CHG == mhlTxConfig.mscLastData) {
				DecrementCBusReferenceCount(SiiMhlTxMscCommandDone)
				    TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone MHL_INT_DSCR_CHG\n"));
				ClrMiscFlag(SiiMhlTxMscCommandDone, FLAGS_SCRATCHPAD_BUSY)
			}
		}
		mhlTxConfig.mscLastCommand = 0;
		mhlTxConfig.mscLastOffset = 0;
		mhlTxConfig.mscLastData = 0;
	} else {
		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone default\n"
				"\tmscLastCommand: 0x%02X mscLastOffset: 0x%02X\n"
				"\tcbusReferenceCount: %d\n", (int)mhlTxConfig.mscLastCommand,
				(int)mhlTxConfig.mscLastOffset,
				(int)mhlTxConfig.cbusReferenceCount));
	}
	if (!(FLAGS_RCP_READY & mhlTxConfig.miscFlags)) {
		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone. have(%s %s) sent(%s %s)\n",
				FLAG_OR_NOT(DEV_CATEGORY)
				, FLAG_OR_NOT(DEV_FEATURE_FLAGS)
				, SENT_OR_NOT(PATH_EN)
				, SENT_OR_NOT(DCAP_RDY)
			       ));
		if (FLAGS_HAVE_DEV_CATEGORY & mhlTxConfig.miscFlags) {
			if (FLAGS_HAVE_DEV_FEATURE_FLAGS & mhlTxConfig.miscFlags) {
				if (FLAGS_SENT_PATH_EN & mhlTxConfig.miscFlags) {
					if (FLAGS_SENT_DCAP_RDY & mhlTxConfig.miscFlags) {
						if (mhlTxConfig.ucDevCapCacheIndex >=
						    sizeof(mhlTxConfig.aucDevCapCache)) {
							mhlTxConfig.miscFlags |= FLAGS_RCP_READY;
							mhlTxConfig.mhlConnectionEvent = true;
							mhlTxConfig.mhlConnected =
							    MHL_TX_EVENT_RCP_READY;
							AppNotifyMhlEvent(mhlTxConfig.mhlConnected,
									  0);
						}
					}
				}
			}
		}
	}
}

#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
int SiiHidWrite(int key);
#endif
void SiiMhlTxMscWriteBurstDone(uint8_t data1)
{
	TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscWriteBurstDone(%02X)\n", (int)data1));
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
	SiiHidWrite(0);
#else
#define WRITE_BURST_TEST_SIZE 16
	uint8_t temp[WRITE_BURST_TEST_SIZE];
	uint8_t i;
	data1 = data1;
	TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscWriteBurstDone(%02X) \"", (int)data1));
	SiiMhlTxDrvGetScratchPad(0, temp, WRITE_BURST_TEST_SIZE);
	for (i = 0; i < WRITE_BURST_TEST_SIZE; ++i) {
		if (temp[i] >= ' ') {
			TX_DEBUG_PRINT(("%02X %c ", (int)temp[i], temp[i]));
		} else {
			TX_DEBUG_PRINT(("%02X . ", (int)temp[i]));
		}
	}
	TX_DEBUG_PRINT(("\"\n"));
#endif
}

void SiiMhlTxGotMhlMscMsg(uint8_t subCommand, uint8_t cmdData)
{
	mhlTxConfig.mscMsgArrived = true;
	mhlTxConfig.mscMsgSubCommand = subCommand;
	mhlTxConfig.mscMsgData = cmdData;
}


#ifdef MDT_SUPPORT
extern enum mdt_state g_state_for_mdt;
extern struct msc_request g_prior_msc_request;
#endif



/* extern enum mdt_state         g_state_for_mdt; */
void SiiMhlTxGotMhlIntr(uint8_t intr_0, uint8_t intr_1)
{
	TX_DEBUG_PRINT(("MhlTx: INTERRUPT Arrived. %02X, %02X\n", (int)intr_0, (int)intr_1));
	if (MHL_INT_DCAP_CHG & intr_0) {
		if (MHL_STATUS_DCAP_RDY & mhlTxConfig.status_0) {
			SiiMhlTxRefreshPeerDevCapEntries();
		}
	}
	if (MHL_INT_DSCR_CHG & intr_0) {
		SiiMhlTxDrvGetScratchPad(0, mhlTxConfig.localScratchPad,
					 sizeof(mhlTxConfig.localScratchPad));
		ClrMiscFlag(SiiMhlTxGotMhlIntr, FLAGS_SCRATCHPAD_BUSY)
		    AppNotifyMhlEvent(MHL_TX_EVENT_DSCR_CHG, 0);
	}
#ifdef MDT_SUPPORT
/* /////////////////////// */
	if ((g_state_for_mdt != WAIT_FOR_WRITE_BURST_COMPLETE)
	    && (g_state_for_mdt != WAIT_FOR_REQ_WRT)
	    && (g_state_for_mdt != WAIT_FOR_GRT_WRT_COMPLETE))
#endif
	{

		if (MHL_INT_REQ_WRT & intr_0) {
			if (FLAGS_SCRATCHPAD_BUSY & mhlTxConfig.miscFlags) {
				SiiMhlTxSetInt(MHL_RCHANGE_INT, MHL_INT_GRT_WRT, 1);
			} else {
				SetMiscFlag(SiiMhlTxGotMhlIntr, FLAGS_SCRATCHPAD_BUSY)
				    SiiMhlTxSetInt(MHL_RCHANGE_INT, MHL_INT_GRT_WRT, 0);
			}
		}
	}			/* ///////////////////// */
	if (MHL_INT_GRT_WRT & intr_0) {
		uint8_t length = sizeof(mhlTxConfig.localScratchPad);
		TX_DEBUG_PRINT(("MhlTx: MHL_INT_GRT_WRT length:%d\n", (int)length));
		SiiMhlTxDoWriteBurst(0x40, mhlTxConfig.localScratchPad, length);
	}
	if (MHL_INT_EDID_CHG & intr_1) {
		SiiMhlTxDrvNotifyEdidChange();
	}
}

void SiiMhlTxGotMhlStatus(uint8_t status_0, uint8_t status_1)
{
	uint8_t StatusChangeBitMask0, StatusChangeBitMask1;
	StatusChangeBitMask0 = status_0 ^ mhlTxConfig.status_0;
	StatusChangeBitMask1 = status_1 ^ mhlTxConfig.status_1;
	mhlTxConfig.status_0 = status_0;
	mhlTxConfig.status_1 = status_1;
	if (MHL_STATUS_DCAP_RDY & StatusChangeBitMask0) {
		TX_DEBUG_PRINT(("MhlTx: DCAP_RDY changed\n"));
		if (MHL_STATUS_DCAP_RDY & status_0) {
			SiiMhlTxRefreshPeerDevCapEntries();
		}
	}
	if (MHL_STATUS_PATH_ENABLED & StatusChangeBitMask1) {
		TX_DEBUG_PRINT(("MhlTx: PATH_EN changed\n"));
		if (MHL_STATUS_PATH_ENABLED & status_1) {
			SiiMhlTxSetPathEn();
		} else {
			SiiMhlTxClrPathEn();
		}
	}
}

bool_t SiiMhlTxRcpSend(uint8_t rcpKeyCode)
{
	bool_t retVal;
	if ((0 == (MHL_FEATURE_RCP_SUPPORT & mhlTxConfig.mscFeatureFlag))
	    || !(FLAGS_RCP_READY & mhlTxConfig.miscFlags)
	    ) {
		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxRcpSend failed\n"));
		retVal = false;
	}
	retVal = MhlTxSendMscMsg(MHL_MSC_MSG_RCP, rcpKeyCode);
	if (retVal) {
		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxRcpSend\n"));
		IncrementCBusReferenceCount(SiiMhlTxRcpSend)
		    MhlTxDriveStates();
	}
	return retVal;
}

bool_t SiiMhlTxRcpkSend(uint8_t rcpKeyCode)
{
	bool_t retVal;
	retVal = MhlTxSendMscMsg(MHL_MSC_MSG_RCPK, rcpKeyCode);
	if (retVal) {
		MhlTxDriveStates();
	}
	return retVal;
}

static bool_t SiiMhlTxRapkSend(uint8_t rapkErrCode)
{
	bool_t retVal;
	retVal = MhlTxSendMscMsg(MHL_MSC_MSG_RAPK, rapkErrCode);
	if (retVal) {
		MhlTxDriveStates();
	}
	return retVal;
}

bool_t SiiMhlTxRcpeSend(uint8_t rcpeErrorCode)
{
	bool_t retVal;
	retVal = MhlTxSendMscMsg(MHL_MSC_MSG_RCPE, rcpeErrorCode);
	if (retVal) {
		MhlTxDriveStates();
	}
	return retVal;
}

static bool_t SiiMhlTxSetStatus(uint8_t regToWrite, uint8_t value)
{
	cbus_req_t req;
	bool_t retVal;
	req.retryCount = 2;
	req.command = MHL_WRITE_STAT;
	req.offsetData = regToWrite;
	req.payload_u.msgData[0] = value;
	TX_DEBUG_PRINT(("MhlTx:SiiMhlTxSetStatus\n"));
	retVal = PutNextCBusTransaction(&req);
	return retVal;
}

static bool_t SiiMhlTxSetDCapRdy(void)
{
	mhlTxConfig.connectedReady |= MHL_STATUS_DCAP_RDY;
	return SiiMhlTxSetStatus(MHL_STATUS_REG_CONNECTED_RDY, mhlTxConfig.connectedReady);
}

static bool_t SiiMhlTxSendLinkMode(void)
{
	return SiiMhlTxSetStatus(MHL_STATUS_REG_LINK_MODE, mhlTxConfig.linkMode);
}

bool_t SiiMhlTxSetPathEn(void)
{
	TX_DEBUG_PRINT(("MhlTx:SiiMhlTxSetPathEn\n"));
	SiiMhlTxTmdsEnable();
	mhlTxConfig.linkMode |= MHL_STATUS_PATH_ENABLED;
	return SiiMhlTxSetStatus(MHL_STATUS_REG_LINK_MODE, mhlTxConfig.linkMode);
}

bool_t SiiMhlTxClrPathEn(void)
{
	TX_DEBUG_PRINT(("MhlTx: SiiMhlTxClrPathEn\n"));
	SiiMhlTxDrvTmdsControl(false);
	mhlTxConfig.linkMode &= ~MHL_STATUS_PATH_ENABLED;
	return SiiMhlTxSetStatus(MHL_STATUS_REG_LINK_MODE, mhlTxConfig.linkMode);
}

bool_t SiiMhlTxReadDevcap(uint8_t offset)
{
	cbus_req_t req;
	TX_DEBUG_PRINT(("MhlTx:SiiMhlTxReadDevcap\n"));
	req.retryCount = 2;
	req.command = MHL_READ_DEVCAP;
	req.offsetData = offset;
	req.payload_u.msgData[0] = 0;
	return PutNextCBusTransaction(&req);
}

static void SiiMhlTxRefreshPeerDevCapEntries(void)
{
	if (mhlTxConfig.ucDevCapCacheIndex >= sizeof(mhlTxConfig.aucDevCapCache)) {
		mhlTxConfig.ucDevCapCacheIndex = 0;
		SiiMhlTxReadDevcap(mhlTxConfig.ucDevCapCacheIndex);
	}
}

static bool_t MhlTxSendMscMsg(uint8_t command, uint8_t cmdData)
{
	cbus_req_t req;
	uint8_t ccode;
	req.retryCount = 2;
	req.command = MHL_MSC_MSG;
	req.payload_u.msgData[0] = command;
	req.payload_u.msgData[1] = cmdData;
	ccode = PutNextCBusTransaction(&req);
	return ((bool_t) ccode);
}

void SiiMhlTxNotifyConnection(bool_t mhlConnected)
{
	mhlTxConfig.mhlConnectionEvent = true;
	TX_DEBUG_PRINT(("MhlTx: SiiMhlTxNotifyConnection MSC_STATE_IDLE %01X\n",
			(int)mhlConnected));
	if (mhlConnected) {



		mhlTxConfig.rapFlags |= RAP_CONTENT_ON;
		TX_DEBUG_PRINT(("RAP CONTENT_ON\n"));
		mhlTxConfig.mhlConnected = MHL_TX_EVENT_CONNECTION;
		mhlTxConfig.mhlHpdRSENflags |= MHL_RSEN;
		SiiMhlTxTmdsEnable();
		SiiMhlTxSendLinkMode();
	} else {


		mhlTxConfig.rapFlags &= ~RAP_CONTENT_ON;
		TX_DEBUG_PRINT(("RAP CONTENT_OFF\n"));
		mhlTxConfig.mhlConnected = MHL_TX_EVENT_DISCONNECTION;
		mhlTxConfig.mhlHpdRSENflags &= ~MHL_RSEN;
	}
	MhlTxProcessEvents();
}

extern uint8_t prefertiming;
void SiiMhlTxNotifyDsHpdChange(uint8_t dsHpdStatus)
{
	TX_DEBUG_PRINT(("MhlTx: SiiMhlTxNotifyDsHpdChange dsHpdStatus=%d\n", (int)dsHpdStatus));
	if (mscAbortFlag) {
		TX_DEBUG_PRINT(("Abort Timer is working in SiiMhlTxNotifyDsHpdChange!\n"));
		return;
	}

	if (0 == dsHpdStatus) {
		TX_DEBUG_PRINT(("MhlTx: Disable TMDS\n"));
		TX_DEBUG_PRINT(("MhlTx: DsHPD OFF\n"));
		mhlTxConfig.mhlHpdRSENflags &= ~MHL_HPD;
		AppNotifyMhlDownStreamHPDStatusChange(dsHpdStatus);
		SiiMhlTxDrvTmdsControl(false);
		hdmi_state_callback(HDMI_STATE_NO_DEVICE);
#ifdef CONFIG_MTK_SMARTBOOK_SUPPORT
		if (MHL_Connect_type == MHL_SMB_CABLE)
			SiiHidSuspend(0);	/* kirby 20130529 */
#endif
	} else {
		hdmi_state_callback(HDMI_STATE_ACTIVE);
		TX_DEBUG_PRINT(("MhlTx: Enable TMDS\n"));
		TX_DEBUG_PRINT(("MhlTx: DsHPD ON\n"));
		mhlTxConfig.mhlHpdRSENflags |= MHL_HPD;
		AppNotifyMhlDownStreamHPDStatusChange(dsHpdStatus);
		/*********************************for data range test*************************/
		/* SiiRegModify(REG_VID_ACEN, BIT1, CLEAR_BITS); */
		/* SiiRegModify(REG_VID_MODE, BIT4, BIT4); */
		TX_EDID_PRINT(("SiiRegRead(TX_PAGE_L0 | 0x0049)=0x%x@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n",
			       SiiRegRead(TX_PAGE_L0 | 0x0049)));

		SiiRegWrite(TX_PAGE_L0 | 0x0049, 0x00);
		TX_EDID_PRINT(("SiiRegRead(TX_PAGE_L0 | 0x0049)=0x%x################################\n", SiiRegRead(TX_PAGE_L0 | 0x0049)));
		/*********************************for data range test*************************/
		SiiDrvMhlTxReadEdid();
		if (VIDEO_CAPABILITY_D_BLOCK_found == true) {
			/* you should set the Quantization Ranges to limited range here(16-235). */
			SiiRegWrite(TX_PAGE_L0 | 0x0049, BIT3 | BIT5 | BIT7);
			TX_EDID_PRINT(("SiiRegRead(TX_PAGE_L0 | 0x0049)=0x%x&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&\n", SiiRegRead(TX_PAGE_L0 | 0x0049)));
		}

		siHdmiTx_VideoSel(prefertiming);	/* setting the VIC and AVI info */
		/* here you should set the prefer timing. */

		SiiMhlTxTmdsEnable();
	}
}

static void MhlTxResetStates(void)
{
	mhlTxConfig.mhlConnectionEvent = false;
	mhlTxConfig.mhlConnected = MHL_TX_EVENT_DISCONNECTION;
	mhlTxConfig.mhlHpdRSENflags &= ~(MHL_RSEN | MHL_HPD);
	mhlTxConfig.rapFlags &= ~RAP_CONTENT_ON;
	TX_DEBUG_PRINT(("RAP CONTENT_OFF\n"));
	mhlTxConfig.mscMsgArrived = false;
	mhlTxConfig.status_0 = 0;
	mhlTxConfig.status_1 = 0;
	mhlTxConfig.connectedReady = 0;
	mhlTxConfig.linkMode = MHL_STATUS_CLK_MODE_NORMAL;
	mhlTxConfig.preferredClkMode = MHL_STATUS_CLK_MODE_NORMAL;
	mhlTxConfig.cbusReferenceCount = 0;
	mhlTxConfig.miscFlags = 0;
	mhlTxConfig.mscLastCommand = 0;
	mhlTxConfig.mscMsgLastCommand = 0;
	mhlTxConfig.ucDevCapCacheIndex = 1 + sizeof(mhlTxConfig.aucDevCapCache);
}

uint8_t SiiTxReadConnectionStatus(void)
{
	return (mhlTxConfig.mhlConnected >= MHL_TX_EVENT_RCP_READY) ? 1 : 0;
}

uint8_t SiiMhlTxSetPreferredPixelFormat(uint8_t clkMode)
{
	if (~MHL_STATUS_CLK_MODE_MASK & clkMode) {
		return 1;
	} else {
		mhlTxConfig.preferredClkMode = clkMode;
		if (mhlTxConfig.ucDevCapCacheIndex <= sizeof(mhlTxConfig.aucDevCapCache)) {
			if (mhlTxConfig.ucDevCapCacheIndex > DEVCAP_OFFSET_VID_LINK_MODE) {
				ExamineLocalAndPeerVidLinkMode();
			}
		}
		return 0;
	}
}

uint8_t SiiTxGetPeerDevCapEntry(uint8_t index, uint8_t *pData)
{
	if (mhlTxConfig.ucDevCapCacheIndex < sizeof(mhlTxConfig.aucDevCapCache)) {
		return 1;
	} else {
		*pData = mhlTxConfig.aucDevCapCache[index];
		return 0;
	}
}

ScratchPadStatus_e SiiGetScratchPadVector(uint8_t offset, uint8_t length, uint8_t *pData)
{
	if (!(MHL_FEATURE_SP_SUPPORT & mhlTxConfig.mscFeatureFlag)) {
		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxRequestWriteBurst failed SCRATCHPAD_NOT_SUPPORTED\n"));
		return SCRATCHPAD_NOT_SUPPORTED;
	} else if (FLAGS_SCRATCHPAD_BUSY & mhlTxConfig.miscFlags) {
		return SCRATCHPAD_BUSY;
	} else if ((offset >= sizeof(mhlTxConfig.localScratchPad)) ||
		   (length > (sizeof(mhlTxConfig.localScratchPad) - offset))) {
		return SCRATCHPAD_BAD_PARAM;
	} else {
		uint8_t i, reg;
		for (i = 0, reg = offset;
		     (i < length) && (reg < sizeof(mhlTxConfig.localScratchPad)); ++i, ++reg) {
			pData[i] = mhlTxConfig.localScratchPad[reg];
		}
		return SCRATCHPAD_SUCCESS;
	}
}

void SiiMhlTxNotifyRgndMhl(void)
{
	if (MHL_TX_EVENT_STATUS_PASSTHROUGH == AppNotifyMhlEvent(MHL_TX_EVENT_RGND_MHL, 0)) {
		/* SiiMhlTxDrvProcessRgndMhl(); */
	}
}

/* ///////////////////////////////////////////////////////////////////////////// */
/*  */
/* CBusQueueReset */
/*  */
void CBusQueueReset(void)
{
	TX_DEBUG_PRINT(("MhlTx:CBusQueueReset\n"));

	/* reset queue of pending CBUS requests. */
	CBusQueue.head = 0;
	CBusQueue.tail = 0;
	/* QUEUE_DEPTH(CBusQueue)=0; */
}


#if defined(__KERNEL__) && defined(USE_PROC)
void mhl_seq_show(struct seq_file *s)
{
	if (mhlTxConfig.mhlHpdRSENflags & MHL_HPD)
		seq_puts(s, "MHL CABLE CONNECTED      [YES]\n");
	else
		seq_puts(s, "MHL CABLE CONNECTED      [NO]\n");
	if (mhlTxConfig.mhlHpdRSENflags & MHL_RSEN)
		seq_puts(s, "MHL SOURCE CONNECTED     [YES]\n");
	else
		seq_puts(s, "MHL SOURCE CONNECTED     [NO]\n");
	if (mhlTxConfig.connectedReady & MHL_STATUS_DCAP_RDY)
		seq_puts(s, "MHL DCP_RDY              [YES]\n");
	else
		seq_puts(s, "MHL DCP_RDY              [NO]\n");
	seq_printf(s, "Work Queque Depth:       [%d]\n", (int)QUEUE_DEPTH(CBusQueue));
}
#endif
