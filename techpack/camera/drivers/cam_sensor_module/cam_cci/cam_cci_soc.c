// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include "cam_cci_dev.h"
#include "cam_cci_core.h"

static int cam_cci_init_master(struct cci_device *cci_dev,
	enum cci_i2c_master_t master)
{
	int i = 0, rc = 0;
	void __iomem *base = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	uint32_t max_queue_0_size = 0, max_queue_1_size = 0;

	soc_info = &cci_dev->soc_info;
	base = soc_info->reg_map[0].mem_base;

	if (cci_dev->hw_version == CCI_VERSION_1_2_9) {
		max_queue_0_size = CCI_I2C_QUEUE_0_SIZE_V_1_2;
		max_queue_1_size = CCI_I2C_QUEUE_1_SIZE_V_1_2;
	} else {
		max_queue_0_size = CCI_I2C_QUEUE_0_SIZE;
		max_queue_1_size = CCI_I2C_QUEUE_1_SIZE;
	}

	cci_dev->master_active_slave[master]++;
	if (!cci_dev->cci_master_info[master].is_initilized) {
		/* Re-initialize the completion */
		reinit_completion(
		&cci_dev->cci_master_info[master].reset_complete);
		reinit_completion(&cci_dev->cci_master_info[master].rd_done);

		/* reinit the reports for the queue */
		for (i = 0; i < NUM_QUEUES; i++)
			reinit_completion(
			&cci_dev->cci_master_info[master].report_q[i]);

		/* Set reset pending flag to true */
		cci_dev->cci_master_info[master].reset_pending = true;
		cci_dev->cci_master_info[master].status = 0;
		if (cci_dev->ref_count == 1) {
			cam_io_w_mb(CCI_RESET_CMD_RMSK,
				base + CCI_RESET_CMD_ADDR);
			cam_io_w_mb(0x1, base + CCI_RESET_CMD_ADDR);
		} else {
			cam_io_w_mb((master == MASTER_0) ?
				CCI_M0_RESET_RMSK : CCI_M1_RESET_RMSK,
				base + CCI_RESET_CMD_ADDR);
		}
		if (!wait_for_completion_timeout(
			&cci_dev->cci_master_info[master].reset_complete,
			CCI_TIMEOUT)) {
			CAM_ERR(CAM_CCI,
				"Failed: reset complete timeout for master: %d",
				master);
			rc = -ETIMEDOUT;
			cci_dev->master_active_slave[master]--;
			return rc;
		}

		flush_workqueue(cci_dev->write_wq[master]);

		/* Setting up the queue size for master */
		cci_dev->cci_i2c_queue_info[master][QUEUE_0].max_queue_size
					= max_queue_0_size;
		cci_dev->cci_i2c_queue_info[master][QUEUE_1].max_queue_size
					= max_queue_1_size;

		CAM_DBG(CAM_CCI, "CCI Master[%d] :: Q0: %d Q1: %d", master,
			cci_dev->cci_i2c_queue_info[master][QUEUE_0]
				.max_queue_size,
			cci_dev->cci_i2c_queue_info[master][QUEUE_1]
				.max_queue_size);

		cci_dev->cci_master_info[master].status = 0;
		cci_dev->cci_master_info[master].is_initilized = true;
		cci_dev->is_burst_read[master] = false;
	}

	return 0;
}

int cam_cci_init(struct v4l2_subdev *sd,
	struct cam_cci_ctrl *c_ctrl)
{
	uint8_t i = 0;
	int32_t rc = 0;
	struct cci_device *cci_dev;
	enum cci_i2c_master_t master = c_ctrl->cci_info->cci_i2c_master;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote = {0};
	struct cam_hw_soc_info *soc_info = NULL;
	void __iomem *base = NULL;

	cci_dev = v4l2_get_subdevdata(sd);
	if (!cci_dev || !c_ctrl) {
		CAM_ERR(CAM_CCI,
			"failed: invalid params cci_dev:%pK, c_ctrl:%pK",
			cci_dev, c_ctrl);
		rc = -EINVAL;
		return rc;
	}

	soc_info = &cci_dev->soc_info;
	base = soc_info->reg_map[0].mem_base;

	if (!soc_info || !base) {
		CAM_ERR(CAM_CCI,
			"failed: invalid params soc_info:%pK, base:%pK",
			soc_info, base);
		rc = -EINVAL;
		return rc;
	}

