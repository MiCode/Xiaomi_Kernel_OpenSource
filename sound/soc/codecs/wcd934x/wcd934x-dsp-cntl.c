/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/component.h>
#include <linux/debugfs.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-irq.h>
#include <linux/mfd/wcd934x/registers.h>
#include <sound/soc.h>
#include <sound/wcd-dsp-mgr.h>
#include "wcd934x.h"
#include "wcd934x-dsp-cntl.h"

#define WCD_CNTL_DIR_NAME_LEN_MAX 32
#define WCD_CPE_FLL_MAX_RETRIES 5
#define WCD_MEM_ENABLE_MAX_RETRIES 20
#define WCD_DSP_BOOT_TIMEOUT_MS 3000
#define WCD_SYSFS_ENTRY_MAX_LEN 8

#define WCD_CNTL_MUTEX_LOCK(codec, lock)         \
{                                                \
	dev_dbg(codec->dev, "mutex_lock(%s)\n",  \
		__func__);                       \
	mutex_lock(&lock);                       \
}

#define WCD_CNTL_MUTEX_UNLOCK(codec, lock)       \
{                                                \
	dev_dbg(codec->dev, "mutex_unlock(%s)\n",\
		__func__);                       \
	mutex_unlock(&lock);                     \
}

struct wcd_cntl_attribute {
	struct attribute attr;
	ssize_t (*show)(struct wcd_dsp_cntl *cntl, char *buf);
	ssize_t (*store)(struct wcd_dsp_cntl *cntl, const char *buf,
			 ssize_t count);
};

#define WCD_CNTL_ATTR(_name, _mode, _show, _store) \
static struct wcd_cntl_attribute cntl_attr_##_name = {	\
	.attr = {.name = __stringify(_name), .mode = _mode},	\
	.show = _show,	\
	.store = _store,	\
}

#define to_wcd_cntl_attr(a) \
	container_of((a), struct wcd_cntl_attribute, attr)

#define to_wcd_cntl(kobj) \
	container_of((kobj), struct wcd_dsp_cntl, wcd_kobj)

static u8 mem_enable_values[] = {
	0xFE, 0xFC, 0xF8, 0xF0,
	0xE0, 0xC0, 0x80, 0x00,
};

static ssize_t wdsp_boot_show(struct wcd_dsp_cntl *cntl, char *buf)
{
	return snprintf(buf, WCD_SYSFS_ENTRY_MAX_LEN,
			"%u", cntl->boot_reqs);
}

static ssize_t wdsp_boot_store(struct wcd_dsp_cntl *cntl,
			       const char *buf, ssize_t count)
{
	u32 val;
	bool vote;
	int ret;

	ret = kstrtou32(buf, 10, &val);
	if (ret) {
		dev_err(cntl->codec->dev,
			"%s: Invalid entry, ret = %d\n", __func__, ret);
		return -EINVAL;
	}

	if (val > 0) {
		cntl->boot_reqs++;
		vote = true;
	} else {
		cntl->boot_reqs--;
		vote = false;
	}

	if (cntl->m_dev && cntl->m_ops &&
	    cntl->m_ops->vote_for_dsp)
		ret = cntl->m_ops->vote_for_dsp(cntl->m_dev, vote);
	else
		ret = -EINVAL;

	if (IS_ERR_VALUE(ret))
		dev_err(cntl->codec->dev,
			"%s: failed to %s dsp\n", __func__,
			vote ? "enable" : "disable");
	return count;
}

WCD_CNTL_ATTR(boot, 0660, wdsp_boot_show, wdsp_boot_store);

static ssize_t wcd_cntl_sysfs_show(struct kobject *kobj,
				   struct attribute *attr, char *buf)
{
	struct wcd_cntl_attribute *wcd_attr = to_wcd_cntl_attr(attr);
	struct wcd_dsp_cntl *cntl = to_wcd_cntl(kobj);
	ssize_t ret = -EINVAL;

	if (cntl && wcd_attr->show)
		ret = wcd_attr->show(cntl, buf);

	return ret;
}

static ssize_t wcd_cntl_sysfs_store(struct kobject *kobj,
				    struct attribute *attr, const char *buf,
				    size_t count)
{
	struct wcd_cntl_attribute *wcd_attr = to_wcd_cntl_attr(attr);
	struct wcd_dsp_cntl *cntl = to_wcd_cntl(kobj);
	ssize_t ret = -EINVAL;

	if (cntl && wcd_attr->store)
		ret = wcd_attr->store(cntl, buf, count);

	return ret;
}

