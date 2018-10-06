/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/reboot.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/ramdump.h>
#include <soc/qcom/subsystem_notif.h>
#include <linux/highmem.h>

#include "peripheral-loader.h"
#include "../../misc/qseecom_kernel.h"
#include "pil_bg_intf.h"
#include "bgcom_interface.h"

#define INVALID_GPIO	-1
#define NUM_GPIOS	4
#define SECURE_APP	"bgapp"
#define desc_to_data(d)	container_of(d, struct pil_bg_data, desc)
#define subsys_to_data(d) container_of(d, struct pil_bg_data, subsys_desc)
#define BG_RAMDUMP_SZ	0x00102000
#define BG_VERSION_SZ	32
#define BG_CRASH_IN_TWM	-2
/**
 * struct pil_bg_data
 * @qseecom_handle: handle of TZ app
 * @bg_queue: private queue to schedule worker threads for bottom half
 * @restart_work: work struct for executing ssr
 * @reboot_blk: notification block for reboot event
 * @subsys_desc: subsystem descriptor
 * @subsys: subsystem device pointer
 * @gpios: array to hold all gpio handle
 * @desc: PIL descriptor
 * @address_fw: address where firmware binaries loaded
 * @ramdump_dev: ramdump device pointer
 * @size_fw: size of bg firmware binaries
 * @errfatal_irq: irq number to indicate bg crash or shutdown
 * @status_irq: irq to indicate bg status
 * @app_status: status of tz app loading
 * @is_ready: Is BG chip up
 * @err_ready: The error ready signal
 */

struct pil_bg_data {
	struct qseecom_handle *qseecom_handle;
	struct workqueue_struct *bg_queue;
	struct work_struct restart_work;
	struct notifier_block reboot_blk;
	struct subsys_desc subsys_desc;
	struct subsys_device *subsys;
	unsigned int gpios[NUM_GPIOS];
	int errfatal_irq;
	int status_irq;
	struct pil_desc desc;
	phys_addr_t address_fw;
	void *ramdump_dev;
	u32 cmd_status;
	size_t size_fw;
	int app_status;
	bool is_ready;
	struct completion err_ready;
};

static irqreturn_t bg_status_change(int irq, void *dev_id);

/**
 * bg_app_shutdown_notify() - Toggle AP2BG err fatal gpio when
 * called by SSR framework.
 * @subsys: struct containing private BG data.
 *
 * Return: none.
 */
static void bg_app_shutdown_notify(const struct subsys_desc *subsys)
{
	struct pil_bg_data *bg_data = subsys_to_data(subsys);

	/* Disable irq if already BG is up */
	if (bg_data->is_ready) {
		disable_irq(bg_data->status_irq);
		disable_irq(bg_data->errfatal_irq);
		bg_data->is_ready = false;
	}
	/* Toggle AP2BG err fatal gpio here to inform apps err fatal event */
	if (gpio_is_valid(bg_data->gpios[2])) {
		pr_debug("Sending Apps shutdown signal\n");
		gpio_set_value(bg_data->gpios[2], 1);
	}
}

/**
 * bg_app_reboot_notify() - Toggle AP2BG err fatal gpio.
 * @nb: struct containing private BG data.
 *
 * Return: NOTIFY_DONE indicating success.
 */
static int bg_app_reboot_notify(struct notifier_block *nb,
		unsigned long code, void *unused)
{
	struct pil_bg_data *bg_data = container_of(nb,
					struct pil_bg_data, reboot_blk);

	/* Disable irq if already BG is up */
	if (bg_data->is_ready) {
		disable_irq(bg_data->status_irq);
		disable_irq(bg_data->errfatal_irq);
		bg_data->is_ready = false;
	}
	/* Toggle AP2BG err fatal gpio here to inform apps err fatal event */
	if (gpio_is_valid(bg_data->gpios[2])) {
		pr_debug("Sending reboot signal\n");
		gpio_set_value(bg_data->gpios[2], 1);
	}
	return NOTIFY_DONE;
}

