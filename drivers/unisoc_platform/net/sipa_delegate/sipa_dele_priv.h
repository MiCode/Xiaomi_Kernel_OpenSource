/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SIPA_DELE_PRIV_H_
#define _SIPA_DELE_PRIV_H_
#include <linux/sipc.h>
#include <linux/sipa.h>

/* flag for CMD/DONE/EVT msg type */
#define SMSG_FLG_DELE_REQUEST		0x1
#define SMSG_FLG_DELE_RELEASE		0x2
#define SMSG_FLG_DELE_ADDR_DL_TX	0x3
#define SMSG_FLG_DELE_ADDR_DL_RX	0x4
#define SMSG_FLG_DELE_ADDR_UL_TX	0x5
#define SMSG_FLG_DELE_ADDR_UL_RX	0x6
#define SMSG_FLG_DELE_ENABLE		0x7
#define SMSG_FLG_DELE_DISABLE		0x8
#define SMSG_FLG_DELE_ENTER_FLOWCTRL	0x9
#define SMSG_FLG_DELE_EXIT_FLOWCTRL	0x10

#define SMSG_VAL_DELE_REQ_SUCCESS	0x0
#define SMSG_VAL_DELE_REQ_FAIL		0x1

struct sipa_delegate_plat_drv_cfg {
	phys_addr_t mem_base;
	phys_addr_t mem_end;
	phys_addr_t reg_base;
	phys_addr_t reg_end;
	u32 ul_fifo_depth;
	u32 dl_fifo_depth;
	u32 sipa_sys_eb;
};

enum sipa_dele_state {
	SIPA_DELE_ACTIVE,
	SIPA_DELE_REQUESTING,
	SIPA_DELE_RELEASING,
	SIPA_DELE_RELEASED,
	SIPA_DELE_POWER_OFF
};

struct sipa_delegator;

typedef void (*sipa_dele_msg_func)(void *priv, u16 flag, u32 data);

struct sipa_dele_smsg_work_type {
	struct work_struct work;
	struct sipa_delegator *delegator;
	struct smsg msg;
};

struct sipa_delegator {
	struct device *pdev;
	struct sipa_delegate_plat_drv_cfg *cfg;
	enum sipa_rm_res_id prod_id;
	enum sipa_rm_res_id cons_prod;
	enum sipa_rm_res_id cons_user;
	enum sipa_dele_state stat;
	u32 cons_ref_cnt;
	u32 dst;
	u32 chan;
	u32 smsg_cnt;
	bool connected;
	bool is_powered;
	atomic_t requesting_cons;
	bool pd_eb_flag;
	bool pd_get_flag;
	spinlock_t lock;
	struct task_struct *thread;
	struct work_struct notify_work;
	struct workqueue_struct *smsg_wq;
	struct sipa_dele_smsg_work_type req_work;
	struct sipa_dele_smsg_work_type rls_work;
	struct sipa_dele_smsg_work_type done_work;
	struct delayed_work restart_work;
	struct workqueue_struct *restart_wq;

	sipa_dele_msg_func on_open;
	sipa_dele_msg_func on_close;
	sipa_dele_msg_func on_cmd;
	sipa_dele_msg_func on_done;
	sipa_dele_msg_func on_evt;
	int (*local_request_prod)(void *name);
	int (*local_release_prod)(void *name);
	int (*req_res)(void *name);
	int (*rls_res)(void *name);
};

struct miniap_delegator {
	struct sipa_delegator delegator;

	struct sprd_pms *pms;
	char pms_name[20];

	bool ready;

	dma_addr_t ul_free_fifo_phy;
	dma_addr_t ul_filled_fifo_phy;
	dma_addr_t dl_free_fifo_phy;
	dma_addr_t dl_filled_fifo_phy;

	u8 *ul_free_fifo_virt;
	u8 *ul_filled_fifo_virt;
	u8 *dl_free_fifo_virt;
	u8 *dl_filled_fifo_virt;
};

struct ap_delegator {
	struct sipa_delegator delegator;

};

struct cp_delegator {
	struct sipa_delegator delegator;

};

struct sipa_delegator_create_params {
	struct device *pdev;
	struct sipa_delegate_plat_drv_cfg *cfg;
	enum sipa_rm_res_id prod_id;
	enum sipa_rm_res_id cons_prod;
	enum sipa_rm_res_id cons_user;
	u32 dst;
	u32 chan;
};

void sipa_dele_start_done_work(struct sipa_delegator *delegator,
			       u16 flag, u32 val);
void sipa_dele_on_commad(void *priv, u16 flag, u32 data);
int sipa_dele_local_req_r_prod(void *user_data);
int cp_delegator_init(struct sipa_delegator_create_params *params);

int sipa_delegator_init(struct sipa_delegator *delegator,
			struct sipa_delegator_create_params *params);
void sipa_delegator_exit(struct sipa_delegator *delegator);
int sipa_delegator_start(struct sipa_delegator *delegator);

#endif /* !_SIPA_DELE_PRIV_H_ */
