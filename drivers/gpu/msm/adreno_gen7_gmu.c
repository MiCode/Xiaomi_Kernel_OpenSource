// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <dt-bindings/regulator/qcom,rpmh-regulator-levels.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/dma-map-ops.h>
#include <linux/firmware.h>
#include <linux/interconnect.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kobject.h>
#include <linux/of_platform.h>
#include <linux/qcom-iommu-util.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/soc/qcom/llcc-qcom.h>
#include <linux/sysfs.h>
#include <linux/mailbox/qmp.h>
#include <soc/qcom/cmd-db.h>

#include "adreno.h"
#include "adreno_gen7.h"
#include "adreno_trace.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"
#include "kgsl_util.h"

static struct gmu_vma_entry gen7_gmu_vma[] = {
	[GMU_ITCM] = {
			.start = 0x00000000,
			.size = SZ_16K,
		},
	[GMU_CACHE] = {
			.start = SZ_16K,
			.size = (SZ_16M - SZ_16K),
			.next_va = SZ_16K,
		},
	[GMU_DTCM] = {
			.start = SZ_256M + SZ_16K,
			.size = SZ_16K,
		},
	[GMU_DCACHE] = {
			.start = 0x0,
			.size = 0x0,
		},
	[GMU_NONCACHED_KERNEL] = {
			.start = 0x60000000,
			.size = SZ_512M,
			.next_va = 0x60000000,
		},
	[GMU_NONCACHED_KERNEL_EXTENDED] = {
			.start = 0xc0000000,
			.size = SZ_512M,
			.next_va = 0xc0000000,
		},
};

static u32 gen7_rscc_tcsm_drv0_status_reglist[] = {
	GEN7_RSCC_TCS0_DRV0_STATUS,
	GEN7_RSCC_TCS1_DRV0_STATUS,
	GEN7_RSCC_TCS2_DRV0_STATUS,
	GEN7_RSCC_TCS3_DRV0_STATUS,
	GEN7_RSCC_TCS4_DRV0_STATUS,
	GEN7_RSCC_TCS5_DRV0_STATUS,
	GEN7_RSCC_TCS6_DRV0_STATUS,
	GEN7_RSCC_TCS7_DRV0_STATUS,
	GEN7_RSCC_TCS8_DRV0_STATUS,
	GEN7_RSCC_TCS9_DRV0_STATUS,
};

static u32 gen7_6_0_rscc_tcsm_drv0_status_reglist[] = {
	GEN7_6_0_RSCC_TCS0_DRV0_STATUS,
	GEN7_6_0_RSCC_TCS1_DRV0_STATUS,
	GEN7_6_0_RSCC_TCS2_DRV0_STATUS,
	GEN7_6_0_RSCC_TCS3_DRV0_STATUS,
	GEN7_6_0_RSCC_TCS4_DRV0_STATUS,
	GEN7_6_0_RSCC_TCS5_DRV0_STATUS,
	GEN7_6_0_RSCC_TCS6_DRV0_STATUS,
	GEN7_6_0_RSCC_TCS7_DRV0_STATUS,
	GEN7_6_0_RSCC_TCS8_DRV0_STATUS,
	GEN7_6_0_RSCC_TCS9_DRV0_STATUS,
};

static ssize_t log_stream_enable_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct gen7_gmu_device *gmu = container_of(kobj, struct gen7_gmu_device, log_kobj);
	bool val;
	int ret;

	ret = kstrtobool(buf, &val);
	if (ret)
		return ret;

	gmu->log_stream_enable = val;
	return count;
}

static ssize_t log_stream_enable_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct gen7_gmu_device *gmu = container_of(kobj, struct gen7_gmu_device, log_kobj);

	return scnprintf(buf, PAGE_SIZE, "%d\n", gmu->log_stream_enable);
}

static ssize_t log_group_mask_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct gen7_gmu_device *gmu = container_of(kobj, struct gen7_gmu_device, log_kobj);
	u32 val;
	int ret;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return ret;

	gmu->log_group_mask = val;
	return count;
}

static ssize_t log_group_mask_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct gen7_gmu_device *gmu = container_of(kobj, struct gen7_gmu_device, log_kobj);

	return scnprintf(buf, PAGE_SIZE, "%x\n", gmu->log_group_mask);
}

static struct kobj_attribute log_stream_enable_attr =
	__ATTR(log_stream_enable, 0644, log_stream_enable_show, log_stream_enable_store);

static struct kobj_attribute log_group_mask_attr =
	__ATTR(log_group_mask, 0644, log_group_mask_show, log_group_mask_store);

static struct attribute *log_attrs[] = {
	&log_stream_enable_attr.attr,
	&log_group_mask_attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(log);

static struct kobj_type log_kobj_type = {
	.sysfs_ops = &kobj_sysfs_ops,
	.default_groups = log_groups,
};

static int gen7_timed_poll_check_rscc(struct gen7_gmu_device *gmu,
		unsigned int offset, unsigned int expected_ret,
		unsigned int timeout, unsigned int mask)
{
	u32 value;

	return readl_poll_timeout(gmu->rscc_virt + (offset << 2), value,
		(value & mask) == expected_ret, 100, timeout * 1000);
}

struct gen7_gmu_device *to_gen7_gmu(struct adreno_device *adreno_dev)
{
	struct gen7_device *gen7_dev = container_of(adreno_dev,
					struct gen7_device, adreno_dev);

	return &gen7_dev->gmu;
}

struct adreno_device *gen7_gmu_to_adreno(struct gen7_gmu_device *gmu)
{
	struct gen7_device *gen7_dev =
			container_of(gmu, struct gen7_device, gmu);

	return &gen7_dev->adreno_dev;
}

#define RSC_CMD_OFFSET 2

static void _regwrite(void __iomem *regbase,
		unsigned int offsetwords, unsigned int value)
{
	void __iomem *reg;

	reg = regbase + (offsetwords << 2);
	__raw_writel(value, reg);
}

void gen7_load_rsc_ucode(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	void __iomem *rscc = gmu->rscc_virt;
	unsigned int seq_offset = GEN7_RSCC_SEQ_MEM_0_DRV0;

	/* Disable SDE clock gating */
	_regwrite(rscc, GEN7_GPU_RSCC_RSC_STATUS0_DRV0, BIT(24));

	/* Setup RSC PDC handshake for sleep and wakeup */
	_regwrite(rscc, GEN7_RSCC_PDC_SLAVE_ID_DRV0, 1);
	_regwrite(rscc, GEN7_RSCC_HIDDEN_TCS_CMD0_DATA, 0);
	_regwrite(rscc, GEN7_RSCC_HIDDEN_TCS_CMD0_ADDR, 0);
	_regwrite(rscc, GEN7_RSCC_HIDDEN_TCS_CMD0_DATA + RSC_CMD_OFFSET, 0);
	_regwrite(rscc, GEN7_RSCC_HIDDEN_TCS_CMD0_ADDR + RSC_CMD_OFFSET, 0);
	_regwrite(rscc, GEN7_RSCC_HIDDEN_TCS_CMD0_DATA + RSC_CMD_OFFSET * 2,
			0x80000000);
	_regwrite(rscc, GEN7_RSCC_HIDDEN_TCS_CMD0_ADDR + RSC_CMD_OFFSET * 2, 0);
	_regwrite(rscc, GEN7_RSCC_OVERRIDE_START_ADDR, 0);
	_regwrite(rscc, GEN7_RSCC_PDC_SEQ_START_ADDR, 0x4520);
	_regwrite(rscc, GEN7_RSCC_PDC_MATCH_VALUE_LO, 0x4510);
	_regwrite(rscc, GEN7_RSCC_PDC_MATCH_VALUE_HI, 0x4514);

	if (adreno_is_gen7_6_0(adreno_dev))
		seq_offset = GEN7_6_0_RSCC_SEQ_MEM_0_DRV0;

	/* Load RSC sequencer uCode for sleep and wakeup */
	_regwrite(rscc, seq_offset, 0xeaaae5a0);
	_regwrite(rscc, seq_offset + 1, 0xe1a1ebab);
	_regwrite(rscc, seq_offset + 2, 0xa2e0a581);
	_regwrite(rscc, seq_offset + 3, 0xecac82e2);
	_regwrite(rscc, seq_offset + 4, 0x0020edad);
}

int gen7_load_pdc_ucode(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct resource *res_cfg;
	void __iomem *cfg = NULL;

	res_cfg = platform_get_resource_byname(gmu->pdev, IORESOURCE_MEM,
			"gmu_pdc");
	if (res_cfg)
		cfg = ioremap(res_cfg->start, resource_size(res_cfg));

	if (!cfg) {
		dev_err(&gmu->pdev->dev, "Failed to map PDC CFG\n");
		return -ENODEV;
	}

	/* Setup GPU PDC */
	_regwrite(cfg, GEN7_PDC_GPU_SEQ_START_ADDR, 0);
	_regwrite(cfg, GEN7_PDC_GPU_ENABLE_PDC, 0x80000001);

	iounmap(cfg);

	return 0;
}

/* Configure and enable GMU low power mode */
static void gen7_gmu_power_config(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* Disable GMU WB/RB buffer and caches at boot */
	gmu_core_regwrite(device, GEN7_GMU_SYS_BUS_CONFIG, 0x1);
	gmu_core_regwrite(device, GEN7_GMU_ICACHE_CONFIG, 0x1);
	gmu_core_regwrite(device, GEN7_GMU_DCACHE_CONFIG, 0x1);
}

static void gmu_ao_sync_event(struct adreno_device *adreno_dev)
{
	unsigned long flags;
	u64 ticks;

	/*
	 * Get the GMU always on ticks and log it in a trace message. This
	 * will be used to map GMU ticks to ftrace time. Do this in atomic
	 * context to ensure nothing happens between reading the always
	 * on ticks and doing the trace.
	 */

	local_irq_save(flags);

	ticks = gen7_read_alwayson(adreno_dev);

	trace_gmu_ao_sync(ticks);

	local_irq_restore(flags);
}

int gen7_gmu_device_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	gmu_ao_sync_event(adreno_dev);

	/* Bring GMU out of reset */
	gmu_core_regwrite(device, GEN7_GMU_CM3_SYSRESET, 0);

	/* Make sure the write is posted before moving ahead */
	wmb();

