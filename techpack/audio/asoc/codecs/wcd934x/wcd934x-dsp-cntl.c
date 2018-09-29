/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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
#include <sound/soc.h>
#include <sound/wcd-dsp-mgr.h>
#include <asoc/wcd934x_registers.h>
#include "wcd934x.h"
#include "wcd934x-dsp-cntl.h"
#include "../wcd9xxx-irq.h"
#include "../core.h"

#define WCD_CNTL_DIR_NAME_LEN_MAX 32
#define WCD_CPE_FLL_MAX_RETRIES 5
#define WCD_MEM_ENABLE_MAX_RETRIES 20
#define WCD_DSP_BOOT_TIMEOUT_MS 3000
#define WCD_SYSFS_ENTRY_MAX_LEN 8
#define WCD_PROCFS_ENTRY_MAX_LEN 16
#define WCD_934X_RAMDUMP_START_ADDR 0x20100000
#define WCD_934X_RAMDUMP_SIZE ((1024 * 1024) - 128)
#define WCD_MISCDEV_CMD_MAX_LEN 11

#define WCD_CNTL_MUTEX_LOCK(codec, lock)             \
{                                                    \
	dev_dbg(codec->dev, "%s: mutex_lock(%s)\n",  \
		__func__, __stringify_1(lock));      \
	mutex_lock(&lock);                           \
}

#define WCD_CNTL_MUTEX_UNLOCK(codec, lock)            \
{                                                     \
	dev_dbg(codec->dev, "%s: mutex_unlock(%s)\n", \
		__func__, __stringify_1(lock));       \
	mutex_unlock(&lock);                          \
}

enum wcd_mem_type {
	WCD_MEM_TYPE_ALWAYS_ON,
	WCD_MEM_TYPE_SWITCHABLE,
};

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

#ifdef CONFIG_DEBUG_FS
#define WCD_CNTL_SET_ERR_IRQ_FLAG(cntl)\
	atomic_cmpxchg(&cntl->err_irq_flag, 0, 1)
#define WCD_CNTL_CLR_ERR_IRQ_FLAG(cntl)\
	atomic_set(&cntl->err_irq_flag, 0)

static u16 wdsp_reg_for_debug_dump[] = {
	WCD934X_CPE_SS_CPE_CTL,
	WCD934X_CPE_SS_PWR_SYS_PSTATE_CTL_0,
	WCD934X_CPE_SS_PWR_SYS_PSTATE_CTL_1,
	WCD934X_CPE_SS_PWR_CPEFLL_CTL,
	WCD934X_CPE_SS_PWR_CPE_SYSMEM_DEEPSLP_0,
	WCD934X_CPE_SS_PWR_CPE_SYSMEM_DEEPSLP_1,
	WCD934X_CPE_SS_PWR_CPE_SYSMEM_DEEPSLP_OVERRIDE,
	WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_0,
	WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_1,
	WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_2,
	WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_3,
	WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_4,
	WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_5,
	WCD934X_CPE_SS_PWR_CPE_DRAM1_SHUTDOWN,
	WCD934X_CPE_SS_SOC_SW_COLLAPSE_CTL,
	WCD934X_CPE_SS_MAD_CTL,
	WCD934X_CPE_SS_CPAR_CTL,
	WCD934X_CPE_SS_CPAR_CFG,
	WCD934X_CPE_SS_WDOG_CFG,
	WCD934X_CPE_SS_STATUS,
	WCD934X_CPE_SS_SS_ERROR_INT_MASK_0A,
	WCD934X_CPE_SS_SS_ERROR_INT_MASK_0B,
	WCD934X_CPE_SS_SS_ERROR_INT_MASK_1A,
	WCD934X_CPE_SS_SS_ERROR_INT_MASK_1B,
	WCD934X_CPE_SS_SS_ERROR_INT_STATUS_0A,
	WCD934X_CPE_SS_SS_ERROR_INT_STATUS_0B,
	WCD934X_CPE_SS_SS_ERROR_INT_STATUS_1A,
	WCD934X_CPE_SS_SS_ERROR_INT_STATUS_1B,
};

