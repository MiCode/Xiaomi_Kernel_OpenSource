/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_RV_H__
#define __MTK_APU_MDW_RV_H__

#include "mdw.h"
#include "mdw_rv_msg.h"

struct mdw_rv_dev;

struct mdw_rv_cmd {
	struct mdw_cmd *c;
	struct mdw_mem *cb;
	struct list_head u_item; // to usr list
	struct mdw_ipi_msg_sync s_msg; // for ipi
	uint64_t start_ts_ns; // create time at ap
};

struct mdw_rv_cmd_func {
	struct mdw_rv_cmd *(*create)(struct mdw_fpriv *mpriv,
		struct mdw_cmd *c);
	int (*delete)(struct mdw_cmd *c);
	void (*done)(struct mdw_rv_cmd *rc, int ret);
};

struct mdw_rv_dev {
	struct rpmsg_device *rpdev;
	struct rpmsg_endpoint *ept;
	struct mdw_device *mdev;

	struct mdw_ipi_param param;

	struct list_head s_list; // for sync msg
	struct mutex msg_mtx;
	struct mutex mtx;

	struct work_struct init_wk; // init wq to avoid ipi conflict

	const struct mdw_rv_cmd_func *cmd_funcs;

	/* rv information */
	uint32_t rv_version;
	unsigned long dev_mask[BITS_TO_LONGS(MDW_DEV_MAX)];
	uint8_t dev_num[MDW_DEV_MAX];
	unsigned long mem_mask[BITS_TO_LONGS(MDW_MEM_TYPE_MAX)];
	struct mdw_mem minfos[MDW_MEM_TYPE_MAX];
	uint8_t meta_data[MDW_DEV_MAX][MDW_DEV_META_SIZE];
	struct mdw_stat *stat;
	uint64_t stat_iova;
};

int mdw_rv_dev_init(struct mdw_device *mdev);
void mdw_rv_dev_deinit(struct mdw_device *mdev);
int mdw_rv_dev_run_cmd(struct mdw_fpriv *mpriv, struct mdw_cmd *c);
int mdw_rv_dev_set_param(struct mdw_rv_dev *mrdev, uint32_t idx, uint32_t val);
int mdw_rv_dev_get_param(struct mdw_rv_dev *mrdev, uint32_t idx, uint32_t *val);

/* rv cmd functions */
extern const struct mdw_rv_cmd_func mdw_rv_cmd_func_v2;
extern const struct mdw_rv_cmd_func mdw_rv_cmd_func_v3;

#endif
