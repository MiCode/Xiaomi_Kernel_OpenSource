/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MT2712__REGACC__H__
#define __MT2712__REGACC__H__

#define MAKE_MASK_32(e, s) (((e) - (s)) == 31 ? 0xffffffffUL :\
				((1UL << ((e) - (s) + 1)) - 1))

#define MAKE_MASK_64(e, s) (((e) - (s)) == 63 ? 0xffffffffffffffffULL :\
				((1ULL << ((e) - (s) + 1)) - 1))

#define GET_BITS(e, s, reg, data) \
	(data = ((e) - (s) > 31) ?\
	(((reg) >> (s)) & MAKE_MASK_64(e,  s)) :\
	(((reg) >> (s)) & MAKE_MASK_32(e, s)))

#define SET_BITS(e, s, reg, val) do { \
	if ((e) - (s) > 31) { \
		reg = ((((val) << (s)) & (MAKE_MASK_64((e), (s)) << (s))) |\
		((reg) & (~(MAKE_MASK_64((e), (s)) << (s))))); \
	} \
	else { \
		reg = ((((val) << (s)) & (MAKE_MASK_32((e), (s)) << (s))) |\
		((reg) & (~(MAKE_MASK_32((e), (s)) << (s))))); \
	} \
} while (0)

#define TX_NORMAL_DESC_TDES2_HL_B1L_HBIT_POS  0xd
#define TX_NORMAL_DESC_TDES2_HL_B1L_LBIT_POS  0

#define TX_NORMAL_DESC_TDES2_B2L_HBIT_POS  0x1d
#define TX_NORMAL_DESC_TDES2_B2L_LBIT_POS  0x10

#define TX_NORMAL_DESC_TDES2_TTSE_HBIT_POS  0x1e
#define TX_NORMAL_DESC_TDES2_TTSE_LBIT_POS  0x1e

#define TX_NORMAL_DESC_TDES2_IC_HBIT_POS  0x1f
#define TX_NORMAL_DESC_TDES2_IC_LBIT_POS  0x1f

#define TX_NORMAL_DESC_TDES3_FL_HBIT_POS  0xe
#define TX_NORMAL_DESC_TDES3_FL_LBIT_POS  0

#define TX_NORMAL_DESC_TDES3_FD_HBIT_POS  0x1d
#define TX_NORMAL_DESC_TDES3_FD_LBIT_POS  0x1d

#define TX_NORMAL_DESC_TDES3_CPC_HBIT_POS  0x1b
#define TX_NORMAL_DESC_TDES3_CPC_LBIT_POS  0x1a

#define TX_NORMAL_DESC_TDES3_LD_HBIT_POS  0x1c
#define TX_NORMAL_DESC_TDES3_LD_LBIT_POS  0x1c

#define TX_NORMAL_DESC_TDES3_CTXT_HBIT_POS  0x1e
#define TX_NORMAL_DESC_TDES3_CTXT_LBIT_POS  0x1e

#define TX_NORMAL_DESC_TDES3_OWN_HBIT_POS  0x1f
#define TX_NORMAL_DESC_TDES3_OWN_LBIT_POS  0x1f

#define RX_CONTEXT_DESC_RDES3_OWN_HBIT_POS  0x1f
#define RX_CONTEXT_DESC_RDES3_OWN_LBIT_POS  0x1f

#define RX_CONTEXT_DESC_RDES3_CTXT_HBIT_POS  0x1e
#define RX_CONTEXT_DESC_RDES3_CTXT_LBIT_POS  0x1e

#define RX_NORMAL_DESC_RDES3_RS1V_HBIT_POS  0x1a
#define RX_NORMAL_DESC_RDES3_RS1V_LBIT_POS  0x1a

#define RX_NORMAL_DESC_RDES1_TSA_HBIT_POS  0xe
#define RX_NORMAL_DESC_RDES1_TSA_LBIT_POS  0xe

#define TX_NORMAL_DESC_TDES2_HL_B1L_MLF_WR(ptr, data) \
	({SET_BITS(TX_NORMAL_DESC_TDES2_HL_B1L_HBIT_POS, \
			TX_NORMAL_DESC_TDES2_HL_B1L_LBIT_POS, ptr, data); })

#define TX_NORMAL_DESC_TDES2_B2L_MLF_WR(ptr, data) \
	({SET_BITS(TX_NORMAL_DESC_TDES2_B2L_HBIT_POS, \
			TX_NORMAL_DESC_TDES2_B2L_LBIT_POS, ptr, data); })

#define TX_NORMAL_DESC_TDES2_TTSE_MLF_WR(ptr, data) \
	({SET_BITS(TX_NORMAL_DESC_TDES2_TTSE_HBIT_POS, \
			TX_NORMAL_DESC_TDES2_TTSE_LBIT_POS, ptr, data); })

#define TX_NORMAL_DESC_TDES3_FL_MLF_WR(ptr, data) \
	({SET_BITS(TX_NORMAL_DESC_TDES3_FL_HBIT_POS, \
			TX_NORMAL_DESC_TDES3_FL_LBIT_POS, ptr, data); })

#define TX_NORMAL_DESC_TDES3_FD_MLF_WR(ptr, data) \
	({SET_BITS(TX_NORMAL_DESC_TDES3_FD_HBIT_POS, \
			TX_NORMAL_DESC_TDES3_FD_LBIT_POS, ptr, data); })

#define TX_NORMAL_DESC_TDES3_CTXT_MLF_WR(ptr, data) \
	({SET_BITS(TX_NORMAL_DESC_TDES3_CTXT_HBIT_POS, \
			TX_NORMAL_DESC_TDES3_CTXT_LBIT_POS, ptr, data); })

#define TX_NORMAL_DESC_TDES3_OWN_MLF_WR(ptr, data) \
	({SET_BITS(TX_NORMAL_DESC_TDES3_OWN_HBIT_POS, \
			TX_NORMAL_DESC_TDES3_OWN_LBIT_POS, ptr, data); })

#define TX_NORMAL_DESC_TDES2_IC_MLF_WR(ptr, data) \
	({SET_BITS(TX_NORMAL_DESC_TDES2_IC_HBIT_POS, \
			TX_NORMAL_DESC_TDES2_IC_LBIT_POS, ptr, data); })

#define TX_NORMAL_DESC_TDES3_CPC_MLF_WR(ptr, data) \
	({SET_BITS(TX_NORMAL_DESC_TDES3_CPC_HBIT_POS, \
			TX_NORMAL_DESC_TDES3_CPC_LBIT_POS, ptr, data); })

#define TX_NORMAL_DESC_TDES3_LD_MLF_RD(ptr, data) \
	({GET_BITS(TX_NORMAL_DESC_TDES3_LD_HBIT_POS, \
			TX_NORMAL_DESC_TDES3_LD_LBIT_POS, ptr, data); })

#define TX_NORMAL_DESC_TDES3_LD_MLF_WR(ptr, data) \
	({SET_BITS(TX_NORMAL_DESC_TDES3_LD_HBIT_POS, \
			TX_NORMAL_DESC_TDES3_LD_LBIT_POS, ptr, data); })

#define TX_NORMAL_DESC_TDES3_CTXT_MLF_RD(ptr, data) \
	({GET_BITS(TX_NORMAL_DESC_TDES3_CTXT_HBIT_POS, \
			TX_NORMAL_DESC_TDES3_CTXT_LBIT_POS, ptr, data); })

#define TX_NORMAL_DESC_TDES3_OWN_MLF_RD(ptr, data) \
	({GET_BITS(TX_NORMAL_DESC_TDES3_OWN_HBIT_POS, \
			TX_NORMAL_DESC_TDES3_OWN_LBIT_POS, ptr, data); })

#define RX_CONTEXT_DESC_RDES3_OWN_MLF_RD(ptr, data) \
	({GET_BITS(RX_CONTEXT_DESC_RDES3_OWN_HBIT_POS, \
			RX_CONTEXT_DESC_RDES3_OWN_LBIT_POS, ptr, data); })

#define RX_CONTEXT_DESC_RDES3_CTXT_MLF_RD(ptr, data) \
	({GET_BITS(RX_CONTEXT_DESC_RDES3_CTXT_HBIT_POS, \
			RX_CONTEXT_DESC_RDES3_CTXT_LBIT_POS, ptr, data); })

#define RX_NORMAL_DESC_RDES3_RS1V_MLF_RD(ptr, data) \
	({GET_BITS(RX_NORMAL_DESC_RDES3_RS1V_HBIT_POS, \
			RX_NORMAL_DESC_RDES3_RS1V_LBIT_POS, ptr, data); })

#define RX_NORMAL_DESC_RDES1_TSA_MLF_RD(ptr, data) \
	({GET_BITS(RX_NORMAL_DESC_RDES1_TSA_HBIT_POS, \
			RX_NORMAL_DESC_RDES1_TSA_LBIT_POS, ptr, data); })

#define TX_NORMAL_DESC_TDES0_ML_RD(ptr, data) ({data = ptr; })
#define TX_NORMAL_DESC_TDES0_ML_WR(ptr, data) ({ptr = data; })
#define TX_NORMAL_DESC_TDES1_ML_RD(ptr, data) ({data = ptr; })
#define TX_NORMAL_DESC_TDES1_ML_WR(ptr, data) ({ptr = data; })
#define TX_NORMAL_DESC_TDES2_ML_WR(ptr, data) ({ptr = data; })
#define TX_NORMAL_DESC_TDES3_ML_RD(ptr, data) ({data = ptr; })
#define TX_NORMAL_DESC_TDES3_ML_WR(ptr, data) ({ptr = data; })
#define RX_NORMAL_DESC_RDES0_ML_WR(ptr, data) ({ptr = data; })
#define RX_NORMAL_DESC_RDES1_ML_WR(ptr, data) ({ptr = data; })
#define RX_NORMAL_DESC_RDES2_ML_WR(ptr, data) ({ptr = data; })
#define RX_NORMAL_DESC_RDES3_ML_RD(ptr, data) ({data = ptr; })
#define RX_NORMAL_DESC_RDES3_ML_WR(ptr, data) ({ptr = data; })
#define RX_CONTEXT_DESC_RDES0_ML_RD(ptr, data) ({data = ptr; })
#define RX_CONTEXT_DESC_RDES1_ML_RD(ptr, data) ({data = ptr; })

#define TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_HBIT_POS  0x2
#define TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_LBIT_POS  0x2

#define TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_MLF_RD(ptr, data) \
	({GET_BITS(TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_HBIT_POS, \
			TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_LBIT_POS, ptr, data); })

#define TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_MLF_WR(ptr, data) \
	({SET_BITS(TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_HBIT_POS, \
			TX_PKT_FEATURES_PKT_ATTRIBUTES_PTP_ENABLE_LBIT_POS, ptr, data); })

#define BASE_ADDRESS dwc_eth_qos_platform_base_addr

#define MAC_MCR_REG_ADDR (BASE_ADDRESS + 0)
#define MAC_MCR_REG_WR(data) (iowrite32(data, (void *)MAC_MCR_REG_ADDR))
#define MAC_MCR_REG_RD(data) ((data) = ioread32((void *)MAC_MCR_REG_ADDR))

#define MAC_MCR_MASK_7 (unsigned long)(0x1)
#define MAC_MCR_RES_WR_MASK_7 (unsigned long)(0xffffff7f)

#define MAC_MCR_DM_MASK (unsigned long)(0x1)
#define MAC_MCR_DM_WR_MASK (unsigned long)(0xffffdfff)
#define MAC_MCR_DM_UDFWR(data) do {\
		unsigned long v;\
		MAC_MCR_REG_RD(v);\
		v = (v & (MAC_MCR_RES_WR_MASK_7)) | ((0 & (MAC_MCR_MASK_7)) << 7);\
		v = ((v & MAC_MCR_DM_WR_MASK) | (((data) & MAC_MCR_DM_MASK) << 13));\
		MAC_MCR_REG_WR(v);\
} while (0)

#define MAC_MCR_FES_MASK (unsigned long)(0x1)
#define MAC_MCR_FES_WR_MASK (unsigned long)(0xffffbfff)
#define MAC_MCR_FES_UDFWR(data) do {\
		unsigned long v;\
		MAC_MCR_REG_RD(v);\
		v = (v & (MAC_MCR_RES_WR_MASK_7)) | ((0 & (MAC_MCR_MASK_7)) << 7);\
		v = ((v & MAC_MCR_FES_WR_MASK) | (((data) & MAC_MCR_FES_MASK) << 14));\
		MAC_MCR_REG_WR(v);\
} while (0)

