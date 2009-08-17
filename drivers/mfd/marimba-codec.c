/* Copyright (c) 2009-2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/mfd/msm-adie-codec.h>
#include <linux/mfd/marimba.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#define MARIMBA_CDC_RX_CTL 0x81
#define MARIMBA_CDC_RX_CTL_ST_EN_MASK 0x20
#define MARIMBA_CDC_RX_CTL_ST_EN_SHFT 0x5
#define MARIMBA_CODEC_CDC_LRXG     0x84
#define MARIMBA_CODEC_CDC_RRXG     0x85
#define MARIMBA_CODEC_CDC_LTXG     0x86
#define MARIMBA_CODEC_CDC_RTXG     0x87

#define MAX_MDELAY_US 2000
#define MIN_MDELAY_US 1000

struct adie_codec_path {
	struct adie_codec_dev_profile *profile;
	struct adie_codec_register_image img;
	u32 hwsetting_idx;
	u32 stage_idx;
	u32 curr_stage;
};

static struct adie_codec_register adie_codec_tx_regs[] = {
	{ 0x04, 0xc0, 0x8C },
	{ 0x0D, 0xFF, 0x00 },
	{ 0x0E, 0xFF, 0x00 },
	{ 0x0F, 0xFF, 0x00 },
	{ 0x10, 0xF8, 0x68 },
	{ 0x11, 0xFE, 0x00 },
	{ 0x12, 0xFE, 0x00 },
	{ 0x13, 0xFF, 0x58 },
	{ 0x14, 0xFF, 0x00 },
	{ 0x15, 0xFE, 0x00 },
	{ 0x16, 0xFF, 0x00 },
	{ 0x1A, 0xFF, 0x00 },
	{ 0x80, 0x01, 0x00 },
	{ 0x82, 0x7F, 0x18 },
	{ 0x83, 0x1C, 0x00 },
	{ 0x86, 0xFF, 0xAC },
	{ 0x87, 0xFF, 0xAC },
	{ 0x89, 0xFF, 0xFF },
	{ 0x8A, 0xF0, 0x30 }
};

static struct adie_codec_register adie_codec_rx_regs[] = {
	{ 0x23, 0xF8, 0x00 },
	{ 0x24, 0x6F, 0x00 },
	{ 0x25, 0x7F, 0x00 },
	{ 0x26, 0xFC, 0x00 },
	{ 0x28, 0xFE, 0x00 },
	{ 0x29, 0xFE, 0x00 },
	{ 0x33, 0xFF, 0x00 },
	{ 0x34, 0xFF, 0x00 },
	{ 0x35, 0xFC, 0x00 },
	{ 0x36, 0xFE, 0x00 },
	{ 0x37, 0xFE, 0x00 },
	{ 0x38, 0xFE, 0x00 },
	{ 0x39, 0xF0, 0x00 },
	{ 0x3A, 0xFF, 0x0A },
	{ 0x3B, 0xFC, 0xAC },
	{ 0x3C, 0xFC, 0xAC },
	{ 0x3D, 0xFF, 0x55 },
	{ 0x3E, 0xFF, 0x55 },
	{ 0x3F, 0xCF, 0x00 },
	{ 0x40, 0x3F, 0x00 },
	{ 0x41, 0x3F, 0x00 },
	{ 0x42, 0xFF, 0x00 },
	{ 0x43, 0xF7, 0x00 },
	{ 0x43, 0xF7, 0x00 },
	{ 0x43, 0xF7, 0x00 },
	{ 0x43, 0xF7, 0x00 },
	{ 0x44, 0xF7, 0x00 },
	{ 0x45, 0xFF, 0x00 },
	{ 0x46, 0xFF, 0x00 },
	{ 0x47, 0xF7, 0x00 },
	{ 0x48, 0xF7, 0x00 },
	{ 0x49, 0xFF, 0x00 },
	{ 0x4A, 0xFF, 0x00 },
	{ 0x80, 0x02, 0x00 },
	{ 0x81, 0xFF, 0x4C },
	{ 0x83, 0x23, 0x00 },
	{ 0x84, 0xFF, 0xAC },
	{ 0x85, 0xFF, 0xAC },
	{ 0x88, 0xFF, 0xFF },
	{ 0x8A, 0x0F, 0x03 },
	{ 0x8B, 0xFF, 0xAC },
	{ 0x8C, 0x03, 0x01 },
	{ 0x8D, 0xFF, 0x00 },
	{ 0x8E, 0xFF, 0x00 }
};

static struct adie_codec_register adie_codec_lb_regs[] = {
	{ 0x2B, 0x8F, 0x02 },
	{ 0x2C, 0x8F, 0x02 }
};

struct adie_codec_state {
	struct adie_codec_path path[ADIE_CODEC_MAX];
	u32 ref_cnt;
	struct marimba *pdrv_ptr;
	struct marimba_codec_platform_data *codec_pdata;
	struct mutex lock;
};

static struct adie_codec_state adie_codec;

/* Array containing write details of Tx and RX Digital Volume
   Tx and Rx and both the left and right channel use the same data
*/
u8 adie_codec_rx_tx_dig_vol_data[] = {
	0x81, 0x82, 0x83, 0x84,
	0x85, 0x86, 0x87, 0x88,
	0x89, 0x8a, 0x8b, 0x8c,
	0x8d, 0x8e, 0x8f, 0x90,
	0x91, 0x92, 0x93, 0x94,
	0x95, 0x96, 0x97, 0x98,
	0x99, 0x9a, 0x9b, 0x9c,
	0x9d, 0x9e, 0x9f, 0xa0,
	0xa1, 0xa2, 0xa3, 0xa4,
	0xa5, 0xa6, 0xa7, 0xa8,
	0xa9, 0xaa, 0xab, 0xac,
	0xad, 0xae, 0xaf, 0xb0,
	0xb1, 0xb2, 0xb3, 0xb4,
	0xb5, 0xb6, 0xb7, 0xb8,
	0xb9, 0xba, 0xbb, 0xbc,
	0xbd, 0xbe, 0xbf, 0xc0,
	0xc1, 0xc2, 0xc3, 0xc4,
	0xc5, 0xc6, 0xc7, 0xc8,
	0xc9, 0xca, 0xcb, 0xcc,
	0xcd, 0xce, 0xcf, 0xd0,
	0xd1, 0xd2, 0xd3, 0xd4,
	0xd5, 0xd6, 0xd7, 0xd8,
	0xd9, 0xda, 0xdb, 0xdc,
	0xdd, 0xde, 0xdf, 0xe0,
	0xe1, 0xe2, 0xe3, 0xe4,
	0xe5, 0xe6, 0xe7, 0xe8,
	0xe9, 0xea, 0xeb, 0xec,
	0xed, 0xee, 0xf0, 0xf1,
	0xf2, 0xf3, 0xf4, 0xf5,
	0xf6, 0xf7, 0xf8, 0xf9,
	0xfa, 0xfb, 0xfc, 0xfd,
	0xfe, 0xff, 0x00, 0x01,
	0x02, 0x03, 0x04, 0x05,
	0x06, 0x07, 0x08, 0x09,
	0x0a, 0x0b, 0x0c, 0x0d,
	0x0e, 0x0f, 0x10, 0x11,
	0x12, 0x13, 0x14, 0x15,
	0x16, 0x17, 0x18, 0x19,
	0x1a, 0x1b, 0x1c, 0x1d,
	0x1e, 0x1f, 0x20, 0x21,
	0x22, 0x23, 0x24, 0x25,
	0x26, 0x27, 0x28, 0x29,
	0x2a, 0x2b, 0x2c, 0x2d,
	0x2e, 0x2f, 0x30, 0x31,
	0x32, 0x33, 0x34, 0x35,
	0x36, 0x37, 0x38, 0x39,
	0x3a, 0x3b, 0x3c, 0x3d,
	0x3e, 0x3f, 0x40, 0x41,
	0x42, 0x43, 0x44, 0x45,
	0x46, 0x47, 0x48, 0x49,
	0x4a, 0x4b, 0x4c, 0x4d,
	0x4e, 0x4f, 0x50, 0x51,
	0x52, 0x53, 0x54, 0x55,
	0x56, 0x57, 0x58, 0x59,
	0x5a, 0x5b, 0x5c, 0x5d,
	0x5e, 0x5f, 0x60, 0x61,
	0x62, 0x63, 0x64, 0x65,
	0x66, 0x67, 0x68, 0x69,
	0x6a, 0x6b, 0x6c, 0x6d,
	0x6e, 0x6f, 0x70, 0x71,
	0x72, 0x73, 0x74, 0x75,
	0x76, 0x77, 0x78, 0x79,
	0x7a, 0x7b, 0x7c, 0x7d,
	0x7e, 0x7f
};

