// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/videodev2.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/timer.h>

#include "cam_io_util.h"
#include "cam_hw.h"
#include "cam_hw_intf.h"
#include "jpeg_enc_core.h"
#include "jpeg_enc_soc.h"
#include "cam_soc_util.h"
#include "cam_io_util.h"
#include "cam_jpeg_hw_intf.h"
#include "cam_jpeg_hw_mgr_intf.h"
#include "cam_cpas_api.h"
#include "cam_debug_util.h"
#include "cam_common_util.h"

#define CAM_JPEG_HW_IRQ_IS_FRAME_DONE(jpeg_irq_status, hi) \
	((jpeg_irq_status) & (hi)->int_status.framedone)
#define CAM_JPEG_HW_IRQ_IS_RESET_ACK(jpeg_irq_status, hi) \
	((jpeg_irq_status) & (hi)->int_status.resetdone)
#define CAM_JPEG_HW_IRQ_IS_ERR(jpeg_irq_status, hi) \
	((jpeg_irq_status) & (hi)->int_status.iserror)
#define CAM_JPEG_HW_IRQ_IS_STOP_DONE(jpeg_irq_status, hi) \
	((jpeg_irq_status) & (hi)->int_status.stopdone)

#define CAM_JPEG_ENC_RESET_TIMEOUT msecs_to_jiffies(500)