	if (master >= MASTER_MAX || master < 0) {
		CAM_ERR(CAM_CCI, "Incorrect Master: %d", master);
		return -EINVAL;
	}

	if (!cci_dev->write_wq[master]) {
		CAM_ERR(CAM_CCI, "Null memory for write wq[:%d]", master);
		rc = -ENOMEM;
		return rc;
	}

	if (cci_dev->ref_count++) {
		rc = cam_cci_init_master(cci_dev, master);
		if (rc) {
			CAM_ERR(CAM_CCI, "Failed to init: Master: %d: rc: %d",
				master, rc);
			cci_dev->ref_count--;
		}
		CAM_DBG(CAM_CCI, "ref_count %d, master: %d",
			cci_dev->ref_count, master);
		return rc;
	}

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_LOWSVS_VOTE;
	axi_vote.num_paths = 1;
	axi_vote.axi_path[0].path_data_type = CAM_AXI_PATH_DATA_ALL;
	axi_vote.axi_path[0].transac_type = CAM_AXI_TRANSACTION_WRITE;
	axi_vote.axi_path[0].camnoc_bw = CAM_CPAS_DEFAULT_AXI_BW;
	axi_vote.axi_path[0].mnoc_ab_bw = CAM_CPAS_DEFAULT_AXI_BW;
	axi_vote.axi_path[0].mnoc_ib_bw = CAM_CPAS_DEFAULT_AXI_BW;

	rc = cam_cpas_start(cci_dev->cpas_handle, &ahb_vote, &axi_vote);
	if (rc) {
		CAM_ERR(CAM_CCI, "CPAS start failed rc= %d", rc);
		return rc;
	}

	cam_cci_get_clk_rates(cci_dev, c_ctrl);

	/* Enable Regulators and IRQ*/
	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_LOWSVS_VOTE, true);
	if (rc < 0) {
		CAM_DBG(CAM_CCI, "request platform resources failed, rc: %d",
			rc);
		goto platform_enable_failed;
	}

	cci_dev->hw_version = cam_io_r_mb(base + CCI_HW_VERSION_ADDR);
	CAM_DBG(CAM_CCI, "hw_version = 0x%x", cci_dev->hw_version);

	cci_dev->payload_size = MSM_CCI_WRITE_DATA_PAYLOAD_SIZE_11;
	cci_dev->support_seq_write = 1;

	rc = cam_cci_init_master(cci_dev, master);
	if (rc) {
		CAM_ERR(CAM_CCI, "Failed to init: Master: %d, rc: %d",
			master, rc);
		goto reset_complete_failed;
	}

	for (i = 0; i < MASTER_MAX; i++)
		cci_dev->i2c_freq_mode[i] = I2C_MAX_MODES;

	cam_io_w_mb(CCI_IRQ_MASK_0_RMSK, base + CCI_IRQ_MASK_0_ADDR);
	cam_io_w_mb(CCI_IRQ_MASK_0_RMSK, base + CCI_IRQ_CLEAR_0_ADDR);
	cam_io_w_mb(CCI_IRQ_MASK_1_RMSK, base + CCI_IRQ_MASK_1_ADDR);
	cam_io_w_mb(CCI_IRQ_MASK_1_RMSK, base + CCI_IRQ_CLEAR_1_ADDR);
	cam_io_w_mb(0x1, base + CCI_IRQ_GLOBAL_CLEAR_CMD_ADDR);

	/* Set RD FIFO threshold for M0 & M1 */
	if (cci_dev->hw_version != CCI_VERSION_1_2_9) {
		cam_io_w_mb(CCI_I2C_RD_THRESHOLD_VALUE,
				base + CCI_I2C_M0_RD_THRESHOLD_ADDR);
		cam_io_w_mb(CCI_I2C_RD_THRESHOLD_VALUE,
				base + CCI_I2C_M1_RD_THRESHOLD_ADDR);
	}

	cci_dev->cci_state = CCI_STATE_ENABLED;

	return 0;

reset_complete_failed:
	cam_soc_util_disable_platform_resource(soc_info, 1, 1);
platform_enable_failed:
	cci_dev->ref_count--;
	cam_cpas_stop(cci_dev->cpas_handle);

	return rc;
}

