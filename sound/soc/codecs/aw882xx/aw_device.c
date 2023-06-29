/* #define DEBUG */
#include <linux/module.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/syscalls.h>
#include <sound/control.h>
#include <linux/uaccess.h>


#include "aw_data_type.h"
#include "aw_log.h"
#include "aw_device.h"
#include "aw_dsp.h"
/*#include "aw_afe.h"*/
#include "aw_bin_parse.h"

#define AW_DEV_SYSST_CHECK_MAX   (10)

enum {
	AW_EXT_DSP_WRITE_NONE = 0,
	AW_EXT_DSP_WRITE,
};

static char *profile_name[AW_PROFILE_MAX] = {
		"Music", "Voice", "Voip", "Ringtone", "Ringtone_hs",
		"Lowpower", "Bypass", "Mmi", "Fm", "Notification", "Receiver"
	};


static char ext_dsp_prof_write = AW_EXT_DSP_WRITE_NONE;
static DEFINE_MUTEX(g_ext_dsp_prof_wr_lock); /*lock ext wr flag*/
static unsigned int g_fade_in_time = AW_1000_US / 10;
static unsigned int g_fade_out_time = AW_1000_US >> 1;
static LIST_HEAD(g_dev_list);
static DEFINE_MUTEX(g_dev_lock);

/*********************************awinic acf*************************************/
static void aw_dev_reg_dump(struct aw_device *aw_dev)
{
	int reg_num = aw_dev->ops.aw_get_reg_num();
	uint8_t i = 0;
	unsigned int reg_val = 0;

	for (i = 0; i < reg_num; i++) {
		if (aw_dev->ops.aw_check_rd_access(i)) {
			aw_dev->ops.aw_i2c_read(aw_dev, i, &reg_val);
			aw_dev_info(aw_dev->dev, "read: reg = 0x%02x, val = 0x%04x",
				i, reg_val);
		}
	}
}

static uint8_t aw_dev_crc8_check(unsigned char *data, uint32_t data_size)
{
	uint8_t crc_value = 0x00;
	uint8_t pdatabuf = 0;
	int i;

	while (data_size--) {
		pdatabuf = *data++;
		for (i = 0; i < 8; i++) {
			/*if the lowest bit is 1*/
			if ((crc_value ^ (pdatabuf)) & 0x01) {
				/*Xor multinomial*/
				crc_value ^= 0x18;
				crc_value >>= 1;
				crc_value |= 0x80;
			} else {
				crc_value >>= 1;
			}
			pdatabuf >>= 1;
		}
	}
	return crc_value;
}

static int aw_dev_check_cfg_by_hdr(struct aw_container *aw_cfg)
{
	struct aw_cfg_hdr *cfg_hdr = NULL;
	struct aw_cfg_dde *cfg_dde = NULL;
	unsigned int end_data_offset = 0;
	unsigned int act_data = 0;
	unsigned int hdr_ddt_len = 0;
	uint8_t act_crc8 = 0;
	int i;

	cfg_hdr = (struct aw_cfg_hdr *)aw_cfg->data;

	/*check file type id is awinic acf file*/
	if (cfg_hdr->a_id != ACF_FILE_ID) {
		aw_pr_err("not acf type file");
		return -EINVAL;
	}

	hdr_ddt_len = cfg_hdr->a_hdr_offset + cfg_hdr->a_ddt_size;
	if (hdr_ddt_len > aw_cfg->len) {
		aw_pr_err("hdrlen with ddt_len [%d] overflow file size[%d]",
		cfg_hdr->a_hdr_offset, aw_cfg->len);
		return -EINVAL;
	}

	/*check data size*/
	cfg_dde = (struct aw_cfg_dde *)((char *)aw_cfg->data + cfg_hdr->a_hdr_offset);
	act_data += hdr_ddt_len;
	for (i = 0; i < cfg_hdr->a_ddt_num; i++)
		act_data += cfg_dde[i].data_size;

	if (act_data != aw_cfg->len) {
		aw_pr_err("act_data[%d] not equal to file size[%d]!",
			act_data, aw_cfg->len);
		return -EINVAL;
	}

	for (i = 0; i < cfg_hdr->a_ddt_num; i++) {
		/* data check */
		end_data_offset = cfg_dde[i].data_offset + cfg_dde[i].data_size;
		if (end_data_offset > aw_cfg->len) {
			aw_pr_err("a_ddt_num[%d] end_data_offset[%d] overflow file size[%d]",
				i, end_data_offset, aw_cfg->len);
			return -EINVAL;
		}

		/* crc check */
		act_crc8 = aw_dev_crc8_check(aw_cfg->data + cfg_dde[i].data_offset, cfg_dde[i].data_size);
		if (act_crc8 != cfg_dde[i].data_crc) {
			aw_pr_err("a_ddt_num[%d] crc8 check failed, act_crc8:0x%x != data_crc 0x%x",
				i, (uint32_t)act_crc8, cfg_dde[i].data_crc);
			return -EINVAL;
		}
	}

	aw_pr_info("project name [%s]", cfg_hdr->a_project);
	aw_pr_info("custom name [%s]", cfg_hdr->a_custom);
	aw_pr_info("version name [%d.%d.%d.%d]", cfg_hdr->a_version[3], cfg_hdr->a_version[2],
						cfg_hdr->a_version[1], cfg_hdr->a_version[0]);
	aw_pr_info("author id %d", cfg_hdr->a_author_id);

	return 0;
}

int aw_dev_load_acf_check(struct aw_container *aw_cfg)
{
	struct aw_cfg_hdr *cfg_hdr = NULL;

	if (aw_cfg == NULL) {
		aw_pr_err("aw_prof is NULL");
		return -ENOMEM;
	}

	if (aw_cfg->len < sizeof(struct aw_cfg_hdr)) {
		aw_pr_err("cfg hdr size[%d] overflow file size[%d]",
			aw_cfg->len, (int)sizeof(struct aw_cfg_hdr));
		return -EINVAL;
	}

	cfg_hdr = (struct aw_cfg_hdr *)aw_cfg->data;
	switch (cfg_hdr->a_hdr_version) {
	case AW_CFG_HDR_VER_0_0_0_1:
		return aw_dev_check_cfg_by_hdr(aw_cfg);
	default:
		aw_pr_err("unsupported hdr_version [0x%x]", cfg_hdr->a_hdr_version);
		return -EINVAL;
	}

	return 0;
}


static int aw_dev_parse_raw_reg(struct aw_device *aw_dev,
		uint8_t *data, uint32_t data_len, struct aw_prof_desc *prof_desc)
{
	aw_dev_info(aw_dev->dev, "data_size:%d enter", data_len);

	if (data_len % 4) {
		aw_dev_err(aw_dev->dev, "bin data len get error!");
		return -EINVAL;
	}

	prof_desc->sec_desc[AW_PROFILE_DATA_TYPE_REG].data = data;
	prof_desc->sec_desc[AW_PROFILE_DATA_TYPE_REG].len = data_len;

	prof_desc->prof_st = AW_PROFILE_OK;

	return 0;
}

