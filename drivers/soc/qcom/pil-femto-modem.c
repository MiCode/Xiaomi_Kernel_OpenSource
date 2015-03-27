/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>

#include <asm/page.h>
#include <asm/cacheflush.h>

#include "peripheral-loader.h"
#include "pil-q6v5.h"
#include "pil-msa.h"

#define MAX_MODEM_ID		4
#define MAX_NUM_Q6		4
#define Q6_FREQ_COL_MAX		7
#define Q6_FREQ_COL_MIN		0
#define Q6_FREQ_COL_MASK	0x00000007
#define Q6_FREQ_ROW_MAX		5
#define Q6_FREQ_ROW_MIN		0
#define Q6_FREQ_ROW_MASK	0x0000000F
#define Q6_FREQ_ROW_SHIFT	28
#define Q6_FREQ_COL_SHIFT(q6)	(Q6_FREQ_ROW_SHIFT - (((q6) + 1) * 3))

/* These macros assist with creating sysfs attributes */
#define MAKE_RO_ATTR(_name, _ptr, _index)\
	_ptr->kobj_attr_##_name.attr.name = __stringify(_name);\
	_ptr->kobj_attr_##_name.attr.mode = S_IRUSR;\
	_ptr->kobj_attr_##_name.show = show_##_name;\
	_ptr->kobj_attr_##_name.store = NULL;\
	sysfs_attr_init(&_ptr->kobj_attr_##_name.attr);\
	_ptr->attr_grp.attrs[_index] = &_ptr->kobj_attr_##_name.attr;

#define MAKE_RW_ATTR(_name, _ptr, _index)\
	_ptr->kobj_attr_##_name.attr.name = __stringify(_name);\
	_ptr->kobj_attr_##_name.attr.mode = S_IRUSR|S_IWUSR; \
	_ptr->kobj_attr_##_name.show = show_##_name;\
	_ptr->kobj_attr_##_name.store = store_##_name;\
	sysfs_attr_init(&_ptr->kobj_attr_##_name.attr);\
	_ptr->attr_grp.attrs[_index] = &_ptr->kobj_attr_##_name.attr;

enum modem_status {
	MODEM_STATUS_OFFLINE = 0,
	MODEM_STATUS_MBA_LOADING,
	MODEM_STATUS_MBA_RUNNING,
	MODEM_STATUS_MBA_ERROR,
	MODEM_STATUS_PMI_LOADING,
	MODEM_STATUS_PMI_ERROR,
	MODEM_STATUS_PMI_LOADED,
	MODEM_STATUS_PMI_NOT_FOUND,
	MODEM_STATUS_ADVANCE_FAILED,
	MODEM_STATUS_Q6_FREQ_ERROR,
	MODEM_STATUS_LAST
};

static char *modem_status_str[MODEM_STATUS_LAST] = {
	"OFFLINE",
	"MBA LOADING",
	"MBA RUNNING",
	"MBA ERROR",
	"PMI LOADING",
	"PMI ERROR",
	"PMI LOADED",
	"PMI NOT FOUND",
	"ADVANCE FAILED",
	"Q6 FREQ ERROR"
};

struct modem_pil_data {
	struct modem_data image;
	const char *name;
	u32 num_images;
	u32 id;

	/* sysfs */
	struct kobject *kobj;
	struct attribute_group attr_grp;
	struct kobj_attribute kobj_attr_status;
	struct kobj_attribute kobj_attr_imgs_loaded;
	struct kobj_attribute kobj_attr_last_img_loaded;
	struct kobj_attribute kobj_attr_img_loading;
	u32 status;
	u32 imgs_loaded;
	int last_img_loaded;
	int img_loading;
};

struct femto_modem_data {
	struct q6v5_data *q6;
	struct modem_pil_data *modem;
	u32 max_num_modems;
	u32 disc_modems;
	u8  change_q6_freqs;
	u32 q6_freq_row;
	u32 q6_freq_col[MAX_NUM_Q6];

	/* sysfs */
	struct kobject *kobj;
	struct attribute_group attr_grp;
	struct kobj_attribute kobj_attr_mba_status;
	struct kobj_attribute kobj_attr_enable;
	struct kobj_attribute kobj_attr_q6_freq_row;
	struct kobj_attribute kobj_attr_q6_freq_col;

	u32 mba_status;
	u8  enable;
};

#define SET_SYSFS_VALUE(_ptr, _attr, _val)\
	do {\
		_ptr->_attr = (_val);\
		sysfs_notify(_ptr->kobj, NULL, __stringify(_attr));\
	} while (0)

#define RMB_MBA_COMMAND			0x08
#define RMB_MBA_STATUS			0x0C
#define CMD_RMB_ADVANCE			0x03
#define CMD_MBA_SET_Q6_FREQS		0x04
#define STATUS_RMB_UPDATE_ACK		0x06
#define STATUS_SET_FREQS_ACK		0x07
#define RMB_ADVANCE_COMPLETE		0xFE
#define POLL_INTERVAL_US		50
#define TIMEOUT_US			1000000

static DECLARE_WAIT_QUEUE_HEAD(fw_map_wq);
static DEFINE_SEMAPHORE(fw_map_mutex);
static void *fw_bounce_buf;
static void *fw_io_buf;

static void *pil_femto_modem_map_fw_mem(phys_addr_t paddr, size_t size, void *d)
{
	for (;;) {
		wait_event_interruptible(fw_map_wq, fw_bounce_buf == NULL);

		if (down_interruptible(&fw_map_mutex))
			return NULL;

		if (fw_bounce_buf == NULL)
			break;

		up(&fw_map_mutex);
	}

	fw_bounce_buf = vmalloc(size);
	if (fw_bounce_buf == NULL) {
		pr_err("%s - Cannot allocate memory\n", __func__);
		goto ret_err;
	}

	fw_io_buf = ioremap(paddr, size);
	if (fw_io_buf == NULL) {
		pr_err("%s - Cannot map firmware memory\n", __func__);
		goto ret_err_after_free;
	}

	up(&fw_map_mutex);
	return fw_bounce_buf;

ret_err_after_free:
	kfree(fw_bounce_buf);
	fw_bounce_buf = NULL;
ret_err:
	up(&fw_map_mutex);
	return NULL;
}

static void pil_femto_modem_unmap_fw_mem(void *vaddr, size_t size, void *data)
{
	u8 *src, *dst;

	down(&fw_map_mutex);

	if (vaddr == NULL)
		goto ret_err;

	if (vaddr != fw_bounce_buf) {
		pr_err("%s - Cannot unmap that was not mapped\n", __func__);
		goto ret_err;
	}

	src = (u8 *) fw_bounce_buf;
	dst = (u8 *) fw_io_buf;
	while (size >= 4) {
		*(u32 *) dst = *(u32 *) src;
		dst += 4;
		src += 4;
		size -= 4;
	}
	while (size-- > 0)
		*dst++ = *src++;

	vfree(fw_bounce_buf);
	fw_bounce_buf = NULL;
	iounmap(fw_io_buf);
	fw_io_buf = NULL;

	isb();

ret_err:
	up(&fw_map_mutex);
	wake_up_interruptible(&fw_map_wq);
}

static int pil_femto_modem_send_rmb_advance(void __iomem *rmb_base, u32 id)
{
	int ret;
	u32 cmd = CMD_RMB_ADVANCE;
	int status;

	if (!rmb_base)
		return -EINVAL;

	/* Prepare the command */
	cmd |= id << 8;

	/* Sent the MBA command */
	writel_relaxed(cmd, rmb_base + RMB_MBA_COMMAND);

	/* Wait for MBA status. */
	ret = readl_poll_timeout(rmb_base + RMB_MBA_STATUS, status,
		((status < 0) || (status == STATUS_RMB_UPDATE_ACK)),
		POLL_INTERVAL_US, TIMEOUT_US);

	if (ret)
		return ret;

	if (status != STATUS_RMB_UPDATE_ACK)
		return -EINVAL;

	return ret;
}

static int pil_femto_modem_send_mba_set_q6_freqs(struct femto_modem_data *drv)
{
	int ret;
	u32 cmd = CMD_MBA_SET_Q6_FREQS;
	int status;
	u32 q6;

	if (!drv)
		return -EINVAL;

	if (!drv->q6)
		return -EINVAL;

	/* The format of the upper 16 bits of the command word is as follows:
	 * ___________________________________________________________
	 * | 31 30 29 28 | 27 26 25 | 24 23 22 | 21 20 19 | 18 17 16 |
	 * |-------------|----------|----------|----------|----------|
	 * | Q6 row num  | Q6 0 col | Q6 1 col | Q6 2 col | Q6 3 col |
	 * -----------------------------------------------------------
	 *
	 * The lower 16 bits contains the command id.
	 */
	cmd |= (drv->q6_freq_row & Q6_FREQ_ROW_MASK) <<
		Q6_FREQ_ROW_SHIFT;
	for (q6 = 0; q6 < MAX_NUM_Q6; q6++) {
		cmd |= (drv->q6_freq_col[q6] & Q6_FREQ_COL_MASK) <<
			Q6_FREQ_COL_SHIFT(q6);
	}

	/* Sent the MBA command */
	writel_relaxed(cmd, drv->q6->rmb_base + RMB_MBA_COMMAND);

	/* Wait for MBA status. */
	ret = readl_poll_timeout(drv->q6->rmb_base + RMB_MBA_STATUS, status,
		((status < 0) || (status == STATUS_SET_FREQS_ACK)),
		POLL_INTERVAL_US, TIMEOUT_US);

	if (ret)
		return ret;

	if (status != STATUS_SET_FREQS_ACK)
		return -EINVAL;

	return ret;
}

static int pil_femto_modem_stop(struct femto_modem_data *drv)
{
	if (!drv)
		return -EINVAL;

	/* Only need to shutdown the Q6 PIL descriptor, because shutting down
	 * the others does nothing.
	 */
	pil_shutdown(&drv->q6->desc);
	return 0;
}

static int pil_femto_modem_start(struct femto_modem_data *drv)
{
	int ret;
	u32 index;

	/* MBA must load, else we can't load any firmware images */
	SET_SYSFS_VALUE(drv, mba_status, MODEM_STATUS_MBA_LOADING);
	drv->q6->desc.fw_name = drv->q6->desc.name;
	ret = pil_boot(&drv->q6->desc);
	if (ret) {
		SET_SYSFS_VALUE(drv, mba_status, MODEM_STATUS_MBA_ERROR);
		return ret;
	}
	SET_SYSFS_VALUE(drv, mba_status, MODEM_STATUS_MBA_RUNNING);

	/* Set the q6 frequencies, if needed */
	if (drv->change_q6_freqs) {
		ret = pil_femto_modem_send_mba_set_q6_freqs(drv);
		if (ret) {
			SET_SYSFS_VALUE(drv, mba_status,
					MODEM_STATUS_Q6_FREQ_ERROR);
			return ret;
		}
	}

	/* Load the other modem images, if possible. */
	for (index = 0; index < drv->max_num_modems; index++) {
		struct modem_pil_data *modem = &drv->modem[index];
		int img;
		char *pmi_name = kzalloc((strlen(modem->name) + 5), GFP_KERNEL);
		struct modem_data *image = &modem->image;

		if (!pmi_name) {
			ret = -ENOMEM;
			SET_SYSFS_VALUE(modem, status, MODEM_STATUS_PMI_ERROR);
			pil_shutdown(&drv->q6->desc);
			break;
		}

		/* Initialize stats */
		SET_SYSFS_VALUE(modem, imgs_loaded, 0);
		SET_SYSFS_VALUE(modem, last_img_loaded, -1);
		SET_SYSFS_VALUE(modem, img_loading, -1);

		/* Try to load each image. */
		for (img = 0; img < modem->num_images; img++) {
			SET_SYSFS_VALUE(modem, status,
				MODEM_STATUS_PMI_LOADING);
			SET_SYSFS_VALUE(modem, img_loading, img);

			/* The filename for each image is name_nn, where nn is
			 * the 2 digit image index.
			 */
			pmi_name[0] = '\0';
			snprintf(pmi_name, (strlen(modem->name) + 5), "%s_%02u",
				 modem->name, img);

			/* Have to change the descriptor name so it boots the
			 * correct file.
			 */
			image->desc.name = image->desc.fw_name = pmi_name;

			/* Try to boot the image. */
			ret = pil_boot(&image->desc);
			if (ret) {
				SET_SYSFS_VALUE(modem, status,
					MODEM_STATUS_PMI_NOT_FOUND);
				if (!modem->id) {
					/* This is a catastrophic failure.
					 * Modem 0 must load.
					 */
					pil_shutdown(&drv->q6->desc);
					SET_SYSFS_VALUE(drv, mba_status,
						MODEM_STATUS_MBA_ERROR);
					break;
				} else
					/* This image didn't load, but it's not
					 * a catastrophic failure; continue
					 * loading.
					 */
					ret = 0;
			} else {
				/* Update stats */
				SET_SYSFS_VALUE(modem, last_img_loaded, img);
				SET_SYSFS_VALUE(modem, img_loading, -1);
				SET_SYSFS_VALUE(modem, imgs_loaded,
					modem->imgs_loaded + 1);
				SET_SYSFS_VALUE(modem, status,
					MODEM_STATUS_PMI_LOADED);
			}
		}

		if (modem->imgs_loaded == 0)
			SET_SYSFS_VALUE(modem, status, MODEM_STATUS_PMI_ERROR);

		/* Free the allocated name */
		kfree(pmi_name);

		if (index == (drv->max_num_modems - 1)) {
			/* Tell the MBA this was the last RMB */
			ret = pil_femto_modem_send_rmb_advance(
				image->rmb_base,
				RMB_ADVANCE_COMPLETE);

			/* If the advance fails, the MBA is in an error state */
			if (ret)
				SET_SYSFS_VALUE(drv, mba_status,
					MODEM_STATUS_MBA_ERROR);
		} else {
			/* Tell the MBA to move to the next RMB.
			 * Note that the MBA needs the actual id of the
			 * modem specified in the device tree,
			 * not the logical index.
			 */
			ret = pil_femto_modem_send_rmb_advance(
				image->rmb_base,
				drv->modem[(index + 1)].id);

			if (ret) {
				/* This is a catastrophic failure; we've
				 * gotten out of sync with the MBA.
				 */
				SET_SYSFS_VALUE(modem, status,
					MODEM_STATUS_ADVANCE_FAILED);
				SET_SYSFS_VALUE(drv, mba_status,
					MODEM_STATUS_MBA_ERROR);

				pil_shutdown(&drv->q6->desc);
				break;
			}
		}
	}

	return ret;
}

/*
 * The following are for sysfs
 */
#define TO_DRV(attr, elem) \
	container_of(attr, struct femto_modem_data, elem)
static ssize_t show_mba_status(struct kobject *kobj,
			       struct kobj_attribute *attr,
			       char *buf)
{
	struct femto_modem_data *drv = TO_DRV(attr, kobj_attr_mba_status);

	if (!drv)
		return -EINVAL;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 modem_status_str[drv->mba_status]);
}

static ssize_t store_enable(struct kobject *kobj,
			    struct kobj_attribute *attr,
			    const char *buf,
			    size_t count)
{
	struct femto_modem_data *drv = TO_DRV(attr, kobj_attr_enable);
	u8 enable_val;
	int ret;

	if (!drv)
		return -EINVAL;

	if (kstrtou8(buf, 0, &enable_val))
		return -EINVAL;

	if (enable_val > 1)
		return -EINVAL;

	/* Only start/stop if it's different. */
	if (enable_val != drv->enable) {
		if (enable_val)
			ret = pil_femto_modem_start(drv);
		else
			ret = pil_femto_modem_stop(drv);
		if (ret)
			return ret;
		SET_SYSFS_VALUE(drv, enable, enable_val);
	}

	return count;
}

static ssize_t show_enable(struct kobject *kobj,
			   struct kobj_attribute *attr,
			   char *buf)
{
	struct femto_modem_data *drv = TO_DRV(attr, kobj_attr_enable);

	if (!drv)
		return -EINVAL;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
		(drv->enable ? "ENABLED" : "DISABLED"));
}

