// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/slab.h>
#include <linux/soc/qcom/mdt_loader.h>
#include <misc/qseecom_kernel.h>
#include <soc/qcom/qseecomi.h>
#include <soc/qcom/ramdump.h>

#include "../soc/qcom/helioscom.h"

#include "qcom_common.h"
#include "qcom_pil_info.h"
#include "remoteproc_internal.h"

#define SECURE_APP "heliosapp"

#define RESULT_SUCCESS		0
#define RESULT_FAILURE		-1

/* Helios Ramdump Size 3100 KB */
#define HELIOS_RAMDUMP_SZ	(0x307000)

/* tzapp command list.*/
enum helios_tz_commands {
	HELIOS_RPROC_RAMDUMP,
	HELIOS_RPROC_IMAGE_LOAD,
	HELIOS_RPROC_AUTH_MDT,
	HELIOS_RPROC_DLOAD_CONT,
	HELIOS_RPROC_GET_HELIOS_VERSION,
	HELIOS_RPROC_SHUTDOWN,
	HELIOS_RPROC_DUMPINFO,
	HELIOS_RPROC_UP_INFO,
	HELIOS_RPROC_RESTART,
	HELIOS_RPROC_POWERDOWN,
};

/* tzapp bg request.*/
struct tzapp_helios_req {
	uint8_t tzapp_helios_cmd;
	uint8_t padding[3];
	u32 address_fw;
	size_t size_fw;
} __attribute__((__packed__));

/* tzapp bg response.*/
struct tzapp_helios_rsp {
	uint32_t tzapp_helios_cmd;
	uint32_t helios_info_len;
	int32_t status;
	uint32_t helios_info[100];
} __attribute__((__packed__));

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
 * struct qcom_helios
 * @dev: Device pointer
 * @rproc: Remoteproc handle for helios
 * @firmware_name: FW image file name
 * @ssr_subdev: SSR subdevice to be registered with remoteproc
 * @ssr_name: SSR subdevice name used as reference in remoteproc
 * @config_type: config handle registered with heliosCOM
 * @address_fw: address where firmware binaries loaded in DMA
 * @size_fw: size of helios firmware binaries in DMA
 * @qseecom_handle: handle of TZ app
 * @cmd_status: qseecom command status
 * @app_status: status of tz app loading
 * @is_ready: Is helios chip up
 * @err_ready: The error ready signal
 * @region_start: DMA handle for loading FW
 * @region_end: DMA address indicating end of DMA buffer
 * @region: CPU address for DMA buffer
 * @is_region_allocated: Is DMA buffer allocated
 * @region_size: DMA buffer size for FW
 */
struct qcom_helios {
	struct device *dev;
	struct rproc *rproc;

	const char *firmware_name;

	struct qcom_rproc_ssr ssr_subdev;
	const char *ssr_name;

	struct helioscom_reset_config_type config_type;

	phys_addr_t address_fw;
	size_t size_fw;

	struct qseecom_handle *qseecom_handle;
	u32 cmd_status;
	int app_status;
	bool is_ready;
	struct completion err_ready;

	phys_addr_t region_start;
	phys_addr_t region_end;
	void *region;
	bool is_region_allocated;
	size_t region_size;
};

/* Callback function registered with helioscom which triggers restart flow */
static void helios_crash_handler(void *handle, void *priv)
{
	struct rproc *helios_rproc = (struct rproc *)priv;

	pr_debug("Helios is crashed! Starting recovery...\n");
	rproc_report_crash(helios_rproc, RPROC_FATAL_ERROR);
}

/**
 * get_cmd_rsp_buffers() - Function sets cmd & rsp buffer pointers and
 *						   aligns buffer lengths
 * @handle: index of qseecom_handle
 * @cmd: req buffer - set to qseecom_handle.sbuf
 * @cmd_len: ptr to req buffer len
 * @rsp: rsp buffer - set to qseecom_handle.sbuf + offset
 * @rsp_len: ptr to rsp buffer len
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
 * load_helios_tzapp() - Called to load TZ app.
 * @pbd: struct containing private <helios> data.
 *
 * Return: 0 on success. Error code on failure.
 */
