/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#include <linux/spi/spi.h>
#include "goodix_ts_core.h"

/* name */
#define TS_DRIVER_NAME				"GT9896S"
#define TS_DT_COMPATIBLE			"goodix,gt9896s"

/* reg */
#define TS_REG_REMOVE_HOLD			0x2014
#define TS_REG_SET_SPI_ARGS			0x3082
//TODO:to modify later
#define TS_REG_ESD_TICK_R			0x3103

/* cmd */
#define COMMAND_SLEEP				0x05
#define COMMAND_END_SEND_CFG_YS		0x7D
#define COMMAND_START_SEND_CFG		0x80
#define COMMAND_SEND_SMALL_CFG		0x81
#define COMMAND_SEND_CFG_PREPARE_OK	0x82
#define COMMAND_END_SEND_CFG		0x83
#define COMMAND_READ_CFG_PREPARE_OK	0x85
#define COMMAND_START_READ_CFG		0x86

#define CMD_ACK_BUF_OVERFLOW		0x01
#define CMD_ACK_CHECKSUM_ERROR		0x02
#define CMD_ACK_BUSY				0x04
#define CMD_ACK_OK					0x80
#define CMD_ACK_IDLE				0xFF

/* flag */
#define SPI_FLAG_WR		0xF0
#define SPI_FLAG_RD		0xF1
#define MASK_8BIT 		0xFF

/* value */
#define REQUEST_HANDLED					0x00
#define REQUEST_CONFIG					0x01
#define REQUEST_BAKREF					0x02
#define REQUEST_RESET					0x03
#define REQUEST_RELOADFW				0x05
#define REQUEST_IDLE					0xff

#define TS_CMD_CFG_ERR					0x7E
#define TS_CMD_CFG_OK					0x7F
#define	TS_CMD_REG_READY				0xFF

#define TS_SET_SPI_ARGS_VALUE			0x0F
#define GOODIX_ESD_TICK_WRITE_DATA_YS	0xAA

/* times*/
#define TS_WAIT_CMD_FREE_RETRY_TIMES	10
#define TS_WAIT_CFG_READY_RETRY_TIMES	30
#define TS_RESET_IC_INIT_RETRY_TIMES	10

/* length*/
#define TS_CFG_HEAD_LEN_YS		5
#define IRQ_HEAD_LEN_YS			8
#define IRQ_HEAD_LEN_NOR		2
#define GOODIX_SPI_BUFF_MAX_SIZE	(8 * 1024 + 16)

/*others*/
#define TYPE_STYLUS					1
#define TYPE_TOUCH					2
#define TYPE_STYLUS_HOVER			3
#define TS_CFG_DATA_EQUAL_FLASH		99

#define BYTES_PER_COORD				8
#define TS_CFG_BAG_NUM_INDEX		2

/* this struct used for get lcm width and heigh. We have reached a consensus
 * with display owner and the members of tag_videolfb in front of lcm_width
 * are fixed. If you want to change tag_videolfb, please contect display owner
 * in advance.
 */
struct tag_videolfb {
	u64 fb_base;
	u32 islcmfound;
	u32 fps;
	u32 vram;
	char lcmname[1];
	u32 lcm_width;
	u32 lcm_heigh;
};
/*struction & enum*/
enum TS_SEND_CFG_REPLY {
	TS_CFG_REPLY_PKGS_ERR   = 0x01,
	TS_CFG_REPLY_CHKSUM_ERR = 0x02,
	TS_CFG_REPLY_DATA_ERR   = 0x03,
	TS_CFG_REPLY_DATA_EQU   = 0x07,
};

/*for config & firmware*/
const char *gt9896s_firmware_buf;
const char *gt9896s_config_buf;

static struct platform_device *gt9896s_pdev;

#ifdef CONFIG_OF
#define GET_L16(data) (data & 0xffff)
#define GET_H16(data) ((data >> 16) & 0xffff)
static void mtk_drm_lcm_info_get(struct gt9896s_ts_board_data *board_data)
{
	struct device_node *chosen_node;
	struct tag_videolfb *videolfb_tag = NULL;
	unsigned long size = 0;
	unsigned int offset = 0;
	u32 lcm_width;
	u32 lcm_heigh;
	u32 lcm_name_len;

	chosen_node = of_find_node_by_path("/chosen");
	if (chosen_node) {
		videolfb_tag = (struct tag_videolfb *)of_get_property(
			chosen_node, "atag,videolfb", (int *)&size);

		if (videolfb_tag) {
			lcm_name_len = strlen((char *)(videolfb_tag->lcmname));
			offset = 2 + 3 + ((lcm_name_len + 1 + 4) >> 2);
			lcm_width = *((unsigned int *)videolfb_tag + offset);
			lcm_heigh = *((unsigned int *)videolfb_tag + offset + 1);

			if ((lcm_heigh == 0) || (lcm_width == 0))
				return;

			if ((lcm_name_len == GET_H16(lcm_width))
				&& (lcm_name_len == GET_H16(lcm_heigh))) {
				board_data->lcm_max_x = GET_L16(lcm_width);
				board_data->lcm_max_y = GET_L16(lcm_heigh);
			} else {
				ts_err("heigh_h[%d], heigh_l[%d], width_h[%d], width_l[%d]",
					GET_H16(lcm_heigh), GET_L16(lcm_heigh),
					GET_H16(lcm_width), GET_H16(lcm_width));
			}
		} else {
			ts_err("videolfb_tag not found");
		}
	} else {
		ts_err("of_chosen not found");
	}

}
/**
 * gt9896s_parse_dt_resolution - parse resolution from dt
 * @node: devicetree node
 * @board_data: pointer to board data structure
 * return: 0 - no error, <0 error
 */
static int gt9896s_parse_dt_resolution(struct device_node *node,
		struct gt9896s_ts_board_data *board_data)
{
	int r, err;

	r = of_property_read_u32(node, "goodix,panel-max-x",
				 &board_data->panel_max_x);
	if (r)
		err = -ENOENT;

	r = of_property_read_u32(node, "goodix,panel-max-y",
				 &board_data->panel_max_y);
	if (r)
		err = -ENOENT;

	r = of_property_read_u32(node, "goodix,panel-max-w",
				 &board_data->panel_max_w);
	if (r)
		err = -ENOENT;

	/* For unreal lcm test */
	r = of_property_read_u32(node, "goodix,input-max-x",
				 &board_data->input_max_x);
	if (r)
		err = -ENOENT;

	r = of_property_read_u32(node, "goodix,input-max-y",
				&board_data->input_max_y);
	if (r)
		err = -ENOENT;

	board_data->swap_axis = of_property_read_bool(node,
			"goodix,swap-axis");