	if (gmu_core_timed_poll_check(device, GEN7_GMU_CM3_FW_INIT_RESULT,
			BIT(8), 100, GENMASK(8, 0))) {
		dev_err(&gmu->pdev->dev, "GMU failed to come out of reset\n");
		gmu_core_fault_snapshot(device);
		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * gen7_gmu_hfi_start() - Write registers and start HFI.
 * @device: Pointer to KGSL device
 */
int gen7_gmu_hfi_start(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	gmu_core_regwrite(device, GEN7_GMU_HFI_CTRL_INIT, 1);

	if (gmu_core_timed_poll_check(device, GEN7_GMU_HFI_CTRL_STATUS,
			BIT(0), 100, BIT(0))) {
		dev_err(&gmu->pdev->dev, "GMU HFI init failed\n");
		gmu_core_fault_snapshot(device);
		return -ETIMEDOUT;
	}

	return 0;
}

int gen7_rscc_wakeup_sequence(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct device *dev = &gmu->pdev->dev;

	/* Skip wakeup sequence if we didn't do the sleep sequence */
	if (!test_bit(GMU_PRIV_RSCC_SLEEP_DONE, &gmu->flags))
		return 0;

	/* RSC wake sequence */
	gmu_core_regwrite(device, GEN7_GMU_RSCC_CONTROL_REQ, BIT(1));

	/* Write request before polling */
	wmb();

	if (gmu_core_timed_poll_check(device, GEN7_GMU_RSCC_CONTROL_ACK,
				BIT(1), 100, BIT(1))) {
		dev_err(dev, "Failed to do GPU RSC power on\n");
		return -ETIMEDOUT;
	}

	if (gen7_timed_poll_check_rscc(gmu, GEN7_RSCC_SEQ_BUSY_DRV0,
				0x0, 100, UINT_MAX)) {
		dev_err(dev, "GPU RSC sequence stuck in waking up GPU\n");
		return -ETIMEDOUT;
	}

	gmu_core_regwrite(device, GEN7_GMU_RSCC_CONTROL_REQ, 0);

	clear_bit(GMU_PRIV_RSCC_SLEEP_DONE, &gmu->flags);

	return 0;
}

int gen7_rscc_sleep_sequence(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret;

	if (!test_bit(GMU_PRIV_FIRST_BOOT_DONE, &gmu->flags))
		return 0;

	if (test_bit(GMU_PRIV_RSCC_SLEEP_DONE, &gmu->flags))
		return 0;

	gmu_core_regwrite(device, GEN7_GMU_CM3_SYSRESET, 1);
	/* Make sure M3 is in reset before going on */
	wmb();

	gmu_core_regread(device, GEN7_GMU_GENERAL_9, &gmu->log_wptr_retention);

	gmu_core_regwrite(device, GEN7_GMU_RSCC_CONTROL_REQ, BIT(0));
	/* Make sure the request completes before continuing */
	wmb();

	ret = gen7_timed_poll_check_rscc(gmu, GEN7_GPU_RSCC_RSC_STATUS0_DRV0,
			BIT(16), 100, BIT(16));
	if (ret) {
		dev_err(&gmu->pdev->dev, "GPU RSC power off fail\n");
		return -ETIMEDOUT;
	}

	gmu_core_regwrite(device, GEN7_GMU_RSCC_CONTROL_REQ, 0);

	if (adreno_dev->lm_enabled)
		gmu_core_regwrite(device, GEN7_GMU_AO_SPARE_CNTL, 0);

	set_bit(GMU_PRIV_RSCC_SLEEP_DONE, &gmu->flags);

	return 0;
}

static struct kgsl_memdesc *find_gmu_memdesc(struct gen7_gmu_device *gmu,
	u32 addr, u32 size)
{
	int i;

	for (i = 0; i < gmu->global_entries; i++) {
		struct kgsl_memdesc *md = &gmu->gmu_globals[i];

		if ((addr >= md->gmuaddr) &&
				(((addr + size) <= (md->gmuaddr + md->size))))
			return md;
	}

	return NULL;
}

static int find_vma_block(struct gen7_gmu_device *gmu, u32 addr, u32 size)
{
	int i;

	for (i = 0; i < GMU_MEM_TYPE_MAX; i++) {
		struct gmu_vma_entry *vma = &gmu->vma[i];

		if ((addr >= vma->start) &&
			((addr + size) <= (vma->start + vma->size)))
			return i;
	}

	return -ENOENT;
}

static void load_tcm(struct adreno_device *adreno_dev, const u8 *src,
	u32 tcm_start, u32 base, const struct gmu_block_header *blk)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 tcm_offset = tcm_start + ((blk->addr - base)/sizeof(u32));

	kgsl_regmap_bulk_write(&device->regmap, tcm_offset, src,
		blk->size >> 2);
}

int gen7_gmu_load_fw(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	const u8 *fw = (const u8 *)gmu->fw_image->data;

	while (fw < gmu->fw_image->data + gmu->fw_image->size) {
		const struct gmu_block_header *blk =
					(const struct gmu_block_header *)fw;
		int id;

		fw += sizeof(*blk);

		/* Don't deal with zero size blocks */
		if (blk->size == 0)
			continue;

		id = find_vma_block(gmu, blk->addr, blk->size);

		if (id < 0) {
			dev_err(&gmu->pdev->dev,
				"Unknown block in GMU FW addr:0x%x size:0x%x\n",
				blk->addr, blk->size);
			return -EINVAL;
		}

		if (id == GMU_ITCM) {
			load_tcm(adreno_dev, fw,
				GEN7_GMU_CM3_ITCM_START,
				gmu->vma[GMU_ITCM].start, blk);
		} else if (id == GMU_DTCM) {
			load_tcm(adreno_dev, fw,
				GEN7_GMU_CM3_DTCM_START,
				gmu->vma[GMU_DTCM].start, blk);
		} else {
			struct kgsl_memdesc *md =
				find_gmu_memdesc(gmu, blk->addr, blk->size);

			if (!md) {
				dev_err(&gmu->pdev->dev,
					"No backing memory for GMU FW block addr:0x%x size:0x%x\n",
					blk->addr, blk->size);
				return -EINVAL;
			}

			memcpy(md->hostptr + (blk->addr - md->gmuaddr), fw,
				blk->size);
		}

		fw += blk->size;
	}

	/* Proceed only after the FW is written */
	wmb();
	return 0;
}

static const char *oob_to_str(enum oob_request req)
{
	switch (req) {
	case oob_gpu:
		return "oob_gpu";
	case oob_perfcntr:
		return "oob_perfcntr";
	case oob_boot_slumber:
		return "oob_boot_slumber";
	case oob_dcvs:
		return "oob_dcvs";
	default:
		return "unknown";
	}
}

static void trigger_reset_recovery(struct adreno_device *adreno_dev,
	enum oob_request req)
{
	/*
	 * Trigger recovery for perfcounter oob only since only
	 * perfcounter oob can happen alongside an actively rendering gpu.
	 */
	if (req != oob_perfcntr)
		return;

	if (adreno_dev->dispatch_ops && adreno_dev->dispatch_ops->fault)
		adreno_dev->dispatch_ops->fault(adreno_dev,
			ADRENO_GMU_FAULT_SKIP_SNAPSHOT);
}

int gen7_gmu_oob_set(struct kgsl_device *device,
		enum oob_request req)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret = 0;
	int set, check;

	if (req == oob_perfcntr && gmu->num_oob_perfcntr++)
		return 0;

	if (req >= oob_boot_slumber) {
		dev_err(&gmu->pdev->dev,
			"Unsupported OOB request %s\n",
			oob_to_str(req));
		return -EINVAL;
	}

	set = BIT(30 - req * 2);
	check = BIT(31 - req);

	gmu_core_regwrite(device, GEN7_GMU_HOST2GMU_INTR_SET, set);

	if (gmu_core_timed_poll_check(device, GEN7_GMU_GMU2HOST_INTR_INFO, check,
				100, check)) {
		if (req == oob_perfcntr)
			gmu->num_oob_perfcntr--;
		gmu_core_fault_snapshot(device);
		ret = -ETIMEDOUT;
		WARN(1, "OOB request %s timed out\n", oob_to_str(req));
		trigger_reset_recovery(adreno_dev, req);
	}

	gmu_core_regwrite(device, GEN7_GMU_GMU2HOST_INTR_CLR, check);

	trace_kgsl_gmu_oob_set(set);
	return ret;
}

void gen7_gmu_oob_clear(struct kgsl_device *device,
		enum oob_request req)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int clear = BIT(31 - req * 2);

	if (req == oob_perfcntr && --gmu->num_oob_perfcntr)
		return;

	if (req >= oob_boot_slumber) {
		dev_err(&gmu->pdev->dev, "Unsupported OOB clear %s\n",
				oob_to_str(req));
		return;
	}

	gmu_core_regwrite(device, GEN7_GMU_HOST2GMU_INTR_SET, clear);
	trace_kgsl_gmu_oob_clear(clear);
}

void gen7_gmu_irq_enable(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct gen7_hfi *hfi = &gmu->hfi;

	/* Clear pending IRQs and Unmask needed IRQs */
	gmu_core_regwrite(device, GEN7_GMU_GMU2HOST_INTR_CLR, UINT_MAX);
	gmu_core_regwrite(device, GEN7_GMU_AO_HOST_INTERRUPT_CLR, UINT_MAX);

	gmu_core_regwrite(device, GEN7_GMU_GMU2HOST_INTR_MASK,
			(unsigned int)~HFI_IRQ_MASK);
	gmu_core_regwrite(device, GEN7_GMU_AO_HOST_INTERRUPT_MASK,
			(unsigned int)~GMU_AO_INT_MASK);

	/* Enable all IRQs on host */
	enable_irq(hfi->irq);
	enable_irq(gmu->irq);
}

void gen7_gmu_irq_disable(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct gen7_hfi *hfi = &gmu->hfi;

	/* Disable all IRQs on host */
	disable_irq(gmu->irq);
	disable_irq(hfi->irq);

	/* Mask all IRQs and clear pending IRQs */
	gmu_core_regwrite(device, GEN7_GMU_GMU2HOST_INTR_MASK, UINT_MAX);
	gmu_core_regwrite(device, GEN7_GMU_AO_HOST_INTERRUPT_MASK, UINT_MAX);

	gmu_core_regwrite(device, GEN7_GMU_GMU2HOST_INTR_CLR, UINT_MAX);
	gmu_core_regwrite(device, GEN7_GMU_AO_HOST_INTERRUPT_CLR, UINT_MAX);
}

static int gen7_gmu_hfi_start_msg(struct adreno_device *adreno_dev)
{
	struct hfi_start_cmd req;
	int ret;

	ret = CMD_MSG_HDR(req, H2F_MSG_START);
	if (ret)
		return ret;

	return gen7_hfi_send_generic_req(adreno_dev, &req);
}

static int gen7_complete_rpmh_votes(struct gen7_gmu_device *gmu,
		u32 timeout)
{
	struct adreno_device *adreno_dev = gen7_gmu_to_adreno(gmu);
	int i, ret = 0;

	if (adreno_is_gen7_6_0(adreno_dev)) {
		for (i = 0; i < ARRAY_SIZE(gen7_6_0_rscc_tcsm_drv0_status_reglist); i++)
			ret |= gen7_timed_poll_check_rscc(gmu,
					gen7_6_0_rscc_tcsm_drv0_status_reglist[i],
					BIT(0), timeout, BIT(0));
	} else {
		for (i = 0; i < ARRAY_SIZE(gen7_rscc_tcsm_drv0_status_reglist); i++)
			ret |= gen7_timed_poll_check_rscc(gmu,
					gen7_rscc_tcsm_drv0_status_reglist[i],
					BIT(0), timeout, BIT(0));
	}

	if (ret)
		dev_err(&gmu->pdev->dev, "RPMH votes timedout: %d\n", ret);

	return ret;
}

#define GX_GDSC_POWER_OFF	BIT(0)
#define GX_CLK_OFF		BIT(1)
#define is_on(val)		(!(val & (GX_GDSC_POWER_OFF | GX_CLK_OFF)))

bool gen7_gmu_gx_is_on(struct adreno_device *adreno_dev)
{
	unsigned int val;

	gmu_core_regread(KGSL_DEVICE(adreno_dev),
			GEN7_GMU_GFX_PWR_CLK_STATUS, &val);
	return is_on(val);
}

static const char *idle_level_name(int level)
{
	if (level == GPU_HW_ACTIVE)
		return "GPU_HW_ACTIVE";
	else if (level == GPU_HW_IFPC)
		return "GPU_HW_IFPC";

	return "(Unknown)";
}

int gen7_gmu_wait_for_lowest_idle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	unsigned int reg, reg1, reg2, reg3, reg4, reg5, reg6, reg7, reg8;
	unsigned long t;
	u64 ts1, ts2;

	ts1 = gen7_read_alwayson(adreno_dev);

	t = jiffies + msecs_to_jiffies(100);
	do {
		gmu_core_regread(device,
			GEN7_GPU_GMU_CX_GMU_RPMH_POWER_STATE, &reg);
		gmu_core_regread(device, GEN7_GMU_GFX_PWR_CLK_STATUS, &reg1);

		/*
		 * Check that we are at lowest level. If lowest level is IFPC
		 * double check that GFX clock is off.
		 */
		if (gmu->idle_level == reg)
			if (!(gmu->idle_level == GPU_HW_IFPC && is_on(reg1)))
				return 0;

		/* Wait 100us to reduce unnecessary AHB bus traffic */
		usleep_range(10, 100);
	} while (!time_after(jiffies, t));

	/* Check one last time */
	gmu_core_regread(device, GEN7_GPU_GMU_CX_GMU_RPMH_POWER_STATE, &reg);
	gmu_core_regread(device, GEN7_GMU_GFX_PWR_CLK_STATUS, &reg1);

	/*
	 * Check that we are at lowest level. If lowest level is IFPC
	 * double check that GFX clock is off.
	 */
	if (gmu->idle_level == reg)
		if (!(gmu->idle_level == GPU_HW_IFPC && is_on(reg1)))
			return 0;

	ts2 = gen7_read_alwayson(adreno_dev);

	/* Collect abort data to help with debugging */
	gmu_core_regread(device, GEN7_GPU_GMU_AO_GPU_CX_BUSY_STATUS, &reg2);
	gmu_core_regread(device, GEN7_GMU_RBBM_INT_UNMASKED_STATUS, &reg3);
	gmu_core_regread(device, GEN7_GMU_GMU_PWR_COL_KEEPALIVE, &reg4);
	gmu_core_regread(device, GEN7_GMU_AO_SPARE_CNTL, &reg5);

	dev_err(&gmu->pdev->dev,
		"----------------------[ GMU error ]----------------------\n");
	dev_err(&gmu->pdev->dev,
		"Timeout waiting for lowest idle level %s\n",
		idle_level_name(gmu->idle_level));
	dev_err(&gmu->pdev->dev, "Start: %llx (absolute ticks)\n", ts1);
	dev_err(&gmu->pdev->dev, "Poll: %llx (ticks relative to start)\n",
		ts2-ts1);
	dev_err(&gmu->pdev->dev,
		"RPMH_POWER_STATE=%x GFX_PWR_CLK_STATUS=%x\n", reg, reg1);
	dev_err(&gmu->pdev->dev, "CX_BUSY_STATUS=%x\n", reg2);
	dev_err(&gmu->pdev->dev,
		"RBBM_INT_UNMASKED_STATUS=%x PWR_COL_KEEPALIVE=%x\n",
		reg3, reg4);
	dev_err(&gmu->pdev->dev, "GEN7_GMU_AO_SPARE_CNTL=%x\n", reg5);

	/* Access GX registers only when GX is ON */
	if (is_on(reg1)) {
		kgsl_regread(device, GEN7_CP_STATUS_1, &reg6);
		kgsl_regread(device, GEN7_CP_CP2GMU_STATUS, &reg7);
		kgsl_regread(device, GEN7_CP_CONTEXT_SWITCH_CNTL, &reg8);

		dev_err(&gmu->pdev->dev, "GEN7_CP_STATUS_1=%x\n", reg6);
		dev_err(&gmu->pdev->dev,
			"CP2GMU_STATUS=%x CONTEXT_SWITCH_CNTL=%x\n",
			reg7, reg8);
	}

	WARN_ON(1);
	gmu_core_fault_snapshot(device);
	return -ETIMEDOUT;
}

