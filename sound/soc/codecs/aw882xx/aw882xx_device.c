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


#include "aw882xx_log.h"
#include "aw882xx_device.h"
#include "aw882xx_dsp.h"
/*#include "aw_afe.h"*/
#include "aw882xx_bin_parse.h"
#include "aw882xx_spin.h"

#define AW_DEV_SYSST_CHECK_MAX   (10)


char ext_dsp_prof_write = AW_EXT_DSP_WRITE_NONE;
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

char *aw882xx_dev_get_ext_dsp_prof_write(void)
{
	return (&ext_dsp_prof_write);
}

struct mutex *aw882xx_dev_get_ext_dsp_prof_wr_lock(void)
{
	return (&g_ext_dsp_prof_wr_lock);
}

/*
static int aw_dev_dsp_fw_update(struct aw_device *aw_dev)
{
	int  ret;
	struct aw_sec_data_desc *dsp_data;

	char *prof_name = aw882xx_dev_get_prof_name(aw_dev, aw_dev->set_prof);

	if (prof_name == NULL) {
		aw_dev_err(aw_dev->dev, "get prof name failed");
		return -EINVAL;
	}

	dsp_data = aw882xx_dev_get_prof_data(aw_dev,
		aw_dev->set_prof, AW_PROFILE_DATA_TYPE_DSP);
	if (dsp_data == NULL ||
		dsp_data->data == NULL ||
			dsp_data->len == 0) {
		aw_dev_info(aw_dev->dev, "dsp data is NULL");
		return 0;
	}

	mutex_lock(&g_ext_dsp_prof_wr_lock);
	if (ext_dsp_prof_write == AW_EXT_DSP_WRITE_NONE) {
		ret = aw882xx_dsp_write_params(aw_dev, dsp_data->data, dsp_data->len);
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
}
*/

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
		if (aw_dev->efuse_check == AW_EF_OR_CHECK)
			reg_icalk = (reg_icalk >> desc->icalk_shift) | (reg_icalkl >> desc->icalkl_shift);
		else
			reg_icalk = (reg_icalk >> desc->icalk_shift) & (reg_icalkl >> desc->icalkl_shift);
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
		if (aw_dev->efuse_check == AW_EF_OR_CHECK)
			reg_vcalk = (reg_vcalk >> desc->vcalk_shift) | (reg_vcalkl >> desc->vcalkl_shift);
		else
			reg_vcalk = (reg_vcalk >> desc->vcalk_shift) & (reg_vcalkl >> desc->vcalkl_shift);
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