enum adie_vol_type {
	ADIE_CODEC_RX_DIG_VOL,
	ADIE_CODEC_TX_DIG_VOL,
	ADIE_CODEC_VOL_TYPE_MAX
};

struct adie_codec_ch_vol_cntrl {
	u8 codec_reg;
	u8 codec_mask;
	u8 *vol_cntrl_data;
};

struct adie_codec_vol_cntrl_data {

	enum adie_vol_type vol_type;

	/* Jump length used while doing writes in incremental fashion */
	u32 jump_length;
	s32 min_mb;		/* Min Db applicable to the vol control */
	s32 max_mb;		/* Max Db applicable to the vol control */
	u32 step_in_mb;
	u32 steps;		/* No of steps allowed for this vol type */

	struct adie_codec_ch_vol_cntrl *ch_vol_cntrl_info;
};

static struct adie_codec_ch_vol_cntrl adie_codec_rx_vol_cntrl[] = {
	{MARIMBA_CODEC_CDC_LRXG, 0xff, adie_codec_rx_tx_dig_vol_data},
	{MARIMBA_CODEC_CDC_RRXG, 0xff, adie_codec_rx_tx_dig_vol_data}
};

static struct adie_codec_ch_vol_cntrl adie_codec_tx_vol_cntrl[] = {
	{MARIMBA_CODEC_CDC_LTXG, 0xff, adie_codec_rx_tx_dig_vol_data},
	{MARIMBA_CODEC_CDC_RTXG, 0xff, adie_codec_rx_tx_dig_vol_data}
};