/**
 * get_cmd_rsp_buffers() - Function sets cmd & rsp buffer pointers and
 *                         aligns buffer lengths
 * @hdl:	index of qseecom_handle
 * @cmd:	req buffer - set to qseecom_handle.sbuf
 * @cmd_len:	ptr to req buffer len
 * @rsp:	rsp buffer - set to qseecom_handle.sbuf + offset
 * @rsp_len:	ptr to rsp buffer len
 *
 * Return: Success always .
 */
static int get_cmd_rsp_buffers(struct qseecom_handle *handle, void **cmd,
			uint32_t *cmd_len, void **rsp, uint32_t *rsp_len)
{
	*cmd = handle->sbuf;
	if (*cmd_len & QSEECOM_ALIGN_MASK)
		*cmd_len = QSEECOM_ALIGN(*cmd_len);

	*rsp = handle->sbuf + *cmd_len;
	if (*rsp_len & QSEECOM_ALIGN_MASK)
		*rsp_len = QSEECOM_ALIGN(*rsp_len);

	return 0;
}

/**
 * pil_load_bg_tzapp() - Called to load TZ app.
 * @pbd: struct containing private BG data.
 *
 * Return: 0 on success. Error code on failure.
 */
static int pil_load_bg_tzapp(struct pil_bg_data *pbd)
{
	int rc;

	/* return success if already loaded */
	if (pbd->qseecom_handle && !pbd->app_status)
		return 0;
	/* Load the APP */
	rc = qseecom_start_app(&pbd->qseecom_handle, SECURE_APP, SZ_4K);
	if (rc < 0) {
		dev_err(pbd->desc.dev, "BG TZ app load failure\n");
		pbd->app_status = RESULT_FAILURE;
		return -EIO;
	}
	pbd->app_status = RESULT_SUCCESS;
	return 0;
}

/**
 * bgpil_tzapp_comm() - Function called to communicate with TZ APP.
 * @req:	struct containing command and parameters.
 *
 * Return: 0 on success. Error code on failure.
 */
static long bgpil_tzapp_comm(struct pil_bg_data *pbd,
				struct tzapp_bg_req *req)
{
	struct tzapp_bg_req *bg_tz_req;
	struct tzapp_bg_rsp *bg_tz_rsp;
	int rc, req_len, rsp_len;
	unsigned char *ascii;
	char fiwmare_version[100] = {'\0'};
	char ascii_string[5];

	/* Fill command structure */
	req_len = sizeof(struct tzapp_bg_req);
	rsp_len = sizeof(struct tzapp_bg_rsp);
	rc = get_cmd_rsp_buffers(pbd->qseecom_handle,
		(void **)&bg_tz_req, &req_len,
		(void **)&bg_tz_rsp, &rsp_len);
	if (rc)
		goto end;

	bg_tz_req->tzapp_bg_cmd = req->tzapp_bg_cmd;
	bg_tz_req->address_fw = req->address_fw;
	bg_tz_req->size_fw = req->size_fw;
	rc = qseecom_send_command(pbd->qseecom_handle,
		(void *)bg_tz_req, req_len, (void *)bg_tz_rsp, rsp_len);
	pr_debug("BG PIL qseecom returned with value 0x%x and status 0x%x\n",
		rc, bg_tz_rsp->status);
	if (rc || bg_tz_rsp->status)
		pbd->cmd_status = bg_tz_rsp->status;
	else
		pbd->cmd_status = 0;
	/* if last command sent was BG_VERSION print the version*/
	if (req->tzapp_bg_cmd == BGPIL_GET_BG_VERSION) {
		int i;

		pr_info("BG FW version ");
		for (i = 0; i < bg_tz_rsp->bg_info_len; i++) {
			pr_info("0x%08x ", bg_tz_rsp->bg_info[i]);
			ascii = (unsigned char *)&bg_tz_rsp->bg_info[i];
			snprintf(ascii_string, PAGE_SIZE, "%c%c%c%c", ascii[0],
						ascii[1], ascii[2], ascii[3]);
			strlcat(fiwmare_version, ascii_string,
						PAGE_SIZE);
		}
		pr_info("%s\n", fiwmare_version);
	}
end:
	return rc;
}

/**
 * wait_for_err_ready() - Called in power_up to wait for error ready.
 * Signal waiting function.
 * @bg_data: BG PIL private structure.
 *
 * Return: 0 on success. Error code on failure.
 */
