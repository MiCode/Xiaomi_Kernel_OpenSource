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

#define TS_BIN_VERSION_START_INDEX		5
#define TS_BIN_VERSION_LEN				4
#define TS_CFG_BIN_HEAD_RESERVED_LEN	6
#define TS_CFG_OFFSET_LEN				2
#define TS_IC_TYPE_NAME_MAX_LEN			15
#define TS_CFG_BIN_HEAD_LEN \
		(sizeof(struct goodix_cfg_bin_head) + \
		TS_CFG_BIN_HEAD_RESERVED_LEN)
#define TS_PKG_CONST_INFO_LEN \
		(sizeof(struct goodix_cfg_pkg_const_info))
#define TS_PKG_REG_INFO_LEN	\
		(sizeof(struct goodix_cfg_pkg_reg_info))
#define TS_PKG_HEAD_LEN \
		(TS_PKG_CONST_INFO_LEN + TS_PKG_REG_INFO_LEN)

/*cfg block definitin*/
#define TS_CFG_BLOCK_PID_LEN		8
#define TS_CFG_BLOCK_VID_LEN		8
#define TS_CFG_BLOCK_FW_MASK_LEN	9
#define TS_CFG_BLOCK_FW_PATCH_LEN	4
#define TS_CFG_BLOCK_RESERVED_LEN	9

#define TS_NORMAL_CFG				0x01
#define TS_HIGH_SENSE_CFG			0x03
#define TS_RQST_FW_RETRY_TIMES		2

#pragma pack(1)
struct goodix_cfg_pkg_reg {
	u16 addr;
	u8 reserved1;
	u8 reserved2;
};

struct goodix_cfg_pkg_const_info {
	u32 pkg_len;
	u8 ic_type[TS_IC_TYPE_NAME_MAX_LEN];
	u8 cfg_type;
	u8 sensor_id;
	u8 hw_pid[TS_CFG_BLOCK_PID_LEN];
	u8 hw_vid[TS_CFG_BLOCK_VID_LEN];
	u8 fw_mask[TS_CFG_BLOCK_FW_MASK_LEN];
	u8 fw_patch[TS_CFG_BLOCK_FW_PATCH_LEN];
	u16 x_res_offset;
	u16 y_res_offset;
	u16 trigger_offset;
};

struct goodix_cfg_pkg_reg_info {
	struct goodix_cfg_pkg_reg cfg_send_flag;
	struct goodix_cfg_pkg_reg version_base;
	struct goodix_cfg_pkg_reg pid;
	struct goodix_cfg_pkg_reg vid;
	struct goodix_cfg_pkg_reg sensor_id;
	struct goodix_cfg_pkg_reg fw_mask;
	struct goodix_cfg_pkg_reg fw_status;
	struct goodix_cfg_pkg_reg cfg_addr;
	struct goodix_cfg_pkg_reg esd;
	struct goodix_cfg_pkg_reg command;
	struct goodix_cfg_pkg_reg coor;
	struct goodix_cfg_pkg_reg gesture;
	struct goodix_cfg_pkg_reg fw_request;
	struct goodix_cfg_pkg_reg proximity;
	u8 reserved[TS_CFG_BLOCK_RESERVED_LEN];
};

struct goodix_cfg_bin_head {
	u32 bin_len;
	u8 checksum;
	u8 bin_version[TS_BIN_VERSION_LEN];
	u8 pkg_num;
};

#pragma pack()

struct goodix_cfg_package {
	struct goodix_cfg_pkg_const_info cnst_info;
	struct goodix_cfg_pkg_reg_info reg_info;
	const u8 *cfg;
	u32 pkg_len;
};

struct goodix_cfg_bin {
	unsigned char *bin_data;
	unsigned int bin_data_len;
	struct goodix_cfg_bin_head head;
	struct goodix_cfg_package *cfg_pkgs;
};

static int goodix_read_cfg_bin(struct device *dev, const char *cfg_name,
			struct goodix_cfg_bin *cfg_bin)
{
	const struct firmware *firmware = NULL;
	int ret;
	int retry = GOODIX_RETRY_3;

	ts_info("cfg_bin_name:%s", cfg_name);

	while (retry--) {
		ret = request_firmware(&firmware, cfg_name, dev);
		if (!ret)
			break;
		ts_info("get cfg bin retry:[%d]", GOODIX_RETRY_3 - retry);
		msleep(200);
	}
	if (retry < 0) {
		ts_err("failed get cfg bin[%s] error:%d", cfg_name, ret);
		return ret;
	}

