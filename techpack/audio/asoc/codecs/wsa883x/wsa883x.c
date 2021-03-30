// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/bitops.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <soc/soundwire.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <asoc/msm-cdc-pinctrl.h>
#include <asoc/msm-cdc-supply.h>
#include "wsa883x.h"
#include "internal.h"
#include "asoc/bolero-slave-internal.h"

#define T1_TEMP -10
#define T2_TEMP 150
#define LOW_TEMP_THRESHOLD 5
#define HIGH_TEMP_THRESHOLD 45
#define TEMP_INVALID	0xFFFF
#define WSA883X_TEMP_RETRY 3

#define MAX_NAME_LEN	40
#define WSA883X_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 |\
			SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_48000 |\
			SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000 |\
			SNDRV_PCM_RATE_384000)
/* Fractional Rates */
#define WSA883X_FRAC_RATES (SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_88200 |\
				SNDRV_PCM_RATE_176400 | SNDRV_PCM_RATE_352800)

#define WSA883X_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
		SNDRV_PCM_FMTBIT_S24_LE |\
		SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

enum {
	WSA_4OHMS =4,
	WSA_8OHMS = 8,
	WSA_16OHMS = 16,
	WSA_32OHMS = 32,
};

struct wsa_temp_register {
	u8 d1_msb;
	u8 d1_lsb;
	u8 d2_msb;
	u8 d2_lsb;
	u8 dmeas_msb;
	u8 dmeas_lsb;
};

struct wsa_reg_mask_val {
	u16 reg;
	u8 mask;
	u8 val;
};

static const struct wsa_reg_mask_val reg_init[] = {
	{WSA883X_PA_FSM_BYP, 0x01, 0x00},
	{WSA883X_ISENSE2, 0xE0, 0x40},
	{WSA883X_ADC_6, 0x02, 0x02},
	{WSA883X_CDC_SPK_DSM_A2_0, 0xFF, 0x0A},
	{WSA883X_CDC_SPK_DSM_A2_1, 0x0F, 0x08},
	{WSA883X_CDC_SPK_DSM_A3_0, 0xFF, 0xF3},
	{WSA883X_CDC_SPK_DSM_A3_1, 0x07, 0x07},
	{WSA883X_CDC_SPK_DSM_A4_0, 0xFF, 0x79},
	{WSA883X_CDC_SPK_DSM_A4_1, 0x03, 0x02},
	{WSA883X_CDC_SPK_DSM_A5_0, 0xFF, 0x0B},
	{WSA883X_CDC_SPK_DSM_A5_1, 0x03, 0x02},
	{WSA883X_CDC_SPK_DSM_A6_0, 0xFF, 0x8A},
	{WSA883X_CDC_SPK_DSM_A7_0, 0xFF, 0x9B},
	{WSA883X_CDC_SPK_DSM_C_0, 0xFF, 0x68},
	{WSA883X_CDC_SPK_DSM_C_1, 0xFF, 0x54},
	{WSA883X_CDC_SPK_DSM_C_2, 0xFF, 0xF2},
	{WSA883X_CDC_SPK_DSM_C_3, 0x3F, 0x20},
	{WSA883X_CDC_SPK_DSM_R1, 0xFF, 0x83},
	{WSA883X_CDC_SPK_DSM_R2, 0xFF, 0x7F},
	{WSA883X_CDC_SPK_DSM_R3, 0xFF, 0x9D},
	{WSA883X_CDC_SPK_DSM_R4, 0xFF, 0x82},
	{WSA883X_CDC_SPK_DSM_R5, 0xFF, 0x8B},
	{WSA883X_CDC_SPK_DSM_R6, 0xFF, 0x9B},
	{WSA883X_CDC_SPK_DSM_R7, 0xFF, 0x3F},
	{WSA883X_VBAT_SNS, 0x60, 0x20},
	{WSA883X_DRE_CTL_0, 0xF0, 0x90},
	{WSA883X_DRE_IDLE_DET_CTL, 0x10, 0x00},
	{WSA883X_CURRENT_LIMIT, 0x78, 0x40},
	{WSA883X_DRE_CTL_0, 0x07, 0x02},
	{WSA883X_VAGC_TIME, 0x0F, 0x0F},
	{WSA883X_VAGC_ATTN_LVL_1_2, 0xFF, 0x00},
	{WSA883X_VAGC_ATTN_LVL_3, 0xFF, 0x00},
	{WSA883X_VAGC_CTL, 0x01, 0x01},
	{WSA883X_TAGC_CTL, 0x0E, 0x0A},
	{WSA883X_TAGC_TIME, 0x0C, 0x0C},
	{WSA883X_TAGC_E2E_GAIN, 0x1F, 0x02},
	{WSA883X_TEMP_CONFIG0, 0x07, 0x02},
	{WSA883X_TEMP_CONFIG1, 0x07, 0x02},
	{WSA883X_OTP_REG_1, 0xFF, 0x49},
	{WSA883X_OTP_REG_2, 0xC0, 0x80},
	{WSA883X_OTP_REG_3, 0xFF, 0xC9},
	{WSA883X_OTP_REG_4, 0xC0, 0x40},
	{WSA883X_TAGC_CTL, 0x01, 0x01},
	{WSA883X_ADC_2, 0x40, 0x00},
	{WSA883X_ADC_7, 0x04, 0x04},
	{WSA883X_ADC_7, 0x02, 0x02},
	{WSA883X_CKWD_CTL_0, 0x60, 0x00},
	{WSA883X_DRE_CTL_1, 0x3E, 0x20},
	{WSA883X_CKWD_CTL_1, 0x1F, 0x1B},
	{WSA883X_GMAMP_SUP1, 0x60, 0x60},
};

static int wsa883x_handle_post_irq(void *data);
static int wsa883x_get_temperature(struct snd_soc_component *component,
				   int *temp);
enum {
	WSA8830 = 0,
	WSA8835,
};

enum {
	SPKR_STATUS = 0,
	WSA_SUPPLIES_LPM_MODE,
	SPKR_ADIE_LB,
};

enum {
	WSA883X_IRQ_INT_SAF2WAR = 0,
	WSA883X_IRQ_INT_WAR2SAF,
	WSA883X_IRQ_INT_DISABLE,
	WSA883X_IRQ_INT_OCP,
	WSA883X_IRQ_INT_CLIP,
	WSA883X_IRQ_INT_PDM_WD,
	WSA883X_IRQ_INT_CLK_WD,
	WSA883X_IRQ_INT_INTR_PIN,
	WSA883X_IRQ_INT_UVLO,
	WSA883X_IRQ_INT_PA_ON_ERR,
	WSA883X_NUM_IRQS,
};

static const struct regmap_irq wsa883x_irqs[WSA883X_NUM_IRQS] = {
	REGMAP_IRQ_REG(WSA883X_IRQ_INT_SAF2WAR, 0, 0x01),
	REGMAP_IRQ_REG(WSA883X_IRQ_INT_WAR2SAF, 0, 0x02),
	REGMAP_IRQ_REG(WSA883X_IRQ_INT_DISABLE, 0, 0x04),
	REGMAP_IRQ_REG(WSA883X_IRQ_INT_OCP, 0, 0x08),
	REGMAP_IRQ_REG(WSA883X_IRQ_INT_CLIP, 0, 0x10),
	REGMAP_IRQ_REG(WSA883X_IRQ_INT_PDM_WD, 0, 0x20),
	REGMAP_IRQ_REG(WSA883X_IRQ_INT_CLK_WD, 0, 0x40),
	REGMAP_IRQ_REG(WSA883X_IRQ_INT_INTR_PIN, 0, 0x80),
	REGMAP_IRQ_REG(WSA883X_IRQ_INT_UVLO, 1, 0x01),
	REGMAP_IRQ_REG(WSA883X_IRQ_INT_PA_ON_ERR, 1, 0x02),
};

static struct regmap_irq_chip wsa883x_regmap_irq_chip = {
	.name = "wsa883x",
	.irqs = wsa883x_irqs,
	.num_irqs = ARRAY_SIZE(wsa883x_irqs),
	.num_regs = 2,
	.status_base = WSA883X_INTR_STATUS0,
	.mask_base = WSA883X_INTR_MASK0,
	.type_base = WSA883X_INTR_LEVEL0,
	.ack_base = WSA883X_INTR_CLEAR0,
	.use_ack = 1,
	.runtime_pm = false,
	.handle_post_irq = wsa883x_handle_post_irq,
	.irq_drv_data = NULL,
};

