// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#include <linux/module.h>
#include "cam_cci_core.h"
#include "cam_cci_dev.h"


static int disable_optmz;
module_param(disable_optmz, int, 0644);

static int32_t cam_cci_convert_type_to_num_bytes(
	enum camera_sensor_i2c_type type)
{
	int32_t num_bytes;

	switch (type) {
	case CAMERA_SENSOR_I2C_TYPE_BYTE:
		num_bytes = 1;
		break;
	case CAMERA_SENSOR_I2C_TYPE_WORD:
		num_bytes = 2;
		break;
	case CAMERA_SENSOR_I2C_TYPE_3B:
		num_bytes = 3;
		break;
	case CAMERA_SENSOR_I2C_TYPE_DWORD:
		num_bytes = 4;
		break;
	default:
		CAM_ERR(CAM_CCI, "failed: %d", type);
		num_bytes = 0;
		break;
	}
	return num_bytes;
}

static void cam_cci_flush_queue(struct cci_device *cci_dev,
	enum cci_i2c_master_t master)
{
	int32_t rc = 0;
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;
	void __iomem *base = soc_info->reg_map[0].mem_base;

	cam_io_w_mb(1 << master, base + CCI_HALT_REQ_ADDR);
	if (!cci_dev->cci_master_info[master].status)
		reinit_completion(&cci_dev->cci_master_info[master]
			.reset_complete);
	rc = wait_for_completion_timeout(
		&cci_dev->cci_master_info[master].reset_complete, CCI_TIMEOUT);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "wait failed");
	} else if (rc == 0) {
		CAM_ERR(CAM_CCI, "wait timeout");

		/* Set reset pending flag to true */
		cci_dev->cci_master_info[master].reset_pending = true;
		cci_dev->cci_master_info[master].status = 0;

		/* Set proper mask to RESET CMD address based on MASTER */
		if (master == MASTER_0)
			cam_io_w_mb(CCI_M0_RESET_RMSK,
				base + CCI_RESET_CMD_ADDR);
		else
			cam_io_w_mb(CCI_M1_RESET_RMSK,
				base + CCI_RESET_CMD_ADDR);

		/* wait for reset done irq */
		rc = wait_for_completion_timeout(
			&cci_dev->cci_master_info[master].reset_complete,
			CCI_TIMEOUT);
		if (rc <= 0)
			CAM_ERR(CAM_CCI, "wait failed %d", rc);
		cci_dev->cci_master_info[master].status = 0;
	}
}

static int32_t cam_cci_validate_queue(struct cci_device *cci_dev,
	uint32_t len,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	int32_t rc = 0;
	uint32_t read_val = 0;
	uint32_t reg_offset = master * 0x200 + queue * 0x100;
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;
	void __iomem *base = soc_info->reg_map[0].mem_base;
	unsigned long flags;

	read_val = cam_io_r_mb(base +
		CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR + reg_offset);
	CAM_DBG(CAM_CCI, "CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR %d len %d max %d",
		read_val, len,
		cci_dev->cci_i2c_queue_info[master][queue].max_queue_size);
	if ((read_val + len + 1) >
		cci_dev->cci_i2c_queue_info[master][queue].max_queue_size) {
		uint32_t reg_val = 0;
		uint32_t report_val = CCI_I2C_REPORT_CMD | (1 << 8);

		CAM_DBG(CAM_CCI, "CCI_I2C_REPORT_CMD");
		cam_io_w_mb(report_val,
			base + CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
			reg_offset);
		read_val++;
		CAM_DBG(CAM_CCI,
			"CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR %d, queue: %d",
			read_val, queue);
		cam_io_w_mb(read_val, base +
			CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR + reg_offset);
		reg_val = 1 << ((master * 2) + queue);
		CAM_DBG(CAM_CCI, "CCI_QUEUE_START_ADDR");
		spin_lock_irqsave(
			&cci_dev->cci_master_info[master].lock_q[queue], flags);
		atomic_set(
			&cci_dev->cci_master_info[master].done_pending[queue],
			1);
		cam_io_w_mb(reg_val, base +
			CCI_QUEUE_START_ADDR);
		CAM_DBG(CAM_CCI, "wait_for_completion_timeout");
		atomic_set(&cci_dev->cci_master_info[master].q_free[queue], 1);
		spin_unlock_irqrestore(
			&cci_dev->cci_master_info[master].lock_q[queue], flags);
		rc = wait_for_completion_timeout(
			&cci_dev->cci_master_info[master].report_q[queue],
			CCI_TIMEOUT);
		if (rc <= 0) {
			CAM_ERR(CAM_CCI, "Wait_for_completion_timeout: rc: %d",
				rc);
			if (rc == 0)
				rc = -ETIMEDOUT;
			cam_cci_flush_queue(cci_dev, master);
			return rc;
		}
		rc = cci_dev->cci_master_info[master].status;
		if (rc < 0) {
			CAM_ERR(CAM_CCI, "Failed rc %d", rc);
			cci_dev->cci_master_info[master].status = 0;
		}
	}

	return rc;
}

static int32_t cam_cci_write_i2c_queue(struct cci_device *cci_dev,
	uint32_t val,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	int32_t rc = 0;
	uint32_t reg_offset = master * 0x200 + queue * 0x100;
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;
	void __iomem *base = soc_info->reg_map[0].mem_base;

	if (!cci_dev) {
		CAM_ERR(CAM_CCI, "Failed");
		return -EINVAL;
	}

	rc = cam_cci_validate_queue(cci_dev, 1, master, queue);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "Failed %d", rc);
		return rc;
	}
	CAM_DBG(CAM_CCI, "CCI_I2C_M0_Q0_LOAD_DATA_ADDR:val 0x%x:0x%x",
		CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
		reg_offset, val);
	cam_io_w_mb(val, base + CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
		reg_offset);
	return rc;
}

static int32_t cam_cci_lock_queue(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue, uint32_t en)
{
	uint32_t val;

	if (queue != PRIORITY_QUEUE)
		return 0;

	val = en ? CCI_I2C_LOCK_CMD : CCI_I2C_UNLOCK_CMD;
	return cam_cci_write_i2c_queue(cci_dev, val, master, queue);
}

#ifdef DUMP_CCI_REGISTERS
static void cam_cci_dump_registers(struct cci_device *cci_dev,
	enum cci_i2c_master_t master, enum cci_i2c_queue_t queue)
{
	uint32_t read_val = 0;
	uint32_t i = 0;
	uint32_t reg_offset = 0;
	void __iomem *base = cci_dev->soc_info.reg_map[0].mem_base;

	/* CCI Top Registers */
	CAM_INFO(CAM_CCI, "****CCI TOP Registers ****");
	for (i = 0; i < DEBUG_TOP_REG_COUNT; i++) {
		reg_offset = DEBUG_TOP_REG_START + i * 4;
		read_val = cam_io_r_mb(base + reg_offset);
		CAM_INFO(CAM_CCI, "offset = 0x%X value = 0x%X",
			reg_offset, read_val);
	}

	/* CCI Master registers */
	CAM_INFO(CAM_CCI, "****CCI MASTER %d Registers ****",
		master);
	for (i = 0; i < DEBUG_MASTER_REG_COUNT; i++) {
		reg_offset = DEBUG_MASTER_REG_START + master*0x100 + i * 4;
		read_val = cam_io_r_mb(base + reg_offset);
		CAM_INFO(CAM_CCI, "offset = 0x%X value = 0x%X",
			reg_offset, read_val);
	}

	/* CCI Master Queue registers */
	CAM_INFO(CAM_CCI, " **** CCI MASTER%d QUEUE%d Registers ****",
		master, queue);
	for (i = 0; i < DEBUG_MASTER_QUEUE_REG_COUNT; i++) {
		reg_offset = DEBUG_MASTER_QUEUE_REG_START +  master*0x200 +
			queue*0x100 + i * 4;
		read_val = cam_io_r_mb(base + reg_offset);
		CAM_INFO(CAM_CCI, "offset = 0x%X value = 0x%X",
			reg_offset, read_val);
	}

	/* CCI Interrupt registers */
	CAM_INFO(CAM_CCI, " ****CCI Interrupt Registers ****");
	for (i = 0; i < DEBUG_INTR_REG_COUNT; i++) {
		reg_offset = DEBUG_INTR_REG_START + i * 4;
		read_val = cam_io_r_mb(base + reg_offset);
		CAM_INFO(CAM_CCI, "offset = 0x%X value = 0x%X",
			reg_offset, read_val);
	}
}
#endif

static uint32_t cam_cci_wait(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	int32_t rc = 0;

	if (!cci_dev) {
		CAM_ERR(CAM_CCI, "failed");
		return -EINVAL;
	}

	rc = wait_for_completion_timeout(
		&cci_dev->cci_master_info[master].report_q[queue], CCI_TIMEOUT);
	CAM_DBG(CAM_CCI, "wait DONE_for_completion_timeout");

	if (rc <= 0) {
#ifdef DUMP_CCI_REGISTERS
		cam_cci_dump_registers(cci_dev, master, queue);
#endif
		CAM_ERR(CAM_CCI, "wait for queue: %d", queue);
		if (rc == 0) {
			rc = -ETIMEDOUT;
			cam_cci_flush_queue(cci_dev, master);
			return rc;
		}
	}
	rc = cci_dev->cci_master_info[master].status;
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "failed rc %d", rc);
		cci_dev->cci_master_info[master].status = 0;
		return rc;
	}

	return 0;
}

