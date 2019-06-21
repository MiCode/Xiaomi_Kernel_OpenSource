/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stringify.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/component.h>
#include <linux/dma-mapping.h>
#include <soc/qcom/ramdump.h>
#include <sound/wcd-dsp-mgr.h>
#include "wcd-dsp-utils.h"

/* Forward declarations */
static char *wdsp_get_cmpnt_type_string(enum wdsp_cmpnt_type);

/* Component related macros */
#define WDSP_GET_COMPONENT(wdsp, x) ((x >= WDSP_CMPNT_TYPE_MAX || x < 0) ? \
					NULL : (&(wdsp->cmpnts[x])))
#define WDSP_GET_CMPNT_TYPE_STR(x) wdsp_get_cmpnt_type_string(x)

/*
 * These #defines indicate the bit number in status field
 * for each of the status. If bit is set, it indicates
 * the status as done, else if bit is not set, it indicates
 * the status is either failed or not done.
 */
#define WDSP_STATUS_INITIALIZED   BIT(0)
#define WDSP_STATUS_CODE_DLOADED  BIT(1)
#define WDSP_STATUS_DATA_DLOADED  BIT(2)
#define WDSP_STATUS_BOOTED        BIT(3)

/* Helper macros for printing wdsp messages */
#define WDSP_ERR(wdsp, fmt, ...)		\
	dev_err(wdsp->mdev, "%s: " fmt "\n", __func__, ##__VA_ARGS__)
#define WDSP_DBG(wdsp, fmt, ...)	\
	dev_dbg(wdsp->mdev, "%s: " fmt "\n", __func__, ##__VA_ARGS__)

/* Helper macros for locking */
#define WDSP_MGR_MUTEX_LOCK(wdsp, lock)         \
{                                               \
	WDSP_DBG(wdsp, "mutex_lock(%s)",        \
		 __stringify_1(lock));          \
	mutex_lock(&lock);                      \
}

#define WDSP_MGR_MUTEX_UNLOCK(wdsp, lock)       \
{                                               \
	WDSP_DBG(wdsp, "mutex_unlock(%s)",      \
		 __stringify_1(lock));          \
	mutex_unlock(&lock);                    \
}

/* Helper macros for using status mask */
#define WDSP_SET_STATUS(wdsp, state)                  \
{                                                     \
	wdsp->status |= state;                        \
	WDSP_DBG(wdsp, "set 0x%lx, new_state = 0x%x", \
		 state, wdsp->status);                \
}

#define WDSP_CLEAR_STATUS(wdsp, state)                  \
{                                                       \
	wdsp->status &= (~state);                       \
	WDSP_DBG(wdsp, "clear 0x%lx, new_state = 0x%x", \
		 state, wdsp->status);                  \
}

#define WDSP_STATUS_IS_SET(wdsp, state) (wdsp->status & state)

/* SSR relate status macros */
#define WDSP_SSR_STATUS_WDSP_READY    BIT(0)
#define WDSP_SSR_STATUS_CDC_READY     BIT(1)
#define WDSP_SSR_STATUS_READY         \
	(WDSP_SSR_STATUS_WDSP_READY | WDSP_SSR_STATUS_CDC_READY)
#define WDSP_SSR_READY_WAIT_TIMEOUT   (10 * HZ)

enum wdsp_ssr_type {

	/* Init value, indicates there is no SSR in progress */
	WDSP_SSR_TYPE_NO_SSR = 0,

	/*
	 * Indicates WDSP crashed. The manager driver internally
	 * decides when to perform WDSP restart based on the
	 * users of wdsp. Hence there is no explicit WDSP_UP.
	 */
	WDSP_SSR_TYPE_WDSP_DOWN,

	/* Indicates codec hardware is down */
	WDSP_SSR_TYPE_CDC_DOWN,

	/* Indicates codec hardware is up, trigger to restart WDSP */
	WDSP_SSR_TYPE_CDC_UP,
};

struct wdsp_cmpnt {

	/* OF node of the phandle */
	struct device_node *np;

	/*
	 * Child component's dev_name, should be set in DT for the child's
	 * phandle if child's dev->of_node does not match the phandle->of_node
	 */
	const char *cdev_name;

	/* Child component's device node */
	struct device *cdev;

	/* Private data that component may want back on callbacks */
	void *priv_data;

	/* Child ops */
	struct wdsp_cmpnt_ops *ops;
};

struct wdsp_ramdump_data {

	/* Ramdump device */
	void *rd_dev;

	/* DMA address of the dump */
	dma_addr_t rd_addr;

	/* Virtual address of the dump */
	void *rd_v_addr;

	/* Data provided through error interrupt */
	struct wdsp_err_signal_arg err_data;
};

struct wdsp_mgr_priv {

	/* Manager driver's struct device pointer */
	struct device *mdev;

	/* Match struct for component framework */
	struct component_match *match;

	/* Manager's ops/function callbacks */
	struct wdsp_mgr_ops *ops;

	/* Array to store information for all expected components */
	struct wdsp_cmpnt cmpnts[WDSP_CMPNT_TYPE_MAX];

	/* The filename of image to be downloaded */
	const char *img_fname;

	/* Keeps track of current state of manager driver */
	u32 status;

	/* Work to load the firmware image after component binding */
	struct work_struct load_fw_work;

	/* List of segments in image to be downloaded */
	struct list_head *seg_list;

	/* Base address of the image in memory */
	u32 base_addr;

	/* Instances using dsp */
	int dsp_users;

	/* Lock for serializing ops called by components */
	struct mutex api_mutex;

	struct wdsp_ramdump_data dump_data;

	/* SSR related */
	enum wdsp_ssr_type ssr_type;
	struct mutex ssr_mutex;
	struct work_struct ssr_work;
	u16 ready_status;
	struct completion ready_compl;

	/* Debugfs related */
	struct dentry *entry;
	bool panic_on_error;
};

static char *wdsp_get_ssr_type_string(enum wdsp_ssr_type type)
{
	switch (type) {
	case WDSP_SSR_TYPE_NO_SSR:
		return "NO_SSR";
	case WDSP_SSR_TYPE_WDSP_DOWN:
		return "WDSP_DOWN";
	case WDSP_SSR_TYPE_CDC_DOWN:
		return "CDC_DOWN";
	case WDSP_SSR_TYPE_CDC_UP:
		return "CDC_UP";
	default:
		pr_err("%s: Invalid ssr_type %d\n",
			__func__, type);
		return "Invalid";
	}
}

static char *wdsp_get_cmpnt_type_string(enum wdsp_cmpnt_type type)
{
	switch (type) {
	case WDSP_CMPNT_CONTROL:
		return "control";
	case WDSP_CMPNT_IPC:
		return "ipc";
	case WDSP_CMPNT_TRANSPORT:
		return "transport";
	default:
		pr_err("%s: Invalid component type %d\n",
			__func__, type);
		return "Invalid";
	}
}

static void __wdsp_clr_ready_locked(struct wdsp_mgr_priv *wdsp,
				    u16 value)
{
	wdsp->ready_status &= ~(value);
	WDSP_DBG(wdsp, "ready_status = 0x%x", wdsp->ready_status);
}

static void __wdsp_set_ready_locked(struct wdsp_mgr_priv *wdsp,
				    u16 value, bool mark_complete)
{
	wdsp->ready_status |= value;
	WDSP_DBG(wdsp, "ready_status = 0x%x", wdsp->ready_status);

	if (mark_complete &&
	    wdsp->ready_status == WDSP_SSR_STATUS_READY) {
		WDSP_DBG(wdsp, "marking ready completion");
		complete(&wdsp->ready_compl);
	}
}

static void wdsp_broadcast_event_upseq(struct wdsp_mgr_priv *wdsp,
				       enum wdsp_event_type event,
				       void *data)
{
	struct wdsp_cmpnt *cmpnt;
	int i;

	for (i = 0; i < WDSP_CMPNT_TYPE_MAX; i++) {
		cmpnt = WDSP_GET_COMPONENT(wdsp, i);
		if (cmpnt && cmpnt->ops && cmpnt->ops->event_handler)
			cmpnt->ops->event_handler(cmpnt->cdev, cmpnt->priv_data,
						  event, data);
	}
}

static void wdsp_broadcast_event_downseq(struct wdsp_mgr_priv *wdsp,
					 enum wdsp_event_type event,
					 void *data)
{
	struct wdsp_cmpnt *cmpnt;
	int i;

	for (i = WDSP_CMPNT_TYPE_MAX - 1; i >= 0; i--) {
		cmpnt = WDSP_GET_COMPONENT(wdsp, i);
		if (cmpnt && cmpnt->ops && cmpnt->ops->event_handler)
			cmpnt->ops->event_handler(cmpnt->cdev, cmpnt->priv_data,
						  event, data);
	}
}

static int wdsp_unicast_event(struct wdsp_mgr_priv *wdsp,
			      enum wdsp_cmpnt_type type,
			      enum wdsp_event_type event,
			      void *data)
{
	struct wdsp_cmpnt *cmpnt;
	int ret;

	cmpnt = WDSP_GET_COMPONENT(wdsp, type);
	if (cmpnt && cmpnt->ops && cmpnt->ops->event_handler) {
		ret = cmpnt->ops->event_handler(cmpnt->cdev, cmpnt->priv_data,
						event, data);
	} else {
		WDSP_ERR(wdsp, "not valid event_handler for %s",
			 WDSP_GET_CMPNT_TYPE_STR(type));
		ret = -EINVAL;
	}

	return ret;
}

static void wdsp_deinit_components(struct wdsp_mgr_priv *wdsp)
{
	struct wdsp_cmpnt *cmpnt;
	int i;

	for (i = WDSP_CMPNT_TYPE_MAX - 1; i >= 0; i--) {
		cmpnt = WDSP_GET_COMPONENT(wdsp, i);
		if (cmpnt && cmpnt->ops && cmpnt->ops->deinit)
			cmpnt->ops->deinit(cmpnt->cdev, cmpnt->priv_data);
	}
}

static int wdsp_init_components(struct wdsp_mgr_priv *wdsp)
{
	struct wdsp_cmpnt *cmpnt;
	int fail_idx = WDSP_CMPNT_TYPE_MAX;
	int i, ret = 0;

	for (i = 0; i < WDSP_CMPNT_TYPE_MAX; i++) {

		cmpnt = WDSP_GET_COMPONENT(wdsp, i);

		/* Init is allowed to be NULL */
		if (!cmpnt->ops || !cmpnt->ops->init)
			continue;
		ret = cmpnt->ops->init(cmpnt->cdev, cmpnt->priv_data);
		if (ret) {
			WDSP_ERR(wdsp, "Init failed (%d) for component %s",
				 ret, WDSP_GET_CMPNT_TYPE_STR(i));
				fail_idx = i;
				break;
		}
	}

	if (fail_idx < WDSP_CMPNT_TYPE_MAX) {
		/* Undo init for already initialized components */
		for (i = fail_idx - 1; i >= 0; i--) {
			struct wdsp_cmpnt *cmpnt = WDSP_GET_COMPONENT(wdsp, i);

			if (cmpnt->ops && cmpnt->ops->deinit)
				cmpnt->ops->deinit(cmpnt->cdev,
						   cmpnt->priv_data);
		}
	} else {
		wdsp_broadcast_event_downseq(wdsp, WDSP_EVENT_POST_INIT, NULL);
	}

	return ret;
}

static int wdsp_load_each_segment(struct wdsp_mgr_priv *wdsp,
				  struct wdsp_img_segment *seg)
{
	struct wdsp_img_section img_section;
	int ret;

	WDSP_DBG(wdsp,
		 "base_addr 0x%x, split_fname %s, load_addr 0x%x, size 0x%zx",
		 wdsp->base_addr, seg->split_fname, seg->load_addr, seg->size);

	if (seg->load_addr < wdsp->base_addr) {
		WDSP_ERR(wdsp, "Invalid addr 0x%x, base_addr = 0x%x",
			 seg->load_addr, wdsp->base_addr);
		return -EINVAL;
	}

	img_section.addr = seg->load_addr - wdsp->base_addr;
	img_section.size = seg->size;
	img_section.data = seg->data;

	ret = wdsp_unicast_event(wdsp, WDSP_CMPNT_TRANSPORT,
				 WDSP_EVENT_DLOAD_SECTION,
				 &img_section);
	if (ret < 0)
		WDSP_ERR(wdsp,
			 "Failed, err = %d for base_addr = 0x%x split_fname = %s, load_addr = 0x%x, size = 0x%zx",
			 ret, wdsp->base_addr, seg->split_fname,
			 seg->load_addr, seg->size);
	return ret;
}

static int wdsp_download_segments(struct wdsp_mgr_priv *wdsp,
				  unsigned int type)
{
	struct wdsp_cmpnt *ctl;
	struct wdsp_img_segment *seg = NULL;
	enum wdsp_event_type pre, post;
	long status;
	int ret;

	ctl = WDSP_GET_COMPONENT(wdsp, WDSP_CMPNT_CONTROL);

	if (type == WDSP_ELF_FLAG_RE) {
		pre = WDSP_EVENT_PRE_DLOAD_CODE;
		post = WDSP_EVENT_POST_DLOAD_CODE;
		status = WDSP_STATUS_CODE_DLOADED;
	} else if (type == WDSP_ELF_FLAG_WRITE) {
		pre = WDSP_EVENT_PRE_DLOAD_DATA;
		post = WDSP_EVENT_POST_DLOAD_DATA;
		status = WDSP_STATUS_DATA_DLOADED;
	} else {
		WDSP_ERR(wdsp, "Invalid type %u", type);
		return -EINVAL;
	}

	ret = wdsp_get_segment_list(ctl->cdev, wdsp->img_fname,
				    type, wdsp->seg_list, &wdsp->base_addr);
	pr_info("%s: downloading wdsp firmware: %s.\n", __func__, wdsp->img_fname);
	if (ret < 0 ||
	    list_empty(wdsp->seg_list)) {
		WDSP_ERR(wdsp, "Error %d to get image segments for type %d",
			 ret, type);
		wdsp_broadcast_event_downseq(wdsp, WDSP_EVENT_DLOAD_FAILED,
					     NULL);
		goto done;
	}

	/* Notify all components that image is about to be downloaded */
	wdsp_broadcast_event_upseq(wdsp, pre, NULL);

	/* Go through the list of segments and download one by one */
	list_for_each_entry(seg, wdsp->seg_list, list) {
		ret = wdsp_load_each_segment(wdsp, seg);
		if (ret)
			goto dload_error;
	}

	/* Flush the list before setting status and notifying components */
	wdsp_flush_segment_list(wdsp->seg_list);

	WDSP_SET_STATUS(wdsp, status);

	/* Notify all components that image is downloaded */
	wdsp_broadcast_event_downseq(wdsp, post, NULL);
done:
	return ret;

dload_error:
	wdsp_flush_segment_list(wdsp->seg_list);
	wdsp_broadcast_event_downseq(wdsp, WDSP_EVENT_DLOAD_FAILED, NULL);
	return ret;
}

static int wdsp_init_and_dload_code_sections(struct wdsp_mgr_priv *wdsp)
{
	int ret;
	bool is_initialized;

	is_initialized = WDSP_STATUS_IS_SET(wdsp, WDSP_STATUS_INITIALIZED);

	if (!is_initialized) {
		/* Components are not initialized yet, initialize them */
		ret = wdsp_init_components(wdsp);
		if (ret < 0) {
			WDSP_ERR(wdsp, "INIT failed, err = %d", ret);
			goto done;
		}
		WDSP_SET_STATUS(wdsp, WDSP_STATUS_INITIALIZED);
	}

	/* Download the read-execute sections of image */
	ret = wdsp_download_segments(wdsp, WDSP_ELF_FLAG_RE);
	if (ret < 0) {
		WDSP_ERR(wdsp, "Error %d to download code sections", ret);
		goto done;
	}
done:
	return ret;
}

static void wdsp_load_fw_image(struct work_struct *work)
{
	struct wdsp_mgr_priv *wdsp;
	int ret;

	wdsp = container_of(work, struct wdsp_mgr_priv, load_fw_work);
	if (!wdsp) {
		pr_err("%s: Invalid private_data\n", __func__);
		return;
	}

	ret = wdsp_init_and_dload_code_sections(wdsp);
	if (ret < 0)
		WDSP_ERR(wdsp, "dload code sections failed, err = %d", ret);
}

static int wdsp_enable_dsp(struct wdsp_mgr_priv *wdsp)
{
	int ret;

	/* Make sure wdsp is in good state */
	if (!WDSP_STATUS_IS_SET(wdsp, WDSP_STATUS_CODE_DLOADED)) {
		WDSP_ERR(wdsp, "WDSP in invalid state 0x%x", wdsp->status);
		return -EINVAL;
	}

	/*
	 * Acquire SSR mutex lock to make sure enablement of DSP
	 * does not race with SSR handling.
	 */
	WDSP_MGR_MUTEX_LOCK(wdsp, wdsp->ssr_mutex);
	/* Download the read-write sections of image */
	ret = wdsp_download_segments(wdsp, WDSP_ELF_FLAG_WRITE);
	if (ret < 0) {
		WDSP_ERR(wdsp, "Data section download failed, err = %d", ret);
		goto done;
	}

	wdsp_broadcast_event_upseq(wdsp, WDSP_EVENT_PRE_BOOTUP, NULL);

	ret = wdsp_unicast_event(wdsp, WDSP_CMPNT_CONTROL,
				 WDSP_EVENT_DO_BOOT, NULL);
	if (ret < 0) {
		WDSP_ERR(wdsp, "Failed to boot dsp, err = %d", ret);
		WDSP_CLEAR_STATUS(wdsp, WDSP_STATUS_DATA_DLOADED);
		goto done;
	}

	wdsp_broadcast_event_downseq(wdsp, WDSP_EVENT_POST_BOOTUP, NULL);
	WDSP_SET_STATUS(wdsp, WDSP_STATUS_BOOTED);
done:
	WDSP_MGR_MUTEX_UNLOCK(wdsp, wdsp->ssr_mutex);
	return ret;
}

static int wdsp_disable_dsp(struct wdsp_mgr_priv *wdsp)
{
	int ret;

	WDSP_MGR_MUTEX_LOCK(wdsp, wdsp->ssr_mutex);

	/*
	 * If Disable happened while SSR is in progress, then set the SSR
	 * ready status indicating WDSP is now ready. Ignore the disable
	 * event here and let the SSR handler go through shutdown.
	 */
	if (wdsp->ssr_type != WDSP_SSR_TYPE_NO_SSR) {
		__wdsp_set_ready_locked(wdsp, WDSP_SSR_STATUS_WDSP_READY, true);
		WDSP_MGR_MUTEX_UNLOCK(wdsp, wdsp->ssr_mutex);
		return 0;
	}

	WDSP_MGR_MUTEX_UNLOCK(wdsp, wdsp->ssr_mutex);

	/* Make sure wdsp is in good state */
	if (!WDSP_STATUS_IS_SET(wdsp, WDSP_STATUS_BOOTED)) {
		WDSP_ERR(wdsp, "wdsp in invalid state 0x%x", wdsp->status);
		ret = -EINVAL;
		goto done;
	}

	wdsp_broadcast_event_downseq(wdsp, WDSP_EVENT_PRE_SHUTDOWN, NULL);
	ret = wdsp_unicast_event(wdsp, WDSP_CMPNT_CONTROL,
				 WDSP_EVENT_DO_SHUTDOWN, NULL);
	if (ret < 0) {
		WDSP_ERR(wdsp, "Failed to shutdown dsp, err = %d", ret);
		goto done;
	}

	wdsp_broadcast_event_downseq(wdsp, WDSP_EVENT_POST_SHUTDOWN, NULL);
	WDSP_CLEAR_STATUS(wdsp, WDSP_STATUS_BOOTED);

	/* Data sections are to be downloaded per boot */
	WDSP_CLEAR_STATUS(wdsp, WDSP_STATUS_DATA_DLOADED);
done:
	return ret;
}

static int wdsp_register_cmpnt_ops(struct device *wdsp_dev,
				   struct device *cdev,
				   void *priv_data,
				   struct wdsp_cmpnt_ops *ops)
{
	struct wdsp_mgr_priv *wdsp;
	struct wdsp_cmpnt *cmpnt;
	int i, ret;

	if (!wdsp_dev || !cdev || !ops)
		return -EINVAL;

	wdsp = dev_get_drvdata(wdsp_dev);

	WDSP_MGR_MUTEX_LOCK(wdsp, wdsp->api_mutex);

	for (i = 0; i < WDSP_CMPNT_TYPE_MAX; i++) {
		cmpnt = WDSP_GET_COMPONENT(wdsp, i);
		if ((cdev->of_node && cdev->of_node == cmpnt->np) ||
		    (cmpnt->cdev_name &&
		     !strcmp(dev_name(cdev), cmpnt->cdev_name))) {
			break;
		}
	}

	if (i == WDSP_CMPNT_TYPE_MAX) {
		WDSP_ERR(wdsp, "Failed to register component dev %s",
			 dev_name(cdev));
		ret = -EINVAL;
		goto done;
	}

	cmpnt->cdev = cdev;
	cmpnt->ops = ops;
	cmpnt->priv_data = priv_data;
done:
	WDSP_MGR_MUTEX_UNLOCK(wdsp, wdsp->api_mutex);
	return 0;
}

static struct device *wdsp_get_dev_for_cmpnt(struct device *wdsp_dev,
					     enum wdsp_cmpnt_type type)
{
	struct wdsp_mgr_priv *wdsp;
	struct wdsp_cmpnt *cmpnt;

	if (!wdsp_dev || type >= WDSP_CMPNT_TYPE_MAX)
		return NULL;

	wdsp = dev_get_drvdata(wdsp_dev);
	cmpnt = WDSP_GET_COMPONENT(wdsp, type);

	return cmpnt->cdev;
}

static int wdsp_get_devops_for_cmpnt(struct device *wdsp_dev,
				     enum wdsp_cmpnt_type type,
				     void *data)
{
	struct wdsp_mgr_priv *wdsp;
	int ret = 0;

	if (!wdsp_dev || type >= WDSP_CMPNT_TYPE_MAX)
		return -EINVAL;

	wdsp = dev_get_drvdata(wdsp_dev);
	ret = wdsp_unicast_event(wdsp, type,
				 WDSP_EVENT_GET_DEVOPS, data);
	if (ret)
		WDSP_ERR(wdsp, "get_dev_ops failed for cmpnt type %d",
			 type);
	return ret;
}

static void wdsp_collect_ramdumps(struct wdsp_mgr_priv *wdsp)
{
	struct wdsp_img_section img_section;
	struct wdsp_err_signal_arg *data = &wdsp->dump_data.err_data;
	struct ramdump_segment rd_seg;
	int ret = 0;

	if (wdsp->ssr_type != WDSP_SSR_TYPE_WDSP_DOWN ||
	    !data->mem_dumps_enabled) {
		WDSP_DBG(wdsp, "cannot dump memory, ssr_type %s, dumps %s",
			 wdsp_get_ssr_type_string(wdsp->ssr_type),
			 !(data->mem_dumps_enabled) ? "disabled" : "enabled");
		goto done;
	}

	if (data->dump_size == 0 ||
	    data->remote_start_addr < wdsp->base_addr) {
		WDSP_ERR(wdsp, "Invalid start addr 0x%x or dump_size 0x%zx",
			 data->remote_start_addr, data->dump_size);
		goto done;
	}

	if (!wdsp->dump_data.rd_dev) {
		WDSP_ERR(wdsp, "Ramdump device is not setup");
		goto done;
	}

	WDSP_DBG(wdsp, "base_addr 0x%x, dump_start_addr 0x%x, dump_size 0x%zx",
		 wdsp->base_addr, data->remote_start_addr, data->dump_size);

	/* Allocate memory for dumps */
	wdsp->dump_data.rd_v_addr = dma_alloc_coherent(wdsp->mdev,
						       data->dump_size,
						       &wdsp->dump_data.rd_addr,
						       GFP_KERNEL);
	if (!wdsp->dump_data.rd_v_addr)
		goto done;

	img_section.addr = data->remote_start_addr - wdsp->base_addr;
	img_section.size = data->dump_size;
	img_section.data = wdsp->dump_data.rd_v_addr;

	ret = wdsp_unicast_event(wdsp, WDSP_CMPNT_TRANSPORT,
				 WDSP_EVENT_READ_SECTION,
				 &img_section);
	if (ret < 0) {
		WDSP_ERR(wdsp, "Failed to read dumps, size 0x%zx at addr 0x%x",
			 img_section.size, img_section.addr);
		goto err_read_dumps;
	}

	/*
	 * If panic_on_error flag is explicitly set through the debugfs,
	 * then cause a BUG here to aid debugging.
	 */
	BUG_ON(wdsp->panic_on_error);

	rd_seg.address = (unsigned long) wdsp->dump_data.rd_v_addr;
	rd_seg.size = img_section.size;
	rd_seg.v_address = wdsp->dump_data.rd_v_addr;

	ret = do_ramdump(wdsp->dump_data.rd_dev, &rd_seg, 1);
	if (ret < 0)
		WDSP_ERR(wdsp, "do_ramdump failed with error %d", ret);

err_read_dumps:
	dma_free_coherent(wdsp->mdev, data->dump_size,
			  wdsp->dump_data.rd_v_addr, wdsp->dump_data.rd_addr);
done:
	return;
}

static void wdsp_ssr_work_fn(struct work_struct *work)
{
	struct wdsp_mgr_priv *wdsp;
	int ret;

	wdsp = container_of(work, struct wdsp_mgr_priv, ssr_work);
	if (!wdsp) {
		pr_err("%s: Invalid private_data\n", __func__);
		return;
	}

	WDSP_MGR_MUTEX_LOCK(wdsp, wdsp->ssr_mutex);

	/* Issue ramdumps and shutdown only if DSP is currently booted */
	if (WDSP_STATUS_IS_SET(wdsp, WDSP_STATUS_BOOTED)) {
		wdsp_collect_ramdumps(wdsp);
		ret = wdsp_unicast_event(wdsp, WDSP_CMPNT_CONTROL,
					 WDSP_EVENT_DO_SHUTDOWN, NULL);
		if (ret < 0)
			WDSP_ERR(wdsp, "Failed WDSP shutdown, err = %d", ret);

		wdsp_broadcast_event_downseq(wdsp, WDSP_EVENT_POST_SHUTDOWN,
					     NULL);
		WDSP_CLEAR_STATUS(wdsp, WDSP_STATUS_BOOTED);
	}

	WDSP_MGR_MUTEX_UNLOCK(wdsp, wdsp->ssr_mutex);
	ret = wait_for_completion_timeout(&wdsp->ready_compl,
					  WDSP_SSR_READY_WAIT_TIMEOUT);
	WDSP_MGR_MUTEX_LOCK(wdsp, wdsp->ssr_mutex);
	if (ret == 0) {
		WDSP_ERR(wdsp, "wait_for_ready timed out, status = 0x%x",
			 wdsp->ready_status);
		goto done;
	}

	/* Data sections are to downloaded per WDSP boot */
	WDSP_CLEAR_STATUS(wdsp, WDSP_STATUS_DATA_DLOADED);

	/*
	 * Even though code section could possible be retained on DSP
	 * crash, go ahead and still re-download just to avoid any
	 * memory corruption from previous crash.
	 */
	WDSP_CLEAR_STATUS(wdsp, WDSP_STATUS_CODE_DLOADED);

	/* If codec restarted, then all components must be re-initialized */
	if (wdsp->ssr_type == WDSP_SSR_TYPE_CDC_UP) {
		wdsp_deinit_components(wdsp);
		WDSP_CLEAR_STATUS(wdsp, WDSP_STATUS_INITIALIZED);
	}

	ret = wdsp_init_and_dload_code_sections(wdsp);
	if (ret < 0) {
		WDSP_ERR(wdsp, "Failed to dload code sections err = %d",
			 ret);
		goto done;
	}

	/* SSR handling is finished, mark SSR type as NO_SSR */
	wdsp->ssr_type = WDSP_SSR_TYPE_NO_SSR;
done:
	WDSP_MGR_MUTEX_UNLOCK(wdsp, wdsp->ssr_mutex);
}

static int wdsp_ssr_handler(struct wdsp_mgr_priv *wdsp, void *arg,
			    enum wdsp_ssr_type ssr_type)
{
	enum wdsp_ssr_type current_ssr_type;
	struct wdsp_err_signal_arg *err_data;

	WDSP_MGR_MUTEX_LOCK(wdsp, wdsp->ssr_mutex);

	current_ssr_type = wdsp->ssr_type;
	WDSP_DBG(wdsp, "Current ssr_type %s, handling ssr_type %s",
		 wdsp_get_ssr_type_string(current_ssr_type),
		 wdsp_get_ssr_type_string(ssr_type));
	wdsp->ssr_type = ssr_type;

	if (arg) {
		err_data = (struct wdsp_err_signal_arg *) arg;
		memcpy(&wdsp->dump_data.err_data, err_data,
		       sizeof(*err_data));
	} else {
		memset(&wdsp->dump_data.err_data, 0,
		       sizeof(wdsp->dump_data.err_data));
	}

	switch (ssr_type) {

	case WDSP_SSR_TYPE_WDSP_DOWN:
		__wdsp_clr_ready_locked(wdsp, WDSP_SSR_STATUS_WDSP_READY);
		wdsp_broadcast_event_downseq(wdsp, WDSP_EVENT_PRE_SHUTDOWN,
					     NULL);
		reinit_completion(&wdsp->ready_compl);
		schedule_work(&wdsp->ssr_work);
		break;

	case WDSP_SSR_TYPE_CDC_DOWN:
		__wdsp_clr_ready_locked(wdsp, WDSP_SSR_STATUS_CDC_READY);
		/*
		 * If DSP is booted when CDC_DOWN is received, it needs
		 * to be shutdown.
		 */
		if (WDSP_STATUS_IS_SET(wdsp, WDSP_STATUS_BOOTED)) {
			__wdsp_clr_ready_locked(wdsp,
						WDSP_SSR_STATUS_WDSP_READY);
			wdsp_broadcast_event_downseq(wdsp,
						     WDSP_EVENT_PRE_SHUTDOWN,
						     NULL);
		}
		reinit_completion(&wdsp->ready_compl);
		schedule_work(&wdsp->ssr_work);
		break;

	case WDSP_SSR_TYPE_CDC_UP:
		__wdsp_set_ready_locked(wdsp, WDSP_SSR_STATUS_CDC_READY, true);
		break;

	default:
		WDSP_ERR(wdsp, "undefined ssr_type %d\n", ssr_type);
		/* Revert back the ssr_type for undefined events */
		wdsp->ssr_type = current_ssr_type;
		break;
	}

	WDSP_MGR_MUTEX_UNLOCK(wdsp, wdsp->ssr_mutex);

	return 0;
}

#ifdef CONFIG_DEBUG_FS
static int __wdsp_dbg_dump_locked(struct wdsp_mgr_priv *wdsp, void *arg)
{
	struct wdsp_err_signal_arg *err_data;
	int ret = 0;

	/* If there is no SSR, set the SSR type to collect ramdumps */
	if (wdsp->ssr_type == WDSP_SSR_TYPE_NO_SSR) {
		wdsp->ssr_type = WDSP_SSR_TYPE_WDSP_DOWN;
	} else {
		WDSP_DBG(wdsp, "SSR handling is running, skip debug ramdump");
		ret = 0;
		goto done;
	}

	if (arg) {
		err_data = (struct wdsp_err_signal_arg *) arg;
		memcpy(&wdsp->dump_data.err_data, err_data,
		       sizeof(*err_data));
	} else {
		WDSP_DBG(wdsp, "Invalid input, arg is NULL");
		ret = -EINVAL;
		goto done;
	}
	wdsp_collect_ramdumps(wdsp);
	wdsp->ssr_type = WDSP_SSR_TYPE_NO_SSR;
done:
	return ret;
}
static int wdsp_debug_dump_handler(struct wdsp_mgr_priv *wdsp, void *arg)
{
	int ret = 0;

	WDSP_MGR_MUTEX_LOCK(wdsp, wdsp->ssr_mutex);
	ret = __wdsp_dbg_dump_locked(wdsp, arg);
	WDSP_MGR_MUTEX_UNLOCK(wdsp, wdsp->ssr_mutex);

	return ret;
}
#else
static int __wdsp_dbg_dump_locked(struct wdsp_mgr_priv *wdsp, void *arg)
{
	return 0;
}

static int wdsp_debug_dump_handler(struct wdsp_mgr_priv *wdsp, void *arg)
{
	return 0;
}
#endif

static int wdsp_signal_handler(struct device *wdsp_dev,
			       enum wdsp_signal signal, void *arg)
{
	struct wdsp_mgr_priv *wdsp;
	int ret;

	if (!wdsp_dev)
		return -EINVAL;

	wdsp = dev_get_drvdata(wdsp_dev);

#ifdef CONFIG_DEBUG_FS
	if (signal != WDSP_DEBUG_DUMP_INTERNAL)
		WDSP_MGR_MUTEX_LOCK(wdsp, wdsp->api_mutex);
#else
	WDSP_MGR_MUTEX_LOCK(wdsp, wdsp->api_mutex);
#endif

	WDSP_DBG(wdsp, "Raised signal %d", signal);

	switch (signal) {
	case WDSP_IPC1_INTR:
		ret = wdsp_unicast_event(wdsp, WDSP_CMPNT_IPC,
					 WDSP_EVENT_IPC1_INTR, NULL);
		break;
	case WDSP_ERR_INTR:
		ret = wdsp_ssr_handler(wdsp, arg, WDSP_SSR_TYPE_WDSP_DOWN);
		break;
	case WDSP_CDC_DOWN_SIGNAL:
		ret = wdsp_ssr_handler(wdsp, arg, WDSP_SSR_TYPE_CDC_DOWN);
		break;
	case WDSP_CDC_UP_SIGNAL:
		ret = wdsp_ssr_handler(wdsp, arg, WDSP_SSR_TYPE_CDC_UP);
		break;
	case WDSP_DEBUG_DUMP:
		ret = wdsp_debug_dump_handler(wdsp, arg);
		break;
	case WDSP_DEBUG_DUMP_INTERNAL:
		ret = __wdsp_dbg_dump_locked(wdsp, arg);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret < 0)
		WDSP_ERR(wdsp, "handling signal %d failed with error %d",
			 signal, ret);

#ifdef CONFIG_DEBUG_FS
	if (signal != WDSP_DEBUG_DUMP_INTERNAL)
		WDSP_MGR_MUTEX_UNLOCK(wdsp, wdsp->api_mutex);
#else
	WDSP_MGR_MUTEX_UNLOCK(wdsp, wdsp->api_mutex);
#endif

	return ret;
}

static int wdsp_vote_for_dsp(struct device *wdsp_dev,
			     bool vote)
{
	struct wdsp_mgr_priv *wdsp;
	int ret = 0;

	if (!wdsp_dev)
		return -EINVAL;

	wdsp = dev_get_drvdata(wdsp_dev);

	WDSP_MGR_MUTEX_LOCK(wdsp, wdsp->api_mutex);
	WDSP_DBG(wdsp, "request %s, current users = %d",
		 vote ? "enable" : "disable", wdsp->dsp_users);

	if (vote) {
		wdsp->dsp_users++;
		if (wdsp->dsp_users == 1)
			ret = wdsp_enable_dsp(wdsp);
	} else {
		if (wdsp->dsp_users == 0)
			goto done;

		wdsp->dsp_users--;
		if (wdsp->dsp_users == 0)
			ret = wdsp_disable_dsp(wdsp);
	}

	if (ret < 0)
		WDSP_DBG(wdsp, "wdsp %s failed, err = %d",
			 vote ? "enable" : "disable", ret);

done:
	WDSP_MGR_MUTEX_UNLOCK(wdsp, wdsp->api_mutex);
	return ret;
}

static int wdsp_suspend(struct device *wdsp_dev)
{
	struct wdsp_mgr_priv *wdsp;
	int rc = 0, i;

	if (!wdsp_dev) {
		pr_err("%s: Invalid handle to device\n", __func__);
		return -EINVAL;
	}

	wdsp = dev_get_drvdata(wdsp_dev);

	for (i =  WDSP_CMPNT_TYPE_MAX - 1; i >= 0; i--) {
		rc = wdsp_unicast_event(wdsp, i, WDSP_EVENT_SUSPEND, NULL);
		if (rc < 0) {
			WDSP_ERR(wdsp, "component %s failed to suspend\n",
				WDSP_GET_CMPNT_TYPE_STR(i));
			break;
		}
	}

	return rc;
}

static int wdsp_resume(struct device *wdsp_dev)
{
	struct wdsp_mgr_priv *wdsp;
	int rc = 0, i;

	if (!wdsp_dev) {
		pr_err("%s: Invalid handle to device\n", __func__);
		return -EINVAL;
	}

	wdsp = dev_get_drvdata(wdsp_dev);

	for (i =  0; i < WDSP_CMPNT_TYPE_MAX; i++) {
		rc = wdsp_unicast_event(wdsp, i, WDSP_EVENT_RESUME, NULL);
		if (rc < 0) {
			WDSP_ERR(wdsp, "component %s failed to resume\n",
				WDSP_GET_CMPNT_TYPE_STR(i));
			break;
		}
	}

	return rc;
}

static struct wdsp_mgr_ops wdsp_ops = {
	.register_cmpnt_ops = wdsp_register_cmpnt_ops,
	.get_dev_for_cmpnt = wdsp_get_dev_for_cmpnt,
	.get_devops_for_cmpnt = wdsp_get_devops_for_cmpnt,
	.signal_handler = wdsp_signal_handler,
	.vote_for_dsp = wdsp_vote_for_dsp,
	.suspend = wdsp_suspend,
	.resume = wdsp_resume,
};

static int wdsp_mgr_compare_of(struct device *dev, void *data)
{
	struct wdsp_cmpnt *cmpnt = data;

	/*
	 * First try to match based on of_node, if of_node is not
	 * present, try to match on the dev_name
	 */
	return ((dev->of_node && dev->of_node == cmpnt->np) ||
		(cmpnt->cdev_name &&
		 !strcmp(dev_name(dev), cmpnt->cdev_name)));
}

static void wdsp_mgr_debugfs_init(struct wdsp_mgr_priv *wdsp)
{
	wdsp->entry = debugfs_create_dir("wdsp_mgr", NULL);
	if (IS_ERR_OR_NULL(wdsp->entry))
		return;

	debugfs_create_bool("panic_on_error", 0644,
			    wdsp->entry, &wdsp->panic_on_error);

	debugfs_create_u32("wdsp_status", S_IRUGO,
			    wdsp->entry, &wdsp->status);
}

static void wdsp_mgr_debugfs_remove(struct wdsp_mgr_priv *wdsp)
{
	debugfs_remove_recursive(wdsp->entry);
	wdsp->entry = NULL;
}

static int wdsp_mgr_bind(struct device *dev)
{
	struct wdsp_mgr_priv *wdsp = dev_get_drvdata(dev);
	struct wdsp_cmpnt *cmpnt;
	int ret, idx;

	wdsp->ops = &wdsp_ops;

	/* Setup ramdump device */
	wdsp->dump_data.rd_dev = create_ramdump_device("wdsp", dev);
	if (!wdsp->dump_data.rd_dev)
		dev_info(dev, "%s: create_ramdump_device failed\n", __func__);

	ret = component_bind_all(dev, wdsp->ops);
	if (ret < 0) {
		WDSP_ERR(wdsp, "component_bind_all failed %d\n", ret);
		return ret;
	}

	/* Make sure all components registered ops */
	for (idx = 0; idx < WDSP_CMPNT_TYPE_MAX; idx++) {
		cmpnt = WDSP_GET_COMPONENT(wdsp, idx);
		if (!cmpnt->cdev || !cmpnt->ops) {
			WDSP_ERR(wdsp, "%s did not register ops\n",
				 WDSP_GET_CMPNT_TYPE_STR(idx));
			ret = -EINVAL;
			component_unbind_all(dev, wdsp->ops);
			break;
		}
	}

	wdsp_mgr_debugfs_init(wdsp);

	/* Schedule the work to download image if binding was successful. */
	if (!ret)
		schedule_work(&wdsp->load_fw_work);

	return ret;
}

static void wdsp_mgr_unbind(struct device *dev)
{
	struct wdsp_mgr_priv *wdsp = dev_get_drvdata(dev);
	struct wdsp_cmpnt *cmpnt;
	int idx;

	cancel_work_sync(&wdsp->load_fw_work);

	component_unbind_all(dev, wdsp->ops);

	wdsp_mgr_debugfs_remove(wdsp);

	if (wdsp->dump_data.rd_dev) {
		destroy_ramdump_device(wdsp->dump_data.rd_dev);
		wdsp->dump_data.rd_dev = NULL;
	}

	/* Clear all status bits */
	wdsp->status = 0x00;

	/* clean up the components */
	for (idx = 0; idx < WDSP_CMPNT_TYPE_MAX; idx++) {
		cmpnt = WDSP_GET_COMPONENT(wdsp, idx);
		cmpnt->cdev = NULL;
		cmpnt->ops = NULL;
		cmpnt->priv_data = NULL;
	}
}

static const struct component_master_ops wdsp_master_ops = {
	.bind = wdsp_mgr_bind,
	.unbind = wdsp_mgr_unbind,
};

static void *wdsp_mgr_parse_phandle(struct wdsp_mgr_priv *wdsp,
				    int index)
{
	struct device *mdev = wdsp->mdev;
	struct device_node *np;
	struct wdsp_cmpnt *cmpnt = NULL;
	struct of_phandle_args pargs;
	u32 value;
	int ret;

	ret = of_parse_phandle_with_fixed_args(mdev->of_node,
					      "qcom,wdsp-components", 1,
					      index, &pargs);
	if (ret) {
		WDSP_ERR(wdsp, "parse_phandle at index %d failed %d",
			 index, ret);
		return NULL;
	}

	np = pargs.np;
	value = pargs.args[0];

	if (value >= WDSP_CMPNT_TYPE_MAX) {
		WDSP_ERR(wdsp, "invalid phandle_arg to of_node %s", np->name);
		goto done;
	}

	cmpnt = WDSP_GET_COMPONENT(wdsp, value);
	if (cmpnt->np || cmpnt->cdev_name) {
		WDSP_ERR(wdsp, "cmpnt %d already added", value);
		cmpnt = NULL;
		goto done;
	}

	cmpnt->np = np;
	of_property_read_string(np, "qcom,wdsp-cmpnt-dev-name",
				&cmpnt->cdev_name);
done:
	of_node_put(np);
	return cmpnt;
}

static int wdsp_mgr_parse_dt_entries(struct wdsp_mgr_priv *wdsp)
{
	struct device *dev = wdsp->mdev;
	void *match_data;
	int ph_idx, ret;

	ret = of_property_read_string(dev->of_node, "qcom,img-filename",
				      &wdsp->img_fname);
	if (ret < 0) {
		WDSP_ERR(wdsp, "Reading property %s failed, error = %d",
			 "qcom,img-filename", ret);
		return ret;
	}

#ifdef GOOGLE_HOTWORD
	wdsp->img_fname  = "cpe_intl";
	pr_info("%s: using global wdsp fw: %s.\n", __func__, wdsp->img_fname);
#else
	pr_info("%s: using non-global wdsp fw: %s.\n", __func__, wdsp->img_fname);
#endif

	ret = of_count_phandle_with_args(dev->of_node,
					 "qcom,wdsp-components",
					 NULL);
	if (ret == -ENOENT) {
		WDSP_ERR(wdsp, "Property %s not defined in DT",
			 "qcom,wdsp-components");
		goto done;
	} else if (ret != WDSP_CMPNT_TYPE_MAX * 2) {
		WDSP_ERR(wdsp, "Invalid phandle + arg count %d, expected %d",
			 ret, WDSP_CMPNT_TYPE_MAX * 2);
		ret = -EINVAL;
		goto done;
	}

	ret = 0;

	for (ph_idx = 0; ph_idx < WDSP_CMPNT_TYPE_MAX; ph_idx++) {

		match_data = wdsp_mgr_parse_phandle(wdsp, ph_idx);
		if (!match_data) {
			WDSP_ERR(wdsp, "component not found at idx %d", ph_idx);
			ret = -EINVAL;
			goto done;
		}

		component_match_add(dev, &wdsp->match,
				    wdsp_mgr_compare_of, match_data);
	}

done:
	return ret;
}

static int wdsp_mgr_probe(struct platform_device *pdev)
{
	struct wdsp_mgr_priv *wdsp;
	struct device *mdev = &pdev->dev;
	int ret;

	wdsp = devm_kzalloc(mdev, sizeof(*wdsp), GFP_KERNEL);
	if (!wdsp)
		return -ENOMEM;
	wdsp->mdev = mdev;
	wdsp->seg_list = devm_kzalloc(mdev, sizeof(struct list_head),
				      GFP_KERNEL);
	if (!wdsp->seg_list) {
		devm_kfree(mdev, wdsp);
		return -ENOMEM;
	}

	ret = wdsp_mgr_parse_dt_entries(wdsp);
	if (ret)
		goto err_dt_parse;

	INIT_WORK(&wdsp->load_fw_work, wdsp_load_fw_image);
	INIT_LIST_HEAD(wdsp->seg_list);
	mutex_init(&wdsp->api_mutex);
	mutex_init(&wdsp->ssr_mutex);
	wdsp->ssr_type = WDSP_SSR_TYPE_NO_SSR;
	wdsp->ready_status = WDSP_SSR_STATUS_READY;
	INIT_WORK(&wdsp->ssr_work, wdsp_ssr_work_fn);
	init_completion(&wdsp->ready_compl);
	arch_setup_dma_ops(wdsp->mdev, 0, 0, NULL, 0);
	dev_set_drvdata(mdev, wdsp);

	ret = component_master_add_with_match(mdev, &wdsp_master_ops,
					      wdsp->match);
	if (ret < 0) {
		WDSP_ERR(wdsp, "Failed to add master, err = %d", ret);
		goto err_master_add;
	}

	return 0;

err_master_add:
	mutex_destroy(&wdsp->api_mutex);
	mutex_destroy(&wdsp->ssr_mutex);
err_dt_parse:
	devm_kfree(mdev, wdsp->seg_list);
	devm_kfree(mdev, wdsp);
	dev_set_drvdata(mdev, NULL);

	return ret;
}

static int wdsp_mgr_remove(struct platform_device *pdev)
{
	struct device *mdev = &pdev->dev;
	struct wdsp_mgr_priv *wdsp = dev_get_drvdata(mdev);

	component_master_del(mdev, &wdsp_master_ops);

	mutex_destroy(&wdsp->api_mutex);
	mutex_destroy(&wdsp->ssr_mutex);
	devm_kfree(mdev, wdsp->seg_list);
	devm_kfree(mdev, wdsp);
	dev_set_drvdata(mdev, NULL);

	return 0;
};

static const struct of_device_id wdsp_mgr_dt_match[] = {
	{.compatible = "qcom,wcd-dsp-mgr" },
	{ }
};

static struct platform_driver wdsp_mgr_driver = {
	.driver = {
		.name = "wcd-dsp-mgr",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(wdsp_mgr_dt_match),
	},
	.probe = wdsp_mgr_probe,
	.remove = wdsp_mgr_remove,
};

int wcd_dsp_mgr_init(void)
{
	return platform_driver_register(&wdsp_mgr_driver);
}

void wcd_dsp_mgr_exit(void)
{
	platform_driver_unregister(&wdsp_mgr_driver);
}

MODULE_DESCRIPTION("WCD DSP manager driver");
MODULE_DEVICE_TABLE(of, wdsp_mgr_dt_match);
MODULE_LICENSE("GPL v2");
