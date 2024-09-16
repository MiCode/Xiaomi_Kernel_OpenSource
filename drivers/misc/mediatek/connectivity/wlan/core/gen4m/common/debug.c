#include "precomp.h"


#if (CFG_SUPPORT_STATISTICS == 1)

#define WAKE_MAX_CMD_EVENT_NUM		20
#define WAKE_STR_BUFFER_LEN	(60 + 20 * WAKE_MAX_CMD_EVENT_NUM)

struct WAKE_CMD_T {
	uint8_t ucCmdId;
	uint8_t ucFlagIsUesd;
	uint16_t u2Cnt;
};

struct WAKE_EVENT_T {
	uint8_t ucEventId;
	uint8_t ucFlagIsUesd;
	uint16_t u2Cnt;
};

struct WAKE_INFO_T {
	struct WAKE_CMD_T arCmd[WAKE_MAX_CMD_EVENT_NUM];
	uint8_t ucCmdCnt;
	uint32_t u4TotalCmd;

	struct WAKE_EVENT_T arEvent[WAKE_MAX_CMD_EVENT_NUM];
	uint8_t ucEventCnt;
	uint32_t u4TotalEvent;

	uint32_t au4TxDataCnt[WLAN_WAKE_MAX_NUM];
	uint32_t u4TxCnt;
	uint32_t au4RxDataCnt[WLAN_WAKE_MAX_NUM];
	uint32_t u4RxCnt;
};

static struct WAKE_INFO_T *gprWakeInfoStatics;
static uint8_t aucStr[WAKE_STR_BUFFER_LEN];
#endif

#if (CFG_SUPPORT_TRACE_TC4 == 1)
struct COMMAND {
	uint8_t ucCID;
	u_int8_t fgSetQuery;
	u_int8_t fgNeedResp;
	uint8_t ucCmdSeqNum;
};

struct SECURITY_FRAME {
	uint16_t u2EthType;
	uint16_t u2Reserved;
};

struct MGMT_FRAME {
	uint16_t u2FrameCtl;
	uint16_t u2DurationID;
};

struct TC_RES_RELEASE_ENTRY {
	uint64_t u8RelaseTime;
	uint32_t u4RelCID;
	uint32_t u4Tc4RelCnt;
	uint32_t u4AvailableTc4;
};

struct CMD_TRACE_ENTRY {
	uint64_t u8TxTime;
	enum COMMAND_TYPE eCmdType;
	union {
		struct COMMAND rCmd;
		struct SECURITY_FRAME rSecFrame;
		struct MGMT_FRAME rMgmtFrame;
	} u;
};

#define TC_RELEASE_TRACE_BUF_MAX_NUM 100
#define TXED_CMD_TRACE_BUF_MAX_NUM 100

static struct TC_RES_RELEASE_ENTRY *gprTcReleaseTraceBuffer;
static struct CMD_TRACE_ENTRY *gprCmdTraceEntry;
void wlanDebugTC4Init(void)
{
	/* debug for command/tc4 resource begin */
	gprTcReleaseTraceBuffer =
		kalMemAlloc(TC_RELEASE_TRACE_BUF_MAX_NUM * sizeof(
				    struct TC_RES_RELEASE_ENTRY), PHY_MEM_TYPE);
	kalMemZero(gprTcReleaseTraceBuffer,
		   TC_RELEASE_TRACE_BUF_MAX_NUM * sizeof(struct
				   TC_RES_RELEASE_ENTRY));
	gprCmdTraceEntry = kalMemAlloc(TXED_CMD_TRACE_BUF_MAX_NUM *
				       sizeof(struct CMD_TRACE_ENTRY),
					   PHY_MEM_TYPE);
	kalMemZero(gprCmdTraceEntry,
		   TXED_CMD_TRACE_BUF_MAX_NUM * sizeof(struct
				   CMD_TRACE_ENTRY));
	/* debug for command/tc4 resource end */
}

void wlanDebugTC4Uninit(void)
{
	/* debug for command/tc4 resource begin */
	kalMemFree(gprTcReleaseTraceBuffer, PHY_MEM_TYPE,
		   TC_RELEASE_TRACE_BUF_MAX_NUM * sizeof(struct
				   TC_RES_RELEASE_ENTRY));
	kalMemFree(gprCmdTraceEntry, PHY_MEM_TYPE,
		   TXED_CMD_TRACE_BUF_MAX_NUM * sizeof(struct
				   CMD_TRACE_ENTRY));
	/* debug for command/tc4 resource end */
}

