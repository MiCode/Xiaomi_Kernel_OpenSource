/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "CAM-CDM-HW %s:%d " fmt, __func__, __LINE__

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/ion.h>
#include <linux/iommu.h>
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
		pr_err("Failed to read CDM pending BL's\n");
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
		pr_err("Failed to read CDM IRQ mask\n");
		return rc;
	}

	if (enable == true) {
		if (cam_cdm_write_hw_reg(cdm_hw, CDM_IRQ_MASK,
			(irq_mask | 0x4))) {
			pr_err("Write failed to enable BL done irq\n");
		} else {
			atomic_inc(&core->bl_done);
			rc = 0;
			CDM_CDBG("BL done irq enabled =%d\n",
				atomic_read(&core->bl_done));
		}
	} else {
		if (cam_cdm_write_hw_reg(cdm_hw, CDM_IRQ_MASK,
			(irq_mask & 0x70003))) {
			pr_err("Write failed to disable BL done irq\n");
		} else {
			atomic_dec(&core->bl_done);
			rc = 0;
			CDM_CDBG("BL done irq disable =%d\n",
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
			pr_err("Failed to Write CDM HW core enable\n");
			rc = -EIO;
		}
	} else {
		if (cam_cdm_write_hw_reg(cdm_hw, CDM_CFG_CORE_EN, 0x02)) {
			pr_err("Failed to Write CDM HW core disable\n");
			rc = -EIO;
		}
	}
	return rc;
}

int cam_hw_cdm_enable_core_dbg(struct cam_hw_info *cdm_hw)
{
	int rc = 0;

	if (cam_cdm_write_hw_reg(cdm_hw, CDM_DBG_CORE_DBUG, 0x10100)) {
		pr_err("Failed to Write CDM HW core debug\n");
		rc = -EIO;
	}

	return rc;
}

int cam_hw_cdm_disable_core_dbg(struct cam_hw_info *cdm_hw)
{
	int rc = 0;

	if (cam_cdm_write_hw_reg(cdm_hw, CDM_DBG_CORE_DBUG, 0)) {
		pr_err("Failed to Write CDM HW core debug\n");
		rc = -EIO;
	}

	return rc;
}

void cam_hw_cdm_dump_scratch_registors(struct cam_hw_info *cdm_hw)
{
	uint32_t dump_reg = 0;

	cam_cdm_read_hw_reg(cdm_hw, CDM_CFG_CORE_EN, &dump_reg);
	pr_err("dump core en=%x\n", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_0_REG, &dump_reg);
	pr_err("dump scratch0=%x\n", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_1_REG, &dump_reg);
	pr_err("dump scratch1=%x\n", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_2_REG, &dump_reg);
	pr_err("dump scratch2=%x\n", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_3_REG, &dump_reg);
	pr_err("dump scratch3=%x\n", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_4_REG, &dump_reg);
	pr_err("dump scratch4=%x\n", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_5_REG, &dump_reg);
	pr_err("dump scratch5=%x\n", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_6_REG, &dump_reg);
	pr_err("dump scratch6=%x\n", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_SCRATCH_7_REG, &dump_reg);
	pr_err("dump scratch7=%x\n", dump_reg);

}

void cam_hw_cdm_dump_core_debug_registers(
	struct cam_hw_info *cdm_hw)
{
	uint32_t dump_reg, core_dbg, loop_cnt;