static void cam_cci_load_report_cmd(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;
	void __iomem *base = soc_info->reg_map[0].mem_base;

	uint32_t reg_offset = master * 0x200 + queue * 0x100;
	uint32_t read_val = cam_io_r_mb(base +
		CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR + reg_offset);
	uint32_t report_val = CCI_I2C_REPORT_CMD | (1 << 8);

	CAM_DBG(CAM_CCI, "CCI_I2C_REPORT_CMD curr_w_cnt: %d", read_val);
	cam_io_w_mb(report_val,
		base + CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
		reg_offset);
	read_val++;

	CAM_DBG(CAM_CCI, "CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR %d", read_val);
	cam_io_w_mb(read_val, base +
		CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR + reg_offset);
}

static int32_t cam_cci_wait_report_cmd(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	unsigned long flags;
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;
	void __iomem *base = soc_info->reg_map[0].mem_base;

	uint32_t reg_val = 1 << ((master * 2) + queue);

	cam_cci_load_report_cmd(cci_dev, master, queue);
	spin_lock_irqsave(
		&cci_dev->cci_master_info[master].lock_q[queue], flags);
	atomic_set(&cci_dev->cci_master_info[master].q_free[queue], 1);
	atomic_set(&cci_dev->cci_master_info[master].done_pending[queue], 1);
	spin_unlock_irqrestore(
		&cci_dev->cci_master_info[master].lock_q[queue], flags);
	cam_io_w_mb(reg_val, base +
		CCI_QUEUE_START_ADDR);

	return cam_cci_wait(cci_dev, master, queue);
}

static int32_t cam_cci_transfer_end(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	int32_t rc = 0;
	unsigned long flags;

	spin_lock_irqsave(
		&cci_dev->cci_master_info[master].lock_q[queue], flags);
	if (atomic_read(&cci_dev->cci_master_info[master].q_free[queue]) == 0) {
		spin_unlock_irqrestore(
			&cci_dev->cci_master_info[master].lock_q[queue], flags);
		rc = cam_cci_lock_queue(cci_dev, master, queue, 0);
		if (rc < 0) {
			CAM_ERR(CAM_CCI, "failed rc: %d", rc);
			return rc;
		}
		rc = cam_cci_wait_report_cmd(cci_dev, master, queue);
		if (rc < 0) {
			CAM_ERR(CAM_CCI, "failed rc %d", rc);
			return rc;
		}
	} else {
		atomic_set(
			&cci_dev->cci_master_info[master].done_pending[queue],
			1);
		spin_unlock_irqrestore(
			&cci_dev->cci_master_info[master].lock_q[queue], flags);
		rc = cam_cci_wait(cci_dev, master, queue);
		if (rc < 0) {
			CAM_ERR(CAM_CCI, "failed rc %d", rc);
			return rc;
		}
		rc = cam_cci_lock_queue(cci_dev, master, queue, 0);
		if (rc < 0) {
			CAM_ERR(CAM_CCI, "failed rc %d", rc);
			return rc;
		}
		rc = cam_cci_wait_report_cmd(cci_dev, master, queue);
		if (rc < 0) {
			CAM_ERR(CAM_CCI, "Failed rc %d", rc);
			return rc;
		}
	}

	return rc;
}

static int32_t cam_cci_get_queue_free_size(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	uint32_t read_val = 0;
	uint32_t reg_offset = master * 0x200 + queue * 0x100;
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;
	void __iomem *base = soc_info->reg_map[0].mem_base;

	read_val = cam_io_r_mb(base +
		CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR + reg_offset);
	CAM_DBG(CAM_CCI, "CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR %d max %d", read_val,
		cci_dev->cci_i2c_queue_info[master][queue].max_queue_size);
	return ((cci_dev->cci_i2c_queue_info[master][queue].max_queue_size) -
			read_val);
}

static void cam_cci_process_half_q(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	unsigned long flags;
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;
	void __iomem *base = soc_info->reg_map[0].mem_base;
	uint32_t reg_val = 1 << ((master * 2) + queue);

	spin_lock_irqsave(&cci_dev->cci_master_info[master].lock_q[queue],
		flags);
	if (atomic_read(&cci_dev->cci_master_info[master].q_free[queue]) == 0) {
		cam_cci_load_report_cmd(cci_dev, master, queue);
		atomic_set(&cci_dev->cci_master_info[master].q_free[queue], 1);
		cam_io_w_mb(reg_val, base +
			CCI_QUEUE_START_ADDR);
	}
	spin_unlock_irqrestore(&cci_dev->cci_master_info[master].lock_q[queue],
		flags);
}

static int32_t cam_cci_process_full_q(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	int32_t rc = 0;
	unsigned long flags;


	spin_lock_irqsave(&cci_dev->cci_master_info[master].lock_q[queue],
		flags);
	if (atomic_read(&cci_dev->cci_master_info[master].q_free[queue]) == 1) {
		atomic_set(
			&cci_dev->cci_master_info[master].done_pending[queue],
			1);
		spin_unlock_irqrestore(
			&cci_dev->cci_master_info[master].lock_q[queue], flags);
		rc = cam_cci_wait(cci_dev, master, queue);
		if (rc < 0) {
			CAM_ERR(CAM_CCI, "failed rc %d", rc);
			return rc;
		}
	} else {
		spin_unlock_irqrestore(
			&cci_dev->cci_master_info[master].lock_q[queue], flags);
		rc = cam_cci_wait_report_cmd(cci_dev, master, queue);
		if (rc < 0) {
			CAM_ERR(CAM_CCI, "failed rc %d", rc);
			return rc;
		}
	}

	return rc;
}

static int32_t cam_cci_calc_cmd_len(struct cci_device *cci_dev,
	struct cam_cci_ctrl *c_ctrl, uint32_t cmd_size,
	 struct cam_sensor_i2c_reg_array *i2c_cmd, uint32_t *pack)
{
	uint8_t i;
	struct cam_sensor_i2c_reg_array *cmd = i2c_cmd;
	uint32_t len = 0;
	uint8_t data_len = 0, addr_len = 0;
	uint8_t pack_max_len;
	struct cam_sensor_i2c_reg_setting *msg;
	uint32_t size = cmd_size;

	if (!cci_dev || !c_ctrl) {
		CAM_ERR(CAM_CCI, "failed");
		return -EINVAL;
	}

	msg = &c_ctrl->cfg.cci_i2c_write_cfg;
	*pack = 0;

	if (c_ctrl->cmd == MSM_CCI_I2C_WRITE_SEQ ||
		c_ctrl->cmd == MSM_CCI_I2C_WRITE_BURST) {
		addr_len = cam_cci_convert_type_to_num_bytes(msg->addr_type);
		len = (size + addr_len) <= (cci_dev->payload_size) ?
			(size + addr_len):cci_dev->payload_size;
	} else {
		addr_len = cam_cci_convert_type_to_num_bytes(msg->addr_type);
		data_len = cam_cci_convert_type_to_num_bytes(msg->data_type);
		len = data_len + addr_len;
		pack_max_len = size < (cci_dev->payload_size-len) ?
			size : (cci_dev->payload_size-len);
		/* xiaomi add a flag to disable this optimization*/
		if ((!c_ctrl->cci_info->disable_optmz) && (!disable_optmz))
		{
			CAM_DBG(CAM_CCI, "enable writing optimization for 0x%02X", c_ctrl->cci_info->sid<<1);
			for (i = 0; i < pack_max_len;) {
				if (cmd->delay || ((cmd - i2c_cmd) >= (cmd_size - 1)))
					break;
				if (cmd->reg_addr + 1 ==
					(cmd+1)->reg_addr) {
					len += data_len;
					if (len > cci_dev->payload_size) {
						len = len - data_len;
						break;
					}
					(*pack)++;
				} else {
					break;
				}
				i += data_len;
				cmd++;
			}
		}
	}

	if (len > cci_dev->payload_size) {
		CAM_ERR(CAM_CCI, "Len error: %d", len);
		return -EINVAL;
	}

	len += 1; /*add i2c WR command*/
	len = len/4 + 1;

	return len;
}

static uint32_t cam_cci_cycles_per_ms(unsigned long clk)
{
	uint32_t cycles_per_us;

	if (clk) {
		cycles_per_us = ((clk/1000)*256)/1000;
	} else {
		CAM_ERR(CAM_CCI, "failed: Can use default: %d",
			CYCLES_PER_MICRO_SEC_DEFAULT);
		cycles_per_us = CYCLES_PER_MICRO_SEC_DEFAULT;
	}

	return cycles_per_us;
}

void cam_cci_get_clk_rates(struct cci_device *cci_dev,
	struct cam_cci_ctrl *c_ctrl)