/* Bitmask for GPU idle status check */
#define CXGXCPUBUSYIGNAHB	BIT(30)
int gen7_gmu_wait_for_idle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	u32 status2;
	u64 ts1;

	ts1 = gen7_read_alwayson(adreno_dev);
	if (gmu_core_timed_poll_check(device, GEN7_GPU_GMU_AO_GPU_CX_BUSY_STATUS,
			0, 100, CXGXCPUBUSYIGNAHB)) {
		gmu_core_regread(device,
				GEN7_GPU_GMU_AO_GPU_CX_BUSY_STATUS2, &status2);
		dev_err(&gmu->pdev->dev,
				"GMU not idling: status2=0x%x %llx %llx\n",
				status2, ts1,
				gen7_read_alwayson(ADRENO_DEVICE(device)));
		gmu_core_fault_snapshot(device);
		return -ETIMEDOUT;
	}

	return 0;
}

void gen7_gmu_version_info(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);

	/* GMU version info is at a fixed offset in the DTCM */
	gmu_core_regread(device, GEN7_GMU_CM3_DTCM_START + 0xff8,
			&gmu->ver.core);
	gmu_core_regread(device, GEN7_GMU_CM3_DTCM_START + 0xff9,
			&gmu->ver.core_dev);
	gmu_core_regread(device, GEN7_GMU_CM3_DTCM_START + 0xffa,
			&gmu->ver.pwr);
	gmu_core_regread(device, GEN7_GMU_CM3_DTCM_START + 0xffb,
			&gmu->ver.pwr_dev);
	gmu_core_regread(device, GEN7_GMU_CM3_DTCM_START + 0xffc,
			&gmu->ver.hfi);

	/* Check gmu fw on device is up to date with minimum required version */
	if (gmu->ver.core < gen7_core->gmu_fw_version)
		dev_err(&gmu->pdev->dev,
			"GMU FW version 0x%x does not satisfy the required minimum (0x%x)\n",
			gmu->ver.core, gen7_core->gmu_fw_version);
}

int gen7_gmu_itcm_shadow(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	u32 i, *dest;

	if (gmu->itcm_shadow)
		return 0;

	gmu->itcm_shadow = vzalloc(gmu->vma[GMU_ITCM].size);
	if (!gmu->itcm_shadow)
		return -ENOMEM;

	dest = (u32 *)gmu->itcm_shadow;

	for (i = 0; i < (gmu->vma[GMU_ITCM].size >> 2); i++)
		gmu_core_regread(KGSL_DEVICE(adreno_dev),
			GEN7_GMU_CM3_ITCM_START + i, dest++);

	return 0;
}

void gen7_gmu_register_config(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 val;

	/* Clear any previously set cm3 fault */
	atomic_set(&gmu->cm3_fault, 0);

	/* Vote veto for FAL10 */
	gmu_core_regwrite(device, GEN7_GPU_GMU_CX_GMU_CX_FALNEXT_INTF, 0x1);
	gmu_core_regwrite(device, GEN7_GPU_GMU_CX_GMU_CX_FAL_INTF, 0x1);

	/* Program GMU QOS control to avoid bandwidth starvation */
	if (adreno_is_gen7_6_0(adreno_dev))
		gmu_core_regwrite(device, GEN7_GMU_CX_MRC_GBIF_QOS_CTRL, 0x33);

	/* Turn on TCM retention */
	adreno_cx_misc_regwrite(adreno_dev, GEN7_GPU_CX_MISC_TCM_RET_CNTL, 1);

	/* Clear init result to make sure we are getting fresh value */
	gmu_core_regwrite(device, GEN7_GMU_CM3_FW_INIT_RESULT, 0);
	gmu_core_regwrite(device, GEN7_GMU_CM3_BOOT_CONFIG, 0x2);

	gmu_core_regwrite(device, GEN7_GMU_HFI_QTBL_ADDR,
			gmu->hfi.hfi_mem->gmuaddr);
	gmu_core_regwrite(device, GEN7_GMU_HFI_QTBL_INFO, 1);

	gmu_core_regwrite(device, GEN7_GMU_AHB_FENCE_RANGE_0, BIT(31) |
			FIELD_PREP(GENMASK(30, 18), 0x32) |
			FIELD_PREP(GENMASK(17, 0), 0x8a0));

	/*
	 * Make sure that CM3 state is at reset value. Snapshot is changing
	 * NMI bit and if we boot up GMU with NMI bit set GMU will boot
	 * straight in to NMI handler without executing __main code
	 */
	gmu_core_regwrite(device, GEN7_GMU_CM3_CFG, 0x4052);

	/**
	 * We may have asserted gbif halt as part of reset sequence which may
	 * not get cleared if the gdsc was not reset. So clear it before
	 * attempting GMU boot.
	 */
	kgsl_regwrite(device, GEN7_GBIF_HALT, 0x0);

	/* Set the log wptr index */
	gmu_core_regwrite(device, GEN7_GMU_GENERAL_9,
			gmu->log_wptr_retention);

	/* Pass chipid to GMU FW, must happen before starting GMU */
	gmu_core_regwrite(device, GEN7_GMU_GENERAL_10,
			adreno_is_gen7_6_0(adreno_dev) ?
			ADRENO_GMU_REV(ADRENO_GPUREV(adreno_dev)) :
			ADRENO_GMU_CHIPID(adreno_dev->chipid));

	/* Log size is encoded in (number of 4K units - 1) */
	val = (gmu->gmu_log->gmuaddr & GENMASK(31, 12)) |
		((GMU_LOG_SIZE/SZ_4K - 1) & GENMASK(7, 0));
	gmu_core_regwrite(device, GEN7_GMU_GENERAL_8, val);

	/* Configure power control and bring the GMU out of reset */
	gen7_gmu_power_config(adreno_dev);

	/*
	 * Enable BCL throttling -
	 * XOCLK1: countable: 0x13 (25% throttle)
	 * XOCLK2: countable: 0x17 (58% throttle)
	 * XOCLK3: countable: 0x19 (75% throttle)
	 * POWER_CONTROL_SELECT_0 controls counters 0 - 3, each selector
	 * is 8 bits wide.
	 */
	if (adreno_dev->bcl_enabled)
		gmu_core_regrmw(device, GEN7_GMU_CX_GMU_POWER_COUNTER_SELECT_0,
			0xffffff00, FIELD_PREP(GENMASK(31, 24), 0x19) |
			FIELD_PREP(GENMASK(23, 16), 0x17) |
			FIELD_PREP(GENMASK(15, 8), 0x13));
}

static struct gmu_vma_node *find_va(struct gmu_vma_entry *vma, u32 addr, u32 size)
{
	struct rb_node *node = vma->vma_root.rb_node;

