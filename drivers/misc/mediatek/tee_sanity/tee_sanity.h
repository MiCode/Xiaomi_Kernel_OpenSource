/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __TEE_SANITY_H__
#define __TEE_SANITY_H__

#define PFX                     KBUILD_MODNAME ": "

#define tee_log(p, s, fmt, args...) \
	(p += scnprintf(p, sizeof(s) - strlen(s), fmt, ##args))

/* TEE sanity UT commands */
#define TEE_UT_READ_INTR	0
#define TEE_UT_TRIGGER_INTR	1

/* MTK_SIP_KERNEL_TEE_CONTROL SMC op_id */
#define TEE_OP_ID_NONE                  (0xFFFF0000)
#define TEE_OP_ID_SET_PENDING           (0xFFFF0001)

#endif /* __TEE_SANITY_H__ */
