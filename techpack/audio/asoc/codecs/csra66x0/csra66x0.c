// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include "csra66x0.h"

#define DRV_NAME "csra66x0_codec"
#define CSRA66X0_SYSFS_ENTRY_MAX_LEN 64

/* CSRA66X0 register default values */
static struct reg_default csra66x0_reg_defaults[] = {
	{CSRA66X0_AUDIO_IF_RX_CONFIG1,           0x00},
	{CSRA66X0_AUDIO_IF_RX_CONFIG2,           0x0B},
	{CSRA66X0_AUDIO_IF_RX_CONFIG3,           0x0F},
	{CSRA66X0_AUDIO_IF_TX_EN,                0x00},
	{CSRA66X0_AUDIO_IF_TX_CONFIG1,           0x6B},
	{CSRA66X0_AUDIO_IF_TX_CONFIG2,           0x02},
	{CSRA66X0_I2C_DEVICE_ADDRESS,            0x0D},
	{CSRA66X0_CHIP_ID_FA,                    0x39},
	{CSRA66X0_ROM_VER_FA,                    0x08},
	{CSRA66X0_CHIP_REV_0_FA,                 0x05},
	{CSRA66X0_CHIP_REV_1_FA,                 0x03},
	{CSRA66X0_CH1_MIX_SEL,                   0x01},
	{CSRA66X0_CH2_MIX_SEL,                   0x10},
	{CSRA66X0_CH1_SAMPLE1_SCALE_0,           0x00},
	{CSRA66X0_CH1_SAMPLE1_SCALE_1,           0x20},
	{CSRA66X0_CH1_SAMPLE3_SCALE_0,           0x00},
	{CSRA66X0_CH1_SAMPLE3_SCALE_1,           0x20},
	{CSRA66X0_CH1_SAMPLE5_SCALE_0,           0x00},
	{CSRA66X0_CH1_SAMPLE5_SCALE_1,           0x20},
	{CSRA66X0_CH1_SAMPLE7_SCALE_0,           0x00},
	{CSRA66X0_CH1_SAMPLE7_SCALE_1,           0x20},
	{CSRA66X0_CH1_SAMPLE2_SCALE_0,           0x00},
	{CSRA66X0_CH1_SAMPLE2_SCALE_1,           0x20},
	{CSRA66X0_CH1_SAMPLE4_SCALE_0,           0x00},
	{CSRA66X0_CH1_SAMPLE4_SCALE_1,           0x20},
	{CSRA66X0_CH1_SAMPLE6_SCALE_0,           0x00},
	{CSRA66X0_CH1_SAMPLE6_SCALE_1,           0x20},
	{CSRA66X0_CH1_SAMPLE8_SCALE_0,           0x00},
	{CSRA66X0_CH1_SAMPLE8_SCALE_1,           0x20},
	{CSRA66X0_CH2_SAMPLE1_SCALE_0,           0x00},
	{CSRA66X0_CH2_SAMPLE1_SCALE_1,           0x20},
	{CSRA66X0_CH2_SAMPLE3_SCALE_0,           0x00},
	{CSRA66X0_CH2_SAMPLE3_SCALE_1,           0x20},
	{CSRA66X0_CH2_SAMPLE5_SCALE_0,           0x00},
	{CSRA66X0_CH2_SAMPLE5_SCALE_1,           0x20},
	{CSRA66X0_CH2_SAMPLE7_SCALE_0,           0x00},
	{CSRA66X0_CH2_SAMPLE7_SCALE_1,           0x20},
	{CSRA66X0_CH2_SAMPLE2_SCALE_0,           0x00},
	{CSRA66X0_CH2_SAMPLE2_SCALE_1,           0x20},
	{CSRA66X0_CH2_SAMPLE4_SCALE_0,           0x00},
	{CSRA66X0_CH2_SAMPLE4_SCALE_1,           0x20},
	{CSRA66X0_CH2_SAMPLE6_SCALE_0,           0x00},
	{CSRA66X0_CH2_SAMPLE6_SCALE_1,           0x20},
	{CSRA66X0_CH2_SAMPLE8_SCALE_0,           0x00},
	{CSRA66X0_CH2_SAMPLE8_SCALE_1,           0x20},
	{CSRA66X0_VOLUME_CONFIG_FA,              0x26},
	{CSRA66X0_STARTUP_DELAY_FA,              0x00},
	{CSRA66X0_CH1_VOLUME_0_FA,               0x19},
	{CSRA66X0_CH1_VOLUME_1_FA,               0x01},
	{CSRA66X0_CH2_VOLUME_0_FA,               0x19},
	{CSRA66X0_CH2_VOLUME_1_FA,               0x01},
	{CSRA66X0_QUAD_ENC_COUNT_0_FA,           0x00},
	{CSRA66X0_QUAD_ENC_COUNT_1_FA,           0x00},
	{CSRA66X0_SOFT_CLIP_CONFIG,              0x00},
	{CSRA66X0_CH1_HARD_CLIP_THRESH,          0x00},
	{CSRA66X0_CH2_HARD_CLIP_THRESH,          0x00},
	{CSRA66X0_SOFT_CLIP_THRESH,              0x00},
	{CSRA66X0_DS_ENABLE_THRESH_0,            0x05},
	{CSRA66X0_DS_ENABLE_THRESH_1,            0x00},
	{CSRA66X0_DS_TARGET_COUNT_0,             0x00},
	{CSRA66X0_DS_TARGET_COUNT_1,             0xFF},
	{CSRA66X0_DS_TARGET_COUNT_2,             0xFF},
	{CSRA66X0_DS_DISABLE_THRESH_0,           0x0F},
	{CSRA66X0_DS_DISABLE_THRESH_1,           0x00},
	{CSRA66X0_DCA_CTRL,                      0x07},
	{CSRA66X0_CH1_DCA_THRESH,                0x40},
	{CSRA66X0_CH2_DCA_THRESH,                0x40},
	{CSRA66X0_DCA_ATTACK_RATE,               0x00},
	{CSRA66X0_DCA_RELEASE_RATE,              0x00},
	{CSRA66X0_CH1_OUTPUT_INVERT_EN,          0x00},
	{CSRA66X0_CH2_OUTPUT_INVERT_EN,          0x00},
	{CSRA66X0_CH1_176P4K_DELAY,              0x00},
	{CSRA66X0_CH2_176P4K_DELAY,              0x00},
	{CSRA66X0_CH1_192K_DELAY,                0x00},
	{CSRA66X0_CH2_192K_DELAY,                0x00},
	{CSRA66X0_DEEMP_CONFIG_FA,               0x00},
	{CSRA66X0_CH1_TREBLE_GAIN_CTRL_FA,       0x00},
	{CSRA66X0_CH2_TREBLE_GAIN_CTRL_FA,       0x00},
	{CSRA66X0_CH1_TREBLE_FC_CTRL_FA,         0x00},
	{CSRA66X0_CH2_TREBLE_FC_CTRL_FA,         0x00},
	{CSRA66X0_CH1_BASS_GAIN_CTRL_FA,         0x00},
	{CSRA66X0_CH2_BASS_GAIN_CTRL_FA,         0x00},
	{CSRA66X0_CH1_BASS_FC_CTRL_FA,           0x00},
	{CSRA66X0_CH2_BASS_FC_CTRL_FA,           0x00},
	{CSRA66X0_FILTER_SEL_8K,                 0x00},
	{CSRA66X0_FILTER_SEL_11P025K,            0x00},
	{CSRA66X0_FILTER_SEL_16K,                0x00},
	{CSRA66X0_FILTER_SEL_22P05K,             0x00},
	{CSRA66X0_FILTER_SEL_32K,                0x00},
	{CSRA66X0_FILTER_SEL_44P1K_48K,          0x00},
	{CSRA66X0_FILTER_SEL_88P2K_96K,          0x00},
	{CSRA66X0_FILTER_SEL_176P4K_192K,        0x00},
	/* RESERVED */
	{CSRA66X0_USER_DSP_CTRL,                 0x00},
	{CSRA66X0_TEST_TONE_CTRL,                0x00},
	{CSRA66X0_TEST_TONE_FREQ_0,              0x00},
	{CSRA66X0_TEST_TONE_FREQ_1,              0x00},
	{CSRA66X0_TEST_TONE_FREQ_2,              0x00},
	{CSRA66X0_AUDIO_RATE_CTRL_FA,            0x08},
	{CSRA66X0_MODULATION_INDEX_CTRL,         0x3F},
	{CSRA66X0_MODULATION_INDEX_COUNT,        0x10},
	{CSRA66X0_MIN_MODULATION_PULSE_WIDTH,    0x7A},
	{CSRA66X0_DEAD_TIME_CTRL,                0x00},
	{CSRA66X0_DEAD_TIME_THRESHOLD_0,         0xE7},
	{CSRA66X0_DEAD_TIME_THRESHOLD_1,         0x26},
	{CSRA66X0_DEAD_TIME_THRESHOLD_2,         0x40},
	{CSRA66X0_CH1_LOW_SIDE_DLY,              0x00},
	{CSRA66X0_CH2_LOW_SIDE_DLY,              0x00},
	{CSRA66X0_SPECTRUM_CTRL,                 0x00},
	/* RESERVED */
	{CSRA66X0_SPECTRUM_SPREAD_CTRL,          0x0C},
	/* RESERVED */
	{CSRA66X0_EXT_PA_PROTECT_POLARITY,       0x03},
	{CSRA66X0_TEMP0_BACKOFF_COMP_VALUE,      0x98},
	{CSRA66X0_TEMP0_SHUTDOWN_COMP_VALUE,     0xA3},
	{CSRA66X0_TEMP1_BACKOFF_COMP_VALUE,      0x98},
	{CSRA66X0_TEMP1_SHUTDOWN_COMP_VALUE,     0xA3},
	{CSRA66X0_TEMP_PROT_BACKOFF,             0x00},
	{CSRA66X0_TEMP_READ0_FA,                 0x00},
	{CSRA66X0_TEMP_READ1_FA,                 0x00},
	{CSRA66X0_CHIP_STATE_CTRL_FA,            0x02},
	/* RESERVED */
	{CSRA66X0_PWM_OUTPUT_CONFIG,             0x00},
	{CSRA66X0_MISC_CONTROL_STATUS_0,         0x08},
	{CSRA66X0_MISC_CONTROL_STATUS_1_FA,      0x40},
	{CSRA66X0_PIO0_SELECT,                   0x00},
	{CSRA66X0_PIO1_SELECT,                   0x00},
	{CSRA66X0_PIO2_SELECT,                   0x00},
	{CSRA66X0_PIO3_SELECT,                   0x00},
	{CSRA66X0_PIO4_SELECT,                   0x00},
	{CSRA66X0_PIO5_SELECT,                   0x00},
	{CSRA66X0_PIO6_SELECT,                   0x00},
	{CSRA66X0_PIO7_SELECT,                   0x00},
	{CSRA66X0_PIO8_SELECT,                   0x00},
	{CSRA66X0_PIO_DIRN0,                     0xFF},
	{CSRA66X0_PIO_DIRN1,                     0x01},
	{CSRA66X0_PIO_PULL_EN0,                  0xFF},
	{CSRA66X0_PIO_PULL_EN1,                  0x01},
	{CSRA66X0_PIO_PULL_DIR0,                 0x00},
	{CSRA66X0_PIO_PULL_DIR1,                 0x00},
	{CSRA66X0_PIO_DRIVE_OUT0_FA,             0x00},
	{CSRA66X0_PIO_DRIVE_OUT1_FA,             0x00},
	{CSRA66X0_PIO_STATUS_IN0_FA,             0x00},
	{CSRA66X0_PIO_STATUS_IN1_FA,             0x00},
	/* RESERVED */
	{CSRA66X0_IRQ_OUTPUT_ENABLE,             0x00},
	{CSRA66X0_IRQ_OUTPUT_POLARITY,           0x01},
	{CSRA66X0_IRQ_OUTPUT_STATUS_FA,          0x00},
	{CSRA66X0_CLIP_DCA_STATUS_FA,            0x00},
	{CSRA66X0_CHIP_STATE_STATUS_FA,          0x02},
	{CSRA66X0_FAULT_STATUS_FA,               0x00},
	{CSRA66X0_OTP_STATUS_FA,                 0x00},
	{CSRA66X0_AUDIO_IF_STATUS_FA,            0x00},
	/* RESERVED */
	{CSRA66X0_DSP_SATURATION_STATUS_FA,      0x00},
	{CSRA66X0_AUDIO_RATE_STATUS_FA,          0x00},
	/* RESERVED */
	{CSRA66X0_DISABLE_PWM_OUTPUT,            0x00},
	/* RESERVED */
	{CSRA66X0_OTP_VER_FA,                    0x03},
	{CSRA66X0_RAM_VER_FA,                    0x02},
	/* RESERVED */
	{CSRA66X0_AUDIO_SATURATION_FLAGS_FA,     0x00},
	{CSRA66X0_DCOFFSET_CHAN_1_01_FA,         0x00},
	{CSRA66X0_DCOFFSET_CHAN_1_02_FA,         0x00},
	{CSRA66X0_DCOFFSET_CHAN_1_03_FA,         0x00},
	{CSRA66X0_DCOFFSET_CHAN_2_01_FA,         0x00},
	{CSRA66X0_DCOFFSET_CHAN_2_02_FA,         0x00},
	{CSRA66X0_DCOFFSET_CHAN_2_03_FA,         0x00},
	{CSRA66X0_FORCED_PA_SWITCHING_CTRL,      0x90},
	{CSRA66X0_PA_FORCE_PULSE_WIDTH,          0x07},
	{CSRA66X0_PA_HIGH_MODULATION_CTRL_CH1,   0x00},
	/* RESERVED */
	{CSRA66X0_HIGH_MODULATION_THRESHOLD_LOW, 0xD4},
	{CSRA66X0_HIGH_MODULATION_THRESHOLD_HIGH, 0x78},
	/* RESERVED */
	{CSRA66X0_PA_FREEZE_CTRL,                0x00},
	{CSRA66X0_DCA_FREEZE_CTRL,               0x3C},
	/* RESERVED */
};

