#include "goodix_cfg_bin.h"

extern struct goodix_module goodix_modules;
int goodix_ts_stage2_init(struct goodix_ts_core *core_data);
int goodix_ts_core_release(struct goodix_ts_core *core_data);

static int goodix_parse_cfg_bin(struct goodix_cfg_bin *cfg_bin);
static int goodix_get_reg_and_cfg(struct goodix_ts_device *ts_dev,
				struct goodix_cfg_bin *cfg_bin);
static int goodix_read_cfg_bin(struct device *dev,
				struct goodix_cfg_bin *cfg_bin);
static void goodix_cfg_pkg_leToCpu(struct goodix_cfg_package *pkg);
int goodix_get_lockdowninfo(struct goodix_ts_core *ts_core)
{
	int ret = 0;
	struct goodix_ts_device *ts_dev = ts_core->ts_dev;

	ret = ts_dev->hw_ops->read(ts_dev, TS_LOCKDOWN_REG,
				ts_core->lockdown_info, GOODIX_LOCKDOWN_SIZE);
	if (ret) {
		ts_err("can't get lockdown");
		return -EINVAL;
	}

	ts_info("lockdown is:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x",
		     ts_core->lockdown_info[0], ts_core->lockdown_info[1],
		     ts_core->lockdown_info[2], ts_core->lockdown_info[3],
		     ts_core->lockdown_info[4], ts_core->lockdown_info[5],
		     ts_core->lockdown_info[6], ts_core->lockdown_info[7]);
	return 0;
}

