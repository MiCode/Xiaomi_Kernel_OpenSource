/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __QCOM_TGU_H__
#define __QCOM_TGU_H__

struct tgu_test_notifier {
	void (*cb)(void);
};

extern int register_tgu_notifier(struct tgu_test_notifier *tgu_test);
extern int unregister_tgu_notifier(struct tgu_test_notifier *tgu_test);
#endif /* __QCOM_MPM_H__ */