int cam_jpeg_enc_init_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_enc_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_enc_device_core_info *core_info = NULL;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote = {0};
	unsigned long flags;
	int rc;

	if (!device_priv) {
		CAM_ERR(CAM_JPEG, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &jpeg_enc_dev->soc_info;
	core_info = (struct cam_jpeg_enc_device_core_info *)
		jpeg_enc_dev->core_info;

	if (!soc_info || !core_info) {
		CAM_ERR(CAM_JPEG, "soc_info = %pK core_info = %pK",
			soc_info, core_info);
		return -EINVAL;
	}

	mutex_lock(&core_info->core_mutex);
	if (++core_info->ref_count > 1) {
		mutex_unlock(&core_info->core_mutex);
		return 0;
	}

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_LOWSVS_VOTE;
	axi_vote.num_paths = 2;
	axi_vote.axi_path[0].path_data_type = CAM_AXI_PATH_DATA_ALL;
	axi_vote.axi_path[0].transac_type = CAM_AXI_TRANSACTION_READ;
	axi_vote.axi_path[0].camnoc_bw = JPEG_VOTE;
	axi_vote.axi_path[0].mnoc_ab_bw = JPEG_VOTE;
	axi_vote.axi_path[0].mnoc_ib_bw = JPEG_VOTE;
	axi_vote.axi_path[1].path_data_type = CAM_AXI_PATH_DATA_ALL;
	axi_vote.axi_path[1].transac_type = CAM_AXI_TRANSACTION_WRITE;
	axi_vote.axi_path[1].camnoc_bw = JPEG_VOTE;
	axi_vote.axi_path[1].mnoc_ab_bw = JPEG_VOTE;
	axi_vote.axi_path[1].mnoc_ib_bw = JPEG_VOTE;


	rc = cam_cpas_start(core_info->cpas_handle,
		&ahb_vote, &axi_vote);
	if (rc) {
		CAM_ERR(CAM_JPEG, "cpass start failed: %d", rc);
		goto cpas_failed;
	}

	rc = cam_jpeg_enc_enable_soc_resources(soc_info);
	if (rc) {
		CAM_ERR(CAM_JPEG, "soc enable is failed %d", rc);
		goto soc_failed;
	}
	spin_lock_irqsave(&jpeg_enc_dev->hw_lock, flags);
	jpeg_enc_dev->hw_state = CAM_HW_STATE_POWER_UP;
	spin_unlock_irqrestore(&jpeg_enc_dev->hw_lock, flags);

	mutex_unlock(&core_info->core_mutex);

	return 0;

soc_failed:
	cam_cpas_stop(core_info->cpas_handle);
cpas_failed:
	--core_info->ref_count;
	mutex_unlock(&core_info->core_mutex);

	return rc;
}

int cam_jpeg_enc_deinit_hw(void *device_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_enc_dev = device_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_enc_device_core_info *core_info = NULL;
	unsigned long flags;
	int rc;

	if (!device_priv) {
		CAM_ERR(CAM_JPEG, "Invalid cam_dev_info");
		return -EINVAL;
	}

	soc_info = &jpeg_enc_dev->soc_info;
	core_info = (struct cam_jpeg_enc_device_core_info *)
		jpeg_enc_dev->core_info;
	if (!soc_info || !core_info) {
		CAM_ERR(CAM_JPEG, "soc_info = %pK core_info = %pK",
			soc_info, core_info);
		return -EINVAL;
	}

	mutex_lock(&core_info->core_mutex);
	if (--core_info->ref_count > 0) {
		mutex_unlock(&core_info->core_mutex);
		return 0;
	}

	if (core_info->ref_count < 0) {
		CAM_ERR(CAM_JPEG, "ref cnt %d", core_info->ref_count);
		core_info->ref_count = 0;
		mutex_unlock(&core_info->core_mutex);
		return -EFAULT;
	}

	spin_lock_irqsave(&jpeg_enc_dev->hw_lock, flags);
	jpeg_enc_dev->hw_state = CAM_HW_STATE_POWER_DOWN;
	spin_unlock_irqrestore(&jpeg_enc_dev->hw_lock, flags);
	rc = cam_jpeg_enc_disable_soc_resources(soc_info);
	if (rc)
		CAM_ERR(CAM_JPEG, "soc disable failed %d", rc);

	rc = cam_cpas_stop(core_info->cpas_handle);
	if (rc)
		CAM_ERR(CAM_JPEG, "cpas stop failed: %d", rc);

	mutex_unlock(&core_info->core_mutex);

	return 0;
}

irqreturn_t cam_jpeg_enc_irq(int irq_num, void *data)
{
	struct cam_hw_info *jpeg_enc_dev = data;
	struct cam_jpeg_enc_device_core_info *core_info = NULL;
	uint32_t irq_status = 0;
	uint32_t encoded_size = 0;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_enc_device_hw_info *hw_info = NULL;
	void __iomem *mem_base;

	if (!jpeg_enc_dev) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return IRQ_HANDLED;
	}
	soc_info = &jpeg_enc_dev->soc_info;
	core_info = (struct cam_jpeg_enc_device_core_info *)
		jpeg_enc_dev->core_info;
	hw_info = core_info->jpeg_enc_hw_info;
	mem_base = soc_info->reg_map[0].mem_base;

	spin_lock(&jpeg_enc_dev->hw_lock);
	if (jpeg_enc_dev->hw_state == CAM_HW_STATE_POWER_DOWN) {
		CAM_ERR(CAM_JPEG, "JPEG HW is in off state");
		spin_unlock(&jpeg_enc_dev->hw_lock);
		return IRQ_HANDLED;
	}
	irq_status = cam_io_r_mb(mem_base +
		core_info->jpeg_enc_hw_info->reg_offset.int_status);

	cam_io_w_mb(irq_status,
		soc_info->reg_map[0].mem_base +
		core_info->jpeg_enc_hw_info->reg_offset.int_clr);
	spin_unlock(&jpeg_enc_dev->hw_lock);

	CAM_DBG(CAM_JPEG, "irq_num: %d  irq_status: 0x%x , core_state: %d",
		irq_num, irq_status, core_info->core_state);

	if (CAM_JPEG_HW_IRQ_IS_FRAME_DONE(irq_status, hw_info)) {
		spin_lock(&jpeg_enc_dev->hw_lock);
		if (core_info->core_state == CAM_JPEG_ENC_CORE_READY) {
			CAM_TRACE(CAM_JPEG, "Ctx %lld ENC FrameDone IRQ",
				core_info->irq_cb.irq_cb_data.private_data);
			encoded_size = cam_io_r_mb(mem_base +
			core_info->jpeg_enc_hw_info->reg_offset.encode_size);
			if (core_info->irq_cb.jpeg_hw_mgr_cb) {
				core_info->irq_cb.jpeg_hw_mgr_cb(irq_status,
					encoded_size,
					(void *)&core_info->irq_cb.irq_cb_data);
			} else {
				CAM_ERR(CAM_JPEG, "unexpected done, no cb");
			}
			cam_cpas_notify_event("JPEG FrameDone", 0);
		} else {
			CAM_ERR(CAM_JPEG, "unexpected done irq");
		}
		core_info->core_state = CAM_JPEG_ENC_CORE_NOT_READY;
		spin_unlock(&jpeg_enc_dev->hw_lock);
	}
	if (CAM_JPEG_HW_IRQ_IS_RESET_ACK(irq_status, hw_info)) {
		spin_lock(&jpeg_enc_dev->hw_lock);
		if (core_info->core_state == CAM_JPEG_ENC_CORE_RESETTING) {
			core_info->core_state = CAM_JPEG_ENC_CORE_READY;
			complete(&jpeg_enc_dev->hw_complete);
		} else {
			CAM_ERR(CAM_JPEG, "unexpected reset irq");
		}
		spin_unlock(&jpeg_enc_dev->hw_lock);
	}
	if (CAM_JPEG_HW_IRQ_IS_STOP_DONE(irq_status, hw_info)) {
		spin_lock(&jpeg_enc_dev->hw_lock);
		if (core_info->core_state == CAM_JPEG_ENC_CORE_ABORTING) {
			core_info->core_state = CAM_JPEG_ENC_CORE_NOT_READY;
			complete(&jpeg_enc_dev->hw_complete);
			if (core_info->irq_cb.jpeg_hw_mgr_cb) {
				core_info->irq_cb.jpeg_hw_mgr_cb(irq_status, -1,
					(void *)&core_info->irq_cb.irq_cb_data);
			}
		} else {
			CAM_ERR(CAM_JPEG, "unexpected abort irq");
		}
		spin_unlock(&jpeg_enc_dev->hw_lock);
	}
	/* Unexpected/unintended HW interrupt */
	if (CAM_JPEG_HW_IRQ_IS_ERR(irq_status, hw_info)) {
		spin_lock(&jpeg_enc_dev->hw_lock);
		core_info->core_state = CAM_JPEG_ENC_CORE_NOT_READY;
		CAM_ERR_RATE_LIMIT(CAM_JPEG,
			"error irq_num %d  irq_status = %x , core_state %d",
			irq_num, irq_status, core_info->core_state);

		if (core_info->irq_cb.jpeg_hw_mgr_cb) {
			core_info->irq_cb.jpeg_hw_mgr_cb(irq_status, -1,
				(void *)&core_info->irq_cb.irq_cb_data);
		}
		spin_unlock(&jpeg_enc_dev->hw_lock);
	}

	return IRQ_HANDLED;
}