static ssize_t show_q6_freq_row(struct kobject *kobj,
			   struct kobj_attribute *attr,
			   char *buf)
{
	struct femto_modem_data *drv = TO_DRV(attr, kobj_attr_q6_freq_row);

	if (!drv)
		return -EINVAL;

	return scnprintf(buf, PAGE_SIZE, "%u\n", drv->q6_freq_row);
}

static ssize_t show_q6_freq_col(struct kobject *kobj,
			   struct kobj_attribute *attr,
			   char *buf)
{
	char freq_str[16];
	struct femto_modem_data *drv = TO_DRV(attr, kobj_attr_q6_freq_col);
	int index;
	int len = 0;

	if (!drv)
		return -EINVAL;

	for (index = 0; index < MAX_NUM_Q6; index++)
		len += snprintf(freq_str + len, 16, "%u ",
				drv->q6_freq_col[index]);

	return scnprintf(buf, PAGE_SIZE, "%s\n", freq_str);
}

#define TO_MODEM(attr, elem) \
	container_of(attr, struct modem_pil_data, elem)

static ssize_t show_status(struct kobject *kobj,
			   struct kobj_attribute *attr,
			   char *buf)
{
	struct modem_pil_data *modem = TO_MODEM(attr, kobj_attr_status);

	if (!modem)
		return -EINVAL;

	return scnprintf(buf, PAGE_SIZE, "%s\n",
		modem_status_str[modem->status]);
}