	while (node != NULL) {
		struct gmu_vma_node *data = rb_entry(node, struct gmu_vma_node, node);

		if (addr + size <= data->va)
			node = node->rb_left;
		else if (addr >= data->va + data->size)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

/* Return true if VMA supports dynamic allocations */
static bool vma_is_dynamic(int vma_id)
{
	/* Dynamic allocations are done in the GMU_NONCACHED_KERNEL space */
	return vma_id == GMU_NONCACHED_KERNEL;
}

static int insert_va(struct gmu_vma_entry *vma, u32 addr, u32 size)
{
	struct rb_node **node, *parent = NULL;
	struct gmu_vma_node *new = kzalloc(sizeof(*new), GFP_NOWAIT);

	if (new == NULL)
		return -ENOMEM;

	new->va = addr;
	new->size = size;

	node = &vma->vma_root.rb_node;
	while (*node != NULL) {
		struct gmu_vma_node *this;

		parent = *node;
		this = rb_entry(parent, struct gmu_vma_node, node);

		if (addr + size <= this->va)
			node = &parent->rb_left;
		else if (addr >= this->va + this->size)
			node = &parent->rb_right;
		else {
			kfree(new);
			return -EEXIST;
		}
	}

	/* Add new node and rebalance tree */
	rb_link_node(&new->node, parent, node);
	rb_insert_color(&new->node, &vma->vma_root);

	return 0;
}

static u32 find_unmapped_va(struct gmu_vma_entry *vma, u32 size, u32 va_align)
{
	struct rb_node *node = rb_first(&vma->vma_root);
	u32 cur = vma->start;
	bool found = false;

	va_align = hfi_get_gmu_va_alignment(va_align);

	cur = ALIGN(cur, va_align);

	while (node) {
		struct gmu_vma_node *data = rb_entry(node, struct gmu_vma_node, node);

		if (cur + size <= data->va) {
			found = true;
			break;
		}

		cur = ALIGN(data->va + data->size, va_align);
		node = rb_next(node);
	}

	/* Do we have space after the last node? */
	if (!found && (cur + size <= vma->start + vma->size))
		found = true;
	return found ? cur : 0;
}

static int _map_gmu_dynamic(struct gen7_gmu_device *gmu,
	struct kgsl_memdesc *md,
	u32 addr, u32 vma_id, int attrs, u32 va_align)
{
	int ret;
	struct gmu_vma_entry *vma = &gmu->vma[vma_id];
	struct gmu_vma_node *vma_node = NULL;
	u32 size = md->size;

	spin_lock(&vma->lock);
	if (!addr) {
		addr = find_unmapped_va(vma, size, va_align);
		if (addr == 0) {
			spin_unlock(&vma->lock);
			dev_err(&gmu->pdev->dev,
				"Insufficient VA space size: %x\n", size);
			return -ENOMEM;
		}
	}

	ret = insert_va(vma, addr, size);
	spin_unlock(&vma->lock);
	if (ret < 0) {
		dev_err(&gmu->pdev->dev,
			"Could not insert va: %x size %x\n", addr, size);
		return ret;
	}

	ret = gmu_core_map_memdesc(gmu->domain, md, addr, attrs);
	if (!ret) {
		md->gmuaddr = addr;
		return 0;
	}

	/* Failed to map to GMU */
	dev_err(&gmu->pdev->dev,
		"Unable to map GMU kernel block: addr:0x%08x size:0x%x :%d\n",
		addr, md->size, ret);

	spin_lock(&vma->lock);
	vma_node = find_va(vma, md->gmuaddr, md->size);
	if (vma_node)
		rb_erase(&vma_node->node, &vma->vma_root);
	spin_unlock(&vma->lock);
	kfree(vma_node);

	return ret;
}

static int _map_gmu_static(struct gen7_gmu_device *gmu,
	struct kgsl_memdesc *md,
	u32 addr, u32 vma_id, int attrs, u32 va_align)
{
	int ret;
	struct gmu_vma_entry *vma = &gmu->vma[vma_id];

	if (!addr)
		addr = ALIGN(vma->next_va, hfi_get_gmu_va_alignment(va_align));

	ret = gmu_core_map_memdesc(gmu->domain, md, addr, attrs);
	if (ret) {
		dev_err(&gmu->pdev->dev,
			"Unable to map GMU kernel block: addr:0x%08x size:0x%x :%d\n",
			addr, md->size, ret);
		return ret;
	}
	md->gmuaddr = addr;
	vma->next_va = md->gmuaddr + md->size;
	return 0;
}

static int _map_gmu(struct gen7_gmu_device *gmu,
	struct kgsl_memdesc *md,
	u32 addr, u32 vma_id, int attrs, u32 va_align)
{
	return vma_is_dynamic(vma_id) ?
			_map_gmu_dynamic(gmu, md, addr, vma_id, attrs, va_align) :
			_map_gmu_static(gmu, md, addr, vma_id, attrs, va_align);
}

int gen7_gmu_import_buffer(struct gen7_gmu_device *gmu, u32 vma_id,
				struct kgsl_memdesc *md, u32 size, u32 attrs)
{
	return _map_gmu(gmu, md, 0, vma_id, attrs, 0);
}

struct kgsl_memdesc *gen7_reserve_gmu_kernel_block(struct gen7_gmu_device *gmu,
	u32 addr, u32 size, u32 vma_id, u32 va_align)
{
	int ret;
	struct kgsl_memdesc *md;
	struct kgsl_device *device = KGSL_DEVICE(gen7_gmu_to_adreno(gmu));
	int attrs = IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV;

	if (gmu->global_entries == ARRAY_SIZE(gmu->gmu_globals))
		return ERR_PTR(-ENOMEM);

	md = &gmu->gmu_globals[gmu->global_entries];

	ret = kgsl_allocate_kernel(device, md, size, 0, KGSL_MEMDESC_SYSMEM);
	if (ret) {
		memset(md, 0x0, sizeof(*md));
		return ERR_PTR(-ENOMEM);
	}

	ret = _map_gmu(gmu, md, addr, vma_id, attrs, va_align);
	if (ret) {
		kgsl_sharedmem_free(md);
		memset(md, 0x0, sizeof(*md));
		return ERR_PTR(ret);
	}

	gmu->global_entries++;

	return md;
}

struct kgsl_memdesc *gen7_reserve_gmu_kernel_block_fixed(struct gen7_gmu_device *gmu,
	u32 addr, u32 size, u32 vma_id, const char *resource, int attrs, u32 va_align)
{
	int ret;
	struct kgsl_memdesc *md;
	struct kgsl_device *device = KGSL_DEVICE(gen7_gmu_to_adreno(gmu));

	if (gmu->global_entries == ARRAY_SIZE(gmu->gmu_globals))
		return ERR_PTR(-ENOMEM);

	md = &gmu->gmu_globals[gmu->global_entries];

	ret = kgsl_memdesc_init_fixed(device, gmu->pdev, resource, md);
	if (ret)
		return ERR_PTR(ret);

	ret = _map_gmu(gmu, md, addr, vma_id, attrs, va_align);

	sg_free_table(md->sgt);
	kfree(md->sgt);
	md->sgt = NULL;

	if (!ret)
		gmu->global_entries++;
	else {
		dev_err(&gmu->pdev->dev,
			"Unable to map GMU kernel block: addr:0x%08x size:0x%x :%d\n",
			addr, md->size, ret);
		memset(md, 0x0, sizeof(*md));
		md = ERR_PTR(ret);
	}
	return md;
}

int gen7_alloc_gmu_kernel_block(struct gen7_gmu_device *gmu,
	struct kgsl_memdesc *md, u32 size, u32 vma_id, int attrs)
{
	int ret;
	struct kgsl_device *device = KGSL_DEVICE(gen7_gmu_to_adreno(gmu));

	ret = kgsl_allocate_kernel(device, md, size, 0, KGSL_MEMDESC_SYSMEM);
	if (ret)
		return ret;

	ret = _map_gmu(gmu, md, 0, vma_id, attrs, 0);
	if (ret)
		kgsl_sharedmem_free(md);

	return ret;
}

void gen7_free_gmu_block(struct gen7_gmu_device *gmu, struct kgsl_memdesc *md)
{
	int vma_id = find_vma_block(gmu, md->gmuaddr, md->size);
	struct gmu_vma_entry *vma;
	struct gmu_vma_node *vma_node;

	if ((vma_id < 0) || !vma_is_dynamic(vma_id))
		return;

	vma = &gmu->vma[vma_id];

	/*
	 * Do not remove the vma node if we failed to unmap the entire buffer. This is because the
	 * iommu driver considers remapping an already mapped iova as fatal.
	 */
	if (md->size != iommu_unmap(gmu->domain, md->gmuaddr, md->size))
		goto free;

	spin_lock(&vma->lock);
	vma_node = find_va(vma, md->gmuaddr, md->size);
	if (vma_node)
		rb_erase(&vma_node->node, &vma->vma_root);
	spin_unlock(&vma->lock);
	kfree(vma_node);
free:
	kgsl_sharedmem_free(md);
}

static int gen7_gmu_process_prealloc(struct gen7_gmu_device *gmu,
	struct gmu_block_header *blk)
{
	struct kgsl_memdesc *md;

	int id = find_vma_block(gmu, blk->addr, blk->value);

	if (id < 0) {
		dev_err(&gmu->pdev->dev,
			"Invalid prealloc block addr: 0x%x value:%d\n",
			blk->addr, blk->value);
		return id;
	}

	/* Nothing to do for TCM blocks or user uncached */
	if (id == GMU_ITCM || id == GMU_DTCM || id == GMU_NONCACHED_USER)
		return 0;

	/* Check if the block is already allocated */
	md = find_gmu_memdesc(gmu, blk->addr, blk->value);
	if (md != NULL)
		return 0;

	md = gen7_reserve_gmu_kernel_block(gmu, blk->addr, blk->value, id, 0);

	return PTR_ERR_OR_ZERO(md);
}

int gen7_gmu_parse_fw(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);
	struct gmu_block_header *blk;
	int ret, offset = 0;
	const char *gmufw_name = gen7_core->gmufw_name;

	/*
	 * If GMU fw already saved and verified, do nothing new.
	 * Skip only request_firmware and allow preallocation to
	 * ensure in scenario where GMU request firmware succeeded
	 * but preallocation fails, we don't return early without
	 * successful preallocations on next open call.
	 */
	if (!gmu->fw_image) {

		if (gen7_core->gmufw_name == NULL)
			return -EINVAL;

		ret = request_firmware(&gmu->fw_image, gmufw_name,
				&gmu->pdev->dev);
		if (ret) {
			if (gen7_core->gmufw_bak_name) {
				gmufw_name = gen7_core->gmufw_bak_name;
				ret = request_firmware(&gmu->fw_image, gmufw_name,
					&gmu->pdev->dev);
			}

			if (ret) {
				dev_err(&gmu->pdev->dev,
					"request_firmware (%s) failed: %d\n",
					gmufw_name, ret);

				return ret;
			}
		}
	}

	/*
	 * Zero payload fw blocks contain metadata and are
	 * guaranteed to precede fw load data. Parse the
	 * metadata blocks.
	 */
	while (offset < gmu->fw_image->size) {
		blk = (struct gmu_block_header *)&gmu->fw_image->data[offset];

		if (offset + sizeof(*blk) > gmu->fw_image->size) {
			dev_err(&gmu->pdev->dev, "Invalid FW Block\n");
			return -EINVAL;
		}

		/* Done with zero length blocks so return */
		if (blk->size)
			break;

		offset += sizeof(*blk);

		if (blk->type == GMU_BLK_TYPE_PREALLOC_REQ ||
			blk->type == GMU_BLK_TYPE_PREALLOC_PERSIST_REQ) {
			ret = gen7_gmu_process_prealloc(gmu, blk);

			if (ret)
				return ret;
		}
	}

	return 0;
}

int gen7_gmu_memory_init(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	/* GMU master log */
	if (IS_ERR_OR_NULL(gmu->gmu_log))
		gmu->gmu_log = gen7_reserve_gmu_kernel_block(gmu, 0,
				GMU_LOG_SIZE, GMU_NONCACHED_KERNEL, 0);

	return PTR_ERR_OR_ZERO(gmu->gmu_log);
}

static int gen7_gmu_init(struct adreno_device *adreno_dev)
{
	int ret;

	ret = gen7_gmu_parse_fw(adreno_dev);
	if (ret)
		return ret;

	ret = gen7_gmu_memory_init(adreno_dev);
	if (ret)
		return ret;

	return gen7_hfi_init(adreno_dev);
}

static void _do_gbif_halt(struct kgsl_device *device, u32 reg, u32 ack_reg,
	u32 mask, const char *client)
{
	u32 ack;
	unsigned long t;

	kgsl_regwrite(device, reg, mask);

	t = jiffies + msecs_to_jiffies(100);
	do {
		kgsl_regread(device, ack_reg, &ack);
		if ((ack & mask) == mask)
			return;

		/*
		 * If we are attempting recovery in case of stall-on-fault
		 * then the halt sequence will not complete as long as SMMU
		 * is stalled.
		 */
		kgsl_mmu_pagefault_resume(&device->mmu, false);

		usleep_range(10, 100);
	} while (!time_after(jiffies, t));

	/* Check one last time */
	kgsl_mmu_pagefault_resume(&device->mmu, false);

	kgsl_regread(device, ack_reg, &ack);
	if ((ack & mask) == mask)
		return;

	dev_err(device->dev, "%s GBIF halt timed out\n", client);
}

static void gen7_gmu_pwrctrl_suspend(struct adreno_device *adreno_dev)
{
	int ret = 0;
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* Disconnect GPU from BUS is not needed if CX GDSC goes off later */

	/* Check no outstanding RPMh voting */
	gen7_complete_rpmh_votes(gmu, 1);

	/* Clear the WRITEDROPPED fields and set fence to allow mode */
	gmu_core_regwrite(device, GEN7_GMU_AHB_FENCE_STATUS_CLR, 0x7);
	gmu_core_regwrite(device, GEN7_GMU_AO_AHB_FENCE_CTRL, 0);

	/* Make sure above writes are committed before we proceed to recovery */
	wmb();

	gmu_core_regwrite(device, GEN7_GMU_CM3_SYSRESET, 1);

	/* Halt GX traffic */
	if (gen7_gmu_gx_is_on(adreno_dev))
		_do_gbif_halt(device, GEN7_RBBM_GBIF_HALT,
				GEN7_RBBM_GBIF_HALT_ACK,
				GEN7_GBIF_GX_HALT_MASK,
				"GX");

	/* Halt CX traffic */
	_do_gbif_halt(device, GEN7_GBIF_HALT, GEN7_GBIF_HALT_ACK,
			GEN7_GBIF_ARB_HALT_MASK, "CX");

	if (gen7_gmu_gx_is_on(adreno_dev))
		kgsl_regwrite(device, GEN7_RBBM_SW_RESET_CMD, 0x1);

	/* Allow the software reset to complete */
	udelay(100);

	/*
	 * This is based on the assumption that GMU is the only one controlling
	 * the GX HS. This code path is the only client voting for GX through
	 * the regulator interface.
	 */
	if (gmu->gx_gdsc) {
		if (gen7_gmu_gx_is_on(adreno_dev)) {
			/* Switch gx gdsc control from GMU to CPU
			 * force non-zero reference count in clk driver
			 * so next disable call will turn
			 * off the GDSC
			 */
			ret = regulator_enable(gmu->gx_gdsc);
			if (ret)
				dev_err(&gmu->pdev->dev,
					"suspend fail: gx enable %d\n", ret);

			ret = regulator_disable(gmu->gx_gdsc);
			if (ret)
				dev_err(&gmu->pdev->dev,
					"suspend fail: gx disable %d\n", ret);

			if (gen7_gmu_gx_is_on(adreno_dev))
				dev_err(&gmu->pdev->dev,
					"gx is stuck on\n");
		}
	}
}

/*
 * gen7_gmu_notify_slumber() - initiate request to GMU to prepare to slumber
 * @device: Pointer to KGSL device
 */
static int gen7_gmu_notify_slumber(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int bus_level = pwr->pwrlevels[pwr->default_pwrlevel].bus_freq;
	int perf_idx = gmu->hfi.dcvs_table.gpu_level_num -
			pwr->default_pwrlevel - 1;
	struct hfi_prep_slumber_cmd req = {
		.freq = perf_idx,
		.bw = bus_level,
	};
	int ret;

	/* Disable the power counter so that the GMU is not busy */
	gmu_core_regwrite(device, GEN7_GMU_CX_GMU_POWER_COUNTER_ENABLE, 0);

	ret = CMD_MSG_HDR(req, H2F_MSG_PREPARE_SLUMBER);
	if (ret)
		return ret;

	ret = gen7_hfi_send_generic_req(adreno_dev, &req);

	/* Make sure the fence is in ALLOW mode */
	gmu_core_regwrite(device, GEN7_GMU_AO_AHB_FENCE_CTRL, 0);
	return ret;
}

void gen7_gmu_suspend(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	gen7_gmu_pwrctrl_suspend(adreno_dev);

	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

	kgsl_pwrctrl_disable_cx_gdsc(device, gmu->cx_gdsc);

	gen7_rdpm_cx_freq_update(gmu, 0);

	dev_err(&gmu->pdev->dev, "Suspended GMU\n");

	device->state = KGSL_STATE_NONE;
}

static int gen7_gmu_dcvs_set(struct adreno_device *adreno_dev,
		int gpu_pwrlevel, int bus_level)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct hfi_dcvstable_cmd *table = &gmu->hfi.dcvs_table;
	struct hfi_gx_bw_perf_vote_cmd req = {
		.ack_type = DCVS_ACK_BLOCK,
		.freq = INVALID_DCVS_IDX,
		.bw = INVALID_DCVS_IDX,
	};
	int ret = 0;

	if (!test_bit(GMU_PRIV_HFI_STARTED, &gmu->flags))
		return 0;

	/* Do not set to XO and lower GPU clock vote from GMU */
	if ((gpu_pwrlevel != INVALID_DCVS_IDX) &&
			(gpu_pwrlevel >= table->gpu_level_num - 1))
		return -EINVAL;

