/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef MDSS_DEBUG_H
#define MDSS_DEBUG_H

#include <stdarg.h>
#include "mdss.h"
#include "mdss_mdp_trace.h"

#define MISR_POLL_SLEEP		2000
#define MISR_POLL_TIMEOUT	32000
#define MISR_CRC_BATCH_CFG	0x101
#define DATA_LIMITER (-1)
#define XLOG_TOUT_DATA_LIMITER (NULL)
#define XLOG_FUNC_ENTRY	0x1111
#define XLOG_FUNC_EXIT	0x2222
#define MDSS_REG_BLOCK_NAME_LEN (5)

#define MDSS_XLOG(...) mdss_xlog(__func__, ##__VA_ARGS__, DATA_LIMITER)
#define MDSS_XLOG_TOUT_HANDLER(...)	\
	mdss_xlog_tout_handler(__func__, ##__VA_ARGS__, XLOG_TOUT_DATA_LIMITER)

#define ATRACE_END(name) trace_tracing_mark_write(current->tgid, name, 0)
#define ATRACE_BEGIN(name) trace_tracing_mark_write(current->tgid, name, 1)
#define ATRACE_FUNC() ATRACE_BEGIN(__func__)

#define ATRACE_INT(name, value) \
	trace_mdp_trace_counter(current->tgid, name, value)

#ifdef CONFIG_DEBUG_FS
struct mdss_debug_base {
	struct mdss_debug_data *mdd;
	char name[80];
	void __iomem *base;
	size_t off;
	size_t cnt;
	size_t max_offset;
	char *buf;
	size_t buf_len;
	struct list_head head;
};

struct debug_log {
	struct dentry *xlog;
	u32 xlog_enable;
	u32 panic_on_err;
	u32 enable_reg_dump;
};

struct mdss_debug_data {
	struct dentry *root;
	struct list_head base_list;
	struct debug_log logd;
};

int mdss_debugfs_init(struct mdss_data_type *mdata);
int mdss_debugfs_remove(struct mdss_data_type *mdata);
int mdss_debug_register_base(const char *name, void __iomem *base,
				    size_t max_offset);
int mdss_misr_set(struct mdss_data_type *mdata, struct mdp_misr *req,
			struct mdss_mdp_ctl *ctl);
int mdss_misr_get(struct mdss_data_type *mdata, struct mdp_misr *resp,
			struct mdss_mdp_ctl *ctl);
void mdss_misr_crc_collect(struct mdss_data_type *mdata, int block_id);

int mdss_create_xlog_debug(struct mdss_debug_data *mdd);
void mdss_xlog(const char *name, ...);
void mdss_xlog_dump(void);
void mdss_dump_reg(char __iomem *base, int len);
void mdss_dsi_debug_check_te(struct mdss_panel_data *pdata);
void mdss_xlog_tout_handler(const char *name, ...);
#else
static inline int mdss_debugfs_init(struct mdss_data_type *mdata) { return 0; }
static inline int mdss_debugfs_remove(struct mdss_data_type *mdata)
{ return 0; }
static inline int mdss_debug_register_base(const char *name, void __iomem *base,
					size_t max_offset) { return 0; }
static inline int mdss_misr_set(struct mdss_data_type *mdata,
					struct mdp_misr *req,
					struct mdss_mdp_ctl *ctl)
{ return 0; }
static inline int mdss_misr_get(struct mdss_data_type *mdata,
					struct mdp_misr *resp,
					struct mdss_mdp_ctl *ctl)
{ return 0; }
static inline void mdss_misr_crc_collect(struct mdss_data_type *mdata,
						int block_id) { }

static inline int create_xlog_debug(struct mdss_data_type *mdata) { }
static inline void mdss_xlog(const char *name, ...) { }
static inline void mdss_xlog_dump(void) { }
static inline void mdss_dump_reg(char __iomem *base, int len) { }
static inline void mdss_dsi_debug_check_te(struct mdss_panel_data *pdata) { }
static inline void mdss_xlog_tout_handler(const char *name, ...) { }
#endif
#endif /* MDSS_DEBUG_H */