static struct adie_codec_vol_cntrl_data adie_codec_vol_cntrl[] = {
	{ADIE_CODEC_RX_DIG_VOL, 5100, -12700, 12700, 100, 255,
	 adie_codec_rx_vol_cntrl},

	{ADIE_CODEC_TX_DIG_VOL, 5100, -12700, 12700, 100, 255,
	 adie_codec_tx_vol_cntrl}
};

static int adie_codec_write(u8 reg, u8 mask, u8 val)
{
	int rc;

	rc = marimba_write_bit_mask(adie_codec.pdrv_ptr, reg,  &val, 1, mask);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: fail to write reg %x\n", __func__, reg);
		return -EIO;
	}

	pr_debug("%s: write reg %x val %x\n", __func__, reg, val);

	return 0;
}

static int adie_codec_read(u8 reg, u8 *val)
{
	return marimba_read(adie_codec.pdrv_ptr, reg, val, 1);
}

static int adie_codec_read_dig_vol(enum adie_vol_type vol_type, u32 chan_index,
				   u32 *cur_index)
{
	u32 counter;
	u32 size;
	u8 reg, mask, cur_val;
	int rc;

	reg =
	    adie_codec_vol_cntrl[vol_type].
	    ch_vol_cntrl_info[chan_index].codec_reg;

	mask =
	    adie_codec_vol_cntrl[vol_type].
	    ch_vol_cntrl_info[chan_index].codec_mask;

	rc = marimba_read(adie_codec.pdrv_ptr, reg, &cur_val, 1);

	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: fail to read reg %x\n", __func__, reg);
		return -EIO;
	}

	cur_val = cur_val & mask;

	pr_debug("%s: reg 0x%x  mask 0x%x  reg_value = 0x%x"
		"vol_type = %d\n", __func__, reg, mask, cur_val, vol_type);

	size = adie_codec_vol_cntrl[vol_type].steps;

	for (counter = 0; counter <= size; counter++) {

		if (adie_codec_vol_cntrl[vol_type].ch_vol_cntrl_info
		    [chan_index].vol_cntrl_data[counter] == cur_val) {
			*cur_index = counter;
			return 0;
		}
	}

	pr_err("%s: could not find 0x%x in reg 0x%x values array\n",
			__func__, cur_val, reg);

	return -EINVAL;;
}

