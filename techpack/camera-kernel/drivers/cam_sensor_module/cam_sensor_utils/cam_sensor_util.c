// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#if IS_ENABLED(CONFIG_ISPV3)
#include <linux/ispv3_ioparam.h>
#endif
#include <clocksource/arm_arch_timer.h>
#include "cam_sensor_util.h"
#include "cam_mem_mgr.h"
#include "cam_res_mgr_api.h"

#define CAM_SENSOR_PINCTRL_STATE_SLEEP "cam_suspend"
#define CAM_SENSOR_PINCTRL_STATE_DEFAULT "cam_default"

#ifdef CONFIG_WL2866D
extern int wl2866d_camera_power_up(int out_iotype, int out_out_delay);
extern int wl2866d_camera_power_down(int out_iotype, int out_out_delay);
#endif



#define VALIDATE_VOLTAGE(min, max, config_val) ((config_val) && \
	(config_val >= min) && (config_val <= max))

static struct i2c_settings_list*
	cam_sensor_get_i2c_ptr(struct i2c_settings_array *i2c_reg_settings,
		uint32_t size)
{
	struct i2c_settings_list *tmp;

	tmp = kzalloc(sizeof(struct i2c_settings_list), GFP_KERNEL);

	if (tmp != NULL)
		list_add_tail(&(tmp->list),
			&(i2c_reg_settings->list_head));
	else
		return NULL;

	tmp->i2c_settings.reg_setting = (struct cam_sensor_i2c_reg_array *)
		vzalloc(size * sizeof(struct cam_sensor_i2c_reg_array));
	if (tmp->i2c_settings.reg_setting == NULL) {
		list_del(&(tmp->list));
		kfree(tmp);
		return NULL;
	}
	tmp->i2c_settings.size = size;

	return tmp;
}

int32_t cam_sensor_util_get_current_qtimer_ns(uint64_t *qtime_ns)
{
	uint64_t ticks = 0;
	int32_t rc = 0;

	ticks = arch_timer_read_counter();
	if (ticks == 0) {
		CAM_ERR(CAM_SENSOR, "qtimer returned 0, rc:%d", rc);
		return -EINVAL;
	}

	if (qtime_ns != NULL) {
		*qtime_ns = mul_u64_u32_div(ticks,
			QTIMER_MUL_FACTOR, QTIMER_DIV_FACTOR);
		CAM_DBG(CAM_SENSOR, "Qtimer time: 0x%x", *qtime_ns);
	} else {
		CAM_ERR(CAM_SENSOR, "NULL pointer passed");
		return -EINVAL;
	}

	return rc;
}

int32_t delete_request(struct i2c_settings_array *i2c_array)
{
	struct i2c_settings_list *i2c_list = NULL, *i2c_next = NULL;
	int32_t rc = 0;

	if (i2c_array == NULL) {
		CAM_ERR(CAM_SENSOR, "FATAL:: Invalid argument");
		return -EINVAL;
	}

	list_for_each_entry_safe(i2c_list, i2c_next,
		&(i2c_array->list_head), list) {
		vfree(i2c_list->i2c_settings.reg_setting);
		list_del(&(i2c_list->list));
		kfree(i2c_list);
	}
	INIT_LIST_HEAD(&(i2c_array->list_head));
	i2c_array->is_settings_valid = 0;

	return rc;
}

int32_t cam_sensor_handle_delay(
	uint32_t **cmd_buf,
	uint16_t generic_op_code,
	struct i2c_settings_array *i2c_reg_settings,
	uint32_t offset, uint32_t *byte_cnt,
	struct list_head *list_ptr)
{
	int32_t rc = 0;
	struct cam_cmd_unconditional_wait *cmd_uncond_wait =
		(struct cam_cmd_unconditional_wait *) *cmd_buf;
	struct i2c_settings_list *i2c_list = NULL;

	if (list_ptr == NULL) {
		CAM_ERR(CAM_SENSOR, "Invalid list ptr");
		return -EINVAL;
	}

	if (offset > 0) {
		i2c_list =
			list_entry(list_ptr, struct i2c_settings_list, list);
		if (generic_op_code ==
			CAMERA_SENSOR_WAIT_OP_HW_UCND)
			i2c_list->i2c_settings.reg_setting[offset - 1].delay =
				cmd_uncond_wait->delay;
		else
			i2c_list->i2c_settings.delay = cmd_uncond_wait->delay;
		(*cmd_buf) +=
			sizeof(
			struct cam_cmd_unconditional_wait) / sizeof(uint32_t);
		(*byte_cnt) +=
			sizeof(
			struct cam_cmd_unconditional_wait);
	} else {
		CAM_ERR(CAM_SENSOR, "Delay Rxed Before any buffer: %d", offset);
		return -EINVAL;
	}

	return rc;
}

int32_t cam_sensor_handle_poll(
	uint32_t **cmd_buf,
	struct i2c_settings_array *i2c_reg_settings,
	uint32_t *byte_cnt, int32_t *offset,
	struct list_head **list_ptr)
{
	struct i2c_settings_list  *i2c_list;
	int32_t rc = 0;
	struct cam_cmd_conditional_wait *cond_wait
		= (struct cam_cmd_conditional_wait *) *cmd_buf;

	i2c_list =
		cam_sensor_get_i2c_ptr(i2c_reg_settings, 1);
	if (!i2c_list || !i2c_list->i2c_settings.reg_setting) {
		CAM_ERR(CAM_SENSOR, "Failed in allocating mem for list");
		return -ENOMEM;
	}

	i2c_list->op_code = CAM_SENSOR_I2C_POLL;
	i2c_list->i2c_settings.data_type =
		cond_wait->data_type;
	i2c_list->i2c_settings.addr_type =
		cond_wait->addr_type;
	i2c_list->i2c_settings.reg_setting->reg_addr =
		cond_wait->reg_addr;
	i2c_list->i2c_settings.reg_setting->reg_data =
		cond_wait->reg_data;
	i2c_list->i2c_settings.reg_setting->delay =
		cond_wait->timeout;

	(*cmd_buf) += sizeof(struct cam_cmd_conditional_wait) /
		sizeof(uint32_t);
	(*byte_cnt) += sizeof(struct cam_cmd_conditional_wait);

	*offset = 1;
	*list_ptr = &(i2c_list->list);

	return rc;
}

int32_t cam_sensor_handle_random_write(
	struct cam_cmd_i2c_random_wr *cam_cmd_i2c_random_wr,
	struct i2c_settings_array *i2c_reg_settings,
	uint32_t *cmd_length_in_bytes, int32_t *offset,
	struct list_head **list)
{
	struct i2c_settings_list  *i2c_list;
	int32_t rc = 0, cnt;

	i2c_list = cam_sensor_get_i2c_ptr(i2c_reg_settings,
		cam_cmd_i2c_random_wr->header.count);
	if (i2c_list == NULL ||
		i2c_list->i2c_settings.reg_setting == NULL) {
		CAM_ERR(CAM_SENSOR, "Failed in allocating i2c_list");
		return -ENOMEM;
	}

	*cmd_length_in_bytes = (sizeof(struct i2c_rdwr_header) +
		sizeof(struct i2c_random_wr_payload) *
		(cam_cmd_i2c_random_wr->header.count));
	i2c_list->op_code = CAM_SENSOR_I2C_WRITE_RANDOM;
	i2c_list->i2c_settings.addr_type =
		cam_cmd_i2c_random_wr->header.addr_type;
	i2c_list->i2c_settings.data_type =
		cam_cmd_i2c_random_wr->header.data_type;

	for (cnt = 0; cnt < (cam_cmd_i2c_random_wr->header.count);
		cnt++) {
		i2c_list->i2c_settings.reg_setting[cnt].reg_addr =
			cam_cmd_i2c_random_wr->random_wr_payload[cnt].reg_addr;
		i2c_list->i2c_settings.reg_setting[cnt].reg_data =
			cam_cmd_i2c_random_wr->random_wr_payload[cnt].reg_data;
		i2c_list->i2c_settings.reg_setting[cnt].data_mask = 0;
	}
	*offset = cnt;
	*list = &(i2c_list->list);

	return rc;
}

static int32_t cam_sensor_handle_continuous_write(
	struct cam_cmd_i2c_continuous_wr *cam_cmd_i2c_continuous_wr,
	struct i2c_settings_array *i2c_reg_settings,
	uint32_t *cmd_length_in_bytes, int32_t *offset,
	struct list_head **list)
{
	struct i2c_settings_list *i2c_list;
	int32_t rc = 0, cnt;

	i2c_list = cam_sensor_get_i2c_ptr(i2c_reg_settings,
		cam_cmd_i2c_continuous_wr->header.count);
	if (i2c_list == NULL ||
		i2c_list->i2c_settings.reg_setting == NULL) {
		CAM_ERR(CAM_SENSOR, "Failed in allocating i2c_list");
		return -ENOMEM;
	}

	*cmd_length_in_bytes = (sizeof(struct i2c_rdwr_header) +
		sizeof(cam_cmd_i2c_continuous_wr->reg_addr) +
		sizeof(struct cam_cmd_read) *
		(cam_cmd_i2c_continuous_wr->header.count));
	if (cam_cmd_i2c_continuous_wr->header.op_code ==
		CAMERA_SENSOR_I2C_OP_CONT_WR_BRST)
		i2c_list->op_code = CAM_SENSOR_I2C_WRITE_BURST;
	else if (cam_cmd_i2c_continuous_wr->header.op_code ==
		CAMERA_SENSOR_I2C_OP_CONT_WR_SEQN)
		i2c_list->op_code = CAM_SENSOR_I2C_WRITE_SEQ;
	else
		return -EINVAL;

	i2c_list->i2c_settings.addr_type =
		cam_cmd_i2c_continuous_wr->header.addr_type;
	i2c_list->i2c_settings.data_type =
		cam_cmd_i2c_continuous_wr->header.data_type;
	i2c_list->i2c_settings.size =
		cam_cmd_i2c_continuous_wr->header.count;

	for (cnt = 0; cnt < (cam_cmd_i2c_continuous_wr->header.count);
		cnt++) {
		i2c_list->i2c_settings.reg_setting[cnt].reg_addr =
			cam_cmd_i2c_continuous_wr->reg_addr;
		i2c_list->i2c_settings.reg_setting[cnt].reg_data =
			cam_cmd_i2c_continuous_wr->data_read[cnt].reg_data;
		i2c_list->i2c_settings.reg_setting[cnt].data_mask = 0;
	}
	*offset = cnt;
	*list = &(i2c_list->list);

	return rc;
}

static int32_t cam_sensor_get_io_buffer(
	struct cam_buf_io_cfg *io_cfg,
	struct cam_sensor_i2c_reg_setting *i2c_settings)
{
	uintptr_t buf_addr = 0x0;
	size_t buf_size = 0;
	int32_t rc = 0;

	if (io_cfg == NULL || i2c_settings == NULL) {
		CAM_ERR(CAM_SENSOR,
			"Invalid args, io buf or i2c settings is NULL");
		return -EINVAL;
	}