void wlanTraceTxCmd(struct CMD_INFO *prCmd)
{
	static uint16_t u2CurEntry;
	struct CMD_TRACE_ENTRY *prCurCmd =
			&gprCmdTraceEntry[u2CurEntry];

	prCurCmd->u8TxTime = sched_clock();
	prCurCmd->eCmdType = prCmd->eCmdType;
	if (prCmd->eCmdType == COMMAND_TYPE_MANAGEMENT_FRAME) {
		struct WLAN_MAC_MGMT_HEADER *prMgmt = (struct
			WLAN_MAC_MGMT_HEADER *)prCmd->prMsduInfo->prPacket;

		prCurCmd->u.rMgmtFrame.u2FrameCtl = prMgmt->u2FrameCtrl;
		prCurCmd->u.rMgmtFrame.u2DurationID = prMgmt->u2Duration;
	} else if (prCmd->eCmdType == COMMAND_TYPE_SECURITY_FRAME ||
			prCmd->eCmdType == COMMAND_TYPE_DATA_FRAME) {
		uint8_t *pucPkt = (uint8_t *)((struct sk_buff *)
					      prCmd->prPacket)->data;

		prCurCmd->u.rSecFrame.u2EthType =
			(pucPkt[ETH_TYPE_LEN_OFFSET] << 8) |
			(pucPkt[ETH_TYPE_LEN_OFFSET + 1]);
	} else {
		prCurCmd->u.rCmd.ucCID = prCmd->ucCID;
		prCurCmd->u.rCmd.ucCmdSeqNum = prCmd->ucCmdSeqNum;
		prCurCmd->u.rCmd.fgNeedResp = prCmd->fgNeedResp;
		prCurCmd->u.rCmd.fgSetQuery = prCmd->fgSetQuery;
	}
	u2CurEntry++;
	if (u2CurEntry == TC_RELEASE_TRACE_BUF_MAX_NUM)
		u2CurEntry = 0;
}

void wlanTraceReleaseTcRes(struct ADAPTER *prAdapter,
			   uint32_t u4TxRlsCnt, uint32_t u4Available)
{
	static uint16_t u2CurEntry;
	struct TC_RES_RELEASE_ENTRY *prCurBuf =
			&gprTcReleaseTraceBuffer[u2CurEntry];

	prCurBuf->u8RelaseTime = sched_clock();
	prCurBuf->u4Tc4RelCnt =  u4TxRlsCnt;
	prCurBuf->u4AvailableTc4 = u4Available;
	u2CurEntry++;
	if (u2CurEntry == TXED_CMD_TRACE_BUF_MAX_NUM)
		u2CurEntry = 0;
}

void wlanDumpTcResAndTxedCmd(uint8_t *pucBuf,
			     uint32_t maxLen)
{
	uint16_t i = 0;
	struct TC_RES_RELEASE_ENTRY *prTcRel =
			gprTcReleaseTraceBuffer;
	struct CMD_TRACE_ENTRY *prCmd = gprCmdTraceEntry;

	if (pucBuf) {
		int bufLen = 0;

		for (; i < TXED_CMD_TRACE_BUF_MAX_NUM / 2; i++) {
			bufLen = snprintf(pucBuf, maxLen,
				  "%d: Time %llu, Type %d, Content %08x; %d: Time %llu, Type %d, Content %08x\n",
				  i * 2, prCmd[i * 2].u8TxTime,
				  prCmd[i * 2].eCmdType,
				  *(uint32_t *)
					(&prCmd[i * 2].u.rCmd.ucCID),
				  i * 2 + 1,
				  prCmd[i * 2 + 1].u8TxTime,
				  prCmd[i * 2 + 1].eCmdType,
				  *(uint32_t *)
					(&prCmd[i * 2 + 1].u.rCmd.ucCID));
			if (bufLen <= 0)
				break;
			pucBuf += bufLen;
			maxLen -= bufLen;
		}
		for (i = 0; i < TC_RELEASE_TRACE_BUF_MAX_NUM / 2; i++) {
			bufLen = snprintf(pucBuf, maxLen,
				  "%d: Time %llu, Tc4Cnt %d, Free %d, CID %08x; %d: Time %llu, Tc4Cnt %d, Free %d CID %08x\n",
				  i * 2, prTcRel[i * 2].u8RelaseTime,
				  prTcRel[i * 2].u4Tc4RelCnt,
				  prTcRel[i * 2].u4AvailableTc4,
				  prTcRel[i * 2].u4RelCID,
				  i * 2 + 1,
				  prTcRel[i * 2 + 1].u8RelaseTime,
				  prTcRel[i * 2 + 1].u4Tc4RelCnt,
				  prTcRel[i * 2 + 1].u4AvailableTc4,
				  prTcRel[i * 2 + 1].u4RelCID);
			if (bufLen <= 0)
				break;
			pucBuf += bufLen;
			maxLen -= bufLen;
		}
	} else {
		for (; i < TXED_CMD_TRACE_BUF_MAX_NUM / 4; i++) {
			LOG_FUNC(
				 "%d: Time %llu, Type %d, Content %08x; %d: Time %llu, Type %d, Content %08x; ",
				 i * 4, prCmd[i * 4].u8TxTime,
				 prCmd[i * 4].eCmdType,
				 *(uint32_t *)(&prCmd[i * 4].u.rCmd.ucCID),
				 i * 4 + 1, prCmd[i * 4 + 1].u8TxTime,
				 prCmd[i * 4 + 1].eCmdType,
				 *(uint32_t *)(&prCmd[i * 4 + 1].u.rCmd.ucCID));
			LOG_FUNC(
				 "%d: Time %llu, Type %d, Content %08x; %d: Time %llu, Type %d, Content %08x\n",
				 i * 4 + 2, prCmd[i * 4 + 2].u8TxTime,
				 prCmd[i * 4 + 2].eCmdType,
				 *(uint32_t *)(&prCmd[i * 4 + 2].u.rCmd.ucCID),
				 i * 4 + 3, prCmd[i * 4 + 3].u8TxTime,
				 prCmd[i * 4 + 3].eCmdType,
				 *(uint32_t *)(&prCmd[i * 4 + 3].u.rCmd.ucCID));
		}
		for (i = 0; i < TC_RELEASE_TRACE_BUF_MAX_NUM / 4; i++) {
			LOG_FUNC(
				"%d: Time %llu, Tc4Cnt %d, Free %d, CID %08x; %d: Time %llu, Tc4Cnt %d, Free %d, CID %08x;",
				i * 4, prTcRel[i * 4].u8RelaseTime,
				prTcRel[i * 4].u4Tc4RelCnt,
				prTcRel[i * 4].u4AvailableTc4,
				prTcRel[i * 4].u4RelCID,
				i * 4 + 1, prTcRel[i * 4 + 1].u8RelaseTime,
				prTcRel[i * 4 + 1].u4Tc4RelCnt,
				prTcRel[i * 4 + 1].u4AvailableTc4,
				prTcRel[i * 4 + 1].u4RelCID);
			LOG_FUNC(
				"%d: Time %llu, Tc4Cnt %d, Free %d, CID %08x; %d: Time %llu, Tc4Cnt %d, Free %d, CID %08x\n",
				i * 4 + 2, prTcRel[i * 4 + 2].u8RelaseTime,
				prTcRel[i * 4 + 2].u4Tc4RelCnt,
				prTcRel[i * 4 + 2].u4AvailableTc4,
				prTcRel[i * 4 + 2].u4RelCID,
				i * 4 + 3, prTcRel[i * 4 + 3].u8RelaseTime,
				prTcRel[i * 4 + 3].u4Tc4RelCnt,
				prTcRel[i * 4 + 3].u4AvailableTc4,
				prTcRel[i * 4 + 3].u4RelCID);
		}
	}
}
#endif