static void wcd_cntl_collect_debug_dumps(struct wcd_dsp_cntl *cntl,
					 bool internal)
{
	struct snd_soc_codec *codec = cntl->codec;
	struct wdsp_err_signal_arg arg;
	enum wdsp_signal signal;
	int i;
	u8 val;

	/* If WDSP SSR happens, skip collecting debug dumps */
	if (WCD_CNTL_SET_ERR_IRQ_FLAG(cntl) != 0)
		return;

	/* Mask all error interrupts */
	snd_soc_write(codec, WCD934X_CPE_SS_SS_ERROR_INT_MASK_0A,
		      0xFF);
	snd_soc_write(codec, WCD934X_CPE_SS_SS_ERROR_INT_MASK_0B,
		      0xFF);

	/* Collect important WDSP registers dump for debug use */
	pr_err("%s: Dump the WDSP registers for debug use\n", __func__);
	for (i = 0; i < sizeof(wdsp_reg_for_debug_dump)/sizeof(u16); i++) {
		val = snd_soc_read(codec, wdsp_reg_for_debug_dump[i]);
		pr_err("%s: reg = 0x%x, val = 0x%x\n", __func__,
		       wdsp_reg_for_debug_dump[i], val);
	}

	/* Trigger NMI in WDSP to sync and update the memory */
	snd_soc_write(codec, WCD934X_CPE_SS_BACKUP_INT, 0x02);

	/* Collect WDSP ramdump for debug use */
	if (cntl->m_dev && cntl->m_ops && cntl->m_ops->signal_handler) {
		arg.mem_dumps_enabled = cntl->ramdump_enable;
		arg.remote_start_addr = WCD_934X_RAMDUMP_START_ADDR;
		arg.dump_size = WCD_934X_RAMDUMP_SIZE;
		signal = internal ? WDSP_DEBUG_DUMP_INTERNAL : WDSP_DEBUG_DUMP;
		cntl->m_ops->signal_handler(cntl->m_dev, signal, &arg);
	}

	/* Unmask the fatal irqs */
	snd_soc_write(codec, WCD934X_CPE_SS_SS_ERROR_INT_MASK_0A,
		      ~(cntl->irqs.fatal_irqs & 0xFF));
	snd_soc_write(codec, WCD934X_CPE_SS_SS_ERROR_INT_MASK_0B,
		      ~((cntl->irqs.fatal_irqs >> 8) & 0xFF));

	WCD_CNTL_CLR_ERR_IRQ_FLAG(cntl);
}
#else
#define WCD_CNTL_SET_ERR_IRQ_FLAG(cntl) 0
#define WCD_CNTL_CLR_ERR_IRQ_FLAG(cntl) do {} while (0)
static void wcd_cntl_collect_debug_dumps(struct wcd_dsp_cntl *cntl,
					 bool internal)
{
}
#endif

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

	if (ret < 0)
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

static void wcd_cntl_change_online_state(struct wcd_dsp_cntl *cntl,
					 u8 online)
{
	struct wdsp_ssr_entry *ssr_entry = &cntl->ssr_entry;
	unsigned long ret;

	WCD_CNTL_MUTEX_LOCK(cntl->codec, cntl->ssr_mutex);
	ssr_entry->offline = !online;
	/* Make sure the write is complete */
	wmb();
	ret = xchg(&ssr_entry->offline_change, 1);
	wake_up_interruptible(&ssr_entry->offline_poll_wait);
	dev_dbg(cntl->codec->dev,
		"%s: requested %u, offline %u offline_change %u, ret = %ldn",
		__func__, online, ssr_entry->offline,
		ssr_entry->offline_change, ret);
	WCD_CNTL_MUTEX_UNLOCK(cntl->codec, cntl->ssr_mutex);
}