	if (io_cfg->direction == CAM_BUF_OUTPUT) {
		rc = cam_mem_get_cpu_buf(io_cfg->mem_handle[0],
			&buf_addr, &buf_size);
		if ((rc < 0) || (!buf_addr)) {
			CAM_ERR(CAM_SENSOR,
				"invalid buffer, rc: %d, buf_addr: %pK",
				rc, buf_addr);
			return -EINVAL;
		}
		CAM_DBG(CAM_SENSOR,
			"buf_addr: %pK, buf_size: %zu, offsetsize: %d",
			(void *)buf_addr, buf_size, io_cfg->offsets[0]);
		if (io_cfg->offsets[0] >= buf_size) {
			CAM_ERR(CAM_SENSOR,
				"invalid size:io_cfg->offsets[0]: %d, buf_size: %d",
				io_cfg->offsets[0], buf_size);
			return -EINVAL;
		}
		i2c_settings->read_buff =
			 (uint8_t *)buf_addr + io_cfg->offsets[0];
		i2c_settings->read_buff_len =
			buf_size - io_cfg->offsets[0];
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid direction: %d",
			io_cfg->direction);
		rc = -EINVAL;
	}
	return rc;
}

int32_t cam_sensor_util_write_qtimer_to_io_buffer(
	uint64_t qtime_ns, struct cam_buf_io_cfg *io_cfg)
{
	uintptr_t buf_addr = 0x0, target_buf = 0x0;
	size_t buf_size = 0, target_size = 0;
	int32_t rc = 0;

	if (io_cfg == NULL) {
		CAM_ERR(CAM_SENSOR,
			"Invalid args, io buf is NULL");
		return -EINVAL;
	}

	if (io_cfg->direction == CAM_BUF_OUTPUT) {
		rc = cam_mem_get_cpu_buf(io_cfg->mem_handle[0],
			&buf_addr, &buf_size);
		if ((rc < 0) || (!buf_addr)) {
			CAM_ERR(CAM_SENSOR,
				"invalid buffer, rc: %d, buf_addr: %pK",
				rc, buf_addr);
			return -EINVAL;
		}
		CAM_DBG(CAM_SENSOR,
			"buf_addr: %pK, buf_size: %zu, offsetsize: %d",
			(void *)buf_addr, buf_size, io_cfg->offsets[0]);
		if (io_cfg->offsets[0] >= buf_size) {
			CAM_ERR(CAM_SENSOR,
				"invalid size:io_cfg->offsets[0]: %d, buf_size: %d",
				io_cfg->offsets[0], buf_size);
			return -EINVAL;
		}

		target_buf  = buf_addr + io_cfg->offsets[0];
		target_size = buf_size - io_cfg->offsets[0];

		if (target_size < sizeof(uint64_t)) {
			CAM_ERR(CAM_SENSOR,
				"not enough size for qtimer, target_size:%d",
				target_size);
			return -EINVAL;
		}

		memcpy((void *)target_buf, &qtime_ns, sizeof(uint64_t));
	} else {
		CAM_ERR(CAM_SENSOR, "Invalid direction: %d",
			io_cfg->direction);
		rc = -EINVAL;
	}
	return rc;
}

static int32_t cam_sensor_handle_random_read(
	struct cam_cmd_i2c_random_rd *cmd_i2c_random_rd,
	struct i2c_settings_array *i2c_reg_settings,
	uint16_t *cmd_length_in_bytes,
	int32_t *offset,
	struct list_head **list,
	struct cam_buf_io_cfg *io_cfg)
{
	struct i2c_settings_list *i2c_list;
	int32_t rc = 0, cnt = 0;

	i2c_list = cam_sensor_get_i2c_ptr(i2c_reg_settings,
		cmd_i2c_random_rd->header.count);
	if ((i2c_list == NULL) ||
		(i2c_list->i2c_settings.reg_setting == NULL)) {
		CAM_ERR(CAM_SENSOR,
			"Failed in allocating i2c_list: %pK",
			i2c_list);
		return -ENOMEM;
	}

	rc = cam_sensor_get_io_buffer(io_cfg, &(i2c_list->i2c_settings));
	if (rc) {
		CAM_ERR(CAM_SENSOR, "Failed to get read buffer: %d", rc);
	} else {
		*cmd_length_in_bytes = sizeof(struct i2c_rdwr_header) +
			(sizeof(struct cam_cmd_read) *
			(cmd_i2c_random_rd->header.count));
		i2c_list->op_code = CAM_SENSOR_I2C_READ_RANDOM;
		i2c_list->i2c_settings.addr_type =
			cmd_i2c_random_rd->header.addr_type;
		i2c_list->i2c_settings.data_type =
			cmd_i2c_random_rd->header.data_type;
		i2c_list->i2c_settings.size =
			cmd_i2c_random_rd->header.count;

		for (cnt = 0; cnt < (cmd_i2c_random_rd->header.count);
			cnt++) {
			i2c_list->i2c_settings.reg_setting[cnt].reg_addr =
				cmd_i2c_random_rd->data_read[cnt].reg_data;
		}
		*offset = cnt;
		*list = &(i2c_list->list);
	}

	return rc;
}

static int32_t cam_sensor_handle_continuous_read(
	struct cam_cmd_i2c_continuous_rd *cmd_i2c_continuous_rd,
	struct i2c_settings_array *i2c_reg_settings,
	uint16_t *cmd_length_in_bytes, int32_t *offset,
	struct list_head **list,
	struct cam_buf_io_cfg *io_cfg)
{
	struct i2c_settings_list *i2c_list;
	int32_t rc = 0, cnt = 0;

	i2c_list = cam_sensor_get_i2c_ptr(i2c_reg_settings, 1);
	if ((i2c_list == NULL) ||
		(i2c_list->i2c_settings.reg_setting == NULL)) {
		CAM_ERR(CAM_SENSOR,
			"Failed in allocating i2c_list: %pK",
			i2c_list);
		return -ENOMEM;
	}

	rc = cam_sensor_get_io_buffer(io_cfg, &(i2c_list->i2c_settings));
	if (rc) {
		CAM_ERR(CAM_SENSOR, "Failed to get read buffer: %d", rc);
	} else {
		*cmd_length_in_bytes = sizeof(struct cam_cmd_i2c_continuous_rd);
		i2c_list->op_code = CAM_SENSOR_I2C_READ_SEQ;

		i2c_list->i2c_settings.addr_type =
			cmd_i2c_continuous_rd->header.addr_type;
		i2c_list->i2c_settings.data_type =
			cmd_i2c_continuous_rd->header.data_type;
		i2c_list->i2c_settings.size =
			cmd_i2c_continuous_rd->header.count;
		i2c_list->i2c_settings.reg_setting[0].reg_addr =
			cmd_i2c_continuous_rd->reg_addr;

		*offset = cnt;
		*list = &(i2c_list->list);
	}

	return rc;
}