static int wait_for_err_ready(struct pil_bg_data *bg_data)
{
	int ret;

	if ((!bg_data->status_irq))
		return 0;

	ret = wait_for_completion_timeout(&bg_data->err_ready,
			msecs_to_jiffies(10000));
	if (!ret) {
		pr_err("[%s]: Error ready timed out\n", bg_data->desc.name);
		return -ETIMEDOUT;
	}
	return 0;
}

/**
 * bg_powerup() - Called by SSR framework on userspace invocation.
 * does load tz app and call peripheral loader.
 * @subsys: struct containing private BG data.
 *
 * Return: 0 on success. Error code on failure.
 */
static int bg_powerup(const struct subsys_desc *subsys)
{
	struct pil_bg_data *bg_data = subsys_to_data(subsys);
	int ret;

	init_completion(&bg_data->err_ready);
	if (!bg_data->qseecom_handle) {
		ret = pil_load_bg_tzapp(bg_data);
		if (ret) {
			dev_err(bg_data->desc.dev,
				"%s: BG TZ app load failure\n",
				__func__);
			return ret;
		}
	}
	pr_debug("bgapp loaded\n");
	bg_data->desc.fw_name = subsys->fw_name;

	ret = devm_request_irq(bg_data->desc.dev, bg_data->status_irq,
		bg_status_change,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"bg2ap_status", bg_data);
	if (ret < 0) {
		dev_err(bg_data->desc.dev,
			"%s: BG2AP_STATUS IRQ#%d re registration failed, err=%d",
			__func__, bg_data->status_irq, ret);
			return ret;
	}
	disable_irq(bg_data->status_irq);

	/* Enable status and err fatal irqs */
	ret = pil_boot(&bg_data->desc);
	if (ret) {
		dev_err(bg_data->desc.dev,
			"%s: BG PIL Boot failed\n", __func__);
		return ret;
	}
	enable_irq(bg_data->status_irq);
	ret = wait_for_err_ready(bg_data);
	if (ret) {
		dev_err(bg_data->desc.dev,
			"[%s:%d]: Timed out waiting for error ready: %s!\n",
			current->comm, current->pid, bg_data->desc.name);
		return ret;
	}
	return ret;
}

/**
 * bg_shutdown() - Called by SSR framework on userspace invocation.
 * disable status interrupt to avoid spurious signal during PRM exit.
 * @subsys: struct containing private BG data.
 * @force_stop: unused
 *
 * Return: always success
 */
static int bg_shutdown(const struct subsys_desc *subsys, bool force_stop)
{
	struct pil_bg_data *bg_data = subsys_to_data(subsys);

	if (bg_data->is_ready) {
		disable_irq(bg_data->status_irq);
		devm_free_irq(bg_data->desc.dev, bg_data->status_irq, bg_data);
		disable_irq(bg_data->errfatal_irq);
		bg_data->is_ready = false;
	}
	return 0;
}

/**
 * bg_auth_metadata() - Called by Peripheral loader framework
 * send command to tz app for authentication of metadata.
 * @pil: pil descriptor.
 * @metadata: metadata load address
 * @size: size of metadata
 *
 * Return: 0 on success. Error code on failure.
 */
static int bg_auth_metadata(struct pil_desc *pil,
	const u8 *metadata, size_t size,
	phys_addr_t addr, void *sz)
{
	struct pil_bg_data *bg_data = desc_to_data(pil);
	struct tzapp_bg_req bg_tz_req;
	void *mdata_buf;
	dma_addr_t mdata_phys;
	unsigned long attrs = 0;
	struct device dev = {0};
	int ret;

	arch_setup_dma_ops(&dev, 0, 0, NULL, 0);

	dev.coherent_dma_mask = DMA_BIT_MASK(sizeof(dma_addr_t) * 8);
	attrs |= DMA_ATTR_STRONGLY_ORDERED;
	mdata_buf = dma_alloc_attrs(&dev, size,
			&mdata_phys, GFP_KERNEL, attrs);

	if (!mdata_buf) {
		pr_err("BG_PIL: Allocation for metadata failed.\n");
		return -ENOMEM;
	}

	/* Make sure there are no mappings in PKMAP and fixmap */
	kmap_flush_unused();
	kmap_atomic_flush_unused();

	memcpy(mdata_buf, metadata, size);

	bg_tz_req.tzapp_bg_cmd = BGPIL_AUTH_MDT;
	bg_tz_req.address_fw = (phys_addr_t)mdata_phys;
	bg_tz_req.size_fw = size;

	ret = bgpil_tzapp_comm(bg_data, &bg_tz_req);
	if (ret || bg_data->cmd_status) {
		dev_err(pil->dev,
			"%s: BGPIL_AUTH_MDT qseecom call failed\n",
				__func__);
		return bg_data->cmd_status;
	}
	dma_free_attrs(&dev, size, mdata_buf, mdata_phys, attrs);
	pr_debug("BG MDT Authenticated\n");
	return 0;
}