int cam_jpeg_enc_reset_hw(void *data,
	void *start_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_enc_dev = data;
	struct cam_jpeg_enc_device_core_info *core_info = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_enc_device_hw_info *hw_info = NULL;
	void __iomem *mem_base;
	unsigned long rem_jiffies;
	unsigned long flags;

	if (!jpeg_enc_dev) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return -EINVAL;
	}
	/* maskdisable.clrirq.maskenable.resetcmd */
	soc_info = &jpeg_enc_dev->soc_info;
	core_info = (struct cam_jpeg_enc_device_core_info *)
		jpeg_enc_dev->core_info;
	hw_info = core_info->jpeg_enc_hw_info;
	mem_base = soc_info->reg_map[0].mem_base;

	mutex_lock(&core_info->core_mutex);
	spin_lock_irqsave(&jpeg_enc_dev->hw_lock, flags);
	if (jpeg_enc_dev->hw_state == CAM_HW_STATE_POWER_DOWN) {
		CAM_ERR(CAM_JPEG, "JPEG HW is in off state");
		spin_unlock_irqrestore(&jpeg_enc_dev->hw_lock, flags);
		mutex_unlock(&core_info->core_mutex);
		return -EINVAL;
	}
	if (core_info->core_state == CAM_JPEG_ENC_CORE_RESETTING) {
		CAM_ERR(CAM_JPEG, "alrady resetting");
		spin_unlock_irqrestore(&jpeg_enc_dev->hw_lock, flags);
		mutex_unlock(&core_info->core_mutex);
		return 0;
	}

	reinit_completion(&jpeg_enc_dev->hw_complete);
	core_info->core_state = CAM_JPEG_ENC_CORE_RESETTING;
	spin_unlock_irqrestore(&jpeg_enc_dev->hw_lock, flags);

	cam_io_w_mb(hw_info->reg_val.int_mask_disable_all,
		mem_base + hw_info->reg_offset.int_mask);
	cam_io_w_mb(hw_info->reg_val.int_clr_clearall,
		mem_base + hw_info->reg_offset.int_clr);
	cam_io_w_mb(hw_info->reg_val.int_mask_enable_all,
		mem_base + hw_info->reg_offset.int_mask);
	cam_io_w_mb(hw_info->reg_val.reset_cmd,
		mem_base + hw_info->reg_offset.reset_cmd);

	rem_jiffies = cam_common_wait_for_completion_timeout(
			&jpeg_enc_dev->hw_complete,
			CAM_JPEG_ENC_RESET_TIMEOUT);
	if (!rem_jiffies) {
		CAM_ERR(CAM_JPEG, "error Reset Timeout");
		core_info->core_state = CAM_JPEG_ENC_CORE_NOT_READY;
	}

	mutex_unlock(&core_info->core_mutex);
	return 0;
}