static int adie_codec_set_dig_vol(enum adie_vol_type vol_type, u32 chan_index,
				  u32 cur_index, u32 target_index)
{
	u32 count;
	u8 reg, mask, val;
	u32 i;
	u32 index;
	u32 index_jump;

	int rc;

	index_jump = adie_codec_vol_cntrl[vol_type].jump_length;

	reg =
	    adie_codec_vol_cntrl[vol_type].
	    ch_vol_cntrl_info[chan_index].codec_reg;

	mask =
	    adie_codec_vol_cntrl[vol_type].
	    ch_vol_cntrl_info[chan_index].codec_mask;

	/* compare the target index with current index */
	if (cur_index < target_index) {

		/* Volume is being increased loop and increase it in 4-5 steps
		 */
		count = ((target_index - cur_index) * 100 / index_jump);
		index = cur_index;

		for (i = 1; i <= count; i++) {
			index = index + (int)(index_jump / 100);

			val =
			    adie_codec_vol_cntrl[vol_type].ch_vol_cntrl_info
			    [chan_index].vol_cntrl_data[index];

			pr_debug("%s: write reg %x val 0x%x\n",
					__func__, reg, val);

			rc = adie_codec_write(reg, mask, val);
			if (rc < 0) {
				pr_err("%s: write reg %x val 0x%x failed\n",
					__func__, reg, val);
				return rc;
			}
		}

		/*do one final write to take it to the target index level */
		val =
		    adie_codec_vol_cntrl[vol_type].ch_vol_cntrl_info
		    [chan_index].vol_cntrl_data[target_index];

		pr_debug("%s: write reg %x val 0x%x\n", __func__, reg, val);

		rc = adie_codec_write(reg, mask, val);

		if (rc < 0) {
			pr_err("%s: write reg %x val 0x%x failed\n",
					__func__, reg, val);
			return rc;
		}

	} else {

		/* Volume is being decreased from the current setting */
		index = cur_index;
		/* loop and decrease it in 4-5 steps */
		count = ((cur_index - target_index) * 100 / index_jump);

		for (i = 1; i <= count; i++) {
			index = index - (int)(index_jump / 100);

			val =
			    adie_codec_vol_cntrl[vol_type].ch_vol_cntrl_info
			    [chan_index].vol_cntrl_data[index];

			pr_debug("%s: write reg %x val 0x%x\n",
					__func__, reg, val);

			rc = adie_codec_write(reg, mask, val);
			if (rc < 0) {
				pr_err("%s: write reg %x val 0x%x failed\n",
					__func__, reg, val);
				return rc;
			}
		}

		/* do one final write to take it to the target index level */
		val =
		    adie_codec_vol_cntrl[vol_type].ch_vol_cntrl_info
		    [chan_index].vol_cntrl_data[target_index];

		pr_debug("%s: write reg %x val 0x%x\n", __func__, reg, val);

		rc = adie_codec_write(reg, mask, val);

		if (rc < 0) {
			pr_err("%s: write reg %x val 0x%x failed\n",
					__func__, reg, val);
			return rc;
		}
	}
	return 0;
}