static int cam_sensor_handle_slave_info(
	struct camera_io_master *io_master,
	uint32_t *cmd_buf)
{
	int rc = 0;
	struct cam_cmd_i2c_info *i2c_info = (struct cam_cmd_i2c_info *)cmd_buf;

	if (io_master == NULL || cmd_buf == NULL) {
		CAM_ERR(CAM_SENSOR, "Invalid args");
		return -EINVAL;
	}

	switch (io_master->master_type) {
	case CCI_MASTER:
		io_master->cci_client->sid = (i2c_info->slave_addr >> 1);
		io_master->cci_client->i2c_freq_mode = i2c_info->i2c_freq_mode;
		break;

	case I2C_MASTER:
		io_master->client->addr = i2c_info->slave_addr;
		break;

	case SPI_MASTER:
		break;

	default:
		CAM_ERR(CAM_SENSOR, "Invalid master type: %d",
			io_master->master_type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

/**
 * Name : cam_sensor_i2c_command_parser
 * Description : Parse CSL CCI packet and apply register settings
 * Parameters :  io_master        input  master information
 *               i2c_reg_settings output register settings to fill
 *               cmd_desc         input  command description
 *               num_cmd_buffers  input  number of command buffers to process
 *               io_cfg           input  buffer details for read operation only
 * Description :
 * Handle multiple I2C RD/WR and WAIT cmd formats in one command
 * buffer, for example, a command buffer of m x RND_WR + 1 x HW_
 * WAIT + n x RND_WR with num_cmd_buf = 1. Do not exepect RD/WR
 * with different cmd_type and op_code in one command buffer.
 */
int cam_sensor_i2c_command_parser(
	struct camera_io_master *io_master,
	struct i2c_settings_array *i2c_reg_settings,
	struct cam_cmd_buf_desc   *cmd_desc,
	int32_t num_cmd_buffers,
	struct cam_buf_io_cfg *io_cfg)
{
	int16_t                   rc = 0, i = 0;
	size_t                    len_of_buff = 0;
	uintptr_t                 generic_ptr;
	uint16_t                  cmd_length_in_bytes = 0;
	size_t                    remain_len = 0;
	size_t                    tot_size = 0;

	for (i = 0; i < num_cmd_buffers; i++) {
		uint32_t                  *cmd_buf = NULL;
		struct common_header      *cmm_hdr;
		uint16_t                  generic_op_code;
		uint32_t                  byte_cnt = 0;
		uint32_t                  j = 0;
		struct list_head          *list = NULL;

		/*
		 * It is not expected the same settings to
		 * be spread across multiple cmd buffers
		 */
		CAM_DBG(CAM_SENSOR, "Total cmd Buf in Bytes: %d",
			cmd_desc[i].length);

		if (!cmd_desc[i].length)
			continue;

		rc = cam_mem_get_cpu_buf(cmd_desc[i].mem_handle,
			&generic_ptr, &len_of_buff);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"cmd hdl failed:%d, Err: %d, Buffer_len: %zd",
				cmd_desc[i].mem_handle, rc, len_of_buff);
			return rc;
		}

		remain_len = len_of_buff;
		if ((len_of_buff < sizeof(struct common_header)) ||
			(cmd_desc[i].offset >
			(len_of_buff - sizeof(struct common_header)))) {
			CAM_ERR(CAM_SENSOR, "buffer provided too small");
			return -EINVAL;
		}
		cmd_buf = (uint32_t *)generic_ptr;
		cmd_buf += cmd_desc[i].offset / sizeof(uint32_t);

		remain_len -= cmd_desc[i].offset;
		if (remain_len < cmd_desc[i].length) {
			CAM_ERR(CAM_SENSOR, "buffer provided too small");
			return -EINVAL;
		}

		while (byte_cnt < cmd_desc[i].length) {
			if ((remain_len - byte_cnt) <
				sizeof(struct common_header)) {
				CAM_ERR(CAM_SENSOR, "Not enough buffer");
				rc = -EINVAL;
				goto end;
			}
			cmm_hdr = (struct common_header *)cmd_buf;
			generic_op_code = cmm_hdr->fifth_byte;
			switch (cmm_hdr->cmd_type) {
			case CAMERA_SENSOR_CMD_TYPE_I2C_RNDM_WR: {
				uint32_t cmd_length_in_bytes   = 0;
				struct cam_cmd_i2c_random_wr
					*cam_cmd_i2c_random_wr =
					(struct cam_cmd_i2c_random_wr *)cmd_buf;

				if ((remain_len - byte_cnt) <
					sizeof(struct cam_cmd_i2c_random_wr)) {
					CAM_ERR(CAM_SENSOR,
						"Not enough buffer provided");
					rc = -EINVAL;
					goto end;
				}
				tot_size = sizeof(struct i2c_rdwr_header) +
					(sizeof(struct i2c_random_wr_payload) *
					cam_cmd_i2c_random_wr->header.count);

				if (tot_size > (remain_len - byte_cnt)) {
					CAM_ERR(CAM_SENSOR,
						"Not enough buffer provided");
					rc = -EINVAL;
					goto end;
				}

				rc = cam_sensor_handle_random_write(
					cam_cmd_i2c_random_wr,
					i2c_reg_settings,
					&cmd_length_in_bytes, &j, &list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
					"Failed in random write %d", rc);
					rc = -EINVAL;
					goto end;
				}

				cmd_buf += cmd_length_in_bytes /
					sizeof(uint32_t);
				byte_cnt += cmd_length_in_bytes;
				break;
			}
			case CAMERA_SENSOR_CMD_TYPE_I2C_CONT_WR: {
				uint32_t cmd_length_in_bytes   = 0;
				struct cam_cmd_i2c_continuous_wr
				*cam_cmd_i2c_continuous_wr =
				(struct cam_cmd_i2c_continuous_wr *)
				cmd_buf;

				if ((remain_len - byte_cnt) <
				sizeof(struct cam_cmd_i2c_continuous_wr)) {
					CAM_ERR(CAM_SENSOR,
						"Not enough buffer provided");
					rc = -EINVAL;
					goto end;
				}

				tot_size = sizeof(struct i2c_rdwr_header) +
				sizeof(cam_cmd_i2c_continuous_wr->reg_addr) +
				(sizeof(struct cam_cmd_read) *
				cam_cmd_i2c_continuous_wr->header.count);

				if (tot_size > (remain_len - byte_cnt)) {
					CAM_ERR(CAM_SENSOR,
						"Not enough buffer provided");
					rc = -EINVAL;
					goto end;
				}

				rc = cam_sensor_handle_continuous_write(
					cam_cmd_i2c_continuous_wr,
					i2c_reg_settings,
					&cmd_length_in_bytes, &j, &list);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
					"Failed in continuous write %d", rc);
					goto end;
				}

				cmd_buf += cmd_length_in_bytes /
					sizeof(uint32_t);
				byte_cnt += cmd_length_in_bytes;
				break;
			}
			case CAMERA_SENSOR_CMD_TYPE_WAIT: {
				if ((remain_len - byte_cnt) <
				sizeof(struct cam_cmd_unconditional_wait)) {
					CAM_ERR(CAM_SENSOR,
						"Not enough buffer space");
					rc = -EINVAL;
					goto end;
				}
				if (generic_op_code ==
					CAMERA_SENSOR_WAIT_OP_HW_UCND ||
					generic_op_code ==
						CAMERA_SENSOR_WAIT_OP_SW_UCND) {
					rc = cam_sensor_handle_delay(
						&cmd_buf, generic_op_code,
						i2c_reg_settings, j, &byte_cnt,
						list);
					if (rc < 0) {
						CAM_ERR(CAM_SENSOR,
							"delay hdl failed: %d",
							rc);
						goto end;
					}

				} else if (generic_op_code ==
					CAMERA_SENSOR_WAIT_OP_COND) {
					rc = cam_sensor_handle_poll(
						&cmd_buf, i2c_reg_settings,
						&byte_cnt, &j, &list);
					if (rc < 0) {
						CAM_ERR(CAM_SENSOR,
							"Random read fail: %d",
							rc);
						goto end;
					}
				} else {
					CAM_ERR(CAM_SENSOR,
						"Wrong Wait Command: %d",
						generic_op_code);
					rc = -EINVAL;
					goto end;
				}
				break;
			}
			case CAMERA_SENSOR_CMD_TYPE_I2C_INFO: {
				if (remain_len - byte_cnt <
					sizeof(struct cam_cmd_i2c_info)) {
					CAM_ERR(CAM_SENSOR,
						"Not enough buffer space");
					rc = -EINVAL;
					goto end;
				}
				rc = cam_sensor_handle_slave_info(
					io_master, cmd_buf);
				if (rc) {
					CAM_ERR(CAM_SENSOR,
					"Handle slave info failed with rc: %d",
					rc);
					goto end;
				}
				cmd_length_in_bytes =
					sizeof(struct cam_cmd_i2c_info);
				cmd_buf +=
					cmd_length_in_bytes / sizeof(uint32_t);
				byte_cnt += cmd_length_in_bytes;
				break;
			}
			case CAMERA_SENSOR_CMD_TYPE_I2C_RNDM_RD: {
				uint16_t cmd_length_in_bytes   = 0;
				struct cam_cmd_i2c_random_rd *i2c_random_rd =
				(struct cam_cmd_i2c_random_rd *)cmd_buf;

				if (remain_len - byte_cnt <
					sizeof(struct cam_cmd_i2c_random_rd)) {
					CAM_ERR(CAM_SENSOR,
						"Not enough buffer space");
					rc = -EINVAL;
					goto end;
				}

				tot_size = sizeof(struct i2c_rdwr_header) +
					(sizeof(struct cam_cmd_read) *
					i2c_random_rd->header.count);

				if (tot_size > (remain_len - byte_cnt)) {
					CAM_ERR(CAM_SENSOR,
						"Not enough buffer provided %d, %d, %d",
						tot_size, remain_len, byte_cnt);
					rc = -EINVAL;
					goto end;
				}

				rc = cam_sensor_handle_random_read(
					i2c_random_rd,
					i2c_reg_settings,
					&cmd_length_in_bytes, &j, &list,
					io_cfg);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
					"Failed in random read %d", rc);
					goto end;
				}

				cmd_buf += cmd_length_in_bytes /
					sizeof(uint32_t);
				byte_cnt += cmd_length_in_bytes;
				break;
			}
			case CAMERA_SENSOR_CMD_TYPE_I2C_CONT_RD: {
				uint16_t cmd_length_in_bytes   = 0;
				struct cam_cmd_i2c_continuous_rd
				*i2c_continuous_rd =
				(struct cam_cmd_i2c_continuous_rd *)cmd_buf;

				if (remain_len - byte_cnt <
				    sizeof(struct cam_cmd_i2c_continuous_rd)) {
					CAM_ERR(CAM_SENSOR,
						"Not enough buffer space");
					rc = -EINVAL;
					goto end;
				}

				tot_size =
				sizeof(struct cam_cmd_i2c_continuous_rd);

				if (tot_size > (remain_len - byte_cnt)) {
					CAM_ERR(CAM_SENSOR,
						"Not enough buffer provided %d, %d, %d",
						tot_size, remain_len, byte_cnt);
					rc = -EINVAL;
					goto end;
				}

				rc = cam_sensor_handle_continuous_read(
					i2c_continuous_rd,
					i2c_reg_settings,
					&cmd_length_in_bytes, &j, &list,
					io_cfg);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
					"Failed in continuous read %d", rc);
					goto end;
				}

				cmd_buf += cmd_length_in_bytes /
					sizeof(uint32_t);
				byte_cnt += cmd_length_in_bytes;
				break;
			}
			default:
				CAM_ERR(CAM_SENSOR, "Invalid Command Type:%d",
					 cmm_hdr->cmd_type);
				rc = -EINVAL;
				goto end;
			}
		}
		i2c_reg_settings->is_settings_valid = 1;
	}

end:
	return rc;
}

int cam_sensor_util_i2c_apply_setting(
	struct camera_io_master *io_master_info,
	struct i2c_settings_list *i2c_list)
{
	int32_t rc = 0;
	uint32_t i, size;

	switch (i2c_list->op_code) {
	case CAM_SENSOR_I2C_WRITE_RANDOM: {
		rc = camera_io_dev_write(io_master_info,
			&(i2c_list->i2c_settings));
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to random write I2C settings: %d",
				rc);
			return rc;
		}
	break;
	}
	case CAM_SENSOR_I2C_WRITE_SEQ: {
		rc = camera_io_dev_write_continuous(
			io_master_info, &(i2c_list->i2c_settings), CAM_SENSOR_I2C_WRITE_SEQ);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to seq write I2C settings: %d",
				rc);
			return rc;
		}
	break;
	}
	case CAM_SENSOR_I2C_WRITE_BURST: {
		rc = camera_io_dev_write_continuous(
			io_master_info, &(i2c_list->i2c_settings), CAM_SENSOR_I2C_WRITE_BURST);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"Failed to burst write I2C settings: %d",
				rc);
			return rc;
		}
	break;
	}
	case CAM_SENSOR_I2C_POLL: {
		size = i2c_list->i2c_settings.size;
		for (i = 0; i < size; i++) {
			rc = camera_io_dev_poll(
			io_master_info,
			i2c_list->i2c_settings.reg_setting[i].reg_addr,
			i2c_list->i2c_settings.reg_setting[i].reg_data,
			i2c_list->i2c_settings.reg_setting[i].data_mask,
			i2c_list->i2c_settings.addr_type,
			i2c_list->i2c_settings.data_type,
			i2c_list->i2c_settings.reg_setting[i].delay);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"i2c poll apply setting Fail: %d", rc);
				return rc;
			}
		}
	break;
	}
	default:
		CAM_ERR(CAM_SENSOR, "Wrong Opcode: %d", i2c_list->op_code);
		rc = -EINVAL;
	break;
	}

	return rc;
}

int32_t cam_sensor_i2c_read_data(
	struct i2c_settings_array *i2c_settings,
	struct camera_io_master *io_master_info)
{
	int32_t                   rc = 0;
	struct i2c_settings_list  *i2c_list;
	uint32_t                  cnt = 0;
	uint8_t                   *read_buff = NULL;
	uint32_t                  buff_length = 0;
	uint32_t                  read_length = 0;

	list_for_each_entry(i2c_list,
		&(i2c_settings->list_head), list) {
		read_buff = i2c_list->i2c_settings.read_buff;
		buff_length = i2c_list->i2c_settings.read_buff_len;
		if ((read_buff == NULL) || (buff_length == 0)) {
			CAM_ERR(CAM_SENSOR,
				"Invalid input buffer, buffer: %pK, length: %d",
				read_buff, buff_length);
			return -EINVAL;
		}

		if (i2c_list->op_code == CAM_SENSOR_I2C_READ_RANDOM) {
			read_length = i2c_list->i2c_settings.data_type *
				i2c_list->i2c_settings.size;
			if ((read_length > buff_length) ||
				(read_length < i2c_list->i2c_settings.size)) {
				CAM_ERR(CAM_SENSOR,
				"Invalid size, readLen:%d, bufLen:%d, size: %d",
				read_length, buff_length,
				i2c_list->i2c_settings.size);
				return -EINVAL;
			}
			for (cnt = 0; cnt < (i2c_list->i2c_settings.size);
				cnt++) {
				struct cam_sensor_i2c_reg_array *reg_setting =
				&(i2c_list->i2c_settings.reg_setting[cnt]);
				rc = camera_io_dev_read(io_master_info,
					reg_setting->reg_addr,
					&reg_setting->reg_data,
					i2c_list->i2c_settings.addr_type,
					i2c_list->i2c_settings.data_type);
				if (rc < 0) {
					CAM_ERR(CAM_SENSOR,
					"Failed: random read I2C settings: %d",
					rc);
					return rc;
				}
				if (i2c_list->i2c_settings.data_type <
					CAMERA_SENSOR_I2C_TYPE_MAX) {
					memcpy(read_buff,
					&reg_setting->reg_data,
					i2c_list->i2c_settings.data_type);
					read_buff +=
					i2c_list->i2c_settings.data_type;
				}
			}
		} else if (i2c_list->op_code == CAM_SENSOR_I2C_READ_SEQ) {
			read_length = i2c_list->i2c_settings.size;
			if (read_length > buff_length) {
				CAM_ERR(CAM_SENSOR,
				"Invalid buffer size, readLen: %d, bufLen: %d",
				read_length, buff_length);
				return -EINVAL;
			}
			rc = camera_io_dev_read_seq(
				io_master_info,
				i2c_list->i2c_settings.reg_setting[0].reg_addr,
				read_buff,
				i2c_list->i2c_settings.addr_type,
				i2c_list->i2c_settings.data_type,
				i2c_list->i2c_settings.size);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"failed: seq read I2C settings: %d",
					rc);
				return rc;
			}
		}
	}

	return rc;
}