static ssize_t show_imgs_loaded(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	struct modem_pil_data *modem = TO_MODEM(attr, kobj_attr_imgs_loaded);

	if (!modem)
		return -EINVAL;

	return scnprintf(buf, PAGE_SIZE, "%u\n", modem->imgs_loaded);
}

static ssize_t show_last_img_loaded(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    char *buf)
{
	struct modem_pil_data *modem =
		TO_MODEM(attr, kobj_attr_last_img_loaded);

	if (!modem)
		return -EINVAL;

	if (modem->last_img_loaded < 0)
		return scnprintf(buf, PAGE_SIZE, "NONE\n");
	else
		return scnprintf(buf, PAGE_SIZE, "%s_%02d\n", modem->name,
			modem->last_img_loaded);
}

static ssize_t show_img_loading(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	struct modem_pil_data *modem = TO_MODEM(attr, kobj_attr_img_loading);

	if (!modem)
		return -EINVAL;

	if (modem->img_loading < 0)
		return scnprintf(buf, PAGE_SIZE, "NONE\n");
	else
		return scnprintf(buf, PAGE_SIZE, "%s_%02d\n", modem->name,
			modem->img_loading);
}

static int pil_femto_modem_create_sysfs(struct femto_modem_data *drv)
{
	int ret = 0;

	if (!drv)
		return -EINVAL;

	/* Create the sysfs kobj */
	drv->kobj = kobject_create_and_add("femto_modem", kernel_kobj);
	if (!drv->kobj)
		return -ENOMEM;

	/* Allocate memory for the group */
	drv->attr_grp.attrs =
		kzalloc(sizeof(struct attribute *) * 4, GFP_KERNEL);

	if (!drv->attr_grp.attrs) {
		ret = -ENOMEM;
		goto cleanup_kobj;
	}

	/* Create the attributes and add them to the group */
	MAKE_RO_ATTR(mba_status, drv, 0);
	MAKE_RW_ATTR(enable, drv, 1);

	/* Add these only if they are specified */
	if (drv->change_q6_freqs) {
		MAKE_RO_ATTR(q6_freq_row, drv, 2);
		MAKE_RO_ATTR(q6_freq_col, drv, 3);
	}

	/* Create sysfs group*/
	ret = sysfs_create_group(drv->kobj, &drv->attr_grp);
	if (ret)
		goto cleanup_grp;

	drv->mba_status = MODEM_STATUS_OFFLINE;
	drv->enable = 0;

	return ret;

cleanup_grp:
	kzfree(drv->attr_grp.attrs);
	drv->attr_grp.attrs = NULL;

cleanup_kobj:
	kobject_put(drv->kobj);
	drv->kobj = NULL;

	return ret;
}

