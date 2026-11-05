// SPDX-License-Identifier: GPL-2.0-only
/*
 * Qualcomm ADSP/SLPI Peripheral Image Loader for MSM8974 and MSM8996
 *
 * Copyright (C) 2016 Linaro Ltd
 * Copyright (C) 2014 Sony Mobile Communications AB
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/panic_notifier.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/regulator/consumer.h>
#include <linux/remoteproc.h>
#include <linux/soc/qcom/mdt_loader.h>
#include <linux/soc/qcom/smem.h>
#include <linux/soc/qcom/smem_state.h>
#include <linux/soc/qcom/qcom_aoss.h>
#include <soc/qcom/qcom_ramdump.h>
#include <soc/qcom/secure_buffer.h>
#include <trace/hooks/remoteproc.h>
#include <linux/iopoll.h>
#include <linux/refcount.h>
#include <trace/events/rproc_qcom.h>
#include <linux/interconnect.h>

#include "qcom_common.h"
#include "qcom_pil_info.h"
#include "qcom_q6v5.h"
#include "remoteproc_internal.h"

#define ADSP_DECRYPT_SHUTDOWN_DELAY_MS	100
#define RPROC_HANDOVER_POLL_DELAY_MS	1

#define MAX_ASSIGN_COUNT 3

#define to_rproc(d) container_of(d, struct rproc, dev)

#define SOCCP_SLEEP_US  100
#define SOCCP_TIMEOUT_US  10000
#define SOCCP_STATE_MASK 0x600
#define SPARE_REG_SOCCP_D0  0x1
#define SOCCP_D0  0x2
#define SOCCP_D1  0x4
#define SOCCP_D3  0x8

struct adsp_data {
	int crash_reason_smem;
	int crash_reason_stack;
	unsigned int smem_host_id;
	const char *firmware_name;
	const char *dtb_firmware_name;
	int pas_id;
	int dtb_pas_id;
	unsigned int minidump_id;
	bool both_dumps;
	bool uses_elf64;
	bool auto_boot;
	bool decrypt_shutdown;

	char **proxy_pd_names;

	const char *load_state;
	const char *ssr_name;
	const char *sysmon_name;
	int ssctl_id;

	int region_assign_idx;
	int region_assign_count;
	bool region_assign_shared;
	int region_assign_vmid;
	bool dma_phys_below_32b;
	bool check_status;
};

struct qcom_adsp {
	struct device *dev;
	struct device *minidump_dev;
	struct rproc *rproc;

	struct qcom_q6v5 q6v5;

	struct clk *xo;
	struct clk *aggre2_clk;

	struct regulator *cx_supply;
	struct regulator *px_supply;
	struct reg_info *regs;
	int reg_cnt;

	struct device *proxy_pds[3];

	int proxy_pd_count;

	const char *dtb_firmware_name;
	int pas_id;
	int dtb_pas_id;
	unsigned int minidump_id;
	bool both_dumps;
	int crash_reason_smem;
	int crash_reason_stack;
	unsigned int smem_host_id;
	bool decrypt_shutdown;
	const char *info_name;

	const struct firmware *firmware;
	const struct firmware *dtb_firmware;

	struct completion start_done;
	struct completion stop_done;

	phys_addr_t mem_phys;
	phys_addr_t dtb_mem_phys;
	phys_addr_t mem_reloc;
	phys_addr_t dtb_mem_reloc;
	phys_addr_t region_assign_phys[MAX_ASSIGN_COUNT];
	void *mem_region;
	void *dtb_mem_region;
	size_t mem_size;
	size_t dtb_mem_size;

	size_t region_assign_size[MAX_ASSIGN_COUNT];

	int region_assign_idx;
	int region_assign_count;
	bool region_assign_shared;
	int region_assign_vmid;
	u64 region_assign_perms[MAX_ASSIGN_COUNT];

	bool dma_phys_below_32b;
	bool subsys_recovery_disabled;
	bool region_assigned;

	struct qcom_rproc_glink glink_subdev;
	struct qcom_rproc_subdev smd_subdev;
	struct qcom_rproc_ssr ssr_subdev;
	struct qcom_sysmon *sysmon;

	struct qcom_scm_pas_metadata pas_metadata;
	struct qcom_scm_pas_metadata dtb_pas_metadata;

	struct qcom_smem_state *wake_state;
	struct qcom_smem_state *sleep_state;
	struct notifier_block panic_blk;
	struct mutex adsp_lock;
	unsigned int wake_bit;
	unsigned int sleep_bit;
	refcount_t current_users;
	void *tcsr_addr;
	void *spare_reg_addr;
	bool check_status;
	bool rproc_ddr_set_icc_low_svs;
	unsigned int rproc_ddr_lowsvs_icc_bw;
};

static ssize_t txn_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct qcom_adsp *adsp = (struct qcom_adsp *)platform_get_drvdata(pdev);

	return sysfs_emit(buf, "%u\n", qcom_sysmon_get_txn_id(adsp->sysmon));
}
static DEVICE_ATTR_RO(txn_id);

static int adsp_custom_segment_dump(struct qcom_adsp *adsp,
				    struct rproc_dump_segment *segment,
				    void *dest, size_t offset, size_t size)
{
	int len = strlen("md_dbg_buf");
	void __iomem *base;
	int total_offset;
	bool valid = false;
	int i;

	if (segment->priv && strnlen(segment->priv, len + 1) == len &&
		    !strcmp(segment->priv, "md_dbg_buf"))
		goto custom_segment_dump;

	/*
	 * Also, do second level of check for custom segments in
	 * adsp_custom_segment_dump(), which checks if the segment
	 * lies outside the subsystem region range.
	 */
	for (i = 0; i < adsp->region_assign_count; i++) {
		total_offset = segment->da + segment->offset +
				offset - adsp->region_assign_phys[i];
		if (!(total_offset < 0 ||
				total_offset + size > adsp->region_assign_size[i])) {
			valid = true;
			break;
		}
	}

	if (!valid)
		return -EINVAL;

custom_segment_dump:
	base = ioremap((unsigned long)le64_to_cpu(segment->da) + offset, size);
	if (!base) {
		dev_err(adsp->dev, "failed to map custom_segment region\n");
		return -EINVAL;
	}

	memcpy_fromio(dest, base, size);
	iounmap(base);

	return 0;
}

void adsp_segment_dump(struct rproc *rproc, struct rproc_dump_segment *segment,
		     void *dest, size_t offset, size_t size)
{
	struct qcom_adsp *adsp = rproc->priv;
	int total_offset;

	total_offset = segment->da + segment->offset + offset - adsp->mem_phys;
	if (!(total_offset < 0 || total_offset + size > adsp->mem_size)) {
		memcpy_fromio(dest, adsp->mem_region + total_offset, size);
		return;
	} else if (!adsp_custom_segment_dump(adsp, segment, dest, offset, size)) {
		return;
	}

	dev_err(adsp->dev,
		"invalid copy request for segment %pad with offset %zu and size %zu)\n",
		&segment->da, offset, size);
	memset(dest, 0xff, size);
}

static void adsp_minidump(struct rproc *rproc)
{
	struct qcom_adsp *adsp = rproc->priv;

	trace_rproc_qcom_event(dev_name(adsp->dev), "adsp_minidump", "enter");

	if (rproc->dump_conf == RPROC_COREDUMP_DISABLED)
		goto exit;

	qcom_minidump(rproc, adsp->minidump_dev, adsp->minidump_id, adsp_segment_dump,
			adsp->both_dumps);

exit:
	trace_rproc_qcom_event(dev_name(adsp->dev), "adsp_minidump", "exit");
}

static void disable_regulators(struct qcom_adsp *adsp)
{
	int i;

	for (i = (adsp->reg_cnt - 1); i >= 0; i--) {
		regulator_set_voltage(adsp->regs[i].reg, 0, INT_MAX);
		regulator_set_load(adsp->regs[i].reg, 0);
		regulator_disable(adsp->regs[i].reg);
	}
}
static int enable_regulators(struct qcom_adsp *adsp)
{
	int i, rc = 0;

	for (i = 0; i < adsp->reg_cnt; i++) {
		regulator_set_voltage(adsp->regs[i].reg, adsp->regs[i].uV, INT_MAX);
		regulator_set_load(adsp->regs[i].reg, adsp->regs[i].uA);
		rc = regulator_enable(adsp->regs[i].reg);
		if (rc) {
			dev_err(adsp->dev, "Regulator enable failed(rc:%d)\n",
				rc);
			goto err_enable;
		}
	}
	return rc;

err_enable:
	disable_regulators(adsp);
	return rc;
}
static int adsp_pds_enable(struct qcom_adsp *adsp, struct device **pds,
			   size_t pd_count)
{
	int ret;
	int i;

	for (i = 0; i < pd_count; i++) {
		dev_pm_genpd_set_performance_state(pds[i], INT_MAX);
		ret = pm_runtime_get_sync(pds[i]);
		if (ret < 0) {
			pm_runtime_put_noidle(pds[i]);
			dev_pm_genpd_set_performance_state(pds[i], 0);
			goto unroll_pd_votes;
		}
	}

	return 0;

unroll_pd_votes:
	for (i--; i >= 0; i--) {
		dev_pm_genpd_set_performance_state(pds[i], 0);
		pm_runtime_put(pds[i]);
	}

	return ret;
};

static void adsp_pds_disable(struct qcom_adsp *adsp, struct device **pds,
			     size_t pd_count)
{
	int i;

	for (i = 0; i < pd_count; i++) {
		dev_pm_genpd_set_performance_state(pds[i], 0);
		pm_runtime_put(pds[i]);
	}
}

static int adsp_shutdown_poll_decrypt(struct qcom_adsp *adsp)
{
	unsigned int retry_num = 50;
	int ret;

	do {
		msleep(ADSP_DECRYPT_SHUTDOWN_DELAY_MS);
		ret = qcom_scm_pas_shutdown(adsp->pas_id);
	} while (ret == -EINVAL && --retry_num);

	return ret;
}

static int adsp_unprepare(struct rproc *rproc)
{
	struct qcom_adsp *adsp = rproc->priv;
	struct device *dev = NULL;

	if (adsp->dma_phys_below_32b)
		dev = adsp->dev;
	/*
	 * adsp_load() did pass pas_metadata to the SCM driver for storing
	 * metadata context. It might have been released already if
	 * auth_and_reset() was successful, but in other cases clean it up
	 * here.
	 */
	qcom_scm_pas_metadata_release(&adsp->pas_metadata, dev);
	if (adsp->dtb_pas_id)
		qcom_scm_pas_metadata_release(&adsp->dtb_pas_metadata, dev);

	return 0;
}