	mutex_lock(&cdm_hw->hw_mutex);
	cam_cdm_read_hw_reg(cdm_hw, CDM_CFG_CORE_EN, &dump_reg);
	pr_err("CDM HW core status=%x\n", dump_reg);
	/* First pause CDM, If it fails still proceed to dump debug info */
	cam_hw_cdm_enable_core(cdm_hw, false);
	cam_hw_cdm_bl_fifo_pending_bl_rb(cdm_hw, &dump_reg);
	pr_err("CDM HW current pending BL=%x\n", dump_reg);
	loop_cnt = dump_reg;
	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_DEBUG_STATUS, &dump_reg);
	pr_err("CDM HW Debug status reg=%x\n", dump_reg);
	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_CORE_DBUG, &core_dbg);
	if (core_dbg & 0x100) {
		cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_LAST_AHB_ADDR, &dump_reg);
		pr_err("AHB dump reglastaddr=%x\n", dump_reg);
		cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_LAST_AHB_DATA, &dump_reg);
		pr_err("AHB dump reglastdata=%x\n", dump_reg);
	} else {
		pr_err("CDM HW AHB dump not enable\n");
	}

	if (core_dbg & 0x10000) {
		int i;

		pr_err("CDM HW BL FIFO dump with loop count=%d\n", loop_cnt);
		for (i = 0 ; i < loop_cnt ; i++) {
			cam_cdm_write_hw_reg(cdm_hw, CDM_BL_FIFO_RB, i);
			cam_cdm_read_hw_reg(cdm_hw, CDM_BL_FIFO_BASE_RB,
				&dump_reg);
			pr_err("BL(%d) base addr =%x\n", i, dump_reg);
			cam_cdm_read_hw_reg(cdm_hw, CDM_BL_FIFO_LEN_RB,
				&dump_reg);
			pr_err("BL(%d) len=%d tag=%d\n", i,
				(dump_reg & 0xFFFFF), (dump_reg & 0xFF000000));
		}
	} else {
		pr_err("CDM HW BL FIFO readback not enable\n");
	}

	pr_err("CDM HW default dump\n");
	cam_cdm_read_hw_reg(cdm_hw, CDM_CFG_CORE_CFG, &dump_reg);
	pr_err("CDM HW core cfg=%x\n", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_IRQ_STATUS, &dump_reg);
	pr_err("CDM HW irq status=%x\n", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_IRQ_SET, &dump_reg);
	pr_err("CDM HW irq set reg=%x\n", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_CURRENT_BL_BASE, &dump_reg);
	pr_err("CDM HW current BL base=%x\n", dump_reg);

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_CURRENT_BL_LEN, &dump_reg);
	pr_err("CDM HW current BL len=%d tag=%d\n", (dump_reg & 0xFFFFF),
		(dump_reg & 0xFF000000));

	cam_cdm_read_hw_reg(cdm_hw, CDM_DBG_CURRENT_USED_AHB_BASE, &dump_reg);
	pr_err("CDM HW current AHB base=%x\n", dump_reg);

	cam_hw_cdm_bl_fifo_pending_bl_rb(cdm_hw, &dump_reg);
	pr_err("CDM HW current pending BL=%x\n", dump_reg);

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
			pr_err("Failed to read CDM pending BL's\n");
			rc = -EIO;
			break;
		}
		available_bl_slots = CAM_CDM_HWFIFO_SIZE - pending_bl;
		if (available_bl_slots < 0) {
			pr_err("Invalid available slots %d:%d:%d\n",
				available_bl_slots, CAM_CDM_HWFIFO_SIZE,
				pending_bl);
			break;
		}
		if (bl_count < (available_bl_slots - 1)) {
			CDM_CDBG("BL slot available_cnt=%d requested=%d\n",
				(available_bl_slots - 1), bl_count);
				rc = bl_count;
				break;
		} else if (0 == (available_bl_slots - 1)) {
			rc = cam_hw_cdm_enable_bl_done_irq(cdm_hw, true);
			if (rc) {
				pr_err("Enable BL done irq failed\n");
				break;
			}
			time_left = wait_for_completion_timeout(
				&core->bl_complete, msecs_to_jiffies(
				CAM_CDM_BL_FIFO_WAIT_TIMEOUT));
			if (time_left <= 0) {
				pr_err("CDM HW BL Wait timed out failed\n");
				if (cam_hw_cdm_enable_bl_done_irq(cdm_hw,
					false))
					pr_err("Disable BL done irq failed\n");
				rc = -EIO;
				break;
			}
			if (cam_hw_cdm_enable_bl_done_irq(cdm_hw, false))
				pr_err("Disable BL done irq failed\n");
			rc = 0;
			CDM_CDBG("CDM HW is ready for data\n");
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
		pr_err("Failed to write CDM base to BL base\n");
		return true;
	}
	if (cam_cdm_write_hw_reg(cdm_hw, CDM_BL_FIFO_LEN_REG,
		((len & 0xFFFFF) | ((tag & 0xFF) << 20)))) {
		pr_err("Failed to write CDM BL len\n");
		return true;
	}
	return false;
}