int cam_jpeg_enc_start_hw(void *data,
	void *start_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_enc_dev = data;
	struct cam_jpeg_enc_device_core_info *core_info = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_enc_device_hw_info *hw_info = NULL;
	void __iomem *mem_base;
	unsigned long flags;

	if (!jpeg_enc_dev) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return -EINVAL;
	}

	soc_info = &jpeg_enc_dev->soc_info;
	core_info = (struct cam_jpeg_enc_device_core_info *)
		jpeg_enc_dev->core_info;
	hw_info = core_info->jpeg_enc_hw_info;
	mem_base = soc_info->reg_map[0].mem_base;

	spin_lock_irqsave(&jpeg_enc_dev->hw_lock, flags);
	if (jpeg_enc_dev->hw_state == CAM_HW_STATE_POWER_DOWN) {
		CAM_ERR(CAM_JPEG, "JPEG HW is in off state");
		spin_unlock_irqrestore(&jpeg_enc_dev->hw_lock, flags);
		return -EINVAL;
	}
	if (core_info->core_state != CAM_JPEG_ENC_CORE_READY) {
		CAM_ERR(CAM_JPEG, "Error not ready: %d", core_info->core_state);
		spin_unlock_irqrestore(&jpeg_enc_dev->hw_lock, flags);
		return -EINVAL;
	}
	spin_unlock_irqrestore(&jpeg_enc_dev->hw_lock, flags);

	CAM_DBG(CAM_JPEG, "Starting JPEG ENC");
	cam_io_w_mb(hw_info->reg_val.hw_cmd_start,
		mem_base + hw_info->reg_offset.hw_cmd);

	return 0;
}

int cam_jpeg_enc_stop_hw(void *data,
	void *stop_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_enc_dev = data;
	struct cam_jpeg_enc_device_core_info *core_info = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_jpeg_enc_device_hw_info *hw_info = NULL;
	void __iomem *mem_base;
	unsigned long rem_jiffies;
	unsigned long flags;

	if (!jpeg_enc_dev) {
		CAM_ERR(CAM_JPEG, "Invalid args");
		return -EINVAL;
	}
	soc_info = &jpeg_enc_dev->soc_info;
	core_info = (struct cam_jpeg_enc_device_core_info *)
		jpeg_enc_dev->core_info;
	hw_info = core_info->jpeg_enc_hw_info;
	mem_base = soc_info->reg_map[0].mem_base;

	mutex_lock(&core_info->core_mutex);
	spin_lock_irqsave(&jpeg_enc_dev->hw_lock, flags);
	if (jpeg_enc_dev->hw_state == CAM_HW_STATE_POWER_DOWN) {
		CAM_ERR(CAM_JPEG, "JPEG HW is in off state");
		spin_unlock_irqrestore(&jpeg_enc_dev->hw_lock, flags);
		mutex_unlock(&core_info->core_mutex);
		return -EINVAL;
	}
	if (core_info->core_state == CAM_JPEG_ENC_CORE_ABORTING) {
		CAM_ERR(CAM_JPEG, "alrady stopping");
		spin_unlock_irqrestore(&jpeg_enc_dev->hw_lock, flags);
		mutex_unlock(&core_info->core_mutex);
		return 0;
	}

	reinit_completion(&jpeg_enc_dev->hw_complete);
	core_info->core_state = CAM_JPEG_ENC_CORE_ABORTING;
	spin_unlock_irqrestore(&jpeg_enc_dev->hw_lock, flags);

	cam_io_w_mb(hw_info->reg_val.hw_cmd_stop,
		mem_base + hw_info->reg_offset.hw_cmd);

	rem_jiffies = cam_common_wait_for_completion_timeout(
			&jpeg_enc_dev->hw_complete,
			CAM_JPEG_ENC_RESET_TIMEOUT);
	if (!rem_jiffies) {
		CAM_ERR(CAM_JPEG, "error Reset Timeout");
		core_info->core_state = CAM_JPEG_ENC_CORE_NOT_READY;
	}

	mutex_unlock(&core_info->core_mutex);
	return 0;
}

int cam_jpeg_enc_hw_dump(
	struct cam_hw_info           *jpeg_enc_dev,
	struct cam_jpeg_hw_dump_args *dump_args)
{

	int                                   i;
	uint8_t                              *dst;
	uint32_t                             *addr, *start;
	uint32_t                              num_reg, min_len;
	uint32_t                              reg_start_offset;
	size_t                                remain_len;
	struct cam_hw_soc_info               *soc_info;
	struct cam_jpeg_hw_dump_header       *hdr;
	struct cam_jpeg_enc_device_hw_info   *hw_info;
	struct cam_jpeg_enc_device_core_info *core_info;

	soc_info = &jpeg_enc_dev->soc_info;
	core_info = (struct cam_jpeg_enc_device_core_info *)
		jpeg_enc_dev->core_info;
	hw_info = core_info->jpeg_enc_hw_info;
	mutex_lock(&core_info->core_mutex);
	spin_lock(&jpeg_enc_dev->hw_lock);

