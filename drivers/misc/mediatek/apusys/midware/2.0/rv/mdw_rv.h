/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_RV_H__
#define __MTK_APU_MDW_RV_H__

#include "mdw.h"
#include "mdw_msg.h"

struct mdw_rv_dev {
	struct rpmsg_device *rpdev;
	struct rpmsg_endpoint *ept;
	struct mdw_device *mdev;

	struct mdw_ipi_param param;

	struct list_head s_list; // for sync msg
	struct mutex msg_mtx;
	struct mutex mtx;

	struct list_head c_list; // for cmd
	atomic_t clock_flag;
	struct work_struct c_wk; // for re-trigger cmd after unlock

	/* rv information */
	uint32_t rv_version;
	uint64_t dev_bitmask;
	uint8_t dev_num[MDW_DEV_MAX];
	uint8_t meta_data[MDW_DEV_MAX][MDW_DEV_META_SIZE];
};

struct mdw_rv_cmd {
	struct mdw_cmd *c;
	struct mdw_mem *cb;
	struct list_head u_item; // to usr list
	struct list_head d_item; // to dev list
	struct mdw_ipi_msg_sync s_msg; // for ipi
	uint64_t start_ts_ns; // create time at ap
};

struct mdw_rv_cmd_v2 {
	/* cmdinfo */
	uint64_t cmd_uid;
	uint32_t priority;
	uint32_t hardlimit;
	uint32_t softlimit;
	uint32_t power_save;
	uint32_t num_subcmds;
	uint32_t subcmds_offset;
	uint32_t num_cmdbufs;
	uint32_t cmdbufs_offset;
	uint32_t adj_matrix_offset;
} __attribute__((__packed__));

struct mdw_rv_subcmd_v2 {
	/* in */
	uint32_t type;
	uint32_t suggest_time;
	uint32_t vlm_usage;
	uint32_t vlm_ctx_id;
	uint32_t vlm_force;
	uint32_t boost;
	uint32_t pack_id;
	uint32_t cmdbuf_start_idx;
	uint32_t num_cmdbufs;

	/* out */
	uint32_t ip_time;
	uint32_t driver_time;
} __attribute__((__packed__));

struct mdw_rv_cmdbuf_v2 {
	uint64_t device_va;
	uint32_t size;
} __attribute__((__packed__));

int mdw_rv_dev_init(struct mdw_device *mdev);
void mdw_rv_dev_deinit(struct mdw_device *mdev);
struct mdw_rv_dev *mdw_rv_dev_get(void);
int mdw_rv_dev_handshake(void);
int mdw_rv_dev_run_cmd(struct mdw_rv_cmd *rc);
int mdw_rv_dev_lock(void);
int mdw_rv_dev_unlock(void);
int mdw_rv_dev_set_param(uint32_t idx, uint32_t val);
uint32_t mdw_rv_dev_get_param(uint32_t idx);

int mdw_rv_cmd_exec(struct mdw_fpriv *mpriv, struct mdw_cmd *c);
void mdw_rv_cmd_done(struct mdw_rv_cmd *rc, int ret);

#endif
