 /*
  * Goodix Touchscreen Driver
  * Copyright (C) 2020 - 2021 Goodix, Inc.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be a reference
  * to you, when you are integrating the GOODiX's CTP IC into your system,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * General Public License for more details.
  *
  */
#include "goodix_ts_core.h"

/* berlin_A SPI mode setting */
#define GOODIX_SPI_MODE_REG			0xC900
#define GOODIX_SPI_NORMAL_MODE_0	0x01

/* berlin_A D12 setting */
#define GOODIX_REG_CLK_STA0			0xD807
#define GOODIX_CLK_STA0_ENABLE		0xFF
#define GOODIX_REG_CLK_STA1			0xD806
#define GOODIX_CLK_STA1_ENABLE		0x77
#define GOODIX_REG_TRIM_D12			0xD006
#define GOODIX_TRIM_D12_LEVEL		0x3C
#define GOODIX_REG_RESET			0xD808
#define GOODIX_RESET_EN				0xFA
#define HOLD_CPU_REG_W				0x0002
#define HOLD_CPU_REG_R				0x2000

#define DEV_CONFIRM_VAL				0xAA
#define BOOTOPTION_ADDR				0x10000
#define FW_VERSION_INFO_ADDR_BRA	0x1000C
#define FW_VERSION_INFO_ADDR		0x10014

#define GOODIX_IC_INFO_MAX_LEN		1024
#define GOODIX_IC_INFO_ADDR_BRA		0x10068
#define GOODIX_IC_INFO_ADDR			0x10070


enum brl_request_code {
	BRL_REQUEST_CODE_CONFIG = 0x01,
	BRL_REQUEST_CODE_REF_ERR = 0x02,
	BRL_REQUEST_CODE_RESET = 0x03,
	BRL_REQUEST_CODE_CLOCK = 0x04,
};

static int brl_select_spi_mode(struct goodix_ts_core *cd)
{
	int ret;
	int i;
	u8 w_value = GOODIX_SPI_NORMAL_MODE_0;
	u8 r_value;

	if (cd->bus->bus_type == GOODIX_BUS_TYPE_I2C ||
			cd->bus->ic_type != IC_TYPE_BERLIN_A)
		return 0;

	for (i = 0; i < GOODIX_RETRY_5; i++) {
		cd->hw_ops->write(cd, GOODIX_SPI_MODE_REG,
				&w_value, 1);
		ret = cd->hw_ops->read(cd, GOODIX_SPI_MODE_REG,
				&r_value, 1);
		if (!ret && r_value == w_value)
			return 0;
	}
	ts_err("failed switch SPI mode, ret:%d r_value:%02x", ret, r_value);
	return -EINVAL;
}

static int brl_dev_confirm(struct goodix_ts_core *cd)
{
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	int ret = 0;
	int retry = GOODIX_RETRY_3;
	u8 tx_buf[8] = {0};
	u8 rx_buf[8] = {0};

	memset(tx_buf, DEV_CONFIRM_VAL, sizeof(tx_buf));
	while (retry--) {
		ret = hw_ops->write(cd, BOOTOPTION_ADDR,
			tx_buf, sizeof(tx_buf));
		if (ret < 0)
			return ret;
		ret = hw_ops->read(cd, BOOTOPTION_ADDR,
			rx_buf, sizeof(rx_buf));
		if (ret < 0)
			return ret;
		if (!memcmp(tx_buf, rx_buf, sizeof(tx_buf)))
			break;
		usleep_range(5000, 5100);
	}

	if (retry < 0) {
		ret = -EINVAL;
		ts_err("device confirm failed, rx_buf:%*ph", 8, rx_buf);
	}

	ts_info("device connected");
	return ret;
}

static int brl_reset_after(struct goodix_ts_core *cd)
{
	u8 reg_val[2] = {0};
	u8 temp_buf[12] = {0};
	int ret;
	int retry;

	if (cd->bus->ic_type != IC_TYPE_BERLIN_A)
		return 0;

	ts_info("IN");

	/* select spi mode */
	ret = brl_select_spi_mode(cd);
	if (ret < 0)
		return ret;

	/* hold cpu */
	retry = GOODIX_RETRY_10;
	while (retry--) {
		reg_val[0] = 0x01;
		reg_val[1] = 0x00;
		ret = cd->hw_ops->write(cd, HOLD_CPU_REG_W, reg_val, 2);
		ret |= cd->hw_ops->read(cd, HOLD_CPU_REG_R, &temp_buf[0], 4);
		ret |= cd->hw_ops->read(cd, HOLD_CPU_REG_R, &temp_buf[4], 4);
		ret |= cd->hw_ops->read(cd, HOLD_CPU_REG_R, &temp_buf[8], 4);
		if (!ret && !memcmp(&temp_buf[0], &temp_buf[4], 4) &&
			!memcmp(&temp_buf[4], &temp_buf[8], 4) &&
			!memcmp(&temp_buf[0], &temp_buf[8], 4)) {
			break;
		}
	}
	if (retry < 0) {
		ts_err("failed to hold cpu, status:%*ph", 12, temp_buf);
		return -EINVAL;
	}

	/* enable sta0 clk */
	retry = GOODIX_RETRY_5;
	while (retry--) {
		reg_val[0] = GOODIX_CLK_STA0_ENABLE;
		ret = cd->hw_ops->write(cd, GOODIX_REG_CLK_STA0, reg_val, 1);
		ret |= cd->hw_ops->read(cd, GOODIX_REG_CLK_STA0, temp_buf, 1);
		if (!ret && temp_buf[0] == GOODIX_CLK_STA0_ENABLE)
			break;
	}
	if (retry < 0) {
		ts_err("failed to enable group0 clock, ret:%d status:%02x",
				ret, temp_buf[0]);
		return -EINVAL;
	}

	/* enable sta1 clk */
	retry = GOODIX_RETRY_5;
	while (retry--) {
		reg_val[0] = GOODIX_CLK_STA1_ENABLE;
		ret = cd->hw_ops->write(cd, GOODIX_REG_CLK_STA1, reg_val, 1);
		ret |= cd->hw_ops->read(cd, GOODIX_REG_CLK_STA1, temp_buf, 1);
		if (!ret && temp_buf[0] == GOODIX_CLK_STA1_ENABLE)
			break;
	}
	if (retry < 0) {
		ts_err("failed to enable group1 clock, ret:%d status:%02x",
				ret, temp_buf[0]);
		return -EINVAL;
	}

	/* set D12 level */
	retry = GOODIX_RETRY_5;
	while (retry--) {
		reg_val[0] = GOODIX_TRIM_D12_LEVEL;
		ret = cd->hw_ops->write(cd, GOODIX_REG_TRIM_D12, reg_val, 1);
		ret |= cd->hw_ops->read(cd, GOODIX_REG_TRIM_D12, temp_buf, 1);
		if (!ret && temp_buf[0] == GOODIX_TRIM_D12_LEVEL)
			break;
	}
	if (retry < 0) {
		ts_err("failed to set D12, ret:%d status:%02x",
				ret, temp_buf[0]);
		return -EINVAL;
	}

	usleep_range(5000, 5100);
	/* soft reset */
	reg_val[0] = GOODIX_RESET_EN;
	ret = cd->hw_ops->write(cd, GOODIX_REG_RESET, reg_val, 1);
	if (ret < 0)
		return ret;

	/* select spi mode */
	ret = brl_select_spi_mode(cd);
	if (ret < 0)
		return ret;

	ts_info("OUT");

	return 0;
}