int32_t msm_camera_fill_vreg_params(
	struct cam_hw_soc_info *soc_info,
	struct cam_sensor_power_setting *power_setting,
	uint16_t power_setting_size)
{
	int32_t rc = 0, j = 0, i = 0;
	int num_vreg;

	/* Validate input parameters */
	if (!soc_info || !power_setting) {
		CAM_ERR(CAM_SENSOR, "failed: soc_info %pK power_setting %pK",
			soc_info, power_setting);
		return -EINVAL;
	}

	num_vreg = soc_info->num_rgltr;

	if ((num_vreg <= 0) || (num_vreg > CAM_SOC_MAX_REGULATOR)) {
		CAM_ERR(CAM_SENSOR, "failed: num_vreg %d", num_vreg);
		return -EINVAL;
	}

	for (i = 0; i < power_setting_size; i++) {

		if (power_setting[i].seq_type < SENSOR_MCLK ||
			power_setting[i].seq_type >= SENSOR_SEQ_TYPE_MAX) {
			CAM_ERR(CAM_SENSOR, "failed: Invalid Seq type: %d",
				power_setting[i].seq_type);
			return -EINVAL;
		}

		switch (power_setting[i].seq_type) {
		case SENSOR_VDIG:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(soc_info->rgltr_name[j],
					"cam_vdig")) {

					CAM_DBG(CAM_SENSOR,
						"i: %d j: %d cam_vdig", i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}
			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		case SENSOR_VIO:
			for (j = 0; j < num_vreg; j++) {

				if (!strcmp(soc_info->rgltr_name[j],
					"cam_vio")) {
					CAM_DBG(CAM_SENSOR,
						"i: %d j: %d cam_vio", i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}

			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		case SENSOR_VANA:
			for (j = 0; j < num_vreg; j++) {

				if (!strcmp(soc_info->rgltr_name[j],
					"cam_vana")) {
					CAM_DBG(CAM_SENSOR,
						"i: %d j: %d cam_vana", i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}

			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		case SENSOR_VANA1:
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(soc_info->rgltr_name[j],
					"cam_vana1")) {
					CAM_DBG(CAM_SENSOR,
						"i: %d j: %d cam_vana1", i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}
			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		case SENSOR_VAF:
			for (j = 0; j < num_vreg; j++) {

				if (!strcmp(soc_info->rgltr_name[j],
					"cam_vaf")) {
					CAM_DBG(CAM_SENSOR,
						"i: %d j: %d cam_vaf", i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}

					break;
				}

			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;

		case SENSOR_CUSTOM_REG1:
			for (j = 0; j < num_vreg; j++) {

				if (!strcmp(soc_info->rgltr_name[j],
					"cam_v_custom1")) {
					CAM_DBG(CAM_SENSOR,
						"i:%d j:%d cam_vcustom1", i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}

			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;
		case SENSOR_CUSTOM_REG2:
			for (j = 0; j < num_vreg; j++) {

				if (!strcmp(soc_info->rgltr_name[j],
					"cam_v_custom2")) {
					CAM_DBG(CAM_SENSOR,
						"i:%d j:%d cam_vcustom2", i, j);
					power_setting[i].seq_val = j;

					if (VALIDATE_VOLTAGE(
						soc_info->rgltr_min_volt[j],
						soc_info->rgltr_max_volt[j],
						power_setting[i].config_val)) {
						soc_info->rgltr_min_volt[j] =
						soc_info->rgltr_max_volt[j] =
						power_setting[i].config_val;
					}
					break;
				}
			}
			if (j == num_vreg)
				power_setting[i].seq_val = INVALID_VREG;
			break;
		default:
			break;
		}
	}

	return rc;
}

int cam_sensor_util_request_gpio_table(
		struct cam_hw_soc_info *soc_info, int gpio_en)
{
	int rc = 0, i = 0;
	uint8_t size = 0;
	struct cam_soc_gpio_data *gpio_conf =
			soc_info->gpio_data;
	struct gpio *gpio_tbl = NULL;

	if (!gpio_conf) {
		CAM_DBG(CAM_SENSOR, "No GPIO data");
		return 0;
	}

	if (gpio_conf->cam_gpio_common_tbl_size <= 0) {
		CAM_ERR(CAM_SENSOR, "No GPIO entry");
		return -EINVAL;
	}

	gpio_tbl = gpio_conf->cam_gpio_req_tbl;
	size = gpio_conf->cam_gpio_req_tbl_size;

	if (!gpio_tbl || !size) {
		CAM_ERR(CAM_SENSOR, "invalid gpio_tbl %pK / size %d",
			gpio_tbl, size);
		return -EINVAL;
	}

	for (i = 0; i < size; i++) {
		CAM_DBG(CAM_SENSOR, "%s%d, i: %d, gpio %d dir %lld",
			soc_info->dev_name, soc_info->index, i,
			gpio_tbl[i].gpio, gpio_tbl[i].flags);
	}

	if (gpio_en) {
		for (i = 0; i < size; i++) {
			rc = cam_res_mgr_gpio_request(soc_info->dev,
					gpio_tbl[i].gpio,
					gpio_tbl[i].flags, gpio_tbl[i].label);
			if (rc) {
				/*
				 * After GPIO request fails, contine to
				 * apply new gpios, outout a error message
				 * for driver bringup debug
				 */
				CAM_ERR(CAM_SENSOR, "gpio %d:%s request fails",
					gpio_tbl[i].gpio, gpio_tbl[i].label);
			}
		}
	} else {
		cam_res_mgr_gpio_free_arry(soc_info->dev, gpio_tbl, size);
	}

	return rc;
}

bool cam_sensor_util_check_gpio_is_shared(
	struct cam_hw_soc_info *soc_info)
{
	int rc = 0;
	uint8_t size = 0;
	struct cam_soc_gpio_data *gpio_conf =
			soc_info->gpio_data;
	struct gpio *gpio_tbl = NULL;

	if (!gpio_conf) {
		CAM_DBG(CAM_SENSOR, "No GPIO data");
		return false;
	}

	if (gpio_conf->cam_gpio_common_tbl_size <= 0) {
		CAM_DBG(CAM_SENSOR, "No GPIO entry");
		return false;
	}

	gpio_tbl = gpio_conf->cam_gpio_req_tbl;
	size = gpio_conf->cam_gpio_req_tbl_size;

	if (!gpio_tbl || !size) {
		CAM_ERR(CAM_SENSOR, "invalid gpio_tbl %pK / size %d",
			gpio_tbl, size);
		return false;
	}

	rc = cam_res_mgr_util_check_if_gpio_is_shared(
		gpio_tbl, size);
	if (!rc) {
		CAM_DBG(CAM_SENSOR,
			"dev: %s don't have shared gpio resources",
			soc_info->dev_name);
		return false;
	}

	return true;
}

static int32_t cam_sensor_validate(void *ptr, size_t remain_buf)
{
	struct common_header *cmm_hdr = (struct common_header *)ptr;
	size_t validate_size = 0;

	if (remain_buf < sizeof(struct common_header))
		return -EINVAL;

	if (cmm_hdr->cmd_type == CAMERA_SENSOR_CMD_TYPE_PWR_UP ||
		cmm_hdr->cmd_type == CAMERA_SENSOR_CMD_TYPE_PWR_DOWN)
		validate_size = sizeof(struct cam_cmd_power);
	else if (cmm_hdr->cmd_type == CAMERA_SENSOR_CMD_TYPE_WAIT)
		validate_size = sizeof(struct cam_cmd_unconditional_wait);

	if (remain_buf < validate_size) {
		CAM_ERR(CAM_SENSOR, "Invalid cmd_buf len %zu min %zu",
			remain_buf, validate_size);
		return -EINVAL;
	}
	return 0;
}

int32_t cam_sensor_update_power_settings(void *cmd_buf,
	uint32_t cmd_length, struct cam_sensor_power_ctrl_t *power_info,
	size_t cmd_buf_len)
{
	int32_t rc = 0, tot_size = 0, last_cmd_type = 0;
	int32_t i = 0, pwr_up = 0, pwr_down = 0;
	struct cam_sensor_power_setting *pwr_settings;
	void *ptr = cmd_buf, *scr;
	struct cam_cmd_power *pwr_cmd = (struct cam_cmd_power *)cmd_buf;
	struct common_header *cmm_hdr = (struct common_header *)cmd_buf;

	if (!pwr_cmd || !cmd_length || cmd_buf_len < (size_t)cmd_length ||
		cam_sensor_validate(cmd_buf, cmd_buf_len)) {
		CAM_ERR(CAM_SENSOR, "Invalid Args: pwr_cmd %pK, cmd_length: %d",
			pwr_cmd, cmd_length);
		return -EINVAL;
	}

	power_info->power_setting_size = 0;
	power_info->power_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting) *
			MAX_POWER_CONFIG, GFP_KERNEL);
	if (!power_info->power_setting)
		return -ENOMEM;

	power_info->power_down_setting_size = 0;
	power_info->power_down_setting =
		kzalloc(sizeof(struct cam_sensor_power_setting) *
			MAX_POWER_CONFIG, GFP_KERNEL);
	if (!power_info->power_down_setting) {
		kfree(power_info->power_setting);
		power_info->power_setting = NULL;
		power_info->power_setting_size = 0;
		return -ENOMEM;
	}

	while (tot_size < cmd_length) {
		if (cam_sensor_validate(ptr, (cmd_length - tot_size))) {
			rc = -EINVAL;
			goto free_power_settings;
		}
		if (cmm_hdr->cmd_type ==
			CAMERA_SENSOR_CMD_TYPE_PWR_UP) {
			struct cam_cmd_power *pwr_cmd =
				(struct cam_cmd_power *)ptr;

			if ((U16_MAX - power_info->power_setting_size) <
				pwr_cmd->count) {
				CAM_ERR(CAM_SENSOR, "ERR: Overflow occurs");
				rc = -EINVAL;
				goto free_power_settings;
			}

			power_info->power_setting_size += pwr_cmd->count;
			if ((power_info->power_setting_size > MAX_POWER_CONFIG)
				|| (pwr_cmd->count >= SENSOR_SEQ_TYPE_MAX)) {
				CAM_ERR(CAM_SENSOR,
				"pwr_up setting size %d, pwr_cmd->count: %d",
					power_info->power_setting_size,
					pwr_cmd->count);
				rc = -EINVAL;
				goto free_power_settings;
			}
			scr = ptr + sizeof(struct cam_cmd_power);
			tot_size = tot_size + sizeof(struct cam_cmd_power);

			if (pwr_cmd->count == 0)
				CAM_WARN(CAM_SENSOR, "pwr_up_size is zero");

			for (i = 0; i < pwr_cmd->count; i++, pwr_up++) {
				power_info->power_setting[pwr_up].seq_type =
				pwr_cmd->power_settings[i].power_seq_type;
				power_info->power_setting[pwr_up].config_val =
				pwr_cmd->power_settings[i].config_val_low;
				power_info->power_setting[pwr_up].delay = 0;
				if (i) {
					scr = scr +
						sizeof(
						struct cam_power_settings);
					tot_size = tot_size +
						sizeof(
						struct cam_power_settings);
				}
				if (tot_size > cmd_length) {
					CAM_ERR(CAM_SENSOR,
						"Error: Cmd Buffer is wrong");
					rc = -EINVAL;
					goto free_power_settings;
				}
				CAM_DBG(CAM_SENSOR,
				"Seq Type[%d]: %d Config_val: %ld", pwr_up,
				power_info->power_setting[pwr_up].seq_type,
				power_info->power_setting[pwr_up].config_val);
			}
			last_cmd_type = CAMERA_SENSOR_CMD_TYPE_PWR_UP;
			ptr = (void *) scr;
			cmm_hdr = (struct common_header *)ptr;
		} else if (cmm_hdr->cmd_type == CAMERA_SENSOR_CMD_TYPE_WAIT) {
			struct cam_cmd_unconditional_wait *wait_cmd =
				(struct cam_cmd_unconditional_wait *)ptr;
			if ((wait_cmd->op_code ==
				CAMERA_SENSOR_WAIT_OP_SW_UCND) &&
				(last_cmd_type ==
				CAMERA_SENSOR_CMD_TYPE_PWR_UP)) {
				if (pwr_up > 0) {
					pwr_settings =
					&power_info->power_setting[pwr_up - 1];
					pwr_settings->delay +=
						wait_cmd->delay;
				} else {
					CAM_ERR(CAM_SENSOR,
					"Delay is expected only after valid power up setting");
				}
			} else if ((wait_cmd->op_code ==
				CAMERA_SENSOR_WAIT_OP_SW_UCND) &&
				(last_cmd_type ==
				CAMERA_SENSOR_CMD_TYPE_PWR_DOWN)) {
				if (pwr_down > 0) {
					pwr_settings =
					&power_info->power_down_setting[
						pwr_down - 1];
					pwr_settings->delay +=
						wait_cmd->delay;
				} else {
					CAM_ERR(CAM_SENSOR,
					"Delay is expected only after valid power up setting");
				}
			} else {
				CAM_DBG(CAM_SENSOR, "Invalid op code: %d",
					wait_cmd->op_code);
			}

			tot_size = tot_size +
				sizeof(struct cam_cmd_unconditional_wait);
			if (tot_size > cmd_length) {
				CAM_ERR(CAM_SENSOR, "Command Buffer is wrong");
				return -EINVAL;
			}
			scr = (void *) (wait_cmd);
			ptr = (void *)
				(scr +
				sizeof(struct cam_cmd_unconditional_wait));
			CAM_DBG(CAM_SENSOR, "ptr: %pK sizeof: %d Next: %pK",
				scr, (int32_t)sizeof(
				struct cam_cmd_unconditional_wait), ptr);

			cmm_hdr = (struct common_header *)ptr;
		} else if (cmm_hdr->cmd_type ==
			CAMERA_SENSOR_CMD_TYPE_PWR_DOWN) {
			struct cam_cmd_power *pwr_cmd =
				(struct cam_cmd_power *)ptr;

			scr = ptr + sizeof(struct cam_cmd_power);
			tot_size = tot_size + sizeof(struct cam_cmd_power);
			if ((U16_MAX - power_info->power_down_setting_size) <
				pwr_cmd->count) {
				CAM_ERR(CAM_SENSOR, "ERR: Overflow");
				rc = -EINVAL;
				goto free_power_settings;
			}

			power_info->power_down_setting_size += pwr_cmd->count;
			if ((power_info->power_down_setting_size >
				MAX_POWER_CONFIG) || (pwr_cmd->count >=
				SENSOR_SEQ_TYPE_MAX)) {
				CAM_ERR(CAM_SENSOR,
				"pwr_down_setting_size %d, pwr_cmd->count: %d",
					power_info->power_down_setting_size,
					pwr_cmd->count);
				rc = -EINVAL;
				goto free_power_settings;
			}

			if (pwr_cmd->count == 0)
				CAM_ERR(CAM_SENSOR, "pwr_down size is zero");

			for (i = 0; i < pwr_cmd->count; i++, pwr_down++) {
				pwr_settings =
				&power_info->power_down_setting[pwr_down];
				pwr_settings->seq_type =
				pwr_cmd->power_settings[i].power_seq_type;
				pwr_settings->config_val =
				pwr_cmd->power_settings[i].config_val_low;
				power_info->power_down_setting[pwr_down].delay
					= 0;
				if (i) {
					scr = scr +
						sizeof(
						struct cam_power_settings);
					tot_size =
						tot_size +
						sizeof(
						struct cam_power_settings);
				}
				if (tot_size > cmd_length) {
					CAM_ERR(CAM_SENSOR,
						"Command Buffer is wrong");
					rc = -EINVAL;
					goto free_power_settings;
				}
				CAM_DBG(CAM_SENSOR,
					"Seq Type[%d]: %d Config_val: %ld",
					pwr_down, pwr_settings->seq_type,
					pwr_settings->config_val);
			}
			last_cmd_type = CAMERA_SENSOR_CMD_TYPE_PWR_DOWN;
			ptr = (void *) scr;
			cmm_hdr = (struct common_header *)ptr;
		} else {
			CAM_ERR(CAM_SENSOR,
				"Error: Un expected Header Type: %d",
				cmm_hdr->cmd_type);
			rc = -EINVAL;
			goto free_power_settings;
		}
	}

	return rc;
free_power_settings:
	kfree(power_info->power_down_setting);
	kfree(power_info->power_setting);
	power_info->power_down_setting = NULL;
	power_info->power_setting = NULL;
	power_info->power_down_setting_size = 0;
	power_info->power_setting_size = 0;
	return rc;
}

int cam_get_dt_power_setting_data(struct device_node *of_node,
	struct cam_hw_soc_info *soc_info,
	struct cam_sensor_power_ctrl_t *power_info)
{
	int rc = 0, i;
	int count = 0;
	const char *seq_name = NULL;
	uint32_t *array = NULL;
	struct cam_sensor_power_setting *ps;
	int c, end;

	if (!power_info)
		return -EINVAL;

	count = of_property_count_strings(of_node, "qcom,cam-power-seq-type");
	power_info->power_setting_size = count;

	CAM_DBG(CAM_SENSOR, "qcom,cam-power-seq-type count %d", count);

	if (count <= 0)
		return 0;

	ps = kcalloc(count, sizeof(*ps), GFP_KERNEL);
	if (!ps)
		return -ENOMEM;
	power_info->power_setting = ps;

	for (i = 0; i < count; i++) {
		rc = of_property_read_string_index(of_node,
			"qcom,cam-power-seq-type", i, &seq_name);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "failed");
			goto ERROR1;
		}
		CAM_DBG(CAM_SENSOR, "seq_name[%d] = %s", i, seq_name);
		if (!strcmp(seq_name, "cam_vio")) {
			ps[i].seq_type = SENSOR_VIO;
		} else if (!strcmp(seq_name, "cam_vana")) {
			ps[i].seq_type = SENSOR_VANA;
		} else if (!strcmp(seq_name, "cam_vana1")) {
			ps[i].seq_type = SENSOR_VANA1;
		} else if (!strcmp(seq_name, "cam_clk")) {
			ps[i].seq_type = SENSOR_MCLK;
		} else {
			CAM_ERR(CAM_SENSOR, "unrecognized seq-type %s",
				seq_name);
			rc = -EILSEQ;
			goto ERROR1;
		}
		CAM_DBG(CAM_SENSOR, "seq_type[%d] %d", i, ps[i].seq_type);
	}

	array = kcalloc(count, sizeof(uint32_t), GFP_KERNEL);
	if (!array) {
		rc = -ENOMEM;
		goto ERROR1;
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-power-seq-cfg-val",
		array, count);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "failed ");
		goto ERROR2;
	}

	for (i = 0; i < count; i++) {
		ps[i].config_val = array[i];
		CAM_DBG(CAM_SENSOR, "power_setting[%d].config_val = %ld", i,
			ps[i].config_val);
	}

	rc = of_property_read_u32_array(of_node, "qcom,cam-power-seq-delay",
		array, count);
	if (rc < 0) {
		CAM_ERR(CAM_SENSOR, "failed");
		goto ERROR2;
	}
	for (i = 0; i < count; i++) {
		ps[i].delay = array[i];
		CAM_DBG(CAM_SENSOR, "power_setting[%d].delay = %d", i,
			ps[i].delay);
	}
	kfree(array);

	power_info->power_down_setting =
		kcalloc(count, sizeof(*ps), GFP_KERNEL);

	if (!power_info->power_down_setting) {
		CAM_ERR(CAM_SENSOR, "failed");
		rc = -ENOMEM;
		goto ERROR1;
	}

	power_info->power_down_setting_size = count;

	end = count - 1;

	for (c = 0; c < count; c++) {
		power_info->power_down_setting[c] = ps[end];
		end--;
	}
	return rc;
ERROR2:
	kfree(array);
ERROR1:
	kfree(ps);
	return rc;
}

int cam_sensor_util_init_gpio_pin_tbl(
	struct cam_hw_soc_info *soc_info,
	struct msm_camera_gpio_num_info **pgpio_num_info)
{
	int rc = 0, val = 0;
	uint32_t gpio_array_size;
	struct device_node *of_node = NULL;
	struct cam_soc_gpio_data *gconf = NULL;
	struct msm_camera_gpio_num_info *gpio_num_info = NULL;

	if (!soc_info->dev) {
		CAM_ERR(CAM_SENSOR, "device node NULL");
		return -EINVAL;
	}

	of_node = soc_info->dev->of_node;

	gconf = soc_info->gpio_data;
	if (!gconf) {
		CAM_ERR(CAM_SENSOR, "No gpio_common_table is found");
		return -EINVAL;
	}

	if (!gconf->cam_gpio_common_tbl) {
		CAM_ERR(CAM_SENSOR, "gpio_common_table is not initialized");
		return -EINVAL;
	}

	gpio_array_size = gconf->cam_gpio_common_tbl_size;

	if (!gpio_array_size) {
		CAM_ERR(CAM_SENSOR, "invalid size of gpio table");
		return -EINVAL;
	}

	*pgpio_num_info = kzalloc(sizeof(struct msm_camera_gpio_num_info),
		GFP_KERNEL);
	if (!*pgpio_num_info)
		return -ENOMEM;
	gpio_num_info = *pgpio_num_info;

	rc = of_property_read_u32(of_node, "gpio-vana", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "read gpio-vana failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-vana invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_VANA] =
				gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_VANA] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-vana %d",
			gpio_num_info->gpio_num[SENSOR_VANA]);
	}

	rc = of_property_read_u32(of_node, "gpio-vana1", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "read gpio-vana1 failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-vana1 invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_VANA1] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_VANA1] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-vana1 %d",
			gpio_num_info->gpio_num[SENSOR_VANA1]);
	}

	rc = of_property_read_u32(of_node, "gpio-vio", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "read gpio-vio failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-vio invalid %d", val);
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_VIO] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_VIO] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-vio %d",
			gpio_num_info->gpio_num[SENSOR_VIO]);
	}

	rc = of_property_read_u32(of_node, "gpio-vaf", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "read gpio-vaf failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-vaf invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_VAF] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_VAF] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-vaf %d",
			gpio_num_info->gpio_num[SENSOR_VAF]);
	}

	rc = of_property_read_u32(of_node, "gpio-vdig", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "read gpio-vdig failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-vdig invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_VDIG] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_VDIG] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-vdig %d",
				gpio_num_info->gpio_num[SENSOR_VDIG]);
	}

	rc = of_property_read_u32(of_node, "gpio-reset", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "read gpio-reset failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-reset invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_RESET] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_RESET] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-reset %d",
			gpio_num_info->gpio_num[SENSOR_RESET]);
	}

	rc = of_property_read_u32(of_node, "gpio-standby", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"read gpio-standby failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-standby invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_STANDBY] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_STANDBY] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-standby %d",
			gpio_num_info->gpio_num[SENSOR_STANDBY]);
	}

	rc = of_property_read_u32(of_node, "gpio-af-pwdm", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"read gpio-af-pwdm failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-af-pwdm invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_VAF_PWDM] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_VAF_PWDM] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-af-pwdm %d",
			gpio_num_info->gpio_num[SENSOR_VAF_PWDM]);
	}

	rc = of_property_read_u32(of_node, "gpio-custom1", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"read gpio-custom1 failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-custom1 invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_CUSTOM_GPIO1] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-custom1 %d",
			gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO1]);
	}

	rc = of_property_read_u32(of_node, "gpio-custom2", &val);
	if (rc != -EINVAL) {
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR,
				"read gpio-custom2 failed rc %d", rc);
			goto free_gpio_info;
		} else if (val >= gpio_array_size) {
			CAM_ERR(CAM_SENSOR, "gpio-custom2 invalid %d", val);
			rc = -EINVAL;
			goto free_gpio_info;
		}
		gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2] =
			gconf->cam_gpio_common_tbl[val].gpio;
		gpio_num_info->valid[SENSOR_CUSTOM_GPIO2] = 1;

		CAM_DBG(CAM_SENSOR, "gpio-custom2 %d",
			gpio_num_info->gpio_num[SENSOR_CUSTOM_GPIO2]);
	} else {
		rc = 0;
	}

	return rc;

