// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/kernel.h>

#include <media/cam_req_mgr.h>
#include "cam_soc_util.h"
#include "cam_smmu_api.h"
#include "cam_cdm_intf_api.h"
#include "cam_cdm.h"
#include "cam_cdm_core_common.h"
#include "cam_cdm_soc.h"
#include "cam_io_util.h"
#include "cam_hw_cdm170_reg.h"

#define CAM_HW_CDM_CPAS_0_NAME "qcom,cam170-cpas-cdm0"
#define CAM_HW_CDM_IPE_0_NAME "qcom,cam170-ipe0-cdm"
#define CAM_HW_CDM_IPE_1_NAME "qcom,cam170-ipe1-cdm"
#define CAM_HW_CDM_BPS_NAME "qcom,cam170-bps-cdm"

#define CAM_CDM_BL_FIFO_WAIT_TIMEOUT 2000

static void cam_hw_cdm_work(struct work_struct *work);

/* DT match table entry for all CDM variants*/
static const struct of_device_id msm_cam_hw_cdm_dt_match[] = {
	{
		.compatible = CAM_HW_CDM_CPAS_0_NAME,
		.data = &cam170_cpas_cdm_offset_table,
	},
	{}
};

static enum cam_cdm_id cam_hw_cdm_get_id_by_name(char *name)
{
	if (!strcmp(CAM_HW_CDM_CPAS_0_NAME, name))
		return CAM_CDM_CPAS_0;

	return CAM_CDM_MAX;
}

int cam_hw_cdm_bl_fifo_pending_bl_rb(struct cam_hw_info *cdm_hw,
	uint32_t *pending_bl)
{
	int rc = 0;

	if (cam_cdm_read_hw_reg(cdm_hw, CDM_BL_FIFO_PENDING_REQ_RB,
		pending_bl)) {
		CAM_ERR(CAM_CDM, "Failed to read CDM pending BL's");
		rc = -EIO;
	}

	return rc;
}

static int cam_hw_cdm_enable_bl_done_irq(struct cam_hw_info *cdm_hw,
	bool enable)
{
	int rc = -EIO;
	uint32_t irq_mask = 0;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;

	if (cam_cdm_read_hw_reg(cdm_hw, CDM_IRQ_MASK,
		&irq_mask)) {
		CAM_ERR(CAM_CDM, "Failed to read CDM IRQ mask");
		return rc;
	}

	if (enable == true) {
		if (cam_cdm_write_hw_reg(cdm_hw, CDM_IRQ_MASK,
			(irq_mask | 0x4))) {
			CAM_ERR(CAM_CDM, "Write failed to enable BL done irq");
		} else {
			atomic_inc(&core->bl_done);
			rc = 0;
			CAM_DBG(CAM_CDM, "BL done irq enabled =%d",
				atomic_read(&core->bl_done));
		}
	} else {
		if (cam_cdm_write_hw_reg(cdm_hw, CDM_IRQ_MASK,
			(irq_mask & 0x70003))) {
			CAM_ERR(CAM_CDM, "Write failed to disable BL done irq");
		} else {
			atomic_dec(&core->bl_done);
			rc = 0;
			CAM_DBG(CAM_CDM, "BL done irq disable =%d",
				atomic_read(&core->bl_done));
		}
	}
	return rc;
}

static int cam_hw_cdm_enable_core(struct cam_hw_info *cdm_hw, bool enable)
{
	int rc = 0;

	if (enable == true) {
		if (cam_cdm_write_hw_reg(cdm_hw, CDM_CFG_CORE_EN, 0x01)) {
			CAM_ERR(CAM_CDM, "Failed to Write CDM HW core enable");
			rc = -EIO;
		}
	} else {
		if (cam_cdm_write_hw_reg(cdm_hw, CDM_CFG_CORE_EN, 0x02)) {
			CAM_ERR(CAM_CDM, "Failed to Write CDM HW core disable");
			rc = -EIO;
		}
	}
	return rc;
}

int cam_hw_cdm_enable_core_dbg(struct cam_hw_info *cdm_hw)
{
	int rc = 0;

	if (cam_cdm_write_hw_reg(cdm_hw, CDM_DBG_CORE_DBUG, 0x10100)) {
		CAM_ERR(CAM_CDM, "Failed to Write CDM HW core debug");
		rc = -EIO;
	}

	return rc;
}

int cam_hw_cdm_disable_core_dbg(struct cam_hw_info *cdm_hw)
{
	int rc = 0;

	if (cam_cdm_write_hw_reg(cdm_hw, CDM_DBG_CORE_DBUG, 0)) {
		CAM_ERR(CAM_CDM, "Failed to Write CDM HW core debug");
		rc = -EIO;
	}

	return rc;
}

void cam_hw_cdm_dump_scratch_registors(struct cam_hw_info *cdm_hw)
{
	uint32_t dump_reg = 0;

	cam_cdm_read_hw_reg(cdm_hw, CDM_CFG_CORE_EN, &dump_reg);
	CAM_ERR(CAM_CDM, "dump core en=%x", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_0_REG, &dump_reg);
	CAM_ERR(CAM_CDM, "dump scratch0=%x", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_1_REG, &dump_reg);
	CAM_ERR(CAM_CDM, "dump scratch1=%x", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_2_REG, &dump_reg);
	CAM_ERR(CAM_CDM, "dump scratch2=%x", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_3_REG, &dump_reg);
	CAM_ERR(CAM_CDM, "dump scratch3=%x", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_4_REG, &dump_reg);
	CAM_ERR(CAM_CDM, "dump scratch4=%x", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_5_REG, &dump_reg);
	CAM_ERR(CAM_CDM, "dump scratch5=%x", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_6_REG, &dump_reg);
	CAM_ERR(CAM_CDM, "dump scratch6=%x", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_7_REG, &dump_reg);
	CAM_ERR(CAM_CDM, "dump scratch7=%x", dump_reg);

}

