/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include "msm_sd.h"
#include "msm_cci.h"
#include "msm_cam_cci_hwreg.h"
#include "msm_camera_io_util.h"
#include "msm_camera_dt_util.h"
#include "cam_hw_ops.h"

#define V4L2_IDENT_CCI 50005
#define CCI_I2C_QUEUE_0_SIZE 64
#define CCI_I2C_QUEUE_1_SIZE 16
#define CYCLES_PER_MICRO_SEC_DEFAULT 4915
#define CCI_MAX_DELAY 1000000

#define CCI_TIMEOUT msecs_to_jiffies(500)

/* TODO move this somewhere else */
#define MSM_CCI_DRV_NAME "msm_cci"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

#undef CCI_DBG
#ifdef MSM_CCI_DEBUG
#define CCI_DBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CCI_DBG(fmt, args...) pr_debug(fmt, ##args)
#endif

/* Max bytes that can be read per CCI read transaction */
#define CCI_READ_MAX 12
#define CCI_I2C_READ_MAX_RETRIES 3
#define CCI_I2C_MAX_READ 8192
#define CCI_I2C_MAX_WRITE 8192

#define PRIORITY_QUEUE (QUEUE_0)
#define SYNC_QUEUE (QUEUE_1)

static struct v4l2_subdev *g_cci_subdev;

static struct msm_cam_clk_info cci_clk_info[CCI_NUM_CLK_CASES][CCI_NUM_CLK_MAX];

static void msm_cci_dump_registers(struct cci_device *cci_dev,
	enum cci_i2c_master_t master, enum cci_i2c_queue_t queue)
{
	uint32_t read_val = 0;
	uint32_t i = 0;
	uint32_t reg_offset = 0;

	/* CCI Top Registers */
	CCI_DBG(" **** %s : %d CCI TOP Registers ****\n", __func__, __LINE__);
	for (i = 0; i < DEBUG_TOP_REG_COUNT; i++) {
		reg_offset = DEBUG_TOP_REG_START + i * 4;
		read_val = msm_camera_io_r_mb(cci_dev->base + reg_offset);
		CCI_DBG("%s : %d offset = 0x%X value = 0x%X\n",
			__func__, __LINE__, reg_offset, read_val);
	}

	/* CCI Master registers */
	CCI_DBG(" **** %s : %d CCI MASTER%d Registers ****\n",
		__func__, __LINE__, master);
	for (i = 0; i < DEBUG_MASTER_REG_COUNT; i++) {
		if (i == 6)
			continue;
		reg_offset = DEBUG_MASTER_REG_START + master*0x100 + i * 4;
		read_val = msm_camera_io_r_mb(cci_dev->base + reg_offset);
		CCI_DBG("%s : %d offset = 0x%X value = 0x%X\n",
			__func__, __LINE__, reg_offset, read_val);
	}

	/* CCI Master Queue registers */
	CCI_DBG(" **** %s : %d CCI MASTER%d QUEUE%d Registers ****\n",
		__func__, __LINE__, master, queue);
	for (i = 0; i < DEBUG_MASTER_QUEUE_REG_COUNT; i++) {
		reg_offset = DEBUG_MASTER_QUEUE_REG_START +  master*0x200 +
			queue*0x100 + i * 4;
		read_val = msm_camera_io_r_mb(cci_dev->base + reg_offset);
		CCI_DBG("%s : %d offset = 0x%X value = 0x%X\n",
			__func__, __LINE__, reg_offset, read_val);
	}

	/* CCI Interrupt registers */
	CCI_DBG(" **** %s : %d CCI Interrupt Registers ****\n",
		__func__, __LINE__);
	for (i = 0; i < DEBUG_INTR_REG_COUNT; i++) {
		reg_offset = DEBUG_INTR_REG_START + i * 4;
		read_val = msm_camera_io_r_mb(cci_dev->base + reg_offset);
		CCI_DBG("%s : %d offset = 0x%X value = 0x%X\n",
			__func__, __LINE__, reg_offset, read_val);
	}
}

static int32_t msm_cci_set_clk_param(struct cci_device *cci_dev,
	struct msm_camera_cci_ctrl *c_ctrl)
{
	struct msm_cci_clk_params_t *clk_params = NULL;
	enum cci_i2c_master_t master = c_ctrl->cci_info->cci_i2c_master;
	enum i2c_freq_mode_t i2c_freq_mode = c_ctrl->cci_info->i2c_freq_mode;

	clk_params = &cci_dev->cci_clk_params[i2c_freq_mode];

	if ((i2c_freq_mode >= I2C_MAX_MODES) || (i2c_freq_mode < 0)) {
		pr_err("%s:%d invalid i2c_freq_mode = %d",
			__func__, __LINE__, i2c_freq_mode);
		return -EINVAL;
	}
	if (cci_dev->i2c_freq_mode[master] == i2c_freq_mode)
		return 0;
	if (MASTER_0 == master) {
		msm_camera_io_w_mb(clk_params->hw_thigh << 16 |
			clk_params->hw_tlow,
			cci_dev->base + CCI_I2C_M0_SCL_CTL_ADDR);
		msm_camera_io_w_mb(clk_params->hw_tsu_sto << 16 |
			clk_params->hw_tsu_sta,
			cci_dev->base + CCI_I2C_M0_SDA_CTL_0_ADDR);
		msm_camera_io_w_mb(clk_params->hw_thd_dat << 16 |
			clk_params->hw_thd_sta,
			cci_dev->base + CCI_I2C_M0_SDA_CTL_1_ADDR);
		msm_camera_io_w_mb(clk_params->hw_tbuf,
			cci_dev->base + CCI_I2C_M0_SDA_CTL_2_ADDR);
		msm_camera_io_w_mb(clk_params->hw_scl_stretch_en << 8 |
			clk_params->hw_trdhld << 4 | clk_params->hw_tsp,
			cci_dev->base + CCI_I2C_M0_MISC_CTL_ADDR);
	} else if (MASTER_1 == master) {
		msm_camera_io_w_mb(clk_params->hw_thigh << 16 |
			clk_params->hw_tlow,
			cci_dev->base + CCI_I2C_M1_SCL_CTL_ADDR);
		msm_camera_io_w_mb(clk_params->hw_tsu_sto << 16 |
			clk_params->hw_tsu_sta,
			cci_dev->base + CCI_I2C_M1_SDA_CTL_0_ADDR);
		msm_camera_io_w_mb(clk_params->hw_thd_dat << 16 |
			clk_params->hw_thd_sta,
			cci_dev->base + CCI_I2C_M1_SDA_CTL_1_ADDR);
		msm_camera_io_w_mb(clk_params->hw_tbuf,
			cci_dev->base + CCI_I2C_M1_SDA_CTL_2_ADDR);
		msm_camera_io_w_mb(clk_params->hw_scl_stretch_en << 8 |
			clk_params->hw_trdhld << 4 | clk_params->hw_tsp,
			cci_dev->base + CCI_I2C_M1_MISC_CTL_ADDR);
	}
	cci_dev->i2c_freq_mode[master] = i2c_freq_mode;
	return 0;
}

static void msm_cci_flush_queue(struct cci_device *cci_dev,
	enum cci_i2c_master_t master)
{
	int32_t rc = 0;

	msm_camera_io_w_mb(1 << master, cci_dev->base + CCI_HALT_REQ_ADDR);
	rc = wait_for_completion_timeout(
		&cci_dev->cci_master_info[master].reset_complete, CCI_TIMEOUT);
	if (rc < 0) {
		pr_err("%s:%d wait failed\n", __func__, __LINE__);
	} else if (rc == 0) {
		pr_err("%s:%d wait timeout\n", __func__, __LINE__);

		/* Set reset pending flag to TRUE */
		cci_dev->cci_master_info[master].reset_pending = TRUE;

		/* Set proper mask to RESET CMD address based on MASTER */
		if (master == MASTER_0)
			msm_camera_io_w_mb(CCI_M0_RESET_RMSK,
				cci_dev->base + CCI_RESET_CMD_ADDR);
		else
			msm_camera_io_w_mb(CCI_M1_RESET_RMSK,
				cci_dev->base + CCI_RESET_CMD_ADDR);

		/* wait for reset done irq */
		rc = wait_for_completion_timeout(
			&cci_dev->cci_master_info[master].reset_complete,
			CCI_TIMEOUT);
		if (rc <= 0)
			pr_err("%s:%d wait failed %d\n", __func__, __LINE__,
				rc);
	}
	return;
}

static int32_t msm_cci_validate_queue(struct cci_device *cci_dev,
	uint32_t len,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	int32_t rc = 0;
	uint32_t read_val = 0;
	uint32_t reg_offset = master * 0x200 + queue * 0x100;
	read_val = msm_camera_io_r_mb(cci_dev->base +
		CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR + reg_offset);
	CDBG("%s line %d CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR %d len %d max %d\n",
		__func__, __LINE__, read_val, len,
		cci_dev->cci_i2c_queue_info[master][queue].max_queue_size);
	if ((read_val + len + 1) > cci_dev->
		cci_i2c_queue_info[master][queue].max_queue_size) {
		uint32_t reg_val = 0;
		uint32_t report_val = CCI_I2C_REPORT_CMD | (1 << 8);
		CDBG("%s:%d CCI_I2C_REPORT_CMD\n", __func__, __LINE__);
		msm_camera_io_w_mb(report_val,
			cci_dev->base + CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
			reg_offset);
		read_val++;
		CDBG("%s:%d CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR %d, queue: %d\n",
			__func__, __LINE__, read_val, queue);
		msm_camera_io_w_mb(read_val, cci_dev->base +
			CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR + reg_offset);
		reg_val = 1 << ((master * 2) + queue);
		CDBG("%s:%d CCI_QUEUE_START_ADDR\n", __func__, __LINE__);
		atomic_set(&cci_dev->cci_master_info[master].
						done_pending[queue], 1);
		msm_camera_io_w_mb(reg_val, cci_dev->base +
			CCI_QUEUE_START_ADDR);
		CDBG("%s line %d wait_for_completion_timeout\n",
			__func__, __LINE__);
		atomic_set(&cci_dev->cci_master_info[master].q_free[queue], 1);
		rc = wait_for_completion_timeout(&cci_dev->
			cci_master_info[master].report_q[queue], CCI_TIMEOUT);
		if (rc <= 0) {
			pr_err("%s: wait_for_completion_timeout %d\n",
				 __func__, __LINE__);
			if (rc == 0)
				rc = -ETIMEDOUT;
			msm_cci_flush_queue(cci_dev, master);
			return rc;
		}
		rc = cci_dev->cci_master_info[master].status;
		if (rc < 0)
			pr_err("%s failed rc %d\n", __func__, rc);
	}
	return rc;
}