static int pil_femto_modem_desc_create_sysfs(struct femto_modem_data *drv,
					     struct modem_pil_data *modem)
{
	int ret = 0;
	char sysfs_dirname_str[10];

	if (!drv || !modem)
		return -EINVAL;

	/* Generate the name for the directory */
	sysfs_dirname_str[0] = '\0';
	snprintf(sysfs_dirname_str, sizeof(sysfs_dirname_str),
		 "modem%d", modem->id);

	/* Create the kobj as a child of the overall driver kobj */
	modem->kobj = kobject_create_and_add(sysfs_dirname_str, drv->kobj);
	if (!modem->kobj)
		return -ENOMEM;

	/* Allocate memory for the group */
	modem->attr_grp.attrs =
		kzalloc(sizeof(struct attribute *) * 4, GFP_KERNEL);

	if (!modem->attr_grp.attrs) {
		ret = -ENOMEM;
		goto cleanup_pil_kobj;
	}

	/* Create the attributes and add them to the group */
	MAKE_RO_ATTR(status, modem, 0);
	MAKE_RO_ATTR(imgs_loaded, modem, 1);
	MAKE_RO_ATTR(last_img_loaded, modem, 2);
	MAKE_RO_ATTR(img_loading, modem, 3);

	/* Create sysfs group*/
	ret = sysfs_create_group(modem->kobj, &modem->attr_grp);
	if (ret)
		goto cleanup_grp_mem;

