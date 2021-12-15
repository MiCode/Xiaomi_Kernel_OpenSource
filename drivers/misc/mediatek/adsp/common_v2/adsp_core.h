/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ADSP_CORE_H__
#define __ADSP_CORE_H__

#include "adsp_helper.h"

enum ADSP_CORE_STATE {
	ADSP_RESET       = 0,
	ADSP_SUSPEND     = 1,
	ADSP_SLEEP       = 2,
	ADSP_RUNNING     = 3,
	ADSP_SUSPENDING  = 4,
};

enum {
	APTIME_UNFREEZE  = 0,
	APTIME_FREEZE    = 1,
};

struct timesync_t {
	u32 tick_h;
	u32 tick_l;
	u32 ts_h;
	u32 ts_l;
	u32 freeze;
	u32 version;
};

struct adsp_priv;

/* core api */
#define get_adsp_core_by_ptr(ptr)  _get_adsp_core(ptr, 0)
#define get_adsp_core_by_id(id)    _get_adsp_core(NULL, id)

struct adsp_priv *_get_adsp_core(void *ptr, int id);

void set_adsp_state(struct adsp_priv *pdata, int state);
int get_adsp_state(struct adsp_priv *pdata);
bool is_adsp_system_running(void);

void __iomem *adsp_get_sharedmem_base(struct adsp_priv *pdata, int id);
int adsp_copy_to_sharedmem(struct adsp_priv *pdata, int id, const void *src,
			int count);
int adsp_copy_from_sharedmem(struct adsp_priv *pdata, int id, void *dst,
			int count);
void timesync_to_adsp(struct adsp_priv *pdata, u32 fz);
void switch_adsp_power(bool on);

int adsp_reset(void);
void adsp_extern_notify_chain(enum ADSP_NOTIFY_EVENT event);

/* wakelock */
int adsp_awake_init(struct adsp_priv *pdata, u32 mask);
int adsp_awake_lock(u32 cid);
int adsp_awake_unlock(u32 cid);
#endif