{
	int32_t src_clk_idx, j;
	uint32_t cci_clk_src;
	unsigned long clk;
	struct cam_cci_clk_params_t *clk_params = NULL;

	enum i2c_freq_mode i2c_freq_mode = c_ctrl->cci_info->i2c_freq_mode;
	struct cam_hw_soc_info *soc_info = &cci_dev->soc_info;

	if (i2c_freq_mode >= I2C_MAX_MODES ||
		i2c_freq_mode < I2C_STANDARD_MODE) {
		CAM_ERR(CAM_CCI, "Invalid frequency mode: %d",
			(int32_t)i2c_freq_mode);
		cci_dev->clk_level_index = -1;
		return;
	}

	clk_params = &cci_dev->cci_clk_params[i2c_freq_mode];
	cci_clk_src = clk_params->cci_clk_src;

	src_clk_idx = soc_info->src_clk_idx;

	if (src_clk_idx < 0) {
		cci_dev->cycles_per_us = CYCLES_PER_MICRO_SEC_DEFAULT;
		cci_dev->clk_level_index = 0;
		return;
	}

	if (cci_clk_src == 0) {
		clk = soc_info->clk_rate[0][src_clk_idx];
		cci_dev->cycles_per_us = cam_cci_cycles_per_ms(clk);
		cci_dev->clk_level_index = 0;
		return;
	}

	for (j = 0; j < CAM_MAX_VOTE; j++) {
		clk = soc_info->clk_rate[j][src_clk_idx];
		if (clk == cci_clk_src) {
			cci_dev->cycles_per_us = cam_cci_cycles_per_ms(clk);
			cci_dev->clk_level_index = j;
			return;
		}
	}
}

static int32_t cam_cci_set_clk_param(struct cci_device *cci_dev,
	struct cam_cci_ctrl *c_ctrl)
{
	struct cam_cci_clk_params_t *clk_params = NULL;
	enum cci_i2c_master_t master = c_ctrl->cci_info->cci_i2c_master;
	enum i2c_freq_mode i2c_freq_mode = c_ctrl->cci_info->i2c_freq_mode;
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;
	void __iomem *base = soc_info->reg_map[0].mem_base;

	if ((i2c_freq_mode >= I2C_MAX_MODES) || (i2c_freq_mode < 0)) {
		CAM_ERR(CAM_CCI, "invalid i2c_freq_mode = %d", i2c_freq_mode);
		return -EINVAL;
	}

	clk_params = &cci_dev->cci_clk_params[i2c_freq_mode];

	if (cci_dev->i2c_freq_mode[master] == i2c_freq_mode)
		return 0;
	if (master == MASTER_0) {
		cam_io_w_mb(clk_params->hw_thigh << 16 |
			clk_params->hw_tlow,
			base + CCI_I2C_M0_SCL_CTL_ADDR);
		cam_io_w_mb(clk_params->hw_tsu_sto << 16 |
			clk_params->hw_tsu_sta,
			base + CCI_I2C_M0_SDA_CTL_0_ADDR);
		cam_io_w_mb(clk_params->hw_thd_dat << 16 |
			clk_params->hw_thd_sta,
			base + CCI_I2C_M0_SDA_CTL_1_ADDR);
		cam_io_w_mb(clk_params->hw_tbuf,
			base + CCI_I2C_M0_SDA_CTL_2_ADDR);
		cam_io_w_mb(clk_params->hw_scl_stretch_en << 8 |
			clk_params->hw_trdhld << 4 | clk_params->hw_tsp,
			base + CCI_I2C_M0_MISC_CTL_ADDR);
	} else if (master == MASTER_1) {
		cam_io_w_mb(clk_params->hw_thigh << 16 |
			clk_params->hw_tlow,
			base + CCI_I2C_M1_SCL_CTL_ADDR);
		cam_io_w_mb(clk_params->hw_tsu_sto << 16 |
			clk_params->hw_tsu_sta,
			base + CCI_I2C_M1_SDA_CTL_0_ADDR);
		cam_io_w_mb(clk_params->hw_thd_dat << 16 |
			clk_params->hw_thd_sta,
			base + CCI_I2C_M1_SDA_CTL_1_ADDR);
		cam_io_w_mb(clk_params->hw_tbuf,
			base + CCI_I2C_M1_SDA_CTL_2_ADDR);
		cam_io_w_mb(clk_params->hw_scl_stretch_en << 8 |
			clk_params->hw_trdhld << 4 | clk_params->hw_tsp,
			base + CCI_I2C_M1_MISC_CTL_ADDR);
	}
	cci_dev->i2c_freq_mode[master] = i2c_freq_mode;

	return 0;
}

static int32_t cam_cci_data_queue(struct cci_device *cci_dev,
	struct cam_cci_ctrl *c_ctrl, enum cci_i2c_queue_t queue,
	enum cci_i2c_sync sync_en)
{
	uint16_t i = 0, j = 0, k = 0, h = 0, len = 0;
	int32_t rc = 0, free_size = 0, en_seq_write = 0;
	uint8_t data[12];
	struct cam_sensor_i2c_reg_setting *i2c_msg =
		&c_ctrl->cfg.cci_i2c_write_cfg;
	struct cam_sensor_i2c_reg_array *i2c_cmd = i2c_msg->reg_setting;
	enum cci_i2c_master_t master = c_ctrl->cci_info->cci_i2c_master;
	uint16_t reg_addr = 0, cmd_size = i2c_msg->size;
	uint32_t read_val = 0, reg_offset, val, delay = 0;
	uint32_t max_queue_size, queue_size = 0, cmd = 0;
	struct cam_hw_soc_info *soc_info =
		&cci_dev->soc_info;
	void __iomem *base = soc_info->reg_map[0].mem_base;
	unsigned long flags;

	if (i2c_cmd == NULL) {
		CAM_ERR(CAM_CCI, "Failed: i2c cmd is NULL");
		return -EINVAL;
	}

	if ((!cmd_size) || (cmd_size > CCI_I2C_MAX_WRITE)) {
		CAM_ERR(CAM_CCI, "failed: invalid cmd_size %d",
			cmd_size);
		return -EINVAL;
	}

	CAM_DBG(CAM_CCI, "addr type %d data type %d cmd_size %d",
		i2c_msg->addr_type, i2c_msg->data_type, cmd_size);

	if (i2c_msg->addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX) {
		CAM_ERR(CAM_CCI, "failed: invalid addr_type 0x%X",
			i2c_msg->addr_type);
		return -EINVAL;
	}
	if (i2c_msg->data_type >= CAMERA_SENSOR_I2C_TYPE_MAX) {
		CAM_ERR(CAM_CCI, "failed: invalid data_type 0x%X",
			i2c_msg->data_type);
		return -EINVAL;
	}
	reg_offset = master * 0x200 + queue * 0x100;

	cam_io_w_mb(cci_dev->cci_wait_sync_cfg.cid,
		base + CCI_SET_CID_SYNC_TIMER_ADDR +
		cci_dev->cci_wait_sync_cfg.csid *
		CCI_SET_CID_SYNC_TIMER_OFFSET);

	val = CCI_I2C_SET_PARAM_CMD | c_ctrl->cci_info->sid << 4 |
		c_ctrl->cci_info->retries << 16 |
		c_ctrl->cci_info->id_map << 18;

	CAM_DBG(CAM_CCI, "CCI_I2C_M0_Q0_LOAD_DATA_ADDR:val 0x%x:0x%x",
		CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
		reg_offset, val);
	cam_io_w_mb(val, base + CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
		reg_offset);

	spin_lock_irqsave(&cci_dev->cci_master_info[master].lock_q[queue],
		flags);
	atomic_set(&cci_dev->cci_master_info[master].q_free[queue], 0);
	spin_unlock_irqrestore(&cci_dev->cci_master_info[master].lock_q[queue],
		flags);

	max_queue_size =
		cci_dev->cci_i2c_queue_info[master][queue].max_queue_size;

	if (c_ctrl->cmd == MSM_CCI_I2C_WRITE_SEQ)
		queue_size = max_queue_size;
	else
		queue_size = max_queue_size/2;
	reg_addr = i2c_cmd->reg_addr;

	if (sync_en == MSM_SYNC_ENABLE && cci_dev->valid_sync &&
		cmd_size < max_queue_size) {
		val = CCI_I2C_WAIT_SYNC_CMD |
			((cci_dev->cci_wait_sync_cfg.line) << 4);
		cam_io_w_mb(val,
			base + CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
			reg_offset);
	}