	board_data->x2x = of_property_read_bool(node,
			"goodix,x2x");

	board_data->y2y = of_property_read_bool(node,
			"goodix,y2y");

	return 0;
}

/**
 * gt9896s_parse_dt - parse board data from dt
 * @dev: pointer to device
 * @board_data: pointer to board data structure
 * return: 0 - no error, <0 error
 */
static int gt9896s_parse_dt(struct device_node *node,
	struct gt9896s_ts_board_data *board_data)
{
	struct property *prop;
	const char *name_tmp;
	int r;

	if (!board_data) {
		ts_err("invalid board data");
		return -EINVAL;
	}

	/* get gpio property*/
	r = of_get_named_gpio(node, "goodix,reset-gpio", 0);
	if (r < 0) {
		ts_err("invalid reset-gpio in dt: %d", r);
		return -EINVAL;
	}
	ts_info("get reset-gpio[%d] from dt", r);
	board_data->reset_gpio = r;

	r = of_get_named_gpio(node, "goodix,irq-gpio", 0);
	if (r < 0) {
		ts_err("invalid irq-gpio in dt: %d", r);
		return -EINVAL;
	}
	ts_info("get irq-gpio[%d] from dt", r);
	board_data->irq_gpio = r;

	/* get irq trigger type property*/
	r = of_property_read_u32(node, "goodix,irq-flags",
			&board_data->irq_flags);
	if (r) {
		ts_err("invalid irq-flags");
		return -EINVAL;
	}

	r = of_property_read_string(node, "goodix,firmware-version",
			&gt9896s_firmware_buf);
	if (r < 0)
		ts_err("Invalid firmware version in dts : %d", r);

	r = of_property_read_string(node, "goodix,config-version",
			&gt9896s_config_buf);
	if (r < 0) {
		ts_err("Invalid config version in dts : %d", r);
		return -EINVAL;
	}

	/* get power property*/
	memset(board_data->avdd_name, 0, sizeof(board_data->avdd_name));
	r = of_property_read_string(node, "goodix,avdd-name", &name_tmp);
	if (!r) {
		ts_info("avdd name form dt: %s", name_tmp);
		if (strlen(name_tmp) < sizeof(board_data->avdd_name))
			strlcpy(board_data->avdd_name,
				name_tmp, sizeof(board_data->avdd_name));
		else
			ts_info("invalied avdd name length: %ld > %ld",
				strlen(name_tmp),
				sizeof(board_data->avdd_name));
	}

	r = of_property_read_u32(node, "goodix,power-on-delay-us",
				&board_data->power_on_delay_us);
	if (!r) {
		/* 1000ms is too large, maybe you have pass a wrong value */
		if (board_data->power_on_delay_us > 1000 * 1000) {
			ts_err("Power on delay time exceed 1s, please check");
			board_data->power_on_delay_us = 0;
		}
	}

	r = of_property_read_u32(node, "goodix,power-off-delay-us",
				&board_data->power_off_delay_us);
	if (!r) {
		/* 1000ms is too large, maybe you have pass */
		if (board_data->power_off_delay_us > 1000 * 1000) {
			ts_err("Power off delay time exceed 1s, please check");
			board_data->power_off_delay_us = 0;
		}
	}

	/* get lcm info */
	mtk_drm_lcm_info_get(board_data);

	/* get xyz resolutions */
	r = gt9896s_parse_dt_resolution(node, board_data);
	if (r < 0) {
		ts_err("Failed to parse resolutions:%d", r);
		return r;
	}

	/* key map */
	prop = of_find_property(node, "goodix,panel-key-map", NULL);
	if (prop && prop->length) {
		if (prop->length / sizeof(u32) > GOODIX_MAX_TP_KEY) {
			ts_err("Size of panel-key-map is invalid");
			return r;
		}

		board_data->panel_max_key = prop->length / sizeof(u32);
		board_data->tp_key_num = prop->length / sizeof(u32);
		r = of_property_read_u32_array(node,
				"goodix,panel-key-map",
				&board_data->panel_key_map[0],
				board_data->panel_max_key);
		if (r) {
			ts_err("failed get key map, %d", r);
			return r;
		}
	}

	/*get pen-enable switch and pen keys, must after "key map"*/
	board_data->pen_enable = of_property_read_bool(node,
					"goodix,pen-enable");
	if (board_data->pen_enable)
		ts_info("goodix pen enabled");

	ts_info("***key:%d, %d, %d, %d",
		board_data->panel_key_map[0], board_data->panel_key_map[1],
		board_data->panel_key_map[2], board_data->panel_key_map[3]);

	ts_debug("[DT]x:%d, y:%d, w:%d, p:%d", board_data->panel_max_x,
		 board_data->panel_max_y, board_data->panel_max_w,
		 board_data->panel_max_p);

	return 0;
}
#endif

/**
 * gt9896s_spi_read- read device register through spi bus
 * @dev: pointer to device data
 * @addr: register address
 * @data: read buffer
 * @len: bytes to read
 * return: 0 - read ok, < 0 - spi transter error
 */
int gt9896s_spi_read(struct gt9896s_ts_device *dev, unsigned int addr,
	unsigned char *data, unsigned int len)
{
	struct spi_device *spi = dev->spi_dev;
	u8 *rx_buf = dev->rx_buff;
	u8 *tx_buf = dev->tx_buff;
	struct spi_transfer xfers;
	struct spi_message spi_msg;
	int ret = 0;

	spi_message_init(&spi_msg);
	memset(&xfers, 0, sizeof(xfers));

	/*spi_read tx_buf format: 0xF1 + addr(2bytes) + data*/
	tx_buf[0] = SPI_FLAG_RD; //0xF1 start read flag
	tx_buf[1] = (addr >> 8) & MASK_8BIT;
	tx_buf[2] = addr & MASK_8BIT;
	tx_buf[3] = MASK_8BIT;
	tx_buf[4] = MASK_8BIT;

	xfers.tx_buf = tx_buf;
	xfers.rx_buf = rx_buf;
	xfers.len = len + 5;
	xfers.cs_change = 0;
	spi_message_add_tail(&xfers, &spi_msg);
	ret = spi_sync(spi, &spi_msg);
	if (ret < 0) {
		ts_err("Spi transfer error:%d\n", ret);
		return ret;
	}
	memcpy(data, &rx_buf[5], len);

	return ret;
}

/**
 * gt9896s_spi_write- write device register through spi bus
 * @dev: pointer to device data
 * @addr: register address
 * @data: write buffer
 * @len: bytes to write
 * return: 0 - write ok; < 0 - spi transter error.
 */