	if (firmware->size <= 0) {
		ts_err("request_firmware, cfg_bin length ERROR,len:%zu",
			firmware->size);
		ret = -EINVAL;
		goto exit;
	}

	cfg_bin->bin_data_len = firmware->size;
	/* allocate memory for cfg_bin->bin_data */
	cfg_bin->bin_data = kzalloc(cfg_bin->bin_data_len, GFP_KERNEL);
	if (!cfg_bin->bin_data) {
		ret = -ENOMEM;
		goto exit;
	}
	memcpy(cfg_bin->bin_data, firmware->data, cfg_bin->bin_data_len);

exit:
	release_firmware(firmware);
	return ret;
}

static int goodix_parse_cfg_bin(struct goodix_cfg_bin *cfg_bin)
{
	u16 offset1, offset2;
	u8 checksum;
	int i;

	/* copy cfg_bin head info */
	if (cfg_bin->bin_data_len < sizeof(struct goodix_cfg_bin_head)) {
		ts_err("Invalid cfg_bin size:%d", cfg_bin->bin_data_len);
		return -EINVAL;
	}

	memcpy(&cfg_bin->head, cfg_bin->bin_data,
		sizeof(struct goodix_cfg_bin_head));
	cfg_bin->head.bin_len = le32_to_cpu(cfg_bin->head.bin_len);

	/*check length*/
	if (cfg_bin->bin_data_len != cfg_bin->head.bin_len) {
		ts_err("cfg_bin len check failed,%d != %d",
			cfg_bin->head.bin_len, cfg_bin->bin_data_len);
		return -EINVAL;
	}

	/*check cfg_bin valid*/
	checksum = 0;
	for (i = TS_BIN_VERSION_START_INDEX; i < cfg_bin->bin_data_len; i++)
		checksum += cfg_bin->bin_data[i];

	if (checksum != cfg_bin->head.checksum) {
		ts_err("cfg_bin checksum check filed 0x%02x != 0x%02x",
			cfg_bin->head.checksum, checksum);
		return -EINVAL;
	}

	/*allocate memory for cfg packages*/
	cfg_bin->cfg_pkgs = kzalloc(sizeof(struct goodix_cfg_package) *
					cfg_bin->head.pkg_num, GFP_KERNEL);
	if (!cfg_bin->cfg_pkgs)
		return -ENOMEM;

	/*get cfg_pkg's info*/
	for (i = 0; i < cfg_bin->head.pkg_num; i++) {
		/*get cfg pkg length*/
		if (i == cfg_bin->head.pkg_num - 1) {
			offset1 = cfg_bin->bin_data[TS_CFG_BIN_HEAD_LEN +
					i * TS_CFG_OFFSET_LEN] +
					(cfg_bin->bin_data[TS_CFG_BIN_HEAD_LEN +
					i * TS_CFG_OFFSET_LEN + 1] << 8);

			cfg_bin->cfg_pkgs[i].pkg_len =
					cfg_bin->bin_data_len - offset1;
		} else {
			offset1 = cfg_bin->bin_data[TS_CFG_BIN_HEAD_LEN +
					i * TS_CFG_OFFSET_LEN] +
					(cfg_bin->bin_data[TS_CFG_BIN_HEAD_LEN +
					i * TS_CFG_OFFSET_LEN + 1] << 8);

			offset2 = cfg_bin->bin_data[TS_CFG_BIN_HEAD_LEN +
					i * TS_CFG_OFFSET_LEN + 2] +
					(cfg_bin->bin_data[TS_CFG_BIN_HEAD_LEN +
					i * TS_CFG_OFFSET_LEN + 3] << 8);

			if (offset2 <= offset1) {
				ts_err("offset error,pkg:%d, offset1:%d, offset2:%d",
						i, offset1, offset2);
				goto exit;
			}

			cfg_bin->cfg_pkgs[i].pkg_len = offset2 - offset1;
		}
		/*get cfg pkg head*/
		memcpy(&cfg_bin->cfg_pkgs[i].cnst_info,
			&cfg_bin->bin_data[offset1], TS_PKG_CONST_INFO_LEN);
		memcpy(&cfg_bin->cfg_pkgs[i].reg_info,
			&cfg_bin->bin_data[offset1 + TS_PKG_CONST_INFO_LEN],
			TS_PKG_REG_INFO_LEN);

		/*get configuration data*/
		cfg_bin->cfg_pkgs[i].cfg =
				&cfg_bin->bin_data[offset1 + TS_PKG_HEAD_LEN];
	}

	/*debug, print pkg information*/
	ts_info("Driver bin info: ver %s, len %d, pkgs %d",
			cfg_bin->head.bin_version,
			cfg_bin->head.bin_len,
			cfg_bin->head.pkg_num);

	return 0;
exit:
	kfree(cfg_bin->cfg_pkgs);
	return -EINVAL;
}