static int load_helios_tzapp(struct qcom_helios *pbd)
{
	int rc;

	/* return success if already loaded */
	if (pbd->qseecom_handle && !pbd->app_status)
		return 0;

	/* Load the APP */
	pr_debug("Start loading of secure app\n");
	rc = qseecom_start_app(&pbd->qseecom_handle, SECURE_APP, SZ_4K);
	if (rc < 0) {
		dev_err(pbd->dev, "<helios> TZ app load failure\n");
		pbd->app_status = RESULT_FAILURE;
		return -EIO;
	}
	pbd->app_status = RESULT_SUCCESS;
	pr_debug("App loaded\n");
	return 0;
}

/**
 * helios_tzapp_comm() - Function called to communicate with TZ APP.
 * @pbd: struct containing private <helios> data.
 * @req: struct containing command and parameters.
 *
 * Return: 0 on success. Error code on failure.
 */
static long helios_tzapp_comm(struct qcom_helios *pbd,
		struct tzapp_helios_req *req)
{
	struct tzapp_helios_req *helios_tz_req;
	struct tzapp_helios_rsp *helios_tz_rsp;
	int rc, req_len, rsp_len;

	/* Fill command structure */
	req_len = sizeof(struct tzapp_helios_req);
	rsp_len = sizeof(struct tzapp_helios_rsp);
	rc = get_cmd_rsp_buffers(pbd->qseecom_handle,
			(void **)&helios_tz_req, &req_len,
			(void **)&helios_tz_rsp, &rsp_len);
	if (rc)
		goto end;

	helios_tz_req->tzapp_helios_cmd = req->tzapp_helios_cmd;
	helios_tz_req->address_fw = req->address_fw;
	helios_tz_req->size_fw = req->size_fw;

	rc = qseecom_send_command(pbd->qseecom_handle,
			(void *)helios_tz_req, req_len, (void *)helios_tz_rsp, rsp_len);
	if (rc || helios_tz_rsp->status)
		pbd->cmd_status = helios_tz_rsp->status;
	else
		pbd->cmd_status = 0;

end:
	return rc;
}

/**
 * helios_auth_metadata() - Called by load operation of remoteproc framework
 * send command to tz app for authentication of metadata.
 * @helios_data: struct containing private <helios> data
 * @metadata: metadata load address
 * @size: size of metadata
 *
 * Return: 0 on success. Error code on failure.
 */
static int helios_auth_metadata(struct qcom_helios *helios_data,
		const u8 *metadata, size_t size)
{
	struct tzapp_helios_req helios_tz_req;
	struct qtee_shm shm;
	int ret;

	ret = qtee_shmbridge_allocate_shm(size, &shm);
	if (ret) {
		pr_err("Shmbridge memory allocation failed\n");
		return ret;
	}

	/* Make sure there are no mappings in PKMAP and fixmap */
	kmap_flush_unused();

	memcpy(shm.vaddr, metadata, size);

	helios_tz_req.tzapp_helios_cmd = HELIOS_RPROC_AUTH_MDT;
	helios_tz_req.address_fw = shm.paddr;
	helios_tz_req.size_fw = size;

	ret = helios_tzapp_comm(helios_data, &helios_tz_req);
	if (ret || helios_data->cmd_status) {
		dev_err(helios_data->dev,
				"%s: Metadata loading failed\n",
				__func__);
		ret = helios_data->cmd_status;
		goto tzapp_com_failed;
	}

	pr_debug("Metadata loaded successfully\n");

tzapp_com_failed:
	qtee_shmbridge_free_shm(&shm);
	return ret;
}

/**
 * helios_auth_and_xfer() - Called by start operation of remoteproc framework
 * to signal tz app to authenticate and boot helios chip.
 * @helios_data: struct containing private <helios> data.
 *
 * Return: 0 on success. Error code on failure.
 */