/*pwd enable update reg*/
static int aw_dev_reg_fw_update(struct aw_device *aw_dev)
{
	int ret = -1;
	int i = 0;
	unsigned int reg_addr = 0;
	unsigned int reg_val = 0;
	unsigned int read_val = 0;
	unsigned int read_vol = 0;
	unsigned int efcheck_val = 0;
	struct aw_int_desc *int_desc = &aw_dev->int_desc;
	struct aw_profctrl_desc *profctrl_desc = &aw_dev->profctrl_desc;
	struct aw_bstctrl_desc *bstctrl_desc = &aw_dev->bstctrl_desc;
	struct aw_work_mode *work_mode = &aw_dev->work_mode;
	struct aw_cali_desc *cali_desc = &aw_dev->cali_desc;
	struct aw_volume_desc *vol_desc = &aw_dev->volume_desc;
	struct aw_sec_data_desc *reg_data;
	int16_t *data;
	int data_len;

	char *prof_name = aw882xx_dev_get_prof_name(aw_dev, aw_dev->set_prof);

	if (prof_name == NULL) {
		aw_dev_err(aw_dev->dev, "get prof name failed");
		return -EINVAL;
	}

	reg_data = aw882xx_dev_get_prof_data(aw_dev, aw_dev->set_prof, AW_PROFILE_DATA_TYPE_REG);
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

		if (reg_addr == aw_dev->efcheck_desc.reg) {
			efcheck_val = reg_val & (~aw_dev->efcheck_desc.mask);
			if (efcheck_val == aw_dev->efcheck_desc.or_val)
				aw_dev->efuse_check = AW_EF_OR_CHECK;
			else
				aw_dev->efuse_check = AW_EF_AND_CHECK;

			aw_dev_info(aw_dev->dev, "efuse check: %d", aw_dev->efuse_check);
		}

		if (reg_addr == work_mode->reg) {
			if ((reg_val & (~work_mode->mask)) == work_mode->rcv_val)
				aw_dev->monitor_start = false;
			else
				aw_dev->monitor_start = true;
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

		/*enable uls hmute*/
		if (reg_addr == aw_dev->uls_hmute_desc.reg) {
			reg_val &= aw_dev->uls_hmute_desc.mask;
			reg_val |= aw_dev->uls_hmute_desc.enable;
		}

		if ((cali_desc->mode == AW_CALI_MODE_NONE) &&
				(reg_addr == aw_dev->txen_desc.reg)) {
			aw_dev->txen_desc.reserve_val = reg_val & (~aw_dev->txen_desc.mask);
			aw_dev_info(aw_dev->dev, "reserve_val = 0x%04x",
						aw_dev->txen_desc.reserve_val);
		}

		if (reg_addr == aw_dev->txen_desc.reg) {
			reg_val &= aw_dev->txen_desc.mask;
			reg_val |= aw_dev->txen_desc.disable;
		}

		if (reg_addr == aw_dev->volume_desc.reg) {
			read_vol = (reg_val & (~aw_dev->volume_desc.mask)) >>
				aw_dev->volume_desc.shift;
			aw_dev->volume_desc.init_volume =
				aw_dev->ops.aw_reg_val_to_db(read_vol);
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

	aw882xx_spin_set_record_val(aw_dev);
	ret = aw_dev_set_vcalb(aw_dev);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "can't set vcalb");
		return ret;
	}

	if (aw_dev->cur_prof != aw_dev->set_prof)
		/*clear control volume when PA change profile*/
		vol_desc->ctl_volume = 0;


	/*keep min volume*/
	aw882xx_dev_set_volume(aw_dev, vol_desc->mute_volume);

	aw_dev_info(aw_dev->dev, "load %s done", prof_name);

	return ret;
}

int aw882xx_dev_set_volume(struct aw_device *aw_dev, unsigned int set_vol)
{
	int ret = -1;
	unsigned int hw_vol = 0;
	struct aw_volume_desc *vol_desc = &aw_dev->volume_desc;

	hw_vol = set_vol + vol_desc->init_volume;

	ret = aw_dev->ops.aw_set_hw_volume(aw_dev, hw_vol);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "set volume failed");
		return ret;
	}

	return 0;
}

int aw882xx_dev_get_volume(struct aw_device *aw_dev, unsigned int *get_vol)
{
	int ret = -1;
	unsigned int hw_vol = 0;
	struct aw_volume_desc *vol_desc = &aw_dev->volume_desc;

	ret = aw_dev->ops.aw_get_hw_volume(aw_dev, &hw_vol);
	if (ret < 0) {
		aw_dev_err(aw_dev->dev, "read volume failed");
		return ret;
	}

	*get_vol = hw_vol - vol_desc->init_volume;

	return 0;
}

static void aw_dev_fade_in(struct aw_device *aw_dev)
{
	int i = 0;
	int fade_step = aw_dev->vol_step;
	struct aw_volume_desc *desc = &aw_dev->volume_desc;
	int fade_in_vol = desc->ctl_volume;

	if (fade_step == 0 || g_fade_in_time == 0) {
		aw882xx_dev_set_volume(aw_dev, fade_in_vol);
		return;
	}

	/*volume up*/
	for (i = desc->mute_volume; i >= fade_in_vol; i -= fade_step) {
		aw882xx_dev_set_volume(aw_dev, i);
		usleep_range(g_fade_in_time, g_fade_in_time + 10);
	}

	if (i != fade_in_vol)
		aw882xx_dev_set_volume(aw_dev, fade_in_vol);

}