static int wsa883x_handle_post_irq(void *data)
{
	struct wsa883x_priv *wsa883x = data;
	u32 sts1 = 0, sts2 = 0;

	regmap_read(wsa883x->regmap, WSA883X_INTR_STATUS0, &sts1);
	regmap_read(wsa883x->regmap, WSA883X_INTR_STATUS1, &sts2);

	wsa883x->swr_slave->slave_irq_pending =
			((sts1 || sts2) ? true : false);

	return IRQ_HANDLED;
}

#ifdef CONFIG_DEBUG_FS
static int codec_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static int get_parameters(char *buf, u32 *param1, int num_of_par)
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

			if (kstrtou32(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else {
			return -EINVAL;
		}
	}
	return 0;
}

static bool is_swr_slave_reg_readable(int reg)
{
	int ret = true;

	if (((reg > 0x46) && (reg < 0x4A)) ||
	    ((reg > 0x4A) && (reg < 0x50)) ||
	    ((reg > 0x55) && (reg < 0xD0)) ||
	    ((reg > 0xD0) && (reg < 0xE0)) ||
	    ((reg > 0xE0) && (reg < 0xF0)) ||
	    ((reg > 0xF0) && (reg < 0x100)) ||
	    ((reg > 0x105) && (reg < 0x120)) ||
	    ((reg > 0x205) && (reg < 0x220)) ||
	    ((reg > 0x305) && (reg < 0x320)) ||
	    ((reg > 0x405) && (reg < 0x420)) ||
	    ((reg > 0x128) && (reg < 0x130)) ||
	    ((reg > 0x228) && (reg < 0x230)) ||
	    ((reg > 0x328) && (reg < 0x330)) ||
	    ((reg > 0x428) && (reg < 0x430)) ||
	    ((reg > 0x138) && (reg < 0x205)) ||
	    ((reg > 0x238) && (reg < 0x305)) ||
	    ((reg > 0x338) && (reg < 0x405)) ||
	    ((reg > 0x405) && (reg < 0xF00)) ||
	    ((reg > 0xF05) && (reg < 0xF20)) ||
	    ((reg > 0xF25) && (reg < 0xF30)) ||
	    ((reg > 0xF35) && (reg < 0x2000)))
		ret = false;

	return ret;
}

static ssize_t swr_slave_reg_show(struct swr_device *pdev, char __user *ubuf,
					size_t count, loff_t *ppos)
{
	int i, reg_val, len;
	ssize_t total = 0;
	char tmp_buf[SWR_SLV_MAX_BUF_LEN];

	if (!ubuf || !ppos)
		return 0;

	for (i = (((int) *ppos/BYTES_PER_LINE) + SWR_SLV_START_REG_ADDR);
		i <= SWR_SLV_MAX_REG_ADDR; i++) {
		if (!is_swr_slave_reg_readable(i))
			continue;
		swr_read(pdev, pdev->dev_num, i, &reg_val, 1);
		len = snprintf(tmp_buf, sizeof(tmp_buf), "0x%.3x: 0x%.2x\n", i,
			       (reg_val & 0xFF));
		if (len < 0) {
			pr_err("%s: fail to fill the buffer\n", __func__);
			total = -EFAULT;
			goto copy_err;
		}
		if ((total + len) >= count - 1)
			break;
		if (copy_to_user((ubuf + total), tmp_buf, len)) {
			pr_err("%s: fail to copy reg dump\n", __func__);
			total = -EFAULT;
			goto copy_err;
		}
		total += len;
		*ppos += len;
	}

copy_err:
	*ppos = SWR_SLV_MAX_REG_ADDR * BYTES_PER_LINE;
	return total;
}

static ssize_t codec_debug_dump(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	struct swr_device *pdev;

	if (!count || !file || !ppos || !ubuf)
		return -EINVAL;

	pdev = file->private_data;
	if (!pdev)
		return -EINVAL;

	if (*ppos < 0)
		return -EINVAL;

	return swr_slave_reg_show(pdev, ubuf, count, ppos);
}

static ssize_t codec_debug_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char lbuf[SWR_SLV_RD_BUF_LEN];
	struct swr_device *pdev = NULL;
	struct wsa883x_priv *wsa883x = NULL;

	if (!count || !file || !ppos || !ubuf)
		return -EINVAL;

	pdev = file->private_data;
	if (!pdev)
		return -EINVAL;

	wsa883x = swr_get_dev_data(pdev);
	if (!wsa883x)
		return -EINVAL;

	if (*ppos < 0)
		return -EINVAL;

	snprintf(lbuf, sizeof(lbuf), "0x%x\n",
			(wsa883x->read_data & 0xFF));

	return simple_read_from_buffer(ubuf, count, ppos, lbuf,
					       strnlen(lbuf, 7));
}

static ssize_t codec_debug_peek_write(struct file *file,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char lbuf[SWR_SLV_WR_BUF_LEN];
	int rc = 0;
	u32 param[5];
	struct swr_device *pdev = NULL;
	struct wsa883x_priv *wsa883x = NULL;

	if (!cnt || !file || !ppos || !ubuf)
		return -EINVAL;

	pdev = file->private_data;
	if (!pdev)
		return -EINVAL;

	wsa883x = swr_get_dev_data(pdev);
	if (!wsa883x)
		return -EINVAL;

	if (*ppos < 0)
		return -EINVAL;

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';
	rc = get_parameters(lbuf, param, 1);
	if (!((param[0] <= SWR_SLV_MAX_REG_ADDR) && (rc == 0)))
		return -EINVAL;
	swr_read(pdev, pdev->dev_num, param[0], &wsa883x->read_data, 1);
	if (rc == 0)
		rc = cnt;
	else
		pr_err("%s: rc = %d\n", __func__, rc);

	return rc;
}

static ssize_t codec_debug_write(struct file *file,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char lbuf[SWR_SLV_WR_BUF_LEN];
	int rc = 0;
	u32 param[5];
	struct swr_device *pdev;

	if (!file || !ppos || !ubuf)
		return -EINVAL;

	pdev = file->private_data;
	if (!pdev)
		return -EINVAL;

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';
	rc = get_parameters(lbuf, param, 2);
	if (!((param[0] <= SWR_SLV_MAX_REG_ADDR) &&
		(param[1] <= 0xFF) && (rc == 0)))
		return -EINVAL;
	swr_write(pdev, pdev->dev_num, param[0], &param[1]);
	if (rc == 0)
		rc = cnt;
	else
		pr_err("%s: rc = %d\n", __func__, rc);

	return rc;
}

static const struct file_operations codec_debug_write_ops = {
	.open = codec_debug_open,
	.write = codec_debug_write,
};

static const struct file_operations codec_debug_read_ops = {
	.open = codec_debug_open,
	.read = codec_debug_read,
	.write = codec_debug_peek_write,
};

static const struct file_operations codec_debug_dump_ops = {
	.open = codec_debug_open,
	.read = codec_debug_dump,
};
#endif

static void wsa883x_regcache_sync(struct wsa883x_priv *wsa883x)
{
	mutex_lock(&wsa883x->res_lock);
	regcache_mark_dirty(wsa883x->regmap);
	regcache_sync(wsa883x->regmap);
	mutex_unlock(&wsa883x->res_lock);
}