	if (jpeg_enc_dev->hw_state == CAM_HW_STATE_POWER_DOWN) {
		CAM_ERR(CAM_JPEG, "JPEG HW is in off state");
		spin_unlock(&jpeg_enc_dev->hw_lock);
		mutex_unlock(&core_info->core_mutex);
		return -EINVAL;
	}

	spin_unlock(&jpeg_enc_dev->hw_lock);

	if (dump_args->buf_len <= dump_args->offset) {
		CAM_WARN(CAM_JPEG, "dump buffer overshoot %zu %zu",
			dump_args->buf_len, dump_args->offset);
		mutex_unlock(&core_info->core_mutex);
		return -ENOSPC;
	}

	remain_len = dump_args->buf_len - dump_args->offset;
	min_len =  sizeof(struct cam_jpeg_hw_dump_header) +
		    soc_info->reg_map[0].size + sizeof(uint32_t);
	if (remain_len < min_len) {
		CAM_WARN(CAM_JPEG, "dump buffer exhaust %zu %u",
			remain_len, min_len);
		mutex_unlock(&core_info->core_mutex);
		return -ENOSPC;
	}

	dst = (uint8_t *)dump_args->cpu_addr + dump_args->offset;
	hdr = (struct cam_jpeg_hw_dump_header *)dst;
	snprintf(hdr->tag, CAM_JPEG_HW_DUMP_TAG_MAX_LEN,
		"JPEG_REG:");
	hdr->word_size = sizeof(uint32_t);
	addr = (uint32_t *)(dst + sizeof(struct cam_jpeg_hw_dump_header));
	start = addr;
	*addr++ = soc_info->index;
	num_reg = (hw_info->reg_dump.end_offset -
		hw_info->reg_dump.start_offset)/4;
	reg_start_offset = hw_info->reg_dump.start_offset;
	for (i = 0; i < num_reg; i++) {
		*addr++ = soc_info->mem_block[0]->start +
			reg_start_offset + i*4;
		*addr++ = cam_io_r(soc_info->reg_map[0].mem_base + (i*4));
	}

	mutex_unlock(&core_info->core_mutex);
	hdr->size = hdr->word_size * (addr - start);
	dump_args->offset += hdr->size +
		sizeof(struct cam_jpeg_hw_dump_header);
	CAM_DBG(CAM_JPEG, "offset %zu", dump_args->offset);

	return 0;
}

static int  cam_jpeg_enc_mini_dump(struct cam_hw_info *dev, void *args) {

	struct cam_jpeg_mini_dump_core_info *md;
	struct cam_jpeg_enc_device_hw_info   *hw_info;
	struct cam_jpeg_enc_device_core_info *core_info;

	if (!dev || !args) {
		CAM_ERR(CAM_JPEG, "Invalid params priv %pK args %pK", dev, args);
		return -EINVAL;
	}

	core_info = (struct cam_jpeg_enc_device_core_info *)dev->core_info;
	hw_info = core_info->jpeg_enc_hw_info;
	md = (struct cam_jpeg_mini_dump_core_info *)args;

	md->framedone = hw_info->int_status.framedone;
	md->resetdone = hw_info->int_status.resetdone;
	md->iserror = hw_info->int_status.iserror;
	md->stopdone = hw_info->int_status.stopdone;
	md->open_count = dev->open_count;
	md->hw_state = dev->hw_state;
	md->ref_count = core_info->ref_count;
	md->core_state = core_info->core_state;
	return 0;
}

int cam_jpeg_enc_dump_camnoc_misr_val(struct cam_jpeg_enc_device_hw_info *hw_info,
	struct cam_hw_soc_info *soc_info, void *cmd_args)
{
	void __iomem                         *enc_mem_base = NULL;
	void __iomem                         *camnoc_mem_base = NULL;
	struct cam_jpeg_misr_dump_args       *pmisr_args;
	int32_t val;
	uint32_t index = 0;
	int i;
	bool mismatch = false;
	int32_t camnoc_misr_val[CAM_JPEG_CAMNOC_MISR_VAL_ROW][
		CAM_JPEG_CAMNOC_MISR_VAL_COL] = {{0}};

	enc_mem_base = soc_info->reg_map[0].mem_base;
	camnoc_mem_base = soc_info->reg_map[1].mem_base;
	if (!camnoc_mem_base) {
		CAM_ERR(CAM_JPEG, "Invalid camnoc base address");
		return -EINVAL;
	}
	pmisr_args = (struct cam_jpeg_misr_dump_args *)cmd_args;
	if (!pmisr_args) {
		CAM_ERR(CAM_JPEG, "Invalid command argument");
		return -EINVAL;
	}