static int brl_power_on(struct goodix_ts_core *cd, bool on)
{
	int ret = 0;
	int iovdd_gpio = cd->board_data.iovdd_gpio;
	int avdd_gpio = cd->board_data.avdd_gpio;
	int reset_gpio = cd->board_data.reset_gpio;

	if (on) {
		if (iovdd_gpio > 0) {
			gpio_direction_output(iovdd_gpio, 1);
			ts_err("qzrtest---- to enable iovdd");
		} else if (cd->iovdd) {
			ret = regulator_enable(cd->iovdd);
			if (ret < 0) {
				ts_err("Failed to enable iovdd:%d", ret);
				goto power_off;
			}
		}
		usleep_range(3000, 3100);
		if (avdd_gpio > 0) {
			gpio_direction_output(avdd_gpio, 1);
			ts_err("qzrtest---- to enable avdd");
		} else if (cd->avdd) {
			ret = regulator_enable(cd->avdd);
			if (ret < 0) {
				ts_err("Failed to enable avdd:%d", ret);
				goto power_off;
			}
		}
		usleep_range(15000, 15100);
		gpio_direction_output(reset_gpio, 1);
		usleep_range(4000, 4100);
		ret = brl_dev_confirm(cd);
		if (ret < 0)
			goto power_off;
		ret = brl_reset_after(cd);
		if (ret < 0)
			goto power_off;

		msleep(GOODIX_NORMAL_RESET_DELAY_MS);
		return 0;
	}

power_off:
	gpio_direction_output(reset_gpio, 0);
	if (iovdd_gpio > 0)
		gpio_direction_output(iovdd_gpio, 0);
	else if (cd->iovdd)
		regulator_disable(cd->iovdd);
	if (avdd_gpio > 0)
		gpio_direction_output(avdd_gpio, 0);
	else if (cd->avdd)
		regulator_disable(cd->avdd);
	return ret;
}

#define GOODIX_SLEEP_CMD	0x84
int brl_suspend(struct goodix_ts_core *cd)
{
	struct goodix_ts_cmd sleep_cmd;

	sleep_cmd.cmd = GOODIX_SLEEP_CMD;
	sleep_cmd.len = 4;
	if (cd->hw_ops->send_cmd(cd, &sleep_cmd))
		ts_err("failed send sleep cmd");

	return 0;
}

int brl_resume(struct goodix_ts_core *cd)
{
	return cd->hw_ops->reset(cd, GOODIX_NORMAL_RESET_DELAY_MS);
}

#define GOODIX_GESTURE_CMD_BA	0x12
#define GOODIX_GESTURE_CMD		0xA6
int brl_gesture(struct goodix_ts_core *cd, int gesture_type)
{
	struct goodix_ts_cmd cmd;

	if (cd->bus->ic_type == IC_TYPE_BERLIN_A)
		cmd.cmd = GOODIX_GESTURE_CMD_BA;
	else
		cmd.cmd = GOODIX_GESTURE_CMD;
	cmd.len = 5;
	cmd.data[0] = gesture_type;
	if (cd->hw_ops->send_cmd(cd, &cmd))
		ts_err("failed send gesture cmd");

	return 0;
}

static int brl_reset(struct goodix_ts_core *cd, int delay)
{
	ts_info("chip_reset");

	gpio_direction_output(cd->board_data.reset_gpio, 0);
	usleep_range(2000, 2100);
	gpio_direction_output(cd->board_data.reset_gpio, 1);
	if (delay < 20)
		usleep_range(delay * 1000, delay * 1000 + 100);
	else
		msleep(delay);

	return brl_select_spi_mode(cd);
}

static int brl_irq_enbale(struct goodix_ts_core *cd, bool enable)
{
	if (enable && !atomic_cmpxchg(&cd->irq_enabled, 0, 1)) {
		enable_irq(cd->irq);
		ts_debug("Irq enabled");
		return 0;
	}

	if (!enable && atomic_cmpxchg(&cd->irq_enabled, 1, 0)) {
		disable_irq(cd->irq);
		ts_debug("Irq disabled");
		return 0;
	}
	ts_info("warnning: irq deepth inbalance!");
	return 0;
}

static int brl_read(struct goodix_ts_core *cd, unsigned int addr,
		unsigned char *data, unsigned int len)
{
	struct goodix_bus_interface *bus = cd->bus;

	return bus->read(bus->dev, addr, data, len);
}

static int brl_write(struct goodix_ts_core *cd, unsigned int addr,
		 unsigned char *data, unsigned int len)
{
	struct goodix_bus_interface *bus = cd->bus;

	return bus->write(bus->dev, addr, data, len);
}

/* command ack info */
#define CMD_ACK_IDLE             0x01
#define CMD_ACK_BUSY             0x02
#define CMD_ACK_BUFFER_OVERFLOW  0x03
#define CMD_ACK_CHECKSUM_ERROR   0x04
#define CMD_ACK_OK               0x80

#define GOODIX_CMD_RETRY 6
static int brl_send_cmd(struct goodix_ts_core *cd,
	struct goodix_ts_cmd *cmd)
{
	int ret, retry, i;
	struct goodix_ts_cmd cmd_ack;
	struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	cmd->state = 0;
	cmd->ack = 0;
	goodix_append_checksum(&(cmd->buf[2]), cmd->len - 2,
		CHECKSUM_MODE_U8_LE);
	ts_debug("cmd data %*ph", cmd->len, &(cmd->buf[2]));