static irqreturn_t wsa883x_saf2war_handle_irq(int irq, void *data)
{
	pr_err_ratelimited("%s: interrupt for irq =%d triggered\n",
			   __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t wsa883x_war2saf_handle_irq(int irq, void *data)
{
	pr_err_ratelimited("%s: interrupt for irq =%d triggered\n",
			   __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t wsa883x_otp_handle_irq(int irq, void *data)
{
	pr_err_ratelimited("%s: interrupt for irq =%d triggered\n",
			   __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t wsa883x_ocp_handle_irq(int irq, void *data)
{
	pr_err_ratelimited("%s: interrupt for irq =%d triggered\n",
			   __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t wsa883x_clip_handle_irq(int irq, void *data)
{
	pr_err_ratelimited("%s: interrupt for irq =%d triggered\n",
			   __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t wsa883x_pdm_wd_handle_irq(int irq, void *data)
{
	pr_err_ratelimited("%s: interrupt for irq =%d triggered\n",
			   __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t wsa883x_clk_wd_handle_irq(int irq, void *data)
{
	pr_err_ratelimited("%s: interrupt for irq =%d triggered\n",
			   __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t wsa883x_ext_int_handle_irq(int irq, void *data)
{
	pr_err_ratelimited("%s: interrupt for irq =%d triggered\n",
			   __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t wsa883x_uvlo_handle_irq(int irq, void *data)
{
	pr_err_ratelimited("%s: interrupt for irq =%d triggered\n",
			   __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t wsa883x_pa_on_err_handle_irq(int irq, void *data)
{
	u8 pa_fsm_sta = 0, pa_fsm_err = 0;
	struct wsa883x_priv *wsa883x = data;
	struct snd_soc_component *component = NULL;

	if (!wsa883x)
		return IRQ_NONE;

	component = wsa883x->component;
	if (!component)
		return IRQ_NONE;

	pa_fsm_sta = (snd_soc_component_read32(component, WSA883X_PA_FSM_STA)
			& 0x70);

	if (pa_fsm_sta)
		pa_fsm_err = snd_soc_component_read32(component,
						WSA883X_PA_FSM_ERR_COND);
	pr_err_ratelimited("%s: irq: %d, pa_fsm_sta: %d, pa_fsm_err: %d\n",
		__func__, irq, pa_fsm_sta, pa_fsm_err);

	snd_soc_component_update_bits(component, WSA883X_PA_FSM_CTL,
					0x10, 0x00);
	snd_soc_component_update_bits(component, WSA883X_PA_FSM_CTL,
					0x10, 0x10);
	snd_soc_component_update_bits(component, WSA883X_PA_FSM_CTL,
					0x10, 0x00);

	return IRQ_HANDLED;
}

static const char * const wsa_dev_mode_text[] = {
	"speaker", "receiver", "ultrasound"
};

static const struct soc_enum wsa_dev_mode_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(wsa_dev_mode_text), wsa_dev_mode_text);

static int wsa_dev_mode_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = wsa883x->dev_mode;

	dev_dbg(component->dev, "%s: mode = 0x%x\n", __func__,
			wsa883x->dev_mode);

	return 0;
}

static int wsa_dev_mode_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	wsa883x->dev_mode =  ucontrol->value.integer.value[0];

	return 0;
}

static const char * const wsa_pa_gain_text[] = {
	"G_18_DB", "G_16P5_DB", "G_15_DB", "G_13P5_DB", "G_12_DB", "G_10P5_DB",
	"G_9_DB", "G_7P5_DB", "G_6_DB", "G_4P5_DB", "G_3_DB", "G_1P5_DB",
	"G_0_DB"
};

static const struct soc_enum wsa_pa_gain_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(wsa_pa_gain_text), wsa_pa_gain_text);

static int wsa_pa_gain_get(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = wsa883x->pa_gain;

	dev_dbg(component->dev, "%s: PA gain = 0x%x\n", __func__,
			wsa883x->pa_gain);

	return 0;
}

static int wsa_pa_gain_put(struct snd_kcontrol *kcontrol,
			   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s: ucontrol->value.integer.value[0]  = %ld\n",
		__func__, ucontrol->value.integer.value[0]);

	wsa883x->pa_gain =  ucontrol->value.integer.value[0];

	return 0;
}

static int wsa883x_get_mute(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = wsa883x->pa_mute;

	return 0;
}

static int wsa883x_set_mute(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);
	int value = ucontrol->value.integer.value[0];

	dev_dbg(component->dev, "%s: mute current %d, new %d\n",
		__func__, wsa883x->pa_mute, value);

	wsa883x->pa_mute = value;

	return 0;
}

static int wsa_get_temp(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
			snd_soc_kcontrol_component(kcontrol);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);
	int temp = 0;

	if (test_bit(SPKR_STATUS, &wsa883x->status_mask))
		temp = wsa883x->curr_temp;
	else
		wsa883x_get_temperature(component, &temp);

	ucontrol->value.integer.value[0] = temp;

	return 0;
}

static ssize_t wsa883x_codec_version_read(struct snd_info_entry *entry,
			       void *file_private_data, struct file *file,
			       char __user *buf, size_t count, loff_t pos)
{
	struct wsa883x_priv *wsa883x;
	char buffer[WSA883X_VERSION_ENTRY_SIZE];
	int len = 0;

	wsa883x = (struct wsa883x_priv *) entry->private_data;
	if (!wsa883x) {
		pr_err("%s: wsa883x priv is null\n", __func__);
		return -EINVAL;
	}

	switch (wsa883x->version) {
	case WSA883X_VERSION_1_0:
		len = snprintf(buffer, sizeof(buffer), "WSA883X_1_0\n");
		break;
	case WSA883X_VERSION_1_1:
		len = snprintf(buffer, sizeof(buffer), "WSA883X_1_1\n");
		break;
	default:
		len = snprintf(buffer, sizeof(buffer), "VER_UNDEFINED\n");
		break;
	}

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static struct snd_info_entry_ops wsa883x_codec_info_ops = {
	.read = wsa883x_codec_version_read,
};

static ssize_t wsa883x_variant_read(struct snd_info_entry *entry,
				    void *file_private_data,
				    struct file *file,
				    char __user *buf, size_t count,
				    loff_t pos)
{
	struct wsa883x_priv *wsa883x;
	char buffer[WSA883X_VARIANT_ENTRY_SIZE];
	int len = 0;

	wsa883x = (struct wsa883x_priv *) entry->private_data;
	if (!wsa883x) {
		pr_err("%s: wsa883x priv is null\n", __func__);
		return -EINVAL;
	}

	switch (wsa883x->variant) {
	case WSA8830:
		len = snprintf(buffer, sizeof(buffer), "WSA8830\n");
		break;
	case WSA8835:
		len = snprintf(buffer, sizeof(buffer), "WSA8835\n");
		break;
	default:
		len = snprintf(buffer, sizeof(buffer), "UNDEFINED\n");
		break;
	}

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

static struct snd_info_entry_ops wsa883x_variant_ops = {
	.read = wsa883x_variant_read,
};

/*
 * wsa883x_codec_info_create_codec_entry - creates wsa883x module
 * @codec_root: The parent directory
 * @component: Codec instance
 *
 * Creates wsa883x module and version entry under the given
 * parent directory.
 *
 * Return: 0 on success or negative error code on failure.
 */
int wsa883x_codec_info_create_codec_entry(struct snd_info_entry *codec_root,
					  struct snd_soc_component *component)
{
	struct snd_info_entry *version_entry;
	struct snd_info_entry *variant_entry;
	struct wsa883x_priv *wsa883x;
	struct snd_soc_card *card;
	char name[80];

	if (!codec_root || !component)
		return -EINVAL;

	wsa883x = snd_soc_component_get_drvdata(component);
	if (wsa883x->entry) {
		dev_dbg(wsa883x->dev,
			"%s:wsa883x module already created\n", __func__);
		return 0;
	}
	card = component->card;

	snprintf(name, sizeof(name), "%s.%llx", "wsa883x",
		 wsa883x->swr_slave->addr);

	wsa883x->entry = snd_info_create_module_entry(codec_root->module,
						(const char *)name,
						codec_root);
	if (!wsa883x->entry) {
		dev_dbg(component->dev, "%s: failed to create wsa883x entry\n",
			__func__);
		return -ENOMEM;
	}
	wsa883x->entry->mode = S_IFDIR | 0555;
	if (snd_info_register(wsa883x->entry) < 0) {
		snd_info_free_entry(wsa883x->entry);
		return -ENOMEM;
	}

	version_entry = snd_info_create_card_entry(card->snd_card,
						   "version",
						   wsa883x->entry);
	if (!version_entry) {
		dev_dbg(component->dev, "%s: failed to create wsa883x version entry\n",
			__func__);
		snd_info_free_entry(wsa883x->entry);
		return -ENOMEM;
	}

	version_entry->private_data = wsa883x;
	version_entry->size = WSA883X_VERSION_ENTRY_SIZE;
	version_entry->content = SNDRV_INFO_CONTENT_DATA;
	version_entry->c.ops = &wsa883x_codec_info_ops;

	if (snd_info_register(version_entry) < 0) {
		snd_info_free_entry(version_entry);
		snd_info_free_entry(wsa883x->entry);
		return -ENOMEM;
	}
	wsa883x->version_entry = version_entry;

	variant_entry = snd_info_create_card_entry(card->snd_card,
						   "variant",
						   wsa883x->entry);
	if (!variant_entry) {
		dev_dbg(component->dev,
			"%s: failed to create wsa883x variant entry\n",
			__func__);
		snd_info_free_entry(version_entry);
		snd_info_free_entry(wsa883x->entry);
		return -ENOMEM;
	}

	variant_entry->private_data = wsa883x;
	variant_entry->size = WSA883X_VARIANT_ENTRY_SIZE;
	variant_entry->content = SNDRV_INFO_CONTENT_DATA;
	variant_entry->c.ops = &wsa883x_variant_ops;

	if (snd_info_register(variant_entry) < 0) {
		snd_info_free_entry(variant_entry);
		snd_info_free_entry(version_entry);
		snd_info_free_entry(wsa883x->entry);
		return -ENOMEM;
	}
	wsa883x->variant_entry = variant_entry;

	return 0;
}
EXPORT_SYMBOL(wsa883x_codec_info_create_codec_entry);

/*
 * wsa883x_codec_get_dev_num - returns swr device number
 * @component: Codec instance
 *
 * Return: swr device number on success or negative error
 * code on failure.
 */
int wsa883x_codec_get_dev_num(struct snd_soc_component *component)
{
	struct wsa883x_priv *wsa883x;

	if (!component)
		return -EINVAL;

	wsa883x = snd_soc_component_get_drvdata(component);
	if (!wsa883x) {
		pr_err("%s: wsa883x component is NULL\n", __func__);
		return -EINVAL;
	}

	return wsa883x->swr_slave->dev_num;
}
EXPORT_SYMBOL(wsa883x_codec_get_dev_num);

static int wsa883x_get_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = wsa883x->comp_enable;
	return 0;
}

static int wsa883x_set_compander(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);
	int value = ucontrol->value.integer.value[0];

	dev_dbg(component->dev, "%s: Compander enable current %d, new %d\n",
		 __func__, wsa883x->comp_enable, value);
	wsa883x->comp_enable = value;
	return 0;
}

static int wsa883x_get_visense(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = wsa883x->visense_enable;
	return 0;
}

static int wsa883x_set_visense(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);
	int value = ucontrol->value.integer.value[0];

	dev_dbg(component->dev, "%s: VIsense enable current %d, new %d\n",
		 __func__, wsa883x->visense_enable, value);
	wsa883x->visense_enable = value;
	return 0;
}

static int wsa883x_get_ext_vdd_spk(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = wsa883x->ext_vdd_spk;

	return 0;
}

static int wsa883x_put_ext_vdd_spk(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component =
				snd_soc_kcontrol_component(kcontrol);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);
	int value = ucontrol->value.integer.value[0];

	dev_dbg(component->dev, "%s: Ext VDD SPK enable current %d, new %d\n",
		 __func__, wsa883x->ext_vdd_spk, value);
	wsa883x->ext_vdd_spk = value;

	return 0;
}

static const struct snd_kcontrol_new wsa883x_snd_controls[] = {
	SOC_ENUM_EXT("WSA PA Gain", wsa_pa_gain_enum,
		     wsa_pa_gain_get, wsa_pa_gain_put),

	SOC_SINGLE_EXT("WSA PA Mute", SND_SOC_NOPM, 0, 1, 0,
		wsa883x_get_mute, wsa883x_set_mute),

	SOC_SINGLE_EXT("WSA Temp", SND_SOC_NOPM, 0, UINT_MAX, 0,
			wsa_get_temp, NULL),

	SOC_ENUM_EXT("WSA MODE", wsa_dev_mode_enum,
		     wsa_dev_mode_get, wsa_dev_mode_put),

	SOC_SINGLE_EXT("COMP Switch", SND_SOC_NOPM, 0, 1, 0,
		wsa883x_get_compander, wsa883x_set_compander),

	SOC_SINGLE_EXT("VISENSE Switch", SND_SOC_NOPM, 0, 1, 0,
		wsa883x_get_visense, wsa883x_set_visense),

	SOC_SINGLE_EXT("External VDD_SPK", SND_SOC_NOPM, 0, 1, 0,
		wsa883x_get_ext_vdd_spk, wsa883x_put_ext_vdd_spk),
};

static const struct snd_kcontrol_new swr_dac_port[] = {
	SOC_DAPM_SINGLE("Switch", SND_SOC_NOPM, 0, 1, 0)
};

static int wsa883x_set_port(struct snd_soc_component *component, int port_idx,
			u8 *port_id, u8 *num_ch, u8 *ch_mask, u32 *ch_rate,
			u8 *port_type)
{
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	*port_id = wsa883x->port[port_idx].port_id;
	*num_ch = wsa883x->port[port_idx].num_ch;
	*ch_mask = wsa883x->port[port_idx].ch_mask;
	*ch_rate = wsa883x->port[port_idx].ch_rate;
	*port_type = wsa883x->port[port_idx].port_type;
	return 0;
}

static int wsa883x_enable_swr_dac_port(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);
	u8 port_id[WSA883X_MAX_SWR_PORTS];
	u8 num_ch[WSA883X_MAX_SWR_PORTS];
	u8 ch_mask[WSA883X_MAX_SWR_PORTS];
	u32 ch_rate[WSA883X_MAX_SWR_PORTS];
	u8 port_type[WSA883X_MAX_SWR_PORTS];
	u8 num_port = 0;

	dev_dbg(component->dev, "%s: event %d name %s\n", __func__,
		event, w->name);
	if (wsa883x == NULL)
		return -EINVAL;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		wsa883x_set_port(component, SWR_DAC_PORT,
				&port_id[num_port], &num_ch[num_port],
				&ch_mask[num_port], &ch_rate[num_port],
				&port_type[num_port]);
		++num_port;

		if (wsa883x->comp_enable) {
			wsa883x_set_port(component, SWR_COMP_PORT,
					&port_id[num_port], &num_ch[num_port],
					&ch_mask[num_port], &ch_rate[num_port],
					&port_type[num_port]);
			++num_port;
		}
		if (wsa883x->visense_enable) {
			wsa883x_set_port(component, SWR_VISENSE_PORT,
					&port_id[num_port], &num_ch[num_port],
					&ch_mask[num_port], &ch_rate[num_port],
					&port_type[num_port]);
			++num_port;
		}
		swr_connect_port(wsa883x->swr_slave, &port_id[0], num_port,
				&ch_mask[0], &ch_rate[0], &num_ch[0],
					&port_type[0]);
		break;
	case SND_SOC_DAPM_POST_PMU:
		set_bit(SPKR_STATUS, &wsa883x->status_mask);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		wsa883x_set_port(component, SWR_DAC_PORT,
				&port_id[num_port], &num_ch[num_port],
				&ch_mask[num_port], &ch_rate[num_port],
				&port_type[num_port]);
		++num_port;

		if (wsa883x->comp_enable) {
			wsa883x_set_port(component, SWR_COMP_PORT,
					&port_id[num_port], &num_ch[num_port],
					&ch_mask[num_port], &ch_rate[num_port],
					&port_type[num_port]);
			++num_port;
		}
		if (wsa883x->visense_enable) {
			wsa883x_set_port(component, SWR_VISENSE_PORT,
					&port_id[num_port], &num_ch[num_port],
					&ch_mask[num_port], &ch_rate[num_port],
					&port_type[num_port]);
			++num_port;
		}
		swr_disconnect_port(wsa883x->swr_slave, &port_id[0], num_port,
				&ch_mask[0], &port_type[0]);
		break;
	case SND_SOC_DAPM_POST_PMD:
		if (swr_set_device_group(wsa883x->swr_slave, SWR_GROUP_NONE))
			dev_err(component->dev,
				"%s: set num ch failed\n", __func__);

		swr_slvdev_datapath_control(wsa883x->swr_slave,
					    wsa883x->swr_slave->dev_num,
					    false);
		break;
	default:
		break;
	}
	return 0;
}

static int wsa883x_spkr_event(struct snd_soc_dapm_widget *w,
			struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component =
			snd_soc_dapm_to_component(w->dapm);
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	dev_dbg(component->dev, "%s: %s %d\n", __func__, w->name, event);
	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		swr_slvdev_datapath_control(wsa883x->swr_slave,
					    wsa883x->swr_slave->dev_num,
					    true);
		/* Added delay as per HW sequence */
		usleep_range(250, 300);
		snd_soc_component_update_bits(component, WSA883X_DRE_CTL_1,
						0x01, 0x01);
		/* Added delay as per HW sequence */
		usleep_range(250, 300);
		wcd_enable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_PA_ON_ERR);
		wcd_enable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_UVLO);
		/* Force remove group */
		swr_remove_from_group(wsa883x->swr_slave,
				      wsa883x->swr_slave->dev_num);
		snd_soc_component_update_bits(component,
				WSA883X_VBAT_ADC_FLT_CTL,
				0x0E, 0x06);
		snd_soc_component_update_bits(component,
				WSA883X_VBAT_ADC_FLT_CTL,
				0x01, 0x01);
		if (test_bit(SPKR_ADIE_LB, &wsa883x->status_mask))
			snd_soc_component_update_bits(component,
				WSA883X_PA_FSM_CTL, 0x01, 0x01);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		if (!test_bit(SPKR_ADIE_LB, &wsa883x->status_mask))
			wcd_disable_irq(&wsa883x->irq_info,
					WSA883X_IRQ_INT_PDM_WD);
		snd_soc_component_update_bits(component,
				WSA883X_VBAT_ADC_FLT_CTL,
				0x01, 0x00);
		snd_soc_component_update_bits(component,
				WSA883X_VBAT_ADC_FLT_CTL,
				0x0E, 0x00);
		snd_soc_component_update_bits(component, WSA883X_PA_FSM_CTL,
				0x01, 0x00);
		snd_soc_component_update_bits(component, WSA883X_PA_FSM_CTL,
				0x10, 0x00);
		snd_soc_component_update_bits(component, WSA883X_PA_FSM_CTL,
				0x10, 0x10);
		snd_soc_component_update_bits(component, WSA883X_PA_FSM_CTL,
				0x10, 0x00);
		snd_soc_component_update_bits(component, WSA883X_PDM_WD_CTL,
				0x01, 0x00);
		wcd_disable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_UVLO);
		wcd_disable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_PA_ON_ERR);
		clear_bit(SPKR_STATUS, &wsa883x->status_mask);
		clear_bit(SPKR_ADIE_LB, &wsa883x->status_mask);
		break;
	}
	return 0;
}

static const struct snd_soc_dapm_widget wsa883x_dapm_widgets[] = {
	SND_SOC_DAPM_INPUT("IN"),
	SND_SOC_DAPM_MIXER_E("SWR DAC_Port", SND_SOC_NOPM, 0, 0, swr_dac_port,
		ARRAY_SIZE(swr_dac_port), wsa883x_enable_swr_dac_port,
		SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMU |
		SND_SOC_DAPM_PRE_PMD | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_SPK("SPKR", wsa883x_spkr_event),
};

static const struct snd_soc_dapm_route wsa883x_audio_map[] = {
	{"SWR DAC_Port", "Switch", "IN"},
	{"SPKR", NULL, "SWR DAC_Port"},
};

int wsa883x_set_channel_map(struct snd_soc_component *component, u8 *port,
			    u8 num_port, unsigned int *ch_mask,
			    unsigned int *ch_rate, u8 *port_type)
{
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);
	int i;

	if (!port || !ch_mask || !ch_rate ||
		(num_port > WSA883X_MAX_SWR_PORTS)) {
		dev_err(component->dev,
			"%s: Invalid port=%pK, ch_mask=%pK, ch_rate=%pK\n",
			__func__, port, ch_mask, ch_rate);
		return -EINVAL;
	}
	for (i = 0; i < num_port; i++) {
		wsa883x->port[i].port_id = port[i];
		wsa883x->port[i].ch_mask = ch_mask[i];
		wsa883x->port[i].ch_rate = ch_rate[i];
		wsa883x->port[i].num_ch = __sw_hweight8(ch_mask[i]);
		if (port_type)
			wsa883x->port[i].port_type = port_type[i];
	}

	return 0;
}
EXPORT_SYMBOL(wsa883x_set_channel_map);

static void wsa883x_codec_init(struct snd_soc_component *component)
{
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);
	int i;

	if (!wsa883x)
		return;

	for (i = 0; i < ARRAY_SIZE(reg_init); i++)
		snd_soc_component_update_bits(component, reg_init[i].reg,
					reg_init[i].mask, reg_init[i].val);

	if (wsa883x->variant == WSA8830)
		snd_soc_component_update_bits(component, WSA883X_DRE_CTL_0,
					0x07, 0x03);
}

static int32_t wsa883x_temp_reg_read(struct snd_soc_component *component,
				     struct wsa_temp_register *wsa_temp_reg)
{
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	if (!wsa883x) {
		dev_err(component->dev, "%s: wsa883x is NULL\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&wsa883x->res_lock);

	snd_soc_component_update_bits(component, WSA883X_PA_FSM_BYP,
				0x01, 0x01);
	snd_soc_component_update_bits(component, WSA883X_PA_FSM_BYP,
				0x04, 0x04);
	snd_soc_component_update_bits(component, WSA883X_PA_FSM_BYP,
				0x02, 0x02);
	snd_soc_component_update_bits(component, WSA883X_PA_FSM_BYP,
				0x80, 0x80);
	snd_soc_component_update_bits(component, WSA883X_PA_FSM_BYP,
				0x20, 0x20);
	snd_soc_component_update_bits(component, WSA883X_PA_FSM_BYP,
				0x40, 0x40);

	snd_soc_component_update_bits(component, WSA883X_TADC_VALUE_CTL,
				0x01, 0x00);
	wsa_temp_reg->dmeas_msb = snd_soc_component_read32(
					component, WSA883X_TEMP_MSB);
	wsa_temp_reg->dmeas_lsb = snd_soc_component_read32(
					component, WSA883X_TEMP_LSB);
	snd_soc_component_update_bits(component, WSA883X_TADC_VALUE_CTL,
					0x01, 0x01);
	wsa_temp_reg->d1_msb = snd_soc_component_read32(
					component, WSA883X_OTP_REG_1);
	wsa_temp_reg->d1_lsb = snd_soc_component_read32(
					component, WSA883X_OTP_REG_2);
	wsa_temp_reg->d2_msb = snd_soc_component_read32(
					component, WSA883X_OTP_REG_3);
	wsa_temp_reg->d2_lsb = snd_soc_component_read32(
					component, WSA883X_OTP_REG_4);

	snd_soc_component_update_bits(component, WSA883X_PA_FSM_BYP,
				0xE7, 0x00);
	mutex_unlock(&wsa883x->res_lock);

	return 0;
}

static int wsa883x_get_temperature(struct snd_soc_component *component,
				   int *temp)
{
	struct wsa_temp_register reg;
	int dmeas, d1, d2;
	int ret = 0;
	int temp_val = 0;
	int t1 = T1_TEMP;
	int t2 = T2_TEMP;
	u8 retry = WSA883X_TEMP_RETRY;
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	if (!wsa883x)
		return -EINVAL;

	do {
		ret = wsa883x_temp_reg_read(component, &reg);
		if (ret) {
			pr_err("%s: temp read failed: %d, current temp: %d\n",
				__func__, ret, wsa883x->curr_temp);
			if (temp)
				*temp = wsa883x->curr_temp;
			return 0;
		}
		/*
		 * Temperature register values are expected to be in the
		 * following range.
		 * d1_msb  = 68 - 92 and d1_lsb  = 0, 64, 128, 192
		 * d2_msb  = 185 -218 and  d2_lsb  = 0, 64, 128, 192
		 */
		if ((reg.d1_msb < 68 || reg.d1_msb > 92) ||
		    (!(reg.d1_lsb == 0 || reg.d1_lsb == 64 || reg.d1_lsb == 128 ||
			reg.d1_lsb == 192)) ||
		    (reg.d2_msb < 185 || reg.d2_msb > 218) ||
		    (!(reg.d2_lsb == 0 || reg.d2_lsb == 64 || reg.d2_lsb == 128 ||
			reg.d2_lsb == 192))) {
			printk_ratelimited("%s: Temperature registers[%d %d %d %d] are out of range\n",
					   __func__, reg.d1_msb, reg.d1_lsb, reg.d2_msb,
					   reg.d2_lsb);
		}
		dmeas = ((reg.dmeas_msb << 0x8) | reg.dmeas_lsb) >> 0x6;
		d1 = ((reg.d1_msb << 0x8) | reg.d1_lsb) >> 0x6;
		d2 = ((reg.d2_msb << 0x8) | reg.d2_lsb) >> 0x6;

		if (d1 == d2)
			temp_val = TEMP_INVALID;
		else
			temp_val = t1 + (((dmeas - d1) * (t2 - t1))/(d2 - d1));

		if (temp_val <= LOW_TEMP_THRESHOLD ||
			temp_val >= HIGH_TEMP_THRESHOLD) {
			pr_debug("%s: T0: %d is out of range[%d, %d]\n", __func__,
				 temp_val, LOW_TEMP_THRESHOLD, HIGH_TEMP_THRESHOLD);
			if (retry--)
				msleep(10);
		} else {
			break;
		}
	} while (retry);

	wsa883x->curr_temp = temp_val;
	if (temp)
		*temp = temp_val;
	pr_debug("%s: t0 measured: %d dmeas = %d, d1 = %d, d2 = %d\n",
		  __func__, temp_val, dmeas, d1, d2);

	return ret;
}

static int wsa883x_codec_probe(struct snd_soc_component *component)
{
	char w_name[MAX_NAME_LEN];
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);
	struct swr_device *dev;
	int variant = 0, version = 0;
	struct snd_soc_dapm_context *dapm =
			snd_soc_component_get_dapm(component);

	if (!wsa883x)
		return -EINVAL;

	if (!component->name_prefix)
		return -EINVAL;

	snd_soc_component_init_regmap(component, wsa883x->regmap);

	dev = wsa883x->swr_slave;
	wsa883x->component = component;

	variant = (snd_soc_component_read32(component, WSA883X_OTP_REG_0)
					    & 0x0F);
	wsa883x->variant = variant;

	version = (snd_soc_component_read32(component, WSA883X_CHIP_ID0)
					    & 0xFF);
	wsa883x->version = version;

	wsa883x_codec_init(component);
	wsa883x->global_pa_cnt = 0;

	memset(w_name, 0, sizeof(w_name));
	strlcpy(w_name, component->name_prefix, sizeof(w_name));
	strlcat(w_name, " ", sizeof(w_name));
	strlcat(w_name, wsa883x->dai_driver->playback.stream_name,
				sizeof(w_name));
	snd_soc_dapm_ignore_suspend(dapm, w_name);

	memset(w_name, 0, sizeof(w_name));
	strlcpy(w_name, component->name_prefix, sizeof(w_name));
	strlcat(w_name, " IN", sizeof(w_name));
	snd_soc_dapm_ignore_suspend(dapm, w_name);

	memset(w_name, 0, sizeof(w_name));
	strlcpy(w_name, component->name_prefix, sizeof(w_name));
	strlcat(w_name, " SWR DAC_PORT", sizeof(w_name));
	snd_soc_dapm_ignore_suspend(dapm, w_name);

	memset(w_name, 0, sizeof(w_name));
	strlcpy(w_name, component->name_prefix, sizeof(w_name));
	strlcat(w_name, " SPKR", sizeof(w_name));
	snd_soc_dapm_ignore_suspend(dapm, w_name);

	snd_soc_dapm_sync(dapm);

	return 0;
}

static void wsa883x_codec_remove(struct snd_soc_component *component)
{
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	if (!wsa883x)
		return;

	snd_soc_component_exit_regmap(component);

	return;
}

static int wsa883x_soc_codec_suspend(struct snd_soc_component *component)
{
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	if (!wsa883x)
		return 0;

	wsa883x->dapm_bias_off = true;
	return 0;
}

static int wsa883x_soc_codec_resume(struct snd_soc_component *component)
{
	struct wsa883x_priv *wsa883x = snd_soc_component_get_drvdata(component);

	if (!wsa883x)
		return 0;

	wsa883x->dapm_bias_off = false;
	return 0;
}

static const struct snd_soc_component_driver soc_codec_dev_wsa883x_wsa = {
	.name = "",
	.probe = wsa883x_codec_probe,
	.remove = wsa883x_codec_remove,
	.controls = wsa883x_snd_controls,
	.num_controls = ARRAY_SIZE(wsa883x_snd_controls),
	.dapm_widgets = wsa883x_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(wsa883x_dapm_widgets),
	.dapm_routes = wsa883x_audio_map,
	.num_dapm_routes = ARRAY_SIZE(wsa883x_audio_map),
	.suspend =  wsa883x_soc_codec_suspend,
	.resume = wsa883x_soc_codec_resume,
};

static int wsa883x_gpio_ctrl(struct wsa883x_priv *wsa883x, bool enable)
{
	int ret = 0;

	if (enable)
		ret = msm_cdc_pinctrl_select_active_state(
						wsa883x->wsa_rst_np);
	else
		ret = msm_cdc_pinctrl_select_sleep_state(
						wsa883x->wsa_rst_np);
	if (ret != 0)
		dev_err(wsa883x->dev,
			"%s: Failed to turn state %d; ret=%d\n",
			__func__, enable, ret);

	return ret;
}

static int wsa883x_swr_up(struct wsa883x_priv *wsa883x)
{
	int ret;

	ret = wsa883x_gpio_ctrl(wsa883x, true);
	if (ret)
		dev_err(wsa883x->dev, "%s: Failed to enable gpio\n", __func__);

	return ret;
}

static int wsa883x_swr_down(struct wsa883x_priv *wsa883x)
{
	int ret;

	ret = wsa883x_gpio_ctrl(wsa883x, false);
	if (ret)
		dev_err(wsa883x->dev, "%s: Failed to disable gpio\n", __func__);

	return ret;
}

static int wsa883x_swr_reset(struct wsa883x_priv *wsa883x)
{
	u8 retry = WSA883X_NUM_RETRY;
	u8 devnum = 0;
	struct swr_device *pdev;

	pdev = wsa883x->swr_slave;
	while (swr_get_logical_dev_num(pdev, pdev->addr, &devnum) && retry--) {
		/* Retry after 1 msec delay */
		usleep_range(1000, 1100);
	}
	pdev->dev_num = devnum;
	wsa883x_regcache_sync(wsa883x);

	return 0;
}

static int wsa883x_event_notify(struct notifier_block *nb,
				unsigned long val, void *ptr)
{
	u16 event = (val & 0xffff);
	struct wsa883x_priv *wsa883x = container_of(nb, struct wsa883x_priv,
						    parent_nblock);

	if (!wsa883x)
		return -EINVAL;

	switch (event) {
	case BOLERO_SLV_EVT_PA_OFF_PRE_SSR:
		if (test_bit(SPKR_STATUS, &wsa883x->status_mask))
			snd_soc_component_update_bits(wsa883x->component,
						WSA883X_PA_FSM_CTL,
						0x01, 0x00);
		wsa883x_swr_down(wsa883x);
		break;

	case BOLERO_SLV_EVT_SSR_UP:
		wsa883x_swr_up(wsa883x);
		/* Add delay to allow enumerate */
		usleep_range(20000, 20010);
		wsa883x_swr_reset(wsa883x);
		break;

	case BOLERO_SLV_EVT_PA_ON_POST_FSCLK:
		if (test_bit(SPKR_STATUS, &wsa883x->status_mask)) {
			snd_soc_component_update_bits(wsa883x->component,
						WSA883X_PDM_WD_CTL,
						0x01, 0x01);
			snd_soc_component_update_bits(wsa883x->component,
						WSA883X_PA_FSM_CTL,
						0x01, 0x01);
			wcd_enable_irq(&wsa883x->irq_info,
					WSA883X_IRQ_INT_PDM_WD);
			/* Added delay as per HW sequence */
			usleep_range(3000, 3100);
			snd_soc_component_update_bits(wsa883x->component,
						WSA883X_DRE_CTL_1,
						0x01, 0x00);
			/* Added delay as per HW sequence */
			usleep_range(5000, 5050);
		}
		break;
	case BOLERO_SLV_EVT_PA_ON_POST_FSCLK_ADIE_LB:
		if (test_bit(SPKR_STATUS, &wsa883x->status_mask))
			set_bit(SPKR_ADIE_LB, &wsa883x->status_mask);
		break;
	default:
		dev_dbg(wsa883x->dev, "%s: unknown event %d\n",
			__func__, event);
		break;
	}

	return 0;
}

static int wsa883x_enable_supplies(struct device * dev,
				   struct wsa883x_priv *priv)
{
	int ret = 0;

	/* Parse power supplies */
	msm_cdc_get_power_supplies(dev, &priv->regulator,
				   &priv->num_supplies);
	if (!priv->regulator || (priv->num_supplies <= 0)) {
		dev_err(dev, "%s: no power supplies defined\n", __func__);
		return -EINVAL;
	}

	ret = msm_cdc_init_supplies(dev, &priv->supplies,
				    priv->regulator, priv->num_supplies);
	if (!priv->supplies) {
		dev_err(dev, "%s: Cannot init wsa supplies\n",
			__func__);
		return ret;
	}

	ret = msm_cdc_enable_static_supplies(dev, priv->supplies,
					     priv->regulator,
					     priv->num_supplies);
	if (ret)
		dev_err(dev, "%s: wsa static supply enable failed!\n",
			__func__);

	return ret;
}

static struct snd_soc_dai_driver wsa_dai[] = {
	{
		.name = "",
		.playback = {
			.stream_name = "",
			.rates = WSA883X_RATES | WSA883X_FRAC_RATES,
			.formats = WSA883X_FORMATS,
			.rate_max = 192000,
			.rate_min = 8000,
			.channels_min = 1,
			.channels_max = 2,
		},
	},
};

static int wsa883x_swr_probe(struct swr_device *pdev)
{
	int ret = 0, i = 0;
	struct wsa883x_priv *wsa883x;
	u8 devnum = 0;
	bool pin_state_current = false;
	struct wsa_ctrl_platform_data *plat_data = NULL;
	struct snd_soc_component *component;
	const char *wsa883x_name_prefix_of = NULL;
	char buffer[MAX_NAME_LEN];
	int dev_index = 0;

	wsa883x = devm_kzalloc(&pdev->dev, sizeof(struct wsa883x_priv),
			    GFP_KERNEL);
	if (!wsa883x)
		return -ENOMEM;

	ret = wsa883x_enable_supplies(&pdev->dev, wsa883x);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	wsa883x->wsa_rst_np = of_parse_phandle(pdev->dev.of_node,
					     "qcom,spkr-sd-n-node", 0);
	if (!wsa883x->wsa_rst_np) {
		dev_dbg(&pdev->dev, "%s: pinctrl not defined\n", __func__);
		goto err;
	}
	swr_set_dev_data(pdev, wsa883x);
	wsa883x->swr_slave = pdev;
	pin_state_current = msm_cdc_pinctrl_get_state(wsa883x->wsa_rst_np);
	wsa883x_gpio_ctrl(wsa883x, true);
	/*
	 * Add 5msec delay to provide sufficient time for
	 * soundwire auto enumeration of slave devices as
	 * as per HW requirement.
	 */
	usleep_range(5000, 5010);
	ret = swr_get_logical_dev_num(pdev, pdev->addr, &devnum);
	if (ret) {
		dev_dbg(&pdev->dev,
			"%s get devnum %d for dev addr %lx failed\n",
			__func__, devnum, pdev->addr);
		ret = -EPROBE_DEFER;
		goto err;
	}
	pdev->dev_num = devnum;

	wsa883x->regmap = devm_regmap_init_swr(pdev,
					       &wsa883x_regmap_config);
	if (IS_ERR(wsa883x->regmap)) {
		ret = PTR_ERR(wsa883x->regmap);
		dev_err(&pdev->dev, "%s: regmap_init failed %d\n",
			__func__, ret);
		goto dev_err;
	}

	/* Set all interrupts as edge triggered */
	for (i = 0; i < wsa883x_regmap_irq_chip.num_regs; i++)
		regmap_write(wsa883x->regmap, (WSA883X_INTR_LEVEL0 + i), 0);

	wsa883x_regmap_irq_chip.irq_drv_data = wsa883x;
	wsa883x->irq_info.wcd_regmap_irq_chip = &wsa883x_regmap_irq_chip;
	wsa883x->irq_info.codec_name = "WSA883X";
	wsa883x->irq_info.regmap = wsa883x->regmap;
	wsa883x->irq_info.dev = &pdev->dev;
	ret = wcd_irq_init(&wsa883x->irq_info, &wsa883x->virq);

	if (ret) {
		dev_err(wsa883x->dev, "%s: IRQ init failed: %d\n",
			__func__, ret);
		goto dev_err;
	}

	wcd_request_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_SAF2WAR,
			"WSA SAF2WAR", wsa883x_saf2war_handle_irq, NULL);

	wcd_disable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_SAF2WAR);

	wcd_request_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_WAR2SAF,
			"WSA WAR2SAF", wsa883x_war2saf_handle_irq, NULL);

	wcd_disable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_WAR2SAF);

	wcd_request_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_DISABLE,
			"WSA OTP", wsa883x_otp_handle_irq, NULL);

	wcd_disable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_DISABLE);

	wcd_request_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_OCP,
			"WSA OCP", wsa883x_ocp_handle_irq, NULL);

	wcd_disable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_OCP);

	wcd_request_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_CLIP,
			"WSA CLIP", wsa883x_clip_handle_irq, NULL);

	wcd_disable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_CLIP);

	wcd_request_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_PDM_WD,
			"WSA PDM WD", wsa883x_pdm_wd_handle_irq, NULL);

	wcd_disable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_PDM_WD);

	wcd_request_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_CLK_WD,
			"WSA CLK WD", wsa883x_clk_wd_handle_irq, NULL);

	wcd_disable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_CLK_WD);

	wcd_request_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_INTR_PIN,
			"WSA EXT INT", wsa883x_ext_int_handle_irq, NULL);

	wcd_disable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_INTR_PIN);

	/* Under Voltage Lock out (UVLO) interrupt handle */
	wcd_request_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_UVLO,
			"WSA UVLO", wsa883x_uvlo_handle_irq, NULL);

	wcd_disable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_UVLO);

	wcd_request_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_PA_ON_ERR,
			"WSA PA ERR", wsa883x_pa_on_err_handle_irq, wsa883x);

	wcd_disable_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_PA_ON_ERR);

	ret = of_property_read_string(pdev->dev.of_node, "qcom,wsa-prefix",
				&wsa883x_name_prefix_of);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: Looking up %s property in node %s failed\n",
			__func__, "qcom,wsa-prefix",
			pdev->dev.of_node->full_name);
		goto err_irq;
	}

	wsa883x->driver = devm_kzalloc(&pdev->dev,
			sizeof(struct snd_soc_component_driver), GFP_KERNEL);
        if (!wsa883x->driver) {
                ret = -ENOMEM;
                goto err_irq;
        }

        memcpy(wsa883x->driver, &soc_codec_dev_wsa883x_wsa,
                        sizeof(struct snd_soc_component_driver));

	wsa883x->dai_driver = devm_kzalloc(&pdev->dev,
				sizeof(struct snd_soc_dai_driver), GFP_KERNEL);
	if (!wsa883x->dai_driver) {
		ret = -ENOMEM;
		goto err_mem;
	}

	memcpy(wsa883x->dai_driver, wsa_dai, sizeof(struct snd_soc_dai_driver));

	/* Get last digit from HEX format */
	dev_index = (int)((char)(pdev->addr & 0xF));

	snprintf(buffer, sizeof(buffer), "wsa-codec.%d", dev_index);
	wsa883x->driver->name = kstrndup(buffer, strlen(buffer), GFP_KERNEL);

	snprintf(buffer, sizeof(buffer), "wsa_rx%d", dev_index);
	wsa883x->dai_driver->name =
				kstrndup(buffer, strlen(buffer), GFP_KERNEL);

	snprintf(buffer, sizeof(buffer), "WSA883X_AIF%d Playback", dev_index);
	wsa883x->dai_driver->playback.stream_name =
				kstrndup(buffer, strlen(buffer), GFP_KERNEL);

	/* Number of DAI's used is 1 */
	ret = snd_soc_register_component(&pdev->dev,
				wsa883x->driver, wsa883x->dai_driver, 1);

	wsa883x->wsa883x_name_prefix = kstrndup(wsa883x_name_prefix_of,
			strlen(wsa883x_name_prefix_of), GFP_KERNEL);
	component = snd_soc_lookup_component(&pdev->dev, wsa883x->driver->name);
	if (!component) {
		dev_err(&pdev->dev, "%s: component is NULL \n", __func__);
		ret = -EINVAL;
		goto err_mem;
	}
	component->name_prefix = wsa883x->wsa883x_name_prefix;

	wsa883x->parent_np = of_parse_phandle(pdev->dev.of_node,
					      "qcom,bolero-handle", 0);
	if (wsa883x->parent_np) {
		wsa883x->parent_dev =
				of_find_device_by_node(wsa883x->parent_np);
		if (wsa883x->parent_dev) {
			plat_data = dev_get_platdata(&wsa883x->parent_dev->dev);
			if (plat_data) {
				wsa883x->parent_nblock.notifier_call =
							wsa883x_event_notify;
				if (plat_data->register_notifier)
					plat_data->register_notifier(
						plat_data->handle,
						&wsa883x->parent_nblock,
						true);
				wsa883x->register_notifier =
						plat_data->register_notifier;
				wsa883x->handle = plat_data->handle;
			} else {
				dev_err(&pdev->dev, "%s: plat data not found\n",
					__func__);
			}
		} else {
			dev_err(&pdev->dev, "%s: parent dev not found\n",
				__func__);
		}
	} else {
		dev_info(&pdev->dev, "%s: parent node not found\n", __func__);
	}

	mutex_init(&wsa883x->res_lock);

