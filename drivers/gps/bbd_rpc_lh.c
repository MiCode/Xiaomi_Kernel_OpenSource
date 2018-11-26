/*
 * Copyright 2015 Broadcom Corporation
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation (the "GPL").
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * A copy of the GPL is available at
 * http://www.broadcom.com/licenses/GPLv2.php, or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Sensor RPC handler for 4773/4774
 *
 * tabstop = 8
 */


#ifdef __KERNEL__

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/bug.h>
#include <linux/printk.h>
#include "bbd.h"


ssize_t bbd_sensor_write(const char *buf, unsigned int size);
void bcm_on_packet_received(void *_priv, unsigned char *data, size_t size);
#else


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define pr_debug 	printf
#define pr_info 	printf
#define pr_warn 	printf
#define pr_err		printf
#define WARN_ON(x) (if (x) printf("error in %s:%d\n", __func__, __LINE__))

ssize_t bbd_sensor_write(const unsigned char *buf, size_t size)
{
	int i = 0;
	printf("Sensor data(%d bytes) ", size);
	for (i = 0; i < size; i++)
		printf("%02X ", buf[i]);
	printf("\n");
	return size;
}

#endif

/*
******************* CUSTOMIZATION FILE ********************
* This file can be edited for your project, and you
* can customize your transport layer by changing some of the default value
* Note that you can also define these value from your makefile directly
*/




#ifndef TLCUST_ENABLE_RELIABLE_PL
	#define TLCUST_ENABLE_RELIABLE_PL 1
#endif



#ifndef TLCUST_MAX_OUTGOING_PACKET_SIZE
	#define TLCUST_MAX_OUTGOING_PACKET_SIZE 2048
#endif



#ifndef TLCUST_MAX_INCOMING_PACKET_SIZE
	#define TLCUST_MAX_INCOMING_PACKET_SIZE 2048
#endif



#ifndef TLCUST_RELIABLE_RETRY_TIMEOUT_MS
	#define TLCUST_RELIABLE_RETRY_TIMEOUT_MS 1000
#endif


#ifndef TLCUST_RELIABLE_MAX_RETRY
	#define TLCUST_RELIABLE_MAX_RETRY 10
#endif




#ifndef TLCUST_RELIABLE_MAX_PACKETS
	#define TLCUST_RELIABLE_MAX_PACKETS 150
#endif

/*public constants that are built from the customization file*/
#define MAX_OUTGOING_PACKET_SIZE  TLCUST_MAX_OUTGOING_PACKET_SIZE
#define MAX_INCOMING_PACKET_SIZE  TLCUST_MAX_INCOMING_PACKET_SIZE
#define RELIABLE_RETRY_TIMEOUT_MS TLCUST_RELIABLE_RETRY_TIMEOUT_MS
#define RELIABLE_MAX_RETRY        TLCUST_RELIABLE_MAX_RETRY
#define RELIABLE_MAX_PACKETS      TLCUST_RELIABLE_MAX_PACKETS

#define MAX_HEADER_SIZE 14


#define _DIM(x) ((unsigned int)(sizeof(x)/sizeof(*(x))))


/*
 * The following are used for the software flow control (UART)
 */
static const unsigned char XON_CHARACTER = 0x11;
static const unsigned char XOFF_CHARACTER = 0x13;

/*
 * The following are used for the protocol.
 */
static const unsigned char ESCAPE_CHARACTER         = 0xB0;
static const unsigned char SOP_CHARACTER            = 0x01;
static const unsigned char EOP_CHARACTER            = 0x01;
static const unsigned char ESCAPED_ESCAPE_CHARACTER = 0x03;
static const unsigned char ESCAPED_XON_CHARACTER    = 0x04;
static const unsigned char ESCAPED_XOFF_CHARACTER   = 0x05;

/*
 * The following are the bit field definition for the flags
 */
static const unsigned short FLAG_PACKET_ACK      = (1<<0);

static const unsigned short FLAG_RELIABLE_PACKET = (1<<1);


static const unsigned short FLAG_RELIABLE_ACK    = (1<<2);


static const unsigned short FLAG_RELIABLE_NACK   = (1<<3);





static const unsigned short FLAG_MSG_LOST        = (1<<4);