void cam_hw_cdm_dump_core_debug_registers(
	struct cam_hw_info *cdm_hw)
{
	uint32_t dump_reg, core_dbg, loop_cnt;

	mutex_lock(&cdm_hw->hw_mutex);
	cam_cdm_read_hw_reg(cdm_hw, CDM_CFG_CORE_EN, &dump_reg);
	CAM_ERR(CAM_CDM, "CDM HW core status=%x", dump_reg);
	/* First pause CDM, If it fails still proceed to dump debug info */
	cam_hw_cdm_enable_core(cdm_hw, false);
	cam_hw_cdm_bl_fifo_pending_bl_rb(cdm_hw, &dump_reg);
	CAM_ERR(CAM_CDM, "CDM HW current pending BL=%x", dump_reg);
	loop_cnt = dump_reg;
	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_DEBUG_STATUS, &dump_reg);
	CAM_ERR(CAM_CDM, "CDM HW Debug status reg=%x", dump_reg);
	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_CORE_DBUG, &core_dbg);
	if (core_dbg & 0x100) {
		cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_LAST_AHB_ADDR, &dump_reg);
		CAM_ERR(CAM_CDM, "AHB dump reglastaddr=%x", dump_reg);
		cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_LAST_AHB_DATA, &dump_reg);
		CAM_ERR(CAM_CDM, "AHB dump reglastdata=%x", dump_reg);
	} else {
		CAM_ERR(CAM_CDM, "CDM HW AHB dump not enable");
	}

	if (core_dbg & 0x10000) {
		int i;

		CAM_ERR(CAM_CDM, "CDM HW BL FIFO dump with loop count=%d",
			loop_cnt);
		for (i = 0 ; i < loop_cnt ; i++) {
			cam_cdm_write_hw_reg(cdm_hw, CDM_BL_FIFO_RB, i);
			cam_cdm_read_hw_reg(cdm_hw, CDM_BL_FIFO_BASE_RB,
				&dump_reg);
			CAM_ERR(CAM_CDM, "BL(%d) base addr =%x", i, dump_reg);
			cam_cdm_read_hw_reg(cdm_hw, CDM_BL_FIFO_LEN_RB,
				&dump_reg);
			CAM_ERR(CAM_CDM, "BL(%d) len=%d tag=%d", i,
				(dump_reg & 0xFFFFF), (dump_reg & 0xFF000000));
		}
	} else {
		CAM_ERR(CAM_CDM, "CDM HW BL FIFO readback not enable");
	}

	CAM_ERR(CAM_CDM, "CDM HW default dump");
	cam_cdm_read_hw_reg(cdm_hw, CDM_CFG_CORE_CFG, &dump_reg);
	CAM_ERR(CAM_CDM, "CDM HW core cfg=%x", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_IRQ_STATUS, &dump_reg);
	CAM_ERR(CAM_CDM, "CDM HW irq status=%x", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_IRQ_SET, &dump_reg);
	CAM_ERR(CAM_CDM, "CDM HW irq set reg=%x", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_CURRENT_BL_BASE, &dump_reg);
	CAM_ERR(CAM_CDM, "CDM HW current BL base=%x", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_CURRENT_BL_LEN, &dump_reg);
	CAM_ERR(CAM_CDM, "CDM HW current BL len=%d tag=%d",
		(dump_reg & 0xFFFFF), (dump_reg & 0xFF000000));

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_CURRENT_USED_AHB_BASE, &dump_reg);
	CAM_ERR(CAM_CDM, "CDM HW current AHB base=%x", dump_reg);

	cam_hw_cdm_bl_fifo_pending_bl_rb(cdm_hw, &dump_reg);
	CAM_ERR(CAM_CDM, "CDM HW current pending BL=%x", dump_reg);

	/* Enable CDM back */
	cam_hw_cdm_enable_core(cdm_hw, true);
	mutex_unlock(&cdm_hw->hw_mutex);

}

int cam_hw_cdm_wait_for_bl_fifo(struct cam_hw_info *cdm_hw,
	uint32_t bl_count)
{
	uint32_t pending_bl = 0;
	int32_t available_bl_slots = 0;
	int rc = -EIO;
	long time_left;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;

	do {
		if (cam_cdm_read_hw_reg(cdm_hw, CDM_BL_FIFO_PENDING_REQ_RB,
			&pending_bl)) {
			CAM_ERR(CAM_CDM, "Failed to read CDM pending BL's");
			rc = -EIO;
			break;
		}
		available_bl_slots = CAM_CDM_HWFIFO_SIZE - pending_bl;
		if (available_bl_slots < 0) {
			CAM_ERR(CAM_CDM, "Invalid available slots %d:%d:%d",
				available_bl_slots, CAM_CDM_HWFIFO_SIZE,
				pending_bl);
			break;
		}
		if (bl_count < (available_bl_slots - 1)) {
			CAM_DBG(CAM_CDM,
				"BL slot available_cnt=%d requested=%d",
				(available_bl_slots - 1), bl_count);
				rc = bl_count;
				break;
		} else if (0 == (available_bl_slots - 1)) {
			rc = cam_hw_cdm_enable_bl_done_irq(cdm_hw, true);
			if (rc) {
				CAM_ERR(CAM_CDM, "Enable BL done irq failed");
				break;
			}
			time_left = wait_for_completion_timeout(
				&core->bl_complete, msecs_to_jiffies(
				CAM_CDM_BL_FIFO_WAIT_TIMEOUT));
			if (time_left <= 0) {
				CAM_ERR(CAM_CDM,
					"CDM HW BL Wait timed out failed");
				if (cam_hw_cdm_enable_bl_done_irq(cdm_hw,
					false))
					CAM_ERR(CAM_CDM,
						"Disable BL done irq failed");
				rc = -EIO;
				break;
			}
			if (cam_hw_cdm_enable_bl_done_irq(cdm_hw, false))
				CAM_ERR(CAM_CDM, "Disable BL done irq failed");
			rc = 0;
			CAM_DBG(CAM_CDM, "CDM HW is ready for data");
		} else {
			rc = (bl_count - (available_bl_slots - 1));
			break;
		}
	} while (1);

	return rc;
}