static int goodix_parse_cfg_bin(struct goodix_cfg_bin *cfg_bin)
{
	u8 checksum;
	int i, r;
	u16 offset1, offset2;

	if (!cfg_bin->bin_data || cfg_bin->bin_data_len == 0) {
		ts_err("NO cfg_bin data, cfg_bin data length:%d",
			cfg_bin->bin_data_len);
		r = -EINVAL;
		goto exit;
	}

	/* copy cfg_bin head info */
	if (cfg_bin->bin_data_len < sizeof(struct goodix_cfg_bin_head)) {
		ts_err("Invalid cfg_bin size:%d", cfg_bin->bin_data_len);
		r = -EINVAL;
		goto exit;
	}
	memcpy(&cfg_bin->head, cfg_bin->bin_data,
		sizeof(struct goodix_cfg_bin_head));
	cfg_bin->head.bin_len = le32_to_cpu(cfg_bin->head.bin_len);

	/*check length*/
	if (cfg_bin->bin_data_len != cfg_bin->head.bin_len) {
		ts_err("cfg_bin len check failed,%d != %d",
			cfg_bin->head.bin_len, cfg_bin->bin_data_len);
		r = -EINVAL;
		goto exit;
	}

	/*check cfg_bin valid*/
	checksum = 0;
	for (i = TS_BIN_VERSION_START_INDEX; i < cfg_bin->bin_data_len; i++) {
		checksum += cfg_bin->bin_data[i];
	}
	if (checksum != cfg_bin->head.checksum) {
		ts_err("cfg_bin checksum check filed 0x%02x != 0x%02x",
			cfg_bin->head.checksum, checksum);
		r = -EINVAL;
		goto exit;
	}

	/*allocate memory for cfg packages*/
	cfg_bin->cfg_pkgs = kzalloc(sizeof(struct goodix_cfg_package) *
					cfg_bin->head.pkg_num, GFP_KERNEL);
	if (!cfg_bin->cfg_pkgs) {
		ts_err("cfg_pkgs, allocate memory ERROR");
		r = -ENOMEM;
		goto exit;
	}

	/*get cfg_pkg's info*/
	for (i = 0; i < cfg_bin->head.pkg_num; i++) {
		/*get cfg pkg length*/
		if (i == cfg_bin->head.pkg_num - 1) {
			offset1 = cfg_bin->bin_data[TS_CFG_BIN_HEAD_LEN + i * TS_CFG_OFFSET_LEN] +
				(cfg_bin->bin_data[TS_CFG_BIN_HEAD_LEN + i * TS_CFG_OFFSET_LEN + 1] << 8);

			cfg_bin->cfg_pkgs[i].pkg_len = cfg_bin->bin_data_len - offset1;
		} else {
			offset1 = cfg_bin->bin_data[TS_CFG_BIN_HEAD_LEN + i * TS_CFG_OFFSET_LEN] +
				(cfg_bin->bin_data[TS_CFG_BIN_HEAD_LEN + i * TS_CFG_OFFSET_LEN + 1] << 8);

			offset2 = cfg_bin->bin_data[TS_CFG_BIN_HEAD_LEN + i * TS_CFG_OFFSET_LEN + 2] +
				(cfg_bin->bin_data[TS_CFG_BIN_HEAD_LEN + i * TS_CFG_OFFSET_LEN + 3] << 8);

			if (offset2 <= offset1) {
				ts_err("offset error,pkg:%d, offset1:%d, offset2:%d", i, offset1, offset2);
				r = -EINVAL;
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
		/*compatible little edition and big edition*/
		goodix_cfg_pkg_leToCpu(&cfg_bin->cfg_pkgs[i]);

		/*get configuration data*/
		cfg_bin->cfg_pkgs[i].cfg = &cfg_bin->bin_data[offset1 + TS_PKG_HEAD_LEN];
	}

	/*debug, print pkg information*/
	ts_info("Driver bin info: ver %s, len %d, pkgs %d", cfg_bin->head.bin_version,
		cfg_bin->head.bin_len, cfg_bin->head.pkg_num);
	for (i = 0; i < cfg_bin->head.pkg_num; i++) {
		ts_debug("---------------------------------------------");
		ts_debug("------package:%d------", i + 1);
		ts_debug("package len:%04x", cfg_bin->cfg_pkgs[i].cnst_info.pkg_len);
		ts_debug("package ic_type:%s", cfg_bin->cfg_pkgs[i].cnst_info.ic_type);
		ts_debug("package cfg_type:%01x", cfg_bin->cfg_pkgs[i].cnst_info.cfg_type);
		ts_debug("package sensor_id:%01x", cfg_bin->cfg_pkgs[i].cnst_info.sensor_id);
		ts_debug("package hw_pid:%s", cfg_bin->cfg_pkgs[i].cnst_info.hw_pid);
		ts_debug("package hw_vid:%s", cfg_bin->cfg_pkgs[i].cnst_info.hw_vid);
		ts_debug("package fw_mask_version:%s", cfg_bin->cfg_pkgs[i].cnst_info.fw_mask);
		ts_debug("package fw_patch_version:%s", cfg_bin->cfg_pkgs[i].cnst_info.fw_patch);
		ts_debug("package x_res_offset:%02x", cfg_bin->cfg_pkgs[i].cnst_info.x_res_offset);
		ts_debug("package y_res_offset:%02x", cfg_bin->cfg_pkgs[i].cnst_info.y_res_offset);
		ts_debug("package trigger_offset:%02x", cfg_bin->cfg_pkgs[i].cnst_info.trigger_offset);

		ts_debug("reg info");
		ts_debug("send_cfg_flag reg:%02x", cfg_bin->cfg_pkgs[i].reg_info.cfg_send_flag.addr);
		ts_debug("version base reg:%02x, len:%d",
				cfg_bin->cfg_pkgs[i].reg_info.version_base.addr,
				cfg_bin->cfg_pkgs[i].reg_info.version_base.reserved1);
		ts_debug("pid reg:%02x:%d", cfg_bin->cfg_pkgs[i].reg_info.pid.addr,
			cfg_bin->cfg_pkgs[i].reg_info.pid.reserved1);
		ts_debug("vid reg:%02x:%d", cfg_bin->cfg_pkgs[i].reg_info.vid.addr,
			cfg_bin->cfg_pkgs[i].reg_info.vid.reserved1);
		ts_debug("sensor_id reg:%02x,mask:%x", cfg_bin->cfg_pkgs[i].reg_info.sensor_id.addr,
			cfg_bin->cfg_pkgs[i].reg_info.sensor_id.addr);
		ts_debug("fw_status reg:%02x", cfg_bin->cfg_pkgs[i].reg_info.fw_status.addr);
		ts_debug("cfg_addr reg:%02x", cfg_bin->cfg_pkgs[i].reg_info.cfg_addr.addr);
		ts_debug("esd reg:%02x", cfg_bin->cfg_pkgs[i].reg_info.esd.addr);
		ts_debug("command reg:%02x", cfg_bin->cfg_pkgs[i].reg_info.command.addr);
		ts_debug("coor:%02x", cfg_bin->cfg_pkgs[i].reg_info.coor.addr);
		ts_debug("gesture:%02x", cfg_bin->cfg_pkgs[i].reg_info.gesture.addr);
		ts_debug("fw_request:%02x", cfg_bin->cfg_pkgs[i].reg_info.fw_request.addr);
		ts_debug("proximity:%02x", cfg_bin->cfg_pkgs[i].reg_info.proximity.addr);

		ts_debug("--------------------------------------------");
	}
	r = 0;
exit:
	return r;
}

static int goodix_cfg_bin_proc(struct goodix_ts_core *core_data)
{
	int r;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	struct goodix_cfg_bin *cfg_bin = kzalloc(sizeof(struct goodix_cfg_bin),
						 GFP_KERNEL);
	if (!cfg_bin) {
		return -ENOMEM;
	}
	if (!ts_dev) {
		ts_err("ts device can't be null");
		return -EINVAL;
	}

	/* when start init config bin with error state */
	ts_dev->cfg_bin_state = CFG_BIN_STATE_ERROR;

	/*get cfg_bin from file system*/
	r = goodix_read_cfg_bin(ts_dev->dev, cfg_bin);
	if (r < 0) {
		ts_err("Failed get valid config bin data");
		goto exit;
	}
	/*parse cfg bin*/
	r = goodix_parse_cfg_bin(cfg_bin);
	if (!r) {
		ts_info("parse cfg bin SUCCESS");
	} else {
		ts_err("parse cfg bin FAILED");
		goto exit;
	}

	/*get register address and configuration from cfg bin*/
	r = goodix_get_reg_and_cfg(ts_dev, cfg_bin);
	if (!r) {
		ts_info("success get reg and cfg info from cfg bin");
	} else {
		ts_err("failed get cfg and reg info, update fw then retry");
	}

	/*debug*/
	ts_info("cfg_send_flag:0x%04x", ts_dev->reg.cfg_send_flag);
	ts_info("pid:0x%04x", ts_dev->reg.pid);
	ts_info("vid:0x%04x", ts_dev->reg.vid);
	ts_info("sensor_id:0x%04x", ts_dev->reg.sensor_id);
	ts_info("fw_mask:0x%04x", ts_dev->reg.fw_mask);
	ts_info("fw_status:0x%04x", ts_dev->reg.fw_status);
	ts_info("cfg_addr:0x%04x", ts_dev->reg.cfg_addr);
	ts_info("esd:0x%04x", ts_dev->reg.esd);
	ts_info("command:0x%04x", ts_dev->reg.command);
	ts_info("coor:0x%04x", ts_dev->reg.coor);
	ts_info("gesture:0x%04x", ts_dev->reg.gesture);
	ts_info("fw_request:0x%04x", ts_dev->reg.fw_request);
	ts_info("proximity:0x%04x", ts_dev->reg.proximity);
	goodix_get_lockdowninfo(core_data);

exit:
	kfree(cfg_bin->cfg_pkgs);
	kfree(cfg_bin->bin_data);
	kfree(cfg_bin);
	if (r) {
		goodix_ts_blocking_notify(NOTIFY_CFG_BIN_FAILED, &r);
	} else {
		goodix_ts_blocking_notify(NOTIFY_CFG_BIN_SUCCESS, &r);
	}
	ts_info("cfg bin state %d, ret %d", ts_dev->cfg_bin_state, r);
	return r;
}

static int goodix_extract_cfg_pkg(struct goodix_ts_device *ts_dev,
		struct goodix_cfg_package *cfg_pkg)
{
	struct goodix_ts_config *ts_cfg;

	if (cfg_pkg->cnst_info.cfg_type == TS_NORMAL_CFG) {
		ts_cfg = &(ts_dev->normal_cfg);
	} else if (cfg_pkg->cnst_info.cfg_type == TS_HIGH_SENSE_CFG) {
		ts_cfg = &(ts_dev->highsense_cfg);
	} else {
		ts_err("unknown cfg type %d", cfg_pkg->cnst_info.cfg_type);
		return -EINVAL;
	}

	ts_cfg->length = cfg_pkg->pkg_len -
				TS_PKG_CONST_INFO_LEN - TS_PKG_REG_INFO_LEN;
	if (ts_cfg->length > sizeof(ts_cfg->data)) {
		ts_err("illegal cfg length %d", ts_cfg->length);
		return -EINVAL;
	}
	if (ts_cfg->length) {
		ts_info("get config type %d, len %d",
			cfg_pkg->cnst_info.cfg_type, ts_cfg->length);
		memcpy(ts_cfg->data, cfg_pkg->cfg, ts_cfg->length);
		ts_cfg->initialized = TS_CFG_STABLE;
		mutex_init(&ts_cfg->lock);
	} else {
		ts_info("no config data");
	}

	/*get register info*/
	ts_dev->reg.cfg_send_flag = cfg_pkg->reg_info.cfg_send_flag.addr;
	ts_dev->reg.version_base = cfg_pkg->reg_info.version_base.addr;
	ts_dev->reg.version_len = cfg_pkg->reg_info.version_base.reserved1;
	ts_dev->reg.pid = cfg_pkg->reg_info.pid.addr;
	ts_dev->reg.pid_len = cfg_pkg->reg_info.pid.reserved1;
	ts_dev->reg.vid = cfg_pkg->reg_info.vid.addr;
	ts_dev->reg.vid_len = cfg_pkg->reg_info.vid.reserved1;
	ts_dev->reg.sensor_id = cfg_pkg->reg_info.sensor_id.addr;
	ts_dev->reg.sensor_id_mask = cfg_pkg->reg_info.sensor_id.reserved1;
	ts_dev->reg.fw_mask = cfg_pkg->reg_info.fw_mask.addr;
	ts_dev->reg.fw_status = cfg_pkg->reg_info.fw_status.addr;
	ts_dev->reg.cfg_addr = cfg_pkg->reg_info.cfg_addr.addr;
	ts_dev->reg.esd = cfg_pkg->reg_info.esd.addr;
	ts_dev->reg.command = cfg_pkg->reg_info.command.addr;
	ts_dev->reg.coor = cfg_pkg->reg_info.coor.addr;
	ts_dev->reg.gesture = cfg_pkg->reg_info.gesture.addr;
	ts_dev->reg.fw_request = cfg_pkg->reg_info.fw_request.addr;
	ts_dev->reg.proximity = cfg_pkg->reg_info.proximity.addr;

	return 0;
}

static int goodix_get_reg_and_cfg(struct goodix_ts_device *ts_dev,
				struct goodix_cfg_bin *cfg_bin)
{
	int i;
	u16 addr;
	u8 read_len;
	char temp_sensor_id = -1;
	u8 temp_fw_mask[TS_CFG_BLOCK_FW_MASK_LEN] = {0x00};
	u8 temp_pid[TS_CFG_BLOCK_PID_LEN] = {0x00};
	int r = -EINVAL;

	if (!cfg_bin->head.pkg_num || !cfg_bin->cfg_pkgs) {
		ts_err("there is none cfg package, pkg_num:%d",
			cfg_bin->head.pkg_num);
		return -EINVAL;
	}

	memset(&ts_dev->normal_cfg, 0, sizeof(ts_dev->normal_cfg));
	memset(&ts_dev->highsense_cfg, 0, sizeof(ts_dev->highsense_cfg));

	/* find suitable cfg packages */
	for (i = 0; i < cfg_bin->head.pkg_num; i++) {
		/*get ic type*/
		if (!strncmp(cfg_bin->cfg_pkgs[i].cnst_info.ic_type,
				"normandy", strlen("normandy"))) {
			ts_dev->ic_type = IC_TYPE_NORMANDY;
		} else if (!strncmp(cfg_bin->cfg_pkgs[i].cnst_info.ic_type,
				"yellowstone", strlen("yellowstone"))) {
			ts_dev->ic_type = IC_TYPE_YELLOWSTONE;
		} else {
			ts_err("unknow ic type of cfg_bin:%s",
				cfg_bin->cfg_pkgs[i].cnst_info.ic_type);
			continue;
		}

		ts_info("ic_type:%d", ts_dev->ic_type);

		/* contrast sensor id */
		addr = cfg_bin->cfg_pkgs[i].reg_info.sensor_id.addr;
		if (!addr) {
			ts_info("pkg:%d, sensor_id reg is NULL", i);
			continue;
		}

		r = ts_dev->hw_ops->read(ts_dev, addr, &temp_sensor_id, 1);
		if (r < 0) {
			ts_err("failed get sensor of pkg:%d, reg:0x%02x", i, addr);
			goto get_default_pkg;
		}
		ts_info("sensor id is %d", temp_sensor_id);
		/*sensor.reserved1 is a mask, if it's not ZERO, use it*/
		if (cfg_bin->cfg_pkgs[i].reg_info.sensor_id.reserved1 != 0)
			temp_sensor_id &= cfg_bin->cfg_pkgs[i].reg_info.sensor_id.reserved1;

		if (temp_sensor_id != cfg_bin->cfg_pkgs[i].cnst_info.sensor_id) {
			ts_err("pkg:%d, sensor id contrast FAILED, reg:0x%02x",
				i, addr);
			ts_err("sensor_id from i2c:%d, sensor_id of cfg bin:%d",
				temp_sensor_id,
				cfg_bin->cfg_pkgs[i].cnst_info.sensor_id);
			continue;
		}

		/*contrast fw_mask, if this reg is null, skip this step*/
		addr = cfg_bin->cfg_pkgs[i].reg_info.fw_mask.addr;
		if (addr && cfg_bin->cfg_pkgs[i].cnst_info.fw_mask[0]) {
			r = ts_dev->hw_ops->read(ts_dev, addr, temp_fw_mask,
						 sizeof(temp_fw_mask));
			if (r < 0) {
				ts_err("failed read fw_mask pkg:%d, reg:0x%02x",
					i, addr);
				goto get_default_pkg;
			}
			if (strncmp(temp_fw_mask, cfg_bin->cfg_pkgs[i].cnst_info.fw_mask,
					sizeof(temp_fw_mask))) {
				ts_err("pkg:%d, fw_mask contrast FAILED, reg:0x%02x,", i, addr);
				ts_err("mask from i2c:%s, mask of cfg bin:%s",
					temp_fw_mask,
					cfg_bin->cfg_pkgs[i].cnst_info.fw_mask);
				continue;
			}
		}

		/*contrast pid*/
		addr = cfg_bin->cfg_pkgs[i].reg_info.pid.addr;
		read_len = cfg_bin->cfg_pkgs[i].reg_info.pid.reserved1;
		if (!addr) {
			ts_err("pkg:%d, pid reg is NULL", i);
			continue;
		}
		if (read_len <= 0 || read_len > TS_CFG_BLOCK_PID_LEN) {
			ts_err("pkg:%d, hw_pid length ERROR, len:%d",
				i, read_len);
			continue;
		}
		r = ts_dev->hw_ops->read(ts_dev, addr, temp_pid, read_len);
		if (r < 0) {
			ts_err("failed read pid pkg:%d, pid reg:0x%02x", i, addr);
			goto get_default_pkg;
		}
		if (strncmp(temp_pid, cfg_bin->cfg_pkgs[i].cnst_info.hw_pid, read_len)) {
			ts_err("pkg:%d, pid contrast FAILED, reg:0x%02x", i, addr);
			ts_err("pid from i2c:%s, pid of cfg bin:%s",
				temp_pid, cfg_bin->cfg_pkgs[i].cnst_info.hw_pid);
			continue;
		}

		ts_info("try get package info: ic type %s, cfg type %d",
			cfg_bin->cfg_pkgs[i].cnst_info.ic_type,
			cfg_bin->cfg_pkgs[i].cnst_info.cfg_type);
		/* currently only support normal and high_sense config */
		if (cfg_bin->cfg_pkgs[i].cnst_info.cfg_type == TS_NORMAL_CFG ||
		    cfg_bin->cfg_pkgs[i].cnst_info.cfg_type == TS_HIGH_SENSE_CFG) {
			r = goodix_extract_cfg_pkg(ts_dev, &cfg_bin->cfg_pkgs[i]);
			if (!r) {
				ts_dev->cfg_bin_state = CFG_BIN_STATE_INITIALIZED;
				ts_info("success parse cfg bin");
			} else {
				ts_err("failed parse cfg bin");
				break;
			}
		}
	}

get_default_pkg:
	if (ts_dev->cfg_bin_state != CFG_BIN_STATE_INITIALIZED) {
		ts_err("no valid normal cfg, use cfg_pkg 0 as default");
		/* Foo code for recover dead IC.
		 * force set package 0 config type to normal config, this will
		 * config will use to recover IC.
		 */
		cfg_bin->cfg_pkgs[0].cnst_info.cfg_type = TS_NORMAL_CFG;
		if (goodix_extract_cfg_pkg(ts_dev, &cfg_bin->cfg_pkgs[0])) {
			ts_err("failed get valid config for IC recover");
			ts_dev->cfg_bin_state = CFG_BIN_STATE_ERROR;
		} else {
			ts_dev->cfg_bin_state = CFG_BIN_STATE_TEMP;
			ts_info("get temp config data");
		}

		r = -EINVAL;
	}

	return r;
}

static int goodix_read_cfg_bin(struct device *dev, struct goodix_cfg_bin *cfg_bin)
{
	const struct firmware *firmware = NULL;
	char cfg_bin_name[32] = {0x00};
	int i = 0, r;

	/*get cfg_bin_name*/
	strlcpy(cfg_bin_name, TS_DEFAULT_CFG_BIN, sizeof(cfg_bin_name));

	ts_info("cfg_bin_name:%s", cfg_bin_name);

	for (i = 0; i < TS_RQST_FW_RETRY_TIMES; i++) {
		r = request_firmware(&firmware, cfg_bin_name, dev);
		if (r < 0) {
			ts_err("failed get cfg bin[%s] error:%d, try_times:%d",
				cfg_bin_name, r, i + 1);
			msleep(1000);
		} else {
			ts_info("Cfg_bin image [%s] is ready, try_times:%d",
				cfg_bin_name, i + 1);
			break;
		}
	}
	if (i >= TS_RQST_FW_RETRY_TIMES) {
		ts_err("get cfg_bin FAILED");
		goto exit;
	}

	if (firmware->size <= 0) {
		ts_err("request_firmware, cfg_bin length ERROR,len:%zu",
			firmware->size);
		r = -EINVAL;
		goto exit;
	}

	cfg_bin->bin_data_len = firmware->size;
	/*allocate memory for cfg_bin->bin_data*/
	cfg_bin->bin_data = kzalloc(cfg_bin->bin_data_len, GFP_KERNEL);
	if (!cfg_bin->bin_data)
		r = -ENOMEM;

	memcpy(cfg_bin->bin_data, firmware->data, cfg_bin->bin_data_len);

	r = 0;
exit:
	if (firmware) {
		release_firmware(firmware);
		firmware = NULL;
	}
	return r;
}

static void goodix_cfg_pkg_leToCpu(struct goodix_cfg_package *pkg)
{
	if (!pkg) {
		ts_err("cfg package is NULL");
		return;
	}
	/*package const_info*/
	pkg->cnst_info.pkg_len = le32_to_cpu(pkg->cnst_info.pkg_len);
	pkg->cnst_info.x_res_offset = le16_to_cpu(pkg->cnst_info.x_res_offset);
	pkg->cnst_info.y_res_offset = le16_to_cpu(pkg->cnst_info.y_res_offset);
	pkg->cnst_info.trigger_offset = le16_to_cpu(pkg->cnst_info.trigger_offset);

	/*package reg_info*/
	pkg->reg_info.cfg_send_flag.addr = le16_to_cpu(pkg->reg_info.cfg_send_flag.addr);
	pkg->reg_info.pid.addr = le16_to_cpu(pkg->reg_info.pid.addr);
	pkg->reg_info.vid.addr = le16_to_cpu(pkg->reg_info.vid.addr);
	pkg->reg_info.sensor_id.addr = le16_to_cpu(pkg->reg_info.sensor_id.addr);
	pkg->reg_info.fw_status.addr = le16_to_cpu(pkg->reg_info.fw_status.addr);
	pkg->reg_info.cfg_addr.addr = le16_to_cpu(pkg->reg_info.cfg_addr.addr);
	pkg->reg_info.esd.addr = le16_to_cpu(pkg->reg_info.esd.addr);
	pkg->reg_info.command.addr = le16_to_cpu(pkg->reg_info.command.addr);
	pkg->reg_info.coor.addr = le16_to_cpu(pkg->reg_info.coor.addr);
	pkg->reg_info.gesture.addr = le16_to_cpu(pkg->reg_info.gesture.addr);
	pkg->reg_info.fw_request.addr = le16_to_cpu(pkg->reg_info.fw_request.addr);
	pkg->reg_info.proximity.addr = le16_to_cpu(pkg->reg_info.proximity.addr);
}

static int goodix_later_init_thread(void *data)
{
	int ret;
	struct goodix_ts_core *ts_core = data;
	struct goodix_ts_device *ts_dev;

	if (!data) {
		ts_err("ts core data can't be null");
		return -EINVAL;
	}

	if (!atomic_read(&ts_core->initialized)) {
		wait_for_completion(&goodix_modules.core_comp);
	}

	if (!atomic_read(&ts_core->initialized)) {
		ts_err("ts core not init");
		return -EINVAL;
	}

	ts_dev = ts_core->ts_dev;
	if (!ts_dev) {
		ts_err("ts dev data can't be null");
		return -EINVAL;
	}

	ret = goodix_cfg_bin_proc(ts_core);
	if (ret) {
		ts_err("parse cfg bin encounter error, %d", ret);
	} else {
		ts_info("success get cfg bin");
	}

	if (ts_dev->cfg_bin_state == CFG_BIN_STATE_ERROR) {
		ts_err("parse cfg bin encounter fatal err");
		goto release_core;
	}

	if (ts_dev->cfg_bin_state == CFG_BIN_STATE_TEMP) {
		ts_err("failed get valid config data, retry after fwupdate");
		ret = goodix_do_fw_update(UPDATE_MODE_BLOCK|UPDATE_MODE_FORCE|
					UPDATE_MODE_FLASH_CFG|
					UPDATE_MODE_SRC_REQUEST);
		if (ret) {
			ts_err("fw update failed, %d", ret);
			goto release_core;
		}
		ts_info("fw update success retry parse cfg bin");
		ret = goodix_cfg_bin_proc(ts_core);
		if (ret) {
			ts_err("failed parse cfg bin after fw update");
			goto release_core;
		}
	} else {
		ts_info("success parse config bin");
		ret = goodix_do_fw_update(UPDATE_MODE_BLOCK|
					UPDATE_MODE_FLASH_CFG|
					UPDATE_MODE_SRC_REQUEST);
		if (ret) {
			ts_err("fw update failed, %d[ignore]", ret);
			ret = 0;
		}
	}
	ret = goodix_ts_stage2_init(ts_core);
	if (!ret) {
		ts_info("stage2 init success");
		return ret;
	}
	ts_err("stage2 init failed, %d", ret);

release_core:
	goodix_ts_core_release(ts_core);
	return ret;
}

int goodix_start_later_init(struct goodix_ts_core *ts_core)
{
	struct task_struct *init_thrd;
	/* create and run update thread */
	init_thrd = kthread_run(goodix_later_init_thread,
				ts_core, "goodix_init_thread");
	if (IS_ERR_OR_NULL(init_thrd)) {
		ts_err("Failed to create update thread:%ld",
			PTR_ERR(init_thrd));
		return -EFAULT;
	}
	return 0;
}
