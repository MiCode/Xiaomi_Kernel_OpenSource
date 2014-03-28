/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include "wcd_cpe_services.h"

struct wcd_cpe_cdc_cb {
	/* codec provided callback to enable RCO */
	int (*cdc_clk_en) (struct snd_soc_codec *, bool);

	/* callback for FLL setup for codec */
	int (*cpe_clk_en) (struct snd_soc_codec *, bool);
};

struct wcd_cpe_core {
	/* handle to cpe services */
	void *cpe_handle;

	/* registration handle to cpe services */
	void *cpe_reg_handle;

	/* cmi registration handle for afe service */
	void *cmi_afe_handle;

	/* handle to codec */
	struct snd_soc_codec *codec;

	/* codec device */
	struct device *dev;

	/* firmware image file name */
	char fname[64];

	/* codec information needed by cpe services */
	struct cpe_svc_codec_info_v1 cdc_info;

	/* work to perform image download */
	struct work_struct load_fw_work;

	/* flag to indicate mode in which cpe needs to be booted */
	int cpe_debug_mode;

	/* callbacks for codec specific implementation */
	struct wcd_cpe_cdc_cb cpe_cdc_cb;
};

struct wcd_cpe_params {
	struct snd_soc_codec *codec;
	struct wcd_cpe_core * (*get_cpe_core) (
				struct snd_soc_codec *);
	struct wcd_cpe_cdc_cb *cdc_cb;
	int dbg_mode;
	u16 cdc_major_ver;
	u16 cdc_minor_ver;
	u32 cdc_id;
};

struct wcd_cpe_core *wcd_cpe_init_and_boot(const char *,
	struct snd_soc_codec *, struct wcd_cpe_params *params);