	if (gpu_pwrlevel < table->gpu_level_num - 1)
		req.freq = table->gpu_level_num - gpu_pwrlevel - 1;

	if (bus_level < pwr->ddr_table_count && bus_level > 0)
		req.bw = bus_level;

	/* GMU will vote for slumber levels through the sleep sequence */
	if ((req.freq == INVALID_DCVS_IDX) &&
		(req.bw == INVALID_DCVS_IDX)) {
		return 0;
	}

	ret = CMD_MSG_HDR(req, H2F_MSG_GX_BW_PERF_VOTE);
	if (ret)
		return ret;

	ret = gen7_hfi_send_generic_req(adreno_dev, &req);
	if (ret) {
		dev_err_ratelimited(&gmu->pdev->dev,
			"Failed to set GPU perf idx %d, bw idx %d\n",
			req.freq, req.bw);

		/*
		 * If this was a dcvs request along side an active gpu, request
		 * dispatcher based reset and recovery.
		 */
		if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
			adreno_dispatcher_fault(adreno_dev, ADRENO_GMU_FAULT |
				ADRENO_GMU_FAULT_SKIP_SNAPSHOT);
	}

	if (req.freq != INVALID_DCVS_IDX)
		gen7_rdpm_mx_freq_update(gmu,
			gmu->hfi.dcvs_table.gx_votes[req.freq].freq);

	return ret;
}

static int gen7_gmu_clock_set(struct adreno_device *adreno_dev, u32 pwrlevel)
{
	return gen7_gmu_dcvs_set(adreno_dev, pwrlevel, INVALID_DCVS_IDX);
}

static int gen7_gmu_ifpc_store(struct kgsl_device *device,
		unsigned int val)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	unsigned int requested_idle_level;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_IFPC))
		return -EINVAL;

	if (val)
		requested_idle_level = GPU_HW_IFPC;
	else
		requested_idle_level = GPU_HW_ACTIVE;

	if (gmu->idle_level == requested_idle_level)
		return 0;

	/* Power down the GPU before changing the idle level */
	return adreno_power_cycle_u32(adreno_dev, &gmu->idle_level,
		requested_idle_level);
}

static unsigned int gen7_gmu_ifpc_show(struct kgsl_device *device)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(ADRENO_DEVICE(device));

	return gmu->idle_level == GPU_HW_IFPC;
}

/* Send an NMI to the GMU */
void gen7_gmu_send_nmi(struct kgsl_device *device, bool force)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	u32 result;

	/*
	 * Do not send NMI if the SMMU is stalled because GMU will not be able
	 * to save cm3 state to DDR.
	 */
	if (gen7_gmu_gx_is_on(adreno_dev) && gen7_is_smmu_stalled(device)) {
		dev_err(&gmu->pdev->dev,
			"Skipping NMI because SMMU is stalled\n");
		return;
	}

	if (force)
		goto nmi;

	/*
	 * We should not send NMI if there was a CM3 fault reported because we
	 * don't want to overwrite the critical CM3 state captured by gmu before
	 * it sent the CM3 fault interrupt. Also don't send NMI if GMU reset is
	 * already active. We could have hit a GMU assert and NMI might have
	 * already been triggered.
	 */

	/* make sure we're reading the latest cm3_fault */
	smp_rmb();

	if (atomic_read(&gmu->cm3_fault))
		return;

	gmu_core_regread(device, GEN7_GMU_CM3_FW_INIT_RESULT, &result);

	if (result & 0xE00)
		return;

nmi:
	/* Mask so there's no interrupt caused by NMI */
	gmu_core_regwrite(device, GEN7_GMU_GMU2HOST_INTR_MASK, UINT_MAX);

	/* Make sure the interrupt is masked before causing it */
	wmb();

	/* This will cause the GMU to save it's internal state to ddr */
	gmu_core_regrmw(device, GEN7_GMU_CM3_CFG, BIT(9), BIT(9));

	/* Make sure the NMI is invoked before we proceed*/
	wmb();

	/* Wait for the NMI to be handled */
	udelay(200);
}

static void gen7_gmu_cooperative_reset(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	unsigned int result;

	gmu_core_regwrite(device, GEN7_GMU_CX_GMU_WDOG_CTRL, 0);
	gmu_core_regwrite(device, GEN7_GMU_HOST2GMU_INTR_SET, BIT(17));

	/*
	 * After triggering graceful death wait for snapshot ready
	 * indication from GMU.
	 */
	if (!gmu_core_timed_poll_check(device, GEN7_GMU_CM3_FW_INIT_RESULT,
				0x800, 2, 0x800))
		return;

	gmu_core_regread(device, GEN7_GMU_CM3_FW_INIT_RESULT, &result);
	dev_err(&gmu->pdev->dev,
		"GMU cooperative reset timed out 0x%x\n", result);
	/*
	 * If we dont get a snapshot ready from GMU, trigger NMI
	 * and if we still timeout then we just continue with reset.
	 */
	gen7_gmu_send_nmi(device, true);

	gmu_core_regread(device, GEN7_GMU_CM3_FW_INIT_RESULT, &result);
	if ((result & 0x800) != 0x800)
		dev_err(&gmu->pdev->dev,
			"GMU cooperative reset NMI timed out 0x%x\n", result);
}

static int gen7_gmu_wait_for_active_transition(struct kgsl_device *device)
{
	unsigned int reg;
	struct gen7_gmu_device *gmu = to_gen7_gmu(ADRENO_DEVICE(device));

	if (gmu_core_timed_poll_check(device, GEN7_GPU_GMU_CX_GMU_RPMH_POWER_STATE,
			GPU_HW_ACTIVE, 100, GENMASK(3, 0))) {
		gmu_core_regread(device, GEN7_GPU_GMU_CX_GMU_RPMH_POWER_STATE, &reg);
		dev_err(&gmu->pdev->dev,
			"GMU failed to move to ACTIVE state, Current state: 0x%x\n",
			reg);

		return -ETIMEDOUT;
	}

	return 0;
}

static bool gen7_gmu_scales_bandwidth(struct kgsl_device *device)
{
	return true;
}

void gen7_gmu_handle_watchdog(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 mask;

	/* Temporarily mask the watchdog interrupt to prevent a storm */
	gmu_core_regread(device, GEN7_GMU_AO_HOST_INTERRUPT_MASK, &mask);
	gmu_core_regwrite(device, GEN7_GMU_AO_HOST_INTERRUPT_MASK,
			(mask | GMU_INT_WDOG_BITE));

	gen7_gmu_send_nmi(device, false);

	dev_err_ratelimited(&gmu->pdev->dev,
			"GMU watchdog expired interrupt received\n");
}

static irqreturn_t gen7_gmu_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	const struct gen7_gpudev *gen7_gpudev =
		to_gen7_gpudev(ADRENO_GPU_DEVICE(adreno_dev));
	unsigned int status = 0;

	gmu_core_regread(device, GEN7_GMU_AO_HOST_INTERRUPT_STATUS, &status);
	gmu_core_regwrite(device, GEN7_GMU_AO_HOST_INTERRUPT_CLR, status);

	if (status & GMU_INT_HOST_AHB_BUS_ERR)
		dev_err_ratelimited(&gmu->pdev->dev,
				"AHB bus error interrupt received\n");

	if (status & GMU_INT_WDOG_BITE)
		gen7_gpudev->handle_watchdog(adreno_dev);

	if (status & GMU_INT_FENCE_ERR) {
		unsigned int fence_status;

		gmu_core_regread(device, GEN7_GMU_AHB_FENCE_STATUS,
			&fence_status);
		dev_err_ratelimited(&gmu->pdev->dev,
			"FENCE error interrupt received %x\n", fence_status);
	}

	if (status & ~GMU_AO_INT_MASK)
		dev_err_ratelimited(&gmu->pdev->dev,
				"Unhandled GMU interrupts 0x%lx\n",
				status & ~GMU_AO_INT_MASK);

	return IRQ_HANDLED;
}

void gen7_gmu_aop_send_acd_state(struct gen7_gmu_device *gmu, bool flag)
{
	struct qmp_pkt msg;
	char msg_buf[36];
	u32 size;
	int ret;

	if (IS_ERR_OR_NULL(gmu->mailbox.channel))
		return;

	size = scnprintf(msg_buf, sizeof(msg_buf),
			"{class: gpu, res: acd, val: %d}", flag);

	/* mailbox controller expects 4-byte aligned buffer */
	msg.size = ALIGN((size + 1), SZ_4);
	msg.data = msg_buf;

	ret = mbox_send_message(gmu->mailbox.channel, &msg);

	if (ret < 0)
		dev_err(&gmu->pdev->dev,
			"AOP mbox send message failed: %d\n", ret);
}

int gen7_gmu_enable_clks(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	gen7_rdpm_cx_freq_update(gmu, gmu->freqs[0] / 1000);

	ret = kgsl_clk_set_rate(gmu->clks, gmu->num_clks, "gmu_clk",
			gmu->freqs[0]);
	if (ret) {
		dev_err(&gmu->pdev->dev, "Unable to set the GMU clock\n");
		return ret;
	}

	ret = kgsl_clk_set_rate(gmu->clks, gmu->num_clks, "hub_clk",
			adreno_dev->gmu_hub_clk_freq);
	if (ret && ret != -ENODEV) {
		dev_err(&gmu->pdev->dev, "Unable to set the HUB clock\n");
		return ret;
	}

	ret = clk_bulk_prepare_enable(gmu->num_clks, gmu->clks);
	if (ret) {
		dev_err(&gmu->pdev->dev, "Cannot enable GMU clocks\n");
		return ret;
	}

	device->state = KGSL_STATE_AWARE;

	return 0;
}

static int gen7_gmu_first_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int level, ret;

	kgsl_pwrctrl_request_state(device, KGSL_STATE_AWARE);

	gen7_gmu_aop_send_acd_state(gmu, adreno_dev->acd_enabled);

	ret = kgsl_pwrctrl_enable_cx_gdsc(device, gmu->cx_gdsc);
	if (ret)
		return ret;

	ret = gen7_gmu_enable_clks(adreno_dev);
	if (ret)
		goto gdsc_off;

	ret = gen7_gmu_load_fw(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	gen7_gmu_version_info(adreno_dev);

	ret = gen7_gmu_itcm_shadow(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	gen7_gmu_register_config(adreno_dev);

	gen7_gmu_irq_enable(adreno_dev);

	/* Vote for minimal DDR BW for GMU to init */
	level = pwr->pwrlevels[pwr->default_pwrlevel].bus_min;
	icc_set_bw(pwr->icc_path, 0, kBps_to_icc(pwr->ddr_table[level]));

	ret = gen7_gmu_device_start(adreno_dev);
	if (ret)
		goto err;

	if (!test_bit(GMU_PRIV_PDC_RSC_LOADED, &gmu->flags)) {
		ret = gen7_load_pdc_ucode(adreno_dev);
		if (ret)
			goto err;

		gen7_load_rsc_ucode(adreno_dev);
		set_bit(GMU_PRIV_PDC_RSC_LOADED, &gmu->flags);
	}

	ret = gen7_gmu_hfi_start(adreno_dev);
	if (ret)
		goto err;

	ret = gen7_hfi_start(adreno_dev);
	if (ret)
		goto err;

	icc_set_bw(pwr->icc_path, 0, 0);

	device->gmu_fault = false;

	kgsl_pwrctrl_set_state(device, KGSL_STATE_AWARE);

	return 0;

err:
	gen7_gmu_irq_disable(adreno_dev);

	if (device->gmu_fault) {
		gen7_gmu_suspend(adreno_dev);
		return ret;
	}

clks_gdsc_off:
	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

gdsc_off:
	kgsl_pwrctrl_disable_cx_gdsc(device, gmu->cx_gdsc);

	gen7_rdpm_cx_freq_update(gmu, 0);

	return ret;
}

static int gen7_gmu_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret = 0;

	kgsl_pwrctrl_request_state(device, KGSL_STATE_AWARE);

	ret = kgsl_pwrctrl_enable_cx_gdsc(device, gmu->cx_gdsc);
	if (ret)
		return ret;

	ret = gen7_gmu_enable_clks(adreno_dev);
	if (ret)
		goto gdsc_off;

	/*
	 * TLB operations are skipped during slumber. Incase CX doesn't
	 * go down, it can result in incorrect translations due to stale
	 * TLB entries. Flush TLB before boot up to ensure fresh start.
	 */
	kgsl_mmu_flush_tlb(&device->mmu);

	ret = gen7_rscc_wakeup_sequence(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	ret = gen7_gmu_load_fw(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	gen7_gmu_register_config(adreno_dev);

	gen7_gmu_irq_enable(adreno_dev);

	ret = gen7_gmu_device_start(adreno_dev);
	if (ret)
		goto err;

	ret = gen7_gmu_hfi_start(adreno_dev);
	if (ret)
		goto err;

	ret = gen7_hfi_start(adreno_dev);
	if (ret)
		goto err;

	device->gmu_fault = false;

	kgsl_pwrctrl_set_state(device, KGSL_STATE_AWARE);

	return 0;

err:
	gen7_gmu_irq_disable(adreno_dev);

	if (device->gmu_fault) {
		gen7_gmu_suspend(adreno_dev);
		return ret;
	}

clks_gdsc_off:
	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

gdsc_off:
	kgsl_pwrctrl_disable_cx_gdsc(device, gmu->cx_gdsc);

	gen7_rdpm_cx_freq_update(gmu, 0);

	return ret;
}

static void set_acd(struct adreno_device *adreno_dev, void *priv)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	adreno_dev->acd_enabled = *((bool *)priv);
	gen7_gmu_aop_send_acd_state(gmu, adreno_dev->acd_enabled);
}

static int gen7_gmu_acd_set(struct kgsl_device *device, bool val)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	if (IS_ERR_OR_NULL(gmu->mailbox.channel))
		return -EINVAL;

	/* Don't do any unneeded work if ACD is already in the correct state */
	if (adreno_dev->acd_enabled == val)
		return 0;

	/* Power cycle the GPU for changes to take effect */
	return adreno_power_cycle(adreno_dev, set_acd, &val);
}