static void adsp_add_coredump_segments(struct qcom_adsp *adsp, const struct firmware *fw)
{
	struct rproc *rproc = adsp->rproc;
	struct rproc_dump_segment *entry;

	rproc_coredump_cleanup(rproc);
	if (qcom_register_dump_segments(rproc, fw) < 0) {
		rproc_coredump_cleanup(adsp->rproc);
		return;
	}

	list_for_each_entry(entry, &rproc->dump_segments, node)
		entry->da = adsp->mem_phys + entry->da - adsp->mem_reloc;
}

static int adsp_load(struct rproc *rproc, const struct firmware *fw)
{
	struct qcom_adsp *adsp = rproc->priv;
	struct device *dev = NULL;
	int ret = 0;

	trace_rproc_qcom_event(dev_name(adsp->dev), "adsp_load", "enter");

	if (adsp->dma_phys_below_32b)
		dev = adsp->dev;

	/* Store firmware handle to be used in adsp_start() */
	adsp->firmware = fw;

	qcom_q6v5_pas_set_bw(&adsp->q6v5, UINT_MAX, UINT_MAX);
	if (adsp->dtb_pas_id) {
		ret = request_firmware(&adsp->dtb_firmware, adsp->dtb_firmware_name, adsp->dev);
		if (ret) {
			dev_err(adsp->dev, "request_firmware failed for %s: %d\n",
				adsp->dtb_firmware_name, ret);
			goto exit_load;
		}

		ret = qcom_mdt_pas_init(adsp->dev, adsp->dtb_firmware, adsp->dtb_firmware_name,
					adsp->dtb_pas_id, adsp->dtb_mem_phys,
					&adsp->dtb_pas_metadata, adsp->dma_phys_below_32b);
		if (ret)
			goto release_dtb_firmware;

		ret = qcom_mdt_load_no_init(adsp->dev, adsp->dtb_firmware, adsp->dtb_firmware_name,
					    adsp->dtb_pas_id, adsp->dtb_mem_region,
					    adsp->dtb_mem_phys, adsp->dtb_mem_size,
					    &adsp->dtb_mem_reloc);
		if (ret)
			goto release_dtb_metadata;
	}

	goto exit_load;

release_dtb_metadata:
	if (adsp->dtb_pas_id && adsp->dma_phys_below_32b)
		qcom_scm_pas_shutdown(adsp->dtb_pas_id);

	if (adsp->dtb_pas_id)
		qcom_scm_pas_metadata_release(&adsp->dtb_pas_metadata, dev);
release_dtb_firmware:
	release_firmware(adsp->dtb_firmware);

exit_load:
	trace_rproc_qcom_event(dev_name(adsp->dev), "adsp_load", "exit");
	qcom_q6v5_pas_set_bw(&adsp->q6v5, 0, 0);

	return ret;
}

static void add_mpss_dsm_mem_ssr_dump(struct qcom_adsp *adsp)
{
	struct rproc *rproc = adsp->rproc;
	struct device_node *np;
	struct resource imem;
	void __iomem *base;
	int ret = 0, i;
	const char *prop = "qcom,msm-imem-mss-dsm";
	dma_addr_t da;
	size_t size;

	if (!adsp->region_assign_idx || adsp->region_assign_shared)
		return;

	np = of_find_compatible_node(NULL, NULL, prop);
	if (!np) {
		pr_err("%s entry missing!\n", prop);
		return;
	}

	ret = of_address_to_resource(np, 0, &imem);
	of_node_put(np);
	if (ret < 0) {
		pr_err("address to resource conversion failed for %s\n", prop);
		return;
	}

	base = ioremap(imem.start, resource_size(&imem));
	if (!base) {
		pr_err("failed to map MSS DSM region\n");
		return;
	}

	/*
	 * There can be multiple DSM partitions based on the Modem flavor.
	 * Each DSM partition start address and size are written to IMEM by Modem and each
	 * partition consumes 4 bytes (2 bytes for address and 2 bytes for size) of IMEM.
	 *
	 * Modem physical address range has to be in the low 4G (32 bits only) and low 2
	 * bytes will be zeros, so, left shift by 16 to get proper address & size.
	 */
	for (i = 0; i < resource_size(&imem); i = i + 4) {
		da = (u32)(__raw_readw(base + i) << 16);
		size = (u32)(__raw_readw(base + (i + 2)) << 16);
		if (da && size)
			rproc_coredump_add_custom_segment(rproc, da, size, adsp_segment_dump, NULL);
	}

	iounmap(base);
}

static int adsp_assign_memory_region(struct qcom_adsp *adsp)
{
	struct qcom_scm_vmperm perm[2];
	unsigned int perm_size = 1;
	struct device_node *node;
	struct resource r;
	int offset, ret;

	if (!adsp->region_assign_idx)
		return 0;

	for (offset = 0; offset < adsp->region_assign_count; ++offset) {
		node = of_parse_phandle(adsp->dev->of_node, "memory-region",
					adsp->region_assign_idx + offset);
		if (!node) {
			dev_err(adsp->dev, "missing shareable memory-region %d\n", offset);
			return -EINVAL;
		}

		ret = of_address_to_resource(node, 0, &r);
		if (ret)
			return ret;


		if (adsp->region_assign_shared)  {
			perm[0].vmid = QCOM_SCM_VMID_HLOS;
			perm[0].perm = QCOM_SCM_PERM_RW;
			perm[1].vmid = adsp->region_assign_vmid;
			perm[1].perm = QCOM_SCM_PERM_RW;
			perm_size = 2;
		} else {
			perm[0].vmid = adsp->region_assign_vmid;
			perm[0].perm = QCOM_SCM_PERM_RW;
			perm_size = 1;
		}

		adsp->region_assign_phys[offset] = r.start;
		adsp->region_assign_size[offset] = resource_size(&r);
		adsp->region_assign_perms[offset] = BIT(QCOM_SCM_VMID_HLOS);

		ret = qcom_scm_assign_mem(adsp->region_assign_phys[offset],
					  adsp->region_assign_size[offset],
					  &adsp->region_assign_perms[offset],
					  perm, perm_size);
		if (ret < 0) {
			dev_err(adsp->dev, "assign memory %d failed\n", offset);
			return ret;
		}
	}

	return 0;
}

static void adsp_unassign_memory_region(struct qcom_adsp *adsp)
{
	struct qcom_scm_vmperm perm;
	int offset, ret;

	if (!adsp->region_assign_idx || adsp->region_assign_shared)
		return;

	for (offset = 0; offset < adsp->region_assign_count; ++offset) {
		perm.vmid = QCOM_SCM_VMID_HLOS;
		perm.perm = QCOM_SCM_PERM_RW;

		ret = qcom_scm_assign_mem(adsp->region_assign_phys[offset],
					  adsp->region_assign_size[offset],
					  &adsp->region_assign_perms[offset],
					  &perm, 1);
		if (ret < 0)
			dev_err(adsp->dev, "unassign memory failed\n");
	}
}

static int adsp_start(struct rproc *rproc)
{
	struct qcom_adsp *adsp = rproc->priv;
	struct device *dev = NULL;
	bool auth_reset_ret = false;
	int ret, err;

	trace_rproc_qcom_event(dev_name(adsp->dev), "adsp_start", "enter");

	if (adsp->check_status)
		refcount_set(&adsp->current_users, 0);

	if (adsp->dma_phys_below_32b)
		dev = adsp->dev;

	ret = qcom_q6v5_prepare(&adsp->q6v5);
	if (ret)
		goto exit_start;

	if (!adsp->region_assign_shared ||
			(adsp->region_assign_shared && !adsp->region_assigned)) {
		ret = adsp_assign_memory_region(adsp);
		if (ret)
			goto disable_irqs;
	}
	adsp->region_assigned = true;

	ret = adsp_pds_enable(adsp, adsp->proxy_pds, adsp->proxy_pd_count);
	if (ret < 0)
		goto unassign_memory;

	ret = clk_prepare_enable(adsp->xo);
	if (ret)
		goto disable_proxy_pds;

	ret = clk_prepare_enable(adsp->aggre2_clk);
	if (ret)
		goto disable_xo_clk;

	if (adsp->cx_supply) {
		ret = regulator_enable(adsp->cx_supply);
		if (ret)
			goto disable_aggre2_clk;
	}

	if (adsp->px_supply) {
		ret = regulator_enable(adsp->px_supply);
		if (ret)
			goto disable_cx_supply;
	}

	ret = enable_regulators(adsp);
	if (ret)
		goto disable_px_supply;

	qcom_q6v5_pas_set_bw(&adsp->q6v5, UINT_MAX, UINT_MAX);

	trace_rproc_qcom_event(dev_name(adsp->dev), "dtb_auth_reset", "enter");

	if (adsp->dtb_pas_id) {
		ret = qcom_scm_pas_auth_and_reset(adsp->dtb_pas_id);
		if (ret)
			panic("Panicking, auth and reset failed for remoteproc %s dtb ret=%d\n",
				rproc->name, ret);

		auth_reset_ret = true;
	}

	trace_rproc_qcom_event(dev_name(adsp->dev), "Q6_firmware_loading", "enter");

	ret = qcom_mdt_pas_init(adsp->dev, adsp->firmware, rproc->firmware, adsp->pas_id,
				adsp->mem_phys, &adsp->pas_metadata, adsp->dma_phys_below_32b);
	if (ret)
		goto disable_regulator;

	ret = qcom_mdt_load_no_init(adsp->dev, adsp->firmware, rproc->firmware, adsp->pas_id,
				    adsp->mem_region, adsp->mem_phys, adsp->mem_size,
				    &adsp->mem_reloc);
	if (ret)
		goto unlock_pas_metadata;

	qcom_pil_info_store(adsp->info_name, adsp->mem_phys, adsp->mem_size);
	adsp_add_coredump_segments(adsp, adsp->firmware);
	trace_rproc_qcom_event(dev_name(adsp->dev), "Q6_auth_reset", "enter");

	ret = qcom_scm_pas_auth_and_reset(adsp->pas_id);

	trace_rproc_qcom_event(dev_name(adsp->dev), "Q6_auth_reset", "exit");
	if (ret)
		panic("Panicking, auth and reset failed for remoteproc %s ret=%d\n",
				rproc->name, ret);
	trace_rproc_qcom_event(dev_name(adsp->dev), "Q6_auth_reset", "exit");

	if (!qcom_pil_timeouts_disabled()) {
		ret = qcom_q6v5_wait_for_start(&adsp->q6v5, msecs_to_jiffies(5000));
		if (rproc->recovery_disabled && ret)
			panic("Panicking, remoteproc %s failed to bootup.\n", adsp->rproc->name);
		else if (ret == -ETIMEDOUT) {
			dev_err(adsp->dev, "start timed out\n");
			goto release_pas_metadata;
		}
	}

	qcom_scm_pas_metadata_release(&adsp->pas_metadata, dev);

	adsp->q6v5.seq++;
	goto exit_start;

unlock_pas_metadata:
	/*
	 * Unlocking of pas metadata only required either the call to
	 * auth and reset is not reached after qcom_mdt_pas_init() success.
	 * In case auth and reset is success, no need to call shutdown or
	 * unlock pas metadata.
	 */
	if (adsp->dma_phys_below_32b) {
		err = qcom_scm_pas_shutdown(adsp->pas_id);
		if (err && adsp->decrypt_shutdown)
			err = adsp_shutdown_poll_decrypt(adsp);
		if (err)
			panic("Panicking, remoteproc %s failed to unlock pas_metadata.\n",
			      rproc->name);
	}
release_pas_metadata:
	qcom_scm_pas_metadata_release(&adsp->pas_metadata, dev);
disable_regulator:
	disable_regulators(adsp);
disable_px_supply:
	if (adsp->px_supply)
		regulator_disable(adsp->px_supply);
disable_cx_supply:
	if (adsp->cx_supply)
		regulator_disable(adsp->cx_supply);
disable_aggre2_clk:
	clk_disable_unprepare(adsp->aggre2_clk);
disable_xo_clk:
	clk_disable_unprepare(adsp->xo);
disable_proxy_pds:
	adsp_pds_disable(adsp, adsp->proxy_pds, adsp->proxy_pd_count);
unassign_memory:
	adsp->region_assigned = false;
	adsp_unassign_memory_region(adsp);
disable_irqs:
	qcom_q6v5_unprepare(&adsp->q6v5);
exit_start:
	if (adsp->dtb_pas_id && adsp->dma_phys_below_32b && !auth_reset_ret)
		qcom_scm_pas_shutdown(adsp->dtb_pas_id);

	if (adsp->dtb_pas_id)
		qcom_scm_pas_metadata_release(&adsp->dtb_pas_metadata, dev);

	release_firmware(adsp->dtb_firmware);
	qcom_q6v5_pas_set_bw(&adsp->q6v5, 0, 0);
	/* Remove pointer to the loaded firmware, only valid in adsp_load() & adsp_start() */
	adsp->firmware = NULL;
	trace_rproc_qcom_event(dev_name(adsp->dev), "adsp_start", "exit");
	return ret;
}

