// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt)    "%s: " fmt, __func__

#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/dma-heap.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/qseecom.h>
#include <linux/qtee_shmbridge.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/soc/qcom/mdt_loader.h>
#include <linux/qseecom_kernel.h>
#include <linux/soc/qcom/slatecom_interface.h>
#include <linux/suspend.h>
#include "qcom_common.h"
#include "remoteproc_internal.h"


#define SECURE_APP		"slateapp"
#define INVALID_GPIO		-1
#define NUM_GPIOS		3

#define RESULT_SUCCESS		0
#define RESULT_FAILURE		-1

/* Slate Ramdump Size 8MB + 3MB (ITCM) */
#define SLATE_RAMDUMP_SZ 0XB00000
#define SLATE_MINIDUMP_SZ (0x400*40)
#define SLATE_RAMDUMP		3
#define SLATE_MINIDUMP		4

#define SLATE_CRASH_IN_TWM	-2

#define segment_is_hash(flag) (((flag) & (0x7 << 24)) == (0x2 << 24))

static struct workqueue_struct *slate_reset_wq;

/* tzapp command list.*/
enum slate_tz_commands {
	SLATE_RPROC_RAMDUMP,
	SLATE_RPROC_IMAGE_LOAD,
	SLATE_RPROC_AUTH_MDT,
	SLATE_RPROC_DLOAD_CONT,
	SLATE_RPROC_GET_SLATE_VERSION,
	SLATE_RPROC_SHUTDOWN,
	SLATE_RPROC_DUMPINFO,
	SLATE_RPROC_UP_INFO,
	SLATE_RPROC_RESET,
};

/* tzapp bg request.*/
struct tzapp_slate_req {
	uint64_t address_fw;
	uint32_t size_fw;
	uint8_t tzapp_slate_cmd;
} __packed;

/* tzapp bg response.*/
struct tzapp_slate_rsp {
	uint32_t tzapp_slate_cmd;
	uint32_t slate_info_len;
	int32_t status;
	uint32_t slate_info[100];
} __packed;

/**
 * struct pil_mdt - Representation of <name>.mdt file in memory
 * @hdr: ELF32 header
 * @phdr: ELF32 program headers
 */
struct pil_mdt {
	struct elf32_hdr hdr;
	struct elf32_phdr phdr[];
};

/**
 * struct qcom_slate
 * @dev: Device pointer
 * @rproc: Remoteproc handle for slate
 * @firmware_name: FW image file name
 * @ssr_subdev: SSR subdevice to be registered with remoteproc
 * @ssr_name: SSR subdevice name used as reference in remoteproc
 * @glink_subdev: GLINK subdevice to be registered with remoteproc
 * @sysmon: sysmon subdevice to be registered with remoteproc
 * @sysmon_name: sysmon subdevice name used as reference in remoteproc
 * @ssctl_id: instance id of the ssctl QMI service
 * @reboot_nb: notifier block to handle reboot scenarios
 * @address_fw: address where firmware binaries loaded in DMA
 * @size_fw: size of slate firmware binaries in DMA
 * @qseecom_handle: handle of TZ app
 * @cmd_status: qseecom command status
 * @app_status: status of tz app loading
 * @is_ready: Is slate chip up
 * @err_ready: The error ready signal
 * @shutdown_done: The shutdown done signal
 * @region_start: DMA handle for loading FW
 * @region_end: DMA address indicating end of DMA buffer
 * @region: CPU address for DMA buffer
 * @is_region_allocated: Is DMA buffer allocated
 * @region_size: DMA buffer size for FW
 * @gpios: arry to hold all gpio handler
 * @status_irq: irq to indicate slate status
 * @slate_queue: private queue to schedule worker thread for bottom half
 * @restart_work: work struct for executing ssr
 * @mem_phys: pa of DMA buffer for ramdump
 * @mem_region: va of DMA buffer for ramdump
 * @mem_size: DMA buffer size for ramdump
 */
struct qcom_slate {
	struct device *dev;

	struct rproc *rproc;

	const char *firmware_name;

	struct qcom_rproc_ssr ssr_subdev;
	const char *ssr_name;

	struct qcom_rproc_glink glink_subdev;
	struct qcom_sysmon *sysmon;
	const char *sysmon_name;
	int ssctl_id;

	struct notifier_block reboot_nb;

	phys_addr_t address_fw;
	size_t size_fw;

	struct qseecom_handle *qseecom_handle;
	u32 cmd_status;
	int app_status;
	bool is_ready;
	struct completion err_ready;
	struct completion shutdown_done;

	phys_addr_t region_start;
	phys_addr_t region_end;
	void *region;
	bool is_region_allocated;
	size_t region_size;

	unsigned int gpios[NUM_GPIOS];
	int status_irq;
	struct workqueue_struct *slate_queue;
	struct work_struct restart_work;

	phys_addr_t mem_phys;
	void *mem_region;
	size_t mem_size;

	struct wakeup_source rproc_slate_ws;
};

static irqreturn_t slate_status_change(int irq, void *dev_id);
struct mutex cmdsync_lock;

static ssize_t txn_id_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct platform_device *pdev = container_of(dev,
						struct platform_device, dev);
	struct qcom_slate *slate_data =
			(struct qcom_slate *)platform_get_drvdata(pdev);

	return sysfs_emit(buf, "%u\n",
			qcom_sysmon_get_txn_id(slate_data->sysmon));
}
static DEVICE_ATTR_RO(txn_id);

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
 * load_slate_tzapp() - Called to load TZ app.
 * @pbd: struct containing private SLATE data.
 *
 * Return: 0 on success. Error code on failure.
 */

