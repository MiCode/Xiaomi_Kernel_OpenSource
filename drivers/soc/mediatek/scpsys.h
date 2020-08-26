/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __SOC_MEDIATEK_SCPSYS_H
#define __SOC_MEDIATEK_SCPSYS_H

#define MAX_STEPS	5

#define _BUS_PROT(_type, _set_ofs, _clr_ofs,			\
		_en_ofs, _sta_ofs, _mask, _ignore_clr_ack) {	\
		.type = _type,					\
		.set_ofs = _set_ofs,				\
		.clr_ofs = _clr_ofs,				\
		.en_ofs = _en_ofs,				\
		.sta_ofs = _sta_ofs,				\
		.mask = _mask,					\
		.ignore_clr_ack = _ignore_clr_ack,		\
	}

#define BUS_PROT(_type, _set_ofs, _clr_ofs,		\
		_en_ofs, _sta_ofs, _mask)		\
		_BUS_PROT(_type, _set_ofs, _clr_ofs,	\
		_en_ofs, _sta_ofs, _mask, false)

#define BUS_PROT_IGN(_type, _set_ofs, _clr_ofs,	\
		_en_ofs, _sta_ofs, _mask)		\
		_BUS_PROT(_type, _set_ofs, _clr_ofs,	\
		_en_ofs, _sta_ofs, _mask, true)

#define MT2701_TOP_AXI_PROT_EN_MM_M0		BIT(1)
#define MT2701_TOP_AXI_PROT_EN_CONN_M		BIT(2)
#define MT2701_TOP_AXI_PROT_EN_CONN_S		BIT(8)

#define MT6873_TOP_AXI_PROT_EN_MD		BIT(7)
#define MT6873_TOP_AXI_PROT_EN_VDNR_MD		(BIT(2) | BIT(14) | \
						 BIT(22))

#define MT7622_TOP_AXI_PROT_EN_ETHSYS		(BIT(3) | BIT(17))
#define MT7622_TOP_AXI_PROT_EN_HIF0		(BIT(24) | BIT(25))
#define MT7622_TOP_AXI_PROT_EN_HIF1		(BIT(26) | BIT(27) | \
						 BIT(28))
#define MT7622_TOP_AXI_PROT_EN_WB		(BIT(2) | BIT(6) | \
						 BIT(7) | BIT(8))

#define MT6853_TOP_AXI_PROT_EN_MD	(BIT(7))
#define MT6853_TOP_AXI_PROT_EN_VDNR_MD	(BIT(2) | BIT(12) |  \
			BIT(20))
#define MT6853_TOP_AXI_PROT_EN_CONN	(BIT(13) | BIT(18))
#define MT6853_TOP_AXI_PROT_EN_CONN_2ND	(BIT(14))
#define MT6853_TOP_AXI_PROT_EN_1_CONN	(BIT(10))
#define MT6853_TOP_AXI_PROT_EN_1_MFG1	(BIT(21))
#define MT6853_TOP_AXI_PROT_EN_2_MFG1	(BIT(5) | BIT(6))
#define MT6853_TOP_AXI_PROT_EN_MFG1	(BIT(21) | BIT(22))
#define MT6853_TOP_AXI_PROT_EN_2_MFG1_2ND	(BIT(7))
#define MT6853_TOP_AXI_PROT_EN_MM_2_ISP	(BIT(8))
#define MT6853_TOP_AXI_PROT_EN_MM_2_ISP_2ND	(BIT(9))
#define MT6853_TOP_AXI_PROT_EN_MM_IPE	(BIT(16))
#define MT6853_TOP_AXI_PROT_EN_MM_IPE_2ND	(BIT(17))
#define MT6853_TOP_AXI_PROT_EN_MM_VDEC	(BIT(24))
#define MT6853_TOP_AXI_PROT_EN_MM_VDEC_2ND	(BIT(25))
#define MT6853_TOP_AXI_PROT_EN_MM_VENC	(BIT(26))
#define MT6853_TOP_AXI_PROT_EN_MM_VENC_2ND	(BIT(27))
#define MT6853_TOP_AXI_PROT_EN_MM_DISP	(BIT(0) | BIT(2) |  \
			BIT(10) | BIT(12) |  \
			BIT(16) | BIT(24) |  \
			BIT(26))
#define MT6853_TOP_AXI_PROT_EN_MM_2_DISP	(BIT(8))
#define MT6853_TOP_AXI_PROT_EN_DISP	(BIT(6) | BIT(23))
#define MT6853_TOP_AXI_PROT_EN_MM_DISP_2ND	(BIT(1) | BIT(3) |  \
			BIT(17) | BIT(25) |  \
			BIT(27))