#define MAC_MCR_PS_MASK (unsigned long)(0x1)
#define MAC_MCR_PS_WR_MASK (unsigned long)(0xffff7fff)
#define MAC_MCR_PS_UDFWR(data) do {\
		unsigned long v;\
		MAC_MCR_REG_RD(v);\
		v = (v & (MAC_MCR_RES_WR_MASK_7)) | ((0 & (MAC_MCR_MASK_7)) << 7);\
		v = ((v & MAC_MCR_PS_WR_MASK) | (((data) & MAC_MCR_PS_MASK) << 15));\
		MAC_MCR_REG_WR(v);\
} while (0)

#define MAC_MPFR_REG_ADDR (BASE_ADDRESS + 0x8)
#define MAC_MPFR_REG_WR(data) (iowrite32(data, (void *)MAC_MPFR_REG_ADDR))
#define MAC_MPFR_REG_RD(data) ((data) = ioread32((void *)MAC_MPFR_REG_ADDR))

#define MAC_HTR_REG_ADDR (BASE_ADDRESS + 0x10)
#define MAC_HTR_REG_ADDRESS(i) (MAC_HTR_REG_ADDR + ((i) * 4))
#define MAC_HTR_REG_WR(i, data) (iowrite32(data, (void *)MAC_HTR_REG_ADDRESS(i)))

#define MAC_QTFCR_REG_ADDR (BASE_ADDRESS + 0x70)
#define MAC_QTFCR_REG_ADDRESS(i) (MAC_QTFCR_REG_ADDR + ((i) * 4))
#define MAC_QTFCR_REG_WR(i, data) (iowrite32(data, (void *)MAC_QTFCR_REG_ADDRESS(i)))
#define MAC_QTFCR_REG_RD(i, data) ((data) = ioread32((void *)MAC_QTFCR_REG_ADDRESS(i)))

#define MAC_QTFCR_MASK_8 (unsigned long)(0xff)
#define MAC_QTFCR_RES_WR_MASK_8 (unsigned long)(0xffff00ff)
#define MAC_QTFCR_MASK_2 (unsigned long)(0x3)
#define MAC_QTFCR_RES_WR_MASK_2 (unsigned long)(0xfffffff3)

#define MAC_QTFCR_TFE_MASK (unsigned long)(0x1)
#define MAC_QTFCR_TFE_WR_MASK (unsigned long)(0xfffffffd)
#define MAC_QTFCR_TFE_UDFWR(i, data) do {\
		unsigned long v;\
		MAC_QTFCR_REG_RD(i, v);\
		v = (v & (MAC_QTFCR_RES_WR_MASK_8)) | ((0 & (MAC_QTFCR_MASK_8)) << 8);\
		v = (v & (MAC_QTFCR_RES_WR_MASK_2)) | ((0 & (MAC_QTFCR_MASK_2)) << 2);\
		v = ((v & MAC_QTFCR_TFE_WR_MASK) | (((data) & MAC_QTFCR_TFE_MASK) << 1));\
		MAC_QTFCR_REG_WR(i, v);\
} while (0)

#define MAC_QTFCR_PT_MASK (unsigned long)(0xffff)
#define MAC_QTFCR_PT_WR_MASK (unsigned long)(0xffff)
#define MAC_QTFCR_PT_UDFWR(i, data) do {\
		unsigned long v;\
		MAC_QTFCR_REG_RD(i, v);\
		v = (v & (MAC_QTFCR_RES_WR_MASK_8)) | ((0 & (MAC_QTFCR_MASK_8)) << 8);\
		v = (v & (MAC_QTFCR_RES_WR_MASK_2)) | ((0 & (MAC_QTFCR_MASK_2)) << 2);\
		v = ((v & MAC_QTFCR_PT_WR_MASK) | (((data) & MAC_QTFCR_PT_MASK) << 16));\
		MAC_QTFCR_REG_WR(i, v);\
} while (0)

#define MAC_RFCR_REG_ADDR (BASE_ADDRESS + 0x90)
#define MAC_RFCR_REG_WR(data) (iowrite32(data, (void *)MAC_RFCR_REG_ADDR))
#define MAC_RFCR_REG_RD(data) ((data) = ioread32((void *)MAC_RFCR_REG_ADDR))

#define MAC_RFCR_MASK_9 (unsigned long)(0x7fffff)
#define MAC_RFCR_RES_WR_MASK_9 (unsigned long)(0x1ff)
#define MAC_RFCR_MASK_2 (unsigned long)(0x3f)
#define MAC_RFCR_RES_WR_MASK_2 (unsigned long)(0xffffff03)

#define MAC_RFCR_RFE_MASK (unsigned long)(0x1)
#define MAC_RFCR_RFE_WR_MASK (unsigned long)(0xfffffffe)
#define MAC_RFCR_RFE_UDFWR(data) do {\
		unsigned long v;\
		MAC_RFCR_REG_RD(v);\
		v = (v & (MAC_RFCR_RES_WR_MASK_9)) | ((0 & (MAC_RFCR_MASK_9)) << 9);\
		v = (v & (MAC_RFCR_RES_WR_MASK_2)) | ((0 & (MAC_RFCR_MASK_2)) << 2);\
		v = ((v & MAC_RFCR_RFE_WR_MASK) | (((data) & MAC_RFCR_RFE_MASK) << 0));\
		MAC_RFCR_REG_WR(v);\
} while (0)

#define MAC_TQPM0R_REG_ADDR (BASE_ADDRESS + 0x98)
#define MAC_TQPM0R_REG_WR(data) (iowrite32(data, (void *)MAC_TQPM0R_REG_ADDR))
#define MAC_TQPM0R_REG_RD(data) ((data) = ioread32((void *)MAC_TQPM0R_REG_ADDR))

#define MAC_TQPM0R_PSTQ0_MASK (unsigned long)(0xff)
#define MAC_TQPM0R_PSTQ0_WR_MASK (unsigned long)(0xffffff00)
#define MAC_TQPM0R_PSTQ0_UDFWR(data) do {\
		unsigned long v;\
		MAC_TQPM0R_REG_RD(v);\
		v = ((v & MAC_TQPM0R_PSTQ0_WR_MASK) | (((data) & MAC_TQPM0R_PSTQ0_MASK) << 0));\
		MAC_TQPM0R_REG_WR(v);\
} while (0)

#define MAC_TQPM0R_PSTQ1_MASK (unsigned long)(0xff)
#define MAC_TQPM0R_PSTQ1_WR_MASK (unsigned long)(0xffff00ff)
#define MAC_TQPM0R_PSTQ1_UDFWR(data) do {\
		unsigned long v;\
		MAC_TQPM0R_REG_RD(v);\
		v = ((v & MAC_TQPM0R_PSTQ1_WR_MASK) | (((data) & MAC_TQPM0R_PSTQ1_MASK) << 8));\
		MAC_TQPM0R_REG_WR(v);\
} while (0)

#define MAC_TQPM0R_PSTQ2_MASK (unsigned long)(0xff)
#define MAC_TQPM0R_PSTQ2_WR_MASK (unsigned long)(0xff00ffff)
#define MAC_TQPM0R_PSTQ2_UDFWR(data) do {\
		unsigned long v;\
		MAC_TQPM0R_REG_RD(v);\
		v = ((v & MAC_TQPM0R_PSTQ2_WR_MASK) | (((data) & MAC_TQPM0R_PSTQ2_MASK) << 16));\
		MAC_TQPM0R_REG_WR(v);\
} while (0)

#define MAC_TQPM0R_PSTQ3_MASK (unsigned long)(0xff)
#define MAC_TQPM0R_PSTQ3_WR_MASK (unsigned long)(0xffffff)
#define MAC_TQPM0R_PSTQ3_UDFWR(data) do {\
		unsigned long v;\
		MAC_TQPM0R_REG_RD(v);\
		v = ((v & MAC_TQPM0R_PSTQ3_WR_MASK) | (((data) & MAC_TQPM0R_PSTQ3_MASK) << 24));\
		MAC_TQPM0R_REG_WR(v);\
} while (0)

#define MAC_TQPM1R_REG_ADDR (BASE_ADDRESS + 0x9c)
#define MAC_TQPM1R_REG_WR(data) (iowrite32(data, (void *)MAC_TQPM1R_REG_ADDR))
#define MAC_TQPM1R_REG_RD(data) ((data) = ioread32((void *)MAC_TQPM1R_REG_ADDR))

#define MAC_TQPM1R_PSTQ4_MASK (unsigned long)(0xff)
#define MAC_TQPM1R_PSTQ4_WR_MASK (unsigned long)(0xffffff00)
#define MAC_TQPM1R_PSTQ4_UDFWR(data) do {\
		unsigned long v;\
		MAC_TQPM1R_REG_RD(v);\
		v = ((v & MAC_TQPM1R_PSTQ4_WR_MASK) | (((data) & MAC_TQPM1R_PSTQ4_MASK) << 0));\
		MAC_TQPM1R_REG_WR(v);\
} while (0)

#define MAC_TQPM1R_PSTQ5_MASK (unsigned long)(0xff)
#define MAC_TQPM1R_PSTQ5_WR_MASK (unsigned long)(0xffff00ff)
#define MAC_TQPM1R_PSTQ5_UDFWR(data) do {\
		unsigned long v;\
		MAC_TQPM1R_REG_RD(v);\
		v = ((v & MAC_TQPM1R_PSTQ5_WR_MASK) | (((data) & MAC_TQPM1R_PSTQ5_MASK) << 8));\
		MAC_TQPM1R_REG_WR(v);\
} while (0)

#define MAC_TQPM1R_PSTQ6_MASK (unsigned long)(0xff)
#define MAC_TQPM1R_PSTQ6_WR_MASK (unsigned long)(0xff00ffff)
#define MAC_TQPM1R_PSTQ6_UDFWR(data) do {\
		unsigned long v;\
		MAC_TQPM1R_REG_RD(v);\
		v = ((v & MAC_TQPM1R_PSTQ6_WR_MASK) | (((data) & MAC_TQPM1R_PSTQ6_MASK) << 16));\
		MAC_TQPM1R_REG_WR(v);\
} while (0)

#define MAC_TQPM1R_PSTQ7_MASK (unsigned long)(0xff)
#define MAC_TQPM1R_PSTQ7_WR_MASK (unsigned long)(0xffffff)
#define MAC_TQPM1R_PSTQ7_UDFWR(data) do {\
		unsigned long v;\
		MAC_TQPM1R_REG_RD(v);\
		v = ((v & MAC_TQPM1R_PSTQ7_WR_MASK) | (((data) & MAC_TQPM1R_PSTQ7_MASK) << 24));\
		MAC_TQPM1R_REG_WR(v);\
} while (0)

#define MAC_RQC0R_REG_ADDR (BASE_ADDRESS + 0xa0)
#define MAC_RQC0R_REG_WR(data) (iowrite32(data, (void *)MAC_RQC0R_REG_ADDR))
#define MAC_RQC0R_REG_RD(data) ((data) = ioread32((void *)MAC_RQC0R_REG_ADDR))

#define MAC_RQC0R_MASK_16 (unsigned long)(0xffff)
#define MAC_RQC0R_RES_WR_MASK_16 (unsigned long)(0xffff)

#define MAC_RQC0R_RXQEN_MASK (unsigned long)(0x3)
#define MAC_RQC0R_RXQEN_WR_MASK(i)  (unsigned long)(~((~(~0 << (2))) << (0 + ((i) * 2))))
#define MAC_RQC0R_RXQEN_UDFWR(i, data) do {\
		unsigned long v;\
		MAC_RQC0R_REG_RD(v);\
		v = (v & (MAC_RQC0R_RES_WR_MASK_16)) | (((0) & (MAC_RQC0R_MASK_16)) << 16);\
		v = ((v & MAC_RQC0R_RXQEN_WR_MASK(i)) | (((data) & MAC_RQC0R_RXQEN_MASK) << (0 + (i) * 2)));\
		MAC_RQC0R_REG_WR(v);\
} while (0)

#define MAC_RQC2R_REG_ADDR (BASE_ADDRESS + 0xa8)
#define MAC_RQC2R_REG_WR(data) (iowrite32(data, (void *)MAC_RQC2R_REG_ADDR))
#define MAC_RQC2R_REG_RD(data) ((data) = ioread32((void *)MAC_RQC2R_REG_ADDR))

#define MAC_RQC2R_PSRQ0_MASK (unsigned long)(0xff)
#define MAC_RQC2R_PSRQ0_WR_MASK (unsigned long)(0xffffff00)
#define MAC_RQC2R_PSRQ0_UDFWR(data) do {\
		unsigned long v;\
		MAC_RQC2R_REG_RD(v);\
		v = ((v & MAC_RQC2R_PSRQ0_WR_MASK) | (((data) & MAC_RQC2R_PSRQ0_MASK) << 0));\
		MAC_RQC2R_REG_WR(v);\
} while (0)