int gt9896s_spi_write(struct gt9896s_ts_device *dev, unsigned int addr,
		unsigned char *data, unsigned int len)
{
	struct spi_device *spi = dev->spi_dev;
	u8 *tx_buf = dev->tx_buff;
	struct spi_transfer xfers;
	struct spi_message spi_msg;
	int ret = 0;

	spi_message_init(&spi_msg);
	memset(&xfers, 0, sizeof(xfers));

	/*spi_write tx_buf format: 0xF0 + addr(2bytes) + data*/
	tx_buf[0] = SPI_FLAG_WR; //0xF0 start write flag
	tx_buf[1] = (addr >> 8) & MASK_8BIT;
	tx_buf[2] = addr & MASK_8BIT;
	memcpy(&tx_buf[3], data, len);
	xfers.tx_buf = tx_buf;
	xfers.len = len + 3;
	xfers.cs_change = 0;
	spi_message_add_tail(&xfers, &spi_msg);
	//thp_bus_lock();
	ret = spi_sync(spi, &spi_msg);
	//thp_bus_unlock();
	if (ret < 0) {
		ts_err("Spi transfer error:%d\n", ret);
	}

	return ret;
}

/**
 * gt9896s_reset_ic_init- init ic after reset
 * @ts_dev: pointer to device data
 * return: 0 - init ok, < 0 - init failed
 */
int gt9896s_reset_ic_init(struct gt9896s_ts_device *ts_dev)
{
	u8 reg_val = 0;
	u8 ack_val = 0;
	int ret = -1, retry = 0;

	/* set spi transfer args */
	reg_val = TS_SET_SPI_ARGS_VALUE;
	for (retry = 0; retry < TS_RESET_IC_INIT_RETRY_TIMES; retry++) {
		/* write spi args*/
		ret = gt9896s_spi_write(ts_dev, TS_REG_SET_SPI_ARGS, &reg_val, 1);
		if (ret) {
			ts_err("spi write spi tranfer args failed, ret %d", ret);
			goto exit;
		}

		/* read back & check spi args*/
		ret = gt9896s_spi_read(ts_dev, TS_REG_SET_SPI_ARGS, &ack_val, 1);
		if (ret) {
			ts_err("spi read spi tranfer args failed, ret %d", ret);
			goto exit;
		}

		if (ack_val == reg_val) {
			ts_info("set spi tranfer args success!");
			break;
		} else {
			ts_err("set spi tranfer args failed, retry %d!", retry);
		}
	}
	if (TS_RESET_IC_INIT_RETRY_TIMES == retry) {
		ts_err("failed to set spi tranfer args 100 times!");
		return -EINVAL;
	}

	/* remove GIO force to hold CPU, confirm fw work normally for ES_CHIP */
	reg_val = 0x0;
	for (retry = 0; retry < TS_RESET_IC_INIT_RETRY_TIMES; retry++) {
		/* spi write to remove hold*/
		ret = gt9896s_spi_write(ts_dev, TS_REG_REMOVE_HOLD, &reg_val, 1);
		if (ret) {
			ts_err("spi write to remove GIO force to hold CPU failed");
			goto exit;
		}

		/* read back & check */
		ret = gt9896s_spi_read(ts_dev, TS_REG_REMOVE_HOLD, &ack_val, 1);
		if (ret) {
			ts_err("spi read to remove GIO force to hold CPU failed");
			goto exit;
		}

		if (ack_val == reg_val) {
			ts_info("remove GIO force to hold CPU success");
			break;
		} else {
			ts_err("failed to remove GIO force to hold CPU success, retry %d",
					retry);
		}
	}
	if (TS_RESET_IC_INIT_RETRY_TIMES == retry) {
		ts_err("failed to remove GIO force to hold CPU 100 times!");
		return -EINVAL;
	}

exit:
	return ret;
}

/* confirm current device is gt9896s or not.
 * If confirmed 0 will return.
 */
static int gt9896s_ts_dev_prepare(struct gt9896s_ts_device *ts_dev)
{
	int ret = 0;

	/* reset ic */
	gpio_direction_output(ts_dev->board_data.reset_gpio, 0);
	udelay(2000);
	gpio_direction_output(ts_dev->board_data.reset_gpio, 1);
	msleep(50);

	ts_info("ts_dev->ic_type = IC_TYPE_YELLOWSTONE_SPI");
	/* set spi args & remove GIO hold for ES_CHIP*/
	ret = gt9896s_reset_ic_init(ts_dev);
	if (ret)
		ts_err("reset ic init failed, ret %d", ret);

	return ret;
}

static void gt9896s_cmd_init(struct gt9896s_ts_device *dev,
			    struct gt9896s_ts_cmd *ts_cmd,
			    u8 cmds, u16 cmd_data, u32 reg_addr)
{
	u16 checksum = 0;
	ts_cmd->initialized = false;
	if (!reg_addr || !cmds)
		return;

	if (dev->ic_type == IC_TYPE_YELLOWSTONE_SPI) {
		ts_cmd->cmd_reg = reg_addr;
		ts_cmd->length = 5;
		ts_cmd->cmds[0] = cmds;
		ts_cmd->cmds[1] = (cmd_data >> 8) & 0xFF;
		ts_cmd->cmds[2] = cmd_data & 0xFF;
		checksum = ts_cmd->cmds[0] + ts_cmd->cmds[1] +
			ts_cmd->cmds[2];
		ts_cmd->cmds[3] = (checksum >> 8) & 0xFF;
		ts_cmd->cmds[4] = checksum & 0xFF;
		ts_cmd->initialized = true;
	} else {
		ts_err("unsupported ic type");
	}
}

/**
 * gt9896s_send_command - seng cmd to firmware
 *
 * @dev: pointer to device
 * @cmd: pointer to command struct which cotain command data
 * Returns 0 - succeed,<0 - failed
 */
int gt9896s_send_command(struct gt9896s_ts_device *dev, struct gt9896s_ts_cmd *cmd)
{
	int ret;

	if (!cmd || !cmd->initialized)
		return -EINVAL;

	ret = gt9896s_spi_write(dev, cmd->cmd_reg, cmd->cmds, cmd->length);
	if (ret < 0)
		ts_err("spi write to send command 0x%X failed,ret %d",
				cmd->cmds[0], ret);
	return ret;
}

static int gt9896s_read_version(struct gt9896s_ts_device *dev,
		struct gt9896s_ts_version *version)
{
	u8 buffer[GOODIX_PID_MAX_LEN + 1];
	u8 temp_buf[256];
	u16 checksum = 0;
	u8 pid_read_len = dev->reg.pid_len;
	u8 vid_read_len = dev->reg.vid_len;
	u8 sensor_id_mask = dev->reg.sensor_id_mask;
	int r;