	val = cam_io_r_mb(enc_mem_base + hw_info->reg_offset.core_cfg);
	index = (val >> hw_info->int_status.scale_enable_shift) &
		hw_info->int_status.scale_enable;
	CAM_DBG(CAM_JPEG, "index %d", index);

	for (i = 0; i < hw_info->camnoc_misr_sigdata; i++) {
		camnoc_misr_val[index][i] = cam_io_r_mb(camnoc_mem_base +
			hw_info->camnoc_misr_reg_offset.sigdata0 + (i * 8));
		if (hw_info->prev_camnoc_misr_val[index][i] != camnoc_misr_val[index][i])
			mismatch = true;
	}
	if (mismatch && (pmisr_args->req_id != 1)) {
		CAM_ERR(CAM_JPEG,
			"CAMNOC ENC_MISR MISMATCH [req:%d][i:%d][index:%d]\n"
			"curr SigData:0x%x %x %x %x prev SigData:0x%x %x %x %x isbug:%d",
			pmisr_args->req_id, i, index,
			camnoc_misr_val[index][3], camnoc_misr_val[index][2],
			camnoc_misr_val[index][1], camnoc_misr_val[index][0],
			hw_info->prev_camnoc_misr_val[index][3],
			hw_info->prev_camnoc_misr_val[index][2],
			hw_info->prev_camnoc_misr_val[index][1],
			hw_info->prev_camnoc_misr_val[index][0], pmisr_args->enable_bug);
		if (pmisr_args->enable_bug)
			BUG_ON(1);
	}
	CAM_DBG(CAM_JPEG,
		"CAMNOC ENC MISR req:%d SigData:0x%x %x %x %x",
		pmisr_args->req_id,
		camnoc_misr_val[index][3], camnoc_misr_val[index][2],
		camnoc_misr_val[index][1], camnoc_misr_val[index][0]);
	mismatch = false;
	for (i = 0; i < hw_info->camnoc_misr_sigdata; i++)
		hw_info->prev_camnoc_misr_val[index][i] = camnoc_misr_val[index][i];
	/* stop misr : cam_noc_cam_noc_0_req_link_misrprb_MiscCtl_Low */
	cam_io_w_mb(hw_info->camnoc_misr_reg_val.misc_ctl_stop,
		camnoc_mem_base + hw_info->camnoc_misr_reg_offset.misc_ctl);
	return 0;
}

int cam_jpeg_enc_dump_hw_misr_val(struct cam_jpeg_enc_device_hw_info *hw_info,
	struct cam_hw_soc_info *soc_info, void *cmd_args)
{
	void __iomem                         *enc_mem_base = NULL;
	void __iomem                         *camnoc_mem_base = NULL;
	struct cam_jpeg_misr_dump_args       *pmisr_args;
	int32_t val;
	uint32_t index = 0;
	int offset, i, j;
	bool mismatch = false;
	int32_t enc_misr_val[CAM_JPEG_ENC_MISR_VAL_NUM][CAM_JPEG_CAMNOC_MISR_VAL_ROW][
		CAM_JPEG_CAMNOC_MISR_VAL_COL] = {{{0}}};

	enc_mem_base = soc_info->reg_map[0].mem_base;
	camnoc_mem_base = soc_info->reg_map[1].mem_base;
	if (!camnoc_mem_base) {
		CAM_ERR(CAM_JPEG, "Invalid camnoc base address");
		return -EINVAL;
	}
	pmisr_args = (struct cam_jpeg_misr_dump_args *)cmd_args;
	if (!pmisr_args) {
		CAM_ERR(CAM_JPEG, "Invalid command argument");
		return -EINVAL;
	}

	val = cam_io_r_mb(enc_mem_base + hw_info->reg_offset.core_cfg);
	index = (val >> hw_info->int_status.scale_enable_shift) &
		hw_info->int_status.scale_enable;
	CAM_DBG(CAM_JPEG, "index %d", index);