bool cam_hw_cdm_commit_bl_write(struct cam_hw_info *cdm_hw)
{
	if (cam_cdm_write_hw_reg(cdm_hw, CDM_BL_FIFO_STORE_REG, 1)) {
		pr_err("Failed to write CDM commit BL\n");
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
		pr_err("bl_tag invalid =%d\n", core->bl_tag);
		rc = -EINVAL;
		goto end;
	}
	CDM_CDBG("CDM write BL last cmd tag=%x total=%d cookie=%d\n",
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
		pr_err("CDM hw bl write failed for gen irq bltag=%d\n",
			core->bl_tag);
		list_del_init(&node->entry);
		kfree(node);
		rc = -EIO;
		goto end;
	}

	if (cam_hw_cdm_commit_bl_write(cdm_hw)) {
		pr_err("Cannot commit the genirq BL with tag tag=%d\n",
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
		pr_info("requested BL more than max size, cnt=%d max=%d\n",
			req->data->cmd_arrary_count, CAM_CDM_HWFIFO_SIZE);
	}

	if (atomic_read(&core->error))
		return -EIO;

	mutex_lock(&cdm_hw->hw_mutex);
	mutex_lock(&client->lock);
	rc = cam_hw_cdm_bl_fifo_pending_bl_rb(cdm_hw, &pending_bl);
	if (rc) {
		pr_err("Cannot read the current BL depth\n");
		mutex_unlock(&client->lock);
		mutex_unlock(&cdm_hw->hw_mutex);
		return rc;
	}

	for (i = 0; i < req->data->cmd_arrary_count ; i++) {
		uint64_t hw_vaddr_ptr = 0;
		size_t len = 0;

		if ((!cdm_cmd->cmd[i].len) &&
			(cdm_cmd->cmd[i].len > 0x100000)) {
			pr_err("cmd len(%d) is invalid cnt=%d total cnt=%d\n",
				cdm_cmd->cmd[i].len, i,
				req->data->cmd_arrary_count);
			rc = -EINVAL;
			break;
		}
		if (atomic_read(&core->error)) {
			pr_err_ratelimited("In error state cnt=%d total cnt=%d\n",
				i, req->data->cmd_arrary_count);
			rc = -EIO;
			break;
		}
		if (write_count == 0) {
			write_count = cam_hw_cdm_wait_for_bl_fifo(cdm_hw,
				(req->data->cmd_arrary_count - i));
			if (write_count < 0) {
				pr_err("wait for bl fifo failed %d:%d\n",
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
				pr_err("Hw bl hw_iova is invalid %d:%d\n",
					i, req->data->cmd_arrary_count);
				rc = -EINVAL;
				break;
			}
			rc = 0;
			hw_vaddr_ptr =
				(uint64_t)cdm_cmd->cmd[i].bl_addr.hw_iova;
			len = cdm_cmd->cmd[i].len + cdm_cmd->cmd[i].offset;
		} else {
			pr_err("Only mem hdl/hw va type is supported %d\n",
				req->data->type);
			rc = -EINVAL;
			break;
		}

		if ((!rc) && (hw_vaddr_ptr) && (len) &&
			(len >= cdm_cmd->cmd[i].offset)) {
			CDM_CDBG("Got the HW VA\n");
			if (core->bl_tag >=
				(CAM_CDM_HWFIFO_SIZE - 1))
				core->bl_tag = 0;
			rc = cam_hw_cdm_bl_write(cdm_hw,
				((uint32_t)hw_vaddr_ptr +
					cdm_cmd->cmd[i].offset),
				(cdm_cmd->cmd[i].len - 1), core->bl_tag);
			if (rc) {
				pr_err("Hw bl write failed %d:%d\n",
					i, req->data->cmd_arrary_count);
				rc = -EIO;
				break;
			}
		} else {
			pr_err("Sanity check failed for hdl=%x len=%zu:%d\n",
				cdm_cmd->cmd[i].bl_addr.mem_handle, len,
				cdm_cmd->cmd[i].offset);
			pr_err("Sanity check failed for %d:%d\n",
				i, req->data->cmd_arrary_count);
			rc = -EINVAL;
			break;
		}

		if (!rc) {
			CDM_CDBG("write BL success for cnt=%d with tag=%d\n",
				i, core->bl_tag);

			CDM_CDBG("Now commit the BL\n");
			if (cam_hw_cdm_commit_bl_write(cdm_hw)) {
				pr_err("Cannot commit the BL %d tag=%d\n",
					i, core->bl_tag);
				rc = -EIO;
				break;
			}
			CDM_CDBG("BL commit success BL %d tag=%d\n", i,
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

		CDM_CDBG("IRQ status=%x\n", payload->irq_status);
		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_INFO_INLINE_IRQ_MASK) {
			struct cam_cdm_bl_cb_request_entry *node;

			CDM_CDBG("inline IRQ data=%x\n",
				payload->irq_data);
			mutex_lock(&cdm_hw->hw_mutex);
			node = cam_cdm_find_request_by_bl_tag(
					payload->irq_data,
					&core->bl_request_list);
			if (node) {
				if (node->request_type ==
					CAM_HW_CDM_BL_CB_CLIENT) {
					cam_cdm_notify_clients(cdm_hw,
						CAM_CDM_CB_STATUS_BL_SUCCESS,
						(void *)node);
				} else if (node->request_type ==
						CAM_HW_CDM_BL_CB_INTERNAL) {
					pr_err("Invalid node=%pK %d\n", node,
						node->request_type);
				}
				list_del_init(&node->entry);
				kfree(node);
			} else {
				pr_err("Invalid node for inline irq status=%x data=%x\n",
					payload->irq_status, payload->irq_data);
			}
			mutex_unlock(&cdm_hw->hw_mutex);
		}

		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_INFO_RST_DONE_MASK) {
			CDM_CDBG("CDM HW reset done IRQ\n");
			complete(&core->reset_complete);
		}
		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_INFO_BL_DONE_MASK) {
			if (atomic_read(&core->bl_done)) {
				CDM_CDBG("CDM HW BL done IRQ\n");
				complete(&core->bl_complete);
			}
		}
		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_ERROR_INV_CMD_MASK) {
			pr_err_ratelimited("Invalid command IRQ, Need HW reset\n");
			atomic_inc(&core->error);
			cam_hw_cdm_dump_core_debug_registers(cdm_hw);
		}
		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_ERROR_AHB_BUS_MASK) {
			pr_err_ratelimited("AHB Error IRQ\n");
			atomic_inc(&core->error);
			cam_hw_cdm_dump_core_debug_registers(cdm_hw);
			atomic_dec(&core->error);
		}
		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_ERROR_OVER_FLOW_MASK) {
			pr_err_ratelimited("Overflow Error IRQ\n");
			atomic_inc(&core->error);
			cam_hw_cdm_dump_core_debug_registers(cdm_hw);
			atomic_dec(&core->error);
		}
		kfree(payload);
	} else {
		pr_err("NULL payload\n");
	}

}