	if (!version) {
		ts_err("pointer of version is NULL");
		return -EINVAL;
	}
	version->valid = false;

	/*check reg info valid*/
	if (!dev->reg.pid || !dev->reg.sensor_id || !dev->reg.vid) {
		ts_err("reg is NULL, pid:0x%04x, vid:0x%04x, sensor_id:0x%04x",
			dev->reg.pid, dev->reg.vid, dev->reg.sensor_id);
		return -EINVAL;
	}

	if (!pid_read_len || pid_read_len > GOODIX_PID_MAX_LEN ||
	    !vid_read_len || vid_read_len > GOODIX_VID_MAX_LEN) {
		ts_err("invalied pid vid length, pid_len:%d, vid_len:%d",
			pid_read_len, vid_read_len);
		return -EINVAL;
	}

	/*check checksum*/
	if (dev->reg.version_base && dev->reg.version_len < sizeof(temp_buf)) {
		r = gt9896s_spi_read(dev, dev->reg.version_base,
				temp_buf, dev->reg.version_len);
		if (r < 0) {
			ts_err("Read version base failed, reg:0x%02x, len:%d",
				dev->reg.version_base, dev->reg.version_len);
			if (version)
				version->valid = false;
			goto exit;
		}

		if (dev->ic_type == IC_TYPE_YELLOWSTONE_SPI)
			checksum = checksum_u8_ys(temp_buf, dev->reg.version_len);
		if (checksum) {
			ts_err("checksum error:0x%02x, base:0x%02x, len:%d",
			       checksum, dev->reg.version_base,
			       dev->reg.version_len);
			ts_err("%*ph", (int)(dev->reg.version_len / 2),
			       temp_buf);
			ts_err("%*ph", (int)(dev->reg.version_len -
						dev->reg.version_len / 2),
				&temp_buf[dev->reg.version_len / 2]);

			if (version)
				version->valid = false;
			r = -EINVAL;
			goto exit;
		}
	}

	/*read pid*/
	memset(buffer, 0, sizeof(buffer));
	memset(version->pid, 0, sizeof(version->pid));
	r = gt9896s_spi_read(dev, dev->reg.pid, buffer, pid_read_len);
	if (r < 0) {
		ts_err("Read pid failed");
		if (version)
			version->valid = false;
		goto exit;
	}
	memcpy(version->pid, buffer, pid_read_len);

	/*read vid*/
	memset(buffer, 0, sizeof(buffer));
	memset(version->vid, 0, sizeof(version->vid));
	r = gt9896s_spi_read(dev, dev->reg.vid, buffer, vid_read_len);
	if (r < 0) {
		ts_err("Read vid failed");
		if (version)
			version->valid = false;
		goto exit;
	}
	memcpy(version->vid, buffer, vid_read_len);

	/*read sensor_id*/
	memset(buffer, 0, sizeof(buffer));
	r = gt9896s_spi_read(dev, dev->reg.sensor_id, buffer, 1);
	if (r < 0) {
		ts_err("Read sensor_id failed");
		if (version)
			version->valid = false;
		goto exit;
	}
	if (sensor_id_mask != 0) {
		version->sensor_id = buffer[0] & sensor_id_mask;
		ts_info("sensor_id_mask:0x%02x, sensor_id:0x%02x",
			sensor_id_mask, version->sensor_id);
	} else {
		version->sensor_id = buffer[0];
	}

	version->valid = true;

	ts_info("PID:%s,SensorID:%d, VID:%*ph", version->pid,
		version->sensor_id, (int)sizeof(version->vid), version->vid);
exit:
	return r;
}

static int gt9896s_wait_cfg_cmd_ready(struct gt9896s_ts_device *dev,
			u8 right_cmd, u8 send_cmd)
{
	int try_times = 0;
	u8 cmd_flag = 0;
	u8 cmd_buf[3] = {0};
	u16 command_reg = dev->reg.command;
	struct gt9896s_ts_cmd ts_cmd;

	gt9896s_cmd_init(dev, &ts_cmd, send_cmd, 0, command_reg);

	for (try_times = 0; try_times < TS_WAIT_CFG_READY_RETRY_TIMES;
	     try_times++) {
		if (gt9896s_spi_read(dev, command_reg, cmd_buf, 3)) {
			ts_err("Read cmd_reg error");
			return -EINVAL;
		}
		cmd_flag = cmd_buf[0];
		if (cmd_flag == right_cmd) {
			return 0;
		} else if (cmd_flag != send_cmd) {
			ts_err("failed cmd_reg:0x%X, 0x%X, 0x%X",
			       cmd_buf[0], cmd_buf[1], cmd_buf[2]);
			if (gt9896s_send_command(dev, &ts_cmd)) {
				ts_err("Resend cmd 0x%02X FAILED", send_cmd);
				return -EINVAL;
			}
		}
		usleep_range(10000, 11000);
	}

	return -EINVAL;
}

static int _do_gt9896s_send_config(struct gt9896s_ts_device *dev,
		struct gt9896s_ts_config *config)
{
	int r = 0;
	int try_times = 0;
	u8 buf[3] = {0};
	u16 command_reg = dev->reg.command;
	u16 cfg_reg = dev->reg.cfg_addr;
	struct gt9896s_ts_cmd ts_cmd;

	/*1. Inquire command_reg until it's free*/
	for (try_times = 0; try_times < TS_WAIT_CMD_FREE_RETRY_TIMES; try_times++) {
		if (!gt9896s_spi_read(dev, command_reg, buf, 1) &&
		    buf[0] == TS_CMD_REG_READY)
			break;
		usleep_range(10000, 11000);
	}
	if (try_times >= TS_WAIT_CMD_FREE_RETRY_TIMES) {
		ts_err("failed send cfg, reg:0x%04x is not 0xff", command_reg);
		r = -EINVAL;
		goto exit;
	}

	/*2. send "start write cfg" command*/
	gt9896s_cmd_init(dev, &ts_cmd, COMMAND_START_SEND_CFG,
			 0, dev->reg.command);
	if (gt9896s_send_command(dev, &ts_cmd)) {
		ts_err("failed send cfg, COMMAND_START_SEND_CFG ERROR");
		r = -EINVAL;
		goto exit;
	}

	/*3. wait ic set command_reg to 0x82*/
	if (gt9896s_wait_cfg_cmd_ready(dev, COMMAND_SEND_CFG_PREPARE_OK,
				      COMMAND_START_SEND_CFG)) {
		ts_err("failed send cfg, reg:0x%04x is not 0x82", command_reg);
		r = -EINVAL;
		goto exit;
	}

