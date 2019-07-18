#include "goodix_cfg_bin.h"
#include <drm/drm_notifier.h>

extern int goodix_i2c_write(struct goodix_ts_device *dev, unsigned int reg, unsigned char *data, unsigned int len);
extern int goodix_i2c_read(struct goodix_ts_device *dev, unsigned int reg, unsigned char *data, unsigned int len);
extern int goodix_hw_reset(struct goodix_ts_device *dev);
extern struct goodix_module goodix_modules;

int goodix_start_cfg_bin(struct goodix_ts_core *ts_core)
{
	struct task_struct *cfg_bin_thrd;
	/* create and run update thread */
	cfg_bin_thrd = kthread_run(goodix_cfg_bin_proc,
							ts_core, "goodix-parse_cfg_bin");
	if (IS_ERR_OR_NULL(cfg_bin_thrd)) {
		ts_err("Failed to create update thread:%ld",
				PTR_ERR(cfg_bin_thrd));
		return -EFAULT;
	}
	return 0;
}

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

int goodix_parse_cfg_bin(struct goodix_cfg_bin *cfg_bin)
{
	u8 checksum;
	int i, r;
	u16 offset1, offset2;
	if (!cfg_bin->bin_data || cfg_bin->bin_data_len == 0) {
		ts_err("NO cfg_bin data, cfg_bin data length:%d", cfg_bin->bin_data_len);
		r = -EINVAL;
		goto exit;
	}

	/* copy cfg_bin head info */
	if (cfg_bin->bin_data_len < sizeof(struct goodix_cfg_bin_head)) {
		ts_err("Invalid cfg_bin size:%d", cfg_bin->bin_data_len);
		r = -EINVAL;
		goto exit;
	}
	memcpy(&cfg_bin->head, cfg_bin->bin_data, sizeof(struct goodix_cfg_bin_head));
	cfg_bin->head.bin_len = le32_to_cpu(cfg_bin->head.bin_len);


	/*check length*/
	if (cfg_bin->bin_data_len != cfg_bin->head.bin_len) {
		ts_err("cfg_bin length error,len in cfg_bin:%d, len of firmware:%d",
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
		ts_err("cfg_bin checksum ERROR, checksum in cfg_bin:0x%02x, checksum caculate:0x%02x",
				cfg_bin->head.checksum, checksum);
		r = -EINVAL;
		goto exit;
	}

	/*allocate memory for cfg packages*/
	cfg_bin->cfg_pkgs = kzalloc(sizeof(struct goodix_cfg_package) * cfg_bin->head.pkg_num, GFP_KERNEL);
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
				&cfg_bin->bin_data[offset1],
				TS_PKG_CONST_INFO_LEN);
		memcpy(&cfg_bin->cfg_pkgs[i].reg_info,
				&cfg_bin->bin_data[offset1 + TS_PKG_CONST_INFO_LEN],
				TS_PKG_REG_INFO_LEN);
		/*compatible little edition and big edition*/
		goodix_cfg_pkg_leToCpu(&cfg_bin->cfg_pkgs[i]);

		/*get configuration data*/
		cfg_bin->cfg_pkgs[i].cfg = &cfg_bin->bin_data[offset1 + TS_PKG_HEAD_LEN];
	}

	/*debug, print pkg information*/
	for (i = 0; i < cfg_bin->head.pkg_num; i++) {
		ts_info("---------------------------------------------");
		ts_info("------package:%d------", i + 1);
		ts_info("package len:%04x", cfg_bin->cfg_pkgs[i].cnst_info.pkg_len);
		ts_info("package ic_type:%s", cfg_bin->cfg_pkgs[i].cnst_info.ic_type);
		ts_info("package cfg_type:%01x", cfg_bin->cfg_pkgs[i].cnst_info.cfg_type);
		ts_info("package sensor_id:%01x", cfg_bin->cfg_pkgs[i].cnst_info.sensor_id);
		ts_info("package hw_pid:%s", cfg_bin->cfg_pkgs[i].cnst_info.hw_pid);
		ts_info("package hw_vid:%s", cfg_bin->cfg_pkgs[i].cnst_info.hw_vid);
		ts_info("package fw_mask_version:%s", cfg_bin->cfg_pkgs[i].cnst_info.fw_mask);
		ts_info("package fw_patch_version:%s", cfg_bin->cfg_pkgs[i].cnst_info.fw_patch);
		ts_info("package x_res_offset:%02x", cfg_bin->cfg_pkgs[i].cnst_info.x_res_offset);
		ts_info("package y_res_offset:%02x", cfg_bin->cfg_pkgs[i].cnst_info.y_res_offset);
		ts_info("package trigger_offset:%02x", cfg_bin->cfg_pkgs[i].cnst_info.trigger_offset);

		ts_info("");
		ts_info("send_cfg_flag reg:%02x", cfg_bin->cfg_pkgs[i].reg_info.cfg_send_flag.addr);
		ts_info("version base reg:%02x, len:%d",
				cfg_bin->cfg_pkgs[i].reg_info.version_base.addr,
				cfg_bin->cfg_pkgs[i].reg_info.version_base.reserved1);
		ts_info("pid reg:%02x", cfg_bin->cfg_pkgs[i].reg_info.pid.addr);
		ts_info("vid reg:%02x", cfg_bin->cfg_pkgs[i].reg_info.vid.addr);
		ts_info("sensor_id reg:%02x", cfg_bin->cfg_pkgs[i].reg_info.sensor_id.addr);
		ts_info("fw_status reg:%02x", cfg_bin->cfg_pkgs[i].reg_info.fw_status.addr);
		ts_info("cfg_addr reg:%02x", cfg_bin->cfg_pkgs[i].reg_info.cfg_addr.addr);
		ts_info("esd reg:%02x", cfg_bin->cfg_pkgs[i].reg_info.esd.addr);
		ts_info("command reg:%02x", cfg_bin->cfg_pkgs[i].reg_info.command.addr);
		ts_info("coor:%02x", cfg_bin->cfg_pkgs[i].reg_info.coor.addr);
		ts_info("gesture:%02x", cfg_bin->cfg_pkgs[i].reg_info.gesture.addr);
		ts_info("fw_request:%02x", cfg_bin->cfg_pkgs[i].reg_info.fw_request.addr);
		ts_info("proximity:%02x", cfg_bin->cfg_pkgs[i].reg_info.proximity.addr);

		ts_info("--------------------------------------------");
	}
	r = 0;