/*
 * rproc_config_check: Check back the config register
 *
 * Polled read on a register till with a 5ms timeout and 100-200Us interval.
 * Returns immediately if the expected value is read back from the addr.
 *
 * state: new state of the rproc
 * addr: Address to poll on
 *
 * return: 0 if the expected value is read back from the address
 * -ETIMEDOUT is the value was not read in 5ms
 */
static int rproc_config_check(struct qcom_adsp *adsp, u32 state, void *addr)
{
	unsigned int retry_num = 50;
	u32 val;

	do {
		usleep_range(SOCCP_SLEEP_US, SOCCP_SLEEP_US + 100);
		val = readl(addr);
	} while (!(val & state) && --retry_num);

	return (val & state) ? 0 : -ETIMEDOUT;
}

static int rproc_config_check_atomic(struct qcom_adsp *adsp, u32 state, void *addr)
{
	u32 val;

	return readx_poll_timeout_atomic(readl, addr, val,
				val == state, SOCCP_SLEEP_US, SOCCP_TIMEOUT_US);
}
/*
 * rproc_find_status_register: Find the power control regs and INT's
 *
 * Call this function to calculated the tcsr config register, which
 * is the register to be chacked to read the current state of the rproc.
 *
 * return: 0 for success
 *
 */
static int rproc_find_status_register(struct qcom_adsp *adsp)
{
	struct device_node *tcsr;
	struct device_node *np = adsp->dev->of_node;
	struct resource res;
	u32 offset, addr;
	int ret;
	void *tcsr_base;

	tcsr = of_parse_phandle(np, "soccp-tcsr", 0);
	if (!tcsr) {
		dev_err(adsp->dev, "Unable to find the soccp config register\n");
		return -EINVAL;
	}

	ret = of_address_to_resource(tcsr, 0, &res);
	of_node_put(tcsr);
	if (ret) {
		dev_err(adsp->dev, "Unable to find the tcsr base addr\n");
		return ret;
	}

	tcsr_base = ioremap_wc(res.start, resource_size(&res));
	if (!tcsr_base) {
		dev_err(adsp->dev, "Unable to find the tcsr base addr\n");
		return -ENOMEM;
	}

	ret = of_property_read_u32_index(np, "soccp-tcsr", 1, &offset);
	if (ret < 0) {
		dev_err(adsp->dev, "Unable to find the tcsr config offset addr\n");
		iounmap(tcsr_base);
		return ret;
	}
	adsp->tcsr_addr = tcsr_base + offset;

	ret = of_property_read_u32(np, "soccp-spare", &addr);
	if (!addr) {
		dev_err(adsp->dev, "Unable to find the running config register\n");
		return -EINVAL;
	}

	adsp->spare_reg_addr = ioremap_wc(addr, 4);
	if (!adsp->spare_reg_addr) {
		dev_err(adsp->dev, "Unable to find the tcsr base addr\n");
		return -ENOMEM;
	}

	return 0;
}
static bool rproc_poll_handover(struct qcom_adsp *adsp)
{
	unsigned int retry_num = 50;

	do {
		msleep(RPROC_HANDOVER_POLL_DELAY_MS);
	} while (!adsp->q6v5.handover_issued && --retry_num);

	return adsp->q6v5.handover_issued;
}

static int bus_bw_init(struct qcom_adsp *adsp)
{
	int ret = 0;
	bool rproc_ddr_set_icc_low_svs;
	unsigned int rproc_ddr_lowsvs_icc_bw;

	rproc_ddr_set_icc_low_svs = of_property_read_bool(adsp->dev->of_node,
				     "rproc-ddr-set-icc-low-svs");

	if (rproc_ddr_set_icc_low_svs) {
		adsp->rproc_ddr_set_icc_low_svs = rproc_ddr_set_icc_low_svs;
		ret = of_property_read_u32(adsp->dev->of_node,
				"rproc-ddr-lowsvs-icc-bw", &rproc_ddr_lowsvs_icc_bw);
		if (!rproc_ddr_lowsvs_icc_bw) {
			dev_err(adsp->dev, "Low SVS ICC Bw is not available.\n");
			return ret;
		}
		adsp->rproc_ddr_lowsvs_icc_bw = rproc_ddr_lowsvs_icc_bw;
		dev_info(adsp->dev, "LowSVS Bus BW: %d\n", rproc_ddr_lowsvs_icc_bw);
	} else {
		adsp->rproc_ddr_set_icc_low_svs = false;
		dev_info(adsp->dev, "Low SVS vote for ICC is not set.\n");
	}

	return ret;
}

static int set_icc_bw(struct qcom_adsp *adsp, bool enable)
{
	int ret;
	u32 avg_bw, peak_bw;

	avg_bw = enable
			? adsp->rproc_ddr_set_icc_low_svs
				? adsp->rproc_ddr_lowsvs_icc_bw
				: UINT_MAX
			: 0;
	peak_bw = enable
			? adsp->rproc_ddr_set_icc_low_svs
				? adsp->rproc_ddr_lowsvs_icc_bw
				: UINT_MAX
			: 0;

	ret = icc_set_bw(adsp->q6v5.path, avg_bw, peak_bw);

	return ret;
}

/*
 * rproc_set_state: Request the SOCCP to change state
 *
 * Function to request the SOCCP to move to Running/Dormant.
 * Blocking API, where the MAX timeout is 5 seconds.
 *
 * state: 1 to set state to RUNNING (D3 to D0)
 *        0 to set state to SUSPEND (D0 to D3)
 *
 * return: 0 if status is set, else -ETIMEOUT
 */