	rc = cam_cci_lock_queue(cci_dev, master, queue, 1);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "failed line %d", rc);
		return rc;
	}

	while (cmd_size) {
		uint32_t pack = 0;

		len = cam_cci_calc_cmd_len(cci_dev, c_ctrl, cmd_size,
			i2c_cmd, &pack);
		if (len <= 0) {
			CAM_ERR(CAM_CCI, "failed");
			return -EINVAL;
		}

		read_val = cam_io_r_mb(base +
			CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR + reg_offset);
		CAM_DBG(CAM_CCI, "CUR_WORD_CNT_ADDR %d len %d max %d",
			read_val, len, max_queue_size);
		/* + 1 - space alocation for Report CMD */
		if ((read_val + len + 1) > queue_size) {
			if ((read_val + len + 1) > max_queue_size) {
				rc = cam_cci_process_full_q(cci_dev,
					master, queue);
				if (rc < 0) {
					CAM_ERR(CAM_CCI, "failed rc: %d", rc);
					return rc;
				}
				continue;
			}
			cam_cci_process_half_q(cci_dev, master, queue);
		}

		CAM_DBG(CAM_CCI, "cmd_size %d addr 0x%x data 0x%x",
			cmd_size, i2c_cmd->reg_addr, i2c_cmd->reg_data);
		delay = i2c_cmd->delay;
		i = 0;
		data[i++] = CCI_I2C_WRITE_CMD;

		/*
		 * in case of multiple command
		 * MSM_CCI_I2C_WRITE : address is not continuous, so update
		 *	address for a new packet.
		 * MSM_CCI_I2C_WRITE_SEQ : address is continuous, need to keep
		 *	the incremented address for a
		 *	new packet
		 */
		if (c_ctrl->cmd == MSM_CCI_I2C_WRITE ||
			c_ctrl->cmd == MSM_CCI_I2C_WRITE_ASYNC ||
			c_ctrl->cmd == MSM_CCI_I2C_WRITE_SYNC ||
			c_ctrl->cmd == MSM_CCI_I2C_WRITE_SYNC_BLOCK)
			reg_addr = i2c_cmd->reg_addr;

		if (en_seq_write == 0) {
			/* either byte or word addr */
			if (i2c_msg->addr_type == CAMERA_SENSOR_I2C_TYPE_BYTE)
				data[i++] = reg_addr;
			else {
				data[i++] = (reg_addr & 0xFF00) >> 8;
				data[i++] = reg_addr & 0x00FF;
			}
		}
		/* max of 10 data bytes */
		do {
			if (i2c_msg->data_type == CAMERA_SENSOR_I2C_TYPE_BYTE) {
				data[i++] = i2c_cmd->reg_data;
				if (c_ctrl->cmd == MSM_CCI_I2C_WRITE_SEQ)
					reg_addr++;
			} else {
				if ((i + 1) <= cci_dev->payload_size) {
					switch (i2c_msg->data_type) {
					case CAMERA_SENSOR_I2C_TYPE_DWORD:
						data[i++] = (i2c_cmd->reg_data &
							0xFF000000) >> 24;
						/* fallthrough */
					case CAMERA_SENSOR_I2C_TYPE_3B:
						data[i++] = (i2c_cmd->reg_data &
							0x00FF0000) >> 16;
						/* fallthrough */
					case CAMERA_SENSOR_I2C_TYPE_WORD:
						data[i++] = (i2c_cmd->reg_data &
							0x0000FF00) >> 8;
						/* fallthrough */
					case CAMERA_SENSOR_I2C_TYPE_BYTE:
						data[i++] = i2c_cmd->reg_data &
							0x000000FF;
						break;
					default:
						CAM_ERR(CAM_CCI,
							"invalid data type: %d",
							i2c_msg->data_type);
						return -EINVAL;
					}

					if (c_ctrl->cmd ==
						MSM_CCI_I2C_WRITE_SEQ)
						reg_addr++;
				} else
					break;
			}
			i2c_cmd++;
			--cmd_size;
		} while (((c_ctrl->cmd == MSM_CCI_I2C_WRITE_SEQ ||
			c_ctrl->cmd == MSM_CCI_I2C_WRITE_BURST) || pack--) &&
				(cmd_size > 0) && (i <= cci_dev->payload_size));
		free_size = cam_cci_get_queue_free_size(cci_dev, master,
				queue);
		if ((c_ctrl->cmd == MSM_CCI_I2C_WRITE_SEQ ||
			c_ctrl->cmd == MSM_CCI_I2C_WRITE_BURST) &&
			((i-1) == MSM_CCI_WRITE_DATA_PAYLOAD_SIZE_11) &&
			cci_dev->support_seq_write && cmd_size > 0 &&
			free_size > BURST_MIN_FREE_SIZE) {
			data[0] |= 0xF0;
			en_seq_write = 1;
		} else {
			data[0] |= ((i-1) << 4);
			en_seq_write = 0;
		}
		len = ((i-1)/4) + 1;

		read_val = cam_io_r_mb(base +
			CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR + reg_offset);
		for (h = 0, k = 0; h < len; h++) {
			cmd = 0;
			for (j = 0; (j < 4 && k < i); j++)
				cmd |= (data[k++] << (j * 8));
			CAM_DBG(CAM_CCI,
				"LOAD_DATA_ADDR 0x%x, q: %d, len:%d, cnt: %d",
				cmd, queue, len, read_val);
			cam_io_w_mb(cmd, base +
				CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
				master * 0x200 + queue * 0x100);

			read_val += 1;
			cam_io_w_mb(read_val, base +
				CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR + reg_offset);
		}

		if ((delay > 0) && (delay < CCI_MAX_DELAY) &&
			en_seq_write == 0) {
			cmd = (uint32_t)((delay * cci_dev->cycles_per_us) /
				0x100);
			cmd <<= 4;
			cmd |= CCI_I2C_WAIT_CMD;
			CAM_DBG(CAM_CCI,
				"CCI_I2C_M0_Q0_LOAD_DATA_ADDR 0x%x", cmd);
			cam_io_w_mb(cmd, base +
				CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
				master * 0x200 + queue * 0x100);
			read_val += 1;
			cam_io_w_mb(read_val, base +
				CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR + reg_offset);
		}
	}

	rc = cam_cci_transfer_end(cci_dev, master, queue);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "Slave: 0x%x failed rc %d",
			(c_ctrl->cci_info->sid << 1), rc);
		return rc;
	}

	return rc;
}

static int32_t cam_cci_burst_read(struct v4l2_subdev *sd,
	struct cam_cci_ctrl *c_ctrl)
{
	int32_t rc = 0;
	uint32_t val = 0, i = 0, j = 0, irq_mask_update = 0;
	unsigned long rem_jiffies, flags;
	int32_t read_words = 0, exp_words = 0;
	int32_t index = 0, first_byte = 0, total_read_words = 0;
	enum cci_i2c_master_t master;
	enum cci_i2c_queue_t queue = QUEUE_1;
	struct cci_device                  *cci_dev = NULL;
	struct cam_cci_read_cfg            *read_cfg = NULL;
	struct cam_hw_soc_info             *soc_info = NULL;
	void __iomem                       *base = NULL;

	cci_dev = v4l2_get_subdevdata(sd);
	master = c_ctrl->cci_info->cci_i2c_master;
	read_cfg = &c_ctrl->cfg.cci_i2c_read_cfg;

	if (c_ctrl->cci_info->cci_i2c_master >= MASTER_MAX
		|| c_ctrl->cci_info->cci_i2c_master < 0) {
		CAM_ERR(CAM_CCI, "Invalid I2C master addr");
		return -EINVAL;
	}

	soc_info = &cci_dev->soc_info;
	base = soc_info->reg_map[0].mem_base;

	mutex_lock(&cci_dev->cci_master_info[master].mutex);
	if (cci_dev->cci_master_info[master].is_first_req) {
		cci_dev->cci_master_info[master].is_first_req = false;
		CAM_DBG(CAM_CCI, "Master: %d, curr_freq: %d, req_freq: %d",
			master, cci_dev->i2c_freq_mode[master],
			c_ctrl->cci_info->i2c_freq_mode);
		down(&cci_dev->cci_master_info[master].master_sem);
	} else if (c_ctrl->cci_info->i2c_freq_mode
		!= cci_dev->i2c_freq_mode[master]) {
		CAM_DBG(CAM_CCI, "Master: %d, curr_freq: %d, req_freq: %d",
			master, cci_dev->i2c_freq_mode[master],
			c_ctrl->cci_info->i2c_freq_mode);
		down(&cci_dev->cci_master_info[master].master_sem);
	} else {
		CAM_DBG(CAM_CCI, "Master: %d, curr_freq: %d, req_freq: %d",
			master, cci_dev->i2c_freq_mode[master],
			c_ctrl->cci_info->i2c_freq_mode);
		spin_lock(&cci_dev->cci_master_info[master].freq_cnt);
		cci_dev->cci_master_info[master].freq_ref_cnt++;
		spin_unlock(&cci_dev->cci_master_info[master].freq_cnt);
	}