static const unsigned short FLAG_MSG_GARBAGE     = (1<<5);


static const unsigned short FLAG_SIZE_EXTENDED   = (1<<6);


static const unsigned short FLAG_EXTENDED        = (1<<7);


static const unsigned short FLAG_INTERNAL_PACKET = (1<<8);


static const unsigned short FLAG_IGNORE_SEQID    = (1<<9);



/* Enumeration of all the RPCs for the codec. DO NOT CHANGE THE ORDER,
 * DELETE, or INSERT anything to keep backward compatibility.
 */
#define RPC_DEFINITION(klass, method) RPC_##klass##_##method
enum {
	RPC_DEFINITION(IRpcA, A)
	, RPC_DEFINITION(IRpcA, B)
	, RPC_DEFINITION(IRpcA, C)

	, RPC_DEFINITION(IRpcB, A)
	, RPC_DEFINITION(IRpcB, B)

	, RPC_DEFINITION(IRpcC, A)
	, RPC_DEFINITION(IRpcC, B)
	, RPC_DEFINITION(IRpcC, C)

	, RPC_DEFINITION(IRpcD, A)

	, RPC_DEFINITION(IRpcE, A)
	, RPC_DEFINITION(IRpcE, B)

	, RPC_DEFINITION(IRpcF, A)
	, RPC_DEFINITION(IRpcF, B)
	, RPC_DEFINITION(IRpcF, C)

	, RPC_DEFINITION(IRpcG, A)
	, RPC_DEFINITION(IRpcG, B)
	, RPC_DEFINITION(IRpcG, C)
	, RPC_DEFINITION(IRpcG, D)
	, RPC_DEFINITION(IRpcG, E)

	, RPC_DEFINITION(IRpcH, A)
	, RPC_DEFINITION(IRpcH, B)
	, RPC_DEFINITION(IRpcH, C)
	, RPC_DEFINITION(IRpcH, D)

	, RPC_DEFINITION(IRpcI, A)
	, RPC_DEFINITION(IRpcI, B)
	, RPC_DEFINITION(IRpcI, C)

	, RPC_DEFINITION(IRpcJ, A)
	, RPC_DEFINITION(IRpcJ, B)

	, RPC_DEFINITION(IRpcK, A)
	, RPC_DEFINITION(IRpcK, B)
	, RPC_DEFINITION(IRpcK, C)

	, RPC_DEFINITION(IRpcL, A)

	, RPC_DEFINITION(IRpcSensorRequest,  Data)
	, RPC_DEFINITION(IRpcSensorResponse, Data)
};


enum {
	WAIT_FOR_ESC_SOP = 0
		, WAIT_FOR_SOP
		, WAIT_FOR_MESSAGE_COMPLETE
		, WAIT_FOR_EOP
};


typedef struct stTransportLayerStats {
	unsigned int ulRxGarbageBytes;
	unsigned int ulRxPacketLost;
	unsigned int ulRemotePacketLost;

	unsigned int ulRemoteGarbage;

	unsigned int ulPacketSent;

	unsigned int ulPacketReceived;
	unsigned int ulAckReceived;
	unsigned int ulReliablePacketSent;
	unsigned int ulReliableRetransmit;
	unsigned int ulReliablePacketReceived;
	unsigned int ulMaxRetransmitCount;
} stTransportLayerStats;

static unsigned int m_uiParserState;
static unsigned int m_uiRxLen;
static unsigned int m_uiEscLen;
static unsigned int m_ulByteCntSinceLastValidPacket;
static unsigned char m_aucRxMessageBuf[MAX_INCOMING_PACKET_SIZE+
									   MAX_HEADER_SIZE];
static unsigned char m_aucRxEscapedBuf[(MAX_INCOMING_PACKET_SIZE+
										MAX_HEADER_SIZE+2)*2];
static stTransportLayerStats m_otCurrentStats;
static unsigned char m_ucDelayAckCount;



static unsigned char m_ucLastRxSeqId;
static unsigned char m_ucLastAckSeqId;
static unsigned char m_ucReliableSeqId;
static unsigned char m_ucReliableCrc;
static unsigned short m_usReliableLen;
static bool m_bOngoingSync;

























/**
 * CRC table - from GlUtlCrc::ucCrcTable
 *
 */