	retry = 0;
	while (retry++ < GOODIX_CMD_RETRY) {
		ret = hw_ops->write(cd, misc->cmd_addr,
				    cmd->buf, sizeof(*cmd));
		if (ret < 0) {
			ts_err("failed write command");
			return ret;
		}
		for (i = 0; i < GOODIX_CMD_RETRY; i++) {
			/* check command result */
			ret = hw_ops->read(cd, misc->cmd_addr,
				cmd_ack.buf, sizeof(cmd_ack));
			if (ret < 0) {
				ts_err("failed read command ack, %d", ret);
				return ret;
			}
			ts_debug("cmd ack data %*ph",
				 (int)sizeof(cmd_ack), cmd_ack.buf);
			if (cmd_ack.ack == CMD_ACK_OK) {
				msleep(40);		// wait for cmd response
				return 0;
			}
			if (cmd_ack.ack == CMD_ACK_BUSY ||
			    cmd_ack.ack == 0x00) {
				usleep_range(1000, 1100);
				continue;
			}
			if (cmd_ack.ack == CMD_ACK_BUFFER_OVERFLOW)
				usleep_range(10000, 11000);
			usleep_range(1000, 1100);
			break;
		}
	}
	ts_err("failed get valid cmd ack");
	return -EINVAL;
}

#pragma  pack(1)
struct goodix_config_head {
	union {
		struct {
			u8 panel_name[8];
			u8 fw_pid[8];
			u8 fw_vid[4];
			u8 project_name[8];
			u8 file_ver[2];
			u32 cfg_id;
			u8 cfg_ver;
			u8 cfg_time[8];
			u8 reserved[15];
			u8 flag;
			u16 cfg_len;
			u8 cfg_num;
			u16 checksum;
		};
		u8 buf[64];
	};
};
#pragma pack()

#define CONFIG_CND_LEN			4
#define CONFIG_CMD_START		0x04
#define CONFIG_CMD_WRITE		0x05
#define CONFIG_CMD_EXIT			0x06
#define CONFIG_CMD_READ_START	0x07
#define CONFIG_CMD_READ_EXIT	0x08

#define CONFIG_CMD_STATUS_PASS	0x80
#define CONFIG_CMD_WAIT_RETRY	20

static int wait_cmd_status(struct goodix_ts_core *cd,
	u8 target_status, int retry)
{
	struct goodix_ts_cmd cmd_ack;
	struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	int i, ret;

	for (i = 0; i < retry; i++) {
		ret = hw_ops->read(cd, misc->cmd_addr, cmd_ack.buf,
			sizeof(cmd_ack));
		if (!ret && cmd_ack.state == target_status) {
			ts_debug("status check pass");
			return 0;
		}
		ts_debug("cmd buf %*ph", (int)sizeof(cmd_ack), cmd_ack.buf);
		msleep(20);
	}

	ts_err("cmd status not ready, retry %d, ack 0x%x, status 0x%x, ret %d",
			i, cmd_ack.ack, cmd_ack.state, ret);
	return -EINVAL;
}

static int send_cfg_cmd(struct goodix_ts_core *cd,
	struct goodix_ts_cmd *cfg_cmd)
{
	int ret;

	ret = cd->hw_ops->send_cmd(cd, cfg_cmd);
	if (ret) {
		ts_err("failed write cfg prepare cmd %d", ret);
		return ret;
	}
	ret = wait_cmd_status(cd, CONFIG_CMD_STATUS_PASS,
		CONFIG_CMD_WAIT_RETRY);
	if (ret) {
		ts_err("failed wait for fw ready for config, %d", ret);
		return ret;
	}
	return 0;
}

static int brl_send_config(struct goodix_ts_core *cd, u8 *cfg, int len)
{
	int ret;
	u8 *tmp_buf;
	struct goodix_ts_cmd cfg_cmd;
	struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	if (len > misc->fw_buffer_max_len) {
		ts_err("config len exceed limit %d > %d",
			len, misc->fw_buffer_max_len);
		return -EINVAL;
	}

	tmp_buf = kzalloc(len, GFP_KERNEL);
	if (!tmp_buf)
		return -ENOMEM;

	cfg_cmd.len = CONFIG_CND_LEN;
	cfg_cmd.cmd = CONFIG_CMD_START;
	ret = send_cfg_cmd(cd, &cfg_cmd);
	if (ret) {
		ts_err("failed write cfg prepare cmd %d", ret);
		goto exit;
	}

	ts_debug("try send config to 0x%x, len %d", misc->fw_buffer_addr, len);
	ret = hw_ops->write(cd, misc->fw_buffer_addr, cfg, len);
	if (ret) {
		ts_err("failed write config data, %d", ret);
		goto exit;
	}
	ret = hw_ops->read(cd, misc->fw_buffer_addr, tmp_buf, len);
	if (ret) {
		ts_err("failed read back config data");
		goto exit;
	}

	if (memcmp(cfg, tmp_buf, len)) {
		ts_err("config data read back compare file");
		ret = -EINVAL;
		goto exit;
	}
	/* notify fw for recive config */
	memset(cfg_cmd.buf, 0, sizeof(cfg_cmd));
	cfg_cmd.len = CONFIG_CND_LEN;
	cfg_cmd.cmd = CONFIG_CMD_WRITE;
	ret = send_cfg_cmd(cd, &cfg_cmd);
	if (ret)
		ts_err("failed send config data ready cmd %d", ret);

exit:
	memset(cfg_cmd.buf, 0, sizeof(cfg_cmd));
	cfg_cmd.len = CONFIG_CND_LEN;
	cfg_cmd.cmd = CONFIG_CMD_EXIT;
	if (send_cfg_cmd(cd, &cfg_cmd)) {
		ts_err("failed send config write end command");
		ret = -EINVAL;
	}

	if (!ret) {
		ts_info("success send config");
		msleep(100);
	}

	kfree(tmp_buf);
	return ret;
}

/*
 * return: return config length on succes, other wise return < 0
 **/
