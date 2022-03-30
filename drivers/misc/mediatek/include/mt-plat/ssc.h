/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#if !defined(__SSC_H__)
#define __SSC_H__

#include <linux/kernel.h>
#include <linux/notifier.h>

#define MTK_SSC_DTS_COMPATIBLE "mediatek,ssc"
#define MTK_SSC_SAFE_VLOGIC_STRING "safe-vlogic-uV"

enum ssc_notifier {
	SSC_ENABLE_VLOGIC_BOUND = 1,
	SSC_DISABLE_VLOGIC_BOUND,
	SSC_TIMEOUT,
};

enum ssc_reqeust_id {
	SSC_GPU = 0,
	SSC_APU,
	SSC_SW,
	SSC_ERR,
	SSC_REQUEST_NUM,
};

int ssc_vlogic_bound_register_notifier(struct notifier_block *nb);
int ssc_vlogic_bound_unregister_notifier(struct notifier_block *nb);
int ssc_enable_vlogic_bound(int request_id);
int ssc_disable_vlogic_bound(int request_id);
unsigned int ssc_get_safe_vlogic(void);

#endif/* __SSC_H__ */