static const unsigned char crc_table[] = {
	0x00, 0x4d, 0x9a, 0xd7, 0x79, 0x34, 0xe3, 0xae, 0xf2, 0xbf,
	0x68, 0x25, 0x8b, 0xc6, 0x11, 0x5c, 0xa9, 0xe4, 0x33, 0x7e,
	0xd0, 0x9d, 0x4a, 0x07, 0x5b, 0x16, 0xc1, 0x8c, 0x22, 0x6f,
	0xb8, 0xf5, 0x1f, 0x52, 0x85, 0xc8, 0x66, 0x2b, 0xfc, 0xb1,
	0xed, 0xa0, 0x77, 0x3a, 0x94, 0xd9, 0x0e, 0x43, 0xb6, 0xfb,
	0x2c, 0x61, 0xcf, 0x82, 0x55, 0x18, 0x44, 0x09, 0xde, 0x93,
	0x3d, 0x70, 0xa7, 0xea, 0x3e, 0x73, 0xa4, 0xe9, 0x47, 0x0a,
	0xdd, 0x90, 0xcc, 0x81, 0x56, 0x1b, 0xb5, 0xf8, 0x2f, 0x62,
	0x97, 0xda, 0x0d, 0x40, 0xee, 0xa3, 0x74, 0x39, 0x65, 0x28,
	0xff, 0xb2, 0x1c, 0x51, 0x86, 0xcb, 0x21, 0x6c, 0xbb, 0xf6,
	0x58, 0x15, 0xc2, 0x8f, 0xd3, 0x9e, 0x49, 0x04, 0xaa, 0xe7,
	0x30, 0x7d, 0x88, 0xc5, 0x12, 0x5f, 0xf1, 0xbc, 0x6b, 0x26,
	0x7a, 0x37, 0xe0, 0xad, 0x03, 0x4e, 0x99, 0xd4, 0x7c, 0x31,
	0xe6, 0xab, 0x05, 0x48, 0x9f, 0xd2, 0x8e, 0xc3, 0x14, 0x59,
	0xf7, 0xba, 0x6d, 0x20, 0xd5, 0x98, 0x4f, 0x02, 0xac, 0xe1,
	0x36, 0x7b, 0x27, 0x6a, 0xbd, 0xf0, 0x5e, 0x13, 0xc4, 0x89,
	0x63, 0x2e, 0xf9, 0xb4, 0x1a, 0x57, 0x80, 0xcd, 0x91, 0xdc,
	0x0b, 0x46, 0xe8, 0xa5, 0x72, 0x3f, 0xca, 0x87, 0x50, 0x1d,
	0xb3, 0xfe, 0x29, 0x64, 0x38, 0x75, 0xa2, 0xef, 0x41, 0x0c,
	0xdb, 0x96, 0x42, 0x0f, 0xd8, 0x95, 0x3b, 0x76, 0xa1, 0xec,
	0xb0, 0xfd, 0x2a, 0x67, 0xc9, 0x84, 0x53, 0x1e, 0xeb, 0xa6,
	0x71, 0x3c, 0x92, 0xdf, 0x08, 0x45, 0x19, 0x54, 0x83, 0xce,
	0x60, 0x2d, 0xfa, 0xb7, 0x5d, 0x10, 0xc7, 0x8a, 0x24, 0x69,
	0xbe, 0xf3, 0xaf, 0xe2, 0x35, 0x78, 0xd6, 0x9b, 0x4c, 0x01,
	0xf4, 0xb9, 0x6e, 0x23, 0x8d, 0xc0, 0x17, 0x5a, 0x06, 0x4b,
	0x9c, 0xd1, 0x7f, 0x32, 0xe5, 0xa8
};

/**
 * crc_calc from GlUtlCrc::GlUtlCrcCalc
 *
 *
 */

static inline unsigned char crc_calc(unsigned char *m_ucCrcState,
									 unsigned char ucData)

{
	*m_ucCrcState = crc_table[*m_ucCrcState ^ ucData];
	return *m_ucCrcState;
}


/**
 * crc_calc_many - from GlUtlCrc::GlUtlCrcCalc
 *
 */
