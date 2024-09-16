/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

/*******************************************************************************
 *            C O M P I L E R	 F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *            E X T E R N A L	R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"

#if (CFG_SUPPORT_STATISTICS == 1)

enum EVENT_TYPE {
	EVENT_RX,
	EVENT_TX,
};
/*******************************************************************************
 *            C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *            F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *            P R I V A T E  F U N C T I O N S
 *******************************************************************************
 */

uint32_t u4TotalTx;
uint32_t u4NoDelayTx;
uint32_t u4TotalRx;
uint32_t u4NoDelayRx;

static uint8_t g_ucTxRxFlag;
static uint8_t g_ucTxIpProto;
static uint16_t g_u2TxUdpPort;
static uint32_t g_u4TxDelayThreshold;
static uint8_t g_ucRxIpProto;
static uint16_t g_u2RxUdpPort;
static uint32_t g_u4RxDelayThreshold;

void StatsResetTxRx(void)
{
	u4TotalRx = 0;
	u4TotalTx = 0;
	u4NoDelayRx = 0;
	u4NoDelayTx = 0;
}

uint64_t StatsEnvTimeGet(void)
{
	uint64_t u8Clk;

	u8Clk = sched_clock();	/* unit: naro seconds */

	return (uint64_t) u8Clk;	/* sched_clock *//* jiffies size = 4B */
}

void StatsEnvGetPktDelay(OUT uint8_t *pucTxRxFlag,
	OUT uint8_t *pucTxIpProto, OUT uint16_t *pu2TxUdpPort,
	OUT uint32_t *pu4TxDelayThreshold, OUT uint8_t *pucRxIpProto,
	OUT uint16_t *pu2RxUdpPort, OUT uint32_t *pu4RxDelayThreshold)
{
	*pucTxRxFlag = g_ucTxRxFlag;
	*pucTxIpProto = g_ucTxIpProto;
	*pu2TxUdpPort = g_u2TxUdpPort;
	*pu4TxDelayThreshold = g_u4TxDelayThreshold;
	*pucRxIpProto = g_ucRxIpProto;
	*pu2RxUdpPort = g_u2RxUdpPort;
	*pu4RxDelayThreshold = g_u4RxDelayThreshold;
}

void StatsEnvSetPktDelay(IN uint8_t ucTxOrRx, IN uint8_t ucIpProto,
	IN uint16_t u2UdpPort, uint32_t u4DelayThreshold)
{
#define MODULE_RESET 0
#define MODULE_TX 1
#define MODULE_RX 2

	if (ucTxOrRx == MODULE_TX) {
		g_ucTxRxFlag |= BIT(0);
		g_ucTxIpProto = ucIpProto;
		g_u2TxUdpPort = u2UdpPort;
		g_u4TxDelayThreshold = u4DelayThreshold;
	} else if (ucTxOrRx == MODULE_RX) {
		g_ucTxRxFlag |= BIT(1);
		g_ucRxIpProto = ucIpProto;
		g_u2RxUdpPort = u2UdpPort;
		g_u4RxDelayThreshold = u4DelayThreshold;
	} else if (ucTxOrRx == MODULE_RESET) {
		g_ucTxRxFlag = 0;
		g_ucTxIpProto = 0;
		g_u2TxUdpPort = 0;
		g_u4TxDelayThreshold = 0;
		g_ucRxIpProto = 0;
		g_u2RxUdpPort = 0;
		g_u4RxDelayThreshold = 0;
	}
}