static int helios_auth_and_xfer(struct qcom_helios *helios_data)
{
	struct tzapp_helios_req helios_tz_req;
	uint32_t ns_vmids[] = {VMID_HLOS};
	uint32_t ns_vm_perms[] = {PERM_READ | PERM_WRITE};
	u64 shm_bridge_handle;
	int ret;

	ret = qtee_shmbridge_register(helios_data->address_fw, helios_data->size_fw,
			ns_vmids, ns_vm_perms, 1, PERM_READ | PERM_WRITE,
			&shm_bridge_handle);

	if (ret) {
		dev_err(helios_data->dev,
				"%s: Failed to create shm bridge [%d]\n",
				__func__, ret);
		return ret;
	}

	helios_tz_req.tzapp_helios_cmd = HELIOS_RPROC_IMAGE_LOAD;
	helios_tz_req.address_fw = helios_data->address_fw;
	helios_tz_req.size_fw = helios_data->size_fw;

	ret = helios_tzapp_comm(helios_data, &helios_tz_req);
	if (ret || helios_data->cmd_status) {
		dev_err(helios_data->dev,
				"%s: Firmware image authentication failed\n",
				__func__);
		ret = helios_data->cmd_status;
		goto tzapp_comm_failed;
	}

	/* helios Transfer of image is complete, free up the memory */
	pr_debug("Firmware authentication and transfer done\n");

tzapp_comm_failed:
	qtee_shmbridge_deregister(shm_bridge_handle);
	return ret;
}

/**
 * helios_prepare() - Called by rproc_boot. This loads tz app.
 * @rproc: struct containing private helios data.
 *
 * Return: 0 on success. Error code on failure.
 */
static int helios_prepare(struct rproc *rproc)
{
	struct qcom_helios *helios = (struct qcom_helios *)rproc->priv;
	int ret = 0;

	init_completion(&helios->err_ready);
	if (!helios->qseecom_handle) {
		ret = load_helios_tzapp(helios);
		if (ret) {
			dev_err(helios->dev,
					"%s: helios TZ app load failure\n",
					__func__);
			return ret;
		}
		helios->is_ready = true;
	}
	pr_debug("heliosapp loaded\n");
	return ret;
}

#define segment_is_hash(flag) (((flag) & (0x7 << 24)) == (0x2 << 24))

static int segment_is_loadable(const struct elf32_phdr *p)
{
	return (p->p_type == PT_LOAD) && !segment_is_hash(p->p_flags) &&
		p->p_memsz;
}

static bool segment_is_relocatable(const struct elf32_phdr *p)
{
	return !!(p->p_flags & BIT(27));
}

static int helios_alloc_mem(struct qcom_helios *helios, size_t aligned_size)
{
	helios->region = dma_alloc_coherent(helios->dev, aligned_size,
			&helios->region_start, GFP_KERNEL);

	if (!helios->region)
		return -ENOMEM;

	return 0;
}