static ssize_t wdsp_ssr_entry_read(struct snd_info_entry *entry,
				   void *file_priv_data, struct file *file,
				   char __user *buf, size_t count, loff_t pos)
{
	int len = 0;
	char buffer[WCD_PROCFS_ENTRY_MAX_LEN];
	struct wcd_dsp_cntl *cntl;
	struct wdsp_ssr_entry *ssr_entry;
	ssize_t ret;
	u8 offline;

	cntl = (struct wcd_dsp_cntl *) entry->private_data;
	if (!cntl) {
		pr_err("%s: Invalid private data for SSR procfs entry\n",
		       __func__);
		return -EINVAL;
	}

	ssr_entry = &cntl->ssr_entry;

	WCD_CNTL_MUTEX_LOCK(cntl->codec, cntl->ssr_mutex);
	offline = ssr_entry->offline;
	/* Make sure the read is complete */
	rmb();
	dev_dbg(cntl->codec->dev, "%s: offline = %s\n", __func__,
		offline ? "true" : "false");
	len = snprintf(buffer, sizeof(buffer), "%s\n",
		       offline ? "OFFLINE" : "ONLINE");
	ret = simple_read_from_buffer(buf, count, &pos, buffer, len);
	WCD_CNTL_MUTEX_UNLOCK(cntl->codec, cntl->ssr_mutex);

	return ret;
}

static unsigned int wdsp_ssr_entry_poll(struct snd_info_entry *entry,
					void *private_data, struct file *file,
					poll_table *wait)
{
	struct wcd_dsp_cntl *cntl;
	struct wdsp_ssr_entry *ssr_entry;
	unsigned int ret = 0;

	if (!entry || !entry->private_data) {
		pr_err("%s: %s is NULL\n", __func__,
		       (!entry) ? "entry" : "private_data");
		return -EINVAL;
	}

	cntl = (struct wcd_dsp_cntl *) entry->private_data;
	ssr_entry = &cntl->ssr_entry;

	dev_dbg(cntl->codec->dev, "%s: Poll wait, offline = %u\n",
		__func__, ssr_entry->offline);
	poll_wait(file, &ssr_entry->offline_poll_wait, wait);
	dev_dbg(cntl->codec->dev, "%s: Woken up Poll wait, offline = %u\n",
		__func__, ssr_entry->offline);

	WCD_CNTL_MUTEX_LOCK(cntl->codec, cntl->ssr_mutex);
	if (xchg(&ssr_entry->offline_change, 0))
		ret = POLLIN | POLLPRI | POLLRDNORM;
	dev_dbg(cntl->codec->dev, "%s: ret (%d) from poll_wait\n",
		__func__, ret);
	WCD_CNTL_MUTEX_UNLOCK(cntl->codec, cntl->ssr_mutex);

	return ret;
}

static struct snd_info_entry_ops wdsp_ssr_entry_ops = {
	.read = wdsp_ssr_entry_read,
	.poll = wdsp_ssr_entry_poll,
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
		if (ret < 0) {
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

	if (ret < 0) {
		dev_err(codec->dev,
			"%s: Failed to enable cdc clk, err = %d\n",
			__func__, ret);
		goto done;
	}
	/* Pull CPAR out of reset */
	snd_soc_update_bits(codec, WCD934X_CPE_SS_CPAR_CTL, 0x04, 0x00);

	/* Configure and Enable CPE FLL clock */
	ret = wcd_cntl_cpe_fll_ctrl(cntl, true);
	if (ret < 0) {
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

	snd_soc_update_bits(codec, WCD934X_CPE_SS_CPAR_CTL, 0x04, 0x04);
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
	if (ret < 0)
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

	/* Put CPAR in reset */
	snd_soc_update_bits(codec, WCD934X_CPE_SS_CPAR_CTL, 0x04, 0x04);
done:
	WCD_CNTL_MUTEX_UNLOCK(codec, cntl->clk_mutex);
	return ret;
}

static void wcd_cntl_cpar_ctrl(struct wcd_dsp_cntl *cntl,
			       bool enable)
{
	struct snd_soc_codec *codec = cntl->codec;

	if (enable)
		snd_soc_update_bits(codec, WCD934X_CPE_SS_CPAR_CTL, 0x03, 0x03);
	else
		snd_soc_update_bits(codec, WCD934X_CPE_SS_CPAR_CTL, 0x03, 0x00);
}

static int wcd_cntl_enable_memory(struct wcd_dsp_cntl *cntl,
				  enum wcd_mem_type mem_type)
{
	struct snd_soc_codec *codec = cntl->codec;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	int loop_cnt = 0;
	u8 status;
	int ret = 0;


	switch (mem_type) {

	case WCD_MEM_TYPE_ALWAYS_ON:

		/* 512KB of always on region */
		wcd9xxx_slim_write_repeat(wcd9xxx,
				WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_0,
				ARRAY_SIZE(mem_enable_values),
				mem_enable_values);
		wcd9xxx_slim_write_repeat(wcd9xxx,
				WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_1,
				ARRAY_SIZE(mem_enable_values),
				mem_enable_values);
		break;

	case WCD_MEM_TYPE_SWITCHABLE:

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

		/* Rest of the memory */
		wcd9xxx_slim_write_repeat(wcd9xxx,
				WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_2,
				ARRAY_SIZE(mem_enable_values),
				mem_enable_values);
		wcd9xxx_slim_write_repeat(wcd9xxx,
				WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_3,
				ARRAY_SIZE(mem_enable_values),
				mem_enable_values);

		snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_DRAM1_SHUTDOWN,
			      0x05);
		break;

	default:
		dev_err(cntl->codec->dev, "%s: Invalid mem_type %d\n",
			__func__, mem_type);
		ret = -EINVAL;
		break;
	}
done:
	/* Make sure Deep sleep of memories is enabled for all banks */
	snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_DEEPSLP_0, 0xFF);
	snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_DEEPSLP_1, 0x0F);

	return ret;
}