static const struct sysfs_ops wcd_cntl_sysfs_ops = {
	.show = wcd_cntl_sysfs_show,
	.store = wcd_cntl_sysfs_store,
};

static struct kobj_type wcd_cntl_ktype = {
	.sysfs_ops = &wcd_cntl_sysfs_ops,
};

static int wcd_cntl_cpe_fll_calibrate(struct wcd_dsp_cntl *cntl)
{
	struct snd_soc_codec *codec = cntl->codec;
	int ret = 0, retry = 0;
	u8 cal_lsb, cal_msb;
	u8 lock_det;

	/* Make sure clocks are gated */
	snd_soc_update_bits(codec, WCD934X_CPE_SS_CPE_CTL,
			    0x05, 0x00);

	/* Enable CPE FLL reference clock */
	snd_soc_update_bits(codec, WCD934X_CLK_SYS_MCLK2_PRG1,
			    0x80, 0x80);

	snd_soc_update_bits(codec, WCD934X_CPE_FLL_USER_CTL_5,
			    0xF3, 0x13);
	snd_soc_write(codec, WCD934X_CPE_FLL_L_VAL_CTL_0, 0x50);

	/* Disable CPAR reset and Enable CPAR clk */
	snd_soc_update_bits(codec, WCD934X_CPE_SS_CPAR_CTL,
			    0x02, 0x02);

	/* Write calibration l-value based on cdc clk rate */
	if (cntl->clk_rate == 9600000) {
		cal_lsb = 0x6d;
		cal_msb = 0x00;
	} else {
		cal_lsb = 0x56;
		cal_msb = 0x00;
	}
	snd_soc_write(codec, WCD934X_CPE_FLL_USER_CTL_6, cal_lsb);
	snd_soc_write(codec, WCD934X_CPE_FLL_USER_CTL_7, cal_msb);

	/* FLL mode to follow power up sequence */
	snd_soc_update_bits(codec, WCD934X_CPE_FLL_FLL_MODE,
			    0x60, 0x00);

	/* HW controlled CPE FLL */
	snd_soc_update_bits(codec, WCD934X_CPE_FLL_FLL_MODE,
			    0x80, 0x80);

	/* Force on CPE FLL */
	snd_soc_update_bits(codec, WCD934X_CPE_SS_CPAR_CFG,
			    0x04, 0x04);

	do {
		/* Time for FLL calibration to complete */
		usleep_range(1000, 1100);
		lock_det = snd_soc_read(codec, WCD934X_CPE_FLL_STATUS_3);
		retry++;
	} while (!(lock_det & 0x01) &&
		 retry <= WCD_CPE_FLL_MAX_RETRIES);

	if (!(lock_det & 0x01)) {
		dev_err(codec->dev, "%s: lock detect not set, 0x%02x\n",
			__func__, lock_det);
		ret = -EIO;
		goto err_lock_det;
	}

	snd_soc_update_bits(codec, WCD934X_CPE_FLL_FLL_MODE,
			    0x60, 0x20);
	snd_soc_update_bits(codec, WCD934X_CPE_SS_CPAR_CFG,
			    0x04, 0x00);
	return ret;

err_lock_det:
	/* Undo the register settings */
	snd_soc_update_bits(codec, WCD934X_CPE_SS_CPAR_CFG,
			    0x04, 0x00);
	snd_soc_update_bits(codec, WCD934X_CPE_FLL_FLL_MODE,
			    0x80, 0x00);
	snd_soc_update_bits(codec, WCD934X_CPE_SS_CPAR_CTL,
			    0x02, 0x00);
	return ret;
}