	for (i = 0; i < hw_info->max_misr; i++) {
		offset = hw_info->reg_offset.misr_rd0 + (i * 0x10);
		for (j = 0; j < hw_info->max_misr_rd; j++) {
			enc_misr_val[i][index][j] = cam_io_r_mb(enc_mem_base +
				offset + (j * 4));
			if (hw_info->prev_enc_misr_val[i][index][j] !=
				enc_misr_val[i][index][j])
				mismatch = true;
		}
		if (mismatch && (pmisr_args->req_id != 1)) {
			CAM_ERR(CAM_JPEG,
				"ENC_MISR RD MISMATCH [req:%d][i:%d][index:%d][j:%d]\n"
				"curr:0x%x %x %x %x prev:0x%x %x %x %x isbug:%d",
				pmisr_args->req_id, i, index, j, enc_misr_val[i][index][3],
				enc_misr_val[i][index][2], enc_misr_val[i][index][1],
				enc_misr_val[i][index][0], hw_info->prev_enc_misr_val[i][index][3],
				hw_info->prev_enc_misr_val[i][index][2],
				hw_info->prev_enc_misr_val[i][index][1],
				hw_info->prev_enc_misr_val[i][index][0], pmisr_args->enable_bug);
			if (pmisr_args->enable_bug)
				BUG_ON(1);
		}
		CAM_DBG(CAM_JPEG, "ENC_MISR RD [req:%d][%d]: 0x%x %x %x %x",
			pmisr_args->req_id, i,
			enc_misr_val[i][index][3], enc_misr_val[i][index][2],
			enc_misr_val[i][index][1], enc_misr_val[i][index][0]);
		mismatch = false;

		for (j = 0; j < hw_info->max_misr_rd; j++)
			hw_info->prev_enc_misr_val[i][index][j] = enc_misr_val[i][index][j];
	}

	return 0;
}

int cam_jpeg_enc_config_cmanoc_hw_misr(struct cam_jpeg_enc_device_hw_info *hw_info,
	struct cam_hw_soc_info *soc_info, void *cmd_args)
{
	void __iomem                         *enc_mem_base = NULL;
	void __iomem                         *camnoc_mem_base = NULL;
	uint32_t                             *camnoc_misr_test = NULL;
	int val = 0;

	if (!hw_info->camnoc_misr_support) {
		CAM_DBG(CAM_JPEG, "camnoc misr is not supported");
		return 0;
	}

	enc_mem_base = soc_info->reg_map[0].mem_base;
	camnoc_mem_base = soc_info->reg_map[1].mem_base;
	if (!camnoc_mem_base) {
		CAM_ERR(CAM_JPEG, "Invalid camnoc base address");
		return -EINVAL;
	}
	camnoc_misr_test = (uint32_t *)cmd_args;
	if (!camnoc_misr_test) {
		CAM_ERR(CAM_JPEG, "Invalid command argument");
		return -EINVAL;
	}

	/* enable all MISRs */
	cam_io_w_mb(hw_info->reg_val.misr_cfg, enc_mem_base +
		hw_info->reg_offset.misr_cfg);

	/* cam_noc_cam_noc_0_req_link_misrprb_MainCtl_Low
	 * enable CRC generation on both RD, WR and transaction payload
	 */
	cam_io_w_mb(hw_info->camnoc_misr_reg_val.main_ctl, camnoc_mem_base +
		hw_info->camnoc_misr_reg_offset.main_ctl);

	/* cam_noc_cam_noc_0_req_link_misrprb_IdMask_Low */
	cam_io_w_mb(hw_info->camnoc_misr_reg_val.main_ctl, camnoc_mem_base +
		hw_info->camnoc_misr_reg_offset.id_mask_low);

	/* cam_noc_cam_noc_0_req_link_misrprb_IdValue_Low */
	switch (*camnoc_misr_test) {
	case CAM_JPEG_MISR_ID_LOW_RD:
		val = hw_info->camnoc_misr_reg_val.id_value_low_rd;
		break;
	case CAM_JPEG_MISR_ID_LOW_WR:
		val = hw_info->camnoc_misr_reg_val.id_value_low_wr;
		break;
	default:
		val = hw_info->camnoc_misr_reg_val.id_value_low_rd;
		break;
	}
	cam_io_w_mb(val, camnoc_mem_base +
		hw_info->camnoc_misr_reg_offset.id_value_low);

	/* start/reset misr : cam_noc_cam_noc_0_req_link_misrprb_MiscCtl_Low */
	cam_io_w_mb(hw_info->camnoc_misr_reg_val.misc_ctl_start,
		camnoc_mem_base + hw_info->camnoc_misr_reg_offset.misc_ctl);
	CAM_DBG(CAM_JPEG, "ENC CAMNOC MISR configured");

	return 0;
}