#define BCL_RESP_TYPE_MASK   BIT(0)
#define BCL_SID0_MASK        GENMASK(7, 1)
#define BCL_SID1_MASK        GENMASK(14, 8)
#define BCL_SID2_MASK        GENMASK(21, 15)

static int gen7_bcl_sid_set(struct kgsl_device *device, u32 sid_id, u64 sid_val)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	u32 bcl_data, val = (u32) sid_val;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_BCL) ||
		!FIELD_GET(BCL_RESP_TYPE_MASK, adreno_dev->bcl_data))
		return -EINVAL;

	switch (sid_id) {
	case 0:
		adreno_dev->bcl_data &= ~BCL_SID0_MASK;
		bcl_data = adreno_dev->bcl_data | FIELD_PREP(BCL_SID0_MASK, val);
		break;
	case 1:
		adreno_dev->bcl_data &= ~BCL_SID1_MASK;
		bcl_data = adreno_dev->bcl_data | FIELD_PREP(BCL_SID1_MASK, val);
		break;
	case 2:
		adreno_dev->bcl_data &= ~BCL_SID2_MASK;
		bcl_data = adreno_dev->bcl_data | FIELD_PREP(BCL_SID2_MASK, val);
		break;
	default:
		return -EINVAL;
	}

	return adreno_power_cycle_u32(adreno_dev, &adreno_dev->bcl_data, bcl_data);
}

static u64 gen7_bcl_sid_get(struct kgsl_device *device, u32 sid_id)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_BCL) ||
		!FIELD_GET(BCL_RESP_TYPE_MASK, adreno_dev->bcl_data))
		return 0;

	switch (sid_id) {
	case 0:
		return ((u64) FIELD_GET(BCL_SID0_MASK, adreno_dev->bcl_data));
	case 1:
		return ((u64) FIELD_GET(BCL_SID1_MASK, adreno_dev->bcl_data));
	case 2:
		return ((u64) FIELD_GET(BCL_SID2_MASK, adreno_dev->bcl_data));
	default:
		return 0;
	}
}

static const struct gmu_dev_ops gen7_gmudev = {
	.oob_set = gen7_gmu_oob_set,
	.oob_clear = gen7_gmu_oob_clear,
	.ifpc_store = gen7_gmu_ifpc_store,
	.ifpc_show = gen7_gmu_ifpc_show,
	.cooperative_reset = gen7_gmu_cooperative_reset,
	.wait_for_active_transition = gen7_gmu_wait_for_active_transition,
	.scales_bandwidth = gen7_gmu_scales_bandwidth,
	.acd_set = gen7_gmu_acd_set,
	.bcl_sid_set = gen7_bcl_sid_set,
	.bcl_sid_get = gen7_bcl_sid_get,
	.send_nmi = gen7_gmu_send_nmi,
};

static int gen7_gmu_bus_set(struct adreno_device *adreno_dev, int buslevel,
	u32 ab)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret = 0;

	if (buslevel != pwr->cur_buslevel) {
		ret = gen7_gmu_dcvs_set(adreno_dev, INVALID_DCVS_IDX, buslevel);
		if (ret)
			return ret;

		pwr->cur_buslevel = buslevel;

		trace_kgsl_buslevel(device, pwr->active_pwrlevel, buslevel);
	}

	if (ab != pwr->cur_ab) {
		icc_set_bw(pwr->icc_path, MBps_to_icc(ab), 0);
		pwr->cur_ab = ab;
	}

	return ret;
}

static void gen7_free_gmu_globals(struct gen7_gmu_device *gmu)
{
	int i;

	for (i = 0; i < gmu->global_entries && i < ARRAY_SIZE(gmu->gmu_globals); i++) {
		struct kgsl_memdesc *md = &gmu->gmu_globals[i];

		if (!md->gmuaddr)
			continue;

		iommu_unmap(gmu->domain, md->gmuaddr, md->size);

		if (md->priv & KGSL_MEMDESC_SYSMEM)
			kgsl_sharedmem_free(md);

		memset(md, 0, sizeof(*md));
	}

	if (gmu->domain) {
		iommu_detach_device(gmu->domain, &gmu->pdev->dev);
		iommu_domain_free(gmu->domain);
		gmu->domain = NULL;
	}

	gmu->global_entries = 0;
}

static int gen7_gmu_aop_mailbox_init(struct adreno_device *adreno_dev,
		struct gen7_gmu_device *gmu)
{
	struct kgsl_mailbox *mailbox = &gmu->mailbox;

	mailbox->client.dev = &gmu->pdev->dev;
	mailbox->client.tx_block = true;
	mailbox->client.tx_tout = 1000;
	mailbox->client.knows_txdone = false;

	mailbox->channel = mbox_request_channel(&mailbox->client, 0);
	if (IS_ERR(mailbox->channel))
		return PTR_ERR(mailbox->channel);

	adreno_dev->acd_enabled = true;
	return 0;
}

static void gen7_gmu_acd_probe(struct kgsl_device *device,
		struct gen7_gmu_device *gmu, struct device_node *node)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct kgsl_pwrlevel *pwrlevel =
			&pwr->pwrlevels[pwr->num_pwrlevels - 1];
	struct hfi_acd_table_cmd *cmd = &gmu->hfi.acd_table;
	int ret, i, cmd_idx = 0;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_ACD))
		return;

	cmd->hdr = CREATE_MSG_HDR(H2F_MSG_ACD_TBL, sizeof(*cmd), HFI_MSG_CMD);

	cmd->version = 1;
	cmd->stride = 1;
	cmd->enable_by_level = 0;

	/*
	 * Iterate through each gpu power level and generate a mask for GMU
	 * firmware for ACD enabled levels and store the corresponding control
	 * register configurations to the acd_table structure.
	 */
	for (i = 0; i < pwr->num_pwrlevels; i++) {
		if (pwrlevel->acd_level) {
			cmd->enable_by_level |= (1 << (i + 1));
			cmd->data[cmd_idx++] = pwrlevel->acd_level;
		}
		pwrlevel--;
	}

	if (!cmd->enable_by_level)
		return;

	cmd->num_levels = cmd_idx;

	ret = gen7_gmu_aop_mailbox_init(adreno_dev, gmu);
	if (ret)
		dev_err(&gmu->pdev->dev,
			"AOP mailbox init failed: %d\n", ret);
}

static int gen7_gmu_reg_probe(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret;

	ret = kgsl_regmap_add_region(&device->regmap, gmu->pdev, "gmu", NULL, NULL);

	if (ret)
		dev_err(&gmu->pdev->dev, "Unable to map the GMU registers\n");
	/*
	 * gmu_ao_blk_dec1 and gmu_ao_blk_dec2 are contiguous and contained within the gmu region
	 * mapped above. gmu_ao_blk_dec0 is not within the gmu region and is mapped separately.
	 */
	kgsl_regmap_add_region(&device->regmap, gmu->pdev, "gmu_ao_blk_dec0", NULL, NULL);

	return ret;
}

static int gen7_gmu_clk_probe(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret, i;
	int tbl_size;
	int num_freqs;
	int offset;

	ret = devm_clk_bulk_get_all(&gmu->pdev->dev, &gmu->clks);
	if (ret < 0)
		return ret;

	/*
	 * Voting for apb_pclk will enable power and clocks required for
	 * QDSS path to function. However, if QCOM_KGSL_QDSS_STM is not enabled,
	 * QDSS is essentially unusable. Hence, if QDSS cannot be used,
	 * don't vote for this clock.
	 */
	if (!IS_ENABLED(CONFIG_QCOM_KGSL_QDSS_STM)) {
		for (i = 0; i < ret; i++) {
			if (!strcmp(gmu->clks[i].id, "apb_pclk")) {
				gmu->clks[i].clk = NULL;
				break;
			}
		}
	}

	gmu->num_clks = ret;

	/* Read the optional list of GMU frequencies */
	if (of_get_property(gmu->pdev->dev.of_node,
		"qcom,gmu-freq-table", &tbl_size) == NULL)
		goto default_gmu_freq;

	num_freqs = (tbl_size / sizeof(u32)) / 2;
	if (num_freqs != ARRAY_SIZE(gmu->freqs))
		goto default_gmu_freq;

	for (i = 0; i < num_freqs; i++) {
		offset = i * 2;
		ret = of_property_read_u32_index(gmu->pdev->dev.of_node,
			"qcom,gmu-freq-table", offset, &gmu->freqs[i]);
		if (ret)
			goto default_gmu_freq;
		ret = of_property_read_u32_index(gmu->pdev->dev.of_node,
			"qcom,gmu-freq-table", offset + 1, &gmu->vlvls[i]);
		if (ret)
			goto default_gmu_freq;
	}
	return 0;

default_gmu_freq:
	/* The GMU frequency table is missing or invalid. Go with a default */
	gmu->freqs[0] = GMU_FREQ_MIN;
	gmu->vlvls[0] = RPMH_REGULATOR_LEVEL_LOW_SVS;
	gmu->freqs[1] = GMU_FREQ_MAX;
	gmu->vlvls[1] = RPMH_REGULATOR_LEVEL_SVS;

	return 0;
}

static void gen7_gmu_rdpm_probe(struct gen7_gmu_device *gmu,
		struct kgsl_device *device)
{
	struct resource *res;

	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM, "rdpm_cx");
	if (res)
		gmu->rdpm_cx_virt = devm_ioremap(&device->pdev->dev,
				res->start, resource_size(res));

	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM, "rdpm_mx");
	if (res)
		gmu->rdpm_mx_virt = devm_ioremap(&device->pdev->dev,
				res->start, resource_size(res));
}

static int gmu_cx_gdsc_event(struct notifier_block *nb,
	unsigned long event, void *data)
{
	struct kgsl_pwrctrl *pwr = container_of(nb, struct kgsl_pwrctrl, cx_gdsc_nb);
	struct kgsl_device *device = container_of(pwr, struct kgsl_device, pwrctrl);
	u32 val;

	if (!(event & REGULATOR_EVENT_DISABLE) || !pwr->cx_gdsc_wait)
		return 0;

	if (kgsl_regmap_read_poll_timeout(&device->regmap, GEN7_GPU_CC_CX_GDSCR,
		val, !(val & BIT(31)), 100, 100 * 1000))
		dev_err(device->dev, "GPU CX wait timeout.\n");

	pwr->cx_gdsc_wait = false;
	complete_all(&device->pwrctrl.cx_gdsc_gate);

	return 0;
}

static int gen7_gmu_regulators_probe(struct gen7_gmu_device *gmu,
		struct platform_device *pdev)
{
	gmu->cx_gdsc = devm_regulator_get(&pdev->dev, "vddcx");
	if (IS_ERR(gmu->cx_gdsc)) {
		if (PTR_ERR(gmu->cx_gdsc) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Couldn't get the vddcx gdsc\n");
		return PTR_ERR(gmu->cx_gdsc);
	}

	gmu->gx_gdsc = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(gmu->gx_gdsc)) {
		if (PTR_ERR(gmu->gx_gdsc) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Couldn't get the vdd gdsc\n");
		return PTR_ERR(gmu->gx_gdsc);
	}

	return 0;
}

void gen7_gmu_remove(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	if (!IS_ERR_OR_NULL(gmu->mailbox.channel))
		mbox_free_channel(gmu->mailbox.channel);

	adreno_dev->acd_enabled = false;

	if (gmu->fw_image)
		release_firmware(gmu->fw_image);

	gen7_free_gmu_globals(gmu);

	vfree(gmu->itcm_shadow);
	if (gmu->log_kobj.state_initialized)
		kobject_put(&gmu->log_kobj);
}

static int gen7_gmu_iommu_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long addr, int flags, void *token)
{
	char *fault_type = "unknown";

	if (flags & IOMMU_FAULT_TRANSLATION)
		fault_type = "translation";
	else if (flags & IOMMU_FAULT_PERMISSION)
		fault_type = "permission";
	else if (flags & IOMMU_FAULT_EXTERNAL)
		fault_type = "external";
	else if (flags & IOMMU_FAULT_TRANSACTION_STALLED)
		fault_type = "transaction stalled";

	dev_err(dev, "GMU fault addr = %lX, context=kernel (%s %s fault)\n",
			addr,
			(flags & IOMMU_FAULT_WRITE) ? "write" : "read",
			fault_type);

	return 0;
}