static void wcd_cntl_disable_memory(struct wcd_dsp_cntl *cntl,
				    enum wcd_mem_type mem_type)
{
	struct snd_soc_codec *codec = cntl->codec;
	u8 val;

	switch (mem_type) {
	case WCD_MEM_TYPE_ALWAYS_ON:
		snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_1,
			      0xFF);
		snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_0,
			      0xFF);
		break;
	case WCD_MEM_TYPE_SWITCHABLE:
		snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_3,
			      0xFF);
		snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_SHUTDOWN_2,
			      0xFF);
		snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_DRAM1_SHUTDOWN,
			      0x07);

		snd_soc_update_bits(codec, WCD934X_CPE_SS_SOC_SW_COLLAPSE_CTL,
				    0x01, 0x00);
		val = snd_soc_read(codec, WCD934X_CPE_SS_SOC_SW_COLLAPSE_CTL);
		if (val & 0x02)
			dev_err(codec->dev,
				"%s: Disable switchable failed, val = 0x%02x",
				__func__, val);

		snd_soc_update_bits(codec, WCD934X_TEST_DEBUG_MEM_CTRL,
				    0x80, 0x00);
		break;
	default:
		dev_err(cntl->codec->dev, "%s: Invalid mem_type %d\n",
			__func__, mem_type);
		break;
	}

	snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_DEEPSLP_0, 0xFF);
	snd_soc_write(codec, WCD934X_CPE_SS_PWR_CPE_SYSMEM_DEEPSLP_1, 0x0F);
}

static void wcd_cntl_do_shutdown(struct wcd_dsp_cntl *cntl)
{
	struct snd_soc_codec *codec = cntl->codec;

	/* Disable WDOG */
	snd_soc_update_bits(codec, WCD934X_CPE_SS_WDOG_CFG,
			    0x3F, 0x01);

	/* Put WDSP in reset state */
	snd_soc_update_bits(codec, WCD934X_CPE_SS_CPE_CTL,
			    0x02, 0x00);

	/* If DSP transitions from boot to shutdown, then vote for SVS */
	if (cntl->is_wdsp_booted)
		cntl->cdc_cb->cdc_vote_svs(codec, true);
	cntl->is_wdsp_booted = false;
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
	} else {
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
		cntl->is_wdsp_booted = true;
		goto done;
	}

	/* Boot in normal mode */
	ret = wait_for_completion_timeout(&cntl->boot_complete,
				msecs_to_jiffies(WCD_DSP_BOOT_TIMEOUT_MS));
	if (!ret) {
		dev_err(codec->dev, "%s: WDSP boot timed out\n",
			__func__);
		wcd_cntl_collect_debug_dumps(cntl, true);
		ret = -ETIMEDOUT;
		goto err_boot;
	} else {
		/*
		 * Re-initialize the return code to 0, as in success case,
		 * it will hold the remaining time for completion timeout
		 */
		ret = 0;
	}

	dev_dbg(codec->dev, "%s: WDSP booted in normal mode\n", __func__);
	cntl->is_wdsp_booted = true;

	/* Enable WDOG */
	snd_soc_update_bits(codec, WCD934X_CPE_SS_WDOG_CFG,
			    0x10, 0x10);