static int32_t msm_cci_write_i2c_queue(struct cci_device *cci_dev,
	uint32_t val,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	int32_t rc = 0;
	uint32_t reg_offset = master * 0x200 + queue * 0x100;

	if (!cci_dev) {
		pr_err("%s: failed %d", __func__, __LINE__);
		return -EINVAL;
	}

	CDBG("%s:%d called\n", __func__, __LINE__);
	rc = msm_cci_validate_queue(cci_dev, 1, master, queue);
	if (rc < 0) {
		pr_err("%s: failed %d", __func__, __LINE__);
		return rc;
	}
	CDBG("%s CCI_I2C_M0_Q0_LOAD_DATA_ADDR:val 0x%x:0x%x\n",
		__func__, CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
		reg_offset, val);
	msm_camera_io_w_mb(val, cci_dev->base + CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
		reg_offset);
	return rc;
}

static uint32_t msm_cci_wait(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	int32_t rc = 0;

	if (!cci_dev) {
		pr_err("%s: failed %d", __func__, __LINE__);
		return -EINVAL;
	}

	rc = wait_for_completion_timeout(&cci_dev->
		cci_master_info[master].report_q[queue], CCI_TIMEOUT);
	CDBG("%s line %d wait DONE_for_completion_timeout\n",
		__func__, __LINE__);

	if (rc <= 0) {
		msm_cci_dump_registers(cci_dev, master, queue);
		pr_err("%s: %d wait for queue: %d\n",
			 __func__, __LINE__, queue);
		if (rc == 0)
			rc = -ETIMEDOUT;
		msm_cci_flush_queue(cci_dev, master);
		return rc;
	}
	rc = cci_dev->cci_master_info[master].status;
	if (rc < 0) {
		pr_err("%s: %d failed rc %d\n", __func__, __LINE__, rc);
		return rc;
	}
	return 0;
}

static int32_t msm_cci_addr_to_num_bytes(
	enum msm_camera_i2c_reg_addr_type addr_type)
{
	int32_t retVal;

	switch (addr_type) {
	case MSM_CAMERA_I2C_BYTE_ADDR:
		retVal = 1;
		break;
	case MSM_CAMERA_I2C_WORD_ADDR:
		retVal = 2;
		break;
	case MSM_CAMERA_I2C_3B_ADDR:
		retVal = 3;
		break;
	default:
		pr_err("%s: %d failed: %d\n", __func__, __LINE__, addr_type);
		retVal = 1;
		break;
	}
	return retVal;
}

static int32_t msm_cci_data_to_num_bytes(
	enum msm_camera_i2c_data_type data_type)
{
	int32_t retVal;

	switch (data_type) {
	case MSM_CAMERA_I2C_SET_BYTE_WRITE_MASK_DATA:
	case MSM_CAMERA_I2C_SET_BYTE_MASK:
	case MSM_CAMERA_I2C_UNSET_BYTE_MASK:
	case MSM_CAMERA_I2C_BYTE_DATA:
		retVal = 1;
		break;
	case MSM_CAMERA_I2C_SET_WORD_MASK:
	case MSM_CAMERA_I2C_UNSET_WORD_MASK:
	case MSM_CAMERA_I2C_WORD_DATA:
		retVal = 2;
		break;
	case MSM_CAMERA_I2C_DWORD_DATA:
		retVal = 4;
		break;
	default:
		pr_err("%s: %d failed: %d\n", __func__, __LINE__, data_type);
		retVal = 1;
		break;
	}
	return retVal;
}

static int32_t msm_cci_calc_cmd_len(struct cci_device *cci_dev,
	struct msm_camera_cci_ctrl *c_ctrl, uint32_t cmd_size,
	 struct msm_camera_i2c_reg_array *i2c_cmd, uint32_t *pack)
{
	uint8_t i;
	uint32_t len = 0;
	uint8_t data_len = 0, addr_len = 0;
	uint8_t pack_max_len;
	struct msm_camera_i2c_reg_setting *msg;
	struct msm_camera_i2c_reg_array *cmd = i2c_cmd;
	uint32_t size = cmd_size;

	if (!cci_dev || !c_ctrl) {
		pr_err("%s: failed %d", __func__, __LINE__);
		return -EINVAL;
	}

	msg = &c_ctrl->cfg.cci_i2c_write_cfg;
	*pack = 0;

	if (c_ctrl->cmd == MSM_CCI_I2C_WRITE_SEQ) {
		addr_len = msm_cci_addr_to_num_bytes(msg->addr_type);
		len = (size + addr_len) <= (cci_dev->payload_size) ?
			(size + addr_len):cci_dev->payload_size;
	} else {
		addr_len = msm_cci_addr_to_num_bytes(msg->addr_type);
		data_len = msm_cci_data_to_num_bytes(msg->data_type);
		len = data_len + addr_len;
		pack_max_len = size < (cci_dev->payload_size-len) ?
			size : (cci_dev->payload_size-len);
		for (i = 0; i < pack_max_len;) {
			if (cmd->delay || ((cmd - i2c_cmd) >= (cmd_size - 1)))
				break;
			if (cmd->reg_addr + 1 ==
				(cmd+1)->reg_addr) {
				len += data_len;
				*pack += data_len;
			} else
				break;
			i += data_len;
			cmd++;
		}
	}

	if (len > cci_dev->payload_size) {
		pr_err("Len error: %d", len);
		return -EINVAL;
	}

	len += 1; /*add i2c WR command*/
	len = len/4 + 1;

	return len;
}

static void msm_cci_load_report_cmd(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	uint32_t reg_offset = master * 0x200 + queue * 0x100;
	uint32_t read_val = msm_camera_io_r_mb(cci_dev->base +
		CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR + reg_offset);
	uint32_t report_val = CCI_I2C_REPORT_CMD | (1 << 8);

	CDBG("%s:%d CCI_I2C_REPORT_CMD curr_w_cnt: %d\n",
		__func__, __LINE__, read_val);
	msm_camera_io_w_mb(report_val,
		cci_dev->base + CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
		reg_offset);
	read_val++;

	CDBG("%s:%d CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR %d\n",
		__func__, __LINE__, read_val);
	msm_camera_io_w_mb(read_val, cci_dev->base +
		CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR + reg_offset);
}

static int32_t msm_cci_wait_report_cmd(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	uint32_t reg_val = 1 << ((master * 2) + queue);
	msm_cci_load_report_cmd(cci_dev, master, queue);
	atomic_set(&cci_dev->cci_master_info[master].q_free[queue], 1);
	atomic_set(&cci_dev->cci_master_info[master].done_pending[queue], 1);
	msm_camera_io_w_mb(reg_val, cci_dev->base +
		CCI_QUEUE_START_ADDR);
	return msm_cci_wait(cci_dev, master, queue);
}

static void msm_cci_process_half_q(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	uint32_t reg_val = 1 << ((master * 2) + queue);
	if (0 == atomic_read(&cci_dev->cci_master_info[master].q_free[queue])) {
		msm_cci_load_report_cmd(cci_dev, master, queue);
		atomic_set(&cci_dev->cci_master_info[master].q_free[queue], 1);
		msm_camera_io_w_mb(reg_val, cci_dev->base +
			CCI_QUEUE_START_ADDR);
	}
}

static int32_t msm_cci_process_full_q(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	int32_t rc = 0;
	if (1 == atomic_read(&cci_dev->cci_master_info[master].q_free[queue])) {
		atomic_set(&cci_dev->cci_master_info[master].
						done_pending[queue], 1);
		rc = msm_cci_wait(cci_dev, master, queue);
		if (rc < 0) {
			pr_err("%s: %d failed rc %d\n", __func__, __LINE__, rc);
			return rc;
		}
	} else {
		rc = msm_cci_wait_report_cmd(cci_dev, master, queue);
		if (rc < 0) {
			pr_err("%s: %d failed rc %d\n", __func__, __LINE__, rc);
			return rc;
		}
	}
	return rc;
}

static int32_t msm_cci_lock_queue(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue, uint32_t en)
{
	uint32_t val;

	if (queue != PRIORITY_QUEUE)
		return 0;

	val = en ? CCI_I2C_LOCK_CMD : CCI_I2C_UNLOCK_CMD;
	return msm_cci_write_i2c_queue(cci_dev, val, master, queue);
}

static int32_t msm_cci_transfer_end(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	int32_t rc = 0;

	if (0 == atomic_read(&cci_dev->cci_master_info[master].q_free[queue])) {
		rc = msm_cci_lock_queue(cci_dev, master, queue, 0);
		if (rc < 0) {
			pr_err("%s failed line %d\n", __func__, __LINE__);
			return rc;
		}
		rc = msm_cci_wait_report_cmd(cci_dev, master, queue);
		if (rc < 0) {
			pr_err("%s: %d failed rc %d\n", __func__, __LINE__, rc);
			return rc;
		}
	} else {
		atomic_set(&cci_dev->cci_master_info[master].
						done_pending[queue], 1);
		rc = msm_cci_wait(cci_dev, master, queue);
		if (rc < 0) {
			pr_err("%s: %d failed rc %d\n", __func__, __LINE__, rc);
			return rc;
		}
		rc = msm_cci_lock_queue(cci_dev, master, queue, 0);
		if (rc < 0) {
			pr_err("%s failed line %d\n", __func__, __LINE__);
			return rc;
		}
		rc = msm_cci_wait_report_cmd(cci_dev, master, queue);
		if (rc < 0) {
			pr_err("%s: %d failed rc %d\n", __func__, __LINE__, rc);
			return rc;
		}
	}
	return rc;
}

static int32_t msm_cci_get_queue_free_size(struct cci_device *cci_dev,
	enum cci_i2c_master_t master,
	enum cci_i2c_queue_t queue)
{
	uint32_t read_val = 0;
	uint32_t reg_offset = master * 0x200 + queue * 0x100;
	read_val = msm_camera_io_r_mb(cci_dev->base +
		CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR + reg_offset);
	CDBG("%s line %d CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR %d max %d\n",
		__func__, __LINE__, read_val,
		cci_dev->cci_i2c_queue_info[master][queue].max_queue_size);
	return (cci_dev->
		cci_i2c_queue_info[master][queue].max_queue_size) -
		read_val;
}

static int32_t msm_cci_data_queue(struct cci_device *cci_dev,
	struct msm_camera_cci_ctrl *c_ctrl, enum cci_i2c_queue_t queue,
	enum cci_i2c_sync sync_en)
{
	uint16_t i = 0, j = 0, k = 0, h = 0, len = 0;
	int32_t rc = 0, free_size = 0, en_seq_write = 0;
	uint32_t cmd = 0, delay = 0;
	uint8_t data[12];
	uint16_t reg_addr = 0;
	struct msm_camera_i2c_reg_setting *i2c_msg =
		&c_ctrl->cfg.cci_i2c_write_cfg;
	uint16_t cmd_size = i2c_msg->size;
	struct msm_camera_i2c_reg_array *i2c_cmd = i2c_msg->reg_setting;
	enum cci_i2c_master_t master = c_ctrl->cci_info->cci_i2c_master;