static bool csra66x0_addr_is_in_range(unsigned int addr,
	unsigned int addr_min, unsigned int addr_max)
{
	if ((addr >= addr_min)
			&& (addr <= addr_max))
		return true;
	else
		return false;
}

static bool csra66x0_volatile_register(struct device *dev, unsigned int reg)
{
	/* coeff registers */
	if (csra66x0_addr_is_in_range(reg, CSRA66X0_COEFF_BASE,
		CSRA66X0_MAX_COEFF_ADDR))
		return true;

	/* control registers */
	switch (reg) {
	case CSRA66X0_CHIP_ID_FA:
	case CSRA66X0_ROM_VER_FA:
	case CSRA66X0_CHIP_REV_0_FA:
	case CSRA66X0_CHIP_REV_1_FA:
	case CSRA66X0_TEMP_READ0_FA:
	case CSRA66X0_TEMP_READ1_FA:
	case CSRA66X0_CHIP_STATE_CTRL_FA:
	case CSRA66X0_MISC_CONTROL_STATUS_1_FA:
	case CSRA66X0_IRQ_OUTPUT_STATUS_FA:
	case CSRA66X0_CLIP_DCA_STATUS_FA:
	case CSRA66X0_CHIP_STATE_STATUS_FA:
	case CSRA66X0_FAULT_STATUS_FA:
	case CSRA66X0_OTP_STATUS_FA:
	case CSRA66X0_AUDIO_IF_STATUS_FA:
	case CSRA66X0_DSP_SATURATION_STATUS_FA:
	case CSRA66X0_AUDIO_RATE_STATUS_FA:
	case CSRA66X0_CH1_MIX_SEL:
	case CSRA66X0_CH2_MIX_SEL:
	case CSRA66X0_CH1_SAMPLE1_SCALE_0:
	case CSRA66X0_CH1_SAMPLE1_SCALE_1:
	case CSRA66X0_CH1_SAMPLE3_SCALE_0:
	case CSRA66X0_CH1_SAMPLE3_SCALE_1:
	case CSRA66X0_CH1_SAMPLE5_SCALE_0:
	case CSRA66X0_CH1_SAMPLE5_SCALE_1:
	case CSRA66X0_CH1_SAMPLE7_SCALE_0:
	case CSRA66X0_CH1_SAMPLE7_SCALE_1:
	case CSRA66X0_CH1_SAMPLE2_SCALE_0:
	case CSRA66X0_CH1_SAMPLE2_SCALE_1:
	case CSRA66X0_CH1_SAMPLE4_SCALE_0:
	case CSRA66X0_CH1_SAMPLE4_SCALE_1:
	case CSRA66X0_CH1_SAMPLE6_SCALE_0:
	case CSRA66X0_CH1_SAMPLE6_SCALE_1:
	case CSRA66X0_CH1_SAMPLE8_SCALE_0:
	case CSRA66X0_CH1_SAMPLE8_SCALE_1:
	case CSRA66X0_CH2_SAMPLE1_SCALE_0:
	case CSRA66X0_CH2_SAMPLE1_SCALE_1:
	case CSRA66X0_CH2_SAMPLE3_SCALE_0:
	case CSRA66X0_CH2_SAMPLE3_SCALE_1:
	case CSRA66X0_CH2_SAMPLE5_SCALE_0:
	case CSRA66X0_CH2_SAMPLE5_SCALE_1:
	case CSRA66X0_CH2_SAMPLE7_SCALE_0:
	case CSRA66X0_CH2_SAMPLE7_SCALE_1:
	case CSRA66X0_CH2_SAMPLE2_SCALE_0:
	case CSRA66X0_CH2_SAMPLE2_SCALE_1:
	case CSRA66X0_CH2_SAMPLE4_SCALE_0:
	case CSRA66X0_CH2_SAMPLE4_SCALE_1:
	case CSRA66X0_CH2_SAMPLE6_SCALE_0:
	case CSRA66X0_CH2_SAMPLE6_SCALE_1:
	case CSRA66X0_CH2_SAMPLE8_SCALE_0:
	case CSRA66X0_CH2_SAMPLE8_SCALE_1:
	case CSRA66X0_RAM_VER_FA:
		return true;
	default:
		return false;
	}
}