free_gpio_info:
	kfree(gpio_num_info);
	gpio_num_info = NULL;
	return rc;
}

int msm_camera_pinctrl_init(
	struct msm_pinctrl_info *sensor_pctrl, struct device *dev)
{

	sensor_pctrl->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(sensor_pctrl->pinctrl)) {
		CAM_DBG(CAM_SENSOR, "Getting pinctrl handle failed");
		return -EINVAL;
	}
	sensor_pctrl->gpio_state_active =
		pinctrl_lookup_state(sensor_pctrl->pinctrl,
				CAM_SENSOR_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_active)) {
		CAM_ERR(CAM_SENSOR,
			"Failed to get the active state pinctrl handle");
		return -EINVAL;
	}
	sensor_pctrl->gpio_state_suspend
		= pinctrl_lookup_state(sensor_pctrl->pinctrl,
				CAM_SENSOR_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(sensor_pctrl->gpio_state_suspend)) {
		CAM_ERR(CAM_SENSOR,
			"Failed to get the suspend state pinctrl handle");
		return -EINVAL;
	}

	return 0;
}

int cam_sensor_bob_pwm_mode_switch(struct cam_hw_soc_info *soc_info,
	int bob_reg_idx, bool flag)
{
	int rc = 0;
	uint32_t op_current =
		(flag == true) ? soc_info->rgltr_op_mode[bob_reg_idx] : 0;

	if (soc_info->rgltr[bob_reg_idx] != NULL) {
		rc = regulator_set_load(soc_info->rgltr[bob_reg_idx],
			op_current);
		if (rc)
			CAM_WARN(CAM_SENSOR,
				"BoB PWM SetLoad failed rc: %d", rc);
	}

	return rc;
}