#if (CFG_SUPPORT_STATISTICS == 1)

void wlanWakeStaticsInit(void)
{
	gprWakeInfoStatics =
		kalMemAlloc(WAKE_MAX_CMD_EVENT_NUM * sizeof(
				    struct WAKE_INFO_T), PHY_MEM_TYPE);
	if (gprWakeInfoStatics != NULL)
		kalMemZero(gprWakeInfoStatics,
		   WAKE_MAX_CMD_EVENT_NUM * sizeof(struct
				   WAKE_INFO_T));
}

void wlanWakeStaticsUninit(void)
{
	if (gprWakeInfoStatics != NULL)
		kalMemFree(gprWakeInfoStatics, PHY_MEM_TYPE,
		WAKE_MAX_CMD_EVENT_NUM * sizeof(struct WAKE_INFO_T));
}

uint32_t wlanWakeLogCmd(uint8_t ucCmdId)
{
	int i = 0;
	int j = 0;

	if ((gprWakeInfoStatics == NULL) || (wlan_fb_power_down != TRUE))
		return 1;

	for (i = 0; i < WAKE_MAX_CMD_EVENT_NUM; i++) {
		if ((gprWakeInfoStatics->arCmd[i].ucFlagIsUesd == TRUE)
			&& (gprWakeInfoStatics->arCmd[i].ucCmdId == ucCmdId)) {
			/*old item ++*/
			gprWakeInfoStatics->arCmd[i].u2Cnt++;
			gprWakeInfoStatics->u4TotalCmd++;
			break;
		}
	}

	if (i >= WAKE_MAX_CMD_EVENT_NUM) {
		/*add new item*/
		for (j = 0; j < WAKE_MAX_CMD_EVENT_NUM; j++) {
			if (gprWakeInfoStatics->arCmd[j].ucFlagIsUesd != TRUE) {
				gprWakeInfoStatics->ucCmdCnt++;
				gprWakeInfoStatics->arCmd[j].ucCmdId = ucCmdId;
				gprWakeInfoStatics->arCmd[j].u2Cnt++;
				gprWakeInfoStatics->u4TotalCmd++;
				gprWakeInfoStatics->arCmd[j].ucFlagIsUesd
					= TRUE;
				break;
			}
		}

		if (j >= WAKE_MAX_CMD_EVENT_NUM) {
			DBGLOG_LIMITED(OID, WARN,
			"Wake cmd over flow %d-0x%02x\n",
			WAKE_MAX_CMD_EVENT_NUM, ucCmdId);
		}
	}
	return 0;
}

