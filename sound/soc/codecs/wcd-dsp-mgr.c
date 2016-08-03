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
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stringify.h>
#include <linux/of.h>
#include <linux/component.h>
#include <sound/wcd-dsp-mgr.h>
#include "wcd-dsp-utils.h"

/* Forward declarations */
static char *wdsp_get_cmpnt_type_string(enum wdsp_cmpnt_type);

/* Component related macros */
#define WDSP_GET_COMPONENT(wdsp, x) (&(wdsp->cmpnts[x]))
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
};

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
	if (IS_ERR_VALUE(ret))
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
	int ret;

	ctl = WDSP_GET_COMPONENT(wdsp, WDSP_CMPNT_CONTROL);

	if (type == WDSP_ELF_FLAG_RE) {
		pre = WDSP_EVENT_PRE_DLOAD_CODE;
		post = WDSP_EVENT_POST_DLOAD_CODE;
	} else if (type == WDSP_ELF_FLAG_WRITE) {
		pre = WDSP_EVENT_PRE_DLOAD_DATA;
		post = WDSP_EVENT_POST_DLOAD_DATA;
	} else {
		WDSP_ERR(wdsp, "Invalid type %u", type);
		return -EINVAL;
	}

	ret = wdsp_get_segment_list(ctl->cdev, wdsp->img_fname,
				    type, wdsp->seg_list, &wdsp->base_addr);
	if (IS_ERR_VALUE(ret) ||
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
		if (IS_ERR_VALUE(ret)) {
			wdsp_broadcast_event_downseq(wdsp,
						     WDSP_EVENT_DLOAD_FAILED,
						     NULL);
			goto dload_error;
		}
	}

	/* Notify all components that image is downloaded */
	wdsp_broadcast_event_downseq(wdsp, post, NULL);

dload_error:
	wdsp_flush_segment_list(wdsp->seg_list);
done:
	return ret;
}

static void wdsp_load_fw_image(struct work_struct *work)
{
	struct wdsp_mgr_priv *wdsp;
	struct wdsp_cmpnt *cmpnt;
	int ret, idx;

	wdsp = container_of(work, struct wdsp_mgr_priv, load_fw_work);
	if (!wdsp) {
		pr_err("%s: Invalid private_data\n", __func__);
		goto done;
	}

	/* Initialize the components first */
	ret = wdsp_init_components(wdsp);
	if (IS_ERR_VALUE(ret))
		goto done;

	/* Set init done status */
	WDSP_SET_STATUS(wdsp, WDSP_STATUS_INITIALIZED);

	/* Download the read-execute sections of image */
	ret = wdsp_download_segments(wdsp, WDSP_ELF_FLAG_RE);
	if (IS_ERR_VALUE(ret)) {
		WDSP_ERR(wdsp, "Error %d to download code sections", ret);
		for (idx = 0; idx < WDSP_CMPNT_TYPE_MAX; idx++) {
			cmpnt = WDSP_GET_COMPONENT(wdsp, idx);
			if (cmpnt->ops && cmpnt->ops->deinit)
				cmpnt->ops->deinit(cmpnt->cdev,
						   cmpnt->priv_data);
		}
		WDSP_CLEAR_STATUS(wdsp, WDSP_STATUS_INITIALIZED);
	}

	WDSP_SET_STATUS(wdsp, WDSP_STATUS_CODE_DLOADED);
done:
	return;
}

static int wdsp_enable_dsp(struct wdsp_mgr_priv *wdsp)
{
	int ret;

	/* Make sure wdsp is in good state */
	if (!WDSP_STATUS_IS_SET(wdsp, WDSP_STATUS_CODE_DLOADED)) {
		WDSP_ERR(wdsp, "WDSP in invalid state 0x%x", wdsp->status);
		ret = -EINVAL;
		goto done;
	}

	/* Download the read-write sections of image */
	ret = wdsp_download_segments(wdsp, WDSP_ELF_FLAG_WRITE);
	if (IS_ERR_VALUE(ret)) {
		WDSP_ERR(wdsp, "Data section download failed, err = %d", ret);
		goto done;
	}

	WDSP_SET_STATUS(wdsp, WDSP_STATUS_DATA_DLOADED);

	wdsp_broadcast_event_upseq(wdsp, WDSP_EVENT_PRE_BOOTUP, NULL);

	ret = wdsp_unicast_event(wdsp, WDSP_CMPNT_CONTROL,
				 WDSP_EVENT_DO_BOOT, NULL);
	if (IS_ERR_VALUE(ret)) {
		WDSP_ERR(wdsp, "Failed to boot dsp, err = %d", ret);
		WDSP_CLEAR_STATUS(wdsp, WDSP_STATUS_DATA_DLOADED);
		goto done;
	}

	wdsp_broadcast_event_downseq(wdsp, WDSP_EVENT_POST_BOOTUP, NULL);
	WDSP_SET_STATUS(wdsp, WDSP_STATUS_BOOTED);
done:
	return ret;
}