static int helios_alloc_region(struct qcom_helios *helios, phys_addr_t min_addr,
		phys_addr_t max_addr, size_t align)
{
	size_t size = max_addr - min_addr;
	size_t aligned_size;
	int ret;

	/* Don't reallocate due to fragmentation concerns, just sanity check */
	if (helios->is_region_allocated) {
		if (WARN(helios->region_end - helios->region_start < size,
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

	ret = helios_alloc_mem(helios, aligned_size);
	if (ret) {
		dev_err(helios->dev,
				"%s: Failed to allocate relocatable region\n",
				__func__);
		helios->region_start = 0;
		helios->region_end = 0;
		return ret;
	}

	helios->is_region_allocated = true;
	helios->region_end = helios->region_start + size;
	helios->region_size = aligned_size;

	return 0;
}

static int helios_setup_region(struct qcom_helios *helios, const struct pil_mdt *mdt)
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

	/*
	 * Align the max address to the next 4K boundary to satisfy iommus and
	 * XPUs that operate on 4K chunks.
	 */
	max_addr_n = ALIGN(max_addr_n, SZ_4K);
	max_addr_r = ALIGN(max_addr_r, SZ_4K);

	if (relocatable) {
		ret = helios_alloc_region(helios, min_addr_r, max_addr_r, align);
	} else {
		helios->region_start = min_addr_n;
		helios->region_end = max_addr_n;
	}

	return ret;
}

static int helios_load(struct rproc *rproc, const struct firmware *fw)
{
	struct qcom_helios *helios = (struct qcom_helios *)rproc->priv;
	int ret = 0;
	const struct pil_mdt *mdt;

	mdt = (const struct pil_mdt *)fw->data;
	ret = helios_setup_region(helios, mdt);
	if (ret) {
		dev_err(helios->dev, "%s: helios memory setup failure\n", __func__);
		return ret;
	}
	pr_debug("Loading from %pa to %pa\n", &helios->region_start,
			&helios->region_end);

	ret = qcom_mdt_load_no_init(helios->dev, fw, rproc->firmware, 0,
			helios->region, helios->region_start, helios->region_size,
			NULL);
	if (ret) {
		dev_err(helios->dev, "%s: helios memory setup failure\n", __func__);
		return ret;
	}

	/* Send the metadata */
	ret = helios_auth_metadata(helios, fw->data, fw->size);
	if (ret) {
		dev_err(helios->dev, "%s: helios TZ app load failure\n", __func__);
		return ret;
	}

	return 0;
}

static int helios_start(struct rproc *rproc)
{
	struct qcom_helios *helios = (struct qcom_helios *)rproc->priv;
	int ret = 0;

	helios->address_fw = helios->region_start;
	helios->size_fw = helios->region_end - helios->region_start;
	ret = helios_auth_and_xfer(helios);
	if (ret) {
		dev_err(helios->dev, "%s: helios TZ app load failure\n", __func__);
		return ret;
	}

	pr_debug("Helios is booted up!\n");

	dma_free_coherent(helios->dev, helios->region_size, helios->region,
			helios->region_start);
	helios->is_region_allocated = false;
	helios->region = NULL;
	helios->region_start = 0;
	helios->region_end = 0;
	helios->region_size = 0;

	return 0;
}

static void dumpfn(struct rproc *rproc, struct rproc_dump_segment *segment,
		void *dest, size_t offset, size_t size)
{
	if (segment)
		memcpy(dest, segment->priv, size);
	pr_debug("Dump Segment Added\n");
}

static void helios_coredump(struct rproc *rproc)
{
	struct qcom_helios *helios = (struct qcom_helios *)rproc->priv;
	struct tzapp_helios_req helios_tz_req;
	uint32_t ns_vmids[] = {VMID_HLOS};
	uint32_t ns_vm_perms[] = {PERM_READ | PERM_WRITE};
	u64 shm_bridge_handle;
	phys_addr_t start_addr;
	void *region;
	int ret;
	unsigned long size = HELIOS_RAMDUMP_SZ;

	rproc_coredump_cleanup(rproc);

	region = dma_alloc_attrs(helios->dev, size,
				&start_addr, GFP_KERNEL, DMA_ATTR_SKIP_ZEROING);
	if (region == NULL) {
		dev_err(helios->dev,
			"Helios failure to allocate ramdump region of size %zx\n",
			size);
		return;
	}

	ret = qtee_shmbridge_register(start_addr, size,
		ns_vmids, ns_vm_perms, 1, PERM_READ|PERM_WRITE,
		&shm_bridge_handle);

	if (ret) {
		dev_err(helios->dev,
				"%s: Failed to create shm bridge. ret=[%d]\n",
				__func__, ret);
		dma_free_attrs(helios->dev, size, region,
			start_addr, DMA_ATTR_SKIP_ZEROING);
		return;
	}

	helios_tz_req.tzapp_helios_cmd = HELIOS_RPROC_RAMDUMP;
	helios_tz_req.address_fw = start_addr;
	helios_tz_req.size_fw = size;

	ret = helios_tzapp_comm(helios, &helios_tz_req);
	if (ret || helios->cmd_status) {
		dev_err(helios->dev, "%s: Helios Ramdump collection failed\n", __func__);
		return;
	}

	pr_debug("Add coredump segment!\n");
	ret = rproc_coredump_add_custom_segment(rproc, start_addr, size, &dumpfn, region);
	if (ret) {
		dev_err(helios->dev, "failed to add rproc segment: %d\n", ret);
		qtee_shmbridge_deregister(shm_bridge_handle);
		rproc_coredump_cleanup(helios->rproc);
		return;
	}

	rproc_coredump(rproc);

	qtee_shmbridge_deregister(shm_bridge_handle);
	dma_free_attrs(helios->dev, size, region,
			   start_addr, DMA_ATTR_SKIP_ZEROING);
}

static int helios_shutdown(struct rproc *rproc)
{
	struct qcom_helios *helios = (struct qcom_helios *)rproc->priv;
	struct tzapp_helios_req helios_tz_req;
	int ret;

	helios_tz_req.tzapp_helios_cmd = HELIOS_RPROC_RESTART;
	helios_tz_req.address_fw = 0;
	helios_tz_req.size_fw = 0;

	ret = helios_tzapp_comm(helios, &helios_tz_req);
	if (!ret && !helios->cmd_status) {
		/* Helios Shutdown is success but Helios is responding.
		 * In this case, collect ramdump and store it.
		 */
		pr_info("A2H response is received! Collect ramdump now!\n");
		helios_coredump(rproc);
	} else {
		dev_err(helios->dev, "%s: Helios restart failed\n", __func__);
		ret = helios->cmd_status;
	}

	return ret;
}

/**
 * helios_stop() - Called by stop operation of remoteproc framework
 * Triggers a watchdog bite in helios to start ramdump collection
 * @rproc: struct containing private helios data.
 *
 * Return: always success
 */
static int helios_stop(struct rproc *rproc)
{
	struct qcom_helios *helios = (struct qcom_helios *)rproc->priv;
	int ret = 0;

	/* In case of crash, STOP operation is dummy */
	if (rproc->state == RPROC_CRASHED) {
		pr_err("Helios is crashed!. No need to do anything here! Collect ramdump directly.\n");
		return ret;
	}

	if (helios->is_ready)
		ret = helios_shutdown(rproc);

	pr_info("Helios Shutdown is success\n");
	return ret;
}

static const struct rproc_ops helios_ops = {
	.prepare = helios_prepare,
	.get_boot_addr = rproc_elf_get_boot_addr,
	.load = helios_load,
	.start = helios_start,
	.stop = helios_stop,
	.coredump = helios_coredump,
};

static int rproc_helios_driver_probe(struct platform_device *pdev)
{
	struct qcom_helios *helios;
	struct rproc *rproc;
	const char *fw_name;
	const char *ssr_name;
	void *config_handle = NULL;
	int ret;

	ret = of_property_read_string(pdev->dev.of_node,
			"qcom,firmware-name", &fw_name);
	if (ret)
		return ret;

	ret = of_property_read_string(pdev->dev.of_node,
			"qcom,ssr-name", &ssr_name);
	if (ret)
		return ret;

	rproc = rproc_alloc(&pdev->dev, pdev->name, &helios_ops,
			fw_name, sizeof(*helios));
	if (!rproc) {
		dev_err(&pdev->dev, "unable to allocate remoteproc\n");
		return -ENOMEM;
	}
	rproc_coredump_set_elf_info(rproc, ELFCLASS32, EM_NONE);

	helios = (struct qcom_helios *)rproc->priv;
	helios->ssr_name = ssr_name;
	helios->firmware_name = fw_name;
	helios->dev = &pdev->dev;
	rproc->dump_conf = RPROC_COREDUMP_ENABLED;
	rproc->recovery_disabled = false;
	rproc->auto_boot = false;
	helios->rproc = rproc;
	platform_set_drvdata(pdev, helios);

	qcom_add_ssr_subdev(rproc, &helios->ssr_subdev, helios->ssr_name);

	/* Register callback for Helios Crash with heliosCom */
	helios->config_type.priv = (void *)rproc;
	helios->config_type.helioscom_reset_notification_cb = helios_crash_handler;
	config_handle = helioscom_pil_reset_register(&helios->config_type);
	if (!config_handle) {
		ret = -ENOMEM;
		dev_err(helios->dev, "%s: Invalid Handle\n", __func__);
		goto free_rproc;
	}

	/* Register with rproc */
	ret = rproc_add(rproc);
	if (ret)
		goto free_rproc;

	pr_debug("Helios probe is completed\n");
	return 0;

free_rproc:
	rproc_free(rproc);

	return ret;
}

static int rproc_helios_driver_remove(struct platform_device *pdev)
{
	struct qcom_helios *helios = platform_get_drvdata(pdev);

	rproc_del(helios->rproc);
	rproc_free(helios->rproc);

	return 0;
}

static const struct of_device_id rproc_helios_match_table[] = {
	{.compatible = "qcom,rproc-helios"},
	{}
};
MODULE_DEVICE_TABLE(of, rproc_helios_match_table);

static struct platform_driver rproc_helios_driver = {
	.probe = rproc_helios_driver_probe,
	.remove = rproc_helios_driver_remove,
	.driver = {
		.name = "qcom-rproc-helios",
		.of_match_table = rproc_helios_match_table,
	},
};

module_platform_driver(rproc_helios_driver);
MODULE_DESCRIPTION("Support for booting QTI helios SoC");
MODULE_LICENSE("GPL v2");