int rproc_set_state(struct rproc *rproc, bool state)
{
	int ret = 0;
	int users;
	struct qcom_adsp *adsp;

	if (!rproc || !rproc->priv) {
		pr_err("no rproc or adsp\n");
		return -EINVAL;
	}

	adsp = (struct qcom_adsp *)rproc->priv;
	if (!adsp->q6v5.running) {
		dev_err(adsp->dev, "rproc is not running\n");
		return -EINVAL;
	} else if (!adsp->q6v5.handover_issued) {
		dev_err(adsp->dev, "rproc is running but handover is not received\n");
		if (!rproc_poll_handover(adsp)) {
			dev_err(adsp->dev, "retry for handover timedout\n");
			return -EINVAL;
		}
	}

	mutex_lock(&adsp->adsp_lock);
	users = refcount_read(&adsp->current_users);
	if (state) {
		if (users >= 1) {
			refcount_inc(&adsp->current_users);
			ret = 0;
			goto soccp_out;
		}

		ret = enable_regulators(adsp);
		if (ret) {
			dev_err(adsp->dev, "failed to enable regulators\n");
			goto soccp_out;
		}

		ret = set_icc_bw(adsp, true);
		if (ret < 0) {
			dev_err(adsp->q6v5.dev, "failed to set bandwidth request\n");
			goto soccp_out;
		}

		ret = clk_prepare_enable(adsp->xo);
		if (ret) {
			dev_err(adsp->dev, "failed to enable clks\n");
			goto soccp_out;
		}

		ret = qcom_smem_state_update_bits(adsp->wake_state,
					    SOCCP_STATE_MASK,
					    BIT(adsp->wake_bit));
		if (ret) {
			dev_err(adsp->dev, "failed to update smem bits for D3 to D0\n");
			goto soccp_out;
		}

		ret = rproc_config_check(adsp, SOCCP_D0 | SOCCP_D1, adsp->tcsr_addr);
		if (ret) {
			dev_err(adsp->dev, "%s requested D3->D0: soccp failed to update tcsr val=%d\n",
				current->comm, readl(adsp->tcsr_addr));
			goto soccp_out;
		}

		ret = rproc_config_check(adsp, SPARE_REG_SOCCP_D0, adsp->spare_reg_addr);
		if (ret) {
			ret = qcom_smem_state_update_bits(adsp->wake_state,
						    SOCCP_STATE_MASK,
						    !BIT(adsp->wake_bit));
			if (ret)
				dev_err(adsp->dev, "failed to clear smem bits after a failed D0 request\n");

			dev_err(adsp->dev, "%s requested D3->D0: soccp failed to update spare reg val=%d\n",
				current->comm, readl(adsp->spare_reg_addr));

			goto soccp_out;
		}

		refcount_set(&adsp->current_users, 1);
	} else {
		if (users > 1) {
			refcount_dec(&adsp->current_users);
			ret = 0;
			goto soccp_out;
		} else if (users == 1) {
			ret = qcom_smem_state_update_bits(adsp->sleep_state,
					    SOCCP_STATE_MASK,
					    BIT(adsp->sleep_bit));
			if (ret) {
				dev_err(adsp->dev, "failed to update smem bits for D0 to D3\n");
				goto soccp_out;
			}

			ret = rproc_config_check(adsp, SOCCP_D3, adsp->tcsr_addr);
			if (ret) {
				ret = qcom_smem_state_update_bits(adsp->sleep_state,
						    SOCCP_STATE_MASK,
						    !BIT(adsp->sleep_bit));
				if (ret)
					dev_err(adsp->dev, "failed to clear smem bits after a failed D3 request\n");

				dev_err(adsp->dev, "%s requested D0->D3 failed: TCSR value:%d\n",
					current->comm, readl(adsp->tcsr_addr));
				goto soccp_out;
			}

			ret = set_icc_bw(adsp, false);
			if (ret < 0) {
				dev_err(adsp->q6v5.dev, "failed to set bandwidth request\n");
				goto soccp_out;
			}

			disable_regulators(adsp);
			clk_disable_unprepare(adsp->xo);

			refcount_set(&adsp->current_users, 0);
		}
	}

soccp_out:
	if (ret && (adsp->rproc->state != RPROC_RUNNING)) {
		dev_err(adsp->dev, "SOCCP has crashed while processing a D transition req by %s\n",
			current->comm);
		ret = -EBUSY;
	}

	mutex_unlock(&adsp->adsp_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(rproc_set_state);

static int rproc_panic_handler(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	struct qcom_adsp *adsp = container_of(this, struct qcom_adsp, panic_blk);
	int ret;

	if (!adsp)
		return NOTIFY_DONE;
	/* wake up SOCCP during panic to run error handlers on SOCCP */
	dev_info(adsp->dev, "waking SOCCP from panic path\n");
	ret = qcom_smem_state_update_bits(adsp->wake_state,
				    SOCCP_STATE_MASK,
				    BIT(adsp->wake_bit));
	if (ret) {
		dev_err(adsp->dev, "failed to update smem bits for D3 to D0\n");
		goto done;
	}
	ret = rproc_config_check_atomic(adsp, SOCCP_D0, adsp->tcsr_addr);
	if (ret)
		dev_err(adsp->dev, "failed to change to D0\n");

	ret = rproc_config_check_atomic(adsp, SPARE_REG_SOCCP_D0, adsp->spare_reg_addr);
	if (ret)
		dev_err(adsp->dev, "failed to change to D0\n");
done:
	return NOTIFY_DONE;
}

static void qcom_pas_handover(struct qcom_q6v5 *q6v5)
{
	struct qcom_adsp *adsp = container_of(q6v5, struct qcom_adsp, q6v5);
	int ret;

	if (adsp->check_status) {
		ret = rproc_config_check(adsp, SOCCP_D3, adsp->tcsr_addr);
		if (ret)
			dev_err(adsp->dev, "state not changed in handover TCSR val = %d\n",
				readl(adsp->tcsr_addr));
		else
			dev_info(adsp->dev, "state changed in handover for soccp! TCSR val = %d\n",
					readl(adsp->tcsr_addr));
	}
	if (adsp->px_supply)
		regulator_disable(adsp->px_supply);
	if (adsp->cx_supply)
		regulator_disable(adsp->cx_supply);
	disable_regulators(adsp);
	clk_disable_unprepare(adsp->aggre2_clk);
	clk_disable_unprepare(adsp->xo);
	adsp_pds_disable(adsp, adsp->proxy_pds, adsp->proxy_pd_count);
}

static int adsp_stop(struct rproc *rproc)
{
	struct qcom_adsp *adsp = rproc->priv;
	int handover;
	int ret = 0;

	trace_rproc_qcom_event(dev_name(adsp->dev), "adsp_stop", "enter");

	ret = qcom_q6v5_request_stop(&adsp->q6v5, adsp->sysmon);
	if (ret == -ETIMEDOUT)
		dev_err(adsp->dev, "timed out on wait\n");

	qcom_q6v5_pas_set_bw(&adsp->q6v5, UINT_MAX, UINT_MAX);
	ret = qcom_scm_pas_shutdown(adsp->pas_id);
	if (ret && adsp->decrypt_shutdown)
		ret = adsp_shutdown_poll_decrypt(adsp);

	if (ret)
		panic("Panicking, remoteproc %s failed to shutdown.\n", rproc->name);

	if (adsp->dtb_pas_id) {
		ret = qcom_scm_pas_shutdown(adsp->dtb_pas_id);
		if (ret)
			panic("Panicking, remoteproc %s dtb failed to shutdown.\n", rproc->name);
	}

	qcom_q6v5_pas_set_bw(&adsp->q6v5, 0, 0);
	handover = qcom_q6v5_unprepare(&adsp->q6v5);
	if (handover)
		qcom_pas_handover(&adsp->q6v5);

	add_mpss_dsm_mem_ssr_dump(adsp);

	adsp_unassign_memory_region(adsp);

	adsp->q6v5.seq++;
	trace_rproc_qcom_event(dev_name(adsp->dev), "adsp_stop", "exit");

	return ret;
}

static void *adsp_da_to_va(struct rproc *rproc, u64 da, size_t len, bool *is_iomem)
{
	struct qcom_adsp *adsp = rproc->priv;
	int offset;

	offset = da - adsp->mem_phys;
	if (offset < 0 || offset + len > adsp->mem_size)
		return NULL;

	if (is_iomem)
		*is_iomem = true;

	return adsp->mem_region + offset;
}

static unsigned long adsp_panic(struct rproc *rproc)
{
	struct qcom_adsp *adsp = rproc->priv;

	return qcom_q6v5_panic(&adsp->q6v5);
}

static const struct rproc_ops adsp_ops = {
	.unprepare = adsp_unprepare,
	.start = adsp_start,
	.stop = adsp_stop,
	.da_to_va = adsp_da_to_va,
	.load = adsp_load,
	.panic = adsp_panic,
};

static const struct rproc_ops adsp_minidump_ops = {
	.unprepare = adsp_unprepare,
	.start = adsp_start,
	.stop = adsp_stop,
	.da_to_va = adsp_da_to_va,
	.parse_fw = qcom_register_dump_segments,
	.load = adsp_load,
	.panic = adsp_panic,
	.coredump = adsp_minidump,
};

static int adsp_init_clock(struct qcom_adsp *adsp)
{
	int ret;

	adsp->xo = devm_clk_get(adsp->dev, "xo");
	if (IS_ERR(adsp->xo)) {
		ret = PTR_ERR(adsp->xo);
		if (ret != -EPROBE_DEFER)
			dev_err(adsp->dev, "failed to get xo clock");
		return ret;
	}

	adsp->aggre2_clk = devm_clk_get_optional(adsp->dev, "aggre2");
	if (IS_ERR(adsp->aggre2_clk)) {
		ret = PTR_ERR(adsp->aggre2_clk);
		if (ret != -EPROBE_DEFER)
			dev_err(adsp->dev,
				"failed to get aggre2 clock");
		return ret;
	}

	return 0;
}

static int adsp_init_regulator(struct qcom_adsp *adsp)
{
	int len;
	int i, rc;
	char uv_ua[50];
	u32 uv_ua_vals[2];
	const char *reg_name;

	adsp->reg_cnt = of_property_count_strings(adsp->dev->of_node,
						  "reg-names");
	if (adsp->reg_cnt <= 0) {
		dev_err(adsp->dev, "No regulators added!\n");
		return 0;
	}

	adsp->regs = devm_kzalloc(adsp->dev,
				  sizeof(struct reg_info) * adsp->reg_cnt,
				  GFP_KERNEL);
	if (!adsp->regs)
		return -ENOMEM;

	for (i = 0; i < adsp->reg_cnt; i++) {
		of_property_read_string_index(adsp->dev->of_node, "reg-names",
					      i, &reg_name);

		adsp->regs[i].reg = devm_regulator_get(adsp->dev, reg_name);
		if (IS_ERR(adsp->regs[i].reg)) {
			dev_err(adsp->dev, "failed to get %s reg\n", reg_name);
			return PTR_ERR(adsp->regs[i].reg);
		}

		/* Read current(uA) and voltage(uV) value */
		snprintf(uv_ua, sizeof(uv_ua), "%s-uV-uA", reg_name);
		if (!of_find_property(adsp->dev->of_node, uv_ua, &len))
			continue;

		rc = of_property_read_u32_array(adsp->dev->of_node, uv_ua,
						uv_ua_vals,
						ARRAY_SIZE(uv_ua_vals));
		if (rc) {
			dev_err(adsp->dev, "Failed to read uVuA value(rc:%d)\n",
				rc);
			return rc;
		}

		if (uv_ua_vals[0] > 0)
			adsp->regs[i].uV = uv_ua_vals[0];
		if (uv_ua_vals[1] > 0)
			adsp->regs[i].uA = uv_ua_vals[1];
	}
	return 0;
}

static int adsp_pds_attach(struct device *dev, struct device **devs,
			   char **pd_names)
{
	size_t num_pds = 0;
	int ret;
	int i;

	if (!pd_names)
		return 0;

	/* Handle single power domain */
	if (dev->pm_domain) {
		devs[0] = dev;
		pm_runtime_enable(dev);
		return 1;
	}

	while (pd_names[num_pds])
		num_pds++;

	for (i = 0; i < num_pds; i++) {
		devs[i] = dev_pm_domain_attach_by_name(dev, pd_names[i]);
		if (IS_ERR_OR_NULL(devs[i])) {
			ret = PTR_ERR(devs[i]) ? : -ENODATA;
			goto unroll_attach;
		}
	}

	return num_pds;

unroll_attach:
	for (i--; i >= 0; i--)
		dev_pm_domain_detach(devs[i], false);

	return ret;
};

static void adsp_pds_detach(struct qcom_adsp *adsp, struct device **pds,
			    size_t pd_count)
{
	struct device *dev = adsp->dev;
	int i;

	/* Handle single power domain */
	if (dev->pm_domain && pd_count) {
		pm_runtime_disable(dev);
		return;
	}

	for (i = 0; i < pd_count; i++)
		dev_pm_domain_detach(pds[i], false);
}

static int adsp_alloc_memory_region(struct qcom_adsp *adsp)
{
	struct reserved_mem *rmem;
	struct device_node *node;

	node = of_parse_phandle(adsp->dev->of_node, "memory-region", 0);
	if (!node) {
		dev_err(adsp->dev, "no memory-region specified\n");
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(node);
	of_node_put(node);
	if (!rmem) {
		dev_err(adsp->dev, "unable to resolve memory-region\n");
		return -EINVAL;
	}

	adsp->mem_phys = adsp->mem_reloc = rmem->base;
	adsp->mem_size = rmem->size;
	adsp->mem_region = devm_ioremap_wc(adsp->dev, adsp->mem_phys, adsp->mem_size);
	if (!adsp->mem_region) {
		dev_err(adsp->dev, "unable to map memory region: %pa+%zx\n",
			&rmem->base, adsp->mem_size);
		return -EBUSY;
	}

	if (!adsp->dtb_pas_id)
		return 0;

	node = of_parse_phandle(adsp->dev->of_node, "memory-region", 1);
	if (!node) {
		dev_err(adsp->dev, "no dtb memory-region specified\n");
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(node);
	of_node_put(node);
	if (!rmem) {
		dev_err(adsp->dev, "unable to resolve dtb memory-region\n");
		return -EINVAL;
	}

	adsp->dtb_mem_phys = adsp->dtb_mem_reloc = rmem->base;
	adsp->dtb_mem_size = rmem->size;
	adsp->dtb_mem_region = devm_ioremap_wc(adsp->dev, adsp->dtb_mem_phys, adsp->dtb_mem_size);
	if (!adsp->dtb_mem_region) {
		dev_err(adsp->dev, "unable to map dtb memory region: %pa+%zx\n",
			&rmem->base, adsp->dtb_mem_size);
		return -EBUSY;
	}

	return 0;
}

static int adsp_setup_32b_dma_allocs(struct qcom_adsp *adsp)
{
	int ret;

	if (!adsp->dma_phys_below_32b)
		return 0;

	ret = of_reserved_mem_device_init_by_idx(adsp->dev, adsp->dev->of_node,
			adsp->dtb_firmware_name ? 2 : 1);
	if (ret) {
		dev_err(adsp->dev,
			"Unable to get the CMA area for performing dma_alloc_* calls\n");
		goto out;
	}

	ret = dma_set_mask_and_coherent(adsp->dev, DMA_BIT_MASK(32));
	if (ret)
		dev_err(adsp->dev, "Unable to set the coherent mask to 32-bits!\n");

out:
	return ret;
}

static void rproc_recovery_set(struct rproc *rproc)
{
	struct qcom_adsp *adsp = (struct qcom_adsp *)rproc->priv;

	adsp->subsys_recovery_disabled = rproc->recovery_disabled;
}

void qcom_rproc_update_recovery_status(struct rproc *rproc, bool enable, bool locked)
{
	struct qcom_adsp *adsp;

	if (!rproc)
		return;

	adsp = (struct qcom_adsp *)rproc->priv;
	if (locked)
		mutex_lock(&rproc->lock);
	if (enable) {
		/* Save recovery flag */
		adsp->subsys_recovery_disabled = rproc->recovery_disabled;
		rproc->recovery_disabled = !enable;
		pr_info("qcom rproc: %s: recovery enabled by kernel client\n", rproc->name);
	} else {
		/* Restore recovery flag */
		rproc->recovery_disabled = adsp->subsys_recovery_disabled;
		pr_info("qcom rproc: %s: recovery disabled by kernel client\n", rproc->name);
	}

	if (locked)
		mutex_unlock(&rproc->lock);
}
EXPORT_SYMBOL_GPL(qcom_rproc_update_recovery_status);

static int adsp_probe(struct platform_device *pdev)
{
	const struct adsp_data *desc;
	struct qcom_adsp *adsp;
	struct rproc *rproc;
	const char *fw_name, *dtb_fw_name = NULL;
	const struct rproc_ops *ops = &adsp_ops;
	char md_dev_name[32];
	int ret;

	desc = of_device_get_match_data(&pdev->dev);
	if (!desc)
		return -EINVAL;

	if (!qcom_scm_is_available())
		return -EPROBE_DEFER;

	fw_name = desc->firmware_name;
	ret = of_property_read_string(pdev->dev.of_node, "firmware-name",
				      &fw_name);
	if (ret < 0 && ret != -EINVAL)
		return ret;

	if (desc->dtb_firmware_name) {
		dtb_fw_name = desc->dtb_firmware_name;
		ret = of_property_read_string_index(pdev->dev.of_node, "firmware-name", 1,
						    &dtb_fw_name);
		if (ret < 0 && ret != -EINVAL)
			return ret;
	}

	if (desc->minidump_id)
		ops = &adsp_minidump_ops;

	rproc = rproc_alloc(&pdev->dev, pdev->name, ops, fw_name, sizeof(*adsp));

	if (!rproc) {
		dev_err(&pdev->dev, "unable to allocate remoteproc\n");
		return -ENOMEM;
	}

	rproc->recovery_disabled = true;
	rproc->auto_boot = desc->auto_boot;
	if (desc->uses_elf64)
		rproc_coredump_set_elf_info(rproc, ELFCLASS64, EM_NONE);
	else
		rproc_coredump_set_elf_info(rproc, ELFCLASS32, EM_NONE);

	adsp = rproc->priv;
	adsp->dev = &pdev->dev;
	adsp->rproc = rproc;
	adsp->minidump_id = desc->minidump_id;
	adsp->pas_id = desc->pas_id;
	adsp->info_name = desc->sysmon_name;
	adsp->decrypt_shutdown = desc->decrypt_shutdown;
	adsp->both_dumps = desc->both_dumps;
	adsp->region_assign_idx = desc->region_assign_idx;
	adsp->region_assign_count = min_t(int, MAX_ASSIGN_COUNT, desc->region_assign_count);
	adsp->region_assign_vmid = desc->region_assign_vmid;
	adsp->region_assign_shared = desc->region_assign_shared;
	adsp->dma_phys_below_32b = desc->dma_phys_below_32b;
	adsp->check_status = desc->check_status;
	adsp->subsys_recovery_disabled = true;

	if (dtb_fw_name) {
		adsp->dtb_firmware_name = dtb_fw_name;
		adsp->dtb_pas_id = desc->dtb_pas_id;
	}
	platform_set_drvdata(pdev, adsp);

	ret = device_init_wakeup(adsp->dev, true);
	if (ret)
		goto free_rproc;

	ret = adsp_alloc_memory_region(adsp);
	if (ret)
		goto free_rproc;

	ret = adsp_setup_32b_dma_allocs(adsp);
	if (ret)
		goto free_rproc;

	ret = adsp_init_clock(adsp);
	if (ret)
		goto free_rproc;

	ret = adsp_init_regulator(adsp);
	if (ret)
		goto free_rproc;

	ret = bus_bw_init(adsp);
	if (ret)
		goto free_rproc;

	ret = adsp_pds_attach(&pdev->dev, adsp->proxy_pds,
			      desc->proxy_pd_names);
	if (ret < 0)
		goto free_rproc;
	adsp->proxy_pd_count = ret;

	ret = qcom_q6v5_init(&adsp->q6v5, pdev, rproc, desc->crash_reason_smem,
			     desc->crash_reason_stack, desc->smem_host_id,
			     desc->load_state, qcom_pas_handover);
	if (ret)
		goto detach_proxy_pds;

	if (adsp->check_status) {
		if (rproc_find_status_register(adsp))
			goto detach_proxy_pds;
		adsp->wake_state = devm_qcom_smem_state_get(&pdev->dev, "wakeup", &adsp->wake_bit);

		if (IS_ERR(adsp->wake_state)) {
			dev_err(&pdev->dev, "failed to acquire wake state\n");
			goto detach_proxy_pds;
		}

		adsp->sleep_state = devm_qcom_smem_state_get(&pdev->dev, "sleep", &adsp->sleep_bit);

		if (IS_ERR(adsp->sleep_state)) {
			dev_err(&pdev->dev, "failed to acquire sleep state\n");
			goto detach_proxy_pds;
		}

		mutex_init(&adsp->adsp_lock);

		refcount_set(&adsp->current_users, 0);
	}

	qcom_q6v5_register_ssr_subdev(&adsp->q6v5, &adsp->ssr_subdev.subdev);

	qcom_add_glink_subdev(rproc, &adsp->glink_subdev, desc->ssr_name);
	qcom_add_smd_subdev(rproc, &adsp->smd_subdev);
	adsp->sysmon = qcom_add_sysmon_subdev(rproc,
					      desc->sysmon_name,
					      desc->ssctl_id);
	if (IS_ERR(adsp->sysmon)) {
		ret = PTR_ERR(adsp->sysmon);
		goto detach_proxy_pds;
	}

	ret = device_create_file(adsp->dev, &dev_attr_txn_id);
	if (ret)
		goto remove_subdevs;

	snprintf(md_dev_name, ARRAY_SIZE(md_dev_name), "%s-md", pdev->dev.of_node->name);
	adsp->minidump_dev = qcom_create_ramdump_device(md_dev_name, NULL);
	if (!adsp->minidump_dev)
		dev_err(&pdev->dev, "Unable to create %s minidump device.\n", md_dev_name);

	qcom_add_ssr_subdev(rproc, &adsp->ssr_subdev, desc->ssr_name);
	ret = rproc_add(rproc);
	if (ret)
		goto destroy_minidump_dev;

	/*
	 * Concurrent stores can happen on the same global variable with
	 * different subsystem probe, however, as it is happening with
	 * same value at max, compiler can do is to optimize it away with
	 * single store compared to multiple store on worst case, so be it.
	 */
	rproc_recovery_set_fn = rproc_recovery_set;

	if (adsp->check_status) {
		adsp->panic_blk.priority = INT_MAX - 2;
		adsp->panic_blk.notifier_call = rproc_panic_handler;
		atomic_notifier_chain_register(&panic_notifier_list, &adsp->panic_blk);
	}

	return 0;

destroy_minidump_dev:
	if (adsp->minidump_dev)
		qcom_destroy_ramdump_device(adsp->minidump_dev);

	device_remove_file(adsp->dev, &dev_attr_txn_id);
remove_subdevs:
	qcom_remove_sysmon_subdev(adsp->sysmon);
detach_proxy_pds:
	adsp_pds_detach(adsp, adsp->proxy_pds, adsp->proxy_pd_count);
free_rproc:
	device_init_wakeup(adsp->dev, false);
	rproc_free(rproc);

	return ret;
}

static void adsp_remove(struct platform_device *pdev)
{
	struct qcom_adsp *adsp = platform_get_drvdata(pdev);

	rproc_del(adsp->rproc);
	qcom_q6v5_deinit(&adsp->q6v5);
	if (adsp->minidump_dev)
		qcom_destroy_ramdump_device(adsp->minidump_dev);
	device_remove_file(adsp->dev, &dev_attr_txn_id);
	adsp_unassign_memory_region(adsp);
	qcom_remove_glink_subdev(adsp->rproc, &adsp->glink_subdev);
	qcom_remove_sysmon_subdev(adsp->sysmon);
	qcom_remove_smd_subdev(adsp->rproc, &adsp->smd_subdev);
	qcom_remove_ssr_subdev(adsp->rproc, &adsp->ssr_subdev);
	if (adsp->check_status)
		atomic_notifier_chain_unregister(&panic_notifier_list, &adsp->panic_blk);
	adsp_pds_detach(adsp, adsp->proxy_pds, adsp->proxy_pd_count);
	device_init_wakeup(adsp->dev, false);
	rproc_free(adsp->rproc);
}

static const struct adsp_data adsp_resource_init = {
		.crash_reason_smem = 423,
		.firmware_name = "adsp.mdt",
		.pas_id = 1,
		.auto_boot = true,
		.ssr_name = "lpass",
		.sysmon_name = "adsp",
		.ssctl_id = 0x14,
};

static const struct adsp_data sdm845_adsp_resource_init = {
		.crash_reason_smem = 423,
		.firmware_name = "adsp.mdt",
		.pas_id = 1,
		.auto_boot = true,
		.load_state = "adsp",
		.ssr_name = "lpass",
		.sysmon_name = "adsp",
		.ssctl_id = 0x14,
};

static const struct adsp_data sdxpinn_mpss_resource = {
	.crash_reason_smem = 421,
	.firmware_name = "modem.mdt",
	.pas_id = 4,
	.dtb_firmware_name = "modem_dtb.mdt",
	.dtb_pas_id = 0x26,
	.minidump_id = 3,
	.decrypt_shutdown = true,
	.uses_elf64 = true,
	.auto_boot = false,
	.load_state = "modem",
	.ssr_name = "mpss",
	.sysmon_name = "modem",
	.ssctl_id = 0x12,
	.dma_phys_below_32b = true,
	.region_assign_idx = 3,
	.region_assign_count = 3,
	.region_assign_vmid = QCOM_SCM_VMID_MSS_MSA,
};

static const struct adsp_data sm6350_adsp_resource = {
	.crash_reason_smem = 423,
	.firmware_name = "adsp.mdt",
	.pas_id = 1,
	.auto_boot = true,
	.proxy_pd_names = (char*[]){
		"lcx",
		"lmx",
		NULL
	},
	.load_state = "adsp",
	.ssr_name = "lpass",
	.sysmon_name = "adsp",
	.ssctl_id = 0x14,
};

static const struct adsp_data sm8150_adsp_resource = {
		.crash_reason_smem = 423,
		.firmware_name = "adsp.mdt",
		.pas_id = 1,
		.auto_boot = true,
		.proxy_pd_names = (char*[]){
			"cx",
			NULL
		},
		.load_state = "adsp",
		.ssr_name = "lpass",
		.sysmon_name = "adsp",
		.ssctl_id = 0x14,
};

static const struct adsp_data sm8250_adsp_resource = {
	.crash_reason_smem = 423,
	.firmware_name = "adsp.mdt",
	.pas_id = 1,
	.auto_boot = true,
	.proxy_pd_names = (char*[]){
		"lcx",
		"lmx",
		NULL
	},
	.load_state = "adsp",
	.ssr_name = "lpass",
	.sysmon_name = "adsp",
	.ssctl_id = 0x14,
};

static const struct adsp_data sm8350_adsp_resource = {
	.crash_reason_smem = 423,
	.firmware_name = "adsp.mdt",
	.pas_id = 1,
	.auto_boot = true,
	.proxy_pd_names = (char*[]){
		"lcx",
		"lmx",
		NULL
	},
	.load_state = "adsp",
	.ssr_name = "lpass",
	.sysmon_name = "adsp",
	.ssctl_id = 0x14,
};

static const struct adsp_data msm8996_adsp_resource = {
		.crash_reason_smem = 423,
		.firmware_name = "adsp.mdt",
		.pas_id = 1,
		.auto_boot = true,
		.proxy_pd_names = (char*[]){
			"cx",
			NULL
		},
		.ssr_name = "lpass",
		.sysmon_name = "adsp",
		.ssctl_id = 0x14,
};

static const struct adsp_data cdsp_resource_init = {
	.crash_reason_smem = 601,
	.firmware_name = "cdsp.mdt",
	.pas_id = 18,
	.auto_boot = true,
	.ssr_name = "cdsp",
	.sysmon_name = "cdsp",
	.ssctl_id = 0x17,
};

static const struct adsp_data sdm845_cdsp_resource_init = {
	.crash_reason_smem = 601,
	.firmware_name = "cdsp.mdt",
	.pas_id = 18,
	.auto_boot = true,
	.load_state = "cdsp",
	.ssr_name = "cdsp",
	.sysmon_name = "cdsp",
	.ssctl_id = 0x17,
};

static const struct adsp_data sm6350_cdsp_resource = {
	.crash_reason_smem = 601,
	.firmware_name = "cdsp.mdt",
	.pas_id = 18,
	.auto_boot = true,
	.proxy_pd_names = (char*[]){
		"cx",
		"mx",
		NULL
	},
	.load_state = "cdsp",
	.ssr_name = "cdsp",
	.sysmon_name = "cdsp",
	.ssctl_id = 0x17,
};

static const struct adsp_data sm8150_cdsp_resource = {
	.crash_reason_smem = 601,
	.firmware_name = "cdsp.mdt",
	.pas_id = 18,
	.auto_boot = true,
	.proxy_pd_names = (char*[]){
		"cx",
		NULL
	},
	.load_state = "cdsp",
	.ssr_name = "cdsp",
	.sysmon_name = "cdsp",
	.ssctl_id = 0x17,
};

static const struct adsp_data sm8250_cdsp_resource = {
	.crash_reason_smem = 601,
	.firmware_name = "cdsp.mdt",
	.pas_id = 18,
	.auto_boot = true,
	.proxy_pd_names = (char*[]){
		"cx",
		NULL
	},
	.load_state = "cdsp",
	.ssr_name = "cdsp",
	.sysmon_name = "cdsp",
	.ssctl_id = 0x17,
};

static const struct adsp_data sc8280xp_nsp0_resource = {
	.crash_reason_smem = 601,
	.firmware_name = "cdsp.mdt",
	.pas_id = 18,
	.auto_boot = true,
	.proxy_pd_names = (char*[]){
		"nsp",
		NULL
	},
	.ssr_name = "cdsp0",
	.sysmon_name = "cdsp",
	.ssctl_id = 0x17,
};

static const struct adsp_data sc8280xp_nsp1_resource = {
	.crash_reason_smem = 633,
	.firmware_name = "cdsp.mdt",
	.pas_id = 30,
	.auto_boot = true,
	.proxy_pd_names = (char*[]){
		"nsp",
		NULL
	},
	.ssr_name = "cdsp1",
	.sysmon_name = "cdsp1",
	.ssctl_id = 0x20,
};

static const struct adsp_data sm8350_cdsp_resource = {
	.crash_reason_smem = 601,
	.firmware_name = "cdsp.mdt",
	.pas_id = 18,
	.auto_boot = true,
	.proxy_pd_names = (char*[]){
		"cx",
		"mxc",
		NULL
	},
	.load_state = "cdsp",
	.ssr_name = "cdsp",
	.sysmon_name = "cdsp",
	.ssctl_id = 0x17,
};

static const struct adsp_data mpss_resource_init = {
	.crash_reason_smem = 421,
	.firmware_name = "modem.mdt",
	.pas_id = 4,
	.minidump_id = 3,
	.auto_boot = false,
	.proxy_pd_names = (char*[]){
		"cx",
		"mss",
		NULL
	},
	.load_state = "modem",
	.ssr_name = "mpss",
	.sysmon_name = "modem",
	.ssctl_id = 0x12,
};

static const struct adsp_data sc8180x_mpss_resource = {
	.crash_reason_smem = 421,
	.firmware_name = "modem.mdt",
	.pas_id = 4,
	.uses_elf64 = true,
	.auto_boot = false,
	.proxy_pd_names = (char*[]){
		"cx",
		NULL
	},
	.load_state = "modem",
	.ssr_name = "mpss",
	.sysmon_name = "modem",
	.ssctl_id = 0x12,
};

static const struct adsp_data msm8996_slpi_resource_init = {
		.crash_reason_smem = 424,
		.firmware_name = "slpi.mdt",
		.pas_id = 12,
		.auto_boot = true,
		.proxy_pd_names = (char*[]){
			"ssc_cx",
			NULL
		},
		.ssr_name = "dsps",
		.sysmon_name = "slpi",
		.ssctl_id = 0x16,
};

static const struct adsp_data sdm845_slpi_resource_init = {
		.crash_reason_smem = 424,
		.firmware_name = "slpi.mdt",
		.pas_id = 12,
		.auto_boot = true,
		.proxy_pd_names = (char*[]){
			"lcx",
			"lmx",
			NULL
		},
		.load_state = "slpi",
		.ssr_name = "dsps",
		.sysmon_name = "slpi",
		.ssctl_id = 0x16,
};

static const struct adsp_data wcss_resource_init = {
	.crash_reason_smem = 421,
	.firmware_name = "wcnss.mdt",
	.pas_id = 6,
	.auto_boot = true,
	.ssr_name = "mpss",
	.sysmon_name = "wcnss",
	.ssctl_id = 0x12,
};

static const struct adsp_data sdx55_mpss_resource = {
	.crash_reason_smem = 421,
	.firmware_name = "modem.mdt",
	.pas_id = 4,
	.auto_boot = true,
	.proxy_pd_names = (char*[]){
		"cx",
		"mss",
		NULL
	},
	.ssr_name = "mpss",
	.sysmon_name = "modem",
	.ssctl_id = 0x22,
};

static const struct adsp_data sm8450_mpss_resource = {
	.crash_reason_smem = 421,
	.firmware_name = "modem.mdt",
	.pas_id = 4,
	.minidump_id = 3,
	.auto_boot = false,
	.decrypt_shutdown = true,
	.proxy_pd_names = (char*[]){
		"cx",
		"mss",
		NULL
	},
	.load_state = "modem",
	.ssr_name = "mpss",
	.sysmon_name = "modem",
	.ssctl_id = 0x12,
};

static const struct adsp_data sm8550_adsp_resource = {
	.crash_reason_smem = 423,
	.firmware_name = "adsp.mdt",
	.dtb_firmware_name = "adsp_dtb.mdt",
	.pas_id = 1,
	.dtb_pas_id = 0x24,
	.minidump_id = 5,
	.auto_boot = true,
	.proxy_pd_names = (char*[]){
		"lcx",
		"lmx",
		NULL
	},
	.load_state = "adsp",
	.ssr_name = "lpass",
	.sysmon_name = "adsp",
	.ssctl_id = 0x14,
};

static const struct adsp_data sm8550_cdsp_resource = {
	.crash_reason_smem = 601,
	.firmware_name = "cdsp.mdt",
	.dtb_firmware_name = "cdsp_dtb.mdt",
	.pas_id = 18,
	.dtb_pas_id = 0x25,
	.minidump_id = 7,
	.auto_boot = true,
	.proxy_pd_names = (char*[]){
		"cx",
		"mxc",
		"nsp",
		NULL
	},
	.load_state = "cdsp",
	.ssr_name = "cdsp",
	.sysmon_name = "cdsp",
	.ssctl_id = 0x17,
};

static const struct adsp_data sm8550_mpss_resource = {
	.crash_reason_smem = 421,
	.firmware_name = "modem.mdt",
	.dtb_firmware_name = "modem_dtb.mdt",
	.pas_id = 4,
	.dtb_pas_id = 0x26,
	.minidump_id = 3,
	.auto_boot = false,
	.decrypt_shutdown = true,
	.proxy_pd_names = (char*[]){
		"cx",
		"mss",
		NULL
	},
	.load_state = "modem",
	.ssr_name = "mpss",
	.sysmon_name = "modem",
	.ssctl_id = 0x12,
	.region_assign_idx = 2,
};

static const struct adsp_data sun_adsp_resource = {
	.crash_reason_smem = 423,
	.firmware_name = "adsp.mdt",
	.dtb_firmware_name = "adsp_dtb.mdt",
	.pas_id = 1,
	.dtb_pas_id = 0x24,
	.minidump_id = 5,
	.load_state = "adsp",
	.ssr_name = "lpass",
	.sysmon_name = "adsp",
	.ssctl_id = 0x14,
	.uses_elf64 = true,
	.auto_boot = true,
	.crash_reason_stack = 660,
	.smem_host_id = 2,
};

static const struct adsp_data sun_cdsp_resource = {
	.crash_reason_smem = 601,
	.firmware_name = "cdsp.mdt",
	.dtb_firmware_name = "cdsp_dtb.mdt",
	.pas_id = 18,
	.dtb_pas_id = 0x25,
	.minidump_id = 7,
	.load_state = "cdsp",
	.ssr_name = "cdsp",
	.sysmon_name = "cdsp",
	.ssctl_id = 0x17,
	.uses_elf64 = true,
	.region_assign_idx = 2,
	.region_assign_count = 1,
	.region_assign_shared = true,
	.region_assign_vmid = QCOM_SCM_VMID_CDSP,
	.auto_boot = true,
	.crash_reason_stack = 660,
	.smem_host_id = 5,
};

static const struct adsp_data sun_mpss_resource = {
	.crash_reason_smem = 421,
	.firmware_name = "modem.mdt",
	.dtb_firmware_name = "modem_dtb.mdt",
	.pas_id = 4,
	.dtb_pas_id = 0x26,
	.minidump_id = 3,
	.decrypt_shutdown = true,
	.load_state = "modem",
	.ssr_name = "mpss",
	.uses_elf64 = true,
	.sysmon_name = "modem",
	.ssctl_id = 0x12,
	.region_assign_idx = 3,
	.region_assign_count = 2,
	.region_assign_vmid = QCOM_SCM_VMID_MSS_MSA,
	.dma_phys_below_32b = true,
	.both_dumps = true,
};

static const struct adsp_data sun_soccp_resource = {
	.crash_reason_smem = 656,
	.firmware_name = "soccp.mbn",
	.pas_id = 51,
	.ssr_name = "soccp",
	.sysmon_name = "soccp",
	.check_status = true,
	.auto_boot = true,
};

static const struct adsp_data tuna_adsp_resource = {
	.crash_reason_smem = 423,
	.firmware_name = "adsp.mdt",
	.dtb_firmware_name = "adsp_dtb.mdt",
	.pas_id = 1,
	.dtb_pas_id = 0x24,
	.minidump_id = 5,
	.load_state = "adsp",
	.ssr_name = "lpass",
	.sysmon_name = "adsp",
	.ssctl_id = 0x14,
	.uses_elf64 = true,
	.auto_boot = true,
	.crash_reason_stack = 660,
	.smem_host_id = 2,
};

static const struct adsp_data tuna_cdsp_resource = {
	.crash_reason_smem = 601,
	.firmware_name = "cdsp.mdt",
	.dtb_firmware_name = "cdsp_dtb.mdt",
	.pas_id = 18,
	.dtb_pas_id = 0x25,
	.minidump_id = 7,
	.load_state = "cdsp",
	.ssr_name = "cdsp",
	.sysmon_name = "cdsp",
	.ssctl_id = 0x17,
	.uses_elf64 = true,
	.region_assign_idx = 2,
	.region_assign_count = 1,
	.region_assign_shared = true,
	.region_assign_vmid = QCOM_SCM_VMID_CDSP,
	.auto_boot = true,
	.crash_reason_stack = 660,
	.smem_host_id = 5,
};

static const struct adsp_data tuna_mpss_resource = {
	.crash_reason_smem = 421,
	.firmware_name = "modem.mdt",
	.dtb_firmware_name = "modem_dtb.mdt",
	.pas_id = 4,
	.dtb_pas_id = 0x26,
	.minidump_id = 3,
	.decrypt_shutdown = true,
	.load_state = "modem",
	.ssr_name = "mpss",
	.uses_elf64 = true,
	.auto_boot = false,
	.sysmon_name = "modem",
	.ssctl_id = 0x12,
	.region_assign_idx = 3,
	.region_assign_count = 1,
	.region_assign_vmid = QCOM_SCM_VMID_MSS_MSA,
	.dma_phys_below_32b = true,
	.both_dumps = true,
};

static const struct adsp_data kera_adsp_resource = {
	.crash_reason_smem = 423,
	.firmware_name = "adsp.mdt",
	.dtb_firmware_name = "adsp_dtb.mdt",
	.pas_id = 1,
	.dtb_pas_id = 0x24,
	.minidump_id = 5,
	.load_state = "adsp",
	.ssr_name = "lpass",
	.sysmon_name = "adsp",
	.ssctl_id = 0x14,
	.uses_elf64 = true,
	.auto_boot = true,
	.crash_reason_stack = 660,
	.smem_host_id = 2,
};

static const struct adsp_data kera_cdsp_resource = {
	.crash_reason_smem = 601,
	.firmware_name = "cdsp.mdt",
	.dtb_firmware_name = "cdsp_dtb.mdt",
	.pas_id = 18,
	.dtb_pas_id = 0x25,
	.minidump_id = 7,
	.load_state = "cdsp",
	.ssr_name = "cdsp",
	.sysmon_name = "cdsp",
	.ssctl_id = 0x17,
	.uses_elf64 = true,
	.region_assign_idx = 2,
	.region_assign_count = 1,
	.region_assign_shared = true,
	.region_assign_vmid = QCOM_SCM_VMID_CDSP,
	.auto_boot = true,
	.crash_reason_stack = 660,
	.smem_host_id = 5,
};

static const struct adsp_data kera_mpss_resource = {
	.crash_reason_smem = 421,
	.firmware_name = "modem.mdt",
	.dtb_firmware_name = "modem_dtb.mdt",
	.pas_id = 4,
	.dtb_pas_id = 0x26,
	.minidump_id = 3,
	.decrypt_shutdown = true,
	.load_state = "modem",
	.ssr_name = "mpss",
	.uses_elf64 = true,
	.auto_boot = false,
	.sysmon_name = "modem",
	.ssctl_id = 0x12,
	.region_assign_idx = 3,
	.region_assign_count = 1,
	.region_assign_vmid = QCOM_SCM_VMID_MSS_MSA,
	.dma_phys_below_32b = true,
	.both_dumps = true,
};

static const struct adsp_data pineapple_adsp_resource = {
	.crash_reason_smem = 423,
	.firmware_name = "adsp.mdt",
	.dtb_firmware_name = "adsp_dtb.mdt",
	.pas_id = 1,
	.dtb_pas_id = 0x24,
	.minidump_id = 5,
	.load_state = "adsp",
	.ssr_name = "lpass",
	.sysmon_name = "adsp",
	.ssctl_id = 0x14,
	.uses_elf64 = true,
	.auto_boot = true,
};

static const struct adsp_data pineapple_cdsp_resource = {
	.crash_reason_smem = 601,
	.firmware_name = "cdsp.mdt",
	.dtb_firmware_name = "cdsp_dtb.mdt",
	.pas_id = 18,
	.dtb_pas_id = 0x25,
	.minidump_id = 7,
	.load_state = "cdsp",
	.ssr_name = "cdsp",
	.sysmon_name = "cdsp",
	.ssctl_id = 0x17,
	.uses_elf64 = true,
	.region_assign_idx = 2,
	.region_assign_count = 1,
	.region_assign_shared = true,
	.region_assign_vmid = QCOM_SCM_VMID_CDSP,
	.auto_boot = true,
};

static const struct adsp_data pineapple_mpss_resource = {
	.crash_reason_smem = 421,
	.firmware_name = "modem.mdt",
	.dtb_firmware_name = "modem_dtb.mdt",
	.pas_id = 4,
	.dtb_pas_id = 0x26,
	.minidump_id = 3,
	.decrypt_shutdown = true,
	.load_state = "modem",
	.ssr_name = "mpss",
	.uses_elf64 = true,
	.sysmon_name = "modem",
	.ssctl_id = 0x12,
	.region_assign_idx = 3,
	.region_assign_count = 2,
	.region_assign_vmid = QCOM_SCM_VMID_MSS_MSA,
	.dma_phys_below_32b = true,
};

static const struct adsp_data parrot_adsp_resource = {
	.crash_reason_smem = 423,
	.firmware_name = "adsp.mdt",
	.pas_id = 1,
	.minidump_id = 5,
	.load_state = "adsp",
	.ssr_name = "lpass",
	.sysmon_name = "adsp",
	.ssctl_id = 0x14,
	.uses_elf64 = true,
	.auto_boot = false,
};

static const struct adsp_data parrot_cdsp_resource = {
	.crash_reason_smem = 601,
	.firmware_name = "cdsp.mdt",
	.pas_id = 18,
	.minidump_id = 7,
	.load_state = "cdsp",
	.uses_elf64 = true,
	.auto_boot = true,
	.ssr_name = "cdsp",
	.sysmon_name = "cdsp",
	.ssctl_id = 0x17,
};

static const struct adsp_data parrot_mpss_resource = {
	.crash_reason_smem = 421,
	.firmware_name = "modem.mdt",
	.pas_id = 4,
	.minidump_id = 3,
	.load_state = "modem",
	.uses_elf64 = true,
	.auto_boot = false,
	.ssr_name = "mpss",
	.sysmon_name = "modem",
	.ssctl_id = 0x12,
	.dma_phys_below_32b = true,
	.decrypt_shutdown = true,
	.both_dumps = true,
};

static const struct adsp_data parrot_wpss_resource = {
	.crash_reason_smem = 626,
	.firmware_name = "wpss.mdt",
	.pas_id = 6,
	.minidump_id = 4,
	.load_state = "wpss",
	.uses_elf64 = true,
	.ssr_name = "wpss",
	.sysmon_name = "wpss",
	.ssctl_id = 0x19,
};

static const struct adsp_data ravelin_adsp_resource = {
	.crash_reason_smem = 423,
	.firmware_name = "adsp.mdt",
	.pas_id = 1,
	.minidump_id = 5,
	.load_state = "adsp",
	.ssr_name = "lpass",
	.sysmon_name = "adsp",
	.ssctl_id = 0x14,
	.uses_elf64 = true,
	.auto_boot = false,
};

static const struct adsp_data ravelin_mpss_resource = {
	.crash_reason_smem = 421,
	.firmware_name = "modem.mdt",
	.pas_id = 4,
	.minidump_id = 3,
	.load_state = "modem",
	.uses_elf64 = true,
	.auto_boot = false,
	.ssr_name = "mpss",
	.sysmon_name = "modem",
	.ssctl_id = 0x12,
	.dma_phys_below_32b = true,
	.decrypt_shutdown = true,
	.both_dumps = true,
};

static const struct adsp_data ravelin_wpss_resource = {
	.crash_reason_smem = 626,
	.firmware_name = "wpss.mdt",
	.pas_id = 6,
	.minidump_id = 4,
	.load_state = "wpss",
	.uses_elf64 = true,
	.ssr_name = "wpss",
	.sysmon_name = "wpss",
	.ssctl_id = 0x19,
};

static const struct adsp_data monaco_adsp_resource = {
	.crash_reason_smem = 423,
	.firmware_name = "adsp.mdt",
	.pas_id = 1,
	.minidump_id = 5,
	.uses_elf64 = false,
	.ssr_name = "lpass",
	.sysmon_name = "adsp",
	.ssctl_id = 0x14,
};

static const struct adsp_data monaco_modem_resource = {
	.crash_reason_smem = 421,
	.firmware_name = "modem.mdt",
	.pas_id = 4,
	.minidump_id = 3,
	.uses_elf64 = true,
	.ssr_name = "mpss",
	.sysmon_name = "modem",
	.ssctl_id = 0x12,
};

static const struct adsp_data tuna_wpss_resource = {
	.crash_reason_smem = 626,
	.firmware_name = "wpss.mdt",
	.pas_id = 6,
	.minidump_id = 4,
	.load_state = "wpss",
	.uses_elf64 = true,
	.ssr_name = "wpss",
	.sysmon_name = "wpss",
	.ssctl_id = 0x19,
};

static const struct adsp_data kera_wpss_resource = {
	.crash_reason_smem = 626,
	.firmware_name = "wpss.mdt",
	.pas_id = 6,
	.minidump_id = 4,
	.load_state = "wpss",
	.uses_elf64 = true,
	.ssr_name = "wpss",
	.sysmon_name = "wpss",
	.ssctl_id = 0x19,
};

static const struct of_device_id adsp_of_match[] = {
	{ .compatible = "qcom,msm8226-adsp-pil", .data = &adsp_resource_init},
	{ .compatible = "qcom,msm8953-adsp-pil", .data = &msm8996_adsp_resource},
	{ .compatible = "qcom,msm8974-adsp-pil", .data = &adsp_resource_init},
	{ .compatible = "qcom,msm8996-adsp-pil", .data = &msm8996_adsp_resource},
	{ .compatible = "qcom,msm8996-slpi-pil", .data = &msm8996_slpi_resource_init},
	{ .compatible = "qcom,msm8998-adsp-pas", .data = &msm8996_adsp_resource},
	{ .compatible = "qcom,msm8998-slpi-pas", .data = &msm8996_slpi_resource_init},
	{ .compatible = "qcom,qcs404-adsp-pas", .data = &adsp_resource_init },
	{ .compatible = "qcom,qcs404-cdsp-pas", .data = &cdsp_resource_init },
	{ .compatible = "qcom,qcs404-wcss-pas", .data = &wcss_resource_init },
	{ .compatible = "qcom,sc7180-mpss-pas", .data = &mpss_resource_init},
	{ .compatible = "qcom,sc7280-mpss-pas", .data = &mpss_resource_init},
	{ .compatible = "qcom,sc8180x-adsp-pas", .data = &sm8150_adsp_resource},
	{ .compatible = "qcom,sc8180x-cdsp-pas", .data = &sm8150_cdsp_resource},
	{ .compatible = "qcom,sc8180x-mpss-pas", .data = &sc8180x_mpss_resource},
	{ .compatible = "qcom,sc8280xp-adsp-pas", .data = &sm8250_adsp_resource},
	{ .compatible = "qcom,sc8280xp-nsp0-pas", .data = &sc8280xp_nsp0_resource},
	{ .compatible = "qcom,sc8280xp-nsp1-pas", .data = &sc8280xp_nsp1_resource},
	{ .compatible = "qcom,sdm660-adsp-pas", .data = &adsp_resource_init},
	{ .compatible = "qcom,sdm845-adsp-pas", .data = &sdm845_adsp_resource_init},
	{ .compatible = "qcom,sdm845-cdsp-pas", .data = &sdm845_cdsp_resource_init},
	{ .compatible = "qcom,sdm845-slpi-pas", .data = &sdm845_slpi_resource_init},
	{ .compatible = "qcom,sdx55-mpss-pas", .data = &sdx55_mpss_resource},
	{ .compatible = "qcom,sdxpinn-modem-pas", .data = &sdxpinn_mpss_resource},
	{ .compatible = "qcom,sm6115-adsp-pas", .data = &adsp_resource_init},
	{ .compatible = "qcom,sm6115-cdsp-pas", .data = &cdsp_resource_init},
	{ .compatible = "qcom,sm6115-mpss-pas", .data = &sc8180x_mpss_resource},
	{ .compatible = "qcom,sm6350-adsp-pas", .data = &sm6350_adsp_resource},
	{ .compatible = "qcom,sm6350-cdsp-pas", .data = &sm6350_cdsp_resource},
	{ .compatible = "qcom,sm6350-mpss-pas", .data = &mpss_resource_init},
	{ .compatible = "qcom,sm8150-adsp-pas", .data = &sm8150_adsp_resource},
	{ .compatible = "qcom,sm8150-cdsp-pas", .data = &sm8150_cdsp_resource},
	{ .compatible = "qcom,sm8150-mpss-pas", .data = &mpss_resource_init},
	{ .compatible = "qcom,sm8150-slpi-pas", .data = &sdm845_slpi_resource_init},
	{ .compatible = "qcom,sm8250-adsp-pas", .data = &sm8250_adsp_resource},
	{ .compatible = "qcom,sm8250-cdsp-pas", .data = &sm8250_cdsp_resource},
	{ .compatible = "qcom,sm8250-slpi-pas", .data = &sdm845_slpi_resource_init},
	{ .compatible = "qcom,sm8350-adsp-pas", .data = &sm8350_adsp_resource},
	{ .compatible = "qcom,sm8350-cdsp-pas", .data = &sm8350_cdsp_resource},
	{ .compatible = "qcom,sm8350-slpi-pas", .data = &sdm845_slpi_resource_init},
	{ .compatible = "qcom,sm8350-mpss-pas", .data = &mpss_resource_init},
	{ .compatible = "qcom,sm8450-adsp-pas", .data = &sm8350_adsp_resource},
	{ .compatible = "qcom,sm8450-cdsp-pas", .data = &sm8350_cdsp_resource},
	{ .compatible = "qcom,sm8450-slpi-pas", .data = &sdm845_slpi_resource_init},
	{ .compatible = "qcom,sm8450-mpss-pas", .data = &sm8450_mpss_resource},
	{ .compatible = "qcom,sm8550-adsp-pas", .data = &sm8550_adsp_resource},
	{ .compatible = "qcom,sm8550-cdsp-pas", .data = &sm8550_cdsp_resource},
	{ .compatible = "qcom,sm8550-mpss-pas", .data = &sm8550_mpss_resource},
	{ .compatible = "qcom,pineapple-adsp-pas", .data = &pineapple_adsp_resource},
	{ .compatible = "qcom,pineapple-cdsp-pas", .data = &pineapple_cdsp_resource},
	{ .compatible = "qcom,pineapple-modem-pas", .data = &pineapple_mpss_resource},
	{ .compatible = "qcom,sun-adsp-pas", .data = &sun_adsp_resource},
	{ .compatible = "qcom,sun-cdsp-pas", .data = &sun_cdsp_resource},
	{ .compatible = "qcom,sun-modem-pas", .data = &sun_mpss_resource},
	{ .compatible = "qcom,sun-soccp-pas", .data = &sun_soccp_resource},
	{ .compatible = "qcom,tuna-adsp-pas", .data = &tuna_adsp_resource},
	{ .compatible = "qcom,tuna-cdsp-pas", .data = &tuna_cdsp_resource},
	{ .compatible = "qcom,tuna-modem-pas", .data = &tuna_mpss_resource},
	{ .compatible = "qcom,kera-adsp-pas", .data = &kera_adsp_resource},
	{ .compatible = "qcom,kera-cdsp-pas", .data = &kera_cdsp_resource},
	{ .compatible = "qcom,kera-modem-pas", .data = &kera_mpss_resource},
	{ .compatible = "qcom,parrot-adsp-pas", .data = &parrot_adsp_resource},
	{ .compatible = "qcom,parrot-cdsp-pas", .data = &parrot_cdsp_resource},
	{ .compatible = "qcom,parrot-modem-pas", .data = &parrot_mpss_resource},
	{ .compatible = "qcom,parrot-wpss-pas", .data = &parrot_wpss_resource},
	{ .compatible = "qcom,ravelin-adsp-pas", .data = &ravelin_adsp_resource},
	{ .compatible = "qcom,ravelin-modem-pas", .data = &ravelin_mpss_resource},
	{ .compatible = "qcom,ravelin-wpss-pas", .data = &ravelin_wpss_resource},
	{ .compatible = "qcom,monaco-adsp-pas", .data = &monaco_adsp_resource},
	{ .compatible = "qcom,monaco-modem-pas", .data = &monaco_modem_resource},
	{ .compatible = "qcom,tuna-wpss-pas", .data = &tuna_wpss_resource},
	{ .compatible = "qcom,kera-wpss-pas", .data = &kera_wpss_resource},
	{ },
};
MODULE_DEVICE_TABLE(of, adsp_of_match);

static struct platform_driver adsp_driver = {
	.probe = adsp_probe,
	.remove_new = adsp_remove,
	.driver = {
		.name = "qcom_q6v5_pas",
		.of_match_table = adsp_of_match,
	},
};

module_platform_driver(adsp_driver);
MODULE_DESCRIPTION("Qualcomm Hexagon v5 Peripheral Authentication Service driver");
MODULE_LICENSE("GPL v2");