#define MT6853_TOP_AXI_PROT_EN_MM_2_DISP_2ND	(BIT(9))
#define MT6853_TOP_AXI_PROT_EN_2_AUDIO	(BIT(4))
#define MT6853_TOP_AXI_PROT_EN_2_ADSP_DORMANT	(BIT(3))
#define MT6853_TOP_AXI_PROT_EN_2_CAM	(BIT(0))
#define MT6853_TOP_AXI_PROT_EN_MM_CAM	(BIT(0) | BIT(2))
#define MT6853_TOP_AXI_PROT_EN_1_CAM	(BIT(22))
#define MT6853_TOP_AXI_PROT_EN_MM_CAM_2ND	(BIT(1) | BIT(3))
#define MT6853_TOP_AXI_PROT_EN_VDNR_CAM	(BIT(19))

#define MT8173_TOP_AXI_PROT_EN_MM_M0		BIT(1)
#define MT8173_TOP_AXI_PROT_EN_MM_M1		BIT(2)
#define MT8173_TOP_AXI_PROT_EN_MFG_S		BIT(14)
#define MT8173_TOP_AXI_PROT_EN_MFG_M0		BIT(21)
#define MT8173_TOP_AXI_PROT_EN_MFG_M1		BIT(22)
#define MT8173_TOP_AXI_PROT_EN_MFG_SNOOP_OUT	BIT(23)

#define MT8192_TOP_AXI_PROT_EN_DISP			(BIT(6) | BIT(23))
#define MT8192_TOP_AXI_PROT_EN_CONN			(BIT(13) | BIT(18))
#define MT8192_TOP_AXI_PROT_EN_CONN_2ND		BIT(14)
#define MT8192_TOP_AXI_PROT_EN_MFG1			GENMASK(22, 21)
#define MT8192_TOP_AXI_PROT_EN_1_CONN			BIT(10)
#define MT8192_TOP_AXI_PROT_EN_1_MFG1			BIT(21)
#define MT8192_TOP_AXI_PROT_EN_1_CAM			BIT(22)
#define MT8192_TOP_AXI_PROT_EN_2_CAM			BIT(0)
#define MT8192_TOP_AXI_PROT_EN_2_ADSP			BIT(3)
#define MT8192_TOP_AXI_PROT_EN_2_AUDIO			BIT(4)
#define MT8192_TOP_AXI_PROT_EN_2_MFG1			GENMASK(6, 5)
#define MT8192_TOP_AXI_PROT_EN_2_MFG1_2ND		BIT(7)
#define MT8192_TOP_AXI_PROT_EN_MM_CAM			(BIT(0) | BIT(2))
#define MT8192_TOP_AXI_PROT_EN_MM_DISP			(BIT(0) | BIT(2) | \
							BIT(10) | BIT(12) | \
							BIT(14) | BIT(16) | \
							BIT(24) | BIT(26))
#define MT8192_TOP_AXI_PROT_EN_MM_CAM_2ND		(BIT(1) | BIT(3))
#define MT8192_TOP_AXI_PROT_EN_MM_DISP_2ND		(BIT(1) | BIT(3) | \
							BIT(15) | BIT(17) | \
							BIT(25) | BIT(27))
#define MT8192_TOP_AXI_PROT_EN_MM_ISP2			BIT(14)
#define MT8192_TOP_AXI_PROT_EN_MM_ISP2_2ND		BIT(15)
#define MT8192_TOP_AXI_PROT_EN_MM_IPE			BIT(16)
#define MT8192_TOP_AXI_PROT_EN_MM_IPE_2ND		BIT(17)
#define MT8192_TOP_AXI_PROT_EN_MM_VDEC			BIT(24)
#define MT8192_TOP_AXI_PROT_EN_MM_VDEC_2ND		BIT(25)
#define MT8192_TOP_AXI_PROT_EN_MM_VENC			BIT(26)
#define MT8192_TOP_AXI_PROT_EN_MM_VENC_2ND		BIT(27)
#define MT8192_TOP_AXI_PROT_EN_MM_2_ISP		BIT(8)
#define MT8192_TOP_AXI_PROT_EN_MM_2_DISP		(BIT(8) | BIT(12))
#define MT8192_TOP_AXI_PROT_EN_MM_2_ISP_2ND		BIT(9)
#define MT8192_TOP_AXI_PROT_EN_MM_2_DISP_2ND		(BIT(9) | BIT(13))
#define MT8192_TOP_AXI_PROT_EN_MM_2_MDP		BIT(12)
#define MT8192_TOP_AXI_PROT_EN_MM_2_MDP_2ND		BIT(13)
#define MT8192_TOP_AXI_PROT_EN_VDNR_CAM		BIT(21)

enum regmap_type {
	INVALID_TYPE = 0,
	IFR_TYPE,
	SMI_TYPE,
};

struct bus_prot {
	enum regmap_type type;
	u32 set_ofs;
	u32 clr_ofs;
	u32 en_ofs;
	u32 sta_ofs;
	u32 mask;
	bool ignore_clr_ack;
};

#endif /* __SOC_MEDIATEK_SCPSYS_H */