	/* Set the I2C Frequency */
	rc = cam_cci_set_clk_param(cci_dev, c_ctrl);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "cam_cci_set_clk_param failed rc = %d", rc);
		mutex_unlock(&cci_dev->cci_master_info[master].mutex);
		goto rel_master;
	}
	mutex_unlock(&cci_dev->cci_master_info[master].mutex);

	mutex_lock(&cci_dev->cci_master_info[master].mutex_q[queue]);
	reinit_completion(&cci_dev->cci_master_info[master].report_q[queue]);
	/*
	 * Call validate queue to make sure queue is empty before starting.
	 * If this call fails, don't proceed with i2c_read call. This is to
	 * avoid overflow / underflow of queue
	 */
	rc = cam_cci_validate_queue(cci_dev,
		cci_dev->cci_i2c_queue_info[master][queue].max_queue_size - 1,
		master, queue);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "Initial validataion failed rc %d", rc);
		goto rel_mutex_q;
	}

	if (c_ctrl->cci_info->retries > CCI_I2C_READ_MAX_RETRIES) {
		CAM_ERR(CAM_CCI, "More than max retries");
		goto rel_mutex_q;
	}

	if (read_cfg->data == NULL) {
		CAM_ERR(CAM_CCI, "Data ptr is NULL");
		goto rel_mutex_q;
	}

	if (read_cfg->addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX) {
		CAM_ERR(CAM_CCI, "failed : Invalid addr type: %u",
			read_cfg->addr_type);
		rc = -EINVAL;
		goto rel_mutex_q;
	}

	CAM_DBG(CAM_CCI, "set param sid 0x%x retries %d id_map %d",
		c_ctrl->cci_info->sid, c_ctrl->cci_info->retries,
		c_ctrl->cci_info->id_map);
	val = CCI_I2C_SET_PARAM_CMD | c_ctrl->cci_info->sid << 4 |
		c_ctrl->cci_info->retries << 16 |
		c_ctrl->cci_info->id_map << 18;
	rc = cam_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CAM_DBG(CAM_CCI, "failed rc: %d", rc);
		goto rel_mutex_q;
	}

	val = CCI_I2C_LOCK_CMD;
	rc = cam_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CAM_DBG(CAM_CCI, "failed rc: %d", rc);
		goto rel_mutex_q;
	}

	val = CCI_I2C_WRITE_DISABLE_P_CMD | (read_cfg->addr_type << 4);
	for (i = 0; i < read_cfg->addr_type; i++) {
		val |= ((read_cfg->addr >> (i << 3)) & 0xFF)  <<
		((read_cfg->addr_type - i) << 3);
	}

	rc = cam_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CAM_DBG(CAM_CCI, "failed rc: %d", rc);
		goto rel_mutex_q;
	}

	val = CCI_I2C_READ_CMD | (read_cfg->num_byte << 4);
	rc = cam_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CAM_DBG(CAM_CCI, "failed rc: %d", rc);
		goto rel_mutex_q;
	}

	val = CCI_I2C_UNLOCK_CMD;
	rc = cam_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CAM_DBG(CAM_CCI, "failed rc: %d", rc);
		goto rel_mutex_q;
	}

	val = cam_io_r_mb(base + CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR
			+ master * 0x200 + queue * 0x100);
	CAM_DBG(CAM_CCI, "cur word cnt 0x%x", val);
	cam_io_w_mb(val, base + CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR
			+ master * 0x200 + queue * 0x100);

	val = 1 << ((master * 2) + queue);
	cam_io_w_mb(val, base + CCI_QUEUE_START_ADDR);

	exp_words = ((read_cfg->num_byte / 4) + 1);
	CAM_DBG(CAM_CCI, "waiting for threshold [exp_words %d]", exp_words);

	while (total_read_words != exp_words) {
		rem_jiffies = wait_for_completion_timeout(
			&cci_dev->cci_master_info[master].th_complete,
			CCI_TIMEOUT);
		if (!rem_jiffies) {
			rc = -ETIMEDOUT;
			val = cam_io_r_mb(base +
				CCI_I2C_M0_READ_BUF_LEVEL_ADDR +
				master * 0x100);
			CAM_ERR(CAM_CCI,
				"wait_for_completion_timeout rc = %d FIFO buf_lvl:0x%x",
				rc, val);
#ifdef DUMP_CCI_REGISTERS
			cam_cci_dump_registers(cci_dev, master, queue);
#endif
			cam_cci_flush_queue(cci_dev, master);
			goto rel_mutex_q;
		}

		if (cci_dev->cci_master_info[master].status) {
			CAM_ERR(CAM_CCI, "Error with Salve: 0x%x",
				(c_ctrl->cci_info->sid << 1));
			rc = -EINVAL;
			cci_dev->cci_master_info[master].status = 0;
			goto rel_mutex_q;
		}

		read_words = cam_io_r_mb(base +
			CCI_I2C_M0_READ_BUF_LEVEL_ADDR + master * 0x100);
		if (read_words <= 0) {
			CAM_DBG(CAM_CCI, "FIFO Buffer lvl is 0");
			continue;
		}

		j++;
		CAM_DBG(CAM_CCI, "Iteration: %u read_words %d", j, read_words);

		total_read_words += read_words;
		while (read_words > 0) {
			val = cam_io_r_mb(base +
				CCI_I2C_M0_READ_DATA_ADDR + master * 0x100);
			for (i = 0; (i < 4) &&
				(index < read_cfg->num_byte); i++) {
				CAM_DBG(CAM_CCI, "i:%d index:%d", i, index);
				if (!first_byte) {
					CAM_DBG(CAM_CCI, "sid 0x%x",
						val & 0xFF);
					first_byte++;
				} else {
					read_cfg->data[index] =
						(val  >> (i * 8)) & 0xFF;
					CAM_DBG(CAM_CCI, "data[%d] 0x%x", index,
						read_cfg->data[index]);
					index++;
				}
			}
			read_words--;
		}

		CAM_DBG(CAM_CCI, "Iteraion:%u total_read_words %d",
			j, total_read_words);

		spin_lock_irqsave(&cci_dev->lock_status, flags);
		if (cci_dev->irqs_disabled) {
			irq_mask_update =
				cam_io_r_mb(base + CCI_IRQ_MASK_1_ADDR) |
				CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD;
			if (master == MASTER_0 && cci_dev->irqs_disabled &
				CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD)
				irq_mask_update |=
					CCI_IRQ_STATUS_1_I2C_M0_RD_THRESHOLD;
			else if (master == MASTER_1 && cci_dev->irqs_disabled &
				CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD)
				irq_mask_update |=
					CCI_IRQ_STATUS_1_I2C_M1_RD_THRESHOLD;
			cam_io_w_mb(irq_mask_update,
				base + CCI_IRQ_MASK_1_ADDR);
		}
		spin_unlock_irqrestore(&cci_dev->lock_status, flags);

		if (total_read_words == exp_words) {
		   /*
		    * This wait is for RD_DONE irq, if RD_DONE is
		    * triggered we will call complete on both threshold
		    * & read done waits. As part of the threshold wait
		    * we will be draining the entire buffer out. This
		    * wait is to compensate for the complete invoked for
		    * RD_DONE exclusively.
		    */
			rem_jiffies = wait_for_completion_timeout(
			&cci_dev->cci_master_info[master].rd_done,
			CCI_TIMEOUT);
			if (!rem_jiffies) {
				rc = -ETIMEDOUT;
				val = cam_io_r_mb(base +
					CCI_I2C_M0_READ_BUF_LEVEL_ADDR +
					master * 0x100);
				CAM_ERR(CAM_CCI,
					"Failed to receive RD_DONE irq rc = %d FIFO buf_lvl:0x%x",
					rc, val);
				#ifdef DUMP_CCI_REGISTERS
					cam_cci_dump_registers(cci_dev,
						master, queue);
				#endif
					cam_cci_flush_queue(cci_dev, master);
				goto rel_mutex_q;
			}

			if (cci_dev->cci_master_info[master].status) {
				CAM_ERR(CAM_CCI, "Error with Slave 0x%x",
					(c_ctrl->cci_info->sid << 1));
				rc = -EINVAL;
				cci_dev->cci_master_info[master].status = 0;
				goto rel_mutex_q;
			}
			break;
		}
	}

	CAM_DBG(CAM_CCI, "Burst read successful words_read %d",
		total_read_words);

rel_mutex_q:
	mutex_unlock(&cci_dev->cci_master_info[master].mutex_q[queue]);
rel_master:
	spin_lock(&cci_dev->cci_master_info[master].freq_cnt);
	if (cci_dev->cci_master_info[master].freq_ref_cnt == 0)
		up(&cci_dev->cci_master_info[master].master_sem);
	else
		cci_dev->cci_master_info[master].freq_ref_cnt--;
	spin_unlock(&cci_dev->cci_master_info[master].freq_cnt);
	return rc;
}

static int32_t cam_cci_read(struct v4l2_subdev *sd,
	struct cam_cci_ctrl *c_ctrl)
{
	int32_t rc = 0;
	uint32_t val = 0;
	int32_t read_words = 0, exp_words = 0;
	int32_t index = 0, first_byte = 0;
	uint32_t i = 0;
	enum cci_i2c_master_t master;
	enum cci_i2c_queue_t queue = QUEUE_1;
	struct cci_device *cci_dev = NULL;
	struct cam_cci_read_cfg *read_cfg = NULL;
	struct cam_hw_soc_info *soc_info = NULL;
	void __iomem *base = NULL;

