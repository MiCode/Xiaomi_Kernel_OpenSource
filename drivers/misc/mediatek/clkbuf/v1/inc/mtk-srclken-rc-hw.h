/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: ren-ting.wang <ren-ting.wang@mediatek.com>
 */

#ifndef SRCLKEN_RC_HW_H
#define SRCLKEN_RC_HW_H

#include "mtk_clkbuf_common.h"

#define RC_INIT_DONE		1

struct srclken_rc_subsys {
	const char *name;
	struct xo_buf_ctl_t xo_buf_ctl;
	u32 init_mode;
	u32 init_req;
	u8 xo_idx;
	u8 idx;
};

struct srclken_rc_hw {
	struct srclken_rc_subsys *subsys;
	bool init_done;
	u8 subsys_num;
};

bool is_srclken_rc_init_done(void);
int srclken_rc_init(void);
void srclken_rc_exit(void);
int srclken_rc_post_init(void);
int srclken_rc_hw_init(struct platform_device *pdev);
int srclken_rc_get_subsys_req_mode(u8 idx, u32 *val);
int srclken_rc_get_subsys_sw_req(u8 idx, u32 *val);
int srclken_rc_get_cfg_val(const char *name, u32 *val);
u8 srclken_rc_get_subsys_count(void);
const char *srclken_rc_get_subsys_name(u8 idx);
void srclken_rc_init_done_callback(int rc_init_done);
void __srclken_rc_xo_buf_callback_init(struct xo_buf_ctl_t *xo_buf_ctl);
int srclken_rc_get_cfg_count(void);
const char *srclken_rc_get_cfg_name(u32 idx);
int srclken_rc_subsys_ctrl(u8 idx, const char *mode);
int __srclken_rc_subsys_ctrl(struct srclken_rc_subsys *subsys,
		enum CLKBUF_CTL_CMD cmd, enum SRCLKEN_RC_REQ rc_req);
int srclken_rc_dump_time(u8 idx, char *buf, u32 buf_size);
int srclken_rc_dump_trace(u8 idx, char *buf, u32 buf_size);
u8 rc_get_trace_num(void);
int srclken_rc_dump_subsys_sta(u8 idx, char *buf);
int srclken_rc_dump_sta(const char *name, char *buf);

/* Sysfs functions */
ssize_t rc_cfg_ctl_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
ssize_t rc_trace_ctl_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t rc_trace_ctl_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
ssize_t rc_subsys_ctl_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
ssize_t rc_subsys_ctl_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t rc_subsys_sta_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t rc_subsys_sta_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);
ssize_t rc_sta_reg_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);
ssize_t rc_sta_reg_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

#endif /* SRCLKEN_RC_HW_H */