static int load_slate_tzapp(struct qcom_slate *pbd)
{
	int rc;

	/* return success if already loaded */
	if (pbd->qseecom_handle && !pbd->app_status)
		return 0;
	/* Load the APP */
	rc = qseecom_start_app(&pbd->qseecom_handle, SECURE_APP, SZ_4K);
	if (rc < 0) {
		dev_err(pbd->dev, "SLATE TZ app load failure\n");
		pbd->app_status = RESULT_FAILURE;
		return -EIO;
	}
	pbd->app_status = RESULT_SUCCESS;
	return 0;
}

/**
 * slate_tzapp_comm() - Function called to communicate with TZ APP.
 * @pbd: struct containing private SLATE data.
 * @req: struct containing command and parameters.
 *
 * Return: 0 on success. Error code on failure.
 */

static long slate_tzapp_comm(struct qcom_slate *pbd,
				struct tzapp_slate_req *req)
{
	struct tzapp_slate_req *slate_tz_req;
	struct tzapp_slate_rsp *slate_tz_rsp;
	int rc, req_len, rsp_len, i;
	unsigned char *ascii;

	/* Fill command structure */
	req_len = sizeof(struct tzapp_slate_req);
	rsp_len = sizeof(struct tzapp_slate_rsp);

	mutex_lock(&cmdsync_lock);
	rc = get_cmd_rsp_buffers(pbd->qseecom_handle,
		(void **)&slate_tz_req, &req_len,
		(void **)&slate_tz_rsp, &rsp_len);
	if (rc)
		goto end;

	slate_tz_req->tzapp_slate_cmd = req->tzapp_slate_cmd;
	slate_tz_req->address_fw = req->address_fw;
	slate_tz_req->size_fw = req->size_fw;
	rc = qseecom_send_command(pbd->qseecom_handle,
		(void *)slate_tz_req, req_len, (void *)slate_tz_rsp, rsp_len);

	mutex_unlock(&cmdsync_lock);
	pr_debug("SLATE PIL qseecom returned with value 0x%x and status 0x%x\n",
		rc, slate_tz_rsp->status);
	if (rc || slate_tz_rsp->status)
		pbd->cmd_status = slate_tz_rsp->status;
	else
		pbd->cmd_status = 0;
	/* if last command sent was SLATE_VERSION print the version */
	if (req->tzapp_slate_cmd == SLATE_RPROC_GET_SLATE_VERSION) {
		pr_info("SLATE FW version\n");
		for (i = 0; i < slate_tz_rsp->slate_info_len; i++) {
			pr_info("Version 0x%x\n", slate_tz_rsp->slate_info[i]);
			ascii = (unsigned char *)&slate_tz_rsp->slate_info[i];
		}
	}
	return rc;
end:
	mutex_unlock(&cmdsync_lock);
	return rc;
}

/**
 * slate_restart_work() - schedule by interrupt handler to carry
 * out ssr sequence
 * @work: work struct.
 *
 * Return: none.
 */
static void slate_restart_work(struct work_struct *work)
{

	struct qcom_slate *slate_data =
		container_of(work, struct qcom_slate, restart_work);
	struct rproc *slate_rproc = slate_data->rproc;

	/* Trigger  apps crash if recovery is disabled */
	BUG_ON(slate_rproc->recovery_disabled);

	/* If recovery is enabled, go for recovery path */
	pr_debug("Slate is crashed! Starting recovery...\n");
	rproc_report_crash(slate_rproc, RPROC_FATAL_ERROR);

}

static irqreturn_t slate_status_change(int irq, void *dev_id)
{
	bool value;
	struct qcom_slate *drvdata = (struct qcom_slate *)dev_id;
	struct tzapp_slate_req slate_tz_req;
	int ret = 0;

	if (!drvdata)
		return IRQ_HANDLED;

	value = gpio_get_value(drvdata->gpios[0]);
	update_s2a_status(value);

	if (value && !drvdata->is_ready) {
		dev_info(drvdata->dev,
			"SLATE services are up and running: irq state changed 0->1\n");
		__pm_relax(&drvdata->rproc_slate_ws);
		drvdata->is_ready = true;
		complete(&drvdata->err_ready);
		slate_tz_req.tzapp_slate_cmd = SLATE_RPROC_UP_INFO;
		ret = slate_tzapp_comm(drvdata, &slate_tz_req);
		if (ret || drvdata->cmd_status) {
			dev_err(drvdata->dev, "%s: SLATE RPROC get slate version failed error %d\n",
				__func__, drvdata->cmd_status);
			return IRQ_NONE;
		}
	} else if (!value && drvdata->is_ready) {
		dev_err(drvdata->dev,
			"SLATE got unexpected reset: irq state changed 1->0\n");
			/* skip dump collection and return with shutdown completion signal */
			if (is_slate_unload_only()) {
				dev_info(drvdata->dev,
					"Received shutdown_only request\n");
				complete(&drvdata->shutdown_done);
				return IRQ_HANDLED;
			}
		__pm_stay_awake(&drvdata->rproc_slate_ws);
		queue_work(drvdata->slate_queue, &drvdata->restart_work);
	} else {
		dev_err(drvdata->dev, "SLATE status irq: unknown status\n");
	}

	return IRQ_HANDLED;
}

