/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_IPI_H__
#define __MDLA_IPI_H__

#include <linux/types.h>

enum MDLA_IPI_TYPE_0 {
	MDLA_IPI_PWR_TIME,
	MDLA_IPI_TIMEOUT,
	MDLA_IPI_ULOG,
	MDLA_IPI_TRACE_ENABLE,
	MDLA_IPI_CMD_CHECK,
	MDLA_IPI_PREEMPT_CNT,
	MDLA_IPI_PMU_COUNT,
	MDLA_IPI_ADDR,
	MDLA_IPI_PROFILE_ENABLE,
	MDLA_IPI_FORCE_PWR_ON,
	MDLA_IPI_DUMP_CMDBUF_EN,
	MDLA_IPI_INFO,

	NF_MDLA_IPI_TYPE_0
};


enum MDLA_IPI_ADDR_TYPE_1 {
	MDLA_IPI_ADDR_BOOT,
	MDLA_IPI_ADDR_MAIN,
	MDLA_IPI_ADDR_DBG_DATA,

	MDLA_IPI_ADDR_BOOT_SZ,
	MDLA_IPI_ADDR_MAIN_SZ,
	MDLA_IPI_ADDR_DBG_DATA_SZ,

	NF_MDLA_IPI_ADDR_TYPE_1
};


enum MDLA_IPI_INFO_TYPE_1 {
	MDLA_IPI_INFO_PWR,
	MDLA_IPI_INFO_REG,
	MDLA_IPI_INFO_CMDBUF,

	NF_MDLA_IPI_INFO_TYPE_1
};

enum MDLA_IPI_DIR_TYPE {
	MDLA_IPI_READ,
	MDLA_IPI_WRITE,
};

#define DEFINE_IPI_DBGFS_ATTRIBUTE(name, TYPE_0, TYPE_1, fmt)		\
static int name ## _set(void *data, u64 val)				\
{									\
	mdla_ipi_send(TYPE_0, TYPE_1, val);				\
	*(u64 *)data = val;						\
	return 0;							\
}									\
static int name ## _get(void *data, u64 *val)				\
{									\
	mdla_ipi_recv(TYPE_0, TYPE_1, val);				\
	*(u64 *)data = *val;						\
	return 0;							\
}									\
static int name ## _open(struct inode *i, struct file *f)		\
{									\
	__simple_attr_check_format(fmt, 0ull);				\
	return simple_attr_open(i, f, name ## _get, name ## _set, fmt);	\
}									\
static const struct file_operations name ## _fops = {			\
	.owner	 = THIS_MODULE,						\
	.open	 = name ## _open,					\
	.release = simple_attr_release,					\
	.read	 = debugfs_attr_read,					\
	.write	 = debugfs_attr_write,					\
	.llseek  = no_llseek,						\
}



int mdla_ipi_send(int type_0, int type_1, u64 val);
int mdla_ipi_recv(int type_0, int type_1, u64 *val);
int mdla_ipi_init(void);
void mdla_ipi_deinit(void);

#endif /* __MDLA_IPI_H__ */