static bool csra66x0_writeable_registers(struct device *dev, unsigned int reg)
{
	if (csra66x0_addr_is_in_range(reg, CSRA66X0_BASE,
		CSRA66X0_MAX_REGISTER_ADDR)
		|| csra66x0_addr_is_in_range(reg, CSRA66X0_COEFF_BASE,
		CSRA66X0_MAX_COEFF_ADDR))
		return true;
	else
		return false;
}

static bool csra66x0_readable_registers(struct device *dev, unsigned int reg)
{
	if (csra66x0_addr_is_in_range(reg, CSRA66X0_BASE,
		CSRA66X0_MAX_REGISTER_ADDR)
		|| csra66x0_addr_is_in_range(reg, CSRA66X0_COEFF_BASE,
		CSRA66X0_MAX_COEFF_ADDR))
		return true;
	else
		return false;
}

/* codec private data */
struct csra66x0_priv {
	struct regmap *regmap;
	struct snd_soc_component *component;
	int spk_volume_ch1;
	int spk_volume_ch2;
	int irq;
	int vreg_gpio;
	u32 irq_active_low;
	u32 in_cluster;
	u32 is_master;
	bool is_probed;
	u32 max_num_cluster_devices;
	u32 num_cluster_devices;
	u32 sysfs_reg_addr;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	struct dentry *debugfs_dir;
	struct dentry *debugfs_file_wo;
	struct dentry *debugfs_file_ro;
#endif /* CONFIG_DEBUG_FS */
};

struct csra66x0_cluster_device {
	struct csra66x0_priv *csra66x0_ptr;
	const char *csra66x0_prefix;
};

struct csra66x0_cluster_device csra_clust_dev_tbl[] = {
	{NULL, "CSRA_12"},
	{NULL, "CSRA_34"},
	{NULL, "CSRA_56"},
	{NULL, "CSRA_78"},
	{NULL, "CSRA_9A"},
	{NULL, "CSRA_BC"},
	{NULL, "CSRA_DE"},
	{NULL, "CSRA_F0"}
};

