/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __ADSP_FEATURE_DEFINE_H__
#define __ADSP_FEATURE_DEFINE_H__

enum {
	DEREGI_FLAG_NODELAY = 1 << 0,
};

struct adsp_feature_control {
	int total;
	unsigned int feature_set; /* from dts */
	struct mutex lock;
	struct workqueue_struct *wq;
	struct delayed_work suspend_work;
	int delay_ms;
	int (*suspend)(void);
	int (*resume)(void);
};

ssize_t adsp_dump_feature_state(u32 cid, char *buffer, int size);
int adsp_get_feature_index(const char *str);
bool is_feature_in_set(u32 cid, u32 fid);
bool adsp_feature_is_active(u32 cid);
bool flush_suspend_work(u32 cid);

int _adsp_register_feature(u32 cid, u32 fid, u32 opt);
int _adsp_deregister_feature(u32 cid, u32 fid, u32 opt);

int init_adsp_feature_control(u32 cid, u32 feature_set, int delay_ms,
			struct workqueue_struct *wq,
			int (*_suspend)(void),
			int (*_resume)(void));
#endif