static int gen7_gmu_iommu_init(struct gen7_gmu_device *gmu)
{
	int ret;
	int no_stall = 1;

	gmu->domain = iommu_domain_alloc(&platform_bus_type);
	if (gmu->domain == NULL) {
		dev_err(&gmu->pdev->dev, "Unable to allocate GMU IOMMU domain\n");
		return -ENODEV;
	}

	/*
	 * Disable stall on fault for the GMU context bank.
	 * This sets SCTLR.CFCFG = 0.
	 * Also note that, the smmu driver sets SCTLR.HUPCF = 0 by default.
	 */
	iommu_domain_set_attr(gmu->domain,
		DOMAIN_ATTR_FAULT_MODEL_NO_STALL, &no_stall);

	ret = iommu_attach_device(gmu->domain, &gmu->pdev->dev);
	if (!ret) {
		iommu_set_fault_handler(gmu->domain,
			gen7_gmu_iommu_fault_handler, gmu);
		return 0;
	}

	dev_err(&gmu->pdev->dev,
		"Unable to attach GMU IOMMU domain: %d\n", ret);
	iommu_domain_free(gmu->domain);
	gmu->domain = NULL;

	return ret;
}

int gen7_gmu_probe(struct kgsl_device *device,
		struct platform_device *pdev)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret, i;

	gmu->pdev = pdev;

	dma_set_coherent_mask(&gmu->pdev->dev, DMA_BIT_MASK(64));
	gmu->pdev->dev.dma_mask = &gmu->pdev->dev.coherent_dma_mask;
	set_dma_ops(&gmu->pdev->dev, NULL);

	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
						"rscc");
	if (res) {
		gmu->rscc_virt = devm_ioremap(&device->pdev->dev, res->start,
						resource_size(res));
		if (!gmu->rscc_virt) {
			dev_err(&gmu->pdev->dev, "rscc ioremap failed\n");
			return -ENOMEM;
		}
	}

	/* Setup any rdpm register ranges */
	gen7_gmu_rdpm_probe(gmu, device);

	/* Set up GMU regulators */
	ret = gen7_gmu_regulators_probe(gmu, pdev);
	if (ret)
		return ret;

	ret = gen7_gmu_clk_probe(adreno_dev);
	if (ret < 0)
		return ret;

	/* Set up GMU IOMMU and shared memory with GMU */
	ret = gen7_gmu_iommu_init(gmu);
	if (ret)
		goto error;

	gmu->vma = gen7_gmu_vma;
	for (i = 0; i < ARRAY_SIZE(gen7_gmu_vma); i++) {
		struct gmu_vma_entry *vma = &gen7_gmu_vma[i];

		vma->vma_root = RB_ROOT;
		spin_lock_init(&vma->lock);
	}

	/* Map and reserve GMU CSRs registers */
	ret = gen7_gmu_reg_probe(adreno_dev);
	if (ret)
		goto error;

	/* Populates RPMh configurations */
	ret = gen7_build_rpmh_tables(adreno_dev);
	if (ret)
		goto error;

	/* Set up GMU idle state */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_IFPC))
		gmu->idle_level = GPU_HW_IFPC;
	else
		gmu->idle_level = GPU_HW_ACTIVE;

	gen7_gmu_acd_probe(device, gmu, pdev->dev.of_node);

	set_bit(GMU_ENABLED, &device->gmu_core.flags);

	device->gmu_core.dev_ops = &gen7_gmudev;

	/* Set default GMU attributes */
	gmu->log_stream_enable = false;
	gmu->log_group_mask = 0x3;

	/* GMU sysfs nodes setup */
	(void) kobject_init_and_add(&gmu->log_kobj, &log_kobj_type, &dev->kobj, "log");

	of_property_read_u32(gmu->pdev->dev.of_node, "qcom,gmu-perf-ddr-bw",
		&gmu->perf_ddr_bw);

	gmu->irq = kgsl_request_irq(gmu->pdev, "gmu",
		gen7_gmu_irq_handler, device);

	if (gmu->irq >= 0)
		return 0;

	ret = gmu->irq;

error:
	gen7_gmu_remove(device);
	return ret;
}

static void gen7_gmu_active_count_put(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return;

	if (WARN(atomic_read(&device->active_cnt) == 0,
		"Unbalanced get/put calls to KGSL active count\n"))
		return;

	if (atomic_dec_and_test(&device->active_cnt)) {
		kgsl_pwrscale_update_stats(device);
		kgsl_pwrscale_update(device);
		kgsl_start_idle_timer(device);
	}

	trace_kgsl_active_count(device,
		(unsigned long) __builtin_return_address(0));

	wake_up(&device->active_cnt_wq);
}

int gen7_halt_gbif(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/* Halt new client requests */
	kgsl_regwrite(device, GEN7_GBIF_HALT, GEN7_GBIF_CLIENT_HALT_MASK);
	ret = adreno_wait_for_halt_ack(device,
		GEN7_GBIF_HALT_ACK, GEN7_GBIF_CLIENT_HALT_MASK);

	/* Halt all AXI requests */
	kgsl_regwrite(device, GEN7_GBIF_HALT, GEN7_GBIF_ARB_HALT_MASK);
	ret = adreno_wait_for_halt_ack(device,
		GEN7_GBIF_HALT_ACK, GEN7_GBIF_ARB_HALT_MASK);

	/* De-assert the halts */
	kgsl_regwrite(device, GEN7_GBIF_HALT, 0x0);

	return ret;
}

static int gen7_gmu_power_off(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret = 0;

	if (device->gmu_fault)
		goto error;

	/* Wait for the lowest idle level we requested */
	ret = gen7_gmu_wait_for_lowest_idle(adreno_dev);
	if (ret)
		goto error;

	ret = gen7_complete_rpmh_votes(gmu, 2);
	if (ret)
		goto error;

	ret = gen7_gmu_notify_slumber(adreno_dev);
	if (ret)
		goto error;

	ret = gen7_gmu_wait_for_idle(adreno_dev);
	if (ret)
		goto error;

	ret = gen7_rscc_sleep_sequence(adreno_dev);
	if (ret)
		goto error;

	gen7_rdpm_mx_freq_update(gmu, 0);

	/* Now that we are done with GMU and GPU, Clear the GBIF */
	ret = gen7_halt_gbif(adreno_dev);
	if (ret)
		goto error;

	gen7_gmu_irq_disable(adreno_dev);

	gen7_hfi_stop(adreno_dev);

	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

	kgsl_pwrctrl_disable_cx_gdsc(device, gmu->cx_gdsc);

	gen7_rdpm_cx_freq_update(gmu, 0);

	device->state = KGSL_STATE_NONE;

	return 0;

error:
	gen7_gmu_irq_disable(adreno_dev);
	gen7_hfi_stop(adreno_dev);
	gen7_gmu_suspend(adreno_dev);

	return ret;
}

void gen7_enable_gpu_irq(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_pwrctrl_irq(device, true);

	adreno_irqctrl(adreno_dev, 1);
}

void gen7_disable_gpu_irq(struct adreno_device *adreno_dev)
{
	kgsl_pwrctrl_irq(KGSL_DEVICE(adreno_dev), false);

	if (gen7_gmu_gx_is_on(adreno_dev))
		adreno_irqctrl(adreno_dev, 0);
}

static int gen7_gpu_boot(struct adreno_device *adreno_dev)
{
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/* Clear any GPU faults that might have been left over */
	adreno_clear_gpu_fault(adreno_dev);

	adreno_set_active_ctxs_null(adreno_dev);

	ret = kgsl_mmu_start(device);
	if (ret)
		goto err;

	ret = gen7_gmu_oob_set(device, oob_gpu);
	if (ret)
		goto oob_clear;

	ret = gen7_gmu_hfi_start_msg(adreno_dev);
	if (ret)
		goto oob_clear;

	/* Clear the busy_data stats - we're starting over from scratch */
	memset(&adreno_dev->busy_data, 0, sizeof(adreno_dev->busy_data));

	gen7_start(adreno_dev);

	if (gen7_core->qos_value && adreno_is_preemption_enabled(adreno_dev))
		kgsl_regwrite(device, GEN7_RBBM_GBIF_CLIENT_QOS_CNTL,
			gen7_core->qos_value[adreno_dev->cur_rb->id]);

	/* Re-initialize the coresight registers if applicable */
	adreno_coresight_start(adreno_dev);

	adreno_perfcounter_start(adreno_dev);

	/* Clear FSR here in case it is set from a previous pagefault */
	kgsl_mmu_clear_fsr(&device->mmu);

	gen7_enable_gpu_irq(adreno_dev);

	ret = gen7_rb_start(adreno_dev);
	if (ret) {
		gen7_disable_gpu_irq(adreno_dev);
		goto oob_clear;
	}

	/*
	 * At this point it is safe to assume that we recovered. Setting
	 * this field allows us to take a new snapshot for the next failure
	 * if we are prioritizing the first unrecoverable snapshot.
	 */
	if (device->snapshot)
		device->snapshot->recovered = true;

	/* Start the dispatcher */
	adreno_dispatcher_start(device);

	device->reset_counter++;

	gen7_gmu_oob_clear(device, oob_gpu);

	return 0;

oob_clear:
	gen7_gmu_oob_clear(device, oob_gpu);

err:
	gen7_gmu_power_off(adreno_dev);

	return ret;
}

static void gmu_idle_timer(struct timer_list *t)
{
	struct kgsl_device *device = container_of(t, struct kgsl_device,
					idle_timer);

	kgsl_schedule_work(&device->idle_check_ws);
}

static int gen7_boot(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	if (WARN_ON(test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags)))
		return 0;

	kgsl_pwrctrl_request_state(device, KGSL_STATE_ACTIVE);

	ret = gen7_gmu_boot(adreno_dev);
	if (ret)
		return ret;

	ret = gen7_gpu_boot(adreno_dev);
	if (ret)
		return ret;

	kgsl_start_idle_timer(device);
	kgsl_pwrscale_wake(device);

	set_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	device->pwrctrl.last_stat_updated = ktime_get();

	kgsl_pwrctrl_set_state(device, KGSL_STATE_ACTIVE);

	return ret;
}

static int gen7_first_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret;

	if (test_bit(GMU_PRIV_FIRST_BOOT_DONE, &gmu->flags)) {
		if (!test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
			return gen7_boot(adreno_dev);

		return 0;
	}

	ret = gen7_ringbuffer_init(adreno_dev);
	if (ret)
		return ret;

	ret = gen7_microcode_read(adreno_dev);
	if (ret)
		return ret;

	ret = gen7_init(adreno_dev);
	if (ret)
		return ret;

	ret = gen7_gmu_init(adreno_dev);
	if (ret)
		return ret;

	kgsl_pwrctrl_request_state(device, KGSL_STATE_ACTIVE);

	ret = gen7_gmu_first_boot(adreno_dev);
	if (ret)
		return ret;

	ret = gen7_gpu_boot(adreno_dev);
	if (ret)
		return ret;

	adreno_get_bus_counters(adreno_dev);

	adreno_dev->cooperative_reset = ADRENO_FEATURE(adreno_dev,
						 ADRENO_COOP_RESET);

	adreno_create_profile_buffer(adreno_dev);

	set_bit(GMU_PRIV_FIRST_BOOT_DONE, &gmu->flags);
	set_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	/*
	 * BCL needs respective Central Broadcast register to
	 * be programed from TZ. This programing happens only
	 * when zap shader firmware load is successful. Zap firmware
	 * load can fail in boot up path hence enable BCL only after we
	 * successfully complete first boot to ensure that Central
	 * Broadcast register was programed before enabling BCL.
	 */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_BCL))
		adreno_dev->bcl_enabled = true;

	/*
	 * There is a possible deadlock scenario during kgsl firmware reading
	 * (request_firmware) and devfreq update calls. During first boot, kgsl
	 * device mutex is held and then request_firmware is called for reading
	 * firmware. request_firmware internally takes dev_pm_qos_mtx lock.
	 * Whereas in case of devfreq update calls triggered by thermal/bcl or
	 * devfreq sysfs, it first takes the same dev_pm_qos_mtx lock and then
	 * tries to take kgsl device mutex as part of get_dev_status/target
	 * calls. This results in deadlock when both thread are unable to acquire
	 * the mutex held by other thread. Enable devfreq updates now as we are
	 * done reading all firmware files.
	 */
	device->pwrscale.devfreq_enabled = true;

	device->pwrctrl.last_stat_updated = ktime_get();

	kgsl_pwrctrl_set_state(device, KGSL_STATE_ACTIVE);

	return 0;
}