int msm_cam_sensor_handle_reg_gpio(int seq_type,
	struct msm_camera_gpio_num_info *gpio_num_info, int val)
{
	int gpio_offset = -1;

	if (!gpio_num_info) {
		CAM_INFO(CAM_SENSOR, "Input Parameters are not proper");
		return 0;
	}

	CAM_DBG(CAM_SENSOR, "Seq type: %d, config: %d", seq_type, val);

	gpio_offset = seq_type;

	if (gpio_num_info->valid[gpio_offset] == 1) {
		CAM_DBG(CAM_SENSOR, "VALID GPIO offset: %d, seqtype: %d",
			 gpio_offset, seq_type);
		cam_res_mgr_gpio_set_value(
			gpio_num_info->gpio_num
			[gpio_offset], val);
	}

	return 0;
}

static int cam_config_mclk_reg(struct cam_sensor_power_ctrl_t *ctrl,
	struct cam_hw_soc_info *soc_info, int32_t index)
{
	int32_t num_vreg = 0, j = 0, rc = 0, idx = 0;
	struct cam_sensor_power_setting *ps = NULL;
	struct cam_sensor_power_setting *pd = NULL;

	num_vreg = soc_info->num_rgltr;

	pd = &ctrl->power_down_setting[index];

	for (j = 0; j < num_vreg; j++) {
		if (!strcmp(soc_info->rgltr_name[j], "cam_clk")) {
			ps = NULL;
			for (idx = 0; idx < ctrl->power_setting_size; idx++) {
				if (ctrl->power_setting[idx].seq_type ==
					pd->seq_type) {
					ps = &ctrl->power_setting[idx];
					break;
				}
			}

			if (ps != NULL) {
				CAM_DBG(CAM_SENSOR, "Disable MCLK Regulator");
				rc = cam_soc_util_regulator_disable(
					soc_info->rgltr[j],
					soc_info->rgltr_name[j],
					soc_info->rgltr_min_volt[j],
					soc_info->rgltr_max_volt[j],
					soc_info->rgltr_op_mode[j],
					soc_info->rgltr_delay[j]);

				if (rc) {
					CAM_ERR(CAM_SENSOR,
						"MCLK REG DISALBE FAILED: %d",
						rc);
					return rc;
				}

				ps->data[0] =
					soc_info->rgltr[j];
			}
		}
	}

	return rc;
}

int cam_sensor_core_power_up(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info)
{
	int rc = 0, index = 0, no_gpio = 0, ret = 0, num_vreg, j = 0, i = 0;
	int32_t vreg_idx = -1;
	struct cam_sensor_power_setting *power_setting = NULL;
	struct msm_camera_gpio_num_info *gpio_num_info = NULL;

	CAM_DBG(CAM_SENSOR, "Enter");
	if (!ctrl) {
		CAM_ERR(CAM_SENSOR, "Invalid ctrl handle");
		return -EINVAL;
	}

	gpio_num_info = ctrl->gpio_num_info;
	num_vreg = soc_info->num_rgltr;

	if ((num_vreg <= 0) || (num_vreg > CAM_SOC_MAX_REGULATOR)) {
		CAM_ERR(CAM_SENSOR, "failed: num_vreg %d", num_vreg);
		return -EINVAL;
	}

	ret = msm_camera_pinctrl_init(&(ctrl->pinctrl_info), ctrl->dev);
	if (ret < 0) {
		/* Some sensor subdev no pinctrl. */
		CAM_DBG(CAM_SENSOR, "Initialization of pinctrl failed");
		ctrl->cam_pinctrl_status = 0;
	} else {
		ctrl->cam_pinctrl_status = 1;
	}

	rc = cam_sensor_util_request_gpio_table(soc_info, 1);
	if (rc < 0) {
		no_gpio = rc;
	}

	if (ctrl->cam_pinctrl_status) {
		ret = pinctrl_select_state(
			ctrl->pinctrl_info.pinctrl,
			ctrl->pinctrl_info.gpio_state_active);
		if (ret)
			CAM_ERR(CAM_SENSOR, "cannot set pin to active state");
	}

	CAM_DBG(CAM_SENSOR, "power setting size: %d", ctrl->power_setting_size);

	for (index = 0; index < ctrl->power_setting_size; index++) {
		CAM_DBG(CAM_SENSOR, "index: %d", index);
		power_setting = &ctrl->power_setting[index];
		if (!power_setting) {
			CAM_ERR(CAM_SENSOR,
				"Invalid power up settings for index %d",
				index);
			return -EINVAL;
		}

		CAM_DBG(CAM_SENSOR, "seq_type %d", power_setting->seq_type);

		switch (power_setting->seq_type) {
		case SENSOR_MCLK:
			if (power_setting->seq_val >= soc_info->num_clk) {
				CAM_ERR(CAM_SENSOR, "clk index %d >= max %u",
					power_setting->seq_val,
					soc_info->num_clk);
				goto power_up_failed;
			}
			for (j = 0; j < num_vreg; j++) {
				if (!strcmp(soc_info->rgltr_name[j],
					"cam_clk")) {
					CAM_DBG(CAM_SENSOR,
						"Enable cam_clk: %d", j);

					if (IS_ERR_OR_NULL(
						soc_info->rgltr[j])) {
						rc = PTR_ERR(
							soc_info->rgltr[j]);
						rc = rc ? rc : -EINVAL;
						CAM_ERR(CAM_SENSOR,
							"vreg %s %d",
							soc_info->rgltr_name[j],
							rc);
						goto power_up_failed;
					}

					rc =  cam_soc_util_regulator_enable(
					soc_info->rgltr[j],
					soc_info->rgltr_name[j],
					soc_info->rgltr_min_volt[j],
					soc_info->rgltr_max_volt[j],
					soc_info->rgltr_op_mode[j],
					soc_info->rgltr_delay[j]);
					if (rc) {
						CAM_ERR(CAM_SENSOR,
							"Reg enable failed");
						goto power_up_failed;
					}
					power_setting->data[0] =
						soc_info->rgltr[j];
				}
			}
			if (power_setting->config_val)
				soc_info->clk_rate[0][power_setting->seq_val] =
					power_setting->config_val;

			for (j = 0; j < soc_info->num_clk; j++) {
				rc = cam_soc_util_clk_enable(soc_info, false,
					j, 0, NULL);
				if (rc) {
					CAM_ERR(CAM_UTIL,
						"Failed in clk enable %d", i);
					break;
				}
			}

			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "clk enable failed");
				goto power_up_failed;
			}
			break;
		case SENSOR_RESET:
		case SENSOR_STANDBY:
		case SENSOR_CUSTOM_GPIO1:
		case SENSOR_CUSTOM_GPIO2:
			if (no_gpio) {
				CAM_ERR(CAM_SENSOR, "request gpio failed");
				goto power_up_failed;
			}
			if (!gpio_num_info) {
				CAM_ERR(CAM_SENSOR, "Invalid gpio_num_info");
				goto power_up_failed;
			}
#if IS_ENABLED(CONFIG_ISPV3)
                        if ((power_setting->config_val & POWER_CFG_VAL_TYPE_MASK)
                                        == POWER_CFG_VAL_TYPE_EX)
                                continue;
#endif
			CAM_DBG(CAM_SENSOR, "gpio set val %d",
				gpio_num_info->gpio_num
				[power_setting->seq_type]);

			rc = msm_cam_sensor_handle_reg_gpio(
				power_setting->seq_type,
				gpio_num_info,
				(int) power_setting->config_val);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Error in handling VREG GPIO");
				goto power_up_failed;
			}
			break;
		case SENSOR_VANA:
		case SENSOR_VANA1:
		case SENSOR_VDIG:
		case SENSOR_VIO:
		case SENSOR_VAF:
		case SENSOR_VAF_PWDM:
		case SENSOR_CUSTOM_REG1:
		case SENSOR_CUSTOM_REG2:
			if (power_setting->seq_val == INVALID_VREG)
				break;

			if (power_setting->seq_val >= CAM_VREG_MAX) {
				CAM_ERR(CAM_SENSOR, "vreg index %d >= max %d",
					power_setting->seq_val,
					CAM_VREG_MAX);
				goto power_up_failed;
			}
			if (power_setting->seq_val < num_vreg) {
				CAM_DBG(CAM_SENSOR, "Enable Regulator");
				vreg_idx = power_setting->seq_val;

				if (IS_ERR_OR_NULL(
					soc_info->rgltr[vreg_idx])) {
					rc = PTR_ERR(soc_info->rgltr[vreg_idx]);
					rc = rc ? rc : -EINVAL;

					CAM_ERR(CAM_SENSOR, "%s get failed %d",
						soc_info->rgltr_name[vreg_idx],
						rc);

					goto power_up_failed;
				}

				rc =  cam_soc_util_regulator_enable(
					soc_info->rgltr[vreg_idx],
					soc_info->rgltr_name[vreg_idx],
					soc_info->rgltr_min_volt[vreg_idx],
					soc_info->rgltr_max_volt[vreg_idx],
					soc_info->rgltr_op_mode[vreg_idx],
					soc_info->rgltr_delay[vreg_idx]);
				if (rc) {
					CAM_ERR(CAM_SENSOR,
						"Reg Enable failed for %s",
						soc_info->rgltr_name[vreg_idx]);
					goto power_up_failed;
				}
				power_setting->data[0] =
						soc_info->rgltr[vreg_idx];
			} else {
				CAM_ERR(CAM_SENSOR, "usr_idx:%d dts_idx:%d",
					power_setting->seq_val, num_vreg);
			}

			rc = msm_cam_sensor_handle_reg_gpio(
				power_setting->seq_type,
				gpio_num_info, 1);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Error in handling VREG GPIO");
				goto power_up_failed;
			}
			break;
