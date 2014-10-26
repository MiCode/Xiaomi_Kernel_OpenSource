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

#include <soc/qcom/ramdump.h>
#include <linux/dma-mapping.h>
#include "wcd_cpe_services.h"

#define WCD_CPE_LAB_MAX_LATENCY 250
#define WCD_CPE_MAD_SLIM_CHANNEL 140

/* Indicates CPE block is ready for image re-download */
#define WCD_CPE_BLK_READY  (1 << 0)
/* Indicates the underlying bus is ready */
#define WCD_CPE_BUS_READY (1 << 1)

/*
 * only when the underlying bus and CPE block both are ready,
 * the state will be ready to download
 */
#define WCD_CPE_READY_TO_DLOAD	\
	(WCD_CPE_BLK_READY | WCD_CPE_BUS_READY)

#define WCD_CPE_LOAD_IMEM (1 << 0)
#define WCD_CPE_LOAD_DATA (1 << 1)
#define WCD_CPE_LOAD_ALL \
	(WCD_CPE_LOAD_IMEM | WCD_CPE_LOAD_DATA)

enum {
	WCD_CPE_LSM_CAL_AFE = 0,
	WCD_CPE_LSM_CAL_LSM,
	WCD_CPE_LSM_CAL_MAX,
};

struct wcd_cpe_cdc_cb {
	/* codec provided callback to enable RCO */
	int (*cdc_clk_en) (struct snd_soc_codec *, bool);

	/* callback for FLL setup for codec */
	int (*cpe_clk_en) (struct snd_soc_codec *, bool);
	int (*cdc_ext_clk)(struct snd_soc_codec *codec, int enable, bool dapm);
	int (*lab_cdc_ch_ctl)(struct snd_soc_codec *codec, u8 event);
};

enum wcd_cpe_ssr_state_event {
	/* Indicates CPE is initialized */
	WCD_CPE_INITIALIZED = 0,
	/* Indicates that IMEM is downloaded to CPE */
	WCD_CPE_IMEM_DOWNLOADED,
	/* Indicates CPE is enabled */
	WCD_CPE_ENABLED,
	/* Indicates that CPE is currently active */
	WCD_CPE_ACTIVE,
	/* Event from underlying bus notifying bus is down */
	WCD_CPE_BUS_DOWN_EVENT,
	/* Event from CPE block, notifying CPE is down */
	WCD_CPE_SSR_EVENT,
	/* Event from underlying bus notifying bus is up */
	WCD_CPE_BUS_UP_EVENT,
};

struct wcd_cpe_ssr_entry {
	int offline;
	u32 offline_change;
	wait_queue_head_t offline_poll_wait;
	struct snd_info_entry *entry;
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
	const struct wcd_cpe_cdc_cb *cpe_cdc_cb;

	/* work to handle CPE SSR*/
	struct work_struct ssr_work;

	/* PM handle for suspend mode during SSR */
	struct pm_qos_request pm_qos_req;

	/* completion event indicating CPE OFFLINE */
	struct completion offline_compl;

	/* entry into snd card procfs indicating cpe status */
	struct wcd_cpe_ssr_entry ssr_entry;

	/*
	 * completion event to signal CPE is
	 * ready for image re-download
	 */
	struct completion ready_compl;

	/* maintains the status for cpe ssr */
	u8 ready_status;

	/* Indicate SSR type */
	enum wcd_cpe_ssr_state_event ssr_type;

	/* mutex to protect cpe ssr status variables */
	struct mutex ssr_lock;

	/* Store the calibration data needed for cpe */
	struct cal_type_data *cal_data[WCD_CPE_LSM_CAL_MAX];

	/* completion event to signal CPE is online */
	struct completion online_compl;

	/* reference counter for cpe usage */
	u8 cpe_users;

	/* Ramdump support */
	void *cpe_ramdump_dev;
	struct ramdump_segment cpe_ramdump_seg;
	dma_addr_t cpe_dump_addr;
	void *cpe_dump_v_addr;
};

struct wcd_cpe_params {
	struct snd_soc_codec *codec;
	struct wcd_cpe_core * (*get_cpe_core) (
				struct snd_soc_codec *);
	const struct wcd_cpe_cdc_cb *cdc_cb;
	int dbg_mode;
	u16 cdc_major_ver;
	u16 cdc_minor_ver;
	u32 cdc_id;
};

int wcd_cpe_ssr_event(void *core_handle,
		      enum wcd_cpe_ssr_state_event event);
struct wcd_cpe_core *wcd_cpe_init(const char *,
struct snd_soc_codec *, struct wcd_cpe_params *params);