exit:
	return r;
}

int goodix_cfg_bin_proc(void *data)
{
	struct goodix_ts_core *core_data = data;
	struct goodix_ts_device *ts_dev = core_data->ts_dev;

	struct device *dev = ts_dev->dev;
	int r;
	struct goodix_cfg_bin *cfg_bin = kzalloc(sizeof(struct goodix_cfg_bin),
							GFP_KERNEL);
	if (!cfg_bin) {
		ts_err("Failed to alloc memory for cfg_bin");
		r = -ENOMEM;
		goto exit;
	}
	/*get cfg_bin from file system*/
	r = goodix_read_cfg_bin(dev, cfg_bin);
	if (r < 0) {
		ts_err("read cfg_bin from /etc/firmware FAILED");
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
	/*init i2c_set_doze_mode para*/
	ts_dev->doze_mode_set_count = 0;
	mutex_init(&ts_dev->doze_mode_lock);


	/*get register address and configuration from cfg bin*/
	r = goodix_get_reg_and_cfg(ts_dev, cfg_bin);
	if (!r) {
		ts_info("get reg and cfg from cfg_bin SUCCESS");
	} else {
		if (r != -EBUS) {
			ts_err("get reg and cfg from cfg_bin FAILED, update fw then retry");
			goodix_modules.core_data = core_data;
			goodix_modules.core_exit = false;
			complete_all(&goodix_modules.core_comp);
			goto exit;
		} else {
			ts_err("get reg and cfg from cfg_bin FAILED, I2C com ERROR");
			goto exit;
		}
	}
	/*init i2c_set_doze_mode para*/
	ts_dev->doze_mode_set_count = 0;
	mutex_init(&ts_dev->doze_mode_lock);
	/*debug*/
	ts_info("@@@@@@@@@");
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
	ts_info("@@@@@@@@@");


	/* initialize firmware */
	r = ts_dev->hw_ops->init(ts_dev);
	if (-EBUS == r)
		goto exit;

	/* alloc/config/register input device */
	r = goodix_ts_input_dev_config(core_data);
	if (r < 0)
		goto exit;

	/* request irq line */
	r = goodix_ts_irq_setup(core_data);
	if (r < 0)
		goto exit;

	/*set flag, prevent fwupdate module parse cfg_group again*/
	core_data->cfg_group_parsed = true;

	/* inform the external module manager that
	 * touch core layer is ready now */
	goodix_modules.core_data = core_data;
	goodix_modules.core_exit = false;
	/*complete_all(&goodix_modules.core_comp);*/

#ifdef CONFIG_DRM
	core_data->fb_notifier.notifier_call = goodix_ts_fb_notifier_callback;
	if (drm_register_client(&core_data->fb_notifier))
		ts_err("Failed to register fb notifier client:%d", r);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	core_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	core_data->early_suspend.resume = goodix_ts_lateresume;
	core_data->early_suspend.suspend = goodix_ts_earlysuspend;
	register_early_suspend(&core_data->early_suspend);
#endif
	goodix_get_lockdowninfo(core_data);

	/* esd protector */
	goodix_ts_esd_init(core_data);

	/* generic notifier callback */
	core_data->ts_notifier.notifier_call = goodix_generic_noti_callback;
	goodix_ts_register_notifier(&core_data->ts_notifier);

exit:
	complete_all(&goodix_modules.core_comp);

	if (cfg_bin->cfg_pkgs) {
		kfree(cfg_bin->cfg_pkgs);
		cfg_bin->cfg_pkgs = NULL;
	}

	if (cfg_bin->bin_data) {
		kfree(cfg_bin->bin_data);
		cfg_bin->bin_data = NULL;
	}
	return r;
}

int goodix_get_reg_and_cfg(struct goodix_ts_device *ts_dev, struct goodix_cfg_bin *cfg_bin)
{
	int i;
	u16 addr;
	u8 read_len;
	struct goodix_cfg_package *normal_pkg, *high_sense_pkg;
	char temp_sensor_id = -1;
	u8 temp_fw_mask[TS_CFG_BLOCK_FW_MASK_LEN] = {0x00};
	u8 temp_pid[TS_CFG_BLOCK_PID_LEN] = {0x00};
	int r = -EINVAL;
	normal_pkg = NULL;
	high_sense_pkg = NULL;

	if (!cfg_bin->head.pkg_num || !cfg_bin->cfg_pkgs) {
		ts_err("there is none cfg package, pkg_num:%d", cfg_bin->head.pkg_num);
		r = -EINVAL;
		goto exit;
	}

	/*select suitable cfg packages*/
	for (i = 0; i < cfg_bin->head.pkg_num; i++) {
		/*get ic type*/
		if (!strncmp(cfg_bin->cfg_pkgs[i].cnst_info.ic_type, "nanjing",
					sizeof(cfg_bin->cfg_pkgs[i].cnst_info.ic_type)))
			ts_dev->ic_type = IC_TYPE_NANJING;
		else if (!strncmp(cfg_bin->cfg_pkgs[i].cnst_info.ic_type, "normandy",
					sizeof(cfg_bin->cfg_pkgs[i].cnst_info.ic_type)))
			ts_dev->ic_type = IC_TYPE_NORMANDY;
		else
			ts_err("get ic type FAILED, unknow ic type from cfg_bin:%s",
					cfg_bin->cfg_pkgs[i].cnst_info.ic_type);


		ts_info("ic_type:%d", ts_dev->ic_type);

		/*contrast sensor id*/
		addr = cfg_bin->cfg_pkgs[i].reg_info.sensor_id.addr;
		if (!addr) {
			ts_info("pkg:%d, sensor_id reg is NULL", i);
			continue;
		}

		r = goodix_i2c_read(ts_dev, addr,
				&temp_sensor_id, 1);

		if (r < 0) {
			ts_err("read sensor id FAILED,I2C ERROR, pkg:%d, sensor_id reg:0x%02x", i, addr);
			goto exit;
		} else {
			/*sensor.reserved1 is a mask, if it's not ZERO, use it*/
			if (cfg_bin->cfg_pkgs[i].reg_info.sensor_id.reserved1 != 0)
				temp_sensor_id &= cfg_bin->cfg_pkgs[i].reg_info.sensor_id.reserved1;

			if (temp_sensor_id != cfg_bin->cfg_pkgs[i].cnst_info.sensor_id) {
				ts_err("pkg:%d, sensor id contrast FAILED, reg:0x%02x", i, addr);
				ts_err("sensor_id from i2c:%d, sensor_id of cfg bin:%d",
						temp_sensor_id,
						cfg_bin->cfg_pkgs[i].cnst_info.sensor_id);
				continue;
			}
		}

		/*contrast fw_mask, if this reg is null, skip this step*/
		addr = cfg_bin->cfg_pkgs[i].reg_info.fw_mask.addr;
		if (!addr || !cfg_bin->cfg_pkgs[i].cnst_info.fw_mask[0]) {
			ts_err("pkg:%d, fw_mask of cfg bin is NULL, Skip!!", i);
		} else {
			r = goodix_i2c_read(ts_dev, addr, temp_fw_mask, sizeof(temp_fw_mask));
			if (r < 0) {
				ts_err("read fw_mask FAILED, I2C ERROR, pkg: %d, fw_mask reg:0x%02x", i, addr);
				goto exit;
			} else if (strncmp(temp_fw_mask, cfg_bin->cfg_pkgs[i].cnst_info.fw_mask,
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
			ts_err("pkg:%d, hw_pid length ERROR, len:%d", i, read_len);
			continue;
		}
		r = goodix_i2c_read(ts_dev, addr, temp_pid, read_len);
		if (r < 0) {
			ts_err("read pid FAILED, I2C ERROR, pkg: %d, pid reg:0x%02x", i, addr);
			goto exit;
		} else if (strncmp(temp_pid, cfg_bin->cfg_pkgs[i].cnst_info.hw_pid, read_len)) {
				ts_err("pkg:%d, pid contrast FAILED, reg:0x%02x", i, addr);
				ts_err("pid from i2c:%s, pid of cfg bin:%s",
						temp_pid,
						cfg_bin->cfg_pkgs[i].cnst_info.hw_pid);
				continue;
		}

		/*contrast success, cfg_type*/
		if (cfg_bin->cfg_pkgs[i].cnst_info.cfg_type == TS_NORMAL_CFG) {
			ts_info("find normal cfg_pkg SUCCESS");
			r = 0;
			normal_pkg = &cfg_bin->cfg_pkgs[i];
		}
		if (cfg_bin->cfg_pkgs[i].cnst_info.cfg_type == TS_HIGH_SENSE_CFG) {
			ts_info("find high sense cfg_pkg SUCCESS");
			high_sense_pkg = &cfg_bin->cfg_pkgs[i];
		}
	}

	/*get register address from normal_pkg*/
	if (!normal_pkg) {
		ts_err("ERROR, none suitable normal_pkg exist in cfg_bin");
		/*ts_dev->ic_type = IC_TYPE_NONE;*/
		r = -EINVAL;
		goto exit;
	} else {
		/*get ic type*/
		if (!strncmp(normal_pkg->cnst_info.ic_type, "nanjing",
					sizeof(normal_pkg->cnst_info.ic_type)))
			ts_dev->ic_type = IC_TYPE_NANJING;
		else if (!strncmp(normal_pkg->cnst_info.ic_type, "normandy",
					sizeof(normal_pkg->cnst_info.ic_type)))
			ts_dev->ic_type = IC_TYPE_NORMANDY;
		else
			ts_err("get ic type FAILED, unknow ic type from cfg_bin:%s",
					normal_pkg->cnst_info.ic_type);

		/*get register info*/
		ts_dev->reg.cfg_send_flag = normal_pkg->reg_info.cfg_send_flag.addr;

		ts_dev->reg.version_base = normal_pkg->reg_info.version_base.addr;
		ts_dev->reg.version_len = normal_pkg->reg_info.version_base.reserved1;

		ts_dev->reg.pid = normal_pkg->reg_info.pid.addr;
		ts_dev->reg.pid_len = normal_pkg->reg_info.pid.reserved1;

		ts_dev->reg.vid = normal_pkg->reg_info.vid.addr;
		ts_dev->reg.vid_len = normal_pkg->reg_info.vid.reserved1;

		ts_dev->reg.sensor_id = normal_pkg->reg_info.sensor_id.addr;
		ts_dev->reg.sensor_id_mask = normal_pkg->reg_info.sensor_id.reserved1;

		ts_dev->reg.fw_mask = normal_pkg->reg_info.fw_mask.addr;
		ts_dev->reg.fw_status = normal_pkg->reg_info.fw_status.addr;
		ts_dev->reg.cfg_addr = normal_pkg->reg_info.cfg_addr.addr;
		ts_dev->reg.esd = normal_pkg->reg_info.esd.addr;
		ts_dev->reg.command = normal_pkg->reg_info.command.addr;
		ts_dev->reg.coor = normal_pkg->reg_info.coor.addr;
		ts_dev->reg.gesture = normal_pkg->reg_info.gesture.addr;
		ts_dev->reg.fw_request = normal_pkg->reg_info.fw_request.addr;
		ts_dev->reg.proximity = normal_pkg->reg_info.proximity.addr;
	}

	/*get configuration from pkgs*/
	if (normal_pkg) {
		ts_info("normal cfg is found!");
		if (!ts_dev->normal_cfg) {
			ts_dev->normal_cfg = devm_kzalloc(ts_dev->dev,
					sizeof(*ts_dev->normal_cfg), GFP_KERNEL);
			if (!ts_dev->normal_cfg) {
				ts_err("Failed to alloc memory for normal cfg");
				return -ENOMEM;
			}
			mutex_init(&ts_dev->normal_cfg->lock);
		}

		ts_dev->normal_cfg->length = normal_pkg->pkg_len -
			TS_PKG_CONST_INFO_LEN - TS_PKG_REG_INFO_LEN;
		memcpy(ts_dev->normal_cfg->data,
				normal_pkg->cfg,
				ts_dev->normal_cfg->length);
	}

	if (high_sense_pkg) {
		ts_info("high sense cfg is found!");
		if (!ts_dev->highsense_cfg) {
			ts_dev->highsense_cfg = devm_kzalloc(ts_dev->dev,
					sizeof(*ts_dev->highsense_cfg), GFP_KERNEL);
			if (!ts_dev->highsense_cfg) {
				ts_err("Failed to alloc memory for high sense cfg");
				return -ENOMEM;
			}
			mutex_init(&ts_dev->highsense_cfg->lock);
		}

		ts_dev->highsense_cfg->length = high_sense_pkg->pkg_len -
			TS_PKG_CONST_INFO_LEN - TS_PKG_REG_INFO_LEN;
		memcpy(ts_dev->highsense_cfg->data,
				high_sense_pkg->cfg,
				ts_dev->highsense_cfg->length);
	}

exit:
	return r;
}


int goodix_read_cfg_bin(struct device *dev, struct goodix_cfg_bin *cfg_bin)
{
	int r;
	const struct firmware *firmware;
	char cfg_bin_name[32] = {0x00};
	int i = 0;
	struct goodix_ts_core *core_data = dev_get_drvdata(dev);
	struct goodix_ts_device *ts_dev = core_data->ts_dev;
	struct goodix_ts_board_data *ts_bdata  = ts_dev->board_data;

	if (ts_dev == NULL) {
		ts_err("can't get ts_dev");
		return -EINVAL;
	}
	ts_bdata = ts_dev->board_data;
	if (ts_bdata == NULL) {
		ts_err("can't get ts_bdata");
		return -EINVAL;
	}
	/*get cfg_bin_name*/
	if (ts_bdata->cfg_bin_name)
		strlcpy(cfg_bin_name, ts_bdata->cfg_bin_name,
				sizeof(cfg_bin_name));
	else
		strlcpy(cfg_bin_name, TS_DEFAULT_CFG_BIN,
				sizeof(cfg_bin_name));
	ts_info("ts_bdata->cfg_bin_name:%s", ts_bdata->cfg_bin_name);

	ts_info("cfg_bin_name:%s", cfg_bin_name);

	for (i = 0; i < TS_RQST_FW_RETRY_TIMES; i++) {
		r = request_firmware(&firmware, cfg_bin_name, dev);
		if (r < 0) {
			ts_err("Cfg_bin image [%s] not available,error:%d, try_times:%d", cfg_bin_name, r, i + 1);
			msleep(1000);
		} else {
			ts_info("Cfg_bin image [%s] is ready, try_times:%d", cfg_bin_name, i + 1);
			break;
		}
	}
	if (i >= TS_RQST_FW_RETRY_TIMES) {
		ts_err("get cfg_bin FAILED");
		goto exit;
	}

	if (firmware->size <= 0) {
		ts_err("request_firmware, cfg_bin length ERROR,len:%zu", firmware->size);
		r = -EINVAL;
		goto exit;
	}

	cfg_bin->bin_data_len = firmware->size;
	/*allocate memory for cfg_bin->bin_data*/
	cfg_bin->bin_data = kzalloc(cfg_bin->bin_data_len, GFP_KERNEL);
	if (!cfg_bin->bin_data) {
		ts_err("Allocate memory for cfg_bin->bin_data FAILED");
		r = -ENOMEM;
	}
	memcpy(cfg_bin->bin_data, firmware->data, cfg_bin->bin_data_len);

	r = 0;
exit:
	if (firmware) {
		release_firmware(firmware);
		firmware = NULL;
	}
	return r;
}

int goodix_read_cfg_bin_from_dts(struct device_node *node, struct goodix_cfg_bin *cfg_bin)
{
	unsigned int len = 0;
	struct property *prop = NULL;

	prop = of_find_property(node, "goodix_cfg_bin", &len);
	if (!prop || !prop->value || len == 0) {
		ts_err("Invalid cfg type, size:%u",  len);
		return -EINVAL;
	}

	cfg_bin->bin_data_len = len;
	/*allocate memory for cfg_bin->bin_data*/
	cfg_bin->bin_data = kzalloc(cfg_bin->bin_data_len, GFP_KERNEL);
	if (!cfg_bin->bin_data) {
		ts_err("Allocate memory for cfg_bin->bin_data FAILED");
		return -ENOMEM;
	}
	memcpy(cfg_bin->bin_data, prop->value, cfg_bin->bin_data_len);

	return 0;
}

void goodix_cfg_pkg_leToCpu(struct goodix_cfg_package *pkg)
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