bool cam_hw_cdm_bl_write(struct cam_hw_info *cdm_hw, uint32_t src,
	uint32_t len, uint32_t tag)
{
	if (cam_cdm_write_hw_reg(cdm_hw, CDM_BL_FIFO_BASE_REG, src)) {
		CAM_ERR(CAM_CDM, "Failed to write CDM base to BL base");
		return true;
	}
	if (cam_cdm_write_hw_reg(cdm_hw, CDM_BL_FIFO_LEN_REG,
		((len & 0xFFFFF) | ((tag & 0xFF) << 20)))) {
		CAM_ERR(CAM_CDM, "Failed to write CDM BL len");
		return true;
	}
	return false;
}

bool cam_hw_cdm_commit_bl_write(struct cam_hw_info *cdm_hw)
{
	if (cam_cdm_write_hw_reg(cdm_hw, CDM_BL_FIFO_STORE_REG, 1)) {
		CAM_ERR(CAM_CDM, "Failed to write CDM commit BL");
		return true;
	}
	return false;
}

int cam_hw_cdm_submit_gen_irq(struct cam_hw_info *cdm_hw,
	struct cam_cdm_hw_intf_cmd_submit_bl *req)
{
	struct cam_cdm_bl_cb_request_entry *node;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;
	uint32_t len;
	int rc;

	if (core->bl_tag > 63) {
		CAM_ERR(CAM_CDM, "bl_tag invalid =%d", core->bl_tag);
		rc = -EINVAL;
		goto end;
	}
	CAM_DBG(CAM_CDM, "CDM write BL last cmd tag=%x total=%d cookie=%d",
		core->bl_tag, req->data->cmd_arrary_count, req->data->cookie);
	node = kzalloc(sizeof(struct cam_cdm_bl_cb_request_entry),
			GFP_KERNEL);
	if (!node) {
		rc = -ENOMEM;
		goto end;
	}
	node->request_type = CAM_HW_CDM_BL_CB_CLIENT;
	node->client_hdl = req->handle;
	node->cookie = req->data->cookie;
	node->bl_tag = core->bl_tag;
	node->userdata = req->data->userdata;
	list_add_tail(&node->entry, &core->bl_request_list);
	len = core->ops->cdm_required_size_genirq() * core->bl_tag;
	core->ops->cdm_write_genirq(((uint32_t *)core->gen_irq.kmdvaddr + len),
		core->bl_tag);
	rc = cam_hw_cdm_bl_write(cdm_hw, (core->gen_irq.vaddr + (4*len)),
		((4 * core->ops->cdm_required_size_genirq()) - 1),
		core->bl_tag);
	if (rc) {
		CAM_ERR(CAM_CDM, "CDM hw bl write failed for gen irq bltag=%d",
			core->bl_tag);
		list_del_init(&node->entry);
		kfree(node);
		rc = -EIO;
		goto end;
	}

	if (cam_hw_cdm_commit_bl_write(cdm_hw)) {
		CAM_ERR(CAM_CDM, "Cannot commit the genirq BL with tag tag=%d",
			core->bl_tag);
		list_del_init(&node->entry);
		kfree(node);
		rc = -EIO;
	}

end:
	return rc;
}

int cam_hw_cdm_submit_bl(struct cam_hw_info *cdm_hw,
	struct cam_cdm_hw_intf_cmd_submit_bl *req,
	struct cam_cdm_client *client)
{
	int i, rc;
	struct cam_cdm_bl_request *cdm_cmd = req->data;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;
	uint32_t pending_bl = 0;
	int write_count = 0;

	if (req->data->cmd_arrary_count > CAM_CDM_HWFIFO_SIZE) {
		pr_info("requested BL more than max size, cnt=%d max=%d",
			req->data->cmd_arrary_count, CAM_CDM_HWFIFO_SIZE);
	}

	if (atomic_read(&core->error))
		return -EIO;

	mutex_lock(&cdm_hw->hw_mutex);
	mutex_lock(&client->lock);
	rc = cam_hw_cdm_bl_fifo_pending_bl_rb(cdm_hw, &pending_bl);
	if (rc) {
		CAM_ERR(CAM_CDM, "Cannot read the current BL depth");
		mutex_unlock(&client->lock);
		mutex_unlock(&cdm_hw->hw_mutex);
		return rc;
	}