/**
 * bg_get_firmware_addr() - Called by Peripheral loader framework
 * to get address and size of bg firmware binaries.
 * @pil: pil descriptor.
 * @addr: fw load address
 * @size: size of fw
 *
 * Return: 0 on success.
 */
static int bg_get_firmware_addr(struct pil_desc *pil,
					phys_addr_t addr, size_t size)
{
	struct pil_bg_data *bg_data = desc_to_data(pil);

	bg_data->address_fw = addr;
	bg_data->size_fw = size;
	pr_debug("BG PIL loads firmware blobs at 0x%x with size 0x%x\n",
		addr, size);
	return 0;
}

static int bg_get_version(const struct subsys_desc *subsys)
{
	struct pil_bg_data *bg_data = subsys_to_data(subsys);
	struct pil_desc desc = bg_data->desc;
	struct tzapp_bg_req bg_tz_req;
	int ret;
	struct device dev = {NULL};

	arch_setup_dma_ops(&dev, 0, 0, NULL, 0);

	desc.attrs = 0;
	desc.attrs |= DMA_ATTR_SKIP_ZEROING;
	desc.attrs |= DMA_ATTR_STRONGLY_ORDERED;


	bg_tz_req.tzapp_bg_cmd = BGPIL_GET_BG_VERSION;

	ret = bgpil_tzapp_comm(bg_data, &bg_tz_req);
	if (ret || bg_data->cmd_status) {
		dev_dbg(desc.dev, "%s: BG PIL get BG version failed error %d\n",
			__func__, bg_data->cmd_status);
		return bg_data->cmd_status;
	}

	return 0;
}

/**
 * bg_auth_and_xfer() - Called by Peripheral loader framework
 * to signal tz app to authenticate and boot bg chip.
 * @pil: pil descriptor.
 *
 * Return: 0 on success. Error code on failure.
 */
static int bg_auth_and_xfer(struct pil_desc *pil)
{
	struct pil_bg_data *bg_data = desc_to_data(pil);
	struct tzapp_bg_req bg_tz_req;
	int ret;

	bg_tz_req.tzapp_bg_cmd = BGPIL_IMAGE_LOAD;
	bg_tz_req.address_fw = bg_data->address_fw;
	bg_tz_req.size_fw = bg_data->size_fw;

	ret = bgpil_tzapp_comm(bg_data, &bg_tz_req);
	if (bg_data->cmd_status == BG_CRASH_IN_TWM) {
		/* Do ramdump and resend boot cmd */
		if (is_twm_exit())
			bg_data->subsys_desc.ramdump(true,
				&bg_data->subsys_desc);
		bg_tz_req.tzapp_bg_cmd = BGPIL_DLOAD_CONT;
		ret = bgpil_tzapp_comm(bg_data, &bg_tz_req);
	}
	if (ret || bg_data->cmd_status) {
		dev_err(pil->dev,
			"%s: BGPIL_IMAGE_LOAD qseecom call failed\n",
			__func__);
		pil_free_memory(&bg_data->desc);
		return bg_data->cmd_status;
	}
	ret = bg_get_version(&bg_data->subsys_desc);
	/* BG Transfer of image is complete, free up the memory */
	pr_debug("BG Firmware authentication and transfer done\n");
	pil_free_memory(&bg_data->desc);
	return 0;
}

/**
 * bg_ramdump() - Called by SSR framework to save dump of BG internal
 * memory, BG PIL does allocate region from dynamic memory and pass this
 * region to tz to dump memory content of BG.
 * @subsys: subsystem descriptor.
 *
 * Return: 0 on success. Error code on failure.
 */