	/*4. write cfg*/
	if (gt9896s_spi_write(dev, cfg_reg, config->data, config->length)) {
		ts_err("Send cfg FAILED, write cfg to fw ERROR");
		r = -EINVAL;
		goto exit;
	}

	/*5. send "end send cfg" command*/
	gt9896s_cmd_init(dev, &ts_cmd, COMMAND_END_SEND_CFG,
			 0, dev->reg.command);
	if (gt9896s_send_command(dev, &ts_cmd)) {
		ts_err("failed send cfg, COMMAND_END_SEND_CFG ERROR");
		r = -EINVAL;
		goto exit;
	}

	if (dev->ic_type == IC_TYPE_YELLOWSTONE_SPI) {
		/*6. wait 0x7f or 0x7e */
		for (try_times = 0; try_times < TS_WAIT_CMD_FREE_RETRY_TIMES;
		     try_times++) {
			r = gt9896s_spi_read(dev, command_reg, buf, 3);
			if (!r && (buf[0] == TS_CMD_CFG_ERR ||
				   buf[0] == TS_CMD_CFG_OK))
				break;
			usleep_range(10000, 11000);
		}
		ts_info("send config result: %*ph", 3, buf);

		/* set 0x7D to end send config process */
		gt9896s_cmd_init(dev, &ts_cmd, COMMAND_END_SEND_CFG_YS,
				 0, dev->reg.command);
		if (gt9896s_send_command(dev, &ts_cmd)) {
			ts_err("failed send cfg end cmd");
			r = -EINVAL;
			goto exit;
		}

		if (try_times >= TS_WAIT_CMD_FREE_RETRY_TIMES) {
			ts_err("failed get result");
			r = -EINVAL;
			goto exit;
		}

		if (buf[0] == TS_CMD_CFG_ERR) {
			if (buf[2] != TS_CFG_REPLY_DATA_EQU) {
				ts_err("failed send cfg");
				r = -EINVAL;
				goto exit;
			} else {
				ts_info("config data equal with flash");
				r = TS_CFG_DATA_EQUAL_FLASH;
				goto exit;
			}
		}
	}

	ts_info("Send cfg SUCCESS");
	r = 0;

exit:
	return r;
}

static int gt9896s_send_config(struct gt9896s_ts_device *dev,
		struct gt9896s_ts_config *config)
{
	int r = 0;

	if (!config || !config->initialized) {
		ts_err("invalid config data");
		return -EINVAL;
	}

	/*check configuration valid*/
	// TODO remove this
	// r = gt9896s_check_cfg_valid(dev, config->data, config->length);
	// if (r != 0) {
	// 	ts_err("cfg check FAILED");
	// 	return -EINVAL;
	// }

	ts_info("ver:%02xh,size:%d", config->data[0], config->length);
	mutex_lock(&config->lock);

	r = _do_gt9896s_send_config(dev, config);
	if (TS_CFG_DATA_EQUAL_FLASH == r)
		ts_info("config data equal with flash, r %d!", r);
	else if (r != 0)
		ts_err("_do_gt9896s_send_config fail, r %d!", r);

	mutex_unlock(&config->lock);
	return r;
}

/* success return config length else return -1 */
static int gt9896s_read_config_ys(struct gt9896s_ts_device *dev, u8 *buf)
{
	u32 cfg_addr = dev->reg.cfg_addr;
	int sub_bags = 0;
	int offset = 0;
	int subbag_len;
	u16 checksum;
	int i;
	int ret;

	/*read config head*/
	ret = gt9896s_spi_read(dev, cfg_addr, buf, TS_CFG_HEAD_LEN_YS);
	if (ret) {
		ts_err("spi read failed, addr 0x%X, ret %d", cfg_addr, ret);
		goto err_out;
	}

	offset = TS_CFG_HEAD_LEN_YS;
	sub_bags = buf[TS_CFG_BAG_NUM_INDEX];
	checksum = checksum_u8_ys(buf, TS_CFG_HEAD_LEN_YS);
	if (checksum) {
		ts_err("Config head checksum err:0x%x,data:%*ph",
				checksum, TS_CFG_HEAD_LEN_YS, buf);
		ret = -EINVAL;
		goto err_out;
	}

	ts_info("config_version:%u, vub_bags:%u", buf[0], sub_bags);
	for (i = 0; i < sub_bags; i++) {
		/* read sub head [0]: sub bag num, [1]: sub bag length */
		ret = gt9896s_spi_read(dev, cfg_addr + offset, buf + offset, 2);
		if (ret) {
			ts_err("spi read failed, addr 0x%X, ret %d", cfg_addr + offset, ret);
			goto err_out;
		}

		/* get sub bag length */
		subbag_len = buf[offset + 1];
		ts_debug("sub bag num:%u,sub bag length:%u",
			 buf[offset], subbag_len);

		/* read sub bag data */
		ret = gt9896s_spi_read(dev, cfg_addr + offset + 2,
				      buf + offset + 2, subbag_len + 2);
		if (ret) {
			ts_err("spi read failed, addr 0x%X, ret %d",
					cfg_addr + offset + 2, ret);
			goto err_out;
		}

		/* check sub bag checksum */
		checksum = checksum_u8_ys(buf + offset, subbag_len + 4);
		if (checksum) {
			ts_err("sub bag checksum err:0x%x", checksum);
			ret = -EINVAL;
			goto err_out;
		}
		offset += subbag_len + 4;
		ts_debug("sub bag %d, data:%*ph",
			 buf[offset], buf[offset + 1] + 4, buf + offset);
	}
	ret = offset;

err_out:
	return ret;
}