static void aw_dev_fade_out(struct aw_device *aw_dev)
{
	int i = 0;
	int fade_step = aw_dev->vol_step;
	struct aw_volume_desc *desc = &aw_dev->volume_desc;

	if (fade_step == 0 || g_fade_out_time == 0) {
		aw882xx_dev_set_volume(aw_dev, desc->mute_volume);
		return;
	}

	for (i = desc->ctl_volume; i <= desc->mute_volume; i += fade_step) {
		aw882xx_dev_set_volume(aw_dev, i);
		usleep_range(g_fade_out_time, g_fade_out_time + 10);
	}

	if (i != desc->mute_volume) {
		aw882xx_dev_set_volume(aw_dev, desc->mute_volume);
		usleep_range(g_fade_out_time, g_fade_out_time + 10);
	}
}

static void aw_dev_pwd(struct aw_device *aw_dev, bool pwd)
{
	struct aw_pwd_desc *pwd_desc = &aw_dev->pwd_desc;

	aw_dev_dbg(aw_dev->dev, "enter, pwd: %d", pwd);

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

	aw_dev_dbg(aw_dev->dev, "enter, amppd: %d", amppd);

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

void aw882xx_dev_mute(struct aw_device *aw_dev, bool mute)
{
	struct aw_mute_desc *mute_desc = &aw_dev->mute_desc;

	aw_dev_dbg(aw_dev->dev, "enter, mute: %d, cali_result: %d",
				mute, aw_dev->cali_desc.cali_result);

	if (mute) {
		aw_dev_fade_out(aw_dev);
		aw_dev->ops.aw_i2c_write_bits(aw_dev, mute_desc->reg,
				mute_desc->mask,
				mute_desc->enable);
		usleep_range(AW_5000_US, AW_5000_US + 50);
	} else {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, mute_desc->reg,
				mute_desc->mask,
				mute_desc->disable);
		aw_dev_fade_in(aw_dev);
	}
	aw_dev_info(aw_dev->dev, "done");
}

static void aw_dev_uls_hmute(struct aw_device *aw_dev, bool uls_hmute)
{
	struct aw_uls_hmute_desc *uls_hmute_desc = &aw_dev->uls_hmute_desc;

	aw_dev_dbg(aw_dev->dev, "enter, uls_hmute: %d", uls_hmute);

	if (uls_hmute_desc->reg == AW_REG_NONE)
		return;

	if (uls_hmute) {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, uls_hmute_desc->reg,
				uls_hmute_desc->mask,
				uls_hmute_desc->enable);
	} else {
		aw_dev->ops.aw_i2c_write_bits(aw_dev, uls_hmute_desc->reg,
				uls_hmute_desc->mask,
				uls_hmute_desc->disable);
	}
	aw_dev_info(aw_dev->dev, "done");
}


int aw882xx_dev_get_int_status(struct aw_device *aw_dev, uint16_t *int_status)
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

void aw882xx_dev_clear_int_status(struct aw_device *aw_dev)
{
	uint16_t int_status = 0;

	/*read int status and clear*/
	aw882xx_dev_get_int_status(aw_dev, &int_status);
	/*make suer int status is clear*/
	aw882xx_dev_get_int_status(aw_dev, &int_status);
	aw_dev_info(aw_dev->dev, "done");
}

int aw882xx_dev_set_intmask(struct aw_device *aw_dev, bool flag)
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
		if ((reg_val & desc->pll_check) == desc->pll_check) {
			ret = 0;
			break;
		} else {
			aw_dev_dbg(aw_dev->dev, "check pll lock fail, cnt=%d, reg_val=0x%04x",
					i, reg_val);
			usleep_range(AW_2000_US, AW_2000_US + 10);
		}
	}
	if (ret < 0)
		aw_dev_err(aw_dev->dev, "pll&clk check fail");
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
				cco_mux_desc->mask, cco_mux_desc->divided_val);
	ret = aw_dev_mode1_pll_check(aw_dev);

	/* change mode1 */
	aw_dev->ops.aw_i2c_write_bits(aw_dev, cco_mux_desc->reg,
				cco_mux_desc->mask, cco_mux_desc->bypass_val);
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