int cam_jpeg_enc_process_cmd(void *device_priv, uint32_t cmd_type,
	void *cmd_args, uint32_t arg_size)
{
	struct cam_hw_info *jpeg_enc_dev = device_priv;
	struct cam_jpeg_enc_device_core_info *core_info = NULL;
	struct cam_jpeg_enc_device_hw_info   *hw_info = NULL;
	struct cam_jpeg_match_pid_args       *match_pid_mid = NULL;
	uint32_t    *num_pid = NULL;
	struct cam_hw_soc_info               *soc_info = NULL;
	int i, rc = 0;

	if (!device_priv) {
		CAM_ERR(CAM_JPEG, "Invalid arguments");
		return -EINVAL;
	}

	if (cmd_type >= CAM_JPEG_CMD_MAX) {
		CAM_ERR(CAM_JPEG, "Invalid command : %x", cmd_type);
		return -EINVAL;
	}

	core_info = (struct cam_jpeg_enc_device_core_info *)
		jpeg_enc_dev->core_info;

	hw_info = core_info->jpeg_enc_hw_info;
	soc_info = &jpeg_enc_dev->soc_info;

	switch (cmd_type) {
	case CAM_JPEG_CMD_SET_IRQ_CB:
	{
		struct cam_jpeg_set_irq_cb *irq_cb = cmd_args;
		struct cam_jpeg_irq_cb_data *irq_cb_data;

		if (!cmd_args) {
			CAM_ERR(CAM_JPEG, "cmd args NULL");
			return -EINVAL;
		}

		irq_cb_data = &irq_cb->irq_cb_data;
		spin_lock(&jpeg_enc_dev->hw_lock);

		if (irq_cb->b_set_cb) {
			core_info->irq_cb.jpeg_hw_mgr_cb = irq_cb->jpeg_hw_mgr_cb;
			core_info->irq_cb.irq_cb_data.private_data = irq_cb_data->private_data;
			core_info->irq_cb.irq_cb_data.jpeg_req = irq_cb_data->jpeg_req;
		} else {
			core_info->irq_cb.jpeg_hw_mgr_cb = NULL;
			core_info->irq_cb.irq_cb_data.private_data = NULL;
			core_info->irq_cb.irq_cb_data.jpeg_req = NULL;
		}
		spin_unlock(&jpeg_enc_dev->hw_lock);
		rc = 0;
		break;
	}
	case CAM_JPEG_CMD_HW_DUMP:
	{
		rc = cam_jpeg_enc_hw_dump(jpeg_enc_dev,
			cmd_args);
		break;
	}
	case CAM_JPEG_CMD_GET_NUM_PID:
		if (!cmd_args) {
			CAM_ERR(CAM_JPEG, "cmd args NULL");
			return -EINVAL;
		}

		num_pid = (uint32_t    *)cmd_args;
		*num_pid = core_info->num_pid;

		break;
	case CAM_JPEG_CMD_MATCH_PID_MID:
		match_pid_mid = (struct cam_jpeg_match_pid_args *)cmd_args;

		if (!cmd_args) {
			CAM_ERR(CAM_JPEG, "cmd args NULL");
			return -EINVAL;
		}

		for (i = 0 ; i < core_info->num_pid; i++) {
			if (core_info->pid[i] == match_pid_mid->pid)
				break;
		}

		if (i == core_info->num_pid)
			match_pid_mid->pid_match_found = false;
		else
			match_pid_mid->pid_match_found = true;

		if (match_pid_mid->pid_match_found) {
			if (match_pid_mid->fault_mid == core_info->rd_mid) {
				match_pid_mid->match_res =
					CAM_JPEG_ENC_INPUT_IMAGE;
			} else if (match_pid_mid->fault_mid ==
				core_info->wr_mid) {
				match_pid_mid->match_res =
					CAM_JPEG_ENC_OUTPUT_IMAGE;
			} else
				match_pid_mid->pid_match_found = false;
		}

		break;
	case CAM_JPEG_CMD_MINI_DUMP:
	{
		rc = cam_jpeg_enc_mini_dump(jpeg_enc_dev, cmd_args);
		break;
	}
	case CAM_JPEG_CMD_CONFIG_HW_MISR:
	{
		if (hw_info->camnoc_misr_support)
			rc = cam_jpeg_enc_config_cmanoc_hw_misr(hw_info, soc_info, cmd_args);
		else
			CAM_DBG(CAM_JPEG, "camnoc misr is not supported");
		break;
	}
	case CAM_JPEG_CMD_DUMP_HW_MISR_VAL:
	{
		if (hw_info->camnoc_misr_support) {
			rc = cam_jpeg_enc_dump_hw_misr_val(hw_info, soc_info, cmd_args);
			if (rc)
				break;
			rc = cam_jpeg_enc_dump_camnoc_misr_val(hw_info, soc_info, cmd_args);
		} else {
			CAM_DBG(CAM_JPEG, "camnoc misr is not supported");
		}
		break;
	}
	default:
		rc = -EINVAL;
		break;
	}
	if (rc)
		CAM_ERR(CAM_JPEG, "error cmdtype %d rc = %d", cmd_type, rc);
	return rc;
}