#ifdef CONFIG_WL2866D
		case SENSOR_WL2866D_DVDD1:
		case SENSOR_WL2866D_DVDD2:
		case SENSOR_WL2866D_AVDD1:
		case SENSOR_WL2866D_AVDD2:
			//wl2866 out port num :
			//OUT_DVDD1 = 0
			//OUT_DVDD2 = 1
			//OUT_AVDD1 = 2
			//OUT_AVDD2 = 3
			//but we pre set SENSOR_WL2866D_DVDD1 = 13.
			rc = wl2866d_camera_power_up(((int)power_setting->seq_type),((int)power_setting->delay));
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,"wl2866d_camera_power_up_io_type [%d] failed",power_setting->seq_type);
				goto power_up_failed;
			}
			break;
#endif
		default:
			CAM_ERR(CAM_SENSOR, "error power seq type %d",
				power_setting->seq_type);
			break;
		}
		if (power_setting->delay > 20)
			msleep(power_setting->delay);
		else if (power_setting->delay)
			usleep_range(power_setting->delay * 1000,
				(power_setting->delay * 1000) + 1000);
	}

	return 0;
power_up_failed:
	CAM_ERR(CAM_SENSOR, "failed. rc:%d", rc);
	for (index--; index >= 0; index--) {
		CAM_DBG(CAM_SENSOR, "index %d",  index);
		power_setting = &ctrl->power_setting[index];
		CAM_DBG(CAM_SENSOR, "type %d",
			power_setting->seq_type);
		switch (power_setting->seq_type) {
		case SENSOR_MCLK:
			for (i = soc_info->num_clk - 1; i >= 0; i--) {
				cam_soc_util_clk_disable(soc_info, false, i);
			}
			ret = cam_config_mclk_reg(ctrl, soc_info, index);
			if (ret < 0) {
				CAM_ERR(CAM_SENSOR,
					"config clk reg failed rc: %d", ret);
				continue;
			}
			break;
		case SENSOR_RESET:
		case SENSOR_STANDBY:
		case SENSOR_CUSTOM_GPIO1:
		case SENSOR_CUSTOM_GPIO2:
			if (!gpio_num_info)
				continue;
			if (!gpio_num_info->valid
				[power_setting->seq_type])
				continue;
			cam_res_mgr_gpio_set_value(
				gpio_num_info->gpio_num
				[power_setting->seq_type], GPIOF_OUT_INIT_LOW);
			break;
		case SENSOR_VANA:
		case SENSOR_VANA1:
		case SENSOR_VDIG:
		case SENSOR_VIO:
		case SENSOR_VAF:
		case SENSOR_VAF_PWDM:
		case SENSOR_CUSTOM_REG1:
		case SENSOR_CUSTOM_REG2:
			if (power_setting->seq_val < num_vreg) {
				CAM_DBG(CAM_SENSOR, "Disable Regulator");
				vreg_idx = power_setting->seq_val;

				rc =  cam_soc_util_regulator_disable(
					soc_info->rgltr[vreg_idx],
					soc_info->rgltr_name[vreg_idx],
					soc_info->rgltr_min_volt[vreg_idx],
					soc_info->rgltr_max_volt[vreg_idx],
					soc_info->rgltr_op_mode[vreg_idx],
					soc_info->rgltr_delay[vreg_idx]);

				if (rc) {
					CAM_ERR(CAM_SENSOR,
					"Fail to disalbe reg: %s",
					soc_info->rgltr_name[vreg_idx]);
					soc_info->rgltr[vreg_idx] = NULL;
					msm_cam_sensor_handle_reg_gpio(
						power_setting->seq_type,
						gpio_num_info,
						GPIOF_OUT_INIT_LOW);
					continue;
				}
				power_setting->data[0] =
						soc_info->rgltr[vreg_idx];

			} else {
				CAM_ERR(CAM_SENSOR, "seq_val:%d > num_vreg: %d",
					power_setting->seq_val, num_vreg);
			}

			msm_cam_sensor_handle_reg_gpio(power_setting->seq_type,
				gpio_num_info, GPIOF_OUT_INIT_LOW);

			break;
		default:
			CAM_ERR(CAM_SENSOR, "error power seq type %d",
				power_setting->seq_type);
			break;
		}
		if (power_setting->delay > 20) {
			msleep(power_setting->delay);
		} else if (power_setting->delay) {
			usleep_range(power_setting->delay * 1000,
				(power_setting->delay * 1000) + 1000);
		}
	}

	if (ctrl->cam_pinctrl_status) {
		ret = pinctrl_select_state(
			ctrl->pinctrl_info.pinctrl,
			ctrl->pinctrl_info.gpio_state_suspend);
		if (ret)
			CAM_ERR(CAM_SENSOR, "cannot set pin to suspend state");
		devm_pinctrl_put(ctrl->pinctrl_info.pinctrl);
	}

	ctrl->cam_pinctrl_status = 0;
	cam_sensor_util_request_gpio_table(soc_info, 0);

	return -EINVAL;
}

#if IS_ENABLED(CONFIG_ISPV3)
int cam_sensor_core_power_up_extra(struct cam_sensor_power_ctrl_t *ctrl,
                struct cam_hw_soc_info *soc_info)
{
        int rc = 0, index = 0, no_gpio = 0;
        int config_val = 0;
        struct cam_sensor_power_setting *power_setting = NULL;
        struct msm_camera_gpio_num_info *gpio_num_info = NULL;

        CAM_DBG(CAM_SENSOR, "Enter");
        if (!ctrl) {
                CAM_ERR(CAM_SENSOR, "Invalid ctrl handle");
                return -EINVAL;
        }

        gpio_num_info = ctrl->gpio_num_info;

        CAM_DBG(CAM_SENSOR, "power setting size: %d", ctrl->power_setting_size);

        for (index = 0; index < ctrl->power_setting_size; index++) {
                CAM_DBG(CAM_SENSOR, "index: %d", index);
                power_setting = &ctrl->power_setting[index];
                if (!power_setting) {
                        CAM_ERR(CAM_SENSOR,
                                "Invalid power up settings for index %d",
                                index);
                        return -EINVAL;
                }

                if ((power_setting->config_val & POWER_CFG_VAL_TYPE_MASK)
                                != POWER_CFG_VAL_TYPE_EX)
                        continue;

                config_val = (int) power_setting->config_val & POWER_CFG_VAL_MASK;

                CAM_DBG(CAM_SENSOR,
                                "seq_type %d, power_setting.config_val %x, config_val %x",
                                power_setting->seq_type, power_setting->config_val, config_val);

                switch (power_setting->seq_type) {
                case SENSOR_CUSTOM_GPIO1:
                case SENSOR_CUSTOM_GPIO2:
                        if (no_gpio) {
                                CAM_ERR(CAM_SENSOR, "request gpio failed");
                                goto power_up_ex_failed;
                        }
                        if (!gpio_num_info) {
                                CAM_ERR(CAM_SENSOR, "Invalid gpio_num_info");
                                goto power_up_ex_failed;
                        }
                        CAM_INFO(CAM_SENSOR, "gpio set val %d, configval %d",
                                gpio_num_info->gpio_num[power_setting->seq_type], config_val);

                        rc = msm_cam_sensor_handle_reg_gpio(
                                power_setting->seq_type,
                                gpio_num_info,
                                config_val);
                        if (rc < 0) {
                                CAM_ERR(CAM_SENSOR,
                                        "Error in handling VREG GPIO");
                                goto power_up_ex_failed;
                        }
                        break;
                default:
                        CAM_ERR(CAM_SENSOR, "error power seq type %d",
                                power_setting->seq_type);
                        break;
                }

                if (power_setting->delay > 20) {
                        msleep(power_setting->delay);
                } else if (power_setting->delay) {
                        usleep_range(power_setting->delay * 1000,
                                (power_setting->delay * 1000) + 1000);
                }
        }