static void wcd_cntl_config_cpar(struct wcd_dsp_cntl *cntl)
{
	struct snd_soc_codec *codec = cntl->codec;
	u8 nom_lo, nom_hi, svs2_lo, svs2_hi;

	/* Configure CPAR */
	nom_hi = svs2_hi = 0;
	if (cntl->clk_rate == 9600000) {
		nom_lo = 0x90;
		svs2_lo = 0x50;
	} else {
		nom_lo = 0x70;
		svs2_lo = 0x3e;
	}

	snd_soc_write(codec, WCD934X_TEST_DEBUG_LVAL_NOM_LOW, nom_lo);
	snd_soc_write(codec, WCD934X_TEST_DEBUG_LVAL_NOM_HIGH, nom_hi);
	snd_soc_write(codec, WCD934X_TEST_DEBUG_LVAL_SVS_SVS2_LOW, svs2_lo);
	snd_soc_write(codec, WCD934X_TEST_DEBUG_LVAL_SVS_SVS2_HIGH, svs2_hi);

	snd_soc_update_bits(codec, WCD934X_CPE_SS_PWR_CPEFLL_CTL,
			    0x03, 0x03);
}

static int wcd_cntl_cpe_fll_ctrl(struct wcd_dsp_cntl *cntl,
				 bool enable)
{
	struct snd_soc_codec *codec = cntl->codec;
	int ret = 0;

	if (enable) {
		ret = wcd_cntl_cpe_fll_calibrate(cntl);
		if (IS_ERR_VALUE(ret)) {
			dev_err(codec->dev,
				"%s: cpe_fll_cal failed, err = %d\n",
				__func__, ret);
			goto done;
		}

		wcd_cntl_config_cpar(cntl);

		/* Enable AHB CLK and CPE CLK*/
		snd_soc_update_bits(codec, WCD934X_CPE_SS_CPE_CTL,
				    0x05, 0x05);
	} else {
		/* Disable AHB CLK and CPE CLK */
		snd_soc_update_bits(codec, WCD934X_CPE_SS_CPE_CTL,
				    0x05, 0x00);
		/* Reset the CPAR mode for CPE FLL */
		snd_soc_write(codec, WCD934X_CPE_FLL_FLL_MODE, 0x20);
		snd_soc_update_bits(codec, WCD934X_CPE_SS_CPAR_CFG,
				    0x04, 0x00);
		snd_soc_update_bits(codec, WCD934X_CPE_SS_CPAR_CTL,
				    0x02, 0x00);
	}
done:
	return ret;
}

static int wcd_cntl_clocks_enable(struct wcd_dsp_cntl *cntl)
{
	struct snd_soc_codec *codec = cntl->codec;
	int ret;

	WCD_CNTL_MUTEX_LOCK(codec, cntl->clk_mutex);
	/* Enable codec clock */
	if (cntl->cdc_cb && cntl->cdc_cb->cdc_clk_en)
		ret = cntl->cdc_cb->cdc_clk_en(codec, true);
	else
		ret = -EINVAL;

	if (IS_ERR_VALUE(ret)) {
		dev_err(codec->dev,
			"%s: Failed to enable cdc clk, err = %d\n",
			__func__, ret);
		goto done;
	}

	/* Configure and Enable CPE FLL clock */
	ret = wcd_cntl_cpe_fll_ctrl(cntl, true);
	if (IS_ERR_VALUE(ret)) {
		dev_err(codec->dev,
			"%s: Failed to enable cpe clk, err = %d\n",
			__func__, ret);
		goto err_cpe_clk;
	}
	cntl->is_clk_enabled = true;

	/* Ungate the CPR clock  */
	snd_soc_update_bits(codec, WCD934X_CODEC_RPM_CLK_GATE, 0x10, 0x00);
done:
	WCD_CNTL_MUTEX_UNLOCK(codec, cntl->clk_mutex);
	return ret;

err_cpe_clk:
	if (cntl->cdc_cb && cntl->cdc_cb->cdc_clk_en)
		cntl->cdc_cb->cdc_clk_en(codec, false);

	WCD_CNTL_MUTEX_UNLOCK(codec, cntl->clk_mutex);
	return ret;
}

static int wcd_cntl_clocks_disable(struct wcd_dsp_cntl *cntl)
{
	struct snd_soc_codec *codec = cntl->codec;
	int ret = 0;

	WCD_CNTL_MUTEX_LOCK(codec, cntl->clk_mutex);
	if (!cntl->is_clk_enabled) {
		dev_info(codec->dev, "%s: clocks already disabled\n",
			__func__);
		goto done;
	}

	/* Gate the CPR clock  */
	snd_soc_update_bits(codec, WCD934X_CODEC_RPM_CLK_GATE, 0x10, 0x10);

	/* Disable CPE FLL clock */
	ret = wcd_cntl_cpe_fll_ctrl(cntl, false);
	if (IS_ERR_VALUE(ret))
		dev_err(codec->dev,
			"%s: Failed to disable cpe clk, err = %d\n",
			__func__, ret);

	/*
	 * Even if CPE FLL disable failed, go ahead and disable
	 * the codec clock
	 */
	if (cntl->cdc_cb && cntl->cdc_cb->cdc_clk_en)
		ret = cntl->cdc_cb->cdc_clk_en(codec, false);
	else
		ret = -EINVAL;

	cntl->is_clk_enabled = false;
done:
	WCD_CNTL_MUTEX_UNLOCK(codec, cntl->clk_mutex);
	return ret;
}

