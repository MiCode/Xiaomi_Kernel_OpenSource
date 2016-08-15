/**********************************************************************************/
/*  Copyright (c) 2011, Silicon Image, Inc.  All rights reserved.                 */
/*  Copyright(C) 2016 XiaoMi, Inc.                                                */
/*  No part of this work may be reproduced, modified, distributed, transmitted,   */
/*  transcribed, or translated into any language or computer format, in any form  */
/*  or by any means without written permission of: Silicon Image, Inc.,           */
/*  1140 East Arques Avenue, Sunnyvale, California 94085                          */
/**********************************************************************************/
/*
   @file si_mhl_tx.c
 */

#include "si_mhl_defs.h"
#include "si_mhl_tx_api.h"
#include "si_mhl_tx.h"
#include "si_mhl_tx_base_drv_api.h"
#include "si_drv_mhl_tx.h"
#include "sii_9244_api.h"
#include "si_app_devcap.h"
#include <linux/input.h>
#include <linux/mhl_api.h>
#include <linux/delay.h>
/*
   queue implementation
 */
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

cbus_req_t *GetNextCBusTransactionImpl(void)
{
	if (0 == QUEUE_DEPTH(CBusQueue))
		return NULL;
	else {
		cbus_req_t *retVal;
		retVal = &CBusQueue.queue[CBusQueue.head];
		ADVANCE_QUEUE_HEAD(CBusQueue)
			return retVal;
	}
}

#ifdef ENABLE_TX_DEBUG_PRINT
cbus_req_t *GetNextCBusTransactionWrapper(char *pszFunction, int iLine)
{
	TX_DEBUG_PRINT(("MhlTx:%d GetNextCBusTransaction: %s depth: %d  head: %d  tail: %d\n",
				iLine, pszFunction,
				(int)QUEUE_DEPTH(CBusQueue),
				(int)CBusQueue.head,
				(int)CBusQueue.tail));
	return GetNextCBusTransactionImpl();
}

#define GetNextCBusTransaction(func) GetNextCBusTransactionWrapper(#func, __LINE__)
#else
#define GetNextCBusTransaction(func) GetNextCBusTransactionImpl()
#endif

bool_t PutNextCBusTransactionImpl(cbus_req_t * pReq)
{
	if (QUEUE_FULL(CBusQueue))
		return false;

	CBusQueue.queue[CBusQueue.tail] = *pReq;
	ADVANCE_QUEUE_TAIL(CBusQueue)
		return true;
}

#ifdef ENABLE_TX_DEBUG_PRINT

bool_t PutNextCBusTransactionWrapper(cbus_req_t * pReq, int iLine)
{
	bool_t retVal;

	TX_DEBUG_PRINT(("MhlTx:%d PutNextCBusTransaction %02X %02X %02X depth:%d head: %d tail:%d\n"
				, iLine
				, (int)pReq->command
				, (int)((MHL_MSC_MSG == pReq->command)?pReq->payload_u.msgData[0]:pReq->offsetData)
				, (int)((MHL_MSC_MSG == pReq->command)?pReq->payload_u.msgData[1]:pReq->payload_u.msgData[0])
				, (int)QUEUE_DEPTH(CBusQueue)
				, (int)CBusQueue.head
				, (int)CBusQueue.tail
		      ));
	retVal = PutNextCBusTransactionImpl(pReq);

	if (!retVal)
		TX_DEBUG_PRINT(("MhlTx:%d PutNextCBusTransaction queue full, when adding event %02x\n", iLine, (int)pReq->command));
	return retVal;
}

#define PutNextCBusTransaction(req) PutNextCBusTransactionWrapper(req, __LINE__)
#else
#define PutNextCBusTransaction(req) PutNextCBusTransactionImpl(req)
#endif

bool_t PutPriorityCBusTransactionImpl(cbus_req_t * pReq)
{
	if (QUEUE_FULL(CBusQueue))
		return false;

	RETREAT_QUEUE_HEAD(CBusQueue)
		CBusQueue.queue[CBusQueue.head] = *pReq;
	return true;
}
#ifdef ENABLE_TX_DEBUG_PRINT
bool_t PutPriorityCBusTransactionWrapper(cbus_req_t *pReq, int iLine)
{
	bool_t retVal;
	TX_DEBUG_PRINT(("MhlTx:%d: PutPriorityCBusTransaction %02X %02X %02X depth:%d head: %d tail:%d\n"
				, iLine
				, (int)pReq->command
				, (int)((MHL_MSC_MSG == pReq->command)?pReq->payload_u.msgData[0]:pReq->offsetData)
				, (int)((MHL_MSC_MSG == pReq->command)?pReq->payload_u.msgData[1]:pReq->payload_u.msgData[0])
				, (int)QUEUE_DEPTH(CBusQueue)
				, (int)CBusQueue.head
				, (int)CBusQueue.tail
		      ));
	retVal = PutPriorityCBusTransactionImpl(pReq);
	if (!retVal) {
		TX_DEBUG_PRINT(("MhlTx:%d: PutPriorityCBusTransaction queue full, when adding event 0x%02X\n", iLine, (int)pReq->command));
	}
	return retVal;
}

#define PutPriorityCBusTransaction(pReq) PutPriorityCBusTransactionWrapper(pReq, __LINE__)
#else
#define PutPriorityCBusTransaction(pReq) PutPriorityCBusTransactionImpl(pReq)
#endif

#define IncrementCBusReferenceCount(func) {mhlTxConfig.cbusReferenceCount++; TX_DEBUG_PRINT(("MhlTx:%s cbusReferenceCount:%d\n", #func, (int)mhlTxConfig.cbusReferenceCount)); }
#define DecrementCBusReferenceCount(func) {mhlTxConfig.cbusReferenceCount--; TX_DEBUG_PRINT(("MhlTx:%s cbusReferenceCount:%d\n", #func, (int)mhlTxConfig.cbusReferenceCount)); }