/**
 * setup_slate_gpio_irq() - called in probe to configure input/
 * output gpio.
 * @drvdata: private data struct for SLATE.
 *
 * Return: 0 on success. Error code on failure.
 */
static int setup_slate_gpio_irq(struct platform_device *pdev,
				struct qcom_slate *drvdata)
{
	int ret = -1;
	int irq, i;

	if (gpio_request(drvdata->gpios[0], "SLATE2AP_STATUS")) {
		dev_err(&pdev->dev,
			"%s Failed to configure SLATE2AP_STATUS gpio\n",
				__func__);
		goto err;
	}
	gpio_direction_input(drvdata->gpios[0]);
	/* SLATE2AP STATUS IRQ */
	irq = gpio_to_irq(drvdata->gpios[0]);
	if (irq < 0) {
		dev_err(&pdev->dev,
		"%s: bad SLATE2AP_STATUS IRQ resource, err = %d\n",
			__func__, irq);
		goto err;
	}

	drvdata->status_irq = irq;
	ret = devm_request_threaded_irq(drvdata->dev, drvdata->status_irq,
		NULL, slate_status_change,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"slate2ap_status", drvdata);

	if (ret < 0) {
		dev_err(drvdata->dev,
			"%s: SLATE2AP_STATUS IRQ#%d re registration failed, err=%d\n",
			__func__, drvdata->status_irq, ret);
		goto err;
	}

	ret = irq_set_irq_wake(drvdata->status_irq, true);
	if (ret < 0) {
		dev_err(drvdata->dev,
			"%s: SLATE2AP_STATUS IRQ#%d set wakeup capable failed, err=%d\n",
			__func__, drvdata->status_irq, ret);
		goto err;
	}

	/* AP2SLATE STATUS IRQ */
	if (gpio_request(drvdata->gpios[1], "AP2SLATE_STATUS")) {
		dev_err(&pdev->dev,
			"%s Failed to configure AP2SLATE_STATUS gpio\n",
				__func__);
		goto err;
	}
	/*
	 * Put status gpio in default high state which will
	 * make transition to low on any sudden reset case of msm
	 */
	gpio_direction_output(drvdata->gpios[1], 1);
	/* Inform SLATE that AP is up */
	gpio_set_value(drvdata->gpios[1], 1);
	return 0;
err:
	for (i = 0; i < NUM_GPIOS; ++i) {
		if (gpio_is_valid(drvdata->gpios[i]))
			gpio_free(drvdata->gpios[i]);
	}
	return ret;
}

/**
 * slate_dt_parse_gpio() - called in probe to parse gpio's
 * @drvdata: private data struct for SLATE.
 *
 * Return: 0 on success. Error code on failure.
 */
static int slate_dt_parse_gpio(struct platform_device *pdev,
				struct qcom_slate *drvdata)
{
	int i, val;

	for (i = 0; i < NUM_GPIOS; i++)
		drvdata->gpios[i] = INVALID_GPIO;

	val = of_get_named_gpio(pdev->dev.of_node,
					"qcom,slate2ap-status-gpio", 0);
	if (val >= 0) {
		drvdata->gpios[0] = val;
		update_s2a_status(gpio_get_value(drvdata->gpios[0]));
	} else {
		pr_err("SLATE status gpio not found, error=%d\n", val);
		return -EINVAL;
	}

	val = of_get_named_gpio(pdev->dev.of_node,
					"qcom,ap2slate-status-gpio", 0);
	if (val >= 0)
		drvdata->gpios[1] = val;
	else {
		pr_err("ap2slate status gpio not found, error=%d\n", val);
		return -EINVAL;
	}

	return 0;
}

/**
 * slate_auth_and_xfer() - Called by start operation of remoteproc framework
 * to signal tz app to authenticate and boot slate chip.
 * @slate_data: struct containing private <slate> data.
 *
 * Return: 0 on success. Error code on failure.
 */
static int slate_auth_and_xfer(struct qcom_slate *slate_data)
{
	struct tzapp_slate_req slate_tz_req;
	uint32_t ns_vmids[] = {VMID_HLOS};
	uint32_t ns_vm_perms[] = {PERM_READ | PERM_WRITE};
	u64 shm_bridge_handle;
	int ret;

	ret = qtee_shmbridge_register(slate_data->address_fw, slate_data->size_fw,
			ns_vmids, ns_vm_perms, 1, PERM_READ | PERM_WRITE,
			&shm_bridge_handle);

	if (ret) {
		dev_err(slate_data->dev,
				"%s: Failed to create shm bridge [%d]\n",
				__func__, ret);
		return ret;
	}

	slate_tz_req.tzapp_slate_cmd = SLATE_RPROC_IMAGE_LOAD;
	slate_tz_req.address_fw = slate_data->address_fw;
	slate_tz_req.size_fw = slate_data->size_fw;

	ret = slate_tzapp_comm(slate_data, &slate_tz_req);

	if (slate_data->cmd_status == SLATE_CRASH_IN_TWM) {
		slate_tz_req.tzapp_slate_cmd = SLATE_RPROC_DLOAD_CONT;
		ret = slate_tzapp_comm(slate_data, &slate_tz_req);
	}

	if (ret || slate_data->cmd_status) {
		dev_err(slate_data->dev,
				"%s: Firmware image authentication failed\n",
				__func__);
		ret = slate_data->cmd_status;
		goto tzapp_comm_failed;
	}

	/* slate Transfer of image is complete, free up the memory */
	pr_debug("Firmware authentication and transfer done\n");

tzapp_comm_failed:
	qtee_shmbridge_deregister(shm_bridge_handle);
	return ret;
}