static int bg_ramdump(int enable, const struct subsys_desc *subsys)
{
	struct pil_bg_data *bg_data = subsys_to_data(subsys);
	struct pil_desc desc = bg_data->desc;
	struct ramdump_segment *ramdump_segments;
	struct tzapp_bg_req bg_tz_req;
	phys_addr_t start_addr;
	void *region;
	int ret;
	struct device dev = {0};

	arch_setup_dma_ops(&dev, 0, 0, NULL, 0);

	desc.attrs = 0;
	desc.attrs |= DMA_ATTR_SKIP_ZEROING;
	desc.attrs |= DMA_ATTR_STRONGLY_ORDERED;

	region = dma_alloc_attrs(desc.dev, BG_RAMDUMP_SZ,
				&start_addr, GFP_KERNEL, desc.attrs);

	if (region == NULL) {
		dev_dbg(desc.dev,
			"BG PIL failure to allocate ramdump region of size %zx\n",
			BG_RAMDUMP_SZ);
		return -ENOMEM;
	}

	ramdump_segments = kcalloc(1, sizeof(*ramdump_segments), GFP_KERNEL);
	if (!ramdump_segments)
		return -ENOMEM;

	bg_tz_req.tzapp_bg_cmd = BGPIL_RAMDUMP;
	bg_tz_req.address_fw = start_addr;
	bg_tz_req.size_fw = BG_RAMDUMP_SZ;

	ret = bgpil_tzapp_comm(bg_data, &bg_tz_req);
	if (ret || bg_data->cmd_status) {
		dev_dbg(desc.dev, "%s: BG PIL ramdump collection failed\n",
			__func__);
		return bg_data->cmd_status;
	}

	ramdump_segments->address = start_addr;
	ramdump_segments->size = BG_RAMDUMP_SZ;

	do_ramdump(bg_data->ramdump_dev, ramdump_segments, 1);
	kfree(ramdump_segments);
	dma_free_attrs(desc.dev, BG_RAMDUMP_SZ, region,
		       start_addr, desc.attrs);
	return 0;
}

static struct pil_reset_ops pil_ops_trusted = {
	.init_image = bg_auth_metadata,
	.mem_setup =  bg_get_firmware_addr,
	.auth_and_reset = bg_auth_and_xfer,
	.shutdown = NULL,
	.proxy_vote = NULL,
	.proxy_unvote = NULL,
};

/**
 * bg_restart_work() - scheduled by interrupt handler to carry
 * out ssr sequence
 * @work: work struct.
 *
 * Return: none.
 */
static void bg_restart_work(struct work_struct *work)
{
	struct pil_bg_data *drvdata =
		container_of(work, struct pil_bg_data, restart_work);
		subsystem_restart_dev(drvdata->subsys);
}

static irqreturn_t bg_errfatal(int irq, void *dev_id)
{
	struct pil_bg_data *drvdata = (struct pil_bg_data *)dev_id;

	if (!drvdata)
		return IRQ_HANDLED;

	dev_dbg(drvdata->desc.dev, "BG s/w err fatal\n");

	queue_work(drvdata->bg_queue, &drvdata->restart_work);

	return IRQ_HANDLED;
}

static irqreturn_t bg_status_change(int irq, void *dev_id)
{
	bool value;
	struct pil_bg_data *drvdata = (struct pil_bg_data *)dev_id;

	if (!drvdata)
		return IRQ_HANDLED;

	value = gpio_get_value(drvdata->gpios[0]);
	if (value == true && !drvdata->is_ready) {
		dev_info(drvdata->desc.dev,
			"BG services are up and running: irq state changed 0->1\n");
		drvdata->is_ready = true;
		complete(&drvdata->err_ready);
	} else if (value == false && drvdata->is_ready) {
		dev_err(drvdata->desc.dev,
			"BG got unexpected reset: irq state changed 1->0\n");
		queue_work(drvdata->bg_queue, &drvdata->restart_work);
	} else {
		dev_err(drvdata->desc.dev,
			"BG status irq: unknown status\n");
	}

	return IRQ_HANDLED;
}