uint32_t wlanWakeLogEvent(uint8_t ucEventId)
{
	int i = 0;
	int j = 0;

	if ((gprWakeInfoStatics == NULL) || (wlan_fb_power_down != TRUE))
		return 1;

	for (i = 0; i < WAKE_MAX_CMD_EVENT_NUM; i++) {
		if ((gprWakeInfoStatics->arEvent[i].ucFlagIsUesd == TRUE)
		&&
		(gprWakeInfoStatics->arEvent[i].ucEventId == ucEventId)) {
			/*old item ++*/
			gprWakeInfoStatics->arEvent[i].u2Cnt++;
			gprWakeInfoStatics->u4TotalEvent++;
			break;
		}
	}

	if (i >= WAKE_MAX_CMD_EVENT_NUM) {
		/*add new item*/
		for (j = 0; j < WAKE_MAX_CMD_EVENT_NUM; j++) {
			if (gprWakeInfoStatics->arEvent[j].ucFlagIsUesd
				!= TRUE) {
				gprWakeInfoStatics->ucEventCnt++;
				gprWakeInfoStatics->arEvent[j].ucEventId
					= ucEventId;
				gprWakeInfoStatics->arEvent[j].u2Cnt++;
				gprWakeInfoStatics->u4TotalEvent++;
				gprWakeInfoStatics->arEvent[j].ucFlagIsUesd
					= TRUE;
				break;
			}
		}

		if (j >= WAKE_MAX_CMD_EVENT_NUM) {
			DBGLOG(OID, WARN,
			"Wake event over flow %d-0x%02x\n",
			WAKE_MAX_CMD_EVENT_NUM, ucEventId);
		}
	}
	return 0;
}

void wlanLogTxData(enum WAKE_DATA_TYPE dataType)
{
	if ((gprWakeInfoStatics != NULL) && (wlan_fb_power_down == TRUE)) {
		gprWakeInfoStatics->au4TxDataCnt[dataType]++;
		gprWakeInfoStatics->u4TxCnt++;
	}
}

void wlanLogRxData(enum WAKE_DATA_TYPE dataType)
{
	if ((gprWakeInfoStatics != NULL) && (wlan_fb_power_down == TRUE)) {
		gprWakeInfoStatics->au4RxDataCnt[dataType]++;
		gprWakeInfoStatics->u4RxCnt++;
	}
}

static void wlanWakeStaticsClear(void)
{
	if (gprWakeInfoStatics != NULL) {
		kalMemZero(gprWakeInfoStatics,
			WAKE_MAX_CMD_EVENT_NUM * sizeof(struct
				   WAKE_INFO_T));
	}
}