/**
 * wait_for_err_ready() - Called in power_up to wait for error ready.
 * Signal waiting function.
 * @slate_data: SLATE RPROC private structure.
 *
 * Return: 0 on success. Error code on failure.
 */
static int wait_for_err_ready(struct qcom_slate *slate_data)
{
	int ret;

	if ((!slate_data->status_irq))
		return 0;

	ret = wait_for_completion_timeout(&slate_data->err_ready,
			msecs_to_jiffies(10000));
	if (!ret) {
		pr_err("[%s]: Error ready timed out\n",
			slate_data->firmware_name);
		return -ETIMEDOUT;
	}
	return 0;
}

/**
 * wait_for_shutdown_done() - Called in power_down to wait for shutdown done.
 * Signal waiting function.
 * @slate_data: SLATE RPROC private structure.
 *
 * Return: 0 on success. Error code on failure.
 */
static int wait_for_shutdown_done(struct qcom_slate *slate_data)
{
	int ret;

	if ((!slate_data->status_irq))
		return 0;

	ret = wait_for_completion_timeout(&slate_data->shutdown_done,
			msecs_to_jiffies(10000));
	if (!ret) {
		pr_err("[%s]: shutdown done timed out\n",
			slate_data->firmware_name);
		return -ETIMEDOUT;
	}
	return 0;
}

void send_reset_signal(struct qcom_slate *slate_data)
{
	struct tzapp_slate_req slate_tz_req;
	int ret;

	slate_tz_req.tzapp_slate_cmd = SLATE_RPROC_RESET;
	slate_tz_req.address_fw = 0;
	slate_tz_req.size_fw = 0;

	ret = slate_tzapp_comm(slate_data, &slate_tz_req);
	if (ret || slate_data->cmd_status)
		dev_err(slate_data->dev,
			"%s: Failed to send reset signal to tzapp\n",
			__func__);
}

int slate_flash_mode(struct qcom_slate *slate_data)
{
	int ret;
	int retry_attempt = 2;

	do {
		ret = wait_for_err_ready(slate_data);
		if (!ret)
			return RESULT_SUCCESS;
		dev_err(slate_data->dev,
			"[%s:%d]: Timed out waiting for error ready: %s!\n",
			current->comm, current->pid, slate_data->firmware_name);

		pr_debug("Retry booting slate, Mode: Flash, attempt: %d\n",
				retry_attempt);
		send_reset_signal(slate_data);
		retry_attempt -= 1;
	} while (retry_attempt);

	return ret;
}

static int slate_start(struct rproc *rproc)
{
	struct qcom_slate *slate_data = (struct qcom_slate *)rproc->priv;
	struct tzapp_slate_req slate_tz_req;
	int ret = 0;

	if (!slate_data) {
		pr_err("%s - Invalid slate pointer !!\n",
			__func__);
		return -EINVAL;
	}

	/* Slate is booted from flash */
	if (get_slate_boot_mode()) {
		if (gpio_get_value(slate_data->gpios[0])) {
			pr_info("Slate is booted up!! Mode: FLASH\n");
			slate_data->is_ready = true;
			/* Update dump configuration when it boots from FLASH Mode. */
			slate_tz_req.tzapp_slate_cmd = SLATE_RPROC_UP_INFO;
			ret = slate_tzapp_comm(slate_data, &slate_tz_req);
			if (ret || slate_data->cmd_status)
				dev_err(slate_data->dev,
				"%s: failed to update dump info %d\n",
				__func__, slate_data->cmd_status);
			return RESULT_SUCCESS;
		} else
			return RESULT_FAILURE;
	}

	slate_data->address_fw = slate_data->region_start;
	slate_data->size_fw = slate_data->region_end - slate_data->region_start;
	pr_debug("SLATE PIL loads firmware blobs at 0x%llx with size 0x%zx\n",
		slate_data->address_fw, slate_data->size_fw);

	ret = slate_auth_and_xfer(slate_data);
	if (ret) {
		dev_err(slate_data->dev, "%s slate TZ app load failure\n",
					__func__);
		return ret;
	}

	ret = wait_for_err_ready(slate_data);
	if (ret) {
		dev_err(slate_data->dev,
			"[%s:%d]: Timed out waiting for error ready: %s!\n",
			current->comm, current->pid, slate_data->firmware_name);
		return ret;
	}

	pr_err("Slate is booted up!! Mode: HOST\n");

	dma_free_coherent(slate_data->dev, slate_data->region_size,
		slate_data->region, slate_data->region_start);
	slate_data->is_region_allocated = false;
	slate_data->region = NULL;
	slate_data->region_start = 0;
	slate_data->region_end = 0;
	slate_data->region_size = 0;
	return 0;
}

/**
 * slate_coredump() - Called by SSR framework to save dump of SLATE internal
 * memory, SLATE PIL does allocate region from dynamic memory and pass this
 * region to tz to dump memory content of SLATE.
 * @rproc: remoteproc handle for slate.
 *
 * Return: 0 on success. Error code on failure.
 */