	cci_dev = v4l2_get_subdevdata(sd);
	master = c_ctrl->cci_info->cci_i2c_master;
	read_cfg = &c_ctrl->cfg.cci_i2c_read_cfg;

	if (c_ctrl->cci_info->cci_i2c_master >= MASTER_MAX
		|| c_ctrl->cci_info->cci_i2c_master < 0) {
		CAM_ERR(CAM_CCI, "Invalid I2C master addr");
		return -EINVAL;
	}

	soc_info = &cci_dev->soc_info;
	base = soc_info->reg_map[0].mem_base;

	mutex_lock(&cci_dev->cci_master_info[master].mutex);
	if (cci_dev->cci_master_info[master].is_first_req) {
		cci_dev->cci_master_info[master].is_first_req = false;
		CAM_DBG(CAM_CCI, "Master: %d, curr_freq: %d, req_freq: %d",
			master, cci_dev->i2c_freq_mode[master],
			c_ctrl->cci_info->i2c_freq_mode);
		down(&cci_dev->cci_master_info[master].master_sem);
	} else if (c_ctrl->cci_info->i2c_freq_mode
		!= cci_dev->i2c_freq_mode[master]) {
		CAM_DBG(CAM_CCI, "Master: %d, curr_freq: %d, req_freq: %d",
			master, cci_dev->i2c_freq_mode[master],
			c_ctrl->cci_info->i2c_freq_mode);
		down(&cci_dev->cci_master_info[master].master_sem);
	} else {
		CAM_DBG(CAM_CCI, "Master: %d, curr_freq: %d, req_freq: %d",
			master, cci_dev->i2c_freq_mode[master],
			c_ctrl->cci_info->i2c_freq_mode);
		spin_lock(&cci_dev->cci_master_info[master].freq_cnt);
		cci_dev->cci_master_info[master].freq_ref_cnt++;
		spin_unlock(&cci_dev->cci_master_info[master].freq_cnt);
	}

	/* Set the I2C Frequency */
	rc = cam_cci_set_clk_param(cci_dev, c_ctrl);
	if (rc < 0) {
		mutex_unlock(&cci_dev->cci_master_info[master].mutex);
		CAM_ERR(CAM_CCI, "cam_cci_set_clk_param failed rc = %d", rc);
		goto rel_master;
	}
	mutex_unlock(&cci_dev->cci_master_info[master].mutex);

	mutex_lock(&cci_dev->cci_master_info[master].mutex_q[queue]);
	reinit_completion(&cci_dev->cci_master_info[master].report_q[queue]);
	/*
	 * Call validate queue to make sure queue is empty before starting.
	 * If this call fails, don't proceed with i2c_read call. This is to
	 * avoid overflow / underflow of queue
	 */
	rc = cam_cci_validate_queue(cci_dev,
		cci_dev->cci_i2c_queue_info[master][queue].max_queue_size - 1,
		master, queue);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "Initial validataion failed rc %d", rc);
		goto rel_mutex_q;
	}

	if (c_ctrl->cci_info->retries > CCI_I2C_READ_MAX_RETRIES) {
		CAM_ERR(CAM_CCI, "More than max retries");
		goto rel_mutex_q;
	}

	if (read_cfg->data == NULL) {
		CAM_ERR(CAM_CCI, "Data ptr is NULL");
		goto rel_mutex_q;
	}

	CAM_DBG(CAM_CCI, "master %d, queue %d", master, queue);
	CAM_DBG(CAM_CCI, "set param sid 0x%x retries %d id_map %d",
		c_ctrl->cci_info->sid, c_ctrl->cci_info->retries,
		c_ctrl->cci_info->id_map);
	val = CCI_I2C_SET_PARAM_CMD | c_ctrl->cci_info->sid << 4 |
		c_ctrl->cci_info->retries << 16 |
		c_ctrl->cci_info->id_map << 18;
	rc = cam_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CAM_DBG(CAM_CCI, "failed rc: %d", rc);
		goto rel_mutex_q;
	}

	val = CCI_I2C_LOCK_CMD;
	rc = cam_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CAM_DBG(CAM_CCI, "failed rc: %d", rc);
		goto rel_mutex_q;
	}

	if (read_cfg->addr_type >= CAMERA_SENSOR_I2C_TYPE_MAX) {
		CAM_ERR(CAM_CCI, "failed : Invalid addr type: %u",
			read_cfg->addr_type);
		rc = -EINVAL;
		goto rel_mutex_q;
	}

	val = CCI_I2C_WRITE_DISABLE_P_CMD | (read_cfg->addr_type << 4);
	for (i = 0; i < read_cfg->addr_type; i++) {
		val |= ((read_cfg->addr >> (i << 3)) & 0xFF)  <<
		((read_cfg->addr_type - i) << 3);
	}

	rc = cam_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CAM_DBG(CAM_CCI, "failed rc: %d", rc);
		goto rel_mutex_q;
	}

	val = CCI_I2C_READ_CMD | (read_cfg->num_byte << 4);
	rc = cam_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CAM_DBG(CAM_CCI, "failed rc: %d", rc);
		goto rel_mutex_q;
	}

	val = CCI_I2C_UNLOCK_CMD;
	rc = cam_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CAM_DBG(CAM_CCI, "failed rc: %d", rc);
		goto rel_mutex_q;
	}

	val = cam_io_r_mb(base + CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR
			+ master * 0x200 + queue * 0x100);
	CAM_DBG(CAM_CCI, "cur word cnt 0x%x", val);
	cam_io_w_mb(val, base + CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR
			+ master * 0x200 + queue * 0x100);

	val = 1 << ((master * 2) + queue);
	cam_io_w_mb(val, base + CCI_QUEUE_START_ADDR);
	CAM_DBG(CAM_CCI,
		"waiting_for_rd_done [exp_words: %d]",
		((read_cfg->num_byte / 4) + 1));

	rc = wait_for_completion_timeout(
		&cci_dev->cci_master_info[master].rd_done, CCI_TIMEOUT);
	if (rc <= 0) {
#ifdef DUMP_CCI_REGISTERS
		cam_cci_dump_registers(cci_dev, master, queue);
#endif
		if (rc == 0)
			rc = -ETIMEDOUT;
		val = cam_io_r_mb(base +
			CCI_I2C_M0_READ_BUF_LEVEL_ADDR + master * 0x100);
		CAM_ERR(CAM_CCI,
			"wait_for_completion_timeout rc = %d FIFO buf_lvl: 0x%x",
			rc, val);
		cam_cci_flush_queue(cci_dev, master);
		goto rel_mutex_q;
	} else {
		rc = 0;
	}

	if (cci_dev->cci_master_info[master].status) {
		CAM_ERR(CAM_CCI, "ERROR with Slave 0x%x:",
			(c_ctrl->cci_info->sid << 1));
		rc = -EINVAL;
		cci_dev->cci_master_info[master].status = 0;
		goto rel_mutex_q;
	}

	read_words = cam_io_r_mb(base +
		CCI_I2C_M0_READ_BUF_LEVEL_ADDR + master * 0x100);
	exp_words = ((read_cfg->num_byte / 4) + 1);
	if (read_words != exp_words) {
		CAM_ERR(CAM_CCI, "read_words = %d, exp words = %d",
			read_words, exp_words);
		memset(read_cfg->data, 0, read_cfg->num_byte);
		rc = -EINVAL;
		goto rel_mutex_q;
	}
	index = 0;
	CAM_DBG(CAM_CCI, "index %d num_type %d", index, read_cfg->num_byte);
	first_byte = 0;
	while (read_words > 0) {
		val = cam_io_r_mb(base +
			CCI_I2C_M0_READ_DATA_ADDR + master * 0x100);
		CAM_DBG(CAM_CCI, "read val 0x%x", val);
		for (i = 0; (i < 4) && (index < read_cfg->num_byte); i++) {
			CAM_DBG(CAM_CCI, "i:%d index:%d", i, index);
			if (!first_byte) {
				CAM_DBG(CAM_CCI, "sid 0x%x", val & 0xFF);
				first_byte++;
			} else {
				read_cfg->data[index] =
					(val  >> (i * 8)) & 0xFF;
				CAM_DBG(CAM_CCI, "data[%d] 0x%x", index,
					read_cfg->data[index]);
				index++;
			}
		}
		read_words--;
	}
rel_mutex_q:
	mutex_unlock(&cci_dev->cci_master_info[master].mutex_q[queue]);
rel_master:
	spin_lock(&cci_dev->cci_master_info[master].freq_cnt);
	if (cci_dev->cci_master_info[master].freq_ref_cnt == 0)
		up(&cci_dev->cci_master_info[master].master_sem);
	else
		cci_dev->cci_master_info[master].freq_ref_cnt--;
	spin_unlock(&cci_dev->cci_master_info[master].freq_cnt);
	return rc;
}

static int32_t cam_cci_i2c_write(struct v4l2_subdev *sd,
	struct cam_cci_ctrl *c_ctrl, enum cci_i2c_queue_t queue,
	enum cci_i2c_sync sync_en)
{
	int32_t rc = 0;
	struct cci_device *cci_dev;
	enum cci_i2c_master_t master;