static int aw_dev_parse_raw_dsp(struct aw_device *aw_dev,
			uint8_t *data, uint32_t data_len, struct aw_prof_desc *prof_desc)
{
	aw_dev_info(aw_dev->dev, "data_size:%d enter", data_len);

	prof_desc->sec_desc[AW_PROFILE_DATA_TYPE_DSP].data = data;
	prof_desc->sec_desc[AW_PROFILE_DATA_TYPE_DSP].len = data_len;

	return 0;
}

static int aw_dev_parse_reg_bin_with_hdr(struct aw_device *aw_dev,
			uint8_t *data, uint32_t data_len, struct aw_prof_desc *prof_desc)
{
	struct aw_bin *aw_bin = NULL;
	int ret;

	aw_dev_info(aw_dev->dev, "data_size:%d enter", data_len);

	aw_bin = kzalloc(data_len + sizeof(struct aw_bin), GFP_KERNEL);
	if (aw_bin == NULL) {
		aw_dev_err(aw_dev->dev, "devm_kzalloc aw_bin failed");
		return -ENOMEM;
	}

	aw_bin->info.len = data_len;
	memcpy(aw_bin->info.data, data, data_len);

	ret = aw_parsing_bin_file(aw_bin);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "parse bin failed");
		goto parse_bin_failed;
	}

	if ((aw_bin->all_bin_parse_num != 1) ||
		(aw_bin->header_info[0].bin_data_type != DATA_TYPE_REGISTER)) {
		aw_dev_err(aw_dev->dev, "bin num or type error");
		goto parse_bin_failed;
	}

	if (aw_bin->header_info[0].valid_data_len % 4) {
		aw_dev_err(aw_dev->dev, "bin data len get error!");
		return -EINVAL;
	}

	prof_desc->sec_desc[AW_PROFILE_DATA_TYPE_REG].data =
				data + aw_bin->header_info[0].valid_data_addr;
	prof_desc->sec_desc[AW_PROFILE_DATA_TYPE_REG].len =
				aw_bin->header_info[0].valid_data_len;
	prof_desc->prof_st = AW_PROFILE_OK;

	kfree(aw_bin);
	aw_bin = NULL;

	return 0;

parse_bin_failed:
	kfree(aw_bin);
	aw_bin = NULL;
	return ret;
}

static int aw_dev_parse_data_by_sec_type(struct aw_device *aw_dev, struct aw_cfg_hdr *cfg_hdr,
			struct aw_cfg_dde *prof_hdr, struct aw_prof_desc *scene_prof_desc)
{
	switch (prof_hdr->data_type) {
	case ACF_SEC_TYPE_REG:
		return aw_dev_parse_raw_reg(aw_dev,
					(uint8_t *)cfg_hdr + prof_hdr->data_offset,
					prof_hdr->data_size,
					scene_prof_desc);
		break;
	case ACF_SEC_TYPE_HDR_REG:
		return aw_dev_parse_reg_bin_with_hdr(aw_dev,
					(uint8_t *)cfg_hdr + prof_hdr->data_offset,
					prof_hdr->data_size,
					scene_prof_desc);
		break;
	case ACF_SEC_TYPE_MONITOR:
		return aw_monitor_parse_fw(&aw_dev->monitor_desc,
				(uint8_t *)cfg_hdr + prof_hdr->data_offset,
				prof_hdr->data_size);
		break;
	}

	return 0;
}

static int aw_dev_parse_dev_type(struct aw_device *aw_dev,
		struct aw_cfg_hdr *prof_hdr, struct aw_all_prof_info *all_prof_info)
{
	int i = 0;
	int ret;
	int sec_num = 0;
	struct aw_cfg_dde *cfg_dde =
		(struct aw_cfg_dde *)((char *)prof_hdr + prof_hdr->a_hdr_offset);

	aw_dev_info(aw_dev->dev, "enter");

	for (i = 0; i < prof_hdr->a_ddt_num; i++) {
		if ((aw_dev->i2c->adapter->nr == cfg_dde[i].dev_bus) &&
			(aw_dev->i2c->addr == cfg_dde[i].dev_addr) &&
			(cfg_dde[i].type == AW_DEV_TYPE_ID)) {
			if (cfg_dde[i].data_type != ACF_SEC_TYPE_MONITOR) {
				ret = aw_dev_parse_data_by_sec_type(aw_dev, prof_hdr, &cfg_dde[i],
						&all_prof_info->prof_desc[cfg_dde[i].dev_profile]);
				if (ret < 0) {
					aw_dev_err(aw_dev->dev, "parse dev driver bin data failed");
					return ret;
				}
				sec_num++;
			} else {
				ret = aw_dev_parse_data_by_sec_type(aw_dev, prof_hdr, &cfg_dde[i], NULL);
				if (ret < 0) {
					aw_dev_err(aw_dev->dev, "parse monitor bin data failed");
					return ret;
				}
			}
		}
	}

	if (sec_num == 0) {
		aw_dev_info(aw_dev->dev, "get dev type num is %d, please use default", sec_num);
		return AW_DEV_TYPE_NONE;
	}

	return AW_DEV_TYPE_OK;
}

static int aw_dev_parse_dev_default_type(struct aw_device *aw_dev,
		struct aw_cfg_hdr *prof_hdr, struct aw_all_prof_info *all_prof_info)
{
	int i = 0;
	int ret;
	int sec_num = 0;
	struct aw_cfg_dde *cfg_dde =
		(struct aw_cfg_dde *)((char *)prof_hdr + prof_hdr->a_hdr_offset);

	aw_dev_info(aw_dev->dev, "enter");

	for (i = 0; i < prof_hdr->a_ddt_num; i++) {
		if ((aw_dev->index == cfg_dde[i].dev_index) &&
			(cfg_dde[i].type == AW_DEV_DEFAULT_TYPE_ID)) {
			if (cfg_dde[i].data_type != ACF_SEC_TYPE_MONITOR) {
				ret = aw_dev_parse_data_by_sec_type(aw_dev, prof_hdr, &cfg_dde[i],
						&all_prof_info->prof_desc[cfg_dde[i].dev_profile]);
				if (ret < 0) {
					aw_dev_err(aw_dev->dev, "parse dev driver bin data failed");
					return ret;
				}
				sec_num++;
			} else {
				ret = aw_dev_parse_data_by_sec_type(aw_dev, prof_hdr, &cfg_dde[i], NULL);
				if (ret < 0) {
					aw_dev_err(aw_dev->dev, "parse monitor bin data failed");
					return ret;
				}
			}
		}
	}

	if (sec_num == 0) {
		aw_dev_err(aw_dev->dev, "get dev default type failed, get num[%d]", sec_num);
		return -EINVAL;
	}