static void slate_coredump(struct rproc *rproc)
{
	struct qcom_slate *slate_data = (struct qcom_slate *)rproc->priv;
	struct tzapp_slate_req slate_tz_req;
	uint32_t ns_vmids[] = {VMID_HLOS};
	uint32_t ns_vm_perms[] = {PERM_READ | PERM_WRITE};
	u64 shm_bridge_handle;
	void *region;
	phys_addr_t start_addr;
	uint32_t dump_info;
	unsigned long size = SLATE_RAMDUMP_SZ;
	unsigned long attr = 0;
	int ret = 0;

	pr_err("Setup for Coredump.\n");

	if (!slate_data->qseecom_handle) {
		ret = load_slate_tzapp(slate_data);
		if (ret) {
			dev_err(slate_data->dev,
				"%s: SLATE TZ app load failure\n",
				__func__);
			return;
		}
	}

	/* This check is added here to make slate dump collection
	 * decision in RTOS/TWM mode exit. Only way for kernel to know slate state
	 * info(crashed/running)in RTOS/TWM exit is by reading S2A irq line.
	 * When S2A is pulled LOW, it is interpreted as slate crashed state and
	 * slate dump needs to be collected. Once dumps are collected TZ will
	 * automatically send SLATE_RESET CMD.
	 * When S2A is pulled HIGH, it is interpreted as slate running state and
	 * dump should not be collected. At this point it is necessary
	 * to send SLATE_RESET CMD to bring slate out of RTOS slate.
	 * This check does not disturb SSR/system dump collection.
	 */

	if (is_twm_exit()) {
		/* reset_cmd signals shutdown on slate, lets ack s2a irq for same
		 * otherwise it will haunt after slate boot up and crash system.
		 */
		if (!gpio_get_value(slate_data->gpios[0])) {
			pr_info("TWM Exit: Collect Dump, slate is CRASHED..!!\n");
			/* We are assuming that Slate TZapp has started
			 * subsystem_ramdump service.
			 * Introducing delay here for subsystem_ramdump
			 * service to start.
			 */
			msleep(5000);
		} else {
			enable_irq(slate_data->status_irq);
			slate_data->is_ready = true;
			pr_info("TWM Exit: Skip dump collection, slate is RUNNING ..!!\n");
			/* Send RESET CMD to bring slate out of RTOS state */
			slate_data->cmd_status = 0;
			slate_tz_req.tzapp_slate_cmd = SLATE_RPROC_RESET;
			ret = slate_tzapp_comm(slate_data, &slate_tz_req);
			if (ret || slate_data->cmd_status) {
				dev_err(slate_data->dev,
					"%s: Failed to send reset signal to tzapp\n",
					__func__);
				goto rtos_out;
			}
			/* reset_cmd signals shutdown on slate, lets ack s2a irq for same
			 * otherwise it will haunt after slate boot up and crash system.
			 */
			/* By this time if S2A is not pulled then wait for it to go LOW */
			if (gpio_get_value(slate_data->gpios[0])) {
				ret = wait_for_shutdown_done(slate_data);
				if (ret) {
					dev_err(slate_data->dev,
					"[%s:%d]: Timed out waiting for error ready: %s!\n",
					current->comm, current->pid, slate_data->firmware_name);
				}
			}
			goto rtos_out;
		}
	}
	rproc_coredump_cleanup(rproc);

	slate_tz_req.tzapp_slate_cmd = SLATE_RPROC_DUMPINFO;
	ret = slate_tzapp_comm(slate_data, &slate_tz_req);
	dump_info = slate_data->cmd_status;

	if (dump_info == SLATE_RAMDUMP)
		size = SLATE_RAMDUMP_SZ;
	else if (dump_info == SLATE_MINIDUMP)
		size = SLATE_MINIDUMP_SZ;
	else {
		dev_err(slate_data->dev,
			"%s: SLATE RPROC ramdump collection failed\n",
			__func__);
		return;
	}

	region = dma_alloc_attrs(slate_data->dev, size,
			&start_addr, GFP_KERNEL, attr);

	if (region == NULL) {
		dev_dbg(slate_data->dev,
			"fail to allocate ramdump region of size %zx\n",
			size);
		return;
	}
	slate_data->mem_phys = start_addr;
	slate_data->mem_size = size;
	slate_data->mem_region = region;

	ret = qtee_shmbridge_register(start_addr, size, ns_vmids,
		ns_vm_perms, 1, PERM_READ | PERM_WRITE, &shm_bridge_handle);

	if (ret) {
		pr_err("%s Failed to create shm bridge. ret=[%d]\n",
			__func__, ret);
		goto dma_free;
	}

	slate_tz_req.tzapp_slate_cmd = SLATE_RPROC_RAMDUMP;
	slate_tz_req.address_fw = start_addr;
	slate_tz_req.size_fw = size;
	ret = slate_tzapp_comm(slate_data, &slate_tz_req);
	if (ret != 0 || slate_data->cmd_status) {
		dev_dbg(slate_data->dev,
			"%s: SLATE RPROC ramdmp collection failed\n",
			__func__);
		goto shm_free;
	}

	dma_sync_single_for_cpu(slate_data->dev, slate_data->mem_phys, size, DMA_FROM_DEVICE);

	pr_debug("Add coredump segment!\n");
	ret = rproc_coredump_add_custom_segment(rproc, start_addr, size,
			NULL, NULL);

	if (ret) {
		dev_err(slate_data->dev, "failed to add rproc_segment: %d\n",
			ret);
		rproc_coredump_cleanup(slate_data->rproc);
		goto shm_free;
	}

	rproc_coredump(rproc);

shm_free:
	qtee_shmbridge_deregister(shm_bridge_handle);
dma_free:
	dma_free_attrs(slate_data->dev, size, region,
			start_addr, attr);
	return;
rtos_out:
	disable_irq(slate_data->status_irq);
	slate_data->is_ready = false;
}

