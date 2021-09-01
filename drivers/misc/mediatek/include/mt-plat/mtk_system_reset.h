/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef _MTK_SYSTEM_RESET_H
#define _MTK_SYSTEM_RESET_H

#include <uapi/linux/psci.h>

/* no export symbol to aee_exception_reboot, only used in exception flow */
/* PSCI v1.1 extended power state encoding for SYSTEM_RESET2 function */
#define PSCI_1_1_RESET2_TYPE_VENDOR_SHIFT   31
#define PSCI_1_1_RESET2_TYPE_VENDOR     \
	(1 << PSCI_1_1_RESET2_TYPE_VENDOR_SHIFT)

#if IS_ENABLED(CONFIG_64BIT)
#define PSCI_FN_NATIVE(version, name)   PSCI_##version##_FN64_##name
#else
#define PSCI_FN_NATIVE(version, name)   PSCI_##version##_FN_##name
#endif

/*
 * reset type bit definition
 * Bit 0 - 7  for MTK Domain definition
 * Bit 8 - 15 for definition by MTK Domain
 * Bit 16 -30 for reserved bit
 * Bit 31     for vendor bit
 */

#define RESET2_TYPE_DOMAIN_SHIFT	UL(0)
#define RESET2_TYPE_DOMAIN_USAGE_SHIFT	UL(8)
#define RESET2_TYPE_VENDOR		PSCI_1_1_RESET2_TYPE_VENDOR_SHIFT

/* MTK Domain definition */
enum mtk_domain {
	/* 0 will not be used to avoid the case no setting reset type */
	MTK_DOMAIN_AEE = 1,
	MTK_DOMAIN_MAX = 255,
};

#endif /* _MTK_SYSTEM_RESET_H */
