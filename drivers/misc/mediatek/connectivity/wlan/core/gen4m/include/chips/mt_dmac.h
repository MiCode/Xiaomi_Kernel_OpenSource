/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology	5th Rd.
 * Science-based Industrial Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2004, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source	code is	an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering	the source code	is stricitly prohibited, unless	the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	mt_dmac.h

	Abstract:
	Ralink Wireless Chip MAC related definition & structures

	Revision History:
	Who			When		  What
	--------	----------	  -------------------------------------
*/

#ifndef __MT_DMAC_H__
#define __MT_DMAC_H__

struct TMAC_TXD_0 {
	/* DWORD 0 */
	uint32_t TxByteCount : 16;
	uint32_t EthTypeOffset : 7;
	uint32_t IpChkSumOffload : 1;
	uint32_t UdpTcpChkSumOffload : 1;
	uint32_t rsv_25 : 1;
	uint32_t q_idx : 5;
	uint32_t p_idx : 1;				/* P_IDX_XXX */
};

/* TMAC_TXD_0.PIdx */
#define P_IDX_LMAC	0
#define P_IDX_MCU	1
#define P_IDX_PLE_CTRL_PSE_PORT_3   3

#define UMAC_PLE_CTRL_P3_Q_0X1E     0x1e
#define UMAC_PLE_CTRL_P3_Q_0X1F     0x1f

/* TMAC_TXD_0.QIdx */
#define TxQ_IDX_AC0	0x00
#define TxQ_IDX_AC1	0x01
#define TxQ_IDX_AC2	0x02
#define TxQ_IDX_AC3	0x03
#define TxQ_IDX_AC10	0x04
#define TxQ_IDX_AC11	0x05
#define TxQ_IDX_AC12	0x06
#define TxQ_IDX_AC13	0x07
#define TxQ_IDX_AC20	0x08
#define TxQ_IDX_AC21	0x09
#define TxQ_IDX_AC22	0x0a
#define TxQ_IDX_AC23	0x0b
#define TxQ_IDX_AC30	0x0c
#define TxQ_IDX_AC31	0x0d
#define TxQ_IDX_AC32	0x0e
#define TxQ_IDX_AC33	0x0f

#define TxQ_IDX_ALTX0	0x10
#define TxQ_IDX_BMC0	0x11
#define TxQ_IDX_BCN0	0x12
#define TxQ_IDX_PSMP0	0x13

#define TxQ_IDX_ALTX1	0x14
#define TxQ_IDX_BMC1	0x15
#define TxQ_IDX_BCN1	0x16
#define TxQ_IDX_PSMP1	0x17

#define TxQ_IDX_MCU0	0x00
#define TxQ_IDX_MCU1	0x01
#define TxQ_IDX_MCU2	0x02
#define TxQ_IDX_MCU3	0x03
#define TxQ_IDX_MCU_PDA	0x1e


/* TMAC_TXD_0.p_idx +  TMAC_TXD_0.q_idx */
#define PQ_IDX_WMM0_AC0	((P_IDX_LMAC << 5) | (TxQ_IDX_AC0))
#define PQ_IDX_WMM0_AC1	((P_IDX_LMAC << 5) | (TxQ_IDX_AC1))
#define PQ_IDX_WMM0_AC2	((P_IDX_LMAC << 5) | (TxQ_IDX_AC2))
#define PQ_IDX_WMM0_AC3	((P_IDX_LMAC << 5) | (TxQ_IDX_AC3))
#define PQ_IDX_WMM1_AC0	((P_IDX_LMAC << 5) | (TxQ_IDX_AC10))
#define PQ_IDX_WMM1_AC1	((P_IDX_LMAC << 5) | (TxQ_IDX_AC11))
#define PQ_IDX_WMM1_AC2	((P_IDX_LMAC << 5) | (TxQ_IDX_AC12))
#define PQ_IDX_WMM1_AC3	((P_IDX_LMAC << 5) | (TxQ_IDX_AC13))
#define PQ_IDX_WMM2_AC0	((P_IDX_LMAC << 5) | (TxQ_IDX_AC20))
#define PQ_IDX_WMM2_AC1	((P_IDX_LMAC << 5) | (TxQ_IDX_AC21))
#define PQ_IDX_WMM2_AC2	((P_IDX_LMAC << 5) | (TxQ_IDX_AC22))
#define PQ_IDX_WMM2_AC3	((P_IDX_LMAC << 5) | (TxQ_IDX_AC23))
#define PQ_IDX_WMM3_AC0	((P_IDX_LMAC << 5) | (TxQ_IDX_AC30))
#define PQ_IDX_WMM3_AC1	((P_IDX_LMAC << 5) | (TxQ_IDX_AC31))
#define PQ_IDX_WMM3_AC2	((P_IDX_LMAC << 5) | (TxQ_IDX_AC32))
#define PQ_IDX_WMM3_AC3	((P_IDX_LMAC << 5) | (TxQ_IDX_AC33))