/**
 * slate_prepare() - Called by rproc_boot. This loads tz app.
 * @rproc: struct caontaining private slate data.
 *
 * Return: 0 on success. Error code on failure.
 */
static int slate_prepare(struct rproc *rproc)
{
	struct qcom_slate *slate_data = (struct qcom_slate *)rproc->priv;
	int ret = 0;

	if (!slate_data) {
		pr_err("%s - Invalid slate pointer !!\n",
			__func__);
		return -EINVAL;
	}
	if (slate_data->app_status != RESULT_SUCCESS) {
		ret = load_slate_tzapp(slate_data);
		if (ret) {
			dev_err(slate_data->dev,
			"%s: slate TZ app load failure\n", __func__);
			return ret;
		}
	}
	pr_debug("slateapp loaded\n");

	return ret;
}

static int slate_alloc_mem(struct qcom_slate *slate_data, size_t aligned_size)
{
	slate_data->region = dma_alloc_coherent(slate_data->dev, aligned_size,
			&slate_data->region_start, GFP_KERNEL);
	if (!slate_data->region)
		return -ENOMEM;

	return 0;
}

static int slate_alloc_region(struct qcom_slate *slate_data, phys_addr_t min_addr,
				phys_addr_t max_addr, size_t align)
{
	size_t size = max_addr - min_addr;
	size_t aligned_size;
	int ret;

	/* Don't reallocate due to fragmentation concerns, just sanity check */
	if (slate_data->is_region_allocated) {
		if (WARN(slate_data->region_end - slate_data->region_start < size,
			"Can't reuse PIL memory, too small\n"))
			return -ENOMEM;
		return 0;
	}

	if (align >= SZ_4M)
		aligned_size = ALIGN(size, SZ_4M);
	else if (align >= SZ_1M)
		aligned_size = ALIGN(size, SZ_1M);
	else
		aligned_size = ALIGN(size, SZ_4K);

	ret = slate_alloc_mem(slate_data, aligned_size);
	if (ret) {
		dev_err(slate_data->dev,
				"%s: Failed to allocate relocatable region\n",
				__func__);
		slate_data->region_start = 0;
		slate_data->region_end = 0;
		return ret;
	}

	slate_data->is_region_allocated = true;
	slate_data->region_end = slate_data->region_start + size;
	slate_data->region_size = aligned_size;

	return 0;
}

static bool segment_is_relocatable(const struct elf32_phdr *p)
{
	return !!(p->p_flags & BIT(27));
}

static int segment_is_loadable(const struct elf32_phdr *p)
{
	return (p->p_type == PT_LOAD) && !segment_is_hash(p->p_flags) &&
		p->p_memsz;
}

static int slate_setup_region(struct qcom_slate *slate_data, const struct pil_mdt *mdt)
{
	const struct elf32_phdr *phdr;
	phys_addr_t min_addr_r, min_addr_n, max_addr_r, max_addr_n, start, end;
	size_t align = 0;
	int i, ret = 0;
	bool relocatable = false;

	min_addr_n = min_addr_r = (phys_addr_t)ULLONG_MAX;
	max_addr_n = max_addr_r = 0;

	/* Find the image limits */
	for (i = 0; i < mdt->hdr.e_phnum; i++) {
		phdr = &mdt->phdr[i];
		if (!segment_is_loadable(phdr))
			continue;

		start = phdr->p_paddr;
		end = start + phdr->p_memsz;

		if (segment_is_relocatable(phdr)) {
			min_addr_r = min(min_addr_r, start);
			max_addr_r = max(max_addr_r, end);
			/*
			 * Lowest relocatable segment dictates alignment of
			 * relocatable region
			 */
			if (min_addr_r == start)
				align = phdr->p_align;
			relocatable = true;
		} else {
			min_addr_n = min(min_addr_n, start);
			max_addr_n = max(max_addr_n, end);
		}
	}

	/* Align the max address to the next 4K boundary to satisfy iommus and
	 * XPUs that operate on 4K chunks.
	 */

	max_addr_n = ALIGN(max_addr_n, SZ_4K);
	max_addr_r = ALIGN(max_addr_r, SZ_4K);

	if (relocatable) {
		ret = slate_alloc_region(slate_data, min_addr_r, max_addr_r, align);
	} else {
		slate_data->region_start = min_addr_n;
		slate_data->region_end = max_addr_n;
	}

	return ret;
}

static int slate_auth_metadata(struct qcom_slate *slate_data, const u8 *metadata, size_t size)
{
	struct tzapp_slate_req slate_tz_req;
	struct qtee_shm shm;
	int ret = 0;

	ret = qtee_shmbridge_allocate_shm(size, &shm);
	if (ret) {
		pr_err("Shmbridge memory allocation failed\n");
		return ret;
	}

	/* Make sure there are no mapping in PKMAP and fixmap */
	kmap_flush_unused();

	memcpy(shm.vaddr, metadata, size);

	slate_tz_req.tzapp_slate_cmd = SLATE_RPROC_AUTH_MDT;
	slate_tz_req.address_fw = shm.paddr;
	slate_tz_req.size_fw = size;

	ret = slate_tzapp_comm(slate_data, &slate_tz_req);
	if (ret || slate_data->cmd_status) {
		dev_err(slate_data->dev,
			"%s: Metadata loading is failed\n",
			__func__);
		ret = slate_data->cmd_status;
		goto tzapp_com_failed;
	}
	pr_debug("Metadata loaded successfully\n");

tzapp_com_failed:
	qtee_shmbridge_free_shm(&shm);
	return ret;
}

