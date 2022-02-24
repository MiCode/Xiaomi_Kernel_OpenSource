/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MFD_MT6359_IRQ_H__
#define __MFD_MT6359_IRQ_H__

#define SP_TOP_GEN(sp)	\
	{	\
		.hwirq_base = -1,	\
		.num_int_regs = 1,	\
		.en_reg = MT6359_##sp##_TOP_INT_CON0,		\
		.mask_reg = MT6359_##sp##_TOP_INT_MASK_CON0,	\
		.sta_reg = MT6359_##sp##_TOP_INT_STATUS0,		\
		.raw_sta_reg = MT6359_##sp##_TOP_INT_RAW_STATUS0,	\
		.top_offset = PMIC_INT_STATUS_##sp##_TOP_SHIFT,		\
	}

#endif /* __MFD_MT6359_IRQ_H__ */