static int goodix_get_reg_and_cfg(struct goodix_ts_core *cd, u8 sensor_id,
			struct goodix_cfg_bin *cfg_bin)
{
	int i;
	u8 cfg_type;
	u32 cfg_len;
	struct goodix_cfg_package *cfg_pkg;

	if (!cfg_bin->head.pkg_num || !cfg_bin->cfg_pkgs) {
		ts_err("there is none cfg package, pkg_num:%d",
			cfg_bin->head.pkg_num);
		return -EINVAL;
	}

	/* find cfg packages with same sensor_id */
	for (i = 0; i < cfg_bin->head.pkg_num; i++) {
		cfg_pkg = &cfg_bin->cfg_pkgs[i];
		if (sensor_id != cfg_pkg->cnst_info.sensor_id) {
			ts_info("pkg:%d, sensor id contrast FAILED, bin %d != %d",
				i, cfg_pkg->cnst_info.sensor_id, sensor_id);
			continue;
		}
		cfg_type = cfg_pkg->cnst_info.cfg_type;
		if (cfg_type >= GOODIX_MAX_CONFIG_GROUP) {
			ts_err("usupported config type %d",
				cfg_pkg->cnst_info.cfg_type);
			goto err_out;
		}

		cfg_len = cfg_pkg->pkg_len - TS_PKG_CONST_INFO_LEN -
				TS_PKG_REG_INFO_LEN;
		if (cfg_len > GOODIX_CFG_MAX_SIZE) {
			ts_err("config len exceed limit %d > %d",
				cfg_len, GOODIX_CFG_MAX_SIZE);
			goto err_out;
		}
		if (cd->ic_configs[cfg_type]) {
			ts_err("found same type config twice for sensor id %d, skiped",
				sensor_id);
			continue;
		}
		cd->ic_configs[cfg_type] =
				kzalloc(sizeof(struct goodix_ic_config),
				GFP_KERNEL);
		if (!cd->ic_configs[cfg_type])
			goto err_out;
		cd->ic_configs[cfg_type]->len = cfg_len;
		memcpy(cd->ic_configs[cfg_type]->data, cfg_pkg->cfg, cfg_len);
		ts_info("get config type %d, len %d, for sensor id %d",
			cfg_type, cfg_len, sensor_id);
	}
	return 0;

err_out:
	/* parse config enter error, release memory alloced */
	for (i = 0; i < GOODIX_MAX_CONFIG_GROUP; i++) {
		kfree(cd->ic_configs[i]);
		cd->ic_configs[i] = NULL;
	}
	return -EINVAL;
}

static int goodix_get_config_data(struct goodix_ts_core *cd, u8 sensor_id)
{
	struct goodix_cfg_bin cfg_bin = {0};
	char *cfg_name = cd->board_data.cfg_bin_name;
	int ret;

	/*get cfg_bin from file system*/
	ret = goodix_read_cfg_bin(&cd->pdev->dev, cfg_name, &cfg_bin);
	if (ret) {
		ts_err("failed get valid config bin data");
		return ret;
	}

	/*parse cfg bin*/
	ret = goodix_parse_cfg_bin(&cfg_bin);
	if (ret) {
		ts_err("failed parse cfg bin");
		goto err_out;
	}

	/*get register address and configuration from cfg bin*/
	ret = goodix_get_reg_and_cfg(cd, sensor_id, &cfg_bin);
	if (!ret)
		ts_info("success get reg and cfg info from cfg bin");
	else
		ts_err("failed get cfg and reg info, update fw then retry");

	kfree(cfg_bin.cfg_pkgs);
err_out:
	kfree(cfg_bin.bin_data);
	return ret;
}

int goodix_get_config_proc(struct goodix_ts_core *cd)
{
	return goodix_get_config_data(cd, cd->fw_version.sensor_id);
}