static int slate_load(struct rproc *rproc, const struct firmware *fw)
{
	struct qcom_slate *slate_data = (struct qcom_slate *)rproc->priv;
	int ret = 0;
	const struct pil_mdt *mdt;

	if (!slate_data) {
		pr_err("%s - Invalid slate pointer !!\n",
			__func__);
		return -EINVAL;
	}

	/* Enable status and err fetal irqs */
	enable_irq(slate_data->status_irq);

	/* boot slate from flash */
	if (get_slate_boot_mode()) {
		if (gpio_get_value(slate_data->gpios[0]))
			return RESULT_SUCCESS;
		ret = slate_flash_mode(slate_data);
		if (!ret)
			return RESULT_SUCCESS;
		dev_err(slate_data->dev,
			"%s: Failed to boot slate from flash\n",
			__func__);
		return ret;
	}

	mdt = (const struct pil_mdt *)fw->data;
	ret = slate_setup_region(slate_data, mdt);
	if (ret) {
		dev_err(slate_data->dev, "%s: slate memory_setup failure\n",
			__func__);
		return ret;
	}
	pr_debug("Loading from %pa tp %pa\n", &slate_data->region_start,
			&slate_data->region_end);

	ret = qcom_mdt_load_no_init(slate_data->dev, fw, rproc->firmware, 0,
		slate_data->region, slate_data->region_start,
		slate_data->region_size, NULL);
	if (ret) {
		dev_err(slate_data->dev, "%s: slate memory setup failure\n",
			__func__);
		return ret;
	}

	/* send the metadata */
	ret = slate_auth_metadata(slate_data, fw->data, fw->size);
	if (ret) {
		dev_err(slate_data->dev, "%s: slate TZ app load failure\n",
			__func__);
		return ret;
	}
	return ret;
}

static int slate_stop(struct rproc *rproc)
{
	struct qcom_slate *slate_data = rproc->priv;
	struct tzapp_slate_req slate_tz_req;
	int ret = RESULT_FAILURE;

	if (!slate_data) {
		pr_err("%s - Invalid slate pointer !!\n",
			__func__);
		return -EINVAL;
	}

	if (!slate_data->is_ready) {
		dev_err(slate_data->dev, "%s: Slate is not up!\n", __func__);
		return ret;
	}

	/* Dont send shutdown cmd if slate is crashed or offline state*/
	if (!gpio_get_value(slate_data->gpios[0]))
		goto out;

	ret = load_slate_tzapp(slate_data);
	if (ret) {
		dev_err(slate_data->dev,
			"%s: SLATE TZ app load failure\n",
			__func__);
		return ret;
	}
	slate_data->cmd_status = 0;
	slate_tz_req.tzapp_slate_cmd = SLATE_RPROC_SHUTDOWN;
	slate_tz_req.address_fw = 0;
	slate_tz_req.size_fw = 0;

	ret = slate_tzapp_comm(slate_data, &slate_tz_req);
	if (ret || slate_data->cmd_status) {
		pr_debug("Slate pil shutdown failed\n");
		return ret;
	}

	/* wait for slate shutdown completion in exclusive slate shutdown request */
	if (is_slate_unload_only()) {
		ret = wait_for_shutdown_done(slate_data);
		if (ret) {
			dev_err(slate_data->dev,
			"[%s:%d]: Timed out waiting for error ready: %s!\n",
			current->comm, current->pid, slate_data->firmware_name);
			return ret;
		}
	}
out:
	disable_irq(slate_data->status_irq);
	slate_data->is_ready = false;

	return 0;
}

static void *slate_da_to_va(struct rproc *rproc, u64 da, size_t len, bool *is_iomem)
{
	struct qcom_slate *slate_data = (struct qcom_slate *)rproc->priv;
	int offset;

	offset = da - slate_data->mem_phys;
	if (offset < 0 || offset + len > slate_data->mem_size)
		return NULL;

	return slate_data->mem_region + offset;
}

static const struct rproc_ops slate_ops = {
	.prepare = slate_prepare,
	.get_boot_addr = rproc_elf_get_boot_addr,
	.load = slate_load,
	.start = slate_start,
	.stop = slate_stop,
	.coredump = slate_coredump,
	.da_to_va = slate_da_to_va,
};

static int slate_app_reboot_notify(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct qcom_slate *slate_data = container_of(nb,
					struct qcom_slate, reboot_nb);

	/* Disable irq if already SLATE is up */
	if (slate_data->is_ready) {
		disable_irq(slate_data->status_irq);
		slate_data->is_ready = false;
	}
	/* Toggle AP2SLATE err fetal gpio here to inform apps err fetal event */
	if (gpio_is_valid(slate_data->gpios[1])) {
		pr_debug("Sending reboot signal\n");
		gpio_set_value(slate_data->gpios[1], 1);
	}
	return NOTIFY_DONE;
}

#ifdef CONFIG_DEEPSLEEP
static int rproc_slate_suspend(struct device *dev)
{
	if (pm_suspend_via_firmware()) {
		struct qcom_slate *slate_data = dev_get_drvdata(dev);

		qseecom_shutdown_app(&slate_data->qseecom_handle);
		slate_data->qseecom_handle = NULL;
	}
	return 0;
}