	uint32_t read_val = 0;
	uint32_t reg_offset;
	uint32_t val = 0;
	uint32_t max_queue_size, queue_size = 0;

	if (i2c_cmd == NULL) {
		pr_err("%s:%d Failed line\n", __func__,
			__LINE__);
		return -EINVAL;
	}

	if ((!cmd_size) || (cmd_size > CCI_I2C_MAX_WRITE)) {
		pr_err("%s:%d failed: invalid cmd_size %d\n",
			__func__, __LINE__, cmd_size);
		return -EINVAL;
	}

	CDBG("%s addr type %d data type %d cmd_size %d\n", __func__,
		i2c_msg->addr_type, i2c_msg->data_type, cmd_size);

	if (i2c_msg->addr_type >= MSM_CAMERA_I2C_ADDR_TYPE_MAX) {
		pr_err("%s:%d failed: invalid addr_type 0x%X\n",
			__func__, __LINE__, i2c_msg->addr_type);
		return -EINVAL;
	}
	if (i2c_msg->data_type >= MSM_CAMERA_I2C_DATA_TYPE_MAX) {
		pr_err("%s:%d failed: invalid data_type 0x%X\n",
			__func__, __LINE__, i2c_msg->data_type);
		return -EINVAL;
	}
	reg_offset = master * 0x200 + queue * 0x100;

	msm_camera_io_w_mb(cci_dev->cci_wait_sync_cfg.cid,
		cci_dev->base + CCI_SET_CID_SYNC_TIMER_ADDR +
		cci_dev->cci_wait_sync_cfg.csid *
		CCI_SET_CID_SYNC_TIMER_OFFSET);

	val = CCI_I2C_SET_PARAM_CMD | c_ctrl->cci_info->sid << 4 |
		c_ctrl->cci_info->retries << 16 |
		c_ctrl->cci_info->id_map << 18;

	CDBG("%s CCI_I2C_M0_Q0_LOAD_DATA_ADDR:val 0x%x:0x%x\n",
		__func__, CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
		reg_offset, val);
	msm_camera_io_w_mb(val, cci_dev->base + CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
		reg_offset);

	atomic_set(&cci_dev->cci_master_info[master].q_free[queue], 0);

	max_queue_size = cci_dev->cci_i2c_queue_info[master][queue].
			max_queue_size;

	if (c_ctrl->cmd == MSM_CCI_I2C_WRITE_SEQ)
		queue_size = max_queue_size;
	else
		queue_size = max_queue_size/2;
	reg_addr = i2c_cmd->reg_addr;

	if (sync_en == MSM_SYNC_ENABLE && cci_dev->valid_sync &&
		cmd_size < max_queue_size) {
		val = CCI_I2C_WAIT_SYNC_CMD |
			((cci_dev->cci_wait_sync_cfg.line) << 4);
		msm_camera_io_w_mb(val,
			cci_dev->base + CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
			reg_offset);
	}

	rc = msm_cci_lock_queue(cci_dev, master, queue, 1);
	if (rc < 0) {
		pr_err("%s failed line %d\n", __func__, __LINE__);
		return rc;
	}

	while (cmd_size) {
		uint32_t pack = 0;
		len = msm_cci_calc_cmd_len(cci_dev, c_ctrl, cmd_size,
			i2c_cmd, &pack);
		if (len <= 0) {
			pr_err("%s failed line %d\n", __func__, __LINE__);
			return -EINVAL;
		}

		read_val = msm_camera_io_r_mb(cci_dev->base +
			CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR + reg_offset);
		CDBG("%s line %d CUR_WORD_CNT_ADDR %d len %d max %d\n",
			__func__, __LINE__, read_val, len, max_queue_size);
		/* + 1 - space alocation for Report CMD */
		if ((read_val + len + 1) > queue_size) {
			if ((read_val + len + 1) > max_queue_size) {
				rc = msm_cci_process_full_q(cci_dev,
					master, queue);
				if (rc < 0) {
					pr_err("%s failed line %d\n",
						__func__, __LINE__);
					return rc;
				}
				continue;
			}
			msm_cci_process_half_q(cci_dev,	master, queue);
		}

		CDBG("%s cmd_size %d addr 0x%x data 0x%x\n", __func__,
			cmd_size, i2c_cmd->reg_addr, i2c_cmd->reg_data);
		delay = i2c_cmd->delay;
		i = 0;
		data[i++] = CCI_I2C_WRITE_CMD;

		/* in case of multiple command
		* MSM_CCI_I2C_WRITE : address is not continuous, so update
		*			address for a new packet.
		* MSM_CCI_I2C_WRITE_SEQ : address is continuous, need to keep
		*			the incremented address for a
		*			new packet */
		if (c_ctrl->cmd == MSM_CCI_I2C_WRITE ||
			c_ctrl->cmd == MSM_CCI_I2C_WRITE_ASYNC ||
			c_ctrl->cmd == MSM_CCI_I2C_WRITE_SYNC ||
			c_ctrl->cmd == MSM_CCI_I2C_WRITE_SYNC_BLOCK)
			reg_addr = i2c_cmd->reg_addr;

		if (en_seq_write == 0) {
			/* either byte or word addr */
			if (i2c_msg->addr_type == MSM_CAMERA_I2C_BYTE_ADDR)
				data[i++] = reg_addr;
			else {
				data[i++] = (reg_addr & 0xFF00) >> 8;
				data[i++] = reg_addr & 0x00FF;
			}
		}
		/* max of 10 data bytes */
		do {
			if (i2c_msg->data_type == MSM_CAMERA_I2C_BYTE_DATA) {
				data[i++] = i2c_cmd->reg_data;
				reg_addr++;
			} else {
				if ((i + 1) <= cci_dev->payload_size) {
					data[i++] = (i2c_cmd->reg_data &
						0xFF00) >> 8; /* MSB */
					data[i++] = i2c_cmd->reg_data &
						0x00FF; /* LSB */
					reg_addr++;
				} else
					break;
			}
			i2c_cmd++;
			--cmd_size;
		} while (((c_ctrl->cmd == MSM_CCI_I2C_WRITE_SEQ) || pack--) &&
				(cmd_size > 0) && (i <= cci_dev->payload_size));
		free_size = msm_cci_get_queue_free_size(cci_dev, master,
				queue);
		if ((c_ctrl->cmd == MSM_CCI_I2C_WRITE_SEQ) &&
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

		read_val = msm_camera_io_r_mb(cci_dev->base +
			CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR + reg_offset);
		for (h = 0, k = 0; h < len; h++) {
			cmd = 0;
			for (j = 0; (j < 4 && k < i); j++)
				cmd |= (data[k++] << (j * 8));
			CDBG("%s LOAD_DATA_ADDR 0x%x, q: %d, len:%d, cnt: %d\n",
				__func__, cmd, queue, len, read_val);
			msm_camera_io_w_mb(cmd, cci_dev->base +
				CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
				master * 0x200 + queue * 0x100);

			read_val += 1;
			msm_camera_io_w_mb(read_val, cci_dev->base +
				CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR + reg_offset);
		}

		if ((delay > 0) && (delay < CCI_MAX_DELAY) &&
			en_seq_write == 0) {
			cmd = (uint32_t)((delay * cci_dev->cycles_per_us) /
				0x100);
			cmd <<= 4;
			cmd |= CCI_I2C_WAIT_CMD;
			CDBG("%s CCI_I2C_M0_Q0_LOAD_DATA_ADDR 0x%x\n",
				__func__, cmd);
			msm_camera_io_w_mb(cmd, cci_dev->base +
				CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
				master * 0x200 + queue * 0x100);
			read_val += 1;
			msm_camera_io_w_mb(read_val, cci_dev->base +
				CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR + reg_offset);
		}
	}

	rc = msm_cci_transfer_end(cci_dev, master, queue);
	if (rc < 0) {
		pr_err("%s: %d failed rc %d\n", __func__, __LINE__, rc);
		return rc;
	}
	return rc;
}

static int32_t msm_cci_i2c_read(struct v4l2_subdev *sd,
	struct msm_camera_cci_ctrl *c_ctrl)
{
	int32_t rc = 0;
	uint32_t val = 0;
	int32_t read_words = 0, exp_words = 0;
	int32_t index = 0, first_byte = 0;
	uint32_t i = 0;
	enum cci_i2c_master_t master;
	enum cci_i2c_queue_t queue = QUEUE_1;
	struct cci_device *cci_dev = NULL;
	struct msm_camera_cci_i2c_read_cfg *read_cfg = NULL;
	CDBG("%s line %d\n", __func__, __LINE__);
	cci_dev = v4l2_get_subdevdata(sd);
	master = c_ctrl->cci_info->cci_i2c_master;
	read_cfg = &c_ctrl->cfg.cci_i2c_read_cfg;
	mutex_lock(&cci_dev->cci_master_info[master].mutex_q[queue]);

	/* Set the I2C Frequency */
	rc = msm_cci_set_clk_param(cci_dev, c_ctrl);
	if (rc < 0) {
		pr_err("%s:%d msm_cci_set_clk_param failed rc = %d\n",
			__func__, __LINE__, rc);
		return rc;
	}

	/*
	 * Call validate queue to make sure queue is empty before starting.
	 * If this call fails, don't proceed with i2c_read call. This is to
	 * avoid overflow / underflow of queue
	 */
	rc = msm_cci_validate_queue(cci_dev,
		cci_dev->cci_i2c_queue_info[master][queue].max_queue_size - 1,
		master, queue);
	if (rc < 0) {
		pr_err("%s:%d Initial validataion failed rc %d\n", __func__,
			__LINE__, rc);
		goto ERROR;
	}

	if (c_ctrl->cci_info->retries > CCI_I2C_READ_MAX_RETRIES) {
		pr_err("%s:%d More than max retries\n", __func__,
			__LINE__);
		goto ERROR;
	}

	if (read_cfg->data == NULL) {
		pr_err("%s:%d Data ptr is NULL\n", __func__,
			__LINE__);
		goto ERROR;
	}

	CDBG("%s master %d, queue %d\n", __func__, master, queue);
	CDBG("%s set param sid 0x%x retries %d id_map %d\n", __func__,
		c_ctrl->cci_info->sid, c_ctrl->cci_info->retries,
		c_ctrl->cci_info->id_map);
	val = CCI_I2C_SET_PARAM_CMD | c_ctrl->cci_info->sid << 4 |
		c_ctrl->cci_info->retries << 16 |
		c_ctrl->cci_info->id_map << 18;
	rc = msm_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CDBG("%s failed line %d\n", __func__, __LINE__);
		goto ERROR;
	}