int aw882xx_dev_get_fade_vol_step(struct aw_device *aw_dev)
{
	return aw_dev->vol_step;
}

void aw882xx_dev_set_fade_vol_step(struct aw_device *aw_dev, unsigned int step)
{
	aw_dev->vol_step = step;
}

void aw882xx_dev_get_fade_time(unsigned int *time, bool fade_in)
{
	if (fade_in)
		*time = g_fade_in_time;
	else
		*time = g_fade_out_time;
}

void aw882xx_dev_set_fade_time(unsigned int time, bool fade_in)
{
	if (fade_in)
		g_fade_in_time = time;
	else
		g_fade_out_time = time;
}

/*init aw_device*/
void aw882xx_dev_deinit(struct aw_device *aw_dev)
{
	if (aw_dev == NULL)
		return;

	if (aw_dev->prof_info.prof_desc != NULL) {
		kfree(aw_dev->prof_info.prof_desc);
		aw_dev->prof_info.prof_desc = NULL;
		aw_dev->prof_info.count = 0;
	}
}

int aw882xx_dev_get_cali_re(struct aw_device *aw_dev, int32_t *cali_re)
{

	return aw882xx_dsp_read_cali_re(aw_dev, cali_re);
}

int aw882xx_dev_dc_status(struct aw_device *aw_dev)
{
	return aw882xx_dsp_get_dc_status(aw_dev);
}

int aw882xx_dev_status(struct aw_device *aw_dev)
{
	return aw_dev->status;
}