	/* Initialize the values */
	SET_SYSFS_VALUE(modem, status, MODEM_STATUS_OFFLINE);
	SET_SYSFS_VALUE(modem, imgs_loaded, 0);
	SET_SYSFS_VALUE(modem, last_img_loaded, -1);
	SET_SYSFS_VALUE(modem, img_loading, -1);

	return ret;

cleanup_grp_mem:
	kzfree(modem->attr_grp.attrs);
	modem->attr_grp.attrs = NULL;

cleanup_pil_kobj:
	kobject_put(modem->kobj);
	modem->kobj = NULL;

	return ret;
}

static int pil_femto_modem_desc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *node;
	struct femto_modem_data *drv;
	struct modem_pil_data *modem;
	struct pil_desc *mba_desc;
	struct resource *res;
	struct modem_data *mba;
	void __iomem *rmb;
	int ret;
	u32 id;
	bool skip_entry;

	if (!dev->of_node) {
		dev_err(dev, "%s: device tree information missing\n", __func__);
		return -ENODEV;
	}

	node = dev->of_node;

	if (dev->parent == NULL) {
		dev_err(dev, "%s: parent device missing\n", __func__);
		return -ENODEV;
	}

	drv = dev_get_drvdata(dev->parent);
	if (drv == NULL) {
		dev_err(dev, "%s: driver data not found in parent device\n",
			__func__);
		return -ENODEV;
	}

	/* Make sure there are not more modems than specified */
	if (drv->disc_modems == drv->max_num_modems) {
		dev_err(dev, "%s: found more than max of %u modems.\n",
			__func__, drv->max_num_modems);
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "qcom,modem-id", &id);
	if (ret)
		return ret;

	/* Sanity check id */
	if (id > MAX_MODEM_ID)
		return -EINVAL;

	modem = &drv->modem[drv->disc_modems];
	modem->id = id;

	/* Retrieve the RMB base */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rmb_base");
	rmb = devm_request_and_ioremap(dev, res);
	if (!rmb)
		return -ENOMEM;

	/* The q6 structure should always point to modem[0] RMB regs */
	if (!modem->id && drv->q6)
		drv->q6->rmb_base = rmb;

	/* Retrieve the firmware name */
	ret = of_property_read_string(node, "qcom,firmware-name", &modem->name);
	if (ret)
		return ret;

	/* Retrieve the maximum number of images for this modem */
	ret = of_property_read_u32(node, "qcom,max-num-images",
		&modem->num_images);
	if (ret)
		return ret;

	/* Read the skip entry check flag */
	skip_entry = of_property_read_bool(node, "qcom,pil-skip-entry-check");

	/* Initialize the image attributes */
	mba = &modem->image;
	mba->rmb_base = rmb;

	/* Why isn't there one descriptor per file?  Because, the pil_desc_init
	 * function has a built-in maximum of 10, meaning it will fail after 10
	 * descriptors have been allocated.  Since there could be many more than
	 * 10 images loaded by this driver, it is necessary to have only one
	 * PIL descriptor per modem and reuse it for each image.
	 */
	mba_desc = &mba->desc;
	mba_desc->name = modem->name;
	mba_desc->dev = dev;
	mba_desc->ops = &pil_msa_femto_mba_ops;
	mba_desc->owner = THIS_MODULE;
	mba_desc->proxy_timeout = 0;
	mba_desc->flags = skip_entry ? PIL_SKIP_ENTRY_CHECK : 0;
	mba_desc->map_fw_mem = pil_femto_modem_map_fw_mem;
	mba_desc->unmap_fw_mem = pil_femto_modem_unmap_fw_mem;

	ret = pil_desc_init(mba_desc);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, modem);

	/* Create the sysfs attributes */
	ret = pil_femto_modem_desc_create_sysfs(drv, modem);
	if (ret)
		pil_desc_release(mba_desc);

	drv->disc_modems++;
	return ret;
}