	for (i = 0; i < req->data->cmd_arrary_count ; i++) {
		dma_addr_t hw_vaddr_ptr = 0;
		size_t len = 0;

		if ((!cdm_cmd->cmd[i].len) &&
			(cdm_cmd->cmd[i].len > 0x100000)) {
			CAM_ERR(CAM_CDM,
				"cmd len(%d) is invalid cnt=%d total cnt=%d",
				cdm_cmd->cmd[i].len, i,
				req->data->cmd_arrary_count);
			rc = -EINVAL;
			break;
		}
		if (atomic_read(&core->error)) {
			CAM_ERR_RATE_LIMIT(CAM_CDM,
				"In error state cnt=%d total cnt=%d\n",
				i, req->data->cmd_arrary_count);
			rc = -EIO;
			break;
		}
		if (write_count == 0) {
			write_count = cam_hw_cdm_wait_for_bl_fifo(cdm_hw,
				(req->data->cmd_arrary_count - i));
			if (write_count < 0) {
				CAM_ERR(CAM_CDM,
					"wait for bl fifo failed %d:%d",
					i, req->data->cmd_arrary_count);
				rc = -EIO;
				break;
			}
		} else {
			write_count--;
		}

		if (req->data->type == CAM_CDM_BL_CMD_TYPE_MEM_HANDLE) {
			rc = cam_mem_get_io_buf(
				cdm_cmd->cmd[i].bl_addr.mem_handle,
				core->iommu_hdl.non_secure, &hw_vaddr_ptr,
				&len);
		} else if (req->data->type == CAM_CDM_BL_CMD_TYPE_HW_IOVA) {
			if (!cdm_cmd->cmd[i].bl_addr.hw_iova) {
				CAM_ERR(CAM_CDM,
					"Hw bl hw_iova is invalid %d:%d",
					i, req->data->cmd_arrary_count);
				rc = -EINVAL;
				break;
			}
			rc = 0;
			hw_vaddr_ptr =
				(uint64_t)cdm_cmd->cmd[i].bl_addr.hw_iova;
			len = cdm_cmd->cmd[i].len + cdm_cmd->cmd[i].offset;
		} else {
			CAM_ERR(CAM_CDM,
				"Only mem hdl/hw va type is supported %d",
				req->data->type);
			rc = -EINVAL;
			break;
		}

		if ((!rc) && (hw_vaddr_ptr) && (len) &&
			(len >= cdm_cmd->cmd[i].offset)) {

			if ((len - cdm_cmd->cmd[i].offset) <
				cdm_cmd->cmd[i].len) {
				CAM_ERR(CAM_CDM,
					"Not enough buffer cmd offset: %u cmd length: %u",
					cdm_cmd->cmd[i].offset,
					cdm_cmd->cmd[i].len);
				rc = -EINVAL;
				break;
			}

			CAM_DBG(CAM_CDM, "Got the HW VA");
			if (core->bl_tag >=
				(CAM_CDM_HWFIFO_SIZE - 1))
				core->bl_tag = 0;
			rc = cam_hw_cdm_bl_write(cdm_hw,
				((uint32_t)hw_vaddr_ptr +
					cdm_cmd->cmd[i].offset),
				(cdm_cmd->cmd[i].len - 1), core->bl_tag);
			if (rc) {
				CAM_ERR(CAM_CDM, "Hw bl write failed %d:%d",
					i, req->data->cmd_arrary_count);
				rc = -EIO;
				break;
			}
		} else {
			CAM_ERR(CAM_CDM,
				"Sanity check failed for hdl=%x len=%zu:%d",
				cdm_cmd->cmd[i].bl_addr.mem_handle, len,
				cdm_cmd->cmd[i].offset);
			CAM_ERR(CAM_CDM, "Sanity check failed for %d:%d",
				i, req->data->cmd_arrary_count);
			rc = -EINVAL;
			break;
		}

		if (!rc) {
			CAM_DBG(CAM_CDM,
				"write BL success for cnt=%d with tag=%d total_cnt=%d",
				i, core->bl_tag, req->data->cmd_arrary_count);

			CAM_DBG(CAM_CDM, "Now commit the BL");
			if (cam_hw_cdm_commit_bl_write(cdm_hw)) {
				CAM_ERR(CAM_CDM,
					"Cannot commit the BL %d tag=%d",
					i, core->bl_tag);
				rc = -EIO;
				break;
			}
			CAM_DBG(CAM_CDM, "BL commit success BL %d tag=%d", i,
				core->bl_tag);
			core->bl_tag++;
			if ((req->data->flag == true) &&
				(i == (req->data->cmd_arrary_count -
				1))) {
				rc = cam_hw_cdm_submit_gen_irq(
					cdm_hw, req);
				if (rc == 0)
					core->bl_tag++;
			}
		}
	}
	mutex_unlock(&client->lock);
	mutex_unlock(&cdm_hw->hw_mutex);
	return rc;

}