static int brl_read_config(struct goodix_ts_core *cd, u8 *cfg, int size)
{
	int ret;
	struct goodix_ts_cmd cfg_cmd;
	struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	struct goodix_config_head cfg_head;

	if (!cfg)
		return -EINVAL;

	cfg_cmd.len = CONFIG_CND_LEN;
	cfg_cmd.cmd = CONFIG_CMD_READ_START;
	ret = send_cfg_cmd(cd, &cfg_cmd);
	if (ret) {
		ts_err("failed send config read prepare command");
		return ret;
	}

	ret = hw_ops->read(cd, misc->fw_buffer_addr,
			   cfg_head.buf, sizeof(cfg_head));
	if (ret) {
		ts_err("failed read config head %d", ret);
		goto exit;
	}

	if (checksum_cmp(cfg_head.buf, sizeof(cfg_head), CHECKSUM_MODE_U8_LE)) {
		ts_err("config head checksum error");
		ret = -EINVAL;
		goto exit;
	}

	cfg_head.cfg_len = le16_to_cpu(cfg_head.cfg_len);
	if (cfg_head.cfg_len > misc->fw_buffer_max_len ||
	    cfg_head.cfg_len > size) {
		ts_err("cfg len exceed buffer size %d > %d", cfg_head.cfg_len,
			 misc->fw_buffer_max_len);
		ret = -EINVAL;
		goto exit;
	}

	memcpy(cfg, cfg_head.buf, sizeof(cfg_head));
	ret = hw_ops->read(cd, misc->fw_buffer_addr + sizeof(cfg_head),
			   cfg + sizeof(cfg_head), cfg_head.cfg_len);
	if (ret) {
		ts_err("failed read cfg pack, %d", ret);
		goto exit;
	}

	ts_info("config len %d", cfg_head.cfg_len);
	if (checksum_cmp(cfg + sizeof(cfg_head),
			 cfg_head.cfg_len, CHECKSUM_MODE_U16_LE)) {
		ts_err("config body checksum error");
		ret = -EINVAL;
		goto exit;
	}
	ts_info("success read config data: len %zu",
		cfg_head.cfg_len + sizeof(cfg_head));
exit:
	memset(cfg_cmd.buf, 0, sizeof(cfg_cmd));
	cfg_cmd.len = CONFIG_CND_LEN;
	cfg_cmd.cmd = CONFIG_CMD_READ_EXIT;
	if (send_cfg_cmd(cd, &cfg_cmd)) {
		ts_err("failed send config read finish command");
		ret = -EINVAL;
	}
	if (ret)
		return -EINVAL;
	return cfg_head.cfg_len + sizeof(cfg_head);
}

/*
 *	return: 0 for no error.
 *	GOODIX_EBUS when encounter a bus error
 *	GOODIX_ECHECKSUM version checksum error
 *	GOODIX_EVERSION  patch ID compare failed,
 *	in this case the sensorID is valid.
 */
static int brl_read_version(struct goodix_ts_core *cd,
			struct goodix_fw_version *version)
{
	int ret, i;
	u32 fw_addr;
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	u8 buf[sizeof(struct goodix_fw_version)] = {0};
	u8 temp_pid[8] = {0};

	if (cd->bus->ic_type == IC_TYPE_BERLIN_A)
		fw_addr = FW_VERSION_INFO_ADDR_BRA;
	else
		fw_addr = FW_VERSION_INFO_ADDR;
	//printk("GTP: FW_VERSION_INFO_ADDR \n");
	for (i = 0; i < 2; i++) {
		ret = hw_ops->read(cd, fw_addr, buf, sizeof(buf));
		if (ret) {
			ts_info("read fw version: %d, retry %d", ret, i);
			ret = -GOODIX_EBUS;
			usleep_range(5000, 5100);
			continue;
		}

		if (!checksum_cmp(buf, sizeof(buf), CHECKSUM_MODE_U8_LE))
			break;

		ts_info("invalid fw version: checksum error!");
		ts_info("fw version:%*ph", (int)sizeof(buf), buf);
		ret = -GOODIX_ECHECKSUM;
		usleep_range(10000, 11000);
	}
	if (ret) {
		ts_err("failed get valied fw version");
		return ret;
	}
	memcpy(version, buf, sizeof(*version));
	memcpy(temp_pid, version->rom_pid, sizeof(version->rom_pid));
	ts_info("rom_pid:%s", temp_pid);
	ts_info("rom_vid:%*ph", (int)sizeof(version->rom_vid),
		version->rom_vid);
	ts_info("pid:%s", version->patch_pid);
	ts_info("vid:%*ph", (int)sizeof(version->patch_vid),
		version->patch_vid);
	ts_info("sensor_id:%d", version->sensor_id);

	return 0;
}