	cci_dev = v4l2_get_subdevdata(sd);

	if (cci_dev->cci_state != CCI_STATE_ENABLED) {
		CAM_ERR(CAM_CCI, "invalid cci state %d",
			cci_dev->cci_state);
		return -EINVAL;
	}
	master = c_ctrl->cci_info->cci_i2c_master;
	CAM_DBG(CAM_CCI, "set param sid 0x%x retries %d id_map %d",
		c_ctrl->cci_info->sid, c_ctrl->cci_info->retries,
		c_ctrl->cci_info->id_map);

	mutex_lock(&cci_dev->cci_master_info[master].mutex);
	if (cci_dev->cci_master_info[master].is_first_req) {
		cci_dev->cci_master_info[master].is_first_req = false;
		CAM_DBG(CAM_CCI, "Master: %d, curr_freq: %d, req_freq: %d",
			master, cci_dev->i2c_freq_mode[master],
			c_ctrl->cci_info->i2c_freq_mode);
		down(&cci_dev->cci_master_info[master].master_sem);
	} else if (c_ctrl->cci_info->i2c_freq_mode
		!= cci_dev->i2c_freq_mode[master]) {
		CAM_DBG(CAM_CCI, "Master: %d, curr_freq: %d, req_freq: %d",
			master, cci_dev->i2c_freq_mode[master],
			c_ctrl->cci_info->i2c_freq_mode);
		down(&cci_dev->cci_master_info[master].master_sem);
	} else {
		CAM_DBG(CAM_CCI, "Master: %d, curr_freq: %d, req_freq: %d",
			master, cci_dev->i2c_freq_mode[master],
			c_ctrl->cci_info->i2c_freq_mode);
		spin_lock(&cci_dev->cci_master_info[master].freq_cnt);
		cci_dev->cci_master_info[master].freq_ref_cnt++;
		spin_unlock(&cci_dev->cci_master_info[master].freq_cnt);
	}

	/* Set the I2C Frequency */
	rc = cam_cci_set_clk_param(cci_dev, c_ctrl);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "cam_cci_set_clk_param failed rc = %d", rc);
		mutex_unlock(&cci_dev->cci_master_info[master].mutex);
		goto ERROR;
	}
	mutex_unlock(&cci_dev->cci_master_info[master].mutex);

	reinit_completion(&cci_dev->cci_master_info[master].report_q[queue]);
	/*
	 * Call validate queue to make sure queue is empty before starting.
	 * If this call fails, don't proceed with i2c_write call. This is to
	 * avoid overflow / underflow of queue
	 */
	rc = cam_cci_validate_queue(cci_dev,
		cci_dev->cci_i2c_queue_info[master][queue].max_queue_size-1,
		master, queue);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "Initial validataion failed rc %d",
			rc);
		goto ERROR;
	}
	if (c_ctrl->cci_info->retries > CCI_I2C_READ_MAX_RETRIES) {
		CAM_ERR(CAM_CCI, "More than max retries");
		goto ERROR;
	}
	rc = cam_cci_data_queue(cci_dev, c_ctrl, queue, sync_en);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "failed rc: %d", rc);
		goto ERROR;
	}

ERROR:
	spin_lock(&cci_dev->cci_master_info[master].freq_cnt);
	if (cci_dev->cci_master_info[master].freq_ref_cnt == 0)
		up(&cci_dev->cci_master_info[master].master_sem);
	else
		cci_dev->cci_master_info[master].freq_ref_cnt--;
	spin_unlock(&cci_dev->cci_master_info[master].freq_cnt);
	return rc;
}

static void cam_cci_write_async_helper(struct work_struct *work)
{
	int rc;
	struct cci_device *cci_dev;
	struct cci_write_async *write_async =
		container_of(work, struct cci_write_async, work);
	struct cam_sensor_i2c_reg_setting *i2c_msg;
	enum cci_i2c_master_t master;
	struct cam_cci_master_info *cci_master_info;

	cci_dev = write_async->cci_dev;
	i2c_msg = &write_async->c_ctrl.cfg.cci_i2c_write_cfg;
	master = write_async->c_ctrl.cci_info->cci_i2c_master;
	cci_master_info = &cci_dev->cci_master_info[master];

	mutex_lock(&cci_master_info->mutex_q[write_async->queue]);
	rc = cam_cci_i2c_write(&(cci_dev->v4l2_dev_str.sd),
		&write_async->c_ctrl, write_async->queue, write_async->sync_en);
	mutex_unlock(&cci_master_info->mutex_q[write_async->queue]);
	if (rc < 0)
		CAM_ERR(CAM_CCI, "failed rc: %d", rc);

	kfree(write_async->c_ctrl.cfg.cci_i2c_write_cfg.reg_setting);
	kfree(write_async);
}

static int32_t cam_cci_i2c_write_async(struct v4l2_subdev *sd,
	struct cam_cci_ctrl *c_ctrl, enum cci_i2c_queue_t queue,
	enum cci_i2c_sync sync_en)
{
	int32_t rc = 0;
	struct cci_write_async *write_async;
	struct cci_device *cci_dev;
	struct cam_sensor_i2c_reg_setting *cci_i2c_write_cfg;
	struct cam_sensor_i2c_reg_setting *cci_i2c_write_cfg_w;

	cci_dev = v4l2_get_subdevdata(sd);

	write_async = kzalloc(sizeof(*write_async), GFP_KERNEL);
	if (!write_async)
		return -ENOMEM;


	INIT_WORK(&write_async->work, cam_cci_write_async_helper);
	write_async->cci_dev = cci_dev;
	write_async->c_ctrl = *c_ctrl;
	write_async->queue = queue;
	write_async->sync_en = sync_en;

	cci_i2c_write_cfg = &c_ctrl->cfg.cci_i2c_write_cfg;
	cci_i2c_write_cfg_w = &write_async->c_ctrl.cfg.cci_i2c_write_cfg;

	if (cci_i2c_write_cfg->size == 0) {
		kfree(write_async);
		return -EINVAL;
	}

	cci_i2c_write_cfg_w->reg_setting =
		kzalloc(sizeof(struct cam_sensor_i2c_reg_array)*
		cci_i2c_write_cfg->size, GFP_KERNEL);
	if (!cci_i2c_write_cfg_w->reg_setting) {
		CAM_ERR(CAM_CCI, "Couldn't allocate memory");
		kfree(write_async);
		return -ENOMEM;
	}
	memcpy(cci_i2c_write_cfg_w->reg_setting,
		cci_i2c_write_cfg->reg_setting,
		(sizeof(struct cam_sensor_i2c_reg_array)*
						cci_i2c_write_cfg->size));

	cci_i2c_write_cfg_w->addr_type = cci_i2c_write_cfg->addr_type;
	cci_i2c_write_cfg_w->addr_type = cci_i2c_write_cfg->addr_type;
	cci_i2c_write_cfg_w->data_type = cci_i2c_write_cfg->data_type;
	cci_i2c_write_cfg_w->size = cci_i2c_write_cfg->size;
	cci_i2c_write_cfg_w->delay = cci_i2c_write_cfg->delay;

	queue_work(cci_dev->write_wq[write_async->queue], &write_async->work);

	return rc;
}

static int32_t cam_cci_read_bytes(struct v4l2_subdev *sd,
	struct cam_cci_ctrl *c_ctrl)
{
	int32_t rc = 0;
	struct cci_device *cci_dev = NULL;
	enum cci_i2c_master_t master;
	struct cam_cci_read_cfg *read_cfg = NULL;
	uint16_t read_bytes = 0;

	if (!sd || !c_ctrl) {
		CAM_ERR(CAM_CCI, "sd %pK c_ctrl %pK", sd, c_ctrl);
		return -EINVAL;
	}
	if (!c_ctrl->cci_info) {
		CAM_ERR(CAM_CCI, "cci_info NULL");
		return -EINVAL;
	}
	cci_dev = v4l2_get_subdevdata(sd);
	if (!cci_dev) {
		CAM_ERR(CAM_CCI, "cci_dev NULL");
		return -EINVAL;
	}
	if (cci_dev->cci_state != CCI_STATE_ENABLED) {
		CAM_ERR(CAM_CCI, "invalid cci state %d", cci_dev->cci_state);
		return -EINVAL;
	}

	if (c_ctrl->cci_info->cci_i2c_master >= MASTER_MAX
			|| c_ctrl->cci_info->cci_i2c_master < 0) {
		CAM_ERR(CAM_CCI, "Invalid I2C master addr");
		return -EINVAL;
	}

	master = c_ctrl->cci_info->cci_i2c_master;
	read_cfg = &c_ctrl->cfg.cci_i2c_read_cfg;
	if ((!read_cfg->num_byte) || (read_cfg->num_byte > CCI_I2C_MAX_READ)) {
		CAM_ERR(CAM_CCI, "read num bytes 0");
		rc = -EINVAL;
		goto ERROR;
	}

	read_bytes = read_cfg->num_byte;

