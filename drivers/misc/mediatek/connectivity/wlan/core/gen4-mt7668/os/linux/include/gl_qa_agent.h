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
/*! \file   gl_qa_agent.h
*    \brief  This file includes private ioctl support.
*/

#ifndef _GL_QA_AGENT_H
#define _GL_QA_AGENT_H
#if CFG_SUPPORT_QA_TOOL
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

#define HQA_CMD_MAGIC_NO 0x18142880
#define HQA_CHIP_ID_6632	0x6632
#define HQA_CHIP_ID_7668	0x7668

#if CFG_SUPPORT_TX_BF
#define HQA_BF_STR_SIZE 512
#endif

#define HQA_RX_STATISTIC_NUM 66
#define BUFFER_BIN_MODE 0x0
#define EFUSE_MODE 0x2

extern UINT_8 uacEEPROMImage[MAX_EEPROM_BUFFER_SIZE];

#if 0
typedef struct _PARAM_RX_STAT_T {
	UINT_32 MacFCSErr;	/* Y            0x820F_D014 */
	UINT_32 MacMdrdy;	/* Y            0x820F_D030 */
	UINT_32 FCSErr_CCK;	/* Y            0x8207_021C     [15:00] */
	UINT_32 FCSErr_OFDM;	/* Y            0x8207_021C     [31:16] */
	UINT_32 CCK_PD;		/* Y            0x8207_020C     [15:00] */
	UINT_32 OFDM_PD;	/* Y            0x8207_020C     [15:00] */
	UINT_32 CCK_SIG_Err;	/* Y            0x8207_0210     [31:16] */
	UINT_32 CCK_SFD_Err;	/* Y            0x8207_0210     [15:00] */
	UINT_32 OFDM_SIG_Err;	/* Y            0x8207_0214     [31:16] */
	UINT_32 OFDM_TAG_Err;	/* Y            0x8207_0214     [15:00] */
	UINT_32 WB_RSSSI0;	/* Y            0x8207_21A8     [23:16] */
	UINT_32 IB_RSSSI0;	/* Y            0x8207_21A8     [31:24] */
	UINT_32 WB_RSSSI1;	/* Y            0x8207_21A8     [07:00] */
	UINT_32 IB_RSSSI1;	/* Y            0x8207_21A8     [15:08] */
	UINT_32 PhyMdrdyCCK;	/* Y            0x8207_0220     [15:00] */
	UINT_32 PhyMdrdyOFDM;	/* Y            0x8207_0220     [31:16] */
	UINT_32 DriverRxCount;	/* Y            FW Counter Band0 */
	UINT_32 RCPI0;		/* Y            RXV4            [07:00] */
	UINT_32 RCPI1;		/* Y            RXV4            [15:08] */
	UINT_32 FreqOffsetFromRX;	/* Y            RXV5            MISC1[24:00]    OFDM:[11:00]    CCK:[10:00] */
	UINT_32 RSSI0;		/* N */
	UINT_32 RSSI1;		/* N */
	UINT_32 rx_fifo_full;	/* N */
	UINT_32 RxLenMismatch;	/* N */
	UINT_32 MacFCSErr_band1;	/* Y            0x820F_D214 */
	UINT_32 MacMdrdy_band1;	/* Y            0x820F_D230 */
	/* Y            RXV3            [23:16] (must set 0x8207066C[1:0] = 0x0 ~ 0x3) */
	UINT_32 FAGC_IB_RSSSI[4];
	/* Y            RXV3            [31:24] (must set 0x8207066C[1:0] = 0x0 ~ 0x3) */
	UINT_32 FAGC_WB_RSSSI[4];
	/* Y            0x8207_21A8     [31:24] [15:08] 0x8207_29A8     [31:24] [15:08] */
	UINT_32 Inst_IB_RSSSI[4];
	/* Y            0x8207_21A8     [23:16] [07:00] 0x8207_29A8     [23:16] [07:00] */
	UINT_32 Inst_WB_RSSSI[4];
	UINT_32 ACIHitLow;	/* Y            0x8207_21B0     [18] */
	UINT_32 ACIHitHigh;	/* Y            0x8207_29B0     [18] */
	UINT_32 DriverRxCount1;	/* Y            FW Counter Band1 */
	UINT_32 RCPI2;		/* Y            RXV4            [23:16] */
	UINT_32 RCPI3;		/* Y            RXV4            [31:24] */
	UINT_32 RSSI2;		/* N */
	UINT_32 RSSI3;		/* N */
	UINT_32 SNR0;		/* Y            RXV5            (MISC1 >> 19) - 16 */
	UINT_32 SNR1;		/* N */
	UINT_32 SNR2;		/* N */
	UINT_32 SNR3;		/* N */
	UINT_32 rx_fifo_full_band1;	/* N */
	UINT_32 RxLenMismatch_band1;	/* N */
	UINT_32 CCK_PD_band1;	/* Y            0x8207_040C     [15:00] */
	UINT_32 OFDM_PD_band1;	/* Y            0x8207_040C     [31:16] */
	UINT_32 CCK_SIG_Err_band1;	/* Y            0x8207_0410     [31:16] */
	UINT_32 CCK_SFD_Err_band1;	/* Y            0x8207_0410     [15:00] */
	UINT_32 OFDM_SIG_Err_band1;	/* Y            0x8207_0414     [31:16] */
	UINT_32 OFDM_TAG_Err_band1;	/* Y            0x8207_0414     [15:00] */
	UINT_32 PhyMdrdyCCK_band1;	/* Y            0x8207_0420     [15:00] */
	UINT_32 PhyMdrdyOFDM_band1;	/* Y            0x8207_0420     [31:16] */
	UINT_32 CCK_FCS_Err_band1;	/* Y            0x8207_041C     [15:00] */
	UINT_32 OFDM_FCS_Err_band1;	/* Y            0x8207_041C     [31:16] */
	UINT_32 MuPktCount;	/* Y            MT_ATEUpdateRxStatistic RXV1_2ND_CYCLE->GroupId */
} PARAM_RX_STAT_T, *P_PARAM_RX_STAT_T;
#else
typedef struct _PARAM_RX_STAT_T {
	UINT_32 MAC_FCS_Err;	/* b0 */
	UINT_32 MAC_Mdrdy;	/* b0 */
	UINT_32 FCSErr_CCK;
	UINT_32 FCSErr_OFDM;
	UINT_32 CCK_PD;
	UINT_32 OFDM_PD;
	UINT_32 CCK_SIG_Err;
	UINT_32 CCK_SFD_Err;
	UINT_32 OFDM_SIG_Err;
	UINT_32 OFDM_TAG_Err;
	UINT_32 WB_RSSI0;
	UINT_32 IB_RSSI0;
	UINT_32 WB_RSSI1;
	UINT_32 IB_RSSI1;
	UINT_32 PhyMdrdyCCK;
	UINT_32 PhyMdrdyOFDM;
	UINT_32 DriverRxCount;
	UINT_32 RCPI0;
	UINT_32 RCPI1;
	UINT_32 FreqOffsetFromRX;
	UINT_32 RSSI0;
	UINT_32 RSSI1;		/* insert new member here */
	UINT_32 OutOfResource;	/* MT7615 begin here */
	UINT_32 LengthMismatchCount_B0;
	UINT_32 MAC_FCS_Err1;	/* b1 */
	UINT_32 MAC_Mdrdy1;	/* b1 */
	UINT_32 FAGCRssiIBR0;
	UINT_32 FAGCRssiIBR1;
	UINT_32 FAGCRssiIBR2;
	UINT_32 FAGCRssiIBR3;
	UINT_32 FAGCRssiWBR0;
	UINT_32 FAGCRssiWBR1;
	UINT_32 FAGCRssiWBR2;
	UINT_32 FAGCRssiWBR3;

	UINT_32 InstRssiIBR0;
	UINT_32 InstRssiIBR1;
	UINT_32 InstRssiIBR2;
	UINT_32 InstRssiIBR3;
	UINT_32 InstRssiWBR0;
	UINT_32 InstRssiWBR1;
	UINT_32 InstRssiWBR2;
	UINT_32 InstRssiWBR3;
	UINT_32 ACIHitLower;
	UINT_32 ACIHitUpper;
	UINT_32 DriverRxCount1;
	UINT_32 RCPI2;
	UINT_32 RCPI3;
	UINT_32 RSSI2;
	UINT_32 RSSI3;
	UINT_32 SNR0;
	UINT_32 SNR1;
	UINT_32 SNR2;
	UINT_32 SNR3;
	UINT_32 OutOfResource1;
	UINT_32 LengthMismatchCount_B1;
	UINT_32 CCK_PD_Band1;
	UINT_32 OFDM_PD_Band1;
	UINT_32 CCK_SIG_Err_Band1;
	UINT_32 CCK_SFD_Err_Band1;
	UINT_32 OFDM_SIG_Err_Band1;
	UINT_32 OFDM_TAG_Err_Band1;
	UINT_32 PHY_CCK_MDRDY_Band1;
	UINT_32 PHY_OFDM_MDRDY_Band1;
	UINT_32 CCK_FCS_Err_Band1;
	UINT_32 OFDM_FCS_Err_Band1;
	UINT_32 MRURxCount;
	UINT_32 SIGMCS;
	UINT_32 SINR;
	UINT_32 RXVRSSI;
	UINT_32 Reserved[184];
	UINT_32 PHY_Mdrdy;
	UINT_32 Noise_Floor;
	UINT_32 AllLengthMismatchCount_B0;
	UINT_32 AllLengthMismatchCount_B1;
	UINT_32 AllMacMdrdy0;
	UINT_32 AllMacMdrdy1;
	UINT_32 AllFCSErr0;
	UINT_32 AllFCSErr1;
	UINT_32 RXOK0;
	UINT_32 RXOK1;
	UINT_32 PER0;
	UINT_32 PER1;
} PARAM_RX_STAT_T, *P_PARAM_RX_STAT_T;
extern PARAM_RX_STAT_T g_HqaRxStat;
#endif

typedef struct _HQA_CMD_FRAME {
	UINT_32 MagicNo;
	UINT_16 Type;
	UINT_16 Id;
	UINT_16 Length;
	UINT_16 Sequence;
	UCHAR Data[2048];
} __packed HQA_CMD_FRAME;

typedef INT_32(*HQA_CMD_HANDLER) (struct net_device *prNetDev,
				  IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame);

typedef struct _HQA_CMD_TABLE {
	HQA_CMD_HANDLER *CmdSet;
	UINT_32 CmdSetSize;
	UINT_32 CmdOffset;
} HQA_CMD_TABLE;

int HQA_CMDHandler(struct net_device *prNetDev, IN union iwreq_data *prIwReqData, HQA_CMD_FRAME *HqaCmdFrame);

int priv_qa_agent(IN struct net_device *prNetDev,
		  IN struct iw_request_info *prIwReqInfo, IN union iwreq_data *prIwReqData, IN char *pcExtra);

int priv_set_eeprom_mode(IN UINT_32 u4Mode);
#endif /*CFG_SUPPORT_QA_TOOL */
#endif /* _GL_QA_AGENT_H */
