/*
 * Copyright (c) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "mtk_mdp_core.h"
#include "mtk_mdp_vpu.h"
#include "mtk_vpu.h"
#include "mtk_mdp_ipi.h"

static inline struct mtk_mdp_ctx *vpu_to_ctx(struct mtk_mdp_vpu *vpu)
{
	return container_of(vpu, struct mtk_mdp_ctx, vpu);
}


static void handle_init_ack_msg(void *data, void *priv)
{
	struct mdp_ipi_init_ack *init_msg = (struct mdp_ipi_init_ack *)data;
	struct mtk_mdp_vpu *vpu = (struct mtk_mdp_vpu *)priv;
	uint32_t vpu_addr;

	vpu_addr =  init_msg->shmem_addr;
	vpu->shmem_va = (void *)vpu_mapping_dm_addr(
		    vpu->pdev, (uint32_t)vpu_addr);
	vpu->param = (struct mdp_process_param *)vpu->shmem_va;
	vpu->failure = init_msg->status;
	pr_debug("mdp handle_init_ack_msg:shmem_va=%x,%x\n", (uint32_t)vpu->shmem_va, vpu_addr);
}

static void handle_deinit_ack_msg(void *priv)
{
	struct mtk_mdp_vpu *vpu = (struct mtk_mdp_vpu *)priv;

	vpu->shmem_va = NULL;
	vpu->param = NULL;
	pr_debug("mdp handle_deinit_ack_msg\n");
}

static void mtk_mdp_vpu_ipi_handler(void *data, unsigned int len, void *priv)
{
	struct mdp_ipi_comm_ack *ack = (struct mdp_ipi_comm_ack *)data;

	if (ack->status == 0) {
		switch (ack->msg_id) {
		case VPU_MDP_INIT_ACK:
			handle_init_ack_msg(data, priv);
			break;
		case VPU_MDP_DEINIT_ACK:
		case VPU_MDP_PROCESS_ACK:
			break;
		default:
			pr_err("invalid msg=%d", ack->msg_id);
			break;
		}
	}
}


static int mtk_mdp_vpu_send_msg(void *msg, int len, struct mtk_mdp_vpu *vpu,
				int id)
{
	int err;

	err = vpu_ipi_send(vpu->pdev, (enum ipi_id)id, msg, len);
	if (err != 0) {
		pr_debug("vpu_ipi_send fail status %d\n", err);
		return -EBUSY;
	}
	return MDP_IPI_MSG_STATUS_OK;
}

int mtk_mdp_vpu_register(struct mtk_mdp_vpu *vpu, struct platform_device *pdev)
{
	struct mtk_mdp_dev *mdp = platform_get_drvdata(pdev);
	int err;

	err = vpu_ipi_register(mdp->vpu_dev, IPI_MDP,
			       mtk_mdp_vpu_ipi_handler, "mdp_vpu", (void *)vpu);
	if (err != 0) {
		pr_debug("vpu_ipi_registration fail status=%d\n",
			err);
		return -EBUSY;
	}
	return 0;
}

int mtk_mdp_vpu_init(struct mtk_mdp_vpu *vpu)
{
	int err;
	struct mdp_ipi_init msg;
	struct mtk_mdp_ctx *ctx = vpu_to_ctx(vpu);

	pr_err("mtk_mdp_vpu_init\n");

	memset(vpu, 0, sizeof(*vpu));
	vpu->pdev = ctx->mdp_dev->vpu_dev;

	msg.msg_id = AP_MDP_INIT;
	msg.ipi_id = IPI_MDP;
	msg.mdp_priv = (uint32_t)vpu;
	err = mtk_mdp_vpu_send_msg((void *)&msg, sizeof(msg), vpu, msg.ipi_id);
	if (!err && vpu->failure != MDP_IPI_MSG_STATUS_OK)
		err = vpu->failure;
	return err;
}

int mtk_mdp_vpu_deinit(struct mtk_mdp_vpu *vpu)
{
	int err;
	struct mdp_ipi_deinit msg;

	msg.msg_id = AP_MDP_DEINIT;
	msg.ipi_id = IPI_MDP;
	/*msg.h_drv = vpu->param->h_drv;*/
	msg.mdp_priv = (uint32_t)vpu;
	err = mtk_mdp_vpu_send_msg((void *)&msg, sizeof(msg), vpu, msg.ipi_id);
	if (!err && vpu->failure != MDP_IPI_MSG_STATUS_OK)
		err = vpu->failure;
	return err;
}

int mtk_mdp_vpu_process(struct mtk_mdp_vpu *vpu)
{
	int err;
	struct mdp_ipi_process msg;

	msg.msg_id = AP_MDP_PROCESS;
	msg.ipi_id = IPI_MDP;
	/*msg.h_drv = vpu->param->h_drv;*/
	msg.mdp_priv = (uint32_t)vpu;
	err = mtk_mdp_vpu_send_msg((void *)&msg, sizeof(msg), vpu, msg.ipi_id);
	if (!err && vpu->failure != MDP_IPI_MSG_STATUS_OK) {
		err = vpu->failure;
		handle_deinit_ack_msg(vpu);
	}
	return err;
}

