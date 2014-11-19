/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <media/msm_isp.h>
#include "msm_sd.h"
#include "msm_cci.h"
#include "msm_cam_cci_hwreg.h"
#include "msm_camera_io_util.h"

#define V4L2_IDENT_CCI 50005
#define CCI_I2C_QUEUE_0_SIZE 64
#define CCI_I2C_QUEUE_1_SIZE 16
#define CYCLES_PER_MICRO_SEC 4915
#define CCI_MAX_DELAY 10000

#define CCI_TIMEOUT msecs_to_jiffies(100)

/* TODO move this somewhere else */
#define MSM_CCI_DRV_NAME "msm_cci"

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

/* Max bytes that can be read per CCI read transaction */
#define CCI_READ_MAX 12
#define CCI_I2C_READ_MAX_RETRIES 3
#define CCI_I2C_MAX_READ 8192
#define CCI_I2C_MAX_WRITE 8192

static struct v4l2_subdev *g_cci_subdev;

static void msm_cci_set_clk_param(struct cci_device *cci_dev)
{
	struct msm_cci_clk_params_t *clk_params = NULL;
	uint8_t count = 0;

	for (count = 0; count < MASTER_MAX; count++) {
		if (MASTER_0 == count) {
			clk_params = &cci_dev->cci_clk_params[count];
			msm_camera_io_w(clk_params->hw_thigh << 16 |
				clk_params->hw_tlow,
				cci_dev->base + CCI_I2C_M0_SCL_CTL_ADDR);
			msm_camera_io_w(clk_params->hw_tsu_sto << 16 |
				clk_params->hw_tsu_sta,
				cci_dev->base + CCI_I2C_M0_SDA_CTL_0_ADDR);
			msm_camera_io_w(clk_params->hw_thd_dat << 16 |
				clk_params->hw_thd_sta,
				cci_dev->base + CCI_I2C_M0_SDA_CTL_1_ADDR);
			msm_camera_io_w(clk_params->hw_tbuf,
				cci_dev->base + CCI_I2C_M0_SDA_CTL_2_ADDR);
			msm_camera_io_w(clk_params->hw_scl_stretch_en << 8 |
				clk_params->hw_trdhld << 4 | clk_params->hw_tsp,
				cci_dev->base + CCI_I2C_M0_MISC_CTL_ADDR);
		} else if (MASTER_1 == count) {
			clk_params = &cci_dev->cci_clk_params[count];
			msm_camera_io_w(clk_params->hw_thigh << 16 |
				clk_params->hw_tlow,
				cci_dev->base + CCI_I2C_M1_SCL_CTL_ADDR);
			msm_camera_io_w(clk_params->hw_tsu_sto << 16 |
				clk_params->hw_tsu_sta,
				cci_dev->base + CCI_I2C_M1_SDA_CTL_0_ADDR);
			msm_camera_io_w(clk_params->hw_thd_dat << 16 |
				clk_params->hw_thd_sta,
				cci_dev->base + CCI_I2C_M1_SDA_CTL_1_ADDR);
			msm_camera_io_w(clk_params->hw_tbuf,
				cci_dev->base + CCI_I2C_M1_SDA_CTL_2_ADDR);
			msm_camera_io_w(clk_params->hw_scl_stretch_en << 8 |
				clk_params->hw_trdhld << 4 | clk_params->hw_tsp,
				cci_dev->base + CCI_I2C_M1_MISC_CTL_ADDR);
		}
	}
	return;
}

static void msm_cci_flush_queue(struct cci_device *cci_dev,
	enum cci_i2c_master_t master)
{
	int32_t rc = 0;