static void cam_hw_cdm_work(struct work_struct *work)
{
	struct cam_cdm_work_payload *payload;
	struct cam_hw_info *cdm_hw;
	struct cam_cdm *core;

	payload = container_of(work, struct cam_cdm_work_payload, work);
	if (payload) {
		cdm_hw = payload->hw;
		core = (struct cam_cdm *)cdm_hw->core_info;

		CAM_DBG(CAM_CDM, "IRQ status=0x%x", payload->irq_status);
		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_INFO_INLINE_IRQ_MASK) {
			struct cam_cdm_bl_cb_request_entry *node, *tnode;

			CAM_DBG(CAM_CDM, "inline IRQ data=0x%x",
				payload->irq_data);
			mutex_lock(&cdm_hw->hw_mutex);
			list_for_each_entry_safe(node, tnode,
					&core->bl_request_list, entry) {
				if (node->request_type ==
					CAM_HW_CDM_BL_CB_CLIENT) {
					cam_cdm_notify_clients(cdm_hw,
						CAM_CDM_CB_STATUS_BL_SUCCESS,
						(void *)node);
				} else if (node->request_type ==
					CAM_HW_CDM_BL_CB_INTERNAL) {
					CAM_ERR(CAM_CDM,
						"Invalid node=%pK %d", node,
						node->request_type);
				}
				list_del_init(&node->entry);
				if (node->bl_tag == payload->irq_data) {
					kfree(node);
					break;
				}
				kfree(node);
			}
			mutex_unlock(&cdm_hw->hw_mutex);
		}

		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_INFO_RST_DONE_MASK) {
			CAM_DBG(CAM_CDM, "CDM HW reset done IRQ");
			complete(&core->reset_complete);
		}
		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_INFO_BL_DONE_MASK) {
			if (atomic_read(&core->bl_done)) {
				CAM_DBG(CAM_CDM, "CDM HW BL done IRQ");
				complete(&core->bl_complete);
			}
		}
		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_ERROR_INV_CMD_MASK) {
			CAM_ERR_RATE_LIMIT(CAM_CDM,
				"Invalid command IRQ, Need HW reset\n");
			atomic_inc(&core->error);
			cam_hw_cdm_dump_core_debug_registers(cdm_hw);
		}
		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_ERROR_AHB_BUS_MASK) {
			CAM_ERR_RATE_LIMIT(CAM_CDM, "AHB Error IRQ\n");
			atomic_inc(&core->error);
			cam_hw_cdm_dump_core_debug_registers(cdm_hw);
			atomic_dec(&core->error);
		}
		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_ERROR_OVER_FLOW_MASK) {
			CAM_ERR_RATE_LIMIT(CAM_CDM, "Overflow Error IRQ\n");
			atomic_inc(&core->error);
			cam_hw_cdm_dump_core_debug_registers(cdm_hw);
			atomic_dec(&core->error);
		}
		kfree(payload);
	} else {
		CAM_ERR(CAM_CDM, "NULL payload");
	}

}

static void cam_hw_cdm_iommu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long iova, int flags, void *token,
	uint32_t buf_info)
{
	struct cam_hw_info *cdm_hw = NULL;
	struct cam_cdm *core = NULL;

	if (token) {
		cdm_hw = (struct cam_hw_info *)token;
		core = (struct cam_cdm *)cdm_hw->core_info;
		atomic_inc(&core->error);
		cam_hw_cdm_dump_core_debug_registers(cdm_hw);
		CAM_ERR_RATE_LIMIT(CAM_CDM, "Page fault iova addr %pK\n",
			(void *)iova);
		cam_cdm_notify_clients(cdm_hw, CAM_CDM_CB_STATUS_PAGEFAULT,
			(void *)iova);
		atomic_dec(&core->error);
	} else {
		CAM_ERR(CAM_CDM, "Invalid token");
	}

}

irqreturn_t cam_hw_cdm_irq(int irq_num, void *data)
{
	struct cam_hw_info *cdm_hw = data;
	struct cam_cdm *cdm_core = cdm_hw->core_info;
	struct cam_cdm_work_payload *payload;
	bool work_status;

	CAM_DBG(CAM_CDM, "Got irq");
	payload = kzalloc(sizeof(struct cam_cdm_work_payload), GFP_ATOMIC);
	if (payload) {
		if (cam_cdm_read_hw_reg(cdm_hw, CDM_IRQ_STATUS,
				&payload->irq_status)) {
			CAM_ERR(CAM_CDM, "Failed to read CDM HW IRQ status");
		}
		if (!payload->irq_status) {
			CAM_ERR_RATE_LIMIT(CAM_CDM, "Invalid irq received\n");
			kfree(payload);
			return IRQ_HANDLED;
		}
		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_INFO_INLINE_IRQ_MASK) {
			if (cam_cdm_read_hw_reg(cdm_hw, CDM_IRQ_USR_DATA,
				&payload->irq_data)) {
				CAM_ERR(CAM_CDM,
					"Failed to read CDM HW IRQ data");
			}
		}
		CAM_DBG(CAM_CDM, "Got payload=%d", payload->irq_status);
		payload->hw = cdm_hw;
		INIT_WORK((struct work_struct *)&payload->work,
			cam_hw_cdm_work);
		if (cam_cdm_write_hw_reg(cdm_hw, CDM_IRQ_CLEAR,
			payload->irq_status))
			CAM_ERR(CAM_CDM, "Failed to Write CDM HW IRQ Clear");
		if (cam_cdm_write_hw_reg(cdm_hw, CDM_IRQ_CLEAR_CMD, 0x01))
			CAM_ERR(CAM_CDM, "Failed to Write CDM HW IRQ cmd");
		work_status = queue_work(cdm_core->work_queue, &payload->work);
		if (work_status == false) {
			CAM_ERR(CAM_CDM, "Failed to queue work for irq=0x%x",
				payload->irq_status);
			kfree(payload);
		}
	}

	return IRQ_HANDLED;
}

int cam_hw_cdm_alloc_genirq_mem(void *hw_priv)
{
	struct cam_hw_info *cdm_hw = hw_priv;
	struct cam_mem_mgr_request_desc genirq_alloc_cmd;
	struct cam_mem_mgr_memory_desc genirq_alloc_out;
	struct cam_cdm *cdm_core = NULL;
	int rc =  -EINVAL;

	if (!hw_priv)
		return rc;

	cdm_core = (struct cam_cdm *)cdm_hw->core_info;
	genirq_alloc_cmd.align = 0;
	genirq_alloc_cmd.size = (8 * CAM_CDM_HWFIFO_SIZE);
	genirq_alloc_cmd.smmu_hdl = cdm_core->iommu_hdl.non_secure;
	genirq_alloc_cmd.flags = CAM_MEM_FLAG_HW_READ_WRITE;
	rc = cam_mem_mgr_request_mem(&genirq_alloc_cmd,
		&genirq_alloc_out);
	if (rc) {
		CAM_ERR(CAM_CDM, "Failed to get genirq cmd space rc=%d", rc);
		goto end;
	}
	cdm_core->gen_irq.handle = genirq_alloc_out.mem_handle;
	cdm_core->gen_irq.vaddr = (genirq_alloc_out.iova & 0xFFFFFFFF);
	cdm_core->gen_irq.kmdvaddr = genirq_alloc_out.kva;
	cdm_core->gen_irq.size = genirq_alloc_out.len;

end:
	return rc;
}