	return 0;
}

static int aw_dev_parse_skt_type(struct aw_device *aw_dev,
		struct aw_cfg_hdr *prof_hdr, struct aw_all_prof_info *all_prof_info)
{
	int i = 0;
	int ret;
	int sec_num = 0;
	struct aw_cfg_dde *cfg_dde =
		(struct aw_cfg_dde *)((char *)prof_hdr + prof_hdr->a_hdr_offset);

	aw_dev_info(aw_dev->dev, "enter");

	for (i = 0; i < prof_hdr->a_ddt_num; i++) {
		if ((aw_dev->index == cfg_dde[i].dev_index) &&
			(cfg_dde[i].type == AW_SKT_TYPE_ID)) {
			if (cfg_dde[i].data_type == ACF_SEC_TYPE_DSP) {
				ret = aw_dev_parse_raw_dsp(aw_dev,
					(uint8_t *)prof_hdr + cfg_dde[i].data_offset,
					cfg_dde[i].data_size,
					&all_prof_info->prof_desc[cfg_dde[i].dev_profile]);
				if (ret < 0) {
					aw_dev_err(aw_dev->dev, "parse dsp bin data failed");
					return ret;
				}
				sec_num++;
			}
		}
	}

	aw_dev_info(aw_dev->dev, "get dsp data prof cnt is %d ", sec_num);
	return 0;
}

static int aw_dev_acf_load_by_hdr(struct aw_device *aw_dev,
		struct aw_cfg_hdr *prof_hdr, struct aw_all_prof_info *all_prof_info)
{
	int ret;

	ret = aw_dev_parse_dev_type(aw_dev, prof_hdr, all_prof_info);
	if (ret < 0) {
		return ret;
	} else if (ret == AW_DEV_TYPE_NONE) {
		aw_dev_info(aw_dev->dev, "get dev type num is0, parse default dev type");
		ret = aw_dev_parse_dev_default_type(aw_dev, prof_hdr, all_prof_info);
		if (ret < 0)
			return ret;
	}

	ret = aw_dev_parse_skt_type(aw_dev, prof_hdr, all_prof_info);
	if (ret < 0)
		return ret;

	return 0;
}


static int aw_dev_cfg_get_vaild_prof(struct aw_device *aw_dev,
				struct aw_all_prof_info all_prof_info)
{
	int i;
	int num = 0;
	struct aw_prof_desc *prof_desc = all_prof_info.prof_desc;
	struct aw_prof_info *prof_info = &aw_dev->prof_info;

	for (i = 0; i < AW_PROFILE_MAX; i++) {
		if (prof_desc[i].prof_st == AW_PROFILE_OK)
			aw_dev->prof_info.count++;
	}

	aw_dev_info(aw_dev->dev, "get vaild profile:%d", aw_dev->prof_info.count);

	if (!aw_dev->prof_info.count) {
		aw_dev_err(aw_dev->dev, "no profile data");
		return -EPERM;
	}

	prof_info->prof_desc = kzalloc(prof_info->count * sizeof(struct aw_prof_desc), GFP_KERNEL);
	if (prof_info->prof_desc == NULL) {
		aw_dev_err(aw_dev->dev, "prof_desc kzalloc failed");
		return -ENOMEM;
	}

	for (i = 0; i < AW_PROFILE_MAX; i++) {
		if (prof_desc[i].prof_st == AW_PROFILE_OK) {
			if (num >= prof_info->count) {
				aw_dev_err(aw_dev->dev, "get scene num[%d] overflow count[%d]",
						num, prof_info->count);
				return -ENOMEM;
			}
			prof_info->prof_desc[num] = prof_desc[i];
			prof_info->prof_desc[num].id = i;
			num++;
		}
	}

	return 0;
}

static int aw_dev_cfg_load(struct aw_device *aw_dev, struct aw_container *aw_cfg)
{
	struct aw_cfg_hdr *cfg_hdr = NULL;
	struct aw_all_prof_info all_prof_info;
	int ret;

	aw_dev_info(aw_dev->dev, "enter");
	memset(&all_prof_info, 0, sizeof(struct aw_all_prof_info));

	cfg_hdr = (struct aw_cfg_hdr *)aw_cfg->data;
	switch (cfg_hdr->a_hdr_version) {
	case AW_CFG_HDR_VER_0_0_0_1:
		ret = aw_dev_acf_load_by_hdr(aw_dev, cfg_hdr, &all_prof_info);
		if (ret < 0) {
			aw_dev_err(aw_dev->dev, "hdr_cersion[0x%x] parse failed",
						cfg_hdr->a_hdr_version);
			return ret;
		}
		break;
	default:
		aw_pr_err("unsupported hdr_version [0x%x]", cfg_hdr->a_hdr_version);
		return -EINVAL;
	}

	ret = aw_dev_cfg_get_vaild_prof(aw_dev, all_prof_info);
	if (ret < 0)
		return ret;

	aw_dev_info(aw_dev->dev, "parse cfg success");
	return 0;
}

static struct aw_sec_data_desc *aw_dev_get_prof_data(struct aw_device *aw_dev, int index, int data_type)
{
	struct aw_sec_data_desc *sec_data = NULL;
	struct aw_prof_desc *prof_desc = NULL;

	if (index >= aw_dev->prof_info.count) {
		aw_dev_err(aw_dev->dev, "index[%d] overflow count[%d]",
				index, aw_dev->prof_info.count);
		return NULL;
	}

	prof_desc = &aw_dev->prof_info.prof_desc[index];

	if (data_type >= AW_PROFILE_DATA_TYPE_MAX) {
		aw_dev_err(aw_dev->dev, "unsupport data type id [%d]", data_type);
		return NULL;
	}

	sec_data = &prof_desc->sec_desc[data_type];

	aw_dev_dbg(aw_dev->dev, "get prof[%s] data len[%d]", profile_name[prof_desc->id], sec_data->len);

	return sec_data;
}


/*****************************awinic device*************************************/
static char *aw_dev_get_prof_name(struct aw_device *aw_dev, int index)
{
	struct aw_prof_desc *prof_desc = NULL;

	if (index < 0) {
		aw_dev_err(aw_dev->dev, "index[%d] error", index);
		return NULL;
	}

	if (index >= aw_dev->prof_info.count) {
		aw_dev_err(aw_dev->dev, "index[%d] overflow count[%d]",
			index, aw_dev->prof_info.count);
		return NULL;
	}

	prof_desc = &aw_dev->prof_info.prof_desc[index];

	return profile_name[prof_desc->id];
}