static void cam_hw_cdm_iommu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long iova, int flags, void *token)
{
	struct cam_hw_info *cdm_hw = NULL;
	struct cam_cdm *core = NULL;

	if (token) {
		cdm_hw = (struct cam_hw_info *)token;
		core = (struct cam_cdm *)cdm_hw->core_info;
		atomic_inc(&core->error);
		cam_hw_cdm_dump_core_debug_registers(cdm_hw);
		pr_err_ratelimited("Page fault iova addr %pK\n", (void *)iova);
		cam_cdm_notify_clients(cdm_hw, CAM_CDM_CB_STATUS_PAGEFAULT,
			(void *)iova);
		atomic_dec(&core->error);
	} else {
		pr_err("Invalid token\n");
	}

}

irqreturn_t cam_hw_cdm_irq(int irq_num, void *data)
{
	struct cam_hw_info *cdm_hw = data;
	struct cam_cdm *cdm_core = cdm_hw->core_info;
	struct cam_cdm_work_payload *payload;
	bool work_status;

	CDM_CDBG("Got irq\n");
	payload = kzalloc(sizeof(struct cam_cdm_work_payload), GFP_ATOMIC);
	if (payload) {
		if (cam_cdm_read_hw_reg(cdm_hw, CDM_IRQ_STATUS,
				&payload->irq_status)) {
			pr_err("Failed to read CDM HW IRQ status\n");
		}
		if (!payload->irq_status) {
			pr_err_ratelimited("Invalid irq received\n");
			kfree(payload);
			return IRQ_HANDLED;
		}
		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_INFO_INLINE_IRQ_MASK) {
			if (cam_cdm_read_hw_reg(cdm_hw, CDM_IRQ_USR_DATA,
				&payload->irq_data)) {
				pr_err("Failed to read CDM HW IRQ data\n");
			}
		}
		CDM_CDBG("Got payload=%d\n", payload->irq_status);
		payload->hw = cdm_hw;
		INIT_WORK((struct work_struct *)&payload->work,
			cam_hw_cdm_work);
		if (cam_cdm_write_hw_reg(cdm_hw, CDM_IRQ_CLEAR,
			payload->irq_status))
			pr_err("Failed to Write CDM HW IRQ Clear\n");
		if (cam_cdm_write_hw_reg(cdm_hw, CDM_IRQ_CLEAR_CMD, 0x01))
			pr_err("Failed to Write CDM HW IRQ cmd\n");
		work_status = queue_work(cdm_core->work_queue, &payload->work);
		if (work_status == false) {
			pr_err("Failed to queue work for irq=%x\n",
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
		pr_err("Failed to get genirq cmd space rc=%d\n", rc);
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
		pr_err("Failed to put genirq cmd space for hw\n");

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
		pr_err("Enable platform failed\n");
		goto end;
	}

	CDM_CDBG("Enable soc done\n");

/* Before triggering the reset to HW, clear the reset complete */
	atomic_set(&cdm_core->error, 0);
	atomic_set(&cdm_core->bl_done, 0);
	reinit_completion(&cdm_core->reset_complete);
	reinit_completion(&cdm_core->bl_complete);

	if (cam_cdm_write_hw_reg(cdm_hw, CDM_IRQ_MASK, 0x70003)) {
		pr_err("Failed to Write CDM HW IRQ mask\n");
		goto disable_return;
	}
	if (cam_cdm_write_hw_reg(cdm_hw, CDM_CFG_RST_CMD, 0x9)) {
		pr_err("Failed to Write CDM HW reset\n");
		goto disable_return;
	}

	CDM_CDBG("Waiting for CDM HW resetdone\n");
	time_left = wait_for_completion_timeout(&cdm_core->reset_complete,
		msecs_to_jiffies(CAM_CDM_HW_RESET_TIMEOUT));

	/*
	 * Check for HW error and recover as a workaround
	 * Sometimes CDM HW triggers irq with invalid status for
	 * HW reset command, so ignore reset failure and proceed further
	 * as a workaround.
	 */
	if (time_left <= 0) {
		pr_err("CDM HW reset Wait failed time_left=%ld\n", time_left);
		time_left = 1;
	}

	if (time_left <= 0) {
		pr_err("CDM HW reset Wait failed rc=%d\n", rc);
		goto disable_return;
	} else {
		CDM_CDBG("CDM Init success\n");
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
		pr_err("disable platform failed\n");
	} else {
		CDM_CDBG("CDM Deinit success\n");
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
	struct cam_axi_vote axi_vote;

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
	cdm_hw_intf->hw_type = CAM_HW_CDM;
	cdm_hw->open_count = 0;
	mutex_init(&cdm_hw->hw_mutex);
	spin_lock_init(&cdm_hw->hw_lock);
	init_completion(&cdm_hw->hw_complete);

	rc = cam_hw_cdm_soc_get_dt_properties(cdm_hw, msm_cam_hw_cdm_dt_match);
	if (rc) {
		pr_err("Failed to get dt properties\n");
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
		pr_err("Failed to get CDM HW name for %s\n", cdm_core->name);
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

	CDM_CDBG("type %d index %d\n", cdm_hw_intf->hw_type,
		cdm_hw_intf->hw_idx);

	platform_set_drvdata(pdev, cdm_hw_intf);

	rc = cam_smmu_get_handle("cpas-cdm0", &cdm_core->iommu_hdl.non_secure);
	if (rc < 0) {
		pr_err("cpas-cdm get iommu handle failed\n");
		goto unlock_release_mem;
	}
	cam_smmu_reg_client_page_fault_handler(cdm_core->iommu_hdl.non_secure,
		cam_hw_cdm_iommu_fault_handler, cdm_hw);

	rc = cam_smmu_ops(cdm_core->iommu_hdl.non_secure, CAM_SMMU_ATTACH);
	if (rc < 0) {
		pr_err("Attach iommu non secure handle failed\n");
		goto destroy_non_secure_hdl;
	}
	cdm_core->iommu_hdl.secure = -1;

	cdm_core->work_queue = alloc_workqueue(cdm_core->name,
		WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS,
		CAM_CDM_INFLIGHT_WORKS);

	rc = cam_soc_util_request_platform_resource(&cdm_hw->soc_info,
			cam_hw_cdm_irq, cdm_hw);
	if (rc) {
		pr_err("Failed to request platform resource\n");
		goto destroy_non_secure_hdl;
	}

	cpas_parms.cam_cpas_client_cb = cam_cdm_cpas_cb;
	cpas_parms.cell_index = cdm_hw->soc_info.index;
	cpas_parms.dev = &pdev->dev;
	cpas_parms.userdata = cdm_hw_intf;
	strlcpy(cpas_parms.identifier, "cpas-cdm", CAM_HW_IDENTIFIER_LENGTH);
	rc = cam_cpas_register_client(&cpas_parms);
	if (rc) {
		pr_err("Virtual CDM CPAS registration failed\n");
		goto release_platform_resource;
	}
	CDM_CDBG("CPAS registration successful handle=%d\n",
		cpas_parms.client_handle);
	cdm_core->cpas_handle = cpas_parms.client_handle;

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_SVS_VOTE;
	axi_vote.compressed_bw = CAM_CPAS_DEFAULT_AXI_BW;
	axi_vote.uncompressed_bw = CAM_CPAS_DEFAULT_AXI_BW;
	rc = cam_cpas_start(cdm_core->cpas_handle, &ahb_vote, &axi_vote);
	if (rc) {
		pr_err("CPAS start failed\n");
		goto cpas_unregister;
	}

	rc = cam_hw_cdm_init(cdm_hw, NULL, 0);
	if (rc) {
		pr_err("Failed to Init CDM HW\n");
		goto cpas_stop;
	}
	cdm_hw->open_count++;

	if (cam_cdm_read_hw_reg(cdm_hw, CDM_CFG_HW_VERSION,
		&cdm_core->hw_version)) {
		pr_err("Failed to read CDM HW Version\n");
		goto deinit;
	}

	if (cam_cdm_read_hw_reg(cdm_hw, CDM_CFG_TITAN_VERSION,
		&cdm_core->hw_family_version)) {
		pr_err("Failed to read CDM family Version\n");
		goto deinit;
	}

	CDM_CDBG("CDM Hw version read success family =%x hw =%x\n",
		cdm_core->hw_family_version, cdm_core->hw_version);
	cdm_core->ops = cam_cdm_get_ops(cdm_core->hw_version, NULL,
		false);
	if (!cdm_core->ops) {
		pr_err("Failed to util ops for hw\n");
		goto deinit;
	}

	if (!cam_cdm_set_cam_hw_version(cdm_core->hw_version,
		&cdm_core->version)) {
		pr_err("Failed to set cam he version for hw\n");
		goto deinit;
	}

	rc = cam_hw_cdm_deinit(cdm_hw, NULL, 0);
	if (rc) {
		pr_err("Failed to Deinit CDM HW\n");
		cdm_hw->open_count--;
		goto cpas_stop;
	}

	rc = cam_cpas_stop(cdm_core->cpas_handle);
	if (rc) {
		pr_err("CPAS stop failed\n");
		cdm_hw->open_count--;
		goto cpas_unregister;
	}

	rc = cam_cdm_intf_register_hw_cdm(cdm_hw_intf,
		soc_private, CAM_HW_CDM, &cdm_core->index);
	if (rc) {
		pr_err("HW CDM Interface registration failed\n");
		cdm_hw->open_count--;
		goto cpas_unregister;
	}
	cdm_hw->open_count--;
	mutex_unlock(&cdm_hw->hw_mutex);

	CDM_CDBG("CDM%d probe successful\n", cdm_hw_intf->hw_idx);

	return rc;

deinit:
	if (cam_hw_cdm_deinit(cdm_hw, NULL, 0))
		pr_err("Deinit failed for hw\n");
	cdm_hw->open_count--;
cpas_stop:
	if (cam_cpas_stop(cdm_core->cpas_handle))
		pr_err("CPAS stop failed\n");
cpas_unregister:
	if (cam_cpas_unregister_client(cdm_core->cpas_handle))
		pr_err("CPAS unregister failed\n");
release_platform_resource:
	if (cam_soc_util_release_platform_resource(&cdm_hw->soc_info))
		pr_err("Release platform resource failed\n");

	flush_workqueue(cdm_core->work_queue);
	destroy_workqueue(cdm_core->work_queue);
destroy_non_secure_hdl:
	cam_smmu_reg_client_page_fault_handler(cdm_core->iommu_hdl.non_secure,
		NULL, cdm_hw);
	if (cam_smmu_destroy_handle(cdm_core->iommu_hdl.non_secure))
		pr_err("Release iommu secure hdl failed\n");
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
		pr_err("Failed to get dev private data\n");
		return rc;
	}

	cdm_hw = cdm_hw_intf->hw_priv;
	if (!cdm_hw) {
		pr_err("Failed to get hw private data for type=%d idx=%d\n",
			cdm_hw_intf->hw_type, cdm_hw_intf->hw_idx);
		return rc;
	}

	cdm_core = cdm_hw->core_info;
	if (!cdm_core) {
		pr_err("Failed to get hw core data for type=%d idx=%d\n",
			cdm_hw_intf->hw_type, cdm_hw_intf->hw_idx);
		return rc;
	}

	if (cdm_hw->open_count != 0) {
		pr_err("Hw open count invalid type=%d idx=%d cnt=%d\n",
			cdm_hw_intf->hw_type, cdm_hw_intf->hw_idx,
			cdm_hw->open_count);
		return rc;
	}

	rc = cam_hw_cdm_deinit(cdm_hw, NULL, 0);
	if (rc) {
		pr_err("Deinit failed for hw\n");
		return rc;
	}

	rc = cam_cpas_unregister_client(cdm_core->cpas_handle);
	if (rc) {
		pr_err("CPAS unregister failed\n");
		return rc;
	}

	if (cam_soc_util_release_platform_resource(&cdm_hw->soc_info))
		pr_err("Release platform resource failed\n");

	flush_workqueue(cdm_core->work_queue);
	destroy_workqueue(cdm_core->work_queue);

	if (cam_smmu_destroy_handle(cdm_core->iommu_hdl.non_secure))
		pr_err("Release iommu secure hdl failed\n");
	cam_smmu_reg_client_page_fault_handler(cdm_core->iommu_hdl.non_secure,
		NULL, cdm_hw);

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