#define MAC_RQC2R_PSRQ1_MASK (unsigned long)(0xff)
#define MAC_RQC2R_PSRQ1_WR_MASK (unsigned long)(0xffff00ff)
#define MAC_RQC2R_PSRQ1_UDFWR(data) do {\
		unsigned long v;\
		MAC_RQC2R_REG_RD(v);\
		v = ((v & MAC_RQC2R_PSRQ1_WR_MASK) | (((data) & MAC_RQC2R_PSRQ1_MASK) << 8));\
		MAC_RQC2R_REG_WR(v);\
} while (0)

#define MAC_RQC2R_PSRQ2_MASK (unsigned long)(0xff)
#define MAC_RQC2R_PSRQ2_WR_MASK (unsigned long)(0xff00ffff)
#define MAC_RQC2R_PSRQ2_UDFWR(data) do {\
		unsigned long v;\
		MAC_RQC2R_REG_RD(v);\
		v = ((v & MAC_RQC2R_PSRQ2_WR_MASK) | (((data) & MAC_RQC2R_PSRQ2_MASK) << 16));\
		MAC_RQC2R_REG_WR(v);\
} while (0)

#define MAC_RQC2R_PSRQ3_MASK (unsigned long)(0xff)
#define MAC_RQC2R_PSRQ3_WR_MASK (unsigned long)(0xffffff)
#define MAC_RQC2R_PSRQ3_UDFWR(data) do {\
		unsigned long v;\
		MAC_RQC2R_REG_RD(v);\
		v = ((v & MAC_RQC2R_PSRQ3_WR_MASK) | (((data) & MAC_RQC2R_PSRQ3_MASK) << 24));\
		MAC_RQC2R_REG_WR(v);\
} while (0)

#define MAC_RQC3R_REG_ADDR (BASE_ADDRESS + 0xac)
#define MAC_RQC3R_REG_WR(data) (iowrite32(data, (void *)MAC_RQC3R_REG_ADDR))
#define MAC_RQC3R_REG_RD(data) ((data) = ioread32((void *)MAC_RQC3R_REG_ADDR))

#define MAC_RQC3R_PSRQ4_MASK (unsigned long)(0xff)
#define MAC_RQC3R_PSRQ4_WR_MASK (unsigned long)(0xffffff00)
#define MAC_RQC3R_PSRQ4_UDFWR(data) do {\
		unsigned long v;\
		MAC_RQC3R_REG_RD(v);\
		v = ((v & MAC_RQC3R_PSRQ4_WR_MASK) | (((data) & MAC_RQC3R_PSRQ4_MASK) << 0));\
		MAC_RQC3R_REG_WR(v);\
} while (0)

#define MAC_RQC3R_PSRQ5_MASK (unsigned long)(0xff)
#define MAC_RQC3R_PSRQ5_WR_MASK (unsigned long)(0xffff00ff)
#define MAC_RQC3R_PSRQ5_UDFWR(data) do {\
		unsigned long v;\
		MAC_RQC3R_REG_RD(v);\
		v = ((v & MAC_RQC3R_PSRQ5_WR_MASK) | (((data) & MAC_RQC3R_PSRQ5_MASK) << 8));\
		MAC_RQC3R_REG_WR(v);\
} while (0)

#define MAC_RQC3R_PSRQ6_MASK (unsigned long)(0xff)
#define MAC_RQC3R_PSRQ6_WR_MASK (unsigned long)(0xff00ffff)
#define MAC_RQC3R_PSRQ6_UDFWR(data) do {\
		unsigned long v;\
		MAC_RQC3R_REG_RD(v);\
		v = ((v & MAC_RQC3R_PSRQ6_WR_MASK) | (((data) & MAC_RQC3R_PSRQ6_MASK) << 16));\
		MAC_RQC3R_REG_WR(v);\
} while (0)

#define MAC_RQC3R_PSRQ7_MASK (unsigned long)(0xff)
#define MAC_RQC3R_PSRQ7_WR_MASK (unsigned long)(0xffffff)
#define MAC_RQC3R_PSRQ7_UDFWR(data) do {\
		unsigned long v;\
		MAC_RQC3R_REG_RD(v);\
		v = ((v & MAC_RQC3R_PSRQ7_WR_MASK) | (((data) & MAC_RQC3R_PSRQ7_MASK) << 24));\
		MAC_RQC3R_REG_WR(v);\
} while (0)

#define MAC_ISR_REG_ADDR (BASE_ADDRESS + 0xb0)
#define MAC_ISR_REG_RD(data) ((data) = ioread32((void *)MAC_ISR_REG_ADDR))

#define MAC_ISR_RGSMIIS_LPOS 0
#define MAC_ISR_RGSMIIS_HPOS 0

#define MAC_IMR_REG_ADDR (BASE_ADDRESS + 0xb4)
#define MAC_IMR_REG_WR(data) (iowrite32(data, (void *)MAC_IMR_REG_ADDR))
#define MAC_IMR_REG_RD(data) ((data) = ioread32((void *)MAC_IMR_REG_ADDR))

#define MAC_PCS_REG_ADDR (BASE_ADDRESS + 0xf8)
#define MAC_PCS_REG_WR(data) (iowrite32(data, (void *)MAC_PCS_REG_ADDR))
#define MAC_PCS_REG_RD(data) ((data) = ioread32((void *)MAC_PCS_REG_ADDR))

#define MAC_HFR0_REG_ADDR (BASE_ADDRESS + 0x11c)
#define MAC_HFR0_REG_RD(data) ((data) = ioread32((void *)MAC_HFR0_REG_ADDR))

#define MAC_HFR0_MIISEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_GMIISEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_HDSEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_PCSSEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_VLANHASEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_SMASEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_RWKSEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_MGKSEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_MMCSEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_ARPOFFLDEN_MASK (unsigned long)(0x1)
#define MAC_HFR0_TSSSEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_EEESEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_TXCOESEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_RXCOE_MASK (unsigned long)(0x1)
#define MAC_HFR0_ADDMACADRSEL_MASK (unsigned long)(0x1f)
#define MAC_HFR0_MACADR32SEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_MACADR64SEL_MASK (unsigned long)(0x1)
#define MAC_HFR0_TSINTSEL_MASK (unsigned long)(0x3)
#define MAC_HFR0_SAVLANINS_MASK (unsigned long)(0x1)
#define MAC_HFR0_ACTPHYSEL_MASK (unsigned long)(0x7)

#define MAC_HFR1_REG_ADDR (BASE_ADDRESS + 0x120)
#define MAC_HFR1_REG_RD(data) ((data) = ioread32((void *)MAC_HFR1_REG_ADDR))

#define MAC_HFR1_RXFIFOSIZE_MASK (unsigned long)(0x1f)
#define MAC_HFR1_TXFIFOSIZE_MASK (unsigned long)(0x1f)
#define MAC_HFR1_ADVTHWORD_MASK (unsigned long)(0x1)
#define MAC_HFR1_DCBEN_MASK (unsigned long)(0x1)
#define MAC_HFR1_SPHEN_MASK (unsigned long)(0x1)
#define MAC_HFR1_TSOEN_MASK (unsigned long)(0x1)
#define MAC_HFR1_DMADEBUGEN_MASK (unsigned long)(0x1)
#define MAC_HFR1_AVSEL_MASK (unsigned long)(0x1)
#define MAC_HFR1_LPMODEEN_MASK (unsigned long)(0x1)
#define MAC_HFR1_HASHTBLSZ_MASK (unsigned long)(0x3)
#define MAC_HFR1_L3L4FILTERNUM_MASK (unsigned long)(0xf)

#define MAC_HFR2_REG_ADDR (BASE_ADDRESS + 0x124)
#define MAC_HFR2_REG_RD(data) ((data) = ioread32((void *)MAC_HFR2_REG_ADDR))

#define MAC_HFR2_RXQCNT_MASK (unsigned long)(0xf)
#define MAC_HFR2_TXQCNT_MASK (unsigned long)(0xf)
#define MAC_HFR2_RXCHCNT_MASK (unsigned long)(0xf)
#define MAC_HFR2_TXCHCNT_MASK (unsigned long)(0xf)
#define MAC_HFR2_PPSOUTNUM_MASK (unsigned long)(0x7)
#define MAC_HFR2_AUXSNAPNUM_MASK (unsigned long)(0x7)

#define MAC_HFR2_RXQCNT_LPOS 0
#define MAC_HFR2_RXQCNT_HPOS 3
#define MAC_HFR2_TXQCNT_LPOS 6
#define MAC_HFR2_TXQCNT_HPOS 9

#define MAC_GMIIAR_REG_ADDR (BASE_ADDRESS + 0x200)
#define MAC_GMIIAR_REG_WR(data) (iowrite32(data, (void *)MAC_GMIIAR_REG_ADDR))
#define MAC_GMIIAR_REG_RD(data) ((data) = ioread32((void *)MAC_GMIIAR_REG_ADDR))

#define MAC_GMIIAR_GB_LPOS 0
#define MAC_GMIIAR_GB_HPOS 0

#define MAC_GMIIDR_REG_ADDR (BASE_ADDRESS + 0x204)
#define MAC_GMIIDR_REG_WR(data) (iowrite32(data, (void *)MAC_GMIIDR_REG_ADDR))
#define MAC_GMIIDR_REG_RD(data) ((data) = ioread32((void *)MAC_GMIIDR_REG_ADDR))

#define MAC_GMIIDR_GD_LPOS 0
#define MAC_GMIIDR_GD_HPOS 15

#define MAC_GMIIDR_GD_MASK (unsigned long)(0xffff)
#define MAC_GMIIDR_GD_WR_MASK (unsigned long)(0xffff0000)
#define MAC_GMIIDR_GD_UDFWR(data) do {\
		unsigned long v;\
		MAC_GMIIDR_REG_RD(v);\
		v = ((v & MAC_GMIIDR_GD_WR_MASK) | (((data) & MAC_GMIIDR_GD_MASK) << 0));\
		MAC_GMIIDR_REG_WR(v);\
} while (0)

#define MAC_MA0HR_REG_ADDR (BASE_ADDRESS + 0x300)
#define MAC_MA0HR_REG_WR(data) (iowrite32(data, (void *)MAC_MA0HR_REG_ADDR))

#define MAC_MA0LR_REG_ADDR (BASE_ADDRESS + 0x304)
#define MAC_MA0LR_REG_WR(data) (iowrite32(data, (void *)MAC_MA0LR_REG_ADDR))

#define MAC_MA1_31HR_REG_ADDR (BASE_ADDRESS + 0x308)
#define MAC_MA1_31HR_REG_ADDRESS(i) (MAC_MA1_31HR_REG_ADDR + (((i) - 1) * 8))
#define MAC_MA1_31HR_REG_WR(i, data) (iowrite32(data, (void *)MAC_MA1_31HR_REG_ADDRESS(i)))
#define MAC_MA1_31HR_REG_RD(i, data) ((data) = ioread32((void *)MAC_MA1_31HR_REG_ADDRESS(i)))

#define MAC_MA1_31HR_MASK_19 (unsigned long)(0x1f)
#define MAC_MA1_31HR_RES_WR_MASK_19 (unsigned long)(0xff07ffff)

#define MAC_MA1_31HR_ADDRHI_MASK (unsigned long)(0xffff)
#define MAC_MA1_31HR_ADDRHI_WR_MASK (unsigned long)(0xffff0000)
#define MAC_MA1_31HR_ADDRHI_UDFWR(i, data) do {\
		unsigned long v;\
		MAC_MA1_31HR_REG_RD(i, v);\
		v = (v & (MAC_MA1_31HR_RES_WR_MASK_19)) | ((0 & (MAC_MA1_31HR_MASK_19)) << 19);\
		v = ((v & MAC_MA1_31HR_ADDRHI_WR_MASK) | (((data) & MAC_MA1_31HR_ADDRHI_MASK) << 0));\
		MAC_MA1_31HR_REG_WR(i, v);\
} while (0)

#define MAC_MA1_31HR_AE_MASK (unsigned long)(0x1)
#define MAC_MA1_31HR_AE_WR_MASK (unsigned long)(0x7fffffff)
#define MAC_MA1_31HR_AE_UDFWR(i, data) do {\
		unsigned long v;\
		MAC_MA1_31HR_REG_RD(i, v);\
		v = (v & (MAC_MA1_31HR_RES_WR_MASK_19)) | ((0 & (MAC_MA1_31HR_MASK_19)) << 19);\
		v = ((v & MAC_MA1_31HR_AE_WR_MASK) | (((data) & MAC_MA1_31HR_AE_MASK) << 31));\
		MAC_MA1_31HR_REG_WR(i, v);\
} while (0)