/**
 * setup_bg_gpio_irq() - called in probe to configure input/
 * output gpio.
 * @drvdata: private data struct for BG.
 *
 * Return: 0 on success. Error code on failure.
 */
static int setup_bg_gpio_irq(struct platform_device *pdev,
					struct pil_bg_data *drvdata)
{
	int ret = -1;
	int irq, i;

	if (gpio_request(drvdata->gpios[0], "BG2AP_STATUS")) {
		dev_err(&pdev->dev,
			"%s Failed to configure BG2AP_STATUS gpio\n",
				__func__);
		goto err;
	}
	if (gpio_request(drvdata->gpios[1], "BG2AP_ERRFATAL")) {
		dev_err(&pdev->dev,
			"%s Failed to configure BG2AP_ERRFATAL gpio\n",
				__func__);
		goto err;
	}
	gpio_direction_input(drvdata->gpios[0]);
	gpio_direction_input(drvdata->gpios[1]);
	/* BG2AP STATUS IRQ */
	irq = gpio_to_irq(drvdata->gpios[0]);
	if (irq < 0) {
		dev_err(&pdev->dev,
		"%s: bad BG2AP_STATUS IRQ resource, err = %d\n",
			__func__, irq);
		goto err;
	}

	drvdata->status_irq = irq;
	/* BG2AP ERR_FATAL irq. */
	irq = gpio_to_irq(drvdata->gpios[1]);
	if (irq < 0) {
		dev_err(&pdev->dev, "bad BG2AP_ERRFATAL IRQ resource\n");
		goto err;
	}
	ret = request_irq(irq, bg_errfatal,
		IRQF_TRIGGER_RISING | IRQF_ONESHOT, "bg2ap_errfatal", drvdata);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"%s: BG2AP_ERRFATAL IRQ#%d request failed,\n",
				__func__, irq);
		goto err;
	}
	drvdata->errfatal_irq = irq;
	/* Configure outgoing GPIO's */
	if (gpio_request(drvdata->gpios[2], "AP2BG_ERRFATAL")) {
		dev_err(&pdev->dev,
			"%s Failed to configure AP2BG_ERRFATAL gpio\n",
				__func__);
		goto err;
	}
	if (gpio_request(drvdata->gpios[3], "AP2BG_STATUS")) {
		dev_err(&pdev->dev,
			"%s Failed to configure AP2BG_STATUS gpio\n",
				__func__);
		goto err;
	}
	/*
	 * Put status gpio in default high state which will
	 * make transition to low on any sudden reset case of msm
	 */
	gpio_direction_output(drvdata->gpios[2], 0);
	gpio_direction_output(drvdata->gpios[3], 1);
	/* Inform BG that AP is up */
	gpio_set_value(drvdata->gpios[3], 1);
	return 0;
err:
	for (i = 0; i < NUM_GPIOS; ++i) {
		if (gpio_is_valid(drvdata->gpios[i]))
			gpio_free(drvdata->gpios[i]);
	}
	return ret;
}

/**
 * bg_dt_parse_gpio() - called in probe to parse gpio's
 * @drvdata: private data struct for BG.
 *
 * Return: 0 on success. Error code on failure.
 */
static int bg_dt_parse_gpio(struct platform_device *pdev,
				struct pil_bg_data *drvdata)
{
	int i, val;

	for (i = 0; i < NUM_GPIOS; i++)
		drvdata->gpios[i] = INVALID_GPIO;
	val = of_get_named_gpio(pdev->dev.of_node,
					"qcom,bg2ap-status-gpio", 0);
	if (val >= 0)
		drvdata->gpios[0] = val;
	else {
		pr_err("BG status gpio not found, error=%d\n", val);
		return -EINVAL;
	}
	val = of_get_named_gpio(pdev->dev.of_node,
					"qcom,bg2ap-errfatal-gpio", 0);
	if (val >= 0)
		drvdata->gpios[1] = val;
	else {
		pr_err("BG err-fatal gpio not found, error=%d\n", val);
		return -EINVAL;
	}
	val = of_get_named_gpio(pdev->dev.of_node,
					"qcom,ap2bg-errfatal-gpio", 0);
	if (val >= 0)
		drvdata->gpios[2] = val;
	else {
		pr_err("ap2bg err-fatal gpio not found, error=%d\n", val);
		return -EINVAL;
	}
	val = of_get_named_gpio(pdev->dev.of_node,
					"qcom,ap2bg-status-gpio", 0);
	if (val >= 0)
		drvdata->gpios[3] = val;
	else {
		pr_err("ap2bg status gpio not found, error=%d\n", val);
		return -EINVAL;
	}
	return 0;
}

