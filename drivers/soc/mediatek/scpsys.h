/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __SOC_MEDIATEK_SCPSYS_H
#define __SOC_MEDIATEK_SCPSYS_H

#define MAX_STEPS	4

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

#define MT7622_TOP_AXI_PROT_EN_ETHSYS		(BIT(3) | BIT(17))
#define MT7622_TOP_AXI_PROT_EN_HIF0		(BIT(24) | BIT(25))
#define MT7622_TOP_AXI_PROT_EN_HIF1		(BIT(26) | BIT(27) | \
						 BIT(28))
#define MT7622_TOP_AXI_PROT_EN_WB		(BIT(2) | BIT(6) | \
						 BIT(7) | BIT(8))

#define MT8173_TOP_AXI_PROT_EN_MM_M0		BIT(1)
#define MT8173_TOP_AXI_PROT_EN_MM_M1		BIT(2)
#define MT8173_TOP_AXI_PROT_EN_MFG_S		BIT(14)
#define MT8173_TOP_AXI_PROT_EN_MFG_M0		BIT(21)
#define MT8173_TOP_AXI_PROT_EN_MFG_M1		BIT(22)
#define MT8173_TOP_AXI_PROT_EN_MFG_SNOOP_OUT	BIT(23)

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