static int pil_femto_modem_desc_driver_exit(
	struct platform_device *pdev)
{
	struct modem_pil_data *modem =
		(struct modem_pil_data *)platform_get_drvdata(pdev);
	if (modem)
		pil_desc_release(&modem->image.desc);
	return 0;
}

static int pil_femto_modem_driver_probe(
	struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct femto_modem_data *drv;
	struct q6v5_data *q6;
	struct pil_desc *q6_desc;
	struct device_node *p_node = pdev->dev.of_node;
	struct property *prop = NULL;
	const __be32 *p = NULL;
	int ret = 0;
	u32 index = 0;
	u32 value = 0;

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	platform_set_drvdata(pdev, drv);

	/* Retrieve the maximum number of modems */
	ret = of_property_read_u32(p_node, "qcom,max-num-modems",
		&drv->max_num_modems);
	if (ret)
		return ret;

	/* Max number of modems must be greater than zero */
	if (!drv->max_num_modems)
		return -EINVAL;

	/* Retrieve the q6 frequency row (if there) */
	drv->change_q6_freqs = 0;
	ret = of_property_read_u32(p_node, "qcom,q6-freq-row",
		&drv->q6_freq_row);

	if (!ret) {
		/* Validate q6 frequency row */
		if (drv->q6_freq_row > Q6_FREQ_ROW_MAX) {
			dev_err(dev, "Invalid q6 freq row value: %u\n",
				drv->q6_freq_row);
			return -EINVAL;
		}

		/* Retrieve the q6 frequency column values (if there) */
		of_property_for_each_u32(p_node, "qcom,q6-freq-col", prop, p,
			value) {
			if (index >= MAX_NUM_Q6) {
				dev_warn(dev,
					"Ignoring extra q6 freq col values.\n");
				break;
			}

			/* Validate q6 frequency column value */
			if (value > Q6_FREQ_COL_MAX) {
				dev_err(dev,
					"Invalid q6 %u freq column value: %u\n",
					index, value);
				return -EINVAL;
			}

			/* Value is valid, store it */
			drv->q6_freq_col[index] = value;
			index++;
		}

		if (!prop) {
			dev_err(dev, "Missing q6 frequency column values!\n");
			return -EINVAL;
		}

		drv->change_q6_freqs = 1;
	}

	/* Allocate memory for modem structs */
	drv->modem = devm_kzalloc(&pdev->dev,
		(sizeof(*(drv->modem)) * drv->max_num_modems), GFP_KERNEL);
	if (!drv->modem)
		return -ENOMEM;

	/* This controls the loading of the MBA firmware to Q6[0] */
	q6 = pil_q6v5_init(pdev);
	if (IS_ERR(q6))
		return PTR_ERR(q6);
	drv->q6 = q6;

	/* This is needed for legacy code.  Always on. */
	q6->self_auth = 1;

	q6_desc = &q6->desc;
	q6_desc->ops = &pil_msa_mss_ops;
	q6_desc->owner = THIS_MODULE;
	q6_desc->proxy_timeout = 0;
	q6_desc->map_fw_mem = pil_femto_modem_map_fw_mem;
	q6_desc->unmap_fw_mem = pil_femto_modem_unmap_fw_mem;

	/* For this target, PBL interactions are different. */
	pil_msa_mss_ops.proxy_vote = NULL;
	pil_msa_mss_ops.proxy_unvote = NULL;

	/* Initialize the number of discovered modems. */
	drv->disc_modems = 0;

	/* Parse the device tree to get RMB regs and filenames for each modem */
	ret = of_platform_populate(p_node, NULL, NULL, &pdev->dev);
	if (ret)
		return ret;

	/* Initialize the PIL descriptor */
	ret = pil_desc_init(q6_desc);

	if (ret)
		return ret;

	/* Create the sysfs attributes */
	ret = pil_femto_modem_create_sysfs(drv);

	if (ret)
		pil_desc_release(q6_desc);

	return ret;
}