#ifdef CONFIG_DEBUG_FS
	if (!wsa883x->debugfs_dent) {
		wsa883x->debugfs_dent = debugfs_create_dir(
					dev_name(&pdev->dev), 0);
		if (!IS_ERR(wsa883x->debugfs_dent)) {
			wsa883x->debugfs_peek =
				debugfs_create_file("swrslave_peek",
				S_IFREG | 0444,
				wsa883x->debugfs_dent,
				(void *) pdev,
				&codec_debug_read_ops);

		wsa883x->debugfs_poke =
				debugfs_create_file("swrslave_poke",
				S_IFREG | 0444,
				wsa883x->debugfs_dent,
				(void *) pdev,
				&codec_debug_write_ops);

		wsa883x->debugfs_reg_dump =
				debugfs_create_file(
				"swrslave_reg_dump",
				S_IFREG | 0444,
				wsa883x->debugfs_dent,
				(void *) pdev,
				&codec_debug_dump_ops);
	}
}
#endif

	return 0;

err_mem:
	kfree(wsa883x->wsa883x_name_prefix);
	if (wsa883x->dai_driver) {
		kfree(wsa883x->dai_driver->name);
		kfree(wsa883x->dai_driver->playback.stream_name);
		kfree(wsa883x->dai_driver);
	}
	if (wsa883x->driver) {
		kfree(wsa883x->driver->name);
		kfree(wsa883x->driver);
	}