#define PQ_IDX_ALTX0	((P_IDX_LMAC << 5) | (TxQ_IDX_ALTX0))
#define PQ_IDX_BMC0		((P_IDX_LMAC << 5) | (TxQ_IDX_ALTX0))
#define PQ_IDX_BCN0		((P_IDX_LMAC << 5) | (TxQ_IDX_ALTX0))
#define PQ_IDX_PSMP0	((P_IDX_LMAC << 5) | (TxQ_IDX_ALTX0))

#define PQ_IDX_MCU_RQ0		((P_IDX_MCU << 5) | (TxQ_IDX_MCU0))
#define PQ_IDX_MCU_RQ1		((P_IDX_MCU << 5) | (TxQ_IDX_MCU1))
#define PQ_IDX_MCU_RQ2		((P_IDX_MCU << 5) | (TxQ_IDX_MCU2))
#define PQ_IDX_MCU_RQ3		((P_IDX_MCU << 5) | (TxQ_IDX_MCU3))

struct TMAC_TXD_1 {
	/* DWORD 1 */
	uint32_t wlan_idx : 8;
	uint32_t hdr_info : 5;			/* in unit of WORD(2 bytes) */
	uint32_t hdr_format : 2;			/* TMI_HDR_FT_XXX */
	uint32_t ft : 1;			/* TMI_HDR_FT_XXX */
	uint32_t txd_len : 1;				/* TMI_FT_XXXX */
	uint32_t hdr_pad : 2;
	uint32_t UNxV : 1;
	uint32_t amsdu : 1;
	uint32_t tid : 3;
	uint32_t pkt_ft : 2;
	uint32_t OwnMacAddr : 6;
};

/* TMAC_TXD_1.pkt_ft */
#define	TMI_PKT_FT_HIF_CT		0	/* Cut-through */
#define	TMI_PKT_FT_HIF_SF		1	/* Store & forward  */
#define	TMI_PKT_FT_HIF_CMD	2	/* Command frame to N9/CR4 */
#define	TMI_PKT_FT_HIF_FW		3	/* Firmware frame to PDA */

#define TMI_PKT_FT_MCU_CT       0   /* N9 Cut-through */
#define TMI_PKT_FT_MCU_FW       3   /* N9 to UMAC/LMAC */

/* TMAC_TXD_1.hdr_format */
#define TMI_HDR_FT_NON_80211	0x0
#define TMI_HDR_FT_CMD		0x1
#define TMI_HDR_FT_NOR_80211	0x2
#define TMI_HDR_FT_ENH_80211	0x3

/* if TMAC_TXD_1.hdr_format  == HDR_FORMAT_NON_80211 */
#define TMI_HDR_INFO_0_BIT_MRD		0
#define TMI_HDR_INFO_0_BIT_EOSP		1
#define TMI_HDR_INFO_0_BIT_RMVL		2
#define TMI_HDR_INFO_0_BIT_VLAN		3
#define TMI_HDR_INFO_0_BIT_ETYP		4
#define TMI_HDR_INFO_0_VAL(_mrd, _eosp, _rmvl, _vlan, _etyp)	\
	((((_mrd) ? 1 : 0) << TMI_HDR_INFO_0_BIT_MRD) | \
	 (((_eosp) ? 1 : 0) << TMI_HDR_INFO_0_BIT_EOSP) |\
	 (((_rmvl) ? 1 : 0) << TMI_HDR_INFO_0_BIT_RMVL) |\
	 (((_vlan) ? 1 : 0) << TMI_HDR_INFO_0_BIT_VLAN) |\
	 (((_etyp) ? 1 : 0) << TMI_HDR_INFO_0_BIT_ETYP))


/* if TMAC_TXD_1.hdr_format  == HDR_FORMAT_CMD */
#define TMI_HDR_INFO_1_MASK_RSV		(0x1f)
#define TMI_HDR_INFO_1_VAL			0