static int pil_femto_modem_driver_exit(
	struct platform_device *pdev)
{
	struct femto_modem_data *drv = platform_get_drvdata(pdev);
	pil_desc_release(&drv->q6->desc);
	return 0;
}

static struct of_device_id pil_femto_modem_match_table[] = {
	{ .compatible = "qcom,pil-femto-modem" },
	{}
};
MODULE_DEVICE_TABLE(of, pil_femto_modem_match_table);

static struct of_device_id pil_femto_modem_desc_match_table[] = {
	{ .compatible = "qcom,pil-femto-modem-desc" },
	{}
};
MODULE_DEVICE_TABLE(of, pil_femto_modem_desc_match_table);

static struct platform_driver pil_femto_modem_driver = {
	.probe = pil_femto_modem_driver_probe,
	.remove = pil_femto_modem_driver_exit,
	.driver = {
		.name = "pil-femto-modem",
		.of_match_table = pil_femto_modem_match_table,
		.owner = THIS_MODULE,
	},
};

static struct platform_driver pil_femto_modem_desc_driver = {
	.probe = pil_femto_modem_desc_probe,
	.remove = pil_femto_modem_desc_driver_exit,
	.driver = {
		.name = "pil-femto-modem-desc",
		.of_match_table = pil_femto_modem_desc_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init pil_femto_modem_init(void)
{
	int result;
	result = platform_driver_register(&pil_femto_modem_driver);
	if (result)
		return result;
	return platform_driver_register(&pil_femto_modem_desc_driver);
}
module_init(pil_femto_modem_init);

static void __exit pil_femto_modem_exit(void)
{
	platform_driver_unregister(&pil_femto_modem_desc_driver);
	platform_driver_unregister(&pil_femto_modem_driver);
}
module_exit(pil_femto_modem_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Support for booting FSM99XX and FSM90XX modems");