static int rproc_slate_resume(struct device *dev)
{
	return 0;
}
#endif

static int rproc_slate_driver_probe(struct platform_device *pdev)
{
	struct qcom_slate *slate;
	struct rproc *rproc;
	const char *fw_name;
	int ret;
	char md_dev_name[32];

	ret = of_property_read_string(pdev->dev.of_node,
			"qcom,firmware-name", &fw_name);
	if (ret)
		return ret;

	mutex_init(&cmdsync_lock);
	rproc = rproc_alloc(&pdev->dev, pdev->name, &slate_ops,
			fw_name, sizeof(*slate));
	if (!rproc) {
		dev_err(&pdev->dev, "unable to allocate remoteproc\n");
		mutex_destroy(&cmdsync_lock);
		return -ENOMEM;
	}

	rproc_coredump_set_elf_info(rproc, ELFCLASS32, EM_NONE);

	slate = (struct qcom_slate *)rproc->priv;

	slate->dev = &pdev->dev;

	/* Add wake lock */
	wakeup_source_add(&slate->rproc_slate_ws);

	/* Read GPIO configuration */
	ret = slate_dt_parse_gpio(pdev, slate);
	if (ret)
		goto free_rproc;

	ret = setup_slate_gpio_irq(pdev, slate);
	if (ret < 0)
		goto free_rproc;
	/* Disable S2A irq bydefault */
	disable_irq(slate->status_irq);

	slate->firmware_name = fw_name;
	slate->app_status = RESULT_FAILURE;
	rproc->dump_conf = RPROC_COREDUMP_ENABLED;
	rproc->recovery_disabled = true;
	rproc->auto_boot = false;
	slate->rproc = rproc;
	slate->sysmon_name = "slatefw";
	slate->ssr_name = "slatefw";
	slate->ssctl_id = -EINVAL;
	platform_set_drvdata(pdev, slate);

	/* Register SSR subdev to rproc*/
	qcom_add_ssr_subdev(rproc, &slate->ssr_subdev, slate->ssr_name);
	qcom_add_glink_subdev(rproc, &slate->glink_subdev, slate->ssr_name);

	slate->sysmon = qcom_add_sysmon_subdev(rproc, slate->sysmon_name,
			slate->ssctl_id);
	if (IS_ERR(slate->sysmon)) {
		ret = PTR_ERR(slate->sysmon);
		dev_err(slate->dev, "%s: Error while adding sysmon subdevice:[%d]\n",
				__func__, ret);
		goto free_rproc;
	}

	ret = device_create_file(slate->dev, &dev_attr_txn_id);
	if (ret)
		goto remove_subdev;

	/* Register callback for handling reboot */
	slate->reboot_nb.notifier_call = slate_app_reboot_notify;
	register_reboot_notifier(&slate->reboot_nb);

	slate->slate_queue = alloc_workqueue("slate_queue", 0, 0);
	if (!slate->slate_queue) {
		dev_err(slate->dev, "%s: creation of slate_queue failed\n",
				__func__);
		goto unregister_notify;
	}

	/* Initialize work queue for reset handler */
	INIT_WORK(&slate->restart_work, slate_restart_work);

	snprintf(md_dev_name, ARRAY_SIZE(md_dev_name),
			"%s-md", pdev->dev.of_node->name);

	/* Register with rproc */
	ret = rproc_add(rproc);
	if (ret)
		goto destroy_wq;

	init_completion(&slate->err_ready);
	init_completion(&slate->shutdown_done);
	pr_debug("Slate probe is completed\n");
	return 0;

destroy_wq:
	destroy_workqueue(slate_reset_wq);
unregister_notify:
	unregister_reboot_notifier(&slate->reboot_nb);
remove_subdev:
	qcom_remove_sysmon_subdev(slate->sysmon);
free_rproc:
	rproc_free(rproc);
	mutex_destroy(&cmdsync_lock);

	return ret;
}

static int rproc_slate_driver_remove(struct platform_device *pdev)
{
	struct qcom_slate *slate = platform_get_drvdata(pdev);

	if (slate_reset_wq)
		destroy_workqueue(slate_reset_wq);
	device_remove_file(slate->dev, &dev_attr_txn_id);
	qcom_remove_glink_subdev(slate->rproc, &slate->glink_subdev);
	qcom_remove_sysmon_subdev(slate->sysmon);
	qcom_remove_ssr_subdev(slate->rproc, &slate->ssr_subdev);
	unregister_reboot_notifier(&slate->reboot_nb);
	rproc_del(slate->rproc);
	rproc_free(slate->rproc);

	return 0;
}

static const struct dev_pm_ops rproc_slate_pm_ops = {
#ifdef CONFIG_DEEPSLEEP
	.suspend = rproc_slate_suspend,
	.resume = rproc_slate_resume,
#endif
};

static const struct of_device_id rproc_slate_match_table[] = {
	{.compatible = "qcom,rproc-slate"},
	{}
};
MODULE_DEVICE_TABLE(of, rproc_slate_match_table);

static struct platform_driver rproc_slate_driver = {
	.probe = rproc_slate_driver_probe,
	.remove = rproc_slate_driver_remove,
	.driver = {
		.name = "qcom-rproc-slate",
		.of_match_table = rproc_slate_match_table,
		.pm = &rproc_slate_pm_ops,
	},
};

module_platform_driver(rproc_slate_driver);
MODULE_DESCRIPTION("Support for booting QTI slate SoC");
MODULE_LICENSE("GPL");