#define SetMiscFlag(func, x) { mhlTxConfig.miscFlags |=  (x); TX_DEBUG_PRINT(("MhlTx:%s set %s\n", #func, #x)); }
#define ClrMiscFlag(func, x) { mhlTxConfig.miscFlags &= ~(x); TX_DEBUG_PRINT(("MhlTx:%s clr %s\n", #func, #x)); }

static mhlTx_config_t mhlTxConfig = { 0 };

static bool_t SiiMhlTxSetDCapRdy(void);
static bool_t SiiMhlTxRapkSend(void);

static void MhlTxResetStates(void);
static bool_t MhlTxSendMscMsg(uint8_t command, uint8_t cmdData);
static void SiiMhlTxRefreshPeerDevCapEntries(void);
static void MhlTxDriveStates(void);

extern uint8_t rcpSupportTable[];

bool_t MhlTxCBusBusy(void)
{
	return ((QUEUE_DEPTH(CBusQueue) > 0) || SiiMhlTxDrvCBusBusy()
		|| mhlTxConfig.cbusReferenceCount) ? true : false;
}

/*
   QualifyPathEnable
   Support MHL 1.0 sink devices.

   return value
   1 - consider PATH_EN received
   0 - consider PATH_EN NOT received
 */
static uint8_t QualifyPathEnable(void)
{
	uint8_t retVal = 0;
	if (MHL_STATUS_PATH_ENABLED & mhlTxConfig.status_1) {
		TX_DEBUG_PRINT(("\t\t\tMHL_STATUS_PATH_ENABLED\n"));
		retVal = 1;
	} else {
		if (0x10 ==
		    mhlTxConfig.aucDevCapCache[DEVCAP_OFFSET_MHL_VERSION]) {
			if (0x44 ==
			    mhlTxConfig.
			    aucDevCapCache[DEVCAP_OFFSET_INT_STAT_SIZE]) {
				retVal = 1;
				TX_DEBUG_PRINT(("\t\t\tLegacy Support for MHL_STATUS_PATH_ENABLED\n"));
			}
		}
	}
	return retVal;
}

static void SiiMhlTxTmdsEnable(void)
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

static bool_t SiiMhlTxSetInt(uint8_t regToWrite, uint8_t mask,
			     uint8_t priorityLevel)
{
	cbus_req_t req;
	bool_t retVal;



	req.retryCount = 2;
	req.command = MHL_SET_INT;
	req.offsetData = regToWrite;
	req.payload_u.msgData[0] = mask;
	if (0 == priorityLevel)
		retVal = PutPriorityCBusTransaction(&req);
	else
		retVal = PutNextCBusTransaction(&req);

	return retVal;
}

static bool_t SiiMhlTxDoWriteBurst(uint8_t startReg, uint8_t *pData, uint8_t length)
{
	if (FLAGS_WRITE_BURST_PENDING & mhlTxConfig.miscFlags) {
		cbus_req_t req;
		bool_t retVal;

		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxDoWriteBurst startReg:%d length:%d\n", (int)startReg, (int)length));

		req.retryCount  = 1;
		req.command     = MHL_WRITE_BURST;
		req.length      = length;
		req.offsetData  = startReg;
		req.payload_u.pdatabytes  = pData;

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
			TX_DEBUG_PRINT(("MhlTx:SiiMhlTxRequestWriteBurst failed FLAGS_SCRATCHPAD_BUSY \n"));
		} else {
			bool_t temp;
			uint8_t i, reg;
			TX_DEBUG_PRINT(("MhlTx:SiiMhlTxRequestWriteBurst, request sent\n"));
			for (i = 0, reg = startReg; (i < length) && (reg < SCRATCHPAD_SIZE); ++i, ++reg) {
				mhlTxConfig.localScratchPad[reg] = pData[i];
			}

			temp =  SiiMhlTxSetInt(MHL_RCHANGE_INT, MHL_INT_REQ_WRT, 1);
			retVal = temp ? SCRATCHPAD_SUCCESS : SCRATCHPAD_FAIL;
		}
	}
	return retVal;
}

bool_t SiiMhlTxInitialize(uint8_t pollIntervalMs)
{
	TX_DEBUG_PRINT(("MhlTx:SiiMhlTxInitialize\n"));


	CBusQueue.head = 0;
	CBusQueue.tail = 0;

	mhlTxConfig.pollIntervalMs = pollIntervalMs;

	TX_DEBUG_PRINT(("MhlTx: HPD: %d RSEN: %d\n"
				, (int)((mhlTxConfig.mhlHpdRSENflags & MHL_HPD)?1:0)
				, (int)((mhlTxConfig.mhlHpdRSENflags & MHL_RSEN)?1:0)
		      ));
	MhlTxResetStates();
	TX_DEBUG_PRINT(("MhlTx: HPD: %d RSEN: %d\n",
			(mhlTxConfig.mhlHpdRSENflags & MHL_HPD) ? 1 : 0,
			(mhlTxConfig.mhlHpdRSENflags & MHL_RSEN) ? 1 : 0));

	return SiiMhlTxChipInitialize();
}

void CBusQueueReset(void)
{
	TX_DEBUG_PRINT(("MhlTx:CBusQueueReset\n"));


	CBusQueue.head = 0;
	CBusQueue.tail = 0;

}