static int marimba_adie_codec_set_device_digital_volume(
		struct adie_codec_path *path_ptr,
		u32 num_channels, u32 vol_percentage /* in percentage */)
{
	enum adie_vol_type vol_type;
	s32 milli_bel;
	u32 chan_index;
	u32 step_index;
	u32 cur_step_index = 0;

	if (!path_ptr  || (path_ptr->curr_stage !=
				ADIE_CODEC_DIGITAL_ANALOG_READY)) {
		pr_info("%s: Marimba codec not ready for volume control\n",
		       __func__);
		return  -EPERM;
	}

	if (num_channels > 2) {
		pr_err("%s: Marimba codec only supports max two channels\n",
		       __func__);
		return -EINVAL;
	}

	if (path_ptr->profile->path_type == ADIE_CODEC_RX)
		vol_type = ADIE_CODEC_RX_DIG_VOL;
	else if (path_ptr->profile->path_type == ADIE_CODEC_TX)
		vol_type = ADIE_CODEC_TX_DIG_VOL;
	else {
		pr_err("%s: invalid device data neither RX nor TX\n",
				__func__);
		return -EINVAL;
	}

	milli_bel = ((adie_codec_vol_cntrl[vol_type].max_mb -
			adie_codec_vol_cntrl[vol_type].min_mb) *
			vol_percentage) / 100;

	milli_bel = adie_codec_vol_cntrl[vol_type].min_mb + milli_bel;

	pr_debug("%s: milli bell = %d vol_type = %d vol_percentage = %d"
		 " num_cha =  %d \n",
		 __func__, milli_bel, vol_type, vol_percentage, num_channels);


	step_index = ((milli_bel
		       - adie_codec_vol_cntrl[vol_type].min_mb
		       + (adie_codec_vol_cntrl[vol_type].step_in_mb / 2))
		      / adie_codec_vol_cntrl[vol_type].step_in_mb);


	for (chan_index = 0; chan_index < num_channels; chan_index++) {
		adie_codec_read_dig_vol(vol_type, chan_index, &cur_step_index);

		pr_debug("%s: cur_step_index = %u  current vol = 0x%x\n",
				__func__, cur_step_index,
			adie_codec_vol_cntrl[vol_type].ch_vol_cntrl_info
			[chan_index].vol_cntrl_data[cur_step_index]);

		pr_debug("%s: step index = %u  new volume = 0x%x\n",
		 __func__, step_index,
		 adie_codec_vol_cntrl[vol_type].ch_vol_cntrl_info
		 [chan_index].vol_cntrl_data[step_index]);

		adie_codec_set_dig_vol(vol_type, chan_index, cur_step_index,
				       step_index);

	}
	return 0;
}

static int marimba_adie_codec_setpath(struct adie_codec_path *path_ptr,
					u32 freq_plan, u32 osr)
{
	int rc = 0;
	u32 i, freq_idx = 0, freq = 0;

	if ((path_ptr->curr_stage != ADIE_CODEC_DIGITAL_OFF) &&
		(path_ptr->curr_stage != ADIE_CODEC_FLASH_IMAGE)) {
		rc = -EBUSY;
		goto error;
	}

	for (i = 0; i < path_ptr->profile->setting_sz; i++) {
		if (path_ptr->profile->settings[i].osr == osr) {
			if (path_ptr->profile->settings[i].freq_plan >=
				freq_plan) {
				if (freq == 0) {
					freq = path_ptr->profile->settings[i].
								freq_plan;
					freq_idx = i;
				} else if (path_ptr->profile->settings[i].
					freq_plan < freq) {
					freq = path_ptr->profile->settings[i].
								freq_plan;
					freq_idx = i;
				}
			}
		}
	}

	if (freq_idx >= path_ptr->profile->setting_sz)
		rc = -ENODEV;
	else {
		path_ptr->hwsetting_idx = freq_idx;
		path_ptr->stage_idx = 0;
	}

error:
	return rc;
}

static u32 marimba_adie_codec_freq_supported(
				struct adie_codec_dev_profile *profile,
				u32 requested_freq)
{
	u32 i, rc = -EINVAL;

	for (i = 0; i < profile->setting_sz; i++) {
		if (profile->settings[i].freq_plan >= requested_freq) {
			rc = 0;
			break;
		}
	}
	return rc;
}