/* success return config_len, <= 0 failed */
static int gt9896s_read_config(struct gt9896s_ts_device *dev,
			      u8 *config_data)
{
	struct gt9896s_ts_cmd ts_cmd;
	u8 cmd_flag;
	u32 cmd_reg = dev->reg.command;
	int r = 0;
	int i;

	/*check config params legal*/
	if (!config_data) {
		ts_err("Illegal params");
		return -EINVAL;
	}

	if (!dev->reg.command) {
		ts_err("command register ERROR:0x%04x", dev->reg.command);
		return -EINVAL;
	}

	/* wait for IC in IDLE state */
	for (i = 0; i < TS_WAIT_CMD_FREE_RETRY_TIMES; i++) {
		cmd_flag = 0;
		r = gt9896s_spi_read(dev, cmd_reg, &cmd_flag, 1);
		if (r < 0 || cmd_flag == TS_CMD_REG_READY)
			break;
		usleep_range(10000, 11000);
	}
	if (cmd_flag != TS_CMD_REG_READY) {
		ts_err("Wait for IC ready IDEL state timeout:addr 0x%x\n", cmd_reg);
		r = -EAGAIN;
		goto exit;
	}

	/* 0x86 read config command */
	gt9896s_cmd_init(dev, &ts_cmd, COMMAND_START_READ_CFG,
			 0, cmd_reg);
	r = gt9896s_send_command(dev, &ts_cmd);
	if (r) {
		ts_err("Failed send read config command");
		goto exit;
	}

	/* wait for config data ready */
	if (gt9896s_wait_cfg_cmd_ready(dev, COMMAND_READ_CFG_PREPARE_OK,
				      COMMAND_START_READ_CFG)) {
		ts_err("Wait for config data ready timeout");
		r = -EAGAIN;
		goto exit;
	}

	if (dev->ic_type == IC_TYPE_YELLOWSTONE_SPI)
		r = gt9896s_read_config_ys(dev, config_data);

	if (r <= 0)
		ts_err("Failed read config data");

	/* clear command */
	gt9896s_cmd_init(dev, &ts_cmd, TS_CMD_REG_READY, 0, cmd_reg);
	r = gt9896s_send_command(dev, &ts_cmd);
	if (r) {
		ts_err("Failed send read config command");
		goto exit;
	}

exit:
	return r;
}

/**
 * gt9896s_hw_reset - reset device
 *
 * @dev: pointer to touch device
 * Returns 0 - succeed,<0 - failed
 */
int gt9896s_hw_reset(struct gt9896s_ts_device *dev)
{
	u8 data[2] = {0x00};
	int r = 0;

	ts_info("HW reset");

	gpio_direction_output(dev->board_data.reset_gpio, 0);
	udelay(2000);
	gpio_direction_output(dev->board_data.reset_gpio, 1);
	msleep(100);

	if (dev->ic_type == IC_TYPE_YELLOWSTONE_SPI) {
		/* set spi args & remove GIO hold for ES_CHIP*/
		r = gt9896s_reset_ic_init(dev);
		if (r) {
			ts_err("reset ic init failed, ret %d", r);
			return r;
		}
	}

	/*init dynamic esd*/
	if (dev->reg.esd) {
		r = gt9896s_spi_write(dev, dev->reg.esd, data, 1);
		if (r < 0)
			ts_err("IC reset, init dynamic esd FAILED");
	} else {
		ts_info("reg.esd is NULL, skip dynamic esd init");
	}

	return r;
}

/**
 * gt9896s_request_handler - handle firmware request
 *
 * @dev: pointer to touch device
 * @request_data: requset information
 * Returns 0 - succeed,<0 - failed
 */
static int gt9896s_request_handler(struct gt9896s_ts_device *dev)
{
	unsigned char buffer[1];
	int r;

	/* read reg.fw_request */
	r = gt9896s_spi_read(dev, dev->reg.fw_request, buffer, 1);
	if (r < 0) {
		ts_debug("spi read fw_request failed, addr 0x%X, r %d",
					dev->reg.fw_request, r);
		return r;
	}

	switch (buffer[0]) {
	case REQUEST_CONFIG:
		ts_info("HW request config");
		r = gt9896s_send_config(dev, &(dev->normal_cfg));
		if (r != 0)
			ts_info("request config, send config faild");
	break;
	case REQUEST_BAKREF:
		ts_info("HW request bakref");
	break;
	case REQUEST_RESET:
		ts_info("HW requset reset");
		r = gt9896s_hw_reset(dev);
		if (r != 0)
			ts_info("request reset, reset faild");
	break;
	case REQUEST_RELOADFW:
		ts_info("HW request reload fw");
		gt9896s_do_fw_update(UPDATE_MODE_FORCE|UPDATE_MODE_SRC_REQUEST);
	break;
	case REQUEST_IDLE:
		ts_info("HW request idle");
	break;
	default:
		ts_info("Unknown hw request:%d", buffer[0]);
	break;
	}

	buffer[0] = 0x00;
	r = gt9896s_spi_write(dev, dev->reg.fw_request, buffer, 1);
	return r;
}

static void gt9896s_swap_coords(struct gt9896s_ts_device *dev,
		unsigned int *coor_x, unsigned int *coor_y)
{
	unsigned int temp;
	struct gt9896s_ts_board_data *bdata = &dev->board_data;

	if (bdata->swap_axis) {
		temp = *coor_x;
		*coor_x = *coor_y;
		*coor_y = temp;
	}

	if (bdata->lcm_max_x && bdata->lcm_max_y) {
		if (!bdata->x2x)
			*coor_x = bdata->lcm_max_x - *coor_x;
		if (!bdata->y2y)
			*coor_y = bdata->lcm_max_y - *coor_y;
	} else {
		if (!bdata->x2x)
			*coor_x = bdata->panel_max_x - *coor_x;
		if (!bdata->y2y)
			*coor_y = bdata->panel_max_y - *coor_y;
	}
}

static void gt9896s_parse_finger_ys(struct gt9896s_ts_device *dev,
	struct gt9896s_touch_data *touch_data, unsigned char *buf, int touch_num)
{
	unsigned int id = 0, x = 0, y = 0, w = 0;
	static u32 pre_finger_map;
	u32 cur_finger_map = 0;
	u8 *coor_data;
	int i;

	coor_data = &buf[IRQ_HEAD_LEN_YS];
	for (i = 0; i < touch_num; i++) {
		id = (coor_data[0] >> 4) & 0x0F;
		if (id >= GOODIX_MAX_TOUCH) {
			ts_info("invaild finger id =%d", id);
			break;
		}
		x = be16_to_cpup((__be16 *)(coor_data + 2));
		y = be16_to_cpup((__be16 *)(coor_data + 4));
		w = be16_to_cpup((__be16 *)(coor_data + 6));
		gt9896s_swap_coords(dev, &x, &y);
		touch_data->coords[id].status = TS_TOUCH;
		touch_data->coords[id].x = x;
		touch_data->coords[id].y = y;
		touch_data->coords[id].w = w;
		cur_finger_map |= (1 << id);
		coor_data += BYTES_PER_COORD;
	}

	/* process finger release */
	for (i = 0; i < GOODIX_MAX_TOUCH; i++) {
		if (cur_finger_map & (1 << i))
			continue;
		if (pre_finger_map & (1 << i))
			touch_data->coords[i].status = TS_RELEASE;
	}

	pre_finger_map = cur_finger_map;
	touch_data->touch_num = touch_num;
}

static void gt9896s_parse_pen_ys(struct gt9896s_ts_device *dev,
	struct gt9896s_pen_data *pen_data, unsigned char *buf, int touch_num)
{
	ts_info("unsupported");
}