static void wcd_cntl_cpar_ctrl(struct wcd_dsp_cntl *cntl,
			       bool enable)
{
	struct snd_soc_codec *codec = cntl->codec;

	if (enable)
		snd_soc_write(codec, WCD934X_CPE_SS_CPAR_CTL, 0x03);
	else
		snd_soc_write(codec, WCD934X_CPE_SS_CPAR_CTL, 0x00);
}

static int wcd_cntl_enable_memory(struct wcd_dsp_cntl *cntl)
{
	struct snd_soc_codec *codec = cntl->codec;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	int loop_cnt = 0;
	u8 status;
	int ret = 0;

	snd_soc_update_bits(codec, WCD934X_CPE_SS_SOC_SW_COLLAPSE_CTL,
			    0x04, 0x00);
	snd_soc_update_bits(codec, WCD934X_TEST_DEBUG_MEM_CTRL,
			    0x80, 0x80);
	snd_soc_update_bits(codec, WCD934X_CPE_SS_SOC_SW_COLLAPSE_CTL,
			    0x01, 0x01);

	do {
		loop_cnt++;
		/* Time to enable the power domain for memory */
		usleep_range(100, 150);
		status = snd_soc_read(codec,
				      WCD934X_CPE_SS_SOC_SW_COLLAPSE_CTL);
	} while ((status & 0x02) != 0x02 &&
		 loop_cnt != WCD_MEM_ENABLE_MAX_RETRIES);

	if ((status & 0x02) != 0x02) {
		dev_err(cntl->codec->dev,
			"%s: power domain not enabled, status = 0x%02x\n",
			__func__, status);
		ret = -EIO;
		goto done;
	}

	/* 512KB of always on region */
	wcd9xxx_slim_write_repeat(wcd9xxx,
				  WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_0,
				  ARRAY_SIZE(mem_enable_values),
				  mem_enable_values);
	wcd9xxx_slim_write_repeat(wcd9xxx,
				  WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_1,
				  ARRAY_SIZE(mem_enable_values),
				  mem_enable_values);

	snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_DRAM1_SHUTDOWN, 0x05);

	/* Rest of the memory */
	wcd9xxx_slim_write_repeat(wcd9xxx,
				  WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_2,
				  ARRAY_SIZE(mem_enable_values),
				  mem_enable_values);
	wcd9xxx_slim_write_repeat(wcd9xxx,
				  WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_3,
				  ARRAY_SIZE(mem_enable_values),
				  mem_enable_values);

	snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_DEEPSLP_0, 0x05);
done:
	return ret;
}

static void wcd_cntl_disable_memory(struct wcd_dsp_cntl *cntl)
{
	struct snd_soc_codec *codec = cntl->codec;

	snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_DEEPSLP_0, 0xFF);
	snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_3, 0xFF);
	snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_2, 0xFF);
	snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_DRAM1_SHUTDOWN, 0x07);
	snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_1, 0xFF);
	snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_0, 0xFF);

	snd_soc_update_bits(codec, WCD934X_CPE_SS_SOC_SW_COLLAPSE_CTL,
			    0x01, 0x00);
	snd_soc_update_bits(codec, WCD934X_TEST_DEBUG_MEM_CTRL,
			    0x80, 0x00);
	snd_soc_update_bits(codec, WCD934X_CPE_SS_SOC_SW_COLLAPSE_CTL,
			    0x04, 0x04);
}

static void wcd_cntl_do_shutdown(struct wcd_dsp_cntl *cntl)
{
	struct snd_soc_codec *codec = cntl->codec;

	/* Disable WDOG */
	snd_soc_update_bits(codec, WCD934X_CPE_SS_WDOG_CFG,
			    0x3F, 0x01);
	snd_soc_update_bits(codec, WCD934X_CODEC_RPM_CLK_MCLK_CFG,
			    0x04, 0x00);

	/* Put WDSP in reset state */
	snd_soc_update_bits(codec, WCD934X_CPE_SS_CPE_CTL,
			    0x02, 0x00);
}