#define MAC_MA1_31LR_REG_ADDR (BASE_ADDRESS + 0x30c)
#define MAC_MA1_31LR_REG_ADDRESS(i) (MAC_MA1_31LR_REG_ADDR + (((i) - 1) * 8))
#define MAC_MA1_31LR_REG_WR(i, data) (iowrite32(data, (void *)MAC_MA1_31LR_REG_ADDRESS(i)))

#define MAC_MA32_127HR_REG_ADDR (BASE_ADDRESS + 0x400)
#define MAC_MA32_127HR_REG_ADDRESS(i) (MAC_MA32_127HR_REG_ADDR + (((i) - 32) * 8))
#define MAC_MA32_127HR_REG_WR(i, data) (iowrite32(data, (void *)MAC_MA32_127HR_REG_ADDRESS(i)))
#define MAC_MA32_127HR_REG_RD(i, data) ((data) = ioread32((void *)MAC_MA32_127HR_REG_ADDRESS(i)))

#define MAC_MA32_127HR_MASK_19 (unsigned long)(0xfff)
#define MAC_MA32_127HR_RES_WR_MASK_19 (unsigned long)(0x8007ffff)

#define MAC_MA32_127HR_ADDRHI_MASK (unsigned long)(0xffff)
#define MAC_MA32_127HR_ADDRHI_WR_MASK (unsigned long)(0xffff0000)
#define MAC_MA32_127HR_ADDRHI_UDFWR(i, data) do {\
		unsigned long v;\
		MAC_MA32_127HR_REG_RD(i, v);\
		v = (v & (MAC_MA32_127HR_RES_WR_MASK_19)) | ((0 & (MAC_MA32_127HR_MASK_19)) << 19);\
		v = ((v & MAC_MA32_127HR_ADDRHI_WR_MASK) | (((data) & MAC_MA32_127HR_ADDRHI_MASK) << 0));\
		MAC_MA32_127HR_REG_WR(i, v);\
} while (0)

#define MAC_MA32_127HR_AE_MASK (unsigned long)(0x1)
#define MAC_MA32_127HR_AE_WR_MASK (unsigned long)(0x7fffffff)
#define MAC_MA32_127HR_AE_UDFWR(i, data) do {\
		unsigned long v;\
		MAC_MA32_127HR_REG_RD(i, v);\
		v = (v & (MAC_MA32_127HR_RES_WR_MASK_19)) | ((0 & (MAC_MA32_127HR_MASK_19)) << 19);\
		v = ((v & MAC_MA32_127HR_AE_WR_MASK) | (((data) & MAC_MA32_127HR_AE_MASK) << 31));\
		MAC_MA32_127HR_REG_WR(i, v);\
} while (0)

#define MAC_MA32_127LR_REG_ADDR (BASE_ADDRESS + 0x404)
#define MAC_MA32_127LR_REG_ADDRESS(i) (MAC_MA32_127LR_REG_ADDR + (((i) - 32) * 8))
#define MAC_MA32_127LR_REG_WR(i, data) (iowrite32(data, (void *)MAC_MA32_127LR_REG_ADDRESS(i)))

#define MMC_CNTRL_REG_ADDR (BASE_ADDRESS + 0x700)
#define MMC_CNTRL_REG_WR(data) (iowrite32(data, (void *)MMC_CNTRL_REG_ADDR))
#define MMC_CNTRL_REG_RD(data) ((data) = ioread32((void *)MMC_CNTRL_REG_ADDR))

#define MMC_INTR_RX_REG_ADDR (BASE_ADDRESS + 0x704)
#define MMC_INTR_TX_REG_ADDR (BASE_ADDRESS + 0x708)

#define MMC_INTR_MASK_RX_REG_ADDR (BASE_ADDRESS + 0x70c)
#define MMC_INTR_MASK_RX_REG_WR(data) (iowrite32(data, (void *)MMC_INTR_MASK_RX_REG_ADDR))

#define MMC_INTR_MASK_TX_REG_ADDR (BASE_ADDRESS + 0x710)
#define MMC_INTR_MASK_TX_REG_WR(data) (iowrite32(data, (void *)MMC_INTR_MASK_TX_REG_ADDR))

#define MMC_TXOCTETCOUNT_GB_REG_ADDR (BASE_ADDRESS + 0x714)
#define MMC_TXPACKETCOUNT_GB_REG_ADDR (BASE_ADDRESS + 0x718)
#define MMC_TXBROADCASTPACKETS_G_REG_ADDR (BASE_ADDRESS + 0x71c)
#define MMC_TXMULTICASTPACKETS_G_REG_ADDR (BASE_ADDRESS + 0x720)
#define MMC_TX64OCTETS_GB_REG_ADDR (BASE_ADDRESS + 0x724)
#define MMC_TX65TO127OCTETS_GB_REG_ADDR (BASE_ADDRESS + 0x728)
#define MMC_TX128TO255OCTETS_GB_REG_ADDR (BASE_ADDRESS + 0x72c)
#define MMC_TX256TO511OCTETS_GB_REG_ADDR (BASE_ADDRESS + 0x730)
#define MMC_TX512TO1023OCTETS_GB_REG_ADDR (BASE_ADDRESS + 0x734)
#define MMC_TX1024TOMAXOCTETS_GB_REG_ADDR (BASE_ADDRESS + 0x738)
#define MMC_TXUNICASTPACKETS_GB_REG_ADDR (BASE_ADDRESS + 0x73c)
#define MMC_TXMULTICASTPACKETS_GB_REG_ADDR (BASE_ADDRESS + 0x740)
#define MMC_TXBROADCASTPACKETS_GB_REG_ADDR (BASE_ADDRESS + 0x744)
#define MMC_TXUNDERFLOWERROR_REG_ADDR (BASE_ADDRESS + 0x748)
#define MMC_TXSINGLECOL_G_REG_ADDR (BASE_ADDRESS + 0x74c)
#define MMC_TXMULTICOL_G_REG_ADDR (BASE_ADDRESS + 0x750)
#define MMC_TXDEFERRED_REG_ADDR (BASE_ADDRESS + 0x754)
#define MMC_TXLATECOL_REG_ADDR (BASE_ADDRESS + 0x758)
#define MMC_TXEXESSCOL_REG_ADDR (BASE_ADDRESS + 0x75c)
#define MMC_TXCARRIERERROR_REG_ADDR (BASE_ADDRESS + 0x760)
#define MMC_TXOCTETCOUNT_G_REG_ADDR (BASE_ADDRESS + 0x764)
#define MMC_TXPACKETSCOUNT_G_REG_ADDR (BASE_ADDRESS + 0x768)
#define MMC_TXEXCESSDEF_REG_ADDR (BASE_ADDRESS + 0x76c)
#define MMC_TXPAUSEPACKETS_REG_ADDR (BASE_ADDRESS + 0x770)
#define MMC_TXVLANPACKETS_G_REG_ADDR (BASE_ADDRESS + 0x774)
#define MMC_TXOVERSIZE_G_REG_ADDR (BASE_ADDRESS + 0x778)
#define MMC_RXPACKETCOUNT_GB_REG_ADDR (BASE_ADDRESS + 0x780)
#define MMC_RXOCTETCOUNT_GB_REG_ADDR (BASE_ADDRESS + 0x784)
#define MMC_RXOCTETCOUNT_G_REG_ADDR (BASE_ADDRESS + 0x788)
#define MMC_RXBROADCASTPACKETS_G_REG_ADDR (BASE_ADDRESS + 0x78c)
#define MMC_RXMULTICASTPACKETS_G_REG_ADDR (BASE_ADDRESS + 0x790)
#define MMC_RXCRCERROR_REG_ADDR (BASE_ADDRESS + 0x794)
#define MMC_RXALIGNMENTERROR_REG_ADDR (BASE_ADDRESS + 0x798)
#define MMC_RXRUNTERROR_REG_ADDR (BASE_ADDRESS + 0x79c)
#define MMC_RXJABBERERROR_REG_ADDR (BASE_ADDRESS + 0x7a0)
#define MMC_RXUNDERSIZE_G_REG_ADDR (BASE_ADDRESS + 0x7a4)
#define MMC_RXOVERSIZE_G_REG_ADDR (BASE_ADDRESS + 0x7a8)
#define MMC_RX64OCTETS_GB_REG_ADDR (BASE_ADDRESS + 0x7ac)
#define MMC_RX65TO127OCTETS_GB_REG_ADDR (BASE_ADDRESS + 0x7b0)
#define MMC_RX128TO255OCTETS_GB_REG_ADDR (BASE_ADDRESS + 0x7b4)
#define MMC_RX256TO511OCTETS_GB_REG_ADDR (BASE_ADDRESS + 0x7b8)
#define MMC_RX512TO1023OCTETS_GB_REG_ADDR (BASE_ADDRESS + 0x7bc)
#define MMC_RX1024TOMAXOCTETS_GB_REG_ADDR (BASE_ADDRESS + 0x7c0)
#define MMC_RXUNICASTPACKETS_G_REG_ADDR (BASE_ADDRESS + 0x7c4)
#define MMC_RXLENGTHERROR_REG_ADDR (BASE_ADDRESS + 0x7c8)
#define MMC_RXOUTOFRANGETYPE_REG_ADDR (BASE_ADDRESS + 0x7cc)
#define MMC_RXPAUSEPACKETS_REG_ADDR (BASE_ADDRESS + 0x7d0)
#define MMC_RXFIFOOVERFLOW_REG_ADDR (BASE_ADDRESS + 0x7d4)
#define MMC_RXVLANPACKETS_GB_REG_ADDR (BASE_ADDRESS + 0x7d8)
#define MMC_RXWATCHDOGERROR_REG_ADDR (BASE_ADDRESS + 0x7dc)
#define MMC_RXRCVERROR_REG_ADDR (BASE_ADDRESS + 0x7e0)
#define MMC_RXCTRLPACKETS_G_REG_ADDR (BASE_ADDRESS + 0x7e4)

#define MMC_IPC_INTR_MASK_RX_REG_ADDR (BASE_ADDRESS + 0x800)
#define MMC_IPC_INTR_MASK_RX_REG_WR(data) (iowrite32(data, (void *)MMC_IPC_INTR_MASK_RX_REG_ADDR))

#define MMC_IPC_INTR_RX_REG_ADDR (BASE_ADDRESS + 0x808)
#define MMC_RXIPV4_GD_PKTS_REG_ADDR (BASE_ADDRESS + 0x810)
#define MMC_RXIPV4_HDRERR_PKTS_REG_ADDR (BASE_ADDRESS + 0x814)
#define MMC_RXIPV4_NOPAY_PKTS_REG_ADDR (BASE_ADDRESS + 0x818)
#define MMC_RXIPV4_FRAG_PKTS_REG_ADDR (BASE_ADDRESS + 0x81c)
#define MMC_RXIPV4_UBSBL_PKTS_REG_ADDR (BASE_ADDRESS + 0x820)
#define MMC_RXIPV6_GD_PKTS_REG_ADDR (BASE_ADDRESS + 0x824)
#define MMC_RXIPV6_HDRERR_PKTS_REG_ADDR (BASE_ADDRESS + 0x828)
#define MMC_RXIPV6_NOPAY_PKTS_REG_ADDR (BASE_ADDRESS + 0x82c)
#define MMC_RXUDP_GD_PKTS_REG_ADDR (BASE_ADDRESS + 0x830)
#define MMC_RXUDP_ERR_PKTS_REG_ADDR (BASE_ADDRESS + 0x834)
#define MMC_RXTCP_GD_PKTS_REG_ADDR (BASE_ADDRESS + 0x838)
#define MMC_RXTCP_ERR_PKTS_REG_ADDR (BASE_ADDRESS + 0x83c)
#define MMC_RXICMP_GD_PKTS_REG_ADDR (BASE_ADDRESS + 0x840)
#define MMC_RXICMP_ERR_PKTS_REG_ADDR (BASE_ADDRESS + 0x844)
#define MMC_RXIPV4_GD_OCTETS_REG_ADDR (BASE_ADDRESS + 0x850)
#define MMC_RXIPV4_HDRERR_OCTETS_REG_ADDR (BASE_ADDRESS + 0x854)
#define MMC_RXIPV4_NOPAY_OCTETS_REG_ADDR (BASE_ADDRESS + 0x858)
#define MMC_RXIPV4_FRAG_OCTETS_REG_ADDR (BASE_ADDRESS + 0x85c)
#define MMC_RXIPV4_UDSBL_OCTETS_REG_ADDR (BASE_ADDRESS + 0x860)
#define MMC_RXIPV6_GD_OCTETS_REG_ADDR (BASE_ADDRESS + 0x864)
#define MMC_RXIPV6_HDRERR_OCTETS_REG_ADDR (BASE_ADDRESS + 0x868)
#define MMC_RXIPV6_NOPAY_OCTETS_REG_ADDR (BASE_ADDRESS + 0x86c)
#define MMC_RXUDP_GD_OCTETS_REG_ADDR (BASE_ADDRESS + 0x870)
#define MMC_RXUDP_ERR_OCTETS_REG_ADDR (BASE_ADDRESS + 0x874)
#define MMC_RXTCP_GD_OCTETS_REG_ADDR (BASE_ADDRESS + 0x878)
#define MMC_RXTCP_ERR_OCTETS_REG_ADDR (BASE_ADDRESS + 0x87c)
#define MMC_RXICMP_GD_OCTETS_REG_ADDR (BASE_ADDRESS + 0x880)
#define MMC_RXICMP_ERR_OCTETS_REG_ADDR (BASE_ADDRESS + 0x884)