/*static int aw_dev_dsp_fw_update(struct aw_device *aw_dev);
{
	int  ret;
	struct aw_sec_data_desc *dsp_data;

	char *prof_name = aw_dev_get_prof_name(aw_dev, aw_dev->set_prof);

	if (prof_name == NULL) {
		aw_dev_err(aw_dev->dev, "get prof name failed");
		return -EINVAL;
	}

	dsp_data = aw_dev_get_prof_data(aw_dev,
		aw_dev->set_prof, AW_PROFILE_DATA_TYPE_DSP);
	if (dsp_data == NULL ||
		dsp_data->data == NULL ||
			dsp_data->len == 0) {
		aw_dev_info(aw_dev->dev, "dsp data is NULL");
		return 0;
	}

	mutex_lock(&g_ext_dsp_prof_wr_lock);
	if (ext_dsp_prof_write == AW_EXT_DSP_WRITE_NONE) {
		ret = aw_dsp_write_params(aw_dev, dsp_data->data, dsp_data->len);
		if (ret) {
			aw_dev_err(aw_dev->dev, "dsp params update failed !");
			mutex_unlock(&g_ext_dsp_prof_wr_lock);
			return ret;
		}
		ext_dsp_prof_write = AW_EXT_DSP_WRITE;
	} else {
		aw_dev_dbg(aw_dev->dev, "dsp params already update !");
	}
	mutex_unlock(&g_ext_dsp_prof_wr_lock);

	aw_dev_info(aw_dev->dev, "load %s done", prof_name);
	return 0;
}*/

/*pwd enable update reg*/
static int aw_dev_reg_fw_update(struct aw_device *aw_dev)
{
	int i = 0;
	unsigned int reg_addr = 0;
	unsigned int reg_val = 0;
	unsigned int read_val;
	int ret = -1;
	unsigned int init_volume = 0;
	struct aw_int_desc *int_desc = &aw_dev->int_desc;
	struct aw_profctrl_desc *profctrl_desc = &aw_dev->profctrl_desc;
	struct aw_bstctrl_desc *bstctrl_desc = &aw_dev->bstctrl_desc;
	struct aw_sec_data_desc *reg_data;
	int16_t *data;
	int data_len;

	char *prof_name = aw_dev_get_prof_name(aw_dev, aw_dev->set_prof);

	if (prof_name == NULL) {
		aw_dev_err(aw_dev->dev, "get prof name failed");
		return -EINVAL;
	}

	reg_data = aw_dev_get_prof_data(aw_dev, aw_dev->set_prof, AW_PROFILE_DATA_TYPE_REG);
	if (reg_data == NULL)
		return -EINVAL;

	data = (int16_t *)reg_data->data;
	data_len = reg_data->len >> 1;

	for (i = 0; i < data_len; i += 2) {
		reg_addr = data[i];
		reg_val = data[i + 1];

		if (reg_addr == int_desc->mask_reg) {
			int_desc->int_mask = reg_val;
			reg_val = int_desc->mask_default;
		}

		if (aw_dev->bstcfg_enable) {
			if (reg_addr == profctrl_desc->reg) {
				profctrl_desc->cfg_prof_mode =
					reg_val & (~profctrl_desc->mask);
			}

			if (reg_addr == bstctrl_desc->reg) {
				bstctrl_desc->cfg_bst_type =
					reg_val & (~bstctrl_desc->mask);
			}
		}

		/*keep amppd status*/
		if (reg_addr == aw_dev->amppd_desc.reg) {
			aw_dev->amppd_st = reg_val & (~aw_dev->amppd_desc.mask);
			aw_dev_info(aw_dev->dev, "amppd_st=0x%04x", aw_dev->amppd_st);
			aw_dev->ops.aw_i2c_read(aw_dev,
			(unsigned char)reg_addr,
			(unsigned int *)&read_val);
			read_val &= (~aw_dev->amppd_desc.mask);
			reg_val &= aw_dev->amppd_desc.mask;
			reg_val |= read_val;
		}

		/*keep pwd status*/
		if (reg_addr == aw_dev->pwd_desc.reg) {
			aw_dev->ops.aw_i2c_read(aw_dev,
			(unsigned char)reg_addr,
			(unsigned int *)&read_val);
			read_val &= (~aw_dev->pwd_desc.mask);
			reg_val &= aw_dev->pwd_desc.mask;
			reg_val |= read_val;
		}
		/*keep mute status*/
		if (reg_addr == aw_dev->mute_desc.reg) {
			/*get bin value*/
			aw_dev->mute_st = reg_val & (~aw_dev->mute_desc.mask);
			aw_dev_info(aw_dev->dev, "mute_st=0x%04x", aw_dev->mute_st);
			aw_dev->ops.aw_i2c_read(aw_dev,
			(unsigned char)reg_addr,
			(unsigned int *)&read_val);
			read_val &= (~aw_dev->mute_desc.mask);
			reg_val &= aw_dev->mute_desc.mask;
			reg_val |= read_val;
		}

		if (reg_addr == aw_dev->vcalb_desc.vcalb_reg)
			continue;

		aw_dev_dbg(aw_dev->dev, "reg=0x%04x, val = 0x%04x",
			(uint16_t)reg_addr, (uint16_t)reg_val);
		ret = aw_dev->ops.aw_i2c_write(aw_dev,
			(unsigned char)reg_addr,
			(unsigned int)reg_val);
		if (ret < 0)
			break;
	}

	aw_dev->ops.aw_get_volume(aw_dev, &init_volume);
	aw_dev->volume_desc.init_volume = init_volume;

	/*keep min volume*/
	aw_dev->ops.aw_set_volume(aw_dev, aw_dev->volume_desc.mute_volume);

	aw_dev_info(aw_dev->dev, "load %s done", prof_name);

	return ret;
}

static void aw_dev_fade_in(struct aw_device *aw_dev)
{
	int i = 0;
	int fade_step = aw_dev->vol_step;
	struct aw_volume_desc *desc = &aw_dev->volume_desc;

	if (fade_step == 0 || g_fade_in_time == 0) {
		aw_dev->ops.aw_set_volume(aw_dev, desc->init_volume);
		return;
	}
	/*volume up*/
	for (i = desc->mute_volume; i >= desc->init_volume; i -= fade_step) {
		if (i < desc->init_volume)
			i = desc->init_volume;
		aw_dev->ops.aw_set_volume(aw_dev, i);
		usleep_range(g_fade_in_time, g_fade_in_time + 10);
	}
	if (i != desc->init_volume)
		aw_dev->ops.aw_set_volume(aw_dev, desc->init_volume);
}

static void aw_dev_fade_out(struct aw_device *aw_dev)
{
	int i = 0;
	unsigned start_volume = 0;
	int fade_step = aw_dev->vol_step;
	struct aw_volume_desc *desc = &aw_dev->volume_desc;

	if (fade_step == 0 || g_fade_out_time == 0) {
		aw_dev->ops.aw_set_volume(aw_dev, desc->mute_volume);
		return;
	}

	aw_dev->ops.aw_get_volume(aw_dev, &start_volume);
	i = start_volume;
	for (i = start_volume; i <= desc->mute_volume; i += fade_step) {
		if (i > desc->mute_volume)
			i = desc->mute_volume;
		aw_dev->ops.aw_set_volume(aw_dev, i);
		usleep_range(g_fade_out_time, g_fade_out_time + 10);
	}
	if (i != desc->mute_volume) {
		aw_dev->ops.aw_set_volume(aw_dev, desc->mute_volume);
		usleep_range(g_fade_out_time, g_fade_out_time + 10);
	}
}