int cam_hw_cdm_release_genirq_mem(void *hw_priv)
{
	struct cam_hw_info *cdm_hw = hw_priv;
	struct cam_cdm *cdm_core = NULL;
	struct cam_mem_mgr_memory_desc genirq_release_cmd;
	int rc =  -EINVAL;

	if (!hw_priv)
		return rc;

	cdm_core = (struct cam_cdm *)cdm_hw->core_info;
	genirq_release_cmd.mem_handle = cdm_core->gen_irq.handle;
	rc = cam_mem_mgr_release_mem(&genirq_release_cmd);
	if (rc)
		CAM_ERR(CAM_CDM, "Failed to put genirq cmd space for hw");

	return rc;
}

int cam_hw_cdm_init(void *hw_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *cdm_hw = hw_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_cdm *cdm_core = NULL;
	int rc;
	long time_left;

	if (!hw_priv)
		return -EINVAL;

	soc_info = &cdm_hw->soc_info;
	cdm_core = (struct cam_cdm *)cdm_hw->core_info;

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_SVS_VOTE, true);
	if (rc) {
		CAM_ERR(CAM_CDM, "Enable platform failed");
		goto end;
	}

	CAM_DBG(CAM_CDM, "Enable soc done");

/* Before triggering the reset to HW, clear the reset complete */
	atomic_set(&cdm_core->error, 0);
	atomic_set(&cdm_core->bl_done, 0);
	reinit_completion(&cdm_core->reset_complete);
	reinit_completion(&cdm_core->bl_complete);

	if (cam_cdm_write_hw_reg(cdm_hw, CDM_IRQ_MASK, 0x70003)) {
		CAM_ERR(CAM_CDM, "Failed to Write CDM HW IRQ mask");
		goto disable_return;
	}
	if (cam_cdm_write_hw_reg(cdm_hw, CDM_CFG_RST_CMD, 0x9)) {
		CAM_ERR(CAM_CDM, "Failed to Write CDM HW reset");
		goto disable_return;
	}

	CAM_DBG(CAM_CDM, "Waiting for CDM HW resetdone");
	time_left = wait_for_completion_timeout(&cdm_core->reset_complete,
		msecs_to_jiffies(CAM_CDM_HW_RESET_TIMEOUT));

	if (time_left <= 0) {
		CAM_ERR(CAM_CDM, "CDM HW reset Wait failed rc=%d", rc);
		goto disable_return;
	} else {
		CAM_DBG(CAM_CDM, "CDM Init success");
		cdm_hw->hw_state = CAM_HW_STATE_POWER_UP;
		cam_cdm_write_hw_reg(cdm_hw, CDM_IRQ_MASK, 0x70003);
		rc = 0;
		goto end;
	}

disable_return:
	rc = -EIO;
	cam_soc_util_disable_platform_resource(soc_info, true, true);
end:
	return rc;
}

int cam_hw_cdm_deinit(void *hw_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *cdm_hw = hw_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_cdm *cdm_core = NULL;
	int rc = 0;

	if (!hw_priv)
		return -EINVAL;

	soc_info = &cdm_hw->soc_info;
	cdm_core = cdm_hw->core_info;
	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc) {
		CAM_ERR(CAM_CDM, "disable platform failed");
	} else {
		CAM_DBG(CAM_CDM, "CDM Deinit success");
		cdm_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	}

	return rc;
}