done:
	/* If dsp booted up, then remove vote on SVS */
	if (cntl->is_wdsp_booted)
		cntl->cdc_cb->cdc_vote_svs(codec, false);

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
	    cntl->m_ops->signal_handler)
		ret = cntl->m_ops->signal_handler(cntl->m_dev, WDSP_IPC1_INTR,
						  NULL);
	else
		ret = -EINVAL;

	if (ret < 0)
		dev_err(cntl->codec->dev,
			"%s: Failed to handle irq %d\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t wcd_cntl_err_irq(int irq, void *data)
{
	struct wcd_dsp_cntl *cntl = data;
	struct snd_soc_codec *codec = cntl->codec;
	struct wdsp_err_signal_arg arg;
	u16 status = 0;
	u8 reg_val;
	int rc, ret = 0;

	reg_val = snd_soc_read(codec, WCD934X_CPE_SS_SS_ERROR_INT_STATUS_0A);
	status = status | reg_val;

	reg_val = snd_soc_read(codec, WCD934X_CPE_SS_SS_ERROR_INT_STATUS_0B);
	status = status | (reg_val << 8);

	dev_info(codec->dev, "%s: error interrupt status = 0x%x\n",
		__func__, status);

	if ((status & cntl->irqs.fatal_irqs) &&
	    (cntl->m_dev && cntl->m_ops && cntl->m_ops->signal_handler)) {
		/*
		 * If WDSP SSR happens, skip collecting debug dumps.
		 * If debug dumps collecting happens first, WDSP_ERR_INTR
		 * will be blocked in signal_handler and get processed later.
		 */
		rc = WCD_CNTL_SET_ERR_IRQ_FLAG(cntl);
		arg.mem_dumps_enabled = cntl->ramdump_enable;
		arg.remote_start_addr = WCD_934X_RAMDUMP_START_ADDR;
		arg.dump_size = WCD_934X_RAMDUMP_SIZE;
		ret = cntl->m_ops->signal_handler(cntl->m_dev, WDSP_ERR_INTR,
						  &arg);
		if (ret < 0)
			dev_err(cntl->codec->dev,
				"%s: Failed to handle fatal irq 0x%x\n",
				__func__, status & cntl->irqs.fatal_irqs);
		wcd_cntl_change_online_state(cntl, 0);
		if (rc == 0)
			WCD_CNTL_CLR_ERR_IRQ_FLAG(cntl);
	} else {
		dev_err(cntl->codec->dev, "%s: Invalid signal_handler\n",
			__func__);
	}

	return IRQ_HANDLED;
}

static int wcd_control_handler(struct device *dev, void *priv_data,
			       enum wdsp_event_type event, void *data)
{
	struct wcd_dsp_cntl *cntl = priv_data;
	struct snd_soc_codec *codec = cntl->codec;
	int ret = 0;

	switch (event) {
	case WDSP_EVENT_POST_INIT:
	case WDSP_EVENT_POST_DLOAD_CODE:
	case WDSP_EVENT_DLOAD_FAILED:
	case WDSP_EVENT_POST_SHUTDOWN:

		/* Disable CPAR */
		wcd_cntl_cpar_ctrl(cntl, false);
		/* Disable all the clocks */
		ret = wcd_cntl_clocks_disable(cntl);
		if (ret < 0)
			dev_err(codec->dev,
				"%s: Failed to disable clocks, err = %d\n",
				__func__, ret);

		if (event == WDSP_EVENT_POST_DLOAD_CODE)
			/* Mark DSP online since code download is complete */
			wcd_cntl_change_online_state(cntl, 1);
		break;

	case WDSP_EVENT_PRE_DLOAD_DATA:
	case WDSP_EVENT_PRE_DLOAD_CODE:

		/* Enable all the clocks */
		ret = wcd_cntl_clocks_enable(cntl);
		if (ret < 0) {
			dev_err(codec->dev,
				"%s: Failed to enable clocks, err = %d\n",
				__func__, ret);
			goto done;
		}

		/* Enable CPAR */
		wcd_cntl_cpar_ctrl(cntl, true);

		if (event == WDSP_EVENT_PRE_DLOAD_CODE)
			wcd_cntl_enable_memory(cntl, WCD_MEM_TYPE_ALWAYS_ON);
		else if (event == WDSP_EVENT_PRE_DLOAD_DATA)
			wcd_cntl_enable_memory(cntl, WCD_MEM_TYPE_SWITCHABLE);
		break;

	case WDSP_EVENT_DO_BOOT:

		ret = wcd_cntl_do_boot(cntl);
		if (ret < 0)
			dev_err(codec->dev,
				"%s: WDSP boot failed, err = %d\n",
				__func__, ret);
		break;

	case WDSP_EVENT_DO_SHUTDOWN:

		wcd_cntl_do_shutdown(cntl);
		wcd_cntl_disable_memory(cntl, WCD_MEM_TYPE_SWITCHABLE);
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
	if (ret < 0) {
		dev_err(codec->dev,
			"%s: Failed to add kobject %s, err = %d\n",
			__func__, dir, ret);
		goto done;
	}

	ret = sysfs_create_file(&cntl->wcd_kobj, &cntl_attr_boot.attr);
	if (ret < 0) {
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

	debugfs_create_u32("debug_mode", 0644,
			   cntl->entry, &cntl->debug_mode);
	debugfs_create_bool("ramdump_enable", 0644,
			    cntl->entry, &cntl->ramdump_enable);
done:
	return;
}

static void wcd_cntl_debugfs_remove(struct wcd_dsp_cntl *cntl)
{
	if (cntl)
		debugfs_remove(cntl->entry);
}

static int wcd_miscdev_release(struct inode *inode, struct file *filep)
{
	struct wcd_dsp_cntl *cntl = container_of(filep->private_data,
						 struct wcd_dsp_cntl, miscdev);
	if (!cntl->m_dev || !cntl->m_ops ||
	    !cntl->m_ops->vote_for_dsp) {
		dev_err(cntl->codec->dev,
			"%s: DSP not ready to boot\n", __func__);
		return -EINVAL;
	}

	/* Make sure the DSP users goes to zero upon closing dev node */
	while (cntl->boot_reqs > 0) {
		cntl->m_ops->vote_for_dsp(cntl->m_dev, false);
		cntl->boot_reqs--;
	}

	return 0;
}

static ssize_t wcd_miscdev_write(struct file *filep, const char __user *ubuf,
				 size_t count, loff_t *pos)
{
	struct wcd_dsp_cntl *cntl = container_of(filep->private_data,
						 struct wcd_dsp_cntl, miscdev);
	char val[WCD_MISCDEV_CMD_MAX_LEN];
	bool vote;
	int ret = 0;

	if (count == 0 || count > WCD_MISCDEV_CMD_MAX_LEN) {
		pr_err("%s: Invalid count = %zd\n", __func__, count);
		ret = -EINVAL;
		goto done;
	}

	ret = copy_from_user(val, ubuf, count);
	if (ret < 0) {
		dev_err(cntl->codec->dev,
			"%s: copy_from_user failed, err = %d\n",
			__func__, ret);
		ret = -EFAULT;
		goto done;
	}

	if (val[0] == '1') {
		cntl->boot_reqs++;
		vote = true;
	} else if (val[0] == '0') {
		if (cntl->boot_reqs == 0) {
			dev_err(cntl->codec->dev,
				"%s: WDSP already disabled\n", __func__);
			ret = -EINVAL;
			goto done;
		}
		cntl->boot_reqs--;
		vote = false;
	} else if (!strcmp(val, "DEBUG_DUMP")) {
		dev_dbg(cntl->codec->dev,
			"%s: Collect dumps for debug use\n", __func__);
		wcd_cntl_collect_debug_dumps(cntl, false);
		goto done;
	} else {
		dev_err(cntl->codec->dev, "%s: Invalid value %s\n",
			__func__, val);
		ret = -EINVAL;
		goto done;
	}

	dev_dbg(cntl->codec->dev,
		"%s: booted = %s, ref_cnt = %d, vote = %s\n",
		__func__, cntl->is_wdsp_booted ? "true" : "false",
		cntl->boot_reqs, vote ? "true" : "false");

	if (cntl->m_dev && cntl->m_ops &&
	    cntl->m_ops->vote_for_dsp)
		ret = cntl->m_ops->vote_for_dsp(cntl->m_dev, vote);
	else
		ret = -EINVAL;
done:
	if (ret)
		return ret;
	else
		return count;
}

static const struct file_operations wcd_miscdev_fops = {
	.write = wcd_miscdev_write,
	.release = wcd_miscdev_release,
};

static int wcd_cntl_miscdev_create(struct wcd_dsp_cntl *cntl)
{
	snprintf(cntl->miscdev_name, ARRAY_SIZE(cntl->miscdev_name),
		"wcd_dsp%u_control", cntl->dsp_instance);
	cntl->miscdev.minor = MISC_DYNAMIC_MINOR;
	cntl->miscdev.name = cntl->miscdev_name;
	cntl->miscdev.fops = &wcd_miscdev_fops;
	cntl->miscdev.parent = cntl->codec->dev;

	return misc_register(&cntl->miscdev);
}

static void wcd_cntl_miscdev_destroy(struct wcd_dsp_cntl *cntl)
{
	misc_deregister(&cntl->miscdev);
}

static int wcd_control_init(struct device *dev, void *priv_data)
{
	struct wcd_dsp_cntl *cntl = priv_data;
	struct snd_soc_codec *codec = cntl->codec;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	struct wcd9xxx_core_resource *core_res = &wcd9xxx->core_res;
	int ret;
	bool err_irq_requested = false;

	ret = wcd9xxx_request_irq(core_res,
				  cntl->irqs.cpe_ipc1_irq,
				  wcd_cntl_ipc_irq, "CPE IPC1",
				  cntl);
	if (ret < 0) {
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
	if (ret < 0) {
		dev_err(codec->dev, "%s: Failed to enable clocks, err = %d\n",
			__func__, ret);
		goto err_clk_enable;
	}
	wcd_cntl_cpar_ctrl(cntl, true);

	return 0;

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
	struct snd_soc_codec *codec;
	struct snd_card *card;
	struct snd_info_entry *entry;
	char proc_name[WCD_PROCFS_ENTRY_MAX_LEN];
	char wcd_cntl_dir_name[WCD_CNTL_DIR_NAME_LEN_MAX];
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

	if (!cntl->m_ops->register_cmpnt_ops) {
		dev_err(dev, "%s: invalid master callback register_cmpnt_ops\n",
			__func__);
		ret = -EINVAL;
		goto done;
	}

	ret = cntl->m_ops->register_cmpnt_ops(master, dev, cntl, &control_ops);
	if (ret) {
		dev_err(dev, "%s: register_cmpnt_ops failed, err = %d\n",
			__func__, ret);
		goto done;
	}

	ret = wcd_cntl_miscdev_create(cntl);
	if (ret < 0) {
		dev_err(dev, "%s: misc dev register failed, err = %d\n",
			__func__, ret);
		goto done;
	}

	snprintf(wcd_cntl_dir_name, WCD_CNTL_DIR_NAME_LEN_MAX,
		 "%s%d", "wdsp", cntl->dsp_instance);
	ret = wcd_cntl_sysfs_init(wcd_cntl_dir_name, cntl);
	if (ret < 0) {
		dev_err(dev, "%s: sysfs_init failed, err = %d\n",
			__func__, ret);
		goto err_sysfs_init;
	}

	wcd_cntl_debugfs_init(wcd_cntl_dir_name, cntl);

	codec = cntl->codec;
	card = codec->component.card->snd_card;
	snprintf(proc_name, WCD_PROCFS_ENTRY_MAX_LEN, "%s%d%s", "cpe",
		 cntl->dsp_instance, "_state");
	entry = snd_info_create_card_entry(card, proc_name, card->proc_root);
	if (!entry) {
		/* Do not treat this as Fatal error */
		dev_err(dev, "%s: Failed to create procfs entry %s\n",
			__func__, proc_name);
		goto err_sysfs_init;
	}

	cntl->ssr_entry.entry = entry;
	cntl->ssr_entry.offline = 1;
	entry->size = WCD_PROCFS_ENTRY_MAX_LEN;
	entry->content = SNDRV_INFO_CONTENT_DATA;
	entry->c.ops = &wdsp_ssr_entry_ops;
	entry->private_data = cntl;
	ret = snd_info_register(entry);
	if (ret < 0) {
		dev_err(dev, "%s: Failed to register entry %s, err = %d\n",
			__func__, proc_name, ret);
		snd_info_free_entry(entry);
		/* Let bind still happen even if creating the entry failed */
		ret = 0;
	}
done:
	return ret;

err_sysfs_init:
	wcd_cntl_miscdev_destroy(cntl);
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

	/* Remove the sysfs entries */
	wcd_cntl_sysfs_remove(cntl);

	/* Remove the debugfs entries */
	wcd_cntl_debugfs_remove(cntl);

	/* Remove the misc device */
	wcd_cntl_miscdev_destroy(cntl);
}

static const struct component_ops wcd_ctrl_component_ops = {
	.bind = wcd_ctrl_component_bind,
	.unbind = wcd_ctrl_component_unbind,
};

/*
 * wcd_dsp_ssr_event: handle the SSR event raised by caller.
 * @cntl: Handle to the wcd_dsp_cntl structure
 * @event: The SSR event to be handled
 *
 * Notifies the manager driver about the SSR event.
 * Returns 0 on success and negative error code on error.
 */
int wcd_dsp_ssr_event(struct wcd_dsp_cntl *cntl, enum cdc_ssr_event event)
{
	int ret = 0;

	if (!cntl) {
		pr_err("%s: Invalid handle to control\n", __func__);
		return -EINVAL;
	}

	if (!cntl->m_dev || !cntl->m_ops || !cntl->m_ops->signal_handler) {
		dev_err(cntl->codec->dev,
			"%s: Invalid signal_handler callback\n", __func__);
		return -EINVAL;
	}

	switch (event) {
	case WCD_CDC_DOWN_EVENT:
		ret = cntl->m_ops->signal_handler(cntl->m_dev,
						  WDSP_CDC_DOWN_SIGNAL,
						  NULL);
		if (ret < 0)
			dev_err(cntl->codec->dev,
				"%s: WDSP_CDC_DOWN_SIGNAL failed, err = %d\n",
				__func__, ret);
		wcd_cntl_change_online_state(cntl, 0);
		break;
	case WCD_CDC_UP_EVENT:
		ret = cntl->m_ops->signal_handler(cntl->m_dev,
						  WDSP_CDC_UP_SIGNAL,
						  NULL);
		if (ret < 0)
			dev_err(cntl->codec->dev,
				"%s: WDSP_CDC_UP_SIGNAL failed, err = %d\n",
				__func__, ret);
		break;
	default:
		dev_err(cntl->codec->dev, "%s: Invalid event %d\n",
			__func__, event);
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(wcd_dsp_ssr_event);

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

	if (!params->cb || !params->cb->cdc_clk_en ||
	    !params->cb->cdc_vote_svs) {
		dev_err(codec->dev,
			"%s: clk_en and vote_svs callbacks must be provided\n",
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
	mutex_init(&control->ssr_mutex);
	init_waitqueue_head(&control->ssr_entry.offline_poll_wait);
	WCD_CNTL_CLR_ERR_IRQ_FLAG(control);

	/*
	 * The default state of WDSP is in SVS mode.
	 * Vote for SVS now, the vote will be removed only
	 * after DSP is booted up.
	 */
	control->cdc_cb->cdc_vote_svs(codec, true);

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
	wcd_cntl_disable_memory(control, WCD_MEM_TYPE_SWITCHABLE);
	wcd_cntl_disable_memory(control, WCD_MEM_TYPE_ALWAYS_ON);

	component_del(codec->dev, &wcd_ctrl_component_ops);

	mutex_destroy(&control->clk_mutex);
	mutex_destroy(&control->ssr_mutex);
	kfree(*cntl);
	*cntl = NULL;
}
EXPORT_SYMBOL(wcd_dsp_cntl_deinit);