static int wdsp_disable_dsp(struct wdsp_mgr_priv *wdsp)
{
	int ret;

	/* Make sure wdsp is in good state */
	if (!WDSP_STATUS_IS_SET(wdsp, WDSP_STATUS_BOOTED)) {
		WDSP_ERR(wdsp, "wdsp in invalid state 0x%x", wdsp->status);
		ret = -EINVAL;
		goto done;
	}

	wdsp_broadcast_event_downseq(wdsp, WDSP_EVENT_PRE_SHUTDOWN, NULL);
	ret = wdsp_unicast_event(wdsp, WDSP_CMPNT_CONTROL,
				 WDSP_EVENT_DO_SHUTDOWN, NULL);
	if (IS_ERR_VALUE(ret)) {
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

static int wdsp_intr_handler(struct device *wdsp_dev,
			     enum wdsp_intr intr)
{
	struct wdsp_mgr_priv *wdsp;
	int ret;

	if (!wdsp_dev)
		return -EINVAL;

	wdsp = dev_get_drvdata(wdsp_dev);
	WDSP_MGR_MUTEX_LOCK(wdsp, wdsp->api_mutex);

	switch (intr) {
	case WDSP_IPC1_INTR:
		ret = wdsp_unicast_event(wdsp, WDSP_CMPNT_IPC,
					 WDSP_EVENT_IPC1_INTR, NULL);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (IS_ERR_VALUE(ret))
		WDSP_ERR(wdsp, "handling intr %d failed with error %d",
			 intr, ret);
	WDSP_MGR_MUTEX_UNLOCK(wdsp, wdsp->api_mutex);

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

	if (IS_ERR_VALUE(ret))
		WDSP_DBG(wdsp, "wdsp %s failed, err = %d",
			 vote ? "enable" : "disable", ret);

done:
	WDSP_MGR_MUTEX_UNLOCK(wdsp, wdsp->api_mutex);
	return ret;
}

static int wdsp_suspend(struct device *wdsp_dev)
{
	return 0;
}

static int wdsp_resume(struct device *wdsp_dev)
{
	return 0;
}

static struct wdsp_mgr_ops wdsp_ops = {
	.register_cmpnt_ops = wdsp_register_cmpnt_ops,
	.get_dev_for_cmpnt = wdsp_get_dev_for_cmpnt,
	.intr_handler = wdsp_intr_handler,
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

static int wdsp_mgr_bind(struct device *dev)
{
	struct wdsp_mgr_priv *wdsp = dev_get_drvdata(dev);
	struct wdsp_cmpnt *cmpnt;
	int ret, idx;

	wdsp->ops = &wdsp_ops;

	ret = component_bind_all(dev, wdsp->ops);
	if (IS_ERR_VALUE(ret))
		WDSP_ERR(wdsp, "component_bind_all failed %d\n", ret);

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

	component_unbind_all(dev, wdsp->ops);

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
	if (IS_ERR_VALUE(ret)) {
		WDSP_ERR(wdsp, "Reading property %s failed, error = %d",
			 "qcom,img-filename", ret);
		return ret;
	}

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
	dev_set_drvdata(mdev, wdsp);

	ret = component_master_add_with_match(mdev, &wdsp_master_ops,
					      wdsp->match);
	if (IS_ERR_VALUE(ret)) {
		WDSP_ERR(wdsp, "Failed to add master, err = %d", ret);
		goto err_master_add;
	}

	return 0;

err_master_add:
	mutex_destroy(&wdsp->api_mutex);
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
module_platform_driver(wdsp_mgr_driver);

MODULE_DESCRIPTION("WCD DSP manager driver");
MODULE_DEVICE_TABLE(of, wdsp_mgr_dt_match);
MODULE_LICENSE("GPL v2");