void StatsEnvRxTime2Host(IN struct ADAPTER *prAdapter, struct sk_buff *prSkb)
{
	uint8_t *pucEth = prSkb->data;
	uint16_t u2EthType = 0;
	uint8_t ucIpVersion = 0;
	uint8_t ucIpProto = 0;
	uint16_t u2IPID = 0;
	uint16_t u2UdpDstPort = 0;
	uint16_t u2UdpSrcPort = 0;
	uint64_t u8IntTime = 0;
	uint64_t u8RxTime = 0;
	uint32_t u4Delay = 0;
	struct timeval tval;
	struct rtc_time tm;

	if ((g_ucTxRxFlag & BIT(1)) == 0)
		return;

	if (prSkb->len <= 24 + ETH_HLEN)
		return;
	u2EthType = (pucEth[ETH_TYPE_LEN_OFFSET] << 8)
		| (pucEth[ETH_TYPE_LEN_OFFSET + 1]);
	pucEth += ETH_HLEN;
	if (u2EthType != ETH_P_IPV4)
		return;
	ucIpProto = pucEth[9];
	if (g_ucRxIpProto && (ucIpProto != g_ucRxIpProto))
		return;
	ucIpVersion = (pucEth[0] & IPVH_VERSION_MASK) >> IPVH_VERSION_OFFSET;
	if (ucIpVersion != IPVERSION)
		return;
	u2IPID = pucEth[4] << 8 | pucEth[5];
	u8IntTime = GLUE_RX_GET_PKT_INT_TIME(prSkb);
	u4Delay = ((uint32_t)(sched_clock() - u8IntTime))/NSEC_PER_USEC;
	u8RxTime = GLUE_RX_GET_PKT_RX_TIME(prSkb);
	do_gettimeofday(&tval);
	rtc_time_to_tm(tval.tv_sec, &tm);

	switch (ucIpProto) {
	case IP_PRO_TCP:
	case IP_PRO_UDP:
		u2UdpSrcPort = (pucEth[20] << 8) | pucEth[21];
		u2UdpDstPort = (pucEth[22] << 8) | pucEth[23];
		if (g_u2RxUdpPort && (u2UdpSrcPort != g_u2RxUdpPort))
			break;
	case IP_PRO_ICMP:
		u4TotalRx++;
		if (g_u4RxDelayThreshold && (u4Delay <= g_u4RxDelayThreshold)) {
			u4NoDelayRx++;
			break;
		}
		DBGLOG(RX, INFO,
	"IPID 0x%04x src %d dst %d UP %d,delay %u us,int2rx %lu us,IntTime %llu,%u/%u,leave at %02d:%02d:%02d.%06ld\n",
			u2IPID, u2UdpSrcPort, u2UdpDstPort,
			((pucEth[1] & IPTOS_PREC_MASK) >> IPTOS_PREC_OFFSET),
			u4Delay,
			((uint32_t)(u8RxTime - u8IntTime))/NSEC_PER_USEC,
			u8IntTime, u4NoDelayRx, u4TotalRx,
			tm.tm_hour, tm.tm_min, tm.tm_sec, tval.tv_usec);
		break;
	default:
		break;
	}
}

void StatsEnvTxTime2Hif(IN struct ADAPTER *prAdapter,
	IN struct MSDU_INFO *prMsduInfo)
{
	uint64_t u8SysTime, u8SysTimeIn;
	uint32_t u4TimeDiff;
	uint8_t *pucEth = ((struct sk_buff *)prMsduInfo->prPacket)->data;
	uint32_t u4PacketLen = ((struct sk_buff *)prMsduInfo->prPacket)->len;
	uint8_t ucIpVersion = 0;
	uint8_t ucIpProto = 0;
	uint8_t *pucEthBody = NULL;
	uint16_t u2EthType = 0;
	uint8_t *pucAheadBuf = NULL;
	uint16_t u2IPID = 0;
	uint16_t u2UdpDstPort = 0;
	uint16_t u2UdpSrcPort = 0;

	u8SysTime = StatsEnvTimeGet();
	u8SysTimeIn = GLUE_GET_PKT_XTIME(prMsduInfo->prPacket);

	if ((g_ucTxRxFlag & BIT(0)) == 0)
		return;

	if ((u8SysTimeIn == 0) || (u8SysTime <= u8SysTimeIn))
		return;

	/* units of u4TimeDiff is micro seconds (us) */
	if (u4PacketLen < 24 + ETH_HLEN)
		return;
	pucAheadBuf = &pucEth[76];
	u2EthType = (pucAheadBuf[ETH_TYPE_LEN_OFFSET] << 8)
		| (pucAheadBuf[ETH_TYPE_LEN_OFFSET + 1]);
	pucEthBody = &pucAheadBuf[ETH_HLEN];
	if (u2EthType != ETH_P_IPV4)
		return;
	ucIpProto = pucEthBody[9];
	if (g_ucTxIpProto && (ucIpProto != g_ucTxIpProto))
		return;
	ucIpVersion = (pucEthBody[0] & IPVH_VERSION_MASK)
		>> IPVH_VERSION_OFFSET;
	if (ucIpVersion != IPVERSION)
		return;
	u2IPID = pucEthBody[4]<<8 | pucEthBody[5];
	u8SysTime = u8SysTime - u8SysTimeIn;
	u4TimeDiff = (uint32_t) u8SysTime;
	u4TimeDiff = u4TimeDiff / 1000;	/* ns to us */

	switch (ucIpProto) {
	case IP_PRO_TCP:
	case IP_PRO_UDP:
		u2UdpDstPort = (pucEthBody[22] << 8) | pucEthBody[23];
		u2UdpSrcPort = (pucEthBody[20] << 8) | pucEthBody[21];
		if (g_u2TxUdpPort && (u2UdpDstPort != g_u2TxUdpPort))
			break;
	case IP_PRO_ICMP:
		u4TotalTx++;
		if (g_u4TxDelayThreshold
			&& (u4TimeDiff <= g_u4TxDelayThreshold)) {
			u4NoDelayTx++;
			break;
		}
		DBGLOG(TX, INFO,
			"IPID 0x%04x src %d dst %d UP %d,delay %u us,u8SysTimeIn %llu, %u/%u\n",
			u2IPID, u2UdpSrcPort, u2UdpDstPort,
			((pucEthBody[1] & IPTOS_PREC_MASK)
				>> IPTOS_PREC_OFFSET),
			u4TimeDiff, u8SysTimeIn, u4NoDelayTx, u4TotalTx);
		break;
	default:
		break;
	}
}