	val = CCI_I2C_LOCK_CMD;
	rc = msm_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CDBG("%s failed line %d\n", __func__, __LINE__);
		goto ERROR;
	}

	if (read_cfg->addr_type >= MSM_CAMERA_I2C_ADDR_TYPE_MAX) {
		CDBG("%s failed line %d\n", __func__, __LINE__);
		goto ERROR;
	}

	val = CCI_I2C_WRITE_DISABLE_P_CMD | (read_cfg->addr_type << 4);
	for (i = 0; i < read_cfg->addr_type; i++) {
		val |= ((read_cfg->addr >> (i << 3)) & 0xFF)  <<
		((read_cfg->addr_type - i) << 3);
	}

	rc = msm_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CDBG("%s failed line %d\n", __func__, __LINE__);
		goto ERROR;
	}

	val = CCI_I2C_READ_CMD | (read_cfg->num_byte << 4);
	rc = msm_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CDBG("%s failed line %d\n", __func__, __LINE__);
		goto ERROR;
	}

	val = CCI_I2C_UNLOCK_CMD;
	rc = msm_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CDBG("%s failed line %d\n", __func__, __LINE__);
		goto ERROR;
	}

	val = msm_camera_io_r_mb(cci_dev->base + CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR
			+ master * 0x200 + queue * 0x100);
	CDBG("%s cur word cnt 0x%x\n", __func__, val);
	msm_camera_io_w_mb(val, cci_dev->base + CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR
			+ master * 0x200 + queue * 0x100);

	val = 1 << ((master * 2) + queue);
	msm_camera_io_w_mb(val, cci_dev->base + CCI_QUEUE_START_ADDR);
	CDBG("%s:%d E wait_for_completion_timeout\n", __func__,
		__LINE__);

	rc = wait_for_completion_timeout(&cci_dev->
		cci_master_info[master].reset_complete, CCI_TIMEOUT);
	if (rc <= 0) {
		msm_cci_dump_registers(cci_dev, master, queue);
		if (rc == 0)
			rc = -ETIMEDOUT;
		pr_err("%s: %d wait_for_completion_timeout rc = %d\n",
			 __func__, __LINE__, rc);
		msm_cci_flush_queue(cci_dev, master);
		goto ERROR;
	} else {
		rc = 0;
	}

	read_words = msm_camera_io_r_mb(cci_dev->base +
		CCI_I2C_M0_READ_BUF_LEVEL_ADDR + master * 0x100);
	exp_words = ((read_cfg->num_byte / 4) + 1);
	if (read_words != exp_words) {
		pr_err("%s:%d read_words = %d, exp words = %d\n", __func__,
			__LINE__, read_words, exp_words);
		memset(read_cfg->data, 0, read_cfg->num_byte);
		rc = -EINVAL;
		goto ERROR;
	}
	index = 0;
	CDBG("%s index %d num_type %d\n", __func__, index,
		read_cfg->num_byte);
	first_byte = 0;
	do {
		val = msm_camera_io_r_mb(cci_dev->base +
			CCI_I2C_M0_READ_DATA_ADDR + master * 0x100);
		CDBG("%s read val 0x%x\n", __func__, val);
		for (i = 0; (i < 4) && (index < read_cfg->num_byte); i++) {
			CDBG("%s i %d index %d\n", __func__, i, index);
			if (!first_byte) {
				CDBG("%s sid 0x%x\n", __func__, val & 0xFF);
				first_byte++;
			} else {
				read_cfg->data[index] =
					(val  >> (i * 8)) & 0xFF;
				CDBG("%s data[%d] 0x%x\n", __func__, index,
					read_cfg->data[index]);
				index++;
			}
		}
	} while (--read_words > 0);
ERROR:
	mutex_unlock(&cci_dev->cci_master_info[master].mutex_q[queue]);
	return rc;
}

static int32_t msm_cci_i2c_read_bytes(struct v4l2_subdev *sd,
	struct msm_camera_cci_ctrl *c_ctrl)
{
	int32_t rc = 0;
	struct cci_device *cci_dev = NULL;
	enum cci_i2c_master_t master;
	struct msm_camera_cci_i2c_read_cfg *read_cfg = NULL;
	uint16_t read_bytes = 0;