uint32_t wlanWakeDumpRes(void)
{
	uint8_t i = 0;
	uint8_t flag = 0;
	char *pos = NULL;
	char *end = NULL;
	int ret = 0;

	if ((gprWakeInfoStatics == NULL)
	|| (wlan_fb_power_down != TRUE)) {
		wlanWakeStaticsClear();
		return 1;
	}

	/*Log Style: one line log or human friendly log.*/
#if 1
	kalMemZero(&aucStr[0], sizeof(uint8_t)*WAKE_STR_BUFFER_LEN);
	pos = &aucStr[0];
	end = &aucStr[0] + WAKE_STR_BUFFER_LEN - 1;

	if (gprWakeInfoStatics->ucCmdCnt > 0) {
		flag = 1;
		ret = snprintf(pos, (end - pos + 1), "CMD(%u:%u)= ",
			gprWakeInfoStatics->ucCmdCnt,
			gprWakeInfoStatics->u4TotalCmd);
		if (ret < 0 || ret >= (end - pos + 1))
			return 1;
		pos += ret;

		for (i = 0; i < gprWakeInfoStatics->ucCmdCnt; i++) {
			ret = snprintf(pos, (end - pos + 1), "0x%02x-%d ",
				gprWakeInfoStatics->arCmd[i].ucCmdId,
				gprWakeInfoStatics->arCmd[i].u2Cnt);
			if (ret < 0 || ret >= (end - pos + 1))
				return 1;
			pos += ret;
		}
	}

	if (gprWakeInfoStatics->ucEventCnt > 0) {
		flag = 1;
		ret = snprintf(pos, (end - pos + 1), "EVENT(%u:%u)= ",
			gprWakeInfoStatics->ucEventCnt,
			gprWakeInfoStatics->u4TotalEvent);
		if (ret < 0 || ret >= (end - pos + 1))
			return 1;
		pos += ret;

		for (i = 0; i < gprWakeInfoStatics->ucEventCnt; i++) {
			ret = snprintf(pos, (end - pos + 1), "0x%02x-%d ",
				gprWakeInfoStatics->arEvent[i].ucEventId,
				gprWakeInfoStatics->arEvent[i].u2Cnt);
			if (ret < 0 || ret >= (end - pos + 1))
				return 1;
			pos += ret;
		}
	}

	if (gprWakeInfoStatics->u4TxCnt > 0) {
		flag = 1;
		ret = snprintf(pos, (end - pos + 1),
			"TX(%u)=%u-%u-%u-%u-%u-%u ",
			gprWakeInfoStatics->u4TxCnt,
			gprWakeInfoStatics->au4TxDataCnt[WLAN_WAKE_ARP],
			gprWakeInfoStatics->au4TxDataCnt[WLAN_WAKE_IPV4],
			gprWakeInfoStatics->au4TxDataCnt[WLAN_WAKE_IPV6],
			gprWakeInfoStatics->au4TxDataCnt[WLAN_WAKE_1X],
			gprWakeInfoStatics->au4TxDataCnt[WLAN_WAKE_TDLS],
			gprWakeInfoStatics->au4TxDataCnt[WLAN_WAKE_OTHER]);

		if (ret < 0 || ret >= (end - pos + 1))
			return 1;
		pos += ret;
	}

	if (gprWakeInfoStatics->u4RxCnt > 0) {
		flag = 1;
		ret = snprintf(pos, (end - pos + 1),
			"RX(%u)=%u-%u-%u-%u-%u-%u ",
			gprWakeInfoStatics->u4RxCnt,
			gprWakeInfoStatics->au4RxDataCnt[WLAN_WAKE_ARP],
			gprWakeInfoStatics->au4RxDataCnt[WLAN_WAKE_IPV4],
			gprWakeInfoStatics->au4RxDataCnt[WLAN_WAKE_IPV6],
			gprWakeInfoStatics->au4RxDataCnt[WLAN_WAKE_1X],
			gprWakeInfoStatics->au4RxDataCnt[WLAN_WAKE_TDLS],
			gprWakeInfoStatics->au4RxDataCnt[WLAN_WAKE_OTHER]);
		if (ret < 0 || ret >= (end - pos + 1))
			return 1;
		pos += ret;
	}

	if (flag != 0)
		DBGLOG(OID, INFO, "[WLAN-LP] %s\n", (char *)&aucStr[0]);
#else
	/*1.dump cmd*/
	if (gprWakeInfoStatics->ucCmdCnt > 0) {
		kalMemZero(&aucStr[0], sizeof(uint8_t)*WAKE_STR_BUFFER_LEN);
		pos = &aucStr[0];
		end = &aucStr[0] + WAKE_STR_BUFFER_LEN - 1;
		for (i = 0; i < gprWakeInfoStatics->ucCmdCnt; i++) {

			ret = snprintf(pos, end - pos, " 0x%02x ",
			gprWakeInfoStatics->arCmd[i].ucCmdId);
			if (ret < 0 || ret >= end - pos)
				return 1;
			pos += ret;
		}
		DBGLOG(OID, INFO, "[LP-CMD-ID-%u][%s]\n",
			gprWakeInfoStatics->ucCmdCnt, (char *)&aucStr[0]);

		kalMemZero(&aucStr[0], sizeof(uint8_t)*WAKE_STR_BUFFER_LEN);
		pos = &aucStr[0];
		end = &aucStr[0] + WAKE_STR_BUFFER_LEN - 1;
		for (i = 0; i < gprWakeInfoStatics->ucCmdCnt; i++) {

			ret = snprintf(pos, end - pos, " %u ",
				gprWakeInfoStatics->arCmd[i].u2Cnt);
			if (ret < 0 || ret >= end - pos)
				return 1;
			pos += ret;
		}
		DBGLOG(OID, INFO, "[LP-CMD-CNT-%u][%s]\n",
			gprWakeInfoStatics->u4TotalCmd, (char *)&aucStr[0]);
	}

	/*2.dump event*/
	if (gprWakeInfoStatics->ucCmdCnt > 0) {

		kalMemZero(&aucStr[0], sizeof(uint8_t)*WAKE_STR_BUFFER_LEN);
		pos = &aucStr[0];
		end = &aucStr[0] + WAKE_STR_BUFFER_LEN - 1;
		for (i = 0; i < gprWakeInfoStatics->ucEventCnt; i++) {

			ret = snprintf(pos, end - pos, " 0x%02x ",
				gprWakeInfoStatics->arEvent[i].ucEventId);
			if (ret < 0 || ret >= end - pos)
				return 1;
			pos += ret;
		}
		DBGLOG(OID, INFO, "[LP-EVENT-ID-%u][%s]\n",
			gprWakeInfoStatics->ucEventCnt, (char *)&aucStr[0]);

		kalMemZero(&aucStr[0], sizeof(uint8_t)*WAKE_STR_BUFFER_LEN);
		pos = &aucStr[0];
		end = &aucStr[0] + WAKE_STR_BUFFER_LEN - 1;
		for (i = 0; i < gprWakeInfoStatics->ucEventCnt; i++) {

			ret = snprintf(pos, end - pos, " %u ",
				gprWakeInfoStatics->arEvent[i].u2Cnt);
			if (ret < 0 || ret >= end - pos) {
				end[-1] = '\0';
				return 1;
			}
			pos += ret;
		}
		DBGLOG(OID, INFO, "[LP-EVENT-CNT-%u][%s]\n",
			gprWakeInfoStatics->u4TotalEvent, (char *)&aucStr[0]);
	}

	/*3.dump tx/rx data*/
	if (gprWakeInfoStatics->u4TxCnt > 0) {
		DBGLOG(OID, INFO, "[LP-EVENT-TX-%u][%u-%u-%u-%u-%u-%u]\n",
			gprWakeInfoStatics->u4TxCnt,
			gprWakeInfoStatics->au4TxDataCnt[WLAN_WAKE_ARP],
			gprWakeInfoStatics->au4TxDataCnt[WLAN_WAKE_IPV4],
			gprWakeInfoStatics->au4TxDataCnt[WLAN_WAKE_IPV6],
			gprWakeInfoStatics->au4TxDataCnt[WLAN_WAKE_1X],
			gprWakeInfoStatics->au4TxDataCnt[WLAN_WAKE_TDLS],
			gprWakeInfoStatics->au4TxDataCnt[WLAN_WAKE_OTHER]);
	}

	if (gprWakeInfoStatics->u4RxCnt > 0) {
		DBGLOG(OID, INFO, "[LP-EVENT-RX-%u][%u-%u-%u-%u-%u-%u]\n",
			gprWakeInfoStatics->u4RxCnt,
			gprWakeInfoStatics->au4RxDataCnt[WLAN_WAKE_ARP],
			gprWakeInfoStatics->au4RxDataCnt[WLAN_WAKE_IPV4],
			gprWakeInfoStatics->au4RxDataCnt[WLAN_WAKE_IPV6],
			gprWakeInfoStatics->au4RxDataCnt[WLAN_WAKE_1X],
			gprWakeInfoStatics->au4RxDataCnt[WLAN_WAKE_TDLS],
			gprWakeInfoStatics->au4RxDataCnt[WLAN_WAKE_OTHER]);
	}
#endif
	wlanWakeStaticsClear();
	return 0;
}