static bool gen7_irq_pending(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 status;

	kgsl_regread(device, GEN7_RBBM_INT_0_STATUS, &status);

	/* Return busy if a interrupt is pending */
	return ((status & adreno_dev->irq_mask) ||
		atomic_read(&adreno_dev->pending_irq_refcnt));
}

static int gen7_power_off(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret;

	WARN_ON(!test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags));

	adreno_suspend_context(device);

	/*
	 * adreno_suspend_context() unlocks the device mutex, which
	 * could allow a concurrent thread to attempt SLUMBER sequence.
	 * Hence, check the flags again before proceeding with SLUMBER.
	 */
	if (!test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		return 0;

	kgsl_pwrctrl_request_state(device, KGSL_STATE_SLUMBER);

	ret = gen7_gmu_oob_set(device, oob_gpu);
	if (ret)
		goto no_gx_power;

	if (gen7_irq_pending(adreno_dev)) {
		gen7_gmu_oob_clear(device, oob_gpu);
		return -EBUSY;
	}

	kgsl_pwrscale_update_stats(device);

	/* Save active coresight registers if applicable */
	adreno_coresight_stop(adreno_dev);

	adreno_irqctrl(adreno_dev, 0);

no_gx_power:
	gen7_gmu_oob_clear(device, oob_gpu);

	kgsl_pwrctrl_irq(device, false);

	gen7_gmu_power_off(adreno_dev);

	adreno_set_active_ctxs_null(adreno_dev);

	adreno_dispatcher_stop(adreno_dev);

	adreno_ringbuffer_stop(adreno_dev);

	if (!IS_ERR_OR_NULL(adreno_dev->gpu_llc_slice))
		llcc_slice_deactivate(adreno_dev->gpu_llc_slice);

	if (!IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice))
		llcc_slice_deactivate(adreno_dev->gpuhtw_llc_slice);

	clear_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	del_timer_sync(&device->idle_timer);

	kgsl_pwrscale_sleep(device);

	kgsl_pwrctrl_clear_l3_vote(device);

	/*
	 * Reset the context records so that CP can start
	 * at the correct read pointer for BV thread after
	 * coming out of slumber.
	 */
	gen7_reset_preempt_records(adreno_dev);

	kgsl_pwrctrl_set_state(device, KGSL_STATE_SLUMBER);

	return ret;
}

static void gmu_idle_check(struct work_struct *work)
{
	struct kgsl_device *device = container_of(work,
					struct kgsl_device, idle_check_ws);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret;

	mutex_lock(&device->mutex);

	if (test_bit(GMU_DISABLE_SLUMBER, &device->gmu_core.flags))
		goto done;

	if (atomic_read(&device->active_cnt) || time_is_after_jiffies(device->idle_jiffies)) {
		kgsl_pwrscale_update(device);
		kgsl_start_idle_timer(device);
		goto done;
	}

	if (!test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		goto done;

	spin_lock(&device->submit_lock);

	if (device->submit_now) {
		spin_unlock(&device->submit_lock);
		kgsl_pwrscale_update(device);
		kgsl_start_idle_timer(device);
		goto done;
	}

	device->slumber = true;
	spin_unlock(&device->submit_lock);

	ret = gen7_power_off(adreno_dev);
	if (ret == -EBUSY) {
		kgsl_pwrscale_update(device);
		kgsl_start_idle_timer(device);
	}

done:
	mutex_unlock(&device->mutex);
}

static int gen7_gmu_first_open(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/*
	 * Do the one time settings that need to happen when we
	 * attempt to boot the gpu the very first time
	 */
	ret = gen7_first_boot(adreno_dev);
	if (ret)
		return ret;

	/*
	 * A client that does a first_open but never closes the device
	 * may prevent us from going back to SLUMBER. So trigger the idle
	 * check by incrementing the active count and immediately releasing it.
	 */
	atomic_inc(&device->active_cnt);
	gen7_gmu_active_count_put(adreno_dev);

	return 0;
}

static int gen7_gmu_last_close(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		return gen7_power_off(adreno_dev);

	return 0;
}

static int gen7_gmu_active_count_get(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret = 0;

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return -EINVAL;

	if (test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags))
		return -EINVAL;

	if ((atomic_read(&device->active_cnt) == 0) &&
		!test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		ret = gen7_boot(adreno_dev);

	if (ret == 0)
		atomic_inc(&device->active_cnt);

	trace_kgsl_active_count(device,
		(unsigned long) __builtin_return_address(0));

	return ret;
}

static int gen7_gmu_pm_suspend(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret;

	if (test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags))
		return 0;

	kgsl_pwrctrl_request_state(device, KGSL_STATE_SUSPEND);

	/* Halt any new submissions */
	reinit_completion(&device->halt_gate);

	/* wait for active count so device can be put in slumber */
	ret = kgsl_active_count_wait(device, 0, HZ);
	if (ret) {
		dev_err(device->dev,
			"Timed out waiting for the active count\n");
		goto err;
	}

	ret = adreno_idle(device);
	if (ret)
		goto err;

	if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		gen7_power_off(adreno_dev);

	set_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags);

	adreno_get_gpu_halt(adreno_dev);

	kgsl_pwrctrl_set_state(device, KGSL_STATE_SUSPEND);

	return 0;
err:
	adreno_dispatcher_start(device);
	return ret;
}

static void gen7_gmu_pm_resume(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	if (WARN(!test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags),
		"resume invoked without a suspend\n"))
		return;

	adreno_put_gpu_halt(adreno_dev);

	adreno_dispatcher_start(device);

	clear_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags);
}

static void gen7_gmu_touch_wakeup(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	int ret;

	/*
	 * Do not wake up a suspended device or until the first boot sequence
	 * has been completed.
	 */
	if (test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags) ||
		!test_bit(GMU_PRIV_FIRST_BOOT_DONE, &gmu->flags))
		return;

	if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		goto done;

	kgsl_pwrctrl_request_state(device, KGSL_STATE_ACTIVE);

	ret = gen7_gmu_boot(adreno_dev);
	if (ret)
		return;

	ret = gen7_gpu_boot(adreno_dev);
	if (ret)
		return;

	kgsl_pwrscale_wake(device);

	set_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	device->pwrctrl.last_stat_updated = ktime_get();

	kgsl_pwrctrl_set_state(device, KGSL_STATE_ACTIVE);

done:
	/*
	 * When waking up from a touch event we want to stay active long enough
	 * for the user to send a draw command. The default idle timer timeout
	 * is shorter than we want so go ahead and push the idle timer out
	 * further for this special case
	 */
	mod_timer(&device->idle_timer, jiffies +
			msecs_to_jiffies(adreno_wake_timeout));
}

int gen7_gmu_register_gdsc_notifier(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	device->pwrctrl.cx_gdsc_nb.notifier_call = gmu_cx_gdsc_event;
	return devm_regulator_register_notifier(gmu->cx_gdsc, &device->pwrctrl.cx_gdsc_nb);
}

const struct adreno_power_ops gen7_gmu_power_ops = {
	.first_open = gen7_gmu_first_open,
	.last_close = gen7_gmu_last_close,
	.active_count_get = gen7_gmu_active_count_get,
	.active_count_put = gen7_gmu_active_count_put,
	.pm_suspend = gen7_gmu_pm_suspend,
	.pm_resume = gen7_gmu_pm_resume,
	.touch_wakeup = gen7_gmu_touch_wakeup,
	.gpu_clock_set = gen7_gmu_clock_set,
	.gpu_bus_set = gen7_gmu_bus_set,
	.register_gdsc_notifier = gen7_gmu_register_gdsc_notifier,
};

int gen7_gmu_device_probe(struct platform_device *pdev,
	u32 chipid, const struct adreno_gpu_core *gpucore)
{
	struct adreno_device *adreno_dev;
	struct kgsl_device *device;
	struct gen7_device *gen7_dev;
	int ret;

	gen7_dev = devm_kzalloc(&pdev->dev, sizeof(*gen7_dev),
			GFP_KERNEL);
	if (!gen7_dev)
		return -ENOMEM;

	adreno_dev = &gen7_dev->adreno_dev;

	ret = gen7_probe_common(pdev, adreno_dev, chipid, gpucore);
	if (ret)
		return ret;

	ret = adreno_dispatcher_init(adreno_dev);
	if (ret) {
		dev_err(&pdev->dev, "adreno_dispatcher_init failed ret %d\n", ret);
		return ret;
	}

	device = KGSL_DEVICE(adreno_dev);

	INIT_WORK(&device->idle_check_ws, gmu_idle_check);

	timer_setup(&device->idle_timer, gmu_idle_timer, 0);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_DMS)) {
		set_bit(ADRENO_DEVICE_DMS, &adreno_dev->priv);
		adreno_dev->dms_enabled = true;
	}

	adreno_dev->irq_mask = GEN7_INT_MASK;

	return 0;
}

int gen7_gmu_reset(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);

	gen7_disable_gpu_irq(adreno_dev);

	gen7_gmu_irq_disable(adreno_dev);

	gen7_hfi_stop(adreno_dev);

	/* Hard reset the gmu and gpu */
	gen7_gmu_suspend(adreno_dev);

	gen7_reset_preempt_records(adreno_dev);

	clear_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	/* Attempt to reboot the gmu and gpu */
	return gen7_boot(adreno_dev);
}

int gen7_gmu_hfi_probe(struct adreno_device *adreno_dev)
{
	struct gen7_gmu_device *gmu = to_gen7_gmu(adreno_dev);
	struct gen7_hfi *hfi = &gmu->hfi;

	hfi->irq = kgsl_request_irq(gmu->pdev, "hfi",
		gen7_hfi_irq_handler, KGSL_DEVICE(adreno_dev));

	return hfi->irq < 0 ? hfi->irq : 0;
}

int gen7_gmu_add_to_minidump(struct adreno_device *adreno_dev)
{
	struct gen7_device *gen7_dev = container_of(adreno_dev,
					struct gen7_device, adreno_dev);
	int ret;

	ret = kgsl_add_va_to_minidump(adreno_dev->dev.dev, KGSL_GEN7_DEVICE,
			(void *)(gen7_dev), sizeof(struct gen7_device));
	if (ret)
		return ret;

	ret = kgsl_add_va_to_minidump(adreno_dev->dev.dev, KGSL_GMU_LOG_ENTRY,
			gen7_dev->gmu.gmu_log->hostptr, gen7_dev->gmu.gmu_log->size);
	if (ret)
		return ret;

	ret = kgsl_add_va_to_minidump(adreno_dev->dev.dev, KGSL_HFIMEM_ENTRY,
			gen7_dev->gmu.hfi.hfi_mem->hostptr, gen7_dev->gmu.hfi.hfi_mem->size);

	return ret;
}

static int gen7_gmu_bind(struct device *dev, struct device *master, void *data)
{
	struct kgsl_device *device = dev_get_drvdata(master);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	const struct gen7_gpudev *gen7_gpudev = to_gen7_gpudev(gpudev);
	int ret;

	ret = gen7_gmu_probe(device, to_platform_device(dev));
	if (ret)
		return ret;

	if (gen7_gpudev->hfi_probe) {
		ret = gen7_gpudev->hfi_probe(adreno_dev);

		if (ret) {
			gen7_gmu_remove(device);
			return ret;
		}
	}

	return 0;
}

static void gen7_gmu_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct kgsl_device *device = dev_get_drvdata(master);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);
	const struct gen7_gpudev *gen7_gpudev = to_gen7_gpudev(gpudev);

	if (gen7_gpudev->hfi_remove)
		gen7_gpudev->hfi_remove(adreno_dev);

	gen7_gmu_remove(device);
}

static const struct component_ops gen7_gmu_component_ops = {
	.bind = gen7_gmu_bind,
	.unbind = gen7_gmu_unbind,
};

static int gen7_gmu_probe_dev(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &gen7_gmu_component_ops);
}

static int gen7_gmu_remove_dev(struct platform_device *pdev)
{
	component_del(&pdev->dev, &gen7_gmu_component_ops);
	return 0;
}

static const struct of_device_id gen7_gmu_match_table[] = {
	{ .compatible = "qcom,gen7-gmu" },
	{ },
};

struct platform_driver gen7_gmu_driver = {
	.probe = gen7_gmu_probe_dev,
	.remove = gen7_gmu_remove_dev,
	.driver = {
		.name = "adreno-gen7-gmu",
		.of_match_table = gen7_gmu_match_table,
	},
};