static int marimba_adie_codec_enable_sidetone(
				struct adie_codec_path *rx_path_ptr,
				u32 enable)
{
	int rc = 0;

	pr_debug("%s()\n", __func__);

	mutex_lock(&adie_codec.lock);

	if (!rx_path_ptr || &adie_codec.path[ADIE_CODEC_RX] != rx_path_ptr) {
		pr_err("%s: invalid path pointer\n", __func__);
		rc = -EINVAL;
		goto error;
	} else if (rx_path_ptr->curr_stage !=
		ADIE_CODEC_DIGITAL_ANALOG_READY) {
		pr_err("%s: bad state\n", __func__);
		rc = -EPERM;
		goto error;
	}

	if (enable)
		rc = adie_codec_write(MARIMBA_CDC_RX_CTL,
		MARIMBA_CDC_RX_CTL_ST_EN_MASK,
		(0x1 << MARIMBA_CDC_RX_CTL_ST_EN_SHFT));
	else
		rc = adie_codec_write(MARIMBA_CDC_RX_CTL,
		MARIMBA_CDC_RX_CTL_ST_EN_MASK, 0);

error:
	mutex_unlock(&adie_codec.lock);
	return rc;
}

static void adie_codec_reach_stage_action(struct adie_codec_path *path_ptr,
						u32 stage)
{
	u32 iter;
	struct adie_codec_register *reg_info;

	if (stage == ADIE_CODEC_FLASH_IMAGE) {
		/* perform reimage */
		for (iter = 0; iter < path_ptr->img.img_sz; iter++) {
			reg_info = &path_ptr->img.regs[iter];
			adie_codec_write(reg_info->reg,
			reg_info->mask, reg_info->val);
		}
	}
}

static int marimba_adie_codec_proceed_stage(struct adie_codec_path *path_ptr,
						u32 state)
{
	int rc = 0, loop_exit = 0;
	struct adie_codec_action_unit *curr_action;
	struct adie_codec_hwsetting_entry *setting;
	u8 reg, mask, val;

	mutex_lock(&adie_codec.lock);
	setting = &path_ptr->profile->settings[path_ptr->hwsetting_idx];
	while (!loop_exit) {
		curr_action = &setting->actions[path_ptr->stage_idx];
		switch (curr_action->type) {
		case ADIE_CODEC_ACTION_ENTRY:
			ADIE_CODEC_UNPACK_ENTRY(curr_action->action,
			reg, mask, val);
			adie_codec_write(reg, mask, val);
			break;
		case ADIE_CODEC_ACTION_DELAY_WAIT:
			if (curr_action->action > MAX_MDELAY_US)
				msleep(curr_action->action/1000);
			else if (curr_action->action < MIN_MDELAY_US)
				udelay(curr_action->action);
			else
				mdelay(curr_action->action/1000);
			break;
		case ADIE_CODEC_ACTION_STAGE_REACHED:
			adie_codec_reach_stage_action(path_ptr,
				curr_action->action);
			if (curr_action->action == state) {
				path_ptr->curr_stage = state;
				loop_exit = 1;
			}
			break;
		default:
			BUG();
		}

		path_ptr->stage_idx++;
		if (path_ptr->stage_idx == setting->action_sz)
			path_ptr->stage_idx = 0;
	}
	mutex_unlock(&adie_codec.lock);
	return rc;
}

static void marimba_codec_bring_up(void)
{
	/* bring up sequence for Marimba codec core
	 * ensure RESET_N = 0 and GDFS_CLAMP_EN=1 -
	 * set GDFS_EN_FEW=1 then GDFS_EN_REST=1 then
	 * GDFS_CLAMP_EN = 0 and finally RESET_N = 1
	 * Marimba codec bring up should use the Marimba
	 * slave address after which the codec slave
	 * address can be used
	 */

	/* Bring up codec */
	adie_codec_write(0xFF, 0xFF, 0x08);

	/* set GDFS_EN_FEW=1 */
	adie_codec_write(0xFF, 0xFF, 0x0a);

	/* set GDFS_EN_REST=1 */
	adie_codec_write(0xFF, 0xFF, 0x0e);

	/* set RESET_N=1 */
	adie_codec_write(0xFF, 0xFF, 0x07);

	adie_codec_write(0xFF, 0xFF, 0x17);

	/* enable band gap */
	adie_codec_write(0x03, 0xFF, 0x04);

	/* dither delay selected and dmic gain stage bypassed */
	adie_codec_write(0x8F, 0xFF, 0x44);
}