static void statsParsePktInfo(uint8_t *pucPkt, struct sk_buff *skb,
	uint8_t status, uint8_t eventType)
{
	/* get ethernet protocol */
	uint16_t u2EtherType =
		(pucPkt[ETH_TYPE_LEN_OFFSET] << 8)
			| (pucPkt[ETH_TYPE_LEN_OFFSET + 1]);
	uint8_t *pucEthBody = &pucPkt[ETH_HLEN];

	switch (u2EtherType) {
	case ETH_P_ARP:
	{
		uint16_t u2OpCode = (pucEthBody[6] << 8) | pucEthBody[7];

		switch (eventType) {
		case EVENT_RX:
			if (u2OpCode == ARP_PRO_REQ)
				DBGLOG_LIMITED(RX, TRACE,
					"<RX> Arp Req From IP: %d.%d.%d.%d\n",
					pucEthBody[14], pucEthBody[15],
					pucEthBody[16], pucEthBody[17]);
			else if (u2OpCode == ARP_PRO_RSP)
				DBGLOG_LIMITED(RX, TRACE,
					"<RX> Arp Rsp from IP: %d.%d.%d.%d\n",
					pucEthBody[14], pucEthBody[15],
					pucEthBody[16], pucEthBody[17]);
			break;
		case EVENT_TX:
			if (u2OpCode == ARP_PRO_REQ)
				DBGLOG_LIMITED(TX, TRACE,
					"<TX> Arp Req to IP: %d.%d.%d.%d\n",
					pucEthBody[24], pucEthBody[25],
					pucEthBody[26], pucEthBody[27]);
			else if (u2OpCode == ARP_PRO_RSP)
				DBGLOG_LIMITED(TX, TRACE,
					"<TX> Arp Rsp to IP: %d.%d.%d.%d\n",
					pucEthBody[24], pucEthBody[25],
					pucEthBody[26], pucEthBody[27]);
			break;
		}
		break;
	}
	case ETH_P_IPV4:
	{
		/* IP header without options */
		uint8_t ucIpProto = pucEthBody[9];
		uint8_t ucIpVersion =
			(pucEthBody[0] & IPVH_VERSION_MASK)
				>> IPVH_VERSION_OFFSET;
		uint16_t u2IpId = *(uint16_t *) &pucEthBody[4];

		if (ucIpVersion != IPVERSION)
			break;
		switch (ucIpProto) {
		case IP_PRO_ICMP:
		{
			/* the number of ICMP packets is seldom
			 * so we print log here
			 */
			uint8_t ucIcmpType;
			uint16_t u2IcmpId, u2IcmpSeq;
			uint8_t *pucIcmp = &pucEthBody[20];

			ucIcmpType = pucIcmp[0];
			/* don't log network unreachable packet */
			if (ucIcmpType == 3)
				break;
			u2IcmpId = *(uint16_t *) &pucIcmp[4];
			u2IcmpSeq = *(uint16_t *) &pucIcmp[6];
			switch (eventType) {
			case EVENT_RX:
				DBGLOG_LIMITED(RX, TRACE,
					"<RX> ICMP: Type %d, Id BE 0x%04x, Seq BE 0x%04x\n",
					ucIcmpType, u2IcmpId, u2IcmpSeq);
				break;
			case EVENT_TX:
				DBGLOG_LIMITED(TX, TRACE,
					"<TX> ICMP: Type %d, Id 0x%04x, Seq BE 0x%04x\n",
					ucIcmpType, u2IcmpId, u2IcmpSeq);
				break;
			}
			break;
		}
		case IP_PRO_UDP:
		{
			/* the number of DHCP packets is seldom
			 * so we print log here
			 */
			uint8_t *pucUdp = &pucEthBody[20];
			uint8_t *pucBootp = &pucUdp[UDP_HDR_LEN];
			struct BOOTP_PROTOCOL *prBootp;
			uint16_t u2UdpDstPort;
			uint16_t u2UdpSrcPort;
			uint32_t u4TransID;
			prBootp =
				(struct BOOTP_PROTOCOL *) &pucUdp[UDP_HDR_LEN];

			u2UdpDstPort = (pucUdp[2] << 8) | pucUdp[3];
			u2UdpSrcPort = (pucUdp[0] << 8) | pucUdp[1];
			if ((u2UdpDstPort == UDP_PORT_DHCPS)
				|| (u2UdpDstPort == UDP_PORT_DHCPC)) {
				WLAN_GET_FIELD_BE32(
					&prBootp->u4TransId, &u4TransID);
				switch (eventType) {
				case EVENT_RX:
					DBGLOG_LIMITED(RX, INFO,
						"<RX> DHCP: IPID 0x%02x, MsgType 0x%x, TransID 0x%04x\n",
						u2IpId, prBootp->aucOptions[6],
						u4TransID);
					break;
				case EVENT_TX:
					DBGLOG_LIMITED(TX, INFO,
						"<TX> DHCP: IPID 0x%02x, MsgType 0x%x, TransID 0x%04x\n",
						u2IpId, prBootp->aucOptions[6],
						u4TransID);
					break;
				}
			} else if (u2UdpSrcPort == UDP_PORT_DNS) { /* tx dns */
				uint16_t u2TransId =
					(pucBootp[0] << 8) | pucBootp[1];

				if (eventType == EVENT_RX)
					DBGLOG_LIMITED(RX, INFO,
						"<RX> DNS: IPID 0x%02x, TransID 0x%04x\n",
						u2IpId, u2TransId);
			}
			break;
		}
		}
		break;
	}
	case ETH_P_IPV6:
	{
		/* IPv6 header without options */
		uint8_t ucIpv6Proto =
			pucEthBody[IPV6_HDR_LEN];
		uint8_t ucIpVersion =
			(pucEthBody[0] & IPVH_VERSION_MASK)
				>> IPVH_VERSION_OFFSET;

		if (ucIpVersion != IP_VERSION_6)
			break;
		switch (ucIpv6Proto) {
		case 0x85:
			switch (eventType) {
			case EVENT_RX:
				DBGLOG_LIMITED(RX, INFO,
					"<RX><IPv6> Router Solicitation\n");
				break;
			case EVENT_TX:
				DBGLOG_LIMITED(TX, INFO,
					"<TX><IPv6> Router Solicitation\n");
				break;
			}
			break;
		case 0x86:
			switch (eventType) {
			case EVENT_RX:
				DBGLOG_LIMITED(RX, INFO,
					"<RX><IPv6> Router Advertisement\n");
				break;
			case EVENT_TX:
				DBGLOG_LIMITED(TX, INFO,
					"<TX><IPv6> Router Advertisement\n");
				break;
			}
			break;
		case ICMPV6_TYPE_NEIGHBOR_SOLICITATION:
			switch (eventType) {
			case EVENT_RX:
				DBGLOG_LIMITED(RX, INFO,
					"<RX><IPv6> Neighbor Solicitation\n");
				break;
			case EVENT_TX:
				DBGLOG_LIMITED(TX, INFO,
					"<TX><IPv6> Neighbor Solicitation\n");
				break;
			}
			break;
		case ICMPV6_TYPE_NEIGHBOR_ADVERTISEMENT:
			switch (eventType) {
			case EVENT_RX:
				DBGLOG_LIMITED(RX, INFO,
					"<RX><IPv6> Neighbor Advertisement\n");
				break;
			case EVENT_TX:
				DBGLOG_LIMITED(TX, INFO,
					"<TX><IPv6> Neighbor Advertisement\n");
				break;
			}
			break;
		}
		break;
	}
	case ETH_P_1X:
	{
		uint8_t *pucEapol = pucEthBody;
		uint8_t ucEapolType = pucEapol[1];

		switch (ucEapolType) {
		case 0: /* eap packet */
			switch (eventType) {
			case EVENT_RX:
				DBGLOG_LIMITED(RX, INFO,
					"<RX> EAP Packet: code %d, id %d, type %d\n",
					pucEapol[4], pucEapol[5], pucEapol[7]);
				break;
			case EVENT_TX:
				DBGLOG_LIMITED(TX, INFO,
					"<TX> EAP Packet: code %d, id %d, type %d\n",
					pucEapol[4], pucEapol[5],
					pucEapol[7]);
				break;
			}
			break;
		case 1: /* eapol start */
			switch (eventType) {
			case EVENT_RX:
				DBGLOG_LIMITED(RX, INFO, "<RX> EAPOL: start\n");
				break;
			case EVENT_TX:
				DBGLOG_LIMITED(TX, INFO, "<TX> EAPOL: start\n");
				break;
			}
			break;
		case 3: /* key */
			switch (eventType) {
			case EVENT_RX:
				DBGLOG_LIMITED(RX, INFO,
					"<RX> EAPOL: key, KeyInfo 0x%04x\n",
					*((uint16_t *)(&pucEapol[5])));
				break;
			case EVENT_TX:
				DBGLOG_LIMITED(TX, INFO,
					"<TX> EAPOL: key, KeyInfo 0x%04x\n",
					*((uint16_t *)(&pucEapol[5])));
				break;
			}

			break;
		}
		break;
	}
#if CFG_SUPPORT_WAPI
	case ETH_WPI_1X:
	{
		uint8_t ucSubType = pucEthBody[3]; /* sub type filed*/
		uint16_t u2Length = *(uint16_t *)&pucEthBody[6];
		uint16_t u2Seq = *(uint16_t *)&pucEthBody[8];

		switch (eventType) {
		case EVENT_RX:
			DBGLOG_LIMITED(RX, INFO,
				"<RX> WAPI: subType %d, Len %d, Seq %d\n",
				ucSubType, u2Length, u2Seq);
			break;
		case EVENT_TX:
			DBGLOG_LIMITED(TX, INFO,
				"<TX> WAPI: subType %d, Len %d, Seq %d\n",
				ucSubType, u2Length, u2Seq);
			break;
		}
		break;
	}
#endif
	case ETH_PRO_TDLS:
		switch (eventType) {
		case EVENT_RX:
			DBGLOG_LIMITED(RX, INFO,
				"<RX> TDLS type %d, category %d, Action %d, Token %d\n",
				pucEthBody[0], pucEthBody[1],
				pucEthBody[2], pucEthBody[3]);
			break;
		case EVENT_TX:
			DBGLOG_LIMITED(TX, INFO,
				"<TX> TDLS type %d, category %d, Action %d, Token %d\n",
				pucEthBody[0], pucEthBody[1],
				pucEthBody[2], pucEthBody[3]);
			break;
		}
		break;
	}
}
/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to display rx packet information.
 *
 * \param[in] pPkt			Pointer to the packet
 * \param[out] None
 *
 * \retval None
 */