#define LE16_TO_CPU(x)  (x = le16_to_cpu(x))
#define LE32_TO_CPU(x)  (x = le32_to_cpu(x))
static int convert_ic_info(struct goodix_ic_info *info, const u8 *data)
{
	int i;
	struct goodix_ic_info_version *version = &info->version;
	struct goodix_ic_info_feature *feature = &info->feature;
	struct goodix_ic_info_param *parm = &info->parm;
	struct goodix_ic_info_misc *misc = &info->misc;

	info->length = le16_to_cpup((__le16 *)data);

	data += 2;
	memcpy(version, data, sizeof(*version));
	version->config_id = le32_to_cpu(version->config_id);

	data += sizeof(struct goodix_ic_info_version);
	memcpy(feature, data, sizeof(*feature));
	feature->freqhop_feature =
		le16_to_cpu(feature->freqhop_feature);
	feature->calibration_feature =
		le16_to_cpu(feature->calibration_feature);
	feature->gesture_feature =
		le16_to_cpu(feature->gesture_feature);
	feature->side_touch_feature =
		le16_to_cpu(feature->side_touch_feature);
	feature->stylus_feature =
		le16_to_cpu(feature->stylus_feature);

	data += sizeof(struct goodix_ic_info_feature);
	parm->drv_num = *(data++);
	parm->sen_num = *(data++);
	parm->button_num = *(data++);
	parm->force_num = *(data++);
	parm->active_scan_rate_num = *(data++);
	if (parm->active_scan_rate_num > MAX_SCAN_RATE_NUM) {
		ts_err("invalid scan rate num %d > %d",
			parm->active_scan_rate_num, MAX_SCAN_RATE_NUM);
		return -EINVAL;
	}
	for (i = 0; i < parm->active_scan_rate_num; i++)
		parm->active_scan_rate[i] =
			le16_to_cpup((__le16 *)(data + i * 2));

	data += parm->active_scan_rate_num * 2;
	parm->mutual_freq_num = *(data++);
	if (parm->mutual_freq_num > MAX_SCAN_FREQ_NUM) {
		ts_err("invalid mntual freq num %d > %d",
			parm->mutual_freq_num, MAX_SCAN_FREQ_NUM);
		return -EINVAL;
	}
	for (i = 0; i < parm->mutual_freq_num; i++)
		parm->mutual_freq[i] =
			le16_to_cpup((__le16 *)(data + i * 2));

	data += parm->mutual_freq_num * 2;
	parm->self_tx_freq_num = *(data++);
	if (parm->self_tx_freq_num > MAX_SCAN_FREQ_NUM) {
		ts_err("invalid tx freq num %d > %d",
			parm->self_tx_freq_num, MAX_SCAN_FREQ_NUM);
		return -EINVAL;
	}
	for (i = 0; i < parm->self_tx_freq_num; i++)
		parm->self_tx_freq[i] =
			le16_to_cpup((__le16 *)(data + i * 2));

	data += parm->self_tx_freq_num * 2;
	parm->self_rx_freq_num = *(data++);
	if (parm->self_rx_freq_num > MAX_SCAN_FREQ_NUM) {
		ts_err("invalid rx freq num %d > %d",
			parm->self_rx_freq_num, MAX_SCAN_FREQ_NUM);
		return -EINVAL;
	}
	for (i = 0; i < parm->self_rx_freq_num; i++)
		parm->self_rx_freq[i] =
			le16_to_cpup((__le16 *)(data + i * 2));

	data += parm->self_rx_freq_num * 2;
	parm->stylus_freq_num = *(data++);
	if (parm->stylus_freq_num > MAX_FREQ_NUM_STYLUS) {
		ts_err("invalid stylus freq num %d > %d",
			parm->stylus_freq_num, MAX_FREQ_NUM_STYLUS);
		return -EINVAL;
	}
	for (i = 0; i < parm->stylus_freq_num; i++)
		parm->stylus_freq[i] =
			le16_to_cpup((__le16 *)(data + i * 2));

	data += parm->stylus_freq_num * 2;
	memcpy(misc, data, sizeof(*misc));
	misc->cmd_addr = le32_to_cpu(misc->cmd_addr);
	misc->cmd_max_len = le16_to_cpu(misc->cmd_max_len);
	misc->cmd_reply_addr = le32_to_cpu(misc->cmd_reply_addr);
	misc->cmd_reply_len = le16_to_cpu(misc->cmd_reply_len);
	misc->fw_state_addr = le32_to_cpu(misc->fw_state_addr);
	misc->fw_state_len = le16_to_cpu(misc->fw_state_len);
	misc->fw_buffer_addr = le32_to_cpu(misc->fw_buffer_addr);
	misc->fw_buffer_max_len = le16_to_cpu(misc->fw_buffer_max_len);
	misc->frame_data_addr = le32_to_cpu(misc->frame_data_addr);
	misc->frame_data_head_len = le16_to_cpu(misc->frame_data_head_len);

	misc->fw_attr_len = le16_to_cpu(misc->fw_attr_len);
	misc->fw_log_len = le16_to_cpu(misc->fw_log_len);
	misc->stylus_struct_len = le16_to_cpu(misc->stylus_struct_len);
	misc->mutual_struct_len = le16_to_cpu(misc->mutual_struct_len);
	misc->self_struct_len = le16_to_cpu(misc->self_struct_len);
	misc->noise_struct_len = le16_to_cpu(misc->noise_struct_len);
	misc->touch_data_addr = le32_to_cpu(misc->touch_data_addr);
	misc->touch_data_head_len = le16_to_cpu(misc->touch_data_head_len);
	misc->point_struct_len = le16_to_cpu(misc->point_struct_len);
	LE32_TO_CPU(misc->mutual_rawdata_addr);
	LE32_TO_CPU(misc->mutual_diffdata_addr);
	LE32_TO_CPU(misc->mutual_refdata_addr);
	LE32_TO_CPU(misc->self_rawdata_addr);
	LE32_TO_CPU(misc->self_diffdata_addr);
	LE32_TO_CPU(misc->self_refdata_addr);
	LE32_TO_CPU(misc->iq_rawdata_addr);
	LE32_TO_CPU(misc->iq_refdata_addr);
	LE32_TO_CPU(misc->im_rawdata_addr);
	LE16_TO_CPU(misc->im_readata_len);
	LE32_TO_CPU(misc->noise_rawdata_addr);
	LE16_TO_CPU(misc->noise_rawdata_len);
	LE32_TO_CPU(misc->stylus_rawdata_addr);
	LE16_TO_CPU(misc->stylus_rawdata_len);
	LE32_TO_CPU(misc->noise_data_addr);
	LE32_TO_CPU(misc->esd_addr);

	return 0;
}

static void print_ic_info(struct goodix_ic_info *ic_info)
{
	struct goodix_ic_info_version *version = &ic_info->version;
	struct goodix_ic_info_feature *feature = &ic_info->feature;
	struct goodix_ic_info_param *parm = &ic_info->parm;
	struct goodix_ic_info_misc *misc = &ic_info->misc;

	ts_info("ic_info_length:                %d",
		ic_info->length);
	ts_info("info_customer_id:              0x%01X",
		version->info_customer_id);
	ts_info("info_version_id:               0x%01X",
		version->info_version_id);
	ts_info("ic_die_id:                     0x%01X",
		version->ic_die_id);
	ts_info("ic_version_id:                 0x%01X",
		version->ic_version_id);
	ts_info("config_id:                     0x%4X",
		version->config_id);
	ts_info("config_version:                0x%01X",
		version->config_version);
	ts_info("frame_data_customer_id:        0x%01X",
		version->frame_data_customer_id);
	ts_info("frame_data_version_id:         0x%01X",
		version->frame_data_version_id);
	ts_info("touch_data_customer_id:        0x%01X",
		version->touch_data_customer_id);
	ts_info("touch_data_version_id:         0x%01X",
		version->touch_data_version_id);

	ts_info("freqhop_feature:               0x%04X",
		feature->freqhop_feature);
	ts_info("calibration_feature:           0x%04X",
		feature->calibration_feature);
	ts_info("gesture_feature:               0x%04X",
		feature->gesture_feature);
	ts_info("side_touch_feature:            0x%04X",
		feature->side_touch_feature);
	ts_info("stylus_feature:                0x%04X",
		feature->stylus_feature);

	ts_info("Drv*Sen,Button,Force num:      %d x %d, %d, %d",
		parm->drv_num, parm->sen_num,
		parm->button_num, parm->force_num);

	ts_info("Cmd:                           0x%04X, %d",
		misc->cmd_addr, misc->cmd_max_len);
	ts_info("Cmd-Reply:                     0x%04X, %d",
		misc->cmd_reply_addr, misc->cmd_reply_len);
	ts_info("FW-State:                      0x%04X, %d",
		misc->fw_state_addr, misc->fw_state_len);
	ts_info("FW-Buffer:                     0x%04X, %d",
		misc->fw_buffer_addr, misc->fw_buffer_max_len);
	ts_info("Touch-Data:                    0x%04X, %d",
		misc->touch_data_addr, misc->touch_data_head_len);
	ts_info("point_struct_len:              %d",
		misc->point_struct_len);
	ts_info("mutual_rawdata_addr:           0x%04X",
		misc->mutual_rawdata_addr);
	ts_info("mutual_diffdata_addr:          0x%04X",
		misc->mutual_diffdata_addr);
	ts_info("self_rawdata_addr:             0x%04X",
		misc->self_rawdata_addr);
	ts_info("self_diffdata_addr:            0x%04X",
		misc->self_diffdata_addr);
	ts_info("stylus_rawdata_addr:           0x%04X, %d",
		misc->stylus_rawdata_addr, misc->stylus_rawdata_len);
	ts_info("esd_addr:                      0x%04X",
		misc->esd_addr);
}

static int brl_get_ic_info(struct goodix_ts_core *cd,
	struct goodix_ic_info *ic_info)
{
	int ret, i;
	u16 length = 0;
	u32 ic_addr;
	u8 afe_data[GOODIX_IC_INFO_MAX_LEN] = {0};
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;