#define MAC_TCR_REG_ADDR (BASE_ADDRESS + 0xb00)
#define MAC_TCR_REG_WR(data) (iowrite32(data, (void *)MAC_TCR_REG_ADDR))
#define MAC_TCR_REG_RD(data) ((data) = ioread32((void *)MAC_TCR_REG_ADDR))

#define MAC_TCR_TSCFUPDT_LPOS 1
#define MAC_TCR_TSCFUPDT_HPOS 1
#define MAC_TCR_TSINIT_LPOS 2
#define MAC_TCR_TSINIT_HPOS 2
#define MAC_TCR_TSUPDT_LPOS 3
#define MAC_TCR_TSUPDT_HPOS 3
#define MAC_TCR_TSADDREG_LPOS 5
#define MAC_TCR_TSADDREG_HPOS 5
#define MAC_TCR_TSCTRLSSR_LPOS 9
#define MAC_TCR_TSCTRLSSR_HPOS 9
#define MAC_TCR_TXTSSTSM_LPOS 24
#define MAC_TCR_TXTSSTSM_HPOS 24

#define MAC_TCR_MASK_29 (unsigned long)(0x7)
#define MAC_TCR_RES_WR_MASK_29 (unsigned long)(0x1fffffff)
#define MAC_TCR_MASK_25 (unsigned long)(0x7)
#define MAC_TCR_RES_WR_MASK_25 (unsigned long)(0xf1ffffff)
#define MAC_TCR_MASK_21 (unsigned long)(0x7)
#define MAC_TCR_RES_WR_MASK_21 (unsigned long)(0xff1fffff)
#define MAC_TCR_MASK_19 (unsigned long)(0x1)
#define MAC_TCR_RES_WR_MASK_19 (unsigned long)(0xfff7ffff)
#define MAC_TCR_MASK_6 (unsigned long)(0x3)
#define MAC_TCR_RES_WR_MASK_6 (unsigned long)(0xffffff3f)

#define MAC_TCR_TSINIT_MASK (unsigned long)(0x1)
#define MAC_TCR_TSINIT_WR_MASK (unsigned long)(0xfffffffb)
#define MAC_TCR_TSINIT_UDFWR(data) do {\
		unsigned long v;\
		MAC_TCR_REG_RD(v);\
		v = (v & (MAC_TCR_RES_WR_MASK_29)) | ((0 & (MAC_TCR_MASK_29)) << 29);\
		v = (v & (MAC_TCR_RES_WR_MASK_25)) | ((0 & (MAC_TCR_MASK_25)) << 25);\
		v = (v & (MAC_TCR_RES_WR_MASK_21)) | ((0 & (MAC_TCR_MASK_21)) << 21);\
		v = (v & (MAC_TCR_RES_WR_MASK_19)) | ((0 & (MAC_TCR_MASK_19)) << 19);\
		v = (v & (MAC_TCR_RES_WR_MASK_6)) | ((0 & (MAC_TCR_MASK_6)) << 6);\
		v = ((v & MAC_TCR_TSINIT_WR_MASK) | (((data) & MAC_TCR_TSINIT_MASK) << 2));\
		MAC_TCR_REG_WR(v);\
} while (0)

#define MAC_TCR_TSUPDT_MASK (unsigned long)(0x1)
#define MAC_TCR_TSUPDT_WR_MASK (unsigned long)(0xfffffff7)
#define MAC_TCR_TSUPDT_UDFWR(data) do {\
		unsigned long v;\
		MAC_TCR_REG_RD(v);\
		v = (v & (MAC_TCR_RES_WR_MASK_29)) | ((0 & (MAC_TCR_MASK_29)) << 29);\
		v = (v & (MAC_TCR_RES_WR_MASK_25)) | ((0 & (MAC_TCR_MASK_25)) << 25);\
		v = (v & (MAC_TCR_RES_WR_MASK_21)) | ((0 & (MAC_TCR_MASK_21)) << 21);\
		v = (v & (MAC_TCR_RES_WR_MASK_19)) | ((0 & (MAC_TCR_MASK_19)) << 19);\
		v = (v & (MAC_TCR_RES_WR_MASK_6)) | ((0 & (MAC_TCR_MASK_6)) << 6);\
		v = ((v & MAC_TCR_TSUPDT_WR_MASK) | (((data) & MAC_TCR_TSUPDT_MASK) << 3));\
		MAC_TCR_REG_WR(v);\
} while (0)

#define MAC_TCR_TSADDREG_MASK (unsigned long)(0x1)
#define MAC_TCR_TSADDREG_WR_MASK (unsigned long)(0xffffffdf)
#define MAC_TCR_TSADDREG_UDFWR(data) do {\
		unsigned long v;\
		MAC_TCR_REG_RD(v);\
		v = (v & (MAC_TCR_RES_WR_MASK_29)) | ((0 & (MAC_TCR_MASK_29)) << 29);\
		v = (v & (MAC_TCR_RES_WR_MASK_25)) | ((0 & (MAC_TCR_MASK_25)) << 25);\
		v = (v & (MAC_TCR_RES_WR_MASK_21)) | ((0 & (MAC_TCR_MASK_21)) << 21);\
		v = (v & (MAC_TCR_RES_WR_MASK_19)) | ((0 & (MAC_TCR_MASK_19)) << 19);\
		v = (v & (MAC_TCR_RES_WR_MASK_6)) | ((0 & (MAC_TCR_MASK_6)) << 6);\
		v = ((v & MAC_TCR_TSADDREG_WR_MASK) | (((data) & MAC_TCR_TSADDREG_MASK) << 5));\
		MAC_TCR_REG_WR(v);\
} while (0)

#define MAC_SSIR_REG_ADDR (BASE_ADDRESS + 0xb04)
#define MAC_SSIR_REG_WR(data) (iowrite32(data, (void *)MAC_SSIR_REG_ADDR))
#define MAC_SSIR_REG_RD(data) ((data) = ioread32((void *)MAC_SSIR_REG_ADDR))

#define MAC_SSIR_MASK_24 (unsigned int)(0xff)
#define MAC_SSIR_RES_WR_MASK_24 (unsigned int)(0xffffff)
#define MAC_SSIR_MASK_0 (unsigned int)(0xff)
#define MAC_SSIR_RES_WR_MASK_0 (unsigned int)(0xffffff00)

#define MAC_SSIR_SSINC_MASK (unsigned int)(0xff)
#define MAC_SSIR_SSINC_WR_MASK (unsigned int)(0xff00ffff)
#define MAC_SSIR_SSINC_UDFWR(data) do {\
		unsigned int v;\
		MAC_SSIR_REG_RD(v);\
		v = (v & (MAC_SSIR_RES_WR_MASK_24)) | ((0 & (MAC_SSIR_MASK_24)) << 24);\
		v = (v & (MAC_SSIR_RES_WR_MASK_0)) | ((0 & (MAC_SSIR_MASK_0)) << 0);\
		v = ((v & MAC_SSIR_SSINC_WR_MASK) | (((data) & MAC_SSIR_SSINC_MASK) << 16));\
		MAC_SSIR_REG_WR(v);\
} while (0)

#define MAC_STSR_REG_ADDR (BASE_ADDRESS + 0xb08)
#define MAC_STSR_REG_RD(data) ((data) = ioread32((void *)MAC_STSR_REG_ADDR))

#define MAC_STNSR_REG_ADDR (BASE_ADDRESS + 0xb0c)
#define MAC_STNSR_REG_RD(data) ((data) = ioread32((void *)MAC_STNSR_REG_ADDR))

#define MAC_STNSR_TSSS_LPOS 0
#define MAC_STNSR_TSSS_HPOS 30

#define MAC_STSUR_REG_ADDR (BASE_ADDRESS + 0xb10)
#define MAC_STSUR_REG_WR(data) (iowrite32(data, (void *)MAC_STSUR_REG_ADDR))

#define MAC_STNSUR_REG_ADDR (BASE_ADDRESS + 0xb14)
#define MAC_STNSUR_REG_WR(data) (iowrite32(data, (void *)MAC_STNSUR_REG_ADDR))
#define MAC_STNSUR_REG_RD(data) ((data) = ioread32((void *)MAC_STNSUR_REG_ADDR))

#define MAC_STNSUR_TSSS_MASK (unsigned long)(0x7fffffff)
#define MAC_STNSUR_TSSS_WR_MASK (unsigned long)(0x80000000)
#define MAC_STNSUR_TSSS_UDFWR(data) do {\
		unsigned long v;\
		MAC_STNSUR_REG_RD(v);\
		v = ((v & MAC_STNSUR_TSSS_WR_MASK) | (((data) & MAC_STNSUR_TSSS_MASK) << 0));\
		MAC_STNSUR_REG_WR(v);\
} while (0)

#define MAC_STNSUR_ADDSUB_MASK (unsigned long)(0x1)
#define MAC_STNSUR_ADDSUB_WR_MASK (unsigned long)(0x7fffffff)
#define MAC_STNSUR_ADDSUB_UDFWR(data) do {\
		unsigned long v;\
		MAC_STNSUR_REG_RD(v);\
		v = ((v & MAC_STNSUR_ADDSUB_WR_MASK) | (((data) & MAC_STNSUR_ADDSUB_MASK) << 31));\
		MAC_STNSUR_REG_WR(v);\
} while (0)

#define MAC_TAR_REG_ADDR (BASE_ADDRESS + 0xb18)
#define MAC_TAR_REG_WR(data) (iowrite32(data, (void *)MAC_TAR_REG_ADDR))

#define MAC_TTSN_REG_ADDR (BASE_ADDRESS + 0xb30)
#define MAC_TTSN_REG_RD(data) ((data) = ioread32((void *)MAC_TTSN_REG_ADDR))

#define MAC_TTSN_TXTSSTSMIS_LPOS 31
#define MAC_TTSN_TXTSSTSMIS_HPOS 31

#define MAC_TTSN_TXTSSTSLO_MASK (unsigned long)(0x7fffffff)
#define MAC_TTSN_TXTSSTSLO_UDFRD(data) do {\
		MAC_TTSN_REG_RD(data);\
		(data) = (((data) >> 0) & MAC_TTSN_TXTSSTSLO_MASK);\
} while (0)

#define MAC_TTN_REG_ADDR (BASE_ADDRESS + 0xb34)
#define MAC_TTN_REG_RD(data) ((data) = ioread32((void *)MAC_TTN_REG_ADDR))
#define MAC_TTN_TXTSSTSHI_UDFRD(data) (MAC_TTN_REG_RD(data))

#define MTL_OMR_REG_ADDR (BASE_ADDRESS + 0xc00)
#define MTL_OMR_REG_WR(data) (iowrite32(data, (void *)MTL_OMR_REG_ADDR))
#define MTL_OMR_REG_RD(data) ((data) = ioread32((void *)MTL_OMR_REG_ADDR))

#define MTL_OMR_DTXSTS_LPOS 1
#define MTL_OMR_DTXSTS_HPOS 1

#define MTL_RQDCM0R_REG_ADDR (BASE_ADDRESS + 0xc30)
#define MTL_RQDCM0R_REG_WR(data) (iowrite32(data, (void *)MTL_RQDCM0R_REG_ADDR))

#define MTL_RQDCM1R_REG_ADDR (BASE_ADDRESS + 0xc34)
#define MTL_RQDCM1R_REG_WR(data) (iowrite32(data, (void *)MTL_RQDCM1R_REG_ADDR))