static void aw_dev_pwd(struct aw_device *aw_dev, bool pwd)
{
	struct aw_pwd_desc *pwd_desc = &aw_dev->pwd_desc;

	aw_dev_dbg(aw_dev->dev, "enter");

	if (pwd) {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, pwd_desc->reg,
				pwd_desc->mask,
				pwd_desc->enable);
	} else {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, pwd_desc->reg,
				pwd_desc->mask,
				pwd_desc->disable);
	}
	aw_dev_info(aw_dev->dev, "done");
}

static void aw_dev_amppd(struct aw_device *aw_dev, bool amppd)
{
	struct aw_amppd_desc *amppd_desc = &aw_dev->amppd_desc;

	aw_dev_dbg(aw_dev->dev, "enter");

	if (amppd) {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, amppd_desc->reg,
				amppd_desc->mask,
				amppd_desc->enable);
	} else {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, amppd_desc->reg,
				amppd_desc->mask,
				amppd_desc->disable);
	}
	aw_dev_info(aw_dev->dev, "done");
}

static void aw_dev_mute(struct aw_device *aw_dev, bool mute)
{
	struct aw_mute_desc *mute_desc = &aw_dev->mute_desc;

	aw_dev_dbg(aw_dev->dev, "enter");

	if (mute) {
		aw_dev_fade_out(aw_dev);
		aw_dev->ops.aw_i2c_write_bits(aw_dev, mute_desc->reg,
				mute_desc->mask,
				mute_desc->enable);
	} else {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, mute_desc->reg,
				mute_desc->mask,
				mute_desc->disable);
		aw_dev_fade_in(aw_dev);
	}
	aw_dev_info(aw_dev->dev, "done");
}

static int aw_dev_get_icalk(struct aw_device *aw_dev, int16_t *icalk)
{
	int ret = -1;
	unsigned int reg_val = 0;
	uint16_t reg_icalk = 0;
	uint16_t reg_icalkl = 0;
	struct aw_vcalb_desc *desc = &aw_dev->vcalb_desc;

	if (desc->icalkl_reg == AW_REG_NONE) {
		ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->icalk_reg, &reg_val);
		reg_icalk = (uint16_t)reg_val & (~desc->icalk_reg_mask);
	} else {
		ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->icalk_reg, &reg_val);
		reg_icalk = (uint16_t)reg_val & (~desc->icalk_reg_mask);
		ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->icalkl_reg, &reg_val);
		reg_icalkl = (uint16_t)reg_val & (~desc->icalkl_reg_mask);
		reg_icalk = (reg_icalk >> desc->icalk_shift) | (reg_icalkl >> desc->icalkl_shift);
	}

	if (reg_icalk & (~desc->icalk_sign_mask))
		reg_icalk = reg_icalk | (~desc->icalk_neg_mask);

	*icalk = (int16_t)reg_icalk;

	return ret;
}

static int aw_dev_get_vcalk(struct aw_device *aw_dev, int16_t *vcalk)
{
	int ret = -1;
	unsigned int reg_val = 0;
	uint16_t reg_vcalk = 0;
	uint16_t reg_vcalkl = 0;
	struct aw_vcalb_desc *desc = &aw_dev->vcalb_desc;

	if (desc->vcalkl_reg == AW_REG_NONE) {
		ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->vcalk_reg, &reg_val);
		reg_vcalk = (uint16_t)reg_val & (~desc->vcalk_reg_mask);
	} else {
		ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->vcalk_reg, &reg_val);
		reg_vcalk = (uint16_t)reg_val & (~desc->vcalk_reg_mask);
		ret = aw_dev->ops.aw_i2c_read(aw_dev, desc->vcalkl_reg, &reg_val);
		reg_vcalkl = (uint16_t)reg_val & (~desc->vcalkl_reg_mask);
		reg_vcalk = (reg_vcalk >> desc->vcalk_shift) | (reg_vcalkl >> desc->vcalkl_shift);
	}

	if (reg_vcalk & (~desc->vcalk_sign_mask))
		reg_vcalk = reg_vcalk | (~desc->vcalk_neg_mask);

	*vcalk = (int16_t)reg_vcalk;

	return ret;
}

static int aw_dev_set_vcalb(struct aw_device *aw_dev)
{
	int ret = -1;
	unsigned int reg_val;
	int vcalb;
	int icalk;
	int vcalk;
	int16_t icalk_val = 0;
	int16_t vcalk_val = 0;

	struct aw_vcalb_desc *desc = &aw_dev->vcalb_desc;

	if (desc->icalk_reg == AW_REG_NONE || desc->vcalb_reg == AW_REG_NONE) {
		aw_dev_info(aw_dev->dev, "REG None!");
		return 0;
	}

	ret = aw_dev_get_icalk(aw_dev, &icalk_val);
	if (ret < 0)
		return ret;

	ret = aw_dev_get_vcalk(aw_dev, &vcalk_val);
	if (ret < 0)
		return ret;

	icalk = desc->cabl_base_value + desc->icalk_value_factor * icalk_val;
	vcalk = desc->cabl_base_value + desc->vcalk_value_factor * vcalk_val;
	if (!vcalk) {
		aw_dev_err(aw_dev->dev, "vcalk is 0");
		return -EINVAL;
	}

	vcalb = desc->vcal_factor * icalk / vcalk;

	reg_val = (unsigned int)vcalb;
	aw_dev_info(aw_dev->dev, "icalk=%d, vcalk=%d, vcalb=%d, reg_val=0x%04x",
			icalk, vcalk, vcalb, reg_val);

	ret =  aw_dev->ops.aw_i2c_write(aw_dev, desc->vcalb_reg, reg_val);

	aw_dev_info(aw_dev->dev, "done");

	return ret;
}

int aw_dev_get_int_status(struct aw_device *aw_dev, uint16_t *int_status)
{
	int ret = -1;
	unsigned int reg_val = 0;

	ret = aw_dev->ops.aw_i2c_read(aw_dev, aw_dev->int_desc.st_reg, &reg_val);
	if (ret < 0)
		aw_dev_err(aw_dev->dev, "read interrupt reg fail, ret=%d", ret);
	else
		*int_status = reg_val;

	aw_dev_dbg(aw_dev->dev, "read interrupt reg = 0x%04x", *int_status);
	return ret;
}

void aw_dev_clear_int_status(struct aw_device *aw_dev)
{
	uint16_t int_status = 0;

	/*read int status and clear*/
	aw_dev_get_int_status(aw_dev, &int_status);
	/*make suer int status is clear*/
	aw_dev_get_int_status(aw_dev, &int_status);
	aw_dev_info(aw_dev->dev, "done");
}

