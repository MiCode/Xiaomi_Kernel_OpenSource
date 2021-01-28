/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#ifndef __MODEM_SECURE_BASE_H__
#define __MODEM_SECURE_BASE_H__

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>


size_t mt_secure_call(size_t function_id,
		size_t arg0, size_t arg1, size_t arg2,
		size_t arg3, size_t r1, size_t r2, size_t r3);

#define mdreg_write32(reg_id, value)		\
	mt_secure_call(MTK_SIP_KERNEL_CCCI_GET_INFO, \
			reg_id, value, 0, 0, 0, 0, 0)




#endif				/* __MODEM_SECURE_BASE_H__ */