#define MTL_QTOMR_REG_ADDR (BASE_ADDRESS + 0xd00)
#define MTL_QTOMR_REG_ADDRESS(i) (MTL_QTOMR_REG_ADDR + ((i) * 0x40))
#define MTL_QTOMR_REG_WR(i, data) (iowrite32(data, (void *)MTL_QTOMR_REG_ADDRESS(i)))
#define MTL_QTOMR_REG_RD(i, data) ((data) = ioread32((void *)MTL_QTOMR_REG_ADDRESS(i)))

#define MTL_QTOMR_FTQ_LPOS 0
#define MTL_QTOMR_FTQ_HPOS 0

#define MTL_QTOMR_MASK_26 (unsigned long)(0x3f)
#define MTL_QTOMR_RES_WR_MASK_26 (unsigned long)(0x3ffffff)
#define MTL_QTOMR_MASK_7 (unsigned long)(0x1ff)
#define MTL_QTOMR_RES_WR_MASK_7 (unsigned long)(0xffff007f)

#define MTL_QTOMR_TQS_MASK (unsigned long)(0x3ff)
#define MTL_QTOMR_TQS_WR_MASK (unsigned long)(0xfc00ffff)
#define MTL_QTOMR_TQS_UDFWR(i, data) do {\
		unsigned long v;\
		MTL_QTOMR_REG_RD(i, v);\
		v = (v & (MTL_QTOMR_RES_WR_MASK_26)) | ((0 & (MTL_QTOMR_MASK_26)) << 26);\
		v = (v & (MTL_QTOMR_RES_WR_MASK_7)) | ((0 & (MTL_QTOMR_MASK_7)) << 7);\
		v = ((v & MTL_QTOMR_TQS_WR_MASK) | (((data) & MTL_QTOMR_TQS_MASK) << 16));\
		MTL_QTOMR_REG_WR(i, v);\
} while (0)

#define MTL_QTOMR_TXQEN_MASK (unsigned long)(0x3)
#define MTL_QTOMR_TXQEN_WR_MASK (unsigned long)(0xfffffff3)
#define MTL_QTOMR_TXQEN_UDFWR(i, data) do {\
		unsigned long v;\
		MTL_QTOMR_REG_RD(i, v);\
		v = (v & (MTL_QTOMR_RES_WR_MASK_26)) | ((0 & (MTL_QTOMR_MASK_26)) << 26);\
		v = (v & (MTL_QTOMR_RES_WR_MASK_7)) | ((0 & (MTL_QTOMR_MASK_7)) << 7);\
		v = ((v & MTL_QTOMR_TXQEN_WR_MASK) | (((data) & MTL_QTOMR_TXQEN_MASK) << 2));\
		MTL_QTOMR_REG_WR(i, v);\
} while (0)

#define MTL_QTOMR_TSF_MASK (unsigned long)(0x1)
#define MTL_QTOMR_TSF_WR_MASK (unsigned long)(0xfffffffd)
#define MTL_QTOMR_TSF_UDFWR(i, data) do {\
		unsigned long v;\
		MTL_QTOMR_REG_RD(i, v);\
		v = (v & (MTL_QTOMR_RES_WR_MASK_26)) | ((0 & (MTL_QTOMR_MASK_26)) << 26);\
		v = (v & (MTL_QTOMR_RES_WR_MASK_7)) | ((0 & (MTL_QTOMR_MASK_7)) << 7);\
		v = ((v & MTL_QTOMR_TSF_WR_MASK) | (((data) & MTL_QTOMR_TSF_MASK) << 1));\
		MTL_QTOMR_REG_WR(i, v);\
} while (0)

#define MTL_QTOMR_FTQ_MASK (unsigned long)(0x1)
#define MTL_QTOMR_FTQ_WR_MASK (unsigned long)(0xfffffffe)
#define MTL_QTOMR_FTQ_UDFWR(i, data) do {\
		unsigned long v;\
		MTL_QTOMR_REG_RD(i, v);\
		v = (v & (MTL_QTOMR_RES_WR_MASK_26)) | ((0 & (MTL_QTOMR_MASK_26)) << 26);\
		v = (v & (MTL_QTOMR_RES_WR_MASK_7)) | ((0 & (MTL_QTOMR_MASK_7)) << 7);\
		v = ((v & MTL_QTOMR_FTQ_WR_MASK) | (((data) & MTL_QTOMR_FTQ_MASK) << 0));\
		MTL_QTOMR_REG_WR(i, v);\
} while (0)

#define MTL_QW_REG_ADDR (BASE_ADDRESS + 0xd18)
#define MTL_QW_REG_ADDRESS(i) (MTL_QW_REG_ADDR + ((i) * 0x40))
#define MTL_QW_REG_WR(i, data) (iowrite32(data, (void *)MTL_QW_REG_ADDRESS(i)))

#define MTL_QW_MASK_21 (unsigned long)(0x7ff)
#define MTL_QW_RES_WR_MASK_21 (unsigned long)(0x1fffff)

#define MTL_QW_ISCQW_MASK (unsigned long)(0x1fffff)
#define MTL_QW_ISCQW_WR_MASK (unsigned long)(0xffe00000)

#define MTL_QW_ISCQW_UDFWR(i, data) do {\
		unsigned long v = 0; \
		v = (v & (MTL_QW_RES_WR_MASK_21)) | ((0 & (MTL_QW_MASK_21)) << 21);\
		(v) = ((v & MTL_QW_ISCQW_WR_MASK) | (((data) & MTL_QW_ISCQW_MASK) << 0));\
		MTL_QW_REG_WR(i, v);\
} while (0)

#define MTL_QROMR_REG_ADDR (BASE_ADDRESS + 0xd30)
#define MTL_QROMR_REG_ADDRESS(i) (MTL_QROMR_REG_ADDR + ((i) * 64))
#define MTL_QROMR_REG_WR(i, data) (iowrite32(data, (void *)MTL_QROMR_REG_ADDRESS(i)))
#define MTL_QROMR_REG_RD(i, data) ((data) = ioread32((void *)MTL_QROMR_REG_ADDRESS(i)))

#define MTL_QROMR_MASK_30 (unsigned long)(0x3)
#define MTL_QROMR_RES_WR_MASK_30 (unsigned long)(0x3fffffff)
#define MTL_QROMR_MASK_11 (unsigned long)(0x3)
#define MTL_QROMR_RES_WR_MASK_11 (unsigned long)(0xffffe7ff)
#define MTL_QROMR_MASK_2 (unsigned long)(0x1)
#define MTL_QROMR_RES_WR_MASK_2 (unsigned long)(0xfffffffb)

#define MTL_QROMR_FEP_MASK (unsigned long)(0x1)
#define MTL_QROMR_FEP_WR_MASK (unsigned long)(0xffffffef)
#define MTL_QROMR_FEP_UDFWR(i, data) do {\
		unsigned long v;\
		MTL_QROMR_REG_RD(i, v);\
		v = (v & (MTL_QROMR_RES_WR_MASK_30)) | ((0 & (MTL_QROMR_MASK_30)) << 30);\
		v = (v & (MTL_QROMR_RES_WR_MASK_11)) | ((0 & (MTL_QROMR_MASK_11)) << 11);\
		v = (v & (MTL_QROMR_RES_WR_MASK_2)) | ((0 & (MTL_QROMR_MASK_2)) << 2);\
		v = ((v & MTL_QROMR_FEP_WR_MASK) | (((data) & MTL_QROMR_FEP_MASK) << 4));\
		MTL_QROMR_REG_WR(i, v);\
} while (0)

#define MTL_QROMR_EHFC_MASK (unsigned long)(0x1)
#define MTL_QROMR_EHFC_WR_MASK (unsigned long)(0xffffff7f)
#define MTL_QROMR_EHFC_UDFWR(i, data) do {\
		unsigned long v;\
		MTL_QROMR_REG_RD(i, v);\
		v = (v & (MTL_QROMR_RES_WR_MASK_30)) | ((0 & (MTL_QROMR_MASK_30)) << 30);\
		v = (v & (MTL_QROMR_RES_WR_MASK_11)) | ((0 & (MTL_QROMR_MASK_11)) << 11);\
		v = (v & (MTL_QROMR_RES_WR_MASK_2)) | ((0 & (MTL_QROMR_MASK_2)) << 2);\
		v = ((v & MTL_QROMR_EHFC_WR_MASK) | (((data) & MTL_QROMR_EHFC_MASK) << 7));\
		MTL_QROMR_REG_WR(i, v);\
} while (0)

#define MTL_QROMR_RFA_MASK (unsigned long)(0x3f)
#define MTL_QROMR_RFA_WR_MASK (unsigned long)(0xffffc0ff)
#define MTL_QROMR_RFA_UDFWR(i, data) do {\
			unsigned long v;\
			MTL_QROMR_REG_RD(i, v);\
			v = (v & (MTL_QROMR_RES_WR_MASK_30)) | ((0 & (MTL_QROMR_MASK_30)) << 30);\
			v = (v & (MTL_QROMR_RES_WR_MASK_11)) | ((0 & (MTL_QROMR_MASK_11)) << 11);\
			v = (v & (MTL_QROMR_RES_WR_MASK_2)) | ((0 & (MTL_QROMR_MASK_2)) << 2);\
			v = ((v & MTL_QROMR_RFA_WR_MASK) | (((data) & MTL_QROMR_RFA_MASK) << 8));\
			MTL_QROMR_REG_WR(i, v);\
	} while (0)

#define MTL_QROMR_RFD_MASK (unsigned long)(0x3f)
#define MTL_QROMR_RFD_WR_MASK (unsigned long)(0xfff03fff)
#define MTL_QROMR_RFD_UDFWR(i, data) do {\
		unsigned long v;\
		MTL_QROMR_REG_RD(i, v);\
		v = (v & (MTL_QROMR_RES_WR_MASK_30)) | ((0 & (MTL_QROMR_MASK_30)) << 30);\
		v = (v & (MTL_QROMR_RES_WR_MASK_11)) | ((0 & (MTL_QROMR_MASK_11)) << 11);\
		v = (v & (MTL_QROMR_RES_WR_MASK_2)) | ((0 & (MTL_QROMR_MASK_2)) << 2);\
		v = ((v & MTL_QROMR_RFD_WR_MASK) | (((data) & MTL_QROMR_RFD_MASK) << 14));\
		MTL_QROMR_REG_WR(i, v);\
} while (0)

#define MTL_QROMR_RQS_MASK (unsigned long)(0x3ff)
#define MTL_QROMR_RQS_WR_MASK (unsigned long)(0xc00fffff)
#define MTL_QROMR_RQS_UDFWR(i, data) do {\
		unsigned long v;\
		MTL_QROMR_REG_RD(i, v);\
		v = (v & (MTL_QROMR_RES_WR_MASK_30)) | ((0 & (MTL_QROMR_MASK_30)) << 30);\
		v = (v & (MTL_QROMR_RES_WR_MASK_11)) | ((0 & (MTL_QROMR_MASK_11)) << 11);\
		v = (v & (MTL_QROMR_RES_WR_MASK_2)) | ((0 & (MTL_QROMR_MASK_2)) << 2);\
		v = ((v & MTL_QROMR_RQS_WR_MASK) | (((data) & MTL_QROMR_RQS_MASK) << 20));\
		MTL_QROMR_REG_WR(i, v);\
} while (0)

#define DMA_BMR_REG_ADDR (BASE_ADDRESS + 0x1000)
#define DMA_BMR_REG_WR(data) (iowrite32(data, (void *)DMA_BMR_REG_ADDR))
#define DMA_BMR_REG_RD(data) ((data) = ioread32((void *)DMA_BMR_REG_ADDR))

#define DMA_BMR_SWR_LPOS 0
#define DMA_BMR_SWR_HPOS 0

#define DMA_BMR_MASK_15 (unsigned long)(0xffff)
#define DMA_BMR_RES_WR_MASK_15 (unsigned long)(0x80007fff)
#define DMA_BMR_MASK_6 (unsigned long)(0x1f)
#define DMA_BMR_RES_WR_MASK_6 (unsigned long)(0xfffff83f)

#define DMA_BMR_SWR_MASK (unsigned long)(0x1)
#define DMA_BMR_SWR_WR_MASK (unsigned long)(0xfffffffe)
#define DMA_BMR_SWR_UDFWR(data) do {\
		unsigned long v;\
		DMA_BMR_REG_RD(v);\
		v = (v & (DMA_BMR_RES_WR_MASK_15)) | ((0 & (DMA_BMR_MASK_15)) << 15);\
		v = (v & (DMA_BMR_RES_WR_MASK_6)) | ((0 & (DMA_BMR_MASK_6)) << 6);\
		v = ((v & DMA_BMR_SWR_WR_MASK) | (((data) & DMA_BMR_SWR_MASK) << 0));\
		DMA_BMR_REG_WR(v);\
} while (0)