#endif

uint32_t wlanSetDriverDbgLevel(IN uint32_t u4DbgIdx, IN uint32_t u4DbgMask)
{
	uint32_t u4Idx;
	uint32_t fgStatus = WLAN_STATUS_SUCCESS;

	if (u4DbgIdx == DBG_ALL_MODULE_IDX) {
		for (u4Idx = 0; u4Idx < DBG_MODULE_NUM; u4Idx++)
			aucDebugModule[u4Idx] = (uint8_t) u4DbgMask;
		LOG_FUNC("Set ALL DBG module log level to [0x%02x]\n",
				u4DbgMask);
	} else if (u4DbgIdx < DBG_MODULE_NUM) {
		aucDebugModule[u4DbgIdx] = (uint8_t) u4DbgMask;
		LOG_FUNC("Set DBG module[%u] log level to [0x%02x]\n",
				u4DbgIdx, u4DbgMask);
	} else {
		fgStatus = WLAN_STATUS_FAILURE;
	}

	if (fgStatus == WLAN_STATUS_SUCCESS)
		wlanDriverDbgLevelSync();

	return fgStatus;
}

uint32_t wlanGetDriverDbgLevel(IN uint32_t u4DbgIdx, OUT uint32_t *pu4DbgMask)
{
	if (u4DbgIdx < DBG_MODULE_NUM) {
		*pu4DbgMask = aucDebugModule[u4DbgIdx];
		return WLAN_STATUS_SUCCESS;
	}

	return WLAN_STATUS_FAILURE;
}

uint32_t wlanDbgLevelUiSupport(IN struct ADAPTER *prAdapter, uint32_t u4Version,
		uint32_t ucModule)
{
	uint32_t u4Enable = ENUM_WIFI_LOG_LEVEL_SUPPORT_DISABLE;

	switch (u4Version) {
	case ENUM_WIFI_LOG_LEVEL_VERSION_V1:
		switch (ucModule) {
		case ENUM_WIFI_LOG_MODULE_DRIVER:
			u4Enable = ENUM_WIFI_LOG_LEVEL_SUPPORT_ENABLE;
			break;
		case ENUM_WIFI_LOG_MODULE_FW:
			u4Enable = ENUM_WIFI_LOG_LEVEL_SUPPORT_ENABLE;
			break;
		}
		break;
	default:
		break;
	}

	return u4Enable;
}

uint32_t wlanDbgGetLogLevelImpl(IN struct ADAPTER *prAdapter,
		uint32_t u4Version, uint32_t ucModule)
{
	uint32_t u4Level = ENUM_WIFI_LOG_LEVEL_DEFAULT;

	switch (u4Version) {
	case ENUM_WIFI_LOG_LEVEL_VERSION_V1:
		wlanDbgGetGlobalLogLevel(ucModule, &u4Level);
		break;
	default:
		break;
	}

	return u4Level;
}

