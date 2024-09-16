/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
 ** Id: //Department/DaVinci/BRANCHES/
 *      MT6620_WIFI_DRIVER_V2_3/include/nic/hif_tx.h#1
 */


#ifndef _HIF_TX_H
#define _HIF_TX_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
/* Maximum buffer size for individual HIF TCQ Buffer */
/* Reserved field was not included */
#define HIF_TX_BUFF_MAX_SIZE                1552

/* Maximum buffer count for individual HIF TCQ */
#if 0
#define HIF_TX_BUFF_COUNT_TC0               3
#define HIF_TX_BUFF_COUNT_TC1               3
#define HIF_TX_BUFF_COUNT_TC2               3
#define HIF_TX_BUFF_COUNT_TC3               3
#define HIF_TX_BUFF_COUNT_TC4               2
#endif

#define TX_HDR_SIZE                         sizeof(struct HIF_TX_HEADER)

#define CMD_HDR_SIZE                        sizeof(struct WIFI_CMD)

/* !< 2048 Bytes CMD payload buffer */
#define CMD_PKT_SIZE_FOR_IMAGE              2048

/*! NIC_HIF_TX_HEADER_T (for short-header format) */
/* DW 0, Byte 0,1 */
#define HIF_TX_HDR_TX_BYTE_COUNT_MASK       BITS(0, 15)

/* DW 0, Byte 2 */
#define HIF_TX_HDR_ETHER_TYPE_OFFSET_MASK   BITS(0, 6)
#define HIF_TX_HDR_IP_CSUM                  BIT(7)

/* DW 0, Byte 3 */
#define HIF_TX_HDR_TCP_CSUM                 BIT(0)
#define HIF_TX_HDR_QUEUE_IDX_MASK           BITS(3, 6)
#define HIF_TX_HDR_QUEUE_IDX_OFFSET         3
#define HIF_TX_HDR_PORT_IDX_MASK            BIT(7)
#define HIF_TX_HDR_PORT_IDX_OFFSET          7

/*******************************************************************************
 *                         D A T A   T Y P E S
 *******************************************************************************
 */
struct HIF_HW_TX_HEADER {
	uint16_t u2TxByteCount;
	uint8_t ucEtherTypeOffset;
	uint8_t ucCSflags;
	uint8_t aucBuffer[0];
};

struct HIF_TX_HEADER {
	uint16_t u2TxByteCount_UserPriority;
	uint8_t ucEtherTypeOffset;
	uint8_t ucResource_PktType_CSflags;
	uint8_t ucWlanHeaderLength;
	uint8_t ucPktFormtId_Flags;
	uint16_t u2LLH;		/* for BOW */
	uint16_t u2SeqNo;	/* for BOW */
	uint8_t ucStaRecIdx;
	uint8_t ucForwardingType_SessionID_Reserved;
	uint8_t ucPacketSeqNo;
	uint8_t ucAck_BIP_BasicRate;
	uint8_t aucReserved[2];
};

enum ENUM_HIF_OOB_CTRL_PKT_TYPE {
	HIF_OOB_CTRL_PKT_TYPE_LOOPBACK = 1,
	HIF_OOB_CTRL_PKT_TYP_NUM
};

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#define TFCB_FRAME_PAD_TO_DW(u2Length)      ALIGN_4(u2Length)

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
/* Kevin: we don't have to call following function to inspect the data
 * structure. It will check automatically while at compile time.
 */
static __KAL_INLINE__ void hif_txDataTypeCheck(void);

static __KAL_INLINE__ void hif_txDataTypeCheck(void)
{
	DATA_STRUCT_INSPECTING_ASSERT(sizeof(struct HIF_TX_HEADER) == 16);
}

#endif /*_HIF_TX_H */