#define DMA_SBUS_REG_ADDR (BASE_ADDRESS + 0x1004)
#define DMA_SBUS_REG_WR(data) (iowrite32(data, (void *)DMA_SBUS_REG_ADDR))
#define DMA_SBUS_REG_RD(data) ((data) = ioread32((void *)DMA_SBUS_REG_ADDR))

#define DMA_SBUS_MASK_25 (unsigned long)(0x1f)
#define DMA_SBUS_RES_WR_MASK_25 (unsigned long)(0xc1ffffff)
#define DMA_SBUS_MASK_20 (unsigned long)(0x1)
#define DMA_SBUS_RES_WR_MASK_20 (unsigned long)(0xffefffff)
#define DMA_SBUS_MASK_15 (unsigned long)(0x1)
#define DMA_SBUS_RES_WR_MASK_15 (unsigned long)(0xffff7fff)
#define DMA_SBUS_MASK_8 (unsigned long)(0xf)
#define DMA_SBUS_RES_WR_MASK_8 (unsigned long)(0xfffff0ff)

#define DMA_SBUS_BLEN4_MASK (unsigned long)(0x1)
#define DMA_SBUS_BLEN4_WR_MASK (unsigned long)(0xfffffffd)
#define DMA_SBUS_BLEN4_UDFWR(data) do {\
		unsigned long v;\
		DMA_SBUS_REG_RD(v);\
		v = (v & (DMA_SBUS_RES_WR_MASK_25)) | ((0 & (DMA_SBUS_MASK_25)) << 25);\
		v = (v & (DMA_SBUS_RES_WR_MASK_20)) | ((0 & (DMA_SBUS_MASK_20)) << 20);\
		v = (v & (DMA_SBUS_RES_WR_MASK_15)) | ((0 & (DMA_SBUS_MASK_15)) << 15);\
		v = (v & (DMA_SBUS_RES_WR_MASK_8)) | ((0 & (DMA_SBUS_MASK_8)) << 8);\
		v = ((v & DMA_SBUS_BLEN4_WR_MASK) | (((data) & DMA_SBUS_BLEN4_MASK) << 1));\
		DMA_SBUS_REG_WR(v);\
} while (0)

#define DMA_SBUS_BLEN8_MASK (unsigned long)(0x1)
#define DMA_SBUS_BLEN8_WR_MASK (unsigned long)(0xfffffffb)
#define DMA_SBUS_BLEN8_UDFWR(data) do {\
		unsigned long v;\
		DMA_SBUS_REG_RD(v);\
		v = (v & (DMA_SBUS_RES_WR_MASK_25)) | ((0 & (DMA_SBUS_MASK_25)) << 25);\
		v = (v & (DMA_SBUS_RES_WR_MASK_20)) | ((0 & (DMA_SBUS_MASK_20)) << 20);\
		v = (v & (DMA_SBUS_RES_WR_MASK_15)) | ((0 & (DMA_SBUS_MASK_15)) << 15);\
		v = (v & (DMA_SBUS_RES_WR_MASK_8)) | ((0 & (DMA_SBUS_MASK_8)) << 8);\
		v = ((v & DMA_SBUS_BLEN8_WR_MASK) | (((data) & DMA_SBUS_BLEN8_MASK) << 2));\
		DMA_SBUS_REG_WR(v);\
} while (0)

#define DMA_SBUS_BLEN16_MASK (unsigned long)(0x1)
#define DMA_SBUS_BLEN16_WR_MASK (unsigned long)(0xfffffff7)
#define DMA_SBUS_BLEN16_UDFWR(data) do {\
		unsigned long v;\
		DMA_SBUS_REG_RD(v);\
		v = (v & (DMA_SBUS_RES_WR_MASK_25)) | ((0 & (DMA_SBUS_MASK_25)) << 25);\
		v = (v & (DMA_SBUS_RES_WR_MASK_20)) | ((0 & (DMA_SBUS_MASK_20)) << 20);\
		v = (v & (DMA_SBUS_RES_WR_MASK_15)) | ((0 & (DMA_SBUS_MASK_15)) << 15);\
		v = (v & (DMA_SBUS_RES_WR_MASK_8)) | ((0 & (DMA_SBUS_MASK_8)) << 8);\
		v = ((v & DMA_SBUS_BLEN16_WR_MASK) | (((data) & DMA_SBUS_BLEN16_MASK) << 3));\
		DMA_SBUS_REG_WR(v);\
} while (0)

#define DMA_SBUS_RD_OSR_LMT_MASK (unsigned long)(0xf)
#define DMA_SBUS_RD_OSR_LMT_WR_MASK (unsigned long)(0xfff0ffff)
#define DMA_SBUS_RD_OSR_LMT_UDFWR(data) do {\
		unsigned long v;\
		DMA_SBUS_REG_RD(v);\
		v = (v & (DMA_SBUS_RES_WR_MASK_25)) | ((0 & (DMA_SBUS_MASK_25)) << 25);\
		v = (v & (DMA_SBUS_RES_WR_MASK_20)) | ((0 & (DMA_SBUS_MASK_20)) << 20);\
		v = (v & (DMA_SBUS_RES_WR_MASK_15)) | ((0 & (DMA_SBUS_MASK_15)) << 15);\
		v = (v & (DMA_SBUS_RES_WR_MASK_8)) | ((0 & (DMA_SBUS_MASK_8)) << 8);\
		v = ((v & DMA_SBUS_RD_OSR_LMT_WR_MASK) | (((data) & DMA_SBUS_RD_OSR_LMT_MASK) << 16));\
		DMA_SBUS_REG_WR(v);\
} while (0)

#define DMA_ISR_REG_ADDR (BASE_ADDRESS + 0x1008)
#define DMA_ISR_REG_RD(data) ((data) = ioread32((void *)DMA_ISR_REG_ADDR))

#define DMA_ISR_MACIS_LPOS 17
#define DMA_ISR_MACIS_HPOS 17

#define DMA_DSR0_REG_ADDR (BASE_ADDRESS + 0x100c)
#define DMA_DSR0_REG_RD(data) ((data) = ioread32((void *)DMA_DSR0_REG_ADDR))

#define DMA_DSR0_RPS0_LPOS 8
#define DMA_DSR0_RPS0_HPOS 11
#define DMA_DSR0_TPS0_LPOS 12
#define DMA_DSR0_TPS0_HPOS 15
#define DMA_DSR0_RPS1_LPOS 16
#define DMA_DSR0_RPS1_HPOS 19
#define DMA_DSR0_TPS1_LPOS 20
#define DMA_DSR0_TPS1_HPOS 23
#define DMA_DSR0_RPS2_LPOS 24
#define DMA_DSR0_RPS2_HPOS 27
#define DMA_DSR0_TPS2_LPOS 28
#define DMA_DSR0_TPS2_HPOS 31

#define DMA_DSR0_TPS0_MASK (unsigned long)(0xf)

#define DMA_DSR0_TPS0_UDFRD(data) do {\
		DMA_DSR0_REG_RD(data);\
		data = ((data >> 12) & DMA_DSR0_TPS0_MASK);\
} while (0)

#define DMA_DSR1_REG_ADDR (BASE_ADDRESS + 0x1010)
#define DMA_DSR1_REG_RD(data) ((data) = ioread32((void *)DMA_DSR1_REG_ADDR))

#define DMA_DSR1_RPS3_LPOS 0
#define DMA_DSR1_RPS3_HPOS 3
#define DMA_DSR1_TPS3_LPOS 4
#define DMA_DSR1_TPS3_HPOS 7
#define DMA_DSR1_RPS4_LPOS 8
#define DMA_DSR1_RPS4_HPOS 11
#define DMA_DSR1_TPS4_LPOS 12
#define DMA_DSR1_TPS4_HPOS 15
#define DMA_DSR1_RPS5_LPOS 16
#define DMA_DSR1_RPS5_HPOS 19
#define DMA_DSR1_TPS5_LPOS 20
#define DMA_DSR1_TPS5_HPOS 23
#define DMA_DSR1_RPS6_LPOS 24
#define DMA_DSR1_RPS6_HPOS 27
#define DMA_DSR1_TPS6_LPOS 28
#define DMA_DSR1_TPS6_HPOS 31

#define DMA_DSR2_REG_ADDR (BASE_ADDRESS + 0x1014)
#define DMA_DSR2_REG_RD(data) ((data) = ioread32((void *)DMA_DSR2_REG_ADDR))

#define DMA_DSR2_RPS7_LPOS 0
#define DMA_DSR2_RPS7_HPOS 3
#define DMA_DSR2_TPS7_LPOS 4
#define DMA_DSR2_TPS7_HPOS 7

#define DMA_CR_REG_ADDR (BASE_ADDRESS + 0x1100)
#define DMA_CR_REG_ADDRESS(i) (DMA_CR_REG_ADDR + ((i) * 0x80))
#define DMA_CR_REG_WR(i, data) (iowrite32(data, (void *)DMA_CR_REG_ADDRESS(i)))
#define DMA_CR_REG_RD(i, data) ((data) = ioread32((void *)DMA_CR_REG_ADDRESS(i)))

#define DMA_CR_MASK_25 (unsigned long)(0x7f)
#define DMA_CR_RES_WR_MASK_25 (unsigned long)(0x1ffffff)
#define DMA_CR_MASK_21 (unsigned long)(0x3)
#define DMA_CR_RES_WR_MASK_21 (unsigned long)(0xff9fffff)

#define DMA_CR_PBLX8_MASK (unsigned long)(0x1)
#define DMA_CR_PBLX8_WR_MASK (unsigned long)(0xfffeffff)
#define DMA_CR_PBLX8_UDFWR(i, data) do {\
		unsigned long v;\
		DMA_CR_REG_RD(i, v);\
		v = (v & (DMA_CR_RES_WR_MASK_25)) | ((0 & (DMA_CR_MASK_25)) << 25);\
		v = (v & (DMA_CR_RES_WR_MASK_21)) | ((0 & (DMA_CR_MASK_21)) << 21);\
		v = ((v & DMA_CR_PBLX8_WR_MASK) | (((data) & DMA_CR_PBLX8_MASK) << 16));\
		DMA_CR_REG_WR(i, v);\
} while (0)

#define DMA_TCR_REG_ADDR (BASE_ADDRESS + 0x1104)
#define DMA_TCR_REG_ADDRESS(i) (DMA_TCR_REG_ADDR + ((i) * 128))
#define DMA_TCR_REG_WR(i, data) (iowrite32(data, (void *)DMA_TCR_REG_ADDRESS(i)))
#define DMA_TCR_REG_RD(i, data) ((data) = ioread32((void *)DMA_TCR_REG_ADDRESS(i)))

#define DMA_TCR_MASK_22 (unsigned long)(0x3ff)
#define DMA_TCR_RES_WR_MASK_22 (unsigned long)(0x3fffff)
#define DMA_TCR_MASK_13 (unsigned long)(0x7)
#define DMA_TCR_RES_WR_MASK_13 (unsigned long)(0xffff1fff)
#define DMA_TCR_MASK_5 (unsigned long)(0x7f)
#define DMA_TCR_RES_WR_MASK_5 (unsigned long)(0xfffff01f)

#define DMA_TCR_ST_MASK (unsigned long)(0x1)
#define DMA_TCR_ST_WR_MASK (unsigned long)(0xfffffffe)
#define DMA_TCR_ST_UDFWR(i, data) do {\
		unsigned long v;\
		DMA_TCR_REG_RD(i, v);\
		v = (v & (DMA_TCR_RES_WR_MASK_22)) | ((0 & (DMA_TCR_MASK_22)) << 22);\
		v = (v & (DMA_TCR_RES_WR_MASK_13)) | ((0 & (DMA_TCR_MASK_13)) << 13);\
		v = (v & (DMA_TCR_RES_WR_MASK_5)) | ((0 & (DMA_TCR_MASK_5)) << 5);\
		v = ((v & DMA_TCR_ST_WR_MASK) | (((data) & DMA_TCR_ST_MASK) << 0));\
		DMA_TCR_REG_WR(i, v);\
} while (0)