	if (!sd || !c_ctrl) {
		pr_err("%s:%d sd %pK c_ctrl %pK\n", __func__,
			__LINE__, sd, c_ctrl);
		return -EINVAL;
	}
	if (!c_ctrl->cci_info) {
		pr_err("%s:%d cci_info NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	cci_dev = v4l2_get_subdevdata(sd);
	if (!cci_dev) {
		pr_err("%s:%d cci_dev NULL\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (cci_dev->cci_state != CCI_STATE_ENABLED) {
		pr_err("%s invalid cci state %d\n",
			__func__, cci_dev->cci_state);
		return -EINVAL;
	}

	if (c_ctrl->cci_info->cci_i2c_master >= MASTER_MAX
			|| c_ctrl->cci_info->cci_i2c_master < 0) {
		pr_err("%s:%d Invalid I2C master addr\n", __func__, __LINE__);
		return -EINVAL;
	}

	master = c_ctrl->cci_info->cci_i2c_master;
	read_cfg = &c_ctrl->cfg.cci_i2c_read_cfg;
	if ((!read_cfg->num_byte) || (read_cfg->num_byte > CCI_I2C_MAX_READ)) {
		pr_err("%s:%d read num bytes 0\n", __func__, __LINE__);
		rc = -EINVAL;
		goto ERROR;
	}

	read_bytes = read_cfg->num_byte;
	do {
		if (read_bytes > CCI_READ_MAX)
			read_cfg->num_byte = CCI_READ_MAX;
		else
			read_cfg->num_byte = read_bytes;
		rc = msm_cci_i2c_read(sd, c_ctrl);
		if (rc < 0) {
			pr_err("%s:%d failed rc %d\n", __func__, __LINE__, rc);
			goto ERROR;
		}
		if (read_bytes > CCI_READ_MAX) {
			read_cfg->addr += CCI_READ_MAX;
			read_cfg->data += CCI_READ_MAX;
			read_bytes -= CCI_READ_MAX;
		} else {
			read_bytes = 0;
		}
	} while (read_bytes);
ERROR:
	return rc;
}

static int32_t msm_cci_i2c_write(struct v4l2_subdev *sd,
	struct msm_camera_cci_ctrl *c_ctrl, enum cci_i2c_queue_t queue,
	enum cci_i2c_sync sync_en)
{
	int32_t rc = 0;
	struct cci_device *cci_dev;
	enum cci_i2c_master_t master;

	cci_dev = v4l2_get_subdevdata(sd);
	if (c_ctrl->cci_info->cci_i2c_master >= MASTER_MAX
			|| c_ctrl->cci_info->cci_i2c_master < 0) {
		pr_err("%s:%d Invalid I2C master addr\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (cci_dev->cci_state != CCI_STATE_ENABLED) {
		pr_err("%s invalid cci state %d\n",
			__func__, cci_dev->cci_state);
		return -EINVAL;
	}
	master = c_ctrl->cci_info->cci_i2c_master;
	CDBG("%s set param sid 0x%x retries %d id_map %d\n", __func__,
		c_ctrl->cci_info->sid, c_ctrl->cci_info->retries,
		c_ctrl->cci_info->id_map);

	/* Set the I2C Frequency */
	rc = msm_cci_set_clk_param(cci_dev, c_ctrl);
	if (rc < 0) {
		pr_err("%s:%d msm_cci_set_clk_param failed rc = %d\n",
			__func__, __LINE__, rc);
		return rc;
	}
	/*
	 * Call validate queue to make sure queue is empty before starting.
	 * If this call fails, don't proceed with i2c_write call. This is to
	 * avoid overflow / underflow of queue
	 */
	rc = msm_cci_validate_queue(cci_dev,
		cci_dev->cci_i2c_queue_info[master][queue].max_queue_size-1,
		master, queue);
	if (rc < 0) {
		pr_err("%s:%d Initial validataion failed rc %d\n",
		__func__, __LINE__, rc);
		goto ERROR;
	}
	if (c_ctrl->cci_info->retries > CCI_I2C_READ_MAX_RETRIES) {
		pr_err("%s:%d More than max retries\n", __func__,
			__LINE__);
		goto ERROR;
	}
	rc = msm_cci_data_queue(cci_dev, c_ctrl, queue, sync_en);
	if (rc < 0) {
		CDBG("%s failed line %d\n", __func__, __LINE__);
		goto ERROR;
	}

ERROR:
	return rc;
}

static void msm_cci_write_async_helper(struct work_struct *work)
{
	int rc;
	struct cci_device *cci_dev;
	struct cci_write_async *write_async =
		container_of(work, struct cci_write_async, work);
	struct msm_camera_i2c_reg_setting *i2c_msg;
	enum cci_i2c_master_t master;
	struct msm_camera_cci_master_info *cci_master_info;

	cci_dev = write_async->cci_dev;
	i2c_msg = &write_async->c_ctrl.cfg.cci_i2c_write_cfg;
	master = write_async->c_ctrl.cci_info->cci_i2c_master;
	cci_master_info = &cci_dev->cci_master_info[master];

	mutex_lock(&cci_master_info->mutex_q[write_async->queue]);
	rc = msm_cci_i2c_write(&cci_dev->msm_sd.sd,
		&write_async->c_ctrl, write_async->queue, write_async->sync_en);
	mutex_unlock(&cci_master_info->mutex_q[write_async->queue]);
	if (rc < 0)
		pr_err("%s: %d failed\n", __func__, __LINE__);

	kfree(write_async->c_ctrl.cfg.cci_i2c_write_cfg.reg_setting);
	kfree(write_async);

	CDBG("%s: %d Exit\n", __func__, __LINE__);
}

static int32_t msm_cci_i2c_write_async(struct v4l2_subdev *sd,
	struct msm_camera_cci_ctrl *c_ctrl, enum cci_i2c_queue_t queue,
	enum cci_i2c_sync sync_en)
{
	struct cci_write_async *write_async;
	struct cci_device *cci_dev;
	struct msm_camera_i2c_reg_setting *cci_i2c_write_cfg;
	struct msm_camera_i2c_reg_setting *cci_i2c_write_cfg_w;

	cci_dev = v4l2_get_subdevdata(sd);

	CDBG("%s: %d Enter\n", __func__, __LINE__);

	write_async = kzalloc(sizeof(*write_async), GFP_KERNEL);
	if (!write_async) {
		pr_err("%s: %d Couldn't allocate memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	INIT_WORK(&write_async->work, msm_cci_write_async_helper);
	write_async->cci_dev = cci_dev;
	write_async->c_ctrl = *c_ctrl;
	write_async->queue = queue;
	write_async->sync_en = sync_en;

	cci_i2c_write_cfg = &c_ctrl->cfg.cci_i2c_write_cfg;
	cci_i2c_write_cfg_w = &write_async->c_ctrl.cfg.cci_i2c_write_cfg;

	if (cci_i2c_write_cfg->size == 0) {
		pr_err("%s: %d Size = 0\n", __func__, __LINE__);
		kfree(write_async);
		return -EINVAL;
	}

	cci_i2c_write_cfg_w->reg_setting =
		kzalloc(sizeof(struct msm_camera_i2c_reg_array)*
		cci_i2c_write_cfg->size, GFP_KERNEL);
	if (!cci_i2c_write_cfg_w->reg_setting) {
		pr_err("%s: %d Couldn't allocate memory\n", __func__, __LINE__);
		kfree(write_async);
		return -ENOMEM;
	}
	memcpy(cci_i2c_write_cfg_w->reg_setting,
		cci_i2c_write_cfg->reg_setting,
		(sizeof(struct msm_camera_i2c_reg_array)*
						cci_i2c_write_cfg->size));

	cci_i2c_write_cfg_w->addr_type = cci_i2c_write_cfg->addr_type;
	cci_i2c_write_cfg_w->data_type = cci_i2c_write_cfg->data_type;
	cci_i2c_write_cfg_w->size = cci_i2c_write_cfg->size;
	cci_i2c_write_cfg_w->delay = cci_i2c_write_cfg->delay;

	queue_work(cci_dev->write_wq[write_async->queue], &write_async->work);

	CDBG("%s: %d Exit\n", __func__, __LINE__);

	return 0;
}

static int32_t msm_cci_pinctrl_init(struct cci_device *cci_dev)
{
	struct msm_pinctrl_info *cci_pctrl = NULL;

	cci_pctrl = &cci_dev->cci_pinctrl;
	cci_pctrl->pinctrl = devm_pinctrl_get(&cci_dev->pdev->dev);
	if (IS_ERR_OR_NULL(cci_pctrl->pinctrl)) {
		pr_err("%s:%d devm_pinctrl_get cci_pinctrl failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	cci_pctrl->gpio_state_active = pinctrl_lookup_state(
						cci_pctrl->pinctrl,
						CCI_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(cci_pctrl->gpio_state_active)) {
		pr_err("%s:%d look up state  for active state failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	cci_pctrl->gpio_state_suspend = pinctrl_lookup_state(
						cci_pctrl->pinctrl,
						CCI_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(cci_pctrl->gpio_state_suspend)) {
		pr_err("%s:%d look up state for suspend state failed\n",
			__func__, __LINE__);
		return -EINVAL;
	}
	return 0;
}

static uint32_t msm_cci_cycles_per_ms(unsigned long clk)
{
	uint32_t cycles_per_us;

	if (clk)
		cycles_per_us = ((clk/1000)*256)/1000;
	else {
		pr_err("%s:%d, failed: Can use default: %d",
			__func__, __LINE__, CYCLES_PER_MICRO_SEC_DEFAULT);
		cycles_per_us = CYCLES_PER_MICRO_SEC_DEFAULT;
	}
	return cycles_per_us;
}

static struct msm_cam_clk_info *msm_cci_get_clk(struct cci_device *cci_dev,
	struct msm_camera_cci_ctrl *c_ctrl)
{
	uint32_t j;
	int32_t idx;
	uint32_t cci_clk_src;
	unsigned long clk;

	struct msm_cci_clk_params_t *clk_params = NULL;
	enum i2c_freq_mode_t i2c_freq_mode = c_ctrl->cci_info->i2c_freq_mode;
	struct device_node *of_node = cci_dev->pdev->dev.of_node;
	clk_params = &cci_dev->cci_clk_params[i2c_freq_mode];
	cci_clk_src = clk_params->cci_clk_src;

	idx = of_property_match_string(of_node,
		"clock-names", CCI_CLK_SRC_NAME);
	if (idx < 0) {
		cci_dev->cycles_per_us = CYCLES_PER_MICRO_SEC_DEFAULT;
		return &cci_clk_info[0][0];
	}

	if (cci_clk_src == 0) {
		clk = cci_clk_info[0][idx].clk_rate;
		cci_dev->cycles_per_us = msm_cci_cycles_per_ms(clk);
		return &cci_clk_info[0][0];
	}

	for (j = 0; j < cci_dev->num_clk_cases; j++) {
		clk = cci_clk_info[j][idx].clk_rate;
		if (clk == cci_clk_src) {
			cci_dev->cycles_per_us = msm_cci_cycles_per_ms(clk);
			cci_dev->cci_clk_src = cci_clk_src;
			return &cci_clk_info[j][0];
		}
	}

	return NULL;
}

static int32_t msm_cci_i2c_set_sync_prms(struct v4l2_subdev *sd,
	struct msm_camera_cci_ctrl *c_ctrl)
{
	int32_t rc = 0;
	struct cci_device *cci_dev;

	cci_dev = v4l2_get_subdevdata(sd);
	if (!cci_dev || !c_ctrl) {
		pr_err("%s:%d failed: invalid params %pK %pK\n", __func__,
			__LINE__, cci_dev, c_ctrl);
		rc = -EINVAL;
		return rc;
	}
	cci_dev->cci_wait_sync_cfg = c_ctrl->cfg.cci_wait_sync_cfg;
	cci_dev->valid_sync = cci_dev->cci_wait_sync_cfg.csid < 0 ? 0 : 1;

	return rc;
}

static int32_t msm_cci_init(struct v4l2_subdev *sd,
	struct msm_camera_cci_ctrl *c_ctrl)
{
	uint8_t i = 0;
	int32_t rc = 0, ret = 0;
	struct cci_device *cci_dev;
	enum cci_i2c_master_t master = MASTER_0;
	struct msm_cam_clk_info *clk_info = NULL;

	cci_dev = v4l2_get_subdevdata(sd);
	if (!cci_dev || !c_ctrl) {
		pr_err("%s:%d failed: invalid params %pK %pK\n", __func__,
			__LINE__, cci_dev, c_ctrl);
		rc = -EINVAL;
		return rc;
	}

	rc = cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CCI,
			CAM_AHB_SVS_VOTE);
	if (rc < 0) {
		pr_err("%s: failed to vote for AHB\n", __func__);
		return rc;
	}

	if (cci_dev->ref_count++) {
		CDBG("%s ref_count %d\n", __func__, cci_dev->ref_count);
		master = c_ctrl->cci_info->cci_i2c_master;
		CDBG("%s:%d master %d\n", __func__, __LINE__, master);
		if (master < MASTER_MAX && master >= 0) {
			mutex_lock(&cci_dev->cci_master_info[master].mutex);
			flush_workqueue(cci_dev->write_wq[master]);
			/* Re-initialize the completion */
			reinit_completion(&cci_dev->
				cci_master_info[master].reset_complete);
			for (i = 0; i < NUM_QUEUES; i++)
				reinit_completion(&cci_dev->
					cci_master_info[master].report_q[i]);
			/* Set reset pending flag to TRUE */
			cci_dev->cci_master_info[master].reset_pending = TRUE;
			/* Set proper mask to RESET CMD address */
			if (master == MASTER_0)
				msm_camera_io_w_mb(CCI_M0_RESET_RMSK,
					cci_dev->base + CCI_RESET_CMD_ADDR);
			else
				msm_camera_io_w_mb(CCI_M1_RESET_RMSK,
					cci_dev->base + CCI_RESET_CMD_ADDR);
			/* wait for reset done irq */
			rc = wait_for_completion_timeout(
				&cci_dev->cci_master_info[master].
				reset_complete,
				CCI_TIMEOUT);
			if (rc <= 0)
				pr_err("%s:%d wait failed %d\n", __func__,
					__LINE__, rc);
			mutex_unlock(&cci_dev->cci_master_info[master].mutex);
		}
		return 0;
	}
	ret = msm_cci_pinctrl_init(cci_dev);
	if (ret < 0) {
		pr_err("%s:%d Initialization of pinctrl failed\n",
				__func__, __LINE__);
		cci_dev->cci_pinctrl_status = 0;
	} else {
		cci_dev->cci_pinctrl_status = 1;
	}
	rc = msm_camera_request_gpio_table(cci_dev->cci_gpio_tbl,
		cci_dev->cci_gpio_tbl_size, 1);
	if (cci_dev->cci_pinctrl_status) {
		ret = pinctrl_select_state(cci_dev->cci_pinctrl.pinctrl,
				cci_dev->cci_pinctrl.gpio_state_active);
		if (ret)
			pr_err("%s:%d cannot set pin to active state\n",
				__func__, __LINE__);
	}
	if (rc < 0) {
		CDBG("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}

	rc = msm_camera_config_vreg(&cci_dev->pdev->dev, cci_dev->cci_vreg,
		cci_dev->regulator_count, NULL, 0, &cci_dev->cci_reg_ptr[0], 1);
	if (rc < 0) {
		pr_err("%s:%d cci config_vreg failed\n", __func__, __LINE__);
		goto clk_enable_failed;
	}

	rc = msm_camera_enable_vreg(&cci_dev->pdev->dev, cci_dev->cci_vreg,
		cci_dev->regulator_count, NULL, 0, &cci_dev->cci_reg_ptr[0], 1);
	if (rc < 0) {
		pr_err("%s:%d cci enable_vreg failed\n", __func__, __LINE__);
		goto reg_enable_failed;
	}

	clk_info = msm_cci_get_clk(cci_dev, c_ctrl);
	if (!clk_info) {
		pr_err("%s: clk enable failed\n", __func__);
		goto reg_enable_failed;
	}

	rc = msm_cam_clk_enable(&cci_dev->pdev->dev, clk_info,
		cci_dev->cci_clk, cci_dev->num_clk, 1);
	if (rc < 0) {
		CDBG("%s: clk enable failed\n", __func__);
		goto reg_enable_failed;
	}
	/* Re-initialize the completion */
	reinit_completion(&cci_dev->cci_master_info[master].reset_complete);
	for (i = 0; i < NUM_QUEUES; i++)
		reinit_completion(&cci_dev->cci_master_info[master].
			report_q[i]);
	enable_irq(cci_dev->irq->start);
	cci_dev->hw_version = msm_camera_io_r_mb(cci_dev->base +
		CCI_HW_VERSION_ADDR);
	pr_info("%s:%d: hw_version = 0x%x\n", __func__, __LINE__,
		cci_dev->hw_version);
	cci_dev->payload_size =
			MSM_CCI_WRITE_DATA_PAYLOAD_SIZE_10;
	cci_dev->support_seq_write = 0;
	if (cci_dev->hw_version >= 0x10020000) {
		cci_dev->payload_size =
			MSM_CCI_WRITE_DATA_PAYLOAD_SIZE_11;
		cci_dev->support_seq_write = 1;
	}
	cci_dev->cci_master_info[MASTER_0].reset_pending = TRUE;
	msm_camera_io_w_mb(CCI_RESET_CMD_RMSK, cci_dev->base +
			CCI_RESET_CMD_ADDR);
	msm_camera_io_w_mb(0x1, cci_dev->base + CCI_RESET_CMD_ADDR);
	rc = wait_for_completion_timeout(
		&cci_dev->cci_master_info[MASTER_0].reset_complete,
		CCI_TIMEOUT);
	if (rc <= 0) {
		pr_err("%s: wait_for_completion_timeout %d\n",
			 __func__, __LINE__);
		if (rc == 0)
			rc = -ETIMEDOUT;
		goto reset_complete_failed;
	}
	for (i = 0; i < MASTER_MAX; i++)
		cci_dev->i2c_freq_mode[i] = I2C_MAX_MODES;
	msm_camera_io_w_mb(CCI_IRQ_MASK_0_RMSK,
		cci_dev->base + CCI_IRQ_MASK_0_ADDR);
	msm_camera_io_w_mb(CCI_IRQ_MASK_0_RMSK,
		cci_dev->base + CCI_IRQ_CLEAR_0_ADDR);
	msm_camera_io_w_mb(0x1, cci_dev->base + CCI_IRQ_GLOBAL_CLEAR_CMD_ADDR);

	for (i = 0; i < MASTER_MAX; i++) {
		if (!cci_dev->write_wq[i]) {
			pr_err("Failed to flush write wq\n");
			rc = -ENOMEM;
			goto reset_complete_failed;
		} else {
			flush_workqueue(cci_dev->write_wq[i]);
		}
	}
	cci_dev->cci_state = CCI_STATE_ENABLED;

	return 0;

reset_complete_failed:
	disable_irq(cci_dev->irq->start);
	msm_cam_clk_enable(&cci_dev->pdev->dev, clk_info,
		cci_dev->cci_clk, cci_dev->num_clk, 0);
reg_enable_failed:
	msm_camera_config_vreg(&cci_dev->pdev->dev, cci_dev->cci_vreg,
		cci_dev->regulator_count, NULL, 0, &cci_dev->cci_reg_ptr[0], 0);
clk_enable_failed:
	if (cci_dev->cci_pinctrl_status) {
		ret = pinctrl_select_state(cci_dev->cci_pinctrl.pinctrl,
				cci_dev->cci_pinctrl.gpio_state_suspend);
		if (ret)
			pr_err("%s:%d cannot set pin to suspend state\n",
				__func__, __LINE__);
	}
	msm_camera_request_gpio_table(cci_dev->cci_gpio_tbl,
		cci_dev->cci_gpio_tbl_size, 0);
request_gpio_failed:
	cci_dev->ref_count--;
	if (cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CCI,
		CAM_AHB_SUSPEND_VOTE) < 0)
		pr_err("%s: failed to remove vote for AHB\n", __func__);
	return rc;
}

static int32_t msm_cci_release(struct v4l2_subdev *sd)
{
	uint8_t i = 0, rc = 0;
	struct cci_device *cci_dev;

	cci_dev = v4l2_get_subdevdata(sd);
	if (!cci_dev->ref_count || cci_dev->cci_state != CCI_STATE_ENABLED) {
		pr_err("%s invalid ref count %d / cci state %d\n",
			__func__, cci_dev->ref_count, cci_dev->cci_state);
		rc = -EINVAL;
		goto ahb_vote_suspend;
	}
	if (--cci_dev->ref_count) {
		CDBG("%s ref_count Exit %d\n", __func__, cci_dev->ref_count);
		rc = 0;
		goto ahb_vote_suspend;
	}
	for (i = 0; i < MASTER_MAX; i++)
		if (cci_dev->write_wq[i])
			flush_workqueue(cci_dev->write_wq[i]);

	disable_irq(cci_dev->irq->start);
	msm_cam_clk_enable(&cci_dev->pdev->dev, &cci_clk_info[0][0],
		cci_dev->cci_clk, cci_dev->num_clk, 0);

	rc = msm_camera_enable_vreg(&cci_dev->pdev->dev, cci_dev->cci_vreg,
		cci_dev->regulator_count, NULL, 0, &cci_dev->cci_reg_ptr[0], 0);
	if (rc < 0)
		pr_err("%s:%d cci disable_vreg failed\n", __func__, __LINE__);

	rc = msm_camera_config_vreg(&cci_dev->pdev->dev, cci_dev->cci_vreg,
		cci_dev->regulator_count, NULL, 0, &cci_dev->cci_reg_ptr[0], 0);
	if (rc < 0)
		pr_err("%s:%d cci unconfig_vreg failed\n", __func__, __LINE__);

	if (cci_dev->cci_pinctrl_status) {
		rc = pinctrl_select_state(cci_dev->cci_pinctrl.pinctrl,
				cci_dev->cci_pinctrl.gpio_state_suspend);
		if (rc)
			pr_err("%s:%d cannot set pin to active state\n",
				__func__, __LINE__);
	}
	cci_dev->cci_pinctrl_status = 0;
	msm_camera_request_gpio_table(cci_dev->cci_gpio_tbl,
		cci_dev->cci_gpio_tbl_size, 0);
	for (i = 0; i < MASTER_MAX; i++)
		cci_dev->i2c_freq_mode[i] = I2C_MAX_MODES;
	cci_dev->cci_state = CCI_STATE_DISABLED;
	cci_dev->cycles_per_us = 0;
	cci_dev->cci_clk_src = 0;

ahb_vote_suspend:
	if (cam_config_ahb_clk(NULL, 0, CAM_AHB_CLIENT_CCI,
		CAM_AHB_SUSPEND_VOTE) < 0)
		pr_err("%s: failed to remove vote for AHB\n", __func__);
	return rc;
}

static int32_t msm_cci_write(struct v4l2_subdev *sd,
	struct msm_camera_cci_ctrl *c_ctrl)
{
	int32_t rc = 0;
	struct cci_device *cci_dev;
	enum cci_i2c_master_t master;
	struct msm_camera_cci_master_info *cci_master_info;
	uint32_t i;

	cci_dev = v4l2_get_subdevdata(sd);
	if (!cci_dev || !c_ctrl) {
		pr_err("%s:%d failed: invalid params %pK %pK\n", __func__,
			__LINE__, cci_dev, c_ctrl);
		rc = -EINVAL;
		return rc;
	}

	master = c_ctrl->cci_info->cci_i2c_master;
	cci_master_info = &cci_dev->cci_master_info[master];

	switch (c_ctrl->cmd) {
	case MSM_CCI_I2C_WRITE_SYNC_BLOCK:
		mutex_lock(&cci_master_info->mutex_q[SYNC_QUEUE]);
		rc = msm_cci_i2c_write(sd, c_ctrl,
			SYNC_QUEUE, MSM_SYNC_ENABLE);
		mutex_unlock(&cci_master_info->mutex_q[SYNC_QUEUE]);
		break;
	case MSM_CCI_I2C_WRITE_SYNC:
		rc = msm_cci_i2c_write_async(sd, c_ctrl,
			SYNC_QUEUE, MSM_SYNC_ENABLE);
		break;
	case MSM_CCI_I2C_WRITE:
	case MSM_CCI_I2C_WRITE_SEQ:
		for (i = 0; i < NUM_QUEUES; i++) {
			if (mutex_trylock(&cci_master_info->mutex_q[i])) {
				rc = msm_cci_i2c_write(sd, c_ctrl, i,
					MSM_SYNC_DISABLE);
				mutex_unlock(&cci_master_info->mutex_q[i]);
				return rc;
			}
		}
		mutex_lock(&cci_master_info->mutex_q[PRIORITY_QUEUE]);
		rc = msm_cci_i2c_write(sd, c_ctrl,
			PRIORITY_QUEUE, MSM_SYNC_DISABLE);
		mutex_unlock(&cci_master_info->mutex_q[PRIORITY_QUEUE]);
		break;
	case MSM_CCI_I2C_WRITE_ASYNC:
		rc = msm_cci_i2c_write_async(sd, c_ctrl,
			PRIORITY_QUEUE, MSM_SYNC_DISABLE);
		break;
	default:
		rc = -ENOIOCTLCMD;
	}
	return rc;
}

static int32_t msm_cci_config(struct v4l2_subdev *sd,
	struct msm_camera_cci_ctrl *cci_ctrl)
{
	int32_t rc = 0;
	CDBG("%s line %d cmd %d\n", __func__, __LINE__,
		cci_ctrl->cmd);
	switch (cci_ctrl->cmd) {
	case MSM_CCI_INIT:
		rc = msm_cci_init(sd, cci_ctrl);
		break;
	case MSM_CCI_RELEASE:
		rc = msm_cci_release(sd);
		break;
	case MSM_CCI_I2C_READ:
		rc = msm_cci_i2c_read_bytes(sd, cci_ctrl);
		break;
	case MSM_CCI_I2C_WRITE:
	case MSM_CCI_I2C_WRITE_SEQ:
	case MSM_CCI_I2C_WRITE_SYNC:
	case MSM_CCI_I2C_WRITE_ASYNC:
	case MSM_CCI_I2C_WRITE_SYNC_BLOCK:
		rc = msm_cci_write(sd, cci_ctrl);
		break;
	case MSM_CCI_GPIO_WRITE:
		break;
	case MSM_CCI_SET_SYNC_CID:
		rc = msm_cci_i2c_set_sync_prms(sd, cci_ctrl);
		break;

	default:
		rc = -ENOIOCTLCMD;
	}
	CDBG("%s line %d rc %d\n", __func__, __LINE__, rc);
	cci_ctrl->status = rc;
	return rc;
}

static irqreturn_t msm_cci_irq(int irq_num, void *data)
{
	uint32_t irq;
	struct cci_device *cci_dev = data;
	irq = msm_camera_io_r_mb(cci_dev->base + CCI_IRQ_STATUS_0_ADDR);
	msm_camera_io_w_mb(irq, cci_dev->base + CCI_IRQ_CLEAR_0_ADDR);
	msm_camera_io_w_mb(0x1, cci_dev->base + CCI_IRQ_GLOBAL_CLEAR_CMD_ADDR);
	CDBG("%s CCI_I2C_M0_STATUS_ADDR = 0x%x\n", __func__, irq);
	if (irq & CCI_IRQ_STATUS_0_RST_DONE_ACK_BMSK) {
		if (cci_dev->cci_master_info[MASTER_0].reset_pending == TRUE) {
			cci_dev->cci_master_info[MASTER_0].reset_pending =
				FALSE;
			complete(&cci_dev->cci_master_info[MASTER_0].
				reset_complete);
		}
		if (cci_dev->cci_master_info[MASTER_1].reset_pending == TRUE) {
			cci_dev->cci_master_info[MASTER_1].reset_pending =
				FALSE;
			complete(&cci_dev->cci_master_info[MASTER_1].
				reset_complete);
		}
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK) {
		cci_dev->cci_master_info[MASTER_0].status = 0;
		complete(&cci_dev->cci_master_info[MASTER_0].reset_complete);
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M0_Q0_REPORT_BMSK) {
		struct msm_camera_cci_master_info *cci_master_info;
		cci_master_info = &cci_dev->cci_master_info[MASTER_0];
		atomic_set(&cci_master_info->q_free[QUEUE_0], 0);
		cci_master_info->status = 0;
		if (atomic_read(&cci_master_info->done_pending[QUEUE_0]) == 1) {
			complete(&cci_master_info->report_q[QUEUE_0]);
			atomic_set(&cci_master_info->done_pending[QUEUE_0], 0);
		}
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M0_Q1_REPORT_BMSK) {
		struct msm_camera_cci_master_info *cci_master_info;
		cci_master_info = &cci_dev->cci_master_info[MASTER_0];
		atomic_set(&cci_master_info->q_free[QUEUE_1], 0);
		cci_master_info->status = 0;
		if (atomic_read(&cci_master_info->done_pending[QUEUE_1]) == 1) {
			complete(&cci_master_info->report_q[QUEUE_1]);
			atomic_set(&cci_master_info->done_pending[QUEUE_1], 0);
		}
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK) {
		cci_dev->cci_master_info[MASTER_1].status = 0;
		complete(&cci_dev->cci_master_info[MASTER_1].reset_complete);
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M1_Q0_REPORT_BMSK) {
		struct msm_camera_cci_master_info *cci_master_info;
		cci_master_info = &cci_dev->cci_master_info[MASTER_1];
		atomic_set(&cci_master_info->q_free[QUEUE_0], 0);
		cci_master_info->status = 0;
		if (atomic_read(&cci_master_info->done_pending[QUEUE_0]) == 1) {
			complete(&cci_master_info->report_q[QUEUE_0]);
			atomic_set(&cci_master_info->done_pending[QUEUE_0], 0);
		}
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M1_Q1_REPORT_BMSK) {
		struct msm_camera_cci_master_info *cci_master_info;
		cci_master_info = &cci_dev->cci_master_info[MASTER_1];
		atomic_set(&cci_master_info->q_free[QUEUE_1], 0);
		cci_master_info->status = 0;
		if (atomic_read(&cci_master_info->done_pending[QUEUE_1]) == 1) {
			complete(&cci_master_info->report_q[QUEUE_1]);
			atomic_set(&cci_master_info->done_pending[QUEUE_1], 0);
		}
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M0_Q0Q1_HALT_ACK_BMSK) {
		cci_dev->cci_master_info[MASTER_0].reset_pending = TRUE;
		msm_camera_io_w_mb(CCI_M0_RESET_RMSK,
			cci_dev->base + CCI_RESET_CMD_ADDR);
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M1_Q0Q1_HALT_ACK_BMSK) {
		cci_dev->cci_master_info[MASTER_1].reset_pending = TRUE;
		msm_camera_io_w_mb(CCI_M1_RESET_RMSK,
			cci_dev->base + CCI_RESET_CMD_ADDR);
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M0_ERROR_BMSK) {
		pr_err("%s:%d MASTER_0 error 0x%x\n", __func__, __LINE__, irq);
		cci_dev->cci_master_info[MASTER_0].status = -EINVAL;
		msm_camera_io_w_mb(CCI_M0_HALT_REQ_RMSK,
			cci_dev->base + CCI_HALT_REQ_ADDR);
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M1_ERROR_BMSK) {
		pr_err("%s:%d MASTER_1 error 0x%x\n", __func__, __LINE__, irq);
		cci_dev->cci_master_info[MASTER_1].status = -EINVAL;
		msm_camera_io_w_mb(CCI_M1_HALT_REQ_RMSK,
			cci_dev->base + CCI_HALT_REQ_ADDR);
	}
	return IRQ_HANDLED;
}

static int msm_cci_irq_routine(struct v4l2_subdev *sd, u32 status,
	bool *handled)
{
	struct cci_device *cci_dev = v4l2_get_subdevdata(sd);
	irqreturn_t ret;
	CDBG("%s line %d\n", __func__, __LINE__);
	ret = msm_cci_irq(cci_dev->irq->start, cci_dev);
	CDBG("%s: msm_cci_irq return %d\n", __func__, ret);
	*handled = TRUE;
	return 0;
}

static long msm_cci_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int32_t rc = 0;
	CDBG("%s line %d\n", __func__, __LINE__);
	switch (cmd) {
	case VIDIOC_MSM_CCI_CFG:
		rc = msm_cci_config(sd, arg);
		break;
	case MSM_SD_NOTIFY_FREEZE:
		break;
	case MSM_SD_UNNOTIFY_FREEZE:
		break;
	case MSM_SD_SHUTDOWN: {
		struct msm_camera_cci_ctrl ctrl_cmd;
		ctrl_cmd.cmd = MSM_CCI_RELEASE;
		rc = msm_cci_config(sd, &ctrl_cmd);
		break;
	}
	default:
		rc = -ENOIOCTLCMD;
	}
	CDBG("%s line %d rc %d\n", __func__, __LINE__, rc);
	return rc;
}

static struct v4l2_subdev_core_ops msm_cci_subdev_core_ops = {
	.ioctl = &msm_cci_subdev_ioctl,
	.interrupt_service_routine = msm_cci_irq_routine,
};

static const struct v4l2_subdev_ops msm_cci_subdev_ops = {
	.core = &msm_cci_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops msm_cci_internal_ops;

static void msm_cci_init_cci_params(struct cci_device *new_cci_dev)
{
	uint8_t i = 0, j = 0;
	for (i = 0; i < NUM_MASTERS; i++) {
		new_cci_dev->cci_master_info[i].status = 0;
		mutex_init(&new_cci_dev->cci_master_info[i].mutex);
		init_completion(&new_cci_dev->
			cci_master_info[i].reset_complete);

		for (j = 0; j < NUM_QUEUES; j++) {
			mutex_init(&new_cci_dev->cci_master_info[i].mutex_q[j]);
			init_completion(&new_cci_dev->
				cci_master_info[i].report_q[j]);
			if (j == QUEUE_0)
				new_cci_dev->cci_i2c_queue_info[i][j].
					max_queue_size = CCI_I2C_QUEUE_0_SIZE;
			else
				new_cci_dev->cci_i2c_queue_info[i][j].
					max_queue_size = CCI_I2C_QUEUE_1_SIZE;
			}
	}
	return;
}

static int32_t msm_cci_init_gpio_params(struct cci_device *cci_dev)
{
	int32_t rc = 0, i = 0;
	uint32_t *val_array = NULL;
	uint8_t tbl_size = 0;
	struct device_node *of_node = cci_dev->pdev->dev.of_node;
	struct gpio *gpio_tbl = NULL;

	cci_dev->cci_gpio_tbl_size = tbl_size = of_gpio_count(of_node);
	CDBG("%s gpio count %d\n", __func__, tbl_size);
	if (!tbl_size) {
		pr_err("%s:%d gpio count 0\n", __func__, __LINE__);
		return 0;
	}

	gpio_tbl = cci_dev->cci_gpio_tbl =
		kzalloc(sizeof(struct gpio) * tbl_size, GFP_KERNEL);
	if (!gpio_tbl) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		return 0;
	}

	for (i = 0; i < tbl_size; i++) {
		gpio_tbl[i].gpio = of_get_gpio(of_node, i);
		CDBG("%s gpio_tbl[%d].gpio = %d\n", __func__, i,
			gpio_tbl[i].gpio);
	}

	val_array = kzalloc(sizeof(uint32_t) * tbl_size, GFP_KERNEL);
	if (!val_array) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		rc = -ENOMEM;
		goto ERROR1;
	}

	rc = of_property_read_u32_array(of_node, "qcom,gpio-tbl-flags",
		val_array, tbl_size);
	if (rc < 0) {
		pr_err("%s failed %d\n", __func__, __LINE__);
		goto ERROR2;
	}
	for (i = 0; i < tbl_size; i++) {
		gpio_tbl[i].flags = val_array[i];
		CDBG("%s gpio_tbl[%d].flags = %ld\n", __func__, i,
			gpio_tbl[i].flags);
	}

	for (i = 0; i < tbl_size; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,gpio-tbl-label", i, &gpio_tbl[i].label);
		CDBG("%s gpio_tbl[%d].label = %s\n", __func__, i,
			gpio_tbl[i].label);
		if (rc < 0) {
			pr_err("%s failed %d\n", __func__, __LINE__);
			goto ERROR2;
		}
	}

	kfree(val_array);
	return rc;

ERROR2:
	kfree(val_array);
ERROR1:
	kfree(cci_dev->cci_gpio_tbl);
	cci_dev->cci_gpio_tbl = NULL;
	cci_dev->cci_gpio_tbl_size = 0;
	return rc;
}

static void msm_cci_init_default_clk_params(struct cci_device *cci_dev,
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

static void msm_cci_init_clk_params(struct cci_device *cci_dev)
{
	int32_t rc = 0;
	uint32_t val = 0;
	uint8_t count = 0;
	struct device_node *of_node = cci_dev->pdev->dev.of_node;
	struct device_node *src_node = NULL;

	for (count = 0; count < I2C_MAX_MODES; count++) {

		if (I2C_STANDARD_MODE == count)
			src_node = of_find_node_by_name(of_node,
				"qcom,i2c_standard_mode");
		else if (I2C_FAST_MODE == count)
			src_node = of_find_node_by_name(of_node,
				"qcom,i2c_fast_mode");
		else if (I2C_FAST_PLUS_MODE == count)
			src_node = of_find_node_by_name(of_node,
				"qcom,i2c_fast_plus_mode");
		else
			src_node = of_find_node_by_name(of_node,
				"qcom,i2c_custom_mode");

		rc = of_property_read_u32(src_node, "qcom,hw-thigh", &val);
		CDBG("%s qcom,hw-thigh %d, rc %d\n", __func__, val, rc);
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_thigh = val;
			rc = of_property_read_u32(src_node, "qcom,hw-tlow",
				&val);
			CDBG("%s qcom,hw-tlow %d, rc %d\n", __func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tlow = val;
			rc = of_property_read_u32(src_node, "qcom,hw-tsu-sto",
				&val);
			CDBG("%s qcom,hw-tsu-sto %d, rc %d\n",
				__func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tsu_sto = val;
			rc = of_property_read_u32(src_node, "qcom,hw-tsu-sta",
				&val);
			CDBG("%s qcom,hw-tsu-sta %d, rc %d\n",
				__func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tsu_sta = val;
			rc = of_property_read_u32(src_node, "qcom,hw-thd-dat",
				&val);
			CDBG("%s qcom,hw-thd-dat %d, rc %d\n",
				__func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_thd_dat = val;
			rc = of_property_read_u32(src_node, "qcom,hw-thd-sta",
				&val);
			CDBG("%s qcom,hw-thd-sta %d, rc %d\n", __func__,
				val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_thd_sta = val;
			rc = of_property_read_u32(src_node, "qcom,hw-tbuf",
				&val);
			CDBG("%s qcom,hw-tbuf %d, rc %d\n", __func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tbuf = val;
			rc = of_property_read_u32(src_node,
				"qcom,hw-scl-stretch-en", &val);
			CDBG("%s qcom,hw-scl-stretch-en %d, rc %d\n",
				__func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_scl_stretch_en = val;
			rc = of_property_read_u32(src_node, "qcom,hw-trdhld",
				&val);
			CDBG("%s qcom,hw-trdhld %d, rc %d\n",
				__func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_trdhld = val;
			rc = of_property_read_u32(src_node, "qcom,hw-tsp",
				&val);
			CDBG("%s qcom,hw-tsp %d, rc %d\n", __func__, val, rc);
		}
		if (!rc) {
			cci_dev->cci_clk_params[count].hw_tsp = val;
			val = 0;
			rc = of_property_read_u32(src_node, "qcom,cci-clk-src",
				&val);
			CDBG("%s qcom,cci-clk-src %d, rc %d\n",
				__func__, val, rc);
			cci_dev->cci_clk_params[count].cci_clk_src = val;
		}
		else
			msm_cci_init_default_clk_params(cci_dev, count);



		of_node_put(src_node);
		src_node = NULL;
	}
	return;
}

struct v4l2_subdev *msm_cci_get_subdev(void)
{
	return g_cci_subdev;
}

static int msm_cci_get_clk_info(struct cci_device *cci_dev,
	struct platform_device *pdev)
{
	uint32_t count;
	uint32_t count_r;
	int i, j, rc;
	const uint32_t *p;
	int index = 0;

	struct device_node *of_node;
	of_node = pdev->dev.of_node;

	count = of_property_count_strings(of_node, "clock-names");
	cci_dev->num_clk = count;

	CDBG("%s: count = %d\n", __func__, count);
	if (count == 0) {
		pr_err("%s: no clocks found in device tree, count=%d",
			__func__, count);
		return 0;
	}

	if (count > CCI_NUM_CLK_MAX) {
		pr_err("%s: invalid count=%d, max is %d\n", __func__,
			count, CCI_NUM_CLK_MAX);
		return -EINVAL;
	}

	p = of_get_property(of_node, "qcom,clock-rates", &count_r);
	if (!p || !count_r) {
		pr_err("failed\n");
		return -EINVAL;
	}

	count_r /= sizeof(uint32_t);
	cci_dev->num_clk_cases = count_r/count;

	if (cci_dev->num_clk_cases > CCI_NUM_CLK_CASES) {
		pr_err("%s: invalid count=%d, max is %d\n", __func__,
			cci_dev->num_clk_cases, CCI_NUM_CLK_CASES);
		return -EINVAL;
	}

	index = 0;
	for (i = 0; i < count_r/count; i++) {
		for (j = 0; j < count; j++) {
			rc = of_property_read_string_index(of_node,
				"clock-names", j,
				&(cci_clk_info[i][j].clk_name));
			CDBG("%s: clock-names[%d][%d] = %s\n", __func__,
				i, j, cci_clk_info[i][j].clk_name);
			if (rc < 0) {
				pr_err("%s:%d, failed\n", __func__, __LINE__);
				return rc;
			}

			cci_clk_info[i][j].clk_rate =
				(be32_to_cpu(p[index]) == 0) ?
					(long)-1 : be32_to_cpu(p[index]);
			CDBG("%s: clk_rate[%d][%d] = %ld\n", __func__, i, j,
				cci_clk_info[i][j].clk_rate);
			index++;
		}
	}
	return 0;
}



static int msm_cci_probe(struct platform_device *pdev)
{
	struct cci_device *new_cci_dev;
	int rc = 0, i = 0;
	CDBG("%s: pdev %pK device id = %d\n", __func__, pdev, pdev->id);
	new_cci_dev = kzalloc(sizeof(struct cci_device), GFP_KERNEL);
	if (!new_cci_dev) {
		pr_err("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}
	v4l2_subdev_init(&new_cci_dev->msm_sd.sd, &msm_cci_subdev_ops);
	new_cci_dev->msm_sd.sd.internal_ops = &msm_cci_internal_ops;
	snprintf(new_cci_dev->msm_sd.sd.name,
			ARRAY_SIZE(new_cci_dev->msm_sd.sd.name), "msm_cci");
	v4l2_set_subdevdata(&new_cci_dev->msm_sd.sd, new_cci_dev);
	platform_set_drvdata(pdev, &new_cci_dev->msm_sd.sd);
	CDBG("%s sd %pK\n", __func__, &new_cci_dev->msm_sd.sd);
	if (pdev->dev.of_node)
		of_property_read_u32((&pdev->dev)->of_node,
			"cell-index", &pdev->id);

	rc = msm_cci_get_clk_info(new_cci_dev, pdev);
	if (rc < 0) {
		pr_err("%s: msm_cci_get_clk_info() failed", __func__);
		kfree(new_cci_dev);
		return -EFAULT;
	}

	new_cci_dev->ref_count = 0;
	new_cci_dev->mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "cci");
	if (!new_cci_dev->mem) {
		pr_err("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto cci_no_resource;
	}
	new_cci_dev->irq = platform_get_resource_byname(pdev,
					IORESOURCE_IRQ, "cci");
	if (!new_cci_dev->irq) {
		pr_err("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto cci_no_resource;
	}
	CDBG("%s line %d cci irq start %d end %d\n", __func__,
		__LINE__,
		(int) new_cci_dev->irq->start,
		(int) new_cci_dev->irq->end);
	new_cci_dev->io = request_mem_region(new_cci_dev->mem->start,
		resource_size(new_cci_dev->mem), pdev->name);
	if (!new_cci_dev->io) {
		pr_err("%s: no valid mem region\n", __func__);
		rc = -EBUSY;
		goto cci_no_resource;
	}

	new_cci_dev->base = ioremap(new_cci_dev->mem->start,
		resource_size(new_cci_dev->mem));
	if (!new_cci_dev->base) {
		rc = -ENOMEM;
		goto cci_release_mem;
	}
	rc = request_irq(new_cci_dev->irq->start, msm_cci_irq,
		IRQF_TRIGGER_RISING, "cci", new_cci_dev);
	if (rc < 0) {
		pr_err("%s: irq request fail\n", __func__);
		rc = -EBUSY;
		goto cci_release_mem;
	}

	disable_irq(new_cci_dev->irq->start);
	new_cci_dev->msm_sd.close_seq = MSM_SD_CLOSE_2ND_CATEGORY | 0x6;
	msm_sd_register(&new_cci_dev->msm_sd);
	new_cci_dev->pdev = pdev;
	msm_cci_init_cci_params(new_cci_dev);
	msm_cci_init_clk_params(new_cci_dev);
	msm_cci_init_gpio_params(new_cci_dev);

	rc = msm_camera_get_dt_vreg_data(new_cci_dev->pdev->dev.of_node,
		&(new_cci_dev->cci_vreg), &(new_cci_dev->regulator_count));
	if (rc < 0) {
		pr_err("%s: msm_camera_get_dt_vreg_data fail\n", __func__);
		rc = -EFAULT;
		goto cci_release_mem;
	}

	if ((new_cci_dev->regulator_count < 0) ||
		(new_cci_dev->regulator_count > MAX_REGULATOR)) {
		pr_err("%s: invalid reg count = %d, max is %d\n", __func__,
			new_cci_dev->regulator_count, MAX_REGULATOR);
		rc = -EFAULT;
		goto cci_invalid_vreg_data;
	}

	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc)
		pr_err("%s: failed to add child nodes, rc=%d\n", __func__, rc);
	new_cci_dev->cci_state = CCI_STATE_DISABLED;
	g_cci_subdev = &new_cci_dev->msm_sd.sd;
	for (i = 0; i < MASTER_MAX; i++) {
		new_cci_dev->write_wq[i] = create_singlethread_workqueue(
								"msm_cci_wq");
		if (!new_cci_dev->write_wq[i])
			pr_err("Failed to create write wq\n");
	}
	CDBG("%s cci subdev %pK\n", __func__, &new_cci_dev->msm_sd.sd);
	CDBG("%s line %d\n", __func__, __LINE__);
	return 0;

cci_invalid_vreg_data:
	kfree(new_cci_dev->cci_vreg);
cci_release_mem:
	release_mem_region(new_cci_dev->mem->start,
		resource_size(new_cci_dev->mem));
cci_no_resource:
	kfree(new_cci_dev);
	return rc;
}

static int msm_cci_exit(struct platform_device *pdev)
{
	struct v4l2_subdev *subdev = platform_get_drvdata(pdev);
	struct cci_device *cci_dev =
		v4l2_get_subdevdata(subdev);
	release_mem_region(cci_dev->mem->start, resource_size(cci_dev->mem));
	kfree(cci_dev);
	return 0;
}

static const struct of_device_id msm_cci_dt_match[] = {
	{.compatible = "qcom,cci"},
	{}
};

MODULE_DEVICE_TABLE(of, msm_cci_dt_match);

static struct platform_driver cci_driver = {
	.probe = msm_cci_probe,
	.remove = msm_cci_exit,
	.driver = {
		.name = MSM_CCI_DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = msm_cci_dt_match,
	},
};

static int __init msm_cci_init_module(void)
{
	return platform_driver_register(&cci_driver);
}

static void __exit msm_cci_exit_module(void)
{
	platform_driver_unregister(&cci_driver);
}

module_init(msm_cci_init_module);
module_exit(msm_cci_exit_module);
MODULE_DESCRIPTION("MSM CCI driver");
MODULE_LICENSE("GPL v2");