static int gt9896s_touch_handler_ys(struct gt9896s_ts_device *dev,
		struct gt9896s_ts_event *ts_event,
		u8 *pre_buf, u32 pre_buf_len)
{
	struct gt9896s_touch_data *touch_data = &ts_event->touch_data;
	struct gt9896s_pen_data *pen_data = &ts_event->pen_data;
	static u8 buffer[IRQ_HEAD_LEN_YS +
			 BYTES_PER_COORD * GOODIX_MAX_TOUCH + 2];
	int touch_num = 0, r = -EINVAL;
	u8 point_type = 0;
	u16 chksum = 0;

	static u8 pre_finger_num;
	static u8 pre_pen_num;

	/* clean event buffer */
	memset(ts_event, 0, sizeof(*ts_event));
	/* copy pre-data to buffer */
	memcpy(buffer, pre_buf, pre_buf_len);

	/* get touch num */
	touch_num = buffer[2] & 0x0F;
	if (unlikely(touch_num > GOODIX_MAX_TOUCH)) {
		touch_num = -EINVAL;
		goto exit_clean_sta;
	}

	/* read all coor data */
	if (unlikely(touch_num > 1)) {
		r = gt9896s_spi_read(dev, dev->reg.coor + pre_buf_len,
				&buffer[pre_buf_len], (touch_num - 1) * BYTES_PER_COORD);
		if (unlikely(r < 0))
			goto exit_clean_sta;
	}

	/* check coor data checksum except coor head */
	if (touch_num > 0) {
		chksum = checksum_u8_ys(&buffer[IRQ_HEAD_LEN_YS],
					touch_num * BYTES_PER_COORD + 2);
		if (unlikely(chksum != 0)) {
			ts_debug("checksum error:%x", chksum);
			r = -EINVAL;
			goto exit_clean_sta;
		}
	}

	/* get touch type */
	if (touch_num > 0)
		point_type = buffer[(touch_num - 1) * BYTES_PER_COORD + IRQ_HEAD_LEN_YS];
	if (touch_num >= 1 &&
		(point_type == TYPE_STYLUS_HOVER || point_type == TYPE_STYLUS)) {
		/* cur type is pen */
		if (pre_finger_num) {
			ts_event->event_type = EVENT_TOUCH;
			gt9896s_parse_finger_ys(dev, touch_data, buffer, 0);
			pre_finger_num = 0;
		} else {
			pre_pen_num = 1;
			ts_event->event_type = EVENT_PEN;
			gt9896s_parse_pen_ys(dev, pen_data, buffer, touch_num);
		}
	} else {
		/* cur type is finger */
		if (pre_pen_num) {
			ts_event->event_type = EVENT_PEN;
			gt9896s_parse_pen_ys(dev, pen_data, buffer, 0);
			pre_pen_num = 0;
		} else {
			ts_event->event_type = EVENT_TOUCH;
			gt9896s_parse_finger_ys(dev, touch_data, buffer, touch_num);
			pre_finger_num = touch_num;
		}
	}

	/* process custom info */
	if (buffer[3] & 0x01)
		ts_debug("TODO add custom info process function");

exit_clean_sta:
	return r;
}

static int gt9896s_event_handler(struct gt9896s_ts_device *dev,
		struct gt9896s_ts_event *ts_event)
{
	int pre_read_len = 0;
	u8 pre_buf[32];
	u8 event_sta;
	int r;

	/* get coor pre_read_len */
	if (dev->ic_type == IC_TYPE_YELLOWSTONE_SPI)
		pre_read_len = IRQ_HEAD_LEN_YS + BYTES_PER_COORD + 2;

	/* read coor head */
	r = gt9896s_spi_read(dev, dev->reg.coor, pre_buf, pre_read_len);
	if (unlikely(r < 0)) {
		ts_debug("spi read coor head failed, addr 0x%X, len %d, r %d",
				dev->reg.coor, pre_read_len, r);
		return r;
	}

	/* check coor head checksum */
	if (dev->ic_type == IC_TYPE_YELLOWSTONE_SPI &&
	    checksum_u8_ys(pre_buf, IRQ_HEAD_LEN_YS)) {
		ts_debug("irq head checksum error %*ph", IRQ_HEAD_LEN_YS, pre_buf);
		return -EINVAL;
	}

	/* buffer[0]: event state */
	event_sta = pre_buf[0];
	if (likely((event_sta & GOODIX_TOUCH_EVENT) == GOODIX_TOUCH_EVENT)) {
		/* handle touch event */
		if (dev->ic_type == IC_TYPE_YELLOWSTONE_SPI)
			gt9896s_touch_handler_ys(dev, ts_event, pre_buf, pre_read_len);
	} else if (unlikely((event_sta & GOODIX_REQUEST_EVENT) ==
			     GOODIX_REQUEST_EVENT)) {
		/* handle request event */
		ts_event->event_type = EVENT_REQUEST;
		gt9896s_request_handler(dev);
	} else if ((event_sta & GOODIX_GESTURE_EVENT) ==
		   GOODIX_GESTURE_EVENT) {
		/* handle gesture event */
		ts_debug("Gesture event");
	} else {
		ts_debug("unknow event type:0x%x", event_sta);
		r = -EINVAL;
	}

	return r;
}

/**
 * gt9896s_hw_suspend - Let touch deivce stay in lowpower mode.
 * @dev: pointer to gt9896s touch device
 * @return: 0 - succeed, < 0 - failed
 */
static int gt9896s_hw_suspend(struct gt9896s_ts_device *dev)
{
	struct gt9896s_ts_cmd sleep_cmd;
	int r = 0;

	gt9896s_cmd_init(dev, &sleep_cmd, COMMAND_SLEEP, 0, dev->reg.command);
	if (sleep_cmd.initialized) {
		r = gt9896s_send_command(dev, &sleep_cmd);
		if (!r)
			ts_info("Chip in sleep mode");
	} else {
		ts_err("Uninitialized sleep command");
	}
	return r;
}

/**
 * gt9896s_hw_resume - Let touch deivce stay in active  mode.
 * @dev: pointer to gt9896s touch device
 * @return: 0 - succeed, < 0 - failed
 */
static int gt9896s_hw_resume(struct gt9896s_ts_device *dev)
{
	int ret;

	ret = gt9896s_hw_reset(dev);
	if (ret)
		ts_err("%s: hw reset failed, ret %d", __func__, ret);

	return ret;
}

static int gt9896s_esd_check(struct gt9896s_ts_device *dev)
{
	int r;
	u8 data = 0;

	if (dev->reg.esd == 0) {
		ts_err("esd reg is NULL");
		return 0;
	}

	/*check dynamic esd*/
	//TODO:to check reg & value later
	r = dev->hw_ops->read_trans(dev, TS_REG_ESD_TICK_R, &data, 1);
	if (r < 0 || (data == GOODIX_ESD_TICK_WRITE_DATA_YS)) {
		ts_info("dynamic esd occur, r:%d, data:0x%02x", r, data);
		r = -EINVAL;
		goto exit;
	}

exit:
	return r;
}