int aw_dev_set_intmask(struct aw_device *aw_dev, bool flag)
{
	struct aw_int_desc *desc = &aw_dev->int_desc;
	int ret = -1;

	if (flag)
		ret = aw_dev->ops.aw_i2c_write(aw_dev, desc->mask_reg,
					desc->int_mask);
	else
		ret = aw_dev->ops.aw_i2c_write(aw_dev, desc->mask_reg,
					desc->mask_default);
	aw_dev_info(aw_dev->dev, "done");
	return ret;
}

static int aw_dev_mode1_pll_check(struct aw_device *aw_dev)
{
	int ret = -1;
	unsigned char i;
	unsigned int reg_val = 0;
	struct aw_sysst_desc *desc = &aw_dev->sysst_desc;

	for (i = 0; i < AW_DEV_SYSST_CHECK_MAX; i++) {
		aw_dev->ops.aw_i2c_read(aw_dev, desc->reg, &reg_val);
		if (reg_val & desc->pll_check) {
			ret = 0;
			break;
		} else {
			aw_dev_dbg(aw_dev->dev, "check pll lock fail, cnt=%d, reg_val=0x%04x",
					i, reg_val);
			usleep_range(AW_2000_US, AW_2000_US + 10);
		}
	}
	if (ret < 0)
		aw_dev_err(aw_dev->dev, "check fail");
	else
		aw_dev_info(aw_dev->dev, "done");

	return ret;
}

static int aw_dev_mode2_pll_check(struct aw_device *aw_dev)
{
	int ret = -1;
	unsigned int reg_val = 0;
	struct aw_cco_mux_desc *cco_mux_desc = &aw_dev->cco_mux_desc;

	aw_dev->ops.aw_i2c_read(aw_dev, cco_mux_desc->reg, &reg_val);
	reg_val &= (~cco_mux_desc->mask);
	aw_dev_dbg(aw_dev->dev, "REG_PLLCTRL1_bit14 = 0x%04x", reg_val);
	if (reg_val == cco_mux_desc->divided_val) {
		aw_dev_dbg(aw_dev->dev, "CCO_MUX is already divided");
		return ret;
	}

	/* change mode2 */
	aw_dev->ops.aw_i2c_write_bits(aw_dev, cco_mux_desc->reg,
				cco_mux_desc->mask, cco_mux_desc->bypass_val);
	ret = aw_dev_mode1_pll_check(aw_dev);

	/* change mode1 */
	aw_dev->ops.aw_i2c_write_bits(aw_dev, cco_mux_desc->reg,
				cco_mux_desc->mask, cco_mux_desc->divided_val);
	if (ret == 0) {
		usleep_range(AW_2000_US, AW_2000_US + 10);
		ret = aw_dev_mode1_pll_check(aw_dev);
	}

	return ret;
}

static int aw_dev_syspll_check(struct aw_device *aw_dev)
{
	int ret = -1;

	ret = aw_dev_mode1_pll_check(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev,
			"mode1 check iis failed try switch to mode2 check");

		ret= aw_dev_mode2_pll_check(aw_dev);
		if (ret < 0)
			aw_dev_err(aw_dev->dev, "mode2 check iis failed");
	}

	return ret;
}

static int aw_dev_sysst_check(struct aw_device *aw_dev)
{
	int ret = -1;
	unsigned char i;
	unsigned int reg_val = 0;
	struct aw_sysst_desc *desc = &aw_dev->sysst_desc;

	for (i = 0; i < AW_DEV_SYSST_CHECK_MAX; i++) {
		aw_dev->ops.aw_i2c_read(aw_dev, desc->reg, &reg_val);
		if (((reg_val & (~desc->mask)) & desc->st_check) == desc->st_check) {
			ret = 0;
			break;
		} else {
			aw_dev_info(aw_dev->dev, "check fail, cnt=%d, reg_val=0x%04x",
					i, reg_val);
			usleep_range(AW_2000_US, AW_2000_US + 10);
		}
	}
	if (ret < 0)
		aw_dev_err(aw_dev->dev, "check fail");
	else
		aw_dev_info(aw_dev->dev, "done");

	return ret;
}

int aw_dev_get_profile_count(struct aw_device *aw_dev)
{
	if (aw_dev == NULL) {
		aw_dev_err(aw_dev->dev, "aw_dev is NULL");
		return -ENOMEM;
	}

	return aw_dev->prof_info.count;
}

int aw_dev_get_profile_name(struct aw_device *aw_dev, char *name, int index)
{
	int dev_profile_id;

	if (index < 0) {
		aw_dev_err(aw_dev->dev, "index[%d] error", index);
		return -EINVAL;
	}

	if (index > aw_dev->prof_info.count) {
		aw_dev_err(aw_dev->dev, "index[%d] overflow dev prof num[%d]",
				index, aw_dev->prof_info.count);
		return -EINVAL;
	}

	if (aw_dev->prof_info.prof_desc[index].id >= AW_PROFILE_MAX) {
		aw_dev_err(aw_dev->dev, "can not find match id ");
		return -EINVAL;
	}

	dev_profile_id = aw_dev->prof_info.prof_desc[index].id;

	strlcpy(name, profile_name[dev_profile_id],
			strlen(profile_name[dev_profile_id]) + 1);
	/*aw_dev_dbg(aw_dev->dev, "%s: get name [%s]",
			__func__, profile_name[dev_profile_id]);*/
	return 0;
}

int aw_dev_check_profile_index(struct aw_device *aw_dev, int index)
{
	if ((index >= aw_dev->prof_info.count) || (index < 0))
		return -EINVAL;
	else
		return 0;
}

int aw_dev_get_profile_index(struct aw_device *aw_dev)
{
	return aw_dev->set_prof;
}

int aw_dev_set_profile_index(struct aw_device *aw_dev, int index)
{
	if (index >= aw_dev->prof_info.count || index < 0) {
		return -EINVAL;
	} else {
		aw_dev->set_prof = index;
		aw_dev_info(aw_dev->dev, "set prof[%s]",
			profile_name[aw_dev->prof_info.prof_desc[index].id]);
	}
	mutex_lock(&g_ext_dsp_prof_wr_lock);
	ext_dsp_prof_write = AW_EXT_DSP_WRITE_NONE;
	mutex_unlock(&g_ext_dsp_prof_wr_lock);

	return 0;
}

int aw_dev_get_fade_vol_step(struct aw_device *aw_dev)
{
	return aw_dev->vol_step;
}

void aw_dev_set_fade_vol_step(struct aw_device *aw_dev, unsigned int step)
{
	aw_dev->vol_step = step;
}

void aw_dev_get_fade_time(unsigned int *time, bool fade_in)
{
	if (fade_in)
		*time = g_fade_in_time;
	else
		*time = g_fade_out_time;
}

