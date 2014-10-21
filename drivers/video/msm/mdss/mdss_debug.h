/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/mdss_io_util.h>

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

enum mdss_dbg_reg_dump_flag {
	MDSS_REG_DUMP_IN_LOG = BIT(0),
	MDSS_REG_DUMP_IN_MEM = BIT(1),
};

enum mdss_dbg_xlog_flag {
	MDSS_XLOG_DEFAULT = BIT(0),
	MDSS_XLOG_IOMMU = BIT(1),
	MDSS_XLOG_DBG = BIT(6),
	MDSS_XLOG_ALL = BIT(7)
};

#define MDSS_XLOG(...) mdss_xlog(__func__, __LINE__, MDSS_XLOG_DEFAULT, \
		##__VA_ARGS__, DATA_LIMITER)

#define MDSS_XLOG_TOUT_HANDLER(...)	\
	mdss_xlog_tout_handler_default(__func__, ##__VA_ARGS__, \
		XLOG_TOUT_DATA_LIMITER)

#define MDSS_XLOG_DBG(...) mdss_xlog(__func__, __LINE__, MDSS_XLOG_DBG, \
		##__VA_ARGS__, DATA_LIMITER)

#define MDSS_XLOG_ALL(...) mdss_xlog(__func__, __LINE__, MDSS_XLOG_ALL,	\
		##__VA_ARGS__, DATA_LIMITER)

#define MDSS_XLOG_IOMMU(...) mdss_xlog(__func__, __LINE__, MDSS_XLOG_IOMMU, \
		##__VA_ARGS__, DATA_LIMITER)

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
	u32 *reg_dump;
};

struct mdss_debug_data {
	struct dentry *root;
	struct dentry *perf;
	struct list_head base_list;
};

#define DEFINE_MDSS_DEBUGFS_SEQ_FOPS(__prefix)				\
static int __prefix ## _open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, __prefix ## _show, inode->i_private);	\
}									\
static const struct file_operations __prefix ## _fops = {		\
	.owner = THIS_MODULE,						\
	.open = __prefix ## _open,					\
	.release = single_release,					\
	.read = seq_read,						\
	.llseek = seq_lseek,						\
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
void mdss_xlog(const char *name, int line, int flag, ...);
void mdss_dump_reg(struct mdss_debug_base *dbg, u32 reg_dump_flag);
void mdss_xlog_tout_handler_default(const char *name, ...);
int mdss_xlog_tout_handler_iommu(struct iommu_domain *domain,
	struct device *dev, unsigned long iova, int flags, void *token);
#else
static inline int mdss_debugfs_init(struct mdss_data_type *mdata) { return 0; }
static inline int mdss_debugfs_remove(struct mdss_data_type *mdata)
{
	return 0;
}
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

static inline int create_xlog_debug(struct mdss_data_type *mdata) { return 0; }
static inline void mdss_xlog_dump(void) { }
static inline void mdss_dump_reg(struct mdss_debug_base *dbg,
	u32 reg_dump_flag) { }
static inline void mdss_xlog(const char *name, int line, int flag...) { }
static inline void mdss_dsi_debug_check_te(struct mdss_panel_data *pdata) { }
static inline void mdss_xlog_tout_handler_default(const char *name, ...) { }
static inline int  mdss_xlog_tout_handler_iommu(struct iommu_domain *domain,
	struct device *dev, unsigned long iova, int flags, void *token) { }
#endif

static inline int mdss_debug_register_io(const char *name,
		struct dss_io_data *io_data)
{
	return mdss_debug_register_base(name, io_data->base, io_data->len);
}

#endif /* MDSS_DEBUG_H */