	if (cd->bus->ic_type == IC_TYPE_BERLIN_A)
		ic_addr = GOODIX_IC_INFO_ADDR_BRA;
	else
		ic_addr = GOODIX_IC_INFO_ADDR;

	for (i = 0; i < GOODIX_RETRY_3; i++) {
		ret = hw_ops->read(cd, ic_addr,
				   (u8 *)&length, sizeof(length));
		if (ret) {
			ts_info("failed get ic info length, %d", ret);
			usleep_range(5000, 5100);
			continue;
		}
		length = le16_to_cpu(length);
		if (length >= GOODIX_IC_INFO_MAX_LEN) {
			ts_info("invalid ic info length %d, retry %d",
				length, i);
			continue;
		}

		ret = hw_ops->read(cd, ic_addr, afe_data, length);
		if (ret) {
			ts_info("failed get ic info data, %d", ret);
			usleep_range(5000, 5100);
			continue;
		}
		/* judge whether the data is valid */
		if (is_risk_data((const uint8_t *)afe_data, length)) {
			ts_info("fw info data invalid");
			usleep_range(5000, 5100);
			continue;
		}
		if (checksum_cmp((const uint8_t *)afe_data,
					length, CHECKSUM_MODE_U8_LE)) {
			ts_info("fw info checksum error!");
			usleep_range(5000, 5100);
			continue;
		}
		break;
	}
	if (i == GOODIX_RETRY_3) {
		ts_err("failed get ic info");
		return -EINVAL;
	}

	ret = convert_ic_info(ic_info, afe_data);
	if (ret) {
		ts_err("convert ic info encounter error");
		return ret;
	}

	print_ic_info(ic_info);

	/* check some key info */
	if (!ic_info->misc.cmd_addr || !ic_info->misc.fw_buffer_addr ||
	    !ic_info->misc.touch_data_addr) {
		ts_err("cmd_addr fw_buf_addr and touch_data_addr is null");
		return -EINVAL;
	}

	return 0;
}

#define GOODIX_ESD_TICK_WRITE_DATA	0xAA
static int brl_esd_check(struct goodix_ts_core *cd)
{
	int ret;
	u32 esd_addr;
	u8 esd_value;

	if (!cd->ic_info.misc.esd_addr)
		return 0;

	esd_addr = cd->ic_info.misc.esd_addr;
	ret = cd->hw_ops->read(cd, esd_addr, &esd_value, 1);
	if (ret) {
		ts_err("failed get esd value, %d", ret);
		return ret;
	}

	if (esd_value == GOODIX_ESD_TICK_WRITE_DATA) {
		ts_err("esd check failed, 0x%x", esd_value);
		return -EINVAL;
	}
	esd_value = GOODIX_ESD_TICK_WRITE_DATA;
	ret = cd->hw_ops->write(cd, esd_addr, &esd_value, 1);
	if (ret) {
		ts_err("failed refrash esd value");
		return ret;
	}
	return 0;
}

#define IRQ_EVENT_HEAD_LEN			8
#define BYTES_PER_POINT				8
#define COOR_DATA_CHECKSUM_SIZE		2

#define GOODIX_TOUCH_EVENT			0x80
#define GOODIX_REQUEST_EVENT		0x40
#define GOODIX_GESTURE_EVENT		0x20
#define POINT_TYPE_STYLUS_HOVER		0x01
#define POINT_TYPE_STYLUS			0x03

static void goodix_parse_finger(struct goodix_touch_data *touch_data,
				u8 *buf, int touch_num)
{
	unsigned int id = 0, x = 0, y = 0, w = 0;
	u8 *coor_data;
	int i;

	coor_data = &buf[IRQ_EVENT_HEAD_LEN];
	for (i = 0; i < touch_num; i++) {
		id = (coor_data[0] >> 4) & 0x0F;
		if (id >= GOODIX_MAX_TOUCH) {
			ts_info("invalid finger id =%d", id);
			touch_data->touch_num = 0;
			return;
		}
		x = le16_to_cpup((__le16 *)(coor_data + 2));
		y = le16_to_cpup((__le16 *)(coor_data + 4));
		w = le16_to_cpup((__le16 *)(coor_data + 6));
		touch_data->coords[id].status = TS_TOUCH;
		touch_data->coords[id].x = x;
		touch_data->coords[id].y = y;
		touch_data->coords[id].w = w;
		coor_data += BYTES_PER_POINT;
	}
	touch_data->touch_num = touch_num;
}

static unsigned int goodix_pen_btn_code[] = {BTN_STYLUS, BTN_STYLUS2};
static void goodix_parse_pen(struct goodix_pen_data *pen_data,
	u8 *buf, int touch_num)
{
	unsigned int id = 0;
	u8 cur_key_map = 0;
	u8 *coor_data;
	int16_t x_angle, y_angle;
	int i;

	pen_data->coords.tool_type = BTN_TOOL_PEN;

	if (touch_num) {
		pen_data->coords.status = TS_TOUCH;
		coor_data = &buf[IRQ_EVENT_HEAD_LEN];

		id = (coor_data[0] >> 4) & 0x0F;
		pen_data->coords.x = le16_to_cpup((__le16 *)(coor_data + 2));
		pen_data->coords.y = le16_to_cpup((__le16 *)(coor_data + 4));
		pen_data->coords.p = le16_to_cpup((__le16 *)(coor_data + 6));
		x_angle = le16_to_cpup((__le16 *)(coor_data + 8));
		y_angle = le16_to_cpup((__le16 *)(coor_data + 10));
		pen_data->coords.tilt_x = x_angle / 100;
		pen_data->coords.tilt_y = y_angle / 100;
	} else {
		pen_data->coords.status = TS_RELEASE;
	}

	cur_key_map = (buf[3] & 0x0F) >> 1;
	for (i = 0; i < GOODIX_MAX_PEN_KEY; i++) {
		pen_data->keys[i].code = goodix_pen_btn_code[i];
		if (!(cur_key_map & (1 << i)))
			continue;
		pen_data->keys[i].status = TS_TOUCH;
	}
}