static void marimba_codec_bring_down(void)
{
	adie_codec_write(0xFF, 0xFF, 0x07);
	adie_codec_write(0xFF, 0xFF, 0x06);
	adie_codec_write(0xFF, 0xFF, 0x0e);
	adie_codec_write(0xFF, 0xFF, 0x08);
	adie_codec_write(0x03, 0xFF, 0x00);
}

static int marimba_adie_codec_open(struct adie_codec_dev_profile *profile,
	struct adie_codec_path **path_pptr)
{
	int rc = 0;

	mutex_lock(&adie_codec.lock);

	if (!profile || !path_pptr) {
		rc = -EINVAL;
		goto error;
	}

	if (adie_codec.path[profile->path_type].profile) {
		rc = -EBUSY;
		goto error;
	}

	if (!adie_codec.ref_cnt) {

		if (adie_codec.codec_pdata &&
				adie_codec.codec_pdata->marimba_codec_power) {

			rc = adie_codec.codec_pdata->marimba_codec_power(1);
			if (rc) {
				pr_err("%s: could not power up marimba "
						"codec\n", __func__);
				goto error;
			}
		}
		marimba_codec_bring_up();
	}

	adie_codec.path[profile->path_type].profile = profile;
	*path_pptr = (void *) &adie_codec.path[profile->path_type];
	adie_codec.ref_cnt++;
	adie_codec.path[profile->path_type].hwsetting_idx = 0;
	adie_codec.path[profile->path_type].curr_stage = ADIE_CODEC_FLASH_IMAGE;
	adie_codec.path[profile->path_type].stage_idx = 0;


error:

	mutex_unlock(&adie_codec.lock);
	return rc;
}

static int marimba_adie_codec_close(struct adie_codec_path *path_ptr)
{
	int rc = 0;

	mutex_lock(&adie_codec.lock);

	if (!path_ptr) {
		rc = -EINVAL;
		goto error;
	}
	if (path_ptr->curr_stage != ADIE_CODEC_DIGITAL_OFF)
		adie_codec_proceed_stage(path_ptr, ADIE_CODEC_DIGITAL_OFF);

	BUG_ON(!adie_codec.ref_cnt);

	path_ptr->profile = NULL;
	adie_codec.ref_cnt--;

	if (!adie_codec.ref_cnt) {

		marimba_codec_bring_down();

		if (adie_codec.codec_pdata &&
				adie_codec.codec_pdata->marimba_codec_power) {

			rc = adie_codec.codec_pdata->marimba_codec_power(0);
			if (rc) {
				pr_err("%s: could not power down marimba "
						"codec\n", __func__);
				goto error;
			}
		}
	}

error:
	mutex_unlock(&adie_codec.lock);
	return rc;
}

static const struct adie_codec_operations marimba_adie_ops = {
	.codec_id = MARIMBA_ID,
	.codec_open = marimba_adie_codec_open,
	.codec_close = marimba_adie_codec_close,
	.codec_setpath = marimba_adie_codec_setpath,
	.codec_proceed_stage = marimba_adie_codec_proceed_stage,
	.codec_freq_supported = marimba_adie_codec_freq_supported,
	.codec_enable_sidetone = marimba_adie_codec_enable_sidetone,
	.codec_set_device_digital_volume =
			marimba_adie_codec_set_device_digital_volume,
};

#ifdef CONFIG_DEBUG_FS
static struct dentry *debugfs_marimba_dent;
static struct dentry *debugfs_peek;
static struct dentry *debugfs_poke;
static struct dentry *debugfs_power;

static unsigned char read_data;

static int codec_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static int get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (strict_strtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
			}
		else
			return -EINVAL;
	}
	return 0;
}

static ssize_t codec_debug_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char lbuf[8];

	snprintf(lbuf, sizeof(lbuf), "0x%x\n", read_data);
	return simple_read_from_buffer(ubuf, count, ppos, lbuf, strlen(lbuf));
}