static unsigned char crc_calc_many(unsigned char *m_ucCrcState,
								   const unsigned char *pucData,
								   unsigned short usLen)
{
	while (usLen--) {
		*m_ucCrcState =  crc_table[*m_ucCrcState ^ (*pucData++)];
	}
	return *m_ucCrcState;
}


/*
 * BbdBridge_OnRpcReceived - copied from RpcEngine::OnRpcReceived
 *
 */
static int BbdBridge_OnRpcReceived(unsigned short usRpcId,
								   unsigned char *pRpcPayload,
								   unsigned short usRpcLen)
{
	if (usRpcId == RPC_DEFINITION(IRpcSensorResponse, Data)) {
		/* Read 2 byte size */
		ssize_t result;
		unsigned short size = *pRpcPayload++;
		size |= *pRpcPayload++ << 8;

		result = bbd_sensor_write(pRpcPayload, size);
		WARN_ON(size != usRpcLen-2);
		WARN_ON((short) result != size);

		return 1;
	}

	return 0;
}

/*
 * BbdBridge_CheckPacketSanity - copied from RpcEngine::OnPacketReceived
 *
 */
static bool BbdBridge_CheckPacketSanity(unsigned char *pucData,
										unsigned short usSize)
{
	long lSize = (long) usSize;
	while (lSize > 0) {
		unsigned short usRpcId = *pucData++;
		unsigned short usRpcLen;
		lSize--;

		if (usRpcId&0x80) {
			usRpcId &= ~0x80;
			usRpcId <<= 8;
			usRpcId |= *pucData++; lSize--;
		}

		usRpcLen = *pucData++; lSize--;
		if (usRpcLen&0x80) {
			usRpcLen &= ~0x80;
			usRpcLen <<= 8;
			usRpcLen |= *pucData++; lSize--;
		}

		pucData += usRpcLen;
		lSize -= usRpcLen;
	}

	return lSize == 0;
}

/*
 * RpcEngine_OnPacketReceived - copied from RpcEngine::OnPacketReceived
 *
 */
static int BbdBridge_OnPacketReceived(unsigned char *pucData,
									  unsigned short usSize)
{
	int sensor = 0, gnss = 0;
	if (BbdBridge_CheckPacketSanity(pucData, usSize)) {
		long lSize = (long)usSize;

		while (lSize > 0) {
			unsigned short usRpcId    = *pucData++;
			unsigned short usRpcLen;
			lSize--;

			if (usRpcId&0x80) {
				usRpcId &= ~0x80;
				usRpcId <<= 8;
				usRpcId |= *pucData++; lSize--;
			}

			usRpcLen = *pucData++; lSize--;
			if (usRpcLen&0x80) {
				usRpcLen &= ~0x80;
				usRpcLen <<= 8;
				usRpcLen |= *pucData++; lSize--;
			}
			if (BbdBridge_OnRpcReceived(usRpcId, pucData, usRpcLen))
				sensor++;
			else
				gnss++;
			pucData += usRpcLen;
			lSize -= usRpcLen;
		}
	} else {
		WARN_ON(1);
	}

	return (sensor > 0);
}


