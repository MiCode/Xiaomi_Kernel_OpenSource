/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_RV_H__
#define __MTK_APU_MDW_RV_H__

#include "mdw.h"
#include "mdw_rv_msg.h"

struct mdw_rv_dev {
	struct rpmsg_device *rpdev;
	struct rpmsg_endpoint *ept;
	struct mdw_device *mdev;

	struct mdw_ipi_param param;

	struct list_head s_list; // for sync msg
	struct mutex msg_mtx;
	struct mutex mtx;

	struct work_struct init_wk; // init wq to avoid ipi conflict

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

struct mdw_rv_cmd {
	struct mdw_cmd *c;
	struct mdw_mem *cb;
	struct list_head u_item; // to usr list
	struct mdw_ipi_msg_sync s_msg; // for ipi
	uint64_t start_ts_ns; // create time at ap
};

struct mdw_rv_msg_cmd {
	/* ids */
	uint64_t session_id;
	uint64_t cmd_id;
	uint32_t pid;
	uint32_t tgid;
	/* params */
	uint32_t priority;
	uint32_t hardlimit;
	uint32_t softlimit;
	uint32_t power_save;
	uint32_t power_plcy;
	uint32_t power_dtime;
	uint32_t app_type;
	uint32_t num_subcmds;
	uint32_t subcmds_offset;
	uint32_t num_cmdbufs;
	uint32_t cmdbuf_infos_offset;
	uint32_t adj_matrix_offset;
	uint32_t exec_infos_offset;
} __attribute__((__packed__));

struct mdw_rv_msg_sc {
	/* params */
	uint32_t type;
	uint32_t suggest_time;
	uint32_t vlm_usage;
	uint32_t vlm_ctx_id;
	uint32_t vlm_force;
	uint32_t boost;
	uint32_t turbo_boost;
	uint32_t min_boost;
	uint32_t max_boost;
	uint32_t hse_en;
	uint32_t driver_time;
	uint32_t ip_time;
	uint32_t bw;
	uint32_t pack_id;
	/* cmdbufs info */
	uint32_t cmdbuf_start_idx;
	uint32_t num_cmdbufs;
} __attribute__((__packed__));

struct mdw_rv_msg_cb {
	uint64_t device_va;
	uint32_t size;
} __attribute__((__packed__));

int mdw_rv_dev_init(struct mdw_device *mdev);
void mdw_rv_dev_deinit(struct mdw_device *mdev);
int mdw_rv_dev_run_cmd(struct mdw_fpriv *mpriv, struct mdw_cmd *c);
int mdw_rv_dev_set_param(struct mdw_rv_dev *mrdev, uint32_t idx, uint32_t val);
int mdw_rv_dev_get_param(struct mdw_rv_dev *mrdev, uint32_t idx, uint32_t *val);

struct mdw_rv_cmd *mdw_rv_cmd_create(struct mdw_fpriv *mpriv,
	struct mdw_cmd *c);
int mdw_rv_cmd_delete(struct mdw_rv_cmd *rc);
void mdw_rv_cmd_done(struct mdw_rv_cmd *rc, int ret);

#endif