static int wcd_cntl_do_boot(struct wcd_dsp_cntl *cntl)
{
	struct snd_soc_codec *codec = cntl->codec;
	int ret = 0;

	/*
	 * Debug mode is set from debugfs file node. If debug_mode
	 * is set, then do not configure the watchdog timer. This
	 * will be required for debugging the DSP firmware.
	 */
	if (cntl->debug_mode) {
		snd_soc_update_bits(codec, WCD934X_CPE_SS_WDOG_CFG,
				    0x3F, 0x01);
		snd_soc_update_bits(codec, WCD934X_CODEC_RPM_CLK_MCLK_CFG,
				    0x04, 0x00);
	} else {
		snd_soc_update_bits(codec, WCD934X_CODEC_RPM_CLK_MCLK_CFG,
				    0x04, 0x04);
		snd_soc_update_bits(codec, WCD934X_CPE_SS_WDOG_CFG,
				    0x3F, 0x21);
	}

	/* Make sure all the error interrupts are cleared */
	snd_soc_write(codec, WCD934X_CPE_SS_SS_ERROR_INT_CLEAR_0A, 0xFF);
	snd_soc_write(codec, WCD934X_CPE_SS_SS_ERROR_INT_CLEAR_0B, 0xFF);

	reinit_completion(&cntl->boot_complete);

	/* Remove WDSP out of reset */
	snd_soc_update_bits(codec, WCD934X_CPE_SS_CPE_CTL,
			    0x02, 0x02);

	/*
	 * In debug mode, DSP may not boot up normally,
	 * wait indefinitely for DSP to boot.
	 */
	if (cntl->debug_mode) {
		wait_for_completion(&cntl->boot_complete);
		dev_dbg(codec->dev, "%s: WDSP booted in dbg mode\n", __func__);
		goto done;
	}

	/* Boot in normal mode */
	ret = wait_for_completion_timeout(&cntl->boot_complete,
				msecs_to_jiffies(WCD_DSP_BOOT_TIMEOUT_MS));
	if (!ret) {
		dev_err(codec->dev, "%s: WDSP boot timed out\n",
			__func__);
		ret = -ETIMEDOUT;
		goto err_boot;
	}

	dev_dbg(codec->dev, "%s: WDSP booted in normal mode\n", __func__);

	/* Enable WDOG */
	snd_soc_update_bits(codec, WCD934X_CPE_SS_WDOG_CFG,
			    0x10, 0x10);
done:
	return ret;
err_boot:
	/* call shutdown to perform cleanup */
	wcd_cntl_do_shutdown(cntl);
	return ret;
}