void cam_cci_soc_remove(struct platform_device *pdev,
	struct cci_device *cci_dev)
{
	struct cam_hw_soc_info *soc_info = &cci_dev->soc_info;

	cam_soc_util_release_platform_resource(soc_info);
}

static void cam_cci_init_cci_params(struct cci_device *new_cci_dev)
{
	uint8_t i = 0, j = 0;

	for (i = 0; i < MASTER_MAX; i++) {
		new_cci_dev->cci_master_info[i].status = 0;
		new_cci_dev->cci_master_info[i].freq_ref_cnt = 0;
		new_cci_dev->cci_master_info[i].is_initilized = false;
		mutex_init(&new_cci_dev->cci_master_info[i].mutex);
		sema_init(&new_cci_dev->cci_master_info[i].master_sem, 1);
		spin_lock_init(&new_cci_dev->cci_master_info[i].freq_cnt_lock);
		init_completion(
			&new_cci_dev->cci_master_info[i].reset_complete);
		init_completion(
			&new_cci_dev->cci_master_info[i].th_complete);
		init_completion(
			&new_cci_dev->cci_master_info[i].rd_done);

		for (j = 0; j < NUM_QUEUES; j++) {
			mutex_init(&new_cci_dev->cci_master_info[i].mutex_q[j]);
			init_completion(
				&new_cci_dev->cci_master_info[i].report_q[j]);
			spin_lock_init(
				&new_cci_dev->cci_master_info[i].lock_q[j]);
		}
	}
	spin_lock_init(&new_cci_dev->lock_status);
}

static void cam_cci_init_default_clk_params(struct cci_device *cci_dev,
	uint8_t index)
{
	/* default clock params are for 100Khz */
	cci_dev->cci_clk_params[index].hw_thigh = 201;
	cci_dev->cci_clk_params[index].hw_tlow = 174;
	cci_dev->cci_clk_params[index].hw_tsu_sto = 204;
	cci_dev->cci_clk_params[index].hw_tsu_sta = 231;
	cci_dev->cci_clk_params[index].hw_thd_dat = 22;
	cci_dev->cci_clk_params[index].hw_thd_sta = 162;
	cci_dev->cci_clk_params[index].hw_tbuf = 227;
	cci_dev->cci_clk_params[index].hw_scl_stretch_en = 0;
	cci_dev->cci_clk_params[index].hw_trdhld = 6;
	cci_dev->cci_clk_params[index].hw_tsp = 3;
	cci_dev->cci_clk_params[index].cci_clk_src = 37500000;
}