void wlanDbgSetLogLevelImpl(IN struct ADAPTER *prAdapter,
		uint32_t u4Version, uint32_t u4Module, uint32_t u4level)
{
	uint32_t u4DriverLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;
	uint32_t u4FwLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;

	if (u4level >= ENUM_WIFI_LOG_LEVEL_NUM)
		return;

	switch (u4Version) {
	case ENUM_WIFI_LOG_LEVEL_VERSION_V1:
		wlanDbgSetGlobalLogLevel(u4Module, u4level);
		switch (u4Module) {
		case ENUM_WIFI_LOG_MODULE_DRIVER:
		{
			uint32_t u4DriverLogMask;

			if (u4level == ENUM_WIFI_LOG_LEVEL_DEFAULT)
				u4DriverLogMask = DBG_LOG_LEVEL_DEFAULT;
			else if (u4level == ENUM_WIFI_LOG_LEVEL_MORE)
				u4DriverLogMask = DBG_LOG_LEVEL_MORE;
			else
				u4DriverLogMask = DBG_LOG_LEVEL_EXTREME;

			wlanSetDriverDbgLevel(DBG_ALL_MODULE_IDX,
					(u4DriverLogMask & DBG_CLASS_MASK));
		}
			break;
		case ENUM_WIFI_LOG_MODULE_FW:
		{
			struct CMD_EVENT_LOG_UI_INFO cmd;

			kalMemZero(&cmd,
					sizeof(struct CMD_EVENT_LOG_UI_INFO));
			cmd.ucVersion = u4Version;
			cmd.ucLogLevel = u4level;

			wlanSendSetQueryCmd(prAdapter,
					CMD_ID_LOG_UI_INFO,
					TRUE,
					FALSE,
					FALSE,
					nicCmdEventSetCommon,
					nicOidCmdTimeoutCommon,
					sizeof(struct CMD_EVENT_LOG_UI_INFO),
					(uint8_t *)&cmd,
					NULL,
					0);
		}
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	wlanDbgGetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_DRIVER, &u4DriverLevel);
	wlanDbgGetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_FW, &u4FwLevel);
#if KERNEL_VERSION(4, 14, 0) >= LINUX_VERSION_CODE
#if (CFG_BUILT_IN_DRIVER == 0) && (CFG_MTK_ANDROID_WMT == 1)
	/*
	 * The function definition of get_logtoomuch_enable() and
	 * set_logtoomuch_enable of Android O0 or lower version are different
	 * from that of Android O1 or higher version. Wlan driver supports .ko
	 * module from Android O1. Use CFG_BUILT_IN_DRIVER to distinguish
	 * Android version higher than O1 instead.
	 */
	if ((u4DriverLevel > ENUM_WIFI_LOG_LEVEL_DEFAULT ||
			u4FwLevel > ENUM_WIFI_LOG_LEVEL_DEFAULT) &&
			get_logtoomuch_enable()) {
		DBGLOG(OID, TRACE,
			"Disable printk to much. driver: %d, fw: %d\n",
			u4DriverLevel,
			u4FwLevel);
		set_logtoomuch_enable(0);
	}
#endif
#endif /* KERNEL_VERSION(4, 14, 0) >= LINUX_VERSION_CODE */
}

u_int8_t wlanDbgGetGlobalLogLevel(uint32_t u4Module, uint32_t *pu4Level)
{
	if (u4Module != ENUM_WIFI_LOG_MODULE_DRIVER &&
			u4Module != ENUM_WIFI_LOG_MODULE_FW)
		return FALSE;

	*pu4Level = au4LogLevel[u4Module];
	return TRUE;
}

u_int8_t wlanDbgSetGlobalLogLevel(uint32_t u4Module, uint32_t u4Level)
{
	if (u4Module != ENUM_WIFI_LOG_MODULE_DRIVER &&
			u4Module != ENUM_WIFI_LOG_MODULE_FW)
		return FALSE;

	au4LogLevel[u4Module] = u4Level;
	return TRUE;
}

void wlanDriverDbgLevelSync(void)
{
	uint8_t i = 0;
	uint32_t u4Mask = DBG_CLASS_MASK;
	uint32_t u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;

	/* get the lowest level as module's level */
	for (i = 0; i < DBG_MODULE_NUM; i++)
		u4Mask &= aucDebugModule[i];

	if ((u4Mask & DBG_LOG_LEVEL_EXTREME) == DBG_LOG_LEVEL_EXTREME)
		u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_EXTREME;
	else if ((u4Mask & DBG_LOG_LEVEL_MORE) == DBG_LOG_LEVEL_MORE)
		u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_MORE;
	else
		u4DriverLogLevel = ENUM_WIFI_LOG_LEVEL_DEFAULT;

	wlanDbgSetGlobalLogLevel(ENUM_WIFI_LOG_MODULE_DRIVER, u4DriverLogLevel);
}

static void
firmwareHexDump(const uint8_t *pucPreFix,
		int32_t i4PreFixType,
		int32_t i4RowSize, int32_t i4GroupSize,
		const void *pvBuf, size_t len, u_int8_t fgAscii)
{
#define OLD_KBUILD_MODNAME KBUILD_MODNAME
#undef KBUILD_MODNAME
#define KBUILD_MODNAME "wlan_mt6632_fw"

	const uint8_t *pucPtr = pvBuf;
	int32_t i, i4LineLen, i4Remaining = len;
	uint8_t ucLineBuf[32 * 3 + 2 + 32 + 1];

	if (i4RowSize != 16 && i4RowSize != 32)
		i4RowSize = 16;

	for (i = 0; i < len; i += i4RowSize) {
		i4LineLen = min(i4Remaining, i4RowSize);
		i4Remaining -= i4RowSize;

		/* use kernel API */
		hex_dump_to_buffer(pucPtr + i, i4LineLen, i4RowSize,
				   i4GroupSize,
				   ucLineBuf, sizeof(ucLineBuf), fgAscii);

		switch (i4PreFixType) {
		case DUMP_PREFIX_ADDRESS:
			pr_info("%s%p: %s\n",
				pucPreFix, pucPtr + i, ucLineBuf);
			break;
		case DUMP_PREFIX_OFFSET:
			pr_info("%s%.8x: %s\n", pucPreFix, i, ucLineBuf);
			break;
		default:
			pr_info("%s%s\n", pucPreFix, ucLineBuf);
			break;
		}
	}
#undef KBUILD_MODNAME
#define KBUILD_MODNAME OLD_KBUILD_MODNAME
}