static bool TransportLayer_PacketReceived(void *priv)
{
	unsigned char ucCrc = 0;

	/* minimum is seqId, payload size, flags, and Crc */
	if (m_uiRxLen >= 4) {
		/* compute CRC */
		/* CRC is not applied on itself, nor on the SeqId */
		ucCrc = crc_calc_many(&ucCrc, &m_aucRxMessageBuf[1], m_uiRxLen-2);

		/* CRC has its nibble inverted for stronger CRC (as CRC of
		 * a packet with itself is always 0, if EoP is not detected,
		 * that always reset the CRC)
		 */
		ucCrc = ((ucCrc&0x0F)<<4) | ((ucCrc&0xF0)>>4);
		if (ucCrc != m_aucRxMessageBuf[m_uiRxLen-1]) {
			return false;
		}
	} else {
		return false;
	}

	/* passed CRC check */
	{
		unsigned char *pucData = &m_aucRxMessageBuf[0];
		unsigned short usLen = m_uiRxLen-1;

		unsigned char ucSeqId = *pucData++;             /* usLen--; */
		unsigned short usPayloadSize = *pucData++;      /* usLen--; */
		unsigned short usFlags = *pucData++;            /* usLen--; */

		bool bReliablePacket = false;
		unsigned char ucReliableSeqId = 0;

		bool bReliableAckReceived = false;
		unsigned char ucReliableAckSeqId = 0;

		bool bReliableNackReceived = false;
		unsigned char ucReliableNackSeqId = 0;

		bool bAckReceived = false;

		bool bInternalPacket = false;
		bool bIgnoreSeqId = false;

		unsigned short usAckFlags = 0;
		unsigned int i = 0;

		usLen -= 3;

		for (i = 0; i < 16 && usFlags != 0 && usLen > 0; ++i) {
			unsigned short usFlagMask = (1 << i);
			unsigned short usFlagBit = usFlags & usFlagMask;
			unsigned char ucFlagDetail = 0;

			if (usFlagBit == 0) {
				continue;
			}

			usFlags     &= ~usFlagBit;   /* clear the flag */
			ucFlagDetail = *pucData++;   /* extract flag details */
			--usLen;
			if (usFlagBit == FLAG_PACKET_ACK) /* acknowledgment */ {
				/* flag detail contain the acknowledged SeqId */
				unsigned char ucReceivedAckSeqId = ucFlagDetail;

				m_ucLastAckSeqId = ucReceivedAckSeqId;
				bAckReceived = true;
			} else if (usFlagBit == FLAG_RELIABLE_PACKET) {
				/* This is a reliable packet. we need to provide the proper Ack */
				ucReliableSeqId = ucFlagDetail;
				bReliablePacket = true;
			} else if (usFlagBit == FLAG_RELIABLE_ACK) {
				bReliableAckReceived = true;
				ucReliableAckSeqId = ucFlagDetail;
			} else if (usFlagBit == FLAG_RELIABLE_NACK) {
				bReliableNackReceived = true;
				ucReliableNackSeqId = ucFlagDetail;
			} else if (usFlagBit == FLAG_MSG_LOST) {
				/* remote TransportLayer lost had some SeqId jumps */
				m_otCurrentStats.ulRemotePacketLost += ucFlagDetail;
			} else if (usFlagBit == FLAG_MSG_GARBAGE) {
				/* remote TransportLayer detected garbage */
				m_otCurrentStats.ulRemoteGarbage += ucFlagDetail;
			} else if (usFlagBit == FLAG_SIZE_EXTENDED) {
				/* flag detail contains the MSB of the payload size */
				usPayloadSize |= (ucFlagDetail<<8);
			} else if (usFlagBit == FLAG_EXTENDED) {
				/* the flags are extended, which means that the details
				 * contains the MSB of the 16bit flags
				 */
				usFlags |= (ucFlagDetail<<8);
			} else if (usFlagBit == FLAG_INTERNAL_PACKET) {
				/* don't care about details */
				bInternalPacket = true;
			} else if (usFlagBit == FLAG_IGNORE_SEQID) {
				/* don't care about details */
				bIgnoreSeqId = true;
			} else {
				/* we did not process the flag, just put it back
				 * this is an error, so we can break now, as there is no
				 * point in continuing
				 */
				usFlags |= usFlagBit;
				break;
			}
		}

		/* if flag is not garbage, entire packet should all be consumed */
		/* remaining length of the buffer should be the payload size */
		if (usFlags == 0 &&
				usPayloadSize == usLen) {
			/* we now have a valid packet (at least it passed all our
			 * validity checks, so we are going to trust it
			 */
			unsigned char ucExpectedTxSeqId = (m_ucLastRxSeqId+1)&0xFF;
			if (ucSeqId != ucExpectedTxSeqId
					&& !bIgnoreSeqId
					&& !m_bOngoingSync) {
				/* Some packets were lost, jump in the RxSeqId */
				m_otCurrentStats.ulRxPacketLost +=
					((ucSeqId - ucExpectedTxSeqId)&0xFF);
			}
			m_ucLastRxSeqId = ucSeqId; /* increase expected SeqId */

			if (!bAckReceived || usLen > 0) {
				bool bDelayedEnough = (++m_ucDelayAckCount > 200);
				++m_otCurrentStats.ulPacketReceived;



				if (bDelayedEnough) {
					pr_debug("Skip averted/%d %s %s\n", __LINE__,
							(bAckReceived) ? "ACK"  : "!ack",
							(bDelayedEnough) ? "ENUF" : "!enuf");
					usAckFlags |= FLAG_PACKET_ACK;
					m_ucDelayAckCount = 0;
				} else {
					pr_debug("Skip/%d %s %s %d\n", __LINE__,
							(bAckReceived) ? "ACK"  : "!ack",
							(bDelayedEnough) ? "ENUF" : "!enuf",
							m_ucDelayAckCount);
				}
			} else {
				++m_otCurrentStats.ulAckReceived;
			}

			{
				bool bProcessPacket = bReliablePacket || usLen > 0;
				if (bReliablePacket) {
					pr_debug("TransportLayer_Received Reliable"
						     "(Size %u, SeqId %u)\n",
						     usLen, ucReliableSeqId);
					/* if this is a reliable message, we need to make sure
					 * we didn't received it before!
					 * if we did, the Host probably didn't received the Ack,
					 * so let's just send the ack
					 * Reliable seqId is not enough, so we also use CRC and
					 * Length to confirm this was the same message received!
					 */
					if (ucReliableSeqId == m_ucReliableSeqId
							&& ucCrc == m_ucReliableCrc
							&& usLen == m_usReliableLen) {
						/* already received that packet, remote TransportLayer
						 * probably lost the Acknowledgment, send it again
						 */
						usAckFlags |= FLAG_PACKET_ACK | FLAG_RELIABLE_ACK;
						bProcessPacket = false; /* we should not process it again */
					} else if (ucReliableSeqId == ((m_ucReliableSeqId+1)&0xFF)) {
						/* this is a valid message, just do nothing but update
						 * the reliable info. the message will be processed below
						 */
						usAckFlags |= FLAG_PACKET_ACK | FLAG_RELIABLE_ACK;
						/* already received that packet, remote TransportLayer
						 * probably lost the Acknowledgment, send it again
						 */
						m_usReliableLen   = usLen;
						m_ucReliableSeqId = ucReliableSeqId;
						m_ucReliableCrc   = ucCrc;
						m_otCurrentStats.ulReliablePacketReceived++;
					} else {
						/* we received the wrong reliable SeqId */
						usAckFlags |= FLAG_PACKET_ACK | FLAG_RELIABLE_NACK;
						bProcessPacket = false; /* we cannot accept the packet */
					}
				}

				if (bProcessPacket) {
					if (bInternalPacket) {
						bcm_on_packet_received(priv, m_aucRxEscapedBuf, m_uiEscLen);
					} else {
						/* everything good, notify upper layer that we have
						 * the payload of a packet available
						   In case of sensor data, we don't want send it to lhd! */
							if (!BbdBridge_OnPacketReceived(pucData, usLen))
							bcm_on_packet_received(priv, m_aucRxEscapedBuf, m_uiEscLen);
					}
				} else {

						bcm_on_packet_received(priv, m_aucRxEscapedBuf, m_uiEscLen);
				}
				return true;
			}
		} else {
			pr_info("[SSPBBD]: %s :%d\n", __func__, __LINE__);
			return false;
		}
	}
}