void MhlTxProcessEvents(void)
{
	MhlTxDriveStates();
	if (mhlTxConfig.mhlConnectionEvent) {
		TX_DEBUG_PRINT(("MhlTx:MhlTxProcessEvents mhlConnectionEvent\n"));

		mhlTxConfig.mhlConnectionEvent = false;

		AppNotifyMhlEvent(mhlTxConfig.mhlConnected,
				  mhlTxConfig.mscFeatureFlag);


		if (MHL_TX_EVENT_DISCONNECTION == mhlTxConfig.mhlConnected) {
			MhlTxResetStates();
		} else if (MHL_TX_EVENT_CONNECTION == mhlTxConfig.mhlConnected) {
			SiiMhlTxSetDCapRdy();
		}
	} else if (mhlTxConfig.mscMsgArrived) {
		TX_DEBUG_PRINT (("MhlTx:MhlTxProcessEvents MSC MSG <%02X, %02X>\n"
					, (int) (mhlTxConfig.mscMsgSubCommand)
					, (int) (mhlTxConfig.mscMsgData)
				));


		mhlTxConfig.mscMsgArrived = false;

		switch (mhlTxConfig.mscMsgSubCommand) {
		case MHL_MSC_MSG_RAP:

			if (MHL_RAP_CONTENT_ON == mhlTxConfig.mscMsgData) {
				mhlTxConfig.rapFlags |= RAP_CONTENT_ON;
				TX_DEBUG_PRINT(("RAP CONTENT_ON\n"));
				SiiMhlTxTmdsEnable();
			} else if (MHL_RAP_CONTENT_OFF ==
				   mhlTxConfig.mscMsgData) {
				mhlTxConfig.rapFlags &= ~RAP_CONTENT_ON;
				TX_DEBUG_PRINT(("RAP CONTENT_OFF\n"));
				SiiMhlTxDrvTmdsControl(false);
			}

			SiiMhlTxRapkSend();
			break;

		case MHL_MSC_MSG_RCP:
			if (MHL_LOGICAL_DEVICE_MAP &
			    rcpSupportTable[mhlTxConfig.mscMsgData & 0x7F]) {
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
			AppNotifyMhlEvent(MHL_TX_EVENT_RCPE_RECEIVED,
					mhlTxConfig.mscMsgData);
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

static void MhlTxDriveStates(void)
{

	if (QUEUE_DEPTH(CBusQueue) > 0) {
		if (!SiiMhlTxDrvCBusBusy()) {
			int reQueueRequest = 0;
			cbus_req_t *pReq =
			    GetNextCBusTransaction(MhlTxDriveStates);

			if (MHL_SET_INT == pReq->command) {
				if (MHL_RCHANGE_INT == pReq->offsetData) {
					if (FLAGS_SCRATCHPAD_BUSY & mhlTxConfig.
					    miscFlags) {
						if (MHL_INT_REQ_WRT ==
						    pReq->payload_u.
						    msgData[0]) {
							reQueueRequest = 1;
						} else if (MHL_INT_GRT_WRT ==
							   pReq->payload_u.
							   msgData[0]) {
							reQueueRequest = 0;
						}
					} else {
						if (MHL_INT_REQ_WRT ==
						    pReq->payload_u.
						    msgData[0]) {
							IncrementCBusReferenceCount(MhlTxDriveStates)
							SetMiscFlag(MhlTxDriveStates, FLAGS_SCRATCHPAD_BUSY)
							SetMiscFlag(MhlTxDriveStates, FLAGS_WRITE_BURST_PENDING)
						} else if (MHL_INT_GRT_WRT == pReq->payload_u.msgData[0]) {
							SetMiscFlag(MhlTxDriveStates, FLAGS_SCRATCHPAD_BUSY)
						}
					}
				}
			}
			if (reQueueRequest) {

				if (pReq->retryCount-- > 0)
					PutNextCBusTransaction(pReq);

			} else {
				if (MHL_MSC_MSG == pReq->command) {
					mhlTxConfig.mscMsgLastCommand = pReq->payload_u.msgData[0];
					mhlTxConfig.mscMsgLastData    = pReq->payload_u.msgData[1];
				} else {
					mhlTxConfig.mscLastOffset  = pReq->offsetData;
					mhlTxConfig.mscLastData    = pReq->payload_u.msgData[0];

				}
				mhlTxConfig.mscLastCommand = pReq->command;

				IncrementCBusReferenceCount(MhlTxDriveStates)
					SiiMhlTxDrvSendCbusCommand(pReq);
			}
		}
	}
}

static void ExamineLocalAndPeerVidLinkMode(void)
{

	mhlTxConfig.linkMode &= ~MHL_STATUS_CLK_MODE_MASK;
	mhlTxConfig.linkMode |= MHL_STATUS_CLK_MODE_NORMAL;



	if (MHL_DEV_VID_LINK_SUPP_PPIXEL & mhlTxConfig.
		aucDevCapCache[DEVCAP_OFFSET_VID_LINK_MODE]) {
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
	TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone. data1 = %02X\n",
			(int)data1));

	DecrementCBusReferenceCount(SiiMhlTxMscCommandDone)
	    if (MHL_READ_DEVCAP == mhlTxConfig.mscLastCommand) {
		if (mhlTxConfig.mscLastOffset <
		    sizeof(mhlTxConfig.aucDevCapCache)) {

			mhlTxConfig.aucDevCapCache[mhlTxConfig.mscLastOffset] =
			    data1;
			TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone peer DEV_CAP[0x%02x]:0x%02x index:0x%02x\n", (int)mhlTxConfig.mscLastOffset, (int)data1, (int)mhlTxConfig.ucDevCapCacheIndex));

			if (MHL_DEV_CATEGORY_OFFSET ==
			    mhlTxConfig.mscLastOffset) {
				uint8_t param;
				mhlTxConfig.miscFlags |=
				    FLAGS_HAVE_DEV_CATEGORY;
				param = data1 & MHL_DEV_CATEGORY_POW_BIT;
				TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone FLAGS_HAVE_DEV_CATEGORY\n"));

#if (VBUS_POWER_CHK == ENABLE)
				if (vbusPowerState !=
				    (bool_t) (data1 & MHL_DEV_CATEGORY_POW_BIT)) {
					vbusPowerState = (bool_t) (data1 & MHL_DEV_CATEGORY_POW_BIT);
					AppVbusControl(vbusPowerState);
				}
#endif

			} else if (MHL_DEV_FEATURE_FLAG_OFFSET ==
				   mhlTxConfig.mscLastOffset) {
				mhlTxConfig.miscFlags |=
				    FLAGS_HAVE_DEV_FEATURE_FLAGS;
				TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone FLAGS_HAVE_DEV_FEATURE_FLAGS\n"));


				mhlTxConfig.mscFeatureFlag = data1;

				TX_DEBUG_PRINT(("MhlTx: Peer's Feature Flag = %02X\n\n", (int)data1));
			} else if (DEVCAP_OFFSET_VID_LINK_MODE ==
				   mhlTxConfig.mscLastOffset) {
				ExamineLocalAndPeerVidLinkMode();
			}

			if (++mhlTxConfig.ucDevCapCacheIndex <
			    sizeof(mhlTxConfig.aucDevCapCache)) {

				SiiMhlTxReadDevcap(mhlTxConfig.
						   ucDevCapCacheIndex);
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
		} else if (MHL_STATUS_REG_LINK_MODE ==
			   mhlTxConfig.mscLastOffset) {
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
					"\tmscLastCommand: 0x%02X \n"
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



		SiiMhlTxSetInt(MHL_RCHANGE_INT, MHL_INT_DSCR_CHG, 0);
	} else if (MHL_SET_INT == mhlTxConfig.mscLastCommand) {
		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone MHL_SET_INT\n"));
		if (MHL_RCHANGE_INT == mhlTxConfig.mscLastOffset) {
			TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone MHL_RCHANGE_INT\n"));
			if (MHL_INT_DSCR_CHG == mhlTxConfig.mscLastData) {
				DecrementCBusReferenceCount(SiiMhlTxMscCommandDone)
				    TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone MHL_INT_DSCR_CHG\n"));
				ClrMiscFlag(SiiMhlTxMscCommandDone,
					    FLAGS_SCRATCHPAD_BUSY)
			}
		}

		mhlTxConfig.mscLastCommand = 0;
		mhlTxConfig.mscLastOffset = 0;
		mhlTxConfig.mscLastData = 0;
	} else {
		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone default\n"
				"\tmscLastCommand: 0x%02X mscLastOffset: 0x%02X\n"
				"\tcbusReferenceCount: %d\n",
				(int)mhlTxConfig.mscLastCommand,
				(int)mhlTxConfig.mscLastOffset,
				(int)mhlTxConfig.cbusReferenceCount));
	}
	if (!(FLAGS_RCP_READY & mhlTxConfig.miscFlags)) {
		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscCommandDone. have(%s %s) sent(%s %s)\n", FLAG_OR_NOT(DEV_CATEGORY)
				, FLAG_OR_NOT(DEV_FEATURE_FLAGS)
				, SENT_OR_NOT(PATH_EN)
				, SENT_OR_NOT(DCAP_RDY)
		));
		if (FLAGS_HAVE_DEV_CATEGORY & mhlTxConfig.miscFlags) {
			if (FLAGS_HAVE_DEV_FEATURE_FLAGS & mhlTxConfig.
			    miscFlags) {
				if (FLAGS_SENT_PATH_EN & mhlTxConfig.miscFlags) {
					if (FLAGS_SENT_DCAP_RDY & mhlTxConfig.
					    miscFlags) {
						if (mhlTxConfig.
						    ucDevCapCacheIndex >=
						    sizeof(mhlTxConfig.
							   aucDevCapCache)) {
							mhlTxConfig.miscFlags |=
							    FLAGS_RCP_READY;


							mhlTxConfig.
							    mhlConnectionEvent =
							    true;
							mhlTxConfig.
							    mhlConnected =
							    MHL_TX_EVENT_RCP_READY;
						}
					}
				}
			}
		}
	}
}

void SiiMhlTxMscWriteBurstDone(uint8_t data1)
{
#define WRITE_BURST_TEST_SIZE 16
	uint8_t temp[WRITE_BURST_TEST_SIZE];
	uint8_t i;
	data1 = data1;
	TX_DEBUG_PRINT(("MhlTx:SiiMhlTxMscWriteBurstDone(%02X) \"",
			(int)data1));
	SiiMhlTxDrvGetScratchPad(0, temp, WRITE_BURST_TEST_SIZE);
	for (i = 0; i < WRITE_BURST_TEST_SIZE; ++i) {
		if (temp[i] >= ' ')
			TX_DEBUG_PRINT(("%02X %c ", (int)temp[i], temp[i]));
		else
			TX_DEBUG_PRINT(("%02X . ", (int)temp[i]));

	}
	TX_DEBUG_PRINT(("\"\n"));
}

void SiiMhlTxGotMhlMscMsg(uint8_t subCommand, uint8_t cmdData)
{
	mhlTxConfig.mscMsgArrived = true;
	mhlTxConfig.mscMsgSubCommand = subCommand;
	mhlTxConfig.mscMsgData = cmdData;
}

void SiiMhlTxGotMhlIntr(uint8_t intr_0, uint8_t intr_1)
{
	TX_DEBUG_PRINT(("MhlTx: INTERRUPT Arrived. %02X, %02X\n", (int)intr_0,
			(int)intr_1));

	if (MHL_INT_DCAP_CHG & intr_0) {
		if (MHL_STATUS_DCAP_RDY & mhlTxConfig.status_0)
			SiiMhlTxRefreshPeerDevCapEntries();
	}

	if (MHL_INT_DSCR_CHG & intr_0) {
		SiiMhlTxDrvGetScratchPad(0, mhlTxConfig.localScratchPad,
					 sizeof(mhlTxConfig.localScratchPad));

		ClrMiscFlag(SiiMhlTxGotMhlIntr, FLAGS_SCRATCHPAD_BUSY)
		    AppNotifyMhlEvent(MHL_TX_EVENT_DSCR_CHG, 0);
	}
	if (MHL_INT_REQ_WRT & intr_0) {

		if (FLAGS_SCRATCHPAD_BUSY & mhlTxConfig.miscFlags)
			SiiMhlTxSetInt(MHL_RCHANGE_INT, MHL_INT_GRT_WRT, 1);
		else {
			SetMiscFlag(SiiMhlTxGotMhlIntr, FLAGS_SCRATCHPAD_BUSY)
				SiiMhlTxSetInt(MHL_RCHANGE_INT, MHL_INT_GRT_WRT, 0);
		}
	}
	if (MHL_INT_GRT_WRT & intr_0) {
		uint8_t length = sizeof(mhlTxConfig.localScratchPad);
		TX_DEBUG_PRINT(("MhlTx: MHL_INT_GRT_WRT length:%d\n",
				(int)length));
		SiiMhlTxDoWriteBurst(0x40, mhlTxConfig.localScratchPad, length);
	}

	if (MHL_INT_EDID_CHG & intr_1)
		SiiMhlTxDrvNotifyEdidChange();

}

void SiiMhlTxGotMhlStatus(uint8_t status_0, uint8_t status_1)
{
	uint8_t StatusChangeBitMask0, StatusChangeBitMask1;
	TX_DEBUG_PRINT(("MhlTx: STATUS Arrived. %02X, %02X\n", (int)status_0,
			(int)status_1));

	StatusChangeBitMask0 = status_0 ^ mhlTxConfig.status_0;
	StatusChangeBitMask1 = status_1 ^ mhlTxConfig.status_1;

	mhlTxConfig.status_0 = status_0;
	mhlTxConfig.status_1 = status_1;

	if (MHL_STATUS_DCAP_RDY & StatusChangeBitMask0) {

		TX_DEBUG_PRINT(("MhlTx: DCAP_RDY changed\n"));
		if (MHL_STATUS_DCAP_RDY & status_0)
			SiiMhlTxRefreshPeerDevCapEntries();

	}


	if (MHL_STATUS_PATH_ENABLED & StatusChangeBitMask1) {
		TX_DEBUG_PRINT(("MhlTx: PATH_EN changed\n"));
		if (MHL_STATUS_PATH_ENABLED & status_1) {

			SiiMhlTxSetPathEn();
		} else

			SiiMhlTxClrPathEn();

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
	if (retVal)
		MhlTxDriveStates();

	return retVal;
}

static bool_t SiiMhlTxRapkSend(void)
{
	return (MhlTxSendMscMsg(MHL_MSC_MSG_RAPK, 0));
}

bool_t SiiMhlTxRcpeSend(uint8_t rcpeErrorCode)
{
	bool_t retVal;

	retVal = MhlTxSendMscMsg(MHL_MSC_MSG_RCPE, rcpeErrorCode);
	if (retVal)
		MhlTxDriveStates();

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
	return SiiMhlTxSetStatus(MHL_STATUS_REG_CONNECTED_RDY,
				 mhlTxConfig.connectedReady);
}

static bool_t SiiMhlTxSendLinkMode(void)
{
	return SiiMhlTxSetStatus(MHL_STATUS_REG_LINK_MODE,
				 mhlTxConfig.linkMode);
}

bool_t SiiMhlTxSetPathEn(void)
{
	TX_DEBUG_PRINT(("MhlTx:SiiMhlTxSetPathEn\n"));
	SiiMhlTxTmdsEnable();
	mhlTxConfig.linkMode |= MHL_STATUS_PATH_ENABLED;
	return SiiMhlTxSetStatus(MHL_STATUS_REG_LINK_MODE,
				 mhlTxConfig.linkMode);
}

bool_t SiiMhlTxClrPathEn(void)
{
	TX_DEBUG_PRINT(("MhlTx: SiiMhlTxClrPathEn\n"));
	SiiMhlTxDrvTmdsControl(false);
	mhlTxConfig.linkMode &= ~MHL_STATUS_PATH_ENABLED;
	return SiiMhlTxSetStatus(MHL_STATUS_REG_LINK_MODE,
				 mhlTxConfig.linkMode);
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

/*
 * SiiMhlTxRefreshPeerDevCapEntries
 */

static void SiiMhlTxRefreshPeerDevCapEntries(void)
{


	{
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



void mhl_connect(bool_t mhlConnected)
{
	struct timespec uptime;
	int time_delta = 20;

	if (mhlConnected) {
		ktime_get_ts(&uptime);
		time_delta -= uptime.tv_sec;
		if (time_delta > 0)
			time_delta *= 1000;
		else
			time_delta = 500;
		msleep(time_delta);


	} else {

	}
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
	mhl_connect(mhlConnected);
	MhlTxProcessEvents();
}

void SiiMhlTxNotifyDsHpdChange(uint8_t dsHpdStatus)
{
	if (0 == dsHpdStatus) {
		TX_DEBUG_PRINT(("MhlTx: Disable TMDS\n"));
		TX_DEBUG_PRINT(("MhlTx: DsHPD OFF\n"));
		mhlTxConfig.mhlHpdRSENflags &= ~MHL_HPD;
		AppNotifyMhlDownStreamHPDStatusChange(dsHpdStatus);
		SiiMhlTxDrvTmdsControl(false);
	} else {
		TX_DEBUG_PRINT(("MhlTx: Enable TMDS\n"));
		TX_DEBUG_PRINT(("MhlTx: DsHPD ON\n"));
		mhlTxConfig.mhlHpdRSENflags |= MHL_HPD;
		AppNotifyMhlDownStreamHPDStatusChange(dsHpdStatus);
		SiiMhlTxTmdsEnable();
	}
}

static void MhlTxResetStates(void)
{
	mhlTxConfig.mhlConnectionEvent	= false;
	mhlTxConfig.mhlConnected		= MHL_TX_EVENT_DISCONNECTION;
	mhlTxConfig.mhlHpdRSENflags     &= ~(MHL_RSEN | MHL_HPD);
	mhlTxConfig.rapFlags 			&= ~RAP_CONTENT_ON;
	TX_DEBUG_PRINT(("RAP CONTENT_OFF\n"));
	mhlTxConfig.mscMsgArrived		= false;

	mhlTxConfig.status_0            = 0;
	mhlTxConfig.status_1            = 0;
	mhlTxConfig.connectedReady      = 0;
	mhlTxConfig.linkMode            = MHL_STATUS_CLK_MODE_NORMAL;
	mhlTxConfig.preferredClkMode	= MHL_STATUS_CLK_MODE_NORMAL;
	mhlTxConfig.cbusReferenceCount  = 0;
	mhlTxConfig.miscFlags           = 0;
	mhlTxConfig.mscLastCommand      = 0;
	mhlTxConfig.mscMsgLastCommand   = 0;
	mhlTxConfig.ucDevCapCacheIndex  = 0;
}

/*
 SiiTxReadConnectionStatus
returns:
0: if not fully connected
1: if fully connected
 */
uint8_t SiiTxReadConnectionStatus(void)
{
	return (mhlTxConfig.mhlConnected >= MHL_TX_EVENT_RCP_READY) ? 1 : 0;
}

/*
 SiiMhlTxSetPreferredPixelFormat

 clkMode - the preferred pixel format for the CLK_MODE status register

Returns: 0 -- success
1 -- failure - bits were specified that are not within the mask
 */
uint8_t SiiMhlTxSetPreferredPixelFormat(uint8_t clkMode)
{
	if (~MHL_STATUS_CLK_MODE_MASK & clkMode) {
		return 1;
	} else {
		mhlTxConfig.preferredClkMode = clkMode;



		if (mhlTxConfig.ucDevCapCacheIndex <=
		    sizeof(mhlTxConfig.aucDevCapCache)) {

			if (mhlTxConfig.ucDevCapCacheIndex >
			    DEVCAP_OFFSET_VID_LINK_MODE) {
				ExamineLocalAndPeerVidLinkMode();
			}
		}
		return 0;
	}
}

/*
   SiiTxGetPeerDevCapEntry
   index -- the devcap index to get
 *pData pointer to location to write data
 returns
 0 -- success
 1 -- busy.
 */
uint8_t SiiTxGetPeerDevCapEntry(uint8_t index, uint8_t * pData)
{
	if (mhlTxConfig.ucDevCapCacheIndex < sizeof(mhlTxConfig.aucDevCapCache)) {

		return 1;
	} else {
		*pData = mhlTxConfig.aucDevCapCache[index];
		return 0;
	}
}

ScratchPadStatus_e SiiGetScratchPadVector(uint8_t offset, uint8_t length,
					  uint8_t * pData)
{
	if (!(MHL_FEATURE_SP_SUPPORT & mhlTxConfig.mscFeatureFlag)) {
		TX_DEBUG_PRINT(("MhlTx:SiiMhlTxRequestWriteBurst failed SCRATCHPAD_NOT_SUPPORTED\n"));
		return SCRATCHPAD_NOT_SUPPORTED;
	} else if (FLAGS_SCRATCHPAD_BUSY & mhlTxConfig.miscFlags)
		return SCRATCHPAD_BUSY;
	else if ((offset >= sizeof(mhlTxConfig.localScratchPad)) ||
		   (length > (sizeof(mhlTxConfig.localScratchPad) - offset))) {
		return SCRATCHPAD_BAD_PARAM;
	} else {
		uint8_t i, reg;
		for (i = 0, reg = offset;
		     (i < length)
		     && (reg < sizeof(mhlTxConfig.localScratchPad));
		     ++i, ++reg) {
			pData[i] = mhlTxConfig.localScratchPad[reg];
		}
		return SCRATCHPAD_SUCCESS;
	}
}

void SiiMhlTxNotifyRgndMhl(void)
{
	TX_DEBUG_PRINT(("(SiiMhlTxNotifyRgndMhl)\n"));
	if (MHL_TX_EVENT_STATUS_PASSTHROUGH ==
	    AppNotifyMhlEvent(MHL_TX_EVENT_RGND_MHL, 0)) {

		TX_DEBUG_PRINT(("(MHL_TX_EVENT_STATUS_PASSTHROUGH)\n"));
		SiiMhlTxDrvProcessRgndMhl();
	}
}

void AppNotifyMhlDownStreamHPDStatusChange(bool_t connected)
{

	TX_DEBUG_PRINT(("App:%d AppNotifyMhlDownStreamHPDStatusChange connected:%s\n", (int)__LINE__, connected ? "yes" : "no"));
}

int testCount = 0;
int16_t rcpKeyCode = -1;
uint16_t keycode = KEY_RESERVED;

#define	APP_DEMO_RCP_SEND_KEY_CODE 0x44	/* play */

MhlTxNotifyEventsStatus_e AppNotifyMhlEvent(uint8_t eventCode,
					    uint8_t eventParam)
{
	MhlTxNotifyEventsStatus_e retVal = MHL_TX_EVENT_STATUS_PASSTHROUGH;
	switch (eventCode) {
	case MHL_TX_EVENT_DISCONNECTION:
		TX_DEBUG_PRINT(("App: Got event = MHL_TX_EVENT_DISCONNECTION\n"));
#ifdef	BYPASS_VBUS_HW_SUPPORT

#endif
		break;
	case MHL_TX_EVENT_CONNECTION:
		TX_DEBUG_PRINT(("App: Got event = MHL_TX_EVENT_CONNECTION\n"));
		SiiMhlTxSetPreferredPixelFormat(MHL_STATUS_CLK_MODE_NORMAL);

#ifdef ENABLE_WRITE_BURST_TEST
		testCount = 0;
		testStart = 0;
		TX_DEBUG_PRINT(("App:%d Reset Write Burst test counter\n",
				(int)__LINE__);
#endif
		break;
	case MHL_TX_EVENT_RCP_READY:
		if ((0 == (MHL_FEATURE_RCP_SUPPORT & eventParam))) {
			TX_DEBUG_PRINT(("App:%d Peer does NOT support RCP\n", (int)__LINE__));
		} else {
			TX_DEBUG_PRINT(("App:%d Peer supports RCP\n", (int)__LINE__));

			rcpKeyCode = APP_DEMO_RCP_SEND_KEY_CODE;
		}
		if ((0 == (MHL_FEATURE_RAP_SUPPORT & eventParam))) {
			TX_DEBUG_PRINT(("App:%d Peer does NOT support RAP\n", (int)__LINE__));
		} else {
			TX_DEBUG_PRINT(("App:%d Peer supports RAP\n",
				(int)__LINE__));
#ifdef ENABLE_WRITE_BURST_TEST
		testEnable = 1;
#endif
		}
		if ((0 == (MHL_FEATURE_SP_SUPPORT & eventParam))) {
			TX_DEBUG_PRINT(("App:%d Peer does NOT support WRITE_BURST\n", (int)__LINE__));
		} else {
			TX_DEBUG_PRINT(("App:%d Peer supports WRITE_BURST\n", (int)__LINE__));
		}
		break;
	case MHL_TX_EVENT_RCP_RECEIVED:

		if (eventParam > 0x7F)
			break;
		TX_DEBUG_PRINT(("App: Received an RCP key code = %02X\n", (int)eventParam)); rcpKeyCode = (int16_t) eventParam; printk("RCP key code 0x%02X ", rcpKeyCode);

		switch (rcpKeyCode) {
		case MHL_RCP_CMD_SELECT:
			keycode = KEY_SELECT;
			TX_DEBUG_PRINT(("\nSelect received\n\n"));
			break;
		case MHL_RCP_CMD_UP:
			keycode = KEY_UP;
			TX_DEBUG_PRINT(("\nUp received\n\n"));
			break;
		case MHL_RCP_CMD_DOWN:
			keycode = KEY_DOWN;
			TX_DEBUG_PRINT(("\nDown received\n\n"));
			break;
		case MHL_RCP_CMD_LEFT:
			keycode = KEY_LEFT;
			TX_DEBUG_PRINT(("\nLeft received\n\n"));
			break;
		case MHL_RCP_CMD_RIGHT:
			keycode = KEY_RIGHT;
			TX_DEBUG_PRINT(("\nRight received\n\n"));
			break;
		case MHL_RCP_CMD_ROOT_MENU:
			keycode = KEY_MENU;
			TX_DEBUG_PRINT(("\nRoot Menu received\n\n"));
			break;
		case MHL_RCP_CMD_SETUP_MENU:
			keycode = KEY_MENU;
			TX_DEBUG_PRINT(("\nSetup Menu received\n\n"));
			break;
		case MHL_RCP_CMD_CONTENTS_MENU:
			keycode = KEY_MENU;
			TX_DEBUG_PRINT(("\nContents Menu received\n\n"));
			break;
		case MHL_RCP_CMD_FAVORITE_MENU:
			keycode = KEY_MENU;
			TX_DEBUG_PRINT(("\nFavorite Menu received\n\n"));
			break;
		case MHL_RCP_CMD_EXIT:
			keycode = KEY_EXIT;
			TX_DEBUG_PRINT(("\nExit received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_0:
			keycode = KEY_0;
			TX_DEBUG_PRINT(("\nNumber 0 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_1:
			keycode = KEY_1;
			TX_DEBUG_PRINT(("\nNumber 1 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_2:
			keycode = KEY_2;
			TX_DEBUG_PRINT(("\nNumber 2 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_3:
			keycode = KEY_3;
			TX_DEBUG_PRINT(("\nNumber 3 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_4:
			keycode = KEY_4;
			TX_DEBUG_PRINT(("\nNumber 4 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_5:
			keycode = KEY_5;
			TX_DEBUG_PRINT(("\nNumber 5 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_6:
			keycode = KEY_6;
			TX_DEBUG_PRINT(("\nNumber 6 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_7:
			keycode = KEY_7;
			TX_DEBUG_PRINT(("\nNumber 7 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_8:
			keycode = KEY_8;
			TX_DEBUG_PRINT(("\nNumber 8 received\n\n"));
			break;
		case MHL_RCP_CMD_NUM_9:
			keycode = KEY_9;
			TX_DEBUG_PRINT(("\nNumber 9 received\n\n"));
			break;
		case MHL_RCP_CMD_DOT:
			keycode = KEY_DOT;
			TX_DEBUG_PRINT(("\nDot received\n\n"));
			break;
		case MHL_RCP_CMD_ENTER:
			keycode = KEY_ENTER;
			TX_DEBUG_PRINT(("\nEnter received\n\n"));
			break;
		case MHL_RCP_CMD_CLEAR:
			keycode = KEY_BACK;
			TX_DEBUG_PRINT(("\nClear received\n\n"));
			break;
		case MHL_RCP_CMD_CH_UP:
			keycode = KEY_CHANNELUP;
			TX_DEBUG_PRINT(("\nCH up received\n\n"));
			break;
		case MHL_RCP_CMD_CH_DOWN:
			keycode = KEY_CHANNELUP;
			TX_DEBUG_PRINT(("\nCH down received\n\n"));
			break;
		case MHL_RCP_CMD_PRE_CH:
			TX_DEBUG_PRINT(("\nPre channel received\n\n"));
			break;
		case MHL_RCP_CMD_SOUND_SELECT:
			keycode = KEY_SOUND;
			TX_DEBUG_PRINT(("\nSound Select received\n\n"));
			break;
		case MHL_RCP_CMD_INPUT_SELECT:
			keycode = KEY_SELECT;
			TX_DEBUG_PRINT(("\nInput Select received\n\n"));
			break;
		case MHL_RCP_CMD_SHOW_INFO:
			keycode = KEY_INFO;
			TX_DEBUG_PRINT(("\nShow Infor received\n\n"));
			break;
		case MHL_RCP_CMD_HELP:
			keycode = KEY_HELP;
			TX_DEBUG_PRINT(("\nHelp received\n\n"));
			break;
		case MHL_RCP_CMD_PAGE_UP:
			keycode = KEY_PAGEUP;
			TX_DEBUG_PRINT(("\nPage Up received\n\n"));
			break;
		case MHL_RCP_CMD_PAGE_DOWN:
			keycode = KEY_PAGEDOWN;
			TX_DEBUG_PRINT(("\nPage Down received\n\n"));
			break;
		case MHL_RCP_CMD_VOL_UP:
			keycode = KEY_VOLUMEUP;
			TX_DEBUG_PRINT(("\nVol Up received\n\n"));
			break;
		case MHL_RCP_CMD_VOL_DOWN:
			keycode = KEY_VOLUMEDOWN;
			TX_DEBUG_PRINT(("\nVol Down received\n\n"));
			break;
		case MHL_RCP_CMD_MUTE:
			keycode = KEY_MUTE;
			TX_DEBUG_PRINT(("\nMute received\n\n"));
			break;
		case MHL_RCP_CMD_PLAY:
			keycode = KEY_PLAY;
			TX_DEBUG_PRINT(("\nPlay received\n\n"));
			break;
		case MHL_RCP_CMD_PAUSE:
			keycode = KEY_PAUSE;
			TX_DEBUG_PRINT(("\nPause received\n\n"));
			break;
		case MHL_RCP_CMD_STOP:
			keycode = KEY_STOP;
			TX_DEBUG_PRINT(("\nStop received\n\n"));
			break;
		case MHL_RCP_CMD_FAST_FWD:
			keycode = KEY_FASTFORWARD;
			TX_DEBUG_PRINT(("\nFastfwd received\n\n"));
			break;
		case MHL_RCP_CMD_REWIND:
			keycode = KEY_REWIND;
			TX_DEBUG_PRINT(("\nRewind received\n\n"));
			break;
		case MHL_RCP_CMD_EJECT:
			keycode = KEY_EJECTCD;
			TX_DEBUG_PRINT(("\nEject received\n\n"));
			break;
		case MHL_RCP_CMD_FWD:
			keycode = KEY_FORWARD;
			TX_DEBUG_PRINT(("\nForward received\n\n"));
			break;
		case MHL_RCP_CMD_BKWD:
			keycode = KEY_BACK;
			TX_DEBUG_PRINT(("\nBackward received\n\n"));
			break;
		case MHL_RCP_CMD_PLAY_FUNC:
			keycode = KEY_PLAYCD;
			TX_DEBUG_PRINT(("\nPlay Function received\n\n"));
			break;
		case MHL_RCP_CMD_PAUSE_PLAY_FUNC:
			keycode = KEY_PAUSECD;
			TX_DEBUG_PRINT(("\nPause_Play Function received\n\n"));
			break;
		case MHL_RCP_CMD_STOP_FUNC:
			keycode = KEY_STOP;
			TX_DEBUG_PRINT(("\nStop Function received\n\n"));
			break;
		case MHL_RCP_CMD_F1:
			keycode = KEY_F1;
			TX_DEBUG_PRINT(("\nF1 received\n\n"));
			break;
		case MHL_RCP_CMD_F2:
			keycode = KEY_F2;
			TX_DEBUG_PRINT(("\nF2 received\n\n"));
			break;
		case MHL_RCP_CMD_F3:
			keycode = KEY_F3;
			TX_DEBUG_PRINT(("\nF3 received\n\n"));
			break;
		case MHL_RCP_CMD_F4:
			keycode = KEY_F4;
			TX_DEBUG_PRINT(("\nF4 received\n\n"));
			break;
		case MHL_RCP_CMD_F5:
			keycode = KEY_F5;
			TX_DEBUG_PRINT(("\nF5 received\n\n"));
			break;
		default:
			break;
		}


#if defined(CONFIG_TEGRA_HDMI_MHL_RCP)
		rcp_report_event(EV_KEY, keycode, 1);
		rcp_report_event(EV_KEY, keycode, 0);
#endif
		if (rcpKeyCode >= 0) {
		SiiMhlTxRcpkSend((uint8_t) rcpKeyCode);
		rcpKeyCode = -1; }
		break;
	case MHL_TX_EVENT_RCPK_RECEIVED:
		TX_DEBUG_PRINT(("App: Received an RCPK = %02X\n", (int)eventParam));
#ifdef ENABLE_WRITE_BURST_TEST
		if ((APP_DEMO_RCP_SEND_KEY_CODE == eventParam) && testEnable) {
			testStart = 1;
			TX_DEBUG_PRINT(("App:%d Write Burst test Starting:...\n", (int)__LINE__)); }
#endif
	case MHL_TX_EVENT_RCPE_RECEIVED:
		TX_DEBUG_PRINT(("App: Received an RCPE = %02X\n", (int)eventParam));
		break;
	case MHL_TX_EVENT_DCAP_CHG:
		{
		uint8_t i, myData = 0;
		TX_DEBUG_PRINT(("App: MHL_TX_EVENT_DCAP_CHG: "));
		for (i = 0; i < 16; ++i) {
			if (0 == SiiTxGetPeerDevCapEntry(i, &myData))
				TX_DEBUG_PRINT(("0x%02x ", (int)myData));
			else
				TX_DEBUG_PRINT(("busy "));
			}
			TX_DEBUG_PRINT(("\n"));
		}
		break;
	case MHL_TX_EVENT_DSCR_CHG:
		{
			ScratchPadStatus_e temp;
			uint8_t myData[16];
			temp = SiiGetScratchPadVector(0, sizeof(myData), myData);
			switch (temp) {
			case SCRATCHPAD_FAIL:
			case SCRATCHPAD_NOT_SUPPORTED:
			case SCRATCHPAD_BUSY:
				TX_DEBUG_PRINT(("SiiGetScratchPadVector returned 0x%02x\n", (int)temp));
				break;
			case SCRATCHPAD_SUCCESS:
				{
					uint8_t i;
					TX_DEBUG_PRINT(("New ScratchPad: "));
					for (i = 0; i < sizeof(myData); ++i) {
							TX_DEBUG_PRINT(("(%02x, %c) \n", (int)temp, (char)temp));
						}
						TX_DEBUG_PRINT(("\n"));
					}
				break;
			default:
				break;
			}
		}
		break;
#ifdef BYPASS_VBUS_HW_SUPPORT
	case MHL_TX_EVENT_POW_BIT_CHG:
		if (eventParam) {
		}
		retVal = MHL_TX_EVENT_STATUS_HANDLED;
		break;
	case MHL_TX_EVENT_RGND_MHL:

		retVal = MHL_TX_EVENT_STATUS_HANDLED;
		break;
#else
	case MHL_TX_EVENT_POW_BIT_CHG:
	case MHL_TX_EVENT_RGND_MHL:

		break;
#endif
	default:
		TX_DEBUG_PRINT(("Unknown event: 0x%02x\n",
				(int)eventCode));
	}

		return retVal;
}