int cam_hw_cdm_probe(struct platform_device *pdev)
{
	int rc;
	struct cam_hw_info *cdm_hw = NULL;
	struct cam_hw_intf *cdm_hw_intf = NULL;
	struct cam_cdm *cdm_core = NULL;
	struct cam_cdm_private_dt_data *soc_private = NULL;
	struct cam_cpas_register_params cpas_parms;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote = {0};

	cdm_hw_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!cdm_hw_intf)
		return -ENOMEM;

	cdm_hw = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!cdm_hw) {
		kfree(cdm_hw_intf);
		return -ENOMEM;
	}

	cdm_hw->core_info = kzalloc(sizeof(struct cam_cdm), GFP_KERNEL);
	if (!cdm_hw->core_info) {
		kfree(cdm_hw);
		kfree(cdm_hw_intf);
		return -ENOMEM;
	}

	cdm_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	cdm_hw->soc_info.pdev = pdev;
	cdm_hw->soc_info.dev = &pdev->dev;
	cdm_hw->soc_info.dev_name = pdev->name;
	cdm_hw_intf->hw_type = CAM_HW_CDM;
	cdm_hw->open_count = 0;
	mutex_init(&cdm_hw->hw_mutex);
	spin_lock_init(&cdm_hw->hw_lock);
	init_completion(&cdm_hw->hw_complete);

	rc = cam_hw_cdm_soc_get_dt_properties(cdm_hw, msm_cam_hw_cdm_dt_match);
	if (rc) {
		CAM_ERR(CAM_CDM, "Failed to get dt properties");
		goto release_mem;
	}
	cdm_hw_intf->hw_idx = cdm_hw->soc_info.index;
	cdm_core = (struct cam_cdm *)cdm_hw->core_info;
	soc_private = (struct cam_cdm_private_dt_data *)
		cdm_hw->soc_info.soc_private;
	if (soc_private->dt_cdm_shared == true)
		cdm_core->flags = CAM_CDM_FLAG_SHARED_CDM;
	else
		cdm_core->flags = CAM_CDM_FLAG_PRIVATE_CDM;

	cdm_core->bl_tag = 0;
	cdm_core->id = cam_hw_cdm_get_id_by_name(cdm_core->name);
	if (cdm_core->id >= CAM_CDM_MAX) {
		CAM_ERR(CAM_CDM, "Failed to get CDM HW name for %s",
			cdm_core->name);
		goto release_private_mem;
	}
	INIT_LIST_HEAD(&cdm_core->bl_request_list);
	init_completion(&cdm_core->reset_complete);
	init_completion(&cdm_core->bl_complete);
	cdm_hw_intf->hw_priv = cdm_hw;
	cdm_hw_intf->hw_ops.get_hw_caps = cam_cdm_get_caps;
	cdm_hw_intf->hw_ops.init = cam_hw_cdm_init;
	cdm_hw_intf->hw_ops.deinit = cam_hw_cdm_deinit;
	cdm_hw_intf->hw_ops.start = cam_cdm_stream_start;
	cdm_hw_intf->hw_ops.stop = cam_cdm_stream_stop;
	cdm_hw_intf->hw_ops.read = NULL;
	cdm_hw_intf->hw_ops.write = NULL;
	cdm_hw_intf->hw_ops.process_cmd = cam_cdm_process_cmd;
	mutex_lock(&cdm_hw->hw_mutex);

	CAM_DBG(CAM_CDM, "type %d index %d", cdm_hw_intf->hw_type,
		cdm_hw_intf->hw_idx);

	platform_set_drvdata(pdev, cdm_hw_intf);

	rc = cam_smmu_get_handle("cpas-cdm0", &cdm_core->iommu_hdl.non_secure);
	if (rc < 0) {
		CAM_ERR(CAM_CDM, "cpas-cdm get iommu handle failed");
		goto unlock_release_mem;
	}
	cam_smmu_set_client_page_fault_handler(cdm_core->iommu_hdl.non_secure,
		cam_hw_cdm_iommu_fault_handler, cdm_hw);

	cdm_core->iommu_hdl.secure = -1;

	cdm_core->work_queue = alloc_workqueue(cdm_core->name,
		WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS,
		CAM_CDM_INFLIGHT_WORKS);

	rc = cam_soc_util_request_platform_resource(&cdm_hw->soc_info,
			cam_hw_cdm_irq, cdm_hw);
	if (rc) {
		CAM_ERR(CAM_CDM, "Failed to request platform resource");
		goto destroy_non_secure_hdl;
	}

	cpas_parms.cam_cpas_client_cb = cam_cdm_cpas_cb;
	cpas_parms.cell_index = cdm_hw->soc_info.index;
	cpas_parms.dev = &pdev->dev;
	cpas_parms.userdata = cdm_hw_intf;
	strlcpy(cpas_parms.identifier, "cpas-cdm", CAM_HW_IDENTIFIER_LENGTH);
	rc = cam_cpas_register_client(&cpas_parms);
	if (rc) {
		CAM_ERR(CAM_CDM, "Virtual CDM CPAS registration failed");
		goto release_platform_resource;
	}
	CAM_DBG(CAM_CDM, "CPAS registration successful handle=%d",
		cpas_parms.client_handle);
	cdm_core->cpas_handle = cpas_parms.client_handle;

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_LOWSVS_VOTE;
	axi_vote.num_paths = 1;
	axi_vote.axi_path[0].path_data_type = CAM_AXI_PATH_DATA_ALL;
	axi_vote.axi_path[0].transac_type = CAM_AXI_TRANSACTION_READ;
	axi_vote.axi_path[0].camnoc_bw = CAM_CPAS_DEFAULT_AXI_BW;
	axi_vote.axi_path[0].mnoc_ab_bw = CAM_CPAS_DEFAULT_AXI_BW;
	axi_vote.axi_path[0].mnoc_ib_bw = CAM_CPAS_DEFAULT_AXI_BW;

	rc = cam_cpas_start(cdm_core->cpas_handle, &ahb_vote, &axi_vote);
	if (rc) {
		CAM_ERR(CAM_CDM, "CPAS start failed");
		goto cpas_unregister;
	}

	rc = cam_hw_cdm_init(cdm_hw, NULL, 0);
	if (rc) {
		CAM_ERR(CAM_CDM, "Failed to Init CDM HW");
		goto cpas_stop;
	}
	cdm_hw->open_count++;

	if (cam_cdm_read_hw_reg(cdm_hw, CDM_CFG_HW_VERSION,
		&cdm_core->hw_version)) {
		CAM_ERR(CAM_CDM, "Failed to read CDM HW Version");
		goto deinit;
	}

	if (cam_cdm_read_hw_reg(cdm_hw, CDM_CFG_TITAN_VERSION,
		&cdm_core->hw_family_version)) {
		CAM_ERR(CAM_CDM, "Failed to read CDM family Version");
		goto deinit;
	}

	CAM_DBG(CAM_CDM, "CDM Hw version read success family =%x hw =%x",
		cdm_core->hw_family_version, cdm_core->hw_version);
	cdm_core->ops = cam_cdm_get_ops(cdm_core->hw_version, NULL,
		false);
	if (!cdm_core->ops) {
		CAM_ERR(CAM_CDM, "Failed to util ops for hw");
		goto deinit;
	}

	if (!cam_cdm_set_cam_hw_version(cdm_core->hw_version,
		&cdm_core->version)) {
		CAM_ERR(CAM_CDM, "Failed to set cam he version for hw");
		goto deinit;
	}

	rc = cam_hw_cdm_deinit(cdm_hw, NULL, 0);
	if (rc) {
		CAM_ERR(CAM_CDM, "Failed to Deinit CDM HW");
		cdm_hw->open_count--;
		goto cpas_stop;
	}

	rc = cam_cpas_stop(cdm_core->cpas_handle);
	if (rc) {
		CAM_ERR(CAM_CDM, "CPAS stop failed");
		cdm_hw->open_count--;
		goto cpas_unregister;
	}

	rc = cam_cdm_intf_register_hw_cdm(cdm_hw_intf,
		soc_private, CAM_HW_CDM, &cdm_core->index);
	if (rc) {
		CAM_ERR(CAM_CDM, "HW CDM Interface registration failed");
		cdm_hw->open_count--;
		goto cpas_unregister;
	}
	cdm_hw->open_count--;
	mutex_unlock(&cdm_hw->hw_mutex);

	CAM_DBG(CAM_CDM, "CDM%d probe successful", cdm_hw_intf->hw_idx);

	return rc;