static int sysfs_get_param(char *buf, u32 *param, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");
	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtou32(token, base, &param[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else {
			return -EINVAL;
		}
	}
	return 0;
}

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int debugfs_codec_open_op(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t debugfs_codec_write_op(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct csra66x0_priv *csra66x0 =
			(struct csra66x0_priv *) filp->private_data;
	struct snd_soc_component *component = csra66x0->component;
	char lbuf[32];
	int rc;
	u32 param[2];

	if (!filp || !ppos || !ubuf || !component)
		return -EINVAL;
	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;
	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;
	lbuf[cnt] = '\0';
	rc = sysfs_get_param(lbuf, param, 2);

	if (!(csra66x0_addr_is_in_range(param[0],
		CSRA66X0_BASE, CSRA66X0_MAX_REGISTER_ADDR)
		|| csra66x0_addr_is_in_range(param[0],
		CSRA66X0_COEFF_BASE, CSRA66X0_MAX_COEFF_ADDR))) {
		dev_err(component->dev, "%s: register address 0x%04X out of range\n",
			__func__, param[0]);
		return -EINVAL;
	}
	if ((param[1] < 0) || (param[1] > 255)) {
		dev_err(component->dev, "%s: register data 0x%02X out of range\n",
			__func__, param[1]);
		return -EINVAL;
	}
	if (rc == 0)
	{
		rc = cnt;
		dev_info(component->dev, "%s: reg[0x%04X]=0x%02X\n",
			__func__, param[0], param[1]);
		snd_soc_component_write(component, param[0], param[1]);
	} else {
		dev_err(component->dev, "%s: write to register addr=0x%04X failed\n",
			__func__, param[0]);
	}
	return rc;
}

static ssize_t debugfs_csra66x0_reg_show(struct csra66x0_priv *csra66x0,
		char __user *ubuf, size_t count, loff_t *ppos)
{
	int i, reg_val, len;
	int addr_min, addr_max;
	ssize_t total = 0;
	char tmp_buf[20];
	struct snd_soc_component *component = csra66x0->component;

	if (!ubuf || !ppos || !component || *ppos < 0)
		return -EINVAL;

	if (csra66x0_addr_is_in_range(csra66x0->sysfs_reg_addr,
		CSRA66X0_COEFF_BASE, CSRA66X0_MAX_COEFF_ADDR)) {
		addr_min = CSRA66X0_COEFF_BASE;
		addr_max = CSRA66X0_MAX_COEFF_ADDR;
		csra66x0->sysfs_reg_addr = CSRA66X0_BASE;
	} else {
		addr_min = CSRA66X0_BASE;
		addr_max = CSRA66X0_MAX_REGISTER_ADDR;
	}

	for (i = ((int) *ppos + addr_min);
		i <= addr_max; i++) {
		reg_val = snd_soc_component_read32(component, i);
		len = snprintf(tmp_buf, 20, "0x%04X: 0x%02X\n", i, (reg_val & 0xFF));
		if ((total + len) >= count - 1)
			break;
		if (copy_to_user((ubuf + total), tmp_buf, len)) {
			dev_err(component->dev, "%s: fail to copy reg dump\n",
				__func__);
			total = -EFAULT;
			goto copy_err;
		}
		*ppos += len;
		total += len;
	}

copy_err:
	return total;
}

static ssize_t debugfs_codec_read_op(struct file *filp,
		char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct csra66x0_priv *csra66x0 =
		(struct csra66x0_priv *) filp->private_data;
	ssize_t ret_cnt;

	if (!filp || !ppos || !ubuf || *ppos < 0)
		return -EINVAL;
	ret_cnt = debugfs_csra66x0_reg_show(csra66x0, ubuf, cnt, ppos);
	return ret_cnt;
}

static const struct file_operations debugfs_codec_ops = {
	.open = debugfs_codec_open_op,
	.write = debugfs_codec_write_op,
	.read = debugfs_codec_read_op,
};
#endif /* CONFIG_DEBUG_FS */

/*
 * CSRA66X0 Controls
 */
static const DECLARE_TLV_DB_SCALE(csra66x0_volume_tlv, -9000, 25, 0);
static const DECLARE_TLV_DB_RANGE(csra66x0_bass_treble_tlv,
		0,  0, TLV_DB_SCALE_ITEM(0,   0, 0),
		1, 15, TLV_DB_SCALE_ITEM(-1500, 100, 0),
		16, 30, TLV_DB_SCALE_ITEM(100, 100, 0)
);

static int csra66x0_get_volume(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	unsigned int reg_l = mc->reg;
	unsigned int reg_r = mc->rreg;
	unsigned int val_l, val_r;

	val_l = (snd_soc_component_read32(component, reg_l) & 0xff) |
			((snd_soc_component_read32(component,
			CSRA66X0_CH1_VOLUME_1_FA) & (0x01)) << 8);
	val_r = (snd_soc_component_read32(component, reg_r) & 0xff) |
			((snd_soc_component_read32(component,
			CSRA66X0_CH2_VOLUME_1_FA) & (0x01)) << 8);
	ucontrol->value.integer.value[0] = val_l;
	ucontrol->value.integer.value[1] = val_r;
	return 0;
}

static int csra66x0_set_volume(struct snd_kcontrol *kcontrol,
			struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct csra66x0_priv *csra66x0 =
			snd_soc_component_get_drvdata(component);
	unsigned int reg_l = mc->reg;
	unsigned int reg_r = mc->rreg;
	unsigned int val_l[2];
	unsigned int val_r[2];

	csra66x0->spk_volume_ch1 = (ucontrol->value.integer.value[0]);
	csra66x0->spk_volume_ch2 = (ucontrol->value.integer.value[1]);
	val_l[0] = csra66x0->spk_volume_ch1 & SPK_VOLUME_LSB_MSK;
	val_l[1] = (csra66x0->spk_volume_ch1 & SPK_VOLUME_MSB_MSK) ? 1 : 0;
	val_r[0] = csra66x0->spk_volume_ch2 & SPK_VOLUME_LSB_MSK;
	val_r[1] = (csra66x0->spk_volume_ch2 & SPK_VOLUME_MSB_MSK) ? 1 : 0;
	snd_soc_component_write(component, reg_l, val_l[0]);
	snd_soc_component_write(component, reg_r, val_r[0]);
	snd_soc_component_write(component, CSRA66X0_CH1_VOLUME_1_FA, val_l[1]);
	snd_soc_component_write(component, CSRA66X0_CH2_VOLUME_1_FA, val_r[1]);
	return 0;
}

/* enumerated controls */
static const char * const csra66x0_mute_output_text[] = {"PLAY", "MUTE"};
static const char * const csra66x0_output_invert_text[] = {
		"UNCHANGED", "INVERTED"};
static const char * const csra66x0_deemp_config_text[] = {
		"DISABLED", "ENABLED"};

SOC_ENUM_SINGLE_DECL(csra66x0_mute_output_enum,
			CSRA66X0_MISC_CONTROL_STATUS_1_FA, 2,
			csra66x0_mute_output_text);
SOC_ENUM_SINGLE_DECL(csra66x0_ch1_output_invert_enum,
			CSRA66X0_CH1_OUTPUT_INVERT_EN, 0,
			csra66x0_output_invert_text);
SOC_ENUM_SINGLE_DECL(csra66x0_ch2_output_invert_enum,
			CSRA66X0_CH2_OUTPUT_INVERT_EN, 0,
			csra66x0_output_invert_text);
SOC_ENUM_DOUBLE_DECL(csra66x0_deemp_config_enum,
			CSRA66X0_DEEMP_CONFIG_FA, 0, 1,
			csra66x0_deemp_config_text);

static const struct snd_kcontrol_new csra66x0_snd_controls[] = {
	/* volume */
	SOC_DOUBLE_R_EXT_TLV("PA VOLUME", CSRA66X0_CH1_VOLUME_0_FA,
			CSRA66X0_CH2_VOLUME_0_FA, 0, 0x1C9, 0,
			csra66x0_get_volume, csra66x0_set_volume,
			csra66x0_volume_tlv),

	/* bass treble */
	SOC_DOUBLE_R_TLV("PA BASS GAIN", CSRA66X0_CH1_BASS_GAIN_CTRL_FA,
			CSRA66X0_CH2_BASS_GAIN_CTRL_FA, 0, 0x1E, 0,
			csra66x0_bass_treble_tlv),
	SOC_DOUBLE_R_TLV("PA TREBLE GAIN", CSRA66X0_CH1_TREBLE_GAIN_CTRL_FA,
			CSRA66X0_CH2_TREBLE_GAIN_CTRL_FA, 0, 0x1E, 0,
			csra66x0_bass_treble_tlv),
	SOC_DOUBLE_R("PA BASS_XOVER FREQ", CSRA66X0_CH1_BASS_FC_CTRL_FA,
			CSRA66X0_CH2_BASS_FC_CTRL_FA, 0, 2, 0),
	SOC_DOUBLE_R("PA TREBLE_XOVER FREQ", CSRA66X0_CH1_TREBLE_FC_CTRL_FA,
			CSRA66X0_CH2_TREBLE_FC_CTRL_FA, 0, 2, 0),

	/* switch */
	SOC_ENUM("PA MUTE_OUTPUT SWITCH", csra66x0_mute_output_enum),
	SOC_ENUM("PA DE-EMPHASIS SWITCH", csra66x0_deemp_config_enum),
};

static const struct snd_kcontrol_new csra_mix_switch[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static const struct snd_soc_dapm_widget csra66x0_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("IN"),
	SND_SOC_DAPM_MIXER("MIXER", SND_SOC_NOPM, 0, 0,
			csra_mix_switch, ARRAY_SIZE(csra_mix_switch)),
	SND_SOC_DAPM_DAC("DAC", NULL, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA("PGA", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_OUTPUT("SPKR"),
};

static const struct snd_soc_dapm_route csra66x0_dapm_routes[] = {
	{"MIXER", "Switch", "IN"},
	{"DAC", NULL, "MIXER"},
	{"PGA", NULL, "DAC"},
	{"SPKR", NULL, "PGA"},
};
/*
 * csra66x0_hw_free_mute - Update csra66x0 mute register
 *
 * @component - csra66x0 component
 *
 */
void csra66x0_hw_free_mute(struct snd_soc_component *component)
{
	int val = 0;

	if (component == NULL)
		return;

	val = snd_soc_component_read32(component,
			CSRA66X0_MISC_CONTROL_STATUS_1_FA);
	snd_soc_component_write(component, CSRA66X0_MISC_CONTROL_STATUS_1_FA,
			val | 0x04);
}
EXPORT_SYMBOL(csra66x0_hw_free_mute);

static int csra66x0_wait_for_config_state(struct snd_soc_component *component)
{
	u16 val;
	int cntdwn = WAIT_FOR_CONFIG_STATE_TIMEOUT_MS;

	do {
		/* wait >= 100ms to check if HW has moved to config state */
		msleep(100);
		val = snd_soc_component_read32(component,
				CSRA66X0_CHIP_STATE_STATUS_FA);
		if (val == CONFIG_STATE_ID)
			break;

		cntdwn = cntdwn - 100;
	} while (cntdwn > 0);
	if (cntdwn <= 0)
		return -EFAULT;

	return 0;
}

static int csra66x0_allow_run(struct csra66x0_priv *csra66x0)
{
	struct snd_soc_component *component = csra66x0->component;
	int i;

	/* csra66x0 is not in cluster */
	if (!csra66x0->in_cluster) {
		/* enable interrupts */
		if (csra66x0->irq) {
			snd_soc_component_write(component,
				CSRA66X0_PIO0_SELECT, 0x1);
			if (csra66x0->irq_active_low)
				snd_soc_component_write(component,
					CSRA66X0_IRQ_OUTPUT_POLARITY, 0x0);
			else
				snd_soc_component_write(component,
					CSRA66X0_IRQ_OUTPUT_POLARITY, 0x1);

			snd_soc_component_write(component,
				CSRA66X0_IRQ_OUTPUT_ENABLE, 0x01);
		} else {
			snd_soc_component_write(component,
				CSRA66X0_IRQ_OUTPUT_ENABLE, 0x00);
		}
		/* allow run */
		snd_soc_component_write(component,
			CSRA66X0_CHIP_STATE_CTRL_FA, SET_RUN_STATE);
		return 0;
	}

	/* csra66x0 is part of cluster */
	/* get number of probed cluster devices */
	csra66x0->num_cluster_devices = 0;
	for (i = 0; i < component->card->num_aux_devs; i++) {
		if (i >= csra66x0->max_num_cluster_devices)
			break;
		if (csra_clust_dev_tbl[i].csra66x0_ptr == NULL)
			continue;
		if (csra_clust_dev_tbl[i].csra66x0_ptr->is_probed)
			csra66x0->num_cluster_devices++;
	}

	/* check if all cluster devices are probed */
	if (csra66x0->num_cluster_devices
		== component->card->num_aux_devs) {
		/* allow run of all slave components */
		for (i = 0; i < component->card->num_aux_devs; i++) {
			if (i >= csra66x0->max_num_cluster_devices)
				break;
			if (csra_clust_dev_tbl[i].csra66x0_ptr == NULL)
				continue;
			if (csra_clust_dev_tbl[i].csra66x0_ptr->is_master)
				continue;
			snd_soc_component_write(
				csra_clust_dev_tbl[i].csra66x0_ptr->component,
				CSRA66X0_CHIP_STATE_CTRL_FA, SET_RUN_STATE);
		}
		/* allow run of all master components */
		for (i = 0; i < component->card->num_aux_devs; i++) {
			if (i >= csra66x0->max_num_cluster_devices)
				break;
			if (csra_clust_dev_tbl[i].csra66x0_ptr == NULL)
				continue;
			if (!csra_clust_dev_tbl[i].csra66x0_ptr->is_master)
				continue;

			/* enable interrupts */
			if (csra66x0->irq) {
				snd_soc_component_write(component,
					CSRA66X0_PIO0_SELECT, 0x1);
				if (csra66x0->irq_active_low)
					snd_soc_component_write(component,
						CSRA66X0_IRQ_OUTPUT_POLARITY,
						0x0);
				else
					snd_soc_component_write(component,
						CSRA66X0_IRQ_OUTPUT_POLARITY,
						0x1);

				snd_soc_component_write(component,
					CSRA66X0_IRQ_OUTPUT_ENABLE, 0x01);
			} else {
				snd_soc_component_write(component,
					CSRA66X0_IRQ_OUTPUT_ENABLE, 0x00);
			}
			/* allow run */
			snd_soc_component_write(
				csra_clust_dev_tbl[i].csra66x0_ptr->component,
				CSRA66X0_CHIP_STATE_CTRL_FA, SET_RUN_STATE);
		}
	}
	return 0;
}

static int csra66x0_init(struct csra66x0_priv *csra66x0)
{
	struct snd_soc_component *component = csra66x0->component;
	int ret;

	dev_dbg(component->dev, "%s: initialize %s\n",
		__func__, component->name);
	csra66x0->sysfs_reg_addr = CSRA66X0_BASE;
	/* config */
	snd_soc_component_write(component, CSRA66X0_CHIP_STATE_CTRL_FA,
				SET_CONFIG_STATE);
	/* wait until HW is in config state before proceeding */
	ret = csra66x0_wait_for_config_state(component);
	if (ret) {
		dev_err(component->dev, "%s: timeout while %s is waiting for config state\n",
			__func__, component->name);
	}

	/* setup */
	snd_soc_component_write(component, CSRA66X0_MISC_CONTROL_STATUS_0,
				0x09);
	snd_soc_component_write(component, CSRA66X0_TEMP_PROT_BACKOFF, 0x0C);
	snd_soc_component_write(component, CSRA66X0_EXT_PA_PROTECT_POLARITY,
				0x03);
	snd_soc_component_write(component, CSRA66X0_PWM_OUTPUT_CONFIG, 0xC8);
	csra66x0->spk_volume_ch1 = SPK_VOLUME_M20DB;
	csra66x0->spk_volume_ch2 = SPK_VOLUME_M20DB;
	snd_soc_component_write(component, CSRA66X0_CH1_VOLUME_0_FA,
				SPK_VOLUME_M20DB_LSB);
	snd_soc_component_write(component, CSRA66X0_CH2_VOLUME_0_FA,
				SPK_VOLUME_M20DB_LSB);
	snd_soc_component_write(component, CSRA66X0_CH1_VOLUME_1_FA,
				SPK_VOLUME_M20DB_MSB);
	snd_soc_component_write(component, CSRA66X0_CH2_VOLUME_1_FA,
				SPK_VOLUME_M20DB_MSB);

	/* disable volume ramping */
	snd_soc_component_write(component, CSRA66X0_VOLUME_CONFIG_FA, 0x27);

	snd_soc_component_write(component, CSRA66X0_DEAD_TIME_CTRL, 0x0);
	snd_soc_component_write(component, CSRA66X0_DEAD_TIME_THRESHOLD_0,
				0xE7);
	snd_soc_component_write(component, CSRA66X0_DEAD_TIME_THRESHOLD_1,
				0x26);
	snd_soc_component_write(component, CSRA66X0_DEAD_TIME_THRESHOLD_2,
				0x40);

	snd_soc_component_write(component, CSRA66X0_MIN_MODULATION_PULSE_WIDTH,
				0x7A);
	snd_soc_component_write(component, CSRA66X0_CH1_HARD_CLIP_THRESH, 0x00);
	snd_soc_component_write(component, CSRA66X0_CH2_HARD_CLIP_THRESH, 0x00);

	snd_soc_component_write(component, CSRA66X0_CH1_DCA_THRESH, 0x40);
	snd_soc_component_write(component, CSRA66X0_CH2_DCA_THRESH, 0x40);
	snd_soc_component_write(component, CSRA66X0_DCA_ATTACK_RATE, 0x00);
	snd_soc_component_write(component, CSRA66X0_DCA_RELEASE_RATE, 0x00);

	csra66x0_allow_run(csra66x0);
	return 0;
}

static int csra66x0_reset(struct csra66x0_priv *csra66x0)
{
	struct snd_soc_component *component = csra66x0->component;
	u16 val;

	val = snd_soc_component_read32(component, CSRA66X0_FAULT_STATUS_FA);
	if (val & FAULT_STATUS_INTERNAL)
		dev_dbg(component->dev, "%s: FAULT_STATUS_INTERNAL 0x%X\n",
			__func__, val);
	if (val & FAULT_STATUS_OTP_INTEGRITY)
		dev_dbg(component->dev, "%s: FAULT_STATUS_OTP_INTEGRITY 0x%X\n",
			__func__, val);
	if (val & FAULT_STATUS_PADS2)
		dev_dbg(component->dev, "%s: FAULT_STATUS_PADS2 0x%X\n",
			__func__, val);
	if (val & FAULT_STATUS_SMPS)
		dev_dbg(component->dev, "%s: FAULT_STATUS_SMPS 0x%X\n",
			__func__, val);
	if (val & FAULT_STATUS_TEMP)
		dev_dbg(component->dev, "%s: FAULT_STATUS_TEMP 0x%X\n",
			__func__, val);
	if (val & FAULT_STATUS_PROTECT)
		dev_dbg(component->dev, "%s: FAULT_STATUS_PROTECT 0x%X\n",
			__func__, val);

	dev_dbg(component->dev, "%s: reset %s\n",
		__func__, component->name);
	/* clear fault state and re-init */
	snd_soc_component_write(component, CSRA66X0_FAULT_STATUS_FA, 0x00);
	snd_soc_component_write(component, CSRA66X0_IRQ_OUTPUT_STATUS_FA, 0x00);
	/* apply reset to CSRA66X0 */
	val = snd_soc_component_read32(component,
			CSRA66X0_MISC_CONTROL_STATUS_1_FA);
	snd_soc_component_write(component, CSRA66X0_MISC_CONTROL_STATUS_1_FA,
			val | 0x08);
	/* wait 500ms after reset to recover CSRA66X0 */
	msleep(500);
	return 0;
}

static int csra66x0_msconfig(struct csra66x0_priv *csra66x0)
{
	struct snd_soc_component *component = csra66x0->component;
	int ret;

	dev_dbg(component->dev, "%s: configure %s\n",
		__func__, component->name);
	/* config */
	snd_soc_component_write(component, CSRA66X0_CHIP_STATE_CTRL_FA,
		SET_CONFIG_STATE);
	/* wait until HW is in config state before proceeding */
	ret = csra66x0_wait_for_config_state(component);
	if (ret) {
		dev_err(component->dev, "%s: timeout while %s is waiting for config state\n",
			__func__, component->name);
		return ret;
	}
	snd_soc_component_write(component, CSRA66X0_PIO7_SELECT, 0x04);
	snd_soc_component_write(component, CSRA66X0_PIO8_SELECT, 0x04);
	if (csra66x0->is_master) {
		/* Master specific config */
		snd_soc_component_write(component,
				CSRA66X0_PIO_PULL_EN0, 0xFF);
		snd_soc_component_write(component,
				CSRA66X0_PIO_PULL_DIR0, 0x80);
		snd_soc_component_write(component,
				CSRA66X0_PIO_PULL_EN1, 0x01);
		snd_soc_component_write(component,
				CSRA66X0_PIO_PULL_DIR1, 0x01);
	} else {
		/* Slave specific config */
		snd_soc_component_write(component,
				CSRA66X0_PIO_PULL_EN0, 0x7F);
		snd_soc_component_write(component,
				CSRA66X0_PIO_PULL_EN1, 0x00);
	}
	snd_soc_component_write(component, CSRA66X0_DCA_CTRL, 0x05);
	return 0;
}

static int csra66x0_soc_probe(struct snd_soc_component *component)
{
	struct csra66x0_priv *csra66x0 =
				snd_soc_component_get_drvdata(component);
	struct snd_soc_dapm_context *dapm;
	char name[50];
	unsigned int i;

	csra66x0->component = component;
	if (csra66x0->in_cluster) {
		dapm = snd_soc_component_get_dapm(component);
		dev_dbg(component->dev, "%s: assign prefix %s to component device %s\n",
			__func__, component->name_prefix,
			component->name);

		/* add device to cluster table */
		csra66x0->max_num_cluster_devices =
			ARRAY_SIZE(csra_clust_dev_tbl);
		for (i = 0; i < csra66x0->max_num_cluster_devices; i++) {
			if (!strncmp(component->name_prefix,
				csra_clust_dev_tbl[i].csra66x0_prefix,
				strnlen(
				csra_clust_dev_tbl[i].csra66x0_prefix,
				sizeof(
				csra_clust_dev_tbl[i].csra66x0_prefix)))) {
				csra_clust_dev_tbl[i].csra66x0_ptr = csra66x0;
				break;
			}
			if (i == csra66x0->max_num_cluster_devices - 1)
				dev_warn(component->dev,
					"%s: Unknown prefix %s of cluster device %s\n",
					__func__, component->name_prefix,
					component->name);
		}

		/* master slave config */
		csra66x0_msconfig(csra66x0);
		if (dapm->component) {
			strlcpy(name, dapm->component->name_prefix,
					sizeof(name));
			strlcat(name, " IN", sizeof(name));
			snd_soc_dapm_ignore_suspend(dapm, name);
			strlcpy(name, dapm->component->name_prefix,
					sizeof(name));
			strlcat(name, " SPKR", sizeof(name));
			snd_soc_dapm_ignore_suspend(dapm, name);
		}
	}

	/* common initialization */
	csra66x0->is_probed = 1;
	csra66x0_init(csra66x0);
	return 0;
}

static void csra66x0_soc_remove(struct snd_soc_component *component)
{
	snd_soc_component_write(component, CSRA66X0_CHIP_STATE_CTRL_FA,
				SET_STDBY_STATE);
	return;
}

static const struct snd_soc_component_driver soc_codec_drv_csra66x0 = {
	.name = DRV_NAME,
	.probe  = csra66x0_soc_probe,
	.remove = csra66x0_soc_remove,
	.controls = csra66x0_snd_controls,
	.num_controls = ARRAY_SIZE(csra66x0_snd_controls),
	.dapm_widgets = csra66x0_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(csra66x0_dapm_widgets),
	.dapm_routes = csra66x0_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(csra66x0_dapm_routes),
};

static struct regmap_config csra66x0_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = csra66x0_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(csra66x0_reg_defaults),
	.max_register = CSRA66X0_MAX_COEFF_ADDR,
	.volatile_reg = csra66x0_volatile_register,
	.writeable_reg = csra66x0_writeable_registers,
	.readable_reg = csra66x0_readable_registers,
};

static irqreturn_t csra66x0_irq(int irq, void *data)
{
	struct csra66x0_priv *csra66x0 = (struct csra66x0_priv *) data;
	struct snd_soc_component  *component = csra66x0->component;
	u16    val;
	unsigned int i;

	/* Treat interrupt before component is initialized as spurious */
	if (component == NULL)
		return IRQ_NONE;

	dev_dbg(component->dev, "%s: csra66x0_interrupt triggered by %s\n",
		__func__, component->name);

	/* fault  indication */
	val = snd_soc_component_read32(component, CSRA66X0_IRQ_OUTPUT_STATUS_FA)
		& 0x1;
	if (!val)
		return IRQ_HANDLED;

	if (csra66x0->in_cluster) {
		/* reset all slave components */
		for (i = 0; i < component->card->num_aux_devs; i++) {
			if (i >= csra66x0->max_num_cluster_devices)
				break;
			if (csra_clust_dev_tbl[i].csra66x0_ptr == NULL)
				continue;
			if (csra_clust_dev_tbl[i].csra66x0_ptr->is_master)
				continue;
			csra66x0_reset(csra_clust_dev_tbl[i].csra66x0_ptr);
		}
		/* reset all master components */
		for (i = 0; i < component->card->num_aux_devs; i++) {
			if (i >= csra66x0->max_num_cluster_devices)
				break;
			if (csra_clust_dev_tbl[i].csra66x0_ptr == NULL)
				continue;
			if (csra_clust_dev_tbl[i].csra66x0_ptr->is_master)
				csra66x0_reset(
					csra_clust_dev_tbl[i].csra66x0_ptr);
		}
		/* recover all components */
		for (i = 0; i < component->card->num_aux_devs; i++) {
			if (i >= csra66x0->max_num_cluster_devices)
				break;
			if (csra_clust_dev_tbl[i].csra66x0_ptr == NULL)
				continue;
			csra66x0_msconfig(csra_clust_dev_tbl[i].csra66x0_ptr);
			csra66x0_init(csra_clust_dev_tbl[i].csra66x0_ptr);
		}
	} else {
		csra66x0_reset(csra66x0);
		csra66x0_init(csra66x0);
	}
	return IRQ_HANDLED;
};

static const struct of_device_id csra66x0_of_match[] = {
	{ .compatible = "qcom,csra66x0", },
	{ }
};
MODULE_DEVICE_TABLE(of, csra66x0_of_match);

static ssize_t csra66x0_sysfs_write2reg_addr_value(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	u32 param[2]; /*reg_addr, reg_value */
	char lbuf[CSRA66X0_SYSFS_ENTRY_MAX_LEN];
	struct csra66x0_priv *csra66x0 = dev_get_drvdata(dev);
	struct snd_soc_component *component = csra66x0->component;

	if (!csra66x0) {
		dev_err(component->dev, "%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (count > sizeof(lbuf) - 1)
		return -EINVAL;

	ret = strlcpy(lbuf, buf, count);
	if (ret != count) {
		dev_err(component->dev, "%s: copy input from user space failed. ret=%d\n",
			__func__, ret);
		ret = -EFAULT;
		goto end;
	}

	lbuf[count] = '\0';
	ret = sysfs_get_param(lbuf, param, 2);
	if (ret) {
		dev_err(component->dev, "%s: get sysfs parameter failed. ret=%d\n",
			__func__, ret);
		goto end;
	}

	if (!(csra66x0_addr_is_in_range(param[0],
		CSRA66X0_BASE, CSRA66X0_MAX_REGISTER_ADDR)
		|| csra66x0_addr_is_in_range(param[0],
		CSRA66X0_COEFF_BASE, CSRA66X0_MAX_COEFF_ADDR))) {
		dev_err(component->dev, "%s: register address 0x%04X out of range\n",
			__func__, param[0]);
		ret = -EINVAL;
		goto end;
	}

	if ((param[1] < 0) || (param[1] > 255)) {
		dev_err(component->dev, "%s: register data 0x%02X out of range\n",
			__func__, param[1]);
		ret = -EINVAL;
		goto end;
	}

	snd_soccomponent_component_write(component, param[0], param[1]);
	ret = count;

end:
	return ret;
}

static ssize_t csra66x0_sysfs_read2reg_addr_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	u32 reg_addr;
	char lbuf[CSRA66X0_SYSFS_ENTRY_MAX_LEN];
	struct csra66x0_priv *csra66x0 = dev_get_drvdata(dev);

	if (!csra66x0) {
		dev_err(dev, "%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (count > sizeof(lbuf) - 1)
		return -EINVAL;

	ret = strlcpy(lbuf, buf, count);
	if (ret != count) {
		dev_err(dev, "%s: copy input from user space failed. ret=%d\n",
			__func__, ret);
		ret = -EFAULT;
		goto end;
	}

	lbuf[count] = '\0';
	ret = sysfs_get_param(lbuf, &reg_addr, 1);
	if (ret) {
		dev_err(dev, "%s: get sysfs parameter failed. ret=%d\n",
			__func__, ret);
		goto end;
	}

	if (!(csra66x0_addr_is_in_range(reg_addr,
		CSRA66X0_BASE, CSRA66X0_MAX_REGISTER_ADDR)
		|| csra66x0_addr_is_in_range(reg_addr,
		CSRA66X0_COEFF_BASE, CSRA66X0_MAX_COEFF_ADDR))) {
		dev_err(dev, "%s: register address 0x%04X out of range\n",
			__func__, reg_addr);
		ret = -EINVAL;
		goto end;
	}

	csra66x0->sysfs_reg_addr = reg_addr;
	ret = count;

end:
	return ret;
}

static ssize_t csra66x0_sysfs_read2reg_addr_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	u32 reg_addr;
	struct csra66x0_priv *csra66x0 = dev_get_drvdata(dev);

	if (!csra66x0) {
		dev_err(dev, "%s: invalid input\n", __func__);
		return -EINVAL;
	}

	reg_addr = csra66x0->sysfs_reg_addr;

	ret = snprintf(buf, CSRA66X0_SYSFS_ENTRY_MAX_LEN,
		"0x%04X\n", reg_addr);
	pr_debug("%s: 0x%04X\n", __func__, reg_addr);

	return ret;
}

static ssize_t csra66x0_sysfs_read2reg_value(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;
	u32 reg_val, reg_addr;
	struct csra66x0_priv *csra66x0 = dev_get_drvdata(dev);
	struct snd_soc_component *component = csra66x0->component;

	if (!csra66x0) {
		dev_err(dev, "%s: invalid input\n", __func__);
		return -EINVAL;
	}

	reg_addr = csra66x0->sysfs_reg_addr;
	if (!(csra66x0_addr_is_in_range(reg_addr,
		CSRA66X0_BASE, CSRA66X0_MAX_REGISTER_ADDR)
		|| csra66x0_addr_is_in_range(reg_addr,
		CSRA66X0_COEFF_BASE, CSRA66X0_MAX_COEFF_ADDR))) {
		pr_debug("%s: 0x%04X: register address out of range\n",
			__func__, reg_addr);
		ret = snprintf(buf, CSRA66X0_SYSFS_ENTRY_MAX_LEN,
			"0x%04X: register address out of range\n", reg_addr);
		goto end;
	}

	reg_val = snd_soc_component_read32(component, csra66x0->sysfs_reg_addr);
	ret = snprintf(buf, CSRA66X0_SYSFS_ENTRY_MAX_LEN,
		"0x%04X:	0x%02X\n", csra66x0->sysfs_reg_addr, reg_val);
	pr_debug("%s: 0x%04X: 0x%02X\n", __func__,
		csra66x0->sysfs_reg_addr, reg_val);

end:
	return ret;
}

static ssize_t csra66x0_sysfs_reset(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val, rc;
	struct csra66x0_priv *csra66x0 = dev_get_drvdata(dev);
	struct snd_soc_component *component = csra66x0->component;
	unsigned int i;

	if (!csra66x0) {
		dev_err(dev, "%s: invalid input\n", __func__);
		return -EINVAL;
	}
	rc = kstrtoint(buf, 10, &val);
	if (rc) {
		dev_err(dev, "%s: kstrtoint failed. rc=%d\n", __func__, rc);
		goto end;
	}
	if (val != SYSFS_RESET) {
		dev_err(dev, "%s: value out of range.\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	pr_debug("%s: reset device\n", __func__);
	if (csra66x0->in_cluster) {
		/* reset all slave components */
		for (i = 0; i < component->card->num_aux_devs; i++) {
			if (i >= csra66x0->max_num_cluster_devices)
				break;
			if (csra_clust_dev_tbl[i].csra66x0_ptr == NULL)
				continue;
			if (csra_clust_dev_tbl[i].csra66x0_ptr->is_master)
				continue;
			csra66x0_reset(csra_clust_dev_tbl[i].csra66x0_ptr);
		}
		/* reset all master components */
		for (i = 0; i < component->card->num_aux_devs; i++) {
			if (i >= csra66x0->max_num_cluster_devices)
				break;
			if (csra_clust_dev_tbl[i].csra66x0_ptr == NULL)
				continue;
			if (csra_clust_dev_tbl[i].csra66x0_ptr->is_master)
				csra66x0_reset(
					csra_clust_dev_tbl[i].csra66x0_ptr);
		}
		/* recover all components */
		for (i = 0; i < component->card->num_aux_devs; i++) {
			if (i >= csra66x0->max_num_cluster_devices)
				break;
			if (csra_clust_dev_tbl[i].csra66x0_ptr == NULL)
				continue;
			csra66x0_msconfig(csra_clust_dev_tbl[i].csra66x0_ptr);
			csra66x0_init(csra_clust_dev_tbl[i].csra66x0_ptr);
		}
	} else {
		csra66x0_reset(csra66x0);
		csra66x0_init(csra66x0);
	}

	rc = strnlen(buf, CSRA66X0_SYSFS_ENTRY_MAX_LEN);
end:
	return rc;
}

static DEVICE_ATTR(write2reg_addr_value, 0200, NULL,
	csra66x0_sysfs_write2reg_addr_value);
static DEVICE_ATTR(read2reg_addr, 0644, csra66x0_sysfs_read2reg_addr_get,
	csra66x0_sysfs_read2reg_addr_set);
static DEVICE_ATTR(read2reg_value, 0444, csra66x0_sysfs_read2reg_value, NULL);
static DEVICE_ATTR(reset, 0200, NULL, csra66x0_sysfs_reset);

static struct attribute *csra66x0_fs_attrs[] = {
	&dev_attr_write2reg_addr_value.attr,
	&dev_attr_read2reg_addr.attr,
	&dev_attr_read2reg_value.attr,
	&dev_attr_reset.attr,
	NULL,
};

static struct attribute_group csra66x0_fs_attrs_group = {
	.attrs = csra66x0_fs_attrs,
};

static int csra66x0_sysfs_create(struct i2c_client *client,
	struct csra66x0_priv *csra66x0)
{
	int rc;

	rc = sysfs_create_group(&client->dev.kobj, &csra66x0_fs_attrs_group);
	return rc;
}

static void csra66x0_sysfs_remove(struct i2c_client *client,
	struct csra66x0_priv *csra66x0)
{
	sysfs_remove_group(&client->dev.kobj, &csra66x0_fs_attrs_group);
}

#if IS_ENABLED(CONFIG_I2C)
static int csra66x0_i2c_probe(struct i2c_client *client_i2c,
			const struct i2c_device_id *id)
{
	struct csra66x0_priv *csra66x0;
	int ret, irq_trigger;
#if IS_ENABLED(CONFIG_DEBUG_FS)
	char debugfs_dir_name[32];
#endif

	csra66x0 = devm_kzalloc(&client_i2c->dev, sizeof(struct csra66x0_priv),
			GFP_KERNEL);
	if (csra66x0 == NULL)
		return -ENOMEM;

	csra66x0->regmap = devm_regmap_init_i2c(client_i2c,
			&csra66x0_regmap_config);
	if (IS_ERR(csra66x0->regmap)) {
		ret = PTR_ERR(csra66x0->regmap);
		dev_err(&client_i2c->dev,
				"%s %d: Failed to allocate register map for I2C device: %d\n",
				__func__, __LINE__, ret);
		return ret;
	}

	i2c_set_clientdata(client_i2c, csra66x0);

	/* get data from device tree */
	if (client_i2c->dev.of_node) {
		/* cluster of multiple devices */
		ret = of_property_read_u32(
			client_i2c->dev.of_node, "qcom,csra-cluster",
			&csra66x0->in_cluster);
		if (ret) {
			dev_info(&client_i2c->dev,
			"%s: qcom,csra-cluster property not defined in DT\n", __func__);
			csra66x0->in_cluster = 0;
		}
		/* master or slave device */
		ret = of_property_read_u32(
			client_i2c->dev.of_node, "qcom,csra-cluster-master",
			&csra66x0->is_master);
		if (ret) {
			dev_info(&client_i2c->dev,
			"%s: qcom,csra-cluster-master property not defined in DT, slave assumed\n",
			__func__);
			csra66x0->is_master = 0;
		}

		/* gpio setup for vreg */
		csra66x0->vreg_gpio = of_get_named_gpio(client_i2c->dev.of_node,
			"qcom,csra-vreg-en-gpio", 0);
		if (!gpio_is_valid(csra66x0->vreg_gpio)) {
			dev_err(&client_i2c->dev, "%s: %s property is not found %d\n",
					__func__, "qcom,csra-vreg-en-gpio",
					csra66x0->vreg_gpio);
			return -ENODEV;
		}
		dev_dbg(&client_i2c->dev, "%s: vreg_en gpio %d\n", __func__,
			csra66x0->vreg_gpio);
		ret = gpio_request(csra66x0->vreg_gpio, dev_name(&client_i2c->dev));
		if (ret) {
			if (ret == -EBUSY) {
				/* GPIO was already requested */
				dev_dbg(&client_i2c->dev,
				"%s: gpio %d is already set\n",
				__func__, csra66x0->vreg_gpio);
			} else {
				dev_err(&client_i2c->dev, "%s: Failed to request gpio %d, err: %d\n",
					__func__, csra66x0->vreg_gpio, ret);
			}
		} else {
			gpio_direction_output(csra66x0->vreg_gpio, 1);
			gpio_set_value(csra66x0->vreg_gpio, 0);
		}

		/* register interrupt handle */
		if (client_i2c->irq) {
			csra66x0->irq = client_i2c->irq;
			/* interrupt polarity */
			ret = of_property_read_u32(
				client_i2c->dev.of_node, "irq-active-low",
				&csra66x0->irq_active_low);
			if (ret) {
				dev_info(&client_i2c->dev,
				"%s: irq-active-low property not defined in DT\n", __func__);
				csra66x0->irq_active_low = 0;
			}
			if (csra66x0->irq_active_low)
				irq_trigger = IRQF_TRIGGER_LOW;
			else
				irq_trigger = IRQF_TRIGGER_HIGH;

			ret = devm_request_threaded_irq(&client_i2c->dev,
					csra66x0->irq, NULL, csra66x0_irq,
					irq_trigger | IRQF_ONESHOT,
					"csra66x0_irq", csra66x0);
			if (ret) {
				dev_err(&client_i2c->dev,
				"%s: Failed to request IRQ %d: %d\n",
				__func__, csra66x0->irq, ret);
				csra66x0->irq = 0;
			}
		}
	}

#if IS_ENABLED(CONFIG_DEBUG_FS)
	/* debugfs interface */
	snprintf(debugfs_dir_name, sizeof(debugfs_dir_name), "%s-%s",
		client_i2c->name, dev_name(&client_i2c->dev));
	csra66x0->debugfs_dir = debugfs_create_dir(debugfs_dir_name, NULL);
	if (!csra66x0->debugfs_dir) {
		dev_dbg(&client_i2c->dev,
			"%s: Failed to create /sys/kernel/debug/%s for debugfs\n",
			__func__, debugfs_dir_name);
		ret = -ENOMEM;
		goto err_debugfs;
	}
	csra66x0->debugfs_file_wo = debugfs_create_file(
		"write_reg_val", S_IFREG | S_IRUGO, csra66x0->debugfs_dir,
		(void *) csra66x0,
		&debugfs_codec_ops);
	if (!csra66x0->debugfs_file_wo) {
		dev_dbg(&client_i2c->dev,
			"%s: Failed to create /sys/kernel/debug/%s/write_reg_val\n",
			__func__, debugfs_dir_name);
		ret = -ENOMEM;
		goto err_debugfs;
	}
	csra66x0->debugfs_file_ro = debugfs_create_file(
		"show_reg_dump", S_IFREG | S_IRUGO, csra66x0->debugfs_dir,
		(void *) csra66x0,
		&debugfs_codec_ops);
	if (!csra66x0->debugfs_file_ro) {
		dev_dbg(&client_i2c->dev,
			"%s: Failed to create /sys/kernel/debug/%s/show_reg_dump\n",
			__func__, debugfs_dir_name);
		ret = -ENOMEM;
		goto err_debugfs;
	}
#endif /* CONFIG_DEBUG_FS */

	/* register component */
	ret = snd_soc_register_component(&client_i2c->dev,
			&soc_codec_drv_csra66x0, NULL, 0);
	if (ret != 0) {
		dev_err(&client_i2c->dev, "%s %d: Failed to register component: %d\n",
			__func__, __LINE__, ret);
		if (gpio_is_valid(csra66x0->vreg_gpio)) {
			gpio_set_value(csra66x0->vreg_gpio, 0);
			gpio_free(csra66x0->vreg_gpio);
		}
		return ret;
	}

	ret = csra66x0_sysfs_create(client_i2c, csra66x0);
	if (ret) {
		dev_err(&client_i2c->dev, "%s: sysfs creation failed ret=%d\n",
			__func__, ret);
		goto err_sysfs;
	}

	return 0;

err_sysfs:
	snd_soc_unregister_component(&client_i2c->dev);
	return ret;

#if IS_ENABLED(CONFIG_DEBUG_FS)
err_debugfs:
	debugfs_remove_recursive(csra66x0->debugfs_dir);
	return ret;
#endif
}

static int csra66x0_i2c_remove(struct i2c_client *client_i2c)
{
	struct csra66x0_priv *csra66x0 = i2c_get_clientdata(client_i2c);

	if (csra66x0) {
		if (gpio_is_valid(csra66x0->vreg_gpio)) {
			gpio_set_value(csra66x0->vreg_gpio, 0);
			gpio_free(csra66x0->vreg_gpio);
		}
#if IS_ENABLED(CONFIG_DEBUG_FS)
		debugfs_remove_recursive(csra66x0->debugfs_dir);
#endif
	}

	csra66x0_sysfs_remove(client_i2c, csra66x0);
	snd_soc_unregister_component(&i2c_client->dev);

	return 0;
}

static const struct i2c_device_id csra66x0_i2c_id[] = {
	{ "csra66x0", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, csra66x0_i2c_id);

static struct i2c_driver csra66x0_i2c_driver = {
	.probe =    csra66x0_i2c_probe,
	.remove =   csra66x0_i2c_remove,
	.id_table = csra66x0_i2c_id,
	.driver = {
		.name = "csra66x0",
		.owner = THIS_MODULE,
		.of_match_table = csra66x0_of_match
	},
};
#endif

static int __init csra66x0_codec_init(void)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_I2C)
	ret = i2c_add_driver(&csra66x0_i2c_driver);
	if (ret != 0)
		pr_err("%s: Failed to register CSRA66X0 I2C driver, ret = %d\n",
			__func__, ret);
#endif
	return ret;
}
module_init(csra66x0_codec_init);

static void __exit csra66x0_codec_exit(void)
{
#if IS_ENABLED(CONFIG_I2C)
	i2c_del_driver(&csra66x0_i2c_driver);
#endif
}
module_exit(csra66x0_codec_exit);

MODULE_DESCRIPTION("CSRA66X0 Codec driver");
MODULE_LICENSE("GPL v2");