void wlanPrintFwLog(uint8_t *pucLogContent,
		    uint16_t u2MsgSize, uint8_t ucMsgType,
		    const uint8_t *pucFmt, ...)
{
#define OLD_KBUILD_MODNAME KBUILD_MODNAME
#define OLD_LOG_FUNC LOG_FUNC
#undef KBUILD_MODNAME
#undef LOG_FUNC
#define KBUILD_MODNAME "wlan_mt6632_fw"
#define LOG_FUNC pr_info
#define DBG_LOG_BUF_SIZE 128

	int8_t aucLogBuffer[DBG_LOG_BUF_SIZE];
	int32_t err;
	va_list args;

	if (u2MsgSize > DEBUG_MSG_SIZE_MAX - 1) {
		LOG_FUNC("Firmware Log Size(%d) is too large, type %d\n",
			 u2MsgSize, ucMsgType);
		return;
	}
	switch (ucMsgType) {
	case DEBUG_MSG_TYPE_ASCII: {
		uint8_t *pucChr;

		pucLogContent[u2MsgSize] = '\0';

		/* skip newline */
		pucChr = kalStrChr(pucLogContent, '\0');
		if (*(pucChr - 1) == '\n')
			*(pucChr - 1) = '\0';

		LOG_FUNC("<FW>%s\n", pucLogContent);
	}
	break;
	case DEBUG_MSG_TYPE_DRIVER:
		/* Only 128 Bytes is available to print in driver */
		va_start(args, pucFmt);
		err = vsnprintf(aucLogBuffer, sizeof(aucLogBuffer) - 1, pucFmt,
			  args);
		va_end(args);
		aucLogBuffer[DBG_LOG_BUF_SIZE - 1] = '\0';
		if (err >= 0)
			LOG_FUNC("%s\n", aucLogBuffer);
		break;
	case DEBUG_MSG_TYPE_MEM8:
		firmwareHexDump("fw data:", DUMP_PREFIX_ADDRESS,
				16, 1, pucLogContent, u2MsgSize, true);
		break;
	default:
		firmwareHexDump("fw data:", DUMP_PREFIX_ADDRESS,
				16, 4, pucLogContent, u2MsgSize, true);
		break;
	}

#undef KBUILD_MODNAME
#undef LOG_FUNC
#define KBUILD_MODNAME OLD_KBUILD_MODNAME
#define LOG_FUNC OLD_LOG_FUNC
#undef OLD_KBUILD_MODNAME
#undef OLD_LOG_FUNC
}

/* Begin: Functions used to breakdown packet jitter, for test case VoE 5.7 */
static void wlanSetBE32(uint32_t u4Val, uint8_t *pucBuf)
{
	uint8_t *littleEn = (uint8_t *)&u4Val;

	pucBuf[0] = littleEn[3];
	pucBuf[1] = littleEn[2];
	pucBuf[2] = littleEn[1];
	pucBuf[3] = littleEn[0];
}

void wlanFillTimestamp(struct ADAPTER *prAdapter, void *pvPacket,
		       uint8_t ucPhase)
{
	struct sk_buff *skb = (struct sk_buff *)pvPacket;
	uint8_t *pucEth = NULL;
	uint32_t u4Length = 0;
	uint8_t *pucUdp = NULL;
	struct timeval tval;

	if (!prAdapter || !prAdapter->rDebugInfo.fgVoE5_7Test || !skb)
		return;
	pucEth = skb->data;
	u4Length = skb->len;
	if (u4Length < 200 ||
	    ((pucEth[ETH_TYPE_LEN_OFFSET] << 8) |
	     (pucEth[ETH_TYPE_LEN_OFFSET + 1])) != ETH_P_IPV4)
		return;
	if (pucEth[ETH_HLEN+9] != IP_PRO_UDP)
		return;
	pucUdp = &pucEth[ETH_HLEN+28];
	if (kalStrnCmp(pucUdp, "1345678", 7))
		return;
	do_gettimeofday(&tval);
	switch (ucPhase) {
	case PHASE_XMIT_RCV: /* xmit */
		pucUdp += 20;
		break;
	case PHASE_ENQ_QM: /* enq */
		pucUdp += 28;
		break;
	case PHASE_HIF_TX: /* tx */
		pucUdp += 36;
		break;
	}
	wlanSetBE32(tval.tv_sec, pucUdp);
	wlanSetBE32(tval.tv_usec, pucUdp+4);
}
/* End: Functions used to breakdown packet jitter, for test case VoE 5.7 */