/* if TMAC_TXD_1.hdr_format  == HDR_FORMAT_NOR_80211 */
#define TMI_HDR_INFO_2_MASK_LEN	(0x1f)
#define TMI_HDR_INFO_2_VAL(_len)	(_len >> 1)

/* if TMAC_TXD_1.hdr_format  == HDR_FORMAT_ENH_80211 */
#define TMI_HDR_INFO_3_BIT_EOSP	1
#define TMI_HDR_INFO_3_BIT_AMS	2
#define TMI_HDR_INFO_3_VAL(_eosp, _ams)	\
	((((_eosp) ? 1 : 0) << TMI_HDR_INFO_3_BIT_EOSP) | \
	 (((_ams) ? 1 : 0) << TMI_HDR_INFO_3_BIT_AMS))

/* TMAC_TXD_1.TxDFmt */
#define TMI_FT_SHORT	0
#define TMI_FT_LONG		1

/* TMAC_TXD_1.HdrPad */
#define TMI_HDR_PAD_BIT_MODE		1
#define TMI_HDR_PAD_MODE_TAIL		0
#define TMI_HDR_PAD_MODE_HEAD	1
#define TMI_HDR_PAD_BIT_LEN	0
#define TMI_HDR_PAD_MASK_LEN	0x3

struct TMAC_TXD_2 {
	/* DWORD 2 */
	uint32_t sub_type : 4;
	uint32_t frm_type : 2;
	uint32_t ndp : 1;
	uint32_t ndpa : 1;
	uint32_t sounding : 1;
	uint32_t rts : 1;
	uint32_t bc_mc_pkt : 1;
	uint32_t bip : 1;
	uint32_t duration : 1;
	uint32_t htc_vld : 1;
	uint32_t frag : 2;
	uint32_t max_tx_time : 8;
	uint32_t pwr_offset : 5;
	uint32_t ba_disable : 1;
	uint32_t timing_measure : 1;
	uint32_t fix_rate : 1;
};

struct TMAC_TXD_3 {
	/* DWORD 3 */
	uint32_t no_ack : 1;
	uint32_t protect_frm : 1;
	uint32_t rsv_2_5 : 4;
	uint32_t tx_cnt : 5;
	uint32_t remain_tx_cnt : 5;
	uint32_t sn : 12;
	uint32_t rsv_28_29 : 2;
	uint32_t pn_vld : 1;
	uint32_t sn_vld : 1;
};

struct TMAC_TXD_4 {
	/* DWORD 4 */
	uint32_t pn_low;
};

struct TMAC_TXD_5 {
	/* DWORD 5 */
	uint32_t pid:8;
	uint32_t tx_status_fmt:1;
	uint32_t tx_status_2_mcu:1;
	uint32_t tx_status_2_host:1;
	uint32_t da_select:1;
	uint32_t rsv_12:1;
	uint32_t pwr_mgmt:1;
	uint32_t rsv_14_15:2;
	uint32_t pn_high:16;
};

/* TMAC_TXD_5.da_select */
#define TMI_DAS_FROM_MPDU		0
#define TMI_DAS_FROM_WTBL		1

/* TMAC_TXD_5.bar_sn_ctrl */
#define TMI_BSN_CFG_BY_HW	0
#define TMI_BSN_CFG_BY_SW	1

/* TMAC_TXD_5.pwr_mgmt */
#define TMI_PM_BIT_CFG_BY_HW	0
#define TMI_PM_BIT_CFG_BY_SW	1

struct TMAC_TXD_6 {
	/* DWORD 6 */
	uint32_t bw:3;
	uint32_t dyn_bw:1;
	uint32_t ant_id:12;
	uint32_t tx_rate:12;
	uint32_t TxBF:1;
	uint32_t ldpc:1;
	uint32_t gi:1;
	uint32_t fix_rate_mode:1;
};

/* TMAC_TXD_6.fix_rate_mode */
#define TMI_FIX_RATE_BY_TXD	0
#define TMI_FIX_RATE_BY_CR		1

#define TMI_TX_RATE_BIT_STBC		11
#define TMI_TX_RATE_BIT_NSS		9
#define TMI_TX_RATE_MASK_NSS		0x3