static int pil_bg_driver_probe(struct platform_device *pdev)
{
	struct pil_bg_data *bg_data;
	int rc;

	bg_data = devm_kzalloc(&pdev->dev, sizeof(*bg_data), GFP_KERNEL);
	if (!bg_data)
		return -ENOMEM;
	platform_set_drvdata(pdev, bg_data);
	rc = of_property_read_string(pdev->dev.of_node,
			"qcom,firmware-name", &bg_data->desc.name);
	if (rc)
		return rc;
	bg_data->desc.dev = &pdev->dev;
	bg_data->desc.owner = THIS_MODULE;
	bg_data->desc.ops = &pil_ops_trusted;
	rc = pil_desc_init(&bg_data->desc);
	if (rc)
		return rc;
	/* Read gpio configuration */
	rc = bg_dt_parse_gpio(pdev, bg_data);
	if (rc)
		return rc;
	rc = setup_bg_gpio_irq(pdev, bg_data);
	if (rc < 0)
		return rc;
	bg_data->subsys_desc.name = bg_data->desc.name;
	bg_data->subsys_desc.owner = THIS_MODULE;
	bg_data->subsys_desc.dev = &pdev->dev;
	bg_data->subsys_desc.shutdown = bg_shutdown;
	bg_data->subsys_desc.powerup = bg_powerup;
	bg_data->subsys_desc.ramdump = bg_ramdump;
	bg_data->subsys_desc.free_memory = NULL;
	bg_data->subsys_desc.crash_shutdown = bg_app_shutdown_notify;
	bg_data->ramdump_dev =
		create_ramdump_device(bg_data->subsys_desc.name, &pdev->dev);
	if (!bg_data->ramdump_dev) {
		rc = -ENOMEM;
		goto err_ramdump;
	}
	bg_data->subsys = subsys_register(&bg_data->subsys_desc);
	if (IS_ERR(bg_data->subsys)) {
		rc = PTR_ERR(bg_data->subsys);
		goto err_subsys;
	}

	bg_data->reboot_blk.notifier_call = bg_app_reboot_notify;
	register_reboot_notifier(&bg_data->reboot_blk);

	bg_data->bg_queue = alloc_workqueue("bg_queue", 0, 0);
	if (!bg_data->bg_queue) {
		dev_err(&pdev->dev, "could not create bg_queue\n");
		subsys_unregister(bg_data->subsys);
		goto err_subsys;
	}
	INIT_WORK(&bg_data->restart_work, bg_restart_work);
	return 0;
err_subsys:
	destroy_ramdump_device(bg_data->ramdump_dev);
err_ramdump:
	pil_desc_release(&bg_data->desc);
	return rc;
}

static int pil_bg_driver_exit(struct platform_device *pdev)
{
	struct pil_bg_data *bg_data = platform_get_drvdata(pdev);

	subsys_unregister(bg_data->subsys);
	destroy_ramdump_device(bg_data->ramdump_dev);
	pil_desc_release(&bg_data->desc);

	return 0;
}

const struct of_device_id pil_bg_match_table[] = {
	{.compatible = "qcom,pil-blackghost"},
	{}
};

static struct platform_driver pil_bg_driver = {
	.probe = pil_bg_driver_probe,
	.remove = pil_bg_driver_exit,
	.driver = {
		.name = "subsys-pil-bg",
		.of_match_table = pil_bg_match_table,
		.owner = THIS_MODULE,
	},
};

static int __init pil_bg_init(void)
{
	return platform_driver_register(&pil_bg_driver);
}
module_init(pil_bg_init);

static void __exit pil_bg_exit(void)
{
	platform_driver_unregister(&pil_bg_driver);
}
module_exit(pil_bg_exit);

MODULE_DESCRIPTION("Support for booting QTI Blackghost SoC");
MODULE_LICENSE("GPL v2");