	/*
	 * To avoid any conflicts due to back to back trigger of
	 * THRESHOLD irq's, we reinit the threshold wait before
	 * we load the burst read cmd.
	 */
	reinit_completion(&cci_dev->cci_master_info[master].rd_done);
	reinit_completion(&cci_dev->cci_master_info[master].th_complete);

	CAM_DBG(CAM_CCI, "Bytes to read %u", read_bytes);
	do {
		if (read_bytes >= CCI_I2C_MAX_BYTE_COUNT)
			read_cfg->num_byte = CCI_I2C_MAX_BYTE_COUNT;
		else
			read_cfg->num_byte = read_bytes;

		if (read_cfg->num_byte >= CCI_READ_MAX) {
			cci_dev->is_burst_read = true;
			rc = cam_cci_burst_read(sd, c_ctrl);
		} else {
			cci_dev->is_burst_read = false;
			rc = cam_cci_read(sd, c_ctrl);
		}
		if (rc) {
			CAM_ERR(CAM_CCI, "failed to read rc:%d", rc);
			goto ERROR;
		}

		if (read_bytes >= CCI_I2C_MAX_BYTE_COUNT) {
			read_cfg->addr += (CCI_I2C_MAX_BYTE_COUNT /
				read_cfg->data_type);
			read_cfg->data += CCI_I2C_MAX_BYTE_COUNT;
			read_bytes -= CCI_I2C_MAX_BYTE_COUNT;
		} else {
			read_bytes = 0;
		}
	} while (read_bytes);

ERROR:
	cci_dev->is_burst_read = false;
	return rc;
}

static int32_t cam_cci_i2c_set_sync_prms(struct v4l2_subdev *sd,
	struct cam_cci_ctrl *c_ctrl)
{
	int32_t rc = 0;
	struct cci_device *cci_dev;

	cci_dev = v4l2_get_subdevdata(sd);
	if (!cci_dev || !c_ctrl) {
		CAM_ERR(CAM_CCI, "failed: invalid params %pK %pK",
			cci_dev, c_ctrl);
		rc = -EINVAL;
		return rc;
	}
	cci_dev->cci_wait_sync_cfg = c_ctrl->cfg.cci_wait_sync_cfg;
	cci_dev->valid_sync = cci_dev->cci_wait_sync_cfg.csid < 0 ? 0 : 1;

	return rc;
}

static int32_t cam_cci_release(struct v4l2_subdev *sd)
{
	uint8_t rc = 0;
	struct cci_device *cci_dev;

	cci_dev = v4l2_get_subdevdata(sd);

	rc = cam_cci_soc_release(cci_dev);
	if (rc < 0) {
		CAM_ERR(CAM_CCI, "Failed in releasing the cci: %d", rc);
		return rc;
	}

	return rc;
}

static int32_t cam_cci_write(struct v4l2_subdev *sd,
	struct cam_cci_ctrl *c_ctrl)
{
	int32_t rc = 0;
	struct cci_device *cci_dev;
	enum cci_i2c_master_t master;
	struct cam_cci_master_info *cci_master_info;
	uint32_t i;

	cci_dev = v4l2_get_subdevdata(sd);
	if (!cci_dev || !c_ctrl) {
		CAM_ERR(CAM_CCI, "failed: invalid params %pK %pK",
			cci_dev, c_ctrl);
		rc = -EINVAL;
		return rc;
	}

	master = c_ctrl->cci_info->cci_i2c_master;

	if (c_ctrl->cci_info->cci_i2c_master >= MASTER_MAX
		|| c_ctrl->cci_info->cci_i2c_master < 0) {
		CAM_ERR(CAM_CCI, "Invalid I2C master addr");
		return -EINVAL;
	}

	cci_master_info = &cci_dev->cci_master_info[master];

	switch (c_ctrl->cmd) {
	case MSM_CCI_I2C_WRITE_SYNC_BLOCK:
		mutex_lock(&cci_master_info->mutex_q[SYNC_QUEUE]);
		rc = cam_cci_i2c_write(sd, c_ctrl,
			SYNC_QUEUE, MSM_SYNC_ENABLE);
		mutex_unlock(&cci_master_info->mutex_q[SYNC_QUEUE]);
		break;
	case MSM_CCI_I2C_WRITE_SYNC:
		rc = cam_cci_i2c_write_async(sd, c_ctrl,
			SYNC_QUEUE, MSM_SYNC_ENABLE);
		break;
	case MSM_CCI_I2C_WRITE:
	case MSM_CCI_I2C_WRITE_SEQ:
	case MSM_CCI_I2C_WRITE_BURST:
		for (i = 0; i < NUM_QUEUES; i++) {
			if (mutex_trylock(&cci_master_info->mutex_q[i])) {
				rc = cam_cci_i2c_write(sd, c_ctrl, i,
					MSM_SYNC_DISABLE);
				mutex_unlock(&cci_master_info->mutex_q[i]);
				return rc;
			}
		}
		mutex_lock(&cci_master_info->mutex_q[PRIORITY_QUEUE]);
		rc = cam_cci_i2c_write(sd, c_ctrl,
			PRIORITY_QUEUE, MSM_SYNC_DISABLE);
		mutex_unlock(&cci_master_info->mutex_q[PRIORITY_QUEUE]);
		break;
	case MSM_CCI_I2C_WRITE_ASYNC:
		rc = cam_cci_i2c_write_async(sd, c_ctrl,
			PRIORITY_QUEUE, MSM_SYNC_DISABLE);
		break;
	default:
		rc = -ENOIOCTLCMD;
	}

	return rc;
}

int32_t cam_cci_core_cfg(struct v4l2_subdev *sd,
	struct cam_cci_ctrl *cci_ctrl)
{
	int32_t rc = 0;
	struct cci_device *cci_dev = v4l2_get_subdevdata(sd);
	enum cci_i2c_master_t master = MASTER_MAX;

	if (!cci_dev) {
		CAM_ERR(CAM_CCI, "CCI_DEV IS NULL");
		return -EINVAL;
	}

	if (!cci_ctrl) {
		CAM_ERR(CAM_CCI, "CCI_CTRL IS NULL");
		return -EINVAL;
	}

	master = cci_ctrl->cci_info->cci_i2c_master;
	if (master >= MASTER_MAX) {
		CAM_ERR(CAM_CCI, "INVALID MASTER: %d", master);
		return -EINVAL;
	}

	if (cci_dev->cci_master_info[master].status < 0) {
		CAM_WARN(CAM_CCI, "CCI hardware is resetting");
		return -EAGAIN;
	}
	CAM_DBG(CAM_CCI, "master = %d, cmd = %d", master, cci_ctrl->cmd);

	switch (cci_ctrl->cmd) {
	case MSM_CCI_INIT:
		mutex_lock(&cci_dev->init_mutex);
		rc = cam_cci_init(sd, cci_ctrl);
		mutex_unlock(&cci_dev->init_mutex);
		break;
	case MSM_CCI_RELEASE:
		mutex_lock(&cci_dev->init_mutex);
		rc = cam_cci_release(sd);
		mutex_unlock(&cci_dev->init_mutex);
		break;
	case MSM_CCI_I2C_READ:
		mutex_lock(&cci_dev->init_mutex);
		rc = cam_cci_read_bytes(sd, cci_ctrl);
		/* Added by qudao1@xiaomi.com */
		if (rc < 0) {
			CAM_ERR(CAM_CCI, "cam cci err %d , read, slav 0x%x on dev/master %d/%d",
				rc, cci_ctrl->cci_info->sid << 1,
				cci_ctrl->cci_info->cci_device,
				cci_ctrl->cci_info->cci_i2c_master);
		}
		mutex_unlock(&cci_dev->init_mutex);
		/* End of Added by qudao1@xiaomi.com */
		break;
	case MSM_CCI_I2C_WRITE:
	case MSM_CCI_I2C_WRITE_SEQ:
	case MSM_CCI_I2C_WRITE_BURST:
	case MSM_CCI_I2C_WRITE_SYNC:
	case MSM_CCI_I2C_WRITE_ASYNC:
	case MSM_CCI_I2C_WRITE_SYNC_BLOCK:
		mutex_lock(&cci_dev->init_mutex);
		rc = cam_cci_write(sd, cci_ctrl);
		/* Added by qudao1@xiaomi.com */
		if (rc < 0) {
			CAM_ERR(CAM_CCI, "cam cci err %d , write type %d , slav 0x%x on dev/master %d/%d",
				rc, cci_ctrl->cmd,
				cci_ctrl->cci_info->sid << 1,
				cci_ctrl->cci_info->cci_device,
				cci_ctrl->cci_info->cci_i2c_master);
		}
		mutex_unlock(&cci_dev->init_mutex);
		/* End of Added by qudao1@xiaomi.com */
		break;
	case MSM_CCI_GPIO_WRITE:
		break;
	case MSM_CCI_SET_SYNC_CID:
		rc = cam_cci_i2c_set_sync_prms(sd, cci_ctrl);
		break;

	default:
		rc = -ENOIOCTLCMD;
	}

	cci_ctrl->status = rc;

	return rc;
}