#define TMI_TX_RATE_BIT_MODE		6
#define TMI_TX_RATE_MASK_MODE	0x7
#define TMI_TX_RATE_MODE_CCK		0
#define TMI_TX_RATE_MODE_OFDM	1
#define TMI_TX_RATE_MODE_HTMIX	2
#define TMI_TX_RATE_MODE_HTGF		3
#define TMI_TX_RATE_MODE_VHT		4

#define SHORT_PREAMBLE 0
#define LONG_PREAMBLE 1

#define TMI_TX_RATE_BIT_MCS		0
#define TMI_TX_RATE_MASK_MCS		0x3f
#define TMI_TX_RATE_CCK_1M_LP		0
#define TMI_TX_RATE_CCK_2M_LP		1
#define TMI_TX_RATE_CCK_5M_LP		2
#define TMI_TX_RATE_CCK_11M_LP	3

#define TMI_TX_RATE_CCK_2M_SP		5
#define TMI_TX_RATE_CCK_5M_SP		6
#define TMI_TX_RATE_CCK_11M_SP	7

#define TMI_TX_RATE_OFDM_6M		11
#define TMI_TX_RATE_OFDM_9M		15
#define TMI_TX_RATE_OFDM_12M		10
#define TMI_TX_RATE_OFDM_18M		14
#define TMI_TX_RATE_OFDM_24M		9
#define TMI_TX_RATE_OFDM_36M		13
#define TMI_TX_RATE_OFDM_48M		8
#define TMI_TX_RATE_OFDM_54M		12

#define TMI_TX_RATE_HT_MCS0		0
#define TMI_TX_RATE_HT_MCS1		1
#define TMI_TX_RATE_HT_MCS2		2
#define TMI_TX_RATE_HT_MCS3		3
#define TMI_TX_RATE_HT_MCS4		4
#define TMI_TX_RATE_HT_MCS5		5
#define TMI_TX_RATE_HT_MCS6		6
#define TMI_TX_RATE_HT_MCS7		7
#define TMI_TX_RATE_HT_MCS8		8
#define TMI_TX_RATE_HT_MCS9		9
#define TMI_TX_RATE_HT_MCS10		10
#define TMI_TX_RATE_HT_MCS11		11
#define TMI_TX_RATE_HT_MCS12		12
#define TMI_TX_RATE_HT_MCS13		13
#define TMI_TX_RATE_HT_MCS14		14
#define TMI_TX_RATE_HT_MCS15		15
#define TMI_TX_RATE_HT_MCS16		16
#define TMI_TX_RATE_HT_MCS17		17
#define TMI_TX_RATE_HT_MCS18		18
#define TMI_TX_RATE_HT_MCS19		19
#define TMI_TX_RATE_HT_MCS20		20
#define TMI_TX_RATE_HT_MCS21		21
#define TMI_TX_RATE_HT_MCS22		22
#define TMI_TX_RATE_HT_MCS23		23

#define TMI_TX_RATE_HT_MCS32		32

#define TMI_TX_RATE_VHT_MCS0		0
#define TMI_TX_RATE_VHT_MCS1		1
#define TMI_TX_RATE_VHT_MCS2		2
#define TMI_TX_RATE_VHT_MCS3		3
#define TMI_TX_RATE_VHT_MCS4		4
#define TMI_TX_RATE_VHT_MCS5		5
#define TMI_TX_RATE_VHT_MCS6		6
#define TMI_TX_RATE_VHT_MCS7		7
#define TMI_TX_RATE_VHT_MCS8		8
#define TMI_TX_RATE_VHT_MCS9		9

struct TMAC_TXD_7 {
	uint32_t sw_tx_time:10;
	uint32_t rsv_10:1;
	uint32_t spe_idx:5;
	uint32_t pse_fid:14;
	uint32_t hw_amsdu_cap:1;
	uint32_t hif_err:1;
};

struct TMAC_TXD_L {
	struct TMAC_TXD_0 TxD0;
	struct TMAC_TXD_1 TxD1;
	struct TMAC_TXD_2 TxD2;
	struct TMAC_TXD_3 TxD3;
	struct TMAC_TXD_4 TxD4;
	struct TMAC_TXD_5 TxD5;
	struct TMAC_TXD_6 TxD6;
	struct TMAC_TXD_7 TxD7;
};

struct TMAC_TXD_S {
	struct TMAC_TXD_0 TxD0;
	struct TMAC_TXD_1 TxD1;
	struct TMAC_TXD_7 TxD7;
};

#endif /* __MT_DMAC_H__ */