int aw882xx_dev_init_cali_re(struct aw_device *aw_dev)
{
	int ret = 0;
	struct aw_cali_desc *cali_desc = &aw_dev->cali_desc;

	if (cali_desc->mode) {
		if (cali_desc->cali_re == AW_ERRO_CALI_VALUE) {
			ret = aw882xx_cali_read_re_from_nvram(&cali_desc->cali_re, aw_dev->channel);
			if (ret) {
				aw_dev_info(aw_dev->dev, "read nvram cali failed, use default Re");
				cali_desc->cali_re = AW_ERRO_CALI_VALUE;
				cali_desc->cali_result = CALI_RESULT_NONE;
				return 0;
			}

			if (cali_desc->cali_re < aw_dev->re_min ||
					cali_desc->cali_re > aw_dev->re_max) {
				aw_dev_err(aw_dev->dev, "out range re value: %d",
								cali_desc->cali_re);
				cali_desc->cali_re = AW_ERRO_CALI_VALUE;
				/*cali_result is error when aw-cali-check enable*/
				if (aw_dev->cali_desc.cali_check_st) {
					cali_desc->cali_result = CALI_RESULT_ERROR;
				}
				return -EINVAL;
			}

			aw_dev_dbg(aw_dev->dev, "read re value: %d", cali_desc->cali_re);

			if (aw_dev->cali_desc.cali_check_st) {
				cali_desc->cali_result = CALI_RESULT_NORMAL;
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

int aw882xx_device_irq_reinit(struct aw_device *aw_dev)
{
	int ret;

	/*reg re load*/
	ret = aw_dev_reg_fw_update(aw_dev);
	if (ret < 0)
		return ret;

	return 0;
}

int aw882xx_device_init(struct aw_device *aw_dev, struct aw_container *aw_cfg)
{
	/*acf_hdr_t *hdr;*/
	int ret;

	if (aw_cfg == NULL) {
		aw_dev_err(aw_dev->dev, "aw_cfg is NULL");
		return -ENOMEM;
	}

	ret = aw882xx_dev_parse_acf(aw_dev, aw_cfg);
	if (ret) {
		aw882xx_dev_deinit(aw_dev);
		aw_dev_err(aw_dev->dev, "aw_dev acf load failed");
		return -EINVAL;
	}

	aw_dev_soft_reset(aw_dev);

	aw_dev->cur_prof = AW_INIT_PROFILE;
	aw_dev->set_prof = AW_INIT_PROFILE;
	ret = aw_dev_reg_fw_update(aw_dev);
	if (ret < 0)
		return ret;

	if (aw_dev->ops.aw_frcset_check) {
		ret = aw_dev->ops.aw_frcset_check(aw_dev);
		if (ret)
			return ret;
	}

	aw_dev->status = AW_DEV_PW_ON;
	aw882xx_device_stop(aw_dev);

	aw_dev_info(aw_dev->dev, "init done");
	return 0;
}

int aw882xx_dev_reg_update(struct aw_device *aw_dev, bool force)
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

	if (desc->mode && (desc->cali_re != AW_ERRO_CALI_VALUE)) {
		if ((desc->cali_re >= aw_dev->re_min) &&
				(desc->cali_re <= aw_dev->re_max))
			aw882xx_dsp_write_cali_re(aw_dev, desc->cali_re);
		else
			aw_dev_err(aw_dev->dev, "cali re is out of range");
	}
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

void aw_dev_i2s_enable(struct aw_device *aw_dev, bool flag)
{
	struct aw_txen_desc *txen_desc = &aw_dev->txen_desc;
	struct aw_cali_desc *cali_desc = &aw_dev->cali_desc;

	aw_dev_dbg(aw_dev->dev, "enter, i2s_enable: %d", flag);

	if (txen_desc->reg == AW_REG_NONE) {
		aw_dev_info(aw_dev->dev, "needn't set i2s status");
		return;
	}

	if (flag) {
		if (cali_desc->mode == AW_CALI_MODE_NONE)
			aw_dev->ops.aw_i2c_write_bits(aw_dev,
				txen_desc->reg, txen_desc->mask, txen_desc->reserve_val);
		else
			aw_dev->ops.aw_i2c_write_bits(aw_dev,
				txen_desc->reg, txen_desc->mask, txen_desc->enable);
	} else {
		aw_dev->ops.aw_i2c_write_bits(aw_dev,
				txen_desc->reg, txen_desc->mask, txen_desc->disable);
	}
}


int aw882xx_device_start(struct aw_device *aw_dev)
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
		aw_dev_i2s_enable(aw_dev, false);
		/*clear interrupt*/
		aw882xx_dev_clear_int_status(aw_dev);
		/*close amppd*/
		aw_dev_amppd(aw_dev, true);
		/*power down*/
		aw_dev_pwd(aw_dev, true);
		return -EINVAL;
	}

	/*boost type recover*/
	aw_dev_boost_type_recover(aw_dev);

	/*enable tx feedback*/
	aw_dev_i2s_enable(aw_dev, true);

	if (aw_dev->amppd_st) {
		aw_dev_amppd(aw_dev, true);
	}

	if (aw_dev->ops.aw_reg_force_set)
		aw_dev->ops.aw_reg_force_set(aw_dev);

	/*close uls hmute*/
	aw_dev_uls_hmute(aw_dev, false);

	if (!aw_dev->mute_st) {
		/*close mute*/
		if (aw882xx_cali_check_result(&aw_dev->cali_desc))
			aw882xx_dev_mute(aw_dev, false);
		else
			aw882xx_dev_mute(aw_dev, true);
	}

	/*clear inturrupt*/
	aw882xx_dev_clear_int_status(aw_dev);
	/*set inturrupt mask*/
	aw882xx_dev_set_intmask(aw_dev, true);

	aw882xx_monitor_start(&aw_dev->monitor_desc);
	aw_dev_cali_re_update(aw_dev);

	aw_dev->status = AW_DEV_PW_ON;
	aw_dev_info(aw_dev->dev, "done");
	return 0;
}

int aw882xx_device_stop(struct aw_device *aw_dev)
{
	aw_dev_dbg(aw_dev->dev, "enter");

	if (aw_dev->status == AW_DEV_PW_OFF) {
		aw_dev_dbg(aw_dev->dev, "already power off");
		return 0;
	}

	aw_dev->status = AW_DEV_PW_OFF;

	aw882xx_monitor_stop(&aw_dev->monitor_desc);
	/*clear interrupt*/
	aw882xx_dev_clear_int_status(aw_dev);

	/*set defaut int mask*/
	aw882xx_dev_set_intmask(aw_dev, false);

	/*set uls hmute*/
	aw_dev_uls_hmute(aw_dev, true);

	/*set mute*/
	aw882xx_dev_mute(aw_dev, true);

	/*close tx feedback*/
	aw_dev_i2s_enable(aw_dev, false);

	usleep_range(AW_1000_US, AW_1000_US + 100);

	/*enable amppd*/
	aw_dev_amppd(aw_dev, true);

	/*set power down*/
	aw_dev_pwd(aw_dev, true);

	ext_dsp_prof_write = AW_EXT_DSP_WRITE_NONE;
	aw_dev_info(aw_dev->dev, "done");
	return 0;
}

int aw882xx_dev_set_afe_module_en(int type, int enable)
{
	return aw882xx_dsp_set_afe_module_en(type, enable);
}

int aw882xx_dev_get_afe_module_en(int type, int *status)
{
	return aw882xx_dsp_get_afe_module_en(type, status);
}

int aw882xx_dev_set_copp_module_en(bool enable)
{
	return aw882xx_dsp_set_copp_module_en(enable);
}

static int aw_device_parse_sound_channel_dt(struct aw_device *aw_dev)
{
	int ret = 0;
	uint32_t channel_value = 0;
	struct list_head *dev_list = NULL;
	struct list_head *pos = NULL;
	struct aw_device *local_dev = NULL;

	ret = of_property_read_u32(aw_dev->dev->of_node, "sound-channel", &channel_value);
	if (ret < 0) {
		channel_value = AW_DEV_CH_PRI_L;
		aw_dev_info(aw_dev->dev, "read sound-channel failed,use default");
	}

	aw_dev_info(aw_dev->dev, "read sound-channel value is : %d", channel_value);
	if (channel_value >= AW_DEV_CH_MAX) {
		channel_value = AW_DEV_CH_PRI_L;
	}
	/* when dev_num > 0, get dev list to compare*/
	if (aw_dev->ops.aw_get_dev_num() > 0) {
		ret = aw882xx_dev_get_list_head(&dev_list);
		if (ret) {
			aw_dev_err(aw_dev->dev, "get dev list failed");
			return ret;
		}

		list_for_each(pos, dev_list) {
			local_dev = container_of(pos, struct aw_device, list_node);
			if (local_dev->channel == channel_value) {
				aw_dev_err(local_dev->dev, "sound-channel:%d already exists",
					channel_value);
				return -EINVAL;
			}
		}
	}

	aw_dev->channel = channel_value;

	return 0;

}

static int aw_device_parse_dt(struct aw_device *aw_dev)
{
	int ret = 0;

	ret = aw_device_parse_sound_channel_dt(aw_dev);
	if (ret) {
		aw_dev_err(aw_dev->dev, "parse sound-channel failed!");
		return ret;
	}
	aw882xx_device_parse_topo_id_dt(aw_dev);
	aw882xx_device_parse_port_id_dt(aw_dev);

	return ret;
}

int aw882xx_dev_get_list_head(struct list_head **head)
{
	if (list_empty(&g_dev_list))
		return -EINVAL;

	*head = &g_dev_list;

	return 0;
}

int aw882xx_device_probe(struct aw_device *aw_dev)
{
	int ret = 0;

	INIT_LIST_HEAD(&aw_dev->list_node);

	ret = aw_device_parse_dt(aw_dev);
	if (ret)
		return ret;

	ret = aw882xx_cali_init(&aw_dev->cali_desc);
	if (ret)
		return ret;

	aw882xx_monitor_init(&aw_dev->monitor_desc);
	/*aw_afe_init();*/

	ret = aw882xx_spin_init(&aw_dev->spin_desc);
	if (ret)
		return ret;

	mutex_lock(&g_dev_lock);
	list_add(&aw_dev->list_node, &g_dev_list);
	mutex_unlock(&g_dev_lock);

	return 0;
}

int aw882xx_device_remove(struct aw_device *aw_dev)
{
	aw882xx_monitor_deinit(&aw_dev->monitor_desc);
	aw882xx_cali_deinit(&aw_dev->cali_desc);
	/*aw_afe_deinit();*/
	return 0;
}