static ssize_t codec_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char *access_str = filp->private_data;
	char lbuf[32];
	int rc;
	long int param[5];

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';

	if (!strcmp(access_str, "power")) {
		if (get_parameters(lbuf, param, 1) == 0) {
			switch (param[0]) {
			case 1:
				adie_codec.codec_pdata->marimba_codec_power(1);
				marimba_codec_bring_up();
				break;
			case 0:
				marimba_codec_bring_down();
				adie_codec.codec_pdata->marimba_codec_power(0);
				break;
			default:
				rc = -EINVAL;
				break;
			}
		} else
			rc = -EINVAL;
	} else if (!strcmp(access_str, "poke")) {
		/* write */
		rc = get_parameters(lbuf, param, 2);
		if ((param[0] <= 0xFF) && (param[1] <= 0xFF) &&
			(rc == 0))
			adie_codec_write(param[0], 0xFF, param[1]);
		else
			rc = -EINVAL;
	} else if (!strcmp(access_str, "peek")) {
		/* read */
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] <= 0xFF) && (rc == 0))
			adie_codec_read(param[0], &read_data);
		else
			rc = -EINVAL;
	}

	if (rc == 0)
		rc = cnt;
	else
		pr_err("%s: rc = %d\n", __func__, rc);

	return rc;
}

static const struct file_operations codec_debug_ops = {
	.open = codec_debug_open,
	.write = codec_debug_write,
	.read = codec_debug_read
};
#endif

static int marimba_codec_probe(struct platform_device *pdev)
{
	int rc;

	adie_codec.pdrv_ptr = platform_get_drvdata(pdev);
	adie_codec.codec_pdata = pdev->dev.platform_data;

	if (adie_codec.codec_pdata->snddev_profile_init)
		adie_codec.codec_pdata->snddev_profile_init();

	/* Register the marimba ADIE operations */
	rc = adie_codec_register_codec_operations(&marimba_adie_ops);

#ifdef CONFIG_DEBUG_FS
	debugfs_marimba_dent = debugfs_create_dir("msm_adie_codec", 0);
	if (!IS_ERR(debugfs_marimba_dent)) {
		debugfs_peek = debugfs_create_file("peek",
		S_IFREG | S_IRUGO, debugfs_marimba_dent,
		(void *) "peek", &codec_debug_ops);

		debugfs_poke = debugfs_create_file("poke",
		S_IFREG | S_IRUGO, debugfs_marimba_dent,
		(void *) "poke", &codec_debug_ops);

		debugfs_power = debugfs_create_file("power",
		S_IFREG | S_IRUGO, debugfs_marimba_dent,
		(void *) "power", &codec_debug_ops);
	}
#endif
	return rc;
}

static struct platform_driver marimba_codec_driver = {
	.probe = marimba_codec_probe,
	.driver = {
		.name = "marimba_codec",
		.owner = THIS_MODULE,
	},
};

static int __init marimba_codec_init(void)
{
	s32 rc;

	rc = platform_driver_register(&marimba_codec_driver);
	if (IS_ERR_VALUE(rc))
		goto error;

	adie_codec.path[ADIE_CODEC_TX].img.regs = adie_codec_tx_regs;
	adie_codec.path[ADIE_CODEC_TX].img.img_sz =
	ARRAY_SIZE(adie_codec_tx_regs);
	adie_codec.path[ADIE_CODEC_RX].img.regs = adie_codec_rx_regs;
	adie_codec.path[ADIE_CODEC_RX].img.img_sz =
	ARRAY_SIZE(adie_codec_rx_regs);
	adie_codec.path[ADIE_CODEC_LB].img.regs = adie_codec_lb_regs;
	adie_codec.path[ADIE_CODEC_LB].img.img_sz =
	ARRAY_SIZE(adie_codec_lb_regs);
	mutex_init(&adie_codec.lock);

error:
	return rc;
}

static void __exit marimba_codec_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove(debugfs_peek);
	debugfs_remove(debugfs_poke);
	debugfs_remove(debugfs_power);
	debugfs_remove(debugfs_marimba_dent);
#endif
	platform_driver_unregister(&marimba_codec_driver);
}

module_init(marimba_codec_init);
module_exit(marimba_codec_exit);

MODULE_DESCRIPTION("Marimba codec driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