/*----------------------------------------------------------------------------*/
void StatsRxPktInfoDisplay(struct SW_RFB *prSwRfb)
{
	uint8_t *pPkt = NULL;
	struct sk_buff *skb = NULL;

	if (prSwRfb->u2PacketLen <= ETHER_HEADER_LEN)
		return;

	pPkt = prSwRfb->pvHeader;
	if (!pPkt)
		return;

	skb = (struct sk_buff *)(prSwRfb->pvPacket);
	if (!skb)
		return;

	statsParsePktInfo(pPkt, skb, 0, EVENT_RX);
}

/*----------------------------------------------------------------------------*/
/*! \brief  This routine is called to display tx packet information.
 *
 * \param[in] pPkt			Pointer to the packet
 * \param[out] None
 *
 * \retval None
 */
/*----------------------------------------------------------------------------*/
void StatsTxPktInfoDisplay(uint8_t *pPkt)
{
	uint16_t u2EtherTypeLen;

	u2EtherTypeLen =
		(pPkt[ETH_TYPE_LEN_OFFSET] << 8)
			| (pPkt[ETH_TYPE_LEN_OFFSET + 1]);
	statsParsePktInfo(pPkt, NULL, 0, EVENT_TX);
}

#endif /* CFG_SUPPORT_STATISTICS */

/* End of stats.c */