err_irq:
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_SAF2WAR, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_WAR2SAF, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_DISABLE, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_OCP, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_CLIP, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_PDM_WD, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_CLK_WD, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_INTR_PIN, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_UVLO, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_PA_ON_ERR, NULL);
	wcd_irq_exit(&wsa883x->irq_info, wsa883x->virq);
dev_err:
	if (pin_state_current == false)
		wsa883x_gpio_ctrl(wsa883x, false);
	swr_remove_device(pdev);
err:
	swr_set_dev_data(pdev, NULL);
	return ret;
}

static int wsa883x_swr_remove(struct swr_device *pdev)
{
	struct wsa883x_priv *wsa883x;

	wsa883x = swr_get_dev_data(pdev);
	if (!wsa883x) {
		dev_err(&pdev->dev, "%s: wsa883x is NULL\n", __func__);
		return -EINVAL;
	}

	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_SAF2WAR, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_WAR2SAF, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_DISABLE, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_OCP, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_CLIP, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_PDM_WD, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_CLK_WD, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_INTR_PIN, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_UVLO, NULL);
	wcd_free_irq(&wsa883x->irq_info, WSA883X_IRQ_INT_PA_ON_ERR, NULL);

	if (wsa883x->register_notifier)
		wsa883x->register_notifier(wsa883x->handle,
				&wsa883x->parent_nblock, false);
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(wsa883x->debugfs_dent);
	wsa883x->debugfs_dent = NULL;
