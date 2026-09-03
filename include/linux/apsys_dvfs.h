// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef APSYSDVFS_H
#define APSYSDVFS_H

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/types.h>

#if IS_ENABLED(CONFIG_DVFS_APSYS_SPRD)
int dpu_dvfs_notifier_call_chain(void *data);
int gsp_dvfs_notifier_call_chain(void *data);
int vdsp_dvfs_notifier_call_chain(void *data);
int vsp_dvfs_notifier_call_chain(void *data, bool is_enc);
#else
static inline int dpu_dvfs_notifier_call_chain(void *data)
{
	return 0;
}

static inline int gsp_dvfs_notifier_call_chain(void *data)
{
	return 0;
}

static inline int vdsp_dvfs_notifier_call_chain(void *data)
{
	return 0;
}

static inline int vsp_dvfs_notifier_call_chain(void *data, bool is_enc)
{
	return 0;
}
#endif

#endif		/* APSYSDVFS_H */