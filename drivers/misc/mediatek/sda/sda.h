/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 Mediatek Inc.
 */

#ifndef __MTK_SDA_H__
#define __MTK_SDA_H__

enum SDA_FEATURE {
	SDA_BUS_PARITY = 0,
	NR_SDA_FEATURE
};

enum BUS_PARITY_OP {
	BP_MCU_CLR = 0,
	NR_BUS_PARITY_OP
};

struct tag_chipid {
	u32 size;
	u32 hw_code;
	u32 hw_subcode;
	u32 hw_ver;
	u32 sw_ver;
};
#endif   /*__MTK_SDA_H__*/