static void cam_cci_init_clk_params(struct cci_device *cci_dev)
{
	int32_t rc = 0;
	uint32_t val = 0;
	uint8_t count = 0;
	struct device_node *of_node = cci_dev->v4l2_dev_str.pdev->dev.of_node;
	struct device_node *src_node = NULL;

	for (count = 0; count < I2C_MAX_MODES; count++) {

		if (count == I2C_STANDARD_MODE)
			src_node = of_find_node_by_name(of_node,
				"qcom,i2c_standard_mode");
		else if (count == I2C_FAST_MODE)
			src_node = of_find_node_by_name(of_node,
				"qcom,i2c_fast_mode");
		else if (count == I2C_FAST_PLUS_MODE)
			src_node = of_find_node_by_name(of_node,
				"qcom,i2c_fast_plus_mode");
		else
			src_node = of_find_node_by_name(of_node,
				"qcom,i2c_custom_mode");

		rc = of_property_read_u32(src_node, "hw-thigh", &val);
		CAM_DBG(CAM_CCI, "hw-thigh %d, rc %d", val, rc);
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_thigh = val;
			rc = of_property_read_u32(src_node, "hw-tlow",
				&val);
			CAM_DBG(CAM_CCI, "hw-tlow %d, rc %d",
				val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tlow = val;
			rc = of_property_read_u32(src_node, "hw-tsu-sto",
				&val);
			CAM_DBG(CAM_CCI, "hw-tsu-sto %d, rc %d",
				val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tsu_sto = val;
			rc = of_property_read_u32(src_node, "hw-tsu-sta",
				&val);
			CAM_DBG(CAM_CCI, "hw-tsu-sta %d, rc %d",
				val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tsu_sta = val;
			rc = of_property_read_u32(src_node, "hw-thd-dat",
				&val);
			CAM_DBG(CAM_CCI, "hw-thd-dat %d, rc %d",
				val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_thd_dat = val;
			rc = of_property_read_u32(src_node, "hw-thd-sta",
				&val);
			CAM_DBG(CAM_CCI, "hw-thd-sta %d, rc %d",
				val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_thd_sta = val;
			rc = of_property_read_u32(src_node, "hw-tbuf",
				&val);
			CAM_DBG(CAM_CCI, "hw-tbuf %d, rc %d",
				val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tbuf = val;
			rc = of_property_read_u32(src_node,
				"hw-scl-stretch-en", &val);
			CAM_DBG(CAM_CCI, "hw-scl-stretch-en %d, rc %d",
				val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_scl_stretch_en = val;
			rc = of_property_read_u32(src_node, "hw-trdhld",
				&val);
			CAM_DBG(CAM_CCI, "hw-trdhld %d, rc %d",
				val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_trdhld = val;
			rc = of_property_read_u32(src_node, "hw-tsp",
				&val);
			CAM_DBG(CAM_CCI, "hw-tsp %d, rc %d", val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tsp = val;
			val = 0;
			rc = of_property_read_u32(src_node, "cci-clk-src",
				&val);
			CAM_DBG(CAM_CCI, "cci-clk-src %d, rc %d", val, rc);
			cci_dev->cci_clk_params[count].cci_clk_src = val;
		} else
			cam_cci_init_default_clk_params(cci_dev, count);

		of_node_put(src_node);
	}
}

int cam_cci_parse_dt_info(struct platform_device *pdev,
	struct cci_device *new_cci_dev)
{
	int rc = 0, i = 0;
	struct cam_hw_soc_info *soc_info =
		&new_cci_dev->soc_info;

	rc = cam_soc_util_get_dt_properties(soc_info);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "Parsing DT data failed:%d", rc);
		return -EINVAL;
	}

	new_cci_dev->ref_count = 0;

	rc = cam_soc_util_request_platform_resource(soc_info,
		cam_cci_irq, new_cci_dev);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "requesting platform resources failed:%d", rc);
		return -EINVAL;
	}
	new_cci_dev->v4l2_dev_str.pdev = pdev;
	cam_cci_init_cci_params(new_cci_dev);
	cam_cci_init_clk_params(new_cci_dev);

	for (i = 0; i < MASTER_MAX; i++) {
		new_cci_dev->write_wq[i] = create_singlethread_workqueue(
			"cam_cci_wq");
		if (!new_cci_dev->write_wq[i])
			CAM_ERR(CAM_CCI, "Failed to create write wq");
	}
	CAM_DBG(CAM_CCI, "Exit");
	return 0;
}

int cam_cci_soc_release(struct cci_device *cci_dev,
	enum cci_i2c_master_t master)
{
	uint8_t i = 0, rc = 0;
	struct cam_hw_soc_info *soc_info = &cci_dev->soc_info;

	if (!cci_dev->ref_count || cci_dev->cci_state != CCI_STATE_ENABLED ||
			!cci_dev->master_active_slave[master]) {
		CAM_ERR(CAM_CCI,
			"invalid cci_dev_ref count %u | cci state %d | master_ref_count %u",
			cci_dev->ref_count, cci_dev->cci_state,
			cci_dev->master_active_slave[master]);
		return -EINVAL;
	}

	if (!(--cci_dev->master_active_slave[master])) {
		cci_dev->cci_master_info[master].is_initilized = false;
		CAM_DBG(CAM_CCI,
			"All submodules are released for master: %d", master);
	}

	if (--cci_dev->ref_count) {
		CAM_DBG(CAM_CCI, "Submodule release: Ref_count: %d",
			cci_dev->ref_count);
		return 0;
	}

	for (i = 0; i < MASTER_MAX; i++) {
		if (cci_dev->write_wq[i])
			flush_workqueue(cci_dev->write_wq[i]);
		cci_dev->i2c_freq_mode[i] = I2C_MAX_MODES;
	}

	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc) {
		CAM_ERR(CAM_CCI, "platform resources disable failed, rc=%d",
			rc);
		return rc;
	}

	cci_dev->cci_state = CCI_STATE_DISABLED;
	cci_dev->cycles_per_us = 0;

	cam_cpas_stop(cci_dev->cpas_handle);

	return rc;
}