static irqreturn_t wcd_cntl_ipc_irq(int irq, void *data)
{
	struct wcd_dsp_cntl *cntl = data;
	int ret;

	complete(&cntl->boot_complete);

	if (cntl->m_dev && cntl->m_ops &&
	    cntl->m_ops->intr_handler)
		ret = cntl->m_ops->intr_handler(cntl->m_dev, WDSP_IPC1_INTR);
	else
		ret = -EINVAL;

	if (IS_ERR_VALUE(ret))
		dev_err(cntl->codec->dev,
			"%s: Failed to handle irq %d\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t wcd_cntl_err_irq(int irq, void *data)
{
	struct wcd_dsp_cntl *cntl = data;
	struct snd_soc_codec *codec = cntl->codec;
	u16 status = 0;
	u8 reg_val;

	reg_val = snd_soc_read(codec, WCD934X_CPE_SS_SS_ERROR_INT_STATUS_0A);
	status = status | reg_val;

	reg_val = snd_soc_read(codec, WCD934X_CPE_SS_SS_ERROR_INT_STATUS_0B);
	status = status | (reg_val << 8);

	dev_info(codec->dev, "%s: error interrupt status = 0x%x\n",
		__func__, status);

	return IRQ_HANDLED;
}

static int wcd_control_handler(struct device *dev, void *priv_data,
			       enum wdsp_event_type event, void *data)
{
	struct wcd_dsp_cntl *cntl = priv_data;
	struct snd_soc_codec *codec = cntl->codec;
	int ret = 0;

	switch (event) {
	case WDSP_EVENT_POST_DLOAD_CODE:
	case WDSP_EVENT_DLOAD_FAILED:
	case WDSP_EVENT_POST_SHUTDOWN:

		/* Disable CPAR */
		wcd_cntl_cpar_ctrl(cntl, false);
		/* Disable all the clocks */
		ret = wcd_cntl_clocks_disable(cntl);
		if (IS_ERR_VALUE(ret))
			dev_err(codec->dev,
				"%s: Failed to disable clocks, err = %d\n",
				__func__, ret);
		break;

	case WDSP_EVENT_PRE_DLOAD_CODE:

		wcd_cntl_enable_memory(cntl);
		break;

	case WDSP_EVENT_PRE_DLOAD_DATA:

		/* Enable all the clocks */
		ret = wcd_cntl_clocks_enable(cntl);
		if (IS_ERR_VALUE(ret)) {
			dev_err(codec->dev,
				"%s: Failed to enable clocks, err = %d\n",
				__func__, ret);
			goto done;
		}

		/* Enable CPAR */
		wcd_cntl_cpar_ctrl(cntl, true);
		break;

	case WDSP_EVENT_DO_BOOT:

		ret = wcd_cntl_do_boot(cntl);
		if (IS_ERR_VALUE(ret))
			dev_err(codec->dev,
				"%s: WDSP boot failed, err = %d\n",
				__func__, ret);
		break;

	case WDSP_EVENT_DO_SHUTDOWN:

		wcd_cntl_do_shutdown(cntl);
		break;

	default:
		dev_dbg(codec->dev, "%s: unhandled event %d\n",
			__func__, event);
	}

done:
	return ret;
}

static int wcd_cntl_sysfs_init(char *dir, struct wcd_dsp_cntl *cntl)
{
	struct snd_soc_codec *codec = cntl->codec;
	int ret = 0;

	ret = kobject_init_and_add(&cntl->wcd_kobj, &wcd_cntl_ktype,
				   kernel_kobj, dir);
	if (IS_ERR_VALUE(ret)) {
		dev_err(codec->dev,
			"%s: Failed to add kobject %s, err = %d\n",
			__func__, dir, ret);
		goto done;
	}

	ret = sysfs_create_file(&cntl->wcd_kobj, &cntl_attr_boot.attr);
	if (IS_ERR_VALUE(ret)) {
		dev_err(codec->dev,
			"%s: Failed to add wdsp_boot sysfs entry to %s\n",
			__func__, dir);
		goto fail_create_file;
	}

	return ret;

fail_create_file:
	kobject_put(&cntl->wcd_kobj);
done:
	return ret;
}

static void wcd_cntl_sysfs_remove(struct wcd_dsp_cntl *cntl)
{
	sysfs_remove_file(&cntl->wcd_kobj, &cntl_attr_boot.attr);
	kobject_put(&cntl->wcd_kobj);
}

static void wcd_cntl_debugfs_init(char *dir, struct wcd_dsp_cntl *cntl)
{
	struct snd_soc_codec *codec = cntl->codec;

	cntl->entry = debugfs_create_dir(dir, NULL);
	if (IS_ERR_OR_NULL(dir)) {
		dev_err(codec->dev, "%s debugfs_create_dir failed for %s\n",
			__func__, dir);
		goto done;
	}

	debugfs_create_u32("debug_mode", S_IRUGO | S_IWUSR,
			   cntl->entry, &cntl->debug_mode);
done:
	return;
}

static void wcd_cntl_debugfs_remove(struct wcd_dsp_cntl *cntl)
{
	if (cntl)
		debugfs_remove(cntl->entry);
}

static int wcd_control_init(struct device *dev, void *priv_data)
{
	struct wcd_dsp_cntl *cntl = priv_data;
	struct snd_soc_codec *codec = cntl->codec;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	struct wcd9xxx_core_resource *core_res = &wcd9xxx->core_res;
	char wcd_cntl_dir_name[WCD_CNTL_DIR_NAME_LEN_MAX];
	int ret, ret1;
	bool err_irq_requested = false;

	ret = wcd9xxx_request_irq(core_res,
				  cntl->irqs.cpe_ipc1_irq,
				  wcd_cntl_ipc_irq, "CPE IPC1",
				  cntl);
	if (IS_ERR_VALUE(ret)) {
		dev_err(codec->dev,
			"%s: Failed to request cpe ipc irq, err = %d\n",
			__func__, ret);
		goto done;
	}

	/* Unmask the fatal irqs */
	snd_soc_write(codec, WCD934X_CPE_SS_SS_ERROR_INT_MASK_0A,
		      ~(cntl->irqs.fatal_irqs & 0xFF));
	snd_soc_write(codec, WCD934X_CPE_SS_SS_ERROR_INT_MASK_0B,
		      ~((cntl->irqs.fatal_irqs >> 8) & 0xFF));

	/*
	 * CPE ERR irq is used only for error reporting from WCD DSP,
	 * even if this request fails, DSP can be function normally.
	 * Continuing with init even if the CPE ERR irq request fails.
	 */
	if (wcd9xxx_request_irq(core_res, cntl->irqs.cpe_err_irq,
				wcd_cntl_err_irq, "CPE ERR", cntl))
		dev_info(codec->dev, "%s: Failed request_irq(cpe_err_irq)",
			__func__);
	else
		err_irq_requested = true;


	/* Enable all the clocks */
	ret = wcd_cntl_clocks_enable(cntl);
	if (IS_ERR_VALUE(ret)) {
		dev_err(codec->dev, "%s: Failed to enable clocks, err = %d\n",
			__func__, ret);
		goto err_clk_enable;
	}
	wcd_cntl_cpar_ctrl(cntl, true);

	snprintf(wcd_cntl_dir_name, WCD_CNTL_DIR_NAME_LEN_MAX,
		 "%s%d", "wdsp", cntl->dsp_instance);
	wcd_cntl_debugfs_init(wcd_cntl_dir_name, cntl);
	ret = wcd_cntl_sysfs_init(wcd_cntl_dir_name, cntl);
	if (IS_ERR_VALUE(ret)) {
		dev_err(codec->dev,
			"%s: Failed to init sysfs %d\n",
			__func__, ret);
		goto err_sysfs_init;
	}

	return 0;

err_sysfs_init:
	wcd_cntl_cpar_ctrl(cntl, false);
	ret1 = wcd_cntl_clocks_disable(cntl);
	if (IS_ERR_VALUE(ret1))
		dev_err(codec->dev, "%s: Failed to disable clocks, err = %d\n",
			__func__, ret1);
err_clk_enable:
	/* Mask all error interrupts */
	snd_soc_write(codec, WCD934X_CPE_SS_SS_ERROR_INT_MASK_0A, 0xFF);
	snd_soc_write(codec, WCD934X_CPE_SS_SS_ERROR_INT_MASK_0B, 0xFF);

	/* Free the irq's requested */
	wcd9xxx_free_irq(core_res, cntl->irqs.cpe_ipc1_irq, cntl);

	if (err_irq_requested)
		wcd9xxx_free_irq(core_res, cntl->irqs.cpe_err_irq, cntl);
done:
	return ret;
}

static int wcd_control_deinit(struct device *dev, void *priv_data)
{
	struct wcd_dsp_cntl *cntl = priv_data;
	struct snd_soc_codec *codec = cntl->codec;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	struct wcd9xxx_core_resource *core_res = &wcd9xxx->core_res;

	/* Remove the sysfs entries */
	wcd_cntl_sysfs_remove(cntl);

	/* Remove the debugfs entries */
	wcd_cntl_debugfs_remove(cntl);

	wcd_cntl_clocks_disable(cntl);
	wcd_cntl_cpar_ctrl(cntl, false);

	/* Mask all error interrupts */
	snd_soc_write(codec, WCD934X_CPE_SS_SS_ERROR_INT_MASK_0A, 0xFF);
	snd_soc_write(codec, WCD934X_CPE_SS_SS_ERROR_INT_MASK_0B, 0xFF);

	/* Free the irq's requested */
	wcd9xxx_free_irq(core_res, cntl->irqs.cpe_err_irq, cntl);
	wcd9xxx_free_irq(core_res, cntl->irqs.cpe_ipc1_irq, cntl);

	return 0;
}

static struct wdsp_cmpnt_ops control_ops = {
	.init = wcd_control_init,
	.deinit = wcd_control_deinit,
	.event_handler = wcd_control_handler,
};

static int wcd_ctrl_component_bind(struct device *dev,
				   struct device *master,
				   void *data)
{
	struct wcd_dsp_cntl *cntl;
	int ret = 0;

	if (!dev || !master || !data) {
		pr_err("%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}

	cntl = tavil_get_wcd_dsp_cntl(dev);
	if (!cntl) {
		dev_err(dev, "%s: Failed to get cntl reference\n",
			__func__);
		return -EINVAL;
	}

	cntl->m_dev = master;
	cntl->m_ops = data;

	if (cntl->m_ops->register_cmpnt_ops)
		ret = cntl->m_ops->register_cmpnt_ops(master, dev, cntl,
						      &control_ops);

	if (ret)
		dev_err(dev, "%s: register_cmpnt_ops failed, err = %d\n",
			__func__, ret);
	return ret;
}

static void wcd_ctrl_component_unbind(struct device *dev,
				      struct device *master,
				      void *data)
{
	struct wcd_dsp_cntl *cntl;

	if (!dev) {
		pr_err("%s: Invalid device\n", __func__);
		return;
	}

	cntl = tavil_get_wcd_dsp_cntl(dev);
	if (!cntl) {
		dev_err(dev, "%s: Failed to get cntl reference\n",
			__func__);
		return;
	}

	cntl->m_dev = NULL;
	cntl->m_ops = NULL;
}

static const struct component_ops wcd_ctrl_component_ops = {
	.bind = wcd_ctrl_component_bind,
	.unbind = wcd_ctrl_component_unbind,
};

/*
 * wcd_dsp_cntl_init: Initialize the wcd-dsp control
 * @codec: pointer to the codec handle
 * @params: Parameters required to initialize wcd-dsp control
 *
 * This API is expected to be invoked by the codec driver and
 * provide information essential for the wcd dsp control to
 * configure and initialize the dsp
 */
void wcd_dsp_cntl_init(struct snd_soc_codec *codec,
		       struct wcd_dsp_params *params,
		       struct wcd_dsp_cntl **cntl)
{
	struct wcd_dsp_cntl *control;
	int ret;

	if (!codec || !params) {
		pr_err("%s: Invalid handle to %s\n", __func__,
		       (!codec) ? "codec" : "params");
		*cntl = NULL;
		return;
	}

	if (*cntl) {
		pr_err("%s: cntl is non NULL, maybe already initialized ?\n",
			__func__);
		return;
	}

	control = kzalloc(sizeof(*control), GFP_KERNEL);
	if (!(control))
		return;

	control->codec = codec;
	control->clk_rate = params->clk_rate;
	control->cdc_cb = params->cb;
	control->dsp_instance = params->dsp_instance;
	memcpy(&control->irqs, &params->irqs, sizeof(control->irqs));
	init_completion(&control->boot_complete);
	mutex_init(&control->clk_mutex);

	/*
	 * If this is the last component needed by master to be ready,
	 * then component_bind will be called within the component_add.
	 * Hence, the data pointer should be assigned before component_add,
	 * so that we can access it during this component's bind call.
	 */
	*cntl = control;
	ret = component_add(codec->dev, &wcd_ctrl_component_ops);
	if (ret) {
		dev_err(codec->dev, "%s: component_add failed, err = %d\n",
			__func__, ret);
		kfree(*cntl);
		*cntl = NULL;
	}
}
EXPORT_SYMBOL(wcd_dsp_cntl_init);

/*
 * wcd_dsp_cntl_deinit: De-initialize the wcd-dsp control
 * @cntl: The struct wcd_dsp_cntl to de-initialize
 *
 * This API is intended to be invoked by the codec driver
 * to de-initialize the wcd dsp control
 */
void wcd_dsp_cntl_deinit(struct wcd_dsp_cntl **cntl)
{
	struct wcd_dsp_cntl *control = *cntl;
	struct snd_soc_codec *codec;

	/* If control is NULL, there is nothing to de-initialize */
	if (!control)
		return;
	codec = control->codec;

	/*
	 * Calling shutdown will cleanup all register states,
	 * irrespective of DSP was booted up or not.
	 */
	wcd_cntl_do_shutdown(control);
	wcd_cntl_disable_memory(control);

	component_del(codec->dev, &wcd_ctrl_component_ops);

	mutex_destroy(&control->clk_mutex);
	kfree(*cntl);
	*cntl = NULL;
}
EXPORT_SYMBOL(wcd_dsp_cntl_deinit);