void aw_dev_set_fade_time(unsigned int time, bool fade_in)
{
	if (fade_in)
		g_fade_in_time = time;
	else
		g_fade_out_time = time;
}

/*init aw_device*/
void aw_dev_deinit(struct aw_device *aw_dev)
{
	if (aw_dev == NULL)
		return;

	if (aw_dev->prof_info.prof_desc != NULL) {
		kfree(aw_dev->prof_info.prof_desc);
		aw_dev->prof_info.prof_desc = NULL;
		aw_dev->prof_info.count = 0;
	}
}

int aw_dev_get_cali_re(struct aw_device *aw_dev, int32_t *cali_re)
{

	return aw_dsp_read_cali_re(aw_dev, cali_re);
}

int aw_dev_dc_status(struct aw_device *aw_dev)
{
	return aw_dsp_get_dc_status(aw_dev);
}

int aw_dev_status(struct aw_device *aw_dev)
{
	return aw_dev->status;
}

int aw_dev_init_cali_re(struct aw_device *aw_dev)
{
	int ret = 0;

	if (aw_dev->cali_desc.mode) {
		if (aw_dev->cali_desc.cali_re == AW_ERRO_CALI_VALUE) {
			ret = aw_cali_read_re_from_nvram(&aw_dev->cali_desc.cali_re, aw_dev->channel);
			if (ret) {
				aw_dev_info(aw_dev->dev, "read nvram cali failed, use default Re");
				aw_dev->cali_desc.cali_re = AW_ERRO_CALI_VALUE;
			}
		}
	} else {
		aw_dev_info(aw_dev->dev, "no cali, needn't init cali re");
	}
	return ret;
}

static void aw_dev_soft_reset(struct aw_device *aw_dev)
{
	struct aw_soft_rst *reset = &aw_dev->soft_rst;

	aw_dev->ops.aw_i2c_write(aw_dev, reset->reg, reset->reg_value);
	aw_dev_info(aw_dev->dev, "soft reset done");
}

int aw_device_irq_reinit(struct aw_device *aw_dev)
{
	int ret;

	/*reg re load*/
	ret = aw_dev_reg_fw_update(aw_dev);
	if (ret < 0)
		return ret;

	/*update vcalb*/
	aw_dev_set_vcalb(aw_dev);

	return 0;
}

int aw_device_init(struct aw_device *aw_dev, struct aw_container *aw_cfg)
{
	/*acf_hdr_t *hdr;*/
	int ret;

	if (aw_dev == NULL || aw_cfg == NULL) {
		aw_dev_err(aw_dev->dev, "pointer is NULL");
		return -ENOMEM;
	}

	ret = aw_dev_cfg_load(aw_dev, aw_cfg);
	if (ret) {
		aw_dev_deinit(aw_dev);
		aw_dev_err(aw_dev->dev, "aw_dev acf load failed");
		return -EINVAL;
	}

	aw_dev_soft_reset(aw_dev);

	aw_dev->cur_prof = AW_PROFILE_MUSIC;
	aw_dev->set_prof = AW_PROFILE_MUSIC;
	ret = aw_dev_reg_fw_update(aw_dev);
	if (ret < 0)
		return ret;

	ret = aw_dev_set_vcalb(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "can't set vcalb");
		return ret;
	}

	aw_dev->status = AW_DEV_PW_ON;
	aw_device_stop(aw_dev);

	aw_dev_info(aw_dev->dev, "init done");
	return 0;
}

int aw_dev_reg_update(struct aw_device *aw_dev, bool force)
{
	int ret;

	if (force) {
		aw_dev_soft_reset(aw_dev);
		ret = aw_dev_reg_fw_update(aw_dev);
		if (ret < 0)
			return ret;
	} else {
		if (aw_dev->cur_prof != aw_dev->set_prof) {
			ret = aw_dev_reg_fw_update(aw_dev);
			if (ret < 0)
				return ret;
		}
	}

	aw_dev->cur_prof = aw_dev->set_prof;

	aw_dev_info(aw_dev->dev, "done");
	return 0;
}

static void aw_dev_cali_re_update(struct aw_device *aw_dev)
{
	struct aw_cali_desc *desc = &aw_dev->cali_desc;

	if (desc->mode) {
		if ((desc->cali_re >= aw_dev->re_min) &&
				(desc->cali_re <= aw_dev->re_max))
			aw_dsp_write_cali_re(aw_dev, desc->cali_re);
	}
}

int aw_dev_prof_update(struct aw_device *aw_dev, bool force)
{
	int ret;

	/*if power on need off -- load -- on*/
	if (aw_dev->status == AW_DEV_PW_ON) {
		aw_device_stop(aw_dev);

		ret = aw_dev_reg_update(aw_dev, force);
		if (ret) {
			aw_dev_err(aw_dev->dev, "fw update failed ");
			return ret;
		}

		ret = aw_device_start(aw_dev);
		if (ret) {
			aw_dev_err(aw_dev->dev, "start failed ");
			return ret;
		}
	} else {
		/*if pa off , only update set_prof value*/
		aw_dev_info(aw_dev->dev, "set prof[%d] done !", aw_dev->set_prof);
	}

	aw_dev_info(aw_dev->dev, "update done !");
	return 0;
}

static void aw_dev_boost_type_set(struct aw_device *aw_dev)
{
	struct aw_profctrl_desc *profctrl_desc = &aw_dev->profctrl_desc;
	struct aw_bstctrl_desc *bstctrl_desc = &aw_dev->bstctrl_desc;

	aw_dev_dbg(aw_dev->dev, "enter");

	if (aw_dev->bstcfg_enable) {
		/*set spk mode*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, profctrl_desc->reg,
				profctrl_desc->mask, profctrl_desc->spk_mode);

		/*force boost*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, bstctrl_desc->reg,
				bstctrl_desc->mask, bstctrl_desc->frc_bst);

		aw_dev_dbg(aw_dev->dev, "boost type set done");
	}
}

static void aw_dev_boost_type_recover(struct aw_device *aw_dev)
{

	struct aw_profctrl_desc *profctrl_desc = &aw_dev->profctrl_desc;
	struct aw_bstctrl_desc *bstctrl_desc = &aw_dev->bstctrl_desc;

	aw_dev_dbg(aw_dev->dev, "enter");

	if (aw_dev->bstcfg_enable) {
		/*set transprant*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, bstctrl_desc->reg,
				bstctrl_desc->mask, bstctrl_desc->tsp_type);

		usleep_range(AW_5000_US, AW_5000_US + 50);
		/*set cfg boost type*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, bstctrl_desc->reg,
				bstctrl_desc->mask, bstctrl_desc->cfg_bst_type);

		/*set cfg prof mode*/
		aw_dev->ops.aw_i2c_write_bits(aw_dev, profctrl_desc->reg,
				profctrl_desc->mask, profctrl_desc->cfg_prof_mode);

		aw_dev_dbg(aw_dev->dev, "boost type recover done");
	}
}