	msm_camera_io_w(1 << master, cci_dev->base + CCI_HALT_REQ_ADDR);
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
			msm_camera_io_w(CCI_M0_RESET_RMSK,
				cci_dev->base + CCI_RESET_CMD_ADDR);
		else
			msm_camera_io_w(CCI_M1_RESET_RMSK,
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
	read_val = msm_camera_io_r(cci_dev->base +
		CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR + reg_offset);
	CDBG("%s line %d CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR %d len %d max %d\n",
		__func__, __LINE__, read_val, len,
		cci_dev->cci_i2c_queue_info[master][queue].max_queue_size);
	if ((read_val + len + 1) > cci_dev->
		cci_i2c_queue_info[master][queue].max_queue_size) {
		uint32_t reg_val = 0;
		uint32_t report_val = CCI_I2C_REPORT_CMD | (1 << 8);
		CDBG("%s:%d CCI_I2C_REPORT_CMD\n", __func__, __LINE__);
		msm_camera_io_w(report_val,
			cci_dev->base + CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
			reg_offset);
		read_val++;
		CDBG("%s:%d CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR %d\n",
			__func__, __LINE__, read_val);
		msm_camera_io_w(read_val, cci_dev->base +
			CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR + reg_offset);
		reg_val = 1 << ((master * 2) + queue);
		CDBG("%s:%d CCI_QUEUE_START_ADDR\n", __func__, __LINE__);
		msm_camera_io_w(reg_val, cci_dev->base + CCI_QUEUE_START_ADDR);
		CDBG("%s line %d wait_for_completion_interruptible\n",
			__func__, __LINE__);
		rc = wait_for_completion_timeout(&cci_dev->
			cci_master_info[master].reset_complete, CCI_TIMEOUT);
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

static int32_t msm_cci_data_queue(struct cci_device *cci_dev,
	struct msm_camera_cci_ctrl *c_ctrl, enum cci_i2c_queue_t queue)
{
	uint16_t i = 0, j = 0, k = 0, h = 0, len = 0;
	int32_t rc = 0;
	uint32_t cmd = 0, delay = 0;
	uint8_t data[11];
	uint16_t reg_addr = 0;
	struct msm_camera_i2c_reg_setting *i2c_msg =
		&c_ctrl->cfg.cci_i2c_write_cfg;
	uint16_t cmd_size = i2c_msg->size;
	struct msm_camera_i2c_reg_array *i2c_cmd = i2c_msg->reg_setting;
	enum cci_i2c_master_t master = c_ctrl->cci_info->cci_i2c_master;

	if (i2c_cmd == NULL) {
		pr_err("%s:%d Failed line\n", __func__,
			__LINE__);
		return -EINVAL;
	}

	if ((!cmd_size) || (cmd_size > CCI_I2C_MAX_WRITE)) {
		pr_err("%s:%d Failed line\n", __func__, __LINE__);
		return -EINVAL;
	}

	CDBG("%s addr type %d data type %d\n", __func__,
		i2c_msg->addr_type, i2c_msg->data_type);

	if (i2c_msg->addr_type >= MSM_CAMERA_I2C_ADDR_TYPE_MAX) {
		pr_err("%s failed line %d\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (i2c_msg->data_type >= MSM_CAMERA_I2C_DATA_TYPE_MAX) {
		pr_err("%s failed line %d\n", __func__, __LINE__);
		return -EINVAL;
	}

	reg_addr = i2c_cmd->reg_addr;
	while (cmd_size) {
		CDBG("%s cmd_size %d addr 0x%x data 0x%x\n", __func__,
			cmd_size, i2c_cmd->reg_addr, i2c_cmd->reg_data);
		delay = i2c_cmd->delay;
		data[i++] = CCI_I2C_WRITE_CMD;

		/* in case of multiple command
		* MSM_CCI_I2C_WRITE : address is not continuous, so update
		*			address for a new packet.
		* MSM_CCI_I2C_WRITE_SEQ : address is continuous, need to keep
		*			the incremented address for a
		*			new packet */
		if (c_ctrl->cmd == MSM_CCI_I2C_WRITE)
			reg_addr = i2c_cmd->reg_addr;

		/* either byte or word addr */
		if (i2c_msg->addr_type == MSM_CAMERA_I2C_BYTE_ADDR)
			data[i++] = reg_addr;
		else {
			data[i++] = (reg_addr & 0xFF00) >> 8;
			data[i++] = reg_addr & 0x00FF;
		}
		/* max of 10 data bytes */
		do {
			if (i2c_msg->data_type == MSM_CAMERA_I2C_BYTE_DATA) {
				data[i++] = i2c_cmd->reg_data;
				reg_addr++;
			} else {
				if ((i + 1) <= 10) {
					data[i++] = (i2c_cmd->reg_data &
						0xFF00) >> 8; /* MSB */
					data[i++] = i2c_cmd->reg_data &
						0x00FF; /* LSB */
					reg_addr += 2;
				} else
					break;
			}
			i2c_cmd++;
			--cmd_size;
		} while ((c_ctrl->cmd == MSM_CCI_I2C_WRITE_SEQ) &&
				(cmd_size > 0) && (i <= 10));

		data[0] |= ((i-1) << 4);
		len = ((i-1)/4) + 1;
		rc = msm_cci_validate_queue(cci_dev, len, master, queue);
		if (rc < 0) {
			pr_err("%s: failed %d", __func__, __LINE__);
			return rc;
		}
		for (h = 0, k = 0; h < len; h++) {
			cmd = 0;
			for (j = 0; (j < 4 && k < i); j++)
				cmd |= (data[k++] << (j * 8));
			CDBG("%s CCI_I2C_M0_Q0_LOAD_DATA_ADDR 0x%x\n",
				__func__, cmd);
			msm_camera_io_w(cmd, cci_dev->base +
				CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
				master * 0x200 + queue * 0x100);
		}
		if ((delay > 0) && (delay < CCI_MAX_DELAY)) {
			cmd = (uint32_t)((delay * CYCLES_PER_MICRO_SEC) /
				0x100);
			cmd <<= 4;
			cmd |= CCI_I2C_WAIT_CMD;
			CDBG("%s CCI_I2C_M0_Q0_LOAD_DATA_ADDR 0x%x\n",
				__func__, cmd);
			msm_camera_io_w(cmd, cci_dev->base +
				CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
				master * 0x200 + queue * 0x100);
		}
		i = 0;
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
	CDBG("%s:%d called\n", __func__, __LINE__);
	rc = msm_cci_validate_queue(cci_dev, 1, master, queue);
	if (rc < 0) {
		pr_err("%s: failed %d", __func__, __LINE__);
		return rc;
	}
	CDBG("%s CCI_I2C_M0_Q0_LOAD_DATA_ADDR:val %x:%x\n",
		__func__, CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
		reg_offset, val);
	msm_camera_io_w(val, cci_dev->base + CCI_I2C_M0_Q0_LOAD_DATA_ADDR +
		reg_offset);
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
	mutex_lock(&cci_dev->cci_master_info[master].mutex);

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

	if (read_cfg->addr_type == MSM_CAMERA_I2C_BYTE_ADDR)
		val = CCI_I2C_WRITE_DISABLE_P_CMD | (read_cfg->addr_type << 4) |
			((read_cfg->addr & 0xFF) << 8);
	if (read_cfg->addr_type == MSM_CAMERA_I2C_WORD_ADDR)
		val = CCI_I2C_WRITE_DISABLE_P_CMD | (read_cfg->addr_type << 4) |
			(((read_cfg->addr & 0xFF00) >> 8) << 8) |
			((read_cfg->addr & 0xFF) << 16);
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

	val = msm_camera_io_r(cci_dev->base + CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR +
		master * 0x200 + queue * 0x100);
	CDBG("%s cur word cnt %x\n", __func__, val);
	msm_camera_io_w(val, cci_dev->base + CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR +
		master * 0x200 + queue * 0x100);

	val = 1 << ((master * 2) + queue);
	msm_camera_io_w(val, cci_dev->base + CCI_QUEUE_START_ADDR);
	CDBG("%s:%d E wait_for_completion_timeout\n", __func__,
		__LINE__);
	rc = wait_for_completion_timeout(&cci_dev->
		cci_master_info[master].reset_complete, CCI_TIMEOUT);
	if (rc <= 0) {
		pr_err("%s: wait_for_completion_timeout %d\n",
			 __func__, __LINE__);
		if (rc == 0)
			rc = -ETIMEDOUT;
		msm_cci_flush_queue(cci_dev, master);
		goto ERROR;
	} else {
		rc = 0;
	}
	CDBG("%s:%d E wait_for_completion_timeout\n", __func__,
		__LINE__);

	read_words = msm_camera_io_r(cci_dev->base +
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
		val = msm_camera_io_r(cci_dev->base +
			CCI_I2C_M0_READ_DATA_ADDR + master * 0x100);
		CDBG("%s read val %x\n", __func__, val);
		for (i = 0; (i < 4) && (index < read_cfg->num_byte); i++) {
			CDBG("%s i %d index %d\n", __func__, i, index);
			if (!first_byte) {
				CDBG("%s sid %x\n", __func__, val & 0xFF);
				first_byte++;
			} else {
				read_cfg->data[index] =
					(val  >> (i * 8)) & 0xFF;
				CDBG("%s data[%d] %x\n", __func__, index,
					read_cfg->data[index]);
				index++;
			}
		}
	} while (--read_words > 0);
ERROR:
	mutex_unlock(&cci_dev->cci_master_info[master].mutex);
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
		pr_err("%s:%d sd %p c_ctrl %p\n", __func__,
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

	if (c_ctrl->cci_info->cci_i2c_master > MASTER_MAX
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
	struct msm_camera_cci_ctrl *c_ctrl)
{
	int32_t rc = 0;
	struct cci_device *cci_dev;
	uint32_t val;
	enum cci_i2c_master_t master;
	enum cci_i2c_queue_t queue = QUEUE_0;
	cci_dev = v4l2_get_subdevdata(sd);
	if (c_ctrl->cci_info->cci_i2c_master > MASTER_MAX
			|| c_ctrl->cci_info->cci_i2c_master < 0) {
		pr_err("%s:%d Invalid I2C master addr\n", __func__, __LINE__);
		return -EINVAL;
	}
	master = c_ctrl->cci_info->cci_i2c_master;
	CDBG("%s master %d, queue %d\n", __func__, master, queue);
	CDBG("%s set param sid 0x%x retries %d id_map %d\n", __func__,
		c_ctrl->cci_info->sid, c_ctrl->cci_info->retries,
		c_ctrl->cci_info->id_map);
	mutex_lock(&cci_dev->cci_master_info[master].mutex);

	/*
	 * Call validate queue to make sure queue is empty before starting.
	 * If this call fails, don't proceed with i2c_write call. This is to
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

	val = CCI_I2C_SET_PARAM_CMD | c_ctrl->cci_info->sid << 4 |
		c_ctrl->cci_info->retries << 16 |
		c_ctrl->cci_info->id_map << 18;
	CDBG("%s:%d CCI_I2C_SET_PARAM_CMD\n", __func__, __LINE__);
	rc = msm_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CDBG("%s failed line %d\n", __func__, __LINE__);
		goto ERROR;
	}

	val = CCI_I2C_LOCK_CMD;
	CDBG("%s:%d CCI_I2C_LOCK_CMD\n", __func__, __LINE__);
	rc = msm_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CDBG("%s failed line %d\n", __func__, __LINE__);
		goto ERROR;
	}

	rc = msm_cci_data_queue(cci_dev, c_ctrl, queue);
	if (rc < 0) {
		CDBG("%s failed line %d\n", __func__, __LINE__);
		goto ERROR;
	}
	val = CCI_I2C_UNLOCK_CMD;
	CDBG("%s:%d CCI_I2C_UNLOCK_CMD\n", __func__, __LINE__);
	rc = msm_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CDBG("%s failed line %d\n", __func__, __LINE__);
		goto ERROR;
	}

	val = CCI_I2C_REPORT_CMD | (1 << 8);
	CDBG("%s:%d CCI_I2C_REPORT_CMD\n", __func__, __LINE__);
	rc = msm_cci_write_i2c_queue(cci_dev, val, master, queue);
	if (rc < 0) {
		CDBG("%s failed line %d\n", __func__, __LINE__);
		goto ERROR;
	}

	val = msm_camera_io_r(cci_dev->base + CCI_I2C_M0_Q0_CUR_WORD_CNT_ADDR +
		master * 0x200 + queue * 0x100);
	CDBG("%s:%d cur word count %d\n", __func__, __LINE__, val);
	CDBG("%s:%d CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR\n", __func__, __LINE__);
	msm_camera_io_w(val, cci_dev->base + CCI_I2C_M0_Q0_EXEC_WORD_CNT_ADDR +
		master * 0x200 + queue * 0x100);

	val = 1 << ((master * 2) + queue);
	CDBG("%s:%d CCI_QUEUE_START_ADDR\n", __func__, __LINE__);
	msm_camera_io_w(val, cci_dev->base + CCI_QUEUE_START_ADDR);

	CDBG("%s:%d E wait_for_completion_interruptible\n",
		__func__, __LINE__);
	rc = wait_for_completion_timeout(&cci_dev->
		cci_master_info[master].reset_complete, CCI_TIMEOUT);
	if (rc <= 0) {
		pr_err("%s: wait_for_completion_timeout %d\n",
			 __func__, __LINE__);
		if (rc == 0)
			rc = -ETIMEDOUT;
		msm_cci_flush_queue(cci_dev, master);
		goto ERROR;
	} else {
		rc = cci_dev->cci_master_info[master].status;
	}
	CDBG("%s:%d X wait_for_completion_interruptible\n", __func__,
		__LINE__);

ERROR:
	mutex_unlock(&cci_dev->cci_master_info[master].mutex);
	return rc;
}

static int msm_cci_subdev_g_chip_ident(struct v4l2_subdev *sd,
			struct v4l2_dbg_chip_ident *chip)
{
	if (!chip) {
		pr_err("%s:%d: NULL pointer supplied for chip ident\n",
			 __func__, __LINE__);
		return -EINVAL;
	}
	chip->ident = V4L2_IDENT_CCI;
	chip->revision = 0;
	return 0;
}

static struct msm_cam_clk_info cci_clk_info[] = {
	{"camss_top_ahb_clk", -1},
	{"cci_src_clk", 19200000},
	{"cci_ahb_clk", -1},
	{"cci_clk", -1},
};

static int32_t msm_cci_init(struct v4l2_subdev *sd,
	struct msm_camera_cci_ctrl *c_ctrl)
{
	int32_t rc = 0;
	struct cci_device *cci_dev;
	enum cci_i2c_master_t master;
	cci_dev = v4l2_get_subdevdata(sd);

	if (!cci_dev || !c_ctrl) {
		pr_err("%s:%d failed: invalid params %p %p\n", __func__,
			__LINE__, cci_dev, c_ctrl);
		rc = -ENOMEM;
		return rc;
	}

	if (cci_dev->ref_count++) {
		CDBG("%s ref_count %d\n", __func__, cci_dev->ref_count);
		master = c_ctrl->cci_info->cci_i2c_master;
		CDBG("%s:%d master %d\n", __func__, __LINE__, master);
		if (master < MASTER_MAX && master >= 0) {
			mutex_lock(&cci_dev->cci_master_info[master].mutex);
			/* Set reset pending flag to TRUE */
			cci_dev->cci_master_info[master].reset_pending = TRUE;
			/* Set proper mask to RESET CMD address */
			if (master == MASTER_0)
				msm_camera_io_w(CCI_M0_RESET_RMSK,
					cci_dev->base + CCI_RESET_CMD_ADDR);
			else
				msm_camera_io_w(CCI_M1_RESET_RMSK,
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

	rc = msm_camera_request_gpio_table(cci_dev->cci_gpio_tbl,
		cci_dev->cci_gpio_tbl_size, 1);
	if (rc < 0) {
		cci_dev->ref_count--;
		CDBG("%s: request gpio failed\n", __func__);
		goto request_gpio_failed;
	}

	rc = msm_cam_clk_enable(&cci_dev->pdev->dev, cci_clk_info,
		cci_dev->cci_clk, ARRAY_SIZE(cci_clk_info), 1);
	if (rc < 0) {
		cci_dev->ref_count--;
		CDBG("%s: clk enable failed\n", __func__);
		goto clk_enable_failed;
	}

	enable_irq(cci_dev->irq->start);
	cci_dev->hw_version = msm_camera_io_r(cci_dev->base +
		CCI_HW_VERSION_ADDR);
	cci_dev->cci_master_info[MASTER_0].reset_pending = TRUE;
	msm_camera_io_w(CCI_RESET_CMD_RMSK, cci_dev->base + CCI_RESET_CMD_ADDR);
	msm_camera_io_w(0x1, cci_dev->base + CCI_RESET_CMD_ADDR);
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
	msm_cci_set_clk_param(cci_dev);
	msm_camera_io_w(CCI_IRQ_MASK_0_RMSK,
		cci_dev->base + CCI_IRQ_MASK_0_ADDR);
	msm_camera_io_w(CCI_IRQ_MASK_0_RMSK,
		cci_dev->base + CCI_IRQ_CLEAR_0_ADDR);
	msm_camera_io_w(0x1, cci_dev->base + CCI_IRQ_GLOBAL_CLEAR_CMD_ADDR);
	cci_dev->cci_state = CCI_STATE_ENABLED;

	return 0;

reset_complete_failed:
	disable_irq(cci_dev->irq->start);
	msm_cam_clk_enable(&cci_dev->pdev->dev, cci_clk_info,
		cci_dev->cci_clk, ARRAY_SIZE(cci_clk_info), 0);
clk_enable_failed:
	msm_camera_request_gpio_table(cci_dev->cci_gpio_tbl,
		cci_dev->cci_gpio_tbl_size, 0);
request_gpio_failed:
	cci_dev->ref_count--;
	return rc;
}

static int32_t msm_cci_release(struct v4l2_subdev *sd)
{
	struct cci_device *cci_dev;
	cci_dev = v4l2_get_subdevdata(sd);

	if (!cci_dev->ref_count || cci_dev->cci_state != CCI_STATE_ENABLED) {
		pr_err("%s invalid ref count %d / cci state %d\n",
			__func__, cci_dev->ref_count, cci_dev->cci_state);
		return -EINVAL;
	}

	if (--cci_dev->ref_count) {
		CDBG("%s ref_count Exit %d\n", __func__, cci_dev->ref_count);
		return 0;
	}

	disable_irq(cci_dev->irq->start);

	msm_cam_clk_enable(&cci_dev->pdev->dev, cci_clk_info,
		cci_dev->cci_clk, ARRAY_SIZE(cci_clk_info), 0);

	msm_camera_request_gpio_table(cci_dev->cci_gpio_tbl,
		cci_dev->cci_gpio_tbl_size, 0);

	cci_dev->cci_state = CCI_STATE_DISABLED;

	return 0;
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
		rc = msm_cci_i2c_write(sd, cci_ctrl);
		break;
	case MSM_CCI_GPIO_WRITE:
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
	irq = msm_camera_io_r(cci_dev->base + CCI_IRQ_STATUS_0_ADDR);
	msm_camera_io_w(irq, cci_dev->base + CCI_IRQ_CLEAR_0_ADDR);
	msm_camera_io_w(0x1, cci_dev->base + CCI_IRQ_GLOBAL_CLEAR_CMD_ADDR);
	msm_camera_io_w(0x0, cci_dev->base + CCI_IRQ_GLOBAL_CLEAR_CMD_ADDR);
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
	if ((irq & CCI_IRQ_STATUS_0_I2C_M0_RD_DONE_BMSK) ||
		(irq & CCI_IRQ_STATUS_0_I2C_M0_Q0_REPORT_BMSK) ||
		(irq & CCI_IRQ_STATUS_0_I2C_M0_Q1_REPORT_BMSK)) {
		cci_dev->cci_master_info[MASTER_0].status = 0;
		complete(&cci_dev->cci_master_info[MASTER_0].reset_complete);
	}
	if ((irq & CCI_IRQ_STATUS_0_I2C_M1_RD_DONE_BMSK) ||
		(irq & CCI_IRQ_STATUS_0_I2C_M1_Q0_REPORT_BMSK) ||
		(irq & CCI_IRQ_STATUS_0_I2C_M1_Q1_REPORT_BMSK)) {
		cci_dev->cci_master_info[MASTER_1].status = 0;
		complete(&cci_dev->cci_master_info[MASTER_1].reset_complete);
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M0_Q0Q1_HALT_ACK_BMSK) {
		cci_dev->cci_master_info[MASTER_0].reset_pending = TRUE;
		msm_camera_io_w(CCI_M0_RESET_RMSK,
			cci_dev->base + CCI_RESET_CMD_ADDR);
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M1_Q0Q1_HALT_ACK_BMSK) {
		cci_dev->cci_master_info[MASTER_1].reset_pending = TRUE;
		msm_camera_io_w(CCI_M1_RESET_RMSK,
			cci_dev->base + CCI_RESET_CMD_ADDR);
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M0_ERROR_BMSK) {
		pr_err("%s:%d MASTER_0 error %x\n", __func__, __LINE__, irq);
		cci_dev->cci_master_info[MASTER_0].status = -EINVAL;
		msm_camera_io_w(CCI_M0_HALT_REQ_RMSK,
			cci_dev->base + CCI_HALT_REQ_ADDR);
	}
	if (irq & CCI_IRQ_STATUS_0_I2C_M1_ERROR_BMSK) {
		pr_err("%s:%d MASTER_1 error %x\n", __func__, __LINE__, irq);
		cci_dev->cci_master_info[MASTER_1].status = -EINVAL;
		msm_camera_io_w(CCI_M1_HALT_REQ_RMSK,
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
	case MSM_SD_SHUTDOWN: {
		return rc;
	}
	default:
		rc = -ENOIOCTLCMD;
	}
	CDBG("%s line %d rc %d\n", __func__, __LINE__, rc);
	return rc;
}

static struct v4l2_subdev_core_ops msm_cci_subdev_core_ops = {
	.g_chip_ident = &msm_cci_subdev_g_chip_ident,
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

static void msm_cci_init_clk_params(struct cci_device *cci_dev)
{
	int32_t rc = 0;
	uint32_t val = 0;
	uint8_t count = 0;
	struct device_node *of_node = cci_dev->pdev->dev.of_node;
	struct device_node *src_node = NULL;

	for (count = 0; count < MASTER_MAX; count++) {

		if (MASTER_0 == count)
			src_node = of_find_node_by_name(of_node,
				"qcom,cci-master0");
		else if (MASTER_1 == count)
			src_node = of_find_node_by_name(of_node,
				"qcom,cci-master1");
		else
			return;

		rc = of_property_read_u32(src_node, "qcom,hw-thigh", &val);
		CDBG("%s qcom,hw-thigh %d, rc %d\n", __func__, val, rc);
		if (!rc)
			cci_dev->cci_clk_params[count].hw_thigh = val;
		else
			cci_dev->cci_clk_params[count].hw_thigh = 78;

		rc = of_property_read_u32(src_node, "qcom,hw-tlow", &val);
		CDBG("%s qcom,hw-tlow %d, rc %d\n", __func__, val, rc);
		if (!rc)
			cci_dev->cci_clk_params[count].hw_tlow = val;
		else
			cci_dev->cci_clk_params[count].hw_tlow = 114;

		rc = of_property_read_u32(src_node, "qcom,hw-tsu-sto", &val);
		CDBG("%s qcom,hw-tsu-sto %d, rc %d\n", __func__, val, rc);
		if (!rc)
			cci_dev->cci_clk_params[count].hw_tsu_sto = val;
		else
			cci_dev->cci_clk_params[count].hw_tsu_sto = 28;

		rc = of_property_read_u32(src_node, "qcom,hw-tsu-sta", &val);
		CDBG("%s qcom,hw-tsu-sta %d, rc %d\n", __func__, val, rc);
		if (!rc)
			cci_dev->cci_clk_params[count].hw_tsu_sta = val;
		else
			cci_dev->cci_clk_params[count].hw_tsu_sta = 28;

		rc = of_property_read_u32(src_node, "qcom,hw-thd-dat", &val);
		CDBG("%s qcom,hw-thd-dat %d, rc %d\n", __func__, val, rc);
		if (!rc)
			cci_dev->cci_clk_params[count].hw_thd_dat = val;
		else
			cci_dev->cci_clk_params[count].hw_thd_dat = 10;

		rc = of_property_read_u32(src_node, "qcom,hw-thd-sta", &val);
		CDBG("%s qcom,hwthd-sta %d, rc %d\n", __func__, val, rc);
		if (!rc)
			cci_dev->cci_clk_params[count].hw_thd_sta = val;
		else
			cci_dev->cci_clk_params[count].hw_thd_sta = 77;

		rc = of_property_read_u32(src_node, "qcom,hw-tbuf", &val);
		CDBG("%s qcom,hw-tbuf %d, rc %d\n", __func__, val, rc);
		if (!rc)
			cci_dev->cci_clk_params[count].hw_tbuf = val;
		else
			cci_dev->cci_clk_params[count].hw_tbuf = 118;

		rc = of_property_read_u32(src_node,
			"qcom,hw-scl-stretch-en", &val);
		CDBG("%s qcom,hw-scl-stretch-en %d, rc %d\n",
			__func__, val, rc);
		if (!rc)
			cci_dev->cci_clk_params[count].hw_scl_stretch_en = val;
		else
			cci_dev->cci_clk_params[count].hw_scl_stretch_en = 0;

		rc = of_property_read_u32(src_node, "qcom,hw-trdhld", &val);
		CDBG("%s qcom,hw-trdhld %d, rc %d\n", __func__, val, rc);
		if (!rc)
			cci_dev->cci_clk_params[count].hw_trdhld = val;
		else
			cci_dev->cci_clk_params[count].hw_trdhld = 6;

		rc = of_property_read_u32(src_node, "qcom,hw-tsp", &val);
		CDBG("%s qcom,hw-tsp %d, rc %d\n", __func__, val, rc);
		if (!rc)
			cci_dev->cci_clk_params[count].hw_tsp = val;
		else
			cci_dev->cci_clk_params[count].hw_tsp = 1;

		of_node_put(src_node);
		src_node = NULL;
	}
	return;
}

struct v4l2_subdev *msm_cci_get_subdev(void)
{
	return g_cci_subdev;
}

static int __devinit msm_cci_probe(struct platform_device *pdev)
{
	struct cci_device *new_cci_dev;
	int rc = 0;
	CDBG("%s: pdev %p device id = %d\n", __func__, pdev, pdev->id);
	new_cci_dev = kzalloc(sizeof(struct cci_device), GFP_KERNEL);
	if (!new_cci_dev) {
		CDBG("%s: no enough memory\n", __func__);
		return -ENOMEM;
	}
	v4l2_subdev_init(&new_cci_dev->msm_sd.sd, &msm_cci_subdev_ops);
	new_cci_dev->msm_sd.sd.internal_ops = &msm_cci_internal_ops;
	snprintf(new_cci_dev->msm_sd.sd.name,
			ARRAY_SIZE(new_cci_dev->msm_sd.sd.name), "msm_cci");
	v4l2_set_subdevdata(&new_cci_dev->msm_sd.sd, new_cci_dev);
	platform_set_drvdata(pdev, &new_cci_dev->msm_sd.sd);
	CDBG("%s sd %p\n", __func__, &new_cci_dev->msm_sd.sd);
	if (pdev->dev.of_node)
		of_property_read_u32((&pdev->dev)->of_node,
			"cell-index", &pdev->id);

	new_cci_dev->mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "cci");
	if (!new_cci_dev->mem) {
		CDBG("%s: no mem resource?\n", __func__);
		rc = -ENODEV;
		goto cci_no_resource;
	}
	new_cci_dev->irq = platform_get_resource_byname(pdev,
					IORESOURCE_IRQ, "cci");
	CDBG("%s line %d cci irq start %d end %d\n", __func__,
		__LINE__,
		new_cci_dev->irq->start,
		new_cci_dev->irq->end);
	if (!new_cci_dev->irq) {
		CDBG("%s: no irq resource?\n", __func__);
		rc = -ENODEV;
		goto cci_no_resource;
	}
	new_cci_dev->io = request_mem_region(new_cci_dev->mem->start,
		resource_size(new_cci_dev->mem), pdev->name);
	if (!new_cci_dev->io) {
		CDBG("%s: no valid mem region\n", __func__);
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
		CDBG("%s: irq request fail\n", __func__);
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
	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc)
		pr_err("%s: failed to add child nodes, rc=%d\n", __func__, rc);
	new_cci_dev->cci_state = CCI_STATE_DISABLED;
	g_cci_subdev = &new_cci_dev->msm_sd.sd;
	CDBG("%s cci subdev %p\n", __func__, &new_cci_dev->msm_sd.sd);
	CDBG("%s line %d\n", __func__, __LINE__);
	return 0;

cci_release_mem:
	release_mem_region(new_cci_dev->mem->start,
		resource_size(new_cci_dev->mem));
cci_no_resource:
	kfree(new_cci_dev);
	return 0;
}

static int __exit msm_cci_exit(struct platform_device *pdev)
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