#define DMA_TCR_OSP_MASK (unsigned long)(0x1)
#define DMA_TCR_OSP_WR_MASK (unsigned long)(0xffffffef)
#define DMA_TCR_OSP_UDFWR(i, data) do {\
		unsigned long v;\
		DMA_TCR_REG_RD(i, v);\
		v = (v & (DMA_TCR_RES_WR_MASK_22)) | ((0 & (DMA_TCR_MASK_22)) << 22);\
		v = (v & (DMA_TCR_RES_WR_MASK_13)) | ((0 & (DMA_TCR_MASK_13)) << 13);\
		v = (v & (DMA_TCR_RES_WR_MASK_5)) | ((0 & (DMA_TCR_MASK_5)) << 5);\
		v = ((v & DMA_TCR_OSP_WR_MASK) | (((data) & DMA_TCR_OSP_MASK) << 4));\
		DMA_TCR_REG_WR(i, v);\
} while (0)

#define DMA_TCR_PBL_MASK (unsigned long)(0x3f)
#define DMA_TCR_PBL_WR_MASK (unsigned long)(0xffc0ffff)
#define DMA_TCR_PBL_UDFWR(i, data) do {\
		unsigned long v;\
		DMA_TCR_REG_RD(i, v);\
		v = (v & (DMA_TCR_RES_WR_MASK_22)) | ((0 & (DMA_TCR_MASK_22)) << 22);\
		v = (v & (DMA_TCR_RES_WR_MASK_13)) | ((0 & (DMA_TCR_MASK_13)) << 13);\
		v = (v & (DMA_TCR_RES_WR_MASK_5)) | ((0 & (DMA_TCR_MASK_5)) << 5);\
		v = ((v & DMA_TCR_PBL_WR_MASK) | (((data) & DMA_TCR_PBL_MASK) << 16));\
		DMA_TCR_REG_WR(i, v);\
} while (0)

#define DMA_RCR_REG_ADDR (BASE_ADDRESS + 0x1108)
#define DMA_RCR_REG_ADDRESS(i) (DMA_RCR_REG_ADDR + ((i) * 128))
#define DMA_RCR_REG_WR(i, data) (iowrite32(data, (void *)DMA_RCR_REG_ADDRESS(i)))
#define DMA_RCR_REG_RD(i, data) ((data) = ioread32((void *)DMA_RCR_REG_ADDRESS(i)))

#define DMA_RCR_MASK_28 (unsigned long)(0xf)
#define DMA_RCR_RES_WR_MASK_28 (unsigned long)(0xfffffff)
#define DMA_RCR_MASK_22 (unsigned long)(0x7)
#define DMA_RCR_RES_WR_MASK_22 (unsigned long)(0xfe3fffff)
#define DMA_RCR_MASK_15 (unsigned long)(0x1)
#define DMA_RCR_RES_WR_MASK_15 (unsigned long)(0xffff7fff)

#define DMA_RCR_ST_MASK (unsigned long)(0x1)
#define DMA_RCR_ST_WR_MASK (unsigned long)(0xfffffffe)

#define DMA_RCR_ST_UDFWR(i, data) do {\
		unsigned long v;\
		DMA_RCR_REG_RD(i, v);\
		v = (v & (DMA_RCR_RES_WR_MASK_28)) | ((0 & (DMA_RCR_MASK_28)) << 28);\
		v = (v & (DMA_RCR_RES_WR_MASK_22)) | ((0 & (DMA_RCR_MASK_22)) << 22);\
		v = (v & (DMA_RCR_RES_WR_MASK_15)) | ((0 & (DMA_RCR_MASK_15)) << 15);\
		v = ((v & DMA_RCR_ST_WR_MASK) | (((data) & DMA_RCR_ST_MASK) << 0));\
		DMA_RCR_REG_WR(i, v);\
} while (0)

#define DMA_RCR_RBSZ_MASK (unsigned long)(0x3fff)
#define DMA_RCR_RBSZ_WR_MASK (unsigned long)(0xffff8001)
#define DMA_RCR_RBSZ_UDFWR(i, data) do {\
		unsigned long v;\
		DMA_RCR_REG_RD(i, v);\
		v = (v & (DMA_RCR_RES_WR_MASK_28)) | ((0 & (DMA_RCR_MASK_28)) << 28);\
		v = (v & (DMA_RCR_RES_WR_MASK_22)) | ((0 & (DMA_RCR_MASK_22)) << 22);\
		v = (v & (DMA_RCR_RES_WR_MASK_15)) | ((0 & (DMA_RCR_MASK_15)) << 15);\
		v = ((v & DMA_RCR_RBSZ_WR_MASK) | (((data) & DMA_RCR_RBSZ_MASK) << 1));\
		DMA_RCR_REG_WR(i, v);\
} while (0)

#define DMA_RCR_PBL_MASK (unsigned long)(0x3f)
#define DMA_RCR_PBL_WR_MASK (unsigned long)(0xffc0ffff)
#define DMA_RCR_PBL_UDFWR(i, data) do {\
		unsigned long v;\
		DMA_RCR_REG_RD(i, v);\
		v = (v & (DMA_RCR_RES_WR_MASK_28)) | ((0 & (DMA_RCR_MASK_28)) << 28);\
		v = (v & (DMA_RCR_RES_WR_MASK_22)) | ((0 & (DMA_RCR_MASK_22)) << 22);\
		v = (v & (DMA_RCR_RES_WR_MASK_15)) | ((0 & (DMA_RCR_MASK_15)) << 15);\
		v = ((v & DMA_RCR_PBL_WR_MASK) | (((data) & DMA_RCR_PBL_MASK) << 16));\
		DMA_RCR_REG_WR(i, v);\
} while (0)

#define DMA_TDLAR_REG_ADDR (BASE_ADDRESS + 0x1114)
#define DMA_TDLAR_REG_ADDRESS(i) (DMA_TDLAR_REG_ADDR + ((i) * 128))
#define DMA_TDLAR_REG_WR(i, data) (iowrite32(data, (void *)DMA_TDLAR_REG_ADDRESS(i)))

#define DMA_RDLAR_REG_ADDR (BASE_ADDRESS + 0x111c)
#define DMA_RDLAR_REG_ADDRESS(i) (DMA_RDLAR_REG_ADDR + ((i) * 128))
#define DMA_RDLAR_REG_WR(i, data) (iowrite32(data, (void *)DMA_RDLAR_REG_ADDRESS(i)))

#define DMA_TDTP_TPDR_REG_ADDR (BASE_ADDRESS + 0x1120)
#define DMA_TDTP_TPDR_REG_ADDRESS(i) (DMA_TDTP_TPDR_REG_ADDR + ((i) * 128))
#define DMA_TDTP_TPDR_REG_WR(i, data) (iowrite32(data, (void *)DMA_TDTP_TPDR_REG_ADDRESS(i)))

#define DMA_RDTP_RPDR_REG_ADDR (BASE_ADDRESS + 0x1128)
#define DMA_RDTP_RPDR_REG_ADDRESS(i) (DMA_RDTP_RPDR_REG_ADDR + ((i) * 128))
#define DMA_RDTP_RPDR_REG_WR(i, data) (iowrite32(data, (void *)DMA_RDTP_RPDR_REG_ADDRESS(i)))

#define DMA_TDRLR_REG_ADDR (BASE_ADDRESS + 0x112c)
#define DMA_TDRLR_REG_ADDRESS(i) (DMA_TDRLR_REG_ADDR + ((i) * 128))
#define DMA_TDRLR_REG_WR(i, data) (iowrite32(data, (void *)DMA_TDRLR_REG_ADDRESS(i)))

#define DMA_RDRLR_REG_ADDR (BASE_ADDRESS + 0x1130)
#define DMA_RDRLR_REG_ADDRESS(i) (DMA_RDRLR_REG_ADDR + ((i) * 128))
#define DMA_RDRLR_REG_WR(i, data) (iowrite32(data, (void *)DMA_RDRLR_REG_ADDRESS(i)))

#define DMA_IER_REG_ADDR (BASE_ADDRESS + 0x1134)
#define DMA_IER_REG_ADDRESS(i) (DMA_IER_REG_ADDR + ((i) * 128))
#define DMA_IER_REG_WR(i, data) (iowrite32(data, (void *)DMA_IER_REG_ADDRESS(i)))
#define DMA_IER_REG_RD(i, data) ((data) = ioread32((void *)DMA_IER_REG_ADDRESS(i)))

#define DMA_IER_MASK_16 (unsigned long)(0xffff)
#define DMA_IER_RES_WR_MASK_16 (unsigned long)(0xffff)
#define DMA_IER_MASK_3 (unsigned long)(0x7)
#define DMA_IER_RES_WR_MASK_3 (unsigned long)(0xffffffc7)

#define DMA_IER_RIE_MASK (unsigned long)(0x1)
#define DMA_IER_RIE_WR_MASK (unsigned long)(0xffffffbf)
#define DMA_IER_RIE_UDFWR(i, data) do {\
		unsigned long v;\
		DMA_IER_REG_RD(i, v);\
		v = (v & (DMA_IER_RES_WR_MASK_16)) | ((0 & (DMA_IER_MASK_16)) << 16);\
		v = (v & (DMA_IER_RES_WR_MASK_3)) | ((0 & (DMA_IER_MASK_3)) << 3);\
		v = ((v & DMA_IER_RIE_WR_MASK) | (((data) & DMA_IER_RIE_MASK) << 6));\
		DMA_IER_REG_WR(i, v);\
} while (0)

#define DMA_IER_RBUE_MASK (unsigned long)(0x1)
#define DMA_IER_RBUE_WR_MASK (unsigned long)(0xffffff7f)
#define DMA_IER_RBUE_UDFWR(i, data) do {\
		unsigned long v;\
		DMA_IER_REG_RD(i, v);\
		v = (v & (DMA_IER_RES_WR_MASK_16)) | ((0 & (DMA_IER_MASK_16)) << 16);\
		v = (v & (DMA_IER_RES_WR_MASK_3)) | ((0 & (DMA_IER_MASK_3)) << 3);\
		v = ((v & DMA_IER_RBUE_WR_MASK) | (((data) & DMA_IER_RBUE_MASK) << 7));\
		DMA_IER_REG_WR(i, v);\
} while (0)

#define DMA_RIWTR_REG_ADDR (BASE_ADDRESS + 0x1138)
#define DMA_RIWTR_REG_ADDRESS(i) (DMA_RIWTR_REG_ADDR + ((i) * 128))
#define DMA_RIWTR_REG_WR(i, data) (iowrite32(data, (void *)DMA_RIWTR_REG_ADDRESS(i)))

#define DMA_RIWTR_MASK_8 (unsigned long)(0xffffff)
#define DMA_RIWTR_RES_WR_MASK_8 (unsigned long)(0xff)
#define DMA_RIWTR_RWT_MASK (unsigned long)(0xff)
#define DMA_RIWTR_RWT_WR_MASK (unsigned long)(0xffffff00)
#define DMA_RIWTR_RWT_UDFWR(i, data) do {\
		unsigned long v = 0; \
		v = (v & (DMA_RIWTR_RES_WR_MASK_8)) | (((0) & (DMA_RIWTR_MASK_8)) << 8);\
		(v) = ((v & DMA_RIWTR_RWT_WR_MASK) | (((data) & DMA_RIWTR_RWT_MASK) << 0));\
		DMA_RIWTR_REG_WR(i, v);\
} while (0)

#define DMA_SR_REG_ADDR (BASE_ADDRESS + 0x1160)
#define DMA_SR_REG_ADDRESS(i) (DMA_SR_REG_ADDR + ((i) * 0x80))
#define DMA_SR_REG_WR(i, data) (iowrite32(data, (void *)DMA_SR_REG_ADDRESS(i)))
#define DMA_SR_REG_RD(i, data) ((data) = ioread32((void *)DMA_SR_REG_ADDRESS(i)))

#define DMA_SR_TI_LPOS 0
#define DMA_SR_TI_HPOS 0
#define DMA_SR_TPS_LPOS 1
#define DMA_SR_TPS_HPOS 1
#define DMA_SR_TBU_LPOS 2
#define DMA_SR_TBU_HPOS 2
#define DMA_SR_RI_LPOS 6
#define DMA_SR_RI_HPOS 6
#define DMA_SR_RBU_LPOS 7
#define DMA_SR_RBU_HPOS 7
#define DMA_SR_RPS_LPOS 8
#define DMA_SR_RPS_HPOS 8
#define DMA_SR_RWT_LPOS 9
#define DMA_SR_RWT_HPOS 9
#define DMA_SR_FBE_LPOS 12
#define DMA_SR_FBE_HPOS 12

#define GET_VALUE(data, lbit, hbit) (((data) >> (lbit)) & \
	(~(~0 << ((hbit) - (lbit) + 1))))

#endif