/*
 * bbd_parse_asic_data  - copied from TransportLayer::ParseIncomingData
 *
 */
void bbd_parse_asic_data(unsigned char *pucData,
						 unsigned short usLen,
						 void (*to_gpsd)(unsigned char *packet,
						 unsigned short len, void *priv), void *priv)
{
	unsigned short usIdx = 0;

	while (usIdx != usLen) {
		unsigned char ucData = pucData[usIdx++];
		m_ulByteCntSinceLastValidPacket++;

		if (sizeof(m_aucRxEscapedBuf) > m_uiEscLen)
			m_aucRxEscapedBuf[m_uiEscLen++] = ucData;

		if (ucData == XON_CHARACTER || ucData == XOFF_CHARACTER) {
			continue;
		}

		switch (m_uiParserState) {
		case WAIT_FOR_ESC_SOP:
				{
					if (ucData == ESCAPE_CHARACTER) {
						m_uiParserState = WAIT_FOR_SOP;
						m_otCurrentStats.ulRxGarbageBytes +=
							(m_ulByteCntSinceLastValidPacket - 1);


						m_ulByteCntSinceLastValidPacket = 1;
					}
				}
				break;

		case WAIT_FOR_SOP:
				{
					if (ucData == (SOP_CHARACTER - 1)) {
						m_uiParserState = WAIT_FOR_MESSAGE_COMPLETE;
						m_uiRxLen = 0;
					} else {
						if (ucData != ESCAPE_CHARACTER) {
							m_uiParserState = WAIT_FOR_ESC_SOP;
						} else {
							m_otCurrentStats.ulRxGarbageBytes += 2;
							m_ulByteCntSinceLastValidPacket = 1;
						}

					}
				}
				break;

		case WAIT_FOR_MESSAGE_COMPLETE:
				{
					if (ucData == ESCAPE_CHARACTER) {
						m_uiParserState = WAIT_FOR_EOP;
					} else if (m_uiRxLen < sizeof(m_aucRxMessageBuf)) {
						m_aucRxMessageBuf[m_uiRxLen++] = ucData;
					} else {
						m_uiParserState = WAIT_FOR_ESC_SOP;
					}
				}
				break;

		case WAIT_FOR_EOP:
				{
					if (ucData == EOP_CHARACTER) {
						if (TransportLayer_PacketReceived(priv)) {
							m_ulByteCntSinceLastValidPacket = 0;

						}
						m_uiEscLen = 0;
						m_uiParserState = WAIT_FOR_ESC_SOP;
					} else if (ucData == ESCAPED_ESCAPE_CHARACTER) {
						if (m_uiRxLen < _DIM(m_aucRxMessageBuf)) {
							m_aucRxMessageBuf[m_uiRxLen++] = ESCAPE_CHARACTER;
							m_uiParserState = WAIT_FOR_MESSAGE_COMPLETE;
						} else {
							m_uiParserState = WAIT_FOR_ESC_SOP;
						}
					} else if (ucData == ESCAPED_XON_CHARACTER) {
						if (m_uiRxLen < _DIM(m_aucRxMessageBuf)) {
							m_aucRxMessageBuf[m_uiRxLen++] = XON_CHARACTER;
							m_uiParserState = WAIT_FOR_MESSAGE_COMPLETE;
						} else {
							m_uiParserState = WAIT_FOR_ESC_SOP;
						}
					} else if (ucData == ESCAPED_XOFF_CHARACTER) {
						if (m_uiRxLen < _DIM(m_aucRxMessageBuf)) {
							m_aucRxMessageBuf[m_uiRxLen++] = XOFF_CHARACTER;
							m_uiParserState = WAIT_FOR_MESSAGE_COMPLETE;
						} else {
							m_uiParserState = WAIT_FOR_ESC_SOP;
						}
					} else if (ucData == (SOP_CHARACTER - 1)) {


						m_uiParserState = WAIT_FOR_MESSAGE_COMPLETE;

						m_uiRxLen = 0;
						m_uiEscLen = 0;

						m_otCurrentStats.ulRxGarbageBytes +=
							(m_ulByteCntSinceLastValidPacket-2);


						m_ulByteCntSinceLastValidPacket = 2;
					} else if (ucData == ESCAPE_CHARACTER) {
						m_uiParserState = WAIT_FOR_SOP;
					} else {
						m_uiParserState = WAIT_FOR_ESC_SOP;
					}
				}
				break;
		}
	}
}


#ifdef __KERNEL__
EXPORT_SYMBOL(bbd_parse_asic_data);
#endif


#ifndef __KERNEL__


unsigned char sample1[] = { 0x00, 0x2E, 0xB0, 0x00, 0x85, 0x26,
							0x00, 0x21, 0x24, 0x22, 0x00, 0x02,
							0x00, 0x1E, 0x00, 0x37, 0x0E, 0xD3,
							0x00, 0xCB, 0xFF, 0x50, 0x00, 0xDE,
							0x00, 0x56, 0x00, 0xC7, 0xFF, 0x02,
							0x00, 0x00, 0x00, 0x37, 0x00, 0x90,
							0xFE, 0x18, 0xFF, 0xB7, 0x46, 0x02,
							0x00, 0x00, 0x00, 0xB2, 0xB0, 0x01 };


int main(void)
{

	bbd_parse_asic_data(sample1, sizeof(sample1), NULL, NULL);
	return 0;
}

#endif