#endif
	mutex_destroy(&wsa883x->res_lock);
	snd_soc_unregister_component(&pdev->dev);
	kfree(wsa883x->wsa883x_name_prefix);
	if (wsa883x->dai_driver) {
		kfree(wsa883x->dai_driver->name);
		kfree(wsa883x->dai_driver->playback.stream_name);
		kfree(wsa883x->dai_driver);
	}
	if (wsa883x->driver) {
		kfree(wsa883x->driver->name);
		kfree(wsa883x->driver);
	}
	swr_set_dev_data(pdev, NULL);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int wsa883x_swr_suspend(struct device *dev)
{
	struct wsa883x_priv *wsa883x = swr_get_dev_data(to_swr_device(dev));

	if (!wsa883x) {
		dev_err(dev, "%s: wsa883x private data is NULL\n", __func__);
		return -EINVAL;
	}
	dev_dbg(dev, "%s: system suspend\n", __func__);
	if (wsa883x->dapm_bias_off) {
		msm_cdc_set_supplies_lpm_mode(dev, wsa883x->supplies,
					wsa883x->regulator,
					wsa883x->num_supplies,
					true);
		set_bit(WSA_SUPPLIES_LPM_MODE, &wsa883x->status_mask);
	}
	return 0;
}

static int wsa883x_swr_resume(struct device *dev)
{
	struct wsa883x_priv *wsa883x = swr_get_dev_data(to_swr_device(dev));

	if (!wsa883x) {
		dev_err(dev, "%s: wsa883x private data is NULL\n", __func__);
		return -EINVAL;
	}
	if (test_bit(WSA_SUPPLIES_LPM_MODE, &wsa883x->status_mask)) {
		msm_cdc_set_supplies_lpm_mode(dev, wsa883x->supplies,
					wsa883x->regulator,
					wsa883x->num_supplies,
					false);
		clear_bit(WSA_SUPPLIES_LPM_MODE, &wsa883x->status_mask);
	}
	dev_dbg(dev, "%s: system resume\n", __func__);
	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops wsa883x_swr_pm_ops = {
	.suspend_late = wsa883x_swr_suspend,
	.resume_early = wsa883x_swr_resume,
};

static const struct swr_device_id wsa883x_swr_id[] = {
	{"wsa883x", 0},
	{}
};

static const struct of_device_id wsa883x_swr_dt_match[] = {
	{
		.compatible = "qcom,wsa883x",
	},
	{}
};

static struct swr_driver wsa883x_swr_driver = {
	.driver = {
		.name = "wsa883x",
		.owner = THIS_MODULE,
		.pm = &wsa883x_swr_pm_ops,
		.of_match_table = wsa883x_swr_dt_match,
	},
	.probe = wsa883x_swr_probe,
	.remove = wsa883x_swr_remove,
	.id_table = wsa883x_swr_id,
};

static int __init wsa883x_swr_init(void)
{
	return swr_driver_register(&wsa883x_swr_driver);
}

static void __exit wsa883x_swr_exit(void)
{
	swr_driver_unregister(&wsa883x_swr_driver);
}

module_init(wsa883x_swr_init);
module_exit(wsa883x_swr_exit);

MODULE_DESCRIPTION("WSA883x codec driver");
MODULE_LICENSE("GPL v2");