        return 0;

power_up_ex_failed:
        CAM_ERR(CAM_SENSOR, "failed");
        for (index--; index >= 0; index--) {
                CAM_DBG(CAM_SENSOR, "index %d",  index);
                power_setting = &ctrl->power_setting[index];
                CAM_DBG(CAM_SENSOR, "type %d",
                        power_setting->seq_type);
                switch (power_setting->seq_type) {
        case SENSOR_CUSTOM_GPIO1:
        case SENSOR_CUSTOM_GPIO2:
                        if (!gpio_num_info)
                                continue;
                        if (!gpio_num_info->valid
                                        [power_setting->seq_type])
                                continue;
                        cam_res_mgr_gpio_set_value(
                                        gpio_num_info->gpio_num
                                        [power_setting->seq_type], GPIOF_OUT_INIT_LOW);
                        break;
                default:
                        CAM_ERR(CAM_SENSOR, "error power seq type %d",
                                power_setting->seq_type);
                        break;
                }
                if (power_setting->delay > 20) {
                        msleep(power_setting->delay);
                } else if (power_setting->delay) {
                        usleep_range(power_setting->delay * 1000,
                                (power_setting->delay * 1000) + 1000);
                }
        }

        return rc;
}

int cam_sensor_util_power_down_extra(struct cam_sensor_power_ctrl_t *ctrl,
        struct cam_hw_soc_info *soc_info)
{
        int index = 0;
        struct cam_sensor_power_setting *pd = NULL;
        struct msm_camera_gpio_num_info *gpio_num_info = NULL;
        int config_val = 0;

        CAM_DBG(CAM_SENSOR, "Enter");
        if (!ctrl || !soc_info) {
            CAM_ERR(CAM_SENSOR, "failed ctrl %pK",  ctrl);
            return -EINVAL;
        }

        gpio_num_info = ctrl->gpio_num_info;

        if (ctrl->power_down_setting_size > MAX_POWER_CONFIG) {
            CAM_ERR(CAM_SENSOR, "Invalid: power setting size %d",
                    ctrl->power_setting_size);
            return -EINVAL;
        }

        for (index = 0; index < ctrl->power_down_setting_size; index++) {
            CAM_DBG(CAM_SENSOR, "power_down_index %d",  index);
            pd = &ctrl->power_down_setting[index];
            if (!pd) {
                CAM_ERR(CAM_SENSOR,
                        "Invalid power down settings for index %d",
                        index);
                return -EINVAL;
            }

            if ((pd->config_val & POWER_CFG_VAL_TYPE_MASK)
                    != POWER_CFG_VAL_TYPE_EX)
                continue;

            config_val = (int) pd->config_val & POWER_CFG_VAL_MASK;

            CAM_INFO(CAM_SENSOR,
                    "seq_type %d, power_setting.config_val %x, config_val %x, gpio:%d",
                    pd->seq_type, pd->config_val, config_val, gpio_num_info->gpio_num[pd->seq_type]);

            CAM_DBG(CAM_SENSOR, "seq_type %d",  pd->seq_type);
            switch (pd->seq_type) {
                case SENSOR_CUSTOM_GPIO1:
                case SENSOR_CUSTOM_GPIO2:

                    if (!gpio_num_info->valid[pd->seq_type])
                        continue;

                    cam_res_mgr_gpio_set_value(
                            gpio_num_info->gpio_num
                            [pd->seq_type],
                            (int)config_val);

                    break;
                default:
                    CAM_ERR(CAM_SENSOR, "error power seq type %d",
                            pd->seq_type);
                    break;
            }
            if (pd->delay > 20)
                msleep(pd->delay);
            else if (pd->delay)
                usleep_range(pd->delay * 1000,
                        (pd->delay * 1000) + 1000);
        }

        return 0;
}
#endif

static struct cam_sensor_power_setting*
msm_camera_get_power_settings(struct cam_sensor_power_ctrl_t *ctrl,
				enum msm_camera_power_seq_type seq_type,
				uint16_t seq_val)
{
	struct cam_sensor_power_setting *power_setting, *ps = NULL;
	int idx;

	for (idx = 0; idx < ctrl->power_setting_size; idx++) {
		power_setting = &ctrl->power_setting[idx];
		if (power_setting->seq_type == seq_type &&
			power_setting->seq_val ==  seq_val) {
			ps = power_setting;
			return ps;
		}

	}

	return ps;
}

int cam_sensor_util_power_down(struct cam_sensor_power_ctrl_t *ctrl,
		struct cam_hw_soc_info *soc_info)
{
	int index = 0, ret = 0, num_vreg = 0, i;
	struct cam_sensor_power_setting *pd = NULL;
	struct cam_sensor_power_setting *ps = NULL;
	struct msm_camera_gpio_num_info *gpio_num_info = NULL;

	CAM_DBG(CAM_SENSOR, "Enter");
	if (!ctrl || !soc_info) {
		CAM_ERR(CAM_SENSOR, "failed ctrl %pK",  ctrl);
		return -EINVAL;
	}

	gpio_num_info = ctrl->gpio_num_info;
	num_vreg = soc_info->num_rgltr;

	if ((num_vreg <= 0) || (num_vreg > CAM_SOC_MAX_REGULATOR)) {
		CAM_ERR(CAM_SENSOR, "failed: num_vreg %d", num_vreg);
		return -EINVAL;
	}

	if (ctrl->power_down_setting_size > MAX_POWER_CONFIG) {
		CAM_ERR(CAM_SENSOR, "Invalid: power setting size %d",
			ctrl->power_setting_size);
		return -EINVAL;
	}

	for (index = 0; index < ctrl->power_down_setting_size; index++) {
		CAM_DBG(CAM_SENSOR, "power_down_index %d",  index);
		pd = &ctrl->power_down_setting[index];
		if (!pd) {
			CAM_ERR(CAM_SENSOR,
				"Invalid power down settings for index %d",
				index);
			return -EINVAL;
		}

		ps = NULL;
#ifdef CONFIG_WL2866D
		CAM_DBG(CAM_SENSOR, "seq_type %d, seq_val %d config_val %ld delay %d",\
			pd->seq_type, pd->seq_val,pd->config_val, pd->delay);
#else
		CAM_DBG(CAM_SENSOR, "seq_type %d",  pd->seq_type);
#endif
		switch (pd->seq_type) {
		case SENSOR_MCLK:
			for (i = soc_info->num_clk - 1; i >= 0; i--) {
				cam_soc_util_clk_disable(soc_info, false, i);
			}

			ret = cam_config_mclk_reg(ctrl, soc_info, index);
			if (ret < 0) {
				CAM_ERR(CAM_SENSOR,
					"config clk reg failed rc: %d", ret);
				continue;
			}
			break;
		case SENSOR_RESET:
		case SENSOR_STANDBY:
		case SENSOR_CUSTOM_GPIO1:
		case SENSOR_CUSTOM_GPIO2:

			if (!gpio_num_info->valid[pd->seq_type])
				continue;

#if IS_ENABLED(CONFIG_ISPV3)
                        if ((pd->config_val & POWER_CFG_VAL_TYPE_MASK)
                                        == POWER_CFG_VAL_TYPE_EX)
                                continue;
#endif
			
			cam_res_mgr_gpio_set_value(
				gpio_num_info->gpio_num
				[pd->seq_type],
				(int) pd->config_val);

			break;
		case SENSOR_VANA:
		case SENSOR_VANA1:
		case SENSOR_VDIG:
		case SENSOR_VIO:
		case SENSOR_VAF:
		case SENSOR_VAF_PWDM:
		case SENSOR_CUSTOM_REG1:
		case SENSOR_CUSTOM_REG2:
			if (pd->seq_val == INVALID_VREG)
				break;

			ps = msm_camera_get_power_settings(
				ctrl, pd->seq_type,
				pd->seq_val);
			if (ps) {
				if (pd->seq_val < num_vreg) {
					CAM_DBG(CAM_SENSOR,
						"Disable Regulator");
					ret =  cam_soc_util_regulator_disable(
					soc_info->rgltr[ps->seq_val],
					soc_info->rgltr_name[ps->seq_val],
					soc_info->rgltr_min_volt[ps->seq_val],
					soc_info->rgltr_max_volt[ps->seq_val],
					soc_info->rgltr_op_mode[ps->seq_val],
					soc_info->rgltr_delay[ps->seq_val]);
					if (ret) {
						CAM_ERR(CAM_SENSOR,
						"Reg: %s disable failed",
						soc_info->rgltr_name[
							ps->seq_val]);
						msm_cam_sensor_handle_reg_gpio(
							pd->seq_type,
							gpio_num_info,
							GPIOF_OUT_INIT_LOW);
						continue;
					}
					ps->data[0] =
						soc_info->rgltr[ps->seq_val];
				} else {
					CAM_ERR(CAM_SENSOR,
						"seq_val:%d > num_vreg: %d",
						 pd->seq_val,
						num_vreg);
				}
			} else
				CAM_ERR(CAM_SENSOR,
					"error in power up/down seq");

			ret = msm_cam_sensor_handle_reg_gpio(pd->seq_type,
				gpio_num_info, GPIOF_OUT_INIT_LOW);

			if (ret < 0)
				CAM_ERR(CAM_SENSOR,
					"Error disabling VREG GPIO");
			break;
#ifdef CONFIG_WL2866D
		case SENSOR_WL2866D_DVDD1:
		case SENSOR_WL2866D_DVDD2:
		case SENSOR_WL2866D_AVDD1:
		case SENSOR_WL2866D_AVDD2:
			//wl2866 out port num :
			//OUT_DVDD1 = 0
			//OUT_DVDD2 = 1
			//OUT_AVDD1 = 2
			//OUT_AVDD2 = 3
			//but we pre set SENSOR_WL2866D_DVDD1 = 13.
			ret = wl2866d_camera_power_down(((int)pd->seq_type),((int)pd->delay));
			if (ret < 0) {
				CAM_ERR(CAM_SENSOR,"wl2866d_camera_power_down iotype [%d] failed", pd->seq_type);
				break;
			}
			break;
#endif

		default:
			CAM_ERR(CAM_SENSOR, "error power seq type %d",
				pd->seq_type);
			break;
		}
		if (pd->delay > 20)
			msleep(pd->delay);
		else if (pd->delay)
			usleep_range(pd->delay * 1000,
				(pd->delay * 1000) + 1000);
	}

	if (ctrl->cam_pinctrl_status) {
		ret = pinctrl_select_state(
				ctrl->pinctrl_info.pinctrl,
				ctrl->pinctrl_info.gpio_state_suspend);
		if (ret)
			CAM_ERR(CAM_SENSOR, "cannot set pin to suspend state");

		devm_pinctrl_put(ctrl->pinctrl_info.pinctrl);
	}

	cam_sensor_util_request_gpio_table(soc_info, 0);
	ctrl->cam_pinctrl_status = 0;

	return 0;
}