/* hardware opeation funstions */
static const struct gt9896s_ts_hw_ops hw_spi_ops = {
	.dev_prepare = gt9896s_ts_dev_prepare,
	.read = gt9896s_spi_read,
	.write = gt9896s_spi_write,
	.read_trans = gt9896s_spi_read,
	.write_trans = gt9896s_spi_write,
	.reset = gt9896s_hw_reset,
	.event_handler = gt9896s_event_handler,
	.send_config = gt9896s_send_config,
	.read_config = gt9896s_read_config,
	.send_cmd = gt9896s_send_command,
	.read_version = gt9896s_read_version,
	.suspend = gt9896s_hw_suspend,
	.resume = gt9896s_hw_resume,
	.check_hw = gt9896s_esd_check,
};

static void gt9896s_pdev_release(struct device *dev)
{
	ts_info("gt9896s pdev released");
}
#define BOOT_UPDATE_FIRMWARE_NAME novatek_firmware
char novatek_firmware[25];

static int gt9896s_spi_probe(struct spi_device *spi)
{
	struct gt9896s_ts_device *ts_device = NULL;
	int r = 0;

	ts_info("%s IN", __func__);

	/* init spi_device */
	spi->mode            = SPI_MODE_0;
	spi->bits_per_word   = 8;
	spi->max_speed_hz    = 6 * 1000 * 1000;

	/* init ts device data */
	ts_device = devm_kzalloc(&spi->dev,
		sizeof(struct gt9896s_ts_device), GFP_KERNEL);
	if (!ts_device)
		return -ENOMEM;

	/* alloc memory for spi transfer buffer */
	ts_device->tx_buff = kzalloc(GOODIX_SPI_BUFF_MAX_SIZE, GFP_KERNEL);
	ts_device->rx_buff = kzalloc(GOODIX_SPI_BUFF_MAX_SIZE, GFP_KERNEL);
	if (!ts_device->tx_buff || !ts_device->rx_buff) {
		ts_err("%s: out of memory\n", __func__);
		r = -ENOMEM;
		goto err_spi_buf;
	}
	ts_device->name = "Goodix Touch GT9896S";
	ts_device->spi_dev = spi;
	ts_device->dev = &spi->dev;
	ts_device->hw_ops = &hw_spi_ops;

	/* parse devicetree property */
	if (IS_ENABLED(CONFIG_OF) && spi->dev.of_node) {
		r = gt9896s_parse_dt(spi->dev.of_node,
				    &ts_device->board_data);
		if (r < 0) {
			ts_err("failed parse device info form dts, %d", r);
			r = -EINVAL;
			goto err_spi_buf;
		}
	} else {
		ts_err("no valid device tree node found");
		r = -ENODEV;
		goto err_spi_buf;
	}

	/* init ts core device */
	gt9896s_pdev = kzalloc(sizeof(struct platform_device), GFP_KERNEL);
	if (!gt9896s_pdev) {
		ts_err("kzalloc core dev gt9896s_pdev faliled!");
		r = -ENOMEM;
		goto err_spi_buf;
	}

	/*
	 * you can find this platform dev in
	 * /sys/devices/platfrom/gt9896s_ts.0
	 * gt9896s_pdev->dev.parent = &spi->dev;
	 */
	gt9896s_pdev->name = GOODIX_CORE_DRIVER_NAME;
	gt9896s_pdev->id = 0;
	gt9896s_pdev->num_resources = 0;
	gt9896s_pdev->dev.platform_data = ts_device;
	gt9896s_pdev->dev.release = gt9896s_pdev_release;

	/* register platform device, then the gt9896s_ts_core
	 * module will probe the touch deivce.
	 */
	r = platform_device_register(gt9896s_pdev);
	if (r) {
		ts_err("failed register gt9896s platform device, %d", r);
		goto err_pdev;
	}

	/* register platform driver*/
	r = gt9896s_ts_core_init();
	if (r) {
		ts_err("failed register platform driver, %d", r);
		goto err_pdriver;
	}

	ts_info("%s OUT", __func__);
	return r;

err_pdriver:
	platform_device_unregister(gt9896s_pdev);
err_pdev:
	if (gt9896s_pdev) {
		kfree(gt9896s_pdev);
		gt9896s_pdev = NULL;
	}
err_spi_buf:
	if (ts_device->tx_buff) {
		kfree(ts_device->tx_buff);
		ts_device->tx_buff = NULL;
	}
	if (ts_device->rx_buff) {
		kfree(ts_device->rx_buff);
		ts_device->rx_buff = NULL;
	}
	ts_info("%s OUT, %d", __func__, r);
	return r;
}

static int gt9896s_spi_remove(struct spi_device *spi)
{
	if (gt9896s_pdev) {
		platform_device_unregister(gt9896s_pdev);
		kfree(gt9896s_pdev);
		gt9896s_pdev = NULL;
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id spi_matchs[] = {
	{.compatible = TS_DT_COMPATIBLE,},
	{},
};
#endif

static const struct spi_device_id spi_id_table[] = {
	{TS_DRIVER_NAME, 0},
	{},
};

static struct spi_driver gt9896s_spi_driver = {
	.driver = {
		.name = TS_DRIVER_NAME,
		.owner = THIS_MODULE,
		.bus = &spi_bus_type,
		.of_match_table = spi_matchs,
	},
	.id_table = spi_id_table,
	.probe = gt9896s_spi_probe,
	.remove = gt9896s_spi_remove,
};

/* release manully when prob failed */
void gt9896s_ts_dev_release(void)
{
	if (gt9896s_pdev) {
		platform_device_unregister(gt9896s_pdev);
		kfree(gt9896s_pdev);
		gt9896s_pdev = NULL;
	}
	spi_unregister_driver(&gt9896s_spi_driver);
}

static int __init gt9896s_spi_init(void)
{
	ts_info("GT9896S SPI driver init");
	return spi_register_driver(&gt9896s_spi_driver);
}

static void __exit gt9896s_spi_exit(void)
{
	spi_unregister_driver(&gt9896s_spi_driver);
	ts_info("GT9896S SPI driver exit");
}

late_initcall(gt9896s_spi_init);
module_exit(gt9896s_spi_exit);

MODULE_DESCRIPTION("Goodix THP Driver");
MODULE_AUTHOR("Goodix, Inc.");
MODULE_LICENSE("GPL v2");