static int goodix_touch_handler(struct goodix_ts_core *cd,
				struct goodix_ts_event *ts_event,
				u8 *pre_buf, u32 pre_buf_len)
{
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
	struct goodix_touch_data *touch_data = &ts_event->touch_data;
	struct goodix_pen_data *pen_data = &ts_event->pen_data;
	static u8 buffer[IRQ_EVENT_HEAD_LEN +
			 BYTES_PER_POINT * GOODIX_MAX_TOUCH + 2];
	u8 touch_num = 0;
	int ret = 0;
	u8 point_type = 0;
	static u8 pre_finger_num;
	static u8 pre_pen_num;

	/* clean event buffer */
	memset(ts_event, 0, sizeof(*ts_event));
	/* copy pre-data to buffer */
	memcpy(buffer, pre_buf, pre_buf_len);

	touch_num = buffer[2] & 0x0F;

	if (touch_num > GOODIX_MAX_TOUCH) {
		ts_debug("invalid touch num %d", touch_num);
		return -EINVAL;
	}

	if (unlikely(touch_num > 2)) {
		ret = hw_ops->read(cd,
				misc->touch_data_addr + pre_buf_len,
				&buffer[pre_buf_len],
				(touch_num - 2) * BYTES_PER_POINT);
		if (ret) {
			ts_debug("failed get touch data");
			return ret;
		}
	}

	/* read done */
	hw_ops->after_event_handler(cd);

	if (touch_num > 0) {
		point_type = buffer[IRQ_EVENT_HEAD_LEN] & 0x0F;
		if (point_type == POINT_TYPE_STYLUS ||
				point_type == POINT_TYPE_STYLUS_HOVER) {
			ret = checksum_cmp(&buffer[IRQ_EVENT_HEAD_LEN],
					BYTES_PER_POINT * 2 + 2,
					CHECKSUM_MODE_U8_LE);
			if (ret) {
				ts_debug("touch data checksum error");
				ts_debug("data:%*ph", BYTES_PER_POINT * 2 + 2,
						&buffer[IRQ_EVENT_HEAD_LEN]);
				return -EINVAL;
			}
		} else {
			ret = checksum_cmp(&buffer[IRQ_EVENT_HEAD_LEN],
					touch_num * BYTES_PER_POINT + 2,
					CHECKSUM_MODE_U8_LE);
			if (ret) {
				ts_debug("touch data checksum error");
				ts_debug("data:%*ph",
						touch_num * BYTES_PER_POINT + 2,
						&buffer[IRQ_EVENT_HEAD_LEN]);
				return -EINVAL;
			}
		}
	}

	if (touch_num > 0 && (point_type == POINT_TYPE_STYLUS
				|| point_type == POINT_TYPE_STYLUS_HOVER)) {
		/* stylus info */
		if (pre_finger_num) {
			ts_event->event_type = EVENT_TOUCH;
			goodix_parse_finger(touch_data, buffer, 0);
			pre_finger_num = 0;
		} else {
			pre_pen_num = 1;
			ts_event->event_type = EVENT_PEN;
			goodix_parse_pen(pen_data, buffer, touch_num);
		}
	} else {
		/* finger info */
		if (pre_pen_num) {
			ts_event->event_type = EVENT_PEN;
			goodix_parse_pen(pen_data, buffer, 0);
			pre_pen_num = 0;
		} else {
			ts_event->event_type = EVENT_TOUCH;
			goodix_parse_finger(touch_data,
					    buffer, touch_num);
			pre_finger_num = touch_num;
		}
	}

	/* process custom info */
	if (buffer[3] & 0x01)
		ts_debug("TODO add custom info process function");

	return 0;
}

static int brl_event_handler(struct goodix_ts_core *cd,
			 struct goodix_ts_event *ts_event)
{
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
	int pre_read_len;
	u8 pre_buf[32];
	u8 event_status;
	int ret;

	pre_read_len = IRQ_EVENT_HEAD_LEN +
		BYTES_PER_POINT * 2 + COOR_DATA_CHECKSUM_SIZE;
	ret = hw_ops->read(cd, misc->touch_data_addr,
			   pre_buf, pre_read_len);
	if (ret) {
		ts_debug("failed get event head data");
		return ret;
	}

	if (pre_buf[0] == 0x00) {
		ts_debug("invalid touch head");
		return -EINVAL;
	}

	if (checksum_cmp(pre_buf, IRQ_EVENT_HEAD_LEN, CHECKSUM_MODE_U8_LE)) {
		ts_debug("touch head checksum err[%*ph]",
				IRQ_EVENT_HEAD_LEN, pre_buf);
		return -EINVAL;
	}

	event_status = pre_buf[0];
	if (event_status & GOODIX_TOUCH_EVENT)
		return goodix_touch_handler(cd, ts_event,
					    pre_buf, pre_read_len);

	if (event_status & GOODIX_REQUEST_EVENT) {
		ts_event->event_type = EVENT_REQUEST;
		if (pre_buf[2] == BRL_REQUEST_CODE_CONFIG)
			ts_event->request_code = REQUEST_TYPE_CONFIG;
		else if (pre_buf[2] == BRL_REQUEST_CODE_RESET)
			ts_event->request_code = REQUEST_TYPE_RESET;
		else
			ts_debug("unsupported request code 0x%x", pre_buf[2]);
	}

	if (event_status & GOODIX_GESTURE_EVENT) {
		ts_event->event_type = EVENT_GESTURE;
		ts_event->gesture_type = pre_buf[4];
		memcpy(ts_event->gesture_data, &pre_buf[8],
				GOODIX_GESTURE_DATA_LEN);
	}
	/* read done */
	hw_ops->after_event_handler(cd);

	return 0;
}

static int brl_after_event_handler(struct goodix_ts_core *cd)
{
	struct goodix_ts_hw_ops *hw_ops = cd->hw_ops;
	struct goodix_ic_info_misc *misc = &cd->ic_info.misc;
	u8 sync_clean = 0;

	if (cd->tools_ctrl_sync)
		return 0;
	return hw_ops->write(cd, misc->touch_data_addr,
		&sync_clean, 1);
}

static int brld_get_framedata(struct goodix_ts_core *cd,
		struct ts_rawdata_info *info)
{
	int ret;
	unsigned char val;
	int retry = 20;
	struct frame_head *frame_head;
	unsigned char frame_buf[GOODIX_MAX_FRAMEDATA_LEN];
	unsigned char *cur_ptr;
	unsigned int flag_addr = cd->ic_info.misc.frame_data_addr;

	/* clean touch event flag */
	val = 0;
	ret = brl_write(cd, flag_addr, &val, 1);
	if (ret < 0) {
		ts_err("clean touch event failed, exit!");
		return ret;
	}

	while (retry--) {
		usleep_range(2000, 2100);
		ret = brl_read(cd, flag_addr, &val, 1);
		if (!ret && (val & GOODIX_TOUCH_EVENT))
			break;
	}
	if (retry < 0) {
		ts_err("framedata is not ready val:0x%02x, exit!", val);
		return -EINVAL;
	}

	ret = brl_read(cd, flag_addr, frame_buf, GOODIX_MAX_FRAMEDATA_LEN);
	if (ret < 0) {
		ts_err("read frame data failed");
		return ret;
	}

