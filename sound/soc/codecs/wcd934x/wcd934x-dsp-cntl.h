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

#ifndef __WCD934X_DSP_CNTL_H__
#define __WCD934X_DSP_CNTL_H__

#include <sound/soc.h>
#include <sound/wcd-dsp-mgr.h>

struct wcd_dsp_cdc_cb {
	/* Callback to enable codec clock */
	int (*cdc_clk_en)(struct snd_soc_codec *, bool);
};

struct wcd_dsp_irq_info {
	/* IPC interrupt */
	int cpe_ipc1_irq;

	/* CPE error summary interrupt */
	int cpe_err_irq;

	/*
	 * Bit mask to indicate which of the
	 * error interrupts are to be considered
	 * as fatal.
	 */
	u16 fatal_irqs;
};

struct wcd_dsp_params {
	struct wcd_dsp_cdc_cb *cb;
	struct wcd_dsp_irq_info irqs;

	/* Rate at which the codec clock operates */
	u32 clk_rate;

	/*
	 * Represents the dsp instance, will be used
	 * to create sysfs and debugfs entries with
	 * directory wdsp<dsp-instance>
	 */
	u32 dsp_instance;
};

struct wcd_dsp_cntl {
	/* Handle to codec */
	struct snd_soc_codec *codec;

	/* Clk rate of the codec clock */
	u32 clk_rate;

	/* Callbacks to codec driver */
	const struct wcd_dsp_cdc_cb *cdc_cb;

	/* Completion to indicate WDSP boot done */
	struct completion boot_complete;

	struct wcd_dsp_irq_info irqs;
	u32 dsp_instance;

	/* Sysfs entries related */
	int boot_reqs;
	struct kobject wcd_kobj;

	/* Debugfs related */
	struct dentry *entry;
	u32 debug_mode;

	/* WDSP manager drivers data */
	struct device *m_dev;
	struct wdsp_mgr_ops *m_ops;

	/* clk related */
	struct mutex clk_mutex;
	bool is_clk_enabled;
};

void wcd_dsp_cntl_init(struct snd_soc_codec *codec,
		       struct wcd_dsp_params *params,
		       struct wcd_dsp_cntl **cntl);
void wcd_dsp_cntl_deinit(struct wcd_dsp_cntl **cntl);

#endif /* end __WCD_DSP_CONTROL_H__ */