static void aw_dev_set_frcpwm_mode(struct aw_device *aw_dev)
{
	struct aw_frcpwm_desc *frcpwm_desc = &aw_dev->frcpwm_desc;

	aw_dev_dbg(aw_dev->dev, "enter");

	aw_dev->ops.aw_i2c_write_bits(aw_dev, frcpwm_desc->reg,
			frcpwm_desc->mask, frcpwm_desc->frcpwm_val);

	aw_dev_dbg(aw_dev->dev, "frcpwm_mode set done");
}

int aw_device_start(struct aw_device *aw_dev)
{
	int ret;

	aw_dev_dbg(aw_dev->dev, "enter");

	if (aw_dev->status == AW_DEV_PW_ON) {
		aw_dev_info(aw_dev->dev, "already power on");
		return 0;
	}

	/*set froce boost*/
	aw_dev_boost_type_set(aw_dev);

	/*power on*/
	aw_dev_pwd(aw_dev, false);
	usleep_range(AW_2000_US, AW_2000_US + 10);

	ret = aw_dev_syspll_check(aw_dev);
	if (ret < 0) {
		aw_dev_reg_dump(aw_dev);
		aw_dev_pwd(aw_dev, true);
		aw_dev_dbg(aw_dev->dev, "pll check failed cannot start");
		return ret;
	}

	/*amppd on*/
	aw_dev_amppd(aw_dev, false);
	usleep_range(AW_1000_US, AW_1000_US + 50);

	/*check i2s status*/
	ret = aw_dev_sysst_check(aw_dev);
	if (ret < 0) {
		aw_dev_reg_dump(aw_dev);
		/*close tx feedback*/
		if (aw_dev->ops.aw_i2s_enable)
			aw_dev->ops.aw_i2s_enable(aw_dev, false);
		/*clear interrupt*/
		aw_dev_clear_int_status(aw_dev);
		/*close amppd*/
		aw_dev_amppd(aw_dev, true);
		/*power down*/
		aw_dev_pwd(aw_dev, true);
		return -EINVAL;
	}

	/*boost type recover*/
	aw_dev_boost_type_recover(aw_dev);

	/*enable tx feedback*/
	if (aw_dev->ops.aw_i2s_enable)
		aw_dev->ops.aw_i2s_enable(aw_dev, true);

	if (aw_dev->amppd_st) {
		aw_dev_amppd(aw_dev, true);
	}

	if (aw_dev->frcpwm_en)
		aw_dev_set_frcpwm_mode(aw_dev);

	if (!aw_dev->mute_st) {
		/*close mute*/
		aw_dev_mute(aw_dev, false);
	}

	/*clear inturrupt*/
	aw_dev_clear_int_status(aw_dev);
	/*set inturrupt mask*/
	aw_dev_set_intmask(aw_dev, true);

	aw_monitor_start(&aw_dev->monitor_desc);
	aw_dev_cali_re_update(aw_dev);

	aw_dev->status = AW_DEV_PW_ON;
	aw_dev_info(aw_dev->dev, "done");
	return 0;
}

int aw_device_stop(struct aw_device *aw_dev)
{
	aw_dev_dbg(aw_dev->dev, "enter");

	if (aw_dev->status == AW_DEV_PW_OFF) {
		aw_dev_dbg(aw_dev->dev, "already power off");
		return 0;
	}

	aw_dev->status = AW_DEV_PW_OFF;

	aw_monitor_stop(&aw_dev->monitor_desc);
	/*clear interrupt*/
	aw_dev_clear_int_status(aw_dev);

	/*set defaut int mask*/
	aw_dev_set_intmask(aw_dev, false);

	/*set mute*/
	aw_dev_mute(aw_dev, true);

	/*close tx feedback*/
	if (aw_dev->ops.aw_i2s_enable)
		aw_dev->ops.aw_i2s_enable(aw_dev, false);

	usleep_range(AW_1000_US, AW_1000_US + 100);

	/*enable amppd*/
	aw_dev_amppd(aw_dev, true);

	/*set power down*/
	aw_dev_pwd(aw_dev, true);

	ext_dsp_prof_write = AW_EXT_DSP_WRITE_NONE;
	aw_dev_info(aw_dev->dev, "done");
	return 0;
}

int aw_dev_set_afe_module_en(int type, int enable)
{
	return aw_dsp_set_afe_module_en(type, enable);
}

int aw_dev_get_afe_module_en(int type, int *status)
{
	return aw_dsp_get_afe_module_en(type, status);
}

int aw_dev_set_copp_module_en(bool enable)
{
	return aw_dsp_set_copp_module_en(enable);
}

int aw_dev_set_spin(int spin_mode)
{
	return aw_dsp_write_spin(spin_mode);
}

int aw_dev_get_spin(int *spin_mode)
{
	return aw_dsp_read_spin(spin_mode);
}

static void aw_device_parse_sound_channel_dt(struct aw_device *aw_dev)
{
	int ret;
	uint32_t channel_value;

	aw_dev->channel = AW_DEV_CH_PRI_L;
	ret = of_property_read_u32(aw_dev->dev->of_node, "sound-channel", &channel_value);
	if (ret < 0) {
		aw_dev_info(aw_dev->dev, "read sound-channel failed,use default");
		return;
	}

	aw_dev_dbg(aw_dev->dev, "read sound-channel value is : %d", channel_value);
	if (channel_value >= AW_DEV_CH_MAX) {
		channel_value = AW_DEV_CH_PRI_L;
	}
	aw_dev->channel = channel_value;
}

static void aw_device_parse_dt(struct aw_device *aw_dev)
{
	aw_device_parse_sound_channel_dt(aw_dev);
	aw_device_parse_topo_id_dt(aw_dev);
	aw_device_parse_port_id_dt(aw_dev);
}

int aw_dev_get_list_head(struct list_head **head)
{
	if (list_empty(&g_dev_list))
		return -EINVAL;

	*head = &g_dev_list;

	return 0;
}

int aw_device_probe(struct aw_device *aw_dev)
{
	int ret;
	INIT_LIST_HEAD(&aw_dev->list_node);

	aw_device_parse_dt(aw_dev);
	ret = aw_cali_init(&aw_dev->cali_desc);
	if (ret)
		return ret;

	aw_monitor_init(&aw_dev->monitor_desc);
	/*aw_afe_init();*/

	mutex_lock(&g_dev_lock);
	list_add(&aw_dev->list_node, &g_dev_list);
	mutex_unlock(&g_dev_lock);

	return 0;
}

int aw_device_remove(struct aw_device *aw_dev)
{
	aw_monitor_deinit(&aw_dev->monitor_desc);
	aw_cali_deinit(&aw_dev->cali_desc);
	/*aw_afe_deinit();*/
	return 0;
}