	if (checksum_cmp(frame_buf, cd->ic_info.misc.frame_data_head_len,
			CHECKSUM_MODE_U8_LE)) {
		ts_err("frame head checksum error");
		return -EINVAL;
	}

	frame_head = (struct frame_head *)frame_buf;
	if (checksum_cmp(frame_buf, frame_head->cur_frame_len,
			CHECKSUM_MODE_U16_LE)) {
		ts_err("frame body checksum error");
		return -EINVAL;
	}
	cur_ptr = frame_buf;
	cur_ptr += cd->ic_info.misc.frame_data_head_len;
	cur_ptr += cd->ic_info.misc.fw_attr_len;
	cur_ptr += cd->ic_info.misc.fw_log_len;
	memcpy((u8 *)(info->buff + info->used_size), cur_ptr + 8,
			cd->ic_info.misc.mutual_struct_len - 8);

	return 0;
}

static int brld_get_cap_data(struct goodix_ts_core *cd,
		struct ts_rawdata_info *info)
{
	struct goodix_ts_cmd temp_cmd;
	int tx = cd->ic_info.parm.drv_num;
	int rx = cd->ic_info.parm.sen_num;
	int size = tx * rx;
	int ret;

	/* disable irq & close esd */
	brl_irq_enbale(cd, false);
	goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);

	info->buff[0] = rx;
	info->buff[1] = tx;
	info->used_size = 2;

	temp_cmd.cmd = 0x90;
	temp_cmd.data[0] = 0x81;
	temp_cmd.len = 5;
	ret = brl_send_cmd(cd, &temp_cmd);
	if (ret < 0) {
		ts_err("report rawdata failed, exit!");
		goto exit;
	}

	ret = brld_get_framedata(cd, info);
	if (ret < 0) {
		ts_err("brld get rawdata failed");
		goto exit;
	}
	goodix_rotate_abcd2cbad(tx, rx, &info->buff[info->used_size]);
	info->used_size += size;

	temp_cmd.cmd = 0x90;
	temp_cmd.data[0] = 0x82;
	temp_cmd.len = 5;
	ret = brl_send_cmd(cd, &temp_cmd);
	if (ret < 0) {
		ts_err("report diffdata failed, exit!");
		goto exit;
	}

	ret = brld_get_framedata(cd, info);
	if (ret < 0) {
		ts_err("brld get diffdata failed");
		goto exit;
	}
	goodix_rotate_abcd2cbad(tx, rx, &info->buff[info->used_size]);
	info->used_size += size;

exit:
	temp_cmd.cmd = 0x90;
	temp_cmd.data[0] = 0;
	temp_cmd.len = 5;
	brl_send_cmd(cd, &temp_cmd);
	/* enable irq & esd */
	brl_irq_enbale(cd, true);
	goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);
	return ret;
}

#define GOODIX_CMD_RAWDATA	2
#define GOODIX_CMD_COORD	0
static int brl_get_capacitance_data(struct goodix_ts_core *cd,
		struct ts_rawdata_info *info)
{
	int ret;
	int retry = 20;
	struct goodix_ts_cmd temp_cmd;
	u32 flag_addr = cd->ic_info.misc.touch_data_addr;
	u32 raw_addr = cd->ic_info.misc.mutual_rawdata_addr;
	u32 diff_addr = cd->ic_info.misc.mutual_diffdata_addr;
	int tx = cd->ic_info.parm.drv_num;
	int rx = cd->ic_info.parm.sen_num;
	int size = tx * rx;
	u8 val;

	if (!info) {
		ts_err("input null ptr");
		return -EIO;
	}

	if (cd->bus->ic_type == IC_TYPE_BERLIN_D)
		return brld_get_cap_data(cd, info);

	/* disable irq & close esd */
	brl_irq_enbale(cd, false);
	goodix_ts_blocking_notify(NOTIFY_ESD_OFF, NULL);

    /* switch rawdata mode */
	temp_cmd.cmd = GOODIX_CMD_RAWDATA;
	temp_cmd.len = 4;
	ret = brl_send_cmd(cd, &temp_cmd);
	if (ret < 0) {
		ts_err("switch rawdata mode failed, exit!");
		goto exit;
	}

	/* clean touch event flag */
	val = 0;
	ret = brl_write(cd, flag_addr, &val, 1);
	if (ret < 0) {
		ts_err("clean touch event failed, exit!");
		goto exit;
	}

	while (retry--) {
		usleep_range(5000, 5100);
		ret = brl_read(cd, flag_addr, &val, 1);
		if (!ret && (val & GOODIX_TOUCH_EVENT))
			break;
	}
	if (retry < 0) {
		ts_err("rawdata is not ready val:0x%02x, exit!", val);
		goto exit;
	}

	/* obtain rawdata & diff_rawdata */
	info->buff[0] = rx;
	info->buff[1] = tx;
	info->used_size = 2;

	ret = brl_read(cd, raw_addr, (u8 *)&info->buff[info->used_size],
			size * sizeof(s16));
	if (ret < 0) {
		ts_err("obtian raw_data failed, exit!");
		goto exit;
	}
	goodix_rotate_abcd2cbad(tx, rx, &info->buff[info->used_size]);
	info->used_size += size;

	ret = brl_read(cd, diff_addr, (u8 *)&info->buff[info->used_size],
			size * sizeof(s16));
	if (ret < 0) {
		ts_err("obtian diff_data failed, exit!");
		goto exit;
	}
	goodix_rotate_abcd2cbad(tx, rx, &info->buff[info->used_size]);
	info->used_size += size;

exit:
	/* switch coor mode */
	temp_cmd.cmd = GOODIX_CMD_COORD;
	temp_cmd.len = 4;
	brl_send_cmd(cd, &temp_cmd);
	/* clean touch event flag */
	val = 0;
	brl_write(cd, flag_addr, &val, 1);
	/* enable irq & esd */
	brl_irq_enbale(cd, true);
	goodix_ts_blocking_notify(NOTIFY_ESD_ON, NULL);
	return ret;
}

static struct goodix_ts_hw_ops brl_hw_ops = {
	.power_on = brl_power_on,
	.resume = brl_resume,
	.suspend = brl_suspend,
	.gesture = brl_gesture,
	.reset = brl_reset,
	.irq_enable = brl_irq_enbale,
	.read = brl_read,
	.write = brl_write,
	.send_cmd = brl_send_cmd,
	.send_config = brl_send_config,
	.read_config = brl_read_config,
	.read_version = brl_read_version,
	.get_ic_info = brl_get_ic_info,
	.esd_check = brl_esd_check,
	.event_handler = brl_event_handler,
	.after_event_handler = brl_after_event_handler,
	.get_capacitance_data = brl_get_capacitance_data,
};

struct goodix_ts_hw_ops *goodix_get_hw_ops(void)
{
	return &brl_hw_ops;
}