deinit:
	if (cam_hw_cdm_deinit(cdm_hw, NULL, 0))
		CAM_ERR(CAM_CDM, "Deinit failed for hw");
	cdm_hw->open_count--;
cpas_stop:
	if (cam_cpas_stop(cdm_core->cpas_handle))
		CAM_ERR(CAM_CDM, "CPAS stop failed");
cpas_unregister:
	if (cam_cpas_unregister_client(cdm_core->cpas_handle))
		CAM_ERR(CAM_CDM, "CPAS unregister failed");
release_platform_resource:
	if (cam_soc_util_release_platform_resource(&cdm_hw->soc_info))
		CAM_ERR(CAM_CDM, "Release platform resource failed");

	flush_workqueue(cdm_core->work_queue);
	destroy_workqueue(cdm_core->work_queue);
destroy_non_secure_hdl:
	cam_smmu_set_client_page_fault_handler(cdm_core->iommu_hdl.non_secure,
		NULL, cdm_hw);
	if (cam_smmu_destroy_handle(cdm_core->iommu_hdl.non_secure))
		CAM_ERR(CAM_CDM, "Release iommu secure hdl failed");
unlock_release_mem:
	mutex_unlock(&cdm_hw->hw_mutex);
release_private_mem:
	kfree(cdm_hw->soc_info.soc_private);
release_mem:
	mutex_destroy(&cdm_hw->hw_mutex);
	kfree(cdm_hw_intf);
	kfree(cdm_hw->core_info);
	kfree(cdm_hw);
	return rc;
}

int cam_hw_cdm_remove(struct platform_device *pdev)
{
	int rc = -EBUSY;
	struct cam_hw_info *cdm_hw = NULL;
	struct cam_hw_intf *cdm_hw_intf = NULL;
	struct cam_cdm *cdm_core = NULL;

	cdm_hw_intf = platform_get_drvdata(pdev);
	if (!cdm_hw_intf) {
		CAM_ERR(CAM_CDM, "Failed to get dev private data");
		return rc;
	}

	cdm_hw = cdm_hw_intf->hw_priv;
	if (!cdm_hw) {
		CAM_ERR(CAM_CDM,
			"Failed to get hw private data for type=%d idx=%d",
			cdm_hw_intf->hw_type, cdm_hw_intf->hw_idx);
		return rc;
	}

	cdm_core = cdm_hw->core_info;
	if (!cdm_core) {
		CAM_ERR(CAM_CDM,
			"Failed to get hw core data for type=%d idx=%d",
			cdm_hw_intf->hw_type, cdm_hw_intf->hw_idx);
		return rc;
	}

	if (cdm_hw->open_count != 0) {
		CAM_ERR(CAM_CDM, "Hw open count invalid type=%d idx=%d cnt=%d",
			cdm_hw_intf->hw_type, cdm_hw_intf->hw_idx,
			cdm_hw->open_count);
		return rc;
	}

	rc = cam_hw_cdm_deinit(cdm_hw, NULL, 0);
	if (rc) {
		CAM_ERR(CAM_CDM, "Deinit failed for hw");
		return rc;
	}

	rc = cam_cpas_unregister_client(cdm_core->cpas_handle);
	if (rc) {
		CAM_ERR(CAM_CDM, "CPAS unregister failed");
		return rc;
	}

	if (cam_soc_util_release_platform_resource(&cdm_hw->soc_info))
		CAM_ERR(CAM_CDM, "Release platform resource failed");

	flush_workqueue(cdm_core->work_queue);
	destroy_workqueue(cdm_core->work_queue);

	if (cam_smmu_destroy_handle(cdm_core->iommu_hdl.non_secure))
		CAM_ERR(CAM_CDM, "Release iommu secure hdl failed");
	cam_smmu_unset_client_page_fault_handler(
		cdm_core->iommu_hdl.non_secure, cdm_hw);

	mutex_destroy(&cdm_hw->hw_mutex);
	kfree(cdm_hw->soc_info.soc_private);
	kfree(cdm_hw_intf);
	kfree(cdm_hw->core_info);
	kfree(cdm_hw);

	return 0;
}

static struct platform_driver cam_hw_cdm_driver = {
	.probe = cam_hw_cdm_probe,
	.remove = cam_hw_cdm_remove,
	.driver = {
		.name = "msm_cam_cdm",
		.owner = THIS_MODULE,
		.of_match_table = msm_cam_hw_cdm_dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init cam_hw_cdm_init_module(void)
{
	return platform_driver_register(&cam_hw_cdm_driver);
}

static void __exit cam_hw_cdm_exit_module(void)
{
	platform_driver_unregister(&cam_hw_cdm_driver);
}

module_init(cam_hw_cdm_init_module);
module_exit(cam_hw_cdm_exit_module);
MODULE_DESCRIPTION("MSM Camera HW CDM driver");
MODULE_LICENSE("GPL v2");
