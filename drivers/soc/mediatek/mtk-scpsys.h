/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_SCPSYS_H__
#define __MTK_SCPSYS_H__

struct scp_event_data {
	int event_type;
	int domain_id;
	struct generic_pm_domain *genpd;
};

enum scp_event_type {
	MTK_SCPSYS_PSTATE,
};

int register_scpsys_notifier(struct notifier_block *nb);
int unregister_scpsys_notifier(struct notifier_block *nb);

#endif /* __MTK_SCPSYS_H__ */

